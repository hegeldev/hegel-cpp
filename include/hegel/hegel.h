#pragma once

/**
 * @file hegel.h
 * @brief Hegel C++ SDK - Hypothesis-like property-based testing for C++20
 *
 * This library provides type-safe random data generation for property-based
 * testing, communicating with the Hegel test runner via Unix sockets.
 *
 * @section usage Basic Usage
 *
 * @code{.cpp}
 * #include "hegel/hegel.h"
 *
 * // Type-based generation (schema derived via reflect-cpp)
 * auto gen = hegel::default_generator<Person>();
 * Person p = gen.generate();
 *
 * // Strategy-based generation
 * using namespace hegel::strategies;
 * auto int_gen = integers<int>({.min_value = 0, .max_value = 100});
 * int value = int_gen.generate();
 * @endcode
 *
 * @section env Environment Variables
 * - `HEGEL_DEBUG`: If set, prints REQUEST/RESPONSE JSON to stderr
 *
 * @section deps Dependencies
 * - reflect-cpp (https://github.com/getml/reflect-cpp) - C++20 reflection
 * - nlohmann/json (https://github.com/nlohmann/json) - JSON manipulation
 * - POSIX sockets (sys/socket.h, sys/un.h, unistd.h)
 */

// HegelOptions and supporting classes
#include "options.h"

// All strategy functions
#include "strategies.h"

// Embedded mode (hegel function)
#include "embedded.h"
