/**
 * @file build_pack.cpp
 * @brief PackBuilder implementation for NovelMind
 *
 * Implements pack building functionality:
 * - File and data packing
 * - Compression (zlib)
 * - Encryption (AES-256-GCM)
 * - Pack statistics
 */

#include "NovelMind/editor/build_system.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

#ifdef NOVELMIND_HAS_ZLIB
#include <zlib.h>
#endif

#ifdef NOVELMIND_HAS_OPENSSL
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#endif

namespace fs = std::filesystem;

namespace NovelMind::editor {

// ============================================================================
// PackBuilder Implementation
// ============================================================================

PackBuilder::PackBuilder() = default;
PackBuilder::~PackBuilder() = default;

Result<void> PackBuilder::beginPack(const std::string& outputPath) {
  m_outputPath = outputPath;
  m_entries.clear();
  return Result<void>::ok();
}

Result<void> PackBuilder::addFile(const std::string& sourcePath, const std::string& packPath) {
  try {
    std::ifstream file(sourcePath, std::ios::binary);
    if (!file.is_open()) {
      return Result<void>::error("Cannot open file: " + sourcePath);
    }

    std::vector<u8> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    PackEntry entry;
    entry.path = packPath;
    entry.data = std::move(data);
    entry.originalSize = static_cast<i64>(entry.data.size());

    m_entries.push_back(std::move(entry));
    return Result<void>::ok();

  } catch (const std::exception& e) {
    return Result<void>::error(std::string("Failed to add file: ") + e.what());
  }
}

Result<void> PackBuilder::addData(const std::string& packPath, const std::vector<u8>& data) {
  PackEntry entry;
  entry.path = packPath;
  entry.data = data;
  entry.originalSize = static_cast<i64>(data.size());

  m_entries.push_back(std::move(entry));
  return Result<void>::ok();
}

Result<void> PackBuilder::finalizePack() {
  if (m_outputPath.empty()) {
    return Result<void>::error("Pack not initialized - call beginPack first");
  }

  try {
    std::ofstream output(m_outputPath, std::ios::binary);
    if (!output.is_open()) {
      return Result<void>::error("Cannot create pack file: " + m_outputPath);
    }

    // Write pack header (simplified)
    const char magic[] = "NMRS";
    output.write(magic, 4);

    u32 entryCount = static_cast<u32>(m_entries.size());
    output.write(reinterpret_cast<const char*>(&entryCount), sizeof(entryCount));

    // Write entries
    for (const auto& entry : m_entries) {
      // Write path length and path
      u32 pathLen = static_cast<u32>(entry.path.size());
      output.write(reinterpret_cast<const char*>(&pathLen), sizeof(pathLen));
      output.write(entry.path.c_str(), pathLen);

      // Write data
      std::vector<u8> processedData = entry.data;

      // Apply compression if enabled
      if (m_compressionLevel != CompressionLevel::None) {
        auto compressResult = compressData(entry.data);
        if (compressResult.isOk()) {
          processedData = compressResult.value();
        }
      }

      // Apply encryption if key is set
      if (!m_encryptionKey.empty()) {
        auto encryptResult = encryptData(processedData);
        if (encryptResult.isOk()) {
          processedData = encryptResult.value();
        }
      }

      u64 dataSize = processedData.size();
      output.write(reinterpret_cast<const char*>(&dataSize), sizeof(dataSize));
      output.write(reinterpret_cast<const char*>(processedData.data()),
                   static_cast<std::streamsize>(dataSize));
    }

    output.close();
    return Result<void>::ok();

  } catch (const std::exception& e) {
    return Result<void>::error(std::string("Pack finalization failed: ") + e.what());
  }
}

void PackBuilder::setEncryptionKey(const Core::SecureVector<u8>& key) {
  m_encryptionKey = key;
}

void PackBuilder::setCompressionLevel(CompressionLevel level) {
  m_compressionLevel = level;
}

PackBuilder::PackStats PackBuilder::getStats() const {
  PackStats stats;
  stats.fileCount = static_cast<i32>(m_entries.size());
  stats.uncompressedSize = 0;
  stats.compressedSize = 0;

  for (const auto& entry : m_entries) {
    stats.uncompressedSize += entry.originalSize;
    stats.compressedSize += static_cast<i64>(entry.data.size());
  }

  if (stats.uncompressedSize > 0) {
    stats.compressionRatio =
        static_cast<f32>(stats.compressedSize) / static_cast<f32>(stats.uncompressedSize);
  } else {
    stats.compressionRatio = 1.0f;
  }

  return stats;
}

Result<std::vector<u8>> PackBuilder::compressData(const std::vector<u8>& data) {
#ifdef NOVELMIND_HAS_ZLIB
  if (data.empty()) {
    return Result<std::vector<u8>>::ok(std::vector<u8>());
  }

  // Calculate upper bound for compressed size
  uLongf compressedSize = compressBound(static_cast<uLong>(data.size()));
  std::vector<u8> compressed(compressedSize);

  // Determine compression level based on settings
  int zlibLevel = Z_DEFAULT_COMPRESSION;
  switch (m_compressionLevel) {
  case CompressionLevel::None:
    zlibLevel = Z_NO_COMPRESSION;
    break;
  case CompressionLevel::Fast:
    zlibLevel = Z_BEST_SPEED;
    break;
  case CompressionLevel::Balanced:
    zlibLevel = Z_DEFAULT_COMPRESSION;
    break;
  case CompressionLevel::Maximum:
    zlibLevel = Z_BEST_COMPRESSION;
    break;
  }

  // Compress the data
  int result = compress2(compressed.data(), &compressedSize, data.data(),
                         static_cast<uLong>(data.size()), zlibLevel);

  if (result != Z_OK) {
    return Result<std::vector<u8>>::error("zlib compression failed with code " +
                                          std::to_string(result));
  }

  // Resize to actual compressed size
  compressed.resize(compressedSize);
  return Result<std::vector<u8>>::ok(std::move(compressed));
#else
  // If zlib is not available, return uncompressed data
  return Result<std::vector<u8>>::ok(data);
#endif
}

Result<std::vector<u8>> PackBuilder::encryptData(const std::vector<u8>& data) {
#ifdef NOVELMIND_HAS_OPENSSL
  if (m_encryptionKey.empty()) {
    // No encryption key set, return data as-is
    return Result<std::vector<u8>>::ok(data);
  }

  if (data.empty()) {
    return Result<std::vector<u8>>::ok(std::vector<u8>());
  }

  // AES-256-GCM requires a 32-byte key and 12-byte IV
  constexpr int kKeySize = 32;
  constexpr int kIVSize = 12;
  constexpr int kTagSize = 16;

  // Prepare 32-byte key (pad or truncate if necessary)
  // Use SecureVector to ensure proper zeroing
  Core::SecureVector<u8> key256(kKeySize, 0);
  std::memcpy(key256.data(), m_encryptionKey.data(),
              std::min(m_encryptionKey.size(), static_cast<usize>(kKeySize)));

  // Generate random IV
  std::vector<u8> iv(kIVSize);
  if (RAND_bytes(iv.data(), kIVSize) != 1) {
    return Result<std::vector<u8>>::error("Failed to generate random IV");
  }

  // Create cipher context
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    return Result<std::vector<u8>>::error("Failed to create cipher context");
  }

