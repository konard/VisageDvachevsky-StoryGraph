#include "NovelMind/vfs/pack_security.hpp"

#include "pack_security_detail.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <utility>

#ifdef NOVELMIND_HAS_ZLIB
#include <zlib.h>
#endif

namespace NovelMind::VFS {

void SecurePackReader::setDecryptor(std::unique_ptr<PackDecryptor> decryptor) {
  m_decryptor = std::move(decryptor);
}

void SecurePackReader::setIntegrityChecker(
    std::unique_ptr<PackIntegrityChecker> checker) {
  m_integrityChecker = std::move(checker);
}

Result<void> SecurePackReader::setPublicKeyPem(const std::string &pem) {
  if (!m_integrityChecker) {
    m_integrityChecker = std::make_unique<PackIntegrityChecker>();
  }
  return m_integrityChecker->setPublicKeyPem(pem);
}

Result<void> SecurePackReader::setPublicKeyFromFile(const std::string &path) {
  if (!m_integrityChecker) {
    m_integrityChecker = std::make_unique<PackIntegrityChecker>();
  }
  return m_integrityChecker->setPublicKeyFromFile(path);
}

Result<void> SecurePackReader::openPack(const std::string &path) {
  closePack();
  m_packPath = path;

  if (!m_integrityChecker) {
    m_integrityChecker = std::make_unique<PackIntegrityChecker>();
  }

  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    m_lastResult = PackVerificationResult::CorruptedHeader;
    return Result<void>::error("Failed to open pack file: " + path);
  }

  file.seekg(0, std::ios::end);
  const std::streamoff endPos = file.tellg();
  if (endPos < 0) {
    m_lastResult = PackVerificationResult::CorruptedHeader;
    return Result<void>::error("Failed to determine pack file size");
  }

  m_fileSize = static_cast<u64>(endPos);
  if (m_fileSize < sizeof(PackHeader) + detail::kFooterSize) {
    m_lastResult = PackVerificationResult::CorruptedHeader;
    return Result<void>::error("Pack file too small");
  }

  file.seekg(0, std::ios::beg);
  file.read(reinterpret_cast<char *>(&m_header), sizeof(m_header));
  if (!file) {
    m_lastResult = PackVerificationResult::CorruptedHeader;
    return Result<void>::error("Failed to read pack header");
  }

  if (m_header.magic != detail::kPackMagic) {
    m_lastResult = PackVerificationResult::InvalidMagic;
    return Result<void>::error("Invalid pack magic number");
  }

  if (m_header.versionMajor > detail::kPackVersionMajor) {
    m_lastResult = PackVerificationResult::InvalidVersion;
    return Result<void>::error("Unsupported pack version");
  }

  if (m_header.totalSize != 0 && m_header.totalSize != m_fileSize) {
    m_lastResult = PackVerificationResult::CorruptedHeader;
    return Result<void>::error("Pack size mismatch");
  }

  constexpr u32 MAX_RESOURCE_COUNT = 1000000;
  if (m_header.resourceCount > MAX_RESOURCE_COUNT) {
    m_lastResult = PackVerificationResult::CorruptedHeader;
    return Result<void>::error("Resource count exceeds maximum");
  }

  const u64 resourceTableSize =
      static_cast<u64>(m_header.resourceCount) * detail::kResourceEntrySize;
  if (m_header.resourceTableOffset < sizeof(PackHeader)) {
    m_lastResult = PackVerificationResult::CorruptedResourceTable;
    return Result<void>::error("Invalid resource table offset/size");
  }
  // Safe check to prevent overflow: offset + size <= fileSize
  // Rewritten as: offset <= fileSize && size <= fileSize - offset
  if (m_header.resourceTableOffset > m_fileSize ||
      resourceTableSize > m_fileSize - m_header.resourceTableOffset) {
    m_lastResult = PackVerificationResult::CorruptedResourceTable;
    return Result<void>::error("Invalid resource table offset/size");
  }

  const u64 resourceTableEnd = m_header.resourceTableOffset + resourceTableSize;
  if (m_header.stringTableOffset < resourceTableEnd ||
      m_header.stringTableOffset > m_fileSize) {
    m_lastResult = PackVerificationResult::CorruptedResourceTable;
    return Result<void>::error("Invalid string table offset");
  }

