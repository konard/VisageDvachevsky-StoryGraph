#include <NovelMind/editor/qt/nm_dialogs.hpp>
#include <NovelMind/editor/qt/nm_icon_manager.hpp>
#include <NovelMind/editor/qt/nm_play_mode_controller.hpp>
#include <NovelMind/editor/qt/panels/nm_script_inspector_panel.hpp>
#include <QButtonGroup>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QToolButton>
#include <limits>

namespace NovelMind::editor::qt {

NMScriptInspectorPanel::NMScriptInspectorPanel(QWidget* parent)
    : NMDockPanel("Script Inspector", parent) {
  setupUI();
}

void NMScriptInspectorPanel::onInitialize() {
  NMDockPanel::onInitialize();

  auto& controller = NMPlayModeController::instance();

  // Connect to controller signals
  connect(&controller, &NMPlayModeController::variablesChanged, this,
          &NMScriptInspectorPanel::onVariablesChanged);
  connect(&controller, &NMPlayModeController::flagsChanged, this,
          &NMScriptInspectorPanel::onFlagsChanged);
  connect(&controller, &NMPlayModeController::callStackChanged, this,
          &NMScriptInspectorPanel::onCallStackChanged);
  connect(&controller, &NMPlayModeController::stackFramesChanged, this,
          &NMScriptInspectorPanel::onStackFramesChanged);
  connect(&controller, &NMPlayModeController::currentNodeChanged, this,
          &NMScriptInspectorPanel::onCurrentNodeChanged);
  connect(&controller, &NMPlayModeController::playModeChanged, this,
          &NMScriptInspectorPanel::onPlayModeChanged);

  // Initial update
  onVariablesChanged(controller.currentVariables());
  onFlagsChanged(controller.currentFlags());
  onCallStackChanged(controller.callStack());
  onStackFramesChanged(controller.currentStackFrames());
  onCurrentNodeChanged(controller.currentNodeId());
}

void NMScriptInspectorPanel::onShutdown() {
  NMDockPanel::onShutdown();
}

void NMScriptInspectorPanel::onUpdate(double deltaTime) {
  NMDockPanel::onUpdate(deltaTime);
}

void NMScriptInspectorPanel::setupUI() {
  auto* contentWidget = new QWidget;
  auto* layout = new QVBoxLayout(contentWidget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Add toolbar
  setupToolBar();
  layout->addWidget(m_toolBar);

  // Tab widget for different views
  m_tabWidget = new QTabWidget;

  auto& iconMgr = NMIconManager::instance();

  // Setup tabs
  setupVariablesTab();
  setupFlagsTab();
  setupWatchTab();
  setupSceneHistoryTab();

  m_tabWidget->addTab(m_variablesWidget, iconMgr.getIcon("info", 16), "Variables");
  m_tabWidget->addTab(m_flagsWidget, iconMgr.getIcon("flag", 16), "Flags");
  m_tabWidget->addTab(m_watchWidget, iconMgr.getIcon("eye", 16), "Watch");
  m_tabWidget->addTab(m_sceneHistoryWidget, iconMgr.getIcon("history", 16), "Scene History");

  layout->addWidget(m_tabWidget);

  setContentWidget(contentWidget);
}

void NMScriptInspectorPanel::setupToolBar() {
  m_toolBar = new QToolBar;
  m_toolBar->setObjectName("ScriptInspectorToolBar");
  m_toolBar->setIconSize(QSize(16, 16));

  auto& iconMgr = NMIconManager::instance();

  // Refresh button
  auto* refreshBtn = new QToolButton;
  refreshBtn->setIcon(iconMgr.getIcon("refresh", 16));
  refreshBtn->setToolTip("Refresh all values");
  connect(refreshBtn, &QToolButton::clicked, this, [this]() {
    auto& controller = NMPlayModeController::instance();
    onVariablesChanged(controller.currentVariables());
    onFlagsChanged(controller.currentFlags());
    updateWatchTree();
  });

  m_toolBar->addWidget(refreshBtn);
  m_toolBar->addSeparator();

  // Status indicator
  auto* statusLabel = new QLabel("Status: Stopped");
  statusLabel->setObjectName("statusLabel");
  m_toolBar->addWidget(statusLabel);

  m_toolBar->addSeparator();

  // Current scene indicator
  m_currentSceneLabel = new QLabel("Scene: -");
  m_currentSceneLabel->setStyleSheet("QLabel { color: #4caf50; font-weight: bold; }");
  m_toolBar->addWidget(m_currentSceneLabel);
}

void NMScriptInspectorPanel::setupVariablesTab() {
  m_variablesWidget = new QWidget;
  auto* layout = new QVBoxLayout(m_variablesWidget);
  layout->setContentsMargins(4, 4, 4, 4);

  // Filter input
  auto* filterLayout = new QHBoxLayout;
  auto* filterLabel = new QLabel("Filter:");
  m_variablesFilter = new QLineEdit;
  m_variablesFilter->setPlaceholderText("Filter variables...");
  m_variablesFilter->setClearButtonEnabled(true);
  filterLayout->addWidget(filterLabel);
  filterLayout->addWidget(m_variablesFilter);
  layout->addLayout(filterLayout);

  connect(m_variablesFilter, &QLineEdit::textChanged, this,
          &NMScriptInspectorPanel::updateVariablesTree);

  // Variables tree
  m_variablesTree = new QTreeWidget;
  m_variablesTree->setHeaderLabels({"Name", "Value", "Type"});
  m_variablesTree->setAlternatingRowColors(true);
  m_variablesTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_variablesTree->header()->setStretchLastSection(false);
  m_variablesTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_variablesTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_variablesTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

  connect(m_variablesTree, &QTreeWidget::itemDoubleClicked, this,
          &NMScriptInspectorPanel::onVariableItemDoubleClicked);

  layout->addWidget(m_variablesTree);

  auto* helpLabel = new QLabel("Double-click to edit (only when paused)");
  helpLabel->setStyleSheet("QLabel { color: #a0a0a0; font-size: 9pt; padding: 4px; }");
  layout->addWidget(helpLabel);
}

void NMScriptInspectorPanel::setupFlagsTab() {
  m_flagsWidget = new QWidget;
  auto* layout = new QVBoxLayout(m_flagsWidget);
  layout->setContentsMargins(4, 4, 4, 4);

  // Filter input
  auto* filterLayout = new QHBoxLayout;
  auto* filterLabel = new QLabel("Filter:");
  m_flagsFilter = new QLineEdit;
  m_flagsFilter->setPlaceholderText("Filter flags...");
  m_flagsFilter->setClearButtonEnabled(true);
  filterLayout->addWidget(filterLabel);
  filterLayout->addWidget(m_flagsFilter);
  layout->addLayout(filterLayout);

  connect(m_flagsFilter, &QLineEdit::textChanged, this, &NMScriptInspectorPanel::updateFlagsTree);

  // Flags tree
  m_flagsTree = new QTreeWidget;
  m_flagsTree->setHeaderLabels({"Flag Name", "Value"});
  m_flagsTree->setAlternatingRowColors(true);
  m_flagsTree->header()->setStretchLastSection(true);
  m_flagsTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);

  connect(m_flagsTree, &QTreeWidget::itemDoubleClicked, this,
          &NMScriptInspectorPanel::onFlagItemDoubleClicked);

  layout->addWidget(m_flagsTree);

  auto* helpLabel = new QLabel("Double-click to toggle flag (only when paused)");
  helpLabel->setStyleSheet("QLabel { color: #a0a0a0; font-size: 9pt; padding: 4px; }");
  layout->addWidget(helpLabel);
}

void NMScriptInspectorPanel::setupWatchTab() {
  m_watchWidget = new QWidget;
  auto* layout = new QVBoxLayout(m_watchWidget);
  layout->setContentsMargins(4, 4, 4, 4);

  // Input row for adding watch expressions
  auto* inputLayout = new QHBoxLayout;
  m_watchInput = new QLineEdit;
  m_watchInput->setPlaceholderText("Enter expression (e.g., points >= 100)");

  m_addWatchBtn = new QPushButton("Add");
  m_addWatchBtn->setToolTip("Add watch expression");
  connect(m_addWatchBtn, &QPushButton::clicked, this, &NMScriptInspectorPanel::onAddWatchClicked);
  connect(m_watchInput, &QLineEdit::returnPressed, this,
          &NMScriptInspectorPanel::onAddWatchClicked);

  m_removeWatchBtn = new QPushButton("Remove");
  m_removeWatchBtn->setToolTip("Remove selected watch expression");
  connect(m_removeWatchBtn, &QPushButton::clicked, this,
          &NMScriptInspectorPanel::onRemoveWatchClicked);

  m_clearWatchBtn = new QPushButton("Clear All");
  m_clearWatchBtn->setToolTip("Clear all watch expressions");
  connect(m_clearWatchBtn, &QPushButton::clicked, this,
          &NMScriptInspectorPanel::onClearWatchClicked);

  inputLayout->addWidget(m_watchInput);
  inputLayout->addWidget(m_addWatchBtn);
  inputLayout->addWidget(m_removeWatchBtn);
  inputLayout->addWidget(m_clearWatchBtn);
  layout->addLayout(inputLayout);

  // Watch tree
  m_watchTree = new QTreeWidget;
  m_watchTree->setHeaderLabels({"Expression", "Result"});
  m_watchTree->setAlternatingRowColors(true);
  m_watchTree->header()->setStretchLastSection(true);
  m_watchTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);

