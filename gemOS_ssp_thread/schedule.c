#include<context.h>
#include<memory.h>
#include<schedule.h>
#include<lib.h>
#include<apic.h>
#include<idt.h>
#include<entry.h>
#include<dirty.h>
#include<clone_threads.h>

static u64 numticks;

struct exec_context *pick_next_context(struct exec_context *ctx) 
{
  /*Your code goes in here*/
  int pid = (ctx->pid + 1) == MAX_PROCESSES ?  1 : (ctx->pid + 1);
  while(pid){
    struct exec_context *new_ctx = get_ctx_by_pid(pid);
    if(new_ctx->state == READY)
            return new_ctx;
    ++pid;
    if(pid == ctx->pid + 1)
         pid = 0;
    
    if(pid == MAX_PROCESSES){  
         pid = 1; 
         if(ctx->pid == 0)  // Special handling to schedule swapper
             break;
    }
  }
  return get_ctx_by_pid(0);
}


static void do_sleep_and_alarm_account(struct user_regs *regs) 
{
 /*All processes in sleep() must decrement their sleep count*/ 
  int ctr; 
  struct exec_context *ctx = get_current_ctx();

  for(ctr = 0; ctr < MAX_PROCESSES; ++ctr) {
    struct exec_context *ctx = get_ctx_by_pid(ctr);
    if((ctx->state) == WAITING && ctx->ticks_to_sleep > 0){
      ctx->ticks_to_sleep--;
      if(!ctx->ticks_to_sleep) 
           ctx->state = READY;
    }
  }
  // Decrement ticks to alarm and check if alarm signal need to be sent 
  // For the current process only
  //XXX Only active ticks counted in the current implementation
  
  if(ctx->ticks_to_alarm > 0) {
    ctx->ticks_to_alarm--;
    if(ctx->ticks_to_alarm == 0) {
      invoke_sync_signal(SIGALRM, &regs->entry_rsp, &regs->entry_rip);
      ctx->ticks_to_alarm = ctx->alarm_config_time;
    }
  }

  return;
}

void schedule_resumed(struct exec_context *new_ctx){
    unsigned long cr3;
    extern void *return_from_syscall;
    unsigned long retptr = (unsigned long)(&return_from_syscall);
    unsigned long rsp_stack = new_ctx->os_rsp - sizeof(struct user_regs);
    memcpy((char *) rsp_stack, (char *)&new_ctx->regs, sizeof(struct user_regs));
    set_tss_stack_ptr(new_ctx);
    set_current_ctx(new_ctx);
    new_ctx->state = RUNNING;
    cr3 = new_ctx->pgd << PAGE_SHIFT;
    /*Switch CR3 if needed*/
    asm volatile(
                 "mov %%cr3, %%rax;"
                 "cmp %0, %%rax;"
                 "je 1f;"
                 "mov %0, %%cr3;"
                 "1: mov %1, %%rsp;"
                 "xor %%rax, %%rax;"
                 "callq *%2;"
                :
                :"r" (cr3), "r" (rsp_stack), "r"  (retptr)
                :"memory", "rax");

    return;
}

void schedule(struct exec_context *new_ctx) 
{
    /*TODO switch CR3 if needed*/
    unsigned long cr3;
    extern void *return_from_os;
    unsigned long retptr = (unsigned long)(&return_from_os);
    unsigned long rsp_stack = new_ctx->os_rsp - sizeof(struct user_regs);
    memcpy((char *) rsp_stack, (char *)&new_ctx->regs, sizeof(struct user_regs));
    set_tss_stack_ptr(new_ctx);
    set_current_ctx(new_ctx);
    new_ctx->state = RUNNING;
    cr3 = new_ctx->pgd << PAGE_SHIFT;
    /*Switch CR3 if needed*/
    asm volatile(
                 "mov %%cr3, %%rax;"
                 "cmp %0, %%rax;"
                 "je 1f;"
                 "mov %0, %%cr3;"
                 "1: mov %1, %%rsp;"
                 "xor %%rax, %%rax;"
                 "callq *%2;"
                :
                :"r" (cr3), "r" (rsp_stack), "r"  (retptr)
                :"memory", "rax");
}

int handle_timer_tick(struct user_regs *regs){
    struct exec_context *ctx = get_current_ctx();
    static u32 interrupt_count = 0;
    static u32 num_checkpoints = 0;
    struct exec_context *persistent_ctx = (ctx->type == EXEC_CTX_USER_TH)?get_ctx_by_pid(ctx->ppid):ctx;
    extern u16 track_on;
    extern u32 check_point;
    extern struct user_regs * saved_regs;
   // printk("called %s\n",__func__);
    if(track_on){
       //printk("track on\n");
       interrupt_count += 1;
       if(!check_point){
           printk("check_point bug!!!\n");
       }
       dprintk("interruput count:%u, check_point:%u\n",interrupt_count,check_point);
       if(interrupt_count == check_point){
           if(saved_regs){
               //dprintk("saving regs\n");
               *saved_regs = *regs; //for checkpoint SP
           }
	   num_checkpoints += 1;
           dprintk("end track from schedule, checkpoint num:%u\n",num_checkpoints);
           end_checkpoint(persistent_ctx);
           interrupt_count = 0;
           start_checkpoint(persistent_ctx);
       }
    }
	
/*
   This is the timer interrupt handler. 
   You should account timer ticks for alarm and sleep
   and invoke schedule

 */
  struct exec_context *new_ctx; 
  do_sleep_and_alarm_account(regs);

  stats->ticks++; 
  dprintk("Got a tick. #ticks = %u\n", ++numticks);   /*XXX Do not modify this line*/ 
  ctx->state = READY;

  new_ctx = pick_next_context(ctx);
  if(ctx == new_ctx)
     goto ack_irq_and_return;
  stats->context_switches++;
  dprintk("schedluing: old pid = %d  new pid  = %d\n", ctx->pid, new_ctx->pid); /*XXX: Don't remove*/
  ctx->regs = *regs;  /*Save the register state @IRQ*/
  *regs = new_ctx->regs; /*Load the incomming process onto IRQ stack*/

  if(ctx->pgd != new_ctx->pgd){
     unsigned long cr3 = new_ctx->pgd << PAGE_SHIFT;
     asm volatile("mov %0, %%cr3;"
                  :
                  :"r" (cr3)
                  :"memory"
                 );
  }else{
        stats->lw_context_switches;
  }

  set_tss_stack_ptr(new_ctx);
  set_current_ctx(new_ctx);
  new_ctx->state = RUNNING;

ack_irq_and_return:
    ack_irq();
    return 0;
}

