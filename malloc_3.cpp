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

MallocMetadata* memory_array[MAX_ORDER + 1] = {nullptr};
MallocMetadata* mmap_list = nullptr;
unsigned long long offset;
bool first_smalloc = true;

void initMetaData(MallocMetadata *data, size_t data_size, bool is_free = false) {
    data->size = data_size;
    data->is_free = is_free;
    data->next = nullptr;
    data->prev = nullptr;
}
// list functions    
void insertToList(MallocMetadata** head, MallocMetadata *metadata)
{
    if (*head == NULL || *head > metadata) {
        metadata->next = *head;
        metadata->prev = NULL;
        if (*head != NULL) {
            (*head)->prev = metadata;
        }
        *head = metadata;
        return;
    }
    MallocMetadata* curr = *head;
    while (curr->next != NULL && curr->next < metadata) {
        curr = curr->next;
    }
    metadata->next = curr->next;
    metadata->prev = curr;
    if (curr->next != NULL) {
        curr->next->prev = metadata;
    }
    curr->next = metadata;
}



void removeFromList(MallocMetadata** head, MallocMetadata* metadata) {
    if (*head == NULL || *head != metadata) {
        MallocMetadata* curr = *head;
        while (curr != NULL && curr != metadata) {
            curr = curr->next;
        }
        if (curr == NULL) {
            return; 
        }
        if (curr->prev != NULL) {
            curr->prev->next = curr->next;
        }
        if (curr->next != NULL) {
            curr->next->prev = curr->prev;
        }
        return;
    }
    *head = metadata->next;
    if (*head != NULL) {
        (*head)->prev = NULL;
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
        insertToList(&memory_array[MAX_ORDER], metadata);
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
    MallocMetadata* minFreeBlock = nullptr;   
    for (int i = entry; i < MAX_ORDER + 1; i++) { 
        MallocMetadata* currentMetadata = memory_array[i];
        while (currentMetadata != nullptr) {
            if (currentMetadata->is_free) {
                minFreeBlock = currentMetadata;
                return minFreeBlock;
            }
            currentMetadata = currentMetadata->next;
        }
    }
    return minFreeBlock;
}


//merge 2 blocks recursively as much as possible until order
void* mergeBlock(MallocMetadata* metadata, int order) {
    if(!metadata->is_free || getRelevantEntry(metadata->size) == order) {
        return metadata;
    }
    int relevantEntry = getRelevantEntry(metadata->size);
    while(relevantEntry < order) {
        //unsigned long long ptrV = reinterpret_cast<unsigned long long>(metadata);
        unsigned long long fullSize = getFullSize(relevantEntry);
        MallocMetadata* buddyBlock = (MallocMetadata*)((unsigned long long) metadata ^ fullSize);
        MallocMetadata* currNode = memory_array[relevantEntry];
        while(currNode != nullptr) {
            if(currNode == buddyBlock)
                break;
            currNode = currNode->next;
        }
        if(currNode == nullptr || !currNode->is_free) {
            break;
        }
        removeFromList(&memory_array[relevantEntry], buddyBlock);
        removeFromList(&memory_array[relevantEntry], metadata);

        if(metadata > buddyBlock) {
            metadata = buddyBlock;
        }
        initMetaData(metadata, getDataSize(relevantEntry + 1), true);
        relevantEntry = getRelevantEntry(metadata->size);
        insertToList(&memory_array[relevantEntry], metadata);
    }
    return metadata;
}

MallocMetadata* splitBlock(MallocMetadata* metadata, size_t size) {
    if(metadata == nullptr) 
        return nullptr;
    int relevantEntry = getRelevantEntry(metadata->size);
    if(relevantEntry == 0 || relevantEntry == getRelevantEntry(size))
        return metadata;

    removeFromList(&memory_array[relevantEntry], metadata);
    unsigned long long fullSize = getFullSize(relevantEntry - 1);
    MallocMetadata* buddyBlock = (MallocMetadata*)((unsigned long long)metadata ^ fullSize);
    
    initMetaData(metadata, getDataSize(relevantEntry - 1), true);
    initMetaData(buddyBlock, getDataSize(relevantEntry - 1), true);
    
    insertToList(&memory_array[relevantEntry - 1], metadata);
    insertToList(&memory_array[relevantEntry - 1], buddyBlock);

    return splitBlock(metadata, size);
}
bool isMergePossible(MallocMetadata* metadata, size_t size) {
    int relevantEntry = getRelevantEntry(metadata->size);
    int finalEntry = getRelevantEntry(size);
    for (int i = relevantEntry; i < finalEntry; i++) {
        MallocMetadata* buddyBlock = (MallocMetadata*)((unsigned long long)metadata ^ getFullSize(i));
        MallocMetadata* currNode = memory_array[i];
        while (currNode != nullptr) {
            if (currNode == buddyBlock && currNode->is_free) {
                break;
            }
            currNode = currNode->next;
        }
        if (currNode == nullptr) { 
            return false;
        }     
        if (!buddyBlock->is_free) {
            return false;
        }
        metadata = (metadata > buddyBlock) ? buddyBlock : metadata;
    }

    return (relevantEntry != MAX_ORDER);
}

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
        insertToList(&mmap_list, metadata);
        return (void*)((unsigned long)metadata + sizeof(MallocMetadata));
    } 
} 




