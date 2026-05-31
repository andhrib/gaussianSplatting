#include "Camera.h"
#include "Globals.h"  // for logger

OrbitCamera camera;

XMMATRIX OrbitCamera::GetViewMatrix() const
{
    XMFLOAT3 position = GetPosition();

    XMVECTOR eye   = XMVectorSet( position.x, position.y, position.z, 1.0f );
    XMVECTOR focus = XMLoadFloat3( &target );
    XMVECTOR up    = XMVectorSet( 0, 1, 0, 0 );

    return XMMatrixLookAtLH( eye, focus, up );
}

XMFLOAT3 OrbitCamera::GetPosition() const
{
    float x = target.x + radius * cosf( elevation ) * sinf( azimuth );
    float y = target.y + radius * sinf( elevation );
    float z = target.z + radius * cosf( elevation ) * cosf( azimuth );

    return XMFLOAT3( x, y, z );
}

void OrbitCamera::OnMouseDrag( float dx, float dy )
{
    azimuth += dx * 0.005f;
    elevation += dy * 0.005f;

    // Clamp elevation to avoid flipping upside down
    elevation = std::clamp( elevation, -XM_PIDIV2 + 0.01f, XM_PIDIV2 - 0.01f );

    moved = true;
}

void OrbitCamera::OnMouseScroll( float delta )
{
    radius -= delta * 0.5f;
    radius = std::max( 0.1f, radius );  // prevent going through the target
    moved  = true;
}

void OrbitCamera::PrintPosition() const
{
    logger->info( "Azimuth: {:.2f}, Elevation: {:.2f}", azimuth, elevation );
}
