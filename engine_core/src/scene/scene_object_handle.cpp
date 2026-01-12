/**
 * @file scene_object_handle.cpp
 * @brief Implementation of RAII-safe scene object handle
 *
 * Thread-safe implementation using generation counter pattern to prevent
 * TOCTOU (Time-Of-Check-Time-Of-Use) race conditions.
 */

#include "NovelMind/scene/scene_object_handle.hpp"
#include "NovelMind/scene/scene_graph.hpp"

namespace NovelMind::scene {

SceneObjectHandle::SceneObjectHandle(SceneGraph *sceneGraph,
                                     const std::string &objectId)
    : m_sceneGraph(sceneGraph), m_objectId(objectId) {
  // Capture generation at construction time
  if (m_sceneGraph && !m_objectId.empty()) {
    std::lock_guard<std::mutex> lock(m_sceneGraph->getObjectMutex());
    if (auto *obj = m_sceneGraph->findObject(m_objectId)) {
      m_generation = obj->getGeneration();
    }
  }
}

bool SceneObjectHandle::isValid() const {
  if (!m_sceneGraph || m_objectId.empty()) {
    return false;
  }
  // Thread-safe validity check with generation counter
  std::lock_guard<std::mutex> lock(m_sceneGraph->getObjectMutex());
  return m_sceneGraph->findObjectWithGeneration(m_objectId, m_generation) !=
         nullptr;
}

SceneObjectBase *SceneObjectHandle::get() const {
  if (!m_sceneGraph || m_objectId.empty()) {
    return nullptr;
  }
  // Thread-safe get with generation validation
  // Note: This returns a pointer that is only valid while mutex is held
  // Callers should use withObject() for true thread-safety
  std::lock_guard<std::mutex> lock(m_sceneGraph->getObjectMutex());
  return m_sceneGraph->findObjectWithGeneration(m_objectId, m_generation);
}

bool SceneObjectHandle::withObject(
    std::function<void(SceneObjectBase *)> fn) const {
  if (!m_sceneGraph || m_objectId.empty() || !fn) {
    return false;
  }

  // Thread-safe atomic check-and-use pattern
  std::lock_guard<std::mutex> lock(m_sceneGraph->getObjectMutex());
  if (auto *obj =
          m_sceneGraph->findObjectWithGeneration(m_objectId, m_generation)) {
    fn(obj);
    return true;
  }
  return false;
}

void SceneObjectHandle::reset() {
  m_sceneGraph = nullptr;
  m_objectId.clear();
  m_generation = 0;
}

} // namespace NovelMind::scene
