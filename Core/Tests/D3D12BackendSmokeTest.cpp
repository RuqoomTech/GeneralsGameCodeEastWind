#include "Utility/CppMacros.h"

#include "WW3D2/Backend/RenderBackend.h"
#include "WW3D2/IRenderBackend.h"
#include "WWMath/vector3.h"
#include "WWMath/matrix4.h"

#include <windows.h>

#include <iostream>
#include <algorithm>
#include <cmath>
#include <array>

namespace
{
constexpr wchar_t WindowClassName[] = L"GeneralsD3D12BackendSmoke";

LRESULT CALLBACK smokeWindowProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    if (message == WM_CLOSE)
    {
        DestroyWindow(window);
        return 0;
    }
    return DefWindowProcW(window, message, w_param, l_param);
}

HWND createHiddenWindow(HINSTANCE instance)
{
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = smokeWindowProc;
    window_class.hInstance = instance;
    window_class.lpszClassName = WindowClassName;
    if (RegisterClassExW(&window_class) == 0)
    {
        return nullptr;
    }

    return CreateWindowExW(
        0,
        WindowClassName,
        L"Generals D3D12 backend smoke",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        640,
        480,
        nullptr,
        nullptr,
        instance,
        nullptr);
}
bool verifyProjectedTextureAndMips(IRenderBackend &backend)
{
    std::vector<unsigned char> red(4*4*4),green(2*2*4),blue(4);
    for (unsigned i=0;i<red.size();i+=4) { red[i]=255; red[i+3]=255; }
    for (unsigned i=0;i<green.size();i+=4) { green[i+1]=255; green[i+3]=255; }
    blue[2]=blue[3]=255;
    const RenderBackendTextureMipLevel levels[]={{4,4,16,red.data()},{2,2,8,green.data()},{1,1,4,blue.data()}};
    const auto mip_texture=backend.Create_Static_RGBA8_Texture(levels,3);
    const unsigned char edge[]={255,0,0,255,0,255,0,255};
    const auto projected_texture=backend.Create_Static_RGBA8_Texture(2,1,edge,8);
    RenderBackendTexturedVertex quad[] = {
        {-.8f,-.8f,.5f,1,1,1,1,1,.5f,1}, {-.8f,.8f,.5f,1,1,1,1,1,.5f,1},
        {.8f,.8f,.5f,1,1,1,1,1,1,2}, {.8f,-.8f,.5f,1,1,1,1,1,1,2}};
    const unsigned short indices[]={0,2,1,0,3,2};
    RenderBackendMaterialState material;
    material.clamp_texture=true;
    material.depth_write=false;
    backend.Set_Viewport({0,0,640,480,0,1}); backend.Set_View_Projection(Matrix4x4(true));
    unsigned width=0,height=0;
    std::vector<unsigned char> pixels;
    auto capture=[&]() {
        backend.End_Scene(false);
        const bool read=backend.Read_Output_RGBA8(width,height,pixels) && width==640 && height==480;
        backend.Flip_To_Primary(); return read;
    };
    bool ok=mip_texture.Is_Valid() && projected_texture.Is_Valid();
    backend.Clear(true,true,Vector3(0,0,0),0,1,0); backend.Begin_Scene();
    ok=backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,projected_texture,material) && ok;
    const bool projected_read=capture(); ok=projected_read && ok;
    if (projected_read) {
        // Interpolate (s,t,q) first, then divide. CPU s/q at the vertices would
        // produce a different center color for this varying-q fixture.
        const float q=1.5f+(.5f*(2.f*(320.5f/640.f)-1.f)/.8f);
        const float weight=2.f/q-.5f;
        const auto center=(240*640+320)*4;
        ok=std::abs(int(pixels[center])-int(std::lround(255*(1-weight))))<=2 &&
            std::abs(int(pixels[center+1])-int(std::lround(255*weight)))<=2 &&
            pixels[center+2]==0 && pixels[center+3]==255 && ok;
    }
    material.clamp_texture=false;
    for (unsigned minify=0;minify<2;++minify) {
        for (unsigned i=0;i<4;++i) {
            quad[i].q=1;
            quad[i].u=(i>=2 ? 1.f : 0.f)*(minify ? 1024.f : 1.f);
            quad[i].v=(i==1 || i==2 ? 1.f : 0.f)*(minify ? 1024.f : 1.f);
        }
        backend.Clear(true,true,Vector3(0,0,0),0,1,0); backend.Begin_Scene();
        ok=backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,mip_texture,material) && ok;
        const bool read=capture(); ok=read && ok;
        if (read) for (unsigned channel=0;channel<4;++channel)
            ok=pixels[(240*640+320)*4+channel]==(channel==3 || channel==(minify ? 2u : 0u) ? 255 : 0) && ok;
    }
    const RenderBackendTextureMipLevel bad[]={{4,4,16,red.data()},{3,2,12,green.data()}};
    ok=!backend.Create_Static_RGBA8_Texture(bad,2).Is_Valid() &&
        !backend.Create_Static_RGBA8_Texture(levels,0).Is_Valid() && ok;
    backend.Release_Texture(mip_texture); backend.Release_Texture(projected_texture);
    if (!ok) std::cerr << "Projected texture or authored mip sampling failed.\n";
    return ok;
}

bool verifySceneTextureLoading(IRenderBackend &backend)
{
    const unsigned char red[] = {255,0,0,255}, green[] = {0,255,0,255}, blue[] = {0,0,255,255};
    std::vector<RenderBackendTextureHandle> textures;
    textures.reserve(134);
    textures.push_back(backend.Create_Static_RGBA8_Texture(1,1,red,4));
    RenderBackendTexturedVertex quad[] = {
        {-.8f,0,.5f,1,1,1,1,0,0}, {-.8f,.8f,.5f,1,1,1,1,0,0},
        {0,.8f,.5f,1,1,1,1,0,0}, {0,0,.5f,1,1,1,1,0,0}};
    const unsigned short indices[] = {0,2,1,0,3,2};
    RenderBackendMaterialState material;
    material.depth_test=RenderBackendDepthTest::Disabled;
    material.depth_write=false;
    backend.Set_Viewport({0,0,640,480,0,1});
    backend.Set_View_Projection(Matrix4x4(true));
    backend.Clear(true,true,Vector3(0,0,0),1,1,0);
    backend.Begin_Scene();
    // This draw references the original heap before either growth occurs.
    bool ok=backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,textures.front(),material);
    for (unsigned i=0;i<132;++i) {
        const auto texture=backend.Create_Static_RGBA8_Texture(1,1,i==64 ? green : blue,4);
        textures.push_back(texture);
        ok=texture.Is_Valid() && ok;
        if (!texture.Is_Valid()) std::cerr << "Scene texture upload failed at " << i << ".\n";
        // Record another draw between the 64->128 and 128->256 heap growths.
        if (i==64) {
            for (auto &v:quad) v.y-=.8f;
            ok=backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,texture,material) && ok;
            for (auto &v:quad) v.y+=.8f;
        }
    }
    for (auto &v:quad) v.x+=.8f;
    ok=backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,textures.back(),material) && ok;
    for (auto &v:quad) v.y-=.8f;
    // Old resources must remain sampleable through their rebuilt SRVs in the new heap.
    ok=backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,textures.front(),material) && ok;
    backend.End_Scene(false);
    unsigned width=0,height=0;
    std::vector<unsigned char> pixels;
    const bool read=backend.Read_Output_RGBA8(width,height,pixels) && width==640 && height==480;
    ok=read && ok;
    if (read) {
        const unsigned centers[]={120*640+160,360*640+160,120*640+480,360*640+480};
        const unsigned channels[]={0,1,2,0};
        for (unsigned sample=0;sample<4;++sample) for (unsigned channel=0;channel<4;++channel)
            ok=pixels[centers[sample]*4+channel]==(channel==3 || channel==channels[sample] ? 255 : 0) && ok;
    }
    backend.Flip_To_Primary();
    for (const auto texture:textures) backend.Release_Texture(texture);
    if (!ok) {
        std::cerr << "Scene texture upload or descriptor heap lifetime check failed.\n";
        if (read) for (const unsigned center:{120*640+160,360*640+160,120*640+480,360*640+480})
            std::cerr << int(pixels[center*4]) << ',' << int(pixels[center*4+1]) << ',' <<
                int(pixels[center*4+2]) << ',' << int(pixels[center*4+3]) << '\n';
    }
    return ok;
}

