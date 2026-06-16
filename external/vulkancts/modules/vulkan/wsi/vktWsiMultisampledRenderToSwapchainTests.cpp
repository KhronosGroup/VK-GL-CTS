/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 Google Inc.
 * Copyright (c) 2025 The Khronos Group Inc.
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
 * \brief VK_EXT_multisampled_render_to_swapchain extension tests
 *//*--------------------------------------------------------------------*/

#include "vktWsiMultisampledRenderToSwapchainTests.hpp"

#include "../pipeline/vktPipelineMakeUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkPlatform.hpp"
#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkPrograms.hpp"
#include "vkImageUtil.hpp"
#include "vkPipelineConstructionUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktNativeObjectsUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkWsiUtil.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deRandom.hpp"
#include "deMath.h"

#include "tcuVector.hpp"
#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"

#include <string>
#include <vector>

namespace vkt
{
namespace wsi
{
namespace
{
using namespace vk;
using namespace vkt::pipeline;

using PipelineSp = de::SharedPtr<Unique<VkPipeline>>;

struct VerificationResults
{
    uint32_t color1Verification;
    uint32_t color2Verification;
    uint32_t color3Verification;
};

struct VerifySingleFloatPushConstants
{
    tcu::UVec4 area;
    tcu::Vec4 color;
    uint32_t attachmentNdx;
};

struct VerifySingleIntPushConstants
{
    tcu::UVec4 area;
    tcu::IVec4 color;
    uint32_t attachmentNdx;
};

const VkImageUsageFlags commonImageUsageFlags =
    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
const VkImageUsageFlags colorImageUsageFlags =
    commonImageUsageFlags | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

//! The parameters that define a test case
struct TestParams
{
    vk::wsi::Type wsiType;

    uint32_t frameCount;

    VkFormat swapchainFormat;  //!< Color attachment format
    VkFormat floatColorFormat; //!< Color attachment format
    VkFormat intColorFormat;   //!< Color attachment format

    VkClearValue clearValues[3];

    bool renderToWholeFramebuffer; //!< Whether the test should render to the whole framebuffer.
    bool dynamicRendering;         //!< Whether the test should use dynamic rendering.

    VkSampleCountFlagBits numSamples; //!< Pipeline samples

    int32_t swapchainLocation;
    int32_t floatColorLocation;
    int32_t intColorLocation;

    // Used to carry forward the rng seed from test generation to test run.
    uint32_t rngSeed;

    TestParams() : swapchainFormat{}, floatColorFormat{}, intColorFormat{}, clearValues{}
    {
    }
};

struct Image
{
    Move<VkImage> image;
    de::MovePtr<Allocation> alloc;
    Move<VkImageView> view;

    void allocate(const DeviceInterface &vk, const VkDevice device, Allocator &allocator, const VkFormat format,
                  const tcu::UVec2 &size, const VkImageUsageFlags usage, const uint32_t layerCount);
    Move<VkImageView> makeView(const DeviceInterface &vk, const VkDevice device, const VkFormat format,
                               const uint32_t layerCount);
};

CustomInstance createInstanceWithWsi(Context &context, vk::wsi::Type wsiType)
{
    const auto version = context.getUsedApiVersion();

    std::vector<std::string> requiredExtensions;

    requiredExtensions.push_back("VK_KHR_surface");
    requiredExtensions.push_back(getExtensionName(wsiType));
    if (isDisplaySurface(wsiType))
        requiredExtensions.push_back("VK_KHR_display");

    requiredExtensions.push_back("VK_KHR_get_surface_capabilities2");

    std::vector<std::string> requestedExtensions;
    for (const auto &extensionName : requiredExtensions)
    {
        if (!vk::isCoreInstanceExtension(version, extensionName))
            requestedExtensions.push_back(extensionName);
    }

    return vkt::createCustomInstanceWithExtensions(context, requestedExtensions);
}

struct InstanceHelper
{
    const std::vector<vk::VkExtensionProperties> supportedExtensions;
    CustomInstance instance;
    const vk::InstanceDriver &vki;

    InstanceHelper(Context &context, vk::wsi::Type wsiType)
        : supportedExtensions(enumerateInstanceExtensionProperties(context.getPlatformInterface(), nullptr))
        , instance(createInstanceWithWsi(context, wsiType))
        , vki(instance.getDriver())
    {
    }
};

vk::Move<vk::VkDevice> createDeviceWithWsi(const vk::PlatformInterface &vkp, vk::VkInstance instance,
                                           const vk::InstanceInterface &vki, vk::VkPhysicalDevice physicalDevice,
                                           const uint32_t queueFamilyIndex, const TestParams &params)
{
    const float queuePriorities[]                  = {1.0f};
    const vk::VkDeviceQueueCreateInfo queueInfos[] = {{vk::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr,
                                                       (vk::VkDeviceQueueCreateFlags)0, queueFamilyIndex,
                                                       DE_LENGTH_OF_ARRAY(queuePriorities), &queuePriorities[0]}};
    std::vector<const char *> extensions = {"VK_KHR_swapchain", "VK_EXT_multisampled_render_to_single_sampled",
                                            "VK_EXT_multisampled_render_to_swapchain"};

    vk::VkPhysicalDeviceMultisampledRenderToSwapchainFeaturesEXT mrts_features = initVulkanStructure();
    mrts_features.multisampledRenderToSwapchain                                = VK_TRUE;

    vk::VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesEXT mrtss_features =
        initVulkanStructure(&mrts_features);
    mrtss_features.multisampledRenderToSingleSampled = VK_TRUE;

    vk::VkPhysicalDeviceFeatures2 physicalDeviceFeatures2 = initVulkanStructure(&mrtss_features);
    physicalDeviceFeatures2.features.sampleRateShading    = VK_TRUE;

    vk::VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures = initVulkanStructure();
    dynamicRenderingFeatures.dynamicRendering                             = VK_TRUE;
    if (params.dynamicRendering)
    {
        extensions.push_back("VK_KHR_dynamic_rendering");
        mrts_features.pNext = &dynamicRenderingFeatures;
    }

    const vk::VkDeviceCreateInfo deviceParams = {vk::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                                 &physicalDeviceFeatures2,
                                                 (vk::VkDeviceCreateFlags)0,
                                                 DE_LENGTH_OF_ARRAY(queueInfos),
                                                 &queueInfos[0],
                                                 0u,                                       // enabledLayerCount
                                                 nullptr,                                  // ppEnabledLayerNames
                                                 static_cast<uint32_t>(extensions.size()), // enabledExtensionCount
                                                 extensions.data(),                        // ppEnabledExtensionNames
                                                 nullptr};

    return createCustomDevice(vkp, instance, vki, physicalDevice, &deviceParams);
}

struct DeviceHelper
{
    const vk::VkPhysicalDevice physicalDevice;
    const uint32_t queueFamilyIndex;
    const vk::Unique<vk::VkDevice> device;
    const vk::DeviceDriver vkd;
    const vk::VkQueue queue;

    DeviceHelper(Context &context, const vk::InstanceInterface &vki, vk::VkInstance instance, vk::VkSurfaceKHR surface,
                 const TestParams &params)
        : physicalDevice(chooseDevice(vki, instance, context.getTestContext().getCommandLine()))
        , queueFamilyIndex(vk::wsi::chooseQueueFamilyIndex(vki, physicalDevice, surface))
        , device(createDeviceWithWsi(context.getPlatformInterface(), instance, vki, physicalDevice, queueFamilyIndex,
                                     params))
        , vkd(context.getPlatformInterface(), instance, *device, context.getUsedApiVersion(),
              context.getTestContext().getCommandLine())
        , queue(getDeviceQueue(vkd, *device, queueFamilyIndex, 0))
    {
    }
};

vk::VkSwapchainCreateInfoKHR getBasicSwapchainParameters(const vk::InstanceInterface &vki,
                                                         vk::VkPhysicalDevice physicalDevice, vk::VkSurfaceKHR surface,
                                                         const TestParams &params)
{
    vk::VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = vk::initVulkanStructure();
    surfaceInfo.surface                             = surface;

    vk::VkSwapchainFlagsSurfaceCapabilitiesEXT swapchainFlagsSurfaceCapabilities = vk::initVulkanStructure();

    vk::VkSurfaceCapabilities2KHR surfaceCapabilities2 = vk::initVulkanStructure(&swapchainFlagsSurfaceCapabilities);
    VK_CHECK(vki.getPhysicalDeviceSurfaceCapabilities2KHR(physicalDevice, &surfaceInfo, &surfaceCapabilities2));

    if ((swapchainFlagsSurfaceCapabilities.swapchainSupportedFlags &
         VK_SWAPCHAIN_CREATE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_BIT_EXT) == 0)
        TCU_THROW(NotSupportedError,
                  "VK_SWAPCHAIN_CREATE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_BIT_EXT not supported for swapchain");

    const VkSurfaceCapabilitiesKHR capabilities = surfaceCapabilities2.surfaceCapabilities;

    const std::vector<vk::VkSurfaceFormatKHR> formats =
        vk::wsi::getPhysicalDeviceSurfaceFormats(vki, physicalDevice, surface);
    uint32_t formatNdx = (uint32_t)formats.size();
    for (uint32_t i = 0; i < formats.size(); ++i)
    {
        if (formats[i].format == params.swapchainFormat)
        {
            formatNdx = i;
            break;
        }
    }
    if (formatNdx == (uint32_t)formats.size())
        TCU_THROW(NotSupportedError, "Swapchain format not supported");

    const vk::VkSurfaceTransformFlagBitsKHR transform =
        (capabilities.supportedTransforms & vk::VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ?
            vk::VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR :
            capabilities.currentTransform;

    const VkSwapchainCreateFlagsKHR createFlags   = VK_SWAPCHAIN_CREATE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_BIT_EXT;
    const vk::VkSwapchainCreateInfoKHR parameters = {vk::VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                                     nullptr,
                                                     createFlags,
                                                     surface,
                                                     capabilities.minImageCount,
                                                     formats[formatNdx].format,
                                                     formats[formatNdx].colorSpace,
                                                     capabilities.currentExtent,
                                                     1u,
                                                     vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | commonImageUsageFlags,
                                                     vk::VK_SHARING_MODE_EXCLUSIVE,
                                                     0u,
                                                     nullptr,
                                                     transform,
                                                     vk::VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                                     vk::VK_PRESENT_MODE_FIFO_KHR,
                                                     VK_FALSE,
                                                     VK_NULL_HANDLE};

    return parameters;
}

// Accumulate objects throughout the test to avoid them getting deleted before the command buffer is submitted and waited on.
// Speeds up the test by avoiding making multiple submissions and waits.
class TestData
{
public:
    TestData(Context &contextIn, const TestParams &params);

    void beginCommandBuffer();
    void submitCommandsAndWait();
    void present();
    VkImage getSwapchainImage()
    {
        return swapchainImages[imageNdx];
    }
    VkImageView getSwapchainImageView()
    {
        return *swapchainImageViews[imageNdx];
    }

private:
    Context &context;

public:
    const tcu::UVec2 desiredSize = {256, 256};
    InstanceHelper instHelper;
    NativeObjects native;
    Move<VkSurfaceKHR> surface;
    DeviceHelper devHelper;
    const DeviceInterface &vkd;
    VkDevice device;
    SimpleAllocator allocator;
    VkSwapchainCreateInfoKHR swapchainInfo;
    Move<VkSwapchainKHR> swapchain;
    std::vector<VkImage> swapchainImages;
    std::vector<Move<VkImageView>> swapchainImageViews;

    Move<VkCommandPool> cmdPool;
    Move<VkCommandBuffer> cmdBuffer;
    Move<VkCommandBuffer> presentCmdBuffer;
    std::vector<PipelineSp> computePipelines;
    de::MovePtr<GraphicsPipelineWrapper> graphicsPipeline;
    std::vector<Move<VkDescriptorPool>> descriptorPools;
    std::vector<Move<VkDescriptorSet>> descriptorSets;
    RenderPassWrapper renderPassFramebuffer;
    RenderPassWrapper dataRenderPassFramebuffer;

    tcu::UVec2 framebufferSize; //!< Size of the framebuffer
    tcu::UVec4 renderArea;      //!< Render area

    Move<VkBuffer> vertexBuffer;                     //!< Contains a fullscreen triangle
    de::MovePtr<Allocation> vertexBufferAlloc;       //!< Storage for vertexBuffer
    Move<VkBuffer> verificationBuffer;               //!< Buffer used for validation
    de::MovePtr<Allocation> verificationBufferAlloc; //!< Storage for verificationBuffer
    Move<VkBuffer> singleVerificationBuffer; //!< Buffer used for validation of attachments outside the render area
    de::MovePtr<Allocation> singleVerificationBufferAlloc; //!< Storage for singleVerificationBuffer

    //!< Color attachments
    Image floatColor;
    Image intColor;

    //!< Verification results for logging (an array of 5 to avoid hitting maxPerStageDescriptorStorageImages limit of 4.
    Image verify;

    Move<VkSemaphore> imageAvailableSemaphore;
    uint32_t imageNdx;
};

Move<VkImage> makeImage(const DeviceInterface &vk, const VkDevice device, const VkFormat format, const tcu::UVec2 &size,
                        const uint32_t layerCount, const VkImageUsageFlags usage)
{
    const VkImageCreateFlags createFlags = VK_IMAGE_CREATE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_BIT_EXT;

    const VkImageCreateInfo imageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        createFlags,                         // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        format,                              // VkFormat format;
        makeExtent3D(size.x(), size.y(), 1), // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        layerCount,                          // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        usage,                               // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };
    return createImage(vk, device, &imageParams);
}

void Image::allocate(const DeviceInterface &vk, const VkDevice device, Allocator &allocator, const VkFormat format,
                     const tcu::UVec2 &size, const VkImageUsageFlags usage, const uint32_t layerCount)
{
    image = makeImage(vk, device, format, size, layerCount, usage);
    alloc = bindImage(vk, device, allocator, *image, MemoryRequirement::Any);
    view  = makeView(vk, device, format, layerCount);
}

Move<VkImageView> Image::makeView(const DeviceInterface &vk, const VkDevice device, const VkFormat format,
                                  const uint32_t layerCount)
{
    return makeImageView(vk, device, *image, layerCount > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
                         format, makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, layerCount));
}

//! Create a test-specific MSAA pipeline
de::MovePtr<GraphicsPipelineWrapper> makeGraphicsPipeline(
    const InstanceInterface &vki, const DeviceInterface &vk, const VkPhysicalDevice physicalDevice,
    const VkDevice device, const std::vector<std::string> &deviceExtensions,
    const PipelineLayoutWrapper &pipelineLayout, const VkRenderPass renderPass,
    VkPipelineRenderingCreateInfoKHR *pipelineRenderingCreateInfo, const ShaderWrapper vertexModule,
    const ShaderWrapper fragmentModule, const bool enableBlend, const uint32_t intWriteMask,
    const int32_t integerAttachmentLocation, const tcu::UVec4 &viewportIn, const tcu::UVec4 &scissorIn,
    const VkSampleCountFlagBits numSamples, const bool singleAttachment = false)
{
    std::vector<VkVertexInputBindingDescription> vertexInputBindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions;

    // Vertex attributes: position
    vertexInputBindingDescriptions.push_back(
        makeVertexInputBindingDescription(0u, sizeof(tcu::Vec4), VK_VERTEX_INPUT_RATE_VERTEX));
    vertexInputAttributeDescriptions.push_back(
        makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u));

