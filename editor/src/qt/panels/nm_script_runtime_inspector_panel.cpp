#include <NovelMind/editor/qt/nm_dialogs.hpp>
#include <NovelMind/editor/qt/nm_icon_manager.hpp>
#include <NovelMind/editor/qt/nm_play_mode_controller.hpp>
#include <NovelMind/editor/qt/panels/nm_script_runtime_inspector_panel.hpp>
#include <NovelMind/scripting/vm_debugger.hpp>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMetaType>
#include <QMessageBox>
#include <QPushButton>

namespace NovelMind::editor::qt {

NMScriptRuntimeInspectorPanel::NMScriptRuntimeInspectorPanel(QWidget* parent)
    : NMDockPanel("Script Runtime Inspector", parent) {
  setupUI();
}

NMScriptRuntimeInspectorPanel::~NMScriptRuntimeInspectorPanel() = default;

void NMScriptRuntimeInspectorPanel::onInitialize() {
  NMDockPanel::onInitialize();

  auto& controller = NMPlayModeController::instance();

  // Connect to play mode controller signals
  connect(&controller, &NMPlayModeController::variablesChanged, this,
          &NMScriptRuntimeInspectorPanel::onVariablesChanged);
  connect(&controller, &NMPlayModeController::flagsChanged, this,
          &NMScriptRuntimeInspectorPanel::onFlagsChanged);
  connect(&controller, &NMPlayModeController::callStackChanged, this,
          &NMScriptRuntimeInspectorPanel::onCallStackChanged);
  connect(&controller, &NMPlayModeController::currentNodeChanged, this,
          &NMScriptRuntimeInspectorPanel::onCurrentNodeChanged);
  connect(&controller, &NMPlayModeController::executionStepChanged, this,
          &NMScriptRuntimeInspectorPanel::onExecutionStepChanged);
  connect(&controller, &NMPlayModeController::playModeChanged, this,
          &NMScriptRuntimeInspectorPanel::onPlayModeChanged);

  // Initial state
  updateControlsState();
  updateStatusDisplay();
}

void NMScriptRuntimeInspectorPanel::onShutdown() {
  NMDockPanel::onShutdown();
}

void NMScriptRuntimeInspectorPanel::onUpdate(double deltaTime) {
  NMDockPanel::onUpdate(deltaTime);
  updatePerformanceMetrics(deltaTime);
}

void NMScriptRuntimeInspectorPanel::setDebugger(scripting::VMDebugger* debugger) {
  m_debugger = debugger;
  updateBreakpointsDisplay();
  updateHistoryDisplay();
}

void NMScriptRuntimeInspectorPanel::setExecutionState(DebugExecutionState state) {
  m_executionState = state;
  updateControlsState();
  updateStatusDisplay();
}

void NMScriptRuntimeInspectorPanel::navigateToSource(const QString& filePath, int line) {
  emit sourceNavigationRequested(filePath, line);
}

// =========================================================================
// UI Setup
// =========================================================================

void NMScriptRuntimeInspectorPanel::setupUI() {
  auto* contentWidget = new QWidget;
  auto* layout = new QVBoxLayout(contentWidget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Setup toolbar with control buttons
  setupToolBar();
  layout->addWidget(m_toolBar);

  // Status bar
  auto* statusWidget = new QWidget;
  auto* statusLayout = new QHBoxLayout(statusWidget);
  statusLayout->setContentsMargins(8, 4, 8, 4);
  statusLayout->setSpacing(12);

  m_statusLabel = new QLabel("Idle");
  m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: #888888; }");

  m_sceneLabel = new QLabel("Scene: -");
  m_sceneLabel->setStyleSheet("QLabel { color: #0078d4; }");

  m_lineLabel = new QLabel("Line: -");
  m_lineLabel->setStyleSheet("QLabel { color: #4caf50; font-family: monospace; }");

  statusLayout->addWidget(m_statusLabel);
  statusLayout->addWidget(m_sceneLabel);
  statusLayout->addWidget(m_lineLabel);
  statusLayout->addStretch();

  layout->addWidget(statusWidget);

  // Separator
  auto* separator = new QFrame;
  separator->setFrameShape(QFrame::HLine);
  separator->setFrameShadow(QFrame::Sunken);
  layout->addWidget(separator);

  // Tab widget for different views
  m_tabWidget = new QTabWidget;

  setupVariablesTab();
  setupCallStackTab();
  setupBreakpointsTab();
  setupHistoryTab();
  setupPerformanceTab();

  layout->addWidget(m_tabWidget);

  setContentWidget(contentWidget);
  setMinimumPanelSize(350, 300);
}

void NMScriptRuntimeInspectorPanel::setupToolBar() {
  m_toolBar = new QToolBar;
  m_toolBar->setIconSize(QSize(16, 16));
  m_toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

  auto& iconMgr = NMIconManager::instance();

  // Continue button
  m_continueBtn = new QToolButton;
  m_continueBtn->setIcon(iconMgr.getIcon("play", 16));
  m_continueBtn->setToolTip("Continue (F5)");
  m_continueBtn->setShortcut(QKeySequence(Qt::Key_F5));
  connect(m_continueBtn, &QToolButton::clicked, this,
          &NMScriptRuntimeInspectorPanel::onContinueClicked);
  m_toolBar->addWidget(m_continueBtn);

  // Pause button
  m_pauseBtn = new QToolButton;
  m_pauseBtn->setIcon(iconMgr.getIcon("pause", 16));
  m_pauseBtn->setToolTip("Pause (F6)");
  m_pauseBtn->setShortcut(QKeySequence(Qt::Key_F6));
  connect(m_pauseBtn, &QToolButton::clicked, this, &NMScriptRuntimeInspectorPanel::onPauseClicked);
  m_toolBar->addWidget(m_pauseBtn);

  m_toolBar->addSeparator();

  // Step Into button
  m_stepIntoBtn = new QToolButton;
  m_stepIntoBtn->setIcon(iconMgr.getIcon("step-into", 16));
  m_stepIntoBtn->setToolTip("Step Into (F11)");
  m_stepIntoBtn->setShortcut(QKeySequence(Qt::Key_F11));
  connect(m_stepIntoBtn, &QToolButton::clicked, this,
          &NMScriptRuntimeInspectorPanel::onStepIntoClicked);
  m_toolBar->addWidget(m_stepIntoBtn);

  // Step Over button
  m_stepOverBtn = new QToolButton;
  m_stepOverBtn->setIcon(iconMgr.getIcon("step-over", 16));
  m_stepOverBtn->setToolTip("Step Over (F10)");
  m_stepOverBtn->setShortcut(QKeySequence(Qt::Key_F10));
  connect(m_stepOverBtn, &QToolButton::clicked, this,
          &NMScriptRuntimeInspectorPanel::onStepOverClicked);
  m_toolBar->addWidget(m_stepOverBtn);

  // Step Out button
  m_stepOutBtn = new QToolButton;
  m_stepOutBtn->setIcon(iconMgr.getIcon("step-out", 16));
  m_stepOutBtn->setToolTip("Step Out (Shift+F11)");
  m_stepOutBtn->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F11));
  connect(m_stepOutBtn, &QToolButton::clicked, this,
          &NMScriptRuntimeInspectorPanel::onStepOutClicked);
  m_toolBar->addWidget(m_stepOutBtn);

