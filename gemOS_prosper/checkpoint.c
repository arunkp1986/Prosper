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
/*
void copy_ctx(struct exec_context *dest_ctx, struct exec_context * src_ctx, u8 to_nvm) {
    memcpy((char*)dest_ctx,(char*)src_ctx,sizeof(struct exec_context));
    struct vm_area* vm = src_ctx->vm_area;
    struct vm_area* prev = NULL;
    struct vm_area* head = NULL;
    struct vm_area* curr;
    struct file * fp;
    while(vm){
	if(to_nvm){
            curr = (struct vm_area*)os_chunk_alloc(sizeof(struct vm_area), NVM_META_REG);
	    if(!curr){
	        printk("os chunk alloc failed at  %s\n",__func__);
	    }
	}
	else{
            curr = (struct vm_area*)os_alloc(sizeof(struct vm_area), OS_DS_REG);
	    if(!curr){
	        dprintk("curr to nvm return null in %s\n",__func__);
	    }
	}
        curr->vm_start = vm->vm_start;
        curr->vm_end = vm->vm_end;
        curr->access_flags = vm->access_flags;
	curr->is_nvm = vm->is_nvm;
	if(to_nvm){
	    clflush_multiline((u64)curr, sizeof(struct vm_area));
	}
	if(prev){
            prev->vm_next = curr;
	    if(to_nvm){
	        clflush_multiline((u64)&prev->vm_next, sizeof(struct vm_area*));
	    }
	}
	if(!head)
            head = curr;
	prev = curr;
	vm = vm->vm_next;
    }
    dest_ctx->vm_area = head;
    if(to_nvm){
        clflush_multiline((u64)&dest_ctx->vm_area, sizeof(struct vm_area *));
    }
    for(int i=0; i<MAX_OPEN_FILES; i++){
        if(src_ctx->files[i]){
	    if(to_nvm){
                fp = (struct file*)os_chunk_alloc(sizeof(struct file), NVM_META_REG);
		if(!fp){
		    printk("os alloc failed:%s\n",__func__);
		}
	    }
	    else{
                fp = (struct file*)os_alloc(sizeof(struct file), OS_DS_REG);
		if(!fp){
		    printk("os alloc failed:%s\n",__func__);
		}

	    }
	    memcpy((char*)fp, (char*)src_ctx->files[i], sizeof(struct file));
	    dest_ctx->files[i] = fp;
	    if(to_nvm){
	        clflush_multiline((u64)fp, sizeof(struct file));
	        clflush_multiline((u64)&dest_ctx->files[i], sizeof(struct file*));
	    }
	}
    }
    if(to_nvm){
        clflush_multiline((u64)dest_ctx, sizeof(struct exec_context));
    }
}*/

/*only STACK is maintained in DRAM, all other segments
 * are maintained in NVM, so only VPFN->NVMPFN needs to
 * be saved for these segments.*/
/*
void copy_segment_into_nvm(struct exec_context *ctx, int segment) {
    struct mm_segment *seg = &ctx->mms[segment];
    struct saved_state *cs = get_saved_state(ctx);
    u64 *pte;
    u64* nvaddr;
    if (segment == MM_SEG_STACK) {
        for(u64 vaddr = seg->end-PAGE_SIZE; vaddr >= seg->next_free; vaddr -= PAGE_SIZE) {
	    pte = get_user_pte(ctx, vaddr, 0);
            if(pte) {
                u32 access_flags = (*pte) & (~FLAG_MASK);
                nvaddr = os_page_alloc(NVM_USER_REG);
	        memcpy((char *)nvaddr, (char *)vaddr, PAGE_SIZE);
	        struct mapping_entry* me = (struct mapping_entry*)os_chunk_alloc(
				sizeof(struct mapping_entry), NVM_META_REG);
		if(!me){
		    printk("chunk alloc error at %s\n",__func__);
		}
		dprintk("copying stack addr: %x at %s\n",vaddr,__func__);
                me->vaddr = vaddr>>PAGE_SHIFT;
                me->nvaddr = (u64)nvaddr>>PAGE_SHIFT;
                me->access_flags = seg->access_flags;
                list_add_tail(&me->list, &cs->nv_mappings);
		clflush_multiline((u64)nvaddr,PAGE_SIZE);
		clflush_multiline((u64)me, sizeof(struct mapping_entry));
		clflush_multiline((u64)&cs->nv_mappings, sizeof(struct list_head));
	    }
	}
    }
    else {
        for(u64 vaddr = seg->start; vaddr <= seg->next_free; vaddr += PAGE_SIZE) {
	    pte = get_user_pte(ctx, vaddr, 0);
            if(pte) {
		//clflush_multiline((u64)vaddr,PAGE_SIZE);
                u32 access_flags = (*pte) & (~FLAG_MASK);
                make_nv_mappings(ctx, vaddr, access_flags);
	    }
	}
    }
}
*/

