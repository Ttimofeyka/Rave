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