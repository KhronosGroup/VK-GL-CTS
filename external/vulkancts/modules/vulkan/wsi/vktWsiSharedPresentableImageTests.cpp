/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 Google Inc.
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
 * \brief Tests for shared presentable image extension
 *//*--------------------------------------------------------------------*/

#include "vktWsiSharedPresentableImageTests.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkWsiPlatform.hpp"
#include "vkWsiUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkPlatform.hpp"
#include "vkTypeUtil.hpp"
#include "vkPrograms.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "vkWsiUtil.hpp"

#include "tcuPlatform.hpp"
#include "tcuResultCollector.hpp"
#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"

#include <vector>
#include <string>

using std::string;
using std::vector;

using tcu::Maybe;
using tcu::TestLog;
using tcu::UVec2;

namespace vkt
{
namespace wsi
{
namespace
{
enum Scaling
{
    SCALING_NONE,
    SCALING_UP,
    SCALING_DOWN
};

typedef vector<vk::VkExtensionProperties> Extensions;

void checkAllSupported(const Extensions &supportedExtensions, const vector<string> &requiredExtensions)
{
    for (vector<string>::const_iterator requiredExtName = requiredExtensions.begin();
         requiredExtName != requiredExtensions.end(); ++requiredExtName)
    {
        if (!isExtensionStructSupported(supportedExtensions, vk::RequiredExtension(*requiredExtName)))
            TCU_THROW(NotSupportedError, (*requiredExtName + " is not supported").c_str());
    }
}

CustomInstance createInstanceWithWsi(Context &context, const Extensions &supportedExtensions, vk::wsi::Type wsiType)
{
    const uint32_t version = context.getUsedApiVersion();
    vector<string> extensions;

    if (!vk::isCoreInstanceExtension(version, "VK_KHR_get_physical_device_properties2"))
        extensions.push_back("VK_KHR_get_physical_device_properties2");

    extensions.push_back("VK_KHR_surface");
    extensions.push_back("VK_KHR_get_surface_capabilities2");
    // Required for device extension to expose new physical device bits (in this
    // case, presentation mode enums)
    extensions.push_back(getExtensionName(wsiType));
    if (isDisplaySurface(wsiType))
        extensions.push_back("VK_KHR_display");

    checkAllSupported(supportedExtensions, extensions);

    return vkt::createCustomInstanceWithExtensions(context, extensions);
}

vk::VkPhysicalDeviceFeatures getDeviceNullFeatures(void)
{
    vk::VkPhysicalDeviceFeatures features;
    deMemset(&features, 0, sizeof(features));
    return features;
}

vk::Move<vk::VkDevice> createDeviceWithWsi(const vk::PlatformInterface &vkp, vk::VkInstance instance,
                                           const vk::InstanceInterface &vki, vk::VkPhysicalDevice physicalDevice,
                                           const Extensions &supportedExtensions, const uint32_t queueFamilyIndex,
                                           bool requiresSharedPresentableImage, bool validationEnabled,
                                           const vk::VkAllocationCallbacks *pAllocator = DE_NULL)
{
    const float queuePriorities[]                  = {1.0f};
    const vk::VkDeviceQueueCreateInfo queueInfos[] = {{vk::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, DE_NULL,
                                                       (vk::VkDeviceQueueCreateFlags)0, queueFamilyIndex,
                                                       DE_LENGTH_OF_ARRAY(queuePriorities), &queuePriorities[0]}};
    const vk::VkPhysicalDeviceFeatures features    = getDeviceNullFeatures();
    const char *const extensions[]                 = {"VK_KHR_swapchain", "VK_KHR_shared_presentable_image"};

    const vk::VkDeviceCreateInfo deviceParams = {vk::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                                 DE_NULL,
                                                 (vk::VkDeviceCreateFlags)0,
                                                 DE_LENGTH_OF_ARRAY(queueInfos),
                                                 &queueInfos[0],
                                                 0u,
                                                 DE_NULL,
                                                 requiresSharedPresentableImage ? 2u : 1u,
                                                 DE_ARRAY_BEGIN(extensions),
                                                 &features};

    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(extensions); ++ndx)
    {
        if (!isExtensionStructSupported(supportedExtensions, vk::RequiredExtension(extensions[ndx])))
            TCU_THROW(NotSupportedError, (string(extensions[ndx]) + " is not supported").c_str());
    }

