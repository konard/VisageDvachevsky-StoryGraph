/**
 * @file voice_studio_waveform.cpp
 * @brief Waveform visualization widgets implementation
 */

#include "NovelMind/editor/qt/panels/voice_studio_waveform.hpp"
#include "NovelMind/editor/qt/panels/nm_voice_studio_panel.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace NovelMind::editor::qt {

// ============================================================================
// Color Constants (following UX spec)
// ============================================================================

namespace {
const QColor WAVEFORM_COLOR(0, 188, 212);       // Cyan
const QColor SELECTION_COLOR(33, 150, 243, 77); // Blue 30%
const QColor PLAYHEAD_COLOR(76, 175, 80);       // Green
const QColor TRIM_MARKER_COLOR(255, 152, 0);    // Orange
const QColor VU_SAFE_COLOR(76, 175, 80);        // Green
const QColor VU_CAUTION_COLOR(255, 193, 7);     // Yellow
const QColor VU_DANGER_COLOR(244, 67, 54);      // Red
const QColor BACKGROUND_COLOR(30, 30, 30);      // Dark gray
const QColor CLIPPING_COLOR(244, 67, 54);       // Red

constexpr float MIN_DB = -60.0f;
constexpr float MAX_DB = 0.0f;
} // namespace

// ============================================================================
// WaveformWidget
// ============================================================================

WaveformWidget::WaveformWidget(QWidget *parent) : QWidget(parent) {
  setMinimumSize(400, 120);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
}

void WaveformWidget::setClip(const VoiceClip *clip) {
  m_clip = clip;
  if (m_clip) {
    updatePeakCache();
  }
  update();
}

void WaveformWidget::setSelection(double startSec, double endSec) {
  m_selectionStart = startSec;
  m_selectionEnd = endSec;
  emit selectionChanged(startSec, endSec);
  update();
}

void WaveformWidget::clearSelection() {
  m_selectionStart = 0.0;
  m_selectionEnd = 0.0;
  update();
}

void WaveformWidget::setPlayheadPosition(double seconds) {
  m_playheadPos = seconds;
  update();
}

void WaveformWidget::setZoom(double samplesPerPixel) {
  m_samplesPerPixel = std::max(1.0, samplesPerPixel);
  updatePeakCache();
  emit zoomChanged(m_samplesPerPixel);
  update();
}

void WaveformWidget::zoomIn() { setZoom(m_samplesPerPixel * 0.5); }

void WaveformWidget::zoomOut() { setZoom(m_samplesPerPixel * 2.0); }

void WaveformWidget::zoomToFit() {
  if (!m_clip || m_clip->samples.empty()) {
    return;
  }
  double totalSamples = static_cast<double>(m_clip->samples.size());
  double widgetWidth = static_cast<double>(width());
  if (widgetWidth > 0) {
    setZoom(totalSamples / widgetWidth);
  }
}

void WaveformWidget::setScrollPosition(double seconds) {
  m_scrollPos = seconds;
  update();
}

