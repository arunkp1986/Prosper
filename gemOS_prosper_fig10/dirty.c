#include<dirty.h>
#include<lib.h>
#include<kbd.h>
#include<entry.h>
#include<msr.h>
#include<page.h>
#include<context.h>
#include<memory.h>
#include<schedule.h>
#include<apic.h>
#include <nonvolatile.h>


extern long do_sleep(u32 ticks);
extern void pit_5secs_delay(void);
/*
 This starts the tracking of dirty area at tracking granularity.
 reads stack start and end at the start.
 * 1 page can track 4096*8*(1<<LOG_TRACK_GRAN) bytes of stack area.
 * */
u32* ptr_dirtymap;
struct stack_entry * se_base;
unsigned log_index = 0;
unsigned extra_log_entry = 0;
u16 t_size = 0;
void start_track(struct exec_context* ctx, u16 track_size){
    dprintk("dirty tracking on\n");
    u16 LOG_TRACK_GRAN = 0;
    t_size = track_size;
    if(track_size == 8)
        LOG_TRACK_GRAN = 3;
    else if(track_size == 16)
        LOG_TRACK_GRAN = 4;
    else if(track_size == 32)
        LOG_TRACK_GRAN = 5;
    else if(track_size == 64)
        LOG_TRACK_GRAN = 6;
    else if(track_size == 128)
        LOG_TRACK_GRAN = 7;
    else
        printk("LOG_TRACK_GRAN is not set, value:%u\n",LOG_TRACK_GRAN);

    unsigned long stack_start = ctx->mms[MM_SEG_STACK].start;
    unsigned long stack_end = ctx->mms[MM_SEG_STACK].end;
    struct saved_state* ss = get_saved_state(ctx);
    if(!ptr_dirtymap){
        unsigned stack_track_chunks = (stack_end-stack_start)>>LOG_TRACK_GRAN;
        unsigned dirty_area_bytes = stack_track_chunks >> 3;
        unsigned dirty_num_pages = (dirty_area_bytes >> 12)+1;
        ptr_dirtymap = (u32*)os_page_alloc_pages(FILE_STORE_REG,dirty_num_pages);
	dprintk("dirty bitmap area, only once, ptr:%x\n",ptr_dirtymap);
        //bzero((char *)ptr_dirtymap, dirty_area_bytes);
	int gran_8bytes = (dirty_area_bytes%8)?(dirty_area_bytes>>3)+1:dirty_area_bytes>>3;
        for(int j=0; j<gran_8bytes; j++){
            *(u64*)((unsigned long)ptr_dirtymap+j*8) = 0;
	}
	unsigned numbits = STACK_EXP_SIZE >> LOG_TRACK_GRAN;
        unsigned logentries = numbits>>2; //EXTEND is 4
	unsigned logsize = logentries*(sizeof(struct stack_entry)+
			((1UL<<LOG_TRACK_GRAN)*EXTEND));
	unsigned log_num_pages = logsize%PAGE_SIZE?(logsize>>12)+1:(logsize>>12);
	se_base = (struct stack_entry*)os_page_alloc_pages(NVM_META_REG, log_num_pages);
	//printk("log num pages:%u, base addr:%lx\n",log_num_pages,(unsigned long)se_base);
    }
    list_add_tail(&se_base->list, &ss->nv_stack);
    dprintk("ptr:%x\n",ptr_dirtymap);
    log_index = 0;
    writemsr(MISCREG_TRACK_START, (u64)stack_start);
    writemsr(MISCREG_TRACK_END, (u64)stack_end);
    writemsr(MISCREG_DIRTYMAP_ADDR, (u64)ptr_dirtymap);
    writemsr(MISCREG_LOG_TRACK_GRAN, (u64)LOG_TRACK_GRAN);
    //__asm__ __volatile__("sfence" ::: "memory");
    return;
}

/*
void end_track(struct exec_context *ctx){
#ifdef TRACK_ON
    printk("dirty tracking on\n");
    __asm__ __volatile__("sfence" ::: "memory");
    writemsr(MISCREG_LOG_TRACK_GRAN, (u64)1);
#endif
    x86_dump_gem5_stats();
    x86_reset_gem5_stats();
    return;
}*/


