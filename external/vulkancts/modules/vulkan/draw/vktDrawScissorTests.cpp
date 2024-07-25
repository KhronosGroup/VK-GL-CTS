/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Google Inc.
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief Scissoring tests
 *//*--------------------------------------------------------------------*/

#include "vktDrawScissorTests.hpp"

#include "vktDrawBaseClass.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "tcuTestCase.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include <string>

namespace vkt
{
namespace Draw
{
namespace
{
using namespace vk;
using namespace std;
using namespace tcu;

enum
{
    WIDTH  = 256,
    HEIGHT = 256
};

struct ColorQuad
{
    ColorQuad(uint32_t x, uint32_t y, uint32_t width, uint32_t height, Vec4 color)
        : m_x(x)
        , m_y(y)
        , m_width(width)
        , m_height(height)
        , m_color(color)
    {
    }

    uint32_t m_x;
    uint32_t m_y;
    uint32_t m_width;
    uint32_t m_height;
    Vec4 m_color;
};

ColorQuad scissorQuad(ColorQuad quad, VkRect2D scissor, VkExtent2D framebufferSize)
{
    int left   = quad.m_x;
    int right  = quad.m_x + quad.m_width;
    int top    = quad.m_y;
    int bottom = quad.m_y + quad.m_height;

    left   = de::max(left, scissor.offset.x);
    left   = de::max(left, 0);
    right  = de::min(right, scissor.offset.x + (int)scissor.extent.width);
    right  = de::min(right, (int)framebufferSize.width);
    top    = de::max(top, scissor.offset.y);
    top    = de::max(top, 0);
    bottom = de::min(bottom, scissor.offset.y + (int)scissor.extent.height);
    bottom = de::min(bottom, (int)framebufferSize.height);

    return ColorQuad(left, top, de::max(right - left, 0), de::max(bottom - top, 0), quad.m_color);
}

class TestCommand
{
public:
    TestCommand(void)
    {
    }
    virtual ~TestCommand(void)
    {
    }

    virtual vector<PositionColorVertex> getVertices(uint32_t offset)
    {
        DE_UNREF(offset);
        return vector<PositionColorVertex>();
    }
    virtual void addCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer) = 0;
    virtual uint32_t getMaxScissor(void)
    {
        return 0;
    }
    virtual vector<ColorQuad> getQuad(void)
    {
        return vector<ColorQuad>();
    }
    virtual vector<VkRect2D> updateScissors(vector<VkRect2D> scissors)
    {
        return scissors;
    }
    virtual bool isScissored(void)
    {
        return false;
    }

private:
};

typedef de::SharedPtr<TestCommand> TestCommandSp;

class QuadDrawTestCommand : public TestCommand
{
public:
    QuadDrawTestCommand(uint32_t x, uint32_t y, uint32_t width, uint32_t height, Vec4 color);
    virtual ~QuadDrawTestCommand(void)
    {
    }

    virtual vector<PositionColorVertex> getVertices(uint32_t offset);
    virtual void addCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);
    virtual vector<ColorQuad> getQuad(void)
    {
        return vector<ColorQuad>(1, m_quad);
    }
    virtual bool isScissored(void)
    {
        return true;
    }

private:
    uint32_t m_offset;
    ColorQuad m_quad;
};

QuadDrawTestCommand::QuadDrawTestCommand(uint32_t x, uint32_t y, uint32_t width, uint32_t height, Vec4 color)
    : m_offset(0)
    , m_quad(x, y, width, height, color)
{
}

vector<PositionColorVertex> QuadDrawTestCommand::getVertices(uint32_t offset)
{
    vector<PositionColorVertex> vertices;
    float scaleWidth  = 2.0f / (float)WIDTH;
    float scaleHeight = 2.0f / (float)HEIGHT;
    Vec4 topLeft(-1.0f + scaleWidth * (float)m_quad.m_x, -1.0f + scaleHeight * (float)m_quad.m_y, 0.0f, 1.0f);
    Vec4 topRight(-1.0f + scaleWidth * (float)(m_quad.m_x + m_quad.m_width), -1.0f + scaleHeight * (float)m_quad.m_y,
                  0.0f, 1.0f);
    Vec4 bottomLeft(-1.0f + scaleWidth * (float)m_quad.m_x, -1.0f + scaleHeight * (float)(m_quad.m_y + m_quad.m_height),
                    0.0f, 1.0f);
    Vec4 bottomRight(-1.0f + scaleWidth * (float)(m_quad.m_x + m_quad.m_width),
                     -1.0f + scaleHeight * (float)(m_quad.m_y + m_quad.m_height), 0.0f, 1.0f);

    m_offset = offset;

    vertices.push_back(PositionColorVertex(topLeft, m_quad.m_color));
    vertices.push_back(PositionColorVertex(bottomRight, m_quad.m_color));
    vertices.push_back(PositionColorVertex(bottomLeft, m_quad.m_color));
    vertices.push_back(PositionColorVertex(topLeft, m_quad.m_color));
    vertices.push_back(PositionColorVertex(topRight, m_quad.m_color));
    vertices.push_back(PositionColorVertex(bottomRight, m_quad.m_color));

    return vertices;
}

void QuadDrawTestCommand::addCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    vk.cmdDraw(cmdBuffer, 6u, 1u, m_offset, 0u);
}

