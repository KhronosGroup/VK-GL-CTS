/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017-2020 The Khronos Group Inc.
 * Copyright (c) 2020 AMD
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
 * \brief Tests for VK_KHR_fragment_shading_rate
 *//*--------------------------------------------------------------------*/

#include "vktFragmentShadingRatePixelConsistency.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkPlatform.hpp"

#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkSafetyCriticalUtil.hpp"
#include "vkAppParamsUtil.hpp"

#include "deDefs.h"
#include "deMath.h"
#include "deRandom.h"
#include "deSharedPtr.hpp"
#include "deString.h"

#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"

#include <limits>
#include <string>
#include <sstream>

namespace vkt
{
namespace FragmentShadingRate
{
namespace
{
using namespace vk;
using namespace std;

struct CaseDef
{
    VkExtent2D shadingRate;
    VkSampleCountFlagBits samples;
    VkExtent2D framebufferExtent;
    bool zwCoord;
};

struct Vertex
{
    float x;
    float y;
};

Vertex basicTriangles[6] = {
    {-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f},

    {-1.0f, 1.0f},  {1.0f, -1.0f}, {1.0f, 1.0f},
};

Move<VkDevice> createImageRobustnessDevice(Context &context, const vk::VkInstance &instance,
                                           const InstanceInterface &vki)
{
    const VkPhysicalDevice physicalDevice = chooseDevice(vki, instance, context.getTestContext().getCommandLine());
    const float queuePriority             = 1.0f;

    // Create a universal queue
    const VkDeviceQueueCreateInfo queueParams = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                    // const void* pNext;
        0u,                                         // VkDeviceQueueCreateFlags flags;
        context.getUniversalQueueFamilyIndex(),     // uint32_t queueFamilyIndex;
        1u,                                         // uint32_t queueCount;
        &queuePriority                              // const float* pQueuePriorities;
    };

    // Add image robustness extension if supported
    std::vector<const char *> deviceExtensions;

    deviceExtensions.push_back("VK_KHR_fragment_shading_rate");

    if (context.isDeviceFunctionalitySupported("VK_EXT_image_robustness"))
    {
        deviceExtensions.push_back("VK_EXT_image_robustness");
    }

    VkPhysicalDeviceFragmentShadingRateFeaturesKHR fsrFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR, // VkStructureType sType;
        nullptr,                                                              // void* pNext;
        false,                                                                // VkBool32 pipelineFragmentShadingRate;
        false,                                                                // VkBool32 primitiveFragmentShadingRate;
        false,                                                                // VkBool32 attachmentFragmentShadingRate;
    };

    VkPhysicalDeviceFeatures2 enabledFeatures;
    enabledFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    enabledFeatures.pNext = &fsrFeatures;

    vki.getPhysicalDeviceFeatures2(physicalDevice, &enabledFeatures);

    VkDeviceCreateInfo deviceParams = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,                      // VkStructureType sType;
        &enabledFeatures,                                          // const void* pNext;
        0u,                                                        // VkDeviceCreateFlags flags;
        1u,                                                        // uint32_t queueCreateInfoCount;
        &queueParams,                                              // const VkDeviceQueueCreateInfo* pQueueCreateInfos;
        0u,                                                        // uint32_t enabledLayerCount;
        nullptr,                                                   // const char* const* ppEnabledLayerNames;
        static_cast<uint32_t>(deviceExtensions.size()),            // uint32_t enabledExtensionCount;
        deviceExtensions.empty() ? nullptr : &deviceExtensions[0], // const char* const* ppEnabledExtensionNames;
        nullptr,                                                   // const VkPhysicalDeviceFeatures* pEnabledFeatures;
    };

#ifdef CTS_USES_VULKANSC
    // devices created for Vulkan SC must have VkDeviceObjectReservationCreateInfo structure defined in VkDeviceCreateInfo::pNext chain
    VkDeviceObjectReservationCreateInfo dmrCI = resetDeviceObjectReservationCreateInfo();
    VkPipelineCacheCreateInfo pcCI            = {
        VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                      // const void* pNext;
        VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
            VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT, // VkPipelineCacheCreateFlags flags;
        0U,                                                       // uintptr_t initialDataSize;
        nullptr                                                   // const void* pInitialData;
    };

    const tcu::CommandLine &cmdLine                    = context.getTestContext().getCommandLine();
    de::SharedPtr<ResourceInterface> resourceInterface = context.getResourceInterface();

    std::vector<VkPipelinePoolSize> poolSizes;
    if (cmdLine.isSubProcess())
    {
        resourceInterface->importPipelineCacheData(context.getPlatformInterface(), instance, vki, physicalDevice,
                                                   context.getUniversalQueueFamilyIndex());
        dmrCI = resourceInterface->getStatMax();

        if (resourceInterface->getCacheDataSize() > 0)
        {
            pcCI.initialDataSize               = resourceInterface->getCacheDataSize();
            pcCI.pInitialData                  = resourceInterface->getCacheData();
            dmrCI.pipelineCacheCreateInfoCount = 1;
            dmrCI.pPipelineCacheCreateInfos    = &pcCI;
        }

        poolSizes = resourceInterface->getPipelinePoolSizes();
        if (!poolSizes.empty())
        {
            dmrCI.pipelinePoolSizeCount = uint32_t(poolSizes.size());
            dmrCI.pPipelinePoolSizes    = poolSizes.data();
        }
    }

    dmrCI.pNext                                     = deviceParams.pNext;
    VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
    if (findStructureInChain(dmrCI.pNext, getStructureType<VkPhysicalDeviceVulkanSC10Features>()) == nullptr)
    {
        sc10Features.pNext = &dmrCI;
        deviceParams.pNext = &sc10Features;
    }
    else
        deviceParams.pNext = &dmrCI;

    vector<VkApplicationParametersEXT> appParams;
    if (readApplicationParameters(appParams, cmdLine, false))
    {
        appParams[appParams.size() - 1].pNext = deviceParams.pNext;
        deviceParams.pNext                    = &appParams[0];
    }

    VkFaultCallbackInfo faultCallbackInfo = {
        VK_STRUCTURE_TYPE_FAULT_CALLBACK_INFO, // VkStructureType sType;
        nullptr,                               // void* pNext;
        0U,                                    // uint32_t faultCount;
        nullptr,                               // VkFaultData* pFaults;
        Context::faultCallbackFunction         // PFN_vkFaultCallbackFunction pfnFaultCallback;
    };

    if (cmdLine.isSubProcess())
    {
        // XXX workaround incorrect constness on faultCallbackInfo.pNext.
        faultCallbackInfo.pNext = const_cast<void *>(deviceParams.pNext);
        deviceParams.pNext      = &faultCallbackInfo;
    }
#endif // CTS_USES_VULKANSC

    return createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(),
                              context.getPlatformInterface(), instance, vki, context.getPhysicalDevice(),
                              &deviceParams);
}

class FSRPixelConsistencyInstance : public TestInstance
{
public:
    FSRPixelConsistencyInstance(Context &context, const CaseDef &data);
    ~FSRPixelConsistencyInstance(void);
    tcu::TestStatus iterate(void);

private:
    void clampShadingRate();
    tcu::TestStatus verifyResult(tcu::ConstPixelBufferAccess &resultBuffer, const uint32_t index);

    CaseDef m_data;
    vector<VkExtent2D> m_shadingRateClamped;

