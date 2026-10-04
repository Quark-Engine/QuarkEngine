cmake_minimum_required(VERSION 3.10)
project(QuarkEngine)

execute_process(
    COMMAND git rev-parse --short=7 HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE QUARK_ENGINE_BUILD
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE GIT_REV_PARSE_RESULT
)
if(NOT GIT_REV_PARSE_RESULT EQUAL 0 OR QUARK_ENGINE_BUILD STREQUAL "")
    set(QUARK_ENGINE_BUILD "unknown")
    message(WARNING "Git commit hash unavailable; build number will be 'unknown'.")
endif()
string(TIMESTAMP QUARK_ENGINE_BUILD_DATE "%Y-%m-%d" UTC)

if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/MD /Od /RTC1 /DNDEBUG" CACHE STRING "" FORCE)
    set(CMAKE_C_FLAGS_RELEASE   "/MD /Od /RTC1 /DNDEBUG" CACHE STRING "" FORCE)
    string(APPEND CMAKE_EXE_LINKER_FLAGS_DEBUG " /IGNORE:4099")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_ENGINE "Build Quark Engine" ON)
option(QUARK_BUILD_TESTS "Build Quark Engine unit tests" OFF)
option(BUILD_PLUGIN "Build Plugin" OFF)

set(QC_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(QC_ENABLE_OPENGL ON CACHE BOOL "" FORCE)
set(QC_ENABLE_VULKAN ON CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS    ON CACHE BOOL "" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
if(APPLE)
    set(CMAKE_INSTALL_RPATH "@loader_path")
elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}/quark-libs/QuarkCore/include
    ${CMAKE_SOURCE_DIR}/quark-libs/imgui/include
    ${CMAKE_SOURCE_DIR}/imgui
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/plugins
    ${CMAKE_SOURCE_DIR}/src
)

add_subdirectory(quark-libs/QuarkCore)

add_library(imgui SHARED
    quark-libs/imgui/include/imgui.cpp
    quark-libs/imgui/include/imgui_draw.cpp
    quark-libs/imgui/include/imgui_widgets.cpp
    quark-libs/imgui/include/imgui_tables.cpp
    imgui/imgui_impl_sdl3.cpp
    src/ImGuizmo.cpp
)

add_library(imgui-opengl SHARED imgui/imgui_impl_opengl3.cpp)
add_library(imgui-vulkan SHARED imgui/imgui_impl_vulkan.cpp)
if(WIN32)
    add_library(imgui-direct3d11 SHARED imgui/imgui_impl_dx11.cpp)
endif()

target_include_directories(imgui PUBLIC
    ${CMAKE_SOURCE_DIR}/quark-libs/imgui/include
    ${CMAKE_SOURCE_DIR}/imgui
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/quark-libs/freetype/include
)

target_link_libraries(imgui PUBLIC
    SDL3::SDL3
)

foreach(IMGUI_BACKEND_TARGET IN ITEMS imgui-opengl imgui-vulkan)
    target_include_directories(${IMGUI_BACKEND_TARGET} PUBLIC
        ${CMAKE_SOURCE_DIR}/quark-libs/imgui/include
        ${CMAKE_SOURCE_DIR}/imgui
    )
    target_link_libraries(${IMGUI_BACKEND_TARGET} PUBLIC imgui)
    if(WIN32)
        target_compile_definitions(${IMGUI_BACKEND_TARGET} PRIVATE
            IMGUI_API=__declspec\(dllimport\)
            IMGUI_IMPL_API=__declspec\(dllexport\)
        )
    else()
        target_compile_definitions(${IMGUI_BACKEND_TARGET} PRIVATE
            "IMGUI_API=__attribute__((visibility(\"default\")))"
            "IMGUI_IMPL_API=__attribute__((visibility(\"default\")))"
        )
    endif()
