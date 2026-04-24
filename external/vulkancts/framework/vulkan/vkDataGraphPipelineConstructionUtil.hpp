#ifndef _VKDATAGRAPHPIPELINECONSTRUCTIONUTIL_HPP
#define _VKDATAGRAPHPIPELINECONSTRUCTIONUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024-2025 Arm Ltd.
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
 * \brief Wrapper for the construction of a data graph pipeline.
 */
/*--------------------------------------------------------------------*/

#include "vkRef.hpp"
#include "deSharedPtr.hpp"
#include "vkPrograms.hpp"
#include "vkDataGraphUtil.hpp"

#include <deque>

namespace vk
{

#ifndef CTS_USES_VULKANSC

class DataGraphPipelineWrapper
{
public:
    DataGraphPipelineWrapper() = default;
    DataGraphPipelineWrapper(const DeviceInterface &vk, VkDevice device);
    DataGraphPipelineWrapper(const DeviceInterface &vk, VkDevice device, const ProgramBinary &programBinary);

    DataGraphPipelineWrapper(const DataGraphPipelineWrapper &) noexcept;
    DataGraphPipelineWrapper(DataGraphPipelineWrapper &&) noexcept;
    ~DataGraphPipelineWrapper(void) = default;

    DataGraphPipelineWrapper &operator=(const DataGraphPipelineWrapper &rhs) noexcept;
    DataGraphPipelineWrapper &operator=(DataGraphPipelineWrapper &&rhs) noexcept;

    void setPipelineFeedback(VkPipelineCreationFeedback *pipelineCreateFeedback);
    void setDescriptorSetLayout(VkDescriptorSetLayout descriptorSetLayout);
    void setDescriptorSetLayouts(uint32_t setLayoutCount, const VkDescriptorSetLayout *descriptorSetLayouts);
    void setPipelineCreateFlags(VkPipelineCreateFlags pipelineCreateFlags);
    void buildPipeline(const VkPipelineCache pipelineCache);
    void bind(VkCommandBuffer commandBuffer);

    void addTensor(VkTensorDescriptionARM tensorDesc, uint32_t descriptorSet, uint32_t binding);
    void addConstant(VkTensorDescriptionARM tensorDesc, void *data, uint32_t id,
                     std::vector<DataGraphConstantSparsityHint> sparsityHints = {});
    void addShaderModule(Move<VkShaderModule> module);

    VkPipelineLayout getPipelineLayout(void);

    VkPipeline get()
    {
        return m_pipeline.get();
    }

private:
    void buildPipelineLayout(void);

    struct InternalData
    {
        const DeviceInterface &vk;
        VkDevice device;

        // initialize with most common values
        InternalData(const DeviceInterface &vkd, VkDevice vkDevice) : vk(vkd), device(vkDevice)
        {
        }
    };

    // Store internal data that is needed only for pipeline construction.
    de::SharedPtr<InternalData> m_internalData;
    const ProgramBinary *m_programBinary;
    std::vector<VkDescriptorSetLayout> m_descriptorSetLayouts;
    VkPipelineCreateFlags2KHR m_pipelineCreateFlags;
    void *m_pipelineCreatePNext;
    Move<VkShaderModule> m_module;

    Move<VkPipeline> m_pipeline;
    Move<VkPipelineLayout> m_pipelineLayout;

    VkDataGraphPipelineShaderModuleCreateInfoARM m_shaderModuleInfo;
    VkPipelineCreationFeedbackCreateInfo m_pipelineFeedbackInfo;

    std::vector<VkDataGraphPipelineResourceInfoARM> m_graph_resources{};
    std::vector<VkDataGraphPipelineConstantARM> m_graph_constants{};

    std::deque<VkTensorDescriptionARM> m_tensor_descriptions{};
    std::deque<VkDataGraphPipelineConstantTensorSemiStructuredSparsityInfoARM> m_sparsityInfo{};
};

#endif //#ifndef CTS_USES_VULKANSC

} // namespace vk

#endif // _VKDATAGRAPHPIPELINECONSTRUCTIONUTIL_HPP
