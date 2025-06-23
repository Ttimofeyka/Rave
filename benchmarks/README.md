# Benchmarks

It contains benchmarks for comparing the speed between Rave and other programming languages.

Compilation flags are marked with a comment inside each file.

## Results

All tests were performed on Windows 10 (**WSL**), i5-12400f, 32GB DDR4 3200 Mhz.

**gcc** version: 14

**LLVM** version: 18

**Zig** version: 0.14.1

Enabled technologies: **SSE**, **SSE2**

<sub>**NOTE: Since the testing took place through the CLI, we did not have a clear option to turn off the latest technologies (like AVX) when using Zig.**</sub>

### nqueen

| Time | Rave | C (gcc) | C (clang) |
| ---- | ---- | ------- | --------- |
| Best Run | 2.7s | 2.884s | 2.687s |
| Worst Run | 2.86s | 3.048s | 2.785s |
| **Average** | **2.78s** | **2.966s** | <ins>**2.736s**</ins> |

### dirichlet

| Time | Rave | C (gcc) | C (clang) | Zig |
| ---- | ---- | ------- | --------- | ---- |
| Best Run | 1.032s | 1.194s | 1.111s | **1.120s** |
| Worst Run | 1.166s | 1.362s | 1.272s | **1.380s** |
| **Average** | <ins>**1.099s**</ins> | **1.278s** | **1.192s** | **1.250s** |
