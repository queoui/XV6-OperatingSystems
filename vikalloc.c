// R. Jesse Chaney
// rchaney@pdx.edu

#include "vikalloc.h"
#define BLOCK_SIZE (sizeof(mem_block_t))

// Get to the user data within the given block.
#define BLOCK_DATA(__curr) (((void *) __curr) + (BLOCK_SIZE))

// Just making it easier to print some pointers.
#define PTR "0x%07lx"
#define PTR_T PTR "\t"

// I use this as the head of the heap map. This is also used in the function
// vikalloc_dump2() to display the contents of the heap map.
static mem_block_t *block_list_head = NULL;

// I use this to use this to point to the last block in the heap map. However,
// I dont' really make use of this for any really good reason.
//static mem_block_t *block_list_tail = NULL;

// I use these as the low water and high water pointer value for the heap
// that bevalloc is using. These are used in vikalloc_dump2().
static void *lower_mem_bound = NULL;
static void *upper_mem_bound = NULL;

// Sometimes we want a chatty allocator, like when debugging.
static uint8_t isVerbose = FALSE;

// This is where all the diagnostic output is sent (like all the stuff from
// vikalloc_dump2()). This is established with some gcc magic. You can alter
// it from the command line into another file.
static FILE *vikalloc_log_stream = NULL;

// The afore memtioned gcc magic.
static void init_streams(void) __attribute__((constructor));

// This is the variable that is used to determine how much space is requested
// from each call to sbrk(). Each call to sbrk() must be a multiple of this
// value.
static size_t min_sbrk_size = DEFAULT_SBRK_SIZE;

// The gcc magic implementation.
static void 
init_streams(void)
{
    vikalloc_log_stream = stderr;
}

// Allows us to chnage the multiple used for calls to sbrk().
size_t
vikalloc_set_min(size_t size)
{
    if (0 == size) {
        // just return the current value
        return min_sbrk_size;
    }
    if (size < (BLOCK_SIZE + BLOCK_SIZE)) {
        // In the event that is is set to something silly.
        size = MAX(BLOCK_SIZE + BLOCK_SIZE, SILLY_SBRK_SIZE);
    }
    min_sbrk_size = size;
    return min_sbrk_size;
}

// To be chatty or not to be chatty...
void 
vikalloc_set_verbose(uint8_t verbosity)
{
    isVerbose = verbosity;
    if (isVerbose) {
        fprintf(vikalloc_log_stream, "Verbose enabled\n");
    }
}

// Redirect the diagnostic output into another file.
void 
vikalloc_set_log(FILE *stream)
{
    vikalloc_log_stream = stream;
}

// This is where the fun begins.
// You need to be able to split the first existing block that can support the
// the resuested size.
void *
vikalloc(size_t size)
{
    void *str = NULL;
    mem_block_t * hold = NULL;
    mem_block_t *curr = NULL;
    int allocate = ((size+BLOCK_SIZE)/(min_sbrk_size))+1;

    if (isVerbose) {
        fprintf(vikalloc_log_stream, ">> %d: %s entry: size = %lu\n"
                , __LINE__, __FUNCTION__, size);
    }

    if (0 == size) {
        return NULL;
    }

    //create the first node
    if(!block_list_head)
    {
        str = sbrk(allocate*min_sbrk_size);
        curr = (mem_block_t*)str;
        lower_mem_bound = curr;
        upper_mem_bound = (void*)curr+(allocate * min_sbrk_size);
        block_list_head = curr;
        block_list_head->size = size;
        block_list_head->capacity = (allocate*min_sbrk_size) - BLOCK_SIZE;
        block_list_head->free = FALSE;
        block_list_head->next = NULL;
        block_list_head->prev = NULL;
    }

    //list exists
    else
    {
        curr = block_list_head;
        while(curr){

            //check if there's room 
            if((curr->capacity - curr->size) >= size) {

                //check if the node is free
                if(curr->free == TRUE)
                {
                    curr->size = size;
                    curr->free = FALSE;
                    return BLOCK_DATA(curr);
                    
                }
                //if we can split, do so
                else if ((curr->capacity - curr->size) >= size+BLOCK_SIZE)
                {
                    hold = curr;
                    str = (void*)curr+BLOCK_SIZE+(curr->size);
                    curr = (mem_block_t*)str;
                    curr->size = size;
                    curr->capacity = ((hold->capacity-(BLOCK_SIZE)) - hold->size);
                    hold->capacity = hold->size;
                    curr ->next = hold->next;
                    hold->next = curr;
                    curr->prev = hold;
                    curr->free = FALSE;
                        if(curr->next&&curr->prev)
                        {
                            curr->next->prev = curr;
                        }
                    return BLOCK_DATA(curr);
                }
            
            }
            
            curr = curr->next;
        }

        curr = block_list_head;
        while(curr->next)
            curr = curr ->next;
    
        //if nothing else above works, allocate some more memory
        str = sbrk(allocate*min_sbrk_size);
        hold = curr;
        curr = (mem_block_t*)str;
        curr->prev = hold;
        hold->next = curr;
        curr->next = NULL;
        upper_mem_bound = (void*)curr +(allocate*min_sbrk_size);
        curr->size = size;
        curr->capacity = (allocate * min_sbrk_size) - BLOCK_SIZE;
        curr->free = FALSE;
    }



    return BLOCK_DATA(curr);
}

