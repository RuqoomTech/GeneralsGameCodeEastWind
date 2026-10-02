#include "Utility/CppMacros.h"
#include "WW3D2/shader.h"
#include "WW3D2/IRenderBackend.h"

#include <iostream>

namespace {
int failures = 0;

void require(bool condition, const char *description)
{
    if (!condition) {
        std::cerr << description << '\n';
        ++failures;
    }
}

void rejects(const ShaderClass &shader, const char *description)
{
    RenderBackendMaterialState state;
    state.alpha_reference = 0.314f;
    state.depth_write = false;
    state.source_blend = RenderBackendBlendFactor::SourceColor;
    require(!shader.Get_Render_Backend_State(state), description);
    require(state.alpha_reference == 0.314f && !state.depth_write &&
        state.source_blend == RenderBackendBlendFactor::SourceColor,
        "Rejected material must retain the caller's state");
}
}

// The W3D asset representation remains fixed while runtime draw state changes.
static_assert(SHIFT_DEPTHCOMPARE == 0 && SHIFT_DEPTHMASK == 3 && SHIFT_COLORMASK == 4);
static_assert(SHIFT_DSTBLEND == 5 && SHIFT_FOG == 8 && SHIFT_PRIGRADIENT == 10);
static_assert(SHIFT_SECGRADIENT == 13 && SHIFT_SRCBLEND == 14 && SHIFT_TEXTURING == 16);
static_assert(SHIFT_NPATCHENABLE == 17 && SHIFT_ALPHATEST == 18 && SHIFT_CULLMODE == 19);
static_assert(SHIFT_POSTDETAILCOLORFUNC == 20 && SHIFT_POSTDETAILALPHAFUNC == 24);

