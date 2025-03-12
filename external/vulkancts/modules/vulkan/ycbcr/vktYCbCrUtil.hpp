#ifndef _VKTYCBCRUTIL_HPP
#define _VKTYCBCRUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 Google Inc.
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
 * \brief YCbCr Test Utilities
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"

#include "vktTestCase.hpp"

#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkRef.hpp"

#include "deSharedPtr.hpp"
#include "deRandom.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuFloatFormat.hpp"
#include "tcuFloat.hpp"
#include "tcuInterval.hpp"
#include "tcuFloatFormat.hpp"
#include "tcuFloat.hpp"

#include <vector>
#include <fstream>

namespace vkt
{
namespace ycbcr
{

#define VK_YCBCR_FORMAT_FIRST VK_FORMAT_G8B8G8R8_422_UNORM
#define VK_YCBCR_FORMAT_LAST ((vk::VkFormat)(VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM + 1))

typedef de::SharedPtr<vk::Allocation> AllocationSp;
typedef de::SharedPtr<vk::Unique<vk::VkBuffer>> VkBufferSp;

class MultiPlaneImageData
{
public:
    MultiPlaneImageData(vk::VkFormat format, const tcu::UVec2 &size);
    MultiPlaneImageData(const MultiPlaneImageData &);
    ~MultiPlaneImageData(void);

    vk::VkFormat getFormat(void) const
    {
        return m_format;
    }
    const vk::PlanarFormatDescription &getDescription(void) const
    {
        return m_description;
    }
    const tcu::UVec2 &getSize(void) const
    {
        return m_size;
    }

    size_t getPlaneSize(uint32_t planeNdx) const
    {
        return m_planeData[planeNdx].size();
    }
    void *getPlanePtr(uint32_t planeNdx)
    {
        return &m_planeData[planeNdx][0];
    }
    const void *getPlanePtr(uint32_t planeNdx) const
    {
        return &m_planeData[planeNdx][0];
    }

    tcu::PixelBufferAccess getChannelAccess(uint32_t channelNdx);
    tcu::ConstPixelBufferAccess getChannelAccess(uint32_t channelNdx) const;

private:
    MultiPlaneImageData &operator=(const MultiPlaneImageData &);

    const vk::VkFormat m_format;
    const vk::PlanarFormatDescription m_description;
    const tcu::UVec2 m_size;

    std::vector<uint8_t> m_planeData[vk::PlanarFormatDescription::MAX_PLANES];
};

template <typename planeType>
class YCbCrContent
{

public:
    YCbCrContent()
    {
    }

    ~YCbCrContent()
    {
    }

    static de::MovePtr<std::vector<planeType>> getFrame(std::string fileName, uint32_t width, uint32_t height,
                                                        int frameIndex)
    {
        tcu::FileResource fileResource(fileName.c_str());
        uint32_t uOffset   = width * height;
        uint32_t frameSize = uOffset + (uOffset / 2);
        uint32_t position  = frameSize * frameIndex;
        de::MovePtr<std::vector<planeType>> content(new std::vector<planeType>(0));

        if (position > fileResource.getSize())
        {
            TCU_THROW(NotSupportedError, "Position is higher than the file size, check the frame index provided");
        }

        fileResource.setPosition(position * sizeof(planeType));
        content->resize(frameSize);
        fileResource.read(content->data(), frameSize);
        return content;
    }

