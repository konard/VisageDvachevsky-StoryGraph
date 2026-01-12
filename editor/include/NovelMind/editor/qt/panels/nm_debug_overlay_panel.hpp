#ifndef NOVELMIND_EDITOR_NM_DEBUG_OVERLAY_PANEL_HPP
#define NOVELMIND_EDITOR_NM_DEBUG_OVERLAY_PANEL_HPP

#include <NovelMind/editor/qt/nm_dock_panel.hpp>
#include <QLabel>
#include <QListWidget>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QVariantList>
#include <QVariantMap>

namespace NovelMind::editor::qt {

/**
 * @brief Display mode for the debug overlay
 */
enum class DebugDisplayMode {
  Minimal, ///< Show only essential info
  Extended ///< Show all debugging information
};

/**
 * @brief Debug overlay panel for runtime inspection
 *
 * Provides tabs for:
 * - Variables (editable during pause)
 * - Call Stack
 * - Current Instruction
 * - Active Animations
 * - Audio Channels
 * - Performance Metrics
 */
class NMDebugOverlayPanel : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMDebugOverlayPanel(QWidget* parent = nullptr);
  ~NMDebugOverlayPanel() override = default;

  void onInitialize() override;
  void onShutdown() override;
  void onUpdate(double deltaTime) override;

  void setDisplayMode(DebugDisplayMode mode);
  DebugDisplayMode displayMode() const { return m_displayMode; }

private slots:
  void onVariablesChanged(const QVariantMap& variables);
  void onFlagsChanged(const QVariantMap& flags);
  void onCallStackChanged(const QStringList& stack);
  void onStackFramesChanged(const QVariantList& frames);
  void onPlayModeChanged(int mode);
  void onCurrentNodeChanged(const QString& nodeId);
  void onExecutionStepChanged(int stepIndex, int totalSteps, const QString& instruction);
  void onVariableItemDoubleClicked(QTreeWidgetItem* item, int column);
  void onDisplayModeChanged();

private:
  void setupUI();
  void setupToolBar();
  void updateVariablesTab(const QVariantMap& variables, const QVariantMap& flags);
  void updateCallStackTab(const QStringList& stack);
  void updateCurrentInstructionTab();
  void updateStackFrames(const QVariantList& frames);
  void editVariable(const QString& name, const QVariant& currentValue);
  void updateTabsVisibility();
  void updatePerformanceMetrics(double deltaTime);

  // UI Elements
  QToolBar* m_toolBar = nullptr;
  QTabWidget* m_tabWidget = nullptr;

  // Variables Tab
  QTreeWidget* m_variablesTree = nullptr;

  // Call Stack Tab
  QListWidget* m_callStackList = nullptr;

  // Current Instruction Tab
  QWidget* m_instructionWidget = nullptr;
  QLabel* m_currentNodeLabel = nullptr;
  QLabel* m_instructionIndexLabel = nullptr;
  QLabel* m_instructionCodeLabel = nullptr;
  QTreeWidget* m_stackFramesTree = nullptr;

  // Animations Tab
  QTreeWidget* m_animationsTree = nullptr;

  // Audio Tab
  QTreeWidget* m_audioTree = nullptr;

  // Performance Tab
  QTreeWidget* m_performanceTree = nullptr;
  QTreeWidgetItem* m_frameTimeItem = nullptr;
  QTreeWidgetItem* m_fpsItem = nullptr;
  QTreeWidgetItem* m_memoryItem = nullptr;
  QTreeWidgetItem* m_objectCountItem = nullptr;
  QTreeWidgetItem* m_instructionRateItem = nullptr;

  // State
  DebugDisplayMode m_displayMode = DebugDisplayMode::Extended;
  QVariantMap m_currentVariables;
  QVariantMap m_currentFlags;
  QStringList m_currentCallStack;
  QVariantList m_currentStackFrames;
  QString m_currentNodeId;
  int m_currentStepIndex = -1;
  int m_totalSteps = 0;
  QString m_currentInstruction;
  double m_lastDeltaTime = 0.0;
};

} // namespace NovelMind::editor::qt

#endif // NOVELMIND_EDITOR_NM_DEBUG_OVERLAY_PANEL_HPP
