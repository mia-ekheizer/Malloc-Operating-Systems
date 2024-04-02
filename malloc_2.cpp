#include <unistd.h>
#include <string.h>
#include <cstring>

class MemoryList {
public:
    typedef struct MallocMetadata {
        size_t size;
        bool is_free;
        MallocMetadata* next;
        MallocMetadata* prev;
    } MallocMetadata;

    MallocMetadata dummy;
    int num_free_blocks;
    int num_free_bytes;
    int num_used_blocks;
    int num_used_bytes;

private:
    //private constructor
    MemoryList() {
        dummy.next = nullptr;
        dummy.prev = nullptr;
        num_free_blocks = 0;
        num_free_bytes = 0;
        num_used_blocks = 0;
        num_used_bytes = 0;
    }

public:
    // singleton
    static MemoryList& getInstance() {
        static MemoryList instance;
        return instance;
    }

    //for allocation usage
    void* insert(size_t size) {
        MallocMetadata* temp = &dummy;
        while(temp->next || temp->is_free) {
            if (temp->is_free && temp->size >= size) {
                temp->is_free = false;
                num_free_blocks--; 
                num_free_bytes -= temp->size;
                return temp + 1;
            }
            temp = temp->next;
        } // no free blocks in the right size found:
        MallocMetadata* newMetadata = (MallocMetadata*)(sbrk(size + sizeof(MallocMetadata)));
        if(newMetadata == (void*)(-1))
            return nullptr;

        temp->next = newMetadata;
        newMetadata->prev = temp;
        newMetadata->next = nullptr;
        newMetadata->is_free = false;
        newMetadata->size = size;
        num_used_blocks++;
        num_used_bytes += size;
        return newMetadata + 1;
    }

    //for free usage
    void remove(MallocMetadata* toRemove) {
        if(!toRemove || toRemove->is_free)
            return;

        num_free_blocks++;
        num_free_bytes += toRemove->size;
        toRemove->is_free = true;
    }
};

void* smalloc(size_t size) {
    MemoryList& memList = MemoryList::getInstance();
    if(size == 0 || size > 1e8)
        return nullptr;

    return memList.insert(size);
}

void* scalloc(size_t num, size_t size) {
    if (num == 0 || size == 0 || size * num > 1e8) 
        return nullptr;

    void* newAddress = smalloc(num * size);
    if (newAddress == nullptr) 
        return nullptr;

    memset(newAddress, 0, num * size);
    return newAddress;
}

void sfree(void* p) {
    if (p == nullptr) 
        return;
    
    MemoryList& memList = MemoryList::getInstance();
    memList.remove((MemoryList::MallocMetadata*)p - 1);
}

void* srealloc(void* oldp, size_t size) {
    MemoryList& memList = MemoryList::getInstance();
    if (size == 0 || size > 1e8)
        return nullptr;

    if (oldp == nullptr)
        return smalloc(size);
    
    if (size <= ((MemoryList::MallocMetadata*)oldp - 1)->size)
        return oldp;

    void* newAddress = memList.insert(size);
    newAddress = memmove(newAddress, oldp, ((MemoryList::MallocMetadata*)oldp - 1)->size);
    sfree(oldp);
    return newAddress;
}

size_t _num_free_blocks() {
    MemoryList& memList = MemoryList::getInstance();
    return memList.num_free_blocks;
}

size_t _num_free_bytes() {
    MemoryList& memList = MemoryList::getInstance();
    return memList.num_free_bytes;
}

size_t _num_allocated_blocks() {
    MemoryList& memList = MemoryList::getInstance();
    return memList.num_used_blocks;
}

size_t _num_allocated_bytes() {
    MemoryList& memList = MemoryList::getInstance();
    return memList.num_used_bytes;
}

size_t _size_meta_data() {
    return sizeof(MemoryList::MallocMetadata);
}

size_t _num_meta_data_bytes() {
    return _num_allocated_blocks() * _size_meta_data();
}