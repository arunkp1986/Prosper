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

struct user_regs *saved_regs = NULL;
u16 track_on = 0;
int start_checkpoint(struct exec_context *ctx) {
    //printk("start checkpoint\n");
    track_on = 1;
    extern struct ssp_entry* ptr_dirtymap;
    //dprintk("ptr:%x\n",ptr_dirtymap);
    if(!saved_regs)
        saved_regs = (struct user_regs*)os_chunk_alloc(sizeof(struct user_regs),NVM_META_REG);

    writemsr(MISCREG_DIRTYMAP_ADDR, (u64)ptr_dirtymap);
    writemsr(MISCREG_TRACK_SYNC, (u64)0); //setting 0 to clear sync
    writemsr(MISCREG_LOG_TRACK_GRAN, (u64)1); //setting 1 so that hardware logs addressess
    int x = 0;
    while(x<10){
        unsigned dummy_read = *(unsigned*)(ptr_dirtymap);
        x++;
    }
	/*struct ssp_entry* temp_entry;
	struct ssp_commit_entry* temp_commit_entry;
	struct list_head* temp_head;
	struct ssp_bitmap_entry* current_entry;
	//get commit bitmap to current bitmap
	//unsigned log_write_done = readmsr(MISCREG_TRACK_SYNC);
	//dprintk(" start log write done:%u\n",log_write_done);
	list_for_each(temp_head,ssp_bitmap_list){
            current_entry = list_entry(temp_head, struct ssp_bitmap_entry, list);
	    temp_entry = current_entry->entry;
	    temp_commit_entry = current_entry->commit_entry;
	    if(temp_entry->p0 != temp_commit_entry->p0){
	        printk("BUG!!!!! entry p0:%lx, %lx, commit entry: %lx, %lx\n",
				temp_entry->p0,(unsigned long)temp_entry,temp_commit_entry->p0,
				(unsigned long)temp_commit_entry);
	    }
	    dprintk("updated:%lx, commit:%lx\n",temp_entry->updated_bitmap,temp_commit_entry->commit_bitmap);
	    //temp_entry->current_bitmap = temp_commit_entry->commit_bitmap;
	    if((temp_entry->evicted == 3)){
	        printk("here\n");
	    }
	}*/
      return 0;
}
/*
struct ssp_mapping_entry{
    unsigned long back_address;
    unsigned size;
};

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
unsigned long find_start_address(unsigned long addr,struct exec_context *ctx){
    unsigned long stack_start = ctx->mms[MM_SEG_STACK].start;
    unsigned long stack_end = ctx->mms[MM_SEG_STACK].end;
    unsigned long data_start = ctx->mms[MM_SEG_DATA].start;
    unsigned long data_end = ctx->mms[MM_SEG_DATA].end;
    if(stack_start <= addr && addr< stack_end){
	//printk("mapping entry for stack area\n");
        return stack_start;
    }
    else if(data_start <= addr && addr< data_end){
	//printk("mapping entry for data area\n");
        return data_start;
    }

    struct vm_area * vm = ctx->vm_area;
    while(vm){
	if(vm->vm_start <= addr && addr < vm->vm_end){
	    return vm->vm_start;
	}
        vm = vm->vm_next;
    }
    return 0;
}
*/
unsigned checkpoint_count = 0;
unsigned num_evict = 0;
u64 time_merge = 0;
unsigned bytes_copy = 0;