    return createCustomDevice(validationEnabled, vkp, instance, vki, physicalDevice, &deviceParams, pAllocator);
}

de::MovePtr<vk::wsi::Display> createDisplay(const vk::Platform &platform, const Extensions &supportedExtensions,
                                            vk::wsi::Type wsiType)
{
    try
    {
        return de::MovePtr<vk::wsi::Display>(platform.createWsiDisplay(wsiType));
    }
    catch (const tcu::NotSupportedError &e)
    {
        if (isExtensionStructSupported(supportedExtensions, vk::RequiredExtension(getExtensionName(wsiType))) &&
            platform.hasDisplay(wsiType))
        {
            // If VK_KHR_{platform}_surface was supported, vk::Platform implementation
            // must support creating native display & window for that WSI type.
            throw tcu::TestError(e.getMessage());
        }
        else
            throw;
    }
}

de::MovePtr<vk::wsi::Window> createWindow(const vk::wsi::Display &display, const Maybe<UVec2> &initialSize)
{
    try
    {
        return de::MovePtr<vk::wsi::Window>(display.createWindow(initialSize));
    }
    catch (const tcu::NotSupportedError &e)
    {
        // See createDisplay - assuming that wsi::Display was supported platform port
        // should also support creating a window.
        throw tcu::TestError(e.getMessage());
    }
}

bool wsiTypeSupportsScaling(vk::wsi::Type wsiType)
{
    return vk::wsi::getPlatformProperties(wsiType).swapchainExtent ==
           vk::wsi::PlatformProperties::SWAPCHAIN_EXTENT_SCALED_TO_WINDOW_SIZE;
}

void initSemaphores(const vk::DeviceInterface &vkd, vk::VkDevice device, std::vector<vk::VkSemaphore> &semaphores)
{
    for (size_t ndx = 0; ndx < semaphores.size(); ndx++)
        semaphores[ndx] = createSemaphore(vkd, device).disown();
}

void deinitSemaphores(const vk::DeviceInterface &vkd, vk::VkDevice device, std::vector<vk::VkSemaphore> &semaphores)
{
    for (size_t ndx = 0; ndx < semaphores.size(); ndx++)
    {
        if (semaphores[ndx] != VK_NULL_HANDLE)
            vkd.destroySemaphore(device, semaphores[ndx], DE_NULL);

        semaphores[ndx] = VK_NULL_HANDLE;
    }

    semaphores.clear();
}

void initFences(const vk::DeviceInterface &vkd, vk::VkDevice device, std::vector<vk::VkFence> &fences)
{
    for (size_t ndx = 0; ndx < fences.size(); ndx++)
        fences[ndx] = createFence(vkd, device).disown();
}

void deinitFences(const vk::DeviceInterface &vkd, vk::VkDevice device, std::vector<vk::VkFence> &fences)
{
    for (size_t ndx = 0; ndx < fences.size(); ndx++)
    {
        if (fences[ndx] != VK_NULL_HANDLE)
            vkd.destroyFence(device, fences[ndx], DE_NULL);

        fences[ndx] = VK_NULL_HANDLE;
    }

    fences.clear();
}

void cmdRenderFrame(const vk::DeviceInterface &vkd, vk::VkCommandBuffer commandBuffer,
                    vk::VkPipelineLayout pipelineLayout, vk::VkPipeline pipeline, size_t frameNdx, uint32_t quadCount)
{
    const uint32_t frameNdxValue = (uint32_t)frameNdx;

    vkd.cmdPushConstants(commandBuffer, pipelineLayout, vk::VK_SHADER_STAGE_FRAGMENT_BIT, 0u, 4u, &frameNdxValue);
    vkd.cmdBindPipeline(commandBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkd.cmdDraw(commandBuffer, quadCount * 6u, 1u, 0u, 0u);
}

vk::Move<vk::VkCommandBuffer> createCommandBuffer(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                                  vk::VkCommandPool commandPool, vk::VkPipelineLayout pipelineLayout,
                                                  vk::VkRenderPass renderPass, vk::VkFramebuffer framebuffer,
                                                  vk::VkPipeline pipeline, size_t frameNdx, uint32_t quadCount,
                                                  uint32_t imageWidth, uint32_t imageHeight)
{
    const vk::VkCommandBufferAllocateInfo allocateInfo = {vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, DE_NULL,

                                                          commandPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};

    vk::Move<vk::VkCommandBuffer> commandBuffer(vk::allocateCommandBuffer(vkd, device, &allocateInfo));
    beginCommandBuffer(vkd, *commandBuffer, 0u);

    beginRenderPass(vkd, *commandBuffer, renderPass, framebuffer, vk::makeRect2D(0, 0, imageWidth, imageHeight),
                    tcu::Vec4(0.25f, 0.5f, 0.75f, 1.0f));

    cmdRenderFrame(vkd, *commandBuffer, pipelineLayout, pipeline, frameNdx, quadCount);

    endRenderPass(vkd, *commandBuffer);

    endCommandBuffer(vkd, *commandBuffer);
    return commandBuffer;
}

void deinitCommandBuffers(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkCommandPool commandPool,
                          std::vector<vk::VkCommandBuffer> &commandBuffers)
{
    for (size_t ndx = 0; ndx < commandBuffers.size(); ndx++)
    {
        if (commandBuffers[ndx] != VK_NULL_HANDLE)
            vkd.freeCommandBuffers(device, commandPool, 1u, &commandBuffers[ndx]);

        commandBuffers[ndx] = VK_NULL_HANDLE;
    }

    commandBuffers.clear();
}

vk::Move<vk::VkCommandPool> createCommandPool(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                              uint32_t queueFamilyIndex)
{
    const vk::VkCommandPoolCreateInfo createInfo = {vk::VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, DE_NULL, 0u,
                                                    queueFamilyIndex};

    return vk::createCommandPool(vkd, device, &createInfo);
}

vk::Move<vk::VkFramebuffer> createFramebuffer(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                              vk::VkRenderPass renderPass, vk::VkImageView imageView, uint32_t width,
                                              uint32_t height)
{
    const vk::VkFramebufferCreateInfo createInfo = {vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                                    DE_NULL,

                                                    0u,
                                                    renderPass,
                                                    1u,
                                                    &imageView,
                                                    width,
                                                    height,
                                                    1u};

    return vk::createFramebuffer(vkd, device, &createInfo);
}

vk::Move<vk::VkImageView> createImageView(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkImage image,
                                          vk::VkFormat format)
{
    const vk::VkImageViewCreateInfo createInfo = {vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                                  DE_NULL,

                                                  0u,
                                                  image,
                                                  vk::VK_IMAGE_VIEW_TYPE_2D,
                                                  format,
                                                  vk::makeComponentMappingRGBA(),
                                                  {vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}};

    return vk::createImageView(vkd, device, &createInfo, DE_NULL);
}

vk::Move<vk::VkRenderPass> createRenderPass(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkFormat format)
{
    const vk::VkAttachmentDescription attachments[] = {
        {0u, format, vk::VK_SAMPLE_COUNT_1_BIT,

         vk::VK_ATTACHMENT_LOAD_OP_LOAD, vk::VK_ATTACHMENT_STORE_OP_STORE,

         vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE, vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,

         // This differs from the usual layout handling in that the
         // swapchain image remains in IMAGE_LAYOUT_SHARED_PRESENT_KHR all
         // the time. We should not ever transition it away (or discard the
         // contents with a transition from UNDEFINED) as the PE is accessing
         // the image concurrently with our rendering.
         vk::VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR, vk::VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR}};
    const vk::VkAttachmentReference colorAttachmentRefs[] = {{0u, vk::VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR}};
    const vk::VkSubpassDescription subpasses[]            = {{0u, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, 0u, DE_NULL,

                                                              DE_LENGTH_OF_ARRAY(colorAttachmentRefs), colorAttachmentRefs,
                                                              DE_NULL,

                                                              DE_NULL, 0u, DE_NULL}};

    const vk::VkRenderPassCreateInfo createInfo = {vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                                   DE_NULL,
                                                   0u,

                                                   DE_LENGTH_OF_ARRAY(attachments),
                                                   attachments,

                                                   DE_LENGTH_OF_ARRAY(subpasses),
                                                   subpasses,

                                                   0u,
                                                   DE_NULL};

    return vk::createRenderPass(vkd, device, &createInfo);
}

vk::Move<vk::VkPipeline> createPipeline(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                        vk::VkRenderPass renderPass, vk::VkPipelineLayout layout,
                                        vk::VkShaderModule vertexShaderModule, vk::VkShaderModule fragmentShaderModule,
                                        uint32_t width, uint32_t height)
{
    const vk::VkPipelineVertexInputStateCreateInfo vertexInputState = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, DE_NULL, 0u, 0u, DE_NULL, 0u, DE_NULL};
    const std::vector<vk::VkViewport> viewports(1, vk::makeViewport(tcu::UVec2(width, height)));
    const std::vector<vk::VkRect2D> scissors(1, vk::makeRect2D(tcu::UVec2(width, height)));

    return vk::makeGraphicsPipeline(
        vkd,                  // const DeviceInterface&                        vk
        device,               // const VkDevice                                device
        layout,               // const VkPipelineLayout                        pipelineLayout
        vertexShaderModule,   // const VkShaderModule                          vertexShaderModule
        VK_NULL_HANDLE,       // const VkShaderModule                          tessellationControlShaderModule
        VK_NULL_HANDLE,       // const VkShaderModule                          tessellationEvalShaderModule
        VK_NULL_HANDLE,       // const VkShaderModule                          geometryShaderModule
        fragmentShaderModule, // const VkShaderModule                          fragmentShaderModule
        renderPass,           // const VkRenderPass                            renderPass
        viewports,            // const std::vector<VkViewport>&                viewports
        scissors,             // const std::vector<VkRect2D>&                  scissors
        vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology                     topology
        0u,                                      // const uint32_t                                subpass
        0u,                                      // const uint32_t                                patchControlPoints
        &vertexInputState); // const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
}

vk::Move<vk::VkPipelineLayout> createPipelineLayout(const vk::DeviceInterface &vkd, vk::VkDevice device)
{
    const vk::VkPushConstantRange pushConstants[]   = {{vk::VK_SHADER_STAGE_FRAGMENT_BIT, 0u, 4u}};
    const vk::VkPipelineLayoutCreateInfo createInfo = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        DE_NULL,
        0u,

        0u,
        DE_NULL,

        DE_LENGTH_OF_ARRAY(pushConstants),
        pushConstants,
    };

