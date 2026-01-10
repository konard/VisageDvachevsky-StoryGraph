/**
 * @file nm_voice_studio_panel.cpp
 * @brief Voice Studio panel implementation
 *
 * Provides a comprehensive voice-over authoring environment:
 * - Microphone selection and level monitoring
 * - Recording with waveform visualization
 * - Non-destructive editing (trim, fade in/out)
 * - Audio effects (normalize, high-pass, low-pass, EQ, noise gate)
 * - Preview playback with rendered effects
 * - Preset management
 * - Undo/Redo support for all editing operations
 */

#include "NovelMind/editor/qt/panels/nm_voice_studio_panel.hpp"
#include "NovelMind/audio/audio_recorder.hpp"
#include "NovelMind/audio/voice_manifest.hpp"
#include "NovelMind/core/logger.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"

#include <QApplication>
#include <QAudioDevice>
#include <QAudioOutput>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QMouseEvent>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSplitter>
#include <QTimer>
#include <QToolBar>
#include <QUndoCommand>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <fstream>

namespace NovelMind::editor::qt {

// ============================================================================
// Color Constants (following UX spec)
// ============================================================================

namespace {
const QColor WAVEFORM_COLOR(0, 188, 212);       // Cyan
const QColor SELECTION_COLOR(33, 150, 243, 77); // Blue 30%
const QColor PLAYHEAD_COLOR(76, 175, 80);       // Green
const QColor TRIM_MARKER_COLOR(255, 152, 0);    // Orange
const QColor CLIPPING_COLOR(244, 67, 54);       // Red
const QColor VU_SAFE_COLOR(76, 175, 80);        // Green
const QColor VU_CAUTION_COLOR(255, 193, 7);     // Yellow
const QColor VU_DANGER_COLOR(244, 67, 54);      // Red
const QColor BACKGROUND_COLOR(30, 30, 30);      // Dark gray

constexpr float MIN_DB = -60.0f;
constexpr float MAX_DB = 0.0f;
} // namespace

// ============================================================================
// Undo Commands
// ============================================================================

class EditParamCommand : public QUndoCommand {
public:
  EditParamCommand(VoiceClipEdit *edit, VoiceClipEdit oldEdit,
                   VoiceClipEdit newEdit, const QString &text)
      : QUndoCommand(text), m_edit(edit), m_oldEdit(oldEdit),
        m_newEdit(newEdit) {}

