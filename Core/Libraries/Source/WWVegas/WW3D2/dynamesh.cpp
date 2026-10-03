/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/***************************************************************************
 ***    C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S     ***
 ***************************************************************************
 *                                                                         *
 *                 Project Name : Commando                                 *
 *                                                                         *
 *                     $Archive:: /Commando/Code/ww3d2/dynamesh.cpp       $*
 *                                                                         *
 *                      $Author:: Greg_h                                  $*
 *                                                                         *
 *                     $Modtime:: 12/03/01 4:50p                          $*
 *                                                                         *
 *                    $Revision:: 25                                      $*
 *                                                                         *
 *-------------------------------------------------------------------------*
 * Functions:                                                              *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "dynamesh.h"
#include "sortingrenderer.h"
#include "meshrenderer.h"
#include "mapper.h"
#include "texture.h"
#include <algorithm>
#include <cstdio>
#include <vector>
#include "rinfo.h"
#include "camera.h"



/*
** DynamicMeshModel implementation
*/

DynamicMeshModel::DynamicMeshModel(unsigned int max_polys, unsigned int max_verts) :
	MeshGeometryClass(),
	DynamicMeshPNum(0),
	DynamicMeshVNum(0),
	MatDesc(nullptr),
	MatInfo(nullptr)
{
	MatInfo = NEW_REF(MaterialInfoClass, ());

	MatDesc =  W3DNEW MeshMatDescClass;
	MatDesc->Set_Polygon_Count(max_polys);
	MatDesc->Set_Vertex_Count(max_verts);

	Reset_Geometry(max_polys, max_verts);
}

DynamicMeshModel::DynamicMeshModel(unsigned int max_polys, unsigned int max_verts, MaterialInfoClass *mat_info) :
	MeshGeometryClass(),
	DynamicMeshPNum(0),
	DynamicMeshVNum(0),
	MatDesc(nullptr),
	MatInfo(nullptr)
{
	MatInfo = mat_info;
	MatInfo->Add_Ref();

	MatDesc = W3DNEW MeshMatDescClass;
	MatDesc->Set_Polygon_Count(max_polys);
	MatDesc->Set_Vertex_Count(max_verts);

	Reset_Geometry(max_polys, max_verts);
}

DynamicMeshModel::DynamicMeshModel(const DynamicMeshModel &src) :
	MeshGeometryClass(src),
	DynamicMeshPNum(src.DynamicMeshPNum),
	DynamicMeshVNum(src.DynamicMeshVNum),
	MatDesc(nullptr),
	MatInfo(nullptr)
{
	// Copy the material info structure.
	MatInfo = NEW_REF(MaterialInfoClass, (*(src.MatInfo)));


	// [SKB: Feb 21 2002 @ 11:47pm] :
	// Moved before the remapping cause I don't like referencing null.
	MatDesc = W3DNEW MeshMatDescClass;

	// remap!
	MaterialRemapperClass remapper(src.MatInfo, MatInfo);
	remapper.Remap_Mesh(src.MatDesc, MatDesc);
}

DynamicMeshModel::~DynamicMeshModel()
{
	delete MatDesc;
	MatDesc = nullptr;

	REF_PTR_RELEASE(MatInfo);
}

void DynamicMeshModel::Compute_Plane_Equations()
{
	// Make sure the arrays are allocated before we do this
	get_vert_normals();
	Vector4 * planes = get_planes(true);

	// Set the poly and vertex counts to the dynamic counts, call the base class function, then
	// set them back.
	int old_poly_count = PolyCount;
	int old_vert_count = VertexCount;
	PolyCount = DynamicMeshPNum;
	VertexCount = DynamicMeshVNum;

	MeshGeometryClass::Compute_Plane_Equations(planes);

	PolyCount = old_poly_count;
	VertexCount = old_vert_count;
}