    uint32_t m_supportedFragmentShadingRateCount;
    vector<VkPhysicalDeviceFragmentShadingRateKHR> m_supportedFragmentShadingRates;
};

FSRPixelConsistencyInstance::FSRPixelConsistencyInstance(Context &context, const CaseDef &data)
    : vkt::TestInstance(context)
    , m_data(data)
    , m_supportedFragmentShadingRateCount(0)
{
    // Fetch information about supported fragment shading rates
    context.getInstanceInterface().getPhysicalDeviceFragmentShadingRatesKHR(
        context.getPhysicalDevice(), &m_supportedFragmentShadingRateCount, nullptr);

    m_supportedFragmentShadingRates.resize(m_supportedFragmentShadingRateCount);
    for (uint32_t i = 0; i < m_supportedFragmentShadingRateCount; ++i)
    {
        m_supportedFragmentShadingRates[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR;
        m_supportedFragmentShadingRates[i].pNext = nullptr;
    }
    context.getInstanceInterface().getPhysicalDeviceFragmentShadingRatesKHR(
        context.getPhysicalDevice(), &m_supportedFragmentShadingRateCount, &m_supportedFragmentShadingRates[0]);

    clampShadingRate();
}

FSRPixelConsistencyInstance::~FSRPixelConsistencyInstance(void)
{
}

class FSRPixelConsistencyTestCase : public TestCase
{
public:
    FSRPixelConsistencyTestCase(tcu::TestContext &context, const char *name, const CaseDef data);
    ~FSRPixelConsistencyTestCase(void);
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    CaseDef m_data;
};

FSRPixelConsistencyTestCase::FSRPixelConsistencyTestCase(tcu::TestContext &context, const char *name,
                                                         const CaseDef data)
    : vkt::TestCase(context, name)
    , m_data(data)
{
}

FSRPixelConsistencyTestCase::~FSRPixelConsistencyTestCase(void)
{
}

void FSRPixelConsistencyTestCase::checkSupport(Context &context) const
{
    const VkImageUsageFlags cbUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    context.requireDeviceFunctionality("VK_KHR_fragment_shading_rate");

    if (!context.getFragmentShadingRateFeatures().pipelineFragmentShadingRate)
        TCU_THROW(NotSupportedError, "pipelineFragmentShadingRate not supported");

    VkImageFormatProperties imageProperties;
    VkResult result = context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
        context.getPhysicalDevice(), VK_FORMAT_R32G32_UINT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, cbUsage, 0,
        &imageProperties);

    if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
        TCU_THROW(NotSupportedError, "VK_FORMAT_R32G32_UINT not supported");

    if (!(imageProperties.sampleCounts & m_data.samples))
        TCU_THROW(NotSupportedError, "Image sample count not supported");

    if ((imageProperties.maxExtent.width < m_data.framebufferExtent.width) ||
        (imageProperties.maxExtent.height < m_data.framebufferExtent.height))
        TCU_THROW(NotSupportedError, "Image max extents are smaller than required");
}

void FSRPixelConsistencyTestCase::initPrograms(SourceCollections &programCollection) const
{
    std::stringstream vss;

    vss << "#version 450 core\n"
           "layout(location = 0) in vec2 position;\n"
           "out gl_PerVertex\n"
           "{\n"
           "   vec4 gl_Position;\n"
           "};\n"
           "void main()\n"
           "{\n";
    if (!m_data.zwCoord)
    {
        vss << "  gl_Position = vec4(position, 0, 1);\n";
    }
    else
    {
        vss << "  gl_Position = vec4(position, position);\n";
    }
    vss << "}\n";

    programCollection.glslSources.add("vert") << glu::VertexSource(vss.str());

    std::stringstream fssPass0;

    fssPass0 << "#version 450 core\n"
                "layout(push_constant) uniform PC {\n"
                "    uvec2 shadingRate[2];\n"
                "} pc;\n"
                "layout(location = 0) out uvec2 col0;\n"
                "void main()\n"
                "{\n";
    if (!m_data.zwCoord)
    {
        fssPass0 << "  col0.x = (uint(gl_FragCoord.x) % pc.shadingRate[0].x) + ((uint(gl_FragCoord.y) % "
                    "pc.shadingRate[0].y) * pc.shadingRate[0].x);\n"
                    "  col0.y = (uint(gl_FragCoord.x) % pc.shadingRate[1].x) + ((uint(gl_FragCoord.y) % "
                    "pc.shadingRate[1].y) * pc.shadingRate[1].x);\n";
    }
    else
    {
        fssPass0 << "  col0.x = (uint(gl_FragCoord.z) % pc.shadingRate[0].x) + ((uint(gl_FragCoord.w) % "
                    "pc.shadingRate[0].y) * pc.shadingRate[0].x);\n"
                    "  col0.y = (uint(gl_FragCoord.z) % pc.shadingRate[1].x) + ((uint(gl_FragCoord.w) % "
                    "pc.shadingRate[1].y) * pc.shadingRate[1].x);\n";
    }
    fssPass0 << "}\n";

    programCollection.glslSources.add("frag_pass0") << glu::FragmentSource(fssPass0.str());

    std::stringstream fssPass1;

    fssPass1 << "#version 450 core\n";

    if (m_data.samples == VK_SAMPLE_COUNT_1_BIT)
    {
        fssPass1 << "layout(input_attachment_index=0, set=0, binding=0) uniform usubpassInput inputAttachment;\n";
    }
    else
    {
        fssPass1 << "layout(input_attachment_index=0, set=0, binding=0) uniform usubpassInputMS inputAttachment;\n";
    }

    fssPass1 << "layout(location = 0) out uvec2 col0;\n"
                "void main()\n"
                "{\n";

    if (m_data.samples == VK_SAMPLE_COUNT_1_BIT)
    {
        fssPass1 << "  col0 = subpassLoad(inputAttachment).xy;\n";
    }
    else
    {
        fssPass1 << "  col0 = subpassLoad(inputAttachment, 0).xy;\n";
    }

    fssPass1 << "}\n";

    programCollection.glslSources.add("frag_pass1") << glu::FragmentSource(fssPass1.str());
}

TestInstance *FSRPixelConsistencyTestCase::createInstance(Context &context) const
{
    return new FSRPixelConsistencyInstance(context, m_data);
}

bool compareShadingRate(VkExtent2D ext1, VkExtent2D ext2)
{
    uint32_t ratio1 = std::max(ext1.width, ext1.height) / std::min(ext1.width, ext1.height);
    uint32_t ratio2 = std::max(ext2.width, ext2.height) / std::min(ext2.width, ext2.height);

    return ratio1 < ratio2;
}

