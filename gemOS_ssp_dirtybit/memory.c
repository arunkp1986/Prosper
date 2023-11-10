#include<lib.h>
#include<memory.h>
#include<page.h>
#include<entry.h>
#include <nonvolatile.h>

static struct page_list* pglists_new[MAX_REG];
static struct page_list pglists[MAX_REG];
static unsigned long osalloc_base[MAX_REG];
static struct nodealloc_memory nodemem;
static void init_osalloc_chunks(u32 region);
void init_os_chunk_alloc(u32 region);
unsigned long * saved_ebp_backup;
struct list_head *ssp_bitmap_list = NULL;

#define pgl_bmap_set(pgl, i)  do{ \
                                         u32 lword = i >> 5;\
                                         u32 bit = i - (lword << 5);\
                                         u32 *p = (u32 *)(pgl->bitmap + (lword << 2));\
                                         set_bit(*p , bit);\
                                    }while(0);

#define pgl_bmap_clear(pgl, i)  do{ \
                                         u32 lword = i >> 5;\
                                         u32 bit = i - (lword << 5);\
                                         u32 *p = (u32 *)(pgl->bitmap + (lword << 2));\
                                         clear_bit(*p , bit);\
                                    }while(0);

extern void* osmap(u64 pfn)
{
     return ((void *)(pfn << PAGE_SHIFT));  
}

static void init_pagelist(int type, u64 start, u64 end)
{
     u32 pages_to_reserve, i;
     u32 bytes_page_bitmap;
     struct page_list *pgl;
     if( type == NVM_META_REG || type == NVM_USER_REG ){
         pgl = pglists_new[type];
     }
     else{
         pgl = &pglists[type];
     }
     pgl->size = (end - start) >> PAGE_SHIFT;
     pgl->type = type;
     pgl->start_address = start;
     pgl->last_free_pos = 0;
     bytes_page_bitmap = (pgl->size >> 3); //bytes requires to save bitmap
     if(((bytes_page_bitmap >> PAGE_SHIFT) << PAGE_SHIFT) == bytes_page_bitmap)
           pages_to_reserve = bytes_page_bitmap >> PAGE_SHIFT;
     else
           pages_to_reserve = (bytes_page_bitmap >> PAGE_SHIFT) + 1;
    
     pgl->bitmap = (char *)pgl->start_address;
     pgl->last_free_pos = pages_to_reserve >> 5;//bitmap locations corresponding to actual data page.
     for(i=0; i<pages_to_reserve; ++i)
           pgl_bmap_set(pgl, i);
           
    pgl->free_pages = pgl->size - pages_to_reserve;
    if( type == NVM_META_REG || type == NVM_USER_REG ){
        clflush_multiline((u64)pgl, sizeof(struct page_list));
        //clflush_multiline((u64)pgl->bitmap, bytes_page_bitmap);
    }
    return;
}

struct list_head * os_chunk_alloc_list;

void init_os_chunk_alloc_list(){
    init_list_head(os_chunk_alloc_list);
    clflush_multiline((u64)os_chunk_alloc_list,sizeof(struct list_head));
}

static void init_pagelists()
{
   init_pagelist(OS_DS_REG, REGION_OS_DS_START, REGION_OS_PT_START); 
   init_pagelist(OS_PT_REG, REGION_OS_PT_START, REGION_USER_START);
   init_pagelist(USER_REG,  REGION_USER_START,  REGION_FILE_DS_START);


   //File Management
   init_pagelist(FILE_DS_REG,  REGION_FILE_DS_START, REGION_FILE_STORE_START);
   init_pagelist(FILE_STORE_REG,  REGION_FILE_STORE_START, ENDMEM);
   
   if(HYBRID){
   //NVM
       char magic[6];
       memcpy((char*)magic,(char*)NVM_USER_REG_MAGIC,5);
       magic[5] = '\0';
       if(!strcmp(magic,"GemOS")){
        //not first time boot
           printk("magic:%s\n",magic);
	   saved_state_list = (struct list_head*)(NVM_USER_REG_MAGIC+32);
           //os_chunk_alloc_list = (struct list_head*)(NVM_USER_REG_MAGIC+32+sizeof(struct list_head));
           u64 os_chunk_alloc_list_addr = (u64)(NVM_USER_REG_MAGIC+32+sizeof(struct list_head));
           os_chunk_alloc_list = (struct list_head*)(os_chunk_alloc_list_addr);
	   u64 pglists_addr = (u64)(os_chunk_alloc_list_addr+sizeof(struct list_head));
	   pglists_new[NVM_META_REG] = (struct page_list*)pglists_addr;
	   pglists_addr += sizeof(struct page_list);
	   pglists_new[NVM_USER_REG] = (struct page_list*)pglists_addr;
	   saved_ebp_backup = (unsigned long*)(pglists_addr+sizeof(struct page_list));
	   extern unsigned long saved_ebp;
	   saved_ebp = *(unsigned long*)saved_ebp_backup;
	   //printk("saved state prev:%x, next:%x\n",saved_state_list->prev,saved_state_list->next);
       }
       else{
           memcpy((char*)NVM_USER_REG_MAGIC,"GemOS",5);
           saved_state_list = (struct list_head*)(NVM_USER_REG_MAGIC+32);
	   u64 os_chunk_alloc_list_addr = (u64)(NVM_USER_REG_MAGIC+32+sizeof(struct list_head));
           os_chunk_alloc_list = (struct list_head*)(os_chunk_alloc_list_addr);
	   u64 pglists_addr = (u64)(os_chunk_alloc_list_addr+sizeof(struct list_head));
	   pglists_new[NVM_META_REG] = (struct page_list*)pglists_addr;
	   pglists_addr += sizeof(struct page_list);
	   pglists_new[NVM_USER_REG] = (struct page_list*)pglists_addr;
	   saved_ebp_backup = (unsigned long*)(pglists_addr+sizeof(struct page_list));
	   init_pagelist(NVM_META_REG, NVM_META_REG_START, NVM_USER_REG_START);
           init_pagelist(NVM_USER_REG, NVM_USER_REG_START, REGION_NVM_ENDMEM);
	   init_os_chunk_alloc_list();
	   init_os_chunk_alloc(NVM_META_REG);
	   init_saved_state_list();
       }
   }
}

