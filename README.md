# SertOS: Safety-Critical Preemptive Real-Time Operating System

**SertOS** is a strict ISO C99, freestanding, MISRA C:2012-compliant, hardware-agnostic preemptive real-time operating system kernel designed for deterministic mission-critical applications.

---

## Key Architectural Principles

1. **Freestanding & Zero Syscalls**:
   - Zero OS dynamic allocation (`malloc`/`free`/`sbrk`) per MISRA C:2012 Rule 21.3.
   - Dual task provisioning: caller-supplied static buffers (`sertos_task_create_static`) or deterministic block pool allocation (`sertos_task_create`).

2. **Deterministic $O(1)$ Preemptive Scheduling**:
   - Up to 32 scheduling priority levels using single-cycle Count-Leading-Zeros (CLZ) bitmap search.
   - Intrusive circular doubly linked lists for round-robin time-slicing among equal-priority tasks.

3. **Multi-Architecture Hardware Abstraction**:
   - **ARM Cortex-M55** (ARMv8.1-M Mainline with Helium MVE, double-precision FPU, `PSPLIM` stack limits).
   - **ARM Cortex-M33** (ARMv8-M Mainline with `PSPLIM` hardware stack limit traps and TrustZone).
   - **ARM Cortex-M23** (ARMv8-M Baseline with `PSPLIM` stack limit traps).
   - **ARM Cortex-M7** (ARMv7E-M with double-precision FPU and cache hooks).
   - **ARM Cortex-M4** (ARMv7E-M with single-precision floating-point stacking).
   - **ARM Cortex-M3** (ARMv7-M Thumb-2).
   - **ARM Cortex-M0+** (ARMv6-M with VTOR vector table relocation).
   - **ARM Cortex-M0** (ARMv6-M Thumb-1).
   - **RISC-V RV32I** (32-bit machine-mode trap handler).
   - **Native Windows Simulator** (`port/windows/` for Win32 MinGW-w64).
   - **Native POSIX Simulator** (`port/posix/` for Linux/macOS).

4. **Stack Safety & Canaries**:
   - Configurable stack alignment (8-byte AAPCS compliant).
   - Pattern painting (`0xA5`) and stack high-water mark runtime analysis (`sertos_task_get_stack_high_water_mark`).
   - Magic word TCB canary protection (`0x54434221U`).

## Foundational Submodules

SertOS leverages 7 modular, freestanding submodules tracked via Git submodules under `modules/`:

| Module | Purpose in SertOS | Key Integration Points |
| :--- | :--- | :--- |
| **`atomic`** | Lock-free synchronization | Atomic tick counter (`s_system_ticks`) with acquire/release memory semantics. |
| **`bitmap`** | $O(1)$ priority search | 32-level scheduler ready table using hardware CLZ instruction. |
| **`linked_list`** | Intrusive task queues | Circular doubly linked lists for ready tasks, blocked delay lists, and mutex owner tracking. |
| **`memory_pool`** | Deterministic allocation | Fixed-size block allocation for dynamic tasks, semaphores, mutexes, and queues without OS syscalls. |
| **`ring_buffer`** | FIFO storage | Lock-free single-producer single-consumer circular queue backing `sertos_queue`. |
| **`crc`** | Integrity validation | 32-bit CRC checksum calculation over TCB metadata and stack integrity guards. |
| **`fsm`** | Task lifecycle governance | Formal state transition verification (`READY`, `RUNNING`, `BLOCKED`, `SUSPENDED`, `TERMINATED`). |

---

## Directory Structure

```text
sertos/
├── .gitmodules                 # Submodule configuration
├── CMakeLists.txt              # Static library build configuration
├── inc/                        # Public architecture-agnostic headers
│   ├── sertos_config.h         # User-configurable parameters
│   ├── sertos_types.h          # Core types and status codes
│   ├── sertos_task.h           # TCB and task lifecycle API
│   ├── sertos_scheduler.h      # Preemptive scheduler API
│   ├── sertos_sem.h            # Binary and counting semaphores
│   ├── sertos_mutex.h          # Priority Inheritance Protocol mutex
│   ├── sertos_queue.h          # FIFO message queue (ring_buffer)
│   └── sertos_timer.h          # Monotonic software timers
├── src/                        # Core portable kernel source
│   ├── sertos_task.c           # Task creation, deletion, stack checks
│   ├── sertos_scheduler.c      # O(1) bitmap scheduler engine
│   ├── sertos_sem.c            # Semaphore take/give/ISR signaling
│   ├── sertos_mutex.c          # Recursive mutex with PIP elevation
│   ├── sertos_queue.c          # Thread-safe multi-task queue
│   └── sertos_timer.c          # Software timer dispatcher
├── modules/                    # Foundational submodules
│   ├── atomic/                 # Lock-free primitives & memory barriers
│   ├── bitmap/                 # Bit array and priority indexing
│   ├── crc/                    # CRC-8/16/32 checksum engines
│   ├── fsm/                    # Table-driven state machine
│   ├── linked_list/            # Intrusive circular doubly linked list
│   ├── memory_pool/            # Deterministic block allocator
│   └── ring_buffer/            # Circular byte buffer
└── port/                       # Hardware and simulator ports
    ├── sertos_port.h           # Hardware port contract
    ├── windows/                # Win32 host simulator
    ├── posix/                  # POSIX host simulator
    ├── arm/
    │   ├── cortex-m55/         # ARMv8.1-M (Helium MVE + PSPLIM)
    │   ├── cortex-m33/         # ARMv8-M Mainline (PSPLIM stack limits)
    │   ├── cortex-m23/         # ARMv8-M Baseline (PSPLIM stack limits)
    │   ├── cortex-m7/          # ARMv7E-M (Double-Precision FPU)
    │   ├── cortex-m4/          # ARMv7E-M (Lazy Single-Precision FPU)
    │   ├── cortex-m3/          # ARMv7-M (Thumb-2)
    │   ├── cortex-m0plus/      # ARMv6-M (VTOR relocation)
    │   └── cortex-m0/          # ARMv6-M (Thumb-1)
    └── riscv/
        └── rv32i/              # RISC-V 32-bit trap handler
```

---

## Building the Kernel Library

### Automated Batch Script (`build.bat`)
The repository provides a unified `build.bat` script supporting both Host simulators (Windows/POSIX) and all 8 ARM Cortex targets:

```powershell
# Build all target libraries (Windows + POSIX + all 8 ARM Cortex targets)
.\build.bat all

# Build Windows host library (lib/windows/libsertos_windows.a)
.\build.bat windows

# Build POSIX host library (lib/posix/libsertos_posix.a)
.\build.bat posix

# Build all 8 ARM Cortex libraries (lib/arm/libsertos_cortex_m*.a)
.\build.bat arm

# Build a specific ARM target (m0, m0plus, m3, m4, m7, m23, m33, or m55)
.\build.bat m7
.\build.bat m55
```

### CMake Alternative
The kernel can also be built using CMake:

```powershell
# Configure and build library for host simulation
cmake -B build -G "MinGW Makefiles"
cmake --build build
```

---

## Quality Metrics & Testing

All unit tests reside exclusively in the centralized [`test_bench`](../test_bench/) repository conforming to workspace guidelines. Automated gates enforce:
- Cyclomatic Complexity: $\le 10$ (CCN)
- Function Lines of Code: $\le 75$ (NLOC)
- Parameter Count: $\le 5$ (Params)
- 100% Pass Rate across 14 test suites via Unity + gcov.

---

## License

MIT License. Copyright (c) 2026 Prynce Seitenfus.