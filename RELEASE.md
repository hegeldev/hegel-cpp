RELEASE_TYPE: minor

This release bumps our pinned `libhegel` ([hegel-rust](hegeldev/hegel-rust)) from [0.37.5](https://github.com/hegeldev/hegel-rust/releases/tag/libhegel-v0.37.5) to [0.42.4](https://github.com/hegeldev/hegel-rust/releases/tag/libhegel-v0.42.4). hegel-rust now tags libhegel releases `libhegel-v<version>`, and the build downloads the engine from that tag.

The engine now resolves its settings from named profiles, and a `Settings` field left at its default takes the profile's value: the `default` profile comes from a `hegel.toml` in the working directory or an ancestor, the `HEGEL_DEFAULT_PROFILE` environment variable, or the detected environment (`ci` on a CI server, `workload` inside Antithesis, `development` otherwise). This changes what the defaults mean:

- `Settings::test_cases` left unset runs the profile's number of test cases, which is 100 unless a `hegel.toml` changes it.
- `Settings::suppress_health_check` left empty suppresses the profile's set: nothing locally, `TooSlow` on a CI server.
- `Backend::Auto` leaves the randomness backend to the profile (`Urandom` inside Antithesis, `Default` otherwise) instead of the engine's own detection.

Setting a field explicitly overrides the profile, as before.

`hegel::test()` now rejects a `Settings::stateful_step_count` below 1 with `std::invalid_argument` before the run starts. Previously the engine rejected it as a `std::runtime_error`. The step budget itself is unchanged: each stateful case runs at least one step and at most `stateful_step_count` of them.

Inside [Antithesis](https://antithesis.com/), the engine now reports every run's verdict as an `always` assertion, so a property test is listed and flagged alongside the assertions in the system under test. A property inside a GoogleTest test `Suite.Name` reports as `Suite::Name passes properties`; a test defined with `HEGEL_TEST` or run with an explicit `TestLocation` reports as `<file>::<name> passes properties`. Outside Antithesis nothing changes.

The new engine also changes what runs observe:

- A test case may make 2^20 choices before the engine stops it as an overrun, up from 8,192.
- A test whose verdict for the same generated data differs between two runs now fails the run with `Flaky test detected`, where it previously kept the first verdict. Discarding a case based on state outside the drawn data, such as a call counter, now trips this.
- Shrinking reaches smaller counterexamples in several situations: pairs of draws a test pins together, a draw that only fails at multiples of a round number, a `one_of` whose shorter failing alternative needs a non-trivial value, and stateful sequences with redundant steps.
- The failure database saves a new entry before removing the one it supersedes, so an interrupted shrink no longer loses a failure.
