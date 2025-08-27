# suminworld-system-lab 🖥️

[![C](https://img.shields.io/badge/C-00599C?style=flat&logo=c&logoColor=white)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Linux](https://img.shields.io/badge/Linux-FCC624?style=flat&logo=linux&logoColor=black)](https://www.kernel.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](./LICENSE)

> **System programming & networking lab (C, Linux, OSTEP practice)**

A collection of hands-on system programming exercises, Linux kernel exploration, and network programming fundamentals using C.

---

## 📂 Project Structure

```
suminworld-system-lab/
├── process/                 # Process Management
│   ├── fork_basics/         # fork(), exec(), wait() fundamentals
│   ├── process_tree/        # Process hierarchy and relationships
│   ├── signal_handling/     # Signal processing and handlers
│   └── zombie_orphan/       # Zombie and orphan process examples
├── ipc/                     # Inter-Process Communication
│   ├── pipe/               # Anonymous and named pipes
│   ├── shared_memory/      # POSIX shared memory
│   ├── semaphore/          # Process synchronization
│   ├── message_queue/      # POSIX message queues
│   └── socket_pair/        # Unix domain sockets
├── network/                # Network Programming
│   ├── socket_basics/      # Socket creation and binding
│   ├── echo_server/        # TCP echo server implementation
│   ├── echo_client/        # TCP echo client implementation
│   ├── udp_chat/          # UDP chat application
│   ├── tcpdump_analysis/  # Network packet capture logs
│   └── raw_sockets/       # Raw socket programming
├── memory/                 # Memory Management
│   ├── malloc_free/        # Dynamic memory allocation
│   ├── mmap_examples/      # Memory mapping
│   ├── stack_heap/         # Stack vs heap analysis
│   └── buffer_overflow/    # Security considerations
├── filesystem/            # File System Operations
│   ├── file_io/           # Basic file operations
│   ├── directory_ops/     # Directory traversal
│   ├── file_permissions/  # Permission management
│   └── inotify/          # File system monitoring
├── kernel/               # Kernel Analysis & Notes
│   ├── scheduler/        # kernel/sched/core.c analysis
│   ├── memory_mgmt/      # Memory management deep dive
│   ├── syscalls/         # System call implementation
│   └── modules/          # Loadable kernel modules
└── notes/               # Study Notes & Documentation
    ├── ostep_notes/     # Operating Systems: Three Easy Pieces
    ├── kernel_notes/    # Linux kernel study notes
    ├── network_notes/   # Network programming concepts
    └── references.md    # Useful resources and links
```

---

## 🚀 Goals & Learning Objectives

### 📚 **OSTEP Practice**
- **Process Management**: Understanding fork(), exec(), wait() system calls
- **Memory Virtualization**: Virtual memory, paging, and segmentation
- **Concurrency**: Threads, locks, and synchronization primitives
- **Persistence**: File systems, I/O operations, and storage

### 🐧 **Linux Kernel Deep Dive**
- **Process Scheduler**: Analysis of `kernel/sched/core.c` and scheduling algorithms
- **Memory Management**: Page allocation, virtual memory subsystem
- **System Calls**: Implementation and kernel-user space interaction
- **Device Drivers**: Understanding kernel modules and hardware interaction

### 🌐 **Network Programming Fundamentals**
- **Socket Programming**: TCP/UDP server-client implementations
- **Network Protocols**: Hands-on analysis with tcpdump and Wireshark
- **Raw Sockets**: Low-level network packet manipulation
- **Network Security**: Basic concepts and implementation considerations

---

## 🛠️ Prerequisites & Setup

### **System Requirements**
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y build-essential gcc gdb valgrind
sudo apt install -y linux-headers-$(uname -r) strace ltrace
sudo apt install -y wireshark tcpdump netcat-openbsd

# Development tools
sudo apt install -y git vim tmux htop
```

### **Optional Tools**
```bash
# For kernel development
sudo apt install -y linux-source kernel-package libncurses-dev

# For network analysis
sudo apt install -y nmap iperf3 netstat-nat
```

---

## 🏃 Quick Start

### **Clone and Build**
```bash
git clone https://github.com/sumin-world/suminworld-system-lab.git
cd suminworld-system-lab

# Build all examples
make all

# Or build specific module
make process
make network
make ipc
```

### **Run Examples**

#### Process Management
```bash
# Basic fork example
cd process/fork_basics
gcc -o fork_demo fork_demo.c
./fork_demo

# Process tree visualization
cd process/process_tree
gcc -o ptree process_tree.c
./ptree
```

#### Network Programming
```bash
# Start echo server
cd network/echo_server
gcc -o server echo_server.c
./server 8080

# In another terminal, run client
cd network/echo_client
gcc -o client echo_client.c
./client localhost 8080
```

#### IPC Examples
```bash
# Shared memory demo
cd ipc/shared_memory
gcc -o producer producer.c -lrt
gcc -o consumer consumer.c -lrt
./producer &
./consumer
```

---

## 📖 Study Modules

### 🔄 **Process Management**

**Key Concepts:**
- Process creation with `fork()` and `exec()`
- Process synchronization with `wait()` and `waitpid()`
- Signal handling and inter-process communication
- Process states and lifecycle management

**Example: Basic Fork**
```c
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process
        printf("Child: PID = %d, Parent PID = %d\n", getpid(), getppid());
        return 0;
    } else if (pid > 0) {
        // Parent process
        printf("Parent: PID = %d, Child PID = %d\n", getpid(), pid);
        wait(NULL);  // Wait for child to complete
    } else {
        perror("fork failed");
        return 1;
    }
    
    return 0;
}
```

### 🔗 **Inter-Process Communication**

**Implementation Examples:**
- **Pipes**: Anonymous and named pipes for data streaming
- **Shared Memory**: POSIX shared memory for high-performance IPC
- **Semaphores**: Process synchronization and mutual exclusion
- **Message Queues**: Asynchronous message passing

### 🌐 **Network Programming**

**TCP Echo Server:**
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];
    
    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    // Setup address structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    // Bind socket
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    
    // Listen for connections
    listen(server_fd, 3);
    printf("Server listening on port %d\n", PORT);
    
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        
        // Echo client data
        int bytes_read = read(client_fd, buffer, BUFFER_SIZE);
        write(client_fd, buffer, bytes_read);
        
        close(client_fd);
    }
    
    return 0;
}
```

