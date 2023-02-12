#include "devices/block.h"

#define MB 1048576
#define SWAP_DISK_SIZE 4 * MB /* MB 단위 스왑 디스크 사이즈 */
#define PAGE_SIZE 4096
/*
스왑 디스크의 기본 관리 단위는 512 바이트이고, 페이지의 기본 IO 단위는 4KB(4096바이트)입니다.
따라서 페이지 하나를 스왑디스크에(서) 읽고 쓸 때는 8개의 섹터가 수반됩니다. 

그러므로 비트맵은 항상 한번에 8개의 비트가 flip되는 식으로 관리될 것입니다. 
*/



void get_sector_size (void); 

void swap_map_init ();

size_t swap_out (void * kaddr);

void swap_in (size_t used_index, void *kaddr);

size_t find_empty_slot (size_t cnt); 