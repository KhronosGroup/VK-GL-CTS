/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
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
 * \brief SPIR-V OpUndef instruction tests
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmOpUndefTests.hpp"
#include "vktTestCase.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkCmdUtil.hpp"
#include "vkStrUtil.hpp"
#include "tcuStringTemplate.hpp"

#include <sstream>

using namespace vk;

namespace vkt
{
namespace SpirVAssembly
{
namespace
{

struct Params
{
    Params(VkDescriptorType type_);
    const VkDescriptorType type;
    bool inLiveCode = false;
    bool isArray    = false;
    bool isDynamic() const;
    bool isUniform() const;
};

Params::Params(VkDescriptorType type_) : type(type_)
{
    DE_ASSERT(type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
              type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
}

bool Params::isDynamic() const
{
    return (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC);
}

bool Params::isUniform() const
{
    return (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);
}

class OpUndefBufferComputeTestInstance : public TestInstance
{
public:
    OpUndefBufferComputeTestInstance(Context &context, const Params &params) : TestInstance(context), m_params(params)
    {
    }
    virtual tcu::TestStatus iterate() override;

private:
    const Params m_params;
};

class OpUndefBufferComputeTestCase : public TestCase
{
public:
    OpUndefBufferComputeTestCase(tcu::TestContext &testCtx, const std::string &name, const Params &params)
        : TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual void initPrograms(SourceCollections &programCollection) const override;
    virtual TestInstance *createInstance(Context &context) const override
    {
        return new OpUndefBufferComputeTestInstance(context, m_params);
    }

private:
    const Params m_params;
};

void OpUndefBufferComputeTestCase::initPrograms(SourceCollections &programCollection) const
{
    std::map<std::string, std::string> mapping;

    mapping["STORAGE_CLASS"] = m_params.isUniform() ? "Uniform" : "StorageBuffer";

    mapping["CONDITION_VAL"]    = m_params.inLiveCode ? "OpConstantTrue" : "OpConstantFalse";
    mapping["CONDITION_BRANCH"] = m_params.inLiveCode ? "%then" : "%merge";

    if (m_params.isArray)
    {
        mapping["UNDEF_TYPE_DEF"] = "%len       = OpConstant %uint 4\n"
                                    "%ArrBuffer = OpTypeArray %Buffer %len";
        mapping["TARGET_TYPE"]    = "%ArrBuffer";
        mapping["EXTRACT_OP"]     = "%_         = OpCompositeExtract %v4float %param_undef 0 0";
    }
    else
    {
        mapping["UNDEF_TYPE_DEF"] = "";
        mapping["TARGET_TYPE"]    = "%Buffer";
        mapping["EXTRACT_OP"]     = "%_         = OpCompositeExtract %v4float %param_undef 0";
    }

    const std::string spirvTemplate = R"spirv(
    ; SPIR-V
    ; Version: 1.3
    OpCapability Shader
    OpMemoryModel Logical GLSL450
    OpEntryPoint GLCompute %main "main"
    OpExecutionMode %main LocalSize 1 1 1

    %void      = OpTypeVoid
    %float     = OpTypeFloat 32
    %v4float   = OpTypeVector %float 4
    %uint      = OpTypeInt 32 0

    %Buffer  = OpTypeStruct %v4float

    ${UNDEF_TYPE_DEF}

    %ptr_Buffer = OpTypePointer ${STORAGE_CLASS} %Buffer

    %void_fn   = OpTypeFunction %void
    %helper_fn = OpTypeFunction %void ${TARGET_TYPE}

    %undef_res = OpUndef ${TARGET_TYPE}

    %bool      = OpTypeBool
    %condition = ${CONDITION_VAL} %bool

    ; Auxiliary function
    %process_undef = OpFunction %void None %helper_fn
    %param_undef   = OpFunctionParameter ${TARGET_TYPE}
    %helper_entry  = OpLabel
                     ${EXTRACT_OP}
                     OpReturn
                     OpFunctionEnd

    ; Main function
    %main      = OpFunction %void None %void_fn
    %entry     = OpLabel
                 OpSelectionMerge %merge None
                 OpBranchConditional %condition %then %merge

    %then      = OpLabel
    %unused    = OpFunctionCall %void %process_undef %undef_res
                 OpBranch %merge

    %merge     = OpLabel
                 OpReturn
                 OpFunctionEnd
    )spirv";

    const std::string spirvCode = tcu::StringTemplate(spirvTemplate).specialize(mapping);
    programCollection.spirvAsmSources.add("opundef")
        << spirvCode << SpirVAsmBuildOptions(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3);
}

tcu::TestStatus OpUndefBufferComputeTestInstance::iterate()
{
    const DeviceInterface &di         = m_context.getDeviceInterface();
    const VkDevice device             = m_context.getDevice();
    Allocator &allocator              = m_context.getDefaultAllocator();
    const uint32_t queueFamilyIndex   = m_context.getUniversalQueueFamilyIndex();
    const VkQueue queue               = m_context.getUniversalQueue();
    const VkShaderStageFlagBits stage = VK_SHADER_STAGE_COMPUTE_BIT;

    const VkBufferCreateInfo bufferInfo =
        makeBufferCreateInfo(sizeof(tcu::Vec4), m_params.isUniform() ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT :
                                                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const BufferWithMemory buffer(di, device, allocator, bufferInfo, MemoryRequirement::Local);

    DescriptorSetLayoutBuilder layoutBuilder;
    if (m_params.isArray)
        layoutBuilder.addArrayBinding(m_params.type, 1u, stage);
    else
        layoutBuilder.addSingleBinding(m_params.type, stage);
    Move<VkDescriptorSetLayout> dsLayout = layoutBuilder.build(di, device);

    Move<VkPipelineLayout> pLayout = makePipelineLayout(di, device, *dsLayout);

    Move<VkShaderModule> shaderModule =
        createShaderModule(di, device, m_context.getBinaryCollection().get("opundef"), 0u);
    const VkComputePipelineCreateInfo pipelineCreateInfo{
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // sType
        nullptr,                                        // pNext
        0u,                                             // flags
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // sType
            nullptr,                                             // pNext
            0u,                                                  // flags
            stage,                                               // stage
            *shaderModule,                                       // module
            "main",                                              // pName
            nullptr                                              // pSpecializationInfo
        },                                                       // stage
        *pLayout,                                                // layout
        VK_NULL_HANDLE,                                          // basePipelineHandle
        0                                                        // basePipelineIndex
    };

    VkPipeline object = VK_NULL_HANDLE;
    const VkResult createPipelinesResult =
        di.createComputePipelines(device, VK_NULL_HANDLE, 1u, &pipelineCreateInfo, nullptr, &object);
    if (createPipelinesResult != VK_SUCCESS)
    {
        return tcu::TestStatus::fail(std::string("vkCreatePipelines returned ") + getResultName(createPipelinesResult));
    }
    Move<VkPipeline> pipeline(refdetails::Checked<VkPipeline>(object), Deleter<VkPipeline>(di, device, nullptr));

    Move<VkDescriptorPool> dsPool = DescriptorPoolBuilder()
                                        .addType(m_params.type, 1u)
                                        .build(di, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    Move<VkDescriptorSet> descriptorSet = makeDescriptorSet(di, device, *dsPool, *dsLayout);

    const VkDescriptorBufferInfo descriptorInfo = makeDescriptorBufferInfo(*buffer, 0u, buffer.getBufferSize());
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0), m_params.type, &descriptorInfo)
        .update(di, device);

