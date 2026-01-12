#ifndef NOVELMIND_EDITOR_NM_SCRIPT_RUNTIME_INSPECTOR_PANEL_HPP
#define NOVELMIND_EDITOR_NM_SCRIPT_RUNTIME_INSPECTOR_PANEL_HPP

/**
 * @file nm_script_runtime_inspector_panel.hpp
 * @brief Script Runtime Inspector and Debugger Panel
 *
 * Provides comprehensive debugging capabilities for script execution:
 * - Variable and flag inspection with real-time updates
 * - Breakpoint management with visual indicators
 * - Step debugging controls (Step Into, Over, Out)
 * - Call stack visualization with navigation
 * - Variable change history tracking
 * - Execution state monitoring
 *
 * This panel integrates with the VM Debugger (vm_debugger.hpp) to provide
 * a professional IDE-like debugging experience.
 *
 * Part of Issue #244: Add Script Runtime Inspector and Debugger Panel
 */

#include <NovelMind/editor/qt/nm_dock_panel.hpp>
#include <QLabel>
#include <QListWidget>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QVariantList>
#include <QVariantMap>
#include <memory>

// Forward declarations
namespace NovelMind::scripting {
class VMDebugger;
struct Breakpoint;
struct VariableChangeEvent;
struct CallStackFrame;
} // namespace NovelMind::scripting

namespace NovelMind::editor::qt {

/**
 * @brief Execution state for display purposes
 */
enum class DebugExecutionState {
  Idle,             ///< Not running
  Running,          ///< Running normally
  PausedBreakpoint, ///< Paused at a breakpoint
  PausedStep,       ///< Paused after step
  PausedUser,       ///< Paused by user
  WaitingInput,     ///< Waiting for user input
  Halted            ///< Execution complete
};

/**
 * @brief Script Runtime Inspector Panel
 *
 * A comprehensive debugging panel that provides:
 * - Execution state and control (Play, Pause, Stop, Step)
 * - Variable and flag viewer with editing capability
 * - Call stack display with navigation
 * - Breakpoint management
 * - Variable change history
 * - Performance profiling (basic)
 *
 * Layout:
 * ┌─────────────────────────────────────────────────────────────────┐
 * │ Script Runtime Inspector                                   [x]  │
 * ├──────────────────────────────────────────────────────────────────
 * │ [▶ Continue] [⏸ Pause] [⏭ Step Into] [⏭ Over] [⏭ Out] [⏹ Stop]  │
 * ├──────────────────────────────────────────────────────────────────
 * │ [Variables] [Call Stack] [Breakpoints] [History] [Performance]  │
 * ├──────────────────────────────────────────────────────────────────
 * │  ... tab content ...                                             │
 * └──────────────────────────────────────────────────────────────────┘
 */
class NMScriptRuntimeInspectorPanel : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMScriptRuntimeInspectorPanel(QWidget* parent = nullptr);
  ~NMScriptRuntimeInspectorPanel() override;

  void onInitialize() override;
  void onShutdown() override;
  void onUpdate(double deltaTime) override;

  // =========================================================================
  // External API
  // =========================================================================

  /**
   * @brief Set the debugger instance to monitor
   * @param debugger Pointer to the VM debugger (ownership not transferred)
   */
  void setDebugger(scripting::VMDebugger* debugger);

  /**
   * @brief Get the current debugger
   */
  [[nodiscard]] scripting::VMDebugger* debugger() const { return m_debugger; }

  /**
   * @brief Update execution state display
   */
  void setExecutionState(DebugExecutionState state);

  /**
   * @brief Navigate to a specific source location
   * @param filePath Path to source file
   * @param line Line number to highlight
   */
  void navigateToSource(const QString& filePath, int line);

signals:
  /**
   * @brief Emitted when user requests to navigate to source location
   */
  void sourceNavigationRequested(const QString& filePath, int line);

  /**
   * @brief Emitted when a breakpoint is toggled
   */
  void breakpointToggled(quint32 instructionPointer, bool enabled);

  /**
   * @brief Emitted when user requests continue execution
   */
  void continueRequested();

  /**
   * @brief Emitted when user requests pause
   */
  void pauseRequested();

  /**
   * @brief Emitted when user requests step into
   */
  void stepIntoRequested();

  /**
   * @brief Emitted when user requests step over
   */
  void stepOverRequested();

  /**
   * @brief Emitted when user requests step out
   */
  void stepOutRequested();

