// Exercise the production private snapshot queue, without constructing a
// window/device or linking the unrelated scene/asset graph through CameraClass.
// CameraClass itself remains the production bridge in the game build.
#include "WW3D2/sortingrenderer.cpp"
#include <iostream>
#include <limits>

bool WW3D::IsSortingEnabled = true;

// Texture loading is outside this untextured queue test. Fail if a fixture
// accidentally crosses that boundary; GPU texture lifetime has separate tests.
bool TextureClass::Ensure_Renderer_Texture() { return false; }
RenderBackendTextureHandle TextureBaseClass::Get_Renderer_Texture() const { return {}; }

namespace {
int failures = 0;
void Check(bool condition, const char *name)
{
    if (!condition) { std::cerr << name << '\n'; ++failures; }
}
bool Equal(const Matrix4x4 &a, const Matrix4x4 &b)
{
    for (unsigned row = 0; row < 4; ++row)
        for (unsigned column = 0; column < 4; ++column)
            if (std::fabs(a[row][column] - b[row][column]) > 1.0e-5f) return false;
    return true;
}
class TraceBackend final : public IRenderBackend {
public:
    Matrix4x4 matrix{true};
    std::vector<float> order;
    std::vector<Matrix4x4> matrices;
    std::vector<float> alphas;
    std::vector<float> coordinates;
    bool reject = false;
    void Reset_Trace() { order.clear(); matrices.clear(); alphas.clear(); coordinates.clear(); reject=false; }
    bool Draw_Indexed_Material_Triangles(const RenderBackendTexturedVertex *vertices, unsigned,
        const unsigned short *indices, unsigned count, RenderBackendTextureHandle,
        const RenderBackendMaterialState &state, RenderBackendTextureHandle) override
    {
        for (unsigned i=0;i<count;i+=3) {
            order.push_back(vertices[indices[i]].r);
            coordinates.push_back(vertices[indices[i]].x);
            alphas.push_back(state.alpha_reference);
            matrices.push_back(matrix);
        }
        return !reject;
    }
    void Set_View_Projection(const Matrix4x4 &value) override { matrix=value; }
    void Get_View_Projection(Matrix4x4 &value) const override { value=matrix; }
    void Set_Gamma(float,float,float,bool,bool) override {}
    void Begin_Scene() override {}
    void End_Scene(bool) override {}
    void Flip_To_Primary() override {}
    void Clear(bool,bool,const Vector3&,float,float,unsigned) override {}
    void Set_Viewport(const RenderBackendViewport&) override {}
    RenderBackendTextureHandle Create_Render_Texture(unsigned,unsigned) override { return {}; }
    bool Set_Render_Texture(RenderBackendTextureHandle) override { return false; }
    bool Get_Render_Target_Size(int&,int&) const override { return false; }
    bool Copy_Texture(RenderBackendTextureHandle,RenderBackendTextureHandle) override { return false; }
    void Invalidate_Cached_Render_States() override {}
    bool Has_Stencil() const override { return false; }
    void Reset_Frame_Statistics() override {}
    RenderBackendFrameStatistics Get_Frame_Statistics() const override { return {}; }
    bool Set_Swap_Interval(unsigned) override { return false; }
    unsigned Get_Swap_Interval() const override { return 0; }
    bool Read_Output_RGBA8(unsigned&,unsigned&,std::vector<unsigned char>&) override { return false; }
    RenderBackendTextureHandle Create_Static_RGBA8_Texture(const RenderBackendTextureMipLevel*,unsigned) override { return {}; }
    bool Is_Texture_Valid(RenderBackendTextureHandle) const override { return false; }
    bool Draw_Indexed_Decal_Triangles(const RenderBackendTexturedVertex*,unsigned,
        const unsigned short*,unsigned,RenderBackendTextureHandle,RenderBackendDecalBlendMode) override { return false; }
    bool Draw_Indexed_Terrain_Triangles(const RenderBackendTerrainVertex*,unsigned,
        const unsigned short*,unsigned,RenderBackendTextureHandle,const RenderBackendMaterialState&,
        const RenderBackendTerrainState&) override { return false; }
    void Release_Texture(RenderBackendTextureHandle) override {}
    void Set_Ambient(const Vector3&) override {}
    void Set_Light_Environment(LightEnvironmentClass*) override {}
};
TraceBackend backend;

bool Submit(float id, float z, float bound_z, bool bounded=true, bool sort=true,
            const Matrix3D &view=Matrix3D(true), const Matrix4x4 &projection=Matrix4x4(true))
{
    RenderBackendTexturedVertex vertices[3] = {
        {id,0,z,id,0,0,1,0,0}, {id,1,z,id,0,0,1,0,0}, {id+1,0,z,id,0,0,1,0,0}
    };
    unsigned short indices[3]={0,1,2};
    RenderBackendMaterialState material;
    material.alpha_reference = id/100;
    auto node=Prepare_Submission(vertices,3,indices,3,nullptr,material);
    if (!node) return false;
    const SphereClass bounds(Vector3(0,0,bound_z),1);
    const bool result=Submit_Snapshot(std::move(node),view,projection,bounded ? &bounds : nullptr,sort);
    // The queued draw must survive later mutation of every caller-owned input.
    for (auto &vertex : vertices) { vertex.x=999; vertex.r=999; vertex.z=999; }
    indices[0]=2;
    material.alpha_reference=999;
    return result;
}

void Test_Global_Order_And_Copy()
{
    backend.Reset_Trace();
    Check(Submit(1,-3,80),"First bounded submission");
    Check(Submit(2,-9,-80),"Second bounded submission");
    Check(Submit(3,-5,0,false),"Unbounded submission");
    Check(backend.order.empty(),"Deferred until flush");
    Check(SortingRendererClass::Flush(),"Global flush succeeds");
    Check(backend.order == std::vector<float>({2,3,1}),"Global ascending centroid Z overrides node bounds");
    Check(backend.coordinates == std::vector<float>({2,3,1}),"Copied geometry survives caller mutation");
    Check(backend.alphas == std::vector<float>({0.02f,0.03f,0.01f}),"Copied material survives caller mutation");
    Check(SortingRendererClass::Flush(),"Empty flush succeeds");
    Check(backend.order.size()==3,"Empty flush does not repeat queued draws");
}

void Test_Equal_Z_Node_Order()
{
    backend.Reset_Trace();
    Check(Submit(1,-5,-4),"Negative bound node");
    Check(Submit(2,-5,4),"Positive bound node");
    Check(Submit(3,-5,0,false),"Unbounded zero node");
    Check(Submit(4,-5,4),"Equal positive bound node");
    Check(Submit(5,-5,0,false),"Second unbounded node");
    Check(SortingRendererClass::Flush(),"Equal centroid flush");
    Check(backend.order == std::vector<float>({2,4,3,5,1}),"Descending sphere node order, stable equal bounds and unbounded splice");

    // A fixture larger than the original insertion-sort cutoff exercises its
    // median-partition equal-Z behavior. The sequence was characterized from
    // the original production sorter before migration.
    backend.Reset_Trace();
    for (unsigned i=0;i<20;++i) Check(Submit(float(i),-5,0,false),"Large equal-Z submission");
    Check(SortingRendererClass::Flush(),"Large equal-Z flush");
    const std::vector<float> expected={0,10,18,17,16,15,14,13,12,11,9,8,7,6,5,4,3,2,1,19};
    Check(backend.order==expected,"Original equal-Z quicksort partition sequence");
}

void Test_Snapshot_And_Restore()
{
    backend.Reset_Trace();
    Matrix3D view(true); view.Set_Translation(Vector3(0,0,-20));
    Matrix4x4 projection(true); projection[0][0]=2; projection[1][1]=3;
    const Matrix4x4 snapshot=projection*Matrix4x4(view);
    Check(Submit(1,10,10,true,true,view,projection),"Snapshot first submission");
    Check(Submit(2,-5,-5),"Snapshot second submission");
    Matrix4x4 current(true); current[0][3]=37;
    backend.matrix=current;
    projection[0][0]=99; view.Set_Translation(Vector3(0,0,999));
    Check(SortingRendererClass::Flush(),"Snapshot flush");
    Check(backend.order==std::vector<float>({1,2}),"Camera Z is captured per submission");
    Check(Equal(backend.matrices[0],snapshot),"Captured camera projection/view replayed");
    Check(Equal(backend.matrix,current),"Flush restores actual current backend matrix");
    backend.Reset_Trace(); backend.reject=true;
    Check(!Submit(3,-3,0,false,false),"Immediate backend rejection propagates");
    Check(Equal(backend.matrix,current),"Rejected immediate draw restores matrix");
    Check(Submit(4,-4,0,false),"Deferred rejected draw submission");
    Check(!SortingRendererClass::Flush(),"Deferred backend rejection propagates");
    Check(Equal(backend.matrix,current),"Rejected flush restores matrix");
    Check(SortingRendererClass::Flush(),"Rejected queue released");
}

void Test_Rejection_And_Deinit()
{
    backend.Reset_Trace();
    RenderBackendTexturedVertex vertices[3]={{0,0,0,1,1,1,1,0,0},{1,0,0,1,1,1,1,0,0},{0,1,0,1,1,1,1,0,0}};
    unsigned short indices[3]={0,1,3};
    RenderBackendMaterialState state;
    Check(!Prepare_Submission(vertices,3,indices,3,nullptr,state),"Out-of-range CPU index rejected");
    indices[2]=2;
    Check(!Prepare_Submission(vertices,3,indices,2,nullptr,state),"Partial triangle rejected");
    vertices[0].z=std::numeric_limits<float>::quiet_NaN();
    Check(!Prepare_Submission(vertices,3,indices,3,nullptr,state),"Nonfinite position rejected");
    Check(Submit(1,-3,0,false),"Deinit queued submission");
    SortingRendererClass::Deinit();
    Check(SortingRendererClass::Flush() && backend.order.empty(),"Deinit discards CPU queue");
}
}

IRenderBackend *WW3D::RenderBackend=&backend;

int main()
{
    Test_Global_Order_And_Copy();
    Test_Equal_Z_Node_Order();
    Test_Snapshot_And_Restore();
    Test_Rejection_And_Deinit();
    SortingRendererClass::Deinit();
    return failures ? 1 : 0;
}