  m_toolBar->addSeparator();

  // Stop button
  m_stopBtn = new QToolButton;
  m_stopBtn->setIcon(iconMgr.getIcon("stop", 16));
  m_stopBtn->setToolTip("Stop (Shift+F5)");
  m_stopBtn->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F5));
  connect(m_stopBtn, &QToolButton::clicked, this, &NMScriptRuntimeInspectorPanel::onStopClicked);
  m_toolBar->addWidget(m_stopBtn);
}

void NMScriptRuntimeInspectorPanel::setupVariablesTab() {
  auto& iconMgr = NMIconManager::instance();

  auto* varWidget = new QWidget;
  auto* varLayout = new QVBoxLayout(varWidget);
  varLayout->setContentsMargins(4, 4, 4, 4);

  // Toolbar for variables
  auto* varToolBar = new QHBoxLayout;
  varToolBar->setSpacing(4);

  m_addWatchBtn = new QToolButton;
  m_addWatchBtn->setIcon(iconMgr.getIcon("add", 16));
  m_addWatchBtn->setToolTip("Add Watch Expression");
  varToolBar->addWidget(m_addWatchBtn);

  m_refreshVarsBtn = new QToolButton;
  m_refreshVarsBtn->setIcon(iconMgr.getIcon("refresh", 16));
  m_refreshVarsBtn->setToolTip("Refresh Variables");
  connect(m_refreshVarsBtn, &QToolButton::clicked, this,
          &NMScriptRuntimeInspectorPanel::updateVariablesDisplay);
  varToolBar->addWidget(m_refreshVarsBtn);

  varToolBar->addStretch();
  varLayout->addLayout(varToolBar);

  // Variables tree
  m_variablesTree = new QTreeWidget;
  m_variablesTree->setHeaderLabels({"Name", "Value", "Type", "Changed"});
  m_variablesTree->setAlternatingRowColors(true);
  m_variablesTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_variablesTree->header()->setStretchLastSection(false);
  m_variablesTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_variablesTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_variablesTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  m_variablesTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

  connect(m_variablesTree, &QTreeWidget::itemDoubleClicked, this,
          &NMScriptRuntimeInspectorPanel::onVariableItemDoubleClicked);

  varLayout->addWidget(m_variablesTree);

  // Help text
  auto* helpLabel = new QLabel("Double-click a variable to edit its value (only when paused)");
  helpLabel->setStyleSheet("QLabel { color: #a0a0a0; font-size: 9pt; padding: 4px; }");
  varLayout->addWidget(helpLabel);

  m_tabWidget->addTab(varWidget, iconMgr.getIcon("info", 16), "Variables & Flags");
}

