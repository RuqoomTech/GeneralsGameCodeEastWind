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

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Linegroup.cpp                                                *
 *                                                                                             *
 *                     $Archive::                                                             $*
 *                                                                                             *
 *              Original Author:: Hector Yee                                                   *
 *                                                                                             *
 *                      $Author:: Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 06/26/02 4:04p                                             $*
 *                                                                                             *
 *                    $Revision:: 2                                                            $*
 *                                                                                             *
 * 06/26/02 KM Matrix name change to avoid MAX conflicts                                       *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "WWLib/sharebuf.h"
#include "linegrp.h"
#include "texture.h"
#include "WWMath/wwmath.h"
#include "rinfo.h"
#include "camera.h"
#include "sortingrenderer.h"
#include <array>
#include <cstdio>

// Line groups are a rendering primitive similar to point groups
// They are tetrahedra which are aligned with the view plane with their centers
// at StartLineLoc. The apex of the tetrahedron is at EndLineLoc.
// They can be individually colored LineDiffuse
// and the LineUCoord determines the U coordinate of the texture to use
// the V coordinate is always 0 at the flat end of the tetrahedron
// and 1 at the apex
LineGroupClass::LineGroupClass() :
	StartLineLoc(nullptr),
	EndLineLoc(nullptr),
	LineDiffuse(nullptr),
	TailDiffuse(nullptr),
	ALT(nullptr),
	LineSize(nullptr),
	LineUCoord(nullptr),
	LineCount(0),
	Texture(nullptr),
	Flags(0),
	Shader(ShaderClass::_PresetAdditiveSpriteShader),
	DefaultLineSize(0.0f),
	DefaultLineColor(1.0f, 1.0f, 1.0f),
	DefaultLineAlpha(1.0f),
	DefaultLineUCoord(0.0f),
	DefaultTailDiffuse(0.0f, 0.0f, 0.0f, 0.0f),
	LineMode(TETRAHEDRON)
{
}

LineGroupClass::~LineGroupClass()
{
	REF_PTR_RELEASE(StartLineLoc);
	REF_PTR_RELEASE(EndLineLoc);
	REF_PTR_RELEASE(LineDiffuse);
	REF_PTR_RELEASE(TailDiffuse);
	REF_PTR_RELEASE(ALT);
	REF_PTR_RELEASE(LineSize);
	REF_PTR_RELEASE(LineUCoord);
	REF_PTR_RELEASE(Texture);
}

void LineGroupClass::Set_Arrays(
	ShareBufferClass<Vector3> *startlocs,
	ShareBufferClass<Vector3> *endlocs,
	ShareBufferClass<Vector4> *diffuse,
	ShareBufferClass<Vector4> *taildiffuse,
	ShareBufferClass<unsigned int> *alt,
	ShareBufferClass<float> *sizes,
	ShareBufferClass<float> *ucoords,
	int active_line_count
	)
{
	// The Line locations arrays are NOT optional!
	WWASSERT(startlocs);
	WWASSERT(endlocs);

	// Ensure lengths of all arrays are the same:
	WWASSERT(startlocs->Get_Count() == endlocs->Get_Count());
	WWASSERT(!diffuse || startlocs->Get_Count() == diffuse->Get_Count());
	WWASSERT(!alt || startlocs->Get_Count() == alt->Get_Count());
	WWASSERT(!sizes || startlocs->Get_Count() == sizes->Get_Count());
	WWASSERT(!ucoords || startlocs->Get_Count() == ucoords->Get_Count());
	WWASSERT(!taildiffuse || startlocs->Get_Count() == taildiffuse->Get_Count());

	REF_PTR_SET(StartLineLoc,startlocs);
	REF_PTR_SET(EndLineLoc,endlocs);
	REF_PTR_SET(LineDiffuse,diffuse);
	REF_PTR_SET(TailDiffuse,taildiffuse);
	REF_PTR_SET(ALT,alt);
	REF_PTR_SET(LineSize,sizes);
	REF_PTR_SET(LineUCoord,ucoords);

	if (ALT) {
		LineCount = active_line_count;
	} else {
		LineCount = (active_line_count >= 0) ? active_line_count : StartLineLoc->Get_Count();
	}

}

void LineGroupClass::Set_Line_Size(float size)
{
	DefaultLineSize = size;
}

float LineGroupClass::Get_Line_Size()
{
	return DefaultLineSize;
}

