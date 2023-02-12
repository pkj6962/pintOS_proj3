#include <bitmap.h>
#include <debug.h>
#include <stdio.h> 

#include "swap.h"
#include "devices/block.h"
#include "threads/synch.h"


static struct bitmap * swap_map; /* 스왑 디스크의 각 섹터의 할당 여부를 관리하는 비트맵입니다. */
static struct lock swap_lock; 


/*
swap_map을 시스템에서 정의된 SWAP_DISK_SIZE 크기에 맞게 초기화합니다. 
*/
void 
swap_map_init ()
{

    swap_map = bitmap_create (SWAP_DISK_SIZE / BLOCK_SECTOR_SIZE); 
    if (!swap_map)
        PANIC ("swap map not created\n"); 

    lock_init (&swap_lock); 

    bitmap_set_all (swap_map, false);     
}


/* 
kaddr 가 가리키는 페이지에 있는 내용을 스왑 디스크에서 빈 슬롯을 찾아서 할당합니다. 
*/
size_t 
swap_out (void * kaddr)
{   
    struct block * block = block_get_role (BLOCK_SWAP); 
        
    size_t idx = find_empty_slot (PAGE_SIZE / BLOCK_SECTOR_SIZE); 
    if (idx == BITMAP_ERROR)
        PANIC("Think about it..."); /* TODO: 빈 슬롯이 없으면 어떡하지? */
    ASSERT (idx % 8 == 0); /* 스왑 맵 인덱스는 반드시 8의 배수 단위로 관리되어야 합니다. */ 

    for (int i = 0; i < PAGE_SIZE/BLOCK_SECTOR_SIZE; i++)
    {
        block_write (block, idx + i, kaddr + BLOCK_SECTOR_SIZE * i);
    }

    return idx;
}

/* 스왑 디스크에서 메모리 kaddr로 데이터를 불러옵니다. 스왑 맵의 해당 비트를 FLIP합니다. */
void 
swap_in (size_t used_index, void *kaddr)
{
    ASSERT (used_index % 8 == 0);
    ASSERT (bitmap_all (swap_map, used_index, 8)); /* 스왑 맵의 used_index 부터 연속 8개의 비트는 모두 TRUE여야 합니다. */
    

    struct block * block = block_get_role (BLOCK_SWAP);

    for (int i = 0; i < PAGE_SIZE / BLOCK_SECTOR_SIZE; i++)
    {
        block_read (block, used_index + i, kaddr + BLOCK_SECTOR_SIZE * i); 
    }

    /* 이제 스왑 디스크에 있는 내용을 메모리로 올렸으니 스왑 디스크는 invalid하게 만듭니다. */
    lock_acquire (&swap_lock); 
    bitmap_set_multiple (swap_map, used_index, 8, false); 
    lock_release (&swap_lock); 

} 


/*
    swap map을 탐색해서  CNT개의 연속적인 빈 슬롯이 발견되면 이를 반환합니다.  
    해당 빈 슬롯에 해당하는 비트를 TRUE로 Flip합니다.  
    빈 슬롯이 없으면 BITMAP_ERROR 를 반환합니다. 
*/
size_t 
find_empty_slot (size_t cnt)
{
    lock_acquire (&swap_lock); 
    size_t idx = bitmap_scan_and_flip (swap_map, 0, cnt, false); 
    lock_release (&swap_lock);

    return idx; 
}






/*
긴급: 스왑 블록의 섹터 개수를 알아보자. 

스왑 디스크의 사이즈가 1MB라면, 1MB(2^20) / 512(2^9) bytes = 2^11 = 2048개가 있었으면 한다.  있다!





*/


void get_sector_size ()
{
    struct block * block = block_get_role (BLOCK_SWAP);
    printf("size: %d\n", block->size);
}