void NMScriptRuntimeInspectorPanel::setupCallStackTab() {
  auto* stackWidget = new QWidget;
  auto* stackLayout = new QVBoxLayout(stackWidget);
  stackLayout->setContentsMargins(4, 4, 4, 4);

  m_callStackList = new QListWidget;
  m_callStackList->setAlternatingRowColors(true);
  connect(m_callStackList, &QListWidget::itemDoubleClicked, this,
          &NMScriptRuntimeInspectorPanel::onCallStackItemDoubleClicked);

  stackLayout->addWidget(m_callStackList);

  // Help text
  auto* helpLabel = new QLabel("Double-click to navigate to source location");
  helpLabel->setStyleSheet("QLabel { color: #a0a0a0; font-size: 9pt; padding: 4px; }");
  stackLayout->addWidget(helpLabel);

  m_tabWidget->addTab(stackWidget, "Call Stack");
}

void NMScriptRuntimeInspectorPanel::setupBreakpointsTab() {
  auto& iconMgr = NMIconManager::instance();

  auto* bpWidget = new QWidget;
  auto* bpLayout = new QVBoxLayout(bpWidget);
  bpLayout->setContentsMargins(4, 4, 4, 4);

  // Toolbar for breakpoints
  auto* bpToolBar = new QHBoxLayout;
  bpToolBar->setSpacing(4);

  m_addBpBtn = new QToolButton;
  m_addBpBtn->setIcon(iconMgr.getIcon("add", 16));
  m_addBpBtn->setToolTip("Add Breakpoint");
  connect(m_addBpBtn, &QToolButton::clicked, this,
          &NMScriptRuntimeInspectorPanel::onAddBreakpointClicked);
  bpToolBar->addWidget(m_addBpBtn);

  m_removeBpBtn = new QToolButton;
  m_removeBpBtn->setIcon(iconMgr.getIcon("delete", 16));
  m_removeBpBtn->setToolTip("Remove Selected Breakpoint");
  connect(m_removeBpBtn, &QToolButton::clicked, this,
          &NMScriptRuntimeInspectorPanel::onRemoveBreakpointClicked);
  bpToolBar->addWidget(m_removeBpBtn);

  m_clearBpsBtn = new QToolButton;
  m_clearBpsBtn->setIcon(iconMgr.getIcon("clear", 16));
  m_clearBpsBtn->setToolTip("Clear All Breakpoints");
  connect(m_clearBpsBtn, &QToolButton::clicked, this,
          &NMScriptRuntimeInspectorPanel::onClearBreakpointsClicked);
  bpToolBar->addWidget(m_clearBpsBtn);

  bpToolBar->addStretch();
  bpLayout->addLayout(bpToolBar);

  // Breakpoints tree
  m_breakpointsTree = new QTreeWidget;
  m_breakpointsTree->setHeaderLabels({"Enabled", "Location", "Condition", "Hits"});
  m_breakpointsTree->setAlternatingRowColors(true);
  m_breakpointsTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  m_breakpointsTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_breakpointsTree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
  m_breakpointsTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

  connect(m_breakpointsTree, &QTreeWidget::itemDoubleClicked, this,
          &NMScriptRuntimeInspectorPanel::onBreakpointItemDoubleClicked);

  bpLayout->addWidget(m_breakpointsTree);

  m_tabWidget->addTab(bpWidget, "Breakpoints");
}