  if (m_header.dataOffset < m_header.stringTableOffset ||
      m_header.dataOffset > m_fileSize - detail::kFooterSize) {
    m_lastResult = PackVerificationResult::CorruptedResourceTable;
    return Result<void>::error("Invalid data offset");
  }

  file.seekg(static_cast<std::streamoff>(m_header.resourceTableOffset));
  std::vector<PackResourceEntry> entries(m_header.resourceCount);
  if (!entries.empty()) {
    file.read(reinterpret_cast<char *>(entries.data()),
              static_cast<std::streamsize>(entries.size() *
                                           sizeof(PackResourceEntry)));
    if (!file) {
      m_lastResult = PackVerificationResult::CorruptedResourceTable;
      return Result<void>::error("Failed to read resource table");
    }
  }

  file.seekg(static_cast<std::streamoff>(m_header.stringTableOffset));
  u32 stringCount = 0;
  file.read(reinterpret_cast<char *>(&stringCount), sizeof(stringCount));
  if (!file) {
    m_lastResult = PackVerificationResult::CorruptedResourceTable;
    return Result<void>::error("Failed to read string table count");
  }

  constexpr u32 MAX_STRING_COUNT = 10000000;
  if (stringCount > MAX_STRING_COUNT) {
    m_lastResult = PackVerificationResult::CorruptedResourceTable;
    return Result<void>::error("String table count exceeds maximum");
  }

  std::vector<u32> offsets(stringCount);
  if (!offsets.empty()) {
    file.read(reinterpret_cast<char *>(offsets.data()),
              static_cast<std::streamsize>(offsets.size() * sizeof(u32)));
    if (!file) {
      m_lastResult = PackVerificationResult::CorruptedResourceTable;
      return Result<void>::error("Failed to read string table offsets");
    }
  }

  const auto stringDataStart = file.tellg();
  const u64 stringDataStartU64 =
      stringDataStart >= 0 ? static_cast<u64>(stringDataStart) : 0;
  if (stringDataStart < 0 || stringDataStartU64 > m_header.dataOffset) {
    m_lastResult = PackVerificationResult::CorruptedResourceTable;
    return Result<void>::error("Invalid string table data start");
  }

  const u64 stringDataSize = m_header.dataOffset - stringDataStartU64;
  m_stringTable.clear();
  m_stringTable.reserve(stringCount);

  constexpr usize MAX_STRING_LENGTH = 1024 * 1024;
  for (u32 i = 0; i < stringCount; ++i) {
    if (offsets[i] >= stringDataSize) {
      m_lastResult = PackVerificationResult::CorruptedResourceTable;
      return Result<void>::error("String table offset out of bounds");
    }

    file.seekg(stringDataStart + static_cast<std::streamoff>(offsets[i]));
    std::string str;
    std::getline(file, str, '\0');

    if (!file) {
      m_lastResult = PackVerificationResult::CorruptedResourceTable;
      return Result<void>::error("Failed to read string table entry");
    }

    if (str.size() > MAX_STRING_LENGTH) {
      m_lastResult = PackVerificationResult::CorruptedResourceTable;
      return Result<void>::error("String table entry too large");
    }

    // Safe boundary check to prevent integer overflow
    // Instead of: offset + length <= size (which can overflow)
    // Use: offset <= size && length <= size - offset
    const u64 offset = static_cast<u64>(offsets[i]);
    const u64 strSize = static_cast<u64>(str.size());
    if (offset > stringDataSize || strSize > stringDataSize - offset) {
      m_lastResult = PackVerificationResult::CorruptedResourceTable;
      return Result<void>::error("String table entry out of bounds");
    }

    m_stringTable.push_back(std::move(str));
  }

