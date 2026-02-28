# Parallel Pawns

### Usage

This project runs on Linux. To build and run, ensure you have:
* C++23
* Clang
* CMake (v3.20+)
* clang-format (optional)
* clang-tidy (optional)
* Ccache

First, configure the project using CMake (defaults to a release build, with debug optional):
```bash
cmake -S . -B build [-DCMAKE_BUILD_TYPE=Debug]
```
To then build and run the engine, format the code, or run a static analysis, use:
```bash
cmake --build build --target run -- -j  # Run engine
cmake --build build --target format     # Format with clang-format
cmake --build build --target tidy       # Run clang-tidy
clang-tidy -p build {FILE}              # Single tidy-file target
```