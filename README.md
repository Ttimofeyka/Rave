<h1 align="center">Rave Language</h1>

Rave is a statically typed, compiled, procedural, general-purpose programming language. We (core contributors) took
inspiration from C, C++, Rust and Assembly, making an easy-to-use but low-level language.

## "Hello, world!" Example

```nasm
@inc "std/io"

main: int {
  puts("Hello, world!\n");
  ret 0;
}
```

## Requirements

* `llvm-10` (including dev libs)
* `binutils`, `binutils-avr` (if you need AVR-support)
* dub and dmd compiler

## Building/Running

If you don't want to break your head, you can just run one of the install files, one for Linux, the other for Windows (Powershell).

Otherwise, install all dependencies and write `dub build` in the project directory.

## Reference

The reference is in `reference` directory.

Discord server - https://discord.gg/AfEtyArvsM
