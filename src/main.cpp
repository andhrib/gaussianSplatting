#include "Globals.h"
#include "Camera.h"
#include "SplatLoader.h"
#include "Benchmark.h"
#include "Screenshot.h"

#include <GameFramework/GameFramework.h>
#include <GameFramework/Window.h>

#include <dx12lib/CommandList.h>
#include <dx12lib/CommandQueue.h>
#include <dx12lib/Helpers.h>

#include <shlwapi.h>      // PathRemoveFileSpecW
#include <d3dcompiler.h>  // D3DReadFileToBlob
#include <dxgidebug.h>    // ReportLiveObjects

// ---------------------------------------------------------------------------
// Global definitions (declared extern in Globals.h)
// ---------------------------------------------------------------------------
std::shared_ptr<Device>              pDevice                     = nullptr;
std::shared_ptr<Window>              pGameWindow                 = nullptr;
std::shared_ptr<SwapChain>           pSwapChain                  = nullptr;
std::shared_ptr<Texture>             pDepthTexture               = nullptr;
std::shared_ptr<RootSignature>       pRootSignature              = nullptr;
std::shared_ptr<PipelineStateObject> pPipelineStateObject        = nullptr;
std::shared_ptr<RootSignature>       pComputeRootSignature       = nullptr;
std::shared_ptr<PipelineStateObject> pComputePipelineStateObject = nullptr;

Logger logger;

std::filesystem::path g_screenshotRoot;

