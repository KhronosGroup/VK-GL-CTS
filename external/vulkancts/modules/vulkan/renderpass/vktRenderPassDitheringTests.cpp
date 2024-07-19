/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 Google Inc.
 * Copyright (c) 2022 LunarG, Inc.
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
 * \brief Tests dithering
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassLoadStoreOpNoneTests.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktRenderPassTestsUtil.hpp"
#include "pipeline/vktPipelineImageUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "tcuImageCompare.hpp"
#include "vktTestGroupUtil.hpp"

namespace vkt
{
namespace renderpass
{

using namespace vk;

namespace
{

// ~1 ULP in D24_UNORM (1/2^24 == 0.00000006)
const uint32_t baseDepthValue       = 0b00111110000000000000000000000000; // 0.125f
const uint32_t oneUlpMoreDepthValue = 0b00111110000000000000000000000101; // 0.125000074506f
const uint32_t oneUlpLessDepthValue = 0b00111101111111111111111111110111; // 0.124999932945f

struct TestParams
{
    std::vector<VkViewport> renderAreas;
    std::vector<VkFormat> colorFormats;
    tcu::Vec4 overrideColor;
    tcu::UVec2 imageSize;
    VkFormat depthStencilFormat;
    SharedGroupParams groupParams;
    VkBlendFactor srcFactor;
    VkBlendFactor dstFactor;
    uint32_t stencilClearValue;
    VkCompareOp depthCompareOp;
    float depthClearValue;
    bool blending;
    bool revision2;
};

struct Vertex4RGBA
{
    tcu::Vec4 position;
    tcu::Vec4 color;
};

de::SharedPtr<Move<vk::VkDevice>> g_singletonDevice;

VkDevice getDevice(Context &context)
{
    if (g_singletonDevice)
        return g_singletonDevice->get();

    // Create a universal queue that supports graphics and compute
    const float queuePriority = 1.0f;
    const VkDeviceQueueCreateInfo queueParams{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                    // const void* pNext;
        0u,                                         // VkDeviceQueueCreateFlags flags;
        context.getUniversalQueueFamilyIndex(),     // uint32_t queueFamilyIndex;
        1u,                                         // uint32_t queueCount;
        &queuePriority                              // const float* pQueuePriorities;
    };

    // \note Extensions in core are not explicitly enabled even though
    //         they are in the extension list advertised to tests.
    const auto &extensionPtrs = context.getDeviceCreationExtensions();

    VkPhysicalDeviceLegacyDitheringFeaturesEXT legacyDitheringFeatures = initVulkanStructure();
    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures  = initVulkanStructure();
    VkPhysicalDeviceFeatures2 features2                                = initVulkanStructure();

    const auto addFeatures = makeStructChainAdder(&features2);
    addFeatures(&legacyDitheringFeatures);

    if (context.isDeviceFunctionalitySupported("VK_KHR_dynamic_rendering"))
        addFeatures(&dynamicRenderingFeatures);

    context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);
    features2.features.robustBufferAccess = VK_FALSE;

    const VkDeviceCreateInfo deviceCreateInfo{
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, //sType;
        &features2,                           //pNext;
        0u,                                   //flags
        1,                                    //queueRecordCount;
        &queueParams,                         //pRequestedQueues;
        0u,                                   //layerCount;
        nullptr,                              //ppEnabledLayerNames;
        de::sizeU32(extensionPtrs),           // uint32_t enabledExtensionCount;
        de::dataOrNull(extensionPtrs),        // const char* const* ppEnabledExtensionNames;
        nullptr,                              //pEnabledFeatures;
    };

    Move<VkDevice> device = createCustomDevice(
        context.getTestContext().getCommandLine().isValidationEnabled(), context.getPlatformInterface(),
        context.getInstance(), context.getInstanceInterface(), context.getPhysicalDevice(), &deviceCreateInfo);
    g_singletonDevice = de::SharedPtr<Move<VkDevice>>(new Move<VkDevice>(device));
    return g_singletonDevice->get();
}

std::vector<Vertex4RGBA> createQuad(void)
{
    std::vector<Vertex4RGBA> vertices;

    const float size = 1.0f;
    const tcu::Vec4 red(1.0f, 0.0f, 0.0f, 1.0f);
    const tcu::Vec4 green(0.0f, 1.0f, 0.0f, 1.0f);
    const tcu::Vec4 blue(0.0f, 0.0f, 1.0f, 1.0f);
    const tcu::Vec4 white(1.0f, 1.0f, 1.0f, 1.0f);
    const float *ptr                   = reinterpret_cast<const float *>(&baseDepthValue);
    const float depthValue             = *(ptr);
    const Vertex4RGBA lowerLeftVertex  = {tcu::Vec4(-size, -size, depthValue, 1.0f), red};
    const Vertex4RGBA lowerRightVertex = {tcu::Vec4(size, -size, depthValue, 1.0f), green};
    const Vertex4RGBA upperLeftVertex  = {tcu::Vec4(-size, size, depthValue, 1.0f), blue};
    const Vertex4RGBA upperRightVertex = {tcu::Vec4(size, size, depthValue, 1.0f), white};

    vertices.push_back(lowerLeftVertex);
    vertices.push_back(upperLeftVertex);
    vertices.push_back(lowerRightVertex);
    vertices.push_back(upperLeftVertex);
    vertices.push_back(upperRightVertex);
    vertices.push_back(lowerRightVertex);

    return vertices;
}

std::vector<Vertex4RGBA> createQuad(const tcu::Vec4 &color)
{
    std::vector<Vertex4RGBA> vertices;

    const float size                   = 1.0f;
    const float *ptr                   = reinterpret_cast<const float *>(&baseDepthValue);
    const float depthValue             = *(ptr);
    const Vertex4RGBA lowerLeftVertex  = {tcu::Vec4(-size, -size, depthValue, 1.0f), color};
    const Vertex4RGBA lowerRightVertex = {tcu::Vec4(size, -size, depthValue, 1.0f), color};
    const Vertex4RGBA upperLeftVertex  = {tcu::Vec4(-size, size, depthValue, 1.0f), color};
    const Vertex4RGBA upperRightVertex = {tcu::Vec4(size, size, depthValue, 1.0f), color};

    vertices.push_back(lowerLeftVertex);
    vertices.push_back(upperLeftVertex);
    vertices.push_back(lowerRightVertex);
    vertices.push_back(upperLeftVertex);
    vertices.push_back(upperRightVertex);
    vertices.push_back(lowerRightVertex);

    return vertices;
}

class DitheringTest : public vkt::TestCase
{
public:
    DitheringTest(tcu::TestContext &testContext, const std::string &name, TestParams testParams);
    virtual ~DitheringTest(void);
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual void checkSupport(Context &context) const;
    virtual TestInstance *createInstance(Context &context) const;

private:
    TestParams m_testParams;
};

class DitheringTestInstance : public vkt::TestInstance
{
public:
    DitheringTestInstance(Context &context, TestParams testParams);
    virtual ~DitheringTestInstance(void);
    virtual tcu::TestStatus iterate(void);

private:
    template <typename RenderpassSubpass>
    void render(const VkViewport &vp, bool useDithering);
    void createCommonResources(void);
    void createDrawResources(bool useDithering);

    template <typename AttachmentDescription, typename AttachmentReference, typename SubpassDescription,
              typename RenderPassCreateInfo>
    void createRenderPassFramebuffer(bool useDithering);

    // Data.
private:
    TestParams m_testParams;

