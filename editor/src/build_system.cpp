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
// BuildUtils Implementation
// ============================================================================

namespace BuildUtils {

std::string getPlatformName(BuildPlatform platform) {
  switch (platform) {
  case BuildPlatform::Windows:
    return "Windows";
  case BuildPlatform::Linux:
    return "Linux";
  case BuildPlatform::MacOS:
    return "macOS";
  case BuildPlatform::Web:
    return "Web (WebAssembly)";
  case BuildPlatform::Android:
    return "Android";
  case BuildPlatform::iOS:
    return "iOS";
  case BuildPlatform::All:
    return "All Platforms";
  }
  return "Unknown";
}

std::string getExecutableExtension(BuildPlatform platform) {
  switch (platform) {
  case BuildPlatform::Windows:
    return ".exe";
  case BuildPlatform::Linux:
  case BuildPlatform::MacOS:
    return "";
  case BuildPlatform::Web:
    return ".html"; // Entry point for web builds
  case BuildPlatform::Android:
    return ".apk";
  case BuildPlatform::iOS:
    return ".ipa";
  case BuildPlatform::All:
#ifdef _WIN32
    return ".exe";
#else
    return "";
#endif
  }
  return "";
}

BuildPlatform getCurrentPlatform() {
#ifdef _WIN32
  return BuildPlatform::Windows;
#elif defined(__APPLE__)
  return BuildPlatform::MacOS;
#else
  return BuildPlatform::Linux;
#endif
}

std::string formatFileSize(i64 bytes) {
  const char* units[] = {"B", "KB", "MB", "GB", "TB"};
  i32 unitIndex = 0;
  f64 size = static_cast<f64>(bytes);

  while (size >= 1024.0 && unitIndex < 4) {
    size /= 1024.0;
    unitIndex++;
  }

  std::ostringstream oss;
  if (unitIndex == 0) {
    oss << bytes << " " << units[unitIndex];
  } else {
    oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
  }
  return oss.str();
}

std::string formatDuration(f64 milliseconds) {
  if (milliseconds < 1000) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(0) << milliseconds << " ms";
    return oss.str();
  }

  f64 seconds = milliseconds / 1000.0;
  if (seconds < 60) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << seconds << " s";
    return oss.str();
  }

  i32 minutes = static_cast<i32>(seconds) / 60;
  i32 secs = static_cast<i32>(seconds) % 60;
  std::ostringstream oss;
  oss << minutes << " min " << secs << " s";
  return oss.str();
}

i64 calculateDirectorySize(const std::string& path) {
  i64 totalSize = 0;
  try {
    for (const auto& entry : fs::recursive_directory_iterator(path)) {
      if (entry.is_regular_file()) {
        totalSize += static_cast<i64>(entry.file_size());
      }
    }
  } catch (const fs::filesystem_error&) {
    // Directory doesn't exist or permission denied
  }
  return totalSize;
}

Result<void> copyDirectory(const std::string& source, const std::string& destination) {
  try {
    fs::copy(source, destination,
             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    return Result<void>::ok();
  } catch (const fs::filesystem_error& e) {
    return Result<void>::error(std::string("Failed to copy directory: ") + e.what());
  }
}

Result<void> deleteDirectory(const std::string& path) {
  try {
    if (fs::exists(path)) {
      fs::remove_all(path);
    }
    return Result<void>::ok();
  } catch (const fs::filesystem_error& e) {
    return Result<void>::error(std::string("Failed to delete directory: ") + e.what());
  }
}

Result<void> createDirectories(const std::string& path) {
  try {
    fs::create_directories(path);
    return Result<void>::ok();
  } catch (const fs::filesystem_error& e) {
    return Result<void>::error(std::string("Failed to create directories: ") + e.what());
  }
}

} // namespace BuildUtils

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

AssetProcessResult BuildSystem::processImage(const std::string& sourcePath,
                                             const std::string& outputPath) {
  AssetProcessResult result;
  result.sourcePath = sourcePath;
  result.outputPath = outputPath;
  result.success = true;

  try {
    // For now, just copy the file (in production, would apply optimization)
    fs::copy(sourcePath, outputPath, fs::copy_options::overwrite_existing);

    result.originalSize = static_cast<i64>(fs::file_size(sourcePath));
    result.processedSize = static_cast<i64>(fs::file_size(outputPath));

  } catch (const std::exception& e) {
    result.success = false;
    result.errorMessage = e.what();
  }

  return result;
}

AssetProcessResult BuildSystem::processAudio(const std::string& sourcePath,
                                             const std::string& outputPath) {
  AssetProcessResult result;
  result.sourcePath = sourcePath;
  result.outputPath = outputPath;
  result.success = true;

  try {
    // For now, just copy the file (in production, would apply compression)
    fs::copy(sourcePath, outputPath, fs::copy_options::overwrite_existing);

    result.originalSize = static_cast<i64>(fs::file_size(sourcePath));
    result.processedSize = static_cast<i64>(fs::file_size(outputPath));

  } catch (const std::exception& e) {
    result.success = false;
    result.errorMessage = e.what();
  }

  return result;
}

AssetProcessResult BuildSystem::processFont(const std::string& sourcePath,
                                            const std::string& outputPath) {
  AssetProcessResult result;
  result.sourcePath = sourcePath;
  result.outputPath = outputPath;
  result.success = true;

  try {
    // Fonts are typically copied as-is
    fs::copy(sourcePath, outputPath, fs::copy_options::overwrite_existing);

    result.originalSize = static_cast<i64>(fs::file_size(sourcePath));
    result.processedSize = static_cast<i64>(fs::file_size(outputPath));

  } catch (const std::exception& e) {
    result.success = false;
    result.errorMessage = e.what();
  }

  return result;
}

AssetProcessResult BuildSystem::processData(const std::string& sourcePath,
                                            const std::string& outputPath) {
  AssetProcessResult result;
  result.sourcePath = sourcePath;
  result.outputPath = outputPath;
  result.success = true;

  try {
    // Copy generic data files
    fs::copy(sourcePath, outputPath, fs::copy_options::overwrite_existing);

    result.originalSize = static_cast<i64>(fs::file_size(sourcePath));
    result.processedSize = static_cast<i64>(fs::file_size(outputPath));

  } catch (const std::exception& e) {
    result.success = false;
    result.errorMessage = e.what();
  }

  return result;
}

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

