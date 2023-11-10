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
u64 * dest_addr_base = NULL;
//struct ssp_entry* ptr_dirtymap;
struct stack_entry * se_base;
int start_checkpoint(struct exec_context *ctx) {
    //printk("start checkpoint\n");
    track_on = 1;
    extern struct ssp_entry* ptr_dirtymap;
    //dprintk("ptr:%x\n",ptr_dirtymap);
    
    writemsr(MISCREG_DIRTYMAP_ADDR, (u64)ptr_dirtymap);
    writemsr(MISCREG_TRACK_SYNC, (u64)0); //setting 0 to clear sync
    writemsr(MISCREG_LOG_TRACK_GRAN, (u64)1); //setting 1 so that hardware logs addressess
    int x = 0;
    while(x<10){
        unsigned dummy_read = *(unsigned*)(ptr_dirtymap);
        x++;
    }
    u64 stack_next = ctx->mms[MM_SEG_STACK].next_free;
    u64 stack_end = ctx->mms[MM_SEG_STACK].end;
    if(!checkpoint_stat_list && !ctx->persistent){
	//printk("once\n");
       // unsigned num_stack_mapping_pages = 300;
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
unsigned checkpoint_count = 0;
unsigned num_evict = 0;
unsigned time_merge = 0;
unsigned bytes_copy = 0;

int end_checkpoint(struct exec_context *ctx) {
    //dprintk("end checkpoint\n");
    extern struct ssp_entry* ptr_dirtymap;
    unsigned start_hi = 0, start_lo = 0, end_hi = 0, end_lo = 0;
    writemsr(MISCREG_LOG_TRACK_GRAN, (u64)0);
    int x = 0;
    while(x<10){
     unsigned dummy_read = *(unsigned*)(ptr_dirtymap);
     x++;
    }
    checkpoint_count += 1;
    unsigned log_index = 0;
    unsigned log_entry_size = sizeof(struct stack_entry)+(1UL<<PAGE_SHIFT);

    //flushing memory modifications to main copy
    
    u64 vaddr;
    u64 rdtsc_value_1 = 0;
    u64 rdtsc_value_2 = 0;
    u64 temp_address  = 0;
    //printk("start:%lx, end:%lx, sp:%lx\n",stack_start,stack_end,temp_address);
    struct vm_area * vm = ctx->vm_area;
    while(vm){
	if(vm->vm_start == MMAP_START){
	    vm = vm->vm_next;
	    continue;
	}
	if(vm->is_nvm){
            for(vaddr = vm->vm_start; vaddr < vm->vm_end; vaddr += PAGE_SIZE){
                if(validate_page_table(ctx,vaddr,0)){
                    clflush_multiline(vaddr, PAGE_SIZE);
		}
	    }
	}
	vm = vm->vm_next;
    }
    //dprintk("log based implementation\n");
    //flushing metdata region and apply updated bitmap value on commit bitmap value
    //consolidate pages based on commit bitmap and set commit bitmap to zero
    //update the commit bitmap and page consolidation
    unsigned log_write_done = readmsr(MISCREG_TRACK_SYNC);
    unsigned check_counter = 0;
    //dprintk("end log write done:%u\n",log_write_done);
    while(!log_write_done){
        check_counter += 1;
        if(check_counter==10){
            dprintk("log write not done in :%u\n",check_counter);
            break;
	}
	unsigned dumm_read = *(unsigned*)(ptr_dirtymap);
        log_write_done = readmsr(MISCREG_TRACK_SYNC);
	//dprintk("end log write done:%u\n",log_write_done);
    }
    writemsr(MISCREG_TRACK_SYNC, (u64)0); //setting 0 to clear sync
    struct list_head* temp_head;
    struct ssp_entry* temp_entry;
    struct ssp_commit_entry* temp_commit_entry;
    struct ssp_bitmap_entry* current_entry;
    struct checkpoint_stats* chst = (struct checkpoint_stats*)os_chunk_alloc(
		    sizeof(struct checkpoint_stats),NVM_META_REG);
    start_hi = 0, start_lo = 0, end_hi = 0, end_lo = 0;
    RDTSC_START();
    list_for_each(temp_head,ssp_bitmap_list){
        current_entry = list_entry(temp_head, struct ssp_bitmap_entry, list);
	temp_entry = current_entry->entry;
        clflush_multiline((u64)temp_entry, sizeof(struct ssp_entry));
	temp_commit_entry = current_entry->commit_entry;
	if(temp_entry->p0 != temp_commit_entry->p0){
	    printk("BUG!!!!!\n");
	}
	if(temp_entry->updated_bitmap > 0){
	    unsigned long latest_changes = temp_commit_entry->commit_bitmap^temp_entry->updated_bitmap;
	    if(latest_changes > 0){
	        temp_commit_entry->commit_bitmap |= latest_changes;
                clflush_multiline((u64)temp_commit_entry, sizeof(struct ssp_commit_entry));
		latest_changes = 0;
	    }
	}
	if(temp_commit_entry->commit_bitmap != temp_entry->current_bitmap){
	    temp_commit_entry->commit_bitmap = temp_entry->current_bitmap;
            printk("BUG!!!! commit bitmap\n");
	}
    }
    RDTSC_STOP();
    rdtsc_value_1 = elapsed(start_hi,start_lo,end_hi,end_lo);
    //stack at page level, dirty bit based
    unsigned long stack_start = ctx->mms[MM_SEG_STACK].start;
    unsigned long stack_end = ctx->mms[MM_SEG_STACK].end;
    u64 stack_next = ctx->mms[MM_SEG_STACK].next_free;
    if(!saved_regs){
        printk("Bug!!%s\n",__func__);
    }
    u64 sp_value = (saved_regs != NULL && (stack_next < saved_regs->entry_rsp 
			    && saved_regs->entry_rsp < stack_end)) ? saved_regs->entry_rsp : stack_next;
    u64* pte;
    temp_address  = (sp_value&~((1<<PAGE_SHIFT)-1));
    //u64 dest_addr = (u64)dest_addr_base+(stack_end-temp_address);
    unsigned page_copy_size = 0;
    struct stack_entry * se;
    start_hi = 0, start_lo = 0, end_hi = 0, end_lo = 0;
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
		page_copy_size += 8;
	    }
	    flush_addr(dest_addr-j*8); //for final cacheline
	    */
	    *pte &= ~(1UL<<6);
            clflush_multiline((u64)pte,sizeof(unsigned long));
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
    rdtsc_value_2 = elapsed(start_hi,start_lo,end_hi,end_lo);
    chst->num = checkpoint_count;
    chst->time_to_ssp = rdtsc_value_1;
    chst->time_to_db = rdtsc_value_2;
    chst->time_merge = time_merge;
    chst->bytes_copy_ssp = bytes_copy; 
    chst->bytes_copy_db = page_copy_size;
    chst->num_evict = num_evict; 
    list_add_tail(&chst->list,checkpoint_stat_list);
    //num_evict = 0;
    bytes_copy = 0;
    time_merge = 0;
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
        printk("checkpoint:%u,ssp_time:%lu,time_merge:%lu,page_time:%lu,size_ssp:%u,size_db:%u\n", chst->num, chst->time_to_ssp,chst->time_merge, chst->time_to_db, chst->bytes_copy_ssp, chst->bytes_copy_db);
        temp = temp->next;
    }
    //printk("num merges:%u\n",num_merges);
    return 0;
}


