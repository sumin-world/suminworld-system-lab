# Signal Handling Demo

Unix signal handling in C with comprehensive examples.

## Features

- **Async-signal-safe** handlers
- **sigaction()** with `SA_RESTART`
- **Signal masking** demonstration
- **Job control** examples
- **Automated testing**

## Quick Start

```bash
# Clone and build
git clone <repo-url>
cd signal-demo
make

# Run the program
./signal_demo
```

## How to Use

### Interactive Menu
```bash
$ ./signal_demo

========================================
        Signal Handling Demo
========================================

PID: 12345

Choice (0-7): 1    # Select a demo
```

### Menu Options
```
1. Basic Signals - Test Ctrl+C and Ctrl+Z
2. Signal Masking - Block/unblock SIGINT
3. Alarm Signal - Timer demonstration
4. User Signals - USR1/USR2 handling
5. Job Control - Background/foreground
6. Simple Handler - Minimal pause() loop
7. Automated Test - Self-testing mode
```

### Example: Send Signals from Another Terminal
```bash
# Terminal 1: Run the program
$ ./signal_demo
Choice (0-7): 4
PID: 12345

# Terminal 2: Send signals
$ kill -USR1 12345    # Pause
$ kill -USR1 12345    # Resume
$ kill -USR2 12345    # Reset counter
```

### Example: Job Control
```bash
# Run in background
$ ./signal_demo &
[1] 12345

$ jobs
[1]+ Running  ./signal_demo &

$ fg
# Press Ctrl+Z (will be caught, not stopped)
# Press Ctrl+C then 'y' to exit
```

## Compilation Warnings

When compiling, you may see warnings about unused `write()` return values:
```
warning: ignoring return value of 'write' declared with attribute 'warn_unused_result'
```

These are **intentional** - in signal handlers, we cannot safely handle `write()` failures. The warnings can be suppressed by:
1. Using `(void)write(...)` - adds casting overhead
2. Adding `-Wno-unused-result` flag - suppresses all similar warnings
3. Ignoring them - they're harmless in this context

**Current approach**: We accept these warnings to keep signal handlers simple and educational.

## Future Improvements (Optional)

To eliminate warnings completely:
```c
// Replace all write() calls with:
(void)write(1, msg, len);
```

Or compile with:
```bash
gcc -O2 -Wall -Wextra -Wno-unused-result -o signal_demo signal_demo.c
```

## Platforms

- Linux, macOS (tested)
- POSIX-compliant systems

## Build Commands

```bash
make          # Compile
make run      # Run program
make test     # Run automated test
make clean    # Clean build files
```

## Implementation Notes

### Async-Signal-Safety
```c
void handler(int sig) {
    write(1, "Safe!\n", 6);  // Only async-signal-safe functions
    // printf() is NOT safe in signal handlers
}
```

### Signal-Safe Flags
```c
static volatile sig_atomic_t running = 1;
```

## Files

```
signal_demo.c    # Main program
test.sh          # Automated test script
Makefile         # Build configuration
README.md        # This file
.gitignore       # Git ignore rules
```

## License

MIT

## Author

suminworld