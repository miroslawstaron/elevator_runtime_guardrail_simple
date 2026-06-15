#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <stdio.h>

// Forward declarations from test suites
CU_ErrorCode add_elevator_tests(void);
CU_ErrorCode add_guardrails_tests(void);

int main(void) {
    CU_initialize_registry();

    // Add test suites
    if (add_elevator_tests() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to add elevator tests\n");
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (add_guardrails_tests() != CUE_SUCCESS) {
        fprintf(stderr, "Failed to add guardrails tests\n");
        CU_cleanup_registry();
        return CU_get_error();
    }

    // Run tests
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();

    // Print summary
    printf("\n");
    printf("=====================================\n");
    printf("Test Summary:\n");
    printf("=====================================\n");
    printf("Tests run:     %u\n", CU_get_number_of_tests_run());
    printf("Tests passed:  %u\n", CU_get_number_of_tests_run() - CU_get_number_of_failures() - CU_get_number_of_asserts());
    printf("Tests failed:  %u\n", CU_get_number_of_failures());
    printf("Assertions:    %u\n", CU_get_number_of_asserts());

    int result = (CU_get_number_of_failures() == 0) ? 0 : 1;
    CU_cleanup_registry();
    return result;
}
