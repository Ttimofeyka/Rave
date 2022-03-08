<h1 align="center">Rave Language</h1>

Rave is a statically typed, compiled, procedural systems programming language. We (core contributors) took
inspiration from C, C++, Rust and Assembly making an easy-to-use but lower-level language.

## "Hello, world!" Example

```nasm
@inc "std";

main: int {
  writeln("Hello, world!");
  ret 0;
}
```

## Prerequisites

* `llvm` (including dev libs)
* `binutils`, `binutils-avr` (if you need avr support)
* dub
* D compiler (dmd/ldc/gdc)
* [node.js](https://nodejs.org/) (for documentation)

## Building/Running

Copy `base.dub.json` to `dub.json` and change the flags. Then run `dub build`

## Goals

* ~~Make a lexer~~ (100%)
* ~~Make a preprocessor~~ (100%)
* ~~Make a parser~~ (100%
* Make a semantic analyzer (57%)
* Make a code generator (65%)

Discord server - https://discord.gg/AfEtyArvsM

## TODO

- AST/Analyzer: Add return handling for if/while
