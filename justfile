setup:
    #!/usr/bin/env bash
    set -euo pipefail
    if [ ! -d .venv ]; then
        uv venv .venv
    fi
    if [ -n "${HEGEL_BINARY:-}" ]; then
        ln -sf "$HEGEL_BINARY" .venv/bin/hegel
    else
        uv pip install --python .venv/bin/python "hegel @ git+ssh://git@github.com/antithesishq/hegel.git"
    fi

build:
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build

test:
    cmake -B build
    cmake --build build
    ctest --test-dir build/tests --output-on-failure -j{{ num_cpus() }}

lint: format-check
    @echo "Lint checks passed (format-check)"

check:
    cmake -B build
    cmake --build build
    ctest --test-dir build/tests --output-on-failure -j{{ num_cpus() }}
    just format-check

build-conformance: build
    @echo "Conformance tests built as part of main build"

conformance: build-conformance
    #!/usr/bin/env bash
    set -euo pipefail
    PATH=".venv/bin:$PATH" pytest tests/conformance/test_conformance.py --durations=20 --durations-min=1.0

docs:
    cmake -B build -DHEGEL_BUILD_DOCS=ON
    cmake --build build --target docs
    open build/docs/html/index.html

format:
    find . \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) ! -path "./build/*" | xargs uvx clang-format -i

format-check:
    find . \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) ! -path "./build/*" | xargs uvx clang-format --dry-run -Werror
