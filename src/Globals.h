#pragma once

#include <GameFramework/GameFramework.h>
#include <GameFramework/Window.h>

#include <dx12lib/Device.h>
#include <dx12lib/PipelineStateObject.h>
#include <dx12lib/RootSignature.h>
#include <dx12lib/StructuredBuffer.h>
#include <dx12lib/SwapChain.h>
#include <dx12lib/Texture.h>

#include <DirectXMath.h>

#include <filesystem>
#include <memory>
#include <vector>

using namespace dx12lib;
using namespace DirectX;
using namespace Microsoft::WRL;

static const std::filesystem::path LOD_FILES[]      = { "plush.splat", "plush_100pct_m0.005.splat", "plush_100pct_m0.01.splat", "plush_100pct_m0.015.splat" };
static const float LOD_THRESHOLDS[] = { 0.0f, 1.0f, 3.0f, 9.0f };
static_assert( _countof( LOD_FILES ) == _countof( LOD_THRESHOLDS ),
               "LOD_FILES and LOD_THRESHOLDS must have the same number of entries." );
static const int LOD_COUNT = static_cast<int>( _countof( LOD_FILES ) );

// ---------------------------------------------------------------------------
// GPU-side constant buffer layouts
// ---------------------------------------------------------------------------
struct Constants
{
    XMMATRIX MVP;
    float    s = 0.014f;
    float    padding[3];
};

struct ComputeConstants
{
    XMFLOAT3 cameraPos;
    uint32_t h;
    uint32_t algorithm;
    float    padding[3];
};

// ---------------------------------------------------------------------------
// Splat data layouts
// ---------------------------------------------------------------------------
struct SplatRaw
{
    XMFLOAT3 position;     // 3 x float32
    XMFLOAT3 scale;        // 3 x float32
    uint8_t  color[4];     // RGBA uint8
    uint8_t  rotation[4];  // quaternion uint8
};

struct Splat
{
    XMFLOAT4 position;
    XMFLOAT4 scale;     // w = 0, not transformed by MVP
    XMFLOAT4 color;     // decoded to [0,1] on load
    XMFLOAT4 rotation;  // decoded to [-1,1] on load
};

// ---------------------------------------------------------------------------
// Globals — defined in main.cpp, declared here for all other translation units
// ---------------------------------------------------------------------------
extern std::shared_ptr<Device>              pDevice;
extern std::shared_ptr<Window>              pGameWindow;
extern std::shared_ptr<SwapChain>           pSwapChain;
extern std::shared_ptr<Texture>             pDepthTexture;
extern std::shared_ptr<RootSignature>       pRootSignature;
extern std::shared_ptr<PipelineStateObject> pPipelineStateObject;
extern std::shared_ptr<RootSignature>       pComputeRootSignature;
extern std::shared_ptr<PipelineStateObject> pComputePipelineStateObject;

extern Logger logger;

extern std::filesystem::path g_screenshotRoot;

// LOD runtime state — defined in SplatLoader.cpp
extern std::vector<std::shared_ptr<StructuredBuffer>> pLodBuffers;
extern std::vector<std::vector<Splat>>                lodSplats;
extern std::vector<size_t>                            lodSplatCounts;
extern int                                            activeLod;
extern std::shared_ptr<StructuredBuffer>              pStructuredBuffer;
extern std::filesystem::path                          SPLAT_FILE_PATH;

// Active splat data aliases — updated by SelectLod()
extern std::vector<Splat>* pActiveSplats;
extern size_t              currentSplatCount;

// Convenience macro so all existing uses of `splats` work unchanged.
#define splats ( *pActiveSplats )

extern Constants        constants;
extern ComputeConstants computeConstants;
extern bool             initialFrame;

static const uint32_t THREAD_GROUP_SIZE = 64;
static const uint32_t MAX_THREAD_GROUPS = 1 << 18;