    const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,    // VkStructureType sType;
        nullptr,                                                      // const void* pNext;
        (VkPipelineVertexInputStateCreateFlags)0,                     // VkPipelineVertexInputStateCreateFlags flags;
        static_cast<uint32_t>(vertexInputBindingDescriptions.size()), // uint32_t vertexBindingDescriptionCount;
        dataOrNullPtr(
            vertexInputBindingDescriptions), // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        static_cast<uint32_t>(vertexInputAttributeDescriptions.size()), // uint32_t vertexAttributeDescriptionCount;
        dataOrNullPtr(
            vertexInputAttributeDescriptions), // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };

    const VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                     // const void* pNext;
        (VkPipelineInputAssemblyStateCreateFlags)0,                  // VkPipelineInputAssemblyStateCreateFlags flags;
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,                         // VkPrimitiveTopology topology;
        VK_FALSE,                                                    // VkBool32 primitiveRestartEnable;
    };

    const std::vector<VkViewport> viewports{{
        static_cast<float>(viewportIn.x()), static_cast<float>(viewportIn.y()), // x, y
        static_cast<float>(viewportIn.z()), static_cast<float>(viewportIn.w()), // width, height
        0.0f, 1.0f                                                              // minDepth, maxDepth
    }};

    const std::vector<VkRect2D> scissors = {{
        makeOffset2D(scissorIn.x(), scissorIn.y()),
        makeExtent2D(scissorIn.z(), scissorIn.w()),
    }};

    const VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        (VkPipelineRasterizationStateCreateFlags)0,                 // VkPipelineRasterizationStateCreateFlags flags;
        VK_FALSE,                                                   // VkBool32 depthClampEnable;
        VK_FALSE,                                                   // VkBool32 rasterizerDiscardEnable;
        VK_POLYGON_MODE_FILL,                                       // VkPolygonMode polygonMode;
        VK_CULL_MODE_NONE,                                          // VkCullModeFlags cullMode;
        VK_FRONT_FACE_COUNTER_CLOCKWISE,                            // VkFrontFace frontFace;
        VK_FALSE,                                                   // VkBool32 depthBiasEnable;
        0.0f,                                                       // float depthBiasConstantFactor;
        0.0f,                                                       // float depthBiasClamp;
        0.0f,                                                       // float depthBiasSlopeFactor;
        1.0f,                                                       // float lineWidth;
    };

    VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        (VkPipelineMultisampleStateCreateFlags)0,                 // VkPipelineMultisampleStateCreateFlags flags;
        numSamples,                                               // VkSampleCountFlagBits rasterizationSamples;
        VK_TRUE,                                                  // VkBool32 sampleShadingEnable;
        1.0f,                                                     // float minSampleShading;
        nullptr,                                                  // const VkSampleMask* pSampleMask;
        VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
        VK_FALSE                                                  // VkBool32 alphaToOneEnable;
    };

    // Always blend by addition.  This is used to verify the combination of multiple draw calls.
    const VkColorComponentFlags colorComponentsAll =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendAttachmentState defaultBlendAttachmentState = {
        enableBlend ? VK_TRUE : VK_FALSE, // VkBool32 blendEnable;
        VK_BLEND_FACTOR_ONE,              // VkBlendFactor srcColorBlendFactor;
        VK_BLEND_FACTOR_ONE,              // VkBlendFactor dstColorBlendFactor;
        VK_BLEND_OP_ADD,                  // VkBlendOp colorBlendOp;
        VK_BLEND_FACTOR_ONE,              // VkBlendFactor srcAlphaBlendFactor;
        VK_BLEND_FACTOR_ONE,              // VkBlendFactor dstAlphaBlendFactor;
        VK_BLEND_OP_ADD,                  // VkBlendOp alphaBlendOp;
        colorComponentsAll,               // VkColorComponentFlags colorWriteMask;
    };

    VkPipelineColorBlendAttachmentState blendAttachmentStates[4] = {
        defaultBlendAttachmentState,
        defaultBlendAttachmentState,
        defaultBlendAttachmentState,
        defaultBlendAttachmentState,
    };

    if (enableBlend && integerAttachmentLocation >= 0)
    {
        // Disable blend for the integer attachment unconditionally
        blendAttachmentStates[integerAttachmentLocation].blendEnable = VK_FALSE;
        // But emulate it by outputting to one channel only.
        blendAttachmentStates[integerAttachmentLocation].colorWriteMask =
            ((intWriteMask & 1) != 0 ? VK_COLOR_COMPONENT_R_BIT : 0) |
            ((intWriteMask & 2) != 0 ? VK_COLOR_COMPONENT_G_BIT : 0) |
            ((intWriteMask & 4) != 0 ? VK_COLOR_COMPONENT_B_BIT : 0) |
            ((intWriteMask & 8) != 0 ? VK_COLOR_COMPONENT_A_BIT : 0);
        DE_ASSERT(blendAttachmentStates[integerAttachmentLocation].colorWriteMask != 0);
    }

    const VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        (VkPipelineColorBlendStateCreateFlags)0,                  // VkPipelineColorBlendStateCreateFlags flags;
        VK_FALSE,                                                 // VkBool32 logicOpEnable;
        VK_LOGIC_OP_COPY,                                         // VkLogicOp logicOp;
        singleAttachment ? 1u : 4u,                               // uint32_t attachmentCount;
        blendAttachmentStates,    // const VkPipelineColorBlendAttachmentState* pAttachments;
        {0.0f, 0.0f, 0.0f, 0.0f}, // float blendConstants[4];
    };

    de::MovePtr<GraphicsPipelineWrapper> graphicsPipeline =
        de::MovePtr<GraphicsPipelineWrapper>(new GraphicsPipelineWrapper(
            vki, vk, physicalDevice, device, deviceExtensions, PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, 0u));
    graphicsPipeline.get()
        ->setMonolithicPipelineLayout(pipelineLayout)
        .setupVertexInputState(&vertexInputStateInfo, &pipelineInputAssemblyStateInfo)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass, 0u, vertexModule,
                                          &pipelineRasterizationStateInfo, ShaderWrapper(), ShaderWrapper(),
                                          ShaderWrapper(), nullptr, nullptr, pipelineRenderingCreateInfo)
        .setupFragmentShaderState(pipelineLayout, renderPass, 0u, fragmentModule, nullptr,
                                  &pipelineMultisampleStateInfo)
        .setRenderingColorAttachmentsInfo(pipelineRenderingCreateInfo)
        .setupFragmentOutputState(renderPass, 0u, &pipelineColorBlendStateInfo, &pipelineMultisampleStateInfo)
        .buildPipeline();

    return graphicsPipeline;
}

void logTestImages(Context &context, const TestParams &params, TestData &td)
{
    const DeviceInterface &vk = td.vkd;
    const VkDevice device     = td.device;

    tcu::TestLog &log = context.getTestContext().getLog();

    const VkDeviceSize bufferSize[3] = {
        td.framebufferSize.x() * td.framebufferSize.y() * tcu::getPixelSize(mapVkFormat(params.swapchainFormat)),
        td.framebufferSize.x() * td.framebufferSize.y() * tcu::getPixelSize(mapVkFormat(params.floatColorFormat)),
        td.framebufferSize.x() * td.framebufferSize.y() * tcu::getPixelSize(mapVkFormat(params.intColorFormat)),
    };
    const Move<VkBuffer> buffer[3] = {
        makeBuffer(vk, device, bufferSize[0], VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        makeBuffer(vk, device, bufferSize[1], VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        makeBuffer(vk, device, bufferSize[2], VK_BUFFER_USAGE_TRANSFER_DST_BIT),
    };
    const de::MovePtr<Allocation> bufferAlloc[3] = {
        bindBuffer(vk, device, td.allocator, *buffer[0], MemoryRequirement::HostVisible),
        bindBuffer(vk, device, td.allocator, *buffer[1], MemoryRequirement::HostVisible),
        bindBuffer(vk, device, td.allocator, *buffer[2], MemoryRequirement::HostVisible),
    };

    for (uint32_t bufferNdx = 0; bufferNdx < 3; ++bufferNdx)
        invalidateAlloc(vk, device, *bufferAlloc[bufferNdx]);

    const Unique<VkCommandPool> cmdPool(
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, td.devHelper.queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(makeCommandBuffer(vk, device, *cmdPool));

    beginCommandBuffer(vk, *cmdBuffer);

    const tcu::IVec2 size(td.framebufferSize.x(), td.framebufferSize.y());
    {
        copyImageToBuffer(vk, *cmdBuffer, td.getSwapchainImage(), *buffer[0], size, VK_ACCESS_SHADER_WRITE_BIT,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
        copyImageToBuffer(vk, *cmdBuffer, *td.floatColor.image, *buffer[1], size, VK_ACCESS_SHADER_WRITE_BIT,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
        copyImageToBuffer(vk, *cmdBuffer, *td.intColor.image, *buffer[2], size, VK_ACCESS_SHADER_WRITE_BIT,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
    }

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, td.devHelper.queue, *cmdBuffer);

    const tcu::ConstPixelBufferAccess testImageData[3] = {
        {mapVkFormat(params.swapchainFormat), size.x(), size.y(), 1, bufferAlloc[0]->getHostPtr()},
        {mapVkFormat(params.floatColorFormat), size.x(), size.y(), 1, bufferAlloc[1]->getHostPtr()},
        {mapVkFormat(params.intColorFormat), size.x(), size.y(), 1, bufferAlloc[2]->getHostPtr()},
    };

    log << tcu::TestLog::ImageSet("attachments", "attachments");
    log << tcu::TestLog::Image("Color attachment 1", "Color attachment 1", testImageData[0]);
    log << tcu::TestLog::Image("Color attachment 2", "Color attachment 2", testImageData[1]);
    log << tcu::TestLog::Image("Color attachment 3", "Color attachment 3", testImageData[2]);
    log << tcu::TestLog::EndImageSet;
}

void logVerifyImages(Context &context, TestData &td)
{
    const DeviceInterface &vk = td.vkd;
    const VkDevice device     = td.device;
    tcu::TestLog &log         = context.getTestContext().getLog();

    const VkDeviceSize bufferSize =
        td.framebufferSize.x() * td.framebufferSize.y() * 5 * tcu::getPixelSize(mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM));
    const Move<VkBuffer> buffer = makeBuffer(vk, device, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const de::MovePtr<Allocation> bufferAlloc =
        bindBuffer(vk, device, td.allocator, *buffer, MemoryRequirement::HostVisible);

    invalidateAlloc(vk, device, *bufferAlloc);

    const Unique<VkCommandPool> cmdPool(
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, td.devHelper.queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(makeCommandBuffer(vk, device, *cmdPool));

    beginCommandBuffer(vk, *cmdBuffer);

    copyImageToBuffer(vk, *cmdBuffer, *td.verify.image, *buffer,
                      tcu::IVec2(td.framebufferSize.x(), td.framebufferSize.y()), VK_ACCESS_SHADER_WRITE_BIT,
                      VK_IMAGE_LAYOUT_GENERAL, 5);

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, td.devHelper.queue, *cmdBuffer);

    const tcu::ConstPixelBufferAccess verifyImageData(mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM), td.framebufferSize.x(),
                                                      td.framebufferSize.y(), 5, bufferAlloc->getHostPtr());

    log << tcu::TestLog::ImageSet("attachment error mask", "attachment error mask");
    log << tcu::TestLog::Image(
        "ErrorMask color attachment 1", "Error mask color attachment 1",
        tcu::getSubregion(verifyImageData, 0, 0, 0, td.framebufferSize.x(), td.framebufferSize.y(), 1));
    log << tcu::TestLog::Image(
        "ErrorMask color attachment 2", "Error mask color attachment 2",
        tcu::getSubregion(verifyImageData, 0, 0, 1, td.framebufferSize.x(), td.framebufferSize.y(), 1));
    log << tcu::TestLog::Image(
        "ErrorMask color attachment 3", "Error mask color attachment 3",
        tcu::getSubregion(verifyImageData, 0, 0, 2, td.framebufferSize.x(), td.framebufferSize.y(), 1));
    log << tcu::TestLog::EndImageSet;
}

bool checkAndReportError(Context &context, const uint32_t verifiedPixelCount, const uint32_t expectedPixelCount,
                         const std::string &attachment)
{
    tcu::TestLog &log = context.getTestContext().getLog();

    bool passed = verifiedPixelCount == expectedPixelCount;

    if (passed)
        log << tcu::TestLog::Message << "Verification passed for " << attachment << tcu::TestLog::EndMessage;
    else
        log << tcu::TestLog::Message << "Verification failed for " << attachment << " for "
            << (expectedPixelCount - verifiedPixelCount) << " pixel(s)" << tcu::TestLog::EndMessage;

    return passed;
}

void checkSampleRequirements(Context &context, const VkSampleCountFlagBits numSamples)
{
    const VkPhysicalDeviceLimits &limits = context.getDeviceProperties().limits;

    if ((limits.framebufferColorSampleCounts & numSamples) == 0u)
        TCU_THROW(NotSupportedError, "framebufferColorSampleCounts: sample count not supported");
}

void checkImageRequirements(Context &context, const VkFormat format, const VkFormatFeatureFlags requiredFeatureFlags,
                            const VkImageUsageFlags requiredUsageFlags, const VkSampleCountFlagBits requiredSampleCount,
                            VkImageFormatProperties &imageProperties)
{
    const InstanceInterface &vki          = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

    const VkFormatProperties formatProperties = getPhysicalDeviceFormatProperties(vki, physicalDevice, format);

    if ((formatProperties.optimalTilingFeatures & requiredFeatureFlags) != requiredFeatureFlags)
        TCU_THROW(NotSupportedError, (de::toString(format) + ": format features not supported").c_str());

    const VkImageCreateFlags createFlags = requiredSampleCount == VK_SAMPLE_COUNT_1_BIT ?
                                               VK_IMAGE_CREATE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_BIT_EXT :
                                               0;

    const VkResult result =
        vki.getPhysicalDeviceImageFormatProperties(physicalDevice, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                                                   requiredUsageFlags, createFlags, &imageProperties);

    if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
        TCU_THROW(NotSupportedError, (de::toString(format) + ": format not supported").c_str());

    if ((imageProperties.sampleCounts & requiredSampleCount) != requiredSampleCount)
        TCU_THROW(NotSupportedError, (de::toString(format) + ": sample count not supported").c_str());
}

TestData::TestData(Context &contextIn, const TestParams &params)
    : context(contextIn)
    , instHelper(context, params.wsiType)
    , native(context, instHelper.supportedExtensions, params.wsiType, 1u, tcu::just(desiredSize))
    , surface(createSurface(instHelper.vki, instHelper.instance, params.wsiType, native.getDisplay(),
                            native.getWindow(), context.getTestContext().getCommandLine()))
    , devHelper(context, instHelper.vki, instHelper.instance, *surface, params)
    , vkd(devHelper.vkd)
    , device(*devHelper.device)
    , allocator(vkd, device, getPhysicalDeviceMemoryProperties(instHelper.vki, devHelper.physicalDevice))
    , swapchainInfo(getBasicSwapchainParameters(instHelper.vki, devHelper.physicalDevice, *surface, params))
    , swapchain(createSwapchainKHR(vkd, device, &swapchainInfo))
    , swapchainImages(vk::wsi::getSwapchainImages(vkd, device, *swapchain))
    , cmdPool(createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                contextIn.getUniversalQueueFamilyIndex()))
    , cmdBuffer(makeCommandBuffer(vkd, device, *cmdPool))
    , presentCmdBuffer(makeCommandBuffer(vkd, device, *cmdPool))
    , imageAvailableSemaphore(createSemaphore(vkd, device))
    , imageNdx(0u)
{
    for (const auto &swapchainImage : swapchainImages)
    {
        swapchainImageViews.push_back(
            makeImageView(vkd, device, swapchainImage, VK_IMAGE_VIEW_TYPE_2D, params.swapchainFormat,
                          makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u)));
    }
}

void TestData::beginCommandBuffer()
{
    vk::beginCommandBuffer(vkd, *cmdBuffer);
}

void TestData::submitCommandsAndWait()
{
    VK_CHECK(vkd.endCommandBuffer(*cmdBuffer));

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    vk::submitCommandsAndWait(vkd, device, devHelper.queue, *cmdBuffer, false, 0u, 1u, &*imageAvailableSemaphore,
                              &waitStage);
}

void TestData::present()
{
    vk::beginCommandBuffer(vkd, *presentCmdBuffer);

    const VkImageMemoryBarrier imageBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType            sType
        nullptr,                                // const void*                pNext
        VK_ACCESS_TRANSFER_READ_BIT,            // VkAccessFlags            srcAccessMask
        0u,                                     // VkAccessFlags            dstAccessMask
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,   // VkImageLayout            oldLayout
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,        // VkImageLayout            newLayout
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                    srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                    dstQueueFamilyIndex
        getSwapchainImage(),                    // VkImage                    image
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u,
                                  1u), // VkImageSubresourceRange    subresourceRange
    };
    vkd.cmdPipelineBarrier(*presentCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u,
                           0u, nullptr, 0u, nullptr, 1u, &imageBarrier);
    VK_CHECK(vkd.endCommandBuffer(*presentCmdBuffer));
    vk::submitCommandsAndWait(vkd, device, devHelper.queue, *presentCmdBuffer);

    VkPresentInfoKHR presentInfo = {
        VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, // VkStructureType			sType;
        nullptr,                            // const void*				pNext;
        0u,                                 // uint32_t				    waitSemaphoreCount;
        nullptr,                            // const VkSemaphore*		pWaitSemaphores;
        1u,                                 // uint32_t				    swapchainCount;
        &*swapchain,                        // const VkSwapchainKHR*	pSwapchains;
        &imageNdx,                          // const uint32_t*			pImageIndices;
        nullptr                             // VkResult*				pResults;
    };
    vkd.queuePresentKHR(devHelper.queue, &presentInfo);
}

