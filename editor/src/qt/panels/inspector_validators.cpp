#include "NovelMind/editor/qt/panels/inspector_validators.hpp"
#include "NovelMind/editor/qt/panels/nm_inspector_panel.hpp"
#include "NovelMind/editor/inspector_binding.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTextCursor>
#include <QWidget>

namespace NovelMind::editor::qt {

InspectorValidators::InspectorValidators(NMInspectorPanel* panel) : m_panel(panel) {}

bool InspectorValidators::handlePropertyChanged(const QString& propertyName,
                                                const QString& newValue,
                                                const QString& currentObjectId,
                                                const QStringList& currentObjectIds,
                                                bool multiEditMode, bool& lockAspectRatio,
                                                QPointF& lastScale) {
  // In multi-edit mode, apply changes through InspectorBindingManager
  if (multiEditMode) {
    auto& inspector = InspectorBindingManager::instance();
    auto error =
        inspector.setPropertyValueFromString(propertyName.toStdString(), newValue.toStdString());

    if (error.has_value()) {
      // ERR-1 fix: Show error to user via status bar instead of just logging
      QString errorMsg = QString::fromStdString(error.value());
      qWarning() << "Failed to set property:" << propertyName << "Error:" << errorMsg;
      // Emit signal for status bar or use inline notification
      emit m_panel->propertyError(propertyName, errorMsg);
    }
    return true;
  }

  // Handle reset button signals
  if (handleResetButton(propertyName, currentObjectId)) {
    return true;
  }

  // Handle aspect ratio lock for scale changes
  if (handleAspectRatioLock(propertyName, newValue, currentObjectId, lockAspectRatio, lastScale)) {
    return true;
  }

  // Update last scale tracking for non-locked changes
  updateScaleTracking(propertyName, newValue, lastScale);

  // Single-object mode: emit signal
  emit m_panel->propertyChanged(currentObjectId, propertyName, newValue);
  return true;
}

bool InspectorValidators::handleResetButton(const QString& propertyName,
                                            const QString& currentObjectId) {
  if (propertyName == "reset_position") {
    emit m_panel->propertyChanged(currentObjectId, "position_x", "0");
    emit m_panel->propertyChanged(currentObjectId, "position_y", "0");
    // Update UI spinboxes
    m_panel->updatePropertyValue("position_x", "0");
    m_panel->updatePropertyValue("position_y", "0");
    return true;
  }

  if (propertyName == "reset_rotation") {
    emit m_panel->propertyChanged(currentObjectId, "rotation", "0");
    m_panel->updatePropertyValue("rotation", "0");
    return true;
  }

  if (propertyName == "reset_scale") {
    emit m_panel->propertyChanged(currentObjectId, "scale_x", "1");
    emit m_panel->propertyChanged(currentObjectId, "scale_y", "1");
    m_panel->updatePropertyValue("scale_x", "1");
    m_panel->updatePropertyValue("scale_y", "1");
    return true;
  }

  return false;
}

bool InspectorValidators::handleAspectRatioLock(const QString& propertyName,
                                                const QString& newValue,
                                                const QString& currentObjectId,
                                                bool& lockAspectRatio, QPointF& lastScale) {
  if (!lockAspectRatio || (propertyName != "scale_x" && propertyName != "scale_y")) {
    return false;
  }

  double newScale = newValue.toDouble();

  // Calculate the ratio change
  if (propertyName == "scale_x" && lastScale.x() > 0.0001) {
    double ratio = newScale / lastScale.x();
    double newScaleY = lastScale.y() * ratio;

    // Update scale_y proportionally
    m_panel->updatePropertyValue("scale_y", QString::number(newScaleY, 'f', 2));
    emit m_panel->propertyChanged(currentObjectId, "scale_x", newValue);
    emit m_panel->propertyChanged(currentObjectId, "scale_y", QString::number(newScaleY, 'f', 2));

    lastScale = QPointF(newScale, newScaleY);
    return true;
  } else if (propertyName == "scale_y" && lastScale.y() > 0.0001) {
    double ratio = newScale / lastScale.y();
    double newScaleX = lastScale.x() * ratio;

    // Update scale_x proportionally
    m_panel->updatePropertyValue("scale_x", QString::number(newScaleX, 'f', 2));
    emit m_panel->propertyChanged(currentObjectId, "scale_x", QString::number(newScaleX, 'f', 2));
    emit m_panel->propertyChanged(currentObjectId, "scale_y", newValue);

    lastScale = QPointF(newScaleX, newScale);
    return true;
  }

  return false;
}

void InspectorValidators::updateScaleTracking(const QString& propertyName, const QString& newValue,
                                              QPointF& lastScale) {
  if (propertyName == "scale_x") {
    lastScale.setX(newValue.toDouble());
  } else if (propertyName == "scale_y") {
    lastScale.setY(newValue.toDouble());
  }
}

bool InspectorValidators::queuePropertyUpdate(const QString& propertyName, const QString& newValue,
                                              QHash<QString, QString>& pendingUpdates,
                                              QHash<QString, QString>& lastKnownValues) {
  // Check if value has actually changed before queuing update
  auto lastIt = lastKnownValues.find(propertyName);
  if (lastIt != lastKnownValues.end() && lastIt.value() == newValue) {
    // Value hasn't changed, skip update
    return false;
  }

  // Store the pending update
  pendingUpdates.insert(propertyName, newValue);
  lastKnownValues.insert(propertyName, newValue);
  return true;
}

void InspectorValidators::updatePropertyWidget(const QString& propertyName, const QString& newValue,
                                               const QHash<QString, QWidget*>& propertyWidgets) {
  auto widgetIt = propertyWidgets.find(propertyName);
  if (widgetIt == propertyWidgets.end()) {
    return;
  }

  QWidget* widget = widgetIt.value();
  QSignalBlocker blocker(widget);

  if (auto* lineEdit = qobject_cast<QLineEdit*>(widget)) {
    // Only update if widget doesn't have focus to preserve user editing
    if (!lineEdit->hasFocus()) {
      int cursorPos = lineEdit->cursorPosition();
      int newLength = static_cast<int>(newValue.length());
      lineEdit->setText(newValue);
      lineEdit->setCursorPosition(qMin(cursorPos, newLength));
    }
  } else if (auto* spinBox = qobject_cast<QSpinBox*>(widget)) {
    spinBox->setValue(newValue.toInt());
  } else if (auto* doubleSpinBox = qobject_cast<QDoubleSpinBox*>(widget)) {
    doubleSpinBox->setValue(newValue.toDouble());
  } else if (auto* checkBox = qobject_cast<QCheckBox*>(widget)) {
    checkBox->setChecked(newValue.toLower() == "true" || newValue == "1");
  } else if (auto* comboBox = qobject_cast<QComboBox*>(widget)) {
    comboBox->setCurrentText(newValue);
  } else if (auto* textEdit = qobject_cast<QPlainTextEdit*>(widget)) {
    // Only update if widget doesn't have focus to preserve user editing
    if (!textEdit->hasFocus()) {
      QTextCursor cursor = textEdit->textCursor();
      int cursorPos = cursor.position();
      int anchorPos = cursor.anchor();
      int newLength = static_cast<int>(newValue.length());

      textEdit->setPlainText(newValue);

      if (cursorPos <= newLength) {
        cursor.setPosition(qMin(anchorPos, newLength));
        cursor.setPosition(qMin(cursorPos, newLength), QTextCursor::KeepAnchor);
        textEdit->setTextCursor(cursor);
      }
    }
  } else if (auto* button = qobject_cast<QPushButton*>(widget)) {
    if (button->text() == QObject::tr("Edit Curve...")) {
      button->setProperty("curveId", newValue);
    } else {
      button->setText(newValue.isEmpty() ? "(Select Asset)" : newValue);
    }
  } else {
    // Handle vector widgets (Vector2/Vector3)
    QList<QDoubleSpinBox*> spinBoxes = widget->findChildren<QDoubleSpinBox*>();
    if (!spinBoxes.isEmpty()) {
      QStringList components = newValue.split(',');
      for (int i = 0; i < spinBoxes.size() && i < components.size(); ++i) {
        QSignalBlocker spinBoxBlocker(spinBoxes[i]);
        spinBoxes[i]->setValue(components[i].toDouble());
      }
    }
  }
}

void InspectorValidators::flushPendingUpdates(const QHash<QString, QString>& pendingUpdates,
                                              const QHash<QString, QWidget*>& propertyWidgets) {
  // Process all pending updates in one batch
  for (auto it = pendingUpdates.constBegin(); it != pendingUpdates.constEnd(); ++it) {
    const QString& propertyName = it.key();
    const QString& newValue = it.value();
    updatePropertyWidget(propertyName, newValue, propertyWidgets);
  }
}

} // namespace NovelMind::editor::qt
