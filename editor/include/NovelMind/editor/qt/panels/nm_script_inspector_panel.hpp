#ifndef NOVELMIND_EDITOR_NM_SCRIPT_INSPECTOR_PANEL_HPP
#define NOVELMIND_EDITOR_NM_SCRIPT_INSPECTOR_PANEL_HPP

/**
 * @file nm_script_inspector_panel.hpp
 * @brief Script Inspector Panel for NMS debugging
 *
 * Provides comprehensive runtime inspection during script execution:
 * - Variable values (global, local)
 * - Flag states (boolean story flags)
 * - Watch expressions (custom expressions to evaluate)
 * - Scene history (execution path/call stack)
 *
 * This panel integrates with EditorRuntimeHost and NMPlayModeController
 * to provide real-time debugging capabilities as defined in issue #235.
 *
 * @see NMDebugOverlayPanel for a simpler debug view
 * @see NMPlayModeController for playback control
 */

#include <NovelMind/editor/qt/nm_dock_panel.hpp>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QVariantMap>

namespace NovelMind::editor::qt {

/**
 * @brief Watch expression result
 */
struct WatchExpressionResult {
  QString expression;
  QString result;
  bool isValid = false;
  bool isBoolean = false;
};

/**
 * @brief Script Inspector Panel for comprehensive debugging
 *
 * Features:
 * - Variables tab: Display and edit all runtime variables
 * - Flags tab: Display all boolean flags
 * - Watch tab: User-defined watch expressions
 * - Scene History tab: Execution path showing visited scenes
 *
 * Example usage:
 * @code
 * // Variables are automatically updated when runtime changes
 * // Watch expressions can be added via UI:
 * inspector->addWatchExpression("points >= 100");
 * inspector->addWatchExpression("health < 50");
 *
 * // Results appear in the Watch tab with live evaluation
 * @endcode
 */
class NMScriptInspectorPanel : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMScriptInspectorPanel(QWidget *parent = nullptr);
  ~NMScriptInspectorPanel() override = default;

  void onInitialize() override;
  void onShutdown() override;
  void onUpdate(double deltaTime) override;

  // === Watch Expressions ===

  /**
   * @brief Add a watch expression to monitor
   * @param expression The expression to watch (e.g., "points >= 100")
   */
  void addWatchExpression(const QString &expression);

  /**
   * @brief Remove a watch expression
   * @param expression The expression to remove
   */
  void removeWatchExpression(const QString &expression);

  /**
   * @brief Clear all watch expressions
   */
  void clearWatchExpressions();

  /**
   * @brief Get all watch expressions
   */
  [[nodiscard]] QStringList watchExpressions() const { return m_watchExpressions; }

signals:
  /**
   * @brief Emitted when user clicks on a scene in history to navigate
   */
  void navigateToSceneRequested(const QString &sceneId);

  /**
   * @brief Emitted when user requests to jump to variable definition
   */
  void navigateToVariableDefinition(const QString &variableName,
                                    const QString &scriptPath, int line);

private slots:
  // Controller signals
  void onVariablesChanged(const QVariantMap &variables);
  void onFlagsChanged(const QVariantMap &flags);
  void onCallStackChanged(const QStringList &stack);
  void onStackFramesChanged(const QVariantList &frames);
  void onPlayModeChanged(int mode);
  void onCurrentNodeChanged(const QString &nodeId);

  // UI interactions
  void onVariableItemDoubleClicked(QTreeWidgetItem *item, int column);
  void onFlagItemDoubleClicked(QTreeWidgetItem *item, int column);
  void onWatchItemDoubleClicked(QTreeWidgetItem *item, int column);
  void onSceneHistoryItemDoubleClicked(QListWidgetItem *item);
  void onAddWatchClicked();
  void onRemoveWatchClicked();
  void onClearWatchClicked();

private:
  void setupUI();
  void setupToolBar();
  void setupVariablesTab();
  void setupFlagsTab();
  void setupWatchTab();
  void setupSceneHistoryTab();

  void updateVariablesTree();
  void updateFlagsTree();
  void updateWatchTree();
  void updateSceneHistoryList();

  void editVariable(const QString &name, const QVariant &currentValue);
  void editFlag(const QString &name, bool currentValue);

  /**
   * @brief Evaluate a watch expression against current runtime state
   * @param expression The expression to evaluate
   * @return Result with value and validity
   */
  WatchExpressionResult evaluateExpression(const QString &expression) const;

  /**
   * @brief Parse a simple expression (supports: comparisons, logical ops)
   */
  WatchExpressionResult parseSimpleExpression(const QString &expression) const;

  // UI Elements
  QToolBar *m_toolBar = nullptr;
  QTabWidget *m_tabWidget = nullptr;

  // Variables Tab
  QWidget *m_variablesWidget = nullptr;
  QTreeWidget *m_variablesTree = nullptr;
  QLineEdit *m_variablesFilter = nullptr;

  // Flags Tab
  QWidget *m_flagsWidget = nullptr;
  QTreeWidget *m_flagsTree = nullptr;
  QLineEdit *m_flagsFilter = nullptr;

  // Watch Tab
  QWidget *m_watchWidget = nullptr;
  QTreeWidget *m_watchTree = nullptr;
  QLineEdit *m_watchInput = nullptr;
  QPushButton *m_addWatchBtn = nullptr;
  QPushButton *m_removeWatchBtn = nullptr;
  QPushButton *m_clearWatchBtn = nullptr;

  // Scene History Tab
  QWidget *m_sceneHistoryWidget = nullptr;
  QListWidget *m_sceneHistoryList = nullptr;
  QLabel *m_currentSceneLabel = nullptr;

  // State
  QVariantMap m_currentVariables;
  QVariantMap m_currentFlags;
  QStringList m_currentCallStack;
  QVariantList m_currentStackFrames;
  QString m_currentNodeId;
  QString m_currentSceneId;
  QStringList m_watchExpressions;
  QStringList m_sceneHistory; // Ordered list of visited scenes
};

} // namespace NovelMind::editor::qt

#endif // NOVELMIND_EDITOR_NM_SCRIPT_INSPECTOR_PANEL_HPP
