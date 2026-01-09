#include "NovelMind/editor/qt/nm_main_window.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_asset_browser_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_audio_mixer_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_build_settings_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_console_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_curve_editor_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_debug_overlay_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_diagnostics_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_hierarchy_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_inspector_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_issues_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_localization_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_play_toolbar_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_dialogue_graph_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_palette_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_script_doc_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_script_editor_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_timeline_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_voice_manager_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_voice_studio_panel.hpp"

#include <QDockWidget>
#include <QEvent>
#include <QHash>
#include <QSettings>
#include <QStyle>

namespace NovelMind::editor::qt {

void NMMainWindow::createDefaultLayout() {
  // D2: Apply the Default workspace preset on first load
  applyLayoutPreset(LayoutPreset::Default);
}

// ============================================================================
// D2: Workspace Preset System Implementation
// ============================================================================

void NMMainWindow::applyWorkspacePreset(LayoutPreset preset) {
  m_currentPreset = preset;
  applyLayoutPreset(preset);

  // Save the current preset choice
  QSettings settings("NovelMind", "Editor");
  settings.setValue("workspace/currentPreset", static_cast<int>(preset));
}

QString NMMainWindow::currentWorkspacePresetName() const {
  switch (m_currentPreset) {
  case LayoutPreset::Default:
    return tr("Default");
  case LayoutPreset::StoryScript:
    return tr("Story / Script");
  case LayoutPreset::SceneAnimation:
    return tr("Scene / Animation");
  case LayoutPreset::AudioVoice:
    return tr("Audio / Voice");
  case LayoutPreset::Story:
    return tr("Story");
  case LayoutPreset::Scene:
    return tr("Scene");
  case LayoutPreset::Script:
    return tr("Script");
  case LayoutPreset::Developer:
    return tr("Developer");
  case LayoutPreset::Compact:
    return tr("Compact");
  }
  return tr("Custom");
}

void NMMainWindow::saveWorkspacePreset(const QString& name) {
  if (name.isEmpty()) {
    return;
  }

  QSettings settings("NovelMind", "Editor");
  settings.beginGroup("workspace/custom/" + name);
  settings.setValue("geometry", saveGeometry());
  settings.setValue("state", saveState());
  settings.endGroup();

  setStatusMessage(tr("Workspace preset '%1' saved").arg(name), 2000);
}

bool NMMainWindow::loadWorkspacePreset(const QString& name) {
  if (name.isEmpty()) {
    return false;
  }

  QSettings settings("NovelMind", "Editor");
  settings.beginGroup("workspace/custom/" + name);
  const QByteArray geometry = settings.value("geometry").toByteArray();
  const QByteArray state = settings.value("state").toByteArray();
  settings.endGroup();

  if (geometry.isEmpty() || state.isEmpty()) {
    setStatusMessage(tr("Workspace preset '%1' not found").arg(name), 2000);
    return false;
  }

  restoreGeometry(geometry);
  restoreState(state);
  setStatusMessage(tr("Workspace preset '%1' loaded").arg(name), 2000);
  return true;
}

QStringList NMMainWindow::availableWorkspacePresets() const {
  QStringList presets;

  // Built-in presets
  presets << tr("Default") << tr("Story / Script") << tr("Scene / Animation") << tr("Audio / Voice")
          << tr("Story") << tr("Scene") << tr("Script") << tr("Developer") << tr("Compact");

  // Custom presets from settings
  QSettings settings("NovelMind", "Editor");
  settings.beginGroup("workspace/custom");
  presets << settings.childGroups();
  settings.endGroup();

  return presets;
}

void NMMainWindow::focusNextDock(bool reverse) {
  QList<QDockWidget *> docks;
  docks.append(static_cast<QDockWidget *>(m_sceneViewPanel));
  docks.append(static_cast<QDockWidget *>(m_storyGraphPanel));
  docks.append(static_cast<QDockWidget *>(m_sceneDialogueGraphPanel));
  docks.append(static_cast<QDockWidget *>(m_inspectorPanel));
  docks.append(static_cast<QDockWidget *>(m_consolePanel));
  docks.append(static_cast<QDockWidget *>(m_assetBrowserPanel));
  docks.append(static_cast<QDockWidget *>(m_hierarchyPanel));
  docks.append(static_cast<QDockWidget *>(m_scenePalettePanel));
  docks.append(static_cast<QDockWidget *>(m_scriptEditorPanel));
  docks.append(static_cast<QDockWidget *>(m_scriptDocPanel));
  docks.append(static_cast<QDockWidget *>(m_scriptInspectorPanel));
  docks.append(static_cast<QDockWidget *>(m_scriptRuntimeInspectorPanel));
  docks.append(static_cast<QDockWidget *>(m_playToolbarPanel));
  docks.append(static_cast<QDockWidget *>(m_debugOverlayPanel));
  docks.append(static_cast<QDockWidget *>(m_voiceManagerPanel));
  docks.append(static_cast<QDockWidget *>(m_voiceStudioPanel));
  docks.append(static_cast<QDockWidget *>(m_audioMixerPanel));
  docks.append(static_cast<QDockWidget *>(m_localizationPanel));
  docks.append(static_cast<QDockWidget *>(m_timelinePanel));
  docks.append(static_cast<QDockWidget *>(m_curveEditorPanel));
  docks.append(static_cast<QDockWidget *>(m_buildSettingsPanel));
  docks.append(static_cast<QDockWidget *>(m_issuesPanel));
  docks.append(static_cast<QDockWidget *>(m_diagnosticsPanel));

  QList<QDockWidget *> visible;
  for (auto *dock : docks) {
    if (dock && dock->isVisible()) {
      visible << dock;
    }
  }
  if (visible.isEmpty()) {
    return;
  }

  qsizetype idx = 0;
  if (m_lastFocusedDock) {
    idx = visible.indexOf(m_lastFocusedDock);
  }
  if (idx < 0) {
    idx = 0;
  }

  const qsizetype size = visible.size();
  idx = reverse ? (idx - 1 + size) % size : (idx + 1) % size;

  auto* target = visible.at(idx);
  if (!target) {
    return;
  }
  target->raise();
  target->setFocus(Qt::OtherFocusReason);
  target->setProperty("focusedDock", true);
  target->style()->unpolish(target);
  target->style()->polish(target);
  m_lastFocusedDock = target;
}

bool NMMainWindow::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::FocusIn) {
    if (auto* dock = qobject_cast<QDockWidget*>(watched)) {
      if (m_lastFocusedDock && m_lastFocusedDock != dock) {
        m_lastFocusedDock->setProperty("focusedDock", false);
        m_lastFocusedDock->style()->unpolish(m_lastFocusedDock);
        m_lastFocusedDock->style()->polish(m_lastFocusedDock);
      }
      m_lastFocusedDock = dock;
      m_lastFocusedDock->setProperty("focusedDock", true);
      m_lastFocusedDock->style()->unpolish(m_lastFocusedDock);
      m_lastFocusedDock->style()->polish(m_lastFocusedDock);
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

void NMMainWindow::applyLayoutPreset(LayoutPreset preset) {
  QList<QDockWidget *> docks;
  docks.append(static_cast<QDockWidget *>(m_sceneViewPanel));
  docks.append(static_cast<QDockWidget *>(m_storyGraphPanel));
  docks.append(static_cast<QDockWidget *>(m_sceneDialogueGraphPanel));
  docks.append(static_cast<QDockWidget *>(m_inspectorPanel));
  docks.append(static_cast<QDockWidget *>(m_consolePanel));
  docks.append(static_cast<QDockWidget *>(m_assetBrowserPanel));
  docks.append(static_cast<QDockWidget *>(m_hierarchyPanel));
  docks.append(static_cast<QDockWidget *>(m_scenePalettePanel));
  docks.append(static_cast<QDockWidget *>(m_scriptEditorPanel));
  docks.append(static_cast<QDockWidget *>(m_scriptDocPanel));
  docks.append(static_cast<QDockWidget *>(m_scriptInspectorPanel));
  docks.append(static_cast<QDockWidget *>(m_scriptRuntimeInspectorPanel));
  docks.append(static_cast<QDockWidget *>(m_playToolbarPanel));
  docks.append(static_cast<QDockWidget *>(m_debugOverlayPanel));
  docks.append(static_cast<QDockWidget *>(m_voiceManagerPanel));
  docks.append(static_cast<QDockWidget *>(m_voiceStudioPanel));
  docks.append(static_cast<QDockWidget *>(m_audioMixerPanel));
  docks.append(static_cast<QDockWidget *>(m_localizationPanel));
  docks.append(static_cast<QDockWidget *>(m_timelinePanel));
  docks.append(static_cast<QDockWidget *>(m_curveEditorPanel));
  docks.append(static_cast<QDockWidget *>(m_buildSettingsPanel));
  docks.append(static_cast<QDockWidget *>(m_issuesPanel));
  docks.append(static_cast<QDockWidget *>(m_diagnosticsPanel));

  for (auto *dock : docks) {
    if (!dock) {
      continue;
    }
    dock->setFloating(false);
    dock->hide();
    removeDockWidget(dock);
  }

  setCentralWidget(nullptr);

  if (m_playToolbarPanel) {
    addDockWidget(Qt::TopDockWidgetArea, m_playToolbarPanel);
    m_playToolbarPanel->show();
  }

  switch (preset) {
    // ========================================================================
    // D2: New Workspace Presets
    // ========================================================================

  case LayoutPreset::Default: {
    // D2: Default workspace - balanced layout for general editing
    // Left: Hierarchy, Scene Palette
    // Center: Scene View (main), Story Graph (tab)
    // Right: Inspector Group (Inspector, Scene Palette)
    // Bottom: Console, Asset Browser, Timeline

    if (m_scenePalettePanel)
      m_scenePalettePanel->show();
    if (m_hierarchyPanel)
      m_hierarchyPanel->show();
    if (m_sceneViewPanel)
      m_sceneViewPanel->show();
    if (m_storyGraphPanel)
      m_storyGraphPanel->show();
    if (m_inspectorPanel)
      m_inspectorPanel->show();
    if (m_consolePanel)
      m_consolePanel->show();
    if (m_assetBrowserPanel)
      m_assetBrowserPanel->show();
    if (m_timelinePanel)
      m_timelinePanel->show();

    // Left area
    if (m_hierarchyPanel) {
      addDockWidget(Qt::LeftDockWidgetArea, m_hierarchyPanel);
    }

    // Center area
    if (m_sceneViewPanel) {
      addDockWidget(Qt::TopDockWidgetArea, m_sceneViewPanel);
      m_sceneViewPanel->raise();
    }
    if (m_storyGraphPanel) {
      addDockWidget(Qt::TopDockWidgetArea, m_storyGraphPanel);
    }
    if (m_sceneViewPanel && m_storyGraphPanel) {
      tabifyDockWidget(m_sceneViewPanel, m_storyGraphPanel);
      m_sceneViewPanel->raise();
    }

    // Right area - Inspector Group
    if (m_inspectorPanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_inspectorPanel);
    }
    if (m_scenePalettePanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_scenePalettePanel);
    }
    if (m_inspectorPanel && m_scenePalettePanel) {
      tabifyDockWidget(m_inspectorPanel, m_scenePalettePanel);
      m_inspectorPanel->raise();
    }

    // Bottom area
    if (m_consolePanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_consolePanel);
    }
    if (m_assetBrowserPanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_assetBrowserPanel);
    }
    if (m_timelinePanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_timelinePanel);
    }
    if (m_consolePanel && m_assetBrowserPanel) {
      tabifyDockWidget(m_consolePanel, m_assetBrowserPanel);
    }
    if (m_consolePanel && m_timelinePanel) {
      tabifyDockWidget(m_consolePanel, m_timelinePanel);
    }
    if (m_assetBrowserPanel) {
      m_assetBrowserPanel->raise();
    }

    // Resize
    if (m_hierarchyPanel) {
      resizeDocks({m_hierarchyPanel}, {240}, Qt::Horizontal);
    }
    if (m_inspectorPanel) {
      resizeDocks({m_inspectorPanel}, {320}, Qt::Horizontal);
    }
    if (m_consolePanel) {
      resizeDocks({m_consolePanel}, {220}, Qt::Vertical);
    }
    break;
  }

  case LayoutPreset::StoryScript: {
    // D2: Story/Script focused workspace
    // Left: Script Debugging Group (Script Inspector, Runtime Inspector, Documentation)
    // Center: Story Graph (main), Script Editor (tab)
    // Right: Inspector, Voice Manager, Localization
    // Bottom: Console, Issues, Diagnostics, Build Settings

    if (m_storyGraphPanel)
      m_storyGraphPanel->show();
    if (m_scriptEditorPanel)
      m_scriptEditorPanel->show();
    if (m_scriptDocPanel)
      m_scriptDocPanel->show();
    if (m_scriptInspectorPanel)
      m_scriptInspectorPanel->show();
    if (m_scriptRuntimeInspectorPanel)
      m_scriptRuntimeInspectorPanel->show();
    if (m_inspectorPanel)
      m_inspectorPanel->show();
    if (m_voiceManagerPanel)
      m_voiceManagerPanel->show();
    if (m_localizationPanel)
      m_localizationPanel->show();
    if (m_consolePanel)
      m_consolePanel->show();
    if (m_issuesPanel)
      m_issuesPanel->show();
    if (m_diagnosticsPanel)
      m_diagnosticsPanel->show();
    if (m_buildSettingsPanel)
      m_buildSettingsPanel->show();

    // Left area - Script Debugging Group
    if (m_scriptInspectorPanel) {
      addDockWidget(Qt::LeftDockWidgetArea, m_scriptInspectorPanel);
    }
    if (m_scriptRuntimeInspectorPanel) {
      addDockWidget(Qt::LeftDockWidgetArea, m_scriptRuntimeInspectorPanel);
    }
    if (m_scriptDocPanel) {
      addDockWidget(Qt::LeftDockWidgetArea, m_scriptDocPanel);
    }
    if (m_scriptInspectorPanel && m_scriptRuntimeInspectorPanel) {
      tabifyDockWidget(m_scriptInspectorPanel, m_scriptRuntimeInspectorPanel);
    }
    if (m_scriptInspectorPanel && m_scriptDocPanel) {
      tabifyDockWidget(m_scriptInspectorPanel, m_scriptDocPanel);
    }
    if (m_scriptInspectorPanel) {
      m_scriptInspectorPanel->raise();
    }

    // Center area
    if (m_storyGraphPanel) {
      addDockWidget(Qt::TopDockWidgetArea, m_storyGraphPanel);
      m_storyGraphPanel->raise();
    }
    if (m_scriptEditorPanel) {
      addDockWidget(Qt::TopDockWidgetArea, m_scriptEditorPanel);
    }
    if (m_storyGraphPanel && m_scriptEditorPanel) {
      tabifyDockWidget(m_storyGraphPanel, m_scriptEditorPanel);
      m_storyGraphPanel->raise();
    }

    // Right area
    if (m_inspectorPanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_inspectorPanel);
    }
    if (m_voiceManagerPanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_voiceManagerPanel);
    }
    if (m_localizationPanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_localizationPanel);
    }
    if (m_inspectorPanel && m_voiceManagerPanel) {
      tabifyDockWidget(m_inspectorPanel, m_voiceManagerPanel);
    }
    if (m_inspectorPanel && m_localizationPanel) {
      tabifyDockWidget(m_inspectorPanel, m_localizationPanel);
    }
    if (m_inspectorPanel) {
      m_inspectorPanel->raise();
    }

    // Bottom area - Output Group
    if (m_consolePanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_consolePanel);
    }
    if (m_issuesPanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_issuesPanel);
    }
    if (m_diagnosticsPanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_diagnosticsPanel);
    }
    if (m_buildSettingsPanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_buildSettingsPanel);
    }
    if (m_consolePanel && m_issuesPanel) {
      tabifyDockWidget(m_consolePanel, m_issuesPanel);
    }
    if (m_consolePanel && m_diagnosticsPanel) {
      tabifyDockWidget(m_consolePanel, m_diagnosticsPanel);
    }
    if (m_consolePanel && m_buildSettingsPanel) {
      tabifyDockWidget(m_consolePanel, m_buildSettingsPanel);
    }
    if (m_consolePanel) {
      m_consolePanel->raise();
    }

    // Resize
    if (m_scriptInspectorPanel) {
      resizeDocks({m_scriptInspectorPanel}, {260}, Qt::Horizontal);
    }
    if (m_inspectorPanel) {
      resizeDocks({m_inspectorPanel}, {300}, Qt::Horizontal);
    }
    if (m_consolePanel) {
      resizeDocks({m_consolePanel}, {180}, Qt::Vertical);
    }
    break;
  }

  case LayoutPreset::SceneAnimation: {
    // D2: Scene/Animation focused workspace
    // Left: Hierarchy
    // Center: Scene View (main)
    // Right: Inspector Group (Inspector, Scene Palette)
    // Bottom: Animation Group (Timeline, Curve Editor), Asset Browser

    if (m_sceneViewPanel)
      m_sceneViewPanel->show();
    if (m_hierarchyPanel)
      m_hierarchyPanel->show();
    if (m_scenePalettePanel)
      m_scenePalettePanel->show();
    if (m_inspectorPanel)
      m_inspectorPanel->show();
    if (m_curveEditorPanel)
      m_curveEditorPanel->show();
    if (m_timelinePanel)
      m_timelinePanel->show();
    if (m_assetBrowserPanel)
      m_assetBrowserPanel->show();

    // Left area
    if (m_hierarchyPanel) {
      addDockWidget(Qt::LeftDockWidgetArea, m_hierarchyPanel);
    }

    // Center area
    if (m_sceneViewPanel) {
      addDockWidget(Qt::TopDockWidgetArea, m_sceneViewPanel);
      m_sceneViewPanel->raise();
    }

    // Right area - Inspector Group
    if (m_inspectorPanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_inspectorPanel);
    }
    if (m_scenePalettePanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_scenePalettePanel);
    }
    if (m_inspectorPanel && m_scenePalettePanel) {
      tabifyDockWidget(m_inspectorPanel, m_scenePalettePanel);
      m_inspectorPanel->raise();
    }

    // Bottom area - Animation Group (Timeline, Curve Editor), Asset Browser
    if (m_timelinePanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_timelinePanel);
    }
    if (m_curveEditorPanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_curveEditorPanel);
    }
    if (m_assetBrowserPanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_assetBrowserPanel);
    }
    if (m_timelinePanel && m_curveEditorPanel) {
      tabifyDockWidget(m_timelinePanel, m_curveEditorPanel);
    }
    if (m_timelinePanel && m_assetBrowserPanel) {
      tabifyDockWidget(m_timelinePanel, m_assetBrowserPanel);
    }
    if (m_timelinePanel) {
      m_timelinePanel->raise();
    }

    // Resize
    if (m_hierarchyPanel) {
      resizeDocks({m_hierarchyPanel}, {240}, Qt::Horizontal);
    }
    if (m_inspectorPanel) {
      resizeDocks({m_inspectorPanel}, {320}, Qt::Horizontal);
    }
    if (m_timelinePanel) {
      resizeDocks({m_timelinePanel}, {280}, Qt::Vertical);
    }
    break;
  }

  case LayoutPreset::AudioVoice: {
    // D2 + D6: Audio/Voice focused workspace
    // Left: Asset Browser (filtered to audio)
    // Center: Voice Studio (main), Voice Manager (tab)
    // Right: Inspector, Audio Mixer
    // Bottom: Console, Diagnostics

    if (m_voiceStudioPanel)
      m_voiceStudioPanel->show();
    if (m_voiceManagerPanel)
      m_voiceManagerPanel->show();
    if (m_audioMixerPanel)
      m_audioMixerPanel->show();
    if (m_assetBrowserPanel)
      m_assetBrowserPanel->show();
    if (m_inspectorPanel)
      m_inspectorPanel->show();
    if (m_consolePanel)
      m_consolePanel->show();
    if (m_diagnosticsPanel)
      m_diagnosticsPanel->show();

    // Left area - Asset Browser for audio files
    if (m_assetBrowserPanel) {
      addDockWidget(Qt::LeftDockWidgetArea, m_assetBrowserPanel);
    }

    // Center area - Voice tools
    if (m_voiceStudioPanel) {
      addDockWidget(Qt::TopDockWidgetArea, m_voiceStudioPanel);
      m_voiceStudioPanel->raise();
    }
    if (m_voiceManagerPanel) {
      addDockWidget(Qt::TopDockWidgetArea, m_voiceManagerPanel);
    }
    if (m_voiceStudioPanel && m_voiceManagerPanel) {
      tabifyDockWidget(m_voiceStudioPanel, m_voiceManagerPanel);
      m_voiceStudioPanel->raise();
    }

    // Right area - Inspector and Audio Mixer
    if (m_inspectorPanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_inspectorPanel);
    }
    if (m_audioMixerPanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_audioMixerPanel);
    }
    if (m_inspectorPanel && m_audioMixerPanel) {
      tabifyDockWidget(m_inspectorPanel, m_audioMixerPanel);
      m_audioMixerPanel->raise();
    }

    // Bottom area
    if (m_consolePanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_consolePanel);
    }
    if (m_diagnosticsPanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_diagnosticsPanel);
    }
    if (m_consolePanel && m_diagnosticsPanel) {
      tabifyDockWidget(m_consolePanel, m_diagnosticsPanel);
      m_consolePanel->raise();
    }

    // Resize
    if (m_assetBrowserPanel) {
      resizeDocks({m_assetBrowserPanel}, {280}, Qt::Horizontal);
    }
    if (m_inspectorPanel) {
      resizeDocks({m_inspectorPanel}, {320}, Qt::Horizontal);
    }
    if (m_consolePanel) {
      resizeDocks({m_consolePanel}, {180}, Qt::Vertical);
    }
    break;
  }

    // ========================================================================
    // Legacy Presets (maintained for compatibility)
    // ========================================================================

  case LayoutPreset::Story: {
    if (m_storyGraphPanel)
      m_storyGraphPanel->show();
    if (m_inspectorPanel)
      m_inspectorPanel->show();
    if (m_consolePanel)
      m_consolePanel->show();
    if (m_issuesPanel)
      m_issuesPanel->show();
    if (m_diagnosticsPanel)
      m_diagnosticsPanel->show();
    if (m_voiceManagerPanel)
      m_voiceManagerPanel->show();
    if (m_localizationPanel)
      m_localizationPanel->show();

    if (m_storyGraphPanel) {
      addDockWidget(Qt::TopDockWidgetArea, m_storyGraphPanel);
      m_storyGraphPanel->raise();
    }

    if (m_inspectorPanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_inspectorPanel);
    }
    if (m_debugOverlayPanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_debugOverlayPanel);
    }
    if (m_voiceManagerPanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_voiceManagerPanel);
    }
    if (m_localizationPanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_localizationPanel);
    }
    if (m_inspectorPanel && m_debugOverlayPanel) {
      tabifyDockWidget(m_inspectorPanel, m_debugOverlayPanel);
    }
    if (m_inspectorPanel && m_voiceManagerPanel) {
      tabifyDockWidget(m_inspectorPanel, m_voiceManagerPanel);
    }
    if (m_inspectorPanel && m_localizationPanel) {
      tabifyDockWidget(m_inspectorPanel, m_localizationPanel);
    }

    if (m_consolePanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_consolePanel);
    }
    if (m_issuesPanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_issuesPanel);
    }
    if (m_diagnosticsPanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_diagnosticsPanel);
    }
    if (m_consolePanel && m_issuesPanel) {
      tabifyDockWidget(m_consolePanel, m_issuesPanel);
      m_consolePanel->raise();
    }
    if (m_consolePanel && m_diagnosticsPanel) {
      tabifyDockWidget(m_consolePanel, m_diagnosticsPanel);
    }

    if (m_inspectorPanel) {
      resizeDocks({m_inspectorPanel}, {300}, Qt::Horizontal);
    }
    if (m_consolePanel) {
      resizeDocks({m_consolePanel}, {200}, Qt::Vertical);
    }
    break;
  }
  case LayoutPreset::Scene: {
    if (m_sceneViewPanel)
      m_sceneViewPanel->show();
    if (m_inspectorPanel)
      m_inspectorPanel->show();
    if (m_assetBrowserPanel)
      m_assetBrowserPanel->show();
    if (m_scenePalettePanel)
      m_scenePalettePanel->show();
    if (m_hierarchyPanel)
      m_hierarchyPanel->show();

    if (m_scenePalettePanel) {
      addDockWidget(Qt::LeftDockWidgetArea, m_scenePalettePanel);
    }
    if (m_hierarchyPanel) {
      addDockWidget(Qt::LeftDockWidgetArea, m_hierarchyPanel);
    }
    if (m_scenePalettePanel && m_hierarchyPanel) {
      tabifyDockWidget(m_scenePalettePanel, m_hierarchyPanel);
      m_scenePalettePanel->raise();
    }

    if (m_sceneViewPanel) {
      addDockWidget(Qt::TopDockWidgetArea, m_sceneViewPanel);
      m_sceneViewPanel->raise();
    }

    if (m_inspectorPanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_inspectorPanel);
    }

    if (m_assetBrowserPanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_assetBrowserPanel);
    }

    if (m_hierarchyPanel) {
      resizeDocks({m_hierarchyPanel}, {220}, Qt::Horizontal);
    }
    if (m_inspectorPanel) {
      resizeDocks({m_inspectorPanel}, {300}, Qt::Horizontal);
    }
    if (m_assetBrowserPanel) {
      resizeDocks({m_assetBrowserPanel}, {200}, Qt::Vertical);
    }
    break;
  }
  case LayoutPreset::Script: {
    if (m_scriptEditorPanel)
      m_scriptEditorPanel->show();
    if (m_storyGraphPanel)
      m_storyGraphPanel->show();
    if (m_scriptDocPanel)
      m_scriptDocPanel->show();
    if (m_issuesPanel)
      m_issuesPanel->show();
    if (m_diagnosticsPanel)
      m_diagnosticsPanel->show();
    if (m_voiceManagerPanel)
      m_voiceManagerPanel->show();
    if (m_localizationPanel)
      m_localizationPanel->show();

    if (m_scriptEditorPanel) {
      addDockWidget(Qt::TopDockWidgetArea, m_scriptEditorPanel);
    }
    if (m_storyGraphPanel) {
      addDockWidget(Qt::TopDockWidgetArea, m_storyGraphPanel);
    }
    if (m_scriptEditorPanel && m_storyGraphPanel) {
      tabifyDockWidget(m_scriptEditorPanel, m_storyGraphPanel);
      m_scriptEditorPanel->raise();
    }

    if (m_scriptDocPanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_scriptDocPanel);
    }
    if (m_voiceManagerPanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_voiceManagerPanel);
    }
    if (m_localizationPanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_localizationPanel);
    }
    if (m_scriptDocPanel && m_voiceManagerPanel) {
      tabifyDockWidget(m_scriptDocPanel, m_voiceManagerPanel);
    }
    if (m_scriptDocPanel && m_localizationPanel) {
      tabifyDockWidget(m_scriptDocPanel, m_localizationPanel);
    }

    if (m_issuesPanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_issuesPanel);
    }
    if (m_diagnosticsPanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_diagnosticsPanel);
    }
    if (m_issuesPanel && m_diagnosticsPanel) {
      tabifyDockWidget(m_issuesPanel, m_diagnosticsPanel);
    }

    if (m_scriptEditorPanel) {
      resizeDocks({m_scriptEditorPanel}, {600}, Qt::Horizontal);
    }
    if (m_issuesPanel) {
      resizeDocks({m_issuesPanel}, {200}, Qt::Vertical);
    }
    break;
  }
  case LayoutPreset::Developer: {
    if (m_sceneViewPanel)
      m_sceneViewPanel->show();
    if (m_scriptEditorPanel)
      m_scriptEditorPanel->show();
    if (m_inspectorPanel)
      m_inspectorPanel->show();
    if (m_debugOverlayPanel)
      m_debugOverlayPanel->show();
    if (m_voiceManagerPanel)
      m_voiceManagerPanel->show();
    if (m_localizationPanel)
      m_localizationPanel->show();
    if (m_consolePanel)
      m_consolePanel->show();
    if (m_issuesPanel)
      m_issuesPanel->show();
    if (m_diagnosticsPanel)
      m_diagnosticsPanel->show();
    if (m_hierarchyPanel)
      m_hierarchyPanel->show();
    if (m_assetBrowserPanel)
      m_assetBrowserPanel->show();
    if (m_timelinePanel)
      m_timelinePanel->show();
    if (m_curveEditorPanel)
      m_curveEditorPanel->show();
    if (m_buildSettingsPanel)
      m_buildSettingsPanel->show();

    if (m_hierarchyPanel) {
      addDockWidget(Qt::LeftDockWidgetArea, m_hierarchyPanel);
    }

    if (m_sceneViewPanel) {
      addDockWidget(Qt::TopDockWidgetArea, m_sceneViewPanel);
      m_sceneViewPanel->raise();
    }
    if (m_scriptEditorPanel) {
      addDockWidget(Qt::TopDockWidgetArea, m_scriptEditorPanel);
    }
    if (m_sceneViewPanel && m_scriptEditorPanel) {
      tabifyDockWidget(m_sceneViewPanel, m_scriptEditorPanel);
      m_sceneViewPanel->raise();
    }

    if (m_inspectorPanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_inspectorPanel);
    }
    if (m_debugOverlayPanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_debugOverlayPanel);
    }
    if (m_voiceManagerPanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_voiceManagerPanel);
    }
    if (m_localizationPanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_localizationPanel);
    }
    if (m_inspectorPanel && m_debugOverlayPanel) {
      tabifyDockWidget(m_inspectorPanel, m_debugOverlayPanel);
      m_inspectorPanel->raise();
    }
    if (m_inspectorPanel && m_voiceManagerPanel) {
      tabifyDockWidget(m_inspectorPanel, m_voiceManagerPanel);
    }
    if (m_inspectorPanel && m_localizationPanel) {
      tabifyDockWidget(m_inspectorPanel, m_localizationPanel);
    }

    if (m_consolePanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_consolePanel);
    }
    if (m_issuesPanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_issuesPanel);
    }
    if (m_diagnosticsPanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_diagnosticsPanel);
    }
    if (m_assetBrowserPanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_assetBrowserPanel);
    }
    if (m_timelinePanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_timelinePanel);
    }
    if (m_curveEditorPanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_curveEditorPanel);
    }
    if (m_buildSettingsPanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_buildSettingsPanel);
    }
    if (m_consolePanel && m_issuesPanel) {
      tabifyDockWidget(m_consolePanel, m_issuesPanel);
    }
    if (m_consolePanel && m_diagnosticsPanel) {
      tabifyDockWidget(m_consolePanel, m_diagnosticsPanel);
    }
    if (m_consolePanel && m_assetBrowserPanel) {
      tabifyDockWidget(m_consolePanel, m_assetBrowserPanel);
    }
    if (m_consolePanel && m_timelinePanel) {
      tabifyDockWidget(m_consolePanel, m_timelinePanel);
    }
    if (m_consolePanel && m_curveEditorPanel) {
      tabifyDockWidget(m_consolePanel, m_curveEditorPanel);
    }
    if (m_consolePanel && m_buildSettingsPanel) {
      tabifyDockWidget(m_consolePanel, m_buildSettingsPanel);
    }
    if (m_consolePanel) {
      m_consolePanel->raise();
    }

    if (m_hierarchyPanel) {
      resizeDocks({m_hierarchyPanel}, {220}, Qt::Horizontal);
    }
    if (m_inspectorPanel) {
      resizeDocks({m_inspectorPanel}, {300}, Qt::Horizontal);
    }
    if (m_consolePanel) {
      resizeDocks({m_consolePanel}, {200}, Qt::Vertical);
    }
    break;
  }
  case LayoutPreset::Compact: {
    if (m_sceneViewPanel)
      m_sceneViewPanel->show();
    if (m_storyGraphPanel)
      m_storyGraphPanel->show();
    if (m_scriptEditorPanel)
      m_scriptEditorPanel->show();
    if (m_scenePalettePanel)
      m_scenePalettePanel->show();
    if (m_hierarchyPanel)
      m_hierarchyPanel->show();
    if (m_inspectorPanel)
      m_inspectorPanel->show();
    if (m_voiceManagerPanel)
      m_voiceManagerPanel->show();
    if (m_localizationPanel)
      m_localizationPanel->show();
    if (m_consolePanel)
      m_consolePanel->show();
    if (m_assetBrowserPanel)
      m_assetBrowserPanel->show();
    if (m_issuesPanel)
      m_issuesPanel->show();
    if (m_diagnosticsPanel)
      m_diagnosticsPanel->show();
    if (m_timelinePanel)
      m_timelinePanel->show();
    if (m_curveEditorPanel)
      m_curveEditorPanel->show();

    if (m_scenePalettePanel) {
      addDockWidget(Qt::LeftDockWidgetArea, m_scenePalettePanel);
    }
    if (m_hierarchyPanel) {
      addDockWidget(Qt::LeftDockWidgetArea, m_hierarchyPanel);
    }
    if (m_scenePalettePanel && m_hierarchyPanel) {
      tabifyDockWidget(m_scenePalettePanel, m_hierarchyPanel);
    }

    if (m_sceneViewPanel) {
      addDockWidget(Qt::TopDockWidgetArea, m_sceneViewPanel);
      m_sceneViewPanel->raise();
    }
    if (m_storyGraphPanel) {
      addDockWidget(Qt::TopDockWidgetArea, m_storyGraphPanel);
    }
    if (m_scriptEditorPanel) {
      addDockWidget(Qt::TopDockWidgetArea, m_scriptEditorPanel);
    }
    if (m_sceneViewPanel && m_storyGraphPanel) {
      tabifyDockWidget(m_sceneViewPanel, m_storyGraphPanel);
    }
    if (m_sceneViewPanel && m_scriptEditorPanel) {
      tabifyDockWidget(m_sceneViewPanel, m_scriptEditorPanel);
    }

    if (m_inspectorPanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_inspectorPanel);
    }
    if (m_voiceManagerPanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_voiceManagerPanel);
    }
    if (m_localizationPanel) {
      addDockWidget(Qt::RightDockWidgetArea, m_localizationPanel);
    }
    if (m_inspectorPanel && m_voiceManagerPanel) {
      tabifyDockWidget(m_inspectorPanel, m_voiceManagerPanel);
    }
    if (m_inspectorPanel && m_localizationPanel) {
      tabifyDockWidget(m_inspectorPanel, m_localizationPanel);
    }

    if (m_consolePanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_consolePanel);
    }
    if (m_assetBrowserPanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_assetBrowserPanel);
    }
    if (m_issuesPanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_issuesPanel);
    }
    if (m_diagnosticsPanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_diagnosticsPanel);
    }
    if (m_timelinePanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_timelinePanel);
    }
    if (m_curveEditorPanel) {
      addDockWidget(Qt::BottomDockWidgetArea, m_curveEditorPanel);
    }
    if (m_consolePanel && m_assetBrowserPanel) {
      tabifyDockWidget(m_consolePanel, m_assetBrowserPanel);
    }
    if (m_consolePanel && m_issuesPanel) {
      tabifyDockWidget(m_consolePanel, m_issuesPanel);
    }
    if (m_consolePanel && m_diagnosticsPanel) {
      tabifyDockWidget(m_consolePanel, m_diagnosticsPanel);
    }
    if (m_consolePanel && m_timelinePanel) {
      tabifyDockWidget(m_consolePanel, m_timelinePanel);
    }
    if (m_consolePanel && m_curveEditorPanel) {
      tabifyDockWidget(m_consolePanel, m_curveEditorPanel);
    }
    if (m_consolePanel) {
      m_consolePanel->raise();
    }

    if (m_hierarchyPanel) {
      resizeDocks({m_hierarchyPanel}, {220}, Qt::Horizontal);
    }
    if (m_inspectorPanel) {
      resizeDocks({m_inspectorPanel}, {280}, Qt::Horizontal);
    }
    if (m_consolePanel) {
      resizeDocks({m_consolePanel}, {190}, Qt::Vertical);
    }
    break;
  }
  }
}

