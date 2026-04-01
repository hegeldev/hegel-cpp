setup:
    #!/usr/bin/env bash
    set -euo pipefail
    if [ -n "${HEGEL_BINARY:-}" ]; then
        mkdir -p "$HOME/.local/bin"
        ln -sf "$HEGEL_BINARY" "$HOME/.local/bin/hegel"
    else
        uv tool install --from hegel-core hegel
    fi

build:
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j{{ num_cpus() }}

test:
    cmake -B build
    cmake --build build -j{{ num_cpus() }}
    ctest --test-dir build/tests --output-on-failure -j{{ num_cpus() }}

tidy:
    cmake -B build
    cmake --build build -j{{ num_cpus() }}
    find src -name '*.cpp' | xargs -P{{ num_cpus() }} -I{} clang-tidy -p build {}

check-tidy:
    cmake -B build
    cmake --build build -j{{ num_cpus() }}
    find src -name '*.cpp' | xargs -P{{ num_cpus() }} -I{} clang-tidy -p build -warnings-as-errors='*' {}

lint: format-check check-tidy

check:
    cmake -B build
    cmake --build build -j{{ num_cpus() }}
    ctest --test-dir build/tests --output-on-failure -j{{ num_cpus() }}
    just format-check

build-conformance: build

conformance: build-conformance
    uv run --with "hegel-core" \
        --with pytest --with hypothesis \
        pytest tests/conformance/test_conformance.py --durations=20 --durations-min=1.0

docs:
    cmake -B build -DHEGEL_BUILD_DOCS=ON
    cmake --build build --target docs
    open build/docs/html/index.html

format:
    find . \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) ! -path "./build/*" | xargs uvx clang-format -i

format-check:
    find . \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) ! -path "./build/*" | xargs uvx clang-format --dry-run -Werror