    // Common resources.
    SimpleAllocator m_memAlloc;
    Move<VkBuffer> m_vertexBuffer;
    de::MovePtr<Allocation> m_vertexBufferAlloc;
    Move<VkPipelineLayout> m_pipelineLayout;
    Move<VkShaderModule> m_vertexShaderModule;
    Move<VkShaderModule> m_fragmentShaderModule;

    struct DrawResources
    {
        std::vector<Move<VkImage>> attachmentImages;
        std::vector<de::MovePtr<Allocation>> attachmentImageAllocs;
        std::vector<Move<VkImageView>> imageViews;
        Move<VkImage> depthStencilImage;
        de::MovePtr<Allocation> depthStencilImageAlloc;
        Move<VkImageView> depthStencilImageView;
        Move<VkRenderPass> renderPass;
        Move<VkFramebuffer> framebuffer;
        Move<VkPipeline> pipeline;
    };
    const uint32_t m_noDitheringNdx = 0u;
    const uint32_t m_ditheringNdx   = 1u;

    // 0 for no dithering and 1 for dithering resources.
    DrawResources m_drawResources[2];
};

DitheringTest::DitheringTest(tcu::TestContext &testContext, const std::string &name, TestParams testParams)
    : vkt::TestCase(testContext, name)
    , m_testParams(testParams)
{
}

DitheringTest::~DitheringTest(void)
{
}

void DitheringTest::initPrograms(SourceCollections &sourceCollections) const
{
    sourceCollections.glslSources.add("color_vert")
        << glu::VertexSource("#version 450\n"
                             "layout(location = 0) in highp vec4 position;\n"
                             "layout(location = 1) in highp vec4 color;\n"
                             "layout(location = 0) out highp vec4 vtxColor;\n"
                             "void main (void)\n"
                             "{\n"
                             "    gl_Position = position;\n"
                             "    vtxColor = color;\n"
                             "}\n");

    sourceCollections.glslSources.add("color_frag")
        << glu::FragmentSource("#version 450\n"
                               "layout(location = 0) in highp vec4 vtxColor;\n"
                               "layout(location = 0) out highp vec4 fragColor0;\n"
                               "layout(location = 1) out highp vec4 fragColor1;\n"
                               "layout(location = 2) out highp vec4 fragColor2;\n"
                               "void main (void)\n"
                               "{\n"
                               "    fragColor0 = vtxColor;\n"
                               "    fragColor1 = vtxColor;\n"
                               "    fragColor2 = vtxColor;\n"
                               "}\n");
}

void DitheringTest::checkSupport(Context &ctx) const
{
    // Check for renderpass2 extension if used.
    if (m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
        ctx.requireDeviceFunctionality("VK_KHR_create_renderpass2");

    // Check for dynamic_rendering extension if used
    if (m_testParams.groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
        ctx.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

    ctx.requireDeviceFunctionality("VK_EXT_legacy_dithering");

    uint32_t specVersion = 0;
    const auto extensionProperties =
        vk::enumerateDeviceExtensionProperties(ctx.getInstanceInterface(), ctx.getPhysicalDevice(), DE_NULL);
    for (const auto &extProp : extensionProperties)
    {
        if (strcmp(extProp.extensionName, "VK_EXT_legacy_dithering") == 0)
        {
            specVersion = extProp.specVersion;
            break;
        }
    }

    if (m_testParams.revision2)
    {
        ctx.requireDeviceFunctionality("VK_KHR_maintenance5");

        if (specVersion < 2)
            TCU_THROW(NotSupportedError, "VK_EXT_legacy_dithering specVersion at least 2 is required");
    }
    else
    {
        if (specVersion > 1)
            TCU_THROW(NotSupportedError, "VK_EXT_legacy_dithering specVersion at 1 is required");
    }

    // Check color format support.
    for (const VkFormat format : m_testParams.colorFormats)
    {
        VkImageUsageFlags usage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        const auto &vki    = ctx.getInstanceInterface();
        const auto physDev = ctx.getPhysicalDevice();
        const auto imgType = VK_IMAGE_TYPE_2D;
        const auto tiling  = VK_IMAGE_TILING_OPTIMAL;
        VkImageFormatProperties properties;

        const auto result =
            vki.getPhysicalDeviceImageFormatProperties(physDev, format, imgType, tiling, usage, 0u, &properties);

        if (result != VK_SUCCESS)
            TCU_THROW(NotSupportedError, "Color format not supported");
    }

    // Check depth stencil format support.
    if (m_testParams.depthStencilFormat != VK_FORMAT_UNDEFINED)
    {
        VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        const auto &vki    = ctx.getInstanceInterface();
        const auto physDev = ctx.getPhysicalDevice();
        const auto imgType = VK_IMAGE_TYPE_2D;
        const auto tiling  = VK_IMAGE_TILING_OPTIMAL;
        VkImageFormatProperties properties;

        const auto result = vki.getPhysicalDeviceImageFormatProperties(physDev, m_testParams.depthStencilFormat,
                                                                       imgType, tiling, usage, 0u, &properties);

        if (result != VK_SUCCESS)
            TCU_THROW(NotSupportedError, "Depth/stencil format not supported");
    }
}

TestInstance *DitheringTest::createInstance(Context &context) const
{
    return new DitheringTestInstance(context, m_testParams);
}

DitheringTestInstance::DitheringTestInstance(Context &context, TestParams testParams)
    : vkt::TestInstance(context)
    , m_testParams(testParams)
    , m_memAlloc(context.getDeviceInterface(), getDevice(context),
                 getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()))
{
    createCommonResources();
    createDrawResources(false); // No dithering
    createDrawResources(true);  // Dithering
}

DitheringTestInstance::~DitheringTestInstance(void)
{
}

tcu::TestStatus DitheringTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice vkDevice         = getDevice(m_context);
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const VkQueue queue             = getDeviceQueue(vk, vkDevice, queueFamilyIndex, 0);

    for (const VkViewport &vp : m_testParams.renderAreas)
    {
        if (m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
        {
            render<RenderpassSubpass1>(vp, false);
            render<RenderpassSubpass1>(vp, true);
        }
        else
        {
            render<RenderpassSubpass2>(vp, false);
            render<RenderpassSubpass2>(vp, true);
        }

        // Check output matches to expected within one ULP.
        for (uint32_t i = 0u; i < m_testParams.colorFormats.size(); ++i)
        {
            VkFormat format      = m_testParams.colorFormats[i];
            VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

            // No dithering
            SimpleAllocator imageAllocator(
                vk, vkDevice,
                getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
            de::MovePtr<tcu::TextureLevel> referenceTextureLevelResult = pipeline::readColorAttachment(
                vk, vkDevice, queue, queueFamilyIndex, imageAllocator,
                *m_drawResources[m_noDitheringNdx].attachmentImages[i], format, m_testParams.imageSize, layout);
            const tcu::ConstPixelBufferAccess &referenceAccess = referenceTextureLevelResult->getAccess();

            // Dithering
            de::MovePtr<tcu::TextureLevel> resultTextureLevelResult = pipeline::readColorAttachment(
                vk, vkDevice, queue, queueFamilyIndex, imageAllocator,
                *m_drawResources[m_ditheringNdx].attachmentImages[i], format, m_testParams.imageSize, layout);
            const tcu::ConstPixelBufferAccess &resultAccess = resultTextureLevelResult->getAccess();

            // 1 ULP will always be 1 bit difference no matter the format
            // However, we allow N ULP for additive blending tests since drivers may do dithering while rendering (per draw)
            // which can cause dither pattern to exceed 1ULP threshold with additive blending, see discussion in:
            // https://gitlab.khronos.org/Tracker/vk-gl-cts/-/issues/3785#note_384389
            const uint32_t n_ulp = (m_testParams.blending && m_testParams.dstFactor == VK_BLEND_FACTOR_ONE) ? 4u : 1u;
            const tcu::UVec4 threshold(n_ulp, n_ulp, n_ulp, n_ulp);

            if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "", "", referenceAccess, resultAccess,
                                          threshold, tcu::COMPARE_LOG_ON_ERROR))
                return tcu::TestStatus::fail("Fail");
        }

        // Check depth/stencil
        if (m_testParams.depthStencilFormat != VK_FORMAT_UNDEFINED)
        {
            VkFormat format      = m_testParams.depthStencilFormat;
            VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

            // Depth check.
            {
                // No dithering
                SimpleAllocator imageAllocator(
                    vk, vkDevice,
                    getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
                de::MovePtr<tcu::TextureLevel> referenceTextureLevelResult = pipeline::readDepthAttachment(
                    vk, vkDevice, queue, queueFamilyIndex, imageAllocator,
                    *m_drawResources[m_noDitheringNdx].depthStencilImage, format, m_testParams.imageSize, layout);
                const tcu::ConstPixelBufferAccess &referenceAccess = referenceTextureLevelResult->getAccess();

                // Dithering
                de::MovePtr<tcu::TextureLevel> resultTextureLevelResult = pipeline::readDepthAttachment(
                    vk, vkDevice, queue, queueFamilyIndex, imageAllocator,
                    *m_drawResources[m_ditheringNdx].depthStencilImage, format, m_testParams.imageSize, layout);
                const tcu::ConstPixelBufferAccess &resultAccess = resultTextureLevelResult->getAccess();

                // Depth should be unaffected by dithering
                const float threshold = 0.0f;

                if (!tcu::dsThresholdCompare(m_context.getTestContext().getLog(), "", "", referenceAccess, resultAccess,
                                             threshold, tcu::COMPARE_LOG_ON_ERROR))
                    return tcu::TestStatus::fail("Fail");
            }

            // Stencil check.
            {
                // No dithering
                SimpleAllocator imageAllocator(
                    vk, vkDevice,
                    getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
                de::MovePtr<tcu::TextureLevel> referenceTextureLevelResult = pipeline::readStencilAttachment(
                    vk, vkDevice, queue, queueFamilyIndex, imageAllocator,
                    *m_drawResources[m_noDitheringNdx].depthStencilImage, format, m_testParams.imageSize, layout);
                const tcu::ConstPixelBufferAccess &referenceAccess = referenceTextureLevelResult->getAccess();

                // Dithering
                de::MovePtr<tcu::TextureLevel> resultTextureLevelResult = pipeline::readStencilAttachment(
                    vk, vkDevice, queue, queueFamilyIndex, imageAllocator,
                    *m_drawResources[m_ditheringNdx].depthStencilImage, format, m_testParams.imageSize, layout);
                const tcu::ConstPixelBufferAccess &resultAccess = resultTextureLevelResult->getAccess();

                // Stencil should be unaffected by dithering
                const float threshold = 0.0f;

                if (!tcu::dsThresholdCompare(m_context.getTestContext().getLog(), "", "", referenceAccess, resultAccess,
                                             threshold, tcu::COMPARE_LOG_ON_ERROR))
                    return tcu::TestStatus::fail("Fail");
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

template <typename RenderpassSubpass>
void DitheringTestInstance::render(const VkViewport &vp, bool useDithering)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice vkDevice         = getDevice(m_context);
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const VkQueue queue             = getDeviceQueue(vk, vkDevice, queueFamilyIndex, 0);

    uint32_t resourceNdx       = useDithering ? m_ditheringNdx : m_noDitheringNdx;
    const tcu::UVec2 imageSize = m_testParams.imageSize;
    const bool useDepthStencil = (m_testParams.depthStencilFormat != VK_FORMAT_UNDEFINED);

    // Clear color and transition image to desired layout.
    {
        const auto dstAccess = (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        const auto dstStage  = (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        const auto layout    = (m_testParams.groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING) ?
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL :
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        auto clearColor      = makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f).color;

        if (m_testParams.blending)
            clearColor = makeClearValueColorF32(0.0f, 1.0f, 0.0f, 1.0f).color;

        for (const auto &image : m_drawResources[resourceNdx].attachmentImages)
            clearColorImage(vk, vkDevice, queue, queueFamilyIndex, *image, clearColor,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, layout, dstAccess, dstStage);
    }

    // Clear depth/stencil.
    if (useDepthStencil)
    {
        const auto dstAccess =
            (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        const auto dstStage = (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
        const auto layout   = m_testParams.groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING ?
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL :
                                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        clearDepthStencilImage(vk, vkDevice, queue, queueFamilyIndex, *m_drawResources[resourceNdx].depthStencilImage,
                               m_testParams.depthStencilFormat, m_testParams.depthClearValue,
                               m_testParams.stencilClearValue, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, layout, dstAccess,
                               dstStage);
    }

    // Rendering.
    {
        // Create command pool and allocate command buffer.
        Move<VkCommandPool> cmdPool =
            createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
        Move<VkCommandBuffer> cmdBuffer =
            allocateCommandBuffer(vk, vkDevice, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        const typename RenderpassSubpass::SubpassBeginInfo subpassBeginInfo(DE_NULL, VK_SUBPASS_CONTENTS_INLINE);
        const typename RenderpassSubpass::SubpassEndInfo subpassEndInfo(DE_NULL);
        const VkDeviceSize vertexBufferOffset = 0;
        const uint32_t drawCount = (m_testParams.blending && m_testParams.dstFactor == VK_BLEND_FACTOR_ONE) ? 4u : 1u;

        beginCommandBuffer(vk, *cmdBuffer, 0u);

        if (m_testParams.groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
        {
            std::vector<VkRenderingAttachmentInfoKHR> colorAttachments;

            for (const auto &imageView : m_drawResources[resourceNdx].imageViews)
            {
                VkRenderingAttachmentInfoKHR attachment = {
                    VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,       // VkStructureType sType;
                    DE_NULL,                                               // const void* pNext;
                    *imageView,                                            // VkImageView imageView;
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,              // VkImageLayout imageLayout;
                    VK_RESOLVE_MODE_NONE,                                  // VkResolveModeFlagBits resolveMode;
                    VK_NULL_HANDLE,                                        // VkImageView resolveImageView;
                    VK_IMAGE_LAYOUT_UNDEFINED,                             // VkImageLayout resolveImageLayout;
                    VK_ATTACHMENT_LOAD_OP_LOAD,                            // VkAttachmentLoadOp loadOp;
                    VK_ATTACHMENT_STORE_OP_STORE,                          // VkAttachmentStoreOp storeOp;
                    makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f)) // VkClearValue clearValue;
                };

                colorAttachments.emplace_back(attachment);
            }

            VkRenderingAttachmentInfoKHR dsAttachment = {
                VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,     // VkStructureType sType;
                DE_NULL,                                             // const void* pNext;
                *m_drawResources[resourceNdx].depthStencilImageView, // VkImageView imageView;
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,    // VkImageLayout imageLayout;
                VK_RESOLVE_MODE_NONE,                                // VkResolveModeFlagBits resolveMode;
                VK_NULL_HANDLE,                                      // VkImageView resolveImageView;
                VK_IMAGE_LAYOUT_UNDEFINED,                           // VkImageLayout resolveImageLayout;
                VK_ATTACHMENT_LOAD_OP_LOAD,                          // VkAttachmentLoadOp loadOp;
                VK_ATTACHMENT_STORE_OP_STORE,                        // VkAttachmentStoreOp storeOp;
                makeClearValueDepthStencil(m_testParams.depthClearValue,
                                           m_testParams.stencilClearValue) // VkClearValue clearValue;
            };

            VkRenderingFlags renderingInfoFlags = 0u;
            if (useDithering)
                renderingInfoFlags = VK_RENDERING_ENABLE_LEGACY_DITHERING_BIT_EXT;
            VkRenderingInfoKHR renderingInfo = {
                VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,      // VkStructureType sType;
                DE_NULL,                                   // const void* pNext;
                renderingInfoFlags,                        // VkRenderingFlagsKHR flags;
                makeRect2D(imageSize),                     // VkRect2D renderArea;
                1u,                                        // uint32_t layerCount;
                0u,                                        // uint32_t viewMask;
                (uint32_t)colorAttachments.size(),         // uint32_t colorAttachmentCount;
                colorAttachments.data(),                   // const VkRenderingAttachmentInfoKHR* pColorAttachments;
                useDepthStencil ? &dsAttachment : DE_NULL, // const VkRenderingAttachmentInfoKHR* pDepthAttachment;
                useDepthStencil ? &dsAttachment : DE_NULL  // const VkRenderingAttachmentInfoKHR* pStencilAttachment;
            };

            vk.cmdBeginRendering(*cmdBuffer, &renderingInfo);
        }
        else
        {
            const VkRenderPassBeginInfo renderPassBeginInfo = {
                VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,  // VkStructureType        sType
                DE_NULL,                                   // const void*            pNext
                *m_drawResources[resourceNdx].renderPass,  // VkRenderPass            renderPass
                *m_drawResources[resourceNdx].framebuffer, // VkFramebuffer        framebuffer
                makeRect2D(imageSize),                     // VkRect2D                renderArea
                0u,                                        // uint32_t                clearValueCount
                DE_NULL                                    // const VkClearValue*    pClearValues
            };
            RenderpassSubpass::cmdBeginRenderPass(vk, *cmdBuffer, &renderPassBeginInfo, &subpassBeginInfo);
        }

        vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_drawResources[resourceNdx].pipeline);
        vk.cmdSetViewport(*cmdBuffer, 0u, 1u, &vp);
        for (uint32_t i = 0u; i < drawCount; ++i)
            vk.cmdDraw(*cmdBuffer, 6u, 1, 0, 0);

        if (m_testParams.groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
            vk.cmdEndRendering(*cmdBuffer);
        else
            RenderpassSubpass::cmdEndRenderPass(vk, *cmdBuffer, &subpassEndInfo);
        endCommandBuffer(vk, *cmdBuffer);

        // Submit commands.
        submitCommandsAndWait(vk, vkDevice, queue, cmdBuffer.get());
    }
}

void DitheringTestInstance::createCommonResources(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice vkDevice         = getDevice(m_context);
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    // Shaders.
    m_vertexShaderModule   = createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("color_vert"), 0);
    m_fragmentShaderModule = createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("color_frag"), 0);

    // Vertex buffer.
    {
        const std::vector<Vertex4RGBA> vertices =
            m_testParams.blending ? createQuad(m_testParams.overrideColor) : createQuad();
        const VkBufferCreateInfo vertexBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,                  // VkStructureType        sType
            DE_NULL,                                               // const void*            pNext
            0u,                                                    // VkBufferCreateFlags    flags
            (VkDeviceSize)(sizeof(Vertex4RGBA) * vertices.size()), // VkDeviceSize            size
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,                     // VkBufferUsageFlags    usage
            VK_SHARING_MODE_EXCLUSIVE,                             // VkSharingMode        sharingMode
            1u,                                                    // uint32_t                queueFamilyIndexCount
            &queueFamilyIndex                                      // const uint32_t*        pQueueFamilyIndices
        };

        m_vertexBuffer      = createBuffer(vk, vkDevice, &vertexBufferParams);
        m_vertexBufferAlloc = m_memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer),
                                                  MemoryRequirement::HostVisible);

        VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(),
                                     m_vertexBufferAlloc->getOffset()));

        // Upload vertex data.
        deMemcpy(m_vertexBufferAlloc->getHostPtr(), vertices.data(), vertices.size() * sizeof(Vertex4RGBA));
        flushAlloc(vk, vkDevice, *m_vertexBufferAlloc);
    }

    // Create pipeline layout.
    {
        const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType                sType
            DE_NULL,                                       // const void*                    pNext
            0u,                                            // VkPipelineLayoutCreateFlags    flags
            0u,                                            // uint32_t                        setLayoutCount
            DE_NULL,                                       // const VkDescriptorSetLayout*    pSetLayouts
            0u,                                            // uint32_t                        pushConstantRangeCount
            DE_NULL                                        // const VkPushConstantRange*    pPushConstantRanges
        };

        m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
    }
}

void DitheringTestInstance::createDrawResources(bool useDithering)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice vkDevice         = getDevice(m_context);
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const VkQueue queue             = getDeviceQueue(vk, vkDevice, queueFamilyIndex, 0);