endforeach()
target_link_libraries(imgui-vulkan PUBLIC Vulkan::Vulkan)
if(WIN32)
    target_include_directories(imgui-direct3d11 PUBLIC
        ${CMAKE_SOURCE_DIR}/quark-libs/imgui/include
        ${CMAKE_SOURCE_DIR}/imgui
    )
    target_link_libraries(imgui-direct3d11 PUBLIC imgui d3d11)
    target_compile_definitions(imgui-direct3d11 PRIVATE
        IMGUI_API=__declspec\(dllimport\)
        IMGUI_IMPL_API=__declspec\(dllexport\)
    )
endif()

if(WIN32)
    target_compile_definitions(imgui
        PRIVATE BUILD_LIBTYPE_SHARED
        PRIVATE IMGUI_API=__declspec\(dllexport\)
    )
else()
    target_compile_definitions(imgui
        PRIVATE BUILD_LIBTYPE_SHARED
        PRIVATE "IMGUI_API=__attribute__((visibility(\"default\")))"
    )
endif()

set_target_properties(imgui PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)

if(APPLE)
    set_target_properties(imgui PROPERTIES
        BUILD_RPATH "@loader_path"
        INSTALL_RPATH "@loader_path"
    )

    set_target_properties(imgui-opengl imgui-vulkan PROPERTIES
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
    )
    if(WIN32)
        set_target_properties(imgui-direct3d11 PROPERTIES
            PREFIX ""
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
            LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
            ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
        )
    endif()
