#include <types.h>
#include <mmap.h>
#include <nonvolatile.h>
#include <list.h>
#include <lib.h>

int get_num_pages(int length)
{
    int num_pages = (length / PAGE_SIZE);

    if(length % PAGE_SIZE != 0)
    {
        ++num_pages;
    }
    return num_pages;
}

// Function to get the end address of vm_area given the start address and the length
u64 get_end_addr(u64 start_addr, int length)
{
    return (((start_addr >> PTE_SHIFT)+ get_num_pages(length)) << PTE_SHIFT);
}

// Function to create a new vm_area
struct vm_area* create_vm_area(u64 start_addr, u64 end_addr, u32 flags, u8 is_nvm)
{
    struct vm_area *new_vm_area = alloc_vm_area();
    new_vm_area-> vm_start = start_addr;
    new_vm_area-> vm_end = end_addr;
    new_vm_area-> access_flags = flags;
    new_vm_area->is_nvm = is_nvm;
    return new_vm_area;
}

u32 get_vma_index(struct exec_context* ctx, struct vm_area *vma){
    u32 index = 0;
    u8 found = 0;
    struct vm_area *vm = ctx->vm_area;
    while(vm){
        if(vm == vma){
            found = 1;
            break;
	}
	index += 1;
	vm = vm->vm_next;
    }
    if(!found){
        dprintk("Bug, should not miss, %x,%x\n",ctx->vm_area,vma);
    }
    return index;
}

// Function to create and merge vm areas.
/*If the process is a persistent process, vma, context 
 * changes are logged in the saved context*/
