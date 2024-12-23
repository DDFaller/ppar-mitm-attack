#!/bin/bash
#
# Sorbonne Université - PPAR (S1-24)
# Project - Direct Meet-in-the-Middle Attack
#
# Script to search for golden collisions!
# Run it on Grid'5000 with `oarsub -S ./collision_finder.sh`.
#
# The arguments were retrieved for the username "matheus.daniel".
# For n < 21, we used the sequential algorithm.
#
# Authors: Matheus FERNANDES MORENO
#          Daniel MACHADO CARNEIRO FALLER

#OAR -p fleckenstein
#OAR -l nodes=6/core=32,walltime=14:00:00
#OAR -O mitm_collisions_%jobid%.out
#OAR -E mitm_collisions_%jobid%.err
#OAR --notify [RUNNING]mail:Matheus.Fernandes_Moreno@etu.sorbonne-universite.fr

NUM_CORES=128
MEM_AVAILABLE=3072
ARGUMENTS=(
    "--n 21 --C0 fd4954f413eccb41 --C1 889abfcb908fc6b6"
    "--n 22 --C0 46c1ec5a992f061e --C1 373958e4f1f6d266"
    "--n 23 --C0 63cd111da28dc1a6 --C1 441be85e84ba6a2a"
    "--n 24 --C0 01dc33215e01c148 --C1 3b78ab11d45d3d2c"
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
    "--n 35 --C0 f40a51c51cf56829 --C1 70a8219d7662b9cf"
    "--n 36 --C0 e9e004ca30b21e54 --C1 039d690bb07c9343"
    "--n 37 --C0 101632cd47b6bc59 --C1 6123734c5e2095c6"
    "--n 38 --C0 34758c6de16ac64a --C1 d60104493bfef3c7"
    "--n 39 --C0 83b63c762d1c3022 --C1 5d54033dc8a433ce"
    "--n 40 --C0 c63b1d35aab2a0b9 --C1 af3ebce65531a5d1"
    "--n 41 --C0 d415e83a967e127d --C1 da97e1a1d303d8dc"
    "--n 42 --C0 38d7050f5e176c6c --C1 a88919c2e87c6458"
    "--n 43 --C0 d8709b35cce70795 --C1 b5d8556298ffeace"
    "--n 44 --C0 7a928a5ecb53ec39 --C1 f83c92849b1c71e7"
    "--n 45 --C0 fa88585e027f2e73 --C1 b61cfe1867fcf458"
    "--n 46 --C0 8bd5615d6d1b7383 --C1 f958ff9ca8d1ae81"
    "--n 47 --C0 afb23b592ca8fc42 --C1 60c282a8bf24e56f"
    "--n 48 --C0 0b7ccfbb5cbe9a5e --C1 b48bedcfcb908270"
    "--n 49 --C0 56243f20c0b2c92d --C1 35a73e92e0f9700a"
    "--n 50 --C0 93014089a4d28ab5 --C1 356aae8a203c407d"
)

export OMPI_MCA_osc='^ucx'
export OMPI_MCA_pml='^ucx'
export OMPI_MCA_btl='^openib'

# For debugging: list name of hosts
mpiexec --map-by ppr:1:node --hostfile $OAR_NODEFILE hostname

# Compile the program
mpicc -g mitm_parallel.c -O3 -Wall -Wextra -lm -o mitm_parallel
chmod u+x mitm_parallel

# Search for collisions!
for arg_list in "${ARGUMENTS[@]}"
do
    echo "/// new collision search start ///"
    mpiexec -n $NUM_CORES --hostfile $OAR_NODEFILE ./mitm_parallel \
        $arg_list --mem $MEM_AVAILABLE
done
