# PPAR S1-24 – Final Project

**Authors:**
- Matheus FERNANDES MORENO
- Daniel MACHADO CARNEIRO FALLER

## MitM Attack with a Distributed System

### Overview

We implemented a **distributed hash table** using **linear probing** for collision resolution, designed to operate across multiple nodes in a distributed system using **MPI (Message Passing Interface)**. Each process manages a shard of the hash table, and communication between processes ensures efficient insertion, retrieval, and distribution of key-value pairs.

#### Hash Table Features

- **Linear probing** for collision handling.
- **Buffer management** for storing key-value pairs that must be redirected to other cores.
- **Global synchronization** for checking the completion of distributed operations.

## Results

We included a list of collisions achieved and outputs from Grid'5000 logs to evaluate the correctness of the project implementation. The files are:

**Collisions File**

- `results/collisions.txt`

Plain text file with the n parameter and the collision encountered by the algorithm.

**Execution Logs**

- `results/execution_output/mitm_performance_2735788.out`
- `results/execution_output/mitm_collisions_2722000.out`
- `results/execution_output/mitm_collisions_2724456.out`

Logs of the algorithm containing info about the resource usage and time taken for multiple executions of the algorithm. These files serve as evidence of the algorithm’s behavior under different configurations and parameter settings. They also allow for detailed inspection of performance metrics and intermediate steps.


## How It Works

1. **Initialization**:
   - Each process initializes a local shard of the hash table with a predefined size.
   - A global hash function determines the target process for each key.

2. **Key Insertion**:
   - Keys are hashed to a global index, and the appropriate target process is determined.
   - The key/value pairs are stored on buffers to be exchanged between cores.

3. **Synchronization**:
   - When a process buffer is full it triggers an exchange buffer information using MPI to redistribute entries.
   - Global synchronization ensures all process complete their tasks before termination.

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

This command will execute the algorithm in your computer using 4 CPU cores. However, it can be redefined as example bellow to run for another colision test.

```bash
make run NUM_PROCESSES=6 N=28 C0=783f0f28839ed66e C1=50ca347a6d809ced
```

To run it on the Grid'5000, we have the scripts `collision_finder.sh` and `perfomance_evaluation.sh` that can be used as reference.

### Cleaning program residues

To remove all compiled files and binaries, use:

```bash
make clean_build
```

To remove all log files:

```bash
make clean_logs
```

More details in the report present in the repository (`report.pdf`).

## Requirements

- **C Compiler** (e.g., GCC)
- **MPI Library** (e.g., OpenMPI or MPICH)
