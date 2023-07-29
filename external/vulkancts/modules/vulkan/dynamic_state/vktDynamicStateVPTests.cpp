/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Intel Corporation
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
 * \brief Dynamic State Viewport Tests
 *//*--------------------------------------------------------------------*/

#include "vktDynamicStateVPTests.hpp"

#include "vktDynamicStateBaseClass.hpp"
#include "vktDynamicStateTestCaseUtil.hpp"

#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuRGBA.hpp"

namespace vkt
{
namespace DynamicState
{

using namespace Draw;

namespace
{

class ViewportStateBaseCase : public DynamicStateBaseClass
{
public:
    ViewportStateBaseCase(Context &context, vk::PipelineConstructionType pipelineConstructionType,
                          const char *vertexShaderName, const char *fragmentShaderName)
        : DynamicStateBaseClass(context, pipelineConstructionType, vertexShaderName, fragmentShaderName)
    {
    }

    void initialize(void)
    {
        m_topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

        m_data.push_back(PositionColorVertex(tcu::Vec4(-0.5f, 0.5f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(0.5f, 0.5f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(-0.5f, -0.5f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
        m_data.push_back(PositionColorVertex(tcu::Vec4(0.5f, -0.5f, 1.0f, 1.0f), tcu::RGBA::green().toVec()));

        DynamicStateBaseClass::initialize();
    }

    virtual tcu::Texture2D buildReferenceFrame(void)
    {
        DE_ASSERT(false);
        return tcu::Texture2D(tcu::TextureFormat(), 0, 0);
    }

    virtual void setDynamicStates(void)
    {
        DE_ASSERT(false);
    }

    virtual tcu::TestStatus iterate(void)
    {
        tcu::TestLog &log         = m_context.getTestContext().getLog();
        const vk::VkQueue queue   = m_context.getUniversalQueue();
        const vk::VkDevice device = m_context.getDevice();

        beginRenderPass();

        // set states here
        setDynamicStates();

        m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.getPipeline());

        const vk::VkDeviceSize vertexBufferOffset = 0;
        const vk::VkBuffer vertexBuffer           = m_vertexBuffer->object();
        m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

        m_vk.cmdDraw(*m_cmdBuffer, static_cast<uint32_t>(m_data.size()), 1, 0, 0);

        endRenderPass(m_vk, *m_cmdBuffer);
        endCommandBuffer(m_vk, *m_cmdBuffer);

        submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

        // validation
        {
            tcu::Texture2D referenceFrame = buildReferenceFrame();

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

class ViewportParamTestInstance : public ViewportStateBaseCase
{
public:
    ViewportParamTestInstance(Context &context, vk::PipelineConstructionType pipelineConstructionType,
                              ShaderMap shaders)
        : ViewportStateBaseCase(context, pipelineConstructionType, shaders[glu::SHADERTYPE_VERTEX],
                                shaders[glu::SHADERTYPE_FRAGMENT])
    {
        ViewportStateBaseCase::initialize();
    }

    virtual void setDynamicStates(void)
    {
        const vk::VkViewport viewport = {
            0.0f, 0.0f, static_cast<float>(WIDTH) * 2.0f, static_cast<float>(HEIGHT) * 2.0f, 0.0f, 0.0f};
        const vk::VkRect2D scissor = {{0, 0}, {WIDTH, HEIGHT}};

        setDynamicViewportState(1, &viewport, &scissor);
        setDynamicRasterizationState();
        setDynamicBlendState();
        setDynamicDepthStencilState();
    }

    virtual tcu::Texture2D buildReferenceFrame(void)
    {
        tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5f + static_cast<float>(WIDTH)),
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

                if (xCoord >= 0.0f && xCoord <= 1.0f && yCoord >= 0.0f && yCoord <= 1.0f)
                    referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), x, y);
            }
        }

        return referenceFrame;
    }
};

class ScissorParamTestInstance : public ViewportStateBaseCase
{
public:
    ScissorParamTestInstance(Context &context, vk::PipelineConstructionType pipelineConstructionType, ShaderMap shaders)
        : ViewportStateBaseCase(context, pipelineConstructionType, shaders[glu::SHADERTYPE_VERTEX],
                                shaders[glu::SHADERTYPE_FRAGMENT])
    {
        ViewportStateBaseCase::initialize();
    }

    virtual void setDynamicStates(void)
    {
        const vk::VkViewport viewport = {0.0f, 0.0f, (float)WIDTH, (float)HEIGHT, 0.0f, 0.0f};
        const vk::VkRect2D scissor    = {{0, 0}, {WIDTH / 2, HEIGHT / 2}};

        setDynamicViewportState(1, &viewport, &scissor);
        setDynamicRasterizationState();
        setDynamicBlendState();
        setDynamicDepthStencilState();
    }

    virtual tcu::Texture2D buildReferenceFrame(void)
    {
        tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5f + static_cast<float>(WIDTH)),
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

                if (xCoord >= -0.5f && xCoord <= 0.0f && yCoord >= -0.5f && yCoord <= 0.0f)
                    referenceFrame.getLevel(0).setPixel(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f), x, y);
            }
        }

