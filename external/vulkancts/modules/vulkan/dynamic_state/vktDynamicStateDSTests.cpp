/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Dynamic State Depth Stencil Tests
 *//*--------------------------------------------------------------------*/

#include "vktDynamicStateDSTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktDynamicStateTestCaseUtil.hpp"
#include "vktDynamicStateBaseClass.hpp"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"

#include "vkRefUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkObjUtil.hpp"

#include "vktDrawCreateInfoUtil.hpp"
#include "vktDrawImageObjectUtil.hpp"
#include "vktDrawBufferObjectUtil.hpp"
#include "vkPrograms.hpp"

namespace vkt
{
namespace DynamicState
{

using namespace Draw;

namespace
{

class DepthStencilBaseCase : public TestInstance
{
public:
    DepthStencilBaseCase(Context &context, vk::PipelineConstructionType pipelineConstructionType,
                         const char *vertexShaderName, const char *fragmentShaderName,
                         const char *meshShaderName = nullptr)
        : TestInstance(context)
        , m_pipelineConstructionType(pipelineConstructionType)
        , m_colorAttachmentFormat(vk::VK_FORMAT_R8G8B8A8_UNORM)
        , m_depthStencilAttachmentFormat(vk::VK_FORMAT_UNDEFINED)
        , m_topology(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
        , m_vk(context.getDeviceInterface())
        , m_pipeline_1(context.getInstanceInterface(), m_vk, context.getPhysicalDevice(), context.getDevice(),
                       context.getDeviceExtensions(), pipelineConstructionType)
        , m_pipeline_2(context.getInstanceInterface(), m_vk, context.getPhysicalDevice(), context.getDevice(),
                       context.getDeviceExtensions(), pipelineConstructionType)
        , m_vertexShaderName(vertexShaderName ? vertexShaderName : "")
        , m_fragmentShaderName(fragmentShaderName)
        , m_meshShaderName(meshShaderName ? meshShaderName : "")
        , m_isMesh(meshShaderName != nullptr)
    {
        // Either a classic or mesh pipeline, but not both or none.
        DE_ASSERT((vertexShaderName != nullptr) != (meshShaderName != nullptr));
    }

protected:
    enum
    {
        WIDTH  = 128,
        HEIGHT = 128
    };

    vk::PipelineConstructionType m_pipelineConstructionType;
    vk::VkFormat m_colorAttachmentFormat;
    vk::VkFormat m_depthStencilAttachmentFormat;

    vk::VkPrimitiveTopology m_topology;

    const vk::DeviceInterface &m_vk;

    vk::Move<vk::VkDescriptorPool> m_descriptorPool;
    vk::Move<vk::VkDescriptorSetLayout> m_setLayout;
    vk::PipelineLayoutWrapper m_pipelineLayout;
    vk::Move<vk::VkDescriptorSet> m_descriptorSet;
    vk::GraphicsPipelineWrapper m_pipeline_1;
    vk::GraphicsPipelineWrapper m_pipeline_2;

    de::SharedPtr<Image> m_colorTargetImage;
    vk::Move<vk::VkImageView> m_colorTargetView;

    de::SharedPtr<Image> m_depthStencilImage;
    vk::Move<vk::VkImageView> m_attachmentView;

    PipelineCreateInfo::VertexInputState m_vertexInputState;
    de::SharedPtr<Buffer> m_vertexBuffer;

    vk::Move<vk::VkCommandPool> m_cmdPool;
    vk::Move<vk::VkCommandBuffer> m_cmdBuffer;

    vk::RenderPassWrapper m_renderPass;

    const std::string m_vertexShaderName;
    const std::string m_fragmentShaderName;
    const std::string m_meshShaderName;

    std::vector<PositionColorVertex> m_data;

    PipelineCreateInfo::DepthStencilState m_depthStencilState_1;
    PipelineCreateInfo::DepthStencilState m_depthStencilState_2;

    const bool m_isMesh;

    void initialize(void)
    {
        const vk::VkDevice device = m_context.getDevice();

        vk::VkFormatProperties formatProperties;
        // check for VK_FORMAT_D24_UNORM_S8_UINT support
        m_context.getInstanceInterface().getPhysicalDeviceFormatProperties(
            m_context.getPhysicalDevice(), vk::VK_FORMAT_D24_UNORM_S8_UINT, &formatProperties);
        if (formatProperties.optimalTilingFeatures & vk::VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            m_depthStencilAttachmentFormat = vk::VK_FORMAT_D24_UNORM_S8_UINT;
        }
        else
        {
            // check for VK_FORMAT_D32_SFLOAT_S8_UINT support
            m_context.getInstanceInterface().getPhysicalDeviceFormatProperties(
                m_context.getPhysicalDevice(), vk::VK_FORMAT_D32_SFLOAT_S8_UINT, &formatProperties);
            if (formatProperties.optimalTilingFeatures & vk::VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            {
                m_depthStencilAttachmentFormat = vk::VK_FORMAT_D32_SFLOAT_S8_UINT;
            }
            else
                throw tcu::NotSupportedError("No valid depth stencil attachment available");
        }
        const auto vertDescType = (m_isMesh ? vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : vk::VK_DESCRIPTOR_TYPE_MAX_ENUM);
        std::vector<vk::VkPushConstantRange> pcRanges;

#ifndef CTS_USES_VULKANSC
        // The mesh shading pipeline will contain a set with vertex data.
        if (m_isMesh)
        {
            vk::DescriptorSetLayoutBuilder setLayoutBuilder;
            vk::DescriptorPoolBuilder poolBuilder;

            setLayoutBuilder.addSingleBinding(vertDescType, vk::VK_SHADER_STAGE_MESH_BIT_EXT);
            m_setLayout = setLayoutBuilder.build(m_vk, device);

            poolBuilder.addType(vertDescType);
            m_descriptorPool =
                poolBuilder.build(m_vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

            m_descriptorSet = vk::makeDescriptorSet(m_vk, device, m_descriptorPool.get(), m_setLayout.get());
            pcRanges.push_back(vk::makePushConstantRange(vk::VK_SHADER_STAGE_MESH_BIT_EXT, 0u,
                                                         static_cast<uint32_t>(sizeof(uint32_t))));
        }
#endif // CTS_USES_VULKANSC

        m_pipelineLayout = vk::PipelineLayoutWrapper(m_pipelineConstructionType, m_vk, device, m_setLayout.get(),
                                                     de::dataOrNull(pcRanges));

        const vk::VkExtent3D imageExtent = {WIDTH, HEIGHT, 1};
        const ImageCreateInfo targetImageCreateInfo(vk::VK_IMAGE_TYPE_2D, m_colorAttachmentFormat, imageExtent, 1, 1,
                                                    vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_IMAGE_TILING_OPTIMAL,
                                                    vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                        vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                                        vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT);

        m_colorTargetImage = Image::createAndAlloc(m_vk, device, targetImageCreateInfo, m_context.getDefaultAllocator(),
                                                   m_context.getUniversalQueueFamilyIndex());

        const ImageCreateInfo depthStencilImageCreateInfo(
            vk::VK_IMAGE_TYPE_2D, m_depthStencilAttachmentFormat, imageExtent, 1, 1, vk::VK_SAMPLE_COUNT_1_BIT,
            vk::VK_IMAGE_TILING_OPTIMAL,
            vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT);

        m_depthStencilImage =
            Image::createAndAlloc(m_vk, device, depthStencilImageCreateInfo, m_context.getDefaultAllocator(),
                                  m_context.getUniversalQueueFamilyIndex());

        const ImageViewCreateInfo colorTargetViewInfo(m_colorTargetImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D,
                                                      m_colorAttachmentFormat);
        m_colorTargetView = vk::createImageView(m_vk, device, &colorTargetViewInfo);

        const ImageViewCreateInfo attachmentViewInfo(m_depthStencilImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D,
                                                     m_depthStencilAttachmentFormat);
        m_attachmentView = vk::createImageView(m_vk, device, &attachmentViewInfo);

        RenderPassCreateInfo renderPassCreateInfo;
        renderPassCreateInfo.addAttachment(AttachmentDescription(
            m_colorAttachmentFormat, vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_ATTACHMENT_LOAD_OP_LOAD,
            vk::VK_ATTACHMENT_STORE_OP_STORE, vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE, vk::VK_ATTACHMENT_STORE_OP_STORE,
            vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_IMAGE_LAYOUT_GENERAL));

        renderPassCreateInfo.addAttachment(AttachmentDescription(
            m_depthStencilAttachmentFormat, vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_ATTACHMENT_LOAD_OP_LOAD,
            vk::VK_ATTACHMENT_STORE_OP_STORE, vk::VK_ATTACHMENT_LOAD_OP_LOAD, vk::VK_ATTACHMENT_STORE_OP_STORE,
            vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));

        const vk::VkAttachmentReference colorAttachmentReference = {0, vk::VK_IMAGE_LAYOUT_GENERAL};

        const vk::VkAttachmentReference depthAttachmentReference = {
            1, vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        renderPassCreateInfo.addSubpass(SubpassDescription(vk::VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 0, nullptr, 1,
                                                           &colorAttachmentReference, nullptr, depthAttachmentReference,
                                                           0, nullptr));

        m_renderPass = vk::RenderPassWrapper(m_pipelineConstructionType, m_vk, device, &renderPassCreateInfo);

        const vk::VkVertexInputBindingDescription vertexInputBindingDescription = {
            0,
            (uint32_t)sizeof(tcu::Vec4) * 2,
            vk::VK_VERTEX_INPUT_RATE_VERTEX,
        };

        const vk::VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] = {
            {0u, 0u, vk::VK_FORMAT_R32G32B32A32_SFLOAT, 0u},
            {
                1u,
                0u,
                vk::VK_FORMAT_R32G32B32A32_SFLOAT,
                (uint32_t)(sizeof(float) * 4),
            }};

        m_vertexInputState = PipelineCreateInfo::VertexInputState(1, &vertexInputBindingDescription, 2,
                                                                  vertexInputAttributeDescriptions);

        std::vector<vk::VkViewport> viewports{{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}};
        std::vector<vk::VkRect2D> scissors{{{0u, 0u}, {0u, 0u}}};

        // Shaders.
        const auto &binaries       = m_context.getBinaryCollection();
        const vk::ShaderWrapper fs = vk::ShaderWrapper(m_vk, device, binaries.get(m_fragmentShaderName));
        const vk::ShaderWrapper vs =
            (m_isMesh ? vk::ShaderWrapper() : vk::ShaderWrapper(m_vk, device, binaries.get(m_vertexShaderName)));
        const vk::ShaderWrapper ms =
            (m_isMesh ? vk::ShaderWrapper(m_vk, device, binaries.get(m_meshShaderName)) : vk::ShaderWrapper());

        const PipelineCreateInfo::ColorBlendState::Attachment attachmentState;
        const PipelineCreateInfo::ColorBlendState colorBlendState(
            1u, static_cast<const vk::VkPipelineColorBlendAttachmentState *>(&attachmentState));
        const PipelineCreateInfo::RasterizerState rasterizerState;
        PipelineCreateInfo::DynamicState dynamicState;

        m_pipeline_1.setDefaultTopology(m_topology)
            .setDynamicState(static_cast<const vk::VkPipelineDynamicStateCreateInfo *>(&dynamicState))
            .setDefaultMultisampleState();

#ifndef CTS_USES_VULKANSC
        if (m_isMesh)
        {
            m_pipeline_1.setupPreRasterizationMeshShaderState(
                viewports, scissors, m_pipelineLayout, *m_renderPass, 0u, vk::ShaderWrapper(), ms,
                static_cast<const vk::VkPipelineRasterizationStateCreateInfo *>(&rasterizerState));
        }
        else
#endif // CTS_USES_VULKANSC
        {
            m_pipeline_1.setupVertexInputState(&m_vertexInputState)
                .setupPreRasterizationShaderState(
                    viewports, scissors, m_pipelineLayout, *m_renderPass, 0u, vs,
                    static_cast<const vk::VkPipelineRasterizationStateCreateInfo *>(&rasterizerState));
        }

        m_pipeline_1
            .setupFragmentShaderState(
                m_pipelineLayout, *m_renderPass, 0u, fs,
                static_cast<const vk::VkPipelineDepthStencilStateCreateInfo *>(&m_depthStencilState_1))
            .setupFragmentOutputState(*m_renderPass, 0u,
                                      static_cast<const vk::VkPipelineColorBlendStateCreateInfo *>(&colorBlendState))
            .setMonolithicPipelineLayout(m_pipelineLayout)
            .buildPipeline();

        m_pipeline_2.setDefaultTopology(m_topology)
            .setDynamicState(static_cast<const vk::VkPipelineDynamicStateCreateInfo *>(&dynamicState))
            .setDefaultMultisampleState();

#ifndef CTS_USES_VULKANSC
        if (m_isMesh)
        {
            m_pipeline_2.setupPreRasterizationMeshShaderState(
                viewports, scissors, m_pipelineLayout, *m_renderPass, 0u, vk::ShaderWrapper(), ms,
                static_cast<const vk::VkPipelineRasterizationStateCreateInfo *>(&rasterizerState));
        }
        else
#endif // CTS_USES_VULKANSC
        {
            m_pipeline_2.setupVertexInputState(&m_vertexInputState)
                .setupPreRasterizationShaderState(
                    viewports, scissors, m_pipelineLayout, *m_renderPass, 0u, vs,
                    static_cast<const vk::VkPipelineRasterizationStateCreateInfo *>(&rasterizerState));
        }

        m_pipeline_2
            .setupFragmentShaderState(
                m_pipelineLayout, *m_renderPass, 0u, fs,
                static_cast<const vk::VkPipelineDepthStencilStateCreateInfo *>(&m_depthStencilState_2))
            .setupFragmentOutputState(*m_renderPass, 0u,
                                      static_cast<const vk::VkPipelineColorBlendStateCreateInfo *>(&colorBlendState))
            .setMonolithicPipelineLayout(m_pipelineLayout)
            .buildPipeline();

        std::vector<vk::VkImageView> attachments(2);
        attachments[0] = *m_colorTargetView;
        attachments[1] = *m_attachmentView;

        const FramebufferCreateInfo framebufferCreateInfo(*m_renderPass, attachments, WIDTH, HEIGHT, 1);

        m_renderPass.createFramebuffer(m_vk, device, &framebufferCreateInfo,
                                       {m_colorTargetImage->object(), m_depthStencilImage->object()});

        const vk::VkDeviceSize dataSize = m_data.size() * sizeof(PositionColorVertex);
        const auto bufferUsage =
            (m_isMesh ? vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        m_vertexBuffer = Buffer::createAndAlloc(m_vk, device, BufferCreateInfo(dataSize, bufferUsage),
                                                m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);

        uint8_t *ptr = reinterpret_cast<unsigned char *>(m_vertexBuffer->getBoundMemory().getHostPtr());
        deMemcpy(ptr, &m_data[0], (size_t)dataSize);

        vk::flushAlloc(m_vk, device, m_vertexBuffer->getBoundMemory());

        // Update descriptor set for mesh shaders.
        if (m_isMesh)
        {
            vk::DescriptorSetUpdateBuilder updateBuilder;
            const auto location   = vk::DescriptorSetUpdateBuilder::Location::binding(0u);
            const auto bufferInfo = vk::makeDescriptorBufferInfo(m_vertexBuffer->object(), 0ull, dataSize);

            updateBuilder.writeSingle(m_descriptorSet.get(), location, vertDescType, &bufferInfo);
            updateBuilder.update(m_vk, device);
        }

        const CmdPoolCreateInfo cmdPoolCreateInfo(m_context.getUniversalQueueFamilyIndex());
        m_cmdPool   = vk::createCommandPool(m_vk, device, &cmdPoolCreateInfo);
        m_cmdBuffer = vk::allocateCommandBuffer(m_vk, device, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    }

    virtual tcu::TestStatus iterate(void)
    {
        DE_ASSERT(false);
        return tcu::TestStatus::fail("Implement iterate() method!");
    }

    void beginRenderPass(void)
    {
        const vk::VkClearColorValue clearColor = {{0.0f, 0.0f, 0.0f, 1.0f}};
        beginRenderPassWithClearColor(clearColor);
    }

    void beginRenderPassWithClearColor(const vk::VkClearColorValue &clearColor)
    {
        beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);

        initialTransitionColor2DImage(m_vk, *m_cmdBuffer, m_colorTargetImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL,
                                      vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT);
        initialTransitionDepthStencil2DImage(m_vk, *m_cmdBuffer, m_depthStencilImage->object(),
                                             vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, vk::VK_ACCESS_TRANSFER_WRITE_BIT,
                                             vk::VK_PIPELINE_STAGE_TRANSFER_BIT);

        const ImageSubresourceRange subresourceRangeImage(vk::VK_IMAGE_ASPECT_COLOR_BIT);
        m_vk.cmdClearColorImage(*m_cmdBuffer, m_colorTargetImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1,
                                &subresourceRangeImage);

        const vk::VkClearDepthStencilValue depthStencilClearValue = {0.0f, 0};

        const ImageSubresourceRange subresourceRangeDepthStencil[2] = {vk::VK_IMAGE_ASPECT_DEPTH_BIT,
                                                                       vk::VK_IMAGE_ASPECT_STENCIL_BIT};
        m_vk.cmdClearDepthStencilImage(*m_cmdBuffer, m_depthStencilImage->object(),
                                       vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &depthStencilClearValue, 2,
                                       subresourceRangeDepthStencil);

        vk::VkMemoryBarrier memBarrier;
        memBarrier.sType         = vk::VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memBarrier.pNext         = NULL;
        memBarrier.srcAccessMask = vk::VK_ACCESS_TRANSFER_WRITE_BIT;
        memBarrier.dstAccessMask = vk::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                   vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                   vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        m_vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                                vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                    vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                    vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                                0, 1, &memBarrier, 0, NULL, 0, NULL);

        transition2DImage(
            m_vk, *m_cmdBuffer, m_depthStencilImage->object(),
            vk::VK_IMAGE_ASPECT_DEPTH_BIT | vk::VK_IMAGE_ASPECT_STENCIL_BIT, vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, vk::VK_ACCESS_TRANSFER_WRITE_BIT,
            vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
            vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

        m_renderPass.begin(m_vk, *m_cmdBuffer, vk::makeRect2D(0, 0, WIDTH, HEIGHT));
    }

    void setDynamicViewportState(const uint32_t width, const uint32_t height)
    {
        vk::VkViewport viewport = vk::makeViewport(tcu::UVec2(width, height));
        vk::VkRect2D scissor    = vk::makeRect2D(tcu::UVec2(width, height));
        if (vk::isConstructionTypeShaderObject(m_pipelineConstructionType))
        {
#ifndef CTS_USES_VULKANSC
            m_vk.cmdSetViewportWithCount(*m_cmdBuffer, 1, &viewport);
            m_vk.cmdSetScissorWithCount(*m_cmdBuffer, 1, &scissor);
#else
            m_vk.cmdSetViewportWithCountEXT(*m_cmdBuffer, 1, &viewport);
            m_vk.cmdSetScissorWithCountEXT(*m_cmdBuffer, 1, &scissor);
#endif
        }
        else
        {
            m_vk.cmdSetViewport(*m_cmdBuffer, 0, 1, &viewport);
            m_vk.cmdSetScissor(*m_cmdBuffer, 0, 1, &scissor);
        }
    }

    void setDynamicViewportState(const uint32_t viewportCount, const vk::VkViewport *pViewports,
                                 const vk::VkRect2D *pScissors)
    {
        if (vk::isConstructionTypeShaderObject(m_pipelineConstructionType))
        {
#ifndef CTS_USES_VULKANSC
            m_vk.cmdSetViewportWithCount(*m_cmdBuffer, viewportCount, pViewports);
            m_vk.cmdSetScissorWithCount(*m_cmdBuffer, viewportCount, pScissors);
#else
            m_vk.cmdSetViewportWithCountEXT(*m_cmdBuffer, viewportCount, pViewports);
            m_vk.cmdSetScissorWithCountEXT(*m_cmdBuffer, viewportCount, pScissors);
#endif
        }
        else
        {
            m_vk.cmdSetViewport(*m_cmdBuffer, 0, viewportCount, pViewports);
            m_vk.cmdSetScissor(*m_cmdBuffer, 0, viewportCount, pScissors);
        }
    }

    void setDynamicRasterizationState(const float lineWidth = 1.0f, const float depthBiasConstantFactor = 0.0f,
                                      const float depthBiasClamp = 0.0f, const float depthBiasSlopeFactor = 0.0f)
    {
        m_vk.cmdSetLineWidth(*m_cmdBuffer, lineWidth);
        m_vk.cmdSetDepthBias(*m_cmdBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
    }

    void setDynamicBlendState(const float const1 = 0.0f, const float const2 = 0.0f, const float const3 = 0.0f,
                              const float const4 = 0.0f)
    {
        float blendConstantsants[4] = {const1, const2, const3, const4};
        m_vk.cmdSetBlendConstants(*m_cmdBuffer, blendConstantsants);
    }

    void setDynamicDepthStencilState(const float minDepthBounds = 0.0f, const float maxDepthBounds = 1.0f,
                                     const uint32_t stencilFrontCompareMask = 0xffffffffu,
                                     const uint32_t stencilFrontWriteMask   = 0xffffffffu,
                                     const uint32_t stencilFrontReference   = 0,
                                     const uint32_t stencilBackCompareMask  = 0xffffffffu,
                                     const uint32_t stencilBackWriteMask    = 0xffffffffu,
                                     const uint32_t stencilBackReference    = 0)
    {
        m_vk.cmdSetDepthBounds(*m_cmdBuffer, minDepthBounds, maxDepthBounds);
        m_vk.cmdSetStencilCompareMask(*m_cmdBuffer, vk::VK_STENCIL_FACE_FRONT_BIT, stencilFrontCompareMask);
        m_vk.cmdSetStencilWriteMask(*m_cmdBuffer, vk::VK_STENCIL_FACE_FRONT_BIT, stencilFrontWriteMask);
        m_vk.cmdSetStencilReference(*m_cmdBuffer, vk::VK_STENCIL_FACE_FRONT_BIT, stencilFrontReference);
        m_vk.cmdSetStencilCompareMask(*m_cmdBuffer, vk::VK_STENCIL_FACE_BACK_BIT, stencilBackCompareMask);
        m_vk.cmdSetStencilWriteMask(*m_cmdBuffer, vk::VK_STENCIL_FACE_BACK_BIT, stencilBackWriteMask);
        m_vk.cmdSetStencilReference(*m_cmdBuffer, vk::VK_STENCIL_FACE_BACK_BIT, stencilBackReference);
    }

#ifndef CTS_USES_VULKANSC
    void pushVertexOffset(const uint32_t vertexOffset,
                          const vk::VkShaderStageFlags stageFlags = vk::VK_SHADER_STAGE_MESH_BIT_EXT)
    {
        m_vk.cmdPushConstants(*m_cmdBuffer, *m_pipelineLayout, stageFlags, 0u, static_cast<uint32_t>(sizeof(uint32_t)),
                              &vertexOffset);
    }
#endif // CTS_USES_VULKANSC
};

class DepthBoundsParamTestInstance : public DepthStencilBaseCase
{
public:
    DepthBoundsParamTestInstance(Context &context, vk::PipelineConstructionType pipelineConstructionType,
                                 const ShaderMap &shaders)
        : DepthStencilBaseCase(context, pipelineConstructionType, shaders.at(glu::SHADERTYPE_VERTEX),
                               shaders.at(glu::SHADERTYPE_FRAGMENT), shaders.at(glu::SHADERTYPE_MESH))
    {
        m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, 1.0f, 0.375f, 1.0f), tcu::RGBA::green().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(0.0f, 1.0f, 0.375f, 1.0f), tcu::RGBA::green().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, 0.375f, 1.0f), tcu::RGBA::green().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(0.0f, -1.0f, 0.375f, 1.0f), tcu::RGBA::green().toVec()));

        m_data.push_back(PositionColorVertex(tcu::Vec4(0.0f, 1.0f, 0.625f, 1.0f), tcu::RGBA::green().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 1.0f, 0.625f, 1.0f), tcu::RGBA::green().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(0.0f, -1.0f, 0.625f, 1.0f), tcu::RGBA::green().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, -1.0f, 0.625f, 1.0f), tcu::RGBA::green().toVec()));

