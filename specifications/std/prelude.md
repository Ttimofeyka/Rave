# std/prelude

Auto-imported core functions and utilities.

## Import

This module is automatically imported unless `--noPrelude` flag is used.

## Functions

### exit
```d
void exit(int code)
```
Terminates program with exit code.

### assert
```d
void assert(bool cond)
void assert(bool cond, char* msg)
```
Asserts condition is true. Prints message and exits with code 1 if false.

## Dependencies

The prelude automatically compiles and links:
- std/thread
- std/cstring
- std/ascii
- std/string
- std/error
- std/io
- std/hash

## Example

```d
// No import needed - prelude is auto-imported

std::assert(x > 0, "x must be positive");

if (error) std::exit(1);
```
