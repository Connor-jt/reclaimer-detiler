// Dll1.cpp : Defines the exported functions for the DLL.
//

#include "framework.h"
#include "Dll1.h"
#include <stdint.h>

#include "detiling/DirectXTexXbox.h"



#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <algorithm>

#include <stdio.h>
#include <sys/types.h>
//#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

#include <locale>  
#include <iostream>
#include <codecvt>


#include <string>
#include <stdint.h>
#include <vector>


using namespace std;


// function from 'https://github.com/Lord-Zedd/H5Bitmap/' //
int32_t GetFormat(uint16_t tagformat) {
    switch (tagformat) {
    case  0x0: return 65;
    case  0x1:
    case  0x2: return 61;
    case  0x3: return 49;
        //case  0x4:
        //case  0x5: throw exception("unsupported image format");
    case  0x6: return 85;
        //case  0x7: throw exception("unsupported image format");
    case  0x8: return 86;
    case  0x9: return 115;
    case  0xA: return 88;
    case  0xB: return 87;
        //case  0xC: throw exception("unsupported image format");
    case  0xE: return 71;
    case  0xF: return 74;
    case 0x10: return 77;
        //case 0x11:
        //case 0x12:
        //case 0x13:
        //case 0x14:
        //case 0x15: throw exception("unsupported image format");
    case 0x16: return 51;
        //case 0x17: throw exception("unsupported image format");
    case 0x18: return 2;
    case 0x19: return 10;
    case 0x1A:
    case 0x1B: return 54;
    case 0x1C: return 31;
    case 0x1D: return 24;
    case 0x1E: return 11;
    case 0x1F: return 37;
    case 0x20: return 56;
    case 0x21: return 35;
    case 0x22: return 13;
        //case 0x23: throw exception("unsupported image format");
    case 0x24: return 80;
    case 0x25: return 81;
        //case 0x26: throw exception("unsupported image format");
    case 0x27: return 84;
    case 0x28: return 107; // "This is a guess, tag defs claim this format is deprecated, yet it is still used. gg"
        //case 0x29: 
        //case 0x2A: throw exception("unsupported image format");
    case 0x2B:
    case 0x2C: return 80;
    case 0x2D: return 83;
    case 0x2E: return 84;
    case 0x2F: return 95;
    case 0x30: return 96;
    case 0x31: return 97;
    case 0x32: return 45;
    case 0x33: return 26;
    default: return -1;
    }

}

const int sizeof_DDS_HEADER = 124; // sizeof(DirectX::DDS_HEADER);

void detile_data(char* data, uint32_t data_size, uint32_t width, uint32_t height, uint8_t format, char*& DDSheader_dest)
{
    // construct DDS header information
    size_t header_size = sizeof(uint32_t) + sizeof_DDS_HEADER + sizeof(DDS_HEADER_XBOX_p); 

    // configure meta data
    DirectX::TexMetadata meta = {};
    meta.depth = 1;
    meta.arraySize = 1; // no support for array types yet
    meta.mipLevels = 0; // it seems this does not apply for all versions // selected_bitmap->mipmap_count;
    meta.miscFlags = 8000000; // i think this enables tile mode // the only flag seems to be TEX_MISC_TEXTURECUBE = 0x4
    meta.miscFlags2 = 0;
    meta.dimension = (DirectX::TEX_DIMENSION)3; // TEX_DIMENSION_TEXTURE2D
    meta.width = width;
    meta.height = height;

    // test format
    uint32_t bitm_format = GetFormat(format);
    if (bitm_format == -1) throw exception("DXGI format specified by the tag was either unsupported or invalid");
    meta.format = (DXGI_FORMAT)bitm_format;

    // write header
    DDSheader_dest = new char[header_size + data_size];
    size_t output_size = 0;
    HRESULT hr = EncodeDDSHeader(meta, DirectX::DDS_FLAGS_NONE, (void*)DDSheader_dest, header_size, output_size);
    if (!SUCCEEDED(hr)) throw exception("image failed to generate DDS header");

    // write the bitmap data into our dds 
    memcpy(DDSheader_dest + header_size, data, data_size);

    // fixup header data to convert it into xbox DDS header data
    DDS_HEADER_p* encoded_header = (DDS_HEADER_p*)(DDSheader_dest + sizeof(uint32_t));
    encoded_header->ddspf.flags |= 4; // DDS_FOURCC
    encoded_header->ddspf.fourCC = 0x584F4258; // 'XBOX' backwards

    DDS_HEADER_XBOX_p* encoded_xbox_header = (DDS_HEADER_XBOX_p*)(DDSheader_dest + sizeof(uint32_t) + sizeof_DDS_HEADER); // sizeof(DirectX::DDS_HEADER);
    encoded_xbox_header->dxgiFormat = meta.format;
    encoded_xbox_header->resourceDimension = meta.dimension;
    encoded_xbox_header->miscFlag = meta.miscFlags & ~4; // TEX_MISC_TEXTURECUBE; // this is just what is done in the regular encode
    encoded_xbox_header->arraySize = meta.arraySize;
    encoded_xbox_header->miscFlags2 = meta.miscFlags2;
    encoded_xbox_header->tileMode = Xbox::c_XboxTileModeLinear; // this may not be correct
    encoded_xbox_header->baseAlignment = 32768; // this is the value that the Xbox layout is saying it should be?? // can be anything other than 0??
    encoded_xbox_header->dataSize = data_size;
    encoded_xbox_header->xdkVer = 0;

    Xbox::XboxImage xbox_image = {};
    hr = Xbox::LoadFromDDSMemory(DDSheader_dest, header_size + data_size, nullptr, xbox_image);
    if (!SUCCEEDED(hr)) throw exception("failed to load xbox encoded image");

    DirectX::ScratchImage untiled_image = {};
    hr = Xbox::Detile(xbox_image, untiled_image);
    if (!SUCCEEDED(hr)) throw exception("failed to detile xbox image");


    // return data back to source array
    if (untiled_image.GetPixelsSize() != data_size) throw exception("detiled image data did not match original data length");

    memcpy(data, untiled_image.GetPixels(), untiled_image.GetPixelsSize());
}


DLL1_API int fnDll1(char* data, uint32_t data_size, uint32_t width, uint32_t height, uint8_t format) {

    int result = 0;
    char* cleanup_ptr = nullptr;
    try { detile_data(data, data_size, width, height, format, cleanup_ptr); }
    catch (exception ex) {
        result = -1; // failed
        if (cleanup_ptr) delete[] cleanup_ptr;
        throw;
    }

    if (cleanup_ptr) delete[] cleanup_ptr;
    return result;
}