# std/io.rave

## print, println

Functions for text output to the console.

Example:

```d
std::print("Hello, world!\n");
// Or
std::println("Hello, world!");
```

## fprint, fprintln

Functions for text output to the file.

Example:

```d
std::file f = std::file("foo.txt");
f.open("w");
    std::fprint(&f, "Hello, world!\n");
    // Or
    std::fprintln(&f, "Hello, world!");
f.close();
```

## input

A function for reading a certain simple type from the console.

Example:

```d
int foo;
std::input(&foo);
```

## finput (TEMPORARILY EXCLUDED)

A function for reading a certain simple type from the file.

Example:

```d
std::file f = std::file("foo.txt");
f.open("r");
std::finput(&f, &foo);
f.close();
```

## readLine

A function for reading a line from the console.

Example:

```d
std::string str = std::readLine();
defer ~str;
```

## std::file

### open

The method for opening the file.

### close

The method for closing the file.

### rename

The method for renaming the file.

Example:

```d
std::file f = std::file("test.txt");
f.rename("test2.txt");
```

### exists

A method for checking the existence of a file.

Example:

```d
std::file f = std::file("test.txt");
if(f.exists()) {
    // ...
}
```

### readLine

The method for reading a line from the file.

Example:

```d
std::file f = std::file("test.txt");
f.open("r");
std::string line = f.readLine();
f.close();
```

### readAll

The method for reading all content from the file.

Example:

```d
std::file f = std::file("test.txt");
f.open("r");
std::string buffer = f.readAll();
defer ~buffer;
f.close();
```

### readLineBuffer, readAllBuffer

The call is equivalent to calling functions without a 'Buffer', but instead of a new buffer, the buffer from the argument is used.

### seek

The method for moving the current file position.

A first argument must be a flag: `std::file::position::start` (start of the file), `std::file::position::current` (current position) or `std::file::position::end` (end of the file).

Example:

```d
std::file f = std::file("foo.x");
f.open("r");

std::string buffer = f.readAll(); // Reading all file content
defer ~buffer;

f.seek(std::file::position::start, 0); // Now we again in the start of the file

std::string buffer2 = f.readAll();
defer ~buffer2;

f.close();
```
