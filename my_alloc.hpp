#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <cassert>
#include <mutex>
using namespace std;


// Computes the base-2-logarithm
int mylog2(size_t x) {
    return __builtin_clz(1) - __builtin_clz(x);
}

/**
 * This header will be stored at the beginning of each allocated block.
 * It is required to have the size of the block available in my_free()
 */
struct Header {
    /**
     * The base-2 logarithm of the size of this block
     */
    int log_size=-1;

    /**
     * A singly linked list of free blocks of the same size
     */
    Header* next_free=nullptr;
};

/**
 * Maximum size allowed to allocate (in log-2)
 */
const int MAX_LOG_ALLOC_SIZE=30;
/**
 * Maximum size allowed to allocate (actual number, NOT log-2)
 */
const int MAX_ALLOC_SIZE = 1<<MAX_LOG_ALLOC_SIZE;
/**
 * Each entry of the free list is a singly linked list of freed block.
 * The index of the free_list is the log_size of the block, i.e.
 * free_list[i] contains a linked list of free blocks, each of which has size 2^i
 */
Header *free_list[MAX_LOG_ALLOC_SIZE+1]={nullptr};
/**
 * Each entry in free_list has its own mutex, at the same position in the vector.
 */
mutex free_list_mutex[MAX_LOG_ALLOC_SIZE+1]={mutex()};
/**
 * Guards the access to the sbrk() syscall
 */
mutex sbrk_mutex;

/**
 * Allocate a new block, or re-use a previously freed block.
 * The header is stored in the first address of the block.
 * The returned address is the first address after the header.
 *
 * @param size The block will be at least this big
 * @return The first address after the header
 */
void* myalloc(size_t size) {
    if(size > MAX_ALLOC_SIZE) {
        cerr<<"Tried to allocate enormous block"<<endl;
        exit(1);
    }

    // All block sizes must be powers of two
    // log_size := the smallest number, s.t. 2^log_size >= size
    // rounded_up_size := 2^log_size, i.e. the final size of the block
    size_t size_with_header = size + sizeof(Header);
    int log_size = mylog2(size_with_header-1)+1; // -1/+1 takes care of rounding up
    long rounded_up_size = ((long)1)<<log_size;
    assert(rounded_up_size > size);

    // This will be the result
    Header *header = nullptr;

    // 1. Check if there exists a previously freed block, of proper size
    // 2. If not, then allocate a new block, by calling sbrk(), which increases heap size

    // Acquire exclusive access to the free_list entry, and check if a block of the correct size is available
    free_list_mutex[log_size].lock();
    if(free_list[log_size] != nullptr) {
        //there is a previously freed block, we can re-use
        header = free_list[log_size];
        free_list[log_size] = header->next_free;
        header->next_free = nullptr;
    }
    free_list_mutex[log_size].unlock();


    if(header==nullptr) {
        // there was no previously freed block of the correct size,
        // thus we allocate a new block

        // Increase heap space
        void *start;
        sbrk_mutex.lock();
        start = sbrk(rounded_up_size);
        sbrk_mutex.unlock();
        if(start == (void*)-1) {
            perror("Sbrk failed");
            exit(1);
        }

        //Write the header at the first address of the newly allocated block
        header = (Header*)start;
        header->next_free = nullptr;
        header->log_size = log_size;
    }

    //return the first address AFTER the header
    return (void*)(header + 1);
}

/**
 * Free a previously allocated block at the given address.
 * @param ptr This must be a value, previouly returned by myalloc()
 */
void myfree(void* ptr) {
    Header* header = ((Header*)ptr)-1; // ptr is a value, previously returned by myalloc, the header precedes the block
    int log_size = header->log_size;

    // Acquire exclusive access to the free_list and insert the freed block at the start of the linked list.
    free_list_mutex[log_size].lock();
    header->next_free = free_list[log_size];
    free_list[log_size] = header;
    free_list_mutex[log_size].unlock();
}
