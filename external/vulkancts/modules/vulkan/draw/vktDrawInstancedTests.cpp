/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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
 * \brief Instanced Draw Tests
 *//*--------------------------------------------------------------------*/

#include "vktDrawInstancedTests.hpp"

#include <climits>

#include "deSharedPtr.hpp"
#include "rrRenderer.hpp"
#include "tcuImageCompare.hpp"
#include "tcuRGBA.hpp"
#include "tcuTextureUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkPrograms.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vktDrawBufferObjectUtil.hpp"
#include "vktDrawCreateInfoUtil.hpp"
#include "vktDrawImageObjectUtil.hpp"
#include "vktDrawTestCaseUtil.hpp"

namespace vkt
{
namespace Draw
{
namespace
{

static const int QUAD_GRID_SIZE = 8;
static const int WIDTH          = 128;
static const int HEIGHT         = 128;

enum AttributeDivisor
{
    ATTRIBUTE_DIVISOR_NONE,
    ATTRIBUTE_DIVISOR_EXT,
    ATTRIBUTE_DIVISOR_KHR,
};

struct TestParams
{
    enum DrawFunction
    {
        FUNCTION_DRAW = 0,
        FUNCTION_DRAW_INDEXED,
        FUNCTION_DRAW_INDIRECT,
        FUNCTION_DRAW_INDEXED_INDIRECT,

        FUNTION_LAST
    };

    DrawFunction function;
    vk::VkPrimitiveTopology topology;
    const SharedGroupParams groupParams;

    AttributeDivisor testAttribDivisor;
    uint32_t attribDivisor;

    bool testMultiview;

    bool dynamicState;

    bool useMaintenance5Ext;

    bool isIndirectDraw(void) const
    {
        return (function == FUNCTION_DRAW_INDIRECT || function == FUNCTION_DRAW_INDEXED_INDIRECT);
    }
};

struct VertexPositionAndColor
{
    VertexPositionAndColor(tcu::Vec4 position_, tcu::Vec4 color_) : position(position_), color(color_)
    {
    }

    tcu::Vec4 position;
    tcu::Vec4 color;
};

std::ostream &operator<<(std::ostream &str, TestParams const &v)
{
    std::ostringstream string;

    if (v.dynamicState)
        string << "dynamic_state_";

    switch (v.function)
    {
    case TestParams::FUNCTION_DRAW:
        string << "draw";
        break;
    case TestParams::FUNCTION_DRAW_INDEXED:
        string << "draw_indexed";
        break;
    case TestParams::FUNCTION_DRAW_INDIRECT:
        string << "draw_indirect";
        break;
    case TestParams::FUNCTION_DRAW_INDEXED_INDIRECT:
        string << "draw_indexed_indirect";
        break;
    default:
        DE_ASSERT(false);
    }

    string << "_" << de::toString(v.topology);

    if (v.testAttribDivisor == ATTRIBUTE_DIVISOR_EXT)
        string << "_attrib_divisor_" << v.attribDivisor;
    else if (v.testAttribDivisor == ATTRIBUTE_DIVISOR_KHR)
        string << "_khr_attrib_divisor_" << v.attribDivisor;

    if (v.testMultiview)
        string << "_multiview";

    return str << string.str();
}

rr::PrimitiveType mapVkPrimitiveTopology(vk::VkPrimitiveTopology primitiveTopology)
{
    switch (primitiveTopology)
    {
    case vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
        return rr::PRIMITIVETYPE_POINTS;
    case vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
        return rr::PRIMITIVETYPE_LINES;
    case vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
        return rr::PRIMITIVETYPE_LINE_STRIP;
    case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
        return rr::PRIMITIVETYPE_TRIANGLES;
    case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
        return rr::PRIMITIVETYPE_TRIANGLE_FAN;
    case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
        return rr::PRIMITIVETYPE_TRIANGLE_STRIP;
    case vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
        return rr::PRIMITIVETYPE_LINES_ADJACENCY;
    case vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
        return rr::PRIMITIVETYPE_LINE_STRIP_ADJACENCY;
    case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
        return rr::PRIMITIVETYPE_TRIANGLES_ADJACENCY;
    case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
        return rr::PRIMITIVETYPE_TRIANGLE_STRIP_ADJACENCY;
    default:
        DE_ASSERT(false);
    }
    return rr::PRIMITIVETYPE_LAST;
}

template <typename T>
de::SharedPtr<Buffer> createAndUploadBuffer(const std::vector<T> data, const vk::DeviceInterface &vk,
                                            const Context &context, vk::VkBufferUsageFlags usage)
{
    const vk::VkDeviceSize dataSize = data.size() * sizeof(T);
    de::SharedPtr<Buffer> buffer =
        Buffer::createAndAlloc(vk, context.getDevice(), BufferCreateInfo(dataSize, usage),
                               context.getDefaultAllocator(), vk::MemoryRequirement::HostVisible);

    uint8_t *ptr = reinterpret_cast<uint8_t *>(buffer->getBoundMemory().getHostPtr());

    deMemcpy(ptr, &data[0], static_cast<size_t>(dataSize));

    vk::flushAlloc(vk, context.getDevice(), buffer->getBoundMemory());
    return buffer;
}

class TestVertShader : public rr::VertexShader
{
public:
    TestVertShader(int numInstances, int firstInstance)
        : rr::VertexShader(3, 1)
        , m_numInstances(numInstances)
        , m_firstInstance(firstInstance)
    {
        m_inputs[0].type  = rr::GENERICVECTYPE_FLOAT;
        m_inputs[1].type  = rr::GENERICVECTYPE_FLOAT;
        m_inputs[2].type  = rr::GENERICVECTYPE_FLOAT;
        m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
    }

    virtual ~TestVertShader()
    {
    }

    void shadeVertices(const rr::VertexAttrib *inputs, rr::VertexPacket *const *packets, const int numPackets) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
        {
            const int instanceNdx    = packets[packetNdx]->instanceNdx + m_firstInstance;
            const tcu::Vec4 position = rr::readVertexAttribFloat(inputs[0], packets[packetNdx]->instanceNdx,
                                                                 packets[packetNdx]->vertexNdx, m_firstInstance);
            const tcu::Vec4 color    = rr::readVertexAttribFloat(inputs[1], packets[packetNdx]->instanceNdx,
                                                                 packets[packetNdx]->vertexNdx, m_firstInstance);
            const tcu::Vec4 color2   = rr::readVertexAttribFloat(inputs[2], packets[packetNdx]->instanceNdx,
                                                                 packets[packetNdx]->vertexNdx, m_firstInstance);
            packets[packetNdx]->position =
                position + tcu::Vec4((float)(packets[packetNdx]->instanceNdx * 2.0 / m_numInstances), 0.0, 0.0, 0.0);
            packets[packetNdx]->outputs[0] =
                color + tcu::Vec4((float)instanceNdx / (float)m_numInstances, 0.0, 0.0, 1.0) + color2;
        }
    }

private:
    const int m_numInstances;
    const int m_firstInstance;
};

class TestFragShader : public rr::FragmentShader
{
public:
    TestFragShader(void) : rr::FragmentShader(1, 1)
    {
        m_inputs[0].type  = rr::GENERICVECTYPE_FLOAT;
        m_outputs[0].type = rr::GENERICVECTYPE_FLOAT;
    }

