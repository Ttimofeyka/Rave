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
    int a = 2 << 2;
    int b = 16 >> 3;
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
        std::printf("Called by with!");
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