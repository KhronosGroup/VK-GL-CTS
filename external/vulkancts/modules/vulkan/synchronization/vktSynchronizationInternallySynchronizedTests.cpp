/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
 * Copyright (c) 2025 Google LLC
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
 * \brief Internally Synchronized tests
 *//*--------------------------------------------------------------------*/

#include "tcuTestCase.hpp"
#include "vktSynchronizationUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "tcuTextureUtil.hpp"
#include "vkWsiUtil.hpp"
#include "vkWsiPlatform.hpp"
#include "tcuPlatform.hpp"
#include "deThread.hpp"

#include <cassert>

namespace vkt
{
namespace synchronization
{

using namespace vk;

enum TestType
{
    TYPE_SMALL_IMAGE_SYNC,
    TYPE_LARGE_IMAGE_SYNC,
    TYPE_SMALL_IMAGE_SYNC2,
    TYPE_LARGE_IMAGE_SYNC2,
    TYPE_QUEUE_BIND_SPARSE,
    TYPE_WSI,
    TYPE_DEBUG_UTILS,
    TYPE_PERFORMANCE_CONFIGURATION,
    TYPE_OUT_OF_BAND,
    TYPE_DEVICE_WAIT_IDLE,
};

std::string getTestName(TestType type)
{
    switch (type)
    {
    case TYPE_SMALL_IMAGE_SYNC:
        return "small";
    case TYPE_LARGE_IMAGE_SYNC:
        return "large";
    case TYPE_SMALL_IMAGE_SYNC2:
        return "small2";
    case TYPE_LARGE_IMAGE_SYNC2:
        return "large2";
    case TYPE_QUEUE_BIND_SPARSE:
        return "bind_sparse";
    case TYPE_WSI:
        return "wsi";
    case TYPE_DEBUG_UTILS:
        return "debug_utils";
    case TYPE_PERFORMANCE_CONFIGURATION:
        return "performance_configuration";
    case TYPE_OUT_OF_BAND:
        return "out_of_band";
    case TYPE_DEVICE_WAIT_IDLE:
        return "device_wait_idle";
    default:
        break;
    }
    assert(0);
    return "";
}

enum QueueCreationType
{
    QUEUE_CREATION_SINGLE_QUEUE,
    QUEUE_CREATION_FIRST_INTERN_SYNCED,
    QUEUE_CREATION_LAST_INTERN_SYNCED,
    QUEUE_CREATION_TWO_INTERN_SYNCED_USE_FIRST,
    QUEUE_CREATION_TWO_INTERN_SYNCED_USE_LAST,
};

struct TestParameters
{
    vk::wsi::Type wsiType;
    TestType testType1;
    TestType testType2;
    TestType testType3;
    TestType testType4;

    QueueCreationType queueCreation;
    bool sameQueueFamily;

    bool hasWsiTest() const
    {
        return anyOf(TYPE_WSI);
    }
    bool hasSparseTest() const
    {
        return anyOf(TYPE_QUEUE_BIND_SPARSE);
    }
    bool hasDebugUtilsTest() const
    {
        return anyOf(TYPE_DEBUG_UTILS);
    }
    bool hasPerformanceConfigurationTest() const
    {
        return anyOf(TYPE_PERFORMANCE_CONFIGURATION);
    }
    bool hasOutOfBandTest() const
    {
        return anyOf(TYPE_OUT_OF_BAND);
    }

