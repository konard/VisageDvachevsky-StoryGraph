#include "NovelMind/editor/qt/panels/nm_console_panel.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QScrollBar>
#include <QTextCharFormat>
#include <QToolBar>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

// ============================================================================
// NMConsoleOutput
// ============================================================================

NMConsoleOutput::NMConsoleOutput(QWidget* parent) : QPlainTextEdit(parent) {
  setReadOnly(true);
  setMaximumBlockCount(10000); // Limit history
  setFont(NMStyleManager::instance().monospaceFont());
  setLineWrapMode(QPlainTextEdit::NoWrap);
}

void NMConsoleOutput::appendLog(const LogEntry& entry) {
  m_entries.append(entry);

  // Enforce maximum entry limit to prevent unbounded memory growth
  while (m_entries.size() > MAX_ENTRIES) {
    m_entries.removeFirst();
  }

  // Check if this entry should be visible
  bool visible = false;
  switch (entry.level) {
  case LogLevel::Debug:
    visible = m_showDebug;
    break;
  case LogLevel::Info:
    visible = m_showInfo;
    break;
  case LogLevel::Warning:
    visible = m_showWarning;
    break;
  case LogLevel::Error:
    visible = m_showError;
    break;
  }

  if (!visible)
    return;

  // Format the log entry
  const auto& palette = NMStyleManager::instance().palette();

  QString color;
  QString levelStr;
  switch (entry.level) {
  case LogLevel::Debug:
    color = NMStyleManager::colorToStyleString(palette.textSecondary);
    levelStr = "DBG";
    break;
  case LogLevel::Info:
    color = NMStyleManager::colorToStyleString(palette.info);
    levelStr = "INF";
    break;
  case LogLevel::Warning:
    color = NMStyleManager::colorToStyleString(palette.warning);
    levelStr = "WRN";
    break;
  case LogLevel::Error:
    color = NMStyleManager::colorToStyleString(palette.error);
    levelStr = "ERR";
    break;
  }

  QString timestamp = entry.timestamp.toString("hh:mm:ss.zzz");
  QString source = entry.source.isEmpty() ? "" : QString(" [%1]").arg(entry.source);

  QString html = QString("<span style='color: %1'>[%2] %3%4: %5</span><br>")
                     .arg(color)
                     .arg(timestamp)
                     .arg(levelStr)
                     .arg(source)
                     .arg(entry.message.toHtmlEscaped());

  appendHtml(html);

  // Auto-scroll if enabled
  if (m_autoScroll) {
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
  }
}

void NMConsoleOutput::clear() {
  m_entries.clear();
  QPlainTextEdit::clear();
}

void NMConsoleOutput::setShowDebug(bool show) {
  if (m_showDebug == show)
    return;
  m_showDebug = show;
  refreshDisplay();
}

void NMConsoleOutput::setShowInfo(bool show) {
  if (m_showInfo == show)
    return;
  m_showInfo = show;
  refreshDisplay();
}

void NMConsoleOutput::setShowWarning(bool show) {
  if (m_showWarning == show)
    return;
  m_showWarning = show;
  refreshDisplay();
}

void NMConsoleOutput::setShowError(bool show) {
  if (m_showError == show)
    return;
  m_showError = show;
  refreshDisplay();
}

void NMConsoleOutput::setAutoScroll(bool autoScroll) {
  m_autoScroll = autoScroll;
}

void NMConsoleOutput::refreshDisplay() {
  QPlainTextEdit::clear();

  const auto& palette = NMStyleManager::instance().palette();

  for (const auto& entry : m_entries) {
    bool visible = false;
    switch (entry.level) {
    case LogLevel::Debug:
      visible = m_showDebug;
      break;
    case LogLevel::Info:
      visible = m_showInfo;
      break;
    case LogLevel::Warning:
      visible = m_showWarning;
      break;
    case LogLevel::Error:
      visible = m_showError;
      break;
    }

    if (visible) {
      // Format and display entry directly (don't call appendLog to avoid
      // re-adding to m_entries)
      QString color;
      QString levelStr;
      switch (entry.level) {
      case LogLevel::Debug:
        color = NMStyleManager::colorToStyleString(palette.textSecondary);
        levelStr = "DBG";
        break;
      case LogLevel::Info:
        color = NMStyleManager::colorToStyleString(palette.info);
        levelStr = "INF";
        break;
      case LogLevel::Warning:
        color = NMStyleManager::colorToStyleString(palette.warning);
        levelStr = "WRN";
        break;
      case LogLevel::Error:
        color = NMStyleManager::colorToStyleString(palette.error);
        levelStr = "ERR";
        break;
      }

      QString timestamp = entry.timestamp.toString("hh:mm:ss.zzz");
      QString source = entry.source.isEmpty() ? "" : QString(" [%1]").arg(entry.source);

      QString html = QString("<span style='color: %1'>[%2] %3%4: %5</span><br>")
                         .arg(color)
                         .arg(timestamp)
                         .arg(levelStr)
                         .arg(source)
                         .arg(entry.message.toHtmlEscaped());

      appendHtml(html);
    }
  }

  // Restore auto-scroll position if enabled
  if (m_autoScroll && verticalScrollBar()) {
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
  }
}

// ============================================================================
// NMConsolePanel
// ============================================================================

NMConsolePanel::NMConsolePanel(QWidget* parent) : NMDockPanel(tr("Console"), parent) {
  setPanelId("Console");

  // Console needs width for log messages and height to show several log entries
  setMinimumPanelSize(300, 150);

  setupContent();
  setupToolBar();
}

