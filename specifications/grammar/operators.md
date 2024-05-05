# Operators

## Mathematical and assignment

As in all C-like languages, there is an addition, subtraction, multiplication and division operator.
Also, there is an assignment operator and associated shortened mathematical operators.

Examples:
```d
int main {
    int a = 0;
    int b = 5 + 4;
    int c = 3 * 7;
    int d = 6 / 2;

    int e = (a % 2);

    a -= b;
    c += d;
} => (a+b-c-d);
```

## Getting a pointer to an element

The & operator is used to get a pointer to a variable, function, or structure element.

Examples:
```d
int main {
    int a = 0;
    int* b = &a;
    b[0] = 5; // a = 5
} => 0;
```

## Getting a value by pointer

The * operator (placed before the identifier) allows you to get the first element of the pointer in a shorter form.

Examples:
```d
int getValByPtr(int* a) => *a;

int main {
    int a = 123;
    int* b = &a;
} => getValByPtr(b);
```

## Comparison operators

Everything is exactly like in C-like programming languages.

Examples:
```d
int main {
    int a = 0;
    int b = 2;

    bool c = (a >= b) && (b != 0);
} => 0;
```


## Bitwise operators

Examples:
```d
int main {
    int a = 2 <. 2; // Left shift
    int b = 16 >. 3; // Right shift
    int d = 5 && 5; // AND
    int e = 10 || 10; // OR
    int c = 10 !! 5; // XOR
} => 0;
```
