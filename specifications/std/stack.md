# std/stack

Generic stack (LIFO) data structure with dynamic resizing.

## Types

### std::stack<T>

Generic stack structure.

**Conditions:** T must not be void.

**Fields:**
- `T* data` - Stack data array
- `usize capacity` - Allocated capacity in bytes
- `usize length` - Number of elements

## Methods

### Constructor
```d
std::stack<T> this()
std::stack<T> this(T* data, usize size)
```
Creates empty stack or initializes from array.

### push
```d
void push(T value)
```
Pushes value onto stack. Automatically resizes when full.

### pop
```d
T pop()
```
Removes and returns top element. Returns null if empty.

### top
```d
T top()
```
Returns top element without removing. Returns null if empty.

### clear
```d
void clear()
```
Removes all elements and shrinks capacity if large.

### copy
```d
std::stack<T> copy()
```
Creates a copy of the stack.

### assign
```d
void assign(std::stack<T> of)
```
Appends all elements from another stack.

### ptr
```d
T* ptr()
```
Returns pointer to internal data array.

### hash
```d
uint hash()
```
Returns hash value of the stack.

## Operators

- `std::stack<T> + std::stack<T>` - Concatenates two stacks

## Example

```d
import <std/stack>

std::stack<int> s = std::stack<int>();
s.push(10);
s.push(20);
s.push(30);

int top = s.top();
int popped = s.pop();

s.clear();
```
