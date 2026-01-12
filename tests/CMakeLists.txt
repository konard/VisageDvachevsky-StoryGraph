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
    unit/test_compiler.cpp
    unit/test_story_graph.cpp
    unit/test_dialogue_localization.cpp
    unit/test_localization_manager.cpp
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
    unit/test_scene_graph_deep.cpp
    unit/test_vfs_pack_security.cpp
    unit/test_audio_playback.cpp
    unit/test_input_manager.cpp
    unit/test_renderer_pipeline.cpp
    unit/test_scene_graph.cpp
    unit/test_audio_manager.cpp
    unit/test_pack_reader.cpp
    unit/test_benchmarks.cpp
    unit/test_audio_manager.cpp
    unit/test_clipboard.cpp
    unit/test_recording_panel_thread_safety.cpp
    unit/test_recording_callback_marshaling.cpp
    unit/test_secure_memory.cpp
    unit/test_save_manager.cpp
    unit/test_resource_cache.cpp
    unit/test_resource_id.cpp
    unit/test_resource_manager.cpp
    unit/test_cached_file_system.cpp
    unit/test_event_bus.cpp
    unit/test_interpreter_deserialize.cpp
    unit/test_compiler_float_serialization.cpp
    unit/test_scene_object_handle_thread_safety.cpp
)
# Note: test_build_system.cpp requires editor library, included only in integration_tests

target_link_libraries(unit_tests
    PRIVATE
        engine_core
        Catch2::Catch2WithMain
        novelmind_compiler_options
)

# Link editor library if available (for build_system tests)
if(TARGET novelmind_editor)
    target_link_libraries(unit_tests PRIVATE novelmind_editor)
endif()

# Link Qt6::Core for tests that may rely on Qt types
if(TARGET Qt6::Core)
    target_link_libraries(unit_tests PRIVATE Qt6::Core)
endif()

# Register unit tests
include(CTest)
include(Catch)

# Discover tests with reasonable timeout for fuzz tests
# Windows Debug builds can be significantly slower, so we use a generous default
# SKIP_RETURN_CODE 4: Catch2 returns exit code 4 for skipped tests (SKIP macro)
catch_discover_tests(unit_tests
    PROPERTIES
        TIMEOUT 60  # 60 seconds max per test
        SKIP_RETURN_CODE 4
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
        integration/test_playback_source_mode.cpp
        integration/test_scene_workflow.cpp
        integration/test_scene_registry.cpp
        integration/test_scene_template_manager.cpp
        integration/test_scene_validation.cpp
        integration/test_scene_id_picker.cpp
        unit/test_voice_integration.cpp
        unit/test_performance_cache.cpp
        unit/test_tutorial_types.cpp
        unit/test_state_machine_robustness.cpp
        unit/test_project_integrity.cpp
        unit/test_event_bus.cpp
        unit/test_gizmo_memory.cpp
        unit/test_gizmo_dpi_scaling.cpp
        unit/test_gizmo_rotation_normalization.cpp
        unit/test_build_size_analyzer.cpp
        unit/test_build_system.cpp
        unit/test_selection_mediator.cpp
        unit/test_property_mediator.cpp
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
            SKIP_RETURN_CODE 4
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
            SKIP_RETURN_CODE 4
    )
endif()
