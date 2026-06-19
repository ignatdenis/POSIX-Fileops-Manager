# POSIX FileOps Manager & Indexer

## Overview
This system programming project implements a high-performance, concurrent file system scanner and indexer in C. It uses a Manager-Worker multiprocessing architecture to recursively scan directories, extract file metadata, and securely store the results in a custom binary database format. 

## Key Features
* **Manager-Worker Architecture:** The system uses `fork()` and `exec()` to distribute directory scanning jobs across multiple worker processes.
* **Advanced IPC (Inter-Process Communication):** * Uses a shared memory map (`mmap` with `MAP_SHARED`) to maintain a circular job queue and result channels.
  * Synchronizes access using POSIX Semaphores (`sem_empty`, `sem_full`, `sem_mutex`) to handle backpressure and prevent data races.
  * Utilizes asynchronous anonymous pipes (set to `O_NONBLOCK`) for a control plane, allowing workers to send atomic text messages to the manager.
* **File Metadata & Hashing:** Extracts detailed metadata (size, permissions, uid, gid, mtime) using `lstat` and calculates the SHA256 hash for regular files using OpenSSL.
* **Custom Binary Database:** Writes results atomically to a `.db` file using a structured binary format (`DBHeader`, `FileRecord`, `WorkerStats`).
* **Robust Signal Handling:** Implements graceful shutdown procedures capturing `SIGINT` and `SIGTERM`, safely terminating workers, and saving a partial database state (`DB_PART_COMPLETE`). Generates real-time execution stats upon receiving `SIGUSR1`.

## Repository Structure
* `/src` - Source code for the manager, workers, and ring buffer implementations.
* `/include` - Header files defining IPC data structures and system configurations.
* `/tools` - Contains the `fileops.sh` script used for building, running, and testing the application.
* `/tests` - Bash scripts for stress testing (e.g., generating 50,000 files in RAM) and validating signal handling.
* `/doc` - Architectural documentation detailing the memory map protocol and database formats.
* `/data`, `/logs`, `/reports`, `/bin`, `/tmp` - Runtime directories managed by the build script.

## Build and Execution

The project includes a comprehensive bash script to manage the lifecycle of the application. 

**Prerequisites:** GCC compiler, OpenSSL (`libcrypto`), and a Linux environment.

1. **Initialize directory structure:**
   ```bash
   ./tools/fileops.sh init
