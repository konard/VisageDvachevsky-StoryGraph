/**
 * @file game_launcher.cpp
 * @brief Game Launcher implementation
 */

#include "NovelMind/runtime/game_launcher.hpp"
#include "NovelMind/core/logger.hpp"
#include "NovelMind/input/input_manager.hpp"
#include "NovelMind/localization/localization_manager.hpp"
#include "NovelMind/scripting/script_runtime.hpp"
#include "NovelMind/vfs/multi_pack_manager.hpp"
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace NovelMind::runtime {

std::string LauncherError::format() const {
  std::string result = "[" + code + "] " + message;
  if (!details.empty()) {
    result += "\nDetails: " + details;
  }
  if (!suggestion.empty()) {
    result += "\nSuggestion: " + suggestion;
  }
  return result;
}

GameLauncher::GameLauncher() = default;

GameLauncher::~GameLauncher() {
  if (m_state == LauncherState::Running) {
    quit();
  }
}

Result<void> GameLauncher::initialize(int argc, char* argv[]) {
  m_options = parseArgs(argc, argv);

  if (m_options.help) {
    printHelp(argv[0]);
    m_state = LauncherState::Ready;
    return Result<void>::ok();
  }

  if (m_options.version) {
    printVersion();
    m_state = LauncherState::Ready;
    return Result<void>::ok();
  }

  // Determine base path
  std::string basePath = getExecutableDirectory();
  if (!m_options.configOverride.empty()) {
    fs::path configPath(m_options.configOverride);
    if (configPath.has_parent_path()) {
      basePath = configPath.parent_path().string();
    }
  }

  return initialize(basePath, m_options);
}

Result<void> GameLauncher::initialize(const std::string& basePath, const LaunchOptions& options) {
  setState(LauncherState::Initializing);
  m_basePath = basePath;
  m_options = options;

  // Normalize path
  if (!m_basePath.empty() && m_basePath.back() != '/' && m_basePath.back() != '\\') {
    m_basePath += '/';
  }

  // Initialize in order
  auto result = initializeLogging();
  if (result.isError()) {
    setError("INIT_LOG", "Failed to initialize logging", result.error(),
             "Check write permissions in the logs directory");
    return result;
  }

  result = initializeConfig();
  if (result.isError()) {
    setError("INIT_CONFIG", "Failed to load configuration", result.error(),
             "Check that config/runtime_config.json exists and is valid JSON");
    return result;
  }

  result = initializeDirectories();
  if (result.isError()) {
    setError("INIT_DIRS", "Failed to create directories", result.error(),
             "Check write permissions in the game directory");
    return result;
  }

  result = initializePacks();
  if (result.isError()) {
    setError("INIT_PACKS", "Failed to load resource packs", result.error(),
             "Check that packs_index.json exists and pack files are present");
    return result;
  }

  result = initializeInput();
  if (result.isError()) {
    setError("INIT_INPUT", "Failed to initialize input system", result.error());
    return result;
  }

  result = initializeSaveSystem();
  if (result.isError()) {
    setError("INIT_SAVE", "Failed to initialize save system", result.error(),
             "Check write permissions in the saves directory");
    return result;
  }

  result = initializeLocalization();
  if (result.isError()) {
    setError("INIT_LOCALE", "Failed to initialize localization", result.error(),
             "Check that localization files exist for the selected language");
    return result;
  }

  result = initializeScriptRuntime();
  if (result.isError()) {
    setError("INIT_SCRIPT", "Failed to initialize script runtime", result.error(),
             "Check that compiled scripts are present in the packs");
    return result;
  }

  setState(LauncherState::Ready);
  logInfo("Game launcher initialized successfully");

  return Result<void>::ok();
}

i32 GameLauncher::run() {
  if (m_state != LauncherState::Ready) {
    logError("Cannot run: launcher not in Ready state");
    return 1;
  }

  // Check if help or version was requested
  if (m_options.help || m_options.version) {
    return 0;
  }

  setState(LauncherState::Running);
  m_running = true;

  logInfo("Starting game: " + m_configManager->getConfig().game.name);

  try {
    mainLoop();
  } catch (const std::exception& e) {
    setError("RUNTIME", "Runtime error", e.what());
    logError("Runtime exception: " + std::string(e.what()));
    return 1;
  }

  setState(LauncherState::ShuttingDown);
  logInfo("Game exited normally");

  return 0;
}

void GameLauncher::quit() {
  m_running = false;
}

bool GameLauncher::isRunning() const {
  return m_running;
}

void GameLauncher::showError(const LauncherError& error) {
  m_lastError = error;

  // Log the error
  logError(error.format());

  // Print to console
  std::cerr << "\n=== Error ===\n";
  std::cerr << error.format() << "\n";
  std::cerr << "=============\n\n";

  // In a full implementation, this would also show a GUI dialog
  if (m_onError) {
    m_onError(error);
  }
}

void GameLauncher::showError(const std::string& error) {
  LauncherError err;
  err.code = "ERROR";
  err.message = error;
  showError(err);
}

ConfigManager* GameLauncher::getConfigManager() {
  return m_configManager.get();
}

GameSettings* GameLauncher::getGameSettings() {
  return m_gameSettings.get();
}

vfs::MultiPackManager* GameLauncher::getPackManager() {
  return m_packManager.get();
}

scripting::ScriptRuntime* GameLauncher::getScriptRuntime() {
  return m_scriptRuntime.get();
}

localization::LocalizationManager* GameLauncher::getLocalizationManager() {
  return m_localizationManager.get();
}

input::InputManager* GameLauncher::getInputManager() {
  return m_inputManager.get();
}

const RuntimeConfig& GameLauncher::getConfig() const {
  static RuntimeConfig defaultConfig;
  if (m_configManager) {
    return m_configManager->getConfig();
  }
  return defaultConfig;
}

void GameLauncher::setOnError(OnLauncherError callback) {
  m_onError = std::move(callback);
}

void GameLauncher::setOnStateChanged(OnLauncherStateChanged callback) {
  m_onStateChanged = std::move(callback);
}

void GameLauncher::printVersion() {
  std::cout << "NovelMind Game Launcher version " << NOVELMIND_VERSION_MAJOR << "."
            << NOVELMIND_VERSION_MINOR << "." << NOVELMIND_VERSION_PATCH << "\n";
  std::cout << "A modern visual novel engine\n";
  std::cout << "Copyright (c) 2024 NovelMind Team\n";
}

void GameLauncher::printHelp(const char* programName) {
  std::cout << "Usage: " << programName << " [options]\n\n";
  std::cout << "NovelMind Game Launcher - Play visual novels.\n\n";
  std::cout << "Options:\n";
  std::cout << "  --config <path>   Override config file path\n";
  std::cout << "  --lang <locale>   Override language (e.g., en, ru)\n";
  std::cout << "  --scene <name>    Start from a specific scene\n";
  std::cout << "  --debug           Enable debug mode\n";
  std::cout << "  --verbose         Verbose logging\n";
  std::cout << "  --windowed        Disable fullscreen\n";
  std::cout << "  -h, --help        Show this help message\n";
  std::cout << "  --version         Show version information\n\n";
  std::cout << "The launcher automatically loads configuration from:\n";
  std::cout << "  config/runtime_config.json - Game settings\n";
  std::cout << "  config/runtime_user.json   - User preferences\n";
}

std::string GameLauncher::getExecutableDirectory() {
  try {
    // Try to get the executable path
#ifdef _WIN32
    char buffer[MAX_PATH];
    GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    fs::path exePath(buffer);
#else
    fs::path exePath = fs::read_symlink("/proc/self/exe");
#endif
    return exePath.parent_path().string() + "/";
  } catch (...) {
    // Fall back to current directory
    return fs::current_path().string() + "/";
  }
}

LaunchOptions GameLauncher::parseArgs(int argc, char* argv[]) {
  LaunchOptions opts;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      opts.help = true;
    } else if (arg == "--version") {
      opts.version = true;
    } else if (arg == "--config" && i + 1 < argc) {
      opts.configOverride = argv[++i];
    } else if (arg == "--lang" && i + 1 < argc) {
      opts.langOverride = argv[++i];
    } else if (arg == "--scene" && i + 1 < argc) {
      opts.sceneOverride = argv[++i];
    } else if (arg == "--debug") {
      opts.debugMode = true;
    } else if (arg == "--verbose" || arg == "-v") {
      opts.verbose = true;
    } else if (arg == "--windowed") {
      opts.noFullscreen = true;
    }
  }

  return opts;
}

