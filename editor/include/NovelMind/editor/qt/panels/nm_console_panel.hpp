#pragma once

/**
 * @file nm_console_panel.hpp
 * @brief Console panel for log output and command input
 *
 * Provides:
 * - Log message display with filtering
 * - Color-coded log levels
 * - Clear and filter controls
 * - Auto-scroll option
 */

#include "NovelMind/editor/qt/nm_dock_panel.hpp"
#include <QDateTime>
#include <QPlainTextEdit>
#include <QToolBar>

namespace NovelMind::editor::qt {

/**
 * @brief Log level enumeration
 */
enum class LogLevel { Debug, Info, Warning, Error };

/**
 * @brief Log entry structure
 */
struct LogEntry {
  QDateTime timestamp;
  LogLevel level;
  QString source;
  QString message;
};

/**
 * @brief Custom text edit for console output
 */
class NMConsoleOutput : public QPlainTextEdit {
  Q_OBJECT

public:
  explicit NMConsoleOutput(QWidget* parent = nullptr);

  void appendLog(const LogEntry& entry);
  void clear();

  void setShowDebug(bool show);
  void setShowInfo(bool show);
  void setShowWarning(bool show);
  void setShowError(bool show);

  void setAutoScroll(bool autoScroll);
  [[nodiscard]] bool isAutoScroll() const { return m_autoScroll; }

private:
  void refreshDisplay();

  static constexpr int MAX_ENTRIES = 10000;

  QList<LogEntry> m_entries;
  bool m_showDebug = true;
  bool m_showInfo = true;
  bool m_showWarning = true;
  bool m_showError = true;
  bool m_autoScroll = true;
};

/**
 * @brief Console panel for log output
 */
class NMConsolePanel : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMConsolePanel(QWidget* parent = nullptr);
  ~NMConsolePanel() override;

  void onInitialize() override;
  void onUpdate(double deltaTime) override;

  /**
   * @brief Add a log message
   */
  void log(LogLevel level, const QString& message, const QString& source = QString());

  /**
   * @brief Convenience methods for different log levels
   */
  void logDebug(const QString& message, const QString& source = QString());
  void logInfo(const QString& message, const QString& source = QString());
  void logWarning(const QString& message, const QString& source = QString());
  void logError(const QString& message, const QString& source = QString());

  /**
   * @brief Clear all log messages
   */
  void clear();

  /**
   * @brief Copy selected text to clipboard
   */
  void copySelection();

signals:
  void logAdded(const LogEntry& entry);

private slots:
  void onClear();
  void onCopy();
  void onToggleDebug(bool checked);
  void onToggleInfo(bool checked);
  void onToggleWarning(bool checked);
  void onToggleError(bool checked);
  void onToggleAutoScroll(bool checked);

private:
  void setupToolBar();
  void setupContent();

  NMConsoleOutput* m_output = nullptr;
  QWidget* m_contentWidget = nullptr;
  QToolBar* m_toolBar = nullptr;
};

} // namespace NovelMind::editor::qt
