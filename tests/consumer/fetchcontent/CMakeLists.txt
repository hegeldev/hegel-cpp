cmake_minimum_required(VERSION 3.14)
project(hegel_consumer_fetchcontent LANGUAGES CXX)

if(NOT DEFINED HEGEL_ROOT)
    message(FATAL_ERROR "HEGEL_ROOT must be set to the hegel-cpp source directory")
endif()
if(NOT DEFINED HEGEL_REF)
    message(FATAL_ERROR "HEGEL_REF must be set to a git ref (SHA/tag/branch)")
endif()

include(FetchContent)
FetchContent_Declare(
    hegel
    GIT_REPOSITORY file://${HEGEL_ROOT}
    GIT_TAG ${HEGEL_REF}
)
FetchContent_MakeAvailable(hegel)

add_executable(consumer ${CMAKE_CURRENT_SOURCE_DIR}/../main.cpp)
target_link_libraries(consumer PRIVATE hegel)