Result<void> GameLauncher::initializeLogging() {
  auto& logger = core::Logger::instance();

  // Set log level based on options
  if (m_options.verbose) {
    logger.setLevel(core::LogLevel::Debug);
  } else if (m_options.debugMode) {
    logger.setLevel(core::LogLevel::Info);
  } else {
    logger.setLevel(core::LogLevel::Warning);
  }

  // Create logs directory
  std::string logsDir = m_basePath + "logs/";
  try {
    fs::create_directories(logsDir);
  } catch (const std::exception& e) {
    return Result<void>::error(std::string("Failed to create logs dir: ") + e.what());
  }

  // Set output file
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  char timeStr[64];
  std::strftime(timeStr, sizeof(timeStr), "%Y%m%d_%H%M%S", std::localtime(&time));

  std::string logFile = logsDir + "game_" + timeStr + ".log";
  logger.setOutputFile(logFile);

  logInfo("Logging initialized: " + logFile);
  return Result<void>::ok();
}

Result<void> GameLauncher::initializeConfig() {
  m_configManager = std::make_unique<ConfigManager>();

  auto result = m_configManager->initialize(m_basePath);
  if (result.isError()) {
    return result;
  }

  result = m_configManager->loadConfig();
  if (result.isError()) {
    return result;
  }

  // Apply command-line overrides
  if (m_options.noFullscreen) {
    m_configManager->setFullscreen(false);
  }

  if (!m_options.langOverride.empty()) {
    m_configManager->setLocale(m_options.langOverride);
  }

  // Initialize game settings
  m_gameSettings = std::make_unique<GameSettings>(m_configManager.get());
  result = m_gameSettings->initialize();
  if (result.isError()) {
    return result;
  }

  logInfo("Configuration loaded: " + m_configManager->getConfig().game.name + " v" +
          m_configManager->getConfig().game.version);

  return Result<void>::ok();
}

