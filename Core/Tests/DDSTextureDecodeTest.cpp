#include "WW3D2/ddsfile.h"
#include "WW3D2/bitmaphandler.h"
#include "WW3D2/surfaceclass.h"
#include "WWMath/vector2i.h"
#include "WWMath/vector4.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

static_assert(sizeof(LegacyDDSURFACEDESC2) == 124);
static_assert(offsetof(LegacyDDSURFACEDESC2, PixelFormat) == 72);
static_assert(offsetof(LegacyDDSURFACEDESC2, Caps) == 104);

static bool Test_Authored_DDS_Mips()
{
    const auto path = std::filesystem::temp_directory_path() /
        ("evolution-dds-mips-" + std::to_string(GetCurrentProcessId()) + ".dds");
    struct RemoveFixture {
        std::filesystem::path path;
        ~RemoveFixture() { std::error_code error; std::filesystem::remove(path, error); }
    } cleanup{path};
    LegacyDDSURFACEDESC2 header{};
    header.Size = 124; header.Width = 8; header.Height = 4; header.MipMapCount = 4;
    header.PixelFormat.Size = 32;
    header.PixelFormat.FourCC = unsigned('D') | (unsigned('X')<<8) | (unsigned('T')<<16) | (unsigned('1')<<24);
    std::vector<unsigned char> payload;
    const unsigned short colors[] = {0xf800, 0x001f, 0x07e0, 0xffff};
    for (unsigned level = 0; level < 4; ++level) {
        const unsigned blocks = level == 0 ? 2 : 1;
        for (unsigned block = 0; block < blocks; ++block) {
            const std::size_t offset = payload.size(); payload.resize(offset+8, 0);
            payload[offset] = static_cast<unsigned char>(colors[level]);
            payload[offset+1] = static_cast<unsigned char>(colors[level]>>8);
        }
    }
    const auto write = [&]() {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write("DDS ", 4);
        output.write(reinterpret_cast<const char *>(&header), sizeof(header));
        output.write(reinterpret_cast<const char *>(payload.data()), payload.size());
        return bool(output);
    };
    if (!write()) return false;
    const auto name = path.string();
    DDSFileClass authored(name.c_str(), 0, true);
    bool ok = authored.Get_Mip_Level_Count() == 4 && authored.Load();
    const unsigned expected[][4] = {{255,0,0,255},{0,0,255,255},{0,255,0,255},{255,255,255,255}};
    for (unsigned level = 0; level < 4 && ok; ++level) {
        const unsigned width = 8>>level ? 8>>level : 1;
        const unsigned height = 4>>level ? 4>>level : 1;
        std::vector<unsigned char> pixels(static_cast<std::size_t>(width)*height*4);
        ok = authored.Copy_Level_RGBA8(level, pixels.data(), width*4) && ok;
        for (std::size_t offset = 0; offset < pixels.size(); ++offset)
            ok = pixels[offset] == expected[level][offset%4] && ok;
    }
    DDSFileClass reduced(name.c_str(), 3, true);
    std::array<unsigned char, 4> last{};
    ok = reduced.Get_Mip_Level_Count() == 1 && reduced.Load() &&
        reduced.Copy_Level_RGBA8(0, last.data(), 4) && last == std::array<unsigned char,4>{255,255,255,255} && ok;
    DDSFileClass archival(name.c_str(), 0);
    ok = archival.Get_Mip_Level_Count() == 2 && ok;
    std::array<unsigned char, 4> invalid{};
    ok = !authored.Copy_Level_RGBA8(4, invalid.data(), 4) &&
        !authored.Copy_Level_RGBA8(0, invalid.data(), 4) && ok;

    // A malformed chain or truncated final authored level must not be accepted.
    header.MipMapCount = 5;
    if (!write()) return false;
    DDSFileClass overdeclared(name.c_str(), 0, true);
    ok = overdeclared.Get_Mip_Level_Count() == 0 && !overdeclared.Load() && ok;
    header.MipMapCount = 4; payload.pop_back();
    if (!write()) return false;
    DDSFileClass truncated(name.c_str(), 0, true);
    ok = !truncated.Load() && ok;
    payload.push_back(0);
    for (const unsigned caps : {0x200u, 0x200000u}) {
        header.Caps.Caps2 = caps;
        if (!write()) return false;
        DDSFileClass unsupported(name.c_str(), 0, true);
        ok = unsupported.Get_Mip_Level_Count() == 0 && !unsupported.Load() && ok;
    }
    // Partial 4x4 blocks must clip to 3x5 pixels and preserve destination row padding.
    header.Caps.Caps2 = 0; header.Width = 3; header.Height = 5; header.MipMapCount = 1;
    payload.resize(16, 0);
    payload[0] = 0; payload[1] = 0xf8; payload[8] = 0x1f; payload[9] = 0;
    if (!write()) return false;
    DDSFileClass odd(name.c_str(), 0, true);
    std::array<unsigned char, 80> clipped;
    clipped.fill(0xab);
    ok = odd.Load() && odd.Copy_Level_RGBA8(0, clipped.data(), 16) && ok;
    for (unsigned y = 0; y < 5; ++y) {
        for (unsigned x = 0; x < 3; ++x)
            for (unsigned channel = 0; channel < 4; ++channel)
                ok = clipped[y*16+x*4+channel] == expected[y == 4 ? 1 : 0][channel] && ok;
        for (unsigned channel = 12; channel < 16; ++channel) ok = clipped[y*16+channel] == 0xab && ok;
    }
    // Exercise the production constructor's file FourCC mapping for every supported
    // encoding, followed by an unknown identifier that must not produce an image.
    header.Width = 4; header.Height = 4; header.MipMapCount = 1;
    const WW3DFormat formats[] = {WW3D_FORMAT_DXT1, WW3D_FORMAT_DXT2, WW3D_FORMAT_DXT3,
        WW3D_FORMAT_DXT4, WW3D_FORMAT_DXT5};
    for (unsigned encoding = 0; encoding < 5; ++encoding) {
        header.PixelFormat.FourCC = 0x31545844u + (encoding << 24);
        payload.assign(encoding == 0 ? 8 : 16, 0);
        const unsigned color_offset = encoding == 0 ? 0 : 8;
        payload[color_offset+1] = 0xf8; // red RGB565 endpoint, color indices all zero
        if (encoding == 1 || encoding == 2)
            for (unsigned i = 0; i < 8; ++i) payload[i] = 0xff;
        else if (encoding != 0)
            payload[0] = 0xff; // DXT4/5 alpha endpoint, alpha indices all zero
        if (!write()) return false;
        DDSFileClass encoded(name.c_str(), 0, true);
        std::array<unsigned char, 64> decoded{};
        ok = encoded.Get_Format() == formats[encoding] && encoded.Load() &&
            encoded.Copy_Level_RGBA8(0, decoded.data(), 16) && ok;
        for (std::size_t offset = 0; offset < decoded.size(); ++offset)
            ok = decoded[offset] == expected[0][offset%4] && ok;
    }
    header.PixelFormat.FourCC = 0x44434241u; // unsupported 'ABCD'
    if (!write()) return false;
    DDSFileClass unknown(name.c_str(), 0, true);
    std::array<unsigned char, 64> unchanged;
    unchanged.fill(0xab);
    ok = unknown.Get_Format() == WW3D_FORMAT_UNKNOWN && unknown.Get_Mip_Level_Count() == 0 &&
        !unknown.Load() && !unknown.Copy_Level_RGBA8(0, unchanged.data(), 16) && ok;
    for (const unsigned char value : unchanged) ok = value == 0xab && ok;
    return ok;
}

