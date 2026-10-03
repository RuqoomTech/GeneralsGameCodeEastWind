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

#include "sortingrenderer.h"
#include "ww3d.h"
#include "camera.h"
#include "texture.h"
#include "WWMath/sphere.h"
#include "WWLib/win.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <list>
#include <memory>
#include <vector>

bool SortingRendererClass::_EnableTriangleDraw = true;
namespace {
unsigned minimum_vertices = 32768;
bool failure(const char *reason)
{
    char message[256];
    std::snprintf(message, sizeof(message), "SortingRenderer: %s\n", reason);
    std::fputs(message, stderr);
    OutputDebugStringA(message);
    return false;
}
struct SortingSubmission
{
    std::vector<RenderBackendTexturedVertex> vertices;
    std::vector<unsigned short> indices;
    TextureClass *texture = nullptr;
    RenderBackendMaterialState material;
    Matrix3D view;
    Matrix4x4 view_projection;
    float center_z = 0;
    bool bounded = false;
    ~SortingSubmission() { if (texture) texture->Release_Ref(); }
};
std::list<std::unique_ptr<SortingSubmission>> sorted_nodes;
std::list<std::unique_ptr<SortingSubmission>> unbounded_nodes;
Matrix4x4 restore_view_projection(true);
bool draw(SortingSubmission &node, const unsigned short *indices, unsigned count)
{
    auto *backend = WW3D::Get_Render_Backend();
    if (!backend) return failure("missing render backend");
    RenderBackendTextureHandle handle;
    if (node.texture) {
        if (!node.texture->Ensure_Renderer_Texture()) return failure("texture upload failed");
        handle = node.texture->Get_Renderer_Texture();
    }
    backend->Set_View_Projection(node.view_projection);
    return backend->Draw_Indexed_Material_Triangles(node.vertices.data(),
        static_cast<unsigned>(node.vertices.size()), indices, count, handle, node.material)
        || failure("backend rejected CPU triangles");
}
struct ShortVectorIStruct
{
	unsigned short i;
	unsigned short j;
	unsigned short k;
};

struct TempIndexStruct
{
	ShortVectorIStruct tri;
	unsigned int idx;
	float z;
};

bool operator <(const TempIndexStruct &l, const TempIndexStruct &r) { return l.z < r.z; }
bool operator <=(const TempIndexStruct &l, const TempIndexStruct &r) { return l.z <= r.z; }
bool operator >(const TempIndexStruct &l, const TempIndexStruct &r) { return l.z > r.z; }
bool operator >=(const TempIndexStruct &l, const TempIndexStruct &r) { return l.z >= r.z; }
bool operator ==(const TempIndexStruct &l, const TempIndexStruct &r) { return l.z == r.z; }
// ----------------------------------------------------------------------------
static
void InsertionSort(TempIndexStruct *begin, TempIndexStruct *end)
{
	for (TempIndexStruct *iter = begin + 1; iter < end; ++iter) {
		TempIndexStruct val = iter[0];
		TempIndexStruct *insert = iter;
		while (insert != begin && insert[-1] > val) {
			insert[0] = insert[-1];
			insert -= 1;
		}
		insert[0] = val;
	}
}

// ----------------------------------------------------------------------------
static
void Sort(TempIndexStruct *begin, TempIndexStruct *end)
{
	const std::ptrdiff_t diff = end - begin;
	if (diff <= 16) {
		// Insertion sort has less overhead for small arrays
		InsertionSort(begin, end);
	} else {
		// Choose the median of begin, mid, and (end - 1) as the partitioning element.
		// Rearrange so that *(begin + 1) <= *begin <= *(end - 1).  These will be guard
		// elements.
		TempIndexStruct *mid = begin + diff/2;
		std::swap(mid[0], begin[1]);
		if (begin[1] > end[-1]) {
			std::swap(begin[1], end[-1]);
		}
		if (begin[0] > end[-1]) {
			std::swap(begin[0], end[-1]);
		}
		if (begin[1] > begin[0]) {
			std::swap(begin[1], begin[0]);
		}

		// *begin is now the partitioning element
		TempIndexStruct *begin1 = begin + 1;	// TODO: Temp fix until I find out who is passing me NaN
		TempIndexStruct *end1 = end - 1;			// TODO: Temp fix until I find out who is passing me NaN
		TempIndexStruct *left = begin + 1;
		TempIndexStruct *right = end - 1;
		for (;;) {
#if 0		// TODO: Temp fix until I find out who is passing me NaN.
			do ++left; while (left[0] < begin[0]);		// Scan up to find element >= than partition
			do --right; while (right[0] > begin[0]);	// Scan down to find element <= than partition
#else
			do ++left; while (left < end1 && left[0] < begin[0]);		// Scan up to find element >= than partition
			do --right; while (right > begin1 && right[0] > begin[0]);	// Scan down to find element <= than partition
#endif
			if (right < left) break;									// Pointers crossed.  Partitioning completed.
			std::swap(left[0], right[0]);							// Exchange elements.
		}
		std::swap(begin[0], right[0]);							// Insert partition element

		// Sort the smaller subarray first then the larger
		if (right - begin > end - (right + 1)) {
			Sort(right + 1, end);
			Sort(begin, right);
		} else {
			Sort(begin, right);
			Sort(right + 1, end);
		}
	}
}


}

