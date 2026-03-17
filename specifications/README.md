# Introduction

It contains specifications of Rave.

Since the language is currently under active development, we decided to combine the documentation and specification into one section "specifications" for convenience.

## Lexical

Lexical analysis splits the source code of the program into tokens for subsequent parsing.

The source text can be in the following encodings:
- ASCII;
- UTF-8.

There are two types of comments: single-line and multi-line.

Multilines start with /* and end with */.
Single-lines start with //.

Strings are indicated by text enclosed in double quoted ("").
The characters are enclosed in single quotes (").

## Grammar

##### [Commands](grammar/commands.md)
##### [Declarations](grammar/declarations.md)
##### [Types](grammar/types.md)
##### [Operators](grammar/operators.md)

## Standard library

##### [std/arg](std/arg.md)
##### [std/ascii](std/ascii.md)
##### [std/blas](std/blas.md)
##### [std/crypto](std/crypto.md)
##### [std/cstring](std/cstring.md)
##### [std/error](std/error.md)
##### [std/hash](std/hash.md)
##### [std/http](std/http.md)
##### [std/io](std/io.md)
##### [std/json](std/json.md)
##### [std/library](std/library.md)
##### [std/locale](std/locale.md)
##### [std/map](std/map.md)
##### [std/math](std/math.md)
##### [std/memory](std/memory.md)
##### [std/prelude](std/prelude.md)
##### [std/process](std/process.md)
##### [std/pthread](std/pthread.md)
##### [std/random](std/random.md)
##### [std/socket](std/socket.md)
##### [std/sort](std/sort.md)
##### [std/stack](std/stack.md)
##### [std/string](std/string.md)
##### [std/sysc](std/sysc.md)
##### [std/system](std/system.md)
##### [std/thread](std/thread.md)
##### [std/time](std/time.md)
##### [std/unicode](std/unicode.md)
##### [std/utf](std/utf.md)
##### [std/vector](std/vector.md)

## Other

##### [RaveDoc](ravedoc.md)
