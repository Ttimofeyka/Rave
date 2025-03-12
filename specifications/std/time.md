# std/time.md

## std::getCurrentTime

Sets the `std::time` structure (by the pointer in the first argument) to the value of the current time.

Example:

```d
std::time time;

std::getCurrentTime(&time);

std::println(time);
```

## std::sleep

Stops the current thread for a while (in milliseconds).

Example:

```d
std::time(100); // Sleep for 100 ms
```
