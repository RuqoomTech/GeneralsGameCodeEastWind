#include "rts/w3x_document.h"

#include <string.h>

namespace rts {

const char W3X_SAGE_ASSET_NAMESPACE[] = "uri:ea.com:eala:asset";

W3XDocumentInfo::W3XDocumentInfo()
    : has_xml_declaration(false), is_sage_asset_declaration(false)
{
    root_qualified_name[0] = '\0';
    root_local_name[0] = '\0';
    root_namespace[0] = '\0';
}

W3XTopLevelChildInfo::W3XTopLevelChildInfo()
    : kind(W3X_TOP_LEVEL_CHILD_UNKNOWN)
{
    qualified_name[0] = '\0';
    local_name[0] = '\0';
    namespace_uri[0] = '\0';
}

namespace {

enum ParseResult
{
    PARSE_OK = 0,
    PARSE_EMPTY,
    PARSE_NOT_XML,
    PARSE_MALFORMED,
    PARSE_UNSUPPORTED_DOCTYPE,
    PARSE_NAME_TOO_LONG,
    PARSE_NAMESPACE_TOO_LONG,
    PARSE_NESTING_TOO_DEEP
};

struct QName
{
    QName()
        : name_begin(0), name_end(0), local_begin(0), prefix_begin(0), prefix_end(0), has_prefix(false)
    {
    }

    size_t name_begin;
    size_t name_end;
    size_t local_begin;
    size_t prefix_begin;
    size_t prefix_end;
    bool has_prefix;
};

struct Tag
{
    Tag()
        : attributes_begin(0), attributes_end(0), end_offset(0), self_closing(false)
    {
    }

