#include "Lab.h"

#include <Windows.h>
#include <d3d11.h>
#include <d3d11_3.h>
#include <d3d11sdklayers.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <thread>

#ifndef STEREO_LAB_GIT_SHA
#define STEREO_LAB_GIT_SHA "unknown"
#endif

namespace stereo_lab {
namespace {

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using Clock = std::chrono::steady_clock;

void ThrowIfFailed(HRESULT result, std::string_view operation) {
    if (SUCCEEDED(result)) return;
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "0x%08X", static_cast<unsigned>(result));
    throw std::runtime_error(std::string(operation) + " failed (" + buffer + ").");
}

double Microseconds(Clock::time_point begin, Clock::time_point end) {
    return std::chrono::duration<double, std::micro>(end - begin).count();
}

std::string FeatureLevelName(D3D_FEATURE_LEVEL level) {
    switch (level) {
    case D3D_FEATURE_LEVEL_11_1: return "11_1";
    case D3D_FEATURE_LEVEL_11_0: return "11_0";
    case D3D_FEATURE_LEVEL_10_1: return "10_1";
    case D3D_FEATURE_LEVEL_10_0: return "10_0";
    default: return "unknown";
    }
}

std::string BackendDescription(std::string_view backend) {
    if (backend == "B0") return "two-draw native reference";
    if (backend == "B1") return "shared CPU packet, two native draws";
    if (backend == "B2") return "eye-expanded instancing with side-by-side clip packing";
    if (backend == "B3") return "geometry-shader replication with viewport routing";
    return "unknown";
}

std::string CsvEscape(std::string_view value) {
    if (value.find_first_of(",\"\r\n") == std::string_view::npos) return std::string(value);
    std::string result = "\"";
    for (char c : value) {
        if (c == '\"') result += '\"';
        result += c;
    }
    result += '\"';
    return result;
}

struct Vertex {
    XMFLOAT3 position;
    XMFLOAT3 color;
};

struct alignas(16) FrameConstants {
    XMFLOAT4X4 viewProjection[2];
    XMFLOAT4 workload;
};

struct alignas(16) ObjectConstants {
    XMFLOAT4X4 model;
    XMFLOAT4 color;
};

struct alignas(16) EyeConstants {
    std::uint32_t eye = 0;
    std::uint32_t padding[3]{};
};

static_assert(sizeof(FrameConstants) == 144, "Frame constant-buffer layout must match HLSL.");
static_assert(sizeof(ObjectConstants) == 80, "Structured-object layout must match HLSL.");
static_assert(sizeof(EyeConstants) == 16, "Eye constant-buffer layout must match HLSL.");

struct FrameRecord {
    std::uint64_t frame = 0;
    std::string backend;
    double cpuPrepareUs = 0.0;
    double cpuSubmitUs = 0.0;
    double cpuTotalUs = 0.0;
    double gpuUs = 0.0;
    bool queryValid = false;
};

struct ValidationRecord {
    std::string backend;
    bool passed = false;
    std::uint64_t mismatchPixels = 0;
    std::uint8_t maxChannelError = 0;
    double mismatchPercent = 0.0;
    std::uint64_t leftMismatchPixels = 0;
    std::uint64_t rightMismatchPixels = 0;
    std::uint64_t seamMismatchPixels = 0;
    std::uint64_t outerEdgeMismatchPixels = 0;
    double depthMaxError = 0.0;
    std::uint64_t depthMismatchPixels = 0;
};

struct ImageData {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba;
    std::vector<float> depth;
};

struct GpuQueries {
    ComPtr<ID3D11Query> disjoint;
    ComPtr<ID3D11Query> begin;
    ComPtr<ID3D11Query> end;
};

struct Stats {
    double mean = 0.0;
    double median = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
};

Stats CalculateStats(std::vector<double> values) {
    Stats result;
    if (values.empty()) return result;
    result.mean = std::accumulate(values.begin(), values.end(), 0.0) /
                  static_cast<double>(values.size());
    std::sort(values.begin(), values.end());
    const auto percentile = [&](double p) {
        const double index = p * static_cast<double>(values.size() - 1);
        const auto low = static_cast<std::size_t>(std::floor(index));
        const auto high = static_cast<std::size_t>(std::ceil(index));
        if (low == high) return values[low];
        return values[low] + (values[high] - values[low]) * (index - low);
    };
    result.median = percentile(0.50);
    result.p95 = percentile(0.95);
    result.p99 = percentile(0.99);
    return result;
}

std::string DriverVersionString(LARGE_INTEGER version) {
    const auto high = static_cast<std::uint32_t>(version.HighPart);
    const auto low = static_cast<std::uint32_t>(version.LowPart);
    std::ostringstream out;
    out << HIWORD(high) << '.' << LOWORD(high) << '.' << HIWORD(low) << '.' << LOWORD(low);
    return out.str();
}

}  // namespace

class D3D11Lab::Impl {
public:
    Impl(const Options& options, const RunPaths& paths, Logger& logger)
        : options_(options), paths_(paths), logger_(logger) {}

    int Execute();

private:
    void CreateDevice();
    void WriteCapabilities() const;
    void CreateTargets();
    void CreatePipeline();
    void CreateScene(std::uint32_t count);
    ComPtr<ID3DBlob> CompileShader(const char* entry, const char* profile);
    void ConfigureCommonPipeline();
    void RenderBackend(std::string_view backend, std::uint32_t draws, GpuQueries* queries);
    void DrawReference(std::string_view backend, std::uint32_t draws);
    void DrawInstanced(std::uint32_t draws);
    void DrawGeometryStereo(std::uint32_t draws);
    void UpdateObjectBuffer(const ObjectConstants& object);
    void RunValidation();
    ValidationRecord Compare(const std::string& backend, const ImageData& reference,
                             const ImageData& candidate) const;
    ImageData CaptureTarget();
    void SavePpm(const std::filesystem::path& path, const ImageData& image) const;
    void SavePgmDepth(const std::filesystem::path& path, const ImageData& image) const;
    void SaveDiff(const std::filesystem::path& path, const ImageData& reference,
                  const ImageData& candidate) const;
    void RunBenchmark();
    GpuQueries CreateGpuQueries() const;
    void ResolveGpuQueries(const std::vector<GpuQueries>& queries,
                           std::vector<FrameRecord>& records);
    void WriteBenchmarkCsv() const;
    void WriteValidationCsv() const;
    void WriteSummary() const;
    void DrainDebugLayer();
    std::uint32_t PixelIterations() const;
    std::uint32_t VertexIterations() const;

    Options options_;
    RunPaths paths_;
    Logger& logger_;

    ComPtr<IDXGIAdapter1> adapter_;
    DXGI_ADAPTER_DESC1 adapterDescription_{};
    std::string adapterName_;
    std::string adapterLuid_;
    std::string driverVersion_ = "unavailable";
    std::string binaryHash_ = "unavailable";
    std::string shaderHash_ = "unavailable";
    D3D_FEATURE_LEVEL featureLevel_ = D3D_FEATURE_LEVEL_11_0;
    UINT deviceFlags_ = 0;
    D3D11_FEATURE_DATA_THREADING threading_{};
    bool threadingAvailable_ = false;
    D3D11_FEATURE_DATA_D3D11_OPTIONS3 options3_{};
    bool options3Available_ = false;

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<ID3D11InfoQueue> infoQueue_;
    ComPtr<ID3D11Texture2D> color_;
    ComPtr<ID3D11RenderTargetView> colorRtv_;
    ComPtr<ID3D11Texture2D> colorStaging_;
    ComPtr<ID3D11Texture2D> depth_;
    ComPtr<ID3D11DepthStencilView> depthDsv_;
    ComPtr<ID3D11Texture2D> depthStaging_;

