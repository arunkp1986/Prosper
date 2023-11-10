#include<types.h>
#include<apic.h>
#include<lib.h>
#include<idt.h>
#include<memory.h>
#include<context.h>
#include<entry.h>
#include <dirty.h>

//#define PGD_MASK 0xff8000000000UL 
//#define PUD_MASK 0x7fc0000000UL
//#define PMD_MASK 0x3fe00000UL
//#define PTE_MASK 0x1ff000UL

#define PGD_MASK 0xff8000000000UL 
#define PUD_MASK 0x007fc0000000UL
#define PMD_MASK 0x00003fe00000UL
#define PTE_MASK 0x0000001ff000UL

#define PGD_SHIFT 39
#define PUD_SHIFT 30
#define PMD_SHIFT 21
#define PTE_SHIFT 12


#define FLAG_MASK 0x3ffffffff000UL 
static unsigned long apic_base;

static void clflush(void *addr)
{
    asm volatile(
                   "clflush %0;"
                    :
                    :"m" (addr)
                    :"memory"
    );
   
}
void ack_irq()
{
    u32 *vector;
#ifndef PERIODIC_TIMER
    //Initial count
     vector = (u32 *)(apic_base + APIC_TIMER_INIT_COUNT_OFFSET); 
     *vector = APIC_INTERVAL; // initial timer value
     clflush(vector);
    //Current count
    vector = (u32 *)(apic_base + APIC_TIMER_CURRENT_COUNT_OFFSET); 
    *vector = APIC_INTERVAL; // current count
    clflush(vector);
#endif
    vector = (u32 *)(apic_base + APIC_EOI_OFFSET);  
    *vector = 0;
    return;
}

/*
void reset_timer(u32 interval)
{
    u32 *vector;
    vector = (u32 *)(apic_base + APIC_TIMER_INIT_COUNT_OFFSET); 
    *vector = interval; // initial timer value
    clflush(vector);
    //Current count
    vector = (u32 *)(apic_base + APIC_TIMER_CURRENT_COUNT_OFFSET); 
    *vector = interval; // current count
    clflush(vector);
    vector = (u32 *)(apic_base + APIC_EOI_OFFSET);
    *vector = 0;
    return;
}*/

int do_irq(struct user_regs *regs)
{
    int ret;
    extern u16 track_on;
    ret = handle_timer_tick(regs);
    return ret;
    //regs == stack pointer after pushing the registers  
}

int is_apic_base (u64 pfn)
{
   if(pfn == (apic_base >> PAGE_SHIFT))
       return 1;

   return 0;
}



void install_apic_mapping(u64 pl4)
{
   u64 address = (apic_base >> 21)<<21, pfn;
   u64 *pgd, *pmd, *pud;
   
   pl4 = pl4 << PAGE_SHIFT; 

   pgd = (u64 *)pl4 + ((address & PGD_MASK) >> PGD_SHIFT);
   if(((*pgd) & 0x1) == 0){
           pfn = os_pfn_alloc(OS_PT_REG);
           pfn = pfn << PAGE_SHIFT;
           *pgd = pfn  | 0x3;
   }else{
          pfn = ((*pgd) >> PAGE_SHIFT) << PAGE_SHIFT;  
   }
   pud = (u64 *)pfn + ((address & PUD_MASK) >> PUD_SHIFT);
   if(((*pud) & 0x1) == 0){
           pfn = os_pfn_alloc(OS_PT_REG);
           pfn = pfn << PAGE_SHIFT;
           *pud = pfn | 0x3;
   }else{
          pfn = ((*pud) >> PAGE_SHIFT) << PAGE_SHIFT;  
   }
   pmd = (u64 *)pfn + ((address & PMD_MASK) >> PMD_SHIFT);
   if((*pmd) & 0x1)
        printk("BUG!\n");
   if(config->global_mapping && config->adv_global)
       *pmd = address | 0x183UL | (1UL << 52);
   else if(config->global_mapping)
       *pmd = address | 0x183;
   else
       *pmd = address | 0x83;
   //printk("%s pmd=%x *pmd=%x\n", __func__, pmd, *pmd);
   return;
}

