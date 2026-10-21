# Graphene

Graphene is a real-time engine and editor written in Encore, with Vulkan rendering.

## Build and run

Use Encore 0.1.6 or newer. The current manifest expects sibling `Vulkan`, `Luma`,
`SDL`, `GPU` and `Audio` repositories next to Graphene. Restore the versions in
`encore.lock`; building requires a C/C++ toolchain and the native dependencies
of those sibling libraries.

```sh
encore sync
encore build
target/dev/graphene open examples/spheres
```

The only example contains 1,000 untextured built-in spheres in a 10 × 10 × 10
grid. All instances share engine geometry and a constant gray material. The
project contains no model or texture files and requires no asset download.
Its game entry point is intentionally empty; inspect the scene in the editor
or use Play in Editor.

## Source layout

- `src/`: Encore engine and editor code.
- `native/`: native bridges, importers, Blueprint runtime, automation, profiler,
  embedded shader tables and third-party sources with their licenses.
- `shaders/`: authoritative GLSL and SMAA reference data. Generated native tables
  are committed so normal builds do not require a shader compiler.
- `Assets/`: built-in source content; project content belongs in `Content/`.
- `examples/spheres/`: the single small editor scene.
- `tests/`: engine tests, small fixtures and focused runners in `tests/support/`.
- `tools/shaders/`: shader compilation, validation and table generation.
- `tools/assets/`: asset/schema generators.
- `tools/package/`: optional audio and reconstruction runtime bundling.
- `tools/graphene-mcp/`: MCP adapter for the editor automation API.

Shader regeneration requires `glslc` and `spirv-val`. Run the corresponding
script, for example `sh tools/shaders/generate_cube_shaders.sh` or
`python3 tools/shaders/generate_aa_shaders.py`. Normal editor execution uses the
embedded tables. Native model import accepts OBJ, FBX, glTF and GLB through
`graphene import <project> <source> [name]`; `graphene cook <project>` prepares
native project assets.

## Optional rendering and audio runtimes

Edit → Project Settings → Rendering selects AA, upscaling quality, sharpening
and frame generation. Settings are saved in `Config/Rendering.json`. FSR 3.1,
DLSS and DLAA use an optional Linux/Vulkan SDK bridge; missing support falls
back to supported AA. Frame generation runs in Play/standalone, while editor
navigation uses real frames. NVIDIA 3×/4× require hardware/SDK support.

```sh
python3 ../Vulkan/tools/build_reconstruction.py \
  --sdk-dir target/reconstruction/sdk --output target/reconstruction/runtime
python3 tools/package/bundle_reconstruction.py target/dev/graphene
python3 tools/package/bundle_audio.py target/dev/graphene
```

The reconstruction bundle pins FidelityFX SDK v1.1.4 and NVIDIA SDK v310.9.16,
including their runtime licenses. HRTF packaging uses the pinned Steam Audio
library from the sibling Audio repository. SDK binaries belong beside built
executables and are excluded from Git. Runtime replay does not add gameplay,
animation, audio or particle ticks. The performance panel separates real
renders, generated frames and submitted frames; submissions are not measured
display refreshes. Current presentation is SDR; AMD hardware and Khronos
validation-layer coverage remain separate verification work.

## Development checks

```sh
encore check
encore lint
encore test
python3 tests/check_git_payload.py
python3 tests/check_reconstruction_bundle.py
python3 tests/support/check_reconstruction_settings.py --compiler /path/to/encore
python3 tests/support/check_reconstruction_view.py --compiler /path/to/encore
```

Focused animation runners are in `tests/support/animation/`; they stage the
production modules under ignored `target/` directories. Fixtures required by
engine tests live in `tests/fixtures/`, independently of the example scene.

Before staging, run `python3 tools/check_git_payload.py --worktree`. After
staging, run `python3 tools/check_git_payload.py` and review `git diff --cached`.
The guard checks actual staged blobs and rejects force-added ignored files.
Defaults are 10 MiB per file and 50 MiB total. Build outputs, caches and optional
large grass/rock source packs are excluded by `.gitignore`; their inventory is
`Assets/external-assets.json`. Existing local demo projects were archived under
ignored `target/` during repository cleanup. No large art pack is needed for
the spheres example.
