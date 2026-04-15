from pathlib import Path

from hegel.conformance import (
    BinaryConformance,
    BooleanConformance,
    DictConformance,
    EmptyTestConformance,
    ErrorResponseConformance,
    FloatConformance,
    IntegerConformance,
    ListConformance,
    OneOfConformance,
    SampledFromConformance,
    StopTestOnCollectionMoreConformance,
    StopTestOnGenerateConformance,
    StopTestOnMarkCompleteConformance,
    StopTestOnNewCollectionConformance,
    TextConformance,
    run_conformance_tests,
)

BUILD_DIR = Path(__file__).parent.parent.parent / "build" / "tests" / "conformance" / "cpp"

INT32_MIN = -(2**31)
INT32_MAX = 2**31 - 1


def test_conformance(subtests):
    error_handling_binary = BUILD_DIR / "test_error_handling"

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
            DictConformance(
                BUILD_DIR / "test_hashmaps",
                min_key=INT32_MIN,
                max_key=INT32_MAX,
                min_value=INT32_MIN,
                max_value=INT32_MAX,
            ),
            OneOfConformance(BUILD_DIR / "test_one_of"),
            StopTestOnGenerateConformance(error_handling_binary),
            StopTestOnMarkCompleteConformance(error_handling_binary),
            ErrorResponseConformance(error_handling_binary),
            EmptyTestConformance(error_handling_binary),
            StopTestOnCollectionMoreConformance(error_handling_binary),
            StopTestOnNewCollectionConformance(error_handling_binary),
        ],
        subtests,
    )
