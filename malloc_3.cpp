#include <unistd.h>
#include <string.h>
#include <cstring>
#include <sys/mman.h>

#define MAX_ORDER 10
#define KB 1000
#define MAX_BLOCK_SIZE (KB * 128)
#define NUM_INITIAL_BLOCKS 32
#define MIN_BLOCK_SIZE 128

class MemoryList {
public:
    typedef struct MallocMetadata
    {
        size_t size;
        MallocMetadata *next;
        MallocMetadata *prev;
    } MallocMetadata;
    
    MallocMetadata* dummy;
    int num_free_blocks;
    MemoryList()
    {
        dummy->next = nullptr;
        dummy->prev = nullptr;
        num_free_blocks = 0;
    }

    void* insert(MallocMetadata *newMetadata)
    {
        MallocMetadata *temp = dummy;
        MallocMetadata *previousNode = nullptr;
        
        while(temp && temp < newMetadata) {//find correct position
            previousNode = temp;
            temp = temp->next;
        }
        if(!previousNode) { //prev is null, only dummy in list
            dummy->next = newMetadata;
            newMetadata->prev = dummy;
        }
        else {
            previousNode->next = newMetadata;
            newMetadata->prev = previousNode;
        }
        newMetadata->next = temp;
        num_free_blocks++;
        return newMetadata + 1;
    }

    MallocMetadata* remove()
    {
        MallocMetadata* toRemove = dummy->next;
        dummy->next = toRemove->next;
        toRemove->next->prev = dummy;
        num_free_blocks--;
        return toRemove;
    }
    
    void remove(MallocMetadata* address) {
        MallocMetadata* temp = dummy->next;
        while(temp && temp != address) {
            temp = temp->next;
        }
        if(temp && temp == address) {
            temp->prev->next = temp->next;
            if(temp->next)
                temp->next->prev = temp->prev;
            num_free_blocks--;
        }
    }
};

class MemoryArray
{
public:
    MemoryList* memory_array[MAX_ORDER + 1];
    int num_used_blocks;
    int num_used_bytes;
    int offset;

private:
    MemoryArray()
    {
        num_used_blocks = 0;
        num_used_bytes = 0;    
        for(int i = 0; i <= MAX_ORDER; i++) {
            memory_array[i] = nullptr;
        }
        void* initialProgramBreak = sbrk(0);
        void* newAddress = sbrk(MAX_BLOCK_SIZE * NUM_INITIAL_BLOCKS);
        // if(newAddress == (void*)(-1)) //TODO: is necessary?
        
        offset = (reinterpret_cast<intptr_t>(newAddress) - reinterpret_cast<intptr_t>(initialProgramBreak)) % (NUM_INITIAL_BLOCKS * MAX_BLOCK_SIZE);
        for (int block_num = 1; block_num <= NUM_INITIAL_BLOCKS; block_num++) {
            memory_array[MAX_ORDER + 1]->insert((MemoryList::MallocMetadata*)(reinterpret_cast<intptr_t>(newAddress) + (block_num * MAX_BLOCK_SIZE)));
        }
    }
    
public:
    static MemoryArray &getInstance()
    {
        static MemoryArray instance;
        return instance;
    }

    ~MemoryArray() {
        // TODO: do we need to reset the program break?
    }

    int getRelevantEntry(size_t size) {
        size /= 128;
        int relevantEntry = 0;
        while(size > 0)
        {
            size /= 2;
            relevantEntry++;
        }
        return relevantEntry;
    }
    
    MemoryList::MallocMetadata* isBuddyInEntry(MemoryList::MallocMetadata* newFreeBlock, int entry) {
        //iterate over the whole list.
        MemoryList* memList = memory_array[entry];
        MemoryList::MallocMetadata* curr_metadata = memList->dummy;
        while (curr_metadata->next != nullptr) {
            if(curr_metadata == (MemoryList::MallocMetadata*)((reinterpret_cast<intptr_t>(newFreeBlock) - offset)^((newFreeBlock->size) + offset))) {
                return curr_metadata;
            }
            curr_metadata = curr_metadata->next;
        }
        return nullptr;
    }
    
