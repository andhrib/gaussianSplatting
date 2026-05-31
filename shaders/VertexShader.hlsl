struct Constants
{
    matrix MVP;
    float s;
};

ConstantBuffer<Constants> ConstantsCB : register(b0);

struct Splat
{
    float4 position;
    float4 scale;
    float4 color;
    float4 rotation;
};

StructuredBuffer<Splat> splats : register(t0);

struct VertexShaderOutput
{
	float4 color    : COLOR;
    float2 offset   : OFFSET;
    float falloff   : FALLOFF;
    float4 position : SV_Position;
};

static float2 corners[6] =
{
    float2(-1, -1), float2(-1, 1), float2(1, -1), // triangle 1
    float2(1, -1), float2(-1, 1), float2(1, 1) // triangle 2
};

VertexShaderOutput main(uint vertexID : SV_VertexID)
{
    uint splatIndex = vertexID / 6;
    uint cornerIndex = vertexID % 6;

    Splat splat = splats[splatIndex];

    // Transform center to clip space
    float4 center = mul(ConstantsCB.MVP, splat.position);
    
    float2 offset = corners[cornerIndex] * (ConstantsCB.s);
    center.xy += offset;

    VertexShaderOutput output;
    output.position = center;
    output.color = splat.color;
    output.offset = corners[cornerIndex];
    output.falloff = ConstantsCB.s;
    
    return output;
}