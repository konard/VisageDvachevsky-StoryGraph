#pragma once

/**
 * @file text_layout.hpp
 * @brief Text Layout Engine with RichText and inline commands
 *
 * This module provides comprehensive text layout functionality for
 * visual novels including:
 * - Auto-wrapping text to fit width
 * - RichText formatting (color, bold, italic)
 * - Inline commands ({w=0.2}, {color=#ff0000}, {speed=50})
 * - Text measurement and bounds calculation
 * - Typewriter effect support with pause markers
 */

#include "NovelMind/core/types.hpp"
#include "NovelMind/renderer/color.hpp"
#include "NovelMind/renderer/font.hpp"
#include <functional>
#include <optional>
#include <regex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace NovelMind::renderer {

/**
 * @brief Text alignment options
 */
enum class TextAlign : u8 { Left, Center, Right, Justify };

/**
 * @brief Text style for formatting
 */
struct TextStyle {
  Color color = Color::white();
  bool bold = false;
  bool italic = false;
  f32 size = 16.0f;
  f32 outlineWidth = 0.0f;
  Color outlineColor = Color::black();

  bool operator==(const TextStyle& other) const {
    return color == other.color && bold == other.bold && italic == other.italic &&
           size == other.size && outlineWidth == other.outlineWidth &&
           outlineColor == other.outlineColor;
  }
};

/**
 * @brief Inline command types for typewriter effects
 */
struct WaitCommand {
  f32 duration; // seconds to wait
};

struct SpeedCommand {
  f32 charsPerSecond; // typing speed
};

struct PauseCommand {
  // Wait for user input
};

struct ColorCommand {
  Color color;
};

struct ResetStyleCommand {
  // Reset to default style
};

struct ShakeCommand {
  f32 intensity;
  f32 duration;
};

struct WaveCommand {
  f32 amplitude;
  f32 frequency;
};

/**
 * @brief Inline command variant
 */
using InlineCommand = std::variant<WaitCommand, SpeedCommand, PauseCommand, ColorCommand,
                                   ResetStyleCommand, ShakeCommand, WaveCommand>;

/**
 * @brief Text segment - either text or an inline command
 */
struct TextSegment {
  std::string text;
  TextStyle style;
  std::optional<InlineCommand> command;
  bool isCommand() const { return command.has_value(); }
};

/**
 * @brief A single line of laid-out text
 */
struct TextLine {
  std::vector<TextSegment> segments;
  f32 width = 0.0f;
  f32 height = 0.0f;
  f32 baseline = 0.0f;
};

/**
 * @brief Result of text layout
 */
struct TextLayout {
  std::vector<TextLine> lines;
  f32 totalWidth = 0.0f;
  f32 totalHeight = 0.0f;
  i32 totalCharacters = 0;
  bool rightToLeft = false;
  std::vector<size_t> commandIndices; // Indices where commands occur in character stream
};

/**
 * @brief Typewriter state for animated text display
 */
struct TypewriterState {
  f32 currentCharIndex = 0.0f;
  f32 targetCharIndex = 0.0f;
  f32 charsPerSecond = 30.0f;
  f32 waitTimer = 0.0f;
  bool waitingForInput = false;
  bool complete = false;

  // Current effects
  f32 shakeTimer = 0.0f;
  f32 shakeIntensity = 0.0f;
  f32 waveTime = 0.0f;
  f32 waveAmplitude = 0.0f;
  f32 waveFrequency = 1.0f;
};

/**
 * @brief RichText parser for parsing formatted text
 */
class RichTextParser {
public:
  /**
   * @brief Parse a rich text string into segments
   * @param text The text to parse (may contain inline commands)
   * @param defaultStyle The default text style
   * @return Vector of text segments
   */
  [[nodiscard]] std::vector<TextSegment> parse(const std::string& text,
                                               const TextStyle& defaultStyle) const;

private:
  /**
   * @brief Parse a single inline command
   */
  [[nodiscard]] std::optional<InlineCommand> parseCommand(const std::string& commandStr) const;

  /**
   * @brief Parse color from hex string
   */
  [[nodiscard]] Color parseColor(const std::string& colorStr) const;
};

/**
 * @brief Text layout engine
 *
 * Example usage:
 * @code
 * TextLayoutEngine engine;
 * engine.setFont(font);
 * engine.setMaxWidth(400.0f);
 *
 * std::string text = "Hello {color=#ff0000}world{/color}!{w=0.5} How are you?";
 * TextLayout layout = engine.layout(text);
 *
 * for (const auto& line : layout.lines) {
 *     for (const auto& segment : line.segments) {
 *         renderer.drawText(segment.text, segment.style);
 *     }
 * }
 * @endcode
 */
class TextLayoutEngine {
public:
  TextLayoutEngine();

  /**
   * @brief Set the font to use for layout
   */
  void setFont(std::shared_ptr<Font> font);