    ComPtr<ID3D11VertexShader> referenceVs_;
    ComPtr<ID3D11VertexShader> instancedVs_;
    ComPtr<ID3D11VertexShader> worldVs_;
    ComPtr<ID3D11GeometryShader> stereoGs_;
    ComPtr<ID3D11PixelShader> pixelShader_;
    ComPtr<ID3D11InputLayout> inputLayout_;
    ComPtr<ID3D11Buffer> vertexBuffer_;
    ComPtr<ID3D11Buffer> indexBuffer_;
    ComPtr<ID3D11Buffer> frameBuffer_;
    ComPtr<ID3D11Buffer> objectBuffer_;
    std::array<ComPtr<ID3D11Buffer>, 2> eyeBuffers_;
    ComPtr<ID3D11Buffer> structuredObjects_;
    ComPtr<ID3D11ShaderResourceView> structuredObjectsSrv_;
    ComPtr<ID3D11RasterizerState> rasterizer_;
    ComPtr<ID3D11DepthStencilState> depthState_;

    std::vector<ObjectConstants> objects_;
    std::vector<FrameRecord> frameRecords_;
    std::vector<ValidationRecord> validationRecords_;
    std::uint64_t debugWarnings_ = 0;
    std::uint64_t debugErrors_ = 0;
};

D3D11Lab::D3D11Lab(const Options& options, const RunPaths& paths, Logger& logger)
    : impl_(std::make_unique<Impl>(options, paths, logger)) {}
D3D11Lab::~D3D11Lab() = default;
int D3D11Lab::Execute() { return impl_->Execute(); }

int D3D11Lab::Impl::Execute() {
    shaderHash_ = FileSha256(options_.shaderRoot / "StereoLab.hlsl");
    WriteStatus(paths_, "running", "device", "Creating the D3D11 device and recording capabilities.");
    logger_.Info("device", "Creating a standard D3D11 device.");
    CreateDevice();
    WriteCapabilities();

    WriteStatus(paths_, "running", "pipeline", "Creating offscreen targets, shaders, and test scene.");
    CreateTargets();
    CreatePipeline();
    CreateScene(std::max(options_.draws, options_.validationDraws));

    WriteStatus(paths_, "running", "validation", "Comparing requested backends against B0.");
    RunValidation();
    if (!options_.validationOnly) {
        WriteStatus(paths_, "running", "benchmark", "Collecting CPU and delayed GPU timings.");
        RunBenchmark();
    } else {
        logger_.Info("benchmark", "Performance phase skipped by --validation-only.");
        WriteBenchmarkCsv();
    }

    DrainDebugLayer();
    WriteValidationCsv();
    WriteSummary();

    const bool validationFailed = std::any_of(validationRecords_.begin(), validationRecords_.end(),
                                              [](const auto& record) { return !record.passed; });
    const bool debugFailed = debugErrors_ != 0;
    return (validationFailed || debugFailed) ? 3 : 0;
}

void D3D11Lab::Impl::CreateDevice() {
    ComPtr<IDXGIFactory1> factory;
    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "CreateDXGIFactory1");

    if (!options_.warp) {
        ComPtr<IDXGIFactory6> factory6;
        if (SUCCEEDED(factory.As(&factory6))) {
            for (UINT index = 0;; ++index) {
                ComPtr<IDXGIAdapter1> candidate;
                if (factory6->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                         IID_PPV_ARGS(&candidate)) == DXGI_ERROR_NOT_FOUND) break;
                DXGI_ADAPTER_DESC1 description{};
                candidate->GetDesc1(&description);
                if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
                    adapter_ = std::move(candidate);
                    break;
                }
            }
        }
        if (!adapter_) {
            for (UINT index = 0;; ++index) {
                ComPtr<IDXGIAdapter1> candidate;
                if (factory->EnumAdapters1(index, &candidate) == DXGI_ERROR_NOT_FOUND) break;
                DXGI_ADAPTER_DESC1 description{};
                candidate->GetDesc1(&description);
                if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
                    adapter_ = std::move(candidate);
                    break;
                }
            }
        }
        if (!adapter_) throw std::runtime_error("No hardware DXGI adapter was found.");
        ThrowIfFailed(adapter_->GetDesc1(&adapterDescription_), "IDXGIAdapter1::GetDesc1");
    }

    deviceFlags_ = D3D11_CREATE_DEVICE_SINGLETHREADED;
    if (options_.debugLayer) deviceFlags_ |= D3D11_CREATE_DEVICE_DEBUG;
    const std::array<D3D_FEATURE_LEVEL, 2> requested{
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    const D3D_DRIVER_TYPE driverType = options_.warp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_UNKNOWN;
    HRESULT result = D3D11CreateDevice(options_.warp ? nullptr : adapter_.Get(), driverType, nullptr,
                                       deviceFlags_, requested.data(), static_cast<UINT>(requested.size()),
                                       D3D11_SDK_VERSION, &device_, &featureLevel_, &context_);
    if (result == E_INVALIDARG) {
        const D3D_FEATURE_LEVEL fallback = D3D_FEATURE_LEVEL_11_0;
        result = D3D11CreateDevice(options_.warp ? nullptr : adapter_.Get(), driverType, nullptr,
                                   deviceFlags_, &fallback, 1, D3D11_SDK_VERSION,
                                   &device_, &featureLevel_, &context_);
    }
    if (options_.debugLayer && result == DXGI_ERROR_SDK_COMPONENT_MISSING) {
        throw std::runtime_error("The --debug run requires Graphics Tools / the D3D11 debug layer, but it is not installed.");
    }
    ThrowIfFailed(result, "D3D11CreateDevice");

    if (options_.warp) {
        ComPtr<IDXGIDevice> dxgiDevice;
        ThrowIfFailed(device_.As(&dxgiDevice), "Query IDXGIDevice");
        ComPtr<IDXGIAdapter> baseAdapter;
        ThrowIfFailed(dxgiDevice->GetAdapter(&baseAdapter), "IDXGIDevice::GetAdapter");
        ThrowIfFailed(baseAdapter.As(&adapter_), "Query IDXGIAdapter1");
        ThrowIfFailed(adapter_->GetDesc1(&adapterDescription_), "IDXGIAdapter1::GetDesc1");
    }
    adapterName_ = WideToUtf8(adapterDescription_.Description);
    std::ostringstream luidText;
    luidText << std::hex << std::setfill('0') << std::setw(8)
             << static_cast<std::uint32_t>(adapterDescription_.AdapterLuid.HighPart) << ':'
             << std::setw(8) << adapterDescription_.AdapterLuid.LowPart;
    adapterLuid_ = luidText.str();
    binaryHash_ = FileSha256(ExecutablePath());

    LARGE_INTEGER driver{};
    if (SUCCEEDED(adapter_->CheckInterfaceSupport(__uuidof(IDXGIDevice), &driver))) {
        driverVersion_ = DriverVersionString(driver);
    }
    threadingAvailable_ = SUCCEEDED(device_->CheckFeatureSupport(
        D3D11_FEATURE_THREADING, &threading_, sizeof(threading_)));
    options3Available_ = SUCCEEDED(device_->CheckFeatureSupport(
        D3D11_FEATURE_D3D11_OPTIONS3, &options3_, sizeof(options3_)));
    if (options_.debugLayer) device_.As(&infoQueue_);

    logger_.Info("device", "Adapter: " + adapterName_ + "; driver " + driverVersion_ +
                               "; feature level " + FeatureLevelName(featureLevel_) + ".");
}

