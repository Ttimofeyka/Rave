# AGENTS.md

High-signal context for working in this repository.

## Build

```bash
make                    # Build compiler (requires LLVM 14-21, C++17)
make test               # Build and run unit tests
make clean              # Remove obj/ directory
make cleanStd           # Remove cached std/*.o and std/*.ll files
```

**LLVM version**: Auto-detects in order: 18 → 17 → 16 → 15 → default. Override with `LLVM_VERSION=18`.

**Variables**: `FLAGS` (default: `-O3`), `COMPILER`, `LLVM_STATIC=1` for static linking.

## Tests

Unit tests use a custom macro framework in `tests/unit/test.hpp`:
```cpp
TEST("name") EXPECT_TRUE(condition);
TEST("name") EXPECT_EQ(a, b);
TEST("name") EXPECT_NE(a, b);
TEST("name") EXPECT_NOT_NULL(ptr);
```

Test executables: `./tests/test_nodes`, `./tests/test_parser`

**Writing new tests**: Must define `std::string exePath = "./";` before calling parser functions.

## Running Rave Programs

```bash
./rave source.rave -o output           # Basic compilation
./rave source.rave -o output -Ofast --recompileStd  # Max performance
./rave source.rave -o output.exe -t i686-win32      # Cross-compile for Windows
```

## Key Conventions

- **Import syntax**: `import <std/io>` (angle brackets, not quotes)
- **Prelude**: `std/prelude` and `std/memory` auto-import unless `--noPrelude`
- **Stdlib cache**: Standard library compiles to `std/<module>.<platform>.o`. Use `--recompileStd` after changing optimization flags or stdlib code.
- **Cross-compilation**: Set compiler in `options.json` (e.g., `"compiler": "i686-w64-mingw32-gcc-win32"`) and pass `-t <target>`.
- **Return syntax**: `return` is a variable, not a keyword. Use `return = value` to set the return value. For immediate return, use `@return(value)` or `@return()` to return without changing the return value.

## Types

**Basic types**: `bool`, `char`/`uchar` (8-bit), `short`/`ushort` (16-bit), `int`/`uint` (32-bit), `long`/`ulong` (64-bit), `cent`/`ucent` (128-bit), `half` (16-bit float), `bhalf` (bfloat16), `float` (32-bit), `double` (64-bit), `real` (128-bit float), `isize`/`usize` (pointer-sized), `alias` (variables only).

**Pointers**: `char*`, `bool**`, etc. **Arrays**: `int[10]`, `char*[512]`. **Function pointers**: `void()`, `int(int, int)`.

**SIMD vectors**: `short8`, `int4`, `int8`, `float2`, `float4`, `float8`, `double2`, `double4`, `mask16`, `mask32`.

**Type aliases**: `type intptr = int*;`

## Builtins (Compile-Time)

**Type introspection**: `@sizeOf(type)`, `@baseType(type)`, `@typeToString(type)`, `@isNumeric(type)`, `@isStructure(type)`, `@isPointer(type)`, `@isVector(type)`, `@isArray(type)`, `@isFloat(type)`, `@isUnsigned(type)`.

**Value limits**: `@minOf(type)`, `@maxOf(type)`.

**Diagnostics**: `@error(msg)`, `@warning(msg)`, `@echo(msg)`. **String**: `@contains(str1, str2)`.

**Compile-time args** (functions with `(ctargs)`): `@getCurrArg(type)`, `@getArg(n, type)`, `@getCurrArgType()`, `@getArgType(n)`, `@skipArg()`, `@argsLength()`, `@foreachArgs()`.

**Function introspection**: `@hasMethod(type, name)`, `@hasDestructor(type)`, `@getLinkName(func)`, `@callWithArgs(func, args)`, `@callWithBeforeArgs(func, args)`.

**Other**: `@alloca(n)`, `@compileAndLink(files)`, `@addLibrary(name)`, `@aliasExists(name)`, `@return(value)`, `@fmodf(val, div)`, `@atomicTAS(ptr, val)`, `@atomicClear(ptr)`, `@ctlz(val, isZeroPoison)`, `@cttz(val, isZeroPoison)`.

