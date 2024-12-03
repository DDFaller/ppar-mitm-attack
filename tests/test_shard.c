#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
#include <unistd.h>
#include "../src/mitm.h"


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


int main(int argc, char **argv){
    test_shard(argc,argv);
}