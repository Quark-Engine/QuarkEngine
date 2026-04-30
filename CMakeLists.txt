cmake_minimum_required(VERSION 3.10)
project(QuarkEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(
    ${CMAKE_SOURCE_DIR}/quark-libs/imgui/include
    ${CMAKE_SOURCE_DIR}/quark-libs/raylib/include
    ${CMAKE_SOURCE_DIR}/src/headers
)

file(GLOB RAYLIB_SRC
    ${CMAKE_SOURCE_DIR}/quark-libs/raylib/*.c
)

add_library(raylib STATIC ${RAYLIB_SRC})

target_include_directories(raylib PUBLIC
    ${CMAKE_SOURCE_DIR}/quark-libs/raylib
)

target_compile_definitions(raylib PRIVATE PLATFORM_DESKTOP)

set(EDITOR_FILES
    src/editor/editor_assets.cpp
    src/editor/editor_entity.cpp
    src/editor/editor_ui.cpp
    src/editor/editor_utils.cpp
    src/editor/editor_viewers.cpp
)

set(SOURCE_FILES
    src/camera.cpp
    src/editor.cpp
    src/hub.cpp
    src/ImGuizmo.cpp
    src/lighting.cpp
    src/main.cpp
    src/models.cpp
    src/project.cpp
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

set(RAYLIB_LIB_DIR "${CMAKE_SOURCE_DIR}/quark-libs/raylib/lib")

set(ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets")
if(EXISTS "${ASSETS_DIR}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${ASSETS_DIR}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
        COMMENT "Copying assets to output directory"
    )
else()
    message(WARNING "Assets directory not found: ${ASSETS_DIR}. Skipping asset copy step.")
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME}
        raylib
        opengl32
        winmm
    )

elseif(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME}
        raylib
        GL
        m
    )

elseif(APPLE)
    target_link_libraries(${PROJECT_NAME}
        raylib
        "-framework Cocoa"
        "-framework OpenGL"
        "-framework IOKit"
        "-framework CoreAudio"
        "-framework CoreVideo"
    )
endif()