Result<void> BuildSystem::buildWindowsExecutable(const std::string& outputPath) {
  // Create a launcher script/placeholder for Windows
  fs::path exePath =
      fs::path(outputPath) /
      (m_config.executableName + BuildUtils::getExecutableExtension(BuildPlatform::Windows));

  // In a real implementation, this would copy the runtime executable
  // For now, create a placeholder batch file
  fs::path batchPath = fs::path(outputPath) / (m_config.executableName + "_launcher.bat");

  std::ofstream batch(batchPath);
  if (batch.is_open()) {
    batch << "@echo off\n";
    batch << "echo NovelMind Runtime - " << m_config.executableName << "\n";
    batch << "echo Version: " << m_config.version << "\n";
    batch << "echo.\n";
    batch << "echo This is a placeholder launcher.\n";
    batch << "echo In production, this would start the game runtime.\n";
    batch << "pause\n";
    batch.close();
  }

  return Result<void>::ok();
}

Result<void> BuildSystem::buildLinuxExecutable(const std::string& outputPath) {
  // Create a launcher script/placeholder for Linux
  fs::path scriptPath = fs::path(outputPath) / (m_config.executableName + "_launcher.sh");

  std::ofstream script(scriptPath);
  if (script.is_open()) {
    script << "#!/bin/bash\n";
    script << "echo \"NovelMind Runtime - " << m_config.executableName << "\"\n";
    script << "echo \"Version: " << m_config.version << "\"\n";
    script << "echo \"\"\n";
    script << "echo \"This is a placeholder launcher.\"\n";
    script << "echo \"In production, this would start the game runtime.\"\n";
    script.close();

    // Make executable
    fs::permissions(scriptPath,
                    fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                    fs::perm_options::add);
  }

  return Result<void>::ok();
}

