class MemoryList {
private:
    typedef struct MallocMetadata {
        size_t size;
        bool is_free;
        MallocMetadata* next;
        MallocMetadata* prev;
    }MallocMetadata;

    MallocMetadata* head;

public:
    MemoryList() {
        head = nullptr;
    }
    void insert(MallocMetadata* metadata) {
        if(!head) {
            head = metadata;
            return;
        }
        MallocMetadata* temp = head;
        while(temp->next) {
            temp = temp->next;
        }
        temp->next = metadata;
        metadata->prev = temp;
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