    static bool save(const std::vector<planeType> &data, const std::string &outputFileName)
    {
        std::ofstream outFile(outputFileName, std::ios::binary | std::ios::out);

        if (!outFile.is_open())
        {
            std::cerr << "Error: Unable to open output file '" << outputFileName << "'." << std::endl;
            return false;
        }

        if (data.empty())
        {
            std::cerr << "Error: Data is empty or doesn't exist" << std::endl;
            return false;
        }

        outFile.write(reinterpret_cast<const char *>(data.data()), data.size() * sizeof(planeType));
        outFile.close();

        return true;
    }
};

void extractI420Frame(std::vector<uint8_t> &videoDataPtr, uint32_t frameNumber, uint32_t width, uint32_t height,
                      vkt::ycbcr::MultiPlaneImageData *imageData, bool half_size);

void checkImageSupport(Context &context, vk::VkFormat format, vk::VkImageCreateFlags createFlags,
                       vk::VkImageTiling tiling = vk::VK_IMAGE_TILING_OPTIMAL);

void fillRandomNoNaN(de::Random *randomGen, uint8_t *const data, uint32_t size, const vk::VkFormat format);
void fillRandom(de::Random *randomGen, MultiPlaneImageData *imageData,
                const vk::VkFormat format = vk::VK_FORMAT_UNDEFINED, bool noNan = false);
void fillGradient(MultiPlaneImageData *imageData, const tcu::Vec4 &minVal, const tcu::Vec4 &maxVal);
void fillZero(MultiPlaneImageData *imageData);

std::vector<de::SharedPtr<vk::Allocation>> allocateAndBindImageMemory(
    const vk::DeviceInterface &vkd, vk::VkDevice device, vk::Allocator &allocator, vk::VkImage image,
    vk::VkFormat format, vk::VkImageCreateFlags createFlags,
    vk::MemoryRequirement requirement = vk::MemoryRequirement::Any);

void uploadImage(const vk::DeviceInterface &vkd, vk::VkDevice device, uint32_t queueFamilyNdx, vk::Allocator &allocator,
                 vk::VkImage image, const MultiPlaneImageData &imageData, vk::VkAccessFlags nextAccess,
                 vk::VkImageLayout finalLayout, uint32_t arrayLayer = 0u);

void fillImageMemory(const vk::DeviceInterface &vkd, vk::VkDevice device, uint32_t queueFamilyNdx, vk::VkImage image,
                     const std::vector<de::SharedPtr<vk::Allocation>> &memory, const MultiPlaneImageData &imageData,
                     vk::VkAccessFlags nextAccess, vk::VkImageLayout finalLayout, uint32_t arrayLayer = 0u);

void downloadImage(const vk::DeviceInterface &vkd, vk::VkDevice device, uint32_t queueFamilyNdx,
                   vk::Allocator &allocator, vk::VkImage image, MultiPlaneImageData *imageData,
                   vk::VkAccessFlags prevAccess, vk::VkImageLayout initialLayout, uint32_t baseArrayLayer = 0);

void readImageMemory(const vk::DeviceInterface &vkd, vk::VkDevice device, uint32_t queueFamilyNdx, vk::VkImage image,
                     const std::vector<de::SharedPtr<vk::Allocation>> &memory, MultiPlaneImageData *imageData,
                     vk::VkAccessFlags prevAccess, vk::VkImageLayout initialLayout);

class ChannelAccess
{
public:
    ChannelAccess(tcu::TextureChannelClass channelClass, uint8_t channelSize, const tcu::IVec3 &size,
                  const tcu::IVec3 &bitPitch, void *data, uint32_t bitOffset);

    const tcu::IVec3 &getSize(void) const
    {
        return m_size;
    }
    const tcu::IVec3 &getBitPitch(void) const
    {
        return m_bitPitch;
    }
    void *getDataPtr(void) const
    {
        return m_data;
    }