void DynamicMeshModel::Compute_Vertex_Normals()
{
	// Make sure the arrays are allocated before we do this
	Vector3 * vnorms = get_vert_normals();
	get_planes(true);

	// Set the poly and vertex counts to the dynamic counts, call the base class function, then
	// set them back.
	int old_poly_count = PolyCount;
	int old_vert_count = VertexCount;
	PolyCount = DynamicMeshPNum;
	VertexCount = DynamicMeshVNum;

	MeshGeometryClass::Compute_Vertex_Normals(vnorms);

	PolyCount = old_poly_count;
	VertexCount = old_vert_count;
}

void DynamicMeshModel::Compute_Bounds(Vector3 * verts)
{
	// Set the poly and vertex counts to the dynamic counts, call the base class function, then
	// set them back.
	int old_poly_count = PolyCount;
	int old_vert_count = VertexCount;
	PolyCount = DynamicMeshPNum;
	VertexCount = DynamicMeshVNum;

	MeshGeometryClass::Compute_Bounds(verts);

	PolyCount = old_poly_count;
	VertexCount = old_vert_count;
}

void DynamicMeshModel::Reset()
{
	Set_Counts(0, 0);
	int polycount = Get_Polygon_Count();
	int vertcount = Get_Vertex_Count();
	Reset_Geometry(polycount, vertcount);
	MatDesc->Reset(polycount, vertcount, 1);
	REF_PTR_RELEASE(MatInfo);
	MatInfo = NEW_REF(MaterialInfoClass, ());
}