void FSRPixelConsistencyInstance::clampShadingRate()
{
    uint32_t desiredSize = m_data.shadingRate.width * m_data.shadingRate.height;

    while (desiredSize > 0)
    {
        // Find modes that maximize the area
        for (uint32_t i = 0; i < m_supportedFragmentShadingRateCount; ++i)
        {
            const VkPhysicalDeviceFragmentShadingRateKHR &supportedRate = m_supportedFragmentShadingRates[i];

            if (supportedRate.sampleCounts & VK_SAMPLE_COUNT_1_BIT)
            {
                // We found exact match
                if (supportedRate.fragmentSize.width == m_data.shadingRate.width &&
                    supportedRate.fragmentSize.height == m_data.shadingRate.height)
                {
                    m_shadingRateClamped.push_back(supportedRate.fragmentSize);

                    return;
                }
                else
                {
                    if (supportedRate.fragmentSize.width <= m_data.shadingRate.width &&
                        supportedRate.fragmentSize.height <= m_data.shadingRate.height &&
                        supportedRate.fragmentSize.width * supportedRate.fragmentSize.height == desiredSize)
                    {
                        m_shadingRateClamped.push_back(supportedRate.fragmentSize);
                    }
                }
            }
        }
        if (!m_shadingRateClamped.empty())
        {
            // Sort the modes so that the ones with the smallest aspect ratio are in front
            std::sort(m_shadingRateClamped.begin(), m_shadingRateClamped.end(), compareShadingRate);

            uint32_t desiredRatio = std::max(m_shadingRateClamped[0].width, m_shadingRateClamped[0].height) /
                                    std::min(m_shadingRateClamped[0].width, m_shadingRateClamped[0].height);

            // Leave only entries with the smallest aspect ratio
            auto it = m_shadingRateClamped.begin();
            while (it != m_shadingRateClamped.end())
            {
                uint32_t ratio = std::max(it->width, it->height) / std::min(it->width, it->height);

                if (ratio < desiredRatio)
                {
                    it = m_shadingRateClamped.erase(it, m_shadingRateClamped.end());
                }
                else
                {
                    ++it;
                }
            }

            return;
        }
        else
        {
            desiredSize /= 2;
        }
    }
    DE_ASSERT(0);

    return;
}

tcu::TestStatus FSRPixelConsistencyInstance::verifyResult(tcu::ConstPixelBufferAccess &resultBuffer,
                                                          const uint32_t index)
{
    uint32_t pixelIndex        = std::numeric_limits<unsigned int>::max();
    uint32_t pixelOutsideIndex = std::numeric_limits<unsigned int>::max();

    for (int y = 0; y < resultBuffer.getHeight(); y++)
    {
        for (int x = 0; x < resultBuffer.getWidth(); x++)
        {
            uint32_t pixel = resultBuffer.getPixelUint(x, y)[index];

            // If pixel was not covered by any triangle, we skip it
            if (pixel == std::numeric_limits<unsigned int>::max())
            {
                continue;
            }

            // We check if pixel is part of fragment area that is partially outside of framebuffer area
            bool outsideW = (x / m_shadingRateClamped[index].width + 1) * m_shadingRateClamped[index].width >
                            static_cast<uint32_t>(resultBuffer.getWidth());
            bool outsideH = (y / m_shadingRateClamped[index].height + 1) * m_shadingRateClamped[index].height >
                            static_cast<uint32_t>(resultBuffer.getHeight());

            if (outsideW || outsideH)
            {
                // If image robustness is enabled such pixel can have either a value of 0 or one of the values from the area inside framebuffer
                if (m_context.isDeviceFunctionalitySupported("VK_EXT_image_robustness"))
                {
                    if (pixelOutsideIndex == std::numeric_limits<unsigned int>::max() || pixelOutsideIndex == 0)
                    {
                        pixelOutsideIndex = pixel;
                    }
                    // If value is non-zero we make sure that all 'corner' pixels have the same value
                    else if ((pixel != 0) && (pixelOutsideIndex != pixel))
                    {
                        return tcu::TestStatus(QP_TEST_RESULT_FAIL, qpGetTestResultName(QP_TEST_RESULT_FAIL));
                    }
                }
                // If image robustness is not enabled such pixel can have an undefined value, so we skip it
                else
                {
                    continue;
                }
            }
            else
            {
                if (pixelIndex == std::numeric_limits<unsigned int>::max())
                {
                    if (pixel >= m_shadingRateClamped[index].width * m_shadingRateClamped[index].height)
                    {
                        return tcu::TestStatus(QP_TEST_RESULT_FAIL, qpGetTestResultName(QP_TEST_RESULT_FAIL));
                    }

                    pixelIndex = pixel;
                }
                // If pixel is not part of 'corner' pixels we make sure that is has the same value as other non-'corner' pixels
                else if (pixelIndex != pixel)
                {
                    return tcu::TestStatus(QP_TEST_RESULT_FAIL, qpGetTestResultName(QP_TEST_RESULT_FAIL));
                }
            }
        }
    }

    return tcu::TestStatus(QP_TEST_RESULT_PASS, qpGetTestResultName(QP_TEST_RESULT_PASS));
}

tcu::TestStatus FSRPixelConsistencyInstance::iterate(void)
{
    const VkPhysicalDeviceMemoryProperties memoryProperties =
        vk::getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice());

    const VkInstance instance  = m_context.getInstance();
    const auto &instanceDriver = m_context.getInstanceInterface();

    Move<VkDevice> vkd    = createImageRobustnessDevice(m_context, instance, instanceDriver);
    const VkDevice device = *vkd;
#ifndef CTS_USES_VULKANSC
    de::MovePtr<vk::DeviceDriver> deviceDriver = de::MovePtr<DeviceDriver>(
        new DeviceDriver(m_context.getPlatformInterface(), m_context.getInstance(), device,
                         m_context.getUsedApiVersion(), m_context.getTestContext().getCommandLine()));
#else
    de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter> deviceDriver =
        de::MovePtr<DeviceDriverSC, DeinitDeviceDeleter>(
            new DeviceDriverSC(m_context.getPlatformInterface(), m_context.getInstance(), device,
                               m_context.getTestContext().getCommandLine(), m_context.getResourceInterface(),
                               m_context.getDeviceVulkanSC10Properties(), m_context.getDeviceProperties(),
                               m_context.getUsedApiVersion()),
            vk::DeinitDeviceDeleter(m_context.getResourceInterface().get(), device));
