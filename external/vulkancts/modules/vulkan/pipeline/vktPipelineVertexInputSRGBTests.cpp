/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
 * Copyright (c) 2025 Valve Corporation.
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
 * \brief Vertex Input Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineVertexInputSRGBTests.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include "deRandom.hpp"

#include <vector>
#include <sstream>
#include <cstring>

namespace vkt
{
namespace pipeline
{

namespace
{

using namespace vk;

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

#if 0
VkFormat getUnormFormatVariant(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R8_SRGB: return VK_FORMAT_R8_UNORM;
    case VK_FORMAT_R8G8_SRGB: return VK_FORMAT_R8G8_UNORM;
    case VK_FORMAT_R8G8B8_SRGB: return VK_FORMAT_R8G8B8_UNORM;
    case VK_FORMAT_B8G8R8_SRGB: return VK_FORMAT_B8G8R8_UNORM;
    case VK_FORMAT_R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_UNORM;
    case VK_FORMAT_B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_UNORM;
    default:
        break;
    }

    DE_ASSERT(false);
    return VK_FORMAT_UNDEFINED;
}
#endif

struct SRGBVertexInputParams
{
    PipelineConstructionType constructionType;
    VkFormat format;
    uint32_t component; // 0,1,2,3 == R,G,B,A
    bool strict;

    uint32_t getRandomSeed() const
    {
        return ((static_cast<uint32_t>(format) << 2) | component);
    }

    tcu::IVec3 getExtent() const
    {
        return tcu::IVec3(16, 16, 1);
    }
};

class SRGBVertexInputInstance : public vkt::TestInstance
{
public:
    SRGBVertexInputInstance(Context &context, const SRGBVertexInputParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }
    virtual ~SRGBVertexInputInstance() = default;

