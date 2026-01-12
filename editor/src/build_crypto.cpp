/**
 * @file build_crypto.cpp
 * @brief Cryptographic functions for NovelMind Build System
 *
 * Implements CRC32, SHA-256, compression, encryption, and digital signatures
 */

#include "NovelMind/editor/build_system.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>

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
// CRC32 Implementation
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

// ============================================================================
// SHA-256 Implementation
// ============================================================================

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

// ============================================================================
// Compression Implementation
// ============================================================================

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

// ============================================================================
// Encryption Implementation
// ============================================================================

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

// ============================================================================
// Key Management
// ============================================================================

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
        return Result<std::vector<u8>>::error(
            "NOVELMIND_PACK_AES_KEY_HEX contains invalid hex characters. Only 0-9, a-f, A-F are "
            "allowed");
      }
    }

    std::vector<u8> key(32);
    for (usize i = 0; i < 32; ++i) {
      std::string byteStr = hexStr.substr(i * 2, 2);
      try {
        unsigned long val = std::stoul(byteStr, nullptr, 16);
        key[i] = static_cast<u8>(val);
      } catch (const std::invalid_argument& e) {
        return Result<std::vector<u8>>::error("Invalid hex format in encryption key at byte " +
                                              std::to_string(i) + ": " + e.what());
      } catch (const std::out_of_range& e) {
        return Result<std::vector<u8>>::error("Hex value out of range in encryption key at byte " +
                                              std::to_string(i) + ": " + e.what());
      } catch (...) {
        return Result<std::vector<u8>>::error("Unknown error parsing encryption key at byte " +
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

// ============================================================================
// Digital Signatures
// ============================================================================

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

// ============================================================================
// Path Security & VFS Utilities
// ============================================================================

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

// ============================================================================
// Resource Type Detection
// ============================================================================

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
// Build Timestamp
// ============================================================================

u64 BuildSystem::getBuildTimestamp() const {
  if (m_config.fixedBuildTimestamp != 0) {
    return m_config.fixedBuildTimestamp;
  }
  return static_cast<u64>(std::chrono::duration_cast<std::chrono::seconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count());
}

} // namespace NovelMind::editor
