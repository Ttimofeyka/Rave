# std/arg

Variadic argument support for functions with variable parameter lists.

## Types

### va_list

Platform-specific structure for variadic argument handling. Implementation varies by architecture:

- **X86_64:** Contains GP offset, FP offset, overflow area, and register save area
- **X86, PowerPC, MIPS, AVR, WASM:** Single pointer to argument area
- **AArch64:** Stack pointer, register tops, and offsets
- **PowerPC:** Character and short fields with pointers
- **S390X:** Long fields with character pointers

## Functions

### __builtin_va_start

```d
void __builtin_va_start(void* va)
```

Initializes variadic argument list. Must be called before accessing arguments.

### __builtin_va_end

```d
void __builtin_va_end(void* va)
```

Cleans up variadic argument list after use.

### __builtin_va_copy

```d
void __builtin_va_copy(void* dest, void* src)
```

Copies variadic argument list from src to dest.

## Example

```d
import <std/arg>

int sum(int count, ...) {
    va_list args;
    __builtin_va_start(&args, count);

    int total = 0;
    for (int i = 0; i < count; i++) {
        total += va_arg(args, int);
    }

    __builtin_va_end(&args);
    return total;
}
```