        m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));

        m_depthStencilState_1 =
            PipelineCreateInfo::DepthStencilState(VK_TRUE, VK_TRUE, vk::VK_COMPARE_OP_ALWAYS, VK_FALSE);

        // enable depth bounds test
        m_depthStencilState_2 =
            PipelineCreateInfo::DepthStencilState(VK_FALSE, VK_FALSE, vk::VK_COMPARE_OP_NEVER, VK_TRUE);

        DepthStencilBaseCase::initialize();
    }

    virtual tcu::TestStatus iterate(void)
    {
        tcu::TestLog &log         = m_context.getTestContext().getLog();
        const vk::VkQueue queue   = m_context.getUniversalQueue();
        const vk::VkDevice device = m_context.getDevice();

        beginRenderPass();

        // set states here
        setDynamicViewportState(WIDTH, HEIGHT);
        setDynamicRasterizationState();
        setDynamicBlendState();
        setDynamicDepthStencilState(0.5f, 0.75f);

#ifndef CTS_USES_VULKANSC
        if (m_isMesh)
        {
            m_vk.cmdBindDescriptorSets(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout.get(), 0u,
                                       1u, &m_descriptorSet.get(), 0u, nullptr);

            m_pipeline_1.bind(*m_cmdBuffer);
            pushVertexOffset(0u);
            m_vk.cmdDrawMeshTasksEXT(*m_cmdBuffer, 2u, 1u, 1u);
            pushVertexOffset(4u);
            m_vk.cmdDrawMeshTasksEXT(*m_cmdBuffer, 2u, 1u, 1u);

            m_pipeline_2.bind(*m_cmdBuffer);
            pushVertexOffset(8u);
            m_vk.cmdDrawMeshTasksEXT(*m_cmdBuffer, 2u, 1u, 1u);
        }
        else
#endif // CTS_USES_VULKANSC
        {
            const vk::VkDeviceSize vertexBufferOffset = 0;
            const vk::VkBuffer vertexBuffer           = m_vertexBuffer->object();
            m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

            m_pipeline_1.bind(*m_cmdBuffer);
            m_vk.cmdDraw(*m_cmdBuffer, 4, 1, 0, 0);
            m_vk.cmdDraw(*m_cmdBuffer, 4, 1, 4, 0);

            m_pipeline_2.bind(*m_cmdBuffer);
            m_vk.cmdDraw(*m_cmdBuffer, 4, 1, 8, 0);
        }

        m_renderPass.end(m_vk, *m_cmdBuffer);
        endCommandBuffer(m_vk, *m_cmdBuffer);

        submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

        // validation
        {
            tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat),
                                          (int)(0.5f + static_cast<float>(WIDTH)),
                                          (int)(0.5f + static_cast<float>(HEIGHT)));
            referenceFrame.allocLevel(0);

