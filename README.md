# EPL


EPL - is a compiled, procedural and statically typed high-level systems programming language.

## "Hello, world!" Example

```nasm
@inc "std";

main: int {
  writeln("Hello, world!");
  ret 0;
}
```

## Prerequisites

* `llvm` (+ dev libs)
* `binutils`, `binutils-avr`(if you need avr support)
* dub
* D compiler(dmd/ldc/gdc)

## Building/Running

Copy `base.dub.json` to `dub.json` and change the flags. Then run `dub run -- <filename>`

## Goals

* ~~Make a lexer~~ (100%)
* ~~Make a preprocessor~~ (100%)
* ~~Make a parser~~ (100%)
* Make a code generator (15%)
