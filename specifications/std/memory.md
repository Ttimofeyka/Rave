# std/memory.rave

## std::malloc

Returns a pointer to the memory allocated on the heap.

Example:

```d
int* ptr = cast(int*)std::malloc(sizeof(int) * 8);
```

## std::free

Frees up the memory in the passed pointer.

Example:

```d
char* ptr = std::malloc(64);
std::free(ptr);
```

## std::realloc

Returns the pointer to the memory with the new size.

Example:

```d
long* foo = cast(long*)std::malloc(128);
std::realloc(cast(void*)foo, 256);
```

## std::calloc

Returns a pointer to the allocated memory with the size as a multiplication of two numeric arguments (count and size of each).

Example:

```d
void* bow = std::calloc(64, 2); // 64 length, 2 each => 128 size
```

## std::memcpy, std::memmove, std::memcmp, std::memset

These functions are designed to work with pointers in the same way as in C.

**std::memcpy** - Copying bytes from one pointer to another.

**std::memmove** - Moving bytes from one pointer to another.

**std::memcmp** - Comparing pointers by a certain number of bytes.

**std::memset** - Setting a certain value for a certain number of bytes in the pointer.

Example:

```d
void* ptr1 = std::malloc(4);
void* ptr2 = std::malloc(7);

for(int i=0; i<4; i++) ptr1[i] = '0';
for(int i=0; i<7; i++) ptr2[i] = '1';

// ptr1: 0 0 0 0
// ptr2: 1 1 1 1 1 1 1

std::memcpy(ptr1, ptr2, 2); // ptr1: 1 1 0 0

if(std::memcmp(ptr1, ptr2, 2)) {
    // True (1 1 with 1 1)
    std::memset(ptr2, 0, 7); // ptr2: 0 0 0 0 0 0 0
}
```

## std::swap

Swap a certain number of bytes in both pointers.

Example:

```d
void* ptr1 = std::malloc(2);
void* ptr2 = std::malloc(2);

for(int i=0; i<2; i++) {
    ptr1[i] = 'A';
    ptr2[i] = 'B';
}

// ptr1: A A
// ptr2: B B

std::swap(ptr1, ptr2, 2);

// ptr1: B B
// ptr2: A A
```

## std::new

It has the same return value as **std::malloc**, but it immediately returns the required pointer type and allows you to specify the number of such elements.

Example:

```d
// malloc version
int* ptr1 = cast(int*)std::malloc(4);

// std::new version
int* ptr2 = std::new<int>();
```

## std::pair

This structure allows you to combine any two types of data into one.

Example:

```d
std::pair<float, char> foo = std::pair<float, char>(0f, 10c);

foo.first += 10f;
foo.second -= 10c;
```
