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
    int a = 2 <. 2;
    int b = 16 >. 3;
} => 0;
```

## Flow Control Operator

The 'with' operator allows you to automatically release structures (of any kind - pointers or not) by calling their destructor after the end of the block. It can be overloaded like any other operator.

Examples:
```d
import <std/io>

struct A {
    A this => this;

    void test {

    }

    void ~with {
        std::println("Called by with!");
    }
}

void main {
    with(A a = A()) {
        /*
        After this block, '~with' or the destructor will be automatically called.
        Please note that after this block, the variable will be "deleted" from the public access.
        To avoid this, it is enough to pass only the name of the variable, and not its declaration.
        */
    }

    A b = A();
    with(b) {
        // After this block, the variable is not removed from public access.
    }

    A c = A();
    with(c.test()) {
        /*
        The compiler is able to automatically get the structure that you passed, even if you pass a method call. Please note that if your method has the type of a third-party structure, then the structure received from the method will be used in 'with', and not the root one.
        */
    }
}
```

## Slices

Slices are built-in language constructs that allow programmers to simplify working with arrays and pointers.

There are two types of slices - constant and non-constant.

Constant slices are generated using the operator '..'. They return a constant array with a length that is determined at compile time.

Non-constant arrays are generated using the '->' operator. They return a pointer to the memory allocated from the heap, allowing you to use non-const values to determine the edges of the slice.
They need to be cleaned up via std::free after the end of use.

If you try to put a value to a slice, the slice will reflect the interval of the pointer or array elements that will be set to a value from this array or pointer.

Examples:
```d
import <std/io>

void main {
    int[5] a = [0,5,10,15,20];

    int[3] b = a[0..3]; // Constant slice
    int* c = a[0->3]; // Non-constant slice
    std::free(c);

    int[4] d; // [0,0,0,0]
    d[0..2] = [5,10]; // Setting to 0 and 1 elements values from a constant array
}
```