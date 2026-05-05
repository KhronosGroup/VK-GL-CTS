/*-------------------------------------------------------------------------
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
 * \brief Test coverage for depth optimizations like LRZ (Adreno GPUs) and
 * EZ (Broadcom GPUs). It is based on tests for LRZ and has comments
 * talking about LRZ implementation details, which are left to give
 * a context on the intent of tests.
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassLowResolutionZTests.hpp"
#include "vkPipelineConstructionUtil.hpp"
#include "vktRenderPassTestsUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"

#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"

#include "deSharedPtr.hpp"
#include "deRandom.hpp"

#include <type_traits>
#include <functional>
#include <variant>
#include <utility>
#include <string>
#include <cmath>
#include <set>
#include <map>

using namespace vk;

namespace vkt::renderpass
{
namespace
{

// Define all fragment shader variants used in tests
enum class FS
{
    BASIC = 0,
    DEPTH_GREATER,
    DEPTH_LESS,
    DEPTH_UNCHANGED,
    DEPTH_GREATER_OVERRIDE,
    DEPTH_LESS_OVERRIDE,
    WRITE_DEPTH_OVERRIDE,
    EARLY_FRAG_TESTS,
    DISCARD,
    ATOMIC,
    IMAGE_STORE,
    LAST // used only for fuzz tests
};

// Helper map to easily get shader name from FS enum
const std::map<FS, std::string> fragShaderToName{
    {FS::BASIC, "basic-frag"},
    {FS::DEPTH_GREATER, "depth-greater-frag"},
    {FS::DEPTH_LESS, "depth-less-frag"},
    {FS::DEPTH_UNCHANGED, "depth-unchanged-frag"},
    {FS::DEPTH_GREATER_OVERRIDE, "depth-greater-override-frag"},
    {FS::DEPTH_LESS_OVERRIDE, "depth-less-override-frag"},
    {FS::WRITE_DEPTH_OVERRIDE, "write-depth-override-frag"},
    {FS::EARLY_FRAG_TESTS, "early-frag-tests-frag"},
    {FS::DISCARD, "discard-frag"},
    {FS::ATOMIC, "atomic-frag"},
    {FS::IMAGE_STORE, "image-store-frag"},
};

// Helper enums to make test steps more readable
enum class DepthTest
{
    DISABLE = 0,
    ENABLE
};

enum class DepthWrite
{
    DISABLE = 0,
    ENABLE
};

enum class StencilTest
{
    DISABLE = 0,
    ENABLE
};

enum class Blend
{
    DISABLE = 0,
    ENABLE
};

enum class AlphaToCoverage
{
    DISABLE = 0,
    ENABLE
};

enum class DrawMode
{
    FULL_QUAD = 0,
    EVEN_ROWS,
    ODD_ROWS
};

enum class TestStepType
{
    BEGIN_RENDER_PASS = 0,
    END_RENDER_PASS,
    SUSPEND_RESUME_RENDERING,
    BIND_PIPELINE,
    DRAW,
    CLEAR_ATTACHMENT,
    CLEAR_IMAGE,
    BLIT,
    BEGIN_SECONDARY,
    END_SECONDARY,
    COPY_BUFFER,
    COPY_IMAGE,
    BARRIER // note: barrier steps are added automatically
};

// Define *Data structs to store data relevant for each type of test step

struct BindPipelineData
{
    FS frag;

    DepthTest depthTest;
    DepthWrite depthWrite;
    VkCompareOp depthCompareOp;

    StencilTest stencilTest;
    VkCompareOp stencilCompareOp;
    VkStencilOp stencilPassOp;
    VkStencilOp stencilFailOp;
    VkStencilOp stencilDepthFailOp;
    uint32_t stencilCompareMask;
    uint32_t stencilWriteMask;
    uint32_t stencilReference;

    Blend blend;
    VkColorComponentFlags colorWriteMask;

    AlphaToCoverage alphaToCoverage;
};

struct BeginRenderPassData
{
    VkAttachmentLoadOp depthLoadOp;
    VkAttachmentStoreOp depthStoreOp;
    float depthClearValue;
    uint32_t stencilClearValue;
    uint32_t depthImageIndex; // if depthImageIndex is non 0 then depthImageLayer is set to 0
    uint32_t depthImageLayer; // specifies which layer of first image should be used for attachment
};

struct DrawData
{
    // note draw data is also used for cmdPushConstants
    tcu::Vec4 color;
    float depth;
    float height;
    DrawMode mode;
};

struct ClearBlitCopyData
{
    float depth;
    uint32_t depthImageIndex;
    uint32_t depthImageLayer;
};

struct BarrierData
{
    TestStepType prevStep;
    TestStepType nextStep;
};

// Test step represent part of operations done in a test; test consist of multiple steps
using TestStepVariant = std::variant<BeginRenderPassData, BindPipelineData, DrawData, ClearBlitCopyData, BarrierData>;
struct TestStep
{
    TestStepType type;
    TestStepVariant data;

    TestStep(TestStepType type_, TestStepVariant data_ = {}) : type(type_), data(data_)
    {
    }
};

// Test parameters represent the configuration of a test
struct TestParams
{
    std::string name;
    std::vector<TestStep> stepVect;
};

// Helper functions to create test steps with less boilerplate
namespace StepHelpers
{
static TestStep bindPipeline(VkCompareOp depthCompareOp, FS frag = FS::BASIC, DepthTest depthTest = DepthTest::ENABLE,
                             DepthWrite depthWrite = DepthWrite::ENABLE, StencilTest stencilTest = StencilTest::DISABLE,
                             VkCompareOp stencilCompareOp = VK_COMPARE_OP_GREATER,
                             VkStencilOp stencilPassOp    = VK_STENCIL_OP_ZERO,
                             VkStencilOp stencilFailOp    = VK_STENCIL_OP_ZERO,
                             VkStencilOp depthFailOp = VK_STENCIL_OP_ZERO, uint32_t stencilCompareMask = 0xFF,
                             uint32_t stencilWriteMask = 0xFF, uint32_t stencilReference = 0,
                             Blend blend = Blend::DISABLE, VkColorComponentFlags colorWriteMask = 0xF,
                             AlphaToCoverage alphaToCoverage = AlphaToCoverage::DISABLE)
{
    return {TestStepType::BIND_PIPELINE,
            BindPipelineData{frag, depthTest, depthWrite, depthCompareOp, stencilTest, stencilCompareOp, stencilPassOp,
                             stencilFailOp, depthFailOp, stencilCompareMask, stencilWriteMask, stencilReference, blend,
                             colorWriteMask, alphaToCoverage}};
}

static TestStep bindBlendPipeline(VkCompareOp depthCompareOp, DepthWrite depthWrite, Blend blend,
                                  VkColorComponentFlags colorWriteMask = 0xF,
                                  AlphaToCoverage alphaToCoverage      = AlphaToCoverage::DISABLE)
{
    return {TestStepType::BIND_PIPELINE,
            BindPipelineData{FS::BASIC, DepthTest::ENABLE, depthWrite, depthCompareOp, StencilTest::DISABLE,
                             VK_COMPARE_OP_GREATER, VK_STENCIL_OP_ZERO, VK_STENCIL_OP_ZERO, VK_STENCIL_OP_ZERO, 0xFF,
                             0xFF, 0, blend, colorWriteMask, alphaToCoverage}};
}

static TestStep beginRenderPass(VkAttachmentLoadOp depthLoadOp   = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                float depthClearValue            = 1.0f,
                                VkAttachmentStoreOp depthStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
                                uint32_t stencilClearValue = 0, uint32_t depthImageIndex = 0,
                                uint32_t depthImageLayer = 0)
{
    if (depthImageIndex)
        depthImageLayer = 0;

    return {TestStepType::BEGIN_RENDER_PASS, BeginRenderPassData{depthLoadOp, depthStoreOp, depthClearValue,
                                                                 stencilClearValue, depthImageIndex, depthImageLayer}};
}

static TestStep endRenderPass(void)
{
    return {TestStepType::END_RENDER_PASS};
}

static TestStep suspendResumeRendering(void)
{
    // note: this step applies only to dynamic rendering tests
    return {TestStepType::SUSPEND_RESUME_RENDERING};
}

static TestStep draw(tcu::Vec4 color, float depth, DrawMode mode = DrawMode::FULL_QUAD)
{
    return {TestStepType::DRAW, DrawData{color, depth, 1.0f, mode}};
}

static TestStep clearDepthAttachment(float depth)
{
    return {TestStepType::CLEAR_ATTACHMENT, ClearBlitCopyData{depth}};
}

static TestStep clearDepthImage(float depth)
{
    return {TestStepType::CLEAR_IMAGE, ClearBlitCopyData{depth}};
}

static TestStep blitDepth(float depth)
{
    return {TestStepType::BLIT, ClearBlitCopyData{depth}};
}

static TestStep copyBuffer(float depth)
{
    return {TestStepType::COPY_BUFFER, ClearBlitCopyData{depth}};
}

static TestStep copyImage(float depth)
{
    return {TestStepType::COPY_IMAGE, ClearBlitCopyData{depth}};
}

static TestStep beginSecondary(void)
{
    return {TestStepType::BEGIN_SECONDARY};
}

static TestStep endSecondary(void)
{
    return {TestStepType::END_SECONDARY};
}

} // namespace StepHelpers

VkImageCreateInfo makeImageCreateInfo(VkFormat format, uint32_t size, VkImageUsageFlags usage, uint32_t arrayLayers = 1)
{
    VkImageCreateInfo imageCreateInfo = initVulkanStructure();
    imageCreateInfo.imageType         = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format            = format;
    imageCreateInfo.extent            = {size, size, 1};
    imageCreateInfo.mipLevels         = 1;
    imageCreateInfo.arrayLayers       = arrayLayers;
    imageCreateInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.usage             = usage;
    return imageCreateInfo;
}

// number of depth image used in tests
const uint32_t DEPTH_IMAGE_COUNT = 3;

// number of depth image layers available in first d/s image
const uint32_t DEPTH_LAYER_COUNT = 3;

// Tests operate on large images; to avoid recreating images for each instance, share them
struct SharedData
{
    const VkFormat colorFormat       = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat depthStencilFormat      = VK_FORMAT_D24_UNORM_S8_UINT;
    const VkDeviceSize outBufferSize = 2 * sizeof(tcu::Vec4) + 2 * sizeof(uint32_t);

    std::shared_ptr<ImageWithBuffer> colorBuffer;
    std::shared_ptr<ImageWithMemory> depthStencilBuffer[DEPTH_IMAGE_COUNT];
    std::shared_ptr<ImageWithMemory> copyImage;
    std::shared_ptr<ImageWithMemory> storeImage;

    std::shared_ptr<BufferWithMemory> copyBuffer;
    std::shared_ptr<BufferWithMemory> outBuffer;

    Move<VkImageView> depthStencilViews[DEPTH_IMAGE_COUNT][DEPTH_LAYER_COUNT];
    Move<VkImageView> storeView;
};
std::unique_ptr<SharedData> g_sharedData;

void setupSharedData(Context &context, uint32_t imageSize)
{
    if (g_sharedData)
        return;

    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();
    const auto &vk            = context.getDeviceInterface();
    auto device               = context.getDevice();
    auto &allocator           = context.getDefaultAllocator();

    const VkExtent3D imageExtent = {imageSize, imageSize, 1};
    auto dsIsr = makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 1u, 0u, 1u);

    g_sharedData = std::make_unique<SharedData>();

    // check which of required DS formats is supported
    VkFormatProperties formatProperties;
    VkFormatFeatureFlags feature = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    vki.getPhysicalDeviceFormatProperties(physicalDevice, g_sharedData->depthStencilFormat, &formatProperties);
    if ((formatProperties.optimalTilingFeatures & feature) != feature)
    {
        g_sharedData->depthStencilFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
        vki.getPhysicalDeviceFormatProperties(physicalDevice, g_sharedData->depthStencilFormat, &formatProperties);
    }

    // if format supports transfer destination add it to usage as some tests need it
    VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    feature                      = VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    if (formatProperties.optimalTilingFeatures & feature)
        imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // define images for depth/stencil attachment (first one hase <DEPTH_LAYER_COUNT> layers)
    auto dsCreateInfo = makeImageCreateInfo(g_sharedData->depthStencilFormat, imageSize, imageUsage, DEPTH_LAYER_COUNT);
    for (uint32_t i = 0; i < DEPTH_IMAGE_COUNT; ++i)
    {
        g_sharedData->depthStencilBuffer[i] =
            std::make_shared<ImageWithMemory>(vk, device, allocator, dsCreateInfo, MemoryRequirement::Any);

        // all images except the first one have one layer, to not increase complexity unnecessarily
        dsCreateInfo.arrayLayers = 1;
    }

    // define image view for layer 0 of each of depth/stencil image
    for (uint32_t i = 0; i < DEPTH_IMAGE_COUNT; i++)
    {
        g_sharedData->depthStencilViews[i][0] =
            makeImageView(vk, device, **g_sharedData->depthStencilBuffer[i], VK_IMAGE_VIEW_TYPE_2D,
                          g_sharedData->depthStencilFormat, dsIsr);
    }

    // define image view for each layer of first depth/stencil image (we omit first layer as we created view for it)
    for (uint32_t i = 1; i < DEPTH_LAYER_COUNT; i++)
    {
        dsIsr.baseArrayLayer = i;
        g_sharedData->depthStencilViews[0][i] =
            makeImageView(vk, device, **g_sharedData->depthStencilBuffer[0], VK_IMAGE_VIEW_TYPE_2D,
                          g_sharedData->depthStencilFormat, dsIsr);
    }
    dsIsr.baseArrayLayer = 0;

    // define image for color attachment
    imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    g_sharedData->colorBuffer = std::make_shared<ImageWithBuffer>(
        vk, device, allocator, imageExtent, g_sharedData->colorFormat, imageUsage, VK_IMAGE_TYPE_2D);

    // define image for copy operations
    dsCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    g_sharedData->copyImage =
        std::make_shared<ImageWithMemory>(vk, device, allocator, dsCreateInfo, MemoryRequirement::Any);

    // define image for store operations
    imageUsage           = VK_IMAGE_USAGE_STORAGE_BIT;
    auto colorCreateInfo = makeImageCreateInfo(g_sharedData->colorFormat, 4, imageUsage, 1);
    auto colorIsr        = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    g_sharedData->storeImage =
        std::make_shared<ImageWithMemory>(vk, device, allocator, colorCreateInfo, MemoryRequirement::Any);
    g_sharedData->storeView = makeImageView(vk, device, **g_sharedData->storeImage, VK_IMAGE_VIEW_TYPE_2D,
                                            g_sharedData->colorFormat, colorIsr);

    // define buffer for copy operations
    auto bufferUsage                = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    const uint32_t copyBufferSize   = imageSize * imageSize * 5; // enough to store data for D32_SFLOAT_S8_UINT
    const auto copyBufferCreateInfo = makeBufferCreateInfo(copyBufferSize, bufferUsage);
    g_sharedData->copyBuffer =
        std::make_shared<BufferWithMemory>(vk, device, allocator, copyBufferCreateInfo, MemoryRequirement::HostVisible);

    // define buffer used for cases with atomic operations and for verification with compute shader
    bufferUsage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    const auto outBufferCreateInfo = makeBufferCreateInfo(g_sharedData->outBufferSize, bufferUsage);
    g_sharedData->outBuffer =
        std::make_shared<BufferWithMemory>(vk, device, allocator, outBufferCreateInfo, MemoryRequirement::HostVisible);
}

class LRZTestInstance : public vkt::TestInstance
{
public:
    LRZTestInstance(Context &context, const TestParams &testParams, const SharedGroupParams groupParams);
    virtual ~LRZTestInstance(void) = default;

    virtual tcu::TestStatus iterate(void) override;

protected:
    struct FramebufferState
    {
        tcu::Vec4 color;
        float depth[DEPTH_IMAGE_COUNT][DEPTH_LAYER_COUNT];
        uint32_t stencil;
    };

    void preprocessSteps();

    tcu::Vec4 calculateExpectedColor(uint32_t y) const;
    void simulateDraw(FramebufferState &fb, uint32_t depthImageIndex, uint32_t depthImageLayer,
                      const BindPipelineData &pipeline, const DrawData &dd) const;
    uint32_t applyStencilOp(VkStencilOp op, uint32_t cur, uint32_t ref, uint32_t writeMask) const;

    template <typename T>
    bool doesCompareOpPass(VkCompareOp op, T ref, T val) const;

    const PipelineLayoutWrapper &getGraphicsPipelineLayout(FS frag);
    void preparePipelines(VkRenderPass renderPass);

    template <uint32_t Version>
    Move<VkRenderPass> makeRenderPass(const VkAttachmentLoadOp loadOperation) const;

    void beginRendering(const DeviceInterface &vk, const VkCommandBuffer cmdBuffer,
                        const std::vector<Move<VkRenderPass>> &renderPassVect, const VkFramebuffer framebuffer,
                        const VkRect2D &renderArea, uint32_t flags, VkAttachmentLoadOp depthLoadOp, float depthClear,
                        uint32_t stencilClear, uint32_t depthImage, uint32_t depthLayer) const;
    void endRendering(const DeviceInterface &vk, const VkCommandBuffer cmdBuffer) const;

private:
    const TestParams &m_testParams;
    GroupParams m_groupParams;

    std::vector<float> m_blitDepthsVect;
    std::vector<const BindPipelineData *> m_pipelineDataVect;
    std::vector<int32_t> m_suspendResumeCountPerRenderingVect;
    std::vector<TestStep> m_processedStepVect;

    uint32_t m_highestUsedDepthImageIndex                 = 0;
    uint32_t m_highestUsedDepthImageLayer                 = 0;
    bool m_hasRenderPassWithLoadOpLoad                    = false;
    std::size_t m_numberOfRequiredSecondaryCommandBuffers = 0ULL;
    bool m_hasCustomSecondary                             = false;

    const uint32_t m_imageSize     = 1024;
    const VkExtent3D m_imageExtent = {m_imageSize, m_imageSize, 1};

    // define subresource ranges for color and depth/stencil attachments
    const VkImageAspectFlags m_dsAspect      = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    const VkImageSubresourceRange m_colorIsr = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const VkImageSubresourceRange m_dsIsr    = makeImageSubresourceRange(m_dsAspect, 0u, 1u, 0u, 1u);

    std::vector<PipelineLayoutWrapper> m_graphicsPipelineLayoutVect;
    std::vector<GraphicsPipelineWrapper> m_pipelineVect;
};

LRZTestInstance::LRZTestInstance(Context &context, const TestParams &testParams, const SharedGroupParams groupParams)
    : vkt::TestInstance(context)
    , m_testParams(testParams)
    , m_groupParams(*groupParams)
{
    setupSharedData(context, m_imageSize);

#ifndef CTS_USES_VULKANSC
    // to increase coverage and reduce redundancy when shader object extension is supported
    // use it for tests that operate on secondary command buffers
    if (m_context.getShaderObjectFeaturesEXT().shaderObject && m_groupParams.useSecondaryCmdBuffer)
    {
        m_groupParams.pipelineConstructionType = (m_groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass) ?
                                                     PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV :
                                                     PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_LINKED_BINARY;
    }
#endif // CTS_USES_VULKANSC
}

tcu::TestStatus LRZTestInstance::iterate(void)
{
    const auto &vk        = m_context.getDeviceInterface();
    auto device           = m_context.getDevice();
    auto &allocator       = m_context.getDefaultAllocator();
    auto queue            = m_context.getUniversalQueue();
    auto queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    // some test configurations require altering sequence of steps
    preprocessSteps();

    ImageWithBuffer &colorBuffer = *g_sharedData->colorBuffer;
    BufferWithMemory &outBuffer  = *g_sharedData->outBuffer;

    auto makeRenderPassFunc = &LRZTestInstance::makeRenderPass<1u>;
    if (m_groupParams.renderingType == RENDERING_TYPE_RENDERPASS2)
        makeRenderPassFunc = &LRZTestInstance::makeRenderPass<2u>;

    // create renderPasses if test doesn't use dynamic rendering
    std::vector<Move<VkRenderPass>> renderPassVect;
    const bool useDynamicRendering = m_groupParams.renderingType == RENDERING_TYPE_DYNAMIC_RENDERING;
    if (!useDynamicRendering)
    {
        renderPassVect.push_back(std::invoke(makeRenderPassFunc, this, VK_ATTACHMENT_LOAD_OP_CLEAR));
        if (m_hasRenderPassWithLoadOpLoad)
            renderPassVect.push_back(std::invoke(makeRenderPassFunc, this, VK_ATTACHMENT_LOAD_OP_LOAD));
    }

    // create descriptor pool
    const auto descriptorPool = DescriptorPoolBuilder()
                                    .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2)
                                    .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2)
                                    .build(vk, device, 1u, 3u);

    // FS::ATOMIC fragment shader needs access to buffer for atomic
    const auto fsStorageBufferSetLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(vk, device);
    const auto fsStorageBufferDescriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *fsStorageBufferSetLayout);

    // FS::IMAGE_STORE fragment shader needs access to storage image for imageStore operation
    const auto fsStorageImageSetLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(vk, device);
    const auto fsStorageImageDescriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *fsStorageImageSetLayout);

    // compute shader needs access to rendered image and buffer with expected results
    const auto computeSetLayout = DescriptorSetLayoutBuilder()
                                      .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                                      .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                      .build(vk, device);
    const auto computeDescriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *computeSetLayout);

    // create all needed graphics pipeline layouts with push constant for draw data;
    // order of layouts should be the same as order of returned values in getGraphicsPipelineLayout
    const auto nh                                  = VK_NULL_HANDLE;
    const auto pct                                 = m_groupParams.pipelineConstructionType;
    const auto vfShaderState                       = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
    const VkPushConstantRange graphicsPushConstant = {vfShaderState, 0u, sizeof(DrawData)};
    VkDescriptorSetLayout descriptorSets[]{nh, *fsStorageBufferSetLayout, *fsStorageImageSetLayout};
    for (const auto ds : descriptorSets)
        m_graphicsPipelineLayoutVect.emplace_back(pct, vk, device, ds, &graphicsPushConstant);

    // create all neeede pipelines
    preparePipelines(!useDynamicRendering ? *renderPassVect[0] : nh);

    // define image create info for blit images
    const VkImageUsageFlags blitImageUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VkImageCreateInfo imageCreateInfo      = makeImageCreateInfo(g_sharedData->depthStencilFormat, 1, blitImageUsage);

    std::vector<std::shared_ptr<ImageWithMemory>> blitImagesVect;
    for (std::size_t d = 0; d < m_blitDepthsVect.size(); ++d)
        blitImagesVect.push_back(
            std::make_shared<ImageWithMemory>(vk, device, allocator, imageCreateInfo, MemoryRequirement::Any));

    // creatr required number of framebufers
    Move<VkFramebuffer> framebuffer[DEPTH_IMAGE_COUNT][DEPTH_LAYER_COUNT];
    VkImageView iamgeViews[]{colorBuffer.getImageView(), nh};
    for (uint32_t i = 0; !useDynamicRendering && i <= m_highestUsedDepthImageIndex; ++i)
    {
        iamgeViews[1]     = *g_sharedData->depthStencilViews[i][0];
        framebuffer[i][0] = makeFramebuffer(vk, device, *renderPassVect[0], 2, iamgeViews, m_imageSize, m_imageSize);
    }
    for (uint32_t i = 0; !useDynamicRendering && i <= m_highestUsedDepthImageLayer; ++i)
    {
        iamgeViews[1]     = *g_sharedData->depthStencilViews[0][i];
        framebuffer[0][i] = makeFramebuffer(vk, device, *renderPassVect[0], 2, iamgeViews, m_imageSize, m_imageSize);
    }

    // calculate expected result on CPU before executing GPU commands
    tcu::Vec4 expectedEven = calculateExpectedColor(0);
    tcu::Vec4 expectedOdd  = calculateExpectedColor(1);

    const auto outBufferAlloc = outBuffer.getAllocation();
    float *outBufferData      = static_cast<float *>(outBufferAlloc.getHostPtr());

    // store expected results for even and odd rows in compute buffer
    const VkDeviceSize vec4Size = sizeof(tcu::Vec4);
    deMemcpy(outBufferData, expectedEven.getPtr(), vec4Size);
    deMemcpy(outBufferData + 4, expectedOdd.getPtr(), vec4Size);

    // at the end of compute buffer we store a flag that indicates if test passed;
    // if compute shader writes 1 to this flag, it means that rendered image was incorrect
    static_cast<uint32_t *>(outBufferAlloc.getHostPtr())[8] = 0u;
    flushAlloc(vk, device, outBufferAlloc);

    // update descriptor sets for some fragment shaders that need descriptors
    const auto dbDescInfo = makeDescriptorBufferInfo(*outBuffer, 0, g_sharedData->outBufferSize);
    DescriptorSetUpdateBuilder()
        .writeSingle(*fsStorageBufferDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &dbDescInfo)
        .update(vk, device);

    auto diDescInfo = makeDescriptorImageInfo(VK_NULL_HANDLE, *g_sharedData->storeView, VK_IMAGE_LAYOUT_GENERAL);
    DescriptorSetUpdateBuilder()
        .writeSingle(*fsStorageImageDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &diDescInfo)
        .update(vk, device);

    // update descriptor set for compute shader
    diDescInfo.imageView = iamgeViews[0];
    DescriptorSetUpdateBuilder()
        .writeSingle(*computeDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &diDescInfo)
        .writeSingle(*computeDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &dbDescInfo)
        .update(vk, device);

    // create compute pipeline that will be used to verify rendered image
    const auto &bc = m_context.getBinaryCollection();
    ShaderWrapper cs(vk, device, bc.get("comp"));
    auto computePipelineLayout = makePipelineLayout(vk, device, *computeSetLayout);
    const auto computePipeline = makeComputePipeline(vk, device, *computePipelineLayout, cs.getModule());

    // define required layout transition barriers
    auto initialImageBarrier = makeImageMemoryBarrier(0, VK_ACCESS_MEMORY_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                      VK_IMAGE_LAYOUT_GENERAL, colorBuffer.getImage(), m_colorIsr);
    std::vector layoutTransitionBarriers{initialImageBarrier};

    // transition store image
    initialImageBarrier.image = **g_sharedData->storeImage;
    layoutTransitionBarriers.push_back(initialImageBarrier);

    // transition depth images
    initialImageBarrier.subresourceRange            = m_dsIsr;
    initialImageBarrier.subresourceRange.layerCount = DEPTH_LAYER_COUNT;
    for (uint32_t i = 0; i < DEPTH_IMAGE_COUNT; ++i)
    {
        initialImageBarrier.image = **g_sharedData->depthStencilBuffer[i];
        layoutTransitionBarriers.push_back(initialImageBarrier);

        // only first image has multiple layers
        initialImageBarrier.subresourceRange.layerCount = 1;
    }

    // transition copy image
    initialImageBarrier.image            = **g_sharedData->copyImage;
    initialImageBarrier.subresourceRange = m_dsIsr;
    layoutTransitionBarriers.push_back(initialImageBarrier);

    // transition images used for blit steps
    for (const auto &bi : blitImagesVect)
    {
        initialImageBarrier.image = **bi;
        layoutTransitionBarriers.push_back(initialImageBarrier);
    }

    auto cmdPool       = makeCommandPool(vk, device, queueFamilyIndex);
    auto primCmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    std::vector<Move<VkCommandBuffer>> secCmdBufferVect;

    // cmdBuffer is the command buffer to which we currently record rendering commands
    auto cmdBuffer = *primCmdBuffer;

    uint32_t commonRenderingFlags                  = 0;
    VkCommandBufferBeginInfo commandBufBeginParams = initVulkanStructure();
    commandBufBeginParams.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vk.beginCommandBuffer(cmdBuffer, &commandBufBeginParams));

#ifndef CTS_USES_VULKANSC
    VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo = initVulkanStructure();
    VkCommandBufferInheritanceInfo bufferInheritanceInfo = initVulkanStructure(&inheritanceRenderingInfo);

    // if useSecondaryCmdBuffer flag is set then create secondary command buffer and setup InheritanceInfo
    if (m_groupParams.useSecondaryCmdBuffer)
    {
        inheritanceRenderingInfo.colorAttachmentCount    = 1u;
        inheritanceRenderingInfo.pColorAttachmentFormats = &g_sharedData->colorFormat;
        inheritanceRenderingInfo.depthAttachmentFormat   = g_sharedData->depthStencilFormat;
        inheritanceRenderingInfo.stencilAttachmentFormat = g_sharedData->depthStencilFormat;
        inheritanceRenderingInfo.rasterizationSamples    = VK_SAMPLE_COUNT_1_BIT;

        commandBufBeginParams.pInheritanceInfo = &bufferInheritanceInfo;
        if (m_groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass)
        {
            inheritanceRenderingInfo.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;
        }
        else
        {
            commandBufBeginParams.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
            commonRenderingFlags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;
            if (m_hasCustomSecondary)
                commonRenderingFlags |= VK_RENDERING_CONTENTS_INLINE_BIT_KHR;
        }

        for (size_t i = 0; i < m_numberOfRequiredSecondaryCommandBuffers; ++i)
            secCmdBufferVect.push_back(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY));
    }
#endif // CTS_USES_VULKANSC

    // execute all image layout transitions
    vk.cmdPipelineBarrier(cmdBuffer, 0, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0u, 0u, nullptr, 0u, nullptr,
                          static_cast<uint32_t>(layoutTransitionBarriers.size()), layoutTransitionBarriers.data());

    // if needed setup all blit images with clear values
    for (size_t i = 0; i < blitImagesVect.size(); ++i)
    {
        const auto clearDepth = makeClearDepthStencilValue(m_blitDepthsVect[i], 0);
        vk.cmdClearDepthStencilImage(cmdBuffer, **blitImagesVect[i], VK_IMAGE_LAYOUT_GENERAL, &clearDepth, 1, &m_dsIsr);
    }

    // wait for blits to be finished before starting render pass
    const auto postBlitMemoryBarrier =
        makeMemoryBarrier(VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT);
    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0u,
                          !blitImagesVect.empty(), &postBlitMemoryBarrier, 0u, nullptr, 0u, nullptr);

    uint32_t renderPassIndex = 0;
    uint32_t pipelineIndex   = 0;
    uint32_t blitImageIndex  = 0;
    uint32_t secondaryIndex  = 0;
    const auto renderArea    = makeRect2D(tcu::UVec2(m_imageSize));
    const auto isl           = makeImageSubresourceLayers(m_dsIsr.aspectMask, 0u, 0, 1);

    const BeginRenderPassData *currentRenderpassData = nullptr;
    const BindPipelineData *currentPipelineData      = nullptr;

    DE_UNREF(renderPassIndex);

    // execute all test steps in requested order
    for (const auto &testStep : m_processedStepVect)
    {
        auto currStep = testStep.type;
        if (currStep == TestStepType::BEGIN_RENDER_PASS)
        {
            uint32_t renderingFlag = 0;
#ifndef CTS_USES_VULKANSC
            if (m_suspendResumeCountPerRenderingVect[renderPassIndex] > 0)
                renderingFlag = VK_RENDERING_SUSPENDING_BIT;
#endif // CTS_USES_VULKANSC

            currentRenderpassData = std::get_if<BeginRenderPassData>(&testStep.data);
            beginRendering(vk, cmdBuffer, renderPassVect,
                           *framebuffer[currentRenderpassData->depthImageIndex][currentRenderpassData->depthImageLayer],
                           renderArea, commonRenderingFlags | renderingFlag, currentRenderpassData->depthLoadOp,
                           currentRenderpassData->depthClearValue, currentRenderpassData->stencilClearValue,
                           currentRenderpassData->depthImageIndex, currentRenderpassData->depthImageLayer);
        }
        else if (currStep == TestStepType::END_RENDER_PASS)
        {
            endRendering(vk, cmdBuffer);
            currentRenderpassData = nullptr;
            ++renderPassIndex;
        }
        else if (currStep == TestStepType::SUSPEND_RESUME_RENDERING)
        {
#ifndef CTS_USES_VULKANSC
            DE_ASSERT(currentRenderpassData);

            uint32_t renderingFlag = VK_RENDERING_RESUMING_BIT;

            // if this render pass will be suspended once again then we need to also add suspending flag
            if (m_suspendResumeCountPerRenderingVect[renderPassIndex] > 1)
                renderingFlag |= VK_RENDERING_SUSPENDING_BIT;
            --m_suspendResumeCountPerRenderingVect[renderPassIndex];

            endRendering(vk, cmdBuffer);
            beginRendering(vk, cmdBuffer, renderPassVect,
                           *framebuffer[currentRenderpassData->depthImageIndex][currentRenderpassData->depthImageLayer],
                           renderArea, renderingFlag, currentRenderpassData->depthLoadOp,
                           currentRenderpassData->depthClearValue, currentRenderpassData->stencilClearValue,
                           currentRenderpassData->depthImageIndex, currentRenderpassData->depthImageLayer);
#endif // CTS_USES_VULKANSC
        }
        else if (currStep == TestStepType::BIND_PIPELINE)
        {
            currentPipelineData = std::get_if<BindPipelineData>(&testStep.data);
            m_pipelineVect[pipelineIndex++].bind(cmdBuffer);
        }
        else if (currStep == TestStepType::DRAW)
        {
            auto dd         = std::get<DrawData>(testStep.data);
            dd.height       = static_cast<float>(m_imageSize);
            const auto &gpl = getGraphicsPipelineLayout(currentPipelineData->frag);
            vk.cmdPushConstants(cmdBuffer, *gpl, vfShaderState, 0u, sizeof(DrawData), &dd);

            auto bp                       = VK_PIPELINE_BIND_POINT_GRAPHICS;
            VkDescriptorSet descriptorSet = nh;
            if (currentPipelineData->frag == FS::ATOMIC)
                descriptorSet = *fsStorageBufferDescriptorSet;
            else if (currentPipelineData->frag == FS::IMAGE_STORE)
                descriptorSet = *fsStorageImageDescriptorSet;
            if (descriptorSet != nh)
                vk.cmdBindDescriptorSets(cmdBuffer, bp, *gpl, 0u, 1, &descriptorSet, 0u, nullptr);

            uint32_t ininstanceCount = (dd.mode == DrawMode::FULL_QUAD) ? 1u : (m_imageSize / 2u);
            vk.cmdDraw(cmdBuffer, 4u, ininstanceCount, 0u, 0u);
        }
        else if (currStep == TestStepType::BEGIN_SECONDARY)
        {
            // begin recording secondary command buffer
            cmdBuffer = *secCmdBufferVect[secondaryIndex];
            VK_CHECK(vk.beginCommandBuffer(cmdBuffer, &commandBufBeginParams));
        }
        else if (currStep == TestStepType::END_SECONDARY)
        {
            // end recording secondary command buffer
            endCommandBuffer(vk, cmdBuffer);
            cmdBuffer = *primCmdBuffer;
            vk.cmdExecuteCommands(cmdBuffer, 1u, &*secCmdBufferVect[secondaryIndex++]);
        }
        else if (currStep == TestStepType::BARRIER)
        {
            // barrier step should not be called inside render pass
            DE_ASSERT(!currentRenderpassData);

            // by default assume barrier betwean two render passes
            auto srcStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            VkAccessFlags srcAccess = VK_ACCESS_MEMORY_WRITE_BIT;
            auto dstStage           = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | srcStage;
            auto dstAccess          = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

            auto &bd = std::get<BarrierData>(testStep.data);
            if ((bd.prevStep == TestStepType::CLEAR_IMAGE) || (bd.prevStep == TestStepType::BLIT) ||
                (bd.prevStep == TestStepType::COPY_BUFFER) || (bd.prevStep == TestStepType::COPY_IMAGE))
            {
                srcStage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
                srcAccess = VK_ACCESS_MEMORY_WRITE_BIT;
            }

            if ((bd.nextStep == TestStepType::CLEAR_IMAGE) || (bd.nextStep == TestStepType::BLIT) ||
                (bd.nextStep == TestStepType::COPY_BUFFER) || (bd.nextStep == TestStepType::COPY_IMAGE))
            {
                dstStage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
                dstAccess = VK_ACCESS_MEMORY_WRITE_BIT;
            }

            const auto memoryBarrier = makeMemoryBarrier(srcAccess, dstAccess);
            vk.cmdPipelineBarrier(cmdBuffer, srcStage, dstStage, 0u, 1, &memoryBarrier, 0u, nullptr, 0u, nullptr);
        }
        else if (currStep == TestStepType::CLEAR_ATTACHMENT)
        {
            auto cd = std::get<ClearBlitCopyData>(testStep.data);
            const VkClearRect clearRect{renderArea, 0u, 1u};
            const auto clearValue = makeClearValueDepthStencil(cd.depth, 0);
            const VkClearAttachment clearAttachment{m_dsIsr.aspectMask, 0u, clearValue};
            vk.cmdClearAttachments(cmdBuffer, 1, &clearAttachment, 1, &clearRect);
        }
        else if (currStep == TestStepType::CLEAR_IMAGE)
        {
            DE_ASSERT(cmdBuffer == *primCmdBuffer);
            auto cd                  = std::get<ClearBlitCopyData>(testStep.data);
            const auto clearValue    = makeClearValueDepthStencil(cd.depth, 0);
            auto &depthStencilBuffer = *g_sharedData->depthStencilBuffer[cd.depthImageIndex];
            auto dsIsr               = m_dsIsr;
            dsIsr.baseArrayLayer     = cd.depthImageLayer;
            vk.cmdClearDepthStencilImage(cmdBuffer, *depthStencilBuffer, VK_IMAGE_LAYOUT_GENERAL,
                                         &clearValue.depthStencil, 1, &dsIsr);
        }
        else if (currStep == TestStepType::BLIT)
        {
            DE_ASSERT(cmdBuffer == *primCmdBuffer);
            auto cd                  = std::get<ClearBlitCopyData>(testStep.data);
            auto &depthStencilBuffer = *g_sharedData->depthStencilBuffer[cd.depthImageIndex];

            VkImageBlit blitRegion{isl, {{}, {1, 1, 1}}, isl, {{}, {(int32_t)m_imageSize, (int32_t)m_imageSize, 1}}};
            blitRegion.dstSubresource.baseArrayLayer = cd.depthImageLayer;

            vk.cmdBlitImage(cmdBuffer, **blitImagesVect[blitImageIndex++], VK_IMAGE_LAYOUT_GENERAL, *depthStencilBuffer,
                            VK_IMAGE_LAYOUT_GENERAL, 1, &blitRegion, VK_FILTER_NEAREST);
        }
        else if (currStep == TestStepType::COPY_BUFFER)
        {
            auto cd                 = std::get<ClearBlitCopyData>(testStep.data);
            auto depthStencilBuffer = **g_sharedData->depthStencilBuffer[cd.depthImageIndex];
            auto copyImage          = **g_sharedData->copyImage;
            auto copyBuffer         = **g_sharedData->copyBuffer;

            // wait for previous operations on copyImage to be finished
            auto imageBarrier =
                makeImageMemoryBarrier(VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                       VK_IMAGE_LAYOUT_GENERAL, copyImage, m_dsIsr);
            vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                                  nullptr, 0u, nullptr, 1u, &imageBarrier);

            // there is no easy way to clear buffer with specific depth values so we do that via image
            // and set it to specified depth value first and then copy whole image to buffer
            const auto clearValue = makeClearValueDepthStencil(cd.depth, 0);
            vk.cmdClearDepthStencilImage(cmdBuffer, copyImage, VK_IMAGE_LAYOUT_GENERAL, &clearValue.depthStencil, 1,
                                         &m_dsIsr);

            // wait for clear
            imageBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            imageBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                                  nullptr, 0u, nullptr, 1u, &imageBarrier);

            // copy image to buffer
            auto imageCopy                        = makeBufferImageCopy(m_imageExtent, isl);
            imageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            vk.cmdCopyImageToBuffer(cmdBuffer, copyImage, VK_IMAGE_LAYOUT_GENERAL, copyBuffer, 1, &imageCopy);

            // make sure that data is in copyBuffer and also that previous operations on depthStencilBuffer are finished
            auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                                                   VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
            vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 1u,
                                  &bufferBarrier, 0u, nullptr, 0u, nullptr);

            // copy buffer back to depth image
            imageCopy.imageSubresource.baseArrayLayer = cd.depthImageLayer;
            vk.cmdCopyBufferToImage(cmdBuffer, copyBuffer, depthStencilBuffer, VK_IMAGE_LAYOUT_GENERAL, 1, &imageCopy);
        }
        else if (currStep == TestStepType::COPY_IMAGE)
        {
            auto cd                 = std::get<ClearBlitCopyData>(testStep.data);
            auto depthStencilBuffer = **g_sharedData->depthStencilBuffer[cd.depthImageIndex];
            auto copyImage          = **g_sharedData->copyImage;

            VkImageCopy imageCopy{isl, {}, isl, {}, m_imageExtent};
            imageCopy.dstSubresource.baseArrayLayer = cd.depthImageLayer;

            // wait for previous operations on copyImage to be finished
            auto imageBarrier =
                makeImageMemoryBarrier(VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                       VK_IMAGE_LAYOUT_GENERAL, copyImage, m_dsIsr);
            vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0U,
                                  nullptr, 0u, nullptr, 1u, &imageBarrier);

            // set copy image to specified depth value
            const auto clearValue = makeClearValueDepthStencil(cd.depth, 0);
            vk.cmdClearDepthStencilImage(cmdBuffer, copyImage, VK_IMAGE_LAYOUT_GENERAL, &clearValue.depthStencil, 1,
                                         &m_dsIsr);

            // make sure that data is in copyImage and also that previous operations on depthStencilBuffer are finished
            auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                                                   VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
            vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 1u,
                                  &bufferBarrier, 0u, nullptr, 0u, nullptr);

            // copy image to depth image
            vk.cmdCopyImage(cmdBuffer, **g_sharedData->copyImage, VK_IMAGE_LAYOUT_GENERAL, depthStencilBuffer,
                            VK_IMAGE_LAYOUT_GENERAL, 1, &imageCopy);
        }
        else
            DE_ASSERT(false); // missing implementation for other test step types
    }

    // wait for all rendering to be finished
    const auto postRenderMemoryBarrier = makeMemoryBarrier(VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT);
    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &postRenderMemoryBarrier, 0, nullptr, 0, nullptr);

    // dispatch verification compute shader
    auto bp = VK_PIPELINE_BIND_POINT_COMPUTE;
    vk.cmdBindPipeline(cmdBuffer, bp, *computePipeline);
    vk.cmdBindDescriptorSets(cmdBuffer, bp, *computePipelineLayout, 0u, 1u, &*computeDescriptorSet, 0u, nullptr);
    vk.cmdDispatch(cmdBuffer, m_imageSize, m_imageSize, 1);

    // copy rendered image to host visible buffer so we can display it if the test fails
    tcu::IVec2 imageSize(m_imageSize, m_imageSize);
    copyImageToBuffer(vk, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), imageSize,
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

    endCommandBuffer(vk, cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *primCmdBuffer);

    // verify result
    const Allocation &bufferAllocation = outBuffer.getAllocation();
    invalidateAlloc(vk, device, bufferAllocation);

    // get ninth value from compute buffer, which indicate whether test failed (0 means pass, 1 means fail)
    uint32_t *inOutData = static_cast<uint32_t *>(bufferAllocation.getHostPtr());
    if (inOutData[8] == 0)
        return tcu::TestStatus::pass("Pass");

    // log image if test failed
    const Allocation &imageAllocation = g_sharedData->colorBuffer->getBufferAllocation();
    invalidateAlloc(vk, device, imageAllocation);
    tcu::PixelBufferAccess resultAccess(mapVkFormat(g_sharedData->colorFormat), m_imageSize, m_imageSize, 1,
                                        imageAllocation.getHostPtr());
    m_context.getTestContext().getLog() << tcu::TestLog::Image("Result", "", resultAccess);

    return tcu::TestStatus::fail("Fail");
}

void LRZTestInstance::preprocessSteps()
{
    uint32_t customSuspendResumeCount    = 0;
    bool renderingFullyInSecondaries     = false;
    bool renderingPartiallyInSecondaries = false;

    const bool useDynamicRendering = m_groupParams.renderingType == RENDERING_TYPE_DYNAMIC_RENDERING;

    if (m_groupParams.useSecondaryCmdBuffer)
    {
        renderingFullyInSecondaries     = m_groupParams.secondaryCmdBufferCompletelyContainsDynamicRenderpass;
        renderingPartiallyInSecondaries = !renderingFullyInSecondaries;
    }

    // gather required information from test steps
    for (const auto &testStep : m_testParams.stepVect)
    {
        auto currStep = testStep.type;
        if (currStep == TestStepType::BLIT)
        {
            // memorize depth values used in blit steps - we need to create additional images for them
            m_blitDepthsVect.push_back(std::get<ClearBlitCopyData>(testStep.data).depth);
            continue;
        }
        if (currStep == TestStepType::BEGIN_RENDER_PASS)
        {
            const auto &brpData = std::get<BeginRenderPassData>(testStep.data);

            // memorize highest depth image and layer used in render pass steps to know how many framebuffers we need to create
            m_highestUsedDepthImageIndex = std::max(brpData.depthImageIndex, m_highestUsedDepthImageIndex);
            m_highestUsedDepthImageLayer = std::max(brpData.depthImageLayer, m_highestUsedDepthImageLayer);

            m_hasRenderPassWithLoadOpLoad |= (brpData.depthLoadOp == VK_ATTACHMENT_LOAD_OP_LOAD);
            continue;
        }
        if (currStep == TestStepType::END_RENDER_PASS)
        {
            // memorize rendering flags for each render pass (needed for custom suspend/resume)
            m_suspendResumeCountPerRenderingVect.push_back(customSuspendResumeCount);
            customSuspendResumeCount = 0;
            continue;
        }
        if (currStep == TestStepType::SUSPEND_RESUME_RENDERING)
        {
            // memorize rendering suspending/resuming steps only when dynamic
            // rendering and it is not partially contained in secondaries
            customSuspendResumeCount += (useDynamicRendering && !renderingPartiallyInSecondaries);
            continue;
        }
        if (currStep == TestStepType::BEGIN_SECONDARY)
        {
            ++m_numberOfRequiredSecondaryCommandBuffers;
            m_hasCustomSecondary            = true;
            renderingFullyInSecondaries     = false;
            renderingPartiallyInSecondaries = false;
            continue;
        }

        // we are now only interested in bind pipeline steps
        if (currStep != TestStepType::BIND_PIPELINE)
            continue;

        m_pipelineDataVect.push_back(&std::get<BindPipelineData>(testStep.data));
    }

    // when rendering is fully or partially in secondary command buffers,
    // we need one secondary command buffer per render pass (there is as many renderpasses as
    // there are entries in m_suspendResumeCountPerRendering vector)
    if (renderingFullyInSecondaries || renderingPartiallyInSecondaries)
        m_numberOfRequiredSecondaryCommandBuffers = m_suspendResumeCountPerRenderingVect.size();

    // define variable that will store previous step
    auto prevStep = TestStepType::BARRIER;

    // insert/remove some test steps depending on test configuration
    for (const auto &testStep : m_testParams.stepVect)
    {
        auto currStep      = testStep.type;
        auto addToStepVect = true;

        if (currStep == TestStepType::BEGIN_RENDER_PASS)
        {
            // insert additional barrier step if previus step was renderpass, clear, blit or copy
            if ((prevStep == TestStepType::END_RENDER_PASS) || (prevStep == TestStepType::BLIT) ||
                (prevStep == TestStepType::CLEAR_IMAGE) || (prevStep == TestStepType::COPY_BUFFER) ||
                (prevStep == TestStepType::COPY_IMAGE))
                m_processedStepVect.emplace_back(TestStepType::BARRIER, BarrierData{prevStep, currStep});

            // before starting rendering prepend step in which we start recording to secondary
            if (renderingFullyInSecondaries)
                m_processedStepVect.emplace_back(TestStepType::BEGIN_SECONDARY);

            m_processedStepVect.push_back(testStep);

            // after rendering append step in which we start recording to secondary
            if (renderingPartiallyInSecondaries)
                m_processedStepVect.emplace_back(TestStepType::BEGIN_SECONDARY);

            addToStepVect = false;
        }
        else if (currStep == TestStepType::END_RENDER_PASS)
        {
            if (renderingPartiallyInSecondaries)
                m_processedStepVect.emplace_back(TestStepType::END_SECONDARY);
            m_processedStepVect.push_back(testStep);
            if (renderingFullyInSecondaries)
                m_processedStepVect.emplace_back(TestStepType::END_SECONDARY);

            addToStepVect = false;
        }
        else if (currStep == TestStepType::SUSPEND_RESUME_RENDERING)
        {
            // include this step only for dynamic rendering but not when rendering is partialy contained in secondaries
            addToStepVect = (useDynamicRendering && !renderingPartiallyInSecondaries);
        }
        else if ((currStep == TestStepType::BLIT) || (testStep.type == TestStepType::CLEAR_IMAGE))
        {
            // insert additional barrier step if previus step was renderpass, clear or blit
            if ((prevStep == TestStepType::END_RENDER_PASS) || (prevStep == TestStepType::BLIT) ||
                (prevStep == TestStepType::CLEAR_IMAGE))
                m_processedStepVect.emplace_back(TestStepType::BARRIER, BarrierData{prevStep, currStep});
        }
        else if (testStep.type == TestStepType::BARRIER)
            DE_ASSERT(true); // barrier steps should not be added manualy

        prevStep = testStep.type;
        if (addToStepVect)
            m_processedStepVect.push_back(testStep);
    }
}

tcu::Vec4 LRZTestInstance::calculateExpectedColor(uint32_t y) const
{
    FramebufferState fb{{0, 0, 0, 0}, {}, 0};
    bool isRowOdd                         = (y % 2);
    uint32_t depthImageIndex              = 0;
    uint32_t depthImageLayer              = 0;
    const BindPipelineData *boundPipeline = nullptr;

    // note: we don't have to use processed steps here; we can rely on the original steps
    for (const auto &testStep : m_testParams.stepVect)
    {
        const auto tType = testStep.type;
        if (tType == TestStepType::BEGIN_RENDER_PASS)
        {
            const auto &brpd = std::get<BeginRenderPassData>(testStep.data);
            depthImageIndex  = brpd.depthImageIndex;
            depthImageLayer  = brpd.depthImageLayer;

            if (brpd.depthLoadOp != VK_ATTACHMENT_LOAD_OP_CLEAR)
                continue;

            fb.color   = tcu::Vec4(0, 0, 0, 1);
            fb.stencil = brpd.stencilClearValue;

            fb.depth[depthImageIndex][depthImageLayer] = brpd.depthClearValue;
        }
        else if (tType == TestStepType::BIND_PIPELINE)
        {
            boundPipeline = std::get_if<BindPipelineData>(&testStep.data);
        }
        else if (tType == TestStepType::DRAW)
        {
            DE_ASSERT(boundPipeline);

            // check if this draw doesn't affect the currently checked pixel
            auto &dd = std::get<DrawData>(testStep.data);
            if ((dd.mode == DrawMode::EVEN_ROWS && isRowOdd) || (dd.mode == DrawMode::ODD_ROWS && !isRowOdd))
                continue;

            // FS::DISCARD discards when alpha < 0.01 -> skip all writes
            if ((boundPipeline->frag == FS::DISCARD) && (dd.color[3] < 0.01f))
                continue;

            simulateDraw(fb, depthImageIndex, depthImageLayer, *boundPipeline, dd);
        }
        else if (tType == TestStepType::CLEAR_ATTACHMENT)
        {
            // clear layer that is used as depth/stencil buffer in current renderpass
            const auto &cbd                            = std::get<ClearBlitCopyData>(testStep.data);
            fb.depth[depthImageIndex][depthImageLayer] = cbd.depth;
        }
        else if ((tType == TestStepType::CLEAR_IMAGE) || (tType == TestStepType::BLIT) ||
                 (tType == TestStepType::COPY_BUFFER) || (tType == TestStepType::COPY_IMAGE))
        {
            const auto &cbd                                    = std::get<ClearBlitCopyData>(testStep.data);
            fb.depth[cbd.depthImageIndex][cbd.depthImageLayer] = cbd.depth;
        }
        else if ((tType == TestStepType::END_RENDER_PASS) || (tType == TestStepType::SUSPEND_RESUME_RENDERING) ||
                 (tType == TestStepType::BEGIN_SECONDARY) || (tType == TestStepType::END_SECONDARY))
            continue; // noop
        else
            DE_ASSERT(false); // missing implementation for other test step types
    }

    return fb.color;
}

void LRZTestInstance::simulateDraw(FramebufferState &fb, uint32_t depthImageIndex, uint32_t depthImageLayer,
                                   const BindPipelineData &pipeline, const DrawData &dd) const
{
    if (pipeline.alphaToCoverage == AlphaToCoverage::ENABLE)
    {
        float alpha = dd.color[3];
        if (alpha < 0.001f)
            return;

        // simulation of a2c works only for alpha 0 or 1
        DE_ASSERT(alpha > 0.999f);
    }

    bool stencilPass             = true;
    const bool stencilTestEnable = (pipeline.stencilTest == StencilTest::ENABLE);
    if (stencilTestEnable)
    {
        uint32_t stencilRef = pipeline.stencilReference & pipeline.stencilCompareMask;
        uint32_t stencilVal = fb.stencil & pipeline.stencilCompareMask;
        stencilPass         = doesCompareOpPass(pipeline.stencilCompareOp, stencilRef, stencilVal);

        if (!stencilPass)
        {
            fb.stencil = applyStencilOp(pipeline.stencilFailOp, fb.stencil, pipeline.stencilReference,
                                        pipeline.stencilWriteMask);
            return;
        }
    }

    float drawDepth = dd.depth;
    float &fbDepth  = fb.depth[depthImageIndex][depthImageLayer];
    if (std::set{FS::DEPTH_GREATER_OVERRIDE, FS::DEPTH_LESS_OVERRIDE, FS::WRITE_DEPTH_OVERRIDE}.count(pipeline.frag))
        drawDepth = 1.0f - dd.depth;

    bool depthPass = true;
    if (pipeline.depthTest == DepthTest::ENABLE)
    {
        depthPass = doesCompareOpPass(pipeline.depthCompareOp, drawDepth, fbDepth);
        if (!depthPass)
        {
            if (stencilTestEnable)
                fb.stencil = applyStencilOp(pipeline.stencilDepthFailOp, fb.stencil, pipeline.stencilReference,
                                            pipeline.stencilWriteMask);
            return;
        }
    }

    if (stencilTestEnable)
        fb.stencil =
            applyStencilOp(pipeline.stencilPassOp, fb.stencil, pipeline.stencilReference, pipeline.stencilWriteMask);

    fbDepth = (pipeline.depthWrite == DepthWrite::ENABLE) ? drawDepth : fbDepth;

    VkColorComponentFlags colorWriteMask = pipeline.colorWriteMask;
    if (pipeline.blend == Blend::ENABLE)
    {
        float drawAlpha = dd.color[3];
        for (uint32_t channel = 0; channel < 4; channel++)
        {
            if (colorWriteMask & (VK_COLOR_COMPONENT_R_BIT << channel))
            {
                // alpha blending: src*srcAlpha + dst*(1-srcAlpha)
                fb.color[channel] *= (1.0f - drawAlpha);
                fb.color[channel] += (channel < 3) ? dd.color[channel] * drawAlpha : drawAlpha;
            }
        }
    }
    else
    {
        for (uint32_t channel = 0; channel < 4; channel++)
        {
            if (colorWriteMask & (VK_COLOR_COMPONENT_R_BIT << channel))
                fb.color[channel] = dd.color[channel];
        }
    }
}

uint32_t LRZTestInstance::applyStencilOp(VkStencilOp op, uint32_t val, uint32_t ref, uint32_t writeMask) const
{
    uint32_t result = val;
    switch (op)
    {
    case VK_STENCIL_OP_ZERO:
        result = 0;
        break;
    case VK_STENCIL_OP_REPLACE:
        result = ref;
        break;
    case VK_STENCIL_OP_INCREMENT_AND_CLAMP:
        result = val < 0xFF ? val + 1 : 0xFF;
        break;
    case VK_STENCIL_OP_DECREMENT_AND_CLAMP:
        result = val > 0 ? val - 1 : 0;
        break;
    case VK_STENCIL_OP_INVERT:
        result = ~val & 0xFF;
        break;
    case VK_STENCIL_OP_INCREMENT_AND_WRAP:
        result = (val + 1) & 0xFF;
        break;
    case VK_STENCIL_OP_DECREMENT_AND_WRAP:
        result = (val - 1) & 0xFF;
        break;
    default:
        result = val;
        break;
    }
    return (val & ~writeMask) | (result & writeMask);
}

template <typename T>
bool LRZTestInstance::doesCompareOpPass(VkCompareOp op, T ref, T val) const
{
    if (op == VK_COMPARE_OP_LESS)
        return ref < val;
    if (op == VK_COMPARE_OP_EQUAL)
    {
        if constexpr (std::is_floating_point_v<T>)
            return fabs(ref - val) < 1e-6f;
        return ref == val;
    }
    if (op == VK_COMPARE_OP_LESS_OR_EQUAL)
        return ref <= val;
    if (op == VK_COMPARE_OP_GREATER)
        return ref > val;
    if (op == VK_COMPARE_OP_NOT_EQUAL)
    {
        if constexpr (std::is_floating_point_v<T>)
            return fabs(ref - val) >= 1e-6f;
        return ref != val;
    }
    if (op == VK_COMPARE_OP_GREATER_OR_EQUAL)
        return ref >= val;
    if (op == VK_COMPARE_OP_ALWAYS)
        return true;

    return false;
}

const PipelineLayoutWrapper &LRZTestInstance::getGraphicsPipelineLayout(FS frag)
{
    // pipelines that are used for atomic and image store operations have different pipeline layout;
    // note order of layouts coresponds to order in m_graphicsPipelineLayoutVect
    uint32_t layoutIndex = (frag == FS::ATOMIC) + 2 * (frag == FS::IMAGE_STORE);
    return m_graphicsPipelineLayoutVect[layoutIndex];
}

void LRZTestInstance::preparePipelines(VkRenderPass renderPass)
{
    const auto &vki     = m_context.getInstanceInterface();
    const auto &vk      = m_context.getDeviceInterface();
    auto device         = m_context.getDevice();
    auto physicalDevice = m_context.getPhysicalDevice();

    const std::vector scissors{makeRect2D(m_imageSize, m_imageSize)};
    const std::vector viewports{makeViewport(m_imageSize, m_imageSize)};

    // configure dynamic rendering info (used only when test does dynamic rendering)
    PipelineRenderingCreateInfoWrapper renderingCreateInfoWrapper;
#ifndef CTS_USES_VULKANSC
    VkPipelineRenderingCreateInfo renderingCreateInfo = initVulkanStructure();
    renderingCreateInfo.colorAttachmentCount          = 1;
    renderingCreateInfo.pColorAttachmentFormats       = &g_sharedData->colorFormat;
    renderingCreateInfo.depthAttachmentFormat         = g_sharedData->depthStencilFormat;
    renderingCreateInfo.stencilAttachmentFormat       = g_sharedData->depthStencilFormat;

    if (renderPass == VK_NULL_HANDLE)
        renderingCreateInfoWrapper.ptr = &renderingCreateInfo;
#endif // CTS_USES_VULKANSC

    const VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();

    // configure depth/stencil state
    VkPipelineDepthStencilStateCreateInfo depthStencilState = initVulkanStructure();
    depthStencilState.maxDepthBounds                        = 1.0f;

    // configure color blend state
    VkPipelineColorBlendAttachmentState colorBlendAttachmentState{};
    colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

    VkPipelineColorBlendStateCreateInfo colorBlendState = initVulkanStructure();
    colorBlendState.attachmentCount                     = 1;
    colorBlendState.pAttachments                        = &colorBlendAttachmentState;

    // configure multisample state
    VkPipelineMultisampleStateCreateInfo multisampleState = initVulkanStructure();
    multisampleState.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;
    multisampleState.minSampleShading                     = 1.0f;

    const std::nullptr_t np = nullptr;
    const auto &bc          = m_context.getBinaryCollection();
    ShaderWrapper vs(vk, device, bc.get("vert"));

    for (const auto pd : m_pipelineDataVect)
    {
        const auto &dpd = *pd;

        // construct all required pipelines
        ShaderWrapper fs(vk, device, bc.get(fragShaderToName.at(dpd.frag)));

        // set depth/stencil state
        depthStencilState.depthTestEnable  = static_cast<VkBool32>(dpd.depthTest);
        depthStencilState.depthWriteEnable = static_cast<VkBool32>(dpd.depthWrite);
        depthStencilState.depthCompareOp   = dpd.depthCompareOp;

        depthStencilState.stencilTestEnable = static_cast<VkBool32>(dpd.stencilTest);
        depthStencilState.front.failOp      = dpd.stencilFailOp;
        depthStencilState.front.passOp      = dpd.stencilPassOp;
        depthStencilState.front.depthFailOp = dpd.stencilDepthFailOp;
        depthStencilState.front.compareOp   = dpd.stencilCompareOp;
        depthStencilState.front.compareMask = dpd.stencilCompareMask;
        depthStencilState.front.writeMask   = dpd.stencilWriteMask;
        depthStencilState.front.reference   = dpd.stencilReference;
        depthStencilState.back              = depthStencilState.front;

        // set color blend state
        colorBlendAttachmentState.blendEnable    = static_cast<VkBool32>(dpd.blend);
        colorBlendAttachmentState.colorWriteMask = dpd.colorWriteMask;

        // set multisample state
        multisampleState.alphaToCoverageEnable = static_cast<VkBool32>(dpd.alphaToCoverage);

        m_pipelineVect.emplace_back(vki, vk, physicalDevice, device, m_context.getDeviceExtensions(),
                                    m_groupParams.pipelineConstructionType);

        const PipelineLayoutWrapper &graphicsPipelineLayout = getGraphicsPipelineLayout(dpd.frag);
        (m_pipelineVect.back())
            .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            .setDefaultRasterizationState()
            .setupVertexInputState(&vertexInputState)
            .setupPreRasterizationShaderState2(viewports, scissors, graphicsPipelineLayout, renderPass, 0u, vs, np, {},
                                               {}, {}, np, np, np, np, np, renderingCreateInfoWrapper)
            .setupFragmentShaderState(graphicsPipelineLayout, renderPass, 0u, fs, &depthStencilState, &multisampleState)
            .setupFragmentOutputState(renderPass, 0u, &colorBlendState, &multisampleState)
            .setMonolithicPipelineLayout(graphicsPipelineLayout)
            .buildPipeline();
    }
}

template <uint32_t Version>
Move<VkRenderPass> LRZTestInstance::makeRenderPass(const VkAttachmentLoadOp loadOperation) const
{
    using AttachmentDescription = std::conditional_t<Version == 2, AttachmentDescription2, AttachmentDescription1>;
    using AttachmentReference   = std::conditional_t<Version == 2, AttachmentReference2, AttachmentReference1>;
    using SubpassDescription    = std::conditional_t<Version == 2, SubpassDescription2, SubpassDescription1>;
    using RenderPassCreateInfo  = std::conditional_t<Version == 2, RenderPassCreateInfo2, RenderPassCreateInfo1>;

    const std::nullptr_t np = nullptr;
    const auto lg           = VK_IMAGE_LAYOUT_GENERAL;

    const AttachmentDescription attachments[]{
        {np, 0, g_sharedData->colorFormat, VK_SAMPLE_COUNT_1_BIT, loadOperation, VK_ATTACHMENT_STORE_OP_STORE,
         VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, lg, lg},
        {np, 0, g_sharedData->depthStencilFormat, VK_SAMPLE_COUNT_1_BIT, loadOperation, VK_ATTACHMENT_STORE_OP_STORE,
         loadOperation, VK_ATTACHMENT_STORE_OP_STORE, lg, lg},
    };

    AttachmentReference colorRef(np, 0u, lg, VK_IMAGE_ASPECT_COLOR_BIT);
    AttachmentReference dsRef(np, 1, lg, m_dsIsr.aspectMask);
    SubpassDescription subpass(np, 0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0u, 0u, np, 1, &colorRef, np, &dsRef, 0u, np);
    RenderPassCreateInfo rpci(np, 0, (uint32_t)std::size(attachments), attachments, 1u, &subpass, 0u, np, 0u, np);

    return rpci.createRenderPass(m_context.getDeviceInterface(), m_context.getDevice());
}

void LRZTestInstance::beginRendering(const DeviceInterface &vk, const VkCommandBuffer cmdBuffer,
                                     const std::vector<Move<VkRenderPass>> &renderPassVect,
                                     const VkFramebuffer framebuffer, const VkRect2D &renderArea, uint32_t flags,
                                     VkAttachmentLoadOp depthLoadOp, float depthClear, uint32_t stencilClear,
                                     uint32_t depthImage, uint32_t depthLayer) const
{
    DE_UNREF(flags);
    DE_UNREF(depthImage);
    DE_UNREF(depthLayer);
    DE_UNREF(depthLoadOp);
    DE_ASSERT(g_sharedData);

    const VkClearValue clearValues[]{
        makeClearValueColor({0.0f, 0.0f, 0.0f, 1.0f}),        // color attachment 0
        makeClearValueDepthStencil(depthClear, stencilClear), // D/S attachment 1
    };

    if (m_groupParams.renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
    {
#ifndef CTS_USES_VULKANSC
        // note: using same load op for color and D/S
        VkRenderingAttachmentInfoKHR colorAttachment = initVulkanStructure();
        colorAttachment.imageView                    = g_sharedData->colorBuffer->getImageView();
        colorAttachment.imageLayout                  = VK_IMAGE_LAYOUT_GENERAL;
        colorAttachment.loadOp                       = depthLoadOp;
        colorAttachment.storeOp                      = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue                   = clearValues[0];

        VkRenderingAttachmentInfoKHR dsAttachment = colorAttachment;
        dsAttachment.imageView                    = *g_sharedData->depthStencilViews[depthImage][depthLayer];
        dsAttachment.loadOp                       = depthLoadOp;
        dsAttachment.clearValue                   = clearValues[1];

        VkRenderingInfoKHR renderingInfo   = initVulkanStructure();
        renderingInfo.flags                = flags;
        renderingInfo.renderArea           = renderArea;
        renderingInfo.layerCount           = 1u;
        renderingInfo.colorAttachmentCount = 1u;
        renderingInfo.pColorAttachments    = &colorAttachment;
        renderingInfo.pDepthAttachment     = &dsAttachment;
        renderingInfo.pStencilAttachment   = &dsAttachment;

        vk.cmdBeginRendering(cmdBuffer, &renderingInfo);
#endif // CTS_USES_VULKANSC
    }
    else
    {
        VkRenderPassBeginInfo rpbi = initVulkanStructure();
        rpbi.renderPass            = *renderPassVect[0];
        rpbi.framebuffer           = framebuffer;
        rpbi.renderArea            = renderArea;
        rpbi.clearValueCount       = 2u;
        rpbi.pClearValues          = clearValues;

        if (depthLoadOp == VK_ATTACHMENT_LOAD_OP_LOAD)
            rpbi.renderPass = *renderPassVect[1];

        VkSubpassBeginInfo sbi = initVulkanStructure();

        if (m_groupParams.renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
            vk.cmdBeginRenderPass(cmdBuffer, &rpbi, sbi.contents);
        else
            vk.cmdBeginRenderPass2(cmdBuffer, &rpbi, &sbi);
    }
}

void LRZTestInstance::endRendering(const DeviceInterface &vk, const VkCommandBuffer cmdBuffer) const
{
    if (m_groupParams.renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
    {
#ifndef CTS_USES_VULKANSC
        vk.cmdEndRendering(cmdBuffer);
#endif // CTS_USES_VULKANSC
    }
    else if (m_groupParams.renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
        vk.cmdEndRenderPass(cmdBuffer);
    else if (m_groupParams.renderingType == RENDERING_TYPE_RENDERPASS2)
    {
        VkSubpassEndInfo subpassEndInfo = initVulkanStructure();
        vk.cmdEndRenderPass2(cmdBuffer, &subpassEndInfo);
    }
}

class BaseTestCase : public vkt::TestCase
{
public:
    BaseTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &testParams,
                 const SharedGroupParams groupParams)
        : vkt::TestCase(testCtx, name)
        , m_testParams(testParams)
        , m_groupParams(groupParams)
    {
    }
    virtual ~BaseTestCase(void) = default;

    virtual void checkSupport(Context &context) const override;
    virtual void initPrograms(SourceCollections &programCollection) const override;
    virtual TestInstance *createInstance(Context &context) const override;

private:
    TestParams m_testParams;
    const SharedGroupParams m_groupParams;
};

void BaseTestCase::checkSupport(Context &context) const
{
    const auto &vki     = context.getInstanceInterface();
    auto physicalDevice = context.getPhysicalDevice();

    checkPipelineConstructionRequirements(vki, physicalDevice, m_groupParams->pipelineConstructionType);

    bool usesBlit      = false;
    bool usesClear     = false;
    bool usesSecondary = false;

    for (const auto &testStep : m_testParams.stepVect)
    {
        usesBlit |= (testStep.type == TestStepType::BLIT);
        usesClear |= (testStep.type == TestStepType::CLEAR_IMAGE);
        usesSecondary |= (testStep.type == TestStepType::BEGIN_SECONDARY);
    }

    if (usesSecondary)
        context.requireDeviceFunctionality("VK_KHR_maintenance7");

    if (usesBlit || usesClear)
    {
        VkFormatFeatureFlags formatFeatures = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        formatFeatures |= (usesClear) ? VK_FORMAT_FEATURE_TRANSFER_DST_BIT : 0;
        formatFeatures |= (usesBlit) ? VK_FORMAT_FEATURE_BLIT_DST_BIT : 0;

        VkFormatProperties formatProperties;
        vki.getPhysicalDeviceFormatProperties(physicalDevice, VK_FORMAT_D24_UNORM_S8_UINT, &formatProperties);
        if ((formatProperties.optimalTilingFeatures & formatFeatures) != formatFeatures)
        {
            vki.getPhysicalDeviceFormatProperties(physicalDevice, VK_FORMAT_D32_SFLOAT_S8_UINT, &formatProperties);
            if ((formatProperties.optimalTilingFeatures & formatFeatures) != formatFeatures)
                TCU_THROW(NotSupportedError, "Required format features not supported");
        }
    }
}

void BaseTestCase::initPrograms(SourceCollections &programCollection) const
{
    auto &pc = programCollection.glslSources;

    std::string commonShaderPreamble = R"(#version 450
        layout(push_constant) uniform PushConstant {
            vec4 color;
            float depth;
            float height;
            int mode;
        } pc;)";

    pc.add("vert") << glu::VertexSource(commonShaderPreamble + R"(
        void main(void)
        {
            gl_Position.x = (gl_VertexIndex & 1) * 2.0 - 1.0;
            gl_Position.z = pc.depth;
            gl_Position.w = 1.0;

            if (pc.mode == 0) // full-screen quad
                gl_Position.y = (gl_VertexIndex & 2) - 1.0;
            else
            {
                // draw one 1-pixel-tall strip per instance;
                // even rows when mode is 1, odd rows when mode is 2

                int row       = gl_InstanceIndex * 2 + (pc.mode - 1);
                float pixelH  = 2.0 / pc.height;
                float top     = -1.0 + float(row) * pixelH;
                float bottom  = top + pixelH;
                gl_Position.y = ((gl_VertexIndex & 2) == 0) ? top : bottom;
            }
        })");

    // iterate over all needed fragment shaders and generate them
    std::set<FS> requiredFragShaders;
    for (const auto &testStep : m_testParams.stepVect)
    {
        if (testStep.type != TestStepType::BIND_PIPELINE)
            continue;

        const auto &pipeline = std::get<BindPipelineData>(testStep.data);
        requiredFragShaders.insert(pipeline.frag);
    }

    auto rfsEnd             = requiredFragShaders.end();
    auto commonFragPreamble = commonShaderPreamble + "\n\t\tlayout(location = 0) out vec4 fragColor;\n";
    auto commonFragMain     = R"(
        void main(void)
        {
            gl_FragDepth = pc.depth;
            fragColor = pc.color;
        })";

    if (requiredFragShaders.find(FS::BASIC) != rfsEnd)
    {
        pc.add("basic-frag") << glu::FragmentSource(commonFragPreamble + R"(
            void main(void)
            {
                fragColor = pc.color;
            })");
    }
    if (requiredFragShaders.find(FS::DEPTH_GREATER) != rfsEnd)
    {
        pc.add("depth-greater-frag") << glu::FragmentSource(
            commonFragPreamble + "layout(depth_greater) out float gl_FragDepth;\n" + commonFragMain);
    }
    if (requiredFragShaders.find(FS::DEPTH_LESS) != rfsEnd)
    {
        pc.add("depth-less-frag") << glu::FragmentSource(
            commonFragPreamble + "layout(depth_less) out float gl_FragDepth;\n" + commonFragMain);
    }
    if (requiredFragShaders.find(FS::DEPTH_UNCHANGED) != rfsEnd)
    {
        pc.add("depth-unchanged-frag") << glu::FragmentSource(
            commonFragPreamble + "layout(depth_unchanged) out float gl_FragDepth;\n" + commonFragMain);
    }
    if (requiredFragShaders.find(FS::DEPTH_GREATER_OVERRIDE) != rfsEnd)
    {
        pc.add("depth-greater-override-frag") << glu::FragmentSource(commonFragPreamble + R"(
            layout(depth_greater) out float gl_FragDepth;
            void main(void)
            {
                gl_FragDepth = 1.0 - pc.depth;
                fragColor = pc.color;
            })");
    }
    if (requiredFragShaders.find(FS::DEPTH_LESS_OVERRIDE) != rfsEnd)
    {
        pc.add("depth-less-override-frag") << glu::FragmentSource(commonFragPreamble + R"(
            layout(depth_less) out float gl_FragDepth;
            void main(void)
            {
                gl_FragDepth = 1.0 - pc.depth;
                fragColor = pc.color;
            })");
    }
    if (requiredFragShaders.find(FS::WRITE_DEPTH_OVERRIDE) != rfsEnd)
    {
        pc.add("write-depth-override-frag") << glu::FragmentSource(commonFragPreamble + R"(
            void main(void)
            {
                gl_FragDepth = 1.0 - pc.depth;
                fragColor = pc.color;
            })");
    }
    if (requiredFragShaders.find(FS::EARLY_FRAG_TESTS) != rfsEnd)
    {
        pc.add("early-frag-tests-frag") << glu::FragmentSource(commonFragPreamble + R"(
            layout(early_fragment_tests) in;
            void main(void)
            {
                fragColor = pc.color;
            })");
    }
    if (requiredFragShaders.find(FS::DISCARD) != rfsEnd)
    {
        pc.add("discard-frag") << glu::FragmentSource(commonFragPreamble + R"(
            void main(void)
            {
                if (pc.color.a < 0.01)
                    discard;
                fragColor = pc.color;
            })");
    }
    if (requiredFragShaders.find(FS::ATOMIC) != rfsEnd)
    {
        pc.add("atomic-frag") << glu::FragmentSource(commonFragPreamble + R"(
            layout(binding = 0) buffer Buf
            {
                vec4 expectedEven;  // in, used for verification
                vec4 expectedOdd;   // in, used for verification
                uint isDifferent;   // out, verification expects this to be at index 8
                uint atomicValue;   // out, used only by atomic-frag
            };
            void main(void)
            {
                atomicAdd(atomicValue, 1u);
                fragColor = pc.color;
            })");
    }
    if (requiredFragShaders.find(FS::IMAGE_STORE) != rfsEnd)
    {
        pc.add("image-store-frag") << glu::FragmentSource(commonFragPreamble + R"(
            layout (binding = 0, rgba8) uniform writeonly image2D outImage;
            void main(void)
            {
                imageStore(outImage, ivec2(mod(gl_FragCoord.xy, 4.0)), vec4(2.0));
                fragColor = pc.color;
            })");
    }

    pc.add("comp") << glu::ComputeSource(R"(#version 450
        layout(binding = 0, rgba8) readonly uniform image2D frame;
        layout(binding = 1) buffer Buf
        {
            vec4 expectedEven;  // in
            vec4 expectedOdd;   // in
            uint isDifferent;   // out, verification expects this to be at index 8
            uint atomicValue;   // out, used only by atomic-frag
        };
        void main()
        {
            vec4 expectedColor = mix(expectedEven, expectedOdd, float(gl_GlobalInvocationID.y % 2));
            if (length(imageLoad(frame, ivec2(gl_GlobalInvocationID.xy)) - expectedColor) > 0.01)
                atomicExchange(isDifferent, 1);
        })");
}

TestInstance *BaseTestCase::createInstance(Context &context) const
{
    return new LRZTestInstance(context, m_testParams, m_groupParams);
}

} // namespace

class FuzzTestBuilder
{
public:
    FuzzTestBuilder(uint32_t seed);
    void build(std::vector<TestStep> &stepVect);

protected:
    uint32_t randPercentage();
    std::pair<uint32_t, uint32_t> randDepthImageAndLayer(bool useSeparateDSImages);
    float randDepth(bool biasTowardsBounds = false);

    BeginRenderPassData fuzzRenderPassData(uint32_t depthImageIndex, uint32_t depthImageLayer, bool wasCleared);
    BindPipelineData fuzzPipelineData();
    DrawData fuzzDraw(const BindPipelineData &pipelineData);

private:
    de::Random m_rng;

    // spec doesn't guarantee that 0.3f written via depth clear is exactly equal to 0.3f
    // written via draw call - to prevent errors like that we try to use different depth value
    // in each operation
    const uint32_t m_uniqueDepthValuesCount = 21;
    std::vector<float> m_depthValuesVect;
};

FuzzTestBuilder::FuzzTestBuilder(uint32_t seed) : m_rng(seed)
{
    m_depthValuesVect.reserve(m_uniqueDepthValuesCount);
}

void FuzzTestBuilder::build(std::vector<TestStep> &stepVect)
{
    // determine number of renderpasses; 45% of cases will have 2rp; 20% of cases will have 3 RP
    uint32_t r              = randPercentage();
    uint32_t renderPassCont = 1 + (r > 55) + (r > 80) + (r > 92) + (r > 95);

    // determine if test will operate on layers of same depth image or on separate ds images
    bool useSeparateDSImages = (randPercentage() < 50);

    // we need to make sure each depth image view is cleared befor beeing used for the first time
    bool wasDepthViewCleared[DEPTH_IMAGE_COUNT][DEPTH_LAYER_COUNT]{};

    // force regeneration of depth values to reduce the chance that it will happen mid-test
    m_depthValuesVect.clear();

    for (uint32_t rpIndex = 0; rpIndex < renderPassCont; ++rpIndex)
    {
        // get random depth view and check if it was cleared;
        auto [depthImageIndex, depthImageLayer] = randDepthImageAndLayer(useSeparateDSImages);
        bool &wasCleared                        = wasDepthViewCleared[depthImageIndex][depthImageLayer];

        stepVect.emplace_back(TestStepType::BEGIN_RENDER_PASS,
                              fuzzRenderPassData(depthImageIndex, depthImageLayer, wasCleared));

        // mark used depth image layer as cleared
        wasCleared = true;

        BindPipelineData pipelineData;
        bool requirePipelineBind = true;

        // determine number of draw steps, 1-6 but biased toward 2-3
        r                  = randPercentage();
        uint32_t drawCount = 1 + (r > 20) + (r > 55) + (r > 80) + (r > 95) + (r > 97);

        for (uint32_t drawIndex = 0; drawIndex < drawCount; ++drawIndex)
        {
            // bind pipeline when necesery or 30% for new pipeline bind
            if (requirePipelineBind || (randPercentage() < 30))
            {
                pipelineData        = fuzzPipelineData();
                requirePipelineBind = false;
                stepVect.emplace_back(TestStepType::BIND_PIPELINE, pipelineData);
            }

            // 10% for suspend/resume
            if (randPercentage() < 10)
            {
                requirePipelineBind = true;
                stepVect.emplace_back(TestStepType::SUSPEND_RESUME_RENDERING);
            }

            // 10% for depth attachment clear
            if (randPercentage() < 10)
                stepVect.emplace_back(TestStepType::CLEAR_ATTACHMENT, ClearBlitCopyData{randDepth(), 0, 0});

            stepVect.emplace_back(TestStepType::DRAW, fuzzDraw(pipelineData));
        }

        stepVect.emplace_back(TestStepType::END_RENDER_PASS);

        // 40% for depth image clear / blit / copy
        r = randPercentage();
        if (r < 40)
        {
            auto [depthImage, depthLayer]               = randDepthImageAndLayer(useSeparateDSImages);
            wasDepthViewCleared[depthImage][depthLayer] = true;

            TestStepType stepType = TestStepType::BLIT;
            if (r < 10)
                stepType = TestStepType::CLEAR_IMAGE;
            else if (r < 20)
                stepType = TestStepType::COPY_BUFFER;
            else if (r < 30)
                stepType = TestStepType::COPY_IMAGE;

            stepVect.emplace_back(stepType, ClearBlitCopyData{randDepth(), depthImage, depthLayer});
        }
    }
}

uint32_t FuzzTestBuilder::randPercentage()
{
    return m_rng.getUint32() % 100;
}

std::pair<uint32_t, uint32_t> FuzzTestBuilder::randDepthImageAndLayer(bool useSeparateDSImages)
{
    uint32_t r = randPercentage();
    uint32_t i = (r < 75) ? 0 : (r < 90) ? 1 : 2;

    return useSeparateDSImages ? std::pair{i, 0u} : std::pair{0u, i};
}

float FuzzTestBuilder::randDepth(bool biasTowardsBounds)
{
    if (biasTowardsBounds)
    {
        uint32_t r = randPercentage();
        if (r < 45)
            return 1.0f;
        if (r < 80)
            return 0.0f;
    }

    if (m_depthValuesVect.empty())
    {
        // generate unique depth values from <0.0, 1.0> range
        float step = 1.0f / float(m_uniqueDepthValuesCount - 1);
        for (uint32_t i = 0; i < m_uniqueDepthValuesCount; ++i)
            m_depthValuesVect.push_back(std::clamp(step * float(i), 0.0f, 1.0f));

        // shuffle values
        m_rng.shuffle(m_depthValuesVect.begin(), m_depthValuesVect.end());
    }

    // get value from the back
    float depthValue = m_depthValuesVect.back();
    m_depthValuesVect.pop_back();
    return depthValue;
}

BeginRenderPassData FuzzTestBuilder::fuzzRenderPassData(uint32_t depthImageIndex, uint32_t depthImageLayer,
                                                        bool wasCleared)
{
    VkAttachmentLoadOp depthLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    if (wasCleared)
        depthLoadOp = (randPercentage() < 70) ? VK_ATTACHMENT_LOAD_OP_LOAD : depthLoadOp;

    uint32_t stencilClearValue = (randPercentage() < 80) ? 0 : m_rng.getInt(0, 0xFF);

    return {depthLoadOp,    VK_ATTACHMENT_STORE_OP_STORE, randDepth(true), stencilClearValue, depthImageIndex,
            depthImageLayer};
}

BindPipelineData FuzzTestBuilder::fuzzPipelineData()
{
    BindPipelineData bpd{FS::BASIC};

    uint32_t r = randPercentage();
    if (r < 35)
    {
        bpd.frag = (FS)m_rng.getInt(0, (int)FS::LAST - 1);

        // dont use override shaders in fuzzing tests, if override FS are selected use basic shader instead
        if (std::set{FS::DEPTH_GREATER_OVERRIDE, FS::DEPTH_LESS_OVERRIDE, FS::WRITE_DEPTH_OVERRIDE}.count(bpd.frag))
            bpd.frag = FS::BASIC;

        // decrese chance for DISCARD and EARLY_FRAG_TESTS
        if ((bpd.frag == FS::DISCARD) || (bpd.frag == FS::EARLY_FRAG_TESTS))
            bpd.frag = (r < 5) ? FS::BASIC : bpd.frag;
    }

    // depth test 85%
    bpd.depthTest  = DepthTest(randPercentage() < 85);
    bpd.depthWrite = DepthWrite((bpd.depthTest == DepthTest::ENABLE) && (randPercentage() < 75));

    // pick compare operation, bias toward LESS / GREATER since those are the interesting LRZ ops
    r                  = randPercentage();
    bpd.depthCompareOp = VK_COMPARE_OP_LESS;
    if (r > 93)
        bpd.depthCompareOp = VK_COMPARE_OP_NEVER;
    else if (r > 87)
        bpd.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    else if (r > 81)
        bpd.depthCompareOp = VK_COMPARE_OP_NOT_EQUAL;
    else if (r > 74)
        bpd.depthCompareOp = VK_COMPARE_OP_EQUAL;
    else if (r > 64)
        bpd.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
    else if (r > 54)
        bpd.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    else if (r > 27)
        bpd.depthCompareOp = VK_COMPARE_OP_GREATER;

    // stencil 5%
    bpd.stencilTest = StencilTest(randPercentage() < 5);
    if (bpd.stencilTest == StencilTest::ENABLE)
    {
        bpd.stencilCompareOp   = (VkCompareOp)m_rng.getInt(0, VK_COMPARE_OP_LAST - 1);
        bpd.stencilPassOp      = (VkStencilOp)m_rng.getInt(0, VK_STENCIL_OP_LAST - 1);
        bpd.stencilFailOp      = (VkStencilOp)m_rng.getInt(0, VK_STENCIL_OP_LAST - 1);
        bpd.stencilDepthFailOp = (VkStencilOp)m_rng.getInt(0, VK_STENCIL_OP_LAST - 1);
        bpd.stencilCompareMask = (randPercentage() > 70) ? 0xFF : m_rng.getInt(0, 0xFF);
        bpd.stencilWriteMask   = (randPercentage() > 70) ? 0xFF : m_rng.getInt(0, 0xFF);
        bpd.stencilReference   = m_rng.getInt(0, 0xFF);
    }

    // blend 15%
    bpd.blend = Blend(randPercentage() < 15);

    // partial color mask 3%
    bpd.colorWriteMask = 0xF;
    if (randPercentage() < 3)
    {
        auto &mask = bpd.colorWriteMask;
        mask       = 0;

        for (uint32_t component = 0; component < 4; component++)
            mask |= (randPercentage() < 50) * (VK_COLOR_COMPONENT_R_BIT << component);
        mask = (mask) ? mask : 0xF;
    }

    // alpha to coverage 5%
    bpd.alphaToCoverage = AlphaToCoverage(randPercentage() < 5);

    return bpd;
}

DrawData FuzzTestBuilder::fuzzDraw(const BindPipelineData &pipelineData)
{
    const float colorValue[]{0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    tcu::Vec4 color{1};

    for (uint32_t i = 0; i < 3; ++i)
        color[i] = colorValue[m_rng.getInt(0, std::size(colorValue) - 1)];

    if (pipelineData.alphaToCoverage == AlphaToCoverage::ENABLE)
        color[3] = 1.0f * (randPercentage() < 50);
    else if (pipelineData.frag != FS::DISCARD)
        color[3] = (randPercentage() < 20) ? 1.0f : colorValue[m_rng.getInt(0, 3)];

    return {color, randDepth(), 1.0f, DrawMode::FULL_QUAD};
}

tcu::TestCaseGroup *createFuzzGroup(uint32_t count, uint32_t seed, tcu::TestContext &testCtx,
                                    const SharedGroupParams groupParams)
{
    FuzzTestBuilder testBuilder(seed + groupParams->renderingType + groupParams->useSecondaryCmdBuffer +
                                groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass);

    de::MovePtr<tcu::TestCaseGroup> fuzzGroup(new tcu::TestCaseGroup(testCtx, "fuzz"));
    for (uint32_t caseIndex = 0; caseIndex < count; ++caseIndex)
    {
        TestParams testParams{std::to_string(caseIndex)};
        testBuilder.build(testParams.stepVect);

        fuzzGroup->addChild(new BaseTestCase(testCtx, testParams.name, testParams, groupParams));
    }

    return fuzzGroup.release();
}

static void createChildren(tcu::TestCaseGroup *lrzTests, const SharedGroupParams groupParams)
{
    using namespace StepHelpers;

    // Each test is constructed from a series of steps that are used to build test command buffer.
    // To simplify test configuration, some steps are removed/inserted in the preprocessSteps method.

    FS fsb = FS::BASIC;
    const std::vector<TestParams> directionChangeVect{
        // Direction change with depth write -> invalidates LRZ.
        // Uses checkerboard rows so both GREATER and LESS draws write
        // fragments into the same LRZ block. Draw 2 (full screen,
        // GREATER 0.3) gets rejected by corrupted LRZ values.
        {"greater_to_less_write",
         {beginRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, 0.5f),
          bindPipeline(VK_COMPARE_OP_GREATER),           //
          draw({1, 0, 0, 1}, 0.8f, DrawMode::ODD_ROWS),  //
          bindPipeline(VK_COMPARE_OP_LESS),              //
          draw({0, 1, 0, 1}, 0.2f, DrawMode::EVEN_ROWS), //
          bindPipeline(VK_COMPARE_OP_GREATER),           //
          draw({0, 1, 1, 1}, 0.3f),                      //
          endRenderPass()}},

        // LESS -> GREATER direction change on checkerboard.
        // Clear=1.0. Draw 0 (even, LESS 0.2) lowers LRZ to 0.2.
        // Draw 1 (odd, GREATER 0.8) raises shared-block LRZ to 0.8.
        // Draw 2 (full, GREATER 0.7): on even rows real depth=0.2,
        // 0.7>0.2=true -> should draw, but LRZ=0.8 rejects (0.7 <= 0.8).
        {"less_to_greater_write",
         {beginRenderPass(),
          bindPipeline(VK_COMPARE_OP_LESS),              //
          draw({1, 0, 0, 1}, 0.2f, DrawMode::EVEN_ROWS), //
          bindPipeline(VK_COMPARE_OP_GREATER),           //
          draw({0, 1, 0, 1}, 0.8f, DrawMode::ODD_ROWS),  //
          bindPipeline(VK_COMPARE_OP_GREATER),           //
          draw({0, 1, 1, 1}, 0.7f),                      //
          endRenderPass()}},

        // Same as greater_to_less_write but with LESS_OR_EQUAL / GREATER_OR_EQUAL
        {"ge_to_le_write",
         {beginRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, 0.5f),
          bindPipeline(VK_COMPARE_OP_GREATER_OR_EQUAL),  //
          draw({1, 0, 0, 1}, 0.8f, DrawMode::ODD_ROWS),  //
          bindPipeline(VK_COMPARE_OP_LESS_OR_EQUAL),     //
          draw({0, 1, 0, 1}, 0.2f, DrawMode::EVEN_ROWS), //
          bindPipeline(VK_COMPARE_OP_GREATER_OR_EQUAL),  //
          draw({0, 1, 1, 1}, 0.3f),                      //
          endRenderPass()}},

        // Double direction change -> GREATER -> LESS -> GREATER.
        // 4 draws with checkerboard. After two direction flips the LRZ
        // buffer is thoroughly corrupted.
        {"double_flip",
         {beginRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, 0.5f),
          bindPipeline(VK_COMPARE_OP_GREATER),           //
          draw({1, 0, 0, 1}, 0.8f, DrawMode::ODD_ROWS),  //
          bindPipeline(VK_COMPARE_OP_LESS),              //
          draw({0, 1, 0, 1}, 0.2f, DrawMode::EVEN_ROWS), //
          bindPipeline(VK_COMPARE_OP_GREATER),           //
          draw({0, 0, 1, 1}, 0.6f, DrawMode::ODD_ROWS),  //
          bindPipeline(VK_COMPARE_OP_GREATER),           //
          draw({0, 1, 1, 1}, 0.3f),                      //
          endRenderPass()}},

        // Full-screen draws only.
        // LESS with blend, then LESS with write, then GREATER overwrites
        {"fuzz_less_blend_to_greater",
         {beginRenderPass(),
          bindBlendPipeline(VK_COMPARE_OP_LESS, DepthWrite::DISABLE, Blend::ENABLE,
                            VK_COLOR_COMPONENT_B_BIT), //
          draw({0, 0.5f, 0, 1}, 0.5f),                 //
          bindBlendPipeline(VK_COMPARE_OP_LESS, DepthWrite::ENABLE, Blend::ENABLE, 0),
          draw({0.25f, 0.5f, 1.0f, 0.75f}, 0.2f),  //
          bindPipeline(VK_COMPARE_OP_GREATER),     //
          draw({0.25f, 1.0f, 1.0f, 0.25f}, 0.75f), //
          endRenderPass()}},

        // LESS -> GREATER without depth write -> no invalidation
        {"less_to_greater_no_write",
         {beginRenderPass(),
          bindPipeline(VK_COMPARE_OP_LESS),                                                 //
          draw({1, 0, 0, 1}, 0.5f),                                                         //
          bindPipeline(VK_COMPARE_OP_GREATER, fsb, DepthTest::ENABLE, DepthWrite::DISABLE), //
          draw({0, 1, 0, 1}, 0.8f),
          suspendResumeRendering(),         //
          bindPipeline(VK_COMPARE_OP_LESS), //
          draw({0, 0, 1, 1}, 0.4f),         //
          endRenderPass()}},
    };

    const std::vector<TestParams> directionPreserveVect{

        // GREATER -> EQUAL -> GREATER preserves direction
        {"greater_equal_greater",
         {beginRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, 0.0f),
          bindPipeline(VK_COMPARE_OP_GREATER), //
          draw({1, 0, 0, 1}, 0.5f),            //
          bindPipeline(VK_COMPARE_OP_EQUAL),   //
          draw({0, 1, 0, 1}, 0.5f),            //
          bindPipeline(VK_COMPARE_OP_GREATER), //
          draw({0, 0, 1, 1}, 0.8f),            //
          endRenderPass()}},

        // GREATER -> EQUAL -> LESS invalidates
        {"greater_equal_less",
         {beginRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, 0.0f),
          bindPipeline(VK_COMPARE_OP_GREATER), //
          draw({1, 0, 0, 1}, 0.5f),            //
          bindPipeline(VK_COMPARE_OP_EQUAL),   //
          draw({0, 1, 0, 1}, 0.5f),            //
          bindPipeline(VK_COMPARE_OP_LESS),    //
          draw({0, 0, 1, 1}, 0.3f),            //
          endRenderPass()}},

        // LESS -> ALWAYS with depth write -> invalidates
        {"less_to_always_write",
         {beginRenderPass(),
          bindPipeline(VK_COMPARE_OP_LESS),   //
          draw({1, 0, 0, 1}, 0.5f),           //
          bindPipeline(VK_COMPARE_OP_ALWAYS), //
          draw({0, 1, 0, 1}, 0.8f),           //
          bindPipeline(VK_COMPARE_OP_LESS),   //
          draw({0, 0, 1, 1}, 0.7f),           //
          endRenderPass()}},

        // LESS -> ALWAYS without depth write -> no invalidation
        {"less_to_always_no_write",
         {beginRenderPass(),
          bindPipeline(VK_COMPARE_OP_LESS),                                                //
          draw({1, 0, 0, 1}, 0.5f),                                                        //
          bindPipeline(VK_COMPARE_OP_ALWAYS, fsb, DepthTest::ENABLE, DepthWrite::DISABLE), //
          draw({0, 1, 0, 1}, 0.8f),
          bindPipeline(VK_COMPARE_OP_LESS), //
          draw({0, 0, 1, 1}, 0.4f),         //
          endRenderPass()}},

        // LESS -> NOT_EQUAL with depth write -> invalidates
        {"less_to_not_equal_write",
         {beginRenderPass(),
          bindPipeline(VK_COMPARE_OP_LESS),      //
          draw({1, 0, 0, 1}, 0.5f),              //
          bindPipeline(VK_COMPARE_OP_NOT_EQUAL), //
          draw({0, 1, 0, 1}, 0.8f),              //
          bindPipeline(VK_COMPARE_OP_LESS),      //
          draw({0, 0, 1, 1}, 0.7f),              //
          endRenderPass()}},

        // LESS -> NEVER -> LESS re-enables LRZ
        {"less_never_less",
         {beginRenderPass(),
          bindPipeline(VK_COMPARE_OP_LESS),  //
          draw({1, 0, 0, 1}, 0.5f),          //
          bindPipeline(VK_COMPARE_OP_NEVER), //
          draw({0, 1, 0, 1}, 0.3f),          //
          bindPipeline(VK_COMPARE_OP_LESS),  //
          draw({0, 0, 1, 1}, 0.4f),          //
          endRenderPass()}},

        // No depth test enabled at all
        {"no_depth_test",
         {beginRenderPass(),                                                               //
          bindPipeline(VK_COMPARE_OP_NEVER, fsb, DepthTest::DISABLE, DepthWrite::DISABLE), //
          draw({1, 0, 0, 1}, 0.5f),                                                        //
          draw({0, 1, 0, 1}, 0.3f),                                                        //
          endRenderPass()}},
    };

    const std::vector<TestParams> blendVect{
        // The blending + depth write corruption scenario
        {"depth_corruption_scenario",
         {beginRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, 0.0f),
          bindPipeline(VK_COMPARE_OP_GREATER),                                         //
          draw({1, 0, 0, 1}, 0.1f),                                                    //
          bindBlendPipeline(VK_COMPARE_OP_GREATER, DepthWrite::ENABLE, Blend::ENABLE), //
          draw({0, 1, 0, 0.5f}, 0.4f),                                                 //
          bindPipeline(VK_COMPARE_OP_GREATER),                                         //
          draw({0, 0, 1, 1}, 0.2f),                                                    //
          endRenderPass()}},

        // Partial color mask + depth write -> disables LRZ write
        {"partial_color_mask_depth_write",
         {beginRenderPass(),
          bindPipeline(VK_COMPARE_OP_LESS), //
          draw({1, 0, 0, 1}, 0.5f),         //
          bindBlendPipeline(VK_COMPARE_OP_LESS, DepthWrite::ENABLE, Blend::DISABLE,
                            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT),
          draw({0, 1, 0, 1}, 0.3f), //
          endRenderPass()}},

        // Draw 1 writes color+depth, draw 2 writes depth only (all color writes disabled).
        // LRZ write must be disabled for the RP because draw 1's depth-only write could
        // corrupt the LRZ buffer. If LRZ write is not disabled, draw 2 writes LRZ=0.4 and
        // draw 1 at depth 0.1 is rejected by LRZ in the rendering pass.
        {"color_masked_after_color_depth",
         {beginRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, 0.0f),
          bindPipeline(VK_COMPARE_OP_GREATER),                                             //
          draw({1, 0, 0, 1}, 0.1f),                                                        //
          bindBlendPipeline(VK_COMPARE_OP_GREATER, DepthWrite::ENABLE, Blend::DISABLE, 0), //
          draw({0, 1, 0, 1}, 0.4f),                                                        //
          bindPipeline(VK_COMPARE_OP_GREATER),                                             //
          draw({0, 0, 1, 1}, 0.2f),                                                        //
          endRenderPass()}},
    };

    const std::vector<TestParams> edgeVect{
        // vkCmdClearAttachments on depth mid-RP -> invalidates LRZ
        {"mid_rp_depth_clear",
         {beginRenderPass(),
          bindPipeline(VK_COMPARE_OP_LESS), //
          draw({1, 0, 0, 1}, 0.5f),         //
          clearDepthAttachment(0.8f),       //
          draw({0, 0, 1, 1}, 0.7f),         //
          endRenderPass()}},

        // Depth bounds test coexists with LRZ
        {"depth_bounds_test",
         {beginRenderPass(),
          bindPipeline(VK_COMPARE_OP_LESS), //
          draw({1, 0, 0, 1}, 0.5f),         //
          draw({0, 1, 0, 1}, 0.3f),         //
          endRenderPass()}},

        // No depth attachment - LRZ should be fully disabled
        {"no_depth_attachment",
         {beginRenderPass(),                                               //
          bindPipeline(VK_COMPARE_OP_LESS, FS::BASIC, DepthTest::DISABLE), //
          draw({1, 0, 0, 1}, 0.5f),                                        //
          draw({0, 1, 0, 1}, 0.3f),                                        //
          endRenderPass()}},

        // Alpha to coverage + depth write -> LRZ write disabled.
        {"alpha_to_coverage",
         {beginRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, 0.0f),
          bindPipeline(VK_COMPARE_OP_GREATER), //
          draw({1, 0, 0, 1}, 0.1f),
          bindBlendPipeline(VK_COMPARE_OP_GREATER, DepthWrite::ENABLE, Blend::DISABLE, 15U, AlphaToCoverage::ENABLE),
          draw({0, 1, 0, 0}, 0.4f),
          bindPipeline(VK_COMPARE_OP_GREATER), //
          draw({0, 0, 1, 1}, 0.2f),            //
          endRenderPass()}},
    };

    const std::vector<TestParams> stencilVect{
        // Stencil compare != ALWAYS disables LRZ write
        // Draw 1's stencil EQUAL ref = 1 fails(stencil buffer is 0), so the fragment is killed and
        // depth stays at 0.5. If LRZ write were not *disabled, LRZ would record 0.3,
        // and draw 2 at depth 0.4(which should pass against real depth 0.5) would be incorrectly rejected.
        {"compare_neq_always",
         {beginRenderPass(), //
          bindPipeline(VK_COMPARE_OP_LESS),
          // Draw 1: establish depth = 0.5, stencil stays 0 (clear value)
          draw({1, 0, 0, 1}, 0.5f),
          bindPipeline(VK_COMPARE_OP_LESS, fsb, DepthTest::ENABLE, DepthWrite::ENABLE, StencilTest::ENABLE,
                       VK_COMPARE_OP_EQUAL, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, 0xFF, 0xFF,
                       0x1),
          // Draw 2: stencil EQUAL ref = 1 fails(buf = 0), fragment killed, depth stays 0.5. LRZ write must be disabled.
          draw({0, 1, 0, 1}, 0.3f), //
          bindPipeline(VK_COMPARE_OP_LESS),
          // Draw 3: depth 0.4 < 0.5 -> should pass. If LRZ was wrongly written to 0.3 above, LRZ rejects(0.4 >= 0.3).
          draw({0, 0, 1, 1}, 0.4f), //
          endRenderPass()}},

        // Stencil depthFailOp writes stencil on depth fail
        // Draw 2 fails depth(0.8 not< 0.5) but depthFailOp = REPLACE must still write stencil = 0x42.
        // If LRZ incorrectly rejects draw 2, the stencil write is skipped and draw 3(which requires
        // stencil == 0x42) will fail, producing wrong color.
        {"stencil_depth_fail_op_writes",
         {beginRenderPass(),
          bindPipeline(VK_COMPARE_OP_LESS), //
          draw({1, 0, 0, 1}, 0.5f),
          // depth fails(0.8 not< 0.5), depthFailOp writes stencil = 0x42. LRZ must be disabled so this draw executes.
          bindPipeline(VK_COMPARE_OP_LESS, fsb, DepthTest::ENABLE, DepthWrite::ENABLE, StencilTest::ENABLE,
                       VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, 0xFF, 0xFF,
                       0x42),
          draw({0, 1, 0, 1}, 0.8f),
          // Draw 3: stencil EQUAL ref = 0x42 gates a depth - passing draw.
          // If draw 1's stencil write was skipped, stencil is 0 -> fails.
          bindPipeline(VK_COMPARE_OP_LESS, fsb, DepthTest::ENABLE, DepthWrite::ENABLE, StencilTest::ENABLE,
                       VK_COMPARE_OP_EQUAL, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, 0xFF, 0x0,
                       0x42),
          draw({0, 0, 1, 1}, 0.3f), //
          endRenderPass()}},

        // The blending + depth write corruption scenario
        {"always_both_faces",
         {beginRenderPass(),
          // Draw 1: establish depth=0.1
          bindPipeline(VK_COMPARE_OP_GREATER), //
          draw({1, 0, 0, 1}, 0.1f),
          // Draw 2: stencil ALWAYS passOp=REPLACE triggers stencil_written_based_on_depth_test.
          // LRZ write must be disabled.
          bindPipeline(VK_COMPARE_OP_GREATER, fsb, DepthTest::ENABLE, DepthWrite::ENABLE, StencilTest::ENABLE,
                       VK_COMPARE_OP_GREATER, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, 0xFF, 0xFF,
                       0x1),
          draw({0, 1, 0, 0.5f}, 0.4f),
          // Draw 3: fails depth (0.2 < 0.4)
          bindPipeline(VK_COMPARE_OP_GREATER), //
          draw({0, 0, 1, 1}, 0.2f),            //
          endRenderPass()}},
    };

    const std::vector<TestParams> fragmentShaderVect{

        // layout(depth_greater) + LESS compare
        {"depth_greater_with_less_compare",
         {beginRenderPass(),                                   //
          bindPipeline(VK_COMPARE_OP_LESS, FS::DEPTH_GREATER), //
          draw({1, 0, 0, 1}, 0.5f),                            //
          draw({0, 1, 0, 1}, 0.3f),                            //
          endRenderPass()}},

        // layout(depth_less) + LESS compare -> LRZ disabled for draw
        {"depth_less_with_less_compare",
         {beginRenderPass(),
          bindPipeline(VK_COMPARE_OP_LESS),                 //
          draw({1, 0, 0, 1}, 0.5f),                         //
          bindPipeline(VK_COMPARE_OP_LESS, FS::DEPTH_LESS), //
          draw({0, 1, 0, 1}, 0.3f),                         //
          endRenderPass()}},

        // layout(depth_unchanged) + any compare
        {"depth_unchanged",
         {beginRenderPass(),                                     //
          bindPipeline(VK_COMPARE_OP_LESS, FS::DEPTH_UNCHANGED), //
          draw({1, 0, 0, 1}, 0.5f),                              //
          draw({0, 1, 0, 1}, 0.3f),                              //
          endRenderPass()}},

        // layout(depth_greater) + GREATER compare -> incompatible.
        // FS with depth_greater hint writes 1.0 - pc.depth(increases depth).
        // Draw 2: vertex depth = 0.3, FS depth = 0.7. LRZ sees 0.3 > 0.5 = false -> rejects.
        // Real test: 0.7 > 0.5 = true -> should draw.
        {"depth_greater_with_greater_compare",
         {beginRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, 0.0f),
          bindPipeline(VK_COMPARE_OP_GREATER),                             //
          draw({1, 0, 0, 1}, 0.5f),                                        //
          bindPipeline(VK_COMPARE_OP_GREATER, FS::DEPTH_GREATER_OVERRIDE), //
          draw({0, 1, 0, 1}, 0.3f),                                        //
          endRenderPass()}},

        // layout(depth_less) + LESS compare -> incompatible.
        // FS with depth_less hint writes 1.0 - pc.depth(decreases depth).
        // Draw 2: vertex depth = 0.7, FS depth = 0.3. LRZ sees 0.7 < 0.5 = false -> rejects.
        // Real test: 0.3 < 0.5 = true -> should draw.
        {"depth_less_override_with_less_compare",
         {beginRenderPass(),
          bindPipeline(VK_COMPARE_OP_LESS),                          //
          draw({1, 0, 0, 1}, 0.5f),                                  //
          bindPipeline(VK_COMPARE_OP_LESS, FS::DEPTH_LESS_OVERRIDE), //
          draw({0, 1, 0, 1}, 0.7f),                                  //
          endRenderPass()}},

        // FS writes gl_FragDepth with no layout hint -> LRZ must be disabled.
        // Draw 2 uses WRITE_DEPTH_OVERRIDE which writes gl_FragDepth = 1.0 - 0.8 = 0.2.
        // Vertex depth 0.8 would fail * LRZ(0.8 > 0.5), but post - FS depth 0.2 passes LESS(0.2 < 0.5).
        // If LRZ isn't disabled, the fragment is incorrectly rejected.
        {"frag_depth_no_hint",
         {beginRenderPass(),
          bindPipeline(VK_COMPARE_OP_LESS),                           //
          draw({1, 0, 0, 1}, 0.5f),                                   //
          bindPipeline(VK_COMPARE_OP_LESS, FS::WRITE_DEPTH_OVERRIDE), //
          draw({0, 1, 0, 1}, 0.8f),                                   //
          endRenderPass()}},

        // layout(early_fragment_tests) -> normal LRZ.
        {"early_frag_tests",
         {beginRenderPass(),                                      //
          bindPipeline(VK_COMPARE_OP_LESS, FS::EARLY_FRAG_TESTS), //
          draw({1, 0, 0, 1}, 0.5f),                               //
          draw({0, 1, 0, 1}, 0.3f),                               //
          endRenderPass()}},

        // FS with discard + depth write -> LRZ write disabled.
        // Draw 2 discards(alpha = 0) so depth stays at 0.1. If LRZ write were not disabled,
        // LRZ would record 0.4(written before late - Z evaluates discard).
        // Draw 3 at 0.2 GREATER would then be incorrectly rejected by LRZ(0.2 not> 0.4).
        {"discard_with_depth_write",
         {beginRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, 0.0f),
          bindPipeline(VK_COMPARE_OP_GREATER),              //
          draw({1, 0, 0, 1}, 0.1f),                         //
          bindPipeline(VK_COMPARE_OP_GREATER, FS::DISCARD), //
          draw({0, 1, 0, 0}, 0.4f),                         //
          bindPipeline(VK_COMPARE_OP_GREATER),              //
          draw({0, 0, 1, 1}, 0.2f),                         //
          endRenderPass()}},

        {"atomics",
         {beginRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, 0.0f),
          bindPipeline(VK_COMPARE_OP_GREATER),             //
          draw({1, 0, 0, 1}, 0.1f),                        //
          bindPipeline(VK_COMPARE_OP_GREATER, FS::ATOMIC), //
          draw({0, 1, 0, 0}, 0.4f),                        //
          bindPipeline(VK_COMPARE_OP_GREATER),             //
          draw({0, 0, 1, 1}, 0.2f),                        //
          endRenderPass()}},

        {"image_store",
         {beginRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, 0.0f),
          bindPipeline(VK_COMPARE_OP_GREATER),                  //
          draw({1, 0, 0, 1}, 0.1f),                             //
          bindPipeline(VK_COMPARE_OP_GREATER, FS::IMAGE_STORE), //
          draw({0, 1, 0, 0}, 0.4f),                             //
          bindPipeline(VK_COMPARE_OP_GREATER),                  //
          draw({0, 0, 1, 1}, 0.2f),                             //
          endRenderPass()}},
    };

    const std::vector<TestParams> suspendResumeVect{

        // S/R with LESS draws across boundary
        {"less_across_boundary",
         {beginRenderPass(),
          bindPipeline(VK_COMPARE_OP_LESS), //
          draw({1, 0, 0, 1}, 0.5f),         //
          suspendResumeRendering(),         //
          bindPipeline(VK_COMPARE_OP_LESS), //
          draw({0, 1, 0, 1}, 0.3f),         //
          bindPipeline(VK_COMPARE_OP_LESS), //
          draw({0, 0, 1, 1}, 0.2f),         //
          bindPipeline(VK_COMPARE_OP_LESS), //
          draw({1, 1, 0, 1}, 0.1f),         //
          endRenderPass()}},

        // Direction change invalidates -> propagates across S/R
        {"invalidation_propagates",
         {beginRenderPass(),
          bindPipeline(VK_COMPARE_OP_LESS), //
          draw({1, 0, 0, 1}, 0.5f),         //
          suspendResumeRendering(),
          // Direction change invalidates LRZ
          bindPipeline(VK_COMPARE_OP_GREATER), //
          draw({0, 1, 0, 1}, 0.8f),
          // After resume: LRZ should still be invalidated
          bindPipeline(VK_COMPARE_OP_LESS), //
          draw({0, 0, 1, 1}, 0.4f),         //
          bindPipeline(VK_COMPARE_OP_LESS), //
          draw({1, 1, 0, 1}, 0.3f),         //
          endRenderPass()}},

        // Stencil disables LRZ write -> propagates across S/R
        {"write_disable_propagates",
         {beginRenderPass(),
          suspendResumeRendering(), //
          bindPipeline(VK_COMPARE_OP_LESS, FS::BASIC, DepthTest::ENABLE, DepthWrite::ENABLE, StencilTest::ENABLE,
                       VK_COMPARE_OP_EQUAL, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP),
          draw({1, 0, 0, 1}, 0.5f),
          // after resume: LRZ write should still be disabled
          bindPipeline(VK_COMPARE_OP_LESS), //
          draw({0, 1, 0, 1}, 0.3f),         //
          bindPipeline(VK_COMPARE_OP_LESS), //
          draw({0, 0, 1, 1}, 0.2f),         //
          endRenderPass()}},
    };

    const std::vector<TestParams> secondaryVect{

        // Primary sets LESS direction -> secondary draws LESS
        {"inherits_direction",
         {beginRenderPass(),
          bindPipeline(VK_COMPARE_OP_LESS), //
          draw({1, 0, 0, 1}, 0.5f),         //
          beginSecondary(),                 //
          bindPipeline(VK_COMPARE_OP_LESS), //
          draw({0, 1, 0, 1}, 0.3f),         //
          draw({0, 0, 1, 1}, 0.2f),         //
          endSecondary(),                   //
          endRenderPass()}},

        // Primary invalidates LRZ -> secondary should see it disabled
        {"inherits_invalidation",
         {beginRenderPass(),
          bindPipeline(VK_COMPARE_OP_LESS),    //
          draw({1, 0, 0, 1}, 0.5f),            //
          bindPipeline(VK_COMPARE_OP_GREATER), //
          draw({0, 1, 0, 1}, 0.3f),            //
          beginSecondary(),                    //
          bindPipeline(VK_COMPARE_OP_LESS),    //
          draw({0, 0, 1, 1}, 0.4f),            //
          endSecondary(),                      //
          endRenderPass()}},

        {"secondary_less_greater_then_load",
         {beginRenderPass(),
          bindPipeline(VK_COMPARE_OP_LESS),    //
          draw({1, 0, 0, 1}, 0.5f),            //
          beginSecondary(),                    //
          bindPipeline(VK_COMPARE_OP_GREATER), //
          draw({0, 1, 0, 1}, 0.8f),            //
          endSecondary(),                      //
          endRenderPass(),
          beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD), //
          bindPipeline(VK_COMPARE_OP_LESS),            //
          draw({0, 0, 1, 1}, 0.7f),                    //
          endRenderPass()}},

        // Primary disables LRZ write -> secondary inherits
        {"inherits_write_disable",
         {beginRenderPass(),
          // Stencil compare != ALWAYS -> disables LRZ write
          bindPipeline(VK_COMPARE_OP_LESS, FS::BASIC, DepthTest::ENABLE, DepthWrite::ENABLE, StencilTest::ENABLE,
                       VK_COMPARE_OP_EQUAL, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP),
          draw({1, 0, 0, 1}, 0.5f),         //
          bindPipeline(VK_COMPARE_OP_LESS), //
          draw({0, 1, 0, 1}, 0.3f),         //
          beginSecondary(),                 //
          bindPipeline(VK_COMPARE_OP_LESS), //
          draw({0, 0, 1, 1}, 0.2f),         //
          endSecondary(),                   //
          endRenderPass()}},

        // Multiple sequential draws in secondary
        {"multiple_sequential_draws",
         {beginRenderPass(),
          bindPipeline(VK_COMPARE_OP_LESS), //
          beginSecondary(),                 //
          bindPipeline(VK_COMPARE_OP_LESS), //
          draw({1, 0, 0, 1}, 0.9f),         //
          draw({0, 1, 0, 1}, 0.7f),         //
          draw({0, 0, 1, 1}, 0.5f),         //
          draw({1, 1, 0, 1}, 0.3f),         //
          draw({0, 1, 1, 1}, 0.1f),         //
          endSecondary(),                   //
          endRenderPass()}},
    };

    const std::vector<TestParams> crossRenderpassVect{

        {"reuse_same_direction",
         {
             beginRenderPass(),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 0, 0, 1}, 0.5f),         //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({0, 0, 1, 1}, 0.3f),         //
             endRenderPass(),
         }},

        {"invalidated_persists_on_load",
         {
             beginRenderPass(),
             bindPipeline(VK_COMPARE_OP_LESS),    //
             draw({1, 0, 0, 1}, 0.5f),            //
             bindPipeline(VK_COMPARE_OP_GREATER), //
             draw({0, 1, 0, 1}, 0.8f),            //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({0, 0, 1, 1}, 0.4f),         //
             endRenderPass(),
         }},

        {"mid_rp_clear_then_load",
         {
             beginRenderPass(),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 0, 0, 1}, 0.5f),         //
             clearDepthAttachment(0.8f),       //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({0, 0, 1, 1}, 0.7f),         //
             endRenderPass(),
         }},

        {"direction_change_then_load",
         {
             beginRenderPass(),
             bindPipeline(VK_COMPARE_OP_LESS),    //
             draw({1, 0, 0, 1}, 0.5f),            //
             bindPipeline(VK_COMPARE_OP_GREATER), //
             draw({0, 1, 0, 1}, 0.8f),            //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({0, 0, 1, 1}, 0.7f),         //
             endRenderPass(),
         }},

        {"always_write_then_load",
         {
             beginRenderPass(),
             bindPipeline(VK_COMPARE_OP_LESS),   //
             draw({1, 0, 0, 1}, 0.5f),           //
             bindPipeline(VK_COMPARE_OP_ALWAYS), //
             draw({0, 1, 0, 1}, 0.8f),           //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({0, 0, 1, 1}, 0.7f),         //
             endRenderPass(),
         }},

        {"clear_recovers_after_invalidation",
         {
             beginRenderPass(),
             bindPipeline(VK_COMPARE_OP_LESS),    //
             draw({1, 0, 0, 1}, 0.5f),            //
             bindPipeline(VK_COMPARE_OP_GREATER), //
             draw({0, 1, 0, 1}, 0.8f),            //
             endRenderPass(),
             beginRenderPass(),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({0, 0, 1, 1}, 0.3f),         //
             endRenderPass(),
         }},

        // Blit overwrites depth to 0.8 (was 0.3 from first draw), making LRZ stale.
        {"blit_invalidates",
         {
             beginRenderPass(),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 0, 0, 1}, 0.3f),         //
             endRenderPass(),
             blitDepth(0.8f),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({0, 0, 1, 1}, 0.5f),         //
             endRenderPass(),
         }},

        {"clear_invalidates",
         {
             beginRenderPass(),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 0, 0, 1}, 0.5f),         //
             endRenderPass(),
             clearDepthImage(0.2f),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({0, 0, 1, 1}, 0.3f),         //
             endRenderPass(),
         }},

        {"copy_buffer_invalidates",
         {
             beginRenderPass(),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 0, 0, 1}, 0.3f),         //
             endRenderPass(),
             copyBuffer(0.8f),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({0, 0, 1, 1}, 0.5f),         //
             endRenderPass(),
         }},

        {"copy_image_invalidates",
         {
             beginRenderPass(),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 0, 0, 1}, 0.3f),         //
             endRenderPass(),
             copyImage(0.8f),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({0, 0, 1, 1}, 0.5f),         //
             endRenderPass(),
         }},

        {"different_images_ab_a",
         {
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, 1.0f, VK_ATTACHMENT_STORE_OP_STORE, 0, 0),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 0, 0, 1}, 0.5f),         //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, 0.0f, VK_ATTACHMENT_STORE_OP_STORE, 0, 1),
             bindPipeline(VK_COMPARE_OP_GREATER), //
             draw({0, 1, 0, 1}, 0.5f),            //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD, 0.0f, VK_ATTACHMENT_STORE_OP_STORE, 0, 0),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({0, 0, 1, 1}, 0.3f),         //
             endRenderPass(),
         }},

        {"direction_change",
         {
             beginRenderPass(),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 0, 0, 1}, 0.5f),         //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD, 0.0f),
             bindPipeline(VK_COMPARE_OP_GREATER), //
             draw({0, 0, 1, 1}, 0.8f),            //
             endRenderPass(),
         }},

        {"write_disable_resets_across_rp",
         {
             beginRenderPass(),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 0, 0, 1}, 0.5f),
             bindBlendPipeline(VK_COMPARE_OP_LESS, DepthWrite::ENABLE, Blend::ENABLE), //
             draw({0, 1, 0, 0.5f}, 0.3f),                                              //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD, 0.0f),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({0, 0, 1, 1}, 0.2f),         //
             endRenderPass(),
         }},

        {"concurrent_binning_chain",
         {
             beginRenderPass(),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 0, 0, 1}, 0.5f),         //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD, 0.0f),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({0, 1, 0, 1}, 0.3f),         //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD, 0.0f),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({0, 0, 1, 1}, 0.2f),         //
             endRenderPass(),
         }},

        {"store_dont_care_then_load",
         {
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, 1.0f, VK_ATTACHMENT_STORE_OP_DONT_CARE),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 0, 0, 1}, 0.5f),         //
             endRenderPass(),
             // depth loaded is undefined (storeOp was DONT_CARE), use ALWAYS
             // so draw passes regardless. Tests driver handles edge case.
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD),
             bindPipeline(VK_COMPARE_OP_ALWAYS), //
             draw({0, 0, 1, 1}, 0.3f),           //
             endRenderPass(),
         }},

        {"three_images_rotation",
         {
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, 1.0f, VK_ATTACHMENT_STORE_OP_STORE, 0, 0, 0),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 0, 0, 1}, 0.5f),         //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, 0.0f, VK_ATTACHMENT_STORE_OP_STORE, 0, 1, 0),
             bindPipeline(VK_COMPARE_OP_GREATER), //
             draw({0, 1, 0, 1}, 0.5f),            //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, 1.0f, VK_ATTACHMENT_STORE_OP_STORE, 0, 2, 0),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 1, 0, 1}, 0.7f),         //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD, 0.0f, VK_ATTACHMENT_STORE_OP_STORE, 0, 0, 0),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({0, 0, 1, 1}, 0.3f),         //
             endRenderPass(),
         }},

        {"three_layers_rotation",
         {
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, 1.0f, VK_ATTACHMENT_STORE_OP_STORE, 0, 0, 0),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 0, 0, 1}, 0.5f),         //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, 0.0f, VK_ATTACHMENT_STORE_OP_STORE, 0, 0, 1),
             bindPipeline(VK_COMPARE_OP_GREATER), //
             draw({0, 1, 0, 1}, 0.5f),            //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, 1.0f, VK_ATTACHMENT_STORE_OP_STORE, 0, 0, 2),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 1, 0, 1}, 0.7f),         //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD, 0.0f, VK_ATTACHMENT_STORE_OP_STORE, 0, 0, 0),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({0, 0, 1, 1}, 0.3f),         //
             endRenderPass(),
         }},
    };

    const std::vector<TestParams> crossRenderpassSuspendResumeVect{

        {"sr_storeop_from_last_segment",
         {
             beginRenderPass(),
             suspendResumeRendering(),         //
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 0, 0, 1}, 0.5f),         //
             draw({0, 1, 0, 1}, 0.3f),         //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD, 0.0f),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({0, 0, 1, 1}, 0.2f),         //
             endRenderPass(),
         }},

        {"dont_care_load_no_reuse",
         {
             beginRenderPass(),
             suspendResumeRendering(),         //
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 0, 0, 1}, 0.5f),         //
             endRenderPass(),
             // depth is undefined (loadOp=DONT_CARE), use ALWAYS so draw passes regardless
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_DONT_CARE, 0.0f),
             bindPipeline(VK_COMPARE_OP_ALWAYS), //
             draw({0, 0, 1, 1}, 0.3f),           //
             endRenderPass(),
         }},

        {"rp_before_sr_chain_then_load",
         {
             beginRenderPass(),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 0, 0, 1}, 0.5f),         //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD),
             bindPipeline(VK_COMPARE_OP_LESS),    //
             draw({1, 0, 1, 1}, 0.4f),            //
             bindPipeline(VK_COMPARE_OP_GREATER), //
             draw({0, 1, 0, 1}, 0.8f),            //
             suspendResumeRendering(),            //
             bindPipeline(VK_COMPARE_OP_LESS),    //
             draw({1, 1, 0, 1}, 0.79f),           //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({0, 0, 1, 1}, 0.74f),        //
             endRenderPass(),
         }},

        {"sr_chain_blend_then_load",
         {
             beginRenderPass(),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 0, 0, 1}, 0.5f),         //
             bindBlendPipeline(VK_COMPARE_OP_GREATER, DepthWrite::ENABLE, Blend::ENABLE),
             draw({0, 1, 0, 0.5f}, 0.8f),      //
             suspendResumeRendering(),         //
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 1, 0, 1}, 0.75f),        //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD), //
             bindPipeline(VK_COMPARE_OP_LESS),            //
             draw({0, 0, 1, 1}, 0.7f),                    //
             endRenderPass(),
         }},

        // S/R chain (Phase 1 suspend -> Phase 2 resume) then Phase 3 loads.
        // Tests that storeOp is taken from the resuming phase (Phase 2), not the
        // suspending phase (Phase 1). This is the concurrent binning storeOp bug
        // scenario: the merged S/R chain must correctly store depth for Phase 3.
        {"sr_chain_then_load",
         {
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_CLEAR, 1.0f, VK_ATTACHMENT_STORE_OP_STORE),
             suspendResumeRendering(),         //
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 0, 0, 1}, 0.5f),         //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD, 0.0f, VK_ATTACHMENT_STORE_OP_STORE),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({0, 1, 0, 1}, 0.3f),         //
             endRenderPass(),
             // Phase 3: separate RP that loads depth stored by the S/R chain
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD, 0.0f, VK_ATTACHMENT_STORE_OP_STORE),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({0, 0, 1, 1}, 0.2f),         //
             endRenderPass(),
         }},

        // 3-way S/R chain (Phase 1 -> Phase 2 -> Phase 3) then Phase 4 loads.
        // Exercises longer S/R chains where only the final phase's storeOp matters.
        {"sr_chain_3way_then_load",
         {
             beginRenderPass(),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 0, 0, 1}, 0.5f),         //
             suspendResumeRendering(),         //
             draw({0, 1, 0, 1}, 0.4f),         //
             suspendResumeRendering(),         //
             draw({1, 1, 0, 1}, 0.3f),         //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD), //
             bindPipeline(VK_COMPARE_OP_LESS),            //
             draw({0, 0, 1, 1}, 0.2f),                    //
             endRenderPass(),
         }},

        // Two separate S/R chains back-to-back, both loading / storing depth.
        // Chain 1: Phase 1(suspend) -> Phase 2(resume, stores).
        // Chain 2: Phase 3(suspend, loads) -> Phase 4(resume, stores).
        // Tests concurrent binning WAR tracking between two merged chains.
        {"two_sr_chains",
         {
             beginRenderPass(),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 0, 0, 1}, 0.5f),         //
             suspendResumeRendering(),         //
             draw({0, 1, 0, 1}, 0.4f),         //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 1, 0, 1}, 0.3f),         //
             suspendResumeRendering(),         //
             draw({0, 0, 1, 1}, 0.2f),         //
             endRenderPass(),
         }},

        // Plain CLEAR RP, then two S/R chains both loading depth.
        // Phase 1: normal RP with CLEAR(establishes depth + LRZ).
        // Chain 1: Phase 2(suspend, LOAD) -> Phase 3(resume, STORE).
        // Chain 2: Phase 4(suspend, LOAD) -> Phase 5(resume, STORE).
        // Tests concurrent binning WAR tracking when initial depth comes
        // from a non - S/R renderpass followed by two merged S/R chains.
        {"clear_then_two_sr_chains",
         {
             beginRenderPass(),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({1, 0, 0, 1}, 0.6f),         //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({0, 1, 0, 1}, 0.5f),         //
             suspendResumeRendering(),         //
             draw({1, 1, 0, 1}, 0.4f),         //
             endRenderPass(),
             beginRenderPass(VK_ATTACHMENT_LOAD_OP_LOAD),
             bindPipeline(VK_COMPARE_OP_LESS), //
             draw({0, 1, 1, 1}, 0.3f),         //
             suspendResumeRendering(),         //
             draw({0, 0, 1, 1}, 0.2f),         //
             endRenderPass(),
         }},
    };

    tcu::TestContext &testCtx = lrzTests->getTestContext();

    auto createCaseGroup = [&](const char *groupName, const std::vector<TestParams> &testParamsVect)
    {
        de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, groupName));
        for (const auto &testParams : testParamsVect)
            testGroup->addChild(new BaseTestCase(testCtx, testParams.name, testParams, groupParams));
        lrzTests->addChild(testGroup.release());
    };

    createCaseGroup("direction_change", directionChangeVect);
    createCaseGroup("direction_preserve", directionPreserveVect);
    createCaseGroup("blend", blendVect);
    createCaseGroup("edge", edgeVect);
    createCaseGroup("stencil", stencilVect);
    createCaseGroup("fragment_shader", fragmentShaderVect);
    createCaseGroup("cross_renderpass", crossRenderpassVect);

    if (groupParams->useSecondaryCmdBuffer && !groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
        createCaseGroup("secondary", secondaryVect);

    if ((groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING && !groupParams->useSecondaryCmdBuffer) ||
        (groupParams->useSecondaryCmdBuffer && groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass))
    {
        createCaseGroup("suspend_resume", suspendResumeVect);
        createCaseGroup("cross_renderpass_suspend_resume", crossRenderpassSuspendResumeVect);
    }

    // note different fuzz tests are created for each flag combination in groupParams
    lrzTests->addChild(createFuzzGroup(16, 0xba5eba11, testCtx, groupParams));
}

static void cleanupGroup(tcu::TestCaseGroup *, const SharedGroupParams)
{
    // destroy singleton object
    g_sharedData.reset(nullptr);
}

tcu::TestCaseGroup *createRenderPassLowResolutionZTests(tcu::TestContext &testCtx, const SharedGroupParams groupParams)
{
    return createTestGroup(testCtx, "low_resolution_z", createChildren, groupParams, cleanupGroup);
}

} // namespace vkt::renderpass
