#include<types.h>
#include<lib.h>
#include<context.h>
#include<memory.h>
#include<init.h>
#include<apic.h>
#include<idt.h>
#include<page.h>
#include<file.h>
#include<entry.h>
#include <msr.h>
#include <nonvolatile.h>
#include <mmap.h>
#include <dirty.h>

static struct exec_context *current; 
static struct exec_context *ctx_list;

//https://stackoverflow.com/questions/62757008/how-to-use-m5-in-gem5-20-linking-it-with-my-own-c-program
void x86_reset_gem5_stats()
{
        //printk("Resetting gem5 stats...\n");
        // reset code 0x40
        __asm__ volatile (".word 0x040F; .word 0x0040;" : : "D"(0), "S"(0) :);
}

void x86_dump_gem5_stats()
{
        //printk("Dumping gem5 stats...\n");
        // reset code 0x40
        __asm__ volatile (".word 0x040F; .word 0x0041;" : : "D"(0), "S"(0) :);
}

void reset_exec_ctx()
{
  int ctr;
  struct exec_context *ctx = ctx_list;
  for(ctr=0; ctr < MAX_PROCESSES; ++ctr){
     ctx->state = UNUSED; 
     ctx++;
  }
}


struct exec_context *get_new_ctx()
{
  int ctr;
  struct exec_context *ctx = ctx_list;
  for(ctr=0; ctr < MAX_PROCESSES; ++ctr){
      if(ctx->state == UNUSED){ 
          ctx->pid = ctr;
          ctx->state = NEW;
          return ctx; 
      }
     ctx++;
  }
  printk("%s: System limit on processes reached\n", __func__);
  return NULL;
}

struct exec_context *create_context(char *name, u8 type)
{
  int i;
  struct mm_segment *seg;
  struct exec_context *ctx = get_new_ctx();
  ctx->state = NEW;
  ctx->type = type;
  ctx->used_mem = 0;
  ctx->pgd = 0;
  ctx->os_stack_pfn = os_pfn_alloc(OS_PT_REG);
  ctx->os_rsp = (u64)osmap(ctx->os_stack_pfn + 1);

   // For the standart inputs FD

  ctx->files[0] = create_standard_IO(STDIN);
  ctx->files[1] = create_standard_IO(STDOUT);
  ctx->files[2] = create_standard_IO(STDERR);

  memcpy(ctx->name, name, strlen(name));
  seg = &ctx->mms[MM_SEG_CODE];
  
  seg->start = CODE_START;
  seg->end   = RODATA_START - 1;
  seg->access_flags = MM_RD | MM_EX;
  seg++; 
  
  seg->start = RODATA_START;
  seg->next_free = seg->start;
  seg->end   = DATA_START - 1;
  seg->access_flags = MM_RD;
  seg++; 
  
  seg->start = DATA_START;
  seg->end   = MMAP_START - 1;
  seg->access_flags = MM_RD | MM_WR;
  seg++; 
  
  seg->start = STACK_START - MAX_STACK_SIZE;
  seg->end   = STACK_START;
  seg->access_flags = MM_RD | MM_WR;

  return ctx; 
}

static u64 os_pmd_pfn;

int install_os_pts_range(struct exec_context *ctx, u64 start, u64 length){
    u32 pl4 = ctx->pgd;
    u32 num_2MB = (length>>21);
    u64 pfn = 0;
    u64 pgd_base = 0;
    u64 pud_base = 0;
    u64 pmd_base = 0;
    u64 start_address = (start>>PAGE_2MB_SHIFT)<<PAGE_2MB_SHIFT;
    u64 pt_base = (u64)pl4 << PAGE_SHIFT;
    u64 address = start_address;
    pgd_base = pt_base;
    unsigned long * entry;
    for(int i=0; i<num_2MB; i++){
	//printk("address:%x\n",address);
        entry = (unsigned long *)(pgd_base)+((address & PGD_MASK) >> PGD_SHIFT);
	//printk("entry:%x\n",entry);
        if(*entry & 0x1){
            pt_base = *entry & FLAG_MASK;  
        }else{
	    pfn = os_pfn_alloc(OS_PT_REG);
            *entry = (pfn << PAGE_SHIFT) | 0x3;
            pt_base = pfn << PAGE_SHIFT; 
       }
       pud_base = pt_base;
       entry = (unsigned long*)(pud_base)+((address & PUD_MASK) >> PUD_SHIFT);
       if(*entry & 0x1){
            pt_base = *entry & FLAG_MASK;
	    //printk("entry address:%x inside pmd entry:%x\n",entry,pt_base);
       }else{
            /*Create PMD entry in PUD*/
           pfn = os_pfn_alloc(OS_PT_REG);
	   //printk("pmd pfn:%x\n",pfn);
	   os_pmd_pfn = pfn;
           *entry = (pfn << PAGE_SHIFT) | 0x3;
           pt_base = pfn << PAGE_SHIFT;
	}
	pmd_base = pt_base;
	//printk("pgd_base:%x pud_base:%x pmd_base:%x\n",pgd_base, pud_base, pmd_base);
	entry = (unsigned long *)(pmd_base)+((address & PMD_MASK) >> PMD_SHIFT);
        if(config->global_mapping)
            *entry = address | 0x183;
	else
	    *entry = address | 0x83;
	address += (1 << 21);
	}
    return 0;
}