void NMMainWindow::toggleFocusMode(bool enabled) {
  if (enabled == m_focusModeEnabled) {
    if (enabled) {
      applyFocusModeLayout();
    }
    return;
  }

  m_focusModeEnabled = enabled;
  if (enabled) {
    m_focusGeometry = saveGeometry();
    m_focusState = saveState();
    applyFocusModeLayout();
  } else {
    if (!m_focusGeometry.isEmpty()) {
      restoreGeometry(m_focusGeometry);
    }
    if (!m_focusState.isEmpty()) {
      restoreState(m_focusState);
    } else {
      createDefaultLayout();
    }
  }
}

void NMMainWindow::applyFocusModeLayout() {
  QList<QDockWidget*> docks;
  docks.append(static_cast<QDockWidget*>(m_sceneViewPanel));
  docks.append(static_cast<QDockWidget*>(m_storyGraphPanel));
  docks.append(static_cast<QDockWidget*>(m_sceneDialogueGraphPanel));
  docks.append(static_cast<QDockWidget*>(m_inspectorPanel));
  docks.append(static_cast<QDockWidget*>(m_consolePanel));
  docks.append(static_cast<QDockWidget*>(m_assetBrowserPanel));
  docks.append(static_cast<QDockWidget*>(m_hierarchyPanel));
  docks.append(static_cast<QDockWidget*>(m_scenePalettePanel));
  docks.append(static_cast<QDockWidget*>(m_scriptEditorPanel));
  docks.append(static_cast<QDockWidget*>(m_scriptDocPanel));
  docks.append(static_cast<QDockWidget*>(m_playToolbarPanel));
  docks.append(static_cast<QDockWidget*>(m_debugOverlayPanel));
  docks.append(static_cast<QDockWidget*>(m_voiceManagerPanel));
  docks.append(static_cast<QDockWidget*>(m_voiceStudioPanel));
  docks.append(static_cast<QDockWidget*>(m_audioMixerPanel));
  docks.append(static_cast<QDockWidget*>(m_localizationPanel));
  docks.append(static_cast<QDockWidget*>(m_timelinePanel));
  docks.append(static_cast<QDockWidget*>(m_curveEditorPanel));
  docks.append(static_cast<QDockWidget*>(m_buildSettingsPanel));
  docks.append(static_cast<QDockWidget*>(m_issuesPanel));
  docks.append(static_cast<QDockWidget*>(m_diagnosticsPanel));

  for (auto* dock : docks) {
    if (!dock) {
      continue;
    }
    dock->setFloating(false);
    dock->hide();
    removeDockWidget(dock);
  }

  setCentralWidget(nullptr);

  if (m_playToolbarPanel) {
    addDockWidget(Qt::TopDockWidgetArea, m_playToolbarPanel);
    m_playToolbarPanel->show();
  }

  if (m_sceneViewPanel) {
    addDockWidget(Qt::TopDockWidgetArea, m_sceneViewPanel);
    m_sceneViewPanel->show();
    m_sceneViewPanel->raise();
  }

  if (m_inspectorPanel) {
    addDockWidget(Qt::RightDockWidgetArea, m_inspectorPanel);
    m_inspectorPanel->show();
  }

  if (m_assetBrowserPanel) {
    addDockWidget(Qt::BottomDockWidgetArea, m_assetBrowserPanel);
    m_assetBrowserPanel->show();
  }

  if (m_focusIncludeHierarchy && m_hierarchyPanel) {
    addDockWidget(Qt::LeftDockWidgetArea, m_hierarchyPanel);
    m_hierarchyPanel->show();
  }

  resizeDocks({m_inspectorPanel}, {300}, Qt::Horizontal);
  resizeDocks({m_assetBrowserPanel}, {200}, Qt::Vertical);
}

