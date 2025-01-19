#ifndef _VKTIMAGEPROCESSINGTESTSUTIL_HPP
#define _VKTIMAGEPROCESSINGTESTSUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
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
 * \brief Utility classes
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBuilderUtil.hpp"

#include "tcuTexture.hpp"
#include "tcuVectorType.hpp"
#include "tcuDefs.hpp"

using namespace vk;
using namespace tcu;
namespace vkt
{
namespace ImageProcessing
{

enum ImageProcOp
{
    IMAGE_PROC_OP_SAMPLE_WEIGHTED = 0,
    IMAGE_PROC_OP_BOX_FILTER      = 1,
    IMAGE_PROC_OP_BLOCK_MATCH_SAD = 2,
    IMAGE_PROC_OP_BLOCK_MATCH_SSD = 3,
    IMAGE_PROC_OP_LAST            = 4
};

enum ImageType
{
    IMAGE_TYPE_1D = 0,
    IMAGE_TYPE_1D_ARRAY,
    IMAGE_TYPE_2D,
    IMAGE_TYPE_2D_ARRAY,
    IMAGE_TYPE_3D,
    IMAGE_TYPE_CUBE,
    IMAGE_TYPE_CUBE_ARRAY,
    IMAGE_TYPE_BUFFER,
    IMAGE_TYPE_LAST
};

class ImageProcessingResult : public TextureLevel
{
public:
    ImageProcessingResult(const TextureFormat format, uint32_t width, uint32_t height,
                          const VkSamplerAddressMode addressMode, const VkSamplerReductionMode reductionMode);
    ~ImageProcessingResult()
    {
    }

    const Vec4 getBlockMatchingResult(const bool isSSD, const PixelBufferAccess &targetPixels, const UVec2 &targetCoord,
                                      const PixelBufferAccess &referencePixels, const UVec2 &referenceCoord,
                                      const UVec2 &blockSize, const VkComponentMapping &componentMapping);

private:
    VkSamplerAddressMode m_addressMode;
    VkSamplerReductionMode m_reductionMode;
};

class DescriptorSetLayoutExtBuilder : public DescriptorSetLayoutBuilder
{
public:
    DescriptorSetLayoutExtBuilder(void);
    Move<VkDescriptorSetLayout> buildExt(const DeviceInterface &vk, VkDevice device,
                                         VkDescriptorSetLayoutCreateFlags extraFlags = 0,
                                         VkDescriptorBindingFlags bindingFlag        = 0) const;
};

const std::string getImageProcGLSLStr(const ImageProcOp op);
VkImageViewType mapImageViewType(const ImageType imageType);
VkImageType mapImageType(const ImageType imageType);

VkImageCreateInfo makeImageCreateInfo(const ImageType &imageType, const UVec2 imageSize, const VkFormat format,
                                      const VkImageUsageFlags usage, const VkImageCreateFlags flags,
                                      const VkImageTiling tiling);
Move<VkImageView> makeImageViewUtil(const DeviceInterface &vk, const VkDevice vkDevice, const VkImage image,
                                    const VkImageViewType imageViewType, const VkFormat format,
                                    const VkImageSubresourceRange subresourceRange,
                                    const VkComponentMapping components);
Move<VkImageView> makeImageViewUtil(const DeviceInterface &vk, const VkDevice vkDevice, const VkImage image,
                                    const VkImageViewType imageViewType, const VkFormat format,
                                    const VkImageSubresourceRange subresourceRange);

std::string getFormatPrefix(const TextureFormat &format);
std::string getFormatShortString(const VkFormat format);
std::string getImageTypeName(const ImageType imageType);
std::vector<VkFormat> getOpSupportedFormats(const ImageProcOp op);
const std::string getStageNames(const VkShaderStageFlags stageMask);

} // namespace ImageProcessing
} // namespace vkt

#endif // _VKTIMAGEPROCESSINGTESTSUTIL_HPP
