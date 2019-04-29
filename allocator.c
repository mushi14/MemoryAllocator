/**
 * allocator.c
 *
 * Explores memory management at the C runtime level.
 *
 * Author: <your team members go here>
 *
 * To use (one specific command):
 * LD_PRELOAD=$(pwd)/allocator.so command
 * ('command' will run with your allocator)
 *
 * To use (all following commands):
 * export LD_PRELOAD=$(pwd)/allocator.so
 * (Everything after this point will use your custom allocator -- be careful!)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include <pthread.h>
#include <limits.h>

#ifndef DEBUG
#define DEBUG 1
#endif

#define LOG(fmt, ...) \
        do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)

#define LOGP(str) \
    do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): %s", __FILE__, \
            __LINE__, __func__, str); } while (0)

struct mem_block {
    /* Each allocation is given a unique ID number. If an allocation is split in
     * two, then the resulting new block will be given a new ID. */
    unsigned long alloc_id;

    /* Size of the allocation */
    size_t size;

    /* Space used; if usage == 0, then the block has been freed. */
    size_t usage;

    /* Pointer to the start of the mapped memory region. This simplifies the
     * process of finding where memory mappings begin. */
    struct mem_block *region_start;

    /* If this block is the beginning of a mapped memory region, the region_size
     * member indicates the size of the mapping. In subsequent (split) blocks,
     * this is undefined. */
    size_t region_size;

    /* Next block in the chain */
    struct mem_block *next;
};

/* Start (head) of our linked list: */
struct mem_block *g_head = NULL;

/* Allocation counter: */
unsigned long g_allocations = 0;

pthread_mutex_t g_alloc_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * print_memory
 *
 * TODO: Make this not allocate stuff
 *
 * Prints out the current memory state, including both the regions and blocks.
 * Entries are printed in order, so there is an implied link from the topmost
 * entry to the next, and so on.
 */
void print_memory(void) {
    FILE *fp = NULL;

    if (fp == NULL) {
        fp = stdout;
        fprintf(fp, "-- Current Memory State --\n");
        struct mem_block *current_block = g_head;
        struct mem_block *current_region = NULL;
        while (current_block != NULL) {
            if (current_block->region_start != current_region) {
                current_region = current_block->region_start;
                fprintf(fp, "[REGION] %p-%p %zu\n",
                        current_region,
                        (void *) current_region + current_region->region_size,
                        current_region->region_size);
            }
            fprintf(fp, "[BLOCK]  %p-%p (%ld) %zu %zu %zu\n",
                    current_block,
                    (void *) current_block + current_block->size,
                    current_block->alloc_id,
                    current_block->size,
                    current_block->usage,
                    current_block->usage == 0
                        ? 0 : current_block->usage - sizeof(struct mem_block));
            current_block = current_block->next;
        }
    }

    fclose(fp);
}

void write_memory(FILE *fp) {
    if (fp == NULL) {
        return;
    }

    fprintf(fp, "-- Current Memory State --\n");
    struct mem_block *current_block = g_head;
    struct mem_block *current_region = NULL;
    while (current_block != NULL) {
        if (current_block->region_start != current_region) {
            current_region = current_block->region_start;
            fprintf(fp, "[REGION] %p-%p %zu\n",
                    current_region,
                    (void *) current_region + current_region->region_size,
                    current_region->region_size);
        }
        fprintf(fp, "[BLOCK]  %p-%p (%ld) %zu %zu %zu\n",
                current_block,
                (void *) current_block + current_block->size,
                current_block->alloc_id,
                current_block->size,
                current_block->usage,
                current_block->usage == 0
                    ? 0 : current_block->usage - sizeof(struct mem_block));
        current_block = current_block->next;
    }

    fclose(fp);
}