void WaveformWidget::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  const int w = width();
  const int h = height();
  const int centerY = h / 2;

  // Background
  painter.fillRect(rect(), BACKGROUND_COLOR);

  if (!m_clip || m_clip->samples.empty()) {
    // Draw placeholder text
    painter.setPen(Qt::gray);
    painter.drawText(rect(), Qt::AlignCenter,
                     tr("No audio loaded. Open a file or record."));
    return;
  }

  // Draw waveform grid lines
  painter.setPen(QPen(QColor(60, 60, 60), 1));
  painter.drawLine(0, centerY, w, centerY);
  painter.drawLine(0, h / 4, w, h / 4);
  painter.drawLine(0, 3 * h / 4, w, 3 * h / 4);

  // Draw selection background
  if (m_selectionStart != m_selectionEnd) {
    int startX = static_cast<int>(timeToX(m_selectionStart));
    int endX = static_cast<int>(timeToX(m_selectionEnd));
    if (startX > endX)
      std::swap(startX, endX);
    painter.fillRect(startX, 0, endX - startX, h, SELECTION_COLOR);
  }

  // Draw waveform
  painter.setPen(QPen(WAVEFORM_COLOR, 1));

  const size_t numSamples = m_clip->samples.size();
  const uint32_t sampleRate = m_clip->format.sampleRate;
  const double scrollSamples = m_scrollPos * sampleRate;

  for (int x = 0; x < w; ++x) {
    int64_t startSample =
        static_cast<int64_t>(scrollSamples + x * m_samplesPerPixel);
    int64_t endSample =
        static_cast<int64_t>(scrollSamples + (x + 1) * m_samplesPerPixel);

    if (startSample < 0)
      startSample = 0;
    if (endSample > static_cast<int64_t>(numSamples))
      endSample = static_cast<int64_t>(numSamples);
    if (startSample >= static_cast<int64_t>(numSamples))
      continue;

    // Find min/max in this range
    float minVal = 0.0f, maxVal = 0.0f;
    for (int64_t i = startSample; i < endSample; ++i) {
      float sample = m_clip->samples[static_cast<size_t>(i)];
      if (sample < minVal)
        minVal = sample;
      if (sample > maxVal)
        maxVal = sample;
    }

    // Map to screen coordinates
    int y1 = centerY - static_cast<int>(maxVal * static_cast<float>(h / 2 - 4));
    int y2 = centerY - static_cast<int>(minVal * static_cast<float>(h / 2 - 4));

    painter.drawLine(x, y1, x, y2);
  }

  // Draw trim markers (if trimming is active)
  if (m_clip->edit.trimStartSamples > 0) {
    double trimStartSec =
        static_cast<double>(m_clip->edit.trimStartSamples) / sampleRate;
    int x = static_cast<int>(timeToX(trimStartSec));
    painter.setPen(QPen(TRIM_MARKER_COLOR, 2));
    painter.drawLine(x, 0, x, h);

    // Draw triangle indicator
    QPolygon triangle;
    triangle << QPoint(x, 0) << QPoint(x + 10, 0) << QPoint(x, 10);
    painter.setBrush(TRIM_MARKER_COLOR);
    painter.drawPolygon(triangle);
  }

  if (m_clip->edit.trimEndSamples > 0) {
    double clipDuration = m_clip->getDurationSeconds();
    double trimEndSec =
        clipDuration -
        static_cast<double>(m_clip->edit.trimEndSamples) / sampleRate;
    int x = static_cast<int>(timeToX(trimEndSec));
    painter.setPen(QPen(TRIM_MARKER_COLOR, 2));
    painter.drawLine(x, 0, x, h);

    QPolygon triangle;
    triangle << QPoint(x, 0) << QPoint(x - 10, 0) << QPoint(x, 10);
    painter.setBrush(TRIM_MARKER_COLOR);
    painter.drawPolygon(triangle);
  }

  // Draw playhead
  int playheadX = static_cast<int>(timeToX(m_playheadPos));
  if (playheadX >= 0 && playheadX < w) {
    painter.setPen(QPen(PLAYHEAD_COLOR, 2));
    painter.drawLine(playheadX, 0, playheadX, h);

    // Draw triangle at top
    QPolygon triangle;
    triangle << QPoint(playheadX - 6, 0) << QPoint(playheadX + 6, 0)
             << QPoint(playheadX, 10);
    painter.setBrush(PLAYHEAD_COLOR);
    painter.drawPolygon(triangle);
  }

  // Draw border
  painter.setPen(QColor(80, 80, 80));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(0, 0, w - 1, h - 1);
}

void WaveformWidget::mousePressEvent(QMouseEvent *event) {
  if (!m_clip)
    return;

  double clickTime = xToTime(event->position().x());

  if (event->button() == Qt::LeftButton) {
    if (event->modifiers() & Qt::ShiftModifier) {
      // Extend selection
      if (clickTime < m_selectionStart) {
        setSelection(clickTime, m_selectionEnd);
      } else {
        setSelection(m_selectionStart, clickTime);
      }
    } else {
      // Start new selection
      m_isSelecting = true;
      m_isDragging = false;
      m_dragStartTime = clickTime;
      m_selectionStart = clickTime;
      m_selectionEnd = clickTime;
    }
  } else if (event->button() == Qt::MiddleButton) {
    // Seek playhead
    emit playheadClicked(clickTime);
  }

  update();
}

void WaveformWidget::mouseMoveEvent(QMouseEvent *event) {
  if (!m_clip)
    return;

  double time = xToTime(event->position().x());

  if (m_isSelecting) {
    // Update selection
    if (time < m_dragStartTime) {
      m_selectionStart = time;
      m_selectionEnd = m_dragStartTime;
    } else {
      m_selectionStart = m_dragStartTime;
      m_selectionEnd = time;
    }
    emit selectionChanged(m_selectionStart, m_selectionEnd);
    update();
  }
}

void WaveformWidget::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    m_isSelecting = false;
    m_isDragging = false;
  }
}

void WaveformWidget::wheelEvent(QWheelEvent *event) {
  if (event->modifiers() & Qt::ControlModifier) {
    // Zoom with Ctrl+Wheel
    if (event->angleDelta().y() > 0) {
      zoomIn();
    } else {
      zoomOut();
    }
  } else {
    // Scroll
    double delta = event->angleDelta().y() > 0 ? -0.5 : 0.5;
    setScrollPosition(m_scrollPos + delta);
  }
  event->accept();
}

void WaveformWidget::resizeEvent(QResizeEvent *) { updatePeakCache(); }