void* smalloc(size_t size)
{

    if(first_smalloc) {
        void* startOfProgram = sbrk(0);
        if(startOfProgram == (void*)(-1)) 
            return nullptr;
        offset = (unsigned long long) (MAX_BLOCK_SIZE * NUM_INITIAL_BLOCKS - ((unsigned long long)startOfProgram % (MAX_BLOCK_SIZE * NUM_INITIAL_BLOCKS)));
        if(sbrk(MAX_BLOCK_SIZE * NUM_INITIAL_BLOCKS + offset) == (void*)(-1))
            return nullptr;
        //we will add the offset, do xor and then subtract the offset
        //offset = (reinterpret_cast<intptr_t>(startOfProgram) - reinterpret_cast<intptr_t>(programBreak)) % (NUM_INITIAL_BLOCKS * MAX_BLOCK_SIZE);
        startOfProgram = (void*)((unsigned long long)startOfProgram + offset);
        initMemoryArray(startOfProgram);
        first_smalloc = false;
    }

    if(size <= 0 || size > MAX_ALLOC_SIZE) 
        return nullptr;
    else if(size > MAX_BLOCK_SIZE -sizeof(MallocMetadata)) {
        void* vm_area_address = mmap(NULL, size + sizeof(MallocMetadata) , PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if(vm_area_address == MAP_FAILED) { 
            return nullptr;
        }
        else {
            MallocMetadata* metadata = (MallocMetadata*)(vm_area_address);
            initMetaData(metadata,size,false);
            insertToList(&mmap_list, metadata);
            return (void*)((unsigned long)metadata + sizeof(MallocMetadata));
        } 
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
            removeFromList(&mmap_list, metadata);
            //size_t alignment = calcPageAlignment(metadata->size);
            munmap(metadata, metadata->size + sizeof(MallocMetadata));
            return;
        }
        metadata = metadata->next;
    }
}

void* srealloc(void* oldp, size_t size) {
    if (size <= 0 || size > MAX_ALLOC_SIZE)
        return nullptr;

    if (oldp == nullptr)
        return smalloc(size);
    size_t old_size = ((MallocMetadata*)((unsigned long long)oldp - sizeof(MallocMetadata)))->size;
    MallocMetadata* metadata = mmap_list;
    while(metadata != nullptr) {
        if((void*)((unsigned long long)metadata + sizeof(MallocMetadata)) == oldp){
            if(size <= metadata->size)
                return oldp;
            void* res = smalloc(size);
            if(res == nullptr)
                return nullptr;
            //size_t alignment = calcPageAlignment(metadata->size);
            memmove(res, oldp, metadata->size); //  + alignment + sizeof(MallocMetadata)
            sfree(oldp);
            return res;
        }
        metadata = metadata->next;
    }
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
    void* res = smalloc(size);
    if(res == nullptr) {
        return nullptr;
    }
    memmove(res, oldp, old_size);
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
