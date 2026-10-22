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
- `styles/`: application CSS modules; `graphene.css` imports them in cascade order.
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

Shader regeneration requires `glslc` and `spirv-val`. Run the corresponding
script, for example `sh tools/shaders/generate_cube_shaders.sh` or
`python3 tools/shaders/generate_aa_shaders.py`. Normal editor execution uses the
embedded tables. Native model import accepts OBJ, FBX, glTF and GLB through
`graphene import <project> <source> [name]`; `graphene cook <project>` prepares
native project assets.

Editor styling is split by component under `styles/`. Keep the imports in
`graphene.css` ordered: late layout and component overrides follow their base
rules. Luma resolves nested `@import` paths relative to each importing file;
stylesheets must be shipped together with the root CSS file.

## MCP over HTTP

Open **Edit → Editor Settings → MCP** and enable the server. The default endpoint
is `http://127.0.0.1:8765/mcp`; change the port and select **Apply port** when
running another editor instance. The server is disabled initially and remembers
its enabled state and port in `~/.graphene/editor-settings.json`.

Connect a Streamable HTTP MCP client to that URL with the header
`Authorization: Bearer <token>`. **Copy access token** copies the persistent local
token; its file is `~/.graphene/mcp-token`, readable only by its owner. Tokens
are not stored in projects, returned by inspection tools, or printed in logs.
There is no stdio executable or separate REST API.

Tools cover scene inspection and editing, components, Undo/Redo, camera control,
Content Browser operations, asset import, rendering settings, AutoLOD groups,
UI inspection and input, PNG screenshots, logs and CPU/GPU profiling. Blueprint,
material and animation graph documents can be inspected; graph authoring and
arbitrary code execution are excluded. Authenticated MCP operations do not open
additional editor approval dialogs.

- `list_actors`, `get_ui`, `list_assets` and `get_logs` accept `offset`, `limit`
  (up to 200), and `query`. Follow `nextOffset`; `graphene://scene` is the first
  actor page, not an unbounded dump. Scene rotations use radians.
- `click_widget` resolves visible, enabled widget IDs to hit-tested coordinates;
  `send_input` routes ordered events through the same editor event loop as local
  input. Coordinates use logical pixels. Input replies wait for processing and
  a presented frame. `capture_frame` returns the composed window or viewport as
  an MCP PNG image, with frame number, dimensions and pixel density.
- `edit_scene_batch` takes scene command envelopes (`method` and `params`),
  applies up to 128 edits as one Undo transaction, and rolls back on failure.
- Save, build and import return `jobId`. Poll `get_job_status` for completion;
  acceptance does not mean the disk operation has finished. Import `kind` can
  explicitly select `character`; `auto` chooses model, texture or sound by type.
  The Content Browser index refreshes asynchronously; poll `list_assets` before
  using a newly created, renamed, moved or imported path.
- `get_performance` with `enabled: true` collects CPU/GPU samples without opening
  the profiler panel. Use `enabled: false` to stop this capture.

The endpoint implements MCP versions `2025-11-25` and `2025-06-18`, with JSON
responses to POST, HTTP 202 for notifications and HTTP 405 for GET. Clients must
send `Accept: application/json, text/event-stream`. Browser origins are checked;
use the exact local endpoint origin. The first HTTP implementation requires
Linux, matching Blaze's current reactor support.

Focused transport checks:

```sh
python3 tests/support/mcp/check_http.py --compiler /path/to/encore
cc -O1 -g -pthread -fsanitize=address,undefined tests/mcp_mailbox.c -o /tmp/graphene-mcp-mailbox
/tmp/graphene-mcp-mailbox
```

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
