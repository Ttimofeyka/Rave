# std/process

Process management and control functions.

## Types

### std::process::rusage

Resource usage statistics (Linux/FreeBSD).

**Fields:**
- `std::timeVal ru_utime` - User CPU time
- `std::timeVal ru_stime` - System CPU time
- `long ru_maxrss` - Maximum resident set size
- `long ru_minflt` - Minor page faults
- `long ru_majflt` - Major page faults
- Additional resource tracking fields

## Functions

### getPID
```d
int getPID()
```
Returns current process ID.

### getPPID
```d
int getPPID()
```
Returns parent process ID (Linux/FreeBSD).

### kill
```d
int kill(int pid, int sig)
```
Sends signal to process.

### fork
```d
int fork()
```
Creates child process. Returns 0 in child, child PID in parent.

### clone
```d
int clone(int(void*) fn, void* stack, int flags, void* arg, int* pTID, void* tls, int* cTID)
```
Linux-specific process/thread creation with fine-grained control.

**Clone Flags:**
- `std::process::cloneVM` - Share memory space
- `std::process::cloneFS` - Share filesystem info
- `std::process::cloneFiles` - Share file descriptors
- `std::process::cloneSighand` - Share signal handlers
- `std::process::cloneThread` - Create thread
- `std::process::cloneNewNS` - New namespace
- Additional flags for advanced control

### execve
```d
int execve(char* fName, char** argv, char** envp)
```
Executes program with arguments and environment.

### execl
```d
int execl(char* path, ...)
```
Executes program with variadic arguments.

### waitpid
```d
int waitpid(uint pid, int* status, int options)
```
Waits for child process to change state.

### wait4
```d
uint wait4(uint pid, int* status, int options, std::process::rusage* rusage)
```
Waits for process with resource usage info (Linux/FreeBSD).

## Status Macros

### isTermSignal
```d
int isTermSignal(int s)
```
Checks if process terminated by signal.

### isNormalExit
```d
int isNormalExit(int s)
```
Checks if process exited normally.

### getExitStatus
```d
int getExitStatus(int s)
```
Extracts exit status code.

## Variables

### std::process::environment
```d
char** environment
```
Environment variables array (Linux/FreeBSD).

## Example

```d
import <std/process>

int pid = std::process::fork();

if (pid == 0) {
    std::print("Child process\n");
    std::exit(0);
} else {
    int status;
    std::process::waitpid(pid, &status, 0);
    std::print("Child exited with status: ", std::process::getExitStatus(status), "\n");
}
```
