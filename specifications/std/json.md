# std/json.rave

## std::json::Value

This structure is a universal designation for any possible JSON value.

### makeBoolean, makeInteger, makeReal, makeString, makeArray, makeObject

These methods allow you to set the value of a given structure in JSON format using the desired type and desired value.

Example:

```d
std::json::Value foo;
foo.makeBoolean(true);

std::json::Value bow;
bow.makeInteger(1000);
```

### pointer

Returns a pointer for iteration (it can be useful if it is an object or an array).

Example:

```d
std::string buffer = "{\"foo\": 100, \"bow\": 200}"
std::json::Value test = std::json::parse(buffer);

std::json::Value* elements = test.pointer(); // Pointer with access to all elements by index

std::println("List of existing elements:");
for(int i=0; i<test.length; i++) std::println("\t", elements[i].name);
```

### as&lt;TYPE>

This method allows you to get the desired value from the JSON value in the type you need.

Supported types:
- char, uchar;
- short, ushort;
- int, uint;
- long, ulong;
- cent, ucent;
- half, bhalf;
- float, double, real;
- `char*`, `std::cstring`, `std::string`.

NOTE: if you are using `std::string`, it makes new allocation.

### indexOf

If the value is an object, it returns the index of the variable with the specified name in the pointer (if not found, it returns **-1**).

If the value is not an object, it returns **-1**.

### contains

This works like **indexOf**, but instead of an index it returns `true`/`false` (i.e. whether a variable with the same name has been found or not).

### get

Returns the JSON value of a variable with the specified name.

Example:

```d
std::string buffer = "{\"foo\": 100, \"bow\": 200}"
std::json::Value test = std::json::parse(buffer);

std::println("foo: ", test.get("foo").as<int>(), ", bow: ", test.get("bow").as<int>());
```

### set

Sets the JSON value to a variable. If there is no variable with that name, it creates a new one.

### setBoolean, setShort, setInteger, setLong, setDouble, setArray, setObject

It works as a set, but instead of `std::json::Value` accepts the usual value types.

### isVariable

Determines whether this value is a variable.

### toString

Returns the value in the string interpretation.

## std::json::parse

Parses the specified string, returning a JSON object.
