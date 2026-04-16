#pragma once

/**
 * @file temp_project.h
 * @brief Compile + run an arbitrary hegel C++ program at test time.
 *
 * Mirrors rust/tests/common/project.rs (TempRustProject). A single shared
 * cmake build directory is configured once per process and reused across
 * tests, so per-test cost is just recompiling main.cpp + linking against
 * the parent-built libhegel.a.
 *
 * Tests using TempCppProject must run sequentially within a single gtest
 * binary (gtest's default behavior). Multiple binaries running concurrently
 * via `ctest -j` are fine because each binary has its own shared build dir
 * (named by HEGEL_TEMP_PROJECT_BASE_DIR, set per binary at CMake time).
 */

#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "common/subprocess.h"

#ifndef HEGEL_TEMP_PROJECT_BASE_DIR
#error "HEGEL_TEMP_PROJECT_BASE_DIR must be defined at build time"
#endif
#ifndef HEGEL_TEMP_PROJECT_TEMPLATE_DIR
#error "HEGEL_TEMP_PROJECT_TEMPLATE_DIR must be defined at build time"
#endif
#ifndef HEGEL_TEMP_PROJECT_BUILD_INFO_FILE
#error "HEGEL_TEMP_PROJECT_BUILD_INFO_FILE must be defined at build time"
#endif
#ifndef HEGEL_TEMP_PROJECT_CMAKE_EXE
#error "HEGEL_TEMP_PROJECT_CMAKE_EXE must be defined at build time"
#endif
#ifndef HEGEL_TEMP_PROJECT_CXX_FLAGS
#error "HEGEL_TEMP_PROJECT_CXX_FLAGS must be defined at build time"
#endif
#ifndef HEGEL_TEMP_PROJECT_EXE_LINKER_FLAGS
#error "HEGEL_TEMP_PROJECT_EXE_LINKER_FLAGS must be defined at build time"
#endif

namespace hegel::tests::common {

    class TempCppProject {
      public:
        TempCppProject() = default;

        TempCppProject& main_file(std::string source) {
            main_source_ = std::move(source);
            return *this;
        }

        TempCppProject& env(std::string key, std::string value) {
            env_overrides_.emplace_back(std::move(key), std::move(value));
            return *this;
        }

        TempCppProject& env_remove(std::string key) {
            env_removes_.push_back(std::move(key));
            return *this;
        }

        /// Assert that the binary exits non-zero AND its stderr contains
        /// `substr`. Mirrors TempRustProject::expect_failure.
        TempCppProject& expect_failure(std::string substr) {
            expect_failure_substr_ = std::move(substr);
            return *this;
        }

        SubprocessResult run(const std::vector<std::string>& args = {}) {
            ensure_configured();
            write_main();
            build();
            auto out = exec(args);
            check_expectation(out);
            return out;
        }

      private:
        static const std::filesystem::path& base_dir() {
            static const std::filesystem::path p{HEGEL_TEMP_PROJECT_BASE_DIR};
            return p;
        }
        static std::filesystem::path src_dir() { return base_dir() / "src"; }
        static std::filesystem::path build_dir() {
            return base_dir() / "build";
        }
        static const std::filesystem::path& template_dir() {
            static const std::filesystem::path p{
                HEGEL_TEMP_PROJECT_TEMPLATE_DIR};
            return p;
        }
        static const std::filesystem::path& build_info_file() {
            static const std::filesystem::path p{
                HEGEL_TEMP_PROJECT_BUILD_INFO_FILE};
            return p;
        }
        static const std::string& cmake_exe() {
            static const std::string s{HEGEL_TEMP_PROJECT_CMAKE_EXE};
            return s;
        }
        static const std::string& cxx_flags() {
            static const std::string s{HEGEL_TEMP_PROJECT_CXX_FLAGS};
            return s;
        }
        static const std::string& exe_linker_flags() {
            static const std::string s{HEGEL_TEMP_PROJECT_EXE_LINKER_FLAGS};
            return s;
        }

