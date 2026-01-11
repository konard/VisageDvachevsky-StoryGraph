#pragma once

/**
 * @file voice_studio_waveform.hpp
 * @brief Waveform visualization widgets for Voice Studio
 */

#include <QWidget>
#include <vector>

namespace NovelMind::editor::qt {

// Forward declarations
struct VoiceClip;

/**
 * @brief Widget for displaying and interacting with audio waveforms
 *
 * Features:
 * - Peak waveform visualization
 * - Selection range for trimming
 * - Playhead position indicator
 * - Zoom and scroll
 * - Click-to-seek support
 */
class WaveformWidget : public QWidget {
  Q_OBJECT

public:
  explicit WaveformWidget(QWidget *parent = nullptr);

  // Set the clip to display
  void setClip(const VoiceClip *clip);

  // Selection (for trimming)
  void setSelection(double startSec, double endSec);
  [[nodiscard]] double getSelectionStart() const { return m_selectionStart; }
  [[nodiscard]] double getSelectionEnd() const { return m_selectionEnd; }
  void clearSelection();

  // Playhead
  void setPlayheadPosition(double seconds);
  [[nodiscard]] double getPlayheadPosition() const { return m_playheadPos; }

  // Zoom (samples per pixel)
  void setZoom(double samplesPerPixel);
  void zoomIn();
  void zoomOut();
  void zoomToFit();

  // Scroll position
  void setScrollPosition(double seconds);

signals:
  void selectionChanged(double startSec, double endSec);
  void playheadClicked(double seconds);
  void zoomChanged(double samplesPerPixel);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  double timeToX(double seconds) const;
  double xToTime(double x) const;
  void updatePeakCache();

  const VoiceClip *m_clip = nullptr;
  std::vector<float> m_displayPeaks;

  double m_selectionStart = 0.0;
  double m_selectionEnd = 0.0;
  double m_playheadPos = 0.0;
  double m_scrollPos = 0.0;
  double m_samplesPerPixel = 1000.0;

  bool m_isDragging = false;
  bool m_isSelecting = false;
  double m_dragStartTime = 0.0;
};

/**
 * @brief VU meter visualization widget
 */
class StudioVUMeterWidget : public QWidget {
  Q_OBJECT

public:
  explicit StudioVUMeterWidget(QWidget *parent = nullptr);

  void setLevel(float rmsDb, float peakDb, bool clipping);
  void reset();

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  float m_rmsDb = -60.0f;
  float m_peakDb = -60.0f;
  bool m_clipping = false;
};

} // namespace NovelMind::editor::qt
