import os
import shutil
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
WEBSITE = ROOT / "website"
DOCS_SRC = ROOT / "build" / "docs" / "html"
DOCS_DEST = WEBSITE / "public" / "cpp"
# The docs are served at hegel.dev/cpp. Vercel's `trailingSlash: false` strips
# the trailing slash, so the browser's base URL at /cpp is / — which breaks
# Doxygen's relative asset hrefs (they resolve to /doxygen.css instead of
# /cpp/doxygen.css). Inject <base href="/cpp/"> so relative URLs resolve
# against the docs root regardless of trailing slash.
BASE_HREF = "/cpp/"
PUSH_ATTEMPTS = 5


def git(*args: str) -> None:
    subprocess.run(["git", *args], check=True, cwd=WEBSITE)


def inject_base_href(root: Path, href: str) -> None:
    tag = f'<base href="{href}">'
    for html_path in root.rglob("*.html"):
        content = html_path.read_text(encoding="utf-8")
        content = content.replace("<head>", f"<head>\n{tag}", 1)
        html_path.write_text(content, encoding="utf-8")


def push_with_retry() -> None:
    # Releases of other hegel libraries may be pushing to website main at the
    # same time (to different paths). On rejection, rebase onto the new main
    # and retry — the rebase is clean since each library touches its own path.
    for attempt in range(PUSH_ATTEMPTS):
        result = subprocess.run(
            ["git", "push", "origin", "HEAD:main"], cwd=WEBSITE
        )
        if result.returncode == 0:
            return
        if attempt == PUSH_ATTEMPTS - 1:
            break
        time.sleep(2)
        git("fetch", "origin", "main")
        git("rebase", "origin/main")
    raise RuntimeError(f"Failed to push docs after {PUSH_ATTEMPTS} attempts.")


def main() -> None:
    version = os.environ["VERSION"]
    app_id = os.environ["HEGEL_RELEASE_APP_ID"]
    app_slug = os.environ["HEGEL_RELEASE_APP_SLUG"]

    if DOCS_DEST.exists():
        shutil.rmtree(DOCS_DEST)
    shutil.copytree(DOCS_SRC, DOCS_DEST)
    inject_base_href(DOCS_DEST, BASE_HREF)

    git("config", "user.name", f"{app_slug}[bot]")
    git("config", "user.email", f"{app_id}+{app_slug}[bot]@users.noreply.github.com")

    git("add", "public/cpp")

    status = subprocess.check_output(
        ["git", "status", "--porcelain"], cwd=WEBSITE, text=True
    )
    if not status.strip():
        print("No doc changes to publish.")
        return

    git("commit", "-m", f"Update hegel-cpp docs to v{version}")
    push_with_retry()


if __name__ == "__main__":
    main()
