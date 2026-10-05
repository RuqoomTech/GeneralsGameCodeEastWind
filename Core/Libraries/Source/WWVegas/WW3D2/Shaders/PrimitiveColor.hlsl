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
    float3 texcoord : TEXCOORD0;
    float2 secondaryTexcoord : TEXCOORD1;
};

struct TexturedPSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
    float3 texcoord : TEXCOORD0;
    float2 secondaryTexcoord : TEXCOORD1;
};

Texture2D PrimitiveTexture : register(t0);
SamplerState PrimitiveSampler : register(s0);

TexturedPSInput VSTextured(TexturedVSInput input)
{
    TexturedPSInput output;
    output.position = mul(ViewProjection, float4(input.position, 1.0f));
    output.color = input.color;
    output.texcoord = input.texcoord;
    output.secondaryTexcoord = input.secondaryTexcoord;
    return output;
}

float4 PSTextured(TexturedPSInput input) : SV_TARGET
{
    return PrimitiveTexture.Sample(PrimitiveSampler, input.texcoord.xy) * input.color;
}

// The real decal caller clamps to its edge and explicitly disables mipmaps.
SamplerState DecalSampler : register(s1);
SamplerState MaterialSampler : register(s2);
Texture2D SecondaryTexture : register(t1);
SamplerState SecondarySampler : register(s3);

cbuffer MaterialParameters : register(b1)
{
    uint TextureCombine;
    uint AlphaTest;
    float AlphaReference;
    uint ClampTexture;
    uint SecondaryRGBModulate;
    uint TerrainLayers;
    uint2 MaterialPadding;
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
    float2 coordinates = input.texcoord.xy / input.texcoord.z;
    textureColor = PrimitiveTexture.Sample(MaterialSampler, coordinates);

    float4 color = textureColor;
    if (TextureCombine == 1) color *= input.color;
    // W3D additive and 2X color operations both multiply alpha normally.
    if (TextureCombine == 2)
        color = float4(saturate(textureColor.rgb + input.color.rgb), textureColor.a * input.color.a);
    if (TextureCombine == 3)
        color = float4(saturate(2.0f * textureColor.rgb * input.color.rgb), textureColor.a * input.color.a);
    if (SecondaryRGBModulate != 0)
        color.rgb *= SecondaryTexture.Sample(SecondarySampler, input.secondaryTexcoord).rgb;
    return TestMaterialAlpha(color);
}

Texture2D TerrainCloudTexture : register(t2);
Texture2D TerrainNoiseTexture : register(t3);
SamplerState TerrainCloudSampler : register(s4);
SamplerState TerrainNoiseSampler : register(s5);

struct TerrainVSInput
{
    float3 position : POSITION;
    float4 color : COLOR0;
    float2 tileTexcoord : TEXCOORD0;
    float2 shroudTexcoord : TEXCOORD1;
    float2 cloudTexcoord : TEXCOORD2;
    float2 noiseTexcoord : TEXCOORD3;
};

struct TerrainPSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
    float2 tileTexcoord : TEXCOORD0;
    float2 shroudTexcoord : TEXCOORD1;
    float2 cloudTexcoord : TEXCOORD2;
    float2 noiseTexcoord : TEXCOORD3;
};

TerrainPSInput VSTerrain(TerrainVSInput input)
{
    TerrainPSInput output;
    output.position = mul(ViewProjection, float4(input.position, 1.0f));
    output.color = input.color;
    output.tileTexcoord = input.tileTexcoord;
    output.shroudTexcoord = input.shroudTexcoord;
    output.cloudTexcoord = input.cloudTexcoord;
    output.noiseTexcoord = input.noiseTexcoord;
    return output;
}

float4 PSTerrainColor(TerrainPSInput input) : SV_TARGET
{
    return TestMaterialAlpha(input.color);
}

// fterrain*.nvp: tile, optional shroud, diffuse, optional cloud, optional noise.
// Keep the register arithmetic unsaturated until the output target converts it.
float4 PSTerrainTexture(TerrainPSInput input) : SV_TARGET
{
    float4 color = PrimitiveTexture.Sample(MaterialSampler, input.tileTexcoord);
    if ((TerrainLayers & 1) != 0)
        color *= SecondaryTexture.Sample(SecondarySampler, input.shroudTexcoord);
    color *= input.color;
    if ((TerrainLayers & 2) != 0)
        color *= TerrainCloudTexture.Sample(TerrainCloudSampler, input.cloudTexcoord);
    if ((TerrainLayers & 4) != 0)
        color *= TerrainNoiseTexture.Sample(TerrainNoiseSampler, input.noiseTexcoord);
    return TestMaterialAlpha(color);
}
