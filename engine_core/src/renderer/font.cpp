#include "NovelMind/renderer/font.hpp"
#include "NovelMind/core/logger.hpp"
#include <algorithm>
#include <cstring>

#if defined(NOVELMIND_HAS_FREETYPE)
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

namespace NovelMind::renderer {

Font::Font() : m_handle(nullptr), m_size(0) {}

Font::~Font() {
  destroy();
}

Font::Font(Font&& other) noexcept : m_handle(other.m_handle), m_size(other.m_size) {
  other.m_handle = nullptr;
  other.m_size = 0;
}

Font& Font::operator=(Font&& other) noexcept {
  if (this != &other) {
    destroy();
    m_handle = other.m_handle;
    m_size = other.m_size;
    other.m_handle = nullptr;
    other.m_size = 0;
  }
  return *this;
}

Result<void> Font::loadFromMemory(const std::vector<u8>& data, i32 size) {
  if (data.empty() || size <= 0) {
    return Result<void>::error("Invalid font data or size");
  }

#if defined(NOVELMIND_HAS_FREETYPE)
  FT_Library ft;
  if (FT_Init_FreeType(&ft)) {
    return Result<void>::error("Failed to init FreeType");
  }

  FT_Face face;
  if (FT_New_Memory_Face(ft, reinterpret_cast<const FT_Byte*>(data.data()),
                         static_cast<FT_Long>(data.size()), 0, &face)) {
    FT_Done_FreeType(ft);
    return Result<void>::error("Failed to load font from memory");
  }

  if (FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(size))) {
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    return Result<void>::error("Failed to set font pixel size");
  }

  m_handle = face;
  m_size = size;
  m_library = ft;
  NOVELMIND_LOG_INFO("Font loaded via FreeType, size " + std::to_string(size));
  return Result<void>::ok();
#else
  m_size = size;
  NOVELMIND_LOG_WARN("FreeType not available, font metrics are placeholders");
  return Result<void>::ok();
#endif
}

void Font::destroy() {
#if defined(NOVELMIND_HAS_FREETYPE)
  if (m_handle) {
    FT_Face face = static_cast<FT_Face>(m_handle);
    FT_Done_Face(face);
    m_handle = nullptr;
  }
  if (m_library) {
    FT_Done_FreeType(static_cast<FT_Library>(m_library));
    m_library = nullptr;
  }
#endif
  if (m_handle) {
    // Font resource cleanup is handled by platform backend.
    m_handle = nullptr;
  }
  m_size = 0;
}

bool Font::isValid() const {
  return m_size > 0;
}

i32 Font::getSize() const {
  return m_size;
}

void* Font::getNativeHandle() const {
  return m_handle;
}

} // namespace NovelMind::renderer

