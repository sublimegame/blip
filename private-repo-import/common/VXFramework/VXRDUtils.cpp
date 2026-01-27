
#include "VXRDUtils.hpp"

#ifndef P3S_CLIENT_HEADLESS

// C++
#include <cmath>

// xptools
#include "filesystem.hpp"
#include "vxlog.h"

using namespace vx;
using namespace rendering;

// MARK: - BX File Reader -

BXFileReader::BXFileReader(): _fp(nullptr) {}

BXFileReader::~BXFileReader() {
    close();
}

bool BXFileReader::open(const bx::FilePath &filepath, bx::Error *err) {
    _fp = vx::fs::openBundleFile(filepath.getCPtr());
    if (_fp == nullptr) {
        vxlog_error( "⚠️ BXFileReader: failed to open file");
        return false;
    }
    return true;
}

void BXFileReader::close() {
    if (_fp != nullptr) {
        fclose(_fp);
        _fp = nullptr;
    }
}

int64_t BXFileReader::seek(int64_t offset, bx::Whence::Enum whence) {
    if (_fp == nullptr) {
        return -1;
    }
    fseek(_fp, offset, GetWhence(whence));
    return static_cast<int64_t>(ftell(_fp));
}

int32_t BXFileReader::read(void *data, int32_t size, bx::Error *err) {
    if (_fp == nullptr) {
        vxlog_error( "⚠️ BXFileReader: failed to read file");
        return 0;
    }
    return static_cast<int32_t>(fread(data, 1, (size_t)size, _fp));
}

int BXFileReader::GetWhence(bx::Whence::Enum _whence) {
    switch (_whence) {
        case bx::Whence::Begin:
            return SEEK_SET;
        case bx::Whence::Current:
            return SEEK_CUR;
        case bx::Whence::End:
            return SEEK_END;
    }
}

// MARK: - BX File Writer -

BXFileWriter::BXFileWriter(): _fp(nullptr) {}

BXFileWriter::~BXFileWriter() {
    close();
}

bool BXFileWriter::open(const bx::FilePath &filepath, bool append, bx::Error *err) {
    _fp = vx::fs::openStorageFile(filepath.getCPtr(), append ? "ab" : "wb");
    if (_fp == nullptr) {
        vxlog_error( "⚠️ BXFileWriter: failed to open file");
        return false;
    }
    return true;
}

void BXFileWriter::close() {
    if (_fp != nullptr) {
        fclose(_fp);
        _fp = nullptr;
    }
}

int64_t BXFileWriter::seek(int64_t offset, bx::Whence::Enum whence) {
    if (_fp == nullptr) {
        return -1;
    }
    fseek(_fp, offset, GetWhence(whence));
    return static_cast<int64_t>(ftell(_fp));
}

int32_t BXFileWriter::write(const void *data, int32_t size, bx::Error *err) {
    if (_fp == nullptr) {
        vxlog_error( "⚠️ BXFileWriter: failed to write file");
        return 0;
    }
    return static_cast<int32_t>(fwrite(data, 1, (size_t)size, _fp));
}

int BXFileWriter::GetWhence(bx::Whence::Enum _whence) {
    switch (_whence) {
        case bx::Whence::Begin:
            return SEEK_SET;
        case bx::Whence::Current:
            return SEEK_CUR;
        case bx::Whence::End:
            return SEEK_END;
    }
}

// MARK: - Image -

/// note: doesn't compress PNG
void Utils::savePng(const char* filepath, uint32_t w, uint32_t h, uint32_t srcPitch, const void* src, bimg::TextureFormat::Enum format, bool yflip) {
    BXFileWriter writer;
    bx::Error err;
    if (bx::open(&writer, filepath, false, &err)) {
        bimg::imageWritePng(&writer, w, h, srcPitch, src, format, yflip, &err);
        bx::close(&writer);
    }
}

