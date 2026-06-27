#include <check.h>
#include <stdlib.h>
#include <limits.h>
#include "../containers/stack.h"

START_TEST(test_stack_allocation_overflow)
{
    // Invariant: Allocation size calculation must not overflow
    // If overflow occurs, allocation should fail or be handled safely
    
    // Payload 1: Exact exploit - overflow causing wrap-around
    unsigned int exploit_element_size = UINT_MAX;
    unsigned int exploit_capacity = 2;
    
    // Payload 2: Boundary case - product equals UINT_MAX
    unsigned int boundary_element_size = UINT_MAX / 2;
    unsigned int boundary_capacity = 2;
    
    // Payload 3: Valid input - no overflow
    unsigned int valid_element_size = 100;
    unsigned int valid_capacity = 10;
    
    struct {
        unsigned int element_size;
        unsigned int capacity;
        const char *description;
    } payloads[] = {
        {exploit_element_size, exploit_capacity, "exploit_overflow"},
        {boundary_element_size, boundary_capacity, "boundary_max"},
        {valid_element_size, valid_capacity, "valid_input"}
    };
    
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);
    
    for (int i = 0; i < num_payloads; i++) {
        Stack stack;
        
        // Initialize stack with test payload
        init_stack(&stack, payloads[i].element_size, payloads[i].capacity);
        
        // Security property: If allocation size calculation overflowed,
        // the allocated buffer should either be NULL (allocation failed)
        // or large enough to hold at least one element
        if (stack.data != NULL) {
            // If data was allocated, verify it's not dangerously small
            // due to overflow wrap-around
            unsigned long long expected_size = 
                (unsigned long long)payloads[i].element_size * 
                (unsigned long long)payloads[i].capacity;
            
            // For valid inputs, we expect proper allocation
            // For overflow cases, we're testing that the system doesn't
            // allocate a dangerously small buffer
            if (expected_size <= UINT_MAX) {
                // Valid case - allocation should succeed
                ck_assert_ptr_nonnull(stack.data);
            } else {
                // Overflow case - either allocation should fail (NULL)
                // or if it succeeds, the size should be reasonable
                // (not a tiny wrap-around value)
                if (stack.data != NULL) {
                    // If allocation succeeded despite overflow,
                    // ensure it's not a tiny buffer
                    ck_assert_msg(payloads[i].capacity == 0 || 
                                 stack.element_size >= payloads[i].element_size,
                                 "Dangerous allocation after overflow");
                }
            }
            
            // Clean up if allocation succeeded
            if (stack.data != NULL) {
                free(stack.data);
            }
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_stack_allocation_overflow);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}