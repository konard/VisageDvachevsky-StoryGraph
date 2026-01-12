#include "NovelMind/core/debug_overlay.hpp"
#include <algorithm>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace NovelMind::Core {

DebugOverlay& DebugOverlay::instance() {
  static DebugOverlay instance;
  return instance;
}

void DebugOverlay::update(f32 deltaTime) {
  if (!m_enabled) {
    return;
  }

  m_updateTimer += deltaTime;

  if (deltaTime > 0.0f) {
    const f32 fps = 1.0f / deltaTime;
    m_fpsHistory.push_back(fps);

    while (m_fpsHistory.size() > m_config.fpsHistorySize) {
      m_fpsHistory.pop_front();
    }
  }
}

void DebugOverlay::render() {
  if (!m_enabled || !m_renderCallback) {
    return;
  }

  const std::string output = getFormattedOutput();
  m_renderCallback(output, 10, 10);
}

void DebugOverlay::setMetric(const std::string& name, const std::string& value,
                             const std::string& category) {
  DebugMetric metric;
  metric.name = name;
  metric.value = value;
  metric.category = category;
  m_customMetrics[name] = metric;
}

void DebugOverlay::setMetric(const std::string& name, i64 value, const std::string& category) {
  setMetric(name, std::to_string(value), category);
}

void DebugOverlay::setMetric(const std::string& name, f64 value, const std::string& category,
                             int precision) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(precision) << value;
  setMetric(name, oss.str(), category);
}

void DebugOverlay::removeMetric(const std::string& name) {
  m_customMetrics.erase(name);
}

f32 DebugOverlay::currentFps() const {
  if (m_fpsHistory.empty()) {
    return 0.0f;
  }
  return m_fpsHistory.back();
}

f32 DebugOverlay::averageFps() const {
  if (m_fpsHistory.empty()) {
    return 0.0f;
  }

  const f32 sum = std::accumulate(m_fpsHistory.begin(), m_fpsHistory.end(), 0.0f);
  return sum / static_cast<f32>(m_fpsHistory.size());
}

f32 DebugOverlay::minFps() const {
  if (m_fpsHistory.empty()) {
    return 0.0f;
  }

  return *std::min_element(m_fpsHistory.begin(), m_fpsHistory.end());
}

f32 DebugOverlay::maxFps() const {
  if (m_fpsHistory.empty()) {
    return 0.0f;
  }

  return *std::max_element(m_fpsHistory.begin(), m_fpsHistory.end());
}

std::vector<DebugMetric> DebugOverlay::getAllMetrics() const {
  std::vector<DebugMetric> result;
  result.reserve(m_customMetrics.size() + 10);

  if (m_config.showFps) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << currentFps() << " (avg: " << averageFps()
        << ", min: " << minFps() << ", max: " << maxFps() << ")";
    result.push_back({"FPS", oss.str(), "Performance"});
  }

  if (m_config.showFrameTime) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << m_frameTimeMs << " ms";
    result.push_back({"Frame Time", oss.str(), "Performance"});
  }

  if (m_config.showDrawCalls) {
    result.push_back({"Draw Calls", std::to_string(m_drawCalls), "Rendering"});
  }

  if (m_config.showSceneObjects) {
    result.push_back({"Scene Objects", std::to_string(m_sceneObjects), "Scene"});
  }

  if (m_config.showVfsStats) {
    result.push_back({"VFS Cache", formatBytes(m_vfsCacheSize), "Resources"});
    result.push_back({"VFS Entries", std::to_string(m_vfsCacheEntries), "Resources"});
  }

  if (m_config.showMemoryUsage) {
    result.push_back({"Memory", formatBytes(m_memoryUsage), "System"});
  }

  for (const auto& [name, metric] : m_customMetrics) {
    result.push_back(metric);
  }

  return result;
}

std::string DebugOverlay::getFormattedOutput() const {
  std::ostringstream oss;

  const auto metrics = getAllMetrics();

  std::string currentCategory;
  for (const auto& metric : metrics) {
    if (metric.category != currentCategory && !metric.category.empty()) {
      if (!currentCategory.empty()) {
        oss << "\n";
      }
      oss << "=== " << metric.category << " ===\n";
      currentCategory = metric.category;
    }

    oss << metric.name << ": " << metric.value << "\n";
  }

  return oss.str();
}

void DebugOverlay::beginFrame() {
  if (!m_enabled) {
    return;
  }

  m_frameStart = std::chrono::steady_clock::now();
  m_drawCalls = 0;
}

void DebugOverlay::endFrame() {
  if (!m_enabled) {
    return;
  }

  const auto frameEnd = std::chrono::steady_clock::now();
  m_frameTimeMs = std::chrono::duration<f32, std::milli>(frameEnd - m_frameStart).count();
}

std::string DebugOverlay::formatBytes(usize bytes) const {
  constexpr usize KB = 1024;
  constexpr usize MB = KB * 1024;
  constexpr usize GB = MB * 1024;

  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);

  if (bytes >= GB) {
    oss << static_cast<f64>(bytes) / static_cast<f64>(GB) << " GB";
  } else if (bytes >= MB) {
    oss << static_cast<f64>(bytes) / static_cast<f64>(MB) << " MB";
  } else if (bytes >= KB) {
    oss << static_cast<f64>(bytes) / static_cast<f64>(KB) << " KB";
  } else {
    oss << bytes << " B";
  }

  return oss.str();
}

} // namespace NovelMind::Core
