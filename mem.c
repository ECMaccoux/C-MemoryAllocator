////////////////////////////////////////////////////////////////////////////////
// Main File:        mem.c
// This File:        mem.c
// Other Files:      mem.h, mem.o, libmem.so
// Semester:         CS 354 Spring 2019
//
// Author:           Eric Maccoux
// Email:            emaccoux@wisc.edu
// CS Login:         maccoux
//         
/////////////////////////// OTHER SOURCES OF HELP //////////////////////////////
//                   fully acknowledge and credit all sources of help,
//                   other than Instructors and TAs.
//
// Persons:          Identify persons by name, relationship to you, and email.
//                   Describe in detail the the ideas and help they provided.
//					 N/A
// Online sources:   avoid web searches to solve your problems, but if you do
//                   search, be sure to include Web URLs and description of 
//                   of any information you find.
//					 N/A
////////////////////////////////////////////////////////////////////////////////

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include "mem.h"

/*
 * This structure serves as the header for each allocated and free block.
 * It also serves as the footer for each free block but only containing size.
 */
typedef struct block_header {
        int size_status;
    /*
    * Size of the block is always a multiple of 8.
    * Size is store in all block headers and free block footers.
    *
    * Status is stored only in headers using the two least significant bits.
    *   Bit0 => least significant bit, last bit
    *   Bit0 == 0 => free block
    *   Bit0 == 1 => allocated block
    *
    *   Bit1 => second last bit 
    *   Bit1 == 0 => previous block is free
    *   Bit1 == 1 => previous block is allocated
    * 
    * End Mark: 
    *  The end of the available memory is indicated using a size_status of 1.
    * 
    * Examples:
    * 
    * 1. Allocated block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 27
    *      If the previous block is free, size_status should be 25
    * 
    * 2. Free block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 26
    *      If the previous block is free, size_status should be 24
    *    Footer:
    *      size_status should be 24
    */
} block_header;         

/* Global variable - DO NOT CHANGE. It should always point to the first block,
 * i.e., the block at the lowest address.
 */

block_header *start_block = NULL;

/* 
 * Function for allocating 'size' bytes of heap memory.
 * Argument size: requested size for the payload
 * Returns address of allocated block on success.
 * Returns NULL on failure.
 * This function should:
 * - Check size - Return NULL if not positive or larger than heap space.
 * - Determine block size rounding up to a multiple of 8 and possibly adding padding as a result.
 * - Use BEST-FIT PLACEMENT POLICY to find the block closest to the required block size
 * - Use SPLITTING to divide the chosen free block into two if it is too large.
 * - Update header(s) and footer as needed.
 * Tips: Be careful with pointer arithmetic.
 */
void* Alloc_Mem(int size) {
    
    // Checks for positive inputted size
    if(size <= 0) {
        return NULL;
    }

    // increases size to include enough space for a block header
    size += sizeof(block_header);

    // Round up size to closest multiple of 8 (adding padding)
    while (size % 8 != 0) {
        size++;
    }

    // Defines local variables
    block_header *current_block = start_block;  // current block header being examined
    block_header *best_block = NULL;            // "best" block to allocate
    int previous_block_is_allocated = 1;        // indicator that the last block examined was/was not allocated
    int best_previous_block_is_allocated = -1;  // previous_block_is_allocated, but for best_block

    // Checks for allocated heap memory
    if(current_block == NULL) {
        return NULL;
    }

    // Loops until end of allocated heap memory 
    while(current_block->size_status != 1) {
        // if current_block has already been allocated, set previous_block_is_allocated to 1
        // otherwise, set previous_block_is_allocated is 0
        if((current_block->size_status & 1) != 0) {
            previous_block_is_allocated = 1;
        } else if(current_block->size_status >= size) {
            // if best_block isn't set or if the current_block has a smaller size than best_block,
            // set best_block to current_block
            if(best_block == NULL || best_block->size_status > current_block->size_status) {
                best_block = current_block;
                best_previous_block_is_allocated = previous_block_is_allocated;
            }

            previous_block_is_allocated = 0;
        }

        // set current_block to the next valid block header
        // (amount incremented depends on whether the a or p bits are set)
        if ((current_block->size_status & 3) == 3) {
            current_block = (block_header*)((void*) current_block + (current_block->size_status - 3));
        } else if ((current_block->size_status & 2) == 2) {
            current_block = (block_header*)((void*) current_block + (current_block->size_status - 2));
        } else if ((current_block->size_status & 1) == 1) {
            current_block = (block_header*)((void*) current_block + (current_block->size_status - 1));
        }
    }

    // if best_block is NULL, then there aren't any possible blocks of memory
    // that are large enough to be allocated
    if(best_block == NULL) {
        return NULL;
    }

    // stores the previous size_status of best_block in a variable
    int previous_size_status = best_block->size_status;

    // sets the a-bit of best_block to 1
    best_block->size_status = size + 1;

    // if the block before best_block was allocated, set the p-but of best_block to 1
    if(best_previous_block_is_allocated == 1) {
        best_block->size_status = best_block->size_status + 2;
    }

    // splits best_block and sets the size_status of the new split block's header
    block_header *split_block = (block_header*)((void*) best_block + size);
    
    int split_size = previous_size_status - size;

    // if the size_status of the split block isn't two (meaning that there's a valid block
    // that can be used in the future), then correctly set the header/footer for the split
    // block (otherwise, set the p-bit for the next block header)
    if(split_size != 2) {
        split_block->size_status = split_size;
        block_header *split_footer = (block_header*)((void*) split_block + split_block->size_status - 2 - sizeof(block_header));
        split_footer->size_status = split_block->size_status - 2;
    } else {
        split_block->size_status = split_block->size_status + 2;
    }

    // return a pointer to the start of the data block
    return (void*) ((void*) best_block + sizeof(block_header));
}

