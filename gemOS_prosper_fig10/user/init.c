#include<ulib.h>

#define NUM_ACCESS 150

unsigned lfsr_rand(unsigned seed){
    unsigned lfsr = seed;
    u16 bit;
    bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u;
    lfsr = (lfsr >> 1) | (bit << 15);
    return lfsr;
}
/*import numpy as np
s = np.random.poisson(65, 100)
mean 65
*/
int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
    u32 num_elements = 10000;
    register int count asm("r12");
    count  = 0;
    u32 seed = 0xACE1u;
    u32 rand_value = 0;
    u32 index = 0;
    u32 access = 0;
    u32 input[10000];
    int access_nums = 100;
    int access_array[100] =  {65, 67, 59, 69, 64, 71, 48, 76, 50, 60, 56, 56, 47, 74, 58, 71, 69, 73, 63, 67, 64, 55, 64, 58, 71, 50, 65, 72, 77, 53, 50, 72, 71, 68, 81, 76, 65, 51, 83, 71, 78, 54, 65, 58, 67, 52, 75, 70, 78, 55, 62, 59, 70, 66, 66, 67, 73, 67, 60, 84, 51, 67, 65, 54, 62, 75, 62, 48, 58, 71, 61, 56, 57, 70, 57, 66, 70, 57, 59, 64, 65, 66, 67, 72, 74, 81, 58, 44, 58, 53, 65, 66, 60, 57, 64, 59, 61, 52, 70, 61};
    for(int i=0; i<num_elements; i++){
        rand_value = rand_value ? lfsr_rand(rand_value) : lfsr_rand(seed);
	input[i] = rand_value%num_elements;
    }
    checkpoint_start();
    gem5_dump_stats();
    gem5_reset_stats();
    for(int k=0; k<NUM_ACCESS; k++){
        for(int i=0; i<access_nums; i++){
            index = input[0];
	    access = access_array[i];
            for(int j=0; j<access; j++){
                index = input[index];
                input[index] += 0;
	    }
	    count = 0;
	    while(count<1000){
                count += 1;
	    }
	}
    }
    checkpoint_end();
    gem5_dump_stats();
    gem5_reset_stats();
    checkpoint_stats();
    return 0;
}