void alloc_se_dirty_bitmap(struct exec_context *ctx, u64 start, u64 end, u16 track_size){
    dprintk("start :%x end:%x size:%x\n",start, end, track_size);
    u16 LOG_TRACK_GRAN = 0;
    if(track_size == 8)
        LOG_TRACK_GRAN = 3;
    else if(track_size == 16)
        LOG_TRACK_GRAN = 4;
    else if(track_size == 32)
        LOG_TRACK_GRAN = 5;
    else if(track_size == 64)
        LOG_TRACK_GRAN = 6;
    else if(track_size == 128)
        LOG_TRACK_GRAN = 7;
    else
        printk("LOG_TRACK_GRAN is not set, value:%u\n",LOG_TRACK_GRAN);

    struct saved_state* ss = get_saved_state(ctx);
    static u32 current_entries = 0;
    unsigned extra_entries = 0;
    u32 num_entries = 0;
    if(((end-start)>>LOG_TRACK_GRAN)<<LOG_TRACK_GRAN == (end-start)){
        num_entries = (end-start)>>LOG_TRACK_GRAN;
    }
    else{
        num_entries = ((end-start)>>LOG_TRACK_GRAN)+1;
    }

    if (num_entries > current_entries){
        extra_entries = num_entries - current_entries;
    }

    dprintk("current entries: %d extra_entries:%d \n",current_entries,extra_entries);
    if(extra_entries > 100){
        extra_entries = 100;
    }

    while(extra_entries){
        dprintk("j %d at %s\n",extra_entries, __func__);
        struct stack_entry * se = (struct stack_entry*)os_chunk_alloc(
			sizeof(struct stack_entry)+((1UL<<LOG_TRACK_GRAN)*EXTEND),NVM_META_REG);
	se->addr = 0;
        //se->payload = (void*)os_chunk_alloc((1UL<<LOG_TRACK_GRAN)*EXTEND,NVM_META_REG);
	current_entries += 1;
        list_add_tail(&se->list, &ss->nv_stack);
        //clflush_multiline((u64)se, sizeof(struct stack_entry));
        //clflush_multiline((u64)&ss->nv_stack, sizeof(struct list_head));
	extra_entries -= 1;
    }
}

//optimize code for checking contiguous 0 s