    bool anyOf(TestType type) const
    {
        return testType1 == type || testType2 == type || testType3 == type || testType4 == type;
    }
};

bool withinEpsilon(uint8_t value, uint8_t expected, uint8_t epsilon)
{
    return std::abs((int)value - (int)expected) <= epsilon;
}

de::MovePtr<wsi::Display> createDisplay(const vk::Platform &platform,
                                        const std::vector<VkExtensionProperties> &supportedExtensions,
                                        wsi::Type wsiType)
{
    try
    {
        return de::MovePtr<wsi::Display>(platform.createWsiDisplay(wsiType));
    }
    catch (const tcu::NotSupportedError &e)
    {
        if (isExtensionStructSupported(supportedExtensions, RequiredExtension(getExtensionName(wsiType))) &&
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

de::MovePtr<wsi::Window> createWindow(const wsi::Display &display, const tcu::Maybe<tcu::UVec2> &initialSize)
{
    try
    {
        return de::MovePtr<wsi::Window>(display.createWindow(initialSize));
    }
    catch (const tcu::NotSupportedError &e)
    {
        // See createDisplay - assuming that wsi::Display was supported platform port
        // should also support creating a window.
        throw tcu::TestError(e.getMessage());
    }
}

struct NativeObjects
{
    const de::UniquePtr<wsi::Display> display;
    const de::UniquePtr<wsi::Window> window;

    NativeObjects(Context &context, const std::vector<VkExtensionProperties> &supportedExtensions, wsi::Type wsiType,
                  const tcu::Maybe<tcu::UVec2> &initialWindowSize = tcu::Nothing)
        : display(
              createDisplay(context.getTestContext().getPlatform().getVulkanPlatform(), supportedExtensions, wsiType))
        , window(createWindow(*display, initialWindowSize))
    {
    }
};

class InternallySynchronizedQueuesTestInstance : public vkt::TestInstance
{
public:
    InternallySynchronizedQueuesTestInstance(vkt::Context &context, const TestParameters &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }

private:
    tcu::TestStatus iterate(void);

    const TestParameters m_params;
};

class CaseThread : public de::Thread
{
public:
    CaseThread(const TestType &testType, Context &context, wsi::Type wsiType, DeviceDriver &vk, VkDevice device,
               uint32_t queueFamilyIndex, VkQueue queue)
        : de::Thread()
        , m_testType(testType)
        , m_context(context)
        , m_wsiType(wsiType)
        , m_vk(vk)
        , m_device(device)
        , m_allocator(
              vk, device,
              getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()))
        , m_queueFamilyIndex(queueFamilyIndex)
        , m_queue(queue)
        , m_failed(false)
    {
        if (m_testType == TYPE_QUEUE_BIND_SPARSE)
            createSparseResources();
        else if (m_testType == TYPE_PERFORMANCE_CONFIGURATION)
            createPerformanceConfigurationResources();
        else if (m_testType == TYPE_DEVICE_WAIT_IDLE)
            createDeviceWaitIdleResources();
        else
            createDrawResources();
    }

    void createSparseResources()
    {
        const DeviceInterface &vk       = m_vk;
        const VkDevice device           = m_device;
        const uint32_t queueFamilyIndex = m_queueFamilyIndex;
        auto &alloc                     = m_allocator;

        VkBufferCreateInfo bufferCreateInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,                                // VkStructureType sType;
            nullptr,                                                             // const void* pNext;
            VK_BUFFER_CREATE_SPARSE_BINDING_BIT,                                 // VkBufferCreateFlags flags;
            m_sparseTestData.bufferSize,                                         // VkDeviceSize size;
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                           // VkSharingMode sharingMode;
            0u,                                                                  // uint32_t queueFamilyIndexCount;
            nullptr                                                              // const uint32_t* pQueueFamilyIndices;
        };

        // Create sparse buffer
        m_sparseTestData.sparseBuffer = createBuffer(vk, device, &bufferCreateInfo);

        VkImageCreateInfo imageCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType			sType;
            nullptr,                             // const void*				pNext;
            VK_IMAGE_CREATE_SPARSE_BINDING_BIT |
                VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT,                          // VkImageCreateFlags		flags;
            VK_IMAGE_TYPE_2D,                                                  // VkImageType				imageType;
            VK_FORMAT_R8G8B8A8_UNORM,                                          // VkFormat				format;
            m_sparseTestData.imageSize,                                        // VkExtent3D				extent;
            1u,                                                                // uint32_t				mipLevels;
            1u,                                                                // uint32_t				arrayLayers;
            VK_SAMPLE_COUNT_1_BIT,                                             // VkSampleCountFlagBits	samples;
            VK_IMAGE_TILING_OPTIMAL,                                           // VkImageTiling			tiling;
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags		usage;
            VK_SHARING_MODE_EXCLUSIVE,                                         // VkSharingMode			sharingMode;
            0u,                                                                // uint32_t				queueFamilyIndexCount;
            nullptr,                                                           // const uint32_t*			pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED                                          // VkImageLayout			initialLayout;
        };
        m_sparseTestData.sparseImage = createImage(vk, device, &imageCreateInfo);

        m_cmdPool = createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);

        m_sparseTestData.fence1 = createFence(vk, device);
        m_sparseTestData.fence2 = createFence(vk, device);

        const uint32_t outputBufferSize = de::max(
            m_sparseTestData.bufferSize, m_sparseTestData.imageSize.width * m_sparseTestData.imageSize.height * 4u);
        m_sparseTestData.outputBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
            vk, device, alloc, makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT),
            MemoryRequirement::HostVisible));

        m_sparseTestData.srcBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
            vk, device, alloc, makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
            MemoryRequirement::HostVisible));
        uint8_t *ptr               = (uint8_t *)m_sparseTestData.srcBuffer->getAllocation().getHostPtr();
        for (uint32_t i = 0; i < outputBufferSize; ++i)
            ptr[i] = (uint8_t)i % 255;
        flushAlloc(vk, device, m_sparseTestData.srcBuffer->getAllocation());
    }

    void createDrawResources()
    {
        const InstanceInterface &vki          = m_context.getInstanceInterface();
        const VkInstance instance             = m_context.getInstance();
        const DeviceInterface &vk             = m_vk;
        const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
        const VkDevice device                 = m_device;
        const uint32_t queueFamilyIndex       = m_queueFamilyIndex;
        auto &alloc                           = m_allocator;

        const VkExtent3D imageExtent = getImageExtent();
        const VkRect2D renderArea    = getRenderArea();

        const VkImageSubresourceRange subresourceRange =
            makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

        if (m_testType == TYPE_WSI)
        {
            std::vector<VkExtensionProperties> supportedExtensions =
                enumerateInstanceExtensionProperties(m_context.getPlatformInterface(), nullptr);
            const tcu::UVec2 desiredSize(smallImageExtent.width, smallImageExtent.height);
            m_drawTestData.nativeObjects = de::MovePtr<NativeObjects>(
                new NativeObjects(m_context, supportedExtensions, m_wsiType, tcu::just(desiredSize)));

            m_drawTestData.surface =
                createSurface(vki, instance, m_wsiType, *m_drawTestData.nativeObjects->display,
                              *m_drawTestData.nativeObjects->window, m_context.getTestContext().getCommandLine());

            if (vk::wsi::getPhysicalDeviceSurfaceSupport(vki, physicalDevice, m_queueFamilyIndex,
                                                         *m_drawTestData.surface) != VK_TRUE)
                TCU_THROW(NotSupportedError, "Surface not supported by physical device");

            const VkSurfaceCapabilitiesKHR capabilities =
                wsi::getPhysicalDeviceSurfaceCapabilities(vki, physicalDevice, *m_drawTestData.surface);

            const uint32_t imageWidth =
                (capabilities.currentExtent.width != 0xFFFFFFFFu) ?
                    capabilities.currentExtent.width :
                    de::min(1024u, capabilities.minImageExtent.width +
                                       ((capabilities.maxImageExtent.width - capabilities.minImageExtent.width) / 2));
            const uint32_t imageHeight =
                (capabilities.currentExtent.height != 0xFFFFFFFFu) ?
                    capabilities.currentExtent.height :
                    de::min(1024u, capabilities.minImageExtent.height +
                                       ((capabilities.maxImageExtent.height - capabilities.minImageExtent.height) / 2));

            const std::vector<VkSurfaceFormatKHR> surfaceFormats =
                wsi::getPhysicalDeviceSurfaceFormats(vki, physicalDevice, *m_drawTestData.surface);
            VkSurfaceFormatKHR surfaceFormat = surfaceFormats[0];
            for (const auto &format : surfaceFormats)
            {
                if (format.format == VK_FORMAT_R8G8B8A8_UNORM)
                {
                    surfaceFormat = format;
                    break;
                }
            }

            std::vector<VkPresentModeKHR> presentModes =
                wsi::getPhysicalDeviceSurfacePresentModes(vki, physicalDevice, *m_drawTestData.surface);
            VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
            for (const auto &p : presentModes)
            {
                if (p == VK_PRESENT_MODE_MAILBOX_KHR || p == VK_PRESENT_MODE_IMMEDIATE_KHR)
                {
                    presentMode = p;
                    break;
                }
            }

            const VkSwapchainCreateInfoKHR swapchainInfo = {
                VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, // VkStructureType					sType;
                nullptr,                                     // const void*						pNext;
                0u,                                          // VkSwapchainCreateFlagsKHR		flags;
                *m_drawTestData.surface,                     // VkSurfaceKHR					surface;
                capabilities.minImageCount,                  // uint32_t						minImageCount;
                surfaceFormat.format,                        // VkFormat						imageFormat;
                surfaceFormat.colorSpace,                    // VkColorSpaceKHR					imageColorSpace;
                {imageWidth, imageHeight},                   // VkExtent2D						imageExtent;
                1u,                                          // uint32_t						imageArrayLayers;
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT,   // VkImageUsageFlags				imageUsage;
                VK_SHARING_MODE_EXCLUSIVE,             // VkSharingMode					imageSharingMode;
                0u,                                    // uint32_t						queueFamilyIndexCount;
                nullptr,                               // const uint32_t*					pQueueFamilyIndices;
                VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR, // VkSurfaceTransformFlagBitsKHR	preTransform;
                VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,     // VkCompositeAlphaFlagBitsKHR		compositeAlpha;
                presentMode,                           // VkPresentModeKHR				presentMode;
                VK_FALSE,                              // VkBool32						clipped;
                VK_NULL_HANDLE,                        // VkSwapchainKHR					oldSwapchain;
            };

            m_drawTestData.colorFormat     = surfaceFormat.format;
            m_drawTestData.swapchain       = createSwapchainKHR(vk, device, &swapchainInfo);
            m_drawTestData.swapchainImages = wsi::getSwapchainImages(vk, device, *m_drawTestData.swapchain);
            for (const auto &image : m_drawTestData.swapchainImages)
            {
                VkImageViewCreateInfo imageViewCreateInfo = {
                    VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
                    nullptr,                                  // const void* pNext;
                    (VkImageViewCreateFlags)0u,               // VkImageViewCreateFlags flags;
                    image,                                    // VkImage image;
                    VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
                    surfaceFormat.format,                     // VkFormat format;
                    makeComponentMappingRGBA(),               // VkComponentMapping components;
                    subresourceRange                          // VkImageSubresourceRange subresourceRange;
                };
                m_drawTestData.swapchainImageViews.push_back(
                    createImageView(vk, device, &imageViewCreateInfo, nullptr));
            }
        }
        else
        {
            m_drawTestData.colorFormat   = VK_FORMAT_R8G8B8A8_UNORM;
            VkImageCreateInfo createInfo = {
                VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType          sType
                nullptr,                             // const void*              pNext
                0u,                                  // VkImageCreateFlags       flags
                VK_IMAGE_TYPE_2D,                    // VkImageType              imageType
                m_drawTestData.colorFormat,          // VkFormat                 format
                imageExtent,                         // VkExtent3D               extent
                1u,                                  // uint32_t                 mipLevels
                1u,                                  // uint32_t                 arrayLayers
                VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits    samples
                VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling            tiling
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags        usage
                VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode            sharingMode
                0,                         // uint32_t                 queueFamilyIndexCount
                nullptr,                   // const uint32_t*          pQueueFamilyIndices
                VK_IMAGE_LAYOUT_UNDEFINED  // VkImageLayout            initialLayout
            };

            m_drawTestData.image = de::MovePtr<ImageWithMemory>(
                new ImageWithMemory(vk, device, alloc, createInfo, MemoryRequirement::Any));

            VkImageViewCreateInfo imageViewCreateInfo = {
                VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
                nullptr,                                  // const void* pNext;
                (VkImageViewCreateFlags)0u,               // VkImageViewCreateFlags flags;
                **m_drawTestData.image,                   // VkImage image;
                VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
                m_drawTestData.colorFormat,               // VkFormat format;
                makeComponentMappingRGBA(),               // VkComponentMapping components;
                subresourceRange                          // VkImageSubresourceRange subresourceRange;
            };
            m_drawTestData.imageView = createImageView(vk, device, &imageViewCreateInfo, nullptr);
        }

        m_cmdPool = createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);

        VkImageLayout finalLayout =
            m_testType == TYPE_WSI ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        const VkAttachmentDescription colorAttachmentDescription = {
            (VkAttachmentDescriptionFlags)0u, // VkAttachmentDescriptionFlags    flags
            m_drawTestData.colorFormat,       // VkFormat                        format
            VK_SAMPLE_COUNT_1_BIT,            // VkSampleCountFlagBits           samples
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp              loadOp
            VK_ATTACHMENT_STORE_OP_STORE,     // VkAttachmentStoreOp             storeOp
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp              stencilLoadOp
            VK_ATTACHMENT_STORE_OP_DONT_CARE, // VkAttachmentStoreOp             stencilStoreOp
            VK_IMAGE_LAYOUT_UNDEFINED,        // VkImageLayout                   initialLayout
            finalLayout                       // VkImageLayout                   finalLayout
        };

        const VkAttachmentReference colorAttachmentRef = {
            0u,                                      // uint32_t         attachment
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout    layout
        };

        const VkSubpassDescription subpassDescription = {
            (VkSubpassDescriptionFlags)0u,   // VkSubpassDescriptionFlags       flags
            VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint             pipelineBindPoint
            0u,                              // uint32_t                        inputAttachmentCount
            nullptr,                         // const VkAttachmentReference*    pInputAttachments
            1u,                              // uint32_t                        colorAttachmentCount
            &colorAttachmentRef,             // const VkAttachmentReference*    pColorAttachments
            nullptr,                         // const VkAttachmentReference*    pResolveAttachments
            nullptr,                         // const VkAttachmentReference*    pDepthStencilAttachment
            0u,                              // uint32_t                        preserveAttachmentCount
            nullptr                          // const uint32_t*                 pPreserveAttachments
        };

        const VkRenderPassCreateInfo renderPassInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
            nullptr,                                   // const void* pNext;
            0u,                                        // VkRenderPassCreateFlags flags;
            1u,                                        // uint32_t attachmentCount;
            &colorAttachmentDescription,               // const VkAttachmentDescription* pAttachments;
            1u,                                        // uint32_t subpassCount;
            &subpassDescription,                       // const VkSubpassDescription* pSubpasses;
            0u,                                        // uint32_t dependencyCount;
            nullptr,                                   // const VkSubpassDependency* pDependencies;
        };
        m_drawTestData.renderPass = createRenderPass(vk, device, &renderPassInfo);
        if (m_testType == TYPE_WSI)
        {
            for (const auto &imageView : m_drawTestData.swapchainImageViews)
            {
                m_drawTestData.swapchainFramebuffers.push_back(makeFramebuffer(vk, device, *m_drawTestData.renderPass,
                                                                               *imageView, renderArea.extent.width,
                                                                               renderArea.extent.height));
            }
        }
        else
        {
            m_drawTestData.framebuffer =
                makeFramebuffer(vk, device, *m_drawTestData.renderPass, *m_drawTestData.imageView,
                                renderArea.extent.width, renderArea.extent.height);
        }

        ShaderWrapper vert = ShaderWrapper(vk, device, m_context.getBinaryCollection().get("vert"));
        ShaderWrapper frag = ShaderWrapper(vk, device, m_context.getBinaryCollection().get("frag"));
        const PipelineLayoutWrapper pipelineLayout(PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, vk, device);
        m_drawTestData.pipeline = de::MovePtr<GraphicsPipelineWrapper>(
            new GraphicsPipelineWrapper(vki, vk, physicalDevice, device, {}, PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC));
        const VkPipelineVertexInputStateCreateInfo vertexInput = initVulkanStructure();
        const std::vector<VkViewport> viewport{makeViewport(renderArea.extent)};
        const std::vector<VkRect2D> scissor{makeRect2D(renderArea.extent)};
        m_drawTestData.pipeline->setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
            .setDefaultRasterizationState()
            .setDefaultMultisampleState()
            .setDefaultDepthStencilState()
            .setDefaultColorBlendState()
            .setupVertexInputState(&vertexInput)
            .setupPreRasterizationShaderState(viewport, scissor, pipelineLayout, *m_drawTestData.renderPass, 0u, vert)
            .setupFragmentShaderState(pipelineLayout, *m_drawTestData.renderPass, 0u, frag)
            .setupFragmentOutputState(*m_drawTestData.renderPass)
            .setMonolithicPipelineLayout(pipelineLayout)
            .buildPipeline();

        const auto outputBufferSize = renderArea.extent.width * renderArea.extent.height * 4u;
        m_drawTestData.outputBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
            vk, device, alloc, makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT),
            MemoryRequirement::HostVisible));
    }

    void createPerformanceConfigurationResources()
    {
        const DeviceInterface &vk = m_vk;
        const VkDevice device     = m_device;

        VkQueryPoolPerformanceQueryCreateInfoINTEL queryPoolIntel = {
            VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO_INTEL, // VkStructureType				sType;
            nullptr,                                        // const void*					pNext;
            VK_QUERY_POOL_SAMPLING_MODE_MANUAL_INTEL,       // VkQueryPoolSamplingModeINTEL	performanceCountersSampling;
        };

        VkQueryPoolCreateInfo queryPoolCreateInfo = {
            VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, // VkStructureType					sType;
            &queryPoolIntel,                          // const void*						pNext;
            0,                                        // VkQueryPoolCreateFlags			flags;
            VK_QUERY_TYPE_PERFORMANCE_QUERY_INTEL,    // VkQueryType						queryType;
            1,                                        // uint32_t						queryCount;
            0                                         // VkQueryPipelineStatisticFlags	pipelineStatistics;
        };

        m_performanceConfigurationTestData.queryPool = createQueryPool(vk, device, &queryPoolCreateInfo);
    }

    void createDeviceWaitIdleResources()
    {
        const DeviceInterface &vk       = m_vk;
        const VkDevice device           = m_device;
        const uint32_t queueFamilyIndex = m_queueFamilyIndex;
        auto &alloc                     = m_allocator;

        VkBufferCreateInfo bufferCreateInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0,                                    // VkBufferCreateFlags flags;
            m_deviceWaitIdleTestData.bufferSize,  // VkDeviceSize size;
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT |    // VkBufferUsageFlags usage;
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
            0u,                        // uint32_t queueFamilyIndexCount;
            nullptr                    // const uint32_t* pQueueFamilyIndices;
        };
        m_deviceWaitIdleTestData.srcBuffer = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vk, device, alloc, bufferCreateInfo, MemoryRequirement::Any));
        m_deviceWaitIdleTestData.dstBuffer = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vk, device, alloc, bufferCreateInfo, MemoryRequirement::Any));

        m_cmdPool = createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    }

    virtual ~CaseThread(void)
    {
    }

    virtual void run()
    {
        if (m_testType == TYPE_QUEUE_BIND_SPARSE)
        {
            for (uint32_t i = 0; i < 500; ++i)
                runQueueBindSparse(i);
        }
        else if (m_testType == TYPE_LARGE_IMAGE_SYNC || m_testType == TYPE_LARGE_IMAGE_SYNC2)
        {
            for (uint32_t i = 0; i < 50; ++i)
                runDraw();
        }
        else if (m_testType == TYPE_WSI)
        {
            for (uint32_t i = 0; i < 700; ++i)
                runDraw();
        }
        else if (m_testType == TYPE_PERFORMANCE_CONFIGURATION)
        {
            for (uint32_t i = 0; i < 1000; ++i)
                runPerformanceConfiguration();
        }
        else if (m_testType == TYPE_DEVICE_WAIT_IDLE)
        {
            for (uint32_t i = 0; i < 1000; ++i)
                runDeviceWaitIdle();
        }
        else
        {
            for (uint32_t i = 0; i < 1000; ++i)
                runDraw();
        }
    }

    void bindSparseBuffer(VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence fence,
                          de::MovePtr<Allocation> &bufferAllocation)
    {
        const DeviceInterface &vk = m_vk;
        const VkDevice device     = m_device;
        auto &alloc               = m_allocator;

        VkMemoryRequirements bufferMemoryRequirements =
            getBufferMemoryRequirements(vk, device, *m_sparseTestData.sparseBuffer);
        bufferAllocation = alloc.allocate(bufferMemoryRequirements, MemoryRequirement::Any);

        const VkSparseMemoryBind sparseMemoryBind = {
            0,                             // VkDeviceSize resourceOffset;
            bufferMemoryRequirements.size, // VkDeviceSize size;
            bufferAllocation->getMemory(), // VkDeviceMemory memory;
            0,                             // VkDeviceSize memoryOffset;
            0                              // VkSparseMemoryBindFlags flags;
        };

        const VkSparseBufferMemoryBindInfo sparseBufferMemoryBindInfo = {
            *m_sparseTestData.sparseBuffer, // VkBuffer buffer;
            1u,                             // uint32_t bindCount;
            &sparseMemoryBind               // const VkSparseMemoryBind* pBinds;
        };

        VkBindSparseInfo bindInfo = {
            VK_STRUCTURE_TYPE_BIND_SPARSE_INFO, // VkStructureType							sType;
            nullptr,                            // const void*								pNext;
            1u,                                 // uint32_t									waitSemaphoreCount;
            &waitSemaphore,                     // const VkSemaphore*						pWaitSemaphores;
            1u,                                 // uint32_t									bufferBindCount;
            &sparseBufferMemoryBindInfo,        // const VkSparseBufferMemoryBindInfo*		pBufferBinds;
            0u,                                 // uint32_t									imageOpaqueBindCount;
            nullptr,                            // const VkSparseImageOpaqueMemoryBindInfo*	pImageOpaqueBinds;
            0u,                                 // uint32_t									imageBindCount;
            nullptr,                            // const VkSparseImageMemoryBindInfo*		pImageBinds;
            1u,                                 // uint32_t									signalSemaphoreCount;
            &signalSemaphore,                   // const VkSemaphore*						pSignalSemaphores;
        };

        VK_CHECK(vk.queueBindSparse(m_queue, 1u, &bindInfo, fence));
    }

    void bindSparseImage(VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence fence,
                         std::vector<de::SharedPtr<Allocation>> &allocations)
    {
        const InstanceInterface &vki          = m_context.getInstanceInterface();
        const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
        const DeviceInterface &vk             = m_vk;
        const VkDevice device                 = m_device;
        auto &alloc                           = m_allocator;

        const VkImageAspectFlags imageAspectFlags         = getImageAspectFlags(mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM));
        const VkPhysicalDeviceProperties deviceProperties = getPhysicalDeviceProperties(vki, physicalDevice);
        const VkPhysicalDeviceMemoryProperties deviceMemoryProperties =
            getPhysicalDeviceMemoryProperties(vki, physicalDevice);
        uint32_t sparseMemoryReqCount = 0;

        // Check if the image format supports sparse operations
        {
            VkImageCreateInfo imageCreateInfo = {
                VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType			sType;
                nullptr,                             // const void*				pNext;
                VK_IMAGE_CREATE_SPARSE_BINDING_BIT |
                    VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT,                          // VkImageCreateFlags		flags;
                VK_IMAGE_TYPE_2D,                                                  // VkImageType				imageType;
                VK_FORMAT_R8G8B8A8_UNORM,                                          // VkFormat				format;
                m_sparseTestData.imageSize,                                        // VkExtent3D				extent;
                1u,                                                                // uint32_t				mipLevels;
                1u,                                                                // uint32_t				arrayLayers;
                VK_SAMPLE_COUNT_1_BIT,                                             // VkSampleCountFlagBits	samples;
                VK_IMAGE_TILING_OPTIMAL,                                           // VkImageTiling			tiling;
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags		usage;
                VK_SHARING_MODE_EXCLUSIVE,                                         // VkSharingMode			sharingMode;
                0u,                                                                // uint32_t				queueFamilyIndexCount;
                nullptr,                  // const uint32_t*			pQueueFamilyIndices;
                VK_IMAGE_LAYOUT_UNDEFINED // VkImageLayout			initialLayout;
            };
            if (!checkSparseImageFormatSupport(physicalDevice, vki, imageCreateInfo))
                TCU_THROW(NotSupportedError, "The image format does not support sparse operations.");
        }

        vk.getImageSparseMemoryRequirements(device, *m_sparseTestData.sparseImage, &sparseMemoryReqCount, nullptr);

        DE_ASSERT(sparseMemoryReqCount != 0);

        std::vector<VkSparseImageMemoryRequirements> sparseImageMemoryRequirements;
        sparseImageMemoryRequirements.resize(sparseMemoryReqCount);

        vk.getImageSparseMemoryRequirements(device, *m_sparseTestData.sparseImage, &sparseMemoryReqCount,
                                            &sparseImageMemoryRequirements[0]);

        const uint32_t noMatchFound = ~((uint32_t)0);

        std::vector<uint32_t> aspectIndices;

        VkImageAspectFlags memReqAspectFlags = 0;

        for (uint32_t memoryReqNdx = 0; memoryReqNdx < sparseMemoryReqCount; ++memoryReqNdx)
        {
            if (sparseImageMemoryRequirements[memoryReqNdx].formatProperties.aspectMask & imageAspectFlags)
            {
                aspectIndices.push_back(memoryReqNdx);
                memReqAspectFlags |= sparseImageMemoryRequirements[memoryReqNdx].formatProperties.aspectMask;
            }
        }

        uint32_t metadataAspectIndex = noMatchFound;
        for (uint32_t memoryReqNdx = 0; memoryReqNdx < sparseMemoryReqCount; ++memoryReqNdx)
        {
            if (sparseImageMemoryRequirements[memoryReqNdx].formatProperties.aspectMask & VK_IMAGE_ASPECT_METADATA_BIT)
            {
                metadataAspectIndex = memoryReqNdx;
                break;
            }
        }

        if (memReqAspectFlags != imageAspectFlags)
            TCU_THROW(NotSupportedError, "Required image aspect not supported.");

        const VkMemoryRequirements memoryRequirements =
            getImageMemoryRequirements(vk, device, *m_sparseTestData.sparseImage);

        uint32_t memoryType = noMatchFound;
        for (uint32_t memoryTypeNdx = 0; memoryTypeNdx < deviceMemoryProperties.memoryTypeCount; ++memoryTypeNdx)
        {
            if ((memoryRequirements.memoryTypeBits & (1u << memoryTypeNdx)) != 0 &&
                MemoryRequirement::Any.matchesHeap(deviceMemoryProperties.memoryTypes[memoryTypeNdx].propertyFlags))
            {
                memoryType = memoryTypeNdx;
                break;
            }
        }

        if (memoryType == noMatchFound)
            TCU_THROW(NotSupportedError, "No matching memory type found.");

        if (memoryRequirements.size > deviceProperties.limits.sparseAddressSpaceSize)
            TCU_THROW(NotSupportedError, "Required memory size for sparse resource exceeds device limits.");

        std::vector<VkSparseImageMemoryBind> imageResidencyMemoryBinds;
        std::vector<VkSparseMemoryBind> imageMipTailMemoryBinds;

        for (uint32_t aspectIndex : aspectIndices)
        {
            const VkSparseImageMemoryRequirements aspectRequirements = sparseImageMemoryRequirements[aspectIndex];
            VkExtent3D blockSize = aspectRequirements.formatProperties.imageGranularity;

            uint32_t layerNdx = 0;
            {
                for (uint32_t mipLevelNdx = 0; mipLevelNdx < aspectRequirements.imageMipTailFirstLod; ++mipLevelNdx)
                {
                    const VkExtent3D mipExtent       = mipLevelExtents(m_sparseTestData.imageSize, mipLevelNdx);
                    const tcu::UVec3 numSparseBinds  = alignedDivide(mipExtent, blockSize);
                    const tcu::UVec3 lastBlockExtent = tcu::UVec3(
                        mipExtent.width % blockSize.width ? mipExtent.width % blockSize.width : blockSize.width,
                        mipExtent.height % blockSize.height ? mipExtent.height % blockSize.height : blockSize.height,
                        mipExtent.depth % blockSize.depth ? mipExtent.depth % blockSize.depth : blockSize.depth);

                    for (uint32_t z = 0; z < numSparseBinds.z(); ++z)
                        for (uint32_t y = 0; y < numSparseBinds.y(); ++y)
                            for (uint32_t x = 0; x < numSparseBinds.x(); ++x)
                            {
                                const VkMemoryRequirements allocRequirements = {
                                    // 28.7.5 alignment shows the block size in bytes
                                    memoryRequirements.alignment,      // VkDeviceSize size;
                                    memoryRequirements.alignment,      // VkDeviceSize alignment;
                                    memoryRequirements.memoryTypeBits, // uint32_t memoryTypeBits;
                                };

                                de::SharedPtr<Allocation> allocation(
                                    alloc.allocate(allocRequirements, MemoryRequirement::Any).release());
                                allocations.push_back(allocation);

                                VkOffset3D offset;
                                offset.x = x * blockSize.width;
                                offset.y = y * blockSize.height;
                                offset.z = z * blockSize.depth;

                                VkExtent3D extent;
                                extent.width  = (x == numSparseBinds.x() - 1) ? lastBlockExtent.x() : blockSize.width;
                                extent.height = (y == numSparseBinds.y() - 1) ? lastBlockExtent.y() : blockSize.height;
                                extent.depth  = (z == numSparseBinds.z() - 1) ? lastBlockExtent.z() : blockSize.depth;

                                const VkSparseImageMemoryBind imageMemoryBind = {
                                    {
                                        aspectRequirements.formatProperties
                                            .aspectMask,     // VkImageAspectFlags aspectMask;
                                        mipLevelNdx,         // uint32_t mipLevel;
                                        layerNdx,            // uint32_t arrayLayer;
                                    },                       // VkImageSubresource subresource;
                                    offset,                  // VkOffset3D offset;
                                    extent,                  // VkExtent3D extent;
                                    allocation->getMemory(), // VkDeviceMemory memory;
                                    allocation->getOffset(), // VkDeviceSize memoryOffset;
                                    0u,                      // VkSparseMemoryBindFlags flags;
                                };

                                imageResidencyMemoryBinds.push_back(imageMemoryBind);
                            }
                }

                // Handle MIP tail. There are two cases to consider here:
                //
                // 1) VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT is requested by the driver: each layer needs a separate tail.
                // 2) otherwise:                                                            only one tail is needed.
                if (aspectRequirements.imageMipTailSize > 0)
                {
                    if (layerNdx == 0 ||
                        (aspectRequirements.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) == 0)
                    {
                        const VkMemoryRequirements allocRequirements = {
                            aspectRequirements.imageMipTailSize, // VkDeviceSize size;
                            memoryRequirements.alignment,        // VkDeviceSize alignment;
                            memoryRequirements.memoryTypeBits,   // uint32_t memoryTypeBits;
                        };

                        const de::SharedPtr<Allocation> allocation(
                            alloc.allocate(allocRequirements, MemoryRequirement::Any).release());

                        const VkSparseMemoryBind imageMipTailMemoryBind = {
                            aspectRequirements.imageMipTailOffset +
                                layerNdx * aspectRequirements.imageMipTailStride, // VkDeviceSize resourceOffset;
                            aspectRequirements.imageMipTailSize,                  // VkDeviceSize size;
                            allocation->getMemory(),                              // VkDeviceMemory memory;
                            allocation->getOffset(),                              // VkDeviceSize memoryOffset;
                            0u,                                                   // VkSparseMemoryBindFlags flags;
                        };

                        allocations.push_back(allocation);

                        imageMipTailMemoryBinds.push_back(imageMipTailMemoryBind);
                    }
                }

                // Handle Metadata. Similarly to MIP tail in aspectRequirements, there are two cases to consider here:
                //
                // 1) VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT is requested by the driver: each layer needs a separate tail.
                // 2) otherwise:
                if (metadataAspectIndex != noMatchFound)
                {
                    const VkSparseImageMemoryRequirements metadataAspectRequirements =
                        sparseImageMemoryRequirements[metadataAspectIndex];

                    if (layerNdx == 0 || (metadataAspectRequirements.formatProperties.flags &
                                          VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) == 0)
                    {
                        const VkMemoryRequirements metadataAllocRequirements = {
                            metadataAspectRequirements.imageMipTailSize, // VkDeviceSize size;
                            memoryRequirements.alignment,                // VkDeviceSize alignment;
                            memoryRequirements.memoryTypeBits,           // uint32_t memoryTypeBits;
                        };
                        const de::SharedPtr<Allocation> metadataAllocation(
                            alloc.allocate(metadataAllocRequirements, MemoryRequirement::Any).release());

                        const VkSparseMemoryBind metadataMipTailMemoryBind = {
                            metadataAspectRequirements.imageMipTailOffset +
                                layerNdx *
                                    metadataAspectRequirements.imageMipTailStride, // VkDeviceSize resourceOffset;
                            metadataAspectRequirements.imageMipTailSize,           // VkDeviceSize size;
                            metadataAllocation->getMemory(),                       // VkDeviceMemory memory;
                            metadataAllocation->getOffset(),                       // VkDeviceSize memoryOffset;
                            VK_SPARSE_MEMORY_BIND_METADATA_BIT                     // VkSparseMemoryBindFlags flags;
                        };

                        allocations.push_back(metadataAllocation);

                        imageMipTailMemoryBinds.push_back(metadataMipTailMemoryBind);
                    }
                }
            }
        }

        VkBindSparseInfo bindSparseInfo = {
            VK_STRUCTURE_TYPE_BIND_SPARSE_INFO, //VkStructureType sType;
            nullptr,                            //const void* pNext;
            1u,                                 //uint32_t waitSemaphoreCount;
            &waitSemaphore,                     //const VkSemaphore* pWaitSemaphores;
            0u,                                 //uint32_t bufferBindCount;
            nullptr,                            //const VkSparseBufferMemoryBindInfo* pBufferBinds;
            0u,                                 //uint32_t imageOpaqueBindCount;
            nullptr,                            //const VkSparseImageOpaqueMemoryBindInfo* pImageOpaqueBinds;
            0u,                                 //uint32_t imageBindCount;
            nullptr,                            //const VkSparseImageMemoryBindInfo* pImageBinds;
            1u,                                 //uint32_t signalSemaphoreCount;
            &signalSemaphore                    //const VkSemaphore* pSignalSemaphores;
        };

        VkSparseImageMemoryBindInfo imageResidencyBindInfo;
        VkSparseImageOpaqueMemoryBindInfo imageMipTailBindInfo;

        if (imageResidencyMemoryBinds.size() > 0)
        {
            imageResidencyBindInfo.image     = *m_sparseTestData.sparseImage;
            imageResidencyBindInfo.bindCount = static_cast<uint32_t>(imageResidencyMemoryBinds.size());
            imageResidencyBindInfo.pBinds    = &imageResidencyMemoryBinds[0];

            bindSparseInfo.imageBindCount = 1u;
            bindSparseInfo.pImageBinds    = &imageResidencyBindInfo;
        }

        if (imageMipTailMemoryBinds.size() > 0)
        {
            imageMipTailBindInfo.image     = *m_sparseTestData.sparseImage;
            imageMipTailBindInfo.bindCount = static_cast<uint32_t>(imageMipTailMemoryBinds.size());
            imageMipTailBindInfo.pBinds    = &imageMipTailMemoryBinds[0];

            bindSparseInfo.imageOpaqueBindCount = 1u;
            bindSparseInfo.pImageOpaqueBinds    = &imageMipTailBindInfo;
        }

        VK_CHECK(vk.queueBindSparse(m_queue, 1u, &bindSparseInfo, fence));
    }

    void runQueueBindSparse(uint32_t i)
    {
        const DeviceInterface &vk = m_vk;
        const VkDevice device     = m_device;

        const bool testBuffer = i % 2 == 0;

        {
            const uint32_t outputBufferSize = de::max(
                m_sparseTestData.bufferSize, m_sparseTestData.imageSize.width * m_sparseTestData.imageSize.height * 4u);
            uint32_t *output = (uint32_t *)(*m_sparseTestData.outputBuffer).getAllocation().getHostPtr();
            memset(output, 0, outputBufferSize);
            flushAlloc(vk, device, (*m_sparseTestData.srcBuffer).getAllocation());
        }

        const VkImageSubresourceRange subresourceRange =
            makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
        const VkImageSubresourceLayers subresourceLayers =
            makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);

        const VkQueue queue = m_queue;

        VkFence fences[2] = {*m_sparseTestData.fence1, *m_sparseTestData.fence2};
        vk.resetFences(device, 2u, fences);

        Move<VkSemaphore> waitSemaphore   = createSemaphore(vk, device);
        Move<VkSemaphore> signalSemaphore = createSemaphore(vk, device);

        VkSubmitInfo submit         = initVulkanStructure();
        submit.signalSemaphoreCount = 1u;
        submit.pSignalSemaphores    = &*waitSemaphore;
        vk.queueSubmit(m_queue, 1u, &submit, VK_NULL_HANDLE);

        de::MovePtr<Allocation> bufferAllocation;
        std::vector<de::SharedPtr<Allocation>> imageAllocations;
        if (testBuffer)
        {
            bindSparseBuffer(*waitSemaphore, *signalSemaphore, fences[0], bufferAllocation);
        }
        else
        {
            bindSparseImage(*waitSemaphore, *signalSemaphore, fences[0], imageAllocations);
        }

        const Move<VkCommandBuffer> cmdBuffer(
            allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

        uint32_t data = 0x12345678;

        beginCommandBuffer(vk, *cmdBuffer);
        if (testBuffer)
        {
            vk.cmdFillBuffer(*cmdBuffer, *m_sparseTestData.sparseBuffer, 0u, VK_WHOLE_SIZE, 0x12345678);
            VkBufferCopy bufferCopy = makeBufferCopy(0u, 0u, m_sparseTestData.bufferSize);
            vk.cmdCopyBuffer(*cmdBuffer, *m_sparseTestData.sparseBuffer, **m_sparseTestData.outputBuffer, 1u,
                             &bufferCopy);
        }
        else
        {
            const VkImageMemoryBarrier preBarrier =
                makeImageMemoryBarrier(VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_GENERAL, *m_sparseTestData.sparseImage, subresourceRange);
            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_NONE, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr,
                                  0u, nullptr, 1u, &preBarrier);
            VkBufferImageCopy region = {
                0u,                         // VkDeviceSize				bufferOffset;
                0u,                         // uint32_t					bufferRowLength;
                0u,                         // uint32_t					bufferImageHeight;
                subresourceLayers,          // VkImageSubresourceLayers	imageSubresource;
                {0, 0, 0},                  // VkOffset3D				imageOffset;
                m_sparseTestData.imageSize, // VkExtent3D				imageExtent;
            };
            vk.cmdCopyBufferToImage(*cmdBuffer, **m_sparseTestData.srcBuffer, *m_sparseTestData.sparseImage,
                                    VK_IMAGE_LAYOUT_GENERAL, 1u, &region);
            const VkImageMemoryBarrier barrier = makeImageMemoryBarrier(
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_GENERAL, *m_sparseTestData.sparseImage, subresourceRange);
            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                                  nullptr, 0u, nullptr, 1u, &barrier);
            vk.cmdCopyImageToBuffer(*cmdBuffer, *m_sparseTestData.sparseImage, VK_IMAGE_LAYOUT_GENERAL,
                                    **m_sparseTestData.outputBuffer, 1u, &region);
        }
        endCommandBuffer(vk, *cmdBuffer);

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        VkSubmitInfo submitInfo = {
            VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType				sType;
            nullptr,                       // const void*					pNext;
            1u,                            // uint32_t					    waitSemaphoreCount;
            &*signalSemaphore,             // const VkSemaphore*			pWaitSemaphores;
            &waitStage,                    // const VkPipelineStageFlags*	pWaitDstStageMask;
            1u,                            // uint32_t					    commandBufferCount;
            &*cmdBuffer,                   // const VkCommandBuffer*		pCommandBuffers;
            0u,                            // uint32_t					    signalSemaphoreCount;
            nullptr,                       // const VkSemaphore*			pSignalSemaphores;
        };

        vk.queueSubmit(queue, 1u, &submitInfo, fences[1]);
        VK_CHECK(vk.waitForFences(device, 2u, fences, VK_TRUE, std::numeric_limits<uint64_t>::max()));

        uint32_t *output = (uint32_t *)(*m_sparseTestData.outputBuffer).getAllocation().getHostPtr();
        if (testBuffer)
        {
            for (uint32_t j = 0; j < m_sparseTestData.bufferSize; ++j)
            {
                if (output[j] != data)
                {
                    m_failed = true;
                    return;
                }
            }
        }
        else
        {
            for (uint32_t j = 0; j < m_sparseTestData.bufferSize; ++j)
            {
                if (output[j] != j % 255)
                {
                    m_failed = true;
                    return;
                }
            }
        }
    }

    void runPerformanceConfiguration()
    {
        const DeviceInterface &vk = m_vk;
        const VkDevice device     = m_device;
        const VkQueue queue       = m_queue;

        VkInitializePerformanceApiInfoINTEL performanceApiInfoIntel = {
            VK_STRUCTURE_TYPE_INITIALIZE_PERFORMANCE_API_INFO_INTEL, // VkStructureType	sType;
            nullptr,                                                 // const void*		pNext;
            nullptr,                                                 // void*		    pUserData;
        };

        vk.initializePerformanceApiINTEL(device, &performanceApiInfoIntel);
        const VkQueryPool queryPool = *m_performanceConfigurationTestData.queryPool;

        const Move<VkCommandBuffer> cmdBuffer(
            allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
        beginCommandBuffer(vk, *cmdBuffer);
        vk.cmdResetQueryPool(*cmdBuffer, queryPool, 0u, 1u);
        vk.cmdBeginQuery(*cmdBuffer, queryPool, 0u, 0u);
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
                              0u, nullptr, 0u, nullptr, 0u, nullptr);
        vk.cmdEndQuery(*cmdBuffer, queryPool, 0);
        endCommandBuffer(vk, *cmdBuffer);

        VkPerformanceConfigurationINTEL performanceConfiguration;
        VkPerformanceConfigurationAcquireInfoINTEL acqureInfo = {
            VK_STRUCTURE_TYPE_PERFORMANCE_CONFIGURATION_ACQUIRE_INFO_INTEL, // VkStructureType						sType;
            nullptr,                                                        // const void*							pNext;
            VK_PERFORMANCE_CONFIGURATION_TYPE_COMMAND_QUEUE_METRICS_DISCOVERY_ACTIVATED_INTEL, // VkPerformanceConfigurationTypeINTEL	type;
        };
        VK_CHECK(vk.acquirePerformanceConfigurationINTEL(device, &acqureInfo, &performanceConfiguration));
        VK_CHECK(vk.queueSetPerformanceConfigurationINTEL(queue, performanceConfiguration));
        VkSubmitInfo submitInfo = {
            VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType				sType;
            nullptr,                       // const void*					pNext;
            0u,                            // uint32_t					    waitSemaphoreCount;
            nullptr,                       // const VkSemaphore*			pWaitSemaphores;
            nullptr,                       // const VkPipelineStageFlags*	pWaitDstStageMask;
            1u,                            // uint32_t					    commandBufferCount;
            &*cmdBuffer,                   // const VkCommandBuffer*		pCommandBuffers;
            0u,                            // uint32_t					    signalSemaphoreCount;
            nullptr,                       // const VkSemaphore*			pSignalSemaphores;
        };

        vk.queueSubmit(queue, 1u, &submitInfo, VK_NULL_HANDLE);
        vk.queueWaitIdle(queue);
        VK_CHECK(vk.releasePerformanceConfigurationINTEL(device, performanceConfiguration));
    }

    void runDeviceWaitIdle()
    {
        const DeviceInterface &vk = m_vk;
        const VkDevice device     = m_device;
        const VkQueue queue       = m_queue;

        VkBufferCopy region = {
            0u,                                  // VkDeviceSize				srcOffset;
            0u,                                  // VkDeviceSize				dstOffset;
            m_deviceWaitIdleTestData.bufferSize, // VkDeviceSize				size;
        };

        const Move<VkCommandBuffer> cmdBuffer(
            allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
        beginCommandBuffer(vk, *cmdBuffer);
        vk.cmdCopyBuffer(*cmdBuffer, **m_deviceWaitIdleTestData.srcBuffer, **m_deviceWaitIdleTestData.dstBuffer, 1u,
                         &region);
        endCommandBuffer(vk, *cmdBuffer);
        VkSubmitInfo submitInfo = {
            VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType				sType;
            nullptr,                       // const void*					pNext;
            0u,                            // uint32_t					    waitSemaphoreCount;
            nullptr,                       // const VkSemaphore*			pWaitSemaphores;
            nullptr,                       // const VkPipelineStageFlags*	pWaitDstStageMask;
            1u,                            // uint32_t					    commandBufferCount;
            &*cmdBuffer,                   // const VkCommandBuffer*		pCommandBuffers;
            0u,                            // uint32_t					    signalSemaphoreCount;
            nullptr,                       // const VkSemaphore*			pSignalSemaphores;
        };
        vk.queueSubmit(queue, 1u, &submitInfo, VK_NULL_HANDLE);
        vk.deviceWaitIdle(device);
    }

    void runDraw()
    {
        const DeviceInterface &vk = m_vk;
        const VkDevice device     = m_device;
        const VkQueue queue       = m_queue;

        const VkRect2D renderArea = getRenderArea();

        const auto outputBufferSize = renderArea.extent.width * renderArea.extent.height * 4u;
        void *output                = m_drawTestData.outputBuffer->getAllocation().getHostPtr();
        memset(output, 0, outputBufferSize);

        if (m_testType == TYPE_OUT_OF_BAND)
        {
            VkOutOfBandQueueTypeInfoNV queueTypeInfo = {
                VK_STRUCTURE_TYPE_OUT_OF_BAND_QUEUE_TYPE_INFO_NV, // VkStructureType			sType;
                nullptr,                                          // const void*				pNext;
                VK_OUT_OF_BAND_QUEUE_TYPE_RENDER_NV,              // VkOutOfBandQueueTypeNV	queueType;
            };
            vk.queueNotifyOutOfBandNV(queue, &queueTypeInfo);
        }

        uint32_t imageIndex;
        Move<VkSemaphore> acquireSemaphore;
        Move<VkSemaphore> drawSemaphore;
        VkFramebuffer framebuffer;
        if (m_testType == TYPE_WSI)
        {
            acquireSemaphore = createSemaphore(vk, device);
            drawSemaphore    = createSemaphore(vk, device);
            vk.acquireNextImageKHR(device, *m_drawTestData.swapchain, std::numeric_limits<uint64_t>::max(),
                                   *acquireSemaphore, VK_NULL_HANDLE, &imageIndex);
            framebuffer = *m_drawTestData.swapchainFramebuffers[imageIndex];
        }
        else
        {
            framebuffer = *m_drawTestData.framebuffer;
        }

        const Move<VkCommandBuffer> cmdBuffer(
            allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
        beginCommandBuffer(vk, *cmdBuffer);
        if (m_testType == TYPE_DEBUG_UTILS)
        {
            VkDebugUtilsLabelEXT labelInfo = vk::initVulkanStructure();
            labelInfo.pLabelName           = "vkQueueBeginDebugUtilsLabelEXT";
            vk.queueBeginDebugUtilsLabelEXT(queue, &labelInfo);
        }
        beginRenderPass(vk, *cmdBuffer, *m_drawTestData.renderPass, framebuffer, renderArea);
        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_drawTestData.pipeline->getPipeline());
        vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);
        endRenderPass(vk, *cmdBuffer);

        if (m_testType != TYPE_WSI)
        {
            VkImageMemoryBarrier postImageBarrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,   // VkStructureType            sType
                nullptr,                                  // const void*                pNext
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,     // VkAccessFlags            srcAccessMask
                VK_ACCESS_TRANSFER_READ_BIT,              // VkAccessFlags            dstAccessMask
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout            oldLayout
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,     // VkImageLayout            newLayout
                VK_QUEUE_FAMILY_IGNORED,                  // uint32_t                    srcQueueFamilyIndex
                VK_QUEUE_FAMILY_IGNORED,                  // uint32_t                    dstQueueFamilyIndex
                **m_drawTestData.image,                   // VkImage                    image
                makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u,
                                          1u) // VkImageSubresourceRange    subresourceRange
            };
            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1,
                                  &postImageBarrier);

            VkExtent3D extent = {renderArea.extent.width, renderArea.extent.height, 1u};
            const VkBufferImageCopy copyRegion =
                makeBufferImageCopy(extent, makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
            vk.cmdCopyImageToBuffer(*cmdBuffer, **m_drawTestData.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    **m_drawTestData.outputBuffer, 1u, &copyRegion);
        }
        if (m_testType == TYPE_DEBUG_UTILS)
        {
            vk.queueEndDebugUtilsLabelEXT(queue);
        }
        endCommandBuffer(vk, *cmdBuffer);

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        const VkSubmitInfo submitInfo = {
            VK_STRUCTURE_TYPE_SUBMIT_INFO,    // VkStructureType sType;
            nullptr,                          // const void* pNext;
            m_testType == TYPE_WSI ? 1u : 0u, // uint32_t waitSemaphoreCount;
            &*acquireSemaphore,               // const VkSemaphore* pWaitSemaphores;
            &waitStage,                       // const VkPipelineStageFlags* pWaitDstStageMask;
            1u,                               // uint32_t commandBufferCount;
            &*cmdBuffer,                      // const VkCommandBuffer* pCommandBuffers;
            m_testType == TYPE_WSI ? 1u : 0u, // uint32_t signalSemaphoreCount;
            &*drawSemaphore,                  // const VkSemaphore* pSignalSemaphores;
        };

        const VkCommandBufferSubmitInfo commandBuffersSubmitInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, // VkStructureType	sType;
            nullptr,                                      // const void*		pNext;
            *cmdBuffer,                                   // VkCommandBuffer	commandBuffer;
            0u                                            // uint32_t		deviceMask;
        };

        VkSemaphoreSubmitInfo waitSemaphoreInfo = {
            VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, // VkStructureType			sType;
            nullptr,                                 // const void*				pNext;
            *acquireSemaphore,                       // VkSemaphore				semaphore;
            0u,                                      // uint64_t				value;
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,     // VkPipelineStageFlags2	stageMask;
            0u,                                      // uint32_t				deviceIndex;
        };

        VkSemaphoreSubmitInfo signalSemaphoreInfo = {
            VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,         // VkStructureType			sType;
            nullptr,                                         // const void*				pNext;
            *drawSemaphore,                                  // VkSemaphore				semaphore;
            0u,                                              // uint64_t				value;
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, // VkPipelineStageFlags2	stageMask;
            0u,                                              // uint32_t				deviceIndex;
        };

        const VkSubmitInfo2 submitInfo2 = {
            VK_STRUCTURE_TYPE_SUBMIT_INFO_2,  // VkStructureType					sType;
            nullptr,                          // const void*						pNext;
            0u,                               // VkSubmitFlags						flags;
            m_testType == TYPE_WSI ? 1u : 0u, // uint32_t							waitSemaphoreInfoCount;
            &waitSemaphoreInfo,               // const VkSemaphoreSubmitInfo*		pWaitSemaphoreInfos;
            1u,                               // uint32_t							commandBufferInfoCount;
            &commandBuffersSubmitInfo,        // const VkCommandBufferSubmitInfo*	pCommandBufferInfos;
            m_testType == TYPE_WSI ? 1u : 0u, // uint32_t							signalSemaphoreInfoCount;
            &signalSemaphoreInfo              // const VkSemaphoreSubmitInfo*		pSignalSemaphoreInfos;
        };

        if (m_testType == TYPE_SMALL_IMAGE_SYNC2 || m_testType == TYPE_LARGE_IMAGE_SYNC2)
            VK_CHECK(vk.queueSubmit2(queue, 1u, &submitInfo2, VK_NULL_HANDLE));
        else
            VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, VK_NULL_HANDLE));

        if (m_testType == TYPE_DEBUG_UTILS)
        {
            VkDebugUtilsLabelEXT labelInfo = vk::initVulkanStructure();
            labelInfo.pLabelName           = "vkQueueInsertDebugUtilsLabelEXT";
            vk.queueInsertDebugUtilsLabelEXT(queue, &labelInfo);
        }

        if (m_testType == TYPE_WSI)
        {
            VkPresentInfoKHR presentInfo = {
                VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, // VkStructureType			sType;
                nullptr,                            // const void*				pNext;
                1u,                                 // uint32_t				waitSemaphoreCount;
                &*drawSemaphore,                    // const VkSemaphore*		pWaitSemaphores;
                1u,                                 // uint32_t				swapchainCount;
                &*m_drawTestData.swapchain,         // const VkSwapchainKHR*	pSwapchains;
                &imageIndex,                        // const uint32_t*			pImageIndices;
                nullptr                             // VkResult*				pResults;
            };

            vk.queuePresentKHR(queue, &presentInfo);
            vk.queueWaitIdle(queue);
        }
        else
        {
            vk.queueWaitIdle(queue);
            tcu::ConstPixelBufferAccess resultBuffer = tcu::ConstPixelBufferAccess(
                mapVkFormat(m_drawTestData.colorFormat), renderArea.extent.width, renderArea.extent.height, 1,
                (const void *)m_drawTestData.outputBuffer->getAllocation().getHostPtr());

            const auto count = renderArea.extent.width * renderArea.extent.height;
            int decrement    = (m_testType == TYPE_LARGE_IMAGE_SYNC || m_testType == TYPE_LARGE_IMAGE_SYNC2) ?
                                   renderArea.extent.width / 2 - 1 :
                                   1;
            for (int i = count - 1; i >= 0; i -= decrement)
            {
                uint8_t r           = ((uint8_t *)resultBuffer.getDataPtr())[i * 4 + 0];
                uint8_t g           = ((uint8_t *)resultBuffer.getDataPtr())[i * 4 + 1];
                const uint8_t start = (uint8_t)(256u / renderArea.extent.width / 2u);
                const float range   = 256u / (float)renderArea.extent.width;
                if (!withinEpsilon(r, start + (uint8_t)(range * float(i)), 1) ||
                    !withinEpsilon(g, start + (uint8_t)(range * float(i / renderArea.extent.height)), 1))
                {
                    m_failed = true;
                    return;
                }
            }
        }
    }

    bool hasFailed() const
    {
        return m_failed;
    }

    VkRect2D getRenderArea()
    {
        if (m_testType == TYPE_LARGE_IMAGE_SYNC || m_testType == TYPE_LARGE_IMAGE_SYNC2)
        {
            return largeRenderArea;
        }
        return smallRenderArea;
    }
    VkExtent3D getImageExtent()
    {
        if (m_testType == TYPE_LARGE_IMAGE_SYNC || m_testType == TYPE_LARGE_IMAGE_SYNC2)
        {
            return largeImageExtent;
        }
        return smallImageExtent;
    }