    bool runWithCoords(const std::vector<float> &coords, uint32_t runId, int expectedCoveredRows);
    tcu::TestStatus iterate() override;

protected:
    const SRGBVertexInputParams m_params;
};

class SRGBVertexInputCase : public vkt::TestCase
{
public:
    SRGBVertexInputCase(tcu::TestContext &testCtx, const std::string &name, const SRGBVertexInputParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~SRGBVertexInputCase(void) = default;

    std::string getRequiredCapabilitiesId() const override
    {
        return typeid(SRGBVertexInputCase).name() + std::string(m_params.strict ? "-Strict" : "-NonStrict");
    }

    void initDeviceCapabilities(DevCaps &caps) override;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override
    {
        return new SRGBVertexInputInstance(context, m_params);
    }

protected:
    const SRGBVertexInputParams m_params;
};

void SRGBVertexInputCase::initDeviceCapabilities(DevCaps &caps)
{
    // Main extensions that need to be added for some of these tests if supported.
    caps.addExtension("VK_EXT_shader_object");
    caps.addExtension("VK_EXT_graphics_pipeline_library");

    // Extension dependencies from the main ones above.
    caps.addExtension("VK_KHR_pipeline_library");
    caps.addExtension("VK_KHR_dynamic_rendering");
    caps.addExtension("VK_KHR_depth_stencil_resolve");
    caps.addExtension("VK_KHR_create_renderpass2");
    caps.addExtension("VK_KHR_multiview");
    caps.addExtension("VK_KHR_maintenance2");
    caps.addExtension("VK_KHR_maintenance10");

#ifndef CTS_USES_VULKANSC
    // Features actually used for these tests.
    caps.addFeature(&VkPhysicalDeviceShaderObjectFeaturesEXT::shaderObject);
    caps.addFeature(&VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT::graphicsPipelineLibrary);
    caps.addFeature(&VkPhysicalDeviceDynamicRenderingFeatures::dynamicRendering);
    if (m_params.strict)
        caps.addFeature(&VkPhysicalDeviceMaintenance10FeaturesKHR::maintenance10);
#endif // CTS_USES_VULKANSC
}

void SRGBVertexInputCase::checkSupport(Context &context) const
{
    const auto ctx = context.getContextCommonData();
    checkPipelineConstructionRequirements(ctx.vki, ctx.physicalDevice, m_params.constructionType);

    if (m_params.strict)
        context.requireDeviceFunctionality("VK_KHR_maintenance10");

    const auto checkedFormat = m_params.format;
    VkFormatProperties fmtProps;
    ctx.vki.getPhysicalDeviceFormatProperties(ctx.physicalDevice, checkedFormat, &fmtProps);

    if (!(fmtProps.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT))
        TCU_THROW(NotSupportedError, "Format not supported for vertex buffers");
}

void SRGBVertexInputCase::initPrograms(vk::SourceCollections &programCollection) const
{
    // Each test is only going to check that a given component is properly converted from nonlinear to linear space. We
    // will expect that component to have value 0 or ~0.5, and will use it to create a quad that draws over the top half
    // of the framebuffer. If the component is not properly linearized, coverage will be greater because the value will
    // be larger than 0.5.
    const char *componentNames = "xyzw";
    DE_ASSERT(m_params.component < 4);
    const auto &compName = componentNames[m_params.component];

    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inCoords;\n"
         << "// These XY coords below are normalized to 0..1 and will be transformed to -1..1 later\n"
         << "// Value 10.0 in this array will be replaced with something that's expected to alternate\n"
         << "// between 0 and 0.5 in the vertex buffer to form a quad that covers the top half\n"
         << "vec4 vertices[4] = vec4[](\n"
         << "    vec4(0.0, 10.0, 0.0, 1.0),\n"
         << "    vec4(0.0, 10.0, 0.0, 1.0),\n"
         << "    vec4(1.0, 10.0, 0.0, 1.0),\n"
         << "    vec4(1.0, 10.0, 0.0, 1.0)\n"
         << ");\n"
         << "void main(void) {\n"
         << "    vec4 position = vertices[gl_VertexIndex % 4];\n"
         << "    position.y = inCoords." << compName << ";\n"
         << "    position = position * vec4(2.0, 2.0, 1.0, 1.0) - vec4(1.0, 1.0, 0.0, 0.0); // XY from 0..1 to -1..1\n"
         << "    gl_Position = position;\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main(void) { outColor = vec4(0.0, 0.0, 1.0, 1.0); }\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

// componentValues will have the values that we want for the component we're interested in. E.g. 0.0, 0.5, 0.0, 0.5.
// (those are the values that will replace 10.0 in the vertex shader)
// We need to convert them to an array of values that's suitable to be used as the vertex buffer. We need to check
// how many components the format has, which component we want to test and the component order from the format.
std::vector<uint8_t> prepareVertexBufferContents(VkFormat format, uint32_t testedComponent,
                                                 const std::vector<float> &componentValues)
{
    std::vector<uint8_t> contents;

    const auto paddingByte          = uint8_t{255};
    const auto tcuFormat            = mapVkFormat(format);
    const auto formatComponentCount = static_cast<uint32_t>(tcu::getNumUsedChannels(tcuFormat.order));
    const auto valueCount           = formatComponentCount * de::sizeU32(componentValues);
    contents.reserve(valueCount);

    const auto floatToU8 = [](float v) { return static_cast<uint8_t>(v * 255.0f); };

    DE_ASSERT(testedComponent < formatComponentCount);
    uint32_t componentIndex = testedComponent;

    switch (format)
    {
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_R8G8B8A8_SRGB:
        break;
    case VK_FORMAT_B8G8R8_SRGB:
    case VK_FORMAT_B8G8R8A8_SRGB:
    {
        // Reverse RGB component order.
        if (testedComponent < 3)
            componentIndex = 2 - testedComponent;
    }
    break;
    default:
        DE_ASSERT(false);
        break;
    }

    // Add each value to the vector with padding bytes for the unused components.
    // e.g. format==VK_FORMAT_B8G8R8A8_SRGB and testedComponent==0 (Red), we push (255, 255, componentValueU8, 255)
    // That's suitable for the VK_FORMAT_B8G8R8A8_SRGB vertex attribute and the shader will use inCoords.x.
    for (const auto &componentValue : componentValues)
    {
        const auto u8Value = floatToU8(componentValue);

        for (uint32_t i = 0; i < componentIndex; ++i)
            contents.push_back(paddingByte);
        contents.push_back(u8Value);
        for (uint32_t i = componentIndex + 1; i < formatComponentCount; ++i)
            contents.push_back(paddingByte);
    }

    return contents;
}

bool SRGBVertexInputInstance::runWithCoords(const std::vector<float> &coords, uint32_t runId, int expectedCoveredRows)
{
    const auto ctx            = m_context.getContextCommonData();
    const auto tcuFormat      = mapVkFormat(m_params.format);
    const auto vertBufferData = prepareVertexBufferContents(m_params.format, m_params.component, coords);

    // Vertex buffer.
    const auto vertBufferSize  = static_cast<VkDeviceSize>(de::dataSize(vertBufferData));
    const auto vertBufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    const auto vertBufferInfo  = makeBufferCreateInfo(vertBufferSize, vertBufferUsage);
    BufferWithMemory vertBuffer(ctx.vkd, ctx.device, ctx.allocator, vertBufferInfo, HostIntent::W);
    {
        auto &alloc = vertBuffer.getAllocation();
        memcpy(alloc.getHostPtr(), de::dataOrNull(vertBufferData), de::dataSize(vertBufferData));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    // Framebuffer: it can't be 2x2 or something small because we want to see some detail about extra pixels colored if
    // linearization does not happen. At the same time, we don't want something very large or we could hit precission issues.
    const auto extent      = m_params.getExtent();
    const auto extentVk    = makeExtent3D(extent);
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage  = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, extentVk, colorFormat, colorUsage,
                                VK_IMAGE_TYPE_2D);

    const auto vertStride = tcu::getNumUsedChannels(tcuFormat.order) * DE_SIZEOF32(uint8_t);

    const std::vector<VkVertexInputBindingDescription> inputBindings{
        makeVertexInputBindingDescription(0u, vertStride, VK_VERTEX_INPUT_RATE_VERTEX),
    };

    const auto attribFormat = m_params.format;
    const std::vector<VkVertexInputAttributeDescription> inputAttributes{
        makeVertexInputAttributeDescription(0u, 0u, attribFormat, 0u),
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(inputBindings),
        de::dataOrNull(inputBindings),
        de::sizeU32(inputAttributes),
        de::dataOrNull(inputAttributes),
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        nullptr,
        0u,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        VK_FALSE,
    };

    const std::vector<VkViewport> viewports(1u, makeViewport(extent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(extent));

    PipelineLayoutWrapper pipelineLayout(m_params.constructionType, ctx.vkd, ctx.device);
    RenderPassWrapper renderPass(m_params.constructionType, ctx.vkd, ctx.device, colorFormat);
    renderPass.createFramebuffer(ctx.vkd, ctx.device, colorBuffer.getImage(), colorBuffer.getImageView(),
                                 extentVk.width, extentVk.height);

    const auto &binaries = m_context.getBinaryCollection();
    ShaderWrapper vertShader(ctx.vkd, ctx.device, binaries.get("vert"));
    ShaderWrapper fragShader(ctx.vkd, ctx.device, binaries.get("frag"));

    GraphicsPipelineWrapper pipeline(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device, m_context.getDeviceExtensions(),
                                     m_params.constructionType);
    pipeline.setDefaultRasterizationState()
        .setDefaultColorBlendState()
        .setDefaultDepthStencilState()
        .setDefaultMultisampleState()
        .setupVertexInputState(&vertexInputStateCreateInfo, &inputAssemblyStateCreateInfo)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass.get(), 0u, vertShader)
        .setupFragmentShaderState(pipelineLayout, renderPass.get(), 0u, fragShader)
        .setupFragmentOutputState(renderPass.get(), 0u)
        .buildPipeline();

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f); // Must be different from the frag shader color.
    const tcu::Vec4 geomColor(0.0f, 0.0f, 1.0f, 1.0f);  // Must match frag shader color.
    const VkDeviceSize vertBufferOffset = 0ull;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u), clearColor);
    pipeline.bind(cmdBuffer);
    ctx.vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertBuffer.get(), &vertBufferOffset);
    ctx.vkd.cmdDraw(cmdBuffer, de::sizeU32(coords), 1u, 0u, 0u);
    renderPass.end(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), extent.swizzle(0, 1));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Check that the top half is filled and the rest is not.
    auto &colorBufferAlloc = colorBuffer.getBufferAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, colorBufferAlloc);

    const auto tcuColorFormat = mapVkFormat(colorFormat);
    tcu::ConstPixelBufferAccess result(tcuColorFormat, extent, colorBufferAlloc.getHostPtr());

    tcu::TextureLevel refLevel(tcuColorFormat, extent.x(), extent.y(), extent.z());
    tcu::PixelBufferAccess reference = refLevel.getAccess();
    tcu::clear(reference, clearColor);
    const auto topHalf = tcu::getSubregion(reference, 0, 0, extent.x(), expectedCoveredRows);
    tcu::clear(topHalf, geomColor);

    auto &log             = m_context.getTestContext().getLog();
    const auto resultName = "Result" + std::to_string(runId);
    const tcu::Vec4 threshold(0.0f);

    return tcu::floatThresholdCompare(log, resultName.c_str(), "", reference, result, threshold,
                                      tcu::COMPARE_LOG_ON_ERROR);
}