static void init_pfn_info_alloc_memory(){
    int i;
    u32 startpfn, prevpfn, currentpfn;
    u32 num_pages;

    startpfn = os_pfn_alloc(OS_PT_REG);

    prevpfn = startpfn;

    num_pages = ((sizeof(struct pfn_info)*NUM_PAGES) >> PAGE_SHIFT)+1;

    for(i=0; i< num_pages; i++){
        currentpfn = os_pfn_alloc(OS_PT_REG);
        if(currentpfn != prevpfn+1)
            printk("Error in pfn info memory allocation\n");
        prevpfn = currentpfn;
    }
    list_pfn_info.start = (void *)((u64)startpfn << PAGE_SHIFT);
    list_pfn_info.end = list_pfn_info.start + (NUM_PAGES << PAGE_SHIFT);
}

static u32 get_free_pages(struct page_list *pgl,u32 num)
{
  u32 pfn, bit;
  u32 first_pfn = 0;
  u32 size_in_u32 = pgl->size >> 5, once = 0; //changed to 5 from 8
  u32 *scanner = ((u32 *)(pgl->bitmap)) + pgl->last_free_pos;
  u32 initial_free_pos = pgl->last_free_pos;
  u32 start_free_pos = 0;
  u32 start_bit = 0;
  u32 found = 0;
  u32 loop_index = 0;
  while(pgl->last_free_pos < size_in_u32){
      if(*scanner == 0xffffffff){
	 found = 0;
         pgl->last_free_pos++;
         scanner++;
	 continue;
      }
      //printk("found:%u\n",found);
      for(bit=0; bit<32; ){
          if(!is_set(*scanner, bit)){
	      if(!start_free_pos){
                  start_free_pos = pgl->last_free_pos;
		  start_bit = bit;
	      }
	      found += 1;
              for(loop_index=(bit+1); loop_index<32; loop_index++){
                  if(!is_set(*scanner, loop_index)){
                      found += 1;
		      if(found == num)
                          break;
		  }
	          else{
	              found = 0;
		      start_free_pos = 0;
		      break;
		  }
	      }
	      bit = (loop_index+1);
	  }
	  else{
	      bit += 1;
	  }
          if(found == num)
              break;
      }
      if(found == num){
          break;
      }
      else{
          pgl->last_free_pos++;
          scanner++;
      }
  }
  //start scanning from begining
  if(pgl->last_free_pos >= size_in_u32){
        pgl->last_free_pos = 0;
	start_free_pos = 0;
	found = 0;
        scanner = ((u32 *)(pgl->bitmap)) + pgl->last_free_pos;
        while(pgl->last_free_pos < initial_free_pos){
            if(*scanner == 0xffffffff){
               found = 0;
               pgl->last_free_pos++;
               scanner++;
	       continue;
	    }
            for(bit=0; bit<32; ){
                if(!is_set(*scanner, bit)){
                    if(!start_free_pos){
                        start_free_pos = pgl->last_free_pos;
			start_bit = bit;
		    }
	            found += 1;
                    for(loop_index=(bit+1); loop_index<32; loop_index++){
                        if(!is_set(*scanner, bit+loop_index)){
                            found += 1;
			    if(found == num)
                                break;
			}
	                else{
	                    found = 0;
			    start_free_pos = 0;
		            break;
			}
		    }
	            bit = (loop_index+1);
		}else{
		    bit += 1;
		}
                if(found == num)
                    break;
	    }
            if(found == num){
                break;
	    }
            else{
                pgl->last_free_pos++;
                scanner++;
	    }
	}
  }
  if (found != num){
      printk("no free memory\n");
      printk("blindly passing initial pfn, but memory corruption will happen\n");
      pgl->last_free_pos = 0;
      start_free_pos = pgl->last_free_pos;
      scanner = ((u32 *)(pgl->bitmap)) + pgl->last_free_pos;
      for(bit=0; bit<32; ++bit){
          if(!is_set(*scanner, bit)){
              start_bit = bit;
              break;
          }
      }
  } 
  for(loop_index=0; loop_index<num; loop_index++){
      pfn = (start_free_pos << 5) + (start_bit+loop_index);
      if(!first_pfn){
          first_pfn = pfn;
          //printk("first pfn:%lx\n",pfn);
      }
      //printk("pfn:%lx\n",pfn);
      pgl_bmap_set(pgl, pfn);
      pgl->free_pages--;
  }
  //printk("last pfn:%lx\n",pfn);
  first_pfn += pgl->start_address >> PAGE_SHIFT;
  
//  printk("%s:%x\n", __func__, pfn);
  //printk("before return get_free_pages\n");
  return first_pfn;
}


