#include<entry.h>
#include<lib.h>
#include<context.h>
#include<memory.h>
#include<schedule.h>
#include<file.h>
#include<pipe.h>
#include<kbd.h>
#include<fs.h>
#include<cfork.h>
#include<page.h>
#include<mmap.h>
#include<msr.h>
#include<dirty.h>

/* Returns the pte coresponding to a user address. 
Return NULL if mapping is not present or mapped
in ring-0 */


u64* get_user_pte(struct exec_context *ctx, u64 addr, int dump) 
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

     
         
     /* Address should be mapped as un-priviledged in PUD*/
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
      
      /* Address should be mapped as un-priviledged in PTE*/
      if( (*entry & 0x1) == 0 || (*entry & 0x4) == 0)
          goto out;
      
      if(dump){
            phy_addr = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
            printk("L1: Entry = %x PFN = %x FLAGS = %x\n", (*entry), phy_addr, (*entry) & (~FLAG_MASK)); 
      }
     return entry;

out:
      return NULL;
}

/* Returns 0 if successfully mmaped else return -1 (if not found)*/
int do_unmap_user(struct exec_context *ctx, u64 addr) 
{
    u64 *pte_entry = get_user_pte(ctx, addr, 0);
    if(!pte_entry)
             return -1;
  
    u64 pfn = *pte_entry >> PTE_SHIFT & ~(1UL<<34);
    u32 region = get_mem_region(pfn); 
    //os_pfn_free(USER_REG, (*pte_entry >> PTE_SHIFT) & 0xFFFFFFFF);
    os_pfn_free(region, pfn);
    *pte_entry = 0;  // Clear the PTE
  
    asm volatile ("invlpg (%0);" 
                    :: "r"(addr) 
                    : "memory");   // Flush TLB
      return 0;
}

void install_page_table(struct exec_context *ctx, u64 addr, u64 error_code, u8 is_nvm) {
    u64 *vaddr_base = (u64 *)osmap(ctx->pgd);
    u64 *entry;
    u64 pfn;
    u64 ac_flags = 0x5 | (error_code & 0x2);
  
    entry = vaddr_base + ((addr & PGD_MASK) >> PGD_SHIFT);
    if(*entry & 0x1) {
      // PGD->PUD Present, access it
       pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
       vaddr_base = (u64 *)osmap(pfn);
    }else{
      // allocate PUD
      pfn = os_pfn_alloc(OS_PT_REG);
      *entry = (pfn << PTE_SHIFT) | ac_flags;
      vaddr_base = osmap(pfn);
    }
  
    entry = vaddr_base + ((addr & PUD_MASK) >> PUD_SHIFT);
    if(*entry & 0x1) {
       // PUD->PMD Present, access it
       pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
       vaddr_base = (u64 *)osmap(pfn);
    }else{
       // allocate PMD
       pfn = os_pfn_alloc(OS_PT_REG);
       *entry = (pfn << PTE_SHIFT) | ac_flags;
       vaddr_base = osmap(pfn);
    }
  
   entry = vaddr_base + ((addr & PMD_MASK) >> PMD_SHIFT);
    if(*entry & 0x1) {
       // PMD->PTE Present, access it
       pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
       vaddr_base = (u64 *)osmap(pfn);
    }else{
       // allocate PMD
       pfn = os_pfn_alloc(OS_PT_REG);
       *entry = (pfn << PTE_SHIFT) | ac_flags;
       vaddr_base = osmap(pfn);
    }
   
   entry = vaddr_base + ((addr & PTE_MASK) >> PTE_SHIFT);
   // since this fault occured as frame was not present, we don't need present check here
   if(is_nvm){
       pfn = os_pfn_alloc(NVM_USER_REG);
   }
   else{
       pfn = os_pfn_alloc(USER_REG);
   }
   *entry = (pfn << PTE_SHIFT) | ac_flags;
}