Result<void> GameLauncher::initializeDirectories() {
  if (!m_configManager) {
    return Result<void>::error("ConfigManager not initialized");
  }

  return m_configManager->ensureDirectories();
}

Result<void> GameLauncher::initializePacks() {
  const auto& config = m_configManager->getConfig();
  std::string packsDir = m_basePath + config.packs.directory + "/";
  std::string indexPath = packsDir + config.packs.indexFile;

  // Initialize the pack manager
  m_packManager = std::make_unique<vfs::MultiPackManager>();

  auto initResult = m_packManager->initialize();
  if (initResult.isError()) {
    logError("Failed to initialize pack manager: " + initResult.error());
    return initResult;
  }

  // Set pack directory
  m_packManager->setPackDirectory(packsDir);

  // Configure encryption keys from environment if packs are encrypted
  if (config.packs.encrypted) {
    auto keyResult = m_packManager->configureKeysFromEnvironment();
    if (keyResult.isError()) {
      logError("Failed to configure encryption keys from environment: " + keyResult.error());
      logError("Required environment variables: NOVELMIND_PACK_AES_KEY_HEX, "
               "NOVELMIND_PACK_AES_KEY_FILE, or NOVELMIND_PACK_PUBLIC_KEY");
      return keyResult;
    }
    logInfo("Encryption keys configured from environment");
  }

  // Check if packs directory exists
  if (!fs::exists(packsDir)) {
    logInfo("No packs directory found, running in development mode");
    return Result<void>::ok();
  }

  // Load packs index if available
  if (fs::exists(indexPath)) {
    auto indexResult = loadPacksIndex();
    if (indexResult.isError()) {
      logError("Failed to load packs index: " + indexResult.error());
      return indexResult;
    }
  } else {
    logInfo("No packs_index.json found, running in development mode");
  }

  logInfo("Resource packs initialized from: " + packsDir);
  return Result<void>::ok();
}

