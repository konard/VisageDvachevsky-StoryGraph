#pragma once

/**
 * @file nm_scene_preview_widget.hpp
 * @brief Live scene preview widget for the script editor
 *
 * Provides real-time visual preview of scene state as scripts are edited:
 * - Displays backgrounds, characters, and UI elements
 * - Shows dialogue boxes with proper styling
 * - Updates automatically as script changes (debounced)
 * - Executes script commands up to cursor position
 * - Reuses existing SceneView rendering components
 *
 * This implements Phase 1 of issue #240: Basic live scene preview.
 */

#include "NovelMind/editor/qt/nm_dock_panel.hpp"
#include "NovelMind/editor/editor_runtime_host.hpp"
#include <QFrame>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>

namespace NovelMind::editor::qt {

// Forward declarations
class NMSceneGraphicsScene;
class NMSceneGraphicsView;
class NMPlayPreviewOverlay;

/**
 * @brief Preview status indicator
 */
enum class PreviewStatus {
  Idle,      // No preview active
  Compiling, // Compiling script
  Rendering, // Rendering scene
  Ready,     // Preview ready and displayed
  Error      // Compilation or rendering error
};

/**
 * @brief Scene state extracted from script at cursor position
 */
struct ScenePreviewState {
  std::string currentScene;
  std::string backgroundAsset;
  std::vector<std::pair<std::string, std::string>> characters; // id, position
  std::string dialogueSpeaker;
  std::string dialogueText;
  std::vector<std::string> choices;
  bool hasDialogue = false;
  bool hasChoices = false;
  bool isValid = false;
  std::string errorMessage;
};

/**
 * @brief Live scene preview widget
 *
 * This widget provides a real-time preview of the visual novel scene
 * based on the current script content and cursor position. It:
 *
 * 1. Monitors script changes with debounced updates (300ms)
 * 2. Compiles only the current scene incrementally
 * 3. Executes commands up to the cursor line
 * 4. Renders the resulting scene state
 * 5. Shows dialogue overlay when applicable
 *
 * Architecture:
 * ┌──────────────────────────────────────┐
 * │  Script Editor (text changes)        │
 * └────────────┬─────────────────────────┘
 *              │ textChanged signal
 *              ▼
 * ┌──────────────────────────────────────┐
 * │  NMScenePreviewWidget                │
 * │  ┌────────────────────────────────┐  │
 * │  │ Update Timer (300ms debounce)  │  │
 * │  └────────┬───────────────────────┘  │
 * │           ▼                           │
 * │  ┌────────────────────────────────┐  │
 * │  │ Incremental Compiler           │  │
 * │  │ - Parse current scene only     │  │
 * │  │ - Execute to cursor line       │  │
 * │  └────────┬───────────────────────┘  │
 * │           ▼                           │
 * │  ┌────────────────────────────────┐  │
 * │  │ Scene State Extractor          │  │
 * │  └────────┬───────────────────────┘  │
 * │           ▼                           │
 * │  ┌────────────────────────────────┐  │
 * │  │ NMSceneGraphicsView            │  │
 * │  │ (reuses Scene View rendering)  │  │
 * │  └────────────────────────────────┘  │
 * │  ┌────────────────────────────────┐  │
 * │  │ NMPlayPreviewOverlay           │  │
 * │  │ (shows dialogue)               │  │
 * │  └────────────────────────────────┘  │
 * └──────────────────────────────────────┘
 */
class NMScenePreviewWidget : public QWidget {
  Q_OBJECT

public:
  explicit NMScenePreviewWidget(QWidget* parent = nullptr);
  ~NMScenePreviewWidget() override;

  /**
   * @brief Set the script content to preview
   * @param scriptContent Full script text
   * @param cursorLine Current cursor line (1-based)
   * @param cursorColumn Current cursor column (0-based)
   */
  void setScriptContent(const QString& scriptContent, int cursorLine, int cursorColumn);

