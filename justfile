setup:
    #!/usr/bin/env bash
    set -euo pipefail
    if [ -n "${HEGEL_BINARY:-}" ]; then
        mkdir -p "$HOME/.local/bin"
        ln -sf "$HEGEL_BINARY" "$HOME/.local/bin/hegel"
    else
        uv tool install "hegel-core==0.4.3"
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
    uv run --with 'hegel-core==0.4.3' \
        --with pytest --with hypothesis \
        pytest tests/conformance/test_conformance.py --durations=20 --durations-min=1.0

docs:
    cmake -B build -DHEGEL_BUILD_DOCS=ON
    cmake --build build --target docs
    open build/docs/html/index.html

coverage:
    #!/usr/bin/env bash
    set -euo pipefail
    buildlog=$(mktemp /tmp/hegel-cov-build-XXXXXX.log)
    trap 'rm -f "$buildlog"' EXIT
    # Ensure regular build exists so we can reuse downloaded deps
    cmake -B build >> "$buildlog" 2>&1 || { cat "$buildlog"; exit 1; }
    cmake --build build -j{{ num_cpus() }} >> "$buildlog" 2>&1 || { cat "$buildlog"; exit 1; }
    rm -rf build-coverage
    cmake -B build-coverage -DHEGEL_COVERAGE=ON \
        -DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON="$(pwd)/build/_deps/nlohmann_json-src" \
        -DFETCHCONTENT_SOURCE_DIR_REFLECTCPP="$(pwd)/build/_deps/reflectcpp-src" \
        -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$(pwd)/build/_deps/googletest-src" \
        >> "$buildlog" 2>&1 || { cat "$buildlog"; exit 1; }
    cmake --build build-coverage -j2 >> "$buildlog" 2>&1 || { cat "$buildlog"; exit 1; }
    ctest --test-dir build-coverage/tests --output-on-failure -j{{ num_cpus() }} \
        >> "$buildlog" 2>&1 || { cat "$buildlog"; exit 1; }
    uvx gcovr --root . --filter 'src/' build-coverage \
        --txt --fail-under-line 100

format:
    find . \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) ! -path "./build/*" ! -path "./build-*/*" | xargs uvx clang-format -i

format-check:
    find . \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) ! -path "./build/*" ! -path "./build-*/*" | xargs uvx clang-format --dry-run -Werror
