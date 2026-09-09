#ifndef RTS_W3X_CHILD_DISCOVERY_H
#define RTS_W3X_CHILD_DISCOVERY_H

#include "rts/w3x_document_probe.h"

#include <stddef.h>
#include <string.h>

namespace rts {

static const size_t W3X_CHILD_NAME_CAPACITY = W3X_ROOT_NAME_CAPACITY;
static const size_t W3X_CHILD_NAMESPACE_CAPACITY = W3X_NAMESPACE_CAPACITY;
static const size_t W3X_DISCOVERY_MAX_NESTING_DEPTH = 64;

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
    W3XTopLevelChildInfo()
        : kind(W3X_TOP_LEVEL_CHILD_UNKNOWN)
    {
        qualified_name[0] = '\0';
        local_name[0] = '\0';
        namespace_uri[0] = '\0';
    }

    W3XTopLevelChildKind kind;
    char qualified_name[W3X_CHILD_NAME_CAPACITY];
    char local_name[W3X_CHILD_NAME_CAPACITY];
    char namespace_uri[W3X_CHILD_NAMESPACE_CAPACITY];
};

struct W3XDiscoveryTag
{
    W3XDiscoveryTag()
        : name_begin(0), name_end(0), local_begin(0), prefix_begin(0), prefix_end(0),
          attributes_begin(0), attributes_end(0), end_offset(0), has_prefix(false), self_closing(false)
    {
    }

    size_t name_begin;
    size_t name_end;
    size_t local_begin;
    size_t prefix_begin;
    size_t prefix_end;
    size_t attributes_begin;
    size_t attributes_end;
    size_t end_offset;
    bool has_prefix;
    bool self_closing;
};

inline W3XTopLevelChildDiscoveryResult W3X_Discovery_Map_Probe_Result(W3XDocumentProbeResult result)
{
    switch (result) {
        case W3X_DOCUMENT_PROBE_OK:
            return W3X_TOP_LEVEL_DISCOVERY_OK;
        case W3X_DOCUMENT_PROBE_MALFORMED:
            return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
        case W3X_DOCUMENT_PROBE_UNSUPPORTED_DOCTYPE:
            return W3X_TOP_LEVEL_DISCOVERY_UNSUPPORTED_DOCTYPE;
        case W3X_DOCUMENT_PROBE_NAME_TOO_LONG:
            return W3X_TOP_LEVEL_DISCOVERY_NAME_TOO_LONG;
        case W3X_DOCUMENT_PROBE_NAMESPACE_TOO_LONG:
            return W3X_TOP_LEVEL_DISCOVERY_NAMESPACE_TOO_LONG;
        default:
            return W3X_TOP_LEVEL_DISCOVERY_DOCUMENT_INVALID;
    }
}

inline W3XTopLevelChildDiscoveryResult W3X_Discovery_Copy(
    const unsigned char *data, size_t begin, size_t end, char *output, size_t capacity,
    W3XTopLevelChildDiscoveryResult too_long)
{
    const size_t length = end - begin;
    if (length + 1 > capacity) {
        return too_long;
    }
    if (length != 0) {
        memcpy(output, data + begin, length);
    }
    output[length] = '\0';
    return W3X_TOP_LEVEL_DISCOVERY_OK;
}

inline W3XTopLevelChildDiscoveryResult W3X_Discovery_Split_QName(
    const unsigned char *data, size_t begin, size_t end, W3XDiscoveryTag &tag)
{
    tag.name_begin = begin;
    tag.name_end = end;
    tag.local_begin = begin;
    tag.prefix_begin = begin;
    tag.prefix_end = begin;
    tag.has_prefix = false;

    for (size_t cursor = begin; cursor < end; ++cursor) {
        if (data[cursor] == ':') {
            if (tag.has_prefix || cursor == begin || cursor + 1 == end) {
                return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
            }
            tag.has_prefix = true;
            tag.prefix_end = cursor;
            tag.local_begin = cursor + 1;
        }
    }
    return W3X_TOP_LEVEL_DISCOVERY_OK;
}