u64 map_vm_area(struct vm_area* vm, u64 start_addr, int length, int prot, u8 is_nvm)
{
    u64 addr = -1;
    u32 index = 0;
    struct exec_context* current = get_current_ctx();
    struct saved_state* cs = get_saved_state(current);
    // Merging the requested region with the existing vm_area (END)
    if(vm && vm -> access_flags == prot)
    {
        addr = vm->vm_end;
        vm->vm_end = get_end_addr(vm->vm_end, length);
	/*log entry*/
        dprintk("creating VMA_CHANGE log0 at %s\n",__func__);
	struct log_entry* log0 = (struct log_entry*)os_chunk_alloc(sizeof(struct log_entry),NVM_META_REG);
	if(!log0){
	    printk("os chunk alloc failed at %s\n",__func__);
	}
	log0->event = VMA_CHANGE;
	log0->address.index = get_vma_index(current,vm);
	log0->address.offset = offsetof(struct vm_area, vm_end);
	log0->payload = (void*)os_chunk_alloc(sizeof(unsigned long),NVM_META_REG);
	memcpy((char*)log0->payload, (char*)&vm->vm_end, sizeof(unsigned long));
	log0->size = sizeof(unsigned long);
	list_add_tail(&log0->list, &cs->log);
	clflush_multiline((u64)log0->payload, sizeof(unsigned long));
	clflush_multiline((u64)log0, sizeof(struct log_entry));
        clflush_multiline((u64)&cs->log, sizeof(struct list_head));

        struct vm_area *next = vm -> vm_next;

        // If End address is same as next vm_area. Then expand the current vm_area and delete the other one. 
        if(next && vm->vm_end == next ->vm_start && vm->access_flags == next->access_flags)
        {  /*log entry, create before node is unlinked*/
	    if(current->persistent){
                if(!cs){
		   printk("Error, no saved state present\n");
		}
	        dprintk("creating VMA_CHANGE log1 at %s\n",__func__);
	        struct log_entry* log1 = (struct log_entry*)os_chunk_alloc(sizeof(struct log_entry),NVM_META_REG);
		if(!log1){
		    printk("os chunk alloc failed at %s\n",__func__);
		}
	        log1->event = VMA_CHANGE;
	        log1->address.index = get_vma_index(current,vm);
	        log1->address.offset = offsetof(struct vm_area, vm_end);
	        log1->payload = (void*)os_chunk_alloc(sizeof(unsigned long),NVM_META_REG);
		if(!log1->payload){
		    printk("os chunk alloc failed at %s\n",__func__);
		}
		memcpy((char*)log1->payload, (char*)&vm->vm_end, sizeof(unsigned long));
	        log1->size = sizeof(unsigned long);
	        list_add_tail(&log1->list, &cs->log);
                clflush_multiline((u64)log1->payload, sizeof(unsigned long));
                clflush_multiline((u64)log1, sizeof(struct log_entry));
                clflush_multiline((u64)&cs->log, sizeof(struct list_head));

	        dprintk("creating VMA_REMOVE log2 at %s\n",__func__);
	        struct log_entry* log2 = (struct log_entry*)os_chunk_alloc(sizeof(struct log_entry),NVM_META_REG);
		if(!log2){
                    printk("os chunk alloc failed at %s\n",__func__);
		}
	        log2->event = VMA_REMOVE;
	        log2->address.index = get_vma_index(current,next);
	        log2->address.offset = 0;
	        log2->payload = NULL;
	        log2->size = 0;
	        list_add_tail(&log2->list, &cs->log);
                clflush_multiline((u64)log2, sizeof(struct log_entry));
                clflush_multiline((u64)&cs->log, sizeof(struct list_head));
	    }
            vm->vm_end = next->vm_end;
            vm->vm_next = next->vm_next;
	    dealloc_vm_area(next);
        }
    } else if(vm->vm_next && vm->vm_next->access_flags == prot) 
    {
        // Merging the requested region with existing vm_area (Front)
        struct vm_area *next = vm -> vm_next;
        next->vm_start = start_addr;
	if(current->persistent){
	    dprintk("creating VMA_CHANGE log3 at map_vm_area\n");
	    struct log_entry* log3 = (struct log_entry*)os_chunk_alloc(sizeof(struct log_entry),NVM_META_REG);
	    if(!log3){
	        printk("os chunk alloc failed at %s\n",__func__);
	    }
	    log3->event = VMA_CHANGE;
	    log3->address.index = get_vma_index(current,next);
	    log3->address.offset = offsetof(struct vm_area, vm_start);
	    log3->payload = (void*)os_chunk_alloc(sizeof(unsigned long),NVM_META_REG);
	    if(!log3->payload){
	        printk("os chunk alloc failed at %s\n",__func__);
	    }
	    memcpy((char*)log3->payload, (char*)&next->vm_start, sizeof(unsigned long));
	    log3->size = sizeof(unsigned long);
	    list_add_tail(&log3->list, &cs->log);
	    clflush_multiline((u64)log3->payload, sizeof(unsigned long));
            clflush_multiline((u64)log3, sizeof(struct log_entry));
            clflush_multiline((u64)&cs->log, sizeof(struct list_head));
	}
        addr = start_addr;
    } else
    {
        // Creating a new vm_area with requested access permission
       
        struct vm_area *new_vm_area = create_vm_area(start_addr, get_end_addr(start_addr, length), prot,is_nvm);
        if(vm->vm_next)
        {
            new_vm_area ->vm_next = vm->vm_next;
        }
        vm->vm_next = new_vm_area;
        if(current->persistent){ 
	    dprintk("creating VMA_ADD log4 at map_vm_area\n");
	    struct log_entry* log4 = (struct log_entry*)os_chunk_alloc(sizeof(struct log_entry),NVM_META_REG);
	    if(!log4){
                printk("os chunk alloc failed at %s\n",__func__);
	    }
	    log4->event = VMA_ADD;
	    log4->address.index = get_vma_index(current,vm);
	    log4->address.offset = 0;
	    log4->payload = (void*)os_chunk_alloc(sizeof(struct vm_area),NVM_META_REG);
	    if(!log4->payload){
	        printk("os chunk alloc failed at %s\n",__func__);
	    }
	    memcpy((char*)log4->payload,(char*)new_vm_area, sizeof(struct vm_area));
	    log4->size = sizeof(struct vm_area);
	    list_add_tail(&log4->list, &cs->log); 
	    clflush_multiline((u64)log4->payload, sizeof(struct vm_area));
            clflush_multiline((u64)log4, sizeof(struct log_entry));
            clflush_multiline((u64)&cs->log, sizeof(struct list_head));
	}
        addr = new_vm_area->vm_start;
    }
   return addr;
}