void NMScriptRuntimeInspectorPanel::setupHistoryTab() {
  auto& iconMgr = NMIconManager::instance();

  auto* histWidget = new QWidget;
  auto* histLayout = new QVBoxLayout(histWidget);
  histLayout->setContentsMargins(4, 4, 4, 4);

  // Toolbar
  auto* histToolBar = new QHBoxLayout;
  histToolBar->setSpacing(4);

  m_clearHistoryBtn = new QToolButton;
  m_clearHistoryBtn->setIcon(iconMgr.getIcon("clear", 16));
  m_clearHistoryBtn->setToolTip("Clear History");
  histToolBar->addWidget(m_clearHistoryBtn);

  histToolBar->addStretch();
  histLayout->addLayout(histToolBar);

  // History tree showing variable changes
  m_historyTree = new QTreeWidget;
  m_historyTree->setHeaderLabels({"Variable", "Old Value", "New Value", "Line"});
  m_historyTree->setAlternatingRowColors(true);
  m_historyTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_historyTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_historyTree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
  m_historyTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

  connect(m_historyTree, &QTreeWidget::itemClicked, this,
          &NMScriptRuntimeInspectorPanel::onHistoryItemClicked);

  histLayout->addWidget(m_historyTree);

  m_tabWidget->addTab(histWidget, "History");
}

void NMScriptRuntimeInspectorPanel::setupPerformanceTab() {
  auto* perfWidget = new QWidget;
  auto* perfLayout = new QVBoxLayout(perfWidget);
  perfLayout->setContentsMargins(4, 4, 4, 4);

  m_performanceTree = new QTreeWidget;
  m_performanceTree->setHeaderLabels({"Metric", "Value"});
  m_performanceTree->setAlternatingRowColors(true);
  m_performanceTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_performanceTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);

  // Add performance metric items
  m_frameTimeItem = new QTreeWidgetItem(m_performanceTree, {"Frame Time", "-"});
  m_instructionRateItem = new QTreeWidgetItem(m_performanceTree, {"Instructions/sec", "-"});
  m_memoryItem = new QTreeWidgetItem(m_performanceTree, {"Memory Usage", "-"});
  m_sceneTimeItem = new QTreeWidgetItem(m_performanceTree, {"Scene Execution Time", "-"});

  perfLayout->addWidget(m_performanceTree);

  m_tabWidget->addTab(perfWidget, "Performance");
}

// =========================================================================
// Control Button Handlers
// =========================================================================

void NMScriptRuntimeInspectorPanel::onContinueClicked() {
  if (m_debugger) {
    m_debugger->continueExecution();
  }
  emit continueRequested();
}

void NMScriptRuntimeInspectorPanel::onPauseClicked() {
  if (m_debugger) {
    m_debugger->pause();
  }
  emit pauseRequested();
}

void NMScriptRuntimeInspectorPanel::onStepIntoClicked() {
  if (m_debugger) {
    m_debugger->stepInto();
  }
  emit stepIntoRequested();
}

void NMScriptRuntimeInspectorPanel::onStepOverClicked() {
  if (m_debugger) {
    m_debugger->stepOver();
  }
  emit stepOverRequested();
}

void NMScriptRuntimeInspectorPanel::onStepOutClicked() {
  if (m_debugger) {
    m_debugger->stepOut();
  }
  emit stepOutRequested();
}

void NMScriptRuntimeInspectorPanel::onStopClicked() {
  if (m_debugger) {
    m_debugger->stop();
  }
  emit stopRequested();
}

