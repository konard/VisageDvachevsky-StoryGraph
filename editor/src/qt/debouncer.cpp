/**
 * @file debouncer.cpp
 * @brief Implementation of Debouncer classes
 *
 * This file provides implementations for the Debouncer and PropertyDebouncer
 * classes. Moving the constructor, destructor, and slot implementations here
 * ensures proper vtable and moc-generated code linking.
 *
 * Qt's AUTOMOC will process this file and generate the necessary meta-object
 * code for Q_OBJECT classes defined in debouncer.hpp.
 */

#include "NovelMind/editor/qt/debouncer.hpp"

namespace NovelMind::editor::qt {

Debouncer::Debouncer(int delayMs, QObject* parent) : QObject(parent), m_delayMs(delayMs) {
  m_timer.setSingleShot(true);
  connect(&m_timer, &QTimer::timeout, this, &Debouncer::onTimeout);
}

Debouncer::~Debouncer() = default;

void Debouncer::onTimeout() {
  if (m_pendingCallback) {
    emit triggered();
    m_pendingCallback();
    m_pendingCallback = nullptr;
  }
}

PropertyDebouncer::PropertyDebouncer(int delayMs, QObject* parent) : Debouncer(delayMs, parent) {}

PropertyDebouncer::~PropertyDebouncer() = default;

} // namespace NovelMind::editor::qt
