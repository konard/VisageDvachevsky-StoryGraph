#include "nm_story_graph_panel_detail.hpp"

#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextStream>

namespace NovelMind::editor::qt::detail {
namespace {

constexpr const char* kGraphLayoutFile = ".novelmind/story_graph.json";

QString graphLayoutPath() {
  const auto& pm = ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    return {};
  }
  const QString root = QString::fromStdString(pm.getProjectPath());
  return QDir(root).filePath(kGraphLayoutFile);
}

void ensureGraphLayoutDir() {
  const QString path = graphLayoutPath();
  if (path.isEmpty()) {
    return;
  }
  QFileInfo info(path);
  QDir dir(info.absolutePath());
  if (!dir.exists()) {
    dir.mkpath(".");
  }
}

QString buildGraphBlock(const QStringList& targets) {
  const QString indent("    ");
  QStringList lines;
  lines << indent + "// @graph-begin";
  lines << indent + "// Auto-generated transitions from Story Graph";

  if (targets.isEmpty()) {
    lines << indent + "// (no outgoing transitions)";
  } else if (targets.size() == 1) {
    lines << indent + QString("goto %1").arg(targets.front());
  } else {
    lines << indent + "choice {";
    for (const auto& target : targets) {
      lines << indent + "    " + QString("\"%1\" -> goto %2").arg(target, target);
    }
    lines << indent + "}";
  }

  lines << indent + "// @graph-end";
  return lines.join("\n");
}

/// Checks if a Unicode code point is a valid identifier start character.
/// Matches the same rules as the NMScript lexer for consistency.
bool isUnicodeIdentifierStart(uint codePoint) {
  // ASCII letters
  if ((codePoint >= 'A' && codePoint <= 'Z') || (codePoint >= 'a' && codePoint <= 'z')) {
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

/// Checks if a Unicode code point is valid within an identifier (after start).
bool isUnicodeIdentifierPart(uint codePoint) {
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

} // namespace

bool isValidSpeakerIdentifier(const QString& speaker) {
  if (speaker.isEmpty()) {
    return false;
  }

  // Check first character: must be a letter or underscore
  const QChar first = speaker.at(0);
  if (first != '_' && !isUnicodeIdentifierStart(first.unicode())) {
    return false;
  }

  // Check remaining characters: must be letters, digits, or underscores
  for (int i = 1; i < speaker.length(); ++i) {
    const QChar ch = speaker.at(i);
    if (ch != '_' && !isUnicodeIdentifierPart(ch.unicode())) {
      return false;
    }
  }

  return true;
}

QString sanitizeSpeakerIdentifier(const QString& speaker) {
  if (speaker.isEmpty()) {
    return QStringLiteral("Narrator");
  }

  // If already valid, check if it has meaningful content (not just underscores)
  if (isValidSpeakerIdentifier(speaker)) {
    bool hasNonUnderscore = false;
    for (const QChar& c : speaker) {
      if (c != '_') {
        hasNonUnderscore = true;
        break;
      }
    }
    if (hasNonUnderscore) {
      return speaker;
    }
    // Fall through to return Narrator for underscore-only identifiers
    return QStringLiteral("Narrator");
  }

  QString result;
  result.reserve(speaker.length() + 1);

  for (int i = 0; i < speaker.length(); ++i) {
    const QChar ch = speaker.at(i);

    if (i == 0) {
      // First character must be a letter or underscore
      if (ch == '_' || isUnicodeIdentifierStart(ch.unicode())) {
        result += ch;
      } else if (ch.isDigit()) {
        // Prepend underscore if starts with digit
        result += '_';
        result += ch;
      } else {
        // Replace invalid first character with underscore
        result += '_';
      }
    } else {
      // Subsequent characters can be letters, digits, or underscores
      if (ch == '_' || isUnicodeIdentifierPart(ch.unicode())) {
        result += ch;
      } else {
        // Replace invalid character with underscore
        result += '_';
      }
    }
  }

  // Ensure result contains at least one non-underscore character
  // A speaker name consisting only of underscores is not meaningful
  bool hasNonUnderscore = false;
  for (const QChar& c : result) {
    if (c != '_') {
      hasNonUnderscore = true;
      break;
    }
  }

  if (result.isEmpty() || !hasNonUnderscore) {
    return QStringLiteral("Narrator");
  }

  return result;
}

bool loadGraphLayout(QHash<QString, NMStoryGraphPanel::LayoutNode>& nodes, QString& entryScene) {
  nodes.clear();
  entryScene.clear();

  const QString path = graphLayoutPath();
  if (path.isEmpty()) {
    return false;
  }

  QFile file(path);
  if (!file.exists()) {
    return false;
  }
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }

  const QByteArray data = file.readAll();
  file.close();

  QJsonParseError error{};
  const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject()) {
    return false;
  }

  const QJsonObject root = doc.object();
  entryScene = root.value("entry").toString();

  const QJsonArray nodeArray = root.value("nodes").toArray();
  for (const auto& value : nodeArray) {
    const QJsonObject obj = value.toObject();
    const QString id = obj.value("id").toString();
    if (id.isEmpty()) {
      continue;
    }
    NMStoryGraphPanel::LayoutNode node;
    node.position = QPointF(obj.value("x").toDouble(), obj.value("y").toDouble());
    node.type = obj.value("type").toString();
    node.scriptPath = obj.value("scriptPath").toString();
    node.title = obj.value("title").toString();
    node.speaker = obj.value("speaker").toString();
    node.dialogueText = obj.value("dialogueText").toString();
    if (node.dialogueText.isEmpty()) {
      node.dialogueText = obj.value("text").toString();
    }
    const QJsonArray choicesArray = obj.value("choices").toArray();
    for (const auto& choiceValue : choicesArray) {
      const QString choice = choiceValue.toString();
      if (!choice.isEmpty()) {
        node.choices.push_back(choice);
      }
    }

    // Scene Node specific properties
    node.sceneId = obj.value("sceneId").toString();
    node.hasEmbeddedDialogue = obj.value("hasEmbeddedDialogue").toBool(false);
    node.dialogueCount = obj.value("dialogueCount").toInt(0);
    node.thumbnailPath = obj.value("thumbnailPath").toString();
    // Animation data integration
    node.animationDataPath = obj.value("animationDataPath").toString();
    node.hasAnimationData = obj.value("hasAnimationData").toBool(false);
    node.animationTrackCount = obj.value("animationTrackCount").toInt(0);

    // Condition Node specific properties
    node.conditionExpression = obj.value("conditionExpression").toString();
    const QJsonArray conditionOutputsArray = obj.value("conditionOutputs").toArray();
    for (const auto& outputValue : conditionOutputsArray) {
      const QString output = outputValue.toString();
      if (!output.isEmpty()) {
        node.conditionOutputs.push_back(output);
      }
    }

    // Choice branching mappings
    const QJsonObject choiceTargetsObj = obj.value("choiceTargets").toObject();
    for (auto it = choiceTargetsObj.begin(); it != choiceTargetsObj.end(); ++it) {
      node.choiceTargets.insert(it.key(), it.value().toString());
    }

    // Condition branching mappings
    const QJsonObject conditionTargetsObj = obj.value("conditionTargets").toObject();
    for (auto it = conditionTargetsObj.begin(); it != conditionTargetsObj.end(); ++it) {
      node.conditionTargets.insert(it.key(), it.value().toString());
    }

    nodes.insert(id, node);
  }

  return true;
}

void saveGraphLayout(const QHash<QString, NMStoryGraphPanel::LayoutNode>& nodes,
                     const QString& entryScene) {
  const QString path = graphLayoutPath();
  if (path.isEmpty()) {
    return;
  }

  ensureGraphLayoutDir();

  QJsonObject root;
  if (!entryScene.isEmpty()) {
    root.insert("entry", entryScene);
  }

  QJsonArray nodeArray;
  for (auto it = nodes.constBegin(); it != nodes.constEnd(); ++it) {
    QJsonObject obj;
    obj.insert("id", it.key());
    obj.insert("x", it.value().position.x());
    obj.insert("y", it.value().position.y());
    if (!it.value().type.isEmpty()) {
      obj.insert("type", it.value().type);
    }
    if (!it.value().scriptPath.isEmpty()) {
      obj.insert("scriptPath", it.value().scriptPath);
    }
    if (!it.value().title.isEmpty()) {
      obj.insert("title", it.value().title);
    }
    if (!it.value().speaker.isEmpty()) {
      obj.insert("speaker", it.value().speaker);
    }
    if (!it.value().dialogueText.isEmpty()) {
      obj.insert("dialogueText", it.value().dialogueText);
    }
    if (!it.value().choices.isEmpty()) {
      QJsonArray choicesArray;
      for (const auto& choice : it.value().choices) {
        choicesArray.append(choice);
      }
      obj.insert("choices", choicesArray);
    }

    // Scene Node specific properties
    if (!it.value().sceneId.isEmpty()) {
      obj.insert("sceneId", it.value().sceneId);
    }
    if (it.value().hasEmbeddedDialogue) {
      obj.insert("hasEmbeddedDialogue", it.value().hasEmbeddedDialogue);
    }
    if (it.value().dialogueCount > 0) {
      obj.insert("dialogueCount", it.value().dialogueCount);
    }
    if (!it.value().thumbnailPath.isEmpty()) {
      obj.insert("thumbnailPath", it.value().thumbnailPath);
    }
    // Animation data integration
    if (!it.value().animationDataPath.isEmpty()) {
      obj.insert("animationDataPath", it.value().animationDataPath);
    }
    if (it.value().hasAnimationData) {
      obj.insert("hasAnimationData", it.value().hasAnimationData);
    }
    if (it.value().animationTrackCount > 0) {
      obj.insert("animationTrackCount", it.value().animationTrackCount);
    }

    // Condition Node specific properties
    if (!it.value().conditionExpression.isEmpty()) {
      obj.insert("conditionExpression", it.value().conditionExpression);
    }
    if (!it.value().conditionOutputs.isEmpty()) {
      QJsonArray conditionOutputsArray;
      for (const auto& output : it.value().conditionOutputs) {
        conditionOutputsArray.append(output);
      }
      obj.insert("conditionOutputs", conditionOutputsArray);
    }

    // Choice branching mappings
    if (!it.value().choiceTargets.isEmpty()) {
      QJsonObject choiceTargetsObj;
      for (auto ct = it.value().choiceTargets.constBegin();
           ct != it.value().choiceTargets.constEnd(); ++ct) {
        choiceTargetsObj.insert(ct.key(), ct.value());
      }
      obj.insert("choiceTargets", choiceTargetsObj);
    }

    // Condition branching mappings
    if (!it.value().conditionTargets.isEmpty()) {
      QJsonObject conditionTargetsObj;
      for (auto ct = it.value().conditionTargets.constBegin();
           ct != it.value().conditionTargets.constEnd(); ++ct) {
        conditionTargetsObj.insert(ct.key(), ct.value());
      }
      obj.insert("conditionTargets", conditionTargetsObj);
    }

    nodeArray.push_back(obj);
  }
  root.insert("nodes", nodeArray);

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return;
  }
  file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  file.close();
}

