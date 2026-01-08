#pragma once

#include <cstdio>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace NovelMind::core {

enum class LogLevel { Trace, Debug, Info, Warning, Error, Fatal, Off };

class Logger {
public:
  static Logger &instance();

  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  void setLevel(LogLevel level);
  [[nodiscard]] LogLevel getLevel() const;

  void setOutputFile(const std::string &path);
  void closeOutputFile();

  using LogCallback = std::function<void(LogLevel, const std::string &)>;
  void addLogCallback(LogCallback callback);
  void clearLogCallbacks();

  void log(LogLevel level, std::string_view message);

  void trace(std::string_view message);
  void debug(std::string_view message);
  void info(std::string_view message);
  void warning(std::string_view message);
  void error(std::string_view message);
  void fatal(std::string_view message);

private:
  Logger();
  ~Logger();

  [[nodiscard]] const char *levelToString(LogLevel level) const;
  [[nodiscard]] std::string getCurrentTimestamp() const;

  LogLevel m_level;
  std::ofstream m_fileStream;
  mutable std::mutex m_mutex;
  bool m_useColors;
  std::vector<LogCallback> m_callbacks;
};

} // namespace NovelMind::core

#define NOVELMIND_LOG_TRACE(msg) ::NovelMind::core::Logger::instance().trace(msg)
#define NOVELMIND_LOG_DEBUG(msg) ::NovelMind::core::Logger::instance().debug(msg)
#define NOVELMIND_LOG_INFO(msg) ::NovelMind::core::Logger::instance().info(msg)
#define NOVELMIND_LOG_WARN(msg) ::NovelMind::core::Logger::instance().warning(msg)
#define NOVELMIND_LOG_ERROR(msg) ::NovelMind::core::Logger::instance().error(msg)
#define NOVELMIND_LOG_FATAL(msg) ::NovelMind::core::Logger::instance().fatal(msg)