  /**
   * @brief Update preview with new script content
   *
   * This triggers a debounced update (300ms) to avoid excessive recompilation
   * while the user is typing.
   */
  void requestUpdate();

  /**
   * @brief Force immediate update (bypasses debounce timer)
   */
  void updateImmediately();

  /**
   * @brief Clear the preview
   */
  void clearPreview();

  /**
   * @brief Set the project assets root path for loading textures
   */
  void setAssetsRoot(const QString& path);

  /**
   * @brief Get current preview status
   */
  [[nodiscard]] PreviewStatus getStatus() const { return m_status; }

  /**
   * @brief Check if preview is enabled
   */
  [[nodiscard]] bool isPreviewEnabled() const { return m_previewEnabled; }

  /**
   * @brief Enable or disable the preview
   */
  void setPreviewEnabled(bool enabled);

  /**
   * @brief Set debounce delay in milliseconds
   * @param delay Delay in milliseconds (default: 300ms)
   */
  void setDebounceDelay(int delay);

signals:
  /**
   * @brief Emitted when preview status changes
   */
  void statusChanged(PreviewStatus status);

  /**
   * @brief Emitted when a compilation error occurs
   */
  void compilationError(const QString& message, int line, int column);

  /**
   * @brief Emitted when preview is successfully rendered
   */
  void previewRendered();

private slots:
  void onUpdateTimerTimeout();
  void onTogglePreviewClicked();
  void onResetViewClicked();
  void onToggleGridClicked();

private:
  void setupUI();
  void setupConnections();
  void updatePreview();
  void setStatus(PreviewStatus status);
  void showStatusMessage(const QString& message);

  /**
   * @brief Parse and compile the script incrementally
   * @return Extracted scene state or error
   */
  ScenePreviewState compileScriptAtCursor();

  /**
   * @brief Apply the scene state to the graphics scene
   */
  void applySceneState(const ScenePreviewState& state);

  /**
   * @brief Load and display background asset
   */
  bool loadBackground(const QString& assetPath);

  /**
   * @brief Load and display character sprite
   */
  bool loadCharacter(const QString& characterId, const QString& position);

  /**
   * @brief Update dialogue overlay
   */
  void updateDialogueOverlay(const QString& speaker, const QString& text);

  /**
   * @brief Update choices display
   */
  void updateChoicesOverlay(const QStringList& choices);

  /**
   * @brief Clear all scene objects
   */
  void clearScene();

  /**
   * @brief Get position coordinates from position name (left, right, center)
   */
  QPointF getPositionCoordinates(const QString& position) const;

  // UI Components
  QVBoxLayout* m_layout = nullptr;
  QFrame* m_toolbarFrame = nullptr;
  QPushButton* m_togglePreviewBtn = nullptr;
  QPushButton* m_resetViewBtn = nullptr;
  QPushButton* m_toggleGridBtn = nullptr;
  QLabel* m_statusLabel = nullptr;

  NMSceneGraphicsScene* m_scene = nullptr;
  NMSceneGraphicsView* m_view = nullptr;
  NMPlayPreviewOverlay* m_overlay = nullptr;

  QFrame* m_placeholderFrame = nullptr;
  QLabel* m_placeholderLabel = nullptr;

  // Script content
  QString m_scriptContent;
  int m_cursorLine = 1;
  int m_cursorColumn = 0;
  QString m_assetsRoot;

  // Update control
  QTimer* m_updateTimer = nullptr;
  int m_debounceDelay = 300; // milliseconds
  bool m_updatePending = false;

  // State
  PreviewStatus m_status = PreviewStatus::Idle;
  bool m_previewEnabled = true;
  bool m_gridVisible = false;
  ScenePreviewState m_currentState;

  // Scene objects tracking (for cleanup)
  QHash<QString, QGraphicsItem*> m_sceneObjects;
};

} // namespace NovelMind::editor::qt
