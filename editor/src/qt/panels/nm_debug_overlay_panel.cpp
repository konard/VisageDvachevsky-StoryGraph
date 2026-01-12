#include <NovelMind/editor/qt/nm_dialogs.hpp>
#include <NovelMind/editor/qt/nm_icon_manager.hpp>
#include <NovelMind/editor/qt/nm_play_mode_controller.hpp>
#include <NovelMind/editor/qt/panels/nm_debug_overlay_panel.hpp>
#include <QButtonGroup>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <limits>

namespace NovelMind::editor::qt {

NMDebugOverlayPanel::NMDebugOverlayPanel(QWidget* parent) : NMDockPanel("Debug Overlay", parent) {
  setupUI();
}

void NMDebugOverlayPanel::onInitialize() {
  NMDockPanel::onInitialize();

  auto& controller = NMPlayModeController::instance();

  // Connect to controller signals
  connect(&controller, &NMPlayModeController::variablesChanged, this,
          &NMDebugOverlayPanel::onVariablesChanged);
  connect(&controller, &NMPlayModeController::flagsChanged, this,
          &NMDebugOverlayPanel::onFlagsChanged);
  connect(&controller, &NMPlayModeController::callStackChanged, this,
          &NMDebugOverlayPanel::onCallStackChanged);
  connect(&controller, &NMPlayModeController::stackFramesChanged, this,
          &NMDebugOverlayPanel::onStackFramesChanged);
  connect(&controller, &NMPlayModeController::currentNodeChanged, this,
          &NMDebugOverlayPanel::onCurrentNodeChanged);
  connect(&controller, &NMPlayModeController::executionStepChanged, this,
          &NMDebugOverlayPanel::onExecutionStepChanged);
  connect(&controller, &NMPlayModeController::playModeChanged, this,
          &NMDebugOverlayPanel::onPlayModeChanged);

  // Initial update
  updateVariablesTab(controller.currentVariables(), controller.currentFlags());
  updateCallStackTab(controller.callStack());
  updateStackFrames(controller.currentStackFrames());
  onCurrentNodeChanged(controller.currentNodeId());
  onExecutionStepChanged(controller.currentStepIndex(), controller.totalSteps(),
                         controller.currentInstructionText());
}

void NMDebugOverlayPanel::onShutdown() {
  NMDockPanel::onShutdown();
}

void NMDebugOverlayPanel::onUpdate(double deltaTime) {
  NMDockPanel::onUpdate(deltaTime);
  updatePerformanceMetrics(deltaTime);
}

void NMDebugOverlayPanel::setupUI() {
  // Create content widget
  auto* contentWidget = new QWidget;
  auto* layout = new QVBoxLayout(contentWidget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Add toolbar
  setupToolBar();
  layout->addWidget(m_toolBar);

  m_tabWidget = new QTabWidget;

  auto& iconMgr = NMIconManager::instance();

  // === Variables Tab ===
  {
    auto* varWidget = new QWidget;
    auto* varLayout = new QVBoxLayout(varWidget);
    varLayout->setContentsMargins(4, 4, 4, 4);

    m_variablesTree = new QTreeWidget;
    m_variablesTree->setHeaderLabels({"Name", "Value", "Type"});
    m_variablesTree->setAlternatingRowColors(true);
    m_variablesTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_variablesTree->header()->setStretchLastSection(false);
    m_variablesTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_variablesTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_variablesTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    connect(m_variablesTree, &QTreeWidget::itemDoubleClicked, this,
            &NMDebugOverlayPanel::onVariableItemDoubleClicked);

    auto* helpLabel = new QLabel("💡 Double-click a variable to edit (only when paused)");
    helpLabel->setStyleSheet("QLabel { color: #a0a0a0; font-size: 9pt; padding: 4px; }");

    varLayout->addWidget(m_variablesTree);
    varLayout->addWidget(helpLabel);

    m_tabWidget->addTab(varWidget, iconMgr.getIcon("info", 16), "Variables");
  }

  // === Call Stack Tab ===
  {
    auto* stackWidget = new QWidget;
    auto* stackLayout = new QVBoxLayout(stackWidget);
    stackLayout->setContentsMargins(4, 4, 4, 4);

    m_callStackList = new QListWidget;
    m_callStackList->setAlternatingRowColors(true);

    stackLayout->addWidget(m_callStackList);

    m_tabWidget->addTab(stackWidget, "Call Stack");
  }

  // === Current Instruction Tab ===
  {
    m_instructionWidget = new QWidget;
    auto* instrLayout = new QGridLayout(m_instructionWidget);
    instrLayout->setContentsMargins(8, 8, 8, 8);
    instrLayout->setSpacing(8);

    // Current Node
    auto* nodeHeaderLabel = new QLabel("<b>Current Node:</b>");
    instrLayout->addWidget(nodeHeaderLabel, 0, 0, Qt::AlignTop);

    m_currentNodeLabel = new QLabel("DialogueNode_003");
    m_currentNodeLabel->setStyleSheet("QLabel { color: #0078d4; font-family: monospace; }");
    instrLayout->addWidget(m_currentNodeLabel, 0, 1);

    // Instruction Index
    auto* indexHeaderLabel = new QLabel("<b>Instruction Index:</b>");
    instrLayout->addWidget(indexHeaderLabel, 1, 0, Qt::AlignTop);

    m_instructionIndexLabel = new QLabel("5 / 12");
    m_instructionIndexLabel->setStyleSheet("QLabel { color: #4caf50; font-family: monospace; }");
    instrLayout->addWidget(m_instructionIndexLabel, 1, 1);

    // Separator
    auto* separator = new QFrame;
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    instrLayout->addWidget(separator, 2, 0, 1, 2);

    // Current Instruction Code
    auto* codeHeaderLabel = new QLabel("<b>Current Instruction:</b>");
    instrLayout->addWidget(codeHeaderLabel, 3, 0, Qt::AlignTop);

    m_instructionCodeLabel = new QLabel("SHOW_TEXT \"Hello, world!\"");
    m_instructionCodeLabel->setStyleSheet("QLabel { "
                                          "  background-color: #1e1e1e; "
                                          "  color: #e0e0e0; "
                                          "  font-family: monospace; "
                                          "  padding: 8px; "
                                          "  border: 1px solid #3d3d3d; "
                                          "  border-radius: 3px; "
                                          "}");
    m_instructionCodeLabel->setWordWrap(true);
    instrLayout->addWidget(m_instructionCodeLabel, 3, 1);

    // Stack frames
    auto* stackHeader = new QLabel("<b>Call Stack Frames:</b>");
    instrLayout->addWidget(stackHeader, 4, 0, Qt::AlignTop);

    m_stackFramesTree = new QTreeWidget;
    m_stackFramesTree->setHeaderLabels({"#", "Scene", "Func", "IP", "File:Line"});
    m_stackFramesTree->setAlternatingRowColors(true);
    m_stackFramesTree->setMinimumHeight(120);
    m_stackFramesTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_stackFramesTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_stackFramesTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_stackFramesTree->header()->setStretchLastSection(true);
    instrLayout->addWidget(m_stackFramesTree, 4, 1);

    instrLayout->setRowStretch(4, 1); // Push content to top

    m_tabWidget->addTab(m_instructionWidget, "Current Instruction");
  }

  // === Animations Tab ===
  {
    auto* animWidget = new QWidget;
    auto* animLayout = new QVBoxLayout(animWidget);
    animLayout->setContentsMargins(4, 4, 4, 4);

    m_animationsTree = new QTreeWidget;
    m_animationsTree->setHeaderLabels({"Animation", "Progress", "Target"});
    m_animationsTree->setAlternatingRowColors(true);
    new QTreeWidgetItem(m_animationsTree, {"(No active animations)", "", ""});

    animLayout->addWidget(m_animationsTree);

    m_tabWidget->addTab(animWidget, "Animations");
  }

  // === Audio Tab ===
  {
    auto* audioWidget = new QWidget;
    auto* audioLayout = new QVBoxLayout(audioWidget);
    audioLayout->setContentsMargins(4, 4, 4, 4);

    m_audioTree = new QTreeWidget;
    m_audioTree->setHeaderLabels({"Channel", "File", "Volume", "State"});
    m_audioTree->setAlternatingRowColors(true);
    new QTreeWidgetItem(m_audioTree, {"(No active audio)", "", "", ""});

    audioLayout->addWidget(m_audioTree);

    m_tabWidget->addTab(audioWidget, "Audio");
  }

  // === Performance Tab ===
  {
    auto* perfWidget = new QWidget;
    auto* perfLayout = new QVBoxLayout(perfWidget);
    perfLayout->setContentsMargins(4, 4, 4, 4);

    m_performanceTree = new QTreeWidget;
    m_performanceTree->setHeaderLabels({"Metric", "Value"});
    m_performanceTree->setAlternatingRowColors(true);
    m_performanceTree->header()->setStretchLastSection(true);

    m_frameTimeItem = new QTreeWidgetItem(m_performanceTree, {"Frame Time", "-"});
    m_fpsItem = new QTreeWidgetItem(m_performanceTree, {"FPS", "-"});
    m_memoryItem = new QTreeWidgetItem(m_performanceTree, {"Memory Usage", "-"});
    m_objectCountItem = new QTreeWidgetItem(m_performanceTree, {"Active Objects", "-"});
    m_instructionRateItem =
        new QTreeWidgetItem(m_performanceTree, {"Script Instructions/sec", "-"});

    perfLayout->addWidget(m_performanceTree);

    m_tabWidget->addTab(perfWidget, "Performance");
  }

  layout->addWidget(m_tabWidget);

  // Use setContentWidget instead of setLayout
  setContentWidget(contentWidget);
}

void NMDebugOverlayPanel::updateVariablesTab(const QVariantMap& variables,
                                             const QVariantMap& flags) {
  m_currentVariables = variables;
  m_currentFlags = flags;

  m_variablesTree->clear();

  // Create top-level groups
  auto* globalGroup = new QTreeWidgetItem(m_variablesTree, {"📁 Global Variables", "", ""});
  globalGroup->setExpanded(true);
  globalGroup->setForeground(0, QBrush(QColor("#0078d4")));

  auto* localGroup = new QTreeWidgetItem(m_variablesTree, {"📁 Local Variables", "", ""});
  localGroup->setExpanded(true);
  localGroup->setForeground(0, QBrush(QColor("#0078d4")));

  auto* flagsGroup = new QTreeWidgetItem(m_variablesTree, {"📁 Flags", "", ""});
  flagsGroup->setExpanded(true);
  flagsGroup->setForeground(0, QBrush(QColor("#d48c00")));

  // Populate variables (all go to global for now)
  for (auto it = variables.constBegin(); it != variables.constEnd(); ++it) {
    const QString& name = it.key();
    const QVariant& value = it.value();

    QString valueStr = value.toString();
    QString typeStr = value.typeName();

    // Add quotes for strings
    if (value.metaType().id() == QMetaType::QString) {
      valueStr = QString("\"%1\"").arg(valueStr);
    }

    auto* item = new QTreeWidgetItem(globalGroup, {name, valueStr, typeStr});

    // Color-code by type
    QColor valueColor;
    if (value.metaType().id() == QMetaType::QString) {
      valueColor = QColor("#ce9178"); // String color
    } else if (value.metaType().id() == QMetaType::Int ||
               value.metaType().id() == QMetaType::Double) {
      valueColor = QColor("#b5cea8"); // Number color
    } else {
      valueColor = QColor("#e0e0e0"); // Default
    }
    item->setForeground(1, QBrush(valueColor));
    item->setForeground(2, QBrush(QColor("#a0a0a0")));

    // Store variable name in item data for editing
    item->setData(0, Qt::UserRole, name);
  }

  if (variables.isEmpty()) {
    new QTreeWidgetItem(globalGroup, {"(No variables)", "", ""});
  }

  // Populate flags
  for (auto it = flags.constBegin(); it != flags.constEnd(); ++it) {
    const QString& name = it.key();
    const bool value = it.value().toBool();
    auto* item = new QTreeWidgetItem(flagsGroup, {name, value ? "true" : "false", "Flag"});
    item->setForeground(1, QBrush(value ? QColor("#4caf50") : QColor("#f44336")));
    item->setData(0, Qt::UserRole, QString()); // not editable
  }
  if (flags.isEmpty()) {
    new QTreeWidgetItem(flagsGroup, {"(No flags)", "", ""});
  }
}

void NMDebugOverlayPanel::updateCallStackTab(const QStringList& stack) {
  m_currentCallStack = stack;

  m_callStackList->clear();

  for (int i = static_cast<int>(stack.size()) - 1; i >= 0;
       --i) { // Reverse order (top of stack first)
    const QString& frame = stack[i];
    auto* item = new QListWidgetItem(QString("%1. %2").arg(stack.size() - i).arg(frame));

    if (i == stack.size() - 1) {
      // Highlight current frame
      item->setForeground(QBrush(QColor("#0078d4")));
      item->setIcon(NMIconManager::instance().getIcon("arrow-right", 16));
    }

    m_callStackList->addItem(item);
  }
}

void NMDebugOverlayPanel::onVariablesChanged(const QVariantMap& variables) {
  updateVariablesTab(variables, m_currentFlags);
}

void NMDebugOverlayPanel::onFlagsChanged(const QVariantMap& flags) {
  m_currentFlags = flags;
  updateVariablesTab(m_currentVariables, m_currentFlags);
}

void NMDebugOverlayPanel::onStackFramesChanged(const QVariantList& frames) {
  m_currentStackFrames = frames;
  updateStackFrames(frames);
}

void NMDebugOverlayPanel::onCallStackChanged(const QStringList& stack) {
  updateCallStackTab(stack);
}

void NMDebugOverlayPanel::onCurrentNodeChanged(const QString& nodeId) {
  m_currentNodeId = nodeId;
  updateCurrentInstructionTab();
}

void NMDebugOverlayPanel::onExecutionStepChanged(int stepIndex, int totalSteps,
                                                 const QString& instruction) {
  m_currentStepIndex = stepIndex;
  m_totalSteps = totalSteps;
  m_currentInstruction = instruction;
  updateCurrentInstructionTab();
}

void NMDebugOverlayPanel::onPlayModeChanged([[maybe_unused]] int mode) {
  // Update UI based on play mode
  updateCurrentInstructionTab();
}

void NMDebugOverlayPanel::onVariableItemDoubleClicked(QTreeWidgetItem* item,
                                                      [[maybe_unused]] int column) {
  if (!item->parent()) {
    // Clicked on a group, not a variable
    return;
  }

  auto& controller = NMPlayModeController::instance();

  if (!controller.isPaused()) {
    // Only allow editing when paused
    m_variablesTree->setToolTip("Variables can only be edited when playback is paused");
    return;
  }

  const QString varName = item->data(0, Qt::UserRole).toString();
  if (varName.isEmpty()) {
    return; // Placeholder item
  }

  const QVariant currentValue = m_currentVariables.value(varName);
  editVariable(varName, currentValue);
}

void NMDebugOverlayPanel::editVariable(const QString& name, const QVariant& currentValue) {
  bool ok = false;
  QVariant newValue;

  if (currentValue.metaType().id() == QMetaType::QString) {
    newValue = NMInputDialog::getText(this, "Edit Variable",
                                      QString("Enter new value for '%1':").arg(name),
                                      QLineEdit::Normal, currentValue.toString(), &ok);
  } else if (currentValue.metaType().id() == QMetaType::Int) {
    newValue =
        NMInputDialog::getInt(this, "Edit Variable", QString("Enter new value for '%1':").arg(name),
                              currentValue.toInt(), -2147483647, 2147483647, 1, &ok);
  } else if (currentValue.metaType().id() == QMetaType::Double) {
    newValue = NMInputDialog::getDouble(
        this, "Edit Variable", QString("Enter new value for '%1':").arg(name),
        currentValue.toDouble(), -std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(), 2, &ok);
  } else {
    // Unsupported type, edit as string
    newValue = NMInputDialog::getText(this, "Edit Variable",
                                      QString("Enter new value for '%1':").arg(name),
                                      QLineEdit::Normal, currentValue.toString(), &ok);
  }

  if (ok) {
    NMPlayModeController::instance().setVariable(name, newValue);
  }
}

void NMDebugOverlayPanel::setupToolBar() {
  m_toolBar = new QToolBar;
  m_toolBar->setObjectName("DebugOverlayToolBar");
  m_toolBar->setIconSize(QSize(16, 16));

  [[maybe_unused]] auto& iconMgr = NMIconManager::instance();

  // Display mode toggle
  auto* minimalBtn = new QToolButton;
  minimalBtn->setText("Minimal");
  minimalBtn->setCheckable(true);
  minimalBtn->setToolTip("Show only essential debugging info");

  auto* extendedBtn = new QToolButton;
  extendedBtn->setText("Extended");
  extendedBtn->setCheckable(true);
  extendedBtn->setChecked(true);
  extendedBtn->setToolTip("Show all debugging information");

  auto* modeGroup = new QButtonGroup(this);
  modeGroup->addButton(minimalBtn, static_cast<int>(DebugDisplayMode::Minimal));
  modeGroup->addButton(extendedBtn, static_cast<int>(DebugDisplayMode::Extended));

  connect(modeGroup, QOverload<int>::of(&QButtonGroup::idClicked), this,
          &NMDebugOverlayPanel::onDisplayModeChanged);

  m_toolBar->addWidget(new QLabel("Display Mode: "));
  m_toolBar->addWidget(minimalBtn);
  m_toolBar->addWidget(extendedBtn);
}

void NMDebugOverlayPanel::setDisplayMode(DebugDisplayMode mode) {
  if (m_displayMode == mode)
    return;

  m_displayMode = mode;
  updateTabsVisibility();
}

void NMDebugOverlayPanel::updateTabsVisibility() {
  if (!m_tabWidget)
    return;

  switch (m_displayMode) {
  case DebugDisplayMode::Minimal:
    // Show only Variables and Current Instruction
    m_tabWidget->setTabVisible(0, true);  // Variables
    m_tabWidget->setTabVisible(1, false); // Call Stack
    m_tabWidget->setTabVisible(2, true);  // Current Instruction
    m_tabWidget->setTabVisible(3, false); // Animations
    m_tabWidget->setTabVisible(4, false); // Audio
    m_tabWidget->setTabVisible(5, false); // Performance
    break;

  case DebugDisplayMode::Extended:
    // Show all tabs
    for (int i = 0; i < m_tabWidget->count(); ++i) {
      m_tabWidget->setTabVisible(i, true);
    }
    break;
  }
}

void NMDebugOverlayPanel::onDisplayModeChanged() {
  auto* sender = qobject_cast<QButtonGroup*>(this->sender());
  if (!sender)
    return;

  int modeId = sender->checkedId();
  setDisplayMode(static_cast<DebugDisplayMode>(modeId));
}

void NMDebugOverlayPanel::updatePerformanceMetrics(double deltaTime) {
  if (!m_performanceTree || deltaTime <= 0.0) {
    return;
  }

  m_lastDeltaTime = deltaTime;
  const double frameMs = deltaTime * 1000.0;
  const double fps = 1.0 / deltaTime;

  if (m_frameTimeItem) {
    m_frameTimeItem->setText(1, QString("%1 ms").arg(frameMs, 0, 'f', 2));
  }
  if (m_fpsItem) {
    m_fpsItem->setText(1, QString::number(fps, 'f', 1));
  }
  if (m_memoryItem) {
    m_memoryItem->setText(1, "N/A");
  }
  if (m_objectCountItem) {
    m_objectCountItem->setText(1, "N/A");
  }
  if (m_instructionRateItem) {
    m_instructionRateItem->setText(1, "N/A");
  }
}

void NMDebugOverlayPanel::updateCurrentInstructionTab() {
  auto& controller = NMPlayModeController::instance();

  if (controller.isPlaying() || controller.isPaused()) {
    const QString nodeLabel = m_currentNodeId.isEmpty() ? "(Unknown)" : m_currentNodeId;
    m_currentNodeLabel->setText(nodeLabel);
    const QString indexText = (m_totalSteps > 0 && m_currentStepIndex >= 0)
                                  ? QString("%1 / %2").arg(m_currentStepIndex + 1).arg(m_totalSteps)
                                  : "- / -";
    m_instructionIndexLabel->setText(indexText);
    m_instructionCodeLabel->setText(m_currentInstruction.isEmpty() ? "(No active instruction)"
                                                                   : m_currentInstruction);
    updateStackFrames(m_currentStackFrames);
  } else {
    m_currentNodeLabel->setText("(Not running)");
    m_instructionIndexLabel->setText("- / -");
    m_instructionCodeLabel->setText("(No active instruction)");
    if (m_stackFramesTree) {
      m_stackFramesTree->clear();
      new QTreeWidgetItem(m_stackFramesTree, {"", "(Not running)", "", "", ""});
    }
  }
}

void NMDebugOverlayPanel::updateStackFrames(const QVariantList& frames) {
  if (!m_stackFramesTree) {
    return;
  }

  m_stackFramesTree->clear();

  int idx = 0;
  for (const auto& variantFrame : frames) {
    const QVariantMap frame = variantFrame.toMap();
    const QString scene = frame.value("scene").toString();
    const QString func = frame.value("function").toString();
    const int ip = frame.value("ip").toInt();
    const int line = frame.value("line").toInt();
    const int col = frame.value("column").toInt();
    const QString file = frame.value("file").toString();
    QString fileLine;
    if (!file.isEmpty() || line > 0) {
      fileLine = QString("%1:%2:%3").arg(file.isEmpty() ? "?" : file).arg(line).arg(col);
    }
    auto* item = new QTreeWidgetItem(m_stackFramesTree, {QString::number(++idx), scene,
                                                         func.isEmpty() ? "(scene)" : func,
                                                         QString::number(ip), fileLine});
    item->setForeground(1, QBrush(QColor("#4caf50")));
    item->setForeground(3, QBrush(QColor("#b5cea8")));
  }

  if (frames.isEmpty()) {
    new QTreeWidgetItem(m_stackFramesTree, {"", "(No frames)", "", "", ""});
  }
}

} // namespace NovelMind::editor::qt
