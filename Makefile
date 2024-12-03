# Compiler and flags
CC = mpicc
CFLAGS = -Wall

# Directories
SRC_DIR = src
TEST_DIR = tests
BUILD_DIR = build

# Source files
SRC_FILES = $(SRC_DIR)/mitm_mpi.c $(SRC_DIR)/mitm.c 
TEST_FILES = $(TEST_DIR)/test_shard.c 

# Output executables
EXECUTABLES = $(BUILD_DIR)/test_shard

# Default target
all: setup $(EXECUTABLES)

# Create build directory if not exists
setup:
	@mkdir -p $(BUILD_DIR)

# $@ = test_matrix_basic  $^ = $(TEST_DIR)/matrix_basic_operations.c $(SRC_FILES)
$(BUILD_DIR)/test_shard: $(TEST_DIR)/test_shard.c $(SRC_FILES)
	$(CC) $(CFLAGS) -o $@ $^ -lm

# Run all tests
run-tests: all
	@echo "Running test shard ..."
	@ mpiexec $(BUILD_DIR)/test_shard

clean:
	rm -rf $(BUILD_DIR)/*.o $(BUILD_DIR)/*