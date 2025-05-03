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
} => (a + b * c / d);
```

Division by zero is **undefined behavior** regardless of the types of expression.

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
    int d = 5 & 5; // AND
    int e = 10 || 10; // OR
    int c = 10 !! 5; // XOR
} => 0;
```

## 'in', '!in' operators

The `in` and `!in` operators have identical functions, as in many new programming languages.
They let you know if it is present (or absent) whether the element is in the specified array (or an overloaded structure).

Examples:

```d
import <std/io>

struct Jam {
    int[2] m;

    Jam this {
        Jam this;
        this.m[0] = 10;
        this.m[1] = 200;
    } => this;

    bool operator in(Jam j, int value) => value in j.m;
}

void main {
    Jam foo = Jam();

    if(10 in [10, 20, 30]) std::println("TRUE"); // TRUE

    if(20 in foo) std::println("TRUE2"); // Nothing
    else if(200 in foo) std::println("TRUE3"); // TRUE3

    if(20 !in foo) std::println("FALSE"); // FALSE
}
```