QString resolveScriptPath(const NMGraphNodeItem* node) {
  if (!node) {
    return {};
  }
  QString scriptPath = node->scriptPath();
  if (scriptPath.isEmpty()) {
    return {};
  }
  QFileInfo info(scriptPath);
  if (info.isRelative()) {
    return QString::fromStdString(
        ProjectManager::instance().toAbsolutePath(scriptPath.toStdString()));
  }
  return scriptPath;
}

bool updateSceneGraphBlock(const QString& sceneId, const QString& scriptPath,
                           const QStringList& targets) {
  if (sceneId.isEmpty() || scriptPath.isEmpty()) {
    return false;
  }

  QFile file(scriptPath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }
  QString content = QString::fromUtf8(file.readAll());
  file.close();

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

  const QRegularExpression graphRe("//\\s*@graph-begin[\\s\\S]*?//\\s*@graph-end");
  const bool hasGraphBlock = body.contains(graphRe);

  if (targets.isEmpty()) {
    if (hasGraphBlock) {
      body.replace(graphRe, QString());
    } else {
      return true;
    }
  } else {
    const QString block = buildGraphBlock(targets);
    if (hasGraphBlock) {
      body.replace(graphRe, block);
    } else {
      if (!body.endsWith('\n') && !body.trimmed().isEmpty()) {
        body.append('\n');
      }
      body.append('\n');
      body.append(block);
      body.append('\n');
    }
  }

  QString updated = content.left(bodyStart);
  updated += body;
  updated += content.mid(bodyEnd);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
    return false;
  }
  file.write(updated.toUtf8());
  file.close();
  return true;
}

