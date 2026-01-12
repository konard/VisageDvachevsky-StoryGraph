/**
 * @file main.cpp
 * @brief NovelMind Script Compiler (nmc)
 *
 * Command-line tool for compiling NM Script (.nms) files to bytecode.
 * This tool provides:
 * - Lexical analysis (tokenization)
 * - Parsing (AST generation)
 * - Semantic validation
 * - Bytecode compilation
 * - Output in various formats (binary, JSON)
 *
 * Usage:
 *   nmc <input.nms> [-o output] [--ast] [--tokens] [--validate-only] [--verbose]
 */

#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"
#include "NovelMind/scripting/validator.hpp"
#include "NovelMind/scripting/compiler.hpp"
#include "NovelMind/scripting/script_error.hpp"
#include "NovelMind/core/logger.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <filesystem>

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
const char* Bold = "\033[1m";
} // namespace Color

struct CompilerOptions {
  std::string inputFile;
  std::string outputFile;
  bool showTokens = false;
  bool showAst = false;
  bool showIr = false;
  bool validateOnly = false;
  bool verbose = false;
  bool noColor = false;
  bool help = false;
  bool version = false;
};

void printVersion() {
  std::cout << "NovelMind Script Compiler (nmc) version " << NOVELMIND_VERSION_MAJOR << "."
            << NOVELMIND_VERSION_MINOR << "." << NOVELMIND_VERSION_PATCH << "\n";
  std::cout << "Copyright (c) 2024 NovelMind Team\n";
}

void printUsage(const char* programName) {
  std::cout << "Usage: " << programName << " <input.nms> [options]\n\n";
  std::cout << "NovelMind Script Compiler - Compiles NM Script files to bytecode.\n\n";
  std::cout << "Options:\n";
  std::cout << "  -o, --output <file>   Output file (default: <input>.nmc)\n";
  std::cout << "  --tokens              Show lexer tokens\n";
  std::cout << "  --ast                 Show parsed AST\n";
  std::cout << "  --ir                  Show intermediate representation\n";
  std::cout << "  --validate-only       Only validate, don't compile\n";
  std::cout << "  -v, --verbose         Verbose output\n";
  std::cout << "  --no-color            Disable colored output\n";
  std::cout << "  -h, --help            Show this help message\n";
  std::cout << "  --version             Show version information\n\n";
  std::cout << "Examples:\n";
  std::cout << "  " << programName << " main.nms                  # Compile main.nms to main.nmc\n";
  std::cout << "  " << programName << " main.nms -o game.nmc      # Compile to game.nmc\n";
  std::cout << "  " << programName << " main.nms --validate-only  # Only check for errors\n";
  std::cout << "  " << programName << " main.nms --ast --tokens   # Show debug output\n";
}

