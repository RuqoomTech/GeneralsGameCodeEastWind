// Command & Conquer Generals Evolution - Direct3D 12 WW3D bootstrap shaders.
//
// This file is the canonical first-stage shader source for the migrated
// renderer. Legacy GameEngineDevice .nvp/.nvv assembly programs remain
// reference inputs until each real terrain/filter/tree/water path is moved
// together with its resource bindings and render state.

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

struct TexturedVSInput
{
    float3 position : POSITION;
    float4 color : COLOR0;
    float2 texcoord : TEXCOORD0;
};

struct TexturedPSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
    float2 texcoord : TEXCOORD0;
};

Texture2D PrimitiveTexture : register(t0);
SamplerState PrimitiveSampler : register(s0);

TexturedPSInput VSTextured(TexturedVSInput input)
{
    TexturedPSInput output;
    output.position = float4(input.position, 1.0f);
    output.color = input.color;
    output.texcoord = input.texcoord;
    return output;
}

float4 PSTextured(TexturedPSInput input) : SV_TARGET
{
    return PrimitiveTexture.Sample(PrimitiveSampler, input.texcoord) * input.color;
}
