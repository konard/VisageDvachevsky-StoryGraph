// Test program to validate the script file validation logic
#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <iostream>
#include <string>

using namespace NovelMind::scripting;

bool validateScriptFile(const QString &filePath, QString &errorMessage) {
  // 1. Open and read the file
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    errorMessage = QString("Cannot open file: %1").arg(file.errorString());
    return false;
  }

  // 2. Read file content as UTF-8
  const QString content = QString::fromUtf8(file.readAll());
  file.close();

  if (content.isEmpty()) {
    errorMessage = QString("Script file is empty");
    return false;
  }

  // 3. Tokenize using Lexer
  Lexer lexer;
  auto tokensResult = lexer.tokenize(content.toStdString());

  // Check for lexer errors
  const auto &lexerErrors = lexer.getErrors();
  if (!lexerErrors.empty()) {
    const auto &firstError = lexerErrors[0];
    errorMessage = QString("Script syntax error at line %1, column %2: %3")
                       .arg(firstError.location.line)
                       .arg(firstError.location.column)
                       .arg(QString::fromStdString(firstError.message));
    return false;
  }

  if (!tokensResult.isOk()) {
    errorMessage = QString("Script tokenization failed: %1")
                       .arg(QString::fromStdString(tokensResult.error()));
    return false;
  }

  // 4. Parse the tokens
  Parser parser;
  auto parseResult = parser.parse(tokensResult.value());

  // Check for parser errors
  const auto &parserErrors = parser.getErrors();
  if (!parserErrors.empty()) {
    const auto &firstError = parserErrors[0];
    errorMessage = QString("Script parse error at line %1, column %2: %3")
                       .arg(firstError.location.line)
                       .arg(firstError.location.column)
                       .arg(QString::fromStdString(firstError.message));
    return false;
  }

  if (!parseResult.isOk()) {
    errorMessage = QString("Script parsing failed: %1")
                       .arg(QString::fromStdString(parseResult.error()));
    return false;
  }

  // Script is valid
  return true;
}

int main() {
  std::cout << "Testing script validation...\n\n";

  // Test valid script
  QString errorMsg;
  if (validateScriptFile("experiments/issue-393-validation-test/valid_script.nms", errorMsg)) {
    std::cout << "[PASS] valid_script.nms: Valid script accepted\n";
  } else {
    std::cout << "[FAIL] valid_script.nms: " << errorMsg.toStdString() << "\n";
  }

  // Test invalid scripts
  if (!validateScriptFile("experiments/issue-393-validation-test/invalid_unterminated_string.nms", errorMsg)) {
    std::cout << "[PASS] invalid_unterminated_string.nms: Invalid script rejected\n";
    std::cout << "       Error: " << errorMsg.toStdString() << "\n";
  } else {
    std::cout << "[FAIL] invalid_unterminated_string.nms: Should have been rejected\n";
  }

  if (!validateScriptFile("experiments/issue-393-validation-test/invalid_missing_brace.nms", errorMsg)) {
    std::cout << "[PASS] invalid_missing_brace.nms: Invalid script rejected\n";
    std::cout << "       Error: " << errorMsg.toStdString() << "\n";
  } else {
    std::cout << "[FAIL] invalid_missing_brace.nms: Should have been rejected\n";
  }

  if (!validateScriptFile("experiments/issue-393-validation-test/empty_file.nms", errorMsg)) {
    std::cout << "[PASS] empty_file.nms: Empty file rejected\n";
    std::cout << "       Error: " << errorMsg.toStdString() << "\n";
  } else {
    std::cout << "[FAIL] empty_file.nms: Should have been rejected\n";
  }

  std::cout << "\nAll tests completed!\n";
  return 0;
}
