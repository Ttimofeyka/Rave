# std/locale

Locale and internationalization support.

## Import

```d
import <std/locale>
```

## Constants

### Locale Categories

- `std::locale::ctype` - Character classification (0)
- `std::locale::numeric` - Numeric formatting (1)
- `std::locale::time` - Time formatting (2)
- `std::locale::collate` - String collation (3)
- `std::locale::monetary` - Monetary formatting (4)
- `std::locale::messages` - Message catalogs (5)
- `std::locale::all` - All categories (6)

## Functions

### set
```d
char* set(int category, char* locale)
```

Sets program locale for specified category. Returns current locale string.

**Parameters:**
- `category` - Locale category constant
- `locale` - Locale name (e.g., "en_US.UTF-8", "C")

## Example

```d
import <std/locale>

std::locale::set(std::locale::all, "en_US.UTF-8");
std::locale::set(std::locale::numeric, "de_DE.UTF-8");
```
