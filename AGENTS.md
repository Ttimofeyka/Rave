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
- **Stdlib cache**: Standard library compiles to `std/<module>.<platform>.o`. Use `--recompileStd` after changing optimization flags or stdlib code. **If you modify any `std/*.rave` file, you MUST recompile with `rave -rcs -Ofast` or `--recompileStd`, otherwise cached `.o` files will be used and your changes will be ignored.**
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

**CRITICAL**: If you modify any standard library file (`std/*.rave`), you **MUST** recompile the standard library before the changes take effect:
```bash
rave source.rave -o output -rcs -Ofast          # Recompile std with fast optimization
./rave source.rave -o output --recompileStd      # Alternative flag
```
Without recompilation, the cached `std/*.o` files will be used and your changes will be ignored.

### Core (Auto-imported)

- **prelude** - Auto-imports sysc, thread, cstring, ascii, string, error, io, hash. Provides `std::exit(code)`, `std::assert(cond)`, `std::assert(cond, msg)`.
- **memory** - Memory management: `std::amalloc(alignment, size)`, `std::malloc(size)`, `std::free(ptr)`, `std::realloc(ptr, newsize)`, `std::calloc(num, size)`, `std::memcpy(dest, src, n)`, `std::memmove(dest, src, n)`, `std::swap(one, two, size)`, `std::memset(dest, c, n)`, `std::memcmp(one, two, n)`, `std::new<T>(n)`, `std::new<T>()`, `std::anew<T>(alignment, n)`, `std::anew<T>(alignment)`. Struct `std::pair<P1, P2> this(first, second)`.
- **io** - File/console I/O. Struct `std::file this(name)`, `std::file this(name, bufferSize)`, methods: `file.open(mode)`, `file.openFd(mode, fd)`, `file.isOpen`, `file.rename(to)`, `file.exists`, `file.seek(flag, offset)`, `file.flush`, `file.read(buffer, bytes)`, `file.write(buf, bytes)`, `file.getc`, `file.putc(c)`, `file.close`, `file.readLineBuffer(buffer)`, `file.readLine`, `file.readAllBuffer(buffer)`, `file.readAll`. Struct `std::directory this(path)`, methods: `dir.open`, `dir.close`, `dir.readEntriesBuffer(list)`, `dir.readEntries`, `dir.exists`. Functions: `std::puts(s)`, `std::putswnl(s)`, `std::putswnl(s)`, `std::print(ctargs)`, `std::println(ctargs)`, `std::fprint(ctargs)`, `std::fprintln(ctargs)`, `std::input(ctargs)`, `std::getchar()`, `std::putchar(c)`, `std::readLineBuffer(buffer)`, `std::readLine`, `std::directory::exists(dname)`, `std::directory::create(dname)`, `std::file::rename(oldPath, newPath)`, `std::file::remove(path)`, `std::file::exists(fname)`. ANSI color constants in `std::ansi`.
- **string** - Dynamic string. Struct `std::string this(str)`, `std::string this(n)`, methods: `str.setValue(a)`, `str.set(index, ch)`, `str.at(index)`, `str.isDeleted`, `str.c`, `str.toChar`, `str.toCent`, `str.toUcent`, `str.toLong`, `str.toUlong`, `str.toInt`, `str.toUint`, `str.toDouble`, `str.indexOf(c)`, `str.indexOf(s, begin)`, `str.indexOf(s)`, `str.add(c)`, `str.appendC(cstr, cstrlen)`, `str.append(cstr)`, `str.append(str)`, `str.copy`, `str.replace(c, to)`, `str.replace(from, fromLen, to, toLen)`, `str.substring(from, to)`, `str.split(ch)`, `str.ltrim`, `str.rtrim`, `str.trim`, `str.insert(pos, str)`, `str.hash`. Static: `std::string::fromNumber(n)`, `std::string::fromUNumber(n)`, `std::string::fromDoubleN(n, precision)`, `std::string::fromDouble(n)`, `std::string::fromFloatN(n, precision)`, `std::string::fromFloat(n)`. `std::sprint(ctargs)` for formatting, `std::gsprint(ctargs)` internal.
- **cstring** - C-string utilities: `std::cstring::strlen(str)`, `std::cstring::fromBool(b)`, `std::cstring::strncmp(s1, s2, n)`, `std::cstring::strcmp(s1, s2)`, `std::cstring::strchr(chars, c)`, `std::cstring::strstr(str, substr)`, `std::cstring::indexOf(str, ch)`, `std::cstring::ltos(number, endBuffer)`, `std::cstring::ultos(number, endBuffer)`, `std::cstring::ctos(number, endBuffer)`, `std::cstring::uctos(number, endBuffer)`, `std::cstring::dtos(number, precision, buffer)`, `std::cstring::stol(str)`, `std::cstring::stoul(str)`, `std::cstring::stoc(str)`, `std::cstring::stouc(str)`, `std::cstring::stod(str)`, `std::cstring::stor(str)`. Struct `std::cstring this(data, length)`, `std::cstring this(data)`.
- **error** - Error codes: `std::NotFound`, `std::NotEnoughMemory`, `std::BeyondSize`. Signals: `std::SigInt`, `std::SigKill`, `std::SigSegv`, `std::SigIll`, `std::SigFpe`, `std::SigAlarm`, `std::SigTerm`, `std::SigStop`, `std::SigBreak`, `std::SigAbort`, `std::SigDefault`, `std::SigIgnore`. Function: `std::signal(flag, catcher)`.
- **hash** - `std::hash::crc32(data, length)`.

