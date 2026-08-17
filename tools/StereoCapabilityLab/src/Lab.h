#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace stereo_lab {

struct Options {
    std::uint32_t widthPerEye = 1280;
    std::uint32_t height = 1280;
    std::uint32_t warmupFrames = 300;
    std::uint32_t measuredFrames = 2000;
    std::uint32_t draws = 1000;
    std::uint32_t validationDraws = 64;
    std::string scene = "S1";
    std::vector<std::string> backends{"B0", "B1", "B2", "B3"};
    std::filesystem::path outputRoot;
    std::filesystem::path shaderRoot;
    bool debugLayer = false;
    bool warp = false;
    bool validationOnly = false;
    bool smoke = false;
    bool selfTest = false;
    bool help = false;
};

struct RunPaths {
    std::string runId;
    std::filesystem::path root;
    std::filesystem::path log;
    std::filesystem::path events;
    std::filesystem::path manifest;
    std::filesystem::path benchmark;
    std::filesystem::path validation;
    std::filesystem::path images;
};

class Logger {
public:
    explicit Logger(const RunPaths& paths);
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void Event(std::string_view level, std::string_view phase, std::string_view message);
    void Info(std::string_view phase, std::string_view message);
    void Warn(std::string_view phase, std::string_view message);
    void Error(std::string_view phase, std::string_view message);

private:
    std::mutex mutex_;
    std::ofstream log_;
    std::ofstream events_;
    std::uint64_t sequence_ = 0;
};

Options ParseOptions(int argc, wchar_t** argv, const std::filesystem::path& executable);
std::string Usage();
RunPaths CreateRunPaths(const Options& options);
void WriteTextFile(const std::filesystem::path& path, std::string_view text);
void WriteStatus(const RunPaths& paths, std::string_view state, std::string_view phase,
                 std::string_view detail, int exitCode = -1);
void WriteInitialEvidence(const Options& options, const RunPaths& paths, int argc, wchar_t** argv);
std::string JsonEscape(std::string_view value);
std::string UtcNow();
std::string WideToUtf8(std::wstring_view value);
std::string FileSha256(const std::filesystem::path& path);
std::filesystem::path ExecutablePath();

class D3D11Lab {
public:
    D3D11Lab(const Options& options, const RunPaths& paths, Logger& logger);
    ~D3D11Lab();
    D3D11Lab(const D3D11Lab&) = delete;
    D3D11Lab& operator=(const D3D11Lab&) = delete;

    int Execute();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

int RunSelfTests(const RunPaths& paths, Logger& logger);

}  // namespace stereo_lab
