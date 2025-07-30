# Commands

## Inside-The-Loop
**break** - Stop the current loop.
**continue** - Go to the next iteration of the loop.

Example:
```d
int i = 0;
while (i < 100) {
    if (i == 40) continue;
    else if (i == 67) break;
    i += 1;
}
```

## Outside-Of-Functions

**import** - Import a file.
To import files near the compiler (for example, std), use the prefix and postfix < and >.
Otherwise, put the file name in double quotes.
You don't need to specify the file extension.

Example:
```d
import <std/io> <std/locale>
import "specifications"
```

## Inside-Of-Functions

**while (cond) [body/{body}]** - A block of code executed every time the conditions are true.

Example:
```d
int i = 0;
while (i < 10) {i += 1;}
```

**[if/else] [likely/unlikely] (cond) [body/{body}]** - A block of code executed if the conditions are true.

Examples:

```d
if (A == B) A = C;
else {
    A = B;
}
```

```d
if likely(A == B) A = C;
else A = B;
```

**for(var; cond; expr) [body/{body}]** - The for loop is like in C, creating a local variable and executing the expression after the block and the block itself if the condition is met.

Example:
```d
for(int i=0; i<100; i++) {
    // ...
}

for(int j=100; j>0; j--) {
    // ...
}

for(short z=0; z<20; z+=2) {
    // ...
}
```

Also, you can use a **for(;;)** expression for the infinite loop.

Example:

```d
while (true) {

}

// Or

for(;;) {
    
}
```

**foreach(varElement; var) [body/{body}]** or **foreach(varElement; varWithData; varWithLength) [body/{body}]** - the for version, created as a result of new trends in reducing unnecessary constructions.

Example:
```d
std::vector<int> vint = std::vector<int>();

vint.add(10);
vint.add(20);
vint.add(30);

foreach(number; vint) std::println(number);

// Or

foreach(number; vint.data; vint.length) std::println(number);
```

**switch(expr) {case(expr) {} default {}}** - It works like a switch in C, except for one thing - break and continue are prohibited in switch.

Example:
```d
int n;
std::input(&n);

switch(n) {
    case(10) std::println("10");
    case(20) {
        std::println("20!");
        std::exit(1);
    }
    default std::println("Other");
}
```

**cast(type)expr** - Transformation of an expression from one type to another.

Example:

```d
float a = 0;
int b = cast(int)a;
```

**bitcast(type)expr** - Bit-level reinterpretation of an expression to another type without changing the underlying bits.

Example:

```d
float a = 3.14f;
int b = bitcast(int)a; // Reinterprets the bits of 'a' as an int
```

**defer [body/{body}]** - Generate a body before current block end. If there is no block, the behavior is like fdefer.

Example:

```d
int x;
std::input(&x);

if (x == 0) {
    void* ptr = std::malloc(32);
    defer std::free(ptr);

    // Some work with ptr...
    ptr[0] = 'R';
    ptr[1] = 'a';
    ptr[2] = 'v';
    ptr[3] = 'e';
}
```

**fdefer [body/{body}]** - Generate a body before returning from a function.

Example:

```d
void* ptr = std::malloc(32);
fdefer std::free(ptr);

// Some work with ptr...
ptr[0] = 'R';
ptr[1] = 'a';
ptr[2] = 'v';
ptr[3] = 'e';
```

## Builtin functions

### [SIMD](./simd.md)

**@sizeOf(type)** - Get the size of the type (in bytes).

Example:

```d
int size = @sizeOf(int); // 4
```

**@baseType(type)** - Get the base type from the reduced type. If the type is not a pointer or an array, it is returned unchanged.

Example:

```d
import <std/io>

void main {
    std::println(@sizeOf(@baseType(int*))); // 4
}
```

**@isNumeric, @isStructure, @isPointer, @isVector, @isArray (type)** - Check whether the type is numerical, pointer, SIMD vector, array or structure.

Example:

```d
struct A {}

@if (@isNumeric(int)) {
    // ...
}

@if (@isPointer(int*)) {
    // ...
}

@if (@isArray(int[1])) {
    // ...
}

@if (@isVector(float4)) {
    // ...
}

@if (@isStructure(A)) {
    // ...
}
```

