# Functions

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

Overloading of functions is possible - it is similar to the variant in C++:

```d
int sum(int a, int b) => a+b;
float sum(float a, float b) => a+b;
```

You can call a global function as a method from a variable:

```d
int inc(int a) => a+1;

void main {
    int foo = 0;

    foo = foo.inc();
}
```
Thanks to this, you can not use methods at all.

## Parameters

###### [Parameters](declarations/funcvarparams.md)

## Methods

##### [Structures](declarations/structures.md)