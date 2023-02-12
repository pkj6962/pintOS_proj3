#include "page.h" 
#define MB 1048576


static unsigned
vm_hash_func (const struct hash_elem * e, void * aux UNUSED)
{
    struct vm_entry* vme = hash_entry (e, struct vm_entry, elem);
    
    return hash_int ((int)(vme->vaddr)); 
}

static bool 
vm_less_func (const struct hash_elem * a, const struct hash_elem * b, void * aux UNUSED)
{
    struct vm_entry * aa = hash_entry (a, struct vm_entry, elem);
    struct vm_entry * bb = hash_entry(b, struct vm_entry, elem); 

    if ((int)aa->vaddr < (int)bb->vaddr) return true; 
    else return false;
    //else if ((int)aa->vaddr > (int)bb->vaddr) return false;
    //TODO 값이 같으면? 
}


bool 
vm_init (struct hash * vm)
{
    return hash_init (vm, vm_hash_func, vm_less_func, NULL);  
}



struct vm_entry *   
vme_init (uint8_t type, struct file * file, off_t ofs, uint8_t * upage,
                    uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
    ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT (pg_ofs (upage) == 0);
    ASSERT (ofs % PGSIZE == 0 || read_bytes == 0); 

    struct vm_entry * vme = (struct vm_entry *)malloc (sizeof(struct vm_entry));
    if (vme == NULL)
        PANIC ("Memory Allocation for VM_entry failed "); 

    vme_field_init (vme, type, file, ofs, upage, read_bytes, zero_bytes, writable); 
    insert_vme (& thread_current ()->vm, & vme->elem); 

    return vme; 
}

void
vme_free(struct vm_entry * vme)
{
    free (vme); 
}


void 
vme_field_init (struct vm_entry * vme, uint8_t type, struct file * file, off_t ofs, uint8_t * upage,
                    uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
    vme->type = type; 
    vme->file = file;
    vme->offset = ofs;    
    vme->vaddr = upage; 
    vme->read_bytes = read_bytes;
    vme->zero_bytes = zero_bytes; 
    vme->writable = writable; 
    vme->is_loaded = false; 
}


struct vm_entry *
insert_vme (struct hash *h, struct hash_elem *new)
{
    struct hash_elem * old = hash_insert (h, new);

    return hash_entry (old, struct vm_entry, elem); 
}


struct vm_entry * 
find_vme (void * vaddr)
{
    //vaddr 가 속하는 페이지 번호 추출
    void* p_vaddr = pg_round_down(vaddr);

    // 검색을 위해 vm_entry 할당 
    struct vm_entry * key_vme = (struct vm_entry *) malloc (sizeof(struct vm_entry));
    if (key_vme == NULL)
        PANIC ("Memory Allocation Error for VM_Entry");
    key_vme->vaddr = p_vaddr; 

    // vm 테이블에서 검색 
    struct hash_elem * find = hash_find (&thread_current()->vm, &key_vme->elem);
    if (find == NULL)
        return NULL;

    // hash_entry 에서 vm_entry 로 형변환  
    struct vm_entry * vme = hash_entry(find, struct vm_entry, elem); 

    // 검색을 위해 할당한 vm_entry 할당해제
    vme_free(key_vme);    
    
    return vme; 
}


/* page fault 발생시 실행하는 함수 */
bool
handle_mm_fault (struct vm_entry * vme)
{
    // uint8_t * kpage = palloc_get_page (PAL_USER); 

    size_t swap_slot; 



    /* VM: 페이지를 할당한 후 Frame Table(list) 에 추가 */     
    struct page * pageAlloced = alloc_page (PAL_USER); 
    if (pageAlloced == NULL)
    {
        return false;  
    }
    pageAlloced->vme = vme; 
    if (pageAlloced->kaddr == NULL)
   {
        return false; 
   }
    
    /* 데이터를 읽어 들일 출처 */
    switch (vme->type)
    {
        case VM_BIN: /* 바이너리 파일이면 파일시스템으로부터 읽어 들인다.*/
            if (!load_file (pageAlloced->kaddr, vme))
            {
                return false;
            }
            break;
        case VM_ANON:  /* 스왑 디스크에 현재 데이터가 저장되어 있다.*/  
            swap_slot = vme->swap_slot;
            swap_in (swap_slot, pageAlloced->kaddr); 
            break; 
        case VM_STACK: 
            // Nothing To Do
            break; 
            // expand_stack(); 
        default: 
            PANIC ("Should not be reached yet."); 
    } 

    ASSERT (pg_ofs(pageAlloced->kaddr) == 0); 
    /* 페이지 테이블에 실제 대응되는 물리 페이지를 매핑합니다. */
    if (!install_page (vme->vaddr, pageAlloced->kaddr, vme->writable))
    {
        /* Swap: LRU List에서 페이지를 제거하고 페이지 할당해제 */
        free_page(pageAlloced->kaddr); 
        // palloc_free_page (kpage);
        return false; 
    }        


    return true; 
}