### Data Structures

- **vector** - Dynamic array. Struct `std::vector<VECTOR_T> this`, `std::vector<VECTOR_T> this(size)`, `std::vector<VECTOR_T> this(data, size)`. Methods: `vec.add(value)`, `vec.assign(of)`, `vec.set(index, value)`, `vec.zero`, `vec.clear`, `vec.copy`, `vec.swap(first, second)`, `vec.remove(index)`, `vec.removeLast`, `vec.transform(fn)`, `vec.ptr`, `vec.hash`.
- **stack** - LIFO stack. Struct `std::stack<STACK_T> this`, `std::stack<STACK_T> this(data, size)`. Methods: `stk.push(value)`, `stk.top`, `stk.pop`, `stk.assign(of)`, `stk.copy`, `stk.clear`, `stk.ptr`, `stk.hash`.
- **map** - HashMap<K,V>. Struct `std::hashmap<K, V> this`, `std::hashmap<K, V> this(mapSize)`. Methods: `map.set(key, value)`, `map.find(key)`, `map.contains(key)`, `map.get(key)`, `map.remove(key)`, `map.clear`, `map.next(iterator)`. Helper structs: `std::hashmap::entry<KE, VE>`, `std::hashmap::iterator<KI, VI>`.

### Text & Encoding

- **ascii** - ASCII classification: `std::ascii::isSpace(c)`, `std::ascii::isPrint(c)`, `std::ascii::isLower(c)`, `std::ascii::isUpper(c)`, `std::ascii::isDigit(c)`, `std::ascii::isXDigit(c)`, `std::ascii::isAlphaNumeric(c)`, `std::ascii::toLower(c)`, `std::ascii::toUpper(c)`.
- **unicode** - Unicode classification (includes Cyrillic): `std::unicode::isSpace(c)`, `std::unicode::isDigit(c)`, `std::unicode::isLower(c)`, `std::unicode::isUpper(c)`, `std::unicode::toLower(c)`, `std::unicode::toUpper(c)`.
- **utf** - UTF-8/UTF-16: `std::utf8::isStart(c)`, `std::utf8::utflen(str)`, `std::utf8::charlen(ch)`, `std::utf8::charlen(ch, idx)`, `std::utf8::next(str)`, `std::utf8::toCodepoint(str, at)`, `std::utf8::toUTF16Buffer(u8, u8Length, buffer, bufferSize)`, `std::utf8::toUTF16(u8, u8Length)`, `std::utf8::toUTF16(u8)`. `std::utf16::strlen(wstr)`, `std::utf16::utflen(wstr)`, `std::utf16::toUTF8Buffer(u16, u16Length, buffer, bufferSize)`, `std::utf16::toUTF8Buffer(u16, buffer, bufferSize)`, `std::utf16::toUTF8(u16, u16Length)`, `std::utf16::toUTF8(u16)`.

