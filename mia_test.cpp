#include "malloc_2.cpp"

#include <stdio.h>

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

int main() {
    printf("-----started-------\n");
    verify_blocks(0, 0, 0, 0);

    void *base = sbrk(0);
    char *a = (char *)smalloc(10);
    if (a == nullptr) {
        printf("error: a is nullptr!\n");
    }
    char *b = (char *)smalloc(10);
    if (b == nullptr) {
        printf("error: b is nullptr!\n");
    }
    char *c = (char *)smalloc(10);
    if (c == nullptr) {
        printf("error: c is nullptr!\n");
    }

    printf("----------checking that malloc worked for a, b and c------\n");
    verify_blocks(3, 30, 0, 0);
    verify_size(base);

    printf("--------free(a)--------\n");
    sfree(a);

    printf("----------checking that free(a) worked------\n");
    verify_blocks(3, 30, 1, 10);
    verify_size(base);

    printf("--------free(b)--------\n");
    sfree(b);

    printf("----------checking that free(b) worked------\n");
    verify_blocks(3, 30, 2, 20);
    verify_size(base);

    printf("--------free(c)--------\n");
    sfree(c);

    printf("----------checking that free(c) worked------\n");
    verify_blocks(3, 30, 3, 30);
    verify_size(base);

    char *new_a = (char *)smalloc(10);
    if (a != new_a) {
        printf("error: a != new_a!\n");
    }
    
    char *new_b = (char *)smalloc(10);
    if (b != new_b) {
        printf("error: b != new_b!\n");
    }

    char *new_c = (char *)smalloc(10);
    if (c != new_c) {
        printf("error: c != new_c!\n");
    }
    
    printf("----------checking that malloc worked for new_a, new_b and new_c------\n");
    verify_blocks(3, 30, 0, 0);
    verify_size(base);

    printf("--------free(new_a)--------\n");
    sfree(new_a);

    printf("----------checking that free(new_a) worked------\n");
    verify_blocks(3, 30, 1, 10);
    verify_size(base);

    printf("--------free(new_b)--------\n");
    sfree(new_b);

    printf("----------checking that free(new_b) worked------\n");
    verify_blocks(3, 30, 2, 20);
    verify_size(base);

    printf("--------free(new_c)--------\n");
    sfree(new_c);

    printf("----------checking that free(new_c) worked------\n");
    verify_blocks(3, 30, 3, 30);
    verify_size(base);
    return 0;
}
    