bool verifyStencil(IRenderBackend &backend)
{
    RenderBackendTexturedVertex quad[] = {
        {-.8f,-.8f,.7f,1,0,0,1,0,0}, {-.8f,.8f,.7f,1,0,0,1,0,0},
        {.8f,.8f,.7f,1,0,0,1,0,0}, {.8f,-.8f,.7f,1,0,0,1,0,0}};
    const unsigned short indices[]={0,2,1,0,3,2};
    const unsigned short reverse[]={0,1,2,0,2,3};
    backend.Set_View_Projection(Matrix4x4(true));
    backend.Set_Viewport({0,0,640,480,0,1});
    bool ok=true;
    auto verify=[&](unsigned initial, unsigned expected, RenderBackendMaterialState write,
                    const unsigned short *triangles=nullptr, unsigned read_mask=255) {
        if (triangles == nullptr) triangles=indices;
        backend.Clear(true,true,Vector3(0,0,0),1,.5f,initial);
        backend.Begin_Scene();
        write.color_write=false; write.depth_write=false; write.stencil.enabled=true;
        write.cull=RenderBackendCullMode::None;
        bool drew=backend.Draw_Indexed_Material_Triangles(quad,4,triangles,6,{},write);
        RenderBackendMaterialState read;
        read.depth_test=RenderBackendDepthTest::Disabled; read.depth_write=false;
        read.cull=RenderBackendCullMode::None;
        read.stencil.enabled=true; read.stencil.reference=expected;
        read.stencil.read_mask=read_mask; read.stencil.write_mask=0;
        read.stencil.front.comparison=read.stencil.back.comparison=RenderBackendStencilCompare::Equal;
        drew=backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,{},read) && drew;
        backend.End_Scene(false);
        unsigned width=0,height=0; std::vector<unsigned char> pixels;
        const bool captured=backend.Read_Output_RGBA8(width,height,pixels) && width==640 && height==480;
        const bool red=captured && pixels[(240*640+320)*4]==255 && pixels[(240*640+320)*4+1]==0;
        backend.Flip_To_Primary();
        if (!drew || !red) std::cerr << "Stencil fixture initial=" << initial << " expected=" << expected <<
            " failed, drew=" << drew << " captured=" << captured << ".\n";
        return drew && red;
    };
    RenderBackendMaterialState write;
    write.depth_test=RenderBackendDepthTest::Disabled;
    write.stencil.reference=0xa5; write.stencil.write_mask=0x0f;
    write.stencil.front.pass=write.stencil.back.pass=RenderBackendStencilOperation::Replace;
    ok=verify(0xc0,0xc5,write) && ok;
    ok=verify(0xc0,5,write,indices,0x0f) && ok;
    write.stencil.write_mask=255;
    const RenderBackendStencilOperation operations[]={RenderBackendStencilOperation::Increment,
        RenderBackendStencilOperation::IncrementSaturate,RenderBackendStencilOperation::Decrement,
        RenderBackendStencilOperation::DecrementSaturate,RenderBackendStencilOperation::Invert,
        RenderBackendStencilOperation::Zero,RenderBackendStencilOperation::Keep};
    const unsigned initial[]={255,255,0,0,0x35,71,71};
    const unsigned expected[]={0,255,255,0,0xca,0,71};
    for (unsigned i=0;i<7;++i) {
        write.stencil.front.pass=write.stencil.back.pass=operations[i];
        ok=verify(initial[i],expected[i],write) && ok;
    }
    write.stencil.front.pass=write.stencil.back.pass=RenderBackendStencilOperation::Keep;
    write.depth_test=RenderBackendDepthTest::Less;
    write.stencil.front.depth_fail=write.stencil.back.depth_fail=RenderBackendStencilOperation::Increment;
    ok=verify(9,10,write) && ok;
    write.depth_test=RenderBackendDepthTest::Disabled;
    write.stencil.front.comparison=write.stencil.back.comparison=RenderBackendStencilCompare::Never;
    write.stencil.front.stencil_fail=write.stencil.back.stencil_fail=RenderBackendStencilOperation::Zero;
    ok=verify(9,0,write) && ok;
    write={}; write.depth_test=RenderBackendDepthTest::Disabled; write.stencil.reference=17;
    write.stencil.front.pass=RenderBackendStencilOperation::Replace;
    write.stencil.back.pass=RenderBackendStencilOperation::Increment;
    // With culling disabled, normal indices select BackFace; reversed indices select FrontFace.
    ok=verify(2,3,write) && ok;
    ok=verify(2,17,write,reverse) && ok;

    // A screen-space overlay uses the entire output, bypasses the camera, and
    // restores the tactical viewport for subsequent world geometry.
    Matrix4x4 translated(true); translated[0][3]=10.f;
    backend.Set_View_Projection(translated); backend.Set_Viewport({0,0,160,120,0,1});
    backend.Clear(true,true,Vector3(0,0,0),1,1,0); backend.Begin_Scene();
    RenderBackendMaterialState overlay; overlay.screen_space=true; overlay.depth_write=false;
    overlay.depth_test=RenderBackendDepthTest::Disabled;
    ok=backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,{},overlay) && ok;
    backend.Set_View_Projection(Matrix4x4(true));
    for (auto &v:quad) {v.r=0; v.g=1;}
    overlay.screen_space=false;
    ok=backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,{},overlay) && ok;
    overlay.stencil.enabled=true; overlay.stencil.reference=256;
    ok=!backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,{},overlay) && ok;
    overlay.stencil.reference=0; overlay.stencil.back.pass=static_cast<RenderBackendStencilOperation>(8);
    ok=!backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,{},overlay) && ok;
    backend.End_Scene(false);
    unsigned width=0,height=0; std::vector<unsigned char> pixels;
    const bool captured=backend.Read_Output_RGBA8(width,height,pixels) && width==640 && height==480;
    ok=captured && ok;
    if (captured) ok=pixels[(240*640+320)*4]==255 && pixels[(60*640+80)*4+1]==255 && ok;
    backend.Flip_To_Primary(); backend.Set_Viewport({0,0,640,480,0,1});
    if (!ok) std::cerr << "Stencil or screen-space material checks failed.\n";
    return ok;
}

bool verifyDeferredTextureRelease(IRenderBackend &backend)
{
    const unsigned char red[]={255,0,0,255}, blue[]={0,0,255,255}, green[]={0,255,0,255};
    const auto released=backend.Create_Static_RGBA8_Texture(1,1,red,4);
    RenderBackendTexturedVertex quad[]={
        {-.8f,-.8f,.5f,1,1,1,1,0,0}, {-.8f,.8f,.5f,1,1,1,1,0,0},
        {0,.8f,.5f,1,1,1,1,0,0}, {0,-.8f,.5f,1,1,1,1,0,0}};
    const unsigned short indices[]={0,2,1,0,3,2};
    RenderBackendMaterialState material;
    material.depth_test=RenderBackendDepthTest::Disabled; material.depth_write=false;
    backend.Set_View_Projection(Matrix4x4(true)); backend.Set_Viewport({0,0,640,480,0,1});
    unsigned width=0,height=0;
    std::vector<unsigned char> pixels;
    auto capture=[&]() {
        backend.End_Scene(false);
        return backend.Read_Output_RGBA8(width,height,pixels) && width==640 && height==480;
    };
    auto matches=[&](unsigned x, unsigned channel) {
        const auto offset=(240u*640+x)*4;
        return pixels[offset]==(channel==0 ? 255 : 0) &&
            pixels[offset+1]==(channel==1 ? 255 : 0) &&
            pixels[offset+2]==(channel==2 ? 255 : 0) && pixels[offset+3]==255;
    };

    backend.Clear(true,true,Vector3(0,0,0),1,1,0); backend.Begin_Scene();
    bool ok=released.Is_Valid() && backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,released,material);
    backend.Release_Texture(released); backend.Release_Texture(released);
    ok=!backend.Is_Texture_Valid(released) &&
        !backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,released,material) && ok;
    const auto other=backend.Create_Static_RGBA8_Texture(1,1,blue,4);
    ok=other.Is_Valid() && other.slot!=released.slot && ok;
    for (auto &vertex:quad) vertex.x+=.8f;
    ok=backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,other,material) && ok;
    const bool read=capture(); ok=read && ok;
    if (read) ok=matches(160,0) && matches(480,2) && ok;

    // Reuse the same command frame without presenting: Begin_Scene waits for
    // its fence and collects the retired slot before the next allocation.
    backend.Clear(true,true,Vector3(0,0,0),1,1,0); backend.Begin_Scene();
    const auto replacement=backend.Create_Static_RGBA8_Texture(1,1,green,4);
    ok=replacement.Is_Valid() && replacement.slot==released.slot &&
        replacement.generation!=released.generation && !backend.Is_Texture_Valid(released) && ok;
    backend.Release_Texture(released);
    ok=backend.Is_Texture_Valid(replacement) &&
        backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,replacement,material) && ok;
    const bool replacement_read=capture(); ok=replacement_read && ok;
    if (replacement_read) ok=matches(480,1) && ok;
    backend.Flip_To_Primary();
    backend.Release_Texture(other); backend.Release_Texture(replacement);

    const auto target=backend.Create_Render_Texture(13,7);
    ok=target.Is_Valid() && backend.Set_Render_Texture(target) && ok;
    Matrix4x4 translated(true); translated[0][3]=10.f;
    backend.Set_View_Projection(translated);
    backend.Clear(true,false,Vector3(1,0,0),1,1,0); backend.Begin_Scene();
    backend.Release_Texture(target); backend.Release_Texture(target);
    int target_width=0,target_height=0;
    ok=!backend.Is_Texture_Valid(target) && backend.Get_Render_Target_Size(target_width,target_height) &&
        target_width==13 && target_height==7 && ok;
    // The released target still owns the currently recorded clear/draw/barrier.
    ok=backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,{},material) && ok;
    backend.End_Scene(false);
    ok=backend.Get_Render_Target_Size(target_width,target_height) && target_width==640 && target_height==480 &&
        !backend.Set_Render_Texture(target) && ok;
    const bool preserved=backend.Read_Output_RGBA8(width,height,pixels) && width==640 && height==480;
    ok=preserved && ok;
    if (preserved) ok=matches(480,1) && ok; // Offscreen submission did not replace primary capture.

    backend.Clear(true,true,Vector3(0,0,0),1,1,0); backend.Begin_Scene();
    for (auto &vertex:quad) { vertex.r=0; vertex.g=1; vertex.b=0; }
    // Identity camera and full viewport were restored when the released target
    // was submitted; no explicit restoration here can hide a backend mistake.
    ok=backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,{},material) && ok;
    const bool primary_read=capture(); ok=primary_read && ok;
    if (primary_read) ok=matches(480,1) && ok;
    const auto target_replacement=backend.Create_Render_Texture(13,7);
    ok=target_replacement.Is_Valid() && target_replacement.slot==target.slot &&
        target_replacement.generation!=target.generation && !backend.Set_Render_Texture(target) && ok;
    backend.Release_Texture(target);
    ok=backend.Is_Texture_Valid(target_replacement) && backend.Set_Render_Texture(target_replacement) && ok;
    ok=backend.Set_Render_Texture({}) && ok;
    backend.Release_Texture(target_replacement); backend.Flip_To_Primary();
    if (!ok) std::cerr << "Deferred texture release, descriptor lifetime, or selected target restoration failed.\n";
    return ok;
}