private:
    const VkRect2D smallRenderArea    = makeRect2D(0, 0, 8u, 8u);
    const VkExtent3D smallImageExtent = {8u, 8u, 1u};
    const VkRect2D largeRenderArea    = makeRect2D(0, 0, 4096u, 4096u);
    const VkExtent3D largeImageExtent = {4096u, 4096u, 1u};

    struct SparseTestData
    {
        const uint32_t bufferSize  = 4096u;
        const VkExtent3D imageSize = {64u, 64u, 1u};

        Move<VkFence> fence1;
        Move<VkFence> fence2;
        de::MovePtr<BufferWithMemory> outputBuffer;
        de::MovePtr<BufferWithMemory> srcBuffer;

        Move<VkBuffer> sparseBuffer;
        Move<VkImage> sparseImage;
    } m_sparseTestData;

    struct DrawTestData
    {
        de::MovePtr<NativeObjects> nativeObjects;
        Move<VkSurfaceKHR> surface;
        Move<VkSwapchainKHR> swapchain;
        std::vector<VkImage> swapchainImages;
        std::vector<Move<VkImageView>> swapchainImageViews;
        std::vector<Move<VkFramebuffer>> swapchainFramebuffers;
        de::MovePtr<ImageWithMemory> image;
        Move<VkImageView> imageView;
        Move<VkRenderPass> renderPass;
        Move<VkFramebuffer> framebuffer;
        de::MovePtr<GraphicsPipelineWrapper> pipeline;
        de::MovePtr<BufferWithMemory> outputBuffer;
        VkFormat colorFormat;
    } m_drawTestData;

    struct PerformanceConfigurationTestData
    {
        Move<VkQueryPool> queryPool;
    } m_performanceConfigurationTestData;

    struct DeviceWaitIdleTestData
    {
        const VkDeviceSize bufferSize = 16 * 1024 * 1024;

        de::MovePtr<BufferWithMemory> srcBuffer;
        de::MovePtr<BufferWithMemory> dstBuffer;
    } m_deviceWaitIdleTestData;

    Move<VkCommandPool> m_cmdPool;

    const TestType m_testType;
    Context &m_context;
    vk::wsi::Type m_wsiType;
    DeviceDriver &m_vk;
    VkDevice m_device;
    SimpleAllocator m_allocator;
    uint32_t m_queueFamilyIndex;
    VkQueue m_queue;
    bool m_failed;
};

