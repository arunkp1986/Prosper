#include<types.h>
#include<msr.h>
#include<lib.h>

u64 readmsr(u32 msr){
    u64 low;
    u64 high;
    asm volatile("rdmsr"
		    :"=d" (high),"=a" (low)
		    :"c"(msr)
		    :
    );
    return (low|(high<<32));
}

void writemsr(u32 msr, u64 val){
    u32 low = val & 0xFFFFFFFF;
    u32 high = val >> 32;
    asm volatile("wrmsr" : :"c"(msr),"d"(high),"a"(low):"memory");
    return;
}

