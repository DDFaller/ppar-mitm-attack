#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../mitm.h"
// Definições
#define INSERT_TAG 100

void test_message_exchange(int rank, int num_procs, MPI_Datatype entryType) {
    struct entry test_entry;
    MPI_Status status;
    int flag;

    if (rank == 0) {
        for (int target = 1; target < num_procs; target++) {
            test_entry.k = target * 10;
            test_entry.v = target * 20;
            printf("Node %d sending to Node %d: Key: %lu, Value: %lu\n",
                   rank, target, test_entry.k, test_entry.v);
            MPI_Send(&test_entry, 1, entryType, target, INSERT_TAG, MPI_COMM_WORLD);
        }
    } else {
        while (true){
            // Outros nós aguardam mensagens do nó 0
            MPI_Iprobe(0, INSERT_TAG, MPI_COMM_WORLD, &flag, &status);
            if (flag) {
                MPI_Recv(&test_entry, 1, entryType, 0, INSERT_TAG, MPI_COMM_WORLD, &status);
                printf("Node %d received: Key: %lu, Value: %lu from Node %d\n",
                    rank, test_entry.k, test_entry.v, status.MPI_SOURCE);
                break;
            } else {
                printf("Node %d did not receive any messages.\n", rank);
            }
        }
    }
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, num_procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    // Criar o tipo MPI para struct entry
    MPI_Datatype entryType = createEntryType();

    test_message_exchange(rank, num_procs, entryType);

    // Finalizar
    MPI_Finalize();
    return 0;
}
