#include "NovelMind/editor/qt/qt_selection_manager.hpp"
#include "NovelMind/editor/qt/qt_event_bus.hpp"
#include <QMutexLocker>

namespace NovelMind::editor::qt {

QtSelectionManager& QtSelectionManager::instance() {
  static QtSelectionManager instance;
  return instance;
}

QtSelectionManager::QtSelectionManager() : QObject(nullptr) {}

void QtSelectionManager::select(const QString& id, SelectionType type) {
  QMutexLocker locker(&m_mutex);

  if (m_selectedIds.size() == 1 && m_selectedIds.first() == id && m_currentType == type) {
    return; // Already selected
  }

  m_selectedIds.clear();
  m_selectedIds.append(id);
  m_currentType = type;

  notifySelectionChanged();
}

void QtSelectionManager::selectMultiple(const QStringList& ids, SelectionType type) {
  QMutexLocker locker(&m_mutex);

  m_selectedIds = ids;
  m_currentType = ids.isEmpty() ? SelectionType::None : type;

  notifySelectionChanged();
}

void QtSelectionManager::addToSelection(const QString& id, SelectionType type) {
  QMutexLocker locker(&m_mutex);

  // Can only add items of the same type
  if (!m_selectedIds.isEmpty() && m_currentType != type) {
    // Clear and start new selection of this type
    m_selectedIds.clear();
    m_currentType = type;
  }

  if (!m_selectedIds.contains(id)) {
    m_selectedIds.append(id);
    m_currentType = type;
    notifySelectionChanged();
  } else if (m_selectedIds.isEmpty()) {
    // We just cleared due to type mismatch, so we need to notify
    // even though the id wasn't in the old list
    m_selectedIds.append(id);
    notifySelectionChanged();
  }
}

void QtSelectionManager::removeFromSelection(const QString& id) {
  QMutexLocker locker(&m_mutex);

  if (m_selectedIds.removeOne(id)) {
    if (m_selectedIds.isEmpty()) {
      m_currentType = SelectionType::None;
    }
    notifySelectionChanged();
  }
}

void QtSelectionManager::toggleSelection(const QString& id, SelectionType type) {
  QMutexLocker locker(&m_mutex);

  if (m_selectedIds.contains(id)) {
    // Need to unlock before calling removeFromSelection to avoid deadlock
    locker.unlock();
    removeFromSelection(id);
  } else {
    // Need to unlock before calling addToSelection to avoid deadlock
    locker.unlock();
    addToSelection(id, type);
  }
}

void QtSelectionManager::clearSelection() {
  QMutexLocker locker(&m_mutex);

  if (m_selectedIds.isEmpty())
    return;

  m_selectedIds.clear();
  m_currentType = SelectionType::None;

  emit selectionCleared();
  notifySelectionChanged();
}

bool QtSelectionManager::hasSelection() const {
  QMutexLocker locker(&m_mutex);
  return !m_selectedIds.isEmpty();
}

SelectionType QtSelectionManager::currentSelectionType() const {
  QMutexLocker locker(&m_mutex);
  return m_currentType;
}

QStringList QtSelectionManager::selectedIds() const {
  QMutexLocker locker(&m_mutex);
  return m_selectedIds;
}

QString QtSelectionManager::primarySelection() const {
  QMutexLocker locker(&m_mutex);
  return m_selectedIds.isEmpty() ? QString() : m_selectedIds.first();
}

int QtSelectionManager::selectionCount() const {
  QMutexLocker locker(&m_mutex);
  return static_cast<int>(m_selectedIds.size());
}

bool QtSelectionManager::isSelected(const QString& id) const {
  QMutexLocker locker(&m_mutex);
  return m_selectedIds.contains(id);
}

void QtSelectionManager::notifySelectionChanged() {
  emit selectionChanged(m_selectedIds, m_currentType);

  if (!m_selectedIds.isEmpty()) {
    emit primarySelectionChanged(m_selectedIds.first(), m_currentType);
  }

  // Also notify via event bus for non-Qt subscribers
  QString typeStr;
  switch (m_currentType) {
  case SelectionType::SceneObject:
    typeStr = "SceneObject";
    break;
  case SelectionType::GraphNode:
    typeStr = "GraphNode";
    break;
  case SelectionType::TimelineItem:
    typeStr = "TimelineItem";
    break;
  case SelectionType::Asset:
    typeStr = "Asset";
    break;
  case SelectionType::HierarchyItem:
    typeStr = "HierarchyItem";
    break;
  default:
    typeStr = "None";
    break;
  }

  QtEventBus::instance().publishSelectionChanged(m_selectedIds, typeStr);
}

} // namespace NovelMind::editor::qt
