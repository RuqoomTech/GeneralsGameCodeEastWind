#include "WW3D2/ddsfile.h"
#include "WW3D2/bitmaphandler.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

static_assert(sizeof(LegacyDDSURFACEDESC2) == 124);
static_assert(offsetof(LegacyDDSURFACEDESC2, PixelFormat) == 72);
static_assert(offsetof(LegacyDDSURFACEDESC2, Caps) == 104);

int main()
{
    std::array<unsigned char, 16> block{};
    std::array<unsigned char, 64> pixels{};
    // Red/blue endpoints with each of the four color codes in every row.
    block[1] = 0xf8; block[2] = 0x1f;
    for (unsigned i = 4; i < 8; ++i) block[i] = 0xe4;
    bool ok = BitmapHandlerClass::Decode_DXT_Block_RGBA8(WW3D_FORMAT_DXT1, block.data(), pixels.data());
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
    if (!ok) { std::cerr << "DDS fixed-width header or BC1/2/3 decoding failed.\n"; return 1; }
    std::cout << "DDS header and DXT1-5 RGBA decoding passed.\n";
    return 0;
}
