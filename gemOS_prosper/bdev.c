#include<bdev.h>
#include<lib.h>
#include<memory.h>

static struct pci_dev pci_ide;

static void delay(int x){ 
	       	while(x--);
}

static void ready(struct pci_dev *pdev)
{
   while((inb(pdev->bars[0]+STAT_CMD) & (BDEV_BUSY | BDEV_READY)) != BDEV_READY);
}

static void data_ready(struct pci_dev *pdev)
{
   while(1){	
             u8 retval = inb(pdev->bars[0]+STAT_CMD);
             if(!(retval & BDEV_BUSY) && (retval & BDEV_DR) == BDEV_DR) 
		   break;  
   }
}

static void check_status(struct pci_dev *pdev)
{
   int retval;	
   while(((retval = inb(pdev->bars[0] + STAT_CMD)) & (BDEV_BUSY | BDEV_READY)) != BDEV_READY);
   if(retval & BDEV_ERR){
	   //Try software reset
	   printk("Bdev is error!\n");
           outb(pdev->bars[1] + DEV_CTRL, 0x4);
           delay(10000);
           outb(pdev->bars[1] + DEV_CTRL, 0x0);
           delay(10000);
           retval = inb(pdev->bars[1] + DEV_CTRL); 
	   gassert(retval == BDEV_READY, "Unrecoverable error! Shuting down\n");
   }	   
   return;
}

//https://wiki.osdev.org/PCI_IDE_Controller
//
static int pci_find_ide_device(int vendor_q, int device_q, struct pci_dev *pdev) 
{
    u32 address;

    u32 bus  = 0;
    u32 dev = 0;
    u32 func = 0;
 
    // In gem5, only PCI bus 0 is used
    for(dev=0; dev<32; ++dev){
        int retval;
	int vendor;
	int device;
        // Create configuration address
        address = ((bus << 16) | (dev << 11) | (func << 8) | ((u32)0x80000000));
 
        // Write out the address
        outl(PCI_CONFIG_ADDRESS, address);

        // Read the config
	retval = inl(PCI_CONFIG_DATA);
        dprintk("pci config read for dev %d val %x\n", dev, retval);
        vendor = retval & 0xFFFF;
        device = (retval >> 16) & 0xFFFF;
	if(vendor == vendor_q && device == device_q){
	      u8 offset = 0x8;  // Read the second register https://wiki.osdev.org/PCI
	      address |= offset;
              
	      // Write out the address
              outl(PCI_CONFIG_ADDRESS, address);

	      retval = inl(PCI_CONFIG_DATA);
              
	      if(((retval >> 16) & 0xffff) == 0x101){  // IDE controller class and subclass values are 1
	           printk("Found the IDE device. class etc %x\n", retval);
		   if((retval & 0x1) == 0){
			  dprintk("Legacy mode PCI is allowed now\n");
                          address = ((bus << 16) | (dev << 11) | (func << 8) | ((u32)0x80000000));
			  pdev->base_address = address;
			  pdev->bars[0] =  0x1F0;;
			  pdev->bars[1] =  0x3F4;
                          

			  address = pdev->base_address | 0x20; 
                          outl(PCI_CONFIG_ADDRESS, address);
                          retval = 0x4F1;
			  outl(PCI_CONFIG_DATA, retval);
			  
			  address = pdev->base_address | 0x4;  
	                  
			  // Write out the address
                          outl(PCI_CONFIG_ADDRESS, address);
	                  retval = inl(PCI_CONFIG_DATA);
                          dprintk("cmd and status %x\n", retval);

			  retval = 0x41;  // Disable interrupts and allow io access
                          outl(PCI_CONFIG_ADDRESS, address);
			  outw(PCI_CONFIG_DATA, retval);
                          
			  return 0;

		   }else{
			  printk("Primary channel is in only native mode, we do not support it yet!\n");
                          return -1;
                   }
	      } 	   
	}	
    }
    return -1;
}

