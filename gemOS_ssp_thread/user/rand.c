#include<ulib.h>


int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
   // unsigned idx = rand();
   // while (idx > 0) {
   //    printk()
   // }
   checkpoint_init(CP_INTERVAL);
    printf("Random\n");
    printf("CP_INTERVAL = %d\n", CP_INTERVAL);

   int size = (1 << 15);
   volatile char arr[size];

   unsigned seed = 0xACE1u;
   unsigned bit = 0;
   unsigned idx;

   int times = 2;


    while (times--) {
        for (int i=0; i<size; i++) {
            idx = rand(&seed, &bit) % size;
            arr[idx] = 'A';

            volatile int j = 100;
            while (j) {j--;}
        }
    }

   return 0;
}