#ifndef _VKTENSORWITHMEMORY_HPP
#define _VKTENSORWITHMEMORY_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2025-2026 ARM Ltd.
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
 * \brief Tensor backed with memory
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"

#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTensorMemoryUtil.hpp"

#include <algorithm>

namespace vk
{

#ifndef CTS_USES_VULKANSC
class TensorWithMemory
{
public:
    TensorWithMemory(const vk::DeviceInterface &vk, const vk::VkDevice device, vk::Allocator &allocator,
                     const vk::VkTensorCreateInfoARM &tensorCreateInfo, const vk::MemoryRequirement memoryRequirement)
        : m_tensor(createTensorARM(vk, device, &tensorCreateInfo))
        , m_allocationSize(0)
        , m_externalMemoryHandleTypes(0)
    {
        const VkExternalMemoryTensorCreateInfoARM *const externalInfo =
            findStructure<VkExternalMemoryTensorCreateInfoARM>(tensorCreateInfo.pNext);

        if (externalInfo != nullptr)
        {
            m_externalMemoryHandleTypes = externalInfo->handleTypes;
        }

        m_allocation = bindTensor(vk, device, allocator, *m_tensor, memoryRequirement, &m_allocationSize);
    }

    virtual ~TensorWithMemory() = default;

    const vk::VkTensorARM &get(void) const
    {
        return *m_tensor;
    }
    const vk::VkTensorARM &operator*(void) const
    {
        return get();
    }
    vk::Allocation &getAllocation(void) const
    {
        return *m_allocation;
    }
    VkDeviceSize getAllocationSize(void) const
    {
        return m_allocationSize;
    }
    vk::VkExternalMemoryHandleTypeFlags getExternalMemoryHandleTypes(void) const
    {
        return m_externalMemoryHandleTypes;
    }
    virtual VkImage getAliasedImage(void) const
    {
        return VK_NULL_HANDLE;
    }

protected:
    TensorWithMemory() : m_allocationSize(0), m_externalMemoryHandleTypes(0)
    {
    }

    vk::Move<vk::VkTensorARM> m_tensor;
    de::MovePtr<vk::Allocation> m_allocation;
    vk::VkDeviceSize m_allocationSize;
    vk::VkExternalMemoryHandleTypeFlags m_externalMemoryHandleTypes;

private:
    // "deleted"
    TensorWithMemory(const TensorWithMemory &);
    TensorWithMemory operator=(const TensorWithMemory &);
};

class ImageAliasedTensorWithMemory : public TensorWithMemory
{
public:
    // Image-aliased tensors need a mutable create info because their strides are supplied by the driver image layout.
    // The aliased image is created first, its subresource layout is queried, and those strides are written before
    // creating the tensor.
    ImageAliasedTensorWithMemory(const vk::DeviceInterface &vk, const vk::VkDevice device, vk::Allocator &allocator,
                                 vk::VkTensorCreateInfoARM &tensorCreateInfo, vk::TensorStrides &strides,
                                 const vk::MemoryRequirement memoryRequirement)
    {
        const VkExternalMemoryTensorCreateInfoARM *const externalInfo =
            findStructure<VkExternalMemoryTensorCreateInfoARM>(tensorCreateInfo.pNext);

        if (externalInfo != nullptr)
        {
            m_externalMemoryHandleTypes = externalInfo->handleTypes;
        }

        const VkTensorDescriptionARM *const desc = tensorCreateInfo.pDescription;
        DE_ASSERT(desc != nullptr);
        DE_ASSERT(desc->dimensionCount == 4);
        DE_ASSERT(desc->tiling == VK_TENSOR_TILING_LINEAR_ARM);

        const VkImageTiling imageTiling    = VK_IMAGE_TILING_LINEAR;
        const VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_STORAGE_BIT;

        const uint32_t width  = static_cast<uint32_t>(desc->pDimensions[2]);
        const uint32_t height = static_cast<uint32_t>(desc->pDimensions[1]);
        const uint32_t layers = static_cast<uint32_t>(desc->pDimensions[0]);

        const VkImageCreateInfo imageCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                                   nullptr,
                                                   0u,
                                                   VK_IMAGE_TYPE_2D,
                                                   desc->format,
                                                   {width, height, 1u},
                                                   1u,
                                                   layers,
                                                   VK_SAMPLE_COUNT_1_BIT,
                                                   imageTiling,
                                                   imageUsage,
                                                   VK_SHARING_MODE_EXCLUSIVE,
                                                   0u,
                                                   nullptr,
                                                   VK_IMAGE_LAYOUT_UNDEFINED};

