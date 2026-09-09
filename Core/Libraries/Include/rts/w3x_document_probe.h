#ifndef RTS_W3X_DOCUMENT_PROBE_H
#define RTS_W3X_DOCUMENT_PROBE_H

#include <stddef.h>
#include <string.h>

namespace rts {

static const size_t W3X_ROOT_NAME_CAPACITY = 96;
static const size_t W3X_NAMESPACE_CAPACITY = 192;
static const char W3X_SAGE_ASSET_NAMESPACE[] = "uri:ea.com:eala:asset";

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
    W3XDocumentInfo()
        : has_xml_declaration(false), is_sage_asset_declaration(false)
    {
        root_qualified_name[0] = '\0';
        root_local_name[0] = '\0';
        root_namespace[0] = '\0';
    }

    bool has_xml_declaration;
    bool is_sage_asset_declaration;
    char root_qualified_name[W3X_ROOT_NAME_CAPACITY];
    char root_local_name[W3X_ROOT_NAME_CAPACITY];
    char root_namespace[W3X_NAMESPACE_CAPACITY];
};

inline bool W3X_Is_XML_Whitespace(unsigned char value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

inline bool W3X_Is_Name_Start(unsigned char value)
{
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || value == '_' || value == ':';
}

inline bool W3X_Is_Name_Char(unsigned char value)
{
    return W3X_Is_Name_Start(value) || (value >= '0' && value <= '9') || value == '-' || value == '.';
}

inline bool W3X_Bytes_Equal(const unsigned char *data, size_t size, size_t offset, const char *text)
{
    if (data == 0 || text == 0 || offset > size) {
        return false;
    }

    const size_t text_size = strlen(text);
    if (text_size > size - offset) {
        return false;
    }

    return memcmp(data + offset, text, text_size) == 0;
}

inline void W3X_Skip_Whitespace(const unsigned char *data, size_t size, size_t &offset)
{
    while (offset < size && W3X_Is_XML_Whitespace(data[offset])) {
        ++offset;
    }
}

inline bool W3X_Find_Terminator(const unsigned char *data, size_t size, size_t &offset, const char *terminator)
{
    const size_t terminator_size = strlen(terminator);

    while (offset <= size) {
        if (offset <= size && terminator_size <= size - offset
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

inline W3XDocumentProbeResult W3X_Copy_Range(
    const unsigned char *data, size_t begin, size_t end, char *output, size_t output_capacity,
    W3XDocumentProbeResult too_long_result)
{
    const size_t length = end - begin;
    if (length + 1 > output_capacity) {
        return too_long_result;
    }

    if (length != 0) {
        memcpy(output, data + begin, length);
    }
    output[length] = '\0';
    return W3X_DOCUMENT_PROBE_OK;
}

inline bool W3X_String_Equals_Range(const char *text, const unsigned char *data, size_t begin, size_t end)
{
    if (text == 0) {
        return false;
    }

    const size_t text_size = strlen(text);
    return text_size == end - begin && memcmp(text, data + begin, text_size) == 0;
}

inline W3XDocumentProbeResult Probe_W3X_Document(const void *document_data, size_t document_size, W3XDocumentInfo &info)
{
    info = W3XDocumentInfo();

    if (document_data == 0 || document_size == 0) {
        return W3X_DOCUMENT_PROBE_EMPTY;
    }

    const unsigned char *data = static_cast<const unsigned char *>(document_data);
    size_t offset = 0;

    if (document_size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        offset = 3;
    }

    W3X_Skip_Whitespace(data, document_size, offset);
    if (offset >= document_size) {
        return W3X_DOCUMENT_PROBE_EMPTY;
    }
    if (data[offset] != '<') {
        return W3X_DOCUMENT_PROBE_NOT_XML;
    }

    // Skip XML declaration, processing instructions, and comments before the root.
    for (;;) {
        W3X_Skip_Whitespace(data, document_size, offset);
        if (offset >= document_size) {
            return W3X_DOCUMENT_PROBE_MALFORMED;
        }

        if (W3X_Bytes_Equal(data, document_size, offset, "<!--")) {
            offset += 4;
            if (!W3X_Find_Terminator(data, document_size, offset, "-->")) {
                return W3X_DOCUMENT_PROBE_MALFORMED;
            }
            continue;
        }

        if (W3X_Bytes_Equal(data, document_size, offset, "<!DOCTYPE")) {
            return W3X_DOCUMENT_PROBE_UNSUPPORTED_DOCTYPE;
        }

        if (W3X_Bytes_Equal(data, document_size, offset, "<?")) {
            const size_t instruction_begin = offset;
            offset += 2;
            if (!W3X_Find_Terminator(data, document_size, offset, "?>")) {
                return W3X_DOCUMENT_PROBE_MALFORMED;
            }

            if (W3X_Bytes_Equal(data, document_size, instruction_begin, "<?xml")) {
                const size_t after_xml = instruction_begin + 5;
                if (after_xml < document_size
                    && (W3X_Is_XML_Whitespace(data[after_xml]) || data[after_xml] == '?')) {
                    info.has_xml_declaration = true;
                }
            }
            continue;
        }

        break;
    }

    if (offset >= document_size || data[offset] != '<') {
        return W3X_DOCUMENT_PROBE_MALFORMED;
    }
    ++offset;

    if (offset >= document_size || data[offset] == '/' || data[offset] == '!' || data[offset] == '?'
        || !W3X_Is_Name_Start(data[offset])) {
        return W3X_DOCUMENT_PROBE_MALFORMED;
    }

    const size_t root_name_begin = offset;
    while (offset < document_size && W3X_Is_Name_Char(data[offset])) {
        ++offset;
    }
    const size_t root_name_end = offset;

    W3XDocumentProbeResult copy_result = W3X_Copy_Range(data, root_name_begin, root_name_end,
        info.root_qualified_name, sizeof(info.root_qualified_name), W3X_DOCUMENT_PROBE_NAME_TOO_LONG);
    if (copy_result != W3X_DOCUMENT_PROBE_OK) {
        return copy_result;
    }

    size_t local_name_begin = root_name_begin;
    size_t prefix_end = root_name_begin;
    bool has_prefix = false;
    for (size_t cursor = root_name_begin; cursor < root_name_end; ++cursor) {
        if (data[cursor] == ':') {
            if (has_prefix || cursor == root_name_begin || cursor + 1 == root_name_end) {
                return W3X_DOCUMENT_PROBE_MALFORMED;
            }
            has_prefix = true;
            prefix_end = cursor;
            local_name_begin = cursor + 1;
        }
    }

    copy_result = W3X_Copy_Range(data, local_name_begin, root_name_end,
        info.root_local_name, sizeof(info.root_local_name), W3X_DOCUMENT_PROBE_NAME_TOO_LONG);
    if (copy_result != W3X_DOCUMENT_PROBE_OK) {
        return copy_result;
    }

    bool namespace_found = false;

    // Parse only enough of the root start tag to discover the namespace binding.
    for (;;) {
        W3X_Skip_Whitespace(data, document_size, offset);
        if (offset >= document_size) {
            return W3X_DOCUMENT_PROBE_MALFORMED;
        }

        if (data[offset] == '>') {
            ++offset;
            break;
        }
        if (data[offset] == '/') {
            ++offset;
            if (offset >= document_size || data[offset] != '>') {
                return W3X_DOCUMENT_PROBE_MALFORMED;
            }
            ++offset;
            break;
        }
        if (!W3X_Is_Name_Start(data[offset])) {
            return W3X_DOCUMENT_PROBE_MALFORMED;
        }

        const size_t attribute_name_begin = offset;
        while (offset < document_size && W3X_Is_Name_Char(data[offset])) {
            ++offset;
        }
        const size_t attribute_name_end = offset;

        W3X_Skip_Whitespace(data, document_size, offset);
        if (offset >= document_size || data[offset] != '=') {
            return W3X_DOCUMENT_PROBE_MALFORMED;
        }
        ++offset;
        W3X_Skip_Whitespace(data, document_size, offset);

        if (offset >= document_size || (data[offset] != '"' && data[offset] != '\'')) {
            return W3X_DOCUMENT_PROBE_MALFORMED;
        }

        const unsigned char quote = data[offset++];
        const size_t attribute_value_begin = offset;
        while (offset < document_size && data[offset] != quote) {
            ++offset;
        }
        if (offset >= document_size) {
            return W3X_DOCUMENT_PROBE_MALFORMED;
        }
        const size_t attribute_value_end = offset;
        ++offset;

        bool is_root_namespace_binding = false;
        if (!has_prefix) {
            is_root_namespace_binding = W3X_String_Equals_Range("xmlns", data, attribute_name_begin, attribute_name_end);
        } else if (attribute_name_end - attribute_name_begin > 6
            && W3X_String_Equals_Range("xmlns", data, attribute_name_begin, attribute_name_begin + 5)
            && data[attribute_name_begin + 5] == ':') {
            const size_t binding_prefix_begin = attribute_name_begin + 6;
            is_root_namespace_binding = binding_prefix_begin < attribute_name_end
                && prefix_end - root_name_begin == attribute_name_end - binding_prefix_begin
                && memcmp(data + root_name_begin, data + binding_prefix_begin, prefix_end - root_name_begin) == 0;
        }

        if (is_root_namespace_binding) {
            copy_result = W3X_Copy_Range(data, attribute_value_begin, attribute_value_end,
                info.root_namespace, sizeof(info.root_namespace), W3X_DOCUMENT_PROBE_NAMESPACE_TOO_LONG);
            if (copy_result != W3X_DOCUMENT_PROBE_OK) {
                return copy_result;
            }
            namespace_found = true;
        }
    }

    if (!namespace_found) {
        info.root_namespace[0] = '\0';
    }

    info.is_sage_asset_declaration = strcmp(info.root_local_name, "AssetDeclaration") == 0
        && strcmp(info.root_namespace, W3X_SAGE_ASSET_NAMESPACE) == 0;

    return W3X_DOCUMENT_PROBE_OK;
}

inline const char *Get_W3X_Document_Probe_Result_Name(W3XDocumentProbeResult result)
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

} // namespace rts

#endif // RTS_W3X_DOCUMENT_PROBE_H