    uint32_t resourceNdx                              = useDithering ? m_ditheringNdx : m_noDitheringNdx;
    const std::vector<vk::VkFormat> colorFormats      = m_testParams.colorFormats;
    const tcu::UVec2 &imageSize                       = m_testParams.imageSize;
    const VkComponentMapping componentMappingIdentity = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                                         VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};

    // Attachment images and views.
    for (const VkFormat format : colorFormats)
    {
        VkImageUsageFlags usage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VkImageAspectFlags aspectFlags          = VK_IMAGE_ASPECT_COLOR_BIT;
        const VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
        const VkImageCreateInfo imageParams     = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType            sType
            DE_NULL,                             // const void*                pNext
            0u,                                  // VkImageCreateFlags        flags
            VK_IMAGE_TYPE_2D,                    // VkImageType                imageType
            format,                              // VkFormat                    format
            {imageSize.x(), imageSize.y(), 1u},  // VkExtent3D                extent
            1u,                                  // uint32_t                    mipLevels
            1u,                                  // uint32_t                    arrayLayers
            sampleCount,                         // VkSampleCountFlagBits    samples
            VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling            tiling
            usage,                               // VkImageUsageFlags        usage
            VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode            sharingMode
            1u,                                  // uint32_t                    queueFamilyIndexCount
            &queueFamilyIndex,                   // const uint32_t*            pQueueFamilyIndices
            VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout            initialLayout
        };
        Move<VkImage> image                     = createImage(vk, vkDevice, &imageParams);
        VkMemoryRequirements memoryRequirements = getImageMemoryRequirements(vk, vkDevice, *image);
        de::MovePtr<Allocation> imageAlloc      = m_memAlloc.allocate(memoryRequirements, MemoryRequirement::Any);
        VK_CHECK(vk.bindImageMemory(vkDevice, *image, imageAlloc->getMemory(), imageAlloc->getOffset()));