#endif // CTS_USES_VULKANSC
    const DeviceInterface &vk        = *deviceDriver;
    const VkQueue queue              = getDeviceQueue(vk, device, m_context.getUniversalQueueFamilyIndex(), 0);
    de::MovePtr<Allocator> allocator = de::MovePtr<Allocator>(new SimpleAllocator(vk, device, memoryProperties));

    // Create vertex buffer
    const VkDeviceSize vertexBufferSize = sizeof(basicTriangles);

    const VkFormat imageFormat = VK_FORMAT_R32G32_UINT;

    de::MovePtr<BufferWithMemory> vertexBuffer;
    vertexBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, device, *allocator, makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
        MemoryRequirement::HostVisible));

    float *vbuf = (float *)vertexBuffer->getAllocation().getHostPtr();

    deMemcpy(vbuf, basicTriangles, vertexBufferSize);

    flushAlloc(vk, device, vertexBuffer->getAllocation());

    // Create color output buffer
    const VkDeviceSize colorOutputBufferSize =
        m_data.framebufferExtent.width * m_data.framebufferExtent.height * tcu::getPixelSize(mapVkFormat(imageFormat));

    de::MovePtr<BufferWithMemory> colorOutputBuffer;
    colorOutputBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, device, *allocator, makeBufferCreateInfo(colorOutputBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        MemoryRequirement::HostVisible));

    // Create color attachment for subpass 0
    de::MovePtr<ImageWithMemory> cbImagePass0;
    Move<VkImageView> cbImagePass0View;
    {
        const VkImageUsageFlags cbUsage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        const VkImageCreateInfo imageCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                             // const void* pNext;
            (VkImageCreateFlags)0u,              // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
            imageFormat,                         // VkFormat format;
            {
                m_data.framebufferExtent.width,  // uint32_t width;
                m_data.framebufferExtent.height, // uint32_t height;
                1u                               // uint32_t depth;
            },                                   // VkExtent3D extent;
            1u,                                  // uint32_t mipLevels;
            1u,                                  // uint32_t arrayLayers;
            m_data.samples,                      // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
            cbUsage,                             // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
            0u,                                  // uint32_t queueFamilyIndexCount;
            nullptr,                             // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout initialLayout;
        };
        cbImagePass0 = de::MovePtr<ImageWithMemory>(
            new ImageWithMemory(vk, device, *allocator, imageCreateInfo, MemoryRequirement::Any));

        VkImageViewCreateInfo imageViewCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
            nullptr,                                  // const void* pNext;
            (VkImageViewCreateFlags)0u,               // VkImageViewCreateFlags flags;
            **cbImagePass0,                           // VkImage image;
            VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
            imageFormat,                              // VkFormat format;
            {
                VK_COMPONENT_SWIZZLE_R, // VkComponentSwizzle r;
                VK_COMPONENT_SWIZZLE_G, // VkComponentSwizzle g;
                VK_COMPONENT_SWIZZLE_B, // VkComponentSwizzle b;
                VK_COMPONENT_SWIZZLE_A  // VkComponentSwizzle a;
            },                          // VkComponentMapping  components;
            {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t baseMipLevel;
                1u,                        // uint32_t levelCount;
                0u,                        // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            }                              // VkImageSubresourceRange subresourceRange;
        };
        cbImagePass0View = createImageView(vk, device, &imageViewCreateInfo, NULL);
    }

    // Create color attachment for subpass 1
    de::MovePtr<ImageWithMemory> cbImagePass1;
    Move<VkImageView> cbImagePass1View;
    {
        const VkImageUsageFlags cbUsage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        const VkImageCreateInfo imageCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                             // const void* pNext;
            (VkImageCreateFlags)0u,              // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
            imageFormat,                         // VkFormat format;
            {
                m_data.framebufferExtent.width,  // uint32_t width;
                m_data.framebufferExtent.height, // uint32_t height;
                1u                               // uint32_t depth;
            },                                   // VkExtent3D extent;
            1u,                                  // uint32_t mipLevels;
            1u,                                  // uint32_t arrayLayers;
            VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
            cbUsage,                             // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
            0u,                                  // uint32_t queueFamilyIndexCount;
            nullptr,                             // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout initialLayout;
        };
        cbImagePass1 = de::MovePtr<ImageWithMemory>(
            new ImageWithMemory(vk, device, *allocator, imageCreateInfo, MemoryRequirement::Any));

        VkImageViewCreateInfo imageViewCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
            nullptr,                                  // const void* pNext;
            (VkImageViewCreateFlags)0u,               // VkImageViewCreateFlags flags;
            **cbImagePass1,                           // VkImage image;
            VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
            imageFormat,                              // VkFormat format;
            {
                VK_COMPONENT_SWIZZLE_R, // VkComponentSwizzle r;
                VK_COMPONENT_SWIZZLE_G, // VkComponentSwizzle g;
                VK_COMPONENT_SWIZZLE_B, // VkComponentSwizzle b;
                VK_COMPONENT_SWIZZLE_A  // VkComponentSwizzle a;
            },                          // VkComponentMapping  components;
            {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t baseMipLevel;
                1u,                        // uint32_t levelCount;
                0u,                        // uint32_t baseArrayLayer;
                1u                         // uint32_t layerCount;
            }                              // VkImageSubresourceRange subresourceRange;
        };
        cbImagePass1View = createImageView(vk, device, &imageViewCreateInfo, NULL);
    }

    // Create render pass
    Move<VkRenderPass> renderPass;
    {
        const vk::VkAttachmentReference colorAttachment0Reference = {
            0,                           // attachment
            vk::VK_IMAGE_LAYOUT_GENERAL, // layout
        };

        const vk::VkAttachmentReference colorAttachment1Reference = {
            1,                           // attachment
            vk::VK_IMAGE_LAYOUT_GENERAL, // layout
        };

        std::vector<VkAttachmentDescription> attachmentDescriptions;

        attachmentDescriptions.push_back({
            (VkAttachmentDescriptionFlags)0u, // VkAttachmentDescriptionFlags flags;
            imageFormat,                      // VkFormat format;
            m_data.samples,                   // VkSampleCountFlagBits samples;
            VK_ATTACHMENT_LOAD_OP_LOAD,       // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,     // VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE, // VkAttachmentStoreOp stencilStoreOp;
            VK_IMAGE_LAYOUT_GENERAL,          // VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_GENERAL           // VkImageLayout finalLayout;
        });

        attachmentDescriptions.push_back({
            (VkAttachmentDescriptionFlags)0u, // VkAttachmentDescriptionFlags flags;
            imageFormat,                      // VkFormat format;
            VK_SAMPLE_COUNT_1_BIT,            // VkSampleCountFlagBits samples;
            VK_ATTACHMENT_LOAD_OP_LOAD,       // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,     // VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE, // VkAttachmentStoreOp stencilStoreOp;
            VK_IMAGE_LAYOUT_GENERAL,          // VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_GENERAL           // VkImageLayout finalLayout;
        });

        const VkSubpassDescription subpassDescs[] = {
            {
                (vk::VkSubpassDescriptionFlags)0,    // flags
                vk::VK_PIPELINE_BIND_POINT_GRAPHICS, // pipelineBindPoint
                0u,                                  // inputCount
                nullptr,                             // pInputAttachments
                1u,                                  // colorCount
                &colorAttachment0Reference,          // pColorAttachments
                nullptr,                             // pResolveAttachments
                nullptr,                             // depthStencilAttachment
                0u,                                  // preserveCount
                nullptr,                             // pPreserveAttachments
            },
            {
                (vk::VkSubpassDescriptionFlags)0,    // flags
                vk::VK_PIPELINE_BIND_POINT_GRAPHICS, // pipelineBindPoint
                1u,                                  // inputCount
                &colorAttachment0Reference,          // pInputAttachments
                1u,                                  // colorCount
                &colorAttachment1Reference,          // pColorAttachments
                nullptr,                             // pResolveAttachments
                nullptr,                             // depthStencilAttachment
                0u,                                  // preserveCount
                nullptr,                             // pPreserveAttachments
            },
        };

        const VkSubpassDependency subpassDependency = {
            0u,                                            // srcSubpass;
            1u,                                            // dstSubpass;
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask;
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,         // dstStageMask;
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // srcAccessMask;
            VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,           // dstAccessMask;
            0                                              // dependencyFlags;
        };

        const VkRenderPassCreateInfo renderPassParams = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // sType
            nullptr,                                   // pNext
            (vk::VkRenderPassCreateFlags)0,
            (uint32_t)attachmentDescriptions.size(),        // attachmentCount
            &attachmentDescriptions[0],                     // pAttachments
            sizeof(subpassDescs) / sizeof(subpassDescs[0]), // subpassCount
            subpassDescs,                                   // pSubpasses
            1u,                                             // dependencyCount
            &subpassDependency,                             // pDependencies
        };

        renderPass = createRenderPass(vk, device, &renderPassParams);
    }

    // Create framebuffer
    Move<VkFramebuffer> framebuffer;
    {
        std::vector<VkImageView> attachments;
        attachments.push_back(*cbImagePass0View);
        attachments.push_back(*cbImagePass1View);

        const vk::VkFramebufferCreateInfo framebufferParams = {
            vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // sType
            nullptr,                                       // pNext
            (vk::VkFramebufferCreateFlags)(0),             // createFlags
            *renderPass,                                   // renderPass
            (uint32_t)attachments.size(),                  // attachmentCount
            &attachments[0],                               // pAttachments
            m_data.framebufferExtent.width,                // width
            m_data.framebufferExtent.height,               // height
            1u,                                            // layers
        };

        framebuffer = createFramebuffer(vk, device, &framebufferParams);
    }

    // Create vertex attribute
    const VkVertexInputBindingDescription vertexBinding = {
        0u,                         // uint32_t binding;
        sizeof(Vertex),             // uint32_t stride;
        VK_VERTEX_INPUT_RATE_VERTEX // VkVertexInputRate inputRate;
    };

    const VkVertexInputAttributeDescription vertexInputAttributeDescription = {
        0u,                      // uint32_t location;
        0u,                      // uint32_t binding;
        VK_FORMAT_R32G32_SFLOAT, // VkFormat format;
        0u                       // uint32_t offset;
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                   // const void* pNext;
        (VkPipelineVertexInputStateCreateFlags)0,                  // VkPipelineVertexInputStateCreateFlags flags;
        1u,                                                        // uint32_t vertexBindingDescriptionCount;
        &vertexBinding,                  // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        1u,                              // uint32_t vertexAttributeDescriptionCount;
        &vertexInputAttributeDescription // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                     // const void* pNext;
        (VkPipelineInputAssemblyStateCreateFlags)0,                  // VkPipelineInputAssemblyStateCreateFlags flags;
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,                         // VkPrimitiveTopology topology;
        VK_FALSE                                                     // VkBool32 primitiveRestartEnable;
    };

    // Create rasterization state
    const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        (VkPipelineRasterizationStateCreateFlags)0,                 // VkPipelineRasterizationStateCreateFlags flags;
        VK_FALSE,                                                   // VkBool32 depthClampEnable;
        VK_FALSE,                                                   // VkBool32 rasterizerDiscardEnable;
        VK_POLYGON_MODE_FILL,                                       // VkPolygonMode polygonMode;
        VK_CULL_MODE_NONE,                                          // VkCullModeFlags cullMode;
        VK_FRONT_FACE_CLOCKWISE,                                    // VkFrontFace frontFace;
        VK_FALSE,                                                   // VkBool32 depthBiasEnable;
        0.0f,                                                       // float depthBiasConstantFactor;
        0.0f,                                                       // float depthBiasClamp;
        0.0f,                                                       // float depthBiasSlopeFactor;
        1.0f                                                        // float lineWidth;
    };

    // Create scissor and viewport
    VkViewport viewport = makeViewport(m_data.framebufferExtent.width, m_data.framebufferExtent.height);
    VkRect2D scissor    = makeRect2D(m_data.framebufferExtent.width, m_data.framebufferExtent.height);

    const VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                                               // const void*                                pNext
        (VkPipelineViewportStateCreateFlags)0,                 // VkPipelineViewportStateCreateFlags        flags
        1u,        // uint32_t                                    viewportCount
        &viewport, // const VkViewport*                        pViewports
        1u,        // uint32_t                                    scissorCount
        &scissor   // const VkRect2D*                            pScissors
    };

    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, // VkStructureType   sType;
        nullptr,                                              // const void*   pNext;
        (VkPipelineDynamicStateCreateFlags)0,                 // VkPipelineDynamicStateCreateFlags flags;
        0u,                                                   // uint32_t  dynamicStateCount;
        nullptr,                                              // const VkDynamicState* pDynamicStates;
    };

    const VkPipelineColorBlendAttachmentState colorBlendAttachmentState[] = {{
        VK_FALSE,             // VkBool32 blendEnable;
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor srcColorBlendFactor;
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor dstColorBlendFactor;
        VK_BLEND_OP_ADD,      // VkBlendOp colorBlendOp;
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor srcAlphaBlendFactor;
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor dstAlphaBlendFactor;
        VK_BLEND_OP_ADD,      // VkBlendOp alphaBlendOp;
        0xf                   // VkColorComponentFlags colorWriteMask;
    }};

    const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineColorBlendStateCreateFlags flags;
        VK_FALSE,                                                 // VkBool32 logicOpEnable;
        VK_LOGIC_OP_COPY,                                         // VkLogicOp logicOp;
        sizeof(colorBlendAttachmentState) / sizeof(colorBlendAttachmentState[0]), // uint32_t attachmentCount;
        colorBlendAttachmentState, // const VkPipelineColorBlendAttachmentState* pAttachments;
        {1.0f, 1.0f, 1.0f, 1.0f}   // float blendConstants[4];
    };

    VkPipelineDepthStencilStateCreateInfo depthStencilStateParams = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        0u,                                                         // VkPipelineDepthStencilStateCreateFlags flags;
        VK_FALSE,                                                   // VkBool32 depthTestEnable;
        VK_FALSE,                                                   // VkBool32 depthWriteEnable;
        VK_COMPARE_OP_ALWAYS,                                       // VkCompareOp depthCompareOp;
        VK_FALSE,                                                   // VkBool32 depthBoundsTestEnable;
        VK_FALSE,                                                   // VkBool32 stencilTestEnable;
        // VkStencilOpState front;
        {
            VK_STENCIL_OP_REPLACE, // VkStencilOp failOp;
            VK_STENCIL_OP_REPLACE, // VkStencilOp passOp;
            VK_STENCIL_OP_REPLACE, // VkStencilOp depthFailOp;
            VK_COMPARE_OP_ALWAYS,  // VkCompareOp compareOp;
            0u,                    // uint32_t compareMask;
            0xFFu,                 // uint32_t writeMask;
            0xFFu,                 // uint32_t reference;
        },
        // VkStencilOpState back;
        {
            VK_STENCIL_OP_REPLACE, // VkStencilOp failOp;
            VK_STENCIL_OP_REPLACE, // VkStencilOp passOp;
            VK_STENCIL_OP_REPLACE, // VkStencilOp depthFailOp;
            VK_COMPARE_OP_ALWAYS,  // VkCompareOp compareOp;
            0u,                    // uint32_t compareMask;
            0xFFu,                 // uint32_t writeMask;
            0xFFu,                 // uint32_t reference;
        },
        0.0f, // float minDepthBounds;
        0.0f, // float maxDepthBounds;
    };

    // Create pipeline for pass 0
    Move<VkPipeline> pipelinePass0;
    Move<VkPipelineLayout> pipelineLayoutPass0;
    {
        const VkPushConstantRange pushConstantRange = {
            VK_SHADER_STAGE_FRAGMENT_BIT, // VkShaderStageFlags stageFlags;
            0u,                           // uint32_t offset;
            2 * sizeof(VkExtent2D)        // uint32_t size;
        };

        const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
            nullptr,                                       // pNext
            (VkPipelineLayoutCreateFlags)0,
            0u,                 // setLayoutCount
            nullptr,            // pSetLayouts
            1u,                 // pushConstantRangeCount
            &pushConstantRange, // pPushConstantRanges
        };

        pipelineLayoutPass0 = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);

        const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType                            sType
            nullptr,                                                  // const void*                    pNext
            0u,                                                       // VkPipelineMultisampleStateCreateFlags    flags
            (VkSampleCountFlagBits)m_data.samples, // VkSampleCountFlagBits                    rasterizationSamples
            VK_FALSE,                              // VkBool32                                    sampleShadingEnable
            1.0f,                                  // float                                    minSampleShading
            nullptr,                               // const VkSampleMask*                        pSampleMask
            VK_FALSE,                              // VkBool32                                    alphaToCoverageEnable
            VK_FALSE                               // VkBool32                                    alphaToOneEnable
        };

        Move<VkShaderModule> vertShader =
            createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
        Move<VkShaderModule> fragShader =
            createShaderModule(vk, device, m_context.getBinaryCollection().get("frag_pass0"), 0);

        const VkPipelineShaderStageCreateInfo shaderCreateInfo[] = {
            {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, (VkPipelineShaderStageCreateFlags)0,
                VK_SHADER_STAGE_VERTEX_BIT, // stage
                *vertShader,                // shader
                "main",
                nullptr, // pSpecializationInfo
            },
            {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, (VkPipelineShaderStageCreateFlags)0,
                VK_SHADER_STAGE_FRAGMENT_BIT, // stage
                *fragShader,                  // shader
                "main",
                nullptr, // pSpecializationInfo
            }};

        const VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,        // VkStructureType sType;
            nullptr,                                                // const void* pNext;
            (VkPipelineCreateFlags)0,                               // VkPipelineCreateFlags flags;
            sizeof(shaderCreateInfo) / sizeof(shaderCreateInfo[0]), // uint32_t stageCount;
            &shaderCreateInfo[0],                                   // const VkPipelineShaderStageCreateInfo* pStages;
            &vertexInputStateCreateInfo,   // const VkPipelineVertexInputStateCreateInfo* pVertexInputState;
            &inputAssemblyStateCreateInfo, // const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState;
            nullptr,                       // const VkPipelineTessellationStateCreateInfo* pTessellationState;
            &viewportStateCreateInfo,      // const VkPipelineViewportStateCreateInfo* pViewportState;
            &rasterizationStateCreateInfo, // const VkPipelineRasterizationStateCreateInfo* pRasterizationState;
            &multisampleStateCreateInfo,   // const VkPipelineMultisampleStateCreateInfo* pMultisampleState;
            &depthStencilStateParams,      // const VkPipelineDepthStencilStateCreateInfo* pDepthStencilState;
            &colorBlendStateCreateInfo,    // const VkPipelineColorBlendStateCreateInfo* pColorBlendState;
            &dynamicStateCreateInfo,       // const VkPipelineDynamicStateCreateInfo* pDynamicState;
            pipelineLayoutPass0.get(),     // VkPipelineLayout layout;
            renderPass.get(),              // VkRenderPass renderPass;
            0u,                            // uint32_t subpass;
            VK_NULL_HANDLE,                // VkPipeline basePipelineHandle;
            0                              // int basePipelineIndex;
        };

        pipelinePass0 = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &graphicsPipelineCreateInfo);
    }

    // Create pipeline for pass 1
    Move<VkPipeline> pipelinePass1;
    Move<VkPipelineLayout> pipelineLayoutPass1;
    Move<vk::VkDescriptorPool> descriptorPool;
    Move<vk::VkDescriptorSetLayout> descriptorSetLayout;
    Move<vk::VkDescriptorSet> descriptorSet;
    {
        const VkDescriptorSetLayoutBinding bindings[] = {{
            0u,                                  // binding
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, // descriptorType
            1u,                                  // descriptorCount
            VK_SHADER_STAGE_FRAGMENT_BIT,        // stageFlags
            nullptr,                             // pImmutableSamplers
        }};

        // Create a layout and allocate a descriptor set for it.
        const VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = {
            vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // sType
            nullptr,                                                 // pNext
            (VkDescriptorSetLayoutCreateFlags)(0),                   // flags
            sizeof(bindings) / sizeof(bindings[0]),                  // bindingCount
            &bindings[0]                                             // pBindings
        };

        descriptorSetLayout = vk::createDescriptorSetLayout(vk, device, &setLayoutCreateInfo);

        vk::DescriptorPoolBuilder poolBuilder;

        for (int32_t i = 0; i < (int32_t)(sizeof(bindings) / sizeof(bindings[0])); ++i)
        {
            poolBuilder.addType(bindings[i].descriptorType, bindings[i].descriptorCount);
        }

        descriptorPool = poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
        descriptorSet  = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);

        VkDescriptorImageInfo imageInfo =
            makeDescriptorImageInfo(VK_NULL_HANDLE, *cbImagePass0View, VK_IMAGE_LAYOUT_GENERAL);

        VkWriteDescriptorSet writeDescriptorSet = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // sType
            nullptr,                                // pNext
            *descriptorSet,                         // dstSet
            0u,                                     // dstBinding
            0u,                                     // dstArrayElement
            1u,                                     // descriptorCount
            bindings[0].descriptorType,             // descriptorType
            &imageInfo,                             // pImageInfo
            nullptr,                                // pBufferInfo
            nullptr,                                // pTexelBufferView
        };

        vk.updateDescriptorSets(device, 1, &writeDescriptorSet, 0, NULL);

        const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
            nullptr,                                       // pNext
            (VkPipelineLayoutCreateFlags)0,
            1u,                         // setLayoutCount
            &descriptorSetLayout.get(), // pSetLayouts
            0u,                         // pushConstantRangeCount
            nullptr,                    // pPushConstantRanges
        };

        pipelineLayoutPass1 = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);

        const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType                            sType
            nullptr,               // const void*                                pNext
            0u,                    // VkPipelineMultisampleStateCreateFlags    flags
            VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits                    rasterizationSamples
            VK_FALSE,              // VkBool32                                    sampleShadingEnable
            1.0f,                  // float                                    minSampleShading
            nullptr,               // const VkSampleMask*                        pSampleMask
            VK_FALSE,              // VkBool32                                    alphaToCoverageEnable
            VK_FALSE               // VkBool32                                    alphaToOneEnable
        };

        VkPipelineFragmentShadingRateStateCreateInfoKHR shadingRateStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR, // VkStructureType sType;
            nullptr,                                                                // const void* pNext;
            m_data.shadingRate,                                                     // VkExtent2D fragmentSize;
            {VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,
             VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR}, // VkFragmentShadingRateCombinerOpKHR combinerOps[2];
        };

        Move<VkShaderModule> vertShader =
            createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
        Move<VkShaderModule> fragShader =
            createShaderModule(vk, device, m_context.getBinaryCollection().get("frag_pass1"), 0);

        const VkPipelineShaderStageCreateInfo shaderCreateInfo[] = {
            {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, (VkPipelineShaderStageCreateFlags)0,
                VK_SHADER_STAGE_VERTEX_BIT, // stage
                *vertShader,                // shader
                "main",
                nullptr, // pSpecializationInfo
            },
            {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, (VkPipelineShaderStageCreateFlags)0,
                VK_SHADER_STAGE_FRAGMENT_BIT, // stage
                *fragShader,                  // shader
                "main",
                nullptr, // pSpecializationInfo
            }};

        const VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,        // VkStructureType sType;
            &shadingRateStateCreateInfo,                            // const void* pNext;
            (VkPipelineCreateFlags)0,                               // VkPipelineCreateFlags flags;
            sizeof(shaderCreateInfo) / sizeof(shaderCreateInfo[0]), // uint32_t stageCount;
            &shaderCreateInfo[0],                                   // const VkPipelineShaderStageCreateInfo* pStages;
            &vertexInputStateCreateInfo,   // const VkPipelineVertexInputStateCreateInfo* pVertexInputState;
            &inputAssemblyStateCreateInfo, // const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState;
            nullptr,                       // const VkPipelineTessellationStateCreateInfo* pTessellationState;
            &viewportStateCreateInfo,      // const VkPipelineViewportStateCreateInfo* pViewportState;
            &rasterizationStateCreateInfo, // const VkPipelineRasterizationStateCreateInfo* pRasterizationState;
            &multisampleStateCreateInfo,   // const VkPipelineMultisampleStateCreateInfo* pMultisampleState;
            &depthStencilStateParams,      // const VkPipelineDepthStencilStateCreateInfo* pDepthStencilState;
            &colorBlendStateCreateInfo,    // const VkPipelineColorBlendStateCreateInfo* pColorBlendState;
            &dynamicStateCreateInfo,       // const VkPipelineDynamicStateCreateInfo* pDynamicState;
            pipelineLayoutPass1.get(),     // VkPipelineLayout layout;
            renderPass.get(),              // VkRenderPass renderPass;
            1u,                            // uint32_t subpass;
            VK_NULL_HANDLE,                // VkPipeline basePipelineHandle;
            0                              // int basePipelineIndex;
        };

        pipelinePass1 = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &graphicsPipelineCreateInfo);
    }

    // Create command buffer
    Move<VkCommandPool> cmdPool     = createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                                        m_context.getUniversalQueueFamilyIndex());
    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    VkImageMemoryBarrier preImageBarriers[] = {{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType        sType
                                                nullptr,                                // const void*            pNext
                                                0u,                           // VkAccessFlags        srcAccessMask
                                                VK_ACCESS_TRANSFER_WRITE_BIT, // VkAccessFlags        dstAccessMask
                                                VK_IMAGE_LAYOUT_UNDEFINED,    // VkImageLayout        oldLayout
                                                VK_IMAGE_LAYOUT_GENERAL,      // VkImageLayout        newLayout
                                                VK_QUEUE_FAMILY_IGNORED, // uint32_t                srcQueueFamilyIndex
                                                VK_QUEUE_FAMILY_IGNORED, // uint32_t                dstQueueFamilyIndex
                                                **cbImagePass0,          // VkImage                image
                                                {
                                                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags    aspectMask
                                                    0u,                        // uint32_t                baseMipLevel
                                                    VK_REMAINING_MIP_LEVELS,   // uint32_t                mipLevels,
                                                    0u,                        // uint32_t                baseArray
                                                    VK_REMAINING_ARRAY_LAYERS, // uint32_t                arraySize
                                                }},
                                               {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType        sType
                                                nullptr,                                // const void*            pNext
                                                0u,                           // VkAccessFlags        srcAccessMask
                                                VK_ACCESS_TRANSFER_WRITE_BIT, // VkAccessFlags        dstAccessMask
                                                VK_IMAGE_LAYOUT_UNDEFINED,    // VkImageLayout        oldLayout
                                                VK_IMAGE_LAYOUT_GENERAL,      // VkImageLayout        newLayout
                                                VK_QUEUE_FAMILY_IGNORED, // uint32_t                srcQueueFamilyIndex
                                                VK_QUEUE_FAMILY_IGNORED, // uint32_t                dstQueueFamilyIndex
                                                **cbImagePass1,          // VkImage                image
                                                {
                                                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags    aspectMask
                                                    0u,                        // uint32_t                baseMipLevel
                                                    VK_REMAINING_MIP_LEVELS,   // uint32_t                mipLevels,
                                                    0u,                        // uint32_t                baseArray
                                                    VK_REMAINING_ARRAY_LAYERS, // uint32_t                arraySize
                                                }}};

    // Record commands
    beginCommandBuffer(vk, *cmdBuffer, 0u);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr,
                          sizeof(preImageBarriers) / sizeof(preImageBarriers[0]), preImageBarriers);

    // Clear both images to UINT_MAX
    VkImageSubresourceRange range = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    VkClearValue clearColor       = makeClearValueColorU32(std::numeric_limits<unsigned int>::max(), 0, 0, 0);

    vk.cmdClearColorImage(*cmdBuffer, **cbImagePass0, VK_IMAGE_LAYOUT_GENERAL, &clearColor.color, 1, &range);
    vk.cmdClearColorImage(*cmdBuffer, **cbImagePass1, VK_IMAGE_LAYOUT_GENERAL, &clearColor.color, 1, &range);

    // Barrier between the clear and the rendering
    VkImageMemoryBarrier clearColorBarriers[] = {
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType        sType
         nullptr,                                // const void*            pNext
         VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags        srcAccessMask
         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,   // VkAccessFlags        dstAccessMask
         VK_IMAGE_LAYOUT_GENERAL,                // VkImageLayout        oldLayout
         VK_IMAGE_LAYOUT_GENERAL,                // VkImageLayout        newLayout
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t                srcQueueFamilyIndex
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t                dstQueueFamilyIndex
         **cbImagePass0,                         // VkImage                image
         {
             VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags    aspectMask
             0u,                        // uint32_t                baseMipLevel
             VK_REMAINING_MIP_LEVELS,   // uint32_t                mipLevels,
             0u,                        // uint32_t                baseArray
             VK_REMAINING_ARRAY_LAYERS, // uint32_t                arraySize
         }},
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType        sType
         nullptr,                                // const void*            pNext
         VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags        srcAccessMask
         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,   // VkAccessFlags        dstAccessMask
         VK_IMAGE_LAYOUT_GENERAL,                // VkImageLayout        oldLayout
         VK_IMAGE_LAYOUT_GENERAL,                // VkImageLayout        newLayout
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t                srcQueueFamilyIndex
         VK_QUEUE_FAMILY_IGNORED,                // uint32_t                dstQueueFamilyIndex
         **cbImagePass1,                         // VkImage                image
         {
             VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags    aspectMask
             0u,                        // uint32_t                baseMipLevel
             VK_REMAINING_MIP_LEVELS,   // uint32_t                mipLevels,
             0u,                        // uint32_t                baseArray
             VK_REMAINING_ARRAY_LAYERS, // uint32_t                arraySize
         }}};

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr,
                          sizeof(clearColorBarriers) / sizeof(clearColorBarriers[0]), clearColorBarriers);

    beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer,
                    makeRect2D(m_data.framebufferExtent.width, m_data.framebufferExtent.height), 0, nullptr,
                    VK_SUBPASS_CONTENTS_INLINE, nullptr);

    // Put primitive shading rate in a push constant
    if (m_shadingRateClamped.size() == 1)
    {
        vk.cmdPushConstants(*cmdBuffer, *pipelineLayoutPass0, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                            sizeof(m_shadingRateClamped[0]), &m_shadingRateClamped[0]);
        vk.cmdPushConstants(*cmdBuffer, *pipelineLayoutPass0, VK_SHADER_STAGE_FRAGMENT_BIT,
                            sizeof(m_shadingRateClamped[0]), sizeof(m_shadingRateClamped[0]), &m_shadingRateClamped[0]);
    }
    else
    {
        vk.cmdPushConstants(*cmdBuffer, *pipelineLayoutPass0, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                            static_cast<uint32_t>(m_shadingRateClamped.size() * sizeof(m_shadingRateClamped[0])),
                            &m_shadingRateClamped[0]);
    }

    // Bind vertex buffer
    const VkDeviceSize vertexBufferOffset = 0;
    VkBuffer vb                           = **vertexBuffer;
    vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vb, &vertexBufferOffset);

    // Bind pipeline
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelinePass0);

    // Draw triangles
    vk.cmdDraw(*cmdBuffer, sizeof(basicTriangles) / sizeof(Vertex), 1u, 0u, 0u);

    // Start next subpass
    vk.cmdNextSubpass(*cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);

    // Bind descriptors
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayoutPass1, 0, 1,
                             &descriptorSet.get(), 0, nullptr);

    // Bind vertex buffer
    vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vb, &vertexBufferOffset);

    // Bind pipeline
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelinePass1);

    // Draw triangles
    vk.cmdDraw(*cmdBuffer, sizeof(basicTriangles) / sizeof(Vertex), 1u, 0u, 0u);

    endRenderPass(vk, *cmdBuffer);

    VkImageMemoryBarrier postImageBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType        sType
                                             nullptr,                                // const void*            pNext
                                             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // VkAccessFlags        srcAccessMask
                                             VK_ACCESS_TRANSFER_READ_BIT,          // VkAccessFlags        dstAccessMask
                                             VK_IMAGE_LAYOUT_GENERAL,              // VkImageLayout        oldLayout
                                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // VkImageLayout        newLayout
                                             VK_QUEUE_FAMILY_IGNORED, // uint32_t                srcQueueFamilyIndex
                                             VK_QUEUE_FAMILY_IGNORED, // uint32_t                dstQueueFamilyIndex
                                             **cbImagePass1,          // VkImage                image
                                             {
                                                 VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags    aspectMask
                                                 0u,                        // uint32_t                baseMipLevel
                                                 VK_REMAINING_MIP_LEVELS,   // uint32_t                mipLevels,
                                                 0u,                        // uint32_t                baseArray
                                                 VK_REMAINING_ARRAY_LAYERS, // uint32_t                arraySize
                                             }};

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &postImageBarrier);

    const VkBufferImageCopy copyRegion = {
        0u, // VkDeviceSize bufferOffset;
        0u, // uint32_t bufferRowLength;
        0u, // uint32_t bufferImageHeight;
        {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspect;
            0u,                        // uint32_t mipLevel;
            0u,                        // uint32_t baseArrayLayer;
            1u,                        // uint32_t layerCount;
        },                             // VkImageSubresourceLayers imageSubresource;
        {0, 0, 0},                     // VkOffset3D imageOffset;
        {m_data.framebufferExtent.width, m_data.framebufferExtent.height, 1} // VkExtent3D imageExtent;
    };

    vk.cmdCopyImageToBuffer(*cmdBuffer, **cbImagePass1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **colorOutputBuffer, 1u,
                            &copyRegion);

    const VkBufferMemoryBarrier bufferBarrier = {
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                 // const void* pNext;
        VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags srcAccessMask;
        VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                 // uint32_t dstQueueFamilyIndex;
        **colorOutputBuffer,                     // VkBuffer buffer;
        0ull,                                    // VkDeviceSize offset;
        VK_WHOLE_SIZE                            // VkDeviceSize size;
    };

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0,
                          0, nullptr, 1, &bufferBarrier, 0, nullptr);

    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

    // Read buffer data
    invalidateAlloc(vk, device, colorOutputBuffer->getAllocation());

    tcu::ConstPixelBufferAccess resultBuffer = tcu::ConstPixelBufferAccess(
        tcu::TextureFormat(tcu::TextureFormat::RG, tcu::TextureFormat::UNSIGNED_INT32), m_data.framebufferExtent.width,
        m_data.framebufferExtent.height, 1, (const void *)colorOutputBuffer->getAllocation().getHostPtr());

    for (uint32_t i = 0; i < m_shadingRateClamped.size(); i++)
    {
        tcu::TestStatus result = verifyResult(resultBuffer, i);
        if (result.getCode() == QP_TEST_RESULT_PASS)
        {
            return result;
        }
    }

    return tcu::TestStatus(QP_TEST_RESULT_FAIL, qpGetTestResultName(QP_TEST_RESULT_FAIL));
}

} // namespace

