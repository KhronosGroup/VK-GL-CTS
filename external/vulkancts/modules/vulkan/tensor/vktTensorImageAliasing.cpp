/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 ARM Ltd.
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
 */
/*!
 * \file
 * \brief Tensor Image Aliasing Tests
 */
/*--------------------------------------------------------------------*/

#include "vktTensorTests.hpp"

#include "vktTestCase.hpp"

#include "vktTestGroupUtil.hpp"
#include "vktTensorTestsUtil.hpp"
#include "shaders/vktTensorShaders.hpp"
#include "shaders/vktTensorShaderUtil.hpp"
#include "vkTensorMemoryUtil.hpp"

#include "vkBuilderUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "vkTensorWithMemory.hpp"
#include "vkBufferWithMemory.hpp"

#include "deMemory.h"
#include "deFloat16.h"

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "tcuFunctionLibrary.hpp"
#include "tcuPlatform.hpp"
#include "tcuCommandLine.hpp"
#include "tcuResource.hpp"

#include <numeric>
#include <iostream>

namespace vkt
{
namespace tensor
{

namespace
{

using namespace vk;

VkTensorTilingARM imageTilingToTensorTiling(const VkImageTiling tiling)
{
    switch (tiling)
    {
    case VK_IMAGE_TILING_LINEAR:
        return VK_TENSOR_TILING_LINEAR_ARM;
    case VK_IMAGE_TILING_OPTIMAL:
        return VK_TENSOR_TILING_OPTIMAL_ARM;
    default:
        DE_ASSERT(false);
    }

    return VK_TENSOR_TILING_LINEAR_ARM;
}

VkImageViewType imageTypeToImageViewType(const VkImageType type)
{
    switch (type)
    {
    case VK_IMAGE_TYPE_1D:
        return VK_IMAGE_VIEW_TYPE_1D;
    case VK_IMAGE_TYPE_2D:
        return VK_IMAGE_VIEW_TYPE_2D;
    case VK_IMAGE_TYPE_3D:
        return VK_IMAGE_VIEW_TYPE_3D;
    default:
        DE_ASSERT(false);
    }

    return VK_IMAGE_VIEW_TYPE_1D;
}
TensorDimensions imageShapeToAliasedTensorShape(const VkImageType type, const VkFormat format, const VkExtent3D &extent)
{
    TensorDimensions result;
    switch (type)
    {
    case VK_IMAGE_TYPE_3D:
        result.push_back(extent.depth);
        // Fallthrough
    case VK_IMAGE_TYPE_2D:
        result.push_back(extent.height);
        // Fallthrough
    case VK_IMAGE_TYPE_1D:
        result.push_back(extent.width);
        result.push_back(getFormatComponents(format));
        break;
    default:
        DE_ASSERT(false);
    }

    return result;
}

TensorStrides imageLayoutToAliasedTensorStrides(const VkImageType type, const VkFormat imageFormat,
                                                const VkSubresourceLayout &layout)
{
    const VkFormat tensorFormat    = imageToTensorFormat(imageFormat);
    const size_t tensorElementSize = getFormatSize(tensorFormat);

    TensorStrides result;
    switch (type)
    {
    case VK_IMAGE_TYPE_3D:
        result.push_back(layout.depthPitch);
        // Fallthrough
    case VK_IMAGE_TYPE_2D:
        result.push_back(layout.rowPitch);
        // Fallthrough
    case VK_IMAGE_TYPE_1D:
        result.push_back(getFormatComponents(imageFormat) * tensorElementSize);
        result.push_back(tensorElementSize);
        break;
    default:
        DE_ASSERT(false);
    }

    return result;
}

std::ostream &operator<<(std::ostream &os, ImageAliasingVariant variant)
{
    switch (variant)
    {
    case ImageAliasingVariant::IMAGE_PRODUCER_TENSOR_CONSUMER:
        os << "image_to_tensor";
        break;
    case ImageAliasingVariant::TENSOR_PRODUCER_IMAGE_CONSUMER:
        os << "tensor_to_image";
        break;
    case ImageAliasingVariant::TENSOR_PRODUCER_IMAGE_SAMPLE:
        os << "tensor_to_sampled_image";
        break;
    default:
        // unsupported formats
        DE_ASSERT(false);
    }

    return os;
}

static const char *imageTypeToShortName(const VkImageType imageType)
{
    switch (imageType)
    {
    case VK_IMAGE_TYPE_1D:
        return "1D";
    case VK_IMAGE_TYPE_2D:
        return "2D";
    case VK_IMAGE_TYPE_3D:
        return "3D";
    default:
        DE_ASSERT(false);
    }

    return "";
}

static const char *imageTilingToShortName(const VkImageTiling imageTiling)
{
    switch (imageTiling)
    {
    case VK_IMAGE_TILING_LINEAR:
        return "linear";
    case VK_IMAGE_TILING_OPTIMAL:
        return "optimal";
    default:
        DE_ASSERT(false);
    }

    return "";
}

static const char *imageFormatShortName(const VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R8_SINT:
        return "R8_SINT";
    case VK_FORMAT_R8_UINT:
        return "R8_UINT";
    case VK_FORMAT_R8G8_SINT:
        return "R8G8_SINT";
    case VK_FORMAT_R8G8_UINT:
        return "R8G8_UINT";
    case VK_FORMAT_R8G8B8A8_SINT:
        return "R8G8B8A8_SINT";
    case VK_FORMAT_R8G8B8A8_UINT:
        return "R8G8B8A8_UINT";
    case VK_FORMAT_R16_SINT:
        return "R16_SINT";
    case VK_FORMAT_R16_UINT:
        return "R16_UINT";
    case VK_FORMAT_R16G16_SINT:
        return "R16G16_SINT";
    case VK_FORMAT_R16G16_UINT:
        return "R16G16_UINT";
    case VK_FORMAT_R16G16B16A16_SINT:
        return "R16G16B16A16_SINT";
    case VK_FORMAT_R16G16B16A16_UINT:
        return "R16G16B16A16_UINT";
    case VK_FORMAT_R32_SINT:
        return "R32_SINT";
    case VK_FORMAT_R32_UINT:
        return "R32_UINT";
    case VK_FORMAT_R32G32_SINT:
        return "R32G32_SINT";
    case VK_FORMAT_R32G32_UINT:
        return "R32G32_UINT";
    case VK_FORMAT_R32G32B32A32_SINT:
        return "R32G32B32A32_SINT";
    case VK_FORMAT_R32G32B32A32_UINT:
        return "R32G32B32A32_UINT";
    case VK_FORMAT_R64_SINT:
        return "R64_SINT";
    case VK_FORMAT_R64_UINT:
        return "R64_UINT";
    default:
        // unsupported formats
        DE_ASSERT(false);
        return nullptr;
    }
}

template <typename T>
class TensorImageAliasingTestInstance : public TestInstance
{
public:
    TensorImageAliasingTestInstance(Context &testCtx, const VkImageTiling imageTiling, const VkImageType imageType,
                                    const VkExtent3D &imageShape, const VkFormat imageFormat,
                                    const ImageAliasingVariant variant, const TensorParameters &tensorParameters,
                                    const bool testUnifiedImageLayouts)
        : TestInstance(testCtx)
        , m_imageTiling(imageTiling)
        , m_imageType(imageType)
        , m_imageShape(imageShape)
        , m_imageFormat(imageFormat)
        , m_variant(variant)
        , m_tensorParameters(tensorParameters)
        , m_testUnifiedImageLayouts(testUnifiedImageLayouts)

