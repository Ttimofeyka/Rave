# Parameters of functions and variables

Functions and variables have one thing in common - parameters.

Parameters are set in parentheses before a function or variable.

List of possible declarations:

- C - Disables the mangling of a function or global variable.
- linkname: "name" - Allows you to manage the name of the imported function, leaving a new name for its management.
- vararg - Indicates that a function (or a pointer to a function) has no restrictions on the number of arguments.
- volatile - Informing the compiler that the value of a variable can change from the outside.
- fastcc and coldcc - Denote the type of call agreement. In both cases, you cannot use 'vararg'. The 'fastcc' call convention means that all arguments will be passed (if possible) in registers for acceleration. 'coldcc' means that all arguments will be passed on the stack. Works only for functions.
- pure - Makes the function "clean". A pure function can't change global variables.
- nochecks - Disables all built-in checks in the function.
- private - This function/variable will not be imported from other files.
- noOptimize - Forces the compiler to bypass the function during optimization.

Also, it should be noted that you cannot use the parameter C with linkname, since they interfere with each other.

Examples:

```d
(C) int a = 0;

namespace std {
    extern(linkname: "printf", vararg) int print(char* fmt);
}
```
