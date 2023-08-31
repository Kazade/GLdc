#include <stdint.h>

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

    /* It's frustrating that we need to do this dynamically
     * but we need to know the size allocated when we free() */
    struct AllocEntry* allocations;
} PoolHeader;


static PoolHeader pool_header = {
    {0}, NULL, 0, NULL, 0, NULL
};

void* alloc_base_address(void* pool) {
    return pool_header.base_address;
}

size_t alloc_block_count(void* pool) {
    return pool_header.block_count;
}

void* alloc_next_available(void* pool, size_t required_size) {
    uint8_t* it = pool_header.block_usage;
    uint32_t required_subblocks = (required_size / 256);
    if(required_size % 256) required_subblocks += 1;

    while(true) {
        // Skip full blocks
        while((*it) == 255) {
            ++it;
            if(it >= pool_header.block_usage + sizeof(pool_header.block_usage)) {
                return NULL;
            }
            continue;
        }

        uint32_t found_subblocks = 0;
        bool requires_alignment = required_size >= 2048;

        /* We just need to find enough consecutive blocks */
        while(found_subblocks < required_subblocks) {
            uint8_t t = *it;

            /* Optimisation only. Skip over full blocks */
            if(t == 255) {
                ++it;
                found_subblocks = 0;
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
                            return pool_header.base_address + (offset * 256);
                        }
                    }
                } else {
                    found_subblocks = 0;
                }

                t <<= 1;
            }

            ++it;
            if(it >= pool_header.block_usage + sizeof(pool_header.block_usage)) {
                return NULL;
            }
        }

    }

}

uint32_t alloc_block_from_address(void* p) {

}

int alloc_init(void* pool, size_t size) {
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
    pool_header.base_address = round_up(pool_header.pool, 2048);
    pool_header.block_count = ((p + size) - pool_header.base_address) / 2048;

    return 0;
}

void alloc_shutdown(void* pool) {
    pool_header.pool = NULL;
}

void* alloc_malloc(void* pool, size_t size) {
    void* ret = alloc_next_available(pool, size);
    if(ret) {
        uintptr_t start_subblock = (uint8_t*) ret - pool_header.base_address;

        uint32_t required_subblocks = (size / 256);
        if(size % 256) required_subblocks += 1;

        uintptr_t start_block = start_subblock / 8;
        uintptr_t filled_blocks = required_subblocks / 8;
        uintptr_t trailing_subblocks = required_subblocks % 8;

        for(size_t i = start_block; i < start_block + filled_blocks; ++i) {
            pool_header.block_usage[i] = 255;
        }

        uint8_t* trailing = &pool_header.block_usage[start_block + filled_blocks];
        uint8_t mask = 0;
        for(size_t i = 0; i < trailing_subblocks; ++i) {
            mask |= 1;
            mask <<= 1;
        }

        mask <<= 8 - trailing_subblocks - 1;
        *trailing |= mask;
    }

    return ret;
}

void alloc_free(void* pool, void* p) {

}

void alloc_defrag_start(void* pool) {

}

void* alloc_defrag_address(void* pool, void* p) {

}

void alloc_defrag_commit(void* pool) {

}

bool alloc_defrag_in_progress(void* pool) {

}
