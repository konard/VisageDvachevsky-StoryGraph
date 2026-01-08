# Tests configuration
include(FetchContent)

# Fetch Catch2 for testing
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(Catch2)

# Qt is optional for tests; pull in targets if available.
find_package(Qt6 COMPONENTS Core Test QUIET)

# Unit tests
add_executable(unit_tests
    unit/test_result.cpp
    unit/test_timer.cpp
    unit/test_memory_fs.cpp
    unit/test_vm.cpp
    unit/test_vm_vn.cpp
    unit/test_value.cpp
    unit/test_lexer.cpp
    unit/test_parser.cpp
    unit/test_validator.cpp
    unit/test_story_graph.cpp
    unit/test_voice_integration.cpp
    unit/test_dialogue_localization.cpp
    unit/test_dialogue_box.cpp
    unit/test_animation.cpp
    unit/test_snapshot.cpp
    unit/test_fuzzing.cpp
    unit/test_texture_loading.cpp
    unit/test_voice_manifest.cpp
    unit/test_script_runtime_transition.cpp
    unit/test_runtime_config.cpp
    unit/test_division_safety.cpp
    unit/test_audio_recorder.cpp
    # unit/test_scene_graph_deep.cpp  # TODO: Fix renderer mock API mismatch
    unit/test_vfs_pack_security.cpp
    unit/test_audio_playback.cpp
    unit/test_input_manager.cpp
    unit/test_renderer_pipeline.cpp
    # Issue #179 - Comprehensive test coverage additions
    unit/test_scene_graph.cpp
    unit/test_audio_manager.cpp
    unit/test_pack_reader.cpp
    unit/test_benchmarks.cpp
    unit/test_audio_manager.cpp
)

target_link_libraries(unit_tests
    PRIVATE
        engine_core
        Catch2::Catch2WithMain
        novelmind_compiler_options
)

# Link Qt6::Core for tests that may rely on Qt types
if(TARGET Qt6::Core)
    target_link_libraries(unit_tests PRIVATE Qt6::Core)
endif()

# Register unit tests
include(CTest)
include(Catch)

# Discover tests with reasonable timeout for fuzz tests
# Windows Debug builds can be significantly slower, so we use a generous default
catch_discover_tests(unit_tests
    PROPERTIES
        TIMEOUT 60  # 60 seconds max per test
)

# Integration tests (requires editor)
if(NOVELMIND_BUILD_EDITOR)
    add_executable(integration_tests
        integration/test_editor_runtime.cpp
        integration/test_editor_settings.cpp
        integration/test_asset_browser.cpp
        integration/test_voice_manager.cpp
        integration/test_project_json.cpp
        integration/test_ui_panel_features.cpp
        integration/test_playback_source_mode.cpp  # Issue #82
        integration/test_scene_workflow.cpp  # Issue #179
        integration/test_scene_registry.cpp  # Issue #206
        unit/test_performance_cache.cpp
        unit/test_tutorial_types.cpp
        unit/test_state_machine_robustness.cpp  # Issue #173: State machine fixes
    )

    target_link_libraries(integration_tests
        PRIVATE
            engine_core
            novelmind_editor
            Catch2::Catch2WithMain
            novelmind_compiler_options
    )
    if(TARGET Qt6::Test)
        target_link_libraries(integration_tests PRIVATE Qt6::Test)
    endif()

    target_include_directories(integration_tests
        PRIVATE
            ${CMAKE_SOURCE_DIR}/editor/include
    )

    catch_discover_tests(integration_tests
        PROPERTIES
            TIMEOUT 60
    )

    # Abstraction interface unit tests (issue #150)
    # These tests verify the mock implementations work correctly
    add_executable(abstraction_interface_tests
        unit/test_abstraction_interfaces.cpp
    )

    target_link_libraries(abstraction_interface_tests
        PRIVATE
            engine_core
            novelmind_editor
            Catch2::Catch2WithMain
            novelmind_compiler_options
    )

    target_include_directories(abstraction_interface_tests
        PRIVATE
            ${CMAKE_SOURCE_DIR}/editor/include
    )

    catch_discover_tests(abstraction_interface_tests
        PROPERTIES
            TIMEOUT 60
    )
endif()
