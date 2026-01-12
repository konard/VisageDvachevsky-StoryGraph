#pragma once

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/platform/window.hpp"
#include "NovelMind/renderer/color.hpp"
#include "NovelMind/renderer/font.hpp"
#include "NovelMind/renderer/texture.hpp"
#include "NovelMind/renderer/transform.hpp"
#include <memory>
#include <string>

namespace NovelMind::renderer {

enum class BlendMode { None, Alpha, Additive, Multiply };

class IRenderer {
public:
  virtual ~IRenderer() = default;

  virtual Result<void> initialize(platform::IWindow& window) = 0;
  virtual void shutdown() = 0;

  virtual void beginFrame() = 0;
  virtual void endFrame() = 0;

  virtual void clear(const Color& color) = 0;

  virtual void setBlendMode(BlendMode mode) = 0;

  // Sprite rendering
  virtual void drawSprite(const Texture& texture, const Transform2D& transform,
                          const Color& tint = Color::White) = 0;

  virtual void drawSprite(const Texture& texture, const Rect& sourceRect,
                          const Transform2D& transform, const Color& tint = Color::White) = 0;

  // Primitive rendering
  virtual void drawRect(const Rect& rect, const Color& color) = 0;
  virtual void fillRect(const Rect& rect, const Color& color) = 0;

  // Text rendering
  virtual void drawText(const Font& font, const std::string& text, f32 x, f32 y,
                        const Color& color = Color::White) = 0;

  // Screen effects
  virtual void setFade(f32 alpha, const Color& color = Color::Black) = 0;

  [[nodiscard]] virtual i32 getWidth() const = 0;
  [[nodiscard]] virtual i32 getHeight() const = 0;
};

std::unique_ptr<IRenderer> createRenderer();

} // namespace NovelMind::renderer