        // Create image view.
        const VkImageViewCreateInfo imageViewParams = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType            sType
            DE_NULL,                                  // const void*                pNext
            0u,                                       // VkImageViewCreateFlags    flags
            *image,                                   // VkImage                    image
            VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType            viewType
            format,                                   // VkFormat                    format
            componentMappingIdentity,                 // VkChannelMapping            channels
            {aspectFlags, 0u, 1u, 0u, 1u}             // VkImageSubresourceRange    subresourceRange
        };
        Move<VkImageView> imageView = createImageView(vk, vkDevice, &imageViewParams);

        // Clear and transition image to desired layout for easier looping later when rendering.
        {
            const auto dstAccess = (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            const auto dstStage =
                (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            const auto clearColor = makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f).color;
            const auto layout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

            clearColorImage(vk, vkDevice, queue, queueFamilyIndex, *image, clearColor, VK_IMAGE_LAYOUT_UNDEFINED,
                            layout, dstAccess, dstStage);
        }

        // Store resources.
        m_drawResources[resourceNdx].attachmentImages.emplace_back(image);
        m_drawResources[resourceNdx].attachmentImageAllocs.emplace_back(imageAlloc);
        m_drawResources[resourceNdx].imageViews.emplace_back(imageView);
    }

    // Depth stencil image and view.
    if (m_testParams.depthStencilFormat != VK_FORMAT_UNDEFINED)
    {
        VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VkImageAspectFlags aspectFlags          = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        const VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
        const VkImageCreateInfo imageParams     = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType            sType
            DE_NULL,                             // const void*                pNext
            0u,                                  // VkImageCreateFlags        flags
            VK_IMAGE_TYPE_2D,                    // VkImageType                imageType
            m_testParams.depthStencilFormat,     // VkFormat                    format
            {imageSize.x(), imageSize.y(), 1u},  // VkExtent3D                extent
            1u,                                  // uint32_t                    mipLevels
            1u,                                  // uint32_t                    arrayLayers
            sampleCount,                         // VkSampleCountFlagBits    samples
            VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling            tiling
            usage,                               // VkImageUsageFlags        usage
            VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode            sharingMode
            1u,                                  // uint32_t                    queueFamilyIndexCount
            &queueFamilyIndex,                   // const uint32_t*            pQueueFamilyIndices
            VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout            initialLayout
        };
        m_drawResources[resourceNdx].depthStencilImage      = createImage(vk, vkDevice, &imageParams);
        m_drawResources[resourceNdx].depthStencilImageAlloc = m_memAlloc.allocate(
            getImageMemoryRequirements(vk, vkDevice, *m_drawResources[resourceNdx].depthStencilImage),
            MemoryRequirement::Any);
        VK_CHECK(vk.bindImageMemory(vkDevice, *m_drawResources[resourceNdx].depthStencilImage,
                                    m_drawResources[resourceNdx].depthStencilImageAlloc->getMemory(),
                                    m_drawResources[resourceNdx].depthStencilImageAlloc->getOffset()));

        // Create image view.
        const VkImageViewCreateInfo imageViewParams = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,        // VkStructureType            sType
            DE_NULL,                                         // const void*                pNext
            0u,                                              // VkImageViewCreateFlags    flags
            *m_drawResources[resourceNdx].depthStencilImage, // VkImage                    image
            VK_IMAGE_VIEW_TYPE_2D,                           // VkImageViewType            viewType
            m_testParams.depthStencilFormat,                 // VkFormat                    format
            componentMappingIdentity,                        // VkChannelMapping            channels
            {aspectFlags, 0u, 1u, 0u, 1u}                    // VkImageSubresourceRange    subresourceRange
        };
        m_drawResources[resourceNdx].depthStencilImageView = createImageView(vk, vkDevice, &imageViewParams);

        // Clear and transition image to desired layout for easier looping later when rendering.
        {
            const auto dstAccess =
                (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
            const auto dstStage =
                (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
            const auto layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

            clearDepthStencilImage(vk, vkDevice, queue, queueFamilyIndex,
                                   *m_drawResources[resourceNdx].depthStencilImage, m_testParams.depthStencilFormat,
                                   m_testParams.depthClearValue, m_testParams.stencilClearValue,
                                   VK_IMAGE_LAYOUT_UNDEFINED, layout, dstAccess, dstStage);
        }
    }

    if (m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
        createRenderPassFramebuffer<AttachmentDescription1, AttachmentReference1, SubpassDescription1,
                                    RenderPassCreateInfo1>(useDithering);
    else if (m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
        createRenderPassFramebuffer<AttachmentDescription2, AttachmentReference2, SubpassDescription2,
                                    RenderPassCreateInfo2>(useDithering);

    // Pipeline.
    {
        const VkVertexInputBindingDescription vertexInputBindingDescription = {
            0u,                            // uint32_t                    binding
            (uint32_t)sizeof(Vertex4RGBA), // uint32_t                    strideInBytes
            VK_VERTEX_INPUT_RATE_VERTEX    // VkVertexInputStepRate    inputRate
        };

        const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] = {
            {
                0u,                            // uint32_t    location
                0u,                            // uint32_t    binding
                VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat    format
                0u                             // uint32_t    offset
            },
            {
                1u,                            // uint32_t    location
                0u,                            // uint32_t    binding
                VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat    format
                (uint32_t)(sizeof(float) * 4), // uint32_t    offset
            }};

        const VkPipelineVertexInputStateCreateInfo vertexInputStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                            sType
            DE_NULL,                        // const void*                                pNext
            0u,                             // VkPipelineVertexInputStateCreateFlags    flags
            1u,                             // uint32_t                                    vertexBindingDescriptionCount
            &vertexInputBindingDescription, // const VkVertexInputBindingDescription*    pVertexBindingDescriptions
            2u, // uint32_t                                    vertexAttributeDescriptionCount
            vertexInputAttributeDescriptions // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
        };

        const VkColorComponentFlags writeMask = VK_COLOR_COMPONENT_R_BIT // VkColorComponentFlags    colorWriteMask
                                                | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                                                VK_COLOR_COMPONENT_A_BIT;

        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;
        for (uint32_t i = 0u; i < colorFormats.size(); ++i)
        {
            const VkPipelineColorBlendAttachmentState blendState = {
                m_testParams.blending ? VK_TRUE : VK_FALSE, // VkBool32                    blendEnable
                m_testParams.srcFactor,                     // VkBlendFactor            srcColorBlendFactor
                m_testParams.dstFactor,                     // VkBlendFactor            dstColorBlendFactor
                VK_BLEND_OP_ADD,                            // VkBlendOp                colorBlendOp
                VK_BLEND_FACTOR_ONE,                        // VkBlendFactor            srcAlphaBlendFactor
                VK_BLEND_FACTOR_ZERO,                       // VkBlendFactor            dstAlphaBlendFactor
                VK_BLEND_OP_ADD,                            // VkBlendOp                alphaBlendOp
                writeMask                                   // VkColorComponentFlags    colorWriteMask
            };
            colorBlendAttachmentStates.emplace_back(blendState);
        }

        const VkPipelineColorBlendStateCreateInfo colorBlendStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType                                sType
            DE_NULL,           // const void*                                    pNext
            0u,                // VkPipelineColorBlendStateCreateFlags            flags
            VK_FALSE,          // VkBool32                                        logicOpEnable
            VK_LOGIC_OP_CLEAR, // VkLogicOp                                    logicOp
            (uint32_t)
                colorBlendAttachmentStates.size(), // uint32_t                                        attachmentCount
            colorBlendAttachmentStates.data(),     // const VkPipelineColorBlendAttachmentState*    pAttachments
            {0.0f, 0.0f, 0.0f, 0.0f}               // float                                        blendConstants[4]
        };

        const bool useDepthStencil            = (m_testParams.depthStencilFormat != VK_FORMAT_UNDEFINED);
        const VkStencilOpState stencilOpState = {
            VK_STENCIL_OP_KEEP,  // VkStencilOp failOp;
            VK_STENCIL_OP_KEEP,  // VkStencilOp passOp;
            VK_STENCIL_OP_KEEP,  // VkStencilOp depthFailOp;
            VK_COMPARE_OP_EQUAL, // VkCompareOp compareOp;
            0xff,                // uint32_t compareMask;
            0xff,                // uint32_t writeMask;
            0x81                 // uint32_t reference;
        };
        const VkPipelineDepthStencilStateCreateInfo depthStencilStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType                            sType
            DE_NULL,                              // const void*                                pNext
            0u,                                   // VkPipelineDepthStencilStateCreateFlags    flags
            useDepthStencil ? VK_TRUE : VK_FALSE, // VkBool32                                    depthTestEnable
            useDepthStencil ? VK_TRUE : VK_FALSE, // VkBool32                                    depthWriteEnable
            m_testParams.depthCompareOp,          // VkCompareOp                                depthCompareOp
            VK_FALSE,                             // VkBool32                                    depthBoundsTestEnable
            useDepthStencil ? VK_TRUE : VK_FALSE, // VkBool32                                    stencilTestEnable
            stencilOpState,                       // VkStencilOpState                            front
            stencilOpState,                       // VkStencilOpState                            back
            0.0f,                                 // float                                    minDepthBounds
            1.0f,                                 // float                                    maxDepthBounds
        };

        const VkPipelineMultisampleStateCreateInfo multisampleStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType                            sType
            DE_NULL,               // const void*                                pNext
            0u,                    // VkPipelineMultisampleStateCreateFlags    flags
            VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits                    rasterizationSamples
            VK_FALSE,              // VkBool32                                    sampleShadingEnable
            1.0f,                  // float                                    minSampleShading
            DE_NULL,               // const VkSampleMask*                        pSampleMask
            VK_FALSE,              // VkBool32                                    alphaToCoverageEnable
            VK_FALSE               // VkBool32                                    alphaToOneEnable
        };

        const VkDynamicState dynamicState = VK_DYNAMIC_STATE_VIEWPORT;

        const VkPipelineDynamicStateCreateInfo dynamicStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, // VkStructureType sType;
            DE_NULL,                                              // const void* pNext;
            0u,                                                   // VkPipelineDynamicStateCreateFlags flags;
            1u,                                                   // uint32_t dynamicStateCount;
            &dynamicState                                         // const VkDynamicState* pDynamicStates;
        };

        VkPipelineRenderingCreateInfoKHR renderingCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
                                                                DE_NULL,
                                                                0u,
                                                                0u,
                                                                DE_NULL,
                                                                VK_FORMAT_UNDEFINED,
                                                                VK_FORMAT_UNDEFINED};

        VkPipelineCreateFlags2CreateInfoKHR pipelineCreateFlags2Info = {
            VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR, DE_NULL,
            VK_PIPELINE_CREATE_2_ENABLE_LEGACY_DITHERING_BIT_EXT};

        void *nextPtr = DE_NULL;
        if (m_testParams.groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
        {
            renderingCreateInfo.colorAttachmentCount    = (uint32_t)(colorFormats.size());
            renderingCreateInfo.pColorAttachmentFormats = colorFormats.data();

            if (useDepthStencil)
            {
                renderingCreateInfo.depthAttachmentFormat   = m_testParams.depthStencilFormat;
                renderingCreateInfo.stencilAttachmentFormat = m_testParams.depthStencilFormat;
            }

            nextPtr = &renderingCreateInfo;

            if (m_testParams.revision2)
            {
                pipelineCreateFlags2Info.pNext = nextPtr;
                nextPtr                        = &pipelineCreateFlags2Info;
            }
        }

        const std::vector<VkViewport> viewports(1u, makeViewport(imageSize));
        const std::vector<VkRect2D> scissors(1u, makeRect2D(imageSize));

        m_drawResources[resourceNdx].pipeline = makeGraphicsPipeline(
            vk,                      // const DeviceInterface&                            vk
            vkDevice,                // const VkDevice                                    device
            *m_pipelineLayout,       // const VkPipelineLayout                            pipelineLayout
            *m_vertexShaderModule,   // const VkShaderModule                                vertexShaderModule
            VK_NULL_HANDLE,          // const VkShaderModule                                tessellationControlModule
            VK_NULL_HANDLE,          // const VkShaderModule                                tessellationEvalModule
            VK_NULL_HANDLE,          // const VkShaderModule                                geometryShaderModule
            *m_fragmentShaderModule, // const VkShaderModule                                fragmentShaderModule
            *m_drawResources[resourceNdx].renderPass, // const VkRenderPass                                renderPass
            viewports,                                // const std::vector<VkViewport>&                    viewports
            scissors,                                 // const std::vector<VkRect2D>&                        scissors
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,      // const VkPrimitiveTopology                        topology
            0u,                                       // const uint32_t                                    subpass
            0u,                       // const uint32_t                                    patchControlPoints
            &vertexInputStateParams,  // const VkPipelineVertexInputStateCreateInfo*        vertexInputStateCreateInfo
            DE_NULL,                  // const VkPipelineRasterizationStateCreateInfo*    rasterizationStateCreateInfo
            &multisampleStateParams,  // const VkPipelineMultisampleStateCreateInfo*        multisampleStateCreateInfo
            &depthStencilStateParams, // const VkPipelineDepthStencilStateCreateInfo*        depthStencilStateCreateInfo
            &colorBlendStateParams,   // const VkPipelineColorBlendStateCreateInfo*        colorBlendStateCreateInfo
            &dynamicStateParams,      // const VkPipelineDynamicStateCreateInfo*            dynamicStateCreateInfo
            nextPtr);                 // const void*                                        pNext
    }
}

