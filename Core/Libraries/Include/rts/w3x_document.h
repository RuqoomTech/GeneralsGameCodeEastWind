#ifndef RTS_W3X_DOCUMENT_H
#define RTS_W3X_DOCUMENT_H

#include <stddef.h>

namespace rts {

// Fixed-size summaries keep this pre-runtime parsing seam allocation-free and C++98-compatible.
static const size_t W3X_ROOT_NAME_CAPACITY = 96;
static const size_t W3X_CHILD_NAME_CAPACITY = W3X_ROOT_NAME_CAPACITY;
static const size_t W3X_NAMESPACE_CAPACITY = 192;
static const size_t W3X_CHILD_NAMESPACE_CAPACITY = W3X_NAMESPACE_CAPACITY;
static const size_t W3X_DISCOVERY_MAX_NESTING_DEPTH = 64;

extern const char W3X_SAGE_ASSET_NAMESPACE[];

enum W3XDocumentProbeResult
{
    W3X_DOCUMENT_PROBE_OK = 0,
    W3X_DOCUMENT_PROBE_EMPTY,
    W3X_DOCUMENT_PROBE_NOT_XML,
    W3X_DOCUMENT_PROBE_MALFORMED,
    W3X_DOCUMENT_PROBE_UNSUPPORTED_DOCTYPE,
    W3X_DOCUMENT_PROBE_NAME_TOO_LONG,
    W3X_DOCUMENT_PROBE_NAMESPACE_TOO_LONG
};

struct W3XDocumentInfo
{
    W3XDocumentInfo();

    bool has_xml_declaration;
    bool is_sage_asset_declaration;
    char root_qualified_name[W3X_ROOT_NAME_CAPACITY];
    char root_local_name[W3X_ROOT_NAME_CAPACITY];
    char root_namespace[W3X_NAMESPACE_CAPACITY];
};

enum W3XTopLevelChildKind
{
    W3X_TOP_LEVEL_CHILD_UNKNOWN = 0,
    W3X_TOP_LEVEL_CHILD_W3D_MESH,
    W3X_TOP_LEVEL_CHILD_W3D_HIERARCHY,
    W3X_TOP_LEVEL_CHILD_W3D_CONTAINER,
    W3X_TOP_LEVEL_CHILD_W3D_ANIMATION,
    W3X_TOP_LEVEL_CHILD_W3D_COLLISION_BOX
};

enum W3XTopLevelChildDiscoveryResult
{
    W3X_TOP_LEVEL_DISCOVERY_OK = 0,
    W3X_TOP_LEVEL_DISCOVERY_INVALID_ARGUMENT,
    W3X_TOP_LEVEL_DISCOVERY_DOCUMENT_INVALID,
    W3X_TOP_LEVEL_DISCOVERY_NOT_SAGE_ASSET_DECLARATION,
    W3X_TOP_LEVEL_DISCOVERY_MALFORMED,
    W3X_TOP_LEVEL_DISCOVERY_UNSUPPORTED_DOCTYPE,
    W3X_TOP_LEVEL_DISCOVERY_NAME_TOO_LONG,
    W3X_TOP_LEVEL_DISCOVERY_NAMESPACE_TOO_LONG,
    W3X_TOP_LEVEL_DISCOVERY_NESTING_TOO_DEEP,
    W3X_TOP_LEVEL_DISCOVERY_OUTPUT_TOO_SMALL
};

struct W3XTopLevelChildInfo
{
    W3XTopLevelChildInfo();

    W3XTopLevelChildKind kind;
    char qualified_name[W3X_CHILD_NAME_CAPACITY];
    char local_name[W3X_CHILD_NAME_CAPACITY];
    char namespace_uri[W3X_CHILD_NAMESPACE_CAPACITY];
};

// A1 API: inspect only the document envelope/root element.
W3XDocumentProbeResult Probe_W3X_Document(
    const void *document_data, size_t document_size, W3XDocumentInfo &info);

const char *Get_W3X_Document_Probe_Result_Name(W3XDocumentProbeResult result);

// A2 API: validate nesting and report direct AssetDeclaration child elements without
// decoding their contents. Passing children == 0 and child_capacity == 0 performs a count-only scan.
W3XTopLevelChildDiscoveryResult Discover_W3X_Top_Level_Children(
    const void *document_data, size_t document_size,
    W3XTopLevelChildInfo *children, size_t child_capacity, size_t &child_count);

const char *Get_W3X_Top_Level_Child_Kind_Name(W3XTopLevelChildKind kind);
const char *Get_W3X_Top_Level_Discovery_Result_Name(W3XTopLevelChildDiscoveryResult result);

} // namespace rts

#endif // RTS_W3X_DOCUMENT_H
