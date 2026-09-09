#include "rts/w3x_child_discovery.h"

#include <stdio.h>
#include <string.h>

namespace {

int g_failures = 0;

void Expect_Result(const char *case_name,
    rts::W3XTopLevelChildDiscoveryResult expected,
    rts::W3XTopLevelChildDiscoveryResult actual)
{
    if (expected != actual) {
        fprintf(stderr, "%s: expected %s, got %s\n", case_name,
            rts::Get_W3X_Top_Level_Discovery_Result_Name(expected),
            rts::Get_W3X_Top_Level_Discovery_Result_Name(actual));
        ++g_failures;
    }
}

void Expect_Count(const char *case_name, size_t expected, size_t actual)
{
    if (expected != actual) {
        fprintf(stderr, "%s: expected %lu, got %lu\n", case_name,
            static_cast<unsigned long>(expected), static_cast<unsigned long>(actual));
        ++g_failures;
    }
}

void Expect_Kind(const char *case_name,
    rts::W3XTopLevelChildKind expected,
    rts::W3XTopLevelChildKind actual)
{
    if (expected != actual) {
        fprintf(stderr, "%s: expected %s, got %s\n", case_name,
            rts::Get_W3X_Top_Level_Child_Kind_Name(expected),
            rts::Get_W3X_Top_Level_Child_Kind_Name(actual));
        ++g_failures;
    }
}

void Expect_String(const char *case_name, const char *expected, const char *actual)
{
    if (strcmp(expected, actual) != 0) {
        fprintf(stderr, "%s: expected '%s', got '%s'\n", case_name, expected, actual);
        ++g_failures;
    }
}

void Discover_And_Expect_Count(const char *case_name, const char *xml, size_t expected_count)
{
    size_t child_count = 0;
    const rts::W3XTopLevelChildDiscoveryResult result =
        rts::Discover_W3X_Top_Level_Children(xml, strlen(xml), 0, 0, child_count);
    Expect_Result(case_name, rts::W3X_TOP_LEVEL_DISCOVERY_OK, result);
    if (result == rts::W3X_TOP_LEVEL_DISCOVERY_OK) {
        Expect_Count(case_name, expected_count, child_count);
    }
}

} // namespace

