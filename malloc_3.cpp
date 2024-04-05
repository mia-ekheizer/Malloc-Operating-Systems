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
        bool is_free;
        size_t size;
        MallocMetadata *next;
        MallocMetadata *prev;
    } MallocMetadata;

MallocMetadata* memory_array[MAX_ORDER + 1];
MallocMetadata* mmap_list;
intptr_t offset;
bool first_smalloc = true;

void initMetaData(MallocMetadata *data, size_t data_size, bool is_free = false) {
    data->size = data_size;
    data->is_free = is_free;
    data->next = NULL;
    data->prev = NULL;
}
// list functions    
void insertToList(int entry, MallocMetadata *newMetadata) // returns a pointer to the data
{
    if (!memory_array[entry]) { // empty list
        memory_array[entry] = newMetadata;
        memory_array[entry]->prev = nullptr;
        memory_array[entry]->next = nullptr;
    }
    else {
        MallocMetadata* temp = memory_array[entry];
        MallocMetadata* previousNode = nullptr;
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
}
//SHIHPUL AHOOSHLOOKI - NICK
void insertToMmapList(MallocMetadata *newMetadata) {
    if(!mmap_list) {
        mmap_list = newMetadata;
        mmap_list->prev = nullptr;
        mmap_list->next = nullptr;
    }
    else {
        MallocMetadata* temp = mmap_list;
        MallocMetadata* previousNode = nullptr;
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
            mmap_list = newMetadata;
        }
        else {
            previousNode->next = newMetadata;
            newMetadata->prev = previousNode;
            newMetadata->next = temp;
        }
    }
}
/*MallocMetadata* removeFromList(int entry) //TODO: (Nick)
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
*/

