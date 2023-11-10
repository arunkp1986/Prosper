all: gemOS.kernel
SRCS = main.c lib.c idt.c kbd.c shell.c serial.c apic.c nonvolatile.c memory.c context.c entry.c schedule.c mmap.c cfork.c page.c fs.c file.c pipe.c msr.c list.c bdev.c blkread.c clone_threads.c clone_threads_helper.c
OBJS = boot.o main.o lib.o idt.o kbd.o shell.o serial.o apic.o nonvolatile.o memory.o context.o entry.o schedule.o mmap.o cfork.o page.o  fs.o file.o pipe.o msr.o list.o bdev.o blkread.o clone_threads.o clone_threads_helper.o
UOBJS = user/init.o user/lib.o user/gthread.o
CFLAGS  = -g -nostdlib -nostdinc -fno-builtin -fno-stack-protector -fpic -m64 -I./include -I../include
LDFLAGS = -nostdlib -nodefaultlibs  -q -melf_x86_64 -Tlink64.ld
ASFLAGS = --64  
AS = as 
USER_CFLAGS  = -nostdlib -nostdinc -fno-builtin -fno-stack-protector -fpic -m64 -I./user/


%o: %c
	gcc -c $(CFLAGS) $< -o $@ 

boot.o : boot_64.S
	$(AS) $(ASFLAGS) $^ -o $@

user/%o:user/%c
	gcc -c $(USER_CFLAGS) $< -o $@

gemOS.kernel: $(OBJS) $(UOBJS)
	ld $(LDFLAGS) -o $@ $(OBJS) $(UOBJS)

.Phony: clean
clean:
	rm -f *.o; rm -f gemOS.kernel; rm -f user/*.o;