void D3D11Lab::Impl::WriteCapabilities() const {
    std::ostringstream json;
    json << "{\n  \"schema\": 1,\n"
         << "  \"adapter\": \"" << JsonEscape(adapterName_) << "\",\n"
         << "  \"vendor_id\": " << adapterDescription_.VendorId << ",\n"
         << "  \"device_id\": " << adapterDescription_.DeviceId << ",\n"
         << "  \"adapter_luid\": \"" << adapterLuid_ << "\",\n"
         << "  \"dedicated_video_memory_bytes\": " << adapterDescription_.DedicatedVideoMemory << ",\n"
         << "  \"driver\": \"" << JsonEscape(driverVersion_) << "\",\n"
         << "  \"driver_version_source\": \"IDXGIAdapter::CheckInterfaceSupport(IDXGIDevice)\",\n"
         << "  \"feature_level\": \"" << FeatureLevelName(featureLevel_) << "\",\n"
         << "  \"creation_flags\": " << deviceFlags_ << ",\n"
         << "  \"debug_layer\": " << (options_.debugLayer ? "true" : "false") << ",\n"
         << "  \"warp\": " << (options_.warp ? "true" : "false") << ",\n"
         << "  \"threading_query_succeeded\": " << (threadingAvailable_ ? "true" : "false") << ",\n"
         << "  \"driver_concurrent_creates\": " << (threading_.DriverConcurrentCreates ? "true" : "false") << ",\n"
         << "  \"driver_command_lists\": " << (threading_.DriverCommandLists ? "true" : "false") << ",\n"
         << "  \"options3_query_succeeded\": " << (options3Available_ ? "true" : "false") << ",\n"
         << "  \"vp_rt_array_index_from_any_shader\": "
         << ((options3Available_ && options3_.VPAndRTArrayIndexFromAnyShaderFeedingRasterizer) ? "true" : "false") << ",\n"
         << "  \"vendor_stereo_api\": \"not_used\",\n"
         << "  \"mechanisms\": [\"D3D11 instancing\", \"clip-space side-by-side packing\", \"geometry shader viewport routing\"]\n"
         << "}\n";
    WriteTextFile(paths_.root / "capabilities.json", json.str());
}

void D3D11Lab::Impl::CreateTargets() {
    D3D11_TEXTURE2D_DESC colorDescription{};
    colorDescription.Width = options_.widthPerEye * 2;
    colorDescription.Height = options_.height;
    colorDescription.MipLevels = 1;
    colorDescription.ArraySize = 1;
    colorDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    colorDescription.SampleDesc.Count = 1;
    colorDescription.Usage = D3D11_USAGE_DEFAULT;
    colorDescription.BindFlags = D3D11_BIND_RENDER_TARGET;
    ThrowIfFailed(device_->CreateTexture2D(&colorDescription, nullptr, &color_), "Create color target");
    ThrowIfFailed(device_->CreateRenderTargetView(color_.Get(), nullptr, &colorRtv_), "Create color RTV");
    colorDescription.Usage = D3D11_USAGE_STAGING;
    colorDescription.BindFlags = 0;
    colorDescription.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ThrowIfFailed(device_->CreateTexture2D(&colorDescription, nullptr, &colorStaging_), "Create color staging target");

    D3D11_TEXTURE2D_DESC depthDescription{};
    depthDescription.Width = options_.widthPerEye * 2;
    depthDescription.Height = options_.height;
    depthDescription.MipLevels = 1;
    depthDescription.ArraySize = 1;
    depthDescription.Format = DXGI_FORMAT_R32_TYPELESS;
    depthDescription.SampleDesc.Count = 1;
    depthDescription.Usage = D3D11_USAGE_DEFAULT;
    depthDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ThrowIfFailed(device_->CreateTexture2D(&depthDescription, nullptr, &depth_), "Create depth target");
    D3D11_DEPTH_STENCIL_VIEW_DESC depthView{};
    depthView.Format = DXGI_FORMAT_D32_FLOAT;
    depthView.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    ThrowIfFailed(device_->CreateDepthStencilView(depth_.Get(), &depthView, &depthDsv_), "Create depth DSV");
    depthDescription.Usage = D3D11_USAGE_STAGING;
    depthDescription.BindFlags = 0;
    depthDescription.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ThrowIfFailed(device_->CreateTexture2D(&depthDescription, nullptr, &depthStaging_), "Create depth staging target");
}

