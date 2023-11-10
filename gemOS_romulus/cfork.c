#include <cfork.h>
#include <page.h>
#include<mmap.h>

static inline void invlpg(unsigned long addr) {
    asm volatile("invlpg (%0)" ::"r" (addr) : "memory");
}


void copy_vm_area_ptable(struct exec_context *child, struct exec_context *parent, struct vm_area * vm_area){
    void * os_addr;
    u64 vaddr;
    u32 ppfn;
    struct pfn_info * p;
    u32 access_flags = vm_area->access_flags;
    os_addr = osmap(child->pgd);
    for(vaddr = vm_area->vm_start; vaddr < vm_area->vm_end; vaddr += PAGE_SIZE){
        u64 * paddr = get_user_pte(parent,vaddr,0);
        //invlpg(vaddr);
        if(paddr){
            u64 addr = *paddr;
            ppfn = (u32)(((*paddr) & FLAG_MASK) >> PAGE_SHIFT);
            p = get_pfn_info(ppfn);
            increment_pfn_info_refcount(p);
            map_physical_page((u64)os_addr, vaddr, access_flags, (u32)(((*paddr) & FLAG_MASK) >> PAGE_SHIFT), 0);
            u64 * caddr = get_user_pte(child,vaddr,0);
            *paddr &= 0xfffffffffffffffd;
            *caddr &= 0xfffffffffffffffd;
        }
    }
    return;
}

void cfork_copy_mm(struct exec_context *child, struct exec_context *parent ){

    child->pgd = os_pfn_alloc(OS_PT_REG);
    u64 vaddr;
    void * os_addr;
    struct mm_segment *seg;
    struct pfn_info * p;
    u64 pfn;
    u32 ppfn;

    os_addr = osmap(child->pgd);
    bzero((char *)os_addr, PAGE_SIZE);

    seg = &parent->mms[MM_SEG_CODE];
    for(vaddr = seg->start; vaddr < seg->next_free; vaddr += PAGE_SIZE){
        u64 * paddr = get_user_pte(parent,vaddr,0);
        if(paddr)
            install_ptable((u64)os_addr,seg,vaddr,(u32)(((*paddr) & FLAG_MASK) >> PAGE_SHIFT),0);
    }
    
    seg = &parent->mms[MM_SEG_RODATA];
    for(vaddr = seg->start; vaddr < seg->next_free; vaddr += PAGE_SIZE){
        u64 * paddr = get_user_pte(parent,vaddr,0);
        if(paddr)
            install_ptable((u64)os_addr,seg,vaddr,(u32)(((*paddr) & FLAG_MASK) >> PAGE_SHIFT),0);
    }
    
    seg = &parent->mms[MM_SEG_DATA];
    for(vaddr = seg->start; vaddr < seg->next_free; vaddr += PAGE_SIZE){
        u64 * paddr = get_user_pte(parent,vaddr,0);
        invlpg(vaddr);
        if(paddr){
            u64 addr = *paddr;
            ppfn = (u32)(((*paddr) & FLAG_MASK) >> PAGE_SHIFT);
            p = get_pfn_info(ppfn);
            increment_pfn_info_refcount(p);
            install_ptable((u64)os_addr,seg,vaddr,(u32)(((*paddr) & FLAG_MASK) >> PAGE_SHIFT),0);
            u64 * caddr = get_user_pte(child,vaddr,0);
            *paddr &= 0xfffffffffffffffd;
            *caddr &= 0xfffffffffffffffd;
        }
    }
    seg = &parent->mms[MM_SEG_STACK];
    for(vaddr = seg->end - PAGE_SIZE; vaddr >= seg->next_free; vaddr -= PAGE_SIZE){
      u64 *paddr =  get_user_pte(parent, vaddr, 0);
      
     if(paddr){
         u64 pfn = install_ptable((u64)os_addr, seg, vaddr, 0, 0);  //Returns the blank page in dram 
         pfn = (u64)osmap(pfn);
         memcpy((char *)pfn, (char *)(*paddr & FLAG_MASK), PAGE_SIZE); 
      }
  }

    unsigned long vm_start, vm_end;
    u32 access_flags;
    struct vm_area * vm_area;
    struct vm_area * parent_vm_area = parent->vm_area;
    struct vm_area * child_vm_area = child->vm_area;

    while(parent_vm_area){
           vm_start = parent_vm_area->vm_start;
           vm_end = parent_vm_area->vm_end;
           access_flags = parent_vm_area->access_flags;
           vm_area = create_vm_area(vm_start, vm_end, access_flags,0);
           child_vm_area = vm_area;
           copy_vm_area_ptable(child, parent, parent_vm_area);
           child_vm_area = child_vm_area->vm_next;
           parent_vm_area = parent_vm_area->vm_next;
           
        } 
    copy_os_pts(parent->pgd, child->pgd);
    return;
    
}