static u32 get_free_page(struct page_list *pgl)
{
  u32 pfn, bit;
  u32 size_in_u32 = pgl->size >> 5, once = 0; //changed to 5 from 8
  u32 *scanner = ((u32 *)(pgl->bitmap)) + pgl->last_free_pos;
  u32 initial_free_pos = pgl->last_free_pos;
  u32 found = 0;

  
  /*while(*scanner == 0xffffffff && pgl->last_free_pos < size_in_u32){
         pgl->last_free_pos++;
         scanner++;
  }*/
  while(pgl->last_free_pos < size_in_u32){
      if(*scanner == 0xffffffff){
         found = 0;
         pgl->last_free_pos++;
         scanner++;
         continue;
      }else{
         found = 1;
         break;
      }
  }

  if(pgl->last_free_pos >= size_in_u32){
      pgl->last_free_pos = 0;
      scanner = ((u32 *)(pgl->bitmap)) + pgl->last_free_pos;
      while(pgl->last_free_pos < initial_free_pos){
          if(*scanner == 0xffffffff){
              found = 0;
              pgl->last_free_pos++;
              scanner++;
              continue;
          }else{
              found = 1;
              break;
	  }
      }
   }
  if (!found){
      printk("no free memory\n");
      printk("get free page blindly passing initial pfn, but memory corruption will happen\n");
      pgl->last_free_pos = 0;
      scanner = ((u32 *)(pgl->bitmap)) + pgl->last_free_pos;
  }
  
  for(bit=0; bit<32; ++bit){
      if(!is_set(*scanner, bit))
          break;
  }     
  
  pfn = (pgl->last_free_pos << 5) + bit;
  pgl_bmap_set(pgl, pfn);
  pfn += pgl->start_address >> PAGE_SHIFT;
  pgl->free_pages--;
  
//  printk("%s:%x\n", __func__, pfn);
  return pfn;
}

u32 os_pfns_alloc(u32 region, u32 num)
{
    u32 pfn;
    u32 loop_index = 0;
    struct page_list *pgl;
    if( region == NVM_META_REG || region == NVM_USER_REG ){
        pgl = pglists_new[region];
    }
    else{
        pgl = &pglists[region];
    }

    if(pgl->free_pages <= 0){
         printk("Opps.. out of memory for region %d\n", region);
         return 0;
    }
   pfn = get_free_pages(pgl, num);
   if((region == OS_PT_REG)){
       for(loop_index = 0; loop_index < num; loop_index++){
           u64 addr = pfn+loop_index;
           addr = addr << PAGE_SHIFT;
	   //printk("address:%x\n",addr);
           bzero((char *)addr, PAGE_SIZE);
       }
   }
   /*if(region == NVM_META_REG){
      printk("num:%u, first:%lx, last:%lx\n", num, pfn<< PAGE_SHIFT, (pfn+num)<<PAGE_SHIFT);
   }*/
   
   if(region == USER_REG){
       set_pfn_info(pfn);
       stats->user_reg_pages++;
   }
   //printk("before return os_pfns_alloc\n");
   //u32 bytes_page_bitmap = (pgl->size >> 3); //bytes requires to save bitmap
   if( region == NVM_META_REG || region == NVM_USER_REG ){
       clflush_multiline((u64)pgl, sizeof(struct page_list));
       //clflush_multiline((u64)pgl->bitmap, bytes_page_bitmap);
   }
   return pfn;
}

