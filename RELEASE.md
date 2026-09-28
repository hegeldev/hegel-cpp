RELEASE_TYPE: patch

This patch fixes stateful tests continuing after a rule fails a GoogleTest `ASSERT_*` ([#138](https://github.com/hegeldev/hegel-cpp/issues/138)). A `ASSERT_*` failure now ends the test case.