bool verifySamplers(IRenderBackend &backend)
{
    const unsigned char colors[]={255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255};
    const auto texture=backend.Create_Static_RGBA8_Texture(2,2,colors,8);
    std::vector<unsigned char> red(64),green(16),blue(4);
    for (unsigned i=0;i<red.size();i+=4) {red[i]=255; red[i+3]=255;}
    for (unsigned i=0;i<green.size();i+=4) {green[i+1]=255; green[i+3]=255;}
    blue[2]=blue[3]=255;
    const RenderBackendTextureMipLevel levels[]={{4,4,16,red.data()},{2,2,8,green.data()},{1,1,4,blue.data()}};
    const auto mips=backend.Create_Static_RGBA8_Texture(levels,3);
    RenderBackendTexturedVertex quad[]={
        {-.8f,-.8f,.5f,1,1,1,1,.4f,.25f}, {-.8f,.8f,.5f,1,1,1,1,.4f,.25f},
        {.8f,.8f,.5f,1,1,1,1,.4f,.25f}, {.8f,-.8f,.5f,1,1,1,1,.4f,.25f}};
    const unsigned short indices[]={0,2,1,0,3,2};
    RenderBackendMaterialState material; material.depth_write=false;
    backend.Set_Viewport({0,0,640,480,0,1}); backend.Set_View_Projection(Matrix4x4(true));
    bool ok=texture.Is_Valid() && mips.Is_Valid();
    auto verify=[&](RenderBackendTextureHandle handle,int r,int g,int b) {
        backend.Clear(true,true,Vector3(0,0,0),1,1,0); backend.Begin_Scene();
        const bool drew=backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,handle,material);
        backend.End_Scene(false);
        unsigned width=0,height=0; std::vector<unsigned char> pixels;
        const bool read=backend.Read_Output_RGBA8(width,height,pixels) && width==640 && height==480;
        const unsigned center=(240*640+320)*4;
        const bool match=read && std::abs(int(pixels[center])-r)<=2 &&
            std::abs(int(pixels[center+1])-g)<=2 && std::abs(int(pixels[center+2])-b)<=2 && pixels[center+3]==255;
        if (!drew || !match) std::cerr << "Material sampler pixel failed expected " << r << ',' << g << ',' << b << '\n';
        backend.Flip_To_Primary(); return drew && match;
    };
    material.sampler.mag_filter=RenderBackendTextureFilter::Point;
    ok=verify(texture,255,0,0) && ok;
    material.sampler.mag_filter=RenderBackendTextureFilter::Linear;
    ok=verify(texture,179,77,0) && ok;
    material.sampler.mag_filter=RenderBackendTextureFilter::Point;
    for (auto &v:quad) {v.u=v.v=1.25f;}
    for (unsigned u=0;u<2;++u) for (unsigned v=0;v<2;++v) {
        material.sampler.address_u=static_cast<RenderBackendTextureAddress>(u);
        material.sampler.address_v=static_cast<RenderBackendTextureAddress>(v);
        ok=verify(texture,u==v ? 255 : 0,u ? 255 : 0,v ? 255 : 0) && ok;
    }
    material.sampler={};
    for (unsigned i=0;i<4;++i) {quad[i].u=i>=2 ? 1024.f : 0.f; quad[i].v=.5f;}
    ok=verify(mips,0,0,255) && ok;
    material.sampler.mipmaps=false;
    ok=verify(mips,255,0,0) && ok;
    material.sampler.mipmaps=true;
    const float span=512.f*std::pow(2.f,.25f)/4.f;
    for (unsigned i=0;i<4;++i) quad[i].u=i>=2 ? span : 0.f;
    material.sampler.mip_filter=RenderBackendTextureFilter::Point;
    ok=verify(mips,255,0,0) && ok;
    material.sampler.mip_filter=RenderBackendTextureFilter::Linear;
    ok=verify(mips,191,64,0) && ok;
    for (auto &vertex:quad) vertex.u=.5f;
    material.sampler.min_mip_level=1;
    ok=verify(mips,0,255,0) && ok;
    material.sampler.min_mip_level=2;
    ok=verify(mips,0,0,255) && ok;
    material.sampler.min_mip_level=0;
    ok=verify(mips,255,0,0) && ok;
    // Malformed runtime sampler state must never index outside the fixed heap.
    backend.Begin_Scene(); material.sampler.min_mip_level=16;
    ok=!backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,texture,material) && ok;
    material.sampler.min_mip_level=0;
    material.sampler.max_anisotropy=0;
    ok=!backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,texture,material) && ok;
    material.sampler.max_anisotropy=17;
    ok=!backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,texture,material) && ok;
    material.sampler.max_anisotropy=1;
    material.sampler.address_u=static_cast<RenderBackendTextureAddress>(2);
    ok=!backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,texture,material) && ok;
    backend.End_Scene(false); backend.Flip_To_Primary();
    backend.Release_Texture(texture); backend.Release_Texture(mips);
    if (!ok) std::cerr << "Material sampler state checks failed.\n";
    return ok;
}

bool verifyTreeShroud(IRenderBackend &backend)
{
    const unsigned char basePixels[]={128,64,192,128, 255,0,0,255};
    const unsigned char shroudPixels[]={255,255,255,0, 128,255,64,0};
    const auto base=backend.Create_Static_RGBA8_Texture(2,1,basePixels,8);
    const auto shroud=backend.Create_Static_RGBA8_Texture(2,1,shroudPixels,8);
    RenderBackendTexturedVertex quad[]={
        {-.8f,-.8f,.5f,1,1,1,1,.25f,.5f,1,.75f,.5f},
        {-.8f,.8f,.5f,1,1,1,1,.25f,.5f,1,.75f,.5f},
        {.8f,.8f,.5f,1,1,1,1,.25f,.5f,1,.75f,.5f},
        {.8f,-.8f,.5f,1,1,1,1,.25f,.5f,1,.75f,.5f}};
    const unsigned short indices[]={0,2,1,0,3,2};
    RenderBackendMaterialState material;
    material.depth_write=false;
    material.secondary_rgb_modulate=true;
    material.sampler.mag_filter=RenderBackendTextureFilter::Point;
    material.secondary_sampler.mag_filter=RenderBackendTextureFilter::Point;
    backend.Set_Viewport({0,0,640,480,0,1}); backend.Set_View_Projection(Matrix4x4(true));
    bool ok=base.Is_Valid() && shroud.Is_Valid();
    auto verify=[&](int r,int g,int b,int a,bool layered=true) {
        backend.Clear(true,true,Vector3(0,0,0),0,1,0); backend.Begin_Scene();
        material.secondary_rgb_modulate=layered;
        const bool drew=backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,base,material,layered ? shroud : RenderBackendTextureHandle{});
        backend.End_Scene(false);
        unsigned width=0,height=0; std::vector<unsigned char> pixels;
        const bool read=backend.Read_Output_RGBA8(width,height,pixels) && width==640 && height==480;
        const unsigned center=(240*640+320)*4;
        const bool match=read && std::abs(int(pixels[center])-r)<=2 && std::abs(int(pixels[center+1])-g)<=2 &&
            std::abs(int(pixels[center+2])-b)<=2 && std::abs(int(pixels[center+3])-a)<=2;
        backend.Flip_To_Primary(); return drew && match;
    };
    ok=verify(64,64,48,128) && ok; // Independent UVs; shroud alpha zero must not erase tree alpha.
    ok=verify(128,64,192,128,false) && ok; // A following ordinary material must not retain the layer.
    for (auto &vertex:quad) vertex.u2=1.75f;
    material.secondary_sampler.address_u=RenderBackendTextureAddress::Wrap;
    ok=verify(64,64,48,128) && ok;
    for (auto &vertex:quad) vertex.u2=-.25f;
    material.secondary_sampler.address_u=RenderBackendTextureAddress::Clamp;
    ok=verify(128,64,192,128) && ok;
    for (auto &vertex:quad) vertex.u2=.5f;
    material.secondary_sampler.mag_filter=RenderBackendTextureFilter::Linear;
    ok=verify(96,64,120,128) && ok;
    material.alpha_test=RenderBackendAlphaTest::GreaterEqual;
    material.alpha_reference=96.f/255.f;
    ok=verify(96,64,120,128) && ok;
    material.alpha_reference=129.f/255.f;
    ok=verify(0,0,0,0) && ok;
    material.alpha_test=RenderBackendAlphaTest::Disabled;
    backend.Begin_Scene();
    ok=!backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,base,material) && ok;
    ok=!backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,{},material,shroud) && ok;
    ok=!backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,base,material,{0,1}) && ok;
    material.secondary_sampler.max_anisotropy=17;
    ok=!backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,base,material,shroud) && ok;
    material.secondary_sampler.max_anisotropy=1;
    backend.Release_Texture(shroud);
    ok=!backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,base,material,shroud) && ok;
    backend.End_Scene(false); backend.Flip_To_Primary();
    const auto target=backend.Create_Render_Texture(32,32);
    ok=backend.Set_Render_Texture(target) && ok;
    material.depth_test=RenderBackendDepthTest::Disabled;
    backend.Begin_Scene();
    ok=!backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,base,material,target) && ok;
    backend.End_Scene(false); backend.Set_Render_Texture({});
    backend.Release_Texture(target); backend.Release_Texture(base);
    if (!ok) std::cerr << "Tree RGB shroud, independent UV/sampler, alpha, or resource validation failed.\n";
    return ok;
}

