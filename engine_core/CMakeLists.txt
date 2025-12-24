add_library(engine_core STATIC
    # Core
    src/core/logger.cpp
    src/core/application.cpp
    src/core/timer.cpp
    src/core/file_system.cpp
    src/core/profiler.cpp
    src/core/debug_overlay.cpp
    src/core/property_system.cpp

    # Platform
    src/core/platform_sdl.cpp

    # VFS (Legacy)
    src/vfs/virtual_fs.cpp
    src/vfs/memory_fs.cpp
    src/vfs/pack_reader.cpp
    src/vfs/cached_file_system.cpp

    # VFS (Enhanced)
    src/vfs/file_handle.cpp
    src/vfs/resource_id.cpp
    src/vfs/file_system_backend.cpp
    src/vfs/resource_cache.cpp
    src/vfs/virtual_file_system.cpp
    src/vfs/pack_security.cpp
    src/vfs/pack_integrity_checker.cpp
    src/vfs/pack_decryptor.cpp
    src/vfs/pack_security_detail.cpp
    src/vfs/secure_pack_reader.cpp

    # Renderer
    src/renderer/renderer.cpp
    src/renderer/texture.cpp
    src/renderer/stb_image_impl.cpp
    src/renderer/sprite.cpp
    src/renderer/camera.cpp
    src/renderer/font.cpp

    # Scripting
    src/scripting/interpreter.cpp
    src/scripting/vm.cpp
    src/scripting/vm_security.cpp
    src/scripting/lexer.cpp
    src/scripting/parser.cpp
    src/scripting/compiler.cpp
    src/scripting/validator.cpp
    src/scripting/script_runtime.cpp
    src/scripting/ir_core.cpp
    src/scripting/ir_conversion.cpp
    src/scripting/ir_visual_graph.cpp
    src/scripting/ir_round_trip.cpp
    src/scripting/dialogue_localization.cpp

    # Renderer (Text)
    src/renderer/text_layout.cpp

    # Resources
    src/resource/resource_manager.cpp

    # Scene
    src/scene/scene_manager.cpp
    src/scene/scene_object.cpp
    src/scene/character_sprite.cpp
    src/scene/dialogue_box.cpp
    src/scene/choice_menu.cpp
    src/scene/transition.cpp
    src/scene/scene_graph.cpp
    src/scene/scene_graph_detail.cpp
    src/scene/scene_layer.cpp
    src/scene/scene_object_base.cpp
    src/scene/scene_object_background.cpp
    src/scene/scene_object_character.cpp
    src/scene/scene_object_dialogue.cpp
    src/scene/scene_object_choice.cpp
    src/scene/scene_object_effect.cpp
    src/scene/scene_inspector.cpp
    src/scene/scene_object_properties.cpp
    src/scene/scene_object_handle.cpp

    # Input
    src/input/input_manager.cpp

    # Audio
    src/audio/audio_manager.cpp
    src/audio/miniaudio_impl.cpp
    src/audio/voice_manifest.cpp
    src/audio/audio_recorder.cpp

    # Save
    src/save/save_manager.cpp

    # UI Framework
    src/ui/ui_theme.cpp
    src/ui/ui_widget.cpp
    src/ui/ui_containers.cpp
    src/ui/ui_controls.cpp
    src/ui/ui_manager.cpp

    # Localization
    src/localization/localization_manager.cpp

    # VFS Multi-Pack
    src/vfs/multi_pack_manager.cpp
)

set_source_files_properties(
    src/renderer/stb_image_impl.cpp
    src/audio/miniaudio_impl.cpp
    PROPERTIES
        COMPILE_OPTIONS
        "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-w>"
)

target_include_directories(engine_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(engine_core
    PRIVATE
        novelmind_compiler_options
)

# Find SDL2 (optional for now, will be required later)
find_package(SDL2 QUIET)
if(SDL2_FOUND)
    target_link_libraries(engine_core PRIVATE SDL2::SDL2)
    target_compile_definitions(engine_core PRIVATE NOVELMIND_HAS_SDL2)
endif()

# OpenGL backend
find_package(OpenGL QUIET)
if(OPENGL_FOUND)
    target_link_libraries(engine_core PRIVATE OpenGL::GL)
    target_compile_definitions(engine_core PRIVATE NOVELMIND_HAS_OPENGL)
endif()

# Find OpenSSL for cryptographic operations
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    target_link_libraries(engine_core PRIVATE OpenSSL::Crypto)
    target_compile_definitions(engine_core PRIVATE NOVELMIND_HAS_OPENSSL)
    message(STATUS "OpenSSL found - enabling real cryptographic functions")
else()
    message(STATUS "OpenSSL not found - pack crypto and signatures disabled")
endif()

# Find zlib for pack decompression (optional)
find_package(ZLIB QUIET)
if(ZLIB_FOUND)
    target_link_libraries(engine_core PRIVATE ZLIB::ZLIB)
    target_compile_definitions(engine_core PRIVATE NOVELMIND_HAS_ZLIB)
    message(STATUS "zlib found - enabling pack decompression")
endif()

# FreeType for text rendering
find_package(Freetype QUIET)
if(Freetype_FOUND)
    target_link_libraries(engine_core PRIVATE Freetype::Freetype)
    target_compile_definitions(engine_core PRIVATE NOVELMIND_HAS_FREETYPE)
    message(STATUS "Freetype found - enabling text rendering")
else()
    message(STATUS "Freetype not found - text rendering fallback will be used")
endif()

# Define version
target_compile_definitions(engine_core
    PUBLIC
        NOVELMIND_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        NOVELMIND_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        NOVELMIND_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)
