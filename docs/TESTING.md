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
- [Fuzz Testing](#fuzz-testing)

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
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DNOVELMIND_BUILD_TESTS=ON

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
  -DNOVELMIND_BUILD_TESTS=ON \
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

### Coverage Requirements for New Code

All new code contributions must meet the following coverage requirements:

#### Pull Request Coverage Standards

- **New Features**: Must include tests that achieve at least 70% line coverage for the new code
- **Bug Fixes**: Must include a regression test that reproduces the bug and verifies the fix
- **Refactoring**: Must maintain or improve existing test coverage
- **Critical Paths**: Security-sensitive code, data serialization, and user-facing features require 80%+ coverage

#### What to Test

**DO test:**
- Public APIs and interfaces
- Error handling and edge cases
- Boundary conditions (null, empty, min/max values)
- State transitions
- Thread safety for concurrent components
- Integration points between components
- Critical business logic

**DO NOT test:**
- Third-party library code
- Simple getters/setters without logic
- Trivial forwarding functions
- Generated code (unless it contains custom logic)

#### Acceptable Exceptions

Some code may be difficult or impractical to test with unit tests:
- Platform-specific code requiring special hardware
- GUI event handling (prefer integration tests)
- Complex Qt widget interactions (use Qt Test framework)
- Code that requires specific CI environment setup

Document these exceptions in code comments or PR descriptions.

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

### Mocking and Test Doubles

The project uses manual mocking (test doubles) rather than a mocking framework. This provides full control and keeps dependencies minimal.

#### When to Use Mocks

Use mocks/test doubles when:
- Testing components that depend on external resources (filesystem, network, hardware)
- Testing components that interact with slow or non-deterministic systems
- Isolating the unit under test from its dependencies
- Testing error conditions that are hard to trigger with real implementations

#### Creating Mock Objects

Mocks should implement the same interface as the real object:

```cpp
// Real interface
class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void drawSprite(const Texture& texture, const Transform2D& transform) = 0;
    virtual void drawText(const Font& font, const std::string& text, f32 x, f32 y) = 0;
    virtual i32 getWidth() const = 0;
    virtual i32 getHeight() const = 0;
};

// Mock implementation for testing
class MockRenderer : public IRenderer {
public:
    void drawSprite(const Texture& texture, const Transform2D& transform) override {
        drawSpriteCalls++;
        lastTexture = texture.getId();
    }

    void drawText(const Font& font, const std::string& text, f32 x, f32 y) override {
        drawTextCalls++;
        lastText = text;
    }

    i32 getWidth() const override { return 1920; }
    i32 getHeight() const override { return 1080; }

    // Test instrumentation
    int drawSpriteCalls = 0;
    int drawTextCalls = 0;
    std::string lastTexture;
    std::string lastText;
};

// Usage in tests
TEST_CASE("Scene renders correctly", "[scene][rendering]") {
    MockRenderer renderer;
    SceneGraph scene;

    scene.showBackground("bg.png");
    scene.render(renderer);

    REQUIRE(renderer.drawSpriteCalls == 1);
    REQUIRE(renderer.lastTexture == "bg.png");
}
```

#### Mock Best Practices

**DO:**
- Keep mocks simple and focused
- Implement only the interface methods used by the test
- Use meaningful default return values
- Track method calls and arguments for verification
- Reset mock state between test sections if reused
- Name mocks clearly: `MockRenderer`, `TestRenderer`, `FakeFileSystem`

**DON'T:**
- Don't make mocks too complex with intricate logic
- Don't reuse mocks across unrelated tests
- Don't test the mock itself (test the real implementation separately)
- Don't over-specify mock expectations (makes tests brittle)

#### Types of Test Doubles

**Dummy**: Objects passed around but never used (satisfies parameter requirements)
```cpp
class DummyWindow : public IWindow {
    // Minimal implementation, methods can throw or do nothing
};
```

**Stub**: Provides canned responses to calls
```cpp
class StubResourceManager : public IResourceManager {
    Texture* loadTexture(const std::string& path) override {
        return &m_defaultTexture;  // Always returns same texture
    }
private:
    Texture m_defaultTexture;
};
```

**Mock**: Records calls and allows verification
```cpp
class MockAudioManager : public IAudioManager {
    void playSound(const std::string& soundId) override {
        playedSounds.push_back(soundId);
    }
    std::vector<std::string> playedSounds;
};
```

**Fake**: Working implementation with shortcuts (e.g., in-memory database)
```cpp
class FakeFileSystem : public IFileSystem {
    // Real implementation using in-memory map instead of real filesystem
    std::unordered_map<std::string, std::vector<uint8_t>> files;
};
```

#### Mocking Patterns

**Spy Pattern**: Real object with call recording
```cpp
class SpyRenderer : public Renderer {
    void drawSprite(const Texture& texture, const Transform2D& transform) override {
        drawCalls.push_back({texture.getId(), transform});
        Renderer::drawSprite(texture, transform);  // Call real implementation
    }

    struct DrawCall {
        std::string textureId;
        Transform2D transform;
    };
    std::vector<DrawCall> drawCalls;
};
```

**Builder Pattern for Complex Mocks**: When mocks need configuration
```cpp
class MockRendererBuilder {
public:
    MockRendererBuilder& withWidth(i32 w) { width = w; return *this; }
    MockRendererBuilder& withHeight(i32 h) { height = h; return *this; }
    MockRendererBuilder& failOnDraw() { shouldFailOnDraw = true; return *this; }

    MockRenderer build() {
        MockRenderer renderer;
        renderer.width = width;
        renderer.height = height;
        renderer.shouldFail = shouldFailOnDraw;
        return renderer;
    }

private:
    i32 width = 1920;
    i32 height = 1080;
    bool shouldFailOnDraw = false;
};

// Usage
TEST_CASE("Handle render failure", "[rendering]") {
    auto renderer = MockRendererBuilder()
        .withWidth(800)
        .withHeight(600)
        .failOnDraw()
        .build();
    // Test error handling
}
```

#### Examples from Codebase

See these files for real mocking examples:
- `tests/integration/test_scene_workflow.cpp` - IntegrationMockRenderer
- `tests/integration/test_animation_integration.cpp` - MockSceneObject
- `tests/unit/test_scene_graph_deep.cpp` - MockWindow
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
  -DNOVELMIND_BUILD_TESTS=ON \
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

## Fuzz Testing

Fuzz testing uses libFuzzer to discover crashes, hangs, and undefined behavior by feeding random input to components.

### Building Fuzz Targets

Fuzzing requires Clang with libFuzzer support:

```bash
# Configure with fuzzing enabled
cmake -B build \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DNOVELMIND_ENABLE_FUZZING=ON \
  -DCMAKE_BUILD_TYPE=Release

# Build fuzz targets
cmake --build build

# Fuzz targets are in build/bin/fuzz/
```

### Available Fuzz Targets

- **fuzz_lexer** - Tests Script lexer with arbitrary byte sequences
- **fuzz_parser** - Tests Script parser with arbitrary input
- **fuzz_vm** - Tests full pipeline: lexer → parser → compiler → VM

### Running Fuzz Targets

```bash
cd build/bin

# Run lexer fuzzer for 60 seconds
./fuzz_lexer ../../../tests/fuzz/corpus/lexer -max_total_time=60

# Run parser fuzzer with 100000 runs
./fuzz_parser ../../../tests/fuzz/corpus/parser -runs=100000

# Run VM fuzzer with custom memory limit
./fuzz_vm ../../../tests/fuzz/corpus/vm -rss_limit_mb=2048
```

### Fuzzing Options

Common libFuzzer options:

- `-max_total_time=N` - Run for N seconds
- `-runs=N` - Run N iterations then exit
- `-rss_limit_mb=N` - Memory limit in MB
- `-max_len=N` - Maximum input length
- `-dict=file` - Use mutation dictionary
- `-jobs=N` - Parallel fuzzing jobs
- `-workers=N` - Concurrent workers

See [libFuzzer documentation](https://llvm.org/docs/LibFuzzer.html) for more options.

### Reproducing Crashes

If fuzzing finds a crash, it saves the input to a file:

```bash
# Reproduce a crash
./fuzz_lexer crash-file

# Minimize the crash input
./fuzz_lexer crash-file -minimize_crash=1
```

### Continuous Fuzzing

For long-term fuzzing:

```bash
# Run indefinitely until crash found
./fuzz_lexer corpus/ -max_total_time=0

# Parallel fuzzing with 8 workers
./fuzz_parser corpus/ -jobs=8 -workers=8
```

### Seed Corpus

The `tests/fuzz/corpus/` directory contains seed inputs:

- `corpus/lexer/` - Valid script snippets for lexer
- `corpus/parser/` - Valid scripts for parser
- `corpus/vm/` - Valid scripts for VM

Add your own seed files to improve fuzzing coverage.

### Integration with Sanitizers

Fuzz targets are built with AddressSanitizer and UndefinedBehaviorSanitizer by default to catch memory errors and undefined behavior.

For additional sanitizers:

```bash
# Build with ThreadSanitizer (for concurrent code)
cmake -B build \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DNOVELMIND_ENABLE_FUZZING=ON \
  -DCMAKE_CXX_FLAGS="-fsanitize=fuzzer,thread"
```

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
- [libFuzzer Documentation](https://llvm.org/docs/LibFuzzer.html)
- [CONTRIBUTING.md](../CONTRIBUTING.md) - General contribution guidelines
- [Issue #179](https://github.com/VisageDvachevsky/StoryGraph/issues/179) - Test coverage audit
- [Issue #544](https://github.com/VisageDvachevsky/StoryGraph/issues/544) - Fuzz testing implementation