void DynamicMeshModel::Render(RenderInfoClass &rinfo, const Matrix3D &world)
{
    if (!DynamicMeshVNum || !DynamicMeshPNum) return;
    const auto *positions = Get_Vertex_Array();
    const auto *normals = Get_Vertex_Normal_Array();
    const auto *triangles = Get_Polygon_Array();
    if (!positions || !normals || !triangles || DynamicMeshVNum < 0 || DynamicMeshPNum < 0) {
        std::fputs("DynamicMesh: invalid CPU geometry\n", stderr); return;
    }
    for (int triangle = 0; triangle < DynamicMeshPNum; ++triangle)
        for (unsigned corner = 0; corner < 3; ++corner)
            if (triangles[triangle][corner] >= static_cast<unsigned>(DynamicMeshVNum)) {
                std::fputs("DynamicMesh: invalid CPU index\n", stderr); return;
            }
    const Matrix3D view = rinfo.Camera.Get_View_Matrix();
    const bool sort = Get_Flag(MeshGeometryClass::SORT) && WW3D::Is_Sorting_Enabled();
    SphereClass bounds;
    Get_Bounding_Sphere(&bounds);
    Matrix3D::Transform_Vector(world, bounds.Center, &bounds.Center);
    for (unsigned pass = 0; pass < Get_Pass_Count(); ++pass) {
        auto *texture_buffer = MatDesc->Get_Texture_Array(pass, 0, false);
        auto *second_buffer = MatDesc->Get_Texture_Array(pass, 1, false);
        auto *material_buffer = MatDesc->Get_Material_Array(pass, false);
        auto *shaders = MatDesc->Get_Shader_Array(pass, false);
        unsigned start = 0;
        while (start < static_cast<unsigned>(DynamicMeshPNum)) {
            auto *texture = texture_buffer ? texture_buffer->Get_Array()[start] : MatDesc->Peek_Single_Texture(pass, 0);
            auto *second = second_buffer ? second_buffer->Get_Array()[start] : MatDesc->Peek_Single_Texture(pass, 1);
            auto *material = material_buffer ? material_buffer->Get_Array()[triangles[start].I] : MatDesc->Peek_Single_Material(pass);
            const ShaderClass shader = shaders ? shaders[start] : MatDesc->Get_Single_Shader(pass);
            RenderBackendMaterialState state;
            if (!material || second || !shader.Get_Render_Backend_State(state) ||
                material->Get_Flag(VertexMaterialClass::DEPTH_CUE) || material->Get_Flag(VertexMaterialClass::DEPTH_CUE_TO_ALPHA) ||
                material->Get_Flag(VertexMaterialClass::COPY_SPECULAR_TO_DIFFUSE)) {
                std::fputs("DynamicMesh: unsupported material state\n", stderr); return;
            }
            TextureMapperRenderMapping mapping;
            if (auto *mapper = material->Peek_Mapper()) {
                if (!mapper->Get_Render_Mapping(mapping, rinfo.Camera)) {
                    std::fputs("DynamicMesh: unsupported texture mapper\n", stderr); return;
                }
            }
            const int uv_source = material->Get_UV_Source(0);
            const Vector2 *uv = uv_source >= 0 && uv_source < MeshMatDescClass::MAX_UV_ARRAYS
                ? MatDesc->Get_UV_Array_By_Index(uv_source, false) : nullptr;
            const unsigned *colors = MatDesc->Get_Color_Array(0, false);
            const unsigned *specular = MatDesc->Get_Color_Array(1, false);
            unsigned end = start + 1;
            while (end < static_cast<unsigned>(DynamicMeshPNum) && (end-start)*3 < 65535 &&
                (!texture_buffer || texture_buffer->Get_Array()[end] == texture) &&
                (!second_buffer || second_buffer->Get_Array()[end] == second) &&
                (!material_buffer || material_buffer->Get_Array()[triangles[end].I] == material) &&
                (!shaders || shaders[end] == shader)) ++end;
            std::vector<RenderBackendTexturedVertex> vertices;
            std::vector<unsigned short> indices;
            for (unsigned triangle = start; triangle < end; ++triangle) {
                for (unsigned corner = 0; corner < 3; ++corner) {
                    const unsigned index = triangles[triangle][corner];
                    if (index >= static_cast<unsigned>(DynamicMeshVNum)) {
                        std::fputs("DynamicMesh: invalid CPU index\n", stderr); return;
                    }
                    Vector3 position;
                    Matrix3D::Transform_Vector(world, positions[index], &position);
                    const Vector3 normal = world.Rotate_Vector(normals[index]);
                    Vector3 color; float alpha;
                    if (!TheMeshRenderer.Shade_Vertex(material, rinfo.light_environment, position, normal,
                        colors ? colors[index] : 0xffffffff, specular ? specular[index] : 0,
                        1.0f, 1.0f, false, color, alpha)) return;
                    Vector3 camera_position;
                    Matrix3D::Transform_Vector(view, position, &camera_position);
                    const Vector3 coordinate = mapping.Map_Coordinate(uv ? uv[index] : Vector2(0,0),
                        camera_position, view.Rotate_Vector(normal));
                    indices.push_back(static_cast<unsigned short>(vertices.size()));
                    vertices.push_back({position.X, position.Y, position.Z, color.X, color.Y, color.Z,
                        alpha, coordinate.X, coordinate.Y, coordinate.Z});
                }
            }
            if (!SortingRendererClass::Submit_CPU_Triangles(vertices.data(), static_cast<unsigned>(vertices.size()),
                indices.data(), static_cast<unsigned>(indices.size()),
                shader.Get_Texturing() == ShaderClass::TEXTURING_ENABLE ? texture : nullptr,
                state, rinfo.Camera, &bounds, sort)) return;
            start = end;
        }
    }
}

void DynamicMeshModel::Initialize_Texture_Array(int pass, int stage, TextureClass *texture)
{
	TexBufferClass * texlist = MatDesc->Get_Texture_Array(pass, 0, true);
	for (int lp = 0; lp < PolyCount; lp++) {
		texlist->Set_Element(lp, texture);
	}
}

void DynamicMeshModel::Initialize_Material_Array(int pass, VertexMaterialClass *vmat)
{
	MatBufferClass * vertmatlist = MatDesc->Get_Material_Array(pass, true);
	for (int lp = 0; lp < VertexCount; lp++) {
		vertmatlist->Set_Element(lp, vmat);
	}
}