### Math & Algorithms

- **math** - Math functions in `std::math`: `std::math::exp(x)`, `std::math::isNAN(d)`, `std::math::isNAN(f)`, `std::math::isNAN(hf)`, `std::math::factorial(n)`, `std::math::abs<T>(value)`, `std::math::getSign<T>(value)`, `std::math::copySign<T>(v1, v2)`, `std::math::floor(f)`, `std::math::ceil(d)`, `std::math::loge(x)`, `std::math::log(x)`, `std::math::sqrt(d)`, `std::math::acos(d)`, `std::math::asin(x)`, `std::math::coshn(d, n)`, `std::math::cosh(x)`, `std::math::sinh(x)`, `std::math::tanh(x)`, `std::math::dtanh(x)`, `std::math::acosh(x)`, `std::math::asinh(x)`, `std::math::atanh(x)`, `std::math::erf(x)`, `std::math::pow(f, f2)`, `std::math::cbrt(d)`, `std::math::cos(d)`, `std::math::sin(x)`, `std::math::fma(a, b, c)`, `std::math::tan(d)`, `std::math::mod(x)`, `std::math::round(x, n)`, `std::math::isPrime(number)`, `std::math::sign<T>(x)`, `std::math::min<T>(one, two)`, `std::math::max<T>(one, two)`, `std::math::clamp<T>(value, one, two)`, `std::math::degToRad(d)`, `std::math::radToDeg(r)`. Neural: `std::math::sigmoid(x)`, `std::math::dsigmoid(x)`, `std::math::softmax(x, size)`, `std::math::softplus(x)`, `std::math::rmsnorm(out, x, weight, size)`, `std::math::relu(x)`, `std::math::drelu(x)`, `std::math::gelu(x)`, `std::math::geglu(input, output, size)`. Constants: `std::math::PI`, `std::math::PI_f`, `std::math::PI_hf`, `std::math::NAN`, `std::math::NAN_f`, `std::math::NAN_hf`.
- **sort** - `std::sort<T>(ptr, length)` (quicksort).
- **random** - RNG: `std::randomChar`, `std::randomShort`, `std::randomInt`, `std::randomLong`, `std::randomCent`, `std::randomBuffer(buffer, length)`. Range versions: `std::randomChar(min, max)`, `std::randomShort(min, max)`, `std::randomInt(min, max)`, `std::randomLong(min, max)`, `std::randomCent(min, max)`.

### Threading & Concurrency

- **thread** - Struct `std::thread this`, methods: `thread.run(fn, argument)`, `thread.join`, `thread.hash`. `std::thread::exit(value)`, `std::thread::exit`. `std::thread::getCurrentID`. Primitives: `std::spinlock this`, methods `lock`, `unlock`. `std::thread::spinlock::lock(sl)`, `std::thread::spinlock::unlock(sl)`. Struct `std::mutex this`, methods: `mutex.lock`, `mutex.unlock`, `mutex.trylock`, `mutex.destroy`.
- **pthread** - POSIX threads (non-Windows): `pthread::create(thread, attr, start, arg)`, `pthread::join(thread, valuePtr)`, `pthread::self`, `pthread::exit(retVal)`, `pthread::cancel(thread)`, `pthread::detach(thread)`. Mutex: `pthread::mutex_init(mutex, attr)`, `pthread::mutex_lock(mutex)`, `pthread::mutex_unlock(mutex)`, `pthread::mutex_destroy(mutex)`, `pthread::mutex_trylock(mutex)`. Structs: `pthread::attribute`, `pthread::mutexattr`, `pthread::mutex`.

