#!/bin/bash
#
# Sorbonne Université - PPAR (S1-24)
# Project - Direct Meet-in-the-Middle Attack
#
# Script to check the performance of the parallel code.
# Run it on Grid'5000 with `oarsub -S ./performance_evaluation.sh`.
#
# Authors: Matheus FERNANDES MORENO
#          Daniel MACHADO CARNEIRO FALLER

#OAR -p paradoxe
#OAR -l nodes=5/core=52,walltime=10:00:00
#OAR -O mitm_performance_%jobid%.out
#OAR -E mitm_performance_%jobid%.err

NUM_CORES_DEFAULT=128

export OMPI_MCA_osc='^ucx'
export OMPI_MCA_pml='^ucx'
export OMPI_MCA_btl='^openib'

# For debugging: list name of hosts
mpiexec --map-by ppr:1:node --hostfile $OAR_NODEFILE hostname

# Compile the program
mpicc -g mitm_parallel.c -O3 -Wall -Wextra -lm -D EARLY_EXIT=0 -o mitm_parallel
chmod u+x mitm_parallel

# Evaluate performance for different number of cores
echo "/// results ///"
for num_cores in 1 2 4 8 16 32 64 128 256
do
    mpiexec -n $num_cores --hostfile $OAR_NODEFILE ./mitm_parallel \
        --n 28 --C0 5729420892ef35f6 --C1 ba0807047a8a06d1
done
echo "/// end ///"

# Evaluate performance for different n
arguments=(
    "--n 25 --C0 0f540bf824f87e3f --C1 6cc642b7e61ee75e"
    "--n 26 --C0 45969a884ea5be9b --C1 d2832ca643ecaf99"
    "--n 27 --C0 51e42abda56cf674 --C1 cf9e0c42db67fe44"
    "--n 28 --C0 5729420892ef35f6 --C1 ba0807047a8a06d1"
    "--n 29 --C0 1640fca6b641fb0a --C1 d49fefa475c22b86"
    "--n 30 --C0 a0bca8205bffb86d --C1 9cc9039976126254"
    "--n 31 --C0 be3b6f082c131145 --C1 70d706add9e8817a"
    "--n 32 --C0 90cf2e7ffa21765c --C1 52e6981fd082ce71"
    "--n 33 --C0 77e8d746a6515fb6 --C1 e8e9cb4712662819"
    "--n 34 --C0 bd9d052099dfbb5d --C1 69dc56369f58ac81"
)
echo "/// results ///"
for arg_list in "${arguments[@]}"
do
    # ppr:10:node since we have 26 nodes and need 256 cores
    mpiexec -n $NUM_CORES_DEFAULT --hostfile $OAR_NODEFILE ./mitm_parallel \
        $arg_list
done
echo "/// end ///"

# Evaluate peformance for different levels of compression
# n=33 requires approximately 136GB of RAM
echo "/// results ///"
for mem in 200 100 50 25 12 6 3 2
do
    mpiexec -n $NUM_CORES_DEFAULT --hostfile $OAR_NODEFILE ./mitm_parallel \
        --n 33 --C0 77e8d746a6515fb6 --C1 e8e9cb4712662819 --mem $mem
done
echo "/// end ///"
