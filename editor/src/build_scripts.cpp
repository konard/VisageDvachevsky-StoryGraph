/**
 * @file build_scripts.cpp
 * @brief Script compilation and pack building implementation
 *
 * Extracted from build_system.cpp (issue #482)
 * Contains:
 * - compileBytecode() - Compiles NM Script source files to bytecode
 * - buildPack() - Creates .nmres pack files with compression and encryption
 */

#include "NovelMind/editor/build_system.hpp"

#include "NovelMind/scripting/compiler.hpp"
#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"
#include "NovelMind/scripting/validator.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>

#ifdef NOVELMIND_HAS_ZLIB
#include <zlib.h>
#endif

#ifdef NOVELMIND_HAS_OPENSSL
#include <openssl/evp.h>
#include <openssl/sha.h>
#endif

namespace fs = std::filesystem;

namespace NovelMind::editor {

// ============================================================================
// Script Compilation - compileBytecode()
// ============================================================================

Result<void> BuildSystem::compileBytecode(const std::string& outputPath) {
  try {
    // Create the combined bytecode file with real compiled bytecode
    std::ofstream output(outputPath, std::ios::binary);
    if (!output.is_open()) {
      return Result<void>::error("Cannot create bytecode file: " + outputPath);
    }

    // Write header with NMC1 magic (NovelMind Compiled v1)
    const char magic[] = "NMC1";
    output.write(magic, 4);

    // Write version (format: major << 16 | minor << 8 | patch)
    u32 version = 1;
    output.write(reinterpret_cast<const char*>(&version), sizeof(version));

    // Write script count
    u32 scriptCount = static_cast<u32>(m_scriptFiles.size());
    output.write(reinterpret_cast<const char*>(&scriptCount), sizeof(scriptCount));

    // Prepare script map entries for source mapping (debug builds)
    std::vector<std::tuple<u64, std::string, u32, u32>> scriptMapEntries;
    u64 currentOffset = 12; // After header (4 + 4 + 4)

    // Compile and write bytecode for each script
    for (const auto& scriptPath : m_scriptFiles) {
      std::ifstream file(scriptPath);
      if (!file.is_open()) {
        logMessage("Cannot open script file: " + scriptPath, true);
        continue;
      }

      std::stringstream buffer;
      buffer << file.rdbuf();
      std::string source = buffer.str();
      file.close();

      if (source.empty()) {
        logMessage("Skipping empty script: " + scriptPath, false);
        continue;
      }

      // Step 1: Lexical analysis
      scripting::Lexer lexer;
      auto tokenResult = lexer.tokenize(source);
      if (!tokenResult.isOk()) {
        logMessage("Lexer error in " + scriptPath + ": " + tokenResult.error(), true);
        continue;
      }

      auto tokens = tokenResult.value();

      if (!lexer.getErrors().empty()) {
        for (const auto& err : lexer.getErrors()) {
          logMessage("Lexer error in " + scriptPath + " at line " +
                         std::to_string(err.location.line) + ": " + err.message,
                     true);
        }
        continue;
      }

      // Step 2: Parsing
      scripting::Parser parser;
      auto parseResult = parser.parse(tokens);
      if (!parseResult.isOk()) {
        logMessage("Parse error in " + scriptPath + ": " + parseResult.error(), true);
        continue;
      }

      auto program = std::move(parseResult).value();

      if (!parser.getErrors().empty()) {
        for (const auto& err : parser.getErrors()) {
          logMessage("Parse error in " + scriptPath + " at line " +
                         std::to_string(err.location.line) + ": " + err.message,
                     true);
        }
        continue;
      }

      // Step 3: Validation (optional, but recommended for production)
      scripting::Validator validator;
      auto validationResult = validator.validate(program);

      if (!validationResult.isValid) {
        for (const auto& err : validationResult.errors.all()) {
          if (err.severity == scripting::Severity::Error) {
            logMessage("Validation error in " + scriptPath + " at line " +
                           std::to_string(err.span.start.line) + ": " + err.message,
                       true);
          }
        }
        continue;
      }

      // Step 4: Compile to bytecode
      scripting::Compiler compiler;
      auto compileResult = compiler.compile(program);
      if (!compileResult.isOk()) {
        logMessage("Compile error in " + scriptPath + ": " + compileResult.error(), true);
        continue;
      }

      auto compiledScript = compileResult.value();

      // Record mapping for source map (before writing bytecode)
      fs::path relativePath = fs::relative(scriptPath, m_config.projectPath);
      scriptMapEntries.emplace_back(currentOffset, relativePath.string(), 1, 0);

      // Write compiled bytecode for this script
      // Format:
      // [size:u32][instruction_count:u32][instructions...][string_count:u32][strings...][scene_count:u32][scenes...][char_count:u32][chars...]

      // Calculate total size for this script's bytecode
      std::vector<u8> scriptBytecode;

      // Write instruction count and instructions
      u32 instrCount = static_cast<u32>(compiledScript.instructions.size());

      // Reserve space and build bytecode buffer
      scriptBytecode.reserve(1024); // Initial reservation

      // Add instruction count
      const u8* instrCountPtr = reinterpret_cast<const u8*>(&instrCount);
      scriptBytecode.insert(scriptBytecode.end(), instrCountPtr,
                            instrCountPtr + sizeof(instrCount));

      // Add instructions (opcode + operand pairs)
      for (const auto& instr : compiledScript.instructions) {
        const u8* opcodePtr = reinterpret_cast<const u8*>(&instr.opcode);
        scriptBytecode.insert(scriptBytecode.end(), opcodePtr, opcodePtr + sizeof(instr.opcode));
        const u8* operandPtr = reinterpret_cast<const u8*>(&instr.operand);
        scriptBytecode.insert(scriptBytecode.end(), operandPtr, operandPtr + sizeof(instr.operand));
      }

      // Write string table
      u32 strCount = static_cast<u32>(compiledScript.stringTable.size());
      const u8* strCountPtr = reinterpret_cast<const u8*>(&strCount);
      scriptBytecode.insert(scriptBytecode.end(), strCountPtr, strCountPtr + sizeof(strCount));

      for (const auto& str : compiledScript.stringTable) {
        u32 len = static_cast<u32>(str.length());
        const u8* lenPtr = reinterpret_cast<const u8*>(&len);
        scriptBytecode.insert(scriptBytecode.end(), lenPtr, lenPtr + sizeof(len));
        scriptBytecode.insert(scriptBytecode.end(), str.begin(), str.end());
      }

      // Write scene entry points
      u32 sceneCount = static_cast<u32>(compiledScript.sceneEntryPoints.size());
      const u8* sceneCountPtr = reinterpret_cast<const u8*>(&sceneCount);
      scriptBytecode.insert(scriptBytecode.end(), sceneCountPtr,
                            sceneCountPtr + sizeof(sceneCount));

      for (const auto& [name, index] : compiledScript.sceneEntryPoints) {
        u32 nameLen = static_cast<u32>(name.length());
        const u8* nameLenPtr = reinterpret_cast<const u8*>(&nameLen);
        scriptBytecode.insert(scriptBytecode.end(), nameLenPtr, nameLenPtr + sizeof(nameLen));
        scriptBytecode.insert(scriptBytecode.end(), name.begin(), name.end());
        const u8* indexPtr = reinterpret_cast<const u8*>(&index);
        scriptBytecode.insert(scriptBytecode.end(), indexPtr, indexPtr + sizeof(index));
      }

      // Write characters
      u32 charCount = static_cast<u32>(compiledScript.characters.size());
      const u8* charCountPtr = reinterpret_cast<const u8*>(&charCount);
      scriptBytecode.insert(scriptBytecode.end(), charCountPtr, charCountPtr + sizeof(charCount));

      for (const auto& [id, ch] : compiledScript.characters) {
        // ID
        u32 idLen = static_cast<u32>(id.length());
        const u8* idLenPtr = reinterpret_cast<const u8*>(&idLen);
        scriptBytecode.insert(scriptBytecode.end(), idLenPtr, idLenPtr + sizeof(idLen));
        scriptBytecode.insert(scriptBytecode.end(), id.begin(), id.end());

        // Display name
        u32 displayNameLen = static_cast<u32>(ch.displayName.length());
        const u8* displayNameLenPtr = reinterpret_cast<const u8*>(&displayNameLen);
        scriptBytecode.insert(scriptBytecode.end(), displayNameLenPtr,
                              displayNameLenPtr + sizeof(displayNameLen));
        scriptBytecode.insert(scriptBytecode.end(), ch.displayName.begin(), ch.displayName.end());

        // Color
        u32 colorLen = static_cast<u32>(ch.color.length());
        const u8* colorLenPtr = reinterpret_cast<const u8*>(&colorLen);
        scriptBytecode.insert(scriptBytecode.end(), colorLenPtr, colorLenPtr + sizeof(colorLen));
        scriptBytecode.insert(scriptBytecode.end(), ch.color.begin(), ch.color.end());
      }

      // Write size prefix and bytecode to output file
      u32 bytecodeSize = static_cast<u32>(scriptBytecode.size());
      output.write(reinterpret_cast<const char*>(&bytecodeSize), sizeof(bytecodeSize));
      output.write(reinterpret_cast<const char*>(scriptBytecode.data()),
                   static_cast<std::streamsize>(scriptBytecode.size()));

      currentOffset += sizeof(bytecodeSize) + bytecodeSize;

      logMessage("Compiled " + relativePath.string() + " (" +
                     std::to_string(compiledScript.instructions.size()) + " instructions, " +
                     std::to_string(compiledScript.stringTable.size()) + " strings)",
                 false);
    }

    output.close();

    // Generate script_map.json for source mapping (useful for debugging)
    if (m_config.generateSourceMap) {
      fs::path scriptMapPath = fs::path(outputPath).parent_path() / "script_map.json";
      std::ofstream mapFile(scriptMapPath);
      if (mapFile.is_open()) {
        mapFile << "{\n";
        mapFile << "  \"version\": \"1.0\",\n";
        mapFile << "  \"bytecode_file\": \"compiled_scripts.bin\",\n";
        mapFile << "  \"format\": \"NMC1\",\n";
        mapFile << "  \"entries\": [\n";

        bool first = true;
        for (const auto& [offset, sourceFile, line, col] : scriptMapEntries) {
          if (!first)
            mapFile << ",\n";
          first = false;

          // Escape backslashes for JSON
          std::string escaped;
          for (char c : sourceFile) {
            if (c == '\\')
              escaped += "\\\\";
            else if (c == '"')
              escaped += "\\\"";
            else
              escaped += c;
          }

          mapFile << "    {\n";
          mapFile << "      \"bytecode_offset\": " << offset << ",\n";
          mapFile << "      \"source_file\": \"" << escaped << "\",\n";
          mapFile << "      \"source_line\": " << line << ",\n";
          mapFile << "      \"source_column\": " << col << "\n";
          mapFile << "    }";
        }

        mapFile << "\n  ]\n";
        mapFile << "}\n";
        mapFile.close();

        logMessage("Generated script_map.json with " + std::to_string(scriptMapEntries.size()) +
                       " entries",
                   false);
      }
    }

    return Result<void>::ok();

  } catch (const std::exception& e) {
    return Result<void>::error(std::string("Bytecode generation failed: ") + e.what());
  }
}

// ============================================================================
// Pack Building - buildPack()
// ============================================================================

Result<void> BuildSystem::buildPack(const std::string& outputPath,
                                    const std::vector<std::string>& files, bool encrypt,
                                    bool compress) {
  // Implements pack_file_format.md specification:
  // - 64-byte header with magic, version, flags, offsets, content hash
  // - Resource table with 48-byte entries including type, size, CRC, IV
  // - String table for resource IDs
  // - Resource data with optional compression and encryption
  // - 32-byte footer with magic, CRC32, timestamp, build number

  try {
    if (files.empty()) {
      // Write a minimal valid pack even if empty
      std::ofstream output(outputPath, std::ios::binary);
      if (!output.is_open()) {
        return Result<void>::error("Cannot create pack file: " + outputPath);
      }

      // Write minimal header (64 bytes)
      const char magic[] = "NMRS";
      output.write(magic, 4);
      u16 versionMajor = 1, versionMinor = 0;
      output.write(reinterpret_cast<const char*>(&versionMajor), 2);
      output.write(reinterpret_cast<const char*>(&versionMinor), 2);
      u32 packFlags = 0;
      output.write(reinterpret_cast<const char*>(&packFlags), 4);
      u32 resourceCount = 0;
      output.write(reinterpret_cast<const char*>(&resourceCount), 4);
      u64 tableOff = 64, stringOff = 64, dataOff = 64, fileSize = 64 + 32;
      output.write(reinterpret_cast<const char*>(&tableOff), 8);
      output.write(reinterpret_cast<const char*>(&stringOff), 8);
      output.write(reinterpret_cast<const char*>(&dataOff), 8);
      output.write(reinterpret_cast<const char*>(&fileSize), 8);
      u8 contentHash[16] = {0};
      output.write(reinterpret_cast<const char*>(contentHash), 16);

      // Write footer (32 bytes)
      const char footerMagic[] = "NMRF";
      output.write(footerMagic, 4);
      u32 tablesCrc = 0;
      output.write(reinterpret_cast<const char*>(&tablesCrc), 4);
      // Use deterministic timestamp if configured
      u64 timestamp = getBuildTimestamp();
      output.write(reinterpret_cast<const char*>(&timestamp), 8);
      u32 buildNumber = m_config.buildNumber;
      output.write(reinterpret_cast<const char*>(&buildNumber), 4);
      u8 reserved[12] = {0};
      output.write(reinterpret_cast<const char*>(reserved), 12);

      output.close();
      return Result<void>::ok();
    }

    // Build resource entries in memory first
    struct ResourceEntry {
      std::string resourceId;
      std::string sourcePath;
      ResourceType type;
      std::vector<u8> data;
      u64 uncompressedSize;
      u64 compressedSize;
      u32 crc32;
      u32 flags;
      std::array<u8, 12> iv; // 12 bytes for AES-256-GCM, store first 8 in table
    };

    std::vector<ResourceEntry> entries;
    entries.reserve(files.size());

    CompressionLevel compressionLevel = compress ? m_config.compression : CompressionLevel::None;

    for (const auto& file : files) {
      ResourceEntry entry;
      entry.sourcePath = file;

      // Generate VFS-style resource ID
      fs::path p(file);
      std::string filename = p.filename().string();
      // Normalize to lowercase and forward slashes for VFS
      std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
      entry.resourceId = filename;
      entry.type = getResourceTypeFromExtension(file);

      // Read file data
      std::ifstream input(file, std::ios::binary | std::ios::ate);
      if (!input.is_open()) {
        return Result<void>::error("Cannot read file: " + file);
      }
      auto fileSize = input.tellg();
      input.seekg(0, std::ios::beg);
      std::vector<u8> rawData(static_cast<usize>(fileSize));
      input.read(reinterpret_cast<char*>(rawData.data()), fileSize);
      input.close();

      entry.uncompressedSize = rawData.size();

      // Calculate CRC32 of uncompressed data
      entry.crc32 = calculateCrc32(rawData.data(), rawData.size());

      // Determine resource flags
      entry.flags = 0;
      if (entry.type == ResourceType::Music) {
        entry.flags |= static_cast<u32>(ResourceFlags::Streamable);
      }
      if (entry.type == ResourceType::Texture || entry.type == ResourceType::Font) {
        entry.flags |= static_cast<u32>(ResourceFlags::Preload);
      }

      // Compress data
      std::vector<u8> processedData = rawData;
      if (compress && compressionLevel != CompressionLevel::None) {
        auto compressResult = compressData(rawData, compressionLevel);
        if (compressResult.isOk()) {
          processedData = compressResult.value();
        }
      }

      // Encrypt data if requested
      if (encrypt && !m_config.encryptionKey.empty()) {
        std::array<u8, 12> iv{};
        auto encryptResult = encryptData(processedData, m_config.encryptionKey, iv);
        if (encryptResult.isOk()) {
          processedData = encryptResult.value();
          entry.iv = iv;
        } else {
          // Log warning but continue without encryption
          logMessage("Warning: Encryption failed for " + file + ": " + encryptResult.error(),
                     false);
          entry.iv = {};
        }
      } else {
        entry.iv = {};
      }

      entry.data = std::move(processedData);
      entry.compressedSize = entry.data.size();
      entries.push_back(std::move(entry));
    }

    // Calculate total data size and apply alignment
    // Per spec: resources > 4KB align to 4KB, smaller align to 16 bytes
    constexpr u64 LARGE_ALIGNMENT = 4096;
    constexpr u64 SMALL_ALIGNMENT = 16;

    u64 currentDataOffset = 0;
    std::vector<u64> dataOffsets;
    dataOffsets.reserve(entries.size());

    for (const auto& entry : entries) {
      u64 alignment = (entry.compressedSize > 4096) ? LARGE_ALIGNMENT : SMALL_ALIGNMENT;
      // Align current offset
      currentDataOffset = ((currentDataOffset + alignment - 1) / alignment) * alignment;
      dataOffsets.push_back(currentDataOffset);
      currentDataOffset += entry.compressedSize;
    }

    // Build string table
    std::vector<u32> stringOffsets;
    stringOffsets.reserve(entries.size());
    u32 currentStringOffset = 0;
    for (const auto& entry : entries) {
      stringOffsets.push_back(currentStringOffset);
      currentStringOffset += static_cast<u32>(entry.resourceId.size()) + 1; // +1 for null
    }

    // Calculate section sizes and offsets
    u64 headerSize = 64;
    u64 resourceTableSize = 48 * entries.size();
    u64 stringTableSize = 4 + (4 * entries.size()) + currentStringOffset; // count + offsets + data
    u64 footerSize = 32;

    u64 resourceTableOffset = headerSize;
    u64 stringTableOffset = resourceTableOffset + resourceTableSize;
    u64 dataOffset = stringTableOffset + stringTableSize;
    // Align data section start
    dataOffset = ((dataOffset + SMALL_ALIGNMENT - 1) / SMALL_ALIGNMENT) * SMALL_ALIGNMENT;

    u64 totalFileSize = dataOffset + currentDataOffset + footerSize;

    // Now write the pack file
    std::ofstream output(outputPath, std::ios::binary);
    if (!output.is_open()) {
      return Result<void>::error("Cannot create pack file: " + outputPath);
    }

    // Prepare header buffer for CRC calculation
    std::vector<u8> headerBuffer;
    headerBuffer.reserve(headerSize);

    // Header: magic (4 bytes)
    const char magic[] = "NMRS";
    headerBuffer.insert(headerBuffer.end(), magic, magic + 4);

    // Header: version (4 bytes)
    u16 versionMajor = 1, versionMinor = 0;
    headerBuffer.insert(headerBuffer.end(), reinterpret_cast<u8*>(&versionMajor),
                        reinterpret_cast<u8*>(&versionMajor) + 2);
    headerBuffer.insert(headerBuffer.end(), reinterpret_cast<u8*>(&versionMinor),
                        reinterpret_cast<u8*>(&versionMinor) + 2);

    // Header: flags (4 bytes)
    u32 packFlags = 0;
    if (encrypt && !m_config.encryptionKey.empty())
      packFlags |= 0x01; // ENCRYPTED
    if (compress && compressionLevel != CompressionLevel::None)
      packFlags |= 0x02; // COMPRESSED
    // SIGNED flag (0x04) would be set for Distribution builds
    headerBuffer.insert(headerBuffer.end(), reinterpret_cast<u8*>(&packFlags),
                        reinterpret_cast<u8*>(&packFlags) + 4);

    // Header: resource count (4 bytes)
    u32 resourceCount = static_cast<u32>(entries.size());
    headerBuffer.insert(headerBuffer.end(), reinterpret_cast<u8*>(&resourceCount),
                        reinterpret_cast<u8*>(&resourceCount) + 4);

    // Header: offsets (24 bytes)
    headerBuffer.insert(headerBuffer.end(), reinterpret_cast<u8*>(&resourceTableOffset),
                        reinterpret_cast<u8*>(&resourceTableOffset) + 8);
    headerBuffer.insert(headerBuffer.end(), reinterpret_cast<u8*>(&stringTableOffset),
                        reinterpret_cast<u8*>(&stringTableOffset) + 8);
    headerBuffer.insert(headerBuffer.end(), reinterpret_cast<u8*>(&dataOffset),
                        reinterpret_cast<u8*>(&dataOffset) + 8);

    // Header: total file size (8 bytes)
    headerBuffer.insert(headerBuffer.end(), reinterpret_cast<u8*>(&totalFileSize),
                        reinterpret_cast<u8*>(&totalFileSize) + 8);

    // Header: content hash placeholder (16 bytes) - will be updated later
    u8 contentHash[16] = {0};
    headerBuffer.insert(headerBuffer.end(), contentHash, contentHash + 16);

    // Write header
    output.write(reinterpret_cast<const char*>(headerBuffer.data()),
                 static_cast<std::streamsize>(headerBuffer.size()));

    // Build resource table buffer for CRC calculation
    std::vector<u8> resourceTableBuffer;
    resourceTableBuffer.reserve(resourceTableSize);

    for (usize i = 0; i < entries.size(); ++i) {
      const auto& entry = entries[i];

      // String offset (4 bytes)
      u32 strOffset = stringOffsets[i];
      resourceTableBuffer.insert(resourceTableBuffer.end(), reinterpret_cast<u8*>(&strOffset),
                                 reinterpret_cast<u8*>(&strOffset) + 4);

      // Resource type (4 bytes)
      u32 resType = static_cast<u32>(entry.type);
      resourceTableBuffer.insert(resourceTableBuffer.end(), reinterpret_cast<u8*>(&resType),
                                 reinterpret_cast<u8*>(&resType) + 4);

      // Data offset (8 bytes) - relative to data section start
      u64 entryDataOffset = dataOffsets[i];
      resourceTableBuffer.insert(resourceTableBuffer.end(), reinterpret_cast<u8*>(&entryDataOffset),
                                 reinterpret_cast<u8*>(&entryDataOffset) + 8);

      // Compressed size (8 bytes)
      u64 compSize = entry.compressedSize;
      resourceTableBuffer.insert(resourceTableBuffer.end(), reinterpret_cast<u8*>(&compSize),
                                 reinterpret_cast<u8*>(&compSize) + 8);

      // Uncompressed size (8 bytes)
      u64 uncompSize = entry.uncompressedSize;
      resourceTableBuffer.insert(resourceTableBuffer.end(), reinterpret_cast<u8*>(&uncompSize),
                                 reinterpret_cast<u8*>(&uncompSize) + 8);

      // Resource flags (4 bytes)
      u32 resFlags = entry.flags;
      resourceTableBuffer.insert(resourceTableBuffer.end(), reinterpret_cast<u8*>(&resFlags),
                                 reinterpret_cast<u8*>(&resFlags) + 4);

      // CRC32 (4 bytes)
      u32 crc = entry.crc32;
      resourceTableBuffer.insert(resourceTableBuffer.end(), reinterpret_cast<u8*>(&crc),
                                 reinterpret_cast<u8*>(&crc) + 4);

      // IV - first 8 bytes only (per spec)
      resourceTableBuffer.insert(resourceTableBuffer.end(), entry.iv.begin(), entry.iv.begin() + 8);
    }

    // Write resource table
    output.write(reinterpret_cast<const char*>(resourceTableBuffer.data()),
                 static_cast<std::streamsize>(resourceTableBuffer.size()));

    // Build string table buffer
    std::vector<u8> stringTableBuffer;
    stringTableBuffer.reserve(stringTableSize);

    // String count (4 bytes)
    u32 stringCount = static_cast<u32>(entries.size());
    stringTableBuffer.insert(stringTableBuffer.end(), reinterpret_cast<u8*>(&stringCount),
                             reinterpret_cast<u8*>(&stringCount) + 4);

    // String offsets (4 bytes each)
    for (u32 offset : stringOffsets) {
      stringTableBuffer.insert(stringTableBuffer.end(), reinterpret_cast<u8*>(&offset),
                               reinterpret_cast<u8*>(&offset) + 4);
    }

    // String data (null-terminated)
    for (const auto& entry : entries) {
      stringTableBuffer.insert(stringTableBuffer.end(), entry.resourceId.begin(),
                               entry.resourceId.end());
      stringTableBuffer.push_back(0); // null terminator
    }

    // Write string table
    output.write(reinterpret_cast<const char*>(stringTableBuffer.data()),
                 static_cast<std::streamsize>(stringTableBuffer.size()));

    // Pad to data section alignment
    u64 currentPos = static_cast<u64>(output.tellp());
    if (currentPos < dataOffset) {
      std::vector<u8> padding(dataOffset - currentPos, 0);
      output.write(reinterpret_cast<const char*>(padding.data()),
                   static_cast<std::streamsize>(padding.size()));
    }

    // Write resource data with alignment
    for (usize i = 0; i < entries.size(); ++i) {
      const auto& entry = entries[i];
      u64 targetOffset = dataOffset + dataOffsets[i];
      u64 currentOff = static_cast<u64>(output.tellp());

      // Pad to alignment
      if (currentOff < targetOffset) {
        std::vector<u8> alignPad(targetOffset - currentOff, 0);
        output.write(reinterpret_cast<const char*>(alignPad.data()),
                     static_cast<std::streamsize>(alignPad.size()));
      }

      // Write resource data
      output.write(reinterpret_cast<const char*>(entry.data.data()),
                   static_cast<std::streamsize>(entry.data.size()));
    }

    // Calculate CRC32 of header + resource table + string table
    std::vector<u8> tablesData;
    tablesData.reserve(headerBuffer.size() + resourceTableBuffer.size() + stringTableBuffer.size());
    tablesData.insert(tablesData.end(), headerBuffer.begin(), headerBuffer.end());
    tablesData.insert(tablesData.end(), resourceTableBuffer.begin(), resourceTableBuffer.end());
    tablesData.insert(tablesData.end(), stringTableBuffer.begin(), stringTableBuffer.end());
    u32 tablesCrc = calculateCrc32(tablesData.data(), tablesData.size());

    // Write footer (32 bytes)
    const char footerMagic[] = "NMRF";
    output.write(footerMagic, 4);

    output.write(reinterpret_cast<const char*>(&tablesCrc), 4);

    // Use deterministic timestamp if configured
    u64 timestamp = getBuildTimestamp();
    output.write(reinterpret_cast<const char*>(&timestamp), 8);

    u32 buildNumber = m_config.buildNumber;
    output.write(reinterpret_cast<const char*>(&buildNumber), 4);

    u8 reserved[12] = {0};
    output.write(reinterpret_cast<const char*>(reserved), 12);

    // Calculate content hash (SHA-256 of all resource data, first 128 bits)
    std::vector<u8> allResourceData;
    for (const auto& entry : entries) {
      allResourceData.insert(allResourceData.end(), entry.data.begin(), entry.data.end());
    }
    auto fullHash = calculateSha256(allResourceData.data(), allResourceData.size());

    // Update header with content hash
    output.seekp(0x30); // Offset of content hash in header
    output.write(reinterpret_cast<const char*>(fullHash.data()), 16);

    output.close();

    // Generate signature file if signing is enabled
    if (m_config.signPacks && !m_config.signingPrivateKeyPath.empty()) {
      // Read the pack file for signing
      std::ifstream packFile(outputPath, std::ios::binary);
      if (packFile.is_open()) {
        packFile.seekg(0, std::ios::end);
        auto packSize = packFile.tellg();
        packFile.seekg(0, std::ios::beg);

        std::vector<u8> packData(static_cast<usize>(packSize));
        packFile.read(reinterpret_cast<char*>(packData.data()), packSize);
        packFile.close();

        // Sign the pack data
        auto signResult = signData(packData, m_config.signingPrivateKeyPath);
        if (signResult.isOk()) {
          // Write signature to .sig file
          std::string sigPath = outputPath + ".sig";
          std::ofstream sigFile(sigPath, std::ios::binary);
          if (sigFile.is_open()) {
            sigFile.write(reinterpret_cast<const char*>(signResult.value().data()),
                          static_cast<std::streamsize>(signResult.value().size()));
            sigFile.close();
            logMessage("Generated signature: " + sigPath, false);
          }
        } else {
          logMessage("Warning: Failed to sign pack: " + signResult.error(), false);
        }
      }
    }

    return Result<void>::ok();

  } catch (const std::exception& e) {
    return Result<void>::error(std::string("Pack creation failed: ") + e.what());
  }
}

} // namespace NovelMind::editor
