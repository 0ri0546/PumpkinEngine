# Code conventions

These conventions describe the style used across the Pumpkin engine (C++ modules, C++23).
When in doubt, follow this document.

---

## 1. Naming

| Element                           | Style                             | Example                                      |
|-----------------------------------|-----------------------------------|----------------------------------------------|
| Namespace                         | `PascalCase`                      | `Pumpkin`, `Pumpkin::UI::YAML`               |
| Module                            | `Pumpkin.PascalCase.PascalCase`   | `Pumpkin.Renderer.PrimitiveAssembly`         |
| Class / struct / enum / alias     | `PascalCase`                      | `PrimitiveBatch`, `AssemblyDesc`, `CullMode` |
| Concept / template parameter      | `PascalCase`                      | `template <typename T, std::size_t N>`       |
| Function / method                 | `PascalCase`                      | `AssemblePrimitives`, `Size`, `SetPosition`  |
| Enum value                        | `PascalCase`                      | `CullMode::Back`, `ShaderId::Toon`           |
| Local variable                    | `camelCase`                       | `triCount`, `deltaTime`, `targetFps`         |
| Function parameter                | `camelCase`                       | `desc`, `indices`, `colors`                  |
| Private / protected member        | `m_camelCase`                     | `m_chunks`, `m_isRunning`                    |
| Public data member (plain struct) | `camelCase`, no prefix            | `Primitive::zKey`, `Camera::aspectRatio`     |
| Static member                     | `s_camelCase`                     | `s_instanceCount`                            |
| Global variable                   | `g_camelCase` (avoid globals)     | `g_pool`                                     |
| Constant (`constexpr`/`const`)    | `kPascalCase`                     | `kSetupChunk`, `kDefaultTargetFps`           |
| Macro                             | `UPPER_SNAKE_CASE` (avoid macros) | `PUMPKIN_ASSERT`                             |
| File                              | `PascalCase` + extension          | `PrimitiveAssembly.cppm`                     |

### Details

- **Classes vs. structs.** A `struct` is a plain bundle of data with public fields and no invariants. Its fields use
  plain `camelCase` (see `Primitive`, `AssemblyDesc`, `TerrainConfig`, `Material`). A `class` has private state, and
  every private/protected data member gets the `m_` prefix.
- **Math and short names.** In tight numeric code, short names are fine when they follow a clear pattern:
  `ax, ay, az, ak` / `bx, by, bz, bk` (vertex components), `ia, ib, ic` (indices), `pa, pb, pc` (vertices),
  `w0, w1, w2` (weights), `u, v` (texture coordinates), `i` (loop index), `n` (counter). Outside of math, names must
  be descriptive.
- **Abbreviations.** Allowed only when universally understood or used consistently across the project
  (`fb` = frame buffer, `db` = depth buffer, `ctx` = context, `desc` = descriptor, `dst` = destination, `fps`, `uv`).
  Otherwise spell the word out.
- **Acronyms in identifiers** follow the casing of the surrounding style: `LoadUI`, `UIRenderSystem`, `ShaderId`,
  `SIMD_traits` is the only exception kept for the SIMD trait headers.
- **Booleans** read as a statement: `isRunning`, `transparent`, `valid`. Prefer `is/has/can/should` for new names.
- **Type suffixes for math types**: `Mat4f`, `Quaternionf`, `Vec3f`: base name + dimension + scalar letter
  (`f` = float, `d` = double, `i` = int).
- **Out-parameters** are named `out` (or `out<Thing>` when several) and placed last in the parameter list.
- **Setters and builders** are `Set<Property>`; fluent setters return `*this` by reference.
- **Factories** are `Make<Thing>` (`MakeTerrainConfig`, `MakeSnowParticles`); loaders are `Load<Thing>`;
  generators are `Generate<Thing>`.

---

## 2. Files and modules

- C++20 **modules** are the default. Use `import` instead of `#include`.
- One module per file. The module name mirrors the path and the file name:
  `Pumpkin.Renderer.PrimitiveAssembly` -> `PrimitiveAssembly.cppm` + `PrimitiveAssembly.cpp`.