        // Configure the shared build dir on first call within this process.
        // The build dir is intentionally NOT wiped between processes: cmake's
        // reconfigure is idempotent and tracks its own input files (template
        // CMakeLists.txt + build_info_file are include()d, so changes trigger
        // regeneration). This lets per-process cost drop from ~2s (cold
        // configure) to ~0.5s (warm reconfigure check + incremental build)
        // when ctest runs each TEST() as a separate process via
        // gtest_discover_tests.
        static void ensure_configured() {
            static std::once_flag flag;
            std::call_once(flag, [] {
                std::filesystem::create_directories(src_dir());
                std::filesystem::create_directories(build_dir());

                std::filesystem::copy_file(
                    template_dir() / "CMakeLists.txt",
                    src_dir() / "CMakeLists.txt",
                    std::filesystem::copy_options::overwrite_existing);
                if (!std::filesystem::exists(src_dir() / "main.cpp")) {
                    std::ofstream f(src_dir() / "main.cpp", std::ios::trunc);
                    f << "int main() { return 0; }\n";
                }

                std::vector<std::string> args = {"-S",
                                                 src_dir().string(),
                                                 "-B",
                                                 build_dir().string(),
                                                 "-DHEGEL_BUILD_INFO_FILE=" +
                                                     build_info_file().string(),
                                                 "-DCMAKE_BUILD_TYPE=Release"};
                // Match the parent build's CXX/linker flags so the subject
                // uses the same stdlib etc. (e.g. -stdlib=libc++). Only
                // forwarded when non-empty to keep the cmake command line
                // clean in the common case.
                if (!cxx_flags().empty()) {
                    args.push_back("-DCMAKE_CXX_FLAGS=" + cxx_flags());
                }
                if (!exe_linker_flags().empty()) {
                    args.push_back("-DCMAKE_EXE_LINKER_FLAGS=" +
                                   exe_linker_flags());
                }
                auto r = run_subject(cmake_exe(), args);
                if (r.exit_code != 0) {
                    throw std::runtime_error(
                        "TempCppProject: cmake configure failed (exit " +
                        std::to_string(r.exit_code) + ")\nstdout:\n" +
                        r.stdout_data + "\nstderr:\n" + r.stderr_data);
                }
            });
        }

        // Skip the write if main.cpp's content is byte-identical to what we'd
        // write. This preserves mtime so cmake --build short-circuits the
        // recompile (which dominates per-test cost — hegel.h pulls in
        // reflect-cpp templates and takes ~1.8s to compile from scratch).
        // Tests that reuse the same FAILING_TEST_CODE thus pay the compile
        // cost exactly once across the whole gtest binary run.
        void write_main() const {
            auto path = src_dir() / "main.cpp";
            if (std::filesystem::exists(path)) {
                std::ifstream existing(path, std::ios::binary);
                std::stringstream buf;
                buf << existing.rdbuf();
                if (buf.str() == main_source_) {
                    return;
                }
            }
            std::ofstream f(path, std::ios::binary | std::ios::trunc);
            if (!f) {
                throw std::runtime_error("TempCppProject: failed to open " +
                                         path.string());
            }
            f << main_source_;
            if (!f) {
                throw std::runtime_error("TempCppProject: failed to write " +
                                         path.string());
            }
        }

        static void build() {
            auto r = run_subject(cmake_exe(), {"--build", build_dir().string(),
                                               "--target", "subject"});
            if (r.exit_code != 0) {
                throw std::runtime_error(
                    "TempCppProject: cmake build failed (exit " +
                    std::to_string(r.exit_code) + ")\nstdout:\n" +
                    r.stdout_data + "\nstderr:\n" + r.stderr_data);
            }
        }

        SubprocessResult exec(const std::vector<std::string>& args) const {
            return run_subject((build_dir() / "subject").string(), args,
                               env_overrides_, env_removes_);
        }

        void check_expectation(const SubprocessResult& out) const {
            if (!expect_failure_substr_.has_value()) {
                return;
            }
            if (out.exit_code == 0) {
                FAIL() << "TempCppProject: expected failure containing \""
                       << *expect_failure_substr_
                       << "\" but binary exited 0\nstderr:\n"
                       << out.stderr_data;
            }
            if (out.stderr_data.find(*expect_failure_substr_) ==
                std::string::npos) {
                FAIL() << "TempCppProject: expected stderr to contain \""
                       << *expect_failure_substr_ << "\"\nactual stderr:\n"
                       << out.stderr_data;
            }
        }

        std::string main_source_;
        std::vector<std::pair<std::string, std::string>> env_overrides_;
        std::vector<std::string> env_removes_;
        std::optional<std::string> expect_failure_substr_;
    };

} // namespace hegel::tests::common
