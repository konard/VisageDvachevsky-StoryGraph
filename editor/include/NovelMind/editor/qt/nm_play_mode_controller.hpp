#ifndef NOVELMIND_EDITOR_NM_PLAY_MODE_CONTROLLER_HPP
#define NOVELMIND_EDITOR_NM_PLAY_MODE_CONTROLLER_HPP

#include "NovelMind/editor/editor_runtime_host.hpp"
#include <QElapsedTimer>
#include <QObject>
#include <QSet>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QVariantMap>
#include <limits>

namespace NovelMind::editor::qt {

/**
 * @brief Central controller for Play-In-Editor mode
 *
 * Manages playback state, breakpoints, and runtime integration.
 * Singleton pattern for global access from all panels.
 */
class NMPlayModeController : public QObject {
  Q_OBJECT

public:
  enum PlayMode {
    Stopped, ///< Not running
    Playing, ///< Actively executing
    Paused   ///< Paused at breakpoint or user request
  };
  Q_ENUM(PlayMode)

  /// Get singleton instance
  static NMPlayModeController &instance();

  /// Delete copy/move constructors
  NMPlayModeController(const NMPlayModeController &) = delete;
  NMPlayModeController &operator=(const NMPlayModeController &) = delete;

  // === Playback Control ===

  /// Start or resume playback
  void play();

  /// Pause playback (can be resumed)
  void pause();

  /// Stop playback and reset state
  void stop();

  /// Execute one instruction (only works when paused)
  void stepForward();

  /// Step backward one instruction (when possible)
  void stepBackward();

  /// Step over (execute until next node)
  void stepOver();

  /// Step out (execute until leaving current scene)
  void stepOut();

  /// Play from a specific node (useful for testing)
  void playFromNode(const QString &nodeId);

  /// Jump to a specific node without resetting state
  void jumpToNode(const QString &nodeId);

  /// Select a choice while waiting
  void selectChoice(int index);

  /// Advance dialogue when waiting for input
  void advanceDialogue();

  /// Save runtime state to slot
  bool saveSlot(int slot);

  /// Load runtime state from slot
  bool loadSlot(int slot);

  /// Save runtime state to auto-save
  bool saveAuto();

  /// Load runtime state from auto-save
  bool loadAuto();

  /// Check if auto-save exists
  bool hasAutoSave() const;

  /// Get access to the script runtime (for advanced operations)
  scripting::ScriptRuntime *getScriptRuntime();

  /// Load a project into runtime
  bool loadProject(const QString &projectPath,
                   const QString &scriptsPath = QString(),
                   const QString &assetsPath = QString(),
                   const QString &startScene = QString());

  /// Load current project from ProjectManager (if available)
  bool loadCurrentProject();

  /// Release runtime resources before application shutdown
  void shutdown();

  // === State Queries ===

  /// Get current play mode
  PlayMode playMode() const { return m_playMode; }

  /// Check if currently playing
  bool isPlaying() const { return m_playMode == Playing; }

  /// Check if currently paused
  bool isPaused() const { return m_playMode == Paused; }

  /// Check if stopped
  bool isStopped() const { return m_playMode == Stopped; }

  /// Get current executing node ID
  QString currentNodeId() const { return m_currentNodeId; }
  int currentStepIndex() const { return m_lastStepIndex; }
  int totalSteps() const { return m_totalSteps; }
  QString currentInstructionText() const { return m_currentInstruction; }
  QString currentSpeaker() const { return m_currentSpeaker; }
  QString currentDialogue() const { return m_currentDialogue; }
  QStringList currentChoices() const { return m_currentChoices; }
  bool isWaitingForChoice() const { return m_waitingForChoice; }
  const SceneSnapshot &lastSnapshot() const { return m_lastSnapshot; }
  bool isRuntimeLoaded() const { return m_runtimeLoaded; }

  // === Breakpoint Management ===

  /// Toggle breakpoint on/off for a node
  void toggleBreakpoint(const QString &nodeId);

  /// Set breakpoint state explicitly
  void setBreakpoint(const QString &nodeId, bool enabled);

  /// Check if node has a breakpoint
  bool hasBreakpoint(const QString &nodeId) const;

  /// Get all breakpoint node IDs
  QSet<QString> breakpoints() const { return m_breakpoints; }

  /// Clear all breakpoints
  void clearAllBreakpoints();

  // === Source-Level Breakpoints (file:line) ===

  /**
   * @brief Represents a source-level breakpoint
   */
  struct SourceBreakpoint {
    QString filePath;  ///< Absolute path to script file
    int line = 0;      ///< Line number (1-based)
    bool enabled = true;
    QString condition; ///< Optional conditional expression
  };