u32 os_pfn_alloc(u32 region)
{
    u32 pfn;
    u32 pfn_ssp;
    extern struct ssp_entry* ptr_dirtymap;
    struct page_list *pgl;
    if( region == NVM_META_REG || region == NVM_USER_REG ){
        pgl = pglists_new[region];
    }
    else{
        pgl = &pglists[region];
    }

    if(pgl->free_pages <= 0){
         printk("Opps.. out of memory for region %d\n", region);
         return 0;
    }
   pfn = get_free_page(pgl);

   if(region == NVM_USER_REG && ptr_dirtymap ){
       //printk(" region:%d, pfn:%lx, start:%lx\n",region, (((unsigned long) pfn) << PAGE_SHIFT), NVM_USER_REG_START);
       unsigned long offset = ((((unsigned long) pfn) << PAGE_SHIFT)-NVM_USER_REG_START)>>PAGE_SHIFT;
       //printk("offset:%lx\n",offset);
       //printk("ptr_dirtymap:%lx\n",(unsigned long)ptr_dirtymap);
       struct ssp_entry* bitmap_entry = (struct ssp_entry*)((unsigned long)ptr_dirtymap+
		       offset*sizeof(struct ssp_entry));
       struct ssp_commit_entry *commit_entry = (struct ssp_commit_entry*)os_chunk_alloc(
		       sizeof(struct ssp_commit_entry),NVM_META_REG);
       bitmap_entry->p0 = (((unsigned long) pfn) << PAGE_SHIFT);
       commit_entry->p0 = (((unsigned long) pfn) << PAGE_SHIFT);
       pgl = pglists_new[NVM_USER_REG];
       pfn_ssp = get_free_page(pgl);
       bitmap_entry->p1 = (((unsigned long) pfn_ssp) << PAGE_SHIFT);
       commit_entry->p1 = (((unsigned long) pfn_ssp) << PAGE_SHIFT);
       //printk("p0: %lx, p1: %lx\n",bitmap_entry->p0,bitmap_entry->p1);
       bitmap_entry->updated_bitmap = 0;
       bitmap_entry->current_bitmap = 0;
       bitmap_entry->evicted = 0; //this is set on TLB eviction
       commit_entry->commit_bitmap = 0; //at end_checkpoint, apply updated bitmap here
       //clflush_multiline((u64)bitmap_entry, sizeof(struct ssp_entry));
       clflush_multiline((u64)commit_entry, sizeof(struct ssp_commit_entry));
       struct ssp_bitmap_entry *ssp_list_entry = (struct ssp_bitmap_entry*)os_chunk_alloc(sizeof(struct ssp_bitmap_entry),NVM_META_REG);
       if(!ssp_list_entry){
           printk("Error creating ssp bitmap entry\n");
       }
       ssp_list_entry->entry = bitmap_entry;
       ssp_list_entry->commit_entry = commit_entry;
       clflush_multiline((u64)ssp_list_entry, sizeof(struct ssp_bitmap_entry));
       if(ssp_bitmap_list)
           list_add_tail(&ssp_list_entry->list,ssp_bitmap_list);
   }
   if(region == OS_PT_REG){
       u64 addr = pfn;
       addr = addr << PAGE_SHIFT;
       bzero((char *)addr, PAGE_SIZE);
   }
   if(region == USER_REG){
       set_pfn_info(pfn);
       stats->user_reg_pages++;
   }
   //u32 bytes_page_bitmap = (pgl->size >> 3); //bytes requires to save bitmap
   if( region == NVM_META_REG || region == NVM_USER_REG ){
       clflush_multiline((u64)pgl, sizeof(struct page_list));
       //clflush_multiline((u64)pgl->bitmap, bytes_page_bitmap);
   }

   return pfn;
}
void *os_page_alloc(u32 region)
{
   u32 pfn = os_pfn_alloc(region);
   return ((void *) (((unsigned long) pfn) << PAGE_SHIFT));
}

void *os_page_alloc_pages(u32 region, u32 num){
   if(num == 1)
       return os_page_alloc(region);

   u32 pfn = os_pfns_alloc(region, num);
   //printk("before return os_page_alloc_pages\n");
   return ((void *) (((unsigned long) pfn) << PAGE_SHIFT));

}

void os_pages_free(u32 region, void *paddress, u32 num)
{
    struct page_list *pgl;
    if( region == NVM_META_REG || region == NVM_USER_REG ){
        pgl = pglists_new[region];
    }
    else{
        pgl = &pglists[region];
    }
    u32 pfn = ((unsigned long)paddress - pgl->start_address) >> PAGE_SHIFT;
    u32 loop_index = 0;
    if((u64)paddress <= pgl->start_address || 
        (u64)paddress >= (pgl->start_address + (pgl->size << 12))){
         printk("Opps.. free address not in region %d\n", region);
         return;
    }
   for(loop_index = 0; loop_index < num; loop_index++){
       pgl_bmap_clear(pgl, pfn+loop_index); 
       pgl->free_pages++;
   }
   
   //u32 bytes_page_bitmap = (pgl->size >> 3); //bytes requires to save bitmap
   if( region == NVM_META_REG || region == NVM_USER_REG ){
       clflush_multiline((u64)pgl, sizeof(struct page_list));
       //clflush_multiline((u64)pgl->bitmap, bytes_page_bitmap);
   }

   return;
}


void os_page_free(u32 region, void *paddress)
{
    struct page_list *pgl;
    if( region == NVM_META_REG || region == NVM_USER_REG ){
        pgl = pglists_new[region];
    }
    else{
        pgl = &pglists[region];
    }
    u32 pfn = ((unsigned long)paddress - pgl->start_address) >> PAGE_SHIFT;

    if((u64)paddress <= pgl->start_address || 
        (u64)paddress >= (pgl->start_address + (pgl->size << 12))){
         printk("Opps.. free address [%x][%x] not in region %d\n",paddress, pgl->start_address,region);
         return;
    }
   pgl_bmap_clear(pgl, pfn); 
   pgl->free_pages++;
   //u32 bytes_page_bitmap = (pgl->size >> 3); //bytes requires to save bitmap
   if( region == NVM_META_REG || region == NVM_USER_REG ){
       clflush_multiline((u64)pgl, sizeof(struct page_list));
       //clflush_multiline((u64)pgl->bitmap, bytes_page_bitmap);
   }

   return;
}
void os_pfn_free(u32 region, u64 pfn)
{
   os_page_free(region, (void *) (pfn << PAGE_SHIFT));
    if(region == USER_REG){
       reset_pfn_info(pfn);
       stats->user_reg_pages--;
   }
}

