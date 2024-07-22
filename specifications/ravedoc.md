# RaveDoc

This section describes the documentation specification for each Rave element.

## Function (includes methods too)

```d
/**
* Sum of two numbers.
*
* @param a First number
* @param b Second number
* @return Sum of a and b
* @deprecated This method is deprecated, use ... instead
* @todo This is not finished yet
* @noimpl This is a stub that has no implementation
*/
int sum(int a, int b) => a + b;
```

### Constructor/destructor

```d
struct A {
    void* ptr;

    /**
    * Constructor of A.
    *
    * @param size Size of new pointer
    * @return New A instance
    * @constructor
    * @deprecated This constructor is deprecated, use ... instead
    * @todo This is not finished yet
    * @noimpl This is a stub that has no implementation
    */
    A this(int size) {
        A this;
        this.ptr = std::malloc(size);
    } => this;

    /**
    * Destructor of A.
    *
    * @destructor
    * @deprecated This destructor is deprecated, use ... instead
    * @todo This is not finished yet
    * @noimpl This is a stub that has no implementation
    */
    void ~this {
        if(this.ptr != null) std::free(this.ptr);
    }
}
```

## Namespace

```d
/**
* Contains elements of the standard library.
*
* @deprecated This namespace is deprecated, use ... instead
* @todo This is not finished yet
* @noimpl This is a stub that has no implementation
*/
namespace std {
    // ...
}
```

## Structure

```d
/**
* Simple pair of two elements.
*
* @type K1 Type of the first element
* @type K2 Type of the second element
* @deprecated This structure is deprecated, use ... instead
* @todo This is not finished yet
* @noimpl This is a stub that has no implementation
*/
struct pair<K1, K2> {
    K1 first;
    K2 second;
}
```