void initializeAttachments(TestData &td, std::vector<VkImage> &images, std::vector<VkImageView> &attachments,
                           int32_t attachmentNdxes[3])
{
    int32_t currentNdx = 0;

    // Output attachments
    {
        images.push_back(td.getSwapchainImage());
        attachments.push_back(td.getSwapchainImageView());
        attachmentNdxes[0] = currentNdx++;
    }
    {
        images.push_back(td.floatColor.image.get());
        attachments.push_back(td.floatColor.view.get());
        attachmentNdxes[1] = currentNdx++;
    }
    {
        images.push_back(td.intColor.image.get());
        attachments.push_back(td.intColor.view.get());
        attachmentNdxes[2] = currentNdx++;
    }
}

void initializeAttachmentDescriptions(const TestParams &params, std::vector<VkAttachmentDescription2> &descs,
                                      uint32_t &attachmentUseMask)
{
    // The attachments are either cleared already or should be cleared now.  If an attachment was used in a previous render pass,
    // it will override these values to always LOAD and use the SHADER_READ_ONLY layout.  It's SHADER_READ_ONLY because final layout
    // is always that for simplicity.
    const VkAttachmentLoadOp loadOp   = VK_ATTACHMENT_LOAD_OP_LOAD;
    const VkImageLayout initialLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    // Output attachments
    {
        descs.push_back(VkAttachmentDescription2{
            VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2, // VkStructureType                  sType;
            nullptr,                                    // const void*                      pNext;
            (VkAttachmentDescriptionFlags)0,            // VkAttachmentDescriptionFlags flags;
            params.swapchainFormat,                     // VkFormat format;
            VK_SAMPLE_COUNT_1_BIT,                      // VkSampleCountFlagBits samples;
            (attachmentUseMask & (1 << 0)) != 0 ? VK_ATTACHMENT_LOAD_OP_LOAD : loadOp, // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,                                              // VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE, // VkAttachmentStoreOp stencilStoreOp;
            (attachmentUseMask & (1 << 0)) != 0 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL :
                                                  initialLayout, // VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL             // VkImageLayout finalLayout;
        });
        attachmentUseMask |= 1 << 0;
    }

    {
        descs.push_back(VkAttachmentDescription2{
            VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2, // VkStructureType                  sType;
            nullptr,                                    // const void*                      pNext;
            (VkAttachmentDescriptionFlags)0,            // VkAttachmentDescriptionFlags flags;
            params.floatColorFormat,                    // VkFormat format;
            VK_SAMPLE_COUNT_1_BIT,                      // VkSampleCountFlagBits samples;
            (attachmentUseMask & (1 << 1)) != 0 ? VK_ATTACHMENT_LOAD_OP_LOAD : loadOp, // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,                                              // VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE, // VkAttachmentStoreOp stencilStoreOp;
            (attachmentUseMask & (1 << 1)) != 0 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL :
                                                  initialLayout, // VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL             // VkImageLayout finalLayout;
        });
        attachmentUseMask |= 1 << 1;
    }

    {
        descs.push_back(VkAttachmentDescription2{
            VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2, // VkStructureType                  sType;
            nullptr,                                    // const void*                      pNext;
            (VkAttachmentDescriptionFlags)0,            // VkAttachmentDescriptionFlags flags;
            params.intColorFormat,                      // VkFormat format;
            VK_SAMPLE_COUNT_1_BIT,                      // VkSampleCountFlagBits samples;
            (attachmentUseMask & (1 << 2)) != 0 ? VK_ATTACHMENT_LOAD_OP_LOAD : loadOp, // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,                                              // VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE, // VkAttachmentStoreOp stencilStoreOp;
            (attachmentUseMask & (1 << 2)) != 0 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL :
                                                  initialLayout, // VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL             // VkImageLayout finalLayout;
        });
        attachmentUseMask |= 1 << 2;
    }
}

void initializeRenderingAttachmentInfos(const TestParams &params, TestData &td,
                                        std::vector<VkRenderingAttachmentInfo> &colorAttachmentInfos,
                                        std::vector<VkFormat> &colorAttachmentFormats, uint32_t &attachmentUseMask)
{
    // The attachments are either cleared already or should be cleared now. If an attachment was used in a previous render pass,
    // it will override these values to always LOAD and use the SHADER_READ_ONLY layout. It's SHADER_READ_ONLY because final layout
    // is always that for simplicity.
    const VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

    const VkRenderingAttachmentInfo emptyRenderingAttachmentInfo = {
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, // VkStructureType            sType
        nullptr,                                     // const void*                pNext
        VK_NULL_HANDLE,                              // VkImageView                imageView
        VK_IMAGE_LAYOUT_UNDEFINED,                   // VkImageLayout            imageLayout
        VK_RESOLVE_MODE_NONE,                        // VkResolveModeFlagBits    resolveMode
        VK_NULL_HANDLE,                              // VkImageView                resolveImageView
        VK_IMAGE_LAYOUT_UNDEFINED,                   // VkImageLayout            resolveImageLayout
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,             // VkAttachmentLoadOp        loadOp
        VK_ATTACHMENT_STORE_OP_DONT_CARE,            // VkAttachmentStoreOp        storeOp
        params.clearValues[0]                        // VkClearValue                clearValue
    };

    for (auto &colorAttachmentInfo : colorAttachmentInfos)
    {
        colorAttachmentInfo = emptyRenderingAttachmentInfo;
    }

    // Output attachments
    {
        VkRenderingAttachmentInfo renderingAttachmentInfo = {
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, // VkStructureType            sType
            nullptr,                                     // const void*                pNext
            td.getSwapchainImageView(),                  // VkImageView                imageView
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,    // VkImageLayout            imageLayout
            VK_RESOLVE_MODE_NONE,                        // VkResolveModeFlagBits    resolveMode
            VK_NULL_HANDLE,                              // VkImageView                resolveImageView
            VK_IMAGE_LAYOUT_UNDEFINED,                   // VkImageLayout            resolveImageLayout
            (attachmentUseMask & (1 << 0)) != 0 ? VK_ATTACHMENT_LOAD_OP_LOAD :
                                                  loadOp, // VkAttachmentLoadOp        loadOp
            VK_ATTACHMENT_STORE_OP_STORE,                 // VkAttachmentStoreOp        storeOp
            params.clearValues[0]                         // VkClearValue                clearValue
        };

        colorAttachmentInfos[params.swapchainLocation]   = renderingAttachmentInfo;
        colorAttachmentFormats[params.swapchainLocation] = params.swapchainFormat;
        attachmentUseMask |= 1 << 0;
    }

    {
        VkRenderingAttachmentInfo renderingAttachmentInfo = {
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, // VkStructureType            sType
            nullptr,                                     // const void*                pNext
            td.floatColor.view.get(),                    // VkImageView                imageView
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,    // VkImageLayout            imageLayout
            VK_RESOLVE_MODE_NONE,                        // VkResolveModeFlagBits    resolveMode
            VK_NULL_HANDLE,                              // VkImageView                resolveImageView
            VK_IMAGE_LAYOUT_UNDEFINED,                   // VkImageLayout            resolveImageLayout
            (attachmentUseMask & (1 << 1)) != 0 ? VK_ATTACHMENT_LOAD_OP_LOAD :
                                                  loadOp, // VkAttachmentLoadOp        loadOp
            VK_ATTACHMENT_STORE_OP_STORE,                 // VkAttachmentStoreOp        storeOp
            params.clearValues[1]                         // VkClearValue                clearValue
        };

        colorAttachmentInfos[params.floatColorLocation]   = renderingAttachmentInfo;
        colorAttachmentFormats[params.floatColorLocation] = params.floatColorFormat;
        attachmentUseMask |= 1 << 1;
    }

    {
        VkRenderingAttachmentInfo renderingAttachmentInfo = {
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, // VkStructureType            sType
            nullptr,                                     // const void*                pNext
            td.intColor.view.get(),                      // VkImageView                imageView
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,    // VkImageLayout            imageLayout
            VK_RESOLVE_MODE_NONE,                        // VkResolveModeFlagBits    resolveMode
            VK_NULL_HANDLE,                              // VkImageView                resolveImageView
            VK_IMAGE_LAYOUT_UNDEFINED,                   // VkImageLayout            resolveImageLayout
            (attachmentUseMask & (1 << 2)) != 0 ? VK_ATTACHMENT_LOAD_OP_LOAD :
                                                  loadOp, // VkAttachmentLoadOp        loadOp
            VK_ATTACHMENT_STORE_OP_STORE,                 // VkAttachmentStoreOp        storeOp
            params.clearValues[2]                         // VkClearValue                clearValue
        };

        colorAttachmentInfos[params.intColorLocation]   = renderingAttachmentInfo;
        colorAttachmentFormats[params.intColorLocation] = params.intColorFormat;
        attachmentUseMask |= 1 << 2;
    }
}

