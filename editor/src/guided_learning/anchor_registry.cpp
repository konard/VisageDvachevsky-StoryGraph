/**
 * @file anchor_registry.cpp
 * @brief Implementation of the UI Anchor Registry
 */

#include "NovelMind/editor/guided_learning/anchor_registry.hpp"
#include <QDebug>
#include <algorithm>

namespace NovelMind::editor::guided_learning {

// ============================================================================
// NMAnchorRegistry Implementation
// ============================================================================

NMAnchorRegistry& NMAnchorRegistry::instance() {
  static NMAnchorRegistry instance;
  return instance;
}

NMAnchorRegistry::NMAnchorRegistry() : QObject(nullptr) {
  // Set object name for debugging
  setObjectName("NMAnchorRegistry");
}

void NMAnchorRegistry::registerAnchor(const std::string& id, QWidget* widget,
                                      const std::string& description, const std::string& panelId) {
  if (!widget) {
    qWarning() << "NMAnchorRegistry: Cannot register anchor" << QString::fromStdString(id)
               << "with null widget";
    return;
  }

  std::lock_guard<std::mutex> lock(m_mutex);

  AnchorInfo info;
  info.id = id;
  info.panelId = panelId;
  info.description = description;
  info.widget = widget;

  // Create rect provider that uses the widget
  info.getRect = [weakWidget = QPointer<QWidget>(widget)]() -> QRect {
    QWidget* w = weakWidget.data();
    if (w) {
      return QRect(w->mapToGlobal(QPoint(0, 0)), w->size());
    }
    return QRect();
  };

  // Create visibility provider
  info.isVisible = [weakWidget = QPointer<QWidget>(widget)]() -> bool {
    QWidget* w = weakWidget.data();
    return w && w->isVisible();
  };

  m_anchors[id] = std::move(info);

  emit anchorRegistered(QString::fromStdString(id));
}

void NMAnchorRegistry::registerAnchor(const std::string& id, std::function<QRect()> rectProvider,
                                      std::function<bool()> visibilityProvider,
                                      const std::string& description, const std::string& panelId) {
  std::lock_guard<std::mutex> lock(m_mutex);

  AnchorInfo info;
  info.id = id;
  info.panelId = panelId;
  info.description = description;
  info.getRect = std::move(rectProvider);
  info.isVisible = std::move(visibilityProvider);

  m_anchors[id] = std::move(info);

  emit anchorRegistered(QString::fromStdString(id));
}

void NMAnchorRegistry::unregisterAnchor(const std::string& id) {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_anchors.find(id);
  if (it != m_anchors.end()) {
    m_anchors.erase(it);
    emit anchorUnregistered(QString::fromStdString(id));
  }
}

void NMAnchorRegistry::unregisterPanelAnchors(const std::string& panelId) {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::vector<std::string> toRemove;
  for (const auto& [id, info] : m_anchors) {
    if (info.panelId == panelId) {
      toRemove.push_back(id);
    }
  }

  for (const auto& id : toRemove) {
    m_anchors.erase(id);
    emit anchorUnregistered(QString::fromStdString(id));
  }
}

bool NMAnchorRegistry::hasAnchor(const std::string& id) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_anchors.find(id) != m_anchors.end();
}

std::optional<AnchorInfo> NMAnchorRegistry::getAnchor(const std::string& id) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_anchors.find(id);
  if (it != m_anchors.end()) {
    // Check if widget is still valid
    if (it->second.widget && it->second.widget.isNull()) {
      return std::nullopt;
    }
    return it->second;
  }
  return std::nullopt;
}

std::optional<QRect> NMAnchorRegistry::getAnchorRect(const std::string& id) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_anchors.find(id);
  if (it != m_anchors.end()) {
    // Check if widget is still valid
    if (it->second.widget && it->second.widget.isNull()) {
      return std::nullopt;
    }
    if (it->second.getRect) {
      return it->second.getRect();
    }
  }
  return std::nullopt;
}

bool NMAnchorRegistry::isAnchorVisible(const std::string& id) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_anchors.find(id);
  if (it != m_anchors.end()) {
    // Check if widget is still valid
    if (it->second.widget && it->second.widget.isNull()) {
      return false;
    }
    if (it->second.isVisible) {
      return it->second.isVisible();
    }
  }
  return false;
}

std::vector<std::string> NMAnchorRegistry::getAnchorsForPanel(const std::string& panelId) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::vector<std::string> result;
  for (const auto& [id, info] : m_anchors) {
    if (info.panelId == panelId) {
      result.push_back(id);
    }
  }
  return result;
}

std::vector<std::string> NMAnchorRegistry::getAllAnchorIds() const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::vector<std::string> result;
  result.reserve(m_anchors.size());
  for (const auto& [id, _] : m_anchors) {
    result.push_back(id);
  }
  return result;
}

void NMAnchorRegistry::debugDumpAnchors() const {
  std::lock_guard<std::mutex> lock(m_mutex);

  qDebug() << "=== Anchor Registry Dump ===";
  qDebug() << "Total anchors:" << m_anchors.size();

  for (const auto& [id, info] : m_anchors) {
    QString status;
    if (info.widget) {
      if (info.widget.isNull()) {
        status = "[DESTROYED]";
      } else if (info.isVisible && info.isVisible()) {
        status = "[VISIBLE]";
      } else {
        status = "[HIDDEN]";
      }
    } else {
      status = "[CUSTOM]";
    }

    qDebug() << "  " << QString::fromStdString(id) << status
             << "Panel:" << QString::fromStdString(info.panelId)
             << "Desc:" << QString::fromStdString(info.description);
  }
  qDebug() << "============================";
}

void NMAnchorRegistry::cleanupDestroyedWidgets() {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::vector<std::string> toRemove;
  for (const auto& [id, info] : m_anchors) {
    if (info.widget && info.widget.isNull()) {
      toRemove.push_back(id);
    }
  }

  for (const auto& id : toRemove) {
    m_anchors.erase(id);
    emit anchorUnregistered(QString::fromStdString(id));
  }
}

// ============================================================================
// ScopedAnchorRegistration Implementation
// ============================================================================

ScopedAnchorRegistration::ScopedAnchorRegistration(const std::string& anchorId, QWidget* widget,
                                                   const std::string& description,
                                                   const std::string& panelId)
    : m_anchorId(anchorId), m_registered(true) {
  NMAnchorRegistry::instance().registerAnchor(anchorId, widget, description, panelId);
}

ScopedAnchorRegistration::~ScopedAnchorRegistration() {
  if (m_registered && !m_anchorId.empty()) {
    NMAnchorRegistry::instance().unregisterAnchor(m_anchorId);
  }
}

ScopedAnchorRegistration::ScopedAnchorRegistration(ScopedAnchorRegistration&& other) noexcept
    : m_anchorId(std::move(other.m_anchorId)), m_registered(other.m_registered) {
  other.m_registered = false;
}

ScopedAnchorRegistration&
ScopedAnchorRegistration::operator=(ScopedAnchorRegistration&& other) noexcept {
  if (this != &other) {
    if (m_registered && !m_anchorId.empty()) {
      NMAnchorRegistry::instance().unregisterAnchor(m_anchorId);
    }
    m_anchorId = std::move(other.m_anchorId);
    m_registered = other.m_registered;
    other.m_registered = false;
  }
  return *this;
}

} // namespace NovelMind::editor::guided_learning