/*The vma can be allotted in NVM or DRAM
 * if vma is in NVM, then make an entry for VPFN->NVMPFN in mapping table
 * if vma is in DRAM, then allote a page in NVM and make an entry
 * at the time of end checkpoint, copy changes in DRAM vma area to NVM page
 * nothing needs to be done for NVM vma as pages are already in NVM */
/*
void copy_vma_into_nvm(struct exec_context *ctx) {
    struct vm_area* vm = ctx->vm_area;
    struct saved_state *cs = get_saved_state(ctx);
    u64* pte;
    u64* nvaddr;
    u64 vaddr;
    while(vm){
        if(vm->is_nvm){
	    vaddr = vm->vm_start;
	    while( vaddr < vm->vm_end ){
                pte = get_user_pte(ctx, vaddr, 0);
                if(pte){
		    clflush_multiline((u64)vaddr,PAGE_SIZE);
                    u32 access_flags = (*pte) & (~FLAG_MASK);
                    make_nv_mappings(ctx, vaddr, access_flags);
		}
		vaddr += PAGE_SIZE;
	    }
	}
	else{
            vaddr = vm->vm_start;
            while( vaddr < vm->vm_end ){
	        pte = get_user_pte(ctx, vaddr, 0);
                if(pte){
                    u32 access_flags = (*pte) & (~FLAG_MASK);
                    nvaddr = os_page_alloc(NVM_USER_REG);
	            memcpy((char *)nvaddr, (char *)vaddr, PAGE_SIZE);
	            struct mapping_entry* me = (struct mapping_entry*)os_chunk_alloc(
				sizeof(struct mapping_entry), NVM_META_REG);
		    if(!me){
		        printk("os chunk alloc failed at %s\n",__func__);
		    }
                    me->vaddr = vaddr>>PAGE_SHIFT;
                    me->nvaddr = (u64)nvaddr>>PAGE_SHIFT;
                    me->access_flags = access_flags;
                    list_add_tail(&me->list, &cs->nv_mappings);
		    clflush_multiline((u64)nvaddr,PAGE_SIZE);
		    clflush_multiline((u64)me, sizeof(struct mapping_entry));
		    clflush_multiline((u64)&cs->nv_mappings, sizeof(struct list_head));
		}
		vaddr += PAGE_SIZE;
	    }
	}
	vm = vm->vm_next;
    }
}*/

