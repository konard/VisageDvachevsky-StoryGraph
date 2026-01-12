#pragma once

/**
 * @file nm_scene_palette_panel.hpp
 * @brief Scene Palette panel for quick object creation and asset drops
 */

#include "NovelMind/editor/qt/nm_dock_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include <QButtonGroup>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFrame>
#include <QLabel>
#include <QPushButton>

namespace NovelMind::editor::qt {

class NMAssetDropArea : public QFrame {
  Q_OBJECT

public:
  explicit NMAssetDropArea(QWidget* parent = nullptr);
  void setHintText(const QString& text);

signals:
  void assetsDropped(const QStringList& paths);

protected:
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dropEvent(QDropEvent* event) override;

private:
  QLabel* m_label = nullptr;
};

class NMScenePalettePanel : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMScenePalettePanel(QWidget* parent = nullptr);
  ~NMScenePalettePanel() override;

  void onInitialize() override;
  void onUpdate(double deltaTime) override;

signals:
  void createObjectRequested(NMSceneObjectType type);
  // typeHint: -1 = auto, otherwise static_cast<int>(NMSceneObjectType)
  void assetsDropped(const QStringList& paths, int typeHint);

private slots:
  void onDropModeChanged(int id);
  void onAssetsDropped(const QStringList& paths);

private:
  void setupContent();
  int currentDropType() const;
  QString dropModeLabel(int mode) const;

  QWidget* m_contentWidget = nullptr;
  QButtonGroup* m_dropModeGroup = nullptr;
  QLabel* m_dropModeLabel = nullptr;
  NMAssetDropArea* m_dropArea = nullptr;
};

} // namespace NovelMind::editor::qt
