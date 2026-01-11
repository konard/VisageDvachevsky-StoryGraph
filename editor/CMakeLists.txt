# NovelMind Visual Editor - Qt 6 Based GUI
#
# This is a complete remake of the GUI using Qt 6 Widgets,
# replacing the previous ImGui/SDL2 implementation.

# Find Qt6
find_package(Qt6 COMPONENTS Widgets Core Gui Svg OpenGLWidgets Multimedia Concurrent QUIET)

if(NOT Qt6_FOUND)
    message(STATUS "NovelMind Editor: Qt6 not found - Editor will not be built")
    message(STATUS "  Install Qt6 development packages to enable the editor:")
    message(STATUS "    Ubuntu/Debian: sudo apt-get install qt6-base-dev qt6-svg-dev")
    message(STATUS "    Fedora: sudo dnf install qt6-qtbase-devel qt6-qtsvg-devel")
    message(STATUS "    macOS: brew install qt@6")
    message(STATUS "    Windows: Download Qt6 from qt.io")

    # Disable editor build flag so tests know not to link against it
    set(NOVELMIND_BUILD_EDITOR OFF PARENT_SCOPE)
    return()
endif()

message(STATUS "NovelMind Editor: Qt6 found - Building editor with Qt6 GUI")

# Enable Qt MOC, UIC, RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Qt6 Editor Library
add_library(novelmind_editor STATIC
    # Guided Learning System (Editor-only subsystem)
    src/guided_learning/tutorial_types.cpp
    src/guided_learning/anchor_registry.cpp
    src/guided_learning/tutorial_manager.cpp
    src/guided_learning/help_overlay.cpp
    src/guided_learning/tutorial_subsystem.cpp

    # Core editor systems (backend - retained from previous implementation)
    src/editor_runtime_host.cpp
    src/editor_runtime_host_inspection.cpp
    src/editor_runtime_host_save.cpp
    src/editor_runtime_host_breakpoints.cpp
    src/editor_runtime_host_hot_reload.cpp
    src/editor_runtime_host_runtime.cpp
    src/editor_runtime_host_detail.cpp
    src/asset_pipeline.cpp
    src/editor_settings.cpp
    src/settings_registry.cpp
    src/voice_manager.cpp
    src/timeline_editor.cpp
    src/scene_document.cpp
    src/curve_editor.cpp
    src/curve_editor_library.cpp
    src/curve_editor_view.cpp
    src/selection_system.cpp
    src/event_bus.cpp
    src/project_manager.cpp
    src/inspector_binding.cpp
    src/asset_preview.cpp
    src/timeline_playback.cpp
    src/editor_state.cpp
    src/error_reporter.cpp
    src/hotkeys_manager.cpp
    src/project_integrity.cpp
    src/project_json.cpp
    src/scene_registry.cpp
    src/scene_template_manager.cpp
    src/script_project_context.cpp

    # Build System components
    src/build_system.cpp
    src/build_size_analyzer.cpp

    # Abstraction interfaces and adapters (issue #150)
    src/adapters/QtAudioPlayer.cpp
    src/adapters/QtFileSystem.cpp
    src/core/ServiceLocator.cpp
    # Panel Mediators (EventBus-based decoupled communication)
    src/mediators/selection_mediator.cpp
    src/mediators/workflow_mediator.cpp
    src/mediators/property_mediator.cpp
    src/mediators/playback_mediator.cpp
    src/mediators/scene_mediator.cpp
    include/NovelMind/editor/mediators/scene_mediator.hpp
    src/mediators/scene_registry_mediator.cpp
    include/NovelMind/editor/mediators/scene_registry_mediator.hpp
    src/mediators/panel_mediators.cpp

    # Performance optimization components
    src/qt/lazy_thumbnail_loader.cpp
    src/qt/timeline_render_cache.cpp
    src/qt/performance_metrics.cpp

    # Qt6 GUI implementation
    src/qt/qt_event_bus.cpp
    src/qt/qt_selection_manager.cpp
    src/qt/navigation_location.cpp
    src/qt/nm_style_manager.cpp
    src/qt/nm_style_manager_stylesheet.cpp
    src/qt/nm_dialogs_detail.cpp
    src/qt/nm_message_dialog.cpp
    src/qt/nm_input_dialog.cpp
    src/qt/nm_file_dialog.cpp
    src/qt/nm_color_dialog.cpp
    src/qt/nm_new_project_dialog.cpp
    src/qt/nm_voice_metadata_dialog.cpp
    src/qt/nm_bezier_curve_editor_dialog.cpp
    src/qt/nm_new_scene_dialog.cpp
    src/qt/dialogs/nm_script_welcome_dialog.cpp
    src/qt/nm_main_window.cpp
    src/qt/nm_main_window_menu.cpp
    src/qt/nm_main_window_panels.cpp
    src/qt/nm_main_window_connections.cpp
    src/qt/nm_main_window_layout.cpp
    src/qt/nm_main_window_runtime.cpp
    src/qt/nm_dock_panel.cpp
    src/qt/nm_dock_title_bar.cpp
    src/qt/nm_welcome_dialog.cpp
    src/qt/nm_hotkeys_dialog.cpp
    src/qt/nm_settings_dialog.cpp
    src/qt/nm_undo_manager.cpp
    src/qt/nm_icon_manager.cpp
    src/qt/nm_scrollable_toolbar.cpp
    src/qt/nm_fuzzy_matcher.cpp
    src/qt/nm_command_palette.cpp

    # Phase 0 & 1 panels
    src/qt/panels/nm_scene_view_panel.cpp
    src/qt/panels/nm_scene_view_panel_objects.cpp
    src/qt/panels/nm_scene_view_panel_document.cpp
    src/qt/panels/nm_scene_view_panel_ui.cpp
    src/qt/panels/nm_scene_view_panel_runtime.cpp
    src/qt/panels/nm_scene_view_object.cpp
    src/qt/panels/nm_scene_view_gizmo.cpp
    src/qt/panels/nm_scene_view_scene.cpp
    src/qt/panels/nm_scene_view_view.cpp
    src/qt/panels/nm_scene_view_overlays.hpp
    src/qt/panels/nm_story_graph_panel.cpp
    src/qt/panels/nm_story_graph_panel_handlers.cpp
    src/qt/panels/nm_story_graph_panel_detail.cpp
    src/qt/panels/nm_story_graph_node.cpp
    src/qt/panels/nm_story_graph_connection.cpp
    src/qt/panels/nm_story_graph_scene.cpp
    src/qt/panels/nm_story_graph_view.cpp
    src/qt/panels/nm_story_graph_minimap.cpp
    src/qt/panels/nm_story_graph_voice_integration.cpp
    src/qt/panels/nm_scene_dialogue_graph_panel.cpp
    src/qt/panels/nm_inspector_panel.cpp
    src/qt/panels/nm_inspector_panel_group.cpp
    src/qt/panels/nm_console_panel.cpp
    src/qt/panels/nm_asset_browser_panel.cpp
    src/qt/panels/nm_scene_palette_panel.cpp
    src/qt/panels/nm_hierarchy_panel.cpp
    src/qt/panels/nm_script_editor_panel.cpp
    src/qt/panels/nm_script_editor_panel_editor.cpp
    src/qt/panels/nm_script_editor_panel_highlighter.cpp
    src/qt/panels/nm_script_editor_panel_minimap.cpp
    src/qt/panels/nm_script_editor_panel_find_replace.cpp
    src/qt/panels/nm_script_editor_panel_palette.cpp
    src/qt/panels/nm_script_editor_panel_folding.cpp
    src/qt/panels/nm_script_editor_panel_detail.cpp
    src/qt/panels/nm_script_editor_panel_preview.cpp
    src/qt/panels/nm_script_doc_panel.cpp

    # Phase 3 & 4 panels
    src/qt/panels/nm_timeline_panel.cpp
    src/qt/panels/nm_animation_adapter.cpp
    src/qt/panels/nm_keyframe_item.cpp
    src/qt/panels/nm_curve_editor_panel.cpp
    src/qt/panels/nm_curve_point_item.cpp
    src/qt/panels/nm_voice_manager_panel.cpp
    src/qt/panels/nm_recording_studio_panel.cpp
    src/qt/panels/nm_voice_studio_panel.cpp
    src/qt/panels/nm_audio_mixer_panel.cpp
    src/qt/panels/nm_localization_panel.cpp
    src/qt/panels/nm_diagnostics_panel.cpp
    src/qt/panels/nm_build_settings_panel.cpp
    src/qt/panels/nm_project_settings_panel.cpp

    # Phase 5 - Play-In-Editor
    src/qt/nm_play_mode_controller.cpp
    src/qt/panels/nm_play_toolbar_panel.cpp
    src/qt/panels/nm_debug_overlay_panel.cpp
    src/qt/panels/nm_script_runtime_inspector_panel.cpp
    src/qt/panels/nm_issues_panel.cpp
    src/qt/panels/nm_script_inspector_panel.cpp

    # Phase 6 - Guided Learning System Panel
    src/qt/panels/nm_help_hub_panel.cpp

    # Custom Widgets
    src/qt/widgets/nm_scene_id_picker.cpp
    src/qt/widgets/nm_scene_preview_widget.cpp

    # Performance optimization headers
    include/NovelMind/editor/qt/lazy_thumbnail_loader.hpp
    include/NovelMind/editor/qt/timeline_render_cache.hpp
    include/NovelMind/editor/qt/performance_metrics.hpp

    # Qt6 Headers (required for AUTOMOC to work)
    include/NovelMind/editor/qt/qt_event_bus.hpp
    include/NovelMind/editor/qt/qt_selection_manager.hpp
    include/NovelMind/editor/qt/navigation_location.hpp
    include/NovelMind/editor/qt/nm_style_manager.hpp
    include/NovelMind/editor/qt/nm_dialogs.hpp
    include/NovelMind/editor/qt/nm_main_window.hpp
    include/NovelMind/editor/qt/nm_dock_panel.hpp
    include/NovelMind/editor/qt/nm_dock_title_bar.hpp
    include/NovelMind/editor/qt/nm_welcome_dialog.hpp
    include/NovelMind/editor/qt/dialogs/nm_script_welcome_dialog.hpp
    include/NovelMind/editor/qt/nm_hotkeys_dialog.hpp
    include/NovelMind/editor/qt/nm_settings_dialog.hpp
    include/NovelMind/editor/qt/nm_undo_manager.hpp
    include/NovelMind/editor/qt/nm_icon_manager.hpp
    include/NovelMind/editor/qt/nm_play_mode_controller.hpp
    include/NovelMind/editor/qt/nm_scrollable_toolbar.hpp
    include/NovelMind/editor/qt/nm_bezier_curve_editor_dialog.hpp
    include/NovelMind/editor/qt/nm_fuzzy_matcher.hpp
    include/NovelMind/editor/qt/nm_command_palette.hpp
    include/NovelMind/editor/qt/widgets/nm_scene_preview_widget.hpp

    # Panel headers
    include/NovelMind/editor/qt/panels/nm_scene_view_panel.hpp
    include/NovelMind/editor/qt/panels/nm_story_graph_panel.hpp
    include/NovelMind/editor/qt/panels/nm_story_graph_voice_integration.hpp
    include/NovelMind/editor/qt/panels/nm_story_graph_minimap.hpp
    include/NovelMind/editor/qt/panels/nm_scene_dialogue_graph_panel.hpp
    include/NovelMind/editor/qt/panels/nm_inspector_panel.hpp
    include/NovelMind/editor/qt/panels/nm_console_panel.hpp
    include/NovelMind/editor/qt/panels/nm_asset_browser_panel.hpp
    include/NovelMind/editor/qt/panels/nm_scene_palette_panel.hpp
    include/NovelMind/editor/qt/panels/nm_hierarchy_panel.hpp
    include/NovelMind/editor/qt/panels/nm_script_editor_panel.hpp
    include/NovelMind/editor/qt/panels/nm_script_doc_panel.hpp
    include/NovelMind/editor/qt/panels/nm_timeline_panel.hpp
    include/NovelMind/editor/qt/panels/nm_keyframe_item.hpp
    include/NovelMind/editor/qt/panels/nm_curve_editor_panel.hpp
    include/NovelMind/editor/qt/panels/nm_curve_point_item.hpp
    include/NovelMind/editor/qt/panels/nm_voice_manager_panel.hpp
    include/NovelMind/editor/qt/panels/nm_recording_studio_panel.hpp
    include/NovelMind/editor/qt/panels/nm_voice_studio_panel.hpp
    include/NovelMind/editor/qt/panels/nm_localization_panel.hpp
    include/NovelMind/editor/qt/panels/nm_diagnostics_panel.hpp
    include/NovelMind/editor/qt/panels/nm_build_settings_panel.hpp
    include/NovelMind/editor/qt/panels/nm_project_settings_panel.hpp
    include/NovelMind/editor/qt/panels/nm_play_toolbar_panel.hpp
    include/NovelMind/editor/qt/panels/nm_debug_overlay_panel.hpp
    include/NovelMind/editor/qt/panels/nm_script_runtime_inspector_panel.hpp
    include/NovelMind/editor/qt/panels/nm_issues_panel.hpp
    include/NovelMind/editor/qt/panels/nm_script_inspector_panel.hpp
    include/NovelMind/editor/qt/panels/nm_audio_mixer_panel.hpp
    include/NovelMind/editor/qt/panels/nm_animation_adapter.hpp
    include/NovelMind/editor/qt/panels/nm_help_hub_panel.hpp

    # Guided Learning System headers
    include/NovelMind/editor/guided_learning/tutorial_types.hpp
    include/NovelMind/editor/guided_learning/anchor_registry.hpp
    include/NovelMind/editor/guided_learning/tutorial_manager.hpp
    include/NovelMind/editor/guided_learning/help_overlay.hpp
    include/NovelMind/editor/guided_learning/tutorial_subsystem.hpp

    # Abstraction interface headers (issue #150)
    include/NovelMind/editor/interfaces/IAudioPlayer.hpp
    include/NovelMind/editor/interfaces/QtAudioPlayer.hpp
    include/NovelMind/editor/interfaces/MockAudioPlayer.hpp
    include/NovelMind/editor/interfaces/IFileSystem.hpp
    include/NovelMind/editor/interfaces/QtFileSystem.hpp
    include/NovelMind/editor/interfaces/MockFileSystem.hpp
    include/NovelMind/editor/interfaces/ServiceLocator.hpp
    # Panel Events and Mediators headers
    include/NovelMind/editor/events/panel_events.hpp
    include/NovelMind/editor/mediators/selection_mediator.hpp
    include/NovelMind/editor/mediators/workflow_mediator.hpp
    include/NovelMind/editor/mediators/property_mediator.hpp
    include/NovelMind/editor/mediators/playback_mediator.hpp
    include/NovelMind/editor/mediators/panel_mediators.hpp

    # Scene Registry (issue #206)
    include/NovelMind/editor/scene_registry.hpp

    # Scene Template System (issue #216)
    include/NovelMind/editor/scene_template_manager.hpp

    # Script Project Context (issue #241)
    include/NovelMind/editor/script_project_context.hpp

    # Custom Widget headers
    include/NovelMind/editor/qt/widgets/nm_scene_id_picker.hpp

    # Qt Resources
    resources/editor_resources.qrc
)

