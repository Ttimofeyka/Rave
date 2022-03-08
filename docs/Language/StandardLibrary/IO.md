# Standard Library IO Interface

OS-independent interface for file-base input/output. In the header `:std/io`

## `std.File`

Represents an os-specific file stream. In the header `:std/io/file.epl`

<h3 id="--std.File.open"><code>static open(name: const char*, mode: const char*): File*</code></h3>

Opens a file based on a filename.

- `name`- the name of the file to open.
- `mode` - open mode, can be one of the following:
  - `r` - opens the file for reading, returns `nullptr` if the file doesn't exist.
  - `r+` - opens the file for reading and writing, returns `nullptr` if the file doesn't exist.
  - `w` - opens the file for writing, creating if it doesn't exist.
  - `w+` - opens the file for reading and writing, creating if it doesn't exist.
  - `a` - opens the file for appending, creating if it doesn't exist.
  - `b` can also be appended to the mode for reading/writing in binary mode instead of text mode.
- `File*` - a pointer to a- created file stream.


<h3 id="--std.File.close"><code>close(): void</code></h3>

Closes a file. Must be a valid file returned by [`File.open`](#--std.File.open)

```rust
@inc ":std/str"

@def  stdin_fd 0;
@def stdout_fd 1;
@def stderr_fd 2;

extern stdin: StdFile*;
extern stdout: StdFile*;
extern stderr: StdFile*;

struct StdFile {
    __fd: ulong;

    static extern open(path: const char*, mode: const char*): File*;
    extern close();

    extern read(to: char*, count: ulong);
    extern write(from: const char*, count: ulong);

    get(): char {
        c: char;
        this->read(&c, 1);
        ret c;
    }

    put(c: char) {
        this->write(&c, 1);
    }

    swrite(s: const char*) {
        this->write(&s, StdString.lengthOf(s));
    }
}
```
