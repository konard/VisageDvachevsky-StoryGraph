/**
 * @file game_launcher_main.cpp
 * @brief NovelMind Game Launcher - Main Entry Point
 *
 * This is the primary executable for playing NovelMind visual novels.
 * Users can simply double-click this .exe to start playing without
 * needing any command-line arguments.
 *
 * Features:
 * - Automatic configuration loading from config/runtime_config.json
 * - User settings persistence in config/runtime_user.json
 * - Resource pack loading via packs_index.json
 * - Logging to logs/ directory
 * - User-friendly error messages
 *
 * Usage:
 *   game_launcher              # Start game with default settings
 *   game_launcher --help       # Show help
 *   game_launcher --debug      # Enable debug mode
 *   game_launcher --lang ru    # Override language
 */

#include "NovelMind/runtime/game_launcher.hpp"
#include <iostream>

namespace {

int runGameLauncher(int argc, char* argv[]) {
  NovelMind::runtime::GameLauncher launcher;

  // Set up error callback for user-friendly error display
  launcher.setOnError([](const NovelMind::runtime::LauncherError& error) {
    std::cerr << "\n";
    std::cerr << "╔════════════════════════════════════════════════════════════╗\n";
    std::cerr << "║                    An Error Occurred                       ║\n";
    std::cerr << "╚════════════════════════════════════════════════════════════╝\n\n";
    std::cerr << "Error: " << error.message << "\n\n";

    if (!error.details.empty()) {
      std::cerr << "Details:\n  " << error.details << "\n\n";
    }

    if (!error.suggestion.empty()) {
      std::cerr << "How to fix:\n  " << error.suggestion << "\n\n";
    }

    std::cerr << "If this problem persists, check the logs folder for more "
                 "details.\n";
    std::cerr << "\nPress Enter to exit...";
    std::cin.get();
  });

  // Initialize the launcher
  auto result = launcher.initialize(argc, argv);
  if (result.isError()) {
    launcher.showError(result.error());
    return 1;
  }

  // Run the game
  return launcher.run();
}

} // namespace

// Standard main for console applications
int main(int argc, char* argv[]) {
  return runGameLauncher(argc, argv);
}
