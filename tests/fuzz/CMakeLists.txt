# Fuzzing targets for NovelMind Script processing pipeline
# Uses libFuzzer (Clang) or compatible fuzzing engines
#
# Build with:
#   cmake -B build -DNOVELMIND_ENABLE_FUZZING=ON -DCMAKE_CXX_COMPILER=clang++
#   cmake --build build

# Only build fuzz targets if explicitly enabled
if(NOT NOVELMIND_ENABLE_FUZZING)
    message(STATUS "Fuzzing disabled. Use -DNOVELMIND_ENABLE_FUZZING=ON to enable.")
    return()
endif()

# Fuzzing requires Clang with libFuzzer support
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "Fuzzing targets require Clang compiler. Skipping fuzz targets.")
    return()
endif()

message(STATUS "Building fuzzing targets with libFuzzer")

# Common fuzzing flags
set(FUZZ_FLAGS
    -fsanitize=fuzzer,address,undefined
    -fno-omit-frame-pointer
    -g
)

# Lexer fuzzer
add_executable(fuzz_lexer fuzz_lexer.cpp)
target_link_libraries(fuzz_lexer PRIVATE engine_core)
target_compile_options(fuzz_lexer PRIVATE ${FUZZ_FLAGS})
target_link_options(fuzz_lexer PRIVATE ${FUZZ_FLAGS})

# Parser fuzzer
add_executable(fuzz_parser fuzz_parser.cpp)
target_link_libraries(fuzz_parser PRIVATE engine_core)
target_compile_options(fuzz_parser PRIVATE ${FUZZ_FLAGS})
target_link_options(fuzz_parser PRIVATE ${FUZZ_FLAGS})

# VM fuzzer
add_executable(fuzz_vm fuzz_vm.cpp)
target_link_libraries(fuzz_vm PRIVATE engine_core)
target_compile_options(fuzz_vm PRIVATE ${FUZZ_FLAGS})
target_link_options(fuzz_vm PRIVATE ${FUZZ_FLAGS})

# Create corpus directories for seed inputs
set(CORPUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/corpus)
file(MAKE_DIRECTORY ${CORPUS_DIR}/lexer)
file(MAKE_DIRECTORY ${CORPUS_DIR}/parser)
file(MAKE_DIRECTORY ${CORPUS_DIR}/vm)

# Installation of fuzz targets (optional)
install(TARGETS fuzz_lexer fuzz_parser fuzz_vm
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/fuzz
    OPTIONAL
)

message(STATUS "Fuzzing targets built:")
message(STATUS "  - fuzz_lexer: Tests lexer with arbitrary input")
message(STATUS "  - fuzz_parser: Tests parser with arbitrary input")
message(STATUS "  - fuzz_vm: Tests full pipeline (lexer -> parser -> compiler -> VM)")
message(STATUS "")
message(STATUS "Run fuzzing with:")
message(STATUS "  ./fuzz_lexer corpus/lexer -max_total_time=60")
message(STATUS "  ./fuzz_parser corpus/parser -max_total_time=60")
message(STATUS "  ./fuzz_vm corpus/vm -max_total_time=60")
