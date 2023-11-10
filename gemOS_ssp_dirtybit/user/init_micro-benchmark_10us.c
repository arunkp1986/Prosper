#include<ulib.h>

#define LLC_SIZE (1<<21)
#define NUM_ACCESS 10

#define MILLI_SEC 3000000 //3000000 cycles
#define STACK_ELEM  65536 // 64 pages
#define STACK_SIZE 262144 // 256KB, pages 
#define HEAP1_SIZE 2097152 // 2MB,  
#define HEAP1_ELEM 524288 // 512 pages 
#define HEAP2_SIZE 4194304 // 4MB,
#define HEAP2_ELEM 1048576 // 1024 pages 
#define REC_COUNT 1000 // number of operations


#define RDTSC_START()            \
        __asm__ volatile("RDTSCP\n\t" \
                         "mov %%edx, %0\n\t" \
                         "mov %%eax, %1\n\t" \
                         : "=r" (start_hi), "=r" (start_lo) \
                         :: "%rax", "%rbx", "%rcx", "%rdx");

#define RDTSC_STOP()              \
        __asm__ volatile("RDTSCP\n\t" \
                         "mov %%edx, %0\n\t" \
                         "mov %%eax, %1\n\t" \
                         "CPUID\n\t" \
                         : "=r" (end_hi), "=r" (end_lo) \
                         :: "%rax", "%rbx", "%rcx", "%rdx");

u64 uelapsed(u32 start_hi, u32 start_lo, u32 end_hi, u32 end_lo)
{
        u64 start = (((u64)start_hi) << 32) | start_lo;
        u64 end   = (((u64)end_hi)   << 32) | end_lo;
        return end-start;
}

unsigned lfsr_rand(unsigned seed){
    unsigned lfsr = seed;
    u16 bit;
    bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u;
    lfsr = (lfsr >> 1) | (bit << 15);
    return lfsr;
}

struct data{
        unsigned interval;
        unsigned offset;
        char ops;
        int size;
        int type;
    };