### System & Process

- **sysc** - Syscall wrappers (Linux/FreeBSD): `std::syscall(number, ...)`. Platform-specific syscall numbers in `std::sysctable` namespace (e.g., `std::sysctable::Read`, `std::sysctable::Write`, `std::sysctable::OpenAt`, `std::sysctable::Close`, `std::sysctable::Exit`, `std::sysctable::GetPID`, etc.).
- **process** - Process control in `std::process`: `std::process::isTermSignal(s)`, `std::process::isNormalExit(s)`, `std::process::getExitStatus(s)`, `std::process::execve(fName, argv, envp)`, `std::process::wait4(pid, status, options, rusage)`, `std::process::waitpid(pid, status, options)`, `std::process::getPID`, `std::process::getPPID`, `std::process::kill(pid, sig)`, `std::process::execl(ctargs)`, `std::process::clone(fn, stack, flags, arg, pTID, tls, cTID)`, `std::process::fork`. Struct `std::process::rusage`.
- **system** - `std::system(cmd)`, `std::system::reboot(cmd)`, `std::system::getCwd(buffer, size)`.
- **time** - Structs `std::time` (fields: seconds, milliseconds, microseconds), `std::timeVal` (fields: sec, usec), `std::timeW` (fields: sec, min, hour, mday, month, year, wday, yday, isdst, gmtoff, zone). Functions: `std::getCurrentTime(tc)`, `std::sleep(milliseconds)`. Constants: `std::RealTime`, `std::Monotonic`.
- **arg** - Variadic args: `struct va_list` (platform-dependent), `__builtin_va_start(va)`, `__builtin_va_end(va)`, `__builtin_va_copy(dest, src)`.
- **library** - Dynamic loading in `std::library`: `std::library::open(filename)`, `std::library::close(handle)`, `std::library::sym(handle, name)`, `std::library::error`. Constant: `std::library::RtldLazy`.

### Networking

- **socket** - Structs: `std::socket this`, methods: `sock.open(ip, port, flag)`, `sock.openAny(port)`, `sock.bind`, `sock.listen`, `sock.connect`, `sock.accept(acceptor)`, `sock.read(data, length)`, `sock.write(data, length)`, `sock.write(data)`, `sock.write(str)`, `sock.close`. `std::socket::sockaddr`, `std::socket::sockaddr_in`, `std::socket::addrinfo`, `std::socket::hostent`, `std::socket::in_addr`, `std::socket::iovec`, `std::socket::msghdr`, `std::socket::sockaddr_storage`, `std::socket::hostInfo`. Functions in `std::socket`: `socket(domain, type, protocol)`, `bind(sock, address, addressLen)`, `listen(sock, backlog)`, `connect(sock, address, addressLen)`, `accept(sock, addr, lengthPtr)`, `recv(sock, buffer, length, flags)`, `send(sock, message, length, flags)`, `recvFrom(sock, buffer, length, flags, address, addressLen)`, `sendTo(sock, message, length, flags, destAddr, destLen)`, `recvMsg(sock, message, flags)`, `sendMsg(sock, message, flags)`, `shutdown(sock, how)`, `close(sock)`, `getAddrInfo(node, service, hints, res)`, `freeAddrInfo(res)`, `inetPton(af, src, dst)`, `inetNtop(af, src, dst, cnt)`, `inetAddr(cp)`, `getHostByName(name)`, `getHostByAddr(addr, len, type)`, `htonl(hostlong)`, `htons(hostshort)`, `ntohl(netlong)`, `ntohs(netshort)`, `getPeerName(sock, address, addressLen)`, `getSockName(sock, address, addressLen)`, `getSockOpt(sock, level, optionName, optionValue, optionLen)`, `setSockOpt(sock, level, optionName, optionValue, optionLen)`, `socketpair(domain, type, protocol, socketVector)`. `std::ipv4::isValid(ip)`.
- **http** - HTTP client/server. Struct `std::http::Connection this(url, port)`, methods: `conn.open`, `conn.write(buffer)`, `conn.read(buffer, length)`, `conn.close`, `conn.sendRequestData(method, headers, body)`, `conn.sendRequest(req)`, `conn.sendResponseData(status, message, headers, body)`, `conn.sendResponse(res)`, `conn.readHeaders(contentLength, headers)`, `conn.readResponseData(statusPtr, headers, body)`, `conn.readResponse`, `conn.readRequest`, `conn.request(method, headers, body)`. Struct `std::http::Server this`, methods: `srv.open(url, port)`, `srv.accept`, `srv.close`. Structs: `std::http::Request this` (fields: headers, body, method, url, success), `std::http::Response this` (fields: headers, body, status, success).

