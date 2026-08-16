---
name: implement-new-feature
description: Research, compare, design, implement, and prove new Graphene editor or engine features, including deciding whether reusable functionality belongs in Luma or another companion repository. Use whenever adding or substantially changing user-visible Graphene behavior, editor interaction, viewport tooling, rendering, input, or reusable UI/platform infrastructure.
---

# Implement a New Feature

Treat implementation as complete only after proving the feature in the running
editor. A successful build or unit test is necessary but never sufficient.

## 1. Establish the behavior

- Inspect the relevant Graphene and companion-repository code, manifests,
  nearby tests, current dirty changes, and repository instructions.
- Restate the expected user-visible behavior and measurable acceptance criteria.
- Identify important edge cases, performance constraints, and regressions.
- If the feature touches UI, define its interaction and visual state matrix
  up front: default, hover, pressed, active/selected, keyboard focus, and
  disabled (plus cursor and tooltip behavior when applicable). Specify the
  expected feedback, focus behavior, and accessibility semantics for each
  state rather than treating a successful click as sufficient.
- For UI work, record the style constraints that must remain consistent with
  the existing Luma/Graphene theme: color and typography tokens, spacing,
  sizing, borders, radii, contrast, z-order, opacity, and transition timing.
- Preserve unrelated user changes.

## 2. Research candidate implementations

- Search the internet for existing implementations in comparable engines,
  editors, libraries, papers, and official documentation.
- Prefer primary sources and inspect licensing before copying code or assets.
- Find at least two credible approaches when the problem permits it.
- Add an original approach when it is materially different or better suited to
  Graphene's architecture.
- Record links, the relevant idea from each source, and which parts are
  inference rather than documented fact.

Do not copy an implementation blindly. Adapt algorithms and interaction
semantics to Graphene, Encore, Luma, the renderer, and the current data model.

## 3. Compare the approaches

Compare candidates using evidence appropriate to the feature:

- correctness and interaction quality;
- CPU, GPU, allocation, latency, and frame-time cost;
- implementation and maintenance complexity;
- compatibility with retained UI and the viewport render path;
- testability, portability, and failure modes;
- required public API changes;
- licensing and dependency risk.

Use measurements or a focused prototype when static analysis cannot establish
the difference. Present a compact comparison and recommend one option with
concrete reasons.

## 4. Obtain the user's decision

Present the viable options before implementation and ask the user to choose.
Do not implement while that choice is unresolved.

Skip a new choice only when:

- the user already selected an approach explicitly; or
- only one implementation is viable and the evidence clearly demonstrates it.

In the latter case, explain why the alternatives are not viable before
proceeding.

## 5. Choose repository ownership

For every new primitive or API, decide where it belongs:

- Put it in **Luma** when it is a reusable UI, input, windowing, compositor,
  retained-layout, or platform capability that benefits applications other
  than Graphene.
- Put it in another companion repository when it is a reusable responsibility
  already owned by that repository.
- Keep it in **Graphene** when it expresses engine/editor policy, scene
  semantics, Graphene-specific UX, or has no credible reuse outside Graphene.

Do not move code merely to make Graphene smaller. State the reuse scenario and
the ownership rationale. If companion work is required, implement and validate
the lowest reusable layer first, then integrate it into Graphene. Keep API
changes minimal and avoid Graphene-specific names in reusable packages.

## 6. Implement safely

- Make the smallest coherent implementation of the chosen design.
- Keep interactive hot paths free from unnecessary layout, allocation,
  synchronization, logging, and full-tree rebuilds.
- Add or update focused tests for algorithms, state transitions, regression
  cases, and repository boundaries.
- If investigation exposes a suspected or confirmed Encore compiler defect,
  append it to the repository's `PROBLEMS.md`. Briefly state the problem,
  evidence and status, and the minimal regression test the compiler suite
  should add. Do not silently hide compiler problems behind an application
  workaround.
- For UI features, use the existing Luma/Graphene theme primitives and style
  tokens. Keep UI state explicit and deterministic; do not derive active,
  hover, or focus styling from incidental draw order or stale retained-widget
  state. Preserve pointer capture, keyboard focus, layout stability, and
  input routing while applying visual feedback. Avoid unexplained inline style
  duplication and ensure transitions do not cause flicker, transparency, or
  loss of the active/selected indication.
- Preserve undo/redo, selection, cached UI, renderer, and input behavior unless
  the accepted design intentionally changes them.
- Format, check, lint, test, and build every modified repository with its native
  tooling. Run focused checks first and the applicable full suite before
  completion.

## 7. Prove the feature in the real editor

Launch the newly built binary, not an older process. Verify its PID and binary
path. Test with real or emulated mouse and keyboard input; direct state mutation
or calling an internal helper does not count as interaction validation.

Exercise at minimum:

- the new feature's primary flow and edge cases;
- camera look, flight, pan, and zoom in the viewport;
- object selection and deselection;
- move, rotate, and scale gizmos on all applicable axes;
- relevant toolbar, hover, focus, and keyboard shortcuts;
- undo/redo when the feature changes scene state.
- If the feature adds or changes UI, interact with every new control using the
  mouse and keyboard: enter and leave hover, press and release, keep the
  control active/selected, move focus with the keyboard, and verify disabled
  behavior. Confirm that hover feedback does not erase active/selected state,
  and that tooltips, cursor changes, popups, and focus rings appear when
  applicable.

Capture screenshots:

- before interaction;
- while the feature is actively being used;
- after the resulting state is committed;
- for every visual state central to the acceptance criteria.

Inspect the screenshots yourself. Confirm that the pixels demonstrate the
claimed behavior and that Details, selection, gizmos, overlays, and viewport
composition agree with application state.

- For UI features, capture and inspect every acceptance-critical state from
  the state matrix (at minimum default, hover, pressed/active, and focused;
  include disabled, tooltip, and popup states when applicable). Check visual
  consistency of theme tokens, spacing, typography, contrast, borders,
  opacity, and transitions, including neighboring controls that could regress.

Imitate a real user's activity continuously for at least 1 minute unless the
user specifies a longer duration. Use both mouse and keyboard input and
interact with every control, viewport tool, object, and workflow relevant to
the feature; do not leave the editor idle. Check process liveness, logs, crash
dumps, and renderer/backend errors both during and after the run.

## Completion gate

Report completion only when all of the following are true:

- the user-selected design is implemented in the correct repository;
- focused and full applicable automated checks pass;
- the feature works through actual keyboard and mouse interaction;
- inspected screenshots visibly prove the required states;
- every required UI state and style constraint is visibly proven when the
  feature touches UI; a functional click without correct hover, focus,
  pressed, active, disabled, or consistent styling is incomplete;
- the manipulated-editor soak test completes without a crash, hang, corrupted
  frame, or new backend error;
- no stale process or old binary was mistaken for the tested build.

If any item is unverified, say exactly what remains unverified. Never claim
that a feature is fixed from code inspection, compilation, tests, or process
liveness alone.

## Handoff

Summarize:

- the chosen approach and why it won;
- ownership decisions across Graphene and companion repositories;
- changed files and public APIs;
- automated check results;
- interaction script or exact input sequence;
- UI state matrix, controls exercised, and screenshot paths proving each
  acceptance-critical visual state;
- soak duration, activity performed, PID, logs, and crash-dump result;
- remaining risks or follow-up work.