// =========================================================================
// Data Update Handlers
// =========================================================================

void NMScriptRuntimeInspectorPanel::onVariablesChanged(const QVariantMap& variables) {
  m_currentVariables = variables;
  updateVariablesDisplay();
}

void NMScriptRuntimeInspectorPanel::onFlagsChanged(const QVariantMap& flags) {
  m_currentFlags = flags;
  updateVariablesDisplay();
}

void NMScriptRuntimeInspectorPanel::onCallStackChanged(const QStringList& stack) {
  m_currentCallStack = stack;
  updateCallStackDisplay();
}

void NMScriptRuntimeInspectorPanel::onCurrentNodeChanged(const QString& nodeId) {
  m_currentScene = nodeId;
  m_sceneLabel->setText("Scene: " + (nodeId.isEmpty() ? "-" : nodeId));
}

void NMScriptRuntimeInspectorPanel::onExecutionStepChanged(int stepIndex, int totalSteps,
                                                           const QString& instruction) {
  m_currentStep = stepIndex;
  m_totalSteps = totalSteps;
  m_currentInstruction = instruction;

  if (stepIndex >= 0 && totalSteps > 0) {
    m_lineLabel->setText(QString("Step: %1/%2").arg(stepIndex + 1).arg(totalSteps));
  } else {
    m_lineLabel->setText("Line: -");
  }

  ++m_instructionCount;
}

void NMScriptRuntimeInspectorPanel::onPlayModeChanged(int mode) {
  // Map play mode to execution state
  switch (mode) {
  case 0: // Idle
    setExecutionState(DebugExecutionState::Idle);
    break;
  case 1: // Playing
    setExecutionState(DebugExecutionState::Running);
    break;
  case 2: // Paused
    setExecutionState(DebugExecutionState::PausedUser);
    break;
  default:
    break;
  }
}

// =========================================================================
// UI Interaction Handlers
// =========================================================================

void NMScriptRuntimeInspectorPanel::onVariableItemDoubleClicked(QTreeWidgetItem* item, int column) {
  if (!item || column != 1) {
    return;
  }

  // Only allow editing when paused
  if (m_executionState != DebugExecutionState::PausedBreakpoint &&
      m_executionState != DebugExecutionState::PausedStep &&
      m_executionState != DebugExecutionState::PausedUser) {
    return;
  }

  QString name = item->text(0);
  QVariant currentValue = item->data(1, Qt::UserRole);
  editVariable(name, currentValue);
}

void NMScriptRuntimeInspectorPanel::onBreakpointItemDoubleClicked(QTreeWidgetItem* item,
                                                                  int column) {
  if (!item) {
    return;
  }

  if (column == 0) {
    // Toggle enabled state
    bool enabled = item->checkState(0) == Qt::Checked;
    item->setCheckState(0, enabled ? Qt::Unchecked : Qt::Checked);

    quint32 ip = item->data(0, Qt::UserRole).toUInt();
    if (m_debugger) {
      // Find breakpoint by IP and toggle
      auto breakpoints = m_debugger->getAllBreakpoints();
      for (const auto& bp : breakpoints) {
        if (bp.instructionPointer == ip) {
          m_debugger->setBreakpointEnabled(bp.id, !enabled);
          break;
        }
      }
    }
    emit breakpointToggled(ip, !enabled);
  } else {
    // Navigate to breakpoint location
    QString location = item->text(1);
    // Parse "file:line" format
    qsizetype colonPos = location.lastIndexOf(':');
    if (colonPos > 0) {
      QString file = location.left(colonPos);
      int line = location.mid(colonPos + 1).toInt();
      emit sourceNavigationRequested(file, line);
    }
  }
}

void NMScriptRuntimeInspectorPanel::onCallStackItemDoubleClicked(QListWidgetItem* item) {
  if (!item) {
    return;
  }

  // Parse call stack item to extract location
  // Format: "sceneName (file:line)"
  QString text = item->text();
  qsizetype parenPos = text.indexOf('(');
  if (parenPos > 0) {
    QString location = text.mid(parenPos + 1, text.length() - parenPos - 2);
    qsizetype colonPos = location.lastIndexOf(':');
    if (colonPos > 0) {
      QString file = location.left(colonPos);
      int line = location.mid(colonPos + 1).toInt();
      emit sourceNavigationRequested(file, line);
    }
  }
}

