test:
    cmake -B build
    cmake --build build
    ctest --test-dir build/tests --output-on-failure -j{{ num_cpus() }}

check:
    cmake -B build
    cmake --build build
    ctest --test-dir build/tests --output-on-failure -j{{ num_cpus() }}
    just format-check

docs:
    cmake -B build -DHEGEL_BUILD_DOCS=ON
    cmake --build build --target docs
    open build/docs/html/index.html

format:
    find . \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) ! -path "./build/*" | xargs clang-format -i

format-check:
    find . \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) ! -path "./build/*" | xargs clang-format --dry-run -Werror