  connect(m_watchTree, &QTreeWidget::itemDoubleClicked, this,
          &NMScriptInspectorPanel::onWatchItemDoubleClicked);

  layout->addWidget(m_watchTree);

  auto* helpLabel = new QLabel("Supported: variable names, comparisons (==, !=, <, >, <=, >=), "
                               "logical ops (&&, ||)");
  helpLabel->setStyleSheet("QLabel { color: #a0a0a0; font-size: 9pt; padding: 4px; }");
  helpLabel->setWordWrap(true);
  layout->addWidget(helpLabel);
}

void NMScriptInspectorPanel::setupSceneHistoryTab() {
  m_sceneHistoryWidget = new QWidget;
  auto* layout = new QVBoxLayout(m_sceneHistoryWidget);
  layout->setContentsMargins(4, 4, 4, 4);

  // Scene history list
  m_sceneHistoryList = new QListWidget;
  m_sceneHistoryList->setAlternatingRowColors(true);

  connect(m_sceneHistoryList, &QListWidget::itemDoubleClicked, this,
          &NMScriptInspectorPanel::onSceneHistoryItemDoubleClicked);

  layout->addWidget(m_sceneHistoryList);

  auto* helpLabel = new QLabel("Double-click to navigate to scene in Story Graph");
  helpLabel->setStyleSheet("QLabel { color: #a0a0a0; font-size: 9pt; padding: 4px; }");
  layout->addWidget(helpLabel);
}