Result<void> GameLauncher::loadPacksIndex() {
  const auto& config = m_configManager->getConfig();
  std::string packsDir = m_basePath + config.packs.directory + "/";
  std::string indexPath = packsDir + config.packs.indexFile;

  // Read the packs_index.json
  std::ifstream indexFile(indexPath);
  if (!indexFile.is_open()) {
    return Result<void>::error("Cannot open packs_index.json: " + indexPath);
  }

  std::stringstream buffer;
  buffer << indexFile.rdbuf();
  std::string indexContent = buffer.str();

  // Parse the packs index JSON
  // Expected format:
  // {
  //   "packs": [
  //     { "path": "base.nmpack", "type": "base", "priority": 0 },
  //     { "path": "lang_en.nmpack", "type": "language", "priority": 0 },
  //     { "path": "patch_01.nmpack", "type": "patch", "priority": 1 }
  //   ]
  // }

  // Simple JSON parsing for packs array
  size_t packsStart = indexContent.find("\"packs\"");
  if (packsStart == std::string::npos) {
    return Result<void>::error("No 'packs' array found in packs_index.json");
  }

  size_t arrayStart = indexContent.find('[', packsStart);
  if (arrayStart == std::string::npos) {
    return Result<void>::error("Invalid packs_index.json format");
  }

  size_t arrayEnd = indexContent.find(']', arrayStart);
  if (arrayEnd == std::string::npos) {
    return Result<void>::error("Invalid packs_index.json format");
  }

  std::string packsArray = indexContent.substr(arrayStart, arrayEnd - arrayStart + 1);

  // Parse each pack entry
  size_t pos = 0;
  int loadedPacks = 0;
  while ((pos = packsArray.find('{', pos)) != std::string::npos) {
    size_t entryEnd = packsArray.find('}', pos);
    if (entryEnd == std::string::npos)
      break;

    std::string entry = packsArray.substr(pos, entryEnd - pos + 1);

    // Extract path
    size_t pathStart = entry.find("\"path\"");
    std::string packPath;
    if (pathStart != std::string::npos) {
      size_t valueStart = entry.find('"', pathStart + 6);
      if (valueStart != std::string::npos) {
        size_t valueEnd = entry.find('"', valueStart + 1);
        if (valueEnd != std::string::npos) {
          packPath = entry.substr(valueStart + 1, valueEnd - valueStart - 1);
        }
      }
    }

    // Extract type
    size_t typeStart = entry.find("\"type\"");
    std::string packType = "base";
    if (typeStart != std::string::npos) {
      size_t valueStart = entry.find('"', typeStart + 6);
      if (valueStart != std::string::npos) {
        size_t valueEnd = entry.find('"', valueStart + 1);
        if (valueEnd != std::string::npos) {
          packType = entry.substr(valueStart + 1, valueEnd - valueStart - 1);
        }
      }
    }

    // Extract priority
    size_t priorityStart = entry.find("\"priority\"");
    i32 priority = 0;
    if (priorityStart != std::string::npos) {
      size_t valueStart = entry.find_first_of("-0123456789", priorityStart + 10);
      if (valueStart != std::string::npos) {
        try {
          priority = std::stoi(entry.substr(valueStart));
        } catch (...) {
          priority = 0;
        }
      }
    }

    // Map type string to PackType enum
    vfs::PackType vfsType = vfs::PackType::Base;
    if (packType == "patch")
      vfsType = vfs::PackType::Patch;
    else if (packType == "dlc")
      vfsType = vfs::PackType::DLC;
    else if (packType == "language")
      vfsType = vfs::PackType::Language;
    else if (packType == "mod")
      vfsType = vfs::PackType::Mod;

    // Load the pack
    if (!packPath.empty()) {
      std::string fullPath = packsDir + packPath;
      if (fs::exists(fullPath)) {
        auto loadResult = m_packManager->loadPack(fullPath, vfsType, priority);
        if (loadResult.success) {
          logInfo("Loaded pack: " + packPath + " (type=" + packType +
                  ", priority=" + std::to_string(priority) + ")");
          loadedPacks++;
        } else {
          for (const auto& error : loadResult.errors) {
            logError("Pack load error [" + packPath + "]: " + error);
          }
          for (const auto& missing : loadResult.missingDependencies) {
            logWarning("Missing dependency for " + packPath + ": " + missing);
          }
        }
      } else {
        logWarning("Pack file not found: " + fullPath);
      }
    }

    pos = entryEnd + 1;
  }

  logInfo("Loaded " + std::to_string(loadedPacks) + " pack(s) from index");
  return Result<void>::ok();
}