void NMMainWindow::applyDockLockState(bool locked) {
  m_layoutLocked = locked;

  QList<QDockWidget*> docks;
  docks.append(static_cast<QDockWidget*>(m_sceneViewPanel));
  docks.append(static_cast<QDockWidget*>(m_storyGraphPanel));
  docks.append(static_cast<QDockWidget*>(m_sceneDialogueGraphPanel));
  docks.append(static_cast<QDockWidget*>(m_inspectorPanel));
  docks.append(static_cast<QDockWidget*>(m_consolePanel));
  docks.append(static_cast<QDockWidget*>(m_assetBrowserPanel));
  docks.append(static_cast<QDockWidget*>(m_hierarchyPanel));
  docks.append(static_cast<QDockWidget*>(m_scenePalettePanel));
  docks.append(static_cast<QDockWidget*>(m_scriptEditorPanel));
  docks.append(static_cast<QDockWidget*>(m_scriptDocPanel));
  docks.append(static_cast<QDockWidget*>(m_playToolbarPanel));
  docks.append(static_cast<QDockWidget*>(m_debugOverlayPanel));
  docks.append(static_cast<QDockWidget*>(m_voiceManagerPanel));
  docks.append(static_cast<QDockWidget*>(m_voiceStudioPanel));
  docks.append(static_cast<QDockWidget*>(m_audioMixerPanel));
  docks.append(static_cast<QDockWidget*>(m_localizationPanel));
  docks.append(static_cast<QDockWidget*>(m_timelinePanel));
  docks.append(static_cast<QDockWidget*>(m_curveEditorPanel));
  docks.append(static_cast<QDockWidget*>(m_buildSettingsPanel));
  docks.append(static_cast<QDockWidget*>(m_issuesPanel));
  docks.append(static_cast<QDockWidget*>(m_diagnosticsPanel));

  for (auto* dock : docks) {
    if (!dock) {
      continue;
    }
    QDockWidget::DockWidgetFeatures features = QDockWidget::DockWidgetClosable;
    if (!m_layoutLocked) {
      features |= QDockWidget::DockWidgetMovable;
      if (m_floatAllowed) {
        features |= QDockWidget::DockWidgetFloatable;
      }
    }
    dock->setFeatures(features);
  }
}