  void undo() override { *m_edit = m_oldEdit; }
  void redo() override { *m_edit = m_newEdit; }

private:
  VoiceClipEdit *m_edit;
  VoiceClipEdit m_oldEdit;
  VoiceClipEdit m_newEdit;
};

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

// ============================================================================
// AudioProcessor
// ============================================================================

std::vector<float> AudioProcessor::process(const std::vector<float> &source,
                                           const VoiceClipEdit &edit,
                                           const AudioFormat &format) {
  if (source.empty())
    return {};

  // Apply trim first
  auto result = applyTrim(source, edit.trimStartSamples, edit.trimEndSamples);

  // Apply pre-gain
  if (edit.preGainDb != 0.0f) {
    applyGain(result, edit.preGainDb);
  }

  // Apply high-pass filter
  if (edit.highPassEnabled) {
    applyHighPass(result, edit.highPassFreqHz, format.sampleRate);
  }

  // Apply low-pass filter
  if (edit.lowPassEnabled) {
    applyLowPass(result, edit.lowPassFreqHz, format.sampleRate);
  }

  // Apply EQ
  if (edit.eqEnabled) {
    applyEQ(result, edit.eqLowGainDb, edit.eqMidGainDb, edit.eqHighGainDb,
            edit.eqLowFreqHz, edit.eqHighFreqHz, format.sampleRate);
  }

  // Apply noise gate
  if (edit.noiseGateEnabled) {
    applyNoiseGate(result, edit.noiseGateThresholdDb, edit.noiseGateReductionDb,
                   edit.noiseGateAttackMs, edit.noiseGateReleaseMs,
                   format.sampleRate);
  }

  // Apply fades
  if (edit.fadeInMs > 0 || edit.fadeOutMs > 0) {
    applyFades(result, edit.fadeInMs, edit.fadeOutMs, format.sampleRate);
  }

  // Apply normalization last
  if (edit.normalizeEnabled) {
    applyNormalize(result, edit.normalizeTargetDbFS);
  }

  return result;
}

std::vector<float> AudioProcessor::applyTrim(const std::vector<float> &samples,
                                             int64_t trimStart,
                                             int64_t trimEnd) {
  if (samples.empty())
    return {};

  int64_t totalSamples = static_cast<int64_t>(samples.size());
  int64_t start = std::clamp(trimStart, int64_t(0), totalSamples);
  int64_t end = totalSamples - std::clamp(trimEnd, int64_t(0), totalSamples);

  if (end <= start)
    return {};

  return std::vector<float>(samples.begin() + start, samples.begin() + end);
}

void AudioProcessor::applyFades(std::vector<float> &samples, float fadeInMs,
                                float fadeOutMs, uint32_t sampleRate) {
  if (samples.empty())
    return;

  // Fade in
  if (fadeInMs > 0) {
    int fadeInSamples =
        static_cast<int>(fadeInMs * static_cast<float>(sampleRate) / 1000.0f);
    fadeInSamples = std::min(fadeInSamples, static_cast<int>(samples.size()));

    for (int i = 0; i < fadeInSamples; ++i) {
      float t = static_cast<float>(i) / static_cast<float>(fadeInSamples);
      // Use equal power fade (sine curve)
      samples[static_cast<size_t>(i)] *= std::sin(t * 3.14159f / 2.0f);
    }
  }

  // Fade out
  if (fadeOutMs > 0) {
    int fadeOutSamples =
        static_cast<int>(fadeOutMs * static_cast<float>(sampleRate) / 1000.0f);
    fadeOutSamples = std::min(fadeOutSamples, static_cast<int>(samples.size()));

    int startIdx = static_cast<int>(samples.size()) - fadeOutSamples;
    for (int i = 0; i < fadeOutSamples; ++i) {
      float t = static_cast<float>(i) / static_cast<float>(fadeOutSamples);
      samples[static_cast<size_t>(startIdx + i)] *=
          std::cos(t * 3.14159f / 2.0f);
    }
  }
}

void AudioProcessor::applyGain(std::vector<float> &samples, float gainDb) {
  if (samples.empty() || gainDb == 0.0f)
    return;

  float gainLinear = std::pow(10.0f, gainDb / 20.0f);

  for (auto &sample : samples) {
    sample *= gainLinear;
    // Soft clip to prevent harsh distortion
    sample = std::tanh(sample);
  }
}

void AudioProcessor::applyNormalize(std::vector<float> &samples,
                                    float targetDbFS) {
  if (samples.empty())
    return;

  // Find current peak
  float currentPeak = 0.0f;
  for (const auto &sample : samples) {
    float absVal = std::abs(sample);
    if (absVal > currentPeak)
      currentPeak = absVal;
  }

  if (currentPeak < 0.0001f)
    return; // Too quiet, skip

  // Calculate required gain
  float targetLinear = std::pow(10.0f, targetDbFS / 20.0f);
  float gain = targetLinear / currentPeak;

  for (auto &sample : samples) {
    sample *= gain;
  }
}

void AudioProcessor::applyHighPass(std::vector<float> &samples, float cutoffHz,
                                   uint32_t sampleRate) {
  if (samples.empty() || cutoffHz <= 0 || sampleRate == 0)
    return;

  // Simple 1-pole high-pass filter
  float rc = 1.0f / (2.0f * 3.14159f * cutoffHz);
  float dt = 1.0f / static_cast<float>(sampleRate);
  float alpha = rc / (rc + dt);

  float prevInput = samples[0];
  float prevOutput = samples[0];

  for (size_t i = 1; i < samples.size(); ++i) {
    float output = alpha * (prevOutput + samples[i] - prevInput);
    prevInput = samples[i];
    prevOutput = output;
    samples[i] = output;
  }
}

void AudioProcessor::applyLowPass(std::vector<float> &samples, float cutoffHz,
                                  uint32_t sampleRate) {
  if (samples.empty() || cutoffHz <= 0 || sampleRate == 0)
    return;

  // Simple 1-pole low-pass filter
  float rc = 1.0f / (2.0f * 3.14159f * cutoffHz);
  float dt = 1.0f / static_cast<float>(sampleRate);
  float alpha = dt / (rc + dt);

  float prevOutput = samples[0];

  for (size_t i = 1; i < samples.size(); ++i) {
    float output = prevOutput + alpha * (samples[i] - prevOutput);
    prevOutput = output;
    samples[i] = output;
  }
}

void AudioProcessor::applyEQ(std::vector<float> &samples, float lowGainDb,
                             float midGainDb, float highGainDb, float lowFreq,
                             float highFreq, uint32_t sampleRate) {
  if (samples.empty() || sampleRate == 0)
    return;

  // Simple 3-band EQ using crossover filters
  // This is a simplified implementation

  // Convert gains to linear
  float lowGain = std::pow(10.0f, lowGainDb / 20.0f);
  float midGain = std::pow(10.0f, midGainDb / 20.0f);
  float highGain = std::pow(10.0f, highGainDb / 20.0f);

  // Filter coefficients
  float lowRc = 1.0f / (2.0f * 3.14159f * lowFreq);
  float highRc = 1.0f / (2.0f * 3.14159f * highFreq);
  float dt = 1.0f / static_cast<float>(sampleRate);

  float lowAlpha = dt / (lowRc + dt);
  float highAlpha = highRc / (highRc + dt);

  float lowPrev = samples[0];
  float highPrevIn = samples[0];
  float highPrevOut = 0.0f;

  for (size_t i = 0; i < samples.size(); ++i) {
    // Low band (LP filter)
    float low = lowPrev + lowAlpha * (samples[i] - lowPrev);
    lowPrev = low;

    // High band (HP filter)
    float high = highAlpha * (highPrevOut + samples[i] - highPrevIn);
    highPrevIn = samples[i];
    highPrevOut = high;

    // Mid band (remainder)
    float mid = samples[i] - low - high;

    // Apply gains and sum
    samples[i] = low * lowGain + mid * midGain + high * highGain;
  }
}

void AudioProcessor::applyNoiseGate(std::vector<float> &samples,
                                    float thresholdDb, float reductionDb,
                                    float attackMs, float releaseMs,
                                    uint32_t sampleRate) {
  if (samples.empty() || sampleRate == 0)
    return;

  float threshold = std::pow(10.0f, thresholdDb / 20.0f);
  float reduction = std::pow(10.0f, reductionDb / 20.0f);

  int attackSamples =
      static_cast<int>(attackMs * static_cast<float>(sampleRate) / 1000.0f);
  int releaseSamples =
      static_cast<int>(releaseMs * static_cast<float>(sampleRate) / 1000.0f);

  float envelope = 0.0f;
  float gateGain = reduction;

  for (size_t i = 0; i < samples.size(); ++i) {
    float absVal = std::abs(samples[i]);

    // Update envelope
    if (absVal > envelope) {
      envelope +=
          (absVal - envelope) / static_cast<float>(std::max(1, attackSamples));
    } else {
      envelope -=
          (envelope - absVal) / static_cast<float>(std::max(1, releaseSamples));
    }

    // Gate logic
    if (envelope > threshold) {
      // Open gate
      if (gateGain < 1.0f) {
        gateGain +=
            (1.0f - gateGain) / static_cast<float>(std::max(1, attackSamples));
      }
    } else {
      // Close gate
      if (gateGain > reduction) {
        gateGain -= (gateGain - reduction) /
                    static_cast<float>(std::max(1, releaseSamples));
      }
    }

    samples[i] *= gateGain;
  }
}

float AudioProcessor::calculatePeakDb(const std::vector<float> &samples) {
  if (samples.empty())
    return MIN_DB;

  float peak = 0.0f;
  for (const auto &sample : samples) {
    float absVal = std::abs(sample);
    if (absVal > peak)
      peak = absVal;
  }

  if (peak < 0.0001f)
    return MIN_DB;
  return 20.0f * std::log10(peak);
}

float AudioProcessor::calculateRmsDb(const std::vector<float> &samples) {
  if (samples.empty())
    return MIN_DB;

  float sum = 0.0f;
  for (const auto &sample : samples) {
    sum += sample * sample;
  }
  float rms = std::sqrt(sum / static_cast<float>(samples.size()));

  if (rms < 0.0001f)
    return MIN_DB;
  return 20.0f * std::log10(rms);
}

// ============================================================================
// NMVoiceStudioPanel
// ============================================================================

NMVoiceStudioPanel::NMVoiceStudioPanel(QWidget *parent)
    : NMDockPanel("Voice Studio", parent) {
  m_undoStack = std::make_unique<QUndoStack>(this);
  setupUI();
}

NMVoiceStudioPanel::~NMVoiceStudioPanel() { onShutdown(); }

void NMVoiceStudioPanel::onInitialize() {
  NOVELMIND_LOG_INFO("Voice Studio Panel initialized");

  // Set up update timer
  m_updateTimer = new QTimer(this);
  connect(m_updateTimer, &QTimer::timeout, this,
          &NMVoiceStudioPanel::onUpdateTimer);
  m_updateTimer->start(50); // 20 Hz update rate

  // Load default presets - fields must be in declaration order
  VoiceClipEdit voiceCleanup;
  voiceCleanup.normalizeEnabled = true;
  voiceCleanup.normalizeTargetDbFS = -3.0f;
  voiceCleanup.highPassEnabled = true;
  voiceCleanup.highPassFreqHz = 80.0f;

  VoiceClipEdit podcastQuality;
  podcastQuality.normalizeEnabled = true;
  podcastQuality.normalizeTargetDbFS = -1.0f;
  podcastQuality.highPassEnabled = true;
  podcastQuality.highPassFreqHz = 100.0f;
  podcastQuality.lowPassEnabled = true;
  podcastQuality.lowPassFreqHz = 12000.0f;
  podcastQuality.noiseGateEnabled = true;
  podcastQuality.noiseGateThresholdDb = -40.0f;

  VoiceClipEdit naturalVoice;
  naturalVoice.preGainDb = 3.0f;
  naturalVoice.highPassEnabled = true;
  naturalVoice.highPassFreqHz = 60.0f;

  m_presets = {
      {"(No Preset)", VoiceClipEdit{}},
      {"Voice Cleanup", voiceCleanup},
      {"Podcast Quality", podcastQuality},
      {"Natural Voice", naturalVoice},
  };

  // Populate preset combo
  if (m_presetCombo) {
    m_presetCombo->clear();
    for (const auto &preset : m_presets) {
      m_presetCombo->addItem(preset.name);
    }
  }

  // Refresh audio device list
  refreshDeviceList();

  // Set up media player
  setupMediaPlayer();

  // Set up audio recorder
  setupRecorder();
}

void NMVoiceStudioPanel::onShutdown() {
  if (m_updateTimer) {
    m_updateTimer->stop();
  }

  if (m_mediaPlayer) {
    m_mediaPlayer->stop();
  }

  // Shutdown audio recorder
  if (m_recorder) {
    m_recorder->shutdown();
  }
}

void NMVoiceStudioPanel::onUpdate(double) {
  // Updates happen via timer
}

void NMVoiceStudioPanel::setManifest(audio::VoiceManifest *manifest) {
  m_manifest = manifest;
}

bool NMVoiceStudioPanel::loadFile(const QString &filePath) {
  if (!loadWavFile(filePath)) {
    NMMessageDialog::showWarning(
        this, tr("Error"), tr("Could not load audio file: %1").arg(filePath));
    return false;
  }

  m_currentFilePath = filePath;
  m_lastSavedEdit = m_clip->edit;
  m_undoStack->clear();

  if (m_waveformWidget) {
    m_waveformWidget->setClip(m_clip.get());
    m_waveformWidget->zoomToFit();
  }

  updateUI();
  updateStatusBar();

  return true;
}

bool NMVoiceStudioPanel::loadFromLineId(const QString &lineId,
                                        const QString &locale) {
  if (!m_manifest)
    return false;

  m_currentLineId = lineId;
  m_currentLocale = locale;

  auto *line = m_manifest->getLine(lineId.toStdString());
  if (!line)
    return false;

  // Get the file for this locale
  auto *localeFile = line->getFile(locale.toStdString());
  if (!localeFile || localeFile->filePath.empty()) {
    return false;
  }

  return loadFile(QString::fromStdString(localeFile->filePath));
}

bool NMVoiceStudioPanel::hasUnsavedChanges() const {
  if (!m_clip)
    return false;

  // Compare current edit with last saved
  return m_clip->edit.hasEdits() &&
         (m_clip->edit.trimStartSamples != m_lastSavedEdit.trimStartSamples ||
          m_clip->edit.trimEndSamples != m_lastSavedEdit.trimEndSamples ||
          m_clip->edit.fadeInMs != m_lastSavedEdit.fadeInMs ||
          m_clip->edit.fadeOutMs != m_lastSavedEdit.fadeOutMs ||
          m_clip->edit.preGainDb != m_lastSavedEdit.preGainDb);
}

// ============================================================================
// Slot Implementations
// ============================================================================

void NMVoiceStudioPanel::onInputDeviceChanged(int index) {
  if (!m_recorder || !m_recorder->isInitialized() || !m_inputDeviceCombo)
    return;

  // Get device ID from combo box data
  QString deviceId = m_inputDeviceCombo->itemData(index).toString();

  // Set device on recorder
  auto result = m_recorder->setInputDevice(deviceId.toStdString());
  if (!result) {
    NOVELMIND_LOG_ERROR("Failed to set input device: {}", result.error());
    NMMessageDialog::showWarning(this, tr("Device Error"),
                                 QString::fromStdString(result.error()));
  } else {
    NOVELMIND_LOG_INFO("Input device changed to: {}",
                       m_inputDeviceCombo->currentText().toStdString());
  }
}

void NMVoiceStudioPanel::onRecordClicked() {
  if (!m_recorder || !m_recorder->isInitialized()) {
    NMMessageDialog::showWarning(this, tr("Recording Error"),
                                 tr("Audio recorder is not initialized"));
    return;
  }

  // Generate temporary recording path
  QString tempDir = QDir::temp().filePath("NovelMind/voice_recordings");
  QDir().mkpath(tempDir);

  QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
  m_tempRecordingPath = QDir(tempDir).filePath(QString("recording_%1.wav").arg(timestamp));

  // Start recording
  auto result = m_recorder->startRecording(m_tempRecordingPath.toStdString());
  if (!result) {
    NMMessageDialog::showWarning(this, tr("Recording Error"),
                                 QString::fromStdString(result.error()));
    return;
  }

  m_isRecording = true;
  if (m_recordBtn)
    m_recordBtn->setEnabled(false);
  if (m_stopRecordBtn)
    m_stopRecordBtn->setEnabled(true);
  if (m_cancelRecordBtn)
    m_cancelRecordBtn->setEnabled(true);
  if (m_recordingTimeLabel)
    m_recordingTimeLabel->setText("00:00.000");

  NOVELMIND_LOG_INFO("Recording started: {}", m_tempRecordingPath.toStdString());
}

void NMVoiceStudioPanel::onStopRecordClicked() {
  if (!m_recorder || !m_recorder->isRecording()) {
    return;
  }

  // Stop recording - onRecordingComplete callback will be triggered asynchronously
  auto result = m_recorder->stopRecording();
  if (!result) {
    NOVELMIND_LOG_ERROR("Failed to stop recording: {}", result.error());
    // Still update UI state even if stop failed
    m_isRecording = false;
    if (m_recordBtn)
      m_recordBtn->setEnabled(true);
    if (m_stopRecordBtn)
      m_stopRecordBtn->setEnabled(false);
    if (m_cancelRecordBtn)
      m_cancelRecordBtn->setEnabled(false);
    return;
  }

  // Disable buttons while processing (they'll be re-enabled in onRecordingComplete)
  if (m_stopRecordBtn)
    m_stopRecordBtn->setEnabled(false);
  if (m_cancelRecordBtn)
    m_cancelRecordBtn->setEnabled(false);

  if (m_statusLabel)
    m_statusLabel->setText(tr("Processing recording..."));

  NOVELMIND_LOG_INFO("Recording stopped, processing...");
}

void NMVoiceStudioPanel::onCancelRecordClicked() {
  if (!m_recorder || !m_recorder->isRecording()) {
    return;
  }

  // Cancel recording - this will discard the recorded data
  m_recorder->cancelRecording();

  m_isRecording = false;
  if (m_recordBtn)
    m_recordBtn->setEnabled(true);
  if (m_stopRecordBtn)
    m_stopRecordBtn->setEnabled(false);
  if (m_cancelRecordBtn)
    m_cancelRecordBtn->setEnabled(false);
  if (m_recordingTimeLabel)
    m_recordingTimeLabel->setText("00:00.000");

  if (m_statusLabel)
    m_statusLabel->setText(tr("Recording canceled"));

  // Clean up temp file if it exists
  if (!m_tempRecordingPath.isEmpty() && QFile::exists(m_tempRecordingPath)) {
    QFile::remove(m_tempRecordingPath);
  }
  m_tempRecordingPath.clear();

  NOVELMIND_LOG_INFO("Recording canceled");
}

void NMVoiceStudioPanel::onPlayClicked() {
  if (!m_clip || !m_mediaPlayer)
    return;

  auto &iconMgr = NMIconManager::instance();
  if (m_isPlaying) {
    // Pause
    m_mediaPlayer->pause();
    m_isPlaying = false;
    if (m_playBtn) {
      m_playBtn->setIcon(iconMgr.getIcon("play", 16));
      m_playBtn->setText(tr("Play"));
    }
  } else {
    // Render processed audio to temp file and play
    auto processed = renderProcessedAudio();
    if (processed.empty())
      return;

    // Create temp WAV file
    QString tempPath = QDir::tempPath() + "/novelmind_preview.wav";
    // Save temp file
    // (simplified - in real implementation would write proper WAV)

    m_mediaPlayer->setSource(QUrl::fromLocalFile(m_currentFilePath));
    m_mediaPlayer->play();
    m_isPlaying = true;
    if (m_playBtn) {
      m_playBtn->setIcon(iconMgr.getIcon("pause", 16));
      m_playBtn->setText(tr("Pause"));
    }
  }

  updatePlaybackState();
}

void NMVoiceStudioPanel::onStopClicked() {
  if (m_mediaPlayer) {
    m_mediaPlayer->stop();
  }
  m_isPlaying = false;
  if (m_playBtn) {
    auto &iconMgr = NMIconManager::instance();
    m_playBtn->setIcon(iconMgr.getIcon("play", 16));
    m_playBtn->setText(tr("Play"));
  }
  if (m_waveformWidget) {
    m_waveformWidget->setPlayheadPosition(0.0);
  }
  updatePlaybackState();
}

void NMVoiceStudioPanel::onLoopClicked(bool checked) {
  m_isLooping = checked;
  if (m_mediaPlayer) {
    m_mediaPlayer->setLoops(checked ? QMediaPlayer::Infinite : 1);
  }
}

void NMVoiceStudioPanel::onTrimToSelection() {
  if (!m_clip || !m_waveformWidget)
    return;

  double selStart = m_waveformWidget->getSelectionStart();
  double selEnd = m_waveformWidget->getSelectionEnd();

  if (selStart >= selEnd) {
    NMMessageDialog::showInfo(this, tr("Trim"),
                              tr("Please select a region to keep."));
    return;
  }

  VoiceClipEdit oldEdit = m_clip->edit;
  VoiceClipEdit newEdit = m_clip->edit;

  uint32_t sampleRate = m_clip->format.sampleRate;
  newEdit.trimStartSamples =
      static_cast<int64_t>(selStart * static_cast<double>(sampleRate));
  newEdit.trimEndSamples =
      static_cast<int64_t>((m_clip->getDurationSeconds() - selEnd) *
                           static_cast<double>(sampleRate));

  auto *cmd = new EditParamCommand(&m_clip->edit, oldEdit, newEdit,
                                   tr("Trim to Selection"));
  m_undoStack->push(cmd);

  m_waveformWidget->clearSelection();
  m_waveformWidget->update();
  updateEditControls();
}

void NMVoiceStudioPanel::onResetTrim() {
  if (!m_clip)
    return;

  VoiceClipEdit oldEdit = m_clip->edit;
  VoiceClipEdit newEdit = m_clip->edit;
  newEdit.trimStartSamples = 0;
  newEdit.trimEndSamples = 0;

  auto *cmd =
      new EditParamCommand(&m_clip->edit, oldEdit, newEdit, tr("Reset Trim"));
  m_undoStack->push(cmd);

  if (m_waveformWidget) {
    m_waveformWidget->update();
  }
  updateEditControls();
}

void NMVoiceStudioPanel::onFadeInChanged(double value) {
  if (!m_clip)
    return;
  m_clip->edit.fadeInMs = static_cast<float>(value);
}

void NMVoiceStudioPanel::onFadeOutChanged(double value) {
  if (!m_clip)
    return;
  m_clip->edit.fadeOutMs = static_cast<float>(value);
}

void NMVoiceStudioPanel::onPreGainChanged(double value) {
  if (!m_clip)
    return;
  m_clip->edit.preGainDb = static_cast<float>(value);
}

void NMVoiceStudioPanel::onNormalizeToggled(bool checked) {
  if (!m_clip)
    return;
  m_clip->edit.normalizeEnabled = checked;
  if (m_normalizeTargetSpin) {
    m_normalizeTargetSpin->setEnabled(checked);
  }
}

void NMVoiceStudioPanel::onNormalizeTargetChanged(double value) {
  if (!m_clip)
    return;
  m_clip->edit.normalizeTargetDbFS = static_cast<float>(value);
}

void NMVoiceStudioPanel::onHighPassToggled(bool checked) {
  if (!m_clip)
    return;
  m_clip->edit.highPassEnabled = checked;
  if (m_highPassFreqSpin) {
    m_highPassFreqSpin->setEnabled(checked);
  }
}

void NMVoiceStudioPanel::onHighPassFreqChanged(double value) {
  if (!m_clip)
    return;
  m_clip->edit.highPassFreqHz = static_cast<float>(value);
}

void NMVoiceStudioPanel::onLowPassToggled(bool checked) {
  if (!m_clip)
    return;
  m_clip->edit.lowPassEnabled = checked;
  if (m_lowPassFreqSpin) {
    m_lowPassFreqSpin->setEnabled(checked);
  }
}

void NMVoiceStudioPanel::onLowPassFreqChanged(double value) {
  if (!m_clip)
    return;
  m_clip->edit.lowPassFreqHz = static_cast<float>(value);
}

void NMVoiceStudioPanel::onEQToggled(bool checked) {
  if (!m_clip)
    return;
  m_clip->edit.eqEnabled = checked;
  if (m_eqLowSpin)
    m_eqLowSpin->setEnabled(checked);
  if (m_eqMidSpin)
    m_eqMidSpin->setEnabled(checked);
  if (m_eqHighSpin)
    m_eqHighSpin->setEnabled(checked);
}

void NMVoiceStudioPanel::onEQLowChanged(double value) {
  if (!m_clip)
    return;
  m_clip->edit.eqLowGainDb = static_cast<float>(value);
}

void NMVoiceStudioPanel::onEQMidChanged(double value) {
  if (!m_clip)
    return;
  m_clip->edit.eqMidGainDb = static_cast<float>(value);
}

void NMVoiceStudioPanel::onEQHighChanged(double value) {
  if (!m_clip)
    return;
  m_clip->edit.eqHighGainDb = static_cast<float>(value);
}

void NMVoiceStudioPanel::onNoiseGateToggled(bool checked) {
  if (!m_clip)
    return;
  m_clip->edit.noiseGateEnabled = checked;
  if (m_noiseGateThresholdSpin) {
    m_noiseGateThresholdSpin->setEnabled(checked);
  }
}

void NMVoiceStudioPanel::onNoiseGateThresholdChanged(double value) {
  if (!m_clip)
    return;
  m_clip->edit.noiseGateThresholdDb = static_cast<float>(value);
}

void NMVoiceStudioPanel::onPresetSelected(int index) {
  if (index < 0 || index >= static_cast<int>(m_presets.size()))
    return;
  applyPreset(m_presets[static_cast<size_t>(index)].name);
}

void NMVoiceStudioPanel::onSavePresetClicked() {
  if (!m_clip)
    return;

  bool ok;
  QString name =
      NMInputDialog::getText(this, tr("Save Preset"), tr("Preset name:"),
                             QLineEdit::Normal, QString(), &ok);
  if (!ok || name.isEmpty())
    return;

  // Add or update preset
  bool found = false;
  for (auto &preset : m_presets) {
    if (preset.name == name) {
      preset.edit = m_clip->edit;
      found = true;
      break;
    }
  }

  if (!found) {
    m_presets.push_back({name, m_clip->edit});
    if (m_presetCombo) {
      m_presetCombo->addItem(name);
    }
  }
}

void NMVoiceStudioPanel::onSaveClicked() {
  if (m_currentFilePath.isEmpty()) {
    onSaveAsClicked();
    return;
  }

  if (saveWavFile(m_currentFilePath)) {
    m_lastSavedEdit = m_clip->edit;
    updateStatusBar();
    emit fileSaved(m_currentFilePath);
  }
}

void NMVoiceStudioPanel::onSaveAsClicked() {
  QString filePath =
      NMFileDialog::getSaveFileName(this, tr("Save Audio File"), QString(),
                                    tr("WAV Files (*.wav);;All Files (*)"));

  if (filePath.isEmpty())
    return;

  if (!filePath.endsWith(".wav", Qt::CaseInsensitive)) {
    filePath += ".wav";
  }

  if (saveWavFile(filePath)) {
    m_currentFilePath = filePath;
    m_lastSavedEdit = m_clip->edit;
    updateStatusBar();
    emit fileSaved(filePath);
  }
}

void NMVoiceStudioPanel::onExportClicked() {
  if (!m_clip)
    return;

  QString filePath = NMFileDialog::getSaveFileName(
      this, tr("Export Processed Audio"), QString(),
      tr("WAV Files (*.wav);;All Files (*)"));

  if (filePath.isEmpty())
    return;

  if (!filePath.endsWith(".wav", Qt::CaseInsensitive)) {
    filePath += ".wav";
  }

  // Render and save
  auto processed = renderProcessedAudio();
  if (processed.empty()) {
    NMMessageDialog::showWarning(this, tr("Export Error"),
                                 tr("No audio to export."));
    return;
  }

  // Create new clip with processed audio
  auto exportClip = std::make_unique<VoiceClip>();
  exportClip->samples = processed;
  exportClip->format = m_clip->format;
  exportClip->sourcePath = filePath.toStdString();

  // Save the processed audio (edit params already applied)
  std::swap(m_clip, exportClip);
  bool success = saveWavFile(filePath);
  std::swap(m_clip, exportClip);

  if (success) {
    NMMessageDialog::showInfo(this, tr("Export Complete"),
                              tr("Audio exported to: %1").arg(filePath));
  }
}

void NMVoiceStudioPanel::onOpenClicked() {
  if (hasUnsavedChanges()) {
    auto result = NMMessageDialog::showQuestion(
        this, tr("Unsaved Changes"),
        tr("You have unsaved changes. Do you want to save them first?"),
        {NMDialogButton::Save, NMDialogButton::Discard, NMDialogButton::Cancel},
        NMDialogButton::Save);

    if (result == NMDialogButton::Save) {
      onSaveClicked();
    } else if (result == NMDialogButton::Cancel) {
      return;
    }
  }

  QString filePath = NMFileDialog::getOpenFileName(
      this, tr("Open Audio File"), QString(),
      tr("Audio Files (*.wav *.ogg *.mp3 *.flac);;All Files (*)"));

  if (!filePath.isEmpty()) {
    loadFile(filePath);
  }
}

void NMVoiceStudioPanel::onUndoClicked() {
  m_undoStack->undo();
  updateEditControls();
  if (m_waveformWidget) {
    m_waveformWidget->update();
  }
}

void NMVoiceStudioPanel::onRedoClicked() {
  m_undoStack->redo();
  updateEditControls();
  if (m_waveformWidget) {
    m_waveformWidget->update();
  }
}

void NMVoiceStudioPanel::onWaveformSelectionChanged(double start, double end) {
  // Update selection info in UI if needed
  Q_UNUSED(start);
  Q_UNUSED(end);
}

void NMVoiceStudioPanel::onWaveformPlayheadClicked(double seconds) {
  if (m_mediaPlayer && m_clip) {
    m_mediaPlayer->setPosition(static_cast<qint64>(seconds * 1000));
  }
}

void NMVoiceStudioPanel::onPlaybackPositionChanged(qint64 position) {
  double seconds = static_cast<double>(position) / 1000.0;
  if (m_waveformWidget) {
    m_waveformWidget->setPlayheadPosition(seconds);
  }
  if (m_positionLabel) {
    m_positionLabel->setText(formatTimeMs(seconds));
  }
}

void NMVoiceStudioPanel::onPlaybackStateChanged() {
  if (!m_mediaPlayer)
    return;

  auto state = m_mediaPlayer->playbackState();
  m_isPlaying = (state == QMediaPlayer::PlayingState);

  if (m_playBtn) {
    auto &iconMgr = NMIconManager::instance();
    if (m_isPlaying) {
      m_playBtn->setIcon(iconMgr.getIcon("pause", 16));
      m_playBtn->setText(tr("Pause"));
    } else {
      m_playBtn->setIcon(iconMgr.getIcon("play", 16));
      m_playBtn->setText(tr("Play"));
    }
  }

  updatePlaybackState();
}

void NMVoiceStudioPanel::onPlaybackMediaStatusChanged() {
  // Handle media status changes (loaded, buffering, etc.)
}

void NMVoiceStudioPanel::onLevelUpdate(const audio::LevelMeter &level) {
  if (m_vuMeter) {
    m_vuMeter->setLevel(level.rmsLevelDb, level.peakLevelDb, level.clipping);
  }
  if (m_levelLabel) {
    m_levelLabel->setText(QString("RMS: %1 dB  Peak: %2 dB")
                              .arg(level.rmsLevelDb, 0, 'f', 1)
                              .arg(level.peakLevelDb, 0, 'f', 1));
  }
}

void NMVoiceStudioPanel::onRecordingStateChanged(int state) {
  Q_UNUSED(state);
  // Update recording UI based on state
}

void NMVoiceStudioPanel::onRecordingComplete(
    const audio::RecordingResult &result) {
  m_isRecording = false;
  if (m_recordBtn)
    m_recordBtn->setEnabled(true);
  if (m_stopRecordBtn)
    m_stopRecordBtn->setEnabled(false);
  if (m_cancelRecordBtn)
    m_cancelRecordBtn->setEnabled(false);

  // Load the recorded file
  loadFile(QString::fromStdString(result.filePath));

  emit recordingCompleted(QString::fromStdString(result.filePath));
}

void NMVoiceStudioPanel::onRecordingError(const QString &error) {
  m_isRecording = false;
  if (m_recordBtn)
    m_recordBtn->setEnabled(true);
  if (m_stopRecordBtn)
    m_stopRecordBtn->setEnabled(false);
  if (m_cancelRecordBtn)
    m_cancelRecordBtn->setEnabled(false);

  NMMessageDialog::showWarning(this, tr("Recording Error"), error);
}

void NMVoiceStudioPanel::onUpdateTimer() {
  if (m_isRecording && m_recordingTimeLabel) {
    // Update recording time display
    // (Would get time from recorder in real implementation)
  }

  if (m_isPlaying && m_mediaPlayer) {
    qint64 pos = m_mediaPlayer->position();
    onPlaybackPositionChanged(pos);
  }
}

// ============================================================================
// UI Setup
// ============================================================================

void NMVoiceStudioPanel::setupUI() {
  m_contentWidget = new QWidget(this);
  auto *mainLayout = new QVBoxLayout(m_contentWidget);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // Toolbar
  setupToolbar();
  mainLayout->addWidget(m_toolbar);

  // Main splitter (left column / right column)
  m_mainSplitter = new QSplitter(Qt::Horizontal, m_contentWidget);

  // Left column
  auto *leftWidget = new QWidget(m_mainSplitter);
  auto *leftLayout = new QVBoxLayout(leftWidget);
  leftLayout->setContentsMargins(8, 8, 8, 8);
  leftLayout->setSpacing(8);

  setupDeviceSection();
  leftLayout->addWidget(m_deviceGroup);

  // Recording controls
  auto *recordGroup = new QGroupBox(tr("Recording"), leftWidget);
  auto *recordLayout = new QVBoxLayout(recordGroup);

  auto *recordBtnLayout = new QHBoxLayout();
  auto &iconMgr = NMIconManager::instance();

  m_recordBtn = new QPushButton(tr("Record"), recordGroup);
  m_recordBtn->setIcon(iconMgr.getIcon("record", 16));
  m_recordBtn->setStyleSheet("QPushButton { background-color: #c44; color: "
                             "white; font-weight: bold; }");
  m_recordBtn->setToolTip(tr("Start recording (R)"));
  connect(m_recordBtn, &QPushButton::clicked, this,
          &NMVoiceStudioPanel::onRecordClicked);
  recordBtnLayout->addWidget(m_recordBtn);

  m_stopRecordBtn = new QPushButton(tr("Stop"), recordGroup);
  m_stopRecordBtn->setIcon(iconMgr.getIcon("stop", 16));
  m_stopRecordBtn->setEnabled(false);
  connect(m_stopRecordBtn, &QPushButton::clicked, this,
          &NMVoiceStudioPanel::onStopRecordClicked);
  recordBtnLayout->addWidget(m_stopRecordBtn);

  m_cancelRecordBtn = new QPushButton(tr("Cancel"), recordGroup);
  m_cancelRecordBtn->setIcon(iconMgr.getIcon("file-close", 16));
  m_cancelRecordBtn->setEnabled(false);
  connect(m_cancelRecordBtn, &QPushButton::clicked, this,
          &NMVoiceStudioPanel::onCancelRecordClicked);
  recordBtnLayout->addWidget(m_cancelRecordBtn);

  recordLayout->addLayout(recordBtnLayout);

  m_recordingTimeLabel = new QLabel("00:00.000", recordGroup);
  m_recordingTimeLabel->setStyleSheet(
      "font-family: monospace; font-size: 14px;");
  m_recordingTimeLabel->setAlignment(Qt::AlignCenter);
  recordLayout->addWidget(m_recordingTimeLabel);

  leftLayout->addWidget(recordGroup);

  setupPresetSection();
  leftLayout->addWidget(m_presetCombo->parentWidget());

  leftLayout->addStretch();

  // Right column
  auto *rightWidget = new QWidget(m_mainSplitter);
  auto *rightLayout = new QVBoxLayout(rightWidget);
  rightLayout->setContentsMargins(8, 8, 8, 8);
  rightLayout->setSpacing(8);

  setupTransportSection();
  rightLayout->addWidget(m_transportGroup);

  setupWaveformSection();
  rightLayout->addWidget(m_waveformScroll);

  setupEditSection();
  rightLayout->addWidget(m_editGroup);

  setupFilterSection();
  rightLayout->addWidget(m_filterGroup);

  m_mainSplitter->addWidget(leftWidget);
  m_mainSplitter->addWidget(rightWidget);
  m_mainSplitter->setStretchFactor(0, 1);
  m_mainSplitter->setStretchFactor(1, 2);

  mainLayout->addWidget(m_mainSplitter, 1);

  // Status bar
  setupStatusBar();
  mainLayout->addWidget(m_statusLabel->parentWidget());

  setWidget(m_contentWidget);
}

void NMVoiceStudioPanel::setupToolbar() {
  m_toolbar = new QToolBar(m_contentWidget);
  m_toolbar->setIconSize(QSize(16, 16));
  auto &iconMgr = NMIconManager::instance();

  auto *openAction = m_toolbar->addAction(iconMgr.getIcon("file-open", 16), tr("Open"));
  openAction->setToolTip(tr("Open audio file (Ctrl+O)"));
  connect(openAction, &QAction::triggered, this,
          &NMVoiceStudioPanel::onOpenClicked);

  auto *saveAction = m_toolbar->addAction(iconMgr.getIcon("file-save", 16), tr("Save"));
  saveAction->setToolTip(tr("Save (Ctrl+S)"));
  connect(saveAction, &QAction::triggered, this,
          &NMVoiceStudioPanel::onSaveClicked);

  auto *saveAsAction = m_toolbar->addAction(iconMgr.getIcon("file-save", 16), tr("Save As"));
  saveAsAction->setToolTip(tr("Save As"));
  connect(saveAsAction, &QAction::triggered, this,
          &NMVoiceStudioPanel::onSaveAsClicked);

  auto *exportAction = m_toolbar->addAction(iconMgr.getIcon("export", 16), tr("Export"));
  exportAction->setToolTip(tr("Export processed audio (Ctrl+E)"));
  connect(exportAction, &QAction::triggered, this,
          &NMVoiceStudioPanel::onExportClicked);

  m_toolbar->addSeparator();

  auto *undoAction = m_toolbar->addAction(iconMgr.getIcon("edit-undo", 16), tr("Undo"));
  undoAction->setToolTip(tr("Undo (Ctrl+Z)"));
  connect(undoAction, &QAction::triggered, this,
          &NMVoiceStudioPanel::onUndoClicked);

  auto *redoAction = m_toolbar->addAction(iconMgr.getIcon("edit-redo", 16), tr("Redo"));
  redoAction->setToolTip(tr("Redo (Ctrl+Y)"));
  connect(redoAction, &QAction::triggered, this,
          &NMVoiceStudioPanel::onRedoClicked);
}

void NMVoiceStudioPanel::setupDeviceSection() {
  m_deviceGroup = new QGroupBox(tr("Input Device"), m_contentWidget);
  auto *layout = new QVBoxLayout(m_deviceGroup);

  m_inputDeviceCombo = new QComboBox(m_deviceGroup);
  m_inputDeviceCombo->setToolTip(tr("Select microphone"));
  connect(m_inputDeviceCombo,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &NMVoiceStudioPanel::onInputDeviceChanged);
  layout->addWidget(m_inputDeviceCombo);

  // Input gain
  auto *gainLayout = new QHBoxLayout();
  gainLayout->addWidget(new QLabel(tr("Input Gain:"), m_deviceGroup));

  m_inputGainSlider = new QSlider(Qt::Horizontal, m_deviceGroup);
  m_inputGainSlider->setRange(-12, 12);
  m_inputGainSlider->setValue(0);
  m_inputGainSlider->setToolTip(tr("Adjust microphone input level"));
  gainLayout->addWidget(m_inputGainSlider);

  m_inputGainLabel = new QLabel("0 dB", m_deviceGroup);
  m_inputGainLabel->setMinimumWidth(40);
  gainLayout->addWidget(m_inputGainLabel);

  layout->addLayout(gainLayout);

  // VU Meter
  m_vuMeter = new StudioVUMeterWidget(m_deviceGroup);
  layout->addWidget(m_vuMeter);

  m_levelLabel = new QLabel(tr("RMS: -60 dB  Peak: -60 dB"), m_deviceGroup);
  m_levelLabel->setStyleSheet("font-family: monospace; font-size: 10px;");
  layout->addWidget(m_levelLabel);
}

void NMVoiceStudioPanel::setupTransportSection() {
  m_transportGroup = new QGroupBox(tr("Transport"), m_contentWidget);
  auto *layout = new QHBoxLayout(m_transportGroup);
  auto &iconMgr = NMIconManager::instance();

  m_playBtn = new QPushButton(tr("Play"), m_transportGroup);
  m_playBtn->setIcon(iconMgr.getIcon("play", 16));
  m_playBtn->setToolTip(tr("Play/Pause (Space)"));
  connect(m_playBtn, &QPushButton::clicked, this,
          &NMVoiceStudioPanel::onPlayClicked);
  layout->addWidget(m_playBtn);

  m_stopBtn = new QPushButton(tr("Stop"), m_transportGroup);
  m_stopBtn->setIcon(iconMgr.getIcon("stop", 16));
  m_stopBtn->setToolTip(tr("Stop playback"));
  connect(m_stopBtn, &QPushButton::clicked, this,
          &NMVoiceStudioPanel::onStopClicked);
  layout->addWidget(m_stopBtn);

  m_loopBtn = new QPushButton(tr("Loop"), m_transportGroup);
  m_loopBtn->setIcon(iconMgr.getIcon("loop", 16));
  m_loopBtn->setCheckable(true);
  m_loopBtn->setToolTip(tr("Toggle loop (L)"));
  connect(m_loopBtn, &QPushButton::toggled, this,
          &NMVoiceStudioPanel::onLoopClicked);
  layout->addWidget(m_loopBtn);

  layout->addStretch();

  m_positionLabel = new QLabel("00:00.000", m_transportGroup);
  m_positionLabel->setStyleSheet("font-family: monospace;");
  layout->addWidget(m_positionLabel);

  layout->addWidget(new QLabel("/", m_transportGroup));

  m_durationLabel = new QLabel("00:00.000", m_transportGroup);
  m_durationLabel->setStyleSheet("font-family: monospace;");
  layout->addWidget(m_durationLabel);

  layout->addStretch();

  // Zoom controls
  layout->addWidget(new QLabel(tr("Zoom:"), m_transportGroup));

  auto *zoomOutBtn = new QPushButton("-", m_transportGroup);
  zoomOutBtn->setFixedWidth(30);
  zoomOutBtn->setToolTip(tr("Zoom out (-)"));
  connect(zoomOutBtn, &QPushButton::clicked, this, [this]() {
    if (m_waveformWidget)
      m_waveformWidget->zoomOut();
  });
  layout->addWidget(zoomOutBtn);

  m_zoomSlider = new QSlider(Qt::Horizontal, m_transportGroup);
  m_zoomSlider->setRange(1, 100);
  m_zoomSlider->setValue(50);
  m_zoomSlider->setMaximumWidth(80);
  layout->addWidget(m_zoomSlider);

  auto *zoomInBtn = new QPushButton("+", m_transportGroup);
  zoomInBtn->setFixedWidth(30);
  zoomInBtn->setToolTip(tr("Zoom in (+)"));
  connect(zoomInBtn, &QPushButton::clicked, this, [this]() {
    if (m_waveformWidget)
      m_waveformWidget->zoomIn();
  });
  layout->addWidget(zoomInBtn);

  auto *zoomFitBtn = new QPushButton(tr("Fit"), m_transportGroup);
  zoomFitBtn->setToolTip(tr("Zoom to fit (F)"));
  connect(zoomFitBtn, &QPushButton::clicked, this, [this]() {
    if (m_waveformWidget)
      m_waveformWidget->zoomToFit();
  });
  layout->addWidget(zoomFitBtn);
}

void NMVoiceStudioPanel::setupWaveformSection() {
  m_waveformScroll = new QScrollArea(m_contentWidget);
  m_waveformScroll->setWidgetResizable(true);
  m_waveformScroll->setMinimumHeight(150);

  m_waveformWidget = new WaveformWidget(m_waveformScroll);
  connect(m_waveformWidget, &WaveformWidget::selectionChanged, this,
          &NMVoiceStudioPanel::onWaveformSelectionChanged);
  connect(m_waveformWidget, &WaveformWidget::playheadClicked, this,
          &NMVoiceStudioPanel::onWaveformPlayheadClicked);

  m_waveformScroll->setWidget(m_waveformWidget);
}

void NMVoiceStudioPanel::setupEditSection() {
  m_editGroup = new QGroupBox(tr("Edit"), m_contentWidget);
  m_editGroup->setCheckable(true);
  m_editGroup->setChecked(true);
  auto *layout = new QGridLayout(m_editGroup);
  auto &iconMgr = NMIconManager::instance();

  // Trim controls
  layout->addWidget(new QLabel(tr("Trim:"), m_editGroup), 0, 0);

  m_trimToSelectionBtn =
      new QPushButton(tr("Trim to Selection"), m_editGroup);
  m_trimToSelectionBtn->setIcon(iconMgr.getIcon("edit-cut", 16));
  m_trimToSelectionBtn->setToolTip(
      tr("Remove audio outside selection (Ctrl+T)"));
  connect(m_trimToSelectionBtn, &QPushButton::clicked, this,
          &NMVoiceStudioPanel::onTrimToSelection);
  layout->addWidget(m_trimToSelectionBtn, 0, 1);

  m_resetTrimBtn = new QPushButton(tr("Reset Trim"), m_editGroup);
  m_resetTrimBtn->setIcon(iconMgr.getIcon("property-reset", 16));
  m_resetTrimBtn->setToolTip(tr("Reset trim markers"));
  connect(m_resetTrimBtn, &QPushButton::clicked, this,
          &NMVoiceStudioPanel::onResetTrim);
  layout->addWidget(m_resetTrimBtn, 0, 2);

  // Fade controls
  layout->addWidget(new QLabel(tr("Fade In:"), m_editGroup), 1, 0);
  m_fadeInSpin = new QDoubleSpinBox(m_editGroup);
  m_fadeInSpin->setRange(0.0, 5000.0);
  m_fadeInSpin->setValue(0.0);
  m_fadeInSpin->setSuffix(" ms");
  m_fadeInSpin->setToolTip(tr("Fade in duration in milliseconds"));
  connect(m_fadeInSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMVoiceStudioPanel::onFadeInChanged);
  layout->addWidget(m_fadeInSpin, 1, 1);

  layout->addWidget(new QLabel(tr("Fade Out:"), m_editGroup), 1, 2);
  m_fadeOutSpin = new QDoubleSpinBox(m_editGroup);
  m_fadeOutSpin->setRange(0.0, 5000.0);
  m_fadeOutSpin->setValue(0.0);
  m_fadeOutSpin->setSuffix(" ms");
  m_fadeOutSpin->setToolTip(tr("Fade out duration in milliseconds"));
  connect(m_fadeOutSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMVoiceStudioPanel::onFadeOutChanged);
  layout->addWidget(m_fadeOutSpin, 1, 3);

  // Gain controls
  layout->addWidget(new QLabel(tr("Pre-Gain:"), m_editGroup), 2, 0);
  m_preGainSpin = new QDoubleSpinBox(m_editGroup);
  m_preGainSpin->setRange(-20.0, 20.0);
  m_preGainSpin->setValue(0.0);
  m_preGainSpin->setSuffix(" dB");
  m_preGainSpin->setToolTip(tr("Apply gain before processing"));
  connect(m_preGainSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMVoiceStudioPanel::onPreGainChanged);
  layout->addWidget(m_preGainSpin, 2, 1);

  // Normalize
  m_normalizeCheck = new QCheckBox(tr("Normalize to"), m_editGroup);
  m_normalizeCheck->setToolTip(tr("Automatically adjust peak level"));
  connect(m_normalizeCheck, &QCheckBox::toggled, this,
          &NMVoiceStudioPanel::onNormalizeToggled);
  layout->addWidget(m_normalizeCheck, 2, 2);

  m_normalizeTargetSpin = new QDoubleSpinBox(m_editGroup);
  m_normalizeTargetSpin->setRange(-20.0, 0.0);
  m_normalizeTargetSpin->setValue(-1.0);
  m_normalizeTargetSpin->setSuffix(" dBFS");
  m_normalizeTargetSpin->setEnabled(false);
  m_normalizeTargetSpin->setToolTip(tr("Target peak level for normalization"));
  connect(m_normalizeTargetSpin,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &NMVoiceStudioPanel::onNormalizeTargetChanged);
  layout->addWidget(m_normalizeTargetSpin, 2, 3);
}

void NMVoiceStudioPanel::setupFilterSection() {
  m_filterGroup = new QGroupBox(tr("Filters"), m_contentWidget);
  m_filterGroup->setCheckable(true);
  m_filterGroup->setChecked(true);
  auto *layout = new QGridLayout(m_filterGroup);

  // High-pass
  m_highPassCheck = new QCheckBox(tr("High-Pass Filter"), m_filterGroup);
  m_highPassCheck->setToolTip(
      tr("Remove low-frequency rumble below the cutoff"));
  connect(m_highPassCheck, &QCheckBox::toggled, this,
          &NMVoiceStudioPanel::onHighPassToggled);
  layout->addWidget(m_highPassCheck, 0, 0);

  m_highPassFreqSpin = new QDoubleSpinBox(m_filterGroup);
  m_highPassFreqSpin->setRange(20.0, 500.0);
  m_highPassFreqSpin->setValue(80.0);
  m_highPassFreqSpin->setSuffix(" Hz");
  m_highPassFreqSpin->setEnabled(false);
  connect(m_highPassFreqSpin,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &NMVoiceStudioPanel::onHighPassFreqChanged);
  layout->addWidget(m_highPassFreqSpin, 0, 1);

  // Low-pass
  m_lowPassCheck = new QCheckBox(tr("Low-Pass Filter"), m_filterGroup);
  m_lowPassCheck->setToolTip(tr("Remove high-frequency hiss above the cutoff"));
  connect(m_lowPassCheck, &QCheckBox::toggled, this,
          &NMVoiceStudioPanel::onLowPassToggled);
  layout->addWidget(m_lowPassCheck, 0, 2);

  m_lowPassFreqSpin = new QDoubleSpinBox(m_filterGroup);
  m_lowPassFreqSpin->setRange(1000.0, 20000.0);
  m_lowPassFreqSpin->setValue(12000.0);
  m_lowPassFreqSpin->setSuffix(" Hz");
  m_lowPassFreqSpin->setEnabled(false);
  connect(m_lowPassFreqSpin,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &NMVoiceStudioPanel::onLowPassFreqChanged);
  layout->addWidget(m_lowPassFreqSpin, 0, 3);

  // 3-Band EQ
  m_eqCheck = new QCheckBox(tr("3-Band EQ"), m_filterGroup);
  m_eqCheck->setToolTip(tr("Enable 3-band equalizer"));
  connect(m_eqCheck, &QCheckBox::toggled, this,
          &NMVoiceStudioPanel::onEQToggled);
  layout->addWidget(m_eqCheck, 1, 0);

  layout->addWidget(new QLabel(tr("Low:"), m_filterGroup), 1, 1);
  m_eqLowSpin = new QDoubleSpinBox(m_filterGroup);
  m_eqLowSpin->setRange(-12.0, 12.0);
  m_eqLowSpin->setValue(0.0);
  m_eqLowSpin->setSuffix(" dB");
  m_eqLowSpin->setEnabled(false);
  connect(m_eqLowSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMVoiceStudioPanel::onEQLowChanged);
  layout->addWidget(m_eqLowSpin, 1, 2);

  layout->addWidget(new QLabel(tr("Mid:"), m_filterGroup), 2, 1);
  m_eqMidSpin = new QDoubleSpinBox(m_filterGroup);
  m_eqMidSpin->setRange(-12.0, 12.0);
  m_eqMidSpin->setValue(0.0);
  m_eqMidSpin->setSuffix(" dB");
  m_eqMidSpin->setEnabled(false);
  connect(m_eqMidSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMVoiceStudioPanel::onEQMidChanged);
  layout->addWidget(m_eqMidSpin, 2, 2);

  layout->addWidget(new QLabel(tr("High:"), m_filterGroup), 2, 3);
  m_eqHighSpin = new QDoubleSpinBox(m_filterGroup);
  m_eqHighSpin->setRange(-12.0, 12.0);
  m_eqHighSpin->setValue(0.0);
  m_eqHighSpin->setSuffix(" dB");
  m_eqHighSpin->setEnabled(false);
  connect(m_eqHighSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &NMVoiceStudioPanel::onEQHighChanged);
  layout->addWidget(m_eqHighSpin, 2, 4);

  // Noise gate
  m_noiseGateCheck = new QCheckBox(tr("Noise Gate"), m_filterGroup);
  m_noiseGateCheck->setToolTip(tr("Reduce audio below threshold"));
  connect(m_noiseGateCheck, &QCheckBox::toggled, this,
          &NMVoiceStudioPanel::onNoiseGateToggled);
  layout->addWidget(m_noiseGateCheck, 3, 0);

  layout->addWidget(new QLabel(tr("Threshold:"), m_filterGroup), 3, 1);
  m_noiseGateThresholdSpin = new QDoubleSpinBox(m_filterGroup);
  m_noiseGateThresholdSpin->setRange(-60.0, -20.0);
  m_noiseGateThresholdSpin->setValue(-40.0);
  m_noiseGateThresholdSpin->setSuffix(" dB");
  m_noiseGateThresholdSpin->setEnabled(false);
  connect(m_noiseGateThresholdSpin,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &NMVoiceStudioPanel::onNoiseGateThresholdChanged);
  layout->addWidget(m_noiseGateThresholdSpin, 3, 2);
}

void NMVoiceStudioPanel::setupPresetSection() {
  auto *presetGroup = new QGroupBox(tr("Presets"), m_contentWidget);
  auto *layout = new QVBoxLayout(presetGroup);
  auto &iconMgr = NMIconManager::instance();

  m_presetCombo = new QComboBox(presetGroup);
  m_presetCombo->setToolTip(tr("Select a processing preset"));
  connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &NMVoiceStudioPanel::onPresetSelected);
  layout->addWidget(m_presetCombo);