class RectClearTestCommand : public TestCommand
{
public:
    RectClearTestCommand(uint32_t x, uint32_t y, uint32_t width, uint32_t height, Vec4 color);
    virtual ~RectClearTestCommand(void)
    {
    }

    virtual void addCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);
    virtual vector<ColorQuad> getQuad(void)
    {
        return vector<ColorQuad>(1, m_quad);
    }

private:
    ColorQuad m_quad;
};

RectClearTestCommand::RectClearTestCommand(uint32_t x, uint32_t y, uint32_t width, uint32_t height, Vec4 color)
    : m_quad(x, y, width, height, color)
{
}

void RectClearTestCommand::addCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    const VkClearAttachment attachment = {
        VK_IMAGE_ASPECT_COLOR_BIT,          // VkImageAspectFlags    aspectMask
        0u,                                 // uint32_t                colorAttachment
        makeClearValueColor(m_quad.m_color) // VkClearValue            clearValue
    };

    const VkClearRect rect = {
        makeRect2D(m_quad.m_x, m_quad.m_y, m_quad.m_width, m_quad.m_height), // VkRect2D    rect
        0u,                                                                  // uint32_t    baseArrayLayer
        1u                                                                   // uint32_t    layerCount
    };

    vk.cmdClearAttachments(cmdBuffer, 1u, &attachment, 1u, &rect);
}

class DynamicScissorTestCommand : public TestCommand
{
public:
    DynamicScissorTestCommand(uint32_t firstScissor, vector<VkRect2D> scissors);
    virtual ~DynamicScissorTestCommand(void)
    {
    }

    virtual void addCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);
    virtual uint32_t getMaxScissor(void)
    {
        return m_firstScissor + (uint32_t)m_scissors.size();
    }
    virtual vector<VkRect2D> updateScissors(vector<VkRect2D> scissors);

private:
    uint32_t m_firstScissor;
    vector<VkRect2D> m_scissors;
};

DynamicScissorTestCommand::DynamicScissorTestCommand(uint32_t firstScissor, vector<VkRect2D> scissors)
    : m_firstScissor(firstScissor)
    , m_scissors(scissors)
{
}

void DynamicScissorTestCommand::addCommands(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    vk.cmdSetScissor(cmdBuffer, m_firstScissor, (uint32_t)m_scissors.size(), m_scissors.data());
}

vector<VkRect2D> DynamicScissorTestCommand::updateScissors(vector<VkRect2D> scissors)
{
    for (size_t scissorIdx = 0; scissorIdx < m_scissors.size(); scissorIdx++)
    {
        while (scissors.size() <= m_firstScissor + scissorIdx)
            scissors.push_back(makeRect2D(0, 0)); // Add empty scissor

        scissors[m_firstScissor + scissorIdx] = m_scissors[scissorIdx];
    }

    return scissors;
}

struct TestParams
{
    TestParams(const SharedGroupParams gp) : groupParams(gp), framebufferSize({WIDTH, HEIGHT})
    {
    }

    bool dynamicScissor;
    vector<VkRect2D> staticScissors;
    vector<TestCommandSp> commands;
    bool usesMultipleScissors;
    const SharedGroupParams groupParams;
    VkExtent2D framebufferSize;
};

uint32_t countScissors(TestParams params)
{
    if (params.dynamicScissor)
    {
        uint32_t numScissors = 0u;

        for (size_t commandIdx = 0; commandIdx < params.commands.size(); commandIdx++)
            numScissors = de::max(numScissors, params.commands[commandIdx]->getMaxScissor());

        return numScissors;
    }
    else
        return (uint32_t)params.staticScissors.size();
}

class ScissorTestInstance : public TestInstance
{
public:
    ScissorTestInstance(Context &context, const TestParams &params);
    ~ScissorTestInstance(void);
    TestStatus iterate(void);

protected:
    void drawCommands(VkCommandBuffer cmdBuffer, VkPipeline pipeline, VkBuffer vertexBuffer) const;
    void postRenderCommands(VkCommandBuffer cmdBuffer, VkImage colorTargetImage) const;

#ifndef CTS_USES_VULKANSC
    void beginSecondaryCmdBuffer(VkCommandBuffer cmdBuffer, VkFormat colorAttachmentFormat,
                                 VkRenderingFlagsKHR renderingFlags = 0u) const;
#endif // CTS_USES_VULKANSC

private:
    TestParams m_params;
};

ScissorTestInstance::ScissorTestInstance(Context &context, const TestParams &params)
    : vkt::TestInstance(context)
    , m_params(params)
{
}

ScissorTestInstance::~ScissorTestInstance(void)
{
}

class ScissorTestCase : public TestCase
{
public:
    ScissorTestCase(TestContext &context, const char *name, const TestParams params);
    ~ScissorTestCase(void);
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    TestParams m_params;
};

ScissorTestCase::ScissorTestCase(TestContext &context, const char *name, const TestParams params)
    : vkt::TestCase(context, name)
    , m_params(params)
{
    m_params.usesMultipleScissors = params.staticScissors.size() > 1;

    for (size_t commandIdx = 0; commandIdx < m_params.commands.size(); commandIdx++)
        if (m_params.commands[commandIdx]->getMaxScissor() > 1)
            m_params.usesMultipleScissors = true;
}