namespace NovelMind::renderer {

Result<void> FontAtlas::build(const Font& font, const std::string& charset, i32 padding) {
  m_glyphs.clear();
  m_texture.destroy();
  m_lineHeight = 0;
  m_valid = false;

  if (!font.isValid()) {
    return Result<void>::error("FontAtlas::build - font is not loaded");
  }

#if defined(NOVELMIND_HAS_FREETYPE)
  auto* face = static_cast<FT_Face>(font.getNativeHandle());
  if (!face) {
    return Result<void>::error("FontAtlas::build - missing FreeType face");
  }

  // Fixed atlas width for simplicity; height will grow as needed.
  const i32 atlasWidth = 1024;
  i32 atlasHeight = padding * 2;
  std::vector<u8> atlasPixels(static_cast<size_t>(atlasWidth * atlasHeight * 4), 0);

  i32 cursorX = padding;
  i32 cursorY = padding;
  i32 rowHeight = 0;

  // Record line height from the face metrics
  if (face->size) {
    m_lineHeight = static_cast<i32>(face->size->metrics.height / 64); // 26.6 fixed point
  } else {
    m_lineHeight = font.getSize();
  }

  auto addGlyph = [&](char32_t codepoint) -> Result<void> {
    if (FT_Load_Char(face, static_cast<FT_ULong>(codepoint), FT_LOAD_RENDER)) {
      return Result<void>::error("Failed to load glyph");
    }

    FT_GlyphSlot g = face->glyph;
    const i32 glyphW = static_cast<i32>(g->bitmap.width);
    const i32 glyphH = static_cast<i32>(g->bitmap.rows);

    if (glyphW == 0 || glyphH == 0) {
      GlyphInfo info{};
      info.advanceX = static_cast<f32>(g->advance.x) / 64.0f;
      info.uv = Rect(0, 0, 0, 0);
      m_glyphs[codepoint] = info;
      return Result<void>::ok();
    }

    // Wrap to next line if needed
    if (cursorX + glyphW + padding >= atlasWidth) {
      cursorX = padding;
      cursorY += rowHeight + padding;
      rowHeight = 0;
    }

    // Resize atlas if necessary
    const i32 requiredHeight = cursorY + glyphH + padding;
    if (requiredHeight > atlasHeight) {
      const i32 newHeight = std::max(requiredHeight, atlasHeight + 256);
      atlasPixels.resize(static_cast<size_t>(atlasWidth * newHeight * 4), 0);
      atlasHeight = newHeight;
    }

    // Copy bitmap into atlas (RGBA, white with alpha from glyph)
    for (i32 y = 0; y < glyphH; ++y) {
      const u8* srcRow = reinterpret_cast<const u8*>(g->bitmap.buffer + y * g->bitmap.pitch);
      for (i32 x = 0; x < glyphW; ++x) {
        const u8 alpha = srcRow[x];
        const size_t dstIndex =
            static_cast<size_t>(((cursorY + y) * atlasWidth + (cursorX + x)) * 4);
        atlasPixels[dstIndex + 0] = 255;
        atlasPixels[dstIndex + 1] = 255;
        atlasPixels[dstIndex + 2] = 255;
        atlasPixels[dstIndex + 3] = alpha;
      }
    }

    rowHeight = std::max(rowHeight, glyphH);

    GlyphInfo info{};
    info.advanceX = static_cast<f32>(g->advance.x) / 64.0f;
    info.bearingX = static_cast<f32>(g->bitmap_left);
    info.bearingY = static_cast<f32>(g->bitmap_top);
    info.width = static_cast<f32>(glyphW);
    info.height = static_cast<f32>(glyphH);
    info.uv = Rect(static_cast<f32>(cursorX), static_cast<f32>(cursorY), static_cast<f32>(glyphW),
                   static_cast<f32>(glyphH));

    m_glyphs[codepoint] = info;

    cursorX += glyphW + padding;
    return Result<void>::ok();
  };

  // Build glyphs for charset
  for (char c : charset) {
    const auto code = static_cast<unsigned char>(c);
    auto res = addGlyph(static_cast<char32_t>(code));
    if (res.isError()) {
      NOVELMIND_LOG_WARN("FontAtlas::build - failed for glyph " +
                         std::to_string(static_cast<int>(code)) + ": " + res.error());
    }
  }

  // Ensure atlas height fits final row
  atlasHeight = std::max(atlasHeight, cursorY + rowHeight + padding);
  atlasPixels.resize(static_cast<size_t>(atlasWidth * atlasHeight * 4), 0);

  // Normalize UVs now that final atlas dimensions are known
  for (auto& [_, glyph] : m_glyphs) {
    glyph.uv.x /= static_cast<f32>(atlasWidth);
    glyph.uv.y /= static_cast<f32>(atlasHeight);
    glyph.uv.width /= static_cast<f32>(atlasWidth);
    glyph.uv.height /= static_cast<f32>(atlasHeight);
  }

  auto texRes = m_texture.loadFromRGBA(atlasPixels.data(), atlasWidth, atlasHeight);
  if (texRes.isError()) {
    return texRes;
  }

  m_valid = m_texture.isValid();
  if (!m_valid) {
    return Result<void>::error("FontAtlas texture is not valid");
  }
  return Result<void>::ok();
#else
  (void)font;
  (void)charset;
  (void)padding;
  return Result<void>::error("FreeType not available for FontAtlas");
#endif
}

const GlyphInfo* FontAtlas::getGlyph(char32_t codepoint) const {
  auto it = m_glyphs.find(codepoint);
  if (it != m_glyphs.end()) {
    return &it->second;
  }
  return nullptr;
}

} // namespace NovelMind::renderer
