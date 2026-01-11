cmake_minimum_required(VERSION 3.20)

project(NovelMind
    VERSION 0.2.0
    DESCRIPTION "A modern 2D visual novel engine"
    LANGUAGES CXX
)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE/tooling support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# Options
option(NOVELMIND_BUILD_TESTS "Build unit tests" ON)
option(NOVELMIND_BUILD_EDITOR "Build visual editor" ON)
option(NOVELMIND_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(NOVELMIND_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(NOVELMIND_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(NOVELMIND_ENABLE_LSAN "Enable LeakSanitizer" OFF)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Compiler warnings
add_library(novelmind_compiler_options INTERFACE)

target_compile_options(novelmind_compiler_options INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wcast-qual
        -Wformat=2
        -Wundef
        -Werror=return-type
        -Wshadow
        -Wcast-align
        -Wunused
        -Wnull-dereference
        -Wdouble-promotion
        -Wimplicit-fallthrough
    >
    $<$<CXX_COMPILER_ID:GNU>:
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
    >
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4
        /permissive-
        /Zc:__cplusplus
    >
)

# Sanitizers
if(NOVELMIND_ENABLE_ASAN)
    target_compile_options(novelmind_compiler_options INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
            -fsanitize=address
            -fno-omit-frame-pointer
        >
    )
    target_link_options(novelmind_compiler_options INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
            -fsanitize=address
        >
    )
endif()

if(NOVELMIND_ENABLE_TSAN)
    target_compile_options(novelmind_compiler_options INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
            -fsanitize=thread
            -fno-omit-frame-pointer
        >
    )
    target_link_options(novelmind_compiler_options INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
            -fsanitize=thread
        >
    )
endif()

if(NOVELMIND_ENABLE_UBSAN)
    target_compile_options(novelmind_compiler_options INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
            -fsanitize=undefined
            -fno-omit-frame-pointer
        >
    )
    target_link_options(novelmind_compiler_options INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
            -fsanitize=undefined
        >
    )
endif()

if(NOVELMIND_ENABLE_LSAN)
    target_compile_options(novelmind_compiler_options INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
            -fsanitize=leak
            -fno-omit-frame-pointer
        >
    )
    target_link_options(novelmind_compiler_options INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
            -fsanitize=leak
        >
    )
endif()

# Engine core library
add_subdirectory(engine_core)

# Editor (optional) - process before tests so tests know if editor is available
if(NOVELMIND_BUILD_EDITOR)
    add_subdirectory(editor)
endif()

# Tests - processed after editor so NOVELMIND_BUILD_EDITOR reflects actual state
if(NOVELMIND_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Compiler (nmc - NovelMind Script Compiler)
add_subdirectory(compiler)

# Runtime (console-based visual novel player)
add_subdirectory(runtime)

# Installation
include(GNUInstallDirs)
install(TARGETS engine_core
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY engine_core/include/NovelMind
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
