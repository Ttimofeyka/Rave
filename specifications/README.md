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

##### [std/ascii](std/ascii.md)
##### [std/crypto](std/crypto.md)
##### [std/cstring](std/cstring.md)
##### [std/hash](std/hash.md)
##### [std/io](std/io.md)
##### [std/library](std/library.md)
##### [std/memory](std/memory.md)
##### [std/random](std/random.md)
##### [std/thread](std/thread.md)
##### [std/vector](std/vector.md)

## Other

##### [RaveDoc](ravedoc.md)
