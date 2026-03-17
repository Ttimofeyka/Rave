# std/sort

Sorting algorithms for arrays.

## Functions

### sort
```d
void sort<T>(T* ptr, usize length)
```

Sorts array in-place using quicksort algorithm.

**Parameters:**
- `ptr` - Pointer to array
- `length` - Number of elements

**Requirements:**
- Type T must support comparison operator `<`
- Array must not be null

## Example

```d
import <std/sort>

int[5] numbers = [5, 2, 8, 1, 9];
std::sort<int>(&numbers, 5);
// Result: [1, 2, 5, 8, 9]
```