/*Apply changes recorded in nv_stack log to stack pages in NVM*/
/*
void apply_nv_stack(struct saved_state *cs){
    dprintk("start apply nv stack\n");
    struct stack_entry * se;
    struct list_head* temp;
    struct list_head* head;
    head = &cs->nv_stack;
    temp = head->next;
    while( temp != head ){
        se = list_entry(temp, struct stack_entry, list);
	if( se->addr == 0){
	    dprintk("Not used stack entry!!!!\n");
	}
	else{
            if(se->size == PAGE_SIZE){
	        //memcpy((char*)se->addr,(char*)se->payload.page,se->size);
		int gran_8bytes = 512;
                u64 cache_addr = 0;
                for(int j=0; j<gran_8bytes; j++){
                    *(u64*)(se->addr+j*8) = *(u64*)((unsigned long)se->payload.page+j*8);
                    if(cache_addr != ((se->addr+j*8)&~((1<<6)-1))){
                        cache_addr = ((se->addr+j*8)&~((1<<6)-1));
                        clflush_multiline(cache_addr,64);
		    }
		}
	    }
	    else{
	        //memcpy((char*)se->addr,(char*)se->payload.byte,se->size);
		int gran_8bytes = se->size%8?(se->size>>3)+1:se->size>>3;
                u64 cache_addr = 0;
                for(int j=0; j<gran_8bytes; j++){
                    *(u64*)(se->addr+j*8) = *(u64*)((unsigned long)se->payload.byte+j*8);
                    if(cache_addr != ((se->addr+j*8)&~((1<<6)-1))){
                        cache_addr = ((se->addr+j*8)&~((1<<6)-1));
                        clflush_multiline(cache_addr,64);
		    }
		}
	    }
            //clflush_multiline((u64)se->addr, se->size);
            //printk(" temp: %x next: %x at %s\n",temp,temp->next,__func__);
	}
	temp = temp->next;
    }
    dprintk("done apply nv stack\n");
}*/


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
       //printk("nvm stack pages:%u\n",nvm_stack_pages);
       //dest_addr_base = (u64*)os_page_alloc_pages(NVM_USER_REG, initial_pages);
       ctx->nvm_stack = (u64)os_page_alloc_pages(NVM_USER_REG, nvm_stack_pages);
       //printk("initial_pages:%u, dest_addr:%x\n",initial_pages,dest_addr_base);
       //printk("interval:%u, size:%u, log:%u\n",CHECKPOINT_INTERVAL,TRACK_SIZE,LOG_TRACK_GRAN);
       u64 dest_addr = ctx->nvm_stack+initial_size;
       u64 addr = stack_next;
       u64 cache_addr = 0;
       while( addr < stack_end ){
           /*for(int j=1; j<512; j++){
               *(u64*)(dest_addr-j*8) = *(u64*)(addr+j*8);
	   }*/
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
    writemsr(MISCREG_TRACK_START, (u64)stack_start);
    writemsr(MISCREG_TRACK_END, (u64)stack_end);
    writemsr(MISCREG_DIRTYMAP_ADDR, (u64)ptr_dirtymap);
    writemsr(MISCREG_LOG_TRACK_GRAN, (u64)LOG_TRACK_GRAN);
    ctx->persistent |= MAKE_PERSISTENT;

    return 0;
}

/*get the saved context if one exits
* use the working version of execution context
* copy the persistent execution context to working copy
* apply the changes in log to working copy to make it new
* consistent copy of execution context.
* for vma, the index corresponds to index of vma in the list
* for VMA_CHANGE, index corresponds to vma changed.
* for VMA_ADD, index corresponds to vma before added vma.
* for VMA_REMOVE, index corresponds to vma removed.
* Apply stack changes in nv_stack to NVM stack pages.
* this ensures that nvm stack pages are updated with previous
* checkpoint interval changes.
* */
/*
void apply_logged_changes(struct saved_state *cs){
    dprintk("start apply logged changes\n");
    struct exec_context * saved_ctx;
    struct exec_context * update_ctx;
    saved_ctx = cs->context[cs->latest];
    update_ctx = cs->context[(cs->latest+1)%2];
    copy_ctx(update_ctx,saved_ctx,1);
    struct log_entry * lg;
    struct list_head* temp;
    list_for_each(temp,&cs->log){
        lg = list_entry(temp, struct log_entry, list);
	if(lg->event == CTX_CHANGE){
            dprintk("got ctx_change log\n");
            memcpy((char*)((u64)update_ctx+lg->address.offset),(char*)lg->payload,lg->size);
            clflush_multiline((u64)update_ctx, sizeof(struct exec_context));
	}
	else if(lg->event == VMA_CHANGE){
            dprintk("got vma_change log\n");
            struct vm_area* temp = update_ctx->vm_area;
	    for(int i=0; i<lg->address.index; i++){
                temp = temp->vm_next;
	    }
            memcpy((char*)((u64)temp+lg->address.offset),(char*)lg->payload,lg->size);
            clflush_multiline((u64)temp, sizeof(struct vm_area));
	}
	else if(lg->event == VMA_ADD){
            dprintk("got vma_add log\n");
            struct vm_area* temp = update_ctx->vm_area;
            struct vm_area* node = (struct vm_area *) os_chunk_alloc(sizeof(struct vm_area), NVM_META_REG);
	    if(!node){
	        printk("os chunk alloc failed at %s\n",__func__);
	    }
            memcpy((char*)node,(char*)lg->payload,lg->size);
            if(!temp){
	        update_ctx->vm_area = node;
	    }
            else{
                for(int i=0; i<lg->address.index; i++){
                    temp = temp->vm_next;
		}
		node->vm_next = temp->vm_next;
		temp->vm_next = node;
		}
            clflush_multiline((u64)node, sizeof(struct vm_area));
            clflush_multiline((u64)temp, sizeof(struct vm_area));
            clflush_multiline((u64)update_ctx, sizeof(struct exec_context));
	}
	else if(lg->event == VMA_REMOVE){
            dprintk("got vma_remove log\n");
            struct vm_area* prev;
	    struct vm_area* temp = update_ctx->vm_area;
	    for(int i=0; i<lg->address.index; i++){
                 prev = temp;
                 temp = temp->vm_next;
	    }
            prev->vm_next = temp->vm_next;
            clflush_multiline((u64)prev, sizeof(struct vm_area));
            os_chunk_free(NVM_META_REG, (u64)temp,sizeof(struct vm_area));
	}
    }
    dprintk("done apply logged changes\n");
    apply_nv_stack(cs);
}
*/
/**/

