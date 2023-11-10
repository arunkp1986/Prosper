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
#if 0
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
}
/*only STACK is maintained in DRAM, all other segments
 * are maintained in NVM, so only VPFN->NVMPFN needs to
 * be saved for these segments.*/
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
/*The vma can be allotted in NVM or DRAM
 * if vma is in NVM, then make an entry for VPFN->NVMPFN in mapping table
 * if vma is in DRAM, then allote a page in NVM and make an entry
 * at the time of end checkpoint, copy changes in DRAM vma area to NVM page
 * nothing needs to be done for NVM vma as pages are already in NVM */

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
}

/*Apply changes recorded in nv_stack log to stack pages in NVM*/
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
	        memcpy((char*)se->addr,(char*)se->payload.page,se->size);
	    }
	    else{
	        memcpy((char*)se->addr,(char*)se->payload.byte,se->size);
	    }
            //clflush_multiline((u64)se->addr, se->size);
        //printk(" temp: %x next: %x at %s\n",temp,temp->next,__func__);
	}
	temp = temp->next;
    }
    dprintk("done apply nv stack\n");
}


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
struct list_head *checkpoint_stat_list;
int start_checkpoint(struct exec_context *ctx) {
    dprintk("start checkpoint\n");
    track_on = 1;
    struct saved_state *cs;
    struct exec_context * saved_ctx;
    struct exec_context * update_ctx;
        //x86_dump_gem5_stats();
    //x86_reset_gem5_stats();

    if(!(ctx->persistent & FIRST_TIME)){
        ctx->persistent |= FIRST_TIME;
	checkpoint_stat_list = (struct list_head*)os_chunk_alloc(sizeof(struct list_head),NVM_META_REG);
	init_list_head(checkpoint_stat_list);
        cs = create_saved_state(ctx);
        cs->latest = 0;
        saved_ctx = cs->context[0];
        update_ctx = cs->context[1];
	copy_ctx(saved_ctx,ctx,1);
	//copy_segment_into_nvm(ctx,MM_SEG_CODE);
	//copy_segment_into_nvm(ctx,MM_SEG_RODATA);
	//copy_segment_into_nvm(ctx,MM_SEG_DATA);
	copy_segment_into_nvm(ctx,MM_SEG_STACK);
	//copy_vma_into_nvm(ctx);
	saved_ctx->os_stack_pfn = os_pfn_alloc(NVM_USER_REG);
	update_ctx->os_stack_pfn = os_pfn_alloc(NVM_USER_REG);
	memcpy((char*)(((u64)saved_ctx->os_stack_pfn)<<PAGE_SHIFT),\
		       	(char*)(((u64)ctx->os_stack_pfn)<<PAGE_SHIFT), PAGE_SIZE);
        u64 offset = ctx->os_rsp-(u64)(((u64)ctx->os_stack_pfn)<<PAGE_SHIFT);
        saved_ctx->os_rsp = (u64)(((u64) saved_ctx->os_stack_pfn) << PAGE_SHIFT) + offset; 
        cs->state = INITIAL;
        clflush_multiline((u64)(((u64)saved_ctx->os_stack_pfn)<<PAGE_SHIFT), PAGE_SIZE);
        clflush_multiline((u64)&saved_ctx->os_stack_pfn, sizeof(u64));
        clflush_multiline((u64)&saved_ctx->os_rsp, sizeof(u64));
        clflush_multiline((u64)cs, sizeof(struct saved_state));
	if(TRACK_BYTE_GRAN){
	    /* byte granularity tracking*/
            dprintk("This is byte granularity checkpointing, granularity:%u\n",TRACK_SIZE);
            start_track(ctx,TRACK_SIZE);
	}
        else{
            /*page based dirty tracking*
	     * disable hardware comparator by setting TRACK MSR to 1*/
            dprintk("This is page granularity checkpointing\n");
            writemsr(MISCREG_LOG_TRACK_GRAN, (u64)0);
            clear_dirtybit(ctx, ctx->mms[MM_SEG_STACK].next_free, ctx->mms[MM_SEG_STACK].end);
	}
    }else{
        cs = get_saved_state(ctx);
        cs->state = INITIAL;
        saved_ctx = cs->context[cs->latest];
        struct log_entry * lg;
        struct list_head* temp;
        struct list_head* next;
	memcpy((char*)(((u64)saved_ctx->os_stack_pfn)<<PAGE_SHIFT),\
		       	(char*)(((u64)ctx->os_stack_pfn)<<PAGE_SHIFT), PAGE_SIZE);
        u64 offset = ctx->os_rsp-(u64)(((u64)ctx->os_stack_pfn)<<PAGE_SHIFT);
        saved_ctx->os_rsp = (u64)(((u64) saved_ctx->os_stack_pfn) << PAGE_SHIFT) + offset; 
        clflush_multiline((u64)(((u64)saved_ctx->os_stack_pfn)<<PAGE_SHIFT), PAGE_SIZE);
        clflush_multiline((u64)&saved_ctx->os_stack_pfn, sizeof(u64));
        clflush_multiline((u64)&saved_ctx->os_rsp, sizeof(u64));
	/*dprintk("going to remove log_entry\n");
	temp = (cs->log).next;
	next = temp;
	while( next && next != &cs->log ){
	    next = temp->next;
	    lg = list_entry(temp, struct log_entry, list); 
	    dprintk("size:%u\n",lg->size);
	    os_chunk_free(NVM_META_REG, (u64)lg,sizeof(struct log_entry));
	    temp = next;
	}
	init_list_head(&cs->log);*/
        //clflush_multiline((u64)&cs->log, sizeof(struct list_head));
        /*
	dprintk("going to remove nv stack\n");
	struct stack_entry * se;
	temp = (cs->nv_stack).next;
	next = temp;
	while( next && next != &cs->nv_stack ){
            dprintk("inside loop\n");
	    next = temp->next;
	    se = list_entry(temp, struct stack_entry, list);
	    se->addr = 0;
            dprintk("size:%u\n",se->size);
	    //os_chunk_free(NVM_META_REG, (u64)se,sizeof(struct stack_entry));
	    temp = next;
	}*/
	//init_list_head(&cs->nv_stack);
        //clflush_multiline((u64)&cs->nv_stack, sizeof(struct list_head));
        clflush_multiline((u64)cs, sizeof(struct saved_state));

        if(TRACK_BYTE_GRAN){
	    /*byte granularity tracking, set MSR values and setup tracking* */
	    dprintk("starting byte granularity checkpoint, granularity:%u\n",TRACK_SIZE);
            start_track(ctx,TRACK_SIZE);
        }
        else{
            /*page based dirty tracking so clear the page dirty bits.* */
	    dprintk("starting page granularity checkpoint\n");
            writemsr(MISCREG_LOG_TRACK_GRAN, (u64)0);
            //clear_dirtybit(ctx, ctx->mms[MM_SEG_STACK].next_free, ctx->mms[MM_SEG_STACK].end);
	}
    }

    ctx->persistent |= MAKE_PERSISTENT;
  // print_saved_checkpoint_state();
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
/**/

/*At end checkpoint we need to update nv_mappings
 * this will reflect the VA changes to segments.
 * for STACK we will allot a NVM page, but data copy 
 * to this NVM page happens based on dirty tracking.
 * */
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
		clflush_multiline((u64)me, sizeof(struct mapping_entry));
		clflush_multiline((u64)&me->list, sizeof(struct list_head));
		clflush_multiline((u64)&cs->nv_mappings, sizeof(struct list_head));

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
}

