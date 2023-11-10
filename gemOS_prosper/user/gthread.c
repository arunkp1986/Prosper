#include <gthread.h>
#include <ulib.h>

static struct process_thread_info tinfo __attribute__((section(".user_data"))) = {};
/*XXX Do not modifiy anything above this line*/

static void thread_end_handler() {
	
	void *return_value;
	//write inline to save rax
	asm volatile("mov %%rax,%0;"       
                     :"=m"(return_value)
		     :		
		     :"memory");
   

	int pid = getpid();
	int t_id = 0;

	for(; t_id < MAX_THREADS; t_id++)
		if(tinfo.threads[t_id].pid == pid && tinfo.threads[t_id].status == TH_STATUS_ALIVE)
			break;

        if(t_id == MAX_THREADS)
                  printf("%d: BUG!!! We should not be here\n", __LINE__);
        //printf("Thread called return tid %d pid %d\n", t_id, tinfo.threads[t_id].pid);	
	//call exit if not already called
	gthread_exit(return_value);
        printf("%d: BUG!!! We should not be here\n", __LINE__);

}

static struct thread_info* find_thread_from_pid(int pid, u8 check_alive)
{
        int ctr;  
	struct thread_info *th = &tinfo.threads[0];
		
	for(ctr=0; ctr < MAX_THREADS; ctr++, th++)
                if(th->pid == pid && (!check_alive || th->status == TH_STATUS_ALIVE))
                        return th;
        return NULL; 
}
static int reserve_tid(struct process_thread_info *tinfo) {

	int t_id = 0;
		
	for(; t_id < MAX_THREADS; t_id++)
                if(tinfo->threads[t_id].status == TH_STATUS_UNUSED)
                        break;

        if (t_id == MAX_THREADS)
                return -1;

        tinfo->threads[t_id].status = TH_STATUS_USED;
	return t_id;

}

static void init_galloc_md(struct thread_info *th)
{
   int ctr;
   struct galloc_area *ga = &th->priv_areas[0];
   for(ctr=0; ctr<MAX_GALLOC_AREAS; ++ctr, ga++)
        ga->owner = NULL;
   return;
}

/* Returns 0 on success and -1 on failure */
int gthread_create(int *tid, void *(*fc)(void *), void *arg) {
        long pid;
	printf("create called\n");
	int t_id = 0;
	u64 stack_address; 
        u64 sp;
        // check the sanity of the arguments
        if(!tid || !fc)
                  return -1;

	t_id = reserve_tid(&tinfo);
	printf("tid:%u\n",tid);
	if (t_id == -1 )     //max threads reached
		return -1;		
           	
        stack_address = (u64)mmap(NULL, TH_STACK_SIZE, PROT_READ|PROT_WRITE, 0);
        if(!stack_address)
                    return -1;  // mem allocation failed
	sp = stack_address + TH_STACK_SIZE; 
        sp -= 8;
	*(u64 *)(sp) =(u64)&thread_end_handler;  // The thread return handler
        
         
	pid = clone(fc, sp, arg);
	
        if(pid < 0){
                    printf("Either clone failed or some BUG!!\n");
                    munmap((void *)stack_address, TH_STACK_SIZE);
		    tinfo.threads[t_id].status = TH_STATUS_UNUSED;
                    return -1;
        }
       
	tinfo.threads[t_id].status = TH_STATUS_CLONE_DONE;
	tinfo.threads[t_id].pid = pid;	
	tinfo.threads[t_id].tid = t_id;
	tinfo.num_threads++;
	tinfo.threads[t_id].stack_addr = (void *)stack_address;
	*tid = t_id;		
        init_galloc_md(&tinfo.threads[t_id]);
	tinfo.threads[t_id].status = TH_STATUS_ALIVE;
	make_thread_ready(pid);
        return 0;
}

int gthread_exit(void *retval) {

	int pid = getpid();
	int t_id;
	
	for(t_id = 0; (t_id < MAX_THREADS); t_id++)
		if(tinfo.threads[t_id].pid == pid && tinfo.threads[t_id].status == TH_STATUS_ALIVE)
			break;
        
        if(t_id == MAX_THREADS)
                  printf("%d: BUG!!! We should not be here\n", __LINE__);
	
	tinfo.threads[t_id].ret_addr = retval;
	tinfo.threads[t_id].status = TH_STATUS_RETURNED;
	//call exit
	exit(0);
}

void* gthread_join(int tid) {
        int thread_died = 0;
	int i = 0;
	for(; i < MAX_THREADS; i++)
		if(tinfo.threads[i].tid == tid)
			break;
	
	while(tinfo.threads[i].status == TH_STATUS_ALIVE && !thread_died) {
		thread_died = wait_for_thread(tinfo.threads[i].pid);
	}
	
	if(tinfo.threads[i].status == TH_STATUS_RETURNED || thread_died) {
		munmap(tinfo.threads[i].stack_addr, TH_STACK_SIZE);
		tinfo.threads[i].stack_addr = NULL;
		tinfo.num_threads--;
		tinfo.threads[i].status = TH_STATUS_UNUSED;
                return tinfo.threads[i].ret_addr;
	}
       return NULL;
}

struct galloc_area* find_free_galloc_md(struct thread_info *th)
{
   int ctr;
   struct galloc_area *ga = &th->priv_areas[0];
   for(ctr=0; ctr<MAX_GALLOC_AREAS; ++ctr, ga++)
        if(!ga->owner)
             return ga;
   return NULL; 
}

struct galloc_area* find_galloc_md(struct thread_info *th, u64 address)
{
   int ctr;
   struct galloc_area *ga = &th->priv_areas[0];
   for(ctr=0; ctr<MAX_GALLOC_AREAS; ++ctr, ga++)
        if(ga->owner == th && ga->start == address)
             return ga;
   return NULL; 
}

/*Only threads will invoke this. No need to check*/
void* gmalloc(u32 size, u8 alloc_flag)
{
   u32 page_aligned_size;
   u32 prot_flags = PROT_READ|PROT_WRITE;

   void *ptr = NULL;
   struct thread_info *thinfo = find_thread_from_pid(getpid(), 1);
   struct galloc_area *ga = find_free_galloc_md(thinfo);
   
   if(size > GALLOC_MAX || !thinfo || !ga)
          return NULL;
   
   switch(alloc_flag){
      case GALLOC_OWNONLY:
                          prot_flags |= TP_SIBLINGS_NOACCESS;
                          break;
      case GALLOC_OTRDONLY:
                          prot_flags |= TP_SIBLINGS_RDONLY;
                          break;
      case GALLOC_OTRDWR:
                          prot_flags |= TP_SIBLINGS_RDWR;
                          break;
      default:
                          return NULL; 
   }
   
   page_aligned_size = (size >> 12) << 12;

   if(page_aligned_size != size)
      page_aligned_size += 4096;

   ptr = mmap(NULL, page_aligned_size, prot_flags, MAP_TH_PRIVATE);
   if(ptr == MAP_ERR)
         return NULL;
   ga->owner = thinfo;
   ga->start = (u64) ptr;
   ga->length = page_aligned_size;
   ga->flags = alloc_flag;
   return ptr;
}
/*
   Only threads will invoke this. No need to check.
   Parent can free the area allocated used gfree only if the thread
   has finished without calling gfree.
*/
int gfree(void *ptr)
{
   struct thread_info *tinfo = find_thread_from_pid(getpid(), 1);
   struct galloc_area *ga = find_galloc_md(tinfo, (u64) ptr);
   if(!ga)
        return -1;
   munmap(ptr, ga->length);
   ga->owner = NULL;
   return 0;
}
