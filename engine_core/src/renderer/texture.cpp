#include "NovelMind/renderer/texture.hpp"
#include "NovelMind/core/logger.hpp"
#include "stb/stb_image.h"

#if defined(NOVELMIND_HAS_SDL2) && defined(NOVELMIND_HAS_OPENGL)
#include <SDL.h>
#include <SDL_opengl.h>
#endif

namespace NovelMind::renderer {

Texture::Texture() : m_handle(nullptr), m_width(0), m_height(0) {}

Texture::~Texture() {
  destroy();
}

Texture::Texture(Texture&& other) noexcept
    : m_handle(other.m_handle), m_width(other.m_width), m_height(other.m_height) {
  other.m_handle = nullptr;
  other.m_width = 0;
  other.m_height = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
  if (this != &other) {
    destroy();
    m_handle = other.m_handle;
    m_width = other.m_width;
    m_height = other.m_height;
    other.m_handle = nullptr;
    other.m_width = 0;
    other.m_height = 0;
  }
  return *this;
}

Result<void> Texture::loadFromMemory(const std::vector<u8>& data) {
  if (data.empty()) {
    return Result<void>::error("Empty texture data");
  }

  int width = 0;
  int height = 0;
  int channels = 0;
  stbi_uc* pixels =
      stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(data.data()),
                            static_cast<int>(data.size()), &width, &height, &channels, 4);

  if (!pixels || width <= 0 || height <= 0) {
    const char* reason = stbi_failure_reason();
    return Result<void>::error(reason ? reason : "Failed to decode texture");
  }

  auto result = loadFromRGBA(reinterpret_cast<const u8*>(pixels), width, height);
  stbi_image_free(pixels);
  return result;
}

Result<void> Texture::loadFromRGBA(const u8* pixels, i32 width, i32 height) {
  if (!pixels || width <= 0 || height <= 0) {
    return Result<void>::error("Invalid texture parameters");
  }

  m_width = width;
  m_height = height;

#if defined(NOVELMIND_HAS_SDL2) && defined(NOVELMIND_HAS_OPENGL)
  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  m_handle = reinterpret_cast<void*>(static_cast<uintptr_t>(tex));
#else
  m_handle = nullptr;
#endif

  return Result<void>::ok();
}

void Texture::destroy() {
#if defined(NOVELMIND_HAS_SDL2) && defined(NOVELMIND_HAS_OPENGL)
  if (m_handle) {
    GLuint tex = static_cast<GLuint>(reinterpret_cast<uintptr_t>(m_handle));
    glDeleteTextures(1, &tex);
  }
#endif
  if (m_handle) {
    // Texture resource cleanup is handled by platform backend.
    m_handle = nullptr;
  }
  m_width = 0;
  m_height = 0;
}

bool Texture::isValid() const {
  return m_width > 0 && m_height > 0;
}

i32 Texture::getWidth() const {
  return m_width;
}

i32 Texture::getHeight() const {
  return m_height;
}

void* Texture::getNativeHandle() const {
  return m_handle;
}

} // namespace NovelMind::renderer
