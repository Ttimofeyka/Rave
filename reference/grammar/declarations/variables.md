# Variables

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

## Variables in compile-time

##### [Aliases](declarations/aliases.md)