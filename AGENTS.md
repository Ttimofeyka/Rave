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

## Return Syntax - CRITICAL

**WARNING**: `return` is a **VARIABLE**, not a keyword or statement. You **MUST NOT** write `return;` - this will cause a compilation error.

**Correct usage**:
- `return = value;` - Set the return value (function continues execution)
- `@return(value);` - Set return value and immediately exit the function
- `@return();` - Immediately exit the function without changing the return value

**INCORRECT (will fail)**:
- `return;` - This is NOT valid Rave syntax
- `return value;` - This is NOT valid Rave syntax

**Example**:
```rave
int add(int a, int b) {
    return = a + b;  // Set return value
    // function continues...
    @return();       // optionally exit early
}
```

## Types

**Basic types**: `bool`, `char`/`uchar` (8-bit), `short`/`ushort` (16-bit), `int`/`uint` (32-bit), `long`/`ulong` (64-bit), `cent`/`ucent` (128-bit), `half` (16-bit float), `bhalf` (bfloat16), `float` (32-bit), `double` (64-bit), `real` (128-bit float), `isize`/`usize` (pointer-sized), `alias` (variables only).

**Pointers**: `char*`, `bool**`, etc. **Arrays**: `int[10]`, `char*[512]`. **Function pointers**: `void()`, `int(int, int)`.

**IMPORTANT NOTE**: Array types are **not C-style**. Declaration syntax is `TYPE[NUMBER] var;` (e.g., `int[10] arr;`), not `TYPE var[NUMBER];`.

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

## Standard Library (`std/`)

### Core (Auto-imported)

- **prelude** - Auto-imports sysc, thread, cstring, ascii, string, error, io, hash. Provides `std::exit(code)`, `std::assert(cond)`, `std::assert(cond, msg)`.
- **memory** - Memory management: `std::malloc`, `std::free`, `std::realloc`, `std::amalloc` (aligned), `std::memcpy`, `std::memmove`, `std::memset`, `std::memcmp`, `std::swap`, `std::new<T>()`, `std::anew<T>()`. Struct `std::pair<K,V>`.
- **io** - File/console I/O. Structs `std::file` (open, close, read, write, getc, putc, seek, flush, readLine, readAll), `std::directory` (open, close, readEntries). Functions: `std::puts`, `std::print`, `std::println`, `std::fprint`, `std::input`, `std::getchar`, `std::putchar`, `std::readLine`. ANSI color constants in `std::ansi`.
- **string** - Dynamic string. Struct `std::string`. Methods: `add`, `append`, `at`, `set`, `indexOf`, `replace`, `substring`, `split`, `trim`, `insert`, `copy`, `hash`, `toChar`, `toInt`, `toLong`, `toDouble`. Static: `std::string::fromNumber`, `std::string::fromDouble`. `std::sprint` for formatting.
- **cstring** - C-string utilities: `std::cstring::strlen`, `std::cstring::strcmp`, `std::cstring::strncmp`, `std::cstring::strchr`, `std::cstring::strstr`, `std::cstring::indexOf`. Conversions: `std::cstring::ltos`, `std::cstring::ultos`, `std::cstring::dtos`, `std::cstring::stol`, `std::cstring::stoul`, `std::cstring::stod`. Struct `std::cstring`.
- **error** - Error codes: `std::NotFound`, `std::NotEnoughMemory`, `std::BeyondSize`. Signals: `std::SigInt`, `std::SigKill`, `std::SigSegv`, `std::SigIll`, `std::SigFpe`, `std::SigAlarm`, `std::SigTerm`, `std::SigStop`, `std::SigBreak`, `std::SigAbort`. Function: `std::signal(flag, handler)`.
- **hash** - `std::hash::crc32(data, length)`.

### Data Structures

- **vector** - Dynamic array. Struct `std::vector<T>`. Methods: `add`, `set`, `at`, `remove`, `removeLast`, `clear`, `copy`, `swap`, `assign`, `transform`, `ptr`, `hash`.
- **stack** - LIFO stack. Struct `std::stack<T>`. Methods: `push`, `pop`, `top`, `clear`, `copy`, `ptr`, `hash`.
- **map** - HashMap<K,V>. Struct `std::hashmap<K,V>`. Methods: `set`, `get`, `find`, `contains`, `remove`, `clear`, `next` (iterator).

### Text & Encoding

- **ascii** - ASCII classification: `std::ascii::isSpace`, `std::ascii::isPrint`, `std::ascii::isLower`, `std::ascii::isUpper`, `std::ascii::isDigit`, `std::ascii::isXDigit`, `std::ascii::isAlphaNumeric`, `std::ascii::toLower`, `std::ascii::toUpper`.
- **unicode** - Unicode classification (includes Cyrillic): `std::unicode::isSpace`, `std::unicode::isDigit`, `std::unicode::isLower`, `std::unicode::isUpper`, `std::unicode::toLower`, `std::unicode::toUpper`.
- **utf** - UTF-8/UTF-16: `std::utf8::utflen`, `std::utf8::charlen`, `std::utf8::toCodepoint`, `std::utf8::toUTF16`, `std::utf8::toUTF16Buffer`. `std::utf16::strlen`, `std::utf16::toUTF8`, `std::utf16::toUTF8Buffer`.

