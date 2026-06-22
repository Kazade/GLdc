

FILE(GLOB GL_TESTS ${CMAKE_CURRENT_SOURCE_DIR}/test_*.h)

# The allocator tests are host-side unit tests for the standalone block
# allocator: they assert exact heap addresses and allocate multi-megabyte
# scratch pools, neither of which is meaningful (or safe) under KOS. Exclude
# them from the Dreamcast build; the allocator is still exercised on hardware
# indirectly through the texture tests.
if(PLATFORM_DREAMCAST)
    list(REMOVE_ITEM GL_TESTS ${CMAKE_CURRENT_SOURCE_DIR}/test_allocator.h)
endif()

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})

SET(TEST_GENERATOR_BIN ${CMAKE_SOURCE_DIR}/tools/test_generator.py)
SET(TEST_MAIN_FILENAME ${CMAKE_CURRENT_BINARY_DIR}/main.cpp)

ADD_CUSTOM_COMMAND(
    OUTPUT ${TEST_MAIN_FILENAME}
    COMMAND ${TEST_GENERATOR_BIN} --output ${TEST_MAIN_FILENAME} ${TEST_FILES} ${GL_TESTS}
    DEPENDS ${TEST_FILES} ${GL_TESTS} ${TEST_GENERATOR_BIN}
)

add_executable(gldc_tests ${TEST_FILES} ${TEST_SOURCES} ${TEST_MAIN_FILENAME})
target_link_libraries(gldc_tests GL)

# Directory holding the committed golden reference images.
#
# On the desktop the tests read/write them straight from the source tree. On
# the Dreamcast the goldens are read through the dcload host-filesystem mount
# (KOS mounts it at /pc), so we point at /pc/goldens and copy the references
# next to the .elf so that running the emulator with `-C .` from the build
# directory exposes them as /pc/goldens.
if(PLATFORM_DREAMCAST)
    set(GLDC_GOLDEN_DIR "/pc/goldens")

    add_custom_command(
        TARGET gldc_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/goldens
                ${CMAKE_CURRENT_BINARY_DIR}/goldens
        COMMENT "Copying golden reference images next to gldc_tests.elf"
    )
else()
    set(GLDC_GOLDEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/goldens")
endif()

target_compile_definitions(
    gldc_tests PRIVATE
    GLDC_GOLDEN_DIR="${GLDC_GOLDEN_DIR}"
)

if(NOT PLATFORM_DREAMCAST)
set_target_properties(
    gldc_tests
    PROPERTIES
    COMPILE_OPTIONS "-m32"
    LINK_OPTIONS "-m32"
)
endif()
