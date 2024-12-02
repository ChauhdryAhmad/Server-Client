#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

#define INITIAL_HEAP_SIZE 1024 
#define BIN_COUNT 10           
#define BIN_STEP 16            

typedef struct Block 
{
    unsigned int size;      
    bool is_free;           
    struct Block* next;     
} Block;

void* heap_start = NULL;         
unsigned int heap_size = INITIAL_HEAP_SIZE; 
Block* free_bins[BIN_COUNT];     


unsigned int align_size(unsigned int size) 
{
    unsigned int remainder = size % BIN_STEP;
    
    if (remainder == 0) 
    {
        return size;
    }
    
    unsigned int aligned_size = size + (BIN_STEP - remainder);
    return aligned_size;
}


int get_bin_index(unsigned int size) 
{
    return (size / BIN_STEP < BIN_COUNT) ? size / BIN_STEP : BIN_COUNT - 1;
}


void init_heap() 
{
    heap_start = sbrk(INITIAL_HEAP_SIZE);
    if (heap_start == (void*)-1) 
    {
        perror("sbrk failed");
        exit(EXIT_FAILURE);
    }
    memset(free_bins, 0, sizeof(free_bins));
}


void grow_heap() 
{
    if (sbrk(heap_size) == (void*)-1) 
    {
        perror("sbrk failed");
        exit(EXIT_FAILURE);
    }
    heap_size *= 2; 
}

void* my_malloc(unsigned int size) 
{
    if (size == 0) 
      return NULL;
    size = align_size(size);

    int bin_index = get_bin_index(size);
    for (int i = bin_index; i < BIN_COUNT; i++) 
    {
        if (free_bins[i]) 
        {
            Block* block = free_bins[i];
            free_bins[i] = block->next;
            block->is_free = false;
            return (void*)((char*)block + sizeof(Block));
        }
    }

    if (!heap_start) 
        init_heap();
    static char* heap_top = NULL;
    if (!heap_top) heap_top = (char*)heap_start;

    if ((char*)heap_start + heap_size - heap_top < size + sizeof(Block)) 
    {
        grow_heap(); 
    }

    Block* block = (Block*)heap_top;
    block->size = size;
    block->is_free = false;
    block->next = NULL;

    heap_top += sizeof(Block) + size;
    return (void*)((char*)block + sizeof(Block));
}

void my_free(void* ptr) 
{
    if (!ptr) 
     return;

    Block* block = (Block*)((char*)ptr - sizeof(Block));
    block->is_free = true;

    int bin_index = get_bin_index(block->size);
    block->next = free_bins[bin_index];
    free_bins[bin_index] = block;
}


void print_bins() 
{
    for (int i = 0; i < BIN_COUNT; i++) 
    {
        printf("Bin %d: ", i);
        if (free_bins[i] == NULL) 
        {
            printf("empty\n");
        } 
        else
        {
            printf("occupied\n");
        }
    }
}

// int main() 
// {
    
//     if (!heap_start) 
//       init_heap();

    
//     printf("State of bins before allocation:\n");
//     print_bins();

    
//     void* ptr1 = my_malloc(64);
//     void* ptr2 = my_malloc(128);
//     void* ptr3 = my_malloc(32);
//     void* ptr4 = my_malloc(256);
//     void* ptr5 = my_malloc(16);
    
//     printf("\nAllocated: %p, %p, %p, %p, %p\n", ptr1, ptr2, ptr3, ptr4, ptr5);

    
//     printf("\nState of bins after allocation:\n");
//     print_bins();

    
//     my_free(ptr2);
//     my_free(ptr4);
    
//     printf("\nFreed: %p, %p\n", ptr2, ptr4);

    
//     printf("\nState of bins after freeing some blocks:\n");
//     print_bins();

    
//     void* ptr6 = my_malloc(128);
//     void* ptr7 = my_malloc(64);
    
//     printf("\nAllocated: %p, %p\n", ptr6, ptr7);

    
//     printf("\nState of bins after additional allocations:\n");
//     print_bins();

//     return 0;
// }