void os_pfns_free(u32 region, u64 pfn, u32 num)
{
   u32 loop_index = 0;
   os_pages_free(region, (void *) (pfn << PAGE_SHIFT), num);
    if(region == USER_REG){
       for(loop_index = 0; loop_index < num; loop_index++){
           reset_pfn_info(pfn+loop_index);
           stats->user_reg_pages--;
       }
   }
}


static void init_osalloc_chunks(u32 region)
{
     struct osalloc_chunk *ck;
     int shift = 5, i;
     int numbytes = sizeof(struct osalloc_chunk) * OSALLOC_MAX;
     if(numbytes > PAGE_SIZE){
          printk("Reduce number of OSALLOC types\n");
          return;
     }   
   
    ck = os_page_alloc(region);
    osalloc_base[region] = (unsigned long) ck;
    for(i=0; i<OSALLOC_MAX; ++i){
         int bmap_entries = PAGE_SIZE >> shift;
         ck->chunk_size = 1 << shift;
         ck->free_position = 0;
         init_list(&ck->freelist);
         bzero(ck->bitmap, 16);
         ck->current_pfn = os_pfn_alloc(region);
         shift++;
         ck++;
    }
   
    return;  
}
/*
void init_os_chunk_alloc(u32 region){
    struct osalloc_nugget * ch_new = (struct osalloc_nugget*)os_page_alloc(region);
    ch_new->is_full = 0;
    ch_new->chunk_size = 32;
    ch_new->current_pfn = os_pfn_alloc(region);
    for(int i=0; i<4; i++){
        *((u32*)ch_new->bitmap+i) |= ~(1UL<<32);
    }
    list_add_tail(&ch_new->list,&os_chunk_alloc_list);
}*/


void init_os_chunk_alloc(u32 region){
    struct osalloc_nugget_meta * n_meta = (struct osalloc_nugget_meta*)os_page_alloc(region);
    n_meta->region = region;
    for(int loop_index_one=0; loop_index_one<7; loop_index_one++){
        n_meta->nugget_size[loop_index_one] = (1<<(5+loop_index_one));
	init_list_head(&n_meta->size_list[loop_index_one]);
        struct osalloc_nugget * ch_new = (struct osalloc_nugget*)os_page_alloc(region);
        ch_new->is_full = 0;
        ch_new->chunk_size = (1<<(5+loop_index_one));
        ch_new->current_pfn = os_pfn_alloc(region);

        for(int loop_index_two=0; loop_index_two<4; loop_index_two++){
            *((u32*)ch_new->bitmap+loop_index_two) |= ((1UL<<32)-1);
        }
        list_add_tail(&ch_new->list,&n_meta->size_list[loop_index_one]);
	if(region == NVM_META_REG){
	    clflush_multiline((u64)ch_new,sizeof(struct osalloc_nugget));
	    clflush_multiline((u64)&n_meta->size_list[loop_index_one],sizeof(struct list_head));
	}
    }
    list_add_tail(&n_meta->list, os_chunk_alloc_list);
    if(region == NVM_META_REG){
        clflush_multiline((u64)n_meta, sizeof(struct osalloc_nugget_meta));
	clflush_multiline((u64)os_chunk_alloc_list, sizeof(struct list_head));
    }
}
/*This allocates a new memory nugget to handle allocation
 * requests*/
struct osalloc_nugget * get_new_os_chunk(u32 size, u32 region){
    int log_size = 0;
    int st_size = sizeof(struct osalloc_nugget);
    int temp_st_size = st_size;
    
    while(temp_st_size>>1 >0){
        log_size +=1;
	temp_st_size = temp_st_size>>1;
    }
    
