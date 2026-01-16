test:
    cmake -B build
    cmake --build build
    ctest --test-dir build --output-on-failure -j{{ num_cpus() }}

docs:
    cmake -B build -DHEGEL_BUILD_DOCS=ON
    cmake --build build --target docs
    open build/docs/html/index.html

format:
    find . -name "*.cpp" -o -name "*.hpp" | grep -v build | xargs clang-format -i
