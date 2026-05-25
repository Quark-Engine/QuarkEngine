# Quark Engine

A light-weight engine in C++ and QuarkCore.

## Features

### Editor & Workspace
*   **Quark Hub:** A dedicated project manager to create, rename, delete, and switch between multiple projects.
*   **Scene Hierarchy:** Manage entities in your scene with ease. Supporting renaming, duplication, and deletion.
*   **Inspector:** Detailed control over entity transforms (Position, Rotation, Scale), materials, and lighting properties.
*   **Asset Browser:** Real-time filesystem tracking for textures and models with drag-and-drop support to spawn entities directly into the 3D world.
*   **Undo/Redo System:** A reliable state-based system to revert or re-apply changes.
*   **Transform Gizmos:** Integrated **ImGuizmo** for intuitive 3D manipulation (Translate, Rotate, Scale).

### Graphics & Geometry
*   **Procedural Primitives:** Quickly generate Cubes, Spheres, Hemispheres, Cones, Cylinders, and Toruses with adjustable segments.
*   **Model Support:** Import external 3D assets including `.obj`, `.glb`, `.gltf`, `.fbx`, and `.iqm`.
*   **Mesh Editing:** Advanced "Triangle Sculpt" mode allowing for vertex-level manipulation and mesh triangle detachment.
*   **Material System:** Support for external textures, embedded model textures, albedo coloring, and customizable UV tiling/stretching.

### Lighting & Shading
*   **Real-time Lighting:** Shader-based point light system with configurable intensity, range, and color.
*   **Emission Shaders:** Support for emissive materials and ambient light configuration.
*   **Custom Shaders:** Integrated GLSL lighting shaders for modern 3D rendering.

## Tech Stack

*   **Core:** C++17
*   **Rendering:** QuarkCore
*   **UI:** Dear ImGui via qcImGui
*   **Gizmos:** ImGuizmo
*   **Serialization:** nlohmann/json

## Getting Started

### Prerequisites
*   A C++ compiler supporting C++17.

### Directory Structure
Ensure your project folder contains an `assets` directory with the following resources:
*   `Rubik-Regular.ttf` (Editor font)
*   `lighting.vs` / `lighting.fs` (Shaders)
*   `file.png`, `folder.png`, `full_folder.png` (Editor icons)

*   **projects:** Array of registered projects for the Quark Hub

This file is automatically created on first run and manages both application settings and project registry.

## Dependencies on Linux
```sh
sudo apt-get install -y ninja-build libgl1-mesa-dev libx11-dev libxrandr-dev libxi-dev libxinerama-dev libxcursor-dev libxxf86vm-dev
```

# Build

## Configure CMake
```sh
cmake -S . -B build
```

## Compile
```sh
cmake --build build
```

## Controls

| Key | Action |
|-----|--------|
| **W, A, S, D** | Move Camera |
| **Mouse Right Click (Hold)** | Rotate Camera |
| **P** | Translate Mode |
| **R** | Rotate Mode |
| **S** | Scale Mode |
| **Ctrl + S** | Save Project |
| **Ctrl + Z** | Undo |
| **Ctrl + Y** | Redo |
| **Ctrl + D** | Duplicate Selected |
| **Del** | Delete Selected |
| **Esc** | Release Camera Focus |

## Project Structure

*   `src/hub.cpp`: Project management and launcher.
*   `src/editor/editor.cpp`: Main UI, Inspector, and Asset Browser logic.
*   `src/project.cpp`: Scene serialization (JSON).
*   `src/lighting.cpp`: Light allocation and shader updates.
*   `src/models.cpp`: Model loading and mesh manipulation.

## License

This project is licensed under the MIT License - see the source headers for details. 