        return referenceFrame;
    }
};

class ViewportArrayTestInstance : public DynamicStateBaseClass
{
protected:
    std::string m_geometryShaderName;

public:
    ViewportArrayTestInstance(Context &context, vk::PipelineConstructionType pipelineConstructionType,
                              ShaderMap shaders)
        : DynamicStateBaseClass(context, pipelineConstructionType, shaders[glu::SHADERTYPE_VERTEX],
                                shaders[glu::SHADERTYPE_FRAGMENT])
        , m_geometryShaderName(shaders[glu::SHADERTYPE_GEOMETRY])
    {
        for (int i = 0; i < 4; i++)
        {
            m_data.push_back(
                PositionColorVertex(tcu::Vec4(-1.0f, 1.0f, (float)i / 3.0f, 1.0f), tcu::RGBA::green().toVec()));
            m_data.push_back(
                PositionColorVertex(tcu::Vec4(1.0f, 1.0f, (float)i / 3.0f, 1.0f), tcu::RGBA::green().toVec()));
            m_data.push_back(
                PositionColorVertex(tcu::Vec4(-1.0f, -1.0f, (float)i / 3.0f, 1.0f), tcu::RGBA::green().toVec()));
            m_data.push_back(
                PositionColorVertex(tcu::Vec4(1.0f, -1.0f, (float)i / 3.0f, 1.0f), tcu::RGBA::green().toVec()));
        }

        DynamicStateBaseClass::initialize();
    }

    virtual void initPipeline(const vk::VkDevice device)
    {
        const vk::Unique<vk::VkShaderModule> vs(
            createShaderModule(m_vk, device, m_context.getBinaryCollection().get(m_vertexShaderName), 0));
        const vk::Unique<vk::VkShaderModule> gs(
            createShaderModule(m_vk, device, m_context.getBinaryCollection().get(m_geometryShaderName), 0));
        const vk::Unique<vk::VkShaderModule> fs(
            createShaderModule(m_vk, device, m_context.getBinaryCollection().get(m_fragmentShaderName), 0));
        std::vector<vk::VkViewport> viewports(4u, {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f});
        std::vector<vk::VkRect2D> scissors(4u, {{0u, 0u}, {0u, 0u}});

        const PipelineCreateInfo::ColorBlendState::Attachment attachmentState;
        const PipelineCreateInfo::ColorBlendState colorBlendState(
            1u, static_cast<const vk::VkPipelineColorBlendAttachmentState *>(&attachmentState));
        const PipelineCreateInfo::RasterizerState rasterizerState;
        const PipelineCreateInfo::DepthStencilState depthStencilState;
        PipelineCreateInfo::DynamicState dynamicState;

        m_pipeline.setDefaultTopology(m_topology)
            .setDynamicState(static_cast<const vk::VkPipelineDynamicStateCreateInfo *>(&dynamicState))
            .setDefaultMultisampleState()
            .setupVertexInputStete(&m_vertexInputState)
            .setupPreRasterizationShaderState(
                viewports, scissors, *m_pipelineLayout, *m_renderPass, 0u, *vs,
                static_cast<const vk::VkPipelineRasterizationStateCreateInfo *>(&rasterizerState), DE_NULL, DE_NULL,
                *gs)
            .setupFragmentShaderState(
                *m_pipelineLayout, *m_renderPass, 0u, *fs,
                static_cast<const vk::VkPipelineDepthStencilStateCreateInfo *>(&depthStencilState))
            .setupFragmentOutputState(*m_renderPass, 0u,
                                      static_cast<const vk::VkPipelineColorBlendStateCreateInfo *>(&colorBlendState))
            .setMonolithicPipelineLayout(*m_pipelineLayout)
            .buildPipeline();
    }

