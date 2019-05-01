# Project 3: Memory Allocator

See: https://www.cs.usfca.edu/~mmalensek/cs326/assignments/project-3.html 

Mushahid Hassan  
CS 326, Section 1  
Professor Malensek  
Project 3  
  
This project wasn't the toughest thing I've ever coded, but it was definitely the most debugging time  
I've ever spent debugging a program. Overall, I actually enjoyed writing a memory allocator. I wasn't  
100% confident with malloc/calloc/realloc/free C functions, so this project helped me gain a better  
understanding of those.  
  
Everything is done in the allocator.c file:  
  
- malloc(size): this function allocates memory for the given size. First it calls the reuse function to check  
                to see if there are any regions within already allocated memory that can fit the new   
                allocation. If there are, then the memory is allocated there. If not, then a new page is  
                created of size 4096 and memory is allocated there.  
                  
- calloc(nmembm, size): this function allocates memory for the given size and sets it equals to 0.  
                        So basically, malloc is performed and given (nmemb * size) for size. Once memory  
                        is allocated, memcpy is performed to set the allocated memory to 0.  
                          
- realloc(ptr, size): this function reallocates the memory for the given size. The struct before the pointer is  
                 checked to see if the region contains enough size to be reallocated. If it does, then it  
                 is reallocted to the given size. If not, then malloc is performed to allocate new memory  
                 of the given size and existing memory from the pointer is copied to the new memory.  
                   
- free(ptr): this function frees the memory at a given pointer. The memory is simply is set to zero at the  
             given pointer. Next it is checked if the entire page is zeroed out. If it is, then entire page  
             page is munmapped and linked list is updated. If not, then just the specific region is zeroed out.  
               
- reuse(size): this function checks to see if any specific allocating algorithms should be followed when  
               allocating memory and those specific algorithms are called. If not, then the default is first fit.  
                 
- first_fit(size): this function is an algorithm to find the first possible region that can allocate the memory  
                   size and allocates the memory there.  
                     
- best_fit(size): this function is an algorithm to find the best/most accurate possible region that can allocate  
                  the memory size and allocates the memory there.  
                    
- worst_fit(size): this function is an algorithm to the worst/biggest possible region that can allocate the memory  
                   size and allocates the memory there.  
                     
- print_memory(): Prints out the current memory state, including both the regions and blocks. Entries are printed  
                  in order, so there is an implied link from the topmost entry to the next, and so on. fprintf is  
                  used and stdout is set to file pointer.  
                    
- write_memory(): Writes out the current memory state, including both the regions and blocks. Entries are printed  
                  in order, so there is an implied link from the topmost entry to the next, and so on. Writes to  
                  file given. fprintf is used and file pointer is set to the given file.
