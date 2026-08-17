#include "Lab.h"

#include <Windows.h>
#include <bcrypt.h>
#include <powrprof.h>
#include <winternl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <system_error>

#ifndef STEREO_LAB_VERSION
#define STEREO_LAB_VERSION "unknown"
#endif
#ifndef STEREO_LAB_GIT_SHA
#define STEREO_LAB_GIT_SHA "unknown"
#endif
#ifndef STEREO_LAB_BUILD_UTC
#define STEREO_LAB_BUILD_UTC "unknown"
#endif

namespace stereo_lab {
namespace {

std::uint32_t ParseU32(const std::wstring& text, const wchar_t* option) {
    std::size_t consumed = 0;
    unsigned long value = 0;
    try {
        value = std::stoul(text, &consumed, 10);
    } catch (...) {
        throw std::runtime_error("Invalid integer for " + WideToUtf8(option) + ".");
    }
    if (consumed != text.size() || value == 0 || value > UINT32_MAX) {
        throw std::runtime_error("Out-of-range integer for " + WideToUtf8(option) + ".");
    }
    return static_cast<std::uint32_t>(value);
}

std::vector<std::string> SplitBackends(const std::wstring& value) {
    std::vector<std::string> result;
    std::wstringstream stream(value);
    std::wstring item;
    while (std::getline(stream, item, L',')) {
        std::string backend = WideToUtf8(item);
        std::transform(backend.begin(), backend.end(), backend.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        if (backend != "B0" && backend != "B1" && backend != "B2" && backend != "B3") {
            throw std::runtime_error("Unknown backend '" + backend + "'. Expected B0,B1,B2,B3.");
        }
        if (std::find(result.begin(), result.end(), backend) == result.end()) {
            result.push_back(std::move(backend));
        }
    }
    if (result.empty()) {
        throw std::runtime_error("--backends requires at least one backend.");
    }
    return result;
}

std::string ReadEnvironment(const wchar_t* key) {
    const DWORD required = GetEnvironmentVariableW(key, nullptr, 0);
    if (required == 0) {
        return {};
    }
    std::wstring value(required, L'\0');
    GetEnvironmentVariableW(key, value.data(), required);
    if (!value.empty() && value.back() == L'\0') {
        value.pop_back();
    }
    return WideToUtf8(value);
}

std::string Join(const std::vector<std::string>& values, std::string_view delimiter) {
    std::ostringstream out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) out << delimiter;
        out << values[i];
    }
    return out.str();
}

std::string ActivePowerScheme() {
    GUID* scheme = nullptr;
    if (PowerGetActiveScheme(nullptr, &scheme) != ERROR_SUCCESS || !scheme) return "unavailable";
    wchar_t buffer[64]{};
    const int length = StringFromGUID2(*scheme, buffer, static_cast<int>(std::size(buffer)));
    LocalFree(scheme);
    return length > 1 ? WideToUtf8(std::wstring_view(buffer, static_cast<std::size_t>(length - 1)))
                      : "unavailable";
}

std::string WindowsVersion() {
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    const auto rtlGetVersion = ntdll
        ? reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"))
        : nullptr;
    if (!rtlGetVersion) return "unavailable";
    RTL_OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    if (rtlGetVersion(&version) != 0) return "unavailable";
    std::ostringstream out;
    out << version.dwMajorVersion << '.' << version.dwMinorVersion << '.' << version.dwBuildNumber;
    return out.str();
}

std::string JsonPath(const std::filesystem::path& path) {
    const auto utf8 = path.u8string();
    return JsonEscape(std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size()));
}

}  // namespace

std::string WideToUtf8(std::wstring_view value) {
    if (value.empty()) return {};
    const int required = WideCharToMultiByte(CP_UTF8, 0, value.data(),
                                              static_cast<int>(value.size()), nullptr, 0,
                                              nullptr, nullptr);
    if (required <= 0) throw std::runtime_error("WideCharToMultiByte failed.");
    std::string result(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                        result.data(), required, nullptr, nullptr);
    return result;
}