tcu::TestStatus SRGBVertexInputInstance::iterate()
{
    const auto extent = m_params.getExtent();
    const auto seed   = m_params.getRandomSeed();

    de::Random rnd(seed);
    DE_ASSERT(extent.y() > 2);
    const int coveredRows = rnd.getInt(1, extent.y() - 2);
    const float yCoord    = static_cast<float>(coveredRows) / static_cast<float>(extent.y());

    std::vector<float> wantedYCoords{0.0f, yCoord, 0.0f, yCoord};

    bool strictModeSuccess = false;

    // Run 0: expect linearization, so convert to RGB first.
    {
        std::vector<float> usedCoords;
        usedCoords.reserve(wantedYCoords.size());

        // Alpha must not be linearized.
        if (m_params.component < 3)
        {
            for (const auto &coord : wantedYCoords)
                usedCoords.push_back(tcu::linearChannelToSRGB(coord));
        }
        else
            usedCoords = wantedYCoords;

        strictModeSuccess = runWithCoords(usedCoords, 0u, coveredRows);
    }

    if (strictModeSuccess)
        return tcu::TestStatus::pass("Pass");

    // Strict mode did not work.

    if (m_params.strict)
        TCU_FAIL("Vertex coordinates have unexpected values");

    const bool preLinearizedSuccess = runWithCoords(wantedYCoords, 1u, coveredRows);
    if (!preLinearizedSuccess)
        TCU_FAIL("Vertex coordinates have unexpected values");

    return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "sRGB vertex coordinates are not linearized");
}

