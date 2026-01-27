from pathlib import Path

from hegel.conformance import (
    BinaryConformance,
    BooleanConformance,
    FloatConformance,
    IntegerConformance,
    ListConformance,
    SampledFromConformance,
    TextConformance,
    run_conformance_tests,
)

BUILD_DIR = Path(__file__).parent.parent.parent / "build" / "tests" / "conformance" / "cpp"

INT32_MIN = -(2**31)
INT32_MAX = 2**31 - 1


def test_conformance(subtests):
    run_conformance_tests(
        [
            BooleanConformance(BUILD_DIR / "test_booleans"),
            IntegerConformance(
                BUILD_DIR / "test_integers", min_value=INT32_MIN, max_value=INT32_MAX
            ),
            FloatConformance(BUILD_DIR / "test_floats"),
            TextConformance(BUILD_DIR / "test_text"),
            BinaryConformance(BUILD_DIR / "test_binary"),
            ListConformance(
                BUILD_DIR / "test_lists", min_value=INT32_MIN, max_value=INT32_MAX
            ),
            SampledFromConformance(BUILD_DIR / "test_sampled_from"),
        ],
        subtests,
    )