  // Initialize AES-256-GCM encryption
  if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<std::vector<u8>>::error("Failed to initialize AES-256-GCM");
  }

  // Set IV length
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kIVSize, nullptr) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<std::vector<u8>>::error("Failed to set IV length");
  }

  // Set key and IV
  if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key256.data(), iv.data()) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<std::vector<u8>>::error("Failed to set key and IV");
  }

  // Allocate output buffer (ciphertext + tag)
  std::vector<u8> result(data.size() + kTagSize);
  int outLen = 0;

  // Encrypt the data
  if (EVP_EncryptUpdate(ctx, result.data(), &outLen, data.data(), static_cast<int>(data.size())) !=
      1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<std::vector<u8>>::error("AES-256-GCM encryption failed");
  }

  int cipherLen = outLen;

  // Finalize encryption
  if (EVP_EncryptFinal_ex(ctx, result.data() + outLen, &outLen) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<std::vector<u8>>::error("Failed to finalize AES-256-GCM encryption");
  }

  cipherLen += outLen;

  // Get the authentication tag
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTagSize, result.data() + cipherLen) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<std::vector<u8>>::error("Failed to get AES-256-GCM authentication tag");
  }

  EVP_CIPHER_CTX_free(ctx);

  // Resize to actual size (ciphertext + tag)
  result.resize(static_cast<usize>(cipherLen + kTagSize));

  // Prepend IV to the result (IV + ciphertext + tag)
  std::vector<u8> output;
  output.reserve(iv.size() + result.size());
  output.insert(output.end(), iv.begin(), iv.end());
  output.insert(output.end(), result.begin(), result.end());

  return Result<std::vector<u8>>::ok(std::move(output));
#else
  // If OpenSSL is not available, return unencrypted data
  (void)data;
  return Result<std::vector<u8>>::ok(data);
#endif
}

} // namespace NovelMind::editor