void DynamicMeshClass::Render(RenderInfoClass & rinfo)
{
	if (Is_Not_Hidden_At_All() == false)	return;

	// test for an empty mesh..
	if (PolyCount == 0 ) return;

	// If static sort lists are enabled and this mesh has a sort level, put it on the list instead
	// of rendering it.

	if (WW3D::Are_Static_Sort_Lists_Enabled() && SortLevel != SORT_LEVEL_NONE) {

		WW3D::Add_To_Static_Sort_List(this, SortLevel);

	} else {

		const FrustumClass & frustum = rinfo.Camera.Get_Frustum();

		if (CollisionMath::Overlap_Test(frustum, Get_Bounding_Box()) != CollisionMath::OUTSIDE) {
			Model->Render(rinfo, Transform);
		}
	}
}

bool DynamicMeshClass::End_Vertex()
{
	// check that we have room for a new vertex
	WWASSERT(VertCount < Model->Get_Vertex_Count());

	// if we are a multi-material object record the material
	int pass = Get_Pass_Count();
	while (pass--) {
		if (MultiVertexMaterial[pass]) {
			VertexMaterialClass *mat = Peek_Material_Info()->Get_Vertex_Material(VertexMaterialIdx[pass]);
			Model->Set_Material(VertCount, mat, pass);
			REF_PTR_RELEASE(mat);
		}

	}

	// if we are multi colored, record the color
	for (int color_array_index = 0; color_array_index < MAX_COLOR_ARRAYS; color_array_index++) {
		if (MultiVertexColor[color_array_index]) {
//			Vector4 * color = &((Model->Get_Color_Array(color_array_index))[VertCount]);
//			color->X = CurVertexColor[color_array_index].X;
//			color->Y = CurVertexColor[color_array_index].Y;
//			color->Z = CurVertexColor[color_array_index].Z;
//			color->W = CurVertexColor[color_array_index].W;
			unsigned * color = &((Model->Get_Color_Array(color_array_index))[VertCount]);
			*color = Pack_Vertex_Color(CurVertexColor[color_array_index]);
		}
	}

	// mark this vertex as being complete
	VertCount++;
	TriVertexCount++;

	// if we have 3 or more vertices, add a new poly
	if (TriVertexCount >= 3) {

		// check that we have room for a new poly
		WWASSERT(PolyCount < Model->Get_Polygon_Count());

		// set vertex indices
		TriIndex *poly = &(Model->Get_Non_Const_Polygon_Array())[PolyCount];
		if (TriMode == TRI_MODE_STRIPS) {
			(*poly)[0] = VertCount-3;
			(*poly)[1] = VertCount-2;
			(*poly)[2] = VertCount-1;

			// for every other tri, reverse vertex order
			if (Flip_Face()) {
				(*poly)[1] = VertCount-1;
				(*poly)[2] = VertCount-2;
			}
		} else {
			(*poly)[0] = FanVertex;
			(*poly)[1] = VertCount-2;
			(*poly)[2] = VertCount-1;
		}

		// check each pass
		int pass = Get_Pass_Count();
		while (pass--) {

			// If we are multi texture
			if (MultiTexture[pass]) {
				TextureClass *tex = Peek_Material_Info()->Get_Texture(TextureIdx[pass]);
				Model->Set_Texture(PolyCount, tex, pass);
				REF_PTR_RELEASE(tex);
			}
		}

		// increase the count and record that we have a new material
		PolyCount++;
		Model->Set_Counts(PolyCount, VertCount);
	}
	return true;
}

unsigned DynamicMeshClass::Pack_Vertex_Color(const Vector4 &color)
{
    const auto channel = [](float component) {
        return static_cast<unsigned>(std::max(0.0f, std::min(1.0f, component)) * 255.0f);
    };
    return (channel(color.W) << 24) | (channel(color.X) << 16) |
        (channel(color.Y) << 8) | channel(color.Z);
}

