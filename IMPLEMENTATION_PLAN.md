# Hegel C++ Recovery Plan

Based on audit conducted 2026-04-15.

## Priority 1: Conformance Tests Passing -- DONE

- [x] Fix test_lists.cpp to output `elements` array and handle `mode` param
- [x] Fix test_hashmaps.cpp to handle `mode` param
- [x] Implement error handling conformance binary
- [x] Remove skip_tests from test_conformance.py
- [x] Implement OneOf conformance binary (all 3 modes)
- [x] Fix function-backed vectors unique enforcement
- [x] Fix function-backed dictionaries unbounded loop
- [x] Fix noexcept on json_raw_ref getters
- [x] Fix defensive JSON parsing in hegel.cpp event loop
- [x] Fix defensive error handling for mark_complete response
- [x] Fix defensive error type parsing in communicate_with_core

## Priority 2: Coverage Enforcement -- DONE

- [x] Add `just coverage` target (gcovr, --fail-under-line 100)
- [x] Coverage at 100% line + 100% function (up from 70.9%)

## Priority 3: GTest Integration & Counterexample Display -- DONE

- [x] Create HEGEL_TEST macro with throw_on_failure mode
- [x] Counterexample display shows "Generated: <value>" during final replay
- [x] Verified gtest_failure_test shows shrunk counterexample (51 for >50 test)

## Priority 4: Test Utilities -- DONE

- [x] assert_all_examples
- [x] assert_no_examples
- [x] find_any
- [x] minimal

## Priority 5: Coverage Tests -- DONE

- [x] Connection unit tests (handshake variants, close_stream, recv_request with CLOSE_PAYLOAD)
- [x] communicate_with_core tests (debug logging, generic server error)
- [x] Protocol error tests (payload too large, missing terminator, HEGEL_PROTOCOL_DEBUG env var)
- [x] JSON wrapper tests (push_back const ref, get_binary)
- [x] note() with is_last_run=true test
- [x] Seed option test
- [x] GCOVR_EXCL_LINE markers for genuine gcov artifacts (closing braces after return)

## Priority 6: Protocol Unit Tests -- DONE

- [x] CRC32 unit tests with known test vectors
- [x] Packet serialization/deserialization round-trip tests (socket pairs)
- [x] CBOR encode/decode tests with tagged strings
- [x] Protocol error path tests (closed fd, corrupted magic, CRC mismatch)
- [x] JSON wrapper tests (construction, access, type checking, iteration)

## Priority 7: Generator Completeness -- DONE

- [x] Implement one_of Path 2 (tuple schema for mixed basic/transformed)
- [x] Make map() preserve schema on basic generators
- [x] Add start_span/stop_span protocol commands for filter()
- [x] Fix filter() to use spans with 3 retries (matching Rust/Go)
- [x] Add is_test_aborted() for OVERRUN vs filter exhaustion distinction

## Priority 8: DataSource Abstraction

- [ ] Create DataSource interface (abstract class) with generate/start_span/stop_span/mark_complete
- [ ] Make communicate_with_core and span functions go through DataSource
- [ ] Connection-backed DataSource as concrete implementation
- [ ] Use DataSource in TestCaseData instead of raw Connection*

## Priority 9: Documentation Fixes -- DONE

- [x] Remove reference to non-existent `examples/` directory
