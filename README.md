<h1 align="center">Rave Language</h1>

Rave is a statically typed, compiled, procedural, general-purpose programming language.

## "Hello, world!" Example

```nasm
import <std/io>

void main {
    std::println("Hello, world!");
}
```

You can compile are examples using `rave directory/of/Rave/examples/necessary_example.rave -o preffered/directory/necessary_example`

## Dependencies

* `llvm-15`
**You can also use LLVM from 11 to 14.**
* `clang` or `gcc`
* Make
* mingw (if you need cross-compilation or you are using Windows)

## Building/Running

To install dependencies, you can try running `install.sh` (Arch Linux/Ubuntu/Debian) or `install.bat` (only Windows 64-bit using [choco](https://chocolatey.org))

If the installer does not work well on your system, you can try to install all the dependencies yourself.

After install write `make` in the Rave directory.

### Cross-compilation programs from Linux for Windows

You just need to set the compiler "i686-w64-mingw32-gcc-win32" in options.json, and add "-t i686-win32" to your build command.

### Building in Termux

1. Install llvm-15 from [Termux User Repository](https://github.com/termux-user-repository/tur):
```bash
$ pkg i llvm-15
```
2. Apply termux-specific patch for LLVM:
```bash
$ ./llvm-patch
```
3. Build using `make`.

## Specifications

The specifications is in `specifications` directory.

<a href="https://github.com/Ttimofeyka/Rave/blob/main/bindings.md">Bindings</a>

<a href="https://discord.gg/AfEtyArvsM">Discord</a>

<a href="https://ravelang.space">Web-site</a>
