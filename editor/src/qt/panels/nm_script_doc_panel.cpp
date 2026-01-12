#include "NovelMind/editor/qt/panels/nm_script_doc_panel.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"

#include <QVBoxLayout>

namespace NovelMind::editor::qt {

NMScriptDocPanel::NMScriptDocPanel(QWidget* parent) : NMDockPanel(tr("Script Docs"), parent) {
  setPanelId("ScriptDocs");
  setupContent();
}

void NMScriptDocPanel::onInitialize() {
  NMDockPanel::onInitialize();
}

void NMScriptDocPanel::onUpdate(double deltaTime) {
  NMDockPanel::onUpdate(deltaTime);
  Q_UNUSED(deltaTime);
}

void NMScriptDocPanel::setupContent() {
  m_contentWidget = new QWidget(this);
  auto* layout = new QVBoxLayout(m_contentWidget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_browser = new QTextBrowser(m_contentWidget);
  m_browser->setOpenExternalLinks(false);
  m_browser->setReadOnly(true);

  const auto& palette = NMStyleManager::instance().palette();
  m_browser->setStyleSheet(QString("QTextBrowser {"
                                   "  background-color: %1;"
                                   "  color: %2;"
                                   "  border: none;"
                                   "  padding: 8px;"
                                   "}")
                               .arg(palette.bgMedium.name())
                               .arg(palette.textPrimary.name()));

  m_emptyHtml = tr("<h3>NMScript Docs</h3>"
                   "<p>Hover a keyword in the script editor to see help.</p>");
  m_browser->setHtml(m_emptyHtml);

  layout->addWidget(m_browser);
  setContentWidget(m_contentWidget);
}

void NMScriptDocPanel::setDocHtml(const QString& html) {
  if (!m_browser) {
    return;
  }
  if (html.trimmed().isEmpty()) {
    m_browser->setHtml(m_emptyHtml);
  } else {
    m_browser->setHtml(html);
  }
}

} // namespace NovelMind::editor::qt