/*At end checkpoint we need to update nv_mappings
 * this will reflect the VA changes to segments.
 * for STACK we will allot a NVM page, but data copy 
 * to this NVM page happens based on dirty tracking.
 * */
/*
void update_segment_nv_mappings(struct exec_context *ctx, int segment){
    struct mm_segment *seg = &ctx->mms[segment];
    struct saved_state *cs = get_saved_state(ctx);
    if(!cs){
        printk("get saved context issue at %s\n",__func__);
    }
    struct exec_context * latest = cs->context[cs->latest];
    u64 *pte;
    u64* nvaddr;
    if (segment == MM_SEG_STACK) {
	latest->mms[segment].next_free = seg->next_free; 
        clflush_multiline((u64)&latest->mms[segment].next_free, sizeof(unsigned long));
        for(u64 vaddr = seg->end-PAGE_SIZE; vaddr >= seg->next_free; vaddr -= PAGE_SIZE) {
	    pte = get_user_pte(ctx, vaddr, 0);
            if(pte && !is_mapping_present(cs, vaddr)){
                u32 access_flags = (*pte) & (~FLAG_MASK);
                nvaddr = os_page_alloc(NVM_USER_REG);
	        struct mapping_entry* me = (struct mapping_entry*)os_chunk_alloc(
				sizeof(struct mapping_entry), NVM_META_REG);
		if(!me){
		    printk("os chunk alloc error at %s\n",__func__);
		}
		dprintk("updating stack mapping %x at %s\n",vaddr,__func__);
                me->vaddr = vaddr>>PAGE_SHIFT;
                me->nvaddr = (u64)nvaddr>>PAGE_SHIFT;
                me->access_flags = access_flags;
                list_add_tail(&me->list, &cs->nv_mappings);
		//clflush_multiline((u64)me, sizeof(struct mapping_entry));
		//clflush_multiline((u64)&me->list, sizeof(struct list_head));
		//clflush_multiline((u64)&cs->nv_mappings, sizeof(struct list_head));
	    }
	}
    }
    else {
        latest->mms[segment].next_free = seg->next_free; 
        clflush_multiline((u64)&latest->mms[segment].next_free, sizeof(unsigned long));
        for(u64 vaddr = seg->start; vaddr <= seg->next_free; vaddr += PAGE_SIZE) {
	    pte = get_user_pte(ctx, vaddr, 0);
            if(pte && !is_mapping_present(cs, vaddr)){
                u32 access_flags = (*pte) & (~FLAG_MASK);
                make_nv_mappings(ctx, vaddr, access_flags);
	    }
	}
    }
}*/

/*we need to update nv_mappings for vma allocations and copy data to
 * NVM for vma allotted from DRAM as we are not dirty tracking
 * for heap allocation. for vma allotted from NVM, only make entry in
 * nv_mappings*/
