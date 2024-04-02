#include "malloc_3.cpp"

#include <unistd.h>
#include <cmath>
#include <sys/wait.h>
#include <stdio.h>

#define MAX_ALLOCATION_SIZE (1e8)
#define MMAP_THRESHOLD (128 * 1024)
#define MIN_SPLIT_SIZE (128)
#define MAX_ELEMENT_SIZE (128*1024)

void verify_blocks(int allocated_blocks, int allocated_bytes, int free_blocks, int free_bytes) {
    printf("expected num allocated blocks: %d\n", allocated_blocks);
    printf("actual num allocated blocks: %zu\n", _num_allocated_blocks());
    printf("expected num allocated bytes: %d\n", allocated_bytes);
    printf("actual num allocated bytes: %zu\n", _num_allocated_bytes());
    printf("expected num free blocks: %d\n", free_blocks);
    printf("actual num free blocks: %zu\n", _num_free_blocks());
    printf("expected num free bytes: %d\n", free_bytes);
    printf("actual num free bytes: %zu\n", _num_free_bytes());
    printf("expected num allocated meta data bytes: %zu\n", _size_meta_data() * allocated_blocks);
    printf("actual num allocated meta data bytes: %zu\n", _num_meta_data_bytes());
}


void verify_size(void* base) {
    void *after = sbrk(0);
    printf("expected new heap size: %zu\n", (size_t)after - (size_t)base);
    printf("actual new heap size: %zu\n", _num_allocated_bytes() + _size_meta_data() * _num_allocated_blocks());
}

void verify_size_with_large_blocks(void* base, size_t diff) {
    void *after = sbrk(0);
    printf("expected new heap size: %zu\n", diff);
    printf("actual new heap size: %zu\n", (size_t)after - (size_t)base);
}

void verify_block_by_order(int order0free, int order0used, int order1free, int order1used, int order2free, int order2used, int order3free, int order3used, int order4free, int order4used, int order5free, int order5used, int order6free, int order6used, int order7free, int order7used, int order8free,int  order8used, int order9free, int  order9used, int order10free, int  order10used, int big_blocks_count, long big_blocks_size) {
    unsigned int __total_blocks = order0free + order0used+ order1free + order1used+ order2free + order2used+ order3free + order3used+ order4free + order4used+ order5free + order5used+ order6free + order6used+ order7free + order7used+ order8free + order8used+ order9free + order9used+ order10free + order10used + big_blocks_count;
    unsigned int __total_free_blocks = order0free+ order1free+ order2free+ order3free+ order4free+ order5free+ order6free+ order7free+ order8free+ order9free+ order10free;
    unsigned int __total_free_bytes_with_meta  = order0free*128*pow(2,0) +  order1free*128*pow(2,1) +  order2free*128*pow(2,2) +  order3free*128*pow(2,3) +  order4free*128*pow(2,4) +  order5free*128*pow(2,5) +  order6free*128*pow(2,6) +  order7free*128*pow(2,7) +  order8free*128*pow(2,8) +  order9free*128*pow(2,9)+  order10free*128*pow(2,10);
    unsigned int testing_allocated_bytes;
    if (__total_blocks==0) {
        testing_allocated_bytes = 0;
    }
    else {
        testing_allocated_bytes = big_blocks_size+32 * MAX_ELEMENT_SIZE - (__total_blocks-big_blocks_count)*(_size_meta_data());
    }
    verify_blocks(__total_blocks, testing_allocated_bytes, __total_free_blocks,__total_free_bytes_with_meta - __total_free_blocks*(_size_meta_data()));
}

int main() {
    printf("-----started-------\n");
    void* ptr = smalloc(100000001);
    if (ptr != NULL) {
        printf("error! ptr should be NULL but it is not!\n");
    }
    verify_block_by_order(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0, 0);
    printf("----------free(ptr)--------------\n");
    sfree(ptr);
    verify_block_by_order(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0, 0);
    
    /*
    verify_block_by_order(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    printf("------malloc(ptr1)-------\n");
    // Allocate small block (order 0)
    void *ptr1 = smalloc(40);
    if (ptr1 == nullptr) {
        printf("error: ptr1 is nullptr!\n");
    }
//    verify_size(base);
    verify_block_by_order(1,1,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,31,0,0,0);
    printf("------malloc(ptr2)-------\n");
    // Allocate large block (order 10)
    void *ptr2 = smalloc(MAX_ELEMENT_SIZE+100);
    if (ptr2 == nullptr) {
        printf("error: ptr2 is nullptr!\n");
    }
//    verify_size_with_large_blocks(base, (128 * 1024+100 +_size_meta_data()));
    verify_block_by_order(1,1,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,31,0,1,MAX_ELEMENT_SIZE+100);
    printf("------malloc(ptr3)-------\n");
    // Allocate another small block
    void *ptr3 = smalloc(50);
    if (ptr3 == nullptr) {
        printf("error: ptr3 is nullptr!\n");
    }
    verify_block_by_order(0,2,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,31,0,1,MAX_ELEMENT_SIZE+100);
    printf("------free(ptr1)-------\n");
    // Free the first small block
    sfree(ptr1);
    verify_block_by_order(1,1,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,31,0,1,MAX_ELEMENT_SIZE+100);

    printf("------malloc(ptr4)-------\n");
    // Allocate another small block
    void *ptr4 = smalloc(40);
    if (ptr4 == nullptr) {
        printf("error: ptr4 is nullptr!\n");
    }
    verify_block_by_order(0,2,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,31,0,1,MAX_ELEMENT_SIZE+100);

    // Free all blocks
    printf("------free(ptr3)-------\n");
    sfree(ptr3);
    printf("------free(ptr4)-------\n");
    sfree(ptr4);
    verify_block_by_order(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,1,MAX_ELEMENT_SIZE+100);
    printf("------free(ptr1) again-------\n");
    sfree(ptr1); //free again
    printf("------free(ptr2)-------\n");
    sfree(ptr2);
    verify_block_by_order(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,32,0,0,0);
    */
    return 0;
}
    