bool verifyPersistentTerrainAndMaterials(IRenderBackend &backend)
{
    const unsigned char texels[4][16]={
        {192,128,64,224, 32,255,64,32, 64,32,255,64, 16,96,192,96},
        {255,64,192,255, 128,192,64,96, 64,255,32,64, 32,96,255,32},
        {192,64,128,64, 255,192,32,96, 64,128,192,160, 128,32,255,192},
        {64,255,192,255, 255,32,64,192, 128,192,32,160, 192,96,128,80}};
    RenderBackendTextureHandle textures[4];
    bool ok=true;
    for(unsigned i=0;i<4;++i) {
        textures[i]=backend.Create_Static_RGBA8_Texture(2,2,texels[i],8);
        ok=textures[i].Is_Valid() && ok;
    }
    RenderBackendTerrainVertex quad[]={
        {-.8f,-.8f,.4f,.75f,.5f,.25f,1,.25f,.25f,0,0,0,0,0,0},
        {-.8f,.8f,.4f,.75f,.5f,.25f,1,.25f,.25f,0,0,0,0,0,0},
        {.8f,.8f,.4f,.75f,.5f,.25f,1,.25f,.25f,0,0,0,0,0,0},
        {.8f,-.8f,.4f,.75f,.5f,.25f,1,.25f,.25f,0,0,0,0,0,0}};
    const unsigned short indices[]={0,2,1,0,3,2},bad[]={0,4,1};
    RenderBackendMaterialState material;
    material.depth_write=false;
    material.color_write_mask=7;
    material.sampler.mag_filter=RenderBackendTextureFilter::Point;
    RenderBackendTerrainState terrain;
    terrain.project_world_coordinates=true;
    const float world[]={.5f,.125f,0,.3f, 0,.25f,0,-.2f, 0,0,1,.1f};
    std::copy(world,world+12,terrain.world_transform);
    terrain.shroud_projection[0]=terrain.shroud_projection[1]=2;
    terrain.shroud_projection[2]=-.35f;terrain.shroud_projection[3]=.65f;
    terrain.cloud_noise_projection[0]=.5f;
    terrain.cloud_noise_projection[1]=.6f;terrain.cloud_noise_projection[2]=.85f;
    terrain.shroud_texture=textures[1];terrain.cloud_texture=textures[2];terrain.noise_texture=textures[3];
    terrain.shroud_sampler.mag_filter=terrain.cloud_sampler.mag_filter=terrain.noise_sampler.mag_filter=RenderBackendTextureFilter::Point;
    unsigned width=0,height=0;
    std::vector<unsigned char> pixels;
    auto capture=[&]() {
        backend.End_Scene(false);
        const bool read=backend.Read_Output_RGBA8(width,height,pixels) && width==640 && height==480;
        backend.Flip_To_Primary();return read;
    };
    auto matches=[&](unsigned x,unsigned y,const std::array<float,4> &value) {
        for(unsigned c=0;c<4;++c)
            if(std::abs(int(pixels[(y*width+x)*4+c])-int(std::lround(value[c]*255)))>2) return false;
        return true;
    };
    backend.Set_Viewport({0,0,640,480,0,1});backend.Set_View_Projection(Matrix4x4(true));
    backend.Reset_Frame_Statistics();
    backend.Clear(true,true,Vector3(0,0,0),.6f,1,0);backend.Begin_Scene();
    // The actual terrain caller creates resources while recording its scene.
    const auto geometry=backend.Create_Static_Indexed_Terrain_Geometry(quad,4,indices,6);
    ok=backend.Is_Geometry_Valid(geometry) && backend.Get_Frame_Statistics().static_geometry_uploads==1 && ok;
    ok=!backend.Create_Static_Indexed_Terrain_Geometry(quad,4,bad,3).Is_Valid() && ok;
    // Caller memory can change after upload without changing retained vertices.
    for(auto &v:quad) {v.x=999;v.r=0;}
    for(unsigned frame=0;frame<3;++frame) {
        if(frame) {
            backend.Reset_Frame_Statistics();
            backend.Clear(true,true,Vector3(0,0,0),.6f,1,0);backend.Begin_Scene();
        }
        terrain.cloud_noise_projection[1]=frame==1 ? .1f : .6f;
        terrain.diffuse_after_layers=frame==1;
        Matrix4x4 camera(true);camera[0][3]=frame==2 ? -.2f : 0.f;
        backend.Set_View_Projection(camera);
        const bool drew=backend.Draw_Static_Indexed_Terrain_Geometry(geometry,textures[0],material,terrain);
        const bool read=capture();
        std::array<float,4> expected{};
        const float diffuse[]={.75f,.5f,.25f,1};
        for(unsigned c=0;c<3;++c)
            expected[c]=diffuse[c]*(texels[0][c]/255.f)*(texels[1][c]/255.f)*
                (texels[2][(frame==1 ? 8 : 12)+c]/255.f)*(texels[3][8+c]/255.f);
        expected[3]=.6f;
        const bool passed=drew && read && matches(frame==2 ? 352 : 416,288,expected) &&
            backend.Get_Frame_Statistics().static_geometry_uploads==(frame==0 ? 1u : 0u);
        if(!passed) std::cerr << "Persistent terrain projection/reuse frame " << frame << " failed.\n";
        ok=passed && ok;
    }
    backend.Release_Static_Geometry(geometry);
    ok=!backend.Is_Geometry_Valid(geometry) && ok;
    backend.Set_View_Projection(Matrix4x4(true));
    terrain=RenderBackendTerrainState{};terrain.project_world_coordinates=true;
    terrain.world_transform[0]=terrain.world_transform[5]=.25f;
    terrain.world_transform[3]=-.45f;
    for(unsigned i=0;i<4;++i) {
        quad[i].x=i>=2 ? .8f : -.8f;quad[i].r=1;quad[i].g=quad[i].b=0;
    }
    backend.Clear(true,true,Vector3(0,0,0),.6f,1,0);backend.Begin_Scene();
    const auto retiring=backend.Create_Static_Indexed_Terrain_Geometry(quad,4,indices,6);
    ok=backend.Draw_Static_Indexed_Terrain_Geometry(retiring,{},material,terrain) && ok;
    backend.Release_Static_Geometry(retiring);
    ok=!backend.Is_Geometry_Valid(retiring) &&
        !backend.Draw_Static_Indexed_Terrain_Geometry(retiring,{},material,terrain) && ok;
    for(auto &v:quad) {v.r=0;v.g=1;}
    const auto replacement=backend.Create_Static_Indexed_Terrain_Geometry(quad,4,indices,6);
    terrain.world_transform[3]=.45f;
    ok=backend.Draw_Static_Indexed_Terrain_Geometry(replacement,{},material,terrain) && ok;
    const bool releaseRead=capture();
    ok=releaseRead && matches(176,240,{1,0,0,.6f}) && matches(464,240,{0,1,0,.6f}) && ok;
    backend.Release_Static_Geometry(replacement);
    // Persistent material geometry uses the real bib blend/depth/write contract.
    RenderBackendTexturedVertex bib[]={
        {-.8f,-.8f,.5f,1,1,1,.5f,.25f,.25f}, {-.8f,.8f,.5f,1,1,1,.5f,.25f,.25f},
        {.8f,.8f,.5f,1,1,1,.5f,.25f,.25f}, {.8f,-.8f,.5f,1,1,1,.5f,.25f,.25f}};
    material.depth_test=RenderBackendDepthTest::Always;material.cull=RenderBackendCullMode::None;
    material.source_blend=RenderBackendBlendFactor::SourceAlpha;
    material.destination_blend=RenderBackendBlendFactor::InverseSourceAlpha;
    const auto bibGeometry=backend.Create_Static_Indexed_Textured_Geometry(bib,4,indices,6);
    ok=backend.Is_Geometry_Valid(bibGeometry) && ok;
    for(unsigned frame=0;frame<2;++frame) {
        backend.Reset_Frame_Statistics();backend.Clear(true,true,Vector3(.2f,.4f,.6f),.6f,.1f,0);backend.Begin_Scene();
        ok=!backend.Draw_Static_Indexed_Terrain_Geometry(bibGeometry,{},material,terrain) &&
            !backend.Draw_Static_Indexed_Material_Geometry(geometry,textures[0],material) && ok;
        const bool drew=backend.Draw_Static_Indexed_Material_Geometry(bibGeometry,textures[0],material);
        const bool read=capture();
        const float alpha=.5f*224.f/255.f;
        const std::array<float,4> expected={192.f/255.f*alpha+.2f*(1-alpha),128.f/255.f*alpha+.4f*(1-alpha),64.f/255.f*alpha+.6f*(1-alpha),.6f};
        ok=drew && read && matches(320,240,expected) && backend.Get_Frame_Statistics().static_geometry_uploads==0 && ok;
    }
    backend.Release_Static_Geometry(bibGeometry);
    for(auto texture:textures) backend.Release_Texture(texture);
    if(!ok) std::cerr << "Persistent geometry, GPU projection, material blend, or deferred release failed.\n";
    return ok;
}