  m_savePresetBtn = new QPushButton(tr("Save Preset"), presetGroup);
  m_savePresetBtn->setIcon(iconMgr.getIcon("file-save", 16));
  m_savePresetBtn->setToolTip(tr("Save current settings as a preset"));
  connect(m_savePresetBtn, &QPushButton::clicked, this,
          &NMVoiceStudioPanel::onSavePresetClicked);
  layout->addWidget(m_savePresetBtn);
}

void NMVoiceStudioPanel::setupStatusBar() {
  auto *statusWidget = new QWidget(m_contentWidget);
  auto *layout = new QHBoxLayout(statusWidget);
  layout->setContentsMargins(8, 4, 8, 4);

  m_statusLabel = new QLabel(tr("Ready"), statusWidget);
  layout->addWidget(m_statusLabel);

  layout->addStretch();

  m_fileInfoLabel = new QLabel(statusWidget);
  m_fileInfoLabel->setStyleSheet("color: gray;");
  layout->addWidget(m_fileInfoLabel);

  m_progressBar = new QProgressBar(statusWidget);
  m_progressBar->setMaximumWidth(100);
  m_progressBar->setVisible(false);
  layout->addWidget(m_progressBar);
}

void NMVoiceStudioPanel::setupMediaPlayer() {
  m_mediaPlayer = new QMediaPlayer(this);
  m_audioOutput = new QAudioOutput(this);
  m_mediaPlayer->setAudioOutput(m_audioOutput);

  connect(m_mediaPlayer, &QMediaPlayer::positionChanged, this,
          &NMVoiceStudioPanel::onPlaybackPositionChanged);
  connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged, this,
          &NMVoiceStudioPanel::onPlaybackStateChanged);
  connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged, this,
          &NMVoiceStudioPanel::onPlaybackMediaStatusChanged);
}

