# Encore compiler problems

## Successful incremental build can leave the executable stale

Status: confirmed compiler build-cache defect while fixing the Graphene viewport
view-mode selector on 2026-08-02.

After changing `src/main.enq`, both `encore check` and an extreme `encore build`
completed successfully and the build reported compiling Graphene EHIR, but
`target/extreme/graphene` retained its old timestamp and behavior. The source
contained the new vertical popup while the launched executable still rendered
the removed horizontal strip. No module/object or executable in the selected
profile was refreshed. A clean profile build is required to obtain the changed
program.

Regression test: build a minimal executable, record its timestamp and output,
change a user module's visible string and control-flow shape, then invoke the
same incremental build command. Require a refreshed executable that prints the
new string. Run the test once with a warm cache and once after changing only a
transitive UI-builder function; a successful command must never leave the old
binary in place.

## Adding an adjacent `Vec` field corrupts a retained heap resource in another field

Status: confirmed compiler aggregate/ownership ABI defect in the Graphene
editor on 2026-07-30.

Adding `virtual_mesh_frame_slots: Vec[u32]` immediately after the existing
`virtual_mesh_frame_ready: Vec[bool]` field of `ViewportRenderer`, initializing
both vectors consistently, and only reading the new scalar slots caused the
next frame to segfault in `Vec.get<BindGroup>` inside
`gpu_virtual_geometry_record_debug`. GDB showed that the unrelated nested
`GpuVirtualGeometry.groups` vector had an invalid address. Removing only the
adjacent field restored startup. The source passes `encore check`, so this is
not a Graphene bounds or constructor diagnostic.

Regression test: construct a large aggregate containing a `Vec` of entries,
where each entry owns a heap object containing another `Vec[NativeHandle]`.
Insert an additional scalar `Vec[u32]` adjacent to an existing `Vec[bool]`,
initialize every field positionally, then repeatedly access the nested native
handles. Require stable addresses, valid handles, and clean destruction under
AddressSanitizer before and after the field insertion.

## Mutating a reference-type `Vec` element in a `for` loop emits invalid EHIR

Status: confirmed compiler lowering defect while generalizing virtual geometry
to multiple mesh assets.

Assigning `virtual_mesh.frame_ready = value` where `virtual_mesh` is the loop
variable of `for virtual_mesh in self.virtual_meshes` passes `encore check`,
but `encore build` rejects the lowered function with `setfield target
'entry_for_..._item_virtual_mesh' is not a pointer`. Iterating by index,
constructing a replacement value, and committing it with `Vec.set` avoids the
invalid target.

Regression test: create a reference-type struct with a mutable boolean field,
store it in `Vec`, assign that field from a `for item in values` loop, and read
the result afterward. Require both semantic checking and EHIR/LLVM compilation
to succeed and verify the mutation at runtime.

## Replacing a vector element can invalidate shared nested native resources

Status: confirmed ownership-lowering defect, reproduced in the real Graphene
editor on 2026-07-30.

A `Vec.set` replacement rebuilt `VirtualMeshRendererEntry` from the old entry's
shared `GpuVirtualGeometry` plus a new boolean. The replacement remained
readable, but a nested indirect-command `GpuBuffer` had already been released;
the following draw passed native handle `4` and crashed in
`graphene_vk_frame_draw_mesh_tasks_indirect`. Keeping the resource registry
immutable and storing mutable per-frame booleans in a parallel `Vec[bool]`
avoids replacing owned resource graphs.

Regression test: put a struct containing several nested native-resource
wrappers and vectors into `Vec`, replace the element while reusing those nested
fields, then call every retained resource and destroy the graph exactly once.
Run under AddressSanitizer and require valid handles, no early release,
double-free, use-after-free, or leak.

## Repeated `Vec.get` of reference wrappers can corrupt `Option` cleanup

Status: confirmed ownership-lowering defect, reproduced in the real Graphene
editor on 2026-07-30.

`object_meshes` stored reference-type `MeshId` wrappers. Repeated nested calls
to `Vec[MeshId].get` while grouping actors by virtual mesh eventually aborted
in `__drop_Option_MeshId` with glibc heap corruption. Storing the stable `u64`
payload and constructing `MeshId` only at API boundaries removes owned
`Option` values from the per-frame traversal.