int main()
{
    ShaderClass::Invert_Backface_Culling(false);
    ShaderClass shader;
    RenderBackendMaterialState state;
    require(shader.Get_Render_Backend_State(state), "Default vertex-color material must be supported");
    require(state.depth_test == RenderBackendDepthTest::LessEqual && state.depth_write && state.color_write &&
        state.source_blend == RenderBackendBlendFactor::One && state.destination_blend == RenderBackendBlendFactor::Zero &&
        state.cull == RenderBackendCullMode::Clockwise && state.alpha_test == RenderBackendAlphaTest::Disabled,
        "Default W3D draw state must remain opaque, depth-writing and clockwise-culled");

    const ShaderClass::DepthCompareType legacy_depths[] = {
        ShaderClass::PASS_NEVER, ShaderClass::PASS_LESS, ShaderClass::PASS_EQUAL, ShaderClass::PASS_LEQUAL,
        ShaderClass::PASS_GREATER, ShaderClass::PASS_NOTEQUAL, ShaderClass::PASS_GEQUAL, ShaderClass::PASS_ALWAYS};
    const RenderBackendDepthTest depths[] = {
        RenderBackendDepthTest::Never, RenderBackendDepthTest::Less, RenderBackendDepthTest::Equal, RenderBackendDepthTest::LessEqual,
        RenderBackendDepthTest::Greater, RenderBackendDepthTest::NotEqual, RenderBackendDepthTest::GreaterEqual, RenderBackendDepthTest::Always};
    for (unsigned int i = 0; i < 8; ++i) {
        shader.Set_Depth_Compare(legacy_depths[i]);
        require(shader.Get_Render_Backend_State(state) && state.depth_test == depths[i], "W3D depth comparison changed");
    }
    for (unsigned int write = 0; write < 2; ++write) {
        shader.Set_Depth_Mask(write ? ShaderClass::DEPTH_WRITE_ENABLE : ShaderClass::DEPTH_WRITE_DISABLE);
        shader.Set_Color_Mask(write ? ShaderClass::COLOR_WRITE_ENABLE : ShaderClass::COLOR_WRITE_DISABLE);
        require(shader.Get_Render_Backend_State(state) && state.depth_write == bool(write) && state.color_write == bool(write),
            "Depth and color write masks must survive material translation");
    }

    const ShaderClass::SrcBlendFuncType legacy_sources[] = {
        ShaderClass::SRCBLEND_ZERO, ShaderClass::SRCBLEND_ONE, ShaderClass::SRCBLEND_SRC_ALPHA, ShaderClass::SRCBLEND_ONE_MINUS_SRC_ALPHA};
    const RenderBackendBlendFactor sources[] = {
        RenderBackendBlendFactor::Zero, RenderBackendBlendFactor::One, RenderBackendBlendFactor::SourceAlpha, RenderBackendBlendFactor::InverseSourceAlpha};
    const ShaderClass::DstBlendFuncType legacy_destinations[] = {
        ShaderClass::DSTBLEND_ZERO, ShaderClass::DSTBLEND_ONE, ShaderClass::DSTBLEND_SRC_COLOR,
        ShaderClass::DSTBLEND_ONE_MINUS_SRC_COLOR, ShaderClass::DSTBLEND_SRC_ALPHA, ShaderClass::DSTBLEND_ONE_MINUS_SRC_ALPHA};
    const RenderBackendBlendFactor destinations[] = {
        RenderBackendBlendFactor::Zero, RenderBackendBlendFactor::One, RenderBackendBlendFactor::SourceColor,
        RenderBackendBlendFactor::InverseSourceColor, RenderBackendBlendFactor::SourceAlpha, RenderBackendBlendFactor::InverseSourceAlpha};
    for (unsigned int source = 0; source < 4; ++source) {
        for (unsigned int destination = 0; destination < 6; ++destination) {
            shader.Set_Src_Blend_Func(legacy_sources[source]);
            shader.Set_Dst_Blend_Func(legacy_destinations[destination]);
            require(shader.Get_Render_Backend_State(state) && state.source_blend == sources[source] &&
                state.destination_blend == destinations[destination], "W3D blend pair changed");
        }
        shader.Set_Alpha_Test(ShaderClass::ALPHATEST_ENABLE);
        require(shader.Get_Render_Backend_State(state), "Alpha testing must be supported with every W3D source blend");
        const bool inverse = legacy_sources[source] == ShaderClass::SRCBLEND_ONE_MINUS_SRC_ALPHA;
        require(state.alpha_test == (inverse ? RenderBackendAlphaTest::LessEqual : RenderBackendAlphaTest::GreaterEqual) &&
            state.alpha_reference == (inverse ? 159.0f : 96.0f) / 255.0f,
            "Alpha comparison must retain the inclusive 96/159 thresholds and inverse-source convention");
        shader.Set_Alpha_Test(ShaderClass::ALPHATEST_DISABLE);
        require(shader.Get_Render_Backend_State(state) && state.alpha_test == RenderBackendAlphaTest::Disabled,
            "Disabling alpha testing must clear its prior draw state");
    }

    shader.Reset();
    for (bool inverted : {false, true}) {
        ShaderClass::Invert_Backface_Culling(inverted);
        shader.Set_Cull_Mode(ShaderClass::CULL_MODE_ENABLE);
        require(shader.Get_Render_Backend_State(state) && state.cull ==
            (inverted ? RenderBackendCullMode::CounterClockwise : RenderBackendCullMode::Clockwise),
            "Global backface inversion must affect subsequent material draws");
        shader.Set_Cull_Mode(ShaderClass::CULL_MODE_DISABLE);
        require(shader.Get_Render_Backend_State(state) && state.cull == RenderBackendCullMode::None,
            "Disabled culling must remain disabled under global inversion");
    }
    ShaderClass::Invert_Backface_Culling(false);
    shader.Reset();
    shader.Set_Texturing(ShaderClass::TEXTURING_ENABLE);
    const ShaderClass::PriGradientType gradients[] = {
        ShaderClass::GRADIENT_DISABLE, ShaderClass::GRADIENT_MODULATE, ShaderClass::GRADIENT_ADD, ShaderClass::GRADIENT_MODULATE2X};
    const RenderBackendTextureCombine combines[] = {
        RenderBackendTextureCombine::Replace, RenderBackendTextureCombine::Modulate, RenderBackendTextureCombine::Add, RenderBackendTextureCombine::Modulate2X};
    for (unsigned int i = 0; i < 4; ++i) {
        shader.Set_Primary_Gradient(gradients[i]);
        const unsigned int bits = shader.Get_Bits();
        require(shader.Get_Render_Backend_State(state) && state.texture_combine == combines[i], "Primary texture combination changed");
        require(shader.Get_Bits() == bits, "Runtime translation must not modify W3D asset bits");
        ShaderClass reloaded(bits);
        require(reloaded.Get_Render_Backend_State(state) && state.texture_combine == combines[i],
            "Loaded W3D shader bits must produce the same material state");
    }
    // Full shader color/alpha equations are verified in the D3D12 pixel tests.
    for (auto bump : {ShaderClass::GRADIENT_BUMPENVMAP, ShaderClass::GRADIENT_BUMPENVMAPLUMINANCE}) {
        shader.Set_Primary_Gradient(bump);
        rejects(shader, "Unmigrated bump behavior must not become a successful RGBA draw");
    }
    shader.Set_Primary_Gradient(ShaderClass::GRADIENT_MODULATE);
    for (auto fog : {ShaderClass::FOG_ENABLE, ShaderClass::FOG_SCALE_FRAGMENT, ShaderClass::FOG_WHITE}) {
        shader.Set_Fog_Func(fog);
        rejects(shader, "Unmigrated fog behavior must be explicit");
    }
    shader.Set_Fog_Func(ShaderClass::FOG_DISABLE);
    shader.Set_Secondary_Gradient(ShaderClass::SECONDARY_GRADIENT_ENABLE);
    rejects(shader, "Unmigrated secondary gradient must be explicit");
    shader.Set_Secondary_Gradient(ShaderClass::SECONDARY_GRADIENT_DISABLE);
    for (unsigned int detail = ShaderClass::DETAILCOLOR_DETAIL; detail <= 12; ++detail) {
        shader.Set_Post_Detail_Color_Func(static_cast<ShaderClass::DetailColorFuncType>(detail));
        rejects(shader, "Unmigrated second-texture color operation must be explicit");
    }
    shader.Set_Post_Detail_Color_Func(ShaderClass::DETAILCOLOR_DISABLE);
    for (auto detail : {ShaderClass::DETAILALPHA_DETAIL, ShaderClass::DETAILALPHA_SCALE, ShaderClass::DETAILALPHA_INVSCALE}) {
        shader.Set_Post_Detail_Alpha_Func(detail);
        rejects(shader, "Unmigrated second-texture alpha operation must be explicit");
    }
    shader.Set_Post_Detail_Alpha_Func(ShaderClass::DETAILALPHA_DISABLE);
    shader.Set_NPatch_Enable(ShaderClass::NPATCH_ENABLE);
    rejects(shader, "Unmigrated patch tessellation must be explicit");
    shader.Set_NPatch_Enable(ShaderClass::NPATCH_DISABLE);
    shader.Set_Dst_Blend_Func(static_cast<ShaderClass::DstBlendFuncType>(6));
    rejects(shader, "Invalid destination blend asset bits must be rejected");
    shader.Set_Dst_Blend_Func(ShaderClass::DSTBLEND_ZERO);
    shader.Set_Primary_Gradient(static_cast<ShaderClass::PriGradientType>(6));
    rejects(shader, "Invalid primary gradient asset bits must be rejected");

    std::cout << "W3D shader material contract: " << (failures ? "FAIL" : "PASS") << '\n';
    return failures ? 1 : 0;
}
