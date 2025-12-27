#include "NovelMind/scripting/lexer.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>

namespace NovelMind::scripting {

// Safe wrappers for ctype functions to avoid undefined behavior with negative
// char values The standard ctype functions require input in range [0,
// UCHAR_MAX] or EOF
namespace {
inline bool safeIsDigit(char c) {
  return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

inline bool safeIsAlpha(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) != 0;
}

inline bool safeIsAlnum(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) != 0;
}

inline bool safeIsXdigit(char c) {
  return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

// UTF-8 helper functions for Unicode identifier support
// Checks if a byte is a UTF-8 continuation byte (10xxxxxx)
inline bool isUtf8Continuation(unsigned char c) { return (c & 0xC0) == 0x80; }

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
inline uint32_t decodeUtf8(std::string_view source, size_t &pos) {
  if (pos >= source.size())
    return 0;

  unsigned char c = static_cast<unsigned char>(source[pos]);

  // ASCII character
  if (c < 0x80) {
    return c;
  }

  int seqLen = utf8SequenceLength(c);
  if (seqLen == 0 || pos + seqLen > source.size()) {
    return 0; // Invalid sequence
  }

  // Verify continuation bytes
  for (int i = 1; i < seqLen; ++i) {
    if (!isUtf8Continuation(static_cast<unsigned char>(source[pos + i]))) {
      return 0; // Invalid continuation byte
    }
  }

  // Decode the code point
  uint32_t codePoint = 0;
  switch (seqLen) {
  case 2:
    codePoint = (c & 0x1F) << 6;
    codePoint |= (static_cast<unsigned char>(source[pos + 1]) & 0x3F);
    break;
  case 3:
    codePoint = (c & 0x0F) << 12;
    codePoint |= (static_cast<unsigned char>(source[pos + 1]) & 0x3F) << 6;
    codePoint |= (static_cast<unsigned char>(source[pos + 2]) & 0x3F);
    break;
  case 4:
    codePoint = (c & 0x07) << 18;
    codePoint |= (static_cast<unsigned char>(source[pos + 1]) & 0x3F) << 12;
    codePoint |= (static_cast<unsigned char>(source[pos + 2]) & 0x3F) << 6;
    codePoint |= (static_cast<unsigned char>(source[pos + 3]) & 0x3F);
    break;
  default:
    return 0;
  }

  return codePoint;
}

// Checks if a Unicode code point is a valid identifier start character
// Supports Latin, Cyrillic, Greek, and other common alphabets
inline bool isUnicodeIdentifierStart(uint32_t codePoint) {
  // ASCII letters
  if ((codePoint >= 'A' && codePoint <= 'Z') ||
      (codePoint >= 'a' && codePoint <= 'z')) {
    return true;
  }
  // Latin Extended-A, Extended-B, Extended Additional
  if (codePoint >= 0x00C0 && codePoint <= 0x024F)
    return true;
  // Cyrillic (Russian, Ukrainian, etc.)
  if (codePoint >= 0x0400 && codePoint <= 0x04FF)
    return true;
  // Cyrillic Supplement
  if (codePoint >= 0x0500 && codePoint <= 0x052F)
    return true;
  // Greek
  if (codePoint >= 0x0370 && codePoint <= 0x03FF)
    return true;
  // CJK Unified Ideographs (Chinese, Japanese Kanji)
  if (codePoint >= 0x4E00 && codePoint <= 0x9FFF)
    return true;
  // Hiragana
  if (codePoint >= 0x3040 && codePoint <= 0x309F)
    return true;
  // Katakana
  if (codePoint >= 0x30A0 && codePoint <= 0x30FF)
    return true;
  // Korean Hangul
  if (codePoint >= 0xAC00 && codePoint <= 0xD7AF)
    return true;
  // Arabic
  if (codePoint >= 0x0600 && codePoint <= 0x06FF)
    return true;
  // Hebrew
  if (codePoint >= 0x0590 && codePoint <= 0x05FF)
    return true;

  return false;
}

// Checks if a Unicode code point is valid within an identifier (after start)
inline bool isUnicodeIdentifierPart(uint32_t codePoint) {
  // All identifier start chars are also valid parts
  if (isUnicodeIdentifierStart(codePoint))
    return true;
  // ASCII digits
  if (codePoint >= '0' && codePoint <= '9')
    return true;
  // Unicode combining marks (accents, etc.)
  if (codePoint >= 0x0300 && codePoint <= 0x036F)
    return true;

  return false;
}
} // anonymous namespace

Lexer::Lexer()
    : m_source(), m_start(0), m_current(0), m_line(1), m_column(1),
      m_startColumn(1) {
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
  tokens.emplace_back(TokenType::EndOfFile, "",
                      SourceLocation(m_line, m_column));

  if (!m_errors.empty()) {
    return Result<std::vector<Token>>::error(m_errors[0].message);
  }

  return Result<std::vector<Token>>::ok(std::move(tokens));
}

const std::vector<LexerError> &Lexer::getErrors() const { return m_errors; }

bool Lexer::isAtEnd() const { return m_current >= m_source.size(); }

char Lexer::peek() const {
  if (isAtEnd())
    return '\0';
  return m_source[m_current];
}

char Lexer::peekNext() const {
  if (m_current + 1 >= m_source.size())
    return '\0';
  return m_source[m_current + 1];
}

char Lexer::advance() {
  char c = m_source[m_current++];
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
    char c = peek();
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
  int depth = 1;
  while (!isAtEnd() && depth > 0) {
    if (peek() == '/' && peekNext() == '*') {
      advance();
      advance();
      ++depth;
    } else if (peek() == '*' && peekNext() == '/') {
      advance();
      advance();
      --depth;
    } else {
      advance();
    }
  }
}

Token Lexer::scanToken() {
  skipWhitespace();

  m_start = m_current;
  m_startColumn = m_column;

  if (isAtEnd()) {
    return makeToken(TokenType::EndOfFile);
  }

  char c = advance();

  // Handle newlines
  if (c == '\n') {
    return makeToken(TokenType::Newline);
  }

  // Handle comments
  if (c == '/') {
    if (match('/')) {
      skipLineComment();
      return scanToken(); // Recursively get next token
    }
    if (match('*')) {
      skipBlockComment();
      return scanToken();
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
  unsigned char uc = static_cast<unsigned char>(c);
  if (uc >= 0x80) {
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
}

Token Lexer::makeToken(TokenType type) {
  std::string lexeme(m_source.substr(m_start, m_current - m_start));
  return Token(type, std::move(lexeme), SourceLocation(m_line, m_startColumn));
}

Token Lexer::makeToken(TokenType type, const std::string &lexeme) {
  return Token(type, lexeme, SourceLocation(m_line, m_startColumn));
}

Token Lexer::errorToken(const std::string &message) {
  return Token(TokenType::Error, message,
               SourceLocation(m_line, m_startColumn));
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

  Token token(TokenType::String, std::move(value),
              SourceLocation(m_line, m_startColumn));
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

  if (isFloat) {
    token.floatValue = std::stof(lexeme);
  } else {
    token.intValue = std::stoi(lexeme);
  }

  return token;
}

Token Lexer::scanIdentifier() {
  // Scan both ASCII and Unicode identifier characters
  while (!isAtEnd()) {
    char c = peek();

    // ASCII alphanumeric or underscore
    if (safeIsAlnum(c) || c == '_') {
      advance();
      continue;
    }

    // Check for UTF-8 multibyte character
    unsigned char uc = static_cast<unsigned char>(c);
    if (uc >= 0x80) {
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

  // Validate color format: #RGB, #RGBA, #RRGGBB, #RRGGBBAA
  size_t hexLen = lexeme.size() - 1; // Exclude '#'
  if (hexLen != 3 && hexLen != 4 && hexLen != 6 && hexLen != 8) {
    return errorToken("Invalid color literal format");
  }

  return Token(TokenType::String, std::move(lexeme),
               SourceLocation(m_line, m_startColumn));
}

TokenType Lexer::identifierType(const std::string &lexeme) const {
  auto it = m_keywords.find(lexeme);
  if (it != m_keywords.end()) {
    return it->second;
  }
  return TokenType::Identifier;
}

} // namespace NovelMind::scripting