NMConsolePanel::~NMConsolePanel() = default;

void NMConsolePanel::onInitialize() {
  // Add some demo log messages
  logInfo("NovelMind Editor started", "System");
  logInfo("Qt 6 GUI initialized successfully", "GUI");
  logDebug("Loading user preferences...", "Settings");
  logWarning("No recent projects found", "Project");
}

void NMConsolePanel::onUpdate(double /*deltaTime*/) {
  // No continuous update needed
}

void NMConsolePanel::log(LogLevel level, const QString& message, const QString& source) {
  LogEntry entry;
  entry.timestamp = QDateTime::currentDateTime();
  entry.level = level;
  entry.source = source;
  entry.message = message;

  if (m_output) {
    m_output->appendLog(entry);
  }

  emit logAdded(entry);
}

void NMConsolePanel::logDebug(const QString& message, const QString& source) {
  log(LogLevel::Debug, message, source);
}

void NMConsolePanel::logInfo(const QString& message, const QString& source) {
  log(LogLevel::Info, message, source);
}

void NMConsolePanel::logWarning(const QString& message, const QString& source) {
  log(LogLevel::Warning, message, source);
}

void NMConsolePanel::logError(const QString& message, const QString& source) {
  log(LogLevel::Error, message, source);
}

void NMConsolePanel::clear() {
  if (m_output) {
    m_output->clear();
  }
}

void NMConsolePanel::copySelection() {
  if (m_output && m_output->textCursor().hasSelection()) {
    m_output->copy();
  }
}

void NMConsolePanel::setupToolBar() {
  m_toolBar = new QToolBar(this);
  m_toolBar->setObjectName("ConsoleToolBar");
  m_toolBar->setIconSize(QSize(16, 16));

  // Get icon manager
  auto& iconMgr = NMIconManager::instance();

  // Copy action
  QAction* actionCopy = m_toolBar->addAction(iconMgr.getIcon("copy"), tr("Copy"));
  actionCopy->setToolTip(tr("Copy Selected Text (Ctrl+C)"));
  actionCopy->setShortcut(QKeySequence::Copy);
  connect(actionCopy, &QAction::triggered, this, &NMConsolePanel::onCopy);

  // Clear action
  QAction* actionClear = m_toolBar->addAction(iconMgr.getIcon("delete"), tr("Clear"));
  actionClear->setToolTip(tr("Clear Console"));
  connect(actionClear, &QAction::triggered, this, &NMConsolePanel::onClear);

  m_toolBar->addSeparator();

  // Filter toggles
  QAction* actionDebug = m_toolBar->addAction(tr("Debug"));
  actionDebug->setToolTip(tr("Show Debug Messages"));
  actionDebug->setCheckable(true);
  actionDebug->setChecked(true);
  connect(actionDebug, &QAction::toggled, this, &NMConsolePanel::onToggleDebug);

  QAction* actionInfo = m_toolBar->addAction(tr("Info"));
  actionInfo->setToolTip(tr("Show Info Messages"));
  actionInfo->setCheckable(true);
  actionInfo->setChecked(true);
  connect(actionInfo, &QAction::toggled, this, &NMConsolePanel::onToggleInfo);

  QAction* actionWarning = m_toolBar->addAction(tr("Warning"));
  actionWarning->setToolTip(tr("Show Warning Messages"));
  actionWarning->setCheckable(true);
  actionWarning->setChecked(true);
  connect(actionWarning, &QAction::toggled, this, &NMConsolePanel::onToggleWarning);

  QAction* actionError = m_toolBar->addAction(tr("Error"));
  actionError->setToolTip(tr("Show Error Messages"));
  actionError->setCheckable(true);
  actionError->setChecked(true);
  connect(actionError, &QAction::toggled, this, &NMConsolePanel::onToggleError);

  m_toolBar->addSeparator();

  QAction* actionAutoScroll = m_toolBar->addAction(tr("Auto-Scroll"));
  actionAutoScroll->setToolTip(tr("Auto-scroll to latest message"));
  actionAutoScroll->setCheckable(true);
  actionAutoScroll->setChecked(true);
  connect(actionAutoScroll, &QAction::toggled, this, &NMConsolePanel::onToggleAutoScroll);

  if (auto* layout = qobject_cast<QVBoxLayout*>(m_contentWidget->layout())) {
    layout->insertWidget(0, m_toolBar);
  }
}

void NMConsolePanel::setupContent() {
  m_contentWidget = new QWidget(this);
  auto* layout = new QVBoxLayout(m_contentWidget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_output = new NMConsoleOutput(m_contentWidget);
  layout->addWidget(m_output);

  setContentWidget(m_contentWidget);
}

void NMConsolePanel::onClear() {
  clear();
}

void NMConsolePanel::onCopy() {
  copySelection();
}

void NMConsolePanel::onToggleDebug(bool checked) {
  if (m_output)
    m_output->setShowDebug(checked);
}

void NMConsolePanel::onToggleInfo(bool checked) {
  if (m_output)
    m_output->setShowInfo(checked);
}

void NMConsolePanel::onToggleWarning(bool checked) {
  if (m_output)
    m_output->setShowWarning(checked);
}

void NMConsolePanel::onToggleError(bool checked) {
  if (m_output)
    m_output->setShowError(checked);
}

void NMConsolePanel::onToggleAutoScroll(bool checked) {
  if (m_output)
    m_output->setAutoScroll(checked);
}

} // namespace NovelMind::editor::qt