void NMScriptInspectorPanel::onVariablesChanged(const QVariantMap& variables) {
  m_currentVariables = variables;
  updateVariablesTree();
  updateWatchTree(); // Watch expressions may depend on variables
}

void NMScriptInspectorPanel::onFlagsChanged(const QVariantMap& flags) {
  m_currentFlags = flags;
  updateFlagsTree();
  updateWatchTree(); // Watch expressions may depend on flags
}

void NMScriptInspectorPanel::onCallStackChanged(const QStringList& stack) {
  m_currentCallStack = stack;
}

void NMScriptInspectorPanel::onStackFramesChanged(const QVariantList& frames) {
  m_currentStackFrames = frames;
}

void NMScriptInspectorPanel::onPlayModeChanged(int mode) {
  auto* statusLabel = m_toolBar->findChild<QLabel*>("statusLabel");
  if (statusLabel) {
    QString statusText;
    QString color;
    switch (mode) {
    case NMPlayModeController::Stopped:
      statusText = "Status: Stopped";
      color = "#808080";
      break;
    case NMPlayModeController::Playing:
      statusText = "Status: Running";
      color = "#4caf50";
      break;
    case NMPlayModeController::Paused:
      statusText = "Status: Paused";
      color = "#ff9800";
      break;
    }
    statusLabel->setText(statusText);
    statusLabel->setStyleSheet(QString("QLabel { color: %1; font-weight: bold; }").arg(color));
  }
}