void NMMainWindow::applyTabbedDockMode(bool enabled) {
  m_tabbedDockOnly = enabled;
  if (m_tabbedDockOnly) {
    setDockOptions(QMainWindow::AllowTabbedDocks | QMainWindow::AnimatedDocks);
  } else {
    setDockOptions(QMainWindow::AllowTabbedDocks | QMainWindow::AllowNestedDocks |
                   QMainWindow::GroupedDragging | QMainWindow::AnimatedDocks);
  }

  if (!enabled) {
    return;
  }

  QList<QDockWidget*> docks;
  docks.append(static_cast<QDockWidget*>(m_sceneViewPanel));
  docks.append(static_cast<QDockWidget*>(m_storyGraphPanel));
  docks.append(static_cast<QDockWidget*>(m_sceneDialogueGraphPanel));
  docks.append(static_cast<QDockWidget*>(m_inspectorPanel));
  docks.append(static_cast<QDockWidget*>(m_consolePanel));
  docks.append(static_cast<QDockWidget*>(m_assetBrowserPanel));
  docks.append(static_cast<QDockWidget*>(m_hierarchyPanel));
  docks.append(static_cast<QDockWidget*>(m_scenePalettePanel));
  docks.append(static_cast<QDockWidget*>(m_scriptEditorPanel));
  docks.append(static_cast<QDockWidget*>(m_scriptDocPanel));
  docks.append(static_cast<QDockWidget*>(m_playToolbarPanel));
  docks.append(static_cast<QDockWidget*>(m_debugOverlayPanel));
  docks.append(static_cast<QDockWidget*>(m_voiceManagerPanel));
  docks.append(static_cast<QDockWidget*>(m_voiceStudioPanel));
  docks.append(static_cast<QDockWidget*>(m_audioMixerPanel));
  docks.append(static_cast<QDockWidget*>(m_localizationPanel));
  docks.append(static_cast<QDockWidget*>(m_timelinePanel));
  docks.append(static_cast<QDockWidget*>(m_curveEditorPanel));
  docks.append(static_cast<QDockWidget*>(m_buildSettingsPanel));
  docks.append(static_cast<QDockWidget*>(m_issuesPanel));
  docks.append(static_cast<QDockWidget*>(m_diagnosticsPanel));

  QHash<Qt::DockWidgetArea, QDockWidget*> anchors;
  for (auto* dock : docks) {
    if (!dock || !dock->isVisible()) {
      continue;
    }
    Qt::DockWidgetArea area = dockWidgetArea(dock);
    if (!anchors.contains(area)) {
      anchors.insert(area, dock);
    } else {
      tabifyDockWidget(anchors.value(area), dock);
    }
  }
}