        m_aliasedImage = createImage(vk, device, &imageCreateInfo);

        const VkImageSubresource sub = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u};
        VkSubresourceLayout layout   = {};
        vk.getImageSubresourceLayout(device, m_aliasedImage.get(), &sub, &layout);

        const tcu::TextureFormat tcuFormat = mapVkFormat(desc->format);
        const int64_t componentCount       = static_cast<int64_t>(tcu::getNumUsedChannels(tcuFormat.order));
        const int64_t pixelSize            = static_cast<int64_t>(tcu::getPixelSize(tcuFormat));
        DE_ASSERT(componentCount > 0);
        DE_ASSERT((pixelSize % componentCount) == 0);
        const int64_t elementStride = pixelSize / componentCount;
        DE_ASSERT(elementStride > 0);

        DE_ASSERT(strides.size() == desc->dimensionCount);
        DE_ASSERT(desc->pStrides == strides.data());
        strides[0] = static_cast<int64_t>((layout.arrayPitch != 0u) ? layout.arrayPitch : (layout.rowPitch * height));
        strides[1] = static_cast<int64_t>(layout.rowPitch);
        strides[2] = elementStride * componentCount;
        strides[3] = elementStride;

        m_tensor = createTensorARM(vk, device, &tensorCreateInfo);

        VkMemoryRequirements2 imageReqs  = {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, nullptr, {}};
        VkMemoryRequirements2 tensorReqs = {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, nullptr, {}};
        {
            const VkImageMemoryRequirementsInfo2 imageInfo = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
                                                              nullptr, m_aliasedImage.get()};
            vk.getImageMemoryRequirements2(device, &imageInfo, &imageReqs);

            const VkTensorMemoryRequirementsInfoARM tensorInfo = {VK_STRUCTURE_TYPE_TENSOR_MEMORY_REQUIREMENTS_INFO_ARM,
                                                                  nullptr, *m_tensor};
            vk.getTensorMemoryRequirementsARM(device, &tensorInfo, &tensorReqs);
        }

        const VkDeviceSize imageOffset = layout.offset;
        DE_ASSERT((imageOffset % tensorReqs.memoryRequirements.alignment) == 0u);

        const VkDeviceSize allocationSize =
            std::max(imageReqs.memoryRequirements.size, tensorReqs.memoryRequirements.size + imageOffset);

        VkMemoryRequirements combinedReqs = imageReqs.memoryRequirements;
        combinedReqs.size                 = allocationSize;
        combinedReqs.alignment =
            de::lcm(imageReqs.memoryRequirements.alignment, tensorReqs.memoryRequirements.alignment);
        combinedReqs.memoryTypeBits =
            imageReqs.memoryRequirements.memoryTypeBits & tensorReqs.memoryRequirements.memoryTypeBits;
        DE_ASSERT(combinedReqs.memoryTypeBits != 0u);

        m_allocation     = allocator.allocate(combinedReqs, memoryRequirement);
        m_allocationSize = allocationSize;

        const VkBindImageMemoryInfo bindImageInfo = {VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO, nullptr,
                                                     m_aliasedImage.get(), m_allocation->getMemory(),
                                                     m_allocation->getOffset()};
        VK_CHECK(vk.bindImageMemory2(device, 1u, &bindImageInfo));

        const VkBindTensorMemoryInfoARM bindTensorInfo = {VK_STRUCTURE_TYPE_BIND_TENSOR_MEMORY_INFO_ARM, nullptr,
                                                          *m_tensor, m_allocation->getMemory(),
                                                          m_allocation->getOffset() + imageOffset};
        VK_CHECK(vk.bindTensorMemoryARM(device, 1u, &bindTensorInfo));
    }

    VkImage getAliasedImage(void) const override
    {
        return m_aliasedImage.get();
    }

private:
    Move<VkImage> m_aliasedImage;

    // "deleted"
    ImageAliasedTensorWithMemory(const ImageAliasedTensorWithMemory &);
    ImageAliasedTensorWithMemory operator=(const ImageAliasedTensorWithMemory &);
};

#endif // CTS_USES_VULKANSC

} // namespace vk

#endif // _VKTENSORWITHMEMORY_HPP
