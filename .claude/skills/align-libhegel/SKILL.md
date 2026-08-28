---
name: align-libhegel
description: "How to align the C++ wrapper layer to a new libhegel (hegel-c) release. Use after bumping the pinned libhegel version (cmake/libhegel.cmake + libhegel/hegel.h + nix/flake.nix, via .github/scripts/bump_hegel_rust.py), when `just check` fails to compile or link against libhegel after a pin bump, or whenever the hegel-c C API in hegel.h has changed and the wrappers in src/engine.{h,cpp} need to catch up."
---

# Aligning the C++ wrapper to a new libhegel release

`libhegel` is a Rust cdylib built from hegel-rust's `hegel-c` crate. hegel-cpp
compiles **directly against the vendored C header** (`libhegel/hegel.h`,
included as `<hegel.h>`) and links the prebuilt shared library that
`cmake/libhegel.cmake` downloads and SHA-256-verifies at configure time. There
is no redeclared prototype layer to maintain: the compiler checks every call
against the real header, and the linker checks every symbol against the real
library. Alignment here is therefore not prototype bookkeeping — it is fixing
the thin wrapper layer, its RAII ownership, and its mirrored constants when
the API moves. The public API (`hegel::test()`, `TestCase`, the generators,
`Settings`) must **not** change as a result — only the internal layers.

The layers, and what an ABI change usually touches:

- **`src/engine.h` / `src/engine.cpp`** (`hegel::impl`) — the run-lifecycle
  helpers (`settings_*`, `run_start`, `next_test_case`, `mark_complete`,
  result/failure getters), string-generator construction, and the draw
  primitives used by `src/generators.cpp`. Every fallible call funnels
  through `check_rc` (run lifecycle: throws `std::runtime_error`) or
  `DrawScope::raise_for_rc` (draws: maps result codes to exceptions). Most
  alignments end here.
- **`include/hegel/internal.h`** (+ implementations in `src/engine.cpp`,
  `hegel::internal`) — the template-visible draw primitives
  (`draw_integer`, `draw_float`, `draw_boolean`, spans) and the RAII handle
  classes over engine-owned compound-draw objects (`CollectionHandle`,
  `PoolHandle`, `StateMachineHandle`), plus the `SpanLabel` mirror of
  `hegel_label_t` and `state_machine_done` mirroring
  `HEGEL_STATE_MACHINE_DONE`.
- **`src/test_case.{h,cpp}`** — `TestCaseData` (owns the
  `hegel_test_case_t*`, freed in its destructor) and the `TestCase` method
  implementations. Touch when the test-case lifecycle or ownership changes.
- **`src/hegel.cpp`** — the run loop and the `Settings` →
  `hegel_settings_set_*` mapping switches (`Verbosity`, `Phase`, `Mode`,
  `Backend`, `HealthCheck`). Touch when settings or the run protocol change.

## What the compiler catches for you — and what it does not

Because the header is the binding, a pin bump surfaces most breakage as build
errors:

- **Removed or renamed symbol / retyped signature** → compile error at every
  call site. Fix the wrapper declaration in `src/engine.h` and its
  implementation together, then the callers.
- **Symbol declared in the header but no longer exported by the library** →
  link error when the test executables link `libhegel_c`.

What the build does **not** catch — the real alignment work:

- **Semantic changes**: ownership moving (who frees what), new sentinel
  values, error codes changing meaning. Read the header comments — they
  document ownership and return-code contracts per function.
- **New functions** that should be wired into the library (nothing references
  them, so nothing fails).
- **New optional parameters absorbed as `nullptr`** at the call site
  compile fine either way; whether `nullptr` is behavior-preserving needs a
  read of the header comment.
- **Behavior changes in the engine** that shift what tests observe: the
  `tests/find_quality/` and `tests/shrink_quality/` expectations, and the
  approval snapshots under `tests/approvals/`.
- **A stale build directory silently testing the old release** (see §2).

## The context-based ABI

Every fallible libhegel call follows one convention, and the wrapper layer is
shaped around it:

- The **first argument** is a `hegel_context_t*` (an error-reporting context;
  one per thread here, via `impl::thread_context()`).
- The **return value** is a `hegel_result_t` code (`HEGEL_OK` is 0; failures
  are negative — `HEGEL_E_STOP_TEST`, `HEGEL_E_ASSUME`, …).