// Free the block that contins the passed pointer. You need to coalesce adjacent
// free blocks.
void 
vikfree(void *ptr)
{
#define THINK_ABOUT
#ifdef THINK_ABOUT
    
    mem_block_t *curr = NULL;
    mem_block_t *hold = NULL;
    mem_block_t *del = NULL;
    if(ptr == NULL)
        return;

    //free current 
    curr = ptr - BLOCK_SIZE;
    if(curr->free == FALSE)
    {
        curr->free = TRUE;
        curr->size = 0;
    }
    else
    {
        if (curr->free == TRUE) {
            if (isVerbose) {
                fprintf(vikalloc_log_stream, "Block is already free: ptr = " PTR "\n"
                        , (long) (ptr - lower_mem_bound));
            }
            return;
    }
    }

    //check up
    while(curr -> next && curr->next->free == TRUE)
    {
        del = curr->next;
        curr->next = del->next;

          if(del->next)
          {
            hold = del->next;
            hold->prev = curr;
          }

        curr->capacity += (del->capacity)+BLOCK_SIZE;
        del->prev = del->next = NULL;
        del = NULL;
    }

    //check down
    while(curr->prev && curr->prev->free == TRUE)
    {
        //vikfree(curr->prev);
        curr = curr -> prev;
        while(curr -> next && curr->next->free == TRUE)
        {
            del = curr->next;
            curr->next = del->next;

              if(del->next)
              {
                hold = del->next;
                hold->prev = curr;
              }

            curr->capacity += (del->capacity)+BLOCK_SIZE;
            del->prev = del->next = NULL;
            del = NULL;
        }

    }
    return;

#endif //THINK_ABOUT

}

// Release the kraken, or at least all the vikalloc heap. This should leave
// everything as though it had never been allocated at all. A call to vikalloc
// that follows a call the vikalloc_reset starts compeletely from nothing.
void 
vikalloc_reset(void)
{
    //keep it clean, reset the list
    brk(block_list_head);
    upper_mem_bound = block_list_head;
    lower_mem_bound = block_list_head;
    block_list_head = NULL; 

}

// Is like the regular calloc().
void *
vikcalloc(size_t nmemb, size_t size)
{
    void *str = NULL;
    if(nmemb == 0 || size == 0)
        return NULL;

    //calloc implementation
    str = sbrk(size*min_sbrk_size); 
    block_list_head = (mem_block_t*)str;
    lower_mem_bound = block_list_head;
    upper_mem_bound = (void*)block_list_head+(size * min_sbrk_size);
    block_list_head ->size = size*nmemb;
    block_list_head->capacity = (size*min_sbrk_size) - BLOCK_SIZE;
    block_list_head->free = FALSE;
    block_list_head->next = NULL;
    block_list_head->prev = NULL;
    

    str = block_list_head + BLOCK_SIZE;
    return str;
}

// Like realloc, but simpler.
// If the requested new size does not fit into the current block, create 
// a new block and copy contents. If the requested new size does fit, just 
// adjust the size data member.
void *
vikrealloc(void *ptr, size_t size)
{
    void* hold = ptr;
    mem_block_t *curr = NULL;
    curr = (mem_block_t*)(ptr-BLOCK_SIZE);

    //null ptr? return null
    if(hold == NULL)
    {
        hold = vikalloc(size);
        return hold;
    }

    //size is zero? return null
    if(size == 0){
        vikfree(ptr);
        return NULL;
    }

    //if the size fits, get it in there
    if(curr->capacity >=size)
    {
        curr->size = size;
        return ptr;
    }

    //doesn't fit, allocate more memory and copy the old node
    else
    {
        hold = vikalloc(size);
        memcpy(hold, ptr, curr->capacity);
        vikfree(ptr);
        return hold;
    }

}

// Like the old strdup, but uses vikalloc().
void *
vikstrdup(const char *s)
{
    void *ptr = NULL;

    //make the copy
    ptr = vikalloc(strlen(s)+1);
    strcpy(ptr, s);

    return ptr;
}

// It is totaly gross to include C code like this. But, it pulls out a big
// chunk of code that you don't need to change. It needs access to the static
// variables defined in this module, so it either needs to be included, as
// here, as gross as it may be, or all the code directly in the module.
#include "vikalloc_dump.c"
