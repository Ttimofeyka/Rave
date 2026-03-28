# std/thread.md

## std::thread

This structure is designed to use system threads across a common (cross-platform) codebase.

### run

Run the specified function with the specified argument through a new thread.

Example:

```d
int foo(char* arg) => std::println("Hello from thread!");

void main {
    std::thread thr = std::thread();
    thr.run(foo, null);
}
```

### join

Wait for the end of the thread execution.

Example:

```d
int data1 = 2;
int data2 = 2;

int foo(char* arg) {
    while (data1 != 64) data1 *= 2;
}

int bow(char* arg) {
    while (data2 != 64) data2 *= 2;
}

void main {
    std::thread thr1 = std::thread();
    std::thread thr2 = std::thread();

    thr1.run(foo, null);
    thr2.run(bow, null);

    thr1.join();
    thr2.join();

    // data1, data2 == 64
}
```

### hash

Get an int hash of std::thread.

## std::thread::exit

Exit the thread with a specific code (if no code is specified, it defaults to 0).

## std::thread::spinlock::lock / std::thread::spinlock::unlock

These functions are designed to prevent data races through the implementation of the spinlock.

Example:

```d
int data = 2;

int splock;

int foo(char* arg) {
    std::thread::spinlock::lock(&splock);
    data *= 2;
    std::thread::spinlock::unlock(&splock);
}

int bow(char* arg) {
    std::thread::spinlock::lock(&splock);
    data *= 2;
    std::thread::spinlock::unlock(&splock);
}

void main {
    std::thread thr1 = std::thread();
    std::thread thr2 = std::thread();

    thr1.run(foo, null);
    thr2.run(bow, null);

    thr1.join();
    thr2.join();

    // data == 8
}
```

## std::spinlock

This structure performs the same tasks as the functions above.

Example:

```d
int data = 2;

std::spinlock splock;

int foo(char* arg) {
    splock.lock();
    data *= 2;
    splock.unlock();
}

int bow(char* arg) {
    splock.lock();
    data *= 2;
    splock.unlock();
}

void main {
    std::thread thr1 = std::thread();
    std::thread thr2 = std::thread();

    thr1.run(foo, null);
    thr2.run(bow, null);

    thr1.join();
    thr2.join();

    // data == 8
}
```

## std::mutex

Blocking mutex implementation that uses kernel-level synchronization primitives:
- On Unix: uses pthread mutex (futex-based blocking)
- On Windows: uses CRITICAL_SECTION

This is more efficient than spinlock for longer-held locks, as waiting threads are put to sleep instead of spinning.

### lock

Acquires the mutex, blocking if it's already held by another thread.

### unlock

Releases the mutex, allowing other waiting threads to acquire it.

### trylock

Attempts to acquire the mutex without blocking. Returns 0 on success, non-zero if the mutex is already locked.

### destroy

Destroys the mutex and releases associated resources. Should be called when the mutex is no longer needed.

Example:

```d
import <std/io> <std/thread>

int counter = 0;
std::mutex mtx;

int increment(char* arg) {
    for (int i=0; i<10000; i++) {
        mtx.lock();
        counter += 1;
        mtx.unlock();
    }
}

void main {
    std::thread t1 = std::thread();
    std::thread t2 = std::thread();
    std::thread t3 = std::thread();

    t1.run(increment, null);
    t2.run(increment, null);
    t3.run(increment, null);

    t1.join();
    t2.join();
    t3.join();

    mtx.destroy();

    std::println("Counter: ", counter);
}