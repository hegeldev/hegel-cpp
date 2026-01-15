test:
    cmake -B build
    cmake --build build
    ctest --test-dir build --output-on-failure -j{{ num_cpus() }}

format:
    find . -name "*.cpp" -o -name "*.hpp" | grep -v build | xargs clang-format -i