ScissorTestCase::~ScissorTestCase(void)
{
}

void ScissorTestCase::checkSupport(Context &context) const
{
    if (m_params.groupParams->useDynamicRendering)
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

    if (m_params.usesMultipleScissors)
    {
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_MULTI_VIEWPORT);
    }
}

void ScissorTestCase::initPrograms(SourceCollections &programCollection) const
{
    programCollection.glslSources.add("vert") << glu::VertexSource("#version 430\n"
                                                                   "layout(location = 0) in vec4 in_position;\n"
                                                                   "layout(location = 1) in vec4 in_color;\n"
                                                                   "layout(location = 0) out vec4 out_color;\n"
                                                                   "void main()\n"
                                                                   "{\n"
                                                                   "    gl_Position  = in_position;\n"
                                                                   "    out_color    = in_color;\n"
                                                                   "}\n");

    // Geometry shader draws the same triangles to all viewports
    string geomSource = string("#version 430\n"
                               "layout(invocations = ") +
                        de::toString(countScissors(m_params)) +
                        ") in;\n"
                        "layout(triangles) in;\n"
                        "layout(triangle_strip, max_vertices = 3) out;\n"
                        "layout(location = 0) in vec4 in_color[];\n"
                        "layout(location = 0) out vec4 out_color;\n"
                        "void main()\n"
                        "{\n"
                        "    for (int i = 0; i < gl_in.length(); i++)\n"
                        "    {\n"
                        "        gl_ViewportIndex = gl_InvocationID;\n"
                        "        gl_Position      = gl_in[i].gl_Position;\n"
                        "        out_color        = in_color[i];\n"
                        "        EmitVertex();\n"
                        "    }\n"
                        "    EndPrimitive();\n"
                        "}\n";

    programCollection.glslSources.add("geom") << glu::GeometrySource(geomSource);

    programCollection.glslSources.add("frag") << glu::FragmentSource("#version 430\n"
                                                                     "layout(location = 0) in vec4 in_color;\n"
                                                                     "layout(location = 0) out vec4 out_color;\n"
                                                                     "void main()\n"
                                                                     "{\n"
                                                                     "    out_color = in_color;\n"
                                                                     "}\n");
}

TestInstance *ScissorTestCase::createInstance(Context &context) const
{
    return new ScissorTestInstance(context, m_params);
}

