# std/system.rave

## std::system::getCwd

Copies the current path to the working directory to the specified buffer with the specified length.

Example:

```d
char* buffer = std::new<char>(256);
std::getCwd(buffer, 256);
```