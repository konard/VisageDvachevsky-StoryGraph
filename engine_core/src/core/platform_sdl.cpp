#include "NovelMind/core/logger.hpp"
#include "NovelMind/platform/window.hpp"

#ifdef NOVELMIND_HAS_SDL2
#include <SDL.h>
#endif

namespace NovelMind::platform {

#ifdef NOVELMIND_HAS_SDL2

class SDLWindow : public IWindow {
public:
  SDLWindow() = default;
  ~SDLWindow() override { destroy(); }

  Result<void> create(const WindowConfig& config) override {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
      return Result<void>::error(std::string("Failed to initialize SDL: ") + SDL_GetError());
    }

    u32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL;
    if (config.fullscreen) {
      flags |= SDL_WINDOW_FULLSCREEN;
    }
    if (config.resizable) {
      flags |= SDL_WINDOW_RESIZABLE;
    }

    m_window = SDL_CreateWindow(config.title.c_str(), SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, config.width, config.height, flags);

    if (!m_window) {
      // Clean up SDL on window creation failure
      SDL_Quit();
      return Result<void>::error(std::string("Failed to create window: ") + SDL_GetError());
    }

    m_config = config;
    m_shouldClose = false;

    NOVELMIND_LOG_INFO("Window created successfully");
    return Result<void>::ok();
  }

  void destroy() override {
    if (m_window) {
      SDL_DestroyWindow(m_window);
      m_window = nullptr;
    }
    SDL_Quit();
  }

  void setTitle(const std::string& title) override {
    if (m_window) {
      SDL_SetWindowTitle(m_window, title.c_str());
      m_config.title = title;
    }
  }

  void setSize(i32 width, i32 height) override {
    if (m_window) {
      SDL_SetWindowSize(m_window, width, height);
      m_config.width = width;
      m_config.height = height;
    }
  }

  void setFullscreen(bool fullscreen) override {
    if (m_window) {
      SDL_SetWindowFullscreen(m_window, fullscreen ? SDL_WINDOW_FULLSCREEN : 0);
      m_config.fullscreen = fullscreen;
    }
  }

  [[nodiscard]] i32 getWidth() const override { return m_config.width; }

  [[nodiscard]] i32 getHeight() const override { return m_config.height; }

  [[nodiscard]] bool isFullscreen() const override { return m_config.fullscreen; }

  [[nodiscard]] bool shouldClose() const override { return m_shouldClose; }

  void pollEvents() override {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        m_shouldClose = true;
      } else if (event.type == SDL_WINDOWEVENT) {
        if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
          m_config.width = event.window.data1;
          m_config.height = event.window.data2;
        }
      }
    }
  }

  void swapBuffers() override {
    if (m_window) {
      SDL_GL_SwapWindow(m_window);
    }
  }

  [[nodiscard]] void* getNativeHandle() const override { return m_window; }

private:
  SDL_Window* m_window = nullptr;
  WindowConfig m_config;
  bool m_shouldClose = false;
};

#endif // NOVELMIND_HAS_SDL2

class NullWindow : public IWindow {
public:
  Result<void> create(const WindowConfig& config) override {
    m_config = config;
    m_shouldClose = false;
    NOVELMIND_LOG_WARN("Using null window (no SDL2 available)");
    return Result<void>::ok();
  }

  void destroy() override {
    // Nothing to do
  }

  void setTitle(const std::string& title) override { m_config.title = title; }

  void setSize(i32 width, i32 height) override {
    m_config.width = width;
    m_config.height = height;
  }

  void setFullscreen(bool fullscreen) override { m_config.fullscreen = fullscreen; }

  [[nodiscard]] i32 getWidth() const override { return m_config.width; }

  [[nodiscard]] i32 getHeight() const override { return m_config.height; }

  [[nodiscard]] bool isFullscreen() const override { return m_config.fullscreen; }

  [[nodiscard]] bool shouldClose() const override { return m_shouldClose; }

  void pollEvents() override {
    // Nothing to do
  }

  void swapBuffers() override {
    // Nothing to do
  }

  [[nodiscard]] void* getNativeHandle() const override { return nullptr; }

  void requestClose() { m_shouldClose = true; }

private:
  WindowConfig m_config;
  bool m_shouldClose = false;
};

std::unique_ptr<IWindow> createWindow() {
#ifdef NOVELMIND_HAS_SDL2
  return std::make_unique<SDLWindow>();
#else
  return std::make_unique<NullWindow>();
#endif
}

} // namespace NovelMind::platform