TestStatus ScissorTestInstance::iterate(void)
{
    ConstPixelBufferAccess frame;
    VkFormat colorImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    de::SharedPtr<Image> colorTargetImage;
    TestLog &log              = m_context.getTestContext().getLog();
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    const CmdPoolCreateInfo cmdPoolCreateInfo(m_context.getUniversalQueueFamilyIndex());
    Move<VkCommandPool> cmdPool     = createCommandPool(vk, device, &cmdPoolCreateInfo);
    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    Move<VkCommandBuffer> secCmdBuffer;
    const Unique<VkShaderModule> vs(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0));
    Move<VkShaderModule> gs;
    const Unique<VkShaderModule> fs(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0));
    const uint32_t numScissors    = countScissors(m_params);
    VkDeviceSize vertexBufferSize = 0;
    de::SharedPtr<Buffer> vertexBuffer;
    Move<VkRenderPass> renderPass;
    Move<VkImageView> colorTargetView;
    Move<VkFramebuffer> framebuffer;
    Move<VkPipeline> pipeline;
    TextureLevel refImage;
    VkExtent2D framebufferSize = m_params.framebufferSize;

    if (m_params.usesMultipleScissors)
        gs = createShaderModule(vk, device, m_context.getBinaryCollection().get("geom"), 0);

    // Create color buffer image
    {
        const VkExtent3D targetImageExtent = {WIDTH, HEIGHT, 1};
        const ImageCreateInfo targetImageCreateInfo(
            VK_IMAGE_TYPE_2D, colorImageFormat, targetImageExtent, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        colorTargetImage = Image::createAndAlloc(vk, device, targetImageCreateInfo, m_context.getDefaultAllocator(),
                                                 m_context.getUniversalQueueFamilyIndex());
    }

    const ImageViewCreateInfo colorTargetViewInfo(colorTargetImage->object(), VK_IMAGE_VIEW_TYPE_2D, colorImageFormat);
    colorTargetView = createImageView(vk, device, &colorTargetViewInfo);

    // Create render pass
    if (!m_params.groupParams->useDynamicRendering)
    {
        RenderPassCreateInfo renderPassCreateInfo;
        renderPassCreateInfo.addAttachment(AttachmentDescription(
            colorImageFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

        const VkAttachmentReference colorAttachmentRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        renderPassCreateInfo.addSubpass(SubpassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 0, DE_NULL, 1,
                                                           &colorAttachmentRef, DE_NULL, AttachmentReference(), 0,
                                                           DE_NULL));

        renderPass = createRenderPass(vk, device, &renderPassCreateInfo);

        // Create framebuffer
        vector<VkImageView> colorAttachment{*colorTargetView};
        const FramebufferCreateInfo framebufferCreateInfo(*renderPass, colorAttachment, framebufferSize.width,
                                                          framebufferSize.height, 1);
        framebuffer = createFramebuffer(vk, device, &framebufferCreateInfo);
    }

    // Create vertex buffer
    {
        vector<PositionColorVertex> vertices;

        for (size_t commandIdx = 0; commandIdx < m_params.commands.size(); commandIdx++)
        {
            vector<PositionColorVertex> commandVertices =
                m_params.commands[commandIdx]->getVertices((uint32_t)vertices.size());
            vertices.insert(vertices.end(), commandVertices.begin(), commandVertices.end());
        }

        vertexBufferSize = vertices.size() * sizeof(PositionColorVertex);

        if (vertexBufferSize > 0)
        {
            vertexBuffer = Buffer::createAndAlloc(vk, device,
                                                  BufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
                                                  m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);
            uint8_t *ptr = reinterpret_cast<uint8_t *>(vertexBuffer->getBoundMemory().getHostPtr());

            deMemcpy(ptr, vertices.data(), static_cast<size_t>(vertexBufferSize));
            flushMappedMemoryRange(vk, device, vertexBuffer->getBoundMemory().getMemory(),
                                   vertexBuffer->getBoundMemory().getOffset(), VK_WHOLE_SIZE);
        }
    }

    const PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    Move<VkPipelineLayout> pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

    // Create pipeline
    {
        const PipelineCreateInfo::ColorBlendState::Attachment colorBlendState;

        const VkVertexInputBindingDescription vertexInputBindingDescription = {
            0,                          // deUintre                binding
            (uint32_t)sizeof(Vec4) * 2, // uint32_t                stride
            VK_VERTEX_INPUT_RATE_VERTEX // VkVertexInputRate    inputRate
        };
        const VkViewport viewport = makeViewport(WIDTH, HEIGHT);

        const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] = {
            {0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u},
            {1u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, (uint32_t)(sizeof(float) * 4)}};

        PipelineCreateInfo::VertexInputState vertexInputState = PipelineCreateInfo::VertexInputState(
            1, &vertexInputBindingDescription, 2, vertexInputAttributeDescriptions);

        PipelineCreateInfo pipelineCreateInfo(*pipelineLayout, *renderPass, 0, 0);
        pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", VK_SHADER_STAGE_VERTEX_BIT));
        if (m_params.usesMultipleScissors)
            pipelineCreateInfo.addShader(
                PipelineCreateInfo::PipelineShaderStage(*gs, "main", VK_SHADER_STAGE_GEOMETRY_BIT));
        pipelineCreateInfo.addShader(
            PipelineCreateInfo::PipelineShaderStage(*fs, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
        pipelineCreateInfo.addState(PipelineCreateInfo::VertexInputState(vertexInputState));
        pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));
        pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &colorBlendState));
        pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
        pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
        pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());

        if (m_params.dynamicScissor)
        {
            pipelineCreateInfo.addState(
                PipelineCreateInfo::DynamicState(vector<VkDynamicState>(1, VK_DYNAMIC_STATE_SCISSOR)));
            pipelineCreateInfo.addState(
                PipelineCreateInfo::ViewportState(numScissors, vector<VkViewport>(numScissors, viewport),
                                                  vector<VkRect2D>(numScissors, makeRect2D(0, 0))));
        }
        else
        {
            pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(
                numScissors, vector<VkViewport>(numScissors, viewport), m_params.staticScissors));
        }

#ifndef CTS_USES_VULKANSC
        VkPipelineRenderingCreateInfoKHR renderingCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
                                                             DE_NULL,
                                                             0u,
                                                             1u,
                                                             &colorImageFormat,
                                                             VK_FORMAT_UNDEFINED,
                                                             VK_FORMAT_UNDEFINED};

        if (m_params.groupParams->useDynamicRendering)
            pipelineCreateInfo.pNext = &renderingCreateInfo;
