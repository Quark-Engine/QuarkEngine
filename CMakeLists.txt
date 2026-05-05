cmake_minimum_required(VERSION 3.10)
project(QuarkEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_ENGINE "Build Quark Engine" ON)
option(BUILD_PLUGIN "Build Plugin" OFF)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS    ON CACHE BOOL "" FORCE)
set(GLFW_BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)

add_subdirectory(quark-libs/raylib)

include_directories(
    ${CMAKE_SOURCE_DIR}/quark-libs/imgui/include
    ${CMAKE_SOURCE_DIR}/src/headers
    ${CMAKE_SOURCE_DIR}/src/plugins
    ${CMAKE_SOURCE_DIR}/src
)

add_library(imgui SHARED
    quark-libs/imgui/include/imgui.cpp
    quark-libs/imgui/include/imgui_draw.cpp
    quark-libs/imgui/include/imgui_widgets.cpp
    quark-libs/imgui/include/imgui_tables.cpp
    quark-libs/imgui/include/rlImGui.cpp
    src/ImGuizmo.cpp
)

target_include_directories(imgui PUBLIC
    ${CMAKE_SOURCE_DIR}/quark-libs/imgui/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/headers
)

target_compile_definitions(imgui
    PRIVATE BUILD_LIBTYPE_SHARED
    PRIVATE IMGUI_API=__declspec\(dllexport\)
)

target_link_libraries(imgui PRIVATE raylib)

set_target_properties(imgui PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

if(BUILD_ENGINE)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

    set(EDITOR_FILES
        src/editor/editor_assets.cpp
        src/editor/editor_components_ui.cpp
        src/editor/editor_entity.cpp
        src/editor/editor_ui.cpp
        src/editor/editor_utils.cpp
        src/editor/editor_viewers.cpp
        src/editor/editor.cpp
        src/editor/editor_hierarchy_utils.cpp
        src/editor/editor_file_utils.cpp
    )

    set(SOURCE_FILES
        src/camera.cpp
        src/component.cpp
        src/entity.cpp
        src/hub.cpp
        src/language_manager.cpp
        src/lighting.cpp
        src/main.cpp
        src/models.cpp
        src/project.cpp
        src/plugins/plugin_manager.cpp
        src/rlights.cpp
        src/scene.cpp
        src/tex.cpp
    )

    add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${EDITOR_FILES})

    target_compile_definitions(${PROJECT_NAME}
        PRIVATE USE_LIBTYPE_SHARED
        PRIVATE IMGUI_API=__declspec\(dllimport\)
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE raylib imgui)

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

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            ${CMAKE_BINARY_DIR}/bin/assets
    )

    if(WIN32)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:raylib>
                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        )
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:imgui>
                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        )
    endif()
endif()

if(BUILD_PLUGIN)
    add_library(MyPlugin SHARED
        plugins/MyPlugin/plugin.cpp
    )

    target_include_directories(MyPlugin PRIVATE
        ${CMAKE_SOURCE_DIR}/src/plugins
    )

    if(WIN32)
        target_link_options(MyPlugin PRIVATE -Wl,--allow-shlib-undefined)
    endif()

    set_target_properties(MyPlugin PROPERTIES
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/plugins
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/plugins
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/plugins
    )
endif()