void LineGroupClass::Set_Line_Color(const Vector3 &color)
{
	DefaultLineColor = color;
}

Vector3 LineGroupClass::Get_Line_Color()
{
	return DefaultLineColor;
}

void LineGroupClass::Set_Tail_Diffuse(const Vector4 &tdiffuse)
{
	DefaultTailDiffuse = tdiffuse;
}

Vector4 LineGroupClass::Get_Tail_Diffuse()
{
	return DefaultTailDiffuse;
}

void LineGroupClass::Set_Line_Alpha(float alpha)
{
	DefaultLineAlpha = alpha;
}

float LineGroupClass::Get_Line_Alpha()
{
	return DefaultLineAlpha;
}

void LineGroupClass::Set_Line_UCoord(float ucoord)
{
	DefaultLineUCoord = ucoord;
}

float LineGroupClass::Get_Line_UCoord()
{
	return DefaultLineUCoord;
}

void LineGroupClass::Set_Flag(FlagsType flag, bool on)
{
	if (on) Flags |= 1 << flag;
	else
		Flags &= ~(1 << flag);
}

int LineGroupClass::Get_Flag(FlagsType flag)
{
	return (Flags >> flag) & 0x1;
}

void LineGroupClass::Set_Texture(TextureClass* texture)
{
	REF_PTR_SET(Texture,texture);
}

TextureClass * LineGroupClass::Get_Texture()
{
	if (Texture) Texture->Add_Ref();
	return Texture;
}

TextureClass * LineGroupClass::Peek_Texture()
{
	return Texture;
}

void LineGroupClass::Set_Shader(const ShaderClass &shader)
{
	Shader = shader;
}

ShaderClass LineGroupClass::Get_Shader()
{
	return Shader;
}

void LineGroupClass::Set_Line_Mode(LineModeType linemode)
{
	LineMode = linemode;
}

LineGroupClass::LineModeType LineGroupClass::Get_Line_Mode()
{
	return LineMode;
}

