#include "NovelMind/editor/scene_document.hpp"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace NovelMind::editor {

namespace {

std::string toStdString(const QString& value) {
  return value.toStdString();
}

QString toQString(const std::string& value) {
  return QString::fromStdString(value);
}

} // namespace

Result<SceneDocument> loadSceneDocument(const std::string& path) {
  QFile file(QString::fromStdString(path));
  if (!file.exists()) {
    return Result<SceneDocument>::error("Scene document not found: " + path);
  }
  if (!file.open(QIODevice::ReadOnly)) {
    return Result<SceneDocument>::error("Failed to open scene document: " + path);
  }

  const QByteArray data = file.readAll();
  file.close();

  QJsonParseError error{};
  const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject()) {
    return Result<SceneDocument>::error("Invalid scene document JSON: " + path);
  }

  const QJsonObject root = doc.object();
  SceneDocument scene;
  scene.sceneId = toStdString(root.value("sceneId").toString());

  const QJsonArray objects = root.value("objects").toArray();
  scene.objects.reserve(static_cast<size_t>(objects.size()));
  for (const auto& value : objects) {
    const QJsonObject obj = value.toObject();
    SceneDocumentObject item;
    item.id = toStdString(obj.value("id").toString());
    item.name = toStdString(obj.value("name").toString());
    item.type = toStdString(obj.value("type").toString());
    item.x = static_cast<float>(obj.value("x").toDouble());
    item.y = static_cast<float>(obj.value("y").toDouble());
    item.rotation = static_cast<float>(obj.value("rotation").toDouble());
    item.scaleX = static_cast<float>(obj.value("scaleX").toDouble(1.0));
    item.scaleY = static_cast<float>(obj.value("scaleY").toDouble(1.0));
    item.alpha = static_cast<float>(obj.value("alpha").toDouble(1.0));
    item.visible = obj.value("visible").toBool(true);
    item.zOrder = obj.value("zOrder").toInt();

    const QJsonObject props = obj.value("properties").toObject();
    for (auto it = props.begin(); it != props.end(); ++it) {
      item.properties.emplace(toStdString(it.key()), toStdString(it.value().toString()));
    }
    scene.objects.push_back(std::move(item));
  }

  return Result<SceneDocument>::ok(std::move(scene));
}

Result<void> saveSceneDocument(const SceneDocument& doc, const std::string& path) {
  QJsonObject root;
  root.insert("sceneId", toQString(doc.sceneId));

  QJsonArray objects;
  for (const auto& item : doc.objects) {
    QJsonObject obj;
    obj.insert("id", toQString(item.id));
    obj.insert("name", toQString(item.name));
    obj.insert("type", toQString(item.type));
    obj.insert("x", item.x);
    obj.insert("y", item.y);
    obj.insert("rotation", item.rotation);
    obj.insert("scaleX", item.scaleX);
    obj.insert("scaleY", item.scaleY);
    obj.insert("alpha", item.alpha);
    obj.insert("visible", item.visible);
    obj.insert("zOrder", item.zOrder);

    QJsonObject props;
    for (const auto& kv : item.properties) {
      props.insert(toQString(kv.first), toQString(kv.second));
    }
    obj.insert("properties", props);
    objects.push_back(obj);
  }
  root.insert("objects", objects);

  QFile file(QString::fromStdString(path));
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return Result<void>::error("Failed to write scene document: " + path);
  }

  file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  file.close();
  return Result<void>::ok();
}

} // namespace NovelMind::editor