void NMMainWindow::applyFloatAllowed(bool allowed) {
  m_floatAllowed = allowed;
  applyDockLockState(m_layoutLocked);
}

void NMMainWindow::saveCustomLayout() {
  QSettings settings("NovelMind", "Editor");
  settings.setValue("layout/custom/geometry", saveGeometry());
  settings.setValue("layout/custom/state", saveState());
  setStatusMessage(tr("Layout saved"));
}

void NMMainWindow::loadCustomLayout() {
  QSettings settings("NovelMind", "Editor");
  const QByteArray geometry = settings.value("layout/custom/geometry").toByteArray();
  const QByteArray state = settings.value("layout/custom/state").toByteArray();
  if (geometry.isEmpty() || state.isEmpty()) {
    setStatusMessage(tr("No saved layout found"), 2000);
    return;
  }
  restoreGeometry(geometry);
  restoreState(state);
  setStatusMessage(tr("Layout loaded"), 2000);
}

void NMMainWindow::saveLayout() {
  QSettings settings("NovelMind", "Editor");
  settings.setValue("mainwindow/geometry", saveGeometry());
  settings.setValue("mainwindow/state", saveState());
}

void NMMainWindow::restoreLayout() {
  QSettings settings("NovelMind", "Editor");

  // Restore geometry
  QByteArray geometry = settings.value("mainwindow/geometry").toByteArray();
  if (!geometry.isEmpty()) {
    restoreGeometry(geometry);
  }

  // Restore dock state
  QByteArray state = settings.value("mainwindow/state").toByteArray();
  if (!state.isEmpty()) {
    restoreState(state);
  }

  // Ensure all panels are at least created and available
  // Even if they were hidden in saved state, they should be accessible via View
  // menu
  if (m_sceneViewPanel && !m_sceneViewPanel->isVisible()) {
    m_actionToggleSceneView->setChecked(false);
  }
  if (m_storyGraphPanel && !m_storyGraphPanel->isVisible()) {
    m_actionToggleStoryGraph->setChecked(false);
  }
  if (m_inspectorPanel && !m_inspectorPanel->isVisible()) {
    m_actionToggleInspector->setChecked(false);
  }
  if (m_consolePanel && !m_consolePanel->isVisible()) {
    m_actionToggleConsole->setChecked(false);
  }
  if (m_issuesPanel && !m_issuesPanel->isVisible()) {
    m_actionToggleIssues->setChecked(false);
  }
  if (m_assetBrowserPanel && !m_assetBrowserPanel->isVisible()) {
    m_actionToggleAssetBrowser->setChecked(false);
  }
  if (m_voiceManagerPanel && !m_voiceManagerPanel->isVisible()) {
    m_actionToggleVoiceManager->setChecked(false);
  }
  if (m_localizationPanel && !m_localizationPanel->isVisible()) {
    m_actionToggleLocalization->setChecked(false);
  }
  if (m_timelinePanel && !m_timelinePanel->isVisible()) {
    m_actionToggleTimeline->setChecked(false);
  }
  if (m_curveEditorPanel && !m_curveEditorPanel->isVisible()) {
    m_actionToggleCurveEditor->setChecked(false);
  }
  if (m_buildSettingsPanel && !m_buildSettingsPanel->isVisible()) {
    m_actionToggleBuildSettings->setChecked(false);
  }
  if (m_scenePalettePanel && !m_scenePalettePanel->isVisible()) {
    m_actionToggleScenePalette->setChecked(false);
  }
  if (m_hierarchyPanel && !m_hierarchyPanel->isVisible()) {
    m_actionToggleHierarchy->setChecked(false);
  }
  if (m_scriptEditorPanel && !m_scriptEditorPanel->isVisible()) {
    m_actionToggleScriptEditor->setChecked(false);
  }
  if (m_scriptDocPanel && !m_scriptDocPanel->isVisible()) {
    m_actionToggleScriptDocs->setChecked(false);
  }
  if (m_debugOverlayPanel && !m_debugOverlayPanel->isVisible()) {
    m_actionToggleDebugOverlay->setChecked(false);
  }
}