ComPtr<ID3DBlob> D3D11Lab::Impl::CompileShader(const char* entry, const char* profile) {
    const auto shaderPath = options_.shaderRoot / "StereoLab.hlsl";
    if (!std::filesystem::is_regular_file(shaderPath)) {
        throw std::runtime_error("Shader file not found: " + shaderPath.string());
    }
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifndef NDEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
    ComPtr<ID3DBlob> bytecode;
    ComPtr<ID3DBlob> errors;
    const HRESULT result = D3DCompileFromFile(shaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                                              entry, profile, flags, 0, &bytecode, &errors);
    if (errors && errors->GetBufferSize()) {
        const std::string diagnostics(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
        std::ofstream file(paths_.root / "shader_compile.log", std::ios::app | std::ios::binary);
        file << "[" << UtcNow() << "] " << entry << " / " << profile << '\n' << diagnostics << '\n';
        logger_.Event(FAILED(result) ? "error" : "warning", "shader", diagnostics);
    }
    ThrowIfFailed(result, std::string("Compile shader ") + entry);
    return bytecode;
}

void D3D11Lab::Impl::CreatePipeline() {
    const auto reference = CompileShader("VSReference", "vs_5_0");
    const auto instanced = CompileShader("VSInstancedStereo", "vs_5_0");
    const auto world = CompileShader("VSWorld", "vs_5_0");
    const auto geometry = CompileShader("GSStereo", "gs_5_0");
    const auto pixel = CompileShader("PSMain", "ps_5_0");
    ThrowIfFailed(device_->CreateVertexShader(reference->GetBufferPointer(), reference->GetBufferSize(), nullptr, &referenceVs_), "Create reference VS");
    ThrowIfFailed(device_->CreateVertexShader(instanced->GetBufferPointer(), instanced->GetBufferSize(), nullptr, &instancedVs_), "Create instanced VS");
    ThrowIfFailed(device_->CreateVertexShader(world->GetBufferPointer(), world->GetBufferSize(), nullptr, &worldVs_), "Create world VS");
    ThrowIfFailed(device_->CreateGeometryShader(geometry->GetBufferPointer(), geometry->GetBufferSize(), nullptr, &stereoGs_), "Create stereo GS");
    ThrowIfFailed(device_->CreatePixelShader(pixel->GetBufferPointer(), pixel->GetBufferSize(), nullptr, &pixelShader_), "Create pixel shader");

    const std::array<D3D11_INPUT_ELEMENT_DESC, 2> input{{
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}}};
    ThrowIfFailed(device_->CreateInputLayout(input.data(), static_cast<UINT>(input.size()),
                                              reference->GetBufferPointer(), reference->GetBufferSize(),
                                              &inputLayout_), "Create input layout");

    const std::array<Vertex, 8> vertices{{
        {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.1f, 0.1f}},
        {{-0.5f,  0.5f, -0.5f}, {0.1f, 1.0f, 0.1f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.1f, 0.1f, 1.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.1f}},
        {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.1f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.1f, 1.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.4f, 0.4f, 0.4f}}}};
    const std::array<std::uint16_t, 36> indices{{
        0,1,2, 0,2,3, 4,6,5, 4,7,6, 4,5,1, 4,1,0,
        3,2,6, 3,6,7, 1,5,6, 1,6,2, 4,0,3, 4,3,7}};
    D3D11_BUFFER_DESC description{};
    description.Usage = D3D11_USAGE_IMMUTABLE;
    description.ByteWidth = static_cast<UINT>(sizeof(vertices));
    description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA initial{vertices.data(), 0, 0};
    ThrowIfFailed(device_->CreateBuffer(&description, &initial, &vertexBuffer_), "Create vertex buffer");
    description.ByteWidth = static_cast<UINT>(sizeof(indices));
    description.BindFlags = D3D11_BIND_INDEX_BUFFER;
    initial.pSysMem = indices.data();
    ThrowIfFailed(device_->CreateBuffer(&description, &initial, &indexBuffer_), "Create index buffer");

    description = {};
    description.Usage = D3D11_USAGE_DEFAULT;
    description.ByteWidth = sizeof(FrameConstants);
    description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ThrowIfFailed(device_->CreateBuffer(&description, nullptr, &frameBuffer_), "Create frame constants");
    description.ByteWidth = sizeof(ObjectConstants);
    ThrowIfFailed(device_->CreateBuffer(&description, nullptr, &objectBuffer_), "Create object constants");
    for (std::uint32_t eye = 0; eye < 2; ++eye) {
        EyeConstants constants{eye, {0, 0, 0}};
        description.Usage = D3D11_USAGE_IMMUTABLE;
        description.ByteWidth = sizeof(EyeConstants);
        D3D11_SUBRESOURCE_DATA eyeInitial{&constants, 0, 0};
        ThrowIfFailed(device_->CreateBuffer(&description, &eyeInitial, &eyeBuffers_[eye]), "Create eye constants");
    }

    D3D11_RASTERIZER_DESC rasterizer{};
    rasterizer.FillMode = D3D11_FILL_SOLID;
    rasterizer.CullMode = D3D11_CULL_NONE;
    rasterizer.DepthClipEnable = TRUE;
    ThrowIfFailed(device_->CreateRasterizerState(&rasterizer, &rasterizer_), "Create rasterizer state");
    D3D11_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable = TRUE;
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depth.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    ThrowIfFailed(device_->CreateDepthStencilState(&depth, &depthState_), "Create depth state");
}

std::uint32_t D3D11Lab::Impl::PixelIterations() const { return options_.scene == "S3" ? 24u : 0u; }
std::uint32_t D3D11Lab::Impl::VertexIterations() const { return options_.scene == "S2" ? 24u : 0u; }

void D3D11Lab::Impl::CreateScene(std::uint32_t count) {
    objects_.resize(count);
    const std::uint32_t columns = static_cast<std::uint32_t>(std::ceil(std::sqrt(static_cast<double>(count))));
    const float spacing = std::max(0.12f, 3.4f / static_cast<float>(columns));
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::uint32_t xIndex = index % columns;
        const std::uint32_t yIndex = (index / columns) % columns;
        const std::uint32_t layer = index / (columns * columns);
        const float x = (static_cast<float>(xIndex) - static_cast<float>(columns - 1) * 0.5f) * spacing;
        const float y = (static_cast<float>(yIndex) - static_cast<float>(columns - 1) * 0.5f) * spacing;
        const float z = 4.0f + static_cast<float>(layer) * 0.8f + static_cast<float>(index % 7) * 0.035f;
        const float scale = spacing * (0.33f + static_cast<float>(index % 5) * 0.025f);
        const XMMATRIX model = XMMatrixScaling(scale, scale, scale) * XMMatrixTranslation(x, y, z);
        XMStoreFloat4x4(&objects_[index].model, model);
        objects_[index].color = XMFLOAT4(
            0.35f + 0.65f * static_cast<float>((index * 17) % 97) / 96.0f,
            0.35f + 0.65f * static_cast<float>((index * 31) % 89) / 88.0f,
            0.35f + 0.65f * static_cast<float>((index * 47) % 83) / 82.0f, 1.0f);
    }

    // Deterministic clipping and depth-overlap probes. The first pair straddles
    // opposite eye-frustum boundaries; the second pair intersects in depth.
    if (count >= 4) {
        XMStoreFloat4x4(&objects_[0].model,
                        XMMatrixScaling(0.65f, 0.65f, 0.65f) * XMMatrixTranslation(-4.0f, 0.0f, 4.0f));
        XMStoreFloat4x4(&objects_[1].model,
                        XMMatrixScaling(0.65f, 0.65f, 0.65f) * XMMatrixTranslation(4.0f, 0.0f, 4.0f));
        XMStoreFloat4x4(&objects_[2].model,
                        XMMatrixScaling(0.9f, 0.9f, 0.9f) * XMMatrixTranslation(-0.18f, 0.0f, 3.4f));
        XMStoreFloat4x4(&objects_[3].model,
                        XMMatrixScaling(0.9f, 0.9f, 0.9f) * XMMatrixTranslation(0.18f, 0.0f, 3.7f));
    }

    D3D11_BUFFER_DESC description{};
    description.Usage = D3D11_USAGE_IMMUTABLE;
    description.ByteWidth = static_cast<UINT>(objects_.size() * sizeof(ObjectConstants));
    description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    description.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    description.StructureByteStride = sizeof(ObjectConstants);
    D3D11_SUBRESOURCE_DATA initial{objects_.data(), 0, 0};
    ThrowIfFailed(device_->CreateBuffer(&description, &initial, &structuredObjects_), "Create structured objects");
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = DXGI_FORMAT_UNKNOWN;
    view.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    view.Buffer.NumElements = static_cast<UINT>(objects_.size());
    ThrowIfFailed(device_->CreateShaderResourceView(structuredObjects_.Get(), &view, &structuredObjectsSrv_), "Create object SRV");

    FrameConstants frame{};
    const float nearPlane = 0.1f;
    const float farPlane = 100.0f;
    for (std::uint32_t eye = 0; eye < 2; ++eye) {
        const float sign = eye == 0 ? -1.0f : 1.0f;
        const XMVECTOR eyePosition = XMVectorSet(sign * 0.032f, 0.0f, 0.0f, 1.0f);
        const XMMATRIX viewMatrix = XMMatrixLookAtLH(eyePosition,
                                                     XMVectorSet(sign * 0.032f, 0.0f, 1.0f, 1.0f),
                                                     XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
        const float projectionShift = -sign * 0.003f;
        const XMMATRIX projection = XMMatrixPerspectiveOffCenterLH(
            -0.1f + projectionShift, 0.1f + projectionShift, -0.1f, 0.1f,
            nearPlane, farPlane);
        XMStoreFloat4x4(&frame.viewProjection[eye], viewMatrix * projection);
    }
    frame.workload = XMFLOAT4(static_cast<float>(PixelIterations()),
                              static_cast<float>(VertexIterations()), 0.0f, 0.0f);
    context_->UpdateSubresource(frameBuffer_.Get(), 0, nullptr, &frame, 0, 0);
}