int end_checkpoint(struct exec_context *ctx) {
    //dprintk("end checkpoint\n");
    writemsr(MISCREG_LOG_TRACK_GRAN, (u64)0);
    checkpoint_count += 1;
    //printk("checkpoint:%u\n",checkpoint_count);
    unsigned start_hi = 0, start_lo = 0, end_hi = 0, end_lo = 0;
    u64 rdtsc_value = 0;
    extern struct ssp_entry* ptr_dirtymap;
    extern struct list_head *checkpoint_stat_list;
    int x = 0;
    while(x<10){
        unsigned dummy_read = *(unsigned*)(ptr_dirtymap);
        x++;
    }
    asm volatile("mfence":::"memory");
    unsigned long stack_start = ctx->mms[MM_SEG_STACK].start;
    unsigned long stack_end = ctx->mms[MM_SEG_STACK].end;
    u64 stack_next = ctx->mms[MM_SEG_STACK].next_free;
    unsigned long data_start = ctx->mms[MM_SEG_DATA].start;
    unsigned long data_end = ctx->mms[MM_SEG_DATA].end;
    u64 sp_value = (saved_regs != NULL && (stack_next < saved_regs->entry_rsp 
			    && saved_regs->entry_rsp < stack_end)) ? saved_regs->entry_rsp : stack_next;
    u64 vaddr;
    unsigned long start_addr = 0;
    u64 dest = 0;
    u64 temp_address  = (sp_value&~((1<<PAGE_SHIFT)-1));
    while(temp_address < stack_end){
        clflush_multiline(temp_address, PAGE_SIZE);
	temp_address += PAGE_SIZE;
    }
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
    //printk("after vm flush, count %u,%u\n",count_vma,vmas);
    
    dprintk("log based implementation\n");
    unsigned entries = get_pages_region(NVM_USER_REG);
    unsigned loop_count = 0;
    u64 bitmap_value = 0;
    u64 bitmap_address = 0;
    //flushing metdata region and apply updated bitmap value on commit bitmap value
    //consolidate pages based on commit bitmap and set commit bitmap to zero
    //update the commit bitmap and page consolidation
    unsigned log_write_done = readmsr(MISCREG_TRACK_SYNC);
    unsigned check_counter = 0;
    //dprintk("end log write done:%u\n",log_write_done);
    while(!log_write_done){
        check_counter += 1;
        if(check_counter==10){
            printk("log write not done in :%u\n",check_counter);
            break;
	}
	unsigned dumm_read = *(unsigned*)(ptr_dirtymap);
        log_write_done = readmsr(MISCREG_TRACK_SYNC);
	dprintk("end log write done:%u\n",log_write_done);
    }
    writemsr(MISCREG_TRACK_SYNC, (u64)0); //setting 0 to clear sync
    struct list_head* temp_head;
    struct ssp_entry* temp_entry;
    struct ssp_commit_entry* temp_commit_entry;
    struct ssp_bitmap_entry* current_entry;
    RDTSC_START();
    //dprintk("before loop\n");
    list_for_each(temp_head,ssp_bitmap_list){
        current_entry = list_entry(temp_head, struct ssp_bitmap_entry, list);
        temp_entry = current_entry->entry;
        clflush_multiline((u64)temp_entry, sizeof(struct ssp_entry));
	temp_commit_entry = current_entry->commit_entry;
	if(temp_entry->p0 != temp_commit_entry->p0){
	        printk("BUG!!!!!\n");
	}
        if((temp_entry->updated_bitmap > 0)){
	unsigned long latest_changes = temp_commit_entry->commit_bitmap^temp_entry->updated_bitmap;
        if(latest_changes > 0){
            temp_commit_entry->commit_bitmap |= latest_changes;
            clflush_multiline((u64)temp_commit_entry, sizeof(struct ssp_commit_entry));
            latest_changes = 0;
	}
	    /*temp_commit_entry->commit_bitmap ^= temp_entry->updated_bitmap;
             clflush_multiline((u64)temp_commit_entry, sizeof(struct ssp_commit_entry));*/
	}
	//printk("commit_bitmap:%lx,current_bitmap:%lx, update_bitmap:%lx\n",temp_commit_entry->commit_bitmap, temp_entry->current_bitmap,temp_entry->updated_bitmap);
	if(temp_commit_entry->commit_bitmap != temp_entry->current_bitmap){
	    temp_commit_entry->commit_bitmap = temp_entry->current_bitmap;
	    printk("BUG!!!! commit bitmap\n");
	}
	//page consolidation if TLB is evicted
	/*u64 src_addr = 0;
	    u64 dest_addr = 0;
	    if(temp_entry->evicted == 1){
		dprintk("evicted\n");
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
		temp_commit_entry->commit_bitmap = 0; // now commited values are in page 0
                clflush_multiline((u64)temp_commit_entry, sizeof(struct ssp_commit_entry));
		temp_entry->evicted = 0;
		temp_entry->updated_bitmap = 0;
		temp_entry->current_bitmap = 0;
                clflush_multiline((u64)temp_entry, sizeof(struct ssp_entry));
	    }*/
	}
    //dprintk("after loop\n");
    RDTSC_STOP();
    rdtsc_value = elapsed(start_hi,start_lo,end_hi,end_lo);
    struct checkpoint_stats* chst = (struct checkpoint_stats*)os_chunk_alloc(
			sizeof(struct checkpoint_stats),NVM_META_REG);
    chst->num = checkpoint_count;
    chst->time_to_log = rdtsc_value;
    chst->num_evict = num_evict;
    chst->time_merge = time_merge;
    chst->bytes_copy = bytes_copy;
    list_add_tail(&chst->list,checkpoint_stat_list);
    //num_evict = 0;
    bytes_copy = 0;
    time_merge = 0;
    return 0;
}
//unsigned num_merges = 0;
u8 print_checkpoint_stats(){
    struct checkpoint_stats* chst;
    struct list_head* temp;
    struct list_head* head;
    head = checkpoint_stat_list;
    temp = head->next;
    //dprintk("head:%x\n",head);
    while( temp != head ){
        chst = list_entry(temp, struct checkpoint_stats, list);
        printk("checkpoint:%u, log_time:%lu, time_merge:%lu, copy_size:%u \n", chst->num, chst->time_to_log, chst->time_merge, chst->bytes_copy);
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
	    //dprintk("evicted\n");
            //num_evict += 1;
	    for(int i=0; i<64; i++){
              //printk("commit_bitmap:%lx\n",temp_commit_entry->commit_bitmap);
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
            clflush_multiline((u64)temp_commit_entry, sizeof(struct ssp_commit_entry));
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
