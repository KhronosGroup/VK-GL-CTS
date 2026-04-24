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
 * \brief Test VK_QCOM_multiview_per_view* extensions.
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassMultiviewPerViewTests.hpp"

#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"

#include <array>
#include <sstream>
#include <vector>

#define USE_PER_VIEW_VIEWPORTS_EXT 1

namespace vkt
{
namespace renderpass
{

using namespace vk;

namespace
{

enum class DynamicState
{
    NO = 0,    // Static state.
    YES,       // Dynamic state.
    YES_COUNT, // Dynamic state, with count (requires VK_EXT_extended_dynamic_state).
};

std::string getDynStateSuffix(DynamicState dynState)
{
    if (dynState == DynamicState::NO)
        return "_static";
    if (dynState == DynamicState::YES)
        return "_dynamic";
    if (dynState == DynamicState::YES_COUNT)
        return "_dynamic_with_count";

    DE_ASSERT(false);
    return "";
}

// Each view is going to have different viewports/scissors. What makes them different?
enum ViewportDiffFlagBits
{
    DIFF_FLAGS_OFFSET = (1 << 0),
    DIFF_FLAGS_SIZE   = (1 << 1),
    DIFF_FLAGS_DEPTH  = (1 << 2),
};
using ViewportDiffFlags = uint32_t;

std::string getViewportDiffSuffix(ViewportDiffFlags diffFlags)
{
    std::string suffix;
    if (diffFlags & DIFF_FLAGS_OFFSET)
        suffix += "_offset";
    if (diffFlags & DIFF_FLAGS_SIZE)
        suffix += "_size";
    if (diffFlags & DIFF_FLAGS_DEPTH)
        suffix += "_depth";
    return suffix;
}

struct ViewportsParams
{
    SharedGroupParams groupParams;
    DynamicState viewportDynState;
    DynamicState scissorDynState;
    ViewportDiffFlags viewportDiffFlags;
    bool multiPass; // Single subpass or multiple subpasses.

    // Returns true if the params require VK_EXT_extended_dynamic_state.
    bool requiresExtendedDynamicState() const
    {
        return (viewportDynState == DynamicState::YES_COUNT || scissorDynState == DynamicState::YES_COUNT);
    }

    // Returns true if the params use dynamic rendering.
    bool useDynamicRendering() const
    {
        return (groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING);
    }

    tcu::Vec4 getViewColor(int viewIndex) const
    {
        const std::array<tcu::Vec4, 2> viewColors{
            tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f),
            tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f),
        };
        return viewColors.at(viewIndex);
    }

    tcu::Vec4 getClearColor() const
    {
        return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f); // Must be different from the view colors above.
    }

    float getClearDepth() const
    {
        return 1.0f; // Must be different from geometry depths, applying the viewport transform.
    }
};

class ViewportsInstance : public vkt::TestInstance
{
public:
    ViewportsInstance(Context &context, const ViewportsParams &params) : TestInstance(context), m_params(params)
    {
    }
    virtual ~ViewportsInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const ViewportsParams m_params;
};

class ViewportsCase : public vkt::TestCase
{
public:
    ViewportsCase(tcu::TestContext &testCtx, const std::string &name, const ViewportsParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~ViewportsCase(void) = default;

    std::string getRequiredCapabilitiesId() const override
    {
        return typeid(ViewportsCase).name();
    }

    void initDeviceCapabilities(DevCaps &caps) override
    {
        // For cases with dynamic rendering.
        caps.addExtension("VK_KHR_dynamic_rendering");
        caps.addFeature(&VkPhysicalDeviceDynamicRenderingFeatures::dynamicRendering);
        caps.addExtension("VK_KHR_depth_stencil_resolve");

        // For cases with extended dynamic state.
        caps.addExtension("VK_EXT_extended_dynamic_state");
        caps.addFeature(&VkPhysicalDeviceExtendedDynamicStateFeaturesEXT::extendedDynamicState);

        // Note renderpass2 and its dependencies are always needed, even in the dynamic rendering case, because
        // depth/stencil resolve depends on it.
        caps.addExtension("VK_KHR_create_renderpass2");
        caps.addExtension("VK_KHR_multiview");
        caps.addFeature(&VkPhysicalDeviceMultiviewFeatures::multiview);
        caps.addExtension("VK_KHR_maintenance2");

#ifdef USE_PER_VIEW_VIEWPORTS_EXT
        caps.addExtension("VK_QCOM_multiview_per_view_viewports");
#ifndef CTS_USES_VULKANSC
        caps.addFeature(&VkPhysicalDeviceMultiviewPerViewViewportsFeaturesQCOM::multiviewPerViewViewports);
#endif // CTS_USES_VULKANSC
#else
        caps.addFeature(&VkPhysicalDeviceVulkan12Features::shaderOutputViewportIndex);
#endif

        caps.addFeature(&VkPhysicalDeviceFeatures::multiViewport);
    }

    void checkSupport(Context &context) const override
    {
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_MULTI_VIEWPORT);

        if (m_params.useDynamicRendering())
            context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

        if (m_params.requiresExtendedDynamicState())
            context.requireDeviceFunctionality("VK_EXT_extended_dynamic_state");

        context.requireDeviceFunctionality("VK_KHR_create_renderpass2");
        context.requireDeviceFunctionality("VK_KHR_multiview");

#ifdef USE_PER_VIEW_VIEWPORTS_EXT
        context.requireDeviceFunctionality("VK_QCOM_multiview_per_view_viewports");
#else
        if (context.getUsedApiVersion() < VK_API_VERSION_1_2)
            TCU_THROW(NotSupportedError, "Vulkan version 1.2 required");
#endif
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

    TestInstance *createInstance(Context &context) const override
    {
        return new ViewportsInstance(context, m_params);
    }

protected:
    const ViewportsParams m_params;
};

void ViewportsCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 460\n"

#ifndef USE_PER_VIEW_VIEWPORTS_EXT
         << "#extension GL_EXT_multiview : enable\n"
         << "#extension GL_ARB_shader_viewport_layer_array : enable\n"
#endif

         << "\n"
         << "void main() {\n"
         // Full-screen clockwise triangle strip with 4 vertices.
         << "    const float x = (-1.0+2.0*((gl_VertexIndex & 2)>>1));\n"
         << "    const float y = ( 1.0-2.0* (gl_VertexIndex % 2));\n"
         << "    gl_Position = vec4(x, y, 0.0, 1.0);\n"

#ifndef USE_PER_VIEW_VIEWPORTS_EXT
         << "    gl_ViewportIndex = gl_ViewIndex;\n"
#endif

         << "}\n";

#ifdef USE_PER_VIEW_VIEWPORTS_EXT
    const vk::ShaderBuildOptions spvOpts;
#else
    const vk::ShaderBuildOptions spvOpts(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_5, 0u, false);
#endif
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str()) << spvOpts;

    std::ostringstream frag;
    frag << "#version 460\n"
         << "#extension GL_EXT_multiview : enable\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main() {\n"
         << "    if (gl_ViewIndex == 0)\n"
         << "        outColor = vec4" << m_params.getViewColor(0) << ";\n"
         << "    else if (gl_ViewIndex == 1)\n"
         << "        outColor = vec4" << m_params.getViewColor(1) << ";\n"
         << "    else\n"
         << "        outColor = vec4" << m_params.getClearColor() << ";\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

