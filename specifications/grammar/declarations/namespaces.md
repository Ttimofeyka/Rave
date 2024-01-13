# Namespaces

Namespaces is a declaration that allows you to conveniently manage the prefixes of declarations.

To refer to a namespace member, you need to substitute its name, as well as "::".

Example:

```cpp
namespace Space {
    struct Earth {
        long peoples;
        int temperature;
    }
    
    struct Sun {
        int temperature;

        Sun this(int base) {
            Space::Sun this;
            this.temperature = base;
        } => this;
    }
}

int main {
    Space::Earth earth;
    Space::Sun sun;
    
    ...
    
} => 0;
```

Also, they allow you to create equivalents of "static classes" from D and C++:

```cpp
namespace Earth {
    long peoples;
    int temperature;
    
    void initialize {
        Earth::peoples = ...;
        Earth::temperature = ...;
    }
    
    ...
}
```
