#include "SplatLoader.h"
#include "Globals.h"
#include "Camera.h"

#include <dx12lib/CommandQueue.h>
#include <dx12lib/CommandList.h>

#include <fstream>
#include <stdexcept>

// ---------------------------------------------------------------------------
// LOD runtime state
// ---------------------------------------------------------------------------
std::vector<std::shared_ptr<StructuredBuffer>> pLodBuffers;
std::vector<std::vector<Splat>>                lodSplats;
std::vector<size_t>                            lodSplatCounts;
int                                            activeLod         = 0;
std::shared_ptr<StructuredBuffer>              pStructuredBuffer = nullptr;
std::filesystem::path                          SPLAT_FILE_PATH;

std::vector<Splat>* pActiveSplats    = nullptr;
size_t              currentSplatCount = 0;

// ---------------------------------------------------------------------------

static size_t nextPowerOfTwo( size_t n )
{
    size_t p = 1;
    while ( p < n )
        p <<= 1;
    return p;
}

std::vector<Splat> LoadSplats( const std::filesystem::path& path, size_t* outNumSplats )
{
    std::ifstream file( path, std::ios::binary | std::ios::ate );
    if ( !file )
        throw std::runtime_error( "Failed to open splat file" );

    size_t fileSize = file.tellg();
    file.seekg( 0 );

    size_t                numSplats = fileSize / sizeof( SplatRaw );
    std::vector<SplatRaw> splatsRaw( numSplats );

    file.read( reinterpret_cast<char*>( splatsRaw.data() ), fileSize );

    std::vector<Splat> result( nextPowerOfTwo( numSplats ) );

    for ( size_t i = 0; i < numSplats; i++ )
    {
        result[i].position = XMFLOAT4( splatsRaw[i].position.x, splatsRaw[i].position.y, splatsRaw[i].position.z,
                                       1.0f  // w = 1 for correct matrix multiplication
        );
        result[i].scale    = XMFLOAT4( splatsRaw[i].scale.x, splatsRaw[i].scale.y, splatsRaw[i].scale.z,
                                       0.0f  // w = 0, not transformed by MVP
        );
        result[i].color    = XMFLOAT4( static_cast<float>( splatsRaw[i].color[0] ) / 255.0f,
                                       static_cast<float>( splatsRaw[i].color[1] ) / 255.0f,
                                       static_cast<float>( splatsRaw[i].color[2] ) / 255.0f,
                                       static_cast<float>( splatsRaw[i].color[3] ) / 255.0f );
        result[i].rotation = XMFLOAT4( ( static_cast<float>( splatsRaw[i].rotation[0] ) - 128.0f ) / 128.0f,
                                       ( static_cast<float>( splatsRaw[i].rotation[1] ) - 128.0f ) / 128.0f,
                                       ( static_cast<float>( splatsRaw[i].rotation[2] ) - 128.0f ) / 128.0f,
                                       ( static_cast<float>( splatsRaw[i].rotation[3] ) - 128.0f ) / 128.0f );
    }

    if ( outNumSplats )
        *outNumSplats = numSplats;

    return result;
}

void LoadAllLods()
{
    pLodBuffers.resize( LOD_COUNT );
    lodSplats.resize( LOD_COUNT );
    lodSplatCounts.resize( LOD_COUNT, 0 );

    if ( LOD_COUNT > 0 )
        SPLAT_FILE_PATH = LOD_FILES[0];

    auto& copyQueue = pDevice->GetCommandQueue( D3D12_COMMAND_LIST_TYPE_COPY );
    auto  cmdList   = copyQueue.GetCommandList();

    for ( int i = 0; i < LOD_COUNT; ++i )
    {
        size_t n         = 0;
        lodSplats[i]     = LoadSplats( LOD_FILES[i], &n );
        lodSplatCounts[i] = n;

        pLodBuffers[i] = cmdList->CopyStructuredBuffer( lodSplats[i].size(), sizeof( Splat ), lodSplats[i].data() );

        // Centre the camera on the finest (LOD 0) geometry only.
        if ( i == 0 )
        {
            XMFLOAT3 center = { 0, 0, 0 };
            for ( size_t s = 0; s < n; ++s )
            {
                center.x += lodSplats[0][s].position.x;
                center.y += lodSplats[0][s].position.y;
                center.z += lodSplats[0][s].position.z;
            }
            if ( n > 0 )
                center = XMFLOAT3( center.x / n, center.y / n, center.z / n );
            camera.target = center;
        }
    }

    copyQueue.ExecuteCommandList( cmdList );
    copyQueue.Flush();

    // Point active aliases at LOD 0.
    activeLod         = 0;
    pActiveSplats     = ( LOD_COUNT > 0 ) ? &lodSplats[0] : nullptr;
    pStructuredBuffer = ( LOD_COUNT > 0 ) ? pLodBuffers[0] : nullptr;
    currentSplatCount = ( LOD_COUNT > 0 ) ? lodSplatCounts[0] : 0;
}

void SelectLod( int index )
{
    if ( index < 0 || index >= LOD_COUNT )
        return;
    if ( index == activeLod )
        return;

    activeLod         = index;
    pActiveSplats     = &lodSplats[index];
    pStructuredBuffer = pLodBuffers[index];
    currentSplatCount = lodSplatCounts[index];
    camera.moved      = true;  // trigger re-sort for the new buffer

    logger->info( "LOD switched to {} ({} splats)", index, currentSplatCount );
}

void ReloadSplats( const std::filesystem::path& path )
{
    pDevice->Flush();

    size_t n          = 0;
    lodSplats[0]      = LoadSplats( path, &n );
    lodSplatCounts[0] = n;

    auto& copyQueue = pDevice->GetCommandQueue( D3D12_COMMAND_LIST_TYPE_COPY );
    auto  cmdList   = copyQueue.GetCommandList();
    pLodBuffers[0]  = cmdList->CopyStructuredBuffer( lodSplats[0].size(), sizeof( Splat ), lodSplats[0].data() );
    copyQueue.ExecuteCommandList( cmdList );
    copyQueue.Flush();

    // Keep the active alias in sync (benchmark always runs on LOD 0).
    pActiveSplats     = &lodSplats[0];
    pStructuredBuffer = pLodBuffers[0];
    currentSplatCount = lodSplatCounts[0];

    camera.moved = true;
}
