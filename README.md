<h1 align="center">Rave Language</h1>

Rave is a statically typed, compiled, procedural systems programming language. Taken
inspiration from a lot of programming languages it is an easy-to-use but lower-level language.
Current goals include compiling to arduino and other other similar platforms, all while the
standard library is almost the same.

## "Hello, World!" Example

```nasm
@inc ":std/io/io";

main: int {
  writeln("Hello, World!");
  ret 0;
}
```

## Prerequisites

* `llvm` (including development libraries)
* `binutils`, `binutils-avr` (if you need avr support)
* dub
* D compiler (dmd/ldc/gdc)
* [node.js](https://nodejs.org/) (for documentation)

## Building/Running

Copy `base.dub.json` to `dub.json` and change the linker flags if required.
Then run `dub build` and `./rave <args>` (You can do this in one command: `dub run -- <args>`).

## Goals

* ~~Make a lexer~~ (100%)
* ~~Make a preprocessor~~ (100%)
* ~~Make a parser~~ (100%
* Make a semantic analyzer (77%)
* Make a code generator (65%)

Discord server - [Rave Programming Language Community](https://discord.gg/AfEtyArvsM) <sup><sub>(note: mostly Russian, but feel free to talk in English!)</sub></sup>