/* 
 * Function for freeing up a previously allocated block.
 * Argument ptr: address of the block to be freed up.
 * Returns 0 on success.
 * Returns -1 on failure.
 * This function should:
 * - Return -1 if ptr is NULL.
 * - Return -1 if ptr is not a multiple of 8.
 * - Return -1 if ptr is outside of the heap space.
 * - Return -1 if ptr block is already freed.
 * - USE IMMEDIATE COALESCING if one or both of the adjacent neighbors are free.
 * - Update header(s) and footer as needed.
 */                    
int Free_Mem(void *ptr) {

    // checks for valid inputted ptr
    if(ptr == NULL) {
        return -1;
    }

    // checks to see if ptr is correctly aligned
    unsigned int ptr_value = (unsigned int)ptr;
    if(ptr_value % 8 != 0) {
        return -1;
    }

    // define local variables
    block_header *current_block = start_block;      // block_header that is currently being examined
    int ptr_found = 0;                              // value that keeps track of if the ptr has been found in the heap

    // searches through entire heap for ptr
    // stops when ptr has been found or if end of heap is reached
    while((current_block->size_status != 1) && (ptr_found == 0)) {
        if(current_block == (void*)(ptr - sizeof(block_header))) {
            ptr_found = 1;
        } else {
            // set current_block to the next valid block header
            // (amount incremented depends on whether the a or p bits are set)
            if ((current_block->size_status & 3) == 3) {
                current_block = (block_header*)((void*) current_block + (current_block->size_status - 3));
            } else if ((current_block->size_status & 2) == 2) {
                current_block = (block_header*)((void*) current_block + (current_block->size_status - 2));
            } else if ((current_block->size_status & 1) == 1) {
                current_block = (block_header*)((void*) current_block + (current_block->size_status - 1));
            }
        }
    }

    // checks if ptr was found within heap
    if(ptr_found == 0) {
        return -1;
    }

    // creates ptr_header, which represents the block_header of ptr
    block_header *ptr_header = (block_header*) ((void*) ptr - sizeof(block_header));

    // checks to make sure that the memory block is, in fact, allocated
    if((ptr_header->size_status & 1) == 0) {
        return -1;
    }

    // sets the a-bit to 0
    ptr_header->size_status = ptr_header->size_status - 1;

    // if the p-bit is set, meaning that the previous block is allocated,
    // set the footer for the current block
    // else, coalesce previous block and current block
    if((ptr_header->size_status & 2) == 2) {

        block_header *new_footer = (block_header*) ((void*) ptr_header + (ptr_header->size_status - 2) - sizeof(block_header));
        new_footer->size_status = ptr_header->size_status - 2;

    } else {
        block_header *prev_footer = (block_header*) ((void*) ptr_header - sizeof(block_header));

        int prev_block_size = prev_footer->size_status;

        block_header *prev_block = (block_header*) ((void*) ptr_header - prev_block_size);

        prev_block->size_status = prev_block->size_status + ptr_header->size_status;

        block_header *new_footer = (block_header*) ((void*) ptr_header + ptr_header->size_status - sizeof(block_header));

        new_footer->size_status = (prev_block_size + ptr_header->size_status);

        ptr_header = prev_block;
    }

    // finds the next memory block's header, sets it to next_header
    block_header *next_header = NULL;
    int next_header_size = ptr_header->size_status - 2;

    if((ptr_header->size_status & 2) == 2) {
        next_header = (block_header*) ((void*) ptr_header + next_header_size);
    } else {
        next_header = (block_header*) ((void*) ptr_header + (ptr_header->size_status));
    }

    // sets the p-bit of next_header to 0
    next_header->size_status = next_header->size_status - 2;

    // if needed, coalesce next block and this block
    if((next_header->size_status & 1) == 0) {
        ptr_header->size_status = ptr_header->size_status + next_header->size_status;

        block_header *new_footer = (block_header*) ((void*) next_header + next_header->size_status - sizeof(block_header));

        new_footer->size_status = (ptr_header->size_status - 2);
    }
    
    return 0;
}