int validate_page_table(struct exec_context *ctx, u64 addr, int dump) {
    u64 *vaddr_base = (u64 *)osmap(ctx->pgd);
    u64 *entry;
    u64 pfn;
    
    entry = vaddr_base + ((addr & PGD_MASK) >> PGD_SHIFT);
    if(*entry & 0x1) {
      // PGD->PUD Present, access it
       pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
       vaddr_base = (u64 *)osmap(pfn);
       if(dump)
           printk("PGD-entry:%x, *(PGD-entry):%x, PUD-VA-Base:%x\n",entry,*entry,vaddr_base);
    }else{
      return 0; //PGD->PUD not present 
    }
  
    entry = vaddr_base + ((addr & PUD_MASK) >> PUD_SHIFT);
    if(*entry & 0x1) {
       // PUD->PMD Present, access it
       pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
       vaddr_base = (u64 *)osmap(pfn);
       if(dump)
           printk("PUD-entry:%x, *(PUD-entry):%x, PMD-VA-Base:%x\n",entry,*entry,vaddr_base);
    }else{
       return 0;
    }
  
   entry = vaddr_base + ((addr & PMD_MASK) >> PMD_SHIFT);
    if(*entry & 0x1) {
       // PMD->PTE Present, access it
       pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
       vaddr_base = (u64 *)osmap(pfn);
        if(dump)
           printk("PMD-entry:%x, *(PMD-entry):%x, PTE-VA-Base:%x\n",entry,*entry,vaddr_base);
    }else{
       return 0;
    }
   
   entry = vaddr_base + ((addr & PTE_MASK) >> PTE_SHIFT);
   if(*entry & 0x1) {
       // PTE->PFN Present,
       pfn = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
       vaddr_base = (u64 *)osmap(pfn);
        if(dump)
           printk("PTE-entry:%x, *(PTE-entry):%x, PFN:%x\n",entry,*entry,pfn);
       return 1;
    }else{
       return 0;
    }
}


 long do_write(struct exec_context *ctx, u64 address, u64 length)
{
    if(length > MAX_WRITE_LEN)
       goto bad_write;
    if(!get_user_pte(ctx, address, 0) || 
       ((address >> PAGE_SHIFT) != ((address + length) >> PAGE_SHIFT) && !get_user_pte(ctx, address + length, 0)))
       goto bad_write;
     print_user((char *)address, length);
     return length;

bad_write:
      printk("Bad write address = %x length=%d\n", address, length);
      return -1;
}

static long do_expand(struct exec_context *ctx, u64 size, int segment_t)
{
    u64 old_next_free;
    struct mm_segment *segment;

    // Sanity checks
    if(size > MAX_EXPAND_PAGES)
              goto bad_segment;

    if(segment_t == MAP_RD)
         segment = &ctx->mms[MM_SEG_RODATA];
    else if(segment_t == MAP_WR)
         segment = &ctx->mms[MM_SEG_DATA];
    else
          goto bad_segment;

    if(segment->next_free + (size << PAGE_SHIFT) > segment->end)
          goto bad_segment;
     
     // Good expand call, do it!
     old_next_free = segment->next_free;
     segment->next_free += (size << PAGE_SHIFT);
     return old_next_free;
   
bad_segment:
      return 0;
}

static long do_shrink(struct exec_context *ctx, u64 size, int segment_t)
{
    struct mm_segment *segment;
    u64 address;
    // Sanity checks
    if(size > MAX_EXPAND_PAGES)
              goto bad_segment;

    if(segment_t == MAP_RD)
         segment = &ctx->mms[MM_SEG_RODATA];
    else if(segment_t == MAP_WR)
         segment = &ctx->mms[MM_SEG_DATA];
    else
          goto bad_segment;
    if(segment->next_free - (size << PAGE_SHIFT) < segment->start)
          goto bad_segment;
    
    // Good shrink call, do it!
    segment->next_free -= (size << PAGE_SHIFT);
    for(address = segment->next_free; address < segment->next_free + (size << PAGE_SHIFT); address += PAGE_SIZE) 
          do_unmap_user(ctx, address);
    return segment->next_free;

bad_segment:
      return 0;
   
}


