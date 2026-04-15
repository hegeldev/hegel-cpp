# Hegel C++ Recovery Plan

Based on audit conducted 2026-04-15.

## Priority 1: Conformance Tests Passing

- [x] Fix test_lists.cpp to output `elements` array and handle `mode` param (basic vs non_basic)
- [x] Fix test_hashmaps.cpp to handle `mode` param (basic vs non_basic)
- [x] Implement error handling conformance binary (handles HEGEL_PROTOCOL_TEST_MODE env var)
- [x] Remove skip_tests from test_conformance.py
- [x] Implement OneOf conformance binary (all 3 modes: basic, transformed, non_basic)
- [x] Fix function-backed vectors unique enforcement (if constexpr guard)
- [x] Fix function-backed dictionaries unbounded loop (stop_test after 3 consecutive dupes)
- [x] Fix noexcept on json_raw_ref getters (was causing terminate on type mismatch)
- [x] Fix defensive JSON parsing in hegel.cpp event loop (type checks before extraction)
- [x] Fix defensive error handling for mark_complete response (catch StopTest)
- [x] Fix defensive error type parsing in communicate_with_core

## Priority 2: Coverage Enforcement

- [ ] Add `just coverage` target that runs tests with gcov/llvm-cov, measures library code coverage, and exits non-zero if below 100%

## Priority 3: GTest Integration & Counterexample Display

- [ ] Create HEGEL_TEST macro/fixture that wraps hegel::hegel() in a GTest-compatible way, catching GTest fatal failures (which use `return`, not `throw`) and converting them to exceptions so hegel can detect failures and shrink
- [ ] Fix counterexample display: on final replay, show labeled drawn values (generator type + value), the assertion error, and any note() messages in a structured format
- [ ] Verify gtest_failure_test actually shows shrunk counterexample output

## Priority 4: Test Utilities

- [ ] Implement `assert_all_examples(gen, predicate, options)` -- runs hegel and asserts predicate holds for all generated values
- [ ] Implement `find_any(gen, predicate, options)` -- returns a value satisfying predicate, or fails
- [ ] Implement `minimal(gen, predicate, options)` -- returns the minimal shrunk value satisfying predicate
- [ ] Implement `assert_no_examples(gen, predicate, options)` -- asserts no generated value satisfies predicate

## Priority 5: DataSource Abstraction

- [ ] Extract DataSource interface from TestCaseData (trait/interface for generate, start_span, stop_span, mark_complete)
- [ ] Implement LiveDataSource (wraps Connection, current behavior)
- [ ] Implement FakeDataSource (deterministic, no server, supports error injection)
- [ ] Migrate TestCaseData to use DataSource instead of raw Connection*

## Priority 6: Protocol Unit Tests

- [ ] Add CRC32 unit tests (known test vectors)
- [ ] Add packet serialization/deserialization round-trip tests (using socket pairs)
- [ ] Add handshake unit tests
- [ ] Add stream multiplexing unit tests
- [ ] Add CBOR encode/decode unit tests with tagged strings

## Priority 7: Generator Completeness

- [ ] Implement one_of Path 2: when all generators are basic but some have transforms (map), emit tuple schema for index + value with client-side transform
- [ ] Make map() preserve schema on basic generators (carry schema through FunctionBackedGenerator)
- [ ] Add explicit tests for all 3 one_of paths

## Priority 8: Documentation Fixes

- [x] Remove reference to non-existent `examples/` directory in getting-started.md
- [ ] Verify all code examples in docs actually compile and run