    if((1 << log_size) < st_size){
        ++log_size;
    }
    u8 found_size = 0;
    u8 found_region = 0; 
    u32 num_nugget_structs = PAGE_SIZE>>log_size;
    struct osalloc_nugget * ch_new;
    struct list_head* nug_mhead_temp;
    struct osalloc_nugget_meta *nug_meta_temp;
    list_for_each(nug_mhead_temp, os_chunk_alloc_list){
        nug_meta_temp = list_entry(nug_mhead_temp, struct osalloc_nugget_meta, list);
	if(nug_meta_temp->region == region){
            for(int loop_index_one=0; loop_index_one<7; loop_index_one++){
	        if(nug_meta_temp->nugget_size[loop_index_one] == size){
		    struct list_head *nug_temp = (nug_meta_temp->size_list[loop_index_one]).next;
		    u32 pos = 1;
                    while(nug_temp->next != &nug_meta_temp->size_list[loop_index_one]){
                        pos +=1;
	                nug_temp = nug_temp->next;
		    }
                    if(pos%num_nugget_structs != 0){
	                ch_new = (struct osalloc_nugget*)((u64)list_entry(nug_temp, struct osalloc_nugget, list)+
				       sizeof(struct osalloc_nugget));
                        ch_new->is_full = 0;
                        ch_new->chunk_size = size;
                        ch_new->current_pfn = os_pfn_alloc(region);
                        for(int loop_index_two=0; loop_index_two<4; loop_index_two++){
                            *((u32*)ch_new->bitmap+loop_index_two) |= ((1UL<<32)-1);
	                }
                        list_add_tail(&ch_new->list,&nug_meta_temp->size_list[loop_index_one]);
			if( region == NVM_META_REG ){
		            clflush_multiline((u64)ch_new, sizeof(struct osalloc_nugget));
		            clflush_multiline((u64)&nug_meta_temp->size_list[loop_index_one], sizeof(struct list_head));
			}
			return ch_new;
		    }
                    else{
                        ch_new = (struct osalloc_nugget*)os_page_alloc(region);
	                if(!ch_new){
	                    printk("get_new_os_chunk failed\n");
	                    return NULL;
		        }
	                ch_new->is_full = 0;
                        ch_new->chunk_size = size;
                        ch_new->current_pfn = os_pfn_alloc(region);
                        for(int loop_index_two=0; loop_index_two<4; loop_index_two++){
                            *((u32*)ch_new->bitmap+loop_index_two) |= ((1UL<<32)-1);
	                }
                        list_add_tail(&ch_new->list,&nug_meta_temp->size_list[loop_index_one]);
			if( region == NVM_META_REG ){
		            clflush_multiline((u64)ch_new, sizeof(struct osalloc_nugget));
		            clflush_multiline((u64)&nug_meta_temp->size_list[loop_index_one], sizeof(struct list_head));
			}
			return ch_new;
		    }
		    found_size = 1;
		}
	    }
	    found_region = 1;
	}
    }
    if(!found_size || !found_region){
        printk(" memory allocation failed at line:%d, file:%s\n",__LINE__,__FILE__);
    }
    return NULL;
}

/*
        u32 pos = 1;
    while(temp->next != &os_chunk_alloc_list){
        pos +=1;
	temp = temp->next;
    }
    if(pos%num_chunks != 0){
	ch_new = (struct osalloc_nugget*)((u64)list_entry(temp, struct osalloc_nugget, list)+\
			sizeof(struct osalloc_nugget));
        ch_new->is_full = 0;
        ch_new->chunk_size = size;
        ch_new->current_pfn = os_pfn_alloc(region);
        for(int i=0; i<4; i++){
            *((u32*)ch_new->bitmap+i) |= ~(1UL<<32);
	}
        list_add_tail(&ch_new->list,&os_chunk_alloc_list);
    }
    else{
        ch_new = (struct osalloc_nugget*)os_page_alloc(region);
	if(!ch_new){
	    printk("get_new_os_chunk failed\n");
	    return NULL;
	}
	ch_new->is_full = 0;
        ch_new->chunk_size = size;
        ch_new->current_pfn = os_pfn_alloc(region);
        for(int i=0; i<4; i++){
            *((u32*)ch_new->bitmap+i) |= ~(1UL<<32);
	}
        list_add_tail(&ch_new->list,&os_chunk_alloc_list);
    }
    return ch_new;
}*/

/*Allocates nugget of memory in 32, 64, .. sizes
 * the bitmap location is set to 1 indicates the chunk is
 * free for allocation.*/
u64 os_chunk_alloc(u32 size, u32 region){
    u64 retval;
    int log_size = 0;
    if(!size || size > 2048)
       return 0;

    for(log_size=11; log_size>5; --log_size){
        if(is_set(size, log_size)){
            break;
	}
    }
    if((1 << log_size) < size){
        ++log_size;
    }

    u32 num_bits = PAGE_SIZE>>log_size;
    u32 alloc_size = (1<<log_size);
    struct osalloc_nugget * ch_new;
    struct list_head* nug_mhead_temp;
    struct osalloc_nugget_meta *nug_meta_temp;
    struct list_head * nug_head_temp;
    u8 found_region = 0;
    u8 found_size = 0;
    list_for_each(nug_mhead_temp, os_chunk_alloc_list){
        nug_meta_temp = list_entry(nug_mhead_temp, struct osalloc_nugget_meta, list);
	if(nug_meta_temp->region == region){
            for(int loop_index_one=0; loop_index_one<7; loop_index_one++){
	        if(nug_meta_temp->nugget_size[loop_index_one] == alloc_size){
		    list_for_each(nug_head_temp, &nug_meta_temp->size_list[loop_index_one]){
		        ch_new = list_entry(nug_head_temp, struct osalloc_nugget, list);
                        if(!ch_new->is_full){
		            int loop_counter = 0;
                            while( loop_counter < num_bits){
                                u32 * bitmap_loc = (u32*)ch_new->bitmap+(loop_counter>>5); 
                                if( *bitmap_loc & 1UL<<(31-(loop_counter%(1<<5)))){
			            *bitmap_loc &= ~(1UL<<(31-(loop_counter%(1<<5))));
				    if( region == NVM_META_REG ){
				        clflush_multiline((u64)ch_new, sizeof(struct osalloc_nugget));
				    }
		                    retval = (u64)(ch_new->current_pfn<<PAGE_SHIFT)+(loop_counter*alloc_size);
			            return retval;
			        }
                                loop_counter += 1;
			    }
		            ch_new->is_full = 1;
			    if( region == NVM_META_REG ){
                                clflush_multiline((u64)ch_new, sizeof(struct osalloc_nugget));
			    }
			}
		    }
                    ch_new = get_new_os_chunk(alloc_size, region);
                    if(!ch_new){
                        printk("get_new_os_chunk failed\n");
	                return 0;
		    }
                    if(!ch_new->is_full){
                        int loop_counter = 0;
                        while( loop_counter < num_bits){
                            u32* bitmap_loc = (u32*)ch_new->bitmap+(loop_counter>>5);
                            if( *bitmap_loc & 1UL<<(31-(loop_counter%(1<<5)))){
                                *bitmap_loc &= ~(1UL<<(31-(loop_counter%(1<<5))));
				if( region == NVM_META_REG ){
				    clflush_multiline((u64)ch_new, sizeof(struct osalloc_nugget));
				}
		                retval = (u64)(ch_new->current_pfn<<PAGE_SHIFT)+(loop_counter*alloc_size);
		                return retval;
			    }
	                    loop_counter += 1;
			}
                        ch_new->is_full = 1;
			if( region == NVM_META_REG ){
		            clflush_multiline((u64)ch_new, sizeof(struct osalloc_nugget));
			}
		    }
                    else{
                        printk("chunk alloc Bug!!, new allocated node should not be full\n");
		    }
		    found_size = 1;
		}
	    }
	    found_region = 1;    
	}
    }
    if(!found_region || !found_size){
        printk("region:%u or size not found %s\n",region,__func__);
    }
    return 0;
}

