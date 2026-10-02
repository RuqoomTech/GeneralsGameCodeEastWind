#include "WW3D2/shader.h"
#include "WW3D2/IRenderBackend.h"

#include <array>
#include <cstdint>
#include <iostream>

static_assert(sizeof(unsigned int) == sizeof(std::uint32_t));
static_assert(sizeof(ShaderClass) == sizeof(std::uint32_t));
static_assert(SHIFT_DEPTHCOMPARE == 0 && SHIFT_DEPTHMASK == 3 && SHIFT_COLORMASK == 4);
static_assert(SHIFT_DSTBLEND == 5 && SHIFT_FOG == 8 && SHIFT_PRIGRADIENT == 10);
static_assert(SHIFT_SECGRADIENT == 13 && SHIFT_SRCBLEND == 14 && SHIFT_TEXTURING == 16);
static_assert(SHIFT_NPATCHENABLE == 17 && SHIFT_ALPHATEST == 18 && SHIFT_CULLMODE == 19);
static_assert(SHIFT_POSTDETAILCOLORFUNC == 20 && SHIFT_POSTDETAILALPHAFUNC == 24);

namespace {
int failures = 0;

void Check(bool condition, const char *description)
{
    if (!condition) {
        std::cerr << description << '\n';
        ++failures;
    }
}

bool Equal(const RenderBackendMaterialState &a, const RenderBackendMaterialState &b)
{
    return a.depth_test == b.depth_test && a.source_blend == b.source_blend &&
        a.destination_blend == b.destination_blend && a.cull == b.cull &&
        a.texture_combine == b.texture_combine && a.alpha_test == b.alpha_test &&
        a.alpha_reference == b.alpha_reference && a.depth_write == b.depth_write &&
        a.color_write == b.color_write && a.clamp_texture == b.clamp_texture;
}

void Reject(const ShaderClass &shader, const char *description)
{
    RenderBackendMaterialState state;
    state.depth_test = RenderBackendDepthTest::Greater;
    state.source_blend = RenderBackendBlendFactor::SourceColor;
    state.destination_blend = RenderBackendBlendFactor::InverseSourceAlpha;
    state.cull = RenderBackendCullMode::None;
    state.texture_combine = RenderBackendTextureCombine::Add;
    state.alpha_test = RenderBackendAlphaTest::LessEqual;
    state.alpha_reference = 0.25f;
    state.depth_write = false;
    state.color_write = false;
    state.clamp_texture = true;
    const auto original = state;
    const auto bits = shader.Get_Bits();
    Check(!shader.Get_Render_Backend_State(state), description);
    Check(Equal(state, original), "Rejected shader changed the caller's draw state");
    Check(shader.Get_Bits() == bits, "Rejected shader changed fixed asset bits");
}

void Test_Depth_And_Blending()
{
    const std::array depths = {
        RenderBackendDepthTest::Never, RenderBackendDepthTest::Less,
        RenderBackendDepthTest::Equal, RenderBackendDepthTest::LessEqual,
        RenderBackendDepthTest::Greater, RenderBackendDepthTest::NotEqual,
        RenderBackendDepthTest::GreaterEqual, RenderBackendDepthTest::Always};
    const std::array sources = {
        RenderBackendBlendFactor::Zero, RenderBackendBlendFactor::One,
        RenderBackendBlendFactor::SourceAlpha, RenderBackendBlendFactor::InverseSourceAlpha};
    const std::array destinations = {
        RenderBackendBlendFactor::Zero, RenderBackendBlendFactor::One,
        RenderBackendBlendFactor::SourceColor, RenderBackendBlendFactor::InverseSourceColor,
        RenderBackendBlendFactor::SourceAlpha, RenderBackendBlendFactor::InverseSourceAlpha};
    for (unsigned depth = 0; depth < depths.size(); ++depth) {
        for (unsigned source = 0; source < sources.size(); ++source) {
            for (unsigned destination = 0; destination < destinations.size(); ++destination) {
                ShaderClass shader;
                shader.Set_Depth_Compare(static_cast<ShaderClass::DepthCompareType>(depth));
                shader.Set_Src_Blend_Func(static_cast<ShaderClass::SrcBlendFuncType>(source));
                shader.Set_Dst_Blend_Func(static_cast<ShaderClass::DstBlendFuncType>(destination));
                const auto bits = shader.Get_Bits();
                RenderBackendMaterialState state;
                Check(shader.Get_Render_Backend_State(state), "Valid depth/blend combination rejected");
                Check(state.depth_test == depths[depth], "Depth comparison changed");
                Check(state.source_blend == sources[source], "Source blend changed");
                Check(state.destination_blend == destinations[destination], "Destination blend changed");
                Check(shader.Get_Bits() == bits, "Translation changed fixed shader asset bits");
            }
        }
    }
    for (unsigned destination = 6; destination < 8; ++destination) {
        ShaderClass shader;
        shader.Set_Dst_Blend_Func(static_cast<ShaderClass::DstBlendFuncType>(destination));
        Reject(shader, "Reserved destination blend accepted");
    }
}

void Test_Alpha_And_Writes()
{
    for (unsigned source = 0; source < ShaderClass::SRCBLEND_MAX; ++source) {
        ShaderClass shader;
        shader.Set_Src_Blend_Func(static_cast<ShaderClass::SrcBlendFuncType>(source));
        RenderBackendMaterialState state;
        Check(shader.Get_Render_Backend_State(state), "Alpha-disabled shader rejected");
        Check(state.alpha_test == RenderBackendAlphaTest::Disabled, "Disabled alpha test enabled");
        shader.Set_Alpha_Test(ShaderClass::ALPHATEST_ENABLE);
        Check(shader.Get_Render_Backend_State(state), "Alpha-tested shader rejected");
        const bool inverse = source == ShaderClass::SRCBLEND_ONE_MINUS_SRC_ALPHA;
        Check(state.alpha_test == (inverse ? RenderBackendAlphaTest::LessEqual :
            RenderBackendAlphaTest::GreaterEqual), "Alpha comparison direction changed");
        Check(state.alpha_reference == (inverse ? 159.0f : 96.0f) / 255.0f,
            "Historical alpha-test reference changed");
        shader.Set_Alpha_Test(ShaderClass::ALPHATEST_DISABLE);
        Check(shader.Get_Render_Backend_State(state) && state.alpha_test == RenderBackendAlphaTest::Disabled,
            "Disabling alpha testing retained a previous comparison");
    }
    for (unsigned depth_write = 0; depth_write < 2; ++depth_write) {
        for (unsigned color_write = 0; color_write < 2; ++color_write) {
            ShaderClass shader;
            shader.Set_Depth_Mask(static_cast<ShaderClass::DepthMaskType>(depth_write));
            shader.Set_Color_Mask(static_cast<ShaderClass::ColorMaskType>(color_write));
            RenderBackendMaterialState state;
            Check(shader.Get_Render_Backend_State(state), "Write-mask shader rejected");
            Check(state.depth_write == bool(depth_write) && state.color_write == bool(color_write),
                "Depth/color write masks changed");
        }
    }
}

void Test_Culling_And_Primary_Texture()
{
    struct RestoreCulling {
        bool inverted = ShaderClass::Is_Backface_Culling_Inverted();
        ~RestoreCulling() { ShaderClass::Invert_Backface_Culling(inverted); }
    } restore;
    for (bool inverted : {false, true}) {
        ShaderClass::Invert_Backface_Culling(inverted);
        for (bool enabled : {false, true}) {
            ShaderClass shader;
            shader.Set_Cull_Mode(enabled ? ShaderClass::CULL_MODE_ENABLE : ShaderClass::CULL_MODE_DISABLE);
            RenderBackendMaterialState state;
            Check(shader.Get_Render_Backend_State(state), "Culling shader rejected");
            const auto expected = !enabled ? RenderBackendCullMode::None :
                (inverted ? RenderBackendCullMode::CounterClockwise : RenderBackendCullMode::Clockwise);
            Check(state.cull == expected, "Global cull inversion or disabled culling changed");
        }
    }
    const ShaderClass::PriGradientType modes[] = {
        ShaderClass::GRADIENT_DISABLE, ShaderClass::GRADIENT_MODULATE,
        ShaderClass::GRADIENT_ADD, ShaderClass::GRADIENT_MODULATE2X};
    const RenderBackendTextureCombine expected[] = {
        RenderBackendTextureCombine::Replace, RenderBackendTextureCombine::Modulate,
        RenderBackendTextureCombine::Add, RenderBackendTextureCombine::Modulate2X};
    for (unsigned i = 0; i < 4; ++i) {
        ShaderClass shader;
        shader.Set_Texturing(ShaderClass::TEXTURING_ENABLE);
        shader.Set_Primary_Gradient(modes[i]);
        RenderBackendMaterialState state;
        Check(shader.Get_Render_Backend_State(state), "Supported primary texture gradient rejected");
        Check(state.texture_combine == expected[i], "Primary gradient operation collapsed or changed");
        Check(!state.clamp_texture, "Asset shader unexpectedly imposed a clamp sampler");
        const ShaderClass reloaded(shader.Get_Bits());
        RenderBackendMaterialState reloaded_state;
        Check(reloaded.Get_Render_Backend_State(reloaded_state) && Equal(state, reloaded_state),
            "Reloading fixed shader bits changed material state");
    }
    // Disabled texturing ignores primary/detail operations in the old stage-zero path.
    ShaderClass untextured;
    untextured.Set_Texturing(ShaderClass::TEXTURING_DISABLE);
    untextured.Set_Primary_Gradient(ShaderClass::GRADIENT_BUMPENVMAP);
    untextured.Set_Post_Detail_Color_Func(ShaderClass::DETAILCOLOR_ADD);
    untextured.Set_Post_Detail_Alpha_Func(ShaderClass::DETAILALPHA_SCALE);
    RenderBackendMaterialState state;
    Check(untextured.Get_Render_Backend_State(state), "Inactive texture operations rejected an untextured shader");
}

void Test_Unsupported_Effects()
{
    for (unsigned fog = ShaderClass::FOG_ENABLE; fog <= ShaderClass::FOG_WHITE; ++fog) {
        ShaderClass shader;
        shader.Set_Fog_Func(static_cast<ShaderClass::FogFuncType>(fog));
        Reject(shader, "Unmigrated fog accepted");
    }
    // Include the paired game's additional operations and reserved asset codes.
    for (unsigned detail = ShaderClass::DETAILCOLOR_DETAIL; detail < 16; ++detail) {
        ShaderClass shader;
        shader.Set_Texturing(ShaderClass::TEXTURING_ENABLE);
        shader.Set_Post_Detail_Color_Func(static_cast<ShaderClass::DetailColorFuncType>(detail));
        Reject(shader, "Unmigrated detail-color effect accepted");
    }
    for (unsigned detail = ShaderClass::DETAILALPHA_DETAIL; detail <= ShaderClass::DETAILALPHA_INVSCALE; ++detail) {
        ShaderClass shader;
        shader.Set_Texturing(ShaderClass::TEXTURING_ENABLE);
        shader.Set_Post_Detail_Alpha_Func(static_cast<ShaderClass::DetailAlphaFuncType>(detail));
        Reject(shader, "Unmigrated detail-alpha effect accepted");
    }
    for (unsigned gradient : {3u, 4u, 6u, 7u}) {
        ShaderClass shader;
        shader.Set_Texturing(ShaderClass::TEXTURING_ENABLE);
        shader.Set_Primary_Gradient(static_cast<ShaderClass::PriGradientType>(gradient));
        Reject(shader, "Bump or reserved primary gradient accepted");
    }
    ShaderClass secondary;
    secondary.Set_Secondary_Gradient(ShaderClass::SECONDARY_GRADIENT_ENABLE);
    Reject(secondary, "Unmigrated secondary gradient accepted");
    ShaderClass npatch;
    npatch.Set_NPatch_Enable(ShaderClass::NPATCH_ENABLE);
    Reject(npatch, "Unmigrated N-patch effect accepted");
}

void Test_Fixed_Asset_Value()
{
    ShaderClass::Invert_Backface_Culling(false);
    ShaderClass default_shader;
    RenderBackendMaterialState default_state;
    Check(default_shader.Get_Render_Backend_State(default_state) &&
        Equal(default_state, RenderBackendMaterialState{}), "Default W3D material draw state changed");
    constexpr unsigned bits = SHADE_CNST(ShaderClass::PASS_GEQUAL, ShaderClass::DEPTH_WRITE_DISABLE,
        ShaderClass::COLOR_WRITE_ENABLE, ShaderClass::SRCBLEND_SRC_ALPHA,
        ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA, ShaderClass::FOG_DISABLE,
        ShaderClass::GRADIENT_ADD, ShaderClass::SECONDARY_GRADIENT_DISABLE,
        ShaderClass::TEXTURING_ENABLE, ShaderClass::ALPHATEST_ENABLE,
        ShaderClass::CULL_MODE_ENABLE, ShaderClass::DETAILCOLOR_DISABLE, ShaderClass::DETAILALPHA_DISABLE);
    static_assert(bits == 0x000D88B6u);
    ShaderClass shader(bits);
    ShaderClass copy(shader);
    Check(shader == copy && !(shader != copy), "Shader asset value copy/equality changed");
    RenderBackendMaterialState state;
    Check(shader.Get_Render_Backend_State(state) && shader.Get_Bits() == bits,
        "Fixed shader schema changed during translation");
    copy.Set_Color_Mask(ShaderClass::COLOR_WRITE_DISABLE);
    Check(shader != copy && shader.Get_Bits() == bits && copy.Get_Bits() == (bits & ~0x10u),
        "Shader field mutation changed another asset bit or copy");
}
} // namespace

int main()
{
    Test_Depth_And_Blending();
    Test_Alpha_And_Writes();
    Test_Culling_And_Primary_Texture();
    Test_Unsupported_Effects();
    Test_Fixed_Asset_Value();
    std::cout << "W3D shader material contract: " << (failures ? "FAIL" : "PASS") << '\n';
    return failures ? 1 : 0;
}
