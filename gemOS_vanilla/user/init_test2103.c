/*
 *Here stream access to stack sets all bits in dirty bitmap
 *This test case is to check scalability of the dirty tracking scheme
 *
 * */
#include<ulib.h>

#define PAGE_SIZE 4096
#define SIZE (8*PAGE_SIZE)
#define TARCK_SIZE 64

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{  
   checkpoint_start(1000);
   char * ptr1 = (char*)mmap(NULL,4096,PROT_WRITE,MAP_NVM);
   ptr1[0] = 10;
   char * ptr2 = (char*)mmap(NULL,4096,PROT_READ,MAP_NVM);
   char * ptr3 = (char*)mmap(NULL,4096,PROT_WRITE,MAP_NVM);
   munmap(ptr2, 4096);
   checkpoint_end();
   return 0;
}