static bool Test_CPU_Format_Policy()
{
    const Vector4 unpacked = Unpack_ARGB_Color(0x80402010u);
    bool ok = unpacked.X == 64.0f/255.0f && unpacked.Y == 32.0f/255.0f &&
        unpacked.Z == 16.0f/255.0f && unpacked.W == 128.0f/255.0f;
    ok = Pack_ARGB_Color(Vector4(1.0f, 0.5f, 0.25f, 0.75f)) == 0xbfFF7f3fu && ok;
    ok = Pack_ARGB_Color(Vector4(0, 0, 0, 0)) == 0 &&
        Pack_ARGB_Color(Vector4(1, 1, 1, 1)) == 0xffffffffu && ok;
    unsigned packed = 0;
    Vector4_to_Color(&packed, Vector4(1.0f, 0.5f, 0.25f, 0.75f), WW3D_FORMAT_A8R8G8B8);
    ok = packed == 0xbfFF7f3fu && ok;
    // Selecting storage must work before a renderer exists. DXT is a CPU
    // source encoding; disabling compression chooses an uncompressed target.
    for (const auto format : {WW3D_FORMAT_DXT1, WW3D_FORMAT_DXT2, WW3D_FORMAT_DXT3,
            WW3D_FORMAT_DXT4, WW3D_FORMAT_DXT5}) {
        ok = Get_Valid_Texture_Format(format, true) == format && ok;
        ok = Get_Valid_Texture_Format(format, false) ==
            (format == WW3D_FORMAT_DXT1 ? WW3D_FORMAT_X8R8G8B8 : WW3D_FORMAT_A8R8G8B8) && ok;
    }
    ok = Get_Valid_Texture_Format(WW3D_FORMAT_R8G8B8, false) == WW3D_FORMAT_X8R8G8B8 && ok;
    for (const auto format : {WW3D_FORMAT_A8R8G8B8, WW3D_FORMAT_X8R8G8B8,
            WW3D_FORMAT_R5G6B5, WW3D_FORMAT_A1R5G5B5, WW3D_FORMAT_A4R4G4B4,
            WW3D_FORMAT_A8, WW3D_FORMAT_L8})
        ok = Get_Valid_Texture_Format(format, false) == format && ok;
    for (const auto format : {WW3D_FORMAT_X1R5G5B5, WW3D_FORMAT_R3G3B2,
            WW3D_FORMAT_A8R3G3B2, WW3D_FORMAT_X4R4G4B4, WW3D_FORMAT_A8P8,
            WW3D_FORMAT_P8, WW3D_FORMAT_A8L8, WW3D_FORMAT_A4L4})
        ok = Get_Valid_Texture_Format(format, false) == WW3D_FORMAT_A8R8G8B8 && ok;
    for (const auto format : {WW3D_FORMAT_UNKNOWN, WW3D_FORMAT_U8V8,
            WW3D_FORMAT_L6V5U5, WW3D_FORMAT_X8L8V8U8})
        ok = Get_Valid_Texture_Format(format, true) == WW3D_FORMAT_UNKNOWN && ok;
    return ok;
}

