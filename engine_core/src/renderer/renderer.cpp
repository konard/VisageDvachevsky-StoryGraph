#include "NovelMind/renderer/renderer.hpp"
#include "NovelMind/core/logger.hpp"
#include "NovelMind/platform/window.hpp"
#include <limits>
#include <unordered_map>

#if defined(NOVELMIND_HAS_SDL2) && defined(NOVELMIND_HAS_OPENGL)
#include <SDL.h>
#include <SDL_opengl.h>
#endif

namespace NovelMind::renderer {

class NullRenderer : public IRenderer {
public:
  Result<void> initialize(platform::IWindow& window) override {
    m_width = window.getWidth();
    m_height = window.getHeight();
    NOVELMIND_LOG_WARN("Using null renderer");
    return Result<void>::ok();
  }

  void shutdown() override {
    // Nothing to do
  }

  void beginFrame() override {
    // Nothing to do
  }

  void endFrame() override {
    // Nothing to do
  }

  void clear(const Color& /*color*/) override {
    // Nothing to do
  }

  void setBlendMode(BlendMode /*mode*/) override {
    // Nothing to do
  }

  void drawSprite(const Texture& /*texture*/, const Transform2D& /*transform*/,
                  const Color& /*tint*/) override {
    // Nothing to do
  }

  void drawSprite(const Texture& /*texture*/, const Rect& /*sourceRect*/,
                  const Transform2D& /*transform*/, const Color& /*tint*/) override {
    // Nothing to do
  }

  void drawRect(const Rect& /*rect*/, const Color& /*color*/) override {
    // Nothing to do
  }

  void fillRect(const Rect& /*rect*/, const Color& /*color*/) override {
    // Nothing to do
  }

  void drawText(const Font& /*font*/, const std::string& /*text*/, f32 /*x*/, f32 /*y*/,
                const Color& /*color*/) override {
    // Nothing to do
  }

  void setFade(f32 /*alpha*/, const Color& /*color*/) override {
    // Nothing to do
  }

  [[nodiscard]] i32 getWidth() const override { return m_width; }

  [[nodiscard]] i32 getHeight() const override { return m_height; }

private:
  i32 m_width = 0;
  i32 m_height = 0;
};

#if defined(NOVELMIND_HAS_SDL2) && defined(NOVELMIND_HAS_OPENGL)

class SDLOpenGLRenderer : public IRenderer {
public:
  SDLOpenGLRenderer() = default;
  ~SDLOpenGLRenderer() override { shutdown(); }