            const int32_t frameWidth  = referenceFrame.getWidth();
            const int32_t frameHeight = referenceFrame.getHeight();

            tcu::clear(referenceFrame.getLevel(0), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

            for (int y = 0; y < frameHeight; y++)
            {
                const float yCoord = (float)(y / (0.5 * frameHeight)) - 1.0f;

                for (int x = 0; x < frameWidth; x++)
                {
                    const float xCoord = (float)(x / (0.5 * frameWidth)) - 1.0f;

                    if (xCoord >= 0.0f && xCoord <= 1.0f && yCoord >= -1.0f && yCoord <= 1.0f)
                        referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), x, y);
                    else
                        referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), x, y);
                }
            }

            const vk::VkOffset3D zeroOffset = {0, 0, 0};
            const tcu::ConstPixelBufferAccess renderedFrame =
                m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(), vk::VK_IMAGE_LAYOUT_GENERAL,
                                                zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

            if (!tcu::fuzzyCompare(log, "Result", "Image comparison result", referenceFrame.getLevel(0), renderedFrame,
                                   0.05f, tcu::COMPARE_LOG_RESULT))
            {
                return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Image verification failed");
            }

            return tcu::TestStatus(QP_TEST_RESULT_PASS, "Image verification passed");
        }
    }
};