/* vme 상에 저장된 파일 메타데이터 정보를 활용하여 파일로부터 KADDR에 읽어드립니다.*/
bool 
load_file (void *kaddr, struct vm_entry *vme)
{
    /* file의 offset으로부터 read_bytes만큼 읽습니다. */    
    size_t read_bytes = file_read_at (vme->file, kaddr, vme->read_bytes, vme->offset); 
    
    /* 읽기로 되어 있는 크기와 실제로 읽은 크기가 같아야 합니다. */
    if (read_bytes != vme->read_bytes)
    {
        palloc_free_page (kaddr); 
        return false; 

    }

    /* 페이지의 읽은 부분 나머지 공간은 0으로 채웁니다.  */
    memset (kaddr + read_bytes, 0, vme->zero_bytes);
    return true; 

}

/* 
페이지 폴트가 발생한 지점이 esp가 가리키는 지점에서 32비트 이하로 떨어져 있는 경우와
스택 시작지점으로부터 8MB 이하로 떨어져 있는 경우를 동시에 만족하는 경우 스택 공간의 확장을 허용합니다.  
*/
bool 
stack_heuristic (uint8_t * esp, void * fault_addr)
{
    // printf("%p %p %p\t", esp, fault_addr, (int8_t*)PHYS_BASE - 8 * MB); 
    if (fault_addr >= esp - 32 && fault_addr > (uint8_t*)PHYS_BASE - 8 * MB)
    {
        return true; 
    }
    else 
        return false; 
}


// 해시테이블에 저장된 모든 vme를 순회합니다. 
void 
traverse_vm (struct hash *h)
{
    struct list* bucket; 
    struct list_elem *elem; 
    int bucket_cnt = h->bucket_cnt; 
    struct list *buckets = h->buckets; 

    for(int i = 0; i < bucket_cnt ; i++)
    {
     
        bucket = &buckets[i];
        for(elem = list_begin (bucket);
            elem != list_end (bucket); elem = list_next (elem))
        {
            struct hash_elem * he = list_entry (elem, struct hash_elem, list_elem);
            struct vm_entry * vme = hash_entry (he, struct vm_entry, elem); 
        }
    }
         
}

struct vm_entry * 
check_address (void * vaddr)
{
    
    if (!is_user_vaddr (vaddr))
    {
        sys_exit (-1);
    }
    return find_vme (vaddr);          
}


void 
check_valid_buffer (void * buffer, unsigned size, bool to_write)
{
    struct vm_entry * vme = check_address (buffer); 
    if (vme == NULL)
    {   
        sys_exit (-1); 
    }
    if (to_write == true && vme->writable == false)
    {
        sys_exit (-1); 
    }
    /* 사이즈에 대한 고려가 어렵다. 일단 그부분은 패스하고 나머지만 구현하도록 하자 */

}


void 
check_valid_string (const void * str)
{
    struct vm_entry * vme = find_vme(str); 
    if (vme == NULL)
    {
        sys_exit (-1); 
    }
}



/*

추가할 함수 

vm_init (struct hash * vm)

위를 만드려면

vm_hash_func()
vm_less_func() --> > 또는 < 로만 true, false가 나뉘어야 한다. 




init_vm_entry(vaddr__upage, file, offset, writable, read_bytes, zero_bytes) : load_segment에서 취할 수 있는 정보를 vm_entry에 저장함

insert_vme(struct hash * vm, struct vm_entry * vme)
delete_vme
find_vme




static unsigned vm_hash_func (const struct hash elem e, void * aux)


*/
