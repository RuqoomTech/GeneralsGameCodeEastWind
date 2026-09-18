// Command & Conquer Generals Evolution - Direct3D 12 WW3D bootstrap shader.
//
// This is the first canonical HLSL shader in the migrated renderer. The
// legacy GameEngineDevice .nvp/.nvv shader-assembly sources remain reference
// inputs until their actual terrain/filter/tree/water paths are migrated.

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = float4(input.position, 1.0f);
    output.color = input.color;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
