# POSIX FileOps Manager & Indexer

## Overview
This system programming project implements a high-performance, concurrent file system scanner and indexer in C. It uses a Manager-Worker multiprocessing architecture to recursively scan directories, extract file metadata, and securely store the results in a custom binary database format. 

## Key Features
* **Manager-Worker Architecture:** The system uses `fork()` and `exec()` to distribute directory scanning jobs across multiple worker processes.
* **Advanced IPC:**  Uses a shared memory map to maintain a circular job queue and result channels.
  * Synchronizes access using POSIX Semaphores to handle backpressure and prevent data races.
  * Utilizes asynchronous anonymous pipes for a control plane, allowing workers to send atomic text messages to the manager.
* **File Metadata & Hashing:** Extracts detailed metadata (size, permissions, uid, gid, mtime) using `lstat` and calculates the SHA256 hash for regular files using OpenSSL.
* **Custom Binary Database:** Writes results atomically to a `.db` file using a structured binary format.
* **Robust Signal Handling:** Implements graceful shutdown procedures capturing `SIGINT` and `SIGTERM`, safely terminating workers, and saving a partial database state. Generates real-time execution stats upon receiving `SIGUSR1`.

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

### 1. Initialize Workspace
Create the standard project directory layout (`bin`, `src`, `include`, `data`, `logs`, etc.) and verify that the GCC compiler is available:
```bash
./tools/fileops.sh init
```

### 2. Compile the Project
Automatically find all module files and entry points (`main_*.c`), compile source objects under `tmp/obj/`, and link the final executables into `bin/`:
```bash
./tools/fileops.sh build
```
*Note: You can override the source folder using `./tools/fileops.sh build --src <custom_folder>`.*

### 3. Run the Application
Execute any compiled binary from the `bin/` directory safely by passing the separator `--` followed by the application's mandatory flags:
```bash
# Example: Running the manager with 4 worker processes to index a directory
./tools/fileops.sh run -- fileops_manager --root /path/to/scan --workers 4 --ipc data/ipc.mmap --db data/inventory.db
```

### 4. Run Automated Tests
Trigger the automated testing framework. It scans the `tests/` directory recursively, runs all script components, and outputs an execution pass/fail report to `reports/T2_tests.txt`:
```bash
./tools/fileops.sh test
```

### 5. Clean Build Artifacts
Wipe out all temporary object files (`.o`) and compiled binaries from the workspace to enforce a fresh re-build:
```bash
./tools/fileops.sh clean
```
