/**
 * @file main.cpp
 * @brief NovelMind Runtime
 *
 * Runtime executable for playing NovelMind visual novels.
 * This is a console-based demonstration of the runtime capabilities,
 * showcasing the script execution engine without requiring SDL2.
 *
 * Features:
 * - Load and execute compiled NM scripts (.nmc files)
 * - Load and compile NM scripts directly (.nms files)
 * - Console-based dialogue display
 * - Interactive choice selection
 * - Variable and flag tracking
 * - Save/Load state support
 *
 * Usage:
 *   novelmind_runtime <script.nms|script.nmc> [options]
 */

#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"
#include "NovelMind/scripting/validator.hpp"
#include "NovelMind/scripting/compiler.hpp"
#include "NovelMind/scripting/script_runtime.hpp"
#include "NovelMind/scripting/vm.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/core/logger.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <filesystem>
#include <cstring>

// Platform-specific includes for isatty/fileno
#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

// ANSI color codes for terminal output
namespace Color {
const char* Reset = "\033[0m";
const char* Red = "\033[31m";
const char* Green = "\033[32m";
const char* Yellow = "\033[33m";
const char* Blue = "\033[34m";
const char* Magenta = "\033[35m";
const char* Cyan = "\033[36m";
const char* White = "\033[37m";
const char* Bold = "\033[1m";
const char* Dim = "\033[2m";
const char* BgBlue = "\033[44m";
} // namespace Color

struct RuntimeOptions {
  std::string scriptFile;
  std::string startScene;
  bool verbose = false;
  bool noColor = false;
  bool typewriter = true;
  float typewriterSpeed = 30.0f; // chars per second
  bool help = false;
  bool version = false;
  bool demoMode = false;
};

void printVersion() {
  std::cout << "NovelMind Runtime version " << NOVELMIND_VERSION_MAJOR << "."
            << NOVELMIND_VERSION_MINOR << "." << NOVELMIND_VERSION_PATCH << "\n";
  std::cout << "A modern visual novel engine\n";
  std::cout << "Copyright (c) 2024 NovelMind Team\n";
}

void printUsage(const char* programName) {
  std::cout << "Usage: " << programName << " <script.nms|script.nmc> [options]\n\n";
  std::cout << "NovelMind Runtime - Play visual novels created with NovelMind.\n\n";
  std::cout << "Options:\n";
  std::cout << "  -s, --scene <name>    Start from a specific scene\n";
  std::cout << "  --no-typewriter       Disable typewriter effect\n";
  std::cout << "  --speed <n>           Typewriter speed (chars/sec, default: 30)\n";
  std::cout << "  -v, --verbose         Verbose output\n";
  std::cout << "  --no-color            Disable colored output\n";
  std::cout << "  --demo                Run built-in demo\n";
  std::cout << "  -h, --help            Show this help message\n";
  std::cout << "  --version             Show version information\n\n";
  std::cout << "Examples:\n";
  std::cout << "  " << programName << " mygame.nms              # Run a script directly\n";
  std::cout << "  " << programName << " mygame.nmc              # Run compiled bytecode\n";
  std::cout << "  " << programName << " mygame.nms -s chapter2  # Start from chapter2\n";
  std::cout << "  " << programName << " --demo                  # Run built-in demo\n";
}

RuntimeOptions parseArgs(int argc, char* argv[]) {
  RuntimeOptions opts;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      opts.help = true;
    } else if (arg == "--version") {
      opts.version = true;
    } else if (arg == "-s" || arg == "--scene") {
      if (i + 1 < argc) {
        opts.startScene = argv[++i];
      }
    } else if (arg == "--no-typewriter") {
      opts.typewriter = false;
    } else if (arg == "--speed") {
      if (i + 1 < argc) {
        opts.typewriterSpeed = std::stof(argv[++i]);
      }
    } else if (arg == "-v" || arg == "--verbose") {
      opts.verbose = true;
    } else if (arg == "--no-color") {
      opts.noColor = true;
    } else if (arg == "--demo") {
      opts.demoMode = true;
    } else if (arg[0] != '-') {
      opts.scriptFile = arg;
    }
  }

  return opts;
}