    return vk::createPipelineLayout(vkd, device, &createInfo);
}

struct TestConfig
{
    vk::wsi::Type wsiType;
    Scaling scaling;
    bool useSharedPresentableImage;
    vk::VkPresentModeKHR presentMode;
    vk::VkSurfaceTransformFlagsKHR transform;
    vk::VkCompositeAlphaFlagsKHR alpha;
};

class SharedPresentableImageTestInstance : public TestInstance
{
public:
    SharedPresentableImageTestInstance(Context &context, const TestConfig &testConfig);
    ~SharedPresentableImageTestInstance(void);

    tcu::TestStatus iterate(void);

private:
    const TestConfig m_testConfig;
    const uint32_t m_quadCount;
    const vk::PlatformInterface &m_vkp;
    const Extensions m_instanceExtensions;
    const CustomInstance m_instance;
    const vk::InstanceDriver &m_vki;
    const vk::VkPhysicalDevice m_physicalDevice;
    const de::UniquePtr<vk::wsi::Display> m_nativeDisplay;
    const de::UniquePtr<vk::wsi::Window> m_nativeWindow;
    const vk::Unique<vk::VkSurfaceKHR> m_surface;

    const uint32_t m_queueFamilyIndex;
    const Extensions m_deviceExtensions;
    const vk::Unique<vk::VkDevice> m_device;
    const vk::DeviceDriver m_vkd;
    const vk::VkQueue m_queue;

    const vk::Unique<vk::VkCommandPool> m_commandPool;
    const vk::Unique<vk::VkShaderModule> m_vertexShaderModule;
    const vk::Unique<vk::VkShaderModule> m_fragmentShaderModule;
    const vk::Unique<vk::VkPipelineLayout> m_pipelineLayout;

    vk::VkImageUsageFlags m_supportedUsageFlags;
    const vk::VkSurfaceCapabilitiesKHR m_surfaceProperties;
    const vector<vk::VkSurfaceFormatKHR> m_surfaceFormats;
    const vector<vk::VkPresentModeKHR> m_presentModes;