void Utils::savePng(std::string filename, uint32_t w, uint32_t h, const void* src, bool yflip, void** outData, size_t* outSize, int bitDepth, bool doWrite) {
    // content is expected as an array of row pointers
    png_bytep *row_pointers = static_cast<png_bytep *>(malloc(h * sizeof(png_bytep)));
    png_byte *data = (png_byte*)src;
    const uint16_t pitch = w * 4 * bitDepth / 8;
    for (uint32_t i = 0; i < h; i++) {
        row_pointers[i] = &data[(yflip ? h - 1 - i : i) * pitch];
    }

    vx::fs::writePng(filename.append(".png"), w, h, row_pointers, outData, outSize, bitDepth, doWrite);

    free(row_pointers);
}

void Utils::saveTga(const char* filepath, uint32_t w, uint32_t h, uint32_t srcPitch, const void* src, bool grayscale, bool yflip) {
    BXFileWriter writer;
    bx::Error err;
    if (bx::open(&writer, filepath, false, &err)) {
        bimg::imageWriteTga(&writer, w, h, srcPitch, src, grayscale, yflip, &err);
        bx::close(&writer);
    }
}

// note: point downscale discards texels
void Utils::downscalePoint(void *src, uint16_t srcWidth, uint16_t srcHeight,
                           void **dst, uint16_t dstWidth, uint16_t dstHeight, uint16_t texelBytes) {

    if (*dst != nullptr) free(*dst);
    *dst = nullptr;
    
    if (srcWidth < dstWidth || srcHeight < dstHeight) {
        return;
    }
    
    *dst = malloc(dstWidth * dstHeight * 4);

    const float wScale = static_cast<float>(srcWidth) / static_cast<float>(dstWidth);
    const float hScale = static_cast<float>(srcHeight) / static_cast<float>(dstHeight);
    uint8_t *s = static_cast<uint8_t*>(src);
    uint8_t *d = static_cast<uint8_t*>(*dst);
    for (int h = 0; h < dstHeight; h++) {
        for (int w = 0; w < dstWidth; w++) {
            memcpy(d + (h * dstWidth + w) * texelBytes,
                   s + (static_cast<uint32_t>(floorf(hScale * static_cast<float>(h))) * srcWidth
                    + static_cast<uint32_t>(floorf(wScale * static_cast<float>(w)))) * texelBytes,
                   texelBytes);
        }
    }
}

void Utils::trim(void *src, const uint16_t srcWidth, const uint16_t srcHeight,
                 void **dst, uint16_t& dstWidth, uint16_t& dstHeight,
                 uint16_t texelBytes) {
    
    if (*dst != nullptr) free(*dst);
    *dst = nullptr;
    
    uint16_t startW = srcWidth;
    uint16_t endW = 0;
    
    uint16_t startH = srcHeight;
    uint16_t endH = 0;
    
    const uint8_t *s = static_cast<const uint8_t*>(src);
    const uint8_t *texel;
    
    bool foundOneNonTransparentPixel = false;
    
    for (int h = 0; h < srcHeight; ++h) {
        for (int w = 0; w < srcWidth; ++w) {
            
            texel = s + ((h * srcWidth + w) * texelBytes);
            if (texel[3] != 0) { // not transparent
                foundOneNonTransparentPixel = true;
                if (w <= startW) startW = w;
                if (w > endW) endW = w;
                if (h <= startH) startH = h;
                if (h > endH) endH = h;
            }
        }
    }
    
    if (foundOneNonTransparentPixel == false) {
        dstWidth = srcWidth;
        dstHeight = srcHeight;
        return;
    }
    
    dstWidth = endW - startW;
    dstHeight = endH - startH;

    *dst = malloc(dstWidth * dstHeight * texelBytes);
    uint8_t *d = static_cast<uint8_t*>(*dst);
    
    for (int h = 0; h < dstHeight; ++h) {
        for (int w = 0; w < dstWidth; ++w) {
            memcpy(d + (h * dstWidth + w) * texelBytes,
                   s + ((h + startH) * srcWidth + (w + startW)) * texelBytes,
                   texelBytes);
        }
    }
}