void NMVoiceStudioPanel::setupRecorder() {
  // Initialize AudioRecorder
  m_recorder = std::make_unique<audio::AudioRecorder>();

  auto result = m_recorder->initialize();
  if (!result) {
    NOVELMIND_LOG_ERROR("Failed to initialize AudioRecorder: {}", result.error());
    return;
  }

  // Set up recording format
  audio::RecordingFormat format;
  format.sampleRate = 48000;
  format.channels = 1;  // Mono
  format.bitsPerSample = 16;
  format.outputFormat = audio::RecordingFormat::FileFormat::WAV;
  format.autoTrimSilence = false;  // User controls trimming manually
  format.normalize = false;  // User controls normalization manually
  m_recorder->setRecordingFormat(format);

  // Set up callbacks with Qt::QueuedConnection for thread safety
  m_recorder->setOnLevelUpdate([this](const audio::LevelMeter &level) {
    QMetaObject::invokeMethod(this, [this, level]() {
      onLevelUpdate(level);
    }, Qt::QueuedConnection);
  });

  m_recorder->setOnRecordingStateChanged([this](audio::RecordingState state) {
    QMetaObject::invokeMethod(this, [this, state]() {
      onRecordingStateChanged(static_cast<int>(state));
    }, Qt::QueuedConnection);
  });

  m_recorder->setOnRecordingComplete([this](const audio::RecordingResult &result) {
    QMetaObject::invokeMethod(this, [this, result]() {
      onRecordingComplete(result);
    }, Qt::QueuedConnection);
  });

  m_recorder->setOnRecordingError([this](const std::string &error) {
    QMetaObject::invokeMethod(this, [this, error]() {
      onRecordingError(QString::fromStdString(error));
    }, Qt::QueuedConnection);
  });

  // Start level metering for VU meter display
  m_recorder->startMetering();

  NOVELMIND_LOG_INFO("AudioRecorder initialized successfully");
}

