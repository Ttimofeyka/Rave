# Preprocessor reference

## Working with definitions

### `@ifdef [defname] [ifdefblock] @end`

If the definition exists, the code is processed.

### `@ifndef [defname] [ifndefblock] @end`

If the definition doesn't exist, the code is processed.

### `@def [defname] [defvalue or none] @end`

Creating a definition.

### `@redef [defname] [newdefvalue] @end`

Changing the definition value.

### `@undef [defname]`

Deletes the definition.

### `@inc "[pathname]"`

Inserts the specified file.

`.rave` is inserted automatically.

### `@once [onceblock] @end`

Indicates that when you insert this file multiple times from other files, this code will be inserted only 1 time.

### `@else [elseblock] @end`

If the previous command associated with the conditions is incorrect, then this code is inserted.

### `@err [errval] @end`

Outputs an error with compilation stopped.

### `@warn [warnval] @end`

Outputs a warning.

### `@out [outval] @end`

Outputs a text.

### `@exit [exitcode]`

Stops compilation with specific code.