#endif // CTS_USES_VULKANSC

        pipeline = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);
    }

    // Queue commands and read results.
    {
        const ImageSubresourceRange subresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        const VkOffset3D zeroOffset   = {0, 0, 0};
        const tcu::Vec4 clearColor    = {0.0f, 0.0f, 0.0f, 1.0f};
        const VkClearValue clearValue = makeClearValueColor(clearColor);
        const VkBuffer vBuffer        = (vertexBufferSize > 0) ? vertexBuffer->object() : VK_NULL_HANDLE;
        const VkRect2D renderArea     = makeRect2D(m_params.framebufferSize);

        // Unreference value for Vulkan SC
        DE_UNREF(clearValue);

        clearColorImage(vk, device, m_context.getUniversalQueue(), m_context.getUniversalQueueFamilyIndex(),
                        colorTargetImage->object(), clearColor, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 1u);

#ifndef CTS_USES_VULKANSC
        if (m_params.groupParams->useSecondaryCmdBuffer)
        {
            secCmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

            // record secondary command buffer
            if (m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
            {
                beginSecondaryCmdBuffer(*secCmdBuffer, colorImageFormat,
                                        VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
                beginRendering(vk, *secCmdBuffer, *colorTargetView, renderArea, clearValue,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR);
            }
            else
                beginSecondaryCmdBuffer(*secCmdBuffer, colorImageFormat);

            drawCommands(*secCmdBuffer, *pipeline, vBuffer);

            if (m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
                endRendering(vk, *secCmdBuffer);

            endCommandBuffer(vk, *secCmdBuffer);

            // record primary command buffer
            beginCommandBuffer(vk, *cmdBuffer, 0u);

            if (!m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
                beginRendering(vk, *cmdBuffer, *colorTargetView, renderArea, clearValue,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR,
                               VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);

            vk.cmdExecuteCommands(*cmdBuffer, 1u, &*secCmdBuffer);

            if (!m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
                endRendering(vk, *cmdBuffer);

            postRenderCommands(*cmdBuffer, colorTargetImage->object());
            endCommandBuffer(vk, *cmdBuffer);
        }
        else if (m_params.groupParams->useDynamicRendering)
        {
            beginCommandBuffer(vk, *cmdBuffer);

            beginRendering(vk, *cmdBuffer, *colorTargetView, renderArea, clearValue,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR);
            drawCommands(*cmdBuffer, *pipeline, vBuffer);
            endRendering(vk, *cmdBuffer);
            postRenderCommands(*cmdBuffer, colorTargetImage->object());

            endCommandBuffer(vk, *cmdBuffer);
        }
#endif // CTS_USES_VULKANSC

        if (!m_params.groupParams->useDynamicRendering)
        {
            beginCommandBuffer(vk, *cmdBuffer);

            beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, clearColor);
            drawCommands(*cmdBuffer, *pipeline, vBuffer);
            endRenderPass(vk, *cmdBuffer);
            postRenderCommands(*cmdBuffer, colorTargetImage->object());

            endCommandBuffer(vk, *cmdBuffer);
        }

        submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), cmdBuffer.get());

        frame = colorTargetImage->readSurface(m_context.getUniversalQueue(), m_context.getDefaultAllocator(),
                                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, zeroOffset, WIDTH, HEIGHT,
                                              VK_IMAGE_ASPECT_COLOR_BIT);
    }

    // Generate reference
    {
        refImage.setStorage(vk::mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM), WIDTH, HEIGHT);
        clear(refImage.getAccess(), Vec4(0.0f, 0.0f, 0.0f, 1.0f));

        vector<VkRect2D> scissors = m_params.staticScissors;

        for (size_t commandIdx = 0; commandIdx < m_params.commands.size(); commandIdx++)
        {
            scissors = m_params.commands[commandIdx]->updateScissors(scissors);

            vector<ColorQuad> quad = m_params.commands[commandIdx]->getQuad();

            if (quad.empty())
                continue;

            for (size_t scissorIdx = 0; scissorIdx < scissors.size(); scissorIdx++)
            {
                ColorQuad scissoredQuad = m_params.commands[commandIdx]->isScissored() ?
                                              scissorQuad(quad[0], scissors[scissorIdx], framebufferSize) :
                                              quad[0];

                if (scissoredQuad.m_width == 0 || scissoredQuad.m_height == 0)
                    continue;

                clear(getSubregion(refImage.getAccess(), scissoredQuad.m_x, scissoredQuad.m_y, 0, scissoredQuad.m_width,
                                   scissoredQuad.m_height, 1),
                      scissoredQuad.m_color);
            }
        }
    }

    // Compare results
    qpTestResult res = QP_TEST_RESULT_PASS;

    if (!intThresholdCompare(log, "Result", "Image comparison result", refImage.getAccess(), frame, UVec4(0),
                             COMPARE_LOG_RESULT))
        res = QP_TEST_RESULT_FAIL;

    return TestStatus(res, qpGetTestResultName(res));
}

void ScissorTestInstance::drawCommands(VkCommandBuffer cmdBuffer, VkPipeline pipeline, VkBuffer vertexBuffer) const
{
    const DeviceInterface &vk             = m_context.getDeviceInterface();
    const VkDeviceSize vertexBufferOffset = 0;

    if (vertexBuffer != VK_NULL_HANDLE)
        vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    for (size_t commandIdx = 0; commandIdx < m_params.commands.size(); commandIdx++)
        m_params.commands[commandIdx]->addCommands(vk, cmdBuffer);
}

void ScissorTestInstance::postRenderCommands(VkCommandBuffer cmdBuffer, VkImage colorTargetImage) const
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    transition2DImage(vk, cmdBuffer, colorTargetImage, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
}