template <typename AttachmentDescription, typename AttachmentReference, typename SubpassDescription,
          typename RenderPassCreateInfo>
void DitheringTestInstance::createRenderPassFramebuffer(bool useDithering)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = getDevice(m_context);

    uint32_t resourceNdx               = useDithering ? m_ditheringNdx : m_noDitheringNdx;
    std::vector<VkFormat> colorFormats = m_testParams.colorFormats;
    const tcu::UVec2 &imageSize        = m_testParams.imageSize;

    std::vector<AttachmentDescription> attachmentDescriptions;
    std::vector<AttachmentReference> attachmentReferences;

    for (uint32_t i = 0u; i < colorFormats.size(); ++i)
    {
        const AttachmentDescription attachmentDesc = {
            DE_NULL,                                  // const void*                        pNext
            (VkAttachmentDescriptionFlags)0,          // VkAttachmentDescriptionFlags        flags
            colorFormats[i],                          // VkFormat                            format
            VK_SAMPLE_COUNT_1_BIT,                    // VkSampleCountFlagBits            samples
            VK_ATTACHMENT_LOAD_OP_LOAD,               // VkAttachmentLoadOp                loadOp
            VK_ATTACHMENT_STORE_OP_STORE,             // VkAttachmentStoreOp                storeOp
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp                stencilLoadOp
            VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp                stencilStoreOp
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout                    initialLayout
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL      // VkImageLayout                    finalLayout
        };

        const AttachmentReference attachmentReference = {
            DE_NULL,                                  // const void*            pNext
            i,                                        // uint32_t                attachment
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout        layout
            VK_IMAGE_ASPECT_COLOR_BIT                 // VkImageAspectFlags    aspectMask
        };

        attachmentDescriptions.emplace_back(attachmentDesc);
        attachmentReferences.emplace_back(attachmentReference);
    }

    bool useDepthStencil                      = (m_testParams.depthStencilFormat != VK_FORMAT_UNDEFINED);
    const AttachmentDescription dsDescription = {
        DE_NULL,                                          // const void*                        pNext
        (VkAttachmentDescriptionFlags)0,                  // VkAttachmentDescriptionFlags        flags
        m_testParams.depthStencilFormat,                  // VkFormat                            format
        VK_SAMPLE_COUNT_1_BIT,                            // VkSampleCountFlagBits            samples
        VK_ATTACHMENT_LOAD_OP_LOAD,                       // VkAttachmentLoadOp                loadOp
        VK_ATTACHMENT_STORE_OP_STORE,                     // VkAttachmentStoreOp                storeOp
        VK_ATTACHMENT_LOAD_OP_LOAD,                       // VkAttachmentLoadOp                stencilLoadOp
        VK_ATTACHMENT_STORE_OP_STORE,                     // VkAttachmentStoreOp                stencilStoreOp
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // VkImageLayout                    initialLayout
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL              // VkImageLayout                    finalLayout
    };
    const AttachmentReference dsReference = {
        DE_NULL,                                                // const void*            pNext
        (uint32_t)attachmentReferences.size(),                  // uint32_t                attachment
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,       // VkImageLayout        layout
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT // VkImageAspectFlags    aspectMask
    };

    if (useDepthStencil)
        attachmentDescriptions.emplace_back(dsDescription);

    VkSubpassDescriptionFlags subpassDescriptionFlags = 0u;
    if (useDithering)
        subpassDescriptionFlags = VK_SUBPASS_DESCRIPTION_ENABLE_LEGACY_DITHERING_BIT_EXT;
    const SubpassDescription subpassDescription = {
        DE_NULL,
        subpassDescriptionFlags,                  // VkSubpassDescriptionFlags        flags
        VK_PIPELINE_BIND_POINT_GRAPHICS,          // VkPipelineBindPoint                pipelineBindPoint
        0u,                                       // uint32_t                            viewMask
        0u,                                       // uint32_t                            inputAttachmentCount
        DE_NULL,                                  // const VkAttachmentReference*        pInputAttachments
        (uint32_t)attachmentReferences.size(),    // uint32_t                            colorAttachmentCount
        attachmentReferences.data(),              // const VkAttachmentReference*        pColorAttachments
        DE_NULL,                                  // const VkAttachmentReference*        pResolveAttachments
        useDepthStencil ? &dsReference : DE_NULL, // const VkAttachmentReference*        pDepthStencilAttachment
        0u,                                       // uint32_t                            preserveAttachmentCount
        DE_NULL                                   // const uint32_t*                    pPreserveAttachments
    };

    // Create render pass.
    const RenderPassCreateInfo renderPassInfo = {
        DE_NULL,                                 // const void*                        pNext
        (VkRenderPassCreateFlags)0,              // VkRenderPassCreateFlags            flags
        (uint32_t)attachmentDescriptions.size(), // uint32_t                            attachmentCount
        attachmentDescriptions.data(),           // const VkAttachmentDescription*    pAttachments
        1,                                       // uint32_t                            subpassCount
        &subpassDescription,                     // const VkSubpassDescription*        pSubpasses
        0u,                                      // uint32_t                            dependencyCount
        DE_NULL,                                 // const VkSubpassDependency*        pDependencies
        0u,                                      // uint32_t                            correlatedViewMaskCount
        DE_NULL                                  // const uint32_t*                    pCorrelatedViewMasks
    };

    m_drawResources[resourceNdx].renderPass = renderPassInfo.createRenderPass(vk, vkDevice);

    std::vector<VkImageView> views;
    for (const auto &view : m_drawResources[resourceNdx].imageViews)
        views.emplace_back(*view);

    if (useDepthStencil)
        views.emplace_back(*m_drawResources[resourceNdx].depthStencilImageView);

    // Create framebuffer.
    const VkFramebufferCreateInfo framebufferParams = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType            sType
        DE_NULL,                                   // const void*                pNext
        0u,                                        // VkFramebufferCreateFlags    flags
        *m_drawResources[resourceNdx].renderPass,  // VkRenderPass                renderPass
        (uint32_t)views.size(),                    // uint32_t                    attachmentCount
        views.data(),                              // const VkImageView*        pAttachments
        (uint32_t)imageSize.x(),                   // uint32_t                    width
        (uint32_t)imageSize.y(),                   // uint32_t                    height
        1u                                         // uint32_t                    layers
    };

    m_drawResources[resourceNdx].framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
}

} // namespace