- **Interface unit** (`.cppm`): `export module X;`, imports, then `export namespace Pumpkin { ... }`.
- **Implementation unit** (`.cpp`): `module X;` (no `export`), imports, then `namespace Pumpkin { ... }`.
- Declarations live in the interface; definitions live in the implementation, except for small inline/templated code
  (such as `PrimitiveBatch::operator[]`).
- Third-party headers that cannot be imported (SDL) are included with `#include` at the very top of the file,
  before any `import`, through the project wrapper (`SDL_FixedIncludes.h`).
- `using namespace Pumpkin;` is allowed in `main.cpp` only. Never in a module interface or a header.
- No `#pragma once` / include guards are needed in modules. If a legacy header is still required, use `#pragma once`.

### Import order

Separate groups with a blank line, in this order:

1. Third-party / needed macros `#include`s (top of the file)
2. Project modules (`Pumpkin.*`), grouped by subsystem (Renderer, Utils, UI, Math), alphabetical inside a group
   when practical
3. `import std;` last

```cpp
module;

#include <DLL/Export.hpp>

module Pumpkin.Renderer.PrimitiveAssembly;

import Pumpkin.Renderer.Types;
import Pumpkin.Utils.ThreadPool;

import std;
```

---

## 3. Formatting

### Layout

- **Indentation:** 4 spaces. No tabs.
- **Namespaces are indented** one level, like any other block.
- **Braces:** opening brace on the same line (K&R / Attach style); `else` goes on the same line as the closing
  brace (`} else {`).
- **Line length:** aim for 100 columns. Wrap long signatures and expressions rather than going beyond ~110.
- **Continuation lines** are indented one level (4 spaces) beyond the statement, or aligned with the first argument
  for function signatures. Keep the style consistent inside a file.
- **One statement per line.** The exception is a group of closely related values of the same type, which may share
  one declaration or one line (`const float ax = ..., ay = ..., az = ...;`).
- **Blank lines** separate logical steps inside a function. Do not use more than one blank line in a row.
- **Indentation must be consistent.** Misindented blocks (as in the terrain material block of `main.cpp`) are bugs
  in style; fix them when you see them.
- **Trailing whitespace** is not allowed. Files end with a single newline.

### Spacing

- A space after control keywords: `if (`, `for (`, `while (`, `switch (`.
- No space between a function name and its parenthesis: `Size()`, `AssemblePrimitives(...)`.
- Spaces around binary operators and `=`; none around `::`, `.`, `->`, unary operators.
- Brace-initializers have inner spaces when they hold values: `{ 0.0f, 7.0f, 0.0f }`, `{ 255, 255, 255, 255 }`.
  Empty initializers have none: `{}`.
- Pointer and reference symbols **attach to the type**: `const Texture& texture`, `const char* path`,
  `SDL_Window* window`, `std::vector<Primitive>& dst`.
- Templates: `std::span<const Vertex>`, `std::vector<std::vector<Primitive>>`.
  `template <typename T>` has a space after `template`.

### Control flow

- Braces are always used when the body spans more than one line.
- A single-statement early exit may stay on one line: `if (area == 0.0f) return;`.
  Keep these for guard clauses only (`return`, `continue`, `break`). Nothing else.
- Prefer **early returns / guard clauses** over deep nesting.
- `switch` cases are indented one level; always include a `default:` branch.
- Range-`for` is preferred when the index is not needed: `for (const auto& c : chunks)`.

### Literals

- Floats always carry the `f` suffix: `0.0f`, `1.0f`, `0.5f`. Never write `0.` or `.5f`.
- Digit separators for large numbers: `9'999'999`, `1'000'000'000`.
- Use `std::numbers::pi_v<float>` instead of hand-typed constants.
- Use designated initializers for configuration structs with many fields:

```cpp
return TerrainConfig{
    .cellsX = 128,
    .cellsZ = 128,
    .seed = 727
};
```

- Use `nullptr`, never `NULL` or `0` for pointers.

### Fluent / chained calls

Builder-style chains put the dot at the end of the line and one call per line, indented one level:

```cpp
particles.
    SetPosition(0.0f, 3.0f, 0.0f).
    SetMaxParticles(2000).
    SetSpawnRadius(20);
```

---

## 4. Language usage

### Types

