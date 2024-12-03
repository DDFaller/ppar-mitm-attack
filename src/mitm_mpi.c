#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "mitm.h"



/*
    This file reimplements the logic from the single thread mitm
    for a mpi environment.
*/

#define INSERT_TAG 1

/* Global Variables */
static int local_dict_size;             // Local shard size for each process
static int local_dict_available_space;
static struct entry *local_A;           // Local hash table
static struct entry *global_A;          // Global hash table (for gathering)
static int global_dict_size;            // Total global dictionary size
int rank;                               // MPI rank of the current process

typedef struct {
    struct entry *entries;              // Array of entries (key-value pairs)
    bool* sent_flags;
    u64 size;                           // Current size of the buffer
    u64 capacity;                       // Maximum capacity of the buffer
} unavailable_buffer;

// Global buffer for unavailable entries
unavailable_buffer buffer;


/************************* Helper Functions *************************/

/* Create an MPI datatype for the struct entry */
MPI_Datatype createEntryType() {
    MPI_Datatype entry_type;
    int lengths[3] = { 1, 1, 1 };

    // Calculate displacements
    struct entry dummy_entry;
    MPI_Aint base_address;
    MPI_Aint displacements[3];
    MPI_Get_address(&dummy_entry, &base_address);
    MPI_Get_address(&dummy_entry.k, &displacements[0]);
    MPI_Get_address(&dummy_entry.target_rank, &displacements[1]);
    MPI_Get_address(&dummy_entry.v, &displacements[2]);
    displacements[0] = MPI_Aint_diff(displacements[0], base_address);
    displacements[1] = MPI_Aint_diff(displacements[1], base_address);
    displacements[2] = MPI_Aint_diff(displacements[2], base_address);

    // Define MPI types and create the struct type
    MPI_Datatype types[3] = {MPI_UINT32_T, MPI_INT, MPI_UINT64_T};
    MPI_Type_create_struct(3, lengths, displacements, types, &entry_type);
    MPI_Type_commit(&entry_type);

    return entry_type;
}

/************************* Buffer Management *************************/

/* Initialize the unavailable buffer */
void init_unavailable_buffer(u64 capacity) {
    buffer.size = 0;
    buffer.capacity = capacity;
    buffer.entries = (struct entry*)malloc(capacity * sizeof(struct entry));
    buffer.sent_flags = (bool*)malloc(capacity * sizeof(bool));
    if (buffer.entries == NULL || buffer.sent_flags == NULL) {
        perror("Error allocating memory for unavailable buffer");
        exit(EXIT_FAILURE);
    }
    for (u64 i = 0; i < capacity; i++) {
        buffer.sent_flags[i] = false;
    }
}

/* Add a key-value pair to the unavailable buffer */
void add_to_unavailable_buffer(u64 key, u64 value, int target_node) {
    if (buffer.size >= buffer.capacity) {
        buffer.capacity *= 2;
        buffer.entries = (struct entry*)realloc(buffer.entries, buffer.capacity * sizeof(struct entry));
        if (buffer.entries == NULL) {
            perror("Error reallocating memory for unavailable buffer");
            exit(EXIT_FAILURE);
        }
    }
    buffer.entries[buffer.size].k = key;
    buffer.entries[buffer.size].v = value;
    buffer.entries[buffer.size].target_rank = target_node;
    buffer.size++;
}

void remove_from_unavailable_buffer(u64 key) {
    bool found = false;

    for (u64 i = 0; i < buffer.size; i++) {
        if (buffer.entries[i].k == key) {
            found = true;

            // Shift all subsequent elements to the left
            for (u64 j = i; j < buffer.size - 1; j++) {
                buffer.entries[j] = buffer.entries[j + 1];
            }

            // Decrease the buffer size
            buffer.size--;
            printf("Removed key %lu from the buffer.\n", key);
            break;
        }
    }

    if (!found) {
        printf("Key %lu not found in the buffer.\n", key);
    }
}

/* Print the contents of the unavailable buffer */
void print_unavailable_buffer() {
    if (buffer.size > 0){
        printf("Unavailable Buffer:\n");
        for (int i = 0; i < buffer.size; i++) {
            printf("Key: %u, Value: %lu Target_rank: %u\n", buffer.entries[i].k, buffer.entries[i].v, buffer.entries[i].target_rank);
        }
        return;
    }
    printf("Unavailable Buffer is EMPTY:\n");
}