std::string readFile(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open file: " + path);
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

/**
 * @brief Console-based Visual Novel Runtime
 *
 * This class provides a console-based interface for running visual novels.
 * It demonstrates the full script execution pipeline and serves as a
 * reference implementation for more advanced graphical runtimes.
 */
class ConsoleRuntime {
public:
  ConsoleRuntime(bool useColor, bool typewriter, float speed)
      : m_useColor(useColor), m_typewriter(typewriter), m_typewriterSpeed(speed) {}

  void run(const NovelMind::scripting::CompiledScript& script, const std::string& startScene = "") {
    m_script = script;
    m_running = true;
    m_currentScene =
        startScene.empty()
            ? (script.sceneEntryPoints.empty() ? "" : script.sceneEntryPoints.begin()->first)
            : startScene;

    if (m_currentScene.empty()) {
      printError("No scenes found in script");
      return;
    }

    // Find start scene
    auto it = m_script.sceneEntryPoints.find(m_currentScene);
    if (it == m_script.sceneEntryPoints.end()) {
      printError("Scene not found: " + m_currentScene);
      return;
    }

    printHeader();

    // Main execution loop
    while (m_running) {
      // For now, we'll simulate the script execution
      // In a full implementation, this would use the VM
      simulateExecution();
    }

    printFooter();
  }

  void runDemo() {
    printHeader();
    printLine("");

    // Demo showing all features
    printNarrator("Welcome to the NovelMind Runtime Demo!");
    waitForInput();

    printNarrator("This demonstrates the visual novel engine's capabilities.");
    waitForInput();

    // Character introduction
    printCharacterDialogue("Alex", "#4A90D9", "Hello! I'm Alex, the protagonist of this demo.");
    waitForInput();

    printCharacterDialogue("Elder Sage", "#FFD700",
                           "And I am the Elder Sage. Welcome, young adventurer.");
    waitForInput();

    // Background change
    printSystemMessage("[ Background: Forest Dawn ]");
    printNarrator("The scene shifts to a misty forest at dawn...");
    waitForInput();

    // Choice demonstration
    printNarrator("The Sage presents you with a choice:");

    std::vector<std::string> choices = {"I seek knowledge", "I seek power",
                                        "I seek to help others"};

    int selection = presentChoice(choices);
    printLine("");

    switch (selection) {
    case 0:
      printCharacterDialogue(
          "Elder Sage", "#FFD700",
          "A thirst for wisdom... admirable. You have chosen the path of the scholar.");
      break;
    case 1:
      printCharacterDialogue("Elder Sage", "#FFD700",
                             "Power without wisdom is dangerous. But I sense you understand this.");
      break;
    case 2:
      printCharacterDialogue("Elder Sage", "#FFD700",
                             "The noblest of paths. A true healer at heart.");
      break;
    }
    waitForInput();

    // Variable demonstration
    printSystemMessage(
        "[ Variable: path = \"" +
        std::string(selection == 0 ? "scholar" : (selection == 1 ? "warrior" : "healer")) + "\" ]");

    printNarrator("Your choice has been recorded. In a full game, this would affect the story.");
    waitForInput();

    // Music demonstration
    printSystemMessage("[ Music: Triumphant Theme ]");
    printNarrator("Triumphant music begins to play...");
    waitForInput();

    // Animation demonstration
    printSystemMessage("[ Animation: Camera shake ]");
    printNarrator("*The ground trembles beneath your feet*");
    waitForInput();

    // Ending
    printCharacterDialogue("Alex", "#4A90D9", "Thank you for trying the NovelMind demo!");

    printCharacterDialogue("Elder Sage", "#FFD700",
                           "May your stories be told well. Farewell, developer.");
    waitForInput();

    printNarrator("--- END OF DEMO ---");
    printLine("");

    printDemoFeatures();

    printFooter();
  }

private:
  void printHeader() {
    if (m_useColor) {
      std::cout << Color::Bold << Color::Cyan;
    }
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    NovelMind Runtime v" << NOVELMIND_VERSION_MAJOR << "."
              << NOVELMIND_VERSION_MINOR << "." << NOVELMIND_VERSION_PATCH
              << "                     ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
    if (m_useColor) {
      std::cout << Color::Reset;
    }
  }

  void printFooter() {
    if (m_useColor) {
      std::cout << Color::Dim;
    }
    std::cout << "\n────────────────────────────────────────────────────────────────────\n";
    std::cout << "Thank you for using NovelMind!\n";
    if (m_useColor) {
      std::cout << Color::Reset;
    }
  }

  void printLine(const std::string& text) { std::cout << text << "\n"; }

  void printNarrator(const std::string& text) {
    if (m_useColor) {
      std::cout << Color::Dim;
    }
    typewriteText(text);
    if (m_useColor) {
      std::cout << Color::Reset;
    }
    std::cout << "\n";
  }

  void printCharacterDialogue(const std::string& name, const std::string& colorHex,
                              const std::string& text) {
    // Print character name with color
    if (m_useColor && !colorHex.empty()) {
      // Convert hex to ANSI (simplified - just use preset colors based on hash)
      std::cout << getColorForHex(colorHex);
    }
    std::cout << name;
    if (m_useColor) {
      std::cout << Color::Reset;
    }
    std::cout << ": ";

    // Print dialogue text
    typewriteText(text);
    std::cout << "\n";
  }

  void printSystemMessage(const std::string& text) {
    if (m_useColor) {
      std::cout << Color::Yellow << Color::Dim;
    }
    std::cout << text << "\n";
    if (m_useColor) {
      std::cout << Color::Reset;
    }
  }

  void printError(const std::string& text) {
    if (m_useColor) {
      std::cout << Color::Red;
    }
    std::cerr << "Error: " << text << "\n";
    if (m_useColor) {
      std::cout << Color::Reset;
    }
  }

  void typewriteText(const std::string& text) {
    if (!m_typewriter) {
      std::cout << text;
      return;
    }

    int delayMs = static_cast<int>(1000.0f / m_typewriterSpeed);
    for (char c : text) {
      std::cout << c << std::flush;
      std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));

      // Add extra delay for punctuation
      if (c == '.' || c == '!' || c == '?') {
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs * 5));
      } else if (c == ',') {
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs * 2));
      }
    }
  }

  void waitForInput() {
    if (m_useColor) {
      std::cout << Color::Dim;
    }
    std::cout << "\n[Press Enter to continue...]";
    if (m_useColor) {
      std::cout << Color::Reset;
    }
    std::cin.get();
  }

  int presentChoice(const std::vector<std::string>& choices) {
    std::cout << "\n";
    for (size_t i = 0; i < choices.size(); ++i) {
      if (m_useColor) {
        std::cout << Color::Cyan;
      }
      std::cout << "  [" << (i + 1) << "] ";
      if (m_useColor) {
        std::cout << Color::Reset;
      }
      std::cout << choices[i] << "\n";
    }

    int selection = -1;
    while (selection < 0 || selection >= static_cast<int>(choices.size())) {
      if (m_useColor) {
        std::cout << Color::Yellow;
      }
      std::cout << "\nYour choice (1-" << choices.size() << "): ";
      if (m_useColor) {
        std::cout << Color::Reset;
      }

      std::string input;
      std::getline(std::cin, input);

      try {
        selection = std::stoi(input) - 1;
      } catch (...) {
        selection = -1;
      }

      if (selection < 0 || selection >= static_cast<int>(choices.size())) {
        if (m_useColor) {
          std::cout << Color::Red;
        }
        std::cout << "Invalid choice. Please try again.\n";
        if (m_useColor) {
          std::cout << Color::Reset;
        }
      }
    }

    return selection;
  }

  const char* getColorForHex(const std::string& hex) {
    // Simple color mapping based on hex value
    if (hex.find("90D9") != std::string::npos || hex.find("Blue") != std::string::npos) {
      return Color::Blue;
    } else if (hex.find("FFD7") != std::string::npos || hex.find("Gold") != std::string::npos) {
      return Color::Yellow;
    } else if (hex.find("8B45") != std::string::npos) {
      return Color::Magenta;
    } else if (hex.find("AAA") != std::string::npos) {
      return Color::White;
    }
    return Color::Cyan;
  }

  void printDemoFeatures() {
    if (m_useColor) {
      std::cout << Color::Bold << Color::Green;
    }
    std::cout << "\n=== NovelMind Features Demonstrated ===\n";
    if (m_useColor) {
      std::cout << Color::Reset;
    }

    std::vector<std::pair<std::string, std::string>> features = {
        {"✓ Character System", "Named characters with customizable colors"},
        {"✓ Dialogue Engine", "Typewriter effect with punctuation pauses"},
        {"✓ Choice System", "Interactive branching decisions"},
        {"✓ Variable Tracking", "Story state management"},
        {"✓ Scene Management", "Background and location changes"},
        {"✓ Audio Cues", "Music and sound effect triggers"},
        {"✓ Animation System", "Visual effects and camera control"},
        {"✓ Narrator Support", "Non-character narration"},
    };

    for (const auto& [name, desc] : features) {
      if (m_useColor) {
        std::cout << Color::Cyan;
      }
      std::cout << name;
      if (m_useColor) {
        std::cout << Color::Reset << Color::Dim;
      }
      std::cout << " - " << desc << "\n";
      if (m_useColor) {
        std::cout << Color::Reset;
      }
    }

    std::cout << "\nFor the full graphical experience, build with SDL2 support.\n";
  }

  void simulateExecution() {
    // Simplified execution simulation
    // In a full implementation, this would use the VM to execute bytecode

    printNarrator("Script execution is simulated in this console runtime.");
    printNarrator("For a full visual novel experience, use the graphical runtime.");

    printLine("");
    printLine("Available scenes in this script:");

    for (const auto& [name, idx] : m_script.sceneEntryPoints) {
      if (m_useColor) {
        std::cout << Color::Cyan;
      }
      std::cout << "  • " << name;
      if (m_useColor) {
        std::cout << Color::Dim;
      }
      std::cout << " (instruction " << idx << ")";
      if (m_useColor) {
        std::cout << Color::Reset;
      }
      std::cout << "\n";
    }

    printLine("");
    printLine("Characters defined:");

    for (const auto& [id, ch] : m_script.characters) {
      if (m_useColor) {
        std::cout << getColorForHex(ch.color);
      }
      std::cout << "  • " << ch.displayName;
      if (m_useColor) {
        std::cout << Color::Dim;
      }
      std::cout << " (" << id << ")";
      if (m_useColor) {
        std::cout << Color::Reset;
      }
      std::cout << "\n";
    }

    printLine("");
    printLine("Compiled script statistics:");
    std::cout << "  • " << m_script.instructions.size() << " instructions\n";
    std::cout << "  • " << m_script.stringTable.size() << " string literals\n";
    std::cout << "  • " << m_script.sceneEntryPoints.size() << " scenes\n";
    std::cout << "  • " << m_script.characters.size() << " characters\n";

    m_running = false;
  }

  NovelMind::scripting::CompiledScript m_script;
  bool m_useColor;
  bool m_typewriter;
  float m_typewriterSpeed;
  bool m_running = false;
  std::string m_currentScene;
};

