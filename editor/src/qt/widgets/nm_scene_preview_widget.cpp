#include "NovelMind/editor/qt/widgets/nm_scene_preview_widget.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_overlays.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"
#include "NovelMind/scripting/validator.hpp"
#include <QApplication>
#include <QDebug>
#include <QGraphicsPixmapItem>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPixmap>
#include <QToolButton>

namespace NovelMind::editor::qt {

NMScenePreviewWidget::NMScenePreviewWidget(QWidget *parent)
    : QWidget(parent) {
  setupUI();
  setupConnections();

  // Initialize debounce timer
  m_updateTimer = new QTimer(this);
  m_updateTimer->setSingleShot(true);
  connect(m_updateTimer, &QTimer::timeout, this,
          &NMScenePreviewWidget::onUpdateTimerTimeout);

  setStatus(PreviewStatus::Idle);
}

NMScenePreviewWidget::~NMScenePreviewWidget() = default;

void NMScenePreviewWidget::setupUI() {
  m_layout = new QVBoxLayout(this);
  m_layout->setContentsMargins(0, 0, 0, 0);
  m_layout->setSpacing(0);

  // Toolbar
  m_toolbarFrame = new QFrame(this);
  m_toolbarFrame->setObjectName("PreviewToolbar");
  m_toolbarFrame->setFixedHeight(36);
  auto *toolbarLayout = new QHBoxLayout(m_toolbarFrame);
  toolbarLayout->setContentsMargins(8, 4, 8, 4);
  toolbarLayout->setSpacing(6);

  // Preview toggle button
  m_togglePreviewBtn = new QPushButton(tr("👁️ Preview"), m_toolbarFrame);
  m_togglePreviewBtn->setCheckable(true);
  m_togglePreviewBtn->setChecked(m_previewEnabled);
  m_togglePreviewBtn->setToolTip(tr("Toggle live preview (Ctrl+Shift+V)"));
  m_togglePreviewBtn->setMinimumWidth(90);
  toolbarLayout->addWidget(m_togglePreviewBtn);

  // Reset view button
  m_resetViewBtn = new QPushButton(tr("Reset View"), m_toolbarFrame);
  m_resetViewBtn->setToolTip(tr("Reset camera to default position"));
  toolbarLayout->addWidget(m_resetViewBtn);

  // Grid toggle button
  m_toggleGridBtn = new QPushButton(tr("Grid"), m_toolbarFrame);
  m_toggleGridBtn->setCheckable(true);
  m_toggleGridBtn->setChecked(m_gridVisible);
  m_toggleGridBtn->setToolTip(tr("Toggle grid overlay"));
  toolbarLayout->addWidget(m_toggleGridBtn);

  toolbarLayout->addStretch();

  // Status label
  m_statusLabel = new QLabel(tr("Preview idle"), m_toolbarFrame);
  m_statusLabel->setObjectName("PreviewStatus");
  toolbarLayout->addWidget(m_statusLabel);

  m_layout->addWidget(m_toolbarFrame);

  // Graphics view for scene rendering
  m_scene = new NMSceneGraphicsScene(this);
  m_view = new NMSceneGraphicsView(this);
  m_view->setScene(m_scene);
  m_view->setObjectName("PreviewView");
  m_view->setRenderHint(QPainter::Antialiasing);
  m_view->setRenderHint(QPainter::SmoothPixmapTransform);
  m_view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  m_view->centerOnScene();

  m_layout->addWidget(m_view, 1);

  // Dialogue overlay
  m_overlay = new NMPlayPreviewOverlay(m_view);
  m_overlay->setGeometry(m_view->rect());
  m_overlay->hide();

  // Placeholder when preview is disabled
  m_placeholderFrame = new QFrame(this);
  m_placeholderFrame->setObjectName("PreviewPlaceholder");
  auto *placeholderLayout = new QVBoxLayout(m_placeholderFrame);
  m_placeholderLabel = new QLabel(
      tr("👁️ Live Preview\n\nEnable preview to see your scene as you type"),
      m_placeholderFrame);
  m_placeholderLabel->setAlignment(Qt::AlignCenter);
  QFont placeholderFont = m_placeholderLabel->font();
  placeholderFont.setPointSize(14);
  m_placeholderLabel->setFont(placeholderFont);
  placeholderLayout->addWidget(m_placeholderLabel);
  m_layout->addWidget(m_placeholderFrame, 1);
  m_placeholderFrame->hide();

  // Apply styling
  const auto &palette = NMStyleManager::instance().palette();
  setStyleSheet(QString(R"(
    #PreviewToolbar {
      background: %1;
      border-bottom: 1px solid %2;
    }
    #PreviewStatus {
      color: %3;
      font-size: 11px;
    }
    #PreviewView {
      background: %4;
      border: none;
    }
    #PreviewPlaceholder {
      background: %4;
      color: %5;
    }
  )")
                    .arg(palette["panel.background"])
                    .arg(palette["panel.border"])
                    .arg(palette["text.muted"])
                    .arg(palette["editor.background"])
                    .arg(palette["text.muted"]));
}