/*
void update_vma_nv_mappings(struct exec_context *ctx){
    struct vm_area* vm = ctx->vm_area;
    struct saved_state *cs = get_saved_state(ctx);
    u64* pte;
    u64* nvaddr;
    u64 vaddr;
    while(vm){
        if(vm->is_nvm){
	    vaddr = vm->vm_start;
	    while( vaddr < vm->vm_end ){
                pte = get_user_pte(ctx, vaddr, 0);
		if(pte){
		    clflush_multiline((u64)vaddr,PAGE_SIZE);
		}
                if(pte && !is_mapping_present(cs, vaddr)){
                    u32 access_flags = (*pte) & (~FLAG_MASK);
                    make_nv_mappings(ctx, vaddr, access_flags);
		}
		vaddr += PAGE_SIZE;
	    }
	}
	else{
            vaddr = vm->vm_start;
            while( vaddr < vm->vm_end ){
	        pte = get_user_pte(ctx, vaddr, 0);
                if(pte && !is_mapping_present(cs, vaddr)){
                    u32 access_flags = (*pte) & (~FLAG_MASK);
                    nvaddr = os_page_alloc(NVM_USER_REG);
	            memcpy((char *)nvaddr, (char *)vaddr, PAGE_SIZE);
	            struct mapping_entry* me = (struct mapping_entry*)os_chunk_alloc(
				sizeof(struct mapping_entry), NVM_META_REG);
		    if(!me){
		        printk("os chunk alloc error at %s\n",__func__);
		    }
                    me->vaddr = vaddr>>PAGE_SHIFT;
                    me->nvaddr = (u64)nvaddr>>PAGE_SHIFT;
                    me->access_flags = access_flags;
                    list_add_tail(&me->list, &cs->nv_mappings);
		    clflush_multiline((u64)nvaddr, PAGE_SIZE);
		    clflush_multiline((u64)me, sizeof(struct mapping_entry));
		    clflush_multiline((u64)&cs->nv_mappings, sizeof(struct list_head));
		}
		vaddr += PAGE_SIZE;
	    }
	}
	vm = vm->vm_next;
    }
}*/

