#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "alloc.h"


/* This allocator is designed so that all allocations larger
 * than 2k, fall on a 2k boundary. Smaller allocations will
 * never cross a 2k boundary.
 *
 * House keeping is stored in RAM to avoid reading back from the
 * VRAM to check for usage. Headers can't be easily stored in the
 * blocks anyway as they have to be 2k aligned (so you'd need to
 * store them in reverse or something)
 *
 * Defragmenting the pool will move allocations less than 2k
 * first, and then shift any full 2k blocks to the start of the
 * address space.
 *
 * The maximum pool size is 8M, made up of:
 *
 * - 4096 blocks of 2k
 * - each with 8 sub-blocks of 256 bytes
 *
 * Why?
 *
 * The PVR performs better if textures don't cross 2K memory
 * addresses, so we try to avoid that. Obviously we can't
 * if the allocation is > 2k, but in that case we can at least
 * align with 2k and the VQ codebook (which is usually 2k) will
 * be in its own page.
 *
 * The smallest PVR texture allowed is 8x8 at 16 bit (so 128 bytes)
 * but we're unlikely to use too many of those, so having a min sub-block
 * size of 256 should be OK (a 16x16 image is 512, so two sub-blocks).
 *
 * We could go down to 128 bytes if wastage is an issue, but then we have
 * to store double the number of usage markers.
 *
 * FIXME:
 *
 *  - Allocations < 2048 can still cross boundaries
 *  - Only operates on one pool (ignores what you pass)
 */

#include <assert.h>

#define EIGHT_MEG (8 * 1024 * 1024)
#define TWO_KILOBYTES (2 * 1024)
#define BLOCK_COUNT (EIGHT_MEG / TWO_KILOBYTES)

static inline int round_up(int n, int multiple)
{
    assert(multiple);
    return ((n + multiple - 1) / multiple) * multiple;
}

struct AllocEntry {
    void* pointer;
    size_t size;
    struct AllocEntry* next;
};


typedef struct {
    /* This is a usage bitmask for each block. A block
     * is divided into 8 x 256 byte subblocks. If a block
     * is entirely used, it's value will be 255, if
     * it's entirely free then it will be 0.
     */
    uint8_t block_usage[BLOCK_COUNT];
    uint8_t* pool;  // Pointer to the memory pool
    size_t pool_size; // Size of the memory pool
    uint8_t* base_address; // First 2k aligned address in the pool
    size_t block_count;  // Number of 2k blocks in the pool
    bool defrag_in_progress;

    /* It's frustrating that we need to do this dynamically
     * but we need to know the size allocated when we free()...
     * we could store it statically but it would take 64k if we had
     * an array of block_index -> block size where there would be 2 ** 32
     * entries of 16 bit block sizes. The drawback (aside the memory usage)
     * would be that we won't be able to order by size, so defragging will
     * take much more time.*/
    struct AllocEntry* allocations;
} PoolHeader;


static PoolHeader pool_header = {
    {0}, NULL, 0, NULL, 0, false, NULL
};

void* alloc_base_address(void* pool) {
    (void) pool;
    return pool_header.base_address;
}

size_t alloc_block_count(void* pool) {
    (void) pool;
    return pool_header.block_count;
}

void* alloc_next_available_ex(void* pool, size_t required_size, size_t* start_subblock, size_t* required_subblocks);

void* alloc_next_available(void* pool, size_t required_size) {
    return alloc_next_available_ex(pool, required_size, NULL, NULL);
}

