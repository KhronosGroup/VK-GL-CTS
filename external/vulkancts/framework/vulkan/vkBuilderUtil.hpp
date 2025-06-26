#ifndef _VKBUILDERUTIL_HPP
#define _VKBUILDERUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2015 Google Inc.
 * Copyright (c) 2023-2025 ARM Ltd.
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
 * \brief Vulkan object builder utilities.
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkRef.hpp"

#include <vector>

namespace vk
{

class DescriptorSetLayoutBuilder
{
public:
    DescriptorSetLayoutBuilder(void);

    DescriptorSetLayoutBuilder &addBinding(VkDescriptorType descriptorType, uint32_t descriptorCount,
                                           VkShaderStageFlags stageFlags, const VkSampler *pImmutableSamplers,
                                           VkDescriptorBindingFlags bindingFlags = 0);

    DescriptorSetLayoutBuilder &addIndexedBinding(VkDescriptorType descriptorType, uint32_t descriptorCount,
                                                  VkShaderStageFlags stageFlags, uint32_t dstBinding,
                                                  const VkSampler *pImmutableSamplers,
                                                  VkDescriptorBindingFlags bindingFlags = 0);

    Move<VkDescriptorSetLayout> build(const DeviceInterface &vk, VkDevice device,
                                      VkDescriptorSetLayoutCreateFlags extraFlags = 0) const;

    // helpers

    inline DescriptorSetLayoutBuilder &addSingleBinding(VkDescriptorType descriptorType, VkShaderStageFlags stageFlags,
                                                        VkDescriptorBindingFlags bindingFlags = 0)
    {
        return addBinding(descriptorType, 1u, stageFlags, nullptr, bindingFlags);
    }
    inline DescriptorSetLayoutBuilder &addSingleIndexedBinding(VkDescriptorType descriptorType,
                                                               VkShaderStageFlags stageFlags, uint32_t dstBinding,
                                                               VkDescriptorBindingFlags bindingFlags = 0)
    {
        return addIndexedBinding(descriptorType, 1u, stageFlags, dstBinding, nullptr, bindingFlags);
    }
    inline DescriptorSetLayoutBuilder &addArrayBinding(VkDescriptorType descriptorType, uint32_t descriptorCount,
                                                       VkShaderStageFlags stageFlags,
                                                       VkDescriptorBindingFlags bindingFlags = 0)
    {
        return addBinding(descriptorType, descriptorCount, stageFlags, nullptr, bindingFlags);
    }
    inline DescriptorSetLayoutBuilder &addSingleSamplerBinding(
        VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, const VkSampler *immutableSampler,
        VkDescriptorBindingFlags bindingFlags = 0) //!< \note: Using pointer to sampler to clarify that handle is not
                                                   //!<        copied and argument lifetime is expected to cover build()
                                                   //!<        call.
    {
        return addBinding(descriptorType, 1u, stageFlags, immutableSampler, bindingFlags);
    }
    inline DescriptorSetLayoutBuilder &addSingleIndexedSamplerBinding(
        VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t dstBinding,
        const VkSampler *immutableSampler,
        VkDescriptorBindingFlags bindingFlags = 0) //!< \note: Using pointer to sampler to clarify that handle is not
                                                   //!<        copied and argument lifetime is expected to cover build()
                                                   //!<        call.
    {
        return addIndexedBinding(descriptorType, 1u, stageFlags, dstBinding, immutableSampler, bindingFlags);
    }
    inline DescriptorSetLayoutBuilder &addArraySamplerBinding(VkDescriptorType descriptorType, uint32_t descriptorCount,
                                                              VkShaderStageFlags stageFlags,
                                                              const VkSampler *pImmutableSamplers,
                                                              VkDescriptorBindingFlags bindingFlags = 0)
    {
        return addBinding(descriptorType, descriptorCount, stageFlags, pImmutableSamplers, bindingFlags);
    }

protected:
    DescriptorSetLayoutBuilder(const DescriptorSetLayoutBuilder &);            // delete
    DescriptorSetLayoutBuilder &operator=(const DescriptorSetLayoutBuilder &); // delete

    std::vector<VkDescriptorSetLayoutBinding> m_bindings;
    std::vector<VkDescriptorBindingFlags> m_bindingFlags;

    struct ImmutableSamplerInfo
    {
        uint32_t bindingIndex;
        uint32_t samplerBaseIndex;
    };

