# Fuzz Testing for NovelMind Script

This directory contains fuzz testing targets for the NovelMind Script processing pipeline using [libFuzzer](https://llvm.org/docs/LibFuzzer.html).

## Overview

Fuzz testing is an automated testing technique that provides invalid, unexpected, or random data as inputs to software components. The fuzzing targets in this directory test the robustness of:

1. **Lexer** (`fuzz_lexer.cpp`) - Tokenizes arbitrary byte sequences
2. **Parser** (`fuzz_parser.cpp`) - Parses arbitrary input through lexer → parser pipeline
3. **VM** (`fuzz_vm.cpp`) - Executes arbitrary input through the full pipeline (lexer → parser → compiler → VM)

## Building

Fuzzing requires Clang with libFuzzer support:

```bash
# Configure
cmake -B build \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DNOVELMIND_ENABLE_FUZZING=ON \
  -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Fuzz targets will be in build/bin/
```

## Running

Basic usage:

```bash
cd build/bin

# Run lexer fuzzer for 60 seconds
./fuzz_lexer ../../tests/fuzz/corpus/lexer -max_total_time=60

# Run parser fuzzer for 100000 iterations
./fuzz_parser ../../tests/fuzz/corpus/parser -runs=100000

# Run VM fuzzer with 2GB memory limit
./fuzz_vm ../../tests/fuzz/corpus/vm -rss_limit_mb=2048
```

## Corpus

The `corpus/` directory contains seed inputs to help the fuzzer start with valid examples:

- `corpus/lexer/` - Simple script snippets for lexer testing
- `corpus/parser/` - Valid NM Script programs for parser testing
- `corpus/vm/` - Executable scripts for VM testing

The fuzzer will automatically expand these seeds by mutation and generate new test cases.

## Common Options

- `-max_total_time=N` - Run for N seconds (0 = infinite)
- `-runs=N` - Run exactly N iterations
- `-rss_limit_mb=N` - Memory limit in MB
- `-max_len=N` - Maximum input length in bytes
- `-jobs=N` - Parallel fuzzing jobs
- `-workers=N` - Number of concurrent workers
- `-dict=file` - Mutation dictionary file
- `-minimize_crash=1` - Minimize a crashing input

See [libFuzzer documentation](https://llvm.org/docs/LibFuzzer.html) for all options.

## Reproducing Crashes

If a fuzzer finds a crash, it will save the input to a file (e.g., `crash-abc123`):

```bash
# Reproduce the crash
./fuzz_lexer crash-abc123

# Minimize the crash to smallest reproducing input
./fuzz_lexer crash-abc123 -minimize_crash=1
```

## Sanitizers

Fuzz targets are automatically built with:
- **AddressSanitizer** - Detects memory errors (use-after-free, buffer overflows, etc.)
- **UndefinedBehaviorSanitizer** - Detects undefined behavior (integer overflow, null dereference, etc.)

This ensures that fuzzing catches not just crashes but also subtle memory safety and correctness issues.

## Continuous Fuzzing

For long-running fuzzing campaigns:

```bash
# Run indefinitely until a crash is found
./fuzz_lexer corpus/lexer -max_total_time=0

# Parallel fuzzing with 8 workers (faster coverage)
./fuzz_parser corpus/parser -jobs=8 -workers=8 -max_total_time=3600
```

## Adding New Seed Inputs

To improve fuzzing effectiveness, add realistic test cases to the corpus:

```bash
# Add a new seed file
echo "scene test { say Hero \"New test case\" }" > corpus/parser/new_test.txt

# The fuzzer will use it in the next run
./fuzz_parser corpus/parser
```

## Integration with CI/CD

For automated fuzzing in CI:

```bash
# Short fuzzing run (60 seconds each target)
./fuzz_lexer corpus/lexer -max_total_time=60 -rss_limit_mb=2048
./fuzz_parser corpus/parser -max_total_time=60 -rss_limit_mb=2048
./fuzz_vm corpus/vm -max_total_time=60 -rss_limit_mb=2048
```

## References

- [libFuzzer Tutorial](https://github.com/google/fuzzing/blob/master/tutorial/libFuzzerTutorial.md)
- [libFuzzer Documentation](https://llvm.org/docs/LibFuzzer.html)
- [Issue #544](https://github.com/VisageDvachevsky/StoryGraph/issues/544) - Implementation tracking
- [TESTING.md](../../docs/TESTING.md) - General testing guide