void preRenderingImageLayoutTransition(TestData &td)
{
    const DeviceInterface &vk = td.vkd;

    const VkImageMemoryBarrier imageBarrierTemplate = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,      // VkStructureType            sType;
        nullptr,                                     // const void*                pNext;
        (VkAccessFlags)VK_ACCESS_TRANSFER_WRITE_BIT, // VkAccessFlags              srcAccessMask;
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // VkAccessFlags              dstAccessMask;
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,     // VkImageLayout              oldLayout;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout              newLayout;
        VK_QUEUE_FAMILY_IGNORED,                  // uint32_t                   srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                  // uint32_t                   dstQueueFamilyIndex;
        VK_NULL_HANDLE,                           // VkImage                    image;
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u,
                                  1u), // VkImageSubresourceRange    subresourceRange;
    };

    VkImageMemoryBarrier barriers[3] = {imageBarrierTemplate, imageBarrierTemplate, imageBarrierTemplate};
    barriers[0].image                = td.getSwapchainImage();
    barriers[1].image                = *td.floatColor.image;
    barriers[2].image                = *td.intColor.image;

    vk.cmdPipelineBarrier(*td.cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                              VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                          0u, 0u, nullptr, 0u, nullptr, DE_LENGTH_OF_ARRAY(barriers), barriers);
}

void postRenderingImageLayoutTransition(TestData &td)
{
    const DeviceInterface &vk                       = td.vkd;
    const VkImageMemoryBarrier imageBarrierTemplate = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType            sType
        nullptr,                                // const void*                pNext
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, // VkAccessFlags            srcAccessMask
        VK_ACCESS_SHADER_READ_BIT,                        // VkAccessFlags            dstAccessMask
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,         // VkImageLayout            oldLayout
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,         // VkImageLayout            newLayout
        VK_QUEUE_FAMILY_IGNORED,                          // uint32_t                    srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                          // uint32_t                    dstQueueFamilyIndex
        VK_NULL_HANDLE,                                   // VkImage                    image
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u,
                                  1u), // VkImageSubresourceRange    subresourceRange
    };

    std::vector<VkImageMemoryBarrier> barriers;

    {
        barriers.push_back(imageBarrierTemplate);
        barriers.back().image = td.getSwapchainImage();
    }

    {
        barriers.push_back(imageBarrierTemplate);
        barriers.back().image = *td.floatColor.image;
    }

    {
        barriers.push_back(imageBarrierTemplate);
        barriers.back().image = *td.intColor.image;
    }

    if (!barriers.empty())
    {
        vk.cmdPipelineBarrier(*td.cmdBuffer,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 0u, nullptr,
                              static_cast<uint32_t>(barriers.size()), barriers.data());
    }
}

void preinitializeAttachmentReferences(std::vector<VkAttachmentReference2> &references, const uint32_t count)
{
    references.resize(count, VkAttachmentReference2{
                                 VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, // VkStructureType       sType;
                                 nullptr,                                  // const void*           pNext;
                                 VK_ATTACHMENT_UNUSED,                     // uint32_t              attachment;
                                 VK_IMAGE_LAYOUT_UNDEFINED,                // VkImageLayout         layout;
                                 0,                                        // VkImageAspectFlags    aspectMask;
                             });
}

void initializeAttachmentReference(VkAttachmentReference2 &reference, uint32_t attachment)
{

    reference.attachment = attachment;
    reference.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    reference.aspectMask = VkImageAspectFlags(VK_IMAGE_ASPECT_COLOR_BIT);
}

void addSubpassDescription(const TestParams &params, std::vector<VkAttachmentReference2> &attachmentReferences,
                           std::vector<uint32_t> *preserveAttachments,
                           VkMultisampledRenderToSingleSampledInfoEXT &msrtss,
                           std::vector<VkSubpassDescription2> &subpasses,
                           const std::vector<VkAttachmentReference2> &inputAttachmentReferences,
                           const int32_t attachmentNdxes[3])
{
    // Maximum 3 attachment references for color
    preinitializeAttachmentReferences(attachmentReferences, 4);

    initializeAttachmentReference(attachmentReferences[params.swapchainLocation], attachmentNdxes[0]);
    initializeAttachmentReference(attachmentReferences[params.floatColorLocation], attachmentNdxes[1]);
    initializeAttachmentReference(attachmentReferences[params.intColorLocation], attachmentNdxes[2]);

    // Append MSRTSS to subpass desc
    msrtss = VkMultisampledRenderToSingleSampledInfoEXT{
        VK_STRUCTURE_TYPE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_INFO_EXT, // VkStructureType            sType
        nullptr,                                                          // const void*                pNext
        VK_TRUE,           // VkBool32                    multisampledRenderToSingleSampledEnable
        params.numSamples, // VkSampleCountFlagBits    rasterizationSamples
    };

    VkSubpassDescription2 subpassDescription = {
        VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2, // VkStructureType                 sType;
        &msrtss,                                 // const void*                     pNext;
        (VkSubpassDescriptionFlags)0,            // VkSubpassDescriptionFlags       flags;
        VK_PIPELINE_BIND_POINT_GRAPHICS,         // VkPipelineBindPoint             pipelineBindPoint;
        0u,                                      // uint32_t                        viewMask;
        static_cast<uint32_t>(
            inputAttachmentReferences.size()),    // uint32_t                        inputAttachmentCount;
        dataOrNullPtr(inputAttachmentReferences), // const VkAttachmentReference2*   pInputAttachments;
        4u,                                       // uint32_t                        colorAttachmentCount;
        dataOrNullPtr(attachmentReferences),      // const VkAttachmentReference2*   pColorAttachments;
        nullptr,                                  // const VkAttachmentReference2*   pResolveAttachments;
        nullptr,                                  // const VkAttachmentReference2*   pDepthStencilAttachment;
        preserveAttachments ? static_cast<uint32_t>(preserveAttachments->size()) :
                              0, // uint32_t                        preserveAttachmentCount;
        preserveAttachments ? dataOrNullPtr(*preserveAttachments) :
                              nullptr, // const uint32_t*                 pPreserveAttachments;
    };

    subpasses.push_back(subpassDescription);
}

void createRenderPassAndFramebuffer(TestData &td, const std::vector<VkImage> &images,
                                    const std::vector<VkImageView> &attachments,
                                    const std::vector<VkAttachmentDescription2> &attachmentDescriptions,
                                    const std::vector<VkSubpassDescription2> &subpasses,
                                    const std::vector<VkSubpassDependency2> &subpassDependencies)
{
    const DeviceInterface &vk = td.vkd;
    const VkDevice device     = td.device;

    const VkRenderPassCreateInfo2 renderPassInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,          // VkStructureType sType;
        nullptr,                                              // const void* pNext;
        (VkRenderPassCreateFlags)0,                           // VkRenderPassCreateFlags flags;
        static_cast<uint32_t>(attachmentDescriptions.size()), // uint32_t attachmentCount;
        dataOrNullPtr(attachmentDescriptions),                // const VkAttachmentDescription2* pAttachments;
        static_cast<uint32_t>(subpasses.size()),              // uint32_t subpassCount;
        dataOrNullPtr(subpasses),                             // const VkSubpassDescription2* pSubpasses;
        static_cast<uint32_t>(subpassDependencies.size()),    // uint32_t dependencyCount;
        dataOrNullPtr(subpassDependencies),                   // const VkSubpassDependency2* pDependencies;
        0u,      // uint32_t                         correlatedViewMaskCount;
        nullptr, // const uint32_t*                  pCorrelatedViewMasks;
    };

    td.renderPassFramebuffer = RenderPassWrapper(PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, vk, device, &renderPassInfo);
    td.renderPassFramebuffer.createFramebuffer(vk, device, static_cast<uint32_t>(attachments.size()),
                                               dataOrNullPtr(images), dataOrNullPtr(attachments),
                                               td.framebufferSize.x(), td.framebufferSize.y());
}

void createTestData(const TestParams &params, TestData &td)
{
    const DeviceInterface &vk = td.vkd;
    const VkDevice device     = td.device;

    de::Random rng(params.rngSeed);
    td.framebufferSize = tcu::UVec2(rng.getInt(60, 80), rng.getInt(48, 64));
    td.renderArea      = tcu::UVec4(0, 0, td.framebufferSize.x(), td.framebufferSize.y());
    if (!params.renderToWholeFramebuffer)
    {
        td.renderArea.x() += rng.getInt(5, 15);
        td.renderArea.y() += rng.getInt(5, 15);
        td.renderArea.z() -= td.renderArea.x() + rng.getInt(2, 12);
        td.renderArea.w() -= td.renderArea.y() + rng.getInt(2, 12);
    }

    // Create images
    {
        td.floatColor.allocate(vk, device, td.allocator, params.floatColorFormat, td.framebufferSize,
                               colorImageUsageFlags, 1);
        td.intColor.allocate(vk, device, td.allocator, params.intColorFormat, td.framebufferSize, colorImageUsageFlags,
                             1);

        td.verify.allocate(
            vk, device, td.allocator, VK_FORMAT_R8G8B8A8_UNORM, td.framebufferSize,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, 5);
    }

    // Create vertex and verification buffers
    {
        // A fullscreen triangle
        const std::vector<tcu::Vec4> vertices = {
            tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
            tcu::Vec4(3.0f, -1.0f, 0.0f, 1.0f),
            tcu::Vec4(-1.0f, 3.0f, 0.0f, 1.0f),
        };

        const VkDeviceSize vertexBufferSize = static_cast<VkDeviceSize>(sizeof(vertices[0]) * vertices.size());
        td.vertexBuffer      = makeBuffer(vk, device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        td.vertexBufferAlloc = bindBuffer(vk, device, td.allocator, *td.vertexBuffer, MemoryRequirement::HostVisible);

        deMemcpy(td.vertexBufferAlloc->getHostPtr(), dataOrNullPtr(vertices),
                 static_cast<std::size_t>(vertexBufferSize));
        flushMappedMemoryRange(vk, device, td.vertexBufferAlloc->getMemory(), td.vertexBufferAlloc->getOffset(),
                               VK_WHOLE_SIZE);

        // Initialize the verification data with 0.
        const VerificationResults results = {};

        td.verificationBuffer = makeBuffer(vk, device, sizeof(results), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        td.verificationBufferAlloc =
            bindBuffer(vk, device, td.allocator, *td.verificationBuffer, MemoryRequirement::HostVisible);

        td.singleVerificationBuffer = makeBuffer(vk, device, sizeof(results), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        td.singleVerificationBufferAlloc =
            bindBuffer(vk, device, td.allocator, *td.singleVerificationBuffer, MemoryRequirement::HostVisible);
    }
}

void prepareVerificationBuffers(TestData &td)
{
    const DeviceInterface &vk = td.vkd;
    const VkDevice device     = td.device;

    const VerificationResults results = {};
    deMemcpy(td.verificationBufferAlloc->getHostPtr(), &results, sizeof(results));
    flushMappedMemoryRange(vk, device, td.verificationBufferAlloc->getMemory(), td.verificationBufferAlloc->getOffset(),
                           VK_WHOLE_SIZE);

    deMemcpy(td.singleVerificationBufferAlloc->getHostPtr(), &results, sizeof(results));
    flushMappedMemoryRange(vk, device, td.singleVerificationBufferAlloc->getMemory(),
                           td.singleVerificationBufferAlloc->getOffset(), VK_WHOLE_SIZE);
}

void checkRequirements(Context &context, TestParams params)
{
    const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();
    const vk::InstanceInterface &vki      = context.getInstanceInterface();

    context.requireDeviceFunctionality("VK_KHR_create_renderpass2");

    if (params.dynamicRendering)
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

    context.requireDeviceFunctionality("VK_EXT_multisampled_render_to_swapchain");
    context.requireDeviceFunctionality("VK_EXT_multisampled_render_to_single_sampled");

    // Check extension feature
    {
        VkPhysicalDeviceMultisampledRenderToSingleSampledFeaturesEXT msrtssFeatures = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_FEATURES_EXT,
            nullptr,
            VK_FALSE,
        };
        VkPhysicalDeviceFeatures2 physicalDeviceFeatures = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            &msrtssFeatures,
            {},
        };

        vki.getPhysicalDeviceFeatures2(physicalDevice, &physicalDeviceFeatures);

        if (msrtssFeatures.multisampledRenderToSingleSampled != VK_TRUE)
        {
            TCU_THROW(NotSupportedError, "multisampledRenderToSingleSampled not supported");
        }
    }

    // Check whether formats are supported with the requested usage and sample counts.
    {
        VkImageFormatProperties imageProperties;
        checkImageRequirements(context, params.swapchainFormat,
                               VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
                                   VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,
                               colorImageUsageFlags, VK_SAMPLE_COUNT_1_BIT, imageProperties);
        if ((imageProperties.sampleCounts & params.numSamples) != params.numSamples)
            TCU_THROW(NotSupportedError,
                      (de::toString(params.swapchainFormat) + ": sample count not supported").c_str());
    }
    {
        VkImageFormatProperties imageProperties;
        checkImageRequirements(context, params.floatColorFormat,
                               VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
                                   VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,
                               colorImageUsageFlags, VK_SAMPLE_COUNT_1_BIT, imageProperties);
        if (params.floatColorLocation >= 0 && (imageProperties.sampleCounts & params.numSamples) != params.numSamples)
            TCU_THROW(NotSupportedError,
                      (de::toString(params.floatColorFormat) + ": sample count not supported").c_str());
    }
    {
        VkImageFormatProperties imageProperties;
        checkImageRequirements(context, params.intColorFormat,
                               VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,
                               colorImageUsageFlags, VK_SAMPLE_COUNT_1_BIT, imageProperties);
        if (params.intColorLocation >= 0 && (imageProperties.sampleCounts & params.numSamples) != params.numSamples)
            TCU_THROW(NotSupportedError,
                      (de::toString(params.intColorFormat) + ": sample count not supported").c_str());
    }

    {
        // Check whether sample counts used for rendering are acceptable
        checkSampleRequirements(context, params.numSamples);
    }

    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);
}

