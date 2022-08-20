# Commands

## Inside-The-Loop
**break** - Stop the current loop.

## Outside-Of-Functions

**import** - Import a file.
To import files near the compiler (for example, std), use the prefix and postfix < and >.
Otherwise, put the file name in double quotes.
You do'nt need to specify the file extension.

Example:
```d
import <std/io>
import "specifications"
```

## Inside-Of-Functions

**while([cond]) [body/{body}]** - A block of code executed every time the conditions are true.

Example:
```d
int i = 0;
while(i<10) {i += 1;}
```

**[if/else] ([cond]) [body/{body}]** - A block of code executed if the conditions are true.

Example:
```d
if(A == B) {A = C;}
else {A = B;}
```

**itop([number])** - Getting a pointer to the address specified as a number.

**ptoi([pointer])** - Getting a numeric value (pointer address) using a pointer.