class DepthBoundsTestInstance : public DynamicStateBaseClass
{
public:
    enum
    {
        DEPTH_BOUNDS_MIN   = 0,
        DEPTH_BOUNDS_MAX   = 1,
        DEPTH_BOUNDS_COUNT = 2
    };
    static const float depthBounds[DEPTH_BOUNDS_COUNT];

    DepthBoundsTestInstance(Context &context, vk::PipelineConstructionType pipelineConstructionType,
                            const ShaderMap &shaders);
    virtual void initRenderPass(const vk::VkDevice device);
    virtual void initFramebuffer(const vk::VkDevice device);
    virtual void initPipeline(const vk::VkDevice device);
    virtual tcu::TestStatus iterate(void);

private:
    const vk::VkFormat m_depthAttachmentFormat;

    de::SharedPtr<Draw::Image> m_depthImage;
    vk::Move<vk::VkImageView> m_depthView;
};

const float DepthBoundsTestInstance::depthBounds[DEPTH_BOUNDS_COUNT] = {0.3f, 0.9f};

DepthBoundsTestInstance::DepthBoundsTestInstance(Context &context,
                                                 vk::PipelineConstructionType pipelineConstructionType,
                                                 const ShaderMap &shaders)
    : DynamicStateBaseClass(context, pipelineConstructionType, shaders.at(glu::SHADERTYPE_VERTEX),
                            shaders.at(glu::SHADERTYPE_FRAGMENT), shaders.at(glu::SHADERTYPE_MESH))
    , m_depthAttachmentFormat(vk::VK_FORMAT_D16_UNORM)
{
    const vk::VkDevice device             = m_context.getDevice();
    const vk::VkExtent3D depthImageExtent = {WIDTH, HEIGHT, 1};
    const ImageCreateInfo depthImageCreateInfo(vk::VK_IMAGE_TYPE_2D, m_depthAttachmentFormat, depthImageExtent, 1, 1,
                                               vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_IMAGE_TILING_OPTIMAL,
                                               vk::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                                   vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    m_depthImage = Image::createAndAlloc(m_vk, device, depthImageCreateInfo, m_context.getDefaultAllocator(),
                                         m_context.getUniversalQueueFamilyIndex());

    const ImageViewCreateInfo depthViewInfo(m_depthImage->object(), vk::VK_IMAGE_VIEW_TYPE_2D, m_depthAttachmentFormat);
    m_depthView = vk::createImageView(m_vk, device, &depthViewInfo);

    m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
    m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
    m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
    m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));

    DynamicStateBaseClass::initialize();
}

