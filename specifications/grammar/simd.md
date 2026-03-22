# SIMD

Here are described all builtins related to SIMD (SSE/AVX).

**@vLoad(type, dataPtr, [optional]isAligned)** - Load data from a pointer into a vector.

If the 'isAligned' option is specified and the value is false, the alignment is not used (useful if you still getting errors with alignment).

If used incorrectly (without alignment if 'isAligned' != false), it can cause a segmentation fault.

Example:

```d
float[] data = [10f, 20f, 30f, 40f];

float4 vecData = @vLoad(float4, &data);
// Or (without alignment)
float4 vecData = @vLoad(float4, &data, false);
```

**@vStore(vector, dataPtr, [optional]isAligned)** - Store a data from the vector to the pointer.

If the 'isAligned' option is specified and the value is false, the alignment is not used (useful if you still getting errors with alignment).

If used incorrectly (without alignment if 'isAligned' != false), it can cause a segmentation fault.

Example:

```d
float4 vector = @vFrom(float4, 10f);
float[4] data;

@vStore(vector, &data);
// Or (without alignment)
@vStore(vector, &data, false);
```

**@vFrom(type, value)** - Creates a vector of the specified type in which all elements have the passed value.

Example:

```d
int4 vector = @vFrom(int4, 50);
```

**@vShuffle(vector1, vector2, array)** - Returns a shuffled vector with the specified mask.

Example:

```d
float4 example = @vLoad(float4, &[10f, 20f, 30f, 40f], false); // 10f, 20f, 30f, 40f
jam = @vShuffle(jam, jam, [0, 0, 0, 0]); // 10f, 10f, 10f, 10f
```

**@vHAdd32x4, @vHAdd16x8 (vector1, vector2)** - Add the horizontal elements of two vectors side by side, returning the output vector as a result.

**@vHAdd32x4** works for `int4` and `float4`, **@vHAdd16x8** works for `short8`.

Example:

```d
int4 foo = @vFrom(int4, 100); // 100, 100, 100, 100
foo = @vHAdd32x4(foo, foo); // 200, 200, 200, 200
```

**@vSqrt(vector)** - Compute the square root of each element in a vector.

Requires SSE for float4, SSE2 for double2, and AVX for larger vectors.

Example:

```d
float4 values = @vFrom(float4, 16.0f);
float4 roots = @vSqrt(values); // 4.0, 4.0, 4.0, 4.0
```

**@vAbs(vector)** - Compute the absolute value of each element in a vector.

Works with `short8` and `int4`/`int8`. Requires SSSE3 or AVX2 depending on the vector type.

Example:

```d
int4 values = @vFrom(int4, -5);
int4 absolutes = @vAbs(values); // 5, 5, 5, 5
```

**@vMoveMask128(vector)** - Extract a sign bit mask from a 128-bit vector. Returns an integer where each bit represents the sign bit of the corresponding vector element.

Requires SSE2 support.

Example:

```d
float4 values = @vFrom(float4, -1.0f); // All negative
int mask = @vMoveMask128(values); // Sign bits packed into integer
```

**@vMoveMask256(vector)** - Extract a sign bit mask from a 256-bit vector. Returns an integer where each bit represents the sign bit of the corresponding vector element.

Requires AVX2 support.

Example:

```d
float8 values = @vFrom(float8, -1.0f); // All negative
int mask = @vMoveMask256(values); // Sign bits packed into integer
```
