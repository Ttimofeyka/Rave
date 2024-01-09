# std/library.rave

## std::library::open

Opens a dynamic library for reading.

Example:

```d
void* lib = std::library::open("lib.so");
```

## std::library::close

Closes the dynamic library for reading.

Example:

```d
void* lib = std::library::open("lib.dll");
std::library::close(lib);
```

## std::library::sym

Requesting a variable or function from a dynamic library.

Example:

```d
void* lib = std::library::open("lib.so");
    int(int, int) sum = cast(int(int, int))std::library::sym("_Ravef3sum");
std::library::close(lib);
```
