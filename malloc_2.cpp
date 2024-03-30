#include <unistd.h>

class MemoryList {
public:
    typedef struct MallocMetadata {
        size_t size;
        bool is_free;
        MallocMetadata* next;
        MallocMetadata* prev;
    } MallocMetadata;

private:  
    MallocMetadata dummy;
    int num_free_blocks;
    int num_free_bytes;
    int num_used_blocks;
    int num_used_bytes;
    const int metadata_size = sizeof(MallocMetadata);

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
    void* insert(MallocMetadata* newMetadata, size_t size) {
        newMetadata->size = size;
        MallocMetadata* temp = &dummy;
        while(temp->next) {
            if (temp->is_free && temp->size >= newMetadata->size) {
                temp->is_free = false;
                num_free_blocks--;
                num_used_blocks++; 
                num_used_bytes += temp->size; // the size of the block that was allocated
                num_free_bytes -= temp->size;
                return temp + metadata_size;
            }
            temp = temp->next;
        } // no free blocks in the right size found:
        temp->next = newMetadata;
        newMetadata->prev = temp;
        newMetadata->next = nullptr;
        newMetadata->is_free = false;
        num_used_blocks++;
        num_used_bytes += newMetadata->size;
        return newMetadata + metadata_size;
    }

    //for free usage
    void remove(MallocMetadata* toRemove) {
        if(!toRemove || toRemove->is_free)
            return;
    
        num_used_blocks--;
        num_used_bytes -= toRemove->size;
        num_free_blocks++;
        num_free_bytes += toRemove->size;
        toRemove->is_free = true;
    }

    int getMetadataSize() {
        return metadata_size;
    }
};

void* smalloc(size_t size) {
    MemoryList& memList = MemoryList::getInstance();
    if(size == 0 || size > 1e8)
        return nullptr;

    MemoryList::MallocMetadata* newMetadata = (MemoryList::MallocMetadata*)(sbrk(size + memList.getMetadataSize()));
    if(newMetadata == (void*)(-1))
        return nullptr;

    return memList.insert(newMetadata, size);
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
    memList.remove((MemoryList::MallocMetadata*)(p - memList.getMetadataSize()));
}

void* srealloc(void* oldp, size_t size) {
    MemoryList& memList = MemoryList::getInstance();
    if (size == 0 || size > 1e8 || size <= (MemoryList::MallocMetadata*)(oldp - memList.getMetadataSize())->size)
        return nullptr;
    
    if (oldp == nullptr)
        return smalloc(size);

    void* newAddress = memList.insert((MemoryList::MallocMetadata*)(oldp - memList.getMetadataSize()), size);
    newAddress = memmove(newAddress, oldp, size);
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
    return memList.num_used_blocks + memList.num_free_blocks;
}

size_t _num_allocated_bytes() {
    MemoryList& memList = MemoryList::getInstance();
    return memList.num_used_bytes + memList.num_free_bytes;
}

size_t _num_meta_data_bytes() {
    MemoryList& memList = MemoryList::getInstance();
    return _num_allocated_blocks() * memList.getMetadataSize();
}

size_t _size_meta_data() {
    MemoryList& memList = MemoryList::getInstance();
    return memList.getMetadataSize();
}