CompilerOptions parseArgs(int argc, char* argv[]) {
  CompilerOptions opts;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      opts.help = true;
    } else if (arg == "--version") {
      opts.version = true;
    } else if (arg == "-o" || arg == "--output") {
      if (i + 1 < argc) {
        opts.outputFile = argv[++i];
      } else {
        std::cerr << "Error: -o requires an argument\n";
      }
    } else if (arg == "--tokens") {
      opts.showTokens = true;
    } else if (arg == "--ast") {
      opts.showAst = true;
    } else if (arg == "--ir") {
      opts.showIr = true;
    } else if (arg == "--validate-only") {
      opts.validateOnly = true;
    } else if (arg == "-v" || arg == "--verbose") {
      opts.verbose = true;
    } else if (arg == "--no-color") {
      opts.noColor = true;
    } else if (arg[0] != '-') {
      opts.inputFile = arg;
    } else {
      std::cerr << "Unknown option: " << arg << "\n";
    }
  }

  // Default output file
  if (opts.outputFile.empty() && !opts.inputFile.empty()) {
    fs::path inputPath(opts.inputFile);
    opts.outputFile = inputPath.stem().string() + ".nmc";
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

void printTokens(const std::vector<NovelMind::scripting::Token>& tokens, bool useColor) {
  const char* cyan = useColor ? Color::Cyan : "";
  const char* yellow = useColor ? Color::Yellow : "";
  const char* green = useColor ? Color::Green : "";
  const char* reset = useColor ? Color::Reset : "";

  std::cout << "\n=== TOKENS ===\n";
  for (const auto& token : tokens) {
    std::cout << cyan << "[" << static_cast<int>(token.type) << "] " << yellow << token.lexeme
              << green << " (line " << token.location.line << ", col " << token.location.column
              << ")" << reset << "\n";
  }
  std::cout << "\n";
}

void printAst(const NovelMind::scripting::Program& program, bool useColor) {
  const char* bold = useColor ? Color::Bold : "";
  const char* cyan = useColor ? Color::Cyan : "";
  const char* yellow = useColor ? Color::Yellow : "";
  const char* reset = useColor ? Color::Reset : "";

  std::cout << "\n=== AST ===\n";

  std::cout << bold << "Characters:\n" << reset;
  for (const auto& ch : program.characters) {
    std::cout << "  " << cyan << ch.id << reset << " (\"" << yellow << ch.displayName << reset
              << "\")\n";
  }

  std::cout << bold << "\nScenes:\n" << reset;
  for (const auto& scene : program.scenes) {
    std::cout << "  " << cyan << scene.name << reset << " (" << scene.body.size()
              << " statements)\n";
  }
  std::cout << "\n";
}

void printIr(const NovelMind::scripting::CompiledScript& script, bool useColor) {
  const char* bold = useColor ? Color::Bold : "";
  const char* cyan = useColor ? Color::Cyan : "";
  const char* magenta = useColor ? Color::Magenta : "";
  const char* reset = useColor ? Color::Reset : "";

  std::cout << "\n=== COMPILED IR ===\n";

  std::cout << bold << "Instructions: " << reset << script.instructions.size() << "\n";
  std::cout << bold << "String table: " << reset << script.stringTable.size() << " entries\n";
  std::cout << bold << "Scene entry points:\n" << reset;

  for (const auto& [name, index] : script.sceneEntryPoints) {
    std::cout << "  " << cyan << name << reset << " -> " << magenta << "instruction " << index
              << reset << "\n";
  }

  std::cout << bold << "\nCharacters:\n" << reset;
  for (const auto& [id, ch] : script.characters) {
    std::cout << "  " << cyan << id << reset << ": \"" << ch.displayName << "\"\n";
  }
  std::cout << "\n";
}

void printErrors(const NovelMind::scripting::ErrorList& errors, bool useColor) {
  const char* red = useColor ? Color::Red : "";
  const char* yellow = useColor ? Color::Yellow : "";
  const char* cyan = useColor ? Color::Cyan : "";
  const char* reset = useColor ? Color::Reset : "";

  for (const auto& err : errors.all()) {
    const char* color = red;
    const char* prefix = "error";

    switch (err.severity) {
    case NovelMind::scripting::Severity::Warning:
      color = yellow;
      prefix = "warning";
      break;
    case NovelMind::scripting::Severity::Info:
      color = cyan;
      prefix = "info";
      break;
    case NovelMind::scripting::Severity::Hint:
      color = cyan;
      prefix = "hint";
      break;
    default:
      break;
    }

    std::cerr << color << prefix << reset << ": " << err.message << " [line " << err.span.start.line
              << ", col " << err.span.start.column << "]\n";
  }
}

bool writeCompiledScript(const NovelMind::scripting::CompiledScript& script,
                         const std::string& path) {
  std::ofstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }

  // Write magic number
  const char magic[] = "NMC1";
  file.write(magic, 4);

  // Write version
  NovelMind::u32 version =
      (NOVELMIND_VERSION_MAJOR << 16) | (NOVELMIND_VERSION_MINOR << 8) | NOVELMIND_VERSION_PATCH;
  file.write(reinterpret_cast<const char*>(&version), sizeof(version));

  // Write instruction count and instructions
  NovelMind::u32 instrCount = static_cast<NovelMind::u32>(script.instructions.size());
  file.write(reinterpret_cast<const char*>(&instrCount), sizeof(instrCount));

  for (const auto& instr : script.instructions) {
    file.write(reinterpret_cast<const char*>(&instr.opcode), sizeof(instr.opcode));
    file.write(reinterpret_cast<const char*>(&instr.operand), sizeof(instr.operand));
  }

  // Write string table
  NovelMind::u32 strCount = static_cast<NovelMind::u32>(script.stringTable.size());
  file.write(reinterpret_cast<const char*>(&strCount), sizeof(strCount));

  for (const auto& str : script.stringTable) {
    NovelMind::u32 len = static_cast<NovelMind::u32>(str.length());
    file.write(reinterpret_cast<const char*>(&len), sizeof(len));
    file.write(str.data(), static_cast<std::streamsize>(len));
  }

  // Write scene entry points
  NovelMind::u32 sceneCount = static_cast<NovelMind::u32>(script.sceneEntryPoints.size());
  file.write(reinterpret_cast<const char*>(&sceneCount), sizeof(sceneCount));

  for (const auto& [name, index] : script.sceneEntryPoints) {
    NovelMind::u32 nameLen = static_cast<NovelMind::u32>(name.length());
    file.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
    file.write(name.data(), static_cast<std::streamsize>(nameLen));
    file.write(reinterpret_cast<const char*>(&index), sizeof(index));
  }

  // Write characters
  NovelMind::u32 charCount = static_cast<NovelMind::u32>(script.characters.size());
  file.write(reinterpret_cast<const char*>(&charCount), sizeof(charCount));

  for (const auto& [id, ch] : script.characters) {
    // ID
    NovelMind::u32 idLen = static_cast<NovelMind::u32>(id.length());
    file.write(reinterpret_cast<const char*>(&idLen), sizeof(idLen));
    file.write(id.data(), static_cast<std::streamsize>(idLen));

    // Display name
    NovelMind::u32 nameLen = static_cast<NovelMind::u32>(ch.displayName.length());
    file.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
    file.write(ch.displayName.data(), static_cast<std::streamsize>(nameLen));

    // Color
    NovelMind::u32 colorLen = static_cast<NovelMind::u32>(ch.color.length());
    file.write(reinterpret_cast<const char*>(&colorLen), sizeof(colorLen));
    file.write(ch.color.data(), static_cast<std::streamsize>(colorLen));
  }

  return file.good();
}

