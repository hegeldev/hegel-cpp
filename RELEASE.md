RELEASE_TYPE: minor

The hegel server binary is now managed via a pinned `HEGEL_VERSION` in
`CMakeLists.txt`. CMake configure installs the exact hegel-core commit into
a project-local `.hegel/venv` with version caching, replacing the old
PATH-lookup and auto-install approach. Set `HEGEL_CMD` to override to a
custom binary at runtime. Requires `uv` on PATH.
