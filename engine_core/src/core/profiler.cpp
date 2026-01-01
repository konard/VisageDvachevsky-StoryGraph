#include "NovelMind/core/profiler.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace NovelMind::Core {

Profiler &Profiler::instance() {
  static Profiler instance;
  return instance;
}

void Profiler::beginFrame() {
  if (!m_enabled.load(std::memory_order_relaxed)) {
    return;
  }

  m_frameStart = std::chrono::steady_clock::now();

  std::lock_guard<std::mutex> lock(m_mutex);
  for (auto &[threadId, data] : m_threadData) {
    data.frameSamples.clear();
  }
}

void Profiler::endFrame() {
  if (!m_enabled.load(std::memory_order_relaxed)) {
    return;
  }

  const auto frameEnd = std::chrono::steady_clock::now();
  m_lastFrameTime.store(
      std::chrono::duration<f64, std::milli>(frameEnd - m_frameStart).count(),
      std::memory_order_relaxed);
  ++m_frameCount;
}

void Profiler::beginSample(const std::string &name,
                           const std::string &category) {
  if (!m_enabled.load(std::memory_order_relaxed)) {
    return;
  }

  // Thread safety: Hold lock for entire operation
  std::lock_guard<std::mutex> lock(m_mutex);
  auto &threadData = m_threadData[std::this_thread::get_id()];

  ProfileSample sample;
  sample.name = name;
  sample.category = category;
  sample.startTime = std::chrono::steady_clock::now();
  sample.threadId = std::this_thread::get_id();
  sample.depth = threadData.currentDepth;

  threadData.activeSamples.push_back(sample);
  ++threadData.currentDepth;
}

void Profiler::endSample(const std::string &name) {
  if (!m_enabled.load(std::memory_order_relaxed)) {
    return;
  }

  // Thread safety: Hold lock for entire operation
  std::lock_guard<std::mutex> lock(m_mutex);
  auto &threadData = m_threadData[std::this_thread::get_id()];

  if (threadData.activeSamples.empty()) {
    return;
  }

  for (auto it = threadData.activeSamples.rbegin();
       it != threadData.activeSamples.rend(); ++it) {
    if (it->name == name) {
      it->endTime = std::chrono::steady_clock::now();

      auto &stats = m_stats[name];
      stats.name = name;
      ++stats.callCount;

      const f64 durationMs = it->durationMs();
      stats.totalMs += durationMs;
      stats.minMs = std::min(stats.minMs, durationMs);
      stats.maxMs = std::max(stats.maxMs, durationMs);
      stats.avgMs = stats.totalMs / static_cast<f64>(stats.callCount);

      threadData.frameSamples.push_back(*it);
      threadData.activeSamples.erase(std::next(it).base());

      if (threadData.currentDepth > 0) {
        --threadData.currentDepth;
      }

      return;
    }
  }
}

std::vector<ProfileSample> Profiler::getFrameSamples() const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::vector<ProfileSample> result;
  for (const auto &[threadId, data] : m_threadData) {
    result.insert(result.end(), data.frameSamples.begin(),
                  data.frameSamples.end());
  }

  std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
    return a.startTime < b.startTime;
  });

  return result;
}

std::unordered_map<std::string, ProfileStats> Profiler::getStats() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_stats;
}

void Profiler::reset() {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_threadData.clear();
  m_stats.clear();
  m_frameCount = 0;
  m_lastFrameTime.store(0.0, std::memory_order_relaxed);
}

bool Profiler::exportToJson(const std::string &filename) const {
  std::ofstream file(filename);
  if (!file.is_open()) {
    return false;
  }

  std::lock_guard<std::mutex> lock(m_mutex);

  file << "{\n";
  file << "  \"frameCount\": " << m_frameCount << ",\n";
  file << "  \"lastFrameTimeMs\": " << std::fixed << std::setprecision(3)
       << m_lastFrameTime.load(std::memory_order_relaxed) << ",\n";
  file << "  \"stats\": [\n";

  bool first = true;
  for (const auto &[name, stats] : m_stats) {
    if (!first) {
      file << ",\n";
    }
    first = false;

    file << "    {\n";
    file << "      \"name\": \"" << name << "\",\n";
    file << "      \"callCount\": " << stats.callCount << ",\n";
    file << "      \"totalMs\": " << std::fixed << std::setprecision(3)
         << stats.totalMs << ",\n";
    file << "      \"minMs\": " << std::fixed << std::setprecision(3)
         << stats.minMs << ",\n";
    file << "      \"maxMs\": " << std::fixed << std::setprecision(3)
         << stats.maxMs << ",\n";
    file << "      \"avgMs\": " << std::fixed << std::setprecision(3)
         << stats.avgMs << "\n";
    file << "    }";
  }

  file << "\n  ]\n";
  file << "}\n";

  return true;
}

bool Profiler::exportToChromeTrace(const std::string &filename) const {
  std::ofstream file(filename);
  if (!file.is_open()) {
    return false;
  }

  std::lock_guard<std::mutex> lock(m_mutex);

  file << "{\"traceEvents\":[\n";

  bool first = true;
  for (const auto &[threadId, data] : m_threadData) {
    for (const auto &sample : data.frameSamples) {
      if (!first) {
        file << ",\n";
      }
      first = false;

      const auto startUs =
          std::chrono::duration_cast<std::chrono::microseconds>(
              sample.startTime.time_since_epoch())
              .count();

      std::ostringstream tidStr;
      tidStr << threadId;

      file << "{";
      file << "\"name\":\"" << sample.name << "\",";
      file << "\"cat\":\""
           << (sample.category.empty() ? "default" : sample.category) << "\",";
      file << "\"ph\":\"X\",";
      file << "\"ts\":" << startUs << ",";
      file << "\"dur\":" << sample.durationUs() << ",";
      file << "\"pid\":1,";
      file << "\"tid\":\"" << tidStr.str() << "\"";
      file << "}";
    }
  }

  file << "\n]}\n";

  return true;
}

Profiler::ThreadData &Profiler::getThreadData() {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_threadData[std::this_thread::get_id()];
}

} // namespace NovelMind::Core