int main(int argc, char* argv[]) {
  CompilerOptions opts = parseArgs(argc, argv);

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

  if (opts.help || opts.inputFile.empty()) {
    printUsage(argv[0]);
    return opts.help ? 0 : 1;
  }

  const char* green = useColor ? Color::Green : "";
  const char* red = useColor ? Color::Red : "";
  const char* bold = useColor ? Color::Bold : "";
  const char* reset = useColor ? Color::Reset : "";

  try {
    // Read source file
    if (opts.verbose) {
      std::cout << "Reading " << opts.inputFile << "...\n";
    }

    std::string source = readFile(opts.inputFile);

    // Lexical analysis
    if (opts.verbose) {
      std::cout << "Tokenizing...\n";
    }

    NovelMind::scripting::Lexer lexer;
    auto tokenResult = lexer.tokenize(source);

    if (!tokenResult.isOk()) {
      std::cerr << red << "Lexer error: " << reset << tokenResult.error() << "\n";
      return 1;
    }

    auto tokens = tokenResult.value();

    if (!lexer.getErrors().empty()) {
      for (const auto& err : lexer.getErrors()) {
        std::cerr << red << "Lexer error" << reset << ": " << err.message << " [line "
                  << err.location.line << "]\n";
      }
      return 1;
    }

    if (opts.showTokens) {
      printTokens(tokens, useColor);
    }

    // Parsing
    if (opts.verbose) {
      std::cout << "Parsing...\n";
    }

    NovelMind::scripting::Parser parser;
    auto parseResult = parser.parse(tokens);

    if (!parseResult.isOk()) {
      std::cerr << red << "Parse error: " << reset << parseResult.error() << "\n";
      return 1;
    }

    auto program = std::move(parseResult).value();

    if (!parser.getErrors().empty()) {
      for (const auto& err : parser.getErrors()) {
        std::cerr << red << "Parse error" << reset << ": " << err.message << " [line "
                  << err.location.line << "]\n";
      }
      return 1;
    }

    // Save scene/character counts before moving program
    size_t sceneCount = program.scenes.size();
    size_t characterCount = program.characters.size();

    if (opts.showAst) {
      // Print AST info - characters can be printed, but scene body requires special handling
      const char* cyan = useColor ? Color::Cyan : "";
      const char* yellow = useColor ? Color::Yellow : "";
      std::cout << "\n=== AST ===\n";
      std::cout << bold << "Characters:\n" << reset;
      for (const auto& ch : program.characters) {
        std::cout << "  " << cyan << ch.id << reset << " (\"" << yellow << ch.displayName << reset
                  << "\")\n";
      }
      std::cout << bold << "\nScenes:\n" << reset;
      for (const auto& scene : program.scenes) {
        std::cout << "  " << cyan << scene.name << reset << " (" << scene.body.size()
                  << " statements)\n";
      }
      std::cout << "\n";
    }

    // Validation
    if (opts.verbose) {
      std::cout << "Validating...\n";
    }

    NovelMind::scripting::Validator validator;
    auto validationResult = validator.validate(program);

    if (validationResult.hasErrors() || validationResult.hasWarnings()) {
      printErrors(validationResult.errors, useColor);
    }

    if (!validationResult.isValid) {
      std::cerr << red << "Validation failed" << reset << "\n";
      return 1;
    }

    if (opts.validateOnly) {
      std::cout << green << "Validation passed" << reset << " - " << sceneCount << " scenes, "
                << characterCount << " characters\n";
      return 0;
    }

    // Compilation
    if (opts.verbose) {
      std::cout << "Compiling...\n";
    }

    NovelMind::scripting::Compiler compiler;
    auto compileResult = compiler.compile(program);

    if (!compileResult.isOk()) {
      std::cerr << red << "Compile error: " << reset << compileResult.error() << "\n";
      return 1;
    }

    auto compiledScript = compileResult.value();

    if (!compiler.getErrors().empty()) {
      for (const auto& err : compiler.getErrors()) {
        std::cerr << red << "Compile error" << reset << ": " << err.message << " [line "
                  << err.location.line << "]\n";
      }
      return 1;
    }

    if (opts.showIr) {
      printIr(compiledScript, useColor);
    }

    // Write output
    if (opts.verbose) {
      std::cout << "Writing " << opts.outputFile << "...\n";
    }

    if (!writeCompiledScript(compiledScript, opts.outputFile)) {
      std::cerr << red << "Error: " << reset << "Failed to write output file: " << opts.outputFile
                << "\n";
      return 1;
    }

    std::cout << green << bold << "Success!" << reset << " Compiled " << opts.inputFile << " -> "
              << opts.outputFile << "\n";

    if (opts.verbose) {
      std::cout << "  " << compiledScript.instructions.size() << " instructions\n";
      std::cout << "  " << compiledScript.stringTable.size() << " strings\n";
      std::cout << "  " << compiledScript.sceneEntryPoints.size() << " scenes\n";
      std::cout << "  " << compiledScript.characters.size() << " characters\n";
    }

    return 0;

  } catch (const std::exception& e) {
    std::cerr << red << "Error: " << reset << e.what() << "\n";
    return 1;
  }
}