Result<void> GameLauncher::initializeWindow() {
  // In a full implementation, this would create the game window
  // using the settings from config

  const auto& config = m_configManager->getConfig();
  logInfo("Window: " + std::to_string(config.window.width) + "x" +
          std::to_string(config.window.height) +
          (config.window.fullscreen ? " (fullscreen)" : " (windowed)"));

  return Result<void>::ok();
}

Result<void> GameLauncher::initializeAudio() {
  // In a full implementation, this would initialize the audio system
  // with volume settings from config

  const auto& config = m_configManager->getConfig();
  logInfo("Audio: Master=" + std::to_string(static_cast<int>(config.audio.master * 100)) +
          "%, Music=" + std::to_string(static_cast<int>(config.audio.music * 100)) + "%");

  return Result<void>::ok();
}

Result<void> GameLauncher::initializeLocalization() {
  const auto& config = m_configManager->getConfig();

  // Initialize localization manager
  m_localizationManager = std::make_unique<localization::LocalizationManager>();

  // Set default and current locales
  m_localizationManager->setDefaultLocale(
      localization::LocaleId::fromString(config.localization.defaultLocale));
  m_localizationManager->setCurrentLocale(
      localization::LocaleId::fromString(config.localization.currentLocale));

  // Register available locales
  for (const auto& locale : config.localization.availableLocales) {
    localization::LocaleConfig locConfig;
    locConfig.displayName = locale;
    locConfig.nativeName = locale;
    m_localizationManager->registerLocale(localization::LocaleId::fromString(locale), locConfig);
  }

  // Load localization files from packs
  // VFS path: locales/<locale>.json  (e.g., "locales/en.json",
  // "locales/ru.json") These files should be packed in game packs and contain
  // localized strings
  if (m_packManager && m_packManager->getPackCount() > 0) {
    // Load current locale
    std::string currentLocaleFile = "locales/" + config.localization.currentLocale + ".json";
    logInfo("Loading localization from VFS path: " + currentLocaleFile);

    if (m_packManager->exists(currentLocaleFile)) {
      auto resourceData = m_packManager->readResource(currentLocaleFile);
      if (resourceData.isOk()) {
        std::string jsonContent(resourceData.value().begin(), resourceData.value().end());
        auto loadResult = m_localizationManager->loadStringsFromMemory(
            localization::LocaleId::fromString(config.localization.currentLocale), jsonContent,
            localization::LocalizationFormat::JSON);
        if (loadResult.isError()) {
          logWarning("Failed to load localization from pack: " + loadResult.error());
        } else {
          logInfo("Loaded localization from pack: " + currentLocaleFile);
        }
      }
    } else {
      logInfo("Localization file not found in packs: " + currentLocaleFile);
      logInfo("Available VFS paths for localization: locales/<locale>.json");
    }

    // Also load fallback (default) locale if different from current
    if (config.localization.defaultLocale != config.localization.currentLocale) {
      std::string defaultLocaleFile = "locales/" + config.localization.defaultLocale + ".json";
      if (m_packManager->exists(defaultLocaleFile)) {
        auto resourceData = m_packManager->readResource(defaultLocaleFile);
        if (resourceData.isOk()) {
          std::string jsonContent(resourceData.value().begin(), resourceData.value().end());
          m_localizationManager->loadStringsFromMemory(
              localization::LocaleId::fromString(config.localization.defaultLocale), jsonContent,
              localization::LocalizationFormat::JSON);
          logInfo("Loaded fallback localization: " + defaultLocaleFile);
        }
      }
    }
  }

  // Set callback to handle language changes from settings
  // When language changes, reload localization from the new locale file
  m_localizationManager->setOnLanguageChanged([this](const localization::LocaleId& newLocale) {
    logInfo("Language changed to: " + newLocale.toString());

    // Update config
    if (m_configManager) {
      m_configManager->setLocale(newLocale.toString());
    }

    // Reload localization from new locale file
    if (m_packManager && m_packManager->getPackCount() > 0) {
      std::string localeFile = "locales/" + newLocale.toString() + ".json";
      if (m_packManager->exists(localeFile)) {
        auto resourceData = m_packManager->readResource(localeFile);
        if (resourceData.isOk()) {
          std::string jsonContent(resourceData.value().begin(), resourceData.value().end());
          m_localizationManager->loadStringsFromMemory(newLocale, jsonContent,
                                                       localization::LocalizationFormat::JSON);
          logInfo("Reloaded localization for: " + localeFile);
        }
      }
    }
  });

  logInfo("Localization initialized: " + config.localization.currentLocale);
  return Result<void>::ok();
}