std::string UtcNow() {
    SYSTEMTIME time{};
    GetSystemTime(&time);
    std::ostringstream out;
    out << std::setfill('0') << std::setw(4) << time.wYear << '-'
        << std::setw(2) << time.wMonth << '-' << std::setw(2) << time.wDay << 'T'
        << std::setw(2) << time.wHour << ':' << std::setw(2) << time.wMinute << ':'
        << std::setw(2) << time.wSecond << '.' << std::setw(3) << time.wMilliseconds << 'Z';
    return out.str();
}

std::string JsonEscape(std::string_view value) {
    std::ostringstream out;
    for (const unsigned char c : value) {
        switch (c) {
        case '\"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 0x20) {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<unsigned int>(c) << std::dec;
            } else {
                out << static_cast<char>(c);
            }
        }
    }
    return out.str();
}

std::filesystem::path ExecutablePath() {
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        throw std::runtime_error("Could not resolve executable path.");
    }
    buffer.resize(length);
    return std::filesystem::path(buffer);
}

Options ParseOptions(int argc, wchar_t** argv, const std::filesystem::path& executable) {
    Options result;
    result.outputRoot = executable.parent_path() / "results";
    result.shaderRoot = executable.parent_path() / "shaders";

    auto valueAfter = [&](int& index, const wchar_t* option) -> std::wstring {
        if (++index >= argc) {
            throw std::runtime_error("Missing value after " + WideToUtf8(option) + ".");
        }
        return argv[index];
    };

    for (int i = 1; i < argc; ++i) {
        const std::wstring argument = argv[i];
        if (argument == L"--help" || argument == L"-h") result.help = true;
        else if (argument == L"--debug") result.debugLayer = true;
        else if (argument == L"--warp") result.warp = true;
        else if (argument == L"--validation-only") result.validationOnly = true;
        else if (argument == L"--smoke") result.smoke = true;
        else if (argument == L"--self-test") result.selfTest = true;
        else if (argument == L"--width") result.widthPerEye = ParseU32(valueAfter(i, L"--width"), L"--width");
        else if (argument == L"--height") result.height = ParseU32(valueAfter(i, L"--height"), L"--height");
        else if (argument == L"--warmup") result.warmupFrames = ParseU32(valueAfter(i, L"--warmup"), L"--warmup");
        else if (argument == L"--frames") result.measuredFrames = ParseU32(valueAfter(i, L"--frames"), L"--frames");
        else if (argument == L"--draws") result.draws = ParseU32(valueAfter(i, L"--draws"), L"--draws");
        else if (argument == L"--validation-draws") result.validationDraws = ParseU32(valueAfter(i, L"--validation-draws"), L"--validation-draws");
        else if (argument == L"--scene") result.scene = WideToUtf8(valueAfter(i, L"--scene"));
        else if (argument == L"--backends") result.backends = SplitBackends(valueAfter(i, L"--backends"));
        else if (argument == L"--output") result.outputRoot = valueAfter(i, L"--output");
        else if (argument == L"--shaders") result.shaderRoot = valueAfter(i, L"--shaders");
        else throw std::runtime_error("Unknown option '" + WideToUtf8(argument) + "'.\n" + Usage());
    }

    std::transform(result.scene.begin(), result.scene.end(), result.scene.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (result.scene != "S0" && result.scene != "S1" && result.scene != "S2" && result.scene != "S3") {
        throw std::runtime_error("--scene must be S0, S1, S2, or S3.");
    }
    if (result.widthPerEye < 64 || result.height < 64) {
        throw std::runtime_error("Per-eye dimensions below 64 are not meaningful for this lab.");
    }
    if (result.widthPerEye > 8192 || result.height > 16384) {
        throw std::runtime_error("Dimensions exceed the D3D11 side-by-side texture limit.");
    }
    if (result.widthPerEye * 2u > 16384) {
        throw std::runtime_error("Side-by-side target width would exceed the D3D11 16384 limit.");
    }
    if (result.draws > 100000 || result.validationDraws > 100000) {
        throw std::runtime_error("Object counts above 100000 are rejected to prevent accidental resource exhaustion.");
    }
    if (result.warmupFrames > 100000 || result.measuredFrames > 100000) {
        throw std::runtime_error("Frame counts above 100000 are rejected to prevent accidental query exhaustion.");
    }
    if (result.smoke) {
        result.widthPerEye = 320;
        result.height = 320;
        result.warmupFrames = 5;
        result.measuredFrames = 30;
        result.draws = 100;
        result.validationDraws = 17; // Odd count catches eye/object instance mapping errors.
    }
    return result;
}

std::string Usage() {
    return
        "StereoCapabilityLab - standard D3D11 stereo submission laboratory\n\n"
        "  --smoke                 Fast 320x320 validation/benchmark run\n"
        "  --self-test             CPU-only evidence/schema tests; no D3D device\n"
        "  --debug                 Require the D3D11 debug layer\n"
        "  --warp                  Use the WARP software adapter\n"
        "  --validation-only       Skip performance measurement\n"
        "  --width N               Per-eye width (combined target is 2*N)\n"
        "  --height N              Per-eye height\n"
        "  --warmup N              Warmup frames per backend\n"
        "  --frames N              Measured frames per backend\n"
        "  --draws N               Synthetic object/draw count\n"
        "  --validation-draws N    Object count used for image comparison\n"
        "  --scene S0|S1|S2|S3     Correctness/submission/vertex/pixel workload\n"
        "  --backends B0,B1,B2,B3  Requested implementations\n"
        "  --output PATH           Parent of unique run directories\n"
        "  --shaders PATH          Directory containing StereoLab.hlsl\n";
}

RunPaths CreateRunPaths(const Options& options) {
    SYSTEMTIME time{};
    GetSystemTime(&time);
    std::ostringstream id;
    id << std::setfill('0') << std::setw(4) << time.wYear << std::setw(2) << time.wMonth
       << std::setw(2) << time.wDay << 'T' << std::setw(2) << time.wHour
       << std::setw(2) << time.wMinute << std::setw(2) << time.wSecond << '.'
       << std::setw(3) << time.wMilliseconds << "Z-p" << GetCurrentProcessId();

    RunPaths paths;
    paths.runId = id.str();
    paths.root = std::filesystem::absolute(options.outputRoot) / paths.runId;
    for (unsigned suffix = 1; std::filesystem::exists(paths.root); ++suffix) {
        paths.root = std::filesystem::absolute(options.outputRoot) /
                     (paths.runId + "-" + std::to_string(suffix));
    }
    std::filesystem::create_directories(paths.root / "images");
    paths.log = paths.root / "run.log";
    paths.events = paths.root / "events.jsonl";
    paths.manifest = paths.root / "run_manifest.json";
    paths.benchmark = paths.root / "benchmark.csv";
    paths.validation = paths.root / "validation.csv";
    paths.images = paths.root / "images";
    return paths;
}

Logger::Logger(const RunPaths& paths)
    : log_(paths.log, std::ios::out | std::ios::app),
      events_(paths.events, std::ios::out | std::ios::app) {
    if (!log_ || !events_) throw std::runtime_error("Could not create run logs.");
}

Logger::~Logger() = default;

void Logger::Event(std::string_view level, std::string_view phase, std::string_view message) {
    const std::lock_guard lock(mutex_);
    const auto utc = UtcNow();
    ++sequence_;
    log_ << utc << " [" << level << "] [" << phase << "] " << message << '\n';
    events_ << "{\"utc\":\"" << utc << "\",\"sequence\":" << sequence_
            << ",\"level\":\"" << JsonEscape(level) << "\",\"phase\":\""
            << JsonEscape(phase) << "\",\"message\":\"" << JsonEscape(message)
            << "\"}\n";
    log_.flush();
    events_.flush();
    std::cout << '[' << level << "] " << message << '\n';
}

void Logger::Info(std::string_view phase, std::string_view message) { Event("info", phase, message); }
void Logger::Warn(std::string_view phase, std::string_view message) { Event("warning", phase, message); }
void Logger::Error(std::string_view phase, std::string_view message) { Event("error", phase, message); }

void WriteTextFile(const std::filesystem::path& path, std::string_view text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) throw std::runtime_error("Could not write " + path.string());
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    file.flush();
    if (!file) throw std::runtime_error("Write failed for " + path.string());
}