void NMScriptRuntimeInspectorPanel::onHistoryItemClicked(QTreeWidgetItem* item, int /*column*/) {
  if (!item) {
    return;
  }

  // Navigate to the line where the change occurred
  int line = item->text(3).toInt();
  if (line > 0) {
    // We would need the file path, which isn't stored in the simple history
    // For now, just highlight the item
    item->setSelected(true);
  }
}

void NMScriptRuntimeInspectorPanel::onAddBreakpointClicked() {
  // Show dialog to add breakpoint at specific location
  bool ok = false;
  QString input = NMInputDialog::getText(
      this, "Add Breakpoint", "Enter file:line or instruction address:", QLineEdit::Normal,
      QString(), &ok);
  if (!ok || input.isEmpty()) {
    return;
  }

  // Try to parse as file:line
  qsizetype colonPos = input.lastIndexOf(':');
  if (colonPos > 0) {
    QString file = input.left(colonPos);
    int line = input.mid(colonPos + 1).toInt();

    // Map file:line to IP through source mappings
    if (m_debugger) {
      // Search through source mappings to find matching IP
      std::optional<quint32> foundIP;
      auto mappings = m_debugger->getAllSourceMappings();

      for (const auto& [ip, loc] : mappings) {
        if (QString::fromStdString(loc.filePath).endsWith(file) &&
            loc.line == static_cast<quint32>(line)) {
          foundIP = ip;
          break;
        }
      }

      if (foundIP.has_value()) {
        // Found matching IP, add real breakpoint
        m_debugger->addBreakpoint(foundIP.value(), file.toStdString(), static_cast<quint32>(line));
        updateBreakpointsDisplay();
      } else {
        // No mapping found, show error
        QMessageBox::warning(this, "Breakpoint Error",
                             QString("Could not find instruction at %1:%2.\n"
                                     "This may be because:\n"
                                     "- The source file doesn't match\n"
                                     "- No executable code exists at that line\n"
                                     "- Source mappings are not available")
                                 .arg(file)
                                 .arg(line));
      }
    }
  } else {
    // Try as IP address
    bool ipOk = false;
    quint32 ip = input.toUInt(&ipOk);
    if (ipOk && m_debugger) {
      [[maybe_unused]] quint32 bpId = m_debugger->addBreakpoint(ip);
      updateBreakpointsDisplay();
    }
  }
}

void NMScriptRuntimeInspectorPanel::onRemoveBreakpointClicked() {
  auto* item = m_breakpointsTree->currentItem();
  if (!item) {
    return;
  }

  quint32 ip = item->data(0, Qt::UserRole).toUInt();
  if (m_debugger) {
    m_debugger->removeBreakpointsAt(ip);
  }
  delete item;
}

void NMScriptRuntimeInspectorPanel::onClearBreakpointsClicked() {
  if (m_debugger) {
    m_debugger->clearAllBreakpoints();
  }
  m_breakpointsTree->clear();
}

// =========================================================================
// Update Methods
// =========================================================================

void NMScriptRuntimeInspectorPanel::updateControlsState() {
  bool isRunning = m_executionState == DebugExecutionState::Running;
  bool isPaused = m_executionState == DebugExecutionState::PausedBreakpoint ||
                  m_executionState == DebugExecutionState::PausedStep ||
                  m_executionState == DebugExecutionState::PausedUser;
  [[maybe_unused]] bool isIdle = m_executionState == DebugExecutionState::Idle ||
                                 m_executionState == DebugExecutionState::Halted;

  m_continueBtn->setEnabled(isPaused);
  m_pauseBtn->setEnabled(isRunning);
  m_stepIntoBtn->setEnabled(isPaused);
  m_stepOverBtn->setEnabled(isPaused);
  m_stepOutBtn->setEnabled(isPaused);
  m_stopBtn->setEnabled(isRunning || isPaused);
}