bool updateSceneSayStatement(const QString& sceneId, const QString& scriptPath,
                             const QString& speaker, const QString& text) {
  if (sceneId.isEmpty() || scriptPath.isEmpty()) {
    return false;
  }

  // Skip if text is empty or is the default placeholder "New scene"
  if (text.isEmpty() || text.trimmed() == "New scene") {
    return true; // Not an error, just nothing to update
  }

  QFile file(scriptPath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }
  QString content = QString::fromUtf8(file.readAll());
  file.close();

  // Find the scene block
  // Use Unicode-aware pattern to support Cyrillic and other non-ASCII scene IDs
  const QRegularExpression sceneRe(
      QString("\\bscene\\s+%1(?![\\p{L}\\p{N}_])").arg(QRegularExpression::escape(sceneId)),
      QRegularExpression::UseUnicodePropertiesOption);
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
  // Pattern: say <speaker> "<text>" OR say "<text>"
  // We need to match say statements with or without speaker
  // Use Unicode-aware pattern to match Cyrillic and other non-ASCII identifiers
  const QRegularExpression sayRe("\\bsay\\s+(?:([\\p{L}_][\\p{L}\\p{N}_]*)\\s+)?\"([^\"]*)\"",
                                 QRegularExpression::UseUnicodePropertiesOption);

  QRegularExpressionMatch sayMatch = sayRe.match(body);

  // Escape special characters in the text for NMScript string literals
  // Order matters: escape backslash first, then other characters
  QString escapedText = text;
  escapedText.replace("\\", "\\\\"); // Must be first
  escapedText.replace("\"", "\\\"");
  escapedText.replace("\n", "\\n"); // Newlines
  escapedText.replace("\r", "\\r"); // Carriage returns
  escapedText.replace("\t", "\\t"); // Tabs

  // Sanitize the speaker name to be a valid NMScript identifier (issue #92)
  // This prevents runtime errors like "Undefined character 'rfsfsddsf' [E3001]"
  const QString speakerToUse = sanitizeSpeakerIdentifier(speaker);

  if (!sayMatch.hasMatch()) {
    // No say statement found in the scene - add one at the beginning
    // But first check if the body already has this exact content to avoid
    // duplication
    const QString newSay = QString("\n    say %1 \"%2\"").arg(speakerToUse, escapedText);

    // Check if this say statement already exists to prevent duplication
    if (!body.contains(QString("say %1 \"%2\"").arg(speakerToUse, escapedText),
                       Qt::CaseSensitive)) {
      body = newSay + body;
    }
  } else {
    // Replace the existing say statement
    const QString newSay = QString("say %1 \"%2\"").arg(speakerToUse, escapedText);

    // Only replace if it's actually different to prevent redundant writes
    const QString existingSay = sayMatch.captured(0);
    if (existingSay != newSay) {
      body.replace(sayMatch.capturedStart(), sayMatch.capturedLength(), newSay);
    }
  }

  QString updated = content.left(bodyStart);
  updated += body;
  updated += content.mid(bodyEnd);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
    return false;
  }
  file.write(updated.toUtf8());
  file.close();
  return true;
}

