## Aliases

Alias is a variable that has a value only in the form of a node. That is, when you create it, it is not generated, but its value is substituted instead of its name.

You can change them (but necessarily outside of the functions!).

```cpp
alias None = 0;
alias Three = 3;
alias Four = 4;

namespace A { // Works as an enum in C/C++
    alias One = 0;
    alias Two = 1;
}

int main {
    int result = 0;

    result += None; // 0
    result += A::One; // 0
    result += A::Two; // 1
    result += Three; // 3
    result += Four; // 6
} => result;
```