/*
    list_for_each(temp,&os_chunk_alloc_list){
        os_ch = list_entry(temp, struct osalloc_nugget, list);
        if(os_ch->chunk_size == size){
            if(!os_ch->is_full){
		int loop_counter = 0;
                while( loop_counter < num_bits){
                    u32 * bitmap_loc = (u32*)os_ch->bitmap+(loop_counter>>5); 
                    if( *bitmap_loc & 1UL<<(31-(loop_counter%(1<<5)))){
			*bitmap_loc &= ~(1UL<<(31-(loop_counter%(1<<5))));
		        retval = (u64)(os_ch->current_pfn<<PAGE_SHIFT)+(loop_counter*size);
			return retval;
		    }
                    loop_counter += 1;
		}
		os_ch->is_full = 1;
	    }
	}
    }
    os_ch = get_new_os_chunk(size, region);
    if(!os_ch){
        printk("os_chunk_alloc failed\n");
	return 0;
    }
    if(!os_ch->is_full){
        int loop_counter = 0;
        while( loop_counter < num_bits){
            u32* bitmap_loc = (u32*)os_ch->bitmap+(loop_counter>>5);
            if( *bitmap_loc & 1UL<<(31-(loop_counter%(1<<5)))){
                 *bitmap_loc &= ~(1UL<<(31-(loop_counter%(1<<5))));
		retval = (u64)(os_ch->current_pfn<<PAGE_SHIFT)+(loop_counter*size);
		return retval;
	    }
	    loop_counter += 1;
	}
	os_ch->is_full = 1;
    }
    else{
        printk("chunk alloc Bug!!, new allocated node should not be full\n");
    }
    return 0;
}*/

u8 os_chunk_free(u32 region, u64 address, u32 size){
    u32 pfn = (address>>PAGE_SHIFT);

    int log_size = 0;
    if(!size || size > 2048)
       return 0;

    for(log_size=11; log_size>5; --log_size){
        if(is_set(size, log_size)){
            break;
	}
    }
    if((1 << log_size) < size){
        ++log_size;
    }

    u32 num_bits = PAGE_SIZE>>log_size;
    u32 alloc_size = (1<<log_size);
    struct osalloc_nugget * os_nug;
    struct list_head* nug_mhead_temp;
    struct osalloc_nugget_meta *nug_meta_temp;
    struct list_head * nug_head_temp;
    u8 found_region = 0;
    u8 found_size = 0;
    list_for_each(nug_mhead_temp, os_chunk_alloc_list){
        nug_meta_temp = list_entry(nug_mhead_temp, struct osalloc_nugget_meta, list);
	if(nug_meta_temp->region == region){
            for(int loop_index_one=0; loop_index_one<7; loop_index_one++){
	        if(nug_meta_temp->nugget_size[loop_index_one] == alloc_size){
		    list_for_each(nug_head_temp, &nug_meta_temp->size_list[loop_index_one]){
		        os_nug = list_entry(nug_head_temp, struct osalloc_nugget, list);
                        if(os_nug->current_pfn == pfn){
			    os_nug->is_full = 0;
                            u32 pos = ((address&0xfff)>>log_size);
	                    u32* bitmap_loc = (u32*)os_nug->bitmap+(pos>>5);
	                    /*if( *bitmap_loc & (1UL<< (31-(pos%(1<<5))))){
	                        printk("trying to free %u which is already marked free\n",(127-pos));
			    }*/
	                    *bitmap_loc |= (1UL<<(31-(pos%(1<<5))));
			    if( region == NVM_META_REG )
			        clflush_multiline((u64)bitmap_loc, sizeof(u32));
	                    return 1;
			}
		    }
		    found_size = 1;
		}
	    }
	    found_region = 1;
	}
    }
    if(!found_size){
        printk("size %u not found\n",size);
    }
    if(!found_region){
        printk("region:%u not found\n",region);
    }
    return 0;
}