QStringList splitChoiceLines(const QString& raw) {
  const QStringList lines = raw.split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts);
  QStringList filtered;
  filtered.reserve(lines.size());
  for (const QString& line : lines) {
    const QString trimmed = line.trimmed();
    if (!trimmed.isEmpty()) {
      filtered.push_back(trimmed);
    }
  }
  return filtered;
}

NMStoryGraphPanel::LayoutNode buildLayoutFromNode(const NMGraphNodeItem* node) {
  NMStoryGraphPanel::LayoutNode layout;
  if (!node) {
    return layout;
  }
  layout.position = node->pos();
  layout.type = node->nodeType();
  layout.scriptPath = node->scriptPath();
  layout.title = node->title();
  layout.speaker = node->dialogueSpeaker();
  layout.dialogueText = node->dialogueText();
  layout.choices = node->choiceOptions();

  // Scene Node specific properties
  layout.sceneId = node->sceneId();
  layout.hasEmbeddedDialogue = node->hasEmbeddedDialogue();
  layout.dialogueCount = node->dialogueCount();
  layout.thumbnailPath = node->thumbnailPath();

  // Condition Node specific properties
  layout.conditionExpression = node->conditionExpression();
  layout.conditionOutputs = node->conditionOutputs();

  // Branching mappings
  layout.choiceTargets = node->choiceTargets();
  layout.conditionTargets = node->conditionTargets();

  return layout;
}

