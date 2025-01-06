# PPAR S1-24 – Implementation Project

**Authors:**
- Matheus FERNANDES MORENO
- Daniel MACHADO CARNEIRO FALLER

---

## MitM Attack with a Distributed System

### Overview

We implemented a **distributed hash table** using **linear probing** for collision resolution, designed to operate across multiple nodes in a distributed system using **MPI (Message Passing Interface)**. Each node manages a shard of the hash table, and communication between nodes ensures efficient insertion, retrieval, and distribution of key-value pairs.

#### Hash Table Features
- **Linear probing** for collision handling.
- **Buffer management** for storing key-value pairs that need to be redirected to other nodes.
- **Dynamic redirection** for handling full nodes and ensuring fault tolerance.
- **Global synchronization** for checking the completion of distributed operations.

---

## How It Works

1. **Initialization**:
   - Each node initializes a local shard of the hash table with a predefined size.
   - A global hash function determines the target node for each key.

2. **Key Insertion**:
   - Keys are hashed to a global index, and the appropriate target node is determined.
   - If the target node is full, keys are buffered and redirected to other nodes dynamically.

3. **Synchronization**:
   - Nodes periodically exchange buffer information using MPI to redistribute entries.
   - Global synchronization ensures all nodes complete their tasks before termination.

4. **Completion Check**:
   - A global `is_work_done` check ensures all buffers are empty and no pending operations remain.

---

## Requirements

- **C Compiler** (e.g., GCC)
- **MPI Library** (e.g., OpenMPI or MPICH)

---

## How to Run

The project is managed using a Makefile for building, running, and cleaning up the code.

### Build the Program
To compile the code, use the following command:
```bash
make build
```
### Running the program
It is possible to execute the program with default parameters by:
```bash
make run
```
This command will execute the algorithm in your computer using 4 CPU_CORES. However, it can be redefined as example bellow to run for another colision test.
```bash
make run N=28 C0=783f0f28839ed66e C1=50ca347a6d809ced
```

To run it on the GRID5000, we have scripts collision_finder.sh and perfomance_evaluation.sh that can be used as reference.

### Cleaning program residues
To remove all compiled files and binaries, use:
```bash
make clean_build
```
To remove all log files:
```bash
make clean_logs
```