void WriteStatus(const RunPaths& paths, std::string_view state, std::string_view phase,
                 std::string_view detail, int exitCode) {
    std::ostringstream json;
    json << "{\n  \"schema\": 1,\n  \"run_id\": \"" << JsonEscape(paths.runId)
         << "\",\n  \"updated_utc\": \"" << UtcNow() << "\",\n  \"state\": \""
         << JsonEscape(state) << "\",\n  \"phase\": \"" << JsonEscape(phase)
         << "\",\n  \"detail\": \"" << JsonEscape(detail) << "\",\n  \"exit_code\": ";
    if (exitCode < 0) json << "null"; else json << exitCode;
    json << "\n}\n";
    WriteTextFile(paths.root / "status.json", json.str());

    std::ofstream history(paths.root / "lifecycle.jsonl", std::ios::app);
    history << "{\"utc\":\"" << UtcNow() << "\",\"state\":\"" << JsonEscape(state)
            << "\",\"phase\":\"" << JsonEscape(phase) << "\",\"detail\":\""
            << JsonEscape(detail) << "\",\"exit_code\":";
    if (exitCode < 0) history << "null"; else history << exitCode;
    history << "}\n";
}

std::string FileSha256(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return "unavailable";

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectSize = 0;
    DWORD hashSize = 0;
    DWORD bytes = 0;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
        return "unavailable";
    }
    auto closeAlgorithm = [&] { BCryptCloseAlgorithmProvider(algorithm, 0); };
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                          reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &bytes, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
                          reinterpret_cast<PUCHAR>(&hashSize), sizeof(hashSize), &bytes, 0) < 0) {
        closeAlgorithm();
        return "unavailable";
    }
    std::vector<UCHAR> object(objectSize);
    std::vector<UCHAR> digest(hashSize);
    if (BCryptCreateHash(algorithm, &hash, object.data(), objectSize, nullptr, 0, 0) < 0) {
        closeAlgorithm();
        return "unavailable";
    }
    std::array<char, 64 * 1024> buffer{};
    while (file) {
        file.read(buffer.data(), buffer.size());
        const auto count = file.gcount();
        if (count > 0 && BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()),
                                       static_cast<ULONG>(count), 0) < 0) {
            BCryptDestroyHash(hash);
            closeAlgorithm();
            return "unavailable";
        }
    }
    if (BCryptFinishHash(hash, digest.data(), hashSize, 0) < 0) {
        BCryptDestroyHash(hash);
        closeAlgorithm();
        return "unavailable";
    }
    BCryptDestroyHash(hash);
    closeAlgorithm();
    std::ostringstream out;
    for (const auto byte : digest) out << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(byte);
    return out.str();
}