  /**
   * @brief Toggle a source-level breakpoint
   * @param filePath The script file path
   * @param line The line number (1-based)
   */
  void toggleSourceBreakpoint(const QString &filePath, int line);

  /**
   * @brief Set a source-level breakpoint
   * @param filePath The script file path
   * @param line The line number (1-based)
   * @param enabled Whether the breakpoint is enabled
   */
  void setSourceBreakpoint(const QString &filePath, int line, bool enabled);

  /**
   * @brief Check if a source-level breakpoint exists
   * @param filePath The script file path
   * @param line The line number (1-based)
   * @return true if breakpoint exists and is enabled
   */
  bool hasSourceBreakpoint(const QString &filePath, int line) const;

  /**
   * @brief Get all source breakpoints for a file
   * @param filePath The script file path
   * @return Set of line numbers with breakpoints
   */
  QSet<int> sourceBreakpointsForFile(const QString &filePath) const;

  /**
   * @brief Get all source breakpoints
   * @return List of all source breakpoints
   */
  QList<SourceBreakpoint> allSourceBreakpoints() const;

  /**
   * @brief Clear all source-level breakpoints
   */
  void clearAllSourceBreakpoints();

  /**
   * @brief Clear source-level breakpoints for a specific file
   */
  void clearSourceBreakpointsForFile(const QString &filePath);

  // === Variable Inspection ===

  /// Get all current runtime variables
  QVariantMap currentVariables() const { return m_variables; }

  /// Set a variable value (only works when paused)
  void setVariable(const QString &name, const QVariant &value);

  /// Get current call stack
  QStringList callStack() const { return m_callStack; }
  QVariantList currentStackFrames() const { return m_stackFrames; }
  QVariantMap currentFlags() const { return m_flags; }

signals:
  // === Playback Signals ===

  /// Emitted when play mode changes
  void playModeChanged(PlayMode mode);
  void projectLoaded(const QString &projectPath);

  /// Emitted when a breakpoint is hit
  void breakpointHit(const QString &nodeId);

  /// Emitted when execution reaches a new node
  void currentNodeChanged(const QString &nodeId);

  /// Emitted when dialogue line changes
  void dialogueLineChanged(const QString &speaker, const QString &text);

  /// Emitted when available choices change
  void choicesChanged(const QStringList &choices);

  /// Emitted when a fresh scene snapshot is available
  void sceneSnapshotUpdated();

  /// Emitted when the execution step advances
  void executionStepChanged(int stepIndex, int totalSteps,
                            const QString &instruction);

  // === Data Signals ===

  /// Emitted when variables are updated
  void variablesChanged(const QVariantMap &variables);
  void flagsChanged(const QVariantMap &flags);

  /// Emitted when call stack changes
  void callStackChanged(const QStringList &stack);
  void stackFramesChanged(const QVariantList &frames);

  // === Breakpoint Signals ===

  /// Emitted when breakpoints are modified
  void breakpointsChanged();

  /// Emitted when source-level breakpoints are modified
  void sourceBreakpointsChanged();

  /// Emitted when a source-level breakpoint is hit
  void sourceBreakpointHit(const QString &filePath, int line);

public slots:
  /// Load breakpoints from project settings
  void loadBreakpoints(const QString &projectPath);

  /// Save breakpoints to project settings
  void saveBreakpoints(const QString &projectPath);

private:
  explicit NMPlayModeController(QObject *parent = nullptr);
  ~NMPlayModeController() override = default;

  /// Simulate one execution step
  void simulateStep();

  /// Ensure runtime project is loaded with current start scene
  bool ensureRuntimeLoaded();

  /// Check if current node has breakpoint and pause if needed
  void checkBreakpoint();

  /// Sync cached UI data from runtime
  void refreshRuntimeCache();

  // === State ===
  PlayMode m_playMode = Stopped;
  QString m_currentNodeId;
  QSet<QString> m_breakpoints;
  QHash<QString, QSet<int>> m_sourceBreakpoints; // filePath -> set of lines
  QVariantMap m_variables;
  QVariantMap m_flags;
  QStringList m_callStack;
  QVariantList m_stackFrames;
  QString m_currentSpeaker;
  QString m_currentDialogue;
  QStringList m_currentChoices;
  bool m_waitingForChoice = false;
  QString m_currentInstruction;
  int m_lastStepIndex = -1;
  int m_totalSteps = 0;
  bool m_runtimeLoaded = false;
  SceneSnapshot m_lastSnapshot{};

  // Runtime host and ticking
  EditorRuntimeHost m_runtimeHost;
  QTimer *m_runtimeTimer = nullptr;
  QElapsedTimer m_deltaTimer;
};

} // namespace NovelMind::editor::qt

#endif // NOVELMIND_EDITOR_NM_PLAY_MODE_CONTROLLER_HPP