void Utils::expand(const void *src, const uint16_t srcWidth, const uint16_t srcHeight,
                   void **dst, uint16_t& dstWidth, uint16_t& dstHeight,
                   uint16_t texelBytes) {
    
    if (dstWidth < srcWidth) dstWidth = srcWidth;
    if (dstHeight < srcHeight) dstHeight = srcHeight;
    
    uint16_t paddingLeft = (dstWidth - srcWidth) / 2;
    uint16_t paddingTop = (dstHeight - srcHeight) / 2;
    
    const uint8_t *s = static_cast<const uint8_t*>(src);
    
    if (*dst != nullptr) free(*dst);
    *dst = malloc(dstWidth * dstHeight * texelBytes);
    uint8_t *d = static_cast<uint8_t*>(*dst);
    
    uint8_t transparent[4] = {0, 0, 0, 0};
    
    for (int h = 0; h < dstHeight; ++h) {
        for (int w = 0; w < dstWidth; ++w) {
            if (w < paddingLeft || w >= paddingLeft + srcWidth ||
                h < paddingTop || h >= paddingTop + srcHeight) {
             
                memcpy(d + (h * dstWidth + w) * texelBytes,
                       transparent,
                       texelBytes);
                
            } else {
                memcpy(d + (h * dstWidth + w) * texelBytes,
                       s + ((h - paddingTop) * srcWidth + (w - paddingLeft)) * texelBytes,
                       texelBytes);
            }
        }
    }
}

void Utils::crop(const void *src, const uint16_t srcWidth, const uint16_t srcHeight,
                 void **dst, uint16_t& dstWidth, uint16_t& dstHeight,
                 uint16_t texelBytes) {
    
    // can't crop to make it bigger,
    // making sure of this:
    if (dstWidth > srcWidth) dstWidth = srcWidth;
    if (dstHeight > srcHeight) dstHeight = srcHeight;
    
    uint16_t startLeft = (srcWidth - dstWidth) / 2;
    uint16_t startTop = (srcHeight - dstHeight) / 2;
    
    const uint8_t *s = static_cast<const uint8_t*>(src);
    
    if (*dst != nullptr) free(*dst);
    *dst = malloc(dstWidth * dstHeight * texelBytes);
    uint8_t *d = static_cast<uint8_t*>(*dst);
    
    for (int h = 0; h < dstHeight; ++h) {
        for (int w = 0; w < dstWidth; ++w) {
            memcpy(d + (h * dstWidth + w) * texelBytes,
                   s + ((h + startTop) * srcWidth + (w + startLeft)) * texelBytes,
                   texelBytes);
        }
    }
}

// MARK: - Mesh -

void Utils::generateUVSphere(int slices, int stacks, float **vertices, uint16_t **triangles, size_t *nbVertices, size_t *nbTriangles) {
    *nbVertices = (stacks - 1) * slices + 2;
    *nbTriangles = (stacks - 1) * slices * 2;

    *vertices = static_cast<float *>(malloc(*nbVertices * sizeof(float) * 3));
    *triangles = static_cast<uint16_t *>(malloc(*nbTriangles * sizeof(uint16_t) * 3));

    // add top vertex
    (*vertices)[0] = 0.0f;
    (*vertices)[1] = 1.0f;
    (*vertices)[2] = 0.0f;

    // generate vertices per stack / slice
    int idx = 1;
    for (int i = 0; i < stacks - 1; ++i) {
        auto phi = M_PI * double(i + 1) / double(stacks);
        for (int j = 0; j < slices; ++j) {
            const double theta = 2.0 * M_PI * double(j) / double(slices);
            const float x = std::sin(phi) * std::cos(theta);
            const float y = std::cos(phi);
            const float z = std::sin(phi) * std::sin(theta);
            (*vertices)[idx * 3] = x;
            (*vertices)[idx * 3 + 1] = y;
            (*vertices)[idx * 3 + 2] = z;
            ++idx;
        }
    }

    // add bottom vertex
    (*vertices)[idx * 3] = 0.0f;
    (*vertices)[idx * 3 + 1] = -1.0f;
    (*vertices)[idx * 3 + 2] = 0.0f;
    const int lastVertIdx = idx;

    // add top / bottom triangles
    idx = 0;
    for (int i = 0; i < slices; ++i) {
        int i0 = i + 1;
        int i1 = (i + 1) % slices + 1;
        (*triangles)[idx * 3] = 0;
        (*triangles)[idx * 3 + 1] = i0;
        (*triangles)[idx * 3 + 2] = i1;
        ++idx;

        i0 = i + slices * (stacks - 2) + 1;
        i1 = (i + 1) % slices + slices * (stacks - 2) + 1;
        (*triangles)[idx * 3] = lastVertIdx;
        (*triangles)[idx * 3 + 1] = i1;
        (*triangles)[idx * 3 + 2] = i0;
        ++idx;
    }

    // add quads per stack / slice
    for (int j = 0; j < stacks - 2; ++j) {
        int j0 = j * slices + 1;
        int j1 = (j + 1) * slices + 1;
        for (int i = 0; i < slices; ++i) {
            int i0 = j0 + i;
            int i1 = j0 + (i + 1) % slices;
            int i2 = j1 + (i + 1) % slices;
            int i3 = j1 + i;

            (*triangles)[idx * 3] = i0;
            (*triangles)[idx * 3 + 1] = i2;
            (*triangles)[idx * 3 + 2] = i1;
            ++idx;

            (*triangles)[idx * 3] = i0;
            (*triangles)[idx * 3 + 1] = i3;
            (*triangles)[idx * 3 + 2] = i2;
            ++idx;
        }
    }
}

