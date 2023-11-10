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

extern struct list_head * saved_state_list;

/*This will start the checkpoint
 * if TRACK_BYTE_GRAN is set then byte based dirty tracking,
 * else page based dirty tracking scheme is applied for checkpoint
 * if Byte based then set tracking granularity and call start_track()
 * If TRACK_ON is set then interval value is time between checkpoints 
 * if FIRST_TIME is not set, then it is initial full checkpoint
 * if not FIRST, then clear the logs from previous checkpoint interval.
 * also set MAKE_PERSISTENT ensure that logging of ctx and vma changes.
 * */
u16 track_on = 0;
struct list_head *checkpoint_stat_list = NULL;
struct list_head *se_list;
struct user_regs *saved_regs = NULL;
u64 * dest_addr_base = NULL;
u32* ptr_dirtymap = NULL;
struct stack_entry * se_base;
//unsigned extra_log_entry = 0;
struct bitmap_data{
    u8 pos;
    u8 stream;
    struct bitmap_data* next;
};
struct bitmap_data * btd = NULL;
int start_checkpoint(struct exec_context *ctx) {
    //dprintk("start checkpoint\n");
    track_on = 1;
    extern struct ssp_entry* ptr_sspmap;
    /*if(!checkpoint_stat_list){
    checkpoint_stat_list = (struct list_head*)os_chunk_alloc(sizeof(struct list_head),NVM_META_REG);
    init_list_head(checkpoint_stat_list);
    }*/

    u64 stack_next = ctx->mms[MM_SEG_STACK].next_free;
    u64 stack_end = ctx->mms[MM_SEG_STACK].end;
    u64 stack_start = ctx->mms[MM_SEG_STACK].start;
    if(!ptr_dirtymap){
       //printk("only once\n");
       unsigned initial_size = (stack_end-stack_next);
       unsigned nvm_stack_pages = STACK_EXP_SIZE%PAGE_SIZE?(STACK_EXP_SIZE>>PAGE_SHIFT)+1:(STACK_EXP_SIZE>>PAGE_SHIFT);
       ctx->nvm_stack = (u64)os_page_alloc_pages(NVM_USER_REG, nvm_stack_pages);
       //printk("initial_pages:%u, dest_addr:%x\n",initial_pages,dest_addr_base);
       //printk("interval:%u, size:%u, log:%u\n",CHECKPOINT_INTERVAL,TRACK_SIZE,LOG_TRACK_GRAN);
       u64 dest_addr = ctx->nvm_stack+initial_size;
       u64 addr = stack_next;
       //u64 cache_addr = 0;
       while( addr < stack_end ){
	   dest_addr -= PAGE_SIZE;
	   memcpy((char*)dest_addr,(char*)addr,PAGE_SIZE);
	   addr += PAGE_SIZE;
           clflush_multiline(dest_addr,4096);
       }
       unsigned stack_track_chunks = (stack_end-stack_start)>>LOG_TRACK_GRAN;
       unsigned dirty_bytes = stack_track_chunks >> 3;
       unsigned dirty_pages = dirty_bytes%PAGE_SIZE?(dirty_bytes>>PAGE_SHIFT)+1:(dirty_bytes>>PAGE_SHIFT);
       ptr_dirtymap = (u32*)os_page_alloc_pages(FILE_STORE_REG, dirty_pages);
       //printk("dirty pages:%u, ptr:%x\n",dirty_pages,ptr_dirtymap);
       int gran_8bytes = (dirty_bytes%8)?(dirty_bytes>>3)+1:dirty_bytes>>3;
       for(int j=0; j<gran_8bytes; j++){
            *(u64*)((unsigned long)ptr_dirtymap+j*8) = 0;
       }

       unsigned numbits = STACK_EXP_SIZE >> LOG_TRACK_GRAN;
       unsigned logentries = numbits>>3; //EXTEND is 8
       unsigned logsize = logentries*(sizeof(struct stack_entry)+((1UL<<LOG_TRACK_GRAN)*8));//max size for 0xff 
       unsigned log_pages = logsize%PAGE_SIZE?(logsize>>PAGE_SHIFT)+1:(logsize>>PAGE_SHIFT);       
       se_base = (struct stack_entry*)os_page_alloc_pages(NVM_META_REG, log_pages);
       //printk("log pages:%u, log_base:%x\n",log_pages,se_base);
       
       se_list = (struct list_head*)os_chunk_alloc(sizeof(struct list_head),NVM_META_REG);
       init_list_head(se_list);

       checkpoint_stat_list = (struct list_head*)os_chunk_alloc(sizeof(struct list_head),NVM_META_REG);
       init_list_head(checkpoint_stat_list);
       
       saved_regs = (struct user_regs*)os_chunk_alloc(sizeof(struct user_regs),NVM_META_REG);
       btd = (struct bitmap_data*)os_page_alloc(FILE_DS_REG);
       for(int i=0; i<256; i++){
           u8 pos = 0;
           u8 stream = 0;
           u8 loc = 0;
           struct bitmap_data * dtemp = (struct bitmap_data *)((unsigned long)btd+(i*sizeof(struct bitmap_data)*4));
           struct bitmap_data * prev = NULL;
           while(pos<8){
               if(i&(1<<(7-pos))){
                   stream += 1;
               }else{
                   if(stream){
                       loc = pos - stream;
                       dtemp->pos = loc;
                       dtemp->stream = stream;
		       dtemp->next = NULL;
		       if(prev){
		           prev->next = dtemp;
		       }
		       prev = dtemp;
                       dtemp += 1;
                       stream = 0;
		   }
	       }
               pos += 1;
	   }
           if(stream){
               loc = pos - stream;
               dtemp->pos = loc;
               dtemp->stream = stream;
	       dtemp->next = NULL;
	       if(prev){
	           prev->next = dtemp;
	       }
	   }
       }
       /*for(int i=0; i<256; i++){
        struct bitmap_data * dtemp = (struct bitmap_data *)((unsigned long)btd+(i*sizeof(struct bitmap_data)*4));
        printk("i:%u\n",i);
        for(int j=0; j<4; j++){
            printk("loc:%u,stream:%u, addr:%lx, next:%lx\t",dtemp->pos,dtemp->stream,(unsigned long)dtemp,(unsigned long)dtemp->next);
            dtemp += 1;
        }
        printk("\n");
    }*/

    }
    /*if(extra_log_entry > 0){
        printk("extra logs used\n");
    }*/
    list_add_tail(&se_base->list, se_list);
    writemsr(MISCREG_SSP_ADDR, (u64)ptr_sspmap);
    int x = 0;
    while(x<10){
        unsigned dummy_read = *(unsigned*)(ptr_sspmap);
        x++;
    }
    writemsr(MISCREG_TRACK_START, (u64)stack_start);
    writemsr(MISCREG_TRACK_END, (u64)stack_end);
    writemsr(MISCREG_DIRTYMAP_ADDR, (u64)ptr_dirtymap);
    writemsr(MISCREG_LOG_TRACK_GRAN, (u64)LOG_TRACK_GRAN);
    writemsr(MISCREG_TRACK_SYNC, (u64)0); //setting 0 to clear sync
    ctx->persistent |= MAKE_PERSISTENT;

    return 0;
}


