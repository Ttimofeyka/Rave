# std/string

Dynamic string type with automatic memory management.

## Types

### std::string

Dynamic string structure with automatic resizing.

**Fields:**
- `char* data` - Character data pointer
- `usize length` - String length
- `usize capacity` - Allocated capacity

**Constructors:**
```d
std::string this(char* str)
std::string this(usize n)
```

## Methods

### add
```d
void add(char c)
```
Appends a character to the string.

### append
```d
void append(std::string str)
```
Appends another string.

### appendC
```d
void appendC(char* cstr, usize cstrlen)
void appendC(char* cstr)
void appendC(std::cstring cstr)
```
Appends C-string data.

### indexOf
```d
usize indexOf(char c)
usize indexOf(char* s)
usize indexOf(char* s, usize begin)
usize indexOf(std::string s)
```
Finds first occurrence of character or substring. Returns -1 if not found.

### substring
```d
std::string substring(usize from, usize to)
```
Extracts substring from index range.

### split
```d
std::vector<std::string> split(char ch)
```
Splits string by delimiter character.

### replace
```d
void replace(char c, char to)
std::string replace(char* from, char* to)
std::string replace(std::string from, std::string to)
```
Replaces characters or substrings.

### insert
```d
std::string insert(usize pos, char* str)
```
Inserts string at position.

### trim
```d
std::string trim()
std::string ltrim()
std::string rtrim()
```
Removes whitespace from both ends, left, or right.

### copy
```d
std::string copy()
```
Creates a copy of the string.

### set
```d
void set(usize index, char ch)
```
Sets character at index.

### at
```d
char at(usize index)
```
Gets character at index.

### setValue
```d
void setValue(char* a)
```
Replaces string content.

### hash
```d
uint hash()
```
Returns CRC32 hash of string.

## Conversion Methods

- `char toChar()` - First character
- `int toInt()` - Parse as integer
- `long toLong()` - Parse as long
- `uint toUint()` - Parse as unsigned int
- `ulong toUlong()` - Parse as unsigned long
- `cent toCent()` - Parse as 128-bit int
- `ucent toUcent()` - Parse as unsigned 128-bit int
- `double toDouble()` - Parse as double
- `char* c()` - Get C-string pointer
- `bool isDeleted()` - Check if string is deleted

## Operators

- `std::string + std::string` - Concatenation
- `std::string == std::string` - Equality comparison
- `std::string == char*` - Equality with C-string
- `std::string = char*` - Assignment from C-string

## Utility Functions

### std::string::fromNumber
```d
std::string fromNumber(cent n)
std::string fromNumber(long n)
std::string fromNumber(int n)
std::string fromNumber(short n)
std::string fromNumber(char n)
std::string fromNumber(bool n)
```
Converts signed numbers to strings.

### std::string::fromUNumber
```d
std::string fromUNumber(ucent n)
std::string fromUNumber(ulong n)
std::string fromUNumber(uint n)
std::string fromUNumber(ushort n)
std::string fromUNumber(uchar n)
```
Converts unsigned numbers to strings.

### std::string::fromDouble
```d
std::string fromDouble(double n)
std::string fromDoubleN(double n, int precision)
```
Converts double to string with optional precision.

### std::string::fromFloat
```d
std::string fromFloat(float n)
std::string fromFloatN(float n, int precision)
```
Converts float to string with optional precision.

### std::sprint
```d
std::string sprint(...)
```
Formats variadic arguments into a string.

### std::gsprint
```d
std::string gsprint(...)
```
Thread-safe global sprint using internal buffer.

## Example

```d
import <std/string>

std::string str = "Hello";
str.append(" World");
str.add('!');

std::string num = std::string::fromNumber(42);
int value = str.toInt();

std::vector<std::string> parts = str.split(' ');
std::string trimmed = str.trim();
```