u32 read_dirty_bitmap(struct exec_context *ctx, u64 start, u64 end, u16 track_size){
    //u32 temp = *(u32*)(ptr_dirtymap);
    u32 bytes_copied = 0;
    u16 LOG_TRACK_GRAN = 0;
    if(track_size == 8)
        LOG_TRACK_GRAN = 3;
    else if(track_size == 16)
        LOG_TRACK_GRAN = 4;
    else if(track_size == 32)
        LOG_TRACK_GRAN = 5;
    else if(track_size == 64)
        LOG_TRACK_GRAN = 6;
    else if(track_size == 128)
        LOG_TRACK_GRAN = 7;
    else
        printk("LOG_TRACK_GRAN is not set, value:%u\n",LOG_TRACK_GRAN);

    u64 min_address = readmsr(MISCREG_TRACK_MIN);
    if(min_address == 0xFFFFFFFFFF){
        return 0;
    }
    
    unsigned numbits = STACK_EXP_SIZE >> LOG_TRACK_GRAN;
    unsigned logentries = numbits>>2; //EXTEND is 4

    unsigned long stack_start = ctx->mms[MM_SEG_STACK].start;
    unsigned stack_track_chunks = (end-stack_start)>>LOG_TRACK_GRAN;
    unsigned start_bit_pos = (start - stack_start)>>LOG_TRACK_GRAN;
    if(start_bit_pos<0){
        printk("Bug!!! at %s\n",__func__);
    }
    unsigned start_4byte_pos = start_bit_pos >> 5;
    unsigned long num_4bytes = stack_track_chunks>>5;
    struct saved_state* ss = get_saved_state(ctx);
    
    struct stack_entry * se;
    struct list_head* nv_stack_head = &ss->nv_stack;
    struct list_head* next_stack_entry = (ss->nv_stack).next;

    u64 addr;
    u32 offset;
    for(int loop_index = start_4byte_pos; loop_index < num_4bytes; loop_index++){
	const register u64 value = *(u32*)(ptr_dirtymap+loop_index);
	dprintk("addr:%x, value:%x\n",(ptr_dirtymap+loop_index),value);
	if(value){
            //bzero((char *)(ptr_dirtymap+loop_index), 4);
	    dprintk("value:%x\n",value);
	    *(u32*)(ptr_dirtymap+loop_index) = 0;
            for(int j=0; j<32; ){
		dprintk("j : %d at %s\n",j, __func__);
		u8 condition = (((value>>j) & 0xf) == 0x0);
                if(condition){
                   j += EXTEND;
		   continue;
		}	
		condition = (((value>>j) & 0xf) == 0xf);
		if(condition){
		   dprintk("inside value 0xf condition, value %x\n",value);
                   addr = stack_start+(((loop_index<<5)+j)*(1UL<<LOG_TRACK_GRAN));
		   offset = addr&0xfff;
		   struct mapping_entry* me = get_nv_mapping(ss,addr);
                   if(!me)
                       printk("Bug!!! %s\n",__func__);
                   if(me){
                       if(log_index >= logentries){
                           printk("normally should not come here\n");
		           se = (struct stack_entry*)os_chunk_alloc(
					   sizeof(struct stack_entry)+((1UL<<LOG_TRACK_GRAN)*EXTEND),NVM_META_REG);
                           //se->payload = (void*)os_chunk_alloc((1UL<<LOG_TRACK_GRAN)*EXTEND,NVM_META_REG);
                           list_add_tail(&se->list, &ss->nv_stack);
			   extra_log_entry += 1;
		           //next_stack_entry = &(se->list);
			   //clflush_multiline((u64)&ss->nv_stack, sizeof(struct list_head));
		       }
		       else{
		           //se = list_entry(next_stack_entry, struct stack_entry, list);
			   //printk("log_index:%u\n",log_index);
			   se = (struct stack_entry*)(se_base+log_index);
			   log_index += 1;
		           if(!se)
                               printk("Bug!!! %s\n",__func__);
		       }
	               se->addr = ((me->nvaddr)<<PAGE_SHIFT)+offset;
	               se->size = (1UL<<LOG_TRACK_GRAN)*EXTEND;
		       int gran_8bytes = (se->size%8)?(se->size>>3)+1:se->size>>3;
                       //printk("gran:%u\n",gran_8bytes);
                       u64 cache_addr = 0;
                       for(int j=0; j<gran_8bytes; j++){
                           *(u64*)(se->payload.byte+j*8) = *(u64*)(addr+j*8);
                           if(cache_addr != (((unsigned long)se->payload.byte+j*8)&~((1<<6)-1))){
                               cache_addr = (((unsigned long)se->payload.byte+j*8)&~((1<<6)-1));
                               clflush_multiline(cache_addr,64);
                           }
                       }
	               //memcpy((char*)se->payload.byte,(char*)addr,(1UL<<LOG_TRACK_GRAN)*EXTEND);
		       bytes_copied += (1UL<<LOG_TRACK_GRAN)*EXTEND;
                       //next_stack_entry = next_stack_entry->next;
	               //clflush_multiline((u64)se->payload.byte, (1UL<<LOG_TRACK_GRAN)*EXTEND);
                       //clflush_multiline((u64)se, sizeof(struct stack_entry)+(1UL<<LOG_TRACK_GRAN)*EXTEND);
		   }
		   j += EXTEND;
		   continue;
	       }
	       if((value>>j) & 0x1){
		   dprintk("inside value not 0xf condition\n");
                   addr = stack_start+(((loop_index<<5)+j)*(1UL<<LOG_TRACK_GRAN));
		   offset = addr&0xfff;
		   struct mapping_entry* me = get_nv_mapping(ss,addr);
                   if(!me)
                       printk("Bug!!! %s\n",__func__);
                   if(me){
                       if(log_index >= logentries){
                           printk("normally should not come here\n");
		           se = (struct stack_entry*)os_chunk_alloc(
					   sizeof(struct stack_entry)+((1UL<<LOG_TRACK_GRAN)*EXTEND),NVM_META_REG);
                           //se->payload = (void*)os_chunk_alloc((1UL<<LOG_TRACK_GRAN)*EXTEND,NVM_META_REG);
                           list_add_tail(&se->list, &ss->nv_stack);
			   extra_log_entry += 1;
		           //next_stack_entry = &(se->list);
			   //clflush_multiline((u64)&ss->nv_stack, sizeof(struct list_head));
		       }
                       else{
		           //se = list_entry(next_stack_entry, struct stack_entry, list);
			   //printk("log_index:%u\n",log_index);
			   se = (struct stack_entry*)(se_base+log_index);
                           log_index += 1;
		           if(!se)
                               printk("Bug!!! %s\n",__func__);
		       }
	               se->addr = ((me->nvaddr)<<PAGE_SHIFT)+offset;
	               se->size = (1UL<<LOG_TRACK_GRAN);
		       int gran_8bytes = (se->size%8)?(se->size>>3)+1:se->size>>3;
                       //printk("gran:%u\n",gran_8bytes);
                       u64 cache_addr = 0;
                       for(int j=0; j<gran_8bytes; j++){
                           *(u64*)(se->payload.byte+j*8) = *(u64*)(addr+j*8);
                           if(cache_addr != (((unsigned long)se->payload.byte+j*8)&~((1<<6)-1))){
                               cache_addr = (((unsigned long)se->payload.byte+j*8)&~((1<<6)-1));
                               clflush_multiline(cache_addr,64);
                           }
                       }
	               //memcpy((char*)se->payload.byte,(char*)addr,(1UL<<LOG_TRACK_GRAN));
		       bytes_copied += (1UL<<LOG_TRACK_GRAN);
                       //next_stack_entry = next_stack_entry->next;
	               //clflush_multiline((u64)se->payload.byte, (1UL<<LOG_TRACK_GRAN));
                       //clflush_multiline((u64)se, sizeof(struct stack_entry)+(1UL<<LOG_TRACK_GRAN));
		   }
	       }
	       j += 1; 
	    }
	}
    }
    dprintk("min_address:%x start:%x\n",min_address,start);
    if(min_address < start){
        unsigned min_addr_bit_pos = (min_address - stack_start)>>LOG_TRACK_GRAN;
        unsigned min_addr_4byte_pos = min_addr_bit_pos >> 5;
        dprintk("going to bzero, pos:%u start:%u\n",min_addr_4byte_pos,start_4byte_pos);
	while( min_addr_4byte_pos < start_4byte_pos){
	    //bzero((char *)(ptr_dirtymap+min_addr_4byte_pos), 4);
	    *(u32*)(ptr_dirtymap+min_addr_4byte_pos) = 0;
	    min_addr_4byte_pos +=1;
	}
    }
    return bytes_copied; 
}

