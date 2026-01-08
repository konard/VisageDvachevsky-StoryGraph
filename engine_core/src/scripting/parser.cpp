#include "NovelMind/scripting/parser.hpp"

namespace NovelMind::scripting {

Parser::Parser() : m_tokens(nullptr), m_current(0) {}

Parser::~Parser() = default;

Result<Program> Parser::parse(const std::vector<Token> &tokens) {
  m_tokens = &tokens;
  m_current = 0;
  m_errors.clear();
  m_program = Program{};

  while (!isAtEnd()) {
    try {
      parseDeclaration();
    } catch (...) {
      synchronize();
    }
  }

  if (!m_errors.empty()) {
    return Result<Program>::error(m_errors[0].message);
  }

  return Result<Program>::ok(std::move(m_program));
}

const std::vector<ParseError> &Parser::getErrors() const { return m_errors; }

// Token navigation

bool Parser::isAtEnd() const { return peek().type == TokenType::EndOfFile; }

const Token &Parser::peek() const { return (*m_tokens)[m_current]; }

const Token &Parser::previous() const { return (*m_tokens)[m_current - 1]; }

const Token &Parser::advance() {
  if (!isAtEnd()) {
    ++m_current;
  }
  return previous();
}

bool Parser::check(TokenType type) const {
  if (isAtEnd())
    return false;
  return peek().type == type;
}

bool Parser::match(TokenType type) {
  if (check(type)) {
    advance();
    return true;
  }
  return false;
}

bool Parser::match(std::initializer_list<TokenType> types) {
  for (TokenType type : types) {
    if (check(type)) {
      advance();
      return true;
    }
  }
  return false;
}

const Token &Parser::consume(TokenType type, const std::string &message) {
  if (check(type)) {
    return advance();
  }
  error(message);
  return peek();
}

// Error handling

void Parser::error(const std::string &message) {
  m_errors.emplace_back(message, peek().location);
}

void Parser::synchronize() {
  advance();

  while (!isAtEnd()) {
    // Statement boundaries
    switch (peek().type) {
    case TokenType::Character:
    case TokenType::Scene:
    case TokenType::Show:
    case TokenType::Hide:
    case TokenType::Say:
    case TokenType::Choice:
    case TokenType::If:
    case TokenType::Goto:
    case TokenType::Wait:
    case TokenType::Play:
    case TokenType::Stop:
    case TokenType::Set:
    case TokenType::Transition:
    case TokenType::Move:
      return;
    default:
      break;
    }
    advance();
  }
}

void Parser::skipNewlines() {
  while (match(TokenType::Newline)) {
  }
}

// Grammar rules - declarations

void Parser::parseDeclaration() {
  skipNewlines();
  if (isAtEnd()) {
    return;
  }
  if (match(TokenType::Character)) {
    m_program.characters.push_back(parseCharacterDecl());
  } else if (match(TokenType::Scene)) {
    m_program.scenes.push_back(parseSceneDecl());
  } else {
    auto stmt = parseStatement();
    if (stmt) {
      m_program.globalStatements.push_back(std::move(stmt));
    }
  }
}

CharacterDecl Parser::parseCharacterDecl() {
  // character Hero(name="Alex", color="#FFCC00")
  CharacterDecl decl;

  const Token &id =
      consume(TokenType::Identifier, "Expected character identifier");
  decl.id = id.lexeme;

  if (match(TokenType::LeftParen)) {
    // Parse properties
    do {
      const Token &propName =
          consume(TokenType::Identifier, "Expected property name");
      consume(TokenType::Assign, "Expected '=' after property name");

      if (propName.lexeme == "name") {
        const Token &value =
            consume(TokenType::String, "Expected string for name");
        decl.displayName = value.lexeme;
      } else if (propName.lexeme == "color") {
        const Token &value =
            consume(TokenType::String, "Expected color string");
        decl.color = value.lexeme;
      } else if (propName.lexeme == "sprite") {
        const Token &value =
            consume(TokenType::String, "Expected sprite string");
        decl.defaultSprite = value.lexeme;
      } else {
        error("Unknown character property: " + propName.lexeme);
        // Skip the value
        advance();
      }
    } while (match(TokenType::Comma));

    consume(TokenType::RightParen, "Expected ')' after character properties");
  }

  return decl;
}

SceneDecl Parser::parseSceneDecl() {
  // scene intro { ... }
  SceneDecl decl;

  const Token &name = consume(TokenType::Identifier, "Expected scene name");
  decl.name = name.lexeme;

  consume(TokenType::LeftBrace, "Expected '{' before scene body");

  while (!check(TokenType::RightBrace) && !isAtEnd()) {
    skipNewlines();
    if (check(TokenType::RightBrace) || isAtEnd()) {
      break;
    }
    auto stmt = parseStatement();
    if (stmt) {
      decl.body.push_back(std::move(stmt));
    }
  }

  consume(TokenType::RightBrace, "Expected '}' after scene body");

  return decl;
}

// Grammar rules - statements

StmtPtr Parser::parseStatement() {
  skipNewlines();
  if (check(TokenType::RightBrace) || isAtEnd()) {
    return nullptr;
  }
  if (match(TokenType::Show))
    return parseShowStmt();
  if (match(TokenType::Hide))
    return parseHideStmt();
  if (match(TokenType::Say))
    return parseSayStmt();
  if (match(TokenType::Choice))
    return parseChoiceStmt();
  if (match(TokenType::If))
    return parseIfStmt();
  if (match(TokenType::Goto))
    return parseGotoStmt();
  if (match(TokenType::Wait))
    return parseWaitStmt();
  if (match(TokenType::Play))
    return parsePlayStmt();
  if (match(TokenType::Stop))
    return parseStopStmt();
  if (match(TokenType::Set))
    return parseSetStmt();
  if (match(TokenType::Transition))
    return parseTransitionStmt();
  if (match(TokenType::Move))
    return parseMoveStmt();
  if (match(TokenType::LeftBrace))
    return parseBlock();

  // Check for shorthand say: Identifier "string"
  if (check(TokenType::Identifier)) {
    const Token &id = peek();
    if (m_current + 1 < m_tokens->size() &&
        (*m_tokens)[m_current + 1].type == TokenType::String) {
      advance(); // consume identifier
      SayStmt say;
      say.speaker = id.lexeme;
      const Token &text =
          consume(TokenType::String, "Expected string after speaker");
      say.text = text.lexeme;
      return makeStmt(std::move(say), id.location);
    }
  }

  // Expression statement
  auto expr = parseExpression();
  if (expr) {
    ExpressionStmt exprStmt;
    exprStmt.expression = std::move(expr);
    return makeStmt(std::move(exprStmt), previous().location);
  }

  return nullptr;
}

StmtPtr Parser::parseShowStmt() {
  // show background "bg_name"
  // show Character at position
  // show Character "sprite_name" at position
  SourceLocation loc = previous().location;
  ShowStmt stmt;

  if (match(TokenType::Background)) {
    stmt.target = ShowStmt::Target::Background;
    const Token &resource =
        consume(TokenType::String, "Expected background resource");
    stmt.resource = resource.lexeme;
  } else {
    const Token &id =
        consume(TokenType::Identifier, "Expected character/sprite identifier");
    stmt.identifier = id.lexeme;
    stmt.target = ShowStmt::Target::Character;

    // Optional sprite override
    if (check(TokenType::String)) {
      const Token &sprite = advance();
      stmt.resource = sprite.lexeme;
    }

    // Optional position
    if (match(TokenType::At)) {
      stmt.position = parsePosition();

      if (stmt.position == Position::Custom) {
        const Token &x = consume(TokenType::Float, "Expected X coordinate");
        stmt.customX = x.floatValue;
        consume(TokenType::Comma, "Expected ',' between coordinates");
        const Token &y = consume(TokenType::Float, "Expected Y coordinate");
        stmt.customY = y.floatValue;
      }
    }
  }

  // Optional transition
  if (match(TokenType::Transition)) {
    const Token &trans =
        consume(TokenType::Identifier, "Expected transition type");
    stmt.transition = trans.lexeme;

    if (check(TokenType::Float) || check(TokenType::Integer)) {
      const Token &dur = advance();
      stmt.duration = dur.type == TokenType::Float
                          ? dur.floatValue
                          : static_cast<f32>(dur.intValue);
    }
  }

  return makeStmt(std::move(stmt), loc);
}

StmtPtr Parser::parseHideStmt() {
  // hide Character
  SourceLocation loc = previous().location;
  HideStmt stmt;

  const Token &id =
      consume(TokenType::Identifier, "Expected identifier to hide");
  stmt.identifier = id.lexeme;

  // Optional transition
  if (match(TokenType::Transition)) {
    const Token &trans =
        consume(TokenType::Identifier, "Expected transition type");
    stmt.transition = trans.lexeme;

    if (check(TokenType::Float) || check(TokenType::Integer)) {
      const Token &dur = advance();
      stmt.duration = dur.type == TokenType::Float
                          ? dur.floatValue
                          : static_cast<f32>(dur.intValue);
    }
  }

  return makeStmt(std::move(stmt), loc);
}

StmtPtr Parser::parseSayStmt() {
  // say Character "text"
  // say "text" (narrator)
  SourceLocation loc = previous().location;
  SayStmt stmt;

  if (check(TokenType::Identifier)) {
    const Token &speaker = advance();
    stmt.speaker = speaker.lexeme;
  }

  const Token &text = consume(TokenType::String, "Expected dialogue text");
  stmt.text = text.lexeme;

  return makeStmt(std::move(stmt), loc);
}

StmtPtr Parser::parseChoiceStmt() {
  // choice {
  //     "Option 1" -> goto label1
  //     "Option 2" if condition -> { ... }
  // }
  SourceLocation loc = previous().location;
  ChoiceStmt stmt;

  consume(TokenType::LeftBrace, "Expected '{' before choice options");

  while (!check(TokenType::RightBrace) && !isAtEnd()) {
    ChoiceOption option;

    const Token &text = consume(TokenType::String, "Expected choice text");
    option.text = text.lexeme;

    // Optional condition
    if (match(TokenType::If)) {
      option.condition = parseExpression();
    }

    consume(TokenType::Arrow, "Expected '->' after choice text");

    // Either goto or block
    if (match(TokenType::Goto)) {
      const Token &target =
          consume(TokenType::Identifier, "Expected goto target");
      option.gotoTarget = target.lexeme;
    } else if (check(TokenType::LeftBrace)) {
      advance();
      option.body = parseStatementList();
      consume(TokenType::RightBrace, "Expected '}' after choice body");
    } else {
      // Single statement
      auto singleStmt = parseStatement();
      if (singleStmt) {
        option.body.push_back(std::move(singleStmt));
      }
    }

    stmt.options.push_back(std::move(option));
  }

  consume(TokenType::RightBrace, "Expected '}' after choice block");

  return makeStmt(std::move(stmt), loc);
}

StmtPtr Parser::parseIfStmt() {
  // if condition { ... } else { ... }
  SourceLocation loc = previous().location;
  IfStmt stmt;

  stmt.condition = parseExpression();

  consume(TokenType::LeftBrace, "Expected '{' before if body");
  stmt.thenBranch = parseStatementList();
  consume(TokenType::RightBrace, "Expected '}' after if body");

  if (match(TokenType::Else)) {
    if (match(TokenType::If)) {
      // else if - create nested if as single statement in else branch
      auto nestedIf = parseIfStmt();
      if (nestedIf) {
        stmt.elseBranch.push_back(std::move(nestedIf));
      }
    } else {
      consume(TokenType::LeftBrace, "Expected '{' before else body");
      stmt.elseBranch = parseStatementList();
      consume(TokenType::RightBrace, "Expected '}' after else body");
    }
  }

  return makeStmt(std::move(stmt), loc);
}

StmtPtr Parser::parseGotoStmt() {
  // goto scene_name
  SourceLocation loc = previous().location;
  GotoStmt stmt;

  const Token &target = consume(TokenType::Identifier, "Expected goto target");
  stmt.target = target.lexeme;

  return makeStmt(std::move(stmt), loc);
}

StmtPtr Parser::parseWaitStmt() {
  // wait 2.0
  SourceLocation loc = previous().location;
  WaitStmt stmt;

  const Token &duration = advance();
  if (duration.type == TokenType::Float) {
    stmt.duration = duration.floatValue;
  } else if (duration.type == TokenType::Integer) {
    stmt.duration = static_cast<f32>(duration.intValue);
  } else {
    error("Expected duration after 'wait'");
    stmt.duration = 0.0f;
  }

  return makeStmt(std::move(stmt), loc);
}

StmtPtr Parser::parsePlayStmt() {
  // play sound "file.ogg"
  // play music "file.ogg" loop
  SourceLocation loc = previous().location;
  PlayStmt stmt;

  if (match(TokenType::Sound)) {
    stmt.type = PlayStmt::MediaType::Sound;
  } else if (match(TokenType::Music)) {
    stmt.type = PlayStmt::MediaType::Music;
  } else {
    error("Expected 'sound' or 'music' after 'play'");
    return nullptr;
  }

  const Token &resource = consume(TokenType::String, "Expected resource path");
  stmt.resource = resource.lexeme;

  // Optional volume
  if (check(TokenType::Float) || check(TokenType::Integer)) {
    const Token &vol = advance();
    stmt.volume = vol.type == TokenType::Float ? vol.floatValue
                                               : static_cast<f32>(vol.intValue);
  }

  // Optional loop keyword (represented as identifier)
  if (check(TokenType::Identifier) && peek().lexeme == "loop") {
    advance();
    stmt.loop = true;
  }

  return makeStmt(std::move(stmt), loc);
}

StmtPtr Parser::parseStopStmt() {
  // stop music
  // stop music fade 1.0
  SourceLocation loc = previous().location;
  StopStmt stmt;

  if (match(TokenType::Sound)) {
    stmt.type = PlayStmt::MediaType::Sound;
  } else if (match(TokenType::Music)) {
    stmt.type = PlayStmt::MediaType::Music;
  } else {
    error("Expected 'sound' or 'music' after 'stop'");
    return nullptr;
  }

  // Optional fade
  if (match(TokenType::Fade)) {
    const Token &dur = advance();
    if (dur.type == TokenType::Float) {
      stmt.fadeOut = dur.floatValue;
    } else if (dur.type == TokenType::Integer) {
      stmt.fadeOut = static_cast<f32>(dur.intValue);
    }
  }

  return makeStmt(std::move(stmt), loc);
}

StmtPtr Parser::parseSetStmt() {
  // set variable = expression
  // set flag variable = value
  SourceLocation loc = previous().location;
  SetStmt stmt;

  // Check for optional 'flag' keyword
  if (check(TokenType::Identifier) && peek().lexeme == "flag") {
    advance(); // consume 'flag'
    stmt.isFlag = true;
  }

  const Token &var = consume(TokenType::Identifier, "Expected variable name");
  stmt.variable = var.lexeme;

  consume(TokenType::Assign, "Expected '=' after variable name");

  stmt.value = parseExpression();

  return makeStmt(std::move(stmt), loc);
}

StmtPtr Parser::parseTransitionStmt() {
  // transition fade 1.0
  // transition dissolve 0.5 "#000000"
  SourceLocation loc = previous().location;
  TransitionStmt stmt;

  // Transition type can be a keyword (fade) or identifier (dissolve, slide,
  // etc.)
  const Token &type = advance();
  if (type.type == TokenType::Fade) {
    stmt.type = "fade";
  } else if (type.type == TokenType::Identifier) {
    stmt.type = type.lexeme;
  } else {
    error("Expected transition type (fade, dissolve, slide, etc.)");
    stmt.type = "fade";
  }

  const Token &dur = advance();
  if (dur.type == TokenType::Float) {
    stmt.duration = dur.floatValue;
  } else if (dur.type == TokenType::Integer) {
    stmt.duration = static_cast<f32>(dur.intValue);
  } else {
    error("Expected duration after transition type");
    stmt.duration = 0.0f;
  }

  // Optional color
  if (check(TokenType::String)) {
    const Token &color = advance();
    stmt.color = color.lexeme;
  }

  return makeStmt(std::move(stmt), loc);
}

StmtPtr Parser::parseMoveStmt() {
  // move <character> to <position> duration=<float>
  SourceLocation loc = previous().location;
  MoveStmt stmt;

  const Token &id =
      consume(TokenType::Identifier, "Expected character identifier after 'move'");
  stmt.characterId = id.lexeme;

  consume(TokenType::To, "Expected 'to' after character name");

  stmt.position = parsePosition();

  if (stmt.position == Position::Custom) {
    const Token &x = consume(TokenType::Float, "Expected X coordinate");
    stmt.customX = x.floatValue;
    consume(TokenType::Comma, "Expected ',' between coordinates");
    const Token &y = consume(TokenType::Float, "Expected Y coordinate");
    stmt.customY = y.floatValue;
  }

  // Optional duration parameter: duration=<float>
  if (match(TokenType::Duration)) {
    consume(TokenType::Assign, "Expected '=' after 'duration'");

    const Token &dur = advance();
    if (dur.type == TokenType::Float) {
      stmt.duration = dur.floatValue;
    } else if (dur.type == TokenType::Integer) {
      stmt.duration = static_cast<f32>(dur.intValue);
    } else {
      error("Expected number after 'duration='");
      stmt.duration = 0.5f;
    }
  }

  return makeStmt(std::move(stmt), loc);
}

StmtPtr Parser::parseBlock() {
  SourceLocation loc = previous().location;
  BlockStmt block;

  block.statements = parseStatementList();

  consume(TokenType::RightBrace, "Expected '}' after block");

  return makeStmt(std::move(block), loc);
}

// Grammar rules - expressions (precedence climbing)

ExprPtr Parser::parseExpression() { return parseOr(); }

ExprPtr Parser::parseOr() {
  auto expr = parseAnd();

  while (match(TokenType::Or)) {
    SourceLocation loc = previous().location;
    TokenType op = previous().type;
    auto right = parseAnd();

    BinaryExpr binary;
    binary.left = std::move(expr);
    binary.op = op;
    binary.right = std::move(right);

    expr = makeExpr(std::move(binary), loc);
  }

  return expr;
}

ExprPtr Parser::parseAnd() {
  auto expr = parseEquality();

  while (match(TokenType::And)) {
    SourceLocation loc = previous().location;
    TokenType op = previous().type;
    auto right = parseEquality();

    BinaryExpr binary;
    binary.left = std::move(expr);
    binary.op = op;
    binary.right = std::move(right);

    expr = makeExpr(std::move(binary), loc);
  }

  return expr;
}

ExprPtr Parser::parseEquality() {
  auto expr = parseComparison();

  while (match({TokenType::Equal, TokenType::NotEqual})) {
    SourceLocation loc = previous().location;
    TokenType op = previous().type;
    auto right = parseComparison();

    BinaryExpr binary;
    binary.left = std::move(expr);
    binary.op = op;
    binary.right = std::move(right);

    expr = makeExpr(std::move(binary), loc);
  }

  return expr;
}

ExprPtr Parser::parseComparison() {
  auto expr = parseTerm();

  while (match({TokenType::Less, TokenType::LessEqual, TokenType::Greater,
                TokenType::GreaterEqual})) {
    SourceLocation loc = previous().location;
    TokenType op = previous().type;
    auto right = parseTerm();

    BinaryExpr binary;
    binary.left = std::move(expr);
    binary.op = op;
    binary.right = std::move(right);

    expr = makeExpr(std::move(binary), loc);
  }

  return expr;
}

ExprPtr Parser::parseTerm() {
  auto expr = parseFactor();

  while (match({TokenType::Plus, TokenType::Minus})) {
    SourceLocation loc = previous().location;
    TokenType op = previous().type;
    auto right = parseFactor();

    BinaryExpr binary;
    binary.left = std::move(expr);
    binary.op = op;
    binary.right = std::move(right);

    expr = makeExpr(std::move(binary), loc);
  }

  return expr;
}

ExprPtr Parser::parseFactor() {
  auto expr = parseUnary();

  while (match({TokenType::Star, TokenType::Slash, TokenType::Percent})) {
    SourceLocation loc = previous().location;
    TokenType op = previous().type;
    auto right = parseUnary();

    BinaryExpr binary;
    binary.left = std::move(expr);
    binary.op = op;
    binary.right = std::move(right);

    expr = makeExpr(std::move(binary), loc);
  }

  return expr;
}

ExprPtr Parser::parseUnary() {
  if (match({TokenType::Not, TokenType::Minus})) {
    SourceLocation loc = previous().location;
    TokenType op = previous().type;
    auto operand = parseUnary();

    UnaryExpr unary;
    unary.op = op;
    unary.operand = std::move(operand);

    return makeExpr(std::move(unary), loc);
  }

  return parseCall();
}

ExprPtr Parser::parseCall() {
  auto expr = parsePrimary();

  while (true) {
    if (match(TokenType::LeftParen)) {
      // Function call
      SourceLocation loc = previous().location;
      std::vector<ExprPtr> args;

      if (!check(TokenType::RightParen)) {
        do {
          args.push_back(parseExpression());
        } while (match(TokenType::Comma));
      }

      consume(TokenType::RightParen, "Expected ')' after arguments");

      // Extract function name from expression
      std::string callee;
      if (expr && std::holds_alternative<IdentifierExpr>(expr->data)) {
        callee = std::get<IdentifierExpr>(expr->data).name;
      }

      CallExpr call;
      call.callee = std::move(callee);
      call.arguments = std::move(args);

      expr = makeExpr(std::move(call), loc);
    } else if (match(TokenType::Dot)) {
      // Property access
      SourceLocation loc = previous().location;
      const Token &name =
          consume(TokenType::Identifier, "Expected property name after '.'");

      PropertyExpr prop;
      prop.object = std::move(expr);
      prop.property = name.lexeme;

      expr = makeExpr(std::move(prop), loc);
    } else {
      break;
    }
  }

  return expr;
}

ExprPtr Parser::parsePrimary() {
  SourceLocation loc = peek().location;

  if (match(TokenType::True)) {
    LiteralExpr lit;
    lit.value = true;
    return makeExpr(std::move(lit), loc);
  }

  if (match(TokenType::False)) {
    LiteralExpr lit;
    lit.value = false;
    return makeExpr(std::move(lit), loc);
  }

  if (match(TokenType::Integer)) {
    LiteralExpr lit;
    lit.value = previous().intValue;
    return makeExpr(std::move(lit), loc);
  }

  if (match(TokenType::Float)) {
    LiteralExpr lit;
    lit.value = previous().floatValue;
    return makeExpr(std::move(lit), loc);
  }

  if (match(TokenType::String)) {
    LiteralExpr lit;
    lit.value = previous().lexeme;
    return makeExpr(std::move(lit), loc);
  }

  if (match(TokenType::Identifier)) {
    IdentifierExpr id;
    id.name = previous().lexeme;
    return makeExpr(std::move(id), loc);
  }

  if (match(TokenType::LeftParen)) {
    auto expr = parseExpression();
    consume(TokenType::RightParen, "Expected ')' after expression");
    return expr;
  }

  error("Expected expression");
  if (!isAtEnd()) {
    advance();
  }
  return nullptr;
}

// Helper parsers

Position Parser::parsePosition() {
  if (check(TokenType::Identifier)) {
    const std::string &pos = peek().lexeme;

    if (pos == "left") {
      advance();
      return Position::Left;
    }
    if (pos == "center") {
      advance();
      return Position::Center;
    }
    if (pos == "right") {
      advance();
      return Position::Right;
    }
    if (pos == "custom") {
      advance();
      return Position::Custom;
    }
  }

  error("Expected position (left, center, right, or custom)");
  return Position::Center;
}

std::string Parser::parseString() {
  const Token &str = consume(TokenType::String, "Expected string");
  return str.lexeme;
}

std::vector<StmtPtr> Parser::parseStatementList() {
  std::vector<StmtPtr> statements;

  while (!check(TokenType::RightBrace) && !isAtEnd()) {
    skipNewlines();
    if (check(TokenType::RightBrace) || isAtEnd()) {
      break;
    }
    const size_t start = m_current;
    auto stmt = parseStatement();
    const bool hasStmt = static_cast<bool>(stmt);
    if (hasStmt) {
      statements.push_back(std::move(stmt));
    }
    if (!hasStmt && m_current == start) {
      error("Unexpected token in statement list");
      advance();
    }
  }

  return statements;
}

} // namespace NovelMind::scripting