void NMScriptInspectorPanel::onCurrentNodeChanged(const QString& nodeId) {
  m_currentNodeId = nodeId;

  // Extract scene name from node ID if possible
  // Typical format: "scene_name" or "scene_name_001"
  QString sceneName = nodeId;
  if (sceneName.contains('_')) {
    // Try to get the scene part
    qsizetype lastUnderscore = sceneName.lastIndexOf('_');
    QString suffix = sceneName.mid(lastUnderscore + 1);
    bool isNumber = false;
    suffix.toInt(&isNumber);
    if (isNumber) {
      sceneName = sceneName.left(lastUnderscore);
    }
  }

  m_currentSceneLabel->setText(QString("Scene: %1").arg(sceneName.isEmpty() ? "-" : sceneName));

  // Add to scene history if different from last
  if (!sceneName.isEmpty()) {
    if (m_sceneHistory.isEmpty() || m_sceneHistory.last() != sceneName) {
      m_sceneHistory.append(sceneName);
      updateSceneHistoryList();
    }
  }
}

void NMScriptInspectorPanel::updateVariablesTree() {
  m_variablesTree->clear();

  QString filter = m_variablesFilter->text().toLower();

  // Create groups
  auto* globalGroup = new QTreeWidgetItem(m_variablesTree, {"Global Variables", "", ""});
  globalGroup->setExpanded(true);
  globalGroup->setForeground(0, QBrush(QColor("#0078d4")));

  auto* localGroup = new QTreeWidgetItem(m_variablesTree, {"Local Variables", "", ""});
  localGroup->setExpanded(true);
  localGroup->setForeground(0, QBrush(QColor("#0078d4")));

  int globalCount = 0;
  int localCount = 0;

  for (auto it = m_currentVariables.constBegin(); it != m_currentVariables.constEnd(); ++it) {
    const QString& name = it.key();
    const QVariant& value = it.value();

    // Apply filter
    if (!filter.isEmpty() && !name.toLower().contains(filter)) {
      continue;
    }

    QString valueStr = value.toString();
    QString typeStr = value.typeName();

    // Add quotes for strings
    if (value.metaType().id() == QMetaType::QString) {
      valueStr = QString("\"%1\"").arg(valueStr);
    }

    // Determine if local or global (simple heuristic: underscore prefix = local)
    bool isLocal = name.startsWith("_");
    auto* parentGroup = isLocal ? localGroup : globalGroup;

    auto* item = new QTreeWidgetItem(parentGroup, {name, valueStr, typeStr});

    // Color-code by type
    QColor valueColor;
    if (value.metaType().id() == QMetaType::QString) {
      valueColor = QColor("#ce9178"); // String color
    } else if (value.metaType().id() == QMetaType::Int ||
               value.metaType().id() == QMetaType::Double) {
      valueColor = QColor("#b5cea8"); // Number color
    } else if (value.metaType().id() == QMetaType::Bool) {
      valueColor = value.toBool() ? QColor("#4caf50") : QColor("#f44336");
    } else {
      valueColor = QColor("#e0e0e0"); // Default
    }
    item->setForeground(1, QBrush(valueColor));
    item->setForeground(2, QBrush(QColor("#a0a0a0")));

    // Store variable name in item data for editing
    item->setData(0, Qt::UserRole, name);

    if (isLocal) {
      localCount++;
    } else {
      globalCount++;
    }
  }

  // Update group labels with counts
  globalGroup->setText(0, QString("Global Variables (%1)").arg(globalCount));
  localGroup->setText(0, QString("Local Variables (%1)").arg(localCount));

  // Remove empty groups
  if (globalCount == 0) {
    delete m_variablesTree->takeTopLevelItem(m_variablesTree->indexOfTopLevelItem(globalGroup));
  }
  if (localCount == 0) {
    delete m_variablesTree->takeTopLevelItem(m_variablesTree->indexOfTopLevelItem(localGroup));
  }

  if (m_currentVariables.isEmpty()) {
    new QTreeWidgetItem(m_variablesTree, {"(No variables)", "", ""});
  }
}

