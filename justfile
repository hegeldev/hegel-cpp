set ignore-comments := true

jobs := num_cpus()

build:
    cmake -B build ${CMAKE_FLAGS:-}
    cmake --build build -j{{ jobs }}

check-tests: build
    ctest --test-dir build/tests --output-on-failure -j{{ jobs }}

format:
    find . \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) ! -path "./build/*" \
        | xargs uvx clang-format -i

check-format:
    find . \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) ! -path "./build/*" \
        | xargs uvx clang-format --dry-run -Werror

check-tidy: build
    find src -name '*.cpp' \
        | xargs -P{{ jobs }} -I{} clang-tidy -p build -warnings-as-errors='*' {}

check-docs:
    cmake -B build -DHEGEL_BUILD_DOCS=ON ${CMAKE_FLAGS:-}
    cmake --build build --target docs

docs:
    cmake -B build -DHEGEL_BUILD_DOCS=ON ${CMAKE_FLAGS:-}
    cmake --build build --target docs
    open build/docs/html/index.html

# MODE is one of: subdirectory, fetchcontent, install, tests_on.
check-consumer MODE="subdirectory":
    #!/usr/bin/env bash
    set -euo pipefail
    ROOT=$(pwd)
    BUILD_DIR="$ROOT/build/consumer-{{ MODE }}"
    if [ "{{ MODE }}" = "install" ]; then
        cmake -B build/consumer-hegel-install \
            -DHEGEL_BUILD_TESTS=OFF -DHEGEL_BUILD_CONFORMANCE=OFF
        cmake --build build/consumer-hegel-install -j{{ jobs }}
        cmake --install build/consumer-hegel-install \
            --prefix "$ROOT/build/consumer-hegel-prefix"
        PREFIX_ARG=-DCMAKE_PREFIX_PATH="$ROOT/build/consumer-hegel-prefix"
    else
        PREFIX_ARG=""
    fi
    cmake -B "$BUILD_DIR" -S "tests/consumer/{{ MODE }}" \
        -DHEGEL_ROOT="$ROOT" -DHEGEL_REF=HEAD $PREFIX_ARG
    cmake --build "$BUILD_DIR" -j{{ jobs }}
    "$BUILD_DIR/consumer"

consumer MODE="subdirectory": (check-consumer MODE)

# Run every consumer mode in parallel, re-printing failure output for clarity
check-consumer-all:
    #!/usr/bin/env bash
    set -uo pipefail
    modes=(subdirectory fetchcontent install tests_on)
    mkdir -p build/consumer-logs
    pids=()
    for mode in "${modes[@]}"; do
        just check-consumer "$mode" >"build/consumer-logs/$mode.log" 2>&1 &
        pids+=($!)
    done
    rc=0
    for i in "${!pids[@]}"; do
        mode="${modes[$i]}"
        if wait "${pids[$i]}"; then
            echo "consumer ($mode): passed"
        else
            rc=1
            echo "::group::consumer ($mode) FAILED"
            cat "build/consumer-logs/$mode.log"
            echo "::endgroup::"
        fi
    done
    exit $rc

check-conformance: build
    uv run --with hegel-core \
        --with pytest --with hypothesis \
        pytest tests/conformance/test_conformance.py --durations=20 --durations-min=1.0

check-lint: check-format check-tidy

# these aliases are provided as ux improvements for local developers. CI should use the longer
# forms.
test: check-tests
tidy: check-tidy
lint: check-lint
conformance: check-conformance
check: check-lint check-tests check-docs check-conformance