    {
    }

    tcu::TestStatus iterate() override;

private:
    const VkImageTiling m_imageTiling;
    const VkImageType m_imageType;
    const VkExtent3D m_imageShape;
    const VkFormat m_imageFormat;
    const ImageAliasingVariant m_variant;
    const TensorParameters m_tensorParameters;
    const bool m_testUnifiedImageLayouts;
};

void barrierTransitionFromUndefined(const DeviceInterface &vk, const VkCommandBuffer cmdBuf, const VkImage image,
                                    const VkImageLayout newLayout)
{
    const VkImageSubresourceRange imageRange = {
        VK_IMAGE_ASPECT_COLOR_BIT,
        0, // baseMipLevel
        1, // levelCount
        0, // baseArrayLayer
        1, // layerCount
    };

    VkImageMemoryBarrier2 barrier = initVulkanStructure();
    barrier.srcStageMask          = VK_PIPELINE_STAGE_2_NONE;
    barrier.srcAccessMask         = VK_ACCESS_2_NONE;
    barrier.dstStageMask          = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask =
        VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange    = imageRange;

    VkDependencyInfo dependencyInfo        = initVulkanStructure();
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers    = &barrier;

    vk.cmdPipelineBarrier2(cmdBuf, &dependencyInfo);
}

void barrierTransitionUndefinedToGeneral(const DeviceInterface &vk, const VkCommandBuffer cmdBuf, const VkImage image)
{
    barrierTransitionFromUndefined(vk, cmdBuf, image, VK_IMAGE_LAYOUT_GENERAL);
}

void barrierTransitionUndefinedToTensorAliasing(const DeviceInterface &vk, const VkCommandBuffer cmdBuf,
                                                const VkImage image)
{
    barrierTransitionFromUndefined(vk, cmdBuf, image, VK_IMAGE_LAYOUT_TENSOR_ALIASING_ARM);
}

void barrierTransitionBetweenComputeDispatches(const DeviceInterface &vk, const VkCommandBuffer cmdBuf,
                                               const VkImage image, const VkImageLayout oldLayout,
                                               const VkImageLayout newLayout)
{
    const VkImageSubresourceRange imageRange = {
        VK_IMAGE_ASPECT_COLOR_BIT,
        0, // baseMipLevel
        1, // levelCount
        0, // baseArrayLayer
        1, // layerCount
    };

    VkImageMemoryBarrier2 barrier = initVulkanStructure();
    barrier.srcStageMask          = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask         = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.dstStageMask          = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask         = VK_ACCESS_2_SHADER_READ_BIT;
    barrier.oldLayout             = oldLayout;
    barrier.newLayout             = newLayout;
    barrier.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                 = image;
    barrier.subresourceRange      = imageRange;

    VkDependencyInfo dependencyInfo        = initVulkanStructure();
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers    = &barrier;

    vk.cmdPipelineBarrier2(cmdBuf, &dependencyInfo);
}

void barrierTransitionGeneralToTensorAliasing(const DeviceInterface &vk, const VkCommandBuffer cmdBuf,
                                              const VkImage image)
{
    barrierTransitionBetweenComputeDispatches(vk, cmdBuf, image, VK_IMAGE_LAYOUT_GENERAL,
                                              VK_IMAGE_LAYOUT_TENSOR_ALIASING_ARM);
}

void barrierTransitionTensorAliasingToGeneral(const DeviceInterface &vk, const VkCommandBuffer cmdBuf,
                                              const VkImage image)
{

    barrierTransitionBetweenComputeDispatches(vk, cmdBuf, image, VK_IMAGE_LAYOUT_TENSOR_ALIASING_ARM,
                                              VK_IMAGE_LAYOUT_GENERAL);
}

void barrierNoTransition(const DeviceInterface &vk, const VkCommandBuffer cmdBuf, const VkImage image)
{
    barrierTransitionBetweenComputeDispatches(vk, cmdBuf, image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
}

void barrierTensorBetweenComputeDispatches(const DeviceInterface &vk, const VkCommandBuffer cmdBuf,
                                           const VkTensorARM tensor, const VkAccessFlags2 srcAccess,
                                           const VkAccessFlags2 dstAccess)
{
    VkTensorMemoryBarrierARM barrier = initVulkanStructure();
    barrier.srcStageMask             = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask            = srcAccess;
    barrier.dstStageMask             = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask            = dstAccess;
    barrier.srcQueueFamilyIndex      = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex      = VK_QUEUE_FAMILY_IGNORED;
    barrier.tensor                   = tensor;

    VkDependencyInfo dependencyInfo = initVulkanStructure();
    dependencyInfo.pNext            = &barrier;

    vk.cmdPipelineBarrier2(cmdBuf, &dependencyInfo);
}

void barrierBufferOutput(const DeviceInterface &vk, const VkCommandBuffer cmdBuf, const VkBuffer buffer)
{
    VkBufferMemoryBarrier2 barrier = initVulkanStructure();
    barrier.srcStageMask           = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask          = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.dstStageMask           = VK_PIPELINE_STAGE_2_HOST_BIT;
    barrier.dstAccessMask          = VK_ACCESS_2_HOST_READ_BIT;
    barrier.srcQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer                 = buffer;
    barrier.offset                 = 0;
    barrier.size                   = VK_WHOLE_SIZE;

    VkDependencyInfo dependencyInfo         = initVulkanStructure();
    dependencyInfo.bufferMemoryBarrierCount = 1;
    dependencyInfo.pBufferMemoryBarriers    = &barrier;

    vk.cmdPipelineBarrier2(cmdBuf, &dependencyInfo);
}

template <typename T>
tcu::TestStatus TensorImageAliasingTestInstance<T>::iterate()
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    const bool testOptimalTiling = m_imageTiling == VK_IMAGE_TILING_OPTIMAL;

    const uint32_t elementCount = m_tensorParameters.elements();

    // Create image

    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0, // flags
        m_imageType,
        m_imageFormat,
        m_imageShape,
        1,                     // mipLevels
        1,                     // arrayLayers
        VK_SAMPLE_COUNT_1_BIT, // samples
        m_imageTiling,
        static_cast<VkImageUsageFlags>(
            VK_IMAGE_USAGE_STORAGE_BIT |
            (testOptimalTiling ? VK_IMAGE_USAGE_TENSOR_ALIASING_BIT_ARM : static_cast<VkImageUsageFlagBits>(0u)) |
            (m_variant == ImageAliasingVariant::TENSOR_PRODUCER_IMAGE_SAMPLE ? VK_IMAGE_USAGE_SAMPLED_BIT :
                                                                               static_cast<VkImageUsageFlagBits>(0u))),
        VK_SHARING_MODE_EXCLUSIVE,
        1,                 // queueFamilyIndexCount
        &queueFamilyIndex, // pQueueFamilyIndices
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    const vk::Unique<VkImage> image(vk::createImage(vk, device, &imageCreateInfo, nullptr));

    TensorStrides strides;
    VkDeviceSize subresourceOffset = 0;

    VkTensorDescriptionARM tensorDescription = {
        VK_STRUCTURE_TYPE_TENSOR_DESCRIPTION_ARM,
        nullptr,
        m_tensorParameters.tiling,
        m_tensorParameters.format,
        m_tensorParameters.rank(),
        m_tensorParameters.dimensions.data(),
        nullptr,
        VK_TENSOR_USAGE_SHADER_BIT_ARM,
    };

    if (!testOptimalTiling)
    {
        const VkImageSubresource imageSubresource = {
            VK_IMAGE_ASPECT_COLOR_BIT,
            0, // mipLevel
            0, // arrayLayer
        };

        VkSubresourceLayout imageSubresourceLayout{};
        vk.getImageSubresourceLayout(device, *image, &imageSubresource, &imageSubresourceLayout);

        strides = imageLayoutToAliasedTensorStrides(m_imageType, m_imageFormat, imageSubresourceLayout);
        tensorDescription.pStrides = strides.data();

        subresourceOffset = imageSubresourceLayout.offset;
    }
    else
    {
        tensorDescription.usage |= VK_TENSOR_USAGE_IMAGE_ALIASING_BIT_ARM;
    }

    // Create tensor

    const VkTensorCreateInfoARM tensorCreateInfo = {
        VK_STRUCTURE_TYPE_TENSOR_CREATE_INFO_ARM,
        nullptr,
        0, // flags
        &tensorDescription,
        VK_SHARING_MODE_EXCLUSIVE,
        1,                 // queueFamilyIndexCount
        &queueFamilyIndex, // pQueueFamilyIndices
    };

    vk::Unique<VkTensorARM> tensor(vk::createTensorARM(vk, device, &tensorCreateInfo, nullptr));

    // Get memory requirements

    const VkImageMemoryRequirementsInfo2 imageMemInfo = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
        nullptr,
        *image,
    };

