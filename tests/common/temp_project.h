#pragma once

/**
 * @file temp_project.h
 * @brief Compile + run an arbitrary hegel C++ program at test time.
 *
 * Mirrors rust/tests/common/project.rs (TempRustProject). The `subject`
 * executable is defined as a regular target in the parent cmake build tree
 * (see tests/CMakeLists.txt), so it inherits the exact toolchain used to
 * build libhegel.a — no second cmake configure, no flag forwarding, no ABI
 * mismatch when the parent uses e.g. `-stdlib=libc++`. At test time this
 * class overwrites the subject's source file and invokes `cmake --build
 * --target subject` to rebuild, then runs the produced binary.
 *
 * Tests using TempCppProject must run sequentially (gtest's default within
 * a single binary; ctest RESOURCE_LOCK across binaries) because they share
 * the single `subject` target.
 */

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "common/subprocess.h"

#ifndef HEGEL_TEMP_PROJECT_MAIN_CPP
#error "HEGEL_TEMP_PROJECT_MAIN_CPP must be defined at build time"
#endif
#ifndef HEGEL_TEMP_PROJECT_SUBJECT_BIN
#error "HEGEL_TEMP_PROJECT_SUBJECT_BIN must be defined at build time"
#endif
#ifndef HEGEL_TEMP_PROJECT_BUILD_DIR
#error "HEGEL_TEMP_PROJECT_BUILD_DIR must be defined at build time"
#endif
#ifndef HEGEL_TEMP_PROJECT_CMAKE_EXE
#error "HEGEL_TEMP_PROJECT_CMAKE_EXE must be defined at build time"
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
            write_main();
            build();
            auto out = exec(args);
            check_expectation(out);
            return out;
        }

      private:
        static const std::filesystem::path& main_cpp_path() {
            static const std::filesystem::path p{HEGEL_TEMP_PROJECT_MAIN_CPP};
            return p;
        }
        static const std::filesystem::path& subject_bin() {
            static const std::filesystem::path p{
                HEGEL_TEMP_PROJECT_SUBJECT_BIN};
            return p;
        }
        static const std::string& build_dir() {
            static const std::string s{HEGEL_TEMP_PROJECT_BUILD_DIR};
            return s;
        }
        static const std::string& cmake_exe() {
            static const std::string s{HEGEL_TEMP_PROJECT_CMAKE_EXE};
            return s;
        }

        // Skip the write if main.cpp's content is byte-identical to what we'd
        // write. This preserves mtime so `cmake --build` short-circuits the
        // recompile (which dominates per-test cost — hegel.h pulls in
        // reflect-cpp templates and takes ~1.8s to compile from scratch).
        // Tests that reuse the same test source thus pay the compile cost
        // exactly once across the whole gtest binary run.
        void write_main() const {
            const auto& path = main_cpp_path();
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
            f.close();
            // Make only compares mtimes at 1-second precision. When a full
            // write+build+run cycle completes within the same wall-clock
            // second, the freshly written main.cpp can have the same mtime
            // as the existing subject binary, and `cmake --build` (via make)
            // decides no rebuild is needed — so the next test would run a
            // stale subject. Bump main.cpp's mtime to strictly > the subject
            // binary's mtime at whole-second resolution.
            if (std::filesystem::exists(subject_bin())) {
                namespace fs = std::filesystem;
                auto subj = fs::last_write_time(subject_bin());
                auto bumped = subj + std::chrono::seconds(1);
                if (fs::last_write_time(path) <= bumped) {
                    fs::last_write_time(path, bumped);
                }
            }
        }

        static void build() {
            auto r = run_subject(
                cmake_exe(), {"--build", build_dir(), "--target", "subject"});
            if (r.exit_code != 0) {
                throw std::runtime_error(
                    "TempCppProject: cmake build failed (exit " +
                    std::to_string(r.exit_code) + ")\nstdout:\n" +
                    r.stdout_data + "\nstderr:\n" + r.stderr_data);
            }
        }

        SubprocessResult exec(const std::vector<std::string>& args) const {
            return run_subject(subject_bin().string(), args, env_overrides_,
                               env_removes_);
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
