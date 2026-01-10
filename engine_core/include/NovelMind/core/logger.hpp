#pragma once

#include <cstdio>
#include <format>
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
  static Logger& instance();

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  void setLevel(LogLevel level);
  [[nodiscard]] LogLevel getLevel() const;

  void setOutputFile(const std::string& path);
  void closeOutputFile();

  using LogCallback = std::function<void(LogLevel, const std::string&)>;
  void addLogCallback(LogCallback callback);
  void clearLogCallbacks();

  void log(LogLevel level, std::string_view message);

  void trace(std::string_view message);
  void debug(std::string_view message);
  void info(std::string_view message);
  void warning(std::string_view message);
  void error(std::string_view message);
  void fatal(std::string_view message);

  // Template overloads for format strings with variadic arguments
  template <typename... Args> void trace(std::format_string<Args...> fmt, Args&&... args) {
    trace(std::format(fmt, std::forward<Args>(args)...));
  }

  template <typename... Args> void debug(std::format_string<Args...> fmt, Args&&... args) {
    debug(std::format(fmt, std::forward<Args>(args)...));
  }

  template <typename... Args> void info(std::format_string<Args...> fmt, Args&&... args) {
    info(std::format(fmt, std::forward<Args>(args)...));
  }

  template <typename... Args> void warning(std::format_string<Args...> fmt, Args&&... args) {
    warning(std::format(fmt, std::forward<Args>(args)...));
  }

  template <typename... Args> void error(std::format_string<Args...> fmt, Args&&... args) {
    error(std::format(fmt, std::forward<Args>(args)...));
  }

  template <typename... Args> void fatal(std::format_string<Args...> fmt, Args&&... args) {
    fatal(std::format(fmt, std::forward<Args>(args)...));
  }

private:
  Logger();
  ~Logger();

  [[nodiscard]] const char* levelToString(LogLevel level) const;
  [[nodiscard]] std::string getCurrentTimestamp() const;

  LogLevel m_level;
  std::ofstream m_fileStream;
  mutable std::mutex m_mutex;
  bool m_useColors;
  std::vector<LogCallback> m_callbacks;
};

} // namespace NovelMind::core

#define NOVELMIND_LOG_TRACE(...) ::NovelMind::core::Logger::instance().trace(__VA_ARGS__)
#define NOVELMIND_LOG_DEBUG(...) ::NovelMind::core::Logger::instance().debug(__VA_ARGS__)
#define NOVELMIND_LOG_INFO(...) ::NovelMind::core::Logger::instance().info(__VA_ARGS__)
#define NOVELMIND_LOG_WARN(...) ::NovelMind::core::Logger::instance().warning(__VA_ARGS__)
#define NOVELMIND_LOG_ERROR(...) ::NovelMind::core::Logger::instance().error(__VA_ARGS__)
#define NOVELMIND_LOG_FATAL(...) ::NovelMind::core::Logger::instance().fatal(__VA_ARGS__)
