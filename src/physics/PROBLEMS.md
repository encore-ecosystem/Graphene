# Encore compiler issues encountered by physics

- 2026-10-23: numeric casts followed by multiplication pass checking but emit invalid EHIR on installed Encore 0.1.6-neumann. Existing [#141](https://github.com/encore-ecosystem/encore/issues/141). A core-only sine range-reduction reproducer failed at `x - turns as f32 * period`; explicit `(turns as f32) * period` builds. No compiler change. Regression: check, build and run both forms, or reject the ambiguous form during semantic checking.