- Use fixed-width integers from `<cstdint>` (`std::uint32_t`, `std::uint16_t`, `std::uint8_t`, `std::uint64_t`)
  and `std::size_t` for sizes, counts and indices.
- Always qualify standard types with `std::`. Do not `using namespace std;`.
- Choose the smallest sensible type for stored data (screen size as `std::uint16_t`) but use `int`/`std::size_t`
  for arithmetic and loop counters.
- Use `float` for engine math unless precision demands `double`.

### Const and constexpr

- **`const` by default**: locals, parameters and methods that do not mutate state.
- **`constexpr`** for compile-time values; **`inline constexpr`** for constants defined in an interface.
- Constants are named `kPascalCase`. File-local constants are `static constexpr` (or live in an anonymous namespace).
- Pass non-trivial read-only parameters by `const&`; pass small trivial types (`int`, `float`, enums, `std::span`)
  by value.
- `static_assert` documents and enforces assumptions between related constants:

```cpp
inline constexpr std::size_t kSetupChunk = 512;
inline constexpr std::uint32_t kChunkShift = 9;
static_assert(kSetupChunk == (1u << kChunkShift));
```

### Attributes

- Mark getters and pure functions that return a value `[[nodiscard]]`.
- Mark intentional fallthrough `[[fallthrough]]`.

### Casting

- Only C++ casts: `static_cast`, `reinterpret_cast` (rare, commented), `const_cast` (avoid).
  Never C-style casts.
- Make every narrowing or int/float conversion explicit with `static_cast`.

### `auto`

- Use `auto` when the type is obvious from the right-hand side or is unnameable (lambdas, iterators, `make_unique`,
  `make_shared`, `chrono` expressions).
- Spell out the type for fundamental types and when it carries meaning (`float deltaTime`, `std::uint32_t frames`).
- Write `const auto&` (never `auto&` when the value is not modified).

### Ownership and memory

- Owning pointers are `std::unique_ptr` / `std::shared_ptr`, created with `std::make_unique` / `std::make_shared`.
  No naked `new` / `delete`.
- Raw pointers and references are **non-owning** (`const Texture* albedo`, `Mesh* mesh`).
- Use `std::span` for non-owning views of contiguous data instead of pointer + size.
- Use `std::array` for fixed-size data and `std::vector` for dynamic data; avoid C arrays unless storing small POD
  fields inside a hot struct (`float kv[3]`).
- Wrap C resources (SDL windows, surfaces, TTF) so that they are released in a well-defined scope. Destroy them
  before the library shuts down.
- Hot paths must not allocate. Reuse buffers: `clear()` keeps the capacity, `resize()` only on size changes.

### Default member initializers

Plain structs give every field a sensible default when a zero-initialized value is not the right one:

```cpp
struct AssemblyDesc {
    CullMode culling = CullMode::Back;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
};
```

For POD structs filled right after creation, value-initialize at the call site: `Primitive p{};`.

### Functions

- Keep functions short and single-purpose. Split a function when it needs section comments to be understood.
- Helpers private to a translation unit are `static` or placed in an anonymous namespace; never exported.
- Use lambdas for small local helpers (`weightPlane`) and capture explicitly when the scope is large, `[&]` is
  acceptable for short ones.
- Prefer returning values. Use a non-const reference out-parameter (`PrimitiveBatch& out`) when reusing existing
  storage is the point of the function.
- Validate inputs at the top of public functions and leave outputs in a defined state when the input is invalid
  (`out.chunks.clear()`), instead of asserting.

### Standard library

- Console output: `std::println` / `std::println(stderr, ...)`; formatting: `std::format`.
  No `printf`. Avoid `std::cout` in new code.
- Use `std::min` / `std::max` (including the initializer-list overload) and standard algorithms over hand-written loops
  when they read better.
- Use `std::chrono` for time, with `std::chrono::steady_clock` for measurements.
- Threading goes through the project's thread pool (`ParallelFor(GetPool(), ...)`), not raw `std::thread`, except for
  sleeping in the main loop.

### Error handling

- Recoverable runtime failures (missing asset, failed SDL call): report with `std::println(stderr, ...)` and clean up
  or return an error value. The game must not crash on a missing texture.
