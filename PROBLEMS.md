# Confirmed Encore compiler issues

- **#166 — qualified exported calls lose the callee with an unsuffixed u32 argument.**
  `luma::format_decimal(value, 4)` passes checking but native compilation fails
  with `missing native LLVM callee: format_decimal`. A two-package reduction
  with a public `(f32, u32) -> str` function and nested re-exports reproduces
  the failure; its generated call uses a bare symbol and `usize` argument.
  Explicit `4_u32` builds and runs successfully with the same frozen local
  compiler (`f61f7a7950e66d79f90a14d310a4bfb3dfa8d651f9a2bccb3b324616d15f0971`).
  https://github.com/encore-ecosystem/encore/issues/166
  Status: reported; Graphene supplies explicitly typed precision arguments as
  a verified compatibility workaround. The compiler is unchanged.
  Regression: check, build and run unsuffixed and suffixed calls through nested
  package re-exports, requiring the same correctly typed callee and result.

- **#158 — mixed suffixed/unsuffixed numeric vector literals emit invalid LLVM IR.**
  `vec![16_u32, 17, 13]` passes `encore check`, but dev compilation with the local
  Encore 0.1.6-neumann executable passes `i64` elements to `Vec__push__u32`.
  Reproducer, commands and logs: `target/compiler-repros/ui-vector/`.
  The same failure is confirmed for `vec![1_f32, 0, 0]`; minimal reproduction
  and successful explicit-suffix variant: `target/compiler-repros/ui-vector-float/`.
  https://github.com/encore-ecosystem/encore/issues/158
  Status: reported; numeric vector lookup tables explicitly suffix each
  `u32` / `f32` element as a verified compatibility workaround. The compiler is unchanged.
  Compiler regression: check, build and run the three-element vector above;
  require valid u32 IR and the expected values, including after formatting.

- **#141 — numeric cast followed by multiplication accepted as a pointer cast.**
  Current local compiler accepts `vertex as usize* stride` and later emits an
  invalid scalar `usize__op` call with a pointer receiver; the analogous f32
  expression passes a pointer to `Vec3::from_spherical`. Existing report:
  https://github.com/encore-ecosystem/encore/issues/141
  Status: application uses `(vertex as usize) * stride` and parenthesized
  f32 casts, as already documented by Physics. No compiler modification.
  Regression: check/build/run cast-then-multiply expressions, or reject the
  ambiguous pointer spelling during checking with a source diagnostic.

## Standalone-test reflection linker compatibility

- The original `material_graph_editor.enq` test fails linking generated
  `__encore_reflect_*` callees from `material_graph/cooked.enq`, while the
  full editor executable builds successfully with the same compiler.
  Exact standalone diagnostic: `target/ui-proof/material-editor-link.log`.
  An explicit `binary::encode` / `decode[CookedMaterialGraph]` roundtrip in
  the test entry point makes the complete original editor test pass (exit 0).
  That roundtrip is retained as codec coverage and a current-compiler
  compatibility workaround; the material format and production code are unchanged.
  The same missing-symbol failure also occurred in `material_color_picker.enq`;
  adding the cooked-material codec roundtrip there makes the test pass with
  the same frozen compiler (26.1 seconds). Both consumers should be included
  in the compiler's reflection-retention regression coverage.
  Suspected reflection specialization/retention defect; the cause remains
  unisolated. Simple tuple/vector roundtrips within and across packages pass
  in `target/compiler-repros/nested-binary{,-cross}/`.
  No new compiler issue published. A compiler regression should link the
  indirect MaterialEditor consumer without requiring an explicit codec call.

## Updated compiler batched-test reflection regression

- During the final build the globally installed compiler was replaced externally,
  invalidating the running compiler's self-executable path (`encore (deleted)`).
  The source tree did not change. A local ignored copy of the current installation
  now isolates builds: `target/ui-proof/toolchain/`, SHA-256
  `f61f7a7950e66d79f90a14d310a4bfb3dfa8d651f9a2bccb3b324616d15f0971`.
- With this updated executable all four Game UI tests pass, but the broader editor
  run reports 13 passed / 10 failed: the affected standalone tests link duplicate
  generated reflection symbols, with one definition from `particles/asset.enq`.
  Log: `target/ui-proof/editor-tests-final.log`. The same cases passed with the
  previous local `4d0014...` executable (except the separate cooked-material
  specialization workaround documented above).
  A two-module nested-vector binary-encoding reduction builds successfully in
  `target/compiler-repros/duplicate-reflection/`; it does not reproduce this
  standalone-consumer failure. Cause unisolated; no compiler edits or speculative
  issue publication. All ten affected tests subsequently passed individually in
  fresh standalone workers using that same frozen compiler executable; result
  records: `target/ui-proof/editor-isolated-results.json` and
  `target/ui-proof/editor-isolated-run.log`. Together with the thirteen passing
  batched cases, this verifies all 23 editor tests, while the batched runner
  remains unreliable. The minimal reproductions do not isolate whether batch
  context or compiler cache state triggers the duplicate emission. The compiler
  suite should run these consumers together without duplicate
  `__encore_reflect_*` definitions.

