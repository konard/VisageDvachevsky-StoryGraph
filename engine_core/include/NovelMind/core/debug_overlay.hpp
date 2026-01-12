#pragma once

#include "NovelMind/core/profiler.hpp"
#include "NovelMind/core/types.hpp"
#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace NovelMind::Core {

struct DebugMetric {
  std::string name;
  std::string value;
  std::string category;
};

struct DebugOverlayConfig {
  bool showFps = true;
  bool showFrameTime = true;
  bool showDrawCalls = true;
  bool showSceneObjects = true;
  bool showVfsStats = true;
  bool showMemoryUsage = true;
  bool showProfiler = false;

  usize fpsHistorySize = 60;
  f32 updateInterval = 0.25f;
};

class DebugOverlay {
public:
  static DebugOverlay& instance();

  void setEnabled(bool enabled) { m_enabled = enabled; }
  [[nodiscard]] bool isEnabled() const { return m_enabled; }

  void setConfig(const DebugOverlayConfig& config) { m_config = config; }
  [[nodiscard]] const DebugOverlayConfig& config() const { return m_config; }

  void update(f32 deltaTime);
  void render();

  void setMetric(const std::string& name, const std::string& value,
                 const std::string& category = "");
  void setMetric(const std::string& name, i64 value, const std::string& category = "");
  void setMetric(const std::string& name, f64 value, const std::string& category = "",
                 int precision = 2);
  void removeMetric(const std::string& name);

  void addDrawCalls(u32 count) { m_drawCalls += count; }
  void setDrawCalls(u32 count) { m_drawCalls = count; }
  void setSceneObjectCount(u32 count) { m_sceneObjects = count; }
  void setVfsCacheSize(usize size) { m_vfsCacheSize = size; }
  void setVfsCacheEntries(usize count) { m_vfsCacheEntries = count; }
  void setMemoryUsage(usize bytes) { m_memoryUsage = bytes; }

  [[nodiscard]] f32 currentFps() const;
  [[nodiscard]] f32 averageFps() const;
  [[nodiscard]] f32 minFps() const;
  [[nodiscard]] f32 maxFps() const;
  [[nodiscard]] f32 frameTimeMs() const { return m_frameTimeMs; }

  [[nodiscard]] std::vector<DebugMetric> getAllMetrics() const;
  [[nodiscard]] std::string getFormattedOutput() const;

  using RenderCallback = std::function<void(const std::string& text, i32 x, i32 y)>;
  void setRenderCallback(RenderCallback callback) { m_renderCallback = std::move(callback); }

  void beginFrame();
  void endFrame();

private:
  DebugOverlay() = default;
  ~DebugOverlay() = default;

  DebugOverlay(const DebugOverlay&) = delete;
  DebugOverlay& operator=(const DebugOverlay&) = delete;

  [[nodiscard]] std::string formatBytes(usize bytes) const;

  bool m_enabled = false;
  DebugOverlayConfig m_config;

  std::deque<f32> m_fpsHistory;
  f32 m_frameTimeMs = 0.0f;
  f32 m_updateTimer = 0.0f;

  u32 m_drawCalls = 0;
  u32 m_sceneObjects = 0;
  usize m_vfsCacheSize = 0;
  usize m_vfsCacheEntries = 0;
  usize m_memoryUsage = 0;

  std::chrono::steady_clock::time_point m_frameStart;

  std::unordered_map<std::string, DebugMetric> m_customMetrics;
  RenderCallback m_renderCallback;
};

#ifdef NOVELMIND_DEBUG
#define NOVELMIND_DEBUG_METRIC(name, value)                                                        \
  NovelMind::Core::DebugOverlay::instance().setMetric(name, value)
#define NOVELMIND_DEBUG_METRIC_CAT(name, value, category)                                          \
  NovelMind::Core::DebugOverlay::instance().setMetric(name, value, category)
#else
#define NOVELMIND_DEBUG_METRIC(name, value)
#define NOVELMIND_DEBUG_METRIC_CAT(name, value, category)
#endif

} // namespace NovelMind::Core
