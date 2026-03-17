# Benchmarks

It contains benchmarks for comparing the speed between Rave and other programming languages.

Compilation flags are marked with a comment inside each file.

## Results

All tests were performed on Fedora 43, i5-12400f, 32GB DDR4 3200 Mhz.

**gcc** version: 14

**LLVM** version: 18

Enabled technologies: **SSE**, **SSE2**

### nqueen

| Time | Rave | C (gcc) | C (clang) |
| ---- | ---- | ------- | --------- |
| Best Run | 2.7s | 2.884s | 2.687s |
| Worst Run | 2.86s | 3.048s | 2.785s |
| **Average** | **2.78s** | **2.966s** | <ins>**2.736s**</ins> |

### dirichlet

| Time | Rave | C (gcc) | C (clang) |
| ---- | ---- | ------- | --------- |
| Best Run | **1.032s** | 1.194s | 1.111s |
| Worst Run | 1.166s | **1.362s** | 1.272s |
| **Average** | <ins>**1.099s**</ins> | **1.278s** | **1.192s** |

### mandelbrot

| Time | Rave | C (gcc) | C (clang) |
| ---- | ---- | ------- | --------- |
| Best Run | 9.00s | 9.24s | **8.50s** |
| Worst Run | 9.07s | 9.31s | **8.55s** |
| **Average** | **9.04s** | **9.27s** | <ins>**8.53s**</ins> |

