#pragma once

#include "NovelMind/core/types.hpp"
#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace NovelMind::Core {

struct ProfileSample {
  std::string name;
  std::string category;
  std::chrono::steady_clock::time_point startTime;
  std::chrono::steady_clock::time_point endTime;
  std::thread::id threadId;
  u32 depth = 0;

  [[nodiscard]] f64 durationMs() const {
    return std::chrono::duration<f64, std::milli>(endTime - startTime).count();
  }

  [[nodiscard]] i64 durationUs() const {
    return std::chrono::duration_cast<std::chrono::microseconds>(endTime -
                                                                 startTime)
        .count();
  }
};

struct ProfileStats {
  std::string name;
  usize callCount = 0;
  f64 totalMs = 0.0;
  f64 minMs = std::numeric_limits<f64>::max();
  f64 maxMs = 0.0;
  f64 avgMs = 0.0;
};

class Profiler {
public:
  static Profiler &instance();

  void setEnabled(bool enabled) { m_enabled = enabled; }
  [[nodiscard]] bool isEnabled() const { return m_enabled; }

  void beginFrame();
  void endFrame();

  void beginSample(const std::string &name, const std::string &category = "");
  void endSample(const std::string &name);

  [[nodiscard]] f64 frameTimeMs() const { return m_lastFrameTime; }
  [[nodiscard]] f64 fps() const {
    return m_lastFrameTime > 0.0 ? 1000.0 / m_lastFrameTime : 0.0;
  }
  [[nodiscard]] usize frameCount() const { return m_frameCount; }

  [[nodiscard]] std::vector<ProfileSample> getFrameSamples() const;
  [[nodiscard]] std::unordered_map<std::string, ProfileStats> getStats() const;

  void reset();

  bool exportToJson(const std::string &filename) const;
  bool exportToChromeTrace(const std::string &filename) const;

private:
  Profiler() = default;
  ~Profiler() = default;

  Profiler(const Profiler &) = delete;
  Profiler &operator=(const Profiler &) = delete;

  struct ThreadData {
    std::vector<ProfileSample> activeSamples;
    std::vector<ProfileSample> frameSamples;
    u32 currentDepth = 0;
  };

  ThreadData &getThreadData();

  mutable std::mutex m_mutex;
  std::unordered_map<std::thread::id, ThreadData> m_threadData;
  std::unordered_map<std::string, ProfileStats> m_stats;

  std::chrono::steady_clock::time_point m_frameStart;
  std::atomic<f64> m_lastFrameTime{0.0};
  usize m_frameCount = 0;
  std::atomic<bool> m_enabled{false};
};

class ScopedProfileSample {
public:
  ScopedProfileSample(const std::string &name, const std::string &category = "")
      : m_name(name) {
    Profiler::instance().beginSample(name, category);
  }

  ~ScopedProfileSample() { Profiler::instance().endSample(m_name); }

  ScopedProfileSample(const ScopedProfileSample &) = delete;
  ScopedProfileSample &operator=(const ScopedProfileSample &) = delete;

private:
  std::string m_name;
};

#ifdef NOVELMIND_PROFILING_ENABLED
#define NOVELMIND_PROFILE_SCOPE(name)                                          \
  NovelMind::Core::ScopedProfileSample _profile_##__LINE__(name)
#define NOVELMIND_PROFILE_SCOPE_CAT(name, category)                            \
  NovelMind::Core::ScopedProfileSample _profile_##__LINE__(name, category)
#define NOVELMIND_PROFILE_FUNCTION()                                           \
  NovelMind::Core::ScopedProfileSample _profile_func(__func__)
#define NOVELMIND_PROFILE_BEGIN(name)                                          \
  NovelMind::Core::Profiler::instance().beginSample(name)
#define NOVELMIND_PROFILE_END(name)                                            \
  NovelMind::Core::Profiler::instance().endSample(name)
#else
#define NOVELMIND_PROFILE_SCOPE(name)
#define NOVELMIND_PROFILE_SCOPE_CAT(name, category)
#define NOVELMIND_PROFILE_FUNCTION()
#define NOVELMIND_PROFILE_BEGIN(name)
#define NOVELMIND_PROFILE_END(name)
#endif

} // namespace NovelMind::Core