std::string getFormatShortString(VkFormat format)
{
    std::string formatName = getFormatName(format);
    formatName             = formatName.substr(std::strlen("VK_FORMAT_"));
    return de::toLower(formatName);
}

} // anonymous namespace

tcu::TestCaseGroup *createVertexInputSRGBTests(tcu::TestContext &testCtx,
                                               PipelineConstructionType pipelineConstructionType)
{
    const VkFormat kTestedFormats[] = {
        // clang-format off
        VK_FORMAT_R8_SRGB,
        VK_FORMAT_R8G8_SRGB,
        VK_FORMAT_R8G8B8_SRGB,
        VK_FORMAT_B8G8R8_SRGB,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_SRGB,
        // clang-format on
    };

    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "srgb_vertex_formats"));

    for (const auto format : kTestedFormats)
    {
        const auto grpName = getFormatShortString(format);
        GroupPtr formatGroup(new tcu::TestCaseGroup(testCtx, grpName.c_str()));

        const auto tcuFormat = mapVkFormat(format);
        for (const uint32_t testedComponent : {0u, 1u, 2u, 3u})
        {
            if (testedComponent >= static_cast<uint32_t>(tcu::getNumUsedChannels(tcuFormat.order)))
                continue;

            for (const bool strict : {false, true})
            {
                const SRGBVertexInputParams params{
                    pipelineConstructionType,
                    format,
                    testedComponent,
                    strict,
                };
                const auto testName = "rgba"[testedComponent] + std::string(strict ? "_strict" : "");
                formatGroup->addChild(new SRGBVertexInputCase(testCtx, testName, params));
            }
        }

        mainGroup->addChild(formatGroup.release());
    }

    return mainGroup.release();
}

} // namespace pipeline
} // namespace vkt