inline W3XTopLevelChildDiscoveryResult W3X_Discovery_Parse_Start_Tag(
    const unsigned char *data, size_t size, size_t begin, W3XDiscoveryTag &tag)
{
    tag = W3XDiscoveryTag();
    if (begin >= size || data[begin] != '<') {
        return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
    }

    size_t offset = begin + 1;
    if (offset >= size || data[offset] == '/' || data[offset] == '!' || data[offset] == '?'
        || !W3X_Is_Name_Start(data[offset])) {
        return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
    }

    const size_t name_begin = offset;
    while (offset < size && W3X_Is_Name_Char(data[offset])) {
        ++offset;
    }
    W3XTopLevelChildDiscoveryResult result = W3X_Discovery_Split_QName(data, name_begin, offset, tag);
    if (result != W3X_TOP_LEVEL_DISCOVERY_OK) {
        return result;
    }
    tag.attributes_begin = offset;

    for (;;) {
        W3X_Skip_Whitespace(data, size, offset);
        if (offset >= size) {
            return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
        }
        if (data[offset] == '>') {
            tag.attributes_end = offset;
            tag.end_offset = offset + 1;
            return W3X_TOP_LEVEL_DISCOVERY_OK;
        }
        if (data[offset] == '/') {
            tag.attributes_end = offset;
            if (++offset >= size || data[offset] != '>') {
                return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
            }
            tag.self_closing = true;
            tag.end_offset = offset + 1;
            return W3X_TOP_LEVEL_DISCOVERY_OK;
        }
        if (!W3X_Is_Name_Start(data[offset])) {
            return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
        }

        const size_t attribute_begin = offset;
        while (offset < size && W3X_Is_Name_Char(data[offset])) {
            ++offset;
        }
        W3XDiscoveryTag attribute_name;
        result = W3X_Discovery_Split_QName(data, attribute_begin, offset, attribute_name);
        if (result != W3X_TOP_LEVEL_DISCOVERY_OK) {
            return result;
        }

        W3X_Skip_Whitespace(data, size, offset);
        if (offset >= size || data[offset] != '=') {
            return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
        }
        ++offset;
        W3X_Skip_Whitespace(data, size, offset);
        if (offset >= size || (data[offset] != '"' && data[offset] != '\'')) {
            return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
        }

        const unsigned char quote = data[offset++];
        while (offset < size && data[offset] != quote) {
            if (data[offset] == '<') {
                return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
            }
            ++offset;
        }
        if (offset >= size) {
            return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
        }
        ++offset;
    }
}

inline W3XTopLevelChildDiscoveryResult W3X_Discovery_Parse_End_Tag(
    const unsigned char *data, size_t size, size_t begin,
    size_t &name_begin, size_t &name_end, size_t &end_offset)
{
    if (begin + 1 >= size || data[begin] != '<' || data[begin + 1] != '/') {
        return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
    }

    size_t offset = begin + 2;
    if (offset >= size || !W3X_Is_Name_Start(data[offset])) {
        return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
    }
    name_begin = offset;
    while (offset < size && W3X_Is_Name_Char(data[offset])) {
        ++offset;
    }
    name_end = offset;

    W3XDiscoveryTag name;
    W3XTopLevelChildDiscoveryResult result = W3X_Discovery_Split_QName(data, name_begin, name_end, name);
    if (result != W3X_TOP_LEVEL_DISCOVERY_OK) {
        return result;
    }
    W3X_Skip_Whitespace(data, size, offset);
    if (offset >= size || data[offset] != '>') {
        return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
    }
    end_offset = offset + 1;
    return W3X_TOP_LEVEL_DISCOVERY_OK;
}

inline bool W3X_Discovery_Range_Equals(
    const unsigned char *data, size_t a_begin, size_t a_end, size_t b_begin, size_t b_end)
{
    const size_t length = a_end - a_begin;
    return length == b_end - b_begin && memcmp(data + a_begin, data + b_begin, length) == 0;
}

inline bool W3X_Discovery_Is_Namespace_Binding(
    const unsigned char *data, size_t name_begin, size_t name_end, const W3XDiscoveryTag &element)
{
    if (!element.has_prefix) {
        return W3X_String_Equals_Range("xmlns", data, name_begin, name_end);
    }

    const size_t prefix_length = element.prefix_end - element.prefix_begin;
    return name_end - name_begin == 6 + prefix_length
        && memcmp(data + name_begin, "xmlns:", 6) == 0
        && memcmp(data + name_begin + 6, data + element.prefix_begin, prefix_length) == 0;
}