---

## 🔍 Kernel Analysis Notes

### **Process Scheduler Deep Dive**
```c
// kernel/sched/core.c - Key functions analysis

// Main scheduling function
static void __sched notrace __schedule(bool preempt) {
    struct task_struct *prev, *next;
    struct rq_flags rf;
    struct rq *rq;
    
    // Current implementation analysis notes:
    // 1. CFS (Completely Fair Scheduler) for SCHED_NORMAL
    // 2. RT scheduler for real-time tasks
    // 3. DL scheduler for deadline tasks
}

// Context switching mechanism
static struct task_struct *
pick_next_task(struct rq *rq, struct task_struct *prev, struct rq_flags *rf) {
    // Scheduler class selection logic
    // Analysis of how different schedulers are chosen
}
```

### **System Call Implementation**
```c
// Example: sys_getpid() implementation analysis
SYSCALL_DEFINE0(getpid) {
    return task_tgid_vnr(current);
}

// Notes on:
// - System call entry points
// - Parameter passing mechanisms  
// - Kernel-userspace transitions
// - Security considerations
```

---

## 📊 Performance Analysis

### **Benchmarking Tools**
```bash
# Process creation overhead
time ./fork_benchmark

# Network throughput testing  
iperf3 -s &                    # Server
iperf3 -c localhost -t 10      # Client test

# Memory allocation patterns
valgrind --tool=massif ./memory_test

# System call tracing
strace -c ./syscall_heavy_program
```

