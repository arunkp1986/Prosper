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



//u16 track_on = 0;
//struct ssp_entry* ptr_dirtymap;
int ssp_start_checkpoint(){
    dprintk("ssp start\n");
    extern struct ssp_entry* ptr_sspmap;
    dprintk("ptr:%x\n",ptr_sspmap);
    writemsr(MISCREG_SSP_ADDR, (u64)ptr_sspmap);
    struct ssp_entry* temp_entry;
    struct ssp_commit_entry* temp_commit_entry;
    struct list_head* temp_head;
    struct ssp_bitmap_entry* current_entry;
    list_for_each(temp_head,ssp_bitmap_list){
        current_entry = list_entry(temp_head, struct ssp_bitmap_entry, list);
	temp_entry = current_entry->entry;
	temp_commit_entry = current_entry->commit_entry;
	if(temp_entry->p0 != temp_commit_entry->p0){
	    printk("BUG!!!!! entry p0:%lx, %lx, commit entry: %lx, %lx\n",
				temp_entry->p0,(unsigned long)temp_entry,temp_commit_entry->p0,
				(unsigned long)temp_commit_entry);
	}
	temp_entry->current_bitmap = temp_commit_entry->commit_bitmap;
    }
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
u64 ssp_end_checkpoint(struct exec_context *ctx) {
    dprintk("ssp end checkpoint\n");
    unsigned start_hi = 0, start_lo = 0, end_hi = 0, end_lo = 0;
    //writemsr(MISCREG_LOG_TRACK_GRAN, (u64)0);
    //checkpoint_count += 1;
    extern struct ssp_entry* ptr_sspmap;
    u64 updated_bitmap;
    u64 rdtsc_value_1 = 0;
    //flushing memory modifications to main copy
    //unsigned long stack_start = ctx->mms[MM_SEG_STACK].start;
    //unsigned long stack_end = ctx->mms[MM_SEG_STACK].end;
    //unsigned long data_start = ctx->mms[MM_SEG_DATA].start;
    //unsigned long data_end = ctx->mms[MM_SEG_DATA].end;
    //u64 sp_value = (ctx->regs).entry_rsp;
    u64 vaddr;
    //unsigned long start_addr = 0;
    //struct ssp_mapping_entry mentry;
    //u64 dest = 0;
    //u64 temp_address  = (sp_value&~((1<<PAGE_SHIFT)-1));
    //flush the data from stack and heap area
    /*while(temp_address < stack_end){
        clflush_multiline(temp_address, PAGE_SIZE);
	temp_address += PAGE_SIZE;
    }*/
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
        for(vaddr = vm->vm_start; vaddr < vm->vm_end; vaddr += PAGE_SIZE){
            if(validate_page_table(ctx,vaddr,0)){
                clflush_multiline(vaddr, PAGE_SIZE);
	    }
	}
	vm = vm->vm_next;
    }
    dprintk("log based implementation\n");
    unsigned entries = get_pages_region(NVM_USER_REG);
    unsigned loop_count = 0;
    u64 bitmap_value = 0;
    u64 bitmap_address = 0;
    //flushing metdata region and apply updated bitmap value on commit bitmap value
    //consolidate pages based on commit bitmap and set commit bitmap to zero
    //update the commit bitmap and page consolidation
    /*unsigned log_write_done = readmsr(MISCREG_TRACK_SYNC);
        unsigned check_counter = 0;
	dprintk("end log write done:%u\n",log_write_done);
        while(!log_write_done){
            check_counter += 1;
            if(check_counter==10){
                printk("log write not done in :%u\n",check_counter);
                break;
            }
	    unsigned dumm_read = *(unsigned*)(STACK_START-64);
            log_write_done = readmsr(MISCREG_TRACK_SYNC);
	    dprintk("end log write done:%u\n",log_write_done);
        }
        writemsr(MISCREG_TRACK_SYNC, (u64)0); //setting 0 to clear sync
	*/
    struct list_head* temp_head;
    struct ssp_entry* temp_entry;
    struct ssp_commit_entry* temp_commit_entry;
    struct ssp_bitmap_entry* current_entry;
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
	//page consolidation if TLB is evicted
	u64 src_addr = 0;
	u64 dest_addr = 0;
	if(temp_entry->evicted){
	    for(int i=0; i<64; i++){
	        // copying modified cache lines from p1 to p0
	        if(temp_commit_entry->commit_bitmap & (1UL<<i)){
	            src_addr = temp_commit_entry->p1+(i*64);
                    dest_addr = temp_commit_entry->p0+(i*64);
		    //copy at 8 byte granularity
		    for(int j=0; j<8; j++){
                        *(u64*)(dest_addr+j*8) = *(u64*)(src_addr+j*8);
		    }
                    clflush_multiline(dest_addr,64);
		}
	    }
	    temp_commit_entry->commit_bitmap = 0; // now commited values are in page 0
            clflush_multiline((u64)temp_commit_entry, sizeof(struct ssp_commit_entry));
	    temp_entry->evicted = 0;
	    temp_entry->updated_bitmap = 0;
	    temp_entry->current_bitmap = 0;
            clflush_multiline((u64)temp_entry, sizeof(struct ssp_entry));
	}
    }
    RDTSC_STOP();
    rdtsc_value_1 = elapsed(start_hi,start_lo,end_hi,end_lo);
    return rdtsc_value_1;
}
/*
    else{
        printk("SSP only implements sub page based, not page implementation\n");
	
	u64* pte;
	unsigned copy_size = 0;
	temp_address  = (sp_value&~((1<<PAGE_SHIFT)-1));
        start_addr =  find_start_address(temp_address, ctx);
        if(!start_addr){
            printk("BUG!!! vma not present\n");
	}
	if(!get_back_address(start_addr,&mentry)){
	    printk("BUG!!!! stack mapping table entry missing\n");
	}
        copy_size = mentry.size;
	while(temp_address<stack_end){
	    //printk("temp address:%lx, back address:%lx size:%lx\n",temp_address,mentry.back_address, mentry.size);
            pte = get_user_pte(ctx, temp_address, 0);
	    if(pte && (*pte &(1UL<<6))){
	         dest = mentry.back_address+(stack_end-temp_address);
		 unsigned copy = (copy_size >= PAGE_SIZE)?PAGE_SIZE:copy_size;
		 copy_size -= copy;
		 //printk("destination:%lx\n",dest);
		 int gran_8bytes = (copy%8)?(copy>>3)+1:copy>>3;
                 u64 cache_addr = 0;
                 for(int j=0; j<gran_8bytes; j++){
                     *(u64*)(dest-j*8) = *(u64*)(temp_address+j*8);
                     if(cache_addr != ((dest-j*8)&~((1<<6)-1))){
                         cache_addr = ((dest-j*8)&~((1<<6)-1));
                         clflush_multiline(cache_addr,sizeof(u64));
		     }
		 }
		 //memcpy((char*)dest,(char*)temp_address,copy);
		 *pte &= ~(1UL<<6);
                 clflush_multiline((u64)pte,sizeof(unsigned long));
                 asm volatile ("invlpg (%0);"
                   :: "r"(temp_address)
                   : "memory");   // Flush TLB
	    }
	    temp_address += PAGE_SIZE;
	}
        temp_address  = (data_start&~((1<<PAGE_SHIFT)-1));
        start_addr =  find_start_address(temp_address, ctx);
        if(!start_addr){
            printk("BUG!!! vma not present\n");
	}
	if(!get_back_address(start_addr,&mentry)){
	    printk("BUG!!!! stack mapping table entry missing\n");
	}
        copy_size = mentry.size;
	while(temp_address<data_end){
	    //printk("temp address:%lx, back address:%lx size:%lx\n",temp_address,mentry.back_address, mentry.size);
            pte = get_user_pte(ctx, temp_address, 0);
	    if(pte && (*pte &(1UL<<6))){
	         dest = mentry.back_address+(temp_address-data_start);
		 unsigned copy = (copy_size>=PAGE_SIZE)?PAGE_SIZE:copy_size;
		 copy_size -= copy;
		 //printk("destination:%lx\n",dest);
		 int gran_8bytes = (copy%8)?(copy>>3)+1:copy>>3;
                 u64 cache_addr = 0;
                 for(int j=0; j<gran_8bytes; j++){
                     *(u64*)(dest+j*8) = *(u64*)(temp_address+j*8);
                     if(cache_addr != ((dest+j*8)&~((1<<6)-1))){
                         cache_addr = ((dest+j*8)&~((1<<6)-1));
                         clflush_multiline(cache_addr,sizeof(u64));
                    }
                }
		 //memcpy((char*)dest,(char*)temp_address,copy);
		 *pte &= ~(1UL<<6);
                 clflush_multiline((u64)pte,sizeof(unsigned long));
                 asm volatile ("invlpg (%0);"
                   :: "r"(temp_address)
                   : "memory");   // Flush TLB
	    }
	    temp_address += PAGE_SIZE;
	}
        vm = ctx->vm_area;
        while(vm){
	    if(vm->vm_start == MMAP_START){
	        vm = vm->vm_next;
	        continue;
	    }
            if(!get_back_address(vm->vm_start,&mentry)){
		    printk("BUG!!!! mapping entry not present, address:%lx\n",vm->vm_start);
		}
            copy_size = mentry.size;
	    //printk("temp address:%lx, back address:%lx size:%lx\n",vm->vm_start,mentry.back_address, mentry.size);
	    for(vaddr = vm->vm_start; vaddr < vm->vm_end; vaddr += PAGE_SIZE){
	        pte = get_user_pte(ctx, vaddr, 0);
	        if(pte && (*pte &(1UL<<6))){
	            dest = mentry.back_address+(vaddr-vm->vm_start);
		    //printk("destination:%lx\n",dest);
		    unsigned copy = (copy_size>=PAGE_SIZE)?PAGE_SIZE:copy_size;
		    copy_size -= copy;
		    int gran_8bytes = (copy%8)?(copy>>3)+1:copy>>3;
                    u64 cache_addr = 0;
                    for(int j=0; j<gran_8bytes; j++){
                        *(u64*)(dest+j*8) = *(u64*)(vaddr+j*8);
                        if(cache_addr != ((dest+j*8)&~((1<<6)-1))){
                            cache_addr = ((dest+j*8)&~((1<<6)-1));
                            clflush_multiline(cache_addr,sizeof(u64));
			}
		    }
		    //memcpy((char*)dest,(char*)vaddr,copy);
		    *pte &= ~(1UL<<6);
                    clflush_multiline((u64)pte,sizeof(unsigned long));
                    asm volatile ("invlpg (%0);"
                      :: "r"(vaddr)
                      : "memory");   // Flush TLB
		}
	    }
	    vm = vm->vm_next;
	}
    }
    return 0;
}

*/