elseif(UNIX)
    set_target_properties(imgui PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
endif()

if(BUILD_ENGINE)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

    set(EDITOR_FILES
        src/editor/editor_assets.cpp
        src/editor/editor_components_ui.cpp
        src/editor/editor_desktop.cpp
        src/editor/editor_entity.cpp
        src/editor/editor_entity_clipboard.cpp
        src/editor/editor_entity_commands.cpp
        src/editor/editor_gizmo.cpp
        src/editor/editor_grid.cpp
        src/editor/editor_language_catalog.cpp
        src/editor/editor_layout.cpp
        src/editor/editor_mesh_edit.cpp
        src/editor/editor_polygon_edit.cpp
        src/editor/editor_scene_drop.cpp
        src/editor/editor_scene_file.cpp
        src/editor/editor_scene_picker.cpp
        src/editor/editor_status_bar.cpp
        src/editor/editor_theme.cpp
        src/editor/editor_ui.cpp
        src/editor/editor_utils.cpp
        src/editor/editor_viewers.cpp
        src/editor/editor.cpp
        src/editor/editor_hierarchy_utils.cpp
        src/editor/editor_preferences.cpp
        src/editor/editor_file_utils.cpp
    )

    set(SOURCE_FILES
        src/application.cpp
        src/application_plugin_bridge.cpp
        src/application_scene_renderer.cpp
        src/camera.cpp
        src/component.cpp
        src/command_line.cpp
        src/dynamic_library.cpp
        src/editable_mesh.cpp
        src/assets/asset_library.cpp
        src/assets/preview_cache.cpp
        src/assets/texture_cache.cpp
        src/engine/transform.cpp
        src/engine/component_factory_registry.cpp
        src/engine/scene_document.cpp
        src/engine/scene_runtime.cpp
        src/engine/material_texture_restore.cpp
        src/entity.cpp
        src/hub.cpp
        src/language_manager.cpp
        src/lighting.cpp
        src/main.cpp
        src/models.cpp
        src/project.cpp
        src/qcImGui.cpp
        src/plugins/plugin_manager.cpp
        src/scene.cpp
        src/tex.cpp
        src/text_mesh.cpp
    )

    add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${EDITOR_FILES})

    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
    )

    if(WIN32)
        target_sources(${PROJECT_NAME} PRIVATE QuarkEngine.rc)

        if(MSVC)
            target_link_options(${PROJECT_NAME} PRIVATE
                "$<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>"
                "$<$<CONFIG:Release>:/ENTRY:mainCRTStartup>"
            )
        else()
            target_link_options(${PROJECT_NAME} PRIVATE
                "$<$<CONFIG:Release>:-mwindows>"
                "$<$<CONFIG:Release>:-Wl,-e,mainCRTStartup>"
            )
        endif()
    endif()

    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/quark-libs/freetype/include
        ${CMAKE_SOURCE_DIR}/include
    )

    if(WIN32)
        target_compile_definitions(${PROJECT_NAME}
            PRIVATE USE_LIBTYPE_SHARED
            PRIVATE IMGUI_API=__declspec\(dllimport\)
        )
    else()
        target_compile_definitions(${PROJECT_NAME}
            PRIVATE USE_LIBTYPE_SHARED
            PRIVATE "IMGUI_API=__attribute__((visibility(\"default\")))"
        )
    endif()

    target_link_libraries(${PROJECT_NAME} PRIVATE
        QuarkCore-Combined
        imgui
        imgui-opengl
        imgui-vulkan
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        QUARKENGINE_BUILD
        QUARKCORE_STATIC
        QUARK_ENGINE_BUILD="${QUARK_ENGINE_BUILD}"
        QUARK_ENGINE_BUILD_DATE="${QUARK_ENGINE_BUILD_DATE}"
    )
    if(WIN32)
        target_link_libraries(${PROJECT_NAME} PRIVATE imgui-direct3d11)
    endif()

    if(WIN32)
        target_link_libraries(${PROJECT_NAME} PRIVATE winmm gdi32 opengl32)
    elseif(UNIX AND NOT APPLE)
        target_link_libraries(${PROJECT_NAME} PRIVATE GL m pthread dl rt X11)
    elseif(APPLE)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            "-framework Cocoa"
            "-framework OpenGL"
            "-framework IOKit"
            "-framework CoreAudio"
            "-framework CoreVideo"
        )
    endif()

    if(APPLE)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            BUILD_RPATH "@loader_path"
            INSTALL_RPATH "@loader_path"
        )
    elseif(UNIX)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            BUILD_RPATH "$ORIGIN"
            INSTALL_RPATH "$ORIGIN"
        )
    endif()

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    )

    if(WIN32)
        set(MINGW_RUNTIME_DLLS)
        if(NOT MSVC)
            get_filename_component(MINGW_RUNTIME_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
            foreach(MINGW_RUNTIME IN ITEMS libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll)
                if(EXISTS "${MINGW_RUNTIME_DIR}/${MINGW_RUNTIME}")
                    list(APPEND MINGW_RUNTIME_DLLS "${MINGW_RUNTIME_DIR}/${MINGW_RUNTIME}")
                endif()
            endforeach()
        endif()

        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )

        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:QuarkCore>
                $<TARGET_FILE:QuarkCore-OpenGL>
                $<TARGET_FILE:QuarkCore-Vulkan>
                $<TARGET_FILE:QuarkCore-D3D11>
                $<TARGET_FILE:imgui>
                $<TARGET_FILE:imgui-opengl>
                $<TARGET_FILE:imgui-vulkan>
                $<$<BOOL:${WIN32}>:$<TARGET_FILE:imgui-direct3d11>>
                $<TARGET_FILE:SDL3::SDL3>
                $<TARGET_FILE:assimp::assimp>
                $<TARGET_FILE:ZLIB::ZLIB>
                $<TARGET_FILE:freetype>
                $<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/quark-libs/QuarkCore/external/OpenAL/lib/debug/OpenAL32.dll,${CMAKE_SOURCE_DIR}/quark-libs/QuarkCore/external/OpenAL/lib/release/OpenAL32.dll>
                "${VULKAN_RUNTIME_DLL}"
                ${MINGW_RUNTIME_DLLS}
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
        )
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/quark-libs/QuarkCore/external/Vulkan/lib/SPIRV-Toolsd.dll,${CMAKE_SOURCE_DIR}/quark-libs/QuarkCore/external/Vulkan/lib/SPIRV-Tools.dll>
                $<TARGET_FILE_DIR:${PROJECT_NAME}>/SPIRV-Tools.dll
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/quark-libs/QuarkCore/external/Vulkan/lib/SPIRV-Tools-optd.dll,${CMAKE_SOURCE_DIR}/quark-libs/QuarkCore/external/Vulkan/lib/SPIRV-Tools-opt.dll>
                $<TARGET_FILE_DIR:${PROJECT_NAME}>/SPIRV-Tools-opt.dll
        )
    endif()

    if(UNIX)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:QuarkCore>
                $<TARGET_FILE:QuarkCore-OpenGL>
                $<TARGET_FILE:QuarkCore-Vulkan>
                $<TARGET_FILE:imgui>
                $<TARGET_FILE:imgui-opengl>
                $<TARGET_FILE:imgui-vulkan>
                $<TARGET_FILE:assimp::assimp>
                $<TARGET_FILE:freetype>
                $<TARGET_FILE:SDL3::SDL3>
                $<TARGET_FILE:OpenAL::OpenAL>
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMENT "Copying shared libraries next to ${PROJECT_NAME} (Linux/macOS)..."
        )
        if(APPLE)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E rm -f $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<IF:$<CONFIG:Debug>,libassimpd.6.dylib,libassimp.6.dylib>
                COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:assimp::assimp> $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<IF:$<CONFIG:Debug>,libassimpd.6.dylib,libassimp.6.dylib>
                COMMAND ${CMAKE_COMMAND} -E rm -f $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<IF:$<CONFIG:Debug>,libfreetyped.6.dylib,libfreetype.6.dylib>
                COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:freetype> $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<IF:$<CONFIG:Debug>,libfreetyped.6.dylib,libfreetype.6.dylib>
                COMMAND ${CMAKE_COMMAND} -E rm -f $<TARGET_FILE_DIR:${PROJECT_NAME}>/libopenal.1.dylib
                COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:OpenAL::OpenAL> $<TARGET_FILE_DIR:${PROJECT_NAME}>/libopenal.1.dylib
                COMMENT "Creating macOS shared library compatibility symlinks..."
            )
        else()
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:freetype> $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<IF:$<CONFIG:Debug>,libfreetyped.so.6,libfreetype.so.6>
                COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:assimp::assimp> $<TARGET_FILE_DIR:${PROJECT_NAME}>/$<IF:$<CONFIG:Debug>,libassimpd.so.6,libassimp.so.6>
                COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:SDL3::SDL3> $<TARGET_FILE_DIR:${PROJECT_NAME}>/libSDL3.so.0
                COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:OpenAL::OpenAL> $<TARGET_FILE_DIR:${PROJECT_NAME}>/libopenal.so.1
                COMMENT "Creating shared library symlinks next to ${PROJECT_NAME} (Linux)..."
            )
        endif()
    endif()
