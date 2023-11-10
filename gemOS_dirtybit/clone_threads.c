#include<clone_threads.h>
#include<entry.h>
#include<context.h>
#include<memory.h>
#include<lib.h>
#include<mmap.h>

extern void install_page_table(struct exec_context *ctx, u64 addr, u64 error_code, u8 is_nvm);

static u64* getpte(struct exec_context *ctx, u64 addr, int dump) 
{
    u64 *vaddr_base = (u64 *)osmap(ctx->pgd);
    u64 *entry;
    u32 phy_addr;
    
    entry = vaddr_base + ((addr & PGD_MASK) >> PGD_SHIFT);
    phy_addr = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
    vaddr_base = (u64 *)osmap(phy_addr);
  
    /* Address should be mapped as un-priviledged in PGD*/
    if( (*entry & 0x1) == 0 || (*entry & 0x4) == 0)
        goto out;
    if(dump)
            printk("L4: Entry = %x NextLevel = %x FLAGS = %x\n", (*entry), phy_addr, (*entry) & (~FLAG_MASK)); 

     entry = vaddr_base + ((addr & PUD_MASK) >> PUD_SHIFT);
     phy_addr = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
     vaddr_base = (u64 *)osmap(phy_addr);

      if( (*entry & 0x1) == 0 || (*entry & 0x4) == 0)
          goto out;

     if(dump)
            printk("L3: Entry = %x NextLevel = %x FLAGS = %x\n", (*entry), phy_addr, (*entry) & (~FLAG_MASK)); 

      entry = vaddr_base + ((addr & PMD_MASK) >> PMD_SHIFT);
      phy_addr = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
      vaddr_base = (u64 *)osmap(phy_addr);
      
      /* 
        Address should be mapped as un-priviledged in PMD 
         Huge page mapping not allowed
      */
      if( (*entry & 0x1) == 0 || (*entry & 0x4) == 0 || (*entry & 0x80) == 1)
          goto out;

      if(dump)
            printk("L2: Entry = %x NextLevel = %x FLAGS = %x\n", (*entry), phy_addr, (*entry) & (~FLAG_MASK)); 
     
      entry = vaddr_base + ((addr & PTE_MASK) >> PTE_SHIFT);
      
      if(dump){
            phy_addr = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
            printk("L1: Entry = %x PFN = %x FLAGS = %x\n", (*entry), phy_addr, (*entry) & (~FLAG_MASK)); 
      }
     return entry;

out:
      return NULL;
}
/*
  system call handler for clone, create thread like 
  execution contexts
*/
long do_clone(void *th_func, void *user_stack, void *user_arg) 
{
  int ctr;
  struct exec_context *new_ctx = get_new_ctx();
  struct exec_context *ctx = get_current_ctx();
  u32 pid = new_ctx->pid;
  struct thread *n_thread;
  
  if(ctx->type != EXEC_CTX_USER){
        new_ctx->state = UNUSED;
        return -EINVAL;
  }

  *new_ctx = *ctx;
  if(!ctx->ctx_threads){  // This is the first thread
          ctx->ctx_threads = os_alloc(sizeof(struct ctx_thread_info),OS_DS_REG);
          bzero((char *)ctx->ctx_threads, sizeof(struct ctx_thread_info));
          ctx->ctx_threads->pid = ctx->pid;
  }
     
  n_thread = find_unused_thread(ctx);
  if(!n_thread){
         printk("Max threads reached\n");
         new_ctx->state = UNUSED;
         return -EAGAIN;
  }
  n_thread->pid = pid;
  n_thread->status = TH_USED;
  n_thread->parent_ctx = ctx;

  new_ctx->ppid = ctx->pid;
  new_ctx->pid = pid;
  new_ctx->type = EXEC_CTX_USER_TH;
  // allocate page for os stack in kernel part of process's VAS
  setup_child_context(new_ctx);

  new_ctx->regs.entry_rip = (u64) th_func;
  new_ctx->regs.entry_rsp = (u64) user_stack;
  new_ctx->regs.rbp = new_ctx->regs.entry_rsp;

  //The first argument to the thread is the third argument here
  //First arg (user arg) is put to RDI

  new_ctx->regs.rdi = (u64)user_arg;

  sprintk(new_ctx->name, "thr-%d", new_ctx->pid); 

  dprintk("User stack is at %x fp = %x argument = %x\n", user_stack, th_func, user_arg);
  //Return the pid of newly created thread
  dprintk("ctx %x created %x\n", ctx, new_ctx);
 
  new_ctx->ctx_threads = NULL;	  // Thread can not have threads
  //new line by nikhil
  new_ctx->state = WAITING;
	
  return pid;

}


static struct thread_private_map *findit(struct thread *th, u64 address)
{
 int ctr;
 struct thread_private_map *thmap = &th->private_mappings[0];
 for(ctr = 0; ctr < MAX_PRIVATE_AREAS; ++ctr, thmap++){
      if(thmap->owner && thmap->start_addr <= address && (address < (thmap->start_addr + thmap->length)))
               return thmap;
 } 
 return NULL;
}