void generateRandomClearValues(de::Random &rng, const TestParams &params, VkClearValue clearValues[3], bool smallValues)
{
    const bool usesSignedIntFormat = params.intColorFormat == VK_FORMAT_R16G16B16A16_SINT;

    const float minFloatValue  = 0.05f;
    const float maxFloatValue  = smallValues ? 0.1f : 0.95f;
    const uint32_t minIntValue = smallValues ? 20 : 5000;
    const uint32_t maxIntValue = smallValues ? 100 : 10000;

    clearValues[0].color.float32[0] = rng.getFloat(minFloatValue, maxFloatValue);
    clearValues[0].color.float32[1] = rng.getFloat(minFloatValue, maxFloatValue);
    clearValues[0].color.float32[2] = rng.getFloat(minFloatValue, maxFloatValue);
    clearValues[0].color.float32[3] = rng.getFloat(minFloatValue, maxFloatValue);
    clearValues[1].color.float32[0] = rng.getFloat(minFloatValue, maxFloatValue);
    clearValues[1].color.float32[1] = rng.getFloat(minFloatValue, maxFloatValue);
    clearValues[1].color.float32[2] = rng.getFloat(minFloatValue, maxFloatValue);
    clearValues[1].color.float32[3] = rng.getFloat(minFloatValue, maxFloatValue);
    clearValues[2].color.int32[0]   = (usesSignedIntFormat ? -1 : 1) * rng.getInt(minIntValue, maxIntValue);
    clearValues[2].color.int32[1]   = (usesSignedIntFormat ? -1 : 1) * rng.getInt(minIntValue, maxIntValue);
    clearValues[2].color.int32[2]   = (usesSignedIntFormat ? -1 : 1) * rng.getInt(minIntValue, maxIntValue);
    clearValues[2].color.int32[3]   = (usesSignedIntFormat ? -1 : 1) * rng.getInt(minIntValue, maxIntValue);
}

void clearImagesBeforeDraw(const TestParams &params, TestData &td)
{
    const DeviceInterface &vk = td.vkd;

    const VkImageMemoryBarrier imageBarrierTemplate = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType            sType;
        nullptr,                                // const void*                pNext;
        0,                                      // VkAccessFlags              srcAccessMask;
        VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags              dstAccessMask;
        VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout              oldLayout;
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout              newLayout;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                   srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                   dstQueueFamilyIndex;
        VK_NULL_HANDLE,                         // VkImage                    image;
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u,
                                  1u), // VkImageSubresourceRange    subresourceRange;
    };

    VkImageMemoryBarrier preClearBarriers[3] = {imageBarrierTemplate, imageBarrierTemplate, imageBarrierTemplate};
    preClearBarriers[0].image                = td.getSwapchainImage();
    preClearBarriers[1].image                = *td.floatColor.image;
    preClearBarriers[2].image                = *td.intColor.image;

    vk.cmdPipelineBarrier(*td.cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                          nullptr, 0u, nullptr, DE_LENGTH_OF_ARRAY(preClearBarriers), preClearBarriers);

    vk.cmdClearColorImage(*td.cmdBuffer, td.getSwapchainImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          &params.clearValues[0].color, 1, &preClearBarriers[0].subresourceRange);
    vk.cmdClearColorImage(*td.cmdBuffer, *td.floatColor.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          &params.clearValues[1].color, 1, &preClearBarriers[1].subresourceRange);
    vk.cmdClearColorImage(*td.cmdBuffer, *td.intColor.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          &params.clearValues[2].color, 1, &preClearBarriers[2].subresourceRange);

    const VkMemoryBarrier postClearBarrier = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER,                                           // VkStructureType    sType;
        nullptr,                                                                    // const void*        pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,                                               // VkAccessFlags      srcAccessMask;
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // VkAccessFlags      dstAccessMask;
    };

    vk.cmdPipelineBarrier(*td.cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                              VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                          0u, 1u, &postClearBarrier, 0u, nullptr, 0u, nullptr);
}

void startRenderPass(TestData &td, const uint32_t clearValueCount, const VkClearValue *clearValues)
{
    const DeviceInterface &vk = td.vkd;

    const VkRect2D renderArea = {{static_cast<int32_t>(td.renderArea.x()), static_cast<int32_t>(td.renderArea.y())},
                                 {td.renderArea.z(), td.renderArea.w()}};

    td.renderPassFramebuffer.begin(vk, *td.cmdBuffer, renderArea, clearValueCount, clearValues);
}

void startRendering(const TestParams &params, TestData &td, uint32_t colorAttachmentCount,
                    std::vector<VkRenderingAttachmentInfo> &colorAttachmentInfos)
{
    const DeviceInterface &vk = td.vkd;

    // Append MSRTSS to subpass desc
    VkMultisampledRenderToSingleSampledInfoEXT msrtss = {
        VK_STRUCTURE_TYPE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_INFO_EXT, // VkStructureType            sType
        nullptr,                                                          // const void*                pNext
        VK_TRUE,          // VkBool32                    multisampledRenderToSingleSampledEnable
        params.numSamples // VkSampleCountFlagBits    rasterizationSamples
    };

    const VkRect2D renderArea = {{static_cast<int32_t>(td.renderArea.x()), static_cast<int32_t>(td.renderArea.y())},
                                 {td.renderArea.z(), td.renderArea.w()}};

    VkRenderingInfo renderingInfo = {
        VK_STRUCTURE_TYPE_RENDERING_INFO, // VkStructureType                        sType
        nullptr,                          // const void*                            pNext
        (VkRenderingFlags)0,              // VkRenderingFlags                        flags
        renderArea,                       // VkRect2D                                renderArea
        1u,                               // uint32_t                                layerCount
        0u,                               // uint32_t                                viewMask
        colorAttachmentCount,             // uint32_t                                colorAttachmentCount
        colorAttachmentInfos.data(),      // const VkRenderingAttachmentInfo*        pColorAttachments
        nullptr,                          // const VkRenderingAttachmentInfo*        pDepthAttachment
        nullptr                           // const VkRenderingAttachmentInfo*        pStencilAttachment
    };

    renderingInfo.pNext = &msrtss;

    vk.cmdBeginRendering(*td.cmdBuffer, &renderingInfo);
}

void postDrawBarrier(TestData &td)
{
    const DeviceInterface &vk = td.vkd;

    const VkMemoryBarrier barrier = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER,     // VkStructureType    sType;
        nullptr,                              // const void*        pNext;
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // VkAccessFlags      srcAccessMask;
        VK_ACCESS_SHADER_READ_BIT,            // VkAccessFlags      dstAccessMask;
    };

    vk.cmdPipelineBarrier(*td.cmdBuffer,
                          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 1u, &barrier, 0u, nullptr, 0u, nullptr);
}

void setupVerifyDescriptorSetAndPipeline(Context &context, TestData &td, const VkPushConstantRange *pushConstantRange,
                                         Move<VkPipelineLayout> &verifyPipelineLayout)
{
    const DeviceInterface &vk = td.vkd;
    const VkDevice device     = td.device;

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    td.descriptorPools.emplace_back(DescriptorPoolBuilder()
                                        .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
                                        .addType(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 5u)
                                        .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u)
                                        .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    td.descriptorSets.emplace_back(makeDescriptorSet(vk, device, *td.descriptorPools.back(), *descriptorSetLayout));

    const VkDescriptorBufferInfo resultBufferInfo =
        makeDescriptorBufferInfo(*td.verificationBuffer, 0ull, sizeof(VerificationResults));
    const VkDescriptorImageInfo color1ImageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, td.getSwapchainImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    const VkDescriptorImageInfo color2ImageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, *td.floatColor.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    const VkDescriptorImageInfo color3ImageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, *td.intColor.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    const VkDescriptorImageInfo verifyImageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, *td.verify.view, VK_IMAGE_LAYOUT_GENERAL);

    DescriptorSetUpdateBuilder builder;

    builder.writeSingle(*td.descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(0u),
                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultBufferInfo);
    builder.writeSingle(*td.descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(1u),
                        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &color1ImageInfo);
    builder.writeSingle(*td.descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(2u),
                        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &color2ImageInfo);
    builder.writeSingle(*td.descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(3u),
                        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &color3ImageInfo);
    builder.writeSingle(*td.descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(6u),
                        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &verifyImageInfo);

    builder.update(vk, device);

    const Unique<VkShaderModule> verifyModule(
        createShaderModule(vk, device, context.getBinaryCollection().get("comp"), 0u));

    verifyPipelineLayout = makePipelineLayout(vk, device, 1, &*descriptorSetLayout, 1, pushConstantRange);

    td.computePipelines.push_back(
        PipelineSp(new Unique<VkPipeline>(makeComputePipeline(vk, device, *verifyPipelineLayout, *verifyModule))));

    vk.cmdBindPipeline(*td.cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, **td.computePipelines.back());
    vk.cmdBindDescriptorSets(*td.cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *verifyPipelineLayout, 0u, 1u,
                             &td.descriptorSets.back().get(), 0u, nullptr);
}

void postVerifyBarrier(TestData &td, const Move<VkBuffer> &verificationBuffer)
{
    const DeviceInterface &vk = td.vkd;

    const VkBufferMemoryBarrier barrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType    sType;
        nullptr,                                 // const void*        pNext;
        VK_ACCESS_SHADER_WRITE_BIT,              // VkAccessFlags      srcAccessMask;
        VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags      dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t           srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t           dstQueueFamilyIndex;
        *verificationBuffer,                     // VkBuffer           buffer;
        0ull,                                    // VkDeviceSize       offset;
        VK_WHOLE_SIZE,                           // VkDeviceSize       size;
    };

    vk.cmdPipelineBarrier(*td.cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u,
                          nullptr, 1u, &barrier, 0u, nullptr);
}

void dispatchVerifyConstantColor(Context &context, TestData &td, VkImageView imageView, const VkImageLayout layout,
                                 const Move<VkImageView> &verifyImageView, const Move<VkBuffer> &verificationBuffer,
                                 const uint32_t pushConstantSize, const void *pushConstants,
                                 const std::string &shaderName)
{
    const DeviceInterface &vk = td.vkd;
    const VkDevice device     = td.device;

    // Set up descriptor set
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    td.descriptorPools.emplace_back(DescriptorPoolBuilder()
                                        .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
                                        .addType(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1u)
                                        .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u)
                                        .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    td.descriptorSets.emplace_back(makeDescriptorSet(vk, device, *td.descriptorPools.back(), *descriptorSetLayout));

    const VkDescriptorBufferInfo resultBufferInfo =
        makeDescriptorBufferInfo(*verificationBuffer, 0ull, sizeof(VerificationResults));
    const VkDescriptorImageInfo imageInfo = makeDescriptorImageInfo(VK_NULL_HANDLE, imageView, layout);
    const VkDescriptorImageInfo verifyImageInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, *verifyImageView, VK_IMAGE_LAYOUT_GENERAL);

    DescriptorSetUpdateBuilder builder;

    builder.writeSingle(*td.descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(0u),
                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultBufferInfo);
    builder.writeSingle(*td.descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(1u),
                        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageInfo);
    builder.writeSingle(*td.descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(2u),
                        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &verifyImageInfo);

    builder.update(vk, device);

    // Setup pipeline
    const VkPushConstantRange &verifyPushConstantRange = {
        VK_SHADER_STAGE_COMPUTE_BIT, // VkShaderStageFlags    stageFlags;
        0,                           // uint32_t              offset;
        pushConstantSize,            // uint32_t              size;
    };

    const Unique<VkShaderModule> verifyModule(
        createShaderModule(vk, device, context.getBinaryCollection().get(shaderName), 0u));
    const Unique<VkPipelineLayout> verifyPipelineLayout(
        makePipelineLayout(vk, device, 1, &*descriptorSetLayout, 1, &verifyPushConstantRange));

    td.computePipelines.push_back(
        PipelineSp(new Unique<VkPipeline>(makeComputePipeline(vk, device, *verifyPipelineLayout, *verifyModule))));

    vk.cmdBindPipeline(*td.cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, **td.computePipelines.back());
    vk.cmdBindDescriptorSets(*td.cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *verifyPipelineLayout, 0u, 1u,
                             &td.descriptorSets.back().get(), 0u, nullptr);

    const VkMemoryBarrier preVerifyBarrier = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER,                       // VkStructureType    sType;
        nullptr,                                                // const void*        pNext;
        VK_ACCESS_SHADER_WRITE_BIT,                             // VkAccessFlags      srcAccessMask;
        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, // VkAccessFlags      dstAccessMask;
    };

    vk.cmdPipelineBarrier(*td.cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          (VkDependencyFlags)0, 1u, &preVerifyBarrier, 0u, nullptr, 0u, nullptr);

    // Area is always the first uvec4
    const tcu::UVec4 *area = static_cast<const tcu::UVec4 *>(pushConstants);

    vk.cmdPushConstants(*td.cmdBuffer, *verifyPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, pushConstantSize,
                        pushConstants);
    vk.cmdDispatch(*td.cmdBuffer, (area->z() + 7) / 8, (area->w() + 7) / 8, 1);

    postVerifyBarrier(td, verificationBuffer);
}

void testStart(TestData &td)
{
    const DeviceInterface &vk = td.vkd;
    const VkDevice device     = td.device;

    vk.acquireNextImageKHR(device, *td.swapchain, std::numeric_limits<uint64_t>::max(), *td.imageAvailableSemaphore,
                           VK_NULL_HANDLE, &td.imageNdx);

    td.beginCommandBuffer();

    // Clear verify image
    {
        VkImageMemoryBarrier clearBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType            sType;
            nullptr,                                // const void*                pNext;
            0,                                      // VkAccessFlags              srcAccessMask;
            VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags              dstAccessMask;
            VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout              oldLayout;
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout              newLayout;
            VK_QUEUE_FAMILY_IGNORED,                // uint32_t                   srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                // uint32_t                   dstQueueFamilyIndex;
            *td.verify.image,                       // VkImage                    image;
            makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u,
                                      5u), // VkImageSubresourceRange    subresourceRange;
        };

        vk.cmdPipelineBarrier(*td.cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                              nullptr, 0u, nullptr, 1u, &clearBarrier);

        VkClearColorValue clearToBlack;
        clearToBlack.float32[0] = 0;
        clearToBlack.float32[1] = 0;
        clearToBlack.float32[2] = 0;
        clearToBlack.float32[3] = 1.0;
        vk.cmdClearColorImage(*td.cmdBuffer, *td.verify.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearToBlack, 1,
                              &clearBarrier.subresourceRange);
    }

    // Transition it to GENERAL
    {
        VkImageMemoryBarrier verifyBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType            sType;
            nullptr,                                // const void*                pNext;
            VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags              srcAccessMask;
            VK_ACCESS_SHADER_WRITE_BIT,             // VkAccessFlags              dstAccessMask;
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout              oldLayout;
            VK_IMAGE_LAYOUT_GENERAL,                // VkImageLayout              newLayout;
            VK_QUEUE_FAMILY_IGNORED,                // uint32_t                   srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                // uint32_t                   dstQueueFamilyIndex;
            *td.verify.image,                       // VkImage                    image;
            makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u,
                                      5u), // VkImageSubresourceRange    subresourceRange;
        };

        vk.cmdPipelineBarrier(*td.cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u,
                              0u, nullptr, 0u, nullptr, 1u, &verifyBarrier);
    }
}

