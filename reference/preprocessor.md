# Preprocessor reference

## Working with definitions

### `@ifdef [defname] [ifdefblock] @end`

If the definition exists, the code is processed.

### `@ifndef [defname] [ifndefblock] @end`

If the definition doesn't exist, the code is processed.

### `@def [defname] [defvalue or none] @end`

Creating a definition.

### `@undef [defname]`

Deletes the definition.

### `@inc "pathname" or <pathname>(if std)`
### `@ins "pathname" or <pathname>(if std)`

Inserts the specified file, but "@inc" inserts the file taking into account the protection against its re-inclusion.

This means that "@ins" is an include from C.

`.rave` is inserted automatically.

### `@exit [exitcode]`

Stops compilation with specific code.

### `@macro [macroname]([macroargs]) [macroblock] @endm`

Simplified - extended definitions.
They allow you to work with arguments, which cannot be done in definitions.
Example:

    @macro A(first,second) (first+second) @endm