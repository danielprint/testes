# STL Viewer

A cross-platform Qt 6 desktop application for exploring STL meshes with an interactive OpenGL viewport.

## Features

- Load ASCII and binary STL files via file dialog or drag & drop.
- Optional Assimp integration (`-DUSE_ASSIMP=ON`) with robust fallback STL parser.
- Orbit/pan/zoom camera with optional fly mode (WASD + QE, toggle with **F**).
- Grid and axis gizmos, bounding box visualization, and detailed model metrics (bounds, triangle count, normals source).
- Phong shaded, wireframe, or hybrid rendering with gamma correction and adjustable key light (RMB drag).
- Backface culling toggle, per-face normal visualization, and optional vertex normal recomputation.
- Per-model translate/rotate/scale controls with reset; units displayed in millimeters.
- Screenshot capture to PNG, recent file history (last five), and persistent UI/settings between sessions.

## Controls

- **LMB drag** – Orbit camera around focus point.
- **MMB drag** – Pan camera.
- **Mouse wheel** – Zoom (dolly).
- **RMB drag** – Adjust key light direction.
- **F** – Toggle fly mode; use **WASD** to move, **Q/E** to descend/ascend (hold **Shift** to accelerate).
- **Ctrl+O** – Open STL file.

## Building

### Prerequisites

- CMake ≥ 3.16
- C++20 compiler (MSVC 2019+, GCC 11+, or Clang 12+)
- Qt 6.4+ (Widgets, OpenGL components)
- Optional: Assimp (for extended format support)

### Configure & Build

```bash
cmake -B build -S . -DUSE_ASSIMP=ON
cmake --build build
```

The executable `STLViewer` will be located in `build/` (platform-specific subfolder).

### Windows (MSVC)

```powershell
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 -DUSE_ASSIMP=ON
cmake --build build --config Release
```

### Linux (GCC or Clang)

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DUSE_ASSIMP=ON
cmake --build build
```

### macOS (Clang / Xcode)

```bash
cmake -B build -S . -G Xcode -DUSE_ASSIMP=ON
cmake --build build --config Release
```

If Assimp is unavailable, the build automatically falls back to the internal STL parser (or specify `-DUSE_ASSIMP=OFF`).

## Sample Assets

Two small STL samples are included in the `samples/` folder (`pyramid_ascii.stl`, `tetra_binary.stl`) alongside an example screenshot (`resources/screenshot.png`).

## Troubleshooting

- Ensure Qt’s CMake configuration is discoverable (set `CMAKE_PREFIX_PATH` to your Qt installation if necessary).
- When running on systems without OpenGL 4.1 core profile, adjust the requested version in `main.cpp`.
- If STL files fail to load, check the message dialog for details; malformed files are handled gracefully.