/*
static int install_os_pts_range(struct exec_context *ctx, u64 start, u64 length){
    u32 pl4 = ctx->pgd;
    u32 num_2MB = (length>>21);
    u32 num_pud_entries = (num_2MB>>9);
    u64 pfn = 0;
    u64 pud_base = 0;
    u64 pmd_base = 0;
    u32 startpage_accesses = 0;
    u32 remaining_2MB = 0;
    u32 count = 0;
    u8 first = 1;
    u64 seq_pfn = start>>PAGE_2MB_SHIFT;
    u64 pt_base = (u64)pl4 << PAGE_SHIFT;
    unsigned long *entry = (unsigned long *)(pt_base)+((start & PGD_MASK) >> PGD_SHIFT);
    if(!*entry){
          //Create PUD entry in PGD
          pfn = os_pfn_alloc(OS_PT_REG);
          *entry = (pfn << PAGE_SHIFT) | 0x3;
          pt_base = pfn << PAGE_SHIFT; 
    }else{
          pt_base = *entry & FLAG_MASK;  
    }
    pud_base = pt_base;
    for(int i=0; i<num_pud_entries; i++){
        entry = (unsigned long*)(pud_base)+((start & PUD_MASK) >> PUD_SHIFT)+i;
        if(*entry){
            pt_base = *entry & FLAG_MASK;  
        }else{
            //Create PMD entry in PUD
            pfn = os_pfn_alloc(OS_PT_REG);
            *entry = (pfn << PAGE_SHIFT) | 0x3;
            pt_base = pfn << PAGE_SHIFT;
	}
	pmd_base = pt_base;
	if(first){
	    // create 2MB PMD entries
            entry = (unsigned long *)(pmd_base)+((start & PMD_MASK) >> PMD_SHIFT);
            startpage_accesses = (512-((start & PMD_MASK) >> PMD_SHIFT));
            for(int j=0; j<startpage_accesses; j++){
	        if(config->global_mapping)
                    *entry = (seq_pfn << PAGE_2MB_SHIFT) | 0x183;
	        else
		    *entry = (seq_pfn << PAGE_2MB_SHIFT) | 0x83;
	        entry++;
		seq_pfn++;
	    }
	    first = 0;
	    remaining_2MB = num_2MB-startpage_accesses;
	}
        entry = (unsigned long *)(pmd_base);
        count = remaining_2MB > 512 ? 512 : remaining_2MB;
        while(count){
           if(config->global_mapping)
                *entry = (seq_pfn << PAGE_2MB_SHIFT) | 0x183;  //Global
           else
                 *entry = (seq_pfn << PAGE_2MB_SHIFT) | 0x83;  
          entry++;
	  seq_pfn++;
          count--;
	}
        remaining_2MB -= 512;
    }
    //APIC mapping needed
    //do it after NVM region mapping is setup
    if(start == REGION_NVM_START)
        install_apic_mapping((u64) pl4); 
    return 0;
}
*/
/*
static int install_os_pts(u32 pl4)
{
    int num_2MB = OS_PT_MAPS, count=0, pmdpos=0;
    u64 pfn, pud_base, seq_pfn = 0;
    u64 pt_base = (u64)pl4 << PAGE_SHIFT;
    unsigned long *entry = (unsigned long *)(pt_base);
    if(!*entry){
          //Create PUD entry in PGD
          pfn = os_pfn_alloc(OS_PT_REG);
          *entry = (pfn << PAGE_SHIFT) | 0x3;
          pt_base = pfn << PAGE_SHIFT; 
    }else{
          pt_base = *entry & FLAG_MASK;  
    }
    pud_base = pt_base;
again:
    entry = (unsigned long *)(pud_base) + pmdpos;
    if(*entry){
         pt_base = *entry & FLAG_MASK;  
    }else{
        //Create PMD entry in PUD
        pfn = os_pfn_alloc(OS_PT_REG);
        os_pmd_pfn = pfn;
        *entry = (pfn << PAGE_SHIFT) | 0x3;
        pt_base = pfn << PAGE_SHIFT; 
       
    }
    entry = (unsigned long *)(pt_base);
    count = num_2MB > 512 ? 512 : num_2MB;
    while(count){
           if(config->global_mapping)
                *entry = (seq_pfn << 21) | 0x183;  //Global
           else
                 *entry = (seq_pfn << 21) | 0x83;  
          entry++;
          seq_pfn++;
          count--;
    }
    num_2MB -= 512;

    if(num_2MB > 0){
        ++pmdpos;
        goto again;
    }

//APIC mapping needed
    install_apic_mapping((u64) pl4); 
    return 0;
}
*/
void copy_os_pts(u64 src, u64 dst)
{
   int i;
   unsigned long *src_entry, *dst_entry;
   src = src << PAGE_SHIFT;
   dst = dst << PAGE_SHIFT;

   src_entry = (unsigned long *)(src);
   dst_entry = (unsigned long *)(dst);

   if(!(*src_entry) || !(*dst_entry)){
          printk("Error case1\n");
          return;
   }
   src = *src_entry & FLAG_MASK;
   dst = *dst_entry & FLAG_MASK;
   
   src_entry = (unsigned long *)(src);
   dst_entry = (unsigned long *)(dst);
   
   for(i=0; i<4; ++i)
   {
       dprintk("i=%d source entry = %x dst entry = %x\n", i, *src_entry, *dst_entry);
       *dst_entry = *src_entry;
       dst_entry++;
       src_entry++;
   }
   
   return;
     
}


