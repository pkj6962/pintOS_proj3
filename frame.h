#ifndef FRAME_H_
#define FRAME_H_

#include <list.h>

#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/swap.h"

struct page
{
    uint8_t * kaddr; 
    struct vm_entry * vme;
    struct thread * thread; 
    struct list_elem lru; 
};



void lru_init (void); 
 
void add_page_to_lru_list (struct page * page);

void delete_page_from_lru_list (struct page * page);

struct page * alloc_page (enum palloc_flags flags);

void free_page (void *kaddr);  

struct page * lru_list_find (void *kaddr);

void try_to_free_pages (void); 

struct page * clock_replacement(void);

struct page * give_second_chance_or_return_as_victim (struct page * page); 

void remove_pages_from_dying_thread(struct thread * t); 


#endif