Result<void> GameLauncher::initializeInput() {
  // Initialize input manager
  m_inputManager = std::make_unique<input::InputManager>();

  // Apply input bindings from configuration
  auto result = applyInputBindings();
  if (result.isError()) {
    logWarning("Failed to apply input bindings: " + result.error());
    // Continue with default bindings
  }

  logInfo("Input bindings configured");
  return Result<void>::ok();
}

Result<void> GameLauncher::applyInputBindings() {
  if (!m_inputManager || !m_configManager) {
    return Result<void>::error("Input or config manager not initialized");
  }

  const auto& config = m_configManager->getConfig();

  // Log the configured bindings for debugging
  for (const auto& [action, binding] : config.input.bindings) {
    std::string actionName = inputActionToString(action);
    std::string keys;
    for (const auto& key : binding.keys) {
      if (!keys.empty())
        keys += ", ";
      keys += key;
    }
    std::string mouse;
    for (const auto& btn : binding.mouseButtons) {
      if (!mouse.empty())
        mouse += ", ";
      mouse += btn;
    }
    logInfo("Input binding [" + actionName + "]: keys=[" + keys + "], mouse=[" + mouse + "]");
  }

  // Input bindings are stored in RuntimeConfig and used by the runtime
  // The InputManager handles raw input polling, bindings are checked
  // in the main loop update

  return Result<void>::ok();
}

Result<void> GameLauncher::initializeSaveSystem() {
  const auto& config = m_configManager->getConfig();
  std::string savesDir = m_basePath + config.saves.saveDirectory + "/";

  try {
    fs::create_directories(savesDir);
    logInfo("Save directory: " + savesDir);
    return Result<void>::ok();
  } catch (const std::exception& e) {
    return Result<void>::error(std::string("Failed to create saves dir: ") + e.what());
  }
}

Result<void> GameLauncher::initializeScriptRuntime() {
  const auto& config = m_configManager->getConfig();
  std::string startScene = m_options.sceneOverride.empty() ? "main" : m_options.sceneOverride;

  // Initialize script runtime
  m_scriptRuntime = std::make_unique<scripting::ScriptRuntime>();

  // Configure script runtime from config
  scripting::RuntimeConfig scriptConfig;
  scriptConfig.defaultTextSpeed = static_cast<f32>(config.text.speed);
  scriptConfig.autoAdvanceEnabled = config.text.autoAdvance;
  scriptConfig.autoAdvanceDelay = static_cast<f32>(config.text.autoAdvanceMs) / 1000.0f;
  m_scriptRuntime->setConfig(scriptConfig);

  // Load compiled scripts from packs
  auto loadResult = loadCompiledScripts();
  if (loadResult.isError()) {
    logWarning("No compiled scripts loaded: " + loadResult.error());
    logInfo("Running in development mode without pre-compiled scripts");
  }

  logInfo("Script runtime ready, start scene: " + startScene);
  return Result<void>::ok();
}