// MARK: - BGFX custom callback handler -

BgfxCallback::~BgfxCallback() = default;

void BgfxCallback::fatal(const char* filepath, uint16_t line, bgfx::Fatal::Enum code, const char* str) {
    vxlog_error("⚠️BgfxCallback fatal error: 0x%08x: %s", code, str);
    abort();
}

void BgfxCallback::traceVargs(const char* filepath, uint16_t line, const char* format, va_list argList) {
    // from CallbackStub (bgfx.cpp)
    char temp[2048];
    char* out = temp;
    va_list argListCopy;
    va_copy(argListCopy, argList);
    int32_t len   = bx::snprintf(out, sizeof(temp), "%s (%d): ", filepath, line);
    int32_t total = len + bx::vsnprintf(out + len, sizeof(temp)-len, format, argListCopy);
    va_end(argListCopy);
    if ((int32_t)sizeof(temp) < total) {
        out = (char*)alloca(total+1);
        bx::memCopy(out, temp, len);
        bx::vsnprintf(out + len, total-len, format, argList);
    }
    out[total] = '\0';
#if VX_BGFX_LOG
    vxlog_debug(out);
#endif
#ifdef BGFX_CONFIG_DEBUG
    bx::debugOutput(out);
#endif
}

void BgfxCallback::profilerBegin(const char* name, uint32_t abgr, const char* filepath, uint16_t line) {}
void BgfxCallback::profilerBeginLiteral(const char* name, uint32_t abgr, const char* filePath, uint16_t line) {}
void BgfxCallback::profilerEnd() {}

uint32_t BgfxCallback::cacheReadSize(uint64_t id) { return 0; }
bool BgfxCallback::cacheRead(uint64_t id, void* data, uint32_t size) { return false; }
void BgfxCallback::cacheWrite(uint64_t id, const void* data, uint32_t size) {}

void BgfxCallback::screenShot(const char* filePath, uint32_t w, uint32_t h, uint32_t pitch, const void* data, uint32_t size, bool yflip) {
    char temp[1024];
    bx::snprintf(temp, BX_COUNTOF(temp), "%s.png", filePath);
    Utils::savePng(temp, w, h, pitch, data, bimg::TextureFormat::BGRA8, yflip);
    vxlog_info("💾 screenshot saved %s", filePath);
}

void BgfxCallback::captureBegin(uint32_t w, uint32_t h, uint32_t pitch, bgfx::TextureFormat::Enum format, bool yflip) {}
void BgfxCallback::captureEnd() {}
void BgfxCallback::captureFrame(const void* data, uint32_t size) {}

#endif
