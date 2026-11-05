 <div align="center">

# Pumpkin Engine

[![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/23)
[![CMake](https://img.shields.io/badge/CMake-3.30%2B-064F8C?style=for-the-badge&logo=cmake&logoColor=white)](https://cmake.org/)
[![SDL3](https://img.shields.io/badge/SDL3-Window%20%26%20Events-173B5E?style=for-the-badge)](https://www.libsdl.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-lightgrey?style=for-the-badge)]()
[![Version](https://img.shields.io/badge/Version-1.0-success?style=for-the-badge)]()


**A CPU-based 3D rendering engine built from scratch in modern C++.**

[Features](#features) • [Rendering Pipeline](#rendering-pipeline) • [Procedural Generation](#procedural-terrain-generation) • [Build](#building-the-project) • [Architecture](#project-structure)

</div>

---

## Overview

**Pumpkin Engine** is a 3D CPU rendering engine written in C++23.

The project focuses on implementing the fundamental systems of a modern rendering engine without relying on a traditional GPU rendering API.

It features a handmade CPU rasterizer, a SIMD-accelerated rendering pipeline, a multithreaded Entity Component System, a configurable particle system, procedural terrain generation and a YAML-driven UI module.

The engine is designed around modern C++ features, including C++ modules, multithreading and data-oriented programming techniques.

### Main Highlights

- Handmade CPU rasterizer using SIMD instructions and multithreading.
- Complete rendering pipeline, from vertex processing to fragment shading.
- Multithreaded Entity Component System and scheduler.
- Command buffers and component access policies.
- Multiple rendering passes.
- Procedural terrain, biome coloring, forests and water generation.
- Configurable particle system.
- Skybox rendering using panorama textures.
- YAML-based UI system with text, buttons, images and canvas definitions.
- Cross-platform support for Windows and Linux.

## Features

### CPU Renderer

A fully handmade CPU rasterizer responsible for transforming and drawing 3D geometry.

- SIMD-accelerated operations.
- Multithreaded rendering.
- Vertex processing.
- Homogeneous clipping.
- Perspective division and screen-space transformation.
- Primitive assembly.
- Triangle rasterization.
- Fragment processing.
- Depth handling and face culling.
- Configurable rendering passes.

### Rendering Pipeline

The engine implements the main stages of a traditional graphics pipeline:

```text
3D Model
   |
   v
Vertex Shader
   |
   v
Clipping
   |
   v
Screen-Space Transformation
   |
   v
Primitive Assembly
   |
   v
Rasterization
   |
   v
Fragment Shader
   |
   v
Framebuffer
```

Each stage is handled by the CPU renderer.

### Shaders

Pumpkin Engine provides several shading models:

| Shader | Description |
|---|---|
| Vertex Color | Basic vertex color shader |
| Lambert | Diffuse lighting |
| Blinn-Phong | Diffuse and specular lighting |
| PBR | Metallic, ambient and roughness parameters |
| Cel Shading | Toon-style shading |
| Water Shader | Water rendering used alongside procedural terrain |
| Albedo Color | Reports the optional albedo color stored on a 3D model |

### Rendering Passes

The renderer is organized around dedicated passes:

- Clear Pass
- Geometry Pass
- Particle Pass
- Skybox Pass
- Outline Pass
- Image Pass
- Text Pass
- Button Pass

Each pass handles a specific part of the rendering process.

### Entity Component System

The engine includes a multithreaded ECS architecture.

Its main systems include:

- Entity management.
- Component storage.
- Sparse sets and component pools.
- Registry and world management.
- Command buffers.
- Multithreaded scheduler.
- Thread pool.
- Component access policies.

The scheduler handles system execution and component access, while command buffers provide a mechanism for recording ECS operations.

### Particle System

A configurable particle system with support for:

- Custom particle emitters.
- Configurable velocity.
- Gravity.
- Particle lifetime.
- Color changes.
- Particle spawning and updating.
- Dedicated particle rendering pass.

### Skybox

Panorama texture-based skybox rendering, providing an environment background for 3D scenes.

---

## Procedural Terrain Generation

Pumpkin Engine includes a procedural terrain generation system based on deterministic 2D value noise and fractal noise.

The terrain is generated from a configurable heightmap, which is then converted into a triangle mesh.

### Terrain Features

- Seed-based deterministic generation.
- Configurable terrain dimensions and resolution.
- Fractal noise with multiple octaves.
- Adjustable noise frequency, persistence and lacunarity.
- Height-based biome coloring.
- Slope-based rock coloring.
- Procedural pine tree placement.
- Noise-based forest distribution.
- Terrain-aware tree placement.
- Water surface generation with shoreline clipping.
- Terrain chunk generation and bounding boxes.

### Noise Generation

The terrain generator combines multiple layers of noise to create different levels of detail.

Its noise implementation is composed of:

1. Integer hashing.
2. Deterministic pseudo-random values.
3. 2D value noise with smooth interpolation.
4. Fractal noise combining multiple octaves.

The main parameters are exposed through `TerrainConfig`:

| Parameter | Description |
|---|---|
| `seed` | Controls deterministic generation |
| `noiseScale` | Base noise frequency |
| `noiseOctaves` | Number of noise layers |
| `persistence` | Amplitude multiplier between octaves |
| `lacunarity` | Frequency multiplier between octaves |
| `minHeight` / `maxHeight` | Terrain height limits |
| `baseHeight` | Central terrain height |

### Biomes

Terrain triangles are colored according to their height and surface slope.

Supported terrain colors include:

- Sand
- Grass
- Rock
- Snow

Steep surfaces can be classified as rock independently of their altitude.

### Forest Generation

Pine trees are generated as low-poly meshes and distributed using seeded random positions.

A low-frequency noise mask defines forest regions, allowing trees to appear in groups rather than being uniformly scattered.

Tree placement also considers terrain height to prevent trees from spawning underwater.

### Water Generation

Water is generated at a configurable height.

Terrain triangles intersecting the water level are clipped to produce a shoreline that follows the terrain geometry.

### Terrain Chunks

The terrain can be split into smaller chunks.

Each chunk has its own mesh and axis-aligned bounding box (AABB).

The `TerrainBatcher` performs frustum culling and merges visible chunks into batches, reducing unnecessary geometry processing.

---

## UI Module

Pumpkin Engine includes a UI module driven by YAML configuration files.

### Features

- YAML file parsing and loading.
- Text rendering.
- Buttons.
- Image loading and rendering.
- Font loading.
- Canvas definitions.
- Element resizing when the window size changes.

This allows UI layouts to be described separately from the engine code.

---

## Technologies

| Technology | Usage |
|---|---|
| C++23 | Main programming language |
| C++ Modules | Code organization and modular compilation |
| CMake | Build system |
| SDL3 | Window and event management |
| SDL_ttf | Font management |
| SIMD / AVX2 | CPU acceleration |
| FMA | Hardware-accelerated floating-point operations |
| Multithreading | Parallel rendering and ECS execution |
| YAML | UI configuration |
| GoogleTest | Unit testing |

## Building the Project

### Requirements

- CMake 3.30 or newer.
- A C++23-compatible compiler with C++ modules support.
- Ninja (recommended for the provided CMake configurations).
- An AVX2 and FMA-compatible CPU.
- SDL3 and SDL3_ttf.

**Compiler note:** The project enables AVX2 and FMA compilation flags. The target CPU must support these instruction sets.

### Clone the Repository

```bash
git clone https://github.com/0ri0546/PumpkinEngine.git
cd PumpkinEngine
```

### Configure

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

### Build

```bash
cmake --build build --config Release
```

The engine library and runtime executable are generated in the build output directory:

```text
build/
└── bin/
    ├── PumpkinEngine
    └── PumpkinRuntime
```

On Windows, the corresponding executable and DLL files are generated in the same output directory.

### Build with Tests

Testing can be enabled through the `PUMPKIN_BUILD_TEST` CMake option:

```bash
cmake -S . -B build -G Ninja -DPUMPKIN_BUILD_TEST=ON
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

### Windows Release Builds

Precompiled Windows executables are available in the GitHub Releases section.

The release includes builds produced with different compiler toolchains. The Clang build was observed to produce a faster overall executable in the release testing.

[**Download the latest release**](https://github.com/0ri0546/PumpkinEngine/releases)

---

## Project Structure

```text
PumpkinEngine/
|
├── Assets/
│   ├── Copy/
│   └── Windows/
│
├── CMake/
│
├── PumpkinEngine/
│   ├── Libraries/
│   │
│   ├── Sources/
│   │   ├── Engine/
│   │   │   ├── Demo/
│   │   │   ├── ECS/
│   │   │   ├── Platform/
│   │   │   ├── Renderer/
│   │   │   │   ├── Passes/
│   │   │   │   ├── TerrainGenerator.cpp
│   │   │   │   ├── TerrainBatcher.cpp
│   │   │   │   ├── Rasterizer.cppm
│   │   │   │   ├── Shaders.cppm
│   │   │   │   └── ...
│   │   │   ├── UI/
│   │   │   │   ├── Canvas/
│   │   │   │   ├── Components/
│   │   │   │   ├── Systems/
│   │   │   │   └── YAML/
│   │   │   ├── Utils/
│   │   │   └── main.cpp
│   │   │
│   │   └── ThirdParty/
│   │
│   └── Tests/
│       ├── ECS/
│       ├── Math/
│       └── main.cpp
│
├── CODE_CONVENTIONS.md
├── CMakeLists.txt
├── CMakeSettings.json
└── Doxyfile
```

## Authors

<div align="center">

**Lucas Marcucci & Dylan Hollemaert**

*Designed and developed as a collaborative C++ engine project.*

</div>

---

<div align="center">

**Pumpkin Engine 1.0**

*Built from scratch. Powered by the CPU.*

[GitHub Repository](https://github.com/0ri0546/PumpkinEngine)

</div>
