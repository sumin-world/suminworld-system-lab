# Signal Handling Demo

[![CI](https://github.com/sumin-world/suminworld-system-lab/actions/workflows/c-build.yml/badge.svg?branch=main)](https://github.com/sumin-world/suminworld-system-lab/actions/workflows/c-build.yml?query=branch%3Amain)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

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

## Learning Map: Shell Concepts

This demo maps to key concepts in shell implementation:

| Demo Feature | Shell Concept | Description |
|--------------|---------------|-------------|
| **SIGINT Handler** | `sigaction(SIGINT, ...)` | Handle Ctrl+C with confirmation prompt |
| **SIGTSTP Catch** | Job control awareness | Prevent default stop, show custom behavior |
| **SIGALRM Timer** | Timeout implementation | Periodic timers, alarm-based tasks |
| **USR1/USR2** | State management | Toggle/reset operations via signals |
| **Signal Masking** | Critical section protection | Block signals during important operations |
| **waitpid()** | Zombie prevention | Proper child process cleanup |

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

## Experimental Learning

### SIGTSTP Default Behavior
To see the difference between catching and default handling:
```c
/* In main(), try commenting out SIGTSTP handler: */
// install_handler(SIGTSTP, sigtstp_handler);
install_handler(SIGTSTP, SIG_DFL);  // Use default (actual stop)
```

This shows how job control normally works versus custom handling.

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
make          # Release build
make debug    # Debug build with sanitizers
make run      # Run program
make test     # Run automated test
make clean    # Clean build files
```

### Debug Build
The debug target includes:
- Address Sanitizer (memory errors)
- Undefined Behavior Sanitizer
- Debug symbols (`-g`)
- No optimization (`-O0`)

## Implementation Notes

### Async-Signal-Safety
```c
void handler(int sig) {
    write(1, "Safe!\n", 6);  // Only async-signal-safe functions
    // printf() is NOT safe in signal handlers
}
```

### Advanced: Race-Free Signal Handling
For advanced users, consider `pselect()` instead of `select()`:
```c
/* pselect() with signal mask is more race-free than select() */
// sigset_t mask;
// pselect(nfds, &readfds, NULL, NULL, &timeout, &mask);
```

### Signal-Safe Flags
```c
static volatile sig_atomic_t running = 1;
```

## Files

```
signal_demo.c            # Main program
test.sh                  # Automated test script
Makefile                 # Build configuration
README.md                # This file
LICENSE                  # MIT License
.github/workflows/c.yml  # CI configuration
.gitignore               # Git ignore rules
```

## CI/CD

GitHub Actions automatically:
- Builds release version
- Builds debug version with sanitizers
- Runs automated tests

## License

This project follows the license in the repository root (MIT).

## Author

suminworld