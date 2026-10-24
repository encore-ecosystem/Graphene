---
name: encore-language
description: Develop, debug, review, and explain programs written in the Encore language
disable-model-invocation: false
---

Use for `.enq` source files, `encore.toml` manifests, `encore.lock`


# Encore Language

Treat the Encore book and repository examples as the source of truth. Encore is
young and changes quickly; do not infer its syntax or APIs from similarly named
languages.

## Locate the documentation

Prefer the Markdown sources in the current checkout:

- Book index: `docs/enbook-en/src/SUMMARY.md`
- Book sources: `docs/enbook-en/src/`
- Runnable programs: `examples/`

If the checkout does not contain the book, use:

- [Encore book sources on GitHub](https://github.com/encore-ecosystem/encore/tree/trunk/docs/enbook-en/src)
- [Encore examples on GitHub](https://github.com/encore-ecosystem/encore/tree/trunk/examples)

Read `SUMMARY.md`, then open only the chapters relevant to the task:

| Task | Chapter |
| --- | --- |
| Create, build, run, or test a project | `getting-started.md` |
| Syntax, types, traits, errors, tests, and docs | `language-basics.md` |
| Focused syntax examples | `language-construct-examples.md` |
| Ownership, borrowing, allocation, and cleanup | `memory-model.md` |
| Tasks, async work, channels, and synchronization | `concurrency.md` |
| Diagnostics, lint, formatting, LSP, and IDE behavior | `analysis-tooling.md` |
| Manifests, lockfiles, dependencies, workspaces, and build scripts | `packages.md` |
| Publish a package to the index | `publishing-packages.md` |
| Target triples, toolchains, conditional compilation, and cross-builds | `targets.md` |
| Standard-library modules and APIs | `standard-library.md` |
| Native UI | `native-ui.md` |
| Current limitations or unstable behavior | `beta-notes.md` |

Search the book and examples before inventing an API:

```sh
rg -n '<syntax-or-api>' docs/enbook-en/src examples -g '*.md' -g '*.enq'
```

## Work on an Encore project

1. Inspect `encore.toml`, `encore.lock`, the relevant `.enq` files, and nearby
   tests before editing.
2. Consult the routed book chapters and find the closest repository example.
3. Preserve the project's package layout, dependency model, target, and style.
4. Make the smallest coherent change.
5. Add or update `#attr(test)` functions for changed behavior.
6. Validate from the package directory:

```sh
encore format --check
encore check
encore lint
encore test
encore build
```

Run the smallest relevant commands first, such as `encore test --filter
<name>`. Before handing off a broad or risky change, run the complete applicable
set. Use `encore format` to apply formatting when `format --check` reports
drift.

For executable behavior, use `encore run -- <args>`. For a configured
cross-target, use `encore build --target <triple>` and verify the produced
artifact where the required toolchain is available.

## Handle dependencies safely

- Use `encore add <package>` to add an index package.
- Use `encore sync` to restore the versions recorded in `encore.lock`.
- Use `encore update` only when intentionally selecting newer versions.
- Keep `encore.toml` and `encore.lock` consistent.
- Do not replace `index@` dependencies with absolute `path@` references merely
  to make a local build pass.
- Diagnose registry failures from the manifest, lockfile, configured index, and
  package cache before changing application code.

## Report compiler bugs

When an Encore compiler defect is suspected, read and follow the global
`encore-compiler-issues` skill before applying a workaround or reporting it.
On this installation its entrypoint is
`/home/meshushkevich/.codex/skills/encore-compiler-issues/SKILL.md`;
elsewhere use the available-skills catalog to locate it. If unavailable, state
that limitation and retain the reproduction evidence rather than inventing its
instructions.

The workflow confirms the defect, checks open and closed reports in
`encore-ecosystem/encore`, and publishes confirmed new bugs without separate
per-issue approval, unless the current task forbids external writes. Ordinary
application errors and unverified suspicions must not become compiler issues.

## Review Encore code

Check behavior first, then language-specific hazards:

- ownership, borrowing, and cleanup semantics;
- exhaustive enum matching and error propagation;
- trait visibility and bounds;
- async/task lifetime and synchronization;
- target-specific `#cfg` behavior;
- public and module documentation;
- manifest/lockfile reproducibility.

Support findings with the exact book chapter, repository example, compiler
diagnostic, or test result. If the book and implementation disagree, report
the mismatch rather than silently choosing one.
