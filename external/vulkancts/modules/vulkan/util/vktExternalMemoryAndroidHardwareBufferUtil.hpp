#ifndef _VKTEXTERNALMEMORYANDROIDHARDWAREBUFFERUTIL_HPP
#define _VKTEXTERNALMEMORYANDROIDHARDWAREBUFFERUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
 * Copyright (c) 2023 Google Inc.
 * Copyright (c) 2023 LunarG Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Vulkan external memory utilities for Android Hardware Buffer
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"

#include "vkPlatform.hpp"
#include "vkRefUtil.hpp"

#include "tcuTexture.hpp"
#include "tcuCompressedTexture.hpp"

#include <vector>

#ifndef CTS_USES_VULKANSC

namespace vkt
{

namespace ExternalMemoryUtil
{

class AndroidHardwareBufferExternalApi
{
public:
    /**
     * getInstance obtains the object, that provides an interface to AHB system APIs .
     * If the AHB system API is not supported or if it is not built as supported with the CTS,
     * then this function would return a null object.
     */
    static AndroidHardwareBufferExternalApi *getInstance();

    /* Is AndroidHardwareBuffer supported? */
    static bool supportsAhb();

    /* Are Cube maps supported on current api level? */
    static bool supportsCubeMap();

    /**
     * Allocates a buffer that backs an AHardwareBuffer using the passed parameter as follows:
     * width;      - width in pixels
     * height;     - height in pixels
     * layers;     - number of images
     * format;     - One of AHARDWAREBUFFER_FORMAT_*
     * usage;      - Combination of AHARDWAREBUFFER_USAGE_*
     *
     * Returns a valid AndroidHardwareBufferPtr object on success, or an null AndroidHardwareBufferPtr if
     * the allocation fails for any reason.
     */
    virtual vk::pt::AndroidHardwareBufferPtr allocate(uint32_t width, uint32_t height, uint32_t layers, uint32_t format,
                                                      uint64_t usage) = 0;

    /**
     * Acquire a reference on the given AHardwareBuffer object.  This prevents the
     * object from being deleted until the last reference is removed.
     */
    virtual void acquire(vk::pt::AndroidHardwareBufferPtr buffer) = 0;

    /**
     * Remove a reference that was previously acquired with
     * AHardwareBuffer_acquire().
     */
    virtual void release(vk::pt::AndroidHardwareBufferPtr buffer) = 0;

    /**
     * Return a description of the AHardwareBuffer in the passed in the following fields, if not NULL:
     * width;      - width in pixels
     * height;     - height in pixels
     * layers;     - number of images
     * format;     - One of AHARDWAREBUFFER_FORMAT_*
     * usage;      - Combination of AHARDWAREBUFFER_USAGE_*
     *
     */
    virtual void describe(const vk::pt::AndroidHardwareBufferPtr buffer, uint32_t *width, uint32_t *height,
                          uint32_t *layers, uint32_t *format, uint64_t *usage, uint32_t *stride) = 0;

    /**
     * Return a pointer to buffer data for CPU read. nullptr is returned on failure.
     * Buffer must have been created with usage flags.
     */
    virtual void *lock(vk::pt::AndroidHardwareBufferPtr buffer, uint64_t usage) = 0;

    /**
     * Returns TRUE if locking the buffer for buffer data for CPU read was successful, FALSE otherwise.
     * Buffer must have been created with usage flags.
     * Out parameters will be be filled with required data to access each plane according to planeCountOut.
     * Plane 0 will have all data in planeDataOut[0], planeStrideOut[0], planeRowStrideOut[0]
     * Plane 1 will have all data in planeDataOut[1], planeStrideOut[1], planeRowStrideOut[1]
     * ...
     * planeCount            - number of planes present in buffer
     * planeDataOut            - array of pointers to plane data
     * planePixelStrideOut    - plane stride for each pixel in a row
     * planeRowStrideOut    - plane stride for each row
     */
    virtual bool lockPlanes(vk::pt::AndroidHardwareBufferPtr buffer, uint64_t usage, uint32_t &planeCountOut,
                            void *planeDataOut[4], uint32_t planePixelStrideOut[4], uint32_t planeRowStrideOut[4]) = 0;

    /**
     * Returns TRUE if buffer was unlocked successfully from previous lock operations.
     * FALSE is returned otherwise.
     */
    virtual bool unlock(vk::pt::AndroidHardwareBufferPtr buffer) = 0;

    virtual uint64_t vkUsageToAhbUsage(vk::VkImageUsageFlagBits vkFlag)   = 0;
    virtual uint64_t vkCreateToAhbUsage(vk::VkImageCreateFlagBits vkFlag) = 0;
    virtual uint32_t vkFormatToAhbFormat(vk::VkFormat vkFormat)           = 0;
    virtual uint64_t mustSupportAhbUsageFlags()                           = 0;
    virtual bool ahbFormatIsBlob(uint32_t format)                         = 0;
    virtual bool ahbFormatIsYuv(uint32_t format)                          = 0;

    /* Retrieves all present formats in AHB */
    virtual std::vector<uint32_t> getAllSupportedFormats() = 0;

    /* AHB format as a string */
    virtual const char *getFormatAsString(uint32_t format) = 0;

    virtual ~AndroidHardwareBufferExternalApi();

protected:
    // Protected Constructor
    AndroidHardwareBufferExternalApi();

private:
    // Stop the compiler generating methods of copy the object
    AndroidHardwareBufferExternalApi(AndroidHardwareBufferExternalApi const &copy);            // Not Implemented
    AndroidHardwareBufferExternalApi &operator=(AndroidHardwareBufferExternalApi const &copy); // Not Implemented