// ============================================================================
// Helper Methods
// ============================================================================

void NMVoiceStudioPanel::refreshDeviceList() {
  if (!m_inputDeviceCombo)
    return;

  m_inputDeviceCombo->clear();

  // Use AudioRecorder device list if available
  if (m_recorder && m_recorder->isInitialized()) {
    auto devices = m_recorder->getInputDevices();
    for (const auto &device : devices) {
      QString name = QString::fromStdString(device.name);
      if (device.isDefault) {
        name += tr(" (Default)");
      }
      m_inputDeviceCombo->addItem(name, QString::fromStdString(device.id));
    }

    // Select current device
    const auto *currentDevice = m_recorder->getCurrentInputDevice();
    if (currentDevice) {
      QString currentId = QString::fromStdString(currentDevice->id);
      for (int i = 0; i < m_inputDeviceCombo->count(); ++i) {
        if (m_inputDeviceCombo->itemData(i).toString() == currentId) {
          m_inputDeviceCombo->setCurrentIndex(i);
          break;
        }
      }
    }
  } else {
    // Fallback to Qt devices if recorder not available
    m_inputDeviceCombo->addItem(tr("(Default Device)"));

    auto devices = QMediaDevices::audioInputs();
    for (const auto &device : devices) {
      QString name = device.description();
      if (device == QMediaDevices::defaultAudioInput()) {
        name += tr(" (Default)");
      }
      m_inputDeviceCombo->addItem(name);
    }
  }
}