    virtual ~TestFragShader()
    {
    }

    void shadeFragments(rr::FragmentPacket *packets, const int numPackets,
                        const rr::FragmentShadingContext &context) const
    {
        for (int packetNdx = 0; packetNdx < numPackets; ++packetNdx)
        {
            rr::FragmentPacket &packet = packets[packetNdx];
            for (int fragNdx = 0; fragNdx < rr::NUM_FRAGMENTS_PER_PACKET; ++fragNdx)
            {
                const tcu::Vec4 color = rr::readVarying<float>(packet, context, 0, fragNdx);
                rr::writeFragmentOutput(context, packetNdx, fragNdx, 0, color);
            }
        }
    }
};

class InstancedDrawInstance : public TestInstance
{
public:
    InstancedDrawInstance(Context &context, TestParams params);
    virtual tcu::TestStatus iterate(void);

private:
    void prepareVertexData(int instanceCount, int firstInstance, int instanceDivisor);
    void preRenderCommands(const vk::VkClearValue &clearColor, uint32_t numLayers);
    void draw(vk::VkCommandBuffer cmdBuffer, vk::VkBuffer vertexBuffer, vk::VkBuffer instancedVertexBuffer,
              de::SharedPtr<Buffer> indexBuffer, de::SharedPtr<Buffer> indirectBuffer, vk::VkDeviceSize indexBufferSize,
              uint32_t firstInstance, uint32_t instanceCount);
    void cmdBindIndexBufferImpl(vk::VkCommandBuffer commandBuffer, vk::VkBuffer indexBuffer, vk::VkDeviceSize offset,
                                vk::VkDeviceSize size, vk::VkIndexType indexType);

#ifndef CTS_USES_VULKANSC
    void beginSecondaryCmdBuffer(vk::VkRenderingFlagsKHR renderingFlags = 0u);
#endif // CTS_USES_VULKANSC

private:
    const TestParams m_params;
    const vk::DeviceInterface &m_vk;

    vk::VkFormat m_colorAttachmentFormat;

    vk::Move<vk::VkPipeline> m_pipeline;
    vk::Move<vk::VkPipelineLayout> m_pipelineLayout;

    de::SharedPtr<Image> m_colorTargetImage;
    vk::Move<vk::VkImageView> m_colorTargetView;

    PipelineCreateInfo::VertexInputState m_vertexInputState;

    vk::Move<vk::VkCommandPool> m_cmdPool;
    vk::Move<vk::VkCommandBuffer> m_cmdBuffer;
    vk::Move<vk::VkCommandBuffer> m_secCmdBuffer;

    vk::Move<vk::VkFramebuffer> m_framebuffer;
    vk::Move<vk::VkRenderPass> m_renderPass;

    // Vertex data
    std::vector<VertexPositionAndColor> m_data;
    std::vector<uint32_t> m_indexes;
    std::vector<tcu::Vec4> m_instancedColor;
};

class InstancedDrawCase : public TestCase
{
public:
    InstancedDrawCase(tcu::TestContext &testCtx, const std::string &name, TestParams params)
        : TestCase(testCtx, name)
        , m_params(params)
    {
        m_vertexShader = "#version 430\n"
                         "layout(location = 0) in vec4 in_position;\n"
                         "layout(location = 1) in vec4 in_color;\n"
                         "layout(location = 2) in vec4 in_color_2;\n"
                         "layout(push_constant) uniform TestParams {\n"
                         "    float firstInstance;\n"
                         "    float instanceCount;\n"
                         "} params;\n"
                         "layout(location = 0) out vec4 out_color;\n"
                         "out gl_PerVertex {\n"
                         "    vec4  gl_Position;\n"
                         "    float gl_PointSize;\n"
                         "};\n"
                         "void main() {\n"
                         "    gl_PointSize = 1.0;\n"
                         "    gl_Position  = in_position + vec4(float(gl_InstanceIndex - params.firstInstance) * 2.0 / "
                         "params.instanceCount, 0.0, 0.0, 0.0);\n"
                         "    out_color    = in_color + vec4(float(gl_InstanceIndex) / params.instanceCount, 0.0, 0.0, "
                         "1.0) + in_color_2;\n"
                         "}\n";

        m_fragmentShader = "#version 430\n"
                           "layout(location = 0) in vec4 in_color;\n"
                           "layout(location = 0) out vec4 out_color;\n"
                           "void main()\n"
                           "{\n"
                           "    out_color = in_color;\n"
                           "}\n";
    }