void* alloc_next_available_ex(void* pool, size_t required_size, size_t* start_subblock_out, size_t* required_subblocks_out) {
    (void) pool;

    uint8_t* it = pool_header.block_usage;
    uint32_t required_subblocks = (required_size / 256);
    if(required_size % 256) required_subblocks += 1;

    uint8_t* end = pool_header.block_usage + pool_header.block_count;

    if(required_subblocks_out) {
        *required_subblocks_out = required_subblocks;
    }

    while(it < end) {
        // Skip full blocks
        while((*it) == 255) {
            ++it;
            if(it >= pool_header.block_usage + sizeof(pool_header.block_usage)) {
                return NULL;
            }
            continue;
        }

        uint32_t found_subblocks = 0;

        /* Anything gte to 2048 must be aligned to a 2048 boundary */
        bool requires_alignment = required_size >= 2048;

        /* We just need to find enough consecutive blocks */
        while(found_subblocks < required_subblocks) {
            uint8_t t = *it;

            /* Optimisation only. Skip over full blocks */
            if(t == 255) {
                ++it;
                found_subblocks = 0;

                if(it >= end) {
                    return NULL;
                }

                continue;
            }

            /* Now let's see how many consecutive blocks we can find */
            for(int i = 0; i < 8; ++i) {
                if((t & 0x80) == 0) {
                    if(requires_alignment && found_subblocks == 0 && i != 0) {
                        // Ignore this subblock, because we want the first subblock to be aligned
                        // at a 2048 boundary and this one isn't (i != 0)
                        found_subblocks = 0;
                    } else {
                        found_subblocks++;
                        if(found_subblocks >= required_subblocks) {
                            /* We found space! Now calculate the address */
                            uintptr_t offset = (it - pool_header.block_usage) * 8;
                            offset += (i + 1);
                            offset -= required_subblocks;

                            if(start_subblock_out) {
                                *start_subblock_out = offset;
                            }

                            return pool_header.base_address + (offset * 256);
                        }
                    }
                } else {
                    found_subblocks = 0;
                }

                t <<= 1;
            }

            ++it;
            if(it >= end) {
                return NULL;
            }
        }

    }

    return NULL;
}

int alloc_init(void* pool, size_t size) {
    (void) pool;

    if(pool_header.pool) {
        return -1;
    }

    if(size > EIGHT_MEG) {  // FIXME: >= ?
        return -1;
    }

    uint8_t* p = (uint8_t*) pool;

    memset(pool_header.block_usage, 0, sizeof(pool_header.block_usage));
    pool_header.pool = pool;
    pool_header.pool_size = size;
    pool_header.base_address = (uint8_t*) round_up((uintptr_t) pool_header.pool, 2048);
    pool_header.block_count = ((p + size) - pool_header.base_address) / 2048;
    pool_header.allocations = NULL;

    assert(((uintptr_t) pool_header.base_address) % 2048 == 0);

    return 0;
}

void alloc_shutdown(void* pool) {
    (void) pool;

    struct AllocEntry* it = pool_header.allocations;
    while(it) {
        struct AllocEntry* next = it->next;
        free(it);
        it = next;
    }

    memset(&pool_header, 0, sizeof(pool_header));
}

static inline uint32_t size_to_subblock_count(size_t size) {
    uint32_t required_subblocks = (size / 256);
    if(size % 256) required_subblocks += 1;
    return required_subblocks;
}

static inline uint32_t subblock_from_pointer(void* p) {
    uint8_t* ptr = (uint8_t*) p;
    return (ptr - pool_header.base_address) / 256;
}

void* alloc_malloc(void* pool, size_t size) {
    size_t start_subblock, required_subblocks;
    void* ret = alloc_next_available_ex(pool, size, &start_subblock, &required_subblocks);

    if(size >= 2048) {
        assert(((uintptr_t) ret) % 2048 == 0);
    }

    if(ret) {
        size_t offset = start_subblock % 8;
        size_t block = start_subblock / 8;
        uint8_t mask = 0;

        /* Toggle any bits for the first block */
        for(int i = offset - 1; i >= 0; --i) {
            mask |= (1 << i);
            required_subblocks--;
        }

        if(mask) {
            pool_header.block_usage[block++] |= mask;
        }

        /* Fill any full blocks in the middle of the allocation */
        while(required_subblocks > 8) {
            pool_header.block_usage[block++] = 255;
            required_subblocks -= 8;
        }

        /* Fill out any trailing subblocks */
        mask = 0;
        for(size_t i = 0; i < required_subblocks; ++i) {
            mask |= (1 << (7 - i));
        }

        if(mask) {
            pool_header.block_usage[block++] |= mask;
        }


        /* Insert allocations in the list by size descending so that when we
         * defrag we can move the larger blocks before the smaller ones without
         * much effort */
        struct AllocEntry* new_entry = (struct AllocEntry*) malloc(sizeof(struct AllocEntry));
        new_entry->pointer = ret;
        new_entry->size = size;
        new_entry->next = NULL;

        struct AllocEntry* it = pool_header.allocations;
        struct AllocEntry* last = NULL;

        if(!it) {
            pool_header.allocations = new_entry;
        } else {
            while(it) {
                if(it->size < size) {
                    if(last) {
                        last->next = new_entry;
                    } else {
                        pool_header.allocations = new_entry;
                    }

                    new_entry->next = it;
                    break;
                } else if(!it->next) {
                    it->next = new_entry;
                    new_entry->next = NULL;
                    break;
                }

                last = it;
                it = it->next;
            }
        }
    }

    return ret;
}