    virtual tcu::TestStatus iterate(void)
    {
        tcu::TestLog &log         = m_context.getTestContext().getLog();
        const vk::VkQueue queue   = m_context.getUniversalQueue();
        const vk::VkDevice device = m_context.getDevice();

        beginRenderPass();

        // set states here
        const float halfWidth       = (float)WIDTH / 2;
        const float halfHeight      = (float)HEIGHT / 2;
        const int32_t quarterWidth  = WIDTH / 4;
        const int32_t quarterHeight = HEIGHT / 4;

        const vk::VkViewport viewports[4] = {{0.0f, 0.0f, (float)halfWidth, (float)halfHeight, 0.0f, 0.0f},
                                             {halfWidth, 0.0f, (float)halfWidth, (float)halfHeight, 0.0f, 0.0f},
                                             {halfWidth, halfHeight, (float)halfWidth, (float)halfHeight, 0.0f, 0.0f},
                                             {0.0f, halfHeight, (float)halfWidth, (float)halfHeight, 0.0f, 0.0f}};

        const vk::VkRect2D scissors[4] = {
            {{quarterWidth, quarterHeight}, {quarterWidth, quarterHeight}},
            {{(int32_t)halfWidth, quarterHeight}, {quarterWidth, quarterHeight}},
            {{(int32_t)halfWidth, (int32_t)halfHeight}, {quarterWidth, quarterHeight}},
            {{quarterWidth, (int32_t)halfHeight}, {quarterWidth, quarterHeight}},
        };

        setDynamicViewportState(4, viewports, scissors);
        setDynamicRasterizationState();
        setDynamicBlendState();
        setDynamicDepthStencilState();

        m_vk.cmdBindPipeline(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.getPipeline());

        const vk::VkDeviceSize vertexBufferOffset = 0;
        const vk::VkBuffer vertexBuffer           = m_vertexBuffer->object();
        m_vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

        m_vk.cmdDraw(*m_cmdBuffer, static_cast<uint32_t>(m_data.size()), 1, 0, 0);

        endRenderPass(m_vk, *m_cmdBuffer);
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

                    if (xCoord >= -0.5f && xCoord <= 0.5f && yCoord >= -0.5f && yCoord <= 0.5f)
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

void checkGeometryAndMultiViewportSupport(Context &context)
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_MULTI_VIEWPORT);
}

} // namespace

DynamicStateVPTests::DynamicStateVPTests(tcu::TestContext &testCtx,
                                         vk::PipelineConstructionType pipelineConstructionType)
    : TestCaseGroup(testCtx, "vp_state", "Tests for viewport state")
    , m_pipelineConstructionType(pipelineConstructionType)
{
    /* Left blank on purpose */
}

DynamicStateVPTests::~DynamicStateVPTests()
{
}

void DynamicStateVPTests::init(void)
{
    ShaderMap shaderPaths;
    shaderPaths[glu::SHADERTYPE_VERTEX]   = "vulkan/dynamic_state/VertexFetch.vert";
    shaderPaths[glu::SHADERTYPE_FRAGMENT] = "vulkan/dynamic_state/VertexFetch.frag";

    addChild(new InstanceFactory<ViewportParamTestInstance>(m_testCtx, "viewport",
                                                            "Set viewport which is twice bigger than screen size",
                                                            m_pipelineConstructionType, shaderPaths));
    addChild(new InstanceFactory<ScissorParamTestInstance>(
        m_testCtx, "scissor", "Perform a scissor test on 1/4 bottom-left part of the surface",
        m_pipelineConstructionType, shaderPaths));

    shaderPaths[glu::SHADERTYPE_GEOMETRY] = "vulkan/dynamic_state/ViewportArray.geom";
    addChild(new InstanceFactory<ViewportArrayTestInstance, FunctionSupport0>(
        m_testCtx, "viewport_array", "Multiple viewports and scissors", m_pipelineConstructionType, shaderPaths,
        checkGeometryAndMultiViewportSupport));
}

} // namespace DynamicState
} // namespace vkt
