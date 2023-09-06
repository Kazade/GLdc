#include "tools/test.h"

#include <stdint.h>
#include <assert.h>

#include <GL/gl.h>
#include <GL/glkos.h>

#include "GL/alloc/alloc.h"

static inline int round_up(int n, int multiple)
{
    assert(multiple);
    return ((n + multiple - 1) / multiple) * multiple;
}

class AllocatorTests : public test::TestCase {
public:
    uint8_t __attribute__((aligned(2048))) pool[16 * 2048];

    void set_up() {
    }

    void tear_down() {
        alloc_shutdown(pool);
    }
    
    void test_alloc_init() {
        alloc_init(pool, sizeof(pool));

        void* expected_base_address = (void*) round_up((uintptr_t) pool, 2048);
        assert_equal(alloc_next_available(pool, 16), expected_base_address);
        assert_equal(alloc_base_address(pool), expected_base_address);

        size_t expected_blocks = (
            uintptr_t(pool + sizeof(pool)) -
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

    void test_alloc_malloc() {
        alloc_init(pool, sizeof(pool));

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