    tcu::ResultCollector m_resultCollector;

    vk::Move<vk::VkSwapchainKHR> m_swapchain;
    vk::VkImage m_swapchainImage; // NOTE: not owning. lifetime managed by swapchain
    vk::Move<vk::VkImageView> m_swapchainImageView;
    vk::Move<vk::VkFramebuffer> m_framebuffer;

    vk::Move<vk::VkRenderPass> m_renderPass;
    vk::Move<vk::VkPipeline> m_pipeline;

    std::vector<vk::VkCommandBuffer> m_commandBuffers;
    std::vector<vk::VkSemaphore> m_renderSemaphores;
    std::vector<vk::VkFence> m_fences;

    std::vector<vk::VkSwapchainCreateInfoKHR> m_swapchainConfigs;
    size_t m_swapchainConfigNdx;

    const size_t m_frameCount;
    size_t m_frameNdx;

    const size_t m_maxOutOfDateCount;
    size_t m_outOfDateCount;

    void initSwapchainResources(void);
    void deinitSwapchainResources(void);
    void render(void);
};

std::vector<vk::VkSwapchainCreateInfoKHR> generateSwapchainConfigs(
    const vk::InstanceDriver &vki, const vk::VkPhysicalDevice physicalDevice, vk::VkSurfaceKHR surface,
    uint32_t queueFamilyIndex, Scaling scaling, const vk::VkSurfaceCapabilitiesKHR &properties,
    const vector<vk::VkSurfaceFormatKHR> &formats, const vector<vk::VkPresentModeKHR> &presentModes,
    vk::VkPresentModeKHR presentMode, vk::VkImageUsageFlags supportedImageUsage,
    const vk::VkSurfaceTransformFlagsKHR transform, const vk::VkCompositeAlphaFlagsKHR alpha)
{
    const uint32_t imageLayers             = 1u;
    const vk::VkImageUsageFlags imageUsage = properties.supportedUsageFlags & supportedImageUsage;
    const vk::VkBool32 clipped             = VK_FALSE;
    vector<vk::VkSwapchainCreateInfoKHR> createInfos;

    const uint32_t currentWidth =
        properties.currentExtent.width != 0xFFFFFFFFu ?
            properties.currentExtent.width :
            de::min(1024u, properties.minImageExtent.width +
                               ((properties.maxImageExtent.width - properties.minImageExtent.width) / 2));
    const uint32_t currentHeight =
        properties.currentExtent.height != 0xFFFFFFFFu ?
            properties.currentExtent.height :
            de::min(1024u, properties.minImageExtent.height +
                               ((properties.maxImageExtent.height - properties.minImageExtent.height) / 2));

    const uint32_t imageWidth =
        scaling == SCALING_NONE ?
            currentWidth :
            (scaling == SCALING_UP ?
                 de::max(31u, properties.minImageExtent.width) :
                 de::min(deSmallestGreaterOrEquallPowerOfTwoU32(currentWidth + 1), properties.maxImageExtent.width));
    const uint32_t imageHeight =
        scaling == SCALING_NONE ?
            currentHeight :
            (scaling == SCALING_UP ?
                 de::max(31u, properties.minImageExtent.height) :
                 de::min(deSmallestGreaterOrEquallPowerOfTwoU32(currentHeight + 1), properties.maxImageExtent.height));
    const vk::VkExtent2D imageSize = {imageWidth, imageHeight};

    {
        size_t presentModeNdx;

        for (presentModeNdx = 0; presentModeNdx < presentModes.size(); presentModeNdx++)
        {
            if (presentModes[presentModeNdx] == presentMode)
                break;
        }

        if (presentModeNdx == presentModes.size())
            TCU_THROW(NotSupportedError, "Present mode not supported");

        if ((properties.supportedTransforms & transform) == 0)
            TCU_THROW(NotSupportedError, "Transform not supported");

        if ((properties.supportedCompositeAlpha & alpha) == 0)
            TCU_THROW(NotSupportedError, "Composite alpha not supported");
    }

    for (size_t formatNdx = 0; formatNdx < formats.size(); formatNdx++)
    {
        const vk::VkSurfaceTransformFlagBitsKHR preTransform = (vk::VkSurfaceTransformFlagBitsKHR)transform;
        const vk::VkCompositeAlphaFlagBitsKHR compositeAlpha = (vk::VkCompositeAlphaFlagBitsKHR)alpha;
        const vk::VkFormat imageFormat                       = formats[formatNdx].format;
        const vk::VkColorSpaceKHR imageColorSpace            = formats[formatNdx].colorSpace;
        const vk::VkSwapchainCreateInfoKHR createInfo        = {vk::VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                                                DE_NULL,
                                                                0u,
                                                                surface,
                                                                1, // Always 1 image for a shared presentable image swapchain.
                                                                imageFormat,
                                                                imageColorSpace,
                                                                imageSize,
                                                                imageLayers,
                                                                imageUsage,
                                                                vk::VK_SHARING_MODE_EXCLUSIVE,
                                                                1u,
                                                                &queueFamilyIndex,
                                                                preTransform,
                                                                compositeAlpha,
                                                                presentMode,
                                                                clipped,
                                                                VK_NULL_HANDLE};

        {
            vk::VkImageFormatProperties imageFormatProperties;

            deMemset(&imageFormatProperties, 0, sizeof(imageFormatProperties));

            vk::VkResult result = vki.getPhysicalDeviceImageFormatProperties(
                physicalDevice, imageFormat, vk::VK_IMAGE_TYPE_2D, vk::VK_IMAGE_TILING_OPTIMAL, imageUsage, 0,
                &imageFormatProperties);

            if (result == vk::VK_ERROR_FORMAT_NOT_SUPPORTED)
                continue;
        }

        createInfos.push_back(createInfo);
    }

    return createInfos;
}

vk::VkSurfaceCapabilitiesKHR getPhysicalDeviceSurfaceCapabilities(const vk::InstanceInterface &vki,
                                                                  vk::VkPhysicalDevice physicalDevice,
                                                                  vk::VkSurfaceKHR surface,
                                                                  vk::VkImageUsageFlags *usage)
{
    const vk::VkPhysicalDeviceSurfaceInfo2KHR info = {vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR, DE_NULL,

                                                      surface};
    vk::VkSharedPresentSurfaceCapabilitiesKHR sharedCapabilities;
    vk::VkSurfaceCapabilities2KHR capabilities;

    sharedCapabilities.sType = vk::VK_STRUCTURE_TYPE_SHARED_PRESENT_SURFACE_CAPABILITIES_KHR;
    sharedCapabilities.pNext = DE_NULL;

    capabilities.sType = vk::VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
    capabilities.pNext = &sharedCapabilities;

    VK_CHECK(vki.getPhysicalDeviceSurfaceCapabilities2KHR(physicalDevice, &info, &capabilities));

    TCU_CHECK(sharedCapabilities.sharedPresentSupportedUsageFlags & vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    *usage = sharedCapabilities.sharedPresentSupportedUsageFlags;

    return capabilities.surfaceCapabilities;
}

SharedPresentableImageTestInstance::SharedPresentableImageTestInstance(Context &context, const TestConfig &testConfig)
    : TestInstance(context)
    , m_testConfig(testConfig)
    , m_quadCount(16u)
    , m_vkp(context.getPlatformInterface())
    , m_instanceExtensions(vk::enumerateInstanceExtensionProperties(m_vkp, DE_NULL))
    , m_instance(createInstanceWithWsi(context, m_instanceExtensions, testConfig.wsiType))
    , m_vki(m_instance.getDriver())
    , m_physicalDevice(vk::chooseDevice(m_vki, m_instance, context.getTestContext().getCommandLine()))
    , m_nativeDisplay(createDisplay(context.getTestContext().getPlatform().getVulkanPlatform(), m_instanceExtensions,
                                    testConfig.wsiType))
    , m_nativeWindow(createWindow(*m_nativeDisplay, tcu::Nothing))
    , m_surface(vk::wsi::createSurface(m_vki, m_instance, testConfig.wsiType, *m_nativeDisplay, *m_nativeWindow,
                                       context.getTestContext().getCommandLine()))

    , m_queueFamilyIndex(vk::wsi::chooseQueueFamilyIndex(m_vki, m_physicalDevice, *m_surface))
    , m_deviceExtensions(vk::enumerateDeviceExtensionProperties(m_vki, m_physicalDevice, DE_NULL))
    , m_device(createDeviceWithWsi(m_vkp, m_instance, m_vki, m_physicalDevice, m_deviceExtensions, m_queueFamilyIndex,
                                   testConfig.useSharedPresentableImage,
                                   context.getTestContext().getCommandLine().isValidationEnabled()))
    , m_vkd(m_vkp, m_instance, *m_device, context.getUsedApiVersion(), context.getTestContext().getCommandLine())
    , m_queue(getDeviceQueue(m_vkd, *m_device, m_queueFamilyIndex, 0u))

    , m_commandPool(createCommandPool(m_vkd, *m_device, m_queueFamilyIndex))
    , m_vertexShaderModule(vk::createShaderModule(m_vkd, *m_device, context.getBinaryCollection().get("quad-vert"), 0u))
    , m_fragmentShaderModule(
          vk::createShaderModule(m_vkd, *m_device, context.getBinaryCollection().get("quad-frag"), 0u))
    , m_pipelineLayout(createPipelineLayout(m_vkd, *m_device))

    , m_supportedUsageFlags(0u)
    , m_surfaceProperties(
          getPhysicalDeviceSurfaceCapabilities(m_vki, m_physicalDevice, *m_surface, &m_supportedUsageFlags))
    , m_surfaceFormats(vk::wsi::getPhysicalDeviceSurfaceFormats(m_vki, m_physicalDevice, *m_surface))
    , m_presentModes(vk::wsi::getPhysicalDeviceSurfacePresentModes(m_vki, m_physicalDevice, *m_surface))

    , m_swapchainConfigs(generateSwapchainConfigs(m_vki, m_physicalDevice, *m_surface, m_queueFamilyIndex,
                                                  testConfig.scaling, m_surfaceProperties, m_surfaceFormats,
                                                  m_presentModes, testConfig.presentMode, m_supportedUsageFlags,
                                                  testConfig.transform, testConfig.alpha))
    , m_swapchainConfigNdx(0u)

    , m_frameCount(60u * 5u)
    , m_frameNdx(0u)

    , m_maxOutOfDateCount(20u)
    , m_outOfDateCount(0u)
{
    {
        const tcu::ScopedLogSection surfaceInfo(m_context.getTestContext().getLog(), "SurfaceCapabilities",
                                                "SurfaceCapabilities");
        m_context.getTestContext().getLog() << TestLog::Message << m_surfaceProperties << TestLog::EndMessage;
        m_context.getTestContext().getLog()
            << TestLog::Message << "SharedPresentSupportedUsageFlags: " << m_supportedUsageFlags << TestLog::EndMessage;
    }
}

SharedPresentableImageTestInstance::~SharedPresentableImageTestInstance(void)
{
    deinitSwapchainResources();
}

void SharedPresentableImageTestInstance::initSwapchainResources(void)
{
    const size_t fenceCount        = 6;
    const uint32_t imageWidth      = m_swapchainConfigs[m_swapchainConfigNdx].imageExtent.width;
    const uint32_t imageHeight     = m_swapchainConfigs[m_swapchainConfigNdx].imageExtent.height;
    const vk::VkFormat imageFormat = m_swapchainConfigs[m_swapchainConfigNdx].imageFormat;

    m_swapchain      = vk::createSwapchainKHR(m_vkd, *m_device, &m_swapchainConfigs[m_swapchainConfigNdx]);
    m_swapchainImage = vk::wsi::getSwapchainImages(m_vkd, *m_device, *m_swapchain).front();

    m_renderPass = createRenderPass(m_vkd, *m_device, imageFormat);
    m_pipeline   = createPipeline(m_vkd, *m_device, *m_renderPass, *m_pipelineLayout, *m_vertexShaderModule,
                                  *m_fragmentShaderModule, imageWidth, imageHeight);

    m_swapchainImageView = createImageView(m_vkd, *m_device, m_swapchainImage, imageFormat);
    m_framebuffer = createFramebuffer(m_vkd, *m_device, *m_renderPass, *m_swapchainImageView, imageWidth, imageHeight);

    m_renderSemaphores = std::vector<vk::VkSemaphore>(fenceCount, VK_NULL_HANDLE);
    m_fences           = std::vector<vk::VkFence>(fenceCount, VK_NULL_HANDLE);
    m_commandBuffers   = std::vector<vk::VkCommandBuffer>(m_fences.size(), VK_NULL_HANDLE);

    initSemaphores(m_vkd, *m_device, m_renderSemaphores);

    initFences(m_vkd, *m_device, m_fences);

    // Unlike a traditional swapchain, where we'd acquire a new image from the
    // PE every frame, a shared image swapchain has a single image that is
    // acquired upfront. We acquire it here, transition it to the proper layout,
    // and present it.

    // Acquire the one image
    const uint64_t foreverNs = 0xFFFFFFFFFFFFFFFFul;
    vk::Move<vk::VkSemaphore> semaphore(createSemaphore(m_vkd, *m_device));
    uint32_t imageIndex = 42; // initialize to junk value

    VK_CHECK(m_vkd.acquireNextImageKHR(*m_device, *m_swapchain, foreverNs, *semaphore, VK_NULL_HANDLE, &imageIndex));
    TCU_CHECK(imageIndex == 0);

    // Transition to IMAGE_LAYOUT_SHARED_PRESENT_KHR
    const vk::VkCommandBufferAllocateInfo allocateInfo = {vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, DE_NULL,
                                                          *m_commandPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};

    const vk::Unique<vk::VkCommandBuffer> commandBuffer(vk::allocateCommandBuffer(m_vkd, *m_device, &allocateInfo));
    beginCommandBuffer(m_vkd, *commandBuffer, 0u);

    const vk::VkImageMemoryBarrier barrier = {
        vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        DE_NULL,
        0,
        0,
        vk::VK_IMAGE_LAYOUT_UNDEFINED,
        vk::VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        m_swapchainImage,
        {vk::VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };

    m_vkd.cmdPipelineBarrier(*commandBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             vk::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0u, 0, DE_NULL, 0, DE_NULL, 1, &barrier);

    endCommandBuffer(m_vkd, *commandBuffer);

    const vk::VkPipelineStageFlags waitDstStages[] = {vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    const vk::VkSubmitInfo submitInfo              = {
        vk::VK_STRUCTURE_TYPE_SUBMIT_INFO, DE_NULL, 1, &*semaphore, waitDstStages, 1, &*commandBuffer, 0, DE_NULL,
    };

    VK_CHECK(m_vkd.queueSubmit(m_queue, 1u, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(m_vkd.queueWaitIdle(m_queue));
}

void SharedPresentableImageTestInstance::deinitSwapchainResources(void)
{
    VK_CHECK(m_vkd.queueWaitIdle(m_queue));

    deinitSemaphores(m_vkd, *m_device, m_renderSemaphores);
    deinitFences(m_vkd, *m_device, m_fences);
    deinitCommandBuffers(m_vkd, *m_device, *m_commandPool, m_commandBuffers);

    m_framebuffer        = vk::Move<vk::VkFramebuffer>();
    m_swapchainImageView = vk::Move<vk::VkImageView>();
    m_swapchainImage     = VK_NULL_HANDLE;

    m_swapchain  = vk::Move<vk::VkSwapchainKHR>();
    m_renderPass = vk::Move<vk::VkRenderPass>();
    m_pipeline   = vk::Move<vk::VkPipeline>();
}

void SharedPresentableImageTestInstance::render(void)
{
    const uint64_t foreverNs = 0xFFFFFFFFFFFFFFFFul;
    const vk::VkFence fence  = m_fences[m_frameNdx % m_fences.size()];
    const uint32_t width     = m_swapchainConfigs[m_swapchainConfigNdx].imageExtent.width;
    const uint32_t height    = m_swapchainConfigs[m_swapchainConfigNdx].imageExtent.height;

    // Throttle execution
    if (m_frameNdx >= m_fences.size())
    {
        VK_CHECK(m_vkd.waitForFences(*m_device, 1u, &fence, VK_TRUE, foreverNs));
        VK_CHECK(m_vkd.resetFences(*m_device, 1u, &fence));

        m_vkd.freeCommandBuffers(*m_device, *m_commandPool, 1u,
                                 &m_commandBuffers[m_frameNdx % m_commandBuffers.size()]);
        m_commandBuffers[m_frameNdx % m_commandBuffers.size()] = VK_NULL_HANDLE;
    }

    uint32_t imageIndex                          = 0; // There is only one image.
    const vk::VkSemaphore currentRenderSemaphore = m_renderSemaphores[m_frameNdx % m_renderSemaphores.size()];

    const bool willPresent =
        m_swapchainConfigs[m_swapchainConfigNdx].presentMode == vk::VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR ||
        !m_frameNdx;

    // Create command buffer
    m_commandBuffers[m_frameNdx % m_commandBuffers.size()] =
        createCommandBuffer(m_vkd, *m_device, *m_commandPool, *m_pipelineLayout, *m_renderPass, *m_framebuffer,
                            *m_pipeline, m_frameNdx, m_quadCount, width, height)
            .disown();

    // Submit command buffer
    {
        const vk::VkSubmitInfo submitInfo = {
            vk::VK_STRUCTURE_TYPE_SUBMIT_INFO,
            DE_NULL,
            0u,
            DE_NULL,
            DE_NULL,
            1u,
            &m_commandBuffers[m_frameNdx % m_commandBuffers.size()],
            willPresent ? 1u : 0u, // Only signal the semaphore if we're going to call QueuePresent.
            &currentRenderSemaphore};

        // With a traditional swapchain, we'd fence on completion of
        // AcquireNextImage. We never call that for a shared image swapchain, so
        // fence on completion of the rendering work instead. A real shared
        // image application would want a more substantial pacing mechanism.
        VK_CHECK(m_vkd.queueSubmit(m_queue, 1u, &submitInfo, fence));
    }

    // DEMAND_REFRESH requires us to call QueuePresent whenever we want to be
    // assured the PE has picked up a new frame. The PE /may/ also pick up
    // changes whenever it likes.
    //
    // For CONTINUOUS_REFRESH, we need to just call QueuePresent once on the
    // first frame to kick things off.
    if (willPresent)
    {

        // Present frame
        vk::VkResult result;
        const vk::VkPresentInfoKHR presentInfo = {vk::VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                                  DE_NULL,
                                                  1u,
                                                  &currentRenderSemaphore,
                                                  1u,
                                                  &*m_swapchain,
                                                  &imageIndex,
                                                  &result};

        VK_CHECK_WSI(m_vkd.queuePresentKHR(m_queue, &presentInfo));
        VK_CHECK_WSI(result);
    }

    // With either present mode, we can call GetSwapchainStatus at any time
    // to detect possible OUT_OF_DATE conditions. Let's do that every frame.

    const vk::VkResult swapchainStatus = m_vkd.getSwapchainStatusKHR(*m_device, *m_swapchain);
    VK_CHECK(swapchainStatus);
}

tcu::TestStatus SharedPresentableImageTestInstance::iterate(void)
{
    // Initialize swapchain specific resources
    // Render test
    try
    {
        if (m_frameNdx == 0)
        {
            if (m_outOfDateCount == 0)
                m_context.getTestContext().getLog()
                    << tcu::TestLog::Message << "Swapchain: " << m_swapchainConfigs[m_swapchainConfigNdx]
                    << tcu::TestLog::EndMessage;

            initSwapchainResources();
        }

        render();
    }
    catch (const vk::Error &error)
    {
        if (error.getError() == vk::VK_ERROR_OUT_OF_DATE_KHR)
        {
            m_swapchainConfigs = generateSwapchainConfigs(
                m_vki, m_physicalDevice, *m_surface, m_queueFamilyIndex, m_testConfig.scaling, m_surfaceProperties,
                m_surfaceFormats, m_presentModes, m_testConfig.presentMode, m_supportedUsageFlags,
                m_testConfig.transform, m_testConfig.alpha);

            if (m_outOfDateCount < m_maxOutOfDateCount)
            {
                m_context.getTestContext().getLog()
                    << TestLog::Message << "Frame " << m_frameNdx << ": Swapchain out of date. Recreating resources."
                    << TestLog::EndMessage;
                deinitSwapchainResources();
                m_frameNdx = 0;
                m_outOfDateCount++;

                return tcu::TestStatus::incomplete();
            }
            else
            {
                m_context.getTestContext().getLog()
                    << TestLog::Message << "Frame " << m_frameNdx << ": Swapchain out of date." << TestLog::EndMessage;
                m_resultCollector.fail("Received too many VK_ERROR_OUT_OF_DATE_KHR errors. Received " +
                                       de::toString(m_outOfDateCount) + ", max " + de::toString(m_maxOutOfDateCount));
            }
        }
        else
        {
            m_resultCollector.fail(error.what());
        }

        deinitSwapchainResources();

        m_swapchainConfigNdx++;
        m_frameNdx       = 0;
        m_outOfDateCount = 0;

        if (m_swapchainConfigNdx >= m_swapchainConfigs.size())
            return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
        else
            return tcu::TestStatus::incomplete();
    }

    m_frameNdx++;

    if (m_frameNdx >= m_frameCount)
    {
        m_frameNdx       = 0;
        m_outOfDateCount = 0;
        m_swapchainConfigNdx++;

        deinitSwapchainResources();

        if (m_swapchainConfigNdx >= m_swapchainConfigs.size())
            return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
        else
            return tcu::TestStatus::incomplete();
    }
    else
        return tcu::TestStatus::incomplete();
}

struct Programs
{
    static void init(vk::SourceCollections &dst, TestConfig)
    {
        dst.glslSources.add("quad-vert") << glu::VertexSource(
            "#version 450\n"
            "out gl_PerVertex {\n"
            "\tvec4 gl_Position;\n"
            "};\n"
            "layout(location = 0) out highp uint quadIndex;\n"
            "highp float;\n"
            "void main (void) {\n"
            "\tgl_Position = vec4(((gl_VertexIndex + 2) / 3) % 2 == 0 ? -1.0 : 1.0,\n"
            "\t                   ((gl_VertexIndex + 1) / 3) % 2 == 0 ? -1.0 : 1.0, 0.0, 1.0);\n"
            "\tquadIndex = gl_VertexIndex / 6;\n"
            "}\n");
        dst.glslSources.add("quad-frag") << glu::FragmentSource(
            "#version 310 es\n"
            "layout(location = 0) flat in highp uint quadIndex;\n"
            "layout(location = 0) out highp vec4 o_color;\n"
            "layout(push_constant) uniform PushConstant {\n"
            "\thighp uint frameNdx;\n"
            "} pushConstants;\n"
            "void main (void)\n"
            "{\n"
            "\thighp uint frameNdx = pushConstants.frameNdx;\n"
            "\thighp uint cellX = bitfieldExtract(uint(gl_FragCoord.x), 7, 10);\n"
            "\thighp uint cellY = bitfieldExtract(uint(gl_FragCoord.y), 7, 10);\n"
            "\thighp uint x = quadIndex ^ (frameNdx + (uint(gl_FragCoord.x) >> cellX));\n"
            "\thighp uint y = quadIndex ^ (frameNdx + (uint(gl_FragCoord.y) >> cellY));\n"
            "\thighp uint r = 128u * bitfieldExtract(x, 0, 1)\n"
            "\t             +  64u * bitfieldExtract(y, 1, 1)\n"
            "\t             +  32u * bitfieldExtract(x, 3, 1);\n"
            "\thighp uint g = 128u * bitfieldExtract(y, 0, 1)\n"
            "\t             +  64u * bitfieldExtract(x, 2, 1)\n"
            "\t             +  32u * bitfieldExtract(y, 3, 1);\n"
            "\thighp uint b = 128u * bitfieldExtract(x, 1, 1)\n"
            "\t             +  64u * bitfieldExtract(y, 2, 1)\n"
            "\t             +  32u * bitfieldExtract(x, 4, 1);\n"
            "\to_color = vec4(float(r) / 255.0, float(g) / 255.0, float(b) / 255.0, 1.0);\n"
            "}\n");
    }
};

} // namespace

void createSharedPresentableImageTests(tcu::TestCaseGroup *testGroup, vk::wsi::Type wsiType)
{
    const struct
    {
        Scaling scaling;
        const char *name;
    } scaling[] = {{SCALING_NONE, "scale_none"}, {SCALING_UP, "scale_up"}, {SCALING_DOWN, "scale_down"}};
    const struct
    {
        vk::VkPresentModeKHR mode;
        const char *name;
    } presentModes[] = {
        {vk::VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR, "demand"},
        {vk::VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR, "continuous"},
    };
    const struct
    {
        vk::VkSurfaceTransformFlagsKHR transform;
        const char *name;
    } transforms[] = {{vk::VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR, "identity"},
                      {vk::VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR, "rotate_90"},
                      {vk::VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR, "rotate_180"},
                      {vk::VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR, "rotate_270"},
                      {vk::VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR, "horizontal_mirror"},
                      {vk::VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR, "horizontal_mirror_rotate_90"},
                      {vk::VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR, "horizontal_mirror_rotate_180"},
                      {vk::VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR, "horizontal_mirror_rotate_270"},
                      {vk::VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR, "inherit"}};
    const struct
    {
        vk::VkCompositeAlphaFlagsKHR alpha;
        const char *name;
    } alphas[] = {{vk::VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, "opaque"},
                  {vk::VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, "pre_multiplied"},
                  {vk::VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, "post_multiplied"},
                  {vk::VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR, "inherit"}};

    for (size_t scalingNdx = 0; scalingNdx < DE_LENGTH_OF_ARRAY(scaling); scalingNdx++)
    {
        if (scaling[scalingNdx].scaling == SCALING_NONE || wsiTypeSupportsScaling(wsiType))
        {
            de::MovePtr<tcu::TestCaseGroup> scaleGroup(
                new tcu::TestCaseGroup(testGroup->getTestContext(), scaling[scalingNdx].name));

            for (size_t transformNdx = 0; transformNdx < DE_LENGTH_OF_ARRAY(transforms); transformNdx++)
            {
                de::MovePtr<tcu::TestCaseGroup> transformGroup(
                    new tcu::TestCaseGroup(testGroup->getTestContext(), transforms[transformNdx].name));

                for (size_t alphaNdx = 0; alphaNdx < DE_LENGTH_OF_ARRAY(alphas); alphaNdx++)
                {
                    de::MovePtr<tcu::TestCaseGroup> alphaGroup(
                        new tcu::TestCaseGroup(testGroup->getTestContext(), alphas[alphaNdx].name));

                    for (size_t presentModeNdx = 0; presentModeNdx < DE_LENGTH_OF_ARRAY(presentModes); presentModeNdx++)
                    {
                        const char *const name = presentModes[presentModeNdx].name;
                        TestConfig config;

                        config.wsiType                   = wsiType;
                        config.useSharedPresentableImage = true;
                        config.scaling                   = scaling[scalingNdx].scaling;
                        config.transform                 = transforms[transformNdx].transform;
                        config.alpha                     = alphas[alphaNdx].alpha;
                        config.presentMode               = presentModes[presentModeNdx].mode;

                        alphaGroup->addChild(
                            new vkt::InstanceFactory1<SharedPresentableImageTestInstance, TestConfig, Programs>(
                                testGroup->getTestContext(), name, Programs(), config));
                    }

                    transformGroup->addChild(alphaGroup.release());
                }

                scaleGroup->addChild(transformGroup.release());
            }

            testGroup->addChild(scaleGroup.release());
        }
    }
}

} // namespace wsi
} // namespace vkt