// Function to handle the hint address and MAP_FIXED flags
long look_up_hint_addr(struct vm_area* vm, u64 addr, int length, int prot, int flags, u8 is_nvm)
{
    long ret_addr = 0;
    struct exec_context* current = get_current_ctx();
    struct saved_state* cs = get_saved_state(current);
    while(vm)
    {
        // Requested Region is already mapped
        if(addr >= vm->vm_start && addr < vm->vm_end)
        {
            break;
        } else
        {
            // Creating a new area Region
            u64 start_page = (vm->vm_end);
            u64 end_page = vm->vm_next ? vm->vm_next->vm_start : MMAP_AREA_END;

            if(addr >= start_page && addr < end_page)
            {
                int available_pages = (end_page >> PAGE_SHIFT) - (addr >> PAGE_SHIFT);
                int required_pages = get_num_pages(length);
                // printk("vm[%x -> %x]addr [%x] available[%d] requested[%d] \n",vm->vm_start, vm->vm_end, addr, available_pages, required_pages);
                if(available_pages >= required_pages)
                {
                    u64 end_address = get_end_addr(addr, length);
                    struct vm_area * vm_next = vm->vm_next;
                    
                    if(vm->vm_end == addr && vm->access_flags == prot)
                    {   
                        vm->vm_end = end_address;
			/*log entry*/
			if(current->persistent){
			    dprintk("creating VMA_CHANGE log1 at look_up_hint_addr\n");
	                    struct log_entry* log1 =\
			        (struct log_entry*)os_chunk_alloc(sizeof(struct log_entry),NVM_META_REG);
			        if(!log1){
		                    printk("os chunk alloc failed at %s\n",__func__);
				}
	                        log1->event = VMA_CHANGE;
	                        log1->address.index = get_vma_index(current,vm);
	                        log1->address.offset = offsetof(struct vm_area, vm_end);
	                        log1->payload = (void*)os_chunk_alloc(sizeof(unsigned long),NVM_META_REG);
		                memcpy((char*)log1->payload, (char*)&vm->vm_end, sizeof(unsigned long));
	                        log1->size = sizeof(unsigned long);
	                        list_add_tail(&log1->list, &cs->log);
				clflush_multiline((u64)log1->payload, sizeof(unsigned long));
                                clflush_multiline((u64)log1, sizeof(struct log_entry));
                                clflush_multiline((u64)&cs->log, sizeof(struct list_head));
			}

                        if(vm_next && vm->vm_end == vm_next->vm_start && vm_next->access_flags == prot)
                        {
			 /*log entry, create before node is unlinked*/
			    if(current->persistent){
	                        dprintk("creating VMA_CHANGE log2 at %s\n",__func__);
	                        struct log_entry* log2 =\
				    (struct log_entry*)os_chunk_alloc(sizeof(struct log_entry),NVM_META_REG);
				if(!log2){
		                    printk("os chunk alloc failed at %s\n",__func__);
				}
	                        log2->event = VMA_CHANGE;
	                        log2->address.index = get_vma_index(current,vm);
	                        log2->address.offset = offsetof(struct vm_area, vm_end);
	                        log2->payload = (void*)os_chunk_alloc(sizeof(unsigned long),NVM_META_REG);
				if(!log2->payload){
		                    printk("os chunk alloc failed at %s\n",__func__);
				}
		                memcpy((char*)log2->payload,(char*)&vm->vm_end, sizeof(unsigned long));
	                        log2->size = sizeof(unsigned long);
	                        list_add_tail(&log2->list, &cs->log);
				clflush_multiline((u64)log2->payload, sizeof(unsigned long));
                                clflush_multiline((u64)log2, sizeof(struct log_entry));
                                clflush_multiline((u64)&cs->log, sizeof(struct list_head));

	                        dprintk("creating VMA_REMOVE log3 at %s\n",__func__);
                                struct log_entry* log3 =\
				     (struct log_entry*)os_chunk_alloc(sizeof(struct log_entry),NVM_META_REG);
				if(!log3){	
		                    printk("os chunk alloc failed at %s\n",__func__);
				}
	                        log3->event = VMA_REMOVE;
	                        log3->address.index = get_vma_index(current,vm_next);
	                        log3->address.offset = 0;
	                        log3->payload = NULL;
	                        log3->size = 0;
	                        list_add_tail(&log3->list, &cs->log);
                                clflush_multiline((u64)log3, sizeof(struct log_entry));
                                clflush_multiline((u64)&cs->log, sizeof(struct list_head));
			    }
                            vm->vm_end = vm_next->vm_end;
                            vm->vm_next = vm_next->vm_next;
			    dealloc_vm_area(vm_next);
                        }
                    } else if(vm_next && vm_next->vm_start == end_address && vm_next->access_flags == prot)
                    {
                        vm_next->vm_start = addr;
			/*log entry*/
			if(current->persistent){
	                    dprintk("creating VMA_CHANGE log4 at look_up_hint_addr\n");
	                    struct log_entry* log4 =\
			         (struct log_entry*)os_chunk_alloc(sizeof(struct log_entry),NVM_META_REG);
			    if(!log4){
                                printk("os chunk alloc failed at %s\n",__func__);
			    }
	                    log4->event = VMA_CHANGE;
	                    log4->address.index = get_vma_index(current,vm_next);
	                    log4->address.offset = offsetof(struct vm_area, vm_start);
	                    log4->payload = (void*)os_chunk_alloc(sizeof(unsigned long),NVM_META_REG);
			    if(!log4->payload){
			        printk("os chunk alloc failed at %s\n",__func__);
			    }
	                    memcpy((char*)log4->payload,(char*)&vm_next->vm_start, sizeof(unsigned long));
	                    log4->size = sizeof(unsigned long);
	                    list_add_tail(&log4->list, &cs->log);
			    clflush_multiline((u64)log4->payload, sizeof(unsigned long));
                            clflush_multiline((u64)log4, sizeof(struct log_entry));
                            clflush_multiline((u64)&cs->log, sizeof(struct list_head));
			}
                    } else
                    {
                        struct vm_area *new_vm_area = create_vm_area(addr, get_end_addr(addr, length), prot,is_nvm);
                        if(vm)
                        {
                            if(vm->vm_next)
                            {
                                new_vm_area ->vm_next = vm->vm_next;
                            }
                            vm->vm_next = new_vm_area;
			}
			/*log entry*/
			if(current->persistent){ 
	                    dprintk("creating VMA_ADD log5 at look_up_hint_addr\n");
	                    struct log_entry* log5 =\
			         (struct log_entry*)os_chunk_alloc(sizeof(struct log_entry),NVM_META_REG);
			    if(!log5){
			        printk("os chunk alloc failed at %s\n",__func__);
			    }
	                    log5->event = VMA_ADD;
	                    log5->address.index = get_vma_index(current,vm);
	                    log5->address.offset = 0;
	                    log5->payload = (void*)os_chunk_alloc(sizeof(struct vm_area),NVM_META_REG);
			    if(!log5->payload){
			        printk("os chunk alloc failed at %s\n",__func__);
			    }
	                    memcpy((char*)log5->payload,(char*)new_vm_area, sizeof(struct vm_area));
	                    log5->size = sizeof(struct vm_area);
	                    list_add_tail(&log5->list, &cs->log);
			    clflush_multiline((u64)log5->payload, sizeof(struct vm_area));
                            clflush_multiline((u64)log5, sizeof(struct log_entry));
                            clflush_multiline((u64)&cs->log, sizeof(struct list_head));
			}
		    }
                    ret_addr = addr;
                }

                break;
            }
        }
        vm = vm -> vm_next;
    }

    // If ret_addr is zero and MAP_FIXED is not set. Then we have to look for a new region 
    // Which statisfies the request. Address wont be considered as hint.
    if(ret_addr <= 0 && (flags & MAP_FIXED))
    {
        ret_addr = -1;
    }
    return ret_addr;
}

