#ifndef RTS_ASSET_3D_FORMAT_H
#define RTS_ASSET_3D_FORMAT_H

#include <stddef.h>

namespace rts {

enum Asset3DFormat
{
    ASSET_3D_FORMAT_UNKNOWN = 0,
    ASSET_3D_FORMAT_W3D,
    ASSET_3D_FORMAT_W3X
};

inline char Asset_3D_To_Lower_ASCII(char value)
{
    if (value >= 'A' && value <= 'Z') {
        return static_cast<char>(value - 'A' + 'a');
    }

    return value;
}

inline bool Asset_3D_Extension_Equals(const char *extension, const char *expected)
{
    if (extension == 0 || expected == 0) {
        return false;
    }

    while (*extension != '\0' && *expected != '\0') {
        if (Asset_3D_To_Lower_ASCII(*extension) != Asset_3D_To_Lower_ASCII(*expected)) {
            return false;
        }

        ++extension;
        ++expected;
    }

    return *extension == '\0' && *expected == '\0';
}

inline Asset3DFormat Detect_3D_Asset_Format_From_Path(const char *path)
{
    if (path == 0 || *path == '\0') {
        return ASSET_3D_FORMAT_UNKNOWN;
    }

    const char *extension = 0;

    for (const char *cursor = path; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' || *cursor == '\\') {
            extension = 0;
        } else if (*cursor == '.') {
            extension = cursor;
        }
    }

    if (Asset_3D_Extension_Equals(extension, ".w3d")) {
        return ASSET_3D_FORMAT_W3D;
    }

    if (Asset_3D_Extension_Equals(extension, ".w3x")) {
        return ASSET_3D_FORMAT_W3X;
    }

    return ASSET_3D_FORMAT_UNKNOWN;
}

inline bool Looks_Like_XML_Document(const void *data, size_t size)
{
    if (data == 0 || size == 0) {
        return false;
    }

    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    size_t offset = 0;

    // UTF-8 BOM.
    if (size >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
        offset = 3;
    }

    while (offset < size) {
        const unsigned char value = bytes[offset];
        if (value != ' ' && value != '\t' && value != '\r' && value != '\n') {
            break;
        }
        ++offset;
    }

    return offset < size && bytes[offset] == '<';
}

inline Asset3DFormat Detect_3D_Asset_Format(const char *path, const void *data, size_t size)
{
    const Asset3DFormat format = Detect_3D_Asset_Format_From_Path(path);

    if (format == ASSET_3D_FORMAT_W3X && data != 0 && size != 0 && !Looks_Like_XML_Document(data, size)) {
        // The .w3x extension is also used by unrelated binary formats. Later W3X
        // routing must not send an obviously non-XML file into the SAGE W3X parser.
        return ASSET_3D_FORMAT_UNKNOWN;
    }

    return format;
}

inline const char *Get_3D_Asset_Format_Name(Asset3DFormat format)
{
    switch (format) {
        case ASSET_3D_FORMAT_W3D:
            return "W3D";
        case ASSET_3D_FORMAT_W3X:
            return "W3X";
        default:
            return "Unknown";
    }
}

} // namespace rts

#endif // RTS_ASSET_3D_FORMAT_H