void custom_print(u64 temp){
    printk("value: %x\n",temp);
}

void clear_dirtybit(struct exec_context *ctx, u64 start, u64 end){
    u64 addr = start;
    while( addr<end ){
        u64* pte = get_user_pte(ctx, addr, 0);
	if(pte && (*pte&(1UL<<6))){
	    //printk("address:%x pte entry before:%x at %s\n",addr, *pte, __func__);
	    *pte &= ~(1UL<<6);
	    clflush_multiline((u64)pte,sizeof(unsigned long));
	    //printk("address:%x pte entry after:%x at %s\n",addr, *pte, __func__);
	}
	
	asm volatile ("invlpg (%0);"
                   :: "r"(addr)
                   : "memory");   // Flush TLB
        
	addr += PAGE_SIZE;
    }
}

void alloc_se_dirtybit(struct exec_context *ctx, u64 start, u64 end){
    struct saved_state* ss = get_saved_state(ctx);
    static u32 current_entries = 0;
    u32 extra_entries = 0;
    u32 num_entries = 0;
    if(((end - start)>>PAGE_SHIFT)<<PAGE_SHIFT == (end - start)){
        num_entries = (end - start)>>PAGE_SHIFT;
    }
    else{
        num_entries = ((end - start)>>PAGE_SHIFT)+1;
    }
    
    if(num_entries > current_entries){
        extra_entries = num_entries - current_entries;
    }
    dprintk("current entries: %d\n",current_entries);
    while(extra_entries){
        struct stack_entry * se = (struct stack_entry*)os_chunk_alloc(
			sizeof(struct stack_entry),NVM_META_REG);
        se->payload.page = os_page_alloc(NVM_USER_REG);
	current_entries += 1;
	list_add_tail(&se->list, &ss->nv_stack);
        //clflush_multiline((u64)se, sizeof(struct stack_entry));
        //clflush_multiline((u64)&ss->nv_stack, sizeof(struct list_head));
	extra_entries -= 1;
    }
}