void NMScriptRuntimeInspectorPanel::updateStatusDisplay() {
  m_statusLabel->setText(getStateString(m_executionState));

  // Update status color based on state
  QString color;
  switch (m_executionState) {
  case DebugExecutionState::Idle:
  case DebugExecutionState::Halted:
    color = "#888888";
    break;
  case DebugExecutionState::Running:
    color = "#4caf50";
    break;
  case DebugExecutionState::PausedBreakpoint:
    color = "#ff9800";
    break;
  case DebugExecutionState::PausedStep:
  case DebugExecutionState::PausedUser:
    color = "#2196f3";
    break;
  case DebugExecutionState::WaitingInput:
    color = "#9c27b0";
    break;
  }
  m_statusLabel->setStyleSheet(QString("QLabel { font-weight: bold; color: %1; }").arg(color));
}

void NMScriptRuntimeInspectorPanel::updateVariablesDisplay() {
  m_variablesTree->clear();

  // Add variables section
  auto* varsRoot = new QTreeWidgetItem(m_variablesTree, {"Variables"});
  varsRoot->setExpanded(true);
  varsRoot->setFlags(varsRoot->flags() & ~Qt::ItemIsSelectable);
  varsRoot->setForeground(0, QBrush(QColor("#0078d4")));

  for (auto it = m_currentVariables.begin(); it != m_currentVariables.end(); ++it) {
    auto* item = new QTreeWidgetItem(varsRoot);
    item->setText(0, it.key());
    item->setText(1, formatValue(it.value()));
    item->setText(2, getValueTypeString(it.value()));
    item->setData(1, Qt::UserRole, it.value());
  }

  // Add flags section
  auto* flagsRoot = new QTreeWidgetItem(m_variablesTree, {"Flags"});
  flagsRoot->setExpanded(true);
  flagsRoot->setFlags(flagsRoot->flags() & ~Qt::ItemIsSelectable);
  flagsRoot->setForeground(0, QBrush(QColor("#4caf50")));

  for (auto it = m_currentFlags.begin(); it != m_currentFlags.end(); ++it) {
    auto* item = new QTreeWidgetItem(flagsRoot);
    item->setText(0, it.key());
    item->setText(1, it.value().toBool() ? "true" : "false");
    item->setText(2, "bool");
    item->setData(1, Qt::UserRole, it.value());
    // Color code based on value
    item->setForeground(1, QBrush(it.value().toBool() ? QColor("#4caf50") : QColor("#f44336")));
  }

  m_variablesTree->expandAll();
}

void NMScriptRuntimeInspectorPanel::updateCallStackDisplay() {
  m_callStackList->clear();

  int frameNum = 0;
  for (const QString& frame : m_currentCallStack) {
    auto* item = new QListWidgetItem(QString("#%1 %2").arg(frameNum++).arg(frame), m_callStackList);

    // Highlight current frame
    if (frameNum == 1) {
      item->setBackground(QBrush(QColor("#2d2d30")));
      item->setForeground(QBrush(QColor("#ff9800")));
    }
  }
}

void NMScriptRuntimeInspectorPanel::updateBreakpointsDisplay() {
  m_breakpointsTree->clear();

  if (!m_debugger) {
    return;
  }

  auto breakpoints = m_debugger->getAllBreakpoints();
  for (const auto& bp : breakpoints) {
    auto* item = new QTreeWidgetItem(m_breakpointsTree);
    item->setCheckState(0, bp.enabled ? Qt::Checked : Qt::Unchecked);
    item->setData(0, Qt::UserRole, bp.instructionPointer);

    // Format location
    QString location;
    if (!bp.sourceFile.empty()) {
      location = QString::fromStdString(bp.sourceFile) + ":" + QString::number(bp.sourceLine);
    } else {
      location = QString("IP: %1").arg(bp.instructionPointer);
    }
    item->setText(1, location);

    item->setText(2, QString::fromStdString(bp.condition));
    item->setText(3, QString::number(bp.hitCount));
  }
}

void NMScriptRuntimeInspectorPanel::updateHistoryDisplay() {
  m_historyTree->clear();

  if (!m_debugger) {
    return;
  }

  auto changes = m_debugger->getRecentVariableChanges(50);
  for (const auto& change : changes) {
    auto* item = new QTreeWidgetItem(m_historyTree);
    item->setText(0, QString::fromStdString(change.name));
    item->setText(1, formatValue(QVariant::fromValue(
                         QString::fromStdString(scripting::asString(change.oldValue)))));
    item->setText(2, formatValue(QVariant::fromValue(
                         QString::fromStdString(scripting::asString(change.newValue)))));
    item->setText(3, QString::number(change.sourceLine));
  }
}

