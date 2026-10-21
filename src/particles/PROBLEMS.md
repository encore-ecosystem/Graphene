# Encore issues encountered by VFX

## Heap-handle vector stride mismatch

Confirmed with local `encore 0.1.6-neumann`, executable SHA-256
`bd655564f45bc289ece15c8175aae703abc11ec80fe8d1f576dc0546eac8ad5e`,
source HEAD `7b30f1e2fa740de07209536ed477ea0955b7160c` (dirty checkout).

Report: https://github.com/encore-ecosystem/encore/issues/147

`Vec[T<H>]` allocation/access use the inline payload stride, while generated
cleanup uses the handle stride. The editor crashed releasing its scene VFX
instances. A standalone Core-only two-element vector of a four-u64 heap node
also exits 139 under `MALLOC_PERTURB_=254`; generated LLVM and machine code
confirm 32-byte stores versus 8-byte cleanup.

Regression needed in the compiler: allocation, get/set, iteration and automatic
cleanup of wide heap nodes in vectors, with perturbed allocator contents.

Graphene uses explicit inline slots containing GPU executor handles and inline
scene records until the compiler is corrected. This preserves normal owning
handles and deferred GPU destruction; it does not fix the compiler defect.

## Numeric vector elements use the wrong calling convention

Confirmed with the same local modified compiler identified above. Regression
evidence added to the existing report rather than opening a duplicate:
https://github.com/encore-ecosystem/encore/issues/101#issuecomment-6305580914

`vec![0.5_f32, 2_f32, -1]` builds successfully but fails an exact third-element
comparison. EHIR and LLVM call `Vec::push[f32]` with an `isize`/`i64` argument.
In the actual GPU buffer this corrupted the disabled fluid slice from -1 to
2 or 8 and made simulated volumes invisible. Replacing the literal with
`-1_f32` makes the standalone executable return success. Graphene explicitly
types all padding and slice values in the fluid uniform buffer. The compiler
defect remains unresolved here; add contextual vector-element inference and
generated-call argument validation regressions to the compiler suite.
