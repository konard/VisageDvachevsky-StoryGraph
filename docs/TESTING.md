# Testing Guide

This document describes the testing infrastructure and best practices for the NovelMind project.

## Table of Contents

- [Test Organization](#test-organization)
- [Running Tests](#running-tests)
- [Test Coverage](#test-coverage)
- [Writing Tests](#writing-tests)
- [Concurrency Testing](#concurrency-testing)
- [Performance Benchmarks](#performance-benchmarks)
- [Integration Tests](#integration-tests)

## Test Organization

The project uses [Catch2 v3](https://github.com/catchorg/Catch2) as the testing framework.

### Directory Structure

```
tests/
├── unit/              # Unit tests for individual components
│   ├── test_scene_graph.cpp
│   ├── test_audio_manager.cpp
│   ├── test_pack_reader.cpp
│   └── ...
├── integration/       # Integration tests for workflows
│   ├── test_scene_workflow.cpp
│   └── ...
└── CMakeLists.txt
```

### Test Categories

Tests are tagged for filtering:

- `[unit]` - Unit tests
- `[integration]` - Integration tests
- `[benchmark]` - Performance benchmarks
- `[threading]` - Thread safety tests
- `[error]` - Error path tests

## Running Tests

### Build and Run All Tests

```bash
# Configure with tests enabled
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DNM_BUILD_TESTS=ON

# Build
cmake --build build

# Run all tests
cd build && ctest --output-on-failure
```

### Run Specific Tests

```bash
# Run unit tests only
./bin/unit_tests

# Run specific test case
./bin/unit_tests "SceneGraph creation"

# Run tests with specific tag
./bin/unit_tests "[scene_graph]"

# Run integration tests (requires editor build)
./bin/integration_tests
```

### Verbose Output

```bash
# Show all test details
./bin/unit_tests -v

# Show successful tests too
./bin/unit_tests -s
```

## Test Coverage

### Generating Coverage Reports

#### Local Coverage (Linux/macOS)

```bash
# Configure with coverage flags
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DNM_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="--coverage"

# Build and run tests
cmake --build build
cd build && ctest

# Generate coverage report
lcov --directory . --capture --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/tests/*' '*/build/_deps/*' --output-file coverage.info
lcov --list coverage.info

# Generate HTML report
genhtml coverage.info --output-directory coverage_html
```

#### CI Coverage

Coverage reports are automatically generated in CI for:
- Pull requests
- Pushes to `main` branch

View coverage artifacts in GitHub Actions.

### Coverage Goals

Per Issue #179, the coverage targets are:

- **Engine Core**: ≥ 70%
- **Editor**: ≥ 60%
- **Overall**: Increasing coverage of critical paths

### Current Coverage Status

Run the coverage workflow to see current metrics.

## Writing Tests

### Basic Test Structure

```cpp
#include <catch2/catch_test_macros.hpp>
#include "NovelMind/module/component.hpp"

TEST_CASE("Component does something", "[module][tag]")
{
    Component comp;

    SECTION("Specific behavior") {
        comp.doSomething();
        REQUIRE(comp.getState() == ExpectedState);
    }

    SECTION("Another behavior") {
        comp.doSomethingElse();
        REQUIRE_FALSE(comp.isFailed());
    }
}
```

### Assertions

```cpp
// Basic assertions
REQUIRE(expression);          // Hard requirement
REQUIRE_FALSE(expression);    // Must be false
CHECK(expression);            // Soft check (continues on failure)

// Comparisons
REQUIRE(value == expected);
REQUIRE(value != unexpected);

// Floating point
#include <catch2/catch_approx.hpp>
REQUIRE(floatValue == Catch::Approx(expected).margin(0.01));

// Exceptions
REQUIRE_THROWS(functionThatThrows());
REQUIRE_THROWS_AS(function(), SpecificException);
REQUIRE_NOTHROW(safeFuncton());
```

### Test Fixtures

```cpp
class ComponentFixture {
protected:
    void SetUp() {
        component = std::make_unique<Component>();
        component->initialize();
    }

    std::unique_ptr<Component> component;
};

TEST_CASE_METHOD(ComponentFixture, "Test with fixture", "[component]")
{
    REQUIRE(component != nullptr);
    // Use component in tests
}
```

## Concurrency Testing

### Thread Safety Tests

For components that must be thread-safe (e.g., ResourceManager, AudioManager, PackReader):

```cpp
TEST_CASE("Component thread safety", "[component][threading]")
{
    Component component;

    SECTION("Concurrent operations") {
        std::vector<std::thread> threads;

        // Launch multiple threads performing operations
        for (int i = 0; i < 4; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < 100; ++j) {
                    component.operation();
                }
            });
        }

        // Wait for completion
        for (auto& t : threads) {
            t.join();
        }

        // Verify state
        REQUIRE(component.isValid());
    }
}
```

### Testing with Thread Sanitizer

For comprehensive race condition detection:

```bash
# Configure with ThreadSanitizer
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DNM_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -g"

# Build and run
cmake --build build
cd build && ctest
```

### Testing with Memory Sanitizer

For detecting uninitialized memory reads:

```bash
# Configure with MemorySanitizer (requires Clang)
CC=clang CXX=clang++ cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DNM_BUILD_TESTS=ON \
  -DNOVELMIND_ENABLE_MSAN=ON

# Build and run
cmake --build build
cd build && ctest
```

**Note**: MemorySanitizer requires Clang and all dependencies to be MSan-instrumented. System libraries that are not instrumented may cause false positives.

### Common Thread Safety Patterns

```cpp
// Test reader-writer scenarios
TEST_CASE("Concurrent reads and writes", "[threading]")
{
    SharedResource resource;
    std::atomic<int> readCount{0};
    std::atomic<int> writeCount{0};

    std::vector<std::thread> readers;
    std::vector<std::thread> writers;

    // Start readers
    for (int i = 0; i < 8; ++i) {
        readers.emplace_back([&]() {
            for (int j = 0; j < 100; ++j) {
                auto data = resource.read();
                readCount++;
            }
        });
    }

    // Start writers
    for (int i = 0; i < 2; ++i) {
        writers.emplace_back([&]() {
            for (int j = 0; j < 50; ++j) {
                resource.write(j);
                writeCount++;
            }
        });
    }

    // Join all threads
    for (auto& t : readers) t.join();
    for (auto& t : writers) t.join();

    REQUIRE(readCount == 800);
    REQUIRE(writeCount == 100);
}
```

## Performance Benchmarks

### Writing Benchmarks

```cpp
#include <catch2/benchmark/catch_benchmark.hpp>

TEST_CASE("Benchmark operation", "[benchmark][component]")
{
    Component component;
    // Setup

    BENCHMARK("Operation name") {
        return component.operation();
    };
}
```

### Running Benchmarks

```bash
# Run only benchmarks
./bin/unit_tests "[benchmark]"

# Run with samples
./bin/unit_tests "[benchmark]" --benchmark-samples 100
```

### Benchmark Best Practices

1. **Warmup**: Ensure code is warmed up before measuring
2. **Consistent State**: Reset state between iterations if needed
3. **Real-world Data**: Use realistic data sizes
4. **Label Clearly**: Include context in benchmark names

```cpp
TEST_CASE("Scene rendering benchmarks", "[benchmark][scene]")
{
    SceneGraph graph;
    // Setup scene with realistic complexity
    setupRealisticScene(graph, 100); // 100 objects

    BENCHMARK("Render scene with 100 objects") {
        graph.render(renderer);
    };
}
```

## Integration Tests

### Full Workflow Tests

Integration tests verify complete workflows:

```cpp
TEST_CASE("Complete dialogue scene workflow", "[integration][scene]")
{
    SceneGraph graph;

    // Setup scene
    graph.setSceneId("intro");
    graph.showBackground("park.png");
    auto* char1 = graph.showCharacter("alice", "alice", Position::Center);
    auto* dialogue = graph.showDialogue("Alice", "Hello!");

    // Simulate game loop
    for (int i = 0; i < 60; ++i) {
        graph.update(1.0 / 60.0);
        graph.render(renderer);
    }

    // Verify state
    REQUIRE(char1->isVisible());
    REQUIRE(dialogue->isTypewriterComplete());

    // Test save/load
    auto state = graph.saveState();
    SceneGraph newGraph;
    newGraph.loadState(state);

    REQUIRE(newGraph.getSceneId() == "intro");
}
```

### Integration Test Patterns

1. **Setup realistic scenarios**
2. **Test multiple components together**
3. **Verify end-to-end behavior**
4. **Test save/load cycles**
5. **Verify error recovery**

## Error Path Testing

Always test error conditions:

```cpp
TEST_CASE("Error handling", "[component][error]")
{
    Component component;

    SECTION("Invalid input") {
        auto result = component.processInvalid();
        REQUIRE(result.isError());
    }

    SECTION("Resource not found") {
        auto result = component.loadNonexistent();
        REQUIRE(result.isError());
    }

    SECTION("Out of bounds access") {
        REQUIRE_THROWS(component.accessOutOfBounds());
    }
}
```

## Best Practices

### DO

✓ Write tests for new features before implementation (TDD)
✓ Test both happy paths and error paths
✓ Use descriptive test names
✓ Keep tests focused and atomic
✓ Test edge cases and boundary conditions
✓ Use appropriate tags for filtering
✓ Clean up resources in tests
✓ Test thread safety for concurrent components

### DON'T

✗ Don't write tests that depend on external resources
✗ Don't write tests that depend on execution order
✗ Don't ignore intermittent failures
✗ Don't test implementation details
✗ Don't write overly complex test logic
✗ Don't skip error path testing

## Continuous Integration

All tests run automatically on:
- Pull requests
- Pushes to `main`
- Multiple platforms (Linux, Windows, macOS)
- Multiple compilers (GCC, Clang, MSVC)
- Multiple build types (Debug, Release)

See `.github/workflows/ci.yml` and `.github/workflows/coverage.yml` for details.

## References

- [Catch2 Documentation](https://github.com/catchorg/Catch2/blob/devel/docs/Readme.md)
- [CONTRIBUTING.md](../CONTRIBUTING.md) - General contribution guidelines
- [Issue #179](https://github.com/VisageDvachevsky/StoryGraph/issues/179) - Test coverage audit
