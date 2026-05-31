struct PixelShaderInput
{
    float4 color  : COLOR;
    float2 offset : OFFSET;
    float falloff : FALLOFF;
};

float4 main( PixelShaderInput IN ) : SV_Target
{
    float dist2 = dot(IN.offset, IN.offset);
    float gaussian = exp(-0.5 * dist2 * IN.falloff);
    float alpha = IN.color.a * gaussian;
    return float4(IN.color.rgb, alpha);
}