/******************************************************************
**
** DynamicMeshClass
**
*******************************************************************/
DynamicMeshClass::DynamicMeshClass(int max_poly, int max_vert) :
	Model(nullptr),
	PolyCount(0),
	VertCount(0),
	TriVertexCount(0),
	FanVertex(0),
	TriMode(TRI_MODE_STRIPS),
	SortLevel(SORT_LEVEL_NONE)
{
	int pass = MAX_PASSES;
	while (pass--) {
		MultiTexture[pass] = false;
		TextureIdx[pass] = -1;

		MultiVertexMaterial[pass] = false;
		VertexMaterialIdx[pass] = -1;
	}

	for (int color_array_index = 0; color_array_index < MAX_COLOR_ARRAYS; color_array_index++) {
		MultiVertexColor[color_array_index] = false;
		CurVertexColor[color_array_index].Set(1.0f, 1.0f, 1.0f, 1.0f);
	}

	Model = NEW_REF(DynamicMeshModel, (max_poly, max_vert));
}

DynamicMeshClass::DynamicMeshClass(int max_poly, int max_vert, MaterialInfoClass *mat_info) :
	Model(nullptr),
	PolyCount(0),
	VertCount(0),
	TriVertexCount(0),
	FanVertex(0),
	TriMode(TRI_MODE_STRIPS),
	SortLevel(SORT_LEVEL_NONE)
{
	int pass = MAX_PASSES;
	while (pass--) {
		MultiTexture[pass] = false;
		TextureIdx[pass] = -1;

		MultiVertexMaterial[pass] = false;
		VertexMaterialIdx[pass] = -1;
	}

	for (int color_array_index = 0; color_array_index < MAX_COLOR_ARRAYS; color_array_index++) {
		MultiVertexColor[color_array_index] = false;
		CurVertexColor[color_array_index].Set(1.0f, 1.0f, 1.0f, 1.0f);
	}

	Model = NEW_REF(DynamicMeshModel, (max_poly, max_vert, mat_info));
}

DynamicMeshClass::DynamicMeshClass(const DynamicMeshClass & src) :
	RenderObjClass(src),
	Model(nullptr),
	PolyCount(src.PolyCount),
	VertCount(src.VertCount),
	TriVertexCount(src.TriVertexCount),
	FanVertex(src.FanVertex),
	TriMode(src.TriMode),
	SortLevel(src.SortLevel)
{
	int pass = MAX_PASSES;
	while (pass--) {
		MultiTexture[pass] = src.MultiTexture[pass];
		TextureIdx[pass] = src.TextureIdx[pass];

		MultiVertexMaterial[pass] = src.MultiVertexMaterial[pass];
		VertexMaterialIdx[pass] = src.VertexMaterialIdx[pass];
	}

	for (int color_array_index = 0; color_array_index < MAX_COLOR_ARRAYS; color_array_index++) {
		MultiVertexColor[color_array_index] = src.MultiVertexColor[color_array_index];
		CurVertexColor[color_array_index] = src.CurVertexColor[color_array_index];
	}

	Model = NEW_REF(DynamicMeshModel, (*(src.Model)));
}

void DynamicMeshClass::Resize(int max_polys, int max_verts)
{
	Reset();

	REF_PTR_RELEASE(Model);
	Model = NEW_REF(DynamicMeshModel, (max_polys, max_verts));

	// reset all the texture & vertex material indices
	int pass = MAX_PASSES;
	while (pass--) {
		TextureIdx[pass] = -1;
		VertexMaterialIdx[pass] = -1;
		MultiVertexMaterial[pass] = false;
	}
}

DynamicMeshClass::~DynamicMeshClass()
{
	REF_PTR_RELEASE(Model);
}

RenderObjClass * DynamicMeshClass::Clone() const
{
	return NEW_REF(DynamicMeshClass, (*this));
}

