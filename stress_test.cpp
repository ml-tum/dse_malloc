#include <iostream>
#include <random>
#include <cstring>
#include <thread>
#include "my_alloc.hpp"
using namespace std;

const int ITERATIONS = 1e7;
const int MAX_BYTES_TO_ALLOCATE = 8192;
const int MAX_ALLOCATIONS = 100;

void demo() {
    // initialize random number generator
    mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());

    // Keep track of the allocations, so we're able to free them
    int allocation_count=0;
    byte* allocations[MAX_ALLOCATIONS];

    for(int i=0; i<ITERATIONS; i++) {

        // whether to allocate or free a block
        int alloc_or_free = uniform_int_distribution<int>(0, 1)(rng);

        if(alloc_or_free==1 && allocation_count>0 || allocation_count==MAX_ALLOCATIONS) {
            // free a random block
            int position_to_free = uniform_int_distribution<int>(0, allocation_count-1)(rng);
            myfree(allocations[position_to_free]);

            // erase the freed block from the list of allocations
            allocations[position_to_free] = allocations[allocation_count - 1];
            allocations[allocation_count - 1] = nullptr;
            allocation_count--;
        } else {
            // allocate a block with a random size
            int size = uniform_int_distribution<int>(0, MAX_BYTES_TO_ALLOCATE)(rng);
            byte* ptr = (byte*) myalloc((size_t)(size * sizeof(byte)));

            // write every byte of the block
            memset(ptr, 42, size * sizeof(byte));

            // keep track of the allocation
            allocations[allocation_count] = ptr;
            allocation_count++;
        }

    }
}

int main() {

    std::thread t1(demo);
    std::thread t2(demo);

    cout<<"Start"<<endl;
    t1.join();
    t2.join();
    cout<<"Finish"<<endl;

    return 0;
}