tcu::TestStatus ViewportsInstance::iterate(void)
{
    const auto ctx = m_context.getContextCommonData();

    const tcu::IVec3 extent(16, 16, 2);
    const auto extentU     = extent.asUint();
    const auto extentF     = extent.asFloat();
    const auto extentVk    = makeExtent3D(extentU.x(), extentU.y(), 1u);
    const auto layerCount  = extentU.z();
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage  = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto imageType   = VK_IMAGE_TYPE_2D;
    const auto depthFormat = VK_FORMAT_D16_UNORM;
    const auto depthUsage  = (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto sampleCount = VK_SAMPLE_COUNT_1_BIT;
    const auto bindPoint   = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto renderArea  = makeRect2D(extent);
    const auto colorSRR    = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, layerCount);
    const auto depthSRR    = makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, layerCount);
    const auto colorSRL    = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, layerCount);
    const auto depthSRL    = makeImageSubresourceLayers(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, layerCount);
    const auto clearColor  = m_params.getClearColor();
    const auto clearDepth  = m_params.getClearDepth();

    const auto attAccesses =
        (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    const auto attStages = (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

    // Multilayer color buffer.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, extentVk, colorFormat, colorUsage, imageType,
                                colorSRR, layerCount);

    // Multilayer depth buffer.
    const VkImageCreateInfo dsBufferCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        VK_IMAGE_TYPE_2D,
        depthFormat,
        extentVk,
        1u,
        layerCount,
        sampleCount,
        VK_IMAGE_TILING_OPTIMAL,
        depthUsage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ImageWithMemory depthBuffer(ctx.vkd, ctx.device, ctx.allocator, dsBufferCreateInfo, MemoryRequirement::Any);
    const auto depthView =
        makeImageView(ctx.vkd, ctx.device, *depthBuffer, VK_IMAGE_VIEW_TYPE_2D_ARRAY, depthFormat, depthSRR);
    const auto depthCopyFormat = getDepthCopyFormat(depthFormat);

    const auto depthVerifBufferSize = tcu::getPixelSize(depthCopyFormat) * extent.x() * extent.y() * extent.z();
    const auto depthVerifBufferInfo = makeBufferCreateInfo(depthVerifBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    BufferWithMemory depthVerifBuffer(ctx.vkd, ctx.device, ctx.allocator, depthVerifBufferInfo, HostIntent::R);

    // Framebuffer image views.
    const std::vector<VkImageView> fbViews{colorBuffer.getImageView(), depthView.get()};

    std::vector<uint32_t> subpassMasks;
    if (m_params.multiPass)
    {
        subpassMasks.push_back(0x01u);
        subpassMasks.push_back(0x02u);
    }
    else
    {
        subpassMasks.push_back(0x03u);
    }

    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> framebuffer;

    if (!m_params.useDynamicRendering())
    {
        std::vector<VkAttachmentDescription2> attDescriptions;
        attDescriptions.reserve(2u); // Color and Depth.

        // Color att.
        attDescriptions.push_back(VkAttachmentDescription2{
            VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
            nullptr,
            0u,
            colorFormat,
            sampleCount,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        });

        // Depth att.
        attDescriptions.push_back(VkAttachmentDescription2{
            VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
            nullptr,
            0u,
            depthFormat,
            sampleCount,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        });

        std::vector<VkAttachmentReference2> attReferences;
        attReferences.reserve(2u);

        // Color ref.
        attReferences.push_back(VkAttachmentReference2{
            VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
            nullptr,
            0u,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT,
        });

        // Depth ref.
        attReferences.push_back(VkAttachmentReference2{
            VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
            nullptr,
            1u,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_DEPTH_BIT,
        });

        std::vector<VkSubpassDescription2> subpassDescriptions;
        subpassDescriptions.reserve(subpassMasks.size());

        for (const auto subpassMask : subpassMasks)
        {
            subpassDescriptions.emplace_back(VkSubpassDescription2{
                VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
                nullptr,
                0u,
                bindPoint,
                subpassMask,
                0u,
                nullptr,
                1u,
                &attReferences.front(),
                nullptr,
                &attReferences.back(),
                0u,
                nullptr,
            });
        }

        std::vector<VkSubpassDependency2> subpassDependencies;

        // This is needed because otherwise we have an image layout transition race.
        if (subpassMasks.size() > 1)
        {
            subpassDependencies.push_back(VkSubpassDependency2{
                VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
                nullptr,
                0u,
                1u,
                attStages,
                attStages,
                attAccesses,
                attAccesses,
                VK_DEPENDENCY_BY_REGION_BIT,
                0u,
            });
        }

        const VkRenderPassCreateInfo2 rpCreateInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
            nullptr,
            0u,
            de::sizeU32(attDescriptions),
            de::dataOrNull(attDescriptions),
            de::sizeU32(subpassDescriptions),
            de::dataOrNull(subpassDescriptions),
            de::sizeU32(subpassDependencies),
            de::dataOrNull(subpassDependencies),
            0u,
            nullptr,
        };

        renderPass = createRenderPass2(ctx.vkd, ctx.device, &rpCreateInfo);
        // Note with multiview the framebuffer layer count is specified as 1.
        framebuffer = makeFramebuffer(ctx.vkd, ctx.device, *renderPass, de::sizeU32(fbViews), de::dataOrNull(fbViews),
                                      extentU.x(), extentU.y(), 1u);
    }

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();

    // Base viewport params.
    const float baseViewportX        = extentF.x() / 2.0f;
    const float baseViewportY        = extentF.y() / 2.0f;
    const float baseViewportWidth    = 2.0f;
    const float baseViewportHeight   = 2.0f;
    const float baseViewportMinDepth = 0.0f;
    const float baseViewportMaxDepth = 1.0f;

    // Base scissor params.
    const int32_t baseScissorX       = extent.x() / 2;
    const int32_t baseScissorY       = extent.y() / 2;
    const uint32_t baseScissorWidth  = 2u;
    const uint32_t baseScissorHeight = 2u;

    const auto baseViewport = makeViewport(baseViewportX, baseViewportY, baseViewportWidth, baseViewportHeight,
                                           baseViewportMinDepth, baseViewportMaxDepth);
    const auto baseScissor  = makeRect2D(baseScissorX, baseScissorY, baseScissorWidth, baseScissorHeight);

    std::vector<VkViewport> actualViewports(2u, baseViewport);
    std::vector<VkRect2D> actualScissors(2u, baseScissor);

    if (m_params.viewportDiffFlags & DIFF_FLAGS_OFFSET)
    {
        actualViewports.front().x = (extentF.x() * 3.0f) / 4.0f;
        actualViewports.front().y = (extentF.y() * 3.0f) / 4.0f;
        actualViewports.back().x  = 0.0f;
        actualViewports.back().y  = 0.0f;

        actualScissors.front().offset.x = (extent.x() * 3) / 4;
        actualScissors.front().offset.y = (extent.y() * 3) / 4;
        actualScissors.back().offset.x  = 0;
        actualScissors.back().offset.y  = 0;
    }

    if (m_params.viewportDiffFlags & DIFF_FLAGS_SIZE)
    {
        actualViewports.front().width  = 4.0f;
        actualViewports.front().height = 4.0f;
        actualViewports.back().width   = 3.0f;
        actualViewports.back().height  = 3.0f;

        actualScissors.front().extent.width  = 1u; // Note the scissor will trim the output.
        actualScissors.front().extent.height = 1u;
        actualScissors.back().extent.width   = 3u;
        actualScissors.back().extent.height  = 3u;
    }

    if (m_params.viewportDiffFlags & DIFF_FLAGS_DEPTH)
    {
        actualViewports.front().minDepth = 0.5f;
        actualViewports.front().maxDepth = 0.75f;
        actualViewports.back().minDepth  = 0.25f;
        actualViewports.back().maxDepth  = 1.0f;
    }

    std::vector<VkViewport> staticViewports;

    if (m_params.viewportDynState == DynamicState::NO)
        staticViewports = actualViewports;
    else if (m_params.viewportDynState == DynamicState::YES)
        staticViewports.resize(actualViewports.size(), baseViewport);
    // For YES_COUNT, the static viewports array stays empty.

    std::vector<VkRect2D> staticScissors;
    if (m_params.scissorDynState == DynamicState::NO)
        staticScissors = actualScissors;
    else if (m_params.scissorDynState == DynamicState::YES)
        staticScissors.resize(actualScissors.size(), baseScissor);
    // For YES_COUNT, the static scissors array stays empty.

    const auto stencilOp = makeStencilOpState(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
                                              VK_COMPARE_OP_ALWAYS, 0xFFu, 0xFFu, 0u);
    const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        nullptr,
        0u,
        VK_TRUE,
        VK_TRUE,
        VK_COMPARE_OP_ALWAYS,
        VK_FALSE,
        VK_FALSE,
        stencilOp,
        stencilOp,
        0.0f,
        1.0f,
    };

    std::vector<VkDynamicState> dynamicStates;

    if (m_params.viewportDynState == DynamicState::YES)
        dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    else if (m_params.viewportDynState == DynamicState::YES_COUNT)
        dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT);

    if (m_params.scissorDynState == DynamicState::YES)
        dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
    else if (m_params.scissorDynState == DynamicState::YES_COUNT)
        dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT);

    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(dynamicStates),
        de::dataOrNull(dynamicStates),
    };

    PipelineLayoutWrapper pipelineLayout(m_params.groupParams->pipelineConstructionType, ctx.vkd, ctx.device);

    const auto &binaries = m_context.getBinaryCollection();
    ShaderWrapper vertShader(ctx.vkd, ctx.device, binaries.get("vert"));
    ShaderWrapper fragShader(ctx.vkd, ctx.device, binaries.get("frag"));

    const auto pipelineCount = de::sizeU32(subpassMasks); // One pipeline for each mask.
    using PipelinePtr        = std::unique_ptr<GraphicsPipelineWrapper>;
    std::vector<PipelinePtr> pipelines;
    pipelines.reserve(pipelineCount);

    std::unique_ptr<VkPipelineRenderingCreateInfo> pRenderingCreateInfo;
    if (m_params.useDynamicRendering())
    {
        pRenderingCreateInfo.reset(new VkPipelineRenderingCreateInfo{
            VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            nullptr,
            0u, // Needs to be changed for each pipeline.
            1u,
            &colorFormat,
            depthFormat,
            VK_FORMAT_UNDEFINED,
        });
    }

    for (uint32_t i = 0u; i < pipelineCount; ++i)
    {
        pipelines.emplace_back(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                           m_context.getDeviceExtensions(),
                                                           m_params.groupParams->pipelineConstructionType));
        auto &pipeline = *pipelines.back();

        // Appropriate view mask for each pass.
        if (m_params.useDynamicRendering())
            pRenderingCreateInfo->viewMask = subpassMasks.at(i);

        pipeline.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            .setDefaultRasterizationState()
            .setDefaultMultisampleState()
            .setDefaultColorBlendState()
            .setDefaultViewportsCount()
            .setDefaultScissorsCount()
            .setDynamicState(&dynamicStateCreateInfo)
            .setupVertexInputState(&vertexInputStateCreateInfo)
            .setupPreRasterizationShaderState(staticViewports, staticScissors, pipelineLayout, *renderPass, i,
                                              vertShader, nullptr, ShaderWrapper(), ShaderWrapper(), ShaderWrapper(),
                                              nullptr, nullptr, pRenderingCreateInfo.get())
            .setupFragmentShaderState(pipelineLayout, *renderPass, i, fragShader, &depthStencilStateCreateInfo)
            .setupFragmentOutputState(*renderPass, i)
            .buildPipeline();
    }

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const auto drawQuad = [&]() { ctx.vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u); };

    const auto recordDynamicStates = [&]()
    {
        if (m_params.viewportDynState == DynamicState::YES)
            ctx.vkd.cmdSetViewport(cmdBuffer, 0u, de::sizeU32(actualViewports), de::dataOrNull(actualViewports));
        else if (m_params.viewportDynState == DynamicState::YES_COUNT)
#ifndef CTS_USES_VULKANSC
            ctx.vkd.cmdSetViewportWithCount(cmdBuffer, de::sizeU32(actualViewports), de::dataOrNull(actualViewports));
#else
            ctx.vkd.cmdSetViewportWithCountEXT(cmdBuffer, de::sizeU32(actualViewports),
                                               de::dataOrNull(actualViewports));
#endif // CTS_USES_VULKANSC

        if (m_params.scissorDynState == DynamicState::YES)
            ctx.vkd.cmdSetScissor(cmdBuffer, 0u, de::sizeU32(actualScissors), de::dataOrNull(actualScissors));
        else if (m_params.scissorDynState == DynamicState::YES_COUNT)
#ifndef CTS_USES_VULKANSC
            ctx.vkd.cmdSetScissorWithCount(cmdBuffer, de::sizeU32(actualScissors), de::dataOrNull(actualScissors));
#else
            ctx.vkd.cmdSetScissorWithCountEXT(cmdBuffer, de::sizeU32(actualScissors), de::dataOrNull(actualScissors));
#endif // CTS_USES_VULKANSC
    };

    const std::vector<VkClearValue> clearValues{
        makeClearValueColor(clearColor),
        makeClearValueDepthStencil(clearDepth, 0u),
    };

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    if (m_params.useDynamicRendering())
    {
#ifndef CTS_USES_VULKANSC
        // Move each image to the right layout.
        {
            const auto srcStage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            const auto srcAccess = 0u;
            const auto dstStage  = attStages;
            const auto dstAccess = attAccesses;

            const std::vector<VkImageMemoryBarrier> barriers{
                makeImageMemoryBarrier(srcAccess, dstAccess, VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorBuffer.getImage(), colorSRR),
                makeImageMemoryBarrier(srcAccess, dstAccess, VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, *depthBuffer, depthSRR),
            };

            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, srcStage, dstStage, de::dataOrNull(barriers),
                                          barriers.size());
        }

        beginRendering(ctx.vkd, cmdBuffer, colorBuffer.getImageView(), depthView.get(), false, renderArea,
                       clearValues.front(), clearValues.back(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                       VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, 0u, 1u,
                       subpassMasks.front());
        recordDynamicStates();
        pipelines.front()->bind(cmdBuffer);
        drawQuad();
        endRendering(ctx.vkd, cmdBuffer);
        if (pipelines.size() > 1)
        {
            {
                // Sync writes from one render pass to the next one.
                const auto srcStages = attStages;
                const auto srcAccess = attAccesses;
                const auto dstStages = attStages;
                const auto dstAccess = attAccesses;

                const auto barrier = makeMemoryBarrier(srcAccess, dstAccess);
                cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, srcStages, dstStages, &barrier);
            }

            beginRendering(ctx.vkd, cmdBuffer, colorBuffer.getImageView(), depthView.get(), false, renderArea,
                           clearValues.front(), clearValues.back(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                           VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, 0u, 1u,
                           subpassMasks.back());
            recordDynamicStates();
            pipelines.back()->bind(cmdBuffer);
            drawQuad();
            endRendering(ctx.vkd, cmdBuffer);
        }
#endif // CTS_USES_VULKANSC
    }
    else
    {
        beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, renderArea, de::sizeU32(clearValues),
                        de::dataOrNull(clearValues));
        recordDynamicStates();
        pipelines.front()->bind(cmdBuffer);
        drawQuad();
        if (pipelines.size() > 1)
        {
            ctx.vkd.cmdNextSubpass(cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
            recordDynamicStates(); // This is needed with multiview.
            pipelines.back()->bind(cmdBuffer);
            drawQuad();
        }
        endRenderPass(ctx.vkd, cmdBuffer);
    }

    {
        // Copy color buffer and depth buffer to their verification buffers.
        const auto dstAccess = VK_ACCESS_TRANSFER_READ_BIT;
        const auto dstStages = VK_PIPELINE_STAGE_TRANSFER_BIT;

        const std::vector<VkImageMemoryBarrier> barriers{
            makeImageMemoryBarrier(attAccesses, dstAccess, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorBuffer.getImage(), colorSRR),
            makeImageMemoryBarrier(attAccesses, dstAccess, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, depthBuffer.get(), depthSRR),
        };

        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, attStages, dstStages, de::dataOrNull(barriers),
                                      barriers.size());

        const auto colorRegion = makeBufferImageCopy(extentVk, colorSRL);
        const auto depthRegion = makeBufferImageCopy(extentVk, depthSRL);

        ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, colorBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     colorBuffer.getBuffer(), 1u, &colorRegion);
        ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, depthBuffer.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     depthVerifBuffer.get(), 1u, &depthRegion);

        const auto hostBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &hostBarrier);
    }

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());
    invalidateAlloc(ctx.vkd, ctx.device, depthVerifBuffer.getAllocation());

    const auto tcuColorFormat = mapVkFormat(colorFormat);
    const auto tcuDepthFormat = mapVkFormat(depthFormat);

    tcu::TextureLevel colorRefLevel(tcuColorFormat, extent.x(), extent.y(), extent.z());
    tcu::TextureLevel depthRefLevel(tcuDepthFormat, extent.x(), extent.y(), extent.z());

    tcu::PixelBufferAccess colorRefAccess = colorRefLevel.getAccess();
    tcu::PixelBufferAccess depthRefAccess = depthRefLevel.getAccess();

    tcu::ConstPixelBufferAccess colorResult(tcuColorFormat, extent, colorBuffer.getBufferAllocation().getHostPtr());
    tcu::ConstPixelBufferAccess depthResult(tcuDepthFormat, extent, depthVerifBuffer.getAllocation().getHostPtr());

    bool fail = false;
    auto &log = m_context.getTestContext().getLog();

    for (int z = 0; z < extent.z(); ++z)
    {
        auto colorRefLayer = tcu::getSubregion(colorRefAccess, 0, 0, z, extent.x(), extent.y(), 1);
        auto depthRefLayer = tcu::getSubregion(depthRefAccess, 0, 0, z, extent.x(), extent.y(), 1);

        auto colorResLayer = tcu::getSubregion(colorResult, 0, 0, z, extent.x(), extent.y(), 1);
        auto depthResLayer = tcu::getSubregion(depthResult, 0, 0, z, extent.x(), extent.y(), 1);

        tcu::clear(colorRefLayer, clearColor);
        tcu::clearDepth(depthRefLayer, clearDepth);

        // These are the base settings as seen in the viewport parameters.
        tcu::IVec2 offset = extent.swizzle(0, 1) / tcu::IVec2(2);
        tcu::IVec2 size(2);
        float depth = 0.0f;

        // These changes should match what we did for the pipelines above.
        if (m_params.viewportDiffFlags & DIFF_FLAGS_OFFSET)
        {
            if (z == 0)
                offset = (extent.swizzle(0, 1) * tcu::IVec2(3)) / tcu::IVec2(4);
            else if (z == 1)
                offset = tcu::IVec2(0);
            else
                DE_ASSERT(false);
        }

        if (m_params.viewportDiffFlags & DIFF_FLAGS_SIZE)
        {
            if (z == 0)
                size = tcu::IVec2(1);
            else if (z == 1)
                size = tcu::IVec2(3);
            else
                DE_ASSERT(false);
        }

        if (m_params.viewportDiffFlags & DIFF_FLAGS_DEPTH)
        {
            if (z == 0)
                depth = 0.5f;
            else if (z == 1)
                depth = 0.25f;
            else
                DE_ASSERT(false);
        }

        auto geometryColorRegion = tcu::getSubregion(colorRefLayer, offset.x(), offset.y(), size.x(), size.y());
        tcu::clear(geometryColorRegion, m_params.getViewColor(z));

        auto geometryDepthRegion = tcu::getSubregion(depthRefLayer, offset.x(), offset.y(), size.x(), size.y());
        tcu::clearDepth(geometryDepthRegion, depth);

        {
            const auto name = "Color-Layer" + std::to_string(z);
            if (!tcu::floatThresholdCompare(log, name.c_str(), "", colorRefLayer, colorResLayer, tcu::Vec4(0.0f),
                                            tcu::COMPARE_LOG_ON_ERROR))
                fail = true;
        }
        {
            const float threshold = (1.0f / 65535.0f) * 1.5f; // Depth may not be exact.
            const auto name       = "Depth-Layer" + std::to_string(z);
            if (!tcu::dsThresholdCompare(log, name.c_str(), "", depthRefLayer, depthResLayer, threshold,
                                         tcu::COMPARE_LOG_ON_ERROR))
                fail = true;
        }
    }

    if (fail)
        TCU_FAIL("Unexpected results in color or depth buffers; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

// For testing per-view render areas, we can use multiple viewports/scissors or a common one for all cases.
//
// If we use a single common viewport/scissor for all render areas, we need to make sure render areas are defined
// such that they can all contain the given viewport/scissor (the spec says the viewport/sccissor must be in it).
//
// - If we use a 16x16 framebuffer, the first render area can span from pixels [0, 10) and the second one from [6, 16).
//   - This leaves a 4x4 grid in the middle, range [6, 10), for the viewport/scissor.
//
// If using multiple viewports/scissors, there are several possible approaches. The framebuffer could continue to be
// 16x16 in size, with the first layer render area in [0, 8) (top left quadrant) and the second layers in [8, 16)
// (bottom right quadrant), and matching or smaller viewports and scissors. The viewports and scissors can be set with:
//
// - VK_QCOM_multiview_per_view_viewports.
// - Setting ViewportIndex to ViewIndex from the Geometry shader (not ideal for tilers).
// - Setting ViewportIndex to ViewIndex from the Vertex shader (easier if requiring Vulkan 1.2).
//
enum class RenderAreasViewportType
{
    SINGLE     = 0,
    MULTI_QCOM = 1,
    MULTI_GEOM = 2,
    MULTI_VERT = 3,
};

/*
    To make sure the global render area is not used at any point, we can clear
    the color attachment to a separate value before starting the render pass.
    The clear color could be different for each layer, but results should be
    different for each layer in any case.

    Then, in the render pass, the clear operation will only clear a subregion
    of the attachment and, finally, the draws will affect a subregion of it,
    producing 3 levels of colors.

    If the attachment is loaded, there will be no distinction between the clear
    color from outside the render pass and from inside, so effectively we only
    have 2 levels, or 3 but with the same color in the first 2.

    If we use a color and a resolve attachment, we have different combinations
    for the load operation. The pre-render-pass clear colors should be different
    from the render pass clear colors, to make sure they are applied
    correctly. This means we may need up to 5 different colors:

    0) Geometry color: (0, 0, 1, 1)
    1) SS clear colors: (0, 1, 0, 1) and (0, 1, 1, 1) pre-RP, (0, 0, 0, 1) in-RP
    1) MS clear colors: (1, 1, 0, 1) and (1, 1, 1, 1) pre-RP, (1, 0, 0, 1) in-RP

    0) MS Clear, SS Clear: in the pre-render-pass clear, we should use different
    colors for each image: the SS image will end up with a border containing
    this initial clear color, then a border with the MS clear color from the
    render pass, which should be different from the SS clear color, and then
    the rendered area.

    1) MS Clear, SS Load: Same result as the previous one.

    2) MS Load, SS Clear: Similar result, but the inner border in the SS image
    will contain the pre-render-pass clear color from the MS image.

    3) MS Load, SS Load: Same result as the previous one.
*/

struct RenderAreasParams
{
    SharedGroupParams groupParams;
    RenderAreasViewportType viewportType;
    VkAttachmentLoadOp ssLoadOp; // LOAD or CLEAR only.
    VkAttachmentLoadOp msLoadOp; // If DONT_CARE, no MS attachment used.
    bool multiPass;

    // Returns true if the params use dynamic rendering.
    bool useDynamicRendering() const
    {
        return (groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING);
    }

    // Gets a clear color depending on the usage and image. See above for a list.
    tcu::Vec4 getClearColor(bool ssImage, bool rpClear, uint32_t layer) const
    {
        DE_ASSERT(layer < 2u);

        if (rpClear)
            DE_ASSERT(layer == 0u);

        if (ssImage)
        {
            if (rpClear)
                return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
            return (layer == 0u ? tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f) : tcu::Vec4(0.0f, 1.0f, 1.0f, 1.0f));
        }
        else
        {
            if (rpClear)
                return tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
            return (layer == 0u ? tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f) : tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
        }
    }

    tcu::Vec4 getGeometryColor() const
    {
        return tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }

    bool useMultiSample() const
    {
        return (msLoadOp != VK_ATTACHMENT_LOAD_OP_DONT_CARE);
    }

    VkFormat getColorFormat() const
    {
        return VK_FORMAT_R8G8B8A8_UNORM;
    }

    VkImageType getImageType() const
    {
        return VK_IMAGE_TYPE_2D;
    }

    VkImageUsageFlags getMultisampleColorUsage() const
    {
        return (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    }

    VkImageUsageFlags getSingleSampleColorUsage() const
    {
        return (getMultisampleColorUsage() | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    }

    tcu::IVec3 getExtent() const
    {
        // Note: this needs to be in sync with the render areas, viewports and scissors below.
        return tcu::IVec3(16, 16, 1);
    }

    bool useSingleViewport() const
    {
        return (viewportType == RenderAreasViewportType::SINGLE);
    }

    bool useGeometryShader() const
    {
        return (viewportType == RenderAreasViewportType::MULTI_GEOM);
    }

    std::vector<VkRect2D> getRenderAreas() const
    {
        std::vector<VkRect2D> renderAreas;
        renderAreas.reserve(2u);

        if (useSingleViewport())
        {
            // Overlapping in the center.
            renderAreas.push_back(makeRect2D(0, 0, 10u, 10u));
            renderAreas.push_back(makeRect2D(6, 6, 10u, 10u));
        }
        else
        {
            // Top-left and bottom-right quadrants.
            renderAreas.push_back(makeRect2D(0, 0, 8u, 8u));
            renderAreas.push_back(makeRect2D(8, 8, 8u, 8u));
        }

        return renderAreas;
    }

    std::vector<VkViewport> getViewports() const
    {
        std::vector<VkViewport> viewports;
        viewports.reserve(2u);

        if (useSingleViewport())
        {
            viewports.push_back(makeViewport(6.0f, 6.0f, 4.0f, 4.0f, 0.0f, 1.0f));
        }
        else
        {
            viewports.push_back(makeViewport(0.0f, 0.0f, 8.0f, 8.0f, 0.0f, 1.0f));
            viewports.push_back(makeViewport(8.0f, 8.0f, 8.0f, 8.0f, 0.0f, 1.0f));
        }

        return viewports;
    }

    std::vector<VkRect2D> getScissors() const
    {
        std::vector<VkRect2D> scissors;
        scissors.reserve(2u);

        if (useSingleViewport())
        {
            scissors.push_back(makeRect2D(6, 6, 4u, 4u));
        }
        else
        {
            scissors.push_back(makeRect2D(0, 0, 8u, 8u));
            scissors.push_back(makeRect2D(8, 8, 8u, 8u));
        }

        return scissors;
    }
};

class RenderAreasInstance : public vkt::TestInstance
{
public:
    RenderAreasInstance(Context &context, const RenderAreasParams &params) : TestInstance(context), m_params(params)
    {
    }
    virtual ~RenderAreasInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const RenderAreasParams m_params;
};

class RenderAreasCase : public vkt::TestCase
{
public:
    RenderAreasCase(tcu::TestContext &testCtx, const std::string &name, const RenderAreasParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~RenderAreasCase(void) = default;

    std::string getRequiredCapabilitiesId() const override
    {
        std::string capId = typeid(RenderAreasCase).name();
        if (m_params.viewportType == RenderAreasViewportType::MULTI_QCOM)
            capId += "-VK_QCOM_multiview_per_view_viewports";
        return capId;
    }

    void initDeviceCapabilities(DevCaps &caps) override
    {
        caps.addExtension("VK_QCOM_multiview_per_view_render_areas");
#ifndef CTS_USES_VULKANSC
        caps.addFeature(&VkPhysicalDeviceMultiviewPerViewRenderAreasFeaturesQCOM::multiviewPerViewRenderAreas);
#endif // CTS_USES_VULKANSC

        caps.addExtension("VK_KHR_dynamic_rendering");
        caps.addFeature(&VkPhysicalDeviceDynamicRenderingFeatures::dynamicRendering);
        caps.addExtension("VK_KHR_depth_stencil_resolve");

        // Note renderpass2 and its dependencies are always needed, even in the dynamic rendering case, because
        // depth/stencil resolve depends on it.
        caps.addExtension("VK_KHR_create_renderpass2");
        caps.addExtension("VK_KHR_multiview");
        caps.addFeature(&VkPhysicalDeviceMultiviewFeatures::multiview);
        caps.addFeature(&VkPhysicalDeviceMultiviewFeatures::multiviewGeometryShader);
        caps.addExtension("VK_KHR_maintenance2");

        if (m_params.viewportType == RenderAreasViewportType::MULTI_QCOM)
        {
            caps.addExtension("VK_QCOM_multiview_per_view_viewports");
#ifndef CTS_USES_VULKANSC
            caps.addFeature(&VkPhysicalDeviceMultiviewPerViewViewportsFeaturesQCOM::multiviewPerViewViewports);
#endif // CTS_USES_VULKANSC
        }

        // Required for RenderAreaViewportType::MULTI_VERT if used.
        caps.addFeature(&VkPhysicalDeviceVulkan12Features::shaderOutputViewportIndex);

        // Required for RenderAreaViewportType::MULTI_GEOM if used.
        caps.addFeature(&VkPhysicalDeviceFeatures::geometryShader);

        // Required for cases other than RenderAreaViewportType::SINGLE if used.
        caps.addFeature(&VkPhysicalDeviceFeatures::multiViewport);
    }

    void checkSupport(Context &context) const override
    {
        context.requireDeviceFunctionality("VK_QCOM_multiview_per_view_render_areas");

        if (m_params.viewportType == RenderAreasViewportType::MULTI_QCOM)
        {
            context.requireDeviceFunctionality("VK_QCOM_multiview_per_view_viewports");
        }
        else if (m_params.viewportType == RenderAreasViewportType::MULTI_VERT)
        {
            if (context.getUsedApiVersion() < VK_API_VERSION_1_2)
                TCU_THROW(NotSupportedError, "Vulkan version 1.2 required");
        }
        else if (m_params.viewportType == RenderAreasViewportType::MULTI_GEOM)
        {
            context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
            const auto &multiviewFeatures = context.getMultiviewFeatures();
            if (!multiviewFeatures.multiviewGeometryShader)
                TCU_THROW(NotSupportedError, "multiviewGeometryShader not supported");
        }

        if (m_params.viewportType != RenderAreasViewportType::SINGLE)
            context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_MULTI_VIEWPORT);

        if (m_params.useDynamicRendering())
            context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

        context.requireDeviceFunctionality("VK_KHR_create_renderpass2");
        context.requireDeviceFunctionality("VK_KHR_multiview");

        if (m_params.useMultiSample())
        {
            const auto ctx = context.getContextCommonData();
            VkImageFormatProperties formatProperties;
            ctx.vki.getPhysicalDeviceImageFormatProperties(ctx.physicalDevice, m_params.getColorFormat(),
                                                           m_params.getImageType(), VK_IMAGE_TILING_OPTIMAL,
                                                           m_params.getMultisampleColorUsage(), 0u, &formatProperties);
            if ((formatProperties.sampleCounts & VK_SAMPLE_COUNT_4_BIT) == 0)
                TCU_THROW(NotSupportedError, "Color format does not support 4 samples");
        }
    }

    void initPrograms(vk::SourceCollections &programCollection) const override;

    TestInstance *createInstance(Context &context) const override
    {
        return new RenderAreasInstance(context, m_params);
    }

protected:
    const RenderAreasParams m_params;
};

void RenderAreasCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 460\n";

    if (m_params.viewportType == RenderAreasViewportType::MULTI_VERT)
    {
        vert << "#extension GL_EXT_multiview : enable\n"
             << "#extension GL_ARB_shader_viewport_layer_array : enable\n";
    }

    vert << "out gl_PerVertex {\n"
         << "    vec4 gl_Position;\n"
         << "};\n"
         << "\n"
         << "void main() {\n"
         // Clockwise triangle strip from -0.5 to 0.5 in both axes.
         << "    const float x = float((gl_VertexIndex & 2)>>1) - 0.5;\n"
         << "    const float y = float (gl_VertexIndex & 1) - 0.5;\n"
         << "    gl_Position = vec4(x, y, 0.0, 1.0);\n";

    if (m_params.viewportType == RenderAreasViewportType::MULTI_VERT)
        vert << "    gl_ViewportIndex = gl_ViewIndex;\n";

    vert << "}\n";

    const auto spvOpts =
        ((m_params.viewportType == RenderAreasViewportType::MULTI_VERT) ?
             vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_5, 0u, false) :
             vk::ShaderBuildOptions());
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str()) << spvOpts;

    if (m_params.useGeometryShader())
    {
        std::ostringstream geom;
        geom << "#version 460\n"
             << "#extension GL_EXT_multiview : require\n"
             << "layout (triangles) in;\n"
             << "layout (triangle_strip, max_vertices=3) out;\n"
             << "in gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "} gl_in[3];\n"
             << "out gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "};\n"
             << "void main() {\n"
             << "    for (uint i = 0; i < 3; ++i) {\n"
             << "        gl_Position = gl_in[i].gl_Position;\n"
             << "        gl_ViewportIndex = gl_ViewIndex;\n"
             << "        EmitVertex();\n"
             << "    }\n"
             << "}\n";
        programCollection.glslSources.add("geom") << glu::GeometrySource(geom.str());
    }

    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main() {\n"
         << "    outColor = vec4" << m_params.getGeometryColor() << ";\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

tcu::TestStatus RenderAreasInstance::iterate(void)
{
    const auto ctx = m_context.getContextCommonData();

    const tcu::IVec3 extent(16, 16, 2);
    const auto extentU       = extent.asUint();
    const auto extentVk      = makeExtent3D(extentU.x(), extentU.y(), 1u);
    const auto layerCount    = extentU.z();
    const auto colorFormat   = m_params.getColorFormat();
    const auto colorUsageSS  = m_params.getSingleSampleColorUsage();
    const auto colorUsageMS  = m_params.getMultisampleColorUsage();
    const auto imageType     = m_params.getImageType();
    const auto bindPoint     = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto colorSRR      = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, layerCount);
    const auto attAccesses   = (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    const auto attStages     = (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    const auto ssSampleCount = VK_SAMPLE_COUNT_1_BIT;
    const auto msSampleCount = VK_SAMPLE_COUNT_4_BIT;

    // Multilayer color buffer, single sample.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, extentVk, colorFormat, colorUsageSS, imageType,
                                colorSRR, layerCount, ssSampleCount);

    // Multilayer color buffer, multisample.
    std::unique_ptr<ImageWithBuffer> colorBufferMS;
    if (m_params.useMultiSample())
        colorBufferMS.reset(new ImageWithBuffer(ctx.vkd, ctx.device, ctx.allocator, extentVk, colorFormat, colorUsageMS,
                                                imageType, colorSRR, layerCount, msSampleCount));

    // Framebuffer views.
    std::vector<VkImageView> fbViews;
    fbViews.reserve(2u);
    fbViews.push_back(colorBuffer.getImageView());
    if (m_params.useMultiSample())
        fbViews.push_back(colorBufferMS->getImageView());

    std::vector<uint32_t> subpassMasks;
    if (m_params.multiPass)
    {
        subpassMasks.push_back(0x01u);
        subpassMasks.push_back(0x02u);
    }
    else
    {
        subpassMasks.push_back(0x03u);
    }

    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> framebuffer;

    if (!m_params.useDynamicRendering())
    {
        std::vector<VkAttachmentDescription2> attDescriptions;
        attDescriptions.reserve(2u); // Single sample and maybe multisample.

        // Single sample.
        attDescriptions.push_back(VkAttachmentDescription2{
            VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
            nullptr,
            0u,
            colorFormat,
            ssSampleCount,
            m_params.ssLoadOp,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        });

        if (m_params.useMultiSample())
        {
            attDescriptions.push_back(VkAttachmentDescription2{
                VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
                nullptr,
                0u,
                colorFormat,
                msSampleCount,
                m_params.msLoadOp,
                VK_ATTACHMENT_STORE_OP_DONT_CARE,
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                VK_ATTACHMENT_STORE_OP_DONT_CARE,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            });
        }

        std::vector<VkAttachmentReference2> attReferences;
        attReferences.reserve(2u);

        // Single sample attachment reference.
        attReferences.push_back(VkAttachmentReference2{
            VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
            nullptr,
            0u,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT,
        });

        // Multisample attachment reference.
        if (m_params.useMultiSample())
        {
            attReferences.push_back(VkAttachmentReference2{
                VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
                nullptr,
                1u,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT,
            });
        }

        std::vector<VkSubpassDescription2> subpassDescriptions;
        subpassDescriptions.reserve(subpassMasks.size());

        const VkAttachmentReference2 *pResolveAttachment =
            (m_params.useMultiSample() ? &attReferences.front() : nullptr);

        for (const auto subpassMask : subpassMasks)
        {
            subpassDescriptions.emplace_back(VkSubpassDescription2{
                VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
                nullptr,
                0u,
                bindPoint,
                subpassMask,
                0u,
                nullptr,
                1u,
                &attReferences.back(), // The main attachment is always in the back of the list.
                pResolveAttachment,
                nullptr,
                0u,
                nullptr,
            });
        }

        std::vector<VkSubpassDependency2> subpassDependencies;

        // This is needed because otherwise we have an image layout transition race.
        if (subpassMasks.size() > 1)
        {
            subpassDependencies.push_back(VkSubpassDependency2{
                VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
                nullptr,
                0u,
                1u,
                attStages,
                attStages,
                attAccesses,
                attAccesses,
                VK_DEPENDENCY_BY_REGION_BIT,
                0u,
            });
        }

        const VkRenderPassCreateInfo2 rpCreateInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
            nullptr,
            0u,
            de::sizeU32(attDescriptions),
            de::dataOrNull(attDescriptions),
            de::sizeU32(subpassDescriptions),
            de::dataOrNull(subpassDescriptions),
            de::sizeU32(subpassDependencies),
            de::dataOrNull(subpassDependencies),
            0u,
            nullptr,
        };

        renderPass = createRenderPass2(ctx.vkd, ctx.device, &rpCreateInfo);
        // Note with multiview the framebuffer layer count is specified as 1.
        framebuffer = makeFramebuffer(ctx.vkd, ctx.device, *renderPass, de::sizeU32(fbViews), de::dataOrNull(fbViews),
                                      extentU.x(), extentU.y(), 1u);
    }

    // Pipeline vertex input state.
    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();

    // Viewport state, using a single viewport/scissor or separte ones.
    const auto viewports = m_params.getViewports();
    const auto scissors  = m_params.getScissors();

    PipelineLayoutWrapper pipelineLayout(m_params.groupParams->pipelineConstructionType, ctx.vkd,
                                         ctx.device); //, VK_NULL_HANDLE, &pcRange);

    const auto &binaries = m_context.getBinaryCollection();
    ShaderWrapper vertShader(ctx.vkd, ctx.device, binaries.get("vert"));
    ShaderWrapper fragShader(ctx.vkd, ctx.device, binaries.get("frag"));
    ShaderWrapper geomShader =
        (m_params.useGeometryShader() ? ShaderWrapper(ctx.vkd, ctx.device, binaries.get("geom")) : ShaderWrapper());

    const auto pipelineCount = de::sizeU32(subpassMasks); // One pipeline for each mask.
    using PipelinePtr        = std::unique_ptr<GraphicsPipelineWrapper>;
    std::vector<PipelinePtr> pipelines;
    pipelines.reserve(pipelineCount);

    std::unique_ptr<VkPipelineRenderingCreateInfo> pRenderingCreateInfo;
    if (m_params.useDynamicRendering())
    {
        pRenderingCreateInfo.reset(new VkPipelineRenderingCreateInfo{
            VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            nullptr,
            0u, // Needs to be changed for each pipeline.
            1u,
            &colorFormat,
            VK_FORMAT_UNDEFINED,
            VK_FORMAT_UNDEFINED,
        });
    }

    const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        0u,
        (m_params.useMultiSample() ? msSampleCount : ssSampleCount),
        VK_FALSE,
        0.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE,
    };

    for (uint32_t i = 0u; i < pipelineCount; ++i)
    {
        pipelines.emplace_back(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                                           m_context.getDeviceExtensions(),
                                                           m_params.groupParams->pipelineConstructionType));
        auto &pipeline = *pipelines.back();

        // Appropriate view mask for each pass.
        if (m_params.useDynamicRendering())
            pRenderingCreateInfo->viewMask = subpassMasks.at(i);

        pipeline.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            .setDefaultRasterizationState()
            .setDefaultColorBlendState()
            .setDefaultViewportsCount()
            .setDefaultScissorsCount()
            .setDefaultDepthStencilState()
            .setupVertexInputState(&vertexInputStateCreateInfo)
            .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, i, vertShader, nullptr,
                                              ShaderWrapper(), ShaderWrapper(), geomShader, nullptr, nullptr,
                                              pRenderingCreateInfo.get())
            .setupFragmentShaderState(pipelineLayout, *renderPass, i, fragShader, nullptr, &multisampleStateCreateInfo)
            .setupFragmentOutputState(*renderPass, i, nullptr, &multisampleStateCreateInfo)
            .buildPipeline();
    }

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const auto drawQuad = [&]() { ctx.vkd.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u); };

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    {
        // We first need to clear the images using a full-layer clear to the right colors.
        const auto srcStage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        const auto srcAccess = 0u;
        const auto dstStage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
        const auto dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;

        std::vector<VkImageMemoryBarrier> barriers;
        barriers.reserve(2u);

        const auto oldLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
        const auto clearLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        barriers.push_back(
            makeImageMemoryBarrier(srcAccess, dstAccess, oldLayout, clearLayout, colorBuffer.getImage(), colorSRR));
        if (m_params.useMultiSample())
            barriers.push_back(makeImageMemoryBarrier(srcAccess, dstAccess, oldLayout, clearLayout,
                                                      colorBufferMS->getImage(), colorSRR));

        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, srcStage, dstStage, de::dataOrNull(barriers),
                                      barriers.size());

        for (uint32_t layer = 0u; layer < layerCount; ++layer)
        {
            const auto clearColor = makeClearValueColor(m_params.getClearColor(true, false, layer));
            const auto clearSRR   = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, layer, 1u);
            ctx.vkd.cmdClearColorImage(cmdBuffer, colorBuffer.getImage(), clearLayout, &clearColor.color, 1u,
                                       &clearSRR);
        }

        if (m_params.useMultiSample())
        {
            for (uint32_t layer = 0u; layer < layerCount; ++layer)
            {
                const auto clearColor = makeClearValueColor(m_params.getClearColor(false, false, layer));
                const auto clearSRR   = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, layer, 1u);
                ctx.vkd.cmdClearColorImage(cmdBuffer, colorBufferMS->getImage(), clearLayout, &clearColor.color, 1u,
                                           &clearSRR);
            }
        }
    }
    {
        // Now we sync clears with render pass ops, and take the chance to modify layouts to color att optimal.
        const auto srcStage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
        const auto srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
        const auto dstStage  = attStages;
        const auto dstAccess = attAccesses;

        std::vector<VkImageMemoryBarrier> barriers;
        barriers.reserve(2u);

        const auto oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        const auto rpLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        barriers.push_back(
            makeImageMemoryBarrier(srcAccess, dstAccess, oldLayout, rpLayout, colorBuffer.getImage(), colorSRR));
        if (m_params.useMultiSample())
            barriers.push_back(
                makeImageMemoryBarrier(srcAccess, dstAccess, oldLayout, rpLayout, colorBufferMS->getImage(), colorSRR));

        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, srcStage, dstStage, de::dataOrNull(barriers),
                                      barriers.size());
    }

    const auto renderAreas      = m_params.getRenderAreas();
    const auto globalRenderArea = makeRect2D(extent.swizzle(0, 1));

