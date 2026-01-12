#pragma once

/**
 * @file token.hpp
 * @brief Token definitions for the NM Script lexer
 *
 * This module defines the token types and structures used by the
 * NM Script lexer and parser.
 */

#include "NovelMind/core/types.hpp"
#include <string>

namespace NovelMind::scripting {

/**
 * @brief Token types for NM Script language
 */
enum class TokenType : u8 {
  // End of file
  EndOfFile,

  // Literals
  Integer,
  Float,
  String,
  Identifier,

  // Keywords
  Character,  // character
  Scene,      // scene
  Show,       // show
  Hide,       // hide
  Say,        // say
  Choice,     // choice
  If,         // if
  Else,       // else
  Goto,       // goto
  Wait,       // wait
  Play,       // play
  Stop,       // stop
  Set,        // set
  True,       // true
  False,      // false
  At,         // at
  And,        // and
  Or,         // or
  Not,        // not
  Background, // background
  Music,      // music
  Sound,      // sound
  Transition, // transition
  Fade,       // fade
  Move,       // move
  To,         // to
  Duration,   // duration

  // Operators
  Assign,       // =
  Plus,         // +
  Minus,        // -
  Star,         // *
  Slash,        // /
  Percent,      // %
  Equal,        // ==
  NotEqual,     // !=
  Less,         // <
  LessEqual,    // <=
  Greater,      // >
  GreaterEqual, // >=
  Arrow,        // ->

  // Delimiters
  LeftParen,    // (
  RightParen,   // )
  LeftBrace,    // {
  RightBrace,   // }
  LeftBracket,  // [
  RightBracket, // ]
  Comma,        // ,
  Colon,        // :
  Semicolon,    // ;
  Dot,          // .
  Hash,         // #

  // Special
  Newline,
  Error
};

/**
 * @brief Represents a source location for error reporting
 */
struct SourceLocation {
  u32 line;
  u32 column;

  SourceLocation() : line(1), column(1) {}
  SourceLocation(u32 l, u32 c) : line(l), column(c) {}
};

/**
 * @brief Represents a token in the NM Script language
 */
struct Token {
  TokenType type;
  std::string lexeme;
  SourceLocation location;

  // Literal values (for performance, avoid variant overhead)
  union {
    i32 intValue;
    f32 floatValue;
  };

  Token() : type(TokenType::EndOfFile), lexeme(), location(), intValue(0) {}

  Token(TokenType t, std::string lex, SourceLocation loc)
      : type(t), lexeme(std::move(lex)), location(loc), intValue(0) {}

  [[nodiscard]] bool isKeyword() const {
    return type >= TokenType::Character && type <= TokenType::Duration;
  }

  [[nodiscard]] bool isOperator() const {
    return type >= TokenType::Assign && type <= TokenType::Arrow;
  }

  [[nodiscard]] bool isDelimiter() const {
    return type >= TokenType::LeftParen && type <= TokenType::Hash;
  }

  [[nodiscard]] bool isLiteral() const {
    return type >= TokenType::Integer && type <= TokenType::Identifier;
  }
};

/**
 * @brief Convert token type to string for debugging
 */
[[nodiscard]] inline const char* tokenTypeToString(TokenType type) {
  switch (type) {
  case TokenType::EndOfFile:
    return "EndOfFile";
  case TokenType::Integer:
    return "Integer";
  case TokenType::Float:
    return "Float";
  case TokenType::String:
    return "String";
  case TokenType::Identifier:
    return "Identifier";
  case TokenType::Character:
    return "character";
  case TokenType::Scene:
    return "scene";
  case TokenType::Show:
    return "show";
  case TokenType::Hide:
    return "hide";
  case TokenType::Say:
    return "say";
  case TokenType::Choice:
    return "choice";
  case TokenType::If:
    return "if";
  case TokenType::Else:
    return "else";
  case TokenType::Goto:
    return "goto";
  case TokenType::Wait:
    return "wait";
  case TokenType::Play:
    return "play";
  case TokenType::Stop:
    return "stop";
  case TokenType::Set:
    return "set";
  case TokenType::True:
    return "true";
  case TokenType::False:
    return "false";
  case TokenType::At:
    return "at";
  case TokenType::And:
    return "and";
  case TokenType::Or:
    return "or";
  case TokenType::Not:
    return "not";
  case TokenType::Background:
    return "background";
  case TokenType::Music:
    return "music";
  case TokenType::Sound:
    return "sound";
  case TokenType::Transition:
    return "transition";
  case TokenType::Fade:
    return "fade";
  case TokenType::Move:
    return "move";
  case TokenType::To:
    return "to";
  case TokenType::Duration:
    return "duration";
  case TokenType::Assign:
    return "=";
  case TokenType::Plus:
    return "+";
  case TokenType::Minus:
    return "-";
  case TokenType::Star:
    return "*";
  case TokenType::Slash:
    return "/";
  case TokenType::Percent:
    return "%";
  case TokenType::Equal:
    return "==";
  case TokenType::NotEqual:
    return "!=";
  case TokenType::Less:
    return "<";
  case TokenType::LessEqual:
    return "<=";
  case TokenType::Greater:
    return ">";
  case TokenType::GreaterEqual:
    return ">=";
  case TokenType::Arrow:
    return "->";
  case TokenType::LeftParen:
    return "(";
  case TokenType::RightParen:
    return ")";
  case TokenType::LeftBrace:
    return "{";
  case TokenType::RightBrace:
    return "}";
  case TokenType::LeftBracket:
    return "[";
  case TokenType::RightBracket:
    return "]";
  case TokenType::Comma:
    return ",";
  case TokenType::Colon:
    return ":";
  case TokenType::Semicolon:
    return ";";
  case TokenType::Dot:
    return ".";
  case TokenType::Hash:
    return "#";
  case TokenType::Newline:
    return "Newline";
  case TokenType::Error:
    return "Error";
  }
  return "Unknown";
}

} // namespace NovelMind::scripting
