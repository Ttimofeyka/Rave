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

## finput

A function for reading a certain simple type from the file.

Example:

```d
std::file f = std::file("foo.txt");
f.open("r");
std::finput(&f, &foo);
f.close();
```

## readLine/u8ReadLine

A function for reading a line from the console.

Example:

```d
std::string str = std::readLine();
std::u8string str2 = std::u8ReadLine();
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

### readAll/u8ReadAll

The method for reading all content from the file. u8ReadAll uses `std::u8string`.

Example:

```d
std::file f = std::file("test.txt");
f.open("r");
std::string line = f.readAll();
f.close();
```

