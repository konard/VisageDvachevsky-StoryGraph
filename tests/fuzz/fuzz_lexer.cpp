// Fuzz testing for NovelMind Script Lexer using libFuzzer
// This fuzz target tests the lexer's robustness against arbitrary input

#include "NovelMind/scripting/lexer.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

using namespace NovelMind::scripting;

// libFuzzer entry point
// See: https://llvm.org/docs/LibFuzzer.html
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  // Convert raw bytes to string
  std::string input(reinterpret_cast<const char *>(Data), Size);

  // Test lexer with arbitrary input
  try {
    Lexer lexer;
    auto result = lexer.tokenize(input);

    // The result can be either Ok or Error - both are valid outcomes
    // We just want to ensure no crashes, hangs, or undefined behavior
    if (result.isOk()) {
      // Access the tokens to ensure they're valid
      const auto &tokens = result.value();
      (void)tokens.size(); // Touch the data to ensure it's valid
    } else {
      // Error is acceptable for malformed input
      const auto &error = result.error();
      (void)error.size(); // Touch the error string to ensure it's valid
    }
  } catch (...) {
    // Exceptions are acceptable for extremely malformed input
    // but should not crash the fuzzer
  }

  return 0; // Non-zero return values are reserved for future use
}