    std::vector<ImmutableSamplerInfo> m_immutableSamplerInfos;
    std::vector<VkSampler> m_immutableSamplers;
};

class DescriptorPoolBuilder
{
public:
    DescriptorPoolBuilder(void);

    DescriptorPoolBuilder &addType(VkDescriptorType type, uint32_t numDescriptors = 1u);
    Move<VkDescriptorPool> build(const DeviceInterface &vk, VkDevice device, VkDescriptorPoolCreateFlags flags,
                                 uint32_t maxSets, const void *pNext = nullptr) const;

private:
    DescriptorPoolBuilder(const DescriptorPoolBuilder &);            // delete
    DescriptorPoolBuilder &operator=(const DescriptorPoolBuilder &); // delete

    std::vector<VkDescriptorPoolSize> m_counts;
};

class DescriptorSetUpdateBuilder
{
public:
    class Location
    {
    public:
        static inline Location binding(uint32_t binding_)
        {
            return Location(binding_, 0u);
        }
        static inline Location bindingArrayElement(uint32_t binding_, uint32_t arrayElement)
        {
            return Location(binding_, arrayElement);
        }

    private:
        // \note private to force use of factory methods that have more descriptive names
        inline Location(uint32_t binding_, uint32_t arrayElement) : m_binding(binding_), m_arrayElement(arrayElement)
        {
        }

        friend class DescriptorSetUpdateBuilder;

        const uint32_t m_binding;
        const uint32_t m_arrayElement;
    };

    DescriptorSetUpdateBuilder(void);
    /* DescriptorSetUpdateBuilder    (const DescriptorSetUpdateBuilder&); // do not delete */

    DescriptorSetUpdateBuilder &write(VkDescriptorSet destSet, uint32_t destBinding, uint32_t destArrayElement,
                                      uint32_t count, VkDescriptorType descriptorType,
                                      const VkDescriptorImageInfo *pImageInfo,
                                      const VkDescriptorBufferInfo *pBufferInfo, const VkBufferView *pTexelBufferView,
                                      const void *pNext = nullptr);

    DescriptorSetUpdateBuilder &copy(VkDescriptorSet srcSet, uint32_t srcBinding, uint32_t srcArrayElement,
                                     VkDescriptorSet destSet, uint32_t destBinding, uint32_t destArrayElement,
                                     uint32_t count);

    void update(const DeviceInterface &vk, VkDevice device) const;
    void updateWithPush(const DeviceInterface &vk, VkCommandBuffer cmd, VkPipelineBindPoint bindPoint,
                        VkPipelineLayout pipelineLayout, uint32_t setIdx, uint32_t descriptorIdx = 0,
                        uint32_t numDescriptors = 0) const;
    void clear(void);

    // helpers

    inline DescriptorSetUpdateBuilder &writeSingle(VkDescriptorSet destSet, const Location &destLocation,
                                                   VkDescriptorType descriptorType,
                                                   const VkDescriptorImageInfo *pImageInfo)
    {
        return write(destSet, destLocation.m_binding, destLocation.m_arrayElement, 1u, descriptorType, pImageInfo,
                     nullptr, nullptr);
    }

    inline DescriptorSetUpdateBuilder &writeSingle(VkDescriptorSet destSet, const Location &destLocation,
                                                   VkDescriptorType descriptorType,
                                                   const VkDescriptorBufferInfo *pBufferInfo)
    {
        return write(destSet, destLocation.m_binding, destLocation.m_arrayElement, 1u, descriptorType, nullptr,
                     pBufferInfo, nullptr);
    }

    inline DescriptorSetUpdateBuilder &writeSingle(VkDescriptorSet destSet, const Location &destLocation,
                                                   VkDescriptorType descriptorType,
                                                   const VkBufferView *pTexelBufferView)
    {
        return write(destSet, destLocation.m_binding, destLocation.m_arrayElement, 1u, descriptorType, nullptr, nullptr,
                     pTexelBufferView);
    }

#ifndef CTS_USES_VULKANSC
    inline DescriptorSetUpdateBuilder &writeSingle(
        VkDescriptorSet destSet, const Location &destLocation, VkDescriptorType descriptorType,
        const VkWriteDescriptorSetAccelerationStructureKHR *pAccelerationStructure)
    {
        return write(destSet, destLocation.m_binding, destLocation.m_arrayElement, 1u, descriptorType, nullptr, nullptr,
                     nullptr, pAccelerationStructure);
    }

