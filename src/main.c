#include "mitm.h"
#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
#include "mitm.h"
#include <unistd.h>

int mitm(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, num_procs;
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    process_command_line_options(argc, argv);
    dict_setup_mpi(1.125 * (1ull << n), rank, num_procs);

    u64 k1[16], k2[16];
    int nkey = golden_claw_search_mpi(16, k1, k2, rank, num_procs);

    if (rank == 0) {
        for (int i = 0; i < nkey; i++) {
            printf("Solution found: (%" PRIx64 ", %" PRIx64 ")\n", k1[i], k2[i]);
        }
    }

    dict_cleanup_mpi();
    MPI_Finalize();
    return 0;
}

int test_shard(int argc, char **argv){
        
    MPI_Init(&argc, &argv);

    int num_procs,test_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_rank(MPI_COMM_WORLD, &test_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    fprintf(stdout,"Meu rank %d/%d\n",test_rank,num_procs);

    int * rank_values;
    int elements_per_procs = 12;
    rank_values = (int*)malloc(sizeof(int) * elements_per_procs);
    
    dict_setup_mpi(elements_per_procs,rank,num_procs);
    
    
    for (int i = 0; i < elements_per_procs; i++)
    {
        rank_values[i] = i;
        dict_insert_mpi(rank_values[i],rank_values[i]);
    }

    int communication_count = 0;
    while(!is_work_done(rank,num_procs)){
        printf("Distributing remaining key-value pairs ITERATION > %d\n",communication_count);
        mpi_gather_buffers(rank,num_procs);
        dict_gather_results(rank,num_procs);
        exchange_buffers_variable(rank,num_procs);
        communication_count++;
        sleep(5);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Finalize();
    return 0;
}   

int main2(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, num_procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    process_command_line_options(argc, argv);
    
    u64 k1[16], k2[16];

    MPI_Datatype entryType =  createEntryType();

    struct entry A;
    A.k = 32;
    A.v = 64;

    struct entry * B = (struct entry *)malloc(sizeof(struct entry) * 3);

    MPI_Gather(&A,1,entryType,B,1,entryType,0,MPI_COMM_WORLD);
    
    if (rank == 0) {
        for (int i = 0; i < 3; i++){
            printf("Rank %d : (%u, %lu)\n", 
                rank,B[i].k, B[i].v);
        }
    }

    dict_cleanup_mpi();
    MPI_Finalize();
    return 0;
}



struct optimized_entry {
    u32 k;
    int target_rank;
    u64 v;
};
int struct_padding_verification(int argc, char **argv){
    // test_shard(argc,argv);

    printf("Sizeof struct entry %d\n",sizeof(struct entry));
    printf("Sizeof u32 %d\n",sizeof(u32));
    printf("Sizeof u64 %d\n",sizeof(u64));
    printf("Sizeof optimized entry %d\n",sizeof(struct optimized_entry));    
    printf("Size of int %d\n",sizeof(int));
}


int main(int argc, char **argv){
    test_shard(argc,argv);
}