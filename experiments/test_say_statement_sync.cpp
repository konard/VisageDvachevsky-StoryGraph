/**
 * @file test_say_statement_sync.cpp
 * @brief Test for issue #67 - syncing graph node text to .nms script
 *
 * This test verifies that when dialogue text is changed in a graph node,
 * the corresponding say statement in the .nms script is updated.
 */

#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QTextStream>
#include <iostream>

// Simplified version of the updateSceneSayStatement function for testing
bool updateSceneSayStatement(const QString &sceneId, const QString &scriptPath,
                             const QString &speaker, const QString &text) {
  if (sceneId.isEmpty() || scriptPath.isEmpty()) {
    return false;
  }

  QFile file(scriptPath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }
  QString content = QString::fromUtf8(file.readAll());
  file.close();

  // Find the scene block
  const QRegularExpression sceneRe(
      QString("\\bscene\\s+%1\\b").arg(QRegularExpression::escape(sceneId)));
  const QRegularExpressionMatch match = sceneRe.match(content);
  if (!match.hasMatch()) {
    return false;
  }

  const qsizetype bracePos = content.indexOf('{', match.capturedEnd());
  if (bracePos < 0) {
    return false;
  }

  // Find the scene end
  int depth = 0;
  bool inString = false;
  QChar stringDelimiter;
  bool inLineComment = false;
  bool inBlockComment = false;
  qsizetype sceneEnd = -1;

  for (qsizetype i = bracePos; i < content.size(); ++i) {
    const QChar c = content.at(i);
    const QChar next = (i + 1 < content.size()) ? content.at(i + 1) : QChar();

    if (inLineComment) {
      if (c == '\n') {
        inLineComment = false;
      }
      continue;
    }
    if (inBlockComment) {
      if (c == '*' && next == '/') {
        inBlockComment = false;
        ++i;
      }
      continue;
    }

    if (!inString && c == '/' && next == '/') {
      inLineComment = true;
      ++i;
      continue;
    }
    if (!inString && c == '/' && next == '*') {
      inBlockComment = true;
      ++i;
      continue;
    }

    if (c == '"' || c == '\'') {
      if (!inString) {
        inString = true;
        stringDelimiter = c;
      } else if (stringDelimiter == c && content.at(i - 1) != '\\') {
        inString = false;
      }
    }

    if (inString) {
      continue;
    }

    if (c == '{') {
      ++depth;
    } else if (c == '}') {
      --depth;
      if (depth == 0) {
        sceneEnd = i;
        break;
      }
    }
  }

  if (sceneEnd < 0) {
    return false;
  }

  const qsizetype bodyStart = bracePos + 1;
  const qsizetype bodyEnd = sceneEnd;
  QString body = content.mid(bodyStart, bodyEnd - bodyStart);

  // Find and replace the first say statement
  const QRegularExpression sayRe("\\bsay\\s+(\\w+)\\s+\"([^\"]*)\"",
                                 QRegularExpression::DotMatchesEverythingOption);

  QRegularExpressionMatch sayMatch = sayRe.match(body);
  if (!sayMatch.hasMatch()) {
    // No say statement found in the scene - add one at the beginning
    QString escapedText = text;
    escapedText.replace("\\", "\\\\");
    escapedText.replace("\"", "\\\"");

    QString newSay = QString("\n    say %1 \"%2\"")
                         .arg(speaker.isEmpty() ? "Narrator" : speaker,
                              escapedText);
    body = newSay + body;
  } else {
    // Replace the existing say statement
    QString escapedText = text;
    escapedText.replace("\\", "\\\\");
    escapedText.replace("\"", "\\\"");

    QString newSay =
        QString("say %1 \"%2\"")
            .arg(speaker.isEmpty() ? "Narrator" : speaker, escapedText);
    body.replace(sayMatch.capturedStart(), sayMatch.capturedLength(), newSay);
  }

  QString updated = content.left(bodyStart);
  updated += body;
  updated += content.mid(bodyEnd);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Text |
                 QIODevice::Truncate)) {
    return false;
  }
  file.write(updated.toUtf8());
  file.close();
  return true;
}

int main() {
  // Create a test script file
  const QString testScriptPath = "/tmp/test_script.nms";
  const QString originalContent = R"(// Test script
character Hero(name="Alex", color="#00AAFF")
character Narrator(name="", color="#AAAAAA")

scene intro {
    show background "bg_forest"
    say Hero "Original text from script"
    wait 1.0
}

scene chapter1 {
    say Narrator "Another scene"
}
)";

  // Write original script
  QFile originalFile(testScriptPath);
  if (!originalFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    std::cerr << "Failed to create test script file" << std::endl;
    return 1;
  }
  originalFile.write(originalContent.toUtf8());
  originalFile.close();

  std::cout << "=== Original script ===" << std::endl;
  std::cout << originalContent.toStdString() << std::endl;

  // Test 1: Update text in intro scene
  std::cout << "\n=== Test 1: Update text in intro scene ===" << std::endl;
  bool result1 =
      updateSceneSayStatement("intro", testScriptPath, "Hero",
                              "Updated text from Story Graph editor!");

  if (result1) {
    std::cout << "SUCCESS: say statement updated" << std::endl;
  } else {
    std::cerr << "FAILED: could not update say statement" << std::endl;
    return 1;
  }

  // Read and display updated script
  QFile updatedFile1(testScriptPath);
  if (!updatedFile1.open(QIODevice::ReadOnly | QIODevice::Text)) {
    std::cerr << "Failed to read updated script" << std::endl;
    return 1;
  }
  QString updated1 = QString::fromUtf8(updatedFile1.readAll());
  updatedFile1.close();

  std::cout << "\nUpdated script:" << std::endl;
  std::cout << updated1.toStdString() << std::endl;

  // Verify the update
  if (updated1.contains("Updated text from Story Graph editor!")) {
    std::cout << "VERIFIED: New text is present in script" << std::endl;
  } else {
    std::cerr << "FAILED: New text not found in script" << std::endl;
    return 1;
  }

  if (!updated1.contains("Original text from script")) {
    std::cout << "VERIFIED: Old text is removed from script" << std::endl;
  } else {
    std::cerr << "FAILED: Old text still present in script" << std::endl;
    return 1;
  }

  // Test 2: Update speaker as well
  std::cout << "\n=== Test 2: Update speaker ===" << std::endl;
  bool result2 = updateSceneSayStatement("intro", testScriptPath, "Narrator",
                                         "Now the narrator speaks!");

  if (result2) {
    std::cout << "SUCCESS: say statement updated with new speaker" << std::endl;
  } else {
    std::cerr << "FAILED: could not update say statement" << std::endl;
    return 1;
  }

  QFile updatedFile2(testScriptPath);
  if (!updatedFile2.open(QIODevice::ReadOnly | QIODevice::Text)) {
    std::cerr << "Failed to read updated script" << std::endl;
    return 1;
  }
  QString updated2 = QString::fromUtf8(updatedFile2.readAll());
  updatedFile2.close();

  std::cout << "\nUpdated script:" << std::endl;
  std::cout << updated2.toStdString() << std::endl;

  if (updated2.contains("say Narrator \"Now the narrator speaks!\"")) {
    std::cout << "VERIFIED: Speaker and text updated correctly" << std::endl;
  } else {
    std::cerr << "FAILED: Speaker/text not updated correctly" << std::endl;
    return 1;
  }

  // Clean up
  QFile::remove(testScriptPath);

  std::cout << "\n=== ALL TESTS PASSED ===" << std::endl;
  return 0;
}