    virtual void checkSupport(Context &context) const
    {
        if (m_params.dynamicState)
        {
            const auto physicalVertexInputDynamicState = context.getVertexInputDynamicStateFeaturesEXT();
            if (!physicalVertexInputDynamicState.vertexInputDynamicState)
                TCU_THROW(NotSupportedError, "Implementation does not support vertexInputDynamicState");
        }
        if (m_params.testAttribDivisor == ATTRIBUTE_DIVISOR_EXT)
        {
            context.requireDeviceFunctionality("VK_EXT_vertex_attribute_divisor");

            const auto &vertexAttributeDivisorFeatures = context.getVertexAttributeDivisorFeatures();
            if (m_params.attribDivisor != 1 && !vertexAttributeDivisorFeatures.vertexAttributeInstanceRateDivisor)
                TCU_THROW(NotSupportedError, "Implementation does not support vertexAttributeInstanceRateDivisor");

            if (m_params.attribDivisor == 0 && !vertexAttributeDivisorFeatures.vertexAttributeInstanceRateZeroDivisor)
                TCU_THROW(NotSupportedError, "Implementation does not support vertexAttributeInstanceRateDivisorZero");
        }
#ifndef CTS_USES_VULKANSC
        else if (m_params.testAttribDivisor == ATTRIBUTE_DIVISOR_KHR)
        {
            context.requireDeviceFunctionality("VK_KHR_vertex_attribute_divisor");

            const vk::VkPhysicalDeviceVertexAttributeDivisorFeaturesKHR &vertexAttributeDivisorFeatures =
                context.getVertexAttributeDivisorFeatures();

            if (m_params.attribDivisor != 1 && !vertexAttributeDivisorFeatures.vertexAttributeInstanceRateDivisor)
                TCU_THROW(NotSupportedError, "Implementation does not support vertexAttributeInstanceRateDivisor");

            if (m_params.attribDivisor == 0 && !vertexAttributeDivisorFeatures.vertexAttributeInstanceRateZeroDivisor)
                TCU_THROW(NotSupportedError, "Implementation does not support vertexAttributeInstanceRateDivisorZero");

            const vk::VkPhysicalDeviceVertexAttributeDivisorPropertiesKHR &vertexAttributeDivisorProperties =
                context.getVertexAttributeDivisorProperties();

            if (!vertexAttributeDivisorProperties.supportsNonZeroFirstInstance)
                TCU_THROW(NotSupportedError, "Implementation does not support supportsNonZeroFirstInstance");
        }
#endif

        if (m_params.testMultiview)
        {
            context.requireDeviceFunctionality("VK_KHR_multiview");

            const vk::VkPhysicalDeviceMultiviewFeatures &multiviewFeatures = context.getMultiviewFeatures();

            if (!multiviewFeatures.multiview)
                TCU_THROW(NotSupportedError, "Implementation does not support multiview feature");
        }

#ifndef CTS_USES_VULKANSC
        if (m_params.groupParams->useDynamicRendering)
            context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

        if (m_params.topology == vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN &&
            context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
            !context.getPortabilitySubsetFeatures().triangleFans)
        {
            TCU_THROW(NotSupportedError,
                      "VK_KHR_portability_subset: Triangle fans are not supported by this implementation");
        }

        if (m_params.useMaintenance5Ext)
        {
            context.requireDeviceFunctionality(VK_KHR_MAINTENANCE_5_EXTENSION_NAME);
        }
#endif // CTS_USES_VULKANSC
    }

    TestInstance *createInstance(Context &context) const
    {
        return new InstancedDrawInstance(context, m_params);
    }

    virtual void initPrograms(vk::SourceCollections &programCollection) const
    {
        programCollection.glslSources.add("InstancedDrawVert") << glu::VertexSource(m_vertexShader);
        programCollection.glslSources.add("InstancedDrawFrag") << glu::FragmentSource(m_fragmentShader);
    }

private:
    const TestParams m_params;
    std::string m_vertexShader;
    std::string m_fragmentShader;
};

InstancedDrawInstance::InstancedDrawInstance(Context &context, TestParams params)
    : TestInstance(context)
    , m_params(params)
    , m_vk(context.getDeviceInterface())
    , m_colorAttachmentFormat(vk::VK_FORMAT_R8G8B8A8_UNORM)
{
    const vk::VkDevice device       = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    const vk::VkPushConstantRange pushConstantRange = {
        vk::VK_SHADER_STAGE_VERTEX_BIT, // VkShaderStageFlags    stageFlags;
        0u,                             // uint32_t              offset;
        (uint32_t)sizeof(float) * 2,    // uint32_t              size;
    };

    const PipelineLayoutCreateInfo pipelineLayoutCreateInfo(0, nullptr, 1, &pushConstantRange);
    m_pipelineLayout = vk::createPipelineLayout(m_vk, device, &pipelineLayoutCreateInfo);

    uint32_t arrayLayers                   = m_params.testMultiview ? 2 : 1;
    const vk::VkExtent3D targetImageExtent = {WIDTH, HEIGHT, 1};
    const ImageCreateInfo targetImageCreateInfo(vk::VK_IMAGE_TYPE_2D, m_colorAttachmentFormat, targetImageExtent, 1,
                                                arrayLayers, vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_IMAGE_TILING_OPTIMAL,
                                                vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                    vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                                    vk::VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    m_colorTargetImage = Image::createAndAlloc(m_vk, device, targetImageCreateInfo, m_context.getDefaultAllocator(),
                                               m_context.getUniversalQueueFamilyIndex());

    const enum vk::VkImageViewType imageViewType =
        m_params.testMultiview ? vk::VK_IMAGE_VIEW_TYPE_2D_ARRAY : vk::VK_IMAGE_VIEW_TYPE_2D;
    ImageSubresourceRange subresourceRange = ImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT);

    if (m_params.testMultiview)
        subresourceRange.layerCount = 2;

    const ImageViewCreateInfo colorTargetViewInfo(m_colorTargetImage->object(), imageViewType, m_colorAttachmentFormat,
                                                  subresourceRange);
    m_colorTargetView = vk::createImageView(m_vk, device, &colorTargetViewInfo);

    if (!m_params.groupParams->useDynamicRendering)
    {
        RenderPassCreateInfo renderPassCreateInfo;
        renderPassCreateInfo.addAttachment(AttachmentDescription(
            m_colorAttachmentFormat, vk::VK_SAMPLE_COUNT_1_BIT, vk::VK_ATTACHMENT_LOAD_OP_LOAD,
            vk::VK_ATTACHMENT_STORE_OP_STORE, vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE, vk::VK_ATTACHMENT_STORE_OP_STORE,
            vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_IMAGE_LAYOUT_GENERAL));

        const vk::VkAttachmentReference colorAttachmentReference = {0, vk::VK_IMAGE_LAYOUT_GENERAL};

        renderPassCreateInfo.addSubpass(SubpassDescription(vk::VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 0, nullptr, 1,
                                                           &colorAttachmentReference, nullptr, AttachmentReference(), 0,
                                                           nullptr));

        vk::VkRenderPassMultiviewCreateInfo renderPassMultiviewCreateInfo;
        // Bit mask that specifies which view rendering is broadcast to
        // 0011 = Broadcast to first and second view (layer)
        const uint32_t viewMask = 0x3;
        // Bit mask that specifices correlation between views
        // An implementation may use this for optimizations (concurrent render)
        const uint32_t correlationMask = 0x3;

        if (m_params.testMultiview)
        {
            DE_ASSERT(renderPassCreateInfo.subpassCount == 1);

            renderPassMultiviewCreateInfo.sType        = vk::VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
            renderPassMultiviewCreateInfo.pNext        = nullptr;
            renderPassMultiviewCreateInfo.subpassCount = renderPassCreateInfo.subpassCount;
            renderPassMultiviewCreateInfo.pViewMasks   = &viewMask;
            renderPassMultiviewCreateInfo.correlationMaskCount = 1u;
            renderPassMultiviewCreateInfo.pCorrelationMasks    = &correlationMask;
            renderPassMultiviewCreateInfo.pViewOffsets         = nullptr;
            renderPassMultiviewCreateInfo.dependencyCount      = 0u;

            renderPassCreateInfo.pNext = &renderPassMultiviewCreateInfo;
        }

        m_renderPass = vk::createRenderPass(m_vk, device, &renderPassCreateInfo);

        // create framebuffer
        std::vector<vk::VkImageView> colorAttachments{*m_colorTargetView};
        const FramebufferCreateInfo framebufferCreateInfo(*m_renderPass, colorAttachments, WIDTH, HEIGHT, 1);
        m_framebuffer = vk::createFramebuffer(m_vk, device, &framebufferCreateInfo);
    }

    const vk::VkVertexInputBindingDescription vertexInputBindingDescription[2] = {
        {
            0u,
            (uint32_t)sizeof(VertexPositionAndColor),
            vk::VK_VERTEX_INPUT_RATE_VERTEX,
        },
        {
            1u,
            (uint32_t)sizeof(tcu::Vec4),
            vk::VK_VERTEX_INPUT_RATE_INSTANCE,
        },
    };

    const vk::VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] = {
        {0u, 0u, vk::VK_FORMAT_R32G32B32A32_SFLOAT, 0u},
        {
            1u,
            0u,
            vk::VK_FORMAT_R32G32B32A32_SFLOAT,
            (uint32_t)sizeof(tcu::Vec4),
        },
        {
            2u,
            1u,
            vk::VK_FORMAT_R32G32B32A32_SFLOAT,
            0,
        }};