void createPixelConsistencyTests(tcu::TestContext &testCtx, tcu::TestCaseGroup *parentGroup)
{
    typedef struct
    {
        uint32_t count;
        const char *name;
    } TestGroupCase;

    typedef struct
    {
        VkExtent2D count;
        const char *name;
    } TestGroupCase2D;

    TestGroupCase2D shadingRateCases[] = {
        {{1, 1}, "rate_1x1"},
        {{1, 2}, "rate_1x2"},
        {{1, 4}, "rate_1x4"},
        {{2, 1}, "rate_2x1"},
        {{2, 2}, "rate_2x2"},
        {
            {2, 4},
            "rate_2x4",
        },
        {
            {4, 1},
            "rate_4x1",
        },
        {
            {4, 2},
            "rate_4x2",
        },
        {
            {4, 4},
            "rate_4x4",
        },
    };

    TestGroupCase sampCases[] = {
        {VK_SAMPLE_COUNT_1_BIT, "samples_1"},   {VK_SAMPLE_COUNT_2_BIT, "samples_2"},
        {VK_SAMPLE_COUNT_4_BIT, "samples_4"},   {VK_SAMPLE_COUNT_8_BIT, "samples_8"},
        {VK_SAMPLE_COUNT_16_BIT, "samples_16"},
    };

    TestGroupCase2D extentCases[] = {
        {{1, 1}, "extent_1x1"},         {{4, 4}, "extent_4x4"},         {{33, 35}, "extent_33x35"},
        {{151, 431}, "extent_151x431"}, {{256, 256}, "extent_256x256"},
    };

    de::MovePtr<tcu::TestCaseGroup> pixelGroup(new tcu::TestCaseGroup(testCtx, "pixel_consistency"));

    for (int rateNdx = 0; rateNdx < DE_LENGTH_OF_ARRAY(shadingRateCases); rateNdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> rateGroup(new tcu::TestCaseGroup(testCtx, shadingRateCases[rateNdx].name));

        for (int sampNdx = 0; sampNdx < DE_LENGTH_OF_ARRAY(sampCases); sampNdx++)
        {
            de::MovePtr<tcu::TestCaseGroup> sampleGroup(new tcu::TestCaseGroup(testCtx, sampCases[sampNdx].name));
            for (int extNdx = 0; extNdx < DE_LENGTH_OF_ARRAY(extentCases); extNdx++)
            {
                VkSampleCountFlagBits samples = static_cast<VkSampleCountFlagBits>(sampCases[sampNdx].count);
                VkExtent2D framebufferExtent  = extentCases[extNdx].count;

                CaseDef caseParams{shadingRateCases[rateNdx].count, samples, framebufferExtent, false};
                sampleGroup->addChild(new FSRPixelConsistencyTestCase(testCtx, extentCases[extNdx].name, caseParams));

                // test FragCoord.zw but to avoid duplication limit tests to extent_151x431/256x256 and 1 or 4 samples
                if ((framebufferExtent.width > 150) && (samples & (VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT)))
                {
                    std::string caseName = std::string(extentCases[extNdx].name) + "_zw_coord";
                    caseParams.zwCoord   = true;
                    sampleGroup->addChild(new FSRPixelConsistencyTestCase(testCtx, caseName.c_str(), caseParams));
                }
            }
            rateGroup->addChild(sampleGroup.release());
        }

        pixelGroup->addChild(rateGroup.release());
    }

    parentGroup->addChild(pixelGroup.release());
}

} // namespace FragmentShadingRate
} // namespace vkt