void *first_fit(size_t size) {
	struct mem_block *block = g_head;
	size_t real_size = size + sizeof(struct mem_block);

    if (block == NULL) {
        pthread_mutex_unlock(&g_alloc_mutex);
        return NULL;
    }

	while (block != NULL) {
		if (block->size - block->usage >= real_size) {
            if (block->usage == 0) {
                block->alloc_id = g_allocations++;
                block->usage = real_size;
                pthread_mutex_unlock(&g_alloc_mutex);
                return block + 1;
            } else {
                struct mem_block *new_block = (void*) block + block->usage;

                new_block->alloc_id = g_allocations++;
                new_block->size = block->size - block->usage;
                new_block->usage = real_size;
                new_block->region_start = block->region_start;
                new_block->region_size = block->region_size;
                new_block->next = block->next;

                block->next = new_block;
                block->size = block->usage;
                pthread_mutex_unlock(&g_alloc_mutex);
                return new_block + 1;
            }
		}

        block = block->next;
    }

    pthread_mutex_unlock(&g_alloc_mutex);
    return NULL;
}

void *best_fit(size_t size) {
    struct mem_block *block = g_head;
    struct mem_block *temp_block = g_head;

    size_t real_size = size + sizeof(struct mem_block);
    int counter = 0;
    int best = INT_MAX;
    int index = 0;

    if (block == NULL) {
        pthread_mutex_unlock(&g_alloc_mutex);
        return NULL;
    }

    while (block != NULL) {
        counter++;
        if (block->size - block->usage >= real_size) {
            int temp = block->size - block->usage - real_size;
            if (temp < best) {
                best = temp;
                index = counter;
            }
        }

        block = block->next;
    }

    int temp_count = 0;
    while (temp_block != NULL) {
        temp_count++;

        if (temp_count == index) {
            if (temp_block->usage == 0) {
                temp_block->alloc_id = g_allocations++;
                temp_block->usage = real_size;
                pthread_mutex_unlock(&g_alloc_mutex);
                return temp_block + 1;
            } else {
                struct mem_block *new_block = (void*) temp_block + temp_block->usage;

                new_block->alloc_id = g_allocations++;
                new_block->size = temp_block->size - temp_block->usage;
                new_block->usage = real_size;
                new_block->region_start = temp_block->region_start;
                new_block->region_size = temp_block->region_size;
                new_block->next = temp_block->next;

                temp_block->next = new_block;
                temp_block->size = temp_block->usage;
                pthread_mutex_unlock(&g_alloc_mutex);
                return new_block + 1;
            }
        }

        temp_block = temp_block->next;
    }

    pthread_mutex_unlock(&g_alloc_mutex);
    return NULL;
}

void *worst_fit(size_t size) {
    struct mem_block *block = g_head;
    struct mem_block *temp_block = g_head;

    size_t real_size = size + sizeof(struct mem_block);
    int counter = 0;
    int worst = 0;
    int index = 0;

    if (block == NULL) {
        pthread_mutex_unlock(&g_alloc_mutex);
        return NULL;
    }

    while (block != NULL) {
        counter++;
        if (block->size - block->usage >= real_size) {
            int temp = block->size - block->usage - real_size;
            if (temp > worst) {
                worst = temp;
                index = counter;
            }
        }

        block = block->next;
    }

    int temp_count = 0;
    while (temp_block != NULL) {
        temp_count++;

        if (temp_count == index) {
            if (temp_block->usage == 0) {
                temp_block->alloc_id = g_allocations++;
                temp_block->usage = real_size;
                pthread_mutex_unlock(&g_alloc_mutex);
                return temp_block + 1;
            } else {
                struct mem_block *new_block = (void*) temp_block + temp_block->usage;

                new_block->alloc_id = g_allocations++;
                new_block->size = temp_block->size - temp_block->usage;
                new_block->usage = real_size;
                new_block->region_start = temp_block->region_start;
                new_block->region_size = temp_block->region_size;
                new_block->next = temp_block->next;

                temp_block->next = new_block;
                temp_block->size = temp_block->usage;
                pthread_mutex_unlock(&g_alloc_mutex);
                return new_block + 1;
            }
        }

        temp_block = temp_block->next;
    }

    pthread_mutex_unlock(&g_alloc_mutex);
    return NULL;
}

