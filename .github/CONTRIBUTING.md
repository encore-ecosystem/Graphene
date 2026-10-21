# Contributing to Graphene

Graphene is developed in public through GitHub issues and pull requests.

## Development setup

Install Encore 0.1.4, Clang, a native linker, Vulkan development/runtime
libraries, and SDL3. Restore locked dependencies before building:

```sh
encore sync
```

Create a focused branch from `main`. Keep changes small enough to review and
add tests for behavior changes.

## Required checks

Run the same checks used by CI from the repository root:

```sh
encore format --check
encore check
encore lint
encore test
encore build
```

If formatting fails, run `encore format`, inspect the result, and repeat the
checks.

## Pull requests

- Explain the problem and the chosen solution.
- Link the related issue when one exists.
- Describe manual editor or Vulkan validation that CI cannot perform.
- Call out public API, scene format, or compatibility changes explicitly.
- Do not commit `target/` directories or generated local artifacts.

Changes reach `main` through pull requests after the required Linux CI check
passes.