void NMMainWindow::resetToDefaultLayout() {
  // Remove saved layout
  QSettings settings("NovelMind", "Editor");
  settings.remove("mainwindow/geometry");
  settings.remove("mainwindow/state");

  // Recreate default layout
  if (m_actionFocusMode && m_actionFocusMode->isChecked()) {
    m_actionFocusMode->setChecked(false);
  }
  createDefaultLayout();
}

// ============================================================================
// D2: Workspace Management UI Implementation
// ============================================================================

void NMMainWindow::populateWorkspaceMenu() {
  if (!m_workspaceMenu) {
    return;
  }

  // Get list of all workspace actions before the separator
  const QList<QAction*> allActions = m_workspaceMenu->actions();

  // Find the separator that marks the start of custom workspace section
  int customSectionIndex = -1;
  int managementSectionIndex = -1;
  for (int i = 0; i < allActions.size(); ++i) {
    if (allActions[i]->isSeparator()) {
      if (customSectionIndex == -1 && i > 0 && allActions[i - 1]->text().contains("Legacy")) {
        customSectionIndex = i;
      } else if (customSectionIndex != -1 && managementSectionIndex == -1) {
        managementSectionIndex = i;
        break;
      }
    }
  }

  if (customSectionIndex == -1 || managementSectionIndex == -1) {
    return;
  }

  // Remove existing custom workspace actions (between the two separators)
  for (int i = managementSectionIndex - 1; i > customSectionIndex; --i) {
    QAction* action = allActions[i];
    if (!action->isSeparator()) {
      m_workspaceMenu->removeAction(action);
      delete action;
    }
  }

  // Get custom presets from settings
  QSettings settings("NovelMind", "Editor");
  settings.beginGroup("workspace/custom");
  const QStringList customPresets = settings.childGroups();
  settings.endGroup();

  // Add custom workspace actions
  if (!customPresets.isEmpty()) {
    QAction* separatorAction = allActions[customSectionIndex];
    auto& iconMgr = NMIconManager::instance();

    for (const QString& presetName : customPresets) {
      QAction* customAction =
          new QAction(iconMgr.getIcon("panel-scene", 16), presetName, m_workspaceMenu);
      customAction->setToolTip(tr("Load custom workspace: %1").arg(presetName));

      connect(customAction, &QAction::triggered, this,
              [this, presetName]() { onLoadCustomWorkspace(presetName); });

      m_workspaceMenu->insertAction(separatorAction, customAction);
      separatorAction = customAction; // Insert after this action next time
    }
  }
}