void NMScenePreviewWidget::setupConnections() {
  connect(m_togglePreviewBtn, &QPushButton::clicked, this,
          &NMScenePreviewWidget::onTogglePreviewClicked);
  connect(m_resetViewBtn, &QPushButton::clicked, this,
          &NMScenePreviewWidget::onResetViewClicked);
  connect(m_toggleGridBtn, &QPushButton::clicked, this,
          &NMScenePreviewWidget::onToggleGridClicked);
}

void NMScenePreviewWidget::setScriptContent(const QString &scriptContent,
                                             int cursorLine, int cursorColumn) {
  m_scriptContent = scriptContent;
  m_cursorLine = cursorLine;
  m_cursorColumn = cursorColumn;
}

void NMScenePreviewWidget::requestUpdate() {
  if (!m_previewEnabled) {
    return;
  }

  // Stop any pending update
  m_updateTimer->stop();

  // Start debounce timer
  m_updatePending = true;
  m_updateTimer->start(m_debounceDelay);
}

void NMScenePreviewWidget::updateImmediately() {
  if (!m_previewEnabled) {
    return;
  }

  m_updateTimer->stop();
  m_updatePending = false;
  updatePreview();
}

void NMScenePreviewWidget::clearPreview() {
  clearScene();
  m_overlay->hide();
  setStatus(PreviewStatus::Idle);
  m_currentState = ScenePreviewState();
}

void NMScenePreviewWidget::setAssetsRoot(const QString &path) {
  m_assetsRoot = path;
}

void NMScenePreviewWidget::setPreviewEnabled(bool enabled) {
  if (m_previewEnabled == enabled) {
    return;
  }

  m_previewEnabled = enabled;
  m_togglePreviewBtn->setChecked(enabled);

  if (enabled) {
    m_view->show();
    m_placeholderFrame->hide();
    m_overlay->show();
    requestUpdate();
  } else {
    m_view->hide();
    m_overlay->hide();
    m_placeholderFrame->show();
    clearPreview();
  }
}

void NMScenePreviewWidget::setDebounceDelay(int delay) {
  m_debounceDelay = qMax(0, delay);
}

void NMScenePreviewWidget::onUpdateTimerTimeout() {
  m_updatePending = false;
  updatePreview();
}

void NMScenePreviewWidget::onTogglePreviewClicked() {
  setPreviewEnabled(m_togglePreviewBtn->isChecked());
}

void NMScenePreviewWidget::onResetViewClicked() {
  if (m_view) {
    m_view->centerOnScene();
    m_view->setZoomLevel(1.0);
  }
}

void NMScenePreviewWidget::onToggleGridClicked() {
  m_gridVisible = m_toggleGridBtn->isChecked();
  if (m_scene) {
    m_scene->setGridVisible(m_gridVisible);
    m_scene->update();
  }
}

void NMScenePreviewWidget::updatePreview() {
  if (m_scriptContent.isEmpty()) {
    clearPreview();
    showStatusMessage(tr("No script content"));
    return;
  }

  setStatus(PreviewStatus::Compiling);
  showStatusMessage(tr("⏳ Compiling..."));

  // Compile script at cursor position
  ScenePreviewState state = compileScriptAtCursor();

  if (!state.isValid) {
    setStatus(PreviewStatus::Error);
    showStatusMessage(tr("❌ Error: %1").arg(QString::fromStdString(state.errorMessage)));
    emit compilationError(QString::fromStdString(state.errorMessage), m_cursorLine, m_cursorColumn);
    return;
  }

  setStatus(PreviewStatus::Rendering);
  showStatusMessage(tr("🎨 Rendering..."));

  // Apply scene state
  applySceneState(state);

  setStatus(PreviewStatus::Ready);
  showStatusMessage(tr("✓ Preview ready"));
  emit previewRendered();

  m_currentState = state;
}

