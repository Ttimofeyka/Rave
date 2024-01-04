# std/vector.rave

## std::vector

### add

Add an element to the vector.

Example:

```d
auto vec = std::vector<int>();
vec.add(10);
~vec;
```

### remove, removeLast

Remove a certain (or last) element from the vector.

Example:

```d
auto vec = std::vector<int>();
vec.add(10);
vec.add(20);
vec.add(30);
vec.remove(0); // 20, 30
vec.removeLast(); // 20
~vec;
```

### copy

Create a copy of the vector.

Example:

```d
auto vec = std::vector<int>();
auto vec2 = vec.copy();
~vec;
~vec2;
```

### assign

Add elements of another vector to the vector.

Example:

```d
auto vec = std::vector<int>();
auto vec2 = std::vector<int>();
vec.add(10);
vec2.add(20);
vec.assign(vec2); // 10, 20
~vec;
~vec2;
```

### set

Set the value of the vector element.

Example:

```d
auto vec = std::vector<int>();
vec.add(10);
vec.add(20);
vec.set(0, 100); // 100, 20
~vec;
```

### swap

Swap two values by indexes;

Example:

```d
auto vec = std::vector<int>();
vec.add(100);
vec.add(200);
vec.swap(0, 1); // 100, 200 -> 200, 100
~vec;
```

### + operator

Allows to combine two vectors to the new copy.

Example:

```d
auto vec1 = std::vector<int>();
vec1.add(10);
auto vec2 = std::vector<int>();
vec2.add(20);
std::vector<int> vec3 = vec1 + vec2; // 10, 20
```

### [] operator

Allows to get an element from a vector by index.

Example:

```d
auto vec = std::vector<int>();
vec.add(100);
int element = vec[0]; // 100
```