void remove_apic_mapping(u64 pl4)
{
   u64 address = (apic_base >> 21)<<21, pfn;
   u64 *pgd, *pmd, *pud;
   
   pl4 = pl4 << PAGE_SHIFT; 

   pgd = (u64 *)pl4 + ((address & PGD_MASK) >> PGD_SHIFT);
   if(((*pgd) & 0x1) == 0){
           printk("APIC free BUG! pgd\n");
   }else{
          pfn = ((*pgd) >> PAGE_SHIFT) << PAGE_SHIFT;  
   }
   pud = (u64 *)pfn + ((address & PUD_MASK) >> PUD_SHIFT);
   if(((*pud) & 0x1) == 0){
           printk("APIC free BUG! pud\n");
   }else{
          pfn = ((*pud) >> PAGE_SHIFT) << PAGE_SHIFT;  
   }
   pmd = (u64 *)pfn + ((address & PMD_MASK) >> PMD_SHIFT);
   //printk("%s pmd=%x *pmd=%x\n", __func__, pmd, *pmd);
   if(((*pmd) & 0x1) != 0x1)
        printk("APIC free BUG!\n");
   *pmd = 0;
   return;
  
}
u32 check_point = 0;
void init_apic()
{
    u32 msr = IA32_APIC_BASE_MSR;
    u64 base_low, base_high;
    u32 *vector;
    dprintk("Initializing APIC\n");
    asm volatile(
                    "movl %2, %%ecx;"
                    "rdmsr;"
                    :"=a" (base_low), "=d" (base_high)
                    :"r" (msr)
                    : "memory"
    );
    base_high = (base_high << 32) | base_low;
    dprintk("APIC MSR = %x\n", base_high);
    dprintk("APIC initial state = %d\n", (base_high & (0x1 << 11)) >> 11);
    base_high = (base_high >> 12) << 12;
    dprintk("APIC Base address = %x\n", base_high);
    dprintk("APIC ID = %u\n", *((int *)(base_high + APIC_ID_OFFSET)));
    dprintk("APIC VERSION = %x\n", *((int *)(base_high + APIC_VERSION_OFFSET)));
    apic_base = base_high;
    /*Setup LDR and DFR*/
    vector = (u32 *)(base_high + APIC_DFR_OFFSET);  
    dprintk("DFR before = %x\n", *vector);
    *vector = 0xffffffff;
    clflush(vector);
    vector = (u32 *)(base_high + APIC_LDR_OFFSET);  
    dprintk("LDR before = %x\n", *vector);
    *vector = (*vector) & 0xffffff;
    *vector |= 1;
    
    /*Initialize the spurious interrupt LVT*/
    vector = (u32 *)(base_high + APIC_SPURIOUS_VECTOR_OFFSET);  
    *vector = 1 << 8 | IRQ_SPURIOUS;   
    clflush(vector);
    dprintk("SPR = %x\n", *((int *)(base_high + APIC_SPURIOUS_VECTOR_OFFSET)));
    
    /*APIC timer*/
    // Divide configuration register
    vector = (u32 *)(base_high + APIC_TIMER_DIVIDE_CONFIG_OFFSET); 
    *vector = 1011; // 1010 --> Divide by 128
    clflush(vector);
    //Initial count
    vector = (u32 *)(base_high + APIC_TIMER_INIT_COUNT_OFFSET); 
    *vector = APIC_INTERVAL; // initial timer value
    clflush(vector);
    //Current count
    vector = (u32 *)(base_high + APIC_TIMER_CURRENT_COUNT_OFFSET); 
    *vector = APIC_INTERVAL; // current count
    clflush(vector);
    //The timer LVT in periodic mode
    vector = (u32 *)(base_high + APIC_LVT_TIMER_OFFSET); 
    #ifdef  PERIODIC_TIMER
    *vector = APIC_TIMER | (1 << 17);
    #else
    *vector = APIC_TIMER;
     
    #endif
check_point = ((CHECKPOINT_INTERVAL/calibrate_apic_timer())/APIC_INTERVAL);

}

u64 elapsed(u32 start_hi, u32 start_lo, u32 end_hi, u32 end_lo)
{
        u64 start = (((u64)start_hi) << 32) | start_lo;
        u64 end   = (((u64)end_hi)   << 32) | end_lo;
        return end-start;
}


u32 calibrate_apic_timer(void){
    unsigned start_hi = 0, start_lo = 0, end_hi = 0, end_lo = 0;
    u32 counter1, counter2, ticks, ticks_min;
    u64 temp;
    u64 rdtsc_value = 0;
    u64 rdtsc_value_min;
    u32 *vector1;
    u32 *vector2;
    u32 cpu_apic_ticks = 0;
    counter1 = 10000; 
    ticks_min = rdtsc_value_min = 100000;
    vector1 = (u32 *)(apic_base + APIC_TIMER_INIT_COUNT_OFFSET); 
    vector2 = (u32 *)(apic_base + APIC_TIMER_CURRENT_COUNT_OFFSET);
    for(int i=0; i<5; i++){
        *vector1 = counter1;
        clflush(vector1);
        RDTSC_START();
        for(int j=0; j<100; j++){
            temp = 1+j;
        }
        RDTSC_STOP();
        counter2 = *vector2;
        ticks = counter1-counter2;
	//printk("counter1:%u, counter2:%u\n",counter1,counter2);
	//printk("iter:%d, ticks_min:%u, rdtsc_value_min:%u\n",i,ticks_min,rdtsc_value_min);
        if(ticks<ticks_min)
            ticks_min = ticks;
        rdtsc_value = elapsed(start_hi,start_lo,end_hi,end_lo);
        if(rdtsc_value<rdtsc_value_min)
            rdtsc_value_min = rdtsc_value;
    }
    if(rdtsc_value_min<ticks_min){
        printk("rdtsc_value_min less than ticks_min\n");
    }
    cpu_apic_ticks = (rdtsc_value_min/ticks_min); 
    //printk("cpu_apic_ticks:%u\n",cpu_apic_ticks);
    return cpu_apic_ticks;
}