double WaveformWidget::timeToX(double seconds) const {
  if (!m_clip || m_clip->format.sampleRate == 0)
    return 0.0;
  double sampleOffset = (seconds - m_scrollPos) * m_clip->format.sampleRate;
  return sampleOffset / m_samplesPerPixel;
}

double WaveformWidget::xToTime(double x) const {
  if (!m_clip || m_clip->format.sampleRate == 0)
    return 0.0;
  double sampleOffset = x * m_samplesPerPixel;
  return m_scrollPos + sampleOffset / m_clip->format.sampleRate;
}

void WaveformWidget::updatePeakCache() {
  if (!m_clip || m_clip->samples.empty()) {
    m_displayPeaks.clear();
    return;
  }

  // Calculate peaks for display at current zoom level
  int w = width();
  m_displayPeaks.resize(static_cast<size_t>(w * 2)); // min/max pairs

  const size_t numSamples = m_clip->samples.size();
  const uint32_t sampleRate = m_clip->format.sampleRate;
  const double scrollSamples = m_scrollPos * sampleRate;

  for (int x = 0; x < w; ++x) {
    int64_t startSample =
        static_cast<int64_t>(scrollSamples + x * m_samplesPerPixel);
    int64_t endSample =
        static_cast<int64_t>(scrollSamples + (x + 1) * m_samplesPerPixel);

    if (startSample < 0)
      startSample = 0;
    if (endSample > static_cast<int64_t>(numSamples))
      endSample = static_cast<int64_t>(numSamples);

    float minVal = 0.0f, maxVal = 0.0f;
    for (int64_t i = startSample; i < endSample; ++i) {
      float sample = m_clip->samples[static_cast<size_t>(i)];
      if (sample < minVal)
        minVal = sample;
      if (sample > maxVal)
        maxVal = sample;
    }

    m_displayPeaks[static_cast<size_t>(x * 2)] = minVal;
    m_displayPeaks[static_cast<size_t>(x * 2 + 1)] = maxVal;
  }
}

// ============================================================================
// StudioVUMeterWidget
// ============================================================================

StudioVUMeterWidget::StudioVUMeterWidget(QWidget *parent) : QWidget(parent) {
  setMinimumSize(180, 24);
  setMaximumHeight(30);
}

void StudioVUMeterWidget::setLevel(float rmsDb, float peakDb, bool clipping) {
  m_rmsDb = rmsDb;
  m_peakDb = peakDb;
  m_clipping = clipping;
  update();
}

void StudioVUMeterWidget::reset() {
  m_rmsDb = -60.0f;
  m_peakDb = -60.0f;
  m_clipping = false;
  update();
}

void StudioVUMeterWidget::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  const int w = width();
  const int h = height();
  const int margin = 2;
  const float widthAvailable = static_cast<float>(w - margin * 2);

  // Background
  painter.fillRect(rect(), BACKGROUND_COLOR);

  // Convert dB to normalized value
  auto dbToNorm = [](float db) {
    return std::clamp((db - MIN_DB) / (MAX_DB - MIN_DB), 0.0f, 1.0f);
  };

  float rmsNorm = dbToNorm(m_rmsDb);
  float peakNorm = dbToNorm(m_peakDb);

  // RMS bar with gradient
  int rmsWidth = static_cast<int>(rmsNorm * widthAvailable);

  QLinearGradient gradient(0, 0, w, 0);
  gradient.setColorAt(0.0, VU_SAFE_COLOR);
  gradient.setColorAt(0.7, VU_CAUTION_COLOR);
  gradient.setColorAt(0.9, QColor(200, 100, 40));
  gradient.setColorAt(1.0, VU_DANGER_COLOR);

  painter.fillRect(margin, margin, rmsWidth, h - margin * 2, gradient);

  // Peak indicator line
  int peakPos = margin + static_cast<int>(peakNorm * widthAvailable);
  painter.setPen(QPen(Qt::white, 2));
  painter.drawLine(peakPos, margin, peakPos, h - margin);

  // Scale markers
  painter.setPen(QColor(100, 100, 100));
  for (int db = -60; db <= 0; db += 6) {
    float norm = dbToNorm(static_cast<float>(db));
    int x = margin + static_cast<int>(norm * widthAvailable);
    painter.drawLine(x, h - 4, x, h - 1);
  }

  // Clipping indicator
  if (m_clipping) {
    painter.fillRect(w - 20, margin, 18, h - margin * 2, CLIPPING_COLOR);
    painter.setPen(Qt::white);
    painter.drawText(w - 18, h - 6, "!");
  }

  // Border
  painter.setPen(QColor(80, 80, 80));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(margin, margin, w - margin * 2 - 1, h - margin * 2 - 1);
}

} // namespace NovelMind::editor::qt
