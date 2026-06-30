#!/usr/bin/env python3
"""Coverage gate + exclusion ratchet.

Checks over the library sources (src/, include/hegel/):

1. Coverage gate: every line that is NOT excluded from coverage must be
   covered. Lines that carry no testable behaviour are tolerated without a
   marker — purely structural lines (lone braces / punctuation and 
   unreachability statements (std::unreachable, __builtin_unreachable, 
   assert(false), abort()).

2. Stale-exclusion check: an inline `// GCOVR_EXCL_LINE` on a line that is in
   fact covered is a failure — the marker should be removed (and the ratchet
   lowered). Block exclusions (`// GCOVR_EXCL_START` .. `STOP`) are exempt,
   since they intentionally cover a whole boundary region.

3. Exclusion ratchet: the number of lines hidden from coverage via the markers
   must not EXCEED `.github/coverage-ratchet.json`. Only a human may raise it
   (admitting more excluded code); if the count drops the ratchet auto-tightens
   to the new lower number. The count comes from the source markers, so it is
   identical regardless of compiler/gcov.

Coverage is measured per source line: templates and inlined functions emit one
record per instantiation, so a line is covered if ANY instantiation executed
it (max count over records).

Input is an LCOV `.info` file produced by `llvm-cov export -format=lcov`.

Usage: check-coverage.py <lcov-info>
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

RATCHET_FILE = Path(".github/coverage-ratchet.json")
SOURCE_DIRS = [Path("src"), Path("include/hegel")]
SOURCE_GLOBS = ("*.cpp", "*.h")

EXCL_LINE = re.compile(r"//\s*GCOVR_EXCL_LINE\b")
EXCL_START = re.compile(r"//\s*GCOVR_EXCL_START\b")
EXCL_STOP = re.compile(r"//\s*GCOVR_EXCL_STOP\b")
STRUCTURAL = re.compile(r"^[{}()\[\];,\s]*$")
# An unreachability statement: by construction it is never executed, so it is
# allowed uncovered without a marker (C++ analog of unreachable!()/todo!()).
UNREACHABLE = re.compile(
    r"^\s*(return\s+)?("
    r"std::unreachable\s*\(\)"
    r"|__builtin_unreachable\s*\(\)"
    r"|abort\s*\(\)"
    r"|assert\s*\(\s*(false|0)\b[^;]*\)"
    r")\s*;?\s*$"
)


def _source_files() -> list[Path]:
    return [p for d in SOURCE_DIRS for g in SOURCE_GLOBS
            for p in sorted(d.rglob(g))]


def excluded_lines(path: Path) -> set[int]:
    """Line numbers under coverage-exclusion markers in one file.

    Block START/STOP marker lines themselves are not counted; non-blank lines
    inside a block are, as are inline // GCOVR_EXCL_LINE lines."""
    out: set[int] = set()
    in_block = False
    for i, line in enumerate(path.read_text().splitlines(), 1):
        if EXCL_START.search(line):
            in_block = True
            continue
        if EXCL_STOP.search(line):
            in_block = False
            continue
        if in_block:
            if line.strip():
                out.add(i)
        elif EXCL_LINE.search(line):
            out.add(i)
    return out


def count_excluded() -> int:
    """Count non-blank source lines under coverage-exclusion markers."""
    return sum(len(excluded_lines(p)) for p in _source_files())


def _relativize(sf: str) -> str | None:
    """Map an LCOV SF: path to a repo-relative path under our source dirs,
    or None if it falls outside src/ and include/hegel/."""
    try:
        rel = Path(sf).resolve().relative_to(Path.cwd().resolve())
    except ValueError:
        return None
    s = rel.as_posix()
    for d in SOURCE_DIRS:
        if s == d.as_posix() or s.startswith(d.as_posix() + "/"):
            return s
    return None


def parse_per_line(lcov_info: Path) -> dict[str, dict[int, tuple[int, bool]]]:
    """file -> {line_number: (max_count_over_instantiations, excluded)}.

    Reads an LCOV file: `SF:<path>` opens a record, `DA:<line>,<count>` gives a
    per-line execution count (llvm-cov already aggregates instantiations, but we
    still max over duplicate DA: entries defensively), `end_of_record` closes."""
    out: dict[str, dict[int, tuple[int, bool]]] = {}
    cur: str | None = None
    excl: set[int] = set()
    for line in lcov_info.read_text().splitlines():
        if line.startswith("SF:"):
            cur = _relativize(line[3:].strip())
            if cur is not None:
                out.setdefault(cur, {})
                p = Path(cur)
                excl = excluded_lines(p) if p.exists() else set()
        elif line.startswith("DA:") and cur is not None:
            n_str, _, count_str = line[3:].partition(",")
            n = int(n_str)
            count = int(float(count_str.split(",")[0]))
            agg = out[cur]
            if n in agg:
                prev_count, prev_excl = agg[n]
                agg[n] = (max(prev_count, count), prev_excl)
            else:
                agg[n] = (count, n in excl)
        elif line.startswith("end_of_record"):
            cur = None
    return out