void SortingRendererClass::SetMinVertexBufferSize(unsigned value)
{
    // CPU reserve hint only; upload buffers are owned by the backend.
    minimum_vertices = value;
}

bool SortingRendererClass::Submit_CPU_Triangles(
    const RenderBackendTexturedVertex *vertices, unsigned vertex_count,
    const unsigned short *indices, unsigned index_count, TextureClass *texture,
    const RenderBackendMaterialState &material, const CameraClass &camera,
    const SphereClass *world_bounds, bool sort)
{
    if (!vertices || !indices || !vertex_count || vertex_count > 65536 ||
        !index_count || index_count % 3) return failure("invalid CPU geometry");
    for (unsigned i = 0; i < index_count; ++i)
        if (indices[i] >= vertex_count) return failure("CPU index out of range");
    for (unsigned i = 0; i < vertex_count; ++i)
        if (!std::isfinite(vertices[i].x) || !std::isfinite(vertices[i].y) || !std::isfinite(vertices[i].z))
            return failure("nonfinite CPU position");
    auto node = std::make_unique<SortingSubmission>();
    node->vertices.assign(vertices, vertices + vertex_count);
    node->indices.assign(indices, indices + index_count);
    node->material = material;
    if (texture) {
        if (!texture->Get_Filter().Get_Render_Sampler(node->material.sampler))
            return failure("unsupported CPU texture sampler");
        node->material.clamp_texture = false;
        node->texture = texture;
        texture->Add_Ref();
    }
    auto &mutable_camera = const_cast<CameraClass &>(camera);
    node->view = mutable_camera.Get_View_Matrix();
    Matrix4x4 projection;
    mutable_camera.Get_Zero_To_One_Projection_Matrix(&projection);
    node->view_projection = projection * Matrix4x4(node->view);
    restore_view_projection = node->view_projection;
    if (!sort || !WW3D::Is_Sorting_Enabled())
        return draw(*node, indices, index_count);
    if (world_bounds && world_bounds->Is_Valid()) {
        Vector3 center;
        Matrix3D::Transform_Vector(node->view, world_bounds->Center, &center);
        if (!std::isfinite(center.Z)) return failure("nonfinite sorting bound");
        node->center_z = center.Z;
        node->bounded = true;
        auto position = sorted_nodes.begin();
        while (position != sorted_nodes.end() && node->center_z <= (*position)->center_z) ++position;
        sorted_nodes.insert(position, std::move(node));
    } else unbounded_nodes.push_back(std::move(node));
    return true;
}

bool SortingRendererClass::Flush()
{
    auto position = sorted_nodes.begin();
    while (position != sorted_nodes.end() && (*position)->center_z > 0) ++position;
    sorted_nodes.splice(position, unbounded_nodes);
    if (sorted_nodes.empty()) return true;
    std::vector<SortingSubmission *> nodes;
    std::vector<TempIndexStruct> triangles;
    triangles.reserve(minimum_vertices / 2);
    for (auto &node : sorted_nodes) {
        const unsigned node_index = static_cast<unsigned>(nodes.size());
        nodes.push_back(node.get());
        for (unsigned i = 0; i < node->indices.size(); i += 3) {
            const auto a = node->indices[i], b = node->indices[i+1], c = node->indices[i+2];
            const auto &v1 = node->vertices[a], &v2 = node->vertices[b], &v3 = node->vertices[c];
            const Vector3 center((v1.x + v2.x + v3.x)/3.0f,
                (v1.y + v2.y + v3.y)/3.0f, (v1.z + v2.z + v3.z)/3.0f);
            Vector3 camera_center;
            Matrix3D::Transform_Vector(node->view, center, &camera_center);
            const float z = camera_center.Z;
            if (!std::isfinite(z)) { Deinit(); return failure("nonfinite sorting centroid"); }
            triangles.push_back({{a,b,c}, node_index, z});
        }
    }
    // Preserve the original global ascending centroid-Z sort and its equal-Z
    // partition behavior, after descending bounded-node insertion.
    if (triangles.size() > 1) Sort(triangles.data(), triangles.data() + triangles.size());
    bool success = true;
    if (_EnableTriangleDraw) {
        unsigned start = 0;
        std::vector<unsigned short> indices;
        while (start < triangles.size()) {
            const unsigned node_index = triangles[start].idx;
            unsigned end = start;
            indices.clear();
            while (end < triangles.size() && triangles[end].idx == node_index && indices.size() < 65535) {
                indices.push_back(triangles[end].tri.i);
                indices.push_back(triangles[end].tri.j);
                indices.push_back(triangles[end].tri.k);
                ++end;
            }
            if (!draw(*nodes[node_index], indices.data(), static_cast<unsigned>(indices.size()))) success = false;
            start = end;
        }
    }
    sorted_nodes.clear();
    if (auto *backend = WW3D::Get_Render_Backend()) backend->Set_View_Projection(restore_view_projection);
    return success;
}

void SortingRendererClass::Deinit()
{
    sorted_nodes.clear();
    unbounded_nodes.clear();
}
