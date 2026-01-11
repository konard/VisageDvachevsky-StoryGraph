#pragma once

/**
 * @file secure_pack_reader.hpp
 * @brief Secure pack reader adapter for IVirtualFileSystem
 */

#include "NovelMind/vfs/pack_security.hpp"
#include "NovelMind/vfs/virtual_fs.hpp"
#include <memory>
#include <string>
#include <vector>

namespace NovelMind::vfs {

class SecurePackFileSystem : public IVirtualFileSystem {
public:
  SecurePackFileSystem();
  ~SecurePackFileSystem() override = default;

  void setPublicKeyPem(const std::string &pem);
  void setPublicKeyPath(const std::string &path);
  void setDecryptionKey(const Core::SecureVector<u8> &key);

  Result<void> mount(const std::string &packPath) override;
  void unmount(const std::string &packPath) override;
  void unmountAll() override;

  [[nodiscard]] Result<std::vector<u8>>
  readFile(const std::string &resourceId) const override;

  [[nodiscard]] bool exists(const std::string &resourceId) const override;

  [[nodiscard]] std::optional<ResourceInfo>
  getInfo(const std::string &resourceId) const override;

  [[nodiscard]] std::vector<std::string>
  listResources(ResourceType type = ResourceType::Unknown) const override;

private:
  Result<void> configureReader();

  std::unique_ptr<NovelMind::VFS::SecurePackReader> m_reader;
  std::string m_packPath;
  Core::SecureVector<u8> m_decryptionKey; // Secure storage, zeroed on destruction
  std::string m_publicKeyPem;
  std::string m_publicKeyPath;
};

} // namespace NovelMind::vfs
