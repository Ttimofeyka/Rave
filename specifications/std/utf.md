# std/utf

UTF-8 and UTF-16 encoding conversion and manipulation.

## UTF-8 Functions

### std::utf8::utflen
```d
usize utflen(uchar* str)
```
Returns number of UTF-8 characters (not bytes).

### std::utf8::charlen
```d
usize charlen(uchar ch)
usize charlen(uchar* ch, usize idx)
```
Returns byte length of UTF-8 character.

### std::utf8::isStart
```d
bool isStart(char c)
```
Checks if byte is start of UTF-8 character.

### std::utf8::next
```d
usize next(uchar* str)
```
Returns index of next UTF-8 character.

### std::utf8::toCodepoint
```d
uint toCodepoint(uchar* str, usize at)
```
Converts UTF-8 character at position to Unicode codepoint.

### std::utf8::toUTF16Buffer
```d
usize toUTF16Buffer(char* u8, usize u8Length, ushort* buffer, usize bufferSize)
usize toUTF16Buffer(char* u8, ushort* buffer, usize bufferSize)
```
Converts UTF-8 to UTF-16. Returns required buffer size.

### std::utf8::toUTF16
```d
ushort* toUTF16(char* u8, usize u8Length)
ushort* toUTF16(char* u8)
```
Converts UTF-8 string to UTF-16 (allocates memory).

## UTF-16 Functions

### std::utf16::strlen
```d
int strlen(ushort* wstr)
```
Returns length of UTF-16 string in code units.

### std::utf16::utflen
```d
int utflen(ushort* wstr)
```
Returns number of Unicode characters (handles surrogates).

### std::utf16::toUTF8Buffer
```d
usize toUTF8Buffer(ushort* u16, usize u16Length, char* buffer, usize bufferSize)
usize toUTF8Buffer(ushort* u16, char* buffer, usize bufferSize)
```
Converts UTF-16 to UTF-8. Returns required buffer size.

### std::utf16::toUTF8
```d
char* toUTF8(ushort* u16, usize u16Length)
char* toUTF8(ushort* u16)
```
Converts UTF-16 string to UTF-8 (allocates memory).

## Constants

- `std::utf16::bmpEnd` - End of Basic Multilingual Plane (0xFFFF)

## Example

```d
import <std/utf>

char* utf8Str = "Hello 世界";
usize charCount = std::utf8::utflen(utf8Str);

ushort* utf16 = std::utf8::toUTF16(utf8Str);
char* backToUtf8 = std::utf16::toUTF8(utf16);
```
