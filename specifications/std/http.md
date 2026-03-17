# std/http

HTTP/1.1 client and server implementation.

## Types

### std::http::Response

HTTP response structure.

**Fields:**
- `std::hashmap<std::string, std::string> headers` - Response headers
- `std::string body` - Response body content
- `int status` - HTTP status code
- `int success` - Success indicator (0 = success, -1 = error)

### std::http::Request

HTTP request structure.

**Fields:**
- `std::hashmap<std::string, std::string> headers` - Request headers
- `std::string body` - Request body content
- `std::string method` - HTTP method (GET, POST, etc.)
- `std::string url` - Request URL path
- `int success` - Success indicator

### std::http::Connection

HTTP client connection.

**Fields:**
- `std::socket socket` - Underlying socket
- `std::string protocol` - Protocol (http/https)
- `std::string host` - Hostname
- `std::string path` - URL path
- `char* port` - Port number

**Constructor:**
```d
std::http::Connection this(char* url, char* port)
```

### std::http::Server

HTTP server.

**Fields:**
- `std::socket socket` - Listening socket
- `std::string port` - Server port

## Functions

### Connection Methods

#### open
```d
int open()
```
Opens connection to host. Returns -1 on socket error, -2 on connect error.

#### write
```d
int write(std::cstring buffer)
int write(std::string buffer)
```
Writes data to connection.

#### read
```d
int read(char* buffer, int length)
```
Reads data from connection.

#### close
```d
int close()
```
Closes the connection.

#### sendRequestData
```d
int sendRequestData(std::cstring method, std::hashmap<std::string, std::string>* headers, std::cstring body)
```
Sends HTTP request with method, headers, and body.

#### sendRequest
```d
int sendRequest(std::http::Request req)
```
Sends HTTP request object.

#### readResponse
```d
std::http::Response readResponse()
```
Reads and parses HTTP response.

#### request
```d
std::http::Response request(std::cstring method, std::hashmap<std::string, std::string>* headers, std::cstring body)
```
Sends request and reads response in one call.

### Server Methods

#### open
```d
int open(char* url, int port)
```
Opens server on specified address and port. Returns -1 on socket error, -2 on bind error, -3 on listen error.

#### accept
```d
std::http::Connection accept()
```
Accepts incoming client connection.

#### close
```d
int close()
```
Closes the server socket.

## Example

```d
import <std/http>

// Client
std::http::Connection conn = std::http::Connection("http://example.com", "80");
conn.open();

std::http::Response res = conn.request("GET", null, std::cstring(null, 0));
std::print("Status: ", res.status, "\n");

conn.close();

// Server
std::http::Server server = std::http::Server();
server.open("0.0.0.0", 8080);

std::http::Connection client = server.accept();
std::http::Request req = client.readRequest();
client.sendResponseData(200, "OK", null, std::cstring("Hello", 5));
client.close();

server.close();
```