void do_exit(u8 normal) 
{
  /*You may need to invoke the scheduler from here if there are
    other processes except swapper in the system. Make sure you make 
    the status of the current process to UNUSED before scheduling 
    the next process. If the only process alive in system is swapper, 
    invoke do_cleanup() to shutdown gem5 (by crashing it, huh!)
    */
  // Scheduling new process (swapper if no other available)
  int ctr;
  struct exec_context *ctx = get_current_ctx();
  struct exec_context *new_ctx;
  
  //Need to wake up parent when child process created with vfork exits

#ifdef vfork_var
  vfork_exit_handle(ctx);
#endif
 do_file_exit(ctx);   // Cleanup the files

  // cleanup of this process
  os_pfn_free(OS_PT_REG, ctx->os_stack_pfn);
  ctx->state = UNUSED;
  // If it is a thread call handle exit 
  if(ctx->type == EXEC_CTX_USER_TH)
        handle_thread_exit(ctx, normal);

  if(ctx->ctx_threads){
      cleanup_all_threads(ctx);
  }

  // check if we need to do cleanup
  int proc_exist = -1;

  for(ctr = 1; ctr < MAX_PROCESSES; ctr++) {
    struct exec_context *new_ctx = get_ctx_by_pid(ctr);
    if(new_ctx->state != UNUSED) {
          proc_exist = 1;
          break;
    }
  }

  stats->num_processes--; 
  if(proc_exist == -1) 
        do_cleanup();  /*Call this conditionally, see comments above*/
   
  new_ctx = pick_next_context(ctx);
  schedule(new_ctx);  //Calling from exit

}
//u64 test_pfn = os_pfn_alloc(USER_REG);
  //u64 test_addr = (u64)osmap(test_pfn);
  //map_physical_page(ctx->pgd, test_addr, MM_WR|MM_RD, test_pfn);
  //printk("address: %x value before:%c\n",test_addr,*(char*)test_addr);
  //writemsr(MISCREG_DIRTYMAP_ADDR, (u64)test_addr);
  //printk("msr value gemos: %x\n",readmsr(MISCREG_DIRTYMAP_ADDR));
  //printk("address: %x value after:%c\n",test_addr,*(char*)test_addr);

/*system call handler for sleep*/
long do_sleep(u32 ticks) 
{
  struct exec_context *new_ctx;
  struct exec_context *ctx = get_current_ctx();
  ctx->ticks_to_sleep = ticks;
  ctx->state = WAITING;
  new_ctx = pick_next_context(ctx);
  schedule(new_ctx);  //Calling from sleep
  return 0;
}

static unsigned do_start_track(struct exec_context *ctx){
    start_checkpoint(ctx);
    return 0;
}

static unsigned do_end_track(struct exec_context *ctx){
    end_checkpoint(ctx);
    return 0;
}

static unsigned do_end_track2(struct exec_context *ctx){
    printk("not implemented\n");
    return 0;
}

static unsigned do_custom_print(u64 param){
    //custom_print(param);
    return 0;
}


static unsigned do_read_dirty_bitmap(struct exec_context *ctx, u16 track_size){
    //need to remove this, Arun
    return 0;
}

static long do_fork()
{
  
  struct exec_context *new_ctx = get_new_ctx();
  struct exec_context *ctx = get_current_ctx();
  u32 pid = new_ctx->pid;
  
  *new_ctx = *ctx;  //Copy the process
  new_ctx->pid = pid;
  new_ctx->ppid = ctx->pid; 
  copy_mm(new_ctx, ctx);
  setup_child_context(new_ctx);

  return pid;
}

/*do_cfork creates the child context and makes the child READY for schedule. 
  returns pid of the child process*/

static long do_cfork(){
    u32 pid;
    struct exec_context *new_ctx = get_new_ctx();
    struct exec_context *ctx = get_current_ctx();
    pid = new_ctx->pid;

    *new_ctx = *ctx;
    new_ctx->pid = pid;
    new_ctx->ppid = ctx->pid;
    
    cfork_copy_mm(new_ctx, ctx);
    setup_child_context(new_ctx);

    return pid;

}

/*do_vfork creates the child context and schedules it after keeping parent to WAITING state.
  In do_vfork the child copies user stack and points entry_rsp and rbp to new locations */

static long do_vfork(){
    u32 pid;
    struct exec_context *new_ctx = get_new_ctx();
    struct exec_context *ctx = get_current_ctx();
    pid = new_ctx->pid;
    u64 length = (ctx->mms[MM_SEG_STACK].end)-(ctx->regs.entry_rsp);
    u64 new_entry_rsp = ctx->regs.entry_rsp - length;
    u64 new_rbp = ctx->regs.rbp - length;

    *new_ctx = *ctx;
    new_ctx->pid = pid;
    new_ctx->ppid = ctx->pid;
   
    vfork_copy_mm(new_ctx, ctx);
    setup_child_context(new_ctx);

     memcpy((char*)new_entry_rsp,(char*)ctx->regs.entry_rsp,length);
     ctx->state = WAITING;
     ctx->regs.rax = pid;
     new_ctx->regs.entry_rsp = new_entry_rsp;
     new_ctx->regs.rbp = new_rbp;
     schedule(new_ctx);
     return pid;
}


