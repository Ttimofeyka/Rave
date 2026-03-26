# std/socket

Network socket programming with TCP/IP support.

## Constants

### Address Families
- `std::socket::AfInet` - IPv4 (value: 2)
- `std::socket::AfInet6` - IPv6 (value: 2)
- `std::socket::PfInet` - IPv4 protocol family (value: 2)

### Socket Types
- `std::socket::SockStream` - TCP stream socket (value: 1)

### Shutdown Options
- `std::socket::ShutRD` - Shutdown read (value: 0)
- `std::socket::ShutWR` - Shutdown write (value: 1)
- `std::socket::ShutRDWR` - Shutdown both (value: 2)

### Address Constants
- `std::socket::InAddrAny` - Bind to any address (0)
- `std::socket::InAddrBroadcast` - Broadcast address (4294967295)

### Socket Options
- `std::socket::SocketReuseAddr` - Reuse address option
- `std::socket::SomAxConn` - Max connection queue (128 on Linux, 5 on Windows)

### Flags
- `std::socket::AiPassive` - Passive socket flag
- `std::socket::AiCanonName` - Canonical name flag
- `std::socket::MsgNoSig` - No SIGPIPE signal (Linux)

## Types

### std::socket

Main socket structure.

**Fields:**
- `int fd` (Linux) or `uint* fd` (Windows) - Socket file descriptor
- `std::socket::sockaddr_in address` - Socket address
- `int addressLength` - Address structure length

### std::socket::sockaddr_in

IPv4 socket address.

**Fields:**
- `short sin_family` - Address family
- `ushort sin_port` - Port number (network byte order)
- `std::socket::in_addr sin_addr` - IP address
- `char[8] sin_zero` - Padding

### std::socket::addrinfo

Address information for DNS resolution.

**Fields:**
- `int ai_flags` - Input flags
- `int ai_family` - Address family
- `int ai_socktype` - Socket type
- `int ai_protocol` - Protocol
- `uint/usize ai_addrlen` - Address length
- `std::socket::sockaddr* ai_addr` - Socket address
- `char* ai_canonname` - Canonical name
- `std::socket::addrinfo* ai_next` - Next in linked list

## Socket Methods

### Constructor
```d
std::socket this()
```
Creates new socket (fd initialized to -1).

### open
```d
int open(char* ip, int port, long flag)
```
Opens socket with IP address, port, and flag (InAddrAny or InAddrBroadcast).

### openAny
```d
int openAny(int port)
```
Opens socket on any address with specified port.

### bind
```d
int bind()
```
Binds socket to address.

### listen
```d
int listen()
```
Listens for incoming connections.

### connect
```d
int connect()
```
Connects to remote address.

### accept
```d
void accept(std::socket* acceptor)
```
Accepts incoming connection into acceptor socket.

### read
```d
int read(char* data, int length)
```
Reads data from socket.

### write
```d
int write(char* data, int length)
int write(char* data)
int write(std::string str)
```
Writes data to socket.

### close
```d
int close()
```
Closes socket connection.

## Low-Level Functions

### Byte Order Conversion
```d
uint htonl(uint hostlong)
ushort htons(ushort hostshort)
uint ntohl(uint netlong)
ushort ntohs(ushort netshort)
```
Host-to-network and network-to-host byte order conversion.

### DNS Resolution
```d
int getAddrInfo(char* node, char* service, std::socket::addrinfo* hints, std::socket::addrinfo** res)
void freeAddrInfo(std::socket::addrinfo* res)
char* getAddrInfoStrErr(int errcode)
```
DNS hostname resolution.

### IP Address Conversion
```d
int inetPton(int af, char* src, char* dst)
char* inetNtop(int af, char* src, char* dst, int cnt)
std::socket::in_addr inetAddr(char* cp)
```
Convert between text and binary IP addresses.

### IPv4 Validation
```d
bool std::ipv4::isValid(char* ip)
```
Validates IPv4 address format.

## Example

```d
import <std/socket>

// Server
std::socket server = std::socket();
server.open("0.0.0.0", 8080, std::socket::InAddrAny);
server.bind();
server.listen();

std::socket client = std::socket();
server.accept(&client);

char[1024] buffer;
int received = client.read(&buffer, 1024);
client.write("Hello\n");
client.close();
server.close();

// Client
std::socket conn = std::socket();
conn.open("127.0.0.1", 8080, std::socket::InAddrBroadcast);
conn.connect();
conn.write("GET / HTTP/1.1\r\n\r\n");
conn.close();
```
