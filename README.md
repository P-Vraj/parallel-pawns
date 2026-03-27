# Parallel Pawns

## Usage

### Requirements
* Unix (preferably Linux) environment
* C++23
* Clang
* CMake (v3.20+)
* Ccache
* clang-format (optional)
* clang-tidy (optional)

### Build Instructions
First, configure the project using CMake (defaults to a "Release" build, with "Debug" optional) and build it with:
```bash
cmake -S . -B build [-DCMAKE_BUILD_TYPE=Debug]
cmake --build build -j
```

#### Engine
To build and run the engine, use:
```bash
cmake --build build --target run
```

#### Formatter/Linter
To format the code or run a static analysis, use:
```bash
cmake --build build --target format # Format with clang-format
cmake --build build --target tidy   # Run clang-tidy
clang-tidy -p build <FILE>          # Run clang-tidy on a single file
```

#### Tests
To run tests, use:
```bash
ctest --test-dir build --output-on-failure -j
```
Or to run a single test, such as "Perft", use:
```bash
ctest --test-dir build -R "Perft" --output-on-failure
```

You can find tool-specific build instructions within the files in `tools/`.