// Funtion to handle the MAP_POPULATE Flags. Mapping the physical pages with vm_area
void vm_map_populate(u64 pgd, u64 addr, u32 prot, u32 page_count, u8 is_nvm)
{
    u64 base_addr = (u64) osmap(pgd);
    u32 access_flags = (prot & (PROT_WRITE)) ? MM_WR : 0;
    u64 virtual_addr = addr;
    while(page_count > 0)
    {
     if(is_nvm){
            map_physical_page(pgd, virtual_addr, access_flags, 0, 1);
	}
	else{
            map_physical_page(pgd, virtual_addr, access_flags, 0, 0);
	}
        virtual_addr += PAGE_SIZE;
        --page_count;
    }
}

/**
 * Function will invoked whenever there is page fault. (Lazy allocation)
 * 
 * For valid acess. Map the physical page 
 * Return 0
 * 
 * For invalid access, i.e Access which is not matching the vm_area access rights (Writing on ReadOnly pages)
 * Return -1. 
 */
int vm_area_pagefault(struct exec_context *current, u64 addr, int error_code)
{
    int fault_fixed = -1;
    struct vm_area *vm = current->vm_area;
    while(vm)
    {
        if (addr >= vm->vm_start && addr < vm->vm_end)
        {
            // Checking the vm_area access flags and error_code of the page fault.
            if((error_code & MM_WR) && !(vm->access_flags & (MM_WR|MM_EX)))
            {
                break;
            }
            // Write fault-> 06 || Readfault -> 0x4
            int access_flags = (vm->access_flags & MM_WR) ? 0x6 : 0x4;
            if(vm->is_nvm){
                install_page_table(current, addr, access_flags, 1);
	    }else{
                install_page_table(current, addr, access_flags, 0);
	    }
            fault_fixed = 1;
            break;
        }
        vm = vm->vm_next;
    }
    return fault_fixed;
}

