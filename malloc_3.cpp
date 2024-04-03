#include <unistd.h>
#include <string.h>
#include <cstring>
#include <sys/mman.h>

#define MAX_ORDER 10
#define KB 1024
#define MAX_BLOCK_SIZE (128 * KB)
#define NUM_INITIAL_BLOCKS 32
#define MIN_BLOCK_SIZE 128
#define PAGE_SIZE (4 * KB)
#define MAX_ALLOC_SIZE 100000000

// main structs and variables
typedef struct MallocMetadata
    {
        size_t size;
        MallocMetadata *next;
        MallocMetadata *prev;
    } MallocMetadata;

MallocMetadata* memory_array[MAX_ORDER + 1];
int num_big_blocks = 0;
int num_big_bytes = 0;
int offset;
bool first_smalloc = true;
int num_used_blocks;
int num_used_bytes;

// list functions    
void* insertToList(int entry, MallocMetadata *newMetadata)
{
    if (!memory_array[entry]) { // empty list
        memory_array[entry] = newMetadata;
        memory_array[entry]->prev = nullptr;
        memory_array[entry]->next = nullptr;
    }
    else {
        MallocMetadata *temp = memory_array[entry];
        MallocMetadata *previousNode = nullptr;
        while(temp && temp < newMetadata) {//find correct position
            previousNode = temp;
            temp = temp->next;
        }
        if (!temp) { // newMetadata has the highest address, insert as tail
            previousNode->next = newMetadata;
            newMetadata->prev = previousNode;
            newMetadata->next = nullptr;
        }
        else if (!temp->prev) { // insert as head
            temp->prev = newMetadata;
            newMetadata->prev = nullptr;
            memory_array[entry] = newMetadata;
        }
        else {
            previousNode->next = newMetadata;
            newMetadata->prev = previousNode;
            newMetadata->next = temp;
        }
    }
    return newMetadata + 1;
}

MallocMetadata* removeFromList(int entry)
{
    if (!memory_array[entry]) { // list is empty
        return nullptr;
    }
    MallocMetadata* toRemove = memory_array[entry];
    if (!toRemove->next) { // remove the only node
        memory_array[entry] = nullptr;
    }
    else {
        memory_array[entry] = toRemove->next;
        toRemove->next->prev = nullptr;
    }
    return toRemove;
}
    
void removeFromList(int entry, MallocMetadata* address) {
    if (!memory_array[entry]) {
        return;
    }
    MallocMetadata* temp = memory_array[entry];
    while(temp && temp != address) {
        temp = temp->next;
    }
    if(temp == address) {
        if (!temp->prev && !temp->next) { // remove the only node
            memory_array[entry] = nullptr;
        }
        else if (!temp->prev) { // remove head
            memory_array[entry] = temp->next;
            temp->next->prev = nullptr;
        }
        else if (!temp->next) { //remove tail
            temp->prev->next = nullptr;
        }
        else {
            temp->prev->next = temp->next;
            temp->next->prev = temp->prev;
        }
    }
}

size_t getListSize(int entry) {
    if (memory_array[entry] == nullptr) {
        return 0;
    }
    size_t size = 0;
    MallocMetadata* curr = memory_array[entry];
    while (curr) {
        size++;
        curr = curr->next;
    }
    return size;
}