Result<void> GameLauncher::loadCompiledScripts() {
  if (!m_packManager) {
    return Result<void>::error("Pack manager not initialized");
  }

  if (!m_scriptRuntime) {
    return Result<void>::error("Script runtime not initialized");
  }

  // Try to find compiled scripts in packs
  // Standard location: scripts/compiled_scripts.bin
  const std::string scriptResource = "scripts/compiled_scripts.bin";

  if (!m_packManager->exists(scriptResource)) {
    return Result<void>::error("No compiled scripts found in packs (" + scriptResource + ")");
  }

  // Read the compiled script data
  auto resourceData = m_packManager->readResource(scriptResource);
  if (resourceData.isError()) {
    return Result<void>::error("Failed to read compiled scripts: " + resourceData.error());
  }

  logInfo("Found compiled scripts: " + scriptResource + " (" +
          std::to_string(resourceData.value().size()) + " bytes)");

  // The script runtime would deserialize the compiled script here
  // For now, we log success - full implementation would call:
  // m_scriptRuntime->load(deserializedScript);

  // Count resources of type Script
  auto scriptResources = m_packManager->listResources(vfs::ResourceType::Script);
  logInfo("Found " + std::to_string(scriptResources.size()) + " script resource(s) in packs");

  return Result<void>::ok();
}

void GameLauncher::mainLoop() {
  // Note: lastTime and frameTime are prepared for future frame limiting
  // implementation when integrating with the full Application class
  [[maybe_unused]] constexpr f64 targetFps = 60.0;
  [[maybe_unused]] constexpr f64 frameTime = 1.0 / targetFps;

  logInfo("Entering main loop");

  // For this implementation, we'll run a simplified demo loop
  // In a full implementation, this would integrate with the Application class

  std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
  std::cout << "║           " << m_configManager->getConfig().game.name;
  std::cout << std::string(49 - m_configManager->getConfig().game.name.length(), ' ') << "║\n";
  std::cout << "║           Version " << m_configManager->getConfig().game.version;
  std::cout << std::string(41 - m_configManager->getConfig().game.version.length(), ' ') << "║\n";
  std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

  std::cout << "Game launcher is running.\n";
  std::cout << "Configuration loaded from: " << m_basePath << "config/\n";
  std::cout << "Saves will be stored in: " << m_basePath
            << m_configManager->getConfig().saves.saveDirectory << "/\n";
  std::cout << "Logs are stored in: " << m_basePath
            << m_configManager->getConfig().logging.logDirectory << "/\n\n";

  std::cout << "Current Settings:\n";
  std::cout << "  Resolution: " << m_configManager->getConfig().window.width << "x"
            << m_configManager->getConfig().window.height << "\n";
  std::cout << "  Fullscreen: " << (m_configManager->getConfig().window.fullscreen ? "Yes" : "No")
            << "\n";
  std::cout << "  Language: " << m_configManager->getConfig().localization.currentLocale << "\n";
  std::cout << "  Master Volume: "
            << static_cast<int>(m_configManager->getConfig().audio.master * 100) << "%\n\n";

  std::cout << "Press Enter to exit...\n";
  std::cin.get();

  m_running = false;
}

void GameLauncher::update(f64 deltaTime) {
  (void)deltaTime;

  // Update input manager
  if (m_inputManager) {
    m_inputManager->update();
  }

  // Process input actions based on bindings
  processInput();
}

void GameLauncher::processInput() {
  // Check for Menu action (typically Escape to quit)
  if (isActionTriggered(InputAction::Menu)) {
    logInfo("Menu action triggered");
    // In full implementation, would show menu or quit
    m_running = false;
  }

  // Check for Auto toggle
  if (isActionTriggered(InputAction::Auto)) {
    logInfo("Auto action triggered");
    // In full implementation, would toggle auto-advance
  }

  // Check for Skip
  if (isActionTriggered(InputAction::Skip)) {
    logInfo("Skip action triggered");
    // In full implementation, would enable skip mode
  }

  // Check for QuickSave
  if (isActionTriggered(InputAction::QuickSave)) {
    logInfo("QuickSave action triggered");
    // In full implementation, would quick save
  }

  // Check for QuickLoad
  if (isActionTriggered(InputAction::QuickLoad)) {
    logInfo("QuickLoad action triggered");
    // In full implementation, would quick load
  }

  // Check for FullScreen toggle
  if (isActionTriggered(InputAction::FullScreen)) {
    logInfo("FullScreen action triggered");
    // In full implementation, would toggle fullscreen
  }
}