    VkMemoryRequirements2 imageMemReqs = {
        VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
        nullptr,
        {},
    };

    vk.getImageMemoryRequirements2(device, &imageMemInfo, &imageMemReqs);

    const VkTensorMemoryRequirementsInfoARM tensorMemInfo = {
        VK_STRUCTURE_TYPE_TENSOR_MEMORY_REQUIREMENTS_INFO_ARM,
        nullptr,
        *tensor,
    };

    VkMemoryRequirements2 tensorMemReqs = {
        VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
        nullptr,
        {},
    };

    vk.getTensorMemoryRequirementsARM(device, &tensorMemInfo, &tensorMemReqs);

    // Get memory requirements

    const VkDeviceSize memSize =
        std::max(imageMemReqs.memoryRequirements.size, tensorMemReqs.memoryRequirements.size + subresourceOffset);
    const VkDeviceSize alignment =
        de::lcm(imageMemReqs.memoryRequirements.alignment, tensorMemReqs.memoryRequirements.alignment);

    const uint32_t memTypeBits =
        imageMemReqs.memoryRequirements.memoryTypeBits & tensorMemReqs.memoryRequirements.memoryTypeBits;
    DE_ASSERT(memTypeBits > 0);
    // when binding the tensor, the memory offset must be an multiple of the alignment from the memory requirements
    DE_ASSERT(subresourceOffset % tensorMemReqs.memoryRequirements.alignment == 0);

