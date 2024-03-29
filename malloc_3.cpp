#include <unistd.h>
#define MAX_ORDER 10
#define MAX_BLOCK_SIZE ((2 ** 10) * 128)
#define NUM_INITIAL_BLOCKS 32
#define MIN_BLOCK_SIZE 128

class MemoryArray
{
private:
    MemoryList memory_array[MAX_ORDER + 1];
    int num_allocated_blocks;
    int num_allocated_bytes;
    MemoryArray()
    {
        num_allocated_blocks = 0;
        num_allocated_bytes = 0;    
        for(int i = 0; i <= MAX_ORDER; i++) {
            memory_array[i] = NULL;
        }
        memory_array[MAX_ORDER + 1] = MemoryList();
        void* newAddress = sbrk(MAX_BLOCK_SIZE * NUM_INITIAL_BLOCKS);
        if(newAddress == (void*)(-1)) //TODO: is necessary?
            exit(0);
        for (int block_num = 1; block_num <= NUM_INITIAL_BLOCKS; block_num++) {
            memory_array[MAX_ORDER + 1].insert((MemoryArray::MemoryList::MallocMetadata*)(newAddress + (block_num * MAX_BLOCK_SIZE)), MAX_BLOCK_SIZE);
        }
    }
    
    class MemoryList
    {
    public:
        typedef struct MallocMetadata
        {
            size_t size;
            bool is_free;
            MallocMetadata *next;
            MallocMetadata *prev;
        } MallocMetadata;

    private:
        MallocMetadata dummy;
        int num_free_blocks;
        
    public:
        MemoryList()
        {
            dummy.next = nullptr;
            dummy.prev = nullptr;
            num_free_blocks = 0;
        }

        // TODO: adjust to new implementation
        void *insert(MallocMetadata *newMetadata)
        {
            newMetadata->size = size;
            MallocMetadata *temp = &dummy;
            while (temp->next)
            {
                if (temp->is_free && temp->size >= newMetadata->size)
                {
                    temp->is_free = false;
                    num_free_blocks--;
                    num_allocated_blocks++;
                    num_allocated_bytes += temp->size; // the size of the block that was allocated
                    num_free_bytes -= temp->size;
                    return temp + metadata_size;
                }
                temp = temp->next;
            } // no free blocks in the right size found:
            temp->next = newMetadata;
            newMetadata->prev = temp;
            newMetadata->next = nullptr;
            newMetadata->is_free = false;
            num_allocated_blocks++;
            num_allocated_bytes += newMetadata->size;
            return newMetadata + metadata_size;
        }

        MallocMetadata* remove() //DONE
        {
            MallocMetadata* toRemove = dummy->next;
            dummy->next = toRemove->next;
            toRemove->next->prev = dummy;
            num_free_blocks--;
            num_free_bytes -= toRemove->size;
            return toRemove;
        }
    };
    
public:
    static MemoryArray &getInstance()
    {
        static MemoryArray instance;
        return instance;
    }

    ~MemoryArray() {
        // TODO: do we need to reset the program break?
    }
    int getRelevantEntryForInsert(size_t size) {
        size /= 128;
        int relevantEntry = 0;
        while(size > 0)
        {
            size /= 2;
            relevantEntry++;
        }
        return relevantEntry;
    }
    
    void insert(MemoryList::MallocMetadata* newFreeBlock) { // insert in an ascending order by the address, plus combine buddies iteratively
        int relevantEntry = getRelevantEntryForInsert(newFreeBlock->size);
        /* TODO:
        if((((newFreeBlockSize - offset) XOR Size) + offset) in memory_array[curr_entry]) {
            merge blocks iteratively (stop when we get to entry No.10)
        }
        */
        memory_array[relevantEntry].insert(newFreeBlock);
        //TODO: add statistics
    }

    MemoryList::MallocMetadata* remove(size_t size) { //remove block of the correct size from the free list
        int relevantEntry = getBiggerThanEqualToEntry(size);
        int relevantEntrySize = MIN_BLOCK_SIZE * (2 ** relevantEntry);
        while (size < (relevantEntrySize / 2 - sizeof(MemoryList::MallocMetadata))) { // split the blocks until we get a block of the correct size
            MemoryList::MallocMetadata* wantedAddress = memory_array[relevantEntry].remove();
            MemoryList::MallocMetadata* buddyAddress = wantedAddress + relevantEntrySize / 2;
            memory_array[relevantEntry - 1].insert(buddyAddress);
            memory_array[relevantEntry - 1].insert(wantedAddress);
            //insert the right side of split block
            relevantEntrySize /= 2;
            relevantEntry--;
        }
        num_allocated_blocks++;
        num_allocated_bytes += relevantEntrySize;                           
        return memory_array[relevantEntry].remove() + sizeof(MemoryList::MallocMetadata);
    }

    int getBiggerThanEqualToEntry(size_t size) {
        int curr_entry_size;
        for (int entry = 0; entry < MAX_ORDER+1; entry++) { // find the first index that has a fitting/bigger size block
            int curr_entry_size = MIN_BLOCK_SIZE * (2 ** entry);
            if (memory_array[entry] != NULL && size < curr_entry_size - sizeof(MemoryList::MallocMetadata)) {
                return entry;
            }
        }
    }
    
};

void* smalloc(size_t size)
{
    MemoryArray& memArr = MemoryArray::getInstance();
    if(size == 0 || size > 1e8)
        return nullptr;

    return memArr.remove(size);
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

// TODO: CONTINUE ADJUSTING FROM HERE ON
void sfree(void* p) {
    if (p == nullptr) 
        return;
    
    MemoryArray& memArray = MemoryArray::getInstance();
    memArray.insert((MemoryList::MallocMetadata*)(p - memList.getMetadataSize()));
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
    return memList.num_allocated_blocks + memList.num_free_blocks;
}

size_t _num_allocated_bytes() {
    MemoryList& memList = MemoryList::getInstance();
    return memList.num_allocated_bytes + memList.num_free_bytes;
}

size_t _num_meta_data_bytes() {
    MemoryList& memList = MemoryList::getInstance();
    return _num_allocated_blocks() * memList.getMetadataSize();
}

size_t _size_meta_data() {
    MemoryList& memList = MemoryList::getInstance();
    return memList.getMetadataSize();
}






