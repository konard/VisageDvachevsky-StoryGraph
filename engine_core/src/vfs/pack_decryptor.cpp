#include "NovelMind/vfs/pack_security.hpp"

#include "pack_security_detail.hpp"

#include <algorithm>
#include <cstring>

#ifdef NOVELMIND_HAS_OPENSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

namespace NovelMind::VFS {

void PackDecryptor::setKey(const Core::SecureVector<u8> &key) { m_key = key; }

void PackDecryptor::setKey(const u8 *key, usize keySize) {
  m_key.assign(key, key + keySize);
}

Result<std::vector<u8>> PackDecryptor::decrypt(const u8 *data, usize size,
                                               const u8 *iv, usize ivSize,
                                               const u8 *aad, usize aadSize) {
  if (m_key.empty()) {
    return Result<std::vector<u8>>::error("Decryption key not set");
  }

#ifdef NOVELMIND_HAS_OPENSSL
  if (!data || size == 0) {
    return Result<std::vector<u8>>::error("No encrypted data provided");
  }

  if (!iv || ivSize == 0) {
    return Result<std::vector<u8>>::error("Missing IV for AES-GCM decryption");
  }

  if (size <= detail::kGcmTagSize) {
    return Result<std::vector<u8>>::error("Encrypted payload too small");
  }

  const usize cipherSize = size - detail::kGcmTagSize;
  const u8 *tag = data + cipherSize;

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    return Result<std::vector<u8>>::error("Failed to create cipher context");
  }

  Core::SecureVector<u8> key256(32, 0);
  std::memcpy(key256.data(), m_key.data(), std::min(m_key.size(), size_t(32)));

  if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) !=
      1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<std::vector<u8>>::error("Failed to initialize AES-GCM");
  }

  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(ivSize),
                          nullptr) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<std::vector<u8>>::error("Failed to set IV length");
  }

  if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key256.data(), iv) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<std::vector<u8>>::error("Failed to set AES-GCM key/IV");
  }

  int outLen = 0;
  if (aad && aadSize > 0) {
    if (EVP_DecryptUpdate(ctx, nullptr, &outLen, aad,
                          static_cast<int>(aadSize)) != 1) {
      EVP_CIPHER_CTX_free(ctx);
      return Result<std::vector<u8>>::error("Failed to add AAD");
    }
  }

  std::vector<u8> result(cipherSize);
  if (EVP_DecryptUpdate(ctx, result.data(), &outLen, data,
                        static_cast<int>(cipherSize)) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<std::vector<u8>>::error("AES-GCM decryption failed");
  }

  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG,
                          static_cast<int>(detail::kGcmTagSize),
                          const_cast<u8 *>(tag)) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return Result<std::vector<u8>>::error("Failed to set AES-GCM tag");
  }

  int finalLen = 0;
  int finalResult = EVP_DecryptFinal_ex(ctx, result.data() + outLen, &finalLen);
  EVP_CIPHER_CTX_free(ctx);

  if (finalResult != 1) {
    return Result<std::vector<u8>>::error(
        "AES-GCM authentication failed (bad tag)");
  }

  result.resize(static_cast<usize>(outLen + finalLen));
  return Result<std::vector<u8>>::ok(std::move(result));
#else
  (void)data;
  (void)size;
  (void)iv;
  (void)ivSize;
  (void)aad;
  (void)aadSize;
  return Result<std::vector<u8>>::error("OpenSSL not available for decryption");
#endif
}

Result<Core::SecureVector<u8>> PackDecryptor::deriveKey(const std::string &password,
                                                 const u8 *salt,
                                                 usize saltSize) {
  if (password.empty()) {
    return Result<Core::SecureVector<u8>>::error("Password cannot be empty");
  }

#ifdef NOVELMIND_HAS_OPENSSL
  Core::SecureVector<u8> key(32);
  const int iterations = 100000;

  std::vector<u8> defaultSalt = {0x4E, 0x6F, 0x76, 0x65, 0x6C, 0x4D,
                                 0x69, 0x6E, 0x64, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00};
  const u8 *actualSalt = (salt && saltSize > 0) ? salt : defaultSalt.data();
  const usize actualSaltSize =
      (salt && saltSize > 0) ? saltSize : defaultSalt.size();

  int result = PKCS5_PBKDF2_HMAC(
      password.c_str(), static_cast<int>(password.size()), actualSalt,
      static_cast<int>(actualSaltSize), iterations, EVP_sha256(),
      static_cast<int>(key.size()), key.data());

  if (result != 1) {
    return Result<Core::SecureVector<u8>>::error("PBKDF2 derivation failed");
  }

  return Result<Core::SecureVector<u8>>::ok(std::move(key));
#else
  (void)salt;
  (void)saltSize;
  return Result<Core::SecureVector<u8>>::error("OpenSSL not available for PBKDF2");
#endif
}

Result<std::vector<u8>> PackDecryptor::generateRandomIV(usize size) {
  if (size == 0) {
    return Result<std::vector<u8>>::error("IV size must be positive");
  }

#ifdef NOVELMIND_HAS_OPENSSL
  std::vector<u8> iv(size);
  if (RAND_bytes(iv.data(), static_cast<int>(iv.size())) != 1) {
    return Result<std::vector<u8>>::error("RAND_bytes failed");
  }
  return Result<std::vector<u8>>::ok(std::move(iv));
#else
  return Result<std::vector<u8>>::error("OpenSSL not available for RNG");
#endif
}

} // namespace NovelMind::VFS