    // Allocate memory

    const VkMemoryAllocateInfo memAllocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        nullptr,
        memSize,
        selectMemoryTypeFromTypeBits(m_context, memTypeBits),
    };

    const auto allocation = allocator.allocate(memAllocInfo, alignment);

    // Bind image and tensor to memory (aliasing)

    const VkBindImageMemoryInfo imageBindInfo = {
        VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
        nullptr,
        *image,
        allocation->getMemory(),
        0, // memoryOffset
    };
    VK_CHECK(vk.bindImageMemory2(device, 1, &imageBindInfo));

    const VkBindTensorMemoryInfoARM tensorBindInfo = {
        VK_STRUCTURE_TYPE_BIND_TENSOR_MEMORY_INFO_ARM, nullptr, *tensor, allocation->getMemory(), subresourceOffset,
    };
    VK_CHECK(vk.bindTensorMemoryARM(device, 1, &tensorBindInfo));

    // Make image and tensor views

    auto imageView  = makeImageView(vk, device, *image, imageTypeToImageViewType(m_imageType), m_imageFormat,
                                    makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));
    auto tensorView = makeTensorView(vk, device, *tensor, m_tensorParameters.format);

    // Create two buffers and host-visible memory for them

    const BufferWithMemory tensorBuffer(
        vk, device, allocator, makeBufferCreateInfo(elementCount * sizeof(T), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        MemoryRequirement::HostVisible);
    const BufferWithMemory imageBuffer(
        vk, device, allocator, makeBufferCreateInfo(elementCount * sizeof(T), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        MemoryRequirement::HostVisible);

    // prepare buffers

    {
        const Allocation &tensorBufferAllocation = tensorBuffer.getAllocation();
        const Allocation &imageBufferAllocation  = imageBuffer.getAllocation();

        StridedMemoryUtils<T> tensorBufferMemory({uint32_t(elementCount)}, {}, tensorBufferAllocation.getHostPtr());
        StridedMemoryUtils<T> imageBufferMemory({uint32_t(elementCount)}, {}, imageBufferAllocation.getHostPtr());

        switch (m_variant)
        {
        case ImageAliasingVariant::IMAGE_PRODUCER_TENSOR_CONSUMER:
            imageBufferMemory.fill();
            tensorBufferMemory.clear();
            break;
        case ImageAliasingVariant::TENSOR_PRODUCER_IMAGE_CONSUMER:
        case ImageAliasingVariant::TENSOR_PRODUCER_IMAGE_SAMPLE:
            tensorBufferMemory.fill();
            imageBufferMemory.clear();
            break;
        default:
            DE_ASSERT(false);
        }

        flushAlloc(vk, device, tensorBufferAllocation);
        flushAlloc(vk, device, imageBufferAllocation);
    }

    // Create descriptor set

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_TENSOR_ARM, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_TENSOR_ARM)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    // Write the descriptor set

    const VkDescriptorBufferInfo tensorBufferDescriptorInfo =
        makeDescriptorBufferInfo(*tensorBuffer, 0ull, elementCount * sizeof(T));
    const VkDescriptorBufferInfo imageBufferDescriptorInfo =
        makeDescriptorBufferInfo(*imageBuffer, 0ull, elementCount * sizeof(T));
    const VkDescriptorImageInfo imageDescriptorInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, *imageView, VK_IMAGE_LAYOUT_GENERAL);
    const VkWriteDescriptorSetTensorARM tensorDescriptorInfo{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_TENSOR_ARM, nullptr,
                                                             1, &*tensorView};

    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_TENSOR_ARM,
                     &tensorDescriptorInfo)
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &tensorBufferDescriptorInfo)
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2u),
                     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageDescriptorInfo)
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(3u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &imageBufferDescriptorInfo)
        .update(vk, device);

    Move<VkSampler> sampler;
    if (m_variant == ImageAliasingVariant::TENSOR_PRODUCER_IMAGE_SAMPLE)
    {
        const VkSamplerCreateInfo samplerParams = {
            VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,   // VkStructureType sType;
            nullptr,                                 // const void* pNext;
            (VkSamplerCreateFlags)0,                 // VkSamplerCreateFlags flags;
            VK_FILTER_NEAREST,                       // VkFilter magFilter;
            VK_FILTER_NEAREST,                       // VkFilter minFilter;
            VK_SAMPLER_MIPMAP_MODE_NEAREST,          // VkSamplerMipmapMode mipmapMode;
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeU;
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeV;
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeW;
            0.0f,                                    // float mipLodBias;
            VK_FALSE,                                // VkBool32 anisotropyEnable;
            1.0f,                                    // float maxAnisotropy;
            VK_FALSE,                                // VkBool32 compareEnable;
            VK_COMPARE_OP_ALWAYS,                    // VkCompareOp compareOp;
            0.0f,                                    // float minLod;
            0.0f,                                    // float maxLod;
            VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, // VkBorderColor borderColor;
            VK_FALSE,                                // VkBool32 unnormalizedCoordinates;
        };
        sampler = createSampler(vk, device, &samplerParams);

        VkDescriptorImageInfo imageInfo;
        imageInfo.sampler     = *sampler;
        imageInfo.imageView   = *imageView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        DescriptorSetUpdateBuilder()
            .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(4u),
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo)
            .update(vk, device);
    }

    // Perform the computation

    {
        // Build shaders

        const ProgramBinary &imageBinary  = m_context.getBinaryCollection().get("image_comp");
        const ProgramBinary &tensorBinary = m_context.getBinaryCollection().get("tensor_comp");

        // NOTE: when writing from buffer to image we write all components at once (we run one shader invocation per texel). Hence, we skip the component part of the tensor when computing the dispatch group size
        const uint32_t imageGroupCount = singleDimensionWorkgroupCount(
            elementCount / static_cast<uint32_t>(m_tensorParameters.dimensions.back()), shaderImageAccessWorkgroupSize);
        DE_ASSERT(imageGroupCount <= dispatchWorkgroupCountLimit);
        const uint32_t tensorGroupCount = singleDimensionWorkgroupCount(elementCount, shaderTensorAccessWorkgroupSize);
        DE_ASSERT(tensorGroupCount <= dispatchWorkgroupCountLimit);

        const Unique<VkShaderModule> imageShaderModule(createShaderModule(vk, device, imageBinary, 0u));
        const Unique<VkShaderModule> tensorShaderModule(createShaderModule(vk, device, tensorBinary, 0u));

        // Setup pipeline

        const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));

        const Unique<VkPipeline> imagePipeline(makeComputePipeline(vk, device, *pipelineLayout, *imageShaderModule));
        const Unique<VkPipeline> tensorPipeline(makeComputePipeline(vk, device, *pipelineLayout, *tensorShaderModule));

        // Prepare the command buffer

        const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
        const Unique<VkCommandBuffer> cmdBuffer(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

        // Start recording commands

        beginCommandBuffer(vk, *cmdBuffer);

        vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                                 &descriptorSet.get(), 0u, nullptr);

        switch (m_variant)
        {
        case ImageAliasingVariant::IMAGE_PRODUCER_TENSOR_CONSUMER:
            barrierTransitionUndefinedToGeneral(vk, *cmdBuffer, *image);

            vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *imagePipeline);
            vk.cmdDispatch(*cmdBuffer, imageGroupCount, 1u, 1u);

            if (!testOptimalTiling || m_testUnifiedImageLayouts)
            {
                barrierTensorBetweenComputeDispatches(vk, *cmdBuffer, *tensor, VK_ACCESS_2_SHADER_WRITE_BIT,
                                                      VK_ACCESS_2_SHADER_READ_BIT);
            }
            else
            {
                barrierTransitionGeneralToTensorAliasing(vk, *cmdBuffer, *image);
                barrierTensorBetweenComputeDispatches(vk, *cmdBuffer, *tensor, VK_ACCESS_2_SHADER_WRITE_BIT,
                                                      VK_ACCESS_2_SHADER_READ_BIT);
            }

            vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *tensorPipeline);
            vk.cmdDispatch(*cmdBuffer, tensorGroupCount, 1u, 1u);

            barrierBufferOutput(vk, *cmdBuffer, *tensorBuffer);
            break;

        case ImageAliasingVariant::TENSOR_PRODUCER_IMAGE_CONSUMER:
        case ImageAliasingVariant::TENSOR_PRODUCER_IMAGE_SAMPLE:
            if (!testOptimalTiling || m_testUnifiedImageLayouts)
            {
                barrierTransitionUndefinedToGeneral(vk, *cmdBuffer, *image);
            }
            else
            {
                barrierTransitionUndefinedToTensorAliasing(vk, *cmdBuffer, *image);
            }

            barrierTensorBetweenComputeDispatches(vk, *cmdBuffer, *tensor, VK_ACCESS_2_SHADER_WRITE_BIT,
                                                  VK_ACCESS_2_SHADER_WRITE_BIT);

            vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *tensorPipeline);
            vk.cmdDispatch(*cmdBuffer, tensorGroupCount, 1u, 1u);

            if (!testOptimalTiling || m_testUnifiedImageLayouts)
            {
                barrierNoTransition(vk, *cmdBuffer, *image);
            }
            else
            {
                barrierTransitionTensorAliasingToGeneral(vk, *cmdBuffer, *image);
            }

            vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *imagePipeline);
            vk.cmdDispatch(*cmdBuffer, imageGroupCount, 1u, 1u);

            barrierBufferOutput(vk, *cmdBuffer, *imageBuffer);
            break;
        }

        endCommandBuffer(vk, *cmdBuffer);

        // Submit commands and wait for completion

        submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    }

    // Validate the results

    const Allocation &tensorBufferAllocation = tensorBuffer.getAllocation();
    const Allocation &imageBufferAllocation  = imageBuffer.getAllocation();

    invalidateAlloc(vk, device, tensorBufferAllocation);
    invalidateAlloc(vk, device, imageBufferAllocation);

    StridedMemoryUtils<T> tensorBufferMemory({uint32_t(elementCount)}, {}, tensorBufferAllocation.getHostPtr());
    StridedMemoryUtils<T> imageBufferMemory({uint32_t(elementCount)}, {}, imageBufferAllocation.getHostPtr());

    return compareStridedMemory(tensorBufferMemory, imageBufferMemory);
}

