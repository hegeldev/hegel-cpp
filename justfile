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
            -DHEGEL_BUILD_TESTS=OFF
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

# Verify the library, headers, and a consumer build and run under C++17
# (HEGEL_REFLECTION=OFF drops reflect-cpp / default_generator).
check-cxx17:
    #!/usr/bin/env bash
    set -euo pipefail
    ROOT=$(pwd)
    cmake -B build/cxx17-hegel -DHEGEL_REFLECTION=OFF -DHEGEL_BUILD_TESTS=OFF
    cmake --build build/cxx17-hegel -j{{ jobs }}
    cmake --install build/cxx17-hegel --prefix "$ROOT/build/cxx17-prefix"
    cmake -B build/cxx17-consumer -S tests/consumer/cxx17 \
        -DCMAKE_PREFIX_PATH="$ROOT/build/cxx17-prefix"
    cmake --build build/cxx17-consumer -j{{ jobs }}
    "$ROOT/build/cxx17-consumer/consumer"

check-coverage:
    #!/usr/bin/env bash
    set -euo pipefail
    cmake -B build/coverage -DHEGEL_COVERAGE=ON ${CMAKE_FLAGS:-}
    cmake --build build/coverage -j{{ jobs }}

    # LLVM source-based coverage needs llvm-profdata/llvm-cov matching the
    # clang that built the instrumented binaries. On Linux these are versioned
    # (llvm-cov-18); derive the suffix from CXX (e.g. clang++-18 -> -18).
    cxx="${CXX:-c++}"
    suffix=""
    case "$cxx" in *clang++-*) suffix="-${cxx##*clang++-}";; esac
    llvm_profdata="$(command -v "llvm-profdata$suffix" llvm-profdata 2>/dev/null | head -1 || true)"
    llvm_cov="$(command -v "llvm-cov$suffix" llvm-cov 2>/dev/null | head -1 || true)"
    if { [ -z "$llvm_profdata" ] || [ -z "$llvm_cov" ]; } && command -v xcrun >/dev/null 2>&1; then
        llvm_profdata="${llvm_profdata:-$(xcrun --find llvm-profdata)}"
        llvm_cov="${llvm_cov:-$(xcrun --find llvm-cov)}"
    fi
    if [ -z "$llvm_profdata" ] || [ -z "$llvm_cov" ]; then
        echo "error: llvm-profdata/llvm-cov not found (need clang + llvm)" >&2
        exit 1
    fi

    prof_dir="$PWD/build/coverage/profraw"
    rm -rf "$prof_dir"; mkdir -p "$prof_dir"
    export LLVM_PROFILE_FILE="$prof_dir/cov-%p-%m.profraw"
    ctest --test-dir build/coverage/tests --output-on-failure -j{{ jobs }}

    # Collect the instrumented test executables. `file ... executable` matches
    # both Mach-O and ELF (PIE) executables and excludes shared libraries,
    # without relying on a non-portable `find -perm` mode.
    objs=(); while IFS= read -r b; do objs+=("$b"); done < <(
        find build/coverage/tests -type f -exec sh -c \
            'file -b "$1" | grep -q executable' _ {} \; -print)
    if [ "${#objs[@]}" -eq 0 ]; then
        echo "error: no instrumented test binaries found" >&2; exit 1
    fi
    # Export each object against only its own profile data. Profiles merged
    # across binaries make llvm-cov report benign hash differences between
    # binaries as "N functions have mismatched data" (and drop those counts).
    # The %m component of each profraw filename (cov-<pid>-<%m>.profraw) is
    # the emitting binary's instrumentation signature; a short probe run
    # recovers each binary's signature to pair it with its group of profiles.
    # The checker maxes counts per line across the concatenated traces.
    lcov_dir="$PWD/build/coverage/lcov"
    probe_dir="$PWD/build/coverage/probe"
    rm -rf "$lcov_dir"; mkdir -p "$lcov_dir"
    for o in "${objs[@]}"; do
        rm -rf "$probe_dir"; mkdir -p "$probe_dir"
        # gtest binaries list tests and exit; non-gtest ones (subject) print
        # usage and exit. Either way the exit path writes a probe profile,
        # which is used only for the signature — never merged into coverage.
        LLVM_PROFILE_FILE="$probe_dir/p-%m.profraw" "$o" --gtest_list_tests \
            > /dev/null 2>&1 || true
        probe=("$probe_dir"/p-*.profraw)
        if [ "${#probe[@]}" -ne 1 ] || [ ! -f "${probe[0]}" ]; then
            echo "error: probe run of $o produced no profile" >&2; exit 1
        fi
        sig=$(basename "${probe[0]}" .profraw); sig=${sig#p-}
        group=("$prof_dir"/cov-*-"$sig".profraw)
        if [ ! -f "${group[0]}" ]; then
            echo "error: no test profiles for $o (signature $sig)" >&2; exit 1
        fi
        name=$(basename "$o")
        "$llvm_profdata" merge -sparse "${group[@]}" \
            -o "$lcov_dir/$name.profdata"
        "$llvm_cov" export -format=lcov \
            -instr-profile="$lcov_dir/$name.profdata" \
            "$o" "$PWD/src" "$PWD/include/hegel" \
            > "$lcov_dir/$name.lcov"
    done
    cat "$lcov_dir"/*.lcov > build/coverage/coverage.lcov
    python3 scripts/check-coverage.py build/coverage/coverage.lcov

check-lint: check-format check-tidy

# these aliases are provided as ux improvements for local developers. CI should use the longer
# forms.
test: check-tests
tidy: check-tidy
lint: check-lint
check: check-lint check-tests check-docs check-coverage
