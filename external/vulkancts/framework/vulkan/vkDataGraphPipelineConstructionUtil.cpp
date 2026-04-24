/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 Arm Ltd.
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

#include "vkDataGraphPipelineConstructionUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"

namespace vk
{

#ifndef CTS_USES_VULKANSC

DataGraphPipelineWrapper::DataGraphPipelineWrapper(const DeviceInterface &vk, VkDevice device)
    : m_internalData(new DataGraphPipelineWrapper::InternalData(vk, device))
    , m_programBinary(nullptr)
    , m_pipelineCreateFlags((VkPipelineCreateFlags)0u)
    , m_pipelineCreatePNext(nullptr)
{
}

DataGraphPipelineWrapper::DataGraphPipelineWrapper(const DeviceInterface &vk, VkDevice device,
                                                   const ProgramBinary &programBinary)
    : m_internalData(new DataGraphPipelineWrapper::InternalData(vk, device))
    , m_programBinary(&programBinary)
    , m_pipelineCreateFlags((VkPipelineCreateFlags)0u)
    , m_pipelineCreatePNext(nullptr)
{
}

DataGraphPipelineWrapper::DataGraphPipelineWrapper(const DataGraphPipelineWrapper &rhs) noexcept
    : m_internalData(rhs.m_internalData)
    , m_programBinary(rhs.m_programBinary)
    , m_descriptorSetLayouts(rhs.m_descriptorSetLayouts)
    , m_pipelineCreateFlags(rhs.m_pipelineCreateFlags)
    , m_pipelineCreatePNext(rhs.m_pipelineCreatePNext)
{
    DE_ASSERT(rhs.m_pipeline.get() == VK_NULL_HANDLE);
}

DataGraphPipelineWrapper::DataGraphPipelineWrapper(DataGraphPipelineWrapper &&rhs) noexcept
    : m_internalData(rhs.m_internalData)
    , m_programBinary(rhs.m_programBinary)
    , m_descriptorSetLayouts(rhs.m_descriptorSetLayouts)
    , m_pipelineCreateFlags(rhs.m_pipelineCreateFlags)
    , m_pipelineCreatePNext(rhs.m_pipelineCreatePNext)
{
    DE_ASSERT(rhs.m_pipeline.get() == VK_NULL_HANDLE);
}

DataGraphPipelineWrapper &DataGraphPipelineWrapper::operator=(const DataGraphPipelineWrapper &rhs) noexcept
{
    m_internalData         = rhs.m_internalData;
    m_programBinary        = rhs.m_programBinary;
    m_descriptorSetLayouts = rhs.m_descriptorSetLayouts;
    m_pipelineCreateFlags  = rhs.m_pipelineCreateFlags;
    m_pipelineCreatePNext  = rhs.m_pipelineCreatePNext;
    DE_ASSERT(rhs.m_pipeline.get() == VK_NULL_HANDLE);
    return *this;
}

DataGraphPipelineWrapper &DataGraphPipelineWrapper::operator=(DataGraphPipelineWrapper &&rhs) noexcept
{
    m_internalData         = std::move(rhs.m_internalData);
    m_programBinary        = rhs.m_programBinary;
    m_descriptorSetLayouts = std::move(rhs.m_descriptorSetLayouts);
    m_pipelineCreateFlags  = rhs.m_pipelineCreateFlags;
    m_pipelineCreatePNext  = rhs.m_pipelineCreatePNext;
    DE_ASSERT(rhs.m_pipeline.get() == VK_NULL_HANDLE);
    return *this;
}

void DataGraphPipelineWrapper::setDescriptorSetLayout(VkDescriptorSetLayout descriptorSetLayout)
{
    m_descriptorSetLayouts = {descriptorSetLayout};
}

void DataGraphPipelineWrapper::addShaderModule(Move<VkShaderModule> module)
{
    m_module = module;
}

void DataGraphPipelineWrapper::addTensor(VkTensorDescriptionARM tensorDesc, uint32_t descriptorSet, uint32_t binding)
{
    DE_TEST_ASSERT(VK_TENSOR_USAGE_DATA_GRAPH_BIT_ARM & tensorDesc.usage);

    m_tensor_descriptions.push_back(tensorDesc);
    void *pNext = &m_tensor_descriptions.back();

    m_graph_resources.push_back({
        VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_RESOURCE_INFO_ARM, // VkStructureType    sType;
        pNext,                                                   // const void*        pNext;
        descriptorSet,                                           // uint32_t           descriptorSet;
        binding,                                                 // uint32_t           binding;
        0,                                                       // uint32_t           arrayElement;
    });
}

void DataGraphPipelineWrapper::addConstant(VkTensorDescriptionARM tensorDesc, void *data, uint32_t id,
                                           std::vector<DataGraphConstantSparsityHint> sparsityHints)
{
    DE_TEST_ASSERT(VK_TENSOR_USAGE_DATA_GRAPH_BIT_ARM & tensorDesc.usage);

    m_tensor_descriptions.push_back(tensorDesc);
    void *pNext = &m_tensor_descriptions.back();

    for (const auto &sparsityHint : sparsityHints)
    {
        m_sparsityInfo.push_back({
            VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_CONSTANT_TENSOR_SEMI_STRUCTURED_SPARSITY_INFO_ARM, // VkStructureType sType;
            pNext, // const void* pNext;
            sparsityHint.dimension,
            sparsityHint.zeroCount,
            sparsityHint.groupSize,
        });

        pNext = &m_sparsityInfo.back();
    }

    m_graph_constants.push_back({
        VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_CONSTANT_ARM, // VkStructureType sType;
        pNext,                                              // const void* pNext;
        id,                                                 // uint32_t id;
        data,                                               // const void* pConstantData;
    });
}

void DataGraphPipelineWrapper::setDescriptorSetLayouts(uint32_t setLayoutCount,
                                                       const VkDescriptorSetLayout *descriptorSetLayouts)
{
    m_descriptorSetLayouts.assign(descriptorSetLayouts, descriptorSetLayouts + setLayoutCount);
}

void DataGraphPipelineWrapper::setPipelineCreateFlags(VkPipelineCreateFlags pipelineCreateFlags)
{
    m_pipelineCreateFlags = pipelineCreateFlags;
}

void DataGraphPipelineWrapper::setPipelineFeedback(VkPipelineCreationFeedback *pipelineCreateFeedback)
{
    if (!pipelineCreateFeedback)
    {
        return;
    }

    m_pipelineFeedbackInfo.sType                     = VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO;
    m_pipelineFeedbackInfo.pPipelineCreationFeedback = pipelineCreateFeedback;
    m_pipelineFeedbackInfo.pipelineStageCreationFeedbackCount = 0;
    m_pipelineFeedbackInfo.pPipelineStageCreationFeedbacks    = nullptr;
    m_pipelineFeedbackInfo.pNext                              = m_pipelineCreatePNext;
    m_pipelineCreatePNext                                     = &m_pipelineFeedbackInfo;
}

void DataGraphPipelineWrapper::buildPipeline(const VkPipelineCache pipelineCache)
{
    const auto &vk     = m_internalData->vk;
    const auto &device = m_internalData->device;

    DE_ASSERT(m_pipeline.get() == VK_NULL_HANDLE);

    buildPipelineLayout();

    m_shaderModuleInfo = {
        VK_STRUCTURE_TYPE_DATA_GRAPH_PIPELINE_SHADER_MODULE_CREATE_INFO_ARM, // VkStructureType sType;
        m_pipelineCreatePNext,                                               // const void* pNext;
        m_module.get(),                                                      // VkShaderModule module;
        "main",                                                              // const char* pName;
        nullptr,                            // const VkSpecializationInfo* pSpecializationInfo;
        (uint32_t)m_graph_constants.size(), // uint32_t constantCount;
        m_graph_constants.data(),           // const VkDataGraphPipelineConstantARM* pConstants;
    };

    m_pipeline = vk::makeDataGraphPipeline(vk, device, *m_pipelineLayout, m_pipelineCreateFlags, &m_shaderModuleInfo,
                                           pipelineCache, m_graph_resources.data(), (uint32_t)m_graph_resources.size());
}

void DataGraphPipelineWrapper::bind(VkCommandBuffer commandBuffer)
{

    m_internalData->vk.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_DATA_GRAPH_ARM, m_pipeline.get());
}

void DataGraphPipelineWrapper::buildPipelineLayout(void)
{
    m_pipelineLayout = makePipelineLayout(m_internalData->vk, m_internalData->device, m_descriptorSetLayouts);
}

VkPipelineLayout DataGraphPipelineWrapper::getPipelineLayout(void)
{
    return *m_pipelineLayout;
}

#endif

} // namespace vk