template <typename T>
class TensorImageAliasingTestCase : public TestCase
{
private:
    static std::string buildTestName(const VkImageTiling imageTiling, const VkFormat imageFormat,
                                     const VkImageType imageType, const ImageAliasingVariant variant,
                                     const bool testUnifiedImageLayouts)
    {
        std::ostringstream name;
        name << imageTilingToShortName(imageTiling) << "_" << imageFormatShortName(imageFormat) << "_"
             << imageTypeToShortName(imageType) << "_" << variant;

        if (testUnifiedImageLayouts)
        {
            name << "_unified_image_layouts";
        }

        return name.str();
    }

public:
    TensorImageAliasingTestCase(tcu::TestContext &testCtx, const VkImageTiling imageTiling, const VkImageType imageType,
                                const VkExtent3D &imageShape, const VkFormat imageFormat,
                                const ImageAliasingVariant variant, const bool testUnifiedImageLayouts = false)
        : TestCase(testCtx, buildTestName(imageTiling, imageFormat, imageType, variant, testUnifiedImageLayouts))
        , m_imageTiling{imageTiling}
        , m_imageType{imageType}
        , m_imageShape{imageShape}
        , m_imageFormat(imageFormat)
        , m_variant(variant)
        , m_tensorParameters{imageToTensorFormat(imageFormat),
                             imageTilingToTensorTiling(imageTiling),
                             imageShapeToAliasedTensorShape(imageType, imageFormat, imageShape),
                             {}}
        , m_testUnifiedImageLayouts(testUnifiedImageLayouts)
    {
        DE_ASSERT(m_imageTiling == VK_IMAGE_TILING_OPTIMAL || !testUnifiedImageLayouts);
    }

