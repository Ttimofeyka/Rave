<h1 align="center">The Rave Programming Language</h1>
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
* `clang++` or `g++`
* Make
* MinGW (if you need cross-compilation or you are using Windows)

## Building/Running

To install dependencies, you can try running `install.sh` (Arch Linux/Void Linux/Ubuntu/Debian) or `install.bat` (only Windows 64-bit using [choco](https://chocolatey.org))

If the installer does not work well on your system, you can try to install all the dependencies yourself.

After install write `make` in the Rave directory.

You can compile, for example, "Hello world!" example using `./rave examples/hello_world.rave -o hello_world` in directory with Rave.
To run this example after compiling, try `./examples/hello_world`.

### Cross-compilation programs from Linux for Windows

You just need to set the compiler "i686-w64-mingw32-gcc-win32" in options.json, and add "-t i686-win32" to your build command.

## Specifications

The specifications is in `specifications` directory - [link](https://github.com/Ttimofeyka/Rave/blob/main/specifications/intro.md).

## Troubleshooting errors

### Segmentation fault during compile-time

Most often, this error occurs when using incorrect syntax or builtin instructions with incorrect arguments.
We try to keep these cases to a minimum, but they may still remain.

### SSE/SSE2/SSE3/AVX as not a recognized features

If you have this kinda logs from compiler:

```d
'+sse' is not a recognized feature for this target (ignoring feature)

'+sse2' is not a recognized feature for this target (ignoring feature)

'+sse3' is not a recognized feature for this target (ignoring feature)

'+avx' is not a recognized feature for this target (ignoring feature)
```

You can just disable SSE/SSE2/SSE3/AVX into options.json or using command line options (-noSSE, -noSSE2, -noSSE3, -noAVX).

## Useful links

<a href="https://github.com/Ttimofeyka/Rave/blob/main/bindings.md">Bindings</a>

<a href="https://discord.gg/AfEtyArvsM">Discord</a>

<a href="https://ravelang.space">Web-site</a>