#ifndef CTS_USES_VULKANSC
    const VkMultiviewPerViewRenderAreasRenderPassBeginInfoQCOM perViewRenderAreas = {
        VK_STRUCTURE_TYPE_MULTIVIEW_PER_VIEW_RENDER_AREAS_RENDER_PASS_BEGIN_INFO_QCOM,
        nullptr,
        de::sizeU32(renderAreas),
        de::dataOrNull(renderAreas),
    };
    const auto renderingPNext = &perViewRenderAreas;
#else
    const void *renderingPNext = nullptr;
#endif // CTS_USES_VULKANSC

    if (m_params.useDynamicRendering())
    {
#ifndef CTS_USES_VULKANSC
        // With dynamic rendering, we cannot specify the single-sample and multisample load ops separately.
        // VkRenderingAttachmentInfo uses a single value for both. Same for the clear value.
        if (m_params.useMultiSample())
            DE_ASSERT(m_params.ssLoadOp == m_params.msLoadOp);

        const VkRenderingAttachmentInfo colorAttInfo = {
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            nullptr,
            fbViews.back(), // The main attachment is always in the back of the attachment list.
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_RESOLVE_MODE_AVERAGE_BIT,
            (m_params.useMultiSample() ? fbViews.front() : VK_NULL_HANDLE),
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            m_params.ssLoadOp,
            VK_ATTACHMENT_STORE_OP_STORE,
            makeClearValueColor(m_params.getClearColor(
                !m_params.useMultiSample(), true,
                0u)), // The clear color will come from the MS if needed to make the case similar to the classic render pass.
        };

        const VkRenderingInfo renderingInfo = {
            VK_STRUCTURE_TYPE_RENDERING_INFO,
            renderingPNext,
            0u,
            globalRenderArea,
            1u,
            subpassMasks.front(),
            1u,
            &colorAttInfo,
            nullptr,
            nullptr,
        };

        ctx.vkd.cmdBeginRendering(cmdBuffer, &renderingInfo);
        pipelines.front()->bind(cmdBuffer);
        drawQuad();
        endRendering(ctx.vkd, cmdBuffer);
        if (pipelines.size() > 1)
        {
            {
                // Sync writes from one render pass to the next one.
                const auto srcStages = attStages;
                const auto srcAccess = attAccesses;
                const auto dstStages = attStages;
                const auto dstAccess = attAccesses;

                const auto barrier = makeMemoryBarrier(srcAccess, dstAccess);
                cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, srcStages, dstStages, &barrier);
            }

            const VkRenderingAttachmentInfo colorAttInfo2 = {
                VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                nullptr,
                fbViews.back(), // The main attachment is always in the back of the attachment list.
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_RESOLVE_MODE_AVERAGE_BIT,
                (m_params.useMultiSample() ? fbViews.front() : VK_NULL_HANDLE),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                m_params.ssLoadOp,
                VK_ATTACHMENT_STORE_OP_STORE,
                makeClearValueColor(m_params.getClearColor(
                    !m_params.useMultiSample(), true,
                    0u)), // The clear color will come from the MS if needed to make the case similar to the classic render pass.
            };

            const VkRenderingInfo renderingInfo2 = {
                VK_STRUCTURE_TYPE_RENDERING_INFO,
                renderingPNext,
                0u,
                globalRenderArea,
                1u,
                subpassMasks.back(), // Change in subpass mask.
                1u,
                &colorAttInfo2, // Change in the attachment load op.
                nullptr,
                nullptr,
            };

            ctx.vkd.cmdBeginRendering(cmdBuffer, &renderingInfo2);
            pipelines.back()->bind(cmdBuffer);
            drawQuad();
            endRendering(ctx.vkd, cmdBuffer);
        }
