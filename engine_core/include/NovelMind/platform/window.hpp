#pragma once

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include <memory>
#include <string>

namespace NovelMind::platform {

struct WindowConfig {
  std::string title = "NovelMind";
  i32 width = 1280;
  i32 height = 720;
  bool fullscreen = false;
  bool resizable = true;
  bool vsync = true;
};

class IWindow {
public:
  virtual ~IWindow() = default;

  virtual Result<void> create(const WindowConfig& config) = 0;
  virtual void destroy() = 0;

  virtual void setTitle(const std::string& title) = 0;
  virtual void setSize(i32 width, i32 height) = 0;
  virtual void setFullscreen(bool fullscreen) = 0;

  [[nodiscard]] virtual i32 getWidth() const = 0;
  [[nodiscard]] virtual i32 getHeight() const = 0;
  [[nodiscard]] virtual bool isFullscreen() const = 0;
  [[nodiscard]] virtual bool shouldClose() const = 0;

  virtual void pollEvents() = 0;
  virtual void swapBuffers() = 0;

  [[nodiscard]] virtual void* getNativeHandle() const = 0;
};

std::unique_ptr<IWindow> createWindow();

} // namespace NovelMind::platform