u32 pt_walk(struct exec_context *ctx, u32 segment, u32 stack, u64 address)
{ 
   unsigned long pt_base = (u64) ctx->pgd << PAGE_SHIFT;
   unsigned long entry;
   struct mm_segment *mm = &ctx->mms[segment];
   unsigned long start = mm->start;
   if(stack)
       start = mm->end - PAGE_SIZE;
   if(address)
       start = address;

   
   entry = *((unsigned long *)(pt_base + ((start & PGD_MASK) >> PGD_SHIFT)));
   if((entry & 0x1) == 0)
        return -1;
 
   pt_base = entry & FLAG_MASK;
   
   entry = *((unsigned long *)pt_base + ((start & PUD_MASK) >> PUD_SHIFT));
   if((entry & 0x1) == 0)
        return -1;
   
   pt_base = entry & FLAG_MASK;
   
   entry = *((unsigned long *)pt_base + ((start & PMD_MASK) >> PMD_SHIFT));
   if((entry & 0x1) == 0)
        return -1;
   if(entry & (1UL<<7)){
       return (entry>>21);
   }

   pt_base = entry & FLAG_MASK;
   
   entry = *((unsigned long *)pt_base + ((start & PTE_MASK) >> PTE_SHIFT));
   if((entry & 0x1) == 0)
        return -1;
    
   return (entry >> PAGE_SHIFT);
        
}

u32 invalidate_pte(struct exec_context *ctx, unsigned long addr)
{
   u64 *pte_entry = get_user_pte(ctx, addr, 0);
   if(!pte_entry)
            return -1;
   u32 region = get_mem_region((*pte_entry >> PTE_SHIFT));
   os_pfn_free(region, (*pte_entry >> PTE_SHIFT) & 0xFFFFFFFF);
   *pte_entry = 0; 

   asm volatile ("invlpg (%0);" 
                  :: "r"(addr) 
                  : "memory"); 
   return 0;
}