/*This ensures the context regs are saved and restored*/
/*
void create_log_user_regs(struct exec_context *ctx){
    struct saved_state *cs = get_saved_state(ctx);
    struct log_entry* log = (struct log_entry*)os_chunk_alloc(sizeof(struct log_entry),NVM_META_REG);
    if(!log){
        printk("chunk alloc error at %s\n",__func__);
    }
    log->event = CTX_CHANGE;
    log->address.index = 0;
    log->address.offset = offsetof(struct exec_context, regs);
    log->payload = (void*)os_chunk_alloc(sizeof(struct user_regs),NVM_META_REG);
    if(!log->payload){
        printk("os chunk alloc error at %s\n",__func__);
    }
    memcpy((char*)log->payload, (char*)&ctx->regs, sizeof(struct user_regs));
    log->size = sizeof(struct user_regs);
    list_add_tail(&log->list, &cs->log);
    clflush_multiline((u64)log->payload, sizeof(struct user_regs));
    clflush_multiline((u64)log, sizeof(struct log_entry));
    clflush_multiline((u64)&cs->log, sizeof(struct list_head));
}

void print_nv_mappings(struct exec_context *ctx){
    struct saved_state *cs = get_saved_state(ctx);
    struct mapping_entry * me;
    struct list_head* temp;
    list_for_each(temp,&cs->nv_mappings){
        me = list_entry(temp, struct mapping_entry, list);
        printk("vaddr:%x, nvaddr:%x, access_flags:%x\n",me->vaddr,me->nvaddr,me->access_flags);
    }
}

void save_os_stack(struct exec_context *ctx){
    dprintk("start with save os stack\n");
    struct saved_state *ss = get_saved_state(ctx);
    struct exec_context * latest_ctx = ss->context[ss->latest];
    memcpy((char*)(((u64)latest_ctx->os_stack_pfn)<<PAGE_SHIFT),\
		    (char*)(((u64)ctx->os_stack_pfn)<<PAGE_SHIFT),PAGE_SIZE);
    if(ctx->os_rsp < (u64)(((u64)ctx->os_stack_pfn)<<PAGE_SHIFT)){
        printk("ctx os rsp issue at %s\n",__func__);
    } 
    u64 offset = ctx->os_rsp-(u64)(((u64)ctx->os_stack_pfn)<<PAGE_SHIFT);
    latest_ctx->os_rsp = (u64)(((u64)latest_ctx->os_stack_pfn)<<PAGE_SHIFT)+offset;
    clflush_multiline((u64)(((u64)latest_ctx->os_stack_pfn)<<PAGE_SHIFT), PAGE_SIZE);
    clflush_multiline((u64)&latest_ctx->os_rsp, sizeof(u64));
    dprintk("done with save os stack\n");
}

void dump_stack(struct exec_context *ctx){
    u64* pte;
    struct mm_segment *seg = &ctx->mms[MM_SEG_STACK];
    struct saved_state * ss = get_saved_state(ctx);
    struct list_head* temp_nv_stack_head;
    struct list_head* next_nv_stack_head;
    struct stack_entry * se_temp;
    temp_nv_stack_head = (ss->nv_stack).next;
    next_nv_stack_head = temp_nv_stack_head;
    while( next_nv_stack_head && next_nv_stack_head != &ss->nv_stack ){
	next_nv_stack_head = temp_nv_stack_head->next;
	se_temp = list_entry(temp_nv_stack_head, struct stack_entry, list);
	os_chunk_free(NVM_META_REG, (u64)se_temp,sizeof(struct stack_entry));
	temp_nv_stack_head = next_nv_stack_head;
    }
    init_list_head(&ss->nv_stack);
    clflush_multiline((u64)&ss->nv_stack, sizeof(struct list_head));

    for(u64 vaddr = seg->end-PAGE_SIZE; vaddr >= seg->next_free; vaddr -= PAGE_SIZE) {
        pte = get_user_pte(ctx, vaddr, 0);
            if(pte) {
		dprintk("dump stack %x\n",vaddr);
		struct mapping_entry * me = get_nv_mapping(ss, vaddr);
                if(me){
                    //printk("Adding stack_entry log\n");
                    struct stack_entry * se = (struct stack_entry*)os_chunk_alloc(
				    sizeof(struct stack_entry),NVM_META_REG);
		    if(!se){
		        printk("os chunk alloc failed at %s\n",__func__);
		    }
                    se->addr = (u64)osmap(me->nvaddr);
                    se->payload.page = os_page_alloc(NVM_USER_REG);
                    memcpy((char*)se->payload.page,(char*)((vaddr>>PAGE_SHIFT)<<PAGE_SHIFT),PAGE_SIZE);
                    se->size = PAGE_SIZE;
                    dprintk("stack entry addr:%x at %s\n",&se->list,__func__);
		    list_add_tail(&se->list, &ss->nv_stack);
                    clflush_multiline((u64)se->payload.page, PAGE_SIZE);
                    clflush_multiline((u64)se, sizeof(struct stack_entry));
                    clflush_multiline((u64)&ss->nv_stack, sizeof(struct list_head));
		}
		else{
		    printk("issue, missing stack mapping in dump at %s\n",__func__);
		}
	    }
    }
}
*/

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
int end_checkpoint(struct exec_context *ctx) {
    //dprintk("end checkpoint\n");
    checkpoint_num += 1;

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
    /*for(int i=0; i<256; i++){
        struct bitmap_data * dtemp = (struct bitmap_data *)((unsigned long)btd+(i*sizeof(struct bitmap_data)*4));
        printk("i:%u\n",i);
        while(dtemp){
            printk("loc:%u,stream:%u, addr:%lx, next:%lx\t",dtemp->pos,dtemp->stream,(unsigned long)dtemp,(unsigned long)dtemp->next);
            dtemp = dtemp->next;
        }
        printk("\n");
    }*/
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
			//printk("val:%u\n",value_8);
			//printk("Addr:%lx\n",addr);
	                size = (1UL<<LOG_TRACK_GRAN)*dtemp->stream;
	                //int gran_8bytes = (size%8)?(size>>3)+1:size>>3;
                        se->addr[temp_log_index] = addr;
	                se->size[temp_log_index] = size;
			//printk("index:%u,addr:%lx,size:%u\n",temp_log_index,se->addr[temp_log_index],se->size[temp_log_index]);
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
    RDTSC_STOP();
    rdtsc_value_1 = elapsed(start_hi,start_lo,end_hi,end_lo);
    //printk("log_count:%u,nvm stack:%lx\n",log_index,ctx->nvm_stack);
    unsigned total_log = log_index;
    for(log_index=0; log_index<total_log; log_index++){
        se = (struct stack_entry*)((u64)se_base+(log_entry_size*log_index)); //base+log_index*log_size
        u32 running_size = 0;
	for(int i=0;i<4;i++){
            if(se->size[i]){
                unsigned offset = stack_end-se->addr[i];
		u64 dest_nvm_addr = ctx->nvm_stack+offset;
		u64 src_nvm_addr = (u64)se->payload.byte+running_size;
                //printk("addr:%lx,offset:%u,size:%u\n",se->addr[i],offset,se->size[i]);
		memcpy((char*)dest_nvm_addr,(char*)src_nvm_addr,se->size[i]);
                //clflush_multiline_new((u64)dest_nvm_addr,se->size[i]);
		running_size += se->size[i];
	    }
	}
    }
    asm volatile("sfence":::"memory");
    //u64 rdtsc_value_1 = 0;
    //u64 rdtsc_value_2 = 0;
    //u32 bytes_copied = 0;
    //u32 actual_size = 0;
    struct checkpoint_stats* chst = (struct checkpoint_stats*)os_chunk_alloc(
		    sizeof(struct checkpoint_stats),NVM_META_REG);
    chst->num = checkpoint_num;
    chst->time_to_copy = rdtsc_value_1;
    chst->bytes_copy = bytes_copied;
    list_add_tail(&chst->list,checkpoint_stat_list);
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
        printk("checkpoint:%u, time:%lu, size:%u \n", chst->num, chst->time_to_copy, chst->bytes_copy);
        temp = temp->next;
    }
    return 0;
}

