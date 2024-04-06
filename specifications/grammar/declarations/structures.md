# Structures

Structures are a full-fledged type that has a designation like class in C++.

A structure can have a constructor, a destructor, methods, and operator overloading.

Example:

```d
struct A {
    int a;
    int b;
    
    A this(int f, int s) { // Constructor
        /*
        Control over memory allocation is entirely in the hands of the author of the structure.
        In our case, memory is allocated on the stack.
        */
        A this;
        this.a = f;
        this.b = s;
    } => this; // We have to return the value from the constructor, as in all other functions.
    
    void ~this { // Destructor
        /*
        The destructor must be of type void and have no arguments. This won't be an error, but the compiler will automatically remove all arguments if there are any.

        If there is no destructor in the function, it is generated automatically.
        */

        // ...
    }

    void printAll {
        std::println(this.a,this.b);
    }

    void operator=(A* pointer, int[2] values) {
        /*
        Overloading of the '=' operator. The first argument must have a pointer type to this structure (or a pointer type to a pointer if the structure is in heap).
        */

        pointer.a = values[0];
        pointer.b = values[1];
    }
}
```