## Control Flow

**Conditionals**: `if (cond) { } else { }`, `if likely(cond)`, `if unlikely(cond)`.

**Loops**: `while (cond) { }`, `for (init; cond; step) { }`, `for (;;)`, `foreach (elem; arr) { }`, `foreach (elem; data; length) { }`. Control: `break`, `continue`.

**Switch**: `switch (val) { case(expr) { } default { } }` - no break/continue needed.

**Defer**: `defer { }` (runs at block end), `fdefer { }` (runs at function return).

**Cast**: `cast(type)expr` (conversion), `bitcast(type)expr` (reinterpret bits).

## Operators

**Arithmetic**: `+`, `-`, `*`, `/`, `%`, `+=`, `-=`, `*=`, `/=`.

**Comparison**: `==`, `!=`, `<`, `>`, `<=`, `>=`, `&&`, `||`.

**Bitwise**: `&` (AND), `|` (OR), `!!` (XOR), `<.` (left shift), `>.` (right shift).

**Membership**: `in`, `!in` - check element presence in array or overloaded structure.

**Address-of**: `&var` - get pointer to variable/function/member.

## Declarations

**Functions**: `int sum(int a, int b) => a + b;`. Parentheses optional if no args. Overloading supported.

**Variables**: `int a = 0;`, `char* b = "hello";`. Function pointers: `int(int) f = func;`.

**Structs**: Have constructors (`Type this { }`), destructors (`void ~this { }`), methods, operator overloading (`operator=(Type* ptr, args)`).

**Namespaces**: `namespace Name { }`. Access via `Name::member`.

**Aliases**: `alias name = value;` - compile-time constant substitution.

## Declaration Parameters

`(C)` - no mangling. `(linkname: "name")` - custom linker name. `(vararg)` - variadic. `(volatile)` - external modification. `(fastcc)`/`(coldcc)` - calling conventions. `(cdecl64)` - 64-bit cdecl. `(nochecks)` - disable runtime checks. `(private)` - not importable. `(noOptimize)` - skip optimization. `(inline)` - force inlining. `(align: n)` - alignment. `(arrayable)` - array literal support. `(noOperators)` - disable operator overloading. `(data: "field")`/`(length: "field")` - foreach support. `(ctargs)` - compile-time arguments.

## Platform Constants

**Platform**: `__RAVE_PLATFORM` (X86, X86_64, ARM, AARCH64, etc.), `__RAVE_OS` (LINUX, WINDOWS, MACOS, FREEBSD).

**Endianness**: `__RAVE_LITTLE_ENDIAN`, `__RAVE_BIG_ENDIAN`.

**x86 features**: `__RAVE_SSE`, `__RAVE_SSE2`, `__RAVE_SSE3`, `__RAVE_SSSE3`, `__RAVE_SSE4_1`, `__RAVE_SSE4_2`, `__RAVE_AVX`, `__RAVE_AVX2`, `__RAVE_AVX512`, `__RAVE_FMA`, `__RAVE_POPCNT`.

**ARM features**: `__RAVE_ASIMD`, `__RAVE_SVE`, `__RAVE_SVE2`.

**Compile settings**: `__RAVE_OPTIMIZATION_LEVEL`, `__RAVE_RUNTIME_CHECKS`.

**Usage**: `@if (__RAVE_PLATFORM == "X86_64") { ... }`.

## Architecture

```
src/
├── main.cpp        # CLI, arg parsing
├── compiler.cpp    # Orchestration, linking
├── llvm.cpp        # LLVM IR generation
├── lexer/          # Tokenization
└── parser/         # AST, type checking
    └── nodes/      # 47 AST node types (Node*.cpp)
```

Pipeline: Lexer → Parser (AST + type check) → LLVM IR → Object files → Link

## CI

GitHub Actions runs `make` on push/PR to main. No additional lint/test steps in CI.

## References

- `specifications/` - Language specification documents