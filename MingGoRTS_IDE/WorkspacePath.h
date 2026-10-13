#pragma once
// WorkspacePath — resolves the IDE workspace root without hard-coded paths.
//
// Resolution order:
//   1. --workspace <dir>  (or --workspace=<dir>) on the command line
//   2. MINGGORTS_WORKSPACE environment variable
//   3. walk up from the executable's directory looking for the repo root
//      (a dir holding both CMakeLists.txt and MingGoRTS_IDE/)
//   4. same walk from the current working directory
//   5. the current working directory

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace MingGoRTSIDE {

inline std::filesystem::path ExecutableDir(const char* argv0) {
    namespace fs = std::filesystem;
    std::error_code ec;
#if defined(_WIN32)
    wchar_t buf[MAX_PATH] = {0};
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) return fs::path(buf).parent_path();
#elif defined(__linux__)
    fs::path self = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) return self.parent_path();
#endif
    if (argv0 != nullptr && *argv0 != '\0') {
        fs::path p = fs::absolute(argv0, ec);
        if (!ec) return p.parent_path();
    }
    return {};
}

inline std::filesystem::path FindRepoRootFrom(std::filesystem::path dir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    while (!dir.empty()) {
        if (fs::is_regular_file(dir / "CMakeLists.txt", ec) &&
            fs::is_directory(dir / "MingGoRTS_IDE", ec)) {
            return dir;
        }
        fs::path parent = dir.parent_path();
        if (parent == dir) break;
        dir = parent;
    }
    return {};
}

inline std::string ResolveWorkspacePath(int argc = 0, char** argv = nullptr) {
    namespace fs = std::filesystem;
    for (int i = 1; argv != nullptr && i < argc; ++i) {
        if (std::strcmp(argv[i], "--workspace") == 0 && i + 1 < argc) {
            return argv[i + 1];
        }
        if (std::strncmp(argv[i], "--workspace=", 12) == 0) {
            return argv[i] + 12;
        }
    }
    if (const char* env = std::getenv("MINGGORTS_WORKSPACE")) {
        if (*env != '\0') return env;
    }
    std::error_code ec;
    fs::path root = FindRepoRootFrom(
        ExecutableDir(argv != nullptr && argc > 0 ? argv[0] : nullptr));
    if (root.empty()) root = FindRepoRootFrom(fs::current_path(ec));
    if (root.empty()) root = fs::current_path(ec);
    return root.string();
}

} // namespace MingGoRTSIDE
