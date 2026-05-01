cmake_minimum_required(VERSION 3.10)
project(QuarkEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_ENGINE "Build Quark Engine" ON)
option(BUILD_PLUGIN "Build Plugin" OFF)

include_directories(
    ${CMAKE_SOURCE_DIR}/quark-libs/imgui/include
    ${CMAKE_SOURCE_DIR}/src/headers
    ${CMAKE_SOURCE_DIR}/src/plugins
)

if(BUILD_ENGINE)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

    add_subdirectory(quark-libs/raylib)

    set(EDITOR_FILES
        src/editor/editor_assets.cpp
        src/editor/editor_entity.cpp
        src/editor/editor_ui.cpp
        src/editor/editor_utils.cpp
        src/editor/editor_viewers.cpp
    )

    set(SOURCE_FILES
        src/camera.cpp
        src/editor/editor.cpp
        src/hub.cpp
        src/ImGuizmo.cpp
        src/lighting.cpp
        src/main.cpp
        src/models.cpp
        src/project.cpp
        src/plugins/plugin_manager.cpp
        src/rlights.cpp
        src/scene.cpp
        src/tex.cpp

        quark-libs/imgui/include/imgui.cpp
        quark-libs/imgui/include/imgui_draw.cpp
        quark-libs/imgui/include/imgui_widgets.cpp
        quark-libs/imgui/include/imgui_tables.cpp
        quark-libs/imgui/include/rlImGui.cpp
    )

    add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${EDITOR_FILES})
    target_link_libraries(${PROJECT_NAME} raylib)

    if(WIN32)
        target_link_libraries(${PROJECT_NAME} winmm gdi32 opengl32)
    elseif(UNIX AND NOT APPLE)
        target_link_libraries(${PROJECT_NAME} GL m pthread dl rt X11)
    elseif(APPLE)
        target_link_libraries(${PROJECT_NAME}
            "-framework Cocoa"
            "-framework OpenGL"
            "-framework IOKit"
            "-framework CoreAudio"
            "-framework CoreVideo"
        )
    endif()
endif()

if(BUILD_PLUGIN)
    add_library(MyPlugin SHARED
        plugins/MyPlugin/plugin.cpp

        quark-libs/imgui/include/imgui.cpp
        quark-libs/imgui/include/imgui_draw.cpp
        quark-libs/imgui/include/imgui_widgets.cpp
        quark-libs/imgui/include/imgui_tables.cpp
    )

    target_include_directories(MyPlugin PRIVATE
        ${CMAKE_SOURCE_DIR}/quark-libs/imgui/include
        ${CMAKE_SOURCE_DIR}/src/headers
        ${CMAKE_SOURCE_DIR}/src/plugins
    )

    set_target_properties(MyPlugin PROPERTIES
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    )
endif()