u32 install_ptable(unsigned long base, struct  mm_segment *mm, u64 address, u32 upfn, u8 is_nvm)
{
   if(!address)
      address = mm->start;
   upfn = map_physical_page(base, address, mm->access_flags, upfn, is_nvm);
   return upfn;    
}

u32 map_physical_page(unsigned long base, u64 address, u32 access_flags, u32 upfn, u8 is_nvm)
{
   void *os_addr;
   u64 pfn;
   unsigned long *ptep  = (unsigned long *)base + ((address & PGD_MASK) >> PGD_SHIFT);    
   if(!*ptep)
   {
      pfn = os_pfn_alloc(OS_PT_REG);
      *ptep = (pfn << PAGE_SHIFT) | 0x7; 
      os_addr = osmap(pfn);
      bzero((char *)os_addr, PAGE_SIZE);
   }else 
   {
      os_addr = (void *) ((*ptep) & FLAG_MASK);
   }
   ptep = (unsigned long *)os_addr + ((address & PUD_MASK) >> PUD_SHIFT); 
   if(!*ptep)
   {
      pfn = os_pfn_alloc(OS_PT_REG);
      *ptep = (pfn << PAGE_SHIFT) | 0x7; 
      os_addr = osmap(pfn);
      bzero((char *)os_addr, PAGE_SIZE);
   } else
   {
      os_addr = (void *) ((*ptep) & FLAG_MASK);
   }
   ptep = (unsigned long *)os_addr + ((address & PMD_MASK) >> PMD_SHIFT); 
   if(!*ptep){
      pfn = os_pfn_alloc(OS_PT_REG);
      *ptep = (pfn << PAGE_SHIFT) | 0x7; 
      os_addr = osmap(pfn);
      bzero((char *)os_addr, PAGE_SIZE);
   } else
   {
      os_addr = (void *) ((*ptep) & FLAG_MASK);
   }
   ptep = (unsigned long *)os_addr + ((address & PTE_MASK) >> PTE_SHIFT); 
   if(!upfn){
      if(is_nvm){
          upfn = os_pfn_alloc(NVM_USER_REG);
      }else{
          upfn = os_pfn_alloc(USER_REG);
      }
   }
   *ptep = ((u64)upfn << PAGE_SHIFT) | 0x5;
   if(access_flags & MM_WR)
      *ptep |= 0x2;
   return upfn;    
}