void NMScriptInspectorPanel::updateFlagsTree() {
  m_flagsTree->clear();

  QString filter = m_flagsFilter->text().toLower();

  for (auto it = m_currentFlags.constBegin(); it != m_currentFlags.constEnd(); ++it) {
    const QString& name = it.key();
    const bool value = it.value().toBool();

    // Apply filter
    if (!filter.isEmpty() && !name.toLower().contains(filter)) {
      continue;
    }

    auto* item = new QTreeWidgetItem(m_flagsTree, {name, value ? "true" : "false"});
    item->setForeground(1, QBrush(value ? QColor("#4caf50") : QColor("#f44336")));
    item->setData(0, Qt::UserRole, name);

    // Add icon indicator
    auto& iconMgr = NMIconManager::instance();
    item->setIcon(1, iconMgr.getIcon(value ? "check" : "x", 16));
  }

  if (m_currentFlags.isEmpty()) {
    new QTreeWidgetItem(m_flagsTree, {"(No flags)", ""});
  }
}

void NMScriptInspectorPanel::updateWatchTree() {
  m_watchTree->clear();

  for (const QString& expr : m_watchExpressions) {
    auto result = evaluateExpression(expr);

    auto* item = new QTreeWidgetItem(m_watchTree, {expr, result.result});

    if (result.isValid) {
      if (result.isBoolean) {
        bool boolValue = result.result == "true";
        item->setForeground(1, QBrush(boolValue ? QColor("#4caf50") : QColor("#f44336")));
      } else {
        item->setForeground(1, QBrush(QColor("#b5cea8")));
      }
    } else {
      item->setForeground(1, QBrush(QColor("#ff6b6b")));
      item->setToolTip(1, "Invalid expression or variable not found");
    }

    item->setData(0, Qt::UserRole, expr);
  }

  if (m_watchExpressions.isEmpty()) {
    new QTreeWidgetItem(m_watchTree, {"(No watch expressions)", "Add one above"});
  }
}

void NMScriptInspectorPanel::updateSceneHistoryList() {
  m_sceneHistoryList->clear();

  const qsizetype historyCount = m_sceneHistory.size();
  for (qsizetype i = historyCount - 1; i >= 0; --i) {
    const QString& scene = m_sceneHistory[i];
    auto* item =
        new QListWidgetItem(QString("%1. %2").arg(static_cast<int>(historyCount - i)).arg(scene));

    if (i == historyCount - 1) {
      // Current scene
      item->setForeground(QBrush(QColor("#4caf50")));
      auto& iconMgr = NMIconManager::instance();
      item->setIcon(iconMgr.getIcon("arrow-right", 16));
    }

    item->setData(Qt::UserRole, scene);
    m_sceneHistoryList->addItem(item);
    if (i == 0) {
      break;
    }
  }

  if (m_sceneHistory.isEmpty()) {
    m_sceneHistoryList->addItem("(No scene history)");
  }
}

