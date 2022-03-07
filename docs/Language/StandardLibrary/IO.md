# Standard Library IO Interface

OS-independent interface for file-base input/output. In the header `:std/io`

## `std.File`

Represents an os-specific file stream. In the header `:std/io/file.epl`

#### `static open(name: const char*): File*`

Opens a file based on a filename.

- `name`- the name of the file to open.
- `File*` - a pointer to a created file stream.