/*
  system call handler for clone, create thread like 
  execution contexts
*/
/*
static long do_clone(void *th_func, void *user_stack) 
{
  int ctr;
  struct exec_context *new_ctx = get_new_ctx();
  struct exec_context *ctx = get_current_ctx();
  new_ctx->type = ctx->type;
  new_ctx->state = READY;
  new_ctx->used_mem = ctx->used_mem;
  new_ctx->pgd = ctx->pgd;

  // allocate page for stack
  new_ctx->os_stack_pfn = os_pfn_alloc(OS_PT_REG);
  new_ctx->os_rsp = (u64)osmap(new_ctx->os_stack_pfn + 1);
  for(ctr = 0; ctr < MAX_MM_SEGS; ++ctr)
    new_ctx->mms[ctr] = ctx->mms[ctr];

  new_ctx->regs = ctx -> regs;  // XXX New context inherits the register state

  // Context executes the function on return
  new_ctx->regs.entry_rip = (u64) th_func;
  new_ctx->regs.entry_rsp = (u64) user_stack;
  new_ctx->regs.rbp = new_ctx->regs.entry_rsp;

  new_ctx->pending_signal_bitmap = ctx->pending_signal_bitmap;

  new_ctx->ticks_to_alarm = ctx->ticks_to_alarm;
  new_ctx->ticks_to_sleep = ctx->ticks_to_sleep;
  new_ctx->alarm_config_time = ctx->alarm_config_time;


  for(ctr = 0; ctr < MAX_SIGNALS; ctr++)
      new_ctx->sighandlers[ctr] = ctx -> sighandlers[ctr];

  //Update the name of the process
  for(ctr = 0; ctr < CNAME_MAX && ctx->name[ctr]; ++ctr) 
      new_ctx->name[ctr] = ctx->name[ctr];
  sprintk(new_ctx->name+ctr, "%d", ctx->pid + 1); 

  return 0;
}*/

long invoke_sync_signal(int signo, u64 *ustackp, u64 *urip) 
{
  /*
     If signal handler is registered, manipulate user stack and RIP to execute signal handler
     ustackp and urip are pointers to user RSP and user RIP in the exception/interrupt stack
     Default behavior is exit() if sighandler is not registered for SIGFPE or SIGSEGV.
     Ignored for SIGALRM
  */

  struct exec_context *ctx = get_current_ctx();
  dprintk("Called signal with ustackp=%x urip=%x\n", *ustackp, *urip);
  
  if(ctx->sighandlers[signo] == NULL && signo != SIGALRM) {
      do_exit(0);
  }else if(ctx->sighandlers[signo]){  
      /*
        Add a frame to user stack
        XXX Better to check the sanctity before manipulating user stack
      */
      u64 rsp = (u64)*ustackp;
      *((u64 *)(rsp - 8)) = *urip;
      *ustackp = (u64)(rsp - 8);
      *urip = (u64)(ctx->sighandlers[signo]);
  }
  return 0;

}

/*system call handler for signal, to register a handler*/
static long do_signal(int signo, unsigned long handler) 
{
  struct exec_context *ctx = get_current_ctx();
  if(signo < MAX_SIGNALS && signo > -1) {
         ctx -> sighandlers[signo] = (void *)handler; // save in context
         return 0;
  }
 
  return -1;
}

/*system call handler for alarm*/
static long do_alarm(u32 ticks) 
{
  struct exec_context *ctx = get_current_ctx();
  if(ticks > 0) {
    ctx -> alarm_config_time = ticks;
    ctx -> ticks_to_alarm = ticks;
    return 0;
  }
  return -1;
}


/*system call handler to open file */
/*todo: implement O_TRUNC */

int do_file_open(struct exec_context *ctx,u64 filename, u64 flag, u64 mode)
{

  struct file *filep;
  struct inode * inode;
  u32 fd, i;
  
  dprintk("%s %u %u\n",filename,mode,flag);
  /*This code is to handle stdin,stdout,stderr cases */

  if(!strcmp((char*)filename,"stdin"))
       return open_standard_IO(ctx, STDIN);

  if(!strcmp((char*)filename,"stdout"))
       return open_standard_IO(ctx, STDOUT);

  if(!strcmp((char*)filename,"stderr"))
       return open_standard_IO(ctx, STDERR);
 
  /* END of stdin,stdout,stderr cases */
  return do_regular_file_open(ctx, (char *)filename, flag, mode);

}



