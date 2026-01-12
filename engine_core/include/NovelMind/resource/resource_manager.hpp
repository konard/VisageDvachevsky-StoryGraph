#pragma once

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/renderer/font.hpp"
#include "NovelMind/renderer/texture.hpp"
#include "NovelMind/vfs/virtual_fs.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace NovelMind::resource {

using TextureHandle = std::shared_ptr<renderer::Texture>;
using FontHandle = std::shared_ptr<renderer::Font>;
using FontAtlasHandle = std::shared_ptr<renderer::FontAtlas>;

class ResourceManager {
public:
  explicit ResourceManager(vfs::IVirtualFileSystem* vfs = nullptr);
  ~ResourceManager();

  ResourceManager(const ResourceManager&) = delete;
  ResourceManager& operator=(const ResourceManager&) = delete;

  void setVfs(vfs::IVirtualFileSystem* vfs);
  void setBasePath(const std::string& path);

  [[nodiscard]] Result<TextureHandle> loadTexture(const std::string& id);
  void unloadTexture(const std::string& id);

  [[nodiscard]] Result<FontHandle> loadFont(const std::string& id, i32 size);
  void unloadFont(const std::string& id, i32 size);

  [[nodiscard]] Result<FontAtlasHandle> loadFontAtlas(const std::string& id, i32 size,
                                                      const std::string& charset);

  [[nodiscard]] Result<std::vector<u8>> readData(const std::string& id) const;

  void clearCache();

  [[nodiscard]] size_t getTextureCount() const;
  [[nodiscard]] size_t getFontCount() const;
  [[nodiscard]] size_t getFontAtlasCount() const;

private:
  Result<std::vector<u8>> readResource(const std::string& id) const;
  std::string resolvePath(const std::string& id) const;

  vfs::IVirtualFileSystem* m_vfs = nullptr;
  std::string m_basePath;

  std::unordered_map<std::string, TextureHandle> m_textures;
  std::unordered_map<std::string, std::unordered_map<i32, FontHandle>> m_fonts;
  std::unordered_map<std::string,
                     std::unordered_map<i32, std::unordered_map<std::string, FontAtlasHandle>>>
      m_fontAtlases;
};

} // namespace NovelMind::resource
