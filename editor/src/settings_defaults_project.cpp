/**
 * @file settings_defaults_project.cpp
 * @brief Project default settings registration
 */

#include "NovelMind/editor/settings_registry.hpp"

namespace NovelMind::editor {

void NMSettingsRegistry::registerProjectDefaults() {
  // ========== Project Info ==========
  registerSetting({
      "project.name",
      "Project Name",
      "Name of the visual novel project",
      "Project/General",
      SettingType::String,
      SettingScope::Project,
      std::string("My Visual Novel"),
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "project.author",
      "Author",
      "Project author/creator",
      "Project/General",
      SettingType::String,
      SettingScope::Project,
      std::string(""),
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  // ========== Localization ==========
  registerSetting({
      "project.localization.default_locale",
      "Default Locale",
      "Default language for the project",
      "Project/Localization",
      SettingType::Enum,
      SettingScope::Project,
      std::string("en"),
      {"en", "ja", "zh", "ko", "ru", "fr", "de", "es"},
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "project.localization.fallback_locale",
      "Fallback Locale",
      "Language to use when translation is missing",
      "Project/Localization",
      SettingType::Enum,
      SettingScope::Project,
      std::string("en"),
      {"en", "ja", "zh", "ko", "ru", "fr", "de", "es"},
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  // ========== Audio ==========
  registerSetting({
      "project.audio.sample_rate",
      "Sample Rate (Hz)",
      "Audio sample rate for the project",
      "Project/Audio",
      SettingType::Enum,
      SettingScope::Project,
      std::string("48000"),
      {"44100", "48000", "96000"},
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "project.audio.channels",
      "Channels",
      "Audio channel configuration",
      "Project/Audio",
      SettingType::Enum,
      SettingScope::Project,
      std::string("stereo"),
      {"mono", "stereo"},
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  // ========== Timeline ==========
  registerSetting({
      "project.timeline.default_fps",
      "Default FPS",
      "Frames per second for timeline",
      "Project/Timeline",
      SettingType::IntRange,
      SettingScope::Project,
      60,
      {},
      24.0f,  // min
      120.0f, // max
      {},     // validator
      false,  // requiresRestart
      false,  // isAdvanced
      {}      // tags
  });

  registerSetting({
      "project.timeline.snap_enabled",
      "Enable Snapping",
      "Snap to grid in timeline",
      "Project/Timeline",
      SettingType::Bool,
      SettingScope::Project,
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
      "project.timeline.snap_interval",
      "Snap Interval (frames)",
      "Grid interval for timeline snapping",
      "Project/Timeline",
      SettingType::IntRange,
      SettingScope::Project,
      5,
      {},
      1.0f,  // min
      60.0f, // max
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  // ========== Build ==========
  registerSetting({
      "project.build.output_format",
      "Output Format",
      "Build output format",
      "Project/Build",
      SettingType::Enum,
      SettingScope::Project,
      std::string("standalone"),
      {"standalone", "web", "mobile"},
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  registerSetting({
      "project.build.compression",
      "Compression",
      "Enable asset compression in builds",
      "Project/Build",
      SettingType::Bool,
      SettingScope::Project,
      true,
      {},    // enumOptions
      0.0f,  // minValue
      1.0f,  // maxValue
      {},    // validator
      false, // requiresRestart
      false, // isAdvanced
      {}     // tags
  });

  // ========== Script VM Security ==========
  registerSetting({
      "project.vm.security.max_stack_size",
      "Max Stack Size",
      "Maximum VM stack size (entries)",
      "Project/Script Security",
      SettingType::IntRange,
      SettingScope::Project,
      1024,
      {},
      256.0f,  // min
      8192.0f, // max
      {},      // validator
      false,   // requiresRestart
      false,   // isAdvanced
      {}       // tags
  });

  registerSetting({
      "project.vm.security.max_call_depth",
      "Max Call Depth",
      "Maximum function call depth",
      "Project/Script Security",
      SettingType::IntRange,
      SettingScope::Project,
      64,
      {},
      16.0f,  // min
      256.0f, // max
      {},     // validator
      false,  // requiresRestart
      false,  // isAdvanced
      {}      // tags
  });

  registerSetting({
      "project.vm.security.max_instructions_per_step",
      "Max Instructions Per Step",
      "Maximum instructions executed per update step",
      "Project/Script Security",
      SettingType::IntRange,
      SettingScope::Project,
      10000,
      {},
      1000.0f,   // min
      100000.0f, // max
      {},        // validator
      false,     // requiresRestart
      false,     // isAdvanced
      {}         // tags
  });

  registerSetting({
      "project.vm.security.max_string_length",
      "Max String Length",
      "Maximum string length (characters)",
      "Project/Script Security",
      SettingType::IntRange,
      SettingScope::Project,
      65536,
      {},
      1024.0f,    // min
      1048576.0f, // max
      {},         // validator
      false,      // requiresRestart
      false,      // isAdvanced
      {}          // tags
  });

  registerSetting({
      "project.vm.security.max_variables",
      "Max Variables",
      "Maximum number of variables",
      "Project/Script Security",
      SettingType::IntRange,
      SettingScope::Project,
      1024,
      {},
      128.0f,  // min
      8192.0f, // max
      {},      // validator
      false,   // requiresRestart
      false,   // isAdvanced
      {}       // tags
  });

  registerSetting({
      "project.vm.security.max_loop_iterations",
      "Max Loop Iterations",
      "Maximum loop iterations (infinite loop detection)",
      "Project/Script Security",
      SettingType::IntRange,
      SettingScope::Project,
      100000,
      {},
      10000.0f,    // min
      10000000.0f, // max
      {},          // validator
      false,       // requiresRestart
      false,       // isAdvanced
      {}           // tags
  });

  registerSetting({
      "project.vm.security.allow_native_calls",
      "Allow Native Calls",
      "Allow calling native C++ functions from scripts",
      "Project/Script Security",
      SettingType::Bool,
      SettingScope::Project,
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
      "project.vm.security.allow_file_access",
      "Allow File Access",
      "Allow scripts to read/write files",
      "Project/Script Security",
      SettingType::Bool,
      SettingScope::Project,
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
      "project.vm.security.allow_network_access",
      "Allow Network Access",
      "Allow scripts to make network requests",
      "Project/Script Security",
      SettingType::Bool,
      SettingScope::Project,
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
