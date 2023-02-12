#include "frame.h"
#include "threads/thread.h"

static struct list lru_list;
static struct lock lru_list_lock; 
static struct page * clock_victim;  /* clock 알고리즘에서 다음번에 순회를 시작할 프레임 */
static struct lock clock_lock; 


static void 
__free_page (struct page * page); 

void 
lru_init ()
{
    list_init (&lru_list); 
    lock_init (&lru_list_lock); 
    clock_victim = NULL; 
    lock_init (&clock_lock); 
}

void 
add_page_to_lru_list (struct page * page)
{
    lock_acquire (&lru_list_lock);
    list_push_back (&lru_list, &page->lru); 
    lock_release (&lru_list_lock); 
}

void 
delete_page_from_lru_list (struct page * page)
{
    lock_acquire (&lru_list_lock);
    list_remove (&page->lru); 
    lock_release (&lru_list_lock); 

}

struct page *
alloc_page (enum palloc_flags flags)
{
    uint8_t * kpage = palloc_get_page (flags); 
    while (kpage == NULL)
    {
        try_to_free_pages (); 
        kpage = palloc_get_page (flags); 
    }

    struct page * page = (struct page *)malloc(sizeof (struct page)); 

    if (page == NULL)
    {
        palloc_free_page (kpage); 
        return NULL;  
    }
    // memset (page, 0, sizeof (struct page)); 

    page->kaddr = kpage; 
    page->thread = thread_current ();

    add_page_to_lru_list (page); 

    /* 가끔 다르더라고요? */
    ASSERT(page->kaddr == kpage); 
    

    return page; 

}


void 
free_page (void *kaddr)
{
    struct page * page = lru_list_find (kaddr); 
    if (page != NULL)
        __free_page (page);
}


static void 
__free_page (struct page * page)
{
    delete_page_from_lru_list (page); 
    palloc_free_page (page->kaddr); 
    free (page); 
    
}


/* kaddr에 해당하는 페이지(프레임)를 lru list에서 찾습니다.  */
struct page * 
lru_list_find (void *kaddr)
{
    
    struct list_elem * e;
    for (e = list_begin (&lru_list); e != list_end (&lru_list);
        e = list_next (e))
    {
        struct page * page = list_entry (e, struct page, lru); 
        if (page->kaddr == kaddr)
            return page; 
    }
    return NULL;
}


/* 
더 이상 할당할 페이지가 없을 때 page_fault()에서 호출됩니다. clock 알고리즘으로 victim frame을 찾아서 
해당 프레임을 할당해제하고, 해당 프레임을 점유하던 쓰레드의 페이지테이블에서 해당 엔트리를 클리어합니다. 
*/
void 
try_to_free_pages ()
{

    struct page * page = clock_replacement (); /* clock 알고리즘으로 결정한 방출할 페이지 */

    size_t swap_slot; 

    delete_page_from_lru_list (page);          /* frame list로부터 해당 페이지(프레임) 삭제 */

    struct vm_entry * vme = page->vme;         /* 해당 프레임에 로드된 가상 메모리 엔트리 */
    switch(vme->type)                          /* 페이지에 로드된 데이터의 출처(바이너리, 파일, 스왑디스크)*/
    {
        case VM_BIN: 
            if (pagedir_is_dirty (page->thread->pagedir, page->vme->vaddr)) /* 마지막으로 로드된 이후 수정된 적 있으면 */
            {
                swap_slot= swap_out (page->kaddr);                          /* 스왑 디스크로 스왑 아웃 */
                page->vme->type = VM_ANON;                                  /* 다음번에 로드될 땐 스왑 디스크에서 로드될 것*/
                page->vme->swap_slot = swap_slot;                           /* 스왑 디스크 상 저장된 위치(인덱스) */
            }
            break; 
        case VM_ANON:
            swap_slot = swap_out (page->kaddr);  
            page->vme->swap_slot = swap_slot;
            break; 
        case VM_STACK: 
            break; 
        case VM_FILE:
            NOT_REACHED(); // mapped-file 은 아직 구현되지 않았습니다. 
        default:
            NOT_REACHED(); 
    }

    palloc_free_page (page->kaddr); /* 프레임을 프레임 풀(pool)로 돌려 보냄 */

    pagedir_clear_page (page->thread->pagedir, page->vme->vaddr); /* 페이지 테이블에서 해당 엔트리의 Present 비트를 0으로 설정 */

}


struct page * 
clock_replacement()
{
    struct page *victim_frame, *page; 

    /* clock_replacement 가 최초로 실행될 때 victim_ptr는 NULL입니다. */
    if (clock_victim == NULL)
        clock_victim = list_entry (list_begin (&lru_list), struct page, lru); 

    /* victim 포인터부터 lru_list 끝까지 순회하며 victim을 찾습니다. */
    struct list_elem * e; 
    for (e = &clock_victim->lru; e != list_end (&lru_list);
        e = list_next (e))
    {
        page = list_entry (e, struct page, lru); 
        victim_frame = give_second_chance_or_return_as_victim (page);
        if (victim_frame != NULL)
        {   
            return victim_frame; 
        }
        
    }

    /* 
    lru_list 끝까지 돌았는데 victim 이 없으면 처음부터 clock_victim까지 다시 돕니다. 
    첫번째 반복문에서 clock_victim 은 0으로 set됐기 때문에 이 반복문에서 적어도 하나는 반드시 victim으로 검출됩니다. 
    */  
    for (e = list_begin (&lru_list); e != list_next (&clock_victim->lru);
        e = list_next (e))
    {
        page = list_entry (e, struct page, lru); 
        victim_frame = give_second_chance_or_return_as_victim (page); 
        if (victim_frame != NULL)
            return victim_frame; 
    }
    
    NOT_REACHED(); 
    
}

struct page * give_second_chance_or_return_as_victim (struct page * page)
{

    ASSERT(page->thread)
    lock_acquire (&clock_lock); 
    if (pagedir_is_accessed (page->thread->pagedir, page->vme->vaddr)) // accessed bit 가 1 -> 0으로 바꿈. 
    {
        pagedir_set_accessed (page->thread->pagedir, page->vme->vaddr, 0); 
        lock_release (&clock_lock);
        return NULL ;
    }
    else // accessed bit가 0 -> victim!
    {
        /* victim 포인터를 real victim 다음 프레임으로 설정하여 
        다음 클럭 알고리즘 구동 때 순회를 시작할 포인터로 둡니다. */  
        clock_victim = list_entry (list_next (&page->lru), struct page, lru); 
        lock_release (&clock_lock);
        return page; 
    }


}



void 
remove_pages_from_dying_thread (struct thread * t)
{
    
    struct list_elem * e;
    struct page * last = NULL;  
    for (e = list_begin (&lru_list); e != list_end (&lru_list);
        e = list_next (e))
    
    {
        struct page * page = list_entry (e, struct page, lru);
        if (page->thread == t)
        {
            // free (last); 
            delete_page_from_lru_list (page); 
            // palloc_free_page (page->kaddr); 
            // last = page; 
        }
    }


}
