# std/pthread

POSIX threads (pthreads) interface for multithreading.

## Types

### pthread::attribute

Thread attributes structure (platform-specific size).

### pthread::mutex

Mutex structure for thread synchronization (platform-specific size).

## Functions

### create
```d
int create(ulong* thread, pthread::attribute* attr, void*(void*) start, void* arg)
```
Creates new thread. Returns 0 on success.

**Parameters:**
- `thread` - Output thread ID
- `attr` - Thread attributes (null for defaults)
- `start` - Thread function
- `arg` - Argument passed to thread function

### join
```d
int join(ulong thread, void** valuePtr)
```
Waits for thread to terminate. Returns 0 on success.

### self
```d
ulong self()
```
Returns calling thread's ID.

### exit
```d
void exit(void* retVal)
```
Terminates calling thread with return value.

### cancel
```d
int cancel(ulong thread)
```
Requests thread cancellation. Returns 0 on success.

### detach
```d
int detach(ulong thread)
```
Detaches thread (resources freed on termination). Returns 0 on success.

## Example

```d
import <std/pthread>

void* threadFunc(void* arg) {
    int* num = cast(int*)arg;
    std::print("Thread received: ", num[0], "\n");
    pthread::exit(null);
}

int value = 42;
ulong threadId;
pthread::create(&threadId, null, threadFunc, &value);
pthread::join(threadId, null);
```
