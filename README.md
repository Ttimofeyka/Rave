<h1 align="center">Rave Language</h1>

Rave is a statically typed, compiled, procedural, general-purposeprogramming language. We (core contributors) took
inspiration from C, C++, Rust, Assembly and we are making an easy-to-use but low-level language.

## "Hello, world!" Example

```nasm
@inc "std/io";

main: int {
  writeln("Hello, world!");
  ret 0;
}
```

## Requirements

* `llvm-10` (including dev libs)
* `binutils`, `binutils-avr` (if you need avr support)
* dub
* D compiler (dmd/ldc/gdc)
* [node.js](https://nodejs.org/) (for documentation)

## Building/Running

Rename `base.dub.json` to `dub.json` and change the flags. Then run `dub build`

## Goals

* ~~Make a lexer~~ (100%)
* ~~Make a preprocessor~~ (100%)
* ~~Make a parser~~ (100%
* Make a semantic analyzer (87%)
* Make a code generator (75%)

Discord server - https://discord.gg/AfEtyArvsM