void testEnd(Context &context, const TestParams &params, TestData &td)
{
    // If not rendering to the whole framebuffer and the images were cleared before the render pass, verify that the area outside the render pass is untouched.
    const bool verifyOutsideRenderArea = !params.renderToWholeFramebuffer;
    if (verifyOutsideRenderArea)
    {
        const DeviceInterface &vk = td.vkd;
        const VkDevice device     = td.device;

        const tcu::UVec4 verifyAreas[] = {
            tcu::UVec4(0, 0, td.framebufferSize.x(), td.renderArea.y()),
            tcu::UVec4(0, td.renderArea.y(), td.renderArea.x(), td.renderArea.w()),
            tcu::UVec4(td.renderArea.x() + td.renderArea.z(), td.renderArea.y(),
                       td.framebufferSize.x() - td.renderArea.x() - td.renderArea.z(), td.renderArea.w()),
            tcu::UVec4(0, td.renderArea.y() + td.renderArea.w(), td.framebufferSize.x(),
                       td.framebufferSize.y() - td.renderArea.y() - td.renderArea.w()),
        };

        for (uint32_t areaNdx = 0; areaNdx < DE_LENGTH_OF_ARRAY(verifyAreas); ++areaNdx)
        {
            const VerifySingleFloatPushConstants verifyColor1 = {
                verifyAreas[areaNdx],
                tcu::Vec4(params.clearValues[0].color.float32[0], params.clearValues[0].color.float32[1],
                          params.clearValues[0].color.float32[2], params.clearValues[0].color.float32[3]),
                0,
            };
            dispatchVerifyConstantColor(context, td, td.getSwapchainImageView(),
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, td.verify.view,
                                        td.singleVerificationBuffer, static_cast<uint32_t>(sizeof(verifyColor1)),
                                        &verifyColor1, "comp_singleFloat");

            const VerifySingleFloatPushConstants verifyColor2 = {
                verifyAreas[areaNdx],
                tcu::Vec4(params.clearValues[1].color.float32[0], params.clearValues[1].color.float32[1],
                          params.clearValues[1].color.float32[2], params.clearValues[1].color.float32[3]),
                1,
            };
            dispatchVerifyConstantColor(context, td, *td.floatColor.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        td.verify.view, td.singleVerificationBuffer,
                                        static_cast<uint32_t>(sizeof(verifyColor2)), &verifyColor2, "comp_singleFloat");

            const VerifySingleIntPushConstants verifyColor3 = {
                verifyAreas[areaNdx],
                tcu::IVec4(params.clearValues[2].color.int32[0], params.clearValues[2].color.int32[1],
                           params.clearValues[2].color.int32[2], params.clearValues[2].color.int32[3]),
                2,
            };
            dispatchVerifyConstantColor(context, td, *td.intColor.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        td.verify.view, td.singleVerificationBuffer,
                                        static_cast<uint32_t>(sizeof(verifyColor3)), &verifyColor3, "comp_singleInt");
        }

        invalidateAlloc(vk, device, *td.singleVerificationBufferAlloc);
    }

    td.submitCommandsAndWait();
}

tcu::TestStatus verify(Context &context, const TestParams &params, TestData &td)
{
    logTestImages(context, params, td);

    // Verify draw call results
    {
        const VerificationResults *const results =
            static_cast<const VerificationResults *>(td.verificationBufferAlloc->getHostPtr());
        const uint32_t totalPixels = td.renderArea.z() * td.renderArea.w();
        bool allOk                 = true;
        const char *errorDelim     = "";
        std::string errorMsg       = "Incorrect multisampled rendering for ";

        if (!checkAndReportError(context, results->color1Verification, totalPixels, "color attachment 1"))
        {
            errorMsg += errorDelim;
            errorMsg += "color attachment 1";
            errorDelim = ", ";
            allOk      = false;
        }

        if (!checkAndReportError(context, results->color2Verification, totalPixels, "color attachment 2"))
        {
            errorMsg += errorDelim;
            errorMsg += "color attachment 2";
            errorDelim = ", ";
            allOk      = false;
        }

        if (!checkAndReportError(context, results->color3Verification, totalPixels, "color attachment 3"))
        {
            errorMsg += errorDelim;
            errorMsg += "color attachment 3";
            errorDelim = ", ";
            allOk      = false;
        }

        if (!allOk)
        {
            logVerifyImages(context, td);
            return tcu::TestStatus::fail(errorMsg);
        }
    }

    const bool verifyOutsideRenderArea = !params.renderToWholeFramebuffer;
    if (verifyOutsideRenderArea)
    {
        const VerificationResults *const results =
            static_cast<const VerificationResults *>(td.singleVerificationBufferAlloc->getHostPtr());
        const uint32_t totalPixels =
            td.framebufferSize.x() * td.framebufferSize.y() - td.renderArea.z() * td.renderArea.w();
        bool allOk = true;

        allOk = checkAndReportError(context, results->color1Verification, totalPixels,
                                    "color attachment 1 (outside render area)") &&
                allOk;
        allOk = checkAndReportError(context, results->color2Verification, totalPixels,
                                    "color attachment 2 (outside render area)") &&
                allOk;
        allOk = checkAndReportError(context, results->color3Verification, totalPixels,
                                    "color attachment 3 (outside render area)") &&
                allOk;

        if (!allOk)
        {
            logVerifyImages(context, td);
            return tcu::TestStatus::fail("Detected corruption outside render area");
        }
    }

    return tcu::TestStatus::pass("Pass");
}

void initConstantColorVerifyPrograms(SourceCollections &programCollection, const TestParams params)
{
    const bool usesSignedIntFormat = params.intColorFormat == VK_FORMAT_R16G16B16A16_SINT;

    // Compute shader - Verify outside render area is intact (float colors)
    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "#extension GL_EXT_samplerless_texture_functions : require\n"
            << "\n"
            << "layout(push_constant) uniform PushConstants {\n"
            << "    uvec4 area;\n"
            << "    vec4 color;\n"
            << "    uint attachmentNdx;\n"
            << "} params;\n"
            << "\n"
            << "layout(local_size_x = 8, local_size_y = 8) in;\n"
            << "layout(set = 0, binding = 0, std430) writeonly buffer Output {\n"
            << "    uint colorVerification[3];\n"
            << "} sb_out;\n"
            << "layout(set = 0, binding = 1) uniform texture2D colorImage;\n"
            << "layout(set = 0, binding = 2, rgba8) uniform writeonly image2DArray verify;\n"
            << "\n"
            << "bool v4matches(vec4 a, vec4 b, float error)\n"
            << "{\n"
            << "    return all(lessThan(abs(a - b), vec4(error)));\n"
            << "}\n"
            << "\n"
            << "void main (void)\n"
            << "{\n"
            << "    if (any(greaterThanEqual(gl_GlobalInvocationID.xy, params.area.zw)))\n"
            << "        return;\n"
            << "\n"
            << "    uvec2 coords = params.area.xy + gl_GlobalInvocationID.xy;\n"
            << "\n"
            << "    vec4 result = vec4(1, 0, 0, 1);\n"
            << "    vec4 color = texelFetch(colorImage, ivec2(coords), 0);\n"
            << "    if (v4matches(color, params.color, 0.01))\n"
            << "    {\n"
            << "        atomicAdd(sb_out.colorVerification[params.attachmentNdx], 1);\n"
            << "        result = vec4(0, 1, 0, 1);\n"
            << "    }\n"
            << "    imageStore(verify, ivec3(coords, params.attachmentNdx), result);\n"
            << "}\n";

        programCollection.glslSources.add("comp_singleFloat") << glu::ComputeSource(src.str());
    }

    // Compute shader - Verify outside render area is intact (int colors)
    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "#extension GL_EXT_samplerless_texture_functions : require\n"
            << "\n"
            << "layout(push_constant) uniform PushConstants {\n"
            << "    uvec4 area;\n"
            << "    ivec4 color;\n"
            << "    uint attachmentNdx;\n"
            << "} params;\n"
            << "\n"
            << "layout(local_size_x = 8, local_size_y = 8) in;\n"
            << "layout(set = 0, binding = 0, std430) writeonly buffer Output {\n"
            << "    uint colorVerification[3];\n"
            << "} sb_out;\n"
            << "layout(set = 0, binding = 1) uniform " << (usesSignedIntFormat ? "i" : "u") << "texture2D colorImage;\n"
            << "layout(set = 0, binding = 2, rgba8) uniform writeonly image2DArray verify;\n"
            << "\n"
            << "bool i4matches(ivec4 a, ivec4 b, int error)\n"
            << "{\n"
            << "    return all(lessThanEqual(abs(a - b), ivec4(error)));\n"
            << "}\n"
            << "\n"
            << "void main (void)\n"
            << "{\n"
            << "    if (any(greaterThanEqual(gl_GlobalInvocationID.xy, params.area.zw)))\n"
            << "        return;\n"
            << "\n"
            << "    uvec2 coords = params.area.xy + gl_GlobalInvocationID.xy;\n"
            << "\n"
            << "    vec4 result = vec4(1, 0, 0, 1);\n"
            << "    ivec4 color = ivec4(texelFetch(colorImage, ivec2(coords), 0));\n"
            << "    if (i4matches(color, params.color, 0))\n"
            << "    {\n"
            << "        atomicAdd(sb_out.colorVerification[params.attachmentNdx], 1);\n"
            << "        result = vec4(0, 1, 0, 1);\n"
            << "    }\n"
            << "    imageStore(verify, ivec3(coords, params.attachmentNdx), result);\n"
            << "}\n";

        programCollection.glslSources.add("comp_singleInt") << glu::ComputeSource(src.str());
    }
}