    m_vertexInputState = PipelineCreateInfo::VertexInputState(2, vertexInputBindingDescription,
                                                              DE_LENGTH_OF_ARRAY(vertexInputAttributeDescriptions),
                                                              vertexInputAttributeDescriptions);

    const vk::VkVertexInputBindingDivisorDescriptionEXT vertexInputBindingDivisorDescription = {
        1u,
        m_params.attribDivisor,
    };

    if (m_params.testAttribDivisor != ATTRIBUTE_DIVISOR_NONE)
        m_vertexInputState.addDivisors(1, &vertexInputBindingDivisorDescription);

    const CmdPoolCreateInfo cmdPoolCreateInfo(queueFamilyIndex);
    m_cmdPool   = vk::createCommandPool(m_vk, device, &cmdPoolCreateInfo);
    m_cmdBuffer = vk::allocateCommandBuffer(m_vk, device, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    if (m_params.groupParams->useSecondaryCmdBuffer)
        m_secCmdBuffer = vk::allocateCommandBuffer(m_vk, device, *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY);

    const vk::Unique<vk::VkShaderModule> vs(
        createShaderModule(m_vk, device, m_context.getBinaryCollection().get("InstancedDrawVert"), 0));
    const vk::Unique<vk::VkShaderModule> fs(
        createShaderModule(m_vk, device, m_context.getBinaryCollection().get("InstancedDrawFrag"), 0));

    const PipelineCreateInfo::ColorBlendState::Attachment vkCbAttachmentState;

    vk::VkViewport viewport = vk::makeViewport(WIDTH, HEIGHT);
    vk::VkRect2D scissor    = vk::makeRect2D(WIDTH, HEIGHT);

    PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, 0);
    pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", vk::VK_SHADER_STAGE_VERTEX_BIT));
    pipelineCreateInfo.addShader(
        PipelineCreateInfo::PipelineShaderStage(*fs, "main", vk::VK_SHADER_STAGE_FRAGMENT_BIT));
    pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(m_params.topology));
    pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &vkCbAttachmentState));
    pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1, std::vector<vk::VkViewport>(1, viewport),
                                                                  std::vector<vk::VkRect2D>(1, scissor)));
    pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState());
    pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
    pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());

    if (m_params.dynamicState)
    {
        vk::VkDynamicState dynStates[] = {vk::VK_DYNAMIC_STATE_VERTEX_INPUT_EXT};

        vk::VkPipelineDynamicStateCreateInfo dynamicState{vk::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                          nullptr, 0, 1, dynStates};
        pipelineCreateInfo.addState(dynamicState);
    }
    else
    {
        pipelineCreateInfo.addState(PipelineCreateInfo::VertexInputState(m_vertexInputState));
    }

#ifndef CTS_USES_VULKANSC
    vk::VkPipelineRenderingCreateInfoKHR renderingFormatCreateInfo{
        vk::VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        nullptr,
        0u,
        1u,
        &m_colorAttachmentFormat,
        vk::VK_FORMAT_UNDEFINED,
        vk::VK_FORMAT_UNDEFINED};

    if (m_params.groupParams->useDynamicRendering)
    {
        pipelineCreateInfo.pNext = &renderingFormatCreateInfo;

        if (m_params.testMultiview)
            renderingFormatCreateInfo.viewMask = 3u;
    }
#endif // CTS_USES_VULKANSC

    m_pipeline = vk::createGraphicsPipeline(m_vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);
}

