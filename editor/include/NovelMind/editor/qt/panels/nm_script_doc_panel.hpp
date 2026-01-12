#pragma once

/**
 * @file nm_script_doc_panel.hpp
 * @brief Documentation panel for NMScript language help.
 */

#include "NovelMind/editor/qt/nm_dock_panel.hpp"
#include <QTextBrowser>

namespace NovelMind::editor::qt {

class NMScriptDocPanel final : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMScriptDocPanel(QWidget* parent = nullptr);
  ~NMScriptDocPanel() override = default;

  void onInitialize() override;
  void onUpdate(double deltaTime) override;

public slots:
  void setDocHtml(const QString& html);

private:
  void setupContent();

  QWidget* m_contentWidget = nullptr;
  QTextBrowser* m_browser = nullptr;
  QString m_emptyHtml;
};

} // namespace NovelMind::editor::qt
