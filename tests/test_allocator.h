#include "tools/test.h"

#include <cstdint>
#include <cassert>
#include <malloc.h>
#include <utility>

#include <GL/gl.h>
#include <GL/glkos.h>

#include "GL/alloc/alloc.h"

static inline int round_up(int n, int multiple)
{
    assert(multiple);
    return ((n + multiple - 1) / multiple) * multiple;
}

#define POOL_SIZE (16 * 2048)

class AllocatorTests : public test::TestCase {
public:
    uint8_t* pool = NULL;

    std::vector<std::pair<void*, void*>> defrag_moves;

    void set_up() {
        pool = (uint8_t*) memalign(2048, POOL_SIZE);
        assert(((intptr_t) pool) % 2048 == 0);
    }

    void tear_down() {
        alloc_shutdown(pool);
        free(pool);
    }

    static void on_defrag(void* src, void* dst, void* user_data) {
        AllocatorTests* self = (AllocatorTests*) user_data;
        self->defrag_moves.push_back(std::make_pair(src, dst));
    }

    void test_defrag() {
        alloc_init(pool, POOL_SIZE);

        alloc_malloc(pool, 256);
        void* a2 = alloc_malloc(pool, 256);
        void* a3 = alloc_malloc(pool, 256);

        alloc_free(pool, a2);

        alloc_run_defrag(pool, &AllocatorTests::on_defrag, 5, this);

        assert_equal(defrag_moves.size(), 1u); // Moved a3 -> a2

        assert_equal(defrag_moves[0].first, a3);
        assert_equal(defrag_moves[0].second, a2);

        assert_equal(alloc_malloc(pool, 256), a3);
    }

    void test_poor_alloc_aligned() {
        /* If we try to allocate and there are no suitable aligned
         * slots available, we fallback to any available unaligned slots */
        alloc_init(pool, POOL_SIZE);

        // Leave only space for an unaligned block
        alloc_malloc(pool, (15 * 2048) - 256);

        // Should work, we have space (just) but it's not aligned
        void* a1 = alloc_malloc(pool, 2048 + 256);
        assert_is_not_null(a1);
        assert_equal(a1, pool + ((15 * 2048) - 256));
    }

    void test_poor_alloc_straddling() {
        /*
         * If we try to allocate a small block, it should not
         * cross a 2048 boundary unless there is no other option */
        alloc_init(pool, POOL_SIZE);
        alloc_malloc(pool, (15 * 2048) - 256);
        void* a1 = alloc_malloc(pool, 512);
        assert_true((uintptr_t(a1) % 2048) == 0); // Should've aligned to the last 2048 block

        /* Allocate the rest of the last block, this leaves a 256 block in the
         * penultimate block */
        alloc_malloc(pool, 1536);
        alloc_free(pool, a1);

        /* No choice but to straddle the boundary */
        a1 = alloc_malloc(pool, 768);
    }

    void test_alloc_init() {
        alloc_init(pool, POOL_SIZE);

        void* expected_base_address = (void*) round_up((uintptr_t) pool, 2048);
        assert_equal(alloc_next_available(pool, 16), expected_base_address);
        assert_equal(alloc_base_address(pool), expected_base_address);

        size_t expected_blocks = (
            uintptr_t(pool + POOL_SIZE) -
            uintptr_t(expected_base_address)
        ) / 2048;

        assert_equal(alloc_block_count(pool), expected_blocks);
    }

    void test_complex_case() {
        uint8_t* large_pool = (uint8_t*) malloc(8 * 1024 * 1024);

        alloc_init(large_pool, 8 * 1024 * 1024);
        alloc_malloc(large_pool, 262144);
        alloc_malloc(large_pool, 262144);
        void* a1 = alloc_malloc(large_pool, 524288);
        alloc_free(large_pool, a1);
        alloc_malloc(large_pool, 699056);
        alloc_malloc(large_pool, 128);
        alloc_shutdown(large_pool);

        free(large_pool);
    }

    void test_complex_case2() {
        uint8_t* large_pool = (uint8_t*) malloc(8 * 1024 * 1024);
        alloc_init(large_pool, 8 * 1024 * 1024);

        void* a1 = alloc_malloc(large_pool, 131072);
        alloc_free(large_pool, a1);

        alloc_malloc(large_pool, 174768);
        void* a2 = alloc_malloc(large_pool, 131072);
        alloc_free(large_pool, a2);

        alloc_malloc(large_pool, 174768);
        void* a3 = alloc_malloc(large_pool, 128);

        alloc_free(large_pool, a3);

        alloc_shutdown(large_pool);
        free(large_pool);
    }

    void test_alloc_malloc() {
        alloc_init(pool, POOL_SIZE);

        uint8_t* base_address = (uint8_t*) alloc_base_address(pool);
        void* a1 = alloc_malloc(pool, 1024);

        /* First alloc should always be the base address */
        assert_equal(a1, base_address);

        /* An allocation of <= 2048 (well 1024) will not necessarily be at
         * a 2k boundary */
        void* expected_next_available = base_address + uintptr_t(1024);
        assert_equal(alloc_next_available(pool, 1024), expected_next_available);

        /* Requesting 2k though will force to a 2k boundary */
        expected_next_available = base_address + uintptr_t(2048);
        assert_equal(alloc_next_available(pool, 2048), expected_next_available);

        /* Now alloc 2048 bytes, this should be on the 2k boundary */
        void* a2 = alloc_malloc(pool, 2048);
        assert_equal(a2, expected_next_available);

        /* If we try to allocate 1k, this should go in the second half of the
         * first block */
        expected_next_available = base_address + uintptr_t(1024);
        void* a3 = alloc_malloc(pool, 1024);
        assert_equal(a3, expected_next_available);

        alloc_free(pool, a1);

        /* Next allocation would go in the just freed block */
        expected_next_available = base_address;
        assert_equal(alloc_next_available(pool, 64), expected_next_available);

        /* Now allocate 14 more 2048 size blocks, the following one should
         * return NULL */
        for(int i = 0; i < 14; ++i) {
            alloc_malloc(pool, 2048);
        }

        assert_is_null(alloc_malloc(pool, 2048));

        /* But we should still have room in the second block for this */
        assert_is_not_null(alloc_malloc(pool, 64));
    }

};