//ToDo: need to add log for file operations,
//flush and fence operations to NVM
/*At end checkpoint we need to update the VPFN to NVMPFN
 * mapping maintained in NVM so as to capture VMA layout changes
 * happened in current checkpoint interval.
 * we also create log of user regs in the context.
 * then we read the dirtybit of STACK changes to create stack_entry
 * for those changes to apply those later.
 * we change the latest context information after applying logged
 * changes, a crash after this has no extra work as all changes in current
 * checkpoint are captured and applied*/
unsigned checkpoint_num = 0;
unsigned num_evict = 0;
unsigned bytes_copy_ssp = 0;
u64 time_merge = 0;

int end_checkpoint(struct exec_context *ctx) {
    //dprintk("end checkpoint\n");
    checkpoint_num += 1;
    extern struct ssp_entry* ptr_sspmap;

    writemsr(MISCREG_LOG_TRACK_GRAN, (u64)0);
    //printk("checkpoint_num: %u\n",checkpoint_num);
    /* byte granularity tracking*/
    //extern u32* ptr_dirtymap;
    //extern unsigned extra_log_entry;
    unsigned log_index = 0;
    unsigned log_entry_size = sizeof(struct stack_entry)+((1UL<<LOG_TRACK_GRAN)*8);
    //extern struct stack_entry * se_base;
    if(!ptr_dirtymap){
        printk("issue with ptr_dirtymap at %s\n",__func__);
    }
    /*This load is to invoke the hardware comparator flush*/
    volatile u32 temp = *(u32*)(ptr_dirtymap+8);
    temp = *(u32*)(ptr_dirtymap+4);
    temp = *(u32*)(ptr_dirtymap+8);
    unsigned log_write_done = 0;
    unsigned check_counter = 0;
    //dprintk("end log write done:%u\n",log_write_done);
    unsigned start_hi = 0, start_lo = 0, end_hi = 0, end_lo = 0;
    u64 rdtsc_value_1 = 0;
    u64 rdtsc_value_2 = 0;
    u32 bytes_copied = 0;
    u64 stack_next = ctx->mms[MM_SEG_STACK].next_free;
    u64 stack_end = ctx->mms[MM_SEG_STACK].end;
    u64 stack_start = ctx->mms[MM_SEG_STACK].start;
    int x = 0;
    while(x<10){
        unsigned dummy_read = *(unsigned*)(ptr_sspmap);
        x++;
    }	
    u64 vaddr;
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
    log_write_done = readmsr(MISCREG_TRACK_SYNC);
    while(!log_write_done){
        check_counter += 1;
        if(check_counter%10 == 0){
             printk("log write not done in :%u\n",check_counter);
	     //sleep(1);
	     break;
	}
        /*This load is to invoke the hardware comparator flush*/
        temp = *(u32*)(ptr_dirtymap+4);
        log_write_done = readmsr(MISCREG_TRACK_SYNC);
    }
    //printk("check count:%u\n",check_counter);
    writemsr(MISCREG_TRACK_SYNC, (u64)0); //setting 0 to clear sync
    //SSP Part
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
		}
    //dprintk("after loop\n");
    RDTSC_STOP();
    rdtsc_value_1 = elapsed(start_hi,start_lo,end_hi,end_lo);

    if(!saved_regs ){
            printk("Bug!!%s\n",__func__);
    }
    u64 sp_value = (saved_regs != NULL && (stack_next < saved_regs->entry_rsp 
			    && saved_regs->entry_rsp < stack_end)) ? saved_regs->entry_rsp : stack_next; 
    //printk("sp_value:%x,%x, stack_start:%x,stack_end:%x, start_next:%x\n",sp_value,saved_regs->entry_rsp,stack_start,stack_end,stack_next);
    u64 min_address = readmsr(MISCREG_TRACK_MIN);
    unsigned stack_track_chunks = (stack_end-stack_start)>>LOG_TRACK_GRAN;
    unsigned start_bit_pos = (sp_value - stack_start)>>LOG_TRACK_GRAN;
    //printk("sp_value:%x, stack_start:%x,stack_end:%x, start_bit_pos:%u, total:%u\n",sp_value,stack_start,stack_end,start_bit_pos,stack_track_chunks);
    if(start_bit_pos<0){
        printk("Bug!!! at %s\n",__func__);
    }
    //unsigned start_4byte_pos = start_bit_pos >> 5;
    //unsigned long num_4bytes = stack_track_chunks>>5; 
    unsigned start_8byte_pos = start_bit_pos >> 6;
    unsigned long num_8bytes = stack_track_chunks>>6; 

    struct stack_entry * se;
    u64 addr;
    u64 dest_addr;
    u32 size;
    u32 size2;
    u64 cache_addr = 0;
    u32 offset;
    start_hi = 0, start_lo = 0, end_hi = 0, end_lo = 0;
    RDTSC_START();
    for(int loop_index = start_8byte_pos; loop_index < num_8bytes; loop_index++){
	//printk("loop index:%u, num_8bytes:%u\n",loop_index,num_8bytes);
        u64* base_64 = (u64*)ptr_dirtymap+loop_index;
        const register u64 value_64 = *base_64;
	*base_64 = 0;
	if(value_64){
            for(int i=0; i<8; i++){
		const register u8 value_8 = (value_64>>(i*8))&0xff;
		if(value_8){
		    u64 bit_num = (((u64)base_64-(u64)ptr_dirtymap)+(7-i))<<3;
                    struct bitmap_data * dtemp = (struct bitmap_data *)(
					(unsigned long)btd+(value_8*sizeof(struct bitmap_data)*4));
		    se = (struct stack_entry*)((u64)se_base+(log_entry_size*log_index)); //base+log_index*log_size
		    se->addr[0] = 0;
		    se->addr[1] = 0;
		    se->addr[2] = 0;
		    se->addr[3] = 0;
		    se->size[0] = 0;
		    se->size[1] = 0;
		    se->size[2] = 0;
		    se->size[3] = 0;
                    log_index += 1;
                    u32 running_size = 0; // for 8 bits
		    int temp_log_index = 0;
		    while(dtemp){ //max 4 for 10101010 pattern
	                addr = stack_start+((bit_num+dtemp->pos)*(1UL<<LOG_TRACK_GRAN));
	                size = (1UL<<LOG_TRACK_GRAN)*dtemp->stream;
	                //int gran_8bytes = (size%8)?(size>>3)+1:size>>3;
                        se->addr[temp_log_index] = addr;
	                se->size[temp_log_index] = size;
			dest_addr = (u64)se->payload.byte+running_size;
			/*for(int k=0; k<gran_8bytes; k++){
			    *(u64*)(dest_addr+k*8) = *(u64*)(addr+k*8);
			}*/
			memcpy((char*)dest_addr,(char*)addr,size);
			running_size += size;
		        bytes_copied += size;
			temp_log_index += 1;
			dtemp = dtemp->next;
		    }
                    clflush_multiline_new((u64)se,(running_size+sizeof(struct stack_entry)));
		}
	    }
	}
    }
    asm volatile("sfence":::"memory");
    if(min_address < sp_value){
        unsigned min_addr_bit_pos = (min_address - stack_start)>>LOG_TRACK_GRAN;
        unsigned min_addr_8byte_pos = min_addr_bit_pos >> 6;
        //dprintk("going to bzero, pos:%u start:%u\n",min_addr_4byte_pos,start_4byte_pos);
	while( min_addr_8byte_pos < start_8byte_pos){
            u64* base_64 = (u64*)ptr_dirtymap+min_addr_8byte_pos;
	    *base_64 = 0;
	    min_addr_8byte_pos +=1;
	}
    }
    unsigned total_log = log_index;
    //printk("total log:%u\n",total_log);
    for(log_index=0; log_index<total_log; log_index++){
        se = (struct stack_entry*)((u64)se_base+(log_entry_size*log_index)); //base+log_index*log_size
        u32 running_size = 0;
	for(int i=0;i<4;i++){
            if(se->size[i]){
                unsigned offset = stack_end - se->addr[i];
		u64 dest_nvm_addr = ctx->nvm_stack+offset;
		u64 src_nvm_addr = (u64)se->payload.byte+running_size;
		memcpy((char*)dest_nvm_addr,(char*)src_nvm_addr,se->size[i]);
                clflush_multiline_new((u64)dest_nvm_addr,se->size[i]);
		running_size += se->size[i];
	    }
	}
    }
    asm volatile("sfence":::"memory");
    RDTSC_STOP();
    rdtsc_value_2 = elapsed(start_hi,start_lo,end_hi,end_lo);

    struct checkpoint_stats* chst = (struct checkpoint_stats*)os_chunk_alloc(
		    sizeof(struct checkpoint_stats),NVM_META_REG);
    chst->num = checkpoint_num;
    chst->time_to_prosper = rdtsc_value_2;
    chst->bytes_copy_prosper = bytes_copied;
    chst->time_to_ssp = rdtsc_value_1;
    chst->time_merge = time_merge;
    chst->bytes_copy_ssp = bytes_copy_ssp;
    chst->num_evict = num_evict;
    //dprintk("chst->list:%x\n",&chst->list);
    list_add_tail(&chst->list,checkpoint_stat_list);
    num_evict = 0;
    bytes_copy_ssp = 0;
    time_merge = 0;  
    return 0;
}

//unsigned num_merges = 0;
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
            //num_evict += 1;
            for(int i=0; i<64; i++){

            // copying modified cache lines from p1 to p0
                if(temp_commit_entry->commit_bitmap & (1UL<<i)){
                    src_addr = temp_commit_entry->p1+(i*64);
                    dest_addr = temp_commit_entry->p0+(i*64);
                    //copy at 8 byte granularity
                    for(int j=0; j<8; j++){
                        *(u64*)(dest_addr+j*8) = *(u64*)(src_addr+j*8);
                    }
                    bytes_copy_ssp += 64;
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
u8 print_checkpoint_stats(){
    struct checkpoint_stats* chst;
    struct list_head* temp;
    struct list_head* head;
    head = checkpoint_stat_list;
    temp = head->next;
    //dprintk("head:%x\n",head);
    while( temp != head ){
        chst = list_entry(temp, struct checkpoint_stats, list);
        printk("checkpoint:%u,time_prosper:%lu,size_prosper:%u,time_ssp:%lu,time_merge:%lu,size_ssp:%u\n", chst->num, chst->time_to_prosper, chst->bytes_copy_prosper, chst->time_to_ssp, chst->time_merge, chst->bytes_copy_ssp);
        temp = temp->next;
    }
    return 0;
}

