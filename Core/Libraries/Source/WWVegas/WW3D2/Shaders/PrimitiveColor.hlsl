// Command & Conquer Generals Evolution - Direct3D 12 WW3D bootstrap shaders.
//
// This file is the canonical first-stage shader source for the migrated
// renderer. Legacy GameEngineDevice .nvp/.nvv assembly programs remain
// reference inputs until each real terrain/filter/tree/water path is moved
// together with its resource bindings and render state.

cbuffer CameraTransform : register(b0)
{
    row_major float4x4 ViewProjection;
};

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
    output.position = mul(ViewProjection, float4(input.position, 1.0f));
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
    output.position = mul(ViewProjection, float4(input.position, 1.0f));
    output.color = input.color;
    output.texcoord = input.texcoord;
    return output;
}

float4 PSTextured(TexturedPSInput input) : SV_TARGET
{
    return PrimitiveTexture.Sample(PrimitiveSampler, input.texcoord) * input.color;
}

// The real decal caller clamps to its edge and explicitly disables mipmaps.
SamplerState DecalSampler : register(s1);

cbuffer MaterialParameters : register(b1)
{
    uint TextureCombine;
    uint AlphaTest;
    float AlphaReference;
    uint ClampTexture;
};

float4 TestMaterialAlpha(float4 color)
{
    if (AlphaTest == 1) clip(color.a - AlphaReference);
    if (AlphaTest == 2) clip(AlphaReference - color.a);
    return color;
}

float4 PSMaterialColor(TexturedPSInput input) : SV_TARGET
{
    return TestMaterialAlpha(input.color);
}

float4 PSMaterialTexture(TexturedPSInput input) : SV_TARGET
{
    float4 textureColor;
    if (ClampTexture != 0)
        textureColor = PrimitiveTexture.SampleLevel(DecalSampler, input.texcoord, 0.0f);
    else
        textureColor = PrimitiveTexture.Sample(PrimitiveSampler, input.texcoord);

    float4 color = textureColor;
    if (TextureCombine == 1) color *= input.color;
    // W3D additive and 2X color operations both multiply alpha normally.
    if (TextureCombine == 2)
        color = float4(saturate(textureColor.rgb + input.color.rgb), textureColor.a * input.color.a);
    if (TextureCombine == 3)
        color = float4(saturate(2.0f * textureColor.rgb * input.color.rgb), textureColor.a * input.color.a);
    return TestMaterialAlpha(color);
}
