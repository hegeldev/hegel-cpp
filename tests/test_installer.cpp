#include <gtest/gtest.h>

#include <installer.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
using namespace hegel::impl;

namespace {

    fs::path unique_tmp(const std::string& name) {
        fs::path base = fs::temp_directory_path();
        base /= name + "-" + std::to_string(::getpid()) + "-" +
                std::to_string(::clock());
        std::error_code ec;
        fs::remove_all(base, ec);
        fs::create_directories(base);
        return base;
    }

    struct RemoveOnExit {
        fs::path p;
        explicit RemoveOnExit(fs::path path) : p(std::move(path)) {}
        ~RemoveOnExit() {
            std::error_code ec;
            fs::remove_all(p, ec);
        }
    };

    // Save/restore an env var across a test so we don't leak state into
    // sibling tests in the same binary.
    class ScopedEnv {
      public:
        ScopedEnv(const char* name, const char* value) : name_(name) {
            if (const char* prev = std::getenv(name)) {
                had_prev_ = true;
                prev_ = prev;
            }
            if (value != nullptr) {
                ::setenv(name, value, 1);
            } else {
                ::unsetenv(name);
            }
        }
        ~ScopedEnv() {
            if (had_prev_) {
                ::setenv(name_, prev_.c_str(), 1);
            } else {
                ::unsetenv(name_);
            }
        }
        ScopedEnv(const ScopedEnv&) = delete;
        ScopedEnv& operator=(const ScopedEnv&) = delete;

      private:
        const char* name_;
        bool had_prev_ = false;
        std::string prev_;
    };

    fs::path make_executable(const fs::path& path,
                             const std::string& body = "#!/bin/sh\n") {
        std::ofstream(path) << body;
        ::chmod(path.c_str(), 0755);
        return path;
    }

} // namespace

TEST(ResolveHegelPath, ExistingExecutableAbsolutePath) {
    auto tmp = unique_tmp("hegel-resolve-abs");
    RemoveOnExit cleanup(tmp);
    auto bin = make_executable(tmp / "custom-hegel");

    EXPECT_EQ(resolve_hegel_path(bin.string()), bin.string());
}

TEST(ResolveHegelPath, ExistingNonExecutableThrows) {
    auto tmp = unique_tmp("hegel-resolve-nonexec");
    RemoveOnExit cleanup(tmp);
    auto bin = tmp / "custom-hegel";
    std::ofstream(bin) << "not executable";
    ::chmod(bin.c_str(), 0644);

    EXPECT_THROW(resolve_hegel_path(bin.string()), std::runtime_error);
}

TEST(ResolveHegelPath, BareNameResolvedViaPath) {
    auto tmp = unique_tmp("hegel-resolve-path");
    RemoveOnExit cleanup(tmp);
    auto bin = make_executable(tmp / "custom-hegel-xyz");

    ScopedEnv path("PATH", tmp.c_str());
    EXPECT_EQ(resolve_hegel_path("custom-hegel-xyz"), bin.string());
}

TEST(ResolveHegelPath, BareNameNotFoundThrows) {
    auto tmp = unique_tmp("hegel-resolve-missing-bare");
    RemoveOnExit cleanup(tmp);
    ScopedEnv path("PATH", tmp.c_str());

    EXPECT_THROW(resolve_hegel_path("definitely-not-a-real-binary"),
                 std::runtime_error);
}

TEST(ResolveHegelPath, MissingAbsolutePathThrows) {
    EXPECT_THROW(resolve_hegel_path("/nonexistent/dir/hegel"),
                 std::runtime_error);
}

TEST(HegelCommand, UsesOverrideWhenSet) {
    auto tmp = unique_tmp("hegel-cmd-override");
    RemoveOnExit cleanup(tmp);
    auto bin = make_executable(tmp / "custom-hegel");

    ScopedEnv override_env(HEGEL_SERVER_COMMAND_ENV, bin.c_str());

    auto cmd = hegel_command();
    ASSERT_EQ(cmd.size(), 1u);
    EXPECT_EQ(cmd[0], bin.string());
}

TEST(HegelCommand, DefaultUsesUvToolRun) {
    // Fake uv on a controlled PATH so find_uv() short-circuits without
    // touching the embedded installer.
    auto tmp = unique_tmp("hegel-cmd-default");
    RemoveOnExit cleanup(tmp);
    auto fake_uv = make_executable(tmp / "uv");

    ScopedEnv override_env(HEGEL_SERVER_COMMAND_ENV, nullptr);
    ScopedEnv path("PATH", tmp.c_str());

    auto cmd = hegel_command();
    ASSERT_EQ(cmd.size(), 6u);
    EXPECT_EQ(cmd[0], fake_uv.string());
    EXPECT_EQ(cmd[1], "tool");
    EXPECT_EQ(cmd[2], "run");
    EXPECT_EQ(cmd[3], "--from");
    EXPECT_EQ(cmd[4], std::string("hegel-core==") + HEGEL_SERVER_VERSION);
    EXPECT_EQ(cmd[5], "hegel");
}