tcu::TestStatus InternallySynchronizedQueuesTestInstance::iterate(void)
{
    const vk::PlatformInterface &vkp      = m_context.getPlatformInterface();
    const InstanceInterface &vki          = m_context.getInstanceInterface();
    const VkInstance instance             = m_context.getInstance();
    const DeviceInterface &vk             = m_context.getDeviceInterface();
    const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();

    uint32_t queueFamilyCount = 1u;
    uint32_t queueCount       = 1u;

    QueueCreationType queueCreation = m_params.queueCreation;
    if (queueCreation == QUEUE_CREATION_FIRST_INTERN_SYNCED || queueCreation == QUEUE_CREATION_LAST_INTERN_SYNCED)
    {
        queueFamilyCount = 2u;
        if (m_params.sameQueueFamily)
            queueCount = 2u;
    }

    if (queueCreation == QUEUE_CREATION_TWO_INTERN_SYNCED_USE_FIRST ||
        queueCreation == QUEUE_CREATION_TWO_INTERN_SYNCED_USE_LAST)
        queueCount = 2u;

    uint32_t queueFamilyPropertyCount = 0;
    vki.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
    vki.getPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());

    uint32_t queueFamilyIndex       = queueFamilyPropertyCount;
    uint32_t otherQueueFamilyIndex  = queueFamilyPropertyCount;
    VkQueueFlags requiredQueueFlags = VK_QUEUE_GRAPHICS_BIT;
    if (m_params.hasSparseTest())
        requiredQueueFlags |= VK_QUEUE_SPARSE_BINDING_BIT;

    for (uint32_t i = 0; i < queueFamilyPropertyCount; ++i)
    {
        if ((queueFamilyProperties[i].queueFlags & requiredQueueFlags) == requiredQueueFlags &&
            queueFamilyIndex == queueFamilyPropertyCount && queueFamilyProperties[i].queueCount >= queueCount)
            queueFamilyIndex = i;
        else if (otherQueueFamilyIndex == queueFamilyPropertyCount)
            otherQueueFamilyIndex = i;
    }
    if (queueFamilyIndex == queueFamilyPropertyCount)
    {
        // Fallback to single queue, if 2 queues are not supported
        queueCreation    = QUEUE_CREATION_SINGLE_QUEUE;
        queueFamilyCount = 1u;
        queueCount       = 1u;
        for (uint32_t i = 0; i < queueFamilyPropertyCount; ++i)
        {
            if ((queueFamilyProperties[i].queueFlags & requiredQueueFlags) != 0 &&
                queueFamilyIndex == queueFamilyPropertyCount)
                queueFamilyIndex = i;
            else if (otherQueueFamilyIndex == queueFamilyPropertyCount)
                otherQueueFamilyIndex = i;
        }
    }

    if (queueFamilyIndex == queueFamilyPropertyCount)
        TCU_THROW(NotSupportedError, "No queue family found which supported required queue flags");

    if (otherQueueFamilyIndex == queueFamilyPropertyCount)
    {
        queueCreation    = QUEUE_CREATION_SINGLE_QUEUE;
        queueFamilyCount = 1u;
        queueCount       = 1u;
    }

    if (m_params.sameQueueFamily)
        otherQueueFamilyIndex = queueFamilyIndex;

    const float priorities[2] = {1.0f, 1.0f};

    const bool useSecondQueueFamilyIndex = queueCreation == QUEUE_CREATION_LAST_INTERN_SYNCED;
    const bool useSecondQueue            = queueCreation == QUEUE_CREATION_TWO_INTERN_SYNCED_USE_LAST;

    VkDeviceQueueCreateInfo deviceQueueCreateInfos[2];
    deviceQueueCreateInfos[0] = initVulkanStructure();
    if (queueCreation == QUEUE_CREATION_SINGLE_QUEUE || queueCreation == QUEUE_CREATION_FIRST_INTERN_SYNCED ||
        queueCreation == QUEUE_CREATION_TWO_INTERN_SYNCED_USE_FIRST ||
        queueCreation == QUEUE_CREATION_TWO_INTERN_SYNCED_USE_LAST)
        deviceQueueCreateInfos[0].flags |= VK_DEVICE_QUEUE_CREATE_INTERNALLY_SYNCHRONIZED_BIT_KHR;
    deviceQueueCreateInfos[0].queueFamilyIndex = useSecondQueueFamilyIndex ? otherQueueFamilyIndex : queueFamilyIndex;
    deviceQueueCreateInfos[0].queueCount       = queueCount;
    deviceQueueCreateInfos[0].pQueuePriorities = priorities;

    deviceQueueCreateInfos[1] = initVulkanStructure();
    if (queueCreation == QUEUE_CREATION_LAST_INTERN_SYNCED ||
        queueCreation == QUEUE_CREATION_TWO_INTERN_SYNCED_USE_FIRST ||
        queueCreation == QUEUE_CREATION_TWO_INTERN_SYNCED_USE_LAST)
        deviceQueueCreateInfos[1].flags |= VK_DEVICE_QUEUE_CREATE_INTERNALLY_SYNCHRONIZED_BIT_KHR;
    deviceQueueCreateInfos[1].queueFamilyIndex = useSecondQueueFamilyIndex ? queueFamilyIndex : otherQueueFamilyIndex;
    deviceQueueCreateInfos[1].queueCount       = 1u;
    deviceQueueCreateInfos[1].pQueuePriorities = priorities;

    std::vector<const char *> extensions = {"VK_KHR_internally_synchronized_queues", "VK_KHR_synchronization2"};
    if (m_params.hasWsiTest())
        extensions.push_back("VK_KHR_swapchain");
    if (m_params.hasPerformanceConfigurationTest())
        extensions.push_back("VK_INTEL_performance_query");
    if (m_params.hasOutOfBandTest())
        extensions.push_back("VK_NV_low_latency2");

    VkPhysicalDeviceSynchronization2Features sync2Features = initVulkanStructure();
    sync2Features.synchronization2                         = VK_TRUE;

    VkPhysicalDeviceInternallySynchronizedQueuesFeaturesKHR isqFeatures = initVulkanStructure(&sync2Features);
    isqFeatures.internallySynchronizedQueues                            = VK_TRUE;

    VkPhysicalDeviceFeatures2 features2 = initVulkanStructure(&isqFeatures);
    if (m_params.hasSparseTest())
    {
        features2.features.sparseBinding          = VK_TRUE;
        features2.features.sparseResidencyImage2D = VK_TRUE;
    }

    const VkDeviceCreateInfo deviceInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, // VkStructureType sType;
        &features2,                           // const void* pNext;
        0u,                                   // VkDeviceCreateFlags flags;
        queueFamilyCount,                     // uint32_t queueCreateInfoCount;
        deviceQueueCreateInfos,               // const VkDeviceQueueCreateInfo* pQueueCreateInfos;
        0u,                                   // uint32_t enabledLayerCount;
        nullptr,                              // const char* const* ppEnabledLayerNames;
        (uint32_t)extensions.size(),          // uint32_t enabledExtensionCount;
        extensions.data(),                    // const char* const* ppEnabledExtensionNames;
        nullptr                               // const VkPhysicalDeviceFeatures* pEnabledFeatures;
    };

    Move<VkDevice> device = createCustomDevice(
        m_context.getTestContext().getCommandLine().isValidationEnabled(), m_context.getPlatformInterface(),
        m_context.getInstance(), m_context.getInstanceInterface(), m_context.getPhysicalDevice(), &deviceInfo);

    DeviceDriver vkd(vkp, instance, *device, m_context.getUsedApiVersion(),
                     m_context.getTestContext().getCommandLine());

    const uint32_t queueIndex    = useSecondQueue ? 1u : 0u;
    VkDeviceQueueInfo2 queueInfo = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,                  // VkStructureType			sType;
        nullptr,                                                // const void*				pNext;
        VK_DEVICE_QUEUE_CREATE_INTERNALLY_SYNCHRONIZED_BIT_KHR, // VkDeviceQueueCreateFlags	flags;
        queueFamilyIndex,                                       // uint32_t					queueFamilyIndex;
        queueIndex,                                             // uint32_t					queueIndex;
    };

    VkQueue queue;
    vk.getDeviceQueue2(*device, &queueInfo, &queue);

    std::vector<de::SharedPtr<de::Thread>> threads;
    threads.push_back(de::SharedPtr<CaseThread>(
        new CaseThread(m_params.testType1, m_context, m_params.wsiType, vkd, *device, queueFamilyIndex, queue)));
    threads.push_back(de::SharedPtr<CaseThread>(
        new CaseThread(m_params.testType2, m_context, m_params.wsiType, vkd, *device, queueFamilyIndex, queue)));
    threads.push_back(de::SharedPtr<CaseThread>(
        new CaseThread(m_params.testType3, m_context, m_params.wsiType, vkd, *device, queueFamilyIndex, queue)));
    threads.push_back(de::SharedPtr<CaseThread>(
        new CaseThread(m_params.testType4, m_context, m_params.wsiType, vkd, *device, queueFamilyIndex, queue)));

    for (auto &thread : threads)
        thread->start();

    for (auto &thread : threads)
        thread->join();

    return tcu::TestStatus::pass("Pass");
}

