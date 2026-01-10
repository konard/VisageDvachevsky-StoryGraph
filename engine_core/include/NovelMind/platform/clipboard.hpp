#pragma once

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include <memory>
#include <string>

namespace NovelMind::platform {

/**
 * @brief Interface for clipboard operations
 *
 * Provides platform-independent clipboard access for copy/paste operations.
 * Implementations handle platform-specific clipboard APIs (SDL2, native OS APIs, etc.)
 */
class IClipboard {
public:
  virtual ~IClipboard() = default;

  /**
   * @brief Set text content to clipboard
   * @param text Text to copy to clipboard
   * @return Result indicating success or error message
   */
  [[nodiscard]] virtual Result<void> setText(const std::string& text) = 0;

  /**
   * @brief Get text content from clipboard
   * @return Result containing clipboard text or error message
   */
  [[nodiscard]] virtual Result<std::string> getText() = 0;

  /**
   * @brief Check if clipboard has text content
   * @return true if clipboard contains text, false otherwise
   */
  [[nodiscard]] virtual bool hasText() = 0;
};

/**
 * @brief Create clipboard instance for current platform
 *
 * Returns platform-specific clipboard implementation:
 * - SDL2 clipboard when SDL2 is available
 * - Null clipboard otherwise (no-op implementation)
 *
 * @return Unique pointer to clipboard interface
 */
std::unique_ptr<IClipboard> createClipboard();

} // namespace NovelMind::platform
