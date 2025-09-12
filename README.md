<h1 align="center">The Rave Programming Language</h1>
<p align="center">Rave is a statically typed, compiled, procedural, general-purpose programming language.</p>
<p align="center">
<a href="https://github.com/Ttimofeyka/Rave/releases/latest">
<img src="https://img.shields.io/github/v/release/Ttimofeyka/Rave.svg" alt="Latest Release">
</a>
<br>
<a href="https://discord.gg/AfEtyArvsM">
<img src="https://img.shields.io/discord/872555146968698950?color=7289DA&label=Discord&logo=discord&logoColor=white" alt="Discord">
</a>
</p>
<br/>

## Example of "Hello, world!"

### Reduced version

```d
import <std/io>
void main => std::println("Hello, world!");
```

### Expanded version

```d
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

For maximum performance, use the `-Ofast` or `-O3 --noChecks`. Also, don't forget to compile standard library with these flags using `--recompileStd -Ofast`.

## Dependencies

* `llvm`
**You can use LLVM from 14 to 21.**
* `clang` or `gcc`
* C++ compiler (with support of C++17 and higher)
* Make
* MinGW (if you need cross-compilation or you are using Windows)

## Building/Running

To install dependencies, you can try running `install.sh` (Arch Linux/Void Linux/Ubuntu/Debian/Fedora/RHEL) or `install.bat` (only Windows 64-bit using [choco](https://chocolatey.org))

If the installer does not work well on your system, you can try to install all the dependencies yourself.

After install write `make` in the Rave directory.

You can compile, for example, "Hello world!" example using `./rave examples/hello_world.rave -o hello_world` in directory with Rave.
To run this example after compiling, try `./hello_world`.

### Cross-compilation programs from Linux for Windows

You just need to set the compiler "i686-w64-mingw32-gcc-win32" in options.json, and add "-t i686-win32" to your build command.

## Specifications

The specifications is in `specifications` directory - [link](https://github.com/Ttimofeyka/Rave/blob/main/specifications/README.md).

## Troubleshooting errors

### Segmentation fault during compile-time

Often caused by incorrect syntax or misuse of builtin instructions.
We are continuously working to minimize these occurrences.

## Useful links

<a href="https://github.com/Ttimofeyka/Rave/blob/main/bindings.md">Bindings</a>

<a href="https://discord.gg/AfEtyArvsM">Discord</a>

<a href="https://ravelang.xyz">Web-site</a>
