# Commands

## Inside-The-Loop
**break** - Stop the current loop.
**continue** - Go to the next iteration of the loop.

Example:
```d
int i = 0;
while(i<100) {
    if(i == 40) continue;
    else if(i == 67) break;
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

**while(cond) [body/{body}]** - A block of code executed every time the conditions are true.

Example:
```d
int i = 0;
while(i<10) {i += 1;}
```

**[if/else](cond) [body/{body}]** - A block of code executed if the conditions are true.

Example:
```d
if(A == B) A = C;
else {
    A = B;
}
```

**cast(type)expr** - Transformation of an expression from one type to another.

Example:

```d
float a = 0;
int b = cast(float)a;
```

## Built-In-Functions

**@baseType(type)** - Get the base type from the reduced type. If the type is not a pointer or an array, it is returned unchanged.

Example:

```d
macro new {
    return cast(@baseType(#0)*)std::malloc(sizeof(#0));
}
```

**va_arg(va_list, type)** - Get the next argument with the current type from va_list.

**@random(minimum, maximum)** - Get a random number at compile-time.

Example:

```d
import <std/io>

void main {
    std::println(@random(0,100));
}
```

**@sizeOf(type)** - Get the size of the type.

Example:

```d
int size = @sizeOf(int);
```

**@typesIsEquals(type1, type2), @typesIsNequals(type1, type2)** - Compare the two types with each other.

Example:

```d
@if(@typesIsEquals(int,char)) {
    int a = 0;
};
@if(@typesIsNequals(int,char)) {
    int a = 5;
};
```

**@isNumeric(type)** - Check whether the type is numerical.

Example:

```d
@if(@isNumeric(int)) {
    int a = 0;
};
@if(!@isNumeric(int)) {
    // F
};
```

**@contains(str1, str2)** - Check if str2 is in str1.

Example:

```d
// From std/prelude.rave
@if(@aliasExists(__RAVE_IMPORTED_FROM)) {
    @if(@contains(__RAVE_IMPORTED_FROM, "std/sysc.rave") == false) {
        import <std/sysc>  
    };
};
@if(!@aliasExists(__RAVE_IMPORTED_FROM)) {
    import <std/sysc>
};
```

**@compileAndLink(strings)** - Compile and add files to the linker without importing them.

Example:

```d
// From std/prelude.rave
@compileAndLink("<std/io>");
```

### Compile-time arguments

**@getCurrArg(type)** - Get the value of the current argument, leading to the required type.

Example:

```d
(ctargs) int sumOfTwo {
    int one = @getCurrArg(0, int);
    int two = @getArg(1, int);
} => one+two;
```

**@getCurrArgType()** - Get the type of the current argument.

Example:

```d
(ctargs) int bow {
    @if(@typesIsEquals(@getCurrArgType(), int)) {
        @echo("INT");
    };
}
```

**@getArg(n, type), @getArgType(n)** - Functions similar to the top two, except for the need to add an argument number.

**@typeToString(type)** - Convert the type to a string.

Example:

```d
const(char)* _int = @typeToString(int);
@if(_int == "int") {

};
```

**@aliasExists(name)** - Check if there is an alias with this name.

**@execute(string)** - Compile the code from a constant string.

Example:

```d
void main {
    int a = 0;
    @execute("a = 5;");
    // a = 5
}
```

**@error(strings/aliases), @echo(strings/aliases), @warning(strings/aliases)** - Output strings and/or aliases.

With @warning, only the color changes, but with @error, compilation ends.

Example:

```d
@echo("Hello, ","world!");
@warning("Warning.");
@error("Goodbye!");
```

**@setRuntimeChecks(true/false)** - Enable/disable runtime checks.

**@callWithArgs(args, functionName)** - Call a function with its own arguments and arguments when called.

Example:

```d
void two(int one, int _two) => one + _two;

(ctargs) void one {
    @callWithArgs(2, two);
}
```