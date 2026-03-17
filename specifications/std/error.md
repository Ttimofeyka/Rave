# std/error

Error codes and signal handling for system-level error management.

## Error Codes

### std::NotFound
Value: `-1` - Resource or element not found

### std::NotEnoughMemory
Value: `-2` - Insufficient memory available

### std::BeyondSize
Value: `-3` - Index or size exceeds bounds

## Signal Constants

### std::SigInt
Value: `2` - Interrupt signal (Ctrl+C)

### std::SigIll
Value: `4` - Illegal instruction

### std::SigFpe
Value: `8` - Floating-point exception

### std::SigKill
Value: `9` - Kill signal (cannot be caught)

### std::SigSegv
Value: `11` - Segmentation fault

### std::SigAlarm
Value: `14` - Alarm clock signal

### std::SigTerm
Value: `15` - Termination signal

### std::SigStop
Value: `19` - Stop process

### std::SigBreak
Value: `21` - Break signal

### std::SigAbort
Value: `22` - Abort signal

## Signal Handlers

### std::SigDefault
Default signal handler (terminates process)

### std::SigIgnore
Ignore signal handler

## Functions

### signal

```d
void signal(int flag, void(int) catcher)
```

Registers a signal handler for the specified signal. Platform-specific implementation:
- **Linux:** Uses sigaction system call
- **Windows:** Uses SetConsoleCtrlHandler for SigInt and SigTerm
- **Other:** Uses POSIX signal function

## Example

```d
import <std/error>

void handleInterrupt(int sig) {
    std::print("Caught interrupt signal\n");
    std::exit(0);
}

std::signal(std::SigInt, handleInterrupt);
```
