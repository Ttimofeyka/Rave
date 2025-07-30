# std/thread.md

## std::thread

This structure is designed to use system threads across a common (cross-platform) codebase.

### run

Run the specified function with the specified argument through a new thread.

Example:

```d
int foo(void* arg) => std::println("Hello from thread!");

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

int foo(void* arg) {
    while (data1 != 64) data1 *= 2;
}

int bow(void* arg) {
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

int foo(void* arg) {
    std::thread::spinlock::lock(&splock);
    data *= 2;
    std::thread::spinlock::unlock(&splock);
}

int bow(void* arg) {
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

int foo(void* arg) {
    splock.lock();
    data *= 2;
    splock.unlock();
}

int bow(void* arg) {
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