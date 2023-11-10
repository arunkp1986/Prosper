#include<types.h>
#include<mmap.h>



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
struct vm_area* create_vm_area(u64 start_addr, u64 end_addr, u32 flags)
{
    struct vm_area *new_vm_area = alloc_vm_area();
    new_vm_area-> vm_start = start_addr;
    new_vm_area-> vm_end = end_addr;
    new_vm_area-> access_flags = flags;
    return new_vm_area;
}

// Function to create and merge vm areas.
u64 map_vm_area(struct vm_area* vm, u64 start_addr, int length, int prot)
{
    u64 addr = -1;

    // Merging the requested region with the existing vm_area (END)
    if(vm && vm -> access_flags == prot)
    {
        addr = vm->vm_end;
        vm->vm_end = get_end_addr(vm->vm_end, length);

        struct vm_area *next = vm -> vm_next;

        // If End address is same as next vm_area. Then expand the current vm_area and delete the other one. 
        if(next && vm->vm_end == next ->vm_start && vm->access_flags == next->access_flags)
        {
            vm->vm_end = next->vm_end;
            vm->vm_next = next->vm_next;
            dealloc_vm_area(next);
        }
    } else if(vm->vm_next && vm->vm_next->access_flags == prot) 
    {
        // Merging the requested region with existing vm_area (Front)
        struct vm_area *next = vm -> vm_next;
        next->vm_start = start_addr;
        addr = start_addr;
    } else
    {
        // Creating a new vm_area with requested access permission
       
        struct vm_area *new_vm_area = create_vm_area(start_addr, get_end_addr(start_addr, length), prot);

        if(vm->vm_next)
        {
            new_vm_area ->vm_next = vm->vm_next;
        }
        vm->vm_next = new_vm_area;
        
        addr = new_vm_area->vm_start;
    }
   return addr;
}

// Function to handle the hint address and MAP_FIXED flags
long look_up_hint_addr(struct vm_area* vm, u64 addr, int length, int prot, int flags)
{
    long ret_addr = 0;
    while(vm)
    {
        // Requested Region is already mapped
        if(addr >= vm ->vm_start && addr < vm->vm_end)
        {
            break;
        } else
        {
            // Creating a new area Region
            u64 start_page = (vm->vm_end);
            u64 end_page = vm -> vm_next ? vm->vm_next->vm_start : MMAP_AREA_END;

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
                        if(vm_next && vm->vm_end == vm_next->vm_start && vm_next->access_flags == prot)
                        {
                            vm->vm_end = vm_next->vm_end;
                            vm->vm_next = vm_next->vm_next;
                             dealloc_vm_area(vm_next);
                        }

                    } else if(vm_next && vm_next->vm_start == end_address && vm_next->access_flags == prot)
                    {
                        vm_next->vm_start = addr;
                    } else
                    {
                        struct vm_area *new_vm_area = create_vm_area(addr, get_end_addr(addr, length), prot);
                        if(vm)
                        {
                            if(vm->vm_next)
                            {
                                new_vm_area ->vm_next = vm->vm_next;
                            }
                            vm->vm_next = new_vm_area;
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
void vm_map_populate(u64 pgd, u64 addr, u32 prot, u32 page_count)
{
    u64 base_addr = (u64) osmap(pgd);
    u32 access_flags = (prot & (PROT_WRITE)) ? MM_WR : 0;
    u64 virtual_addr = addr;
    while(page_count > 0)
    {
        map_physical_page(pgd, virtual_addr, access_flags, 0);
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
            install_page_table(current, addr, access_flags);
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

    struct vm_area *vm = current->vm_area;
    int required_pages = get_num_pages(length);
    // Allocating one page dummy vm_area
    if(!vm)
    {        
        vm = create_vm_area(MMAP_AREA_START, get_end_addr(MMAP_AREA_START, PAGE_SIZE), PROT_READ|PROT_EXEC);
        current->vm_area = vm;
    }

    // Hint address handling 
    if(addr || (flags & MAP_FIXED))
    {
       ret_addr = look_up_hint_addr(vm, addr, length, prot, flags);
       if(ret_addr > 0 || ret_addr == -1)
       {
           if(ret_addr > 0 && (flags & MAP_POPULATE))
           {
                u64 base_addr = (u64) osmap(current->pgd);
                vm_map_populate(base_addr, ret_addr, prot, required_pages);
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
            
            ret_addr = map_vm_area(vm, vm->vm_end, length, prot);
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
        vm_map_populate(base_addr, ret_addr, prot, required_pages);
    }
    return ret_addr;
}
/**
 * munmap system call implemenations
 */

int vm_area_unmap(struct exec_context *current, u64 addr, int length)
{
    int isValid = 0;
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
                // Entire Region is deallocated
                if(vm->vm_start == vm->vm_end)
                {
                    if(vm_prev)
                        vm_prev->vm_next = vm->vm_next;
                    else
                         current->vm_area = NULL;
                    dealloc_vm_area(vm);
                }
            } else if(end_addr == vm->vm_end) // Region to be deallocated is toward the end
            {
                vm->vm_end = addr;
            } else
            {
                // If the region is inside the existing region. 
                // Then spiliting the region and creating a new vm_area

                struct vm_area *new_vm_area = create_vm_area(end_addr, vm->vm_end, vm->access_flags);
                new_vm_area->vm_next = vm->vm_next;
                vm->vm_next = new_vm_area;
                vm->vm_end = addr;
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
            deallocated++;
        }
    }
    return isValid;
}