Regression test: fill `Vec` with a small reference-type wrapper around a scalar,
repeatedly call `get` from nested loops and helper functions for at least one
minute, and use every returned value. Require stable values and clean
completion under AddressSanitizer with no invalid `Option` release.

An experimental explicit retained-load implementation was inspected in
generated IR: the specialization correctly omitted retain operations for
`f32` and retained aggregate values. The large mutable GPU graph still became
corrupt, so the later `Option[f32]` failure was secondary heap damage, not
evidence that scalar values were retained. The remaining defect is in
aggregate/container ownership or replacement lowering.

## Repeated owned-field access can double-release nested values

Status: suspected compiler ownership-lowering defect, reproducible in Graphene.

Calling `layout.bounds()` twice in one resize expression eventually crashed in
`__drop_Vec_Widget` through `__drop_WidgetLayout`. Taking one `Rect` snapshot
and reading both dimensions from it avoids the crash.

Regression test: define an owned aggregate containing a nested `Vec`, expose a
small copyable field through a by-value accessor, call that accessor twice on
the same value in one expression, then continue using and dropping the
aggregate. Run the sequence in a loop and assert clean completion under
AddressSanitizer.

## Repeated construction from shared vector-backed values corrupts ownership

Status: suspected compiler ownership-lowering defect; minimal reproduction is
still required.

During continuous camera input, repeatedly rebuilding `viewport(...)` with one
shared `EditorIcons` value eventually crashed inside `viewport`. `EditorIcons`
contains many `SvgDocument` values backed by `Vec[SvgLine]`. Suppressing
unnecessary viewport reconstruction removes the high-frequency trigger, but
does not prove the underlying ownership behavior correct.

Regression test: create a struct containing several vector-backed values, pass
the same instance by value through nested builder functions that copy fields
into returned aggregates, drop each result, and repeat for thousands of
iterations. Verify that the source remains readable and that the program exits
without a use-after-free, double free, or leak under AddressSanitizer.

## Sequential by-value accessors can double-release different owned fields

Status: suspected compiler ownership-lowering defect, reproduced by the
one-minute Graphene viewport input soak on 2026-07-30.

`collect_surface_slots_clipped` called `layout.widget()`, `layout.bounds()`, and
then `layout.children()` on one `WidgetLayout`. After repeated RMB/WASD frame
rebuilds the editor crashed in `__drop_Style`, reached from
`WidgetLayout__widget`. An attempted workaround returning all three fields
through one `traversal_parts()` tuple still crashed in
`__drop_Vec_Widget` inside that accessor. This indicates that returning the
owned fields as one tuple does not avoid the faulty aggregate destruction.

Regression test: define an aggregate with two independently owned nested
vectors and one copyable field. Read them through three by-value accessors in
sequence, recursively process the returned child aggregate, and repeat for at
least one minute under AddressSanitizer. Assert clean destruction, stable
vector contents, and no use-after-free or double free.

## EHIR validation loses type consistency after owned match assignment

Status: suspected compiler lowering defect; `encore check` accepts the source
but `encore build` rejects the generated EHIR.

After matching `Option[SurfaceSlot]::Some(slot)`, assigning the owned `slot` to
an existing `SurfaceSlot` variable before reading two scalar accessors produced
`error[ehir-validation]: function 'main': store source and destination types do
not match` without a source location. Moving the scalar reads before the owned
assignment avoids the invalid lowering.

Regression test: match an `Option` containing a struct with owned and scalar
fields, assign the payload to an existing variable of the identical explicit
type, and access scalar fields on the payload on both sides of that assignment.
Require both semantic checking and EHIR/LLVM compilation to succeed, and make
the EHIR validator diagnostic retain the originating source span.

## `Option[f32]` lowering emits invalid LLVM float constants

Status: confirmed compiler backend defect while adding virtual-geometry
cluster bounds.

Both an explicit `match values.get(index)` returning `f32` and
`values.get(index).unwrap_or(0.0_f32)` passed `encore check` but failed when
the standalone test worker was compiled. LLVM reported `integer constant must
have integer type`; the emitted call used a malformed float argument. Changing
the public cluster-builder input to `Vec[Vec3]` avoided instantiating the
broken scalar option path and compiled correctly.