// ============================================================================
// NMScript Generator (Graph -> Script conversion) - Issue #127
// ============================================================================

QString escapeNMScriptString(const QString& str) {
  QString escaped = str;
  escaped.replace("\\", "\\\\"); // Must be first
  escaped.replace("\"", "\\\"");
  escaped.replace("\n", "\\n");
  escaped.replace("\r", "\\r");
  escaped.replace("\t", "\\t");
  return escaped;
}

QString generateSceneBlock(const NMGraphNodeItem* node) {
  if (!node) {
    return {};
  }

  QString block;
  QTextStream out(&block);

  const QString sceneId = node->nodeIdString();
  const QString nodeType = node->nodeType();

  out << "scene " << sceneId << " {\n";

  // Generate content based on node type
  if (node->isDialogueNode()) {
    const QString speaker = sanitizeSpeakerIdentifier(node->dialogueSpeaker());
    const QString text = node->dialogueText();

    if (!text.isEmpty() && text.trimmed() != "New scene") {
      out << "    say " << speaker << " \"" << escapeNMScriptString(text) << "\"\n";
    }
  } else if (node->isChoiceNode()) {
    const QStringList& choices = node->choiceOptions();
    const auto& targets = node->choiceTargets();

    if (!choices.isEmpty()) {
      out << "    choice {\n";
      for (const QString& choice : choices) {
        QString target = targets.value(choice);
        if (target.isEmpty()) {
          // Use choice text as label if no explicit target
          target = sceneId + "_choice";
        }
        out << "        \"" << escapeNMScriptString(choice) << "\" -> goto " << target << "\n";
      }
      out << "    }\n";
    }
  } else if (node->isConditionNode()) {
    const QString expr = node->conditionExpression();
    const QStringList& outputs = node->conditionOutputs();
    const auto& targets = node->conditionTargets();

    if (!expr.isEmpty()) {
      out << "    if " << expr << " {\n";
      if (!outputs.isEmpty() && outputs.size() >= 2) {
        QString trueTarget = targets.value("true", targets.value(outputs[0]));
        out << "        goto " << trueTarget << "\n";
        out << "    } else {\n";
        QString falseTarget = targets.value("false", targets.value(outputs[1]));
        out << "        goto " << falseTarget << "\n";
        out << "    }\n";
      } else {
        out << "        // Condition branches\n";
        out << "    }\n";
      }
    } else {
      out << "    // Condition node - add condition expression\n";
    }
  } else if (node->isSceneNode()) {
    // Scene nodes might have embedded dialogue
    if (node->hasEmbeddedDialogue()) {
      const QString speaker = sanitizeSpeakerIdentifier(node->dialogueSpeaker());
      const QString text = node->dialogueText();
      if (!text.isEmpty()) {
        out << "    say " << speaker << " \"" << escapeNMScriptString(text) << "\"\n";
      }
    } else {
      out << "    // Scene: " << node->title() << "\n";
    }
  } else {
    // Generic node with dialogue if available
    const QString speaker = sanitizeSpeakerIdentifier(node->dialogueSpeaker());
    const QString text = node->dialogueText();
    if (!text.isEmpty() && text.trimmed() != "New scene") {
      out << "    say " << speaker << " \"" << escapeNMScriptString(text) << "\"\n";
    }
  }

  out << "}\n";
  return block;
}