void D3D11Lab::Impl::ConfigureCommonPipeline() {
    ID3D11RenderTargetView* renderTarget = colorRtv_.Get();
    context_->OMSetRenderTargets(1, &renderTarget, depthDsv_.Get());
    context_->OMSetDepthStencilState(depthState_.Get(), 0);
    context_->RSSetState(rasterizer_.Get());
    context_->IASetInputLayout(inputLayout_.Get());
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    ID3D11Buffer* vertex = vertexBuffer_.Get();
    context_->IASetVertexBuffers(0, 1, &vertex, &stride, &offset);
    context_->IASetIndexBuffer(indexBuffer_.Get(), DXGI_FORMAT_R16_UINT, 0);
    ID3D11Buffer* frame = frameBuffer_.Get();
    context_->VSSetConstantBuffers(0, 1, &frame);
    context_->GSSetConstantBuffers(0, 1, &frame);
    context_->PSSetConstantBuffers(0, 1, &frame);
    context_->PSSetShader(pixelShader_.Get(), nullptr, 0);
}

void D3D11Lab::Impl::UpdateObjectBuffer(const ObjectConstants& object) {
    context_->UpdateSubresource(objectBuffer_.Get(), 0, nullptr, &object, 0, 0);
}

void D3D11Lab::Impl::DrawReference(std::string_view, std::uint32_t draws) {
    context_->VSSetShader(referenceVs_.Get(), nullptr, 0);
    context_->GSSetShader(nullptr, nullptr, 0);
    ID3D11Buffer* object = objectBuffer_.Get();
    context_->VSSetConstantBuffers(1, 1, &object);
    const std::array<D3D11_VIEWPORT, 2> viewports{{
        {0.0f, 0.0f, static_cast<float>(options_.widthPerEye), static_cast<float>(options_.height), 0.0f, 1.0f},
        {static_cast<float>(options_.widthPerEye), 0.0f, static_cast<float>(options_.widthPerEye), static_cast<float>(options_.height), 0.0f, 1.0f}}};
    for (std::uint32_t eye = 0; eye < 2; ++eye) {
        context_->RSSetViewports(1, &viewports[eye]);
        ID3D11Buffer* eyeBuffer = eyeBuffers_[eye].Get();
        context_->VSSetConstantBuffers(2, 1, &eyeBuffer);
        for (std::uint32_t objectIndex = 0; objectIndex < draws; ++objectIndex) {
            UpdateObjectBuffer(objects_[objectIndex]);
            context_->DrawIndexed(36, 0, 0);
        }
    }
}

void D3D11Lab::Impl::DrawInstanced(std::uint32_t draws) {
    context_->VSSetShader(instancedVs_.Get(), nullptr, 0);
    context_->GSSetShader(nullptr, nullptr, 0);
    ID3D11ShaderResourceView* objects = structuredObjectsSrv_.Get();
    context_->VSSetShaderResources(0, 1, &objects);
    const D3D11_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(options_.widthPerEye * 2),
                                  static_cast<float>(options_.height), 0.0f, 1.0f};
    context_->RSSetViewports(1, &viewport);
    context_->DrawIndexedInstanced(36, draws * 2, 0, 0, 0);
    ID3D11ShaderResourceView* nullView = nullptr;
    context_->VSSetShaderResources(0, 1, &nullView);
}

void D3D11Lab::Impl::DrawGeometryStereo(std::uint32_t draws) {
    context_->VSSetShader(worldVs_.Get(), nullptr, 0);
    context_->GSSetShader(stereoGs_.Get(), nullptr, 0);
    ID3D11ShaderResourceView* objects = structuredObjectsSrv_.Get();
    context_->VSSetShaderResources(0, 1, &objects);
    const std::array<D3D11_VIEWPORT, 2> viewports{{
        {0.0f, 0.0f, static_cast<float>(options_.widthPerEye), static_cast<float>(options_.height), 0.0f, 1.0f},
        {static_cast<float>(options_.widthPerEye), 0.0f, static_cast<float>(options_.widthPerEye), static_cast<float>(options_.height), 0.0f, 1.0f}}};
    context_->RSSetViewports(static_cast<UINT>(viewports.size()), viewports.data());
    context_->DrawIndexedInstanced(36, draws, 0, 0, 0);
    ID3D11ShaderResourceView* nullView = nullptr;
    context_->VSSetShaderResources(0, 1, &nullView);
    context_->GSSetShader(nullptr, nullptr, 0);
}

void D3D11Lab::Impl::RenderBackend(std::string_view backend, std::uint32_t draws, GpuQueries* queries) {
    const float clear[4]{0.015f, 0.02f, 0.035f, 1.0f};
    context_->ClearRenderTargetView(colorRtv_.Get(), clear);
    context_->ClearDepthStencilView(depthDsv_.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    ConfigureCommonPipeline();

    if (queries) {
        context_->Begin(queries->disjoint.Get());
        context_->End(queries->begin.Get());
    }
    if (backend == "B0" || backend == "B1") DrawReference(backend, draws);
    else if (backend == "B2") DrawInstanced(draws);
    else if (backend == "B3") DrawGeometryStereo(draws);
    else throw std::runtime_error("Internal error: unsupported backend.");
    if (queries) {
        context_->End(queries->end.Get());
        context_->End(queries->disjoint.Get());
    }
}

ImageData D3D11Lab::Impl::CaptureTarget() {
    context_->CopyResource(colorStaging_.Get(), color_.Get());
    context_->CopyResource(depthStaging_.Get(), depth_.Get());
    context_->Flush();
    ImageData image;
    image.width = options_.widthPerEye * 2;
    image.height = options_.height;
    image.rgba.resize(static_cast<std::size_t>(image.width) * image.height * 4);
    image.depth.resize(static_cast<std::size_t>(image.width) * image.height);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    ThrowIfFailed(context_->Map(colorStaging_.Get(), 0, D3D11_MAP_READ, 0, &mapped), "Map color staging");
    for (std::uint32_t row = 0; row < image.height; ++row) {
        std::memcpy(image.rgba.data() + static_cast<std::size_t>(row) * image.width * 4,
                    static_cast<const std::uint8_t*>(mapped.pData) + static_cast<std::size_t>(row) * mapped.RowPitch,
                    static_cast<std::size_t>(image.width) * 4);
    }
    context_->Unmap(colorStaging_.Get(), 0);
    ThrowIfFailed(context_->Map(depthStaging_.Get(), 0, D3D11_MAP_READ, 0, &mapped), "Map depth staging");
    for (std::uint32_t row = 0; row < image.height; ++row) {
        std::memcpy(image.depth.data() + static_cast<std::size_t>(row) * image.width,
                    static_cast<const std::uint8_t*>(mapped.pData) + static_cast<std::size_t>(row) * mapped.RowPitch,
                    static_cast<std::size_t>(image.width) * sizeof(float));
    }
    context_->Unmap(depthStaging_.Get(), 0);
    return image;
}

void D3D11Lab::Impl::SavePpm(const std::filesystem::path& path, const ImageData& image) const {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) throw std::runtime_error("Could not create image " + path.string());
    file << "P6\n" << image.width << ' ' << image.height << "\n255\n";
    for (std::size_t pixel = 0; pixel < image.rgba.size(); pixel += 4) {
        file.write(reinterpret_cast<const char*>(image.rgba.data() + pixel), 3);
    }
    file.flush();
    if (!file) throw std::runtime_error("Could not finish image " + path.string());
}