static bool Test_CPU_Surface()
{
    SurfaceClass source(2, 2, WW3D_FORMAT_A8R8G8B8);
    const unsigned char bgra[] = {0x10,0x20,0x40,0x80, 0xff,0,0,0xff, 0,0,0xff,0xff, 0xff,0xff,0xff,0xff};
    const auto initial_revision=source.Get_Revision();
    source.Copy(bgra);
    std::vector<unsigned char> rgba;
    bool ok=source.Get_Revision()!=initial_revision && source.Copy_RGBA8(rgba) && rgba.size()==16 &&
        rgba[0]==0x40 && rgba[1]==0x20 && rgba[2]==0x10 && rgba[3]==0x80;
    SurfaceClass copy(2,2,WW3D_FORMAT_A8R8G8B8);
    copy.Copy(0,0,0,0,2,2,&source);
    std::vector<unsigned char> copied;
    ok=copy.Copy_RGBA8(copied) && copied==rgba && ok;
    const auto before_lock=copy.Get_Revision();
    int pitch=0; auto *data=static_cast<unsigned char*>(copy.Lock(&pitch));
    data[0]=0; data[1]=0xff; data[2]=0; data[3]=0xff; copy.Unlock();
    ok=copy.Get_Revision()!=before_lock && copy.Copy_RGBA8(copied) && copied[0]==0 && copied[1]==255 && copied[2]==0 && ok;
    SurfaceClass scaled(1,1,WW3D_FORMAT_A8R8G8B8);
    scaled.Stretch_Copy(0,0,1,1,0,0,2,2,&source);
    std::vector<unsigned char> result;
    ok=scaled.Copy_RGBA8(result) && result.size()==4 && result[0]==0x40 && result[3]==0x80 && ok;
    const auto retained=result;
    scaled.Stretch_Copy(2,0,1,1,0,0,2,2,&source);
    scaled.Copy(0,0,0xffffffffu,0,1,1,&source);
    ok=scaled.Copy_RGBA8(result) && result==retained && ok;
    // A shifted self-copy reads the original image, including overlapping rows.
    copy.Copy(bgra);
    copy.Copy(1,1,0,0,1,1,&copy);
    ok=copy.Copy_RGBA8(copied) && copied[12]==0x40 && copied[13]==0x20 && copied[14]==0x10 && ok;
    copy.Copy(bgra);
    copy.Copy(1,0,0,0,1,2,&copy);
    ok=copy.Copy_RGBA8(copied) && copied[4]==0x40 && copied[12]==0xff && copied[14]==0 && ok;
    const auto before_invalid=copied;
    copy.Copy(Vector2i(-1,0),Vector2i(1,1),bgra);
    ok=copy.Lock(&pitch,Vector2i(-1,0),Vector2i(1,1))==nullptr &&
        copy.Copy_RGBA8(copied) && copied==before_invalid && ok;
    SurfaceClass glyph(2,2,WW3D_FORMAT_A4R4G4B4);
    glyph.Copy(0,0,0,0,2,2,&source);
    ok=glyph.Copy_RGBA8(copied) && copied[0]==68 && copied[1]==34 &&
        copied[2]==17 && copied[3]==136 && copied[12]==255 && copied[15]==255 && ok;
    SurfaceClass alpha_only(1,1,WW3D_FORMAT_A8);
    const unsigned char coverage=128; alpha_only.Copy(&coverage);
    ok=alpha_only.Copy_RGBA8(copied) && copied[0]==0 && copied[1]==0 && copied[2]==0 && copied[3]==128 && ok;
    // A one-byte packed destination may not overwrite its sentinel neighbor.
    unsigned char packed[2]={0xab,0xcd}; const unsigned argb=0xffff00ffu;
    BitmapHandlerClass::Write_B8G8R8A8(packed,WW3D_FORMAT_R3G3B2,argb);
    ok=packed[0]==0xe3 && packed[1]==0xcd && ok;
    return ok;
}