int  merge_pages(){
    printk("merge called\n");
    return 0;
}
/*
void copy_mm_setup_pt(struct saved_state * ss, struct exec_context * ctx){
    void *os_addr;
    u64 vaddr;
    u32 upfn; 
    struct mm_segment *seg;
    ctx->pgd = os_pfn_alloc(OS_PT_REG);
    os_addr = osmap(ctx->pgd);
    bzero((char *)os_addr, PAGE_SIZE);
   
   //CODE segment
    seg = &ctx->mms[MM_SEG_CODE];
    for(vaddr = seg->start; vaddr <= seg->next_free; vaddr += PAGE_SIZE){
        struct mapping_entry * me = get_nv_mapping(ss, vaddr);
        if(!me){
            printk("SEG_CODE, Missing VPFN to NVMPFN mapping!!!!\n");
        }
	else{
            upfn = map_physical_page((u64) os_addr, vaddr, me->access_flags, me->nvaddr, 1);
	}
    }
    seg = &ctx->mms[MM_SEG_RODATA];
    for(vaddr = seg->start; vaddr <= seg->next_free; vaddr += PAGE_SIZE){
        struct mapping_entry * me = get_nv_mapping(ss, vaddr);
        if(!me){
            printk("SEG_RODATA, Missing VPFN to NVMPFN mapping!!!!\n");
        }
	else{
            upfn = map_physical_page((u64) os_addr, vaddr, me->access_flags, me->nvaddr, 1);
	}
    }
    seg = &ctx->mms[MM_SEG_DATA];
    for(vaddr = seg->start; vaddr <= seg->next_free; vaddr += PAGE_SIZE){
        struct mapping_entry * me = get_nv_mapping(ss, vaddr);
        if(!me){
            printk("SEG_DATA Missing VPFN to NVMPFN mapping!!!!\n");
        }
	else{
            upfn = map_physical_page((u64) os_addr, vaddr, me->access_flags, me->nvaddr, 1);
	}
    } 
    seg = &ctx->mms[MM_SEG_STACK];
    for(vaddr = seg->end - PAGE_SIZE; vaddr >= seg->next_free; vaddr -= PAGE_SIZE){
        struct mapping_entry * me = get_nv_mapping(ss, vaddr);
        if(!me){
            printk("SEG STACK Missing VPFN [%x] to NVMPFN mapping!!!!\n",vaddr);
        }
	else{
            upfn = map_physical_page((u64) os_addr, vaddr, me->access_flags, 0, 0);
	    printk("mapping stack [%x]\n",vaddr);
	    void* paddr = osmap(upfn);
	    void* nvaddr = osmap(me->nvaddr);
	    memcpy(paddr, nvaddr, PAGE_SIZE);
	}
    }

    struct vm_area * vm = ctx->vm_area;
    while(vm){
        if(vm->is_nvm){
            for(vaddr = vm->vm_start; vaddr < vm->vm_end; vaddr += PAGE_SIZE){
                struct mapping_entry * me = get_nv_mapping(ss, vaddr);
                if(!me){
                    printk("NVM VMA [%x] Missing VPFN to NVMPFN mapping!!!!\n",vaddr);
		}
		else{
                    upfn = map_physical_page((u64) os_addr, vaddr, me->access_flags, me->nvaddr, 1);
		}
	    }
	}
	else{
	    for(vaddr = vm->vm_start; vaddr < vm->vm_end; vaddr += PAGE_SIZE){
                struct mapping_entry * me = get_nv_mapping(ss, vaddr);
                if(!me){
                    printk(" VMA [%x] Missing VPFN to NVMPFN mapping!!!!\n",vaddr);
		}
		else{
                    upfn = map_physical_page((u64) os_addr, vaddr, me->access_flags, 0, 0);
                    void* paddr = osmap(upfn);
                    void* nvaddr = osmap(me->nvaddr);
                    memcpy(paddr, nvaddr, PAGE_SIZE);
		}
	    }
	}
        vm = vm->vm_next;
    }
}
*/