void NMVoiceStudioPanel::updateUI() {
  bool hasClip = (m_clip != nullptr);

  if (m_playBtn)
    m_playBtn->setEnabled(hasClip);
  if (m_stopBtn)
    m_stopBtn->setEnabled(hasClip);
  if (m_loopBtn)
    m_loopBtn->setEnabled(hasClip);
  if (m_trimToSelectionBtn)
    m_trimToSelectionBtn->setEnabled(hasClip);
  if (m_resetTrimBtn)
    m_resetTrimBtn->setEnabled(hasClip);

  if (hasClip && m_durationLabel) {
    m_durationLabel->setText(formatTimeMs(m_clip->getDurationSeconds()));
  }

  updateEditControls();
}

void NMVoiceStudioPanel::updateEditControls() {
  if (!m_clip)
    return;

  // Update spinboxes without triggering signals
  if (m_fadeInSpin) {
    m_fadeInSpin->blockSignals(true);
    m_fadeInSpin->setValue(m_clip->edit.fadeInMs);
    m_fadeInSpin->blockSignals(false);
  }

  if (m_fadeOutSpin) {
    m_fadeOutSpin->blockSignals(true);
    m_fadeOutSpin->setValue(m_clip->edit.fadeOutMs);
    m_fadeOutSpin->blockSignals(false);
  }

  if (m_preGainSpin) {
    m_preGainSpin->blockSignals(true);
    m_preGainSpin->setValue(m_clip->edit.preGainDb);
    m_preGainSpin->blockSignals(false);
  }

  if (m_normalizeCheck) {
    m_normalizeCheck->blockSignals(true);
    m_normalizeCheck->setChecked(m_clip->edit.normalizeEnabled);
    m_normalizeCheck->blockSignals(false);
  }

  if (m_normalizeTargetSpin) {
    m_normalizeTargetSpin->blockSignals(true);
    m_normalizeTargetSpin->setValue(m_clip->edit.normalizeTargetDbFS);
    m_normalizeTargetSpin->setEnabled(m_clip->edit.normalizeEnabled);
    m_normalizeTargetSpin->blockSignals(false);
  }
}