u32 read_dirtybit(struct exec_context *ctx, u64 start, u64 end){
    dprintk("read dirty bit called with start: [%x] and end: [%x]\n",start, end);
    u64 addr = start & ~(0xfff) ;
    u32 bytes_copied = 0;
    u64* pte;
    u64 nvaddr = 0;
    struct saved_state* ss = get_saved_state(ctx);
    struct stack_entry * se;
    struct list_head* nv_stack_head = &ss->nv_stack;
    struct list_head* next_stack_entry = (ss->nv_stack).next;

    while( addr<end ){
        pte = get_user_pte(ctx, addr, 0);
	if(pte && (*pte &(1UL<<6))){
	    struct mapping_entry* me = get_nv_mapping(ss,addr);
            if(!me)
                printk("Bug!!! %s\n",__func__);
	    if(me){
                dprintk("read dirtybit stack %x\n", addr);
                if(next_stack_entry == nv_stack_head){
                    printk("normally should not come here\n");
		    se = (struct stack_entry*)os_chunk_alloc(sizeof(struct stack_entry),NVM_META_REG);
                    se->payload.page = os_page_alloc(NVM_USER_REG);
                    list_add_tail(&se->list, &ss->nv_stack);
	            next_stack_entry = &(se->list);
                    //clflush_multiline((u64)se, sizeof(struct stack_entry));
		    //clflush_multiline((u64)&ss->nv_stack, sizeof(struct list_head));
		}
                else{
                    se = list_entry(next_stack_entry, struct stack_entry, list);
		    if(!se)
                        printk("Bug!!! %s\n",__func__);
		}
	        se->addr = (me->nvaddr)<<PAGE_SHIFT;
	        se->size = PAGE_SIZE;
		int gran_8bytes = (se->size%8)?(se->size>>3)+1:se->size>>3;
                u64 cache_addr = 0;
                for(int j=0; j<gran_8bytes; j++){
                    *(u64*)(se->payload.page+j*8) = *(u64*)(addr+j*8);
                    if(cache_addr != (((unsigned long)se->payload.page+j*8)&~((1<<6)-1))){
                        cache_addr = (((unsigned long)se->payload.page+j*8)&~((1<<6)-1));
                        clflush_multiline(cache_addr,64);
		    }
		}
	        //memcpy((char*)se->payload.page,(char*)((addr>>PAGE_SHIFT)<<PAGE_SHIFT),PAGE_SIZE);
		bytes_copied += PAGE_SIZE;
                next_stack_entry = next_stack_entry->next;
	        //clflush_multiline((u64)se->payload.page, PAGE_SIZE);
                //clflush_multiline((u64)se, sizeof(struct stack_entry));
	    }
	    *pte &= ~(1UL<<6);
	    clflush_multiline((u64)pte,sizeof(unsigned long));
	    asm volatile ("invlpg (%0);"
                   :: "r"(addr)
                   : "memory");   // Flush TLB
        }
	addr += PAGE_SIZE;
    }
    //to clear stack pages between SP and next_free
    clear_dirtybit(ctx, ctx->mms[MM_SEG_STACK].next_free, start);
    return bytes_copied;
}

u8 print_checkpoint_stats(){
    struct checkpoint_stats* chst;
    struct list_head* temp;
    struct list_head* head;
    head = checkpoint_stat_list;
    temp = head->next;
    dprintk("head:%x\n",head);
    while( temp != head ){
        chst = list_entry(temp, struct checkpoint_stats, list);
        printk("checkpoint:%u, log_time:%lu, copy_time:%lu, copy_size:%u \n", chst->num, chst->time_to_log, chst->time_to_copy, chst->bytes_copy);	
        temp = temp->next;
    }
    return 0;
}