Regression test: instantiate `Vec[f32]`, call `get` for an in-range and
out-of-range index, exercise both explicit `Option` matching and
`unwrap_or(0.0_f32)`, and assert the values at runtime. Require the standalone
test worker's generated LLVM IR to pass `llvm-as` before linking.

## Chained calls on a copyable native-resource wrapper emit malformed LLVM

Status: confirmed compiler backend defect while adding GPU cluster culling.

A boolean expression containing several sequential calls such as
`buffer.available() && buffer.write_u32(...) && buffer.write_f32(...)` passes
`encore check`, but compiling any test worker emits an invalid line at
`__retain_GpuBuffer(%GpuBuffer ...)`; Clang reports `integer constant must have
integer type`. Because the affected public function is part of the library,
every otherwise unrelated test worker fails at the same generated LLVM line.
Splitting the chain into simple statements was insufficient. Building one
packed `Vec[f32]` and issuing a single batch write avoids this backend path.

Regression test: define a small copyable native-handle wrapper with one boolean
query and integer/float write methods. Call those methods repeatedly on one
local wrapper in a chained boolean expression, then compile two standalone
workers (one that calls the function and one unrelated worker importing the
library). Require both generated LLVM modules to pass `llvm-as` and execute the
called worker to verify all writes.

## Large retained-UI builders exhaust the main-thread stack

Status: confirmed compiler backend/code-generation defect; reproduced during
the active Graphene editor input soak on 2026-07-30.

The generated `viewport` function reserves roughly 117 KiB in its prologue and
the generated `main` event loop retains a very large fixed stack frame. After
repeated camera input and retained-UI rebuilds, Linux faults in `viewport`'s
stack-probing instruction before any function body instruction executes. The
core dump points at `movq $0, (%rsp)` in the prologue, proving this is stackd
exhaustion rather than a renderer or Graphene scene-logic access.

Regression test: generate a retained UI from a large builder inside a long
event loop, continuously rebuild several nested widget branches for at least
one minute on the default 8 MiB Linux stack, and assert bounded stack usage and
clean completion. The backend should reuse loop-lifetime stack slots and split
or heap-promote oversized aggregate temporaries instead of assigning every
source temporary a function-lifetime `alloca`.

## Retained virtual-mesh entries crash during generated shutdown cleanup

Status: confirmed compiler ownership-lowering defect; reproduced by the
2026-07-31 editor soak after camera flight and sphere LOD streaming.

The fresh Graphene binary stayed in the AutoLOD renderer, then terminated with
SIGSEGV in `encore_heap_release` from generated
`__drop_VirtualMeshEntry_H`/`__drop_Vec_VirtualMeshEntry_H` while dropping
`ViewportRenderer`. The invalid pointer was reached during aggregate cleanup,
not during Vulkan recording; a coredump was captured for PID 113872. Clearing
the retained virtual-mesh vector before destroying its owning device avoids the
invalid post-device release in Graphene, but the compiler should generate
cleanup in a valid ownership order for nested native/heap aggregates.

Regression test: define an aggregate containing a `Vec[Entry<H>]`, where each
entry owns a nested resource aggregate and a scalar field. Exercise the owner
for at least one minute, destroy the owner/device, and require generated drop
code to complete under AddressSanitizer without an invalid heap release or
double free.

## Large generated `Vec[u32]` return stalls in retain lowering

Status: confirmed compiler runtime/code-generation defect while validating the
macOS Metal backend on 2026-08-21.

A generated shader function that builds and returns a roughly 2,600-element
`Vec[u32]` compiles successfully but never completes in either the editor or a
single focused standalone test. Process samples consistently stop in
`__retain_VecStorage_u32_H` via `encore_node_retain` while returning through
`ViewportRenderer::create_composed`. Preallocating the exact vector capacity
does not change the failure. The macOS path now returns a three-word artifact
token and lets the native backend resolve it to the checked-in metallib; Vulkan
targets continue to receive the complete SPIR-V vector.

Regression test: generate a function that preallocates, fills, and returns a
2,600-element `Vec[u32]`; consume its length and first element from a standalone
test and from a second function returning an aggregate. Require both paths to
finish promptly under a sampling profiler without remaining in
`__retain_VecStorage_u32_H`.
