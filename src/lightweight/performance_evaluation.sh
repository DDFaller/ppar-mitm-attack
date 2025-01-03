#!/bin/bash
#
# Sorbonne Université - PPAR (S1-24)
# Project - Direct Meet-in-the-Middle Attack
#
# Script to check the performance of the parallel code.
# Run it on Grid'5000 with `oarsub -S ./performance_evaluation.sh`.
#
# NOTE: We do not evaluate the performance for the different values of n.
# When we run the collision finder script, it computes this data for us.
# Therefore, we will use the output from that script in our evaluations.
#
# Authors: Matheus FERNANDES MORENO
#          Daniel MACHADO CARNEIRO FALLER

#OAR -p paradoxe
#OAR -l nodes=5/core=52,walltime=6:00:00
#OAR -O mitm_performance_%jobid%.out
#OAR -E mitm_performance_%jobid%.err

N_DEFAULT=28
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
        --n $N_DEFAULT --C0 5729420892ef35f6 --C1 ba0807047a8a06d1
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