/*system call handler to read file */

int do_file_read(struct exec_context *ctx, u64 fd, u64 buff, u64 count){
  int read_size = 0;
  struct file *filep = ctx->files[fd];
  dprintk("fd in read:%d\n",fd);
  
  
  if(!filep){
    return -EINVAL; //file is not opened
  }
  if((filep->mode & O_READ) != O_READ){
    return -EACCES; //file is write only
  }

   
  if(filep->fops->read){
     read_size = filep->fops->read(filep, (char*)buff, count);
     dprintk("buff inside read:%s\n",buff);
     dprintk("read size:%d\n",read_size);
     return read_size;
  }
  return -EINVAL;
  
}

/*system call handler to write file */

int do_file_write(struct exec_context *ctx,u64 fd,u64 buff,u64 count){
  int write_size;
  struct file *filep = ctx->files[fd];
  if(!filep){
    return -EINVAL; //file is not opened
  }
  if(!(filep->mode & O_WRITE)){
    return -EACCES; // file is not opened in write mode
  }
  if(filep->fops->write){
      write_size = filep->fops->write(filep, (char*)buff, count);
      dprintk("write size:%d\n",write_size);
      return write_size;
  }
  return -EINVAL;
}

/*system call handler to create pipe */
int do_create_pipe(struct exec_context *ctx, int* fd)
{
  int val =  create_pipe(ctx, fd);
  return val;
}


int do_dup(struct exec_context *ctx, int oldfd)
{
  int val = fd_dup(ctx, oldfd);
  return val;
}


int do_dup2(struct exec_context *ctx, int oldfd, int newfd)
{
  int val = fd_dup2(ctx, oldfd, newfd);
  return val;
}

int do_close(struct exec_context *ctx, int fd)
{

  int ret;
  struct file *filep = ctx->files[fd];
  if(!filep || !filep->fops || !filep->fops->close){
    return -EINVAL; //file is not opened
  }
  ctx->files[fd] = NULL;
  ret = filep->fops->close(filep);
  
  
}

long do_lseek(struct exec_context *ctx, int fd, long offset, int whence)
{
   struct file *filep = ctx->files[fd];
   if(filep && filep->fops->lseek)
   {
      return filep->fops->lseek(filep, offset, whence);
   }
   return -EINVAL; 
}