inline W3XTopLevelChildDiscoveryResult W3X_Discovery_Find_Namespace(
    const unsigned char *data, const W3XDiscoveryTag &scope, const W3XDiscoveryTag &element,
    char *namespace_uri, size_t capacity, bool &found)
{
    found = false;
    size_t offset = scope.attributes_begin;
    while (offset < scope.attributes_end) {
        W3X_Skip_Whitespace(data, scope.attributes_end, offset);
        if (offset >= scope.attributes_end) {
            break;
        }

        const size_t name_begin = offset;
        while (offset < scope.attributes_end && W3X_Is_Name_Char(data[offset])) {
            ++offset;
        }
        const size_t name_end = offset;
        if (name_begin == name_end) {
            return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
        }

        W3X_Skip_Whitespace(data, scope.attributes_end, offset);
        if (offset >= scope.attributes_end || data[offset] != '=') {
            return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
        }
        ++offset;
        W3X_Skip_Whitespace(data, scope.attributes_end, offset);
        if (offset >= scope.attributes_end || (data[offset] != '"' && data[offset] != '\'')) {
            return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
        }

        const unsigned char quote = data[offset++];
        const size_t value_begin = offset;
        while (offset < scope.attributes_end && data[offset] != quote) {
            ++offset;
        }
        if (offset >= scope.attributes_end) {
            return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
        }
        const size_t value_end = offset++;

        if (W3X_Discovery_Is_Namespace_Binding(data, name_begin, name_end, element)) {
            found = true;
            return W3X_Discovery_Copy(data, value_begin, value_end, namespace_uri, capacity,
                W3X_TOP_LEVEL_DISCOVERY_NAMESPACE_TOO_LONG);
        }
    }
    return W3X_TOP_LEVEL_DISCOVERY_OK;
}

inline W3XTopLevelChildDiscoveryResult W3X_Discovery_Resolve_Namespace(
    const unsigned char *data, const W3XDiscoveryTag &root, const W3XDiscoveryTag &child,
    char *namespace_uri, size_t capacity)
{
    namespace_uri[0] = '\0';
    bool found = false;
    W3XTopLevelChildDiscoveryResult result =
        W3X_Discovery_Find_Namespace(data, child, child, namespace_uri, capacity, found);
    if (result != W3X_TOP_LEVEL_DISCOVERY_OK || found) {
        return result;
    }
    result = W3X_Discovery_Find_Namespace(data, root, child, namespace_uri, capacity, found);
    if (result != W3X_TOP_LEVEL_DISCOVERY_OK) {
        return result;
    }
    if (!found && child.has_prefix) {
        return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
    }
    return W3X_TOP_LEVEL_DISCOVERY_OK;
}

inline W3XTopLevelChildKind W3X_Classify_Top_Level_Child(const char *local_name, const char *namespace_uri)
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

inline W3XTopLevelChildDiscoveryResult W3X_Discovery_Skip_Terminated(
    const unsigned char *data, size_t size, size_t &offset, size_t prefix_size, const char *terminator)
{
    offset += prefix_size;
    return W3X_Find_Terminator(data, size, offset, terminator)
        ? W3X_TOP_LEVEL_DISCOVERY_OK : W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
}

inline W3XTopLevelChildDiscoveryResult W3X_Discovery_Find_Root(
    const unsigned char *data, size_t size, const W3XDocumentInfo &info, size_t &root_begin)
{
    size_t offset = 0;
    if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        offset = 3;
    }

    for (;;) {
        W3X_Skip_Whitespace(data, size, offset);
        if (offset >= size) {
            return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
        }
        if (W3X_Bytes_Equal(data, size, offset, "<!--")) {
            W3XTopLevelChildDiscoveryResult result = W3X_Discovery_Skip_Terminated(data, size, offset, 4, "-->");
            if (result != W3X_TOP_LEVEL_DISCOVERY_OK) {
                return result;
            }
            continue;
        }
        if (W3X_Bytes_Equal(data, size, offset, "<?")) {
            W3XTopLevelChildDiscoveryResult result = W3X_Discovery_Skip_Terminated(data, size, offset, 2, "?>");
            if (result != W3X_TOP_LEVEL_DISCOVERY_OK) {
                return result;
            }
            continue;
        }
        break;
    }

    const size_t root_name_length = strlen(info.root_qualified_name);
    if (offset >= size || data[offset] != '<' || root_name_length == 0
        || root_name_length > size - offset - 1
        || memcmp(data + offset + 1, info.root_qualified_name, root_name_length) != 0) {
        return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
    }
    root_begin = offset;
    return W3X_TOP_LEVEL_DISCOVERY_OK;
}