/*
  Works only when count fits in last level page table 
  TODO  hope every user page is contineous 
*/
static u32 install_ptable_multi(unsigned long pgd, unsigned long start, int count, int write, u8 is_nvm)
{
    void *os_addr;
    u64 pfn, start_pfn, last_pfn;
    int ctr;
    unsigned long *ptep = (unsigned long *)pgd + ((start & PGD_MASK) >> PGD_SHIFT);    
    if(!*ptep){
                pfn = os_pfn_alloc(OS_PT_REG);
                *ptep = (pfn << PAGE_SHIFT) | 0x7; 
                os_addr = osmap(pfn);
                bzero((char *)os_addr, PAGE_SIZE);
    }else{
                os_addr = (void *) ((*ptep) & FLAG_MASK);
    }
   ptep = (unsigned long *)os_addr + ((start & PUD_MASK) >> PUD_SHIFT); 
   if(!*ptep){
                pfn = os_pfn_alloc(OS_PT_REG);
                *ptep = (pfn << PAGE_SHIFT) | 0x7; 
                os_addr = osmap(pfn);
                bzero((char *)os_addr, PAGE_SIZE);
   }else{
                os_addr = (void *) ((*ptep) & FLAG_MASK);
   }
   ptep = (unsigned long *)os_addr + ((start & PMD_MASK) >> PMD_SHIFT); 
   if(!*ptep){
                pfn = os_pfn_alloc(OS_PT_REG);
                *ptep = (pfn << PAGE_SHIFT) | 0x7; 
                os_addr = osmap(pfn);
                bzero((char *)os_addr, PAGE_SIZE);
   }else{
                os_addr = (void *) ((*ptep) & FLAG_MASK);
   }
   ptep = (unsigned long *)os_addr + ((start & PTE_MASK) >> PTE_SHIFT); 
   
   if(is_nvm){ 
       start_pfn = os_pfn_alloc(NVM_USER_REG);
   }else{
       start_pfn = os_pfn_alloc(USER_REG);
   }
   for(ctr=0; ctr < count; ++ctr){
       if(!ctr){
           pfn = start_pfn;
       }else{
             if(is_nvm){
                 pfn = os_pfn_alloc(NVM_USER_REG);
	     }else{
	         pfn = os_pfn_alloc(USER_REG);
	     }
             if(last_pfn != pfn - 1)
                 printk("BUG! PFN not in sequence\n");
         }
         *ptep = (pfn << PAGE_SHIFT) | 0x5;
         if(write)
              *ptep |= 0x2;
         ptep++;
         last_pfn = pfn;
   }
   return start_pfn;
}
struct ssp_entry* ptr_dirtymap = NULL;
int exec_init(struct exec_context *ctx, struct init_args *args)
{
   unsigned long pl4;
   extern unsigned long saved_ebp;  /*Useful when init exits*/
   extern unsigned long * saved_ebp_backup;  /*Useful when init exits*/
   u64 stack_start, sptr, textpfn, fmem;
   void *os_addr;
   printk("Setting up init process ...\n");
   ctx->pgd = os_pfn_alloc(OS_PT_REG);
   os_addr = osmap(ctx->pgd);
   bzero((char *)os_addr, PAGE_SIZE);
   textpfn = install_ptable_multi((u64) os_addr, ctx->mms[MM_SEG_CODE].start, CODE_PAGES, 0,0);//allocated in dram
   ctx->mms[MM_SEG_CODE].next_free = ctx->mms[MM_SEG_CODE].start + (CODE_PAGES << PAGE_SHIFT);
   //dprintk("creating SSP bitmap area ...\n");
   if(!ptr_dirtymap){
       unsigned dirty_num_bytes = (get_pages_region(NVM_USER_REG)*sizeof(struct ssp_entry));
       unsigned dirty_num_pages = dirty_num_bytes%PAGE_SIZE?
	       (dirty_num_bytes>>PAGE_SHIFT)+1:dirty_num_bytes>>PAGE_SHIFT;
       ptr_dirtymap = (struct ssp_entry*)os_page_alloc_pages(NVM_META_REG,dirty_num_pages);
       writemsr(MISCREG_DIRTYMAP_ADDR, (u64)ptr_dirtymap);
       //printk("dirty bitmap area: ptr:%x\n",ptr_dirtymap);
       //dprintk("init ssp bitmap entry list head\n");
       ssp_bitmap_list = (struct list_head*)os_chunk_alloc(sizeof(struct list_head),NVM_META_REG);
       init_list_head(ssp_bitmap_list);
       //checkpoint_stat_list = (struct list_head*)os_chunk_alloc(sizeof(struct list_head),NVM_META_REG);
       //init_list_head(checkpoint_stat_list);
   }
   writemsr(MISCREG_TRACK_START, (u64)MMAP_START);
   writemsr(MISCREG_TRACK_END, (u64)MMAP_AREA_END);
   install_ptable((u64) os_addr, &ctx->mms[MM_SEG_DATA], 0, 0, 0); //allocates in dram   
   ctx->mms[MM_SEG_DATA].next_free = ctx->mms[MM_SEG_DATA].start + PAGE_SIZE;
   stack_start = ctx->mms[MM_SEG_STACK].start;
   ctx->mms[MM_SEG_STACK].start = ctx->mms[MM_SEG_STACK].end - PAGE_SIZE;
   install_ptable((u64) os_addr, &ctx->mms[MM_SEG_STACK], 0, 0, 0); //allocates in dram
   ctx->mms[MM_SEG_STACK].start = stack_start;
   ctx->mms[MM_SEG_STACK].next_free = ctx->mms[MM_SEG_STACK].end - PAGE_SIZE;
   pl4 = ctx->pgd; 
   //if(install_os_pts(pl4))
     //   return -1;
   
   if(install_os_pts_range(ctx, REGION_DRAM_START, (REGION_DRAM_END-REGION_DRAM_START)))
        return -1;
   //printk("dram done\n");

   if(install_os_pts_range(ctx, REGION_NVM_START, (REGION_NVM_ENDMEM-REGION_NVM_START)))
        return -1;

   //printk("nvm done\n");

   install_apic_mapping((u64) pl4);
   pl4 = pl4 << PAGE_SHIFT;
   set_tss_stack_ptr(ctx); 
   stats->num_processes++;   
   sptr = STACK_START;
   fmem = textpfn << PAGE_SHIFT;
   memcpy((char *)fmem, (char *)(0x200000), CODE_PAGES << PAGE_SHIFT);   /*This is where INIT is in the os binary*/ 
   clflush_multiline((u64)fmem, CODE_PAGES << PAGE_SHIFT);
   fmem = CODE_START;
   current = ctx;
   current->state = RUNNING;
   printk("Page table setup done, launching init ...\n");

   asm volatile("mov %%rbp, %0;"
           : "=r" (saved_ebp)
           : 
           :
          );
   *(unsigned long*)saved_ebp_backup = saved_ebp; 
   //printk("saved_ebp:%x\n",saved_ebp);
   asm volatile( 
            "cli;"
            "mov %1, %%cr3;"
            "pushq $0x2b;"
            "pushq %0;"
            "pushfq;"
            "popq %%rax;"
            "or $0x200, %%rax;"
            "pushq %%rax;" 
            "pushq $0x23;"
            "pushq %2;"
            "mov $0x2b, %%ax;"
            "mov %%ax, %%ds;"
            "mov %%ax, %%es;"
            "mov %%ax, %%gs;"
            "mov %%ax, %%fs;"
           : 
           : "r" (sptr), "r" (pl4), "r" (fmem) 
           : "memory"
          );

   //printk("after asm ...\n");
   asm volatile( "mov %0, %%rdi;"
            "mov %1, %%rsi;"
            "mov %2, %%rcx;"
            "mov %3, %%rdx;"
            "mov %4, %%r8;"
	    "mfence;"
            "iretq"
            :
            : "m" (args->rdi), "m" (args->rsi), "m" (args->rcx), "m" (args->rdx), "m" (args->r8)
            : "rsi", "rdi", "rcx", "rdx", "r8", "memory"
           );
   
   return 0; 
}
struct exec_context* get_current_ctx(void)
{
   return current;
}
void set_current_ctx(struct exec_context *ctx)
{
   current = ctx;
}