bool GameLauncher::isActionTriggered(InputAction action) const {
  if (!m_inputManager || !m_configManager) {
    return false;
  }

  const auto& bindings = m_configManager->getConfig().input.bindings;
  auto it = bindings.find(action);
  if (it == bindings.end()) {
    return false;
  }

  const auto& binding = it->second;

  // Check keyboard keys
  for (const auto& keyName : binding.keys) {
    input::Key key = stringToKey(keyName);
    if (key != input::Key::Unknown && m_inputManager->isKeyPressed(key)) {
      return true;
    }
  }

  // Check mouse buttons
  for (const auto& buttonName : binding.mouseButtons) {
    input::MouseButton button = stringToMouseButton(buttonName);
    if (m_inputManager->isMouseButtonPressed(button)) {
      return true;
    }
  }

  return false;
}

input::Key GameLauncher::stringToKey(const std::string& keyName) const {
  // Map common key names to Key enum
  if (keyName == "Space")
    return input::Key::Space;
  if (keyName == "Enter")
    return input::Key::Enter;
  if (keyName == "Escape")
    return input::Key::Escape;
  if (keyName == "Backspace")
    return input::Key::Backspace;
  if (keyName == "Tab")
    return input::Key::Tab;
  if (keyName == "Up")
    return input::Key::Up;
  if (keyName == "Down")
    return input::Key::Down;
  if (keyName == "Left")
    return input::Key::Left;
  if (keyName == "Right")
    return input::Key::Right;
  if (keyName == "LShift")
    return input::Key::LShift;
  if (keyName == "RShift")
    return input::Key::RShift;
  if (keyName == "LCtrl")
    return input::Key::LCtrl;
  if (keyName == "RCtrl")
    return input::Key::RCtrl;
  if (keyName == "LAlt")
    return input::Key::LAlt;
  if (keyName == "RAlt")
    return input::Key::RAlt;

  // Single letter keys
  if (keyName.length() == 1) {
    char c = keyName[0];
    if (c >= 'A' && c <= 'Z') {
      return static_cast<input::Key>(static_cast<int>(input::Key::A) + (c - 'A'));
    }
    if (c >= 'a' && c <= 'z') {
      return static_cast<input::Key>(static_cast<int>(input::Key::A) + (c - 'a'));
    }
    if (c >= '0' && c <= '9') {
      return static_cast<input::Key>(static_cast<int>(input::Key::Num0) + (c - '0'));
    }
  }

  // Function keys
  if (keyName.length() >= 2 && keyName[0] == 'F') {
    int num = 0;
    try {
      num = std::stoi(keyName.substr(1));
    } catch (...) {
      return input::Key::Unknown;
    }
    if (num >= 1 && num <= 12) {
      return static_cast<input::Key>(static_cast<int>(input::Key::F1) + (num - 1));
    }
  }

  // PageUp/PageDown not in enum, so map to existing keys or return Unknown
  if (keyName == "PageUp" || keyName == "PageDown") {
    // For now, PageUp/PageDown are not in the Key enum
    // Return Unknown - in full implementation would extend Key enum
    return input::Key::Unknown;
  }

  return input::Key::Unknown;
}

input::MouseButton GameLauncher::stringToMouseButton(const std::string& buttonName) const {
  if (buttonName == "Left")
    return input::MouseButton::Left;
  if (buttonName == "Right")
    return input::MouseButton::Right;
  if (buttonName == "Middle")
    return input::MouseButton::Middle;
  return input::MouseButton::Left; // Default
}

void GameLauncher::render() {
  // Render frame
}

void GameLauncher::setState(LauncherState state) {
  m_state = state;
  if (m_onStateChanged) {
    m_onStateChanged(state);
  }
}

void GameLauncher::setError(const std::string& code, const std::string& message,
                            const std::string& details, const std::string& suggestion) {
  m_lastError.code = code;
  m_lastError.message = message;
  m_lastError.details = details;
  m_lastError.suggestion = suggestion;
  setState(LauncherState::Error);
}

void GameLauncher::logInfo(const std::string& message) {
  NOVELMIND_LOG_INFO("[Launcher] " + message);
}

void GameLauncher::logWarning(const std::string& message) {
  NOVELMIND_LOG_WARN("[Launcher] " + message);
}

void GameLauncher::logError(const std::string& message) {
  NOVELMIND_LOG_ERROR("[Launcher] " + message);
}

} // namespace NovelMind::runtime