/*
    list_for_each(temp,&os_chunk_alloc_list){
        os_ch = list_entry(temp, struct osalloc_nugget, list);
        if(os_ch->current_pfn == pfn){
            u32 pos = (address&~FLAG_MASK)>>i;
	    u32* bitmap_loc = (u32*)os_ch->bitmap+(pos>>5);
	    if( *bitmap_loc & (1UL<< (31-pos%(1<<5)))){
	        printk("trying to free %u which is already marked free\n",(127-pos));
	    }
	    *bitmap_loc |= (1UL<<(31-pos%(1<<5)));
	    return 1;
	}
    }
    return 0;
}*/

void *os_alloc(u32 size,u32 region)
{
  if( region == NVM_META_REG || region == NVM_USER_REG ){
      printk("Don't use os alloc for NVM regions\n");
  }
  unsigned long retval = 0;
  int i;
  struct osalloc_chunk *ck;
  u32 *ptr;
  if(!size || size > 2048)
       return NULL;
  for(i=11; i>5; --i){
     if(is_set(size, i))
          break;
  }
  
  if((1 << i) != size)
        ++i;
  ck = ((struct osalloc_chunk *) (osalloc_base[region])) + (i-5);
  if(!is_empty(&ck->freelist)){
      struct node *mem = dequeue_front(&ck->freelist);
      if(!mem){
          printk("freelist dequeue error\n");
	  goto normal;
      }
      retval = mem->value;
      node_free(mem); 
      goto out;
  } 
normal: 
  if(ck->free_position > (PAGE_SIZE >> i)){  /*All done with current pfn*/
         ck->current_pfn = os_pfn_alloc(region);
	 if(!ck->current_pfn){
		 printk("%s: Out of memory for the region %d\n", __func__, region);
		 return NULL;
	 }
         ck->free_position = 0;
  }
     
  ptr = (u32 *)(ck->bitmap) + (ck->free_position >> 5);
  retval = (((u64) ck->current_pfn << PAGE_SHIFT) + (ck->free_position << i));
  set_bit(*ptr, (ck->free_position%(1<<5)));
  ck->free_position++;
out:
  return (void *) retval;
}

void os_free(void *ptr, u32 size, u32 region)
{
  struct osalloc_chunk *ck;
  int i;
  if(!size || size > 2048)
       return;
  for(i=11; i>5; --i){
     if(is_set(size, i))
          break;
  }
  ck = ((struct osalloc_chunk *) osalloc_base[region]) + (i-5);
  if (enqueue_tail(&ck->freelist, (u64) ptr) < 0)
       printk("os free enqueue failed:%s\n",__func__);
  return;  
}

static void init_nodealloc_memory()
{
   int i;
   u32 startpfn, prevpfn, currentpfn;
 
   startpfn = os_pfn_alloc(OS_DS_REG);
   prevpfn = startpfn;
   for(i=1; i<NODE_MEM_PAGES; ++i){
        currentpfn = os_pfn_alloc(OS_DS_REG);
        if(currentpfn != prevpfn + 1)
            printk("Error in initmem %s\n", __func__);
        prevpfn = currentpfn;
        nodemem.num_free += PAGE_SIZE / sizeof(struct node);
   }
   
   nodemem.nodes = (void *) ((u64) startpfn << PAGE_SHIFT);
   nodemem.next_free = 0;     
   nodemem.end = nodemem.nodes + (NODE_MEM_PAGES << PAGE_SHIFT);
}

/*XXX*/

struct node *node_alloc()
{
   struct node *n = ((struct node *) nodemem.nodes) + nodemem.next_free;
   if(!nodemem.num_free){
       printk("node memory full\n");
       return NULL;
   }

   if(!n->next)
         goto found;

   while(n->next){
         ++n; 
         if((void *)n >= nodemem.end)
             break;
         nodemem.next_free++;
   }
   
   if((void *)n >= nodemem.end){
          nodemem.next_free = 0;
          n = ((struct node *) nodemem.nodes) + nodemem.next_free;
          while(n->next){
              ++n; 
              nodemem.next_free++;
          }
   }
found:
    nodemem.num_free--;
    nodemem.next_free++;
    return n;
       
}

void node_free(struct node *n)
{
    nodemem.num_free++;
    n->next = NULL; 
      
}
int get_free_pages_region(u32 region)
{
  if(region >= MAX_REG) 
    return -1;
  struct page_list *pgl;
  if( region == NVM_META_REG || region == NVM_USER_REG ){
      pgl = pglists_new[region];
  }
  else{
     pgl = &pglists[region];
  }
  return pgl->free_pages;  
}

int get_pages_region(u32 region)
{
  if(region >= MAX_REG) 
    return -1;
  struct page_list *pgl;
  if( region == NVM_META_REG || region == NVM_USER_REG ){
      pgl = pglists_new[region];
  }
  else{
     pgl = &pglists[region];
  }
  return pgl->size;  
}

void init_mems()
{
    init_pagelists();
    init_osalloc_chunks(OS_DS_REG);
    //init_osalloc_chunks(NVM_META_REG); 
    init_nodealloc_memory();
    init_pfn_info_alloc_memory();
}