/*System Call handler*/
long  do_syscall(int syscall, u64 param1, u64 param2, u64 param3, u64 param4)
{
    struct exec_context *current = get_current_ctx();
    unsigned long saved_sp;
    extern struct user_regs * saved_regs;
 
    asm volatile(
                   "mov %%rbp, %0;"
                    : "=r" (saved_sp) 
                    :
                    : "memory"
                );  

    saved_sp += 0x10;    //rbp points to entry stack and the call-ret address is pushed onto the stack
    memcpy((char *)(&current->regs), (char *)saved_sp, sizeof(struct user_regs));  //user register state saved onto the regs
    stats->syscalls++;
    dprintk("[GemOS] System call invoked. syscall no = %d\n", syscall);
    switch(syscall)
    {
          case SYSCALL_EXIT:
                              dprintk("[GemOS] exit code = %d\n", (int) param1);
                              do_exit(1);
                              break;
          case SYSCALL_GETPID:
                              dprintk("[GemOS] getpid called for process %s, with pid = %d\n", current->name, current->pid);
                              return current->pid;      
          case SYSCALL_EXPAND:
                             return do_expand(current, param1, param2);
          case SYSCALL_SHRINK:
                             return do_shrink(current, param1, param2);
          case SYSCALL_ALARM:
                              return do_alarm(param1);
          case SYSCALL_SLEEP:
                              return do_sleep(param1);
          case SYSCALL_SIGNAL: 
                              return do_signal(param1, param2);
          case SYSCALL_CLONE:
                              return do_clone((void *)param1, (void *)param2, (void*)param3);
          case SYSCALL_FORK:
                              return do_fork();
          case SYSCALL_CFORK:
                              return do_cfork();
          case SYSCALL_VFORK:
                              return do_vfork();
          case SYSCALL_STATS:
                             printk("ticks = %d swapper_invocations = %d context_switches = %d lw_context_switches = %d\n", 
                             stats->ticks, stats->swapper_invocations, stats->context_switches, stats->lw_context_switches);
                             printk("syscalls = %d page_faults = %d used_memory = %d num_processes = %d\n",
                             stats->syscalls, stats->page_faults, stats->used_memory, stats->num_processes);
                             printk("copy-on-write faults = %d allocated user_region_pages = %d\n",stats->cow_page_faults,
                             stats->user_reg_pages);
                             break;
          case SYSCALL_GET_USER_P:
                             return stats->user_reg_pages;
          case SYSCALL_GET_COW_F:
                             return stats->cow_page_faults;

          case SYSCALL_CONFIGURE:
                             memcpy((char *)config, (char *)param1, sizeof(struct os_configs));      
                             break;
          case SYSCALL_PHYS_INFO:
                            printk("OS Data strutures:     0x800000 - 0x2000000\n");
                            printk("Page table structures: 0x2000000 - 0x6400000\n");
                            printk("User pages:            0x6400000 - 0x20000000\n");
                            break;

          case SYSCALL_DUMP_PTT:
                              return (u64) get_user_pte(current, param1, 1);
          
          case SYSCALL_MMAP:
                              return (long) vm_area_map(current, param1, param2, param3, param4);

          case SYSCALL_MUNMAP:
                              return (u64) vm_area_unmap(current, param1, param2);
          case SYSCALL_MPROTECT:
                              return (long) vm_area_mprotect(current, param1, param2, param3);
          case SYSCALL_PMAP:
                              return (long) vm_area_dump(current->vm_area, (int)param1);
          case SYSCALL_OPEN:
                                  return do_file_open(current,param1,param2,param3);
          case SYSCALL_READ:
                                  return do_file_read(current,param1,param2,param3);
          case SYSCALL_WRITE:
                                  return do_file_write(current,param1,param2,param3);
          case SYSCALL_PIPE:
                                  return do_create_pipe(current, (void*) param1);

          case SYSCALL_DUP:
                                  return do_dup(current, param1);

          case SYSCALL_DUP2:
                                  return do_dup2(current, param1, param2);  
          case SYSCALL_CLOSE:
                                  return do_close(current, param1);

          case SYSCALL_LSEEK:
                                  return do_lseek(current, param1, param2, param3);
	  case SYSCALL_START_TRACK:
				  if(saved_regs){
                                      //dprintk("saving regs\n");
                                      memcpy((char *)(saved_regs),
                                                      (char *)saved_sp,
                                                      sizeof(struct user_regs));//for checkpoint SP
                                  }
				  //printk("entry_rsp:%x\n",saved_regs->entry_rsp);
				  return do_start_track(current);
	  case SYSCALL_END_TRACK:
				 if(saved_regs){
                                      //dprintk("saving regs\n");
                                      memcpy((char *)(saved_regs),
                                                      (char *)saved_sp,
                                                      sizeof(struct user_regs));//for checkpoint SP
                                 }
				  //printk("entry_rsp:%x\n",saved_regs->entry_rsp);
				  return do_end_track(current);
	  case SYSCALL_END_TRACK2:
				  return do_end_track2(current);

	  case SYSCALL_CTX_PRINT:
				  return print_checkpoint_stats();
          case SYSCALL_DUMP_GEM5:
				  x86_dump_gem5_stats();
				  return 0;
          case SYSCALL_RESET_GEM5:
				  x86_reset_gem5_stats();
				  return 0;
	  case SYSCALL_BLK_READ:
                                 return read_disk_block((char*)param1,param2);
	  case SYSCALL_MAKE_THREAD_READY:
                                  return do_make_thread_ready(param1);

          case SYSCALL_WAIT_FOR_THREAD:
                                  return do_wait_for_thread(param1);
         case SYSCALL_MERGE:
                                  return merge_pages();

	  default:
                              return -1;
                                
    }
    return 0;   /*GCC shut up!*/
}

extern int do_div_by_zero(struct user_regs *regs) {
    u64 rip = regs->entry_rip;
    printk("Div-by-zero @ [%x]\n", rip);
    invoke_sync_signal(SIGFPE, &regs->entry_rsp, &regs->entry_rip);
    return 0;
}