  constexpr u64 MAX_RESOURCE_SIZE = 512ULL * 1024 * 1024;
  m_entries.clear();
  for (const auto &entry : entries) {
    if (entry.idStringOffset >= m_stringTable.size()) {
      m_lastResult = PackVerificationResult::CorruptedResourceTable;
      return Result<void>::error("Resource ID offset out of bounds");
    }

    const std::string &resourceId = m_stringTable[entry.idStringOffset];
    if (resourceId.empty()) {
      m_lastResult = PackVerificationResult::CorruptedResourceTable;
      return Result<void>::error("Empty resource ID in string table");
    }

    if (entry.compressedSize > MAX_RESOURCE_SIZE) {
      m_lastResult = PackVerificationResult::CorruptedData;
      return Result<void>::error("Resource size exceeds limit");
    }

    // Safe check to prevent overflow when calculating absolute offset
    if (entry.dataOffset > m_fileSize ||
        m_header.dataOffset > m_fileSize - entry.dataOffset) {
      m_lastResult = PackVerificationResult::CorruptedData;
      return Result<void>::error("Resource offset overflow");
    }
    const u64 absoluteOffset = m_header.dataOffset + entry.dataOffset;

    // Safe check to prevent overflow: absoluteOffset + compressedSize <= fileSize - footerSize
    // First check if fileSize is large enough for footer
    if (m_fileSize < detail::kFooterSize) {
      m_lastResult = PackVerificationResult::CorruptedData;
      return Result<void>::error("File too small for footer");
    }
    const u64 maxDataEnd = m_fileSize - detail::kFooterSize;
    if (absoluteOffset > maxDataEnd ||
        entry.compressedSize > maxDataEnd - absoluteOffset) {
      m_lastResult = PackVerificationResult::CorruptedData;
      return Result<void>::error("Resource data extends beyond pack file");
    }

    auto insertResult = m_entries.emplace(resourceId, entry);
    if (!insertResult.second) {
      m_lastResult = PackVerificationResult::CorruptedResourceTable;
      return Result<void>::error("Duplicate resource ID: " + resourceId);
    }
  }

  file.seekg(static_cast<std::streamoff>(m_fileSize - detail::kFooterSize));
  file.read(reinterpret_cast<char *>(&m_footer), sizeof(m_footer));
  if (!file) {
    m_lastResult = PackVerificationResult::CorruptedHeader;
    return Result<void>::error("Failed to read pack footer");
  }

  if (m_footer.magic != detail::kFooterMagic) {
    m_lastResult = PackVerificationResult::CorruptedHeader;
    return Result<void>::error("Invalid pack footer magic");
  }

  file.seekg(0, std::ios::beg);
  u32 crc = 0xFFFFFFFF;
  u64 remaining = m_header.dataOffset;
  std::vector<u8> buffer(64 * 1024);
  while (remaining > 0) {
    const usize toRead =
        static_cast<usize>(std::min<u64>(remaining, buffer.size()));
    file.read(reinterpret_cast<char *>(buffer.data()),
              static_cast<std::streamsize>(toRead));
    const std::streamsize readCount = file.gcount();
    if (readCount <= 0) {
      m_lastResult = PackVerificationResult::CorruptedHeader;
      return Result<void>::error("Failed to read pack for CRC verification");
    }
    crc =
        detail::updateCrc32(crc, buffer.data(), static_cast<usize>(readCount));
    remaining -= static_cast<u64>(readCount);
  }
  crc = ~crc;

  if (crc != m_footer.tablesCrc32) {
    m_lastResult = PackVerificationResult::ChecksumMismatch;
    return Result<void>::error("Pack table CRC mismatch");
  }

  const bool requiresSignature =
      (m_header.flags & detail::kPackFlagSigned) != 0;
  if (requiresSignature) {
    std::string signaturePath = path + ".sig";
    std::ifstream sigFile(signaturePath, std::ios::binary);
    if (!sigFile.is_open()) {
      m_lastResult = PackVerificationResult::SignatureInvalid;
      return Result<void>::error("Missing signature file: " + signaturePath);
    }

    std::vector<u8> signature;
    if (!detail::readFileToBytes(sigFile, signature)) {
      m_lastResult = PackVerificationResult::SignatureInvalid;
      return Result<void>::error("Failed to read signature file");
    }
    if (signature.empty()) {
      m_lastResult = PackVerificationResult::SignatureInvalid;
      return Result<void>::error("Signature file is empty");
    }

    file.clear();
    file.seekg(0, std::ios::beg);
    auto sigReport = m_integrityChecker->verifyPackSignatureStream(
        file, static_cast<usize>(m_fileSize), signature.data(),
        signature.size());
    if (!sigReport.isOk() ||
        sigReport.value().result != PackVerificationResult::Valid) {
      m_lastResult = sigReport.isOk()
                         ? sigReport.value().result
                         : PackVerificationResult::SignatureInvalid;
      return Result<void>::error(sigReport.isOk() ? sigReport.value().message
                                                  : sigReport.error());
    }
  }

