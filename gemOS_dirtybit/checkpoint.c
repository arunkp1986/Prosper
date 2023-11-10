#include <context.h>
#include <list.h>
#include <nonvolatile.h>
#include <memory.h>
#include <lib.h>
#include <dirty.h>
#include <entry.h>
#include <apic.h>
#include <schedule.h>
#include <msr.h>

#define NUM_LOG_PAGES 4


u16 track_on = 0;
struct list_head *checkpoint_stat_list = NULL;
struct user_regs *saved_regs = NULL;
//u64 * dest_addr = NULL;
//u64 * dest_addr_base = NULL;
struct stack_entry * se_base;
int start_checkpoint(struct exec_context *ctx) {
    //dprintk("start checkpoint\n");
    track_on = 1;
    /*if(!checkpoint_stat_list){
        checkpoint_stat_list = (struct list_head*)os_chunk_alloc(sizeof(struct list_head),NVM_META_REG);
        init_list_head(checkpoint_stat_list);
    }*/

    u64 stack_next = ctx->mms[MM_SEG_STACK].next_free;
    u64 stack_end = ctx->mms[MM_SEG_STACK].end;
    if(!checkpoint_stat_list && !ctx->persistent){
	//printk("once\n");
        //unsigned num_stack_mapping_pages = 300;
       //unsigned numbits = STACK_EXP_SIZE >> PAGE_SHIFT;
       unsigned logentries = STACK_EXP_SIZE >> PAGE_SHIFT;
       unsigned logsize = logentries*(sizeof(struct stack_entry)+(1UL<<PAGE_SHIFT));// log struct+ PAGE to hold log
       unsigned log_pages = logsize%PAGE_SIZE?(logsize>>PAGE_SHIFT)+1:(logsize>>PAGE_SHIFT);       
       se_base = (struct stack_entry*)os_page_alloc_pages(NVM_META_REG, log_pages); //this is per context

        u64 addr = stack_next;
	unsigned initial_size = (stack_end-stack_next);
        unsigned nvm_stack_pages = STACK_EXP_SIZE%PAGE_SIZE?(STACK_EXP_SIZE>>PAGE_SHIFT)+1:(STACK_EXP_SIZE>>PAGE_SHIFT);
	ctx->nvm_stack = (u64)os_page_alloc_pages(NVM_META_REG, nvm_stack_pages);
        u64 dest_addr = ctx->nvm_stack+initial_size;
	u64 cache_addr = 0;
        while( addr < stack_end ){
            for(int j=0; j<512; j++){
                dest_addr -= 8;
               *(u64*)(dest_addr) = *(u64*)(addr+j*8);
	       if(j && j%7 == 0)
                   flush_addr(dest_addr);
	    }
	    asm volatile("sfence":::"memory");
	    //dest_addr -= PAGE_SIZE;
            //clflush_multiline(dest_addr,PAGE_SIZE);
            u64* pte = get_user_pte(ctx, addr, 0);
            if(pte && (*pte&(1UL<<6))){
            //printk("address:%x pte entry before:%x at %s\n",addr, *pte, __func__);
            *pte &= ~(1UL<<6);
            clflush_multiline((u64)pte,sizeof(unsigned long));
            //printk("address:%x pte entry after:%x at %s\n",addr, *pte, __func__); 
            asm volatile ("invlpg (%0);"
                   :: "r"(addr)
                   : "memory");   // Flush TLB
	    }
	    addr += PAGE_SIZE;
	}
        checkpoint_stat_list = (struct list_head*)os_chunk_alloc(sizeof(struct list_head),NVM_META_REG);
        init_list_head(checkpoint_stat_list);
	saved_regs = (struct user_regs*)os_chunk_alloc(sizeof(struct user_regs),NVM_META_REG);
        ctx->persistent |= MAKE_PERSISTENT;
    }
    else{
	if(!saved_regs){
	    printk("Bug!!%s\n",__func__);
	}
        u64 sp_value = (saved_regs != NULL && (stack_next < saved_regs->entry_rsp 
				&& saved_regs->entry_rsp < stack_end)) ? saved_regs->entry_rsp : stack_next;
	u64 sp_address  = (sp_value&~((1<<PAGE_SHIFT)-1));
        u64 addr = stack_next;
        while(addr < sp_address){
            u64* pte = get_user_pte(ctx, addr, 0);
            if(pte && (*pte&(1UL<<6))){
            //printk("address:%x pte entry before:%x at %s\n",addr, *pte, __func__);
            *pte &= ~(1UL<<6);
            clflush_multiline((u64)pte,sizeof(unsigned long));
            //printk("address:%x pte entry after:%x at %s\n",addr, *pte, __func__); 
            asm volatile ("invlpg (%0);"
                   :: "r"(addr)
                   : "memory");   // Flush TLB
	    }
            addr += PAGE_SIZE;
	}
    }

    return 0;
}

/*
struct ssp_mapping_entry* get_back_address(unsigned long address,struct ssp_mapping_entry * me){
    unsigned long main_addr = 0;
    unsigned long back_addr = 0;
    unsigned size = 0;
    struct exec_context *current = get_current_ctx();
    for(int loop_count=0; loop_count<MAX_MAPPING; loop_count++){
        main_addr = current->mapping_table[loop_count].main_addr;
        back_addr = current->mapping_table[loop_count].back_addr;
        size = current->mapping_table[loop_count].size;
	if(main_addr == address){
            //printk("address:%lx, back_addr:%lx, size:%u\n",main_addr, back_addr, size);
	    me->back_address = back_addr;
	    me->size = size;
            return me;
	}
    }
    return NULL;
}
*/
unsigned checkpoint_count = 0;
unsigned num_evict = 0;
unsigned bytes_copy = 0;

