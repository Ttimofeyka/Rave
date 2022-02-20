# EPL


EPL - is a compiled, procedural and statically typed high-level programming language.

## "Hello, world!" Example

```nasm
inc "std/*";
main: int {
  print("Hello, world!");
  ret 0;
}
```

## Prerequisites

* `llvm` (+ dev libs)
* dub
* D compiler

## Building/Running

Copy `base.dub.json` to `dub.json` and change the flags. Thenrun `dub run -- <filename>`


## Goals

* ~~Make a lexer~~ (100%)
* ~~Make a preprocessor~~ (100%)
* ~~Make a parser~~ (100%)
* Make a code generator (15%)