static void createChildren(tcu::TestCaseGroup *ditheringTests, const SharedGroupParams groupParams, bool revision2)
{
    tcu::TestContext &testCtx           = ditheringTests->getTestContext();
    uint32_t imageDimensions            = 256u;
    uint32_t smallRenderAreaDimensions  = 31u;
    uint32_t maxRenderOffset            = imageDimensions - smallRenderAreaDimensions;
    uint32_t extraRandomAreaRenderCount = 10u;
    TestParams testParams;
    VkFormat testFormats[] = {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R5G6B5_UNORM_PACK16, VK_FORMAT_R4G4B4A4_UNORM_PACK16,
                              VK_FORMAT_R5G5B5A1_UNORM_PACK16};
    uint32_t testFormatCount = sizeof(testFormats) / sizeof(testFormats[0]);

    testParams.overrideColor      = tcu::Vec4(0.5f, 0.0f, 0.0f, 1.0f);
    testParams.imageSize          = tcu::UVec2{imageDimensions, imageDimensions};
    testParams.groupParams        = groupParams;
    testParams.depthStencilFormat = VK_FORMAT_UNDEFINED;
    testParams.srcFactor          = VK_BLEND_FACTOR_SRC_ALPHA;
    testParams.dstFactor          = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    testParams.depthClearValue    = 1.0f;
    testParams.stencilClearValue  = 0x81;
    testParams.depthCompareOp     = VK_COMPARE_OP_LESS;
    testParams.blending           = false;
    testParams.revision2          = revision2;

    // Complete render pass.
    testParams.renderAreas.emplace_back(makeViewport(testParams.imageSize));

    // Base tests. Ensures dithering works and values are within one ULP.
    {
        // Test dithering works and values are withing one ULP
        de::MovePtr<tcu::TestCaseGroup> baseTests(new tcu::TestCaseGroup(testCtx, "base"));

        // Small render area, snapped to each side (Left, Right, Bottom, Top).
        testParams.renderAreas.emplace_back(
            makeViewport(0.0f, 99.0f, (float)smallRenderAreaDimensions, (float)smallRenderAreaDimensions, 0.0f, 1.0f));
        testParams.renderAreas.emplace_back(makeViewport((float)maxRenderOffset, 99.0f,
                                                         (float)smallRenderAreaDimensions,
                                                         (float)smallRenderAreaDimensions, 0.0f, 1.0f));
        testParams.renderAreas.emplace_back(
            makeViewport(99.0f, 0.0f, (float)smallRenderAreaDimensions, (float)smallRenderAreaDimensions, 0.0f, 1.0f));
        testParams.renderAreas.emplace_back(makeViewport(99.0f, (float)maxRenderOffset,
                                                         (float)smallRenderAreaDimensions,
                                                         (float)smallRenderAreaDimensions, 0.0f, 1.0f));

        // Small render area, snapped to each corner (BotLeft, BotRight, TopLeft, TopRight).
        testParams.renderAreas.emplace_back(
            makeViewport(0.0f, 0.0f, (float)smallRenderAreaDimensions, (float)smallRenderAreaDimensions, 0.0f, 1.0f));
        testParams.renderAreas.emplace_back(makeViewport((float)maxRenderOffset, 0.0f, (float)smallRenderAreaDimensions,
                                                         (float)smallRenderAreaDimensions, 0.0f, 1.0f));
        testParams.renderAreas.emplace_back(makeViewport(0.0f, (float)maxRenderOffset, (float)smallRenderAreaDimensions,
                                                         (float)smallRenderAreaDimensions, 0.0f, 1.0f));
        testParams.renderAreas.emplace_back(makeViewport((float)maxRenderOffset, (float)maxRenderOffset,
                                                         (float)smallRenderAreaDimensions,
                                                         (float)smallRenderAreaDimensions, 0.0f, 1.0f));

        // Some random offsets.
        srand(uint32_t(time(DE_NULL)));
        for (uint32_t i = 0; i < extraRandomAreaRenderCount; ++i)
        {
            uint32_t x_offset = ((uint32_t)rand()) % (maxRenderOffset - 1);
            uint32_t y_offset = ((uint32_t)rand()) % (maxRenderOffset - 1);

            // Ensure odd offset
            x_offset |= 1u;
            y_offset |= 1u;

            testParams.renderAreas.emplace_back(makeViewport((float)x_offset, (float)y_offset,
                                                             (float)smallRenderAreaDimensions,
                                                             (float)smallRenderAreaDimensions, 0.0f, 1.0f));
        }

        for (uint32_t i = 0; i < testFormatCount; ++i)
        {
            testParams.colorFormats.emplace_back(testFormats[i]);
            const std::string iFormatName =
                de::toLower(de::toString(getFormatStr(testParams.colorFormats.back())).substr(10));
            baseTests->addChild(new DitheringTest(testCtx, iFormatName, testParams));

            for (uint32_t j = i + 1; j < testFormatCount; ++j)
            {
                testParams.colorFormats.emplace_back(testFormats[j]);
                const std::string jFormatName =
                    iFormatName + "_and_" +
                    de::toLower(de::toString(getFormatStr(testParams.colorFormats.back())).substr(10));
                baseTests->addChild(new DitheringTest(testCtx, jFormatName, testParams));

                for (uint32_t k = j + 1; k < testFormatCount; ++k)
                {
                    testParams.colorFormats.emplace_back(testFormats[k]);
                    const std::string kFormatName =
                        jFormatName + "_and_" +
                        de::toLower(de::toString(getFormatStr(testParams.colorFormats.back())).substr(10));
                    baseTests->addChild(new DitheringTest(testCtx, kFormatName, testParams));

                    testParams.colorFormats.pop_back();
                }

                testParams.colorFormats.pop_back();
            }

            testParams.colorFormats.pop_back();
        }

        ditheringTests->addChild(baseTests.release());
    }

    // Complete render pass.
    testParams.renderAreas.clear(); // Need to reset all
    testParams.renderAreas.emplace_back(makeViewport(testParams.imageSize));

    // Depth/stencil tests. Ensure dithering works with depth/stencil and it does not affect depth/stencil.
    {
        de::MovePtr<tcu::TestCaseGroup> depthStencilTests(new tcu::TestCaseGroup(testCtx, "depth_stencil"));

        const std::string names[]         = {"Less", "Greater", "Equal"};
        const uint32_t stencilValues[]    = {0x80, 0x82, 0x81};
        const uint32_t stencilValuesCount = sizeof(stencilValues) / sizeof(stencilValues[0]);
        const float *basePtr              = reinterpret_cast<const float *>(&baseDepthValue);
        const float *oneUlpMorePtr        = reinterpret_cast<const float *>(&oneUlpMoreDepthValue);
        const float *oneUlpLessPtr        = reinterpret_cast<const float *>(&oneUlpLessDepthValue);
        const float depthValues[]         = {*oneUlpLessPtr, *oneUlpMorePtr, *basePtr};
        const uint32_t depthValuesCount   = sizeof(depthValues) / sizeof(depthValues[0]);
        const VkCompareOp compareOps[]    = {VK_COMPARE_OP_LESS, VK_COMPARE_OP_GREATER};
        const uint32_t compareOpsCount    = sizeof(compareOps) / sizeof(compareOps[0]);

        testParams.depthStencilFormat = VK_FORMAT_D24_UNORM_S8_UINT;
        for (uint32_t i = 0; i < testFormatCount; ++i)
        {
            testParams.colorFormats.emplace_back(testFormats[i]);
            const std::string formatName =
                de::toLower(de::toString(getFormatStr(testParams.colorFormats.back())).substr(10));

            for (uint32_t j = 0u; j < stencilValuesCount; ++j)
            {
                testParams.stencilClearValue = stencilValues[j];

                for (uint32_t k = 0u; k < depthValuesCount; ++k)
                {
                    testParams.depthClearValue = depthValues[k];

                    for (uint32_t l = 0u; l < compareOpsCount; ++l)
                    {
                        testParams.depthCompareOp = compareOps[l];
                        depthStencilTests->addChild(new DitheringTest(
                            testCtx, "stencil" + names[j] + "_depth" + names[k] + "_op" + names[l] + "_" + formatName,
                            testParams));
                    }
                }
            }
            testParams.colorFormats.pop_back();
        }
        testParams.depthStencilFormat = VK_FORMAT_UNDEFINED;

        ditheringTests->addChild(depthStencilTests.release());
    }

    // Blend tests. Ensure dithering works with blending.
    {
        de::MovePtr<tcu::TestCaseGroup> blendTests(new tcu::TestCaseGroup(testCtx, "blend"));

        testParams.blending = true;
        for (uint32_t i = 0; i < testFormatCount; ++i)
        {
            testParams.colorFormats.emplace_back(testFormats[i]);
            const std::string formatName =
                de::toLower(de::toString(getFormatStr(testParams.colorFormats.back())).substr(10));

            testParams.overrideColor = tcu::Vec4(0.5f, 0.0f, 0.0f, 1.0f);
            testParams.srcFactor     = VK_BLEND_FACTOR_SRC_ALPHA;
            testParams.dstFactor     = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blendTests->addChild(new DitheringTest(testCtx, "srcAlpha_" + formatName, testParams));

            testParams.overrideColor = tcu::Vec4(0.125f, 0.0f, 0.0f, 1.0f);
            testParams.srcFactor     = VK_BLEND_FACTOR_ONE;
            testParams.dstFactor     = VK_BLEND_FACTOR_ONE;
            blendTests->addChild(new DitheringTest(testCtx, "additive_" + formatName, testParams));
            testParams.colorFormats.pop_back();
        }
        testParams.blending = false;

        ditheringTests->addChild(blendTests.release());
    }
}

