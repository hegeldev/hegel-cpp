#include <gtest/gtest.h>

#include <uv.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace fs = std::filesystem;
using namespace hegel::impl::uv;

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

} // namespace

TEST(Uv, CacheDirWithXdg) {
    auto r = cache_dir_from(std::string("/tmp/xdg"), std::nullopt);
    EXPECT_EQ(r, fs::path("/tmp/xdg/hegel"));
}

TEST(Uv, CacheDirWithHome) {
    auto r = cache_dir_from(std::nullopt, std::string("/home/test"));
    EXPECT_EQ(r, fs::path("/home/test/.cache/hegel"));
}

TEST(Uv, CacheDirNoXdgNoHomeThrows) {
    EXPECT_THROW(cache_dir_from(std::nullopt, std::nullopt),
                 std::runtime_error);
}

TEST(Uv, FindUvImplUsesPathUvWhenAvailable) {
    auto tmp = unique_tmp("hegel-uv-path");
    RemoveOnExit cleanup(tmp);
    auto fake = tmp / "uv";
    std::ofstream(fake) << "fake uv";

    auto r = find_uv_impl(fake.string(), fs::path("/nonexistent"));
    EXPECT_EQ(r, fake.string());
}

TEST(Uv, FindUvImplReturnsCachedWhenNotInPath) {
    auto tmp = unique_tmp("hegel-uv-cache");
    RemoveOnExit cleanup(tmp);
    auto fake = tmp / "uv";
    std::ofstream(fake) << "fake uv";

    auto r = find_uv_impl(std::nullopt, tmp);
    EXPECT_EQ(r, fake.string());
}

TEST(Uv, InstallUvFailsWithBadShCommand) {
    auto tmp = unique_tmp("hegel-uv-badsh");
    RemoveOnExit cleanup(tmp);
    EXPECT_THROW(install_uv_with_sh(tmp, "definitely_not_a_real_shell_xyz"),
                 std::runtime_error);
}

// Integration test — actually downloads uv via the embedded installer
// and verifies the resulting binary runs.
TEST(Uv, FindUvImplInstallsWhenMissing) {
    auto tmp = unique_tmp("hegel-uv-install");
    RemoveOnExit cleanup(tmp);

    auto r = find_uv_impl(std::nullopt, tmp);
    ASSERT_TRUE(fs::is_regular_file(tmp / "uv"));
    EXPECT_EQ(r, (tmp / "uv").string());

    // Smoke-test the installed binary: `uv --version` should print
    // something like "uv 0.x.y" and exit zero.
    std::string cmd = r + " --version";
    FILE* p = ::popen(cmd.c_str(), "r");
    ASSERT_NE(p, nullptr);
    std::string out;
    char buf[256];
    while (std::fgets(buf, sizeof(buf), p) != nullptr) {
        out += buf;
    }
    int rc = ::pclose(p);
    EXPECT_EQ(rc, 0);
    EXPECT_FALSE(out.empty());
    EXPECT_NE(out.find("uv"), std::string::npos);
}