//remove from list No.entry the node with Add. address
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
//SHIHPOOLIM AHOOLMANYOOKI
void removeFromMmapList(MallocMetadata* address) {
    if (!mmap_list) {
        return;
    }
    MallocMetadata* temp = mmap_list;
    while(temp && temp != address) {
        temp = temp->next;
    }
    if(temp == address) {
        if (!temp->prev && !temp->next) { // remove the only node
            mmap_list = nullptr;
        }
        else if (!temp->prev) { // remove head
            mmap_list = temp->next;
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

size_t getDataSize(int entry) //return the size of the data
{
    return (size_t) ((MIN_BLOCK_SIZE << entry) - sizeof(MallocMetadata));
}
size_t getFullSize(int entry) {
    return (size_t) (MIN_BLOCK_SIZE << entry);
}

// memory array functions
void initMemoryArray(void* progBreak) {
    for (int block_num = 0; block_num < NUM_INITIAL_BLOCKS; block_num++) {
        MallocMetadata* metadata = (MallocMetadata*)progBreak;
        initMetaData(metadata,getDataSize(MAX_ORDER),true);
        insertToList(MAX_ORDER, metadata);
        progBreak = (void*)((unsigned long)progBreak + MAX_BLOCK_SIZE);
    }
}

size_t getListLen(int entry) {
    if (memory_array[entry] == nullptr) {
        return 0;
    }
    size_t len = 0;
    MallocMetadata* curr = memory_array[entry];
    while (curr) {
        len++;
        curr = curr->next;
    }
    return len;
}


 //find the first size of entry that satisfies us
int getRelevantEntry(size_t data_size) {
    int relevantEntry = 0;
    while(getDataSize(relevantEntry) < data_size)
    {
        relevantEntry++;
    }
    return relevantEntry;
}

//find the first block of the minimal size that satisfies us
MallocMetadata* getMinFreeBlock(size_t size) {
    int entry = getRelevantEntry(size);
    for (int i = entry; i < MAX_ORDER+1; i++) { 
        MallocMetadata* newMetadata = memory_array[i];
        while(newMetadata != nullptr) {
            if(newMetadata->is_free) {
                return newMetadata;
            }
            newMetadata = newMetadata->next;
        }
    }
    return nullptr;
}

//merge 2 blocks recursively as much as possible
void* mergeBlock(MallocMetadata* metadata, int order) {
    if(!metadata->is_free) {
        return metadata;
    }
    int relevantEntry = getRelevantEntry(metadata->size);
    if(relevantEntry == order)
        return metadata;

    while(relevantEntry < order) {
        //unsigned long long ptrV = reinterpret_cast<unsigned long long>(metadata);
        unsigned long long fullSize = getFullSize(relevantEntry);
        MallocMetadata* buddyBlock = (MallocMetadata*)((unsigned long long) metadata ^ fullSize);
        if(!buddyBlock->is_free)
            break;
        removeFromList(relevantEntry, buddyBlock);
        removeFromList(relevantEntry, metadata);

        if(metadata > buddyBlock) {
            metadata = buddyBlock;
        }
        initMetaData(metadata,getDataSize(relevantEntry + 1), true);
        insertToList(relevantEntry + 1, metadata);
        relevantEntry++;
    }
    return metadata;
}

MallocMetadata* splitBlock(MallocMetadata* metadata, size_t size) {
    if(metadata == nullptr) 
        return nullptr;
    int relevantEntry = getRelevantEntry(metadata->size);
    if(relevantEntry == 0 || relevantEntry == getRelevantEntry(size))
        return metadata;

    removeFromList(relevantEntry,metadata);
    unsigned long long fullSize = getFullSize(relevantEntry - 1);
    MallocMetadata* buddyBlock = (MallocMetadata*)((unsigned long long)metadata ^ fullSize);
    
    initMetaData(metadata, getDataSize(relevantEntry - 1), true);
    initMetaData(buddyBlock, getDataSize(relevantEntry - 1), true);
    
    insertToList(relevantEntry - 1, metadata);
    insertToList(relevantEntry - 1, buddyBlock);

    return splitBlock(metadata, size);
}
bool isMergePossible(MallocMetadata* metadata, size_t size) {
    int relevantEntry = getRelevantEntry(metadata->size);
    if(relevantEntry == MAX_ORDER)
        return false;
    int finalEntry = getRelevantEntry(size);
    while(relevantEntry < finalEntry) {
        //unsigned long long ptrV = reinterpret_cast<unsigned long long>(metadata);
        unsigned long long fullSize = getFullSize(relevantEntry);
        MallocMetadata* buddyBlock = (MallocMetadata*)((unsigned long long)metadata ^ fullSize);
        if(!buddyBlock->is_free)
            return false;
        if(metadata > buddyBlock)
            metadata = buddyBlock;
        relevantEntry++;
    }
    return true;
}


/*
MallocMetadata* isBuddyInEntry(MallocMetadata* newFreeBlock, int entry) {
    //iterate over the whole list.
    MallocMetadata* memList = memory_array[entry];
    if (!memList) {
        return nullptr;
    }
    MallocMetadata* curr_metadata = memory_array[entry];
    while (curr_metadata->next != nullptr) {
        //calculate if buddy address is in list
        if(curr_metadata == (MallocMetadata*)(((reinterpret_cast<intptr_t>(newFreeBlock) + offset)^(newFreeBlock->size)) - offset)) {
            return curr_metadata;
        }
        curr_metadata = curr_metadata->next;
    }
    return nullptr;
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
    // TODO:(Nick) int relevantEntry = getBiggerThanEqualToEntry(size);
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
*/

// big allocations

size_t calcPageAlignment(size_t size) {
    size_t allocation_size = size + sizeof(MallocMetadata);
    if (allocation_size % PAGE_SIZE == 0) {
        return 0;
    }
    else {
        return (PAGE_SIZE - (allocation_size % PAGE_SIZE));
    }
}

void* allocateBigBlock(size_t size) {
    //size_t alignment = calcPageAlignment(size);
    void* vm_area_address = mmap(NULL, size + sizeof(MallocMetadata) , PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS, -1, 0);
    if(vm_area_address == MAP_FAILED) { //TODO:
        return nullptr;
    }
    else {
        MallocMetadata* metadata = (MallocMetadata*)(vm_area_address);
        initMetaData(metadata,size,false);
        insertToMmapList(metadata);
        return (void*)((unsigned long)metadata + sizeof(MallocMetadata));
    } 
} 




void* smalloc(size_t size)
{

    if(first_smalloc) {
        void* startOfProgram = sbrk(0);
        if(startOfProgram == (void*)(-1)) //TODO:
            return nullptr;
        offset = (intptr_t) (MAX_BLOCK_SIZE * NUM_INITIAL_BLOCKS - ((unsigned long long)startOfProgram %(MAX_BLOCK_SIZE * NUM_INITIAL_BLOCKS)));
        if(sbrk(MAX_BLOCK_SIZE * NUM_INITIAL_BLOCKS + offset) == (void*)(-1))
            return nullptr;
        //we will add the offset, do xor and then subtract the offset
        //offset = (reinterpret_cast<intptr_t>(startOfProgram) - reinterpret_cast<intptr_t>(programBreak)) % (NUM_INITIAL_BLOCKS * MAX_BLOCK_SIZE);
        initMemoryArray((void*)((unsigned long long)startOfProgram + offset));
        first_smalloc = false;
    }

    if(size <= 0 || size > MAX_ALLOC_SIZE) 
        return nullptr;
    else if(size > MAX_BLOCK_SIZE - sizeof(MallocMetadata)) { 
        return allocateBigBlock(size);
    }
    else {
        MallocMetadata* metadata = getMinFreeBlock(size);
        if(metadata != nullptr) {
            metadata = splitBlock(metadata,size);
            metadata->is_free = false;
            return (void*)((unsigned long long)metadata + sizeof(MallocMetadata));
        }
        return nullptr;
    }
    /*
    if (size > MAX_BLOCK_SIZE  - sizeof(MallocMetadata)) {
        return allocateBigBlock(size);
    }
    num_used_blocks++;
    num_used_bytes += size;
    return removeFromMemoryArray(size);
    */
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

    for(int entry = 0; entry <= MAX_ORDER; entry++) {
        MallocMetadata* metadata = memory_array[entry];
        while(metadata != nullptr) {
            if((void*)((unsigned long long)metadata + sizeof(MallocMetadata)) == p) {
                metadata->is_free = true;
                mergeBlock(metadata, MAX_ORDER);
                return;
            }
            metadata = metadata->next;
        }
    }
    
    MallocMetadata* metadata = mmap_list;
    while(metadata != nullptr) {
        if((void*)((unsigned long long)metadata + sizeof(MallocMetadata)) == p){
            removeFromMmapList(metadata);
            //size_t alignment = calcPageAlignment(metadata->size);
            munmap(metadata,metadata->size + sizeof(MallocMetadata));
            return;
        }
        metadata = metadata->next;
    }
    /*
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
    */
}

void* srealloc(void* oldp, size_t size) {
    if (size == 0 || size > MAX_ALLOC_SIZE)
        return nullptr;

    if (oldp == nullptr)
        return smalloc(size);

    MallocMetadata* metadata = mmap_list;
    if (size > MAX_BLOCK_SIZE - sizeof(MallocMetadata)) { //TODO:
        while(metadata != nullptr) {
            if((void*)((unsigned long long)metadata + sizeof(MallocMetadata)) == oldp){
                if(size <= metadata->size)
                    return oldp;
                void* res = smalloc(size);
                if(res == nullptr)
                    return nullptr;
                memmove(res, oldp, metadata->size); // +sizeof + alignment?
                sfree(oldp);
                return res;
            }
            metadata = metadata->next;
        }
    }
    else {
        for(int i = 0; i < MAX_ORDER + 1; i++) {
            metadata = memory_array[i];
            while(metadata != nullptr) {
                if((void*) ((unsigned long long)metadata + sizeof(MallocMetadata)) == oldp) {
                    if(size <= metadata->size) {
                        return oldp;
                    }
                    if(isMergePossible(metadata,size)) {
                        unsigned long long baseSize = metadata->size;
                        metadata->is_free = true;
                        metadata = (MallocMetadata*) mergeBlock(metadata,getRelevantEntry(size));
                        metadata->is_free = false;
                        void* res = (void*)((unsigned long long)metadata + sizeof(MallocMetadata));
                        if(res != oldp) {
                            memmove(res, oldp, baseSize);
                            sfree(oldp);
                        }
                        return res;
                    }
                }
                metadata = metadata->next;
            }
        }
    }
    void* res = smalloc(size);
    if(res == nullptr) {
        return nullptr;
    }
    memmove(res, oldp, metadata->size);//TODO:
    sfree(oldp);
    return res;
}

size_t _num_free_blocks() {
    
    size_t sum = 0;
    for (int i = 0; i < MAX_ORDER + 1; i++) {
        MallocMetadata* metadata = memory_array[i];
        while(metadata != nullptr) {
            if(metadata->is_free)
                sum++;
            metadata = metadata->next;
        }  
    }
    return sum;
}


size_t _num_free_bytes() {
    size_t sum = 0;
    for (int i = 0; i < MAX_ORDER + 1; i++) {
        MallocMetadata* metadata = memory_array[i];
        while(metadata != nullptr) {
            if(metadata->is_free)
                sum += metadata->size;
            metadata = metadata->next;
        }  
    }
    return sum;
}

size_t _num_allocated_blocks() {
    size_t sum = 0;
    for (int i = 0; i < MAX_ORDER + 1; i++) {
        MallocMetadata* metadata = memory_array[i];
        while(metadata != nullptr) {
            sum++;
            metadata = metadata->next;
        }  
    }
    MallocMetadata* metadata = mmap_list;
    while(metadata != nullptr) {
        sum++;
        metadata = metadata->next;
    }
    return sum;
}

size_t _size_meta_data() {
    return sizeof(MallocMetadata);
}


size_t _num_meta_data_bytes() {
    return _num_allocated_blocks() * _size_meta_data();
}

size_t _num_allocated_bytes() {
    size_t sum = 0;
    for (int i = 0; i < MAX_ORDER + 1; i++) {
        MallocMetadata* metadata = memory_array[i];
        while(metadata != nullptr) {
            sum += metadata->size;
            metadata = metadata->next;
        }  
    }
    MallocMetadata* metadata = mmap_list;
    while(metadata != nullptr) {
        sum += metadata->size;
        metadata = metadata->next;
    }
    return sum;
}
