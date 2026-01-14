# Systems Under Test (SUTs)
#
# These are example programs that demonstrate property-based testing with hegel.
# Each test binary can be run with hegel to find bugs in the corresponding
# data structure implementation.

# List of SUTs
set(SUTS
    fraction
    bounded_queue
    lru_cache
    run_length
    sorted_vec
)

foreach(SUT ${SUTS})
    add_executable(test_${SUT} test_${SUT}.cpp)
    target_link_libraries(test_${SUT} PRIVATE hegel)
    target_include_directories(test_${SUT} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()
