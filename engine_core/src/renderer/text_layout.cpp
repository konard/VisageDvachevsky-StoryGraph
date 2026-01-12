#include "NovelMind/renderer/text_layout.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>

namespace NovelMind::renderer {

// RichTextParser implementation

std::vector<TextSegment> RichTextParser::parse(const std::string& text,
                                               const TextStyle& defaultStyle) const {
  std::vector<TextSegment> segments;
  TextStyle currentStyle = defaultStyle;
  std::string currentText;
  size_t pos = 0;

  while (pos < text.length()) {
    // Check for inline command: {command=value} or {command}
    if (text[pos] == '{') {
      size_t endPos = text.find('}', pos);
      if (endPos != std::string::npos) {
        // Save current text segment if any
        if (!currentText.empty()) {
          TextSegment seg;
          seg.text = currentText;
          seg.style = currentStyle;
          segments.push_back(std::move(seg));
          currentText.clear();
        }

        // Extract command string
        std::string commandStr = text.substr(pos + 1, endPos - pos - 1);

        // Check for closing tag (e.g., {/color})
        if (!commandStr.empty() && commandStr[0] == '/') {
          // Handle closing tag - reset to default style
          std::string tagName = commandStr.substr(1);
          if (tagName == "color") {
            currentStyle.color = defaultStyle.color;
          } else if (tagName == "b" || tagName == "bold") {
            currentStyle.bold = false;
          } else if (tagName == "i" || tagName == "italic") {
            currentStyle.italic = false;
          }
        } else {
          // Parse command
          auto command = parseCommand(commandStr);
          if (command.has_value()) {
            // Add command segment
            TextSegment cmdSeg;
            cmdSeg.style = currentStyle;
            cmdSeg.command = command;
            segments.push_back(std::move(cmdSeg));

            // Apply style-changing commands
            if (std::holds_alternative<ColorCommand>(*command)) {
              currentStyle.color = std::get<ColorCommand>(*command).color;
            } else if (std::holds_alternative<ResetStyleCommand>(*command)) {
              currentStyle = defaultStyle;
            }
          } else {
            // Check for simple tags like {b}, {i}, {color=#xxx}
            if (commandStr == "b" || commandStr == "bold") {
              currentStyle.bold = true;
            } else if (commandStr == "i" || commandStr == "italic") {
              currentStyle.italic = true;
            } else if (commandStr.find("color=") == 0) {
              std::string colorStr = commandStr.substr(6);
              currentStyle.color = parseColor(colorStr);
            }
          }
        }

        pos = endPos + 1;
        continue;
      }
    }

    // Regular character
    currentText += text[pos];
    ++pos;
  }

  // Add remaining text
  if (!currentText.empty()) {
    TextSegment seg;
    seg.text = currentText;
    seg.style = currentStyle;
    segments.push_back(std::move(seg));
  }

  return segments;
}

std::optional<InlineCommand> RichTextParser::parseCommand(const std::string& commandStr) const {
  // Parse key=value format
  size_t eqPos = commandStr.find('=');
  std::string key, value;

  if (eqPos != std::string::npos) {
    key = commandStr.substr(0, eqPos);
    value = commandStr.substr(eqPos + 1);
  } else {
    key = commandStr;
  }

  // Convert to lowercase for comparison
  std::transform(key.begin(), key.end(), key.begin(),
                 [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });

  // Wait command: {w=0.5} or {wait=0.5}
  if (key == "w" || key == "wait") {
    WaitCommand cmd;
    cmd.duration = value.empty() ? 0.5f : std::stof(value);
    return cmd;
  }

  // Speed command: {speed=50} or {s=50}
  if (key == "speed" || key == "s") {
    SpeedCommand cmd;
    cmd.charsPerSecond = value.empty() ? 30.0f : std::stof(value);
    return cmd;
  }

  // Pause command: {p} or {pause}
  if (key == "p" || key == "pause") {
    return PauseCommand{};
  }

  // Color command: {c=#ff0000} or {color=#ff0000}
  if (key == "c" || key == "color") {
    ColorCommand cmd;
    cmd.color = parseColor(value);
    return cmd;
  }

  // Reset command: {reset} or {r}
  if (key == "reset" || key == "r") {
    return ResetStyleCommand{};
  }

  // Shake command: {shake=2.0,0.5}
  if (key == "shake") {
    ShakeCommand cmd;
    size_t commaPos = value.find(',');
    if (commaPos != std::string::npos) {
      cmd.intensity = std::stof(value.substr(0, commaPos));
      cmd.duration = std::stof(value.substr(commaPos + 1));
    } else {
      cmd.intensity = value.empty() ? 2.0f : std::stof(value);
      cmd.duration = 0.5f;
    }
    return cmd;
  }

  // Wave command: {wave=2.0,1.5}
  if (key == "wave") {
    WaveCommand cmd;
    size_t commaPos = value.find(',');
    if (commaPos != std::string::npos) {
      cmd.amplitude = std::stof(value.substr(0, commaPos));
      cmd.frequency = std::stof(value.substr(commaPos + 1));
    } else {
      cmd.amplitude = value.empty() ? 2.0f : std::stof(value);
      cmd.frequency = 1.5f;
    }
    return cmd;
  }

  return std::nullopt;
}

Color RichTextParser::parseColor(const std::string& colorStr) const {
  std::string s = colorStr;

  // Remove leading #
  if (!s.empty() && s[0] == '#') {
    s = s.substr(1);
  }

  // Parse hex color
  if (s.length() >= 6) {
    try {
      u32 hex = static_cast<u32>(std::stoul(s.substr(0, 6), nullptr, 16));
      u8 r = (hex >> 16) & 0xFF;
      u8 g = (hex >> 8) & 0xFF;
      u8 b = hex & 0xFF;
      u8 a = 255;

      // Check for alpha
      if (s.length() >= 8) {
        a = static_cast<u8>(std::stoul(s.substr(6, 2), nullptr, 16));
      }

      return Color(r, g, b, a);
    } catch (...) {
      return Color::white();
    }
  }

  // Short form: #rgb
  if (s.length() >= 3) {
    try {
      u8 r = static_cast<u8>(std::stoul(s.substr(0, 1), nullptr, 16) * 17);
      u8 g = static_cast<u8>(std::stoul(s.substr(1, 1), nullptr, 16) * 17);
      u8 b = static_cast<u8>(std::stoul(s.substr(2, 1), nullptr, 16) * 17);
      return Color(r, g, b, 255);
    } catch (...) {
      return Color::white();
    }
  }

  return Color::white();
}

// TextLayoutEngine implementation

TextLayoutEngine::TextLayoutEngine() = default;

void TextLayoutEngine::setFont(std::shared_ptr<Font> font) {
  m_font = std::move(font);
}

void TextLayoutEngine::setFontAtlas(std::shared_ptr<FontAtlas> atlas) {
  m_fontAtlas = std::move(atlas);
}

void TextLayoutEngine::setMaxWidth(f32 width) {
  m_maxWidth = width;
}

void TextLayoutEngine::setLineHeight(f32 height) {
  m_lineHeight = height;
}

void TextLayoutEngine::setAlignment(TextAlign align) {
  m_alignment = align;
}

void TextLayoutEngine::setRightToLeft(bool enabled) {
  m_rightToLeft = enabled;
}

void TextLayoutEngine::setDefaultStyle(const TextStyle& style) {
  m_defaultStyle = style;
}

TextLayout TextLayoutEngine::layout(const std::string& text) const {
  TextLayout result;

  // Parse rich text into segments
  auto segments = m_parser.parse(text, m_defaultStyle);

  TextLine currentLine;
  f32 lineWidth = 0.0f;
  f32 baseLineHeight = m_defaultStyle.size * m_lineHeight;
  if (m_fontAtlas && m_fontAtlas->isValid()) {
    baseLineHeight = static_cast<f32>(m_fontAtlas->getLineHeight());
  }
  f32 lineHeight = baseLineHeight;
  i32 charCount = 0;

  for (const auto& segment : segments) {
    if (segment.isCommand()) {
      // Add command segment to current line
      result.commandIndices.push_back(static_cast<size_t>(charCount));
      currentLine.segments.push_back(segment);
      continue;
    }

    // Process text segment
    const std::string& segText = segment.text;
    std::string currentWord;

    for (size_t i = 0; i < segText.length(); ++i) {
      char c = segText[i];

      if (c == '\n') {
        // Handle newline
        if (!currentWord.empty()) {
          TextSegment wordSeg;
          wordSeg.text = currentWord;
          wordSeg.style = segment.style;
          currentLine.segments.push_back(std::move(wordSeg));
          charCount += static_cast<i32>(currentWord.length());
          currentWord.clear();
        }

        // Finish current line
        currentLine.width = lineWidth;
        currentLine.height = lineHeight;
        result.lines.push_back(std::move(currentLine));
        result.totalHeight += lineHeight;
        result.totalWidth = std::max(result.totalWidth, lineWidth);

        // Start new line
        currentLine = TextLine{};
        lineWidth = 0.0f;
      } else if (std::isspace(c)) {
        // End of word
        if (!currentWord.empty()) {
          f32 wordWidth = measureWord(currentWord, segment.style);

          // Check if we need to wrap
          if (m_maxWidth > 0.0f && lineWidth + wordWidth > m_maxWidth && lineWidth > 0.0f) {
            // Wrap to next line
            currentLine.width = lineWidth;
            currentLine.height = lineHeight;
            result.lines.push_back(std::move(currentLine));
            result.totalHeight += lineHeight;
            result.totalWidth = std::max(result.totalWidth, lineWidth);

            currentLine = TextLine{};
            lineWidth = 0.0f;
          }

          TextSegment wordSeg;
          wordSeg.text = currentWord;
          wordSeg.style = segment.style;
          currentLine.segments.push_back(std::move(wordSeg));
          lineWidth += wordWidth;
          charCount += static_cast<i32>(currentWord.length());
          currentWord.clear();
        }

        // Add space
        f32 spaceWidth = measureChar(' ', segment.style);
        if (m_maxWidth <= 0.0f || lineWidth + spaceWidth <= m_maxWidth) {
          TextSegment spaceSeg;
          spaceSeg.text = " ";
          spaceSeg.style = segment.style;
          currentLine.segments.push_back(std::move(spaceSeg));
          lineWidth += spaceWidth;
          ++charCount;
        }
      } else {
        currentWord += c;
      }
    }

    // Handle remaining word
    if (!currentWord.empty()) {
      f32 wordWidth = measureWord(currentWord, segment.style);

      // Check if we need to wrap
      if (m_maxWidth > 0.0f && lineWidth + wordWidth > m_maxWidth && lineWidth > 0.0f) {
        currentLine.width = lineWidth;
        currentLine.height = lineHeight;
        result.lines.push_back(std::move(currentLine));
        result.totalHeight += lineHeight;
        result.totalWidth = std::max(result.totalWidth, lineWidth);

        currentLine = TextLine{};
        lineWidth = 0.0f;
      }

      TextSegment wordSeg;
      wordSeg.text = currentWord;
      wordSeg.style = segment.style;
      currentLine.segments.push_back(std::move(wordSeg));
      lineWidth += wordWidth;
      charCount += static_cast<i32>(currentWord.length());
    }
  }

  // Add last line
  if (!currentLine.segments.empty()) {
    currentLine.width = lineWidth;
    currentLine.height = lineHeight;
    result.lines.push_back(std::move(currentLine));
    result.totalHeight += lineHeight;
    result.totalWidth = std::max(result.totalWidth, lineWidth);
  }

  result.totalCharacters = charCount;
  result.rightToLeft = m_rightToLeft;
  return result;
}

std::pair<f32, f32> TextLayoutEngine::measureText(const std::string& text) const {
  auto layout = this->layout(text);
  return {layout.totalWidth, layout.totalHeight};
}

i32 TextLayoutEngine::getCharacterAtPosition(const TextLayout& layout, f32 x, f32 y) const {
  f32 currentY = 0.0f;
  i32 charIndex = 0;

  for (const auto& line : layout.lines) {
    if (y >= currentY && y < currentY + line.height) {
      // Found the line
      f32 currentX = layout.rightToLeft ? line.width : 0.0f;

      for (const auto& segment : line.segments) {
        if (segment.isCommand()) {
          continue;
        }

        for (char c : segment.text) {
          f32 charWidth = measureChar(c, segment.style);
          if (!layout.rightToLeft) {
            if (x >= currentX && x < currentX + charWidth) {
              return charIndex;
            }
            currentX += charWidth;
          } else {
            f32 nextX = currentX - charWidth;
            if (x >= nextX && x < currentX) {
              return charIndex;
            }
            currentX = nextX;
          }
          ++charIndex;
        }
      }

      return charIndex - 1;
    }

    currentY += line.height;

    // Count characters in line
    for (const auto& segment : line.segments) {
      if (!segment.isCommand()) {
        charIndex += static_cast<i32>(segment.text.length());
      }
    }
  }

  return -1;
}

std::pair<f32, f32> TextLayoutEngine::getCharacterPosition(const TextLayout& layout,
                                                           i32 targetIndex) const {
  f32 currentY = 0.0f;
  i32 charIndex = 0;

  for (const auto& line : layout.lines) {
    f32 currentX = layout.rightToLeft ? line.width : 0.0f;

    for (const auto& segment : line.segments) {
      if (segment.isCommand()) {
        continue;
      }

      for (char c : segment.text) {
        f32 charWidth = measureChar(c, segment.style);
        if (charIndex == targetIndex) {
          f32 charX = layout.rightToLeft ? (currentX - charWidth) : currentX;
          return {charX, currentY};
        }
        if (layout.rightToLeft) {
          currentX -= charWidth;
        } else {
          currentX += charWidth;
        }
        ++charIndex;
      }
    }

    currentY += line.height;
  }

  return {0.0f, currentY};
}

std::vector<std::string> TextLayoutEngine::breakIntoWords(const std::string& text) const {
  std::vector<std::string> words;
  std::string current;

  for (char c : text) {
    if (std::isspace(c)) {
      if (!current.empty()) {
        words.push_back(current);
        current.clear();
      }
    } else {
      current += c;
    }
  }

  if (!current.empty()) {
    words.push_back(current);
  }

  return words;
}

f32 TextLayoutEngine::measureChar(char c, const TextStyle& style) const {
  // Use atlas metrics when available for precise width
  if (m_fontAtlas && m_fontAtlas->isValid()) {
    if (const auto* glyph = m_fontAtlas->getGlyph(static_cast<unsigned char>(c))) {
      return glyph->advanceX;
    }
  }

  // Character width estimation using monospace approximation.
  // When font metrics are available, FreeType glyph advances are used.
  if (m_font) {
    // Monospace approximation: 0.6 * size
    return style.size * 0.6f;
  }

  // Fallback: estimate based on character
  if (std::isspace(c)) {
    return style.size * 0.25f;
  }

  // Wide characters
  static const char* wideChars = "WMQOCD";
  if (std::strchr(wideChars, std::toupper(c))) {
    return style.size * 0.7f;
  }

  // Narrow characters
  static const char* narrowChars = "iIlj1!|";
  if (std::strchr(narrowChars, c)) {
    return style.size * 0.3f;
  }

  return style.size * 0.5f;
}

f32 TextLayoutEngine::measureWord(const std::string& word, const TextStyle& style) const {
  f32 width = 0.0f;
  for (char c : word) {
    width += measureChar(c, style);
  }
  return width;
}

// TypewriterAnimator implementation

TypewriterAnimator::TypewriterAnimator() = default;

void TypewriterAnimator::setLayout(const TextLayout& layout) {
  m_layout = &layout;
  m_state = TypewriterState{};
  m_state.targetCharIndex = static_cast<f32>(layout.totalCharacters);
  m_nextCommandIndex = 0;
}

void TypewriterAnimator::setSpeed(f32 charsPerSecond) {
  m_state.charsPerSecond = charsPerSecond;
}

void TypewriterAnimator::setPunctuationPause(f32 multiplier) {
  m_punctuationPause = multiplier;
}

void TypewriterAnimator::start() {
  m_state.currentCharIndex = 0.0f;
  m_state.waitTimer = 0.0f;
  m_state.waitingForInput = false;
  m_state.complete = false;
  m_nextCommandIndex = 0;
  m_currentStyle = TextStyle{};
}

void TypewriterAnimator::update(f64 deltaTime) {
  if (!m_layout || m_state.complete) {
    return;
  }

  if (m_state.waitingForInput) {
    return;
  }

  // Handle wait timer
  if (m_state.waitTimer > 0.0f) {
    m_state.waitTimer -= static_cast<f32>(deltaTime);
    return;
  }

  // Update effects
  if (m_state.shakeTimer > 0.0f) {
    m_state.shakeTimer -= static_cast<f32>(deltaTime);
  }
  m_state.waveTime += static_cast<f32>(deltaTime);

  // Process any commands at current position
  processCommands();

  // Advance character index
  f32 advance = m_state.charsPerSecond * static_cast<f32>(deltaTime);
  m_state.currentCharIndex += advance;

  // Check for punctuation pause
  i32 currentChar = static_cast<i32>(m_state.currentCharIndex);
  if (currentChar > 0 && currentChar < m_layout->totalCharacters) {
    // Get the character at current position
    i32 charIndex = 0;
    for (const auto& line : m_layout->lines) {
      for (const auto& segment : line.segments) {
        if (segment.isCommand()) {
          continue;
        }

        for (char c : segment.text) {
          if (charIndex == currentChar - 1) {
            f32 pause = getPunctuationPause(c);
            if (pause > 0.0f) {
              m_state.waitTimer = pause;
            }
          }
          ++charIndex;
        }
      }
    }
  }

  // Check for completion
  if (m_state.currentCharIndex >= m_state.targetCharIndex) {
    m_state.currentCharIndex = m_state.targetCharIndex;
    m_state.complete = true;
  }
}

void TypewriterAnimator::skipToEnd() {
  if (m_state.waitingForInput) {
    continueFromPause();
    return;
  }

  m_state.currentCharIndex = m_state.targetCharIndex;
  m_state.complete = true;
}

void TypewriterAnimator::continueFromPause() {
  m_state.waitingForInput = false;
}

i32 TypewriterAnimator::getVisibleCharCount() const {
  return static_cast<i32>(m_state.currentCharIndex);
}

bool TypewriterAnimator::isWaitingForInput() const {
  return m_state.waitingForInput;
}

bool TypewriterAnimator::isComplete() const {
  return m_state.complete;
}

const TypewriterState& TypewriterAnimator::getState() const {
  return m_state;
}

const TextStyle& TypewriterAnimator::getCurrentStyle() const {
  return m_currentStyle;
}

void TypewriterAnimator::setCommandCallback(std::function<void(const InlineCommand&)> callback) {
  m_commandCallback = std::move(callback);
}

void TypewriterAnimator::processCommands() {
  if (!m_layout) {
    return;
  }

  // Check if we've reached a command
  while (m_nextCommandIndex < m_layout->commandIndices.size()) {
    size_t cmdCharIndex = m_layout->commandIndices[m_nextCommandIndex];
    if (cmdCharIndex > static_cast<size_t>(m_state.currentCharIndex)) {
      break;
    }

    // Find the command in the layout
    size_t cmdCount = 0;
    for (const auto& line : m_layout->lines) {
      for (const auto& segment : line.segments) {
        if (segment.isCommand()) {
          if (cmdCount == m_nextCommandIndex) {
            const auto& cmd = segment.command.value();

            // Handle command
            std::visit(
                [this](const auto& c) {
                  using T = std::decay_t<decltype(c)>;

                  if constexpr (std::is_same_v<T, WaitCommand>) {
                    m_state.waitTimer = c.duration;
                  } else if constexpr (std::is_same_v<T, SpeedCommand>) {
                    m_state.charsPerSecond = c.charsPerSecond;
                  } else if constexpr (std::is_same_v<T, PauseCommand>) {
                    m_state.waitingForInput = true;
                  } else if constexpr (std::is_same_v<T, ColorCommand>) {
                    m_currentStyle.color = c.color;
                  } else if constexpr (std::is_same_v<T, ResetStyleCommand>) {
                    m_currentStyle = TextStyle{};
                  } else if constexpr (std::is_same_v<T, ShakeCommand>) {
                    m_state.shakeIntensity = c.intensity;
                    m_state.shakeTimer = c.duration;
                  } else if constexpr (std::is_same_v<T, WaveCommand>) {
                    m_state.waveAmplitude = c.amplitude;
                    m_state.waveFrequency = c.frequency;
                  }
                },
                cmd);

            if (m_commandCallback) {
              m_commandCallback(cmd);
            }
          }
          ++cmdCount;
        }
      }
    }

    ++m_nextCommandIndex;
  }
}

f32 TypewriterAnimator::getPunctuationPause(char c) const {
  // Calculate pause duration based on punctuation
  switch (c) {
  case '.':
  case '!':
  case '?':
    return m_punctuationPause / m_state.charsPerSecond;

  case ',':
  case ';':
  case ':':
    return (m_punctuationPause * 0.5f) / m_state.charsPerSecond;

  case '-':
    return (m_punctuationPause * 0.25f) / m_state.charsPerSecond;

  default:
    return 0.0f;
  }
}

} // namespace NovelMind::renderer