extern int do_page_fault(struct user_regs *regs, u64 error_code)
{
     u64 rip, cr2;
     struct exec_context *current = get_current_ctx();
     rip = regs->entry_rip;
     stats->page_faults++;
     /*Get the Faulting VA from cr2 register*/
     asm volatile ("mov %%cr2, %0;"
                  :"=r"(cr2)
                  ::"memory");
     memcpy((char *)(&current->regs), (char *)regs, sizeof(struct user_regs));  //user register state saved onto the regs
     dprintk("PageFault:@ [RIP: %x] [accessed VA: %x] [error code: %x]\n", rip, cr2, error_code);

     /*Check error code. We only handle user pages that are not present*/
     if(!(error_code & 0x4)){
         printk("page fault in kernel mode. pid: %d\n",current->pid);
         goto sig_exit;
     }

     /*Check error code. We only handle user pages that are not present*/
     if((error_code == 0x7) && (validate_page_table(current, cr2,0))){
       int result = handle_cow_fault(current, cr2);
       if(result > 0){ 
          stats->cow_page_faults++;
          goto done;
       }else if(result < 0){
          goto sig_exit;
       }
     }
     if((cr2 >= (current -> mms)[MM_SEG_DATA].start) && (cr2 <= (current -> mms)[MM_SEG_DATA].end)){
       //Data segment
            dprintk("current pid:%d, error:%x \n",current->pid, error_code);
            if(cr2 < (current -> mms)[MM_SEG_DATA].next_free){
            // allocate
                    install_page_table(current, cr2, error_code | 0x2, 0);
                   goto done;
            }else{
                   printk("Inside Data segment\n");
                   goto sig_exit;
            }

      }

    if((cr2 >= (current -> mms)[MM_SEG_RODATA].start) && (cr2 <= (current -> mms)[MM_SEG_RODATA].end)) {
    // ROData Segment
        if(cr2 < (current -> mms)[MM_SEG_RODATA].next_free && ((error_code & 0x2) == 0)){
                 //Bit pos 1 in error code = Read access
               install_page_table(current, cr2, error_code, 0);
               goto done;
        }else{
            dprintk("Inside RO data\n");
            goto sig_exit;
        }
    }

    if ((cr2 >= (current -> mms)[MM_SEG_STACK].start) && (cr2 <= (current -> mms)[MM_SEG_STACK].end)) {
      // Stack Segment
          
           if(cr2 < (current -> mms)[MM_SEG_STACK].next_free){
                    cr2 = (cr2 >> PAGE_SHIFT) << PAGE_SHIFT;
                    while((current->mms)[MM_SEG_STACK].next_free != cr2){
                         (current->mms)[MM_SEG_STACK].next_free -= PAGE_SIZE;
                         install_page_table(current, (current->mms)[MM_SEG_STACK].next_free, error_code | 0x2, 0);
                         // This marks the usage range of stack  from stack end to stack next_free
                    }
	            //printk("next free:%x\n",(current->mms)[MM_SEG_STACK].next_free);
                    //install_page_table(current, (current -> mms)[MM_SEG_STACK].next_free, error_code | 0x2, 0);
                    goto done;
            }else{
                    dprintk("pid:%u\n",current->pid);
		    printk("pfn:%x\n",pt_walk(current, MM_SEG_STACK, 0, cr2));
                    printk("Inside Stack, cr2 [%x] next [%x]\n",cr2, (current -> mms)[MM_SEG_STACK].next_free);
                    goto sig_exit;
            }
    }
    if(cr2 >= MMAP_AREA_START && cr2 <= MMAP_AREA_END)
    {
        stats->mmap_page_faults++;
        int result = vm_area_pagefault(current, cr2, error_code);
        if(result > 0) 
          goto done;
        else{
          goto sig_exit;
        }
    }

/*This must be at the beginning*/
sig_exit:
  printk("(Sig_Exit) PF Error @ [RIP: %x] [accessed VA: %x] [error code: %x]\n", rip, cr2, error_code);
  //printk("pfn:%x\n",pt_walk(current, MM_SEG_STACK, 0, cr2));
  //validate_page_table(current, cr2, 0);
  invoke_sync_signal(SIGSEGV, &regs->entry_rsp, &regs->entry_rip);
  return 0;

done:
  return 0;
}