void NMScenePreviewWidget::setStatus(PreviewStatus status) {
  if (m_status == status) {
    return;
  }

  m_status = status;
  emit statusChanged(status);
}

void NMScenePreviewWidget::showStatusMessage(const QString &message) {
  m_statusLabel->setText(message);
}

ScenePreviewState NMScenePreviewWidget::compileScriptAtCursor() {
  ScenePreviewState state;
  state.isValid = false;

  // Convert QString to std::string for the lexer
  std::string scriptText = m_scriptContent.toStdString();

  // Lex the script
  scripting::Lexer lexer(scriptText);
  auto tokens = lexer.tokenize();

  if (lexer.hasErrors()) {
    const auto &errors = lexer.getErrors();
    if (!errors.empty()) {
      state.errorMessage = errors[0].message;
    } else {
      state.errorMessage = "Lexer error";
    }
    return state;
  }

  // Parse the script
  scripting::Parser parser(tokens);
  auto program = parser.parse();

  if (parser.hasErrors()) {
    const auto &errors = parser.getErrors();
    if (!errors.empty()) {
      state.errorMessage = errors[0].message;
    } else {
      state.errorMessage = "Parser error";
    }
    return state;
  }

  if (!program) {
    state.errorMessage = "Failed to parse program";
    return state;
  }

  // Find the scene definition containing the cursor line
  std::string currentSceneName;
  const scripting::SceneDefinition *currentSceneDef = nullptr;

  for (const auto &scene : program->scenes) {
    if (scene.location.line <= static_cast<u32>(m_cursorLine)) {
      // Check if cursor is within this scene's range
      // For simplicity, we assume scenes are sequential
      currentSceneName = scene.name;
      currentSceneDef = &scene;
    }
  }

  if (!currentSceneDef) {
    // No scene found, return empty but valid state
    state.isValid = true;
    return state;
  }

  state.currentScene = currentSceneName;

  // Extract scene state by simulating execution up to cursor line
  // This is a simplified version - Phase 1 implementation
  for (const auto &stmt : currentSceneDef->body) {
    // Only process statements before cursor line
    if (stmt->location.line > static_cast<u32>(m_cursorLine)) {
      break;
    }

    // Handle different statement types
    if (auto *showStmt = dynamic_cast<const scripting::ShowStatement *>(stmt.get())) {
      if (showStmt->objectType == "background" || showStmt->objectType == "bg") {
        state.backgroundAsset = showStmt->objectId;
      } else if (showStmt->objectType == "character") {
        std::string position = showStmt->position.value_or("center");
        state.characters.push_back({showStmt->objectId, position});
      }
    } else if (auto *sayStmt = dynamic_cast<const scripting::SayStatement *>(stmt.get())) {
      state.dialogueSpeaker = sayStmt->speaker.value_or("Narrator");
      state.dialogueText = sayStmt->text;
      state.hasDialogue = true;
    } else if (auto *choiceStmt = dynamic_cast<const scripting::ChoiceStatement *>(stmt.get())) {
      state.hasChoices = true;
      for (const auto &option : choiceStmt->options) {
        state.choices.push_back(option.text);
      }
    }
  }

  state.isValid = true;
  return state;
}

void NMScenePreviewWidget::applySceneState(const ScenePreviewState &state) {
  // Clear previous scene
  clearScene();

  // Load background
  if (!state.backgroundAsset.empty()) {
    loadBackground(QString::fromStdString(state.backgroundAsset));
  }

  // Load characters
  for (const auto &[characterId, position] : state.characters) {
    loadCharacter(QString::fromStdString(characterId),
                  QString::fromStdString(position));
  }

  // Update dialogue overlay
  if (state.hasDialogue) {
    updateDialogueOverlay(QString::fromStdString(state.dialogueSpeaker),
                          QString::fromStdString(state.dialogueText));
    m_overlay->show();
  } else {
    m_overlay->hide();
  }

  // Update choices overlay
  if (state.hasChoices) {
    QStringList choicesList;
    for (const auto &choice : state.choices) {
      choicesList << QString::fromStdString(choice);
    }
    updateChoicesOverlay(choicesList);
  }

  // Ensure view is centered and visible
  m_view->centerOnScene();
}

