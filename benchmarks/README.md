# Benchmarks

It contains benchmarks for comparing the speed between Rave and other programming languages.

Compilation flags are marked with a comment inside each file.

## Results

All tests were performed on Windows 10 (**WSL**), i5-12400f, 32GB DDR4 3200 Mhz.

Version of **gcc**: 14

**LLVM** version: 18

### nqueen

| Time | Rave | C (gcc) | C (clang) |
| ---- | ---- | ------- | --------- |
| Best Run | 2.664s | 2.884s | 2.687s |
| Worst Run | 2.774s | 3.048s | 2.785s |
| **Average** | <u>**2.719s**</u> | **2.966s** | **2.736s** |
