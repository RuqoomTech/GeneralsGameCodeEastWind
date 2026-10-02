/* Command & Conquer Generals Zero Hour(tm), Copyright 2025 Electronic Arts Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

class CameraClass;
class MeshClass;
class MeshModelClass;
class MaterialPassClass;
class DecalMeshClass;
class Matrix3D;
class Vector2;
class Vector3;
class Vector3i;
class Vector3i16;
class TextureClass;
class VertexMaterialClass;
class ShaderClass;

// Owns renderer-only mesh submissions. Asset geometry and materials stay on the
// CPU; native buffers and draw state are owned by IRenderBackend.
class MeshRendererClass
{
public:
    MeshRendererClass();
    ~MeshRendererClass();
    void Init();
    void Shutdown();
    bool Flush();
    void Register_Mesh_Type(MeshModelClass *model);
    void Unregister_Mesh_Type(MeshModelClass *model);
    void Invalidate(bool shutdown = false);
    void Set_Camera(CameraClass *camera);
    CameraClass *Peek_Camera() const;
    void Enable_Lighting(bool enabled);
    void Set_Force_Multiply(bool enabled);
    void Queue_Base_Passes(MeshClass *mesh);
    void Queue_Material_Pass(MeshClass *mesh, MaterialPassClass *pass, bool delayed);
    void Add_To_Render_List(DecalMeshClass *mesh);
    bool Draw_Decal_Run(MeshClass *mesh, const Matrix3D &world,
        const Vector3 *positions, const Vector3 *normals, const Vector2 *uv,
        unsigned int vertex_count, const Vector3i16 *polygons, unsigned int polygon_count,
        TextureClass *texture, VertexMaterialClass *material, ShaderClass shader);
    bool Draw_Decal_Run(MeshClass *mesh, const Matrix3D &world,
        const Vector3 *positions, const Vector3 *normals, const Vector2 *uv,
        unsigned int vertex_count, const Vector3i *polygons, unsigned int polygon_count,
        TextureClass *texture, VertexMaterialClass *material, ShaderClass shader);
    void Request_Log_Statistics();
    void Log_Statistics_String(bool only_visible);

private:
    MeshRendererClass(const MeshRendererClass &) = delete;
    MeshRendererClass &operator=(const MeshRendererClass &) = delete;
    struct Impl;
    Impl *implementation;
};

extern MeshRendererClass TheMeshRenderer;
