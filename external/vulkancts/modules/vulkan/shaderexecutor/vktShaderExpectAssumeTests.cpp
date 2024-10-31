/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2023 ARM Ltd.
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
 * \brief Test cases for VK_KHR_shader_expect_assume.
 *        Ensure being working the OpAssumeTrueKHR/OpExpectKHR OpCode.
 *//*--------------------------------------------------------------------*/

#include "vktShaderExpectAssumeTests.hpp"
#include "vktShaderExecutor.hpp"
#include "vktTestGroupUtil.hpp"

#include "tcuStringTemplate.hpp"

#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"

#include "tcuResultCollector.hpp"

#include "deArrayUtil.hpp"
#include "deSharedPtr.hpp"
#include "deStringUtil.hpp"

#include <cassert>
#include <string>

namespace vkt
{
namespace shaderexecutor
{

namespace
{

using namespace vk;
constexpr uint32_t kNumElements           = 32;
constexpr VkFormat kColorAttachmentFormat = VK_FORMAT_R32G32_UINT;

enum class OpType
{
    Expect = 0,
    Assume
};

enum class DataClass
{
    Constant = 0,
    SpecializationConstant,
    PushConstant,
    StorageBuffer,
};

enum class DataType
{
    Bool = 0,
    Int8,
    Int16,
    Int32,
    Int64
};

struct TestParam
{
    OpType opType;
    DataClass dataClass;
    DataType dataType;
    uint32_t dataChannelCount;
    VkShaderStageFlagBits shaderType;
    bool wrongExpectation;
    std::string testName;
};

class ShaderExpectAssumeTestInstance : public TestInstance
{
public:
    ShaderExpectAssumeTestInstance(Context &context, const TestParam &testParam)
        : TestInstance(context)
        , m_testParam(testParam)
        , m_vk(m_context.getDeviceInterface())
    {
        initialize();
    }

    virtual tcu::TestStatus iterate(void)
    {
        if (m_testParam.shaderType == VK_SHADER_STAGE_COMPUTE_BIT)
        {
            dispatch();
        }
        else
        {
            render();
        }

        const uint32_t *outputData = reinterpret_cast<uint32_t *>(m_outputAlloc->getHostPtr());
        return validateOutput(outputData);
    }

private:
    tcu::TestStatus validateOutput(const uint32_t *outputData)
    {
        for (uint32_t i = 0; i < kNumElements; i++)
        {
            // (gl_GlobalInvocationID.x, verification result)
            if (outputData[i * 2] != i || outputData[i * 2 + 1] != 1)
            {
                return tcu::TestStatus::fail("Result comparison failed");
            }
        }
        return tcu::TestStatus::pass("Pass");
    }

    void initialize()
    {
        generateCmdBuffer();
        if (m_testParam.shaderType == VK_SHADER_STAGE_COMPUTE_BIT)
        {
            generateStorageBuffers();
            generateComputePipeline();
        }
        else
        {
            generateAttachments();
            generateVertexBuffer();
            generateStorageBuffers();
            generateGraphicsPipeline();
        }
    }

    void generateCmdBuffer()
    {
        const VkDevice device = m_context.getDevice();

        m_cmdPool   = createCommandPool(m_vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                        m_context.getUniversalQueueFamilyIndex());
        m_cmdBuffer = allocateCommandBuffer(m_vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    }

    void generateVertexBuffer()
    {
        const VkDevice device           = m_context.getDevice();
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        Allocator &memAlloc             = m_context.getDefaultAllocator();
        std::vector<tcu::Vec2> vbo;
        // _____
        // |  /
        // | /
        // |/
        vbo.emplace_back(tcu::Vec2(-1, -1));
        vbo.emplace_back(tcu::Vec2(1, 1));
        vbo.emplace_back(tcu::Vec2(-1, 1));
        //   /|
        //  / |
        // /__|
        vbo.emplace_back(tcu::Vec2(-1, -1));
        vbo.emplace_back(tcu::Vec2(1, -1));
        vbo.emplace_back(tcu::Vec2(1, 1));

        const size_t dataSize               = vbo.size() * sizeof(tcu::Vec2);
        const VkBufferCreateInfo bufferInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            dataSize,                             // VkDeviceSize size;
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,    // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            1u,                                   // uint32_t queueFamilyCount;
            &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
        };
        m_vertexBuffer = createBuffer(vk, device, &bufferInfo);
        m_vertexAlloc =
            memAlloc.allocate(getBufferMemoryRequirements(vk, device, *m_vertexBuffer), MemoryRequirement::HostVisible);

        void *vertexData = m_vertexAlloc->getHostPtr();

        VK_CHECK(vk.bindBufferMemory(device, *m_vertexBuffer, m_vertexAlloc->getMemory(), m_vertexAlloc->getOffset()));

        /* Load vertices into vertex buffer */
        deMemcpy(vertexData, vbo.data(), dataSize);
        flushAlloc(vk, device, *m_vertexAlloc);
    }

    void generateAttachments()
    {
        const VkDevice device = m_context.getDevice();
        Allocator &allocator  = m_context.getDefaultAllocator();

        const VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        // Color Attachment
        const VkImageCreateInfo imageInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                             // const void* pNext;
            (VkImageCreateFlags)0,               // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
            kColorAttachmentFormat,              // VkFormat format;
            makeExtent3D(kNumElements, 1, 1),    // VkExtent3D extent;
            1u,                                  // uint32_t mipLevels;
            1u,                                  // uint32_t arrayLayers;
            VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
            imageUsage,                          // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
            0u,                                  // uint32_t queueFamilyIndexCount;
            nullptr,                             // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
        };

        const VkImageSubresourceRange imageSubresource =
            makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

        m_imageColor      = makeImage(m_vk, device, imageInfo);
        m_imageColorAlloc = bindImage(m_vk, device, allocator, *m_imageColor, MemoryRequirement::Any);
        m_imageColorView =
            makeImageView(m_vk, device, *m_imageColor, VK_IMAGE_VIEW_TYPE_2D, kColorAttachmentFormat, imageSubresource);
    }

    void generateGraphicsPipeline()
    {
        const VkDevice device = m_context.getDevice();
        std::vector<VkDescriptorSetLayoutBinding> bindings;

        if (m_testParam.dataClass == DataClass::StorageBuffer)
        {
            VkDescriptorSetLayoutCreateFlags layoutCreateFlags = 0;

            bindings.emplace_back(VkDescriptorSetLayoutBinding{
                0,                                                       // binding
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,                       // descriptorType
                1,                                                       // descriptorCount
                static_cast<VkShaderStageFlags>(m_testParam.shaderType), // stageFlags
                nullptr,                                                 // pImmutableSamplers
            });                                                          // input binding

            // Create a layout and allocate a descriptor set for it.
            const VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = {
                vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // sType
                nullptr,                                                 // pNext
                layoutCreateFlags,                                       // flags
                static_cast<uint32_t>(bindings.size()),                  // bindingCount
                bindings.data()                                          // pBindings
            };

            m_descriptorSetLayout = vk::createDescriptorSetLayout(m_vk, device, &setLayoutCreateInfo);
            m_pipelineLayout      = makePipelineLayout(m_vk, device, 1, &m_descriptorSetLayout.get(), 0, nullptr);
        }
        else if (m_testParam.dataClass == DataClass::PushConstant)
        {
            VkPushConstantRange pushConstant{static_cast<VkShaderStageFlags>(m_testParam.shaderType), 0,
                                             sizeof(VkBool32)};
            m_pipelineLayout = makePipelineLayout(m_vk, device, 0, nullptr, 1, &pushConstant);
        }
        else
        {
            m_pipelineLayout = makePipelineLayout(m_vk, device, 0, nullptr, 0, nullptr);
        }

        Move<VkShaderModule> vertexModule =
            createShaderModule(m_vk, device, m_context.getBinaryCollection().get("vert"), 0u);
        Move<VkShaderModule> fragmentModule =
            createShaderModule(m_vk, device, m_context.getBinaryCollection().get("frag"), 0u);

        const VkVertexInputBindingDescription vertexInputBindingDescription = {
            0,                           // uint32_t binding;
            sizeof(tcu::Vec2),           // uint32_t strideInBytes;
            VK_VERTEX_INPUT_RATE_VERTEX, // VkVertexInputStepRate stepRate;
        };

        const VkVertexInputAttributeDescription vertexInputAttributeDescription = {
            0u,                      // uint32_t location;
            0u,                      // uint32_t binding;
            VK_FORMAT_R32G32_SFLOAT, // VkFormat format;
            0u,                      // uint32_t offsetInBytes;
        };

        const VkPipelineVertexInputStateCreateInfo vertexInputStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                   // const void* pNext;
            0,                                                         // VkPipelineVertexInputStateCreateFlags flags;
            1u,                                                        // uint32_t bindingCount;
            &vertexInputBindingDescription,   // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
            1u,                               // uint32_t attributeCount;
            &vertexInputAttributeDescription, // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
        };

