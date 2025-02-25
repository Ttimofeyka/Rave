# std/cstring.rave

## std::cstring::strlen

Get the length of the C-string.

Example:

```d
int len = std::cstring::strlen("Hello, world!"); // 13
```

## std::cstring::fromBool

Convert a boolean value to a C-string.

Example:

```d
char* value = std::cstring::fromBool(true); // "true"
```

## std::cstring::strcmp

Compare two C-strings.

Example:

```d
bool value = std::cstring::strcmp("One", "Two"); // false
```

## std::cstring::strncmp

Compare two C-strings with fixed count of comparing characters.

Example:
```d
bool value = std::cstring::strncmp("Alien", "Alt", 1); // true
```

## std::cstring::strstr

Find the substring in the string (returns pointer to substring or null).

Example:

```d
char* ptr = std::cstring::strstr("Hello!", "ell");
```

## std::cstring::indexOf

Get the index of char in the string.

Example:

```d
int index = std::cstring::indexOf("String", 'r');
```

## std::cstring::ltos / std::cstring::ultos, std::cstring::ctos / std::cstring::uctos

Convert an integer to a string by writing the result to a pointer to the buffer.

`ltos` - 64-bit number (`ultos` - unsigned), `ctos` - 128-bit number (`uctos` - unsigned).

Example:

```d
char[8] buffer;
std::cstring::ltos(1l, &buffer);
std::println(&buffer); // 1
```

## std::cstring::dtos

Convert a floating-point number to a string by writing the result to a pointer to the buffer.

You can set the number of digits after the decimal point.

Example:

```d
char[8] buffer;
std::cstring::dtos(1.155d, 1, &buffer);
std::println(&buffer); // 1.1
```

## std::cstring::stol / std::cstring::stoul, std::cstring::stoc / std::cstring::stouc

Convert a string to an integer with long/ulong (`std::cstring::stol`, `std::cstring::stoul`) or cent/ucent (`std::cstring::stoc`, `std::cstring::stouc`) sizes.

Example:
```d
long l1 = std::cstring::stol("100");
// Or
long l2 = std::cstring::stol("2000", 4); // Number of symbols

cent c1 = std::cstring::stoc("100");
// Or
cent c2 = std::cstring::stoc("2000", 4); // Number of symbols
```

## std::cstring::stod, std::cstring::stor

Convert a string to a double (`std::cstring::stod`) or a real (`std::cstring::stor`).

Example:
```d
double d1 = std::cstring::stod("10");
// Or
double d2 = std::cstring::stod("76.5", 4); // Number of symbols
```