int end_checkpoint(struct exec_context *ctx) {
    //dprintk("end checkpoint\n");
    checkpoint_count += 1;
    unsigned log_index = 0;
    unsigned log_entry_size = sizeof(struct stack_entry)+(1UL<<PAGE_SHIFT);

    unsigned start_hi = 0, start_lo = 0, end_hi = 0, end_lo = 0;
    //printk("chepoint num:%u\n",checkpoint_count);
    //u64 updated_bitmap;
    //flushing memory modifications to main copy
    unsigned long stack_start = ctx->mms[MM_SEG_STACK].start;
    unsigned long stack_end = ctx->mms[MM_SEG_STACK].end;
    u64 stack_next = ctx->mms[MM_SEG_STACK].next_free;
    if(!saved_regs){
        printk("Bug!!%s\n",__func__);
    }
    u64 sp_value = (saved_regs != NULL && (stack_next < saved_regs->entry_rsp 
			    && saved_regs->entry_rsp < stack_end)) ? saved_regs->entry_rsp : stack_next;
    //printk("saved_rsp:%lx, ctx_rsp:%lx\n",saved_regs->entry_rsp, (ctx->regs).entry_rsp);
    //printk("current size:%u\n",(sp_value - stack_start));
    u64 vaddr;
    unsigned long start_addr = 0;
    u64 rdtsc_value_1 = 0;
    u64 rdtsc_value_2 = 0;
    u64 temp_address  = 0;
    u64* pte;
    temp_address  = (sp_value&~((1<<PAGE_SHIFT)-1));
    //u32 actual_size = (stack_end-sp_value);
    //u64 dest_addr = (u64)dest_addr_base+(stack_end-temp_address);
    unsigned page_copy_size = 0;
    struct stack_entry * se;
    RDTSC_START();
    while(temp_address< stack_end){
        pte = get_user_pte(ctx, temp_address, 0);
	if(pte && (*pte &(1UL<<6))){
            //dprintk("stack pte dirty set\n");
            se = (struct stack_entry*)((u64)se_base+(log_entry_size*log_index)); //base+log_index*log_size
            se->addr = temp_address;
	    memcpy((char*)se->payload.byte,(char*)temp_address,PAGE_SIZE);
	    clflush_multiline_new((u64)se,log_entry_size);
	    log_index += 1;
	    page_copy_size += PAGE_SIZE;
	    /*int j = 0;
            for(j=0; j<512; j++){
                *(u64*)((u64)dest_addr-(j+1)*8) = *(u64*)(temp_address+j*8);
		if(j && j%8 == 0)
		    flush_addr(dest_addr-j*8);
	    }
	    flush_addr(dest_addr-j*8); //for final cacheline
	    */
	    *pte &= ~(1UL<<6);
            clflush_multiline((u64)pte,sizeof(unsigned long)); // it has sfence
            asm volatile ("invlpg (%0);"
                   :: "r"(temp_address)
                   : "memory");   // Flush TLB
	}
        temp_address += PAGE_SIZE;
	//dest_addr -= PAGE_SIZE;
    }
    //printk("total_logs:%u\n",log_index);
    unsigned total_log = log_index;
    for(log_index=0; log_index<total_log; log_index++){
        se = (struct stack_entry*)((u64)se_base+(log_entry_size*log_index)); //base+log_index*log_size
        unsigned offset = stack_end-se->addr;
	u64 dest_nvm_addr = ctx->nvm_stack+offset;
	u64 src_nvm_addr = (u64)se->payload.byte;
	memcpy((char*)dest_nvm_addr,(char*)src_nvm_addr,PAGE_SIZE);
        clflush_multiline_new((u64)dest_nvm_addr,PAGE_SIZE);
    }
    asm volatile("sfence":::"memory");
    RDTSC_STOP();
    rdtsc_value_1 = elapsed(start_hi,start_lo,end_hi,end_lo);
    //u64 rdtsc_value_1 = 0;
    //unsigned page_copy_size = 0;
    //u32 actual_size = 0;
    struct checkpoint_stats* chst = (struct checkpoint_stats*)os_chunk_alloc(
                        sizeof(struct checkpoint_stats),NVM_META_REG);
    chst->num = checkpoint_count;
    chst->time_to_db = rdtsc_value_1;
    chst->bytes_copy_db = page_copy_size;
    list_add_tail(&chst->list,checkpoint_stat_list);
    return 0;
}

unsigned num_merges = 0;
u8 print_checkpoint_stats(){
    struct checkpoint_stats* chst;
    struct list_head* temp;
    struct list_head* head;
    head = checkpoint_stat_list;
    temp = head->next;
    //dprintk("head:%x\n",head);
    while( temp != head ){
        chst = list_entry(temp, struct checkpoint_stats, list);
        printk("checkpoint:%u, page_time:%lu, size_db:%u\n", chst->num, chst->time_to_db, chst->bytes_copy_db);
        temp = temp->next;
    }
    /*struct exec_context *current = get_current_ctx();
    struct vm_area * vm = current->vm_area;
    while(vm){
	printk("start:%lx, nvm:%d\n",vm->vm_start,(u32)vm->is_nvm);
        vm = vm->vm_next;
    }*/
    return 0;
}

int  merge_pages(){
    printk("merge called\n");
    return 0;
}
