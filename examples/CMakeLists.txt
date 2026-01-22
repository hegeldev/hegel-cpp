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
