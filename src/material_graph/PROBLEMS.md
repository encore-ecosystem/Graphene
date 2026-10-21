# Material compiler integration

Encore's installed compiler reporting `0.1.6-neumann` emits invalid EHIR for
`match Option[i32] { Some(value) => value, None => 1 }`: it stores a `usize`
literal into an `i32` result slot. A dependency-free reproducer and emitted
EHIR confirm the failure; `1_i32` compiles successfully. The material build
completion path uses that explicit type.

Tracked with additional evidence in
[encore-ecosystem/encore#78](https://github.com/encore-ecosystem/encore/issues/78).
The compiler suite should compile and run typed-payload/literal `match` arms
in both orders without numeric suffixes. The application workaround does not
resolve the compiler defect.

Restoring an active Material tab exposed a separate startup error path: an
unfilled external preview surface rejected the first frame, then aggregate
teardown crashed in `encore_node_release` while dropping `VirtualMeshEntry`.
Core dumps for PIDs 35045 and 35116 were recorded; the first backtrace is in
`target/material-startup-crash-backtrace.log`. The editor now uses a plain
panel until a preview image producer exists, and explicitly destroys the
viewport before its window on first-frame failure, matching normal shutdown.
The aggregate-drop observation is not classified as a new compiler bug without
an independent reproducer. A minimal retained-vector/early-return lifetime
regression would be needed before a separate compiler report.