void *reuse(size_t size) {
    // TODO: using free space management algorithms, find a block of memory that
    // we can reuse. Return NULL if no suitable block is found.
    char *algo = getenv("ALLOCATOR_ALGORITHM");
    if (algo == NULL) {
        algo = "first_fit";
    }

    if (strcmp(algo, "first_fit") == 0) {
        pthread_mutex_lock(&g_alloc_mutex);
        return first_fit(size);
    } else if (strcmp(algo, "best_fit") == 0) {
        pthread_mutex_lock(&g_alloc_mutex);
        return best_fit(size);
    } else if (strcmp(algo, "worst_fit") == 0) {
        pthread_mutex_lock(&g_alloc_mutex);
        return worst_fit(size);
    }

    return NULL;
}

void *malloc(size_t size) {
    // TODO: allocate memory. You'll first check if you can reuse an existing
    // block. If not, map a new memory region.
    if (size % 8 != 0) {
        size = size + (8 - size % 8);
    }

    bool cal = false;
    // char *algo = getenv("ALLOCATOR_SCRIBBLE");
    // if (strcmp(algo, "1") == 0) {
    //     cal = true;
    //     LOGP("YAH TRUE\n");
    // }

    void* reg_ptr = reuse(size);
    if (reg_ptr == NULL) {
        pthread_mutex_lock(&g_alloc_mutex);

        size_t real_size = size + sizeof(struct mem_block);
        int page_size = getpagesize();
        size_t num_pages = real_size / page_size;
        if (real_size % page_size != 0) {
        	num_pages++;
        }

        size_t region_sz = num_pages * page_size;
        struct mem_block *block = mmap(NULL, region_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (block == MAP_FAILED) {
            perror("mmap error");
            return NULL;
        }

        block->alloc_id = g_allocations++;
        block->size = region_sz; 
        block->usage = real_size;
        block->region_start = block;
        block->region_size = region_sz;
        block->next = NULL;
        
        if (g_head == NULL) {
        	g_head = block;
        } else {
        	struct mem_block *curr = g_head;
        	while (curr->next != NULL) {
        		curr = curr->next;
        	}
        	curr->next = block;
        }

        pthread_mutex_unlock(&g_alloc_mutex);
        
        // if (cal) {
            // calloc(0 , size);
        // } else {
            return block + 1;
        // }
    } else {
        // if (cal) {
            // calloc(0 , size);
        // } else {
            return reg_ptr;
        // }
    }
}

void free(void *ptr) {
    if (ptr == NULL) {
        return;
    }

    struct mem_block *blk = (struct mem_block*) ptr - 1;
    struct mem_block *next_region = NULL;
    int temp = blk->usage;
    blk->usage = 0;
    bool free_region = true;

    if (blk == NULL) {
        return;
    }

    struct mem_block *curr = blk->region_start;
    while (curr != NULL) {
        if (curr->alloc_id != blk->alloc_id) {
            if (curr->region_start != blk->region_start) {
                next_region = curr->region_start;
                break;
            }

            if (curr->usage != 0) {
                free_region = false;
                return;
            }
        }

        curr = curr->next;
    }

    if (free_region) {
        munmap(blk->region_start, blk->region_size);
                
        if (blk == g_head) {
            g_head = next_region;
        } else {
            struct mem_block *prev = g_head;
            while (prev->next != blk) {
                prev = prev->next;
            }
            prev->next = next_region;
        }
    }
    // TODO: free memory. If the containing region is empty (i.e., there are no
    // more blocks in use), then it should be unmapped.
}

void *calloc(size_t nmemb, size_t size) {
    // TODO: hmm, what does calloc do?
    // LOGP("IN HERE IN CALLOC\n");  
    return NULL; 
}

void *realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
        /* If the pointer is NULL, then we simply malloc a new block */
        return malloc(size);
    }

    if (size == 0) {
        /* Realloc to 0 is often the same as freeing the memory block... But the
         * C standard doesn't require this. We will free the block and return
         * NULL here. */
        free(ptr);
        return NULL;
    }

    // TODO: reallocation logic
    struct mem_block *block = (struct mem_block*) ptr - 1;
    if ((block->size - block->usage) >= size) {
        block->usage = size;
        return (block + 1);
    } else {
        return malloc(size);
    }
}