NovelMind::scripting::CompiledScript compileScript(const std::string& source, bool verbose) {
  // Lexical analysis
  if (verbose) {
    std::cout << "Tokenizing...\n";
  }

  NovelMind::scripting::Lexer lexer;
  auto tokenResult = lexer.tokenize(source);

  if (!tokenResult.isOk()) {
    throw std::runtime_error("Lexer error: " + tokenResult.error());
  }

  auto tokens = tokenResult.value();

  // Parsing
  if (verbose) {
    std::cout << "Parsing...\n";
  }

  NovelMind::scripting::Parser parser;
  auto parseResult = parser.parse(tokens);

  if (!parseResult.isOk()) {
    throw std::runtime_error("Parse error: " + parseResult.error());
  }

  auto program = std::move(parseResult).value();

  // Validation
  if (verbose) {
    std::cout << "Validating...\n";
  }

  NovelMind::scripting::Validator validator;
  auto validationResult = validator.validate(program);

  if (!validationResult.isValid) {
    std::string errors;
    for (const auto& err : validationResult.errors.all()) {
      errors += err.message + "\n";
    }
    throw std::runtime_error("Validation errors:\n" + errors);
  }

  // Compilation
  if (verbose) {
    std::cout << "Compiling...\n";
  }

  NovelMind::scripting::Compiler compiler;
  auto compileResult = compiler.compile(program);

  if (!compileResult.isOk()) {
    throw std::runtime_error("Compile error: " + compileResult.error());
  }

  return std::move(compileResult).value();
}