    static bool loadAhbDynamicApis(int32_t sdkVersion);
};

// Buffer class that allows CPU read/writes to Android Hardware Buffers
class AndroidHardwareBufferInstance
{
    // Class/struct/enum/... definitions
public:
    // Can check if format is supported using isFormatSupported
    enum Format : uint32_t
    {
        // Formats exposed by Native Hardware Buffer API
        R8G8B8A8_UNORM = 0,
        R8G8B8X8_UNORM,
        R8G8B8_UNORM,
        R5G6B5_UNORM,
        R16G16B16A16_FLOAT,
        R10G10B10A2_UNORM,
        BLOB,
        D16_UNORM,
        D24_UNORM,
        D24_UNORM_S8_UINT,
        D32_FLOAT,
        D32_FLOAT_S8_UINT, // No CPU side validation available through AHB
        S8_UINT,
        Y8Cb8Cr8_420,
        YCbCr_P010,
        R8_UNORM,
        R16_UINT,
        R16G16_UINT,
        R10G10B10A10_UNORM,

        // Formats not exposed by Native Hardware Buffer API
        // Present in Android Hardware Buffer
        // Values obtained AOSP header (nativewindow/include/vndk/hardware_buffer.h)
        B8G8R8A8_UNORM,
        YV12,
        Y8,
        Y16,
        RAW10,
        RAW12,
        RAW16,
        RAW_OPAQUE,             // No validation possible
        IMPLEMENTATION_DEFINED, // No validation possible
        NV16,                   // AHARDWAREBUFFER_FORMAT_YCbCr_422_SP
        NV21,                   // AHARDWAREBUFFER_FORMAT_YCrCb_420_SP
        YUY2,                   // AHARDWAREBUFFER_FORMAT_YCbCr_422_I

        COUNT,
        UNASSIGNED
    };

    // Can be expanded as needed
    enum Usage : uint32_t
    {
        UNUSED          = 0,
        GPU_FRAMEBUFFER = 1,
        GPU_SAMPLED     = 2,
        CPU_READ        = 4,
        CPU_WRITE       = 8,
    };

    enum ChromaLocation : uint32_t
    {
        COSITED_EVEN = 0, // VK_CHROMA_LOCATION_COSITED_EVEN
        MIDPOINT,         // VK_CHROMA_LOCATION_MIDPOINT
    };

protected:
    struct AccessDataCPU
    {
        uint32_t m_planeCount = 0u;
        union
        {
            uint8_t *m_planeData[4] = {nullptr, nullptr, nullptr, nullptr};
            void *m_planeDataVoid[4];
        };
        uint32_t m_planePixelStride[4] = {0u, 0u, 0u, 0u};
        uint32_t m_planeRowStride[4]   = {0u, 0u, 0u, 0u};
    };

    using AhbApi = AndroidHardwareBufferExternalApi;
    using Handle = vk::pt::AndroidHardwareBufferPtr;

    // Static functions
public:
    static int32_t getSdkVersion();
    static bool isFormatSupported(Format format);
    static bool isFormatYuv(Format format);
    static bool isFormatRaw(Format format);
    static bool isFormatColor(Format format);
    static bool isFormatDepth(Format format);
    static bool isFormatStencil(Format format);
    static bool hasFormatAlpha(Format format);
    static const char *getFormatName(Format format);
    static uint32_t formatToInternalFormat(Format format);
    static tcu::TextureFormat formatToTextureFormat(Format format);
    static ChromaLocation vkChromaLocationToChromaLocation(vk::VkChromaLocation location);
    static void reduceYuvTexture(tcu::TextureLevel &texture, Format format, ChromaLocation xChroma,
                                 ChromaLocation yChroma);

protected:
    static void reduceYuv420Texture(tcu::TextureLevel &texture, ChromaLocation xChroma, ChromaLocation yChroma);
    static void reduceYuv422Texture(tcu::TextureLevel &texture, ChromaLocation xChroma);
    static uint64_t usageToInternalUsage(Usage usage);
    // Pixel stride in bytes
    static uint32_t pixelStride(Format format);

    // Member functions
public:
    ~AndroidHardwareBufferInstance(void);

    bool allocate(Format format, uint32_t width, uint32_t height, uint32_t layers, Usage usage);
    void release(void);

    bool lock(Usage usage);
    bool unlock(void);

    void copyCpuBufferToAndroidBuffer(const tcu::TextureLevel &cpuBuffer);
    void copyCpuBufferToAndroidBufferCompressed(const tcu::CompressedTexture &cpuBuffer);
    void copyAndroidBufferToCpuBuffer(tcu::TextureLevel &cpuBuffer) const;
    void copyAndroidBufferToCpuBufferCompressed(tcu::CompressedTexture &cpuBuffer) const;

    tcu::TextureFormat getAhbTextureFormat(void) const
    {
        return formatToTextureFormat(m_format);
    };
    Handle getHandle(void) const
    {
        return m_handle;
    }

    bool isYuv(void) const;
    bool isRaw(void) const;
    bool hasDepth(void) const;
    bool hasStencil(void) const;

    // Data
protected:
    AccessDataCPU m_accessData;
    AhbApi *m_ahbApi          = AhbApi::getInstance();
    Handle m_handle           = Handle(nullptr);
    Usage m_usage             = UNUSED;
    uint64_t m_internalUsage  = 0u;
    Format m_format           = UNASSIGNED;
    uint32_t m_internalFormat = 0u;
    uint32_t m_width          = 0u;
    uint32_t m_height         = 0u;
    uint32_t m_layers         = 0u;
};

} // namespace ExternalMemoryUtil

} // namespace vkt

#endif // CTS_USES_VULKANSC

#endif // _VKTEXTERNALMEMORYANDROIDHARDWAREBUFFERUTIL_HPP