bool NMScenePreviewWidget::loadBackground(const QString &assetPath) {
  // Construct full asset path
  QString fullPath = m_assetsRoot + "/backgrounds/" + assetPath;

  // Support common image formats
  QStringList extensions = {".png", ".jpg", ".jpeg", ".bmp"};
  QString foundPath;

  for (const QString &ext : extensions) {
    QString testPath = fullPath + ext;
    if (QFile::exists(testPath)) {
      foundPath = testPath;
      break;
    }
  }

  if (foundPath.isEmpty() && !fullPath.endsWith(".png") &&
      !fullPath.endsWith(".jpg") && !fullPath.endsWith(".jpeg")) {
    foundPath = fullPath + ".png"; // Default to PNG
  }

  QPixmap pixmap(foundPath);
  if (pixmap.isNull()) {
    qWarning() << "Failed to load background:" << foundPath;
    return false;
  }

  // Scale to scene size (1280x720 is standard VN resolution)
  pixmap = pixmap.scaled(1280, 720, Qt::IgnoreAspectRatio,
                         Qt::SmoothTransformation);

  auto *bgItem = new QGraphicsPixmapItem(pixmap);
  bgItem->setPos(0, 0);
  bgItem->setZValue(-100); // Behind all other objects
  m_scene->addItem(bgItem);
  m_sceneObjects["background"] = bgItem;

  return true;
}

bool NMScenePreviewWidget::loadCharacter(const QString &characterId,
                                          const QString &position) {
  // Construct full asset path
  QString fullPath =
      m_assetsRoot + "/characters/" + characterId + "/default.png";

  // Try alternative paths
  if (!QFile::exists(fullPath)) {
    fullPath = m_assetsRoot + "/characters/" + characterId + ".png";
  }

  QPixmap pixmap(fullPath);
  if (pixmap.isNull()) {
    qWarning() << "Failed to load character:" << fullPath;
    // Create placeholder
    pixmap = QPixmap(200, 400);
    pixmap.fill(Qt::gray);
  }

  // Scale character sprite (typical height ~500px)
  if (pixmap.height() > 500) {
    pixmap = pixmap.scaledToHeight(500, Qt::SmoothTransformation);
  }

  auto *charItem = new QGraphicsPixmapItem(pixmap);
  QPointF pos = getPositionCoordinates(position);
  charItem->setPos(pos);
  charItem->setZValue(0); // Above background
  m_scene->addItem(charItem);
  m_sceneObjects[characterId] = charItem;

  return true;
}

void NMScenePreviewWidget::updateDialogueOverlay(const QString &speaker,
                                                  const QString &text) {
  if (m_overlay) {
    m_overlay->setDialogue(speaker, text);
    m_overlay->show();
  }
}

void NMScenePreviewWidget::updateChoicesOverlay(const QStringList &choices) {
  if (m_overlay) {
    m_overlay->setChoices(choices);
  }
}

void NMScenePreviewWidget::clearScene() {
  // Remove all scene objects
  for (auto *item : m_sceneObjects.values()) {
    if (item) {
      m_scene->removeItem(item);
      delete item;
    }
  }
  m_sceneObjects.clear();
}

QPointF
NMScenePreviewWidget::getPositionCoordinates(const QString &position) const {
  // Standard VN positions (1280x720 scene)
  if (position == "left") {
    return QPointF(200, 100);
  } else if (position == "right") {
    return QPointF(900, 100);
  } else if (position == "center") {
    return QPointF(540, 100);
  } else if (position == "farleft") {
    return QPointF(50, 100);
  } else if (position == "farright") {
    return QPointF(1100, 100);
  }

  // Default to center
  return QPointF(540, 100);
}

} // namespace NovelMind::editor::qt