WatchExpressionResult NMScriptInspectorPanel::evaluateExpression(const QString& expression) const {
  WatchExpressionResult result;
  result.expression = expression;
  result.isValid = false;

  if (expression.isEmpty()) {
    result.result = "(empty)";
    return result;
  }

  // Check if it's a simple variable lookup
  QString trimmed = expression.trimmed();

  // Check if it's a simple variable
  if (m_currentVariables.contains(trimmed)) {
    result.result = m_currentVariables.value(trimmed).toString();
    result.isValid = true;
    result.isBoolean = m_currentVariables.value(trimmed).metaType().id() == QMetaType::Bool;
    return result;
  }

  // Check if it's a flag
  if (m_currentFlags.contains(trimmed)) {
    result.result = m_currentFlags.value(trimmed).toBool() ? "true" : "false";
    result.isValid = true;
    result.isBoolean = true;
    return result;
  }

  // Try to parse as expression
  return parseSimpleExpression(expression);
}

WatchExpressionResult
NMScriptInspectorPanel::parseSimpleExpression(const QString& expression) const {
  WatchExpressionResult result;
  result.expression = expression;
  result.isValid = false;
  result.isBoolean = true;

  QString expr = expression.trimmed();

  // Handle logical operators (&&, ||)
  if (expr.contains("&&")) {
    QStringList parts = expr.split("&&");
    bool allTrue = true;
    for (const QString& part : parts) {
      auto subResult = evaluateExpression(part.trimmed());
      if (!subResult.isValid) {
        result.result = "(invalid)";
        return result;
      }
      if (subResult.result != "true") {
        allTrue = false;
      }
    }
    result.result = allTrue ? "true" : "false";
    result.isValid = true;
    return result;
  }

  if (expr.contains("||")) {
    QStringList parts = expr.split("||");
    bool anyTrue = false;
    for (const QString& part : parts) {
      auto subResult = evaluateExpression(part.trimmed());
      if (!subResult.isValid) {
        result.result = "(invalid)";
        return result;
      }
      if (subResult.result == "true") {
        anyTrue = true;
      }
    }
    result.result = anyTrue ? "true" : "false";
    result.isValid = true;
    return result;
  }

  // Handle comparison operators
  static const QRegularExpression comparisonRx(R"(^\s*(\w+)\s*(==|!=|<=|>=|<|>)\s*(.+)\s*$)");
  auto match = comparisonRx.match(expr);

  if (match.hasMatch()) {
    QString varName = match.captured(1);
    QString op = match.captured(2);
    QString valueStr = match.captured(3).trimmed();

    // Get variable value
    QVariant varValue;
    if (m_currentVariables.contains(varName)) {
      varValue = m_currentVariables.value(varName);
    } else if (m_currentFlags.contains(varName)) {
      varValue = m_currentFlags.value(varName);
    } else {
      result.result = "(variable not found)";
      return result;
    }

    // Parse comparison value
    bool isNumber = false;
    double compareValue = valueStr.toDouble(&isNumber);

    if (isNumber) {
      double varDouble = varValue.toDouble();
      bool compResult = false;

      if (op == "==") {
        compResult = (varDouble == compareValue);
      } else if (op == "!=") {
        compResult = (varDouble != compareValue);
      } else if (op == "<") {
        compResult = (varDouble < compareValue);
      } else if (op == ">") {
        compResult = (varDouble > compareValue);
      } else if (op == "<=") {
        compResult = (varDouble <= compareValue);
      } else if (op == ">=") {
        compResult = (varDouble >= compareValue);
      }

      result.result = compResult ? "true" : "false";
      result.isValid = true;
      return result;
    }

    // String comparison
    QString varStr = varValue.toString();
    // Remove quotes if present
    if ((valueStr.startsWith('"') && valueStr.endsWith('"')) ||
        (valueStr.startsWith('\'') && valueStr.endsWith('\''))) {
      valueStr = valueStr.mid(1, valueStr.length() - 2);
    }

    bool compResult = false;
    if (op == "==") {
      compResult = (varStr == valueStr);
    } else if (op == "!=") {
      compResult = (varStr != valueStr);
    }

    result.result = compResult ? "true" : "false";
    result.isValid = true;
    return result;
  }

  result.result = "(invalid expression)";
  return result;
}

