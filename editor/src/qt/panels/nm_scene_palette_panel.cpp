#include "NovelMind/editor/qt/panels/nm_scene_palette_panel.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMimeData>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

// ============================================================================
// NMAssetDropArea
// ============================================================================

NMAssetDropArea::NMAssetDropArea(QWidget *parent) : QFrame(parent) {
  setObjectName("ScenePaletteDropArea");
  setAcceptDrops(true);
  setFrameShape(QFrame::StyledPanel);
  setMinimumHeight(110);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(10, 10, 10, 10);
  layout->setSpacing(6);

  m_label = new QLabel(tr("Drop assets here"), this);
  m_label->setAlignment(Qt::AlignCenter);
  m_label->setWordWrap(true);
  layout->addWidget(m_label);
}

void NMAssetDropArea::setHintText(const QString &text) {
  if (m_label) {
    m_label->setText(text);
  }
}

void NMAssetDropArea::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData() && event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
    return;
  }
  QFrame::dragEnterEvent(event);
}

void NMAssetDropArea::dragMoveEvent(QDragMoveEvent *event) {
  if (event->mimeData() && event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
    return;
  }
  QFrame::dragMoveEvent(event);
}

void NMAssetDropArea::dropEvent(QDropEvent *event) {
  if (event->mimeData() && event->mimeData()->hasUrls()) {
    QStringList paths;
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
      if (url.isLocalFile()) {
        paths.append(url.toLocalFile());
      }
    }
    if (!paths.isEmpty()) {
      emit assetsDropped(paths);
      event->acceptProposedAction();
      return;
    }
  }
  QFrame::dropEvent(event);
}

// ============================================================================
// NMScenePalettePanel
// ============================================================================

NMScenePalettePanel::NMScenePalettePanel(QWidget *parent)
    : NMDockPanel(tr("Scene Palette"), parent) {
  setPanelId("ScenePalette");
  setupContent();
}

NMScenePalettePanel::~NMScenePalettePanel() = default;

void NMScenePalettePanel::onInitialize() {}

void NMScenePalettePanel::onUpdate(double /*deltaTime*/) {}

void NMScenePalettePanel::setupContent() {
  m_contentWidget = new QWidget(this);
  auto *layout = new QVBoxLayout(m_contentWidget);
  layout->setContentsMargins(10, 10, 10, 10);
  layout->setSpacing(10);

  auto *title = new QLabel(tr("Create"), m_contentWidget);
  layout->addWidget(title);

  auto *grid = new QGridLayout();
  grid->setSpacing(6);

  auto &iconMgr = NMIconManager::instance();
  const auto &buttonSizes = NMStyleManager::instance().buttonSizes();
  auto makeButton = [&](const QString &label, const QString &iconName,
                        NMSceneObjectType type, int row, int col) {
    auto *btn = new QToolButton(m_contentWidget);
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btn->setIcon(iconMgr.getIcon(iconName, 24));
    btn->setText(label);
    btn->setMinimumSize(buttonSizes.paletteButtonWidth,
                        buttonSizes.paletteButton);
    connect(btn, &QToolButton::clicked, this,
            [this, type]() { emit createObjectRequested(type); });
    grid->addWidget(btn, row, col);
  };

  makeButton(tr("Background"), "object-background",
             NMSceneObjectType::Background, 0, 0);
  makeButton(tr("Character"), "object-character", NMSceneObjectType::Character,
             0, 1);
  makeButton(tr("UI"), "object-ui", NMSceneObjectType::UI, 1, 0);
  makeButton(tr("Effect"), "object-effect", NMSceneObjectType::Effect, 1, 1);

  layout->addLayout(grid);

  m_dropModeLabel = new QLabel(tr("Drop mode: Auto"), m_contentWidget);
  layout->addWidget(m_dropModeLabel);

  auto *dropModeRow = new QHBoxLayout();
  m_dropModeGroup = new QButtonGroup(this);
  m_dropModeGroup->setExclusive(true);

  auto makeDropMode = [&](const QString &label, int id) {
    auto *btn = new QPushButton(label, m_contentWidget);
    btn->setCheckable(true);
    if (id == -1) {
      btn->setChecked(true);
    }
    m_dropModeGroup->addButton(btn, id);
    dropModeRow->addWidget(btn);
  };

  makeDropMode(tr("Auto"), -1);
  makeDropMode(tr("BG"), static_cast<int>(NMSceneObjectType::Background));
  makeDropMode(tr("Char"), static_cast<int>(NMSceneObjectType::Character));
  makeDropMode(tr("UI"), static_cast<int>(NMSceneObjectType::UI));
  makeDropMode(tr("FX"), static_cast<int>(NMSceneObjectType::Effect));

  layout->addLayout(dropModeRow);
  connect(m_dropModeGroup, &QButtonGroup::idClicked, this,
          &NMScenePalettePanel::onDropModeChanged);

  m_dropArea = new NMAssetDropArea(m_contentWidget);
  layout->addWidget(m_dropArea);
  connect(m_dropArea, &NMAssetDropArea::assetsDropped, this,
          &NMScenePalettePanel::onAssetsDropped);

  setContentWidget(m_contentWidget);
}

int NMScenePalettePanel::currentDropType() const {
  if (!m_dropModeGroup) {
    return -1;
  }
  return m_dropModeGroup->checkedId();
}

QString NMScenePalettePanel::dropModeLabel(int mode) const {
  switch (mode) {
  case -1:
    return tr("Drop mode: Auto");
  case static_cast<int>(NMSceneObjectType::Background):
    return tr("Drop mode: Background");
  case static_cast<int>(NMSceneObjectType::Character):
    return tr("Drop mode: Character");
  case static_cast<int>(NMSceneObjectType::UI):
    return tr("Drop mode: UI");
  case static_cast<int>(NMSceneObjectType::Effect):
    return tr("Drop mode: Effect");
  default:
    return tr("Drop mode: Auto");
  }
}

void NMScenePalettePanel::onDropModeChanged(int id) {
  if (m_dropModeLabel) {
    m_dropModeLabel->setText(dropModeLabel(id));
  }
}

void NMScenePalettePanel::onAssetsDropped(const QStringList &paths) {
  emit assetsDropped(paths, currentDropType());
}

} // namespace NovelMind::editor::qt
