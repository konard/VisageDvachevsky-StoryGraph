#include "NovelMind/vfs/pack_security.hpp"

#include "pack_security_detail.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <istream>

#ifdef NOVELMIND_HAS_OPENSSL
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#endif

namespace NovelMind::VFS {

namespace {

#ifdef NOVELMIND_HAS_OPENSSL
bool tryComputeSha256(const u8* data, usize size, std::array<u8, 32>& hash) {
  if (size > 0 && !data) {
    return false;
  }

  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (!ctx) {
    return false;
  }

  bool ok = EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1;
  if (ok && size > 0) {
    ok = EVP_DigestUpdate(ctx, data, static_cast<size_t>(size)) == 1;
  }

  unsigned int hashLen = 0;
  if (ok) {
    ok = EVP_DigestFinal_ex(ctx, hash.data(), &hashLen) == 1 && hashLen == hash.size();
  }

  EVP_MD_CTX_free(ctx);
  return ok;
}

std::string getOpenSslError() {
  unsigned long err = ERR_get_error();
  if (err == 0) {
    return "Unknown OpenSSL error";
  }
  char buffer[256];
  ERR_error_string_n(err, buffer, sizeof(buffer));
  return std::string(buffer);
}
#endif

} // namespace

#ifdef NOVELMIND_HAS_OPENSSL
void PackIntegrityChecker::EVPKeyDeleter::operator()(EVP_PKEY* key) const {
  if (key) {
    EVP_PKEY_free(key);
  }
}
#endif

Result<void> PackIntegrityChecker::setPublicKeyPem(const std::string& pem) {
#ifdef NOVELMIND_HAS_OPENSSL
  if (pem.empty()) {
    return Result<void>::error("Public key PEM is empty");
  }

  // Use RAII wrapper for BIO to prevent leaks on exception
  BIO_ptr bio(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())));
  if (!bio) {
    return Result<void>::error("Failed to allocate BIO for public key");
  }

  EVP_PKEY* key = PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr);

  if (!key) {
    return Result<void>::error("Failed to parse public key PEM: " + getOpenSslError());
  }

  m_publicKey.reset(key);
  return Result<void>::ok();
#else
  (void)pem;
  return Result<void>::error("OpenSSL not available for public key parsing");
#endif
}

Result<void> PackIntegrityChecker::setPublicKeyFromFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open public key file: " + path);
  }

  std::string pem;
  if (!detail::readFileToString(file, pem)) {
    return Result<void>::error("Failed to read public key file: " + path);
  }
  return setPublicKeyPem(pem);
}

Result<PackVerificationReport> PackIntegrityChecker::verifyHeader(const u8* data, usize size) {
  PackVerificationReport report;

  if (size < 64) {
    report.result = PackVerificationResult::CorruptedHeader;
    report.message = "Pack file too small to contain valid header";
    return Result<PackVerificationReport>::ok(report);
  }

  u32 magic;
  std::memcpy(&magic, data, sizeof(magic));

  if (magic != detail::kPackMagic) {
    report.result = PackVerificationResult::InvalidMagic;
    report.message = "Invalid magic number in pack header";
    return Result<PackVerificationReport>::ok(report);
  }

  u16 versionMajor;
  std::memcpy(&versionMajor, data + 4, sizeof(versionMajor));

  if (versionMajor > detail::kPackVersionMajor) {
    report.result = PackVerificationResult::InvalidVersion;
    report.message = "Unsupported pack version: " + std::to_string(versionMajor);
    return Result<PackVerificationReport>::ok(report);
  }

  // Validate total size - offset 40 in the header struct
  u64 totalSize;
  std::memcpy(&totalSize, data + 40, sizeof(totalSize));
  if (totalSize == 0) {
    report.result = PackVerificationResult::CorruptedHeader;
    report.message = "Invalid pack total size: cannot be zero";
    return Result<PackVerificationReport>::ok(report);
  }

  report.result = PackVerificationResult::Valid;
  report.message = "Header verification passed";
  return Result<PackVerificationReport>::ok(report);
}

