 # Lexer Reference

### Supported encodings

The source code can have the following encodings:

- UTF-8;
- ASCII.

### Comments

There are two types of comments in total: multi-line and single-line.


- Multiline comments must be declared with `\*` and end with `*/`.
- Single-line comments should start with `//`. Their end is the end of the line.

### Escape Sequences

- `\'` - Literal single-quote;
- `\"` - Literal double-quote;
- `\\` - Literal backslash;
- `\n` - End-of-line;
- `\t` - Horizontal tab;
- `\r` - Carriage return;
- `\`*`number`* - Denotes a character according to the ASCII table.

### Numbers

The numbers can be in decimal, hexadecimal and octal formats.

Hexadecimal numbers start with `0x`, and octal numbers start with `0o`.
