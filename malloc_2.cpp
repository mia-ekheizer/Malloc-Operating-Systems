
class MemoryList {
public:
    typedef struct MallocMetadata {
        size_t size;
        bool is_free;
        MallocMetadata* next;
        MallocMetadata* prev;
    } MallocMetadata;
private:  
    MallocMetadata* head;
    MallocMetadata dummy;
    int num_free_blocks;
    int num_free_bytes;
    int num_allocated_blocks;
    int num_allocated_bytes;
    const int metadata_size = sizeof(MallocMetadata);
    //private constructor
    MemoryList() {
        dummy.next = nullptr;
        dummy.prev = nullptr;
        head = &dummy;
        num_free_blocks = 0;
        num_free_bytes = 0;
        num_allocated_blocks = 0;
        num_allocated_bytes = 0;
    }

public:
     static MemoryList& getInstance() {
        static MemoryList instance;
        return instance;
    }

    //for allocation usage
    void* insert(MallocMetadata* newMetadata) {
        MallocMetadata* temp = head;
        while(temp->next) {
            if (temp->is_free && temp->size >= newMetadata->size) { //
                temp->is_free = false;
                num_free_blocks--;
                num_allocated_blocks++; 
                num_allocated_bytes += temp->size; // the size of the block that was allocated
                num_free_bytes -= temp->size;
                return temp;
            }
            temp = temp->next;
        } // no free blocks in the right size found:
        temp->next = newMetadata;
        newMetadata->prev = temp;
        newMetadata->next = nullptr;
        newMetadata->is_free = false;
        num_allocated_blocks++;
        num_allocated_bytes += newMetadata->size;
        return newMetadata;
    }

    //for free usage
    void remove(MallocMetadata* toRemove) {
        if(!toRemove || toRemove->is_free)
            return;
    
        num_allocated_blocks--;
        num_allocated_bytes -= toRemove->size;
        num_free_blocks++;
        num_free_bytes += toRemove->size;
        toRemove->is_free = true;
    }
    int getMetadataSize() {
        return metadata_size;
    }

    void initializeMetadata(MallocMetadata* metadata, size_t size) {
        metadata->size = size;
        metadata->is_free = false;
        metadata->next = nullptr;
        metadata->prev = nullptr;
    }
};

void* smalloc(size_t size) {
    MemoryList& memList = MemoryList::getInstance();
    if(size == 0 || size > 1e8)
        return nullptr;
    MemoryList::MallocMetadata* newMetadata = reinterpret_cast<MemoryList::MallocMetadata*>(sbrk(size + memList.getMetadataSize()));
    if(!newMetadata)
        return nullptr;
    memList.initializeMetadata(newMetadata, size);
    return memList.insert(newMetadata);
}
void* scalloc(size_t num, size_t size) {

}

void sfree(void* p) {

}

void* srealloc(void* oldp, size_t size) {

}

size_t _num_free_blocks() {

}

size_t _num_free_bytes() {

}

size_t _num_allocated_blocks() {

}

size_t _num_allocated_bytes() {
    
}

size_t _num_meta_data_bytes() {

}

size_t _size_meta_data() {

}