Result<PackVerificationReport> PackIntegrityChecker::verifyResourceTable(const u8* data, usize size,
                                                                         u64 tableOffset,
                                                                         u32 resourceCount) {
  PackVerificationReport report;

  const usize requiredSize = tableOffset + (resourceCount * detail::kResourceEntrySize);

  if (size < requiredSize) {
    report.result = PackVerificationResult::CorruptedResourceTable;
    report.message = "Pack file too small to contain resource table";
    report.errorOffset = static_cast<u32>(tableOffset);
    return Result<PackVerificationReport>::ok(report);
  }

  for (u32 i = 0; i < resourceCount; ++i) {
    const usize entryOffset = tableOffset + (i * detail::kResourceEntrySize);

    u64 dataOffset;
    std::memcpy(&dataOffset, data + entryOffset + 8, sizeof(dataOffset));

    if (dataOffset >= size) {
      report.result = PackVerificationResult::CorruptedResourceTable;
      report.message = "Invalid resource data offset in entry " + std::to_string(i);
      report.errorOffset = static_cast<u32>(entryOffset);
      return Result<PackVerificationReport>::ok(report);
    }
  }

  report.result = PackVerificationResult::Valid;
  report.message = "Resource table verification passed";
  return Result<PackVerificationReport>::ok(report);
}

Result<PackVerificationReport> PackIntegrityChecker::verifyResource(const u8* data, usize size,
                                                                    u64 offset, usize resourceSize,
                                                                    u32 expectedChecksum) {
  PackVerificationReport report;

  if (offset + resourceSize > size) {
    report.result = PackVerificationResult::CorruptedData;
    report.message = "Resource data extends beyond pack file";
    report.errorOffset = static_cast<u32>(offset);
    return Result<PackVerificationReport>::ok(report);
  }

  const u32 actualChecksum = calculateCrc32(data + offset, resourceSize);

  if (actualChecksum != expectedChecksum) {
    report.result = PackVerificationResult::ChecksumMismatch;
    report.message = "Resource checksum mismatch: expected " + std::to_string(expectedChecksum) +
                     ", got " + std::to_string(actualChecksum);
    report.errorOffset = static_cast<u32>(offset);
    return Result<PackVerificationReport>::ok(report);
  }

  report.result = PackVerificationResult::Valid;
  report.message = "Resource verification passed";
  return Result<PackVerificationReport>::ok(report);
}

Result<PackVerificationReport> PackIntegrityChecker::verifyPackSignature(const u8* data, usize size,
                                                                         const u8* signature,
                                                                         usize signatureSize) {
  PackVerificationReport report;

#ifdef NOVELMIND_HAS_OPENSSL
  if (!data || size == 0) {
    report.result = PackVerificationResult::SignatureInvalid;
    report.message = "No data provided for signature verification";
    return Result<PackVerificationReport>::ok(report);
  }

  if (!signature || signatureSize == 0) {
    report.result = PackVerificationResult::SignatureInvalid;
    report.message = "Signature data missing";
    return Result<PackVerificationReport>::ok(report);
  }

  if (!m_publicKey) {
    report.result = PackVerificationResult::SignatureInvalid;
    report.message = "Public key not set for signature verification";
    return Result<PackVerificationReport>::ok(report);
  }

  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (!ctx) {
    report.result = PackVerificationResult::SignatureInvalid;
    report.message = "Failed to create signature verification context";
    return Result<PackVerificationReport>::ok(report);
  }

  if (EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, m_publicKey.get()) != 1) {
    report.result = PackVerificationResult::SignatureInvalid;
    report.message = "Failed to initialize signature verification: " + getOpenSslError();
    EVP_MD_CTX_free(ctx);
    return Result<PackVerificationReport>::ok(report);
  }

  if (EVP_DigestVerifyUpdate(ctx, data, static_cast<size_t>(size)) != 1) {
    report.result = PackVerificationResult::SignatureInvalid;
    report.message = "Signature verification update failed: " + getOpenSslError();
    EVP_MD_CTX_free(ctx);
    return Result<PackVerificationReport>::ok(report);
  }

  int verify = EVP_DigestVerifyFinal(ctx, signature, static_cast<size_t>(signatureSize));
  EVP_MD_CTX_free(ctx);

  if (verify == 1) {
    report.result = PackVerificationResult::Valid;
    report.message = "Signature verification passed";
  } else if (verify == 0) {
    report.result = PackVerificationResult::SignatureInvalid;
    report.message = "Signature verification failed";
  } else {
    report.result = PackVerificationResult::SignatureInvalid;
    report.message = "Signature verification error: " + getOpenSslError();
  }
