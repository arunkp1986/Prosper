#include<ulib.h>

#define LLC_SIZE (1<<21)

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
static void *thfunc1(void *arg){
    int *ptr = (int *) arg;
    while(1){
        sleep(*ptr);
        page_merge();
    }
}

unsigned operations(unsigned num_ops, unsigned num_dept){
    //unsigned index = start%STACK_ELEM;
    unsigned size = num_ops*1024;
    u32 array[size];
    register int count = 0;
    for(int i=0; i<size; i++){	    
	array[i] = i;
        //index = (index+1)%STACK_ELEM;
    }
    while(count<1000){
        count += 1;
    }
    num_dept = num_dept-1;
    if(num_dept > 0){
        operations(num_ops, num_dept);
    }
    return 0;
}

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
    u32 seed = 0xACE1u;
    u32 rand_value = 0;
    u32 index = 0;
    register int count = 0;
    unsigned num_ops = 0;
    unsigned num_dept = 0;
    unsigned jump = 0;
    void *stackp;
    int thpid;
    int tharg;
    stackp = mmap(NULL, 8192, PROT_READ|PROT_WRITE, 0);
    if(stackp < 0){
        printf("Can not allocated stack\n");
        exit(0);
    }
    tharg = 1000;// thread sleep time
    thpid = clone(&thfunc1, ((u64)stackp) + 8192, &tharg);
    if(thpid <= 0){
        printf("Error creating thread!\n");
        exit(0);
    }
    make_thread_ready(thpid);
    checkpoint_start();
    gem5_dump_stats();
    gem5_reset_stats();
    for(int i=0; i<REC_COUNT; i+=jump){
        rand_value = rand_value ? lfsr_rand(rand_value) : lfsr_rand(seed);
	num_ops = (rand_value%5)+20; //24
	num_dept = (rand_value%4)+1;
	jump = num_ops * num_dept;
        operations(num_ops, num_dept);
    }
    checkpoint_end();
    gem5_dump_stats();
    gem5_reset_stats();
    checkpoint_stats();
    return 0;
}