int main()
{
    std::array<unsigned char, 16> block{};
    std::array<unsigned char, 64> pixels{};
    // Red/blue endpoints with each of the four color codes in every row.
    block[1] = 0xf8; block[2] = 0x1f;
    for (unsigned i = 4; i < 8; ++i) block[i] = 0xe4;
    bool ok = Test_CPU_Surface() && Test_CPU_Format_Policy() && Test_Authored_DDS_Mips();
    ok = BitmapHandlerClass::Decode_DXT_Block_RGBA8(WW3D_FORMAT_DXT1, block.data(), pixels.data()) && ok;
    const unsigned expected[4][4] = {{255,0,0,255},{0,0,255,255},{170,0,85,255},{85,0,170,255}};
    for (unsigned i = 0; i < 16; ++i)
        for (unsigned channel = 0; channel < 4; ++channel)
            ok = pixels[i*4+channel] == expected[i%4][channel] && ok;
    block[0] = 0x1f; block[1] = 0; block[2] = 0; block[3] = 0xf8;
    ok = BitmapHandlerClass::Decode_DXT_Block_RGBA8(WW3D_FORMAT_DXT1, block.data(), pixels.data()) && ok;
    ok = pixels[8] == 127 && pixels[10] == 127 && pixels[11] == 255 && pixels[15] == 0 && ok;

    // BC2 forces four colors even when endpoint zero is less than endpoint one.
    const auto color_block = block;
    block.fill(0);
    for (unsigned i = 0; i < 8; ++i) {
        block[i] = static_cast<unsigned char>((i*2) | ((i*2+1)<<4));
        block[i+8] = color_block[i];
    }
    for (const auto format : {WW3D_FORMAT_DXT2, WW3D_FORMAT_DXT3}) {
        ok = BitmapHandlerClass::Decode_DXT_Block_RGBA8(format, block.data(), pixels.data()) && ok;
        for (unsigned i = 0; i < 16; ++i) ok = pixels[i*4+3] == i*17 && ok;
        ok = pixels[8] == 85 && pixels[10] == 170 && pixels[12] == 170 && pixels[14] == 85 && ok;
    }
    // BC3 exercises every alpha code, including codes crossing byte boundaries.
    std::uint64_t codes = 0;
    for (unsigned i = 0; i < 16; ++i) codes |= std::uint64_t(i%8) << (i*3);
    for (unsigned i = 0; i < 6; ++i) block[i+2] = static_cast<unsigned char>(codes >> (i*8));
    for (const auto format : {WW3D_FORMAT_DXT4, WW3D_FORMAT_DXT5}) {
        block[0] = 210; block[1] = 0;
        ok = BitmapHandlerClass::Decode_DXT_Block_RGBA8(format, block.data(), pixels.data()) && ok;
        const unsigned descending[] = {210,0,180,150,120,90,60,30};
        for (unsigned i = 0; i < 16; ++i) ok = pixels[i*4+3] == descending[i%8] && ok;
        block[0] = 0; block[1] = 200;
        ok = BitmapHandlerClass::Decode_DXT_Block_RGBA8(format, block.data(), pixels.data()) && ok;
        const unsigned ascending[] = {0,200,40,80,120,160,0,255};
        for (unsigned i = 0; i < 16; ++i) ok = pixels[i*4+3] == ascending[i%8] && ok;
        // Premultiplied DXT2/4 colors are preserved, as with direct GPU sampling.
        ok = pixels[8] == 85 && pixels[10] == 170 && ok;
    }
    const auto retained = pixels;
    ok = !BitmapHandlerClass::Decode_DXT_Block_RGBA8(WW3D_FORMAT_UNKNOWN, block.data(), pixels.data()) &&
         !BitmapHandlerClass::Decode_DXT_Block_RGBA8(WW3D_FORMAT_DXT1, nullptr, pixels.data()) &&
         !BitmapHandlerClass::Decode_DXT_Block_RGBA8(WW3D_FORMAT_DXT1, block.data(), nullptr) && pixels == retained && ok;
    // CPU mip generation must honor row pitches and remain in bounds for one-axis/odd images.
    std::array<unsigned char, 40> tall{};
    for (unsigned row = 0; row < 5; ++row) {
        tall[row*8] = static_cast<unsigned char>(row*20+3);
        tall[row*8+1] = static_cast<unsigned char>(row*12+2);
        tall[row*8+2] = static_cast<unsigned char>(row*4+1);
        tall[row*8+3] = 255;
    }
    std::array<unsigned char, 16> tall_mip;
    tall_mip.fill(0xab);
    BitmapHandlerClass::Create_Mipmap(tall_mip.data(), 8, WW3D_FORMAT_A8R8G8B8,
        tall.data(), 8, WW3D_FORMAT_A8R8G8B8, 1, 5);
    const unsigned expected_mip[2][4] = {{10,6,2,252},{50,30,10,252}};
    for (unsigned row = 0; row < 2; ++row) {
        for (unsigned channel = 0; channel < 4; ++channel)
            ok = tall_mip[row*8+channel] == expected_mip[row][channel] && ok;
        for (unsigned channel = 4; channel < 8; ++channel)
            ok = tall_mip[row*8+channel] == 0xab && ok;
    }
    std::array<unsigned char, 20> wide{};
    for (unsigned col = 0; col < 5; ++col)
        for (unsigned channel = 0; channel < 4; ++channel) wide[col*4+channel] = tall[col*8+channel];
    std::array<unsigned char, 12> wide_mip;
    wide_mip.fill(0xab);
    BitmapHandlerClass::Create_Mipmap(wide_mip.data(), 12, WW3D_FORMAT_A8R8G8B8,
        wide.data(), 20, WW3D_FORMAT_A8R8G8B8, 5, 1);
    for (unsigned col = 0; col < 2; ++col)
        for (unsigned channel = 0; channel < 4; ++channel)
            ok = wide_mip[col*4+channel] == expected_mip[col][channel] && ok;
    for (unsigned channel = 8; channel < 12; ++channel) ok = wide_mip[channel] == 0xab && ok;
    std::array<unsigned char, 4> last_mip{};
    BitmapHandlerClass::Create_Mipmap(last_mip.data(), 4, WW3D_FORMAT_A8R8G8B8,
        wide_mip.data(), 12, WW3D_FORMAT_A8R8G8B8, 2, 1);
    ok = last_mip[0] == 28 && last_mip[1] == 16 && last_mip[2] == 4 && last_mip[3] == 252 && ok;
    if (!ok) { std::cerr << "DDS fixed-width header or BC1/2/3 decoding failed.\n"; return 1; }
    std::cout << "DDS header, DXT1-5 RGBA decoding, and CPU mip generation passed.\n";
    return 0;
}