void vfork_copy_mm(struct exec_context *child, struct exec_context *parent ){
    child->pgd = os_pfn_alloc(OS_PT_REG);
    u64 vaddr;
    void * os_addr;
    struct mm_segment *seg;
    struct pfn_info * p;
    u64 pfn;
    u32 ppfn;

    os_addr = osmap(child->pgd);
    bzero((char *)os_addr, PAGE_SIZE);

    void * os_p_addr;

    os_p_addr = osmap(parent->pgd);

    memcpy((char*)os_addr,(char*)os_p_addr, PAGE_SIZE);
    return;
    
}

int handle_cow_fault(struct exec_context *current, u64 cr2){

  void * os_addr;
  struct mm_segment * seg;
  
  u64 * currpte = get_user_pte(current, cr2,0);
  u32 ppfn = (u32)(((*currpte) & FLAG_MASK) >> PAGE_SHIFT);
  struct pfn_info * p = get_pfn_info(ppfn);
  u8 ref_count = get_pfn_info_refcount(p);
  os_addr = osmap(current->pgd);
  struct vm_area * vma =  get_vm_area(current, cr2);

  if((cr2 >= (current -> mms)[MM_SEG_DATA].start) && (cr2 <= (current -> mms)[MM_SEG_DATA].end)){
      dprintk("inside handle cow fault\n");
      dprintk("refcount:%u\n",ref_count);
      u64 *current_pte =  get_user_pte(current, cr2, 0);
      if(ref_count > 1){
      seg = &current->mms[MM_SEG_DATA];
      if(current_pte){
        u64 pfn = install_ptable((u64)os_addr, seg, cr2, 0, 0);  //Returns the blank page in dram
        pfn = (u64)osmap(pfn);
        memcpy((char *)pfn, (char *)(*current_pte & FLAG_MASK), PAGE_SIZE);
        decrement_pfn_info_refcount(p);
        }
      }
      else{
        dprintk("Inside handle cow fault else\n");
        *current_pte = *current_pte | 0x2;
      } 

      }
     if(cr2 >= MMAP_AREA_START && cr2 <= MMAP_AREA_END)
    {
      if(!(vma->access_flags & MM_WR)){
          printk("sig error\n");
          return -1;
      }
      
      u64 *current_pte =  get_user_pte(current, cr2, 0);
      if(ref_count > 1){
      u32 access_flags = vma ? vma->access_flags : MM_WR; //check vm area access_flags allow writing
      if(current_pte){
        u64 pfn = map_physical_page((u64)os_addr, cr2, access_flags, 0, 0);  //Returns the blank page  
        pfn = (u64)osmap(pfn);
        memcpy((char *)pfn, (char *)(*current_pte & FLAG_MASK), PAGE_SIZE);
        decrement_pfn_info_refcount(p);
        }
      }
      else{
        dprintk("Inside handle cow fault else\n");
        *current_pte = *current_pte | 0x2;
      } 
    }
    return 1;
}

void vfork_exit_handle(struct exec_context *ctx){
  u32 ppid = ctx->ppid;
  dprintk("pid:%u ppid:%u\n",ctx->pid,ppid);
  struct exec_context *parent_ctx = get_ctx_by_pid(ppid);
  dprintk("child_state:%d parent_state:%d\n",ctx->state,parent_ctx->state);
  if(parent_ctx->state == WAITING){
     parent_ctx->state = READY;
  }
  return;
}
