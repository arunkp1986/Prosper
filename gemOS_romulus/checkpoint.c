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
#include <mmap.h>

#define NUM_LOG_PAGES 4
#define NUM_OPS 500000

struct romulus_log_entry{
   unsigned long addr;
   unsigned size;
};

struct romulus_mapping_entry{
    unsigned long back_address;
    unsigned size;
};

struct user_regs *saved_regs = NULL;
struct list_head *checkpoint_stat_list = NULL;
u16 track_on = 0;
u32* ptr_dirtymap;
int start_checkpoint(struct exec_context *ctx) {
    //dprintk("start checkpoint\n");
    track_on = 1;
    if(!ptr_dirtymap){
        unsigned total_size = NUM_OPS*sizeof(struct romulus_log_entry);
        unsigned dirty_num_pages = (total_size%PAGE_SIZE)?(total_size>>PAGE_SHIFT)+1:(total_size>>PAGE_SHIFT);
	//printk("dirty num pages:%d\n",dirty_num_pages);
        ptr_dirtymap = (u32*)os_page_alloc_pages(FILE_DS_REG,dirty_num_pages);
        //printk("dirty bitmap area, only once, ptr:%x\n",ptr_dirtymap);
        saved_regs = (struct user_regs*)os_chunk_alloc(sizeof(struct user_regs),NVM_META_REG);
        checkpoint_stat_list = (struct list_head*)os_chunk_alloc(sizeof(struct list_head),NVM_META_REG);
	init_list_head(checkpoint_stat_list);
        //bzero((char *)ptr_dirtymap, dirty_num_pages*PAGE_SIZE);
    }
    //dprintk("ptr:%x\n",ptr_dirtymap);
    //writemsr(MISCREG_TRACK_START, (u64)MMAP_START);
    writemsr(MISCREG_TRACK_START, (u64)(u64)(STACK_START - MAX_STACK_SIZE));
    writemsr(MISCREG_TRACK_END, (u64)STACK_START);
    writemsr(MISCREG_DIRTYMAP_ADDR, (u64)ptr_dirtymap);
    writemsr(MISCREG_LOG_TRACK_GRAN, (u64)1); //setting 1 so that hardware logs addressess
    return 0;
}
/*
struct romulus_mapping_entry* get_back_address(unsigned long address,struct romulus_mapping_entry * me){
    unsigned long main_addr = 0;
    unsigned long back_addr = 0;
    unsigned size = 0;
    struct exec_context *current = get_current_ctx();
    for(int loop=0; loop<MAX_MAPPING; loop++){
        main_addr = current->mapping_table[loop].main_addr;
        back_addr = current->mapping_table[loop].back_addr;
        size = current->mapping_table[loop].size;
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
int end_checkpoint(struct exec_context *ctx) {
    //dprintk("end checkpoint\n");
    writemsr(MISCREG_LOG_TRACK_GRAN, (u64)0);
    checkpoint_count += 1;
    //flushing memory modifications to main copy
    u64 stack_start = ctx->mms[MM_SEG_STACK].start;
    u64 stack_end = ctx->mms[MM_SEG_STACK].end;
    u64 stack_next = ctx->mms[MM_SEG_STACK].next_free;
    extern u64 * backmapping;
    u32 bytes_copy = 0; 
    u64 vaddr;
    u64 main_addr = 0;
    struct romulus_mapping_entry mentry;
    u64 back_addr = 0;
    u64 src_addr;
    u64 dest_addr;
    unsigned start_hi = 0, start_lo = 0, end_hi = 0, end_lo = 0;
    u64 rdtsc_value = 0;
    volatile u32 temp = *(u32*)(ptr_dirtymap+8);
    temp = *(u32*)(ptr_dirtymap+4);
    temp = *(u32*)(ptr_dirtymap+8);
    u64 sp_value = (saved_regs != NULL && (stack_next < saved_regs->entry_rsp
                            && saved_regs->entry_rsp < stack_end)) ? saved_regs->entry_rsp : stack_next;
    u64 temp_address  = sp_value&~(0xfff);
    while(temp_address < stack_end){
        clflush_multiline(temp_address, PAGE_SIZE);
	temp_address += PAGE_SIZE;
    }
    /*temp_address  = data_start;
    while(temp_address < data_end){
        if(validate_page_table(ctx,temp_address,0)){
            clflush_multiline(temp_address, PAGE_SIZE);
	}
	temp_address += PAGE_SIZE;
    }*/
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
    unsigned loop_count = 0;
    struct romulus_log_entry * temp_log = (struct romulus_log_entry*)ptr_dirtymap;
    temp_address = 0;
    unsigned  temp_size = 0;
    unsigned log_entries = readmsr(MISCREG_TRACK_SYNC);
    //printk("log write:%u\n",log_entries);
    unsigned check_counter = 0;
    while(log_entries == 0){
        check_counter += 1;
	if(check_counter==10){
	    //printk("log write not done in :%u\n",check_counter);
            break;
	}
	temp = *(u32*)(ptr_dirtymap+4);
	log_entries = readmsr(MISCREG_TRACK_SYNC);
	//printk("log write:%u\n",log_entries);
    }
    RDTSC_START();
    for(loop_count = 0;loop_count< log_entries; loop_count++){
        clflush_multiline((u64)temp_log, sizeof(struct romulus_log_entry));
	temp_address = temp_log->addr;
	temp_size = temp_log->size;
	temp_log->addr = 0;
	temp_log->size = 0;
	if(temp_address && temp_size && backmapping){
	    u64* pte = get_user_pte(ctx, temp_address,0);
	    //printk("pte:%lx\n",*pte);
	    u32 pfn = (*pte>>12)&((1UL<<32)-1);
	    unsigned long offset = ((((unsigned long) pfn) << PAGE_SHIFT)-NVM_USER_REG_START)>>PAGE_SHIFT;
	    //printk("offset:%u\n",offset);
            struct mapping_entry* entry = (struct mapping_entry*)(((unsigned long)backmapping)+offset*sizeof(struct mapping_entry));
            main_addr =  entry->maddr+(temp_address&(0xfff));
	    back_addr = entry->baddr+(temp_address&(0xfff));
	    //printk("temp_addr:%lx, size:%u, main_addr:%lx, back addr:%lx\n",temp_address,temp_size,main_addr, back_addr);
            int gran_64bytes = temp_size>>6;
	    unsigned remaining = temp_size - (gran_64bytes*64);
	    int gran_8bytes = (remaining%8)?(remaining>>3)+1:remaining>>3;
	    //printk("remaining:%u,gran:%u\n",remaining,gran_8bytes);
            if(gran_64bytes){
	        for(int i=0; i<gran_64bytes; i++){
		    src_addr = main_addr+(i*64);
                    dest_addr = back_addr+(i*64);
                    for(int j=0; j<8; j++){
                        *(u64*)(dest_addr+j*8) = *(u64*)(src_addr+j*8);
		    }
		    bytes_copy += 64;
                    clflush_multiline(dest_addr,64);
                }
	    }
	    if(gran_8bytes){
                for(int j=0; j<gran_8bytes; j++){
                    *(u64*)(back_addr+j*8) = *(u64*)(main_addr+j*8);
		}
		bytes_copy += remaining;
                clflush_multiline(back_addr,remaining);
	    }
	}
	//printk("bytes copied:%u\n",bytes_copy);
        temp_log++;
    }
    RDTSC_STOP();
    rdtsc_value = elapsed(start_hi,start_lo,end_hi,end_lo);
    struct checkpoint_stats* chst = (struct checkpoint_stats*)os_chunk_alloc(
                        sizeof(struct checkpoint_stats),NVM_META_REG);
    chst->num = checkpoint_count;
    chst->time = rdtsc_value;
    chst->bytes_copy = bytes_copy;
    list_add_tail(&chst->list,checkpoint_stat_list);
    return 0;
}

u8 print_checkpoint_stats(){
    struct checkpoint_stats* chst;
    struct list_head* temp;
    struct list_head* head;
    head = checkpoint_stat_list;
    temp = head->next;
    while( temp != head ){
        chst = list_entry(temp, struct checkpoint_stats, list);
        printk("checkpoint:%u,time:%lu,copy_size:%u \n", chst->num, chst->time,chst->bytes_copy);
        temp = temp->next;
    }
    return 0;
}

