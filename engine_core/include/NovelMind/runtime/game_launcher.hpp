#pragma once

/**
 * @file game_launcher.hpp
 * @brief Game Launcher - Main entry point for playing visual novels
 *
 * The Game Launcher provides:
 * - One-click launch from .exe (no CLI flags required)
 * - Automatic config loading from config/runtime_config.json
 * - Resource pack initialization via packs_index.json
 * - Window/audio/locale setup
 * - Error handling with user-friendly messages
 * - Logging to logs/ directory
 *
 * This is the primary runtime for end-users playing the game.
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/input/input_manager.hpp"
#include "NovelMind/runtime/config_manager.hpp"
#include "NovelMind/runtime/game_settings.hpp"
#include "NovelMind/runtime/runtime_config.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Forward declarations
namespace NovelMind::vfs {
class MultiPackManager;
}
namespace NovelMind::scripting {
class ScriptRuntime;
struct CompiledScript;
} // namespace NovelMind::scripting
namespace NovelMind::localization {
class LocalizationManager;
}

namespace NovelMind::runtime {

/**
 * @brief Launcher state
 */
enum class LauncherState {
  Uninitialized,
  Initializing,
  Ready,
  Running,
  Paused,
  Error,
  ShuttingDown
};

/**
 * @brief Launcher error information
 */
struct LauncherError {
  std::string code;
  std::string message;
  std::string details;
  std::string suggestion;

  [[nodiscard]] std::string format() const;
};

/**
 * @brief Command-line options (for developer override)
 */
struct LaunchOptions {
  std::string configOverride; // Override config file path
  std::string langOverride;   // Override language
  std::string sceneOverride;  // Override start scene
  bool debugMode = false;     // Enable debug features
  bool verbose = false;       // Verbose logging
  bool noFullscreen = false;  // Disable fullscreen
  bool help = false;          // Show help
  bool version = false;       // Show version
};

using OnLauncherError = std::function<void(const LauncherError&)>;
using OnLauncherStateChanged = std::function<void(LauncherState)>;

/**
 * @brief Game Launcher
 *
 * Main entry point for the visual novel runtime. This class orchestrates
 * all systems needed to run a game:
 *
 * 1. Parse command-line arguments (optional developer overrides)
 * 2. Load configuration from config/runtime_config.json
 * 3. Load user settings from config/runtime_user.json
 * 4. Initialize logging to logs/game.log
 * 5. Create required directories (saves/, logs/)
 * 6. Load resource packs via packs_index.json
 * 7. Initialize window with configured settings
 * 8. Initialize audio system
 * 9. Load localization for selected language
 * 10. Start the game at the configured start scene
 *
 * Error Handling:
 * - All errors are logged to logs/error.log
 * - User-friendly error messages are displayed
 * - Suggestions for fixing common problems are provided
 *
 * Example usage:
 * @code
 * int main(int argc, char* argv[]) {
 *     NovelMind::runtime::GameLauncher launcher;
 *
 *     auto result = launcher.initialize(argc, argv);
 *     if (result.isError()) {
 *         launcher.showError(result.error());
 *         return 1;
 *     }
 *
 *     return launcher.run();
 * }
 * @endcode
 */
class GameLauncher {
public:
  GameLauncher();
  ~GameLauncher();

  // Non-copyable
  GameLauncher(const GameLauncher&) = delete;
  GameLauncher& operator=(const GameLauncher&) = delete;

  // =========================================================================
  // Initialization
  // =========================================================================

  /**
   * @brief Initialize the launcher
   * @param argc Argument count from main()
   * @param argv Argument values from main()
   * @return Success or error
   */
  Result<void> initialize(int argc, char* argv[]);

  /**
   * @brief Initialize with explicit options (for embedding)
   * @param basePath Base path of the game
   * @param options Launch options
   * @return Success or error
   */
  Result<void> initialize(const std::string& basePath, const LaunchOptions& options = {});

  // =========================================================================
  // Main Loop
  // =========================================================================

  /**
   * @brief Run the game main loop
   * @return Exit code (0 = success)
   */
  i32 run();

  /**
   * @brief Request shutdown
   */
  void quit();