void NMScriptInspectorPanel::onVariableItemDoubleClicked(QTreeWidgetItem* item,
                                                         [[maybe_unused]] int column) {
  if (!item->parent()) {
    // Clicked on a group, not a variable
    return;
  }

  auto& controller = NMPlayModeController::instance();

  if (!controller.isPaused()) {
    QMessageBox::information(this, "Edit Variable", "Variables can only be edited when paused.");
    return;
  }

  const QString varName = item->data(0, Qt::UserRole).toString();
  if (varName.isEmpty()) {
    return;
  }

  const QVariant currentValue = m_currentVariables.value(varName);
  editVariable(varName, currentValue);
}

void NMScriptInspectorPanel::onFlagItemDoubleClicked(QTreeWidgetItem* item,
                                                     [[maybe_unused]] int column) {
  auto& controller = NMPlayModeController::instance();

  if (!controller.isPaused()) {
    QMessageBox::information(this, "Toggle Flag", "Flags can only be toggled when paused.");
    return;
  }

  const QString flagName = item->data(0, Qt::UserRole).toString();
  if (flagName.isEmpty()) {
    return;
  }

  const bool currentValue = m_currentFlags.value(flagName).toBool();
  editFlag(flagName, currentValue);
}

void NMScriptInspectorPanel::onWatchItemDoubleClicked(QTreeWidgetItem* item,
                                                      [[maybe_unused]] int column) {
  const QString expr = item->data(0, Qt::UserRole).toString();
  if (!expr.isEmpty()) {
    // Copy expression to input for editing
    m_watchInput->setText(expr);
    m_watchInput->setFocus();
  }
}

void NMScriptInspectorPanel::onSceneHistoryItemDoubleClicked(QListWidgetItem* item) {
  const QString sceneId = item->data(Qt::UserRole).toString();
  if (!sceneId.isEmpty()) {
    emit navigateToSceneRequested(sceneId);
  }
}

void NMScriptInspectorPanel::onAddWatchClicked() {
  QString expr = m_watchInput->text().trimmed();
  if (expr.isEmpty()) {
    return;
  }

  if (!m_watchExpressions.contains(expr)) {
    m_watchExpressions.append(expr);
    updateWatchTree();
  }

  m_watchInput->clear();
}

void NMScriptInspectorPanel::onRemoveWatchClicked() {
  auto* current = m_watchTree->currentItem();
  if (!current) {
    return;
  }

  const QString expr = current->data(0, Qt::UserRole).toString();
  m_watchExpressions.removeAll(expr);
  updateWatchTree();
}

void NMScriptInspectorPanel::onClearWatchClicked() {
  m_watchExpressions.clear();
  updateWatchTree();
}

void NMScriptInspectorPanel::addWatchExpression(const QString& expression) {
  if (!m_watchExpressions.contains(expression)) {
    m_watchExpressions.append(expression);
    updateWatchTree();
  }
}

void NMScriptInspectorPanel::removeWatchExpression(const QString& expression) {
  m_watchExpressions.removeAll(expression);
  updateWatchTree();
}

void NMScriptInspectorPanel::clearWatchExpressions() {
  m_watchExpressions.clear();
  updateWatchTree();
}

void NMScriptInspectorPanel::editVariable(const QString& name, const QVariant& currentValue) {
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

void NMScriptInspectorPanel::editFlag(const QString& name, bool currentValue) {
  // Simply toggle the flag
  NMPlayModeController::instance().setVariable(name, !currentValue);
}

} // namespace NovelMind::editor::qt
