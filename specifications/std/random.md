# std/random.rave

All the functions below also have their equivalents, in which you can specify the boundaries of the generated number - specify the lower boundary in the first argument and the upper one in the second argument.

## std::randomChar

Returns a random char number.

Example:

```d
char rand = std::randomChar();
```

## std::randomShort

Returns a random short number.

Example:

```d
short rand = std::randomShort();
```

## std::randomInt

Returns a random int number.

Example:

```d
int rand = std::randomInt();
```

## std::randomLong

Returns a random long number.

Example:

```d
long rand = std::randomLong();
```

## std::randomCent

Returns a random cent number.

Example:

```d
cent rand = std::randomCent();
```

## std::randomBuffer

Fills a provided buffer with provided length with random values.
This option is considered more optimized than using std::randomChar through a loop.

Example:

```d
char[32] buffer;
std::randomBuffer(&buffer, 32);
```