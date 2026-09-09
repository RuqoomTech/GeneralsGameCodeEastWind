#include "rts/asset_3d_format.h"

#include <stdio.h>
#include <string.h>

namespace {

int g_failures = 0;

void Expect_Format(const char *case_name, rts::Asset3DFormat expected, rts::Asset3DFormat actual)
{
    if (expected != actual) {
        fprintf(stderr, "%s: expected %s, got %s\n", case_name,
            rts::Get_3D_Asset_Format_Name(expected), rts::Get_3D_Asset_Format_Name(actual));
        ++g_failures;
    }
}

void Expect_Bool(const char *case_name, bool expected, bool actual)
{
    if (expected != actual) {
        fprintf(stderr, "%s: expected %s, got %s\n", case_name,
            expected ? "true" : "false", actual ? "true" : "false");
        ++g_failures;
    }
}

} // namespace

int main()
{
    Expect_Format("lowercase W3D", rts::ASSET_3D_FORMAT_W3D,
        rts::Detect_3D_Asset_Format_From_Path("art/tank.w3d"));
    Expect_Format("uppercase W3D", rts::ASSET_3D_FORMAT_W3D,
        rts::Detect_3D_Asset_Format_From_Path("ART\\TANK.W3D"));
    Expect_Format("lowercase W3X", rts::ASSET_3D_FORMAT_W3X,
        rts::Detect_3D_Asset_Format_From_Path("art/tank.w3x"));
    Expect_Format("uppercase W3X", rts::ASSET_3D_FORMAT_W3X,
        rts::Detect_3D_Asset_Format_From_Path("ART\\TANK.W3X"));
    Expect_Format("dot in directory", rts::ASSET_3D_FORMAT_W3X,
        rts::Detect_3D_Asset_Format_From_Path("mods/v1.0/art/tank.w3x"));
    Expect_Format("compound suffix", rts::ASSET_3D_FORMAT_UNKNOWN,
        rts::Detect_3D_Asset_Format_From_Path("art/tank.w3x.backup"));
    Expect_Format("missing extension", rts::ASSET_3D_FORMAT_UNKNOWN,
        rts::Detect_3D_Asset_Format_From_Path("art/tank"));
    Expect_Format("null path", rts::ASSET_3D_FORMAT_UNKNOWN,
        rts::Detect_3D_Asset_Format_From_Path(0));

    const char xml[] = "<?xml version=\"1.0\"?><AssetDeclaration/>";
    const char xml_with_space[] = " \r\n\t<AssetDeclaration/>";
    const unsigned char xml_with_bom[] = { 0xEF, 0xBB, 0xBF, ' ', '<', 'A', '/', '>' };
    const char binary[] = { 'H', 'M', '3', 'W', 0, 1, 2, 3 };

    Expect_Bool("XML declaration", true, rts::Looks_Like_XML_Document(xml, strlen(xml)));
    Expect_Bool("XML leading whitespace", true,
        rts::Looks_Like_XML_Document(xml_with_space, strlen(xml_with_space)));
    Expect_Bool("XML UTF-8 BOM", true,
        rts::Looks_Like_XML_Document(xml_with_bom, sizeof(xml_with_bom)));
    Expect_Bool("binary data", false, rts::Looks_Like_XML_Document(binary, sizeof(binary)));

    Expect_Format("SAGE W3X XML", rts::ASSET_3D_FORMAT_W3X,
        rts::Detect_3D_Asset_Format("art/tank.w3x", xml, strlen(xml)));
    Expect_Format("non-XML .w3x", rts::ASSET_3D_FORMAT_UNKNOWN,
        rts::Detect_3D_Asset_Format("map.w3x", binary, sizeof(binary)));
    Expect_Format("W3D does not require XML", rts::ASSET_3D_FORMAT_W3D,
        rts::Detect_3D_Asset_Format("art/tank.w3d", binary, sizeof(binary)));

    if (g_failures != 0) {
        fprintf(stderr, "%d W3X asset-format test(s) failed.\n", g_failures);
        return 1;
    }

    puts("W3X asset-format recognition tests passed.");
    return 0;
}