void D3D11Lab::Impl::SavePgmDepth(const std::filesystem::path& path, const ImageData& image) const {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) throw std::runtime_error("Could not create depth image " + path.string());
    file << "P5\n" << image.width << ' ' << image.height << "\n255\n";
    for (float depth : image.depth) {
        const auto value = static_cast<std::uint8_t>(std::clamp((1.0f - depth) * 4096.0f, 0.0f, 255.0f));
        file.put(static_cast<char>(value));
    }
    file.flush();
    if (!file) throw std::runtime_error("Could not finish depth image " + path.string());
}

void D3D11Lab::Impl::SaveDiff(const std::filesystem::path& path, const ImageData& reference,
                              const ImageData& candidate) const {
    ImageData difference;
    difference.width = reference.width;
    difference.height = reference.height;
    difference.rgba.resize(reference.rgba.size(), 255);
    for (std::size_t i = 0; i < reference.rgba.size(); i += 4) {
        const auto dr = static_cast<std::uint8_t>(std::abs(int(reference.rgba[i]) - int(candidate.rgba[i])));
        const auto dg = static_cast<std::uint8_t>(std::abs(int(reference.rgba[i + 1]) - int(candidate.rgba[i + 1])));
        const auto db = static_cast<std::uint8_t>(std::abs(int(reference.rgba[i + 2]) - int(candidate.rgba[i + 2])));
        const auto maximum = std::max({dr, dg, db});
        difference.rgba[i] = maximum;
        difference.rgba[i + 1] = static_cast<std::uint8_t>(maximum / 8);
        difference.rgba[i + 2] = static_cast<std::uint8_t>(maximum / 8);
    }
    SavePpm(path, difference);
}

ValidationRecord D3D11Lab::Impl::Compare(const std::string& backend, const ImageData& reference,
                                         const ImageData& candidate) const {
    ValidationRecord result;
    result.backend = backend;
    const std::size_t pixels = static_cast<std::size_t>(reference.width) * reference.height;
    for (std::size_t pixel = 0; pixel < pixels; ++pixel) {
        std::uint8_t maximum = 0;
        for (std::size_t channel = 0; channel < 4; ++channel) {
            const auto error = static_cast<std::uint8_t>(std::abs(
                int(reference.rgba[pixel * 4 + channel]) - int(candidate.rgba[pixel * 4 + channel])));
            maximum = std::max(maximum, error);
            result.maxChannelError = std::max(result.maxChannelError, error);
        }
        if (maximum > 2) {
            ++result.mismatchPixels;
            const std::size_t x = pixel % reference.width;
            const std::size_t y = pixel / reference.width;
            if (x < options_.widthPerEye) ++result.leftMismatchPixels;
            else ++result.rightMismatchPixels;
            const auto seam = static_cast<std::size_t>(options_.widthPerEye);
            if (x + 2 >= seam && x <= seam + 1) ++result.seamMismatchPixels;
            if (x < 2 || x >= reference.width - 2 || y < 2 || y >= reference.height - 2) {
                ++result.outerEdgeMismatchPixels;
            }
        }
        const double depthError = std::abs(static_cast<double>(reference.depth[pixel]) - candidate.depth[pixel]);
        result.depthMaxError = std::max(result.depthMaxError, depthError);
        if (depthError > 1.0e-5) ++result.depthMismatchPixels;
    }
    result.mismatchPercent = pixels ? (100.0 * static_cast<double>(result.mismatchPixels) / static_cast<double>(pixels)) : 100.0;
    result.passed = result.mismatchPercent <= 0.10 && result.seamMismatchPixels == 0 &&
                    result.depthMismatchPixels <= pixels / 1000;
    return result;
}

void D3D11Lab::Impl::RunValidation() {
    logger_.Info("validation", "Rendering B0 correctness oracle with asymmetric eye projections.");
    RenderBackend("B0", options_.validationDraws, nullptr);
    const ImageData reference = CaptureTarget();
    SavePpm(paths_.images / "B0-reference-color.ppm", reference);
    SavePgmDepth(paths_.images / "B0-reference-depth.pgm", reference);

    for (const auto& backend : options_.backends) {
        RenderBackend(backend, options_.validationDraws, nullptr);
        const ImageData candidate = CaptureTarget();
        SavePpm(paths_.images / (backend + "-color.ppm"), candidate);
        SavePgmDepth(paths_.images / (backend + "-depth.pgm"), candidate);
        SaveDiff(paths_.images / (backend + "-diff.ppm"), reference, candidate);
        auto result = Compare(backend, reference, candidate);
        validationRecords_.push_back(result);
        std::ostringstream message;
        message << backend << " (" << BackendDescription(backend) << ") validation "
                << (result.passed ? "passed" : "FAILED") << ": " << result.mismatchPixels
                << " color mismatches (" << std::fixed << std::setprecision(5) << result.mismatchPercent
                << "%), max channel error " << static_cast<unsigned>(result.maxChannelError)
                << ", depth mismatches " << result.depthMismatchPixels << '.';
        logger_.Event(result.passed ? "info" : "error", "validation", message.str());
    }
    WriteValidationCsv();
}