int  merge_pages(){
    //dprintk("merge called\n");
    unsigned start_hi = 0, start_lo = 0, end_hi = 0, end_lo = 0;
    u64 rdtsc_value = 0;
    struct list_head* temp_head;
    struct ssp_entry* temp_entry;
    struct ssp_commit_entry* temp_commit_entry;
    struct ssp_bitmap_entry* current_entry;
    //num_merges += 1;
    RDTSC_START();
    //printk("num merges:%u\n",num_merges);
    //asm volatile("mfence":::"memory");
    list_for_each(temp_head,ssp_bitmap_list){
        current_entry = list_entry(temp_head, struct ssp_bitmap_entry, list);
        temp_entry = current_entry->entry;
        clflush_multiline((u64)temp_entry, sizeof(struct ssp_entry));
        temp_commit_entry = current_entry->commit_entry;
        u64 src_addr = 0;
        u64 dest_addr = 0;
        if(temp_entry->evicted == 1){
            //printk("evicted\n");
            num_evict += 1;
            for(int i=0; i<64; i++){
            // copying modified cache lines from p1 to p0
                if(temp_commit_entry->commit_bitmap & (1UL<<i)){
                    src_addr = temp_commit_entry->p1+(i*64);
                    dest_addr = temp_commit_entry->p0+(i*64);
                    //copy at 8 byte granularity
                    for(int j=0; j<8; j++){
                        *(u64*)(dest_addr+j*8) = *(u64*)(src_addr+j*8);
                    }
                    bytes_copy += 64;
                    clflush_multiline(dest_addr,64);
                }
            }
            //temp_commit_entry->commit_bitmap = 0; // now commited values are in page 0
            //clflush_multiline((u64)temp_commit_entry, sizeof(struct ssp_commit_entry));
            temp_entry->evicted = 2;
            //temp_entry->updated_bitmap = 0;
            //temp_entry->current_bitmap = 0;
            clflush_multiline((u64)temp_entry, sizeof(struct ssp_entry));
        }
    }
    RDTSC_STOP();
    rdtsc_value = elapsed(start_hi,start_lo,end_hi,end_lo);
    time_merge += rdtsc_value;
    return 0;
}
