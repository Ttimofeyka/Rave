# std/unicode

Unicode character classification and case conversion.

## Functions

### isSpace
```d
bool isSpace(uint c)
```
Checks if codepoint is whitespace (space, tab, newline, etc.).

### isDigit
```d
bool isDigit(uint c)
```
Checks if codepoint is ASCII digit (0-9).

### isLower
```d
bool isLower(uint c)
```
Checks if codepoint is lowercase letter. Supports ASCII and Cyrillic.

### isUpper
```d
bool isUpper(uint c)
```
Checks if codepoint is uppercase letter. Supports ASCII and Cyrillic.

### toLower
```d
uint toLower(uint c)
```
Converts codepoint to lowercase. Supports ASCII and Cyrillic.

### toUpper
```d
uint toUpper(uint c)
```
Converts codepoint to uppercase. Supports ASCII and Cyrillic.

## Notes

- Currently supports ASCII and Russian Cyrillic characters
- Full Unicode support is planned for future versions

## Example

```d
import <std/unicode>

uint ch = 'A';
if (std::unicode::isUpper(ch)) {
    ch = std::unicode::toLower(ch);
}
```