        const VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                     // const void* pNext;
            (VkPipelineInputAssemblyStateCreateFlags)0, // VkPipelineInputAssemblyStateCreateFlags flags;
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,        // VkPrimitiveTopology topology;
            VK_FALSE,                                   // VkBool32 primitiveRestartEnable;
        };

        const VkViewport viewport{0, 0, static_cast<float>(kNumElements), 1, 0, 1};
        const VkRect2D scissor{{0, 0}, {kNumElements, 1}};

        const VkPipelineViewportStateCreateInfo pipelineViewportStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                               // const void* pNext;
            (VkPipelineViewportStateCreateFlags)0,                 // VkPipelineViewportStateCreateFlags flags;
            1u,                                                    // uint32_t viewportCount;
            &viewport,                                             // const VkViewport* pViewports;
            1u,                                                    // uint32_t scissorCount;
            &scissor,                                              // const VkRect2D* pScissors;
        };

        const VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                    // const void* pNext;
            0u,                              // VkPipelineRasterizationStateCreateFlags flags;
            VK_FALSE,                        // VkBool32 depthClampEnable;
            VK_FALSE,                        // VkBool32 rasterizerDiscardEnable;
            VK_POLYGON_MODE_FILL,            // VkPolygonMode polygonMode;
            VK_CULL_MODE_NONE,               // VkCullModeFlags cullMode;
            VK_FRONT_FACE_COUNTER_CLOCKWISE, // VkFrontFace frontFace;
            VK_FALSE,                        // VkBool32 depthBiasEnable;
            0.0f,                            // float depthBiasConstantFactor;
            0.0f,                            // float depthBiasClamp;
            0.0f,                            // float depthBiasSlopeFactor;
            1.0f,                            // float lineWidth;
        };

        const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                  // const void* pNext;
            0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
            VK_SAMPLE_COUNT_1_BIT,                                    // VkSampleCountFlagBits rasterizationSamples;
            VK_FALSE,                                                 // VkBool32 sampleShadingEnable;
            1.0f,                                                     // float minSampleShading;
            nullptr,                                                  // const VkSampleMask* pSampleMask;
            VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
            VK_FALSE                                                  // VkBool32 alphaToOneEnable;
        };

        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentState(
            1,
            {
                false,                                                // VkBool32 blendEnable;
                VK_BLEND_FACTOR_ONE,                                  // VkBlend srcBlendColor;
                VK_BLEND_FACTOR_ONE,                                  // VkBlend destBlendColor;
                VK_BLEND_OP_ADD,                                      // VkBlendOp blendOpColor;
                VK_BLEND_FACTOR_ONE,                                  // VkBlend srcBlendAlpha;
                VK_BLEND_FACTOR_ONE,                                  // VkBlend destBlendAlpha;
                VK_BLEND_OP_ADD,                                      // VkBlendOp blendOpAlpha;
                (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT) // VkChannelFlags channelWriteMask;
            });

        const VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                  // const void* pNext;
            /* always needed */
            0,                                          // VkPipelineColorBlendStateCreateFlags flags;
            false,                                      // VkBool32 logicOpEnable;
            VK_LOGIC_OP_COPY,                           // VkLogicOp logicOp;
            (uint32_t)colorBlendAttachmentState.size(), // uint32_t attachmentCount;
            colorBlendAttachmentState.data(),           // const VkPipelineColorBlendAttachmentState* pAttachments;
            {0.0f, 0.0f, 0.0f, 0.0f},                   // float blendConst[4];
        };

        VkStencilOpState stencilOpState = {
            VK_STENCIL_OP_ZERO,               // VkStencilOp failOp;
            VK_STENCIL_OP_INCREMENT_AND_WRAP, // VkStencilOp passOp;
            VK_STENCIL_OP_INCREMENT_AND_WRAP, // VkStencilOp depthFailOp;
            VK_COMPARE_OP_ALWAYS,             // VkCompareOp compareOp;
            0xff,                             // uint32_t compareMask;
            0xff,                             // uint32_t writeMask;
            0,                                // uint32_t reference;
        };

        VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            // VkStructureType sType;
            nullptr, // const void* pNext;
            0,
            // VkPipelineDepthStencilStateCreateFlags flags;
            VK_FALSE,             // VkBool32 depthTestEnable;
            VK_FALSE,             // VkBool32 depthWriteEnable;
            VK_COMPARE_OP_ALWAYS, // VkCompareOp depthCompareOp;
            VK_FALSE,             // VkBool32 depthBoundsTestEnable;
            VK_FALSE,             // VkBool32 stencilTestEnable;
            stencilOpState,       // VkStencilOpState front;
            stencilOpState,       // VkStencilOpState back;
            0.0f,                 // float minDepthBounds;
            1.0f,                 // float maxDepthBounds;
        };

        const VkPipelineRenderingCreateInfoKHR renderingCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR, // VkStructureType sType;
            nullptr,                                              // const void* pNext;
            0u,                                                   // uint32_t viewMask;
            1,                                                    // uint32_t colorAttachmentCount;
            &kColorAttachmentFormat,                              // const VkFormat* pColorAttachmentFormats;
            VK_FORMAT_UNDEFINED,                                  // VkFormat depthAttachmentFormat;
            VK_FORMAT_UNDEFINED,                                  // VkFormat stencilAttachmentFormat;
        };

        VkSpecializationMapEntry specializationMapEntry = {0, 0, sizeof(VkBool32)};
        VkBool32 specializationData                     = VK_TRUE;
        VkSpecializationInfo specializationInfo = {1, &specializationMapEntry, sizeof(VkBool32), &specializationData};

        const VkPipelineShaderStageCreateInfo pShaderStages[] = {
            {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                             // const void*  pNext;
                (VkPipelineShaderStageCreateFlags)0,                 // VkPipelineShaderStageCreateFlags flags;
                VK_SHADER_STAGE_VERTEX_BIT,                          // VkShaderStageFlagBits stage;
                *vertexModule,                                       // VkShaderModule module;
                "main",                                              // const char* pName;
                (m_testParam.dataClass == DataClass::SpecializationConstant &&
                 m_testParam.shaderType == VK_SHADER_STAGE_VERTEX_BIT) ?
                    &specializationInfo :
                    nullptr, // const VkSpecializationInfo* pSpecializationInfo;
            },
            {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                             // const void* pNext;
                (VkPipelineShaderStageCreateFlags)0,                 // VkPipelineShaderStageCreateFlags flags;
                VK_SHADER_STAGE_FRAGMENT_BIT,                        // VkShaderStageFlagBits stage;
                *fragmentModule,                                     // VkShaderModule module;
                "main",                                              // const char* pName;
                (m_testParam.dataClass == DataClass::SpecializationConstant &&
                 m_testParam.shaderType == VK_SHADER_STAGE_FRAGMENT_BIT) ?
                    &specializationInfo :
                    nullptr, // const VkSpecializationInfo* pSpecializationInfo;
            },
        };

        const VkGraphicsPipelineCreateInfo graphicsPipelineInfo = {
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, // VkStructureType sType;
            &renderingCreateInfo,                            // const void* pNext;
            (VkPipelineCreateFlags)0,                        // VkPipelineCreateFlags flags;
            2u,                                              // uint32_t stageCount;
            pShaderStages,                                   // const VkPipelineShaderStageCreateInfo* pStages;
            &vertexInputStateParams,         // const VkPipelineVertexInputStateCreateInfo* pVertexInputState;
            &pipelineInputAssemblyStateInfo, // const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState;
            nullptr,                         // const VkPipelineTessellationStateCreateInfo* pTessellationState;
            &pipelineViewportStateInfo,      // const VkPipelineViewportStateCreateInfo* pViewportState;
            &pipelineRasterizationStateInfo, // const VkPipelineRasterizationStateCreateInfo* pRasterizationState;
            &pipelineMultisampleStateInfo,   // const VkPipelineMultisampleStateCreateInfo* pMultisampleState;
            &pipelineDepthStencilStateInfo,  // const VkPipelineDepthStencilStateCreateInfo* pDepthStencilState;
            &pipelineColorBlendStateInfo,    // const VkPipelineColorBlendStateCreateInfo* pColorBlendState;
            nullptr,                         // const VkPipelineDynamicStateCreateInfo* pDynamicState;
            *m_pipelineLayout,               // VkPipelineLayout layout;
            VK_NULL_HANDLE,                  // VkRenderPass renderPass;
            0u,                              // uint32_t subpass;
            VK_NULL_HANDLE,                  // VkPipeline basePipelineHandle;
            0,                               // int32_t basePipelineIndex;
        };

        m_pipeline = createGraphicsPipeline(m_vk, device, VK_NULL_HANDLE, &graphicsPipelineInfo);

        // DescriptorSet create/update for input storage buffer
        if (m_testParam.dataClass == DataClass::StorageBuffer)
        {
            // DescriptorPool/DescriptorSet create
            VkDescriptorPoolCreateFlags poolCreateFlags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

            vk::DescriptorPoolBuilder poolBuilder;
            for (uint32_t i = 0; i < static_cast<uint32_t>(bindings.size()); ++i)
            {
                poolBuilder.addType(bindings[i].descriptorType, bindings[i].descriptorCount);
            }
            m_descriptorPool = poolBuilder.build(m_vk, device, poolCreateFlags, 1);

            m_descriptorSet = makeDescriptorSet(m_vk, device, *m_descriptorPool, *m_descriptorSetLayout);

            // DescriptorSet update
            VkDescriptorBufferInfo inputBufferInfo;
            std::vector<VkDescriptorBufferInfo> bufferInfos;

            inputBufferInfo = makeDescriptorBufferInfo(m_inputBuffer.get(), 0, VK_WHOLE_SIZE);
            bufferInfos.push_back(inputBufferInfo); // binding 1 is input if needed

            VkWriteDescriptorSet w = {
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,    // sType
                nullptr,                                   // pNext
                *m_descriptorSet,                          // dstSet
                (uint32_t)0,                               // dstBinding
                0,                                         // dstArrayEllement
                static_cast<uint32_t>(bufferInfos.size()), // descriptorCount
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         // descriptorType
                nullptr,                                   // pImageInfo
                bufferInfos.data(),                        // pBufferInfo
                nullptr,                                   // pTexelBufferView
            };

            m_vk.updateDescriptorSets(device, 1, &w, 0, nullptr);
        }
    }

    void generateStorageBuffers()
    {
        // Avoid creating zero-sized buffer/memory
        const size_t inputBufferSize  = kNumElements * sizeof(uint64_t) * 4; // maximum size, 4 vector of 64bit
        const size_t outputBufferSize = kNumElements * sizeof(uint32_t) * 2;

        // Upload data to buffer
        const VkDevice device           = m_context.getDevice();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        Allocator &memAlloc             = m_context.getDefaultAllocator();

        const VkBufferCreateInfo inputBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            inputBufferSize,                      // VkDeviceSize size;
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,   // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            1u,                                   // uint32_t queueFamilyCount;
            &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
        };

        m_inputBuffer   = createBuffer(m_vk, device, &inputBufferParams);
        m_inputAlloc    = memAlloc.allocate(getBufferMemoryRequirements(m_vk, device, *m_inputBuffer),
                                            MemoryRequirement::HostVisible);
        void *inputData = m_inputAlloc->getHostPtr();

        // element stride of channel count 3 is 4, otherwise same to channel count
        const uint32_t elementStride = (m_testParam.dataChannelCount != 3) ? m_testParam.dataChannelCount : 4;

        for (uint32_t i = 0; i < kNumElements; i++)
        {
            for (uint32_t channel = 0; channel < m_testParam.dataChannelCount; channel++)
            {
                const uint32_t index = (i * elementStride) + channel;
                uint32_t value       = i + channel;
                if (m_testParam.wrongExpectation)
                {
                    value += 1; // write wrong value to storage buffer
                }

                switch (m_testParam.dataType)
                {
                case DataType::Bool: // std430 layout alignment of machine type(GLfloat)
                    reinterpret_cast<int32_t *>(inputData)[index] = m_testParam.wrongExpectation ? VK_FALSE : VK_TRUE;
                    break;
                case DataType::Int8:
                    reinterpret_cast<int8_t *>(inputData)[index] = static_cast<int8_t>(value);
                    break;
                case DataType::Int16:
                    reinterpret_cast<int16_t *>(inputData)[index] = static_cast<int16_t>(value);
                    break;
                case DataType::Int32:
                    reinterpret_cast<int32_t *>(inputData)[index] = static_cast<int32_t>(value);
                    break;
                case DataType::Int64:
                    reinterpret_cast<int64_t *>(inputData)[index] = static_cast<int64_t>(value);
                    break;
                default:
                    assert(false);
                }
            }
        }

        VK_CHECK(m_vk.bindBufferMemory(device, *m_inputBuffer, m_inputAlloc->getMemory(), m_inputAlloc->getOffset()));
        flushAlloc(m_vk, device, *m_inputAlloc);

        const VkBufferCreateInfo outputBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,                                  // VkStructureType sType;
            nullptr,                                                               // const void* pNext;
            0u,                                                                    // VkBufferCreateFlags flags;
            outputBufferSize,                                                      // VkDeviceSize size;
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode sharingMode;
            1u,                                                                    // uint32_t queueFamilyCount;
            &queueFamilyIndex // const uint32_t* pQueueFamilyIndices;
        };

        m_outputBuffer = createBuffer(m_vk, device, &outputBufferParams);
        m_outputAlloc  = memAlloc.allocate(getBufferMemoryRequirements(m_vk, device, *m_outputBuffer),
                                           MemoryRequirement::HostVisible);

        void *outputData = m_outputAlloc->getHostPtr();
        deMemset(outputData, 0, sizeof(outputBufferSize));

        VK_CHECK(
            m_vk.bindBufferMemory(device, *m_outputBuffer, m_outputAlloc->getMemory(), m_outputAlloc->getOffset()));
        flushAlloc(m_vk, device, *m_outputAlloc);
    }

    void generateComputePipeline()
    {
        const VkDevice device = m_context.getDevice();

        const Unique<VkShaderModule> cs(
            createShaderModule(m_vk, device, m_context.getBinaryCollection().get("comp"), 0));

        VkDescriptorSetLayoutCreateFlags layoutCreateFlags = 0;

        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.emplace_back(VkDescriptorSetLayoutBinding{
            0,                                 // binding
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // descriptorType
            1,                                 // descriptorCount
            VK_SHADER_STAGE_COMPUTE_BIT,       // stageFlags
            nullptr,                           // pImmutableSamplers
        });                                    // output binding

        if (m_testParam.dataClass == DataClass::StorageBuffer)
        {
            bindings.emplace_back(VkDescriptorSetLayoutBinding{
                1,                                 // binding
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // descriptorType
                1,                                 // descriptorCount
                VK_SHADER_STAGE_COMPUTE_BIT,       // stageFlags
                nullptr,                           // pImmutableSamplers
            });                                    // input binding
        }

        // Create a layout and allocate a descriptor set for it.
        const VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // sType
            nullptr,                                                 // pNext
            layoutCreateFlags,                                       // flags
            static_cast<uint32_t>(bindings.size()),                  // bindingCount
            bindings.data()                                          // pBindings
        };

        m_descriptorSetLayout = vk::createDescriptorSetLayout(m_vk, device, &setLayoutCreateInfo);

        VkSpecializationMapEntry specializationMapEntry = {0, 0, sizeof(VkBool32)};
        VkBool32 specializationData                     = VK_TRUE;
        VkSpecializationInfo specializationInfo = {1, &specializationMapEntry, sizeof(VkBool32), &specializationData};
        const VkPipelineShaderStageCreateInfo csShaderCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            (VkPipelineShaderStageCreateFlags)0,
            VK_SHADER_STAGE_COMPUTE_BIT, // stage
            *cs,                         // shader
            "main",
            (m_testParam.dataClass == DataClass::SpecializationConstant) ? &specializationInfo :
                                                                           nullptr, // pSpecializationInfo
        };

        VkPushConstantRange pushConstantRange = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(VkBool32)};
        m_pipelineLayout = makePipelineLayout(m_vk, device, 1, &m_descriptorSetLayout.get(), 1, &pushConstantRange);

        const VkComputePipelineCreateInfo pipelineCreateInfo = {
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            nullptr,
            0u,                 // flags
            csShaderCreateInfo, // cs
            *m_pipelineLayout,  // layout
            (vk::VkPipeline)0,  // basePipelineHandle
            0u,                 // basePipelineIndex
        };

        m_pipeline = createComputePipeline(m_vk, device, VK_NULL_HANDLE, &pipelineCreateInfo, nullptr);

        // DescriptorSet create for input/output storage buffer
        VkDescriptorPoolCreateFlags poolCreateFlags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        vk::DescriptorPoolBuilder poolBuilder;
        for (uint32_t i = 0; i < static_cast<uint32_t>(bindings.size()); ++i)
        {
            poolBuilder.addType(bindings[i].descriptorType, bindings[i].descriptorCount);
        }
        m_descriptorPool = poolBuilder.build(m_vk, device, poolCreateFlags, 1);

        m_descriptorSet = makeDescriptorSet(m_vk, device, *m_descriptorPool, *m_descriptorSetLayout);

        // DescriptorSet update
        VkDescriptorBufferInfo outputBufferInfo;
        VkDescriptorBufferInfo inputBufferInfo;
        std::vector<VkDescriptorBufferInfo> bufferInfos;

        outputBufferInfo = makeDescriptorBufferInfo(m_outputBuffer.get(), 0, VK_WHOLE_SIZE);
        bufferInfos.push_back(outputBufferInfo); // binding 0 is output

        if (m_testParam.dataClass == DataClass::StorageBuffer)
        {
            inputBufferInfo = makeDescriptorBufferInfo(m_inputBuffer.get(), 0, VK_WHOLE_SIZE);
            bufferInfos.push_back(inputBufferInfo); // binding 1 is input if needed
        }

        VkWriteDescriptorSet w = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,    // sType
            nullptr,                                   // pNext
            *m_descriptorSet,                          // dstSet
            (uint32_t)0,                               // dstBinding
            0,                                         // dstArrayEllement
            static_cast<uint32_t>(bufferInfos.size()), // descriptorCount
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         // descriptorType
            nullptr,                                   // pImageInfo
            bufferInfos.data(),                        // pBufferInfo
            nullptr,                                   // pTexelBufferView
        };

        m_vk.updateDescriptorSets(device, 1, &w, 0, nullptr);
    }

    void dispatch()
    {
        const VkDevice device = m_context.getDevice();
        const VkQueue queue   = m_context.getUniversalQueue();

        beginCommandBuffer(m_vk, *m_cmdBuffer);
        m_vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipeline);
        m_vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0u, 1,
                                   &m_descriptorSet.get(), 0u, nullptr);

        if (m_testParam.dataClass == DataClass::PushConstant)
        {
            VkBool32 pcValue = VK_TRUE;
            m_vk.cmdPushConstants(*m_cmdBuffer, *m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(VkBool32),
                                  &pcValue);
        }
        m_vk.cmdDispatch(*m_cmdBuffer, 1, 1, 1);

        const VkMemoryBarrier barrier = {
            VK_STRUCTURE_TYPE_MEMORY_BARRIER, // sType
            nullptr,                          // pNext
            VK_ACCESS_SHADER_WRITE_BIT,       // srcAccessMask
            VK_ACCESS_HOST_READ_BIT,          // dstAccessMask
        };
        m_vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                (VkDependencyFlags)0, 1, &barrier, 0, nullptr, 0, nullptr);

        VK_CHECK(m_vk.endCommandBuffer(*m_cmdBuffer));
        submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());
        flushMappedMemoryRange(m_vk, device, m_outputAlloc->getMemory(), 0, VK_WHOLE_SIZE);
    }

    void render()
    {
        const VkDevice device = m_context.getDevice();
        const VkQueue queue   = m_context.getUniversalQueue();

        beginCommandBuffer(m_vk, *m_cmdBuffer);

        // begin render pass
        const VkClearValue clearValue = {}; // { 0, 0, 0, 0 }
        const VkRect2D renderArea     = {{0, 0}, {kNumElements, 1}};

        const VkRenderingAttachmentInfoKHR renderingAttInfo = {
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR, // VkStructureType sType;
            nullptr,                                         // const void* pNext;
            *m_imageColorView,                               // VkImageView imageView;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,        // VkImageLayout imageLayout;
            VK_RESOLVE_MODE_NONE,                            // VkResolveModeFlagBits resolveMode;
            VK_NULL_HANDLE,                                  // VkImageView resolveImageView;
            VK_IMAGE_LAYOUT_UNDEFINED,                       // VkImageLayout resolveImageLayout;
            VK_ATTACHMENT_LOAD_OP_CLEAR,                     // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,                    // VkAttachmentStoreOp storeOp;
            clearValue,                                      // VkClearValue clearValue;
        };

        const VkRenderingInfoKHR renderingInfo = {
            VK_STRUCTURE_TYPE_RENDERING_INFO_KHR, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0,                                    // VkRenderingFlagsKHR flags;
            renderArea,                           // VkRect2D renderArea;
            1u,                                   // uint32_t layerCount;
            0u,                                   // uint32_t viewMask;
            1,                                    // uint32_t colorAttachmentCount;
            &renderingAttInfo,                    // const VkRenderingAttachmentInfoKHR* pColorAttachments;
            nullptr,                              // const VkRenderingAttachmentInfoKHR* pDepthAttachment;
            nullptr                               // const VkRenderingAttachmentInfoKHR* pStencilAttachment;
        };

        auto transition2DImage = [](const vk::DeviceInterface &vk, vk::VkCommandBuffer cmdBuffer, vk::VkImage image,
                                    vk::VkImageAspectFlags aspectMask, vk::VkImageLayout oldLayout,
                                    vk::VkImageLayout newLayout, vk::VkAccessFlags srcAccessMask,
                                    vk::VkAccessFlags dstAccessMask, vk::VkPipelineStageFlags srcStageMask,
                                    vk::VkPipelineStageFlags dstStageMask)
        {
            vk::VkImageMemoryBarrier barrier;
            barrier.sType                           = vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext                           = nullptr;
            barrier.srcAccessMask                   = srcAccessMask;
            barrier.dstAccessMask                   = dstAccessMask;
            barrier.oldLayout                       = oldLayout;
            barrier.newLayout                       = newLayout;
            barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            barrier.image                           = image;
            barrier.subresourceRange.aspectMask     = aspectMask;
            barrier.subresourceRange.baseMipLevel   = 0;
            barrier.subresourceRange.levelCount     = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount     = 1;

            vk.cmdPipelineBarrier(cmdBuffer, srcStageMask, dstStageMask, (vk::VkDependencyFlags)0, 0,
                                  (const vk::VkMemoryBarrier *)nullptr, 0, (const vk::VkBufferMemoryBarrier *)nullptr,
                                  1, &barrier);
        };

        transition2DImage(m_vk, *m_cmdBuffer, *m_imageColor, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        m_vk.cmdBeginRendering(*m_cmdBuffer, &renderingInfo);

        // vertex input setup
        // pipeline setup
        m_vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

        const uint32_t vertexCount = 6;
        const VkDeviceSize pOffset = 0;
        assert(vertexCount <= kNumElements);
        if (m_testParam.dataClass == DataClass::PushConstant)
        {
            const VkBool32 pcValue = VK_TRUE;
            m_vk.cmdPushConstants(*m_cmdBuffer, *m_pipelineLayout,
                                  static_cast<VkShaderStageFlags>(m_testParam.shaderType), 0, sizeof(VkBool32),
                                  &pcValue);
        }
        else if (m_testParam.dataClass == DataClass::StorageBuffer)
        {
            m_vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0u, 1,
                                       &m_descriptorSet.get(), 0u, nullptr);
        }
        m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &pOffset);

        m_vk.cmdDraw(*m_cmdBuffer, vertexCount, 1, 0, 0u);

        m_vk.cmdEndRendering(*m_cmdBuffer);

        VkMemoryBarrier memBarrier = {
            VK_STRUCTURE_TYPE_MEMORY_BARRIER, // sType
            nullptr,                          // pNext
            0u,                               // srcAccessMask
            0u,                               // dstAccessMask
        };
        memBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        memBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        m_vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &memBarrier, 0, nullptr, 0, nullptr);

        // copy color image to output buffer
        const VkImageSubresourceLayers imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        const VkOffset3D imageOffset                    = {};
        const VkExtent3D imageExtent                    = {kNumElements, 1, 1};
        const VkBufferImageCopy copyRegion              = {0, 0, 0, imageSubresource, imageOffset, imageExtent};

        m_vk.cmdCopyImageToBuffer(*m_cmdBuffer, *m_imageColor, VK_IMAGE_LAYOUT_GENERAL, *m_outputBuffer, 1,
                                  &copyRegion);

        VK_CHECK(m_vk.endCommandBuffer(*m_cmdBuffer));

        submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());
        flushMappedMemoryRange(m_vk, device, m_outputAlloc->getMemory(), 0, VK_WHOLE_SIZE);
    }

    TestParam m_testParam;
    const DeviceInterface &m_vk;

    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;
    Move<VkDescriptorPool> m_descriptorPool;
    Move<VkDescriptorSet> m_descriptorSet;
    Move<VkDescriptorSetLayout> m_descriptorSetLayout;
    Move<VkPipelineLayout> m_pipelineLayout;
    Move<VkPipeline> m_pipeline;
    Move<VkBuffer> m_inputBuffer;
    de::MovePtr<Allocation> m_inputAlloc;
    Move<VkBuffer> m_outputBuffer;
    de::MovePtr<Allocation> m_outputAlloc;
    Move<VkBuffer> m_vertexBuffer;
    de::MovePtr<Allocation> m_vertexAlloc;
    Move<VkImage> m_imageColor;
    de::MovePtr<Allocation> m_imageColorAlloc;
    Move<VkImageView> m_imageColorView;
};