void DepthBoundsTestInstance::initRenderPass(const vk::VkDevice device)
{
    RenderPassCreateInfo renderPassCreateInfo;
    renderPassCreateInfo.addAttachment(AttachmentDescription(
        m_colorAttachmentFormat, vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_ATTACHMENT_LOAD_OP_LOAD,
        vk::VK_ATTACHMENT_STORE_OP_STORE, vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE, vk::VK_ATTACHMENT_STORE_OP_STORE,
        vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_IMAGE_LAYOUT_GENERAL));
    renderPassCreateInfo.addAttachment(AttachmentDescription(
        m_depthAttachmentFormat, vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_ATTACHMENT_LOAD_OP_LOAD,
        vk::VK_ATTACHMENT_STORE_OP_STORE, vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE, vk::VK_ATTACHMENT_STORE_OP_STORE,
        vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL));

    const vk::VkAttachmentReference colorAttachmentReference = {0, vk::VK_IMAGE_LAYOUT_GENERAL};

    const vk::VkAttachmentReference depthAttachmentReference = {1, vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};

    renderPassCreateInfo.addSubpass(SubpassDescription(vk::VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 0, nullptr, 1,
                                                       &colorAttachmentReference, nullptr, depthAttachmentReference, 0,
                                                       nullptr));

    m_renderPass = vk::RenderPassWrapper(m_pipelineConstructionType, m_vk, device, &renderPassCreateInfo);
}

void DepthBoundsTestInstance::initFramebuffer(const vk::VkDevice device)
{
    std::vector<vk::VkImageView> attachments(2);
    attachments[0] = *m_colorTargetView;
    attachments[1] = *m_depthView;

    const FramebufferCreateInfo framebufferCreateInfo(*m_renderPass, attachments, WIDTH, HEIGHT, 1);

    m_renderPass.createFramebuffer(m_vk, device, &framebufferCreateInfo,
                                   {m_colorTargetImage->object(), m_depthImage->object()});
}

void DepthBoundsTestInstance::initPipeline(const vk::VkDevice device)
{
    // Shaders.
    const auto &binaries       = m_context.getBinaryCollection();
    const vk::ShaderWrapper fs = vk::ShaderWrapper(m_vk, device, binaries.get(m_fragmentShaderName));
    const vk::ShaderWrapper vs =
        (m_isMesh ? vk::ShaderWrapper() : vk::ShaderWrapper(m_vk, device, binaries.get(m_vertexShaderName)));
    const vk::ShaderWrapper ms =
        (m_isMesh ? vk::ShaderWrapper(m_vk, device, binaries.get(m_meshShaderName)) : vk::ShaderWrapper());
    std::vector<vk::VkViewport> viewports{{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}};
    std::vector<vk::VkRect2D> scissors{{{0u, 0u}, {0u, 0u}}};

    const PipelineCreateInfo::ColorBlendState::Attachment attachmentState;
    const PipelineCreateInfo::ColorBlendState colorBlendState(
        1u, static_cast<const vk::VkPipelineColorBlendAttachmentState *>(&attachmentState));
    const PipelineCreateInfo::RasterizerState rasterizerState;
    const PipelineCreateInfo::DepthStencilState::StencilOpState stencilOpState(
        vk::VK_STENCIL_OP_KEEP, vk::VK_STENCIL_OP_KEEP, vk::VK_STENCIL_OP_KEEP);
    const PipelineCreateInfo::DepthStencilState depthStencilState(false, false, vk::VK_COMPARE_OP_NEVER, true, 0u,
                                                                  stencilOpState, stencilOpState);
    const PipelineCreateInfo::DynamicState dynamicState;

    m_pipeline.setDefaultTopology(m_topology)
        .setDynamicState(static_cast<const vk::VkPipelineDynamicStateCreateInfo *>(&dynamicState))
        .setDefaultMultisampleState();

#ifndef CTS_USES_VULKANSC
    if (m_isMesh)
    {
        m_pipeline.setupPreRasterizationMeshShaderState(
            viewports, scissors, m_pipelineLayout, *m_renderPass, 0u, vk::ShaderWrapper(), ms,
            static_cast<const vk::VkPipelineRasterizationStateCreateInfo *>(&rasterizerState));
    }
    else
#endif // CTS_USES_VULKANSC
    {
        m_pipeline.setupVertexInputState(&m_vertexInputState)
            .setupPreRasterizationShaderState(
                viewports, scissors, m_pipelineLayout, *m_renderPass, 0u, vs,
                static_cast<const vk::VkPipelineRasterizationStateCreateInfo *>(&rasterizerState));
    }

    m_pipeline
        .setupFragmentShaderState(m_pipelineLayout, *m_renderPass, 0u, fs,
                                  static_cast<const vk::VkPipelineDepthStencilStateCreateInfo *>(&depthStencilState))
        .setupFragmentOutputState(*m_renderPass, 0u,
                                  static_cast<const vk::VkPipelineColorBlendStateCreateInfo *>(&colorBlendState))
        .setMonolithicPipelineLayout(m_pipelineLayout)
        .buildPipeline();
}

