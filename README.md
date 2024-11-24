# PPAR S1-24 – Implementation Project

**Authors:**
- Matheus FERNANDES MORENO
- Daniel MACHADO CARNEIRO FALLER

# Mitm attack with a distributed system

## Overview

We implemented a **distributed hash table** using **linear probing** for collision resolution, designed to operate across multiple nodes in a distributed system using **MPI (Message Passing Interface)**. Each node manages a shard of the hash table, and communication between nodes ensures efficient insertion, retrieval, and distribution of key-value pairs.

### Hash table features
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