    inline DescriptorSetUpdateBuilder &writeSingle(VkDescriptorSet destSet, const Location &destLocation,
                                                   VkDescriptorType descriptorType,
                                                   const VkWriteDescriptorSetTensorARM *pTensorInfo)
    {
        return write(destSet, destLocation.m_binding, destLocation.m_arrayElement, 1u, descriptorType, nullptr, nullptr,
                     nullptr, pTensorInfo);
    }

#endif // CTS_USES_VULKANSC

    inline DescriptorSetUpdateBuilder &writeArray(VkDescriptorSet destSet, const Location &destLocation,
                                                  VkDescriptorType descriptorType, uint32_t numDescriptors,
                                                  const VkDescriptorImageInfo *pImageInfo)
    {
        return write(destSet, destLocation.m_binding, destLocation.m_arrayElement, numDescriptors, descriptorType,
                     pImageInfo, nullptr, nullptr);
    }

    inline DescriptorSetUpdateBuilder &writeArray(VkDescriptorSet destSet, const Location &destLocation,
                                                  VkDescriptorType descriptorType, uint32_t numDescriptors,
                                                  const VkDescriptorBufferInfo *pBufferInfo)
    {
        return write(destSet, destLocation.m_binding, destLocation.m_arrayElement, numDescriptors, descriptorType,
                     nullptr, pBufferInfo, nullptr);
    }

    inline DescriptorSetUpdateBuilder &writeArray(VkDescriptorSet destSet, const Location &destLocation,
                                                  VkDescriptorType descriptorType, uint32_t numDescriptors,
                                                  const VkBufferView *pTexelBufferView)
    {
        return write(destSet, destLocation.m_binding, destLocation.m_arrayElement, numDescriptors, descriptorType,
                     nullptr, nullptr, pTexelBufferView);
    }

#ifndef CTS_USES_VULKANSC
    inline DescriptorSetUpdateBuilder &writeArray(
        VkDescriptorSet destSet, const Location &destLocation, VkDescriptorType descriptorType, uint32_t numDescriptors,
        const VkWriteDescriptorSetAccelerationStructureKHR *pAccelerationStructure)
    {
        return write(destSet, destLocation.m_binding, destLocation.m_arrayElement, numDescriptors, descriptorType,
                     nullptr, nullptr, nullptr, pAccelerationStructure);
    }

    inline DescriptorSetUpdateBuilder &writeArray(VkDescriptorSet destSet, const Location &destLocation,
                                                  VkDescriptorType descriptorType, uint32_t numDescriptors,
                                                  const VkWriteDescriptorSetTensorARM *pTernsorInfo)
    {
        return write(destSet, destLocation.m_binding, destLocation.m_arrayElement, numDescriptors, descriptorType,
                     nullptr, nullptr, nullptr, pTernsorInfo);
    }

#endif // CTS_USES_VULKANSC

    inline DescriptorSetUpdateBuilder &copySingle(VkDescriptorSet srcSet, const Location &srcLocation,
                                                  VkDescriptorSet destSet, const Location &destLocation)
    {
        return copy(srcSet, srcLocation.m_binding, srcLocation.m_arrayElement, destSet, destLocation.m_binding,
                    destLocation.m_arrayElement, 1u);
    }

    inline DescriptorSetUpdateBuilder &copyArray(VkDescriptorSet srcSet, const Location &srcLocation,
                                                 VkDescriptorSet destSet, const Location &destLocation, uint32_t count)
    {
        return copy(srcSet, srcLocation.m_binding, srcLocation.m_arrayElement, destSet, destLocation.m_binding,
                    destLocation.m_arrayElement, count);
    }

private:
    DescriptorSetUpdateBuilder &operator=(const DescriptorSetUpdateBuilder &); // delete

    struct WriteDescriptorInfo
    {
        std::vector<VkDescriptorImageInfo> imageInfos;
        std::vector<VkDescriptorBufferInfo> bufferInfos;
        std::vector<VkBufferView> texelBufferViews;
    };

    std::vector<WriteDescriptorInfo> m_writeDescriptorInfos;

    std::vector<VkWriteDescriptorSet> m_writes;
    std::vector<VkCopyDescriptorSet> m_copies;
};

} // namespace vk

#endif // _VKBUILDERUTIL_HPP
