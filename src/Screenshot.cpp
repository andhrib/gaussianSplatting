#include "Screenshot.h"
#include "Globals.h"

#include <dx12lib/CommandQueue.h>
#include <dx12lib/CommandList.h>
#include <dx12lib/Helpers.h>
#include <dx12lib/Texture.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void SaveScreenshot( const std::string& filename, std::shared_ptr<dx12lib::Texture> colorTex )
{
    auto srcResource = colorTex->GetD3D12Resource();

    D3D12_RESOURCE_DESC texDesc = srcResource->GetDesc();
    UINT                width   = static_cast<UINT>( texDesc.Width );
    UINT                height  = texDesc.Height;

    // Ask D3D12 how large a buffer we need for this texture layout.
    ComPtr<ID3D12Device> d3dDevice;
    srcResource->GetDevice( IID_PPV_ARGS( &d3dDevice ) );

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint  = {};
    UINT                               numRows    = 0;
    UINT64                             rowBytes   = 0;
    UINT64                             totalBytes = 0;
    d3dDevice->GetCopyableFootprints( &texDesc, 0, 1, 0, &footprint, &numRows, &rowBytes, &totalBytes );

    // Create a CPU-readable readback buffer.
    ComPtr<ID3D12Resource> readbackBuffer;
    {
        CD3DX12_HEAP_PROPERTIES heapProps( D3D12_HEAP_TYPE_READBACK );
        CD3DX12_RESOURCE_DESC   bufDesc = CD3DX12_RESOURCE_DESC::Buffer( totalBytes );
        ThrowIfFailed( d3dDevice->CreateCommittedResource( &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                                           D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                           IID_PPV_ARGS( &readbackBuffer ) ) );
    }

    // Transition → copy → transition back, all on the direct queue.
    auto& directQueue = pDevice->GetCommandQueue( D3D12_COMMAND_LIST_TYPE_DIRECT );
    auto  cmdList     = directQueue.GetCommandList();

    cmdList->TransitionBarrier( srcResource, D3D12_RESOURCE_STATE_COPY_SOURCE );
    cmdList->FlushResourceBarriers();

    D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
    srcLoc.pResource                   = srcResource.Get();
    srcLoc.Type                        = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    srcLoc.SubresourceIndex            = 0;

    D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
    dstLoc.pResource                   = readbackBuffer.Get();
    dstLoc.Type                        = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dstLoc.PlacedFootprint             = footprint;

    // No wrapper for texture→buffer copy; use the raw command list directly.
    cmdList->GetD3D12CommandList()->CopyTextureRegion( &dstLoc, 0, 0, 0, &srcLoc, nullptr );

    cmdList->TransitionBarrier( srcResource, D3D12_RESOURCE_STATE_PRESENT );
    cmdList->FlushResourceBarriers();

    // Stall until copy is done (one-off, acceptable).
    auto fence = directQueue.ExecuteCommandList( cmdList );
    directQueue.WaitForFenceValue( fence );

    // Map, copy rows (row pitch may be padded by D3D12), swap B↔R, write PNG.
    void* mapped = nullptr;
    ThrowIfFailed( readbackBuffer->Map( 0, nullptr, &mapped ) );

    std::vector<uint8_t> pixels( width * height * 4 );
    for ( UINT row = 0; row < height; ++row )
    {
        const uint8_t* src =
            reinterpret_cast<const uint8_t*>( mapped ) + static_cast<size_t>( row ) * footprint.Footprint.RowPitch;
        uint8_t* dst = pixels.data() + static_cast<size_t>( row ) * width * 4;

        for ( UINT col = 0; col < width; ++col )
        {
            if ( texDesc.Format == DXGI_FORMAT_R10G10B10A2_UNORM )
            {
                uint32_t pixel       = *reinterpret_cast<const uint32_t*>( src + col * 4 );
                dst[col * 4 + 0]     = ( pixel & 0x3FF ) >> 2;            // R
                dst[col * 4 + 1]     = ( ( pixel >> 10 ) & 0x3FF ) >> 2;  // G
                dst[col * 4 + 2]     = ( ( pixel >> 20 ) & 0x3FF ) >> 2;  // B
                dst[col * 4 + 3]     = 255;
            }
            else if ( texDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM )
            {
                dst[col * 4 + 0] = src[col * 4 + 2];  // R  (swap B↔R)
                dst[col * 4 + 1] = src[col * 4 + 1];  // G
                dst[col * 4 + 2] = src[col * 4 + 0];  // B
                dst[col * 4 + 3] = 255;
            }
            else
            {
                dst[col * 4 + 0] = src[col * 4 + 0];  // R
                dst[col * 4 + 1] = src[col * 4 + 1];  // G
                dst[col * 4 + 2] = src[col * 4 + 2];  // B
                dst[col * 4 + 3] = 255;
            }
        }
    }

    D3D12_RANGE emptyRange = { 0, 0 };
    readbackBuffer->Unmap( 0, &emptyRange );

    stbi_write_png( filename.c_str(), static_cast<int>( width ), static_cast<int>( height ), 4, pixels.data(),
                    static_cast<int>( width ) * 4 );

    logger->info( "Screenshot: {}", filename );
}
