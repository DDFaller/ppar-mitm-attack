#include <stdio.h>

struct optimized_entry {
    u32 k;
    int target_rank;
    u64 v;
};

int main(int argc, char **argv){

    printf("Sizeof struct entry %d\n",sizeof(struct entry));
    printf("Sizeof u32 %d\n",sizeof(u32));
    printf("Sizeof u64 %d\n",sizeof(u64));
    printf("Sizeof optimized entry %d\n",sizeof(struct optimized_entry));    
    printf("Size of int %d\n",sizeof(int));
}