void initBasicPrograms(SourceCollections &programCollection, const TestParams params)
{
    // Vertex shader - position
    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "\n"
            << "layout(location = 0) in  vec4 in_position;\n"
            << "\n"
            << "out gl_PerVertex {\n"
            << "    vec4 gl_Position;\n"
            << "};\n"
            << "\n"
            << "void main(void)\n"
            << "{\n"
            << "    gl_Position = in_position;\n"
            << "}\n";

        programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
    }

    const bool usesSignedIntFormat = params.intColorFormat == VK_FORMAT_R16G16B16A16_SINT;
    const char *intTypePrefix      = usesSignedIntFormat ? "i" : "u";

    // The framebuffer contains four attachments with the same number of samples.
    // The fragment shader outputs a different color per sample (in a gradient) to verify that the multisampled image actually has that many samples:
    //
    // - For samples [4s, 4s+3), the shader outputs:
    //
    //     Vec4(0, v, v, v),
    //     Vec4(v, 0, v, v),
    //     Vec4(v, v, 0, v),
    //     Vec4(v, v, v, 0),
    //
    //   for float attachments where v = 1-s*0.2. For sample s, it outputs:
    //
    //     UVec4(v, v + 1, v + 2, v + 3),
    //
    //   for the int attachment where v = (s+1)*(s+1)*10.
    //
    {

        // The shader outputs up to 16 samples
        const uint32_t numSamples = static_cast<uint32_t>(params.numSamples);

        DE_ASSERT(numSamples <= 16);

        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "\n"
            << "layout(location = " << params.swapchainLocation << ") out vec4 o_color1;\n"
            << "layout(location = " << params.floatColorLocation << ") out vec4 o_color2;\n"
            << "layout(location = " << params.intColorLocation << ") out " << intTypePrefix << "vec4 o_color3;\n"
            << "\n"
            << "layout(push_constant) uniform PushConstants {\n"
            << "    uvec4 area;\n"
            << "} params;\n"
            << "\n"
            << "void main(void)\n"
            << "{\n"
            << "    vec2 uv = (gl_FragCoord.xy - vec2(params.area.xy)) / vec2(params.area.zw);\n";
        for (uint32_t sampleID = 0; sampleID < numSamples; ++sampleID)
        {
            const char *uvComponent = sampleID % 2 == 0 ? "uv.x" : "uv.y";

            const float floatValue  = 1 - static_cast<float>(sampleID / 4) * 0.2f;
            const uint32_t intValue = (sampleID + 1) * (sampleID + 1) * 10;
            const float depthValue  = 1 - static_cast<float>(sampleID ^ 1) / 16.0f;

            const tcu::Vec4 floatChannels(sampleID % 4 == 0 ? 0 : floatValue, sampleID % 4 == 1 ? 0 : floatValue,
                                          sampleID % 4 == 2 ? 0 : floatValue, sampleID % 4 == 3 ? 0 : floatValue);
            const tcu::UVec4 intChannels(intValue, intValue + 1, intValue + 2, intValue + 3);

            src << "    " << (sampleID == 0 ? "" : "else ") << "if (gl_SampleID == " << sampleID << ")\n"
                << "    {\n"
                << "        o_color1 = vec4(" << floatChannels.x() << ", " << floatChannels.y() << ", "
                << floatChannels.z() << ", " << floatChannels.w() << ") * " << uvComponent << ";\n"
                << "        o_color2 = vec4(" << floatChannels.x() << ", " << floatChannels.y() << ", "
                << floatChannels.z() << ", " << floatChannels.w() << ") * " << uvComponent << ";\n"
                << "        o_color3 = " << intTypePrefix << "vec4(vec4(" << intChannels.x() << ", " << intChannels.y()
                << ", " << intChannels.z() << ", " << intChannels.w() << ") * " << uvComponent << ");\n"
                << "        gl_FragDepth = " << depthValue << ";\n"
                << "    }\n";
        }
        src << "}\n";

        programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
    }

    // Compute shader - verify the results of rendering
    //
    // Take the formulas used for the fragment shader.  Note the following:
    //
    //    n-1
    //    sum(1 - s*0.2)
    //     0                 n - (n*(n-1))/2 * 0.2
    //  ----------------- = ----------------------- = 1 - (n-1)*0.1
    //          n                    n
    //
    // When rendering is done to every sample and the attachment is resolved, we expect:
    //
    // - For float attachments, average of:
    //   * Horizontal gradient:
    //
    //       Vec4(0, 1, 1, 1)            if 2 samples
    //       Vec4(0.5v, v, 0.5v, v)        o.w. where v = 1 - (n - 1)*0.1 where n = floor(sampleCount / 4).
    //
    //   * Vertical gradient:
    //
    //       Vec4(1, 0, 1, 1)            if 2 samples
    //       Vec4(v, 0.5v, v, 0.5v)        o.w. where v = 1 - (n - 1)*0.1 where n = floor(sampleCount / 4).
    //
    // - For the int attachments, any of UVec4(v, v + 1, v + 2, v + 3) where v = (s+1)*(s+1)*10
    {

        // The shader outputs up to 16 samples
        const uint32_t numSamples = static_cast<uint32_t>(params.numSamples);

        const float floatValue = 1 - static_cast<float>((numSamples / 4) - 1) * 0.1f;

        const tcu::Vec4 floatExpectHorizontal =
            numSamples == 2 ? tcu::Vec4(0, 1, 1, 1) :
                              tcu::Vec4(0.5f * floatValue, floatValue, 0.5f * floatValue, floatValue);
        const tcu::Vec4 floatExpectVertical =
            numSamples == 2 ? tcu::Vec4(1, 0, 1, 1) :
                              tcu::Vec4(floatValue, 0.5f * floatValue, floatValue, 0.5f * floatValue);

        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "#extension GL_EXT_samplerless_texture_functions : require\n"
            << "\n"
            << "layout(push_constant) uniform PushConstants {\n"
            << "    uvec4 area;\n"
            << "} params;\n"
            << "\n"
            << "layout(local_size_x = 8, local_size_y = 8) in;\n"
            << "layout(set = 0, binding = 0, std430) writeonly buffer Output {\n"
            << "    uint colorVerification[3];\n"
            << "} sb_out;\n"
            << "layout(set = 0, binding = 1) uniform texture2D color1Image;\n"
            << "layout(set = 0, binding = 2) uniform texture2D color2Image;\n"
            << "layout(set = 0, binding = 3) uniform " << (usesSignedIntFormat ? "i" : "u")
            << "texture2D color3Image;\n"
            << "layout(set = 0, binding = 6, rgba8) uniform writeonly image2DArray verify;\n"
            << "\n"
            << "bool fmatches(float a, float b, float error)\n"
            << "{\n"
            << "    return abs(a - b) < error;\n"
            << "}\n"
            << "bool umatches(uint a, uint b, uint error)\n"
            << "{\n"
            << "    return abs(a - b) <= error;\n"
            << "}\n"
            << "bool v4matches(vec4 a, vec4 b, vec4 error)\n"
            << "{\n"
            << "    return all(lessThan(abs(a - b), error));\n"
            << "}\n"
            << "bool i4matchesEither(ivec4 a, ivec4 b, ivec4 c, int errorB, int errorC)\n"
            << "{\n"
            << "    return all(lessThanEqual(abs(a - b), ivec4(errorB))) || all(lessThanEqual(abs(a - c), "
               "ivec4(errorC)));\n"
            << "}\n"
            << "\n"
            << "void main (void)\n"
            << "{\n"
            << "    if (any(greaterThanEqual(gl_GlobalInvocationID.xy, params.area.zw)))\n"
            << "        return;\n"
            << "\n"
            << "    uvec2 coords = params.area.xy + gl_GlobalInvocationID.xy;\n"
            << "    vec2 uv = (vec2(gl_GlobalInvocationID.xy) + vec2(0.5)) / vec2(params.area.zw);\n"
            << "\n"
            << "    vec4 result1 = vec4(1, 0, 0, 1);\n"
            << "    vec4 color1 = texelFetch(color1Image, ivec2(coords), 0);\n"
            << "    vec4 expected1H = vec4(" << floatExpectHorizontal.x() << ", " << floatExpectHorizontal.y() << ", "
            << floatExpectHorizontal.z() << ", " << floatExpectHorizontal.w() << ");\n"
            << "    vec4 expected1V = vec4(" << floatExpectVertical.x() << ", " << floatExpectVertical.y() << ", "
            << floatExpectVertical.z() << ", " << floatExpectVertical.w() << ");\n"
            << "    vec4 expected1 = (expected1H * uv.x + expected1V * uv.y) / 2.0;\n"
            // Allow room for precision errors.  Rendering happens at sample locations while verification uv is in the middle of pixel.
            << "    if (v4matches(color1, expected1, max(expected1H / float(params.area.z), expected1V / "
               "float(params.area.w)) + 2.0/255.0))\n"
            << "    {\n"
            << "        atomicAdd(sb_out.colorVerification[0], 1);\n"
            << "        result1 = vec4(0, 1, 0, 1);\n"
            << "    }\n"
            << "    imageStore(verify, ivec3(coords, 0), result1);\n"
            << "\n"
            << "    vec4 result2 = vec4(1, 0, 0, 1);\n"
            << "    vec4 color2 = texelFetch(color2Image, ivec2(coords), 0);\n"
            // Allow room for precision errors.  Rendering happens at sample locations while verification uv is in the middle of pixel.
            << "    if (v4matches(color2, expected1, max(expected1H / float(params.area.z), expected1V / "
               "float(params.area.w)) + 2.0/1024.0))\n"
            << "    {\n"
            << "        atomicAdd(sb_out.colorVerification[1], 1);\n"
            << "        result2 = vec4(0, 1, 0, 1);\n"
            << "    }\n"
            << "    imageStore(verify, ivec3(coords, 1), result2);\n"
            << "\n"
            << "    vec4 result3 = vec4(1, 0, 0, 1);\n"
            << "    ivec4 color3 = ivec4(texelFetch(color3Image, ivec2(coords), 0));\n"
            << "    if (";
        for (uint32_t sampleID = 0; sampleID < numSamples; ++sampleID)
        {
            const uint32_t intValue = (sampleID + 1) * (sampleID + 1) * 10;
            const tcu::UVec4 intExpect(intValue, intValue + 1, intValue + 2, intValue + 3);

            src << (sampleID == 0 ? "" : "        || ") << "i4matchesEither(color3, ivec4(vec4(" << intExpect.x()
                << ", " << intExpect.y() << ", " << intExpect.z() << ", " << intExpect.w() << ") * uv.x), "
                << "ivec4(vec4(" << intExpect.x() << ", " << intExpect.y() << ", " << intExpect.z() << ", "
                << intExpect.w()
                << ") * uv.y), "
                // Allow room for precision errors.  Rendering happens at sample locations while verification uv is in the middle of pixel.
                << intValue << " / int(params.area.z) + 1, " << intValue << " / int(params.area.w) + 1)"
                << (sampleID == numSamples - 1 ? ")" : "") << "\n";
        }
        src << "    {\n"
            << "        atomicAdd(sb_out.colorVerification[2], 1);\n"
            << "        result3 = vec4(0, 1, 0, 1);\n"
            << "    }\n"
            << "    imageStore(verify, ivec3(coords, 2), result3);\n"
            << "\n";
        src << "}\n";

        programCollection.glslSources.add("comp") << glu::ComputeSource(src.str());
    }

    // Always generate constant-color checks as they are used by vkCmdClearAttachments tests
    initConstantColorVerifyPrograms(programCollection, params);
}

void dispatchVerifyBasic(Context &context, TestData &td)
{
    const DeviceInterface &vk = td.vkd;
    const VkDevice device     = td.device;

    postDrawBarrier(td);

    const VkPushConstantRange &verifyPushConstantRange = {
        VK_SHADER_STAGE_COMPUTE_BIT,                                  // VkShaderStageFlags    stageFlags;
        0,                                                            // uint32_t              offset;
        static_cast<uint32_t>(sizeof(tcu::UVec4) + sizeof(uint32_t)), // uint32_t              size;
    };

    Move<VkPipelineLayout> verifyPipelineLayout;
    setupVerifyDescriptorSetAndPipeline(context, td, &verifyPushConstantRange, verifyPipelineLayout);

    vk.cmdPushConstants(*td.cmdBuffer, *verifyPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(tcu::UVec4),
                        &td.renderArea);
    vk.cmdDispatch(*td.cmdBuffer, (td.renderArea.z() + 7) / 8, (td.renderArea.w() + 7) / 8, 1);

    postVerifyBarrier(td, td.verificationBuffer);

    invalidateAlloc(vk, device, *td.verificationBufferAlloc);
}

void drawBasic(Context &context, const TestParams &params, TestData &td)
{
    const InstanceInterface &vki          = td.instHelper.vki;
    const DeviceInterface &vk             = td.vkd;
    const VkPhysicalDevice physicalDevice = td.devHelper.physicalDevice;
    const VkDevice device                 = td.device;

    VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo;
    std::vector<VkFormat> colorAttachmentFormats = {VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED,
                                                    VK_FORMAT_UNDEFINED};
    std::vector<VkRenderingAttachmentInfo> colorAttachmentInfos(4u);

    clearImagesBeforeDraw(params, td);

    if (params.dynamicRendering)
    {
        preRenderingImageLayoutTransition(td);
    }

    // Create a render pass and a framebuffer
    {
        std::vector<VkSubpassDescription2> subpasses;
        std::vector<VkImage> images;
        std::vector<VkImageView> attachments;
        std::vector<VkAttachmentDescription2> attachmentDescriptions;
        std::vector<VkAttachmentReference2> attachmentReferences;
        VkMultisampledRenderToSingleSampledInfoEXT msrtss;
        int32_t attachmentNdxes[3] = {-1, -1, -1};
        uint32_t attachmentUseMask = 0;

        initializeAttachments(td, images, attachments, attachmentNdxes);

        if (params.dynamicRendering)
        {
            initializeRenderingAttachmentInfos(params, td, colorAttachmentInfos, colorAttachmentFormats,
                                               attachmentUseMask);

            pipelineRenderingCreateInfo = {
                VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,     // VkStructureType    sType
                nullptr,                                              // const void*        pNext
                0u,                                                   // uint32_t            viewMask
                static_cast<uint32_t>(colorAttachmentFormats.size()), // uint32_t            colorAttachmentCount
                colorAttachmentFormats.data(),                        // const VkFormat*    pColorAttachmentFormats
                VK_FORMAT_UNDEFINED,                                  // VkFormat            depthAttachmentFormat
                VK_FORMAT_UNDEFINED                                   // VkFormat            stencilAttachmentFormat
            };
        }
        else
        {
            initializeAttachmentDescriptions(params, attachmentDescriptions, attachmentUseMask);

            addSubpassDescription(params, attachmentReferences, nullptr, msrtss, subpasses, {}, attachmentNdxes);

            createRenderPassAndFramebuffer(td, images, attachments, attachmentDescriptions, subpasses, {});
        }
    }

    {
        const VkPushConstantRange &pushConstantRange = {
            VK_SHADER_STAGE_FRAGMENT_BIT,              // VkShaderStageFlags    stageFlags;
            0,                                         // uint32_t              offset;
            static_cast<uint32_t>(sizeof(tcu::UVec4)), // uint32_t              size;
        };

        const ShaderWrapper vertexModule(ShaderWrapper(vk, device, context.getBinaryCollection().get("vert"), 0u));
        const ShaderWrapper fragmentModule(ShaderWrapper(vk, device, context.getBinaryCollection().get("frag"), 0u));
        const PipelineLayoutWrapper pipelineLayout(PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, vk, device, 0, nullptr, 1,
                                                   &pushConstantRange);

        td.graphicsPipeline = makeGraphicsPipeline(
            vki, vk, physicalDevice, device, context.getDeviceExtensions(), pipelineLayout,
            params.dynamicRendering ? VK_NULL_HANDLE : *td.renderPassFramebuffer,
            params.dynamicRendering ? &pipelineRenderingCreateInfo : nullptr, vertexModule, fragmentModule, false, 0,
            params.intColorLocation, td.renderArea, td.renderArea, params.numSamples);

        if (params.dynamicRendering)
        {
            startRendering(params, td, static_cast<uint32_t>(colorAttachmentFormats.size()), colorAttachmentInfos);
        }
        else
        {
            startRenderPass(td, DE_LENGTH_OF_ARRAY(params.clearValues), params.clearValues);
        }

        const VkDeviceSize vertexBufferOffset = 0;
        vk.cmdBindVertexBuffers(*td.cmdBuffer, 0u, 1u, &td.vertexBuffer.get(), &vertexBufferOffset);

        vk.cmdPushConstants(*td.cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(tcu::UVec4),
                            &td.renderArea);
        (*td.graphicsPipeline).bind(*td.cmdBuffer);
        vk.cmdDraw(*td.cmdBuffer, 3, 1u, 0u, 0u);

        if (params.dynamicRendering)
        {
            vk.cmdEndRendering(*td.cmdBuffer);
        }
        else
        {
            td.renderPassFramebuffer.end(vk, *td.cmdBuffer);
        }
    }

    if (params.dynamicRendering)
    {
        postRenderingImageLayoutTransition(td);
    }

    // Verify results
    dispatchVerifyBasic(context, td);
}

