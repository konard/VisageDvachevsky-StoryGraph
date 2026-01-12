// Fuzz testing for NovelMind Script Parser using libFuzzer
// This fuzz target tests the parser's robustness against arbitrary token sequences

#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

using namespace NovelMind::scripting;

// libFuzzer entry point
// See: https://llvm.org/docs/LibFuzzer.html
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  // Convert raw bytes to string
  std::string input(reinterpret_cast<const char*>(Data), Size);

  // Test full lexer -> parser pipeline with arbitrary input
  try {
    Lexer lexer;
    auto tokenResult = lexer.tokenize(input);

    // Only continue to parser if lexing succeeded
    if (tokenResult.isOk()) {
      Parser parser;
      auto parseResult = parser.parse(tokenResult.value());

      // The result can be either Ok or Error - both are valid outcomes
      // We just want to ensure no crashes, hangs, or undefined behavior
      if (parseResult.isOk()) {
        // Access the AST to ensure it's valid
        const auto& program = parseResult.value();
        (void)program.declarations.size();
      } else {
        // Parse error is acceptable for malformed input
        const auto& error = parseResult.error();
        (void)error.size();
      }
    }
  } catch (...) {
    // Exceptions are acceptable for extremely malformed input
    // but should not crash the fuzzer
  }

  return 0; // Non-zero return values are reserved for future use
}
