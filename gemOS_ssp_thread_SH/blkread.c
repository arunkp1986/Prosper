#include<memory.h>
#include<bdev.h>
#include<lib.h>

//char* base_buf = NULL;
int read_disk_block(char *buf, u32 num){
    //printk("read disk block\n");
    if(!buf)
        printk("buffer not allocated\n");
    bdev_read_block(buf, num);
    return -1;
}