bool verifyTerrain(IRenderBackend &backend)
{
    const unsigned char texels[4][16]={
        {192,128,64,224, 32,255,64,32, 64,32,255,64, 16,96,192,96},
        {255,64,192,255, 128,192,64,96, 64,255,32,64, 32,96,255,32},
        {192,64,128,64, 255,192,32,96, 64,128,192,160, 128,32,255,192},
        {64,255,192,255, 255,32,64,192, 128,192,32,160, 192,96,128,80}};
    RenderBackendTextureHandle textures[4];
    bool ok=true;
    for (unsigned i=0;i<4;++i) {
        textures[i]=backend.Create_Static_RGBA8_Texture(2,2,texels[i],8);
        ok=textures[i].Is_Valid() && ok;
    }
    RenderBackendTerrainVertex quad[]={
        {-.8f,-.8f,.5f,1.5f,1.25f,.75f,.5f,.25f,.25f,.75f,.25f,.25f,.75f,.75f,.75f},
        {-.8f,.8f,.5f,1.5f,1.25f,.75f,.5f,.25f,.25f,.75f,.25f,.25f,.75f,.75f,.75f},
        {.8f,.8f,.5f,1.5f,1.25f,.75f,.5f,.25f,.25f,.75f,.25f,.25f,.75f,.75f,.75f},
        {.8f,-.8f,.5f,1.5f,1.25f,.75f,.5f,.25f,.25f,.75f,.25f,.25f,.75f,.75f,.75f}};
    const unsigned short front[]={0,2,1,0,3,2},back[]={0,1,2,0,2,3};
    RenderBackendMaterialState material;
    material.depth_write=false;
    material.sampler.mag_filter=RenderBackendTextureFilter::Point;
    RenderBackendTerrainState terrain;
    terrain.shroud_sampler.mag_filter=RenderBackendTextureFilter::Point;
    terrain.cloud_sampler.mag_filter=RenderBackendTextureFilter::Point;
    terrain.noise_sampler.mag_filter=RenderBackendTextureFilter::Point;
    auto setLayers=[&](unsigned mask) {
        terrain.shroud_texture=mask&1 ? textures[1] : RenderBackendTextureHandle{};
        terrain.cloud_texture=mask&2 ? textures[2] : RenderBackendTextureHandle{};
        terrain.noise_texture=mask&4 ? textures[3] : RenderBackendTextureHandle{};
    };
    auto sample=[&](unsigned texture,unsigned texel) {
        std::array<float,4> value{};
        for(unsigned c=0;c<4;++c) value[c]=texels[texture][4*texel+c]/255.f;
        return value;
    };
    std::array<std::array<float,4>,4> sampled={sample(0,0),sample(1,1),sample(2,2),sample(3,3)};
    auto expected=[&](unsigned mask) {
        std::array<float,4> value{};
        const float diffuse[]={quad[0].r,quad[0].g,quad[0].b,quad[0].a};
        for(unsigned c=0;c<4;++c) {
            value[c]=sampled[0][c];
            if(mask&1) value[c]*=sampled[1][c];
            value[c]*=diffuse[c];
            if(mask&2) value[c]*=sampled[2][c];
            if(mask&4) value[c]*=sampled[3][c];
        }
        return value;
    };
    unsigned width=0,height=0;
    std::vector<unsigned char> pixels;
    auto capture=[&]() {
        backend.End_Scene(false);
        const bool read=backend.Read_Output_RGBA8(width,height,pixels) && width==640 && height==480;
        backend.Flip_To_Primary(); return read;
    };
    auto matches=[&](unsigned x,unsigned y,const std::array<float,4> &value) {
        const unsigned offset=(y*width+x)*4;
        for(unsigned c=0;c<4;++c)
            if(std::abs(int(pixels[offset+c])-int(std::lround(std::clamp(value[c],0.f,1.f)*255.f)))>2) return false;
        return true;
    };
    unsigned terrainCase=0;
    auto verify=[&](const std::array<float,4> &value,RenderBackendTextureHandle base) {
        ++terrainCase;
        backend.Clear(true,true,Vector3(0,0,0),.3f,1,0); backend.Begin_Scene();
        const bool drew=backend.Draw_Indexed_Terrain_Triangles(quad,4,front,6,base,material,terrain);
        const bool read=capture();
        const bool passed=drew && read && matches(320,240,value);
        if(!passed) {
            std::cerr << "Terrain pixel case " << terrainCase << " failed (draw=" << drew << ", read=" << read << ")";
            if(read) for(unsigned c=0;c<4;++c) std::cerr << " " << int(pixels[(240*640+320)*4+c]) << "/" << int(std::lround(std::clamp(value[c],0.f,1.f)*255.f));
            std::cerr << "\n";
        }
        return passed;
    };
    backend.Set_Viewport({0,0,640,480,0,1});
    backend.Set_View_Projection(Matrix4x4(true));
    // Distinct UVs and non-neutral RGBA values expose swapped or retained layers.
    // Diffuse red exceeds one before cloud attenuation: an intermediate RGBA8
    // pass or saturate produces a different final pixel.
    for(unsigned mask=0;mask<8;++mask) {
        setLayers(mask); ok=verify(expected(mask),textures[0]) && ok;
    }
    // The real road caller applies cloud/noise before diffuse and blends its
    // full texture-layer alpha, while preserving the water alpha channel.
    setLayers(6);
    terrain.diffuse_after_layers=true;
    material.source_blend=RenderBackendBlendFactor::SourceAlpha;
    material.destination_blend=RenderBackendBlendFactor::InverseSourceAlpha;
    material.color_write_mask=7;
    auto road=expected(6);
    for(unsigned c=0;c<3;++c) road[c]*=road[3];
    road[3]=.3f;
    ok=verify(road,textures[0]) && ok;
    terrain.diffuse_after_layers=false;
    material.source_blend=RenderBackendBlendFactor::One;
    material.destination_blend=RenderBackendBlendFactor::Zero;
    material.color_write_mask=15;
    setLayers(7);
    RenderBackendSamplerState *samplers[]={&terrain.shroud_sampler,&terrain.cloud_sampler,&terrain.noise_sampler};
    for(unsigned layer=0;layer<3;++layer) {
        const unsigned texture=layer+1,originalTexel=texture;
        auto setU=[&](float u) {
            for(auto &v:quad) {
                if(layer==0) v.u2=u; else if(layer==1) v.u3=u; else v.u4=u;
            }
        };
        const float originalU=originalTexel&1 ? .75f : .25f;
        setU(originalU+1.f); samplers[layer]->address_u=RenderBackendTextureAddress::Wrap;
        ok=verify(expected(7),textures[0]) && ok;
        setU(-.25f); samplers[layer]->address_u=RenderBackendTextureAddress::Clamp;
        sampled[texture]=sample(texture,originalTexel&2);
        ok=verify(expected(7),textures[0]) && ok;
        setU(.5f); samplers[layer]->mag_filter=RenderBackendTextureFilter::Linear;
        const auto right=sample(texture,(originalTexel&2)+1);
        for(unsigned c=0;c<4;++c) sampled[texture][c]=(sampled[texture][c]+right[c])*.5f;
        ok=verify(expected(7),textures[0]) && ok;
        sampled[texture]=sample(texture,originalTexel);
        samplers[layer]->mag_filter=RenderBackendTextureFilter::Point;
        samplers[layer]->address_u=RenderBackendTextureAddress::Wrap; setU(originalU);
    }
    material.alpha_test=RenderBackendAlphaTest::GreaterEqual;
    material.alpha_reference=.01f;
    ok=verify(expected(7),textures[0]) && ok;
    material.alpha_reference=.1f;
    ok=verify({0,0,0,.3f},textures[0]) && ok;
    material.alpha_test=RenderBackendAlphaTest::Disabled;
    material.color_write_mask=7;
    auto preservedAlpha=expected(7); preservedAlpha[3]=.3f;
    ok=verify(preservedAlpha,textures[0]) && ok;
    material.color_write_mask=15;
    // A following texture-disabled terrain draw must select the color shader.
    setLayers(0);
    for(auto &v:quad) {v.r=.2f;v.g=.4f;v.b=.6f;v.a=.8f;}
    ok=verify({.2f,.4f,.6f,.8f},{}) && ok;
    // Terrain uses the same explicit depth/cull contracts as other materials.
    for(unsigned scenario=0;scenario<3;++scenario) {
        material.depth_write=scenario!=2;
        for(auto &v:quad) {v.z=.4f;v.r=1;v.g=v.b=0;v.a=1;}
        backend.Clear(true,true,Vector3(0,0,0),0,1,0); backend.Begin_Scene();
        bool drew=backend.Draw_Indexed_Terrain_Triangles(quad,4,front,6,{},material,terrain);
        for(auto &v:quad) {v.z=scenario==1 ? .4f : .6f;v.r=0;v.b=1;}
        material.depth_write=false;
        drew=backend.Draw_Indexed_Terrain_Triangles(quad,4,front,6,{},material,terrain) && drew;
        const bool read=capture();
        ok=drew && read && matches(320,240,scenario==0 ? std::array<float,4>{1,0,0,1} : std::array<float,4>{0,0,1,1}) && ok;
    }
    for(auto &v:quad) {v.z=.5f;v.r=1;v.g=v.b=0;v.a=1;}
    for(unsigned winding=0;winding<2;++winding) {
        backend.Clear(true,true,Vector3(0,0,0),0,1,0); backend.Begin_Scene();
        const bool drew=backend.Draw_Indexed_Terrain_Triangles(quad,4,winding ? back : front,6,{},material,terrain);
        const bool read=capture();
        ok=drew && read && matches(320,240,winding ? std::array<float,4>{0,0,0,0} : std::array<float,4>{1,0,0,1}) && ok;
    }
    Matrix4x4 camera(true);camera[0][0]=camera[1][1]=.25f;camera[0][3]=.55f;camera[1][3]=-.5f;
    backend.Set_View_Projection(camera);
    backend.Clear(true,true,Vector3(0,0,0),0,1,0); backend.Begin_Scene();
    bool drew=backend.Draw_Indexed_Terrain_Triangles(quad,4,front,6,textures[0],material,terrain);
    camera[0][3]=-.55f;backend.Set_View_Projection(camera);
    drew=backend.Draw_Indexed_Terrain_Triangles(quad,4,front,6,textures[0],material,terrain) && drew;
    const bool cameraRead=capture();
    const std::array<float,4> red={192.f/255.f,0,0,224.f/255.f};
    ok=drew && cameraRead && matches(496,365,red) && matches(144,365,red) && matches(320,240,{0,0,0,0}) && ok;
    backend.Set_View_Projection(Matrix4x4(true));
    const unsigned char stalePixel[]={255,255,255,255};
    const auto stale=backend.Create_Static_RGBA8_Texture(1,1,stalePixel,4);
    ok=stale.Is_Valid() && ok;backend.Release_Texture(stale);
    RenderBackendTextureHandle *layers[]={&terrain.shroud_texture,&terrain.cloud_texture,&terrain.noise_texture};
    backend.Begin_Scene();
    auto reject=[&](RenderBackendTextureHandle base) {
        return !backend.Draw_Indexed_Terrain_Triangles(quad,4,front,6,base,material,terrain);
    };
    setLayers(0);ok=reject({0,1}) && reject(stale) && ok;
    for(unsigned layer=0;layer<3;++layer) {
        setLayers(0);*layers[layer]=textures[layer+1];ok=reject({}) && ok;
        *layers[layer]={0,1};ok=reject(textures[0]) && ok;
        *layers[layer]={textures[layer+1].slot,0};ok=reject(textures[0]) && ok;
        *layers[layer]=stale;ok=reject(textures[0]) && ok;
        *layers[layer]=textures[layer+1];samplers[layer]->max_anisotropy=17;
        ok=reject(textures[0]) && ok;samplers[layer]->max_anisotropy=1;
        samplers[layer]->min_mip_level=16;ok=reject(textures[0]) && ok;samplers[layer]->min_mip_level=0;
        samplers[layer]->address_v=static_cast<RenderBackendTextureAddress>(2);
        ok=reject(textures[0]) && ok;samplers[layer]->address_v=RenderBackendTextureAddress::Wrap;
    }
    backend.End_Scene(false);backend.Flip_To_Primary();
    const auto target=backend.Create_Render_Texture(32,32);
    ok=target.Is_Valid() && backend.Set_Render_Texture(target) && ok;
    material.depth_test=RenderBackendDepthTest::Disabled;
    backend.Begin_Scene();
    setLayers(0);ok=reject(target) && ok;
    for(unsigned layer=0;layer<3;++layer) {
        setLayers(0);*layers[layer]=target;ok=reject(textures[0]) && ok;
    }
    backend.End_Scene(false);backend.Set_Render_Texture({});backend.Release_Texture(target);
    for(const auto texture:textures) backend.Release_Texture(texture);
    if(!ok) std::cerr << "Terrain layer arithmetic, UV/sampler, material state, or resource validation failed.\n";
    return ok;
}