void NMMainWindow::onSaveWorkspaceAs() {
  bool ok = false;
  const QString name = NMInputDialog::getText(this, tr("Save Workspace As"),
                                              tr("Enter a name for this workspace preset:"),
                                              QLineEdit::Normal, QString(), &ok);

  if (!ok || name.isEmpty()) {
    return;
  }

  // Check if name conflicts with built-in presets
  const QStringList builtInNames = {
      tr("Default"),       tr("Story / Script"), tr("Scene / Animation"),
      tr("Audio / Voice"), tr("Story"),          tr("Scene"),
      tr("Script"),        tr("Developer"),      tr("Compact")};

  if (builtInNames.contains(name)) {
    NMMessageDialog::showWarning(this, tr("Invalid Name"),
                                 tr("The name '%1' is reserved for a built-in workspace. "
                                    "Please choose a different name.")
                                     .arg(name));
    return;
  }

  // Save the current workspace
  saveWorkspacePreset(name);

  // Refresh the menu
  populateWorkspaceMenu();
}

void NMMainWindow::onLoadCustomWorkspace(const QString& name) {
  if (!loadWorkspacePreset(name)) {
    NMMessageDialog::showError(this, tr("Load Failed"),
                               tr("Failed to load workspace preset '%1'.").arg(name));
  }
}

