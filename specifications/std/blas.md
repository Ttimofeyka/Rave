# std/blas

SIMD-optimized Basic Linear Algebra Subprograms (BLAS) implementation.

## Import

```d
import <std/blas>
```

## Notes

- Experimental implementation with 16-byte alignment requirement
- Automatically uses SIMD instructions (SSE, AVX) when available
- Falls back to scalar operations on unsupported platforms

## Vector Operations

### abs
```d
void abs<T>(usize n, T* x)
```
Computes absolute value of vector elements in-place.

### sqrt
```d
void sqrt<T>(usize n, T* x)
```
Computes square root of vector elements in-place.

### asum
```d
T asum<T>(usize n, T* x)
```
Returns sum of absolute values: Σ|x[i]|

### axpy
```d
void axpy<T>(usize n, T alpha, T* x, T* y)
```
Vector addition: y[i] += alpha * x[i]

### axpby
```d
void axpby<T>(usize n, T alpha, T* x, T beta, T* y)
```
Scaled vector addition: y[i] = alpha * x[i] + beta * y[i]

### dot
```d
T dot<T>(usize n, T* x, T* y)
```
Dot product: Σ(x[i] * y[i])

### scale
```d
void scale<T>(usize n, T* x, T alpha)
```
Scalar multiplication: x[i] *= alpha

### norm
```d
T norm<T>(usize n, T* x)
```
Euclidean norm: sqrt(Σ(x[i]²))

### max
```d
usize max<T>(usize n, T* x)
```
Returns index of maximum element.

### min
```d
usize min<T>(usize n, T* x)
```
Returns index of minimum element.

## Matrix Operations

### transpose
```d
void transpose<T>(usize rows, usize columns, T* matrix, T* target)
```
Matrix transpose with SIMD optimization.

### matmul
```d
void matmul<T>(usize rows, usize columns, usize inners, T* mat1, T* mat2, T* result)
```
Matrix multiplication: result = mat1 × mat2

### vecmatmul
```d
void vecmatmul<T>(usize rows, usize columns, T* vector, T* matrix, T* result)
```
Vector-matrix multiplication.

## Example

```d
import <std/blas>

float[4] vec1 = [1.0, 2.0, 3.0, 4.0];
float[4] vec2 = [5.0, 6.0, 7.0, 8.0];

float dotProduct = std::blas::dot<float>(4, &vec1, &vec2);
std::blas::scale<float>(4, &vec1, 2.0);
std::blas::axpy<float>(4, 1.5, &vec1, &vec2);
```
