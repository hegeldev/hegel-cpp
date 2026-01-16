# Examples
#
# Example programs demonstrating property-based testing with hegel.

add_executable(example_usage example_usage.cpp)
target_link_libraries(example_usage PRIVATE hegel)

# Systems Under Test (SUTs) - data structures with property tests
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