static struct thread *find_thread_thmap(struct exec_context *ctx, u64 address, struct thread_private_map **thmap)
{
   int i;
   struct thread *th;
   struct thread_private_map *map = NULL;
   
   struct ctx_thread_info *tinfo = ctx->ctx_threads;

   if(!isProcess(ctx) || !tinfo)
          return NULL;
   
   for(i=0; i<MAX_THREADS; ++i){
        th = &tinfo->threads[i];
        if(th->status == TH_USED && (map = findit(th, address))){
               *thmap = map;
               return th;        
        }
   } 
   return NULL; 
}

int handle_thread_private_fault(struct exec_context *current, u64 addr, int error_code)
{
  struct thread *th;
  struct exec_context *parent;
  struct thread_private_map *thmap = NULL;
  u32 pte_flags = 0x5;  //present
 
  if(isProcess(current)){
      printk("BUG!! parent page fault is here!\n");
      goto segfault_exit;
  }        
  //printk("pid %d faulted on %x\n", current->pid, (addr >> 12) << 12);
  parent = get_ctx_by_pid(current->ppid);
  addr = (addr >> 12) << 12;
  th = find_thread_thmap(parent, addr, &thmap);
  
  if(!th || th->parent_ctx != parent || !thmap){
        goto segfault_exit; 
  }
  if(th->pid != current->pid){ // Some sibling is the owner of this area
       if((thmap->flags & TP_SIBLINGS_NOACCESS) || 
          ((error_code & PF_ERROR_WR) && (thmap->flags & TP_SIBLINGS_RDONLY)))           
                goto segfault_exit; 
  }     
try_fix: 
  if((error_code & PF_ERROR_WR) && !(thmap->flags & MM_WR))
        goto segfault_exit;
   if(thmap->flags & MM_WR)
        pte_flags |=  0x2; 
  // Some other thread is executing
  // and faulted (can not be write access as we have checked above) map RO
  if((thmap->flags & TP_SIBLINGS_RDONLY) && th->pid != current->pid)
         pte_flags = 0x5;
  install_page_table(current, addr, pte_flags, 0); 
  return 1; 

segfault_exit:
          segfault_exit(current->pid, current->regs.entry_rip, addr);
	  return -1;
}

void update_mapping(struct exec_context *ctx, struct exec_context *next, struct thread_private_map *thmap, u8 other)
{
  long address = thmap->start_addr; 
  u32 pageit = 0;
  while(pageit < thmap->length){
      u64 *pte = getpte(ctx, address, 0);
      u32 pte_flags = 0;
      if(pte && !other){
             pte_flags = 0x5;
             if(thmap->flags & MAP_WR)
                    pte_flags = 0x7;
      }else if(pte){
             if(thmap->flags & TP_SIBLINGS_RDWR)
                   pte_flags = (thmap->flags & MAP_WR) ? 0x7 : 0x5;
             else if(thmap->flags & TP_SIBLINGS_RDONLY)  
                   pte_flags = 0x5;
      }
      if(pte && ((*pte) >> PTE_SHIFT)){
               *pte = ((*pte >> 3) << 3) | pte_flags;
               asm volatile ("invlpg (%0);" 
                    : 
                    : "r"(address) 
                    : "memory");   // Flush TLB
      }
      pageit += PAGE_SIZE;
      address += PAGE_SIZE;    
  }
}

int handle_private_ctxswitch(struct exec_context *current, struct exec_context *next)
{
  int ctr1, ctr2;
  struct thread *th = NULL;
  struct thread *current_th = NULL;
  struct thread  *next_th = NULL;

  //printk("context switch between outgoing = %d incomming = %d\n", current->pid, next->pid);
  if(current->pgd != next->pgd){
      printk("Unrelated call. Returning\n");
      return -1;
  } 
  if(isProcess(current)){   
      current_th = NULL;
      th = &current->ctx_threads->threads[0];
      next_th = find_thread_from_pid(current, next->pid);
  }else if(isProcess(next)){
      next_th = NULL;
      th = &next->ctx_threads->threads[0];
      current_th = find_thread_from_pid(next, current->pid);
  }else if(current->ppid == next->ppid){ //We are siblings
      struct exec_context *parent = get_ctx_by_pid(current->ppid);
      next_th = find_thread_from_pid(parent, next->pid);
      current_th = find_thread_from_pid(parent, current->pid);
      th = &parent->ctx_threads->threads[0];
  }else{
      printk("Unrelated call. Returning\n");
      return -1;
  } 
  
  for(ctr1=0; ctr1<MAX_THREADS; ++ctr1, th++){
        struct thread_private_map *thmap = &th->private_mappings[0];
        for(ctr2=0; ctr2<MAX_PRIVATE_AREAS; ++ctr2, thmap++){
             if(thmap->owner){  //legitimate
                     if(thmap->owner == next_th || !next_th)  //either incoming is a process or the owner  
                           update_mapping(current, next, thmap, 0);
                     else if(next_th)    //Need to apply the sibling logic
                           update_mapping(current, next, thmap, 1);
             }          
        }
  }

}