// memory array functions
void initMemoryArray() {
    for(int i = 0; i <= MAX_ORDER; i++) {
        memory_array[i] = nullptr;
    }
    void* programBreak = sbrk(MAX_BLOCK_SIZE * NUM_INITIAL_BLOCKS);
    void* newProgramBreak = sbrk(0);
    // if(newAddress == (void*)(-1)) //TODO: is necessary?
    
    offset = (reinterpret_cast<intptr_t>(newProgramBreak) - reinterpret_cast<intptr_t>(programBreak)) % (NUM_INITIAL_BLOCKS * MAX_BLOCK_SIZE);
    for (int block_num = 0; block_num < NUM_INITIAL_BLOCKS; block_num++) {
        MallocMetadata* initMetadata = (MallocMetadata*)(programBreak);
        insertToList(MAX_ORDER, initMetadata);
        programBreak = (void*)((unsigned long)programBreak + MAX_BLOCK_SIZE);
    }
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

int getBiggerThanEqualToEntry(size_t size) {
    int curr_entry_size;
    int entry;
    // find the first index that has a fitting/bigger size block
    for (entry = 0; entry < MAX_ORDER+1; entry++) { 
        curr_entry_size = MIN_BLOCK_SIZE * (1 << entry);
        if (memory_array[entry] != nullptr && size <= curr_entry_size - sizeof(MallocMetadata)) {
            return entry;
        }
    }
    return entry;
}
    
MallocMetadata* isBuddyInEntry(MallocMetadata* newFreeBlock, int entry) {
    //iterate over the whole list.
    MallocMetadata* memList = memory_array[entry];
    if (!memList) {
        return nullptr;
    }
    MallocMetadata* curr_metadata = memory_array[entry];
    while (curr_metadata->next != nullptr) {
        if(curr_metadata == (MallocMetadata*)((reinterpret_cast<intptr_t>(newFreeBlock) - offset)^((newFreeBlock->size) + offset))) {
            return curr_metadata;
        }
        curr_metadata = curr_metadata->next;
    }
    return nullptr;
}

MallocMetadata* isMergePossible(MallocMetadata* currMetadata, size_t newSize) {
    int currSize = currMetadata->size + sizeof(MallocMetadata);
    MallocMetadata* currAddress = currMetadata;
    int relevantEntry = getRelevantEntry(currSize + sizeof(MallocMetadata));
    while (currSize < static_cast<int>(newSize)) {
        MallocMetadata* buddyAddress = isBuddyInEntry(currAddress, relevantEntry);
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
    
MallocMetadata* removeAndMerge(MallocMetadata* firstBuddyAddress, MallocMetadata* secondBuddyAddress) {
    int entry = getRelevantEntry(firstBuddyAddress->size + sizeof(MallocMetadata));
    removeFromList(entry, firstBuddyAddress);
    removeFromList(entry, secondBuddyAddress);
    MallocMetadata* parent = (reinterpret_cast<intptr_t>(firstBuddyAddress) < reinterpret_cast<intptr_t>(secondBuddyAddress)) ? firstBuddyAddress : secondBuddyAddress;
    parent->size = 2 * firstBuddyAddress->size;
    return parent;
}

// insert in an ascending order by the address, plus combine buddies iteratively
void insertToMemoryArray(MallocMetadata* newFreeBlock, size_t originalSize, size_t desiredSize) { 
    int relevantEntry = getRelevantEntry(newFreeBlock->size + sizeof(MallocMetadata));
    MallocMetadata* buddyAddress = isBuddyInEntry(newFreeBlock, relevantEntry);
    MallocMetadata* parent = buddyAddress;
    if (buddyAddress != nullptr && relevantEntry != MAX_ORDER) {
        if (desiredSize == 0 || (desiredSize != 0 && parent->size < desiredSize)) {
            parent = removeAndMerge(newFreeBlock, buddyAddress);
            insertToMemoryArray(parent, originalSize, desiredSize);
        }
    }
    else {
        insertToList(relevantEntry, newFreeBlock);
    }
}

//remove a block in the minimal size possible
void* removeFromMemoryArray(size_t size) { 
    // find a big enough block so we can split it later
    int relevantEntry = getBiggerThanEqualToEntry(size);
    int relevantEntrySize = MIN_BLOCK_SIZE * (1 << relevantEntry);
    // split the blocks until we get a block of the correct size
    while (size <= (relevantEntrySize / 2 - sizeof(MallocMetadata))) { 
        MallocMetadata* wantedAddress = removeFromList(relevantEntry);
        MallocMetadata* buddyAddress = (MallocMetadata*)(reinterpret_cast<intptr_t>(wantedAddress) + relevantEntrySize / 2);
        insertToList(relevantEntry - 1, buddyAddress);
        insertToList(relevantEntry - 1, wantedAddress);
        relevantEntrySize /= 2;
        relevantEntry--;
    }
    relevantEntrySize *= 2;
    relevantEntry++;                         
    return (void*)(removeFromList(relevantEntry) + sizeof(MallocMetadata));
}

// big allocations
size_t calcPageAlignment(size_t size) {
    size_t allocation_size = size + sizeof(MallocMetadata);
    if (allocation_size % PAGE_SIZE == 0) {
        return allocation_size;
    }
    else {
        size_t alignment = PAGE_SIZE - (allocation_size % PAGE_SIZE);
        return allocation_size + alignment;
    }
}

void* allocateBigBlock(size_t size) {
    size_t allocation_size = calcPageAlignment(size);
    void* vm_area_address = mmap(NULL, allocation_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS, -1, 0);
    if(vm_area_address == (void*)(-1)) {
        //TODO:
        return nullptr;
    }
    else {
        MallocMetadata* meta_data = (MallocMetadata*)(vm_area_address);
        meta_data->size = size;
        num_big_blocks++;
        num_big_bytes += size;
        return vm_area_address;
    }
}

void* smalloc(size_t size)
{
    if (first_smalloc) {
        first_smalloc = false;
        initMemoryArray();
    }
    
    if(size == 0 || size > MAX_ALLOC_SIZE)
        return nullptr;

    if (size > MAX_BLOCK_SIZE  - sizeof(MallocMetadata)) {
        return allocateBigBlock(size);
    }
    num_used_blocks++;
    num_used_bytes += size;
    return removeFromMemoryArray(size);
}

void* scalloc(size_t num, size_t size) {
    if (num == 0 || size == 0 || size * num > MAX_ALLOC_SIZE) 
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

    MallocMetadata* newFreeMemory = (MallocMetadata*)p - 1;
    if(newFreeMemory->size > MAX_BLOCK_SIZE - sizeof(MallocMetadata)) {
        munmap(newFreeMemory, newFreeMemory->size + sizeof(MallocMetadata));
        num_big_blocks--;
        num_big_bytes -= newFreeMemory->size;
    }
    else {
        num_used_blocks--;
        num_used_bytes -= newFreeMemory->size;
        insertToMemoryArray(newFreeMemory, newFreeMemory->size, 0);
    }
}

void* srealloc(void* oldp, size_t size) {
    MallocMetadata* meta_data = (MallocMetadata*)oldp - 1;
    if (size == 0 || size > MAX_ALLOC_SIZE)
        return nullptr;

    if (oldp == nullptr)
        return smalloc(size);

    if (size > MAX_BLOCK_SIZE - sizeof(MallocMetadata)) {
        if (size == meta_data->size) {
            return oldp;
        } 
             
        else {
            void* newAddress = allocateBigBlock(size); // returns metadata address
            newAddress = memmove(newAddress, oldp, meta_data->size);
            munmap(oldp, meta_data->size + sizeof(MallocMetadata));
            num_big_blocks--;
            num_big_bytes -= meta_data->size;
            return newAddress;
        }
    }

    else {
        if (size <= ((MallocMetadata*)(oldp) - 1)->size) {
            meta_data->size = size;
            return oldp;
        }

        else { 
            MallocMetadata* addressAfterMerge = isMergePossible(meta_data, size);
            if(addressAfterMerge != nullptr) {
                insertToMemoryArray(meta_data, meta_data->size, size);//merge buddy blocks
                int relevantEntry = getRelevantEntry(size + sizeof(MallocMetadata));
                addressAfterMerge = (MallocMetadata*)memmove(addressAfterMerge, (MallocMetadata*)((char*)oldp - sizeof(MallocMetadata)), meta_data->size);
                removeFromList(relevantEntry, addressAfterMerge);
                return addressAfterMerge;
            }
            else {
                return smalloc(size);
            }
        }
    }
}

size_t _num_free_blocks() {
    int sum_num_free_blocks = 0;
    for (int i = 0; i <= MAX_ORDER + 1; i++) {
        sum_num_free_blocks += getListSize(i);
    }
    return sum_num_free_blocks;
}

size_t _size_meta_data() {
    return sizeof(MallocMetadata);
}

size_t _num_free_bytes() {
    int sum_num_free_bytes = 0;
    for (int i = 0; i <= MAX_ORDER + 1; i++) {
        sum_num_free_bytes += getListSize(i) * ((MIN_BLOCK_SIZE * (1 << i)) - _size_meta_data());
    }
    return sum_num_free_bytes;
}

size_t _num_allocated_blocks() {
    if (!first_smalloc) {
        return num_big_blocks + num_used_blocks + _num_free_blocks();
    }
    return num_big_blocks;
}

size_t _num_meta_data_bytes() {
    return _num_allocated_blocks() * _size_meta_data();
}

size_t _num_allocated_bytes() {
    if (!first_smalloc) {
        return num_big_bytes + _num_allocated_blocks() - _num_meta_data_bytes();
    }
    else if (num_big_blocks != 0) {
        return num_big_bytes - _num_meta_data_bytes();
    }
    else {
        return 0;
    }
}