  /**
   * @brief Check if launcher is running
   */
  [[nodiscard]] bool isRunning() const;

  /**
   * @brief Get current state
   */
  [[nodiscard]] LauncherState getState() const { return m_state; }

  // =========================================================================
  // Error Handling
  // =========================================================================

  /**
   * @brief Show error to user
   *
   * Displays an error dialog or console message with:
   * - Error message
   * - Technical details
   * - Suggestion for fixing
   */
  void showError(const LauncherError& error);

  /**
   * @brief Show error from result
   */
  void showError(const std::string& error);

  /**
   * @brief Get last error
   */
  [[nodiscard]] const LauncherError& getLastError() const { return m_lastError; }

  // =========================================================================
  // System Access
  // =========================================================================

  /**
   * @brief Get the configuration manager
   */
  [[nodiscard]] ConfigManager* getConfigManager();

  /**
   * @brief Get the game settings
   */
  [[nodiscard]] GameSettings* getGameSettings();

  /**
   * @brief Get the pack manager
   */
  [[nodiscard]] vfs::MultiPackManager* getPackManager();

  /**
   * @brief Get the script runtime
   */
  [[nodiscard]] scripting::ScriptRuntime* getScriptRuntime();

  /**
   * @brief Get the localization manager
   */
  [[nodiscard]] localization::LocalizationManager* getLocalizationManager();

  /**
   * @brief Get the input manager
   */
  [[nodiscard]] input::InputManager* getInputManager();

  /**
   * @brief Get the current configuration
   */
  [[nodiscard]] const RuntimeConfig& getConfig() const;

  // =========================================================================
  // Callbacks
  // =========================================================================

  void setOnError(OnLauncherError callback);
  void setOnStateChanged(OnLauncherStateChanged callback);

  // =========================================================================
  // Utility
  // =========================================================================

  /**
   * @brief Print version information
   */
  static void printVersion();

  /**
   * @brief Print usage/help
   */
  static void printHelp(const char* programName);

  /**
   * @brief Get executable directory
   */
  [[nodiscard]] static std::string getExecutableDirectory();

  /**
   * @brief Parse command-line arguments
   */
  [[nodiscard]] static LaunchOptions parseArgs(int argc, char* argv[]);

private:
  // Initialization steps
  Result<void> initializeLogging();
  Result<void> initializeConfig();
  Result<void> initializeDirectories();
  Result<void> initializePacks();
  Result<void> initializeWindow();
  Result<void> initializeAudio();
  Result<void> initializeLocalization();
  Result<void> initializeInput();
  Result<void> initializeSaveSystem();
  Result<void> initializeScriptRuntime();
  Result<void> loadPacksIndex();
  Result<void> loadCompiledScripts();
  Result<void> applyInputBindings();

  // Main loop
  void mainLoop();
  void update(f64 deltaTime);
  void render();
  void processInput();

  // Input action checking
  [[nodiscard]] bool isActionTriggered(InputAction action) const;
  [[nodiscard]] input::Key stringToKey(const std::string& keyName) const;
  [[nodiscard]] input::MouseButton stringToMouseButton(const std::string& buttonName) const;

  // State management
  void setState(LauncherState state);
  void setError(const std::string& code, const std::string& message,
                const std::string& details = "", const std::string& suggestion = "");

  // Logging
  void logInfo(const std::string& message);
  void logWarning(const std::string& message);
  void logError(const std::string& message);

  // Member variables
  std::string m_basePath;
  LaunchOptions m_options;
  LauncherState m_state = LauncherState::Uninitialized;
  LauncherError m_lastError;
  bool m_running = false;

  std::unique_ptr<ConfigManager> m_configManager;
  std::unique_ptr<GameSettings> m_gameSettings;
  std::unique_ptr<vfs::MultiPackManager> m_packManager;
  std::unique_ptr<scripting::ScriptRuntime> m_scriptRuntime;
  std::unique_ptr<localization::LocalizationManager> m_localizationManager;
  std::unique_ptr<input::InputManager> m_inputManager;

  OnLauncherError m_onError;
  OnLauncherStateChanged m_onStateChanged;
};

} // namespace NovelMind::runtime
