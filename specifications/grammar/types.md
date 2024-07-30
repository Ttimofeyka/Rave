# Types

## Basic types

- bool
- char
- short
- int
- long
- cent
- alias (only for variables)

## Pointers

Examples:

- char*
- bool**

## Arrays

Examples:

- char*[512]
- bool[10]

## Structures

Examples:

- A
- B
- std::string

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