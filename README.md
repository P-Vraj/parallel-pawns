# Parallel Pawns

### Run Command
```bash
clang++ -std=c++20 -O3 -march=native -Iinclude src/engine.cpp src/move_gen_tables.cpp -fconstexpr-steps=20000 -o engine
```

Note: `-fconstexpr-steps` can be set up to 2000000 (for more constexpr work to be done)