#ifndef CTS_USES_VULKANSC
void ScissorTestInstance::beginSecondaryCmdBuffer(VkCommandBuffer cmdBuffer, VkFormat colorAttachmentFormat,
                                                  VkRenderingFlagsKHR renderingFlags) const
{
    VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR, // VkStructureType sType;
        DE_NULL,                                                         // const void* pNext;
        renderingFlags,                                                  // VkRenderingFlagsKHR flags;
        0u,                                                              // uint32_t viewMask;
        1u,                                                              // uint32_t colorAttachmentCount;
        &colorAttachmentFormat,                                          // const VkFormat* pColorAttachmentFormats;
        VK_FORMAT_UNDEFINED,                                             // VkFormat depthAttachmentFormat;
        VK_FORMAT_UNDEFINED,                                             // VkFormat stencilAttachmentFormat;
        VK_SAMPLE_COUNT_1_BIT,                                           // VkSampleCountFlagBits rasterizationSamples;
    };
    const VkCommandBufferInheritanceInfo bufferInheritanceInfo = initVulkanStructure(&inheritanceRenderingInfo);

    VkCommandBufferUsageFlags usageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (!m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
        usageFlags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

    const VkCommandBufferBeginInfo commandBufBeginParams{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
        DE_NULL,                                     // const void* pNext;
        usageFlags,                                  // VkCommandBufferUsageFlags flags;
        &bufferInheritanceInfo};

    const DeviceInterface &vk = m_context.getDeviceInterface();
    VK_CHECK(vk.beginCommandBuffer(cmdBuffer, &commandBufBeginParams));
}
#endif // CTS_USES_VULKANSC