    QName name;
    size_t attributes_begin;
    size_t attributes_end;
    size_t end_offset;
    bool self_closing;
};

bool Is_XML_Whitespace(unsigned char value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

bool Is_Name_Start(unsigned char value)
{
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || value == '_' || value == ':';
}

bool Is_Name_Char(unsigned char value)
{
    return Is_Name_Start(value) || (value >= '0' && value <= '9') || value == '-' || value == '.';
}

bool Bytes_Equal(const unsigned char *data, size_t size, size_t offset, const char *text)
{
    if (data == 0 || text == 0 || offset > size) {
        return false;
    }

    const size_t text_size = strlen(text);
    return text_size <= size - offset && memcmp(data + offset, text, text_size) == 0;
}

void Skip_Whitespace(const unsigned char *data, size_t size, size_t &offset)
{
    while (offset < size && Is_XML_Whitespace(data[offset])) {
        ++offset;
    }
}

bool Find_Terminator(const unsigned char *data, size_t size, size_t &offset, const char *terminator)
{
    const size_t terminator_size = strlen(terminator);

    while (offset <= size) {
        if (terminator_size <= size - offset
            && memcmp(data + offset, terminator, terminator_size) == 0) {
            offset += terminator_size;
            return true;
        }
        if (offset == size) {
            break;
        }
        ++offset;
    }
    return false;
}

bool Range_Equals_Text(
    const unsigned char *data, size_t begin, size_t end, const char *text)
{
    if (text == 0) {
        return false;
    }
    const size_t text_size = strlen(text);
    return text_size == end - begin && memcmp(data + begin, text, text_size) == 0;
}

bool Ranges_Equal(
    const unsigned char *data, size_t a_begin, size_t a_end, size_t b_begin, size_t b_end)
{
    const size_t length = a_end - a_begin;
    return length == b_end - b_begin && memcmp(data + a_begin, data + b_begin, length) == 0;
}

ParseResult Copy_Range(
    const unsigned char *data, size_t begin, size_t end,
    char *output, size_t output_capacity, ParseResult too_long)
{
    const size_t length = end - begin;
    if (length + 1 > output_capacity) {
        return too_long;
    }
    if (length != 0) {
        memcpy(output, data + begin, length);
    }
    output[length] = '\0';
    return PARSE_OK;
}

ParseResult Split_QName(const unsigned char *data, size_t begin, size_t end, QName &name)
{
    name = QName();
    name.name_begin = begin;
    name.name_end = end;
    name.local_begin = begin;
    name.prefix_begin = begin;
    name.prefix_end = begin;

    for (size_t cursor = begin; cursor < end; ++cursor) {
        if (data[cursor] == ':') {
            if (name.has_prefix || cursor == begin || cursor + 1 == end) {
                return PARSE_MALFORMED;
            }
            name.has_prefix = true;
            name.prefix_end = cursor;
            name.local_begin = cursor + 1;
        }
    }
    return PARSE_OK;
}

ParseResult Parse_Start_Tag(
    const unsigned char *data, size_t size, size_t begin, Tag &tag, bool strict_attributes)
{
    tag = Tag();
    if (begin >= size || data[begin] != '<') {
        return PARSE_MALFORMED;
    }

    size_t offset = begin + 1;
    if (offset >= size || data[offset] == '/' || data[offset] == '!' || data[offset] == '?'
        || !Is_Name_Start(data[offset])) {
        return PARSE_MALFORMED;
    }

    const size_t name_begin = offset;
    while (offset < size && Is_Name_Char(data[offset])) {
        ++offset;
    }
    ParseResult result = Split_QName(data, name_begin, offset, tag.name);
    if (result != PARSE_OK) {
        return result;
    }
    tag.attributes_begin = offset;

    for (;;) {
        Skip_Whitespace(data, size, offset);
        if (offset >= size) {
            return PARSE_MALFORMED;
        }
        if (data[offset] == '>') {
            tag.attributes_end = offset;
            tag.end_offset = offset + 1;
            return PARSE_OK;
        }
        if (data[offset] == '/') {
            tag.attributes_end = offset;
            ++offset;
            if (offset >= size || data[offset] != '>') {
                return PARSE_MALFORMED;
            }
            tag.self_closing = true;
            tag.end_offset = offset + 1;
            return PARSE_OK;
        }
        if (!Is_Name_Start(data[offset])) {
            return PARSE_MALFORMED;
        }

        const size_t attribute_begin = offset;
        while (offset < size && Is_Name_Char(data[offset])) {
            ++offset;
        }
        if (strict_attributes) {
            QName attribute_name;
            result = Split_QName(data, attribute_begin, offset, attribute_name);
            if (result != PARSE_OK) {
                return result;
            }
        }

        Skip_Whitespace(data, size, offset);
        if (offset >= size || data[offset] != '=') {
            return PARSE_MALFORMED;
        }
        ++offset;
        Skip_Whitespace(data, size, offset);
        if (offset >= size || (data[offset] != '"' && data[offset] != '\'')) {
            return PARSE_MALFORMED;
        }

        const unsigned char quote = data[offset++];
        while (offset < size && data[offset] != quote) {
            if (strict_attributes && data[offset] == '<') {
                return PARSE_MALFORMED;
            }
            ++offset;
        }
        if (offset >= size) {
            return PARSE_MALFORMED;
        }
        ++offset;
    }
}

ParseResult Parse_End_Tag(
    const unsigned char *data, size_t size, size_t begin,
    size_t &name_begin, size_t &name_end, size_t &end_offset)
{
    if (begin + 1 >= size || data[begin] != '<' || data[begin + 1] != '/') {
        return PARSE_MALFORMED;
    }

    size_t offset = begin + 2;
    if (offset >= size || !Is_Name_Start(data[offset])) {
        return PARSE_MALFORMED;
    }
    name_begin = offset;
    while (offset < size && Is_Name_Char(data[offset])) {
        ++offset;
    }
    name_end = offset;

    QName name;
    ParseResult result = Split_QName(data, name_begin, name_end, name);
    if (result != PARSE_OK) {
        return result;
    }

    Skip_Whitespace(data, size, offset);
    if (offset >= size || data[offset] != '>') {
        return PARSE_MALFORMED;
    }
    end_offset = offset + 1;
    return PARSE_OK;
}

ParseResult Skip_Terminated(
    const unsigned char *data, size_t size, size_t &offset,
    size_t prefix_size, const char *terminator)
{
    offset += prefix_size;
    return Find_Terminator(data, size, offset, terminator) ? PARSE_OK : PARSE_MALFORMED;
}

ParseResult Find_Root(
    const unsigned char *data, size_t size, size_t &root_begin, bool &has_xml_declaration)
{
    has_xml_declaration = false;
    size_t offset = 0;

    if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        offset = 3;
    }

    Skip_Whitespace(data, size, offset);
    if (offset >= size) {
        return PARSE_EMPTY;
    }
    if (data[offset] != '<') {
        return PARSE_NOT_XML;
    }

    for (;;) {
        Skip_Whitespace(data, size, offset);
        if (offset >= size) {
            return PARSE_MALFORMED;
        }
        if (Bytes_Equal(data, size, offset, "<!--")) {
            ParseResult result = Skip_Terminated(data, size, offset, 4, "-->");
            if (result != PARSE_OK) {
                return result;
            }
            continue;
        }
        if (Bytes_Equal(data, size, offset, "<!DOCTYPE")) {
            return PARSE_UNSUPPORTED_DOCTYPE;
        }
        if (Bytes_Equal(data, size, offset, "<?")) {
            const size_t instruction_begin = offset;
            ParseResult result = Skip_Terminated(data, size, offset, 2, "?>");
            if (result != PARSE_OK) {
                return result;
            }
            if (Bytes_Equal(data, size, instruction_begin, "<?xml")) {
                const size_t after_xml = instruction_begin + 5;
                if (after_xml < size && (Is_XML_Whitespace(data[after_xml]) || data[after_xml] == '?')) {
                    has_xml_declaration = true;
                }
            }
            continue;
        }
        break;
    }

    root_begin = offset;
    return PARSE_OK;
}

bool Is_Namespace_Binding(
    const unsigned char *data, size_t name_begin, size_t name_end, const QName &element_name)
{
    if (!element_name.has_prefix) {
        return Range_Equals_Text(data, name_begin, name_end, "xmlns");
    }

    const size_t prefix_length = element_name.prefix_end - element_name.prefix_begin;
    return name_end - name_begin == 6 + prefix_length
        && memcmp(data + name_begin, "xmlns:", 6) == 0
        && memcmp(data + name_begin + 6, data + element_name.prefix_begin, prefix_length) == 0;
}

ParseResult Find_Namespace_In_Tag(
    const unsigned char *data, const Tag &scope, const QName &element_name,
    char *namespace_uri, size_t capacity, bool &found)
{
    found = false;
    size_t offset = scope.attributes_begin;

    while (offset < scope.attributes_end) {
        Skip_Whitespace(data, scope.attributes_end, offset);
        if (offset >= scope.attributes_end) {
            break;
        }

        const size_t name_begin = offset;
        while (offset < scope.attributes_end && Is_Name_Char(data[offset])) {
            ++offset;
        }
        const size_t name_end = offset;
        if (name_begin == name_end) {
            return PARSE_MALFORMED;
        }

        Skip_Whitespace(data, scope.attributes_end, offset);
        if (offset >= scope.attributes_end || data[offset] != '=') {
            return PARSE_MALFORMED;
        }
        ++offset;
        Skip_Whitespace(data, scope.attributes_end, offset);
        if (offset >= scope.attributes_end || (data[offset] != '"' && data[offset] != '\'')) {
            return PARSE_MALFORMED;
        }

        const unsigned char quote = data[offset++];
        const size_t value_begin = offset;
        while (offset < scope.attributes_end && data[offset] != quote) {
            ++offset;
        }
        if (offset >= scope.attributes_end) {
            return PARSE_MALFORMED;
        }
        const size_t value_end = offset++;

        if (Is_Namespace_Binding(data, name_begin, name_end, element_name)) {
            ParseResult result = Copy_Range(
                data, value_begin, value_end, namespace_uri, capacity, PARSE_NAMESPACE_TOO_LONG);
            if (result != PARSE_OK) {
                return result;
            }
            found = true;
            // Continue deliberately: A1 historically used the last matching root binding.
        }
    }
    return PARSE_OK;
}

ParseResult Resolve_Root_Namespace(
    const unsigned char *data, const Tag &root, char *namespace_uri, size_t capacity)
{
    namespace_uri[0] = '\0';
    bool found = false;
    return Find_Namespace_In_Tag(data, root, root.name, namespace_uri, capacity, found);
}

ParseResult Resolve_Child_Namespace(
    const unsigned char *data, const Tag &root, const Tag &child,
    char *namespace_uri, size_t capacity)
{
    namespace_uri[0] = '\0';
    bool found = false;

    ParseResult result = Find_Namespace_In_Tag(data, child, child.name, namespace_uri, capacity, found);
    if (result != PARSE_OK || found) {
        return result;
    }

    result = Find_Namespace_In_Tag(data, root, child.name, namespace_uri, capacity, found);
    if (result != PARSE_OK) {
        return result;
    }
    if (!found && child.name.has_prefix) {
        return PARSE_MALFORMED;
    }
    return PARSE_OK;
}

ParseResult Parse_Envelope(
    const unsigned char *data, size_t size, bool strict_attributes,
    W3XDocumentInfo &info, Tag &root)
{
    info = W3XDocumentInfo();

    size_t root_begin = 0;
    bool has_xml_declaration = false;
    ParseResult result = Find_Root(data, size, root_begin, has_xml_declaration);
    if (result != PARSE_OK) {
        return result;
    }

    result = Parse_Start_Tag(data, size, root_begin, root, strict_attributes);
    if (result != PARSE_OK) {
        return result;
    }

    result = Copy_Range(data, root.name.name_begin, root.name.name_end,
        info.root_qualified_name, sizeof(info.root_qualified_name), PARSE_NAME_TOO_LONG);
    if (result != PARSE_OK) {
        return result;
    }
    result = Copy_Range(data, root.name.local_begin, root.name.name_end,
        info.root_local_name, sizeof(info.root_local_name), PARSE_NAME_TOO_LONG);
    if (result != PARSE_OK) {
        return result;
    }
    result = Resolve_Root_Namespace(data, root, info.root_namespace, sizeof(info.root_namespace));
    if (result != PARSE_OK) {
        return result;
    }

    info.has_xml_declaration = has_xml_declaration;
    info.is_sage_asset_declaration = strcmp(info.root_local_name, "AssetDeclaration") == 0
        && strcmp(info.root_namespace, W3X_SAGE_ASSET_NAMESPACE) == 0;
    return PARSE_OK;
}

ParseResult Validate_Trailing(const unsigned char *data, size_t size, size_t offset)
{
    while (offset < size) {
        Skip_Whitespace(data, size, offset);
        if (offset >= size) {
            return PARSE_OK;
        }
        if (Bytes_Equal(data, size, offset, "<!--")) {
            ParseResult result = Skip_Terminated(data, size, offset, 4, "-->");
            if (result != PARSE_OK) {
                return result;
            }
            continue;
        }
        if (Bytes_Equal(data, size, offset, "<?")) {
            ParseResult result = Skip_Terminated(data, size, offset, 2, "?>");
            if (result != PARSE_OK) {
                return result;
            }
            continue;
        }
        return PARSE_MALFORMED;
    }
    return PARSE_OK;
}

W3XTopLevelChildKind Classify_Top_Level_Child(const char *local_name, const char *namespace_uri)
{
    if (strcmp(namespace_uri, W3X_SAGE_ASSET_NAMESPACE) != 0) {
        return W3X_TOP_LEVEL_CHILD_UNKNOWN;
    }
    if (strcmp(local_name, "W3DMesh") == 0) {
        return W3X_TOP_LEVEL_CHILD_W3D_MESH;
    }
    if (strcmp(local_name, "W3DHierarchy") == 0) {
        return W3X_TOP_LEVEL_CHILD_W3D_HIERARCHY;
    }
    if (strcmp(local_name, "W3DContainer") == 0) {
        return W3X_TOP_LEVEL_CHILD_W3D_CONTAINER;
    }
    if (strcmp(local_name, "W3DAnimation") == 0) {
        return W3X_TOP_LEVEL_CHILD_W3D_ANIMATION;
    }
    if (strcmp(local_name, "W3DCollisionBox") == 0) {
        return W3X_TOP_LEVEL_CHILD_W3D_COLLISION_BOX;
    }
    return W3X_TOP_LEVEL_CHILD_UNKNOWN;
}

W3XDocumentProbeResult To_Probe_Result(ParseResult result)
{
    switch (result) {
        case PARSE_OK:
            return W3X_DOCUMENT_PROBE_OK;
        case PARSE_EMPTY:
            return W3X_DOCUMENT_PROBE_EMPTY;
        case PARSE_NOT_XML:
            return W3X_DOCUMENT_PROBE_NOT_XML;
        case PARSE_UNSUPPORTED_DOCTYPE:
            return W3X_DOCUMENT_PROBE_UNSUPPORTED_DOCTYPE;
        case PARSE_NAME_TOO_LONG:
            return W3X_DOCUMENT_PROBE_NAME_TOO_LONG;
        case PARSE_NAMESPACE_TOO_LONG:
            return W3X_DOCUMENT_PROBE_NAMESPACE_TOO_LONG;
        default:
            return W3X_DOCUMENT_PROBE_MALFORMED;
    }
}

W3XTopLevelChildDiscoveryResult To_Discovery_Result(ParseResult result)
{
    switch (result) {
        case PARSE_OK:
            return W3X_TOP_LEVEL_DISCOVERY_OK;
        case PARSE_UNSUPPORTED_DOCTYPE:
            return W3X_TOP_LEVEL_DISCOVERY_UNSUPPORTED_DOCTYPE;
        case PARSE_NAME_TOO_LONG:
            return W3X_TOP_LEVEL_DISCOVERY_NAME_TOO_LONG;
        case PARSE_NAMESPACE_TOO_LONG:
            return W3X_TOP_LEVEL_DISCOVERY_NAMESPACE_TOO_LONG;
        case PARSE_NESTING_TOO_DEEP:
            return W3X_TOP_LEVEL_DISCOVERY_NESTING_TOO_DEEP;
        case PARSE_MALFORMED:
            return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
        default:
            return W3X_TOP_LEVEL_DISCOVERY_DOCUMENT_INVALID;
    }
}

ParseResult Summarize_Child(
    const unsigned char *data, const Tag &root, const Tag &child, W3XTopLevelChildInfo &summary)
{
    ParseResult result = Copy_Range(data, child.name.name_begin, child.name.name_end,
        summary.qualified_name, sizeof(summary.qualified_name), PARSE_NAME_TOO_LONG);
    if (result != PARSE_OK) {
        return result;
    }
    result = Copy_Range(data, child.name.local_begin, child.name.name_end,
        summary.local_name, sizeof(summary.local_name), PARSE_NAME_TOO_LONG);
    if (result != PARSE_OK) {
        return result;
    }
    result = Resolve_Child_Namespace(data, root, child,
        summary.namespace_uri, sizeof(summary.namespace_uri));
    if (result != PARSE_OK) {
        return result;
    }
    summary.kind = Classify_Top_Level_Child(summary.local_name, summary.namespace_uri);
    return PARSE_OK;
}

ParseResult Scan_Top_Level_Children(
    const unsigned char *data, size_t size, const Tag &root,
    W3XTopLevelChildInfo *children, size_t child_capacity, size_t &child_count)
{
    if (root.self_closing) {
        return Validate_Trailing(data, size, root.end_offset);
    }

    size_t name_begin_stack[W3X_DISCOVERY_MAX_NESTING_DEPTH];
    size_t name_end_stack[W3X_DISCOVERY_MAX_NESTING_DEPTH];
    size_t depth = 1;
    name_begin_stack[0] = root.name.name_begin;
    name_end_stack[0] = root.name.name_end;
    size_t offset = root.end_offset;

    while (offset < size && depth != 0) {
        if (data[offset] != '<') {
            ++offset;
            continue;
        }
        if (Bytes_Equal(data, size, offset, "<!--")) {
            ParseResult result = Skip_Terminated(data, size, offset, 4, "-->");
            if (result != PARSE_OK) {
                return result;
            }
            continue;
        }
        if (Bytes_Equal(data, size, offset, "<?")) {
            ParseResult result = Skip_Terminated(data, size, offset, 2, "?>");
            if (result != PARSE_OK) {
                return result;
            }
            continue;
        }
        if (Bytes_Equal(data, size, offset, "<![CDATA[")) {
            ParseResult result = Skip_Terminated(data, size, offset, 9, "]]>");
            if (result != PARSE_OK) {
                return result;
            }
            continue;
        }
        if (Bytes_Equal(data, size, offset, "<!DOCTYPE")) {
            return PARSE_UNSUPPORTED_DOCTYPE;
        }
        if (Bytes_Equal(data, size, offset, "<!")) {
            return PARSE_MALFORMED;
        }

        if (Bytes_Equal(data, size, offset, "</")) {
            size_t close_begin = 0;
            size_t close_end = 0;
            size_t tag_end = 0;
            ParseResult result = Parse_End_Tag(data, size, offset, close_begin, close_end, tag_end);
            if (result != PARSE_OK) {
                return result;
            }
            if (!Ranges_Equal(data,
                    name_begin_stack[depth - 1], name_end_stack[depth - 1], close_begin, close_end)) {
                return PARSE_MALFORMED;
            }
            --depth;
            offset = tag_end;
            continue;
        }

        Tag tag;
        ParseResult result = Parse_Start_Tag(data, size, offset, tag, true);
        if (result != PARSE_OK) {
            return result;
        }

        if (depth == 1) {
            W3XTopLevelChildInfo summary;
            result = Summarize_Child(data, root, tag, summary);
            if (result != PARSE_OK) {
                return result;
            }
            if (children != 0 && child_count < child_capacity) {
                children[child_count] = summary;
            }
            ++child_count;
        }

        offset = tag.end_offset;
        if (!tag.self_closing) {
            if (depth >= W3X_DISCOVERY_MAX_NESTING_DEPTH) {
                return PARSE_NESTING_TOO_DEEP;
            }
            name_begin_stack[depth] = tag.name.name_begin;
            name_end_stack[depth] = tag.name.name_end;
            ++depth;
        }
    }

    if (depth != 0) {
        return PARSE_MALFORMED;
    }
    return Validate_Trailing(data, size, offset);
}

} // namespace

W3XDocumentProbeResult Probe_W3X_Document(
    const void *document_data, size_t document_size, W3XDocumentInfo &info)
{
    info = W3XDocumentInfo();
    if (document_data == 0 || document_size == 0) {
        return W3X_DOCUMENT_PROBE_EMPTY;
    }

    const unsigned char *data = static_cast<const unsigned char *>(document_data);
    Tag root;
    const ParseResult result = Parse_Envelope(data, document_size, false, info, root);
    return To_Probe_Result(result);
}

W3XTopLevelChildDiscoveryResult Discover_W3X_Top_Level_Children(
    const void *document_data, size_t document_size,
    W3XTopLevelChildInfo *children, size_t child_capacity, size_t &child_count)
{
    child_count = 0;
    if (children == 0 && child_capacity != 0) {
        return W3X_TOP_LEVEL_DISCOVERY_INVALID_ARGUMENT;
    }
    if (document_data == 0 || document_size == 0) {
        return W3X_TOP_LEVEL_DISCOVERY_DOCUMENT_INVALID;
    }

    const unsigned char *data = static_cast<const unsigned char *>(document_data);
    W3XDocumentInfo info;
    Tag root;
    ParseResult result = Parse_Envelope(data, document_size, true, info, root);
    if (result != PARSE_OK) {
        return To_Discovery_Result(result);
    }
    if (!info.is_sage_asset_declaration) {
        return W3X_TOP_LEVEL_DISCOVERY_NOT_SAGE_ASSET_DECLARATION;
    }

    result = Scan_Top_Level_Children(data, document_size, root, children, child_capacity, child_count);
    if (result != PARSE_OK) {
        return To_Discovery_Result(result);
    }
    if (children != 0 && child_count > child_capacity) {
        return W3X_TOP_LEVEL_DISCOVERY_OUTPUT_TOO_SMALL;
    }
    return W3X_TOP_LEVEL_DISCOVERY_OK;
}

const char *Get_W3X_Document_Probe_Result_Name(W3XDocumentProbeResult result)
{
    switch (result) {
        case W3X_DOCUMENT_PROBE_OK:
            return "OK";
        case W3X_DOCUMENT_PROBE_EMPTY:
            return "Empty";
        case W3X_DOCUMENT_PROBE_NOT_XML:
            return "NotXML";
        case W3X_DOCUMENT_PROBE_MALFORMED:
            return "Malformed";
        case W3X_DOCUMENT_PROBE_UNSUPPORTED_DOCTYPE:
            return "UnsupportedDoctype";
        case W3X_DOCUMENT_PROBE_NAME_TOO_LONG:
            return "NameTooLong";
        case W3X_DOCUMENT_PROBE_NAMESPACE_TOO_LONG:
            return "NamespaceTooLong";
        default:
            return "Unknown";
    }
}

const char *Get_W3X_Top_Level_Child_Kind_Name(W3XTopLevelChildKind kind)
{
    switch (kind) {
        case W3X_TOP_LEVEL_CHILD_W3D_MESH:
            return "W3DMesh";
        case W3X_TOP_LEVEL_CHILD_W3D_HIERARCHY:
            return "W3DHierarchy";
        case W3X_TOP_LEVEL_CHILD_W3D_CONTAINER:
            return "W3DContainer";
        case W3X_TOP_LEVEL_CHILD_W3D_ANIMATION:
            return "W3DAnimation";
        case W3X_TOP_LEVEL_CHILD_W3D_COLLISION_BOX:
            return "W3DCollisionBox";
        default:
            return "Unknown";
    }
}

const char *Get_W3X_Top_Level_Discovery_Result_Name(W3XTopLevelChildDiscoveryResult result)
{
    switch (result) {
        case W3X_TOP_LEVEL_DISCOVERY_OK:
            return "OK";
        case W3X_TOP_LEVEL_DISCOVERY_INVALID_ARGUMENT:
            return "InvalidArgument";
        case W3X_TOP_LEVEL_DISCOVERY_DOCUMENT_INVALID:
            return "DocumentInvalid";
        case W3X_TOP_LEVEL_DISCOVERY_NOT_SAGE_ASSET_DECLARATION:
            return "NotSageAssetDeclaration";
        case W3X_TOP_LEVEL_DISCOVERY_MALFORMED:
            return "Malformed";
        case W3X_TOP_LEVEL_DISCOVERY_UNSUPPORTED_DOCTYPE:
            return "UnsupportedDoctype";
        case W3X_TOP_LEVEL_DISCOVERY_NAME_TOO_LONG:
            return "NameTooLong";
        case W3X_TOP_LEVEL_DISCOVERY_NAMESPACE_TOO_LONG:
            return "NamespaceTooLong";
        case W3X_TOP_LEVEL_DISCOVERY_NESTING_TOO_DEEP:
            return "NestingTooDeep";
        case W3X_TOP_LEVEL_DISCOVERY_OUTPUT_TOO_SMALL:
            return "OutputTooSmall";
        default:
            return "Unknown";
    }
}

} // namespace rts
