# Contributing to NovelMind

Thank you for your interest in contributing to NovelMind! This document provides guidelines and instructions for contributing.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Environment](#development-environment)
- [Building the Project](#building-the-project)
- [Code Style](#code-style)
- [Submitting Changes](#submitting-changes)
- [Bug Reports](#bug-reports)
- [Feature Requests](#feature-requests)

## Code of Conduct

Please be respectful and constructive in all interactions. We welcome contributors from all backgrounds and experience levels.

## Getting Started

1. Fork the repository on GitHub
2. Clone your fork locally:
   ```bash
   git clone https://github.com/YOUR_USERNAME/NM-.git
   cd NM-
   ```
3. Add the upstream repository:
   ```bash
   git remote add upstream https://github.com/VisageDvachevsky/NM-.git
   ```

## Development Environment

### Prerequisites

- **C++ Compiler**: GCC 11+, Clang 14+, or MSVC 2022+
- **CMake**: Version 3.20 or higher
- **Build System**: Ninja (recommended) or Make
- **Optional**: SDL2 for windowing support

### Platform-Specific Setup

#### Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install -y cmake ninja-build libsdl2-dev
```

#### macOS
```bash
brew install cmake ninja sdl2
```

#### Windows
- Install Visual Studio 2022 with C++ workload
- Install CMake from https://cmake.org/download/

## Building the Project

### Quick Build

```bash
# Configure
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DNOVELMIND_BUILD_TESTS=ON

# Build
cmake --build build

# Run tests
cd build && ctest --output-on-failure
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `NOVELMIND_BUILD_TESTS` | ON | Build unit tests |
| `NOVELMIND_BUILD_EDITOR` | OFF | Build visual editor |
| `NOVELMIND_ENABLE_ASAN` | OFF | Enable AddressSanitizer |
| `NOVELMIND_ENABLE_TSAN` | OFF | Enable ThreadSanitizer |
| `NOVELMIND_ENABLE_UBSAN` | OFF | Enable UndefinedBehaviorSanitizer |

### Build Types

- **Debug**: Full debug symbols, no optimization
- **Release**: Full optimization, no debug symbols
- **RelWithDebInfo**: Optimization with debug symbols

## Code Style

### C++ Standards

- Use C++20 features where appropriate
- Follow the existing code style in the codebase
- Use meaningful variable and function names
- Add comments for complex logic

### Formatting

We use `clang-format` for code formatting. Before submitting:

```bash
# Format all source files
find engine_core -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i
```

### Pre-commit Hooks

To ensure code quality, we provide pre-commit hooks that automatically check your code before commits:

```bash
# Install pre-commit (if not already installed)
pip install pre-commit

# Install the hooks
pre-commit install

# Run manually on all files (optional)
pre-commit run --all-files
```

The pre-commit hooks will:
- Check and fix trailing whitespace
- Ensure files end with newline
- Validate YAML syntax
- Run clang-format on C++ files
- Run cppcheck for static analysis
- Format CMake files

### Naming Conventions

- **Classes/Structs**: `PascalCase` (e.g., `SceneManager`)
- **Functions/Methods**: `camelCase` (e.g., `loadScene`)
- **Variables**: `camelCase` with `m_` prefix for members (e.g., `m_sceneGraph`)
- **Constants**: `UPPER_SNAKE_CASE` (e.g., `MAX_SLOTS`)
- **Namespaces**: `lowercase` (e.g., `NovelMind::scripting`)

### File Organization

- Headers in `engine_core/include/NovelMind/`
- Source files in `engine_core/src/`
- Tests in `tests/unit/` and `tests/integration/`

## Submitting Changes

### Pull Request Process

1. Create a feature branch from `main`:
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. Make your changes with clear, atomic commits

3. Ensure all tests pass:
   ```bash
   cd build && ctest --output-on-failure
   ```

4. Push to your fork and create a Pull Request

5. Fill out the PR template with:
   - Summary of changes
   - Related issue number (if applicable)
   - Test plan

### Commit Messages

Use clear, descriptive commit messages:

```
type: Brief description

Longer description if needed. Explain what and why,
not how (the code shows how).

Fixes #123
```

Types: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`

## Bug Reports

When reporting bugs, please include:

1. **Description**: Clear description of the bug
2. **Steps to Reproduce**: Minimal steps to reproduce the issue
3. **Expected Behavior**: What you expected to happen
4. **Actual Behavior**: What actually happened
5. **Environment**: OS, compiler version, CMake version
6. **Logs/Output**: Any relevant error messages or logs

## Feature Requests

For feature requests, please describe:

1. **Use Case**: Why this feature is needed
2. **Proposed Solution**: How you envision it working
3. **Alternatives**: Any alternatives you've considered

## Testing

### Running Tests

```bash
# Run all tests
cd build && ctest --output-on-failure

# Run specific test
./bin/unit_tests "[lexer]"

# Run with verbose output
./bin/unit_tests -v
```

### Static Analysis

Run static analysis tools locally before submitting:

```bash
# Run cppcheck
cppcheck --enable=warning,style,performance,portability \
  --suppress=missingIncludeSystem \
  --suppress=unusedFunction \
  -I engine_core/include -I editor/include \
  engine_core/src editor/src

# Run clang-tidy (requires compile_commands.json)
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
clang-tidy -p build engine_core/src/*.cpp
```

### Sanitizers

Test your code with sanitizers to catch memory and threading issues:

```bash
# AddressSanitizer (detects memory errors)
cmake -B build -DNOVELMIND_ENABLE_ASAN=ON
cmake --build build
cd build && ctest --output-on-failure

# ThreadSanitizer (detects data races)
cmake -B build -DNOVELMIND_ENABLE_TSAN=ON
cmake --build build
cd build && ctest --output-on-failure

# UndefinedBehaviorSanitizer (detects undefined behavior)
cmake -B build -DNOVELMIND_ENABLE_UBSAN=ON
cmake --build build
cd build && ctest --output-on-failure
```

### Writing Tests

- Use Catch2 for unit tests
- Place unit tests in `tests/unit/`
- Name test files `test_<component>.cpp`
- Cover edge cases and error conditions

## Questions?

If you have questions about contributing, feel free to:

1. Open an issue with the "question" label
2. Check existing documentation in `docs/`

Thank you for contributing to NovelMind!