void LineGroupClass::Render(RenderInfoClass &rinfo)
{
 if (!LineCount) return;
 if (LineCount < 0 || !StartLineLoc || !EndLineLoc ||
     (LineMode != TETRAHEDRON && LineMode != PRISM) ||
     (ALT && ALT->Get_Count() < LineCount)) {
  std::fprintf(stderr, "WW3D: invalid line group arrays\n"); return;
 }
 for (int i = 0; i < LineCount; ++i) {
  const unsigned point = ALT ? ALT->Get_Element(i) : static_cast<unsigned>(i);
  if (point >= static_cast<unsigned>(StartLineLoc->Get_Count()) ||
      point >= static_cast<unsigned>(EndLineLoc->Get_Count()) ||
      (LineSize && point >= static_cast<unsigned>(LineSize->Get_Count())) ||
      (LineDiffuse && point >= static_cast<unsigned>(LineDiffuse->Get_Count())) ||
      (LineUCoord && point >= static_cast<unsigned>(LineUCoord->Get_Count())) ||
      (TailDiffuse && point >= static_cast<unsigned>(TailDiffuse->Get_Count()))) {
   std::fprintf(stderr, "WW3D: line group index exceeds shared arrays\n"); return;
  }
 }
 Shader.Set_Cull_Mode(ShaderClass::CULL_MODE_ENABLE);
 const float value_255 = 0.9961f;
 const bool white = DefaultLineColor.X > value_255 && DefaultLineColor.Y > value_255 &&
                    DefaultLineColor.Z > value_255 && DefaultLineAlpha > value_255;
 Shader.Set_Primary_Gradient(LineDiffuse || !white || !Texture ? ShaderClass::GRADIENT_MODULATE :
                             ShaderClass::GRADIENT_DISABLE);
 Shader.Set_Texturing(Texture ? ShaderClass::TEXTURING_ENABLE : ShaderClass::TEXTURING_DISABLE);
 RenderBackendMaterialState material;
 if (!Shader.Get_Render_Backend_State(material)) {
  std::fprintf(stderr, "WW3D: unsupported line group shader\n"); return;
 }
 const bool sort = Shader.Get_Dst_Blend_Func() != ShaderClass::DSTBLEND_ZERO &&
                   Shader.Get_Alpha_Test() == ShaderClass::ALPHATEST_DISABLE && WW3D::Is_Sorting_Enabled();
 Vector3 offsets[3] = {
  Vector3(WWMath::Cos(WWMATH_PI / 2), WWMath::Sin(WWMATH_PI / 2), 0),
  Vector3(WWMath::Cos(7 * WWMATH_PI / 6), WWMath::Sin(7 * WWMATH_PI / 6), 0),
  Vector3(WWMath::Cos(11 * WWMATH_PI / 6), WWMath::Sin(11 * WWMATH_PI / 6), 0)
 };
 const bool world_space = Get_Flag(TRANSFORM) != 0;
 const Matrix3D &camera_world = rinfo.Camera.Get_Transform();
 if (world_space) {
  // Preserve the original streak cross-section orientation.
  Matrix3D rotation = camera_world;
  rotation.Set_Translation(Vector3(0, 0, 0));
  rotation.Get_Orthogonal_Inverse(rotation);
  for (auto &offset : offsets) Matrix3D::Transform_Vector(rotation, offset, &offset);
 }
 constexpr unsigned short tetra_indices[] = {0,2,1, 0,3,2, 0,1,3, 1,2,3};
 constexpr unsigned short prism_indices[] = {0,1,2, 0,3,1, 1,3,4, 1,4,5,
                                             1,5,2, 0,2,5, 0,5,3, 3,5,4};
 const unsigned vertices_per_line = LineMode == PRISM ? 6 : 4;
 const unsigned indices_per_line = LineMode == PRISM ? 24 : 12;
 const unsigned short *topology = LineMode == PRISM ? prism_indices : tetra_indices;
 constexpr unsigned max_vertices = 2048;
 std::array<RenderBackendTexturedVertex, max_vertices> vertices;
 std::array<unsigned short, max_vertices * 4> indices;
 const unsigned batch_lines = max_vertices / vertices_per_line;
 for (unsigned first = 0; first < static_cast<unsigned>(LineCount); first += batch_lines) {
  const unsigned count = MIN(static_cast<unsigned>(LineCount) - first, batch_lines);
  unsigned vertex_count = 0, index_count = 0;
  for (unsigned i = 0; i < count; ++i) {
   const unsigned point = ALT ? ALT->Get_Element(first + i) : first + i;
   const Vector3 start = StartLineLoc->Get_Element(point), end = EndLineLoc->Get_Element(point);
   const float size = LineSize ? LineSize->Get_Element(point) : DefaultLineSize;
   const float u = LineUCoord ? LineUCoord->Get_Element(point) : DefaultLineUCoord;
   const Vector4 head = LineDiffuse ? LineDiffuse->Get_Element(point) :
       Vector4(DefaultLineColor.X, DefaultLineColor.Y, DefaultLineColor.Z, DefaultLineAlpha);
   const Vector4 tail = TailDiffuse ? TailDiffuse->Get_Element(point) : DefaultTailDiffuse;
   const unsigned base = vertex_count;
   const auto append = [&](Vector3 position, Vector4 color, float v) {
    if (!world_space) Matrix3D::Transform_Vector(camera_world, position, &position);
    for (int c = 0; c < 4; ++c)
     color[c] = static_cast<unsigned>(WWMath::Clamp(color[c], 0.0f, 1.0f) * 255.0f) / 255.0f;
    vertices[vertex_count++] = {position.X, position.Y, position.Z, color.X, color.Y, color.Z, color.W, u, v};
   };
   if (LineMode == TETRAHEDRON) {
    append(end, tail, 1.0f);
    for (const auto &offset : offsets) append(start + size * offset, head, 0.0f);
   } else {
    for (const auto &offset : offsets) append(start + size * offset, head, 0.0f);
    for (const auto &offset : offsets) append(end + size * offset, tail, 1.0f);
   }
   for (unsigned j = 0; j < indices_per_line; ++j) indices[index_count++] = base + topology[j];
  }
  if (!SortingRendererClass::Submit_CPU_Triangles(vertices.data(), vertex_count, indices.data(),
       index_count, Texture, material, rinfo.Camera, nullptr, sort)) {
   std::fprintf(stderr, "WW3D: line group triangle submission failed\n"); return;
  }
 }
}

int LineGroupClass::Get_Polygon_Count()
{
	switch (LineMode) {
		case TETRAHEDRON:
			return LineCount * 4;
			break;
		case PRISM:
			return LineCount * 8;
			break;
	}
	WWASSERT(0);
	return 0;
}