- Log and error messages are written in English, start with a capital letter and do not end with a period when short.

---

## 5. Comments and documentation

- Comments and identifiers are written in **English**.
- Public declarations in an interface unit are documented with Doxygen triple-slash comments and `@brief`:

```cpp
/// @brief Stage 2: builds the primitives from screen-space positions and indices.
///
/// Culls (back faces, flat or offscreen triangles) and sets up the edge, depth and 1/w planes.
/// If the input is invalid, the batch is left empty.
void AssemblePrimitives(...);
```

- Document: what the function does, what the parameters mean when it is not obvious, the preconditions, and what
  happens on invalid input. Do not restate the signature.
- Inline comments use `//`, are full sentences starting with a capital letter, and explain **why** (or the math
  behind it), not what the code literally does.
- Trailing comments on struct fields are aligned in a column and describe the meaning and units:

```cpp
int x0, y0, x1, y1;        // pixel bounding box, clamped to the screen (x1 and y1 excluded)
Plane z;                   // depth
```

- Mention conventions that are easy to get wrong (inclusive/exclusive bounds, coordinate spaces, sign of areas,
  units).
- Do not leave commented-out code in committed files. Use a short `// TODO(name): ...` for planned work.
- No decorative banners or ASCII art.

---

## 6. Project structure

- Rendering code is organized as a pipeline of stages (see `PrimitiveAssembly` = stage 2) and **render passes**
  (`ClearPass`, `SkyboxPass`, `GeometryPass`, ...). Each pass lives in its own module `Pumpkin.Renderer.Passes.<Name>`.
- Module names follow the folder layout: `Pumpkin.<Area>.<Subarea>.<Name>`
  (`Renderer`, `Utils`, `UI`, `Math`, `ECS`, ...).
- Shared plain data types (vertices, colors, planes, materials) go in a `Types` module so other modules do not depend
  on each other for simple structs.
- `main.cpp` stays thin: set up SDL, build the scene, run the loop. It contains no engine logic. File-local helper
  functions go in an anonymous namespace at the top (`LoadTextureOrWarn`, `MakeSnowParticles`).
- Assets are referenced relative to the working directory under `Assets/` (`Assets/Models`, `Assets/Textures`,
  `Assets/UI`).

---

## 7. Performance-sensitive code

The renderer is a CPU software rasterizer, so performance rules apply to the hot paths:

- Prefer flat, contiguous data (`std::vector`, `std::span`) over node-based containers.
- Hoist invariants out of loops, compute reciprocals once (`invArea`) and multiply instead of dividing.
- Do per-pixel/per-triangle setup once and store the result (planes, bounding boxes) instead of recomputing.
- Clamp in float **before** casting to integers to avoid overflow and undefined behavior.
- Split work in chunks sized by a named constant (`kSetupChunk`) and process chunks in parallel. Each task writes
  to its own storage so no locking is needed.
- Keep SIMD code behind the traits module (`Pumpkin.Utils.SIMDTraits`). Document
  the minimum instruction set (SSE2 / AVX2) in the module comment.
- Measure before optimizing: use the render profiler (`Prof::EndFrame`).
  Optimization commits state the numbers.

---

## 8. Tooling and tests

- Compiler: **MSVC**, `/std:c++latest` for Windows and **LLVM**, `-std=c++23` for Linux.
- Tests use **Google Test**. Test files are named `<Module>.Tests.cpp`, test suites are `PascalCase`,
  test names describe the behavior (`Normalize_ZeroVector_ReturnsZero`).
- Benchmarks print aligned columns (op/s, ns/op, err%, cyc/op, total).

---

## 9. Quick checklist before committing

- [ ] Names follow section 1 (`m_` members, `PascalCase` types/functions, `camelCase` locals, `kPascalCase` constants).
- [ ] `import std;` is last in the import list; no stray `#include` other than third-party headers.
- [ ] 4-space indentation, no trailing whitespace, no misindented blocks.
- [ ] Float literals have `f`; narrowing conversions use `static_cast`.
- [ ] Public API is documented with `///` and `@brief`.
- [ ] No commented-out code, no `using namespace` outside `main.cpp`.
- [ ] No allocations or needless copies in hot paths.