    MemoryList::MallocMetadata* removeAndMerge(MemoryList::MallocMetadata* firstBuddyAddress, MemoryList::MallocMetadata* secondBuddyAddress) {
        int entry = getRelevantEntry(firstBuddyAddress->size + sizeof(MemoryList::MallocMetadata));
        memory_array[entry]->remove(firstBuddyAddress);
        memory_array[entry]->remove(secondBuddyAddress);
        MemoryList::MallocMetadata* parent = (reinterpret_cast<intptr_t>(firstBuddyAddress) < reinterpret_cast<intptr_t>(secondBuddyAddress)) ? firstBuddyAddress : secondBuddyAddress;
        parent->size = 2 * firstBuddyAddress->size;
        return parent;
    }

    // insert in an ascending order by the address, plus combine buddies iteratively
    void insert(MemoryList::MallocMetadata* newFreeBlock, size_t originalSize, size_t desiredSize) { 
        int relevantEntry = getRelevantEntry(newFreeBlock->size + sizeof(MemoryList::MallocMetadata));
        MemoryList::MallocMetadata* buddyAddress = isBuddyInEntry(newFreeBlock, relevantEntry);
        MemoryList::MallocMetadata* parent = buddyAddress;
        if (buddyAddress != nullptr && relevantEntry != MAX_ORDER) {
            if (desiredSize == 0 || (desiredSize != 0 && parent->size < desiredSize)) {
                parent = removeAndMerge(newFreeBlock, buddyAddress);
                insert(parent, originalSize, desiredSize);
            }
        }
        else {
            memory_array[relevantEntry]->insert(newFreeBlock);
            num_used_blocks--;
            num_used_bytes -= originalSize;
        }
    }

    //remove a block in the minimal size possible
    void* remove(size_t size) { 
        // find a big enough block so we can split it later
        int relevantEntry = getBiggerThanEqualToEntry(size);
        int relevantEntrySize = MIN_BLOCK_SIZE * (2 << relevantEntry);
        // split the blocks until we get a block of the correct size
        while (size <= (relevantEntrySize / 2 - sizeof(MemoryList::MallocMetadata))) { 
            MemoryList::MallocMetadata* wantedAddress = memory_array[relevantEntry]->remove();
            MemoryList::MallocMetadata* buddyAddress = (MemoryList::MallocMetadata*)(reinterpret_cast<intptr_t>(wantedAddress) + relevantEntrySize / 2);
            memory_array[relevantEntry - 1]->insert(buddyAddress);
            memory_array[relevantEntry - 1]->insert(wantedAddress);
            //insert the right side of split block
            relevantEntrySize /= 2;
            relevantEntry--;
        }
        relevantEntrySize *= 2;
        relevantEntry++;
        num_used_blocks++;
        num_used_bytes += relevantEntrySize;                         
        return (void*)(memory_array[relevantEntry]->remove() + sizeof(MemoryList::MallocMetadata));
    }

    int getBiggerThanEqualToEntry(size_t size) {
        int curr_entry_size;
        int entry;
        // find the first index that has a fitting/bigger size block
        for (entry = 0; entry < MAX_ORDER+1; entry++) { 
            curr_entry_size = MIN_BLOCK_SIZE * (2 << entry);
            if (memory_array[entry] != nullptr && size <= curr_entry_size - sizeof(MemoryList::MallocMetadata)) {
                return entry;
            }
        }
        return entry;
    }
};

void* allocateBigBlocks(size_t size) {
    MemoryArray& memArr = MemoryArray::getInstance();
    void* allocMmap = mmap(NULL, size + sizeof(MemoryList::MallocMetadata), PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS, -1, 0);
    if(allocMmap == (void*)(-1)) {
        //TODO:
        return nullptr;
    }
    else {
        MemoryList::MallocMetadata* meta_data = (MemoryList::MallocMetadata*)(allocMmap);
        meta_data->size = size;
        memArr.num_used_blocks++;
        memArr.num_used_bytes += size;
        return allocMmap;
    }
}

MemoryList::MallocMetadata* isMergePossible(MemoryList::MallocMetadata* currMetadata, size_t newSize) {
    MemoryArray& memArr = MemoryArray::getInstance(); 
    int currSize = currMetadata->size + sizeof(MemoryList::MallocMetadata);
    MemoryList::MallocMetadata* currAddress = currMetadata;
    int relevantEntry = memArr.getRelevantEntry(currSize + sizeof(MemoryList::MallocMetadata));
    while (currSize < static_cast<int>(newSize)) {
        MemoryList::MallocMetadata* buddyAddress = memArr.isBuddyInEntry(currAddress, relevantEntry);
        if (buddyAddress != nullptr && relevantEntry != MAX_ORDER) {
            currSize *= 2;
            currAddress = (reinterpret_cast<intptr_t>(currAddress) < reinterpret_cast<intptr_t>(buddyAddress)) ? currAddress : buddyAddress;
            relevantEntry++;
        }
        else {
            return nullptr;
        }
    }
    return currAddress;
}

