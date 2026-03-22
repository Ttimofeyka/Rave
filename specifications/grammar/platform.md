# Platform-Specific Constants

Rave provides compile-time constants that allow you to write platform-aware code. These constants are automatically set by the compiler based on the target platform and CPU features.

## Platform and OS Detection

| Constant | Description | Possible Values |
|----------|-------------|-----------------|
| `__RAVE_PLATFORM` | Target CPU architecture | `X86`, `X86_64`, `ARM`, `AARCH64`, `POWERPC64`, `MIPS`, `MIPS64`, `AVR` |
| `__RAVE_OS` | Target operating system | `LINUX`, `WINDOWS`, `MACOS`, `FREEBSD`, `UNKNOWN` |
| `__RAVE_LITTLE_ENDIAN` | True if target is little-endian | `true` / `false` |
| `__RAVE_BIG_ENDIAN` | True if target is big-endian | `true` / `false` |

## Compilation Settings

| Constant | Description |
|----------|-------------|
| `__RAVE_OPTIMIZATION_LEVEL` | Current optimization level (0-3) |
| `__RAVE_RUNTIME_CHECKS` | `true` if runtime checks are enabled, `false` if `--noChecks` was used |

## x86/x86_64 CPU Features

These constants are `true` if the target CPU supports the corresponding feature:

| Constant | Description |
|----------|-------------|
| `__RAVE_SSE` | Streaming SIMD Extensions |
| `__RAVE_SSE2` | Streaming SIMD Extensions 2 |
| `__RAVE_SSE3` | Streaming SIMD Extensions 3 |
| `__RAVE_SSSE3` | Supplemental SSE3 |
| `__RAVE_SSE4_1` | SSE4.1 |
| `__RAVE_SSE4_2` | SSE4.2 |
| `__RAVE_SSE4A` | AMD SSE4a |
| `__RAVE_AVX` | Advanced Vector Extensions |
| `__RAVE_AVX2` | Advanced Vector Extensions 2 |
| `__RAVE_AVX512` | AVX-512 instruction set |
| `__RAVE_POPCNT` | POPCNT instruction |
| `__RAVE_FMA` | Fused Multiply-Add |
| `__RAVE_F16C` | Half-precision floating-point conversion |

## ARM/AARCH64 CPU Features

| Constant | Description |
|----------|-------------|
| `__RAVE_ASIMD` | Advanced SIMD (NEON) |
| `__RAVE_FP_ARMV8` | ARMv8 floating-point |
| `__RAVE_SVE` | Scalable Vector Extension |
| `__RAVE_SVE2` | Scalable Vector Extension 2 |

## Other Features

| Constant | Description |
|----------|-------------|
| `__RAVE_HALF` | Half-precision floating-point support |

## Import Context

| Constant | Description |
|----------|-------------|
| `__RAVE_IMPORTED_FROM` | The file path that is importing the current module (only set during import) |

## Usage Examples

### Platform-Specific Code

```d
@if (__RAVE_PLATFORM == "X86_64") {
    // 64-bit x86 specific code
}
@else @if (__RAVE_PLATFORM == "AARCH64") {
    // ARM64 specific code
}
```

### OS-Specific Code

```d
@if (__RAVE_OS == "LINUX") {
    // Linux-specific code
}
@else @if (__RAVE_OS == "WINDOWS") {
    // Windows-specific code
}
```

### Feature Detection

```d
// Use SIMD only if supported
@if (__RAVE_AVX2) {
    // AVX2 optimized code
}
@else @if (__RAVE_SSE2) {
    // SSE2 fallback
}
@else {
    // Scalar fallback
}

// Check for runtime checks
@if (__RAVE_RUNTIME_CHECKS) {
    if (index >= length) {
        std::println("Index out of bounds!");
    }
}
```

### Endianness Handling

```d
@if (__RAVE_LITTLE_ENDIAN) {
    // Little-endian code
}
@else {
    // Big-endian code
}
```

### Conditional Import

```d
// From std/prelude.rave
@if (@aliasExists(__RAVE_IMPORTED_FROM)) {
    @if (!(@contains(__RAVE_IMPORTED_FROM, "std/sysc.rave"))) import <std/sysc>
}
@if (!@aliasExists(__RAVE_IMPORTED_FROM)) import <std/sysc>
```