    Move<VkCommandPool> cmdPool =
        createCommandPool(di, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    Move<VkCommandBuffer> cmd = allocateCommandBuffer(di, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    Move<VkFence> fence(createFence(di, device));

    const VkSubmitInfo submitInfo{
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType sType;
        nullptr,                       // const void* pNext;
        0u,                            // uint32_t waitSemaphoreCount;
        nullptr,                       // const VkSemaphore* pWaitSemaphores;
        nullptr,                       // const VkPipelineStageFlags* pWaitDstStageMask;
        1u,                            // uint32_t commandBufferCount;
        &cmd.get(),                    // const VkCommandBuffer* pCommandBuffers;
        0u,                            // uint32_t signalSemaphoreCount;
        nullptr,                       // const VkSemaphore* pSignalSemaphores;
    };

    const uint32_t dynamicOffset = 0u;

    beginCommandBuffer(di, *cmd);
    di.cmdBindPipeline(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    di.cmdBindDescriptorSets(*cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *pLayout, 0u, 1u, &descriptorSet.get(),
                             (m_params.isDynamic() ? 1u : 0u), (m_params.isDynamic() ? &dynamicOffset : nullptr));
    di.cmdDispatch(*cmd, 1u, 1u, 1u);
    endCommandBuffer(di, *cmd);
    const VkResult queueSubmitResult = di.queueSubmit(queue, 1u, &submitInfo, *fence);
    if (queueSubmitResult != VK_SUCCESS)
    {
        return tcu::TestStatus::fail(std::string("vkQueueSubmit returned ") + getResultName(queueSubmitResult));
    }
    const VkResult waitForFencesResult = di.waitForFences(device, 1u, &fence.get(), VK_TRUE, uint64_t(5e9));
    if (waitForFencesResult != VK_SUCCESS)
    {
        return tcu::TestStatus::fail(std::string("vkWaitForFences returned ") + getResultName(waitForFencesResult));
    }

    return tcu::TestStatus::pass(std::string());
}
} // unnamed namespace

void appendOpUndefTests(tcu::TestContext &testCtx, tcu::TestCaseGroup *opundefGroup)
{
    struct std::pair<std::string, VkDescriptorType> const cases[]
    {
        {"storage", VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}, {"uniform", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
            {"storage_dynamic", VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC},
            {"uniform_dynamic", VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC},
    };

    for (const auto &c : cases)
        for (const bool isArray : {false, true})
            for (const bool inLiveCode : {false, true})
            {
                Params p(c.second);
                p.isArray    = isArray;
                p.inLiveCode = inLiveCode;

                std::ostringstream os;
                os << "buffer_" << c.first;
                if (isArray)
                    os << "_array";
                if (inLiveCode)
                    os << "livecode";

                opundefGroup->addChild(new OpUndefBufferComputeTestCase(testCtx, os.str(), p));
            }
}

} // namespace SpirVAssembly
} // namespace vkt