tcu::TestStatus DepthBoundsTestInstance::iterate(void)
{
    tcu::TestLog &log         = m_context.getTestContext().getLog();
    const vk::VkQueue queue   = m_context.getUniversalQueue();
    const vk::VkDevice device = m_context.getDevice();

    // Prepare depth image
    tcu::Texture2D depthData(vk::mapVkFormat(m_depthAttachmentFormat), (int)(0.5f + static_cast<float>(WIDTH)),
                             (int)(0.5f + static_cast<float>(HEIGHT)));
    depthData.allocLevel(0);

    const int32_t depthDataWidth  = depthData.getWidth();
    const int32_t depthDataHeight = depthData.getHeight();

    for (int y = 0; y < depthDataHeight; ++y)
        for (int x = 0; x < depthDataWidth; ++x)
            depthData.getLevel(0).setPixDepth((float)(y * depthDataWidth + x % 11) / 10, x, y);

    const vk::VkDeviceSize dataSize = depthData.getLevel(0).getWidth() * depthData.getLevel(0).getHeight() *
                                      tcu::getPixelSize(mapVkFormat(m_depthAttachmentFormat));
    de::SharedPtr<Draw::Buffer> stageBuffer =
        Buffer::createAndAlloc(m_vk, device, BufferCreateInfo(dataSize, vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                               m_context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);

    uint8_t *ptr = reinterpret_cast<unsigned char *>(stageBuffer->getBoundMemory().getHostPtr());
    deMemcpy(ptr, depthData.getLevel(0).getDataPtr(), (size_t)dataSize);

    vk::flushAlloc(m_vk, device, stageBuffer->getBoundMemory());

    beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);

    initialTransitionDepth2DImage(m_vk, *m_cmdBuffer, m_depthImage->object(), vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT);

    const vk::VkBufferImageCopy bufferImageCopy = {
        (vk::VkDeviceSize)0, // VkDeviceSize bufferOffset;
        0u,                  // uint32_t bufferRowLength;
        0u,                  // uint32_t bufferImageHeight;
        vk::makeImageSubresourceLayers(vk::VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u,
                                       1u), // VkImageSubresourceLayers imageSubresource;
        vk::makeOffset3D(0, 0, 0),          // VkOffset3D imageOffset;
        vk::makeExtent3D(WIDTH, HEIGHT, 1u) // VkExtent3D imageExtent;
    };
    m_vk.cmdCopyBufferToImage(*m_cmdBuffer, stageBuffer->object(), m_depthImage->object(),
                              vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &bufferImageCopy);

    transition2DImage(m_vk, *m_cmdBuffer, m_depthImage->object(), vk::VK_IMAGE_ASPECT_DEPTH_BIT,
                      vk::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, vk::VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                      vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                      vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                      vk::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | vk::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

    const vk::VkClearColorValue clearColor = {{1.0f, 1.0f, 1.0f, 1.0f}};
    beginRenderPassWithClearColor(clearColor, true);

    // Bind states
    setDynamicViewportState(WIDTH, HEIGHT);
    setDynamicRasterizationState();
    setDynamicBlendState();
    setDynamicDepthStencilState(depthBounds[DEPTH_BOUNDS_MIN], depthBounds[DEPTH_BOUNDS_MAX]);

    m_pipeline.bind(*m_cmdBuffer);

#ifndef CTS_USES_VULKANSC
    if (m_isMesh)
    {
        const auto numVert = static_cast<uint32_t>(m_data.size());
        DE_ASSERT(numVert >= 2u);

        m_vk.cmdBindDescriptorSets(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout.get(), 0u, 1u,
                                   &m_descriptorSet.get(), 0u, nullptr);
        pushVertexOffset(0u, *m_pipelineLayout);
        m_vk.cmdDrawMeshTasksEXT(*m_cmdBuffer, numVert - 2u, 1u, 1u);
    }
    else
#endif // CTS_USES_VULKANSC
    {

        const vk::VkDeviceSize vertexBufferOffset = 0;
        const vk::VkBuffer vertexBuffer           = m_vertexBuffer->object();
        m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

        m_vk.cmdDraw(*m_cmdBuffer, static_cast<uint32_t>(m_data.size()), 1, 0, 0);
    }

    m_renderPass.end(m_vk, *m_cmdBuffer);
    endCommandBuffer(m_vk, *m_cmdBuffer);

    submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

    // Validation
    {
        tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5f + static_cast<float>(WIDTH)),
                                      (int)(0.5f + static_cast<float>(HEIGHT)));
        referenceFrame.allocLevel(0);

        const int32_t frameWidth  = referenceFrame.getWidth();
        const int32_t frameHeight = referenceFrame.getHeight();

        tcu::clear(referenceFrame.getLevel(0), tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

        for (int y = 0; y < frameHeight; ++y)
            for (int x = 0; x < frameWidth; ++x)
                if (depthData.getLevel(0).getPixDepth(x, y) >= depthBounds[DEPTH_BOUNDS_MIN] &&
                    depthData.getLevel(0).getPixDepth(x, y) <= depthBounds[DEPTH_BOUNDS_MAX])
                    referenceFrame.getLevel(0).setPixel(tcu::RGBA::green().toVec(), x, y);

        const vk::VkOffset3D zeroOffset = {0, 0, 0};
        const tcu::ConstPixelBufferAccess renderedFrame =
            m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(), vk::VK_IMAGE_LAYOUT_GENERAL,
                                            zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

        if (!tcu::fuzzyCompare(log, "Result", "Image comparison result", referenceFrame.getLevel(0), renderedFrame,
                               0.05f, tcu::COMPARE_LOG_RESULT))
        {
            return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Image verification failed");
        }

        return tcu::TestStatus(QP_TEST_RESULT_PASS, "Image verification passed");
    }
}

class StencilParamsBasicTestInstance : public DepthStencilBaseCase
{
protected:
    uint32_t m_writeMask;
    uint32_t m_readMask;
    uint32_t m_expectedValue;
    tcu::Vec4 m_expectedColor;

public:
    StencilParamsBasicTestInstance(Context &context, vk::PipelineConstructionType pipelineConstructionType,
                                   const char *vertexShaderName, const char *fragmentShaderName,
                                   const char *meshShaderName, const uint32_t writeMask, const uint32_t readMask,
                                   const uint32_t expectedValue, const tcu::Vec4 expectedColor)
        : DepthStencilBaseCase(context, pipelineConstructionType, vertexShaderName, fragmentShaderName, meshShaderName)
        , m_expectedColor(1.0f, 1.0f, 1.0f, 1.0f)
    {
        m_writeMask     = writeMask;
        m_readMask      = readMask;
        m_expectedValue = expectedValue;
        m_expectedColor = expectedColor;

        m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));

        m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));

        const PipelineCreateInfo::DepthStencilState::StencilOpState frontState_1 =
            PipelineCreateInfo::DepthStencilState::StencilOpState(vk::VK_STENCIL_OP_REPLACE, vk::VK_STENCIL_OP_REPLACE,
                                                                  vk::VK_STENCIL_OP_REPLACE, vk::VK_COMPARE_OP_ALWAYS);

        const PipelineCreateInfo::DepthStencilState::StencilOpState backState_1 =
            PipelineCreateInfo::DepthStencilState::StencilOpState(vk::VK_STENCIL_OP_REPLACE, vk::VK_STENCIL_OP_REPLACE,
                                                                  vk::VK_STENCIL_OP_REPLACE, vk::VK_COMPARE_OP_ALWAYS);

        const PipelineCreateInfo::DepthStencilState::StencilOpState frontState_2 =
            PipelineCreateInfo::DepthStencilState::StencilOpState(vk::VK_STENCIL_OP_REPLACE, vk::VK_STENCIL_OP_REPLACE,
                                                                  vk::VK_STENCIL_OP_REPLACE, vk::VK_COMPARE_OP_EQUAL);

        const PipelineCreateInfo::DepthStencilState::StencilOpState backState_2 =
            PipelineCreateInfo::DepthStencilState::StencilOpState(vk::VK_STENCIL_OP_REPLACE, vk::VK_STENCIL_OP_REPLACE,
                                                                  vk::VK_STENCIL_OP_REPLACE, vk::VK_COMPARE_OP_EQUAL);

        // enable stencil test
        m_depthStencilState_1 = PipelineCreateInfo::DepthStencilState(VK_FALSE, VK_FALSE, vk::VK_COMPARE_OP_NEVER,
                                                                      VK_FALSE, VK_TRUE, frontState_1, backState_1);

        m_depthStencilState_2 = PipelineCreateInfo::DepthStencilState(VK_FALSE, VK_FALSE, vk::VK_COMPARE_OP_NEVER,
                                                                      VK_FALSE, VK_TRUE, frontState_2, backState_2);

        DepthStencilBaseCase::initialize();
    }

    virtual tcu::TestStatus iterate(void)
    {
        tcu::TestLog &log         = m_context.getTestContext().getLog();
        const vk::VkQueue queue   = m_context.getUniversalQueue();
        const vk::VkDevice device = m_context.getDevice();

        beginRenderPass();

        // set states here
        setDynamicViewportState(WIDTH, HEIGHT);
        setDynamicRasterizationState();
        setDynamicBlendState();

#ifndef CTS_USES_VULKANSC
        if (m_isMesh)
        {
            m_vk.cmdBindDescriptorSets(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout.get(), 0u,
                                       1u, &m_descriptorSet.get(), 0u, nullptr);

            m_pipeline_1.bind(*m_cmdBuffer);
            setDynamicDepthStencilState(0.0f, 1.0f, 0xFF, m_writeMask, 0x0F, 0xFF, m_writeMask, 0x0F);
            pushVertexOffset(0u);
            m_vk.cmdDrawMeshTasksEXT(*m_cmdBuffer, 2u, 1u, 1u);

            m_pipeline_2.bind(*m_cmdBuffer);
            setDynamicDepthStencilState(0.0f, 1.0f, m_readMask, 0xFF, m_expectedValue, m_readMask, 0xFF,
                                        m_expectedValue);
            pushVertexOffset(4u);
            m_vk.cmdDrawMeshTasksEXT(*m_cmdBuffer, 2u, 1u, 1u);
        }
        else
#endif // CTS_USES_VULKANSC
        {
            const vk::VkDeviceSize vertexBufferOffset = 0;
            const vk::VkBuffer vertexBuffer           = m_vertexBuffer->object();
            m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

            m_pipeline_1.bind(*m_cmdBuffer);
            setDynamicDepthStencilState(0.0f, 1.0f, 0xFF, m_writeMask, 0x0F, 0xFF, m_writeMask, 0x0F);
            m_vk.cmdDraw(*m_cmdBuffer, 4, 1, 0, 0);

            m_pipeline_2.bind(*m_cmdBuffer);
            setDynamicDepthStencilState(0.0f, 1.0f, m_readMask, 0xFF, m_expectedValue, m_readMask, 0xFF,
                                        m_expectedValue);
            m_vk.cmdDraw(*m_cmdBuffer, 4, 1, 4, 0);
        }

        m_renderPass.end(m_vk, *m_cmdBuffer);
        endCommandBuffer(m_vk, *m_cmdBuffer);

        submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

        // validation
        {
            tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat),
                                          (int)(0.5f + static_cast<float>(WIDTH)),
                                          (int)(0.5f + static_cast<float>(HEIGHT)));
            referenceFrame.allocLevel(0);

            const int32_t frameWidth  = referenceFrame.getWidth();
            const int32_t frameHeight = referenceFrame.getHeight();

            for (int y = 0; y < frameHeight; y++)
            {
                const float yCoord = (float)(y / (0.5 * frameHeight)) - 1.0f;

                for (int x = 0; x < frameWidth; x++)
                {
                    const float xCoord = (float)(x / (0.5 * frameWidth)) - 1.0f;

                    if (xCoord >= -1.0f && xCoord <= 1.0f && yCoord >= -1.0f && yCoord <= 1.0f)
                        referenceFrame.getLevel(0).setPixel(m_expectedColor, x, y);
                }
            }

            const vk::VkOffset3D zeroOffset = {0, 0, 0};
            const tcu::ConstPixelBufferAccess renderedFrame =
                m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(), vk::VK_IMAGE_LAYOUT_GENERAL,
                                                zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

            if (!tcu::fuzzyCompare(log, "Result", "Image comparison result", referenceFrame.getLevel(0), renderedFrame,
                                   0.05f, tcu::COMPARE_LOG_RESULT))
            {
                return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Image verification failed");
            }

            return tcu::TestStatus(QP_TEST_RESULT_PASS, "Image verification passed");
        }
    }
};