struct exec_context *get_ctx_list()
{
   return ctx_list; 
}
//changed by KP Arun to check the pmd level unmapping
//start point
static void pt_cleanup(u64 pfn,int level)
{
     unsigned long* v_add=(unsigned long*) osmap(pfn); 
     unsigned long* entry_addr;
     unsigned long entry_val;
     unsigned int i;
     if(level<1 && !is_apic_base(pfn)){
         u32 region = get_mem_region(pfn);
	 if( region < 0){
	     printk("get mem region bug at %s\n",__func__);
	 }
         os_pfn_free(region , pfn);
            return;
      }
        
     if(level == 3)
             i = 1; // changed to 5 from 2 to make it 3+1+2 GB
     else
             i = 0;     
    for(;i < 512; i++){
        entry_addr=v_add+i;
	entry_val=*(entry_addr);
	if((entry_val & 0x1) == 0)
            continue;
	else{
	    pt_cleanup(entry_val>>12, level-1);
	    *(entry_addr) = 0x0;
	}
    }
    if(!is_apic_base(pfn)){
	//printk("freeing pfn:%x in file %s\n",pfn,__FILE__);
	u32 region = get_mem_region(pfn);
	 if( region < 0){
	     printk("get mem region bug at %s\n",__func__);
	 }
        os_pfn_free(region, pfn);
    }
}


void do_cleanup()
{
   /*TODO split the exit into two parts. load OS CR3 and saved ebp before cleaning up 
     this guy, There is a chicken-egg here.*/
   extern void ret_from_user();
   struct exec_context *ctx = current; 
   printk("Cleaned up %s process\n", current->name);
   //os_free((void *)ctx, sizeof(struct exec_context));
   printk("GemOS shell again!\n", current->name);
   asm volatile("cli;"
                 :::"memory"); 
   remove_apic_mapping((u64) ctx->pgd); 
   //pt_cleanup(ctx->pgd, 4);
   os_pfn_free(OS_PT_REG, ctx->os_stack_pfn);
   os_pfn_free(OS_PT_REG, os_pmd_pfn);
   current->state = UNUSED;
   current = NULL;
   ret_from_user();
}

static void swapper_task()
{
   stats->swapper_invocations++;
   while(1){
             asm volatile("sti;"
                          "hlt;"
                           :::"memory"
                         );
   }
}