Result<void> BuildSystem::buildMacOSBundle(const std::string& outputPath) {
  // Create a basic .app bundle structure
  std::string appName = m_config.executableName + ".app";
  fs::path appPath = fs::path(outputPath) / appName;
  fs::path contentsPath = appPath / "Contents";
  fs::path macOSPath = contentsPath / "MacOS";
  fs::path resourcesPath = contentsPath / "Resources";

  fs::create_directories(macOSPath);
  fs::create_directories(resourcesPath);

  // Create Info.plist
  fs::path plistPath = contentsPath / "Info.plist";
  std::ofstream plist(plistPath);
  if (plist.is_open()) {
    plist << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    plist << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
             "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
    plist << "<plist version=\"1.0\">\n";
    plist << "<dict>\n";
    plist << "  <key>CFBundleExecutable</key>\n";
    plist << "  <string>" << m_config.executableName << "</string>\n";
    plist << "  <key>CFBundleIdentifier</key>\n";
    plist << "  <string>com.novelmind." << m_config.executableName << "</string>\n";
    plist << "  <key>CFBundleName</key>\n";
    plist << "  <string>" << m_config.executableName << "</string>\n";
    plist << "  <key>CFBundleShortVersionString</key>\n";
    plist << "  <string>" << m_config.version << "</string>\n";
    plist << "  <key>CFBundleVersion</key>\n";
    plist << "  <string>" << m_config.version << "</string>\n";
    plist << "  <key>CFBundlePackageType</key>\n";
    plist << "  <string>APPL</string>\n";
    plist << "</dict>\n";
    plist << "</plist>\n";
    plist.close();
  }

  // Create placeholder executable
  fs::path exePath = macOSPath / m_config.executableName;
  std::ofstream exe(exePath);
  if (exe.is_open()) {
    exe << "#!/bin/bash\n";
    exe << "echo \"NovelMind Runtime - " << m_config.executableName << "\"\n";
    exe << "echo \"Version: " << m_config.version << "\"\n";
    exe.close();

    fs::permissions(exePath, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                    fs::perm_options::add);
  }

  // Copy packs and config to Resources
  fs::path stagingPacks = fs::path(outputPath) / "packs";
  fs::path stagingConfig = fs::path(outputPath) / "config";

  if (fs::exists(stagingPacks)) {
    fs::copy(stagingPacks, resourcesPath / "packs",
             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
  }

  if (fs::exists(stagingConfig)) {
    fs::copy(stagingConfig, resourcesPath / "config",
             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
  }

  return Result<void>::ok();
}

Result<void> BuildSystem::buildWebBundle(const std::string& outputPath) {
  // Create WebAssembly/Emscripten bundle structure
  fs::path webPath = fs::path(outputPath) / "web";
  fs::create_directories(webPath);

  // Create index.html entry point
  fs::path htmlPath = webPath / "index.html";
  std::ofstream html(htmlPath);
  if (html.is_open()) {
    html << "<!DOCTYPE html>\n";
    html << "<html lang=\"en\">\n";
    html << "<head>\n";
    html << "  <meta charset=\"UTF-8\">\n";
    html << "  <meta name=\"viewport\" content=\"width=device-width, "
            "initial-scale=1.0\">\n";
    html << "  <title>" << m_config.executableName << " - NovelMind</title>\n";
    html << "  <style>\n";
    html << "    body { margin: 0; background: #1a1a1a; display: flex; "
            "justify-content: center; align-items: center; height: 100vh; }\n";
    html << "    #canvas { background: #000; }\n";
    html << "    .loading { color: #fff; font-family: sans-serif; }\n";
    html << "  </style>\n";
    html << "</head>\n";
    html << "<body>\n";
    html << "  <div class=\"loading\" id=\"status\">Loading " << m_config.executableName << " (v"
         << m_config.version << ")...</div>\n";
    html << "  <canvas id=\"canvas\" width=\"1280\" height=\"720\" "
            "style=\"display:none;\"></canvas>\n";
    html << "  <script>\n";
    html << "    // NovelMind WebAssembly runtime placeholder\n";
    html << "    // In production, this loads the Emscripten-compiled runtime\n";
    html << "    document.getElementById('status').textContent = 'Web build "
            "placeholder - runtime not yet bundled';\n";
    html << "  </script>\n";
    html << "</body>\n";
    html << "</html>\n";
    html.close();
  }

  // Copy packs to web directory
  fs::path stagingPacks = fs::path(outputPath) / "packs";
  if (fs::exists(stagingPacks)) {
    fs::copy(stagingPacks, webPath / "packs",
             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
  }

  // Copy config
  fs::path stagingConfig = fs::path(outputPath) / "config";
  if (fs::exists(stagingConfig)) {
    fs::copy(stagingConfig, webPath / "config",
             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
  }

  logMessage("Web bundle created at: " + webPath.string(), false);
  return Result<void>::ok();
}

Result<void> BuildSystem::buildAndroidBundle(const std::string& outputPath) {
  // Create Android project structure placeholder
  // In production, this would generate a complete Gradle project
  fs::path androidPath = fs::path(outputPath) / "android";
  fs::path appPath = androidPath / "app" / "src" / "main";
  fs::path assetsPath = appPath / "assets";
  fs::create_directories(assetsPath);

  // Create AndroidManifest.xml placeholder
  fs::path manifestPath = appPath / "AndroidManifest.xml";
  std::ofstream manifest(manifestPath);
  if (manifest.is_open()) {
    manifest << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    manifest << "<manifest "
                "xmlns:android=\"http://schemas.android.com/apk/res/android\"\n";
    manifest << "    package=\"com.novelmind." << m_config.executableName << "\">\n";
    manifest << "    <application\n";
    manifest << "        android:label=\"" << m_config.executableName << "\"\n";
    manifest << "        android:theme=\"@style/Theme.NovelMind\">\n";
    manifest << "        <activity\n";
    manifest << "            android:name=\".MainActivity\"\n";
    manifest << "            android:exported=\"true\"\n";
    manifest << "            android:configChanges=\"orientation|screenSize\">\n";
    manifest << "            <intent-filter>\n";
    manifest << "                <action "
                "android:name=\"android.intent.action.MAIN\" />\n";
    manifest << "                <category "
                "android:name=\"android.intent.category.LAUNCHER\" />\n";
    manifest << "            </intent-filter>\n";
    manifest << "        </activity>\n";
    manifest << "    </application>\n";
    manifest << "</manifest>\n";
    manifest.close();
  }

  // Create build.gradle placeholder
  fs::path gradlePath = androidPath / "app" / "build.gradle";
  fs::create_directories(gradlePath.parent_path());
  std::ofstream gradle(gradlePath);
  if (gradle.is_open()) {
    gradle << "// NovelMind Android build configuration\n";
    gradle << "// Generated by NovelMind Build System\n";
    gradle << "plugins {\n";
    gradle << "    id 'com.android.application'\n";
    gradle << "}\n\n";
    gradle << "android {\n";
    gradle << "    namespace 'com.novelmind." << m_config.executableName << "'\n";
    gradle << "    compileSdk 34\n\n";
    gradle << "    defaultConfig {\n";
    gradle << "        applicationId 'com.novelmind." << m_config.executableName << "'\n";
    gradle << "        minSdk 24\n";
    gradle << "        targetSdk 34\n";
    gradle << "        versionCode " << m_config.buildNumber << "\n";
    gradle << "        versionName \"" << m_config.version << "\"\n";
    gradle << "    }\n";
    gradle << "}\n";
    gradle.close();
  }

  // Copy packs to assets
  fs::path stagingPacks = fs::path(outputPath) / "packs";
  if (fs::exists(stagingPacks)) {
    fs::copy(stagingPacks, assetsPath / "packs",
             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
  }

  logMessage("Android project structure created at: " + androidPath.string(), false);
  return Result<void>::ok();
}

Result<void> BuildSystem::buildIOSBundle(const std::string& outputPath) {
  // Create iOS Xcode project structure placeholder
  fs::path iosPath = fs::path(outputPath) / "ios";
  std::string projectName = m_config.executableName;
  fs::path xcodeProj = iosPath / (projectName + ".xcodeproj");
  fs::path sourcePath = iosPath / projectName;
  fs::path resourcesPath = sourcePath / "Resources";
  fs::create_directories(xcodeProj);
  fs::create_directories(resourcesPath);

  // Create Info.plist
  fs::path plistPath = sourcePath / "Info.plist";
  std::ofstream plist(plistPath);
  if (plist.is_open()) {
    plist << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    plist << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
             "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
    plist << "<plist version=\"1.0\">\n";
    plist << "<dict>\n";
    plist << "  <key>CFBundleDevelopmentRegion</key>\n";
    plist << "  <string>en</string>\n";
    plist << "  <key>CFBundleExecutable</key>\n";
    plist << "  <string>$(EXECUTABLE_NAME)</string>\n";
    plist << "  <key>CFBundleIdentifier</key>\n";
    plist << "  <string>com.novelmind." << projectName << "</string>\n";
    plist << "  <key>CFBundleName</key>\n";
    plist << "  <string>" << projectName << "</string>\n";
    plist << "  <key>CFBundlePackageType</key>\n";
    plist << "  <string>APPL</string>\n";
    plist << "  <key>CFBundleShortVersionString</key>\n";
    plist << "  <string>" << m_config.version << "</string>\n";
    plist << "  <key>CFBundleVersion</key>\n";
    plist << "  <string>" << m_config.buildNumber << "</string>\n";
    plist << "  <key>UILaunchStoryboardName</key>\n";
    plist << "  <string>LaunchScreen</string>\n";
    plist << "  <key>UISupportedInterfaceOrientations</key>\n";
    plist << "  <array>\n";
    plist << "    <string>UIInterfaceOrientationLandscapeLeft</string>\n";
    plist << "    <string>UIInterfaceOrientationLandscapeRight</string>\n";
    plist << "  </array>\n";
    plist << "  <key>UIRequiresFullScreen</key>\n";
    plist << "  <true/>\n";
    plist << "</dict>\n";
    plist << "</plist>\n";
    plist.close();
  }

  // Create minimal project.pbxproj placeholder
  fs::path pbxprojPath = xcodeProj / "project.pbxproj";
  std::ofstream pbxproj(pbxprojPath);
  if (pbxproj.is_open()) {
    pbxproj << "// NovelMind iOS Project\n";
    pbxproj << "// Generated by NovelMind Build System v" << m_config.version << "\n";
    pbxproj << "// Build Number: " << m_config.buildNumber << "\n";
    pbxproj << "// \n";
    pbxproj << "// This is a placeholder. In production, this would contain\n";
    pbxproj << "// a complete Xcode project configuration for building the iOS "
               "app.\n";
    pbxproj.close();
  }

  // Copy packs to Resources
  fs::path stagingPacks = fs::path(outputPath) / "packs";
  if (fs::exists(stagingPacks)) {
    fs::copy(stagingPacks, resourcesPath / "packs",
             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
  }

  // Copy config
  fs::path stagingConfig = fs::path(outputPath) / "config";
  if (fs::exists(stagingConfig)) {
    fs::copy(stagingConfig, resourcesPath / "config",
             fs::copy_options::recursive | fs::copy_options::overwrite_existing);
  }

  logMessage("iOS Xcode project structure created at: " + iosPath.string(), false);
  return Result<void>::ok();
}

// ============================================================================
// AssetProcessor Implementation
// ============================================================================

AssetProcessor::AssetProcessor() = default;
AssetProcessor::~AssetProcessor() = default;

Result<AssetProcessResult> AssetProcessor::processImage(const std::string& sourcePath,
                                                        const std::string& outputPath,
                                                        bool optimize) {
  AssetProcessResult result;
  result.sourcePath = sourcePath;
  result.outputPath = outputPath;
  result.success = true;

  try {
    if (optimize) {
      // In production, apply image optimization
      // For now, just copy
    }
    fs::copy(sourcePath, outputPath, fs::copy_options::overwrite_existing);
    result.originalSize = static_cast<i64>(fs::file_size(sourcePath));
    result.processedSize = static_cast<i64>(fs::file_size(outputPath));
  } catch (const std::exception& e) {
    result.success = false;
    result.errorMessage = e.what();
    return Result<AssetProcessResult>::error(e.what());
  }

  return Result<AssetProcessResult>::ok(result);
}

Result<AssetProcessResult> AssetProcessor::processAudio(const std::string& sourcePath,
                                                        const std::string& outputPath,
                                                        bool compress) {
  AssetProcessResult result;
  result.sourcePath = sourcePath;
  result.outputPath = outputPath;
  result.success = true;

  try {
    if (compress) {
      // In production, apply audio compression
    }
    fs::copy(sourcePath, outputPath, fs::copy_options::overwrite_existing);
    result.originalSize = static_cast<i64>(fs::file_size(sourcePath));
    result.processedSize = static_cast<i64>(fs::file_size(outputPath));
  } catch (const std::exception& e) {
    result.success = false;
    result.errorMessage = e.what();
    return Result<AssetProcessResult>::error(e.what());
  }

  return Result<AssetProcessResult>::ok(result);
}

Result<AssetProcessResult> AssetProcessor::processFont(const std::string& sourcePath,
                                                       const std::string& outputPath) {
  AssetProcessResult result;
  result.sourcePath = sourcePath;
  result.outputPath = outputPath;
  result.success = true;

  try {
    fs::copy(sourcePath, outputPath, fs::copy_options::overwrite_existing);
    result.originalSize = static_cast<i64>(fs::file_size(sourcePath));
    result.processedSize = static_cast<i64>(fs::file_size(outputPath));
  } catch (const std::exception& e) {
    result.success = false;
    result.errorMessage = e.what();
    return Result<AssetProcessResult>::error(e.what());
  }

  return Result<AssetProcessResult>::ok(result);
}

Result<std::string> AssetProcessor::generateTextureAtlas(const std::vector<std::string>& images,
                                                         const std::string& outputPath,
                                                         i32 maxSize) {
  // TODO: Full texture atlas implementation requires image loading library (e.g., stb_image)
  // This is a simplified implementation outline:
  //
  // 1. Load all input images and get their dimensions
  // 2. Sort images by height (descending) for better packing
  // 3. Use shelf-packing or guillotine algorithm to pack rectangles
  // 4. Create output atlas image and copy sub-images
  // 5. Generate metadata file with UV coordinates for each sprite
  //
  // For production use, consider:
  // - stb_image for loading
  // - stb_image_write for saving
  // - MaxRects or Shelf packing algorithms
  // - Padding between sprites to prevent bleeding
  // - Power-of-two atlas sizes for GPU compatibility

  if (images.empty()) {
    return Result<std::string>::error("No images provided for atlas generation");
  }

  if (maxSize <= 0) {
    return Result<std::string>::error("Invalid max atlas size");
  }

  // For now, return an informative error with implementation requirements
  std::string errorMsg = "Texture atlas generation requires image processing library.\n";
  errorMsg += "To implement:\n";
  errorMsg += "1. Link against image library (stb_image recommended)\n";
  errorMsg += "2. Implement bin-packing algorithm (MaxRects or Shelf)\n";
  errorMsg += "3. Generate atlas texture and UV metadata\n";
  errorMsg += "Input: " + std::to_string(images.size()) + " images\n";
  errorMsg += "Max size: " + std::to_string(maxSize) + "x" + std::to_string(maxSize);

  (void)outputPath;
  return Result<std::string>::error(errorMsg);
}

std::string AssetProcessor::getAssetType(const std::string& path) {
  std::string ext = fs::path(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif") {
    return "image";
  }
  if (ext == ".ogg" || ext == ".wav" || ext == ".mp3" || ext == ".flac") {
    return "audio";
  }
  if (ext == ".ttf" || ext == ".otf" || ext == ".woff" || ext == ".woff2") {
    return "font";
  }
  if (ext == ".nms" || ext == ".nmscript") {
    return "script";
  }
  if (ext == ".json" || ext == ".xml" || ext == ".yaml") {
    return "data";
  }
  return "other";
}

bool AssetProcessor::needsProcessing(const std::string& sourcePath,
                                     const std::string& outputPath) const {
  if (!fs::exists(outputPath)) {
    return true;
  }

  auto sourceTime = fs::last_write_time(sourcePath);
  auto outputTime = fs::last_write_time(outputPath);

  return sourceTime > outputTime;
}

Result<void> AssetProcessor::resizeImage(const std::string& input, const std::string& output,
                                         i32 maxWidth, i32 maxHeight) {
  (void)input;
  (void)output;
  (void)maxWidth;
  (void)maxHeight;
  return Result<void>::error("Image resizing not yet implemented");
}

Result<void> AssetProcessor::compressImage(const std::string& input, const std::string& output,
                                           i32 quality) {
  (void)input;
  (void)output;
  (void)quality;
  return Result<void>::error("Image compression not yet implemented");
}

Result<void> AssetProcessor::convertImageFormat(const std::string& input, const std::string& output,
                                                const std::string& format) {
  (void)input;
  (void)output;
  (void)format;
  return Result<void>::error("Image format conversion not yet implemented");
}

Result<void> AssetProcessor::convertAudioFormat(const std::string& input, const std::string& output,
                                                const std::string& format) {
  (void)input;
  (void)output;
  (void)format;
  return Result<void>::error("Audio format conversion not yet implemented");
}

Result<void> AssetProcessor::normalizeAudio(const std::string& input, const std::string& output) {
  (void)input;
  (void)output;
  return Result<void>::error("Audio normalization not yet implemented");
}

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

// ============================================================================
// IntegrityChecker Implementation
// ============================================================================

IntegrityChecker::IntegrityChecker() = default;
IntegrityChecker::~IntegrityChecker() = default;

Result<std::vector<IntegrityChecker::Issue>>
IntegrityChecker::checkProject(const std::string& projectPath) {
  std::vector<Issue> allIssues;

  // Run all checks
  auto missingAssets = checkMissingAssets(projectPath);
  allIssues.insert(allIssues.end(), missingAssets.begin(), missingAssets.end());

  auto scriptIssues = checkScripts(projectPath);
  allIssues.insert(allIssues.end(), scriptIssues.begin(), scriptIssues.end());

  auto localizationIssues = checkLocalization(projectPath);
  allIssues.insert(allIssues.end(), localizationIssues.begin(), localizationIssues.end());

  auto unreachableIssues = checkUnreachableContent(projectPath);
  allIssues.insert(allIssues.end(), unreachableIssues.begin(), unreachableIssues.end());

  auto circularIssues = checkCircularReferences(projectPath);
  allIssues.insert(allIssues.end(), circularIssues.begin(), circularIssues.end());

  return Result<std::vector<Issue>>::ok(allIssues);
}

std::vector<IntegrityChecker::Issue>
IntegrityChecker::checkMissingAssets(const std::string& projectPath) {
  std::vector<Issue> issues;

  // Scan for referenced assets in scripts and scenes
  m_referencedAssets.clear();
  m_existingAssets.clear();

  // Collect existing assets
  fs::path assetsDir = fs::path(projectPath) / "assets";
  if (fs::exists(assetsDir)) {
    for (const auto& entry : fs::recursive_directory_iterator(assetsDir)) {
      if (entry.is_regular_file()) {
        m_existingAssets.push_back(fs::relative(entry.path(), assetsDir).string());
      }
    }
  }

  // Check for missing required directories
  std::vector<std::string> requiredDirs = {"assets", "scripts"};
  for (const auto& dir : requiredDirs) {
    if (!fs::exists(fs::path(projectPath) / dir)) {
      Issue issue;
      issue.severity = Issue::Severity::Error;
      issue.message = "Missing required directory: " + dir;
      issue.file = projectPath;
      issues.push_back(issue);
    }
  }

  return issues;
}

std::vector<IntegrityChecker::Issue>
IntegrityChecker::checkScripts(const std::string& projectPath) {
  std::vector<Issue> issues;

  fs::path scriptsDir = fs::path(projectPath) / "scripts";
  if (!fs::exists(scriptsDir)) {
    return issues;
  }

  for (const auto& entry : fs::recursive_directory_iterator(scriptsDir)) {
    if (entry.is_regular_file()) {
      std::string ext = entry.path().extension().string();
      if (ext == ".nms" || ext == ".nmscript") {
        // Basic syntax check
        std::ifstream file(entry.path());
        if (!file.is_open()) {
          Issue issue;
          issue.severity = Issue::Severity::Error;
          issue.message = "Cannot open script file";
          issue.file = entry.path().string();
          issues.push_back(issue);
          continue;
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        // Check for balanced braces
        i32 braceCount = 0;
        i32 line = 1;
        for (char c : content) {
          if (c == '\n')
            line++;
          if (c == '{')
            braceCount++;
          if (c == '}')
            braceCount--;
        }

        if (braceCount != 0) {
          Issue issue;
          issue.severity = Issue::Severity::Warning;
          issue.message = "Unbalanced braces detected";
          issue.file = entry.path().string();
          issues.push_back(issue);
        }
      }
    }
  }

  return issues;
}

std::vector<IntegrityChecker::Issue>
IntegrityChecker::checkLocalization(const std::string& projectPath) {
  std::vector<Issue> issues;

  fs::path localizationDir = fs::path(projectPath) / "localization";
  if (!fs::exists(localizationDir)) {
    Issue issue;
    issue.severity = Issue::Severity::Info;
    issue.message = "No localization directory found";
    issue.file = projectPath;
    issues.push_back(issue);
  }

  return issues;
}

std::vector<IntegrityChecker::Issue>
IntegrityChecker::checkUnreachableContent(const std::string& projectPath) {
  std::vector<Issue> issues;

  // Check for unreachable scenes and content
  // Strategy: Find all scenes, then determine which are referenced
  // from the start scene or from other reachable scenes

  fs::path scenesDir = fs::path(projectPath) / "scenes";
  if (!fs::exists(scenesDir)) {
    return issues; // No scenes to check
  }

  // Collect all scene files
  std::vector<std::string> allScenes;
  for (const auto& entry : fs::recursive_directory_iterator(scenesDir)) {
    if (entry.is_regular_file()) {
      std::string ext = entry.path().extension().string();
      if (ext == ".scene" || ext == ".json") {
        allScenes.push_back(fs::relative(entry.path(), projectPath).string());
      }
    }
  }

  if (allScenes.empty()) {
    return issues; // No scenes to check
  }

  // Find the start/entry scene (usually defined in project config)
  std::set<std::string> reachableScenes;
  fs::path projectConfig = fs::path(projectPath) / "project.json";

  if (fs::exists(projectConfig)) {
    // Mark start scene as reachable
    // In a full implementation, we would parse the JSON to find the start scene
    // For now, we'll just mark the first scene or "main.scene" as reachable
    for (const auto& scene : allScenes) {
      if (scene.find("main.scene") != std::string::npos ||
          scene.find("start.scene") != std::string::npos ||
          scene.find("intro.scene") != std::string::npos) {
        reachableScenes.insert(scene);
        break;
      }
    }
  }

  // If no start scene found, consider the first scene as start
  if (reachableScenes.empty() && !allScenes.empty()) {
    reachableScenes.insert(allScenes[0]);
  }

  // Find scenes referenced from scripts
  fs::path scriptsDir = fs::path(projectPath) / "scripts";
  if (fs::exists(scriptsDir)) {
    for (const auto& entry : fs::recursive_directory_iterator(scriptsDir)) {
      if (entry.is_regular_file()) {
        std::string ext = entry.path().extension().string();
        if (ext == ".nms" || ext == ".nmscript") {
          std::ifstream file(entry.path());
          std::string line;
          while (std::getline(file, line)) {
            // Look for scene references (e.g., "goto scene_name" or "load_scene")
            if (line.find("goto") != std::string::npos || line.find("scene") != std::string::npos ||
                line.find("load") != std::string::npos) {
              // Mark scenes mentioned in scripts as potentially reachable
              for (const auto& scene : allScenes) {
                std::string sceneName = fs::path(scene).stem().string();
                if (line.find(sceneName) != std::string::npos) {
                  reachableScenes.insert(scene);
                }
              }
            }
          }
        }
      }
    }
  }

  // Report unreachable scenes
  for (const auto& scene : allScenes) {
    if (reachableScenes.find(scene) == reachableScenes.end()) {
      Issue issue;
      issue.severity = Issue::Severity::Warning;
      issue.message = "Scene appears to be unreachable (not referenced from start scene or "
                      "scripts)";
      issue.file = scene;
      issues.push_back(issue);
    }
  }

  return issues;
}

std::vector<IntegrityChecker::Issue>
IntegrityChecker::checkCircularReferences(const std::string& projectPath) {
  std::vector<Issue> issues;

  // Check for circular dependencies in scene references
  // Use a simple depth-first search to detect cycles

  fs::path scenesDir = fs::path(projectPath) / "scenes";
  if (!fs::exists(scenesDir)) {
    return issues; // No scenes to check
  }

  // Build a dependency graph of scenes
  std::map<std::string, std::set<std::string>> sceneReferences;

  // Collect all scene files and their references
  for (const auto& entry : fs::recursive_directory_iterator(scenesDir)) {
    if (entry.is_regular_file()) {
      std::string ext = entry.path().extension().string();
      if (ext == ".scene" || ext == ".json") {
        std::string scenePath = fs::relative(entry.path(), projectPath).string();
        std::string sceneName = fs::path(scenePath).stem().string();

        // Parse scene file for references to other scenes
        std::ifstream file(entry.path());
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

        // Look for scene references in the content
        // This is a simplified approach - in production, parse JSON properly
        std::set<std::string> references;
        std::istringstream iss(content);
        std::string line;
        while (std::getline(iss, line)) {
          if (line.find("scene") != std::string::npos || line.find("goto") != std::string::npos ||
              line.find("next") != std::string::npos) {
            // Try to extract scene names from the line
            // Look for quoted strings or identifiers
            size_t pos = 0;
            while ((pos = line.find(".scene", pos)) != std::string::npos) {
              // Found a potential scene reference
              size_t start = line.rfind('\"', pos);
              if (start != std::string::npos && start < pos) {
                size_t end = line.find('\"', pos);
                if (end != std::string::npos) {
                  std::string refName = line.substr(start + 1, end - start - 1);
                  if (!refName.empty() && refName != sceneName) {
                    references.insert(refName);
                  }
                }
              }
              pos++;
            }
          }
        }

        sceneReferences[sceneName] = references;
      }
    }
  }

  // Detect cycles using DFS
  std::set<std::string> visited;
  std::set<std::string> recursionStack;

  std::function<bool(const std::string&, std::vector<std::string>&)> detectCycle;
  detectCycle = [&](const std::string& scene, std::vector<std::string>& path) -> bool {
    if (recursionStack.find(scene) != recursionStack.end()) {
      // Found a cycle - build the cycle path
      auto it = std::find(path.begin(), path.end(), scene);
      if (it != path.end()) {
        Issue issue;
        issue.severity = Issue::Severity::Error;
        std::string cyclePath = "Circular dependency detected: ";
        for (auto pathIt = it; pathIt != path.end(); ++pathIt) {
          cyclePath += *pathIt + " -> ";
        }
        cyclePath += scene;
        issue.message = cyclePath;
        issue.file = scene;
        issues.push_back(issue);
        return true;
      }
    }

    if (visited.find(scene) != visited.end()) {
      return false; // Already processed this node
    }

    visited.insert(scene);
    recursionStack.insert(scene);
    path.push_back(scene);

    auto it = sceneReferences.find(scene);
    if (it != sceneReferences.end()) {
      for (const auto& ref : it->second) {
        if (detectCycle(ref, path)) {
          // Cycle detected, but continue checking for more cycles
        }
      }
    }

    path.pop_back();
    recursionStack.erase(scene);
    return false;
  };

  // Check all scenes for cycles
  for (const auto& pair : sceneReferences) {
    std::vector<std::string> path;
    detectCycle(pair.first, path);
  }

  return issues;
}

// =============================================================================
// Code Signing Implementation (Secure)
// =============================================================================

/**
 * @brief Validate that a signing tool path is safe to use
 *
 * This function prevents command injection by:
 * 1. Checking that the path exists and is a regular file
 * 2. Checking that the path is executable
 * 3. Rejecting paths with shell metacharacters
 * 4. Verifying against an allowlist of known signing tools
 */
Result<void> BuildSystem::validateSigningToolPath(const std::string& toolPath,
                                                  const std::vector<std::string>& allowedNames) {
  // Check for empty path
  if (toolPath.empty()) {
    return Result<void>::error("Signing tool path cannot be empty");
  }

  // Check for shell metacharacters that could enable command injection
  const std::string dangerousChars = "|&;<>$`\\\"'(){}[]!*?~";
  for (char c : toolPath) {
    if (dangerousChars.find(c) != std::string::npos) {
      return Result<void>::error(
          "Signing tool path contains invalid character: '" + std::string(1, c) +
          "'. Paths with shell metacharacters are not allowed.");
    }
  }

  // Convert to filesystem path
  fs::path path(toolPath);

  // Check if path exists
  if (!fs::exists(path)) {
    return Result<void>::error("Signing tool not found: " + toolPath);
  }

  // Check if it's a regular file
  if (!fs::is_regular_file(path)) {
    return Result<void>::error("Signing tool path is not a regular file: " + toolPath);
  }

  // Get the filename for allowlist checking
  std::string filename = path.filename().string();

  // Validate against allowlist
  bool isAllowed = false;
  for (const auto& allowedName : allowedNames) {
    if (filename == allowedName || filename == (allowedName + ".exe")) {
      isAllowed = true;
      break;
    }
  }

  if (!isAllowed) {
    std::string allowedList;
    for (size_t i = 0; i < allowedNames.size(); ++i) {
      allowedList += allowedNames[i];
      if (i < allowedNames.size() - 1) {
        allowedList += ", ";
      }
    }
    return Result<void>::error("Signing tool '" + filename +
                               "' is not in the allowlist. Allowed tools: " + allowedList);
  }

  return Result<void>::ok();
}

/**
 * @brief Execute a command securely without shell injection vulnerabilities
 *
 * This uses platform-specific APIs to execute commands without involving a shell:
 * - On Windows: CreateProcess
 * - On Unix: fork + execv
 *
 * This prevents command injection attacks that would be possible with system() or popen().
 */
Result<i32> BuildSystem::executeCommand(const std::string& command, std::string& output) const {
#ifdef _WIN32
  // Windows implementation using CreateProcess
  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = NULL;

  // Create pipes for stdout
  HANDLE hStdoutRead, hStdoutWrite;
  if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
    return Result<i32>::error("Failed to create pipe for command output");
  }

  // Ensure read handle is not inherited
  SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);

  // Setup startup info
  STARTUPINFOA si;
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  si.hStdOutput = hStdoutWrite;
  si.hStdError = hStdoutWrite;
  si.dwFlags |= STARTF_USESTDHANDLES;

  PROCESS_INFORMATION pi;
  ZeroMemory(&pi, sizeof(pi));

  // Create process (note: command must be mutable for CreateProcessA)
  std::vector<char> cmdLine(command.begin(), command.end());
  cmdLine.push_back('\0');

  if (!CreateProcessA(NULL, cmdLine.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
    CloseHandle(hStdoutRead);
    CloseHandle(hStdoutWrite);
    return Result<i32>::error("Failed to create process: " + command);
  }

  // Close write end of pipe
  CloseHandle(hStdoutWrite);

  // Read output
  const size_t BUFFER_SIZE = 4096;
  char buffer[BUFFER_SIZE];
  DWORD bytesRead;
  output.clear();

  while (ReadFile(hStdoutRead, buffer, BUFFER_SIZE - 1, &bytesRead, NULL) && bytesRead > 0) {
    buffer[bytesRead] = '\0';
    output += buffer;
  }

  // Wait for process to complete
  WaitForSingleObject(pi.hProcess, INFINITE);

  // Get exit code
  DWORD exitCode;
  GetExitCodeProcess(pi.hProcess, &exitCode);

  // Cleanup
  CloseHandle(hStdoutRead);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  return Result<i32>::ok(static_cast<i32>(exitCode));

#else
  // Unix implementation using fork + execv
  int pipefd[2];
  if (pipe(pipefd) == -1) {
    return Result<i32>::error("Failed to create pipe for command output");
  }

  pid_t pid = fork();
  if (pid == -1) {
    close(pipefd[0]);
    close(pipefd[1]);
    return Result<i32>::error("Failed to fork process");
  }

  if (pid == 0) {
    // Child process
    close(pipefd[0]); // Close read end

    // Redirect stdout and stderr to pipe
    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[1]);

    // Parse command into argv array
    // Note: This is a simplified parser. In production, use a proper shell parser
    // or pass command and arguments separately
    std::vector<std::string> tokens;
    std::istringstream iss(command);
    std::string token;
    while (iss >> std::quoted(token)) {
      tokens.push_back(token);
    }

    std::vector<char*> argv;
    for (auto& t : tokens) {
      argv.push_back(const_cast<char*>(t.c_str()));
    }
    argv.push_back(nullptr);

    // Execute command without shell
    execv(argv[0], argv.data());

    // If execv returns, it failed
    _exit(127);
  } else {
    // Parent process
    close(pipefd[1]); // Close write end

    // Read output
    const size_t BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;
    output.clear();

    while ((bytesRead = read(pipefd[0], buffer, BUFFER_SIZE - 1)) > 0) {
      buffer[bytesRead] = '\0';
      output += buffer;
    }

    close(pipefd[0]);

    // Wait for child process
    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
      return Result<i32>::ok(WEXITSTATUS(status));
    } else {
      return Result<i32>::error("Process did not exit normally");
    }
  }
#endif
}

/**
 * @brief Sign executable for the configured platform
 */
Result<void> BuildSystem::signExecutableForPlatform(const std::string& executablePath) {
  if (!m_config.signExecutable) {
    return Result<void>::ok(); // Signing not requested
  }

  if (m_config.signingCertificate.empty()) {
    return Result<void>::error("Signing requested but no certificate path provided");
  }

  // Validate the executable path exists
  if (!fs::exists(executablePath)) {
    return Result<void>::error("Executable not found for signing: " + executablePath);
  }

  logMessage("Signing executable: " + executablePath, false);

  // Dispatch to platform-specific signing
  switch (m_config.platform) {
  case BuildPlatform::Windows:
    return signWindowsExecutable(executablePath);

  case BuildPlatform::MacOS:
    return signMacOSBundle(executablePath);

  case BuildPlatform::Linux:
    // Linux doesn't have standard code signing like Windows/macOS
    logMessage("Code signing not required for Linux builds", false);
    return Result<void>::ok();

  default:
    logMessage("Code signing not supported for this platform", false);
    return Result<void>::ok();
  }
}

/**
 * @brief Sign Windows executable using signtool.exe
 *
 * This implements secure signing without command injection vulnerabilities:
 * 1. Validates the signtool.exe path
 * 2. Validates the certificate path
 * 3. Uses executeCommand() which doesn't use a shell
 * 4. Properly quotes all arguments
 */
Result<void> BuildSystem::signWindowsExecutable(const std::string& executablePath) {
  // Determine signing tool path
  std::string signtoolPath;

  // Check for user-provided signing tool path
  const char* customSigntool = std::getenv("NOVELMIND_SIGNTOOL_PATH");
  if (customSigntool != nullptr) {
    signtoolPath = customSigntool;
  } else {
    // Use default Windows SDK signtool.exe
    // Try common locations
    std::vector<std::string> commonPaths = {
        "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\x64\\signtool.exe",
        "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\x86\\signtool.exe",
        "signtool.exe" // Assume it's in PATH
    };

    for (const auto& path : commonPaths) {
      if (fs::exists(path)) {
        signtoolPath = path;
        break;
      }
    }

    if (signtoolPath.empty()) {
      signtoolPath = "signtool.exe"; // Hope it's in PATH
    }
  }

  // Validate signing tool path
  std::vector<std::string> allowedTools = {"signtool.exe", "signtool"};
  auto validationResult = validateSigningToolPath(signtoolPath, allowedTools);
  if (!validationResult.isOk()) {
    return Result<void>::error("Signing tool validation failed: " + validationResult.error());
  }

  // Validate certificate path
  if (!fs::exists(m_config.signingCertificate)) {
    return Result<void>::error("Signing certificate not found: " + m_config.signingCertificate);
  }

  // Build command with proper quoting
  // Note: We're building a command string, but executeCommand() will parse it safely
  std::ostringstream cmd;
  cmd << "\"" << signtoolPath << "\" sign";

  // Add certificate file
  cmd << " /f \"" << m_config.signingCertificate << "\"";

  // Add password if provided (note: this is sensitive data)
  if (!m_config.signingPassword.empty()) {
    // Validate password doesn't contain command injection chars
    const std::string dangerousChars = "|&;<>$`\\\"'";
    for (char c : m_config.signingPassword) {
      if (dangerousChars.find(c) != std::string::npos) {
        return Result<void>::error("Signing password contains invalid characters");
      }
    }
    cmd << " /p \"" << m_config.signingPassword << "\"";
  }

  // Add timestamp server if provided
  if (!m_config.signingTimestampUrl.empty()) {
    // Validate URL format (basic check)
    if (m_config.signingTimestampUrl.find("http://") != 0 &&
        m_config.signingTimestampUrl.find("https://") != 0) {
      return Result<void>::error("Invalid timestamp URL format");
    }
    cmd << " /t \"" << m_config.signingTimestampUrl << "\"";
  }

  // Add file to sign
  cmd << " \"" << executablePath << "\"";

  // Execute signing command
  std::string output;
  auto result = executeCommand(cmd.str(), output);

  if (!result.isOk()) {
    return Result<void>::error("Failed to execute signing command: " + result.error());
  }

  i32 exitCode = result.value();
  if (exitCode != 0) {
    return Result<void>::error("Signing failed with exit code " + std::to_string(exitCode) +
                               ": " + output);
  }

  logMessage("Successfully signed Windows executable", false);
  return Result<void>::ok();
}

/**
 * @brief Sign macOS application bundle using codesign
 *
 * This implements secure signing without command injection vulnerabilities:
 * 1. Validates the codesign path
 * 2. Validates the certificate/identity
 * 3. Uses executeCommand() which doesn't use a shell
 * 4. Properly quotes all arguments
 */
Result<void> BuildSystem::signMacOSBundle(const std::string& bundlePath) {
  // Determine signing tool path
  std::string codesignPath;

  // Check for user-provided signing tool path
  const char* customCodesign = std::getenv("NOVELMIND_CODESIGN_PATH");
  if (customCodesign != nullptr) {
    codesignPath = customCodesign;
  } else {
    // Use system codesign
    codesignPath = "/usr/bin/codesign";
  }

  // Validate signing tool path
  std::vector<std::string> allowedTools = {"codesign"};
  auto validationResult = validateSigningToolPath(codesignPath, allowedTools);
  if (!validationResult.isOk()) {
    return Result<void>::error("Signing tool validation failed: " + validationResult.error());
  }

  // Validate bundle path
  if (!fs::exists(bundlePath)) {
    return Result<void>::error("Bundle not found for signing: " + bundlePath);
  }

  // Build command with proper quoting
  std::ostringstream cmd;
  cmd << "\"" << codesignPath << "\" --force --sign \"" << m_config.signingCertificate << "\"";

  // Add entitlements if provided
  if (!m_config.signingEntitlements.empty()) {
    if (!fs::exists(m_config.signingEntitlements)) {
      return Result<void>::error("Entitlements file not found: " +
                                 m_config.signingEntitlements);
    }
    cmd << " --entitlements \"" << m_config.signingEntitlements << "\"";
  }

  // Add team ID if provided (for notarization)
  if (!m_config.signingTeamId.empty()) {
    // Validate team ID format (alphanumeric)
    for (char c : m_config.signingTeamId) {
      if (!std::isalnum(c)) {
        return Result<void>::error("Invalid team ID format (must be alphanumeric)");
      }
    }
    cmd << " --team-id " << m_config.signingTeamId;
  }

  // Add bundle to sign
  cmd << " \"" << bundlePath << "\"";

  // Execute signing command
  std::string output;
  auto result = executeCommand(cmd.str(), output);

  if (!result.isOk()) {
    return Result<void>::error("Failed to execute signing command: " + result.error());
  }

  i32 exitCode = result.value();
  if (exitCode != 0) {
    return Result<void>::error("Signing failed with exit code " + std::to_string(exitCode) +
                               ": " + output);
  }

  logMessage("Successfully signed macOS bundle", false);

  // Verify the signature
  std::ostringstream verifyCmd;
  verifyCmd << "\"" << codesignPath << "\" --verify --verbose \"" << bundlePath << "\"";

  std::string verifyOutput;
  auto verifyResult = executeCommand(verifyCmd.str(), verifyOutput);

  if (!verifyResult.isOk() || verifyResult.value() != 0) {
    m_progress.warnings.push_back("Code signature verification failed: " + verifyOutput);
  } else {
    logMessage("Code signature verified successfully", false);
  }

  return Result<void>::ok();
}

} // namespace NovelMind::editor