void alloc_free(void* pool, void* p) {
    (void) pool;

    struct AllocEntry* it = pool_header.allocations;
    struct AllocEntry* last = NULL;
    while(it) {
        if(it->pointer == p) {
            size_t used_subblocks = size_to_subblock_count(it->size);
            size_t subblock = subblock_from_pointer(p);
            size_t block = subblock / 8;
            size_t offset = subblock % 8;
            uint8_t mask = 0;

            /* Wipe out any leading subblocks */
            for(int i = offset; i > 0; --i) {
                mask |= (1 << i);
                used_subblocks--;
            }

            if(mask) {
                pool_header.block_usage[block++] &= ~mask;
            }

            /* Clear any full blocks in the middle of the allocation */
            while(used_subblocks > 8) {
                pool_header.block_usage[block++] = 0;
                used_subblocks -= 8;
            }

            /* Wipe out any trailing subblocks */
            mask = 0;
            for(size_t i = 0; i < used_subblocks; ++i) {
                mask |= (1 << (7 - i));
            }

            if(mask) {
                pool_header.block_usage[block++] &= ~mask;
            }

            if(last) {
                last->next = it->next;
            } else {
                assert(it == pool_header.allocations);
                pool_header.allocations = it->next;
            }

            free(it);
            break;
        }

        last = it;
        it = it->next;
    }
}

void alloc_defrag_start(void* pool) {
    (void) pool;
    pool_header.defrag_in_progress = true;
}

void* alloc_defrag_address(void* pool, void* p) {
    (void) pool;
    return p;
}

void alloc_defrag_commit(void* pool) {
    (void) pool;
    pool_header.defrag_in_progress = false;
}

bool alloc_defrag_in_progress(void* pool) {
    (void) pool;
    return pool_header.defrag_in_progress;
}

static inline uint8_t count_ones(uint8_t byte) {
    static const uint8_t NIBBLE_LOOKUP [16] = {
        0, 1, 1, 2, 1, 2, 2, 3,
        1, 2, 2, 3, 2, 3, 3, 4
    };
    return NIBBLE_LOOKUP[byte & 0x0F] + NIBBLE_LOOKUP[byte >> 4];
}

size_t alloc_count_free(void* pool) {
    uint8_t* it = pool_header.block_usage;
    uint8_t* end = it + pool_header.block_count;

    size_t total_free = 0;

    while(it < end) {
        total_free += count_ones(*it) * 256;
        ++it;
    }

    return total_free;
}

size_t alloc_count_continuous(void* pool) {
    (void) pool;

    size_t largest_block = 0;

    uint8_t* it = pool_header.block_usage;
    uint8_t* end = it + pool_header.block_count;

    size_t current_block = 0;
    while(it < end) {
        uint8_t t = *it++;
        if(!t) {
            current_block += 2048;
        } else {
            for(int i = 7; i >= 0; --i) {
                bool bitset = (t & (1 << i));
                if(bitset) {
                    current_block += (7 - i) * 256;
                    if(largest_block < current_block) {
                        largest_block = current_block;
                        current_block = 0;
                    }
                }
            }
        }
    }

    return largest_block;
}