GpuQueries D3D11Lab::Impl::CreateGpuQueries() const {
    GpuQueries queries;
    D3D11_QUERY_DESC description{D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
    ThrowIfFailed(device_->CreateQuery(&description, &queries.disjoint), "Create disjoint query");
    description.Query = D3D11_QUERY_TIMESTAMP;
    ThrowIfFailed(device_->CreateQuery(&description, &queries.begin), "Create begin timestamp");
    ThrowIfFailed(device_->CreateQuery(&description, &queries.end), "Create end timestamp");
    return queries;
}

void D3D11Lab::Impl::ResolveGpuQueries(const std::vector<GpuQueries>& queries,
                                       std::vector<FrameRecord>& records) {
    context_->Flush();
    const auto deadline = Clock::now() + std::chrono::seconds(30);
    for (std::size_t index = 0; index < queries.size(); ++index) {
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
        UINT64 begin = 0;
        UINT64 end = 0;
        auto waitFor = [&](ID3D11Query* query, void* data, UINT size) {
            HRESULT status = S_FALSE;
            while ((status = context_->GetData(query, data, size, D3D11_ASYNC_GETDATA_DONOTFLUSH)) == S_FALSE) {
                if (Clock::now() >= deadline) return false;
                std::this_thread::yield();
            }
            return SUCCEEDED(status);
        };
        const bool available = waitFor(queries[index].disjoint.Get(), &disjoint, sizeof(disjoint)) &&
                               waitFor(queries[index].begin.Get(), &begin, sizeof(begin)) &&
                               waitFor(queries[index].end.Get(), &end, sizeof(end));
        if (available && !disjoint.Disjoint && disjoint.Frequency != 0 && end >= begin) {
            records[index].gpuUs = (static_cast<double>(end - begin) * 1'000'000.0) /
                                   static_cast<double>(disjoint.Frequency);
            records[index].queryValid = true;
        }
    }
}

void D3D11Lab::Impl::RunBenchmark() {
    for (const auto& backend : options_.backends) {
        const auto validation = std::find_if(validationRecords_.begin(), validationRecords_.end(),
                                             [&](const auto& item) { return item.backend == backend; });
        if (validation == validationRecords_.end() || !validation->passed) {
            logger_.Warn("benchmark", "Skipping " + backend + " because correctness validation failed.");
            continue;
        }
        logger_.Info("benchmark", "Warmup for " + backend + ": " + std::to_string(options_.warmupFrames) + " frames.");
        for (std::uint32_t frame = 0; frame < options_.warmupFrames; ++frame) {
            RenderBackend(backend, options_.draws, nullptr);
        }
        context_->Flush();

        std::vector<GpuQueries> queries;
        queries.reserve(options_.measuredFrames);
        for (std::uint32_t frame = 0; frame < options_.measuredFrames; ++frame) queries.push_back(CreateGpuQueries());
        std::vector<FrameRecord> records(options_.measuredFrames);
        logger_.Info("benchmark", "Measuring " + backend + ": " + std::to_string(options_.measuredFrames) + " frames.");

        for (std::uint32_t frame = 0; frame < options_.measuredFrames; ++frame) {
            const auto totalBegin = Clock::now();
            const auto prepareBegin = totalBegin;
            // The packet lists are deliberately rebuilt: B0 duplicates invariant CPU packets,
            // B1 shares one list, while B2/B3 submit a compact instance domain.
            std::vector<std::uint32_t> packetA;
            std::vector<std::uint32_t> packetB;
            if (backend == "B0") {
                packetA.resize(options_.draws);
                std::iota(packetA.begin(), packetA.end(), 0u);
                packetB = packetA;
            } else if (backend == "B1") {
                packetA.resize(options_.draws);
                std::iota(packetA.begin(), packetA.end(), 0u);
            } else {
                packetA.push_back(options_.draws);
            }
            volatile std::uint32_t packetGuard = packetA.front();
            if (!packetB.empty()) packetGuard ^= packetB.back();
            (void)packetGuard;
            const auto prepareEnd = Clock::now();
            const auto submitBegin = prepareEnd;
            RenderBackend(backend, options_.draws, &queries[frame]);
            const auto submitEnd = Clock::now();
            records[frame].frame = frame;
            records[frame].backend = backend;
            records[frame].cpuPrepareUs = Microseconds(prepareBegin, prepareEnd);
            records[frame].cpuSubmitUs = Microseconds(submitBegin, submitEnd);
            records[frame].cpuTotalUs = Microseconds(totalBegin, submitEnd);
        }
        ResolveGpuQueries(queries, records);
        const auto validQueries = std::count_if(records.begin(), records.end(),
                                                [](const auto& record) { return record.queryValid; });
        logger_.Info("benchmark", backend + " collected " + std::to_string(validQueries) + "/" +
                                      std::to_string(records.size()) + " valid GPU samples.");
        frameRecords_.insert(frameRecords_.end(), records.begin(), records.end());
        WriteBenchmarkCsv();
    }
}

void D3D11Lab::Impl::WriteBenchmarkCsv() const {
    std::ofstream csv(paths_.benchmark, std::ios::trunc);
    if (!csv) throw std::runtime_error("Could not create benchmark.csv.");
    csv << "run_id,process_run,frame_id,backend,backend_description,scene,width_per_eye,height,draws,triangles_per_eye,instances_per_eye,adapter,adapter_luid,driver,feature_level,debug_layer,warp,build_hash,binary_sha256,shader_sha256,cpu_prepare_us,cpu_submit_us,cpu_total_us,gpu_total_us,query_valid\n";
    for (const auto& record : frameRecords_) {
        csv << CsvEscape(paths_.runId) << ",1," << record.frame << ',' << record.backend << ','
            << CsvEscape(BackendDescription(record.backend)) << ',' << options_.scene << ','
            << options_.widthPerEye << ',' << options_.height << ',' << options_.draws << ','
            << options_.draws * 12ull << ',' << options_.draws << ',' << CsvEscape(adapterName_) << ','
            << adapterLuid_ << ',' << CsvEscape(driverVersion_) << ',' << FeatureLevelName(featureLevel_) << ','
            << (options_.debugLayer ? 1 : 0) << ',' << (options_.warp ? 1 : 0) << ','
            << STEREO_LAB_GIT_SHA << ',' << binaryHash_ << ',' << shaderHash_ << ','
            << std::fixed << std::setprecision(3) << record.cpuPrepareUs << ',' << record.cpuSubmitUs << ','
            << record.cpuTotalUs << ',';
        if (record.queryValid) csv << record.gpuUs;
        csv << ',' << (record.queryValid ? 1 : 0) << '\n';
    }
    csv.flush();
    if (!csv) throw std::runtime_error("Could not finish benchmark.csv.");
}

void D3D11Lab::Impl::WriteValidationCsv() const {
    std::ofstream csv(paths_.validation, std::ios::trunc);
    if (!csv) throw std::runtime_error("Could not create validation.csv.");
    csv << "run_id,backend,backend_description,passed,width_per_eye,height,draws,color_tolerance,max_channel_error,mismatch_pixels,mismatch_percent,left_mismatch_pixels,right_mismatch_pixels,seam_mismatch_pixels,outer_edge_mismatch_pixels,depth_tolerance,depth_max_error,depth_mismatch_pixels\n";
    for (const auto& record : validationRecords_) {
        csv << CsvEscape(paths_.runId) << ',' << record.backend << ',' << CsvEscape(BackendDescription(record.backend))
            << ',' << (record.passed ? 1 : 0) << ',' << options_.widthPerEye << ',' << options_.height << ','
            << options_.validationDraws << ",2," << static_cast<unsigned>(record.maxChannelError) << ','
            << record.mismatchPixels << ',' << std::fixed << std::setprecision(7) << record.mismatchPercent
            << ',' << record.leftMismatchPixels << ',' << record.rightMismatchPixels << ','
            << record.seamMismatchPixels << ',' << record.outerEdgeMismatchPixels
            << ",0.00001," << record.depthMaxError << ',' << record.depthMismatchPixels << '\n';
    }
    csv.flush();
    if (!csv) throw std::runtime_error("Could not finish validation.csv.");
}

void D3D11Lab::Impl::DrainDebugLayer() {
    std::ofstream output(paths_.root / "debug_layer.log", std::ios::trunc);
    if (!infoQueue_) {
        output << "Debug layer not enabled for this run. Use --debug for correctness qualification.\n";
        return;
    }
    const auto count = infoQueue_->GetNumStoredMessagesAllowedByRetrievalFilter();
    for (UINT64 index = 0; index < count; ++index) {
        SIZE_T size = 0;
        infoQueue_->GetMessage(index, nullptr, &size);
        std::vector<std::uint8_t> storage(size);
        auto* message = reinterpret_cast<D3D11_MESSAGE*>(storage.data());
        if (FAILED(infoQueue_->GetMessage(index, message, &size))) continue;
        output << "severity=" << message->Severity << " category=" << message->Category
               << " id=" << message->ID << " " << message->pDescription << '\n';
        if (message->Severity == D3D11_MESSAGE_SEVERITY_ERROR ||
            message->Severity == D3D11_MESSAGE_SEVERITY_CORRUPTION) ++debugErrors_;
        else if (message->Severity == D3D11_MESSAGE_SEVERITY_WARNING) ++debugWarnings_;
    }
    logger_.Event(debugErrors_ ? "error" : (debugWarnings_ ? "warning" : "info"), "debug_layer",
                  "D3D11 debug messages: " + std::to_string(debugErrors_) + " errors, " +
                      std::to_string(debugWarnings_) + " warnings.");
}

void D3D11Lab::Impl::WriteSummary() const {
    std::ostringstream summary;
    summary << "# StereoCapabilityLab run " << paths_.runId << "\n\n"
            << "Adapter: **" << adapterName_ << "**  \n"
            << "Driver: **" << driverVersion_ << "**  \n"
            << "Feature level: **" << FeatureLevelName(featureLevel_) << "**  \n"
            << "Workload: **" << options_.scene << ", " << options_.widthPerEye << "x" << options_.height
            << " per eye, " << options_.draws << " objects**\n\n"
            << "## Correctness\n\n"
            << "| Backend | Result | Color mismatches | Left / right | Seam | Max channel error | Depth mismatches |\n"
            << "|---|---:|---:|---:|---:|---:|---:|\n";
    for (const auto& validation : validationRecords_) {
        summary << "| " << validation.backend << " | " << (validation.passed ? "PASS" : "FAIL")
                << " | " << validation.mismatchPixels << " | "
                << validation.leftMismatchPixels << " / " << validation.rightMismatchPixels << " | "
                << validation.seamMismatchPixels << " | "
                << static_cast<unsigned>(validation.maxChannelError) << " | "
                << validation.depthMismatchPixels << " |\n";
    }

    summary << "\n## Performance\n\n"
            << "Debug-layer runs are correctness evidence, not release-performance evidence. GPU statistics omit invalid/disjoint samples.\n\n"
            << "| Backend | CPU prepare mean (us) | CPU submit mean/P50/P95/P99 (us) | GPU mean/P50/P95/P99 (us) | Valid GPU samples |\n"
            << "|---|---:|---|---|---:|\n";
    std::map<std::string, double> cpuMeans;
    std::map<std::string, double> gpuMeans;
    for (const auto& backend : options_.backends) {
        std::vector<double> cpu;
        std::vector<double> prepare;
        std::vector<double> gpu;
        for (const auto& record : frameRecords_) {
            if (record.backend != backend) continue;
            prepare.push_back(record.cpuPrepareUs);
            cpu.push_back(record.cpuSubmitUs);
            if (record.queryValid) gpu.push_back(record.gpuUs);
        }
        const auto cpuStats = CalculateStats(cpu);
        const auto prepareStats = CalculateStats(prepare);
        const auto gpuStats = CalculateStats(gpu);
        if (!cpu.empty()) cpuMeans[backend] = cpuStats.mean;
        if (!gpu.empty()) gpuMeans[backend] = gpuStats.mean;
        summary << "| " << backend << " | " << std::fixed << std::setprecision(2)
                << prepareStats.mean << " | "
                << cpuStats.mean << " / " << cpuStats.median << " / " << cpuStats.p95 << " / " << cpuStats.p99
                << " | " << gpuStats.mean << " / " << gpuStats.median << " / " << gpuStats.p95 << " / " << gpuStats.p99
                << " | " << gpu.size() << " |\n";
    }

    summary << "\n## Decision gate\n\n";
    if (frameRecords_.empty()) {
        summary << "No performance decision: this was a validation-only run.\n";
    } else if (!cpuMeans.contains("B0") || !gpuMeans.contains("B0")) {
        summary << "No promotion decision: B0 reference timings are absent or invalid.\n";
    } else {
        bool performanceCandidate = false;
        for (const auto& backend : {std::string("B2"), std::string("B3")}) {
            const auto validation = std::find_if(validationRecords_.begin(), validationRecords_.end(),
                                                 [&](const auto& record) { return record.backend == backend; });
            if (validation == validationRecords_.end() || !validation->passed ||
                !cpuMeans.contains(backend) || !gpuMeans.contains(backend)) continue;
            const double cpuSaving = 100.0 * (cpuMeans["B0"] - cpuMeans[backend]) / cpuMeans["B0"];
            const double gpuChange = 100.0 * (gpuMeans[backend] - gpuMeans["B0"]) / gpuMeans["B0"];
            const bool passesPerformance = cpuSaving >= 15.0 && gpuChange <= 5.0;
            summary << "- " << backend << ": CPU submission " << cpuSaving << "% lower; GPU "
                    << (gpuChange >= 0 ? "+" : "") << gpuChange << "%; synthetic performance gate "
                    << (passesPerformance ? "PASS" : "FAIL") << ".\n";
            performanceCandidate = performanceCandidate || passesPerformance;
        }
        if (!performanceCandidate) {
            summary << "\nNo one-call backend meets the synthetic performance threshold in this run.\n";
        } else if (options_.debugLayer) {
            summary << "\nA performance threshold appears satisfied, but debug-layer timings are not promotion evidence. Repeat in release mode and pair by binary/configuration signature.\n";
        } else {
            summary << "\nAt least one backend is a synthetic performance candidate. Promotion still requires a separate matching debug-layer correctness run with zero errors before an allowlisted in-game POC.\n";
        }
    }
    summary << "\n## Diagnostics\n\n"
            << "D3D11 debug errors: " << debugErrors_ << "  \n"
            << "D3D11 debug warnings: " << debugWarnings_ << "  \n"
            << "All raw frame samples, validation metrics, images, capabilities, binary hash, and lifecycle events are retained in this directory.\n\n"
            << "## Boundary\n\n"
            << "This standalone result does not prove Skyrim hook reachability, material compatibility, compositor admission, or real MGO performance. Those require StereoTrace and an allowlisted in-game experiment.\n";
    WriteTextFile(paths_.root / "summary.md", summary.str());
}

}  // namespace stereo_lab