/*we need to update nv_mappings for vma allocations and copy data to
 * NVM for vma allotted from DRAM as we are not dirty tracking
 * for heap allocation. for vma allotted from NVM, only make entry in
 * nv_mappings*/

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
}

/*This ensures the context regs are saved and restored*/
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
int end_checkpoint(struct exec_context *ctx) {
    dprintk("end checkpoint\n");
    if(TRACK_BYTE_GRAN){
	/* byte granularity tracking*/
        extern u32* ptr_dirtymap;
        dprintk("going to read dirty bitmap, granularity:%u\n",TRACK_SIZE);
        writemsr(MISCREG_LOG_TRACK_GRAN, (u64)0);
	if(!ptr_dirtymap){
	    printk("issue with ptr_dirtymap at %s\n",__func__);
	}
	 /*This load is to invoke the hardware comparator flush*/
	 u32 temp = *(u32*)(ptr_dirtymap);
	 temp = *(u32*)(ptr_dirtymap);
	 temp = *(u32*)(ptr_dirtymap);
	unsigned log_write_done = readmsr(MISCREG_TRACK_SYNC);
        unsigned check_counter = 0;
        dprintk("end log write done:%u\n",log_write_done);
        while(!log_write_done){
            check_counter += 1;
            if(check_counter==10){
                printk("log write not done in :%u\n",check_counter);
                break;
            }
            /*This load is to invoke the hardware comparator flush*/
            u32 temp = *(u32*)(ptr_dirtymap);
            log_write_done = readmsr(MISCREG_TRACK_SYNC);
            dprintk("end log write done:%u\n",log_write_done);
        }
        writemsr(MISCREG_TRACK_SYNC, (u64)0); //setting 0 to clear sync
    }
    unsigned start_hi = 0, start_lo = 0, end_hi = 0, end_lo = 0;
    u64 rdtsc_value_1 = 0;
    u64 rdtsc_value_2 = 0;
    static u32 checkpoint_num = 0;
    u32 bytes_copied;
    ctx->persistent &= ~MAKE_PERSISTENT;
    struct saved_state *ss = get_saved_state(ctx);
    ss->state = MAPPING;
    dprintk("going to update code mapping\n");
    //update_segment_nv_mappings(ctx,MM_SEG_CODE);
    dprintk("going to update rodata mapping\n");
    //update_segment_nv_mappings(ctx,MM_SEG_RODATA);
    dprintk("going to update data mapping\n");
    //update_segment_nv_mappings(ctx,MM_SEG_DATA);
    dprintk("going to update stack mapping\n");
    //update_segment_nv_mappings(ctx,MM_SEG_STACK);
    dprintk("going to update vma mapping\n");
    //update_vma_nv_mappings(ctx);
    //create_log_user_regs(ctx);
    dprintk("going to read dirtybit\n");
    /*
    struct stack_entry * se_temp;
    struct list_head* temp_nv_stack_head;
    struct list_head* nv_stack_head;
    //struct saved_state* ss = get_saved_state(ctx);

    nv_stack_head = &ss->nv_stack;
    temp_nv_stack_head = nv_stack_head->next;
    while( temp_nv_stack_head && temp_nv_stack_head != nv_stack_head ){
        se_temp = list_entry(temp_nv_stack_head, struct stack_entry, list);
        //os_chunk_free(NVM_META_REG, (u64)se_temp, sizeof(struct stack_entry));
	se_temp->addr = 0;
        temp_nv_stack_head = temp_nv_stack_head->next;
    }*/
    //init_list_head(&ss->nv_stack);
    //clflush_multiline((u64)&ss->nv_stack, sizeof(struct list_head));

    if(TRACK_BYTE_GRAN){
	/* byte granularity tracking*/
        dprintk("going to read dirty bitmap, granularity:%u\n",TRACK_SIZE);
        //writemsr(MISCREG_LOG_TRACK_GRAN, (u64)1);
	//struct checkpoint_stats* chst = (struct checkpoint_stats*)os_chunk_alloc(sizeof(struct checkpoint_stats),NVM_META_REG);
	//RDTSC_START();
	alloc_se_dirty_bitmap(ctx, (ctx->regs).entry_rsp,ctx->mms[MM_SEG_STACK].end, TRACK_SIZE);
	//RDTSC_STOP();
	//rdtsc_value_1 = elapsed(start_hi,start_lo,end_hi,end_lo);
        //start_hi = 0, start_lo = 0, end_hi = 0, end_lo = 0;
	//RDTSC_START();
	bytes_copied = read_dirty_bitmap(ctx, (ctx->regs).entry_rsp, 
			ctx->mms[MM_SEG_STACK].end, TRACK_SIZE);
	//RDTSC_STOP();
	//rdtsc_value_2 = elapsed(start_hi,start_lo,end_hi,end_lo);
	/*chst->num = checkpoint_num;
	chst->time_to_log = rdtsc_value_1;
	chst->time_to_copy = rdtsc_value_2;
        chst->bytes_copy = bytes_copied;
	dprintk("chst->list:%x\n",&chst->list);
        list_add_tail(&chst->list,checkpoint_stat_list);*/
	dprintk("rdtsc cycles taken to copy:%u\n",rdtsc_value);
    }
    else{
        /*page based dirty tracking**/
        dprintk("going to read dirty bit in pagetable\n");
	struct checkpoint_stats* chst = (struct checkpoint_stats*)os_chunk_alloc(sizeof(struct checkpoint_stats),NVM_META_REG);
	RDTSC_START();
        alloc_se_dirtybit(ctx, (ctx->regs).entry_rsp, ctx->mms[MM_SEG_STACK].end);
	RDTSC_STOP();
	rdtsc_value_1 = elapsed(start_hi,start_lo,end_hi,end_lo);
        start_hi = 0, start_lo = 0, end_hi = 0, end_lo = 0;
	RDTSC_START();
        bytes_copied = read_dirtybit(ctx, (ctx->regs).entry_rsp, ctx->mms[MM_SEG_STACK].end);
	RDTSC_STOP();
	rdtsc_value_2 = elapsed(start_hi,start_lo,end_hi,end_lo);
	chst->num = checkpoint_num;
	chst->time_to_log = rdtsc_value_1;
	chst->time_to_copy = rdtsc_value_2;
        chst->bytes_copy = bytes_copied;
        list_add_tail(&chst->list,checkpoint_stat_list);	
	dprintk("rdtsc cycles taken to copy:%u\n",rdtsc_value);
    }

    checkpoint_num += 1;
    //dump_stack(ctx);
    //save_os_stack(ctx);
    ss->state = MODIFYING;
    dprintk("going to apply log\n");
    //apply_logged_changes(ss); 
    dprintk("updating latest in saved context\n");
    ss->latest = (ss->latest+1)%2;
    //save_os_stack(ctx);
    ss->state = UPDATED;

    //x86_dump_gem5_stats();
    //x86_reset_gem5_stats();
    return 0;
}

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
}
#endif