bool verifyMaterials(IRenderBackend &backend)
{
    const unsigned char rgba[] = {128,64,192,128};
    const auto texture = backend.Create_Static_RGBA8_Texture(1,1,rgba,4);
    RenderBackendTexturedVertex quad[] = {
        {-.8f,-.8f,.5f,.25f,.5f,.75f,.5f,0,0},
        {-.8f, .8f,.5f,.25f,.5f,.75f,.5f,0,0},
        { .8f, .8f,.5f,.25f,.5f,.75f,.5f,0,0},
        { .8f,-.8f,.5f,.25f,.5f,.75f,.5f,0,0}};
    const unsigned short indices[] = {0,2,1,0,3,2};
    RenderBackendMaterialState material;
    material.depth_write = false;
    backend.Set_Viewport({0,0,640,480,0,1});
    backend.Set_View_Projection(Matrix4x4(true));
    bool ok = texture.Is_Valid();
    unsigned width=0,height=0;
    std::vector<unsigned char> pixels;
    auto capture = [&]() {
        backend.End_Scene(false);
        const bool read = backend.Read_Output_RGBA8(width,height,pixels) && width==640 && height==480;
        backend.Flip_To_Primary();
        return read;
    };
    auto matches = [&](unsigned channel, float expected) {
        return std::abs(int(pixels[(240*640+320)*4+channel]) -
            int(std::lround(std::clamp(expected,0.f,1.f)*255))) <= 2;
    };
    const float source[] = {.25f,.5f,.75f,.5f}, destination[] = {.2f,.4f,.6f,.8f};
    auto factor = [&](unsigned mode,unsigned channel) {
        return mode==0 ? 0.f : mode==1 ? 1.f : mode==2 ? source[channel] :
            mode==3 ? 1-source[channel] : mode==4 ? source[3] : mode==5 ? 1-source[3] : destination[channel];
    };
    // Validate every supported RGBA blend equation, including RGB factors' alpha mapping.
    for (unsigned src=0;src<7;++src) for (unsigned dst=0;dst<7;++dst) {
        material.source_blend=static_cast<RenderBackendBlendFactor>(src);
        material.destination_blend=static_cast<RenderBackendBlendFactor>(dst);
        backend.Clear(true,true,Vector3(destination[0],destination[1],destination[2]),destination[3],1,0);
        backend.Begin_Scene();
        ok = backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,{},material) && ok;
        const bool read=capture(); ok=read && ok;
        if (read) for (unsigned channel=0;channel<4;++channel)
            ok=matches(channel,source[channel]*factor(src,channel)+destination[channel]*factor(dst,channel)) && ok;
    }
    material.source_blend=RenderBackendBlendFactor::One;
    material.destination_blend=RenderBackendBlendFactor::Zero;
    // The additive and 2X texture operations multiply alpha without adding/scaling it.
    for (unsigned op=0;op<4;++op) {
        material.texture_combine=static_cast<RenderBackendTextureCombine>(op);
        backend.Clear(true,true,Vector3(0,0,0),0,1,0); backend.Begin_Scene();
        ok=backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,texture,material) && ok;
        const bool read=capture(); ok=read && ok;
        if (read) for (unsigned channel=0;channel<4;++channel) {
            const float t=rgba[channel]/255.f;
            const float expected=op==0 ? t : channel==3 || op==1 ? t*source[channel] :
                op==2 ? t+source[channel] : 2*t*source[channel];
            ok=matches(channel,expected) && ok;
        }
    }
    // Exactly representable alpha boundaries test both comparisons and inclusive equality.
    for (unsigned comparison=1;comparison<=2;++comparison) for (unsigned reference=0;reference<3;++reference) {
        material.alpha_test=static_cast<RenderBackendAlphaTest>(comparison);
        material.alpha_reference=.25f*(reference+1);
        backend.Clear(true,true,Vector3(0,0,0),0,1,0); backend.Begin_Scene();
        ok=backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,{},material) && ok;
        const bool read=capture(); ok=read && ok;
        const bool survives=comparison==1 ? reference<=1 : reference>=1;
        if (read) ok=matches(0,survives ? source[0] : 0) && ok;
    }
    material.alpha_test=RenderBackendAlphaTest::Disabled;
    // Exercise the full depth comparison domain against a known 0.5 depth clear.
    for (unsigned depth=0;depth<9;++depth) for (unsigned position=0;position<3;++position) {
        material.depth_test=static_cast<RenderBackendDepthTest>(depth);
        for (auto &v:quad) v.z=.25f*(position+1);
        backend.Clear(true,true,Vector3(0,0,0),0,.5f,0); backend.Begin_Scene();
        ok=backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,{},material) && ok;
        const bool read=capture(); ok=read && ok;
        const bool survives=depth==0 ? false : depth==1 ? position==0 : depth==2 ? position==1 :
            depth==3 ? position<=1 : depth==4 ? position==2 : depth==5 ? position!=1 :
            depth==6 ? position>=1 : true;
        if (read) ok=matches(0,survives ? source[0] : 0) && ok;
    }
    // The cached PSO's culling and color-write state must change independently.
    material.depth_test=RenderBackendDepthTest::Disabled;
    for (unsigned cull=0;cull<3;++cull) for (unsigned color=0;color<2;++color) {
        material.cull=static_cast<RenderBackendCullMode>(cull);
        material.color_write=color!=0;
        backend.Clear(true,true,Vector3(0,0,0),0,1,0); backend.Begin_Scene();
        ok=backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,{},material) && ok;
        const bool read=capture(); ok=read && ok;
        if (read) ok=matches(0,color && cull!=2 ? source[0] : 0) && ok;
    }
    // Shoreline coverage writes destination alpha while preserving scene RGB.
    // Exercise every channel mask to distinguish cached PSOs, including no writes.
    material={}; material.depth_test=RenderBackendDepthTest::Disabled; material.cull=RenderBackendCullMode::None;
    const float retained_channels[]={.1f,.3f,.2f,.75f};
    for(unsigned mask=0;mask<16;++mask) {
        material.color_write_mask=mask;
        backend.Clear(true,true,Vector3(.1f,.3f,.2f),.75f,1,0); backend.Begin_Scene();
        ok=backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,{},material) && ok;
        const bool read=capture(); ok=read && ok;
        if(read) for(unsigned channel=0;channel<4;++channel)
            ok=matches(channel,(mask&(1u<<channel)) ? source[channel] : retained_channels[channel]) && ok;
    }
    material={};
    for (unsigned write=0;write<2;++write) {
        backend.Clear(true,true,Vector3(0,0,0),0,1,0); backend.Begin_Scene();
        material.depth_write=write!=0;
        for (auto &v:quad) { v.z=.25f; v.r=1; v.g=v.b=0; v.a=1; }
        ok=backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,{},material) && ok;
        for (auto &v:quad) { v.z=.75f; v.b=1; v.r=0; }
        ok=backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,{},material) && ok;
        const bool read=capture(); ok=read && ok;
        if (read) ok=matches(write ? 0 : 2,1) && ok;
    }
    // Check the legacy 96/159 byte thresholds through the same float constants used by callers.
    for (unsigned comparison=1;comparison<=2;++comparison) for (int offset=-1;offset<=1;++offset) {
        const int threshold=comparison==1 ? 96 : 159;
        material.alpha_test=static_cast<RenderBackendAlphaTest>(comparison);
        material.alpha_reference=threshold/255.f;
        for (auto &v:quad) { v.z=.5f; v.r=1; v.b=0; v.a=(threshold+offset)/255.f; }
        backend.Clear(true,true,Vector3(0,0,0),0,1,0); backend.Begin_Scene();
        ok=backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,{},material) && ok;
        const bool read=capture(); ok=read && ok;
        if (read) ok=matches(0,(comparison==1 ? offset>=0 : offset<=0) ? 1 : 0) && ok;
    }
    material={};
    backend.Begin_Scene();
    auto rejected = [&](RenderBackendMaterialState bad) {
        return !backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,{},bad);
    };
    material.color_write_mask=16; ok=rejected(material) && ok; material={};
    material.depth_test=static_cast<RenderBackendDepthTest>(9); ok=rejected(material) && ok; material={};
    material.source_blend=static_cast<RenderBackendBlendFactor>(7); ok=rejected(material) && ok; material={};
    material.destination_blend=static_cast<RenderBackendBlendFactor>(7); ok=rejected(material) && ok; material={};
    material.cull=static_cast<RenderBackendCullMode>(3); ok=rejected(material) && ok; material={};
    material.texture_combine=static_cast<RenderBackendTextureCombine>(4); ok=rejected(material) && ok; material={};
    material.alpha_test=static_cast<RenderBackendAlphaTest>(3); ok=rejected(material) && ok; material={};
    material.alpha_reference=std::nanf(""); ok=rejected(material) && ok; material={};
    ok=!backend.Draw_Indexed_Material_Triangles(quad,4,indices,6,{0,1},material) && ok;
    backend.End_Scene(false);
    backend.Release_Texture(texture);
    if (!ok) std::cerr << "W3D material pipeline pixel/state checks failed.\n";
    return ok;
}

bool verifyDecals(IRenderBackend &backend)
{
    const unsigned char rgba[] = {128,64,192,128};
    const auto texture = backend.Create_Static_RGBA8_Texture(1, 1, rgba, 4);
    const unsigned char edge_rgba[] = {255,0,0,255, 0,255,0,255};
    const auto edge = backend.Create_Static_RGBA8_Texture(2, 1, edge_rgba, 8);
    RenderBackendTexturedVertex quad[] = {
        {-0.8f,-0.8f,0.5f, .5f,.5f,.5f,.5f, 0,0},
        {-0.8f, 0.8f,0.5f, .5f,.5f,.5f,.5f, 0,0},
        { 0.8f, 0.8f,0.5f, .5f,.5f,.5f,.5f, 0,0},
        { 0.8f,-0.8f,0.5f, .5f,.5f,.5f,.5f, 0,0},
    };
    const unsigned short front[] = {0,2,1,0,3,2};
    const unsigned short back[] = {0,1,2,0,2,3};
    const unsigned short invalid[] = {0,1,4};
    unsigned width = 0, height = 0;
    std::vector<unsigned char> pixels;
    auto pixelMatches = [](unsigned char value, float expected) {
        return std::abs(int(value) - int(std::lround(std::clamp(expected, 0.0f, 1.0f)*255))) <= 2;
    };
    auto capture = [&]() {
        backend.End_Scene(false);
        return backend.Read_Output_RGBA8(width, height, pixels) && width == 640 && height == 480;
    };
    backend.Set_Viewport({0,0,640,480,0,1});
    backend.Set_View_Projection(Matrix4x4(true));
    bool ok = texture.Is_Valid() && edge.Is_Valid() && backend.Is_Texture_Valid(texture);
    for (const auto blend : {RenderBackendDecalBlendMode::Multiply, RenderBackendDecalBlendMode::Alpha, RenderBackendDecalBlendMode::Additive}) {
        backend.Clear(true, true, Vector3(.2f,.4f,.6f), 1, 1, 0);
        backend.Begin_Scene();
        ok = !backend.Draw_Indexed_Decal_Triangles(quad,4,front,6,{},blend) &&
             !backend.Draw_Indexed_Decal_Triangles(quad,4,invalid,3,texture,blend) &&
             !backend.Draw_Indexed_Decal_Triangles(quad,4,front,6,texture,static_cast<RenderBackendDecalBlendMode>(3)) && ok;
        ok = backend.Draw_Indexed_Decal_Triangles(quad,4,front,6,texture,blend) && ok;
        const bool read = capture(); ok = read && ok;
        if (read) {
            const float source[] = {128.f/510,64.f/510,192.f/510,128.f/510};
            const float destination[] = {.2f,.4f,.6f,1};
            for (unsigned channel = 0; channel < 4; ++channel) {
                const float expected = blend == RenderBackendDecalBlendMode::Multiply ? destination[channel]*source[channel] :
                    blend == RenderBackendDecalBlendMode::Alpha ? source[channel]*source[3]+destination[channel]*(1-source[3]) :
                    source[channel]+destination[channel];
                ok = pixelMatches(pixels[(240*640+320)*4+channel], expected) && ok;
            }
        }
        backend.Flip_To_Primary();
    }
    for (auto &v : quad) { v.r=v.g=v.b=v.a=1; v.u=2.0f; v.v=.5f; }
    // Outside UV range clamps to green. The opposite winding is culled.
    for (unsigned winding = 0; winding < 2; ++winding) {
        backend.Clear(true,true,Vector3(0,0,0),1,1,0); backend.Begin_Scene();
        ok = backend.Draw_Indexed_Decal_Triangles(quad,4,winding ? back : front,6,edge,RenderBackendDecalBlendMode::Alpha) && ok;
        const bool read = capture(); ok = read && ok;
        if (read) ok = pixels[(240*640+320)*4] == 0 && pixels[(240*640+320)*4+1] == (winding ? 0 : 255) && ok;
        backend.Flip_To_Primary();
    }
    RenderBackendColorVertex base[4], probe[4];
    for (unsigned i = 0; i < 4; ++i) {
        base[i] = {quad[i].x,quad[i].y,.6f,1,0,0,1};
        probe[i] = {quad[i].x,quad[i].y,.55f,0,0,1,1};
    }
    for (unsigned depth_case = 0; depth_case < 3; ++depth_case) {
        for (auto &v : quad) v.z = depth_case == 1 ? .7f : depth_case == 2 ? .6f : .5f;
        backend.Clear(true,true,Vector3(0,0,0),1,1,0); backend.Begin_Scene();
        ok = backend.Draw_Indexed_Triangles(base,4,front,6) && ok;
        ok = backend.Draw_Indexed_Decal_Triangles(quad,4,front,6,edge,RenderBackendDecalBlendMode::Alpha) && ok;
        if (depth_case == 0) ok = backend.Draw_Indexed_Triangles(probe,4,front,6) && ok;
        const bool read = capture(); ok = read && ok;
        if (read) {
            const auto center = (240*640+320)*4;
            // Blue proves no depth write; red proves depth rejection; green proves LEQUAL.
            ok = pixels[center+(depth_case == 0 ? 2 : depth_case == 1 ? 0 : 1)] == 255 && ok;
        }
        backend.Flip_To_Primary();
    }
    const auto offscreen = backend.Create_Render_Texture(8,8);
    ok = backend.Set_Render_Texture(offscreen) && ok;
    backend.Clear(true,false,Vector3(0,0,0),1,1,0); backend.Begin_Scene();
    ok = !backend.Draw_Indexed_Decal_Triangles(quad,4,front,6,texture,RenderBackendDecalBlendMode::Alpha) && ok;
    backend.End_Scene(false); ok = backend.Set_Render_Texture({}) && ok;
    backend.Release_Texture(offscreen); backend.Release_Texture(edge); backend.Release_Texture(texture);
    return !backend.Is_Texture_Valid(texture) && ok;
}
} // namespace