    tcu::Interval getChannel(const tcu::FloatFormat &conversionFormat, const tcu::IVec3 &pos) const;
    uint32_t getChannelUint(const tcu::IVec3 &pos) const;
    float getChannel(const tcu::IVec3 &pos) const;
    void setChannel(const tcu::IVec3 &pos, uint32_t x);
    void setChannel(const tcu::IVec3 &pos, float x);

private:
    const tcu::TextureChannelClass m_channelClass;
    const uint8_t m_channelSize;
    const tcu::IVec3 m_size;
    const tcu::IVec3 m_bitPitch;
    void *const m_data;
    const int32_t m_bitOffset;
};

ChannelAccess getChannelAccess(ycbcr::MultiPlaneImageData &data, const vk::PlanarFormatDescription &formatInfo,
                               const tcu::UVec2 &size, int channelNdx);

bool isYChromaSubsampled(vk::VkFormat format);

bool isXChromaSubsampled(vk::VkFormat format);

bool areLsb6BitsDontCare(vk::VkFormat srcFormat, vk::VkFormat dstFormat);

bool areLsb4BitsDontCare(vk::VkFormat srcFormat, vk::VkFormat dstFormat);

tcu::UVec4 getYCbCrBitDepth(vk::VkFormat format);

std::vector<tcu::FloatFormat> getPrecision(vk::VkFormat format);

uint32_t getYCbCrFormatChannelCount(vk::VkFormat format);

int wrap(vk::VkSamplerAddressMode addressMode, int coord, int size);

int divFloor(int a, int b);

void calculateBounds(const ChannelAccess &rPlane, const ChannelAccess &gPlane, const ChannelAccess &bPlane,
                     const ChannelAccess &aPlane, const tcu::UVec4 &bitDepth, const std::vector<tcu::Vec2> &sts,
                     const std::vector<tcu::FloatFormat> &filteringFormat,
                     const std::vector<tcu::FloatFormat> &conversionFormat, const uint32_t subTexelPrecisionBits,
                     vk::VkFilter filter, vk::VkSamplerYcbcrModelConversion colorModel, vk::VkSamplerYcbcrRange range,
                     vk::VkFilter chromaFilter, vk::VkChromaLocation xChromaOffset, vk::VkChromaLocation yChromaOffset,
                     const vk::VkComponentMapping &componentMapping, bool explicitReconstruction,
                     vk::VkSamplerAddressMode addressModeU, vk::VkSamplerAddressMode addressModeV,
                     std::vector<tcu::Vec4> &minBounds, std::vector<tcu::Vec4> &maxBounds,
                     std::vector<tcu::Vec4> &uvBounds, std::vector<tcu::IVec4> &ijBounds);

template <typename planeType> // T can be uint8_t for 8-bit or uint16_t for 16-bit
class YCbCrConvUtil
{
    YCbCrConvUtil()
    {
    }

    virtual ~YCbCrConvUtil()
    {
    }

public:
    // This method will convert multiplanar NV12 to one plane I420
    static de::MovePtr<std::vector<planeType>> MultiPlanarNV12toI420(vkt::ycbcr::MultiPlaneImageData *imageData)
    {
        uint32_t i;
        uint16_t msbShift;
        planeType *yPlaneData    = static_cast<planeType *>(imageData->getPlanePtr(0));
        planeType *uvPlaneData   = static_cast<planeType *>(imageData->getPlanePtr(1));
        tcu::UVec4 channelDepths = ycbcr::getYCbCrBitDepth(imageData->getFormat());
        int bitDepth             = channelDepths.x();
        const uint32_t width     = imageData->getSize().x();
        const uint32_t height    = imageData->getSize().y();
        uint32_t ySize           = width * height;
        uint32_t frameSize       = ySize + (ySize / 2);
        de::MovePtr<std::vector<planeType>> YUVDataPtr =
            de::MovePtr<std::vector<planeType>>(new std::vector<planeType>(frameSize));
        planeType *y_plane = YUVDataPtr.get()->data();
        planeType *u_plane = y_plane + ySize;
        planeType *v_plane = u_plane + (ySize / 4);

        DE_ASSERT(bitDepth == 8 || bitDepth == 10 || bitDepth == 12 || bitDepth == 16);
        TCU_CHECK_AND_THROW(InternalError, bitDepth != 16, "16-bit samples have not been tested yet");
        if (bitDepth == 12)
            msbShift = 4;
        else if (bitDepth == 10)
            msbShift = 6;
        else
            msbShift = 0;

        for (i = 0; i < ySize; i++)
        {
            y_plane[i] = yPlaneData[i] >> msbShift;
        }

        for (i = 0; i < ySize / 4; i++)
        {
            u_plane[i] = uvPlaneData[2 * i] >> msbShift;
            v_plane[i] = uvPlaneData[2 * i + 1] >> msbShift;
        }

        return YUVDataPtr;
    }
};

} // namespace ycbcr
} // namespace vkt

#endif // _VKTYCBCRUTIL_HPP
