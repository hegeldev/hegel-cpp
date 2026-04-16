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