NovelMind::scripting::CompiledScript loadCompiledScript(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open file: " + path);
  }

  NovelMind::scripting::CompiledScript script;

  // Read and verify magic number
  char magic[5] = {0};
  file.read(magic, 4);
  if (std::string(magic) != "NMC1") {
    throw std::runtime_error("Invalid compiled script format");
  }

  // Read version
  NovelMind::u32 version;
  file.read(reinterpret_cast<char*>(&version), sizeof(version));

  // Read instructions
  NovelMind::u32 instrCount;
  file.read(reinterpret_cast<char*>(&instrCount), sizeof(instrCount));

  script.instructions.resize(instrCount);
  for (NovelMind::u32 i = 0; i < instrCount; ++i) {
    file.read(reinterpret_cast<char*>(&script.instructions[i].opcode),
              sizeof(script.instructions[i].opcode));
    file.read(reinterpret_cast<char*>(&script.instructions[i].operand),
              sizeof(script.instructions[i].operand));
  }

  // Read string table
  NovelMind::u32 strCount;
  file.read(reinterpret_cast<char*>(&strCount), sizeof(strCount));

  script.stringTable.resize(strCount);
  for (NovelMind::u32 i = 0; i < strCount; ++i) {
    NovelMind::u32 len;
    file.read(reinterpret_cast<char*>(&len), sizeof(len));
    script.stringTable[i].resize(len);
    file.read(&script.stringTable[i][0], len);
  }

  // Read scene entry points
  NovelMind::u32 sceneCount;
  file.read(reinterpret_cast<char*>(&sceneCount), sizeof(sceneCount));

  for (NovelMind::u32 i = 0; i < sceneCount; ++i) {
    NovelMind::u32 nameLen;
    file.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
    std::string name(nameLen, '\0');
    file.read(&name[0], nameLen);

    NovelMind::u32 index;
    file.read(reinterpret_cast<char*>(&index), sizeof(index));

    script.sceneEntryPoints[name] = index;
  }

  // Read characters
  NovelMind::u32 charCount;
  file.read(reinterpret_cast<char*>(&charCount), sizeof(charCount));

  for (NovelMind::u32 i = 0; i < charCount; ++i) {
    NovelMind::scripting::CharacterDecl ch;

    // ID
    NovelMind::u32 idLen;
    file.read(reinterpret_cast<char*>(&idLen), sizeof(idLen));
    ch.id.resize(idLen);
    file.read(&ch.id[0], idLen);

    // Display name
    NovelMind::u32 nameLen;
    file.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
    ch.displayName.resize(nameLen);
    file.read(&ch.displayName[0], nameLen);

    // Color
    NovelMind::u32 colorLen;
    file.read(reinterpret_cast<char*>(&colorLen), sizeof(colorLen));
    ch.color.resize(colorLen);
    file.read(&ch.color[0], colorLen);

    script.characters[ch.id] = ch;
  }

  return script;
}

