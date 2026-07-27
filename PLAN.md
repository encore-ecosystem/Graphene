# Graphene command platform

## Goal

Graphene remains a complete standalone visual editor. Its UI, console,
Automation API and agents perform the same typed commands through one command
bus. Encoder remains a separate application and integrates through a
first-party plugin.

```text
Graphene UI ---------> CommandBus
Graphene console ----> CommandBus
                           ^
Graphene agent -> MCP -> Automation API
Encoder agent  -> MCP -> Automation API
Encoder plugin ------> Automation API
```

## State and commands

`EditorState` is the single owner of project, scene, assets, selection,
history and tasks. UI handlers stop mutating parallel arrays directly.

`GrapheneCommand` covers project open/create/save/import, scene add/delete/
duplicate/rename/reparent, transforms, properties, selection, build/play/stop
and undo/redo. Execution validates first, applies one transaction, emits a
structured result and publishes ordered `GrapheneEvent` values with a state
revision.

Interactive edits use begin, preview and commit/cancel. Preview updates the
viewport without filling history; commit produces one undo entry. Public
commands address stable object IDs rather than UI indices.

A single command registry supplies CLI syntax, API schemas, MCP tool schemas,
completion and documentation.

## Console, API and MCP

The Graphene console parses a command and calls the command bus in-process. It
is not a shell and does not scrape textual output.

The local Automation API supports protocol negotiation, snapshots, queries,
command execution/cancellation, event subscriptions and task progress. The
first transport is a Unix socket or named pipe protected by a short-lived
capability token. Remote TCP is out of scope.

`graphene-mcp` is a separate stdio MCP server. It contains no editor logic and
translates tools and resources to the Automation API. It exposes project,
scene, selection, assets and diagnostics as resources, and registered commands
as narrowly scoped tools. It never exposes an unrestricted shell command.
Approvals remain enforced by the API and command bus.

## Encoder

Encoder's first-party Graphene plugin discovers or launches Graphene, connects
to the Automation API, mirrors selection/tasks/diagnostics and supplies
Graphene tools to Encoder agents through `graphene-mcp`. Reconnection starts
with a revisioned snapshot before resuming events.

Graphene and Encoder remain independently usable. A public plugin marketplace
is deferred until this first-party protocol is stable.

## Rendering

Luma owns the Vulkan window, device, swapchain and presentation. Graphene
borrows Luma's exact backend device, swapchain and active frame without taking
ownership. Luma records retained base UI, Graphene binds its compatible
pipeline and records the viewport, then Luma appends transient overlays. Luma
performs the only submit and present. The CPU UI bridge and intermediate
sampled viewport are removed from the editor path.

The editor is event-driven while idle. Hover, focus, menus and other chrome
updates patch retained cache regions and request one composed frame; Graphene
records the viewport directly into that frame, with no stored viewport image.
Simulation renders continuously in `PresentMode::Immediate`; viewport frames
are not artificially capped to 60 Hz. A flat retained hit index keeps pointer
latency independent of widget-tree depth.

Startup uses one complete lightweight Graphene frame while project resources,
the retained editor and the viewport are prepared. It never exposes a black
swapchain, partially cached chrome or a viewport hole. The complete local
debug editor must become interactive within one second.

The renderer work has two hard-separated stages. First, the backend reaches
the architectural performance maximum: SVG and geometry quality, atomic
startup, allocation-free indexed composition and all known retained rendering
hot paths are completed and profiled. The direct Vulkan composition frame is
then validated across supported platforms.

In the direct-backend stage Graphene records only its viewport pass into the
borrowed region; Luma records base UI and overlays and remains the sole
submit/present owner. Graphene requires Vulkan on every supported platform.
Failure to initialize a compatible Vulkan backend is explicit.

## Delivery

1. Extract `EditorState` without changing the visible UI.
2. Add the command bus and migrate history, scene and transform operations.
3. Migrate project, build and simulation operations.
4. Drive reactive UI, viewport and persistence from command events.
5. Add the command console and local Automation API.
6. Add `graphene-mcp` and agent permissions.
7. Integrate the Encoder plugin.
8. Adopt Luma's borrowed direct Vulkan frame and delete the CPU bridge.
9. Finish SVG/startup quality, allocation-free indexed composition and the
   backend-independent architectural performance maximum.
10. Pass the SVG, startup and performance gate on the direct Vulkan backend.
11. As a separate stage, adopt Luma direct Vulkan composition and remove the
    intermediate sampled viewport image from the primary path.

## Current implementation status