# Alias for backward compatibility
add_library(editor ALIAS novelmind_editor)

target_include_directories(novelmind_editor
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Find OpenSSL for build system encryption (same as engine_core)
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    target_link_libraries(novelmind_editor PRIVATE OpenSSL::Crypto)
    target_compile_definitions(novelmind_editor PRIVATE NOVELMIND_HAS_OPENSSL)
    message(STATUS "Editor: OpenSSL found - enabling build system encryption")
else()
    message(STATUS "Editor: OpenSSL not found - build encryption disabled")
endif()

# Find zlib for build system compression (same as engine_core)
find_package(ZLIB QUIET)
if(ZLIB_FOUND)
    target_link_libraries(novelmind_editor PRIVATE ZLIB::ZLIB)
    target_compile_definitions(novelmind_editor PRIVATE NOVELMIND_HAS_ZLIB)
    message(STATUS "Editor: zlib found - enabling build system compression")
else()
    message(STATUS "Editor: zlib not found - build compression disabled")
endif()

target_link_libraries(novelmind_editor
    PUBLIC
        engine_core
        Qt6::Widgets
        Qt6::Core
        Qt6::Gui
        Qt6::Svg
        Qt6::OpenGLWidgets
        Qt6::Multimedia
        Qt6::Concurrent
    PRIVATE
        novelmind_compiler_options
)

target_compile_definitions(novelmind_editor PRIVATE
    NOVELMIND_QT6_GUI
)

# Editor executable
add_executable(novelmind_editor_app
    src/qt/main.cpp
)

target_link_libraries(novelmind_editor_app
    PRIVATE
        novelmind_editor
        engine_core
        Qt6::Widgets
        Qt6::Core
        Qt6::Gui
        Qt6::Svg
        Qt6::OpenGLWidgets
        Qt6::Multimedia
        Qt6::Concurrent
        novelmind_compiler_options
)

# Set output name to novelmind_editor
set_target_properties(novelmind_editor_app PROPERTIES
    OUTPUT_NAME "novelmind_editor"
)

# Copy Qt DLLs on Windows (for development convenience)
if(WIN32)
    # Add Qt bin directory to PATH for running
    get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)

    # Use windeployqt for deployment
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET novelmind_editor_app POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-system-d3d-compiler
                "$<TARGET_FILE:novelmind_editor_app>"
            COMMENT "Running windeployqt..."
        )
    endif()
endif()
