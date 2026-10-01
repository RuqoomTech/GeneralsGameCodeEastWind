/* Command & Conquer Generals Zero Hour(tm), Copyright 2025 Electronic Arts Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

class CameraClass;
class MeshClass;
class MeshModelClass;
class MaterialPassClass;
class DecalMeshClass;

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
    void Request_Log_Statistics();
    void Log_Statistics_String(bool only_visible);

private:
    MeshRendererClass(const MeshRendererClass &) = delete;
    MeshRendererClass &operator=(const MeshRendererClass &) = delete;
    struct Impl;
    Impl *implementation;
};

extern MeshRendererClass TheMeshRenderer;
