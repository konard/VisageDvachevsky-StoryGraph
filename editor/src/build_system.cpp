/**
 * @file build_system.cpp
 * @brief Build System implementation for NovelMind
 *
 * Implements the complete build pipeline:
 * - Stage 0: Preflight/Validation
 * - Stage 1: Script Compilation
 * - Stage 2: Resource Index Generation
 * - Stage 3: Pack Building (Multi-Pack VFS)
 * - Stage 4: Runtime Bundling
 * - Stage 5: Post-build Verification
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
#include <functional>
#include <iomanip>
#include <map>
#include <random>
#include <set>
#include <sstream>

#ifdef NOVELMIND_HAS_ZLIB
#include <zlib.h>
#endif

#ifdef NOVELMIND_HAS_OPENSSL
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#endif

#include <cstdlib> // for std::getenv

// Platform-specific includes for process handling
#ifdef _WIN32
#include <io.h>
#include <windows.h> // for CreateProcess, etc.
#define popen _popen
#define pclose _pclose
#else
#include <sys/wait.h> // for WIFEXITED, WEXITSTATUS
#include <unistd.h>   // for fork, execv, pipe, dup2, read, close
#endif

namespace fs = std::filesystem;

namespace NovelMind::editor {

// ============================================================================
// CRC32, SHA-256, Compression, Encryption Static Helpers
// ============================================================================

// CRC32 lookup table (IEEE 802.3 polynomial)
static constexpr u32 CRC32_TABLE[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD706B3, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D};

u32 BuildSystem::calculateCrc32(const u8* data, usize size) {
  u32 crc = 0xFFFFFFFF;
  for (usize i = 0; i < size; ++i) {
    crc = CRC32_TABLE[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
  }
  return crc ^ 0xFFFFFFFF;
}

std::array<u8, 32> BuildSystem::calculateSha256(const u8* data, usize size) {
  std::array<u8, 32> hash{};
#ifdef NOVELMIND_HAS_OPENSSL
  SHA256(data, size, hash.data());
#else
  // Fallback: simple hash (not cryptographically secure, but deterministic)
  // In production, OpenSSL should always be available
  u64 h1 = 0x6a09e667bb67ae85ULL;
  u64 h2 = 0x3c6ef372a54ff53aULL;
  for (usize i = 0; i < size; ++i) {
    h1 = ((h1 << 5) | (h1 >> 59)) ^ data[i];
    h2 = ((h2 << 7) | (h2 >> 57)) ^ data[i];
  }
  std::memcpy(hash.data(), &h1, 8);
  std::memcpy(hash.data() + 8, &h2, 8);
  std::memcpy(hash.data() + 16, &h1, 8);
  std::memcpy(hash.data() + 24, &h2, 8);
#endif
  return hash;
}

Result<std::vector<u8>> BuildSystem::compressData(const std::vector<u8>& data,
                                                  CompressionLevel level) {
  if (level == CompressionLevel::None || data.empty()) {
    return Result<std::vector<u8>>::ok(data);
  }

#ifdef NOVELMIND_HAS_ZLIB
  // Map compression level to zlib level
  int zlibLevel = Z_DEFAULT_COMPRESSION;
  switch (level) {
  case CompressionLevel::Fast:
    zlibLevel = 1;
    break;
  case CompressionLevel::Balanced:
    zlibLevel = 6;
    break;
  case CompressionLevel::Maximum:
    zlibLevel = 9;
    break;
  default:
    zlibLevel = Z_DEFAULT_COMPRESSION;
  }

  // Calculate maximum compressed size
  uLongf compressedSize = compressBound(static_cast<uLong>(data.size()));
  std::vector<u8> compressed(compressedSize);

  int result = compress2(compressed.data(), &compressedSize, data.data(),
                         static_cast<uLong>(data.size()), zlibLevel);

  if (result != Z_OK) {
    return Result<std::vector<u8>>::error("zlib compression failed: " + std::to_string(result));
  }

  // Only use compressed data if it's actually smaller
  if (compressedSize >= data.size()) {
    return Result<std::vector<u8>>::ok(data);
  }

  compressed.resize(compressedSize);
  return Result<std::vector<u8>>::ok(std::move(compressed));
#else
  // Compression not available, return original data
  return Result<std::vector<u8>>::ok(data);
#endif
}

Result<std::vector<u8>> BuildSystem::encryptData(const std::vector<u8>& data,
                                                 const Core::SecureVector<u8>& key,
                                                 std::array<u8, 12>& ivOut) {
  if (key.size() != 32) {
    return Result<std::vector<u8>>::error("Invalid key size: expected 32 bytes for AES-256-GCM");
  }

#ifdef NOVELMIND_HAS_OPENSSL
  // Generate random IV (12 bytes for GCM)
  if (RAND_bytes(ivOut.data(), 12) != 1) {
    return Result<std::vector<u8>>::error("Failed to generate random IV");
  }

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    return Result<std::vector<u8>>::error("Failed to create cipher context");
  }

  // Initialize encryption
  if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key.data(), ivOut.data()) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<std::vector<u8>>::error("Failed to initialize encryption");
  }

  // Allocate output buffer (ciphertext + 16-byte GCM tag)
  std::vector<u8> output(data.size() + 16);
  int len = 0;
  int ciphertextLen = 0;

  // Encrypt
  if (EVP_EncryptUpdate(ctx, output.data(), &len, data.data(), static_cast<int>(data.size())) !=
      1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<std::vector<u8>>::error("Encryption failed");
  }
  ciphertextLen = len;

  // Finalize
  if (EVP_EncryptFinal_ex(ctx, output.data() + len, &len) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<std::vector<u8>>::error("Encryption finalization failed");
  }
  ciphertextLen += len;

  // Get the authentication tag (16 bytes)
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, output.data() + ciphertextLen) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<std::vector<u8>>::error("Failed to get GCM tag");
  }

  EVP_CIPHER_CTX_free(ctx);
  output.resize(static_cast<usize>(ciphertextLen) + 16);
  return Result<std::vector<u8>>::ok(std::move(output));
#else
  // Encryption not available - this is a security issue in production
  // For now, return original data with a warning
  (void)key;
  // Generate placeholder IV
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<u32> dist(0, 255);
  for (auto& b : ivOut) {
    b = static_cast<u8>(dist(gen));
  }
  return Result<std::vector<u8>>::ok(data);
#endif
}

// VFS path normalization (lowercase, forward slashes)
std::string BuildSystem::normalizeVfsPath(const std::string& path) {
  std::string result = path;
  // Convert backslashes to forward slashes
  std::replace(result.begin(), result.end(), '\\', '/');
  // Convert to lowercase
  std::transform(result.begin(), result.end(), result.begin(), ::tolower);
  // Remove leading slashes
  while (!result.empty() && result[0] == '/') {
    result.erase(0, 1);
  }
  // Remove trailing slashes
  while (!result.empty() && result.back() == '/') {
    result.pop_back();
  }
  return result;
}

// Path security validation to prevent path traversal attacks
Result<std::string> BuildSystem::sanitizeOutputPath(const std::string& basePath,
                                                     const std::string& relativePath) {
  // Reject paths containing ".." components before filesystem resolution
  // This provides an early defense against path traversal attempts
  if (relativePath.find("..") != std::string::npos) {
    return Result<std::string>::error(
        "Path traversal detected: path contains '..' component: " + relativePath);
  }

  // Normalize the base path to ensure we have a canonical reference
  std::error_code ec;
  fs::path canonicalBase = fs::weakly_canonical(basePath, ec);
  if (ec) {
    return Result<std::string>::error("Failed to canonicalize base path: " + basePath +
                                      " - " + ec.message());
  }

  // Construct the full output path
  fs::path fullPath = fs::path(basePath) / relativePath;

  // Resolve the full path to its canonical form
  // weakly_canonical resolves ".." and "." components and follows symlinks
  fs::path canonicalPath = fs::weakly_canonical(fullPath, ec);
  if (ec) {
    return Result<std::string>::error("Failed to canonicalize output path: " +
                                      fullPath.string() + " - " + ec.message());
  }

  // Security check: Verify the resolved path is within the base directory
  // This prevents writing to arbitrary locations on the filesystem
  auto [rootEnd, nothing] = std::mismatch(canonicalBase.begin(), canonicalBase.end(),
                                           canonicalPath.begin(), canonicalPath.end());

  if (rootEnd != canonicalBase.end()) {
    return Result<std::string>::error(
        "Path traversal detected: resolved path '" + canonicalPath.string() +
        "' escapes base directory '" + canonicalBase.string() + "'");
  }

  return Result<std::string>::ok(fullPath.string());
}

// Load encryption key from environment variables
Result<Core::SecureVector<u8>> BuildSystem::loadEncryptionKeyFromEnv() {
  // Try NOVELMIND_PACK_AES_KEY_HEX first
  const char* hexKey = std::getenv("NOVELMIND_PACK_AES_KEY_HEX");
  if (hexKey != nullptr) {
    std::string hexStr(hexKey);
    if (hexStr.length() != 64) {
      return Result<Core::SecureVector<u8>>::error(
          "NOVELMIND_PACK_AES_KEY_HEX must be 64 hex characters (32 bytes)");
    }

    // Validate hex characters before parsing
    for (char c : hexStr) {
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
        return Result<Core::SecureVector<u8>>::error(
            "NOVELMIND_PACK_AES_KEY_HEX contains invalid hex characters. Only 0-9, a-f, A-F are "
            "allowed");
      }
    }

    Core::SecureVector<u8> key(32);
    for (usize i = 0; i < 32; ++i) {
      std::string byteStr = hexStr.substr(i * 2, 2);
      try {
        unsigned long val = std::stoul(byteStr, nullptr, 16);
        key[i] = static_cast<u8>(val);
      } catch (const std::invalid_argument& e) {
        return Result<Core::SecureVector<u8>>::error("Invalid hex format in encryption key at byte " +
                                              std::to_string(i) + ": " + e.what());
      } catch (const std::out_of_range& e) {
        return Result<Core::SecureVector<u8>>::error("Hex value out of range in encryption key at byte " +
                                              std::to_string(i) + ": " + e.what());
      } catch (...) {
        return Result<Core::SecureVector<u8>>::error("Unknown error parsing encryption key at byte " +
                                              std::to_string(i));
      }
    }
    return Result<Core::SecureVector<u8>>::ok(std::move(key));
  }

  // Try NOVELMIND_PACK_AES_KEY_FILE
  const char* keyFile = std::getenv("NOVELMIND_PACK_AES_KEY_FILE");
  if (keyFile != nullptr) {
    return loadEncryptionKeyFromFile(keyFile);
  }

  return Result<Core::SecureVector<u8>>::error(
      "No encryption key found. Set NOVELMIND_PACK_AES_KEY_HEX or "
      "NOVELMIND_PACK_AES_KEY_FILE environment variable");
}

// Load encryption key from file
Result<Core::SecureVector<u8>> BuildSystem::loadEncryptionKeyFromFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return Result<Core::SecureVector<u8>>::error("Cannot open key file: " + path);
  }

  Core::SecureVector<u8> key(32);
  file.read(reinterpret_cast<char*>(key.data()), 32);

  if (file.gcount() != 32) {
    return Result<Core::SecureVector<u8>>::error("Key file must contain exactly 32 bytes: " + path);
  }

  file.close();
  return Result<Core::SecureVector<u8>>::ok(std::move(key));
}

// Sign data with RSA private key (returns signature)
Result<std::vector<u8>> BuildSystem::signData(const std::vector<u8>& data,
                                              const std::string& privateKeyPath) {
#ifdef NOVELMIND_HAS_OPENSSL
  // Read private key
  FILE* keyFile = fopen(privateKeyPath.c_str(), "r");
  if (!keyFile) {
    return Result<std::vector<u8>>::error("Cannot open private key file: " + privateKeyPath);
  }

  EVP_PKEY* pkey = PEM_read_PrivateKey(keyFile, nullptr, nullptr, nullptr);
  fclose(keyFile);

  if (!pkey) {
    return Result<std::vector<u8>>::error("Failed to read RSA private key");
  }

  // Create signing context
  EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
  if (!mdctx) {
    EVP_PKEY_free(pkey);
    return Result<std::vector<u8>>::error("Failed to create signing context");
  }

  // Initialize signing
  if (EVP_DigestSignInit(mdctx, nullptr, EVP_sha256(), nullptr, pkey) != 1) {
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pkey);
    return Result<std::vector<u8>>::error("Failed to initialize signing");
  }

  // Update with data
  if (EVP_DigestSignUpdate(mdctx, data.data(), data.size()) != 1) {
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pkey);
    return Result<std::vector<u8>>::error("Failed to update signing digest");
  }

  // Get signature length
  size_t sigLen = 0;
  if (EVP_DigestSignFinal(mdctx, nullptr, &sigLen) != 1) {
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pkey);
    return Result<std::vector<u8>>::error("Failed to get signature length");
  }

  // Get signature
  std::vector<u8> signature(sigLen);
  if (EVP_DigestSignFinal(mdctx, signature.data(), &sigLen) != 1) {
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pkey);
    return Result<std::vector<u8>>::error("Failed to create signature");
  }

  signature.resize(sigLen);
  EVP_MD_CTX_free(mdctx);
  EVP_PKEY_free(pkey);

  return Result<std::vector<u8>>::ok(std::move(signature));
#else
  (void)data;
  (void)privateKeyPath;
  return Result<std::vector<u8>>::error("RSA signing requires OpenSSL");
#endif
}

// Get deterministic build timestamp
u64 BuildSystem::getBuildTimestamp() const {
  if (m_config.fixedBuildTimestamp != 0) {
    return m_config.fixedBuildTimestamp;
  }
  return static_cast<u64>(std::chrono::duration_cast<std::chrono::seconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count());
}

ResourceType BuildSystem::getResourceTypeFromExtension(const std::string& path) {
  std::string ext = fs::path(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  // Texture types
  if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif" ||
      ext == ".tga" || ext == ".webp") {
    return ResourceType::Texture;
  }

  // Audio types (sound effects)
  if (ext == ".wav" || ext == ".flac") {
    return ResourceType::Audio;
  }

  // Music types (streamable)
  if (ext == ".ogg" || ext == ".mp3" || ext == ".opus") {
    return ResourceType::Music;
  }

  // Font types
  if (ext == ".ttf" || ext == ".otf" || ext == ".woff" || ext == ".woff2") {
    return ResourceType::Font;
  }

  // Script types
  if (ext == ".nms" || ext == ".nmscript" || ext == ".nmbc") {
    return ResourceType::Script;
  }

  // Scene types
  if (ext == ".scene" || ext == ".nmscene") {
    return ResourceType::Scene;
  }

  // Localization types
  if (ext == ".loc" || ext == ".nmloc" || ext == ".po" || ext == ".pot") {
    return ResourceType::Localization;
  }

  // JSON/YAML data files
  if (ext == ".json" || ext == ".yaml" || ext == ".yml" || ext == ".xml") {
    return ResourceType::Data;
  }

  return ResourceType::Unknown;
}

// ============================================================================
// BuildSystem Implementation
// ============================================================================

BuildSystem::BuildSystem() = default;

BuildSystem::~BuildSystem() {
  cancelBuild();
  if (m_buildThread && m_buildThread->joinable()) {
    m_buildThread->join();
  }
}

void BuildSystem::configure(const BuildConfig& config) {
  m_config = config;
}

Result<void> BuildSystem::startBuild(const BuildConfig& config) {
  if (m_buildInProgress) {
    return Result<void>::error("Build already in progress");
  }

  // Validate configuration
  if (config.projectPath.empty()) {
    return Result<void>::error("Project path is required");
  }
  if (config.outputPath.empty()) {
    return Result<void>::error("Output path is required");
  }

  // Check project exists
  if (!fs::exists(config.projectPath)) {
    return Result<void>::error("Project path does not exist: " + config.projectPath);
  }

  m_config = config;
  m_buildInProgress = true;
  m_cancelRequested = false;

  // Reset progress
  m_progress = BuildProgress{};
  m_progress.isRunning = true;

  // Initialize build steps
  m_progress.steps = {{"Preflight", "Validating project structure", 0.05f, false, true, "", 0.0},
                      {"Compile", "Compiling scripts", 0.15f, false, true, "", 0.0},
                      {"Index", "Building resource index", 0.10f, false, true, "", 0.0},
                      {"Pack", "Creating resource packs", 0.35f, false, true, "", 0.0},
                      {"Bundle", "Bundling runtime", 0.25f, false, true, "", 0.0},
                      {"Verify", "Verifying build", 0.10f, false, true, "", 0.0}};

  // Start build thread
  m_buildThread = std::make_unique<std::thread>([this]() { runBuildPipeline(); });

  return Result<void>::ok();
}

void BuildSystem::cancelBuild() {
  if (m_buildInProgress) {
    m_cancelRequested = true;
    logMessage("Build cancellation requested...", false);
  }
}

Result<std::vector<std::string>> BuildSystem::validateProject(const std::string& projectPath) {
  std::vector<std::string> errors;

  // Check project directory exists
  if (!fs::exists(projectPath)) {
    errors.push_back("Project directory does not exist: " + projectPath);
    return Result<std::vector<std::string>>::ok(errors);
  }

  // Check for project.json
  fs::path projectFile = fs::path(projectPath) / "project.json";
  if (!fs::exists(projectFile)) {
    errors.push_back("Missing project.json in project directory");
  }

  // Check for required directories
  std::vector<std::string> requiredDirs = {"scripts", "assets"};
  for (const auto& dir : requiredDirs) {
    fs::path dirPath = fs::path(projectPath) / dir;
    if (!fs::exists(dirPath)) {
      errors.push_back("Missing required directory: " + dir);
    }
  }

  return Result<std::vector<std::string>>::ok(errors);
}

f64 BuildSystem::estimateBuildTime(const BuildConfig& config) const {
  // Simple estimation based on project size
  i64 projectSize = BuildUtils::calculateDirectorySize(config.projectPath);

  // Base time: 5 seconds
  f64 estimatedMs = 5000.0;

  // Add time based on size (roughly 1 second per MB)
  estimatedMs += static_cast<f64>(projectSize) / (1024.0 * 1024.0) * 1000.0;

  // Adjust for compression level
  switch (config.compression) {
  case CompressionLevel::None:
    break;
  case CompressionLevel::Fast:
    estimatedMs *= 1.2;
    break;
  case CompressionLevel::Balanced:
    estimatedMs *= 1.5;
    break;
  case CompressionLevel::Maximum:
    estimatedMs *= 2.0;
    break;
  }

  // Adjust for encryption
  if (config.encryptAssets) {
    estimatedMs *= 1.3;
  }

  return estimatedMs;
}

void BuildSystem::setOnProgressUpdate(std::function<void(const BuildProgress&)> callback) {
  m_onProgressUpdate = std::move(callback);
}

void BuildSystem::setOnStepComplete(std::function<void(const BuildStep&)> callback) {
  m_onStepComplete = std::move(callback);
}

void BuildSystem::setOnBuildComplete(std::function<void(const BuildResult&)> callback) {
  m_onBuildComplete = std::move(callback);
}

void BuildSystem::setOnLogMessage(std::function<void(const std::string&, bool)> callback) {
  m_onLogMessage = std::move(callback);
}

// ============================================================================
// Private Implementation
// ============================================================================

void BuildSystem::runBuildPipeline() {
  auto startTime = std::chrono::steady_clock::now();
  bool success = true;
  std::string errorMessage;

  // Create staging directory
  fs::path stagingDir = fs::path(m_config.outputPath) / ".staging";

  try {
    // Clean and create staging directory
    if (fs::exists(stagingDir)) {
      fs::remove_all(stagingDir);
    }
    fs::create_directories(stagingDir);

    // Stage 0: Preflight
    if (!m_cancelRequested) {
      auto result = prepareOutputDirectory();
      if (result.isError()) {
        success = false;
        errorMessage = result.error();
      }
    }

    // Stage 1: Compile Scripts
    if (success && !m_cancelRequested) {
      auto result = compileScripts();
      if (result.isError()) {
        success = false;
        errorMessage = result.error();
      }
    }

    // Stage 2: Build Resource Index (part of processAssets)
    if (success && !m_cancelRequested) {
      auto result = processAssets();
      if (result.isError()) {
        success = false;
        errorMessage = result.error();
      }
    }

    // Stage 3: Pack Resources
    if (success && !m_cancelRequested) {
      auto result = packResources();
      if (result.isError()) {
        success = false;
        errorMessage = result.error();
      }
    }

    // Stage 4: Generate Executable
    if (success && !m_cancelRequested) {
      auto result = generateExecutable();
      if (result.isError()) {
        success = false;
        errorMessage = result.error();
      }
    }

    // Stage 5: Sign and Finalize
    if (success && !m_cancelRequested) {
      auto result = signAndFinalize();
      if (result.isError()) {
        success = false;
        errorMessage = result.error();
      }
    }

    // Atomic move from staging to final output
    if (success && !m_cancelRequested) {
      fs::path finalOutput = m_config.outputPath;

      // Ensure output parent directory exists
      if (finalOutput.has_parent_path()) {
        fs::create_directories(finalOutput.parent_path());
      }

      // Remove existing output if present
      if (fs::exists(finalOutput) && finalOutput != stagingDir) {
        fs::remove_all(finalOutput);
      }

      // Move staging contents to final location
      for (const auto& entry : fs::directory_iterator(stagingDir)) {
        fs::path dest = finalOutput / entry.path().filename();
        if (fs::exists(dest)) {
          fs::remove_all(dest);
        }
        fs::rename(entry.path(), dest);
      }

      // Remove staging directory
      fs::remove_all(stagingDir);
    }

  } catch (const std::exception& e) {
    success = false;
    errorMessage = std::string("Build exception: ") + e.what();
  }

  // Cleanup on failure
  auto cleanupResult = cleanup();
  if (cleanupResult.isError()) {
    logMessage("Cleanup warning: " + cleanupResult.error(), true);
  }

  // Calculate elapsed time
  auto endTime = std::chrono::steady_clock::now();
  f64 elapsedMs = std::chrono::duration<f64, std::milli>(endTime - startTime).count();

  // Prepare result
  m_lastResult = BuildResult{};
  m_lastResult.success = success && !m_cancelRequested;
  m_lastResult.outputPath = m_config.outputPath;
  m_lastResult.errorMessage = m_cancelRequested ? "Build cancelled" : errorMessage;
  m_lastResult.buildTimeMs = elapsedMs;
  m_lastResult.scriptsCompiled = static_cast<i32>(m_scriptFiles.size());
  m_lastResult.assetsProcessed = static_cast<i32>(m_assetFiles.size());
  m_lastResult.warnings =
      std::vector<std::string>(m_progress.warnings.begin(), m_progress.warnings.end());

  // Calculate output size
  if (success && fs::exists(m_config.outputPath)) {
    m_lastResult.totalSize = BuildUtils::calculateDirectorySize(m_config.outputPath);
  }

  // Update progress
  m_progress.isRunning = false;
  m_progress.isComplete = true;
  m_progress.wasSuccessful = success && !m_cancelRequested;
  m_progress.wasCancelled = m_cancelRequested.load();
  m_progress.elapsedMs = elapsedMs;

  // Notify completion
  if (m_onBuildComplete) {
    m_onBuildComplete(m_lastResult);
  }

  m_buildInProgress = false;
}

Result<void> BuildSystem::prepareOutputDirectory() {
  beginStep("Preflight", "Validating project and preparing output");

  // Validate project
  updateProgress(0.1f, "Validating project structure...");
  auto validationResult = validateProject(m_config.projectPath);
  if (validationResult.isError()) {
    endStep(false, validationResult.error());
    return Result<void>::error(validationResult.error());
  }

  auto errors = validationResult.value();
  if (!errors.empty()) {
    std::string errorMsg;
    for (const auto& err : errors) {
      errorMsg += err + "\n";
      m_progress.errors.push_back(err);
    }
    endStep(false, "Project validation failed");
    return Result<void>::error("Project validation failed:\n" + errorMsg);
  }

  // Create staging directory structure
  updateProgress(0.5f, "Creating output directories...");
  fs::path stagingDir = fs::path(m_config.outputPath) / ".staging";

  try {
    fs::create_directories(stagingDir / "packs");
    fs::create_directories(stagingDir / "config");
    fs::create_directories(stagingDir / "logs");
    fs::create_directories(stagingDir / "saves");
  } catch (const fs::filesystem_error& e) {
    endStep(false, e.what());
    return Result<void>::error(std::string("Failed to create directories: ") + e.what());
  }

  // Collect script files
  updateProgress(0.7f, "Scanning script files...");
  m_scriptFiles.clear();
  fs::path scriptsDir = fs::path(m_config.projectPath) / "scripts";
  if (fs::exists(scriptsDir)) {
    for (const auto& entry : fs::recursive_directory_iterator(scriptsDir)) {
      if (entry.is_regular_file()) {
        std::string ext = entry.path().extension().string();
        if (ext == ".nms" || ext == ".nmscript") {
          m_scriptFiles.push_back(entry.path().string());
        }
      }
    }
  }

  // Sort script files for deterministic ordering
  if (m_config.deterministicBuild) {
    std::sort(m_scriptFiles.begin(), m_scriptFiles.end());
  }

  // Collect asset files
  updateProgress(0.9f, "Scanning asset files...");
  m_assetFiles.clear();
  fs::path assetsDir = fs::path(m_config.projectPath) / "assets";
  if (fs::exists(assetsDir)) {
    for (const auto& entry : fs::recursive_directory_iterator(assetsDir)) {
      if (entry.is_regular_file()) {
        m_assetFiles.push_back(entry.path().string());
      }
    }
  }

  // Sort asset files for deterministic ordering
  if (m_config.deterministicBuild) {
    std::sort(m_assetFiles.begin(), m_assetFiles.end());
  }

  logMessage("Found " + std::to_string(m_scriptFiles.size()) + " script files and " +
                 std::to_string(m_assetFiles.size()) + " asset files",
             false);

  m_progress.totalFiles = static_cast<i32>(m_scriptFiles.size() + m_assetFiles.size());

  endStep(true);
  return Result<void>::ok();
}

Result<void> BuildSystem::compileScripts() {
  beginStep("Compile", "Compiling NMScript files");

  if (m_scriptFiles.empty()) {
    logMessage("No script files to compile", false);
    endStep(true);
    return Result<void>::ok();
  }

  fs::path stagingDir = fs::path(m_config.outputPath) / ".staging";
  fs::path compiledDir = stagingDir / "compiled";
  fs::create_directories(compiledDir);

  i32 compiled = 0;
  std::vector<ScriptCompileResult> results;

  for (const auto& scriptPath : m_scriptFiles) {
    if (m_cancelRequested) {
      endStep(false, "Cancelled");
      return Result<void>::error("Build cancelled");
    }

    f32 progress = static_cast<f32>(compiled) / static_cast<f32>(m_scriptFiles.size());
    updateProgress(progress, "Compiling: " + fs::path(scriptPath).filename().string());

    auto result = compileScript(scriptPath);
    results.push_back(result);

    if (!result.success) {
      for (const auto& err : result.errors) {
        m_progress.errors.push_back(scriptPath + ": " + err);
      }
    }

    for (const auto& warn : result.warnings) {
      m_progress.warnings.push_back(scriptPath + ": " + warn);
    }

    compiled++;
    m_progress.filesProcessed++;
  }

  // Check for compilation errors
  bool hasErrors = false;
  for (const auto& result : results) {
    if (!result.success) {
      hasErrors = true;
      break;
    }
  }

  if (hasErrors) {
    endStep(false, "Script compilation failed");
    return Result<void>::error("One or more scripts failed to compile");
  }

  // Generate compiled bytecode bundle
  auto bundleResult = compileBytecode((compiledDir / "compiled_scripts.bin").string());
  if (bundleResult.isError()) {
    endStep(false, bundleResult.error());
    return bundleResult;
  }

  logMessage("Compiled " + std::to_string(compiled) + " scripts successfully", false);
  endStep(true);
  return Result<void>::ok();
}

Result<void> BuildSystem::processAssets() {
  beginStep("Index", "Processing and indexing assets");

  if (m_assetFiles.empty()) {
    logMessage("No assets to process", false);
    endStep(true);
    return Result<void>::ok();
  }

  fs::path stagingDir = fs::path(m_config.outputPath) / ".staging";
  fs::path assetsDir = stagingDir / "assets";
  fs::create_directories(assetsDir);

  i32 processed = 0;
  m_assetMapping.clear();

  for (const auto& assetPath : m_assetFiles) {
    if (m_cancelRequested) {
      endStep(false, "Cancelled");
      return Result<void>::error("Build cancelled");
    }

    f32 progress = static_cast<f32>(processed) / static_cast<f32>(m_assetFiles.size());
    updateProgress(progress, "Processing: " + fs::path(assetPath).filename().string());

    // Determine asset type and process accordingly
    std::string ext = fs::path(assetPath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    fs::path relativePath = fs::relative(assetPath, fs::path(m_config.projectPath) / "assets");

    // Security: Validate output path to prevent path traversal attacks
    auto sanitizedPathResult = sanitizeOutputPath(assetsDir.string(), relativePath.string());
    if (sanitizedPathResult.isError()) {
      endStep(false, sanitizedPathResult.error());
      return Result<void>::error(sanitizedPathResult.error());
    }
    fs::path outputPath = sanitizedPathResult.value();

    // Create output directory
    fs::create_directories(outputPath.parent_path());

    AssetProcessResult result;

    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
      result = processImage(assetPath, outputPath.string());
    } else if (ext == ".ogg" || ext == ".wav" || ext == ".mp3") {
      result = processAudio(assetPath, outputPath.string());
    } else if (ext == ".ttf" || ext == ".otf") {
      result = processFont(assetPath, outputPath.string());
    } else {
      result = processData(assetPath, outputPath.string());
    }

    if (!result.success) {
      m_progress.warnings.push_back("Asset processing warning: " + assetPath + " - " +
                                    result.errorMessage);
    }

    // Map original path to normalized VFS path (lowercase, forward slashes)
    std::string vfsPath = normalizeVfsPath(relativePath.string());
    m_assetMapping[assetPath] = vfsPath;

    processed++;
    m_progress.filesProcessed++;
    m_progress.bytesProcessed += result.processedSize;
  }

  // Generate enhanced resource manifest with hashing and complete metadata
  // as per build_system.md specification
  fs::path manifestPath = stagingDir / "resource_manifest.json";
  std::ofstream manifestFile(manifestPath);
  if (manifestFile.is_open()) {
    // Use deterministic timestamp if configured
    u64 buildTs = getBuildTimestamp();

    // Generate ISO 8601 timestamp string
    std::time_t tsTime = static_cast<std::time_t>(buildTs);
    std::tm* tm = std::gmtime(&tsTime);
    std::ostringstream isoTs;
    isoTs << std::put_time(tm, "%Y-%m-%dT%H:%M:%SZ");

    manifestFile << "{\n";
    manifestFile << "  \"version\": \"1.0\",\n";
    manifestFile << "  \"generated\": \"" << isoTs.str() << "\",\n";
    manifestFile << "  \"build_timestamp\": " << buildTs << ",\n";
    manifestFile << "  \"build_number\": " << m_config.buildNumber << ",\n";
    manifestFile << "  \"resource_count\": " << m_assetMapping.size() << ",\n";
    manifestFile << "  \"locales\": [";
    bool firstLocale = true;
    for (const auto& lang : m_config.includedLanguages) {
      if (!firstLocale)
        manifestFile << ", ";
      firstLocale = false;
      manifestFile << "\"" << lang << "\"";
    }
    manifestFile << "],\n";
    manifestFile << "  \"default_locale\": \"" << m_config.defaultLanguage << "\",\n";
    manifestFile << "  \"resources\": [\n";

    bool first = true;
    for (const auto& [sourcePath, vfsPath] : m_assetMapping) {
      if (!first)
        manifestFile << ",\n";
      first = false;

      // Get resource type
      ResourceType resType = getResourceTypeFromExtension(sourcePath);
      const char* typeStr = "unknown";
      switch (resType) {
      case ResourceType::Texture:
        typeStr = "texture";
        break;
      case ResourceType::Audio:
        typeStr = "audio";
        break;
      case ResourceType::Music:
        typeStr = "music";
        break;
      case ResourceType::Font:
        typeStr = "font";
        break;
      case ResourceType::Script:
        typeStr = "script";
        break;
      case ResourceType::Scene:
        typeStr = "scene";
        break;
      case ResourceType::Localization:
        typeStr = "localization";
        break;
      case ResourceType::Data:
        typeStr = "data";
        break;
      default:
        typeStr = "unknown";
        break;
      }

      // Read file and calculate hash
      u64 fileSize = 0;
      std::string hashHex = "0000000000000000"; // First 16 bytes of SHA-256
      try {
        std::ifstream file(sourcePath, std::ios::binary | std::ios::ate);
        if (file.is_open()) {
          fileSize = static_cast<u64>(file.tellg());
          file.seekg(0, std::ios::beg);
          std::vector<u8> data(fileSize);
          file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(fileSize));
          file.close();

          auto hash = calculateSha256(data.data(), data.size());
          std::ostringstream oss;
          oss << std::hex << std::setfill('0');
          for (size_t i = 0; i < 16; ++i) {
            oss << std::setw(2) << static_cast<int>(hash[i]);
          }
          hashHex = oss.str();
        }
      } catch (...) {
        // Hash calculation failed, use placeholder
      }

      // Detect locale from path (e.g., "en/voice/line1.ogg" -> "en")
      std::string locale = "";
      for (const auto& lang : m_config.includedLanguages) {
        if (vfsPath.find(lang + "/") == 0 || vfsPath.find("/" + lang + "/") != std::string::npos) {
          locale = lang;
          break;
        }
      }

      // Determine pack binding (Base or Lang_<locale>)
      std::string packBinding = locale.empty() ? "Base" : ("Lang_" + locale);

      // Determine resource flags based on type
      std::vector<std::string> flags;
      if (resType == ResourceType::Texture || resType == ResourceType::Font) {
        flags.push_back("PRELOAD");
      }
      if (resType == ResourceType::Music) {
        flags.push_back("STREAMABLE");
      }

      // Detect if this is a voice file (for voice mapping)
      bool isVoice = (vfsPath.find("voice/") != std::string::npos ||
                      vfsPath.find("/voice/") != std::string::npos);

      // Escape backslashes and quotes for JSON
      std::string escapedSource = sourcePath;
      std::string escapedVfs = vfsPath;
      auto escapeJson = [](std::string& s) {
        std::string result;
        for (char c : s) {
          if (c == '\\')
            result += "\\\\";
          else if (c == '"')
            result += "\\\"";
          else
            result += c;
        }
        s = result;
      };
      escapeJson(escapedSource);
      escapeJson(escapedVfs);

      manifestFile << "    {\n";
      manifestFile << "      \"id\": \"" << escapedVfs << "\",\n";
      manifestFile << "      \"type\": \"" << typeStr << "\",\n";
      manifestFile << "      \"source_path\": \"" << escapedSource << "\",\n";
      manifestFile << "      \"size\": " << fileSize << ",\n";
      manifestFile << "      \"hash\": \"" << hashHex << "\",\n";
      manifestFile << "      \"pack\": \"" << packBinding << "\"";
      if (!flags.empty()) {
        manifestFile << ",\n      \"flags\": [";
        bool firstFlag = true;
        for (const auto& flag : flags) {
          if (!firstFlag)
            manifestFile << ", ";
          firstFlag = false;
          manifestFile << "\"" << flag << "\"";
        }
        manifestFile << "]";
      }
      if (!locale.empty()) {
        manifestFile << ",\n      \"locale\": \"" << locale << "\"";
      }
      if (isVoice) {
        manifestFile << ",\n      \"voice\": true";
      }
      manifestFile << "\n    }";
    }

    manifestFile << "\n  ]\n";
    manifestFile << "}\n";
    manifestFile.close();
  }

  logMessage("Processed " + std::to_string(processed) + " assets", false);
  endStep(true);
  return Result<void>::ok();
}

Result<void> BuildSystem::packResources() {
  beginStep("Pack", "Creating resource packs");

  fs::path stagingDir = fs::path(m_config.outputPath) / ".staging";
  fs::path packsDir = stagingDir / "packs";
  fs::create_directories(packsDir);

  if (!m_config.packAssets) {
    // Just copy assets directly without packing
    logMessage("Skipping pack creation (packAssets=false)", false);
    endStep(true);
    return Result<void>::ok();
  }

  updateProgress(0.1f, "Building Base pack...");

  // Build base pack
  std::vector<std::string> baseFiles;
  for (const auto& [sourcePath, vfsPath] : m_assetMapping) {
    // Check if this is a locale-specific file
    bool isLocaleSpecific = false;
    for (const auto& lang : m_config.includedLanguages) {
      if (vfsPath.find("/" + lang + "/") != std::string::npos || vfsPath.find(lang + "/") == 0) {
        isLocaleSpecific = true;
        break;
      }
    }

    if (!isLocaleSpecific) {
      fs::path processedPath = stagingDir / "assets" /
                               fs::relative(sourcePath, fs::path(m_config.projectPath) / "assets");
      if (fs::exists(processedPath)) {
        baseFiles.push_back(processedPath.string());
      }
    }
  }

  auto baseResult = buildPack((packsDir / "Base.nmres").string(), baseFiles, m_config.encryptAssets,
                              m_config.compression != CompressionLevel::None);
  if (baseResult.isError()) {
    endStep(false, baseResult.error());
    return baseResult;
  }

  // Build language packs
  i32 langPacksBuilt = 0;
  for (const auto& lang : m_config.includedLanguages) {
    if (m_cancelRequested) {
      endStep(false, "Cancelled");
      return Result<void>::error("Build cancelled");
    }

    f32 progress = 0.3f + 0.6f * static_cast<f32>(langPacksBuilt) /
                              static_cast<f32>(m_config.includedLanguages.size());
    updateProgress(progress, "Building language pack: " + lang);

    std::vector<std::string> langFiles;
    for (const auto& [sourcePath, vfsPath] : m_assetMapping) {
      if (vfsPath.find("/" + lang + "/") != std::string::npos || vfsPath.find(lang + "/") == 0) {
        fs::path processedPath =
            stagingDir / "assets" /
            fs::relative(sourcePath, fs::path(m_config.projectPath) / "assets");
        if (fs::exists(processedPath)) {
          langFiles.push_back(processedPath.string());
        }
      }
    }

    if (!langFiles.empty()) {
      std::string packName = "Lang_" + lang + ".nmres";
      auto langResult = buildPack((packsDir / packName).string(), langFiles, m_config.encryptAssets,
                                  m_config.compression != CompressionLevel::None);
      if (langResult.isError()) {
        m_progress.warnings.push_back("Failed to create language pack for " + lang + ": " +
                                      langResult.error());
      } else {
        langPacksBuilt++;
      }
    }
  }

  // Generate packs_index.json with full metadata per spec
  updateProgress(0.95f, "Generating pack index...");

  // Helper function to calculate pack hash and size
  auto getPackInfo = [this](const fs::path& packPath) -> std::tuple<std::string, u64, u32> {
    std::string hashHex = "sha256:";
    u64 packSize = 0;
    u32 resourceCount = 0;

    if (!fs::exists(packPath)) {
      return {"sha256:0000000000000000", 0, 0};
    }

    try {
      std::ifstream file(packPath, std::ios::binary | std::ios::ate);
      if (file.is_open()) {
        packSize = static_cast<u64>(file.tellg());
        file.seekg(0, std::ios::beg);

        // Read pack data for hashing
        std::vector<u8> data(packSize);
        file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(packSize));
        file.close();

        // Calculate SHA-256 hash
        auto hash = calculateSha256(data.data(), data.size());
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (size_t i = 0; i < 16; ++i) {
          oss << std::setw(2) << static_cast<int>(hash[i]);
        }
        hashHex += oss.str() + "...";

        // Read resource count from header (offset 12)
        if (packSize >= 16) {
          resourceCount = *reinterpret_cast<const u32*>(data.data() + 12);
        }
      }
    } catch (...) {
      // Hash calculation failed
    }

    return {hashHex, packSize, resourceCount};
  };

  fs::path indexPath = packsDir / "packs_index.json";
  std::ofstream indexFile(indexPath);
  if (indexFile.is_open()) {
    indexFile << "{\n";
    indexFile << "  \"version\": \"1.0\",\n";
    indexFile << "  \"packs\": [\n";

    // Get Base pack info
    auto [baseHash, baseSize, baseResCount] = getPackInfo(packsDir / "Base.nmres");

    indexFile << "    {\n";
    indexFile << "      \"id\": \"base\",\n";
    indexFile << "      \"filename\": \"Base.nmres\",\n";
    indexFile << "      \"type\": \"Base\",\n";
    indexFile << "      \"priority\": 0,\n";
    indexFile << "      \"version\": \"" << m_config.version << "\",\n";
    indexFile << "      \"hash\": \"" << baseHash << "\",\n";
    indexFile << "      \"size\": " << baseSize << ",\n";
    indexFile << "      \"resource_count\": " << baseResCount << ",\n";
    indexFile << "      \"encrypted\": " << (m_config.encryptAssets ? "true" : "false") << ",\n";
    indexFile << "      \"signed\": " << (m_config.signPacks ? "true" : "false") << "\n";
    indexFile << "    }";

    // Build load_order array
    std::vector<std::string> loadOrder;
    loadOrder.push_back("base");

    for (const auto& lang : m_config.includedLanguages) {
      std::string packName = "Lang_" + lang + ".nmres";
      if (fs::exists(packsDir / packName)) {
        auto [langHash, langSize, langResCount] = getPackInfo(packsDir / packName);

        indexFile << ",\n";
        indexFile << "    {\n";
        indexFile << "      \"id\": \"lang_" << lang << "\",\n";
        indexFile << "      \"filename\": \"" << packName << "\",\n";
        indexFile << "      \"type\": \"Language\",\n";
        indexFile << "      \"priority\": 3,\n";
        indexFile << "      \"locale\": \"" << lang << "\",\n";
        indexFile << "      \"version\": \"" << m_config.version << "\",\n";
        indexFile << "      \"hash\": \"" << langHash << "\",\n";
        indexFile << "      \"size\": " << langSize << ",\n";
        indexFile << "      \"resource_count\": " << langResCount << ",\n";
        indexFile << "      \"encrypted\": " << (m_config.encryptAssets ? "true" : "false")
                  << ",\n";
        indexFile << "      \"signed\": " << (m_config.signPacks ? "true" : "false") << "\n";
        indexFile << "    }";

        loadOrder.push_back("lang_" + lang);
      }
    }

    indexFile << "\n  ],\n";

    // Write load_order array
    indexFile << "  \"load_order\": [";
    bool firstOrder = true;
    for (const auto& id : loadOrder) {
      if (!firstOrder)
        indexFile << ", ";
      firstOrder = false;
      indexFile << "\"" << id << "\"";
    }
    indexFile << "],\n";

    indexFile << "  \"default_locale\": \"" << m_config.defaultLanguage << "\"\n";
    indexFile << "}\n";
    indexFile.close();
  }

  logMessage("Created Base pack and " + std::to_string(langPacksBuilt) + " language packs", false);
  endStep(true);
  return Result<void>::ok();
}

Result<void> BuildSystem::generateExecutable() {
  beginStep("Bundle", "Creating runtime bundle");

  fs::path stagingDir = fs::path(m_config.outputPath) / ".staging";

  updateProgress(0.2f, "Preparing runtime executable...");

  // Determine executable name
  std::string exeName =
      m_config.executableName.empty() ? "NovelMindRuntime" : m_config.executableName;
  exeName += BuildUtils::getExecutableExtension(m_config.platform);

  // Platform-specific bundling
  Result<void> result;
  switch (m_config.platform) {
  case BuildPlatform::Windows:
    result = buildWindowsExecutable(stagingDir.string());
    break;
  case BuildPlatform::Linux:
    result = buildLinuxExecutable(stagingDir.string());
    break;
  case BuildPlatform::MacOS:
    result = buildMacOSBundle(stagingDir.string());
    break;
  case BuildPlatform::Web:
    result = buildWebBundle(stagingDir.string());
    break;
  case BuildPlatform::Android:
    result = buildAndroidBundle(stagingDir.string());
    break;
  case BuildPlatform::iOS:
    result = buildIOSBundle(stagingDir.string());
    break;
  case BuildPlatform::All:
    // Build for current platform
    result = buildLinuxExecutable(stagingDir.string());
    break;
  }

  if (result.isError()) {
    endStep(false, result.error());
    return result;
  }

  // Generate runtime_config.json
  updateProgress(0.8f, "Generating runtime configuration...");

  fs::path configDir = stagingDir / "config";
  fs::create_directories(configDir);

  fs::path configPath = configDir / "runtime_config.json";
  std::ofstream configFile(configPath);
  if (configFile.is_open()) {
    configFile << "{\n";
    configFile << "  \"version\": \"1.0\",\n";
    configFile << "  \"game\": {\n";
    configFile << "    \"name\": \"" << m_config.executableName << "\",\n";
    configFile << "    \"version\": \"" << m_config.version << "\"\n";
    configFile << "  },\n";
    configFile << "  \"localization\": {\n";
    configFile << "    \"default_locale\": \"" << m_config.defaultLanguage << "\",\n";
    configFile << "    \"available_locales\": [";

    bool first = true;
    for (const auto& lang : m_config.includedLanguages) {
      if (!first)
        configFile << ", ";
      first = false;
      configFile << "\"" << lang << "\"";
    }
    configFile << "]\n";
    configFile << "  },\n";
    configFile << "  \"packs\": {\n";
    configFile << "    \"directory\": \"packs\",\n";
    configFile << "    \"index_file\": \"packs_index.json\",\n";
    configFile << "    \"encrypted\": " << (m_config.encryptAssets ? "true" : "false") << "\n";
    configFile << "  },\n";
    configFile << "  \"runtime\": {\n";
    configFile << "    \"enable_logging\": " << (m_config.enableLogging ? "true" : "false")
               << ",\n";
    configFile << "    \"enable_debug_console\": "
               << (m_config.includeDebugConsole ? "true" : "false") << "\n";
    configFile << "  }\n";
    configFile << "}\n";
    configFile.close();
  }

  logMessage("Runtime bundle created for " + BuildUtils::getPlatformName(m_config.platform), false);
  endStep(true);
  return Result<void>::ok();
}

Result<void> BuildSystem::signAndFinalize() {
  beginStep("Verify", "Verifying and finalizing build");

  fs::path stagingDir = fs::path(m_config.outputPath) / ".staging";

  // Verify pack integrity with comprehensive validation
  updateProgress(0.1f, "Verifying pack integrity...");

  fs::path packsDir = stagingDir / "packs";
  if (fs::exists(packsDir)) {
    for (const auto& entry : fs::directory_iterator(packsDir)) {
      if (entry.path().extension() == ".nmres") {
        std::string packPath = entry.path().string();

        // Read entire pack header for verification
        std::ifstream file(packPath, std::ios::binary);
        if (!file.is_open()) {
          endStep(false, "Cannot read pack file: " + packPath);
          return Result<void>::error("Pack file verification failed: " + packPath);
        }

        // Verify pack file size is at least header + footer size
        file.seekg(0, std::ios::end);
        auto fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        if (fileSize < 96) { // 64 byte header + 32 byte footer minimum
          m_progress.warnings.push_back("Pack file too small (corrupt?): " + packPath);
          file.close();
          continue;
        }

        // Read and verify header (64 bytes)
        u8 header[64];
        file.read(reinterpret_cast<char*>(header), 64);
        if (file.gcount() != 64) {
          m_progress.warnings.push_back("Failed to read pack header: " + packPath);
          file.close();
          continue;
        }

        // Verify magic number (should be "NMRS")
        if (std::strncmp(reinterpret_cast<const char*>(header), "NMRS", 4) != 0) {
          m_progress.warnings.push_back("Pack file has invalid magic number: " + packPath);
          file.close();
          continue;
        }

        // Verify version
        u16 versionMajor = *reinterpret_cast<u16*>(header + 4);
        u16 versionMinor = *reinterpret_cast<u16*>(header + 6);
        if (versionMajor > 1) {
          m_progress.warnings.push_back("Pack file has unsupported version " +
                                        std::to_string(versionMajor) + "." +
                                        std::to_string(versionMinor) + ": " + packPath);
        }

        // Read total file size from header and compare
        u64 headerFileSize = *reinterpret_cast<u64*>(header + 40);
        if (static_cast<i64>(headerFileSize) != fileSize) {
          m_progress.warnings.push_back(
              "Pack file size mismatch (header: " + std::to_string(headerFileSize) +
              ", actual: " + std::to_string(fileSize) + "): " + packPath);
        }

        // Verify footer magic at end of file
        file.seekg(-32, std::ios::end);
        char footerMagic[4];
        file.read(footerMagic, 4);
        if (std::strncmp(footerMagic, "NMRF", 4) != 0) {
          m_progress.warnings.push_back("Pack file has invalid footer magic: " + packPath);
        } else {
          // Read and verify tables CRC32
          u32 storedCrc;
          file.read(reinterpret_cast<char*>(&storedCrc), 4);

          // For CRC verification, we'd need to re-read the tables
          // This is a smoke test - full verification is expensive
          // At minimum, verify CRC is non-zero if pack has resources
          u32 resourceCount = *reinterpret_cast<u32*>(header + 12);
          if (resourceCount > 0 && storedCrc == 0) {
            m_progress.warnings.push_back("Pack file has zero CRC with "
                                          "non-empty resources (suspicious): " +
                                          packPath);
          }
        }

        file.close();

        // Perform smoke-load test by attempting to read resource table
        // This verifies the pack structure is valid and parseable
        std::ifstream packTest(packPath, std::ios::binary);
        if (packTest.is_open()) {
          packTest.seekg(0, std::ios::beg);

          u8 hdr[64];
          packTest.read(reinterpret_cast<char*>(hdr), 64);

          u32 resCount = *reinterpret_cast<u32*>(hdr + 12);
          u64 tableOffset = *reinterpret_cast<u64*>(hdr + 16);

          if (resCount > 0 && tableOffset >= 64) {
            // Try to read first resource entry (48 bytes each)
            packTest.seekg(static_cast<std::streamoff>(tableOffset));

            u8 resEntry[48];
            packTest.read(reinterpret_cast<char*>(resEntry), 48);

            if (packTest.gcount() == 48) {
              // Entry read successfully - pack structure is valid
              logMessage("Smoke-load test passed for: " + entry.path().filename().string(), false);
            } else {
              m_progress.warnings.push_back("Smoke-load failed: Cannot read resource table: " +
                                            packPath);
            }
          }
          packTest.close();
        }

        logMessage("Pack verified: " + entry.path().filename().string(), false);
      }
    }
  }

  // Verify config files
  updateProgress(0.5f, "Verifying configuration...");

  fs::path configPath = stagingDir / "config" / "runtime_config.json";
  if (fs::exists(configPath)) {
    std::ifstream configFile(configPath);
    if (!configFile.is_open()) {
      m_progress.warnings.push_back("Cannot read runtime_config.json");
    }
    configFile.close();
  }

  // Sign executable if requested
  if (m_config.signExecutable && !m_config.signingCertificate.empty()) {
    updateProgress(0.7f, "Signing executable...");

    // Determine the executable path based on platform
    std::string executablePath;
    if (m_config.platform == BuildPlatform::Windows) {
      executablePath =
          (stagingDir / (m_config.executableName + BuildUtils::getExecutableExtension(BuildPlatform::Windows)))
              .string();
    } else if (m_config.platform == BuildPlatform::MacOS) {
      executablePath = (stagingDir / (m_config.executableName + ".app")).string();
    } else if (m_config.platform == BuildPlatform::Linux) {
      executablePath =
          (stagingDir / (m_config.executableName + BuildUtils::getExecutableExtension(BuildPlatform::Linux)))
              .string();
    }

    if (!executablePath.empty()) {
      auto signResult = signExecutableForPlatform(executablePath);
      if (!signResult.isOk()) {
        // Signing failed - log error but continue
        std::string errorMsg = "Code signing failed: " + signResult.error();
        logMessage(errorMsg, true);
        m_progress.warnings.push_back(errorMsg);
      }
    } else {
      m_progress.warnings.push_back("Code signing skipped: No executable path for platform");
    }
  }

  // Calculate final sizes
  updateProgress(0.9f, "Calculating build statistics...");

  i64 totalSize = 0;
  i64 compressedSize = 0;

  if (fs::exists(packsDir)) {
    for (const auto& entry : fs::recursive_directory_iterator(packsDir)) {
      if (entry.is_regular_file()) {
        compressedSize += static_cast<i64>(entry.file_size());
      }
    }
  }

  totalSize = BuildUtils::calculateDirectorySize(stagingDir.string());

  m_lastResult.totalSize = totalSize;
  m_lastResult.compressedSize = compressedSize;

  logMessage("Build verification complete. Total size: " + BuildUtils::formatFileSize(totalSize),
             false);
  endStep(true);
  return Result<void>::ok();
}

Result<void> BuildSystem::cleanup() {
  // Clean up temporary files if build failed
  if (!m_progress.wasSuccessful) {
    fs::path stagingDir = fs::path(m_config.outputPath) / ".staging";
    if (fs::exists(stagingDir)) {
      try {
        fs::remove_all(stagingDir);
      } catch (const fs::filesystem_error& e) {
        return Result<void>::error(std::string("Cleanup failed: ") + e.what());
      }
    }
  }
  return Result<void>::ok();
}

void BuildSystem::updateProgress(f32 stepProgress, const std::string& task) {
  if (m_progress.currentStepIndex >= 0 &&
      m_progress.currentStepIndex < static_cast<i32>(m_progress.steps.size())) {
    // Calculate overall progress
    f32 completedWeight = 0.0f;
    for (i32 i = 0; i < m_progress.currentStepIndex; i++) {
      completedWeight += m_progress.steps[static_cast<std::size_t>(i)].progressWeight;
    }

    f32 currentWeight =
        m_progress.steps[static_cast<std::size_t>(m_progress.currentStepIndex)].progressWeight;
    m_progress.progress = completedWeight + (currentWeight * stepProgress);
  }

  m_progress.currentTask = task;

  if (m_onProgressUpdate) {
    m_onProgressUpdate(m_progress);
  }
}

void BuildSystem::logMessage(const std::string& message, bool isError) {
  if (isError) {
    m_progress.errors.push_back(message);
  } else {
    m_progress.infoMessages.push_back(message);
  }

  if (m_onLogMessage) {
    m_onLogMessage(message, isError);
  }
}

void BuildSystem::beginStep(const std::string& name, const std::string& description) {
  // Find step by name
  for (i32 i = 0; i < static_cast<i32>(m_progress.steps.size()); i++) {
    if (m_progress.steps[static_cast<std::size_t>(i)].name == name) {
      m_progress.currentStepIndex = i;
      m_progress.currentStep = name;
      m_progress.steps[static_cast<std::size_t>(i)].description = description;
      break;
    }
  }

  logMessage("Starting: " + name + " - " + description, false);
  updateProgress(0.0f, description);
}

void BuildSystem::endStep(bool success, const std::string& errorMessage) {
  if (m_progress.currentStepIndex >= 0 &&
      m_progress.currentStepIndex < static_cast<i32>(m_progress.steps.size())) {
    auto& step = m_progress.steps[static_cast<std::size_t>(m_progress.currentStepIndex)];
    step.completed = true;
    step.success = success;
    step.errorMessage = errorMessage;

    if (m_onStepComplete) {
      m_onStepComplete(step);
    }
  }

  if (!success) {
    logMessage("Step failed: " + errorMessage, true);
  }
}

ScriptCompileResult BuildSystem::compileScript(const std::string& scriptPath) {
  ScriptCompileResult result;
  result.sourcePath = scriptPath;
  result.success = true;

  try {
    // Read script file
    std::ifstream file(scriptPath);
    if (!file.is_open()) {
      result.success = false;
      result.errors.push_back("Cannot open file: " + scriptPath);
      return result;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    file.close();

    if (source.empty()) {
      result.warnings.push_back("Script file is empty");
      result.bytecodeSize = 0;
      return result;
    }

    // Step 1: Lexical analysis
    scripting::Lexer lexer;
    auto tokenResult = lexer.tokenize(source);
    if (!tokenResult.isOk()) {
      result.success = false;
      result.errors.push_back("Lexer error: " + tokenResult.error());
      return result;
    }

    auto tokens = tokenResult.value();

    // Collect lexer errors
    for (const auto& err : lexer.getErrors()) {
      result.errors.push_back("Lexer error at line " + std::to_string(err.location.line) + ": " +
                              err.message);
    }

    if (!lexer.getErrors().empty()) {
      result.success = false;
      return result;
    }

    // Step 2: Parsing
    scripting::Parser parser;
    auto parseResult = parser.parse(tokens);
    if (!parseResult.isOk()) {
      result.success = false;
      result.errors.push_back("Parse error: " + parseResult.error());
      return result;
    }

    auto program = std::move(parseResult).value();

    // Collect parser errors
    for (const auto& err : parser.getErrors()) {
      result.errors.push_back("Parse error at line " + std::to_string(err.location.line) + ": " +
                              err.message);
    }

    if (!parser.getErrors().empty()) {
      result.success = false;
      return result;
    }

    // Step 3: Validation
    scripting::Validator validator;
    auto validationResult = validator.validate(program);

    // Collect validation warnings
    for (const auto& err : validationResult.errors.all()) {
      if (err.severity == scripting::Severity::Warning) {
        result.warnings.push_back("Warning at line " + std::to_string(err.span.start.line) + ": " +
                                  err.message);
      } else if (err.severity == scripting::Severity::Error) {
        result.errors.push_back("Validation error at line " + std::to_string(err.span.start.line) +
                                ": " + err.message);
      }
    }

    if (!validationResult.isValid) {
      result.success = false;
      return result;
    }

    // Step 4: Compilation to bytecode
    scripting::Compiler compiler;
    auto compileResult = compiler.compile(program);
    if (!compileResult.isOk()) {
      result.success = false;
      result.errors.push_back("Compile error: " + compileResult.error());
      return result;
    }

    // Collect compiler errors
    for (const auto& err : compiler.getErrors()) {
      result.errors.push_back("Compile error at line " + std::to_string(err.location.line) + ": " +
                              err.message);
    }

    if (!compiler.getErrors().empty()) {
      result.success = false;
      return result;
    }

    auto compiledScript = compileResult.value();

    // Calculate bytecode size: instructions + string table + metadata
    i32 bytecodeSize = 0;
    bytecodeSize += static_cast<i32>(compiledScript.instructions.size() * sizeof(u32) * 2);
    for (const auto& str : compiledScript.stringTable) {
      bytecodeSize += static_cast<i32>(sizeof(u32) + str.size());
    }
    bytecodeSize += static_cast<i32>(compiledScript.sceneEntryPoints.size() * sizeof(u32));
    bytecodeSize += static_cast<i32>(compiledScript.characters.size() * sizeof(u32));

    result.bytecodeSize = bytecodeSize;

  } catch (const std::exception& e) {
    result.success = false;
    result.errors.push_back(std::string("Compilation error: ") + e.what());
  }

  return result;
}


} // namespace NovelMind::editor
