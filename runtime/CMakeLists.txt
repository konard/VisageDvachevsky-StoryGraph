# NovelMind Runtime
# Console-based runtime for playing visual novels

# Create the console runtime executable
add_executable(novelmind_runtime
    src/main.cpp
)

target_link_libraries(novelmind_runtime
    PRIVATE
        engine_core
        novelmind_compiler_options
)

# Set output name
set_target_properties(novelmind_runtime PROPERTIES
    OUTPUT_NAME "novelmind_runtime"
)

# Create the game launcher executable
# This is the main entry point for end-users to play games
# Users can double-click this .exe to start playing without CLI args
add_executable(game_launcher
    src/game_launcher_main.cpp
)

target_link_libraries(game_launcher
    PRIVATE
        engine_core
        novelmind_compiler_options
)

# Set output name and properties
# The game_launcher is the primary executable in the build folder
set_target_properties(game_launcher PROPERTIES
    OUTPUT_NAME "game_launcher"
)

# On Windows, use WIN32 to hide the console for release builds
if(WIN32)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set_target_properties(game_launcher PROPERTIES
            WIN32_EXECUTABLE TRUE
        )
    endif()
endif()

# On macOS, set properties for app bundle (optional)
if(APPLE)
    set_target_properties(game_launcher PROPERTIES
        MACOSX_BUNDLE FALSE
    )
endif()

# Installation
# For development builds, install to bin/
install(TARGETS novelmind_runtime game_launcher
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ==============================================================================
# Game Bundle Installation Target
# ==============================================================================
# This creates a complete game bundle in the install directory with:
# - game_launcher (main executable at root)
# - config/ (runtime_config.json example)
# - saves/ (empty directory for save files)
# - logs/ (empty directory for log files)
# - packs/ (empty directory for game packs)
#
# For Windows builds: game_launcher.exe is the main entry point
# For macOS builds: game_launcher is the main entry point (or .app bundle)
# For Linux builds: game_launcher is the main entry point

# Install sample config for new games
install(FILES
    ${CMAKE_SOURCE_DIR}/examples/sample_novel/config/runtime_config.json
    DESTINATION ${CMAKE_INSTALL_PREFIX}/config
    OPTIONAL
)

# Create required directories in the install prefix
install(CODE "
    file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/saves\")
    file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/logs\")
    file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/packs\")
    file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/config\")
")