- Scene, selection and history are owned by `EditorState`; add, duplicate,
  delete, rename, visibility, parent and transform operations use one typed
  command history. Interactive rename and viewport drags use
  begin/preview/commit/cancel and create one undo entry.
- Save/build and play/simulate/stop lifecycle transitions also pass through
  `GrapheneCommand`; snapshots expose the authoritative project, task and
  simulation state. Filesystem writes and the compiler worker execute the
  requested effects and report completion back as commands.
- The visible in-process console, local socket/named-pipe Automation API and
  standalone `graphene-mcp` all use the same command decoder and command bus.
  Discovery uses a short-lived 256-bit token and a user-only discovery file.
- Automation capabilities are enforced per method (`read`, `scene.write`,
  `project.write`, `build.execute`, `simulation.control`) and advertised in
  both handshake and discovery. Sensitive delete/save/build requests enter a
  user-facing approval broker; Allow once produces a single-use token and
  replay is rejected. MCP can poll approval state and retry the original tool.
- Revisioned snapshots and ordered `graphene.events` polling are implemented;
  the bounded event window reports `resetRequired` when a client must recover
  from a fresh snapshot.
- Approval requests and one-use grants expire and are bounded; denial and
  replay are represented explicitly by the broker.
- Luma creates the editor window and owns Vulkan presentation. Graphene's RHI
  borrows the exact Luma backend device, swapchain and active frame and records
  only its viewport commands. The editor has no CPU bridge or sampled
  intermediate viewport.
- The editor selects Luma's Direct Vulkan compositor explicitly. Retained
  base, viewport and overlays are ordered inside one compatible render pass;
  Luma owns the single submit/present lifecycle.
- Direct swapchain generations retire by
  `VK_EXT_swapchain_maintenance1` presentation fences during resize and
  presentation-policy changes, without `queue_wait_idle` in the rebuild path.
  Direct UI text is packed into persistent atlas pages and uploaded once
  per dirty page/frame instead of allocating and submitting one Vulkan image
  per string.
- The rebuilt editor passed a live Wayland resize from a 1770x1140 swapchain
  to 2096x2076. Vulkan trace showed a successful acquire/submit/present on
  both generations, and the captured replacement frame retained correct
  layout, text, borders and colors.
- The single-pass path is runtime-validated in the real editor: Luma chrome,
  Graphene scene, selection outline and gizmos compose correctly after live
  Wayland resize, with one acquire/submit/present sequence per frame.
- Temporarily constrained resize extents may collapse the viewport surface.
  Graphene presents retained chrome once instead of aborting or busy-redrawing,
  then resumes direct viewport recording when a valid extent returns.
- There is no producer/consumer queue handoff or alternating viewport image;
  Graphene records directly into Luma's command buffer.
- The retained base excludes the viewport and transient overlays. The scene
  and every overlay are composed exactly once, eliminating alpha-darkened
  borders. Same-size expose events only re-present; compositor startup
  configure is consumed before the expensive initial editor layout.
- Main-editor and project-browser pointer paths use Luma's retained hit index.
  The browser reconciles hover changes instead of rebuilding its complete
  widget tree.
- Composition receives retained viewport bounds and overlay layouts directly;
  stable overlays are cached only when their state changes. The uncapped
  reference run is approximately 6100 FPS without an overlay and 5500 FPS
  with a cached tooltip, compared with the previous approximately 300 FPS.
- Graphene loads its fixed named icon set lazily instead of enumerating all
  1703 Lucide assets. Together with linear editor text scans and Luma's
  flex-layout fast paths, the atomic startup frame appears in roughly 160 ms
  and the complete local debug editor becomes interactive in roughly
  520–550 ms on the reference machine.

## Backend-independent acceptance

- UI, console, API and MCP produce identical command results.
- Execute, undo and redo restore identical scene state.
- Failed commands do not leave partial changes.
- One drag creates one history entry.
- MCP cannot bypass capabilities or approvals.
- Encoder reconnects after Graphene restarts.
- Existing Graphene UI and project files remain compatible.
- No per-frame CPU-to-GPU UI copy remains.
- Startup frame appears within 200 ms and the complete local debug editor is
  interactive within one second without partial frames.
- Startup, idle, hover, animation and continuous rendering profiles contain no
  known avoidable Luma/Graphene full-tree traversal, per-frame allocation,
  redundant repaint or synchronous CPU/GPU wait.

## Direct Vulkan acceptance

- Direct Vulkan records base UI, viewport and overlays into one Luma-owned
  frame lifecycle on Linux and Windows.
- Unsupported or failed Vulkan initialization stops startup with an
  actionable diagnostic.
- Format, check, lint, tests and builds pass.
