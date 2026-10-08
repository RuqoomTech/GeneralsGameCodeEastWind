/* Command & Conquer Generals Zero Hour(tm), Copyright 2025 Electronic Arts Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "meshrenderer.h"
#include "IRenderBackend.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/mesh.h"
#include "WW3D2/meshmdl.h"
#include "WW3D2/camera.h"
#include "WW3D2/mapper.h"
#include "WW3D2/matpass.h"
#include "WW3D2/texture.h"
#include "WW3D2/decalmsh.h"
#include "WW3D2/meshdebugger.h"
#include "WW3D2/sortingrenderer.h"
#include "WWMath/obbox.h"
#include "WWDebug/wwdebug.h"
#include "WWLib/win.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <set>
#include <vector>

MeshRendererClass TheMeshRenderer;

namespace {
float saturate(float value) { return std::max(0.0f, std::min(1.0f, value)); }
Vector3 multiply(const Vector3 &a, const Vector3 &b)
{
    return Vector3(a.X * b.X, a.Y * b.Y, a.Z * b.Z);
}
Vector3 rgb(unsigned int value)
{
    return Vector3(((value >> 16) & 255) / 255.0f,
        ((value >> 8) & 255) / 255.0f, (value & 255) / 255.0f);
}
Vector3 materialColor(VertexMaterialClass::ColorSourceType source,
    const Vector3 &color, unsigned int diffuse, unsigned int specular)
{
    if (source == VertexMaterialClass::COLOR1) return rgb(diffuse);
    if (source == VertexMaterialClass::COLOR2) return rgb(specular);
    return color;
}
bool shadeVertex(VertexMaterialClass *material, const LightEnvironmentClass *lighting,
    bool lighting_enabled, const Vector3 &position, const Vector3 &normal,
    unsigned int diffuse_color, unsigned int specular_color, float alpha_override,
    float emissive_scale, bool additive, Vector3 &color, float &alpha)
    {
        if (!material) return false;
        color = rgb(diffuse_color);
        alpha = ((diffuse_color >> 24) & 255) / 255.0f;
        if (material->Get_Lighting() && lighting_enabled) {
            if (!(lighting != nullptr)) return false;
            Vector3 ambient, diffuse, emissive;
            material->Get_Ambient(&ambient); material->Get_Diffuse(&diffuse); material->Get_Emissive(&emissive);
            ambient = materialColor(material->Get_Ambient_Color_Source(), ambient, diffuse_color, specular_color);
            diffuse = materialColor(material->Get_Diffuse_Color_Source(), diffuse, diffuse_color, specular_color);
            emissive = materialColor(material->Get_Emissive_Color_Source(), emissive, diffuse_color, specular_color);
            if (additive && alpha_override != 1.0f) diffuse.Set(alpha_override, alpha_override, alpha_override);
            color = multiply(ambient, lighting->Get_Equivalent_Ambient()) + emissive * emissive_scale;
            for (int light = 0; light < lighting->Get_Light_Count(); ++light) {
                Vector3 direction = lighting->Get_Light_Direction(light);
                Vector3 illumination = lighting->Get_Light_Diffuse(light);
                float attenuation = 1.0f;
                if (lighting->isPointLight(light)) {
                    direction = lighting->getPointCenter(light) - position;
                    const float distance = direction.Length();
                    const float outer = lighting->getPointOrad(light);
                    const float inner = lighting->getPointIrad(light);
                    if (distance > outer || outer <= 0.0f) continue;
                    const float linear = std::fabs(inner - outer) < 1.0e-5f ? 0.0f : 0.1f / inner;
                    attenuation = 1.0f / (1.0f + linear * distance + 8.0f * distance * distance / (outer * outer));
                    direction.Normalize();
                    illumination = lighting->getPointDiffuse(light);
                    color += multiply(ambient, lighting->getPointAmbient(light)) * attenuation;
                }
                color += multiply(diffuse, illumination) * (std::max(0.0f, Vector3::Dot_Product(normal, direction)) * attenuation);
            }
            alpha = material->Get_Diffuse_Color_Source() == VertexMaterialClass::COLOR1 ? alpha : material->Get_Opacity();
        }
        if (alpha_override != 1.0f) alpha = alpha_override;
        return true;
    }

// Renderer diagnostic state is never part of asset CRC or deterministic state.
bool failure(const MeshClass *mesh, const char *reason)
{
    char message[1024];
    std::snprintf(message, sizeof(message), "MeshRenderer: cannot draw %s: %s\n", mesh->Get_Name(), reason);
    // Keep migration failures observable in the Release executable too.
    std::fputs(message, stderr);
    OutputDebugStringA(message);
    return false;
}
}

struct MeshRendererClass::Impl
{
    struct Task
    {
        MeshClass *mesh;
        MaterialPassClass *pass;
        bool delayed;
        float alpha;
        float emissive;
        Matrix3D transform;
        LightEnvironmentClass lighting;
        bool has_lighting;
        bool has_uv_override = false;
        Vector2 uv_override = Vector2(0, 0);
        Task(MeshClass *object, MaterialPassClass *material, bool after,
            float opacity, float emissive_scale)
            : mesh(object), pass(material), delayed(after), alpha(opacity),
              emissive(emissive_scale), transform(object->Get_Transform()),
              has_lighting(object->Get_Lighting_Environment() != nullptr)
        {
            mesh->Add_Ref();
            if (pass) pass->Add_Ref();
            if (has_lighting) lighting = *object->Get_Lighting_Environment();
            if (object->Get_User_Data() && *static_cast<const int *>(object->Get_User_Data()) == RenderObjClass::USER_DATA_MATERIAL_OVERRIDE) {
                const auto *override = static_cast<const RenderObjClass::Material_Override *>(object->Get_User_Data());
                uv_override = override->customUVOffset;
                has_uv_override = true;
            }
        }
        ~Task() { if (pass) pass->Release_Ref(); mesh->Release_Ref(); }
    };

    CameraClass *camera = nullptr;
    bool lighting_enabled = true;
    bool force_multiply = false;
    unsigned int draws = 0;
    unsigned int failed = 0;
    std::vector<std::unique_ptr<Task>> tasks;
    std::vector<DecalMeshClass *> decals;
    std::set<MeshModelClass *> models;

    void clear()
    {
        tasks.clear();
        for (auto *decal : decals) decal->Release_Ref();
        decals.clear();
    }

    bool shade(Task &task, VertexMaterialClass *material, const Vector3 &position,
        const Vector3 &normal, unsigned int diffuse_color, unsigned int specular_color,
        Vector3 &color, float &alpha)
    {
        if (!shadeVertex(material, task.has_lighting ? &task.lighting : nullptr,
                lighting_enabled, position, normal, diffuse_color, specular_color,
                task.alpha, task.emissive, !task.pass && task.mesh->Is_Additive(), color, alpha))
            return failure(task.mesh, "missing material or mesh light environment");
        return true;
    }

    bool draw(Task &task, int pass_index)
    {
        auto *mesh = task.mesh;
        auto *model = mesh->Peek_Model();
        auto *backend = WW3D::Get_Render_Backend();
        if (!model || !camera || !backend) return failure(mesh, "missing model, camera, or backend");
        if (MeshRendererDebugger::Is_Enabled() && mesh->Is_Disabled_By_Debugger()) return true;
        const bool sorted = model->Get_Flag(MeshGeometryClass::SORT) && WW3D::Is_Sorting_Enabled();
        if (WW3D::Get_NPatches_Level() > 1 && model->Get_Flag(MeshGeometryClass::ALLOW_NPATCHES))
            return failure(mesh, "NPatch tessellation is not migrated");
        if (model->GapFiller)
            return failure(mesh, "NPatch gap geometry is not migrated");
        const int vertex_count = model->Get_Vertex_Count();
        const int polygon_count = model->Get_Polygon_Count();
        if (!vertex_count || !polygon_count) return true;
        const Vector3 *positions = model->Get_Vertex_Array();
        const Vector3 *normals = model->Get_Vertex_Normal_Array();
        const TriIndex *polygons = model->Get_Polygon_Array();
        if (!positions || !normals || !polygons) return failure(mesh, "incomplete CPU geometry");

        Matrix3D world = task.transform;
        std::vector<Vector3> skin_positions, skin_normals;
        const bool skin = model->Get_Flag(MeshGeometryClass::SKIN) != 0;
        if (skin) {
            skin_positions.resize(vertex_count);
            skin_normals.resize(vertex_count);
            mesh->Get_Deformed_Vertices(skin_positions.data(), skin_normals.data());
            positions = skin_positions.data();
            normals = skin_normals.data();
            world.Make_Identity();
        } else if (!task.pass && model->Get_Flag(MeshGeometryClass::ALIGNED)) {
            const Vector3 position = task.transform.Get_Translation();
            Vector3 camera_z;
            camera->Get_Transform().Get_Z_Vector(&camera_z);
            world.Obj_Look_At(position, position + camera_z, 0.0f);
        } else if (!task.pass && model->Get_Flag(MeshGeometryClass::ORIENTED)) {
            const Vector3 position = task.transform.Get_Translation();
            world.Obj_Look_At(position, camera->Get_Transform().Get_Translation(), 0.0f);
        }
        Matrix3D inverse_world;
        world.Get_Inverse(inverse_world);
        Matrix3D view;
        camera->Get_Transform().Get_Orthogonal_Inverse(view);

        SimpleDynVecClass<uint32> apt;
        if (task.pass && !skin && task.pass->Get_Cull_Volume() && MaterialPassClass::Is_Per_Polygon_Culling_Enabled()) {
            Matrix3D inverse;
            task.transform.Get_Orthogonal_Inverse(inverse);
            OBBoxClass local_box;
            OBBoxClass::Transform(inverse, *task.pass->Get_Cull_Volume(), &local_box);
            Vector3 direction;
            local_box.Basis.Get_Z_Vector(&direction);
            direction = -direction;
            if (model->Has_Cull_Tree()) model->Generate_Rigid_APT(local_box, direction, apt);
            else model->Generate_Rigid_APT(direction, apt);
        } else {
            apt.Resize(polygon_count);
            for (int i = 0; i < polygon_count; ++i) apt.Add(i);
        }

        RenderBackendMaterialState projected_material;
        RenderBackendTerrainState projection;
        TextureClass *projected_texture = nullptr;
        if (task.pass && task.pass->Prepare_Projected_Mesh_Pass(projected_material, projection, projected_texture)) {
            if (!projected_texture || !projected_texture->Ensure_Renderer_Texture())
                return failure(mesh, "projected mesh texture unavailable");
            // Rigid local geometry and world projection stay on the GPU. Skin
            // positions are already deformed in world space by the existing owner.
            for (int row=0; row<3; ++row) for (int column=0; column<4; ++column)
                projection.world_transform[row*4+column] = world[row][column];
            std::vector<RenderBackendTerrainVertex> projected_vertices;
            std::vector<unsigned short> projected_indices;
            const auto flush_projected = [&]() {
                if (projected_indices.empty()) return true;
                const bool result = backend->Draw_Indexed_Terrain_Triangles(projected_vertices.data(),
                    static_cast<unsigned int>(projected_vertices.size()), projected_indices.data(),
                    static_cast<unsigned int>(projected_indices.size()), projected_texture->Get_Renderer_Texture(),
                    projected_material, projection);
                if (result) ++draws;
                projected_vertices.clear(); projected_indices.clear();
                return result;
            };
            for (int p=0; p<apt.Count(); ++p) {
                const unsigned int polygon = apt[p];
                if (polygon >= static_cast<unsigned int>(polygon_count)) return failure(mesh, "invalid projected APT polygon");
                const TriIndex &triangle = polygons[polygon];
                if (projected_vertices.size()+3 > 65535 && !flush_projected()) return false;
                for (int corner=0; corner<3; ++corner) {
                    const int index = triangle[corner];
                    if (index < 0 || index >= vertex_count) return failure(mesh, "invalid projected vertex");
                    const Vector3 &position = positions[index];
                    projected_indices.push_back(static_cast<unsigned short>(projected_vertices.size()));
                    projected_vertices.push_back({position.X,position.Y,position.Z,1,1,1,1,0,0,0,0,0,0,0,0});
                }
            }
            if (!flush_projected()) return failure(mesh, "D3D12 projected mesh submission rejected");
            return true;
        }

        std::vector<RenderBackendTexturedVertex> vertices;
        std::vector<unsigned short> indices;
        TextureClass *batch_texture = nullptr;
        VertexMaterialClass *batch_material = nullptr;
        ShaderClass batch_shader;
        RenderBackendMaterialState batch_state;
        TextureMapperRenderMapping mapping;
        RenderBackendTextureHandle texture_handle;
        bool have_batch = false;
        const auto flush = [&]() {
            if (vertices.empty()) return true;
            const bool submitted = sorted ? SortingRendererClass::Submit_CPU_Triangles(vertices.data(),
                    static_cast<unsigned int>(vertices.size()), indices.data(),
                    static_cast<unsigned int>(indices.size()), batch_texture, batch_state,
                    *camera, &mesh->Get_Bounding_Sphere()) :
                backend->Draw_Indexed_Material_Triangles(vertices.data(),
                    static_cast<unsigned int>(vertices.size()), indices.data(),
                    static_cast<unsigned int>(indices.size()), texture_handle, batch_state);
            if (!submitted)
                return failure(mesh, "D3D12 material submission rejected");
            ++draws;
            vertices.clear(); indices.clear();
            return true;
        };

        for (int p = 0; p < apt.Count(); ++p) {
            const unsigned int polygon_index = apt[p];
            if (polygon_index >= static_cast<unsigned int>(polygon_count)) return failure(mesh, "invalid APT polygon index");
            const TriIndex &triangle = polygons[polygon_index];
            for (int corner = 0; corner < 3; ++corner)
                if (triangle[corner] >= vertex_count) return failure(mesh, "invalid vertex index");
            TextureClass *texture = task.pass ? task.pass->Peek_Texture() : model->Peek_Texture(polygon_index, pass_index);
            VertexMaterialClass *material = task.pass ? task.pass->Peek_Material() : model->Peek_Material(triangle.I, pass_index);
            ShaderClass shader = task.pass ? task.pass->Peek_Shader() : model->Get_Shader(polygon_index, pass_index);
            if (!task.pass && model->Peek_Texture(polygon_index, pass_index, 1))
                return failure(mesh, "second texture stage is not migrated");
            if (!material) return failure(mesh, "missing vertex material");
            if (!have_batch || texture != batch_texture || material != batch_material || shader != batch_shader) {
                if (!flush()) return false;
                batch_texture = texture; batch_material = material; batch_shader = shader;
                if (task.pass) {
                    if (!task.pass->Prepare_Render_Material(batch_state)) return failure(mesh, "custom or unsupported additional material pass");
                } else if (!shader.Get_Render_Backend_State(batch_state)) {
                    return failure(mesh, "unsupported W3D material shader behavior");
                }
                if (force_multiply && batch_state.destination_blend == RenderBackendBlendFactor::Zero) {
                    batch_state.source_blend = RenderBackendBlendFactor::DestinationColor;
                    batch_state.destination_blend = RenderBackendBlendFactor::SourceColor;
                }
                if (!task.pass && task.alpha != 1.0f && !mesh->Is_Additive()) {
                    batch_state.source_blend = RenderBackendBlendFactor::SourceAlpha;
                    batch_state.destination_blend = RenderBackendBlendFactor::InverseSourceAlpha;
                    if (batch_state.alpha_test != RenderBackendAlphaTest::Disabled) batch_state.alpha_reference = 96.0f * task.alpha / 255.0f;
                }
                if (material->Get_Flag(VertexMaterialClass::DEPTH_CUE) ||
                    material->Get_Flag(VertexMaterialClass::DEPTH_CUE_TO_ALPHA) ||
                    material->Get_Flag(VertexMaterialClass::COPY_SPECULAR_TO_DIFFUSE))
                    return failure(mesh, "vertex-material depth cue/specular copy is not migrated");
                mapping = TextureMapperRenderMapping();
                if (auto *mapper = material->Peek_Mapper()) {
                    bool mapped;
                    if (!task.pass && task.has_uv_override && mapper->Mapper_ID() == TextureMapperClass::MAPPER_ID_LINEAR_OFFSET) {
                        auto *linear = static_cast<LinearOffsetTextureMapperClass *>(mapper);
                        Vector2 original_offset;
                        linear->Get_Current_UV_Offset(original_offset);
                        const unsigned int original_time = linear->Get_LastUsedSyncTime();
                        linear->Set_LastUsedSyncTime(WW3D::Get_Sync_Time());
                        linear->Set_Current_UV_Offset(task.uv_override);
                        mapped = mapper->Get_Render_Mapping(mapping, *camera);
                        linear->Set_Current_UV_Offset(original_offset);
                        linear->Set_LastUsedSyncTime(original_time);
                    } else mapped = mapper->Get_Render_Mapping(mapping, *camera);
                    if (!mapped) return failure(mesh, "unsupported texture-coordinate mapper");
                }
                // Projective q is a separate interpolated coordinate. Never divide
                // u/v at vertices, which changes perspective interpolation.
                texture_handle = RenderBackendTextureHandle();
                if (shader.Get_Texturing() == ShaderClass::TEXTURING_ENABLE && texture) {
                    const auto &filter = texture->Get_Filter();
                    if (!filter.Get_Render_Sampler(batch_state.sampler))
                        return failure(mesh, "unsupported texture sampler");
                    batch_state.clamp_texture = false;
                    if (!texture->Ensure_Renderer_Texture()) return failure(mesh, "texture upload failed");
                    texture_handle = texture->Get_Renderer_Texture();
                }
                have_batch = true;
            }
            if (vertices.size() + 3 > 65535 && !flush()) return false;
            const int uv_source = material->Get_UV_Source(0);
            const Vector2 *uv = uv_source >= 0 && uv_source < MeshMatDescClass::MAX_UV_ARRAYS
                ? model->Get_UV_Array_By_Index(uv_source) : nullptr;
            if (!uv && mapping.input == TextureMapperRenderMapping::Input::UV && texture_handle.Is_Valid())
                return failure(mesh, "missing texture-coordinate array");
            const unsigned int *dcg = model->Get_DCG_Array(task.pass ? 0 : pass_index);
            const unsigned int *dig = model->Get_DIG_Array(task.pass ? 0 : pass_index);
            for (int corner = 0; corner < 3; ++corner) {
                const int index = triangle[corner];
                Vector3 position;
                Matrix3D::Transform_Vector(world, positions[index], &position);
                Vector3 normal = inverse_world.Inverse_Rotate_Vector(normals[index]);
                normal.Normalize();
                const unsigned int diffuse_color = dcg ? dcg[index] : 0xffffffff;
                const unsigned int specular_color = dig ? dig[index] : 0xffffffff;
                Vector3 color;
                float alpha;
                if (!shade(task, material, position, normal, diffuse_color, specular_color, color, alpha)) return false;
                Vector3 camera_position, camera_normal;
                Matrix3D::Transform_Vector(view, position, &camera_position);
                camera_normal = view.Rotate_Vector(normal);
                Vector3 coordinate = mapping.Map_Coordinate(uv ? uv[index] : Vector2(0, 0), camera_position, camera_normal);
                RenderBackendTexturedVertex vertex = { position.X, position.Y, position.Z,
                    saturate(color.X), saturate(color.Y), saturate(color.Z), saturate(alpha), coordinate.X, coordinate.Y };
                vertex.q = coordinate.Z;
                indices.push_back(static_cast<unsigned short>(vertices.size()));
                vertices.push_back(vertex);
            }
        }
        return flush();
    }

    template<class Triangle>
    bool drawDecalRun(MeshClass *mesh, const Matrix3D &world,
        const Vector3 *positions, const Vector3 *normals, const Vector2 *uv,
        unsigned int vertex_count, const Triangle *polygons, unsigned int polygon_count,
        TextureClass *texture, VertexMaterialClass *material, ShaderClass shader)
    {
        auto *backend = WW3D::Get_Render_Backend();
        if (!mesh || !camera || !backend || !material || !positions || !normals || !uv || !polygons)
            return mesh ? failure(mesh, "incomplete mesh-decal submission") : false;
        if (!polygon_count) return true;
        Task task(mesh, nullptr, false, 1.0f, 1.0f);
        RenderBackendMaterialState state;
        if (!shader.Get_Render_Backend_State(state)) return failure(mesh, "unsupported mesh-decal shader");
        if (material->Get_Flag(VertexMaterialClass::DEPTH_CUE) ||
            material->Get_Flag(VertexMaterialClass::DEPTH_CUE_TO_ALPHA) ||
            material->Get_Flag(VertexMaterialClass::COPY_SPECULAR_TO_DIFFUSE))
            return failure(mesh, "unsupported mesh-decal vertex-material effect");
        TextureMapperRenderMapping mapping;
        if (auto *mapper = material->Peek_Mapper())
            if (!mapper->Get_Render_Mapping(mapping, *camera)) return failure(mesh, "unsupported mesh-decal mapper");
        RenderBackendTextureHandle handle;
        if (shader.Uses_Texture() && texture) {
            const auto &filter = texture->Get_Filter();
            if (filter.Get_U_Addr_Mode() != filter.Get_V_Addr_Mode())
                return failure(mesh, "independent mesh-decal texture address modes");
            if (filter.Get_Min_Filter() == TextureFilterClass::FILTER_TYPE_FAST ||
                filter.Get_Mag_Filter() == TextureFilterClass::FILTER_TYPE_FAST)
                return failure(mesh, "point-filtered mesh-decal sampling");
            state.clamp_texture = filter.Get_U_Addr_Mode() == TextureFilterClass::TEXTURE_ADDRESS_CLAMP;
            if (!texture->Ensure_Renderer_Texture()) return failure(mesh, "mesh-decal texture upload failed");
            handle = texture->Get_Renderer_Texture();
        }
        Matrix3D inverse, view;
        world.Get_Inverse(inverse);
        camera->Get_Transform().Get_Orthogonal_Inverse(view);
        std::vector<RenderBackendTexturedVertex> vertices;
        std::vector<unsigned short> indices;
        const auto flush = [&]() {
            if (vertices.empty()) return true;
            if (!backend->Draw_Indexed_Material_Triangles(vertices.data(),
                    static_cast<unsigned int>(vertices.size()), indices.data(),
                    static_cast<unsigned int>(indices.size()), handle, state))
                return failure(mesh, "D3D12 mesh-decal submission rejected");
            ++draws;
            vertices.clear(); indices.clear();
            return true;
        };
        for (unsigned int polygon = 0; polygon < polygon_count; ++polygon) {
            if (vertices.size() + 3 > 65535 && !flush()) return false;
            for (int corner = 0; corner < 3; ++corner) {
                const unsigned int index = polygons[polygon][corner];
                if (index >= vertex_count) return failure(mesh, "invalid mesh-decal vertex index");
                Vector3 position;
                Matrix3D::Transform_Vector(world, positions[index], &position);
                Vector3 normal = inverse.Inverse_Rotate_Vector(normals[index]);
                normal.Normalize();
                Vector3 color;
                float alpha;
                if (!shade(task, material, position, normal, 0xffffffff, 0xff000000, color, alpha)) return false;
                Vector3 camera_position;
                Matrix3D::Transform_Vector(view, position, &camera_position);
                const Vector3 coordinate = mapping.Map_Coordinate(uv[index], camera_position, view.Rotate_Vector(normal));
                RenderBackendTexturedVertex vertex = {position.X, position.Y, position.Z,
                    saturate(color.X), saturate(color.Y), saturate(color.Z), saturate(alpha), coordinate.X, coordinate.Y};
                vertex.q = coordinate.Z;
                indices.push_back(static_cast<unsigned short>(vertices.size()));
                vertices.push_back(vertex);
            }
        }
        return flush();
    }
};

MeshRendererClass::MeshRendererClass() : implementation(new Impl) {}
MeshRendererClass::~MeshRendererClass() { Shutdown(); delete implementation; }
void MeshRendererClass::Init() { implementation->clear(); implementation->draws = implementation->failed = 0; }
void MeshRendererClass::Shutdown() { Invalidate(true); Set_Camera(nullptr); }
void MeshRendererClass::Set_Camera(CameraClass *camera)
{
    if (camera) camera->Add_Ref();
    if (implementation->camera) implementation->camera->Release_Ref();
    implementation->camera = camera;
}
CameraClass *MeshRendererClass::Peek_Camera() const { return implementation->camera; }
void MeshRendererClass::Enable_Lighting(bool enabled) { implementation->lighting_enabled = enabled; }
bool MeshRendererClass::Shade_Vertex(VertexMaterialClass *material, const LightEnvironmentClass *lighting,
    const Vector3 &position, const Vector3 &normal, unsigned int diffuse_color, unsigned int specular_color,
    float alpha_override, float emissive_scale, bool additive, Vector3 &color, float &alpha) const
{
    return shadeVertex(material, lighting, implementation->lighting_enabled,
        position, normal, diffuse_color, specular_color, alpha_override, emissive_scale, additive, color, alpha);
}

void MeshRendererClass::Set_Force_Multiply(bool enabled) { implementation->force_multiply = enabled; }
void MeshRendererClass::Register_Mesh_Type(MeshModelClass *model)
{
    if (!model) return;
    implementation->models.insert(model);
    model->RendererRegistered = true;
}
void MeshRendererClass::Unregister_Mesh_Type(MeshModelClass *model)
{
    if (!model) return;
    implementation->models.erase(model);
    model->RendererRegistered = false;
}
void MeshRendererClass::Invalidate(bool shutdown)
{
    implementation->clear();
    for (auto *model : implementation->models) model->RendererRegistered = false;
    implementation->models.clear();
    if (shutdown) implementation->draws = implementation->failed = 0;
}
void MeshRendererClass::Queue_Base_Passes(MeshClass *mesh)
{
    if (mesh) implementation->tasks.emplace_back(new Impl::Task(mesh, nullptr, false, mesh->m_alphaOverride, 1.0f));
}
void MeshRendererClass::Queue_Material_Pass(MeshClass *mesh, MaterialPassClass *pass, bool delayed)
{
    if (mesh && pass) implementation->tasks.emplace_back(new Impl::Task(mesh, pass, delayed, mesh->m_materialPassAlphaOverride, mesh->m_materialPassEmissiveOverride));
}
void MeshRendererClass::Add_To_Render_List(DecalMeshClass *mesh)
{
    if (mesh) { mesh->Add_Ref(); implementation->decals.push_back(mesh); }
}
bool MeshRendererClass::Draw_Decal_Run(MeshClass *mesh, const Matrix3D &world,
    const Vector3 *positions, const Vector3 *normals, const Vector2 *uv,
    unsigned int vertex_count, const Vector3i16 *polygons, unsigned int polygon_count,
    TextureClass *texture, VertexMaterialClass *material, ShaderClass shader)
{
    return implementation->drawDecalRun(mesh, world, positions, normals, uv,
        vertex_count, polygons, polygon_count, texture, material, shader);
}
bool MeshRendererClass::Draw_Decal_Run(MeshClass *mesh, const Matrix3D &world,
    const Vector3 *positions, const Vector3 *normals, const Vector2 *uv,
    unsigned int vertex_count, const Vector3i *polygons, unsigned int polygon_count,
    TextureClass *texture, VertexMaterialClass *material, ShaderClass shader)
{
    return implementation->drawDecalRun(mesh, world, positions, normals, uv,
        vertex_count, polygons, polygon_count, texture, material, shader);
}
bool MeshRendererClass::Flush()
{
    if (!implementation->camera) return implementation->tasks.empty() && implementation->decals.empty();
    implementation->camera->Apply();
    bool success = true;
    for (int skin = 0; skin < 2; ++skin) {
        // Complete base passes before procedural passes, and rigid before skin.
        for (int pass = 0; pass < MeshMatDescClass::MAX_PASSES; ++pass)
            for (auto &task : implementation->tasks)
                if (!task->pass && !!task->mesh->Peek_Model()->Get_Flag(MeshGeometryClass::SKIN) == !!skin && pass < task->mesh->Peek_Model()->Get_Pass_Count())
                    success = implementation->draw(*task, pass) && success;
        for (auto &task : implementation->tasks)
            if (task->pass && !task->delayed && !!task->mesh->Peek_Model()->Get_Flag(MeshGeometryClass::SKIN) == !!skin)
                success = implementation->draw(*task, 0) && success;
    }
    for (auto *decal : implementation->decals) {
        success = decal->Render() && success;
    }
    for (auto &task : implementation->tasks)
        if (task->delayed) success = implementation->draw(*task, 0) && success;
    if (!success) ++implementation->failed;
    implementation->clear();
    return success;
}
void MeshRendererClass::Request_Log_Statistics() { Log_Statistics_String(true); }
void MeshRendererClass::Log_Statistics_String(bool)
{
    WWDEBUG_SAY(("MeshRenderer: %u draw calls, %u failed flushes, %u registered CPU models\n",
        implementation->draws, implementation->failed, static_cast<unsigned int>(implementation->models.size())));
}