void NMScriptRuntimeInspectorPanel::updatePerformanceMetrics(double deltaTime) {
  m_lastDeltaTime = deltaTime;

  // Frame time
  m_frameTimeItem->setText(1, QString("%1 ms").arg(deltaTime * 1000.0, 0, 'f', 2));

  // Instruction rate (approximate)
  if (deltaTime > 0) {
    double rate = static_cast<double>(m_instructionCount) / deltaTime;
    m_instructionRateItem->setText(1, QString("%1 instr/s").arg(static_cast<int>(rate)));
    m_instructionCount = 0;
  }

  // Memory usage would require platform-specific code
  m_memoryItem->setText(1, "N/A");

  // Scene execution time
  m_sceneTimeItem->setText(1, QString("%1 s").arg(m_totalSceneTime, 0, 'f', 3));
  m_totalSceneTime += deltaTime;
}

void NMScriptRuntimeInspectorPanel::editVariable(const QString& name,
                                                 const QVariant& currentValue) {
  bool ok = false;
  QString newValueStr =
      NMInputDialog::getText(this, "Edit Variable", QString("Enter new value for '%1':").arg(name),
                             QLineEdit::Normal, formatValue(currentValue), &ok);

  if (!ok || newValueStr.isEmpty()) {
    return;
  }

  if (m_debugger) {
    // Parse the new value based on type
    scripting::Value newValue;

    // Try to parse as different types
    bool isInt = false;
    int intVal = newValueStr.toInt(&isInt);
    if (isInt) {
      newValue = static_cast<NovelMind::i32>(intVal);
    } else {
      bool isFloat = false;
      float floatVal = newValueStr.toFloat(&isFloat);
      if (isFloat) {
        newValue = floatVal;
      } else if (newValueStr.toLower() == "true") {
        newValue = true;
      } else if (newValueStr.toLower() == "false") {
        newValue = false;
      } else {
        newValue = newValueStr.toStdString();
      }
    }

    m_debugger->setVariable(name.toStdString(), newValue);
    updateVariablesDisplay();
  }
}

QString NMScriptRuntimeInspectorPanel::formatValue(const QVariant& value) const {
  if (value.typeId() == QMetaType::Bool) {
    return value.toBool() ? "true" : "false";
  }
  return value.toString();
}

QString NMScriptRuntimeInspectorPanel::getValueTypeString(const QVariant& value) const {
  switch (value.typeId()) {
  case QMetaType::Int:
    return "int";
  case QMetaType::Double:
    return "float";
  case QMetaType::Bool:
    return "bool";
  case QMetaType::QString:
    return "string";
  default:
    return "unknown";
  }
}

QString NMScriptRuntimeInspectorPanel::getStateString(DebugExecutionState state) const {
  switch (state) {
  case DebugExecutionState::Idle:
    return "Idle";
  case DebugExecutionState::Running:
    return "Running";
  case DebugExecutionState::PausedBreakpoint:
    return "Paused (Breakpoint)";
  case DebugExecutionState::PausedStep:
    return "Paused (Step)";
  case DebugExecutionState::PausedUser:
    return "Paused";
  case DebugExecutionState::WaitingInput:
    return "Waiting for Input";
  case DebugExecutionState::Halted:
    return "Halted";
  }
  return "Unknown";
}

QIcon NMScriptRuntimeInspectorPanel::getStateIcon(DebugExecutionState state) const {
  auto& iconMgr = NMIconManager::instance();
  switch (state) {
  case DebugExecutionState::Idle:
  case DebugExecutionState::Halted:
    return iconMgr.getIcon("stop", 16);
  case DebugExecutionState::Running:
    return iconMgr.getIcon("play", 16);
  case DebugExecutionState::PausedBreakpoint:
  case DebugExecutionState::PausedStep:
  case DebugExecutionState::PausedUser:
    return iconMgr.getIcon("pause", 16);
  case DebugExecutionState::WaitingInput:
    return iconMgr.getIcon("wait", 16);
  }
  return QIcon();
}

} // namespace NovelMind::editor::qt