- Any **value the call produces** (a handle, a count, a bool, a buffer) is
  written through a **trailing out-parameter**, never returned. The two
  exceptions that return values directly are `hegel_context_new` and
  `hegel_context_last_error`.
- On a non-OK return, the human-readable message is read back from the
  context via `hegel_context_last_error` (`impl::last_error`).

Two funnels route the codes to exceptions; keep new wrappers on them:

- `check_rc(ctx, rc)` (run lifecycle, `src/engine.cpp`) → any non-OK code
  becomes `std::runtime_error` with the code's label plus the context
  diagnostic.
- `DrawScope::raise_for_rc(rc, what)` (draw path) → `HEGEL_E_STOP_TEST`
  becomes `internal::HegelStopTest` (case marked OVERRUN),
  `HEGEL_E_ASSUME` becomes `internal::HegelReject` (INVALID),
  `HEGEL_E_INVALID_ARG` becomes `std::invalid_argument`, anything else
  `std::runtime_error`. Never mask an engine error with `tc.assume()`.

### Ownership: caller-owned results get RAII owners

The engine frees nothing it hands back. Every caller-owned result must have
exactly one C++ owner whose destructor calls the matching free:

- **Handles created per draw** — `CollectionHandle`, `PoolHandle`,
  `StateMachineHandle` (`include/hegel/internal.h`): the constructor acquires
  through the out-param, the destructor calls `hegel_*_free`
  (with `impl::thread_context()`), and the class is non-copyable. A new
  engine-owned object type gets a new handle class in this shape.
- **Run lifecycle** — `TestCaseData` frees its `hegel_test_case_t*`;
  `hegel.cpp`'s `ResultGuard` frees the `hegel_run_result_t*`; failures are
  freed with `hegel_failure_free` where they are consumed.
- **Engine-allocated buffers** (`hegel_generate_bytes` /
  `hegel_generate_string` results): `BytesResultGuard` / `StringResultGuard`
  in `src/engine.cpp` free them; the bytes are **copied immediately** into a
  `std::string` / `std::vector` — never hold the raw pointer past the guard.
- **Deliberate exception**: `hegel_string_generator_t` handles are immutable
  and shareable; the string-family generators build one at construction and
  release it via `impl::string_generator_free` from their owner's destructor.

`just check-sanitizers` (address+undefined and thread builds) is the safety
net for this section — run it whenever ownership moved, because the coverage
gate cannot see a leak or double-free.

## 1. Find the pin and diff the header first

The pin lives in **three files that must move together**, rewritten by
`.github/scripts/bump_hegel_rust.py` (run by
`.github/workflows/bump-hegel-rust.yml`) — do not hand-bump:

- `cmake/libhegel.cmake` — `HEGEL_LIBHEGEL_VERSION`
- `libhegel/hegel.h` — the vendored C ABI header, fetched from hegel-rust at
  the release tag and reformatted with `uvx clang-format` to repo style
- `nix/flake.nix` — `libhegelVersion` plus each platform asset's SHA-256

The release **tag is `v<VERSION>`** (note the `v` prefix — the raw path
without it 404s):

```bash
curl -sSL https://raw.githubusercontent.com/hegeldev/hegel-rust/v<VERSION>/hegel-c/include/hegel.h
```

**Diff the header before touching any code.** The bump commit already
replaced the vendored copy, so the most signal-per-line diff is:

```bash
git diff HEAD~1 -- libhegel/hegel.h   # or: git diff main -- libhegel/hegel.h
```

(Compare vendored-to-vendored, not vendored-to-upstream: the vendored copy is
clang-formatted to repo style, so a diff against the raw upstream header
drowns in formatting noise.) If the diff is empty or comment-only, the
alignment is a no-op and you only need `just check` to confirm (§5).

## 2. Get the matching library — wipe stale build directories

`cmake/libhegel.cmake` downloads the prebuilt library for the host platform
at configure time and verifies it against the release's `.sha256` sidecar.
There is no runtime version check; the pinned, hash-verified download is the
guarantee — **provided the configure actually re-ran with the new pin**.

`HEGEL_LIBHEGEL_VERSION` is a CMake **cache** variable: an existing `build/`
directory keeps the old value and keeps testing the old release, silently.
After a pin bump, start clean:

```bash
rm -rf build
```

(That covers `build/coverage` and `build/san-*` too.) Platform notes:

- No prebuilt exists for darwin/amd64; on Apple Silicon the download works
  and the module rewrites the dylib's install name itself.
- To test against a locally built engine instead, pass
  `-DHEGEL_LIBHEGEL_LIBRARY=/path/to/libhegel_c.<ext>` (e.g. built from a
  `../hegel-rust` checkout **at the release tag** — a checkout on another
  ref is a different ABI).

**When you need the symbol table**, run `nm` against the downloaded library —
ground truth for which `hegel_*` symbols the release exports:

```bash
nm -D build/libhegel/libhegel_c.so | grep ' T hegel_' | sort    # Linux
nm -gU build/libhegel/libhegel_c.dylib | grep hegel_ | sort     # macOS
```

## 3. Walk the header diff against the wrapper layer

Categorize each change in the `libhegel/hegel.h` diff:

- **Removed symbol** → the compiler flags the wrapper; delete the wrapper
  declaration (`src/engine.h` or `include/hegel/internal.h`), its
  implementation, and re-route callers.
- **Renamed/retyped symbol** → update declaration and implementation
  together; the compiler lists every caller that needs to follow.
- **New symbol** → wrap it **only if you wire it into the library**. The
  coverage gate (§4) requires every line in `src/` and `include/hegel/` to be
  covered, so a wrapper nothing calls fails `just check`. If the new function
  is not needed yet, note it in the commit message and move on.
- **Changed signature** (a new arg, or a value moving between return and
  out-param) → remember the convention: ctx first, result-code return,
  produced value through a trailing out-param. A new **optional** parameter
  with a behavior-preserving default (a NULL callback, a
  nullable pointer) is passed as `nullptr` **at the `hegel::impl` call
  site**, with a comment saying so — the existing precedent is the
  `hegel_output_callback_t` + `user_data` pair of `hegel_run_start` /
  `hegel_test_case_from_blob`, absorbed as `nullptr, nullptr` in
  `src/engine.cpp` (engine output stays on stderr). A **required** new
  parameter must instead be plumbed through the wrapper's signature to a
  real caller decision.
- **New or renumbered enum values** — the two places the build does not
  fully check:
  - `SpanLabel` (`include/hegel/internal.h`) mirrors `hegel_label_t` by
    value and is `static_assert`ed against the C constants in
    `src/engine.cpp` (as is `state_machine_done` ==
    `HEGEL_STATE_MACHINE_DONE`). A renumbering trips the asserts; a **new**
    `HEGEL_LABEL_*` you start using needs both the enum entry and a new
    `static_assert`.
  - The `Settings` mapping switches in `src/hegel.cpp` translate the public
    enums (`Verbosity`, `Phase`, `Mode`, `Backend`, `HealthCheck` in
    `include/hegel/settings.h`) to `HEGEL_*` constants by explicit `switch`
    / mask-building. A new C constant only reaches users if you extend the
    public enum and its switch — that is a deliberate feature decision, not
    part of a minimal alignment; flag it rather than doing it silently.
- **Changed struct layout** (`hegel_date_t`, `hegel_time_t`,
  `hegel_datetime_t`, the `hegel_generate_*_result_t` buffer structs) → the
  C++ code uses these C types directly, so the compiler adapts, but check
  the brace-initializers in `src/engine.cpp` (`draw_date`'s
  `hegel_date_t{1, 1, 1}` bounds, the `{nullptr, 0}` guard initializers) —
  positional initializers follow field order silently if arity still
  matches.
- **Ownership/contract changes** in the header comments (a result becoming
  caller-owned, a new `*_free`) → give the result a RAII owner per the
  ownership section above.

## 4. Repo pitfalls

- **Comments**: ASD-STE100 Simplified Technical English; never describe what
  the code used to do; never mention Hypothesis or other Hegel libraries
  (see `.claude/CLAUDE.md`).
- **Formatting**: run `just format` before committing — `check-format` is
  part of `just check` and covers the vendored header too.
- **clang-tidy** (`just check-tidy`) runs over `src/` with
  warnings-as-errors; new wrapper code must pass it.