void NMVoiceStudioPanel::updatePlaybackState() {
  // Update transport button states
}

void NMVoiceStudioPanel::updateStatusBar() {
  if (!m_clip) {
    if (m_statusLabel)
      m_statusLabel->setText(tr("Ready"));
    if (m_fileInfoLabel)
      m_fileInfoLabel->clear();
    return;
  }

  QString filename = m_currentFilePath;
  if (filename.isEmpty()) {
    filename = tr("Untitled");
  } else {
    qsizetype lastSlash = filename.lastIndexOf('/');
    if (lastSlash >= 0)
      filename = filename.mid(lastSlash + 1);
  }

  QString modified = m_clip->edit.hasEdits() ? " ●" : "";

  if (m_statusLabel) {
    m_statusLabel->setText(filename + modified);
  }

  if (m_fileInfoLabel) {
    m_fileInfoLabel->setText(
        QString("%1 Hz | %2 | %3-bit")
            .arg(m_clip->format.sampleRate)
            .arg(m_clip->format.channels == 1 ? "Mono" : "Stereo")
            .arg(m_clip->format.bitsPerSample));
  }
}

bool NMVoiceStudioPanel::loadWavFile(const QString &filePath) {
  std::ifstream file(filePath.toStdString(), std::ios::binary);
  if (!file.is_open())
    return false;

  // Read WAV header
  char riffHeader[4];
  file.read(riffHeader, 4);
  if (std::string(riffHeader, 4) != "RIFF")
    return false;

  uint32_t fileSize;
  file.read(reinterpret_cast<char *>(&fileSize), 4);

  char waveHeader[4];
  file.read(waveHeader, 4);
  if (std::string(waveHeader, 4) != "WAVE")
    return false;

  // Find fmt chunk
  char chunkId[4];
  uint32_t chunkSize;
  uint16_t audioFormat, numChannels;
  uint32_t sampleRate, byteRate;
  uint16_t blockAlign, bitsPerSample;

  while (file.read(chunkId, 4)) {
    file.read(reinterpret_cast<char *>(&chunkSize), 4);

    if (std::string(chunkId, 4) == "fmt ") {
      file.read(reinterpret_cast<char *>(&audioFormat), 2);
      file.read(reinterpret_cast<char *>(&numChannels), 2);
      file.read(reinterpret_cast<char *>(&sampleRate), 4);
      file.read(reinterpret_cast<char *>(&byteRate), 4);
      file.read(reinterpret_cast<char *>(&blockAlign), 2);
      file.read(reinterpret_cast<char *>(&bitsPerSample), 2);

      // Skip any extra format bytes
      if (chunkSize > 16) {
        file.seekg(chunkSize - 16, std::ios::cur);
      }
    } else if (std::string(chunkId, 4) == "data") {
      // Read audio data
      m_clip = std::make_unique<VoiceClip>();
      m_clip->format.sampleRate = sampleRate;
      m_clip->format.channels = static_cast<uint8_t>(numChannels);
      m_clip->format.bitsPerSample = static_cast<uint8_t>(bitsPerSample);
      m_clip->sourcePath = filePath.toStdString();

      size_t numSamples = chunkSize / (bitsPerSample / 8) / numChannels;
      m_clip->samples.resize(numSamples);

      if (bitsPerSample == 16) {
        std::vector<int16_t> rawSamples(numSamples * numChannels);
        file.read(reinterpret_cast<char *>(rawSamples.data()), chunkSize);

        // Convert to float and mono if needed
        for (size_t i = 0; i < numSamples; ++i) {
          float sample = 0.0f;
          for (int ch = 0; ch < numChannels; ++ch) {
            sample += static_cast<float>(
                          rawSamples[i * static_cast<size_t>(numChannels) +
                                     static_cast<size_t>(ch)]) /
                      32768.0f;
          }
          m_clip->samples[i] = sample / static_cast<float>(numChannels);
        }
      } else if (bitsPerSample == 24) {
        for (size_t i = 0; i < numSamples; ++i) {
          float sample = 0.0f;
          for (int ch = 0; ch < numChannels; ++ch) {
            uint8_t bytes[3];
            file.read(reinterpret_cast<char *>(bytes), 3);
            int32_t value = (bytes[2] << 16) | (bytes[1] << 8) | bytes[0];
            if (value & 0x800000)
              value = static_cast<int32_t>(static_cast<uint32_t>(value) |
                                           0xFF000000u); // Sign extend
            sample += static_cast<float>(value) / 8388608.0f;
          }
          m_clip->samples[i] = sample / static_cast<float>(numChannels);
        }
      } else if (bitsPerSample == 32) {
        std::vector<int32_t> rawSamples(numSamples * numChannels);
        file.read(reinterpret_cast<char *>(rawSamples.data()), chunkSize);

        for (size_t i = 0; i < numSamples; ++i) {
          float sample = 0.0f;
          for (int ch = 0; ch < numChannels; ++ch) {
            sample += static_cast<float>(
                          rawSamples[i * static_cast<size_t>(numChannels) +
                                     static_cast<size_t>(ch)]) /
                      2147483648.0f;
          }
          m_clip->samples[i] = sample / static_cast<float>(numChannels);
        }
      }

      return true;
    } else {
      // Skip unknown chunk
      file.seekg(chunkSize, std::ios::cur);
    }
  }

  return false;
}

