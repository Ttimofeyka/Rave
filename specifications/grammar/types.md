# Types

## Basic types

- bool (boolean type)
- char / uchar (8-bit)
- short / ushort (16-bit)
- int / uint (32-bit)
- long / ulong (64-bit)
- cent / ucent (128-bit)
- half (16-bit float)
- bhalf (16-bit float, it is known as **bfloat16**)
- float (32-bit float)
- double (64-bit float)
- real (128-bit float)
- alias (only for variables)

## Pointers

Examples:

- char*
- bool**
- half*
- uint*

## Arrays

Examples:

- char*[512]
- bool[10]
- real[8]
- uint*[2]

## Structures

Examples:

- A
- B
- std::string
- std::vector<int>

## Function pointers

Examples:

- void()
- int(int, int)
- float(std::string, char*)

## SIMD vectors

- short8
- int4
- float4
- int8
- float8

# Creating new types

In fact, these are just "aliases" for existing types. But they can be used for abbreviations.

Examples:

```cpp
type intptr = int*;
type any = void*;
```