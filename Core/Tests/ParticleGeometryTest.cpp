#include "WW3D2/pointgr.h"
#include "WW3D2/ww3d.h"
#include "WWMath/matrix4.h"
#include "WWMath/wwmath.h"

#include <cmath>
#include <iostream>
#include <vector>

// This test supplies a fixed output size to the production screen-size expansion.
// It does not create a renderer device or substitute native buffer implementations.
void WW3D::Get_Render_Target_Resolution(int &w, int &h, int &bits, bool &windowed)
{
    w = 800; h = 600; bits = 32; windowed = true;
}

extern VectorClass<Vector3> VertexLoc;
extern VectorClass<Vector4> VertexDiffuse;
extern VectorClass<Vector2> VertexUV;

namespace {
int failures = 0;
void Check(bool condition, const char *description)
{
    if (!condition) { std::cerr << description << '\n'; ++failures; }
}
bool Near(float a, float b) { return std::fabs(a - b) < 0.003f; }
bool Near(const Vector3 &a, const Vector3 &b)
{
    return Near(a.X, b.X) && Near(a.Y, b.Y) && Near(a.Z, b.Z);
}

class PointGeometryPeer final : public PointGroupClass {
public:
    void Expand(Vector3 *locations, Vector4 *colors, float *sizes,
                unsigned char *orientations, unsigned char *frames, int count,
                const Matrix4x4 &view = Matrix4x4(true))
    {
        int vertices = 0, polygons = 0;
        Update_Arrays(locations, colors, sizes, orientations, frames, count, count,
                      vertices, polygons, view);
        Check(vertices == count * (Get_Point_Mode() == QUADS ? 4 : 3), "Expansion vertex count");
        Check(polygons == count * (Get_Point_Mode() == QUADS ? 2 : 1), "Expansion polygon count");
    }
    void View_Plane() { VPXMin = VPYMin = -1; VPXMax = VPYMax = 1; }
};

void Test_Triangles_And_Atlas()
{
    PointGeometryPeer group;
    Check(group.Get_Billboard(), "Default particles face the camera");
    group.Set_Point_Mode(PointGroupClass::TRIS);
    group.Set_Point_Size(2);
    group.Set_Frame_Row_Column_Count_Log2(1);
    group.Set_Point_Frame(255); // Atlas frame is masked to its low bits by the real caller.
    Vector3 position(10,20,-30);
    Vector4 color(0.2f,0.4f,0.6f,0.8f);
    group.Expand(&position, &color, nullptr, nullptr, nullptr, 1);
    Check(Near(VertexLoc[0], Vector3(10,16,-30)), "Triangle first vertex");
    Check(Near(VertexLoc[1], Vector3(6.536f,22,-30)), "Triangle second vertex");
    Check(Near(VertexLoc[2], Vector3(13.464f,22,-30)), "Triangle third vertex");
    Check(Near(VertexUV[0].X,0.75f) && Near(VertexUV[0].Y,0.5f), "Triangle atlas masked frame");
    for (int i=0;i<3;++i) Check(VertexDiffuse[i] == color, "Triangle per-point diffuse expansion");

    float size = 1;
    unsigned char orientation = 64, frame = 254;
    group.Expand(&position, nullptr, &size, &orientation, &frame, 1);
    Check(Near(VertexLoc[0],Vector3(12,20,-30)), "Quarter-turn triangle orientation");
    Check(Near(VertexUV[0].X,0.25f) && Near(VertexUV[0].Y,0.5f), "Triangle per-point atlas frame");
}

void Test_Quad_Ground_And_Screensize()
{
    PointGeometryPeer group;
    group.Set_Point_Mode(PointGroupClass::QUADS);
    group.Set_Point_Size(2);
    Vector3 position(3,4,5);
    group.Expand(&position, nullptr, nullptr, nullptr, nullptr, 1);
    Check(Near(VertexLoc[0],Vector3(2,5,5)) && Near(VertexLoc[2],Vector3(4,3,5)), "Quad offsets");
    Check(VertexUV[0] == Vector2(0,0) && VertexUV[2] == Vector2(1,1), "Quad UV corners");

    group.Set_Billboard(false);
    float size = 2;
    unsigned char orientation = 0;
    Matrix3D camera_view(true);
    camera_view.Rotate_X(0.3f);
    camera_view.Rotate_Z(0.4f);
    camera_view.Set_Translation(Vector3(-11,7,9));
    Matrix4x4 view(camera_view);
    group.Expand(&position, nullptr, &size, &orientation, nullptr, 1, view);
    const Vector3 corners[] = {Vector3(5,6,5), Vector3(5,2,5), Vector3(1,2,5), Vector3(1,6,5)};
    for (int i=0;i<4;++i) {
        const Vector4 expected = view * corners[i];
        Check(Near(VertexLoc[i], Vector3(expected.X,expected.Y,expected.Z)), "Ground quad uses supplied camera view");
    }

    group.Set_Point_Mode(PointGroupClass::SCREENSPACE);
    group.View_Plane();
    Vector3 points[] = {Vector3(0,0,0), Vector3(0,0,0)};
    float sizes[] = {1,2};
    group.Expand(points, nullptr, sizes, nullptr, nullptr, 2);
    Check(Near(VertexLoc[0],Vector3(0.5f*2/800,0,-1)), "One-pixel particle footprint");
    Check(Near(VertexLoc[3],Vector3(2.0f/800,-1.0f/600,-1)), "Two-pixel particle footprint");
}

void Test_Large_Expansion_And_Reinitialize()
{
    PointGeometryPeer group;
    std::vector<Vector3> locations(1367,Vector3(1,2,3));
    std::vector<Vector4> colors(locations.size(),Vector4(1,0.5f,0.25f,0.75f));
    group.Set_Point_Mode(PointGroupClass::TRIS);
    group.Set_Point_Size(1);
    group.Expand(locations.data(),colors.data(),nullptr,nullptr,nullptr,static_cast<int>(locations.size()));
    Check(Near(VertexLoc[4098],Vector3(1,0,3)), "Triangle geometry beyond 2048 vertex boundary");
    Check(VertexDiffuse[4100] == colors.back(), "Last expanded particle color");
}
}

int main()
{
    WWMath::Init();
    PointGroupClass::_Init();
    Test_Triangles_And_Atlas();
    Test_Quad_Ground_And_Screensize();
    Test_Large_Expansion_And_Reinitialize();
    PointGroupClass::_Shutdown();
    PointGroupClass::_Init();
    Test_Triangles_And_Atlas();
    PointGroupClass::_Shutdown();
    WWMath::Shutdown();
    return failures ? 1 : 0;
}