class ShaderExpectAssumeCase : public TestCase
{
public:
    ShaderExpectAssumeCase(tcu::TestContext &testCtx, TestParam testParam)
        : TestCase(testCtx, testParam.testName)
        , m_testParam(testParam)
    {
    }
    ShaderExpectAssumeCase(const ShaderExpectAssumeCase &)            = delete;
    ShaderExpectAssumeCase &operator=(const ShaderExpectAssumeCase &) = delete;

    TestInstance *createInstance(Context &ctx) const override
    {
        return new ShaderExpectAssumeTestInstance(ctx, m_testParam);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override
    {
        std::map<std::string, std::string> params;

        params["TEST_ELEMENT_COUNT"] = std::to_string(kNumElements);
        assert(kNumElements < 127); // less than int byte

        switch (m_testParam.opType)
        {
        case OpType::Expect:
            params["TEST_OPERATOR"] = "expectKHR";
            break;
        case OpType::Assume:
            params["TEST_OPERATOR"] = "assumeTrueKHR";
            break;
        default:
            assert(false);
        }

        // default no need additional extension.
        params["DATATYPE_EXTENSION_ENABLE"] = "";

        switch (m_testParam.dataType)
        {
        case DataType::Bool:
            if (m_testParam.dataChannelCount == 1)
            {
                params["DATATYPE"] = "bool";
            }
            else
            {
                params["DATATYPE"] = "bvec" + std::to_string(m_testParam.dataChannelCount);
            }
            break;
        case DataType::Int8:
            assert(m_testParam.opType != OpType::Assume);
            params["DATATYPE_EXTENSION_ENABLE"] = "#extension GL_EXT_shader_explicit_arithmetic_types_int8: enable";
            if (m_testParam.dataChannelCount == 1)
            {
                params["DATATYPE"] = "int8_t";
            }
            else
            {
                params["DATATYPE"] = "i8vec" + std::to_string(m_testParam.dataChannelCount);
            }
            break;
        case DataType::Int16:
            assert(m_testParam.opType != OpType::Assume);
            params["DATATYPE_EXTENSION_ENABLE"] = "#extension GL_EXT_shader_explicit_arithmetic_types_int16: enable";
            if (m_testParam.dataChannelCount == 1)
            {
                params["DATATYPE"] = "int16_t";
            }
            else
            {
                params["DATATYPE"] = "i16vec" + std::to_string(m_testParam.dataChannelCount);
            }
            break;
        case DataType::Int32:
            assert(m_testParam.opType != OpType::Assume);
            params["DATATYPE_EXTENSION_ENABLE"] = "#extension GL_EXT_shader_explicit_arithmetic_types_int32: enable";
            if (m_testParam.dataChannelCount == 1)
            {
                params["DATATYPE"] = "int32_t";
            }
            else
            {
                params["DATATYPE"] = "i32vec" + std::to_string(m_testParam.dataChannelCount);
            }
            break;
        case DataType::Int64:
            assert(m_testParam.opType != OpType::Assume);
            params["DATATYPE_EXTENSION_ENABLE"] = "#extension GL_EXT_shader_explicit_arithmetic_types_int64: enable";
            if (m_testParam.dataChannelCount == 1)
            {
                params["DATATYPE"] = "int64_t";
            }
            else
            {
                params["DATATYPE"] = "i64vec" + std::to_string(m_testParam.dataChannelCount);
            }
            break;
        default:
            assert(false);
        }

        switch (m_testParam.dataClass)
        {
        case DataClass::Constant:
            assert(m_testParam.dataChannelCount == 1);

            params["VARNAME"] = "kThisIsTrue";
            if (m_testParam.opType == OpType::Expect)
            {
                params["EXPECTEDVALUE"] = "true";
                params["WRONGVALUE"]    = "false";
            }
            break;
        case DataClass::SpecializationConstant:
            assert(m_testParam.dataChannelCount == 1);

            params["VARNAME"] = "scThisIsTrue";
            if (m_testParam.opType == OpType::Expect)
            {
                params["EXPECTEDVALUE"] = "true";
                params["WRONGVALUE"]    = "false";
            }
            break;
        case DataClass::StorageBuffer:
        {
            std::string indexingOffset;
            switch (m_testParam.shaderType)
            {
            case VK_SHADER_STAGE_COMPUTE_BIT:
                indexingOffset = "gl_GlobalInvocationID.x";
                break;
            case VK_SHADER_STAGE_VERTEX_BIT:
                indexingOffset = "gl_VertexIndex";
                break;
            case VK_SHADER_STAGE_FRAGMENT_BIT:
                indexingOffset = "uint(gl_FragCoord.x)";
                break;
            default:
                assert(false);
            }

            params["VARNAME"] = "inputBuffer[" + indexingOffset + "]";

            if (m_testParam.opType == OpType::Expect)
            {
                if (m_testParam.dataType == DataType::Bool)
                {
                    params["EXPECTEDVALUE"] =
                        params["DATATYPE"] + "(true)"; // inputBuffer should be same as invocation id
                    params["WRONGVALUE"] =
                        params["DATATYPE"] + "(false)"; // inputBuffer should be same as invocation id
                }
                else
                {
                    // inputBuffer should be same as invocation id + channel
                    params["EXPECTEDVALUE"] = params["DATATYPE"] + "(" + indexingOffset;
                    for (uint32_t channel = 1; channel < m_testParam.dataChannelCount; channel++) // from channel 1
                    {
                        params["EXPECTEDVALUE"] += ", " + indexingOffset + " + " + std::to_string(channel);
                    }
                    params["EXPECTEDVALUE"] += ")";

                    params["WRONGVALUE"] = params["DATATYPE"] + "(" + indexingOffset + "*2 + 3";
                    for (uint32_t channel = 1; channel < m_testParam.dataChannelCount; channel++) // from channel 1
                    {
                        params["WRONGVALUE"] += ", " + indexingOffset + "*2 + 3" + " + " + std::to_string(channel);
                    }
                    params["WRONGVALUE"] += ")";
                }
            }
            break;
        }
        case DataClass::PushConstant:
            assert(m_testParam.dataChannelCount == 1);
            params["VARNAME"] = "pcThisIsTrue";

            if (m_testParam.opType == OpType::Expect)
            {
                params["EXPECTEDVALUE"] = "true";
                params["WRONGVALUE"]    = "false";
            }

            break;
        default:
            assert(false);
        }

        assert(!params["VARNAME"].empty());
        if (params["EXPECTEDVALUE"].empty())
        {
            params["TEST_OPERANDS"] = "(" + params["VARNAME"] + ")";
        }
        else
        {
            params["TEST_OPERANDS"] = "(" + params["VARNAME"] + ", " + params["EXPECTEDVALUE"] + ")";
        }

        switch (m_testParam.shaderType)
        {
        case VK_SHADER_STAGE_COMPUTE_BIT:
            addComputeTestShader(programCollection, params);
            break;
        case VK_SHADER_STAGE_VERTEX_BIT:
            addVertexTestShaders(programCollection, params);
            break;
        case VK_SHADER_STAGE_FRAGMENT_BIT:
            addFragmentTestShaders(programCollection, params);
            break;
        default:
            assert(0);
        }
    }

    void checkSupport(Context &context) const override
    {
        context.requireDeviceFunctionality("VK_KHR_shader_expect_assume");

        const auto &features          = context.getDeviceFeatures();
        const auto &featuresStorage16 = context.get16BitStorageFeatures();
        const auto &featuresF16I8     = context.getShaderFloat16Int8Features();
        const auto &featuresStorage8  = context.get8BitStorageFeatures();

        if (m_testParam.dataType == DataType::Int64)
        {
            if (!features.shaderInt64)
                TCU_THROW(NotSupportedError, "64-bit integers not supported");
        }
        else if (m_testParam.dataType == DataType::Int16)
        {
            context.requireDeviceFunctionality("VK_KHR_16bit_storage");

            if (!features.shaderInt16)
                TCU_THROW(NotSupportedError, "16-bit integers not supported");

            if (!featuresStorage16.storageBuffer16BitAccess)
                TCU_THROW(NotSupportedError, "16-bit storage buffer access not supported");
        }
        else if (m_testParam.dataType == DataType::Int8)
        {
            context.requireDeviceFunctionality("VK_KHR_shader_float16_int8");
            context.requireDeviceFunctionality("VK_KHR_8bit_storage");

            if (!featuresF16I8.shaderInt8)
                TCU_THROW(NotSupportedError, "8-bit integers not supported");

            if (!featuresStorage8.storageBuffer8BitAccess)
                TCU_THROW(NotSupportedError, "8-bit storage buffer access not supported");

            if (!featuresStorage8.uniformAndStorageBuffer8BitAccess)
                TCU_THROW(NotSupportedError, "8-bit Uniform storage buffer access not supported");
        }
    }

private:
    void addComputeTestShader(SourceCollections &programCollection, std::map<std::string, std::string> &params) const
    {
        std::stringstream compShader;

        // Compute shader copies color to linear layout in buffer memory
        compShader << "#version 460 core\n"
                   << "#extension GL_EXT_spirv_intrinsics: enable\n"
                   << "${DATATYPE_EXTENSION_ENABLE}\n"
                   << "spirv_instruction (extensions = [\"SPV_KHR_expect_assume\"], capabilities = [5629], id = 5630)\n"
                   << "void assumeTrueKHR(bool);\n"
                   << "spirv_instruction (extensions = [\"SPV_KHR_expect_assume\"], capabilities = [5629], id = 5631)\n"
                   << "${DATATYPE} expectKHR(${DATATYPE}, ${DATATYPE});\n"
                   << "precision highp float;\n"
                   << "precision highp int;\n"
                   << "layout(set = 0, binding = 0, std430) buffer Block0 { uvec2 outputBuffer[]; };\n";

        // declare input variable.
        if (m_testParam.dataClass == DataClass::Constant)
        {
            compShader << "bool kThisIsTrue = true;\n";
        }
        else if (m_testParam.dataClass == DataClass::SpecializationConstant)
        {
            compShader << "layout (constant_id = 0) const bool scThisIsTrue = false;\n";
        }
        else if (m_testParam.dataClass == DataClass::PushConstant)
        {
            compShader << "layout( push_constant, std430 ) uniform pc { layout(offset = 0) bool pcThisIsTrue; };\n";
        }
        else if (m_testParam.dataClass == DataClass::StorageBuffer)
        {
            compShader << "layout(set = 0, binding = 1, std430) buffer Block1 { ${DATATYPE} inputBuffer[]; };\n";
        }

        compShader << "layout(local_size_x = ${TEST_ELEMENT_COUNT}, local_size_y = 1, local_size_z = 1) in;\n"
                   << "void main()\n"
                   << "{\n";
        if (m_testParam.opType == OpType::Assume)
        {
            compShader << "    ${TEST_OPERATOR} ${TEST_OPERANDS};\n";
        }
        else if (m_testParam.opType == OpType::Expect)
        {
            compShader << "    ${DATATYPE} control = ${WRONGVALUE};\n"
                       << "    if ( ${TEST_OPERATOR}(${VARNAME}, ${EXPECTEDVALUE}) == ${EXPECTEDVALUE} ) {\n"
                       << "        control = ${EXPECTEDVALUE};\n"
                       << "    } else {\n"
                       << "        // set wrong value\n"
                       << "        control = ${WRONGVALUE};\n"
                       << "    }\n";
        }
        compShader << "    outputBuffer[gl_GlobalInvocationID.x].x = gl_GlobalInvocationID.x;\n";

        if (params["EXPECTEDVALUE"].empty())
        {
            compShader << "    outputBuffer[gl_GlobalInvocationID.x].y = uint(${VARNAME});\n";
        }
        else
        {
            if (m_testParam.opType == OpType::Assume)
            {
                compShader << "    outputBuffer[gl_GlobalInvocationID.x].y = uint(${VARNAME} == ${EXPECTEDVALUE});\n";
            }
            else if (m_testParam.opType == OpType::Expect)
            {
                // when m_testParam.wrongExpectation == true, the value of ${VARNAME} is set to ${EXPECTEDVALUE} + 1
                if (m_testParam.wrongExpectation)
                    compShader << "    outputBuffer[gl_GlobalInvocationID.x].y = uint(control == ${WRONGVALUE});\n";
                else
                    compShader << "    outputBuffer[gl_GlobalInvocationID.x].y = uint(control == ${EXPECTEDVALUE});\n";
            }
        }
        compShader << "}\n";

        tcu::StringTemplate computeShaderTpl(compShader.str());
        programCollection.glslSources.add("comp") << glu::ComputeSource(computeShaderTpl.specialize(params));
    }

    void addVertexTestShaders(SourceCollections &programCollection, std::map<std::string, std::string> &params) const
    {
        //vertex shader
        std::stringstream vertShader;
        vertShader << "#version 460\n"
                   << "#extension GL_EXT_spirv_intrinsics: enable\n"
                   << "${DATATYPE_EXTENSION_ENABLE}\n"
                   << "spirv_instruction (extensions = [\"SPV_KHR_expect_assume\"], capabilities = [5629], id = 5630)\n"
                   << "void assumeTrueKHR(bool);\n"
                   << "spirv_instruction (extensions = [\"SPV_KHR_expect_assume\"], capabilities = [5629], id = 5631)\n"
                   << "${DATATYPE} expectKHR(${DATATYPE}, ${DATATYPE});\n"
                   << "precision highp float;\n"
                   << "precision highp int;\n"
                   << "layout(location = 0) in vec4 in_position;\n"
                   << "layout(location = 0) out flat uint value;\n";

        // declare input variable.
        if (m_testParam.dataClass == DataClass::Constant)
        {
            vertShader << "bool kThisIsTrue = true;\n";
        }
        else if (m_testParam.dataClass == DataClass::SpecializationConstant)
        {
            vertShader << "layout (constant_id = 0) const bool scThisIsTrue = false;\n";
        }
        else if (m_testParam.dataClass == DataClass::PushConstant)
        {
            vertShader << "layout( push_constant, std430 ) uniform pc { layout(offset = 0) bool pcThisIsTrue; };\n";
        }
        else if (m_testParam.dataClass == DataClass::StorageBuffer)
        {
            vertShader << "layout(set = 0, binding = 0, std430) buffer Block1 { ${DATATYPE} inputBuffer[]; };\n";
        }

        vertShader << "void main() {\n";
        if (m_testParam.opType == OpType::Assume)
        {
            vertShader << "    ${TEST_OPERATOR} ${TEST_OPERANDS};\n";
        }
        else if (m_testParam.opType == OpType::Expect)
        {
            vertShader << "    ${DATATYPE} control = ${WRONGVALUE};\n"
                       << "    if ( ${TEST_OPERATOR}(${VARNAME}, ${EXPECTEDVALUE}) == ${EXPECTEDVALUE} ) {\n"
                       << "        control = ${EXPECTEDVALUE};\n"
                       << "    } else {\n"
                       << "        // set wrong value\n"
                       << "        control = ${WRONGVALUE};\n"
                       << "    }\n";
        }

        vertShader << "    gl_Position  = in_position;\n";

        if (params["EXPECTEDVALUE"].empty())
        {
            vertShader << "    value = uint(${VARNAME});\n";
        }
        else
        {
            if (m_testParam.opType == OpType::Assume)
            {
                vertShader << "    value = uint(${VARNAME} == ${EXPECTEDVALUE});\n";
            }
            else if (m_testParam.opType == OpType::Expect)
            {
                // when m_testParam.wrongExpectation == true, the value of ${VARNAME} is set to ${EXPECTEDVALUE} + 1
                if (m_testParam.wrongExpectation)
                    vertShader << "    value = uint(control == ${WRONGVALUE});\n";
                else
                    vertShader << "    value = uint(control == ${EXPECTEDVALUE});\n";
            }
        }
        vertShader << "}\n";

        tcu::StringTemplate vertexShaderTpl(vertShader.str());
        programCollection.glslSources.add("vert") << glu::VertexSource(vertexShaderTpl.specialize(params));

        // fragment shader
        std::stringstream fragShader;
        fragShader << "#version 460\n"
                   << "precision highp float;\n"
                   << "precision highp int;\n"
                   << "layout(location = 0) in flat uint value;\n"
                   << "layout(location = 0) out uvec2 out_color;\n"
                   << "void main()\n"
                   << "{\n"
                   << "    out_color.r = uint(gl_FragCoord.x);\n"
                   << "    out_color.g = value;\n"
                   << "}\n";

        tcu::StringTemplate fragmentShaderTpl(fragShader.str());
        programCollection.glslSources.add("frag") << glu::FragmentSource(fragmentShaderTpl.specialize(params));
    }

    void addFragmentTestShaders(SourceCollections &programCollection, std::map<std::string, std::string> &params) const
    {
        //vertex shader
        std::stringstream vertShader;
        vertShader << "#version 460\n"
                   << "precision highp float;\n"
                   << "precision highp int;\n"
                   << "layout(location = 0) in vec4 in_position;\n"
                   << "void main() {\n"
                   << "    gl_Position  = in_position;\n"
                   << "}\n";

        tcu::StringTemplate vertexShaderTpl(vertShader.str());
        programCollection.glslSources.add("vert") << glu::VertexSource(vertexShaderTpl.specialize(params));

        // fragment shader
        std::stringstream fragShader;
        fragShader << "#version 460\n"
                   << "#extension GL_EXT_spirv_intrinsics: enable\n"
                   << "${DATATYPE_EXTENSION_ENABLE}\n"
                   << "spirv_instruction (extensions = [\"SPV_KHR_expect_assume\"], capabilities = [5629], id = 5630)\n"
                   << "void assumeTrueKHR(bool);\n"
                   << "spirv_instruction (extensions = [\"SPV_KHR_expect_assume\"], capabilities = [5629], id = 5631)\n"
                   << "${DATATYPE} expectKHR(${DATATYPE}, ${DATATYPE});\n"
                   << "precision highp float;\n"
                   << "precision highp int;\n"
                   << "layout(location = 0) out uvec2 out_color;\n";
        if (m_testParam.dataClass == DataClass::Constant)
        {
            fragShader << "bool kThisIsTrue = true;\n";
        }
        else if (m_testParam.dataClass == DataClass::SpecializationConstant)
        {
            fragShader << "layout (constant_id = 0) const bool scThisIsTrue = false;\n";
        }
        else if (m_testParam.dataClass == DataClass::PushConstant)
        {
            fragShader << "layout( push_constant, std430 ) uniform pc { layout(offset = 0) bool pcThisIsTrue; };\n";
        }
        else if (m_testParam.dataClass == DataClass::StorageBuffer)
        {
            fragShader << "layout(set = 0, binding = 0, std430) buffer Block1 { ${DATATYPE} inputBuffer[]; };\n";
        }

        fragShader << "void main()\n"
                   << "{\n";

        if (m_testParam.opType == OpType::Assume)
        {
            fragShader << "    ${TEST_OPERATOR} ${TEST_OPERANDS};\n";
        }
        else if (m_testParam.opType == OpType::Expect)
        {
            fragShader << "    ${DATATYPE} control = ${WRONGVALUE};\n"
                       << "    if ( ${TEST_OPERATOR}(${VARNAME}, ${EXPECTEDVALUE}) == ${EXPECTEDVALUE} ) {\n"
                       << "        control = ${EXPECTEDVALUE};\n"
                       << "    } else {\n"
                       << "        // set wrong value\n"
                       << "        control = ${WRONGVALUE};\n"
                       << "    }\n";
        }
        fragShader << "    out_color.r = int(gl_FragCoord.x);\n";

        if (params["EXPECTEDVALUE"].empty())
        {
            fragShader << "    out_color.g = uint(${VARNAME});\n";
        }
        else
        {
            if (m_testParam.opType == OpType::Assume)
            {
                fragShader << "    out_color.g = uint(${VARNAME} == ${EXPECTEDVALUE});\n";
            }
            else if (m_testParam.opType == OpType::Expect)
            {
                // when m_testParam.wrongExpectation == true, the value of ${VARNAME} is set to ${EXPECTEDVALUE} + 1
                if (m_testParam.wrongExpectation)
                    fragShader << "    out_color.g = uint(control == ${WRONGVALUE});\n";
                else
                    fragShader << "    out_color.g = uint(control == ${EXPECTEDVALUE});\n";
            }
        }
        fragShader << "}\n";

        tcu::StringTemplate fragmentShaderTpl(fragShader.str());
        programCollection.glslSources.add("frag") << glu::FragmentSource(fragmentShaderTpl.specialize(params));
    }

private:
    TestParam m_testParam;
};

void addShaderExpectAssumeTests(tcu::TestCaseGroup *testGroup)
{
    VkShaderStageFlagBits stages[] = {
        VK_SHADER_STAGE_VERTEX_BIT,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_SHADER_STAGE_COMPUTE_BIT,
    };

    TestParam testParams[] = {
        {OpType::Expect, DataClass::Constant, DataType::Bool, 0, VK_SHADER_STAGE_ALL, false, "constant"},
        {OpType::Expect, DataClass::SpecializationConstant, DataType::Bool, 0, VK_SHADER_STAGE_ALL, false,
         "specializationconstant"},
        {OpType::Expect, DataClass::PushConstant, DataType::Bool, 0, VK_SHADER_STAGE_ALL, false, "pushconstant"},
        {OpType::Expect, DataClass::StorageBuffer, DataType::Bool, 0, VK_SHADER_STAGE_ALL, false, "storagebuffer_bool"},
        {OpType::Expect, DataClass::StorageBuffer, DataType::Int8, 0, VK_SHADER_STAGE_ALL, false, "storagebuffer_int8"},
        {OpType::Expect, DataClass::StorageBuffer, DataType::Int16, 0, VK_SHADER_STAGE_ALL, false,
         "storagebuffer_int16"},
        {OpType::Expect, DataClass::StorageBuffer, DataType::Int32, 0, VK_SHADER_STAGE_ALL, false,
         "storagebuffer_int32"},
        {OpType::Expect, DataClass::StorageBuffer, DataType::Int64, 0, VK_SHADER_STAGE_ALL, false,
         "storagebuffer_int64"},
        {OpType::Assume, DataClass::Constant, DataType::Bool, 0, VK_SHADER_STAGE_ALL, false, "constant"},
        {OpType::Assume, DataClass::SpecializationConstant, DataType::Bool, 0, VK_SHADER_STAGE_ALL, false,
         "specializationconstant"},
        {OpType::Assume, DataClass::PushConstant, DataType::Bool, 0, VK_SHADER_STAGE_ALL, false, "pushconstant"},
        {OpType::Assume, DataClass::StorageBuffer, DataType::Bool, 0, VK_SHADER_STAGE_ALL, false, "storagebuffer"},
    };

    tcu::TestContext &testCtx = testGroup->getTestContext();

    for (VkShaderStageFlagBits stage : stages)
    {
        const char *stageName = (stage == VK_SHADER_STAGE_VERTEX_BIT)   ? ("vertex") :
                                (stage == VK_SHADER_STAGE_FRAGMENT_BIT) ? ("fragment") :
                                (stage == VK_SHADER_STAGE_COMPUTE_BIT)  ? ("compute") :
                                                                          (nullptr);

        const std::string setName = std::string() + stageName;
        de::MovePtr<tcu::TestCaseGroup> stageGroupTest(
            new tcu::TestCaseGroup(testCtx, setName.c_str(), "Shader Expect Assume Tests"));

        de::MovePtr<tcu::TestCaseGroup> expectGroupTest(
            new tcu::TestCaseGroup(testCtx, "expect", "Shader Expect Tests"));

        de::MovePtr<tcu::TestCaseGroup> assumeGroupTest(
            new tcu::TestCaseGroup(testCtx, "assume", "Shader Assume Tests"));

        for (uint32_t expectationState = 0; expectationState < 2; expectationState++)
        {
            bool wrongExpected = (expectationState == 0) ? false : true;
            for (uint32_t channelCount = 1; channelCount <= 4; channelCount++)
            {
                for (TestParam testParam : testParams)
                {
                    testParam.dataChannelCount = channelCount;
                    testParam.wrongExpectation = wrongExpected;
                    if (channelCount > 1 || wrongExpected)
                    {
                        if (testParam.opType != OpType::Expect || testParam.dataClass != DataClass::StorageBuffer)
                        {
                            continue;
                        }

                        if (channelCount > 1)
                        {
                            testParam.testName = testParam.testName + "_vec" + std::to_string(channelCount);
                        }

                        if (wrongExpected)
                        {
                            testParam.testName = testParam.testName + "_wrong_expected";
                        }
                    }

                    testParam.shaderType = stage;

                    switch (testParam.opType)
                    {
                    case OpType::Expect:
                        expectGroupTest->addChild(new ShaderExpectAssumeCase(testCtx, testParam));
                        break;
                    case OpType::Assume:
                        assumeGroupTest->addChild(new ShaderExpectAssumeCase(testCtx, testParam));
                        break;
                    default:
                        assert(false);
                    }
                }
            }
        }

        stageGroupTest->addChild(expectGroupTest.release());
        stageGroupTest->addChild(assumeGroupTest.release());

        testGroup->addChild(stageGroupTest.release());
    }
}

} // namespace

tcu::TestCaseGroup *createShaderExpectAssumeTests(tcu::TestContext &testCtx)
{
    return createTestGroup(testCtx, "shader_expect_assume", addShaderExpectAssumeTests);
}

} // namespace shaderexecutor
} // namespace vkt
