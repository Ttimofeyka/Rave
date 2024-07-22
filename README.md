<center>
<h1>The Rave Programming Language</h1>
<br/>
<a href="https://github.com/Ttimofeyka/Rave/releases/latest">
    <img src="https://img.shields.io/github/v/release/Ttimofeyka/Rave.svg">
</a>
</center>

Rave is a statically typed, compiled, procedural, general-purpose programming language.

## "Hello, world!" Example

```nasm
import <std/io>

void main {
    std::println("Hello, world!");
}
```

## Advantages

* Fast compilation
* Cross-platform features (for example, working with threads)
* Support for many platforms as target
* Using LLVM for great optimizations

For maximum performance, use the `-Ofast` or `-O3 --noChecks`. Also, don't forget to compile std with these flags using `--recompileStd -Ofast`.

## Dependencies

* `llvm-16`
**You can also use LLVM from 11 to 15.**
* `clang` or `gcc`
* Make
* MinGW (if you need cross-compilation or you are using Windows)

## Building/Running

To install dependencies, you can try running `install.sh` (Arch Linux/Void Linux/Ubuntu/Debian) or `install.bat` (only Windows 64-bit using [choco](https://chocolatey.org))

If the installer does not work well on your system, you can try to install all the dependencies yourself.

After install write `make` in the Rave directory.

You can compile, for example, "Hello world!" example using `./rave examples/hello_world.rave -o hello_world` in directory with Rave.

### Cross-compilation programs from Linux for Windows

You just need to set the compiler "i686-w64-mingw32-gcc-win32" in options.json, and add "-t i686-win32" to your build command.

### Building in Termux

1. Install llvm-16 from [Termux User Repository](https://github.com/termux-user-repository/tur):
```bash
$ pkg i llvm-16
```
2. Apply termux-specific patch for LLVM:
```bash
$ ./llvm-patch.sh
```
3. Build using `make`.

## Specifications

The specifications is in `specifications` directory.

<a href="https://github.com/Ttimofeyka/Rave/blob/main/bindings.md">Bindings</a>

<a href="https://discord.gg/AfEtyArvsM">Discord</a>

<a href="https://ravelang.space">Web-site</a>