int bdev_init(void)
{
  int retval;	
  // Query the IDE controller

  retval = pci_find_ide_device(IDE_VENDOR_ID, IDE_DEVICE_ID, &pci_ide);
  if(retval)
	  goto err_return;

  //Reset the device
  outb(pci_ide.bars[1] + DEV_CTRL, 0x4);
  delay(10000);
  outb(pci_ide.bars[1] + DEV_CTRL, 0x0);
 
  //Select the drive
  outb(pci_ide.bars[0] + DRV_HEAD, SEL_DISK0_CMD);
  delay(10000);
 
  retval = inb(pci_ide.bars[0] + STAT_CMD); 
  if(retval != BDEV_READY)
	  goto err_return;

  //Disable interrupts TODO: When scheduler changed to handle wait on syscall
  outb(pci_ide.bars[1] + DEV_CTRL, 0x2);
  delay(10000);

  // Double check the status
  retval = inb(pci_ide.bars[1] + DEV_CTRL); 

  printk("Disk Initialization is successful for IDE disk %x\n", retval);  
  return 0;

err_return:
  printk("Disk initialization failed. Are you passing the disk image correctly?");
  return -1;
}

int bdev_read_block(char *buf, u32 block_offset)
{
  int sector = block_offset * NUM_SEC_PER_BLOCK;
  int data_ctr = 0;
  ready(&pci_ide);
  outb(pci_ide.bars[0] + SEC_COUNT, NUM_SEC_PER_BLOCK);  // number of sectors
  outb(pci_ide.bars[0] + SEC_NUM_LOW, sector & 0xff);
  outb(pci_ide.bars[0] + SEC_NUM_MID, (sector >> 8) & 0xff);
  outb(pci_ide.bars[0] + SEC_NUM_HIGH, (sector >> 16) & 0xff);
  outb(pci_ide.bars[0] + DRV_HEAD, SEL_DISK0_CMD | ((sector>>24)&0x0f));
  outb(pci_ide.bars[0] + STAT_CMD, BDEV_CMD_READ);
 
  for(int i=0; i<NUM_SEC_PER_BLOCK; ++i){    
       ready(&pci_ide);
       data_ready(&pci_ide);
       for(int j=0; j<256; j++){
	    u16 retval;    
	    retval = inw(pci_ide.bars[0] + OFF_DATA);
	    buf[data_ctr++] = retval & 0xff;
	    buf[data_ctr++] = (retval >> 8) & 0xff;
       }
  }

  check_status(&pci_ide);
  return 0;
}

int bdev_write_block(char *buf, u32 block_offset)
{
  int sector = block_offset * NUM_SEC_PER_BLOCK;
  int data_ctr = 0;
  ready(&pci_ide);
  outb(pci_ide.bars[0] + SEC_COUNT, NUM_SEC_PER_BLOCK);  // number of sectors
  outb(pci_ide.bars[0] + SEC_NUM_LOW, sector & 0xff);
  outb(pci_ide.bars[0] + SEC_NUM_MID, (sector >> 8) & 0xff);
  outb(pci_ide.bars[0] + SEC_NUM_HIGH, (sector >> 16) & 0xff);
  outb(pci_ide.bars[0] + DRV_HEAD, SEL_DISK0_CMD | ((sector>>24)&0x0f));
  outb(pci_ide.bars[0] + STAT_CMD, BDEV_CMD_WRITE);
 
  for(int i=0; i<NUM_SEC_PER_BLOCK; ++i){    
       ready(&pci_ide);
       data_ready(&pci_ide);
       for(int j=0; j<256; j++){
	    u16 retval = *((u16 *)(buf + data_ctr));    
	    outw(pci_ide.bars[0] + OFF_DATA, retval);   
	    data_ctr += 2;
       }
       ready(&pci_ide);
       outb(pci_ide.bars[0] + STAT_CMD, BDEV_CMD_FLUSH);
  }
  check_status(&pci_ide);
  return 0;	
}
