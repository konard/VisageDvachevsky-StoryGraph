#include "NovelMind/resource/resource_manager.hpp"
#include "NovelMind/core/logger.hpp"
#include <filesystem>
#include <fstream>

namespace NovelMind::resource {

namespace fs = std::filesystem;

namespace {

bool readFileToBytes(const std::string &path, std::vector<u8> &out) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }

  file.seekg(0, std::ios::end);
  const std::streampos size = file.tellg();
  if (size == std::streampos(-1) || size < 0) {
    return false;
  }

  out.resize(static_cast<size_t>(size));
  file.seekg(0, std::ios::beg);
  if (!out.empty()) {
    file.read(reinterpret_cast<char *>(out.data()),
              static_cast<std::streamsize>(out.size()));
    if (!file) {
      return false;
    }
  }

  return true;
}

} // namespace

ResourceManager::ResourceManager(vfs::IVirtualFileSystem *vfs) : m_vfs(vfs) {}

ResourceManager::~ResourceManager() { clearCache(); }

void ResourceManager::setVfs(vfs::IVirtualFileSystem *vfs) { m_vfs = vfs; }

void ResourceManager::setBasePath(const std::string &path) {
  m_basePath = path;
  if (!m_basePath.empty() && m_basePath.back() != '/' &&
      m_basePath.back() != '\\') {
    m_basePath.push_back(fs::path::preferred_separator);
  }
}

Result<TextureHandle> ResourceManager::loadTexture(const std::string &id) {
  if (id.empty()) {
    return Result<TextureHandle>::error("Texture id is empty");
  }

  auto it = m_textures.find(id);
  if (it != m_textures.end() && it->second && it->second->isValid()) {
    return Result<TextureHandle>::ok(it->second);
  }

  auto dataResult = readResource(id);
  if (dataResult.isError()) {
    return Result<TextureHandle>::error(dataResult.error());
  }

  auto texture = std::make_shared<renderer::Texture>();
  auto loadResult = texture->loadFromMemory(dataResult.value());
  if (loadResult.isError()) {
    return Result<TextureHandle>::error(loadResult.error());
  }

  m_textures[id] = texture;
  return Result<TextureHandle>::ok(texture);
}

void ResourceManager::unloadTexture(const std::string &id) {
  m_textures.erase(id);
}

Result<FontHandle> ResourceManager::loadFont(const std::string &id, i32 size) {
  if (id.empty()) {
    return Result<FontHandle>::error("Font id is empty");
  }
  if (size <= 0) {
    return Result<FontHandle>::error("Font size must be positive");
  }

  auto &sizeMap = m_fonts[id];
  auto it = sizeMap.find(size);
  if (it != sizeMap.end() && it->second && it->second->isValid()) {
    return Result<FontHandle>::ok(it->second);
  }

  auto dataResult = readResource(id);
  if (dataResult.isError()) {
    return Result<FontHandle>::error(dataResult.error());
  }

  auto font = std::make_shared<renderer::Font>();
  auto loadResult = font->loadFromMemory(dataResult.value(), size);
  if (loadResult.isError()) {
    return Result<FontHandle>::error(loadResult.error());
  }

  sizeMap[size] = font;
  return Result<FontHandle>::ok(font);
}

void ResourceManager::unloadFont(const std::string &id, i32 size) {
  auto it = m_fonts.find(id);
  if (it == m_fonts.end()) {
    return;
  }
  it->second.erase(size);
  if (it->second.empty()) {
    m_fonts.erase(it);
  }
}

Result<FontAtlasHandle>
ResourceManager::loadFontAtlas(const std::string &id, i32 size,
                               const std::string &charset) {
  if (charset.empty()) {
    return Result<FontAtlasHandle>::error("Font atlas charset is empty");
  }

  auto fontResult = loadFont(id, size);
  if (fontResult.isError()) {
    return Result<FontAtlasHandle>::error(fontResult.error());
  }

  auto &sizeMap = m_fontAtlases[id];
  auto &charsetMap = sizeMap[size];
  auto it = charsetMap.find(charset);
  if (it != charsetMap.end() && it->second && it->second->isValid()) {
    return Result<FontAtlasHandle>::ok(it->second);
  }

  auto atlas = std::make_shared<renderer::FontAtlas>();
  auto buildResult = atlas->build(*fontResult.value(), charset);
  if (buildResult.isError()) {
    return Result<FontAtlasHandle>::error(buildResult.error());
  }

  charsetMap[charset] = atlas;
  return Result<FontAtlasHandle>::ok(atlas);
}

Result<std::vector<u8>> ResourceManager::readData(const std::string &id) const {
  return readResource(id);
}

void ResourceManager::clearCache() {
  m_textures.clear();
  m_fonts.clear();
  m_fontAtlases.clear();
}

size_t ResourceManager::getTextureCount() const { return m_textures.size(); }

size_t ResourceManager::getFontCount() const {
  size_t count = 0;
  for (const auto &pair : m_fonts) {
    count += pair.second.size();
  }
  return count;
}

size_t ResourceManager::getFontAtlasCount() const {
  size_t count = 0;
  for (const auto &fontEntry : m_fontAtlases) {
    for (const auto &sizeEntry : fontEntry.second) {
      count += sizeEntry.second.size();
    }
  }
  return count;
}

Result<std::vector<u8>>
ResourceManager::readResource(const std::string &id) const {
  std::vector<u8> data;

  std::string path = resolvePath(id);
  if (!path.empty() && readFileToBytes(path, data)) {
    return Result<std::vector<u8>>::ok(std::move(data));
  }

  if (m_vfs) {
    auto vfsResult = m_vfs->readFile(id);
    if (vfsResult.isOk()) {
      return vfsResult;
    }
  }

  return Result<std::vector<u8>>::error("Failed to read resource: " + id);
}

std::string ResourceManager::resolvePath(const std::string &id) const {
  if (id.empty()) {
    return {};
  }

  fs::path path(id);
  if (path.is_relative()) {
    fs::path base(m_basePath.empty() ? "." : m_basePath);
    fs::path resolved = base / path;
    if (fs::exists(resolved)) {
      return resolved.string();
    }
  } else if (fs::exists(path)) {
    return path.string();
  }

  return {};
}

} // namespace NovelMind::resource