    TestInstance *createInstance(Context &ctx) const override
    {
        return new TensorImageAliasingTestInstance<T>(ctx, m_imageTiling, m_imageType, m_imageShape, m_imageFormat,
                                                      m_variant, m_tensorParameters, m_testUnifiedImageLayouts);
    }

    void checkSupport(Context &context) const override
    {
        context.requireDeviceFunctionality("VK_ARM_tensors");

        requireTensorShapeSupported(context, m_tensorParameters);

        if (m_testUnifiedImageLayouts)
        {
            context.requireDeviceFunctionality("VK_KHR_unified_image_layouts");
        }

        if (m_imageFormat == VK_FORMAT_R64_SINT || m_imageFormat == VK_FORMAT_R64_UINT)
        {
            context.requireDeviceFunctionality("VK_EXT_shader_image_atomic_int64");
        }

        if (!deviceSupportsShaderTensorAccess(context))
        {
            TCU_THROW(NotSupportedError, "Device does not support shader tensor access");
        }

        if (!deviceSupportsShaderStagesTensorAccess(context, VK_SHADER_STAGE_COMPUTE_BIT))
        {
            TCU_THROW(NotSupportedError, "Device does not support shader tensor access in compute shader stage");
        }

        if (m_imageTiling == VK_IMAGE_TILING_LINEAR && !deviceSupportsNonPackedTensors(context))
        {
            TCU_THROW(NotSupportedError,
                      "Device does not support non-packed tensors, which are required to align with image pitches");
        }

        const VkFormatFeatureFlags2 requiredTensorFormatFeaturesFlags =
            VK_FORMAT_FEATURE_2_TENSOR_SHADER_BIT_ARM |
            (m_imageTiling == VK_IMAGE_TILING_OPTIMAL ? VK_FORMAT_FEATURE_2_TENSOR_IMAGE_ALIASING_BIT_ARM : (0u));
        if (!formatSupportTensorFlags(context, imageToTensorFormat(m_imageFormat), m_tensorParameters.tiling,
                                      requiredTensorFormatFeaturesFlags))
        {
            TCU_THROW(NotSupportedError, "Device does not support the tensor flags for this tiling and format");
        }

        const VkImageUsageFlags requiredUsageFlags =
            VK_IMAGE_USAGE_STORAGE_BIT |
            (m_imageTiling == VK_IMAGE_TILING_OPTIMAL ? VK_IMAGE_USAGE_TENSOR_ALIASING_BIT_ARM :
                                                        static_cast<VkImageUsageFlagBits>(0u)) |
            (m_variant == ImageAliasingVariant::TENSOR_PRODUCER_IMAGE_SAMPLE ? VK_IMAGE_USAGE_SAMPLED_BIT :
                                                                               static_cast<VkImageUsageFlagBits>(0u));
        if (!formatSupportImageFlags(context, m_imageType, m_imageFormat, m_imageTiling, requiredUsageFlags))
        {
            TCU_THROW(NotSupportedError,
                      "Device does not support the image usage flags for this image type, tiling and format");
        }
    }