**@contains(str1, str2)** - Check if str2 is in str1.

Example:

```d
// Similar to std/prelude.rave
@if (@aliasExists(__RAVE_IMPORTED_FROM)) {
    @if (!(@contains(__RAVE_IMPORTED_FROM, "std/sysc.rave"))) import <std/sysc>  
}

@if (!@aliasExists(__RAVE_IMPORTED_FROM)) import <std/sysc>
```

**@compileAndLink(strings)** - Compile and add files to the linker without importing them.

Example:

```d
// From std/prelude.rave
@compileAndLink("<std/io>");
```

**@typeToString(type)** - Convert the type to a string.

Example:

```d
@if (@typeToString(int) == "int") {}
```

**@aliasExists(name)** - Check if there is an alias with this name.

**@error(strings/aliases), @echo(strings/aliases), @warning(strings/aliases)** - Output strings and/or aliases.

With @warning, only the color changes, but with @error, compilation ends.

Example:

```d
@echo("Hello, ","world!");
@warning("Warning.");
@error("Goodbye!");
```

**@callWithArgs(functionName, args)** - Call a function with its own arguments and arguments when called.

**@callWithBeforeArgs(functionName, args)** - Call a function with its own arguments (after arguments when called).

Example:

```d
void two(int one, int _two) => one + _two;

(ctargs) void one {
    @callWithArgs(two, 2);
    @callWithBeforeArgs(two, 2);
}
```

**@hasMethod(type, methodName)** - returns true if a structure with a method from the second argument is passed to the first argument.

**@hasDestructor(type)** - returns true if a structure with a destructor is passed to the first argument.

Example:

```d
struct Example {
    Example this {Example this;} => this;

    void foo {}
    int bow => 0;
}

void main {
    auto ex = Example();

    @if (@hasMethod(Example, foo)) {
        if (@hasMethod(Example, bow)) {
            // Has foo and bow
            if (@hasDestructor(Example)) {
                // Has destructor
            }

            if (!@hasDestructor(Example)) {
                // Do not has destructor
            }
        }
    }
}
```

**@getLinkName** - returns the name of the function/variable for the linker.

Example:

```d
import <std/io>

int sum(int x, int y) => x + y;

void main {
    std::println(@getLinkName(sum)); // _RaveF3sum
}
```

### Compile-time arguments

**@getCurrArg(type)** - Get the value of the current argument, leading to the required type.

Example:

```d
(ctargs) int sumOfTwo {
    int one = @getCurrArg(int);
    int two = @getArg(1, int);
} => one + two;
```

**@skipArg()** - Skip the current argument.

Example:

```d
(ctargs) int foo {
    @skipArg(); // skip the first argument
    int second = @getCurrArg(int); // getting the second argument as integer
} => second / 2;
```

**@getCurrArgType()** - Get the type of the current argument.

Example:

```d
(ctargs) int bow {
    @if (@getCurrArgType() == int) @echo("Integer");
}
```

**@foreachArgs() {block};** - Generate code for each argument at compile-time.

Example:

```d
(ctargs) int sum {
    int result = 0;

    @foreachArgs() {
        result += @getCurrArg(int);
    }
} => result;
```

**@getArg(n, type), @getArgType(n)** - Functions similar to the top two, except for the need to add an argument number.

Example:

```d
(ctargs) int sum2 {
    @if (@tNequals(@getArgType(0), @getArgType(1))) @error("Different types!");
} => @getArg(0, int) + @getArg(1, int);
```

**@argsLength()** - Get the number of compile-time arguments.

Example:

```d
(ctargs) int sum0to2 {
    @if (@argsLength() == 0) return = 0;
    @else @if (@argsLength() == 1) return = @getArg(int, 0);
    @else @if (@argsLength() == 2) return = @getArg(int, 0) + @getArg(int, 1);
}
```

**@alloca(n)** - Get a pointer to the memory allocated with size n in the stack.

Example:

```d
void main {
    void* ptr = @alloca(4);
}
```