//! Verify multisampled rendering is done with the exact number of samples.
tcu::TestStatus testBasic(Context &context, const TestParams params)
{
    TestData td(context, params);

    createTestData(params, td);
    for (uint32_t i = 0; i < params.frameCount; ++i)
    {
        prepareVerificationBuffers(td);

        testStart(td);

        drawBasic(context, params, td);

        testEnd(context, params, td);

        auto res = verify(context, params, td);
        if (res.isFail())
        {
            return res;
        }

        td.present();
    }

    return tcu::TestStatus::pass("Pass");
}

struct SupportedFlagsParams
{
    vk::wsi::Type wsiType;
    VkSwapchainCreateFlagBitsKHR flag;
};

void supportedFlagsCheckRequirements(Context &context, const SupportedFlagsParams)
{
    context.requireDeviceFunctionality("VK_EXT_multisampled_render_to_swapchain");
}

void supportedFlagsInitPrograms(SourceCollections &, const SupportedFlagsParams)
{
}

tcu::TestStatus supportedFlagsTest(Context &context, const SupportedFlagsParams params)
{
    tcu::TestLog &log = context.getTestContext().getLog();
    InstanceHelper instHelper(context, params.wsiType);
    NativeObjects native(context, instHelper.supportedExtensions, params.wsiType, 1u, tcu::just(tcu::UVec2(32u)));
    Move<VkSurfaceKHR> surface(createSurface(instHelper.vki, instHelper.instance, params.wsiType, native.getDisplay(),
                                             native.getWindow(), context.getTestContext().getCommandLine()));
    VkPhysicalDevice physicalDevice =
        chooseDevice(instHelper.vki, instHelper.instance, context.getTestContext().getCommandLine());

    vk::VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = vk::initVulkanStructure();
    surfaceInfo.surface                             = *surface;

    vk::VkSwapchainFlagsSurfaceCapabilitiesEXT swapchainFlagsSurfaceCapabilities = vk::initVulkanStructure();

    vk::VkSurfaceCapabilities2KHR surfaceCapabilities2 = vk::initVulkanStructure(&swapchainFlagsSurfaceCapabilities);
    VK_CHECK(
        instHelper.vki.getPhysicalDeviceSurfaceCapabilities2KHR(physicalDevice, &surfaceInfo, &surfaceCapabilities2));

    std::vector<VkExtensionProperties> extensions =
        enumerateDeviceExtensionProperties(instHelper.vki, physicalDevice, nullptr);

    VkPhysicalDeviceProperties properties;
    instHelper.vki.getPhysicalDeviceProperties(physicalDevice, &properties);

    uint32_t apiVersion               = properties.apiVersion;
    bool khr_swapchain                = false;
    bool khr_device_group             = false;
    bool khr_swapchain_mutable_format = false;
    bool khr_present_id2              = false;
    bool khr_present_wait2            = false;
    bool khr_swapchain_maintenance1   = false;
    bool ext_swapchain_maintenance1   = false;
    for (const VkExtensionProperties &ext : extensions)
    {
        if (std::strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
        {
            khr_swapchain = true;
        }
        else if (std::strcmp(ext.extensionName, VK_KHR_DEVICE_GROUP_EXTENSION_NAME) == 0)
        {
            khr_device_group = true;
        }
        else if (std::strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME) == 0)
        {
            khr_swapchain_mutable_format = true;
        }
        else if (std::strcmp(ext.extensionName, VK_KHR_PRESENT_ID_2_EXTENSION_NAME) == 0)
        {
            khr_present_id2 = true;
        }
        else if (std::strcmp(ext.extensionName, VK_KHR_PRESENT_WAIT_2_EXTENSION_NAME) == 0)
        {
            khr_present_wait2 = true;
        }
        else if (std::strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME) == 0)
        {
            khr_swapchain_maintenance1 = true;
        }
        else if (std::strcmp(ext.extensionName, VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME) == 0)
        {
            ext_swapchain_maintenance1 = true;
        }
    }

    const auto flags = swapchainFlagsSurfaceCapabilities.swapchainSupportedFlags;

    if ((apiVersion >= VK_API_VERSION_1_1 || khr_device_group) && khr_swapchain)
    {
        if ((flags & VK_SWAPCHAIN_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT_KHR) == 0)
        {
            log << tcu::TestLog::Message
                << "VkSwapchainFlagsSurfaceCapabilitiesEXT::swapchainSupportedFlags does not include "
                   "VK_SWAPCHAIN_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT_KHR when required (related extensions are "
                   "supported)"
                << tcu::TestLog::EndMessage;
            return tcu::TestStatus::fail("Fail");
        }
    }
    if (apiVersion >= VK_API_VERSION_1_1 && khr_swapchain)
    {
        if ((flags & VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR) == 0)
        {
            log << tcu::TestLog::Message
                << "VkSwapchainFlagsSurfaceCapabilitiesEXT::swapchainSupportedFlags does not include "
                   "VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR when required (related extensions are "
                   "supported)"
                << tcu::TestLog::EndMessage;
            return tcu::TestStatus::fail("Fail");
        }
    }
    if (khr_swapchain_mutable_format)
    {
        if ((flags & VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR) == 0)
        {
            log << tcu::TestLog::Message
                << "VkSwapchainFlagsSurfaceCapabilitiesEXT::swapchainSupportedFlags does not include "
                   "VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR when required (related extensions are "
                   "supported)"
                << tcu::TestLog::EndMessage;
            return tcu::TestStatus::fail("Fail");
        }
    }
    if (khr_swapchain_maintenance1 || ext_swapchain_maintenance1)
    {
        if ((flags & VK_SWAPCHAIN_CREATE_DEFERRED_MEMORY_ALLOCATION_BIT_KHR) == 0)
        {
            log << tcu::TestLog::Message
                << "VkSwapchainFlagsSurfaceCapabilitiesEXT::swapchainSupportedFlags does not include "
                   "VK_SWAPCHAIN_CREATE_DEFERRED_MEMORY_ALLOCATION_BIT_KHR when required (related extensions are "
                   "supported)"
                << tcu::TestLog::EndMessage;
            return tcu::TestStatus::fail("Fail");
        }
    }
    if (khr_present_id2)
    {
        if ((flags & VK_SWAPCHAIN_CREATE_PRESENT_ID_2_BIT_KHR) == 0)
        {
            log << tcu::TestLog::Message
                << "VkSwapchainFlagsSurfaceCapabilitiesEXT::swapchainSupportedFlags does not include "
                   "VK_SWAPCHAIN_CREATE_PRESENT_ID_2_BIT_KHR when required (related extensions are "
                   "supported)"
                << tcu::TestLog::EndMessage;
            return tcu::TestStatus::fail("Fail");
        }
    }
    if (khr_present_wait2)
    {
        if ((flags & VK_SWAPCHAIN_CREATE_PRESENT_WAIT_2_BIT_KHR) == 0)
        {
            log << tcu::TestLog::Message
                << "VkSwapchainFlagsSurfaceCapabilitiesEXT::swapchainSupportedFlags does not include "
                   "VK_SWAPCHAIN_CREATE_PRESENT_WAIT_2_BIT_KHR when required (related extensions are "
                   "supported)"
                << tcu::TestLog::EndMessage;
            return tcu::TestStatus::fail("Fail");
        }
    }

    return tcu::TestStatus::pass("Pass");
}

void generateBasicTest(de::Random &rng, TestParams &params, const VkSampleCountFlagBits sampleCount,
                       const bool renderToWholeFramebuffer, uint32_t attachmentLocation)
{
    params.frameCount = 2u;
    params.numSamples = sampleCount;

    // Set locations for the color attachments.
    params.swapchainLocation  = attachmentLocation;
    params.floatColorLocation = attachmentLocation == 0 ? 1 : 0;
    params.intColorLocation   = 2;

    // Always clear before render pass so outside render area can be verified.
    params.renderToWholeFramebuffer = renderToWholeFramebuffer;

    // Set random clear values.
    generateRandomClearValues(rng, params, params.clearValues, false);

    params.rngSeed = rng.getUint32();
}

std::string getFormatShortString(const VkFormat format)
{
    std::string s(de::toLower(getFormatName(format)));
    return s.substr(10);
}

std::string getFormatCaseName(const VkFormat color1Format, const VkFormat color2Format, const VkFormat color3Format)
{
    std::ostringstream str;
    str << getFormatShortString(color1Format) << "_" << getFormatShortString(color2Format) << "_"
        << getFormatShortString(color3Format);
    return str.str();
}

std::string getSampleCountCaseName(const VkSampleCountFlagBits sampleCount)
{
    std::ostringstream str;
    str << sampleCount << "x";
    return str.str();
}

std::string getFlagTestName(VkSwapchainCreateFlagBitsKHR flag)
{
    switch (flag)
    {
    case VK_SWAPCHAIN_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT_KHR:
        return "split_instance_bind_regions";
    case VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR:
        return "protected";
    case VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR:
        return "mutable_format";
    case VK_SWAPCHAIN_CREATE_DEFERRED_MEMORY_ALLOCATION_BIT_KHR:
        return "deferred_memory_allocation";
    case VK_SWAPCHAIN_CREATE_PRESENT_ID_2_BIT_KHR:
        return "present_id_2";
    case VK_SWAPCHAIN_CREATE_PRESENT_WAIT_2_BIT_KHR:
        return "present_wait_2";
    case VK_SWAPCHAIN_CREATE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_BIT_EXT:
        return "multisampled_render_to_single_sampled";
    default:
        break;
    }
    DE_ASSERT(0);
    return {};
}

void createMultisampledTestsInGroup(tcu::TestCaseGroup *rootGroup, vk::wsi::Type wsiType)
{
    // Color 1 is a float format
    const VkFormat swapchainFormatRange[] = {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
    };

    // Color 2 is a float format
    const VkFormat color2FormatRange[] = {
        VK_FORMAT_R16G16B16A16_SFLOAT,
    };

    // Color 3 is an integer format
    const VkFormat color3FormatRange[] = {
        VK_FORMAT_R32G32B32A32_UINT,
        VK_FORMAT_R16G16B16A16_SINT,
    };

    const VkSampleCountFlagBits sampleRange[] = {
        VK_SAMPLE_COUNT_2_BIT,
        VK_SAMPLE_COUNT_4_BIT,
        VK_SAMPLE_COUNT_8_BIT,
        VK_SAMPLE_COUNT_16_BIT,
    };

    const bool boolRange[]              = {false, true};
    const uint32_t attachmentLocation[] = {0, 1};

    de::Random rng(0xDEADBEEF);

    for (const VkFormat swapchainFormat : swapchainFormatRange)
        for (const VkFormat color2Format : color2FormatRange)
            for (const VkFormat color3Format : color3FormatRange)
            {
                de::MovePtr<tcu::TestCaseGroup> formatGroup(
                    new tcu::TestCaseGroup(rootGroup->getTestContext(),
                                           getFormatCaseName(swapchainFormat, color2Format, color3Format).c_str()));

                for (const VkSampleCountFlagBits sampleCount : sampleRange)
                {
                    de::MovePtr<tcu::TestCaseGroup> sampleGroup(new tcu::TestCaseGroup(
                        rootGroup->getTestContext(), getSampleCountCaseName(sampleCount).c_str()));

                    for (const bool dynamicRendering : boolRange)
                    {
                        de::MovePtr<tcu::TestCaseGroup> dynamicRenderingGroup(new tcu::TestCaseGroup(
                            rootGroup->getTestContext(), dynamicRendering ? "dynamic_rendering" : "render_pass"));

                        for (const bool renderToWholeFramebuffer : boolRange)
                        {
                            de::MovePtr<tcu::TestCaseGroup> wholeFramebufferGroup(new tcu::TestCaseGroup(
                                rootGroup->getTestContext(),
                                renderToWholeFramebuffer ? "whole_framebuffer" : "sub_framebuffer"));
                            for (const bool loc : attachmentLocation)
                            {
                                TestParams testParams;
                                deMemset(&testParams, 0, sizeof(testParams));

                                testParams.wsiType          = wsiType;
                                testParams.swapchainFormat  = swapchainFormat;
                                testParams.floatColorFormat = color2Format;
                                testParams.intColorFormat   = color3Format;
                                testParams.dynamicRendering = dynamicRendering;

                                generateBasicTest(rng, testParams, sampleCount, renderToWholeFramebuffer, loc);

                                addFunctionCaseWithPrograms(wholeFramebufferGroup.get(), loc ? "1" : "0",
                                                            checkRequirements, initBasicPrograms, testBasic,
                                                            testParams);
                            }
                            dynamicRenderingGroup->addChild(wholeFramebufferGroup.release());
                        }
                        sampleGroup->addChild(dynamicRenderingGroup.release());
                    }

                    formatGroup->addChild(sampleGroup.release());
                }
                rootGroup->addChild(formatGroup.release());
            }

    de::MovePtr<tcu::TestCaseGroup> swapchainSupportedFlagsGroup(
        new tcu::TestCaseGroup(rootGroup->getTestContext(), "supported_flags"));

    std::vector<VkSwapchainCreateFlagBitsKHR> flags = {
        VK_SWAPCHAIN_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT_KHR,
        VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR,
        VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR,
        VK_SWAPCHAIN_CREATE_DEFERRED_MEMORY_ALLOCATION_BIT_KHR,
        VK_SWAPCHAIN_CREATE_PRESENT_ID_2_BIT_KHR,
        VK_SWAPCHAIN_CREATE_PRESENT_WAIT_2_BIT_KHR,
        VK_SWAPCHAIN_CREATE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_BIT_EXT,
    };

    for (const auto flag : flags)
    {
        SupportedFlagsParams params;
        params.wsiType = wsiType;
        params.flag    = VK_SWAPCHAIN_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT_KHR;

        std::string testName = getFlagTestName(flag);
        addFunctionCaseWithPrograms(swapchainSupportedFlagsGroup.get(), testName.c_str(),
                                    supportedFlagsCheckRequirements, supportedFlagsInitPrograms, supportedFlagsTest,
                                    params);
    }

    rootGroup->addChild(swapchainSupportedFlagsGroup.release());
}

} // namespace

void createMultisampledRenderToSwapchainTests(tcu::TestCaseGroup *testGroup, vk::wsi::Type wsiType)
{
    createMultisampledTestsInGroup(testGroup, wsiType);
}

} // namespace wsi
} // namespace vkt
