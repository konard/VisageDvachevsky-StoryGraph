#include "NovelMind/vfs/secure_pack_reader.hpp"

namespace NovelMind::vfs {

namespace {
constexpr u32 PACK_FLAG_ENCRYPTED = 1u << 0;
}

SecurePackFileSystem::SecurePackFileSystem() = default;

void SecurePackFileSystem::setPublicKeyPem(const std::string &pem) {
  m_publicKeyPem = pem;
}

void SecurePackFileSystem::setPublicKeyPath(const std::string &path) {
  m_publicKeyPath = path;
}

void SecurePackFileSystem::setDecryptionKey(const Core::SecureVector<u8> &key) {
  m_decryptionKey = key;
}

Result<void> SecurePackFileSystem::configureReader() {
  if (!m_reader) {
    m_reader = std::make_unique<NovelMind::VFS::SecurePackReader>();
  }

  if (!m_publicKeyPem.empty()) {
    auto result = m_reader->setPublicKeyPem(m_publicKeyPem);
    if (result.isError()) {
      return result;
    }
  } else if (!m_publicKeyPath.empty()) {
    auto result = m_reader->setPublicKeyFromFile(m_publicKeyPath);
    if (result.isError()) {
      return result;
    }
  }

  if (!m_decryptionKey.empty()) {
    auto decryptor = std::make_unique<NovelMind::VFS::PackDecryptor>();
    decryptor->setKey(m_decryptionKey);
    m_reader->setDecryptor(std::move(decryptor));
  }

  return Result<void>::ok();
}

Result<void> SecurePackFileSystem::mount(const std::string &packPath) {
  unmountAll();
  m_packPath = packPath;

  auto configResult = configureReader();
  if (configResult.isError()) {
    return configResult;
  }

  auto openResult = m_reader->openPack(packPath);
  if (openResult.isError()) {
    return openResult;
  }

  if ((m_reader->packFlags() & PACK_FLAG_ENCRYPTED) != 0 &&
      m_decryptionKey.empty()) {
    m_reader->closePack();
    return Result<void>::error("Encrypted pack requires decryption key");
  }

  return Result<void>::ok();
}

void SecurePackFileSystem::unmount(const std::string &packPath) {
  if (m_packPath == packPath) {
    unmountAll();
  }
}

void SecurePackFileSystem::unmountAll() {
  if (m_reader && m_reader->isOpen()) {
    m_reader->closePack();
  }
  m_packPath.clear();
}

Result<std::vector<u8>>
SecurePackFileSystem::readFile(const std::string &resourceId) const {
  if (!m_reader || !m_reader->isOpen()) {
    return Result<std::vector<u8>>::error("Pack not mounted");
  }
  return m_reader->readResource(resourceId);
}

bool SecurePackFileSystem::exists(const std::string &resourceId) const {
  return m_reader && m_reader->isOpen() && m_reader->exists(resourceId);
}

std::optional<ResourceInfo>
SecurePackFileSystem::getInfo(const std::string &resourceId) const {
  if (!m_reader || !m_reader->isOpen()) {
    return std::nullopt;
  }

  auto meta = m_reader->getResourceMeta(resourceId);
  if (!meta.has_value()) {
    return std::nullopt;
  }

  ResourceInfo info;
  info.id = resourceId;
  info.type = static_cast<ResourceType>(meta->type);
  info.size = static_cast<usize>(meta->uncompressedSize);
  info.checksum = meta->checksum;
  return info;
}

std::vector<std::string>
SecurePackFileSystem::listResources(ResourceType type) const {
  if (!m_reader || !m_reader->isOpen()) {
    return {};
  }

  auto resources = m_reader->listResources();
  if (type == ResourceType::Unknown) {
    return resources;
  }

  std::vector<std::string> filtered;
  for (const auto &id : resources) {
    auto meta = m_reader->getResourceMeta(id);
    if (meta.has_value() && static_cast<ResourceType>(meta->type) == type) {
      filtered.push_back(id);
    }
  }
  return filtered;
}

} // namespace NovelMind::vfs