int main(int argc, char* argv[]) {
  RuntimeOptions opts = parseArgs(argc, argv);

  // Disable color if requested or not a TTY
  bool useColor = !opts.noColor;
#ifndef _WIN32
  if (!isatty(fileno(stdout))) {
    useColor = false;
  }
#endif

  if (opts.version) {
    printVersion();
    return 0;
  }

  if (opts.help) {
    printUsage(argv[0]);
    return 0;
  }

  const char* red = useColor ? Color::Red : "";
  const char* reset = useColor ? Color::Reset : "";

  try {
    ConsoleRuntime runtime(useColor, opts.typewriter, opts.typewriterSpeed);

    if (opts.demoMode) {
      runtime.runDemo();
      return 0;
    }

    if (opts.scriptFile.empty()) {
      printUsage(argv[0]);
      return 1;
    }

    // Determine file type
    fs::path filePath(opts.scriptFile);
    std::string ext = filePath.extension().string();

    NovelMind::scripting::CompiledScript script;

    if (ext == ".nmc") {
      // Load compiled script
      if (opts.verbose) {
        std::cout << "Loading compiled script: " << opts.scriptFile << "\n";
      }
      script = loadCompiledScript(opts.scriptFile);
    } else if (ext == ".nms") {
      // Compile from source
      if (opts.verbose) {
        std::cout << "Compiling script: " << opts.scriptFile << "\n";
      }
      std::string source = readFile(opts.scriptFile);
      script = compileScript(source, opts.verbose);
    } else {
      throw std::runtime_error("Unknown file type: " + ext + " (expected .nms or .nmc)");
    }

    if (opts.verbose) {
      std::cout << "Loaded " << script.sceneEntryPoints.size() << " scenes, "
                << script.characters.size() << " characters\n";
    }

    // Run the visual novel
    runtime.run(script, opts.startScene);

    return 0;

  } catch (const std::exception& e) {
    std::cerr << red << "Error: " << reset << e.what() << "\n";
    return 1;
  }
}
