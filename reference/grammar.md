# Grammar reference

## Declarations

All declarations of variables and functions must have a type.

They are denoted by `[extern or none] [decl]: [decltype] [...]`.

### Variables

Variables have the form `[varname]: [vartype] [; or [varvalue];]`.

Variables can be set to values in the form `[varname] = [varvalue];`.

### Functions

The functions have the form `[funcname] [[funcargs] or none]: [functype] [=> [funcblock]; or {[funcblock]}]`.

The arguments of the function have the form `([[argname]: [argtype] [,(if there is the next argument) or none] ...)`.

The function call has the form `[funcname][funcargsvalues]`.

Example:

    b(one: int): int => one + 5;
    main(args: char**, argc: int): int { ret b(1); }
    
### Namespaces

Namespaces have the form `namespace [nsname] { [nsblock] }`.

When using them, when accessing the function, you should add `[nsname]::`.

### Structures

The structures have the form `struct [structname] { [structblock] }`.

There may be variables inside the structure.

Example:

    struct A {
        a: int;
    }
    
## Control constructions and operators

### if/else

if/else allows you to operate with conditions.

if has the form `if([condition]) [{[ifblock]} or [lineifblock];]`, and else has the form `else [{[elseblock]} or [lineelseblock];]`.

Example:

    if(foo == 0) boo = 0;
    else if(foo == 1) { boo = 2; }
    else boo = 4;
    
### itop/ptoi

Itop/ptoi allow you to use pointer arithmetic.

Their form - `[itop/ptoi]([integer/pointer][(if itop)[,[ptrtype] or none] or none]) [expr]`.

### sizeof

Sizeof lets you know the exact size of a type or structure.

Sizeof form is `sizeof([type/structname])`.

### addr

Addr - taking the address of the variable.

Addr form is `addr([varname])`.

### ret

`ret` returns the value from the function.

`ret` form is `ret [expr];`.

### while

`while` executes the code block as long as the condition is true.

`while` form is `while([condition]) [{[whileblock]} or [linewhileblock];].

`while` can be stopped by the `break` command.

`break` form is `break;`.