def _source_lines(path: Path, cache: dict[Path, list[str]]) -> list[str]:
    if path not in cache:
        try:
            cache[path] = path.read_text().splitlines()
        except OSError:
            cache[path] = []
    return cache[path]


def countable_lines(parsed: dict[str, dict[int, tuple[int, bool]]],
                    cache: dict[Path, list[str]]):
    """Yield (file, line, count, content) for lines that count toward coverage:
    excluded, structural, and unreachability lines are skipped, since none
    represents testable behaviour."""
    for rel, agg in parsed.items():
        lines = _source_lines(Path(rel), cache)
        for n in sorted(agg):
            count, excluded = agg[n]
            if excluded:
                continue
            content = lines[n - 1] if 1 <= n <= len(lines) else ""
            if STRUCTURAL.match(content) or UNREACHABLE.match(content):
                continue
            yield rel, n, count, content.strip()


def line_coverage(parsed, cache) -> tuple[int, int]:
    """Return (covered, total) over countable source lines."""
    covered = total = 0
    for line in countable_lines(parsed, cache):
        total += 1
        if line[2] > 0:  # (file, line_no, count, content)
            covered += 1
    return covered, total


def find_gaps(parsed, cache) -> list[tuple[str, int, str]]:
    """Return (file, line, content) for uncovered countable lines."""
    return [(rel, n, content)
            for rel, n, count, content in countable_lines(parsed, cache)
            if count == 0]


def find_stale_exclusions(parsed, cache) -> list[tuple[str, int, str]]:
    """Inline // GCOVR_EXCL_LINE markers on lines that are actually covered.

    Block (START/STOP) regions are exempt: they intentionally span covered and
    uncovered code at a boundary, so coverage of a block line is not stale.
    """
    stale: list[tuple[str, int, str]] = []
    for path in _source_files():
        rel = str(path)
        agg = parsed.get(rel, {})
        in_block = False
        for i, line in enumerate(_source_lines(path, cache), 1):
            if EXCL_START.search(line):
                in_block = True
                continue
            if EXCL_STOP.search(line):
                in_block = False
                continue
            if in_block:
                continue
            if EXCL_LINE.search(line) and agg.get(i, (0, False))[0] > 0:
                stale.append((rel, i, line.strip()))
    return stale


def write_ratchet(excluded: int) -> None:
    RATCHET_FILE.write_text(json.dumps({"excluded": excluded}, indent=2) + "\n")


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: check-coverage.py <lcov-info>", file=sys.stderr)
        return 2
    parsed = parse_per_line(Path(sys.argv[1]))
    cache: dict[Path, list[str]] = {}
    failed = False

    covered, total = line_coverage(parsed, cache)
    pct = 100.0 * covered / total if total else 100.0
    print(f"Line coverage (per source line): {pct:.1f}% ({covered}/{total})")

    # 1. Coverage gate: 100% of non-excluded lines.
    gaps = find_gaps(parsed, cache)
    if gaps:
        failed = True
        print(f"\nUncovered, non-excluded lines ({len(gaps)}):", file=sys.stderr)
        for rel, n, content in gaps:
            print(f"  {rel}:{n}: {content}", file=sys.stderr)
        print(
            "\nEvery non-excluded line must be covered. Add tests, or — if a "
            "line is genuinely unreachable — mark it // GCOVR_EXCL_LINE (or "
            "wrap a region in // GCOVR_EXCL_START .. STOP) and raise the "
            "ratchet.",
            file=sys.stderr,
        )

    # 2. Stale inline exclusions: a covered line still marked excluded.
    stale = find_stale_exclusions(parsed, cache)
    if stale:
        failed = True
        print(f"\nCovered lines still marked // GCOVR_EXCL_LINE ({len(stale)}):",
              file=sys.stderr)
        for rel, n, content in stale:
            print(f"  {rel}:{n}: {content}", file=sys.stderr)
        print("\nThese lines are now covered — remove the marker and lower the "
              "ratchet.", file=sys.stderr)

    # 3. Exclusion ratchet: only a human may raise it; it auto-tightens down.
    excluded = count_excluded()
    print(f"Coverage-excluded lines: {excluded}")
    try:
        ratchet = json.loads(RATCHET_FILE.read_text())["excluded"]
    except (OSError, KeyError, json.JSONDecodeError) as e:
        print(f"ERROR: cannot read ratchet from {RATCHET_FILE}: {e}",
              file=sys.stderr)
        return 2

    if excluded > ratchet:
        failed = True
        print(
            f'\nExclusion ratchet EXCEEDED: {excluded} > {ratchet}. Only a human '
            f'may raise it: if the new exclusions are justified, set "excluded" '
            f"in {RATCHET_FILE} to {excluded}; otherwise remove the markers.",
            file=sys.stderr,
        )
    elif excluded < ratchet:
        write_ratchet(excluded)
        print(f"Ratchet tightened: {ratchet} -> {excluded} "
              f"(updated {RATCHET_FILE}).")
    else:
        print(f"Coverage exclusion ratchet OK (matches {ratchet}).")

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
