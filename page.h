#ifndef PAGE_H_
#define PAGE_H_

#include <hash.h>   // included for hash table
#include <debug.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "userprog/syscall.h"
#include "filesys/off_t.h"
#include "filesys/file.h"

#include "vm/frame.h"


#define VM_BIN      0
#define VM_FILE     1
#define VM_ANON     2 
#define VM_STACK    3 
#define PAGE_SIZE 4096 


/* vm_entry 정의 */
struct vm_entry
{
    uint8_t type; 
    void * vaddr;
    bool writable;
    bool is_loaded; 

    /* Meta Data */
    struct file * file; 
    size_t offset;
    size_t read_bytes; 
    /* hash table element */
    size_t zero_bytes;

    /* Swapping */
    size_t swap_slot; 

    struct hash_elem elem; 
    
}; 

/* Function Prototype */ 

bool vm_init (struct hash * vm); 

struct vm_entry * vme_init (uint8_t type, struct file * file, off_t ofs, uint8_t * upage,
                    uint32_t read_bytes, uint32_t zero_bytes, bool writable); 

void vme_free (struct vm_entry * vme); 

void vme_field_init (struct vm_entry * vme, uint8_t type, struct file * file, off_t ofs, uint8_t * upage,
                    uint32_t read_bytes, uint32_t zero_bytes, bool writable); 

struct vm_entry * insert_vme (struct hash *h, struct hash_elem *new); 


struct vm_entry * find_vme (void * vaddr); 

struct vm_entry * check_address (void * addr);

void check_valid_buffer (void * buffer, unsigned size, bool to_write); 

void check_valid_string (const void * str); 

bool stack_heuristic (uint8_t * esp, void * fault_addr); 

bool load_file (void *kaddr, struct vm_entry *vme); 

bool handle_mm_fault (struct vm_entry * vme); 

void  traverse_vm (struct hash *h); 

#endif 