void* smalloc(size_t size)
{
    MemoryArray& memArr = MemoryArray::getInstance();
    if(size == 0 || size > 1e8)
        return nullptr;

    if (size > MAX_BLOCK_SIZE  - sizeof(MemoryList::MallocMetadata)) {
        return allocateBigBlocks(size);
    }
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

void sfree(void* p) {
    if (p == nullptr) 
        return;

    MemoryArray& memArray = MemoryArray::getInstance();
    MemoryList::MallocMetadata* newFreeMemory = (MemoryList::MallocMetadata*)p - 1;
    if(newFreeMemory->size > MAX_BLOCK_SIZE - sizeof(MemoryList::MallocMetadata)) {
        munmap(newFreeMemory, newFreeMemory->size + sizeof(MemoryList::MallocMetadata));
        memArray.num_used_blocks--;
        memArray.num_used_bytes -= newFreeMemory->size;
    }
    else {
        memArray.insert(newFreeMemory, newFreeMemory->size, 0);
    }
}

void* srealloc(void* oldp, size_t size) {
    MemoryArray& memArray = MemoryArray::getInstance();
    MemoryList::MallocMetadata* meta_data = (MemoryList::MallocMetadata*)oldp - 1;
    if (size == 0 || size > 1e8)
        return nullptr;

    if (oldp == nullptr)
        return smalloc(size);

    if (size > MAX_BLOCK_SIZE - sizeof(MemoryList::MallocMetadata)) {
        if (size == meta_data->size) 
            return oldp; 
        else {
            void* newAddress = allocateBigBlocks(size); // return metadata address
            newAddress = memmove(newAddress, oldp, meta_data->size);
            munmap(oldp, meta_data->size + sizeof(MemoryList::MallocMetadata));
            memArray.num_used_blocks--;
            memArray.num_used_bytes -= meta_data->size;
            return newAddress;
        }
    }

    else {
        if (size <= ((MemoryList::MallocMetadata*)(oldp) - 1)->size) {
            meta_data->size = size;
            return oldp;
        }

        else { 
            MemoryList::MallocMetadata* addressAfterMerge = isMergePossible(meta_data, size);
            if(addressAfterMerge != nullptr) {
                memArray.insert(meta_data, meta_data->size, size);//merge buddy blocks
                int relevantEntry = memArray.getRelevantEntry(size + sizeof(MemoryList::MallocMetadata));
                addressAfterMerge = (MemoryList::MallocMetadata*)memmove(addressAfterMerge, (MemoryList::MallocMetadata*)((char*)oldp - sizeof(MemoryList::MallocMetadata)), meta_data->size);
                memArray.memory_array[relevantEntry]->remove(addressAfterMerge);
                return addressAfterMerge;
            }
            else {
                return smalloc(size);
            }
        }
    }
}

// add the statistics of the mmapped blocks (without munmapped)
size_t _num_free_blocks() {
    MemoryArray& memArray = MemoryArray::getInstance();
    int sum_num_free_blocks = 0;
    for (int i = 0; i < MAX_ORDER + 1; i++) {
        sum_num_free_blocks += memArray.memory_array[i]->num_free_blocks;
    }
    return sum_num_free_blocks;
}

size_t _num_free_bytes() {
    MemoryArray& memArray = MemoryArray::getInstance();
    int sum_num_free_bytes = 0;
    for (int i = 0; i < MAX_ORDER + 1; i++) {
        sum_num_free_bytes += memArray.memory_array[i]->num_free_blocks * (MIN_BLOCK_SIZE * (2 << i));
    }
    return sum_num_free_bytes;
}

size_t _num_allocated_blocks() {
    MemoryArray& memArr = MemoryArray::getInstance();
    return memArr.num_used_blocks + _num_free_blocks();
}

size_t _num_allocated_bytes() {
    MemoryArray& memArr = MemoryArray::getInstance();
    return memArr.num_used_bytes + _num_free_bytes();
}

size_t _size_meta_data() {
    return sizeof(MemoryList::MallocMetadata);
}

size_t _num_meta_data_bytes() { 
    return _num_allocated_blocks() * _size_meta_data();
}