  const bool hasContentHash = std::any_of(std::begin(m_header.contentHash),
                                          std::end(m_header.contentHash),
                                          [](u8 byte) { return byte != 0; });
  if (hasContentHash) {
    file.clear();
    file.seekg(0, std::ios::beg);

#ifdef NOVELMIND_HAS_OPENSSL
    EVP_MD_CTX *sha256 = EVP_MD_CTX_new();
    if (!sha256) {
      m_lastResult = PackVerificationResult::ChecksumMismatch;
      return Result<void>::error("Failed to create SHA-256 context");
    }
    if (EVP_DigestInit_ex(sha256, EVP_sha256(), nullptr) != 1) {
      EVP_MD_CTX_free(sha256);
      m_lastResult = PackVerificationResult::ChecksumMismatch;
      return Result<void>::error("Failed to initialize SHA-256");
    }
#else
    detail::Sha256Context sha256;
    detail::sha256Init(sha256);
#endif

    remaining = m_fileSize;
    while (remaining > 0) {
      const usize toRead =
          static_cast<usize>(std::min<u64>(remaining, buffer.size()));
      file.read(reinterpret_cast<char *>(buffer.data()),
                static_cast<std::streamsize>(toRead));
      const std::streamsize readCount = file.gcount();
      if (readCount <= 0) {
        m_lastResult = PackVerificationResult::ChecksumMismatch;
        return Result<void>::error("Failed to read pack for hash verification");
      }
#ifdef NOVELMIND_HAS_OPENSSL
      if (EVP_DigestUpdate(sha256, buffer.data(),
                           static_cast<size_t>(readCount)) != 1) {
        EVP_MD_CTX_free(sha256);
        m_lastResult = PackVerificationResult::ChecksumMismatch;
        return Result<void>::error("Failed to update SHA-256 hash");
      }
#else
      detail::sha256Update(sha256, buffer.data(),
                           static_cast<usize>(readCount));
#endif
      remaining -= static_cast<u64>(readCount);
    }

    std::array<u8, 32> hash{};
#ifdef NOVELMIND_HAS_OPENSSL
    unsigned int hashLen = 0;
    if (EVP_DigestFinal_ex(sha256, hash.data(), &hashLen) != 1 ||
        hashLen != hash.size()) {
      EVP_MD_CTX_free(sha256);
      m_lastResult = PackVerificationResult::ChecksumMismatch;
      return Result<void>::error("Failed to finalize SHA-256 hash");
    }
    EVP_MD_CTX_free(sha256);
#else
    detail::sha256Final(sha256, hash.data());
#endif

    if (std::memcmp(m_header.contentHash, hash.data(), 16) != 0) {
      m_lastResult = PackVerificationResult::ChecksumMismatch;
      return Result<void>::error("Pack content hash mismatch");
    }
  }

  m_isOpen = true;
  m_lastResult = PackVerificationResult::Valid;
  return Result<void>::ok();
}

void SecurePackReader::closePack() {
  m_isOpen = false;
  m_packPath.clear();
  m_entries.clear();
  m_stringTable.clear();
  m_fileSize = 0;
  m_lastResult = PackVerificationResult::Valid;
}

