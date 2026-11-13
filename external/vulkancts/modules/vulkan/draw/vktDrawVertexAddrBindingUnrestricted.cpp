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
 * \brief Vertex address binding unrestricted extension tests
 *//*--------------------------------------------------------------------*/

#include "vktDrawVertexAddrBindingUnrestricted.hpp"
#include "vktTestCase.hpp"

#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkPipelineConstructionUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"

#include "tcuTestCase.hpp"
#include "tcuTestContext.hpp"
#include "tcuVectorType.hpp"
#include "tcuRGBA.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"

#include "deMemory.h"
#include "deStringUtil.hpp"

#include <cstdint>
#include <vector>

namespace vkt
{
namespace Draw
{
using namespace vk;
using namespace tcu;

namespace
{

struct TestParams
{
    bool unrestricted;
    VkFormat format;
    uint32_t offset;
    bool dynamicStride;
    bool dynamicVertexInput;
    uint32_t bindingCount;
    uint32_t attributeCount;
};

struct PositionVertex
{
    PositionVertex(const Vec4 pos) : position(pos)
    {
    }
    Vec4 position;
};

struct ColorVertex
{
    ColorVertex(const Vec4 col) : color(col)
    {
    }
    Vec4 color;
};

struct PositionColorVertex
{
    PositionColorVertex(const Vec4 pos, const Vec4 col) : position(pos), color(col)
    {
    }
    Vec4 position;
    Vec4 color;
};

struct BindingInfo
{
    uint32_t binding;
    uint32_t stride;
    VkVertexInputRate inputRate;
    std::vector<uint8_t> data;
};

struct AttributeInfo
{
    uint32_t location;
    uint32_t binding;
    VkFormat format;
    uint32_t offset;
};

const uint32_t kVertexCount = 6u;

class VertexBindingAddrUnrestrictedBasicTestCase : public TestCase
{
public:
    VertexBindingAddrUnrestrictedBasicTestCase(TestContext &testCtx, const std::string &name, const TestParams &params);
    ~VertexBindingAddrUnrestrictedBasicTestCase(void);
    void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const;

private:
    const TestParams m_params;
};

VertexBindingAddrUnrestrictedBasicTestCase::VertexBindingAddrUnrestrictedBasicTestCase(TestContext &testCtx,
                                                                                       const std::string &name,
                                                                                       const TestParams &params)
    : TestCase(testCtx, name.c_str())
    , m_params(params)
{
}

VertexBindingAddrUnrestrictedBasicTestCase::~VertexBindingAddrUnrestrictedBasicTestCase(void)
{
}

void VertexBindingAddrUnrestrictedBasicTestCase::checkSupport(Context &context) const
{
    if (m_params.unrestricted)
        context.requireDeviceFunctionality("VK_EXT_vertex_address_binding_unrestricted");

    context.requireDeviceFunctionality("VK_KHR_device_address_commands");

    if (m_params.dynamicStride /* && !isConstructionTypeShaderObject(params.constructionType) */)
        context.requireDeviceFunctionality("VK_EXT_extended_dynamic_state");

    if (m_params.dynamicVertexInput /* && !isConstructionTypeShaderObject(params.constructionType) */)
        context.requireDeviceFunctionality("VK_EXT_vertex_input_dynamic_state");
}

std::string getPositionTypeStr(const VkFormat format)
{
    TextureFormat tcuFormat      = mapVkFormat(format);
    const uint32_t numComponents = getNumUsedChannels(tcuFormat.order);

    std::string positionTypeStr = "";

    switch (numComponents)
    {
    case 1:
        positionTypeStr = "float";
        break;
    case 2:
        positionTypeStr = "vec2";
        break;
    case 3:
        positionTypeStr = "vec3";
        break;
    case 4:
        positionTypeStr = "vec4";
        break;
    default:
        DE_ASSERT(false);
    }

    return positionTypeStr;
}

std::string getPositionValueStr(const VkFormat format)
{
    TextureFormat tcuFormat      = mapVkFormat(format);
    const uint32_t numComponents = getNumUsedChannels(tcuFormat.order);

    std::string positionValueStr = "";

    switch (numComponents)
    {
    case 1:
    {
        const std::string posY = "(((gl_VertexIndex + 1) % 3) == 0) ? inPosition.x : -(inPosition.x)";
        positionValueStr       = "inPosition.x, " + posY + ", 0.0f, 1.0f";
        break;
    }
    case 2:
        positionValueStr = "inPosition.xy, 0.0f, 1.0f";
        break;
    case 3:
        positionValueStr = "inPosition.xyz, 1.0f";
        break;
    case 4:
        positionValueStr = "inPosition.xyzw";
        break;
    default:
        DE_ASSERT(false);
    }

    return positionValueStr;
}

void VertexBindingAddrUnrestrictedBasicTestCase::initPrograms(SourceCollections &programCollection) const
{
    const bool hasColor = (m_params.bindingCount == 2u) || (m_params.attributeCount > 1u);

    std::ostringstream vert;
    {
        const std::string positionTypeStr  = getPositionTypeStr(m_params.format);
        const std::string positionValueStr = "vec4(" + getPositionValueStr(m_params.format) + ");\n";

        vert << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n";
        vert << "layout(location = 0) in " << positionTypeStr << " inPosition;\n";

        if (hasColor)
        {
            vert << "layout(location = 1) in vec4 inColor;\n";
            vert << "layout(location = 0) out vec4 outColor;\n";
        }

        vert << "\n"
             << "void main() {\n"
             << "    gl_Position = " << positionValueStr;

        if (hasColor)
            vert << "    outColor = inColor;\n";

        vert << "}\n";
    }
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    {
        frag << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
             << "layout(location = 0) out vec4 outColor;\n";

        if (hasColor)
            frag << "layout(location = 0) in vec4 inColor;\n";

        frag
            << "\n"
            << "void main() {\n"
            << "    outColor = " << (hasColor ? "inColor" : "vec4(1.0f, 0.0f, 0.0f, 1.0f)")
            << ";\n"
            // << "    outColor = " << (hasColor ? "vec4(0.0f, 0.0f, 1.0f, 1.0f)" : "vec4(1.0f, 0.0f, 0.0f, 1.0f)") << ";\n"
            << "}\n";
    }
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

class VertexBindingAddrUnrestrictedBasicTestInstance : public TestInstance
{
public:
    VertexBindingAddrUnrestrictedBasicTestInstance(Context &context, const TestParams &params);
    ~VertexBindingAddrUnrestrictedBasicTestInstance(void);

    std::vector<VkVertexInputBindingDescription> getBindingDescriptions(void);
    std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions(const uint32_t offset);

    std::vector<VkVertexInputBindingDescription2EXT> getBindingDescriptions2(void);
    std::vector<VkVertexInputAttributeDescription2EXT> getAttributeDescriptions2(const uint32_t offset);

    void generateBindingInfos(void);
    void generateAttributeInfos(void);
    TestStatus iterate(void);

private:
    const TestParams m_params;
    std::vector<BindingInfo> m_bindingInfos;
    std::vector<AttributeInfo> m_attributeInfos;
};

TestInstance *VertexBindingAddrUnrestrictedBasicTestCase::createInstance(Context &context) const
{
    return new VertexBindingAddrUnrestrictedBasicTestInstance(context, m_params);
}

std::vector<uint8_t> getPositionData()
{
    std::vector<PositionVertex> positions;

    positions.push_back(PositionVertex(Vec4(1.0f, -1.0f, 0.0f, 1.0f)));
    positions.push_back(PositionVertex(Vec4(-1.0f, 1.0f, 0.0f, 1.0f)));
    positions.push_back(PositionVertex(Vec4(-1.0f, -1.0f, 0.0f, 1.0f)));
    positions.push_back(PositionVertex(Vec4(-1.0f, 1.0f, 0.0f, 1.0f)));
    positions.push_back(PositionVertex(Vec4(1.0f, -1.0f, 0.0f, 1.0f)));
    positions.push_back(PositionVertex(Vec4(1.0f, 1.0f, 0.0f, 1.0f)));

    std::vector<uint8_t> bytes(reinterpret_cast<const uint8_t *>(positions.data()),
                               reinterpret_cast<const uint8_t *>(positions.data()) +
                                   positions.size() * sizeof(PositionVertex));

    return bytes;
}

std::vector<uint8_t> getColorData()
{
    const auto globalColor = RGBA::blue().toVec();
    std::vector<ColorVertex> colors(kVertexCount, globalColor);

    std::vector<uint8_t> bytes(reinterpret_cast<const uint8_t *>(colors.data()),
                               reinterpret_cast<const uint8_t *>(colors.data()) + colors.size() * sizeof(ColorVertex));

    return bytes;
}

std::vector<uint8_t> getPositionAndColorData()
{
    std::vector<PositionColorVertex> positionsAndColors;

    const auto globalColor = RGBA::blue().toVec();

    positionsAndColors.push_back(PositionColorVertex(Vec4(1.0f, -1.0f, 0.0f, 1.0f), globalColor));
    positionsAndColors.push_back(PositionColorVertex(Vec4(-1.0f, 1.0f, 0.0f, 1.0f), globalColor));
    positionsAndColors.push_back(PositionColorVertex(Vec4(-1.0f, -1.0f, 0.0f, 1.0f), globalColor));
    positionsAndColors.push_back(PositionColorVertex(Vec4(-1.0f, 1.0f, 0.0f, 1.0f), globalColor));
    positionsAndColors.push_back(PositionColorVertex(Vec4(1.0f, -1.0f, 0.0f, 1.0f), globalColor));
    positionsAndColors.push_back(PositionColorVertex(Vec4(1.0f, 1.0f, 0.0f, 1.0f), globalColor));

    std::vector<uint8_t> bytes(reinterpret_cast<const uint8_t *>(positionsAndColors.data()),
                               reinterpret_cast<const uint8_t *>(positionsAndColors.data()) +
                                   positionsAndColors.size() * sizeof(PositionColorVertex));

    return bytes;
}

void VertexBindingAddrUnrestrictedBasicTestInstance::generateBindingInfos(void)
{
    uint32_t bindingIdx = 0u;

    const bool firstBindingHasMultipleAttributes = ((m_params.bindingCount == 1u) && (m_params.attributeCount > 1u));
    const auto bindingData = firstBindingHasMultipleAttributes ? getPositionAndColorData() : getPositionData();

    m_bindingInfos[bindingIdx] = {
        bindingIdx,
        static_cast<uint32_t>(firstBindingHasMultipleAttributes ? sizeof(PositionColorVertex) : sizeof(PositionVertex)),
        VK_VERTEX_INPUT_RATE_VERTEX, bindingData};

    if (m_params.bindingCount > 1u)
    {
        bindingIdx++;
        const auto colorData = getColorData();

        m_bindingInfos[bindingIdx] = {bindingIdx, static_cast<uint32_t>(sizeof(ColorVertex)),
                                      VK_VERTEX_INPUT_RATE_VERTEX, colorData};
    }
}

void VertexBindingAddrUnrestrictedBasicTestInstance::generateAttributeInfos(void)
{
    uint32_t attrIdx = 0u;

    const bool firstBindingHasMultipleAttributes = ((m_params.bindingCount == 1u) && (m_params.attributeCount > 1u));

    m_attributeInfos[attrIdx] = {0u, 0u,
                                 m_params.format, // position format
                                 static_cast<uint32_t>(firstBindingHasMultipleAttributes ?
                                                           offsetof(PositionColorVertex, position) :
                                                           offsetof(PositionVertex, position))};

    if ((m_params.bindingCount > 1u) || (m_params.attributeCount > 1u))
    {
        attrIdx++;

        m_attributeInfos[attrIdx] = {1u, (m_params.bindingCount > 1u) ? 1u : 0u,
                                     VK_FORMAT_R32G32B32A32_SFLOAT, // color format
                                     static_cast<uint32_t>(firstBindingHasMultipleAttributes ?
                                                               offsetof(PositionColorVertex, color) :
                                                               offsetof(ColorVertex, color))};
    }
}

std::vector<VkVertexInputBindingDescription> VertexBindingAddrUnrestrictedBasicTestInstance::getBindingDescriptions(
    void)
{
    const auto numBindings = de::sizeU32(m_bindingInfos);

    std::vector<VkVertexInputBindingDescription> descs(numBindings);

    for (uint32_t bindingIdx = 0u; bindingIdx < numBindings; bindingIdx++)
    {
        descs[bindingIdx].binding   = m_bindingInfos[bindingIdx].binding;
        descs[bindingIdx].stride    = m_bindingInfos[bindingIdx].stride;
        descs[bindingIdx].inputRate = m_bindingInfos[bindingIdx].inputRate;
    }

    return descs;
}

std::vector<VkVertexInputAttributeDescription> VertexBindingAddrUnrestrictedBasicTestInstance::getAttributeDescriptions(
    const uint32_t offset)
{
    const auto numAttributes = de::sizeU32(m_attributeInfos);

    std::vector<VkVertexInputAttributeDescription> descs(numAttributes);

    for (uint32_t attrIdx = 0u; attrIdx < numAttributes; attrIdx++)
    {
        descs[attrIdx].location = m_attributeInfos[attrIdx].location;
        descs[attrIdx].binding  = m_attributeInfos[attrIdx].binding;
        descs[attrIdx].format   = m_attributeInfos[attrIdx].format;
        descs[attrIdx].offset   = m_attributeInfos[attrIdx].offset + offset;
    }

    return descs;
}

std::vector<VkVertexInputBindingDescription2EXT> VertexBindingAddrUnrestrictedBasicTestInstance::
    getBindingDescriptions2(void)
{
    const auto numBindings = de::sizeU32(m_bindingInfos);

    std::vector<VkVertexInputBindingDescription2EXT> descs(numBindings);

    for (uint32_t bindingIdx = 0u; bindingIdx < numBindings; bindingIdx++)
    {
        descs[bindingIdx]           = initVulkanStructure();
        descs[bindingIdx].binding   = m_bindingInfos[bindingIdx].binding;
        descs[bindingIdx].stride    = m_bindingInfos[bindingIdx].stride;
        descs[bindingIdx].inputRate = m_bindingInfos[bindingIdx].inputRate;
        descs[bindingIdx].divisor   = 1u;
    }

    return descs;
}

std::vector<VkVertexInputAttributeDescription2EXT> VertexBindingAddrUnrestrictedBasicTestInstance::
    getAttributeDescriptions2(const uint32_t offset)
{
    const auto numAttributes = de::sizeU32(m_attributeInfos);

    const VkVertexInputAttributeDescription2EXT defaultValue = initVulkanStructureConst();
    std::vector<VkVertexInputAttributeDescription2EXT> descs(numAttributes, defaultValue);

    for (uint32_t attrIdx = 0u; attrIdx < numAttributes; attrIdx++)
    {
        descs[attrIdx].location = m_attributeInfos[attrIdx].location;
        descs[attrIdx].binding  = m_attributeInfos[attrIdx].binding;
        descs[attrIdx].format   = m_attributeInfos[attrIdx].format;
        descs[attrIdx].offset   = m_attributeInfos[attrIdx].offset + offset;
    }

    return descs;
}

VertexBindingAddrUnrestrictedBasicTestInstance::VertexBindingAddrUnrestrictedBasicTestInstance(Context &context,
                                                                                               const TestParams &params)
    : TestInstance(context)
    , m_params(params)
{
    m_bindingInfos.resize(m_params.bindingCount);
    m_attributeInfos.resize(m_params.attributeCount);

    generateBindingInfos();
    generateAttributeInfos();
}

VertexBindingAddrUnrestrictedBasicTestInstance::~VertexBindingAddrUnrestrictedBasicTestInstance(void)
{
}

VkImageCreateInfo makeImageCreateInfo(const VkImageType &imageType, const UVec2 imageSize, const VkFormat format,
                                      const VkImageUsageFlags usage, const VkImageCreateFlags flags,
                                      const VkImageTiling tiling)
{
    const VkImageCreateInfo imageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,            // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        flags,                                          // VkImageCreateFlags flags;
        imageType,                                      // VkImageType imageType;
        format,                                         // VkFormat format;
        makeExtent3D(imageSize.x(), imageSize.y(), 1u), // VkExtent3D extent;
        1u,                                             // uint32_t mipLevels;
        1u,                                             // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,                          // VkSampleCountFlagBits samples;
        tiling,                                         // VkImageTiling tiling;
        usage,                                          // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,                      // VkSharingMode sharingMode;
        0u,                                             // uint32_t queueFamilyIndexCount;
        nullptr,                                        // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,                      // VkImageLayout initialLayout;
    };
    return imageParams;
}

VkBindVertexBuffer3InfoKHR makeVertexBuffer3Info(const VkDeviceAddress address, const VkDeviceSize size,
                                                 const bool setStride, const VkDeviceSize stride)
{
    VkStridedDeviceAddressRangeKHR addressRange = {
        address, // VkDeviceAddress	address;
        size,    // VkDeviceSize	size;
        stride   // VkDeviceSize	stride;
    };

    const VkBindVertexBuffer3InfoKHR vertexBuffer3Info = {
        VK_STRUCTURE_TYPE_BIND_VERTEX_BUFFER_3_INFO_KHR, // VkStructureType					sType;
        nullptr,                                         // const void*						pNext;
        setStride,                                       // VkBool32						setStride;
        addressRange,                                    // VkStridedDeviceAddressRangeKHR	addressRange;
        (VkAddressCommandFlagsKHR)0u                     // VkAddressCommandFlagsKHR		addressFlags;
    };
    return vertexBuffer3Info;
}

TestStatus VertexBindingAddrUnrestrictedBasicTestInstance::iterate(void)
{
    const auto &vki             = m_context.getInstanceInterface();
    const auto physicalDevice   = m_context.getPhysicalDevice();
    const auto &vkd             = m_context.getDeviceInterface();
    const auto device           = m_context.getDevice();
    const auto queue            = m_context.getUniversalQueue();
    const auto queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    auto &allocator             = m_context.getDefaultAllocator();

    const auto pipelineConstructionType = PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC;

    const auto renderSize     = UVec2(8u, 8u);
    const auto outImageType   = VK_IMAGE_TYPE_2D;
    const auto outImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto outImageUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    const auto tcuFormat = mapVkFormat(outImageFormat);

    const auto vertexBufferUsage  = (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    const auto vertexBufferMemReq = (MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);

    std::vector<std::shared_ptr<BufferWithMemory>> vertexBuffers(m_params.bindingCount);

    for (uint32_t bindingIdx = 0u; bindingIdx < m_params.bindingCount; bindingIdx++)
    {
        const auto vertexBufferSize = static_cast<VkDeviceSize>(de::dataSize(m_bindingInfos[bindingIdx].data));
        const auto vertexBufferInfo = makeBufferCreateInfo(vertexBufferSize, vertexBufferUsage);

        vertexBuffers[bindingIdx] =
            std::make_shared<BufferWithMemory>(vkd, device, allocator, vertexBufferInfo, vertexBufferMemReq);

        auto &vertexBufferAlloc = vertexBuffers[bindingIdx]->getAllocation();
        void *vertexBufferPtr   = vertexBufferAlloc.getHostPtr();
        deMemcpy(vertexBufferPtr, de::dataOrNull(m_bindingInfos[bindingIdx].data),
                 static_cast<size_t>(vertexBufferSize));
        flushAlloc(vkd, device, vertexBufferAlloc);
    }

    // Create command pool and command buffer
    auto commandPool =
        createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    auto commandBufferPtr = allocateCommandBuffer(vkd, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    auto commandBuffer    = *commandBufferPtr;

    // Create render pass
    const Vec4 clearColor(tcu::RGBA::black().toVec());
    RenderPassWrapper renderPassWrapper(pipelineConstructionType, vkd, device, outImageFormat);

    // Create frame buffer
    const auto colorSubresourceRange = makeDefaultImageSubresourceRange();
    const ImageWithMemory colorImage{
        vkd, device, allocator,
        makeImageCreateInfo(outImageType, renderSize, outImageFormat, outImageUsage, 0u, VK_IMAGE_TILING_OPTIMAL),
        MemoryRequirement::Any};
    const auto colorView =
        makeImageView(vkd, device, colorImage.get(), VK_IMAGE_VIEW_TYPE_2D, outImageFormat, colorSubresourceRange);

    const VkExtent3D imageExtent = makeExtent3D(renderSize.x(), renderSize.y(), 1u);
    renderPassWrapper.createFramebuffer(vkd, device, 1u, &colorImage.get(), &colorView.get(), imageExtent.width,
                                        imageExtent.height, imageExtent.depth);

    // Create color buffer to read back output
    const auto colorBufferSize = (uint32_t)(getPixelSize(tcuFormat) * renderSize.x() * renderSize.y());
    const auto colorBufferInfo = makeBufferCreateInfo(colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const BufferWithMemory colorBuffer(vkd, device, allocator, colorBufferInfo, MemoryRequirement::HostVisible);
    {
        const auto buffAlloc = colorBuffer.getAllocation();
        std::vector<uint8_t> randomData(colorBufferSize, 0xFF);

        deMemcpy(buffAlloc.getHostPtr(), &randomData[0], colorBufferSize);
        flushAlloc(vkd, device, buffAlloc);
    }

    // Create static vertex input state
    const auto vertexBindingDescs   = getBindingDescriptions();
    const auto vertexAttributeDescs = getAttributeDescriptions(m_params.offset);

    const VkPipelineVertexInputStateCreateInfo *nullStaticVertexInputInfo = nullptr;

    const VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                             sType
        nullptr,                                                   // const void*                                 pNext
        0u,                                                        // VkPipelineVertexInputStateCreateFlags       flags
        de::sizeU32(vertexBindingDescs),    // uint32_t                                    vertexBindingDescriptionCount
        de::dataOrNull(vertexBindingDescs), // const VkVertexInputBindingDescription*      pVertexBindingDescriptions
        de::sizeU32(
            vertexAttributeDescs), // uint32_t                                    vertexAttributeDescriptionCount
        de::dataOrNull(
            vertexAttributeDescs), // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
    };

    // Create shader modules
    const auto vertexModule = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
    const auto fragModule   = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("frag"), 0u);

    // Create pipeline layout
    // todo: create info not specified because no descriptors or push consts
    const PipelineLayoutWrapper pipelineLayout(pipelineConstructionType, vkd, device);

    std::vector<VkDynamicState> dynamicStates;
    if (m_params.dynamicStride)
        dynamicStates.push_back(VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE);

    if (m_params.dynamicVertexInput)
        dynamicStates.push_back(VK_DYNAMIC_STATE_VERTEX_INPUT_EXT);

    const VkPipelineDynamicStateCreateInfo dynamicStateInfo{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                            nullptr, 0u, de::sizeU32(dynamicStates),
                                                            de::dataOrNull(dynamicStates)};

    // Create graphics pipeline
    const std::vector<VkViewport> viewports{makeViewport(renderSize)};
    const auto renderArea = makeRect2D(renderSize);
    const std::vector<VkRect2D> scissors{renderArea};

    GraphicsPipelineWrapper graphicsPipeline(vki, vkd, physicalDevice, device, m_context.getDeviceExtensions(),
                                             pipelineConstructionType);
    graphicsPipeline.setDefaultDepthStencilState()
        .setDefaultRasterizationState()
        .setDefaultMultisampleState()
        .setDefaultColorBlendState()
        .setDynamicState((m_params.dynamicStride || m_params.dynamicVertexInput) ? &dynamicStateInfo : nullptr)
        .setupVertexInputState(m_params.dynamicVertexInput ? nullStaticVertexInputInfo : &vertexInputInfo)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPassWrapper, 0u, vertexModule)
        .setupFragmentShaderState(pipelineLayout, *renderPassWrapper, 0u, fragModule)
        .setupFragmentOutputState(*renderPassWrapper)
        .setMonolithicPipelineLayout(pipelineLayout)
        .buildPipeline();

    // Create VkBindVertexBuffer3InfoKHR
    std::vector<VkBindVertexBuffer3InfoKHR> bindingVertextBuffer3Infos(m_params.bindingCount);

    for (uint32_t bindingIdx = 0; bindingIdx < m_params.bindingCount; bindingIdx++)
    {
        const VkBuffer buff = vertexBuffers[bindingIdx]->get();

        VkDeviceAddress deviceAddress = getBufferDeviceAddress(vkd, device, buff);
        deviceAddress -= m_params.offset;

        // The bound range starts offset bytes before the buffer, so extend it to still cover the whole buffer
        const VkDeviceSize deviceSize = vertexBuffers[bindingIdx]->getBufferSize() + m_params.offset;

        bool setStride      = false;
        VkDeviceSize stride = 0u;

        if (m_params.dynamicStride)
        {
            setStride = true;

            if (bindingIdx == 0u)
                stride = ((m_params.bindingCount == 1u) && (m_params.attributeCount > 1u)) ?
                             sizeof(PositionColorVertex) :
                             sizeof(PositionVertex);
            else
                stride = sizeof(ColorVertex);
        }

        const auto vertexBuffer3Info           = makeVertexBuffer3Info(deviceAddress, deviceSize, setStride, stride);
        bindingVertextBuffer3Infos[bindingIdx] = vertexBuffer3Info;
    }

    beginCommandBuffer(vkd, commandBuffer);
    {
        renderPassWrapper.begin(vkd, commandBuffer, renderArea, clearColor);

        graphicsPipeline.bind(commandBuffer);

        vkd.cmdBindVertexBuffers3KHR(commandBuffer, 0u, de::sizeU32(bindingVertextBuffer3Infos),
                                     de::dataOrNull(bindingVertextBuffer3Infos));

        // Dynamic vertex input
        if (m_params.dynamicVertexInput)
        {
            const auto dynVertBindingDescs   = getBindingDescriptions2();
            const auto dynVertAttributeDescs = getAttributeDescriptions2(m_params.offset);

            vkd.cmdSetVertexInputEXT(commandBuffer, de::sizeU32(dynVertBindingDescs),
                                     de::dataOrNull(dynVertBindingDescs), de::sizeU32(dynVertAttributeDescs),
                                     de::dataOrNull(dynVertAttributeDescs));
        }
        vkd.cmdDraw(commandBuffer, kVertexCount, 1u, 0u, 0u);

        renderPassWrapper.end(vkd, commandBuffer);

        copyImageToBuffer(vkd, commandBuffer, *colorImage, *colorBuffer, IVec2(renderSize.x(), renderSize.y()));
    }
    endCommandBuffer(vkd, commandBuffer);

    submitCommandsAndWait(vkd, device, queue, commandBuffer);

    auto bufferAlloc = colorBuffer.getAllocation();
    invalidateAlloc(vkd, device, bufferAlloc);

    const ConstPixelBufferAccess resultImage(tcuFormat, static_cast<int32_t>(renderSize.x()),
                                             static_cast<int32_t>(renderSize.y()), 1, bufferAlloc.getHostPtr());

    TextureLevel refImage(tcuFormat, renderSize.x(), renderSize.y());
    tcu::clear(refImage.getAccess(), ((m_params.bindingCount > 1u) || (m_params.attributeCount > 1u)) ?
                                         tcu::RGBA::blue().toVec() :
                                         tcu::RGBA::red().toVec());

    const Vec4 &threshold = Vec4(0.001f);
    if (!floatThresholdCompare(m_context.getTestContext().getLog(), "ComparisonResult", "Image comparison result",
                               refImage.getAccess(), resultImage, threshold, COMPARE_LOG_EVERYTHING))
        return TestStatus::fail("Fail");

    return TestStatus::pass("Pass");
}

} // namespace

std::string getFormatShortString(const vk::VkFormat format)
{
    const std::string fullName = getFormatName(format);

    DE_ASSERT(de::beginsWith(fullName, "VK_FORMAT_"));

    return de::toLower(fullName.substr(10));
}

TestCaseGroup *createVertexAddressBindingUnrestrictedTests(TestContext &testCtx)
{
    std::vector<VkFormat> vertexPositionFormats = {VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT,
                                                   VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT};

    std::vector<uint32_t> vertexOffsets = {0u, 4u, 16u, 64u, 1024u};

    de::MovePtr<TestCaseGroup> unrestrictedVertexBinding(new TestCaseGroup(testCtx, "unrestricted_vertex_binding"));

    {
        const TestParams params = {
            false,                   // bool unrestricted
            VK_FORMAT_R32G32_SFLOAT, // VkFormat format
            0u,                      // uint32_t offset
            false,                   // bool dynamicStride
            false,                   // bool dynamicVertexInput
            1u,                      // uint32_t bindingCount
            1u                       // uint32_t attributeCount
        };

        unrestrictedVertexBinding->addChild(
            new VertexBindingAddrUnrestrictedBasicTestCase(testCtx, "restricted", params));
    }

    for (const auto &format : vertexPositionFormats)
    {
        for (const auto &offset : vertexOffsets)
        {
            for (uint32_t bindingCnt = 1u; bindingCnt <= 2u; bindingCnt++)
            {
                for (uint32_t attrCnt = 1u; attrCnt <= 2u; attrCnt++)
                {
                    // todo: invalid case
                    if (bindingCnt == 2u && attrCnt == 1u)
                        continue;

                    const TestParams params = {
                        true,       // bool unrestricted
                        format,     // VkFormat format
                        offset,     // uint32_t offset
                        false,      // bool dynamicStride
                        false,      // bool dynamicVertexInput
                        bindingCnt, // uint32_t bindingCount
                        attrCnt     // uint32_t attributeCount
                    };

                    const std::string testName = "unrestricted_" + getFormatShortString(format) + "_" +
                                                 de::toString(offset) + "_binding_" + de::toString(bindingCnt) +
                                                 "_attribute_" + de::toString(attrCnt);
                    unrestrictedVertexBinding->addChild(
                        new VertexBindingAddrUnrestrictedBasicTestCase(testCtx, testName, params));
                }
            }

            // Dynamic stride
            {
                // todo: dynamicStride must be true when pipeline contruction is shader objects

                const TestParams params = {
                    true,   // bool unrestricted
                    format, // VkFormat format
                    offset, // uint32_t offset
                    true,   // bool dynamicStride
                    false,  // bool dynamicVertexInput
                    1u,     // uint32_t bindingCount
                    1u      // uint32_t attributeCount
                };

                const std::string testName =
                    "unrestricted_" + getFormatShortString(format) + "_" + de::toString(offset) + "_dynamic_stride";
                unrestrictedVertexBinding->addChild(
                    new VertexBindingAddrUnrestrictedBasicTestCase(testCtx, testName, params));
            }

            // Dynamic vertex input
            {
                // todo: dynamicStride must be true when pipeline contruction is shader objects

                const TestParams params = {
                    true,   // bool unrestricted
                    format, // VkFormat format
                    offset, // uint32_t offset
                    false,  // bool dynamicStride
                    true,   // bool dynamicVertexInput
                    1u,     // uint32_t bindingCount
                    1u      // uint32_t attributeCount
                };

                const std::string testName =
                    "unrestricted_" + getFormatShortString(format) + "_" + de::toString(offset) + "_dynamic_input";
                unrestrictedVertexBinding->addChild(
                    new VertexBindingAddrUnrestrictedBasicTestCase(testCtx, testName, params));
            }
        }
    }

    return unrestrictedVertexBinding.release();
}

} // namespace Draw
} // namespace vkt