    void initPrograms(vk::SourceCollections &programCollection) const override
    {
        programCollection.glslSources.add("image_comp") << glu::ComputeSource(
            genShaderImageAccess(m_tensorParameters.dimensions, m_tensorParameters.format, m_imageFormat, m_variant));

        programCollection.glslSources.add("tensor_comp") << glu::ComputeSource(genShaderTensorAccess(
            m_tensorParameters.rank(), m_tensorParameters.format,
            (m_variant == ImageAliasingVariant::IMAGE_PRODUCER_TENSOR_CONSUMER) ? AccessVariant::WRITE_TO_BUFFER :
                                                                                  AccessVariant::READ_FROM_BUFFER));
    }

private:
    const VkImageTiling m_imageTiling;
    const VkImageType m_imageType;
    const VkExtent3D m_imageShape;
    const VkFormat m_imageFormat;
    const ImageAliasingVariant m_variant;
    const TensorParameters m_tensorParameters;
    const bool m_testUnifiedImageLayouts;
};

template <VkFormat Format>
void addTensorImageAliasingTests(tcu::TestCaseGroup &testCaseGroup)
{
    using T = typename VkFormatToHostType<Format>::HostType;

    for (const auto variant :
         {IMAGE_PRODUCER_TENSOR_CONSUMER, TENSOR_PRODUCER_IMAGE_CONSUMER, TENSOR_PRODUCER_IMAGE_SAMPLE})
    {
        static constexpr VkExtent3D extent1D = {4093, 1, 1};
        static constexpr VkExtent3D extent2D = {523, 13, 1};
        static constexpr VkExtent3D extent3D = {31, 17, 13};

        // Don't create sampled image test cases for 64-bit components
        if (variant == TENSOR_PRODUCER_IMAGE_SAMPLE && sizeof(T) >= 8)
        {
            continue;
        }

        testCaseGroup.addChild(new TensorImageAliasingTestCase<T>(
            testCaseGroup.getTestContext(), VK_IMAGE_TILING_LINEAR, VK_IMAGE_TYPE_1D, extent1D, Format, variant));
        testCaseGroup.addChild(new TensorImageAliasingTestCase<T>(
            testCaseGroup.getTestContext(), VK_IMAGE_TILING_LINEAR, VK_IMAGE_TYPE_2D, extent2D, Format, variant));
        testCaseGroup.addChild(new TensorImageAliasingTestCase<T>(
            testCaseGroup.getTestContext(), VK_IMAGE_TILING_LINEAR, VK_IMAGE_TYPE_3D, extent3D, Format, variant));

        testCaseGroup.addChild(new TensorImageAliasingTestCase<T>(
            testCaseGroup.getTestContext(), VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_TYPE_1D, extent1D, Format, variant));
        testCaseGroup.addChild(new TensorImageAliasingTestCase<T>(
            testCaseGroup.getTestContext(), VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_TYPE_2D, extent2D, Format, variant));
        testCaseGroup.addChild(new TensorImageAliasingTestCase<T>(
            testCaseGroup.getTestContext(), VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_TYPE_3D, extent3D, Format, variant));

        static constexpr bool testUnifiedImageLayouts = true;
        testCaseGroup.addChild(new TensorImageAliasingTestCase<T>(testCaseGroup.getTestContext(),
                                                                  VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_TYPE_1D, extent1D,
                                                                  Format, variant, testUnifiedImageLayouts));
        testCaseGroup.addChild(new TensorImageAliasingTestCase<T>(testCaseGroup.getTestContext(),
                                                                  VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_TYPE_2D, extent2D,
                                                                  Format, variant, testUnifiedImageLayouts));
        testCaseGroup.addChild(new TensorImageAliasingTestCase<T>(testCaseGroup.getTestContext(),
                                                                  VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_TYPE_3D, extent3D,
                                                                  Format, variant, testUnifiedImageLayouts));
    }
}

