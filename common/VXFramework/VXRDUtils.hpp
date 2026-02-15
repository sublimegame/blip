
#pragma once

// C
#include <stdio.h>
#include <string>

#ifndef P3S_CLIENT_HEADLESS

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weverything"
#include <bx/readerwriter.h>
#include <bx/file.h>
#include <bimg/bimg.h>
#include <bgfx/bgfx.h>
#include <bx/debug.h>
#pragma GCC diagnostic pop

#define VX_BGFX_LOG false

namespace vx {
    namespace rendering {
        //// Mapping our filesystem to bgfx for loading shader files from app bundle
        class BXFileReader : public bx::FileReaderI {
        public:
            BXFileReader();
            ~BXFileReader() override;
            bool open(const bx::FilePath &filepath, bx::Error *err) override;
            void close() override;
            int64_t seek(int64_t offset = 0, bx::Whence::Enum whence = bx::Whence::Current) override;
            int32_t read(void *data, int32_t size, bx::Error *err) override;
        private:
            FILE* _fp;
            int GetWhence(bx::Whence::Enum _whence);
        };

        //// Mapping our filesystem to bgfx for writing files in storage
        class BXFileWriter : public bx::FileWriterI {
        public:
            BXFileWriter();
            ~BXFileWriter() override;
            bool open(const bx::FilePath &filepath, bool append, bx::Error *err) override;
            void close() override;
            int64_t seek(int64_t offset = 0, bx::Whence::Enum whence = bx::Whence::Current) override;
            int32_t write(const void *data, int32_t size, bx::Error *err) override;
        private:
            FILE* _fp;
            int GetWhence(bx::Whence::Enum _whence);
        };

        //// Bgfx custom callback handler
        struct BgfxCallback : public bgfx::CallbackI {
            ~BgfxCallback() override;

            /// Unrecoverable error, inform the user and terminate the application
            void fatal(const char* filepath, uint16_t line, bgfx::Fatal::Enum code, const char* str) override;
            /// Print debug message
            void traceVargs(const char* filepath, uint16_t line, const char* format, va_list argList) override;

            /// NOT IMPLEMENTED
            /// Profiling
            void profilerBegin(const char* name, uint32_t abgr, const char* filepath, uint16_t line) override;
            void profilerBeginLiteral(const char* name, uint32_t abgr, const char* filePath, uint16_t line) override;
            void profilerEnd() override;

            /// NOT IMPLEMENTED
            /// Shader cache used for OpenGL and Direct3D (check CallbackI), if needed, check bgfx callback example
            uint32_t cacheReadSize(uint64_t id) override;
            virtual bool cacheRead(uint64_t id, void* data, uint32_t size) override;
            virtual void cacheWrite(uint64_t id, const void* data, uint32_t size) override;

            /// Write screenshot to file
            /// Note: currently unused since we have GameRenderer::screenshot which allow optional draw and does not depend on current rendering
            virtual void screenShot(const char* filePath, uint32_t w, uint32_t h, uint32_t pitch, const void* data, uint32_t size, bool yflip) override;

            /// NOT IMPLEMENTED
            /// Recording a video, if needed, check AviWriter in bgfx example
            void captureBegin(uint32_t w, uint32_t h, uint32_t pitch, bgfx::TextureFormat::Enum format, bool yflip) override;
            virtual void captureEnd() override;
            virtual void captureFrame(const void* data, uint32_t size) override;
        };

        class Utils {
        public:
            // MARK: - Image -

            /// bimg
            static void savePng(const char* filepath, uint32_t w, uint32_t h, uint32_t srcPitch, const void* src, bimg::TextureFormat::Enum format, bool yflip);
            static void saveTga(const char* filepath, uint32_t w, uint32_t h, uint32_t srcPitch, const void* src, bool grayscale, bool yflip);
            /// lpng
            static void savePng(std::string filename, uint32_t w, uint32_t h, const void* src, bool yflip,
                    void** outData=nullptr, size_t* outSize=nullptr, int bitDepth=8, bool doWrite=true);

            /// Image utils
            static void downscalePoint(void *src, uint16_t srcWidth, uint16_t srcHeight,
                    void **dst, uint16_t dstWidth, uint16_t dstHeight, uint16_t texelBytes);

            static void trim(void *src, const uint16_t srcWidth, const uint16_t srcHeight,
                             void **dst, uint16_t& dstWidth, uint16_t& dstHeight,
                             uint16_t texelBytes);

            static void expand(const void *src, const uint16_t srcWidth, const uint16_t srcHeight,
                             void **dst, uint16_t& dstWidth, uint16_t& dstHeight,
                             uint16_t texelBytes);

            static void crop(const void *src, const uint16_t srcWidth, const uint16_t srcHeight,
                             void **dst, uint16_t& dstWidth, uint16_t& dstHeight,
                             uint16_t texelBytes);

            // MARK: - Mesh -

            static void generateUVSphere(int slices, int stacks, float **vertices, uint16_t **triangles, size_t *nbVertices, size_t *nbTriangles);
        };
    }
}

#else

namespace vx {
    namespace rendering {
        struct BgfxCallback {};
    }
}

#endif