### Data Formats

- **json** - JSON parser. Struct `std::json::Value` with methods: `val.makeBoolean(value)`, `val.makeInteger(value)`, `val.makeReal(value)`, `val.makeString(cstring, len)`, `val.makeArray(values, length)`, `val.makeObject(kv, length)`, `val.as<T>()`, `val.pointer`, `val.indexOf(name)`, `val.contains(name)`, `val.get(name)`, `val.set(name, value)`, `val.setCString(name, str)`, `val.setString(name, str)`, `val.setBoolean(name, value)`, `val.setLong(name, number)`, `val.setDouble(name, number)`, `val.setArray(name, ptr, len)`, `val.setObject(name, ptr, len)`, `val.isVariable`, `val.toString`. Types: `std::json::ValueType::Nothing`, `std::json::ValueType::Integer`, `std::json::ValueType::Real`, `std::json::ValueType::Boolean`, `std::json::ValueType::String`, `std::json::ValueType::Object`, `std::json::ValueType::Array`. Functions: `std::json::parse(str)`, `std::json::parseBuffer(buffer, data)`.
- **csv** - CSV parser. Struct `std::csv::Row`, methods: `row.add(cell, len)`, `row.add(str)`, `row.get(idx)`, `row.toString`. Struct `std::csv::Document`, methods: `doc.addRow(row)`, `doc.get(idx)`, `doc.toString`. Functions: `std::csv::parse(data)`.

### Cryptography

- **crypto** - Encoding: `std::base16::encode(buffer, str)`, `std::base16::decode(buffer, str)`, `std::base32::encode(buffer, str)`, `std::base32::decode(buffer, str)`, `std::base64::encode(buffer, str)`, `std::base64::decode(buffer, str)`.

### Scientific Computing

- **blas** - BLAS operations (SIMD-optimized) in `std::blas`: `std::blas::abs<T>(n, x)`, `std::blas::sqrt<T>(n, x)`, `std::blas::asum<T>(n, x)`, `std::blas::axpy<T>(n, alpha, x, y)`, `std::blas::axpby<T>(n, alpha, x, beta, y)`, `std::blas::dot<T>(n, x, y)`, `std::blas::scale<T>(n, x, alpha)`, `std::blas::norm<T>(n, x)`, `std::blas::max<T>(n, x)`, `std::blas::min<T>(n, x)`, `std::blas::vecmatmul<T>(rows, columns, vector, matrix, result)`, `std::blas::transpose<T>(rows, columns, matrix, target)`, `std::blas::matmul<T>(rows, columns, inners, mat1, mat2, result)`.

### Other

- **locale** - `std::locale::set(category, locale)`. Constants: `std::locale::ctype`, `std::locale::numeric`, `std::locale::time`, `std::locale::collate`, `std::locale::monetary`, `std::locale::messages`, `std::locale::all`.

## References

- `specifications/` - Language specification documents