### Math & Algorithms

- **math** - Math functions in `std::math`: `abs`, `min`, `max`, `clamp`, `sqrt`, `pow`, `exp`, `log`, `loge`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `sinh`, `cosh`, `tanh`, `floor`, `ceil`, `round`, `mod`, `factorial`, `isPrime`, `isNAN`. Neural: `std::math::sigmoid`, `std::math::softmax`, `std::math::rmsnorm`, `std::math::relu`, `std::math::gelu`. Constants: `std::math::PI`, `std::math::NAN`.
- **sort** - `std::sort<T>(ptr, length)` (quicksort).
- **random** - RNG: `std::randomChar`, `std::randomShort`, `std::randomInt`, `std::randomLong`, `std::randomCent`, `std::randomBuffer`. Range versions: `std::randomInt(min, max)`, `std::randomLong(min, max)`, etc.

### Threading & Concurrency

- **thread** - Struct `std::thread`: `run(fn, arg)`, `join`. `std::thread::exit()`. Primitives: `std::spinlock` (`lock`, `unlock`), `std::mutex` (`lock`, `unlock`, `trylock`).
- **pthread** - POSIX threads (non-Windows): `pthread::create`, `pthread::join`, `pthread::self`, `pthread::exit`, `pthread::cancel`, `pthread::detach`. Mutex: `pthread::mutex_init`, `pthread::mutex_lock`, `pthread::mutex_unlock`, `pthread::mutex_destroy`, `pthread::mutex_trylock`.

### System & Process

- **sysc** - Syscall wrappers (Linux/FreeBSD): `std::syscall`. Platform-specific syscall numbers in `std::sysctable` namespace.
- **process** - Process control in `std::process`: `getPID`, `getPPID`, `kill`, `fork`, `clone`, `execve`, `execl`, `wait4`, `waitpid`, `isTermSignal`, `isNormalExit`, `getExitStatus`.
- **system** - `std::system(cmd)`, `std::system::reboot`, `std::system::getCwd`.
- **time** - Structs `std::time`, `std::timeVal`, `std::timeW`. Functions: `std::getCurrentTime()`, `std::sleep(ms)`. Constants: `std::RealTime`, `std::Monotonic`.
- **arg** - Variadic args: `va_list` struct, `__builtin_va_start`, `__builtin_va_end`, `__builtin_va_copy`.
- **library** - Dynamic loading in `std::library`: `open`, `close`, `sym`, `error`.

### Networking

- **socket** - Structs: `std::socket`, `std::socket::sockaddr`, `std::socket::sockaddr_in`, `std::socket::addrinfo`, `std::socket::hostent`, `std::socket::in_addr`, `std::socket::iovec`, `std::socket::msghdr`. Functions in `std::socket`: `socket`, `bind`, `listen`, `connect`, `accept`, `recv`, `send`, `recvFrom`, `sendTo`, `recvMsg`, `sendMsg`, `shutdown`, `close`, `getAddrInfo`, `freeAddrInfo`, `inetAddr`, `inetPton`, `inetNtop`, `getHostByName`, `getHostByAddr`, `htonl`, `htons`, `ntohl`, `ntohs`. `std::ipv4::isValid`.
- **http** - HTTP client/server. Structs in `std::http`: `std::http::Connection` (open, readResponse, readRequest, sendRequest, sendResponse, request), `std::http::Server` (open, accept, close), `std::http::Request`, `std::http::Response`.

### Data Formats

- **json** - JSON parser. Struct `std::json::Value` with types: `std::json::ValueType::Integer`, `std::json::ValueType::Real`, `std::json::ValueType::Boolean`, `std::json::ValueType::String`, `std::json::ValueType::Object`, `std::json::ValueType::Array`. Functions: `std::json::parse`, `std::json::parseBuffer`. Methods on `Value`: `get`, `set`, `as<T>`, `toString`, `indexOf`, `contains`.
- **csv** - CSV parser. Structs `std::csv::Row`, `std::csv::Document`. Functions: `std::csv::parse`. Methods: `add`, `get`, `toString`.

### Cryptography

- **crypto** - Encoding: `std::base16::encode`, `std::base16::decode`, `std::base32::encode`, `std::base32::decode`, `std::base64::encode`, `std::base64::decode`.

### Scientific Computing

- **blas** - BLAS operations (SIMD-optimized) in `std::blas`: `abs`, `sqrt`, `asum`, `axpy`, `axpby`, `dot`, `scale`, `norm`, `max`, `min`, `vecmatmul`, `matmul`, `transpose`.

### Other

- **locale** - `std::locale::set(category, locale)`. Constants: `std::locale::ctype`, `std::locale::numeric`, `std::locale::time`, `std::locale::collate`, `std::locale::monetary`, `std::locale::messages`, `std::locale::all`.

## References

- `specifications/` - Language specification documents