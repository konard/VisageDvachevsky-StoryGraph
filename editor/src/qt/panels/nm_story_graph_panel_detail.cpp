#include "nm_story_graph_panel_detail.hpp"

#include "NovelMind/editor/project_manager.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace NovelMind::editor::qt::detail {
namespace {

constexpr const char *kGraphLayoutFile = ".novelmind/story_graph.json";

QString graphLayoutPath() {
  const auto &pm = ProjectManager::instance();
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

QString buildGraphBlock(const QStringList &targets) {
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
    for (const auto &target : targets) {
      lines << indent + "    " +
                   QString("\"%1\" -> goto %2").arg(target, target);
    }
    lines << indent + "}";
  }

  lines << indent + "// @graph-end";
  return lines.join("\n");
}

} // namespace

bool loadGraphLayout(QHash<QString, NMStoryGraphPanel::LayoutNode> &nodes,
                     QString &entryScene) {
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
  for (const auto &value : nodeArray) {
    const QJsonObject obj = value.toObject();
    const QString id = obj.value("id").toString();
    if (id.isEmpty()) {
      continue;
    }
    NMStoryGraphPanel::LayoutNode node;
    node.position = QPointF(obj.value("x").toDouble(),
                            obj.value("y").toDouble());
    node.type = obj.value("type").toString();
    node.scriptPath = obj.value("scriptPath").toString();
    node.title = obj.value("title").toString();
    node.speaker = obj.value("speaker").toString();
    node.dialogueText = obj.value("dialogueText").toString();
    if (node.dialogueText.isEmpty()) {
      node.dialogueText = obj.value("text").toString();
    }
    const QJsonArray choicesArray = obj.value("choices").toArray();
    for (const auto &choiceValue : choicesArray) {
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

    nodes.insert(id, node);
  }

  return true;
}

void saveGraphLayout(const QHash<QString, NMStoryGraphPanel::LayoutNode> &nodes,
                     const QString &entryScene) {
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
      for (const auto &choice : it.value().choices) {
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

QString resolveScriptPath(const NMGraphNodeItem *node) {
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

bool updateSceneGraphBlock(const QString &sceneId, const QString &scriptPath,
                           const QStringList &targets) {
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

  const QRegularExpression graphRe(
      "//\\s*@graph-begin[\\s\\S]*?//\\s*@graph-end");
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

QStringList splitChoiceLines(const QString &raw) {
  const QStringList lines =
      raw.split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts);
  QStringList filtered;
  filtered.reserve(lines.size());
  for (const QString &line : lines) {
    const QString trimmed = line.trimmed();
    if (!trimmed.isEmpty()) {
      filtered.push_back(trimmed);
    }
  }
  return filtered;
}

NMStoryGraphPanel::LayoutNode
buildLayoutFromNode(const NMGraphNodeItem *node) {
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

  return layout;
}

} // namespace NovelMind::editor::qt::detail
