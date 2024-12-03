#ifndef MITM_H
#define MITM_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <getopt.h>
#include <stddef.h>
#include <mpi.h>

typedef uint64_t u64;       /* portable 64-bit integer */
typedef uint32_t u32;       /* portable 32-bit integer */

struct entry {
    u32 k;
    int target_rank;
    u64 v;
};

extern const u32 EMPTY;
extern const u64 PRIME;

/***************************** global variables ******************************/

extern u64 n;         /* block size (in bits) */
extern u64 mask;      /* this is 2**n - 1 */

extern u64 dict_size; /* number of slots in the hash table */
extern struct entry *A;   /* the hash table */

/* (P, C) : two plaintext-ciphertext pairs */
extern u32 P[2][2];
extern u32 C[2][2];

// MPI Global variables
extern int rank;

/************************ tools and utility functions *************************/

double wtime();
u64 murmur64(u64 x);
void human_format(u64 n, char *target);

/******************************** SPECK block cipher **************************/

void Speck64128KeySchedule(const u32 K[], u32 rk[]);
void Speck64128Encrypt(const u32 Pt[], u32 Ct[], const u32 rk[]);
void Speck64128Decrypt(u32 Pt[], const u32 Ct[], u32 const rk[]);

/******************************** dictionary MPI ********************************/
/* Initialization function to build local hash tables*/
void dict_setup_mpi(u64 global_size, int rank, int num_procs);

#define dict_insert_mpi(key,value) dict_insert_entry(key,value,-1)
/* Tries to insert a entry into the local hash table, if the key isn't destined
    to that machine node instead add the entry to the unavailable buffer
*/
void dict_insert_entry(u64 key, u64 value,int target_rank);

/* TODO: */
int dict_probe_mpi(u64 key, int maxval, u64 values[], int rank, int num_procs);

/* Destroy local hash table */
void dict_cleanup_mpi();

/* Verify thorugh all the buffers with the work has been completed */
bool is_work_done(int rank, int num_procs);

/* Share unavailabe buffer to all nodes, using allgather.*/
void exchange_buffers_variable(int rank, int num_procs);


/******************************** MPI test functions ********************************/
/* Recreate the global dictionary gathering each local hash table */
void dict_gather_results(int rank, int num_procs);

/* Generate a global unavailable buffer gathering results from each node */
void mpi_gather_buffers(int rank, int num_procs);

/***************************** MITM problem ***********************************/

u64 f(u64 k);
u64 g(u64 k);
bool is_good_pair(u64 k1, u64 k2);

/* Added golden claw search mpi*/
int golden_claw_search_mpi(int maxres, u64 k1[], u64 k2[], int rank, int num_procs);

void usage(char **argv);
void process_command_line_options(int argc, char **argv);


/***************************** MPI CREATE TYPE ***********************************/
MPI_Datatype createEntryType();
MPI_Datatype createShardType(MPI_Datatype entryType, int length );

#endif /* MITM_H */
