#pragma once

/**
 * @file inspector_validators.hpp
 * @brief Value validation and property update handling
 *
 * Handles property value changes, validation, and update batching
 */

#include <QHash>
#include <QPointF>
#include <QString>
#include <QStringList>

namespace NovelMind::editor::qt {

class NMInspectorPanel;

/**
 * @brief Handles property validation and updates
 */
class InspectorValidators {
public:
  explicit InspectorValidators(NMInspectorPanel* panel);

  /**
   * @brief Handle property value change from UI
   * @return True if change was handled successfully
   */
  bool handlePropertyChanged(const QString& propertyName, const QString& newValue,
                             const QString& currentObjectId, const QStringList& currentObjectIds,
                             bool multiEditMode, bool& lockAspectRatio, QPointF& lastScale);

  /**
   * @brief Update property widget value
   */
  void updatePropertyWidget(const QString& propertyName, const QString& newValue,
                            const QHash<QString, class QWidget*>& propertyWidgets);

  /**
   * @brief Flush pending updates to widgets (batched updates)
   */
  void flushPendingUpdates(const QHash<QString, QString>& pendingUpdates,
                           const QHash<QString, class QWidget*>& propertyWidgets);

  /**
   * @brief Queue a property update for batching
   * @return True if update was queued (value changed)
   */
  bool queuePropertyUpdate(const QString& propertyName, const QString& newValue,
                           QHash<QString, QString>& pendingUpdates,
                           QHash<QString, QString>& lastKnownValues);

private:
  NMInspectorPanel* m_panel = nullptr;

  // Handle specific property types
  bool handleResetButton(const QString& propertyName, const QString& currentObjectId);
  bool handleAspectRatioLock(const QString& propertyName, const QString& newValue,
                             const QString& currentObjectId, bool& lockAspectRatio,
                             QPointF& lastScale);
  void updateScaleTracking(const QString& propertyName, const QString& newValue,
                           QPointF& lastScale);
};

} // namespace NovelMind::editor::qt
