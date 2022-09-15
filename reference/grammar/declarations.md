# Declarations

## Namespaces

Namespaces is a declaration that allows you to conveniently manage the prefixes of declarations.

To refer to a namespace member, you need to substitute its name, as well as "::".

Example:

```cpp
namespace Space {
    struct Earth {
        long peoples;
        int temperature;
    }
    
    struct Sun {
        int temperature;
    }
}

int main {
    Space::Earth earth;
    Space::Sun sun;
    
    ...
    
    return 0;
}
```

Also, they allow you to create equivalents of "static classes" from D and C++:

```cpp
namespace Earth {
    long peoples;
    int temperature;
    
    void initialize {
        Earth::peoples = ...;
        Earth::temperature = ...;
    }
    
    ...
}
```

## Functions and Variables

Functions and variables have one thing in common - parameters.

Parameters are set in parentheses before a function or variable.

List of possible declarations:

- C - Disables the mangling of a function or global variable.
- linkname: "name" - Allows you to manage the name of the imported function, leaving a new name for its management.
- vararg - Indicates that a function (or a pointer to a function) has no restrictions on the number of arguments.

Also, it should be noted that you cannot use the parameter C with linkname, since they interfere with each other.

Examples:

```d
(C) int a = 0;

namespace std {
    extern(linkname: "printf", vararg) int print();
}
```

## Functions

A function is a declaration that contains a block of code that can be called.

If the function has no arguments, parentheses after its name are optional.

Also, if the entire block of the function is the return of a value, you can use the => operator.

Examples:

```d
int sum(int a, int b) => a+b;

int main {
    return sum(1,1);
}
```

Overloading of functions is possible - it is performed in the "C-style".

```d
int sum(int a, int b) => a+b;
int sum(int a, int b, int c) => a+b+c; // sum3
```

In a normal call, you don't have to specify the number of arguments at the end of the overloaded function. However, for example, when setting a value using a pointer to a function, or passing a pointer to a function, you need to use the function name with the number of arguments at the end.

## Variables

Variables must have a type before their name.

Also, variables can be pointers to a function, having its type.

Examples:
```cpp
int a = 0;
char* b = "Hello, world!"

struct A {
    int first;
    int second;
}

A struc;

int func(int arg) => 0;

int main {
    int(int) f = func;
    f(1);
    return 0;
}
```

## Structures

Structures are types.

There can be variables and methods (functions) inside structures.

A structure can have a constructor (as in C++, D, JS and other languages).

Example:

```d
struct A {
    int a;
    int b;
    
    A this(int f, int s) {
        this.a = f;
        this.b = s;
    }
    
    void printAll {
        std::printf("%d\n%d\n",this.a,this.b);
    }
}
```