tcu::TestStatus InstancedDrawInstance::iterate()
{
    const vk::VkQueue queue                      = m_context.getUniversalQueue();
    const vk::VkDevice device                    = m_context.getDevice();
    static const uint32_t instanceCounts[]       = {0, 1, 2, 4, 20};
    static const uint32_t firstInstanceIndices[] = {0, 1, 3, 4, 20};
    const uint32_t numLayers                     = m_params.testMultiview ? 2 : 1;
    const vk::VkRect2D renderArea                = vk::makeRect2D(WIDTH, HEIGHT);

    qpTestResult res = QP_TEST_RESULT_PASS;

    const vk::VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    int firstInstanceIndicesCount     = DE_LENGTH_OF_ARRAY(firstInstanceIndices);

    // Require 'drawIndirectFirstInstance' feature to run non-zero firstInstance indirect draw tests.
    const auto &deviceFeatures = m_context.getDeviceFeatures();
    if (m_params.isIndirectDraw() && !deviceFeatures.drawIndirectFirstInstance)
    {
        firstInstanceIndicesCount = 1;
    }

    for (int instanceCountNdx = 0; instanceCountNdx < DE_LENGTH_OF_ARRAY(instanceCounts); instanceCountNdx++)
    {
        const uint32_t instanceCount = instanceCounts[instanceCountNdx];
        for (int firstInstanceIndexNdx = 0; firstInstanceIndexNdx < firstInstanceIndicesCount; firstInstanceIndexNdx++)
        {
            // Prepare vertex data for at least one instance
            const uint32_t prepareCount  = de::max(instanceCount, 1u);
            const uint32_t firstInstance = firstInstanceIndices[firstInstanceIndexNdx];

            prepareVertexData(prepareCount, firstInstance,
                              (m_params.testAttribDivisor != ATTRIBUTE_DIVISOR_NONE) ? m_params.attribDivisor : 1);
            const de::SharedPtr<Buffer> vertexBuffer =
                createAndUploadBuffer(m_data, m_vk, m_context, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
            const de::SharedPtr<Buffer> instancedVertexBuffer =
                createAndUploadBuffer(m_instancedColor, m_vk, m_context, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

            de::SharedPtr<Buffer> indexBuffer;
            if (m_params.function == TestParams::FUNCTION_DRAW_INDEXED ||
                m_params.function == TestParams::FUNCTION_DRAW_INDEXED_INDIRECT)
                indexBuffer = createAndUploadBuffer(m_indexes, m_vk, m_context, vk::VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

            de::SharedPtr<Buffer> indirectBuffer;
            if (m_params.function == TestParams::FUNCTION_DRAW_INDIRECT)
            {
                std::vector<vk::VkDrawIndirectCommand> drawCommands;
                drawCommands.push_back({
                    (uint32_t)m_data.size(), // uint32_t vertexCount;
                    instanceCount,           // uint32_t instanceCount;
                    0u,                      // uint32_t firstVertex;
                    firstInstance            // uint32_t firstInstance;
                });
                indirectBuffer =
                    createAndUploadBuffer(drawCommands, m_vk, m_context, vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
            }
            if (m_params.function == TestParams::FUNCTION_DRAW_INDEXED_INDIRECT)
            {
                std::vector<vk::VkDrawIndexedIndirectCommand> drawCommands;
                drawCommands.push_back({
                    (uint32_t)m_indexes.size(), // uint32_t indexCount;
                    instanceCount,              // uint32_t instanceCount;
                    0u,                         // uint32_t firstIndex;
                    0,                          // int32_t vertexOffset;
                    firstInstance               // uint32_t firstInstance;
                });
                indirectBuffer =
                    createAndUploadBuffer(drawCommands, m_vk, m_context, vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
            }

#ifndef CTS_USES_VULKANSC
            const uint32_t layerCount = (m_params.testMultiview) ? 2u : 1u;
            const uint32_t viewMask   = (m_params.testMultiview) ? 3u : 0u;
            if (m_params.groupParams->useSecondaryCmdBuffer)
            {
                // record secondary command buffer
                if (m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
                {
                    beginSecondaryCmdBuffer(vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT);
                    beginRendering(m_vk, *m_secCmdBuffer, *m_colorTargetView, renderArea, clearColor,
                                   vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_LOAD, 0u, layerCount,
                                   viewMask);
                }
                else
                    beginSecondaryCmdBuffer();

                draw(*m_secCmdBuffer, vertexBuffer->object(), instancedVertexBuffer->object(), indexBuffer,
                     indirectBuffer, (m_indexes.size() * sizeof(uint32_t)), firstInstance, instanceCount);

                if (m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
                    endRendering(m_vk, *m_secCmdBuffer);

                endCommandBuffer(m_vk, *m_secCmdBuffer);

                // record primary command buffer
                beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);

                preRenderCommands(clearColor, numLayers);

                if (!m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
                {
                    beginRendering(m_vk, *m_cmdBuffer, *m_colorTargetView, renderArea, clearColor,
                                   vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_LOAD,
                                   vk::VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT_KHR, layerCount, viewMask);
                }

                m_vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_secCmdBuffer);

                if (!m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
                    endRendering(m_vk, *m_cmdBuffer);

                endCommandBuffer(m_vk, *m_cmdBuffer);
            }
            else if (m_params.groupParams->useDynamicRendering)
            {
                beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
                preRenderCommands(clearColor, numLayers);

                beginRendering(m_vk, *m_cmdBuffer, *m_colorTargetView, renderArea, clearColor,
                               vk::VK_IMAGE_LAYOUT_GENERAL, vk::VK_ATTACHMENT_LOAD_OP_LOAD, 0u, layerCount, viewMask);
                draw(*m_cmdBuffer, vertexBuffer->object(), instancedVertexBuffer->object(), indexBuffer, indirectBuffer,
                     (m_indexes.size() * sizeof(uint32_t)), firstInstance, instanceCount);
                endRendering(m_vk, *m_cmdBuffer);

                endCommandBuffer(m_vk, *m_cmdBuffer);
            }
#endif // CTS_USES_VULKANSC

            if (!m_params.groupParams->useDynamicRendering)
            {
                beginCommandBuffer(m_vk, *m_cmdBuffer, 0u);
                preRenderCommands(clearColor, numLayers);

                beginRenderPass(m_vk, *m_cmdBuffer, *m_renderPass, *m_framebuffer, renderArea);
                draw(*m_cmdBuffer, vertexBuffer->object(), instancedVertexBuffer->object(), indexBuffer, indirectBuffer,
                     (m_indexes.size() * sizeof(uint32_t)), firstInstance, instanceCount);
                endRenderPass(m_vk, *m_cmdBuffer);

                endCommandBuffer(m_vk, *m_cmdBuffer);
            }

            /*

            void InstancedDrawInstance::beginRender(vk::VkCommandBuffer cmdBuffer, const vk::VkClearValue& clearColor, vk::VkRenderingFlagsKHR renderingFlags)
            {

                if (m_params.groupParams->useDynamicRendering)
                else
            */

            submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());
            m_context.resetCommandPoolForVKSC(device, *m_cmdPool);

            // Reference rendering
            std::vector<tcu::Vec4> vetrices;
            std::vector<tcu::Vec4> colors;

            for (std::vector<VertexPositionAndColor>::const_iterator it = m_data.begin(); it != m_data.end(); ++it)
            {
                vetrices.push_back(it->position);
                colors.push_back(it->color);
            }

            tcu::TextureLevel refImage(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5 + WIDTH),
                                       (int)(0.5 + HEIGHT));

            tcu::clear(refImage.getAccess(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

            const TestVertShader vertShader(instanceCount, firstInstance);
            const TestFragShader fragShader;
            const rr::Program program(&vertShader, &fragShader);
            const rr::MultisamplePixelBufferAccess colorBuffer =
                rr::MultisamplePixelBufferAccess::fromSinglesampleAccess(refImage.getAccess());
            const rr::RenderTarget renderTarget(colorBuffer);
            const rr::RenderState renderState((rr::ViewportState(colorBuffer)),
                                              m_context.getDeviceProperties().limits.subPixelPrecisionBits);
            const rr::Renderer renderer;

            const rr::VertexAttrib vertexAttribs[] = {
                rr::VertexAttrib(rr::VERTEXATTRIBTYPE_FLOAT, 4, sizeof(tcu::Vec4), 0, &vetrices[0]),
                rr::VertexAttrib(rr::VERTEXATTRIBTYPE_FLOAT, 4, sizeof(tcu::Vec4), 0, &colors[0]),
                // The reference renderer treats a divisor of 0 as meaning per-vertex.  Use INT_MAX instead; it should work just as well.
                rr::VertexAttrib(rr::VERTEXATTRIBTYPE_FLOAT, 4, sizeof(tcu::Vec4),
                                 (m_params.testAttribDivisor != ATTRIBUTE_DIVISOR_NONE) ?
                                     (m_params.attribDivisor == 0 ? INT_MAX : m_params.attribDivisor) :
                                     1,
                                 &m_instancedColor[0])};

            if (m_params.function == TestParams::FUNCTION_DRAW ||
                m_params.function == TestParams::FUNCTION_DRAW_INDIRECT)
            {
                const rr::PrimitiveList primitives =
                    rr::PrimitiveList(mapVkPrimitiveTopology(m_params.topology), (int)vetrices.size(), 0);
                const rr::DrawCommand command(renderState, renderTarget, program, DE_LENGTH_OF_ARRAY(vertexAttribs),
                                              &vertexAttribs[0], primitives);
                renderer.drawInstanced(command, instanceCount);
            }
            else
            {
                const rr::DrawIndices indicies(m_indexes.data());

                const rr::PrimitiveList primitives =
                    rr::PrimitiveList(mapVkPrimitiveTopology(m_params.topology), (int)m_indexes.size(), indicies);
                const rr::DrawCommand command(renderState, renderTarget, program, DE_LENGTH_OF_ARRAY(vertexAttribs),
                                              &vertexAttribs[0], primitives);
                renderer.drawInstanced(command, instanceCount);
            }

            const vk::VkOffset3D zeroOffset = {0, 0, 0};
            for (uint32_t i = 0; i < numLayers; i++)
            {
                const tcu::ConstPixelBufferAccess renderedFrame =
                    m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(), vk::VK_IMAGE_LAYOUT_GENERAL,
                                                    zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT, 0, i);

                tcu::TestLog &log = m_context.getTestContext().getLog();

                std::ostringstream resultDesc;
                resultDesc << "Image layer " << i << " comparison result. Instance count: " << instanceCount
                           << " first instance index: " << firstInstance;

                if (m_params.topology == vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
                {
                    const bool ok = tcu::intThresholdPositionDeviationCompare(
                        log, "Result", resultDesc.str().c_str(), refImage.getAccess(), renderedFrame,
                        tcu::UVec4(4u),      // color threshold
                        tcu::IVec3(1, 1, 0), // position deviation tolerance
                        true,                // don't check the pixels at the boundary
                        tcu::COMPARE_LOG_RESULT);

                    if (!ok)
                        res = QP_TEST_RESULT_FAIL;
                }
                else
                {
                    if (!tcu::fuzzyCompare(log, "Result", resultDesc.str().c_str(), refImage.getAccess(), renderedFrame,
                                           0.05f, tcu::COMPARE_LOG_RESULT))
                        res = QP_TEST_RESULT_FAIL;
                }
            }
        }
    }
    return tcu::TestStatus(res, qpGetTestResultName(res));
}

void InstancedDrawInstance::prepareVertexData(int instanceCount, int firstInstance, int instanceDivisor)
{
    m_data.clear();
    m_indexes.clear();
    m_instancedColor.clear();

    if (m_params.function == TestParams::FUNCTION_DRAW || m_params.function == TestParams::FUNCTION_DRAW_INDIRECT)
    {
        for (int y = 0; y < QUAD_GRID_SIZE; y++)
        {
            for (int x = 0; x < QUAD_GRID_SIZE; x++)
            {
                const float fx0 = -1.0f + (float)(x + 0) / (float)QUAD_GRID_SIZE * 2.0f / (float)instanceCount;
                const float fx1 = -1.0f + (float)(x + 1) / (float)QUAD_GRID_SIZE * 2.0f / (float)instanceCount;
                const float fy0 = -1.0f + (float)(y + 0) / (float)QUAD_GRID_SIZE * 2.0f;
                const float fy1 = -1.0f + (float)(y + 1) / (float)QUAD_GRID_SIZE * 2.0f;

                // Vertices of a quad's lower-left triangle: (fx0, fy0), (fx1, fy0) and (fx0, fy1)
                m_data.push_back(VertexPositionAndColor(tcu::Vec4(fx0, fy0, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
                m_data.push_back(VertexPositionAndColor(tcu::Vec4(fx1, fy0, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
                m_data.push_back(VertexPositionAndColor(tcu::Vec4(fx0, fy1, 1.0f, 1.0f), tcu::RGBA::green().toVec()));

                // Vertices of a quad's upper-right triangle: (fx1, fy1), (fx0, fy1) and (fx1, fy0)
                m_data.push_back(VertexPositionAndColor(tcu::Vec4(fx1, fy1, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
                m_data.push_back(VertexPositionAndColor(tcu::Vec4(fx0, fy1, 1.0f, 1.0f), tcu::RGBA::green().toVec()));
                m_data.push_back(VertexPositionAndColor(tcu::Vec4(fx1, fy0, 1.0f, 1.0f), tcu::RGBA::blue().toVec()));
            }
        }
    }
    else
    {
        for (int y = 0; y < QUAD_GRID_SIZE + 1; y++)
        {
            for (int x = 0; x < QUAD_GRID_SIZE + 1; x++)
            {
                const float fx = -1.0f + (float)x / (float)QUAD_GRID_SIZE * 2.0f / (float)instanceCount;
                const float fy = -1.0f + (float)y / (float)QUAD_GRID_SIZE * 2.0f;

                m_data.push_back(VertexPositionAndColor(
                    tcu::Vec4(fx, fy, 1.0f, 1.0f), (y % 2 ? tcu::RGBA::blue().toVec() : tcu::RGBA::green().toVec())));
            }
        }

        for (int y = 0; y < QUAD_GRID_SIZE; y++)
        {
            for (int x = 0; x < QUAD_GRID_SIZE; x++)
            {
                const int ndx00 = y * (QUAD_GRID_SIZE + 1) + x;
                const int ndx10 = y * (QUAD_GRID_SIZE + 1) + x + 1;
                const int ndx01 = (y + 1) * (QUAD_GRID_SIZE + 1) + x;
                const int ndx11 = (y + 1) * (QUAD_GRID_SIZE + 1) + x + 1;

                // Lower-left triangle of a quad.
                m_indexes.push_back((uint16_t)ndx00);
                m_indexes.push_back((uint16_t)ndx10);
                m_indexes.push_back((uint16_t)ndx01);

                // Upper-right triangle of a quad.
                m_indexes.push_back((uint16_t)ndx11);
                m_indexes.push_back((uint16_t)ndx01);
                m_indexes.push_back((uint16_t)ndx10);
            }
        }
    }

    const int colorCount =
        instanceDivisor == 0 ? 1 : (instanceCount + firstInstance + instanceDivisor - 1) / instanceDivisor;
    for (int i = 0; i < instanceCount + firstInstance; i++)
    {
        m_instancedColor.push_back(tcu::Vec4(0.0, (float)(1.0 - i * 1.0 / colorCount) / 2, 0.0, 1.0));
    }
}

void InstancedDrawInstance::preRenderCommands(const vk::VkClearValue &clearColor, uint32_t numLayers)
{
    const ImageSubresourceRange subresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, numLayers);

    if (m_params.testMultiview)
    {
        vk::VkImageMemoryBarrier barrier{
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            0u,                                         // VkAccessFlags srcAccessMask;
            vk::VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags dstAccessMask;
            vk::VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout oldLayout;
            vk::VK_IMAGE_LAYOUT_GENERAL,                // VkImageLayout newLayout;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                    // uint32_t dstQueueFamilyIndex;
            m_colorTargetImage->object(),               // VkImage image;
            subresourceRange                            // VkImageSubresourceRange subresourceRange;
        };

        m_vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                                (vk::VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &barrier);
    }
    else
    {
        initialTransitionColor2DImage(m_vk, *m_cmdBuffer, m_colorTargetImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL,
                                      vk::VK_ACCESS_TRANSFER_WRITE_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT);
    }

    m_vk.cmdClearColorImage(*m_cmdBuffer, m_colorTargetImage->object(), vk::VK_IMAGE_LAYOUT_GENERAL, &clearColor.color,
                            1, &subresourceRange);

    const vk::VkMemoryBarrier memBarrier{
        vk::VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, vk::VK_ACCESS_TRANSFER_WRITE_BIT,
        vk::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};

    m_vk.cmdPipelineBarrier(*m_cmdBuffer, vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
                            vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 1, &memBarrier, 0, nullptr, 0,
                            nullptr);
}

void InstancedDrawInstance::cmdBindIndexBufferImpl(vk::VkCommandBuffer commandBuffer, vk::VkBuffer indexBuffer,
                                                   vk::VkDeviceSize offset, vk::VkDeviceSize size,
                                                   vk::VkIndexType indexType)
{
#ifndef CTS_USES_VULKANSC
    if (m_params.useMaintenance5Ext)
        m_vk.cmdBindIndexBuffer2KHR(commandBuffer, indexBuffer, offset, size, indexType);
    else
#endif
    {
        DE_UNREF(size);
        m_vk.cmdBindIndexBuffer(commandBuffer, indexBuffer, offset, indexType);
    }
}

void InstancedDrawInstance::draw(vk::VkCommandBuffer cmdBuffer, vk::VkBuffer vertexBuffer,
                                 vk::VkBuffer instancedVertexBuffer, de::SharedPtr<Buffer> indexBuffer,
                                 de::SharedPtr<Buffer> indirectBuffer, vk::VkDeviceSize indexBufferSize,
                                 uint32_t firstInstance, uint32_t instanceCount)
{
    if (m_params.function == TestParams::FUNCTION_DRAW_INDEXED ||
        m_params.function == TestParams::FUNCTION_DRAW_INDEXED_INDIRECT)
        cmdBindIndexBufferImpl(cmdBuffer, indexBuffer->object(), 0, indexBufferSize, vk::VK_INDEX_TYPE_UINT32);

    const vk::VkBuffer vertexBuffers[]{vertexBuffer, instancedVertexBuffer};
    const vk::VkDeviceSize vertexBufferOffsets[]{0, 0};

    m_vk.cmdBindVertexBuffers(cmdBuffer, 0, DE_LENGTH_OF_ARRAY(vertexBuffers), vertexBuffers, vertexBufferOffsets);

    const float pushConstants[] = {(float)firstInstance, (float)instanceCount};
    m_vk.cmdPushConstants(cmdBuffer, *m_pipelineLayout, vk::VK_SHADER_STAGE_VERTEX_BIT, 0u,
                          (uint32_t)sizeof(pushConstants), pushConstants);
    m_vk.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

    if (m_params.dynamicState)
    {
        vk::VkVertexInputBindingDescription2EXT vertexBindingDescription[2]{
            {vk::VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT, 0, 0u,
             (uint32_t)sizeof(VertexPositionAndColor), vk::VK_VERTEX_INPUT_RATE_VERTEX, 1},
            {vk::VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT, 0, 1u, (uint32_t)sizeof(tcu::Vec4),
             vk::VK_VERTEX_INPUT_RATE_INSTANCE, m_params.attribDivisor},

        };
        vk::VkVertexInputAttributeDescription2EXT vertexAttributeDescription[3]{
            {vk::VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT, 0, 0u, 0u,
             vk::VK_FORMAT_R32G32B32A32_SFLOAT, 0u},
            {
                vk::VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
                0,
                1u,
                0u,
                vk::VK_FORMAT_R32G32B32A32_SFLOAT,
                (uint32_t)sizeof(tcu::Vec4),
            },
            {
                vk::VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
                0,
                2u,
                1u,
                vk::VK_FORMAT_R32G32B32A32_SFLOAT,
                0,
            }};

        m_vk.cmdSetVertexInputEXT(cmdBuffer, 2, vertexBindingDescription, 3, vertexAttributeDescription);
    }

    switch (m_params.function)
    {
    case TestParams::FUNCTION_DRAW:
        m_vk.cmdDraw(cmdBuffer, (uint32_t)m_data.size(), instanceCount, 0u, firstInstance);
        break;

    case TestParams::FUNCTION_DRAW_INDEXED:
        m_vk.cmdDrawIndexed(cmdBuffer, (uint32_t)m_indexes.size(), instanceCount, 0u, 0u, firstInstance);
        break;

    case TestParams::FUNCTION_DRAW_INDIRECT:
        m_vk.cmdDrawIndirect(cmdBuffer, indirectBuffer->object(), 0, 1u, 0u);
        break;

    case TestParams::FUNCTION_DRAW_INDEXED_INDIRECT:
        m_vk.cmdDrawIndexedIndirect(cmdBuffer, indirectBuffer->object(), 0, 1u, 0u);
        break;

    default:
        DE_ASSERT(false);
    }
}

#ifndef CTS_USES_VULKANSC
void InstancedDrawInstance::beginSecondaryCmdBuffer(vk::VkRenderingFlagsKHR renderingFlags)
{
    const vk::VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo{
        vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR, // VkStructureType sType;
        nullptr,                                                             // const void* pNext;
        renderingFlags,                                                      // VkRenderingFlagsKHR flags;
        (m_params.testMultiview) ? 3u : 0u,                                  // uint32_t viewMask;
        1u,                                                                  // uint32_t colorAttachmentCount;
        &m_colorAttachmentFormat,                                            // const VkFormat* pColorAttachmentFormats;
        vk::VK_FORMAT_UNDEFINED,                                             // VkFormat depthAttachmentFormat;
        vk::VK_FORMAT_UNDEFINED,                                             // VkFormat stencilAttachmentFormat;
        vk::VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits rasterizationSamples;
    };

    const vk::VkCommandBufferInheritanceInfo bufferInheritanceInfo{
        vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, // VkStructureType sType;
        &inheritanceRenderingInfo,                             // const void* pNext;
        VK_NULL_HANDLE,                                        // VkRenderPass renderPass;
        0u,                                                    // uint32_t subpass;
        VK_NULL_HANDLE,                                        // VkFramebuffer framebuffer;
        VK_FALSE,                                              // VkBool32 occlusionQueryEnable;
        (vk::VkQueryControlFlags)0u,                           // VkQueryControlFlags queryFlags;
        (vk::VkQueryPipelineStatisticFlags)0u                  // VkQueryPipelineStatisticFlags pipelineStatistics;
    };

    vk::VkCommandBufferUsageFlags usageFlags = vk::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (!m_params.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
        usageFlags |= vk::VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

    const vk::VkCommandBufferBeginInfo commandBufBeginParams{
        vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        usageFlags,                                      // VkCommandBufferUsageFlags flags;
        &bufferInheritanceInfo};

    VK_CHECK(m_vk.beginCommandBuffer(*m_secCmdBuffer, &commandBufBeginParams));
}
#endif // CTS_USES_VULKANSC

} // namespace

InstancedTests::InstancedTests(tcu::TestContext &testCtx, const SharedGroupParams groupParams)
    : TestCaseGroup(testCtx, "instanced")
    , m_groupParams(groupParams)
{
    static const vk::VkPrimitiveTopology topologies[] = {
        vk::VK_PRIMITIVE_TOPOLOGY_POINT_LIST,     vk::VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
        vk::VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,     vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
    };
    static const TestParams::DrawFunction functions[] = {
        TestParams::FUNCTION_DRAW,
        TestParams::FUNCTION_DRAW_INDEXED,
        TestParams::FUNCTION_DRAW_INDIRECT,
        TestParams::FUNCTION_DRAW_INDEXED_INDIRECT,
    };
    static const AttributeDivisor attribDivisors[] = {
        ATTRIBUTE_DIVISOR_NONE,
        ATTRIBUTE_DIVISOR_EXT,
#ifndef CTS_USES_VULKANSC
        ATTRIBUTE_DIVISOR_KHR,
#endif
    };

    static const bool multiviews[] = {false, true};

    static const uint32_t divisors[] = {0, 1, 2, 4, 20};
    for (int dynState = 0; dynState < 2; dynState++)
    {
        for (int topologyNdx = 0; topologyNdx < DE_LENGTH_OF_ARRAY(topologies); topologyNdx++)
        {
            // reduce number of tests for dynamic rendering cases where secondary command buffer is used
            if (groupParams->useSecondaryCmdBuffer && (topologyNdx % 2u))
                continue;

            for (int functionNdx = 0; functionNdx < DE_LENGTH_OF_ARRAY(functions); functionNdx++)
            {
                for (int testAttribDivisor = 0; testAttribDivisor < DE_LENGTH_OF_ARRAY(attribDivisors);
                     testAttribDivisor++)
                {
                    AttributeDivisor attribDivisor = attribDivisors[testAttribDivisor];
                    for (int divisorNdx = 0; divisorNdx < DE_LENGTH_OF_ARRAY(divisors); divisorNdx++)
                    {
                        // reduce number of tests for dynamic rendering cases where secondary command buffer is used
                        if (groupParams->useSecondaryCmdBuffer && (divisorNdx % 2u))
                            continue;

                        for (int multiviewNdx = 0; multiviewNdx < DE_LENGTH_OF_ARRAY(multiviews); multiviewNdx++)
                        {
                            // If we don't have VK_EXT_vertex_attribute_divisor or VK_KHR_vertex_attribute_divisor, we only get a divisor or 1.
                            if (attribDivisor == ATTRIBUTE_DIVISOR_NONE && divisors[divisorNdx] != 1)
                                continue;

                            TestParams param{
                                functions[functionNdx],       // DrawFunction function;
                                topologies[topologyNdx],      // vk::VkPrimitiveTopology topology;
                                groupParams,                  // const SharedGroupParams groupParams;
                                attribDivisor,                // AttributeDivisor testAttribDivisor;
                                divisors[divisorNdx],         // uint32_t attribDivisor;
                                multiviews[multiviewNdx],     // bool testMultiview;
                                dynState == 0 ? false : true, // bool dynamicState;
                                false                         // bool useMaintenance5Ext;
                            };

                            // Add multiview tests only when vertex attribute divisor is enabled.
                            if (param.testMultiview && !testAttribDivisor)
                                continue;

                            std::string testName = de::toString(param);

                            addChild(new InstancedDrawCase(m_testCtx, de::toLower(testName), param));

#ifndef CTS_USES_VULKANSC
                            if (TestParams::FUNCTION_DRAW_INDEXED == functions[functionNdx] ||
                                TestParams::FUNCTION_DRAW_INDEXED_INDIRECT == functions[functionNdx])
                            {
                                param.useMaintenance5Ext = true;
                                testName += "_maintenance_5";
                                // Instanced drawing test using vkCmdBindIndexBuffer2KHR() introduced in VK_KHR_maintenance5
                                addChild(new InstancedDrawCase(m_testCtx, de::toLower(testName), param));
                            }
#endif // CTS_USES_VULKANSC
                        }
                    }
                }
            }
        }
    }
}

InstancedTests::~InstancedTests()
{
}

} // namespace Draw
} // namespace vkt