/**
 * mprotect System call Implementation.
 */
int vm_area_mprotect(struct exec_context *current, u64 addr, int length, int prot)
{
    u64 end_addr = get_end_addr(addr, length);
    int isValid = 0;
    struct vm_area *vm = current -> vm_area;

    while(!isValid && vm)
    {
        if(vm ->vm_start <= addr && end_addr <= vm->vm_end)
            isValid = 1;
        else
            vm = vm->vm_next;
    }

    if(isValid)
    {
        int flag = -1;
        u64 *pte_entry = get_user_pte(current, addr, 0);
        flag = (!pte_entry) ? 0 : MAP_POPULATE;

        vm_area_unmap(current, addr, length);
        vm_area_map(current, addr, length, prot, flag);
        isValid = 0;
    } else
    {
        isValid = -1;
    }
    
    return isValid;
}
/**
 * mmap system call implementation.
 */
long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags)
{

    long ret_addr = -1;
    // Checking the hint address ranges
    if((addr && !( addr >= MMAP_AREA_START && addr < MMAP_AREA_END))|| length <=0 )
    {
        return ret_addr;
    }
    u8 is_nvm = 0;
    // MAP_NVM flag handlers
    if(flags & MAP_NVM){
        is_nvm = 1;
    }

    struct vm_area *vm = current->vm_area;
    int required_pages = get_num_pages(length);
    // Allocating one page dummy vm_area
    if(!vm)
    {        
        vm = create_vm_area(MMAP_AREA_START, get_end_addr(MMAP_AREA_START, PAGE_SIZE), PROT_READ|PROT_EXEC,0);
        current->vm_area = vm;
	/*log entry*/
	if(current->persistent){
	    dprintk("creating VMA_ADD log at vm_area_map\n");
	    struct saved_state* cs = get_saved_state(current);
	    struct log_entry* log = (struct log_entry*)os_chunk_alloc(sizeof(struct log_entry),NVM_META_REG);
	    if(!log){
	        printk("os chunk alloc failed at %s\n",__func__);
	    }
	    log->event = VMA_ADD;
	    log->address.index = get_vma_index(current,vm);
	    log->address.offset = 0;
	    log->payload = (struct vm_area*)os_chunk_alloc(sizeof(struct vm_area),NVM_META_REG);
	    if(!log->payload){
                printk("os chunk alloc failed at %s\n",__func__);
	    }
	    memcpy((char*)log->payload, (char*)vm, sizeof(struct vm_area));
	    log->size = sizeof(struct vm_area);
	    list_add_tail(&log->list, &cs->log);
	    clflush_multiline((u64)log->payload, sizeof(struct vm_area));
            clflush_multiline((u64)log, sizeof(struct log_entry));
            clflush_multiline((u64)&cs->log, sizeof(struct list_head));
	}
    }

    // Hint address handling 
    if(addr || (flags & MAP_FIXED))
    {
       ret_addr = look_up_hint_addr(vm, addr, length, prot, flags, is_nvm);
       if(ret_addr > 0 || ret_addr == -1)
       {
           if(ret_addr > 0 && (flags & MAP_POPULATE))
           {
                u64 base_addr = (u64) osmap(current->pgd);
                if(flags & MAP_NVM){
                    vm_map_populate(base_addr, ret_addr, prot, required_pages, 1);
		}else{
                    vm_map_populate(base_addr, ret_addr, prot, required_pages, 0);
		}
           }
           return ret_addr;
       }
           
    }

    int do_created = 0;
   

    // Traversing the linked list of vm_areas
    while(!do_created && vm)
    {
        u64 start_page = (vm->vm_end) >> PAGE_SHIFT;
        u64 end_page;

        if(vm -> vm_next)
            end_page  = (vm->vm_next->vm_start) >> PAGE_SHIFT;
        else
            end_page = MMAP_AREA_END >> PAGE_SHIFT;

        // Available Free pages serves the requested. 
        // then either create a new vm_area or merge with existing one
        if((end_page - start_page) >= required_pages)
        {
            ret_addr = map_vm_area(vm, vm->vm_end, length, prot, is_nvm);
            do_created = 1;
        } else
        {
            vm = vm -> vm_next;
        }
    }
    // MAP_POPULATE flag handlers
    // printk("Before MAP POPULATE [%x] [%d] [%d]\n", ret_addr, flags, MAP_POPULATE);
    if(ret_addr > 0 && (flags & MAP_POPULATE))
    {
        u64 base_addr = (u64) osmap(current->pgd);
        if(flags & MAP_NVM){
	    dprintk("NVM mmap\n");
            vm_map_populate(base_addr, ret_addr, prot, required_pages, 1);
	}else{
	    dprintk("DRAM mmap\n");
            vm_map_populate(base_addr, ret_addr, prot, required_pages, 0);
	}
    }
    
    return ret_addr;
}
/**
 * munmap system call implemenations
 */

