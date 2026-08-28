"""Pin a new libhegel release and leave a fully-formed commit on a local branch.

Resolves the target hegel-rust version (an explicit argument, else the latest
release), writes it into `cmake/libhegel.cmake`, refreshes the vendored C ABI
header (`libhegel/hegel.h`) from hegel-rust at the matching tag, repins the Nix
flake (`nix/flake.nix`) version and per-platform SHA-256 hashes from the release
sidecars, and drops a `RELEASE.md` so merging the PR cuts a hegel-cpp release.

The commit is intentionally *not* pushed: the workflow then realigns the C++
wrapper layer to the new release, amends the result into this commit, and
pushes once.

Requires the GitHub CLI (`gh`) on PATH with `GH_TOKEN` set.
"""

import base64
import os
import re
import subprocess
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
RUST_REPO = "hegeldev/hegel-rust"
# One branch per pinned version. A fixed, reused branch meant every release
# force-recreated it from main, clobbering any manual or agent work pushed to
# the open PR; a per-version branch keeps re-runs of the *same* version
# idempotent (the workflow's force-push) while never touching another
# version's work. The workflow closes superseded bot-only bump PRs after
# pushing.
BRANCH_PREFIX = "ci/bump-hegel-rust-"
LIBHEGEL_CMAKE = ROOT / "cmake" / "libhegel.cmake"
HEADER = ROOT / "libhegel" / "hegel.h"
FLAKE = ROOT / "nix" / "flake.nix"
RELEASE_MD = ROOT / "RELEASE.md"

# The generated C ABI header committed in hegel-rust, tagged with each release.
RUST_HEADER_PATH = "hegel-c/include/hegel.h"

VERSION_RE = re.compile(r'(HEGEL_LIBHEGEL_VERSION\s+")([^"]+)(")')
FLAKE_VERSION_RE = re.compile(r'(libhegelVersion\s*=\s*")([^"]+)(")')
# Each libhegel asset block in flake.nix: an `asset = "..."` line immediately
# followed by its `sha256 = "..."` line. Recapture the hash to repin it.
FLAKE_ASSET_RE = re.compile(
    r'(asset\s*=\s*"(?P<asset>[^"]+)";\s*sha256\s*=\s*")[0-9a-fA-F]+(")'
)


def git(*args: str) -> None:
    subprocess.run(["git", *args], check=True, cwd=ROOT)


def set_output(name: str, value: str) -> None:
    """Expose a step output to later workflow steps (no-op outside Actions)."""
    out = os.environ.get("GITHUB_OUTPUT")
    if not out:
        return
    with open(out, "a") as f:
        f.write(f"{name}={value}\n")


def get_pinned_version() -> str:
    m = VERSION_RE.search(LIBHEGEL_CMAKE.read_text(encoding="utf-8"))
    assert m is not None, "could not find HEGEL_LIBHEGEL_VERSION in libhegel.cmake"
    return m.group(2)


def resolve_latest() -> str:
    # `gh release view` with no tag resolves the latest release; strip the
    # leading `v` so it matches the pinned form (e.g. "0.23.2").
    tag = subprocess.run(
        ["gh", "release", "view", "--repo", RUST_REPO,
         "--json", "tagName", "--jq", ".tagName"],
        check=True, capture_output=True, text=True,
    ).stdout.strip()
    return tag.lstrip("v")


def set_pinned_version(version: str) -> None:
    text = LIBHEGEL_CMAKE.read_text(encoding="utf-8")
    new_text, n = VERSION_RE.subn(rf"\g<1>{version}\g<3>", text, count=1)
    assert n == 1, "expected exactly one HEGEL_LIBHEGEL_VERSION line in libhegel.cmake"
    LIBHEGEL_CMAKE.write_text(new_text, encoding="utf-8")


def refresh_header(version: str) -> None:
    # Fetch the C ABI header from hegel-rust at the released tag and overwrite the
    # vendored copy so it matches exactly what libhegel v{version} exposes.
    content_b64 = subprocess.run(
        ["gh", "api",
         f"repos/{RUST_REPO}/contents/{RUST_HEADER_PATH}?ref=v{version}",
         "--jq", ".content"],
        check=True, capture_output=True, text=True,
    ).stdout.strip()
    header = base64.b64decode(content_b64)
    assert header, f"fetched empty {RUST_HEADER_PATH} for v{version}"
    HEADER.write_bytes(header)
    # Reformat to the repo style (left pointers, etc.) so check-format passes;
    # same invocation as `just format`.
    subprocess.run(["uvx", "clang-format", "-i", str(HEADER)], check=True, cwd=ROOT)


def fetch_asset_sha256(version: str, asset: str) -> str:
    # Each release asset ships a `<asset>.sha256` sidecar ("<hex>  <asset>").
    url = f"https://github.com/{RUST_REPO}/releases/download/v{version}/{asset}.sha256"
    with urllib.request.urlopen(url) as resp:
        sidecar = resp.read().decode("utf-8")
    m = re.search(r"[0-9a-fA-F]{64}", sidecar)
    assert m is not None, f"could not parse SHA-256 for {asset} from {url}"
    return m.group(0)


def refresh_flake(version: str) -> None:
    # Repin the Nix flake to match cmake/libhegel.cmake: bump the version and
    # refresh each platform asset's SHA-256 from its release sidecar.
    text = FLAKE.read_text(encoding="utf-8")
    text, n = FLAKE_VERSION_RE.subn(rf"\g<1>{version}\g<3>", text, count=1)
    assert n == 1, "expected exactly one libhegelVersion line in flake.nix"

    def repin(m: "re.Match[str]") -> str:
        digest = fetch_asset_sha256(version, m.group("asset"))
        return f"{m.group(1)}{digest}{m.group(3)}"

    text, n = FLAKE_ASSET_RE.subn(repin, text)
    assert n >= 1, "expected at least one libhegel asset block in flake.nix"
    FLAKE.write_text(text, encoding="utf-8")


def bump(requested: str) -> None:
    current = get_pinned_version()
    target = requested or resolve_latest()

    if target == current:
        print(f"Already pinned to v{current}; nothing to do.")
        set_output("bumped", "false")
        return

    set_pinned_version(target)
    refresh_header(target)
    refresh_flake(target)

    current_url = f"https://github.com/{RUST_REPO}/releases/tag/v{current}"
    new_url = f"https://github.com/{RUST_REPO}/releases/tag/v{target}"

    RELEASE_MD.write_text(
        "RELEASE_TYPE: patch\n\n"
        f"This patch bumps our pinned `libhegel` ([hegel-rust]({RUST_REPO})) from "
        f"[{current}]({current_url}) to [{target}]({new_url}).\n",
        encoding="utf-8",
    )

    app_id = os.environ["HEGEL_RELEASE_APP_ID"]
    git("config", "user.name", "hegel-release[bot]")
    git("config", "user.email", f"{app_id}+hegel-release[bot]@users.noreply.github.com")

    # The per-version branch for this release. Commit locally only; the
    # workflow pushes it after folding in the wrapper alignment.
    branch = BRANCH_PREFIX + target
    git("checkout", "-B", branch)
    git("add", str(LIBHEGEL_CMAKE), str(HEADER), str(FLAKE), str(RELEASE_MD))
    git("commit", "-m", f"Bump pinned libhegel to {target}")

    set_output("bumped", "true")
    set_output("version", target)
    set_output("branch", branch)


if __name__ == "__main__":
    # An optional argument pins that exact version; with none we take the
    # latest. The repository_dispatch trigger passes client_payload.version.
    bump(sys.argv[1] if len(sys.argv) > 1 else "")
