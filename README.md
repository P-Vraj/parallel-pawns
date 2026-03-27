# Parallel Pawns

***Parfait*** is a distributed chess engine, compliant with the UCI standard.

## Usage

### Requirements
* Unix (preferably Linux) environment
* C++23
* Clang (v17+)
* CMake (v3.20+)
* [Ccache](https://github.com/ccache/ccache)
* [Fastchess](https://github.com/Disservin/fastchess) (optional)
* clang-format (optional)
* clang-tidy (optional)

*Note*: Requires a modern CPU and OS that supports lock-free 128-bit atomic operations. Most recent x86_64 and ARM64 systems with up-to-date compilers will meet this requirement.

### Build Instructions
First, configure the project using CMake (defaults to a "Release" build, with "Debug" optional) and build it with:
```bash
cmake -S . -B build [-DCMAKE_BUILD_TYPE=Release|Debug]
cmake --build build -j
```

#### Engine
To build and run the engine, use:
```bash
cmake --build build --target run
```
To interact with ***Parfait*** through the command line, read the [UCI documentation](https://backscattering.de/chess/uci/).

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

#### Miscellaneous
You can find tool-specific build instructions within the files in `tools/`.
You can find script-specific run instructions within the files in `scripts/`.

### Benchmarks
To benchmark engine-vs-engine performance through real games, run the script:
```bash
bash ./scripts/run-fastchess.sh
```
To enable further logging, run:
```bash
WRITE_LOGS=1 bash ./scripts/run-fastchess.sh
```
This utilizes the Fastchess CLI to run games against engines, allowing for benchmarking and statistical analyses. Update the options by passing them through the command line or by modifying the script.

*Note*: Must have [Fastchess](https://github.com/Disservin/fastchess) and an opening book installed. View [`./scripts/run-fastchess.sh`](./scripts/run-fastchess.sh) and [`./data/openings/README.md`](./data/openings/README.md) for more details.