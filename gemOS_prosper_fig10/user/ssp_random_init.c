#include<ulib.h>


#define PAGE_SIZE 4096
#define PAGE_ELEM 1024
#define PER_PAGE_ACCESS 100
#define MILLI_SEC 3000000 //3000000 cycles
#define STACK_ELEM  65536 // 64 pages
#define STACK_SIZE 262144 // 256KB, pages 
#define REC_COUNT 1000000 // number of operations


unsigned lfsr_rand(unsigned seed){
    unsigned lfsr = seed;
    u16 bit;
    bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u;
    lfsr = (lfsr >> 1) | (bit << 15);
    return lfsr;
}

unsigned operations(unsigned num_ops, int num_dept, u32* input){
    unsigned size = num_ops*PAGE_ELEM;
    u32 array[PAGE_ELEM*33];
    unsigned index = 0;
    unsigned count = 0;
    int j = 0;
    for(int i=0; i<num_ops; i++){
	j = 0;
        while(j<PER_PAGE_ACCESS){
            index = (i*PAGE_ELEM)+*(input+(i*PER_PAGE_ACCESS+j));
	    array[index] = i;
	    j++;
	}
    }
    while(count<1000){
        count += 1;
    }
    num_dept = num_dept-1;
    if(num_dept > 0){
        index = operations(num_ops, num_dept, input);
    }
    return index;
}

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
    register int count = 0;
    u32 seed = 0xACE1u;
    u32 rand_value = 0;
    u32 index = 0;
    u32* input = (u32 *)mmap(NULL, STACK_ELEM*sizeof(unsigned), PROT_READ|PROT_WRITE,MAP_POPULATE);
    if(input < 0){
        printf("mmap failed\n");
	exit(0);
    }
    for(int i=0; i<STACK_ELEM; i++){
        rand_value = rand_value ? lfsr_rand(rand_value) : lfsr_rand(seed);
	input[i] = rand_value%PAGE_ELEM;
    }
    unsigned num_ops = 0;
    unsigned num_dept = 0;
    unsigned jump = 0;
    checkpoint_start();
    gem5_dump_stats();
    gem5_reset_stats();
    index = input[0];
    for(int i=0; i<REC_COUNT; i+=jump){
	//printf("i:%d\n",i);
        rand_value = rand_value ? lfsr_rand(rand_value) : lfsr_rand(seed);
	num_ops = (rand_value%10)+24;
	num_dept = (rand_value%4)+1;
	//num_ops = 33;
	//num_dept = 4;
	jump = num_ops * num_dept;
        index = operations(num_ops, num_dept, input);
    }
    checkpoint_end();
    gem5_dump_stats();
    gem5_reset_stats();
    checkpoint_stats();
    return 0;
}