- **Designated initializers in declaration order** — out-of-order
  designators are a hard error on GCC (Clang tolerates them), so a reorder
  that builds locally on Clang still breaks the GCC CI jobs.
- **Coverage gate** (`just check-coverage`, `scripts/check-coverage.py`):
  every non-excluded line in `src/` and `include/hegel/` must be covered,
  and `// GCOVR_EXCL_*` markers must not exceed the ratchet in
  `.github/coverage-ratchet.json` (it auto-tightens when the count drops;
  only a human raises it). Prefer driving the new path from a test in
  `tests/` over excluding it.
- **Engine behavior changes** can shift the approval snapshots under
  `tests/approvals/` and the `find_quality`/`shrink_quality` expectations.
  Review each shifted snapshot — then accept with
  `APPROVAL_TESTS_USE_REPORTER=AutoApproveReporter` on the failing test
  binary. A quality-test regression is information about the new engine, not
  noise; loosen an expectation only with a comment saying which release
  moved it.
- **References for unclear semantics**: the implementation is
  `hegel-c/src/` in hegel-rust **at the release tag** (ownership, error
  codes, sentinel values), and hegel-go's `internal/libhegel/` is a second,
  complete binding of the same ABI to sanity-check your reading against.

## 5. Verify

```bash
just check    # check-lint (clang-format + clang-tidy) + check-tests + check-docs + check-coverage
```

This is the done-condition — it is exactly the required CI checks. When the
alignment touched handle ownership or threading, also run:

```bash
just check-sanitizers
```

Remember §2: both must run against a **fresh build directory** so the new
pin is actually what gets tested.

## 6. Validation gate — independent completeness audit

The steps above are done by the same context that made the edits, so they
share its blind spots: a contract change you never noticed in the header is
one you also won't notice is unhandled. Close that gap with a
**fresh-context audit** as the final gate. Launch a separate agent (Task
tool, `subagent_type: "general-purpose"`) that has *not* seen your edits and
whose only job is to check the wrapper layer against the header.

Give the agent a self-contained prompt — it starts with no context:

```
Audit hegel-cpp's libhegel wrapper layer against the C ABI header. Do NOT
edit anything — this is a read-only verification.

1. Read the pinned version from cmake/libhegel.cmake (HEGEL_LIBHEGEL_VERSION).
2. Fetch the matching upstream header:
   curl -sSL https://raw.githubusercontent.com/hegeldev/hegel-rust/v<VERSION>/hegel-c/include/hegel.h
   (note the `v` prefix on the tag) and confirm the vendored libhegel/hegel.h
   declares the same functions with the same signatures (the vendored copy is
   clang-formatted, so compare declarations, not bytes).
3. Extract every `hegel_*` function declared in the header.
4. Cross-check each against the repo (src/engine.{h,cpp},
   include/hegel/internal.h, src/test_case.cpp, src/hegel.cpp): it is either
   (a) wrapped and called, or (b) deliberately unused (nothing in src/ or
   include/ needs it). For every wrapped function, confirm the call goes
   through check_rc or DrawScope::raise_for_rc — flag a call whose result
   code is ignored.
5. For every function whose header comment says the result is caller-owned,
   name the C++ owner that frees it (RAII destructor or guard). A result
   with no owner on some path is an OWNERSHIP-LEAK — flag it.
6. For every parameter the wrapper passes as a hardcoded nullptr/0 constant,
   check a comment at the call site explains the absorption. An undocumented
   absorbed parameter is a BURIED-DEFAULT — flag it.
7. Check the mirrored constants: every SpanLabel enumerator in
   include/hegel/internal.h has a static_assert against its HEGEL_LABEL_*
   value in src/engine.cpp, state_machine_done matches
   HEGEL_STATE_MACHINE_DONE, and the Settings switches in src/hegel.cpp
   cover every enumerator of the public enums they translate.

Report a table of every header function with OK / MISSING / UNCHECKED-RC /
OWNERSHIP-LEAK / BURIED-DEFAULT / UNUSED, and a final verdict line. List
discrepancies explicitly; do not fix them.
```

The agent's report is the gate: if it comes back clean, the alignment is
complete. If it flags a discrepancy, return to §3–§4, fix it, and re-run
this gate. This is a genuine independent check only because the agent
rederives the header→wrapper mapping from scratch — do not paste your own
diff or conclusions into its prompt.
