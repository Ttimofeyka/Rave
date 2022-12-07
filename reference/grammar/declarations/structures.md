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
        There is no concept of 'new Struct()' in Rave: the control on whether a pointer to a structure is a list or not lies entirely with the author of the structure.

        This may seem inconvenient, but it increases the efficiency of the constructor quite a lot.
        
        In this example, we return a direct value in the form of a structure from the stack.
        */

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

    void ~with { // Overloading the with operator
        // ...
    }

    void printAll {
        std::printf("%d\n%d\n",this.a,this.b);
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