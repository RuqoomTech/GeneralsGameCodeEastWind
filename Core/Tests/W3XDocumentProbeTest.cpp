#include "rts/w3x_document_probe.h"

#include <stdio.h>
#include <string.h>

namespace {

int g_failures = 0;

void Expect_Result(const char *case_name, rts::W3XDocumentProbeResult expected, rts::W3XDocumentProbeResult actual)
{
    if (expected != actual) {
        fprintf(stderr, "%s: expected %s, got %s\n", case_name,
            rts::Get_W3X_Document_Probe_Result_Name(expected),
            rts::Get_W3X_Document_Probe_Result_Name(actual));
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

void Expect_Bool(const char *case_name, bool expected, bool actual)
{
    if (expected != actual) {
        fprintf(stderr, "%s: expected %s, got %s\n", case_name,
            expected ? "true" : "false", actual ? "true" : "false");
        ++g_failures;
    }
}

void Probe_And_Expect_Sage(const char *case_name, const char *xml, bool has_declaration)
{
    rts::W3XDocumentInfo info;
    const rts::W3XDocumentProbeResult result = rts::Probe_W3X_Document(xml, strlen(xml), info);

    Expect_Result(case_name, rts::W3X_DOCUMENT_PROBE_OK, result);
    if (result != rts::W3X_DOCUMENT_PROBE_OK) {
        return;
    }

    Expect_String("SAGE root qualified name", "AssetDeclaration", info.root_qualified_name);
    Expect_String("SAGE root local name", "AssetDeclaration", info.root_local_name);
    Expect_String("SAGE namespace", rts::W3X_SAGE_ASSET_NAMESPACE, info.root_namespace);
    Expect_Bool("SAGE XML declaration", has_declaration, info.has_xml_declaration);
    Expect_Bool("SAGE classification", true, info.is_sage_asset_declaration);
}

} // namespace

int main()
{
    const char canonical[] =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<AssetDeclaration xmlns=\"uri:ea.com:eala:asset\">\n"
        "  <W3DMesh id=\"TEST.Mesh\" GeometryType=\"Normal\"/>\n"
        "</AssetDeclaration>";
    Probe_And_Expect_Sage("canonical SAGE envelope", canonical, true);

    const char no_declaration[] =
        "<AssetDeclaration xmlns='uri:ea.com:eala:asset'><W3DHierarchy/></AssetDeclaration>";
    Probe_And_Expect_Sage("no XML declaration", no_declaration, false);

    const unsigned char bom_and_comment[] = {
        0xEF, 0xBB, 0xBF,
        '<','?','x','m','l',' ','v','e','r','s','i','o','n','=','\'','1','.','0','\'','?','>',
        '\n','<','!','-','-',' ','f','i','x','t','u','r','e',' ','-','-','>', '\n',
        '<','A','s','s','e','t','D','e','c','l','a','r','a','t','i','o','n',' ',
        'x','m','l','n','s','=','\'','u','r','i',':','e','a','.','c','o','m',':','e','a','l','a',':','a','s','s','e','t','\'',
        '/','>'
    };
    rts::W3XDocumentInfo info;
    rts::W3XDocumentProbeResult result = rts::Probe_W3X_Document(bom_and_comment, sizeof(bom_and_comment), info);
    Expect_Result("BOM and comment", rts::W3X_DOCUMENT_PROBE_OK, result);
    if (result == rts::W3X_DOCUMENT_PROBE_OK) {
        Expect_Bool("BOM SAGE classification", true, info.is_sage_asset_declaration);
        Expect_Bool("BOM XML declaration", true, info.has_xml_declaration);
    }

    const char prefixed[] =
        "<ea:AssetDeclaration xmlns:ea=\"uri:ea.com:eala:asset\"><ea:W3DContainer/></ea:AssetDeclaration>";
    result = rts::Probe_W3X_Document(prefixed, strlen(prefixed), info);
    Expect_Result("prefixed root", rts::W3X_DOCUMENT_PROBE_OK, result);
    if (result == rts::W3X_DOCUMENT_PROBE_OK) {
        Expect_String("prefixed qualified name", "ea:AssetDeclaration", info.root_qualified_name);
        Expect_String("prefixed local name", "AssetDeclaration", info.root_local_name);
        Expect_String("prefixed namespace", rts::W3X_SAGE_ASSET_NAMESPACE, info.root_namespace);
        Expect_Bool("prefixed SAGE classification", true, info.is_sage_asset_declaration);
    }

    const char wrong_namespace[] =
        "<AssetDeclaration xmlns=\"urn:not-ea\"><W3DMesh/></AssetDeclaration>";
    result = rts::Probe_W3X_Document(wrong_namespace, strlen(wrong_namespace), info);
    Expect_Result("wrong namespace parses", rts::W3X_DOCUMENT_PROBE_OK, result);
    if (result == rts::W3X_DOCUMENT_PROBE_OK) {
        Expect_Bool("wrong namespace not SAGE", false, info.is_sage_asset_declaration);
    }

    const char other_root[] = "<W3DMesh id=\"Standalone\"/>";
    result = rts::Probe_W3X_Document(other_root, strlen(other_root), info);
    Expect_Result("other XML root parses", rts::W3X_DOCUMENT_PROBE_OK, result);
    if (result == rts::W3X_DOCUMENT_PROBE_OK) {
        Expect_String("other root name", "W3DMesh", info.root_local_name);
        Expect_Bool("other root not envelope", false, info.is_sage_asset_declaration);
    }

    const char processing_instruction[] =
        "<?tool test?>\n<AssetDeclaration xmlns=\"uri:ea.com:eala:asset\"/>";
    result = rts::Probe_W3X_Document(processing_instruction, strlen(processing_instruction), info);
    Expect_Result("processing instruction", rts::W3X_DOCUMENT_PROBE_OK, result);
    if (result == rts::W3X_DOCUMENT_PROBE_OK) {
        Expect_Bool("processing instruction no XML declaration", false, info.has_xml_declaration);
        Expect_Bool("processing instruction SAGE", true, info.is_sage_asset_declaration);
    }

    const char binary[] = { 'W', '3', 'X', 0, 1, 2, 3 };
    Expect_Result("binary data", rts::W3X_DOCUMENT_PROBE_NOT_XML,
        rts::Probe_W3X_Document(binary, sizeof(binary), info));
    Expect_Result("null data", rts::W3X_DOCUMENT_PROBE_EMPTY,
        rts::Probe_W3X_Document(0, 0, info));

    const char whitespace[] = " \r\n\t ";
    Expect_Result("whitespace only", rts::W3X_DOCUMENT_PROBE_EMPTY,
        rts::Probe_W3X_Document(whitespace, strlen(whitespace), info));

    const char unterminated_comment[] = "<!-- broken";
    Expect_Result("unterminated comment", rts::W3X_DOCUMENT_PROBE_MALFORMED,
        rts::Probe_W3X_Document(unterminated_comment, strlen(unterminated_comment), info));

    const char declaration_only[] = "<?xml version=\"1.0\"?>";
    Expect_Result("declaration without root", rts::W3X_DOCUMENT_PROBE_MALFORMED,
        rts::Probe_W3X_Document(declaration_only, strlen(declaration_only), info));

    const char bad_attribute[] = "<AssetDeclaration xmlns=uri:ea.com:eala:asset/>";
    Expect_Result("unquoted attribute", rts::W3X_DOCUMENT_PROBE_MALFORMED,
        rts::Probe_W3X_Document(bad_attribute, strlen(bad_attribute), info));

    const char doctype[] = "<!DOCTYPE AssetDeclaration><AssetDeclaration/>";
    Expect_Result("DOCTYPE explicitly unsupported", rts::W3X_DOCUMENT_PROBE_UNSUPPORTED_DOCTYPE,
        rts::Probe_W3X_Document(doctype, strlen(doctype), info));

    if (g_failures != 0) {
        fprintf(stderr, "%d W3X document-probe test(s) failed.\n", g_failures);
        return 1;
    }

    puts("W3X document-probe tests passed.");
    return 0;
}
