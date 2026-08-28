---
name: changelog
description: "Changelog style guide for writing RELEASE.md files. Use when creating or reviewing RELEASE.md, writing changelog entries, or preparing a PR that needs release notes."
---

# Changelog Style Guide

This guide describes the style for writing `RELEASE.md` files for hegel-cpp. The style is modeled on the [Hypothesis changelog](https://hypothesis.readthedocs.io/en/latest/changes.html).

## Choosing `RELEASE_TYPE`

hegel-cpp is currently zerover (`0.x.y`), so the usual semver mapping does **not** apply. While we are pre-1.0:

- **`patch`** — Bug fixes, internal changes, **and new features / non-breaking API additions**. The default choice.
- **`minor`** — **Breaking changes only.** Any change that requires users to update their code (renamed/removed APIs, changed signatures, behavior changes that could break downstream tests) is a minor bump.
- **`major`** — Not used while we are zerover. Reserve for the eventual 1.0 and beyond.

If you find yourself reaching for `minor` because the change feels "big," check whether it actually breaks any caller. A large new feature that adds API surface without removing or changing existing behavior is still a `patch`.

## Opening sentence pattern

Every entry should open with a sentence that signals the scope and nature of the change:

- **Patch (fixes, improvements, new features):** Start with `"This patch ..."`
- **Minor (breaking changes):** Start with `"This release ..."` and explain migration
- **Tiny internal-only changes:** A bare sentence is fine — `"Internal refactoring."` or `"Clean up some internal code."`

The opening verb should tell the reader what *kind* of change this is:

| Change type | RELEASE_TYPE | Opening pattern |
|---|---|---|
| Bug fix | `patch` | `"This patch fixes ..."` or `"Fix ..."` |
| New feature | `patch` | `"This patch adds ..."` |
| Improvement | `patch` | `"This patch improves ..."` |
| Performance | `patch` | `"This patch improves the performance of ..."` or `"Optimize ..."` |
| Deprecation | `minor` | `"This release deprecates ..."` |
| Breaking change | `minor` | `"This release changes ..."` (then explain migration) |
| Internal-only | `patch` | `"Internal refactoring."` / `"Refactor some internals."` / `"Clean up some internal code."` |

## Describe the user impact, not the implementation

Bad: "Cache the `hegel_string_generator_t` handle instead of rebuilding it on every draw."

Good: "This patch improves the performance of the string-family generators. `text()`, `from_regex()`, and `emails()` now build their character tables once at generator construction rather than on every draw, which should speed up tests that generate many strings."

Bad: "This patch fixes a bug in integer generation."

Good: "This patch fixes `integers<uint64_t>()` producing out-of-range values when `min_value` was above `INT64_MAX`. Ranges that don't fit in `int64_t` are now bounded correctly."

## Length calibration

- **Internal-only changes:** 1 sentence. (`"Refactor some internals."`)
- **Simple bug fixes:** 1-3 sentences. Describe the bug and what changed.
- **New features:** 1-2 short paragraphs. Describe what it does and why it's useful.
- **Breaking changes / API changes:** Multiple paragraphs. Include before/after code examples and migration guidance.

Don't pad entries. If a change can be described in one sentence, use one sentence.

## Code examples

Include fenced code blocks for:
- New API features (show usage)
- Breaking changes (show before/after)
- Anything where seeing the code is clearer than describing it

Don't include code blocks for bug fixes or internal changes.

## References

- Reference GitHub issues when relevant: `([#123](https://github.com/hegeldev/hegel-cpp/issues/123))`
- Reference previous versions when building on prior work
- Reference related libraries/specs when relevant

## Tone

- Third person, present tense for describing behavior
- Professional but conversational — be direct, not formal
- Honest about uncertainty: `"This should improve performance"`, `"We expect this to..."`, `"In some cases this may..."`
- It's okay to briefly explain *why* a change was made if the motivation isn't obvious

## Things to avoid

- No emojis
- No bullet lists for single-topic entries (use them for multi-topic entries like API cleanups)
- No commit hashes or PR numbers in the text (issue numbers are fine)
- Don't describe the implementation when you can describe the effect
- Don't use vague language like `"various improvements"` — be specific about what changed
- Don't add marketing language or hype

## Examples

**Good patch (bug fix):**

```
RELEASE_TYPE: patch

This patch fixes `text()` ignoring `exclude_characters` when `alphabet` was also provided. Excluded characters are now removed from the alphabet before generation.
```

**Good patch (internal):**

```
RELEASE_TYPE: patch

Internal refactoring of the engine wrapper code.
```

**Good patch (new feature):**

```
RELEASE_TYPE: patch

This patch adds `Settings::report_multiple_failures`. When enabled, `hegel::test()` keeps generating after the first failure to surface additional distinct bugs, and reports all of them at the end of the run, instead of stopping at the first failing example.

This is useful for tests that check several properties at once, where fixing one bug at a time would otherwise require repeated runs.
```

**Good minor (breaking change):**

````
RELEASE_TYPE: minor

This release changes `vectors()`, `sets()`, and `maps()` to take a designated-initializer params struct instead of positional size arguments, matching the other generators.

Before:

```cpp
auto g = vectors(integers<int>(), 1, 10);
```

After:

```cpp
auto g = vectors(integers<int>(), {.min_size = 1, .max_size = 10});
```

To migrate, replace the positional `min`/`max` arguments with a `VectorsParams` (or `SetsParams` / `MapsParams`) initializer as shown above.
````