void createTests(TestCaseGroup *testGroup, const SharedGroupParams groupParams)
{
    TestContext &testCtx = testGroup->getTestContext();
    const Vec4 red(1.0f, 0.0f, 0.0f, 1.0f);
    const Vec4 green(0.0f, 1.0f, 0.0f, 1.0f);
    const Vec4 blue(0.0f, 0.0f, 1.0f, 1.0f);
    const Vec4 yellow(1.0f, 1.0f, 0.0f, 1.0f);

    // Two quads with a single static scissor
    {
        TestParams params(groupParams);
        params.dynamicScissor = false;
        params.staticScissors.push_back(makeRect2D(30, 40, WIDTH - 60, HEIGHT - 80));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(10, 10, 50, 50, red)));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(WIDTH - 80, HEIGHT - 100, 30, 40, green)));

        testGroup->addChild(new ScissorTestCase(testCtx, "static_scissor_two_quads", params));
    }

    // Two clears with a single static scissor
    {
        TestParams params(groupParams);
        params.dynamicScissor = false;
        params.staticScissors.push_back(makeRect2D(30, 40, WIDTH - 60, HEIGHT - 80));
        params.commands.push_back(TestCommandSp(new RectClearTestCommand(10, 10, 50, 50, red)));
        params.commands.push_back(TestCommandSp(new RectClearTestCommand(WIDTH - 80, HEIGHT - 100, 30, 40, green)));

        testGroup->addChild(new ScissorTestCase(testCtx, "static_scissor_two_clears", params));
    }

    // One quad with two static scissors
    {
        TestParams params(groupParams);
        params.dynamicScissor = false;
        params.staticScissors.push_back(makeRect2D(30, 40, WIDTH - 60, HEIGHT - 70));
        params.staticScissors.push_back(makeRect2D(40, 50, WIDTH - 60, HEIGHT - 70));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(10, 10, WIDTH - 10, HEIGHT - 10, red)));

        testGroup->addChild(new ScissorTestCase(testCtx, "two_static_scissors_one_quad", params));
    }

    // Static scissor extending outside viewport
    {
        TestParams params(groupParams);
        params.dynamicScissor = false;
        params.staticScissors.push_back(makeRect2D(30, 40, WIDTH, HEIGHT));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(0, 0, WIDTH, HEIGHT + 30, green)));

        testGroup->addChild(new ScissorTestCase(testCtx, "static_scissor_partially_outside_viewport", params));
    }

    // Static scissor completely outside viewport
    {
        TestParams params(groupParams);
        params.dynamicScissor = false;
        params.staticScissors.push_back(makeRect2D(WIDTH + 30, HEIGHT + 40, WIDTH, HEIGHT));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(100, 100, 20, 30, green)));

        testGroup->addChild(new ScissorTestCase(testCtx, "static_scissor_outside_viewport", params));
    }

    // Static scissor outside viewport and touching right border of viewport
    {
        TestParams params(groupParams);
        params.dynamicScissor = false;
        params.staticScissors.push_back(makeRect2D(WIDTH, 0, WIDTH, HEIGHT));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(100, 100, 20, 30, green)));

        testGroup->addChild(new ScissorTestCase(testCtx, "static_scissor_viewport_border", params));
    }

    // Static scissor with offset + extent equal to largest positive int32
    {
        TestParams params(groupParams);
        params.dynamicScissor = false;
        params.staticScissors.push_back(makeRect2D(100, 100, 0x7fffffff - 100, 0x7fffffff - 100));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(0, 0, WIDTH, HEIGHT, green)));

        testGroup->addChild(new ScissorTestCase(testCtx, "static_scissor_max_int32", params));
    }

    // 16 static scissors (minimum number required when multiViewport supported)
    {
        TestParams params(groupParams);
        params.dynamicScissor = false;

        for (uint32_t i = 0; i < 16; i++)
            params.staticScissors.push_back(makeRect2D(10 + i * 3, 20 + i * 2, WIDTH / 2, HEIGHT / 2));

        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(5, 6, WIDTH - 10, HEIGHT - 2, red)));

        testGroup->addChild(new ScissorTestCase(testCtx, "16_static_scissors", params));
    }

    // Two quads with an empty scissor
    {
        TestParams params(groupParams);
        params.dynamicScissor = false;
        params.staticScissors.push_back(makeRect2D(0, 0, 0, 0));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(10, 10, 50, 50, red)));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(WIDTH - 80, HEIGHT - 100, 30, 40, green)));

        testGroup->addChild(new ScissorTestCase(testCtx, "empty_static_scissor", params));
    }

    // Two quads with a single dynamic scissor
    {
        TestParams params(groupParams);
        params.dynamicScissor = true;
        params.commands.push_back(TestCommandSp(
            new DynamicScissorTestCommand(0, vector<VkRect2D>(1, makeRect2D(30, 40, WIDTH - 60, HEIGHT - 80)))));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(10, 10, 50, 50, red)));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(WIDTH - 80, HEIGHT - 100, 30, 40, green)));

        testGroup->addChild(new ScissorTestCase(testCtx, "dynamic_scissor_two_quads", params));
    }

    // Empty scissor for the first draw
    {
        TestParams params(groupParams);
        params.dynamicScissor = true;
        params.commands.push_back(
            TestCommandSp(new DynamicScissorTestCommand(0, vector<VkRect2D>(1, makeRect2D(0, 0, 0, 0)))));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(10, 10, 50, 50, red)));
        params.commands.push_back(TestCommandSp(
            new DynamicScissorTestCommand(0, vector<VkRect2D>(1, makeRect2D(30, 40, WIDTH - 60, HEIGHT - 80)))));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(WIDTH - 80, HEIGHT - 100, 30, 40, green)));

        testGroup->addChild(new ScissorTestCase(testCtx, "empty_dynamic_scissor_first_draw", params));
    }

    // Two quads with three scissors updated in between
    {
        TestParams params(groupParams);
        VkRect2D rect = makeRect2D(10, 20, WIDTH - 60, HEIGHT - 70);
        vector<VkRect2D> scissors;

        params.dynamicScissor = true;

        scissors.push_back(rect);
        rect.offset.x += 10;
        rect.offset.y += 10;
        scissors.push_back(rect);
        rect.offset.x += 10;
        rect.offset.y += 10;
        scissors.push_back(rect);

        params.commands.push_back(TestCommandSp(new DynamicScissorTestCommand(0, scissors)));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(5, 7, WIDTH - 20, HEIGHT - 9, red)));

        for (size_t scissorIdx = 0; scissorIdx < scissors.size(); scissorIdx++)
            scissors[scissorIdx].offset.x += 20;

        params.commands.push_back(TestCommandSp(new DynamicScissorTestCommand(0, scissors)));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(8, 12, WIDTH - 2, HEIGHT - 19, green)));

        testGroup->addChild(new ScissorTestCase(testCtx, "dynamic_scissor_updates_between_draws", params));
    }

    // Scissor updates out of order
    {
        TestParams params(groupParams);
        VkRect2D rect = makeRect2D(10, 20, WIDTH - 60, HEIGHT - 70);
        vector<VkRect2D> scissors;

        params.dynamicScissor = true;

        scissors.push_back(rect);
        rect.offset.x += 10;
        rect.offset.y += 10;
        scissors.push_back(rect);
        rect.offset.x += 10;
        rect.offset.y += 10;
        scissors.push_back(rect);

        params.commands.push_back(TestCommandSp(new DynamicScissorTestCommand(2, vector<VkRect2D>(1, scissors[2]))));
        params.commands.push_back(TestCommandSp(new DynamicScissorTestCommand(1, vector<VkRect2D>(1, scissors[1]))));
        params.commands.push_back(TestCommandSp(new DynamicScissorTestCommand(0, vector<VkRect2D>(1, scissors[0]))));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(5, 7, WIDTH - 20, HEIGHT - 9, red)));

        for (size_t scissorIdx = 0; scissorIdx < scissors.size(); scissorIdx++)
            scissors[scissorIdx].offset.x += 20;

        params.commands.push_back(TestCommandSp(new DynamicScissorTestCommand(1, vector<VkRect2D>(1, scissors[1]))));
        params.commands.push_back(TestCommandSp(new DynamicScissorTestCommand(0, vector<VkRect2D>(1, scissors[0]))));
        params.commands.push_back(TestCommandSp(new DynamicScissorTestCommand(2, vector<VkRect2D>(1, scissors[2]))));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(8, 12, WIDTH - 2, HEIGHT - 19, green)));

        testGroup->addChild(new ScissorTestCase(testCtx, "dynamic_scissor_out_of_order_updates", params));
    }

    // Dynamic scissor extending outside viewport
    {
        TestParams params(groupParams);
        params.dynamicScissor = true;
        params.commands.push_back(
            TestCommandSp(new DynamicScissorTestCommand(0, vector<VkRect2D>(1, makeRect2D(30, 40, WIDTH, HEIGHT)))));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(0, 0, WIDTH + 50, HEIGHT + 20, green)));

        testGroup->addChild(new ScissorTestCase(testCtx, "dynamic_scissor_partially_outside_viewport", params));
    }

    // Dynamic scissor completely outside viewport
    {
        TestParams params(groupParams);
        params.dynamicScissor = true;
        params.commands.push_back(TestCommandSp(
            new DynamicScissorTestCommand(0, vector<VkRect2D>(1, makeRect2D(WIDTH + 30, HEIGHT + 40, WIDTH, HEIGHT)))));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(100, 100, 20, 30, green)));

        testGroup->addChild(new ScissorTestCase(testCtx, "dynamic_scissor_outside_viewport", params));
    }

    // Dynamic scissor outside viewport and touching right border of viewport
    {
        TestParams params(groupParams);
        params.dynamicScissor = true;
        params.commands.push_back(
            TestCommandSp(new DynamicScissorTestCommand(0, vector<VkRect2D>(1, makeRect2D(WIDTH, 0, WIDTH, HEIGHT)))));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(100, 100, 20, 30, green)));

        testGroup->addChild(new ScissorTestCase(testCtx, "dynamic_scissor_viewport_border", params));
    }

    // Dynamic scissor with offset + extent equal to largest positive int32
    {
        TestParams params(groupParams);
        params.dynamicScissor = true;
        params.commands.push_back(TestCommandSp(new DynamicScissorTestCommand(
            0, vector<VkRect2D>(1, makeRect2D(100, 100, 0x7fffffff - 100, 0x7fffffff - 100)))));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(0, 0, WIDTH, HEIGHT, green)));

        testGroup->addChild(new ScissorTestCase(testCtx, "dynamic_scissor_max_int32", params));
    }

    // 16 dynamic scissors (minimum number required when multiViewport supported)
    {
        TestParams params(groupParams);
        vector<VkRect2D> scissors;
        params.dynamicScissor = true;

        for (uint32_t i = 0; i < 16; i++)
            scissors.push_back(makeRect2D(10 + i * 3, 20 + i * 2, WIDTH / 2, HEIGHT / 2));

        params.commands.push_back(TestCommandSp(new DynamicScissorTestCommand(0, scissors)));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(5, 6, WIDTH - 10, HEIGHT - 2, red)));

        testGroup->addChild(new ScissorTestCase(testCtx, "16_dynamic_scissors", params));
    }

    // Two clears with a single dynamic scissor
    {
        TestParams params(groupParams);
        params.dynamicScissor = true;
        params.commands.push_back(TestCommandSp(
            new DynamicScissorTestCommand(0, vector<VkRect2D>(1, makeRect2D(30, 40, WIDTH - 60, HEIGHT - 80)))));
        params.commands.push_back(TestCommandSp(new RectClearTestCommand(10, 10, 50, 50, red)));
        params.commands.push_back(TestCommandSp(new RectClearTestCommand(WIDTH - 80, HEIGHT - 100, 30, 40, green)));

        testGroup->addChild(new ScissorTestCase(testCtx, "dynamic_scissor_two_clears", params));
    }

    // Mixture of quad draws and clears with dynamic scissor updates
    {
        TestParams params(groupParams);
        vector<VkRect2D> scissors;

        params.dynamicScissor = true;

        scissors.push_back(makeRect2D(30, 40, 50, 60));
        scissors.push_back(makeRect2D(40, 20, 50, 50));
        params.commands.push_back(TestCommandSp(new DynamicScissorTestCommand(0, scissors)));
        params.commands.push_back(TestCommandSp(new RectClearTestCommand(10, 10, 50, 50, red)));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(40, 30, 50, 50, green)));
        scissors[1].extent.width -= 20;
        scissors[1].extent.height += 30;
        scissors[1].offset.x -= 20;
        params.commands.push_back(TestCommandSp(new DynamicScissorTestCommand(1, vector<VkRect2D>(1, scissors[1]))));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(70, 70, 50, 50, blue)));
        params.commands.push_back(TestCommandSp(new RectClearTestCommand(75, 77, 50, 50, yellow)));

        testGroup->addChild(new ScissorTestCase(testCtx, "dynamic_scissor_mix", params));
    }

    // Static scissor off by one, inside frame buffer border
    {
        VkExtent2D size = {WIDTH / 2 - 1, HEIGHT / 2 - 1};

        TestParams params(groupParams);

        params.framebufferSize = size;
        params.dynamicScissor  = false;
        params.staticScissors.push_back(makeRect2D(1, 1, size.width - 2, size.height - 2));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(0, 0, WIDTH * 4, HEIGHT * 4, red)));

        testGroup->addChild(new ScissorTestCase(testCtx, "static_scissor_framebuffer_border_in", params));
    }

    // Dynamic scissor off by one, inside frame buffer border
    {
        VkExtent2D size = {WIDTH / 2 - 1, HEIGHT / 2 - 1};

        TestParams params(groupParams);
        vector<VkRect2D> scissors;

        params.framebufferSize = size;
        params.dynamicScissor  = true;

        scissors.push_back(makeRect2D(1, 1, size.width - 2, size.height - 2));
        params.commands.push_back(TestCommandSp(new DynamicScissorTestCommand(0, scissors)));
        params.commands.push_back(TestCommandSp(new QuadDrawTestCommand(0, 0, WIDTH * 4, HEIGHT * 4, red)));

        testGroup->addChild(new ScissorTestCase(testCtx, "dynamic_scissor_framebuffer_border_in", params));
    }
}

} // namespace

TestCaseGroup *createScissorTests(TestContext &testCtx, const SharedGroupParams groupParams)
{
    return createTestGroup(testCtx, "scissor", createTests, groupParams);
}

} // namespace Draw
} // namespace vkt
