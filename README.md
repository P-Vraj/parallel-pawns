# Parallel Pawns

### Usage
First, configure the project using CMake (defaults to a release build, with debug optional):
```bash
cmake -S . -B build [-DCMAKE_BUILD_TYPE=Debug]
```
To then build and run the engine, format the code, or run a static analysis, use:
```bash
cmake --build build --target run -- -j  # Run engine
cmake --build build --target format     # Format with clang-format
cmake --build build --target tidy       # Run clang-tidy
```