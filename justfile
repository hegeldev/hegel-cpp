test:
    cmake -B build
    cmake --build build
    ctest --test-dir build --output-on-failure

format:
    find . -name "*.cpp" -o -name "*.hpp" | grep -v build | xargs clang-format -i
