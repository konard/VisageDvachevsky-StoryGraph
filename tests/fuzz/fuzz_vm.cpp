// Fuzz testing for NovelMind Script VM using libFuzzer
// This fuzz target tests the VM's robustness against arbitrary bytecode

#include "NovelMind/scripting/compiler.hpp"
#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"
#include "NovelMind/scripting/validator.hpp"
#include "NovelMind/scripting/vm.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

using namespace NovelMind::scripting;

// libFuzzer entry point
// See: https://llvm.org/docs/LibFuzzer.html
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  // Convert raw bytes to string
  std::string input(reinterpret_cast<const char*>(Data), Size);

  // Test full pipeline: lexer -> parser -> validator -> compiler -> VM
  try {
    Lexer lexer;
    auto tokenResult = lexer.tokenize(input);
    if (tokenResult.isError()) {
      return 0; // Lexer error is acceptable
    }

    Parser parser;
    auto parseResult = parser.parse(tokenResult.value());
    if (parseResult.isError()) {
      return 0; // Parser error is acceptable
    }

    // Validator step (optional but recommended)
    Validator validator;
    validator.setReportUnused(false);
    auto validation = validator.validate(parseResult.value());
    // Continue even if validation fails - some invalid scripts
    // should be caught by the compiler or VM

    // Compile to bytecode
    Compiler compiler;
    auto compileResult = compiler.compile(parseResult.value());
    if (compileResult.isError()) {
      return 0; // Compiler error is acceptable
    }

    // Load and run in VM with execution limits
    VirtualMachine vm;
    auto loadResult =
        vm.load(compileResult.value().instructions, compileResult.value().stringTable);
    if (loadResult.isError()) {
      return 0; // VM load error is acceptable
    }

    // Run with limited steps to prevent infinite loops
    // The VM has built-in security guards, but we add extra safety
    constexpr int MAX_STEPS = 10000;
    for (int step = 0; step < MAX_STEPS && !vm.isHalted(); ++step) {
      vm.step();
    }

    // VM executed successfully (or halted early)
  } catch (...) {
    // Exceptions are acceptable for edge cases
    // but should not crash the fuzzer
  }

  return 0; // Non-zero return values are reserved for future use
}