void DynamicMeshClass::Location(float x, float y, float z)
{
	Vector3 * loc = Model->Get_Vertex_Array();
	assert(loc);

	loc[VertCount].X = x;
	loc[VertCount].Y = y;
	loc[VertCount].Z = z;
}

/*
** For moving a vertex after the DynaMesh has already been created.
*/
void DynamicMeshClass::Move_Vertex(int index, float x, float y, float z)
{
	Vector3 * loc = Model->Get_Vertex_Array();
	assert(loc);
	loc[index][0] = x;
	loc[index][1] = y;
	loc[index][2] = z;
}

/*
** Get a vertex value.
*/
void DynamicMeshClass::Get_Vertex(int index, float &x, float &y, float &z)
{
	Vector3 * loc = Model->Get_Vertex_Array();
	assert(loc);
	x = loc[index][0];
	y = loc[index][1];
	z = loc[index][2];
}


/*
** Offset the entire mesh
*/
void DynamicMeshClass::Translate_Vertices(const Vector3 & offset)
{
	Vector3 * loc = Model->Get_Vertex_Array();
	assert(loc);
	for (int i=0; i < Get_Num_Vertices(); i++) {
		loc[i].X += offset.X;
		loc[i].Y += offset.Y;
		loc[i].Z += offset.Z;
	}

	Set_Dirty_Bounds();
	Set_Dirty_Planes();
}

int DynamicMeshClass::Set_Vertex_Material(int idx, int pass)
{
	assert(idx < Peek_Material_Info()->Vertex_Material_Count());
	VertexMaterialIdx[pass] = idx;
	if (!MultiVertexMaterial[pass]) {
		// WWASSERT( VertexMaterialIdx[pass] == 0);
		VertexMaterialClass *mat = Peek_Material_Info()->Get_Vertex_Material(VertexMaterialIdx[pass]);
		Model->Set_Single_Material(mat, pass);
		mat->Release_Ref();
	}
	return VertexMaterialIdx[pass];
}

int DynamicMeshClass::Set_Vertex_Material(VertexMaterialClass *material, bool dont_search, int pass)
{
	// Check if same as the last vertex material
	if (Peek_Material_Info()->Vertex_Material_Count() && (VertexMaterialIdx[pass] != -1) && Peek_Material_Info()->Peek_Vertex_Material(VertexMaterialIdx[pass]) == material) {
		return VertexMaterialIdx[pass];
	}

	// if there are vertex materials in the list then we may have just jumped
	// to becoming a multi-vertex-material object.  Take care of that here.
	if ((!MultiVertexMaterial[pass]) && Peek_Material_Info()->Vertex_Material_Count() && (VertexMaterialIdx[pass] != -1) && Peek_Material_Info()->Peek_Vertex_Material(VertexMaterialIdx[pass]) != material) {

		// allocate the array of per-vertex vertex material overrides
		VertexMaterialClass *mat = Peek_Material_Info()->Get_Vertex_Material(VertexMaterialIdx[pass]);
		Model->Initialize_Material_Array(pass, mat);
		mat->Release_Ref();

		// flag that we need to write the per -vertex vertex material override array
		MultiVertexMaterial[pass] = true;
	}

	// add the material to the material info class if we cant find it in the
	// list.  if we are not supposed to search the list for it then just add
	// it.
	if (!dont_search) {
		int lp = 0, found = 0;
		for (; lp < Peek_Material_Info()->Vertex_Material_Count(); lp ++) {
			VertexMaterialClass *mat = Peek_Material_Info()->Get_Vertex_Material(lp);
			if (material == mat) {
				VertexMaterialIdx[pass] = lp;
				found = 1;
				mat->Release_Ref();
				break;
			}
			mat->Release_Ref();
		}
		if (!found) {
			Peek_Material_Info()->Add_Vertex_Material(material);
			VertexMaterialIdx[pass] = Peek_Material_Info()->Vertex_Material_Count() - 1;
		}
	} else {
		Peek_Material_Info()->Add_Vertex_Material(material);
		VertexMaterialIdx[pass] = Peek_Material_Info()->Vertex_Material_Count() - 1;
	}

	if (!MultiVertexMaterial[pass]) {
		Model->Set_Single_Material(Peek_Material_Info()->Peek_Vertex_Material(VertexMaterialIdx[pass]), pass);
	}
	return(VertexMaterialIdx[pass]);
}

