# Conformance test binaries
# Each test is a separate binary following the conformance protocol


add_executable(test_booleans test_booleans.cpp)
target_link_libraries(test_booleans PRIVATE hegel)
target_include_directories(
    test_booleans
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        $<BUILD_INTERFACE:${nlohmann_json_SOURCE_DIR}/include>
)

add_executable(test_integers test_integers.cpp)
target_link_libraries(test_integers PRIVATE hegel)
target_include_directories(
    test_integers
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        $<BUILD_INTERFACE:${nlohmann_json_SOURCE_DIR}/include>
)

add_executable(test_floats test_floats.cpp)
target_link_libraries(test_floats PRIVATE hegel)
target_include_directories(
    test_floats
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}
        $<BUILD_INTERFACE:${nlohmann_json_SOURCE_DIR}/include>
)

add_executable(test_text test_text.cpp)
target_link_libraries(test_text PRIVATE hegel)
target_include_directories(
    test_text
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}
        $<BUILD_INTERFACE:${nlohmann_json_SOURCE_DIR}/include>
)

add_executable(test_binary test_binary.cpp)
target_link_libraries(test_binary PRIVATE hegel)
target_include_directories(
    test_binary
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}
        $<BUILD_INTERFACE:${nlohmann_json_SOURCE_DIR}/include>
)

add_executable(test_lists test_lists.cpp)
target_link_libraries(test_lists PRIVATE hegel)
target_include_directories(
    test_lists
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}
        $<BUILD_INTERFACE:${nlohmann_json_SOURCE_DIR}/include>
)

add_executable(test_sampled_from test_sampled_from.cpp)
target_link_libraries(test_sampled_from PRIVATE hegel)
target_include_directories(
    test_sampled_from
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}
        $<BUILD_INTERFACE:${nlohmann_json_SOURCE_DIR}/include>
)

add_executable(test_hashmaps test_hashmaps.cpp)
target_link_libraries(test_hashmaps PRIVATE hegel)
target_include_directories(
    test_hashmaps
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        $<BUILD_INTERFACE:${nlohmann_json_SOURCE_DIR}/include>
)

add_executable(test_error_handling test_error_handling.cpp)
target_link_libraries(test_error_handling PRIVATE hegel)

add_executable(test_one_of test_one_of.cpp)
target_link_libraries(test_one_of PRIVATE hegel)
target_include_directories(
    test_one_of
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        $<BUILD_INTERFACE:${nlohmann_json_SOURCE_DIR}/include>
)
