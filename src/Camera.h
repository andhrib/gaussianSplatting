#pragma once

#include <DirectXMath.h>
#include <algorithm>

using namespace DirectX;

struct OrbitCamera
{
    float    azimuth   = -2.91f;       // horizontal angle in radians
    float    elevation = -1.18f;       // vertical angle in radians
    float    radius    = 3.0f;         // distance from target
    XMFLOAT3 target    = { 0, 0, 0 };  // point to look at
    bool     moved     = true;

    XMMATRIX GetViewMatrix() const;
    XMFLOAT3 GetPosition() const;
    void     OnMouseDrag( float dx, float dy );
    void     OnMouseScroll( float delta );
    void     PrintPosition() const;
};

extern OrbitCamera camera;