void checkNothing(Context &)
{
}

void checkMeshShaderSupport(Context &context)
{
    context.requireDeviceFunctionality("VK_EXT_mesh_shader");
}

class StencilParamsBasicTestCase : public TestCase
{
protected:
    TestInstance *createInstance(Context &context) const
    {
        return new StencilParamsBasicTestInstance(
            context, m_pipelineConstructionType, (m_isMesh ? nullptr : "VertexFetch.vert"), "VertexFetch.frag",
            (m_isMesh ? "VertexFetch.mesh" : nullptr), m_writeMask, m_readMask, m_expectedValue, m_expectedColor);
    }

    virtual void initPrograms(vk::SourceCollections &programCollection) const
    {
        programCollection.glslSources.add("VertexFetch.frag") << glu::FragmentSource(
            ShaderSourceProvider::getSource(m_testCtx.getArchive(), "vulkan/dynamic_state/VertexFetch.frag"));

        if (m_isMesh)
        {
            programCollection.glslSources.add("VertexFetch.mesh")
                << glu::MeshSource(
                       ShaderSourceProvider::getSource(m_testCtx.getArchive(), "vulkan/dynamic_state/VertexFetch.mesh"))
                << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
        }
        else
        {
            programCollection.glslSources.add("VertexFetch.vert") << glu::VertexSource(
                ShaderSourceProvider::getSource(m_testCtx.getArchive(), "vulkan/dynamic_state/VertexFetch.vert"));
        }
    }

    virtual void checkSupport(Context &context) const
    {
        checkMeshShaderSupport(context);
        checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                              m_pipelineConstructionType);
    }

    vk::PipelineConstructionType m_pipelineConstructionType;
    uint32_t m_writeMask;
    uint32_t m_readMask;
    uint32_t m_expectedValue;
    tcu::Vec4 m_expectedColor;
    const bool m_isMesh;

public:
    StencilParamsBasicTestCase(tcu::TestContext &context, const std::string &name,
                               const vk::PipelineConstructionType pipelineConstructionType, const uint32_t writeMask,
                               const uint32_t readMask, const uint32_t expectedValue, const tcu::Vec4 expectedColor,
                               const bool isMesh)
        : TestCase(context, name)
        , m_pipelineConstructionType(pipelineConstructionType)
        , m_writeMask(writeMask)
        , m_readMask(readMask)
        , m_expectedValue(expectedValue)
        , m_expectedColor(expectedColor)
        , m_isMesh(isMesh)
    {
    }
};