template <VkFormat... Formats>
void addTensorImageAliasingTestsForFormats(tcu::TestCaseGroup &testCaseGroup)
{
    (addTensorImageAliasingTests<Formats>(testCaseGroup), ...);
}

// clang-format make these much harder to read
// clang-format off
#define TENSOR_IMAGE_ALIASING_TEST_FORMATS \
    VK_FORMAT_R8_SINT, \
    VK_FORMAT_R8_UINT, \
    VK_FORMAT_R8G8_SINT, \
    VK_FORMAT_R8G8_UINT, \
    VK_FORMAT_R8G8B8A8_SINT, \
    VK_FORMAT_R8G8B8A8_UINT, \
    \
    VK_FORMAT_R16_SINT, \
    VK_FORMAT_R16_UINT, \
    VK_FORMAT_R16G16_SINT, \
    VK_FORMAT_R16G16_UINT, \
    VK_FORMAT_R16G16B16A16_SINT, \
    VK_FORMAT_R16G16B16A16_UINT, \
    \
    VK_FORMAT_R32_SINT, \
    VK_FORMAT_R32_UINT, \
    VK_FORMAT_R32G32_SINT, \
    VK_FORMAT_R32G32_UINT, \
    VK_FORMAT_R32G32B32A32_SINT, \
    VK_FORMAT_R32G32B32A32_UINT, \
    \
    VK_FORMAT_R64_SINT, \
    VK_FORMAT_R64_UINT
// clang-format on

} // namespace

tcu::TestCaseGroup *createTensorImageAliasingTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> tensorImageAliasingTests(
        new tcu::TestCaseGroup(testCtx, "image_aliasing", "Tensor image aliasing tests"));

    addTensorImageAliasingTestsForFormats<TENSOR_IMAGE_ALIASING_TEST_FORMATS>(*tensorImageAliasingTests);

    return tensorImageAliasingTests.release();
}

} // namespace tensor
} // namespace vkt