int vm_area_unmap(struct exec_context *current, u64 addr, int length)
{
    int isValid = 0;
    struct saved_state* cs = get_saved_state(current);

    // Address should be page aligned
    if(addr % PAGE_SIZE != 0)
    {
        isValid = -1;
        return isValid;
    }

    int num_pages_deallocate = get_num_pages(length);
    int deallocated = 0;
    struct vm_area *vm = current->vm_area;
    struct vm_area *vm_prev = NULL;

    // Traversing the vm_area linked list
    while(!deallocated && vm)
    {
        if(addr >= vm->vm_start && addr < vm->vm_end)
        {
            u64 end_addr = get_end_addr(addr, length);
            // Region to be deallcated is at start
            if(addr == vm->vm_start)
            {
                vm->vm_start = end_addr;
		/*log entry*/
		if(current->persistent){
	            dprintk("creating VMA_CHANGE log1 at vm_area_unmap\n");
	            struct log_entry* log1 =\
                        (struct log_entry*)os_chunk_alloc(sizeof(struct log_entry),NVM_META_REG);
		    if(!log1){
		        printk("os chunk alloc failed at %s\n",__func__);
		    }
                    log1->event = VMA_CHANGE;
                    log1->address.index = get_vma_index(current,vm);
                    log1->address.offset = offsetof(struct vm_area, vm_start);
                    log1->payload = (void*)os_chunk_alloc(sizeof(unsigned long),NVM_META_REG);
		    if(!log1->payload){
		        printk("os chunk alloc failed at %s\n",__func__);
		    }
                    memcpy((char*)log1->payload, (char*)&vm->vm_start, sizeof(unsigned long));
	            log1->size = sizeof(unsigned long);
	            list_add_tail(&log1->list, &cs->log);
		    clflush_multiline((u64)log1->payload, sizeof(unsigned long));
                    clflush_multiline((u64)log1, sizeof(struct log_entry));
                    clflush_multiline((u64)&cs->log, sizeof(struct list_head));
		}

                // Entire Region is deallocated
                if(vm->vm_start == vm->vm_end)
                {
		    if(vm_prev){
		    /*log entry, create before node is unlinked*/
		        if(current->persistent){
	                    dprintk("creating VMA_REMOVE log2 at %s\n",__func__);
                            struct log_entry* log2 =\
				(struct log_entry*)os_chunk_alloc(sizeof(struct log_entry),NVM_META_REG);
			    if(!log2){	    
		                printk("os chunk alloc failed at %s\n",__func__);
			    }
                            log2->event = VMA_REMOVE;
	                    log2->address.index = get_vma_index(current,vm);
	                    log2->address.offset = 0;
	                    log2->payload = NULL;
	                    log2->size = 0;
	                    list_add_tail(&log2->list, &cs->log);
                            clflush_multiline((u64)log2, sizeof(struct log_entry));
                            clflush_multiline((u64)&cs->log, sizeof(struct list_head));
			}
			vm_prev->vm_next = vm->vm_next;
		    }
                    else{
                         current->vm_area = NULL;
			 /*log entry*/
			 if(current->persistent){
	                    dprintk("creating CTX_CHANGE log3 at %s\n",__func__);
	                    struct log_entry* log3 =\
                                (struct log_entry*)os_chunk_alloc(sizeof(struct log_entry),NVM_META_REG);
			    if(!log3){
			        printk("os chunk alloc failed at %s\n",__func__);
			    }
                            log3->event = CTX_CHANGE;
                            log3->address.index = 0;
                            log3->address.offset = offsetof(struct exec_context, vm_area);
                            log3->payload = (void*)os_chunk_alloc(sizeof(struct vm_area *),NVM_META_REG);
                            memcpy((char*)log3->payload, (char*)&current->vm_area, sizeof(struct vm_area *));
	                    log3->size = sizeof(struct vm_area *);
	                    list_add_tail(&log3->list, &cs->log);
			    clflush_multiline((u64)log3->payload, sizeof(unsigned long));
                            clflush_multiline((u64)log3, sizeof(struct log_entry));
                            clflush_multiline((u64)&cs->log, sizeof(struct list_head));
			}
		    }
		    dealloc_vm_area(vm);
                }
            } else if(end_addr == vm->vm_end) // Region to be deallocated is toward the end
            {
                vm->vm_end = addr;
		/*log entry*/
                if(current->persistent){
	            dprintk("creating VMA_CHANGE log4 at vm_area_unmap\n");
	            struct log_entry* log4 =\
                        (struct log_entry*)os_chunk_alloc(sizeof(struct log_entry),NVM_META_REG);
		    if(!log4){
		        printk("os chunk alloc failed at %s\n",__func__);
		    }
                    log4->event = VMA_CHANGE;
                    log4->address.index = get_vma_index(current,vm);
                    log4->address.offset = offsetof(struct vm_area, vm_end);
                    log4->payload = (void*)os_chunk_alloc(sizeof(unsigned long),NVM_META_REG);
                    memcpy((char*)log4->payload, (char*)&vm->vm_end, sizeof(unsigned long));
	            log4->size = sizeof(unsigned long);
	            list_add_tail(&log4->list, &cs->log);
		    clflush_multiline((u64)log4->payload, sizeof(unsigned long));
                    clflush_multiline((u64)log4, sizeof(struct log_entry));
                    clflush_multiline((u64)&cs->log, sizeof(struct list_head));
		}
            } else
            {
                // If the region is inside the existing region. 
                // Then spiliting the region and creating a new vm_area

                struct vm_area *new_vm_area = create_vm_area(end_addr, vm->vm_end, vm->access_flags,vm->is_nvm);
                new_vm_area->vm_next = vm->vm_next;
                vm->vm_next = new_vm_area;
                vm->vm_end = addr;
		/*log entry*/
                if(current->persistent){ 
	            dprintk("creating VMA_ADD log5 at %s\n",__func__);
	            struct log_entry* log5 =\
			(struct log_entry*)os_chunk_alloc(sizeof(struct log_entry),NVM_META_REG);
		    if(!log5){
		        printk("os chunk alloc failed at %s\n",__func__);
		    }
	            log5->event = VMA_ADD;
	            log5->address.index = get_vma_index(current,vm);
	            log5->address.offset = 0;
	            log5->payload = (void*)os_chunk_alloc(sizeof(struct vm_area),NVM_META_REG);
	            memcpy((char*)log5->payload, (char*)new_vm_area, sizeof(struct vm_area));
	            log5->size = sizeof(struct vm_area);
	            list_add_tail(&log5->list, &cs->log);
		    clflush_multiline((u64)log5->payload, sizeof(struct vm_area));
                    clflush_multiline((u64)log5, sizeof(struct log_entry));
                    clflush_multiline((u64)&cs->log, sizeof(struct list_head));

	            dprintk("creating VMA_CHANGE log6 at vm_area_unmap\n");
	            struct log_entry* log6 =\
			(struct log_entry*)os_chunk_alloc(sizeof(struct log_entry),NVM_META_REG);
		    if(!log6){
		        printk("os chunk alloc failed at %s\n",__func__);
		    }
	            log6->event = VMA_CHANGE;
	            log6->address.index = get_vma_index(current,vm);
	            log6->address.offset = offsetof(struct vm_area, vm_end);
	            log6->payload = (void*)os_chunk_alloc(sizeof(unsigned long),NVM_META_REG);
	            memcpy((char*)log6->payload, (char*)&vm->vm_end, sizeof(struct vm_area));
	            log6->size = sizeof(struct vm_area);
	            list_add_tail(&log6->list, &cs->log);
		    clflush_multiline((u64)log6->payload, sizeof(struct vm_area));
                    clflush_multiline((u64)log6, sizeof(struct log_entry));
                    clflush_multiline((u64)&cs->log, sizeof(struct list_head));
		}
	    }
            deallocated = 1;
        } else
        {
            vm_prev = vm;
            vm = vm->vm_next;

        }

    }

    if(deallocated)
    {
        deallocated = 0;
        while(deallocated < num_pages_deallocate)
        {
            unsigned long addr_dealloc = addr + (PAGE_SIZE * deallocated);
            invalidate_pte(current, addr_dealloc);
	    struct mapping_entry *me = get_nv_mapping(cs,addr_dealloc);
	    if(me)
	        //list_del(&me->list);
            deallocated++;
        }
    }
    return isValid;
}
