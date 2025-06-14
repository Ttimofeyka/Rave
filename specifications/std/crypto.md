# std/crypto.rave

## std::base16

### std::base16::encodeBuffer

This function is similar to the one listed below, except that it does not return a buffer, but accepts it as `std::string*` in the first argument.

### std::base16::encode

Translates the specified set of bytes (or `std::string`) to `std::string` encoded in base16 format.

Examples:

```d
std::string buffer = std::base16::encode(std::cstring("Test", 4));
std::string buffer2 = std::base16::encode("Test");
std::string buffer3 = std::base16::encode(std::string("Test"));
```

### std::base16::decodeBuffer

This function is similar to the one listed below, except that it does not return a buffer, but accepts it as `std::string*` in the first argument.

### std::base16::decode

Decodes data from the base16 format to `std::string`.

Examples:
```d
std::string buffer = std::base16::encode(std::cstring("54657374", 8));
std::string buffer2 = std::base16::encode("54657374");
std::string buffer3 = std::base16::encode(std::string("54657374"));
```

## std::base32

### std::base32::encodeBuffer

This function is similar to the one listed below, except that it does not return a buffer, but accepts it as `std::string*` in the first argument.

### std::base32::encode

Translates the specified set of bytes (or `std::string`) to `std::string` encoded in base32 format.

Examples:

```d
std::string buffer = std::base32::encode(std::cstring("Test", 4));
std::string buffer2 = std::base32::encode("Test");
std::string buffer3 = std::base32::encode(std::string("Test"));
```

### std::base32::decodeBuffer

This function is similar to the one listed below, except that it does not return a buffer, but accepts it as `std::string*` in the first argument.

### std::base32::decode

Decodes data from the base32 format to `std::string`.

Examples:
```d
std::string buffer = std::base132::encode(std::cstring("KRSXG5A=", 8));
std::string buffer2 = std::base32::encode("KRSXG5A=");
std::string buffer3 = std::base32::encode(std::string("KRSXG5A="));
```

## std::base64

### std::base64::encodeBuffer

This function is similar to the one listed below, except that it does not return a buffer, but accepts it as `std::string*` in the first argument.

### std::base64::encode

Translates the specified set of bytes (or `std::string`) to `std::string` encoded in base64 format.

Examples:

```d
std::string buffer = std::base64::encode(std::cstring("Test", 4));
std::string buffer2 = std::base64::encode("Test");
std::string buffer3 = std::base64::encode(std::string("Test"));
```

### std::base64::decodeBuffer

This function is similar to the one listed below, except that it does not return a buffer, but accepts it as `std::string*` in the first argument.

### std::base64::decode

Decodes data from the base64 format to `std::string`.

Examples:
```d
std::string buffer = std::base64::encode(std::cstring("KRSXG5A=", 8));
std::string buffer2 = std::base64::encode("KRSXG5A=");
std::string buffer3 = std::base64::encode(std::string("KRSXG5A="));
```