void WriteInitialEvidence(const Options& options, const RunPaths& paths, int argc, wchar_t** argv) {
    std::ostringstream command;
    for (int i = 0; i < argc; ++i) {
        if (i) command << ' ';
        command << '"' << WideToUtf8(argv[i]) << '"';
    }
    command << '\n';
    WriteTextFile(paths.root / "command_line.txt", command.str());

    const auto executable = ExecutablePath();
    const std::string binaryHash = FileSha256(executable);
    std::ostringstream manifest;
    manifest << "{\n"
             << "  \"schema\": 1,\n"
             << "  \"run_id\": \"" << JsonEscape(paths.runId) << "\",\n"
             << "  \"created_utc\": \"" << UtcNow() << "\",\n"
             << "  \"version\": \"" << STEREO_LAB_VERSION << "\",\n"
             << "  \"source_revision\": \"" << STEREO_LAB_GIT_SHA << "\",\n"
             << "  \"build_utc\": \"" << STEREO_LAB_BUILD_UTC << "\",\n"
             << "  \"binary\": \"" << JsonPath(executable) << "\",\n"
             << "  \"binary_sha256\": \"" << binaryHash << "\",\n"
             << "  \"pid\": " << GetCurrentProcessId() << ",\n"
             << "  \"output\": \"" << JsonPath(paths.root) << "\"\n"
             << "}\n";
    WriteTextFile(paths.manifest, manifest.str());

    std::ostringstream config;
    config << "{\n  \"schema\": 1,\n"
           << "  \"width_per_eye\": " << options.widthPerEye << ",\n"
           << "  \"height\": " << options.height << ",\n"
           << "  \"combined_width\": " << options.widthPerEye * 2u << ",\n"
           << "  \"warmup_frames\": " << options.warmupFrames << ",\n"
           << "  \"measured_frames\": " << options.measuredFrames << ",\n"
           << "  \"draws\": " << options.draws << ",\n"
           << "  \"validation_draws\": " << options.validationDraws << ",\n"
           << "  \"scene\": \"" << JsonEscape(options.scene) << "\",\n"
           << "  \"backends\": \"" << JsonEscape(Join(options.backends, ",")) << "\",\n"
           << "  \"debug_layer\": " << (options.debugLayer ? "true" : "false") << ",\n"
           << "  \"warp\": " << (options.warp ? "true" : "false") << ",\n"
           << "  \"validation_only\": " << (options.validationOnly ? "true" : "false") << ",\n"
           << "  \"smoke\": " << (options.smoke ? "true" : "false") << ",\n"
           << "  \"shader_root\": \"" << JsonPath(options.shaderRoot) << "\",\n"
           << "  \"shader_sha256\": \"" << FileSha256(options.shaderRoot / "StereoLab.hlsl") << "\"\n}\n";
    WriteTextFile(paths.root / "config.json", config.str());

    SYSTEM_INFO system{};
    GetNativeSystemInfo(&system);
    DWORD_PTR processAffinity = 0;
    DWORD_PTR systemAffinity = 0;
    GetProcessAffinityMask(GetCurrentProcess(), &processAffinity, &systemAffinity);
    LARGE_INTEGER qpcFrequency{};
    QueryPerformanceFrequency(&qpcFrequency);
    std::ostringstream processAffinityText;
    processAffinityText << "0x" << std::hex << processAffinity;
    std::ostringstream systemAffinityText;
    systemAffinityText << "0x" << std::hex << systemAffinity;
    std::ostringstream environment;
    environment << "{\n  \"schema\": 1,\n"
                << "  \"computer_name\": \"" << JsonEscape(ReadEnvironment(L"COMPUTERNAME")) << "\",\n"
                << "  \"user_domain\": \"" << JsonEscape(ReadEnvironment(L"USERDOMAIN")) << "\",\n"
                << "  \"processor_identifier\": \"" << JsonEscape(ReadEnvironment(L"PROCESSOR_IDENTIFIER")) << "\",\n"
                << "  \"logical_processors\": " << system.dwNumberOfProcessors << ",\n"
                << "  \"process_architecture\": " << system.wProcessorArchitecture << ",\n"
                << "  \"windows_version\": \"" << WindowsVersion() << "\",\n"
                << "  \"active_power_scheme_guid\": \"" << ActivePowerScheme() << "\",\n"
                << "  \"process_priority_class\": " << GetPriorityClass(GetCurrentProcess()) << ",\n"
                << "  \"thread_priority\": " << GetThreadPriority(GetCurrentThread()) << ",\n"
                << "  \"process_affinity_mask\": \"" << processAffinityText.str() << "\",\n"
                << "  \"system_affinity_mask\": \"" << systemAffinityText.str() << "\",\n"
                << "  \"qpc_frequency_hz\": " << qpcFrequency.QuadPart << "\n}\n";
    WriteTextFile(paths.root / "environment.json", environment.str());
}

int RunSelfTests(const RunPaths& paths, Logger& logger) {
    logger.Info("self_test", "Running CPU-only evidence and escaping checks.");
    bool passed = true;
    std::ostringstream report;
    report << "{\n  \"schema\": 1,\n  \"tests\": [\n";
    auto check = [&](std::string_view name, bool condition, bool last) {
        passed = passed && condition;
        report << "    {\"name\":\"" << JsonEscape(name) << "\",\"passed\":"
               << (condition ? "true" : "false") << "}" << (last ? "\n" : ",\n");
    };
    check("json_escape_quotes", JsonEscape("a\"b") == "a\\\"b", false);
    check("json_escape_control", JsonEscape("a\nb") == "a\\nb", false);
    check("run_directory_exists", std::filesystem::is_directory(paths.root), false);
    check("manifest_exists", std::filesystem::is_regular_file(paths.manifest), true);
    report << "  ],\n  \"passed\": " << (passed ? "true" : "false") << "\n}\n";
    WriteTextFile(paths.root / "self_test.json", report.str());
    logger.Event(passed ? "info" : "error", "self_test", passed ? "All self-tests passed." : "One or more self-tests failed.");
    return passed ? 0 : 2;
}

}  // namespace stereo_lab
