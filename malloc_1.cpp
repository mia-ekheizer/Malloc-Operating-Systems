#include <unistd.h>

void* smalloc(size_t size) {
    if (size == 0 || size > 1e8) {
        return NULL;
    }
    void* newAddress = sbrk((intptr_t)size);
    if (newAddress == (void*)(-1)) {
        return NULL;
    }
    else {
        return newAddress;
    }
}