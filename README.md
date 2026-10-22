# This repository has moved

Hegel for C++ now lives in the Hegel monorepo at **https://github.com/hegeldev/hegel**.

| What you're looking for | Where it is now |
|---|---|
| The `hegel` CMake library (C++ frontend) | [`frontends/cpp/`](https://github.com/hegeldev/hegel/tree/main/frontends/cpp) |
| libhegel, the engine behind every Hegel frontend | [`libhegel/`](https://github.com/hegeldev/hegel/tree/main/libhegel) |
| Changelog | [`frontends/cpp/CHANGELOG.md`](https://github.com/hegeldev/hegel/blob/main/frontends/cpp/CHANGELOG.md) |
| Bug reports and feature requests | [hegeldev/hegel issues](https://github.com/hegeldev/hegel/issues) |

The documentation is still at [hegel.dev/cpp](https://hegel.dev/cpp). To consume a release with CMake's `FetchContent`, fetch the monorepo at the release's tag and point at the frontend's subdirectory:

```cmake
include(FetchContent)
FetchContent_Declare(
    hegel
    GIT_REPOSITORY https://github.com/hegeldev/hegel.git
    GIT_TAG frontends/cpp/0.13.2
    SOURCE_SUBDIR frontends/cpp
)
FetchContent_MakeAvailable(hegel)
```

The library builds the engine from the fetched sources with cargo, so a Rust toolchain is required; see the frontend's README for the details and for linking a prebuilt libhegel instead.

## Tags and history

The full history of this repository was merged into the monorepo unchanged, so every commit here exists there too. Releases are tagged by path and version in the new repository:

| Here | In hegeldev/hegel |
|---|---|
| `v0.13.1` | `frontends/cpp/0.13.1` |

Releases made before the move remain available under this repository's [releases](https://github.com/hegeldev/hegel-cpp/releases), and the `v*` tags here keep working for existing `FetchContent` pins. New releases, starting with 0.13.2, are published only from hegeldev/hegel.

For general information about Hegel, see https://hegel.dev.
