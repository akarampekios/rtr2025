# Cyberpunk City Demo

A real-time rendering demo showcasing ray tracing and volumetric lighting effects in a cyberpunk city environment.

## Features

- **Ray-traced reflections** using Vulkan RTX
- **Volumetric lighting** with light shafts and fog
- **PBR materials** with emissive neon lighting
- **Real-time vehicle movement** on predefined paths
- **Post-processing effects** including bloom and tone mapping

## Requirements

- Windows 10/11 with latest updates
- Visual Studio 2022 (Desktop development with C++ workload)
- Vulkan SDK 1.3+ (glslc must be on PATH)
- NVIDIA RTX 2070 (or better) with updated drivers
- CMake 3.20+

## Quick Start

```powershell
git clone <repository-url>
cd CyberpunkCityDemo
git submodule update --init --recursive
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The compiled binary lives at `build/bin/Release/CyberpunkCityDemo.exe`. Assets and shaders are copied automatically when you build.

### Running

Launch from Explorer or the terminal:

```powershell
build\bin\Release\CyberpunkCityDemo.exe
```

You should see debug output in the console while the renderer runs. Press `Esc` to exit.

### Controls

- `WASD` move
- `Mouse` look (cursor locks on click)
- `Space` toggles the debug UI (placeholder)
- `Esc` quits

## Project Layout

```
src/
├── main.cpp          # Entry point
├── Application.*     # Window + main loop
├── SimpleRenderer.*  # Vulkan ray-tracing renderer
├── Camera.*          # Fly camera logic
├── Scene.*           # Scene setup + animation
├── GLTFLoader.*      # Stub loader / procedural geometry
└── ShaderManager.*   # SPIR-V utilities

assets/               # City meshes (GLB) used at runtime
shaders/              # GLSL/HLSL ray tracing shaders + SPIR-V
external/             # Vendor dependencies (GLFW, glm, stb, imgui)
build/                # Generated CMake/VS artifacts
```

## Tips for Colleagues

- Always build the `Release` configuration; `Debug` lacks ray-tracing paths.
- If shaders fail to compile, ensure the Vulkan SDK `glslc.exe` is first in your PATH.
- Running from Visual Studio: set `build/bin/Release/CyberpunkCityDemo.exe` as the startup program.
- Large assets live in `assets/`; keep the `.glb` files alongside the executable when redistributing.