QString generateNMScriptFromNodes(const QList<NMGraphNodeItem*>& nodes, const QString& entryScene) {
  QString script;
  QTextStream out(&script);

  // Header comment
  out << "// ========================================\n";
  out << "// Generated from Story Graph\n";
  out << "// Generated: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
  out << "// Do not edit manually - changes may be overwritten\n";
  out << "// ========================================\n\n";

  // Entry point marker (not NMScript syntax, but useful comment)
  if (!entryScene.isEmpty()) {
    out << "// Entry point: " << entryScene << "\n\n";
  }

  // Collect unique speakers for character declarations
  QSet<QString> speakers;
  for (const auto* node : nodes) {
    if (node && !node->dialogueSpeaker().isEmpty()) {
      speakers.insert(sanitizeSpeakerIdentifier(node->dialogueSpeaker()));
    }
  }

  // Generate character declarations
  if (!speakers.isEmpty()) {
    out << "// Character declarations\n";
    for (const QString& speaker : speakers) {
      if (speaker != "Narrator") {
        out << "character " << speaker << "(name=\"" << speaker << "\")\n";
      }
    }
    out << "character Narrator(name=\"\", color=\"#AAAAAA\")\n";
    out << "\n";
  }

  // Generate scene blocks
  for (const auto* node : nodes) {
    if (node) {
      out << generateSceneBlock(node) << "\n";
    }
  }

  return script;
}

namespace {

/// Attempts to write script content to a file.
/// @return Empty QString on success, error message on failure
QString tryWriteScriptToFile(const QString& scriptContent, const QString& filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
    return file.errorString();
  }

  if (file.write(scriptContent.toUtf8()) == -1) {
    QString error = file.errorString();
    file.close();
    return error;
  }

  file.close();
  return QString(); // Success
}

} // namespace

bool writeGeneratedScript(const QString& scriptContent, const QString& filename) {
  const auto& pm = ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    return false;
  }

  const QString generatedPath =
      QString::fromStdString(pm.getFolderPath(ProjectFolder::ScriptsGenerated));

  if (generatedPath.isEmpty()) {
    return false;
  }

  // Ensure directory exists
  QDir dir(generatedPath);
  if (!dir.exists()) {
    if (!dir.mkpath(".")) {
      qWarning() << "[StoryGraph] Failed to create directory:" << generatedPath;
      NMMessageDialog::showError(nullptr, QObject::tr("Save Failed"),
                                 QObject::tr("Failed to create directory:\n%1").arg(generatedPath));
      return false;
    }
  }

  const QString filePath = dir.filePath(filename);

  // Attempt to write the file
  QString error = tryWriteScriptToFile(scriptContent, filePath);

  // If write succeeded, log and return success
  if (error.isEmpty()) {
    qDebug() << "[StoryGraph] Generated script written to:" << filePath;
    return true;
  }

  // Write failed - show error dialog with retry and save-as options
  qWarning() << "[StoryGraph] Failed to write generated script:" << filePath << error;

  while (true) {
    const auto choice = NMMessageDialog::showQuestion(
        nullptr, QObject::tr("Save Generated Script Failed"),
        QObject::tr("Failed to save generated script to:\n%1\n\n"
                    "Error: %2\n\n"
                    "What would you like to do?")
            .arg(filePath, error),
        {NMDialogButton::Save, NMDialogButton::Cancel}, NMDialogButton::Save);

    if (choice == NMDialogButton::Cancel) {
      return false;
    }

    if (choice == NMDialogButton::Save) {
      // Show file dialog to select alternative location
      const QString altPath =
          NMFileDialog::getSaveFileName(nullptr, QObject::tr("Save Generated Script"), filePath,
                                        QObject::tr("NMScript Files (*.nms);;All Files (*)"));

      if (altPath.isEmpty()) {
        // User cancelled the file dialog
        continue;
      }

      // Ensure the directory exists for the alternative path
      QFileInfo altFileInfo(altPath);
      QDir altDir = altFileInfo.absoluteDir();
      if (!altDir.exists()) {
        if (!altDir.mkpath(".")) {
          NMMessageDialog::showError(
              nullptr, QObject::tr("Save Failed"),
              QObject::tr("Failed to create directory:\n%1").arg(altDir.absolutePath()));
          continue;
        }
      }

      // Try writing to alternative location
      QString altError = tryWriteScriptToFile(scriptContent, altPath);
      if (altError.isEmpty()) {
        qDebug() << "[StoryGraph] Generated script written to alternative "
                    "location:"
                 << altPath;
        NMMessageDialog::showInfo(nullptr, QObject::tr("Save Successful"),
                                  QObject::tr("Generated script saved to:\n%1").arg(altPath));
        return true;
      }

      // Alternative location also failed
      error = altError;
      // Loop will show the error dialog again with the new error message
      NMMessageDialog::showError(
          nullptr, QObject::tr("Save Failed"),
          QObject::tr("Failed to save to:\n%1\n\nError: %2").arg(altPath, altError));
    }
  }
}