  Result<void> initialize(platform::IWindow& window) override {
    m_width = window.getWidth();
    m_height = window.getHeight();

    auto* native = window.getNativeHandle();
    if (!native) {
      return Result<void>::error("OpenGL renderer requires native window");
    }

    m_window = static_cast<SDL_Window*>(native);
    m_glContext = SDL_GL_CreateContext(m_window);
    if (!m_glContext) {
      return Result<void>::error(std::string("Failed to create GL context: ") + SDL_GetError());
    }

    SDL_GL_MakeCurrent(m_window, m_glContext);
    SDL_GL_SetSwapInterval(1);

    glViewport(0, 0, m_width, m_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, static_cast<GLdouble>(m_width), static_cast<GLdouble>(m_height), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    NOVELMIND_LOG_INFO("SDL OpenGL renderer initialized");
    return Result<void>::ok();
  }

  void shutdown() override {
    if (m_glContext && m_window) {
      SDL_GL_DeleteContext(m_glContext);
      m_glContext = nullptr;
    }
    m_window = nullptr;
  }

  void beginFrame() override {
    glClearColor(0.05f, 0.05f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
  }

  void endFrame() override {
    if (m_window) {
      SDL_GL_SwapWindow(m_window);
    }
  }

  void clear(const Color& color) override {
    glClearColor(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }

  void setBlendMode(BlendMode mode) override {
    switch (mode) {
    case BlendMode::None:
      glDisable(GL_BLEND);
      break;
    case BlendMode::Alpha:
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      break;
    case BlendMode::Additive:
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
      break;
    case BlendMode::Multiply:
      glEnable(GL_BLEND);
      glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
      break;
    }
  }

  void drawSprite(const Texture& texture, const Transform2D& transform,
                  const Color& tint) override {
    if (!texture.isValid()) {
      return;
    }
    drawTexturedQuad(
        texture,
        Rect{0, 0, static_cast<f32>(texture.getWidth()), static_cast<f32>(texture.getHeight())},
        transform, tint);
  }

  void drawSprite(const Texture& texture, const Rect& sourceRect, const Transform2D& transform,
                  const Color& tint) override {
    if (!texture.isValid()) {
      return;
    }
    drawTexturedQuad(texture, sourceRect, transform, tint);
  }

  void drawRect(const Rect& rect, const Color& color) override {
    glColor4f(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(rect.x, rect.y);
    glVertex2f(rect.x + rect.width, rect.y);
    glVertex2f(rect.x + rect.width, rect.y + rect.height);
    glVertex2f(rect.x, rect.y + rect.height);
    glEnd();
  }

  void fillRect(const Rect& rect, const Color& color) override {
    glColor4f(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);
    glBegin(GL_QUADS);
    glVertex2f(rect.x, rect.y);
    glVertex2f(rect.x + rect.width, rect.y);
    glVertex2f(rect.x + rect.width, rect.y + rect.height);
    glVertex2f(rect.x, rect.y + rect.height);
    glEnd();
  }

  void drawText(const Font& font, const std::string& text, f32 x, f32 y,
                const Color& color) override {
    if (text.empty()) {
      return;
    }

    auto atlas = getOrBuildAtlas(font);
    if (!atlas || !atlas->isValid()) {
      return;
    }

    const Texture& texture = atlas->getAtlasTexture();
    if (!texture.isValid()) {
      return;
    }

    f32 penX = x;
    f32 baseline = y + static_cast<f32>(atlas->getLineHeight());

    for (char c : text) {
      if (c == '\n') {
        penX = x;
        baseline += static_cast<f32>(atlas->getLineHeight());
        continue;
      }

      const auto* glyph = atlas->getGlyph(static_cast<unsigned char>(c));
      if (!glyph) {
        penX += static_cast<f32>(font.getSize()) * 0.5f;
        continue;
      }

      Rect src{glyph->uv.x * texture.getWidth(), glyph->uv.y * texture.getHeight(),
               glyph->uv.width * texture.getWidth(), glyph->uv.height * texture.getHeight()};

      Transform2D transform;
      transform.x = penX + glyph->bearingX;
      transform.y = baseline - glyph->bearingY;
      transform.scaleX = 1.0f;
      transform.scaleY = 1.0f;
      transform.rotation = 0.0f;
      transform.anchorX = 0.0f;
      transform.anchorY = 0.0f;

      drawTexturedQuad(texture, src, transform, color);

      penX += glyph->advanceX;
    }
  }

  void setFade(f32 alpha, const Color& color) override {
    glDisable(GL_TEXTURE_2D);
    glColor4f(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, alpha);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(static_cast<f32>(m_width), 0);
    glVertex2f(static_cast<f32>(m_width), static_cast<f32>(m_height));
    glVertex2f(0, static_cast<f32>(m_height));
    glEnd();
    glEnable(GL_TEXTURE_2D);
  }

  [[nodiscard]] i32 getWidth() const override { return m_width; }
  [[nodiscard]] i32 getHeight() const override { return m_height; }

private:
  std::shared_ptr<FontAtlas> getOrBuildAtlas(const Font& font) {
    const Font* key = &font;
    auto it = m_fontAtlases.find(key);
    if (it != m_fontAtlases.end()) {
      return it->second;
    }

    auto atlas = std::make_shared<FontAtlas>();
    static const std::string kDefaultCharset = []() {
      std::string charset;
      charset.reserve(95);
      for (int c = 32; c <= 126; ++c) {
        charset.push_back(static_cast<char>(c));
      }
      return charset;
    }();

    auto buildResult = atlas->build(font, kDefaultCharset);
    if (buildResult.isError()) {
      NOVELMIND_LOG_WARN("Failed to build font atlas: " + buildResult.error());
    }

    m_fontAtlases[key] = atlas;
    return atlas;
  }

  void drawTexturedQuad(const Texture& texture, const Rect& sourceRect,
                        const Transform2D& transform, const Color& tint) {
    const auto handle = reinterpret_cast<uintptr_t>(texture.getNativeHandle());
    if (handle > static_cast<uintptr_t>(std::numeric_limits<GLuint>::max())) {
      return;
    }
    GLuint texId = static_cast<GLuint>(handle);
    glBindTexture(GL_TEXTURE_2D, texId);

    glPushMatrix();
    glTranslatef(transform.x, transform.y, 0.0f);
    glRotatef(transform.rotation, 0.0f, 0.0f, 1.0f);
    glScalef(transform.scaleX, transform.scaleY, 1.0f);
    glTranslatef(-transform.anchorX, -transform.anchorY, 0.0f);

    const f32 u0 = sourceRect.x / static_cast<f32>(texture.getWidth());
    const f32 v0 = sourceRect.y / static_cast<f32>(texture.getHeight());
    const f32 u1 = (sourceRect.x + sourceRect.width) / static_cast<f32>(texture.getWidth());
    const f32 v1 = (sourceRect.y + sourceRect.height) / static_cast<f32>(texture.getHeight());

    glColor4f(tint.r / 255.0f, tint.g / 255.0f, tint.b / 255.0f, tint.a / 255.0f);

    glBegin(GL_QUADS);
    glTexCoord2f(u0, v0);
    glVertex2f(0.0f, 0.0f);

    glTexCoord2f(u1, v0);
    glVertex2f(sourceRect.width, 0.0f);

    glTexCoord2f(u1, v1);
    glVertex2f(sourceRect.width, sourceRect.height);

    glTexCoord2f(u0, v1);
    glVertex2f(0.0f, sourceRect.height);
    glEnd();

    glPopMatrix();
  }

  SDL_Window* m_window = nullptr;
  SDL_GLContext m_glContext = nullptr;
  i32 m_width = 0;
  i32 m_height = 0;
  std::unordered_map<const Font*, std::shared_ptr<FontAtlas>> m_fontAtlases;
};
#endif // NOVELMIND_HAS_SDL2 && NOVELMIND_HAS_OPENGL

std::unique_ptr<IRenderer> createRenderer() {
#if defined(NOVELMIND_HAS_SDL2) && defined(NOVELMIND_HAS_OPENGL)
  return std::make_unique<SDLOpenGLRenderer>();
#else
  // Factory function returns NullRenderer by default.
  // Platform-specific implementations (SDL/OpenGL/Vulkan) are
  // instantiated through platform layer configuration.
  return std::make_unique<NullRenderer>();
#endif
}

} // namespace NovelMind::renderer