Result<std::vector<u8>>
SecurePackReader::readResource(const std::string &resourceId) {
  if (!m_isOpen) {
    return Result<std::vector<u8>>::error("Pack not open");
  }

  auto it = m_entries.find(resourceId);
  if (it == m_entries.end()) {
    return Result<std::vector<u8>>::error("Resource not found: " + resourceId);
  }

  const PackResourceEntry &entry = it->second;
  std::ifstream file(m_packPath, std::ios::binary);
  if (!file.is_open()) {
    return Result<std::vector<u8>>::error("Failed to open pack file");
  }

  const u64 absoluteOffset = m_header.dataOffset + entry.dataOffset;
  file.seekg(static_cast<std::streamoff>(absoluteOffset));
  if (!file) {
    return Result<std::vector<u8>>::error("Failed to seek to resource data");
  }

  std::vector<u8> data(static_cast<usize>(entry.compressedSize));
  if (!data.empty()) {
    file.read(reinterpret_cast<char *>(data.data()),
              static_cast<std::streamsize>(data.size()));
    if (!file) {
      return Result<std::vector<u8>>::error("Failed to read resource data");
    }
  }

  const bool encrypted = (m_header.flags & detail::kPackFlagEncrypted) != 0;
  const bool compressed = (m_header.flags & detail::kPackFlagCompressed) != 0;

  if (encrypted) {
    if (!m_decryptor) {
      return Result<std::vector<u8>>::error("Decryptor not configured");
    }

    std::vector<u8> aad;
    aad.reserve(resourceId.size() + 1 + sizeof(u32) + sizeof(u64));
    aad.insert(aad.end(), resourceId.begin(), resourceId.end());
    aad.push_back(0);

    auto appendU32 = [&aad](u32 value) {
      aad.push_back(static_cast<u8>(value & 0xFF));
      aad.push_back(static_cast<u8>((value >> 8) & 0xFF));
      aad.push_back(static_cast<u8>((value >> 16) & 0xFF));
      aad.push_back(static_cast<u8>((value >> 24) & 0xFF));
    };
    auto appendU64 = [&aad](u64 value) {
      for (int i = 0; i < 8; ++i) {
        aad.push_back(static_cast<u8>((value >> (i * 8)) & 0xFF));
      }
    };

    appendU32(entry.type);
    appendU64(entry.uncompressedSize);

    auto decrypted = m_decryptor->decrypt(
        data.data(), data.size(), entry.iv, sizeof(entry.iv),
        aad.empty() ? nullptr : aad.data(), aad.size());
    if (!decrypted.isOk()) {
      return Result<std::vector<u8>>::error(decrypted.error());
    }
    data = std::move(decrypted.value());
  }

  if (compressed) {
#ifdef NOVELMIND_HAS_ZLIB
    if (entry.uncompressedSize > std::numeric_limits<uLongf>::max()) {
      return Result<std::vector<u8>>::error(
          "Uncompressed size exceeds zlib limits");
    }

    std::vector<u8> decompressed(static_cast<usize>(entry.uncompressedSize));
    uLongf destLen = static_cast<uLongf>(decompressed.size());
    int res = uncompress(decompressed.data(), &destLen, data.data(),
                         static_cast<uLongf>(data.size()));
    if (res != Z_OK) {
      return Result<std::vector<u8>>::error("zlib decompression failed");
    }
    decompressed.resize(static_cast<usize>(destLen));
    data = std::move(decompressed);
#else
    return Result<std::vector<u8>>::error(
        "Compressed pack requires zlib support");
#endif
  }

  if (!data.empty() && data.size() != entry.uncompressedSize) {
    return Result<std::vector<u8>>::error(
        "Resource size mismatch after decode");
  }

  const u32 checksum =
      PackIntegrityChecker::calculateCrc32(data.data(), data.size());
  if (checksum != entry.checksum) {
    return Result<std::vector<u8>>::error("Resource checksum mismatch");
  }

  return Result<std::vector<u8>>::ok(std::move(data));
}

bool SecurePackReader::exists(const std::string &resourceId) const {
  return m_entries.find(resourceId) != m_entries.end();
}

std::vector<std::string> SecurePackReader::listResources() const {
  std::vector<std::string> result;
  result.reserve(m_entries.size());
  for (const auto &pair : m_entries) {
    result.push_back(pair.first);
  }
  return result;
}

std::optional<PackResourceMeta>
SecurePackReader::getResourceMeta(const std::string &resourceId) const {
  auto it = m_entries.find(resourceId);
  if (it == m_entries.end()) {
    return std::nullopt;
  }

  PackResourceMeta meta;
  meta.type = it->second.type;
  meta.uncompressedSize = it->second.uncompressedSize;
  meta.checksum = it->second.checksum;
  return meta;
}

} // namespace NovelMind::VFS
