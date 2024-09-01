# std/ascii.rave

## std::ascii::isSpace

Returns true if the passed character is a space or some other kind of indentation.

Example:

```d
bool foo = std::ascii::isSpace('1'); // false
bool bow = std::ascii::isSpace(' ') && std::ascii::isSpace('\n'); // true
```

## std::ascii::isLower, std::ascii::isUpper

Returns true if a lower or upper character is passed.

Example:

```d
bool lower = std::ascii::isLower('A'); // false
lower = std::ascii::isLower('a'); // true

bool upper = std::ascii::isUpper('1'); // false
upper = std::ascii::isUpper('B'); // true
```

## std::ascii::toLower, std::ascii::toUpper

Returns a character converted to lower or upper.

Example:

```d
char upChar = std::ascii::toUpper('a'); // A
char noChange = std::ascii::toUpper('0'); // 0
char lowerChar = std::ascii::toLower('A'); // a
```

## std::ascii::isDigit

Returns true if the passed character is a digit.

Example:

```d
bool noDigit = std::ascii::isDigit(' '); // false
bool digit = std::ascii::isDigit('7'); // true
```