static void cleanupGroup(tcu::TestCaseGroup *group, const SharedGroupParams, bool revision2)
{
    DE_UNREF(group);
    DE_UNREF(revision2);
    // Destroy singleton objects.
    g_singletonDevice.clear();
}

static tcu::TestCaseGroup *createDitheringRevision1GroupTests(tcu::TestContext &testCtx,
                                                              const SharedGroupParams groupParams)
{
    return createTestGroup(testCtx, "v1", createChildren, groupParams, false, cleanupGroup);
}

static tcu::TestCaseGroup *createDitheringRevision2GroupTests(tcu::TestContext &testCtx,
                                                              const SharedGroupParams groupParams)
{
    return createTestGroup(testCtx, "v2", createChildren, groupParams, true, cleanupGroup);
}

tcu::TestCaseGroup *createRenderPassDitheringTests(tcu::TestContext &testCtx, const SharedGroupParams groupParams)
{
    // Tests for VK_EXT_legacy_dithering
    de::MovePtr<tcu::TestCaseGroup> ditheringTests(new tcu::TestCaseGroup(testCtx, "dithering"));
    ditheringTests->addChild(createDitheringRevision1GroupTests(testCtx, groupParams));
    if (groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
        ditheringTests->addChild(createDitheringRevision2GroupTests(testCtx, groupParams));

    return ditheringTests.release();
}

} // namespace renderpass
} // namespace vkt
