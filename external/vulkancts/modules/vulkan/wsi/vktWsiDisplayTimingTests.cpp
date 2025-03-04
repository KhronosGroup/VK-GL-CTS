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
 * \brief Tests for VK_GOOGLE_display_timing
 *//*--------------------------------------------------------------------*/

#include "vkRefUtil.hpp"
#include "vkWsiPlatform.hpp"
#include "vkWsiUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkPlatform.hpp"
#include "vkTypeUtil.hpp"
#include "vkPrograms.hpp"
#include "vkCmdUtil.hpp"
#include "vkWsiUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "vktWsiDisplayTimingTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "tcuPlatform.hpp"
#include "tcuResultCollector.hpp"
#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"

#include "deClock.h"

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
static const uint64_t MILLISECOND = 1000ull * 1000ull;
static const uint64_t SECOND      = 1000ull * MILLISECOND;

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
    vector<string> extensions;

    extensions.push_back("VK_KHR_surface");
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

vk::Move<vk::VkDevice> createDeviceWithWsi(const vk::PlatformInterface &vkp, const vk::VkInstance instance,
                                           const vk::InstanceInterface &vki, vk::VkPhysicalDevice physicalDevice,
                                           const Extensions &supportedExtensions, const uint32_t queueFamilyIndex,
                                           bool requiresDisplayTiming, bool validationEnabled,
                                           const vk::VkAllocationCallbacks *pAllocator = nullptr)
{
    const float queuePriorities[]                  = {1.0f};
    const vk::VkDeviceQueueCreateInfo queueInfos[] = {{vk::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr,
                                                       (vk::VkDeviceQueueCreateFlags)0, queueFamilyIndex,
                                                       DE_LENGTH_OF_ARRAY(queuePriorities), &queuePriorities[0]}};
    const vk::VkPhysicalDeviceFeatures features    = getDeviceNullFeatures();
    const char *const extensions[]                 = {"VK_KHR_swapchain", "VK_GOOGLE_display_timing"};

    const vk::VkDeviceCreateInfo deviceParams = {vk::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                                 nullptr,
                                                 (vk::VkDeviceCreateFlags)0,
                                                 DE_LENGTH_OF_ARRAY(queueInfos),
                                                 &queueInfos[0],
                                                 0u,
                                                 nullptr,
                                                 requiresDisplayTiming ? 2u : 1u,
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
            vkd.destroySemaphore(device, semaphores[ndx], nullptr);

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
            vkd.destroyFence(device, fences[ndx], nullptr);

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
                                                  vk::VkPipeline pipeline, vk::VkImage image, bool isFirst,
                                                  size_t frameNdx, uint32_t quadCount, uint32_t imageWidth,
                                                  uint32_t imageHeight)
{
    const vk::VkCommandBufferAllocateInfo allocateInfo = {vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr,

                                                          commandPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};

    vk::Move<vk::VkCommandBuffer> commandBuffer(vk::allocateCommandBuffer(vkd, device, &allocateInfo));
    beginCommandBuffer(vkd, *commandBuffer, 0u);

    {
        const vk::VkImageSubresourceRange subRange = {vk::VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        const vk::VkImageMemoryBarrier barrier     = {
            vk::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            nullptr,
            vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            vk::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            isFirst ? vk::VK_IMAGE_LAYOUT_UNDEFINED : vk::VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            image,
            subRange};
        vkd.cmdPipelineBarrier(*commandBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                               vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1,
                               &barrier);
    }

    beginRenderPass(vkd, *commandBuffer, renderPass, framebuffer, vk::makeRect2D(imageWidth, imageHeight),
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
    const vk::VkCommandPoolCreateInfo createInfo = {vk::VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, 0u,
                                                    queueFamilyIndex};

    return vk::createCommandPool(vkd, device, &createInfo);
}

vk::Move<vk::VkFramebuffer> createFramebuffer(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                              vk::VkRenderPass renderPass, vk::VkImageView imageView, uint32_t width,
                                              uint32_t height)
{
    const vk::VkFramebufferCreateInfo createInfo = {vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                                    nullptr,

                                                    0u,
                                                    renderPass,
                                                    1u,
                                                    &imageView,
                                                    width,
                                                    height,
                                                    1u};

    return vk::createFramebuffer(vkd, device, &createInfo);
}

void initFramebuffers(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkRenderPass renderPass,
                      std::vector<vk::VkImageView> imageViews, uint32_t width, uint32_t height,
                      std::vector<vk::VkFramebuffer> &framebuffers)
{
    DE_ASSERT(framebuffers.size() == imageViews.size());

    for (size_t ndx = 0; ndx < framebuffers.size(); ndx++)
        framebuffers[ndx] = createFramebuffer(vkd, device, renderPass, imageViews[ndx], width, height).disown();
}

void deinitFramebuffers(const vk::DeviceInterface &vkd, vk::VkDevice device,
                        std::vector<vk::VkFramebuffer> &framebuffers)
{
    for (size_t ndx = 0; ndx < framebuffers.size(); ndx++)
    {
        if (framebuffers[ndx] != VK_NULL_HANDLE)
            vkd.destroyFramebuffer(device, framebuffers[ndx], nullptr);

        framebuffers[ndx] = VK_NULL_HANDLE;
    }

    framebuffers.clear();
}

vk::Move<vk::VkImageView> createImageView(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkImage image,
                                          vk::VkFormat format)
{
    const vk::VkImageViewCreateInfo createInfo = {vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                                  nullptr,

                                                  0u,
                                                  image,
                                                  vk::VK_IMAGE_VIEW_TYPE_2D,
                                                  format,
                                                  vk::makeComponentMappingRGBA(),
                                                  {vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}};

    return vk::createImageView(vkd, device, &createInfo, nullptr);
}

void initImageViews(const vk::DeviceInterface &vkd, vk::VkDevice device, const std::vector<vk::VkImage> &images,
                    vk::VkFormat format, std::vector<vk::VkImageView> &imageViews)
{
    DE_ASSERT(images.size() == imageViews.size());

    for (size_t ndx = 0; ndx < imageViews.size(); ndx++)
        imageViews[ndx] = createImageView(vkd, device, images[ndx], format).disown();
}

void deinitImageViews(const vk::DeviceInterface &vkd, vk::VkDevice device, std::vector<vk::VkImageView> &imageViews)
{
    for (size_t ndx = 0; ndx < imageViews.size(); ndx++)
    {
        if (imageViews[ndx] != VK_NULL_HANDLE)
            vkd.destroyImageView(device, imageViews[ndx], nullptr);

        imageViews[ndx] = VK_NULL_HANDLE;
    }

    imageViews.clear();
}

vk::Move<vk::VkRenderPass> createRenderPass(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkFormat format)
{
    return vk::makeRenderPass(vkd, device, format, vk::VK_FORMAT_UNDEFINED, vk::VK_ATTACHMENT_LOAD_OP_LOAD,
                              vk::VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

vk::Move<vk::VkPipeline> createPipeline(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                        vk::VkRenderPass renderPass, vk::VkPipelineLayout layout,
                                        vk::VkShaderModule vertexShaderModule, vk::VkShaderModule fragmentShaderModule,
                                        uint32_t width, uint32_t height)
{
    const vk::VkPipelineVertexInputStateCreateInfo vertexInputState = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, 0u, 0u, nullptr, 0u, nullptr};
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
    const vk::VkPipelineLayoutCreateInfo createInfo = {vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                       nullptr,
                                                       0u,

                                                       0u,
                                                       nullptr,

                                                       DE_LENGTH_OF_ARRAY(pushConstants),
                                                       pushConstants};

    return vk::createPipelineLayout(vkd, device, &createInfo);
}

struct TestConfig
{
    vk::wsi::Type wsiType;
    bool useDisplayTiming;
    vk::VkPresentModeKHR presentMode;
};

class DisplayTimingTestInstance : public TestInstance
{
public:
    DisplayTimingTestInstance(Context &context, const TestConfig &testConfig);
    ~DisplayTimingTestInstance(void);

    tcu::TestStatus iterate(void);

private:
    const bool m_useDisplayTiming;
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

    const vk::VkSurfaceCapabilitiesKHR m_surfaceProperties;
    const vector<vk::VkSurfaceFormatKHR> m_surfaceFormats;
    const vector<vk::VkPresentModeKHR> m_presentModes;

    tcu::ResultCollector m_resultCollector;

    vk::Move<vk::VkSwapchainKHR> m_swapchain;
    std::vector<vk::VkImage> m_swapchainImages;
    std::vector<bool> m_isFirst;

    vk::Move<vk::VkRenderPass> m_renderPass;
    vk::Move<vk::VkPipeline> m_pipeline;

    std::vector<vk::VkImageView> m_swapchainImageViews;
    std::vector<vk::VkFramebuffer> m_framebuffers;
    std::vector<vk::VkCommandBuffer> m_commandBuffers;
    std::vector<vk::VkSemaphore> m_acquireSemaphores;
    std::vector<vk::VkSemaphore> m_renderSemaphores;
    std::vector<vk::VkFence> m_fences;

    vk::VkSemaphore m_freeAcquireSemaphore;
    vk::VkSemaphore m_freeRenderSemaphore;

    vk::VkSwapchainCreateInfoKHR m_swapchainConfig;

    const size_t m_frameCount;
    size_t m_frameNdx;

    const size_t m_maxOutOfDateCount;
    size_t m_outOfDateCount;

    std::map<uint32_t, uint64_t> m_queuePresentTimes;

    vk::VkRefreshCycleDurationGOOGLE m_rcDuration;
    uint64_t m_refreshDurationMultiplier;
    uint64_t m_targetIPD;
    uint64_t m_prevDesiredPresentTime;
    uint32_t m_nextPresentID;
    uint32_t m_ignoreThruPresentID;
    bool m_ExpectImage80Late;

    void initSwapchainResources(void);
    void deinitSwapchainResources(void);
    void render(void);
};

vk::VkSwapchainCreateInfoKHR createSwapchainConfig(vk::VkSurfaceKHR surface, uint32_t queueFamilyIndex,
                                                   const vk::VkSurfaceCapabilitiesKHR &properties,
                                                   const vector<vk::VkSurfaceFormatKHR> &formats,
                                                   const vector<vk::VkPresentModeKHR> &presentModes,
                                                   vk::VkPresentModeKHR presentMode)
{
    const uint32_t imageLayers             = 1u;
    const vk::VkImageUsageFlags imageUsage = properties.supportedUsageFlags;
    const vk::VkBool32 clipped             = VK_FALSE;

    const uint32_t imageWidth =
        (properties.currentExtent.width != 0xFFFFFFFFu) ?
            properties.currentExtent.width :
            de::min(1024u, properties.minImageExtent.width +
                               ((properties.maxImageExtent.width - properties.minImageExtent.width) / 2));
    const uint32_t imageHeight =
        (properties.currentExtent.height != 0xFFFFFFFFu) ?
            properties.currentExtent.height :
            de::min(1024u, properties.minImageExtent.height +
                               ((properties.maxImageExtent.height - properties.minImageExtent.height) / 2));
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
    }

    // Pick the first supported transform, alpha, and format:
    vk::VkSurfaceTransformFlagsKHR transform;
    for (transform = 1u; transform <= properties.supportedTransforms; transform = transform << 1u)
    {
        if ((properties.supportedTransforms & transform) != 0)
            break;
    }

    vk::VkCompositeAlphaFlagsKHR alpha;
    for (alpha = 1u; alpha <= properties.supportedCompositeAlpha; alpha = alpha << 1u)
    {
        if ((alpha & properties.supportedCompositeAlpha) != 0)
            break;
    }

    {
        const vk::VkSurfaceTransformFlagBitsKHR preTransform = (vk::VkSurfaceTransformFlagBitsKHR)transform;
        const vk::VkCompositeAlphaFlagBitsKHR compositeAlpha = (vk::VkCompositeAlphaFlagBitsKHR)alpha;
        const vk::VkFormat imageFormat                       = formats[0].format;
        const vk::VkColorSpaceKHR imageColorSpace            = formats[0].colorSpace;
        const vk::VkSwapchainCreateInfoKHR createInfo        = {vk::VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                                                nullptr,
                                                                0u,
                                                                surface,
                                                                properties.minImageCount,
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

        return createInfo;
    }
}

DisplayTimingTestInstance::DisplayTimingTestInstance(Context &context, const TestConfig &testConfig)
    : TestInstance(context)
    , m_useDisplayTiming(testConfig.useDisplayTiming)
    , m_quadCount(16u)
    , m_vkp(context.getPlatformInterface())
    , m_instanceExtensions(vk::enumerateInstanceExtensionProperties(m_vkp, nullptr))
    , m_instance(createInstanceWithWsi(context, m_instanceExtensions, testConfig.wsiType))
    , m_vki(m_instance.getDriver())
    , m_physicalDevice(vk::chooseDevice(m_vki, m_instance, context.getTestContext().getCommandLine()))
    , m_nativeDisplay(createDisplay(context.getTestContext().getPlatform().getVulkanPlatform(), m_instanceExtensions,
                                    testConfig.wsiType))
    , m_nativeWindow(createWindow(*m_nativeDisplay, tcu::Nothing))
    , m_surface(vk::wsi::createSurface(m_vki, m_instance, testConfig.wsiType, *m_nativeDisplay, *m_nativeWindow,
                                       context.getTestContext().getCommandLine()))

    , m_queueFamilyIndex(vk::wsi::chooseQueueFamilyIndex(m_vki, m_physicalDevice, *m_surface))
    , m_deviceExtensions(vk::enumerateDeviceExtensionProperties(m_vki, m_physicalDevice, nullptr))
    , m_device(createDeviceWithWsi(m_vkp, m_instance, m_vki, m_physicalDevice, m_deviceExtensions, m_queueFamilyIndex,
                                   testConfig.useDisplayTiming,
                                   context.getTestContext().getCommandLine().isValidationEnabled()))
    , m_vkd(m_vkp, m_instance, *m_device, context.getUsedApiVersion(), context.getTestContext().getCommandLine())
    , m_queue(getDeviceQueue(m_vkd, *m_device, m_queueFamilyIndex, 0u))

    , m_commandPool(createCommandPool(m_vkd, *m_device, m_queueFamilyIndex))
    , m_vertexShaderModule(vk::createShaderModule(m_vkd, *m_device, context.getBinaryCollection().get("quad-vert"), 0u))
    , m_fragmentShaderModule(
          vk::createShaderModule(m_vkd, *m_device, context.getBinaryCollection().get("quad-frag"), 0u))
    , m_pipelineLayout(createPipelineLayout(m_vkd, *m_device))

    , m_surfaceProperties(vk::wsi::getPhysicalDeviceSurfaceCapabilities(m_vki, m_physicalDevice, *m_surface))
    , m_surfaceFormats(vk::wsi::getPhysicalDeviceSurfaceFormats(m_vki, m_physicalDevice, *m_surface))
    , m_presentModes(vk::wsi::getPhysicalDeviceSurfacePresentModes(m_vki, m_physicalDevice, *m_surface))

    , m_freeAcquireSemaphore(VK_NULL_HANDLE)
    , m_freeRenderSemaphore(VK_NULL_HANDLE)

    , m_swapchainConfig(createSwapchainConfig(*m_surface, m_queueFamilyIndex, m_surfaceProperties, m_surfaceFormats,
                                              m_presentModes, testConfig.presentMode))

    , m_frameCount(60u * 5u)
    , m_frameNdx(0u)

    , m_maxOutOfDateCount(20u)
    , m_outOfDateCount(0u)
    , m_ExpectImage80Late(false)
{
    {
        const tcu::ScopedLogSection surfaceInfo(m_context.getTestContext().getLog(), "SurfaceCapabilities",
                                                "SurfaceCapabilities");
        m_context.getTestContext().getLog() << TestLog::Message << m_surfaceProperties << TestLog::EndMessage;
    }
}

DisplayTimingTestInstance::~DisplayTimingTestInstance(void)
{
    deinitSwapchainResources();
}

void DisplayTimingTestInstance::initSwapchainResources(void)
{
    const size_t fenceCount        = 6;
    const uint32_t imageWidth      = m_swapchainConfig.imageExtent.width;
    const uint32_t imageHeight     = m_swapchainConfig.imageExtent.height;
    const vk::VkFormat imageFormat = m_swapchainConfig.imageFormat;

    m_swapchain       = vk::createSwapchainKHR(m_vkd, *m_device, &m_swapchainConfig);
    m_swapchainImages = vk::wsi::getSwapchainImages(m_vkd, *m_device, *m_swapchain);
    m_isFirst.resize(m_swapchainImages.size(), true);

    m_renderPass = createRenderPass(m_vkd, *m_device, imageFormat);
    m_pipeline   = createPipeline(m_vkd, *m_device, *m_renderPass, *m_pipelineLayout, *m_vertexShaderModule,
                                  *m_fragmentShaderModule, imageWidth, imageHeight);

    m_swapchainImageViews = std::vector<vk::VkImageView>(m_swapchainImages.size(), VK_NULL_HANDLE);
    m_framebuffers        = std::vector<vk::VkFramebuffer>(m_swapchainImages.size(), VK_NULL_HANDLE);
    m_acquireSemaphores   = std::vector<vk::VkSemaphore>(m_swapchainImages.size(), VK_NULL_HANDLE);
    m_renderSemaphores    = std::vector<vk::VkSemaphore>(m_swapchainImages.size(), VK_NULL_HANDLE);

    m_fences         = std::vector<vk::VkFence>(fenceCount, VK_NULL_HANDLE);
    m_commandBuffers = std::vector<vk::VkCommandBuffer>(m_fences.size(), VK_NULL_HANDLE);

    m_freeAcquireSemaphore = VK_NULL_HANDLE;
    m_freeRenderSemaphore  = VK_NULL_HANDLE;

    m_freeAcquireSemaphore = createSemaphore(m_vkd, *m_device).disown();
    m_freeRenderSemaphore  = createSemaphore(m_vkd, *m_device).disown();

    initImageViews(m_vkd, *m_device, m_swapchainImages, imageFormat, m_swapchainImageViews);
    initFramebuffers(m_vkd, *m_device, *m_renderPass, m_swapchainImageViews, imageWidth, imageHeight, m_framebuffers);
    initSemaphores(m_vkd, *m_device, m_acquireSemaphores);
    initSemaphores(m_vkd, *m_device, m_renderSemaphores);

    initFences(m_vkd, *m_device, m_fences);

    if (m_useDisplayTiming)
    {
        // This portion should do interesting bits
        m_queuePresentTimes = std::map<uint32_t, uint64_t>();

        m_vkd.getRefreshCycleDurationGOOGLE(*m_device, *m_swapchain, &m_rcDuration);

        m_refreshDurationMultiplier = 1u;
        m_targetIPD                 = m_rcDuration.refreshDuration;
        m_prevDesiredPresentTime    = 0u;
        m_nextPresentID             = 0u;
        m_ignoreThruPresentID       = 0u;
    }
}

void DisplayTimingTestInstance::deinitSwapchainResources(void)
{
    VK_CHECK(m_vkd.queueWaitIdle(m_queue));

    if (m_freeAcquireSemaphore != VK_NULL_HANDLE)
    {
        m_vkd.destroySemaphore(*m_device, m_freeAcquireSemaphore, nullptr);
        m_freeAcquireSemaphore = VK_NULL_HANDLE;
    }

    if (m_freeRenderSemaphore != VK_NULL_HANDLE)
    {
        m_vkd.destroySemaphore(*m_device, m_freeRenderSemaphore, nullptr);
        m_freeRenderSemaphore = VK_NULL_HANDLE;
    }

    deinitSemaphores(m_vkd, *m_device, m_acquireSemaphores);
    deinitSemaphores(m_vkd, *m_device, m_renderSemaphores);
    deinitFences(m_vkd, *m_device, m_fences);
    deinitCommandBuffers(m_vkd, *m_device, *m_commandPool, m_commandBuffers);
    deinitFramebuffers(m_vkd, *m_device, m_framebuffers);
    deinitImageViews(m_vkd, *m_device, m_swapchainImageViews);

    m_swapchainImages.clear();
    m_isFirst.clear();

    m_swapchain  = vk::Move<vk::VkSwapchainKHR>();
    m_renderPass = vk::Move<vk::VkRenderPass>();
    m_pipeline   = vk::Move<vk::VkPipeline>();
}

vector<vk::VkPastPresentationTimingGOOGLE> getPastPresentationTiming(const vk::DeviceInterface &vkd,
                                                                     vk::VkDevice device, vk::VkSwapchainKHR swapchain)
{
    vector<vk::VkPastPresentationTimingGOOGLE> pastPresentationTimings;
    uint32_t numPastPresentationTimings = 0;

    vkd.getPastPresentationTimingGOOGLE(device, swapchain, &numPastPresentationTimings, nullptr);

    pastPresentationTimings.resize(numPastPresentationTimings);

    if (numPastPresentationTimings > 0)
        vkd.getPastPresentationTimingGOOGLE(device, swapchain, &numPastPresentationTimings,
                                            &pastPresentationTimings[0]);

    return pastPresentationTimings;
}

void DisplayTimingTestInstance::render(void)
{
    const uint64_t foreverNs = ~0x0ull;
    const vk::VkFence fence  = m_fences[m_frameNdx % m_fences.size()];
    const uint32_t width     = m_swapchainConfig.imageExtent.width;
    const uint32_t height    = m_swapchainConfig.imageExtent.height;
    tcu::TestLog &log        = m_context.getTestContext().getLog();

    // Throttle execution
    if (m_frameNdx >= m_fences.size())
    {
        VK_CHECK(m_vkd.waitForFences(*m_device, 1u, &fence, VK_TRUE, foreverNs));
        VK_CHECK(m_vkd.resetFences(*m_device, 1u, &fence));

        m_vkd.freeCommandBuffers(*m_device, *m_commandPool, 1u,
                                 &m_commandBuffers[m_frameNdx % m_commandBuffers.size()]);
        m_commandBuffers[m_frameNdx % m_commandBuffers.size()] = VK_NULL_HANDLE;
    }

    vk::VkSemaphore currentAcquireSemaphore = m_freeAcquireSemaphore;
    vk::VkSemaphore currentRenderSemaphore  = m_freeRenderSemaphore;
    uint32_t imageIndex;

    // Acquire next image
    VK_CHECK(m_vkd.acquireNextImageKHR(*m_device, *m_swapchain, foreverNs, currentAcquireSemaphore, VK_NULL_HANDLE,
                                       &imageIndex));

    // Create command buffer
    m_commandBuffers[m_frameNdx % m_commandBuffers.size()] =
        createCommandBuffer(m_vkd, *m_device, *m_commandPool, *m_pipelineLayout, *m_renderPass,
                            m_framebuffers[imageIndex], *m_pipeline, m_swapchainImages[imageIndex],
                            m_isFirst[imageIndex], m_frameNdx, m_quadCount, width, height)
            .disown();
    m_isFirst[imageIndex] = false;

    // Obtain timing data from previous frames
    if (m_useDisplayTiming)
    {
        const vector<vk::VkPastPresentationTimingGOOGLE> pastPresentationTimings(
            getPastPresentationTiming(m_vkd, *m_device, *m_swapchain));
        bool isEarly = false;
        bool isLate  = false;

        for (size_t pastPresentationInfoNdx = 0; pastPresentationInfoNdx < pastPresentationTimings.size();
             pastPresentationInfoNdx++)
        {
            if (m_queuePresentTimes[pastPresentationTimings[pastPresentationInfoNdx].presentID] >
                pastPresentationTimings[pastPresentationInfoNdx].actualPresentTime)
            {
                m_resultCollector.fail("Image with PresentID " +
                                       de::toString(pastPresentationTimings[pastPresentationInfoNdx].presentID) +
                                       "was displayed before vkQueuePresentKHR was called.");
            }
            if (!m_ignoreThruPresentID)
            {
                // This is the first time that we've received an
                // actualPresentTime for this swapchain.  In order to not
                // perceive these early frames as "late", we need to sync-up
                // our future desiredPresentTime's with the
                // actualPresentTime(s) that we're receiving now.
                const int64_t multiple = m_nextPresentID - pastPresentationTimings.back().presentID;

                m_prevDesiredPresentTime = pastPresentationTimings.back().actualPresentTime + (multiple * m_targetIPD);
                m_ignoreThruPresentID    = pastPresentationTimings[pastPresentationInfoNdx].presentID + 1;
            }
            else if (pastPresentationTimings[pastPresentationInfoNdx].presentID > m_ignoreThruPresentID)
            {
                if (pastPresentationTimings[pastPresentationInfoNdx].actualPresentTime >
                    (pastPresentationTimings[pastPresentationInfoNdx].desiredPresentTime +
                     m_rcDuration.refreshDuration + MILLISECOND))
                {
                    const uint64_t actual  = pastPresentationTimings[pastPresentationInfoNdx].actualPresentTime;
                    const uint64_t desired = pastPresentationTimings[pastPresentationInfoNdx].desiredPresentTime;
                    const uint64_t rdur    = m_rcDuration.refreshDuration;
                    const uint64_t diff1   = actual - (desired + rdur);
                    const uint64_t diff2   = actual - desired;

                    log << TestLog::Message << "Image PresentID "
                        << pastPresentationTimings[pastPresentationInfoNdx].presentID << " was " << diff1
                        << " nsec late." << TestLog::EndMessage;
                    if (m_ExpectImage80Late && (pastPresentationTimings[pastPresentationInfoNdx].presentID == 80))
                    {
                        if (diff1 > (SECOND / 2))
                            log << TestLog::Message
                                << "\tNote: Image PresentID 80 was expected to be late by approximately 1 second."
                                << TestLog::EndMessage;
                        else
                            m_resultCollector.fail(
                                "Image PresentID 80 was not late by approximately 1 second, as expected.");
                    }
                    log << TestLog::Message << "\t\t   actualPresentTime = " << actual << " nsec"
                        << TestLog::EndMessage;
                    log << TestLog::Message << "\t\t - desiredPresentTime= " << desired << " nsec"
                        << TestLog::EndMessage;
                    log << TestLog::Message << "\t\t =========================================" << TestLog::EndMessage;
                    log << TestLog::Message << "\t\t   diff              =       " << diff2 << " nsec"
                        << TestLog::EndMessage;
                    log << TestLog::Message << "\t\t - refreshDuration   =       " << rdur << " nsec"
                        << TestLog::EndMessage;
                    log << TestLog::Message << "\t\t =========================================" << TestLog::EndMessage;
                    log << TestLog::Message << "\t\t   diff              =        " << diff1 << " nsec"
                        << TestLog::EndMessage;

                    isLate = true;
                }
                else if ((pastPresentationTimings[pastPresentationInfoNdx].actualPresentTime >
                          pastPresentationTimings[pastPresentationInfoNdx].earliestPresentTime) &&
                         (pastPresentationTimings[pastPresentationInfoNdx].presentMargin > (2 * MILLISECOND)))
                {
                    const uint64_t actual   = pastPresentationTimings[pastPresentationInfoNdx].actualPresentTime;
                    const uint64_t earliest = pastPresentationTimings[pastPresentationInfoNdx].earliestPresentTime;
                    const uint64_t diff     = actual - earliest;

                    log << TestLog::Message << "Image PresentID "
                        << pastPresentationTimings[pastPresentationInfoNdx].presentID << " can be presented " << diff
                        << " nsec earlier." << TestLog::EndMessage;
                    log << TestLog::Message << "\t\t   actualPresentTime = " << actual << " nsec"
                        << TestLog::EndMessage;
                    log << TestLog::Message << "\t\t -earliestPresentTime= " << earliest << " nsec"
                        << TestLog::EndMessage;
                    log << TestLog::Message << "\t\t =========================================" << TestLog::EndMessage;
                    log << TestLog::Message << "\t\t   diff              =        " << diff << " nsec"
                        << TestLog::EndMessage;

                    isEarly = true;
                }
            }
        }
        // Preference is given to late presents over early presents:
        if (isLate)
        {
            // Demonstrate how to slow down the frame rate if a frame is late,
            // but don't go too slow (for test time reasons):
            if (++m_refreshDurationMultiplier > 2)
                m_refreshDurationMultiplier = 2;
            else
                log << TestLog::Message << "Increasing multiplier." << TestLog::EndMessage;
        }
        else if (isEarly)
        {
            // Demonstrate how to speed up the frame rate if a frame is early,
            // but don't let the multiplier hit zero:
            if (--m_refreshDurationMultiplier == 0)
                m_refreshDurationMultiplier = 1;
            else
                log << TestLog::Message << "Decreasing multiplier." << TestLog::EndMessage;
        }
        m_targetIPD = m_rcDuration.refreshDuration * m_refreshDurationMultiplier;
    }

    // Submit command buffer
    {
        const vk::VkPipelineStageFlags dstStageMask = vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        const vk::VkSubmitInfo submitInfo           = {vk::VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                                       nullptr,
                                                       1u,
                                                       &currentAcquireSemaphore,
                                                       &dstStageMask,
                                                       1u,
                                                       &m_commandBuffers[m_frameNdx % m_commandBuffers.size()],
                                                       1u,
                                                       &currentRenderSemaphore};

        VK_CHECK(m_vkd.queueSubmit(m_queue, 1u, &submitInfo, fence));
    }

    // Present frame
    if (m_useDisplayTiming)
    {
        // This portion should do interesting bits

        // Initially, mirror reference to move things along
        vk::VkResult result;
        vk::VkPresentTimeGOOGLE presentTime = {++m_nextPresentID, m_prevDesiredPresentTime};
        // Record the current time, to record as the time of the vkQueuePresentKHR() call:
        const uint64_t curtimeNano           = deGetMicroseconds() * 1000;
        m_queuePresentTimes[m_nextPresentID] = curtimeNano;

        uint64_t desiredPresentTime = 0u;
        if (m_prevDesiredPresentTime == 0)
        {
            // This must be the first present for this swapchain.  Find out the
            // current time, as the basis for desiredPresentTime:
            if (curtimeNano != 0)
            {
                presentTime.desiredPresentTime = curtimeNano;
                presentTime.desiredPresentTime += (m_targetIPD / 2);
            }
            else
            {
                // Since we didn't find out the current time, don't give a
                // desiredPresentTime:
                presentTime.desiredPresentTime = 0;
            }
        }
        else
        {
            desiredPresentTime = m_prevDesiredPresentTime + m_targetIPD;
            if ((presentTime.presentID == 80) && (m_swapchainConfig.presentMode != vk::VK_PRESENT_MODE_MAILBOX_KHR))
            {
                // Test if desiredPresentTime is 1 second earlier (i.e. before the previous image could have been presented)
                presentTime.desiredPresentTime -= SECOND;
                m_ExpectImage80Late = true;
            }
        }
        m_prevDesiredPresentTime = desiredPresentTime;

        const vk::VkPresentTimesInfoGOOGLE presentTimesInfo = {vk::VK_STRUCTURE_TYPE_PRESENT_TIMES_INFO_GOOGLE, nullptr,
                                                               1u, &presentTime};
        const vk::VkPresentInfoKHR presentInfo              = {vk::VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                                               &presentTimesInfo,
                                                               1u,
                                                               &currentRenderSemaphore,
                                                               1u,
                                                               &*m_swapchain,
                                                               &imageIndex,
                                                               &result};

        VK_CHECK_WSI(m_vkd.queuePresentKHR(m_queue, &presentInfo));
        VK_CHECK_WSI(result);
    }
    else
    {
        vk::VkResult result;
        const vk::VkPresentInfoKHR presentInfo = {vk::VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                                  nullptr,
                                                  1u,
                                                  &currentRenderSemaphore,
                                                  1u,
                                                  &*m_swapchain,
                                                  &imageIndex,
                                                  &result};

        VK_CHECK_WSI(m_vkd.queuePresentKHR(m_queue, &presentInfo));
        VK_CHECK_WSI(result);
    }

    {
        m_freeAcquireSemaphore          = m_acquireSemaphores[imageIndex];
        m_acquireSemaphores[imageIndex] = currentAcquireSemaphore;

        m_freeRenderSemaphore          = m_renderSemaphores[imageIndex];
        m_renderSemaphores[imageIndex] = currentRenderSemaphore;
    }
}

tcu::TestStatus DisplayTimingTestInstance::iterate(void)
{
    // Initialize swapchain specific resources
    // Render test
    try
    {
        if (m_frameNdx == 0)
        {
            if (m_outOfDateCount == 0)
                m_context.getTestContext().getLog()
                    << tcu::TestLog::Message << "Swapchain: " << m_swapchainConfig << tcu::TestLog::EndMessage;

            initSwapchainResources();
        }

        render();
    }
    catch (const vk::Error &error)
    {
        if (error.getError() == vk::VK_ERROR_OUT_OF_DATE_KHR)
        {
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

        return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
    }

    m_frameNdx++;

    if (m_frameNdx >= m_frameCount)
    {
        deinitSwapchainResources();

        return tcu::TestStatus(m_resultCollector.getResult(), m_resultCollector.getMessage());
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
            "highp float;\n"
            "void main (void) {\n"
            "\tgl_Position = vec4(((gl_VertexIndex + 2) / 3) % 2 == 0 ? -1.0 : 1.0,\n"
            "\t                   ((gl_VertexIndex + 1) / 3) % 2 == 0 ? -1.0 : 1.0, 0.0, 1.0);\n"
            "}\n");
        dst.glslSources.add("quad-frag") << glu::FragmentSource(
            "#version 310 es\n"
            "layout(location = 0) out highp vec4 o_color;\n"
            "layout(push_constant) uniform PushConstant {\n"
            "\thighp uint frameNdx;\n"
            "} pushConstants;\n"
            "void main (void)\n"
            "{\n"
            "\thighp uint frameNdx = pushConstants.frameNdx;\n"
            "\thighp uint x = frameNdx + uint(gl_FragCoord.x);\n"
            "\thighp uint y = frameNdx + uint(gl_FragCoord.y);\n"
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

void createDisplayTimingTests(tcu::TestCaseGroup *testGroup, vk::wsi::Type wsiType)
{
    const struct
    {
        vk::VkPresentModeKHR mode;
        const char *name;
    } presentModes[] = {
        {vk::VK_PRESENT_MODE_FIFO_KHR, "fifo"},
        {vk::VK_PRESENT_MODE_FIFO_RELAXED_KHR, "fifo_relaxed"},
        {vk::VK_PRESENT_MODE_IMMEDIATE_KHR, "immediate"},
        {vk::VK_PRESENT_MODE_MAILBOX_KHR, "mailbox"},
    };

    for (size_t presentModeNdx = 0; presentModeNdx < DE_LENGTH_OF_ARRAY(presentModes); presentModeNdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> presentModeGroup(
            new tcu::TestCaseGroup(testGroup->getTestContext(), presentModes[presentModeNdx].name));

        for (size_t ref = 0; ref < 2; ref++)
        {
            const bool isReference = (ref == 0);
            const char *const name = isReference ? "reference" : "display_timing";
            TestConfig config;

            config.wsiType          = wsiType;
            config.useDisplayTiming = !isReference;
            config.presentMode      = presentModes[presentModeNdx].mode;

            presentModeGroup->addChild(new vkt::InstanceFactory1<DisplayTimingTestInstance, TestConfig, Programs>(
                testGroup->getTestContext(), name, Programs(), config));
        }

        testGroup->addChild(presentModeGroup.release());
    }
}

} // namespace wsi
} // namespace vkt
