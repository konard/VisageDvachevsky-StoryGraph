#include "NovelMind/editor/qt/panels/nm_script_editor_panel.hpp"
#include "NovelMind/editor/qt/widgets/nm_scene_preview_widget.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include <QSettings>
#include <QTextCursor>

namespace NovelMind::editor::qt {

void NMScriptEditorPanel::toggleScenePreview() {
  if (!m_scenePreview) {
    return;
  }

  m_scenePreviewEnabled = !m_scenePreviewEnabled;

  if (m_scenePreviewEnabled) {
    // Show the preview widget
    m_scenePreview->show();
    m_scenePreview->setPreviewEnabled(true);

    // Update split sizes (60% editor, 40% preview)
    if (m_mainSplitter) {
      int totalWidth = m_mainSplitter->width();
      QList<int> sizes;
      sizes << static_cast<int>(totalWidth * 0.6)
            << static_cast<int>(totalWidth * 0.4);
      m_mainSplitter->setSizes(sizes);
    }

    // Trigger initial preview update
    onScriptTextChanged();
  } else {
    // Hide the preview widget
    m_scenePreview->hide();
    m_scenePreview->setPreviewEnabled(false);
  }

  // Update action state
  if (m_togglePreviewAction) {
    m_togglePreviewAction->setChecked(m_scenePreviewEnabled);
  }

  // Save preference to settings
  QSettings settings;
  settings.setValue("scriptEditor/previewEnabled", m_scenePreviewEnabled);
}

bool NMScriptEditorPanel::isScenePreviewEnabled() const {
  return m_scenePreviewEnabled;
}

void NMScriptEditorPanel::onScriptTextChanged() {
  if (!m_scenePreviewEnabled || !m_scenePreview) {
    return;
  }

  // Get current editor
  NMScriptEditor *editor = currentEditor();
  if (!editor) {
    return;
  }

  // Get script content
  QString scriptContent = editor->toPlainText();

  // Get cursor position
  QTextCursor cursor = editor->textCursor();
  int cursorLine = cursor.blockNumber() + 1; // 1-based line number
  int cursorColumn = cursor.columnNumber();

  // Set assets root from project (project_path/assets)
  std::string projectPath = ProjectManager::instance().getProjectPath();
  QString assetsRoot =
      QString::fromStdString(projectPath.empty() ? "" : projectPath + "/assets");
  m_scenePreview->setAssetsRoot(assetsRoot);

  // Update preview content
  m_scenePreview->setScriptContent(scriptContent, cursorLine, cursorColumn);

  // Request debounced update
  m_scenePreview->requestUpdate();
}

void NMScriptEditorPanel::onCursorPositionChanged() {
  if (!m_scenePreviewEnabled || !m_scenePreview) {
    return;
  }

  // Update preview when cursor position changes
  // This allows the preview to show different scene states as you navigate
  onScriptTextChanged();
}

} // namespace NovelMind::editor::qt
