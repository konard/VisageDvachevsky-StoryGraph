#pragma once

/**
 * @file scene_document.hpp
 * @brief Scene document serialization for editor/runtime preview
 */

#include "NovelMind/core/result.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace NovelMind::editor {

struct SceneDocumentObject {
  std::string id;
  std::string name;
  std::string type;
  float x = 0.0f;
  float y = 0.0f;
  float rotation = 0.0f;
  float scaleX = 1.0f;
  float scaleY = 1.0f;
  float alpha = 1.0f;
  bool visible = true;
  int zOrder = 0;
  std::unordered_map<std::string, std::string> properties;
};

struct SceneDocument {
  std::string sceneId;
  std::vector<SceneDocumentObject> objects;
};

Result<SceneDocument> loadSceneDocument(const std::string& path);
Result<void> saveSceneDocument(const SceneDocument& doc, const std::string& path);

} // namespace NovelMind::editor