void NMMainWindow::showManageWorkspacesDialog() {
  // Get custom presets from settings
  QSettings settings("NovelMind", "Editor");
  settings.beginGroup("workspace/custom");
  const QStringList customPresets = settings.childGroups();
  settings.endGroup();

  if (customPresets.isEmpty()) {
    NMMessageDialog::showInfo(this, tr("No Custom Workspaces"),
                              tr("You have not created any custom workspace presets yet.\n\n"
                                 "Use 'Save Current Layout As...' to create a custom workspace."));
    return;
  }

  // Create a simple list dialog for managing workspaces
  bool ok = false;
  const QString selected = NMInputDialog::getItem(this, tr("Manage Workspaces"),
                                                  tr("Select a custom workspace to delete:"),
                                                  customPresets, 0, false, &ok);

  if (!ok || selected.isEmpty()) {
    return;
  }

  // Confirm deletion
  const auto result =
      NMMessageDialog::showQuestion(this, tr("Delete Workspace"),
                                    tr("Are you sure you want to delete the workspace '%1'?\n\n"
                                       "This action cannot be undone.")
                                        .arg(selected),
                                    {NMDialogButton::Yes, NMDialogButton::No}, NMDialogButton::No);

  if (result == NMDialogButton::Yes) {
    // Delete the workspace
    settings.beginGroup("workspace/custom");
    settings.remove(selected);
    settings.endGroup();

    setStatusMessage(tr("Workspace '%1' deleted").arg(selected), 2000);

    // Refresh the menu
    populateWorkspaceMenu();
  }
}

} // namespace NovelMind::editor::qt
