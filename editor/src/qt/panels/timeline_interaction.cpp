/**
 * @file timeline_interaction.cpp
 * @brief User interaction handling for NMTimelinePanel
 *
 * Extracted from nm_timeline_panel.cpp as part of refactoring to improve
 * maintainability. Handles all user interaction functionality including:
 * - Keyboard event handling (delete, copy, paste, select all)
 * - Mouse event handling (clicks, box selection)
 * - Keyframe selection operations (single, range, box, multi-select)
 * - Keyframe event handlers (click, move, double-click, drag)
 *
 * Easing dialog is in timeline_easing_dialog.cpp
 */

#include "NovelMind/editor/qt/panels/nm_timeline_panel.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"

#include <QEvent>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>

namespace NovelMind::editor::qt {

// =============================================================================
// Selection Management
// =============================================================================

void NMTimelinePanel::selectKeyframe(const KeyframeId& id, bool additive) {
  if (!additive) {
    // Clear previous selection
    m_selectedKeyframes.clear();
  }

  if (m_selectedKeyframes.contains(id)) {
    // Toggle if already selected in additive mode
    if (additive) {
      m_selectedKeyframes.remove(id);
    }
  } else {
    m_selectedKeyframes.insert(id);
  }

  updateSelectionVisuals();
}

void NMTimelinePanel::clearSelection() {
  m_selectedKeyframes.clear();
  updateSelectionVisuals();
}

void NMTimelinePanel::updateSelectionVisuals() {
  for (auto it = m_keyframeItems.begin(); it != m_keyframeItems.end(); ++it) {
    bool selected = m_selectedKeyframes.contains(it.key());
    it.value()->setSelected(selected);
  }

  // Update data model selection state
  int trackIndex = 0;
  for (auto trackIt = m_tracks.begin(); trackIt != m_tracks.end(); ++trackIt, ++trackIndex) {
    TimelineTrack* track = trackIt.value();
    for (auto& kf : track->keyframes) {
      KeyframeId id;
      id.trackIndex = trackIndex;
      id.frame = kf.frame;
      kf.isSelected = m_selectedKeyframes.contains(id);
    }
  }
}

void NMTimelinePanel::selectAllKeyframes() {
  m_selectedKeyframes.clear();

  int trackIndex = 0;
  for (auto trackIt = m_tracks.begin(); trackIt != m_tracks.end(); ++trackIt, ++trackIndex) {
    TimelineTrack* track = trackIt.value();
    if (!track || !track->visible) {
      continue;
    }

    for (const auto& kf : track->keyframes) {
      KeyframeId id;
      id.trackIndex = trackIndex;
      id.frame = kf.frame;
      m_selectedKeyframes.insert(id);
    }
  }

  updateSelectionVisuals();
}

void NMTimelinePanel::selectKeyframeRange(const KeyframeId& fromId, const KeyframeId& toId) {
  // Determine the frame range
  int startFrame = qMin(fromId.frame, toId.frame);
  int endFrame = qMax(fromId.frame, toId.frame);

  // Determine track range
  int startTrack = qMin(fromId.trackIndex, toId.trackIndex);
  int endTrack = qMax(fromId.trackIndex, toId.trackIndex);

  // Select all keyframes within the range
  int trackIndex = 0;
  for (auto trackIt = m_tracks.begin(); trackIt != m_tracks.end(); ++trackIt, ++trackIndex) {
    if (trackIndex < startTrack || trackIndex > endTrack) {
      continue;
    }

    TimelineTrack* track = trackIt.value();
    if (!track || !track->visible) {
      continue;
    }

    for (const auto& kf : track->keyframes) {
      if (kf.frame >= startFrame && kf.frame <= endFrame) {
        KeyframeId id;
        id.trackIndex = trackIndex;
        id.frame = kf.frame;
        m_selectedKeyframes.insert(id);
      }
    }
  }

  updateSelectionVisuals();
}

void NMTimelinePanel::selectKeyframesInRect(const QRectF& rect) {
  // Clear existing selection
  m_selectedKeyframes.clear();

  // Find all keyframes within the rectangle
  for (auto it = m_keyframeItems.begin(); it != m_keyframeItems.end(); ++it) {
    NMKeyframeItem* kfItem = it.value();
    if (!kfItem) {
      continue;
    }

    // Get the keyframe's scene position
    QPointF kfPos = kfItem->scenePos();

    // Check if the keyframe is within the selection rectangle
    if (rect.contains(kfPos)) {
      m_selectedKeyframes.insert(it.key());
    }
  }

  updateSelectionVisuals();
}

// =============================================================================
// Box Selection
// =============================================================================

void NMTimelinePanel::startBoxSelection(const QPointF& pos) {
  m_isBoxSelecting = true;
  m_boxSelectStart = pos;
  m_boxSelectEnd = pos;

  // Create the selection rectangle visual
  if (!m_boxSelectRect) {
    m_boxSelectRect = new QGraphicsRectItem();
    m_boxSelectRect->setPen(QPen(QColor("#4A90D9"), 1, Qt::DashLine));
    m_boxSelectRect->setBrush(QBrush(QColor(74, 144, 217, 50)));
    m_boxSelectRect->setZValue(99); // Just below playhead
    m_timelineScene->addItem(m_boxSelectRect);
  }

  m_boxSelectRect->setRect(QRectF(m_boxSelectStart, QSizeF(0, 0)));
  m_boxSelectRect->setVisible(true);

  // Clear selection unless Ctrl is held
  // (handled by Qt automatically in the mouse event)
}

void NMTimelinePanel::updateBoxSelection(const QPointF& pos) {
  m_boxSelectEnd = pos;

  // Update the visual rectangle
  if (m_boxSelectRect) {
    QRectF rect = QRectF(m_boxSelectStart, m_boxSelectEnd).normalized();
    m_boxSelectRect->setRect(rect);
  }
}

void NMTimelinePanel::endBoxSelection() {
  if (!m_isBoxSelecting) {
    return;
  }

  m_isBoxSelecting = false;

  // Hide the selection rectangle
  if (m_boxSelectRect) {
    m_boxSelectRect->setVisible(false);
  }

  // Calculate the selection rectangle
  QRectF selectionRect = QRectF(m_boxSelectStart, m_boxSelectEnd).normalized();

  // Select keyframes within the rectangle
  selectKeyframesInRect(selectionRect);
}

// =============================================================================
// Keyframe Event Handlers
// =============================================================================

void NMTimelinePanel::onKeyframeClicked(bool additiveSelection, bool rangeSelection,
                                        const KeyframeId& id) {
  if (rangeSelection && m_lastClickedKeyframe.trackIndex >= 0) {
    // Shift+Click: select range from last clicked to current
    selectKeyframeRange(m_lastClickedKeyframe, id);
  } else {
    selectKeyframe(id, additiveSelection);
  }

  // Remember the last clicked keyframe for range selection
  m_lastClickedKeyframe = id;
}

void NMTimelinePanel::onKeyframeMoved(int oldFrame, int newFrame, int trackIndex) {
  // Find the track
  int currentTrackIndex = 0;
  TimelineTrack* targetTrack = nullptr;

  for (auto it = m_tracks.begin(); it != m_tracks.end(); ++it, ++currentTrackIndex) {
    if (currentTrackIndex == trackIndex) {
      targetTrack = it.value();
      break;
    }
  }

  if (!targetTrack)
    return;

  // Calculate the frame delta
  int frameDelta = newFrame - oldFrame;

  // If multi-select dragging, move all selected keyframes by the same delta
  if (m_isDraggingSelection && m_selectedKeyframes.size() > 1) {
    // Create macro for multiple moves
    NMUndoManager::instance().beginMacro("Move Selected Keyframes");

    // Get track names list
    QStringList trackNames = m_tracks.keys();

    // Move all selected keyframes
    QSet<KeyframeId> newSelection;
    for (const KeyframeId& selId : m_selectedKeyframes) {
      if (selId.trackIndex < 0 || selId.trackIndex >= trackNames.size()) {
        continue;
      }

      QString selTrackName = trackNames.at(selId.trackIndex);
      TimelineTrack* selTrack = getTrack(selTrackName);
      if (!selTrack || selTrack->locked) {
        continue;
      }

      int startFrame = m_dragStartFrames.value(selId, selId.frame);
      int targetFrame = startFrame + frameDelta;
      if (targetFrame < 0) {
        targetFrame = 0;
      }

      // Only move if the keyframe still exists at the start position
      if (selTrack->getKeyframe(startFrame)) {
        auto* cmd = new TimelineKeyframeMoveCommand(this, selTrackName, startFrame, targetFrame);
        NMUndoManager::instance().pushCommand(cmd);

        emit keyframeMoved(selTrackName, startFrame, targetFrame);
      }

      // Update selection to new position
      KeyframeId newSelId;
      newSelId.trackIndex = selId.trackIndex;
      newSelId.frame = targetFrame;
      newSelection.insert(newSelId);
    }

    NMUndoManager::instance().endMacro();

    m_selectedKeyframes = newSelection;
  } else {
    // Single keyframe move
    auto* cmd = new TimelineKeyframeMoveCommand(this, targetTrack->name, oldFrame, newFrame);
    NMUndoManager::instance().pushCommand(cmd);

    // Update selection to new position
    KeyframeId oldId;
    oldId.trackIndex = trackIndex;
    oldId.frame = oldFrame;

    KeyframeId newId;
    newId.trackIndex = trackIndex;
    newId.frame = newFrame;

    if (m_selectedKeyframes.contains(oldId)) {
      m_selectedKeyframes.remove(oldId);
      m_selectedKeyframes.insert(newId);
    }

    emit keyframeMoved(targetTrack->name, oldFrame, newFrame);
  }

  // Re-render to update positions
  renderTracks();
}

void NMTimelinePanel::onKeyframeDoubleClicked(int trackIndex, int frame) {
  showEasingDialog(trackIndex, frame);
}

void NMTimelinePanel::onKeyframeDragStarted(const KeyframeId& id) {
  // If the dragged keyframe is not in the selection, select only it
  if (!m_selectedKeyframes.contains(id)) {
    m_selectedKeyframes.clear();
    m_selectedKeyframes.insert(id);
    updateSelectionVisuals();
  }

  // Store the starting frames for all selected keyframes
  m_dragStartFrames.clear();
  for (const KeyframeId& selId : m_selectedKeyframes) {
    m_dragStartFrames[selId] = selId.frame;
  }

  m_isDraggingSelection = true;
}

void NMTimelinePanel::onKeyframeDragEnded() {
  m_isDraggingSelection = false;
  m_dragStartFrames.clear();
}

// =============================================================================
// Event Filter for Keyboard and Mouse
// =============================================================================

bool NMTimelinePanel::eventFilter(QObject* obj, QEvent* event) {
  if (event->type() == QEvent::KeyPress) {
    QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

    // Delete selected keyframes
    if (keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace) {
      deleteSelectedKeyframes();
      return true;
    }

    // Copy selected keyframes (Ctrl+C)
    if (keyEvent->matches(QKeySequence::Copy)) {
      copySelectedKeyframes();
      return true;
    }

    // Paste keyframes (Ctrl+V)
    if (keyEvent->matches(QKeySequence::Paste)) {
      pasteKeyframes();
      return true;
    }

    // Select all keyframes (Ctrl+A)
    if (keyEvent->matches(QKeySequence::SelectAll)) {
      selectAllKeyframes();
      return true;
    }
  }

  // Handle mouse events for box selection on the graphics view
  if (obj == m_timelineView->viewport()) {
    if (event->type() == QEvent::MouseButtonPress) {
      QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
      if (mouseEvent->button() == Qt::LeftButton) {
        // Check if clicking on empty space (no item at position)
        QPointF scenePos = m_timelineView->mapToScene(mouseEvent->pos());
        QGraphicsItem* item = m_timelineScene->itemAt(scenePos, QTransform());

        // If no item at position, start box selection
        if (!item || item == m_playheadItem) {
          startBoxSelection(scenePos);
          return true;
        }
      }
    } else if (event->type() == QEvent::MouseMove) {
      if (m_isBoxSelecting) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        QPointF scenePos = m_timelineView->mapToScene(mouseEvent->pos());
        updateBoxSelection(scenePos);
        return true;
      }
    } else if (event->type() == QEvent::MouseButtonRelease) {
      if (m_isBoxSelecting) {
        endBoxSelection();
        return true;
      }
    }
  }

  return NMDockPanel::eventFilter(obj, event);
}

} // namespace NovelMind::editor::qt
