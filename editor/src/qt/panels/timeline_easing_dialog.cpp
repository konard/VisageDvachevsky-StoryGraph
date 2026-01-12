/**
 * @file timeline_easing_dialog.cpp
 * @brief Easing dialog for timeline keyframes
 *
 * Extracted from timeline_interaction.cpp to keep files under 500 lines.
 * Handles the easing type selection dialog for keyframes including:
 * - Easing type list
 * - Bezier curve editor integration
 * - Undo command creation
 */

#include "NovelMind/editor/qt/panels/nm_timeline_panel.hpp"
#include "NovelMind/editor/qt/nm_bezier_curve_editor_dialog.hpp"
#include "NovelMind/editor/qt/nm_undo_manager.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

void NMTimelinePanel::showEasingDialog(int trackIndex, int frame) {
  // Find the track and keyframe
  int currentTrackIndex = 0;
  TimelineTrack* targetTrack = nullptr;
  Keyframe* targetKeyframe = nullptr;

  for (auto it = m_tracks.begin(); it != m_tracks.end(); ++it, ++currentTrackIndex) {
    if (currentTrackIndex == trackIndex) {
      targetTrack = it.value();
      targetKeyframe = targetTrack->getKeyframe(frame);
      break;
    }
  }

  if (!targetTrack || !targetKeyframe)
    return;

  // Create easing selection dialog
  QDialog dialog(this);
  dialog.setWindowTitle("Select Easing Type");
  dialog.setMinimumWidth(300);
  QVBoxLayout* layout = new QVBoxLayout(&dialog);

  QListWidget* easingList = new QListWidget(&dialog);
  easingList->addItem("Linear");
  easingList->addItem("Ease In");
  easingList->addItem("Ease Out");
  easingList->addItem("Ease In Out");
  easingList->addItem("Ease In Quad");
  easingList->addItem("Ease Out Quad");
  easingList->addItem("Ease In Out Quad");
  easingList->addItem("Ease In Cubic");
  easingList->addItem("Ease Out Cubic");
  easingList->addItem("Ease In Out Cubic");
  easingList->addItem("Ease In Elastic");
  easingList->addItem("Ease Out Elastic");
  easingList->addItem("Ease In Bounce");
  easingList->addItem("Ease Out Bounce");
  easingList->addItem("Step");
  easingList->addItem("Custom Bezier...");

  // Select current easing
  easingList->setCurrentRow(static_cast<int>(targetKeyframe->easing));

  layout->addWidget(easingList);

  // Add "Edit Bezier Curve..." button when Custom is selected or already set
  QPushButton* editBezierBtn = new QPushButton(tr("Edit Bezier Curve..."), &dialog);
  editBezierBtn->setVisible(targetKeyframe->easing == EasingType::Custom);
  connect(easingList, &QListWidget::currentRowChanged, [editBezierBtn](int row) {
    editBezierBtn->setVisible(row == static_cast<int>(EasingType::Custom));
  });
  layout->addWidget(editBezierBtn);

  QDialogButtonBox* buttonBox =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  layout->addWidget(buttonBox);

  // Handle "Edit Bezier Curve..." button click
  connect(editBezierBtn, &QPushButton::clicked,
          [this, targetKeyframe, targetTrack, frame, &dialog]() {
            BezierCurveResult result;
            if (NMBezierCurveEditorDialog::getEasing(this, targetKeyframe, result)) {
              // Apply the bezier curve result
              targetKeyframe->easing = EasingType::Custom;
              targetKeyframe->handleOutX = result.handleOutX;
              targetKeyframe->handleOutY = result.handleOutY;
              targetKeyframe->handleInX = result.handleInX;
              targetKeyframe->handleInY = result.handleInY;

              emit keyframeEasingChanged(targetTrack->name, frame, EasingType::Custom);
              renderTracks();
              dialog.accept();
            }
          });

  if (dialog.exec() == QDialog::Accepted) {
    int selectedIndex = easingList->currentRow();
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(EasingType::Custom) + 1) {
      // If "Custom Bezier..." is selected, open the bezier editor
      if (selectedIndex == static_cast<int>(EasingType::Custom)) {
        BezierCurveResult result;
        if (NMBezierCurveEditorDialog::getEasing(this, targetKeyframe, result)) {
          // Apply the bezier curve result
          int oldEasing = static_cast<int>(targetKeyframe->easing);
          targetKeyframe->easing = EasingType::Custom;
          targetKeyframe->handleOutX = result.handleOutX;
          targetKeyframe->handleOutY = result.handleOutY;
          targetKeyframe->handleInX = result.handleInX;
          targetKeyframe->handleInY = result.handleInY;

          // Create undo command for easing change
          if (oldEasing != static_cast<int>(EasingType::Custom)) {
            auto* cmd = new ChangeKeyframeEasingCommand(this, targetTrack->name, frame, oldEasing,
                                                        static_cast<int>(EasingType::Custom));
            NMUndoManager::instance().pushCommand(cmd);
          }

          emit keyframeEasingChanged(targetTrack->name, frame, EasingType::Custom);
          renderTracks();
        }
      } else {
        // Create undo command for easing change
        int oldEasing = static_cast<int>(targetKeyframe->easing);
        int newEasing = selectedIndex;

        if (oldEasing != newEasing) {
          auto* cmd =
              new ChangeKeyframeEasingCommand(this, targetTrack->name, frame, oldEasing, newEasing);
          NMUndoManager::instance().pushCommand(cmd);
        }

        emit keyframeEasingChanged(targetTrack->name, frame, static_cast<EasingType>(newEasing));
      }
    }
  }
}

} // namespace NovelMind::editor::qt
