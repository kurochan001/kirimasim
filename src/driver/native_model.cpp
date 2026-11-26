// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "native_model.h"
#include "kirimasim/sir/frame_layout.h"
#include "native_headers.h"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <dlfcn.h>
#include <fcntl.h>
#include <fstream>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

#include "process_environment.h"

namespace kirimasim::driver {
namespace fs = std::filesystem;
namespace {
class TemporaryDirectory {
public:
    explicit TemporaryDirectory(const fs::path& parent) {
        std::string name = (parent / ".build-XXXXXX").string();
        if (mkdtemp(name.data())) path = name;
    }
    ~TemporaryDirectory() {
        if (!path.empty()) {
            std::error_code error;
            fs::remove_all(path, error);
        }
    }
    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    fs::path path;
};
struct Child {
    int error = 0;
    int status = 0;
    std::string output;
};
// Separate arguments reach the compiler even when the cache path contains spaces.
Child invoke(std::vector<std::string> args) {
    int fds[2];
    if (pipe(fds) != 0) return {errno, 0, {}};
    posix_spawn_file_actions_t actions;
    int err = posix_spawn_file_actions_init(&actions);
    const bool initialized = err == 0;
    if (!err) err = posix_spawn_file_actions_addclose(&actions, fds[0]);
    if (!err) err = posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
    if (!err) err = posix_spawn_file_actions_adddup2(&actions, fds[1], STDERR_FILENO);
    if (!err) err = posix_spawn_file_actions_addclose(&actions, fds[1]);
    pid_t pid{};
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (auto& s : args)
        argv.push_back(s.data());
    argv.push_back(nullptr);
    if (!err) err = posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), childEnvironment());
    if (initialized) posix_spawn_file_actions_destroy(&actions);
    close(fds[1]);
    std::string log;
    char buffer[4096];
    while (!err) {
        const auto count = read(fds[0], buffer, sizeof(buffer));
        if (count == 0) break;
        if (count < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (log.size() < 65536)
            log.append(buffer, std::min<std::size_t>(count, 65536 - log.size()));
    }
    close(fds[0]);
    int status = 0;
    if (!err) {
        while (waitpid(pid, &status, 0) < 0) {
            if (errno != EINTR) {
                err = errno;
                break;
            }
        }
    }
    return {err, WIFEXITED(status) ? WEXITSTATUS(status) : 1, std::move(log)};
}
bool writeFile(const fs::path& path, std::string_view text) {
    std::ofstream out(path, std::ios::binary);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    out.close();
    return static_cast<bool>(out);
}
std::string hex(const analysis::Digest& bytes) {
    constexpr std::string_view digits = "0123456789abcdef";
    std::string s;
    for (auto byte : bytes) {
        s += digits[byte >> 4];
        s += digits[byte & 15];
    }
    return s;
}
bool privateDirectory(const fs::path& path) {
    struct stat st{};
    return lstat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && st.st_uid == geteuid() &&
           (st.st_mode & (S_IWGRP | S_IWOTH)) == 0;
}
NativeBuild openModel(const fs::path& file, const sir::VerifiedDesign& verified,
                      const codegen::Model& model, const analysis::Digest& digest) {
    struct stat st{};
    if (lstat(file.c_str(), &st) != 0 || !S_ISREG(st.st_mode) || st.st_uid != geteuid() ||
        (st.st_mode & (S_IWGRP | S_IWOTH)) != 0)
        return {{}, "cannot open cached native module as an owned regular file"};
    int flags = RTLD_NOW | RTLD_LOCAL;
#ifdef __APPLE__
    // A compiler runtime can register thread-exit callbacks when dyld loads
    // it. The CLI's simulation thread exits after the module handle closes.
    flags |= RTLD_NODELETE;
#endif
    NativeModule::Library library(dlopen(file.c_str(), flags), dlclose);
    if (!library) return {{}, dlerror()};
    auto query =
        reinterpret_cast<kirima_module_query_fn>(dlsym(library.get(), "kirima_get_module_table"));
    if (!query) return {{}, "native module has no model query"};
    const auto* api = kirima_get_kernel_api();
    const auto* descriptor = query(api->abi_major, api->abi_minor);
    kirima_model_identity identity{1, KIRIMA_MODEL_LAYOUT_64_V1, {}};
    std::ranges::copy(digest, identity.design_sha256);
    std::vector<kirima_model_layout> layouts;
    layouts.reserve(model.bodies.size());
    std::vector<std::vector<kirima_model_field>> fields(model.bodies.size());
    for (auto id : model.bodies) {
        const auto& frame = verified.design().process(id).frame;
        if (!std::ranges::all_of(frame, codegen::supportsLoopField))
            return {{}, "unrecognized native frame layout"};
        auto& rows = fields[layouts.size()];
        const auto collect = [](void* raw, const sir::FrameVar&, const sir::FrameFieldLayout& f) {
            static_cast<std::vector<kirima_model_field>*>(raw)->push_back(
                {f.offset.value_or(UINT64_MAX), f.logicalOffset, f.size, f.elements, f.id.index(),
                 f.chunksPerElement});
        };
        const auto layout = sir::framePayloadLayout(frame, collect, &rows);
        if (!layout) return {{}, "unrecognized native frame layout"};
        layouts.push_back({KIRIMA_MODEL_PROCESS, id.index(),
                           static_cast<std::uint32_t>(rows.size()), sir::kFrameCellAlignment,
                           layout->bytes, layout->logicalBytes, rows.data()});
    }
    if (kirima_validate_module(api, descriptor, &identity, layouts.data(),
                               static_cast<std::uint32_t>(layouts.size())) != KIRIMA_MODULE_OK)
        return {{}, "native module identity or layout mismatch"};
    return {NativeModule{std::move(library), descriptor}, {}};
}
}  // namespace
fs::path defaultNativeCache() {
    if (const char* xdg = std::getenv("XDG_CACHE_HOME"); xdg && *xdg)
        return fs::path(xdg) / "kirimasim";
    if (const char* home = std::getenv("HOME"); home && *home)
        return fs::path(home) / ".cache/kirimasim";
    return {};
}
NativeBuild loadNativeModel(const sir::VerifiedDesign& verified, const codegen::Model& model,
                            const analysis::Digest& digest, const std::string& cxx,
                            const fs::path& cache) {
    const auto version = invoke({cxx, "--version"});
    if (version.error || version.status)
        return {{}, "cannot query C++ compiler " + cxx, version.error == ENOENT};
    if (cache.empty()) return {{}, "no native cache directory"};
    std::error_code ec;
    fs::create_directories(cache, ec);
    if (ec || !privateDirectory(cache)) return {{}, "cannot use native cache " + cache.string()};
    // Versioned root avoids collision with other kirimasim cache users.
    const auto root = cache / "native-v1";
    fs::create_directory(root, ec);
    if (ec || !privateDirectory(root)) return {{}, "cannot use native module directory"};
    struct utsname host{};
    if (uname(&host) < 0) return {{}, "cannot determine native build target"};
    const std::vector<std::string> flags{
        "-std=c++20",        "-O2",           "-fPIC", "-shared", "-fno-exceptions", "-fno-rtti",
        "-ffp-contract=off", "-fno-fast-math"};
    std::string identity;
    const auto append = [&](std::string_view s) {
        identity += std::to_string(s.size()) + "#";
        identity += s;
    };
    for (const auto& flag : flags)
        append(flag);
    append(cxx);
    append(version.output);
    append(host.sysname);
    append(host.machine);
    append(std::to_string(KIRIMA_ABI_MAJOR));
    append(std::to_string(KIRIMA_ABI_MINOR));
    append(kPublicHeadersDigest);
    append(model.source);
    const auto key = hex(analysis::contentDigest(identity));
    const auto final = root / key;
    if (fs::exists(final / "model.so", ec))
        return openModel(final / "model.so", verified, model, digest);
    TemporaryDirectory work(root);
    if (work.path.empty()) return {{}, "cannot create native build directory"};
    const auto headers = work.path / "include/kirimasim/kernel";
    fs::create_directories(headers, ec);
    if (ec) return {{}, ec.message()};
    for (const auto& h : kNativeHeaders)
        if (!writeFile(headers / h.name, h.text)) return {{}, "cannot write native kernel headers"};
    if (!writeFile(work.path / "model.cpp", model.source))
        return {{}, "cannot write generated C++"};
    std::vector<std::string> args{cxx};
    args.insert(args.end(), flags.begin(), flags.end());
    args.insert(args.end(),
                {"-I", (work.path / "include").string(), (work.path / "model.cpp").string(), "-o",
                 (work.path / "model.so").string()});
    const auto compiled = invoke(std::move(args));
    if (compiled.error || compiled.status)
        return {{}, "native compiler failed: " + compiled.output};
    // Validate the completed temporary file before making this key visible.
    auto opened = openModel(work.path / "model.so", verified, model, digest);
    if (!opened.module) return opened;
    fs::remove_all(work.path / "include", ec);
    fs::rename(work.path, final, ec);
    if (ec) {
        if (fs::exists(final / "model.so"))
            return openModel(final / "model.so", verified, model, digest);
        return {{}, ec.message()};
    }
    work.path.clear();
    return opened;
}
std::string IslandModules::prepare(const sir::VerifiedDesign& design,
                                   const analysis::TableData& table, const NativeOptions& options,
                                   std::uint64_t maxSteps, bool bounds, bool vpi) {
    generated_ = codegen::emitModel(design, table, maxSteps, bounds,
                                    {.includeOrdinary = !vpi && !design.design().foreignReflection,
                                     .allowFrameLoops = !vpi,
                                     .allowScalarFunctions = !vpi,
                                     .approximateTwoState = options.approximateTwoState,
                                     .reportEmissions = options.reportEmissions});
    if (generated_.bodies.empty()) return {};
    const auto load = [&](const auto& model) {
        return loadNativeModel(design, model, table.key.design, options.compiler, options.cache);
    };
    auto regular = load(generated_);
    if (!regular.module) {
        if (options.tier == Tier::Auto && !options.check && regular.compilerMissing) return {};
        return regular.error;
    }
    four_ = std::move(regular.module);
    if (vpi || options.approximateTwoState) return {};
    known_ = codegen::emitKnownModel(design, table, maxSteps, bounds);
    if (known_.model.bodies.empty()) return {};
    auto alternate = load(known_.model);
    if (!alternate.module) return alternate.error;
    two_ = std::move(alternate.module);
    return {};
}
void IslandModules::bind(vm::RunOptions& run) const {
    if (!four_) return;
    if (two_) {
        run.knownIslands.reserve(known_.guards.size());
        for (const auto& guard : known_.guards)
            run.knownIslands.push_back({guard.island, guard.inputs});
    }
    std::size_t regular = 0, alternate = 0;
    for (auto& member : run.islands) {
        if (regular < generated_.bodies.size() && generated_.bodies[regular] == member.process)
            member.native = four_->descriptor->bodies[regular++].resume;
        if (two_ && alternate < known_.model.bodies.size() &&
            known_.model.bodies[alternate] == member.process)
            member.nativeKnown = two_->descriptor->bodies[alternate++].resume;
    }
    // Ordinary processes follow the island members in the generated descriptor.
    // Their own VM frames retain the original wait and notification order.
    if (run.tierCheck) run.checkpointBodies = generated_.checkpointBodies;
    for (const auto& [process, slot] : generated_.countedLoopSlots) {
        if (run.countedLoopSlots.size() <= process.index())
            run.countedLoopSlots.resize(std::size_t{process.index()} + 1, sir::kNone);
        run.countedLoopSlots[process.index()] = slot;
    }
    for (; regular < generated_.bodies.size(); ++regular) {
        const auto process = generated_.bodies[regular].index();
        if (run.combinationalBodies.size() <= process)
            run.combinationalBodies.resize(std::size_t{process} + 1);
        run.combinationalBodies[process] = four_->descriptor->bodies[regular].resume;
    }
}
}  // namespace kirimasim::driver
