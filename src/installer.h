#pragma once

#include <string>
#include <vector>

namespace hegel::impl {

    /// Environment variable that, when set, overrides how the hegel server
    /// binary is invoked. The value may be an absolute path, a relative
    /// path, or a bare name resolvable via `PATH`.
    constexpr const char* HEGEL_SERVER_COMMAND_ENV = "HEGEL_SERVER_COMMAND";

    /// hegel-core version that this library is built against. Kept in sync
    /// with the Rust library's `HEGEL_SERVER_VERSION`.
    constexpr const char* HEGEL_SERVER_VERSION = "0.4.0";

    /// Returns the argv prefix used to invoke the hegel server.
    ///
    /// Behavior matches hegel-rust's `hegel_command`:
    /// - If `HEGEL_SERVER_COMMAND` is set, the override is resolved (via
    ///   `resolve_hegel_path`) and returned as a single-element argv.
    /// - Otherwise, the result is
    ///   `{uv, "tool", "run", "--from", "hegel-core==<VERSION>", "hegel"}`
    ///   where `uv` comes from `hegel::impl::uv::find_uv()`, bootstrapping
    ///   uv on first use if necessary.
    ///
    /// The caller appends the remaining flags (`--stdio`, `--verbosity`, …).
    ///
    /// Throws std::runtime_error if the override is set but unresolvable, or
    /// if uv cannot be found or installed.
    std::vector<std::string> hegel_command();

    // Test hooks ----------------------------------------------------------

    /// Resolve a user-supplied `HEGEL_SERVER_COMMAND` value.
    ///
    /// - If `path` names an existing file, it is validated as executable and
    ///   returned unchanged.
    /// - If `path` has no `/`, it is looked up on `PATH` (via
    ///   `utils::which`), validated, and the resolved absolute path is
    ///   returned.
    /// - Otherwise (missing path or unresolvable bare name), throws
    ///   std::runtime_error naming the env var.
    std::string resolve_hegel_path(const std::string& path);

} // namespace hegel::impl