/* if saved state is MAPPING, then mapping update is in progress and
 * also the stack read dirtmap operation might not have completed,
 * so recover from previous checkpointed state
 * thus we ignore all changes happened in current checkpoint interval.
 * if saved state state=MODIFYING, then it means, the changes to
 * segment (except stack) virtual address are recorded in nv_mappings
 * and the stack change dirty bitmap is read and nv_stack log entries
 * are made, so if recovery happens at this point take the consistent
 * execution state from saved state (pointed to by latest field)
 * and reapply the logged changes.
 * if the state is UPDATED, then all changes are applied and use the
 * execution context pointed to by latest field. 
 * */
/*
int resume_persistent_processes(){
//restore the saved context
//reconstruct the page table
//reset volatile execution context states 
//before restoring the saved context
    //reset_exec_ctx();
    printk("resume persistent processes called\n");
    struct saved_state *ss;
    struct list_head* temp;
    unsigned count = 0;
    struct exec_context * new_ctx;
    list_for_each(temp,saved_state_list){
        ss = list_entry(temp, struct saved_state, list);
        new_ctx = get_ctx_by_pid(ss->pid);
        printk("resuming process:%u\n",count);
	if(ss->state == INVALID){
	   printk("no valid saved state\n");
	}
	else{
	if(ss->state == MODIFYING){
            apply_logged_changes(ss);
	    ss->latest = (ss->latest+1)%2;
	    ss->state = UPDATED;
            clflush_multiline((u64)ss, sizeof(struct saved_state));
        }
	else if(ss->state == INITIAL){
            printk("state is INITIAL\n");	
	}
	else if(ss->state == MAPPING){
            printk("state is MAPPING\n");	
	}
	else if(ss->state == UPDATED){
            printk("state is UPDATED\n");	
	}
        struct exec_context * latest_ctx = ss->context[ss->latest]; 
        copy_ctx(new_ctx,latest_ctx,0);
        copy_mm_setup_pt(ss, new_ctx);
	new_ctx->os_stack_pfn = os_pfn_alloc(OS_PT_REG);
        memcpy((char*)(((u64)new_ctx->os_stack_pfn)<<PAGE_SHIFT),\
		       	(char*)(((u64)latest_ctx->os_stack_pfn)<<PAGE_SHIFT), PAGE_SIZE);
        u64 offset = latest_ctx->os_rsp-(u64)(((u64)latest_ctx->os_stack_pfn)<<PAGE_SHIFT);
        new_ctx->os_rsp = (u64)(((u64) new_ctx->os_stack_pfn) << PAGE_SHIFT) + offset;

	new_ctx->state = READY;
	new_ctx->files[0] = create_standard_IO(STDIN);
        new_ctx->files[1] = create_standard_IO(STDOUT);
        new_ctx->files[2] = create_standard_IO(STDERR);
        u64 pl4 = new_ctx->pgd;
        install_os_pts_range(new_ctx, REGION_DRAM_START, (REGION_DRAM_END-REGION_DRAM_START));
        printk("resume process dram done\n");
        install_os_pts_range(new_ctx, REGION_NVM_START, (REGION_NVM_ENDMEM-REGION_NVM_START));
        printk("resume process nvm done\n");
        install_apic_mapping((u64) pl4);
        count += 1;
	}
	schedule(new_ctx);
    }
    return count;
}*/