/*
 * Function used to initialize the memory allocator.
 * Intended to be called ONLY once by a program.
 * Argument sizeOfRegion: the size of the heap space to be allocated.
 * Returns 0 on success.
 * Returns -1 on failure.
 */                    
int Init_Mem(int sizeOfRegion) {         
    int pagesize;
    int padsize;
    int fd;
    int alloc_size;
    void* space_ptr;
    block_header* end_mark;
    static int allocated_once = 0;
  
    if (0 != allocated_once) {
        fprintf(stderr, 
        "Error:mem.c: Init_Mem has allocated space during a previous call\n");
        return -1;
    }
    if (sizeOfRegion <= 0) {
        fprintf(stderr, "Error:mem.c: Requested block size is not positive\n");
        return -1;
    }

    // Get the pagesize
    pagesize = getpagesize();

    // Calculate padsize as the padding required to round up sizeOfRegion 
    // to a multiple of pagesize
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;

    alloc_size = sizeOfRegion + padsize;

    // Using mmap to allocate memory
    fd = open("/dev/zero", O_RDWR);
    if (-1 == fd) {
        fprintf(stderr, "Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    space_ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, 
                    fd, 0);
    if (MAP_FAILED == space_ptr) {
        fprintf(stderr, "Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }
  
     allocated_once = 1;

    // for double word alignement and end mark
    alloc_size -= 8;

    // To begin with there is only one big free block
    // initialize heap so that start block meets 
    // double word alignement requirement
    start_block = (block_header*) space_ptr + 1;
    end_mark = (block_header*)((void*)start_block + alloc_size);
  
    // Setting up the header
    start_block->size_status = alloc_size;

    // Marking the previous block as used
    start_block->size_status += 2;

    // Setting up the end mark and marking it as used
    end_mark->size_status = 1;

    // Setting up the footer
    block_header *footer = (block_header*) ((char*)start_block + alloc_size - 4);
    footer->size_status = alloc_size;
  
    return 0;
}         
                 
/* 
 * Function to be used for DEBUGGING to help you visualize your heap structure.
 * Prints out a list of all the blocks including this information:
 * No.      : serial number of the block 
 * Status   : free/used (allocated)
 * Prev     : status of previous block free/used (allocated)
 * t_Begin  : address of the first byte in the block (where the header starts) 
 * t_End    : address of the last byte in the block 
 * t_Size   : size of the block as stored in the block header
 */                     
void Dump_Mem() {         
    int counter;
    char status[5];
    char p_status[5];
    char *t_begin = NULL;
    char *t_end = NULL;
    int t_size;

    block_header *current = start_block;
    counter = 1;

    int used_size = 0;
    int free_size = 0;
    int is_used = -1;

    fprintf(stdout, "************************************Block list***\
                    ********************************\n");
    fprintf(stdout, "No.\tStatus\tPrev\tt_Begin\t\tt_End\t\tt_Size\n");
    fprintf(stdout, "-------------------------------------------------\
                    --------------------------------\n");
  
    while (current->size_status != 1) {
        t_begin = (char*)current;
        t_size = current->size_status;
    
        if (t_size & 1) {
            // LSB = 1 => used block
            strcpy(status, "used");
            is_used = 1;
            t_size = t_size - 1;
        } else {
            strcpy(status, "Free");
            is_used = 0;
        }

        if (t_size & 2) {
            strcpy(p_status, "used");
            t_size = t_size - 2;
        } else {
            strcpy(p_status, "Free");
        }

        if (is_used) 
            used_size += t_size;
        else 
            free_size += t_size;

        t_end = t_begin + t_size - 1;
    
        fprintf(stdout, "%d\t%s\t%s\t0x%08lx\t0x%08lx\t%d\n", counter, status, 
        p_status, (unsigned long int)t_begin, (unsigned long int)t_end, t_size);
    
        current = (block_header*)((char*)current + t_size);
        counter = counter + 1;
    }

    fprintf(stdout, "---------------------------------------------------\
                    ------------------------------\n");
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fprintf(stdout, "Total used size = %d\n", used_size);
    fprintf(stdout, "Total free size = %d\n", free_size);
    fprintf(stdout, "Total size = %d\n", used_size + free_size);
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fflush(stdout);

    return;
}         