struct exec_context *get_ctx_by_pid(u32 pid)
{
   if(pid >= MAX_PROCESSES){
        printk("%s: invalid pid %d\n", __func__, pid);
        return NULL;
   }
  return (ctx_list + pid);
}

int set_process_state(struct exec_context *ctx, u32 state)
{
   if(state >= MAX_STATE){
        printk("%s: invalid state %d\n", __func__, state);
        return -1;
   }
   ctx->state = state;
   return 0;
}
#if 0
void load_swapper(struct user_regs *regs)
{
   extern void *return_from_os;
   unsigned long retptr = (unsigned long)(&return_from_os);
   struct exec_context *swapper = ctx_list;
   u64 cr3 = swapper->pgd;
   current = swapper;
   current->state = RUNNING;
   set_tss_stack_ptr(swapper);
   memcpy((char *)regs, (char *)(&swapper->regs), sizeof(struct user_regs));
   ack_irq();
   asm volatile("mov %0, %%cr3;"
                "mov %1, %%rsp;"
                "callq *%2;"
                :
                :"r" (cr3), "r" (regs), "r"  (retptr)
                :"memory");
   
}
#endif
void init_swapper()
{
   u64 ss, cs, rflags; 
   struct exec_context *swapper;
   u64 ctx_l = os_pfn_alloc(OS_PT_REG);
   ctx_list = (struct exec_context *) (ctx_l << PAGE_SHIFT);
   bzero((char*)ctx_list, PAGE_SIZE);
   swapper = ctx_list;
   swapper->state = READY;
   swapper->pgd = 0x70;
   memcpy(swapper->name, "swapper", 8);
   swapper->os_stack_pfn = os_pfn_alloc(OS_PT_REG);
   swapper->os_rsp = (((u64) swapper->os_stack_pfn) << PAGE_SHIFT) + PAGE_SIZE;
   bzero((char *)(&swapper->regs), sizeof(struct user_regs));
   swapper->regs.entry_rip = (unsigned long)(&swapper_task);
   swapper->regs.entry_rsp = swapper->os_rsp;
   asm volatile( "mov %%ss, %0;"
                 "mov %%cs, %1;"
                 "pushf;"
                 "pop %%rax;"
                 "mov %%rax, %2;"
                 : "=r" (ss), "=r" (cs), "=r" (rflags)
                 :
                 : "rax", "memory"
   );
   swapper->regs.entry_ss = ss; 
   swapper->regs.entry_rflags = rflags;
   swapper->regs.entry_cs = cs;
   return;
}

/*
   Copies the mm structures along with the page table entries
   Note: CODE and RODATA are shared now!
*/
void copy_mm(struct exec_context *child, struct exec_context *parent)
{
   void *os_addr;
   u64 vaddr; 
   struct mm_segment *seg;

   child->pgd = os_pfn_alloc(OS_PT_REG);

   os_addr = osmap(child->pgd);
   bzero((char *)os_addr, PAGE_SIZE);
   
   //CODE segment
   seg = &parent->mms[MM_SEG_CODE];
   for(vaddr = seg->start; vaddr < seg->next_free; vaddr += PAGE_SIZE){
      u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
      if(parent_pte)
           install_ptable((u64) os_addr, seg, vaddr, (*parent_pte & FLAG_MASK) >> PAGE_SHIFT,0);   
   } 
   //RODATA segment
   
   seg = &parent->mms[MM_SEG_RODATA];
   for(vaddr = seg->start; vaddr < seg->next_free; vaddr += PAGE_SIZE){
      u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
      if(parent_pte)
           install_ptable((u64)os_addr, seg, vaddr, (*parent_pte & FLAG_MASK) >> PAGE_SHIFT,0);   
   } 
   
   //DATA segment
   seg = &parent->mms[MM_SEG_DATA];
   for(vaddr = seg->start; vaddr < seg->next_free; vaddr += PAGE_SIZE){
      u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
      
      if(parent_pte){
           u64 pfn = install_ptable((u64)os_addr, seg, vaddr, 0, 0);  //Returns the blank page in dram  
           pfn = (u64)osmap(pfn);
           memcpy((char *)pfn, (char *)(*parent_pte & FLAG_MASK), PAGE_SIZE); 
      }
  } 
  
  //STACK segment
  seg = &parent->mms[MM_SEG_STACK];
  for(vaddr = seg->end - PAGE_SIZE; vaddr >= seg->next_free; vaddr -= PAGE_SIZE){
      u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
      
     if(parent_pte){
           u64 pfn = install_ptable((u64)os_addr, seg, vaddr, 0, 0);  //Returns the blank page in dram  
           pfn = (u64)osmap(pfn);
           memcpy((char *)pfn, (char *)(*parent_pte & FLAG_MASK), PAGE_SIZE); 
     }
  } 
 
   
  copy_os_pts(parent->pgd, child->pgd); 
  return; 
}

