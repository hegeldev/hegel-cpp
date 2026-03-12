setup:
    @echo "hegel is auto-installed at runtime into .hegel/venv"
    @echo "To override, set HEGEL_CMD to the path of your hegel binary."

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
    uv run --with "hegel @ git+ssh://git@github.com/antithesishq/hegel-core.git" \
        --with pytest --with pytest-subtests --with hypothesis \
        pytest tests/conformance/test_conformance.py --durations=20 --durations-min=1.0

docs:
    cmake -B build -DHEGEL_BUILD_DOCS=ON
    cmake --build build --target docs
    open build/docs/html/index.html

format:
    find . \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) ! -path "./build/*" | xargs uvx clang-format -i

format-check:
    find . \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) ! -path "./build/*" | xargs uvx clang-format --dry-run -Werror

update-hegel-core-version:
    #!/usr/bin/env bash
    set -euo pipefail
    tag=$(gh api repos/antithesishq/hegel-core/releases/latest --jq '.tag_name')
    sed -i '' "s/static const std::string HEGEL_VERSION = \".*\"/static const std::string HEGEL_VERSION = \"${tag}\"/" src/hegel.cpp
    echo "Updated HEGEL_VERSION to ${tag}"
    # Clear cached install so the next test run picks up the new version
    rm -rf .hegel/venv