static void *thfunc1(void *arg){
    int *ptr = (int *) arg;
    while(1){
        sleep(*ptr);
        page_merge();
    }
}
void operations(unsigned num_ops, unsigned num_dept, struct data *dt, u32* heap1, u32* heap2){
    int prev_interval = dt->interval;
    u32 loop_counter = (1<<14);
    register int count = 0;
    u32 array[STACK_ELEM];
    //printf("interval:%u,type:%d,ops:%c,offset:%u,size:%u\n",dt->interval,dt->type,dt->ops,dt->offset,dt->size);
    for(int i=0; i<num_ops; i++){
        while(prev_interval < dt->interval){
            count = 0;
	    while(count < loop_counter){
                count += 1;
	    }
	    prev_interval += 1;
	}
	if(dt->type == 0){
            if(dt->ops == 'R'){
	        u32 offset = dt->offset;
		u32 curr_size = ((dt->size)>>2)?((dt->size)>>2):1;
		for(int j=0; j<curr_size; j++){
		     offset = (offset + j)%STACK_ELEM;
		     u32 value = *(u32*)(array+offset);
		}
	    }
	    if(dt->ops == 'W'){
	        u32 offset = dt->offset;
		u32 curr_size = ((dt->size)>>2)?((dt->size)>>2):1;
		for(int j=0; j<curr_size; j++){
		     offset = (offset + j)%STACK_ELEM;
		     *(u32*)(array+offset) = 1;
		}
	    }
	}
	else if(dt->type == 1){
            if(dt->ops == 'R'){
	        u32 offset = dt->offset;
		u32 curr_size = ((dt->size)>>2)?((dt->size)>>2):1;
		for(int j=0; j<curr_size; j++){
		     offset = (offset + j)%HEAP1_ELEM;
		     u32 value = *(u32*)(heap1+offset);
		}
	    }
	    if(dt->ops == 'W'){
	        u32 offset = dt->offset;
		u32 curr_size = ((dt->size)>>2)?((dt->size)>>2):1;
		for(int j=0; j<curr_size; j++){
                     offset = (offset + j)%HEAP1_ELEM;
		     *(u32*)(heap1+offset) = 1;
		}
	    }
	}
	else if(dt->type == 2){
            if(dt->ops == 'R'){
	        u32 offset = dt->offset;
		u32 curr_size = ((dt->size)>>2)?((dt->size)>>2):1;
		for(int j=0; j<curr_size; j++){
		     offset = (offset + j)%HEAP2_ELEM;
		     u32 value = *(u32*)(heap2+offset);
		}
	    }
	    if(dt->ops == 'W'){
	        u32 offset = dt->offset;
		u32 curr_size = ((dt->size)>>2)?((dt->size)>>2):1;
		for(int j=0; j<curr_size; j++){
		     offset = (offset + j)%HEAP2_ELEM;
		     *(u32*)(heap2+offset) = 1;
		}
	    }
	}	
	dt += 1;
    }
    num_dept = num_dept-1;
    if(num_dept > 0){
        operations(num_ops, num_dept, dt, heap1, heap2);
    }
}

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
    u32 seed = 0xACE1u;
    u32 rand_value = 0;
    register int count = 0;
    void *stackp;
    int thpid;
    int tharg;
    stackp = mmap(NULL, 8192, PROT_READ|PROT_WRITE, 0);
    if(stackp < 0){
        printf("Can not allocated stack\n");
        exit(0);
    }
    tharg = 100;// thread sleep time
    thpid = clone(&thfunc1, ((u64)stackp) + 8192, &tharg);
    if(thpid <= 0){
        printf("Error creating thread!\n");
        exit(0);
    }
    make_thread_ready(thpid);
    //u32 array[STACK_ELEM];
    u32* heap1 = (u32 *)mmap(NULL, HEAP1_SIZE, PROT_READ|PROT_WRITE, MAP_POPULATE|MAP_NVM);
    u32* gurd = (u32 *)mmap(NULL, 4096, PROT_READ, 0);
    u32* heap2 = (u32 *)mmap(NULL, HEAP2_SIZE, PROT_READ|PROT_WRITE, MAP_POPULATE|MAP_NVM);
    //printf("heap1:%lx, heap2:%lx\n",(unsigned long)heap1,(unsigned long)heap2);
    unsigned size = sizeof(struct data)*REC_COUNT;
    unsigned num_blocks = (size%4096)?(size>>12)+1:(size>>12);
    //printf("num blocks:%u\n",num_blocks);
    char* input = (char *)mmap(NULL, num_blocks*4096, PROT_READ|PROT_WRITE,MAP_POPULATE);
    if(input < 0){
        printf("mmap failed\n");
	exit(0);
    }
    for(int blk_off = 0; blk_off < num_blocks; blk_off++){
        read_blk((char*)((u64)input+(blk_off<<12)), blk_off);
    }
    struct data *dt = (struct data*)input;
    unsigned num_ops = 0;
    unsigned num_dept = 0;
    unsigned jump = 0;
    int i = 0;
    //printf("num count:%u\n",REC_COUNT);
    checkpoint_start();
    gem5_dump_stats();
    gem5_reset_stats();
    for(i=0; i<REC_COUNT; i+=jump){
        rand_value = rand_value ? lfsr_rand(rand_value) : lfsr_rand(seed);
	num_ops = (rand_value%10)+4;
	num_dept = (rand_value%4)+1;
	jump = num_ops * num_dept;
        operations(num_ops, num_dept, dt, heap1, heap2);
	dt += jump; //num_ops*num_dept
    }
    //printf("i:%d\n",i);
    checkpoint_end();
    gem5_dump_stats();
    gem5_reset_stats();
    checkpoint_stats();
    return 0;
}
