#pragma once

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/renderer/texture.hpp"
#include "NovelMind/renderer/transform.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace NovelMind::renderer {

class Font {
public:
  Font();
  ~Font();

  Font(const Font&) = delete;
  Font& operator=(const Font&) = delete;
  Font(Font&& other) noexcept;
  Font& operator=(Font&& other) noexcept;

  Result<void> loadFromMemory(const std::vector<u8>& data, i32 size);
  void destroy();

  [[nodiscard]] bool isValid() const;
  [[nodiscard]] i32 getSize() const;
  [[nodiscard]] void* getNativeHandle() const;

private:
  void* m_handle;
  void* m_library = nullptr;
  i32 m_size;
};

struct GlyphInfo {
  f32 advanceX = 0.0f;
  f32 bearingX = 0.0f;
  f32 bearingY = 0.0f;
  f32 width = 0.0f;
  f32 height = 0.0f;
  Rect uv{};
};

/**
 * @brief FontAtlas builds a texture atlas from a Font (FreeType-backed)
 *        to enable GPU text rendering and accurate metrics.
 */
class FontAtlas {
public:
  FontAtlas() = default;
  ~FontAtlas() = default;

  FontAtlas(const FontAtlas&) = delete;
  FontAtlas& operator=(const FontAtlas&) = delete;
  FontAtlas(FontAtlas&&) = default;
  FontAtlas& operator=(FontAtlas&&) = default;

  Result<void> build(const Font& font, const std::string& charset, i32 padding = 1);

  [[nodiscard]] const GlyphInfo* getGlyph(char32_t codepoint) const;
  [[nodiscard]] const Texture& getAtlasTexture() const { return m_texture; }
  [[nodiscard]] bool isValid() const { return m_valid; }
  [[nodiscard]] i32 getLineHeight() const { return m_lineHeight; }

private:
  GlyphInfo buildGlyph(const Font& font, char32_t codepoint, i32 padding, i32 atlasWidth,
                       i32& cursorX, i32& cursorY, i32& rowHeight, std::vector<u8>& atlasPixels,
                       i32& atlasHeight);

  Texture m_texture;
  std::unordered_map<char32_t, GlyphInfo> m_glyphs;
  i32 m_lineHeight = 0;
  bool m_valid = false;
};

} // namespace NovelMind::renderer
