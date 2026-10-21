# Audio development: Encore compiler evidence

## Package-root-qualified free function emits an undefined LLVM value

Confirmed on local `encore 0.1.6-neumann`, compiler SHA-256
`bd655564f45bc289ece15c8175aae703abc11ec80fe8d1f576dc0546eac8ad5e`.
`encore check` succeeds, but a lib+main project calling
`refrain::values::parse_number("0.5")` emits `call float @parse_number(...)`
without the declaration and fails native compilation. A direct import followed
by `parse_number(...)` builds and executes correctly with the same compiler.

Graphene first encountered this in waveform parsing. Its audio adapter now uses
explicit imports for both numeric parsing and sound-path validation. This is a
verified application workaround, not a compiler fix.

The first full editor package build exposed another manifestation of qualified
lookup (#124): a private helper named `error()` in the new `audio` dependency
made the existing `native::error()` call in Graphene RHI resolve to `AudioError`
instead of `str`. The compiler reported an assignment mismatch at
`window.state.message = native::error()`. Audio's private helpers now use the
distinct names `audio_last_error` and `audio_checked`. The original diagnostic
is retained in `target/graphene-audio-build-v2.log`.

Related report and new reduced evidence:
https://github.com/encore-ecosystem/encore/issues/124

Reproduction, failing and working logs, binary identity and reviewed comment:
`target/compiler-audio-repro/`. The compiler regression suite should assert
successful native emission and correct results for package-root-qualified free
calls in a project with both `src/lib.enq` and `src/main.enq`.
