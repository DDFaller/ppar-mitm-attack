# Sorbonne Université - MODEL (S1-24)
# Distributed Computing Project - MPI Implementation
#
# Makefile for compiling, debugging, and executing experiments with MPI.
#
# Authors: FERNANDES MORENO Matheus (21400700)
#          MACHADO CARNEIRO FALLER Daniel (21400117)

# Compiler and flags
CC = mpicc
CFLAGS += -O3 -Wall -Wextra -g -std=c99 -Iinclude
LDFLAGS += -lm

# Paths
SRC_DIR = src
BUILD_DIR = build
LOGS_DIR = logs

# Source files
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)

# Object files
OBJ_FILES = $(SRC_FILES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Main binary
PROGRAM_BINARY = $(BUILD_DIR)/distributed_program

# Log file for this execution
RESULTS_LOG = $(LOGS_DIR)/results-$(shell date +%F-%T).log

# Number of processes for execution
NUM_PROCESSES = 4

# Execution parameters (configurable at runtime)
N ?= 25
C0 ?= 0ce1f5e3b2d4e8c8
C1 ?= 4f7b73b48e470ee6

# Compile only
build:
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SRC_FILES) -o $(PROGRAM_BINARY) $(LDFLAGS)

# Execute the program
run: build
	@mkdir -p $(LOGS_DIR)
	@echo "Running with $(NUM_PROCESSES) processes and dumping results in $(RESULTS_LOG)..."
	mpiexec -n $(NUM_PROCESSES) ./$(PROGRAM_BINARY) --n $(N) --C0 $(C0) --C1 $(C1)> $(RESULTS_LOG)

# Clean build directory
clean_build:
	rm -rf $(BUILD_DIR)

# Clean logs directory
clean_logs:
	rm -rf $(LOGS_DIR)

