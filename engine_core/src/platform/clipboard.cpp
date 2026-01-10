#include "NovelMind/platform/clipboard.hpp"
#include "NovelMind/core/logger.hpp"

#ifdef NOVELMIND_HAS_SDL2
#include <SDL.h>
#endif

namespace NovelMind::platform {

#ifdef NOVELMIND_HAS_SDL2

/**
 * @brief SDL2-based clipboard implementation
 */
class SDLClipboard : public IClipboard {
public:
  SDLClipboard() = default;
  ~SDLClipboard() override = default;

  Result<void> setText(const std::string& text) override {
    if (SDL_SetClipboardText(text.c_str()) != 0) {
      return Result<void>::error(std::string("Failed to set clipboard text: ") + SDL_GetError());
    }
    return Result<void>::ok();
  }

  Result<std::string> getText() override {
    if (!SDL_HasClipboardText()) {
      return Result<std::string>::error("Clipboard is empty");
    }

    char* text = SDL_GetClipboardText();
    if (!text) {
      return Result<std::string>::error(std::string("Failed to get clipboard text: ") +
                                        SDL_GetError());
    }

    std::string result(text);
    SDL_free(text);
    return Result<std::string>::ok(result);
  }

  bool hasText() override { return SDL_HasClipboardText() == SDL_TRUE; }
};

#endif // NOVELMIND_HAS_SDL2

/**
 * @brief Null clipboard implementation for environments without clipboard support
 */
class NullClipboard : public IClipboard {
public:
  NullClipboard() = default;
  ~NullClipboard() override = default;

  Result<void> setText(const std::string& /*text*/) override {
    NOVELMIND_LOG_WARN("Clipboard not available (no SDL2 support)");
    return Result<void>::error("Clipboard not available");
  }

  Result<std::string> getText() override {
    NOVELMIND_LOG_WARN("Clipboard not available (no SDL2 support)");
    return Result<std::string>::error("Clipboard not available");
  }

  bool hasText() override { return false; }
};

std::unique_ptr<IClipboard> createClipboard() {
#ifdef NOVELMIND_HAS_SDL2
  return std::make_unique<SDLClipboard>();
#else
  return std::make_unique<NullClipboard>();
#endif
}

} // namespace NovelMind::platform