  /**
   * @brief Emitted when user requests stop
   */
  void stopRequested();

private slots:
  // Control button handlers
  void onContinueClicked();
  void onPauseClicked();
  void onStepIntoClicked();
  void onStepOverClicked();
  void onStepOutClicked();
  void onStopClicked();

  // Data update handlers (from play mode controller)
  void onVariablesChanged(const QVariantMap& variables);
  void onFlagsChanged(const QVariantMap& flags);
  void onCallStackChanged(const QStringList& stack);
  void onCurrentNodeChanged(const QString& nodeId);
  void onExecutionStepChanged(int stepIndex, int totalSteps, const QString& instruction);
  void onPlayModeChanged(int mode);

  // UI interaction handlers
  void onVariableItemDoubleClicked(QTreeWidgetItem* item, int column);
  void onBreakpointItemDoubleClicked(QTreeWidgetItem* item, int column);
  void onCallStackItemDoubleClicked(QListWidgetItem* item);
  void onHistoryItemClicked(QTreeWidgetItem* item, int column);
  void onAddBreakpointClicked();
  void onRemoveBreakpointClicked();
  void onClearBreakpointsClicked();

private:
  void setupUI();
  void setupToolBar();
  void setupVariablesTab();
  void setupCallStackTab();
  void setupBreakpointsTab();
  void setupHistoryTab();
  void setupPerformanceTab();

  void updateControlsState();
  void updateStatusDisplay();
  void updateVariablesDisplay();
  void updateCallStackDisplay();
  void updateBreakpointsDisplay();
  void updateHistoryDisplay();
  void updatePerformanceMetrics(double deltaTime);

  void editVariable(const QString& name, const QVariant& currentValue);
  QString formatValue(const QVariant& value) const;
  QString getValueTypeString(const QVariant& value) const;
  QString getStateString(DebugExecutionState state) const;
  QIcon getStateIcon(DebugExecutionState state) const;

  // =========================================================================
  // UI Elements
  // =========================================================================

  // Main layout
  QToolBar* m_toolBar = nullptr;
  QTabWidget* m_tabWidget = nullptr;

  // Control buttons
  QToolButton* m_continueBtn = nullptr;
  QToolButton* m_pauseBtn = nullptr;
  QToolButton* m_stepIntoBtn = nullptr;
  QToolButton* m_stepOverBtn = nullptr;
  QToolButton* m_stepOutBtn = nullptr;
  QToolButton* m_stopBtn = nullptr;

  // Status display
  QLabel* m_statusLabel = nullptr;
  QLabel* m_sceneLabel = nullptr;
  QLabel* m_lineLabel = nullptr;

  // Variables Tab
  QTreeWidget* m_variablesTree = nullptr;
  QToolButton* m_addWatchBtn = nullptr;
  QToolButton* m_refreshVarsBtn = nullptr;

  // Call Stack Tab
  QListWidget* m_callStackList = nullptr;

  // Breakpoints Tab
  QTreeWidget* m_breakpointsTree = nullptr;
  QToolButton* m_addBpBtn = nullptr;
  QToolButton* m_removeBpBtn = nullptr;
  QToolButton* m_clearBpsBtn = nullptr;

  // History Tab
  QTreeWidget* m_historyTree = nullptr;
  QToolButton* m_clearHistoryBtn = nullptr;

  // Performance Tab
  QTreeWidget* m_performanceTree = nullptr;
  QTreeWidgetItem* m_frameTimeItem = nullptr;
  QTreeWidgetItem* m_instructionRateItem = nullptr;
  QTreeWidgetItem* m_memoryItem = nullptr;
  QTreeWidgetItem* m_sceneTimeItem = nullptr;

  // =========================================================================
  // State
  // =========================================================================

  scripting::VMDebugger* m_debugger = nullptr;
  DebugExecutionState m_executionState = DebugExecutionState::Idle;

  // Cached data for display
  QVariantMap m_currentVariables;
  QVariantMap m_currentFlags;
  QStringList m_currentCallStack;
  QString m_currentScene;
  QString m_currentInstruction;
  int m_currentLine = 0;
  int m_currentStep = 0;
  int m_totalSteps = 0;

  // Performance tracking
  double m_lastDeltaTime = 0.0;
  int m_instructionCount = 0;
  double m_totalSceneTime = 0.0;
};

} // namespace NovelMind::editor::qt

#endif // NOVELMIND_EDITOR_NM_SCRIPT_RUNTIME_INSPECTOR_PANEL_HPP