  /**
   * @brief Provide a FontAtlas for accurate glyph metrics
   */
  void setFontAtlas(std::shared_ptr<FontAtlas> atlas);

  /**
   * @brief Set maximum width for text wrapping
   */
  void setMaxWidth(f32 width);

  /**
   * @brief Set line height multiplier
   */
  void setLineHeight(f32 height);

  /**
   * @brief Set text alignment
   */
  void setAlignment(TextAlign align);

  /**
   * @brief Enable right-to-left layout direction
   */
  void setRightToLeft(bool enabled);

  /**
   * @brief Set default text style
   */
  void setDefaultStyle(const TextStyle& style);

  /**
   * @brief Layout text with automatic wrapping and rich text parsing
   * @param text The text to layout (may contain inline commands)
   * @return The laid-out text structure
   */
  [[nodiscard]] TextLayout layout(const std::string& text) const;

  /**
   * @brief Measure text bounds without full layout
   */
  [[nodiscard]] std::pair<f32, f32> measureText(const std::string& text) const;

  /**
   * @brief Get character index at position
   * @param layout The text layout
   * @param x X position relative to layout origin
   * @param y Y position relative to layout origin
   * @return Character index, or -1 if not found
   */
  [[nodiscard]] i32 getCharacterAtPosition(const TextLayout& layout, f32 x, f32 y) const;

  /**
   * @brief Get position of character
   * @param layout The text layout
   * @param charIndex Character index
   * @return Position (x, y) of the character
   */
  [[nodiscard]] std::pair<f32, f32> getCharacterPosition(const TextLayout& layout,
                                                         i32 charIndex) const;

private:
  /**
   * @brief Break text into words for wrapping
   */
  [[nodiscard]] std::vector<std::string> breakIntoWords(const std::string& text) const;

  /**
   * @brief Measure a single character
   */
  [[nodiscard]] f32 measureChar(char c, const TextStyle& style) const;

  /**
   * @brief Measure a word
   */
  [[nodiscard]] f32 measureWord(const std::string& word, const TextStyle& style) const;

  std::shared_ptr<FontAtlas> m_fontAtlas;
  std::shared_ptr<Font> m_font;
  f32 m_maxWidth = 0.0f;
  f32 m_lineHeight = 1.2f;
  TextAlign m_alignment = TextAlign::Left;
  bool m_rightToLeft = false;
  TextStyle m_defaultStyle;
  RichTextParser m_parser;
};

/**
 * @brief Typewriter text animator
 *
 * Handles the typewriter effect for visual novel dialogue.
 *
 * Example usage:
 * @code
 * TypewriterAnimator animator;
 * animator.setLayout(layout);
 * animator.setSpeed(30.0f); // 30 chars per second
 *
 * while (!animator.isComplete()) {
 *     animator.update(deltaTime);
 *     i32 visibleChars = animator.getVisibleCharCount();
 *     // Render only visible characters
 * }
 * @endcode
 */
class TypewriterAnimator {
public:
  TypewriterAnimator();

  /**
   * @brief Set the text layout to animate
   */
  void setLayout(const TextLayout& layout);

  /**
   * @brief Set typing speed (characters per second)
   */
  void setSpeed(f32 charsPerSecond);

  /**
   * @brief Set punctuation pause multiplier
   */
  void setPunctuationPause(f32 multiplier);

  /**
   * @brief Start the animation
   */
  void start();

  /**
   * @brief Update the animation
   * @param deltaTime Time since last update
   */
  void update(f64 deltaTime);

  /**
   * @brief Skip to show all text immediately
   */
  void skipToEnd();

  /**
   * @brief Signal user input (for pause commands)
   */
  void continueFromPause();

  /**
   * @brief Get number of visible characters
   */
  [[nodiscard]] i32 getVisibleCharCount() const;

  /**
   * @brief Check if waiting for user input
   */
  [[nodiscard]] bool isWaitingForInput() const;

  /**
   * @brief Check if animation is complete
   */
  [[nodiscard]] bool isComplete() const;

  /**
   * @brief Get current typewriter state
   */
  [[nodiscard]] const TypewriterState& getState() const;

  /**
   * @brief Get current text style (after applying commands)
   */
  [[nodiscard]] const TextStyle& getCurrentStyle() const;

  /**
   * @brief Set callback for when a command is reached
   */
  void setCommandCallback(std::function<void(const InlineCommand&)> callback);

private:
  /**
   * @brief Process commands at current position
   */
  void processCommands();

  /**
   * @brief Get pause duration for punctuation
   */
  [[nodiscard]] f32 getPunctuationPause(char c) const;

  const TextLayout* m_layout = nullptr;
  TypewriterState m_state;
  TextStyle m_currentStyle;
  f32 m_punctuationPause = 3.0f; // Multiplier for pause at punctuation
  std::function<void(const InlineCommand&)> m_commandCallback;

  // Command tracking
  size_t m_nextCommandIndex = 0;
};

} // namespace NovelMind::renderer
