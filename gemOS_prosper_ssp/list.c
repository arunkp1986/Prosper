#include <list.h>
#include <lib.h>
#include <nonvolatile.h>
#include <types.h>

void list_del(struct list_head *entry)
{
	struct list_head *prev = entry->prev;
	struct list_head *next = entry->next;
	next->prev = prev;
	prev->next = next;
	entry->next = LIST_POISON1;
	entry->prev = LIST_POISON2;
}


static inline void __list_add(struct list_head *new,
			      struct list_head *prev,
			      struct list_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
	clflush_multiline((u64)new, sizeof(struct list_head));
	clflush_multiline((u64)prev, sizeof(struct list_head));
	clflush_multiline((u64)next, sizeof(struct list_head));
}

void list_add(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}

void list_add_tail(struct list_head *new, struct list_head *head)
{
	/*if(head == checkpoint_stat_list){
            struct list_head * temp = head->next;
	    while( temp != head){
	        printk("temp:%x\t",temp);
		temp = temp->next;
	    }
	    printk("\n");
	}*/
	__list_add(new, head->prev, head);
}