#else
  (void)data;
  (void)size;
  (void)signature;
  (void)signatureSize;
  report.result = PackVerificationResult::SignatureInvalid;
  report.message = "OpenSSL not available for signature verification";
#endif

  return Result<PackVerificationReport>::ok(report);
}

Result<PackVerificationReport>
PackIntegrityChecker::verifyPackSignatureStream(std::istream& stream, usize size,
                                                const u8* signature, usize signatureSize) {
  PackVerificationReport report;

#ifdef NOVELMIND_HAS_OPENSSL
  if (!signature || signatureSize == 0) {
    report.result = PackVerificationResult::SignatureInvalid;
    report.message = "Signature data missing";
    return Result<PackVerificationReport>::ok(report);
  }

  if (!m_publicKey) {
    report.result = PackVerificationResult::SignatureInvalid;
    report.message = "Public key not set for signature verification";
    return Result<PackVerificationReport>::ok(report);
  }

  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (!ctx) {
    report.result = PackVerificationResult::SignatureInvalid;
    report.message = "Failed to create signature verification context";
    return Result<PackVerificationReport>::ok(report);
  }

  if (EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, m_publicKey.get()) != 1) {
    report.result = PackVerificationResult::SignatureInvalid;
    report.message = "Failed to initialize signature verification: " + getOpenSslError();
    EVP_MD_CTX_free(ctx);
    return Result<PackVerificationReport>::ok(report);
  }

  constexpr usize kChunkSize = 64 * 1024;
  std::vector<u8> buffer(kChunkSize);
  usize remaining = size;

  while (remaining > 0) {
    const usize toRead = std::min(remaining, kChunkSize);
    stream.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(toRead));
    const std::streamsize readCount = stream.gcount();
    if (readCount <= 0) {
      report.result = PackVerificationResult::SignatureInvalid;
      report.message = "Failed to read data for signature verification";
      EVP_MD_CTX_free(ctx);
      return Result<PackVerificationReport>::ok(report);
    }

    if (EVP_DigestVerifyUpdate(ctx, buffer.data(), static_cast<size_t>(readCount)) != 1) {
      report.result = PackVerificationResult::SignatureInvalid;
      report.message = "Signature verification update failed: " + getOpenSslError();
      EVP_MD_CTX_free(ctx);
      return Result<PackVerificationReport>::ok(report);
    }

    remaining -= static_cast<usize>(readCount);
  }

  int verify = EVP_DigestVerifyFinal(ctx, signature, signatureSize);
  EVP_MD_CTX_free(ctx);

  if (verify == 1) {
    report.result = PackVerificationResult::Valid;
    report.message = "Signature verification passed";
  } else if (verify == 0) {
    report.result = PackVerificationResult::SignatureInvalid;
    report.message = "Signature verification failed";
  } else {
    report.result = PackVerificationResult::SignatureInvalid;
    report.message = "Signature verification error: " + getOpenSslError();
  }
#else
  (void)stream;
  (void)size;
  (void)signature;
  (void)signatureSize;
  report.result = PackVerificationResult::SignatureInvalid;
  report.message = "OpenSSL not available for signature verification";
#endif

  return Result<PackVerificationReport>::ok(report);
}

u32 PackIntegrityChecker::calculateCrc32(const u8* data, usize size) {
  u32 crc = detail::updateCrc32(0xFFFFFFFF, data, size);
  return ~crc;
}

std::array<u8, 32> PackIntegrityChecker::calculateSha256(const u8* data, usize size) {
  std::array<u8, 32> hash{};

#ifdef NOVELMIND_HAS_OPENSSL
  (void)tryComputeSha256(data, size, hash);
#else
  detail::Sha256Context ctx;
  detail::sha256Init(ctx);
  detail::sha256Update(ctx, data, size);
  detail::sha256Final(ctx, hash.data());
#endif

  return hash;
}

} // namespace NovelMind::VFS