void setup_child_context(struct exec_context *child)
{
   //Allocate pgd and OS stack
   child->os_stack_pfn = os_pfn_alloc(OS_PT_REG);
   child->os_rsp = (((u64) child->os_stack_pfn) << PAGE_SHIFT) + PAGE_SIZE;
   child->state = READY;  // Will be eligible in next tick
   stats->num_processes++;
}

struct vm_area * get_vm_area(struct exec_context *ctx, u64 address){
   struct vm_area * vm_area = ctx->vm_area;
   while(vm_area){
      if(address >= vm_area->vm_start && address < vm_area->vm_end)
          return vm_area;
      vm_area = vm_area->vm_next;
   }
   return NULL;
}

int print_exec_context(struct exec_context *ctx){
    printk("pid:%x\n",ctx->pid);
    printk("ppid:%x\n",ctx->ppid);
    printk("type:%x\n",(unsigned)ctx->type);
    printk("persistent:%x\n",(unsigned)ctx->persistent);
    printk("state:%x\n",(unsigned)ctx->state);
    printk("used_mem:%x\n",(unsigned)ctx->used_mem);
    printk("pgd:%x\n",ctx->pgd);
    printk("os_stack_pfn:%x\n",ctx->os_stack_pfn);
    printk("os_rsp:%x\n",ctx->os_rsp);
    for(int i=0; i<MAX_MM_SEGS; i++){
        printk("%x, start:%x, end:%x, next_free:%x, access_flags:%x\n",i,ctx->mms[i].start, 
			ctx->mms[i].end, ctx->mms[i].next_free, ctx->mms[i].access_flags);
    }
    struct vm_area * vm = ctx->vm_area;
    while(vm){
        printk("start:%x, end:%x, access_flags:%x, is_nvm:%x\n",vm->vm_start,
			vm->vm_end,vm->access_flags,(unsigned)vm->is_nvm);
    vm = vm->vm_next;
    }
    printk("name:%s\n",ctx->name);
    printk("r15:%x, r14:%x, r13:%x, r12:%x, r11:%x, r10:%x, r9:%x, r8:%x\n",ctx->regs.r15,
		    ctx->regs.r14,ctx->regs.r13,ctx->regs.r12,ctx->regs.r11,
		    ctx->regs.r10,ctx->regs.r9,ctx->regs.r8); 
   printk("rbp:%x, rdi:%x, rsi:%x, rdx:%x, rcx:%x, rbx:%x, rax:%x\n",ctx->regs.rbp,
		   ctx->regs.rdi,ctx->regs.rsi,ctx->regs.rdx,ctx->regs.rcx,ctx->regs.rbx,ctx->regs.rax); 
   printk("entry_rip:%x, entry_cs:%x, entry_rflags:%x, entry_rsp:%x, entry_ss:%x\n",
		   ctx->regs.entry_rip,ctx->regs.entry_cs,ctx->regs.entry_rflags,
		   ctx->regs.entry_rsp,ctx->regs.entry_ss); 
    printk("pending_signal_bitmap:%x\n",ctx->pending_signal_bitmap);
    for(int i=0; i<MAX_SIGNALS; i++){
        printk("signal:%x\n",ctx->sighandlers[i]);
    }
    printk("ticks_to_sleep:%x\n",ctx->ticks_to_sleep);
    printk("alarm_config_time:%x\n",ctx->alarm_config_time);
    printk("ticks_to_alarm:%x\n",ctx->ticks_to_alarm);
    for(int i=0; i<MAX_OPEN_FILES; i++){
	if(ctx->files[i])
	    printk("file:%x\n",ctx->files[i]);
    }
    return 0;
}