## Large editor module tooling performance

- With the frozen `f61f7a...` compiler above, whole-project `encore check`
  was interrupted after more than eight minutes without diagnostics. A sampled
  checker stack is in import/scope resolution; formatting the 573 KB
  `src/main.enq` also took several minutes and was interrupted before writing.
  Its sampled stack is `encore_str_slice_chars` under `reorder_import_source`.
  Evidence: `target/terrain-finish/checker-stack.log` and
  `target/terrain-finish/formatter-stack.log`. The precise cause and scaling
  remain unisolated; this is a tooling-performance suspicion, not a confirmed
  semantic defect. Terrain module checks and the complete editor build passed.
  No compiler change or application import workaround was applied.
  Regression to add: benchmark check/format on generated large modules with
  many imports and UTF-8 lines, recording time as source size doubles.

## Encore #168 — sending a vector of tuples with nested vectors

Confirmed on installed Encore 0.1.6-neumann: returning a fresh `sending` packet
with `Vec[(str, Vec[u32])]` reports `non-transferable-region`. An equivalent
`Vec[MaterialShaderVariant]` with named `name` and `code` fields passes the
minimal check. Graphene uses the named record for material shader variants.
Report: https://github.com/encore-ecosystem/encore/issues/168.
Regression: accept a fresh packet with the tuple element for both `sending`
returns and worker argument transfer, while still rejecting live aliases.

## Encore #169 — transitive nested fields behind a public wildcard re-export

Installed Encore 0.1.10-neumann reports `unknown-field` on
`resolved.definition.graph.name` when `MaterialGraph` is only transitively
imported through a public wildcard re-export. A dependency-free five-file
reproducer passes with pinned 0.1.6; adding a direct owner-type import makes
0.1.10 pass. The same direct-import workaround also passes the material editor
check in an isolated fixture. Graphene now imports the graph owner explicitly
when checking/building with 0.1.10; this workaround does not fix the compiler.
Report: https://github.com/encore-ecosystem/encore/issues/169.
Regression: resolve nested inline struct fields through re-exports without
requiring consumers to import every nested record type directly.

## Encore #171 — unused vector fields have no native container layout

Official installed Encore 0.1.10-neumann (SHA-256 `40f587f9dcbc90bf3cdda2bfbcbdb7d3b8edc7bbd67ec334ecb782a96b2d6213`)
accepts unused public `Step` and `HistoryEntry { moves: Vec[Step], label: str }`
records, but native compilation emits ownership helpers referring to an
undefined vector layout. A dependency-free reduction fails both normal builds
and standalone tests; explicitly constructing `Vec[Step]` makes the normal
build and executable pass. Reproducer and logs:
`target/material-instances-work/llvm-vector-repro/`.

The material Color Picker standalone test reproduced the same failure twice:
`ContentEdit` refers to missing `Vec_ContentMove` and other unused browser
containers. Material editor test entries now explicitly retain browser/history
types as a compatibility workaround; both the Color Picker (96.3 seconds) and
material editor (95.1 seconds) regressions pass with it. This does not fix the compiler or change production
material behavior. Report: https://github.com/encore-ecosystem/encore/issues/171.
Regression: compile unused public records containing vector fields in both
normal and standalone paths, with and without reachable vector construction;
all emitted ownership helpers must have complete container layouts.

## Heap node call accepts an inline receiver and corrupts the call ABI

Confirmed with installed Encore 0.1.12-neumann (SHA-256
`66fed6804fad740c75dfbcd01ddf098cce7fe981e6ee7d069bd1536c685c295f`).
A dependency-free Store<H> with two Vec fields, `add(self: mut Self)` and
`state(self: Self)` builds successfully but exits 139. The original Graphene
backtrace retained an invalid Vec payload (0x4) in GameplayWorld.state before
binary encoding began. Explicit `Self&` and `mut Self&` receivers build and exit
0 with the same compiler. Graphene now uses the documented node receivers;
this does not fix the compiler's missing receiver-representation diagnostic.
Reproducer: `target/industrial-session/receiver-probe/broken.enq.txt`.
Report: https://github.com/encore-ecosystem/encore/issues/180.
Regression: inline-receiver calls on heap nodes must either be rejected or
perform a correct payload conversion, including records with owning Vec fields.