class StencilParamsAdvancedTestInstance : public DepthStencilBaseCase
{
public:
    StencilParamsAdvancedTestInstance(Context &context, vk::PipelineConstructionType pipelineConstructionType,
                                      const ShaderMap &shaders)
        : DepthStencilBaseCase(context, pipelineConstructionType, shaders.at(glu::SHADERTYPE_VERTEX),
                               shaders.at(glu::SHADERTYPE_FRAGMENT), shaders.at(glu::SHADERTYPE_MESH))
    {
        m_data.push_back(PositionColorVertex(tcu::Vec4(-0.5f, 0.5f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(0.5f, 0.5f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(-0.5f, -0.5f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(0.5f, -0.5f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));

        m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));

        const PipelineCreateInfo::DepthStencilState::StencilOpState frontState_1 =
            PipelineCreateInfo::DepthStencilState::StencilOpState(vk::VK_STENCIL_OP_REPLACE, vk::VK_STENCIL_OP_REPLACE,
                                                                  vk::VK_STENCIL_OP_REPLACE, vk::VK_COMPARE_OP_ALWAYS);

        const PipelineCreateInfo::DepthStencilState::StencilOpState backState_1 =
            PipelineCreateInfo::DepthStencilState::StencilOpState(vk::VK_STENCIL_OP_REPLACE, vk::VK_STENCIL_OP_REPLACE,
                                                                  vk::VK_STENCIL_OP_REPLACE, vk::VK_COMPARE_OP_ALWAYS);

        const PipelineCreateInfo::DepthStencilState::StencilOpState frontState_2 =
            PipelineCreateInfo::DepthStencilState::StencilOpState(vk::VK_STENCIL_OP_REPLACE, vk::VK_STENCIL_OP_REPLACE,
                                                                  vk::VK_STENCIL_OP_REPLACE,
                                                                  vk::VK_COMPARE_OP_NOT_EQUAL);

        const PipelineCreateInfo::DepthStencilState::StencilOpState backState_2 =
            PipelineCreateInfo::DepthStencilState::StencilOpState(vk::VK_STENCIL_OP_REPLACE, vk::VK_STENCIL_OP_REPLACE,
                                                                  vk::VK_STENCIL_OP_REPLACE,
                                                                  vk::VK_COMPARE_OP_NOT_EQUAL);

        // enable stencil test
        m_depthStencilState_1 = PipelineCreateInfo::DepthStencilState(VK_FALSE, VK_FALSE, vk::VK_COMPARE_OP_NEVER,
                                                                      VK_FALSE, VK_TRUE, frontState_1, backState_1);

        m_depthStencilState_2 = PipelineCreateInfo::DepthStencilState(VK_FALSE, VK_FALSE, vk::VK_COMPARE_OP_NEVER,
                                                                      VK_FALSE, VK_TRUE, frontState_2, backState_2);

        DepthStencilBaseCase::initialize();
    }

    virtual tcu::TestStatus iterate(void)
    {
        tcu::TestLog &log         = m_context.getTestContext().getLog();
        const vk::VkQueue queue   = m_context.getUniversalQueue();
        const vk::VkDevice device = m_context.getDevice();

        beginRenderPass();

        // set states here
        setDynamicViewportState(WIDTH, HEIGHT);
        setDynamicRasterizationState();
        setDynamicBlendState();

#ifndef CTS_USES_VULKANSC
        if (m_isMesh)
        {
            m_vk.cmdBindDescriptorSets(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout.get(), 0u,
                                       1u, &m_descriptorSet.get(), 0u, nullptr);

            m_pipeline_1.bind(*m_cmdBuffer);
            setDynamicDepthStencilState(0.0f, 1.0f, 0xFF, 0x0E, 0x0F, 0xFF, 0x0E, 0x0F);
            pushVertexOffset(0u);
            m_vk.cmdDrawMeshTasksEXT(*m_cmdBuffer, 2u, 1u, 1u);

            m_pipeline_2.bind(*m_cmdBuffer);
            setDynamicDepthStencilState(0.0f, 1.0f, 0xFF, 0xFF, 0x0E, 0xFF, 0xFF, 0x0E);
            pushVertexOffset(4u);
            m_vk.cmdDrawMeshTasksEXT(*m_cmdBuffer, 2u, 1u, 1u);
        }
        else
#endif // CTS_USES_VULKANSC
        {
            const vk::VkDeviceSize vertexBufferOffset = 0;
            const vk::VkBuffer vertexBuffer           = m_vertexBuffer->object();
            m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

            m_pipeline_1.bind(*m_cmdBuffer);
            setDynamicDepthStencilState(0.0f, 1.0f, 0xFF, 0x0E, 0x0F, 0xFF, 0x0E, 0x0F);
            m_vk.cmdDraw(*m_cmdBuffer, 4, 1, 0, 0);

            m_pipeline_2.bind(*m_cmdBuffer);
            setDynamicDepthStencilState(0.0f, 1.0f, 0xFF, 0xFF, 0x0E, 0xFF, 0xFF, 0x0E);
            m_vk.cmdDraw(*m_cmdBuffer, 4, 1, 4, 0);
        }

        m_renderPass.end(m_vk, *m_cmdBuffer);
        endCommandBuffer(m_vk, *m_cmdBuffer);

        submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

        // validation
        {
            tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat),
                                          (int)(0.5f + static_cast<float>(WIDTH)),
                                          (int)(0.5f + static_cast<float>(HEIGHT)));
            referenceFrame.allocLevel(0);

            const int32_t frameWidth  = referenceFrame.getWidth();
            const int32_t frameHeight = referenceFrame.getHeight();

            for (int y = 0; y < frameHeight; y++)
            {
                const float yCoord = (float)(y / (0.5 * frameHeight)) - 1.0f;

                for (int x = 0; x < frameWidth; x++)
                {
                    const float xCoord = (float)(x / (0.5 * frameWidth)) - 1.0f;

                    if (xCoord >= -0.5f && xCoord <= 0.5f && yCoord >= -0.5f && yCoord <= 0.5f)
                        referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), x, y);
                    else
                        referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), x, y);
                }
            }

            const vk::VkOffset3D zeroOffset = {0, 0, 0};
            const tcu::ConstPixelBufferAccess renderedFrame =
                m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(), vk::VK_IMAGE_LAYOUT_GENERAL,
                                                zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

            if (!tcu::fuzzyCompare(log, "Result", "Image comparison result", referenceFrame.getLevel(0), renderedFrame,
                                   0.05f, tcu::COMPARE_LOG_RESULT))
            {
                return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Image verification failed");
            }

            return tcu::TestStatus(QP_TEST_RESULT_PASS, "Image verification passed");
        }
    }
};

void checkDepthBoundsSupport(Context &context)
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_DEPTH_BOUNDS);
}

#ifndef CTS_USES_VULKANSC
void checkDepthBoundsAndMeshShaderSupport(Context &context)
{
    checkDepthBoundsSupport(context);
    checkMeshShaderSupport(context);
}
#endif // CTS_USES_VULKANSC

} // namespace

// Tests for depth stencil state
DynamicStateDSTests::DynamicStateDSTests(tcu::TestContext &testCtx,
                                         vk::PipelineConstructionType pipelineConstructionType)
    : TestCaseGroup(testCtx, "ds_state")
    , m_pipelineConstructionType(pipelineConstructionType)
{
    /* Left blank on purpose */
}

DynamicStateDSTests::~DynamicStateDSTests()
{
}

void DynamicStateDSTests::init(void)
{
    ShaderMap basePaths;
    basePaths[glu::SHADERTYPE_FRAGMENT] = "vulkan/dynamic_state/VertexFetch.frag";
    basePaths[glu::SHADERTYPE_MESH]     = nullptr;
    basePaths[glu::SHADERTYPE_VERTEX]   = nullptr;

    for (int useMeshIdx = 0; useMeshIdx < 2; ++useMeshIdx)
    {
        const bool useMesh = (useMeshIdx > 0);
        ShaderMap shaderPaths(basePaths);
        FunctionSupport0::Function depthBoundsCheck = nullptr;
        FunctionSupport0::Function meshSupportCheck = (useMesh ? checkMeshShaderSupport : checkNothing);
        std::string nameSuffix;

        if (useMesh)
        {
#ifndef CTS_USES_VULKANSC
            shaderPaths[glu::SHADERTYPE_MESH] = "vulkan/dynamic_state/VertexFetch.mesh";
            depthBoundsCheck                  = checkDepthBoundsAndMeshShaderSupport;
            nameSuffix                        = "_mesh";
#else
            continue;
#endif // CTS_USES_VULKANSC
        }
        else
        {
            shaderPaths[glu::SHADERTYPE_VERTEX] = "vulkan/dynamic_state/VertexFetch.vert";
            depthBoundsCheck                    = checkDepthBoundsSupport;
        }

        addChild(new InstanceFactory<DepthBoundsParamTestInstance, FunctionSupport0>(
            m_testCtx, "depth_bounds_1" + nameSuffix, m_pipelineConstructionType, shaderPaths, depthBoundsCheck));
        addChild(new InstanceFactory<DepthBoundsTestInstance, FunctionSupport0>(
            m_testCtx, "depth_bounds_2" + nameSuffix, m_pipelineConstructionType, shaderPaths, depthBoundsCheck));
#ifndef CTS_USES_VULKANSC
        addChild(new StencilParamsBasicTestCase(m_testCtx, "stencil_params_basic_1" + nameSuffix,
                                                m_pipelineConstructionType, 0x0D, 0x06, 0x05,
                                                tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), useMesh));
        addChild(new StencilParamsBasicTestCase(m_testCtx, "stencil_params_basic_2" + nameSuffix,
                                                m_pipelineConstructionType, 0x06, 0x02, 0x05,
                                                tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), useMesh));
#endif // CTS_USES_VULKANSC
        addChild(new InstanceFactory<StencilParamsAdvancedTestInstance, FunctionSupport0>(
            m_testCtx, "stencil_params_advanced" + nameSuffix, m_pipelineConstructionType, shaderPaths,
            meshSupportCheck));
    }
}

} // namespace DynamicState
} // namespace vkt