### **Profiling Examples**
```c
// Timing process operations
#include <time.h>

struct timespec start, end;
clock_gettime(CLOCK_MONOTONIC, &start);

// Operation to measure
fork_and_exec_operation();

clock_gettime(CLOCK_MONOTONIC, &end);
double elapsed = (end.tv_sec - start.tv_sec) + 
                (end.tv_nsec - start.tv_nsec) / 1e9;
printf("Operation took: %.6f seconds\n", elapsed);
```

---

## 🧪 Testing & Debugging

### **Memory Debugging**
```bash
# Check for memory leaks
valgrind --leak-check=full ./program

# Detect buffer overflows
gcc -fsanitize=address -g program.c
./a.out
```

### **Network Testing**
```bash
# Packet capture for analysis
sudo tcpdump -i lo -w capture.pcap port 8080

# Load testing
for i in {1..100}; do
    echo "Test message $i" | nc localhost 8080 &
done
```

### **Kernel Module Testing**
```bash
# Load custom kernel module
sudo insmod test_module.ko

# Check kernel logs
dmesg | tail -20

# Unload module
sudo rmmod test_module
```

---

## 📚 Study Resources & References

### **Essential Books**
- **[Operating Systems: Three Easy Pieces](https://pages.cs.wisc.edu/~remzi/OSTEP/)** - Core textbook
- **[The Linux Programming Interface](https://man7.org/tlpi/)** - System programming bible
- **[Understanding the Linux Kernel](https://www.oreilly.com/library/view/understanding-the-linux/0596005652/)** - Kernel internals
- **[Unix Network Programming](https://www.unpbook.com/)** - Network programming guide

### **Online Resources**
- **[Linux Kernel Source](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git)** - Official kernel source
- **[Linux Man Pages](https://man7.org/linux/man-pages/)** - System call documentation  
- **[Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)** - Network programming tutorial
- **[OSDev Wiki](https://wiki.osdev.org/)** - Operating system development

### **Debugging & Analysis Tools**
- **GDB**: GNU Debugger for C programs
- **Valgrind**: Memory debugging and profiling
- **Strace**: System call tracer
- **Wireshark**: Network protocol analyzer
- **Perf**: Linux profiling tool

---

## ⚠️ Important Disclaimers

### **Educational Purpose**
This repository contains educational examples and exercises designed for learning system programming concepts. **Not intended for production use.**

### **Security Considerations**
- Examples may contain **intentional vulnerabilities** for educational purposes
- **Buffer overflow examples** are for understanding security concepts only
- **Always use proper bounds checking** in production code
- **Raw socket examples** require root privileges - use carefully

### **System Safety**
- **Kernel module examples** can potentially crash the system if modified incorrectly
- **Test in virtual machines** when experimenting with kernel code
- **Always backup important data** before running system-level code

### **Legal & Ethical Use**
- Use these tools only on systems you own or have explicit permission to test
- Respect all applicable laws and regulations
- Follow responsible disclosure for any security issues discovered

---

## 🤝 Contributing

### **How to Contribute**
1. **Fork** the repository
2. **Create** a feature branch for your study notes or examples
3. **Document** your code with clear comments and explanations
4. **Test** your examples in a safe environment
5. **Submit** a pull request with detailed description

### **Contribution Areas**
- [ ] Additional OSTEP exercise solutions
- [ ] Kernel analysis deep-dives
- [ ] Network programming advanced examples
- [ ] Performance optimization case studies
- [ ] Security vulnerability demonstrations
- [ ] Cross-platform compatibility improvements

---

## 📄 License

This project is licensed under the **MIT License** - see the [LICENSE](./LICENSE) file for details.

**Educational Use Disclaimer**: This repository is designed for educational purposes. Users assume full responsibility for any consequences of running this code.

---

<div align="center">

**🎓 Educational System Programming Laboratory**

**For learning, research, and exploration in controlled environments only**

</div>