int DynamicMeshClass::Set_Texture(int idx, int pass)
{
	WWASSERT(idx < Peek_Material_Info()->Texture_Count());
	TextureIdx[pass] = idx;
	if (!MultiTexture[pass]) {
		TextureClass *tex = Peek_Material_Info()->Get_Texture(TextureIdx[pass]);
		Model->Set_Single_Texture(tex, pass);
		tex->Release_Ref();
	}
	return TextureIdx[pass];
}

int DynamicMeshClass::Set_Texture(TextureClass *texture, bool dont_search, int pass)
{
	// Check if same as the last texture
	if (Peek_Material_Info()->Texture_Count() && (TextureIdx[pass] != -1) && Peek_Material_Info()->Peek_Texture(TextureIdx[pass]) == texture) {
		return TextureIdx[pass];
	}

	// if there are textures in the list then we may have just jumped
	// to becoming a multi-texture object.  Take care of that here.
	if ((!MultiTexture[pass]) && Peek_Material_Info()->Texture_Count() && (TextureIdx[pass] != -1) && Peek_Material_Info()->Peek_Texture(TextureIdx[pass]) != texture) {

		// allocate the array of per polygon material over-rides
		TextureClass *tex = Peek_Material_Info()->Get_Texture(TextureIdx[pass]);
		Model->Initialize_Texture_Array(pass, 0, tex);
		tex->Release_Ref();

		// flag that we need to write the per polygon material override array
		MultiTexture[pass] = true;
	}

	// add the material to the material info class if we cant find it in the
	// list.  if we are not supposed to search the list for it then just add
	// it.
	if (!dont_search) {
		int lp = 0, found = 0;
		for (; lp < Peek_Material_Info()->Texture_Count(); lp ++) {
			TextureClass *tex = Peek_Material_Info()->Get_Texture(lp);
			if (texture == tex) {
				TextureIdx[pass] = lp;
				found = 1;
				tex->Release_Ref();
				break;
			}
			tex->Release_Ref();
		}
		if (!found) {
			Peek_Material_Info()->Add_Texture(texture);
			TextureIdx[pass] = Peek_Material_Info()->Texture_Count() - 1;
		}
	} else {
		Peek_Material_Info()->Add_Texture(texture);
		TextureIdx[pass] = Peek_Material_Info()->Texture_Count() - 1;
	}

	if (!MultiTexture[pass]) {
		TextureClass *tex = Peek_Material_Info()->Get_Texture(TextureIdx[pass]);
		Model->Set_Single_Texture(tex, pass);
		tex->Release_Ref();
	}
	return(TextureIdx[pass]);
}

/*
**
*/
// Remap locations to match a screen
void DynamicScreenMeshClass::Location( float x, float y, float z)
{
	DynamicMeshClass::Location( (x * 2) - 1, Aspect - (y * 2 * Aspect), 0);
}

// For moving a vertex after the DynaMesh has already been created.
void DynamicScreenMeshClass::Move_Vertex(int index, float x, float y, float z)
{
	DynamicMeshClass::Move_Vertex( index, (x * 2) - 1, Aspect - (y * 2 * Aspect), 0);
}

// Set position
void DynamicScreenMeshClass::Set_Position(const Vector3 &v)
{
	DynamicMeshClass::Set_Position(Vector3(v.X * 2, -(v.Y * 2 * Aspect), 0));
}

void DynamicScreenMeshClass::Reset()
{
	Reset_Flags();
	Reset_Mesh_Counters();
}