int main()
{
    const char canonical[] =
        "<?xml version=\"1.0\"?>\n"
        "<AssetDeclaration xmlns=\"uri:ea.com:eala:asset\">\n"
        "  <!-- direct children only -->\n"
        "  <W3DMesh id=\"Mesh\"><Vertices><V/></Vertices></W3DMesh>\n"
        "  <?tool keep-going?>\n"
        "  <W3DHierarchy id=\"Hierarchy\"/>\n"
        "  <W3DContainer id=\"Container\"><W3DMesh id=\"NestedMustNotCount\"/></W3DContainer>\n"
        "  <W3DAnimation id=\"Animation\"/>\n"
        "  <W3DCollisionBox id=\"Collision\"/>\n"
        "  <FutureAsset id=\"UnknownButPreserved\"/>\n"
        "</AssetDeclaration>";

    rts::W3XTopLevelChildInfo children[6];
    size_t child_count = 0;
    rts::W3XTopLevelChildDiscoveryResult result =
        rts::Discover_W3X_Top_Level_Children(canonical, strlen(canonical), children, 6, child_count);
    Expect_Result("canonical discovery", rts::W3X_TOP_LEVEL_DISCOVERY_OK, result);
    Expect_Count("canonical child count", 6, child_count);
    if (result == rts::W3X_TOP_LEVEL_DISCOVERY_OK && child_count == 6) {
        Expect_Kind("mesh kind", rts::W3X_TOP_LEVEL_CHILD_W3D_MESH, children[0].kind);
        Expect_Kind("hierarchy kind", rts::W3X_TOP_LEVEL_CHILD_W3D_HIERARCHY, children[1].kind);
        Expect_Kind("container kind", rts::W3X_TOP_LEVEL_CHILD_W3D_CONTAINER, children[2].kind);
        Expect_Kind("animation kind", rts::W3X_TOP_LEVEL_CHILD_W3D_ANIMATION, children[3].kind);
        Expect_Kind("collision kind", rts::W3X_TOP_LEVEL_CHILD_W3D_COLLISION_BOX, children[4].kind);
        Expect_Kind("unknown kind", rts::W3X_TOP_LEVEL_CHILD_UNKNOWN, children[5].kind);
        Expect_String("unknown qualified name", "FutureAsset", children[5].qualified_name);
        Expect_String("unknown local name", "FutureAsset", children[5].local_name);
        Expect_String("unknown namespace", rts::W3X_SAGE_ASSET_NAMESPACE, children[5].namespace_uri);
    }

    const char prefixed[] =
        "<ea:AssetDeclaration xmlns:ea=\"uri:ea.com:eala:asset\" "
        "xmlns:model=\"uri:ea.com:eala:asset\">"
        "<ea:W3DMesh/>"
        "<model:W3DHierarchy/>"
        "<local:W3DContainer xmlns:local=\"uri:ea.com:eala:asset\"/>"
        "<W3DAnimation/>"
        "</ea:AssetDeclaration>";
    rts::W3XTopLevelChildInfo prefixed_children[4];
    child_count = 0;
    result = rts::Discover_W3X_Top_Level_Children(
        prefixed, strlen(prefixed), prefixed_children, 4, child_count);
    Expect_Result("prefixed discovery", rts::W3X_TOP_LEVEL_DISCOVERY_OK, result);
    Expect_Count("prefixed child count", 4, child_count);
    if (result == rts::W3X_TOP_LEVEL_DISCOVERY_OK && child_count == 4) {
        Expect_Kind("root prefix inherited", rts::W3X_TOP_LEVEL_CHILD_W3D_MESH, prefixed_children[0].kind);
        Expect_Kind("alternate root binding", rts::W3X_TOP_LEVEL_CHILD_W3D_HIERARCHY, prefixed_children[1].kind);
        Expect_Kind("child-local binding", rts::W3X_TOP_LEVEL_CHILD_W3D_CONTAINER, prefixed_children[2].kind);
        Expect_Kind("unprefixed child has no default namespace", rts::W3X_TOP_LEVEL_CHILD_UNKNOWN, prefixed_children[3].kind);
        Expect_String("alternate qualified name", "model:W3DHierarchy", prefixed_children[1].qualified_name);
        Expect_String("alternate local name", "W3DHierarchy", prefixed_children[1].local_name);
    }

    const char namespace_override[] =
        "<AssetDeclaration xmlns=\"uri:ea.com:eala:asset\">"
        "<W3DMesh xmlns=\"urn:not-ea\"/>"
        "<W3DAnimation/>"
        "</AssetDeclaration>";
    rts::W3XTopLevelChildInfo override_children[2];
    child_count = 0;
    result = rts::Discover_W3X_Top_Level_Children(
        namespace_override, strlen(namespace_override), override_children, 2, child_count);
    Expect_Result("namespace override", rts::W3X_TOP_LEVEL_DISCOVERY_OK, result);
    Expect_Count("namespace override count", 2, child_count);
    if (result == rts::W3X_TOP_LEVEL_DISCOVERY_OK && child_count == 2) {
        Expect_Kind("wrong namespace stays unknown", rts::W3X_TOP_LEVEL_CHILD_UNKNOWN, override_children[0].kind);
        Expect_String("wrong namespace preserved", "urn:not-ea", override_children[0].namespace_uri);
        Expect_Kind("inherited default namespace", rts::W3X_TOP_LEVEL_CHILD_W3D_ANIMATION, override_children[1].kind);
    }

    const char misc_content[] =
        "<AssetDeclaration xmlns=\"uri:ea.com:eala:asset\">"
        "text<![CDATA[not an element <W3DMesh/>]]><!--c--><?pi x?>"
        "<W3DMesh note=\"quoted > is safe\"/>"
        "</AssetDeclaration><!--tail--><?tail ok?>";
    Discover_And_Expect_Count("misc content ignored", misc_content, 1);

    const char empty[] =
        "<AssetDeclaration xmlns=\"uri:ea.com:eala:asset\"/>";
    Discover_And_Expect_Count("self-closing root", empty, 0);

    const char malformed_nested[] =
        "<AssetDeclaration xmlns=\"uri:ea.com:eala:asset\">"
        "<W3DMesh><A></W3DMesh></A>"
        "</AssetDeclaration>";
    child_count = 0;
    result = rts::Discover_W3X_Top_Level_Children(
        malformed_nested, strlen(malformed_nested), 0, 0, child_count);
    Expect_Result("malformed nested closure", rts::W3X_TOP_LEVEL_DISCOVERY_MALFORMED, result);

    const char mismatched_child[] =
        "<AssetDeclaration xmlns=\"uri:ea.com:eala:asset\">"
        "<W3DMesh></W3DHierarchy>"
        "</AssetDeclaration>";
    child_count = 0;
    result = rts::Discover_W3X_Top_Level_Children(
        mismatched_child, strlen(mismatched_child), 0, 0, child_count);
    Expect_Result("mismatched child closure", rts::W3X_TOP_LEVEL_DISCOVERY_MALFORMED, result);

    const char missing_root_close[] =
        "<AssetDeclaration xmlns=\"uri:ea.com:eala:asset\"><W3DMesh/>";
    child_count = 0;
    result = rts::Discover_W3X_Top_Level_Children(
        missing_root_close, strlen(missing_root_close), 0, 0, child_count);
    Expect_Result("missing root close", rts::W3X_TOP_LEVEL_DISCOVERY_MALFORMED, result);

    const char trailing_root[] =
        "<AssetDeclaration xmlns=\"uri:ea.com:eala:asset\"/>"
        "<AssetDeclaration xmlns=\"uri:ea.com:eala:asset\"/>";
    child_count = 0;
    result = rts::Discover_W3X_Top_Level_Children(
        trailing_root, strlen(trailing_root), 0, 0, child_count);
    Expect_Result("second root rejected", rts::W3X_TOP_LEVEL_DISCOVERY_MALFORMED, result);

    const char undeclared_prefix[] =
        "<AssetDeclaration xmlns=\"uri:ea.com:eala:asset\"><x:W3DMesh/></AssetDeclaration>";
    child_count = 0;
    result = rts::Discover_W3X_Top_Level_Children(
        undeclared_prefix, strlen(undeclared_prefix), 0, 0, child_count);
    Expect_Result("undeclared child prefix", rts::W3X_TOP_LEVEL_DISCOVERY_MALFORMED, result);

    const char inner_doctype[] =
        "<AssetDeclaration xmlns=\"uri:ea.com:eala:asset\"><!DOCTYPE Nope></AssetDeclaration>";
    child_count = 0;
    result = rts::Discover_W3X_Top_Level_Children(
        inner_doctype, strlen(inner_doctype), 0, 0, child_count);
    Expect_Result("inner DOCTYPE rejected", rts::W3X_TOP_LEVEL_DISCOVERY_UNSUPPORTED_DOCTYPE, result);

    const char wrong_namespace[] =
        "<AssetDeclaration xmlns=\"urn:not-ea\"><W3DMesh/></AssetDeclaration>";
    child_count = 0;
    result = rts::Discover_W3X_Top_Level_Children(
        wrong_namespace, strlen(wrong_namespace), 0, 0, child_count);
    Expect_Result("wrong envelope namespace",
        rts::W3X_TOP_LEVEL_DISCOVERY_NOT_SAGE_ASSET_DECLARATION, result);

    const char wrong_root[] =
        "<W3DMesh xmlns=\"uri:ea.com:eala:asset\"/>";
    child_count = 0;
    result = rts::Discover_W3X_Top_Level_Children(
        wrong_root, strlen(wrong_root), 0, 0, child_count);
    Expect_Result("wrong root",
        rts::W3X_TOP_LEVEL_DISCOVERY_NOT_SAGE_ASSET_DECLARATION, result);

    const char malformed_probe[] = "<AssetDeclaration xmlns=uri:ea.com:eala:asset/>";
    child_count = 0;
    result = rts::Discover_W3X_Top_Level_Children(
        malformed_probe, strlen(malformed_probe), 0, 0, child_count);
    Expect_Result("malformed A1 envelope",
        rts::W3X_TOP_LEVEL_DISCOVERY_MALFORMED, result);

    rts::W3XTopLevelChildInfo too_small[2];
    child_count = 0;
    result = rts::Discover_W3X_Top_Level_Children(
        canonical, strlen(canonical), too_small, 2, child_count);
    Expect_Result("small output buffer", rts::W3X_TOP_LEVEL_DISCOVERY_OUTPUT_TOO_SMALL, result);
    Expect_Count("small output reports total", 6, child_count);
    Expect_Kind("small output keeps first child", rts::W3X_TOP_LEVEL_CHILD_W3D_MESH, too_small[0].kind);
    Expect_Kind("small output keeps second child", rts::W3X_TOP_LEVEL_CHILD_W3D_HIERARCHY, too_small[1].kind);

    child_count = 123;
    result = rts::Discover_W3X_Top_Level_Children(canonical, strlen(canonical), 0, 1, child_count);
    Expect_Result("null output with nonzero capacity", rts::W3X_TOP_LEVEL_DISCOVERY_INVALID_ARGUMENT, result);
    Expect_Count("invalid argument resets count", 0, child_count);

    if (g_failures != 0) {
        fprintf(stderr, "%d W3X child-discovery test(s) failed.\n", g_failures);
        return 1;
    }

    puts("W3X top-level child-discovery tests passed.");
    return 0;
}