Constants        constants;
ComputeConstants computeConstants;
bool             initialFrame = true;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
void OnUpdate( UpdateEventArgs& e );
void OnKeyPressed( KeyEventArgs& e );
void OnMouseWheel( MouseWheelEventArgs& e );
void OnMouseMoved( MouseMotionEventArgs& e );
void OnResized( ResizeEventArgs& e );
void OnWindowClose( WindowCloseEventArgs& e );
void ChangeScale( bool increase );

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow )
{
    int retCode = 0;

#if defined( _DEBUG )
    Device::EnableDebugLayer();
#endif

    // Capture the project root before we change the working directory, so
    // screenshots land in the project folder rather than the build output dir.
    WCHAR   path[MAX_PATH];
    HMODULE hModule = ::GetModuleHandleW( NULL );
    if ( ::GetModuleFileNameW( hModule, path, MAX_PATH ) > 0 )
    {
        ::PathRemoveFileSpecW( path );
        ::SetCurrentDirectoryW( path );

        g_screenshotRoot = std::filesystem::path( PROJECT_ROOT_DIR );
        std::filesystem::create_directories( g_screenshotRoot / "screenshots" );
    }

    auto& gf = GameFramework::Create( hInstance );
    {
        logger  = gf.CreateLogger( "Splats" );
        pDevice = Device::Create();

        auto description = pDevice->GetDescription();
        logger->info( L"Device Created: {}", description );

        // Load all LOD levels and upload their structured buffers to the GPU.
        LoadAllLods();

        pGameWindow = gf.CreateWindow( L"Splats", 1920, 1080 );
        pSwapChain  = pDevice->CreateSwapChain( pGameWindow->GetWindowHandle() );
        pSwapChain->SetVSync( false );

        pGameWindow->KeyPressed += &OnKeyPressed;
        pGameWindow->MouseWheel += &OnMouseWheel;
        pGameWindow->MouseMoved += &OnMouseMoved;
        pGameWindow->Resize     += &OnResized;
        pGameWindow->Update     += &OnUpdate;
        pGameWindow->Close      += &OnWindowClose;

        // -----------------------------------------------------------------------
        // Graphics root signature  (VS + PS)
        // -----------------------------------------------------------------------
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        CD3DX12_ROOT_PARAMETER1 rootParameters[2];
        rootParameters[0].InitAsConstants( sizeof( Constants ) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX );
        rootParameters[1].InitAsShaderResourceView( 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
                                                    D3D12_SHADER_VISIBILITY_VERTEX );

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription(
            _countof( rootParameters ), rootParameters, 0, nullptr, rootSignatureFlags );

        pRootSignature = pDevice->CreateRootSignature( rootSignatureDescription.Desc_1_1 );

        // -----------------------------------------------------------------------
        // Graphics PSO
        // -----------------------------------------------------------------------
        ComPtr<ID3DBlob> vertexShaderBlob;
        ThrowIfFailed( D3DReadFileToBlob( L"VertexShader.cso", &vertexShaderBlob ) );

        ComPtr<ID3DBlob> pixelShaderBlob;
        ThrowIfFailed( D3DReadFileToBlob( L"PixelShader.cso", &pixelShaderBlob ) );

        struct PipelineStateStream
        {
            CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        pRootSignature;
            CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    PrimitiveTopologyType;
            CD3DX12_PIPELINE_STATE_STREAM_VS                    VS;
            CD3DX12_PIPELINE_STATE_STREAM_PS                    PS;
            CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT  DSVFormat;
            CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
            CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC            BlendState;
            CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL         DepthStencilState;
        } pipelineStateStream;

        pipelineStateStream.pRootSignature        = pRootSignature->GetD3D12RootSignature().Get();
        pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipelineStateStream.VS                    = CD3DX12_SHADER_BYTECODE( vertexShaderBlob.Get() );
        pipelineStateStream.PS                    = CD3DX12_SHADER_BYTECODE( pixelShaderBlob.Get() );
        pipelineStateStream.DSVFormat             = DXGI_FORMAT_D32_FLOAT;
        pipelineStateStream.RTVFormats            = pSwapChain->GetRenderTarget().GetRenderTargetFormats();

        D3D12_RENDER_TARGET_BLEND_DESC blendDesc = {};
        blendDesc.BlendEnable                    = TRUE;
        blendDesc.SrcBlend                       = D3D12_BLEND_SRC_ALPHA;
        blendDesc.DestBlend                      = D3D12_BLEND_INV_SRC_ALPHA;
        blendDesc.BlendOp                        = D3D12_BLEND_OP_ADD;
        blendDesc.SrcBlendAlpha                  = D3D12_BLEND_ONE;
        blendDesc.DestBlendAlpha                 = D3D12_BLEND_ZERO;
        blendDesc.BlendOpAlpha                   = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTargetWriteMask          = D3D12_COLOR_WRITE_ENABLE_ALL;

        CD3DX12_BLEND_DESC blend( D3D12_DEFAULT );
        blend.RenderTarget[0] = blendDesc;
        pipelineStateStream.BlendState = blend;

        CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc( D3D12_DEFAULT );
        depthStencilDesc.DepthWriteMask       = D3D12_DEPTH_WRITE_MASK_ZERO;
        pipelineStateStream.DepthStencilState = depthStencilDesc;

        pPipelineStateObject = pDevice->CreatePipelineStateObject( pipelineStateStream );

        // -----------------------------------------------------------------------
        // Compute root signature  (bitonic sort)
        // -----------------------------------------------------------------------
        D3D12_ROOT_SIGNATURE_FLAGS computeRootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

        CD3DX12_ROOT_PARAMETER1 computeRootParameters[2];
        computeRootParameters[0].InitAsConstants( sizeof( ComputeConstants ) / 4, 0, 0,
                                                  D3D12_SHADER_VISIBILITY_ALL );
        computeRootParameters[1].InitAsUnorderedAccessView( 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
                                                            D3D12_SHADER_VISIBILITY_ALL );

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDescription(
            _countof( computeRootParameters ), computeRootParameters, 0, nullptr, computeRootSignatureFlags );

        pComputeRootSignature = pDevice->CreateRootSignature( computeRootSignatureDescription.Desc_1_1 );

        // -----------------------------------------------------------------------
        // Compute PSO
        // -----------------------------------------------------------------------
        ComPtr<ID3DBlob> computeShaderBlob;
        ThrowIfFailed( D3DReadFileToBlob( L"BitonicSort.cso", &computeShaderBlob ) );

        struct ComputePipelineStateStream
        {
            CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
            CD3DX12_PIPELINE_STATE_STREAM_CS             CS;
        } computePipelineStateStream;

        computePipelineStateStream.pRootSignature = pComputeRootSignature->GetD3D12RootSignature().Get();
        computePipelineStateStream.CS             = CD3DX12_SHADER_BYTECODE( computeShaderBlob.Get() );

        pComputePipelineStateObject = pDevice->CreatePipelineStateObject( computePipelineStateStream );

        // Make sure all LOD buffers are uploaded before the first frame.
        pDevice->GetCommandQueue( D3D12_COMMAND_LIST_TYPE_DIRECT ).Flush();

        pGameWindow->Show();
        retCode = GameFramework::Get().Run();

        // -----------------------------------------------------------------------
        // Teardown
        // -----------------------------------------------------------------------
        pStructuredBuffer.reset();
        for ( auto& buf : pLodBuffers )
            buf.reset();
        pLodBuffers.clear();
        lodSplats.clear();
        lodSplatCounts.clear();
        pActiveSplats = nullptr;
        pPipelineStateObject.reset();
        pRootSignature.reset();
        pComputePipelineStateObject.reset();
        pComputeRootSignature.reset();
        pDepthTexture.reset();
        pDevice.reset();
        pSwapChain.reset();
        pGameWindow.reset();
    }

    GameFramework::Destroy();
    atexit( &Device::ReportLiveObjects );

    return retCode;
}