int main()
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    HWND window = createHiddenWindow(instance);
    if (window == nullptr)
    {
        std::cerr << "D3D12 backend smoke failed: could not create Win32 window.\n";
        return 1;
    }

    IRenderBackend *backend = Create_Render_Backend(window, false);
    if (backend == nullptr)
    {
        DestroyWindow(window);
        UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 backend smoke failed: backend initialization failed.\n";
        return 2;
    }

    int output_width = 0, output_height = 0, output_bits = 0;
    bool output_windowed = false;
    if (!backend->Configure_Output(640, 480, true) ||
        !backend->Is_Device_Ready() ||
        !backend->Has_Stencil() ||
        !backend->Get_Output_Description(
            output_width, output_height, output_bits, output_windowed) ||
        output_width != 640 || output_height != 480 || output_bits != 32 || !output_windowed)
    {
        delete backend;
        DestroyWindow(window);
        UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 backend smoke failed: output resize/description contract failed.\n";
        return 10;
    }

    RenderBackendViewport viewport{0, 0, 640, 480, 0.0f, 1.0f};
    if (backend->Get_Swap_Interval() != 1 ||
        !backend->Set_Swap_Interval(0) || backend->Get_Swap_Interval() != 0 ||
        !backend->Set_Swap_Interval(4) || backend->Get_Swap_Interval() != 4 ||
        backend->Set_Swap_Interval(5) || backend->Get_Swap_Interval() != 4 ||
        !backend->Set_Swap_Interval(0))
    {
        delete backend;
        DestroyWindow(window);
        UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 backend smoke failed: presentation interval contract failed.\n";
        return 11;
    }
    backend->Set_Viewport(viewport);

    unsigned int captured_width = 99, captured_height = 99;
    std::vector<unsigned char> captured_pixels{99};
    if (backend->Read_Output_RGBA8(captured_width, captured_height, captured_pixels) ||
        captured_width != 0 || captured_height != 0 || !captured_pixels.empty())
    {
        delete backend;
        DestroyWindow(window);
        UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 backend smoke failed: capture before submission was accepted.\n";
        return 12;
    }

    const RenderBackendColorVertex triangle_vertices[] = {
        {-0.65f, -0.55f, 0.50f, 1.00f, 0.15f, 0.10f, 1.00f},
        {0.00f, 0.65f, 0.50f, 0.10f, 1.00f, 0.20f, 1.00f},
        {0.65f, -0.55f, 0.50f, 0.10f, 0.25f, 1.00f, 1.00f},
    };
    const unsigned short triangle_indices[] = {0, 1, 2};
    const RenderBackendGeometryHandle static_triangle =
        backend->Create_Static_Indexed_Color_Geometry(triangle_vertices, 3, triangle_indices, 3);
    if (!static_triangle.Is_Valid())
    {
        delete backend;
        DestroyWindow(window);
        UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 backend smoke failed: persistent geometry creation failed.\n";
        return 3;
    }

    const RenderBackendTexturedVertex textured_vertices[] = {
        {-0.75f, -0.70f, 0.40f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f},
        {-0.75f,  0.70f, 0.40f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f},
        { 0.75f,  0.70f, 0.40f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f},
        { 0.75f, -0.70f, 0.40f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
    };
    const unsigned short textured_indices[] = {0, 1, 2, 0, 2, 3};
    const unsigned char checker_rgba[] = {
        255,  64,  64, 255,    64, 255,  64, 255,
         64,  64, 255, 255,   255, 255,  64, 255,
    };
    const RenderBackendGeometryHandle textured_quad =
        backend->Create_Static_Indexed_Textured_Geometry(textured_vertices, 4, textured_indices, 6);
    const RenderBackendTextureHandle checker_texture =
        backend->Create_Static_RGBA8_Texture(2, 2, checker_rgba, 8);
    if (!textured_quad.Is_Valid() || !checker_texture.Is_Valid())
    {
        backend->Release_Texture(checker_texture);
        backend->Release_Static_Geometry(textured_quad);
        backend->Release_Static_Geometry(static_triangle);
        delete backend;
        DestroyWindow(window);
        UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 backend smoke failed: persistent textured resources could not be created.\n";
        return 4;
    }

    // Exercise WW3D's deferred-present contract first: End_Scene(false) may
    // be followed by another scene before Flip_To_Primary(). This is used by
    // existing multi-pass/off-screen callers and must not dead-end the frame.
    backend->Clear(true, true, Vector3(0.04f, 0.08f, 0.12f), 1.0f, 1.0f, 0);
    backend->Begin_Scene();
    if (!backend->Draw_Static_Indexed_Textured_Geometry(textured_quad, checker_texture))
    {
        backend->Release_Texture(checker_texture);
        backend->Release_Static_Geometry(textured_quad);
        backend->Release_Static_Geometry(static_triangle);
        delete backend;
        DestroyWindow(window);
        UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 backend smoke failed: textured indexed geometry/2D submission failed.\n";
        return 5;
    }
    backend->End_Scene(false);

    backend->Clear(true, true, Vector3(0.06f, 0.08f, 0.12f), 1.0f, 1.0f, 0);
    backend->Begin_Scene();
    backend->End_Scene(false);
    backend->Flip_To_Primary();

    if (!backend->Read_Output_RGBA8(captured_width, captured_height, captured_pixels) ||
        captured_width != 640 || captured_height != 480 ||
        captured_pixels.size() != 640u * 480u * 4u ||
        captured_pixels[0] != 15 || captured_pixels[1] != 20 ||
        captured_pixels[2] != 31 || captured_pixels[3] != 255)
    {
        delete backend;
        DestroyWindow(window);
        UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 backend smoke failed: latest presented output capture is incorrect.\n";
        return 13;
    }

    for (int frame = 0; frame < 6; ++frame)
    {
        backend->Reset_Frame_Statistics();
        backend->Set_Swap_Interval(static_cast<unsigned int>(frame % 2));
        const float phase = static_cast<float>(frame) / 5.0f;
        backend->Clear(true, true, Vector3(0.08f + phase * 0.08f, 0.08f, 0.12f), 1.0f, 1.0f, 0);
        backend->Begin_Scene();
        const bool color_draw_ok = frame == 0
            ? backend->Draw_Indexed_Triangles(triangle_vertices, 3, triangle_indices, 3)
            : backend->Draw_Static_Indexed_Color_Geometry(static_triangle);
        const bool textured_draw_ok = backend->Draw_Static_Indexed_Textured_Geometry(textured_quad, checker_texture);
        const bool screen_draw_ok = backend->Draw_2D_Indexed_Triangles(
            triangle_vertices, 3, triangle_indices, 3, RenderBackend2DBlendMode::Alpha);
        const RenderBackendFrameStatistics counters = backend->Get_Frame_Statistics();
        if (!color_draw_ok || !textured_draw_ok || !screen_draw_ok ||
            counters.draw_calls != 3 || counters.triangles != 4 || counters.vertices != 10)
        {
            backend->Release_Texture(checker_texture);
            backend->Release_Static_Geometry(textured_quad);
            backend->Release_Static_Geometry(static_triangle);
            delete backend;
            DestroyWindow(window);
            UnregisterClassW(WindowClassName, instance);
            std::cerr << "D3D12 backend smoke failed: indexed geometry submission failed.\n";
            return 6;
        }
        backend->End_Scene(true);
    }

    // A non-symmetric transform catches row/column transposition. Change it
    // within a scene and interleave UI to catch stale or shared root constants.
    Matrix4x4 view_projection(true);
    view_projection[0][0] = 0.25f;
    view_projection[1][1] = 0.25f;
    view_projection[0][3] = 0.55f;
    view_projection[1][3] = -0.5f;
    const RenderBackendColorVertex top_left_vertices[] = {
        {-1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
        { 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
        {-1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
    };
    bool camera_ok = true;
    for (unsigned int path = 0; path < 3; ++path)
    {
        view_projection[0][3] = 0.55f;
        backend->Set_View_Projection(view_projection);
        backend->Clear(true, true, Vector3(0.0f, 0.0f, 0.0f), 1.0f, 1.0f, 0);
        backend->Begin_Scene();
        auto draw_world = [&]() {
            if (path == 0) return backend->Draw_Indexed_Triangles(triangle_vertices, 3, triangle_indices, 3);
            if (path == 1) return backend->Draw_Static_Indexed_Color_Geometry(static_triangle);
            return backend->Draw_Static_Indexed_Textured_Geometry(textured_quad, checker_texture);
        };
        camera_ok = draw_world() && camera_ok;
        camera_ok = backend->Draw_2D_Indexed_Triangles(
            top_left_vertices, 3, triangle_indices, 3, RenderBackend2DBlendMode::Opaque) && camera_ok;
        view_projection[0][3] = -0.55f;
        backend->Set_View_Projection(view_projection);
        camera_ok = draw_world() && camera_ok;
        backend->End_Scene(false);
        camera_ok = backend->Read_Output_RGBA8(captured_width, captured_height, captured_pixels) && camera_ok;
        if (camera_ok)
        {
            auto colored_pixel = [&](unsigned int x, unsigned int y) {
                const std::size_t offset = (y * captured_width + x) * 4u;
                return captured_pixels[offset] || captured_pixels[offset + 1] || captured_pixels[offset + 2];
            };
            const std::size_t ui_pixel = (10u * captured_width + 10u) * 4u;
            camera_ok = captured_width == 640 && captured_height == 480 &&
                colored_pixel(496, 365) && colored_pixel(144, 365) && !colored_pixel(320, 240) &&
                captured_pixels[ui_pixel] == 255 && captured_pixels[ui_pixel + 1] == 0 &&
                captured_pixels[ui_pixel + 2] == 0;
        }
        backend->Flip_To_Primary();
    }
    // Projected shadows render into one texture, copy to another, and sample
    // that copy later. Exercise actual GPU pixels and restore the output state.
    const auto render_texture = backend->Create_Render_Texture(61, 37);
    const auto shadow_texture = backend->Create_Render_Texture(61, 37);
    const auto wrong_size = backend->Create_Render_Texture(17, 19);
    bool target_ok = render_texture.Is_Valid() && shadow_texture.Is_Valid() && wrong_size.Is_Valid() &&
        !backend->Create_Render_Texture(0, 37).Is_Valid() &&
        !backend->Set_Render_Texture(checker_texture) && !backend->Set_Render_Texture({1, 0}) &&
        !backend->Copy_Texture(shadow_texture, wrong_size) &&
        !backend->Copy_Texture(shadow_texture, shadow_texture);
    const Matrix4x4 identity(true);
    for (unsigned int path = 0; target_ok && path < 3; ++path)
    {
        backend->Set_Viewport({80, 60, 480, 360, 0.0f, 1.0f});
        backend->Set_View_Projection(view_projection);
        backend->Clear(true, true, Vector3(0.0f, 0.0f, 0.0f), 1.0f, 1.0f, 0);
        backend->Begin_Scene();
        target_ok = backend->Draw_Static_Indexed_Color_Geometry(static_triangle) && target_ok;
        backend->End_Scene(false); // A shadow pass must preserve this deferred output.
        target_ok = backend->Read_Output_RGBA8(captured_width, captured_height, captured_pixels) && target_ok;
        const auto retained_output = captured_pixels;
        const std::size_t restored_pixel = (330u * captured_width + 188u) * 4u;
        target_ok = target_ok && (captured_pixels[restored_pixel] || captured_pixels[restored_pixel + 1]);

        target_ok = backend->Set_Render_Texture(render_texture) && target_ok;
        int width = 0, height = 0, bits = 0;
        bool windowed = false;
        target_ok = backend->Get_Render_Target_Size(width, height) && width == 61 && height == 37 && target_ok;
        target_ok = backend->Get_Output_Description(width, height, bits, windowed) &&
            width == 640 && height == 480 && target_ok;
        backend->Set_View_Projection(identity);
        backend->Clear(true, true, Vector3(0.25f, 0.5f, 0.75f), 1.0f, 1.0f, 0);
        backend->Begin_Scene();
        target_ok = !backend->Set_Render_Texture({}) &&
            !backend->Create_Render_Texture(61, 37).Is_Valid() &&
            !backend->Copy_Texture(shadow_texture, render_texture) &&
            !backend->Draw_Static_Indexed_Textured_Geometry(textured_quad, render_texture) && target_ok;
        if (path == 0) target_ok = backend->Draw_Indexed_Triangles(triangle_vertices, 3, triangle_indices, 3) && target_ok;
        if (path == 1) target_ok = backend->Draw_Static_Indexed_Color_Geometry(static_triangle) && target_ok;
        if (path == 2) target_ok = backend->Draw_Static_Indexed_Textured_Geometry(textured_quad, checker_texture) && target_ok;
        target_ok = backend->Draw_2D_Indexed_Triangles(top_left_vertices, 3, triangle_indices, 3,
            path == 0 ? RenderBackend2DBlendMode::Opaque :
            path == 1 ? RenderBackend2DBlendMode::Alpha : RenderBackend2DBlendMode::Additive) && target_ok;
        backend->End_Scene(true); // Offscreen submission must never present or replace output capture.
        target_ok = backend->Read_Output_RGBA8(captured_width, captured_height, captured_pixels) &&
            captured_pixels == retained_output && target_ok;
        target_ok = backend->Copy_Texture(shadow_texture, render_texture) && target_ok;
        target_ok = backend->Set_Render_Texture({}) && target_ok;
        backend->Flip_To_Primary();

        // Selection restored both the saved camera and pixel viewport.
        backend->Clear(true, true, Vector3(0.0f, 0.0f, 0.0f), 1.0f, 1.0f, 0);
        backend->Begin_Scene();
        target_ok = backend->Draw_Static_Indexed_Color_Geometry(static_triangle) && target_ok;
        backend->End_Scene(false);
        target_ok = backend->Read_Output_RGBA8(captured_width, captured_height, captured_pixels) &&
            captured_pixels == retained_output && target_ok;
        backend->Flip_To_Primary();

        backend->Set_Viewport(viewport);
        backend->Set_View_Projection(identity);
        backend->Clear(true, true, Vector3(0.0f, 0.0f, 0.0f), 1.0f, 1.0f, 0);
        backend->Begin_Scene();
        target_ok = backend->Draw_Static_Indexed_Textured_Geometry(textured_quad, shadow_texture) && target_ok;
        backend->End_Scene(false);
        target_ok = backend->Read_Output_RGBA8(captured_width, captured_height, captured_pixels) && target_ok;
        const std::size_t lower_right = (420u * captured_width + 540u) * 4u;
        const std::size_t top_left = (100u * captured_width + 100u) * 4u;
        const std::size_t middle = (240u * captured_width + 320u) * 4u;
        target_ok = target_ok && captured_pixels[top_left] == 255 &&
            captured_pixels[top_left + 1] == (path == 2 ? 128 : 0) &&
            captured_pixels[top_left + 2] == (path == 2 ? 191 : 0) &&
            captured_pixels[lower_right] == 0 && // Outside the sampled quad.
            (captured_pixels[middle] != 64 || captured_pixels[middle + 1] != 128 || captured_pixels[middle + 2] != 191);
        // Inside the quad, away from every offscreen draw, the copied clear is exact.
        const std::size_t background = (390u * captured_width + 540u) * 4u;
        target_ok = target_ok && captured_pixels[background] == 64 &&
            captured_pixels[background + 1] == 128 && captured_pixels[background + 2] == 191 &&
            captured_pixels[background + 3] == 255;
        backend->Flip_To_Primary();
    }
    target_ok = backend->Set_Render_Texture(render_texture) && target_ok;
    backend->Release_Texture(render_texture); // Release of a selected target restores output.
    const auto replacement_texture = backend->Create_Render_Texture(61, 37);
    output_width = output_height = 0;
    target_ok = backend->Get_Render_Target_Size(output_width, output_height) &&
        output_width == 640 && output_height == 480 && replacement_texture.Is_Valid() &&
        replacement_texture.slot == render_texture.slot && replacement_texture.generation != render_texture.generation &&
        !backend->Set_Render_Texture(render_texture) && !backend->Copy_Texture(shadow_texture, render_texture) && target_ok;
    backend->Release_Texture(replacement_texture);
    backend->Release_Texture(wrong_size);
    backend->Release_Texture(shadow_texture);
    backend->Release_Texture(checker_texture);
    backend->Release_Static_Geometry(textured_quad);
    backend->Release_Static_Geometry(static_triangle);
    if (!target_ok)
    {
        delete backend;
        DestroyWindow(window);
        UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 backend smoke failed: render texture lifecycle, pixels, or output restoration is incorrect.\n";
        return 16;
    }
    if (!camera_ok)
    {
        delete backend;
        DestroyWindow(window);
        UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 backend smoke failed: camera transform or screen-space isolation is incorrect.\n";
        return 15;
    }
    if (!verifyDeferredTextureRelease(*backend) || !verifySceneTextureLoading(*backend) || !verifyProjectedTextureAndMips(*backend) ||
        !verifyMaterials(*backend) || !verifySamplers(*backend) || !verifyTerrain(*backend) || !verifyPersistentTerrainAndMaterials(*backend) || !verifyTreeShroud(*backend) || !verifyStencil(*backend) || !verifyDecals(*backend)) {
        delete backend; DestroyWindow(window); UnregisterClassW(WindowClassName, instance);
        std::cerr << "D3D12 decal blending, clamp sampling, culling, depth, or validation failed.\n";
        return 18;
    }
    // A width not divisible by the GPU row alignment catches padding leaks;
    // an asymmetric draw catches vertical inversion and RGBA channel swaps.
    const unsigned char retained_pixel[] = {0,255,0,255};
    const auto retained_texture = backend->Create_Static_RGBA8_Texture(1,1,retained_pixel,4);
    const auto retained_target = backend->Create_Render_Texture(13,7);
    bool capture_ok = retained_texture.Is_Valid() && retained_target.Is_Valid() &&
        backend->Configure_Output(643, 479, true) &&
        backend->Is_Texture_Valid(retained_texture) && backend->Is_Texture_Valid(retained_target);
    capture_ok = capture_ok && !backend->Read_Output_RGBA8(captured_width, captured_height, captured_pixels);
    backend->Set_Viewport(RenderBackendViewport{0, 0, 643, 479, 0.0f, 1.0f});
    backend->Clear(true, true, Vector3(0.25f, 0.5f, 0.75f), 1.0f, 1.0f, 0);
    backend->Begin_Scene();
    capture_ok = capture_ok && !backend->Read_Output_RGBA8(captured_width, captured_height, captured_pixels);
    capture_ok = capture_ok && backend->Draw_2D_Indexed_Triangles(
        top_left_vertices, 3, triangle_indices, 3, RenderBackend2DBlendMode::Opaque);
    const RenderBackendTexturedVertex retained_triangle[] = {
        {-0.25f,-0.25f,0, 1,1,1,1, 0,0},
        {0,0.25f,0, 1,1,1,1, 0,0},
        {0.25f,-0.25f,0, 1,1,1,1, 0,0}};
    RenderBackendMaterialState retained_material;
    retained_material.screen_space = true;
    retained_material.depth_test = RenderBackendDepthTest::Disabled;
    retained_material.depth_write = false;
    retained_material.cull = RenderBackendCullMode::None;
    capture_ok = capture_ok && backend->Draw_Indexed_Material_Triangles(
        retained_triangle,3,triangle_indices,3,retained_texture,retained_material);
    backend->End_Scene(false);
    capture_ok = capture_ok && backend->Read_Output_RGBA8(captured_width, captured_height, captured_pixels) &&
        captured_width == 643 && captured_height == 479 && captured_pixels.size() == 643u * 479u * 4u;
    if (capture_ok)
    {
        const std::size_t lower_right = (643u * 479u - 1u) * 4u;
        capture_ok = captured_pixels[0] == 255 && captured_pixels[1] == 0 && captured_pixels[2] == 0 &&
            captured_pixels[3] == 255 && captured_pixels[lower_right] == 64 &&
            captured_pixels[lower_right + 1] == 128 && captured_pixels[lower_right + 2] == 191 &&
            captured_pixels[lower_right + 3] == 255;
        const std::size_t center = (239u*643u+321u)*4u;
        capture_ok = capture_ok && captured_pixels[center]==0 && captured_pixels[center+1]==255 &&
            captured_pixels[center+2]==0 && captured_pixels[center+3]==255;
    }
    backend->Flip_To_Primary();
    backend->Release_Texture(retained_texture);
    backend->Release_Texture(retained_target);
    delete backend;
    // Texture owners may outlive WW3D shutdown. A replacement device must not
    // reinterpret an old slot/generation as its newly allocated render target.
    backend = Create_Render_Backend(window, false);
    bool device_generation_ok = backend != nullptr;
    if (backend != nullptr)
    {
        const auto new_target = backend->Create_Render_Texture(61, 37);
        device_generation_ok = new_target.Is_Valid() && new_target.slot == checker_texture.slot &&
            new_target.generation != checker_texture.generation && !backend->Set_Render_Texture(checker_texture);
        backend->Release_Texture(checker_texture);
        device_generation_ok = backend->Set_Render_Texture(new_target) && device_generation_ok;
        backend->Release_Texture(new_target);
        delete backend;
    }
    DestroyWindow(window);
    UnregisterClassW(WindowClassName, instance);

    if (!capture_ok)
    {
        std::cerr << "D3D12 backend smoke failed: resized/deferred output capture is incorrect.\n";
        return 14;
    }
    if (!device_generation_ok)
    {
        std::cerr << "D3D12 backend smoke failed: stale texture handle crossed device recreation.\n";
        return 17;
    }
    std::cout << "D3D12 backend smoke passed: camera transforms, render textures/copies, device generations, textured geometry, isolated 2D blending, presentation intervals, and resized/deferred RGBA8 readback.\n";
    return 0;
}
