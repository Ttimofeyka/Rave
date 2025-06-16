# std/crypto.rave

## std::base16

### std::base16::encode

Translates the specified set of bytes (or `std::string`) to bytes encoded in Base16 format and writes them to `std::string*` buffer.

Examples:

```d
std::string buffer = "";
std::base16::encode(&buffer, "Test");
```

### std::base16::decode

Decodes data from the Base16 format and writes it to `std::string*` buffer.

Examples:
```d
std::string buffer = "";
std::base16::decode(&buffer, "54657374");
```

## std::base32

### std::base32::encode

Translates the specified set of bytes (or `std::string`) to bytes encoded in Base32 format and writes them to `std::string*` buffer.

Examples:

```d
std::string buffer = "";
std::base32::encode(&buffer, "Test");
```

### std::base32::decode

Decodes data from the Base32 format and writes it to `std::string*` buffer.

Examples:
```d
std::string buffer = "";
std::base32::decode(&buffer, "KRSXG5A=");
```

## std::base64

### std::base64::encode

Translates the specified set of bytes (or `std::string`) to bytes encoded in Base64 format and writes them to `std::string*` buffer.

Examples:

```d
std::string buffer = "";
std::base64::encode(&buffer, "Test");
```

### std::base64::decode

Decodes data from the Base64 format and writes it to `std::string*` buffer.

Examples:
```d
std::string buffer = "";
std::base64::encode(&buffer, "VGVzdA==");
```
