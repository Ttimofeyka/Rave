# std/sysc

System call interface for Linux and FreeBSD.

## Functions

### syscall
```d
char* syscall(int number, ...)
```
Invokes system call with variadic arguments. Returns result as void pointer.

## System Call Numbers

System call numbers are defined in `std::sysctable` namespace and vary by platform.

### File Operations
- `Read`, `Write`, `OpenAt`, `Close`
- `RenameTo`, `UnlinkAt`, `FStat`, `LSeek`
- `FAccessAt`, `GetDents64`, `MakeDirAt`

### Process Management
- `Exit`, `GetPID`, `GetTID`, `GetPPID`
- `Clone`, `Fork`, `Execve`, `Wait4`, `Kill`

### Signals
- `SigAction`, `SigReturn`

### Networking
- `Socket`, `Bind`, `Connect`, `Listen`
- `Accept4`, `Shutdown`
- `SendTo`, `RecvFrom`, `SendMsg`, `RecvMsg`
- `GetSockName`, `GetPeerName`, `SocketPair`
- `SetSockOpt`, `GetSockOpt`

### Time
- `ClockGetTime`, `GetTimeOfDay`, `NanoSleep`
- `GetRandom`

### System
- `Reboot`, `GetCwd`

## Supported Platforms

- Linux: X86_64, X86, AArch64, ARM, PowerPC, PowerPC64, MIPS64
- FreeBSD: All architectures

## Example

```d
import <std/sysc>

// Get process ID
int pid = cast(int)std::syscall(std::sysctable::GetPID);

// Write to stdout
char* msg = "Hello\n";
std::syscall(std::sysctable::Write, 1, msg, 6);
```
