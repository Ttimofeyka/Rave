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