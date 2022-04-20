This file will contain documentation for the `std/io`.

<h3 id="--std/io/fopen"><code>fopen(filename: char*, flags: int, mode: int): int</code></h3>

Opens the file, returning its handle.

+ `filename` - The name of the file;
+ `flags` - Flags for opening.
+ `mode` - Opening mode.

<h3 id="--std/io/fclose"><code>fclose(fd: int)</code></h3>

Closes the file by the specified handle.

+ `fd` - File descriptor.

<h3 id="--std/io/fwrite"><code>_fwrite(fd: int, buff: char*, size: int)</code></h3>

"Naked" text input to the file.

+ `fd` - File descriptor.
+ `buff` - The buffer of the input text.
+ `size` - The size of the input text.

<h3 id="--std/io/fread"><code>fread(fd: int, buff: char*, size: int): int</code></h3>

Reading text from a file.

+ `fd` - File descriptor.
+ `buff` - The buffer of the output text.
+ `size` - The size of the text being read.


<h3 id="--std/io/mkdir"><code>mkdir(pathname: char*, mode: int)</code></h3>

Creating a directory.

+ `pathname` - The name of the directory.
+ `mode` - Creating mode.


<h3 id="--std/io/rmdir"><code>rmdir(pathname: char*)</code></h3>

Deleting a directory.

+ `pathname` - The name of the directory.

<h3 id="--std/io/chmod"><code>chmod(file: char*, mode: int)</code></h3>

Change file permissions.

+ `file` - The name of the file.
+ `mode` - New permissions for the file.