// ============================================================================
// NMScript Parser (Script -> Graph conversion) - Issue #127
// ============================================================================

ParseResult parseNMScriptContent(const QString& content) {
  ParseResult result;
  result.success = true;

  if (content.isEmpty()) {
    result.success = false;
    result.errorMessage = "Empty content";
    return result;
  }

  // Split content into lines for line number tracking
  const QStringList lines = content.split('\n');

  // Use Unicode-aware patterns matching the existing codebase style
  const QRegularExpression sceneRe("\\bscene\\s+([\\p{L}_][\\p{L}\\p{N}_]*)\\s*\\{",
                                   QRegularExpression::UseUnicodePropertiesOption);

  const QRegularExpression sayRe("\\bsay\\s+([\\p{L}_][\\p{L}\\p{N}_]*)\\s+\"([^\"]*)\"",
                                 QRegularExpression::UseUnicodePropertiesOption);

  const QRegularExpression gotoRe("\\bgoto\\s+([\\p{L}_][\\p{L}\\p{N}_]*)",
                                  QRegularExpression::UseUnicodePropertiesOption);

  const QRegularExpression choiceBlockRe("\\bchoice\\s*\\{([^}]*)\\}",
                                         QRegularExpression::UseUnicodePropertiesOption |
                                             QRegularExpression::DotMatchesEverythingOption);

  const QRegularExpression choiceOptionRe(
      "\"([^\"]+)\"\\s*(?:if\\s+[^-]+)?->\\s*(?:goto\\s+)?([\\p{L}_][\\p{L}\\p{N}_]*)",
      QRegularExpression::UseUnicodePropertiesOption);

  const QRegularExpression ifRe("\\bif\\s+([^{]+)\\s*\\{",
                                QRegularExpression::UseUnicodePropertiesOption);

  // Track scene blocks
  struct SceneBlock {
    QString id;
    qsizetype start;
    qsizetype end;
    int lineNumber;
  };
  QVector<SceneBlock> sceneBlocks;

  // First pass: find all scene blocks with their ranges
  QRegularExpressionMatchIterator sceneIt = sceneRe.globalMatch(content);
  while (sceneIt.hasNext()) {
    const QRegularExpressionMatch match = sceneIt.next();
    SceneBlock block;
    block.id = match.captured(1);
    block.start = match.capturedStart();

    // Calculate line number
    block.lineNumber = 1;
    for (qsizetype i = 0; i < block.start && i < content.size(); ++i) {
      if (content[i] == '\n') {
        ++block.lineNumber;
      }
    }

    sceneBlocks.push_back(block);
  }

  // Calculate end positions for each block
  for (int i = 0; i < sceneBlocks.size(); ++i) {
    if (i + 1 < sceneBlocks.size()) {
      sceneBlocks[i].end = sceneBlocks[i + 1].start;
    } else {
      sceneBlocks[i].end = content.size();
    }
  }

  // Second pass: parse each scene block
  for (const auto& block : sceneBlocks) {
    ParsedNode node;
    node.id = block.id;
    node.sourceLineNumber = block.lineNumber;

    // Extract scene body
    const qsizetype bracePos = content.indexOf('{', block.start);
    if (bracePos < 0 || bracePos >= block.end) {
      continue;
    }

    // Find matching closing brace
    int depth = 0;
    bool inString = false;
    QChar stringDelim;
    qsizetype bodyEnd = block.end;

    for (qsizetype i = bracePos; i < block.end; ++i) {
      const QChar c = content.at(i);

      if (c == '"' || c == '\'') {
        if (!inString) {
          inString = true;
          stringDelim = c;
        } else if (c == stringDelim && (i == 0 || content.at(i - 1) != '\\')) {
          inString = false;
        }
      }

      if (!inString) {
        if (c == '{') {
          ++depth;
        } else if (c == '}') {
          --depth;
          if (depth == 0) {
            bodyEnd = i;
            break;
          }
        }
      }
    }

    const QString body = content.mid(bracePos + 1, bodyEnd - bracePos - 1);

    // Check for say statements
    QRegularExpressionMatchIterator sayIt = sayRe.globalMatch(body);
    if (sayIt.hasNext()) {
      const QRegularExpressionMatch sayMatch = sayIt.next();
      node.speaker = sayMatch.captured(1);
      node.text = sayMatch.captured(2);
      // Unescape the text
      node.text.replace("\\n", "\n");
      node.text.replace("\\t", "\t");
      node.text.replace("\\\"", "\"");
      node.text.replace("\\\\", "\\");
      node.type = "Dialogue";
    }

    // Check for choice blocks
    QRegularExpressionMatch choiceMatch = choiceBlockRe.match(body);
    if (choiceMatch.hasMatch()) {
      const QString choiceContent = choiceMatch.captured(1);
      QRegularExpressionMatchIterator optionIt = choiceOptionRe.globalMatch(choiceContent);

      while (optionIt.hasNext()) {
        const QRegularExpressionMatch optMatch = optionIt.next();
        node.choices.append(optMatch.captured(1));
        node.targets.append(optMatch.captured(2));
        result.edges.append({block.id, optMatch.captured(2)});
      }

      if (!node.choices.isEmpty()) {
        node.type = "Choice";
      }
    }

    // Check for if statements (conditions)
    QRegularExpressionMatch ifMatch = ifRe.match(body);
    if (ifMatch.hasMatch() && node.type.isEmpty()) {
      node.conditionExpr = ifMatch.captured(1).trimmed();
      node.type = "Condition";
      node.conditionOutputs << "true"
                            << "false";
    }

    // Check for goto statements (for edges)
    QRegularExpressionMatchIterator gotoIt = gotoRe.globalMatch(body);
    while (gotoIt.hasNext()) {
      const QRegularExpressionMatch gotoMatch = gotoIt.next();
      const QString target = gotoMatch.captured(1);
      if (!node.targets.contains(target)) {
        node.targets.append(target);
        result.edges.append({block.id, target});
      }
    }

    // Default to Scene type if no specific type detected
    if (node.type.isEmpty()) {
      node.type = "Scene";
    }

    result.nodes.append(node);
  }

  // If first node is identified, set it as entry point
  if (!result.nodes.isEmpty()) {
    result.entryPoint = result.nodes.first().id;
  }

  return result;
}

ParseResult parseNMScriptFile(const QString& scriptPath) {
  ParseResult result;

  QFile file(scriptPath);
  if (!file.exists()) {
    result.success = false;
    result.errorMessage = QString("File not found: %1").arg(scriptPath);
    return result;
  }

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    result.success = false;
    result.errorMessage = QString("Cannot open file: %1").arg(file.errorString());
    return result;
  }

  const QString content = QString::fromUtf8(file.readAll());
  file.close();

  return parseNMScriptContent(content);
}

} // namespace NovelMind::editor::qt::detail