// ---------------------------------------------------------------------------
// OnUpdate — FPS counter, benchmark tick, LOD selection, sort + render
// ---------------------------------------------------------------------------
void OnUpdate( UpdateEventArgs& e )
{
    static uint64_t frameCount = 0;
    static double   totalTime  = 0.0;

    totalTime += e.DeltaTime;
    frameCount++;

    if ( totalTime > 1.0 )
    {
        auto fps   = frameCount / totalTime;
        frameCount = 0;
        totalTime -= 1.0;

        logger->info( "FPS: {:.7}", fps );
        camera.PrintPosition();

        wchar_t buffer[256];
        ::swprintf_s( buffer, L"Splats [FPS: %f]", fps );
        pGameWindow->SetWindowTitle( buffer );
    }

    bool forceSort = false;

    // -----------------------------------------------------------------------
    // Benchmark tick
    // -----------------------------------------------------------------------
    if ( benchState == BenchmarkState::Warmup || benchState == BenchmarkState::Measuring )
    {
        forceSort = true;

        benchFrameCount++;
        benchTimeAccum += e.DeltaTime;

        if ( benchState == BenchmarkState::Warmup && benchFrameCount >= WARMUP_FRAMES )
        {
            benchFrameCount = 0;
            benchTimeAccum  = 0.0;
            benchState      = BenchmarkState::Measuring;
            logger->info( "Benchmark: measuring {} / {}", DIST_NAMES[benchDistIndex],
                          benchmarkFiles[benchFileIndex].filename().string() );
        }
        else if ( benchState == BenchmarkState::Measuring && benchFrameCount >= MEASURE_FRAMES )
        {
            double fps = benchFrameCount / benchTimeAccum;

            std::error_code ec;
            uintmax_t       fileBytes = std::filesystem::file_size( benchmarkFiles[benchFileIndex], ec );
            if ( ec )
                fileBytes = 0;

            benchCsv << benchmarkFiles[benchFileIndex].filename().string() << ","
                     << DIST_NAMES[benchDistIndex] << ","
                     << currentSplatCount << "," << fileBytes << "," << fps << "\n";
            benchCsv.flush();

            logger->info( "Benchmark result: file={} dist={} splats={} fps={:.2f}",
                          benchmarkFiles[benchFileIndex].filename().string(), DIST_NAMES[benchDistIndex],
                          currentSplatCount, fps );

            std::string stem = benchmarkFiles[benchFileIndex].stem().string();
            pendingScreenshotPath =
                ( g_screenshotRoot / "screenshots" / ( stem + "_" + DIST_NAMES[benchDistIndex] + ".png" ) ).string();
            pendingScreenshot = true;
        }
    }

    // -----------------------------------------------------------------------
    // LOD selection — pick the coarsest LOD whose threshold is still <= radius.
    // Skipped while the benchmark is running (benchmarkLodLocked == true).
    // -----------------------------------------------------------------------
    if ( !benchmarkLodLocked && LOD_COUNT > 1 )
    {
        int desiredLod = 0;
        for ( int i = 1; i < LOD_COUNT; ++i )
        {
            if ( camera.radius >= LOD_THRESHOLDS[i] )
                desiredLod = i;
        }
        SelectLod( desiredLod );
    }

    // -----------------------------------------------------------------------
    // Render
    // -----------------------------------------------------------------------
    auto renderTarget = pSwapChain->GetRenderTarget();
    renderTarget.AttachTexture( AttachmentPoint::DepthStencil, pDepthTexture );

    auto viewport = renderTarget.GetViewport();

    XMMATRIX modelMatrix      = XMMatrixIdentity();
    XMMATRIX viewMatrix       = camera.GetViewMatrix();
    float    aspectRatio      = viewport.Width / viewport.Height;
    XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH( XMConvertToRadians( 45.0f ), aspectRatio, 0.1f, 100.0f );
    constants.MVP             = XMMatrixMultiply( modelMatrix, viewMatrix );
    constants.MVP             = XMMatrixMultiply( constants.MVP, projectionMatrix );

    auto& commandQueue = pDevice->GetCommandQueue( D3D12_COMMAND_LIST_TYPE_DIRECT );

    uint32_t n                 = static_cast<uint32_t>( splats.size() );
    uint32_t groupCount        = n / 2 / THREAD_GROUP_SIZE;
    computeConstants.cameraPos = camera.GetPosition();

    auto commandList = commandQueue.GetCommandList();
    commandList->TransitionBarrier( pStructuredBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
    commandList->FlushResourceBarriers();

    if ( camera.moved || forceSort )
    {
        initialFrame = false;
        camera.moved = false;

        const uint32_t localSize = THREAD_GROUP_SIZE * 2;  // 128 — one workgroup's lane

        commandList->SetPipelineState( pComputePipelineStateObject );
        commandList->SetComputeRootSignature( pComputeRootSignature );
        commandList->SetUnorderedAccessView( 1, std::static_pointer_cast<Buffer>( pStructuredBuffer ) );

        auto dispatch = [&]() {
            commandList->SetCompute32BitConstants( 0, computeConstants );
            commandList->Dispatch( groupCount, 1, 1 );
            commandList->UAVBarrier( pStructuredBuffer );
        };

        // Sort each workgroup's lane independently using only local memory.
        computeConstants.h         = localSize;
        computeConstants.algorithm = 0;  // eLocalBitonicMergeSort
        dispatch();

        for ( uint32_t h = localSize * 2; h <= n; h *= 2 )
        {
            // Flip that crosses workgroup boundaries — must use global memory.
            computeConstants.h         = h;
            computeConstants.algorithm = 2;  // eBigFlip
            dispatch();

            for ( uint32_t hh = h / 2; hh > 1; hh /= 2 )
            {
                if ( hh <= localSize )
                {
                    // Disperse fits within a lane — use local memory.
                    computeConstants.h         = hh;
                    computeConstants.algorithm = 1;  // eLocalDisperse
                    dispatch();
                    break;  // local_disperse handles the full cascade internally
                }
                else
                {
                    // Disperse still crosses boundaries — global memory.
                    computeConstants.h         = hh;
                    computeConstants.algorithm = 3;  // eBigDisperse
                    dispatch();
                }
            }
        }
    }

    commandList->TransitionBarrier( pStructuredBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );
    commandList->FlushResourceBarriers();

    commandList->SetPipelineState( pPipelineStateObject );
    commandList->SetGraphicsRootSignature( pRootSignature );
    commandList->SetGraphics32BitConstants( 0, constants );
    commandList->SetShaderResourceView( 1, std::static_pointer_cast<Buffer>( pStructuredBuffer ) );

    const FLOAT clearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    commandList->ClearTexture( renderTarget.GetTexture( AttachmentPoint::Color0 ), clearColor );
    commandList->ClearDepthStencilTexture( pDepthTexture, D3D12_CLEAR_FLAG_DEPTH );

    commandList->SetRenderTarget( renderTarget );
    commandList->SetViewport( renderTarget.GetViewport() );
    commandList->SetScissorRect( CD3DX12_RECT( 0, 0, LONG_MAX, LONG_MAX ) );

    commandList->SetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    commandList->Draw( splats.size() * 6 );

    commandQueue.ExecuteCommandList( commandList );

    // Capture before Present() advances the back buffer index.
    std::shared_ptr<Texture> screenshotTex;
    if ( pendingScreenshot )
        screenshotTex = pSwapChain->GetRenderTarget().GetTexture( AttachmentPoint::Color0 );

    pSwapChain->Present();

    if ( pendingScreenshot )
    {
        pendingScreenshot = false;
        SaveScreenshot( pendingScreenshotPath, screenshotTex );
        AdvanceBenchmark();
    }
}

// ---------------------------------------------------------------------------
// Input / window event handlers
// ---------------------------------------------------------------------------
void ChangeScale( bool increase )
{
    if ( increase )
        constants.s = std::min( 0.02f, constants.s + 0.001f );
    else
        constants.s = std::max( 0.001f, constants.s - 0.001f );
}

void OnKeyPressed( KeyEventArgs& e )
{
    switch ( e.Key )
    {
    case KeyCode::V:
        pSwapChain->ToggleVSync();
        break;
    case KeyCode::B:
        if ( benchState == BenchmarkState::Interactive || benchState == BenchmarkState::Done )
            StartBenchmark();
        break;
    case KeyCode::Escape:
        GameFramework::Get().Stop();
        break;
    case KeyCode::Enter:
        if ( e.Alt )
        {
            [[fallthrough]];
        case KeyCode::F11:
            pGameWindow->ToggleFullscreen();
            break;
        }
    case KeyCode::W:
        ChangeScale( true );
        break;
    case KeyCode::S:
        ChangeScale( false );
        break;
    }
}

void OnMouseWheel( MouseWheelEventArgs& e )
{
    camera.OnMouseScroll( e.WheelDelta );
}

void OnMouseMoved( MouseMotionEventArgs& e )
{
    if ( e.LeftButton )
        camera.OnMouseDrag( static_cast<float>( e.RelX ), static_cast<float>( e.RelY ) );
}

void OnResized( ResizeEventArgs& e )
{
    logger->info( "Window Resize: {}, {}", e.Width, e.Height );
    GameFramework::Get().SetDisplaySize( e.Width, e.Height );

    pDevice->Flush();
    pSwapChain->Resize( e.Width, e.Height );

    auto depthTextureDesc  = CD3DX12_RESOURCE_DESC::Tex2D( DXGI_FORMAT_D32_FLOAT, e.Width, e.Height );
    depthTextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optimizedClearValue      = {};
    optimizedClearValue.Format                 = DXGI_FORMAT_D32_FLOAT;
    optimizedClearValue.DepthStencil           = { 1.0F, 0 };

    pDepthTexture = pDevice->CreateTexture( depthTextureDesc, &optimizedClearValue );
}

void OnWindowClose( WindowCloseEventArgs& e )
{
    GameFramework::Get().Stop();
}