bool NMVoiceStudioPanel::saveWavFile(const QString &filePath) {
  if (!m_clip)
    return false;

  std::ofstream file(filePath.toStdString(), std::ios::binary);
  if (!file.is_open())
    return false;

  const auto &samples = m_clip->samples;
  size_t numSamples = samples.size();
  uint32_t sampleRate = m_clip->format.sampleRate;
  uint16_t numChannels = 1; // Always mono output
  uint16_t bitsPerSample = 16;
  uint16_t blockAlign = numChannels * (bitsPerSample / 8);
  uint32_t byteRate = sampleRate * blockAlign;
  uint32_t dataSize = static_cast<uint32_t>(numSamples * blockAlign);
  uint32_t fileSize = 36 + dataSize;

  // Write RIFF header
  file.write("RIFF", 4);
  file.write(reinterpret_cast<char *>(&fileSize), 4);
  file.write("WAVE", 4);

  // Write fmt chunk
  file.write("fmt ", 4);
  uint32_t fmtSize = 16;
  file.write(reinterpret_cast<char *>(&fmtSize), 4);
  uint16_t audioFormat = 1; // PCM
  file.write(reinterpret_cast<char *>(&audioFormat), 2);
  file.write(reinterpret_cast<char *>(&numChannels), 2);
  file.write(reinterpret_cast<char *>(&sampleRate), 4);
  file.write(reinterpret_cast<char *>(&byteRate), 4);
  file.write(reinterpret_cast<char *>(&blockAlign), 2);
  file.write(reinterpret_cast<char *>(&bitsPerSample), 2);

  // Write data chunk
  file.write("data", 4);
  file.write(reinterpret_cast<char *>(&dataSize), 4);

  // Convert float samples to 16-bit PCM
  for (size_t i = 0; i < numSamples; ++i) {
    float sample = std::clamp(samples[i], -1.0f, 1.0f);
    int16_t pcmSample = static_cast<int16_t>(sample * 32767.0f);
    file.write(reinterpret_cast<char *>(&pcmSample), 2);
  }

  return true;
}

std::vector<float> NMVoiceStudioPanel::renderProcessedAudio() {
  if (!m_clip)
    return {};
  return AudioProcessor::process(m_clip->samples, m_clip->edit, m_clip->format);
}

void NMVoiceStudioPanel::applyPreset(const QString &presetName) {
  if (!m_clip)
    return;

  for (const auto &preset : m_presets) {
    if (preset.name == presetName) {
      VoiceClipEdit oldEdit = m_clip->edit;
      VoiceClipEdit newEdit = preset.edit;
      // Preserve trim settings when applying preset
      newEdit.trimStartSamples = oldEdit.trimStartSamples;
      newEdit.trimEndSamples = oldEdit.trimEndSamples;

      auto *cmd = new EditParamCommand(&m_clip->edit, oldEdit, newEdit,
                                       tr("Apply Preset: %1").arg(presetName));
      m_undoStack->push(cmd);

      updateEditControls();
      break;
    }
  }
}

void NMVoiceStudioPanel::pushUndoCommand(const QString &description) {
  Q_UNUSED(description);
  // Handled by EditParamCommand
}

QString NMVoiceStudioPanel::formatTime(double seconds) const {
  int totalSecs = static_cast<int>(seconds);
  int mins = totalSecs / 60;
  int secs = totalSecs % 60;
  return QString("%1:%2")
      .arg(mins, 2, 10, QChar('0'))
      .arg(secs, 2, 10, QChar('0'));
}

QString NMVoiceStudioPanel::formatTimeMs(double seconds) const {
  int totalMs = static_cast<int>(seconds * 1000);
  int mins = totalMs / 60000;
  int secs = (totalMs % 60000) / 1000;
  int ms = totalMs % 1000;
  return QString("%1:%2.%3")
      .arg(mins, 2, 10, QChar('0'))
      .arg(secs, 2, 10, QChar('0'))
      .arg(ms, 3, 10, QChar('0'));
}

} // namespace NovelMind::editor::qt
