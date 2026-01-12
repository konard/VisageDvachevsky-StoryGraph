#pragma once

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include <vector>

namespace NovelMind::renderer {

class Texture {
public:
  Texture();
  ~Texture();

  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;
  Texture(Texture&& other) noexcept;
  Texture& operator=(Texture&& other) noexcept;

  Result<void> loadFromMemory(const std::vector<u8>& data);
  Result<void> loadFromRGBA(const u8* pixels, i32 width, i32 height);
  void destroy();

  [[nodiscard]] bool isValid() const;
  [[nodiscard]] i32 getWidth() const;
  [[nodiscard]] i32 getHeight() const;
  [[nodiscard]] void* getNativeHandle() const;

private:
  void* m_handle;
  i32 m_width;
  i32 m_height;
};

} // namespace NovelMind::renderer