#endif // CTS_USES_VULKANSC
    }
    else
    {
        std::vector<VkClearValue> clearValues;
        clearValues.reserve(2u);
        clearValues.push_back(makeClearValueColor(m_params.getClearColor(true, true, 0u)));
        if (m_params.useMultiSample())
            clearValues.push_back(makeClearValueColor(m_params.getClearColor(false, true, 0u)));

        const VkRenderPassBeginInfo rpBeginInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            renderingPNext,
            *renderPass,
            *framebuffer,
            globalRenderArea,
            de::sizeU32(clearValues),
            de::dataOrNull(clearValues),
        };

        ctx.vkd.cmdBeginRenderPass(cmdBuffer, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        pipelines.front()->bind(cmdBuffer);
        drawQuad();
        if (pipelines.size() > 1)
        {
            ctx.vkd.cmdNextSubpass(cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
            pipelines.back()->bind(cmdBuffer);
            drawQuad();
        }
        endRenderPass(ctx.vkd, cmdBuffer);
    }

    {
        // Copy single-sample color buffer to verification buffer.
        copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), extent.swizzle(0, 1),
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, layerCount);
    }

    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    invalidateAlloc(ctx.vkd, ctx.device, colorBuffer.getBufferAllocation());
    const auto tcuFormat = mapVkFormat(colorFormat);

    tcu::ConstPixelBufferAccess resultAccess(tcuFormat, extent, colorBuffer.getBufferAllocation().getHostPtr());
    tcu::TextureLevel referenceLevel(tcuFormat, extent.x(), extent.y(), extent.z());
    tcu::PixelBufferAccess referenceAccess = referenceLevel.getAccess();

    const auto geomColor = m_params.getGeometryColor(); // Color of the geometry.
    auto &log            = m_context.getTestContext().getLog();
    bool fail            = false;

    for (int z = 0; z < extent.z(); ++z)
    {
        // Verify each layer separately.
        tcu::ConstPixelBufferAccess resultLayer = tcu::getSubregion(resultAccess, 0, 0, z, extent.x(), extent.y(), 1);
        tcu::PixelBufferAccess referenceLayer = tcu::getSubregion(referenceAccess, 0, 0, z, extent.x(), extent.y(), 1);

        // Prepare reference layer.
        tcu::Vec4 bgColor(0.0f); // General background color for the clear outside the render pass.
        tcu::Vec4 rpColor(0.0f); // Render pass background color.

        // The general background will always come from the single-sample general clear color.
        bgColor = m_params.getClearColor(true, false, static_cast<uint32_t>(z));

        if (m_params.useMultiSample())
        {
            if (m_params.msLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
            {
                // The in-render-area clear color will come from the MS in-render-pass clear color.
                rpColor = m_params.getClearColor(false, true, 0u);
            }
            else if (m_params.msLoadOp == VK_ATTACHMENT_LOAD_OP_LOAD)
            {
                // The in-render-area clear color will come from the MS general clear color due to the load.
                rpColor = m_params.getClearColor(false, false, static_cast<uint32_t>(z));
            }
            else
                DE_ASSERT(false);
        }
        else
        {
            if (m_params.ssLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
            {
                // The in-render-area clear color will come from the single-sample in-render-pass clear color.
                rpColor = m_params.getClearColor(true, true, 0u);
            }
            else if (m_params.ssLoadOp == VK_ATTACHMENT_LOAD_OP_LOAD)
            {
                // The in-render-area clera color will come from the single-sample general clear color.
                rpColor = m_params.getClearColor(true, false, static_cast<uint32_t>(z));
            }
        }

        // Once the colors have been established, we clear the framebuffer with the general background color; then, we
        // find the render area for the given view and clear it to the render pass background color. Then, we use the
        // viewport information to decide where to draw the geometry inside it.
        tcu::clear(referenceLayer, bgColor);

        const auto renderArea = renderAreas.at(z);
        const auto renderAreaAccess =
            tcu::getSubregion(referenceLayer, renderArea.offset.x, renderArea.offset.y,
                              static_cast<int>(renderArea.extent.width), static_cast<int>(renderArea.extent.height));
        tcu::clear(renderAreaAccess, rpColor);

        // The geometry is a square in the middle of the viewport (from -0.5 to 0.5).
        const auto &viewport = viewports.at(std::min(static_cast<size_t>(z), viewports.size() - 1));
        const auto viewportAccess =
            tcu::getSubregion(referenceLayer, static_cast<int>(viewport.x), static_cast<int>(viewport.y),
                              static_cast<int>(viewport.width), static_cast<int>(viewport.height));
        const auto geomAreaAccess =
            tcu::getSubregion(viewportAccess, viewportAccess.getWidth() / 4, viewportAccess.getHeight() / 4,
                              viewportAccess.getWidth() / 2, viewportAccess.getHeight() / 2);
        tcu::clear(geomAreaAccess, geomColor);

        const std::string setName = "Layer" + std::to_string(z);
        const tcu::Vec4 threshold(0.0f); // Exact results.

        if (!tcu::floatThresholdCompare(log, setName.c_str(), "", referenceLayer, resultLayer, threshold,
                                        tcu::COMPARE_LOG_ON_ERROR))
            fail = true;
    }

    if (fail)
        TCU_FAIL("Unexpected results in color buffer; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

std::string getRenderAreasViewportCaseName(RenderAreasViewportType viewportType)
{
    if (viewportType == RenderAreasViewportType::MULTI_QCOM)
        return "multi_viewport_qcom";
    if (viewportType == RenderAreasViewportType::MULTI_VERT)
        return "multi_viewport_vert";
    if (viewportType == RenderAreasViewportType::MULTI_GEOM)
        return "multi_viewport_geom";
    if (viewportType == RenderAreasViewportType::SINGLE)
        return "single_viewport";

    DE_ASSERT(false);
    return "";
}

std::string getAttachmentLoadOpSuffix(VkAttachmentLoadOp loadOp)
{
    if (loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
        return "clear";
    if (loadOp == VK_ATTACHMENT_LOAD_OP_LOAD)
        return "load";
    return "";
}

} // anonymous namespace

tcu::TestCaseGroup *createRenderPassMultiviewPerViewTests(tcu::TestContext &testCtx,
                                                          const SharedGroupParams groupParams)
{
    // Unused attachment tests
    using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "multiview_per_view"));
    GroupPtr viewportsGroup(new tcu::TestCaseGroup(testCtx, "viewports"));
    GroupPtr renderAreasGroup(new tcu::TestCaseGroup(testCtx, "render_areas"));

    for (const auto viewportDynState : {DynamicState::NO, DynamicState::YES, DynamicState::YES_COUNT})
        for (const auto scissorDynState : {DynamicState::NO, DynamicState::YES, DynamicState::YES_COUNT})
            for (const auto diffFlag : {ViewportDiffFlags{DIFF_FLAGS_OFFSET}, ViewportDiffFlags{DIFF_FLAGS_SIZE},
                                        ViewportDiffFlags{DIFF_FLAGS_DEPTH},
                                        ViewportDiffFlags{DIFF_FLAGS_OFFSET | DIFF_FLAGS_SIZE | DIFF_FLAGS_DEPTH}})
                for (const bool multiPass : {false, true})
                {
                    const ViewportsParams params{
                        groupParams, viewportDynState, scissorDynState, diffFlag, multiPass,
                    };
                    const auto testName = std::string("viewport") + getDynStateSuffix(viewportDynState) + "_scissor" +
                                          getDynStateSuffix(scissorDynState) + "_vary" +
                                          getViewportDiffSuffix(params.viewportDiffFlags) +
                                          (multiPass ? "_multipass" : "");
                    viewportsGroup->addChild(new ViewportsCase(testCtx, testName, params));
                }
    mainGroup->addChild(viewportsGroup.release());

    for (const auto viewportType : {RenderAreasViewportType::SINGLE, RenderAreasViewportType::MULTI_QCOM,
                                    RenderAreasViewportType::MULTI_GEOM, RenderAreasViewportType::MULTI_VERT})
        for (const auto ssLoadOp : {VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_LOAD})
            for (const auto msLoadOp :
                 {VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_LOAD})
                for (const bool multiPass : {false, true})
                {
                    const RenderAreasParams params{groupParams, viewportType, ssLoadOp, msLoadOp, multiPass};

                    // This cannot be done directly with dynamic rendering.
                    if (params.useDynamicRendering() && ssLoadOp != msLoadOp)
                        continue;

                    const auto testName = getRenderAreasViewportCaseName(params.viewportType) + "_ss_" +
                                          getAttachmentLoadOpSuffix(params.ssLoadOp) +
                                          ((params.msLoadOp != VK_ATTACHMENT_LOAD_OP_DONT_CARE) ?
                                               "_ms_" + getAttachmentLoadOpSuffix(params.msLoadOp) :
                                               "") +
                                          (multiPass ? "_multipass" : "");
                    renderAreasGroup->addChild(new RenderAreasCase(testCtx, testName, params));
                }
    mainGroup->addChild(renderAreasGroup.release());

    return mainGroup.release();
}

} // namespace renderpass
} // namespace vkt
