/**
 * @file settings_defaults_editor.cpp
 * @brief Editor default settings registration
 */

#include "NovelMind/editor/settings_registry.hpp"

namespace NovelMind::editor {

void NMSettingsRegistry::registerEditorDefaults() {
  // ========== General ==========
  registerSetting({
      "editor.general.autosave",
      "Auto Save",
      "Automatically save project at intervals",
      "Editor/General",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.general.autosave_interval",
      "Auto Save Interval (seconds)",
      "Time between automatic saves",
      "Editor/General",
      SettingType::IntRange,
      SettingScope::User,
      300,     // default
      {},      // enum options
      60.0f,   // min
      3600.0f, // max
      {},      // validator
      false,   // requiresRestart
      false,   // isAdvanced
      {}       // tags
  });

  registerSetting({
      "editor.general.confirm_on_close",
      "Confirm on Close",
      "Ask for confirmation when closing with unsaved changes",
      "Editor/General",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.general.reopen_last_project",
      "Reopen Last Project",
      "Automatically reopen the last project on startup",
      "Editor/General",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  // ========== Appearance ==========
  registerSetting({
      "editor.appearance.theme",
      "Theme",
      "Editor color theme",
      "Editor/Appearance",
      SettingType::Enum,
      SettingScope::User,
      std::string("dark"),
      {"dark", "light"},
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.appearance.ui_scale",
      "UI Scale",
      "Interface scaling factor",
      "Editor/Appearance",
      SettingType::FloatRange,
      SettingScope::User,
      1.0f,
      {},
      0.5f,  // min
      2.0f,  // max
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.appearance.show_tooltips",
      "Show Tooltips",
      "Display helpful tooltips on hover",
      "Editor/Appearance",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  // ========== Workspace ==========
  registerSetting({
      "editor.workspace.default_layout",
      "Default Layout",
      "Workspace layout to use for new projects",
      "Editor/Workspace",
      SettingType::Enum,
      SettingScope::User,
      std::string("default"),
      {"default", "story_focused", "scene_focused", "script_focused",
       "minimal"},
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.workspace.remember_layout",
      "Remember Layout",
      "Restore window layout from last session",
      "Editor/Workspace",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  // ========== Script Editor ==========
  registerSetting({
      "editor.script.show_line_numbers",
      "Show Line Numbers",
      "Display line numbers in script editor",
      "Editor/Script Editor",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.script.word_wrap",
      "Word Wrap",
      "Wrap long lines in script editor",
      "Editor/Script Editor",
      SettingType::Bool,
      SettingScope::User,
      false,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.script.tab_size",
      "Tab Size",
      "Number of spaces per tab",
      "Editor/Script Editor",
      SettingType::IntRange,
      SettingScope::User,
      4,
      {},
      2.0f,  // min
      8.0f,  // max
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.script.insert_spaces",
      "Insert Spaces",
      "Use spaces instead of tabs",
      "Editor/Script Editor",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.script.auto_complete",
      "Auto Complete",
      "Enable code completion suggestions",
      "Editor/Script Editor",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.script.highlight_current_line",
      "Highlight Current Line",
      "Highlight the line with the cursor",
      "Editor/Script Editor",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  // ========== Script Editor - Font Settings ==========
  registerSetting({
      "editor.script.font_family",
      "Font Family",
      "Font family for script editor (e.g. 'Fira Code', 'Consolas', 'Courier New')",
      "Editor/Script Editor/Font",
      SettingType::String,
      SettingScope::User,
      std::string("monospace"),
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {"font", "editor"}     // tags
  });

  registerSetting({
      "editor.script.font_size",
      "Font Size",
      "Font size in pixels",
      "Editor/Script Editor/Font",
      SettingType::IntRange,
      SettingScope::User,
      14,
      {},
      8.0f,   // min
      32.0f,  // max
      {},     // validator
      false,  // requiresRestart
      false,  // isAdvanced
      {"font", "editor"}     // tags
  });

  registerSetting({
      "editor.script.font_ligatures",
      "Enable Font Ligatures",
      "Enable font ligatures for programming fonts (e.g. -> => !=)",
      "Editor/Script Editor/Font",
      SettingType::Bool,
      SettingScope::User,
      false,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {"font", "editor"}     // tags
  });

  // ========== Script Editor - Display Settings ==========
  registerSetting({
      "editor.script.show_minimap",
      "Show Minimap",
      "Display code minimap on the right side",
      "Editor/Script Editor/Display",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {"display", "editor"}     // tags
  });

  registerSetting({
      "editor.script.show_breadcrumbs",
      "Show Breadcrumbs",
      "Display breadcrumb navigation bar",
      "Editor/Script Editor/Display",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {"display", "editor"}     // tags
  });

  registerSetting({
      "editor.script.show_whitespace",
      "Show Whitespace Characters",
      "Display whitespace characters (spaces, tabs)",
      "Editor/Script Editor/Display",
      SettingType::Bool,
      SettingScope::User,
      false,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {"display", "editor"}     // tags
  });

  // ========== Script Editor - Indentation Settings ==========
  registerSetting({
      "editor.script.auto_indent",
      "Auto Indent New Lines",
      "Automatically indent new lines based on context",
      "Editor/Script Editor/Indentation",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {"indentation", "editor"}     // tags
  });

  // ========== Script Editor - Diagnostics Settings ==========
  registerSetting({
      "editor.script.diagnostic_delay",
      "Diagnostic Delay (ms)",
      "Delay before running validation after typing",
      "Editor/Script Editor/Diagnostics",
      SettingType::IntRange,
      SettingScope::User,
      600,
      {},
      100.0f,  // min
      2000.0f, // max
      {},      // validator
      false,   // requiresRestart
      false,   // isAdvanced
      {"diagnostics", "editor"}     // tags
  });

  registerSetting({
      "editor.script.show_inline_errors",
      "Show Inline Errors",
      "Display error messages inline in the editor",
      "Editor/Script Editor/Diagnostics",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {"diagnostics", "editor"}     // tags
  });

  registerSetting({
      "editor.script.show_error_underlines",
      "Show Error Underlines",
      "Underline errors and warnings in the text",
      "Editor/Script Editor/Diagnostics",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {"diagnostics", "editor"}     // tags
  });

  registerSetting({
      "editor.script.validate_on_save",
      "Validate on Save",
      "Run validation when saving files",
      "Editor/Script Editor/Diagnostics",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {"diagnostics", "editor"}     // tags
  });

  registerSetting({
      "editor.script.validate_on_focus_change",
      "Validate on Focus Change",
      "Run validation when switching between tabs",
      "Editor/Script Editor/Diagnostics",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {"diagnostics", "editor"}     // tags
  });

  registerSetting({
      "editor.script.max_errors_displayed",
      "Max Errors Displayed",
      "Maximum number of errors to show in the error list",
      "Editor/Script Editor/Diagnostics",
      SettingType::IntRange,
      SettingScope::User,
      100,
      {},
      10.0f,   // min
      1000.0f, // max
      {},      // validator
      false,   // requiresRestart
      true,    // isAdvanced
      {"diagnostics", "editor"}     // tags
  });

  // ========== Script Editor - Advanced Settings ==========
  registerSetting({
      "editor.script.auto_save_on_focus_loss",
      "Auto-Save on Focus Loss",
      "Automatically save when switching to another tab or window",
      "Editor/Script Editor/Advanced",
      SettingType::Bool,
      SettingScope::User,
      false,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      true,  // isAdvanced
      {"auto-save", "editor"}     // tags
  });

  registerSetting({
      "editor.script.auto_save_delay",
      "Auto-Save Delay (seconds)",
      "Time to wait before auto-saving after changes",
      "Editor/Script Editor/Advanced",
      SettingType::IntRange,
      SettingScope::User,
      30,
      {},
      5.0f,    // min
      300.0f,  // max
      {},      // validator
      false,   // requiresRestart
      true,    // isAdvanced
      {"auto-save", "editor"}     // tags
  });

  registerSetting({
      "editor.script.restore_open_files",
      "Restore Open Files on Startup",
      "Reopen files that were open in the previous session",
      "Editor/Script Editor/Advanced",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      true,  // isAdvanced
      {"session", "editor"}     // tags
  });

  registerSetting({
      "editor.script.restore_cursor_position",
      "Restore Cursor Position",
      "Restore cursor position when reopening files",
      "Editor/Script Editor/Advanced",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      true,  // isAdvanced
      {"session", "editor"}     // tags
  });

  registerSetting({
      "editor.script.file_watcher_enabled",
      "File Watcher",
      "Watch for external file changes",
      "Editor/Script Editor/Advanced",
      SettingType::Enum,
      SettingScope::User,
      std::string("enabled"),
      {"enabled", "disabled", "prompt"},
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      true,  // isAdvanced
      {"file-watcher", "editor"}     // tags
  });

  registerSetting({
      "editor.script.virtual_rendering_threshold",
      "Virtual Rendering Threshold (lines)",
      "Use virtual rendering for files larger than this many lines",
      "Editor/Script Editor/Advanced",
      SettingType::IntRange,
      SettingScope::User,
      1000,
      {},
      100.0f,   // min
      10000.0f, // max
      {},       // validator
      false,    // requiresRestart
      true,     // isAdvanced
      {"performance", "editor"}     // tags
  });

  registerSetting({
      "editor.script.minimap_update_delay",
      "Minimap Update Delay (ms)",
      "Delay before updating minimap after changes",
      "Editor/Script Editor/Advanced",
      SettingType::IntRange,
      SettingScope::User,
      100,
      {},
      0.0f,    // min
      500.0f,  // max
      {},      // validator
      false,   // requiresRestart
      true,    // isAdvanced
      {"performance", "minimap", "editor"}     // tags
  });

  // ========== Audio ==========
  registerSetting({
      "editor.audio.input_device",
      "Input Device",
      "Microphone for voice recording",
      "Editor/Audio",
      SettingType::String,
      SettingScope::User,
      std::string("default"),
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.audio.output_device",
      "Output Device",
      "Audio playback device",
      "Editor/Audio",
      SettingType::String,
      SettingScope::User,
      std::string("default"),
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.audio.buffer_size",
      "Buffer Size",
      "Audio buffer size (lower = less latency, more CPU)",
      "Editor/Audio",
      SettingType::Enum,
      SettingScope::User,
      std::string("512"),
      {"128", "256", "512", "1024", "2048"},
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  // ========== Preview ==========
  registerSetting({
      "editor.preview.scale",
      "Preview Scale",
      "Scale factor for preview window",
      "Editor/Preview",
      SettingType::FloatRange,
      SettingScope::User,
      1.0f,
      {},
      0.25f, // min
      2.0f,  // max
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.preview.show_fps",
      "Show FPS",
      "Display FPS counter in preview",
      "Editor/Preview",
      SettingType::Bool,
      SettingScope::User,
      false,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.preview.vsync",
      "V-Sync",
      "Enable vertical synchronization",
      "Editor/Preview",
      SettingType::Bool,
      SettingScope::User,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  // ========== Diagnostics ==========
  registerSetting({
      "editor.diagnostics.log_level",
      "Log Level",
      "Minimum severity level for logging",
      "Editor/Diagnostics",
      SettingType::Enum,
      SettingScope::User,
      std::string("info"),
      {"trace", "debug", "info", "warning", "error"},
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "editor.diagnostics.show_performance_overlay",
      "Show Performance Overlay",
      "Display performance metrics overlay",
      "Editor/Diagnostics",
      SettingType::Bool,
      SettingScope::User,
      false,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });
}

} // namespace NovelMind::editor
