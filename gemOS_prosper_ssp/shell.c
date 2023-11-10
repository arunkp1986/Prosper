#include <lib.h>
#include <kbd.h>
#include <memory.h>
#include <context.h>
#include <init.h>
#include <serial.h>
#include <dirty.h>

void dsh_help(void)
{
   printk("This is a minimal shell.\n");
   printk("---------\n");
   printk("commands:\n"); 
   printk("---------\n");
   printk("help: prints this help\n");
   printk("clear: clears the screen\n");
   printk("config: set and get the current configuration. To set use #config set var=value\n");
   printk("stats: displays the OS statistics\n");
   printk("init: launch the init process. usage: init [arg1] [arg2] ... [arg5]\n");
   printk("exit|shutdown: shutdown the OS\n");
   
}

void shutdown(){
	printk("Shutting down...\n");
	// On real hardware we will have to use ACPI for shutdown
	// gem5 provides a m5exit pseudo instruction to shutdown 
	// the simulator, so all we have to do is, to emit the opcode
	// for this pseudo instruction
	// see gem5/src/arch/x86/isa/decoder/two_byte_opcodes.isa 
	// for opcodes
	__asm__ volatile (".word 0x040F; .word 0x0021;" : : "D"(0) :);
}

void parse_init(char *cmd, struct init_args *args)
{
    u64 value = 0;
    int ctr, num = 0;
    for(ctr=5; cmd[ctr]; ++ctr){
         if(cmd[ctr] == 32 || cmd[ctr] == '\t'){
              *((u64*) args + num) = value;
              ++num;
              value=0;
              while(cmd[ctr] == 32 || cmd[ctr] == '\t')
                    ++ctr;
         }
         
         value *= 10;
         value += cmd[ctr] - '0'; 
    }   
   *((u64*) args + num) = value;
   return;
     
}

void invoke_dsh(void)
{
    struct acpi_info *acpi;
    char *command;
    struct init_args args = {0, 0 , 0 , 0 , 0};
    command = os_alloc(1024,OS_DS_REG);
    

    while(1){ 
               printk("GemOS# ");
               serial_read(command);
               if(command[0] == 0)
                    continue;
               else if(!strcmp(command,"help"))
                   dsh_help();
               else if(!strcmp(command, "clear")){
                   char abc[10];
                   abc[0] = 27;
                   abc[1] = '[';
                   abc[2] = '2';
                   abc[3] = 'J';
                   abc[4] = 27;
                   abc[5] = '[';
                   abc[6] = 'H';
                   abc[7] = 0;
                   serial_write(abc);
               }
               else if(!strcmp(command, "exit") || !strcmp(command, "shutdown"))
                   shutdown();
	       //else if(!strcmp(command, "resume"))
                   //printk("resumed:%u\n",resume_persistent_processes());

#if 0
               else if(!strcmp(command, "acpi")){
                   acpi = os_alloc(sizeof(struct acpi_info));
                   acpi->SCI_EN = 0;
                 if(acpi->SCI_EN)
                      printk("ACPI already initialized\n");
                 else{      
                       if(find_and_enable_acpi(acpi))
                            printk("ACPI not found in BIOS mmap\n");
                       printk("ACPI initialized\n");
                  }
                  os_free(acpi, sizeof(struct acpi_info));
               }else if(!strcmp(command, "halt")){
                        return;
               }else if(!strcmp(command, "shutdown")){
                       acpi = os_alloc(sizeof(struct acpi_info));
                       acpi->SCI_EN = 0;
                       if(!acpi->SCI_EN && find_and_enable_acpi(acpi))
                            printk("ACPI not found in BIOS mmap\n");
                      else
                            handle_acpi_action(acpi, POWEROFF);   
                      os_free(acpi, sizeof(struct acpi_info));
                             
               }
#endif    
            else if(!memcmp(command, "init", 4)){
                    struct exec_context *ctx;
                    //printk("Mem pt=%d user=%d\n",  get_free_pages_region(OS_PT_REG), get_free_pages_region(USER_REG));
                    ctx = create_context("init", EXEC_CTX_USER);
                    parse_init(command, &args);
                    exec_init(ctx, &args);
                    //printk("Mem pt=%d user=%d\n",  get_free_pages_region(OS_PT_REG), get_free_pages_region(USER_REG));
                    /*Should not return here if all goes well*/
                    
             }else if(!strcmp(command, "stats")){
                 
                 printk("ticks = %d swapper_invocations = %d context_switches = %d lw_context_switches = %d\n", 
                         stats->ticks, stats->swapper_invocations, stats->context_switches, stats->lw_context_switches);
                 printk("syscalls = %d page_faults = %d used_memory = %d num_processes = %d\n",
                          stats->syscalls, stats->page_faults, stats->used_memory, stats->num_processes);
             }else
                     printk("Invalid command\n");
    }
}