endif()

if(QUARK_BUILD_TESTS)
    enable_testing()

    add_executable(QuarkEngineTests
        tests/test_main.cpp
        tests/plugin_event_stub.cpp
        tests/test_transform.cpp
        tests/test_hierarchy.cpp
        tests/test_light_registry.cpp
        tests/test_scene_document.cpp
        tests/test_direct_texture.cpp
        src/component.cpp
        src/editable_mesh.cpp
        src/engine/scene_document.cpp
        src/engine/material_texture_restore.cpp
        src/engine/transform.cpp
        src/engine/component_factory_registry.cpp
        src/entity.cpp
        src/editor/editor_hierarchy_utils.cpp
        src/lighting.cpp
    )

    target_include_directories(QuarkEngineTests PRIVATE
        ${CMAKE_SOURCE_DIR}/quark-libs/QuarkCore/include
        ${CMAKE_SOURCE_DIR}/quark-libs/imgui/include
        ${CMAKE_SOURCE_DIR}/imgui
        ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(QuarkEngineTests PRIVATE QuarkCore-Combined)

    if(WIN32)
        target_compile_definitions(QuarkEngineTests PRIVATE
            USE_LIBTYPE_SHARED
            IMGUI_API=__declspec\(dllimport\)
        )
        target_link_libraries(QuarkEngineTests PRIVATE winmm gdi32 opengl32)
    else()
        target_compile_definitions(QuarkEngineTests PRIVATE
            USE_LIBTYPE_SHARED
            "IMGUI_API=__attribute__((visibility(\"default\")))"
        )
        if(UNIX AND NOT APPLE)
            target_link_libraries(QuarkEngineTests PRIVATE GL m pthread dl rt X11)
        elseif(APPLE)
            target_link_libraries(QuarkEngineTests PRIVATE
                "-framework Cocoa"
                "-framework OpenGL"
                "-framework IOKit"
                "-framework CoreAudio"
                "-framework CoreVideo"
            )
        endif()
    endif()

    target_compile_definitions(QuarkEngineTests PRIVATE QUARKENGINE_BUILD QUARKCORE_STATIC)

    add_test(NAME QuarkEngineTests COMMAND QuarkEngineTests)

    if(WIN32)
        add_custom_command(TARGET QuarkEngineTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:QuarkCore-OpenGL>
                $<TARGET_FILE:QuarkCore-Vulkan>
                $<TARGET_FILE:QuarkCore-D3D11>
                $<TARGET_FILE:SDL3::SDL3>
                $<TARGET_FILE:assimp::assimp>
                $<TARGET_FILE:freetype>
                $<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/quark-libs/QuarkCore/external/OpenAL/lib/debug/OpenAL32.dll,${CMAKE_SOURCE_DIR}/quark-libs/QuarkCore/external/OpenAL/lib/release/OpenAL32.dll>
                "${VULKAN_RUNTIME_DLL}"
                $<TARGET_FILE_DIR:QuarkEngineTests>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/quark-libs/QuarkCore/external/Vulkan/lib/SPIRV-Toolsd.dll,${CMAKE_SOURCE_DIR}/quark-libs/QuarkCore/external/Vulkan/lib/SPIRV-Tools.dll>
                $<TARGET_FILE_DIR:QuarkEngineTests>/SPIRV-Tools.dll
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/quark-libs/QuarkCore/external/Vulkan/lib/SPIRV-Tools-optd.dll,${CMAKE_SOURCE_DIR}/quark-libs/QuarkCore/external/Vulkan/lib/SPIRV-Tools-opt.dll>
                $<TARGET_FILE_DIR:QuarkEngineTests>/SPIRV-Tools-opt.dll
        )
    elseif(UNIX)
        add_custom_command(TARGET QuarkEngineTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:QuarkCore>
                $<TARGET_FILE:QuarkCore-OpenGL>
                $<TARGET_FILE:QuarkCore-Vulkan>
                $<TARGET_FILE:assimp::assimp>
                $<TARGET_FILE:freetype>
                $<TARGET_FILE:SDL3::SDL3>
                $<TARGET_FILE:OpenAL::OpenAL>
                $<TARGET_FILE_DIR:QuarkEngineTests>
        )
        if(APPLE)
            add_custom_command(TARGET QuarkEngineTests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E rm -f
                    "$<TARGET_FILE_DIR:QuarkEngineTests>/$<IF:$<CONFIG:Debug>,libassimpd.6.dylib,libassimp.6.dylib>"
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                    "$<TARGET_FILE_NAME:assimp::assimp>"
                    "$<TARGET_FILE_DIR:QuarkEngineTests>/$<IF:$<CONFIG:Debug>,libassimpd.6.dylib,libassimp.6.dylib>"
                COMMAND ${CMAKE_COMMAND} -E rm -f
                    "$<TARGET_FILE_DIR:QuarkEngineTests>/$<IF:$<CONFIG:Debug>,libfreetyped.6.dylib,libfreetype.6.dylib>"
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                    "$<TARGET_FILE_NAME:freetype>"
                    "$<TARGET_FILE_DIR:QuarkEngineTests>/$<IF:$<CONFIG:Debug>,libfreetyped.6.dylib,libfreetype.6.dylib>"
                COMMAND ${CMAKE_COMMAND} -E rm -f
                    "$<TARGET_FILE_DIR:QuarkEngineTests>/libopenal.1.dylib"
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                    "$<TARGET_FILE_NAME:OpenAL::OpenAL>"
                    "$<TARGET_FILE_DIR:QuarkEngineTests>/libopenal.1.dylib"
                COMMENT "Creating macOS runtime library compatibility symlinks for tests..."
            )
        else()
            add_custom_command(TARGET QuarkEngineTests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E rm -f
                    "$<TARGET_FILE_DIR:QuarkEngineTests>/$<IF:$<CONFIG:Debug>,libfreetyped.so.6,libfreetype.so.6>"
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                    "$<TARGET_FILE_NAME:freetype>"
                    "$<TARGET_FILE_DIR:QuarkEngineTests>/$<IF:$<CONFIG:Debug>,libfreetyped.so.6,libfreetype.so.6>"
                COMMAND ${CMAKE_COMMAND} -E rm -f
                    "$<TARGET_FILE_DIR:QuarkEngineTests>/$<IF:$<CONFIG:Debug>,libassimpd.so.6,libassimp.so.6>"
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                    "$<TARGET_FILE_NAME:assimp::assimp>"
                    "$<TARGET_FILE_DIR:QuarkEngineTests>/$<IF:$<CONFIG:Debug>,libassimpd.so.6,libassimp.so.6>"
                COMMAND ${CMAKE_COMMAND} -E rm -f
                    "$<TARGET_FILE_DIR:QuarkEngineTests>/libSDL3.so.0"
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                    "$<TARGET_FILE_NAME:SDL3::SDL3>"
                    "$<TARGET_FILE_DIR:QuarkEngineTests>/libSDL3.so.0"
                COMMAND ${CMAKE_COMMAND} -E rm -f
                    "$<TARGET_FILE_DIR:QuarkEngineTests>/libopenal.so.1"
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                    "$<TARGET_FILE_NAME:OpenAL::OpenAL>"
                    "$<TARGET_FILE_DIR:QuarkEngineTests>/libopenal.so.1"
                COMMENT "Creating Linux runtime library compatibility symlinks for tests..."
            )
        endif()
    endif()
endif()

if(BUILD_PLUGIN)
    add_library(MyPlugin SHARED
        plugins/MyPlugin/plugin.cpp
    )

    add_library(SceneDiagnostics SHARED
        plugins/SceneDiagnostics/plugin.cpp
    )

    target_include_directories(MyPlugin PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    target_include_directories(SceneDiagnostics PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    if(WIN32)
        target_link_options(MyPlugin PRIVATE -Wl,--allow-shlib-undefined)
        target_link_options(SceneDiagnostics PRIVATE -Wl,--allow-shlib-undefined)
    endif()

    set_target_properties(MyPlugin PROPERTIES
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins
    )

    set_target_properties(SceneDiagnostics PROPERTIES
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins
    )

    if(UNIX)
        set_target_properties(MyPlugin PROPERTIES
            BUILD_RPATH "$ORIGIN/.."
            INSTALL_RPATH "$ORIGIN/.."
        )
        set_target_properties(SceneDiagnostics PROPERTIES
            PREFIX ""
            BUILD_RPATH "$ORIGIN/.."
            INSTALL_RPATH "$ORIGIN/.."
        )
    endif()
endif()
