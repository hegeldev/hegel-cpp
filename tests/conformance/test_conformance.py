from pathlib import Path

from hegel.conformance import run_conformance_tests

BUILD_DIR = (
    Path(__file__).parent.parent.parent / "build" / "tests" / "conformance" / "cpp"
)


def test_conformance():
    """Run all conformance tests against the hegel-cpp SDK."""
    binaries = {
        "booleans": BUILD_DIR / "test_booleans",
        "integers": BUILD_DIR / "test_integers",
        "floats": BUILD_DIR / "test_floats",
        # TODO: text test is disabled due to a bug in hypothesis-jsonschema where
        # minLength is not properly respected for Unicode strings with surrogate pairs.
        # Python len() counts UTF-16 code units while JSON Schema minLength counts
        # codepoints, causing ~10-15% of test cases to fail validation.
        # "text": BUILD_DIR / "test_text",
        "lists": BUILD_DIR / "test_lists",
        "sampled_from": BUILD_DIR / "test_sampled_from",
    }

    assert all(path.exists() for path in binaries.values())
    run_conformance_tests(binaries)
