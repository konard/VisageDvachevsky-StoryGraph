#include "NovelMind/scripting/lexer.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <stdexcept>

namespace NovelMind::scripting {

// Safe wrappers for ctype functions to avoid undefined behavior with negative
// char values The standard ctype functions require input in range [0,
// UCHAR_MAX] or EOF
namespace {
inline bool safeIsDigit(unsigned char c) {
  return std::isdigit(c) != 0;
}

inline bool safeIsAlpha(unsigned char c) {
  return std::isalpha(c) != 0;
}

inline bool safeIsAlnum(unsigned char c) {
  return std::isalnum(c) != 0;
}

inline bool safeIsXdigit(unsigned char c) {
  return std::isxdigit(c) != 0;
}

// UTF-8 helper functions for Unicode identifier support
// Checks if a byte is a UTF-8 continuation byte (10xxxxxx)
inline bool isUtf8Continuation(unsigned char c) {
  return (c & 0xC0) == 0x80;
}

// Checks if a character starts a valid UTF-8 multibyte sequence
// Returns the number of bytes in the sequence (2-4), or 0 if not a valid start
inline int utf8SequenceLength(unsigned char c) {
  if ((c & 0xE0) == 0xC0)
    return 2; // 110xxxxx - 2-byte sequence
  if ((c & 0xF0) == 0xE0)
    return 3; // 1110xxxx - 3-byte sequence
  if ((c & 0xF8) == 0xF0)
    return 4; // 11110xxx - 4-byte sequence
  return 0;
}

// Decodes a UTF-8 code point from a string view starting at position pos
// Returns the Unicode code point and advances pos past the decoded character
// Returns 0 and doesn't advance pos on error
inline uint32_t decodeUtf8(std::string_view source, size_t& pos) {
  if (pos >= source.size())
    return 0;

  unsigned char c = static_cast<unsigned char>(source[pos]);

  // ASCII character
  if (c < 0x80) {
    return c;
  }

  size_t seqLen = static_cast<size_t>(utf8SequenceLength(c));
  if (seqLen == 0 || pos + seqLen > source.size()) {
    return 0; // Invalid sequence or truncated
  }

  // Verify continuation bytes before decoding
  for (size_t i = 1; i < seqLen; ++i) {
    if (!isUtf8Continuation(static_cast<unsigned char>(source[pos + i]))) {
      return 0; // Invalid continuation byte
    }
  }

  // Decode the code point
  uint32_t codePoint = 0;
  switch (seqLen) {
  case 2:
    codePoint = static_cast<uint32_t>(c & 0x1F) << 6;
    codePoint |= static_cast<uint32_t>(static_cast<unsigned char>(source[pos + 1]) & 0x3F);
    // Check for overlong encoding: 2-byte sequences must encode values >= 0x80
    if (codePoint < 0x80) {
      return 0; // Overlong encoding
    }
    break;
  case 3:
    codePoint = static_cast<uint32_t>(c & 0x0F) << 12;
    codePoint |= static_cast<uint32_t>(static_cast<unsigned char>(source[pos + 1]) & 0x3F) << 6;
    codePoint |= static_cast<uint32_t>(static_cast<unsigned char>(source[pos + 2]) & 0x3F);
    // Check for overlong encoding: 3-byte sequences must encode values >= 0x800
    if (codePoint < 0x800) {
      return 0; // Overlong encoding
    }
    // Check for UTF-16 surrogate pairs (U+D800 to U+DFFF) - invalid in UTF-8
    if (codePoint >= 0xD800 && codePoint <= 0xDFFF) {
      return 0; // Surrogate pair
    }
    break;
  case 4:
    codePoint = static_cast<uint32_t>(c & 0x07) << 18;
    codePoint |= static_cast<uint32_t>(static_cast<unsigned char>(source[pos + 1]) & 0x3F) << 12;
    codePoint |= static_cast<uint32_t>(static_cast<unsigned char>(source[pos + 2]) & 0x3F) << 6;
    codePoint |= static_cast<uint32_t>(static_cast<unsigned char>(source[pos + 3]) & 0x3F);
    // Check for overlong encoding: 4-byte sequences must encode values >= 0x10000
    if (codePoint < 0x10000) {
      return 0; // Overlong encoding
    }
    // Check maximum valid Unicode code point (U+10FFFF)
    if (codePoint > 0x10FFFF) {
      return 0; // Beyond valid Unicode range
    }
    break;
  default:
    return 0;
  }

  return codePoint;
}

// Checks if a Unicode code point is a valid identifier start character
// Supports Latin, Cyrillic, Greek, and other common alphabets
// Based on UAX #31 Unicode Identifier and Pattern Syntax
inline bool isUnicodeIdentifierStart(uint32_t codePoint) {
  // ASCII letters
  if ((codePoint >= 'A' && codePoint <= 'Z') || (codePoint >= 'a' && codePoint <= 'z')) {
    return true;
  }

  // Latin Extended-A, Extended-B, Extended Additional
  if (codePoint >= 0x00C0 && codePoint <= 0x024F)
    return true;
  // Latin Extended Additional
  if (codePoint >= 0x1E00 && codePoint <= 0x1EFF)
    return true;

  // Greek and Coptic
  if (codePoint >= 0x0370 && codePoint <= 0x03FF)
    return true;
  // Greek Extended
  if (codePoint >= 0x1F00 && codePoint <= 0x1FFF)
    return true;

  // Cyrillic (Russian, Ukrainian, etc.)
  if (codePoint >= 0x0400 && codePoint <= 0x04FF)
    return true;
  // Cyrillic Supplement
  if (codePoint >= 0x0500 && codePoint <= 0x052F)
    return true;
  // Cyrillic Extended-A
  if (codePoint >= 0x2DE0 && codePoint <= 0x2DFF)
    return true;
  // Cyrillic Extended-B
  if (codePoint >= 0xA640 && codePoint <= 0xA69F)
    return true;

  // Armenian
  if (codePoint >= 0x0530 && codePoint <= 0x058F)
    return true;

  // Hebrew
  if (codePoint >= 0x0590 && codePoint <= 0x05FF)
    return true;

  // Arabic
  if (codePoint >= 0x0600 && codePoint <= 0x06FF)
    return true;
  // Arabic Supplement
  if (codePoint >= 0x0750 && codePoint <= 0x077F)
    return true;
  // Arabic Extended-A
  if (codePoint >= 0x08A0 && codePoint <= 0x08FF)
    return true;

  // Devanagari (Hindi, Sanskrit, etc.)
  if (codePoint >= 0x0900 && codePoint <= 0x097F)
    return true;
  // Bengali
  if (codePoint >= 0x0980 && codePoint <= 0x09FF)
    return true;
  // Gurmukhi (Punjabi)
  if (codePoint >= 0x0A00 && codePoint <= 0x0A7F)
    return true;
  // Gujarati
  if (codePoint >= 0x0A80 && codePoint <= 0x0AFF)
    return true;
  // Oriya
  if (codePoint >= 0x0B00 && codePoint <= 0x0B7F)
    return true;
  // Tamil
  if (codePoint >= 0x0B80 && codePoint <= 0x0BFF)
    return true;
  // Telugu
  if (codePoint >= 0x0C00 && codePoint <= 0x0C7F)
    return true;
  // Kannada
  if (codePoint >= 0x0C80 && codePoint <= 0x0CFF)
    return true;
  // Malayalam
  if (codePoint >= 0x0D00 && codePoint <= 0x0D7F)
    return true;
  // Sinhala
  if (codePoint >= 0x0D80 && codePoint <= 0x0DFF)
    return true;
  // Thai
  if (codePoint >= 0x0E00 && codePoint <= 0x0E7F)
    return true;
  // Lao
  if (codePoint >= 0x0E80 && codePoint <= 0x0EFF)
    return true;
  // Tibetan
  if (codePoint >= 0x0F00 && codePoint <= 0x0FFF)
    return true;

  // Georgian
  if (codePoint >= 0x10A0 && codePoint <= 0x10FF)
    return true;
  // Hangul Jamo (Korean alphabet components)
  if (codePoint >= 0x1100 && codePoint <= 0x11FF)
    return true;
  // Ethiopic
  if (codePoint >= 0x1200 && codePoint <= 0x137F)
    return true;
  // Cherokee
  if (codePoint >= 0x13A0 && codePoint <= 0x13FF)
    return true;
  // Unified Canadian Aboriginal Syllabics
  if (codePoint >= 0x1400 && codePoint <= 0x167F)
    return true;
  // Ogham
  if (codePoint >= 0x1680 && codePoint <= 0x169F)
    return true;
  // Runic
  if (codePoint >= 0x16A0 && codePoint <= 0x16FF)
    return true;
  // Tagalog (Baybayin)
  if (codePoint >= 0x1700 && codePoint <= 0x171F)
    return true;
  // Hanunoo
  if (codePoint >= 0x1720 && codePoint <= 0x173F)
    return true;
  // Buhid
  if (codePoint >= 0x1740 && codePoint <= 0x175F)
    return true;
  // Tagbanwa
  if (codePoint >= 0x1760 && codePoint <= 0x177F)
    return true;
  // Khmer
  if (codePoint >= 0x1780 && codePoint <= 0x17FF)
    return true;
  // Mongolian
  if (codePoint >= 0x1800 && codePoint <= 0x18AF)
    return true;

  // Bopomofo (Chinese phonetic symbols)
  if (codePoint >= 0x3100 && codePoint <= 0x312F)
    return true;
  // Hangul Compatibility Jamo
  if (codePoint >= 0x3130 && codePoint <= 0x318F)
    return true;
  // Bopomofo Extended
  if (codePoint >= 0x31A0 && codePoint <= 0x31BF)
    return true;

  // Hiragana
  if (codePoint >= 0x3040 && codePoint <= 0x309F)
    return true;
  // Katakana
  if (codePoint >= 0x30A0 && codePoint <= 0x30FF)
    return true;

  // CJK Unified Ideographs Extension A
  if (codePoint >= 0x3400 && codePoint <= 0x4DBF)
    return true;
  // CJK Unified Ideographs (Chinese, Japanese Kanji)
  if (codePoint >= 0x4E00 && codePoint <= 0x9FFF)
    return true;

  // Yi Syllables
  if (codePoint >= 0xA000 && codePoint <= 0xA48F)
    return true;
  // Yi Radicals
  if (codePoint >= 0xA490 && codePoint <= 0xA4CF)
    return true;

  // Hangul Jamo Extended-A
  if (codePoint >= 0xA960 && codePoint <= 0xA97F)
    return true;

  // Hangul Syllables (Korean)
  if (codePoint >= 0xAC00 && codePoint <= 0xD7AF)
    return true;
  // Hangul Jamo Extended-B
  if (codePoint >= 0xD7B0 && codePoint <= 0xD7FF)
    return true;

  // CJK Compatibility Ideographs
  if (codePoint >= 0xF900 && codePoint <= 0xFAFF)
    return true;

  return false;
}

// Checks if a Unicode code point is valid within an identifier (after start)
// Based on UAX #31 ID_Continue property
inline bool isUnicodeIdentifierPart(uint32_t codePoint) {
  // All identifier start chars are also valid parts
  if (isUnicodeIdentifierStart(codePoint))
    return true;

  // ASCII digits
  if (codePoint >= '0' && codePoint <= '9')
    return true;

  // Combining Diacritical Marks (accents, etc.)
  if (codePoint >= 0x0300 && codePoint <= 0x036F)
    return true;

  // Arabic combining marks
  if (codePoint >= 0x0610 && codePoint <= 0x061A)
    return true;
  if (codePoint >= 0x064B && codePoint <= 0x065F)
    return true;
  if (codePoint >= 0x0670 && codePoint <= 0x0670)
    return true;

  // Devanagari combining marks
  if (codePoint >= 0x0900 && codePoint <= 0x0903)
    return true;
  if (codePoint >= 0x093A && codePoint <= 0x093C)
    return true;
  if (codePoint >= 0x093E && codePoint <= 0x094F)
    return true;
  if (codePoint >= 0x0951 && codePoint <= 0x0957)
    return true;

  // Bengali combining marks
  if (codePoint >= 0x0981 && codePoint <= 0x0983)
    return true;
  if (codePoint >= 0x09BC && codePoint <= 0x09BC)
    return true;
  if (codePoint >= 0x09BE && codePoint <= 0x09C4)
    return true;
  if (codePoint >= 0x09C7 && codePoint <= 0x09C8)
    return true;
  if (codePoint >= 0x09CB && codePoint <= 0x09CD)
    return true;

  // Thai combining marks
  if (codePoint >= 0x0E31 && codePoint <= 0x0E31)
    return true;
  if (codePoint >= 0x0E34 && codePoint <= 0x0E3A)
    return true;
  if (codePoint >= 0x0E47 && codePoint <= 0x0E4E)
    return true;

  // Connector punctuation (Pc category)
  // Underscore is already handled in ASCII check
  if (codePoint == 0x203F || codePoint == 0x2040)  // undertie, character tie
    return true;
  if (codePoint == 0x2054)  // inverted undertie
    return true;
  if (codePoint >= 0xFE33 && codePoint <= 0xFE34)  // presentation form for vertical low line
    return true;
  if (codePoint >= 0xFE4D && codePoint <= 0xFE4F)  // dashed low line
    return true;
  if (codePoint == 0xFF3F)  // fullwidth low line
    return true;

  // Non-ASCII decimal digits (Nd category)
  // Arabic-Indic digits
  if (codePoint >= 0x0660 && codePoint <= 0x0669)
    return true;
  // Extended Arabic-Indic digits
  if (codePoint >= 0x06F0 && codePoint <= 0x06F9)
    return true;
  // Devanagari digits
  if (codePoint >= 0x0966 && codePoint <= 0x096F)
    return true;
  // Bengali digits
  if (codePoint >= 0x09E6 && codePoint <= 0x09EF)
    return true;
  // Gurmukhi digits
  if (codePoint >= 0x0A66 && codePoint <= 0x0A6F)
    return true;
  // Gujarati digits
  if (codePoint >= 0x0AE6 && codePoint <= 0x0AEF)
    return true;
  // Oriya digits
  if (codePoint >= 0x0B66 && codePoint <= 0x0B6F)
    return true;
  // Tamil digits
  if (codePoint >= 0x0BE6 && codePoint <= 0x0BEF)
    return true;
  // Telugu digits
  if (codePoint >= 0x0C66 && codePoint <= 0x0C6F)
    return true;
  // Kannada digits
  if (codePoint >= 0x0CE6 && codePoint <= 0x0CEF)
    return true;
  // Malayalam digits
  if (codePoint >= 0x0D66 && codePoint <= 0x0D6F)
    return true;
  // Thai digits
  if (codePoint >= 0x0E50 && codePoint <= 0x0E59)
    return true;
  // Lao digits
  if (codePoint >= 0x0ED0 && codePoint <= 0x0ED9)
    return true;
  // Tibetan digits
  if (codePoint >= 0x0F20 && codePoint <= 0x0F29)
    return true;
  // Mongolian digits
  if (codePoint >= 0x1810 && codePoint <= 0x1819)
    return true;
  // Khmer digits
  if (codePoint >= 0x17E0 && codePoint <= 0x17E9)
    return true;

  return false;
}
} // anonymous namespace

Lexer::Lexer() : m_source(), m_start(0), m_current(0), m_line(1), m_column(1), m_startColumn(1) {
  initKeywords();
}

Lexer::~Lexer() = default;

void Lexer::initKeywords() {
  m_keywords["character"] = TokenType::Character;
  m_keywords["scene"] = TokenType::Scene;
  m_keywords["show"] = TokenType::Show;
  m_keywords["hide"] = TokenType::Hide;
  m_keywords["say"] = TokenType::Say;
  m_keywords["choice"] = TokenType::Choice;
  m_keywords["if"] = TokenType::If;
  m_keywords["else"] = TokenType::Else;
  m_keywords["goto"] = TokenType::Goto;
  m_keywords["wait"] = TokenType::Wait;
  m_keywords["play"] = TokenType::Play;
  m_keywords["stop"] = TokenType::Stop;
  m_keywords["set"] = TokenType::Set;
  m_keywords["true"] = TokenType::True;
  m_keywords["false"] = TokenType::False;
  m_keywords["at"] = TokenType::At;
  m_keywords["and"] = TokenType::And;
  m_keywords["or"] = TokenType::Or;
  m_keywords["not"] = TokenType::Not;
  m_keywords["background"] = TokenType::Background;
  m_keywords["music"] = TokenType::Music;
  m_keywords["sound"] = TokenType::Sound;
  m_keywords["transition"] = TokenType::Transition;
  m_keywords["fade"] = TokenType::Fade;
  m_keywords["move"] = TokenType::Move;
  m_keywords["to"] = TokenType::To;
  m_keywords["duration"] = TokenType::Duration;
}

void Lexer::reset() {
  m_source = {};
  m_start = 0;
  m_current = 0;
  m_line = 1;
  m_column = 1;
  m_startColumn = 1;
  m_errors.clear();
}

Result<std::vector<Token>> Lexer::tokenize(std::string_view source) {
  reset();
  m_source = source;

  std::vector<Token> tokens;
  tokens.reserve(source.size() / 4); // Rough estimate

  while (!isAtEnd()) {
    m_start = m_current;
    m_startColumn = m_column;

    Token token = scanToken();

    if (token.type == TokenType::Error) {
      m_errors.emplace_back(token.lexeme, token.location);
    } else if (token.type != TokenType::Newline) {
      // Skip newlines in token stream (optional: keep for statement separation)
      tokens.push_back(std::move(token));
    }
  }

  // Add end-of-file token
  tokens.emplace_back(TokenType::EndOfFile, "", SourceLocation(m_line, m_column));

  if (!m_errors.empty()) {
    return Result<std::vector<Token>>::error(m_errors[0].message);
  }

  return Result<std::vector<Token>>::ok(std::move(tokens));
}

const std::vector<LexerError>& Lexer::getErrors() const {
  return m_errors;
}

bool Lexer::isAtEnd() const {
  return m_current >= m_source.size();
}

unsigned char Lexer::peek() const {
  if (isAtEnd())
    return '\0';
  // Cast to unsigned char to avoid sign extension issues on platforms
  // where char is signed. This ensures bytes with high bit set (128-255)
  // are handled correctly, which is essential for UTF-8 support.
  return static_cast<unsigned char>(m_source[m_current]);
}

unsigned char Lexer::peekNext() const {
  if (m_current + 1 >= m_source.size())
    return '\0';
  // Cast to unsigned char to avoid sign extension issues
  return static_cast<unsigned char>(m_source[m_current + 1]);
}

unsigned char Lexer::advance() {
  // Cast to unsigned char to avoid sign extension issues on platforms
  // where char is signed. This ensures correct handling of UTF-8 and
  // other byte values >= 128.
  unsigned char c = static_cast<unsigned char>(m_source[m_current++]);
  if (c == '\n') {
    ++m_line;
    m_column = 1;
  } else {
    ++m_column;
  }
  return c;
}

bool Lexer::match(char expected) {
  if (isAtEnd())
    return false;
  if (m_source[m_current] != expected)
    return false;
  advance();
  return true;
}

void Lexer::skipWhitespace() {
  while (!isAtEnd()) {
    unsigned char c = peek();
    switch (c) {
    case ' ':
    case '\r':
    case '\t':
      advance();
      break;
    default:
      return;
    }
  }
}

void Lexer::skipLineComment() {
  // Skip until end of line
  while (!isAtEnd() && peek() != '\n') {
    advance();
  }
}

void Lexer::skipBlockComment() {
  // Skip /* ... */
  // Maximum nesting depth to prevent stack overflow from pathological input
  constexpr int MAX_COMMENT_DEPTH = 128;

  int depth = 1;
  u32 startLine = m_line;

  while (!isAtEnd() && depth > 0) {
    if (peek() == '/' && peekNext() == '*') {
      advance();
      advance();
      ++depth;

      // Check if we've exceeded the maximum nesting depth
      if (depth > MAX_COMMENT_DEPTH) {
        m_errors.emplace_back(
          "Comment nesting depth exceeds limit of " + std::to_string(MAX_COMMENT_DEPTH) +
          " (starting at line " + std::to_string(startLine) + ")",
          SourceLocation(m_line, m_column));
        return;
      }
    } else if (peek() == '*' && peekNext() == '/') {
      advance();
      advance();
      --depth;
    } else {
      advance();
    }
  }

  // Check if comment was properly closed
  if (depth > 0) {
    m_errors.emplace_back("Unclosed block comment starting at line " + std::to_string(startLine),
                          SourceLocation(startLine, 1));
  }
}

Token Lexer::scanToken() {
  // Use iteration instead of recursion to avoid stack overflow with many comments
  while (true) {
    skipWhitespace();

    m_start = m_current;
    m_startColumn = m_column;

    if (isAtEnd()) {
      return makeToken(TokenType::EndOfFile);
    }

    unsigned char c = advance();

    // Handle newlines
    if (c == '\n') {
      return makeToken(TokenType::Newline);
    }

    // Handle comments
    if (c == '/') {
      if (match('/')) {
        skipLineComment();
        continue; // Continue to next token
      }
      if (match('*')) {
        skipBlockComment();
        // Check if we hit an unclosed comment error
        if (!m_errors.empty()) {
          return errorToken(m_errors.back().message);
        }
        continue; // Continue to next token
      }
      return makeToken(TokenType::Slash);
    }

    // Handle numbers
    if (safeIsDigit(c)) {
      return scanNumber();
    }

    // Handle identifiers and keywords (ASCII)
    if (safeIsAlpha(c) || c == '_') {
      return scanIdentifier();
    }

    // Handle Unicode identifiers (Cyrillic, Greek, CJK, etc.)
    // Check if this is a UTF-8 multibyte character that could start an identifier
    if (c >= 0x80) {
      // Rewind to re-examine this character as UTF-8
      --m_current;
      --m_column;
      size_t checkPos = m_current;
      uint32_t codePoint = decodeUtf8(m_source, checkPos);
      if (codePoint != 0 && isUnicodeIdentifierStart(codePoint)) {
        return scanIdentifier();
      }
      // Not a valid identifier start - advance past this char and continue
      advance();
    }

    // Handle strings
    if (c == '"') {
      return scanString();
    }

    // Handle color literals
    if (c == '#') {
      if (safeIsXdigit(peek())) {
        return scanColorLiteral();
      }
      return makeToken(TokenType::Hash);
    }

    // Handle operators and delimiters
    switch (c) {
    case '(':
      return makeToken(TokenType::LeftParen);
    case ')':
      return makeToken(TokenType::RightParen);
    case '{':
      return makeToken(TokenType::LeftBrace);
    case '}':
      return makeToken(TokenType::RightBrace);
    case '[':
      return makeToken(TokenType::LeftBracket);
    case ']':
      return makeToken(TokenType::RightBracket);
    case ',':
      return makeToken(TokenType::Comma);
    case ':':
      return makeToken(TokenType::Colon);
    case ';':
      return makeToken(TokenType::Semicolon);
    case '.':
      return makeToken(TokenType::Dot);
    case '+':
      return makeToken(TokenType::Plus);
    case '*':
      return makeToken(TokenType::Star);
    case '%':
      return makeToken(TokenType::Percent);

    case '-':
      if (match('>')) {
        return makeToken(TokenType::Arrow);
      }
      return makeToken(TokenType::Minus);

    case '=':
      if (match('=')) {
        return makeToken(TokenType::Equal);
      }
      return makeToken(TokenType::Assign);

    case '!':
      if (match('=')) {
        return makeToken(TokenType::NotEqual);
      }
      return errorToken("Unexpected character '!'");

    case '<':
      if (match('=')) {
        return makeToken(TokenType::LessEqual);
      }
      return makeToken(TokenType::Less);

    case '>':
      if (match('=')) {
        return makeToken(TokenType::GreaterEqual);
      }
      return makeToken(TokenType::Greater);
    }

    return errorToken("Unexpected character");
  } // end while (true)
}

Token Lexer::makeToken(TokenType type) {
  std::string lexeme(m_source.substr(m_start, m_current - m_start));
  return Token(type, std::move(lexeme), SourceLocation(m_line, m_startColumn));
}

Token Lexer::makeToken(TokenType type, const std::string& lexeme) {
  return Token(type, lexeme, SourceLocation(m_line, m_startColumn));
}

Token Lexer::errorToken(const std::string& message) {
  return Token(TokenType::Error, message, SourceLocation(m_line, m_startColumn));
}

Token Lexer::scanString() {
  std::string value;

  while (!isAtEnd() && peek() != '"') {
    if (peek() == '\n') {
      return errorToken("Unterminated string (newline in string literal)");
    }

    if (peek() == '\\') {
      advance(); // Skip backslash
      if (isAtEnd()) {
        return errorToken("Unterminated string (escape at end)");
      }

      char escaped = advance();
      switch (escaped) {
      case 'n':
        value += '\n';
        break;
      case 'r':
        value += '\r';
        break;
      case 't':
        value += '\t';
        break;
      case '\\':
        value += '\\';
        break;
      case '"':
        value += '"';
        break;
      default:
        return errorToken("Invalid escape sequence");
      }
    } else {
      value += advance();
    }
  }

  if (isAtEnd()) {
    return errorToken("Unterminated string");
  }

  advance(); // Closing quote

  Token token(TokenType::String, std::move(value), SourceLocation(m_line, m_startColumn));
  return token;
}

Token Lexer::scanNumber() {
  // Scan integer part
  while (!isAtEnd() && safeIsDigit(peek())) {
    advance();
  }

  // Check for decimal part
  bool isFloat = false;
  if (peek() == '.' && safeIsDigit(peekNext())) {
    isFloat = true;
    advance(); // Consume '.'

    while (!isAtEnd() && safeIsDigit(peek())) {
      advance();
    }
  }

  std::string lexeme(m_source.substr(m_start, m_current - m_start));
  Token token(isFloat ? TokenType::Float : TokenType::Integer, lexeme,
              SourceLocation(m_line, m_startColumn));

  try {
    if (isFloat) {
      token.floatValue = std::stof(lexeme);
    } else {
      token.intValue = std::stoi(lexeme);
    }
  } catch (const std::out_of_range& e) {
    return errorToken("Number literal out of range: " + lexeme);
  } catch (const std::invalid_argument& e) {
    return errorToken("Invalid number literal: " + lexeme);
  }

  return token;
}

Token Lexer::scanIdentifier() {
  // Scan both ASCII and Unicode identifier characters
  while (!isAtEnd()) {
    unsigned char c = peek();

    // ASCII alphanumeric or underscore
    if (safeIsAlnum(c) || c == '_') {
      advance();
      continue;
    }

    // Check for UTF-8 multibyte character
    if (c >= 0x80) {
      size_t checkPos = m_current;
      uint32_t codePoint = decodeUtf8(m_source, checkPos);
      if (codePoint != 0 && isUnicodeIdentifierPart(codePoint)) {
        // Advance past all bytes of this UTF-8 character
        size_t bytesToAdvance = checkPos - m_current;
        for (size_t i = 0; i < bytesToAdvance; ++i) {
          advance();
        }
        continue;
      }
    }

    // Not a valid identifier character, stop scanning
    break;
  }

  std::string lexeme(m_source.substr(m_start, m_current - m_start));
  TokenType type = identifierType(lexeme);

  return Token(type, std::move(lexeme), SourceLocation(m_line, m_startColumn));
}

Token Lexer::scanColorLiteral() {
  // Already consumed '#', now read hex digits
  while (!isAtEnd() && safeIsXdigit(peek())) {
    advance();
  }

  std::string lexeme(m_source.substr(m_start, m_current - m_start));

  // Validate color format: #RGB (3 hex digits), #RRGGBB (6 hex digits), #RRGGBBAA (8 hex digits)
  size_t hexLen = lexeme.size() - 1; // Exclude '#'
  if (hexLen == 0) {
    return errorToken("Color literal must contain hex digits after '#'");
  } else if (hexLen == 3) {
    // Valid: #RGB format
  } else if (hexLen == 6) {
    // Valid: #RRGGBB format
  } else if (hexLen == 8) {
    // Valid: #RRGGBBAA format with alpha channel
  } else {
    // Invalid length - provide clear error message based on what was provided
    if (hexLen < 3) {
      return errorToken("Color literal too short. Expected #RGB (3 hex digits), #RRGGBB (6 hex digits), or #RRGGBBAA (8 hex digits)");
    } else if (hexLen == 4 || hexLen == 5) {
      return errorToken("Invalid color literal length. Expected #RGB (3 hex digits), #RRGGBB (6 hex digits), or #RRGGBBAA (8 hex digits)");
    } else {
      return errorToken("Color literal too long. Expected #RGB (3 hex digits), #RRGGBB (6 hex digits), or #RRGGBBAA (8 hex digits)");
    }
  }

  return Token(TokenType::String, std::move(lexeme), SourceLocation(m_line, m_startColumn));
}

TokenType Lexer::identifierType(const std::string& lexeme) const {
  auto it = m_keywords.find(lexeme);
  if (it != m_keywords.end()) {
    return it->second;
  }
  return TokenType::Identifier;
}

} // namespace NovelMind::scripting
