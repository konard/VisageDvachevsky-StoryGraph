#include "NovelMind/core/timer.hpp"

namespace NovelMind::core {

Timer::Timer() : m_startTime(Clock::now()), m_lastTickTime(m_startTime), m_deltaTime(0.0) {}

void Timer::reset() {
  m_startTime = Clock::now();
  m_lastTickTime = m_startTime;
  m_deltaTime = 0.0;
}

void Timer::tick() {
  TimePoint currentTime = Clock::now();
  Duration elapsed = currentTime - m_lastTickTime;
  m_deltaTime = elapsed.count();
  m_lastTickTime = currentTime;
}

f64 Timer::getElapsedSeconds() const {
  Duration elapsed = Clock::now() - m_startTime;
  return elapsed.count();
}

f64 Timer::getElapsedMilliseconds() const {
  return getElapsedSeconds() * 1000.0;
}

u64 Timer::getElapsedMicroseconds() const {
  auto elapsed = Clock::now() - m_startTime;
  return static_cast<u64>(std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
}

f64 Timer::getDeltaTime() const {
  return m_deltaTime;
}

} // namespace NovelMind::core
