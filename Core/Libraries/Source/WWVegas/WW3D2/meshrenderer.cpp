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
#include "WWMath/obbox.h"
#include "WWDebug/wwdebug.h"
#include <algorithm>
#include <cmath>
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
// Renderer diagnostic state is never part of asset CRC or deterministic state.
bool failure(const MeshClass *mesh, const char *reason)
{
    WWDEBUG_SAY(("MeshRenderer: cannot draw %s: %s\n", mesh->Get_Name(), reason));
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

    bool draw(Task &task, int pass_index)
    {
        auto *mesh = task.mesh;
        auto *model = mesh->Peek_Model();
        auto *backend = WW3D::Get_Render_Backend();
        if (!model || !camera || !backend) return failure(mesh, "missing model, camera, or backend");
        if (MeshRendererDebugger::Is_Enabled() && mesh->Is_Disabled_By_Debugger()) return true;
        if (model->Get_Flag(MeshGeometryClass::SORT) && WW3D::Is_Sorting_Enabled())
            return failure(mesh, "global transparent triangle sorting is not migrated");
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
            if (!backend->Draw_Indexed_Material_Triangles(vertices.data(),
                    static_cast<unsigned int>(vertices.size()), indices.data(),
                    static_cast<unsigned int>(indices.size()), texture_handle, batch_state))
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
                if (force_multiply && batch_state.destination_blend == RenderBackendBlendFactor::Zero)
                    return failure(mesh, "forced multiply requires destination-color blend support");
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
                    if (filter.Get_U_Addr_Mode() != filter.Get_V_Addr_Mode())
                        return failure(mesh, "independent U/V address modes are not migrated");
                    if (filter.Get_Min_Filter() == TextureFilterClass::FILTER_TYPE_FAST ||
                        filter.Get_Mag_Filter() == TextureFilterClass::FILTER_TYPE_FAST)
                        return failure(mesh, "point-filtered material sampling is not migrated");
                    batch_state.clamp_texture = filter.Get_U_Addr_Mode() == TextureFilterClass::TEXTURE_ADDRESS_CLAMP;
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
                Vector3 color = rgb(diffuse_color);
                float alpha = ((diffuse_color >> 24) & 255) / 255.0f;
                if (material->Get_Lighting() && lighting_enabled) {
                    if (!task.has_lighting) return failure(mesh, "global light state without a mesh light environment is not migrated");
                    Vector3 ambient, diffuse, emissive;
                    material->Get_Ambient(&ambient); material->Get_Diffuse(&diffuse); material->Get_Emissive(&emissive);
                    ambient = materialColor(material->Get_Ambient_Color_Source(), ambient, diffuse_color, specular_color);
                    diffuse = materialColor(material->Get_Diffuse_Color_Source(), diffuse, diffuse_color, specular_color);
                    emissive = materialColor(material->Get_Emissive_Color_Source(), emissive, diffuse_color, specular_color);
                    if (!task.pass && task.alpha != 1.0f && mesh->Is_Additive()) diffuse.Set(task.alpha, task.alpha, task.alpha);
                    color = multiply(ambient, task.lighting.Get_Equivalent_Ambient()) + emissive * task.emissive;
                    for (int light = 0; light < task.lighting.Get_Light_Count(); ++light) {
                        Vector3 direction = task.lighting.Get_Light_Direction(light);
                        Vector3 illumination = task.lighting.Get_Light_Diffuse(light);
                        float attenuation = 1.0f;
                        if (task.lighting.isPointLight(light)) {
                            direction = task.lighting.getPointCenter(light) - position;
                            const float distance = direction.Length();
                            const float outer = task.lighting.getPointOrad(light);
                            const float inner = task.lighting.getPointIrad(light);
                            if (distance > outer || outer <= 0.0f) continue;
                            const float linear = std::fabs(inner - outer) < 1.0e-5f ? 0.0f : 0.1f / inner;
                            attenuation = 1.0f / (1.0f + linear * distance + 8.0f * distance * distance / (outer * outer));
                            direction.Normalize();
                            illumination = task.lighting.getPointDiffuse(light);
                            color += multiply(ambient, task.lighting.getPointAmbient(light)) * attenuation;
                        }
                        color += multiply(diffuse, illumination) * (std::max(0.0f, Vector3::Dot_Product(normal, direction)) * attenuation);
                    }
                    alpha = material->Get_Diffuse_Color_Source() == VertexMaterialClass::COLOR1 ? alpha : material->Get_Opacity();
                }
                if (task.alpha != 1.0f) alpha = task.alpha;
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
    // Decal geometry is a separate real caller whose renderer is still pending.
    for (auto *decal : implementation->decals) {
        success = failure(decal->Peek_Parent(), "mesh decal material draw is not migrated") && success;
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