void mpi_gather_buffers(int rank, int num_procs) {
    // MPI datatype for entries
    MPI_Datatype entryType = createEntryType();

    // Obter o tamanho máximo do buffer de cada nó
    int max_buffer_size;
    MPI_Allreduce(&buffer.size, &max_buffer_size, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

    // Alocar espaço para armazenar todos os buffers no nó mestre
    struct entry *global_buffers = NULL;
    int *buffer_sizes = (int*)malloc(num_procs * sizeof(int));
    int *displacements = (int*)malloc(num_procs * sizeof(int));
    if (rank == 0) {
        global_buffers = (struct entry*)malloc(max_buffer_size * sizeof(struct entry));
        if (!global_buffers) {
            perror("Erro ao alocar memória para buffers globais");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
    }

    // Reunir os tamanhos dos buffers locais
    MPI_Gather(&buffer.size, 1, MPI_INT, buffer_sizes, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank == 0){
        for (int i = 0; i < num_procs; i++)
        {
            printf("Buffer %d size %d\n",i,buffer_sizes[i]);
        }
    }
    
    // Calcular deslocamentos para o MPI_Gatherv
    if (rank == 0) {
        displacements[0] = 0;
        for (int i = 1; i < num_procs; i++) {
            displacements[i] = displacements[i - 1] + buffer_sizes[i - 1];
        }
    }

    // Reunir os buffers de cada nó no processo mestre
    MPI_Gatherv(buffer.entries, buffer.size, entryType,
                global_buffers, buffer_sizes, displacements, entryType, 0, MPI_COMM_WORLD);

    // Exibir os buffers no processo mestre
    if (rank == 0) {
        printf("Conteúdo dos buffers de cada nó:\n");
        for (int i = 0; i < num_procs; i++) {
            printf("Nó %d:\n", i);
            for (int j = 0; j < buffer_sizes[i]; j++) {
                int index = displacements[i] + j;
                printf("  Entry %d: Key: %u, Value: %lu, Target Rank: %d\n",
                       j, global_buffers[index].k, global_buffers[index].v, global_buffers[index].target_rank);
            }
        }
        free(global_buffers);
    }

    free(buffer_sizes);
    free(displacements);
}

/* Clear memory allocated for the unavailable buffer */
void destroy_unavailable_buffer() {
    free(buffer.entries);
    free(buffer.sent_flags);
    buffer.size = 0;
    buffer.capacity = 0;
}

/* Clear memory allocated for the unavailable buffer and then reinit the buffer like a reset */
void clear_unavailable_buffer() {
    destroy_unavailable_buffer();
    init_unavailable_buffer(local_dict_size);
}

/************************* Hash Table Functions *************************/

/* Initialize the dictionary for MPI processes */
void dict_setup_mpi(u64 size, int rank, int num_procs) {
    local_dict_size = size / num_procs;
    global_dict_size = size;
    local_dict_available_space = local_dict_size;
    rank = rank;
    // Allocate local hash table
    local_A = malloc(sizeof(struct entry) * local_dict_size);
    if (local_A == NULL) {
        fprintf(stderr, "Error allocating local dictionary on process %d.\n", rank);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    // Initialize local hash table with EMPTY values
    for (u64 i = 0; i < local_dict_size; i++) {
        local_A[i].k = EMPTY;
    }

    // Initialize the unavailable buffer
    init_unavailable_buffer(local_dict_size);
}

/* Verify duplicates in the hash table shard */
bool is_key_present(u64 key) {
    u64 h = murmur64(key) % local_dict_size;
    u32 k_mod = key % PRIME;

    int iterations = 0;
    while (local_A[h].k != EMPTY && iterations < local_dict_size) {
        if (local_A[h].k == k_mod) {
            // Key is already present in the local hash table
            return true;
        }
        h = (h + 1) % local_dict_size;
        iterations++;
    }
    return false;
}

u64 determine_target_node(u64 key, int target_rank, u64* local_index);

void dict_insert_entry(u64 key, u64 value, int target_rank) {
    // Determine target node and local index
    u64 local_index;
    u64 target_node = determine_target_node(key, target_rank, &local_index);

    if (target_node == rank) {
        // Handle insertion into the local hash table
        handle_local_insertion(key, value, local_index, target_node);
    } else {
        // Add the entry to the buffer
        buffer_entry(key, value, target_node);
    }
}

/************************* Funções Auxiliares *************************/

/* Determine the target node and local index for the key */
u64 determine_target_node(u64 key, int target_rank, u64* local_index) {
    u64 h = murmur64(key) % global_dict_size;
    u64 target_node = h / local_dict_size;
    *local_index = h % local_dict_size;

    if (target_rank != -1 && target_rank != target_node) {
        *local_index = 0;  // Start probing from the beginning
        target_node = target_rank;
    }
    return target_node;
}

/* Handle the insertion of an entry into the local hash table */
void handle_local_insertion(u64 key, u64 value, u64 local_index, u64 target_node) {
    if (is_key_present(key)) {
        remove_from_unavailable_buffer(key);
        printf("Key %lu is already present in Node %d, skipping insertion.\n", key, rank);
        return;
    }

    // Current node is full, buffer the entry for the next node
    if (local_dict_available_space == 0) {
        printf("Current node full\n");
        buffer_entry(key, value, (target_node + 1) % 4);
        return;
    }

    // Attempt to insert the entry using linear probing
    int iterations = local_index;
    while (local_A[local_index].k != EMPTY && iterations < local_dict_size) {
        local_index = (local_index + 1) % local_dict_size;
        iterations++;
    }

    //Probe to the next node
    if (iterations >= local_dict_size) {
        buffer_entry(key, value, (target_node + 1) % 4);
        return;
    }

    // Perform the insertion
    local_A[local_index].k = key % PRIME;
    local_A[local_index].v = value;
    local_A[local_index].target_rank = rank;
    local_dict_available_space--;
}

/* Add an entry to the unavailable buffer */
void buffer_entry(u64 key, u64 value, u64 target_node) {
    add_to_unavailable_buffer(key, value, target_node);
}

/* Clean up and free memory used by the dictionary */
void dict_cleanup_mpi() {
    printf("Cleaning up dictionary on process %d.\n", rank);
    free(local_A);
}
ex
void exchange_buffers_variable(int rank, int num_procs) {
    // Obter tamanhos de buffer de todos os processos
    int* buffer_sizes = (int*)malloc(num_procs * sizeof(int));
    int* displacements = (int*)malloc(num_procs * sizeof(int));
    MPI_Allgather(&buffer.size, 1, MPI_INT, buffer_sizes, 1, MPI_INT, MPI_COMM_WORLD);
    
    // Calcular deslocamentos para os buffers
    displacements[0] = 0;
    for (int i = 1; i < num_procs; i++) {
        displacements[i] = displacements[i - 1] + buffer_sizes[i - 1];
    }
    int global_buffer_size = displacements[num_procs - 1] + buffer_sizes[num_procs - 1];

    // Alocar buffers globais
    struct entry* global_entries = (struct entry*)malloc(global_buffer_size * sizeof(struct entry));
    int* global_target_nodes = (int*)malloc(global_buffer_size * sizeof(int));

    // Compartilhar os dados
    MPI_Allgatherv(buffer.entries, buffer.size, createEntryType(),
                   global_entries, buffer_sizes, displacements, createEntryType(), MPI_COMM_WORLD);
    
    clear_unavailable_buffer();
    assert(buffer.size == 0);
    // Processar apenas as entradas relevantes
    for (int i = 0; i < global_buffer_size; i++) {
        if (global_entries[i].target_rank == rank) {
            dict_insert_entry(global_entries[i].k, global_entries[i].v, global_entries[i].target_rank);
        }
    }
    

    // Limpar memória
    free(global_entries);
    free(global_target_nodes);
    free(buffer_sizes);
    free(displacements);
}

/* Verify thorugh all the buffers with the work has been completed */
bool is_work_done(int rank, int num_procs) {
    // Verificar se o buffer de indisponibilidade local está vazio
    int local_work_done = (buffer.size == 0) ? 1 : 0;

    // Reduzir o estado local de trabalho para verificar o estado global
    int global_work_done;
    MPI_Allreduce(&local_work_done, &global_work_done, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    assert(global_work_done >= local_work_done);
    printf("GLOBAL WORK STATUS %d\n",global_work_done);
    return global_work_done == num_procs;
}

/* Construct the global dict using the local ones, recommend just for testing */
void dict_gather_results(int rank, int num_procs) {
    // MPI datatype for entries
    MPI_Datatype entryType = createEntryType();

    // Tamanho da tabela hash global
    int global_table_size = global_dict_size;

    // Buffer para armazenar todas as tabelas hash no nó mestre
    struct entry *global_table = NULL;
    if (rank == 0) {
        global_table = (struct entry*)malloc(global_table_size * sizeof(struct entry));
        if (!global_table) {
            perror("Erro ao alocar memória para a tabela global");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
    }

    // Reunir as tabelas hash locais no nó mestre
    MPI_Gather(local_A, local_dict_size, entryType, global_table, local_dict_size, entryType, 0, MPI_COMM_WORLD);

    // Exibir os resultados no nó mestre
    if (rank == 0) {
        printf("Resultados das tabelas hash locais:\n");
        for (int i = 0; i < global_table_size; i++) {
            if (global_table[i].k != EMPTY) {
                printf("Rank %d: (%u, %lu, target_rank=%u)\n", 
                       i / local_dict_size, 
                       global_table[i].k, 
                       global_table[i].v, 
                       global_table[i].target_rank);
            } else {
                printf("Rank %d: EMPTY\n", i / local_dict_size);
            }
        }
        free(global_table);
    }
}

/************************* Golden Claw Search *************************/

/* Example of a parallel search operation using the distributed dictionary */
int golden_claw_search_mpi(int maxres, u64 k1[], u64 k2[], int rank, int num_procs) {
    // Parallel search implementation
    // Populate dictionary and gather results
    // Details omitted for brevity
    return 0;
}