inline W3XTopLevelChildDiscoveryResult W3X_Discovery_Validate_Trailing(
    const unsigned char *data, size_t size, size_t offset)
{
    while (offset < size) {
        W3X_Skip_Whitespace(data, size, offset);
        if (offset >= size) {
            return W3X_TOP_LEVEL_DISCOVERY_OK;
        }
        if (W3X_Bytes_Equal(data, size, offset, "<!--")) {
            W3XTopLevelChildDiscoveryResult result = W3X_Discovery_Skip_Terminated(data, size, offset, 4, "-->");
            if (result != W3X_TOP_LEVEL_DISCOVERY_OK) {
                return result;
            }
            continue;
        }
        if (W3X_Bytes_Equal(data, size, offset, "<?")) {
            W3XTopLevelChildDiscoveryResult result = W3X_Discovery_Skip_Terminated(data, size, offset, 2, "?>");
            if (result != W3X_TOP_LEVEL_DISCOVERY_OK) {
                return result;
            }
            continue;
        }
        return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
    }
    return W3X_TOP_LEVEL_DISCOVERY_OK;
}

inline W3XTopLevelChildDiscoveryResult Discover_W3X_Top_Level_Children(
    const void *document_data, size_t document_size,
    W3XTopLevelChildInfo *children, size_t child_capacity, size_t &child_count)
{
    child_count = 0;
    if (children == 0 && child_capacity != 0) {
        return W3X_TOP_LEVEL_DISCOVERY_INVALID_ARGUMENT;
    }

    W3XDocumentInfo document_info;
    const W3XDocumentProbeResult probe_result = Probe_W3X_Document(document_data, document_size, document_info);
    if (probe_result != W3X_DOCUMENT_PROBE_OK) {
        return W3X_Discovery_Map_Probe_Result(probe_result);
    }
    if (!document_info.is_sage_asset_declaration) {
        return W3X_TOP_LEVEL_DISCOVERY_NOT_SAGE_ASSET_DECLARATION;
    }

    const unsigned char *data = static_cast<const unsigned char *>(document_data);
    size_t root_begin = 0;
    W3XTopLevelChildDiscoveryResult result =
        W3X_Discovery_Find_Root(data, document_size, document_info, root_begin);
    if (result != W3X_TOP_LEVEL_DISCOVERY_OK) {
        return result;
    }

    W3XDiscoveryTag root;
    result = W3X_Discovery_Parse_Start_Tag(data, document_size, root_begin, root);
    if (result != W3X_TOP_LEVEL_DISCOVERY_OK) {
        return result;
    }
    if (root.self_closing) {
        return W3X_Discovery_Validate_Trailing(data, document_size, root.end_offset);
    }

    size_t name_begin_stack[W3X_DISCOVERY_MAX_NESTING_DEPTH];
    size_t name_end_stack[W3X_DISCOVERY_MAX_NESTING_DEPTH];
    size_t depth = 1;
    name_begin_stack[0] = root.name_begin;
    name_end_stack[0] = root.name_end;
    size_t offset = root.end_offset;

    while (offset < document_size && depth != 0) {
        if (data[offset] != '<') {
            ++offset;
            continue;
        }
        if (W3X_Bytes_Equal(data, document_size, offset, "<!--")) {
            result = W3X_Discovery_Skip_Terminated(data, document_size, offset, 4, "-->");
            if (result != W3X_TOP_LEVEL_DISCOVERY_OK) {
                return result;
            }
            continue;
        }
        if (W3X_Bytes_Equal(data, document_size, offset, "<?")) {
            result = W3X_Discovery_Skip_Terminated(data, document_size, offset, 2, "?>");
            if (result != W3X_TOP_LEVEL_DISCOVERY_OK) {
                return result;
            }
            continue;
        }
        if (W3X_Bytes_Equal(data, document_size, offset, "<![CDATA[")) {
            result = W3X_Discovery_Skip_Terminated(data, document_size, offset, 9, "]]>");
            if (result != W3X_TOP_LEVEL_DISCOVERY_OK) {
                return result;
            }
            continue;
        }
        if (W3X_Bytes_Equal(data, document_size, offset, "<!DOCTYPE")) {
            return W3X_TOP_LEVEL_DISCOVERY_UNSUPPORTED_DOCTYPE;
        }
        if (W3X_Bytes_Equal(data, document_size, offset, "<!")) {
            return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
        }

        if (W3X_Bytes_Equal(data, document_size, offset, "</")) {
            size_t close_begin = 0;
            size_t close_end = 0;
            size_t tag_end = 0;
            result = W3X_Discovery_Parse_End_Tag(
                data, document_size, offset, close_begin, close_end, tag_end);
            if (result != W3X_TOP_LEVEL_DISCOVERY_OK) {
                return result;
            }
            if (!W3X_Discovery_Range_Equals(data,
                    name_begin_stack[depth - 1], name_end_stack[depth - 1], close_begin, close_end)) {
                return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
            }
            --depth;
            offset = tag_end;
            continue;
        }

        W3XDiscoveryTag tag;
        result = W3X_Discovery_Parse_Start_Tag(data, document_size, offset, tag);
        if (result != W3X_TOP_LEVEL_DISCOVERY_OK) {
            return result;
        }

        if (depth == 1) {
            W3XTopLevelChildInfo summary;
            result = W3X_Discovery_Copy(data, tag.name_begin, tag.name_end,
                summary.qualified_name, sizeof(summary.qualified_name), W3X_TOP_LEVEL_DISCOVERY_NAME_TOO_LONG);
            if (result != W3X_TOP_LEVEL_DISCOVERY_OK) {
                return result;
            }
            result = W3X_Discovery_Copy(data, tag.local_begin, tag.name_end,
                summary.local_name, sizeof(summary.local_name), W3X_TOP_LEVEL_DISCOVERY_NAME_TOO_LONG);
            if (result != W3X_TOP_LEVEL_DISCOVERY_OK) {
                return result;
            }
            result = W3X_Discovery_Resolve_Namespace(data, root, tag,
                summary.namespace_uri, sizeof(summary.namespace_uri));
            if (result != W3X_TOP_LEVEL_DISCOVERY_OK) {
                return result;
            }
            summary.kind = W3X_Classify_Top_Level_Child(summary.local_name, summary.namespace_uri);
            if (children != 0 && child_count < child_capacity) {
                children[child_count] = summary;
            }
            ++child_count;
        }

        offset = tag.end_offset;
        if (!tag.self_closing) {
            if (depth >= W3X_DISCOVERY_MAX_NESTING_DEPTH) {
                return W3X_TOP_LEVEL_DISCOVERY_NESTING_TOO_DEEP;
            }
            name_begin_stack[depth] = tag.name_begin;
            name_end_stack[depth] = tag.name_end;
            ++depth;
        }
    }

    if (depth != 0) {
        return W3X_TOP_LEVEL_DISCOVERY_MALFORMED;
    }
    result = W3X_Discovery_Validate_Trailing(data, document_size, offset);
    if (result != W3X_TOP_LEVEL_DISCOVERY_OK) {
        return result;
    }
    if (children != 0 && child_count > child_capacity) {
        return W3X_TOP_LEVEL_DISCOVERY_OUTPUT_TOO_SMALL;
    }
    return W3X_TOP_LEVEL_DISCOVERY_OK;
}

inline const char *Get_W3X_Top_Level_Child_Kind_Name(W3XTopLevelChildKind kind)
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

inline const char *Get_W3X_Top_Level_Discovery_Result_Name(W3XTopLevelChildDiscoveryResult result)
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

#endif // RTS_W3X_CHILD_DISCOVERY_H