class InternallySynchronizedQueuesTestCase : public vkt::TestCase
{
public:
    InternallySynchronizedQueuesTestCase(tcu::TestContext &context, const char *name, const TestParameters &params)
        : TestCase(context, name)
        , m_params(params)
    {
    }

private:
    void checkSupport(vkt::Context &context) const;
    void initPrograms(SourceCollections &programCollection) const;
    vkt::TestInstance *createInstance(vkt::Context &context) const
    {
        return new InternallySynchronizedQueuesTestInstance(context, m_params);
    }

    const TestParameters m_params;
};

void InternallySynchronizedQueuesTestCase::checkSupport(vkt::Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_synchronization2");
    if (m_params.hasWsiTest())
    {
        context.requireInstanceFunctionality(wsi::getExtensionName(m_params.wsiType));
        context.requireDeviceFunctionality("VK_KHR_swapchain");
    }

    if (m_params.hasDebugUtilsTest())
        context.requireInstanceFunctionality("VK_EXT_debug_utils");

    if (m_params.hasSparseTest())
    {
        context.requireDeviceCoreFeature(DeviceCoreFeature::DEVICE_CORE_FEATURE_SPARSE_BINDING);
        context.requireDeviceCoreFeature(DeviceCoreFeature::DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_IMAGE2D);
    }
    if (m_params.hasPerformanceConfigurationTest())
        context.requireDeviceFunctionality("VK_INTEL_performance_query");
    if (m_params.hasOutOfBandTest())
        context.requireDeviceFunctionality("VK_NV_low_latency2");

    context.requireDeviceFunctionality("VK_KHR_internally_synchronized_queues");
}

