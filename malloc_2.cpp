
class MemoryList {
private:
    typedef struct MallocMetadata {
        size_t size;
        bool is_free;
        MallocMetadata* next;
        MallocMetadata* prev;
    } MallocMetadata;
    
    MallocMetadata* head;
    int num_free_blocks;
    int num_free_bytes;
    int num_allocated_blocks;
    int num_allocated_bytes;
    const int metadata_size = sizeof(MallocMetadata);

public:
    MemoryList() {
        head = nullptr;
        num_free_blocks = 0;
    }

    void* insert(MallocMetadata* newMetadata) {
        if(!head) {
            head = newMetadata;
            num_allocated_blocks++;
            num_allocated_bytes += newMetadata->size;
            return;
        }
        MallocMetadata* temp = head;
        if(num_free_blocks != 0) {
            while(temp->next) {
                if (temp->is_free && temp->size > newMetadata->size) {
                    temp->is_free = false;
                    num_free_blocks--;
                    num_allocated_blocks++; 
                    num_allocated_bytes += temp->size; // the size of the block that was allocated
                    num_free_bytes -= temp->size;
                    return;
                }
                temp = temp->next;
            } // no free blocks in the right size found:
            temp->next = newMetadata;
            newMetadata->prev = temp;
            newMetadata->next = nullptr;
            num_allocated_blocks++;
            num_allocated_bytes += newMetadata->size;
            return;
        }
        
        while(temp->next) {
            temp = temp->next;
        }
        temp->next = newMetadata;
        newMetadata->prev = temp;
        
    }
    void remove(MallocMetadata* metadata) {
        if(metadata == head) {
            head = metadata->next;
            return;
        }
        MallocMetadata* temp = head;
        while(temp->next != metadata) {
            temp = temp->next;
        }
        temp->next = metadata->next;
        metadata->next->prev = temp;
    }

    void* smalloc(size_t size) {
    
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
};

void* smalloc(size_t size) {
    
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






