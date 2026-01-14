"""Conformance tests for hegel-cpp SDK."""

from pathlib import Path

from hegel.conformance import run_conformance_tests

# Path to the conformance binaries (relative to this file)
BUILD_DIR = Path(__file__).parent.parent.parent / "build" / "tests" / "conformance"


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

    # Check that all binaries exist
    missing = [name for name, path in binaries.items() if not path.exists()]
    if missing:
        raise FileNotFoundError(
            f"Conformance binaries not found: {missing}. "
            f"Please build the project first with: cmake --build build"
        )

    run_conformance_tests(binaries)