void InternallySynchronizedQueuesTestCase::initPrograms(SourceCollections &programCollection) const
{
    std::ostringstream vert;
    vert << "#version 450\n"
         << "layout (location=0) out vec2 texCoord;\n"
         << "void main()\n"
         << "{\n"
         << "    texCoord = vec2(gl_VertexIndex & 1u, (gl_VertexIndex >> 1u) & 1u);"
         << "    gl_Position = vec4(texCoord * 2.0f - 1.0f, 0.0f, 1.0f);\n"
         << "}\n";

    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 450\n"
         << "layout (location=0) out vec4 out_color;\n"
         << "layout (location=0) in vec2 texCoord;\n"
         << "void main()\n"
         << "{\n"
         << "    out_color = vec4(texCoord, 0.0f, 1.0f);\n"
         << "}\n";

    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

tcu::TestCaseGroup *createInternallySynchronizedTests(tcu::TestContext &testCtx, SynchronizationType)
{
    de::MovePtr<tcu::TestCaseGroup> internallySynchronizedTests(
        new tcu::TestCaseGroup(testCtx, "internally_synchronized_queues"));

    struct TestParameters params;
    params.testType1 = TYPE_SMALL_IMAGE_SYNC;
    params.testType2 = TYPE_LARGE_IMAGE_SYNC;

    std::vector<TestType> tests = {
        TYPE_SMALL_IMAGE_SYNC2, TYPE_LARGE_IMAGE_SYNC2,         TYPE_QUEUE_BIND_SPARSE, TYPE_WSI,
        TYPE_DEBUG_UTILS,       TYPE_PERFORMANCE_CONFIGURATION, TYPE_OUT_OF_BAND,       TYPE_DEVICE_WAIT_IDLE,
    };

    std::vector<QueueCreationType> queueCreation = {
        QUEUE_CREATION_SINGLE_QUEUE,
        QUEUE_CREATION_FIRST_INTERN_SYNCED,
        QUEUE_CREATION_LAST_INTERN_SYNCED,
        QUEUE_CREATION_TWO_INTERN_SYNCED_USE_FIRST,
        QUEUE_CREATION_TWO_INTERN_SYNCED_USE_LAST,
    };

    for (size_t i = 0; i < tests.size(); ++i)
    {
        params.sameQueueFamily = i % 2 == 0;
        for (size_t j = 0; j < tests.size(); ++j)
        {
            params.testType3     = tests[i];
            params.testType4     = tests[j];
            params.queueCreation = queueCreation[(i + j) % queueCreation.size()];

            std::string testName = getTestName(params.testType3) + "_" + getTestName(params.testType4);

            if (params.testType1 == TYPE_WSI || params.testType2 == TYPE_WSI || params.testType3 == TYPE_WSI ||
                params.testType4 == TYPE_WSI)
            {
                for (int typeNdx = 0; typeNdx < vk::wsi::TYPE_LAST; ++typeNdx)
                {
                    // Multiple concurrent wsi windows are currently not supported on android
                    // dEQP currently has only one SurfaceView, but multiple SurfaceViews in the activity's view hierarchy
                    // would be required, and attach Vulkan WSI objects to each of them
                    if (i == j && typeNdx == vk::wsi::Type::TYPE_ANDROID)
                        continue;

                    params.wsiType = (vk::wsi::Type)typeNdx;

                    std::string name = std::string(getName(params.wsiType)) + "_" + testName;
                    internallySynchronizedTests->addChild(
                        new InternallySynchronizedQueuesTestCase(testCtx, name.c_str(), params));
                }
            }
            else
            {
                internallySynchronizedTests->addChild(
                    new InternallySynchronizedQueuesTestCase(testCtx, testName.c_str(), params));
            }
        }
    }

    return internallySynchronizedTests.release();
}

} // namespace synchronization
} // namespace vkt
