/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief Tests fragment density map extension ( VK_EXT_fragment_density_map )
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassFragmentDensityMapTests.hpp"
#include "pipeline/vktPipelineImageUtil.hpp"
#include "deMath.h"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktRenderPassTestsUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "tcuCommandLine.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include <sstream>
#include <vector>
#include <set>
#include <mutex>
#include <cmath>
#include <algorithm>
#include <iterator>

// Each test generates an image with a color gradient where all colors should be unique when rendered without density map
// ( and for multi_view tests - the quantity of each color in a histogram should be 2 instead of 1 ).
// The whole density map has the same values defined by input fragment area ( one of the test input parameters ).
// With density map enabled - the number of each color in a histogram should be [ fragmentArea.x * fragmentArea.y ]
// ( that value will be doubled for multi_view case ).
//
// Additionally test checks if gl_FragSizeEXT shader variable has proper value ( as defined by fragmentArea input parameter ).
//
// Test variations:
// - multi_view tests check if density map also works when VK_KHR_multiview extension is in use
// - render_copy tests check if it's possible to copy results using input attachment descriptor ( this simulates deferred rendering behaviour )
// - non_divisible_density_size tests check if subsampled images work when its dimension is not divisible by minFragmentDensityTexelSize
// - N_samples tests check if multisampling works with VK_EXT_fragment_density_map extension
// - static_* tests use density map loaded from CPU during vkCmdBeginRenderPass.
// - dynamic_* tests use density map rendered on a GPU in a separate render pass
// - deffered_* tests use density map loaded from CPU during VkEndCommandBuffer.
// - *_nonsubsampled tests check if it's possible to use nonsubsampled images instead of subsampled ones

// There are 3 render passes performed during most of the tests:
//  - render pass that produces density map ( this rp is skipped when density map is static )
//  - render pass that produces subsampled image using density map and eventually copies results to different image ( render_copy )
//  - render pass that copies subsampled image to traditional image using sampler with VK_SAMPLER_CREATE_SUBSAMPLED_BIT_EXT flag.
//    ( because subsampled images cannot be retrieved to CPU in any other way ).
// There are few tests that use additional subpass that resamples subsampled image using diferent density map.

// Code of FragmentDensityMapTestInstance is also used to test subsampledLoads, subsampledCoarseReconstructionEarlyAccess,
// maxDescriptorSetSubsampledSamplers properties.

//#define USE_QCOM_OFFSET_EXT 1
#undef USE_QCOM_OFFSET_EXT

namespace vkt
{

namespace renderpass
{

using namespace vk;

namespace
{

struct TestParams
{
    bool dynamicDensityMap;
    bool deferredDensityMap;
    bool nonSubsampledImages;
    bool subsampledLoads;
    bool coarseReconstruction;
    bool imagelessFramebuffer;
    bool useMemoryAccess;
    bool useMaintenance5;
    uint32_t samplersCount;
    uint32_t viewCount;
    bool multiViewport;
    bool makeCopy;
    bool depthEnabled;
    bool addZeroOffset;
    float renderMultiplier;
    VkSampleCountFlagBits colorSamples;
    tcu::UVec2 fragmentArea;
    tcu::UVec2 densityMapSize;
    VkFormat densityMapFormat;
    VkFormat depthFormat;
    const SharedGroupParams groupParams;
};

struct Vertex4RGBA
{
    tcu::Vec4 position;
    tcu::Vec4 uv;
    tcu::Vec4 color;
};

class DeviceHelper
{
public:
    DeviceHelper(Context &context)
        : m_instance()
        , m_physicalDevice(VK_NULL_HANDLE)
        , m_device()
        , m_vkd()
        , m_queueFamilyIndex(context.getUniversalQueueFamilyIndex())
        , m_queue(VK_NULL_HANDLE)
        , m_allocator()
    {
        m_instance = createCustomInstanceWithExtensions(context, context.getInstanceExtensions());

        const float queuePriority = 1.0f;

        // Create a universal queue that supports graphics and compute
        const VkDeviceQueueCreateInfo queueParams{
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            0u,                                         // VkDeviceQueueCreateFlags flags;
            m_queueFamilyIndex,                         // uint32_t queueFamilyIndex;
            1u,                                         // uint32_t queueCount;
            &queuePriority                              // const float* pQueuePriorities;
        };

        // \note Extensions in core are not explicitly enabled even though
        //         they are in the extension list advertised to tests.
        const auto &extensionPtrs = context.getDeviceCreationExtensions();

        VkPhysicalDevicePortabilitySubsetFeaturesKHR portabilitySubsetFeatures                 = initVulkanStructure();
        VkPhysicalDeviceMultiviewFeatures multiviewFeatures                                    = initVulkanStructure();
        VkPhysicalDeviceImagelessFramebufferFeatures imagelessFramebufferFeatures              = initVulkanStructure();
        VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures                      = initVulkanStructure();
        VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR dynamicRenderingLocalReadFeatures = initVulkanStructure();
        VkPhysicalDeviceFragmentDensityMap2FeaturesEXT fragmentDensityMap2Features             = initVulkanStructure();
        VkPhysicalDeviceFragmentDensityMapFeaturesEXT fragmentDensityMapFeatures               = initVulkanStructure();
        VkPhysicalDeviceFragmentDensityMapOffsetFeaturesEXT fragmentDensityMapOffsetFeatures   = initVulkanStructure();
        VkPhysicalDeviceFeatures2 features2                                                    = initVulkanStructure();

        const auto addFeatures = makeStructChainAdder(&features2);

        if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset"))
            addFeatures(&portabilitySubsetFeatures);

        if (context.isDeviceFunctionalitySupported("VK_KHR_multiview"))
            addFeatures(&multiviewFeatures);

        if (context.isDeviceFunctionalitySupported("VK_KHR_imageless_framebuffer"))
            addFeatures(&imagelessFramebufferFeatures);

        if (context.isDeviceFunctionalitySupported("VK_KHR_dynamic_rendering"))
            addFeatures(&dynamicRenderingFeatures);

        if (context.isDeviceFunctionalitySupported("VK_KHR_dynamic_rendering_local_read"))
            addFeatures(&dynamicRenderingLocalReadFeatures);

        if (context.isDeviceFunctionalitySupported("VK_EXT_fragment_density_map2"))
            addFeatures(&fragmentDensityMap2Features);

#ifdef USE_QCOM_OFFSET_EXT
        if (context.isDeviceFunctionalitySupported("VK_QCOM_fragment_density_map_offset"))
#else
        if (context.isDeviceFunctionalitySupported("VK_EXT_fragment_density_map_offset"))
#endif
            addFeatures(&fragmentDensityMapOffsetFeatures);

        addFeatures(&fragmentDensityMapFeatures);

        const auto &vki     = m_instance.getDriver();
        const auto &cmdLine = context.getTestContext().getCommandLine();
        m_physicalDevice    = chooseDevice(vki, m_instance, cmdLine);

        vki.getPhysicalDeviceFeatures2(m_physicalDevice, &features2);
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

        const auto &vkp = context.getPlatformInterface();

        m_device = createCustomDevice(cmdLine.isValidationEnabled(), vkp, m_instance, vki, m_physicalDevice,
                                      &deviceCreateInfo);

        m_vkd.reset(new DeviceDriver(vkp, m_instance, *m_device, context.getUsedApiVersion(), cmdLine));
        m_vkd->getDeviceQueue(*m_device, m_queueFamilyIndex, 0u, &m_queue);

        VkPhysicalDeviceMemoryProperties memoryProperties;
        vki.getPhysicalDeviceMemoryProperties(m_physicalDevice, &memoryProperties);
        m_allocator.reset(new SimpleAllocator(*m_vkd, *m_device, memoryProperties));
    }

    const InstanceInterface &getInstanceInterface() const
    {
        return m_instance.getDriver();
    }
    VkInstance getInstance() const
    {
        return m_instance;
    }
    VkPhysicalDevice getPhysicalDevice() const
    {
        return m_physicalDevice;
    }
    const DeviceInterface &getDeviceInterface() const
    {
        return *m_vkd;
    }
    VkDevice getDevice() const
    {
        return *m_device;
    }
    uint32_t getQueueFamilyIndex() const
    {
        return m_queueFamilyIndex;
    }
    VkQueue getQueue() const
    {
        return m_queue;
    }
    Allocator &getAllocator() const
    {
        return *m_allocator;
    }

protected:
    CustomInstance m_instance;
    VkPhysicalDevice m_physicalDevice;
    Move<VkDevice> m_device;
    std::unique_ptr<DeviceDriver> m_vkd;
    uint32_t m_queueFamilyIndex;
    VkQueue m_queue;
    std::unique_ptr<SimpleAllocator> m_allocator;
};

// With non-null context, creates and gets the device. With null context, destroys it.
std::unique_ptr<DeviceHelper> g_deviceHelperPtr;

DeviceHelper &getDeviceHelper(Context &context)
{
    std::mutex creationMutex;
    std::lock_guard<std::mutex> lockGuard(creationMutex);

    if (!g_deviceHelperPtr)
        g_deviceHelperPtr.reset(new DeviceHelper(context));
    return *g_deviceHelperPtr;
}

std::vector<Vertex4RGBA> createFullscreenMesh(uint32_t viewCount, tcu::Vec2 redGradient, tcu::Vec2 greenGradient)
{
    DE_ASSERT(viewCount > 0);

    const auto &r    = redGradient;
    const auto &g    = greenGradient;
    const float step = 2.0f / static_cast<float>(viewCount);
    float xStart     = -1.0f;

    std::vector<Vertex4RGBA> resultMesh;
    for (uint32_t viewIndex = 0; viewIndex < viewCount; ++viewIndex)
    {
        const float fIndex       = static_cast<float>(viewIndex);
        const uint32_t nextIndex = viewIndex + 1;
        const float xEnd         = (nextIndex == viewCount) ? 1.0f : (-1.0f + step * static_cast<float>(nextIndex));

        // clang-format off
        // quad vertex                         position                      uv                          color
        const Vertex4RGBA lowerLeftVertex =    {{xStart, 1.0f, 0.0f, 1.0f},  {0.0f, 1.0f, fIndex, 1.0f}, {r.x(), g.y(), 0.0f, 1.0f}};
        const Vertex4RGBA upperLeftVertex =    {{xStart, -1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, fIndex, 1.0f}, {r.x(), g.x(), 0.0f, 1.0f}};
        const Vertex4RGBA lowerRightVertex =   {{xEnd, 1.0f, 0.0f, 1.0f},    {1.0f, 1.0f, fIndex, 1.0f}, {r.y(), g.y(), 0.0f, 1.0f}};
        const Vertex4RGBA upperRightVertex =   {{xEnd, -1.0f, 0.0f, 1.0f},   {1.0f, 0.0f, fIndex, 1.0f}, {r.y(), g.x(), 0.0f, 1.0f}};
        // clang-format on

        const std::vector<Vertex4RGBA> viewData{lowerLeftVertex, lowerRightVertex, upperLeftVertex,
                                                upperLeftVertex, lowerRightVertex, upperRightVertex};

        resultMesh.insert(resultMesh.end(), viewData.begin(), viewData.end());
        xStart = xEnd;
    }

    return resultMesh;
}

// Drop-in replacement for the function above, creating a full screen mesh surrounded by 8 mirrored replicas.
std::vector<Vertex4RGBA> createFullScreenMeshWithMirrors(uint32_t viewCount, tcu::Vec2 r, tcu::Vec2 g)
{
    DE_ASSERT(viewCount == 1u);
    DE_UNREF(viewCount);

    // The original geometry will be between -1 and 1, and the replicas will be offset by -2, 0 or +2 in each axis.
    const std::vector<tcu::Vec4> geometryOffsets{
        // clang-format off
        tcu::Vec4( 0.0f,  0.0f, 0.0f, 0.0f), // Original quad, using the first offset. The rest will be mirrored.
        tcu::Vec4( 0.0f, -2.0f, 0.0f, 0.0f), // Top mirror.
        tcu::Vec4( 2.0f, -2.0f, 0.0f, 0.0f), // Top-right mirror.
        tcu::Vec4( 2.0f,  0.0f, 0.0f, 0.0f), // Right mirror.
        tcu::Vec4( 2.0f,  2.0f, 0.0f, 0.0f), // Bottom-right mirror.
        tcu::Vec4( 0.0f,  2.0f, 0.0f, 0.0f), // Bottom mirror.
        tcu::Vec4(-2.0f,  2.0f, 0.0f, 0.0f), // Bottom-left mirror.
        tcu::Vec4(-2.0f,  0.0f, 0.0f, 0.0f), // Left mirror.
        tcu::Vec4(-2.0f, -2.0f, 0.0f, 0.0f), // Top-left mirror.
        // clang-format on
    };

    // Mirrored colors.
    const auto rm = r.swizzle(1, 0);
    const auto gm = g.swizzle(1, 0);

    // clang-format off
    // quad vertex               position                     uv                        color
    const Vertex4RGBA botLeft =  {{-1.0f,  1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {r.x(), g.y(), 0.0f, 1.0f}};
    const Vertex4RGBA topLeft =  {{-1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {r.x(), g.x(), 0.0f, 1.0f}};
    const Vertex4RGBA botRight = {{ 1.0f,  1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {r.y(), g.y(), 0.0f, 1.0f}};
    const Vertex4RGBA topRight = {{ 1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {r.y(), g.x(), 0.0f, 1.0f}};
    // clang-format on

    std::vector<Vertex4RGBA> resultMesh;
    const auto kVerticesPerQuad = 6u; // Each quad is 2 triangles with 3 vertices: 6 items.
    resultMesh.reserve(kVerticesPerQuad * de::sizeU32(geometryOffsets));

    for (size_t i = 0; i < geometryOffsets.size(); ++i)
    {
        const auto &offset = geometryOffsets.at(i);

        // Vertex red and green are mirrored if they're not centered in their axes.
        const auto &vr = (offset.x() == 0.0f ? r : rm);
        const auto &vg = (offset.y() == 0.0f ? g : gm);

        // Offset position by the geometry offset, then mirror the colors if needed.
        // UV coordinates do not change (unused).
        // clang-format off
        auto bl = botLeft;  bl.position += offset; bl.color.x() = vr.x(); bl.color.y() = vg.y();
        auto tl = topLeft;  tl.position += offset; tl.color.x() = vr.x(); tl.color.y() = vg.x();
        auto br = botRight; br.position += offset; br.color.x() = vr.y(); br.color.y() = vg.y();
        auto tr = topRight; tr.position += offset; tr.color.x() = vr.y(); tr.color.y() = vg.x();
        // clang-format on

        // Push the two triangles.
        resultMesh.push_back(bl);
        resultMesh.push_back(br);
        resultMesh.push_back(tl);
        resultMesh.push_back(tl);
        resultMesh.push_back(br);
        resultMesh.push_back(tr);
    }

    return resultMesh;
}

template <typename T>
void createVertexBuffer(const DeviceInterface &vk, VkDevice vkDevice, const uint32_t &queueFamilyIndex,
                        Allocator &memAlloc, const std::vector<T> &vertices, Move<VkBuffer> &vertexBuffer,
                        de::MovePtr<Allocation> &vertexAlloc)
{
    const VkBufferCreateInfo vertexBufferParams = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,        // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        0u,                                          // VkBufferCreateFlags flags;
        (VkDeviceSize)(sizeof(T) * vertices.size()), // VkDeviceSize size;
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,           // VkBufferUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,                   // VkSharingMode sharingMode;
        1u,                                          // uint32_t queueFamilyIndexCount;
        &queueFamilyIndex                            // const uint32_t* pQueueFamilyIndices;
    };

    vertexBuffer = createBuffer(vk, vkDevice, &vertexBufferParams);
    vertexAlloc =
        memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *vertexBuffer), MemoryRequirement::HostVisible);
    VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, vertexAlloc->getMemory(), vertexAlloc->getOffset()));

    // Upload vertex data
    deMemcpy(vertexAlloc->getHostPtr(), vertices.data(), vertices.size() * sizeof(T));
    flushAlloc(vk, vkDevice, *vertexAlloc);
}

void prepareImageAndImageView(const DeviceInterface &vk, VkDevice vkDevice, Allocator &memAlloc,
                              VkImageCreateFlags imageCreateFlags, VkFormat format, VkExtent3D extent,
                              uint32_t arrayLayers, VkSampleCountFlagBits samples, VkImageUsageFlags usage,
                              uint32_t queueFamilyIndex, VkImageViewCreateFlags viewFlags, VkImageViewType viewType,
                              const VkComponentMapping &channels, const VkImageSubresourceRange &subresourceRange,
                              Move<VkImage> &image, de::MovePtr<Allocation> &imageAlloc, Move<VkImageView> &imageView)
{
    const VkImageCreateInfo imageCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        imageCreateFlags,                    // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        format,                              // VkFormat format;
        extent,                              // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        arrayLayers,                         // uint32_t arrayLayers;
        samples,                             // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        usage,                               // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        1u,                                  // uint32_t queueFamilyIndexCount;
        &queueFamilyIndex,                   // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout initialLayout;
    };

    image = createImage(vk, vkDevice, &imageCreateInfo);

    // Allocate and bind color image memory
    imageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *image), MemoryRequirement::Any);
    VK_CHECK(vk.bindImageMemory(vkDevice, *image, imageAlloc->getMemory(), imageAlloc->getOffset()));

    // create image view for subsampled image
    const VkImageViewCreateInfo imageViewCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        viewFlags,                                // VkImageViewCreateFlags flags;
        *image,                                   // VkImage image;
        viewType,                                 // VkImageViewType viewType;
        format,                                   // VkFormat format;
        channels,                                 // VkChannelMapping channels;
        subresourceRange                          // VkImageSubresourceRange subresourceRange;
    };

    imageView = createImageView(vk, vkDevice, &imageViewCreateInfo);
}

// Class that provides abstraction over renderpass and renderpass2.
class RenderPassWrapperBase
{
public:
    RenderPassWrapperBase()          = default;
    virtual ~RenderPassWrapperBase() = default;

    virtual Move<VkRenderPass> createRenderPassProduceDynamicDensityMap(uint32_t viewMask) const     = 0;
    virtual Move<VkRenderPass> createRenderPassProduceSubsampledImage(uint32_t viewMask, bool makeCopySubpass,
                                                                      bool resampleSubsampled) const = 0;
    virtual Move<VkRenderPass> createRenderPassOutputSubsampledImage() const                         = 0;

    virtual void cmdBeginRenderPass(VkCommandBuffer cmdBuffer, const VkRenderPassBeginInfo *pRenderPassBegin) const = 0;
    virtual void cmdNextSubpass(VkCommandBuffer cmdBuffer) const                                                    = 0;
    virtual void cmdEndRenderPass(VkCommandBuffer cmdBuffer, bool addZeroOffset = false,
                                  uint32_t viewCount = 0u) const                                                    = 0;
};

// Helper template that lets us define all used types basing on single enum value.
template <RenderingType>
struct RenderPassTraits;

template <>
struct RenderPassTraits<RENDERING_TYPE_RENDERPASS_LEGACY>
{
    typedef AttachmentDescription1 AttachmentDesc;
    typedef AttachmentReference1 AttachmentRef;
    typedef SubpassDescription1 SubpassDesc;
    typedef SubpassDependency1 SubpassDep;
    typedef RenderPassCreateInfo1 RenderPassCreateInfo;
    typedef RenderpassSubpass1 RenderpassSubpass;
};

template <>
struct RenderPassTraits<RENDERING_TYPE_RENDERPASS2>
{
    typedef AttachmentDescription2 AttachmentDesc;
    typedef AttachmentReference2 AttachmentRef;
    typedef SubpassDescription2 SubpassDesc;
    typedef SubpassDependency2 SubpassDep;
    typedef RenderPassCreateInfo2 RenderPassCreateInfo;
    typedef RenderpassSubpass2 RenderpassSubpass;
};

// Template that can be used to construct required
// renderpasses using legacy renderpass and renderpass2.
template <RenderingType RenderingTypeValue>
class RenderPassWrapper : public RenderPassWrapperBase
{
    typedef typename RenderPassTraits<RenderingTypeValue>::AttachmentDesc AttachmentDesc;
    typedef typename RenderPassTraits<RenderingTypeValue>::AttachmentRef AttachmentRef;
    typedef typename RenderPassTraits<RenderingTypeValue>::SubpassDesc SubpassDesc;
    typedef typename RenderPassTraits<RenderingTypeValue>::SubpassDep SubpassDep;
    typedef typename RenderPassTraits<RenderingTypeValue>::RenderPassCreateInfo RenderPassCreateInfo;
    typedef typename RenderPassTraits<RenderingTypeValue>::RenderpassSubpass RenderpassSubpass;

public:
    RenderPassWrapper(const DeviceInterface &vk, const VkDevice vkDevice, const TestParams &testParams);
    ~RenderPassWrapper() = default;

    Move<VkRenderPass> createRenderPassProduceDynamicDensityMap(uint32_t viewMask) const override;
    Move<VkRenderPass> createRenderPassProduceSubsampledImage(uint32_t viewMask, bool makeCopySubpass,
                                                              bool resampleSubsampled) const override;
    Move<VkRenderPass> createRenderPassOutputSubsampledImage() const override;

    void cmdBeginRenderPass(VkCommandBuffer cmdBufferm, const VkRenderPassBeginInfo *pRenderPassBegin) const override;
    void cmdNextSubpass(VkCommandBuffer cmdBuffer) const override;
    void cmdEndRenderPass(VkCommandBuffer cmdBuffer, bool addZeroOffset = false,
                          uint32_t viewCount = 0u) const override;

private:
    const DeviceInterface &m_vk;
    const VkDevice m_vkDevice;
    const TestParams &m_testParams;

    const typename RenderpassSubpass::SubpassBeginInfo m_subpassBeginInfo;
    const typename RenderpassSubpass::SubpassEndInfo m_subpassEndInfo;
};

template <RenderingType RenderingTypeValue>
RenderPassWrapper<RenderingTypeValue>::RenderPassWrapper(const DeviceInterface &vk, const VkDevice vkDevice,
                                                         const TestParams &testParams)
    : RenderPassWrapperBase()
    , m_vk(vk)
    , m_vkDevice(vkDevice)
    , m_testParams(testParams)
    , m_subpassBeginInfo(nullptr, testParams.groupParams->useSecondaryCmdBuffer ?
                                      VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS :
                                      VK_SUBPASS_CONTENTS_INLINE)
    , m_subpassEndInfo(nullptr)
{
}

template <RenderingType RenderingTypeValue>
Move<VkRenderPass> RenderPassWrapper<RenderingTypeValue>::createRenderPassProduceDynamicDensityMap(
    uint32_t viewMask) const
{
    DE_ASSERT(m_testParams.dynamicDensityMap);

    std::vector<AttachmentDesc> attachmentDescriptions{{
        nullptr,                                         // const void*                        pNext
        (VkAttachmentDescriptionFlags)0,                 // VkAttachmentDescriptionFlags        flags
        m_testParams.densityMapFormat,                   // VkFormat                            format
        VK_SAMPLE_COUNT_1_BIT,                           // VkSampleCountFlagBits            samples
        VK_ATTACHMENT_LOAD_OP_CLEAR,                     // VkAttachmentLoadOp                loadOp
        VK_ATTACHMENT_STORE_OP_STORE,                    // VkAttachmentStoreOp                storeOp
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,                 // VkAttachmentLoadOp                stencilLoadOp
        VK_ATTACHMENT_STORE_OP_DONT_CARE,                // VkAttachmentStoreOp                stencilStoreOp
        VK_IMAGE_LAYOUT_UNDEFINED,                       // VkImageLayout                    initialLayout
        VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT // VkImageLayout                    finalLayout
    }};

    std::vector<AttachmentRef> colorAttachmentRefs{
        {nullptr, 0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT}};

    std::vector<SubpassDesc> subpassDescriptions{{
        nullptr,
        (VkSubpassDescriptionFlags)0,                      // VkSubpassDescriptionFlags        flags
        VK_PIPELINE_BIND_POINT_GRAPHICS,                   // VkPipelineBindPoint                pipelineBindPoint
        viewMask,                                          // uint32_t                            viewMask
        0u,                                                // uint32_t                            inputAttachmentCount
        nullptr,                                           // const VkAttachmentReference*        pInputAttachments
        static_cast<uint32_t>(colorAttachmentRefs.size()), // uint32_t                            colorAttachmentCount
        colorAttachmentRefs.data(),                        // const VkAttachmentReference*        pColorAttachments
        nullptr,                                           // const VkAttachmentReference*        pResolveAttachments
        nullptr, // const VkAttachmentReference*        pDepthStencilAttachment
        0u,      // uint32_t                            preserveAttachmentCount
        nullptr  // const uint32_t*                    pPreserveAttachments
    }};

    std::vector<SubpassDep> subpassDependencies{{
        nullptr,                                            // const void*                pNext
        0u,                                                 // uint32_t                    srcSubpass
        VK_SUBPASS_EXTERNAL,                                // uint32_t                    dstSubpass
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,      // VkPipelineStageFlags        srcStageMask
        VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT, // VkPipelineStageFlags        dstStageMask
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,               // VkAccessFlags            srcAccessMask
        VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT,        // VkAccessFlags            dstAccessMask
        VK_DEPENDENCY_BY_REGION_BIT,                        // VkDependencyFlags        dependencyFlags
        0u                                                  // int32_t                    viewOffset
    }};

    const RenderPassCreateInfo renderPassInfo(
        nullptr,                                              // const void*                        pNext
        (VkRenderPassCreateFlags)0,                           // VkRenderPassCreateFlags            flags
        static_cast<uint32_t>(attachmentDescriptions.size()), // uint32_t                            attachmentCount
        attachmentDescriptions.data(),                        // const VkAttachmentDescription*    pAttachments
        static_cast<uint32_t>(subpassDescriptions.size()),    // uint32_t                            subpassCount
        subpassDescriptions.data(),                           // const VkSubpassDescription*        pSubpasses
        static_cast<uint32_t>(subpassDependencies.size()),    // uint32_t                            dependencyCount
        subpassDependencies.empty() ? nullptr :
                                      subpassDependencies.data(), // const VkSubpassDependency*        pDependencies
        0u,     // uint32_t                            correlatedViewMaskCount
        nullptr // const uint32_t*                    pCorrelatedViewMasks
    );

    return renderPassInfo.createRenderPass(m_vk, m_vkDevice);
}

template <RenderingType RenderingTypeValue>
Move<VkRenderPass> RenderPassWrapper<RenderingTypeValue>::createRenderPassProduceSubsampledImage(
    uint32_t viewMask, bool makeCopySubpass, bool resampleSubsampled) const
{
    const void *constNullPtr            = nullptr;
    uint32_t multisampleAttachmentIndex = 0;
    uint32_t copyAttachmentIndex        = 0;
    uint32_t densityMapAttachmentIndex  = 0;
    uint32_t depthAttachmentIndex       = 0;

    // add color image
    VkAttachmentLoadOp loadOp = resampleSubsampled ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
    std::vector<AttachmentDesc> attachmentDescriptions{
        // Output color attachment
        {
            nullptr,                                 // const void*                        pNext
            (VkAttachmentDescriptionFlags)0,         // VkAttachmentDescriptionFlags        flags
            VK_FORMAT_R8G8B8A8_UNORM,                // VkFormat                            format
            m_testParams.colorSamples,               // VkSampleCountFlagBits            samples
            loadOp,                                  // VkAttachmentLoadOp                loadOp
            VK_ATTACHMENT_STORE_OP_STORE,            // VkAttachmentStoreOp                storeOp
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp                stencilLoadOp
            VK_ATTACHMENT_STORE_OP_DONT_CARE,        // VkAttachmentStoreOp                stencilStoreOp
            VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout                    initialLayout
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL // VkImageLayout                    finalLayout
        }};

    // add resolve image when we use more than one sample per fragment
    if (m_testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT)
    {
        multisampleAttachmentIndex = static_cast<uint32_t>(attachmentDescriptions.size());
        attachmentDescriptions.emplace_back(
            constNullPtr,                            // const void*                        pNext
            (VkAttachmentDescriptionFlags)0,         // VkAttachmentDescriptionFlags        flags
            VK_FORMAT_R8G8B8A8_UNORM,                // VkFormat                            format
            VK_SAMPLE_COUNT_1_BIT,                   // VkSampleCountFlagBits            samples
            VK_ATTACHMENT_LOAD_OP_CLEAR,             // VkAttachmentLoadOp                loadOp
            VK_ATTACHMENT_STORE_OP_STORE,            // VkAttachmentStoreOp                storeOp
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp                stencilLoadOp
            VK_ATTACHMENT_STORE_OP_DONT_CARE,        // VkAttachmentStoreOp                stencilStoreOp
            VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout                    initialLayout
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL // VkImageLayout                    finalLayout
        );
    }

    // add color image copy ( when render_copy is used )
    if (makeCopySubpass)
    {
        copyAttachmentIndex = static_cast<uint32_t>(attachmentDescriptions.size());
        attachmentDescriptions.emplace_back(
            constNullPtr,                            // const void*                        pNext
            (VkAttachmentDescriptionFlags)0,         // VkAttachmentDescriptionFlags        flags
            VK_FORMAT_R8G8B8A8_UNORM,                // VkFormat                            format
            m_testParams.colorSamples,               // VkSampleCountFlagBits            samples
            VK_ATTACHMENT_LOAD_OP_CLEAR,             // VkAttachmentLoadOp                loadOp
            VK_ATTACHMENT_STORE_OP_STORE,            // VkAttachmentStoreOp                storeOp
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp                stencilLoadOp
            VK_ATTACHMENT_STORE_OP_DONT_CARE,        // VkAttachmentStoreOp                stencilStoreOp
            VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout                    initialLayout
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL // VkImageLayout                    finalLayout
        );
    }

    // add density map
    densityMapAttachmentIndex = static_cast<uint32_t>(attachmentDescriptions.size());
    attachmentDescriptions.emplace_back(
        constNullPtr,                                     // const void*                        pNext
        (VkAttachmentDescriptionFlags)0,                  // VkAttachmentDescriptionFlags        flags
        m_testParams.densityMapFormat,                    // VkFormat                            format
        VK_SAMPLE_COUNT_1_BIT,                            // VkSampleCountFlagBits            samples
        VK_ATTACHMENT_LOAD_OP_LOAD,                       // VkAttachmentLoadOp                loadOp
        VK_ATTACHMENT_STORE_OP_DONT_CARE,                 // VkAttachmentStoreOp                storeOp
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,                  // VkAttachmentLoadOp                stencilLoadOp
        VK_ATTACHMENT_STORE_OP_DONT_CARE,                 // VkAttachmentStoreOp                stencilStoreOp
        VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT, // VkImageLayout                    initialLayout
        VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT  // VkImageLayout                    finalLayout
    );

    // add depth attachment if used for the image-producing variant
    const bool useDepthAttachment = (m_testParams.depthEnabled && !resampleSubsampled);

    if (useDepthAttachment)
    {
        depthAttachmentIndex = de::sizeU32(attachmentDescriptions);
        attachmentDescriptions.emplace_back(constNullPtr, 0u, m_testParams.depthFormat, m_testParams.colorSamples,
                                            loadOp, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                            VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
                                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    std::vector<AttachmentRef> colorAttachmentRefs0{
        {nullptr, 0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT}};

    // for multisampled scenario we need to add resolve attachment
    // (for makeCopy scenario it is used in second subpass)
    AttachmentRef *pResolveAttachments = nullptr;
    AttachmentRef resolveAttachmentRef{nullptr, multisampleAttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       VK_IMAGE_ASPECT_COLOR_BIT};
    if (m_testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT)
        pResolveAttachments = &resolveAttachmentRef;

    const auto tcuDepthFormat =
        (m_testParams.depthEnabled ?
             mapVkFormat(m_testParams.depthFormat) :
             mapVkFormat(VK_FORMAT_D16_UNORM)); // D16_UNORM makes sure we do not assert and have something valid below.
    const VkImageAspectFlags dsAspects =
        ((tcu::hasDepthComponent(tcuDepthFormat.order) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) |
         (tcu::hasStencilComponent(tcuDepthFormat.order) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0));
    AttachmentRef depthAttachmentRef{nullptr, depthAttachmentIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                     dsAspects};
    AttachmentRef *pDepthAttachment = (useDepthAttachment ? &depthAttachmentRef : nullptr);

    std::vector<SubpassDesc> subpassDescriptions{{
        nullptr,
        (VkSubpassDescriptionFlags)0,                       // VkSubpassDescriptionFlags    flags
        VK_PIPELINE_BIND_POINT_GRAPHICS,                    // VkPipelineBindPoint            pipelineBindPoint
        viewMask,                                           // uint32_t                        viewMask
        0u,                                                 // uint32_t                        inputAttachmentCount
        nullptr,                                            // const VkAttachmentReference*    pInputAttachments
        static_cast<uint32_t>(colorAttachmentRefs0.size()), // uint32_t                        colorAttachmentCount
        colorAttachmentRefs0.data(),                        // const VkAttachmentReference*    pColorAttachments
        makeCopySubpass ? nullptr : pResolveAttachments,    // const VkAttachmentReference*    pResolveAttachments
        pDepthAttachment,                                   // const VkAttachmentReference*    pDepthStencilAttachment
        0u,                                                 // uint32_t                        preserveAttachmentCount
        nullptr                                             // const uint32_t*                pPreserveAttachments
    }};

    std::vector<AttachmentRef> inputAttachmentRefs1{
        {nullptr, 0u, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT}};
    std::vector<AttachmentRef> colorAttachmentRefs1{
        {nullptr, copyAttachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT}};
    std::vector<SubpassDep> subpassDependencies;

    if (makeCopySubpass)
    {
        subpassDescriptions.push_back({
            nullptr,
            (VkSubpassDescriptionFlags)0,                       // VkSubpassDescriptionFlags    flags
            VK_PIPELINE_BIND_POINT_GRAPHICS,                    // VkPipelineBindPoint            pipelineBindPoint
            viewMask,                                           // uint32_t                        viewMask
            static_cast<uint32_t>(inputAttachmentRefs1.size()), // uint32_t                        inputAttachmentCount
            inputAttachmentRefs1.data(),                        // const VkAttachmentReference*    pInputAttachments
            static_cast<uint32_t>(colorAttachmentRefs1.size()), // uint32_t                        colorAttachmentCount
            colorAttachmentRefs1.data(),                        // const VkAttachmentReference*    pColorAttachments
            pResolveAttachments,                                // const VkAttachmentReference*    pResolveAttachments
            nullptr, // const VkAttachmentReference*    pDepthStencilAttachment
            0u,      // uint32_t                        preserveAttachmentCount
            nullptr  // const uint32_t*                pPreserveAttachments
        });

        VkDependencyFlags dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        if (m_testParams.viewCount > 1)
            dependencyFlags |= VK_DEPENDENCY_VIEW_LOCAL_BIT;

        subpassDependencies.emplace_back(
            constNullPtr,                                  // const void*                pNext
            0u,                                            // uint32_t                    srcSubpass
            1u,                                            // uint32_t                    dstSubpass
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // VkPipelineStageFlags        srcStageMask
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,         // VkPipelineStageFlags        dstStageMask
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // VkAccessFlags            srcAccessMask
            VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,           // VkAccessFlags            dstAccessMask
            dependencyFlags,                               // VkDependencyFlags        dependencyFlags
            0u                                             // int32_t                    viewOffset
        );
    }

    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    // for coarse reconstruction we need to put barrier on vertex stage
    if (m_testParams.coarseReconstruction)
        dstStageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;

    subpassDependencies.emplace_back(
        constNullPtr,                                           // const void*                pNext
        static_cast<uint32_t>(subpassDescriptions.size()) - 1u, // uint32_t                    srcSubpass
        VK_SUBPASS_EXTERNAL,                                    // uint32_t                    dstSubpass
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,          // VkPipelineStageFlags        srcStageMask
        dstStageMask,                                           // VkPipelineStageFlags        dstStageMask
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,                   // VkAccessFlags            srcAccessMask
        VK_ACCESS_SHADER_READ_BIT,                              // VkAccessFlags            dstAccessMask
        VK_DEPENDENCY_BY_REGION_BIT,                            // VkDependencyFlags        dependencyFlags
        0u                                                      // int32_t                    viewOffset
    );

    VkRenderPassFragmentDensityMapCreateInfoEXT renderPassFragmentDensityMap{
        VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT,
        nullptr,
        {densityMapAttachmentIndex, VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT}};

    void *renderPassInfoPNext = (void *)&renderPassFragmentDensityMap;

    const RenderPassCreateInfo renderPassInfo(
        renderPassInfoPNext,                                  // const void*                        pNext
        (VkRenderPassCreateFlags)0,                           // VkRenderPassCreateFlags            flags
        static_cast<uint32_t>(attachmentDescriptions.size()), // uint32_t                            attachmentCount
        attachmentDescriptions.data(),                        // const VkAttachmentDescription*    pAttachments
        static_cast<uint32_t>(subpassDescriptions.size()),    // uint32_t                            subpassCount
        subpassDescriptions.data(),                           // const VkSubpassDescription*        pSubpasses
        static_cast<uint32_t>(subpassDependencies.size()),    // uint32_t                            dependencyCount
        subpassDependencies.data(),                           // const VkSubpassDependency*        pDependencies
        0u,     // uint32_t                            correlatedViewMaskCount
        nullptr // const uint32_t*                    pCorrelatedViewMasks
    );

    return renderPassInfo.createRenderPass(m_vk, m_vkDevice);
}

template <RenderingType RenderingTypeValue>
Move<VkRenderPass> RenderPassWrapper<RenderingTypeValue>::createRenderPassOutputSubsampledImage() const
{
    // copy subsampled image to ordinary image - you cannot retrieve subsampled image to CPU in any way.
    // You must first convert it into plain image through rendering
    std::vector<AttachmentDesc> attachmentDescriptions{
        // output attachment
        {
            nullptr,                                 // const void*                        pNext
            (VkAttachmentDescriptionFlags)0,         // VkAttachmentDescriptionFlags        flags
            VK_FORMAT_R8G8B8A8_UNORM,                // VkFormat                            format
            VK_SAMPLE_COUNT_1_BIT,                   // VkSampleCountFlagBits            samples
            VK_ATTACHMENT_LOAD_OP_CLEAR,             // VkAttachmentLoadOp                loadOp
            VK_ATTACHMENT_STORE_OP_STORE,            // VkAttachmentStoreOp                storeOp
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp                stencilLoadOp
            VK_ATTACHMENT_STORE_OP_DONT_CARE,        // VkAttachmentStoreOp                stencilStoreOp
            VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout                    initialLayout
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout                    finalLayout
        },
    };

    std::vector<AttachmentRef> colorAttachmentRefs{
        {nullptr, 0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT}};

    std::vector<SubpassDesc> subpassDescriptions{{
        nullptr,
        (VkSubpassDescriptionFlags)0,                      // VkSubpassDescriptionFlags        flags
        VK_PIPELINE_BIND_POINT_GRAPHICS,                   // VkPipelineBindPoint                pipelineBindPoint
        0u,                                                // uint32_t                            viewMask
        0u,                                                // uint32_t                            inputAttachmentCount
        nullptr,                                           // const VkAttachmentReference*        pInputAttachments
        static_cast<uint32_t>(colorAttachmentRefs.size()), // uint32_t                            colorAttachmentCount
        colorAttachmentRefs.data(),                        // const VkAttachmentReference*        pColorAttachments
        nullptr,                                           // const VkAttachmentReference*        pResolveAttachments
        nullptr, // const VkAttachmentReference*        pDepthStencilAttachment
        0u,      // uint32_t                            preserveAttachmentCount
        nullptr  // const uint32_t*                    pPreserveAttachments
    }};

    const RenderPassCreateInfo renderPassInfo(
        nullptr,                                              // const void*                        pNext
        (VkRenderPassCreateFlags)0,                           // VkRenderPassCreateFlags            flags
        static_cast<uint32_t>(attachmentDescriptions.size()), // uint32_t                            attachmentCount
        attachmentDescriptions.data(),                        // const VkAttachmentDescription*    pAttachments
        static_cast<uint32_t>(subpassDescriptions.size()),    // uint32_t                            subpassCount
        subpassDescriptions.data(),                           // const VkSubpassDescription*        pSubpasses
        0,                                                    // uint32_t                            dependencyCount
        nullptr,                                              // const VkSubpassDependency*        pDependencies
        0u,     // uint32_t                            correlatedViewMaskCount
        nullptr // const uint32_t*                    pCorrelatedViewMasks
    );

    return renderPassInfo.createRenderPass(m_vk, m_vkDevice);
}

template <RenderingType RenderingTypeValue>
void RenderPassWrapper<RenderingTypeValue>::cmdBeginRenderPass(VkCommandBuffer cmdBuffer,
                                                               const VkRenderPassBeginInfo *pRenderPassBegin) const
{
    RenderpassSubpass::cmdBeginRenderPass(m_vk, cmdBuffer, pRenderPassBegin, &m_subpassBeginInfo);
}

template <RenderingType RenderingTypeValue>
void RenderPassWrapper<RenderingTypeValue>::cmdNextSubpass(VkCommandBuffer cmdBuffer) const
{
    RenderpassSubpass::cmdNextSubpass(m_vk, cmdBuffer, &m_subpassBeginInfo, &m_subpassEndInfo);
}

template <RenderingType RenderingTypeValue>
void RenderPassWrapper<RenderingTypeValue>::cmdEndRenderPass(VkCommandBuffer cmdBuffer, bool addZeroOffset,
                                                             uint32_t viewCount) const
{
    if (!addZeroOffset)
    {
        RenderpassSubpass::cmdEndRenderPass(m_vk, cmdBuffer, &m_subpassEndInfo);
        return;
    }

    DE_ASSERT(viewCount > 0u);
    const std::vector<VkOffset2D> offsets(viewCount, VkOffset2D{0, 0});

    const VkRenderPassFragmentDensityMapOffsetEndInfoEXT offsetEndInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_OFFSET_END_INFO_EXT,
        m_subpassEndInfo.getPNext(),
        de::sizeU32(offsets),
        de::dataOrNull(offsets),
    };

    typename RenderpassSubpass::SubpassEndInfo subpassEndInfoWithOffsets(&offsetEndInfo);
    RenderpassSubpass::cmdEndRenderPass(m_vk, cmdBuffer, &subpassEndInfoWithOffsets);
}

Move<VkFramebuffer> createImagelessFrameBuffer(const DeviceInterface &vk, VkDevice vkDevice, VkRenderPass renderPass,
                                               VkExtent3D size,
                                               const std::vector<VkFramebufferAttachmentImageInfo> &attachmentInfo)
{
    const uint32_t attachmentCount = static_cast<uint32_t>(attachmentInfo.size());
    const VkFramebufferAttachmentsCreateInfo framebufferAttachmentsCreateInfo{
        VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO, // VkStructureType sType;
        nullptr,                                               // const void* pNext;
        attachmentCount,                                       // uint32_t attachmentImageInfoCount;
        &attachmentInfo[0] // const VkFramebufferAttachmentImageInfo* pAttachmentImageInfos;
    };

    const VkFramebufferCreateInfo framebufferParams{
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
        &framebufferAttachmentsCreateInfo,         // const void* pNext;
        VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,       // VkFramebufferCreateFlags flags;
        renderPass,                                // VkRenderPass renderPass;
        attachmentCount,                           // uint32_t attachmentCount;
        nullptr,                                   // const VkImageView* pAttachments;
        size.width,                                // uint32_t width;
        size.height,                               // uint32_t height;
        1u                                         // uint32_t layers;
    };

    return createFramebuffer(vk, vkDevice, &framebufferParams);
}

Move<VkFramebuffer> createFrameBuffer(const DeviceInterface &vk, VkDevice vkDevice, VkRenderPass renderPass,
                                      VkExtent3D size, const std::vector<VkImageView> &imageViews)
{
    return makeFramebuffer(vk, vkDevice, renderPass, static_cast<uint32_t>(imageViews.size()), imageViews.data(),
                           size.width, size.height);
}

void copyBufferToImage(const DeviceInterface &vk, VkDevice device, VkQueue queue, uint32_t queueFamilyIndex,
                       const VkBuffer &buffer, VkDeviceSize bufferSize, const VkExtent3D &imageSize,
                       uint32_t arrayLayers, VkImage destImage)
{
    Move<VkCommandPool> cmdPool = createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    Move<VkFence> fence             = createFence(vk, device);
    VkImageLayout destImageLayout   = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
    VkPipelineStageFlags destImageDstStageFlags = VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT;
    VkAccessFlags finalAccessMask               = VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT;

    const VkCommandBufferBeginInfo cmdBufferBeginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, // VkCommandBufferUsageFlags flags;
        nullptr,
    };

    const VkBufferImageCopy copyRegion = {
        0,                                              // VkDeviceSize                    bufferOffset
        0,                                              // uint32_t                        bufferRowLength
        0,                                              // uint32_t                        bufferImageHeight
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, arrayLayers}, // VkImageSubresourceLayers        imageSubresource
        {0, 0, 0},                                      // VkOffset3D                    imageOffset
        imageSize                                       // VkExtent3D                    imageExtent
    };

    // Barriers for copying buffer to image
    const VkBufferMemoryBarrier preBufferBarrier =
        makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT,    // VkAccessFlags srcAccessMask;
                                VK_ACCESS_TRANSFER_READ_BIT, // VkAccessFlags dstAccessMask;
                                buffer,                      // VkBuffer buffer;
                                0u,                          // VkDeviceSize offset;
                                bufferSize                   // VkDeviceSize size;
        );

    const VkImageSubresourceRange subresourceRange{
        // VkImageSubresourceRange subresourceRange;
        VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspect;
        0u,                        // uint32_t baseMipLevel;
        1u,                        // uint32_t mipLevels;
        0u,                        // uint32_t baseArraySlice;
        arrayLayers                // uint32_t arraySize;
    };

    const VkImageMemoryBarrier preImageBarrier =
        makeImageMemoryBarrier(0u,                                   // VkAccessFlags srcAccessMask;
                               VK_ACCESS_TRANSFER_WRITE_BIT,         // VkAccessFlags dstAccessMask;
                               VK_IMAGE_LAYOUT_UNDEFINED,            // VkImageLayout oldLayout;
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // VkImageLayout newLayout;
                               destImage,                            // VkImage image;
                               subresourceRange                      // VkImageSubresourceRange subresourceRange;
        );

    const VkImageMemoryBarrier postImageBarrier =
        makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,         // VkAccessFlags srcAccessMask;
                               finalAccessMask,                      // VkAccessFlags dstAccessMask;
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // VkImageLayout oldLayout;
                               destImageLayout,                      // VkImageLayout newLayout;
                               destImage,                            // VkImage image;
                               subresourceRange                      // VkImageSubresourceRange subresourceRange;
        );

    VK_CHECK(vk.beginCommandBuffer(*cmdBuffer, &cmdBufferBeginInfo));
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0,
                          0, nullptr, 1, &preBufferBarrier, 1, &preImageBarrier);
    vk.cmdCopyBufferToImage(*cmdBuffer, buffer, destImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, destImageDstStageFlags, (VkDependencyFlags)0, 0,
                          nullptr, 0, nullptr, 1, &postImageBarrier);
    VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

    const VkPipelineStageFlags pipelineStageFlags = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;

    const VkSubmitInfo submitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType sType;
        nullptr,                       // const void* pNext;
        0u,                            // uint32_t waitSemaphoreCount;
        nullptr,                       // const VkSemaphore* pWaitSemaphores;
        &pipelineStageFlags,           // const VkPipelineStageFlags* pWaitDstStageMask;
        1u,                            // uint32_t commandBufferCount;
        &cmdBuffer.get(),              // const VkCommandBuffer* pCommandBuffers;
        0u,                            // uint32_t signalSemaphoreCount;
        nullptr                        // const VkSemaphore* pSignalSemaphores;
    };

    try
    {
        VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *fence));
        VK_CHECK(vk.waitForFences(device, 1, &fence.get(), true, ~(0ull) /* infinity */));
    }
    catch (...)
    {
        VK_CHECK(vk.deviceWaitIdle(device));
        throw;
    }
}

Move<VkPipeline> buildGraphicsPipeline(const DeviceInterface &vk, const VkDevice device,
                                       const VkPipelineLayout pipelineLayout, const VkShaderModule vertexShaderModule,
                                       const VkShaderModule fragmentShaderModule, const VkRenderPass renderPass,
                                       const std::vector<VkViewport> &viewportVect,
                                       const std::vector<VkRect2D> &scissorVect, const uint32_t subpass,
                                       const VkPipelineMultisampleStateCreateInfo *multisampleStateCreateInfo,
                                       const void *pNext, const bool useDensityMapAttachment,
                                       const bool useDepthAttachment, const bool useMaintenance5 = false)
{
    std::vector<VkPipelineShaderStageCreateInfo> pipelineShaderStageParams(
        2,
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType                        sType
            nullptr,                                             // const void*                            pNext
            0u,                                                  // VkPipelineShaderStageCreateFlags        flags
            VK_SHADER_STAGE_VERTEX_BIT,                          // VkShaderStageFlagBits                stage
            vertexShaderModule,                                  // VkShaderModule                        module
            "main",                                              // const char*                            pName
            nullptr // const VkSpecializationInfo*            pSpecializationInfo
        });
    pipelineShaderStageParams[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    pipelineShaderStageParams[1].module = fragmentShaderModule;

    const VkVertexInputBindingDescription vertexInputBindingDescription{
        0u,                         // uint32_t binding;
        sizeof(Vertex4RGBA),        // uint32_t strideInBytes;
        VK_VERTEX_INPUT_RATE_VERTEX // VkVertexInputStepRate inputRate;
    };

    std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions{
        {0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u},
        {1u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, (uint32_t)(sizeof(float) * 4)},
        {2u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, (uint32_t)(sizeof(float) * 8)}};

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                   // const void* pNext;
        0u,                                                        // VkPipelineVertexInputStateCreateFlags flags;
        1u,                                                        // uint32_t vertexBindingDescriptionCount;
        &vertexInputBindingDescription, // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        static_cast<uint32_t>(vertexInputAttributeDescriptions.size()), // uint32_t vertexAttributeDescriptionCount;
        vertexInputAttributeDescriptions
            .data() // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                                                     // const void*                                pNext
        0u,                                                          // VkPipelineInputAssemblyStateCreateFlags    flags
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // VkPrimitiveTopology                        topology
        VK_FALSE                             // VkBool32                                    primitiveRestartEnable
    };

    const VkPipelineViewportStateCreateInfo viewportStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                                               // const void*                                pNext
        (VkPipelineViewportStateCreateFlags)0,                 // VkPipelineViewportStateCreateFlags        flags
        (uint32_t)viewportVect.size(), // uint32_t                                    viewportCount
        viewportVect.data(),           // const VkViewport*                        pViewports
        (uint32_t)scissorVect.size(),  // uint32_t                                    scissorCount
        scissorVect.data()             // const VkRect2D*                            pScissors
    };

    const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfoDefault{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                                                    // const void*                                pNext
        0u,                                                         // VkPipelineRasterizationStateCreateFlags    flags
        VK_FALSE,                        // VkBool32                                    depthClampEnable
        VK_FALSE,                        // VkBool32                                    rasterizerDiscardEnable
        VK_POLYGON_MODE_FILL,            // VkPolygonMode                            polygonMode
        VK_CULL_MODE_NONE,               // VkCullModeFlags                            cullMode
        VK_FRONT_FACE_COUNTER_CLOCKWISE, // VkFrontFace                                frontFace
        VK_FALSE,                        // VkBool32                                    depthBiasEnable
        0.0f,                            // float                                    depthBiasConstantFactor
        0.0f,                            // float                                    depthBiasClamp
        0.0f,                            // float                                    depthBiasSlopeFactor
        1.0f                             // float                                    lineWidth
    };

    const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfoDefault{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                                                  // const void*                                pNext
        0u,                                                       // VkPipelineMultisampleStateCreateFlags    flags
        VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits                    rasterizationSamples
        VK_FALSE,              // VkBool32                                    sampleShadingEnable
        1.0f,                  // float                                    minSampleShading
        nullptr,               // const VkSampleMask*                        pSampleMask
        VK_FALSE,              // VkBool32                                    alphaToCoverageEnable
        VK_FALSE               // VkBool32                                    alphaToOneEnable
    };

    const VkStencilOpState stencilOpState{
        VK_STENCIL_OP_KEEP,  // VkStencilOp        failOp
        VK_STENCIL_OP_KEEP,  // VkStencilOp        passOp
        VK_STENCIL_OP_KEEP,  // VkStencilOp        depthFailOp
        VK_COMPARE_OP_NEVER, // VkCompareOp        compareOp
        0,                   // uint32_t            compareMask
        0,                   // uint32_t            writeMask
        0                    // uint32_t            reference
    };

    const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfoDefault{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                                                    // const void*                                pNext
        0u,                                                         // VkPipelineDepthStencilStateCreateFlags    flags
        useDepthAttachment ? VK_TRUE : VK_FALSE, // VkBool32                                    depthTestEnable
        useDepthAttachment ? VK_TRUE : VK_FALSE, // VkBool32                                    depthWriteEnable
        VK_COMPARE_OP_LESS_OR_EQUAL,             // VkCompareOp                                depthCompareOp
        VK_FALSE,                                // VkBool32                                    depthBoundsTestEnable
        VK_FALSE,                                // VkBool32                                    stencilTestEnable
        stencilOpState,                          // VkStencilOpState                            front
        stencilOpState,                          // VkStencilOpState                            back
        0.0f,                                    // float                                    minDepthBounds
        1.0f,                                    // float                                    maxDepthBounds
    };

    const std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates(
        2,
        {VK_FALSE,                // VkBool32                    blendEnable
         VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            srcColorBlendFactor
         VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            dstColorBlendFactor
         VK_BLEND_OP_ADD,         // VkBlendOp                colorBlendOp
         VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            srcAlphaBlendFactor
         VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            dstAlphaBlendFactor
         VK_BLEND_OP_ADD,         // VkBlendOp                alphaBlendOp
         VK_COLOR_COMPONENT_R_BIT // VkColorComponentFlags    colorWriteMask
             | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT});

    uint32_t attachmentCount = 1u;
    if (pNext)
    {
        const auto *pipelineRenderingCreateInfo = reinterpret_cast<const VkPipelineRenderingCreateInfoKHR *>(pNext);
        DE_ASSERT(pipelineRenderingCreateInfo->sType == VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR);
        attachmentCount = pipelineRenderingCreateInfo->colorAttachmentCount;
        DE_ASSERT(attachmentCount <= colorBlendAttachmentStates.size());
    }

    const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfoDefault{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType                                sType
        nullptr,                           // const void*                                    pNext
        0u,                                // VkPipelineColorBlendStateCreateFlags            flags
        VK_FALSE,                          // VkBool32                                        logicOpEnable
        VK_LOGIC_OP_CLEAR,                 // VkLogicOp                                    logicOp
        attachmentCount,                   // uint32_t                                        attachmentCount
        colorBlendAttachmentStates.data(), // const VkPipelineColorBlendAttachmentState*    pAttachments
        {0.0f, 0.0f, 0.0f, 0.0f}           // float                                        blendConstants[4]
    };

    VkGraphicsPipelineCreateInfo pipelineCreateInfo{
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, // VkStructureType                                    sType
        pNext,                                           // const void*                                        pNext
        (useDensityMapAttachment ?
             uint32_t(VK_PIPELINE_RASTERIZATION_STATE_CREATE_FRAGMENT_DENSITY_MAP_ATTACHMENT_BIT_EXT) :
             0u),                                   // VkPipelineCreateFlags                            flags
        (uint32_t)pipelineShaderStageParams.size(), // uint32_t                                            stageCount
        &pipelineShaderStageParams[0],              // const VkPipelineShaderStageCreateInfo*            pStages
        &vertexInputStateCreateInfo,          // const VkPipelineVertexInputStateCreateInfo*        pVertexInputState
        &inputAssemblyStateCreateInfo,        // const VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState
        nullptr,                              // const VkPipelineTessellationStateCreateInfo*        pTessellationState
        &viewportStateCreateInfo,             // const VkPipelineViewportStateCreateInfo*            pViewportState
        &rasterizationStateCreateInfoDefault, // const VkPipelineRasterizationStateCreateInfo*    pRasterizationState
        multisampleStateCreateInfo ?
            multisampleStateCreateInfo :
            &multisampleStateCreateInfoDefault, // const VkPipelineMultisampleStateCreateInfo*        pMultisampleState
        &depthStencilStateCreateInfoDefault, // const VkPipelineDepthStencilStateCreateInfo*        pDepthStencilState
        &colorBlendStateCreateInfoDefault,   // const VkPipelineColorBlendStateCreateInfo*        pColorBlendState
        nullptr,                             // const VkPipelineDynamicStateCreateInfo*            pDynamicState
        pipelineLayout,                      // VkPipelineLayout                                    layout
        renderPass,                          // VkRenderPass                                        renderPass
        subpass,                             // uint32_t                                            subpass
        VK_NULL_HANDLE,                      // VkPipeline                                        basePipelineHandle
        0                                    // int32_t basePipelineIndex;
    };

    VkPipelineCreateFlags2CreateInfoKHR pipelineFlags2CreateInfo{};
    if (useDensityMapAttachment && useMaintenance5)
    {
        pipelineFlags2CreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR;
        pipelineFlags2CreateInfo.flags = VK_PIPELINE_CREATE_2_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_BIT_EXT;
        pipelineFlags2CreateInfo.pNext = pipelineCreateInfo.pNext;
        pipelineCreateInfo.pNext       = &pipelineFlags2CreateInfo;
        pipelineCreateInfo.flags       = 0;
    }

    return createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);
}

class FragmentDensityMapTest : public vkt::TestCase
{
public:
    FragmentDensityMapTest(tcu::TestContext &testContext, const std::string &name, const TestParams &testParams);
    virtual void initPrograms(SourceCollections &sourceCollections) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    const TestParams m_testParams;
};

class FragmentDensityMapTestInstance : public vkt::TestInstance
{
public:
    FragmentDensityMapTestInstance(Context &context, const TestParams &testParams);
    virtual tcu::TestStatus iterate(void);

private:
    typedef std::shared_ptr<RenderPassWrapperBase> RenderPassWrapperBasePtr;

    void drawDynamicDensityMap(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);
    void drawSubsampledImage(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);
    void drawCopySubsampledImage(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);
    void drawResampleSubsampledImage(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);
    void drawOutputSubsampledImage(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);
    void remapingBeforeCopySubsampledImage(const DeviceInterface &vk, VkCommandBuffer cmdBuffer);
    void createCommandBufferForRenderpass(const DeviceInterface &vk, VkDevice device,
                                          RenderPassWrapperBasePtr renderPassWrapper, const VkExtent3D &colorImageSize,
                                          const VkRect2D &dynamicDensityMapRenderArea,
                                          const VkRect2D &colorImageRenderArea, const VkRect2D &outputRenderArea);
    void createCommandBufferForDynamicRendering(const DeviceInterface &vk, VkDevice vkDevice,
                                                const VkRect2D &dynamicDensityMapRenderArea,
                                                const VkRect2D &colorImageRenderArea, const VkRect2D &outputRenderArea);
    void endRendering(const DeviceInterface &vk, VkCommandBuffer cmdBuffer, bool addZeroOffset = false,
                      uint32_t viewCount = 0u);
    tcu::TestStatus verifyImage(const DeviceHelper &deviceHelper);

private:
    typedef de::SharedPtr<Unique<VkSampler>> VkSamplerSp;
    typedef de::SharedPtr<Unique<VkImage>> VkImageSp;
    typedef de::SharedPtr<Allocation> AllocationSp;
    typedef de::SharedPtr<Unique<VkImageView>> VkImageViewSp;

    TestParams m_testParams;
    tcu::UVec2 m_renderSize;
    tcu::Vec2 m_densityValue;
    uint32_t m_viewMask;

    Move<VkCommandPool> m_cmdPool;

    std::vector<VkImageSp> m_densityMapImages;
    std::vector<AllocationSp> m_densityMapImageAllocs;
    std::vector<VkImageViewSp> m_densityMapImageViews;

    Move<VkImage> m_colorImage;
    de::MovePtr<Allocation> m_colorImageAlloc;
    Move<VkImageView> m_colorImageView;

    Move<VkImage> m_colorCopyImage;
    de::MovePtr<Allocation> m_colorCopyImageAlloc;
    Move<VkImageView> m_colorCopyImageView;

    Move<VkImage> m_depthImage;
    de::MovePtr<Allocation> m_depthImageAlloc;
    Move<VkImageView> m_depthImageView;

    Move<VkImage> m_colorResolvedImage;
    de::MovePtr<Allocation> m_colorResolvedImageAlloc;
    Move<VkImageView> m_colorResolvedImageView;

    Move<VkImage> m_outputImage;
    de::MovePtr<Allocation> m_outputImageAlloc;
    Move<VkImageView> m_outputImageView;

    std::vector<VkSamplerSp> m_colorSamplers;

    Move<VkRenderPass> m_renderPassProduceDynamicDensityMap;
    Move<VkRenderPass> m_renderPassProduceSubsampledImage;
    Move<VkRenderPass> m_renderPassUpdateSubsampledImage;
    Move<VkRenderPass> m_renderPassOutputSubsampledImage;
    Move<VkFramebuffer> m_framebufferProduceDynamicDensityMap;
    Move<VkFramebuffer> m_framebufferProduceSubsampledImage;
    Move<VkFramebuffer> m_framebufferUpdateSubsampledImage;
    Move<VkFramebuffer> m_framebufferOutputSubsampledImage;

    Move<VkDescriptorSetLayout> m_descriptorSetLayoutProduceSubsampled;

    Move<VkDescriptorSetLayout> m_descriptorSetLayoutOperateOnSubsampledImage;
    Move<VkDescriptorPool> m_descriptorPoolOperateOnSubsampledImage;
    Move<VkDescriptorSet> m_descriptorSetOperateOnSubsampledImage;

    Move<VkDescriptorSetLayout> m_descriptorSetLayoutOutputSubsampledImage;
    Move<VkDescriptorPool> m_descriptorPoolOutputSubsampledImage;
    Move<VkDescriptorSet> m_descriptorSetOutputSubsampledImage;

    Move<VkShaderModule> m_vertexCommonShaderModule;
    Move<VkShaderModule> m_fragmentShaderModuleProduceSubsampledImage;
    Move<VkShaderModule> m_fragmentShaderModuleCopySubsampledImage;
    Move<VkShaderModule> m_fragmentShaderModuleUpdateSubsampledImage;
    Move<VkShaderModule> m_fragmentShaderModuleOutputSubsampledImage;

    std::vector<Vertex4RGBA> m_verticesDDM;
    Move<VkBuffer> m_vertexBufferDDM;
    de::MovePtr<Allocation> m_vertexBufferAllocDDM;

    std::vector<Vertex4RGBA> m_vertices;
    Move<VkBuffer> m_vertexBuffer;
    de::MovePtr<Allocation> m_vertexBufferAlloc;

    std::vector<Vertex4RGBA> m_verticesOutput;
    Move<VkBuffer> m_vertexBufferOutput;
    de::MovePtr<Allocation> m_vertexBufferOutputAlloc;

    Move<VkPipelineLayout> m_pipelineLayoutNoDescriptors;
    Move<VkPipelineLayout> m_pipelineLayoutOperateOnSubsampledImage;
    Move<VkPipelineLayout> m_pipelineLayoutOutputSubsampledImage;
    Move<VkPipeline> m_graphicsPipelineProduceDynamicDensityMap;
    Move<VkPipeline> m_graphicsPipelineProduceSubsampledImage;
    Move<VkPipeline> m_graphicsPipelineCopySubsampledImage;
    Move<VkPipeline> m_graphicsPipelineUpdateSubsampledImage;
    Move<VkPipeline> m_graphicsPipelineOutputSubsampledImage;

    Move<VkCommandBuffer> m_cmdBuffer;
    Move<VkCommandBuffer> m_dynamicDensityMapSecCmdBuffer;
    Move<VkCommandBuffer> m_subsampledImageSecCmdBuffer;
    Move<VkCommandBuffer> m_resampleSubsampledImageSecCmdBuffer;
    Move<VkCommandBuffer> m_outputSubsampledImageSecCmdBuffer;
};

FragmentDensityMapTest::FragmentDensityMapTest(tcu::TestContext &testContext, const std::string &name,
                                               const TestParams &testParams)
    : vkt::TestCase(testContext, name)
    , m_testParams(testParams)
{
    DE_ASSERT(testParams.samplersCount > 0);
}

void FragmentDensityMapTest::initPrograms(SourceCollections &sourceCollections) const
{
    const std::string vertSourceTemplate("#version 450\n"
                                         "#extension GL_EXT_multiview : enable\n"
                                         "${EXTENSIONS}"
                                         "layout(location = 0) in  vec4 inPosition;\n"
                                         "layout(location = 1) in  vec4 inUV;\n"
                                         "layout(location = 2) in  vec4 inColor;\n"
                                         "layout(location = 0) out vec4 outUV;\n"
                                         "layout(location = 1) out vec4 outColor;\n"
                                         "out gl_PerVertex\n"
                                         "{\n"
                                         "  vec4 gl_Position;\n"
                                         "};\n"
                                         "void main(void)\n"
                                         "{\n"
                                         "    gl_Position = inPosition;\n"
                                         "    outUV = inUV;\n"
                                         "    outColor = inColor;\n"
                                         "    ${OPERATION}"
                                         "}\n");

    std::map<std::string, std::string> parameters{{"EXTENSIONS", ""}, {"OPERATION", ""}};
    if (m_testParams.multiViewport)
    {
        parameters["EXTENSIONS"] = "#extension GL_ARB_shader_viewport_layer_array : enable\n";
        parameters["OPERATION"]  = "gl_ViewportIndex = gl_ViewIndex;\n";
    }
    sourceCollections.glslSources.add("vert")
        << glu::VertexSource(tcu::StringTemplate(vertSourceTemplate).specialize(parameters));

    sourceCollections.glslSources.add("frag_produce_subsampled") << glu::FragmentSource(
        "#version 450\n"
        "#extension GL_EXT_fragment_invocation_density : enable\n"
        "#extension GL_EXT_multiview : enable\n"
        "layout(location = 0) in vec4 inUV;\n"
        "layout(location = 1) in vec4 inColor;\n"
        "layout(location = 0) out vec4 fragColor;\n"
        "void main(void)\n"
        "{\n"
        "    fragColor = vec4(inColor.x, inColor.y, 1.0/float(gl_FragSizeEXT.x), 1.0/(gl_FragSizeEXT.y));\n"
        "}\n");

    sourceCollections.glslSources.add("frag_update_subsampled") << glu::FragmentSource(
        "#version 450\n"
        "#extension GL_EXT_fragment_invocation_density : enable\n"
        "#extension GL_EXT_multiview : enable\n"
        "layout(location = 0) in vec4 inUV;\n"
        "layout(location = 1) in vec4 inColor;\n"
        "layout(location = 0) out vec4 fragColor;\n"
        "void main(void)\n"
        "{\n"
        "    if (gl_FragCoord.y < 0.5)\n"
        "        discard;\n"
        "    fragColor = vec4(inColor.x, inColor.y, 1.0/float(gl_FragSizeEXT.x), 1.0/(gl_FragSizeEXT.y));\n"
        "}\n");

    sourceCollections.glslSources.add("frag_copy_subsampled") << glu::FragmentSource(
        "#version 450\n"
        "#extension GL_EXT_fragment_invocation_density : enable\n"
        "#extension GL_EXT_multiview : enable\n"
        "layout(location = 0) in vec4 inUV;\n"
        "layout(location = 1) in vec4 inColor;\n"
        "layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputAtt;\n"
        "layout(location = 0) out vec4 fragColor;\n"
        "void main(void)\n"
        "{\n"
        "    fragColor = subpassLoad(inputAtt);\n"
        "}\n");

    sourceCollections.glslSources.add("frag_copy_subsampled_ms") << glu::FragmentSource(
        "#version 450\n"
        "#extension GL_EXT_fragment_invocation_density : enable\n"
        "#extension GL_EXT_multiview : enable\n"
        "layout(location = 0) in vec4 inUV;\n"
        "layout(location = 1) in vec4 inColor;\n"
        "layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInputMS inputAtt;\n"
        "layout(location = 0) out vec4 fragColor;\n"
        "void main(void)\n"
        "{\n"
        "    fragColor = subpassLoad(inputAtt, gl_SampleID);\n"
        "}\n");

    const char *samplersDefTemplate = "layout(binding = ${BINDING})  uniform ${SAMPLER} subsampledImage${BINDING};\n";
    const char *sumColorsTemplate   = "    fragColor += texture(subsampledImage${BINDING}, inUV.${COMPONENTS});\n";

    const char *densitymapOutputTemplate = "#version 450\n"
                                           "layout(location = 0) in vec4 inUV;\n"
                                           "layout(location = 1) in vec4 inColor;\n"
                                           "${SAMPLERS_DEF}"
                                           "layout(location = 0) out vec4 fragColor;\n"
                                           "void main(void)\n"
                                           "{\n"
                                           "    fragColor = vec4(0);\n"
                                           "${SUM_COLORS}"
                                           "    fragColor /= float(${COUNT});\n"
                                           "}\n";

    parameters = {
        {"SAMPLER", ""},      {"BINDING", ""},
        {"COMPONENTS", ""},   {"COUNT", std::to_string(m_testParams.samplersCount)},
        {"SAMPLERS_DEF", ""}, {"SUM_COLORS", ""},
    };

    std::string sampler2dDefs;
    std::string sampler2dSumColors;
    std::string sampler2dArrayDefs;
    std::string sampler2dArraySumColors;
    for (uint32_t samplerIndex = 0; samplerIndex < m_testParams.samplersCount; ++samplerIndex)
    {
        parameters["BINDING"] = std::to_string(samplerIndex);

        parameters["COMPONENTS"] = "xy";
        parameters["SAMPLER"]    = "sampler2D";
        sampler2dDefs += tcu::StringTemplate(samplersDefTemplate).specialize(parameters);
        sampler2dSumColors += tcu::StringTemplate(sumColorsTemplate).specialize(parameters);

        parameters["COMPONENTS"] = "xyz";
        parameters["SAMPLER"]    = "sampler2DArray";
        sampler2dArrayDefs += tcu::StringTemplate(samplersDefTemplate).specialize(parameters);
        sampler2dArraySumColors += tcu::StringTemplate(sumColorsTemplate).specialize(parameters);
    }

    parameters["SAMPLERS_DEF"] = sampler2dDefs;
    parameters["SUM_COLORS"]   = sampler2dSumColors;
    sourceCollections.glslSources.add("frag_output_2d")
        << glu::FragmentSource(tcu::StringTemplate(densitymapOutputTemplate).specialize(parameters));

    parameters["SAMPLERS_DEF"] = sampler2dArrayDefs;
    parameters["SUM_COLORS"]   = sampler2dArraySumColors;
    sourceCollections.glslSources.add("frag_output_2darray")
        << glu::FragmentSource(tcu::StringTemplate(densitymapOutputTemplate).specialize(parameters));
}

TestInstance *FragmentDensityMapTest::createInstance(Context &context) const
{
    return new FragmentDensityMapTestInstance(context, m_testParams);
}

void FragmentDensityMapTest::checkSupport(Context &context) const
{
    const InstanceInterface &vki            = context.getInstanceInterface();
    const VkPhysicalDevice vkPhysicalDevice = context.getPhysicalDevice();

    context.requireDeviceFunctionality("VK_EXT_fragment_density_map");

    if (m_testParams.addZeroOffset)
    {
        DE_ASSERT(m_testParams.groupParams->renderingType != RENDERING_TYPE_RENDERPASS_LEGACY);

#ifdef USE_QCOM_OFFSET_EXT
        context.requireDeviceFunctionality("VK_QCOM_fragment_density_map_offset");
#else
        context.requireDeviceFunctionality("VK_EXT_fragment_density_map_offset");
#endif
    }

    if (m_testParams.groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
    {
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
        if (m_testParams.makeCopy)
        {
            context.requireDeviceFunctionality("VK_KHR_dynamic_rendering_local_read");

            if ((m_testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT) &&
                (context.getEquivalentApiVersion() > VK_API_VERSION_1_3) &&
                !context.getDeviceVulkan14Properties().dynamicRenderingLocalReadMultisampledAttachments)
                TCU_THROW(NotSupportedError, "dynamicRenderingLocalReadMultisampledAttachments not supported");
        }
    }

    if (m_testParams.imagelessFramebuffer)
        context.requireDeviceFunctionality("VK_KHR_imageless_framebuffer");

    if (m_testParams.useMaintenance5)
        context.requireDeviceFunctionality("VK_KHR_maintenance5");

    VkPhysicalDeviceFragmentDensityMapFeaturesEXT fragmentDensityMapFeatures = initVulkanStructure();
    VkPhysicalDeviceFragmentDensityMap2FeaturesEXT fragmentDensityMap2Features =
        initVulkanStructure(&fragmentDensityMapFeatures);
    VkPhysicalDeviceFeatures2KHR features2 = initVulkanStructure(&fragmentDensityMap2Features);

    context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

    const auto &fragmentDensityMap2Properties = context.getFragmentDensityMap2PropertiesEXT();

    if (!fragmentDensityMapFeatures.fragmentDensityMap)
        TCU_THROW(NotSupportedError, "fragmentDensityMap feature is not supported");
    if (m_testParams.dynamicDensityMap && !fragmentDensityMapFeatures.fragmentDensityMapDynamic)
        TCU_THROW(NotSupportedError, "fragmentDensityMapDynamic feature is not supported");
    if (m_testParams.nonSubsampledImages && !fragmentDensityMapFeatures.fragmentDensityMapNonSubsampledImages)
        TCU_THROW(NotSupportedError, "fragmentDensityMapNonSubsampledImages feature is not supported");

    if (m_testParams.deferredDensityMap)
    {
        context.requireDeviceFunctionality("VK_EXT_fragment_density_map2");
        if (!fragmentDensityMap2Features.fragmentDensityMapDeferred)
            TCU_THROW(NotSupportedError, "fragmentDensityMapDeferred feature is not supported");
    }
    if (m_testParams.subsampledLoads)
    {
        context.requireDeviceFunctionality("VK_EXT_fragment_density_map2");
        if (!fragmentDensityMap2Properties.subsampledLoads)
            TCU_THROW(NotSupportedError, "subsampledLoads property is not supported");
    }
    if (m_testParams.coarseReconstruction)
    {
        context.requireDeviceFunctionality("VK_EXT_fragment_density_map2");
        if (!fragmentDensityMap2Properties.subsampledCoarseReconstructionEarlyAccess)
            TCU_THROW(NotSupportedError, "subsampledCoarseReconstructionEarlyAccess property is not supported");
    }

    if (m_testParams.viewCount > 1)
    {
        context.requireDeviceFunctionality("VK_KHR_multiview");
        if (!context.getMultiviewFeatures().multiview)
            TCU_THROW(NotSupportedError, "Implementation does not support multiview feature");

        if (m_testParams.viewCount > 2)
        {
            context.requireDeviceFunctionality("VK_EXT_fragment_density_map2");
            if (m_testParams.viewCount > fragmentDensityMap2Properties.maxSubsampledArrayLayers)
                TCU_THROW(
                    NotSupportedError,
                    "Maximum number of VkImageView array layers for usages supporting subsampled samplers is to small");
        }
    }

    if (m_testParams.multiViewport)
    {
        context.requireDeviceFunctionality("VK_EXT_shader_viewport_index_layer");
        if (!context.getDeviceFeatures().multiViewport)
            TCU_THROW(NotSupportedError, "multiViewport not supported");
    }

    if (!m_testParams.nonSubsampledImages && (m_testParams.samplersCount > 1))
    {
        context.requireDeviceFunctionality("VK_EXT_fragment_density_map2");
        if (m_testParams.samplersCount > fragmentDensityMap2Properties.maxDescriptorSetSubsampledSamplers)
            TCU_THROW(NotSupportedError, "Required number of subsampled samplers is not supported");
    }

    vk::VkImageUsageFlags colorImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if (m_testParams.makeCopy)
        colorImageUsage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

    uint32_t colorImageCreateFlags =
        m_testParams.nonSubsampledImages ? 0u : (uint32_t)VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT;
    VkImageFormatProperties imageFormatProperties(
        getPhysicalDeviceImageFormatProperties(vki, vkPhysicalDevice, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D,
                                               VK_IMAGE_TILING_OPTIMAL, colorImageUsage, colorImageCreateFlags));

    if ((imageFormatProperties.sampleCounts & m_testParams.colorSamples) == 0)
        TCU_THROW(NotSupportedError, "Color image type not supported");

    if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
        !context.getPortabilitySubsetFeatures().multisampleArrayImage &&
        (m_testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT) && (m_testParams.viewCount != 1))
    {
        TCU_THROW(
            NotSupportedError,
            "VK_KHR_portability_subset: Implementation does not support image array with multiple samples per texel");
    }

    if (m_testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);
}

tcu::Vec2 getFormatDelta(VkFormat densityMapFormat)
{
    // A generic solution could use tcu::getTextureChannelClass, tcu::getTextureFormatBitDepth, etc to calculate a
    // precision depending on the format and the format type. See vktPipelineSamplerBorderSwizzleTests.cpp for an
    // example.
    switch (densityMapFormat)
    {
    case VK_FORMAT_R8G8_UNORM:
    {
        const float prec = 1.0f / 255.0f;
        return tcu::Vec2(prec, prec);
    }
    default:
        break;
    }

    DE_ASSERT(false);
    return tcu::Vec2(0.0f);
}

FragmentDensityMapTestInstance::FragmentDensityMapTestInstance(Context &context, const TestParams &testParams)
    : vkt::TestInstance(context)
    , m_testParams(testParams)
{
    m_renderSize = tcu::UVec2(
        de::roundDown(
            deFloorFloatToInt32(m_testParams.renderMultiplier * static_cast<float>(m_testParams.densityMapSize.x())),
            (int)m_testParams.viewCount),
        deFloorFloatToInt32(m_testParams.renderMultiplier * static_cast<float>(m_testParams.densityMapSize.y())));
    const auto densityValueDelta = getFormatDelta(m_testParams.densityMapFormat);
    const auto areaFloat         = m_testParams.fragmentArea.asFloat();
    // This delta adjustment makes sure that the divison by m_densityValue to obtain the fragment area yields a result
    // that is slightly above the desired value no matter what rounding is applied to the density value when storing it
    // in the fragment density map, and this should result in the desired fragment area being a valid result according
    // to the spec, which says the chosen density should have an area that is not larger than the desired one.
    m_densityValue =
        tcu::Vec2(1.0f / areaFloat.x() - densityValueDelta.x(), 1.0f / areaFloat.y() - densityValueDelta.y());
    m_viewMask = (m_testParams.viewCount > 1) ? ((1u << m_testParams.viewCount) - 1u) : 0u;

    const auto &deviceHelper                      = getDeviceHelper(m_context);
    const DeviceInterface &vk                     = deviceHelper.getDeviceInterface();
    const VkDevice vkDevice                       = deviceHelper.getDevice();
    const uint32_t queueFamilyIndex               = deviceHelper.getQueueFamilyIndex();
    const VkQueue queue                           = deviceHelper.getQueue();
    auto &memAlloc                                = deviceHelper.getAllocator();
    const VkComponentMapping componentMappingRGBA = makeComponentMappingRGBA();
    RenderPassWrapperBasePtr renderPassWrapper;

    // calculate all image sizes, image usage flags, view types etc.
    uint32_t densitiMapCount = 1 + m_testParams.subsampledLoads;
    VkExtent3D densityMapImageSize{m_testParams.densityMapSize.x(), m_testParams.densityMapSize.y(), 1};
    uint32_t densityMapImageLayers = m_testParams.viewCount;
    VkImageViewType densityMapImageViewType =
        (m_testParams.viewCount > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    vk::VkImageUsageFlags densityMapImageUsage =
        VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    const VkImageSubresourceRange densityMapSubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u,
                                                                densityMapImageLayers};
    uint32_t densityMapImageViewFlags                        = 0u;

    const VkFormat colorImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkExtent3D colorImageSize{m_renderSize.x() / m_testParams.viewCount, m_renderSize.y(), 1};
    uint32_t colorImageLayers             = densityMapImageLayers;
    VkImageViewType colorImageViewType    = densityMapImageViewType;
    vk::VkImageUsageFlags colorImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    uint32_t colorImageCreateFlags =
        ((m_testParams.nonSubsampledImages ? 0u : (uint32_t)VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT) |
         (m_testParams.addZeroOffset ? VK_IMAGE_CREATE_FRAGMENT_DENSITY_MAP_OFFSET_BIT_EXT : 0));

    uint32_t depthImageCreateFlags =
        ((m_testParams.nonSubsampledImages ? 0u : (uint32_t)VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT) |
         (m_testParams.addZeroOffset ? VK_IMAGE_CREATE_FRAGMENT_DENSITY_MAP_OFFSET_BIT_EXT : 0));
    const VkImageSubresourceRange colorSubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, colorImageLayers};

    const VkFormat depthImageFormat = m_testParams.depthFormat;
    VkExtent3D depthImageSize{m_renderSize.x(), m_renderSize.y(), 1};
    const VkImageSubresourceRange depthSubresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1};

    bool isColorImageMultisampled = m_testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT;
    bool isDynamicRendering       = m_testParams.groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING;
    bool isDepthEnabled           = m_testParams.depthEnabled;

    VkExtent3D outputImageSize{m_renderSize.x(), m_renderSize.y(), 1};
    const VkImageSubresourceRange outputSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};

    if (m_testParams.dynamicDensityMap)
    {
        DE_ASSERT(!m_testParams.subsampledLoads);

        densityMapImageUsage     = VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        densityMapImageViewFlags = (uint32_t)VK_IMAGE_VIEW_CREATE_FRAGMENT_DENSITY_MAP_DYNAMIC_BIT_EXT;
    }
    else if (m_testParams.deferredDensityMap)
        densityMapImageViewFlags = (uint32_t)VK_IMAGE_VIEW_CREATE_FRAGMENT_DENSITY_MAP_DEFERRED_BIT_EXT;
    if (m_testParams.makeCopy)
        colorImageUsage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

    // Create subsampled color image
    prepareImageAndImageView(vk, vkDevice, memAlloc, colorImageCreateFlags, colorImageFormat, colorImageSize,
                             colorImageLayers, m_testParams.colorSamples, colorImageUsage, queueFamilyIndex, 0u,
                             colorImageViewType, componentMappingRGBA, colorSubresourceRange, m_colorImage,
                             m_colorImageAlloc, m_colorImageView);

    // Create subsampled color image for resolve operation ( when multisampling is used )
    if (isColorImageMultisampled)
    {
        prepareImageAndImageView(vk, vkDevice, memAlloc, colorImageCreateFlags, colorImageFormat, colorImageSize,
                                 colorImageLayers, VK_SAMPLE_COUNT_1_BIT,
                                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, queueFamilyIndex, 0u,
                                 colorImageViewType, componentMappingRGBA, colorSubresourceRange, m_colorResolvedImage,
                                 m_colorResolvedImageAlloc, m_colorResolvedImageView);
    }

    // Create subsampled image copy
    if (m_testParams.makeCopy)
    {
        prepareImageAndImageView(vk, vkDevice, memAlloc, colorImageCreateFlags, colorImageFormat, colorImageSize,
                                 colorImageLayers, m_testParams.colorSamples,
                                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, queueFamilyIndex, 0u,
                                 colorImageViewType, componentMappingRGBA, colorSubresourceRange, m_colorCopyImage,
                                 m_colorCopyImageAlloc, m_colorCopyImageView);
    }

    // Create depth image
    if (isDepthEnabled)
    {
        prepareImageAndImageView(vk, vkDevice, memAlloc, depthImageCreateFlags, depthImageFormat, depthImageSize, 1,
                                 VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, queueFamilyIndex,
                                 0u, VK_IMAGE_VIEW_TYPE_2D, componentMappingRGBA, depthSubresourceRange, m_depthImage,
                                 m_depthImageAlloc, m_depthImageView);
    }

    // Create output image ( data from subsampled color image will be copied into it using sampler with VK_SAMPLER_CREATE_SUBSAMPLED_BIT_EXT )
    prepareImageAndImageView(vk, vkDevice, memAlloc, 0u, colorImageFormat, outputImageSize, 1u, VK_SAMPLE_COUNT_1_BIT,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, queueFamilyIndex,
                             0u, VK_IMAGE_VIEW_TYPE_2D, componentMappingRGBA, outputSubresourceRange, m_outputImage,
                             m_outputImageAlloc, m_outputImageView);

    // Create density map image/images
    const auto fdmCreateFlags = (m_testParams.addZeroOffset ? VK_IMAGE_CREATE_FRAGMENT_DENSITY_MAP_OFFSET_BIT_EXT : 0);
    for (uint32_t mapIndex = 0; mapIndex < densitiMapCount; ++mapIndex)
    {
        Move<VkImage> densityMapImage;
        de::MovePtr<Allocation> densityMapImageAlloc;
        Move<VkImageView> densityMapImageView;

        prepareImageAndImageView(vk, vkDevice, memAlloc, fdmCreateFlags, m_testParams.densityMapFormat,
                                 densityMapImageSize, densityMapImageLayers, VK_SAMPLE_COUNT_1_BIT,
                                 densityMapImageUsage, queueFamilyIndex, densityMapImageViewFlags,
                                 densityMapImageViewType, componentMappingRGBA, densityMapSubresourceRange,
                                 densityMapImage, densityMapImageAlloc, densityMapImageView);

        m_densityMapImages.push_back(VkImageSp(new Unique<VkImage>(densityMapImage)));
        m_densityMapImageAllocs.push_back(AllocationSp(densityMapImageAlloc.release()));
        m_densityMapImageViews.push_back(VkImageViewSp(new Unique<VkImageView>(densityMapImageView)));
    }

    // Create and fill staging buffer, copy its data to density map image
    if (!m_testParams.dynamicDensityMap)
    {
        tcu::TextureFormat densityMapTextureFormat = vk::mapVkFormat(m_testParams.densityMapFormat);
        VkDeviceSize stagingBufferSize = tcu::getPixelSize(densityMapTextureFormat) * densityMapImageSize.width *
                                         densityMapImageSize.height * densityMapImageLayers;
        const vk::VkBufferCreateInfo stagingBufferCreateInfo{
            vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            nullptr,
            0u,                               // flags
            stagingBufferSize,                // size
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // usage
            vk::VK_SHARING_MODE_EXCLUSIVE,    // sharingMode
            0u,                               // queueFamilyCount
            nullptr,                          // pQueueFamilyIndices
        };
        vk::Move<vk::VkBuffer> stagingBuffer = vk::createBuffer(vk, vkDevice, &stagingBufferCreateInfo);
        const vk::VkMemoryRequirements stagingRequirements =
            vk::getBufferMemoryRequirements(vk, vkDevice, *stagingBuffer);
        de::MovePtr<vk::Allocation> stagingAllocation =
            memAlloc.allocate(stagingRequirements, MemoryRequirement::HostVisible);
        VK_CHECK(vk.bindBufferMemory(vkDevice, *stagingBuffer, stagingAllocation->getMemory(),
                                     stagingAllocation->getOffset()));
        tcu::PixelBufferAccess stagingBufferAccess(densityMapTextureFormat, densityMapImageSize.width,
                                                   densityMapImageSize.height, densityMapImageLayers,
                                                   stagingAllocation->getHostPtr());
        tcu::Vec4 fragmentArea(m_densityValue.x(), m_densityValue.y(), 0.0f, 1.0f);

        for (uint32_t mapIndex = 0; mapIndex < densitiMapCount; ++mapIndex)
        {
            // Fill staging buffer with one color
            tcu::clear(stagingBufferAccess, fragmentArea);
            flushAlloc(vk, vkDevice, *stagingAllocation);

            copyBufferToImage(vk, vkDevice, queue, queueFamilyIndex, *stagingBuffer, stagingBufferSize,
                              densityMapImageSize, densityMapImageLayers, **m_densityMapImages[mapIndex]);

            std::swap(fragmentArea.m_data[0], fragmentArea.m_data[1]);
        }
    }

    uint32_t samplerCreateFlags = (uint32_t)VK_SAMPLER_CREATE_SUBSAMPLED_BIT_EXT;
    if (m_testParams.coarseReconstruction)
        samplerCreateFlags |= (uint32_t)VK_SAMPLER_CREATE_SUBSAMPLED_COARSE_RECONSTRUCTION_BIT_EXT;
    if (m_testParams.nonSubsampledImages)
        samplerCreateFlags = 0u;

    const struct VkSamplerCreateInfo samplerInfo
    {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,        // sType
            nullptr,                                  // pNext
            (VkSamplerCreateFlags)samplerCreateFlags, // flags
            VK_FILTER_NEAREST,                        // magFilter
            VK_FILTER_NEAREST,                        // minFilter
            VK_SAMPLER_MIPMAP_MODE_NEAREST,           // mipmapMode
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,    // addressModeU
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,    // addressModeV
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,    // addressModeW
            0.0f,                                     // mipLodBias
            VK_FALSE,                                 // anisotropyEnable
            1.0f,                                     // maxAnisotropy
            false,                                    // compareEnable
            VK_COMPARE_OP_ALWAYS,                     // compareOp
            0.0f,                                     // minLod
            0.0f,                                     // maxLod
            VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,  // borderColor
            VK_FALSE,                                 // unnormalizedCoords
    };

    // Create a sampler that are able to read from subsampled image
    // (more than one sampler is needed only for 4 maxDescriptorSetSubsampledSamplers tests)
    for (uint32_t samplerIndex = 0; samplerIndex < testParams.samplersCount; ++samplerIndex)
        m_colorSamplers.push_back(VkSamplerSp(new Unique<VkSampler>(createSampler(vk, vkDevice, &samplerInfo))));

    if (!isDynamicRendering)
    {
        // Create render passes
        if (m_testParams.groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
            renderPassWrapper = RenderPassWrapperBasePtr(
                new RenderPassWrapper<RENDERING_TYPE_RENDERPASS_LEGACY>(vk, vkDevice, testParams));
        else
            renderPassWrapper =
                RenderPassWrapperBasePtr(new RenderPassWrapper<RENDERING_TYPE_RENDERPASS2>(vk, vkDevice, testParams));

        if (testParams.dynamicDensityMap)
            m_renderPassProduceDynamicDensityMap =
                renderPassWrapper->createRenderPassProduceDynamicDensityMap(m_viewMask);
        m_renderPassProduceSubsampledImage =
            renderPassWrapper->createRenderPassProduceSubsampledImage(m_viewMask, testParams.makeCopy, false);
        if (testParams.subsampledLoads)
            m_renderPassUpdateSubsampledImage =
                renderPassWrapper->createRenderPassProduceSubsampledImage(m_viewMask, false, true);
        m_renderPassOutputSubsampledImage = renderPassWrapper->createRenderPassOutputSubsampledImage();

        // Create framebuffers
        if (!testParams.imagelessFramebuffer)
        {
            if (testParams.dynamicDensityMap)
            {
                m_framebufferProduceDynamicDensityMap =
                    createFrameBuffer(vk, vkDevice, *m_renderPassProduceDynamicDensityMap, densityMapImageSize,
                                      {**m_densityMapImageViews[0]});
            }

            std::vector<VkImageView> imageViewsProduceSubsampledImage = {*m_colorImageView};
            if (isColorImageMultisampled)
                imageViewsProduceSubsampledImage.push_back(*m_colorResolvedImageView);
            if (testParams.makeCopy)
                imageViewsProduceSubsampledImage.push_back(*m_colorCopyImageView);
            imageViewsProduceSubsampledImage.push_back(**m_densityMapImageViews[0]);
            if (testParams.depthEnabled)
                imageViewsProduceSubsampledImage.push_back(*m_depthImageView);

            m_framebufferProduceSubsampledImage = createFrameBuffer(vk, vkDevice, *m_renderPassProduceSubsampledImage,
                                                                    colorImageSize, imageViewsProduceSubsampledImage);

            if (testParams.subsampledLoads)
            {
                m_framebufferUpdateSubsampledImage =
                    createFrameBuffer(vk, vkDevice, *m_renderPassUpdateSubsampledImage, colorImageSize,
                                      {*m_colorImageView, **m_densityMapImageViews[1]});
            }

            m_framebufferOutputSubsampledImage = createFrameBuffer(vk, vkDevice, *m_renderPassOutputSubsampledImage,
                                                                   outputImageSize, {*m_outputImageView});
        }
        else // create same framebuffers as above but with VkFramebufferAttachmentsCreateInfo instead of image views
        {
            // helper lambda used to create VkFramebufferAttachmentImageInfo structure and reduce code size
            auto createFramebufferAttachmentImageInfo = [](VkImageCreateFlags createFlags, VkImageUsageFlags usageFlags,
                                                           VkExtent3D &extent, uint32_t layerCount,
                                                           const VkFormat *format)
            {
                return VkFramebufferAttachmentImageInfo{
                    VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO, // VkStructureType sType;const DeviceInterface &vk,
                    nullptr,                                             // const void* pNext;
                    createFlags,                                         // VkImageCreateFlags flags;
                    usageFlags,                                          // VkImageUsageFlags usage;
                    extent.width,                                        // uint32_t width;
                    extent.height,                                       // uint32_t height;
                    layerCount,                                          // uint32_t layerCount;
                    1u,                                                  // uint32_t viewFormatCount;
                    format                                               // const VkFormat* pViewFormats;
                };
            };

            if (testParams.dynamicDensityMap)
            {
                m_framebufferProduceDynamicDensityMap = createImagelessFrameBuffer(
                    vk, vkDevice, *m_renderPassProduceDynamicDensityMap, densityMapImageSize,
                    {createFramebufferAttachmentImageInfo(0u, densityMapImageUsage, densityMapImageSize,
                                                          densityMapImageLayers, &m_testParams.densityMapFormat)});
            }

            std::vector<VkFramebufferAttachmentImageInfo> attachmentInfoProduceSubsampledImage;
            attachmentInfoProduceSubsampledImage.reserve(5);
            attachmentInfoProduceSubsampledImage.push_back(
                createFramebufferAttachmentImageInfo((VkImageCreateFlags)colorImageCreateFlags, colorImageUsage,
                                                     colorImageSize, colorImageLayers, &colorImageFormat));
            if (isColorImageMultisampled)
            {
                attachmentInfoProduceSubsampledImage.push_back(createFramebufferAttachmentImageInfo(
                    (VkImageCreateFlags)colorImageCreateFlags,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, colorImageSize, colorImageLayers,
                    &colorImageFormat));
            }
            if (testParams.makeCopy)
            {
                attachmentInfoProduceSubsampledImage.push_back(createFramebufferAttachmentImageInfo(
                    (VkImageCreateFlags)colorImageCreateFlags,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, colorImageSize, colorImageLayers,
                    &colorImageFormat));
            }
            attachmentInfoProduceSubsampledImage.push_back(createFramebufferAttachmentImageInfo(
                0u, densityMapImageUsage, densityMapImageSize, densityMapImageLayers, &m_testParams.densityMapFormat));

            if (isDepthEnabled)
            {
                attachmentInfoProduceSubsampledImage.push_back(createFramebufferAttachmentImageInfo(
                    colorImageCreateFlags /*shared with depth*/, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    depthImageSize, colorImageLayers, &depthImageFormat));
            }

            m_framebufferProduceSubsampledImage =
                createImagelessFrameBuffer(vk, vkDevice, *m_renderPassProduceSubsampledImage, colorImageSize,
                                           attachmentInfoProduceSubsampledImage);

            if (testParams.subsampledLoads)
            {
                m_framebufferUpdateSubsampledImage = createImagelessFrameBuffer(
                    vk, vkDevice, *m_renderPassUpdateSubsampledImage, colorImageSize,
                    {createFramebufferAttachmentImageInfo((VkImageCreateFlags)colorImageCreateFlags, colorImageUsage,
                                                          colorImageSize, colorImageLayers, &colorImageFormat),
                     createFramebufferAttachmentImageInfo(0u, densityMapImageUsage, densityMapImageSize,
                                                          densityMapImageLayers, &m_testParams.densityMapFormat)});
            }

            m_framebufferOutputSubsampledImage = createImagelessFrameBuffer(
                vk, vkDevice, *m_renderPassOutputSubsampledImage, outputImageSize,
                {createFramebufferAttachmentImageInfo(
                    0u, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, outputImageSize, 1u,
                    &colorImageFormat)});
        }
    }

    // Create pipeline layout for subpasses that do not use any descriptors
    {
        const VkPipelineLayoutCreateInfo pipelineLayoutParams{
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
            nullptr,                                       // const void* pNext;
            0u,                                            // VkPipelineLayoutCreateFlags flags;
            0u,                                            // uint32_t setLayoutCount;
            nullptr,                                       // const VkDescriptorSetLayout* pSetLayouts;
            0u,                                            // uint32_t pushConstantRangeCount;
            nullptr                                        // const VkPushConstantRange* pPushConstantRanges;
        };

        m_pipelineLayoutNoDescriptors = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
    }

    // Create pipeline layout for subpass that copies data or resamples subsampled image
    if (m_testParams.makeCopy || m_testParams.subsampledLoads)
    {
        m_descriptorSetLayoutOperateOnSubsampledImage =
            DescriptorSetLayoutBuilder()
                .addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr)
                .build(vk, vkDevice);

        // Create and bind descriptor set
        m_descriptorPoolOperateOnSubsampledImage =
            DescriptorPoolBuilder()
                .addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1u)
                .build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

        m_pipelineLayoutOperateOnSubsampledImage =
            makePipelineLayout(vk, vkDevice, *m_descriptorSetLayoutOperateOnSubsampledImage);
        m_descriptorSetOperateOnSubsampledImage = makeDescriptorSet(
            vk, vkDevice, *m_descriptorPoolOperateOnSubsampledImage, *m_descriptorSetLayoutOperateOnSubsampledImage);

        // VUID-vkCmdDraw-imageLayout-00344
        const VkImageLayout inputAttachmentLayout  = (isDynamicRendering && m_testParams.makeCopy) ?
                                                         VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR :
                                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        const VkDescriptorImageInfo inputImageInfo = {
            VK_NULL_HANDLE,       // VkSampler sampler;
            *m_colorImageView,    // VkImageView imageView;
            inputAttachmentLayout // VkImageLayout imageLayout;
        };
        DescriptorSetUpdateBuilder()
            .writeSingle(*m_descriptorSetOperateOnSubsampledImage, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &inputImageInfo)
            .update(vk, vkDevice);
    }

    // Create pipeline layout for last render pass (output subsampled image)
    {
        DescriptorSetLayoutBuilder descriptorSetLayoutBuilder;
        DescriptorPoolBuilder descriptorPoolBuilder;
        for (uint32_t samplerIndex = 0; samplerIndex < testParams.samplersCount; ++samplerIndex)
        {
            descriptorSetLayoutBuilder.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                               VK_SHADER_STAGE_FRAGMENT_BIT,
                                                               &(*m_colorSamplers[samplerIndex]).get());
            descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, samplerIndex + 1u);
        }

        m_descriptorSetLayoutOutputSubsampledImage = descriptorSetLayoutBuilder.build(vk, vkDevice);
        m_descriptorPoolOutputSubsampledImage =
            descriptorPoolBuilder.build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
        m_pipelineLayoutOutputSubsampledImage =
            makePipelineLayout(vk, vkDevice, *m_descriptorSetLayoutOutputSubsampledImage);
        m_descriptorSetOutputSubsampledImage = makeDescriptorSet(vk, vkDevice, *m_descriptorPoolOutputSubsampledImage,
                                                                 *m_descriptorSetLayoutOutputSubsampledImage);

        VkImageView srcImageView = *m_colorImageView;
        if (isColorImageMultisampled)
            srcImageView = *m_colorResolvedImageView;
        else if (m_testParams.makeCopy)
            srcImageView = *m_colorCopyImageView;

        const VkDescriptorImageInfo inputImageInfo{
            VK_NULL_HANDLE,                          // VkSampler sampler;
            srcImageView,                            // VkImageView imageView;
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL // VkImageLayout imageLayout;
        };

        DescriptorSetUpdateBuilder descriptorSetUpdateBuilder;
        for (uint32_t samplerIndex = 0; samplerIndex < testParams.samplersCount; ++samplerIndex)
            descriptorSetUpdateBuilder.writeSingle(*m_descriptorSetOutputSubsampledImage,
                                                   DescriptorSetUpdateBuilder::Location::binding(samplerIndex),
                                                   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &inputImageInfo);
        descriptorSetUpdateBuilder.update(vk, vkDevice);
    }

    // Load vertex and fragment shaders
    auto &bc                   = m_context.getBinaryCollection();
    m_vertexCommonShaderModule = createShaderModule(vk, vkDevice, bc.get("vert"), 0);
    m_fragmentShaderModuleProduceSubsampledImage =
        createShaderModule(vk, vkDevice, bc.get("frag_produce_subsampled"), 0);
    if (m_testParams.makeCopy)
    {
        const char *moduleName = isColorImageMultisampled ? "frag_copy_subsampled_ms" : "frag_copy_subsampled";
        m_fragmentShaderModuleCopySubsampledImage = createShaderModule(vk, vkDevice, bc.get(moduleName), 0);
    }
    if (m_testParams.subsampledLoads)
    {
        const char *moduleName                      = "frag_update_subsampled";
        m_fragmentShaderModuleUpdateSubsampledImage = createShaderModule(vk, vkDevice, bc.get(moduleName), 0);
    }
    const char *moduleName = (m_testParams.viewCount > 1) ? "frag_output_2darray" : "frag_output_2d";
    m_fragmentShaderModuleOutputSubsampledImage = createShaderModule(vk, vkDevice, bc.get(moduleName), 0);

    const std::vector<VkRect2D> dynamicDensityMapRenderArea{
        makeRect2D(densityMapImageSize.width, densityMapImageSize.height)};
    const std::vector<VkRect2D> outputRenderArea{makeRect2D(outputImageSize.width, outputImageSize.height)};
    const VkRect2D colorImageRect = makeRect2D(colorImageSize.width, colorImageSize.height);
    std::vector<VkRect2D> colorImageRenderArea((m_testParams.multiViewport ? m_testParams.viewCount : 1u),
                                               colorImageRect);

    // Create pipelines
    {
        const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType                            sType
            nullptr,                                   // const void*                                pNext
            (VkPipelineMultisampleStateCreateFlags)0u, // VkPipelineMultisampleStateCreateFlags    flags
            (VkSampleCountFlagBits)
                m_testParams.colorSamples, // VkSampleCountFlagBits                    rasterizationSamples
            VK_FALSE,                      // VkBool32                                    sampleShadingEnable
            1.0f,                          // float                                    minSampleShading
            nullptr,                       // const VkSampleMask*                        pSampleMask
            VK_FALSE,                      // VkBool32                                    alphaToCoverageEnable
            VK_FALSE                       // VkBool32                                    alphaToOneEnable
        };

        const std::vector<VkViewport> viewportsProduceDynamicDensityMap{
            makeViewport(densityMapImageSize.width, densityMapImageSize.height)};
        const std::vector<VkViewport> viewportsOutputSubsampledImage{
            makeViewport(outputImageSize.width, outputImageSize.height)};
        std::vector<VkViewport> viewportsSubsampledImage(colorImageRenderArea.size(),
                                                         makeViewport(colorImageSize.width, colorImageSize.height));

        // test multiview in conjunction with multiViewport which specifies a different viewport per view
        if (m_testParams.multiViewport)
        {
            const uint32_t halfWidth    = colorImageSize.width / 2u;
            const float halfWidthFloat  = static_cast<float>(halfWidth);
            const float halfHeightFloat = static_cast<float>(colorImageSize.height / 2u);
            for (uint32_t viewIndex = 0; viewIndex < m_testParams.viewCount; ++viewIndex)
            {
                // modify scissors/viewport for every other view
                bool isOdd = viewIndex % 2;

                auto &rect        = colorImageRenderArea[viewIndex];
                rect.extent.width = halfWidth;
                rect.offset.x     = isOdd * halfWidth;

                auto &viewport  = viewportsSubsampledImage[viewIndex];
                viewport.width  = halfWidthFloat;
                viewport.height = halfHeightFloat;
                viewport.y      = !isOdd * halfHeightFloat;
                viewport.x      = isOdd * halfWidthFloat;
            }
        }

        uint32_t colorAttachmentLocations[] = {VK_ATTACHMENT_UNUSED, 0};
        VkFormat colorImageFormats[]        = {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};
        VkRenderingAttachmentLocationInfoKHR renderingAttachmentLocationInfo{
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_LOCATION_INFO_KHR, nullptr, 2, colorAttachmentLocations};
        std::vector<VkPipelineRenderingCreateInfoKHR> renderingCreateInfo(
            5, {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR, nullptr, m_viewMask, 1u,
                &m_testParams.densityMapFormat, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED});
        renderingCreateInfo[1].pColorAttachmentFormats = &colorImageFormat;
        renderingCreateInfo[3].pColorAttachmentFormats = &colorImageFormat;
        renderingCreateInfo[4].viewMask                = 0;
        renderingCreateInfo[4].pColorAttachmentFormats = &colorImageFormat;

        if (m_testParams.makeCopy)
        {
            renderingCreateInfo[1].colorAttachmentCount    = 2u;
            renderingCreateInfo[1].pColorAttachmentFormats = colorImageFormats;

            renderingCreateInfo[2].pNext                   = &renderingAttachmentLocationInfo;
            renderingCreateInfo[2].colorAttachmentCount    = 2u;
            renderingCreateInfo[2].pColorAttachmentFormats = colorImageFormats;
        }

        const void *pNextForProduceDynamicDensityMap = (isDynamicRendering ? &renderingCreateInfo[0] : nullptr);
        const void *pNextForProduceSubsampledImage   = (isDynamicRendering ? &renderingCreateInfo[1] : nullptr);
        const void *pNextForCopySubsampledImage      = (isDynamicRendering ? &renderingCreateInfo[2] : nullptr);
        const void *pNextForUpdateSubsampledImage    = (isDynamicRendering ? &renderingCreateInfo[3] : nullptr);
        const void *pNextForOutputSubsampledImage    = (isDynamicRendering ? &renderingCreateInfo[4] : nullptr);

        if (testParams.dynamicDensityMap)
            m_graphicsPipelineProduceDynamicDensityMap = buildGraphicsPipeline(
                vk,                             // const DeviceInterface&                            vk
                vkDevice,                       // const VkDevice                                    device
                *m_pipelineLayoutNoDescriptors, // const VkPipelineLayout                            pipelineLayout
                *m_vertexCommonShaderModule, // const VkShaderModule                                vertexShaderModule
                *m_fragmentShaderModuleProduceSubsampledImage, // const VkShaderModule                                fragmentShaderModule
                *m_renderPassProduceDynamicDensityMap, // const VkRenderPass                                renderPass
                viewportsProduceDynamicDensityMap,     // const std::vector<VkViewport>&                    viewport
                dynamicDensityMapRenderArea,           // const std::vector<VkRect2D>&                        scissor
                0u,                                    // const uint32_t                                    subpass
                nullptr, // const VkPipelineMultisampleStateCreateInfo*        multisampleStateCreateInfo
                pNextForProduceDynamicDensityMap, // const void*                                        pNext
                isDynamicRendering, // const bool                                        useDensityMapAttachment
                false,              // const bool                                        useDepthAttachment
                m_testParams.useMaintenance5); // const bool                                        useMaintenance5

        m_graphicsPipelineProduceSubsampledImage = buildGraphicsPipeline(
            vk,                             // const DeviceInterface&                            vk
            vkDevice,                       // const VkDevice                                    device
            *m_pipelineLayoutNoDescriptors, // const VkPipelineLayout                            pipelineLayout
            *m_vertexCommonShaderModule,    // const VkShaderModule                                vertexShaderModule
            *m_fragmentShaderModuleProduceSubsampledImage, // const VkShaderModule                                fragmentShaderModule
            *m_renderPassProduceSubsampledImage, // const VkRenderPass                                renderPass
            viewportsSubsampledImage,            // const std::vector<VkViewport>&                    viewport
            colorImageRenderArea,                // const std::vector<VkRect2D>&                        scissor
            0u,                                  // const uint32_t                                    subpass
            &multisampleStateCreateInfo, // const VkPipelineMultisampleStateCreateInfo*        multisampleStateCreateInfo
            pNextForProduceSubsampledImage, // const void*                                        pNext
            isDynamicRendering,             // const bool                                        useDensityMapAttachment
            isDepthEnabled,                 // const bool                                        useDepthAttachment
            m_testParams.useMaintenance5);  // const bool                                        useMaintenance5

        if (m_testParams.makeCopy)
            m_graphicsPipelineCopySubsampledImage = buildGraphicsPipeline(
                vk,                                        // const DeviceInterface&                            vk
                vkDevice,                                  // const VkDevice                                    device
                *m_pipelineLayoutOperateOnSubsampledImage, // const VkPipelineLayout                            pipelineLayout
                *m_vertexCommonShaderModule, // const VkShaderModule                                vertexShaderModule
                *m_fragmentShaderModuleCopySubsampledImage, // const VkShaderModule                                fragmentShaderModule
                *m_renderPassProduceSubsampledImage, // const VkRenderPass                                renderPass
                viewportsSubsampledImage,            // const std::vector<VkViewport>&                    viewport
                colorImageRenderArea,                // const std::vector<VkRect2D>&                        scissor
                1u,                                  // const uint32_t                                    subpass
                &multisampleStateCreateInfo, // const VkPipelineMultisampleStateCreateInfo*        multisampleStateCreateInfo
                pNextForCopySubsampledImage, // const void*                                        pNext
                isDynamicRendering, // const bool                                        useDensityMapAttachment
                false);             // const bool                                        useDepthAttachment
        if (m_testParams.subsampledLoads)
            m_graphicsPipelineUpdateSubsampledImage = buildGraphicsPipeline(
                vk,                                        // const DeviceInterface&                            vk
                vkDevice,                                  // const VkDevice                                    device
                *m_pipelineLayoutOperateOnSubsampledImage, // const VkPipelineLayout                            pipelineLayout
                *m_vertexCommonShaderModule, // const VkShaderModule                                vertexShaderModule
                *m_fragmentShaderModuleUpdateSubsampledImage, // const VkShaderModule                                fragmentShaderModule
                *m_renderPassUpdateSubsampledImage, // const VkRenderPass                                renderPass
                viewportsSubsampledImage,           // const std::vector<VkViewport>&                    viewport
                colorImageRenderArea,               // const std::vector<VkRect2D>&                        scissor
                0u,                                 // const uint32_t                                    subpass
                &multisampleStateCreateInfo, // const VkPipelineMultisampleStateCreateInfo*        multisampleStateCreateInfo
                pNextForUpdateSubsampledImage, // const void*                                        pNext
                isDynamicRendering, // const bool                                        useDensityMapAttachment
                false,              // const bool                                        useDepthAttachment
                m_testParams.useMaintenance5); // const bool                                        useMaintenance5

        m_graphicsPipelineOutputSubsampledImage = buildGraphicsPipeline(
            vk,                                     // const DeviceInterface&                            vk
            vkDevice,                               // const VkDevice                                    device
            *m_pipelineLayoutOutputSubsampledImage, // const VkPipelineLayout                            pipelineLayout
            *m_vertexCommonShaderModule, // const VkShaderModule                                vertexShaderModule
            *m_fragmentShaderModuleOutputSubsampledImage, // const VkShaderModule                                fragmentShaderModule
            *m_renderPassOutputSubsampledImage, // const VkRenderPass                                renderPass
            viewportsOutputSubsampledImage,     // const std::vector<VkViewport>&                    viewport
            outputRenderArea,                   // const std::vector<VkRect2D>&                        scissor
            0u,                                 // const uint32_t                                    subpass
            nullptr, // const VkPipelineMultisampleStateCreateInfo*        multisampleStateCreateInfo
            pNextForOutputSubsampledImage, // const void*                                        pNext
            false,                         // const bool                                        useDensityMapAttachment
            false);                        // const bool                                        useDepthAttachment
    }

    // Create vertex buffers
    const tcu::Vec2 densityX(m_densityValue.x());
    const tcu::Vec2 densityY(m_densityValue.y());
    m_vertices = createFullScreenMeshWithMirrors(1, {0.0f, 1.0f}, {0.0f, 1.0f}); // create fullscreen quad with gradient
    if (testParams.dynamicDensityMap)
        m_verticesDDM = createFullscreenMesh(1, densityX, densityY); // create fullscreen quad with single color
    m_verticesOutput = createFullscreenMesh(m_testParams.viewCount, {0.0f, 0.0f},
                                            {0.0f, 0.0f}); // create fullscreen mesh with black color

    createVertexBuffer(vk, vkDevice, queueFamilyIndex, memAlloc, m_vertices, m_vertexBuffer, m_vertexBufferAlloc);
    if (testParams.dynamicDensityMap)
        createVertexBuffer(vk, vkDevice, queueFamilyIndex, memAlloc, m_verticesDDM, m_vertexBufferDDM,
                           m_vertexBufferAllocDDM);
    createVertexBuffer(vk, vkDevice, queueFamilyIndex, memAlloc, m_verticesOutput, m_vertexBufferOutput,
                       m_vertexBufferOutputAlloc);

    // Create command pool and command buffer
    m_cmdPool   = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
    m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    if (isDynamicRendering)
        createCommandBufferForDynamicRendering(vk, vkDevice, dynamicDensityMapRenderArea[0], colorImageRect,
                                               outputRenderArea[0]);
    else
        createCommandBufferForRenderpass(vk, vkDevice, renderPassWrapper, colorImageSize,
                                         dynamicDensityMapRenderArea[0], colorImageRect, outputRenderArea[0]);
}

void FragmentDensityMapTestInstance::drawDynamicDensityMap(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    const VkDeviceSize vertexBufferOffset = 0;

    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineProduceDynamicDensityMap);
    vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &m_vertexBufferDDM.get(), &vertexBufferOffset);
    vk.cmdDraw(cmdBuffer, (uint32_t)m_verticesDDM.size(), 1, 0, 0);
}

void FragmentDensityMapTestInstance::drawSubsampledImage(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    const VkDeviceSize vertexBufferOffset = 0;

    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineProduceSubsampledImage);
    vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
    vk.cmdDraw(cmdBuffer, (uint32_t)m_vertices.size(), 1, 0, 0);
}

void FragmentDensityMapTestInstance::drawCopySubsampledImage(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    const VkDeviceSize vertexBufferOffset = 0;

    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineCopySubsampledImage);
    vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayoutOperateOnSubsampledImage, 0,
                             1, &m_descriptorSetOperateOnSubsampledImage.get(), 0, nullptr);
    vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
    vk.cmdDraw(cmdBuffer, (uint32_t)m_vertices.size(), 1, 0, 0);
}

void FragmentDensityMapTestInstance::drawResampleSubsampledImage(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    const VkDeviceSize vertexBufferOffset = 0;

    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineUpdateSubsampledImage);
    vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayoutOperateOnSubsampledImage, 0,
                             1, &m_descriptorSetOperateOnSubsampledImage.get(), 0, nullptr);
    vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
    vk.cmdDraw(cmdBuffer, (uint32_t)m_vertices.size(), 1, 0, 0);
}

void FragmentDensityMapTestInstance::drawOutputSubsampledImage(const DeviceInterface &vk, VkCommandBuffer cmdBuffer)
{
    const VkDeviceSize vertexBufferOffset = 0;

    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineOutputSubsampledImage);
    vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayoutOutputSubsampledImage, 0, 1,
                             &m_descriptorSetOutputSubsampledImage.get(), 0, nullptr);
    vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &m_vertexBufferOutput.get(), &vertexBufferOffset);
    vk.cmdDraw(cmdBuffer, (uint32_t)m_verticesOutput.size(), 1, 0, 0);
}

void FragmentDensityMapTestInstance::remapingBeforeCopySubsampledImage(const DeviceInterface &vk,
                                                                       VkCommandBuffer cmdBuffer)
{
    // Barier before next subpass
    VkMemoryBarrier memoryBarrier =
        makeMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT);
    vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 1, &memoryBarrier, 0,
                          nullptr, 0, nullptr);

    // color attachment remaping
    uint32_t colorAttachmentLocations[] = {VK_ATTACHMENT_UNUSED, 0};
    VkRenderingAttachmentLocationInfoKHR renderingAttachmentLocationInfo{
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_LOCATION_INFO_KHR, nullptr, 2, colorAttachmentLocations};
    vk.cmdSetRenderingAttachmentLocations(cmdBuffer, &renderingAttachmentLocationInfo);
}

void FragmentDensityMapTestInstance::createCommandBufferForRenderpass(const DeviceInterface &vk, VkDevice vkDevice,
                                                                      RenderPassWrapperBasePtr renderPassWrapper,
                                                                      const VkExtent3D &colorImageSize,
                                                                      const VkRect2D &dynamicDensityMapRenderArea,
                                                                      const VkRect2D &colorImageRenderArea,
                                                                      const VkRect2D &outputRenderArea)
{
    const bool isColorImageMultisampled     = m_testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT;
    const VkClearValue attachmentClearValue = makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f);
    const VkClearValue depthClearValue      = makeClearValueDepthStencil(1.0f, 0u);
    const VkClearValue emptyClearValue      = makeClearValueColorU32(0u, 0u, 0u, 0u);
    const uint32_t attachmentCount          = 1 + m_testParams.makeCopy + isColorImageMultisampled;
    const std::vector<VkClearValue> attachmentClearValues(attachmentCount, attachmentClearValue);

    if (m_testParams.groupParams->useSecondaryCmdBuffer)
    {
        VkCommandBufferInheritanceInfo bufferInheritanceInfo{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, // VkStructureType sType;
            nullptr,                                           // const void* pNext;
            *m_renderPassProduceDynamicDensityMap,             // VkRenderPass renderPass;
            0,                                                 // uint32_t subpass;
            *m_framebufferProduceDynamicDensityMap,            // VkFramebuffer framebuffer;
            false,                                             // VkBool32 occlusionQueryEnable;
            0u,                                                // VkQueryControlFlags queryFlags;
            0,                                                 // VkQueryPipelineStatisticFlags pipelineStatistics;
        };
        const VkCommandBufferBeginInfo commandBufBeginParams{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
            nullptr,                                     // const void* pNext;
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT |
                VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, // VkCommandBufferUsageFlags flags;
            &bufferInheritanceInfo};

        if (m_testParams.dynamicDensityMap)
        {
            m_dynamicDensityMapSecCmdBuffer =
                allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
            vk.beginCommandBuffer(*m_dynamicDensityMapSecCmdBuffer, &commandBufBeginParams);
            drawDynamicDensityMap(vk, *m_dynamicDensityMapSecCmdBuffer);
            endCommandBuffer(vk, *m_dynamicDensityMapSecCmdBuffer);
        }

        bufferInheritanceInfo.renderPass  = *m_renderPassProduceSubsampledImage;
        bufferInheritanceInfo.framebuffer = *m_framebufferProduceSubsampledImage;
        m_subsampledImageSecCmdBuffer =
            allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
        vk.beginCommandBuffer(*m_subsampledImageSecCmdBuffer, &commandBufBeginParams);
        drawSubsampledImage(vk, *m_subsampledImageSecCmdBuffer);
        if (m_testParams.makeCopy)
        {
            renderPassWrapper->cmdNextSubpass(*m_subsampledImageSecCmdBuffer);
            drawCopySubsampledImage(vk, *m_subsampledImageSecCmdBuffer);
        }
        endCommandBuffer(vk, *m_subsampledImageSecCmdBuffer);

        if (m_testParams.subsampledLoads)
        {
            bufferInheritanceInfo.renderPass  = *m_renderPassUpdateSubsampledImage;
            bufferInheritanceInfo.framebuffer = *m_framebufferUpdateSubsampledImage;
            m_resampleSubsampledImageSecCmdBuffer =
                allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
            vk.beginCommandBuffer(*m_resampleSubsampledImageSecCmdBuffer, &commandBufBeginParams);
            drawResampleSubsampledImage(vk, *m_resampleSubsampledImageSecCmdBuffer);
            endCommandBuffer(vk, *m_resampleSubsampledImageSecCmdBuffer);
        }

        bufferInheritanceInfo.renderPass  = *m_renderPassOutputSubsampledImage;
        bufferInheritanceInfo.framebuffer = *m_framebufferOutputSubsampledImage;
        m_outputSubsampledImageSecCmdBuffer =
            allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
        vk.beginCommandBuffer(*m_outputSubsampledImageSecCmdBuffer, &commandBufBeginParams);
        drawOutputSubsampledImage(vk, *m_outputSubsampledImageSecCmdBuffer);
        endCommandBuffer(vk, *m_outputSubsampledImageSecCmdBuffer);
    }

    beginCommandBuffer(vk, *m_cmdBuffer, 0u);

    // First render pass - render dynamic density map
    if (m_testParams.dynamicDensityMap)
    {
        std::vector<VkClearValue> attachmentClearValuesDDM{makeClearValueColorF32(1.0f, 1.0f, 1.0f, 1.0f)};

        const VkRenderPassAttachmentBeginInfo renderPassAttachmentBeginInfo{
            VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            1u,                                                  // uint32_t attachmentCount;
            &**m_densityMapImageViews[0]                         // const VkImageView* pAttachments;
        };

        const VkRenderPassBeginInfo renderPassBeginInfoProduceDynamicDensityMap{
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,                                     // VkStructureType sType;
            m_testParams.imagelessFramebuffer ? &renderPassAttachmentBeginInfo : nullptr, // const void* pNext;
            *m_renderPassProduceDynamicDensityMap,                                        // VkRenderPass renderPass;
            *m_framebufferProduceDynamicDensityMap,                                       // VkFramebuffer framebuffer;
            dynamicDensityMapRenderArea,                                                  // VkRect2D renderArea;
            static_cast<uint32_t>(attachmentClearValuesDDM.size()),                       // uint32_t clearValueCount;
            attachmentClearValuesDDM.data() // const VkClearValue* pClearValues;
        };

        renderPassWrapper->cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfoProduceDynamicDensityMap);

        if (m_testParams.groupParams->useSecondaryCmdBuffer)
            vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_dynamicDensityMapSecCmdBuffer);
        else
            drawDynamicDensityMap(vk, *m_cmdBuffer);

        renderPassWrapper->cmdEndRenderPass(*m_cmdBuffer);
    }

    // Render subsampled image
    {
        std::vector<VkImageView> imageViewsProduceSubsampledImage = {*m_colorImageView};
        if (isColorImageMultisampled)
            imageViewsProduceSubsampledImage.push_back(*m_colorResolvedImageView);
        if (m_testParams.makeCopy)
            imageViewsProduceSubsampledImage.push_back(*m_colorCopyImageView);
        imageViewsProduceSubsampledImage.push_back(**m_densityMapImageViews[0]);
        if (m_testParams.depthEnabled)
            imageViewsProduceSubsampledImage.push_back(*m_depthImageView);

        const VkRenderPassAttachmentBeginInfo renderPassAttachmentBeginInfo{
            VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO,            // VkStructureType sType;
            nullptr,                                                        // const void* pNext;
            static_cast<uint32_t>(imageViewsProduceSubsampledImage.size()), // uint32_t attachmentCount;
            imageViewsProduceSubsampledImage.data()                         // const VkImageView* pAttachments;
        };

        std::vector<VkClearValue> produceSubsampledImageClearValues = attachmentClearValues;
        if (m_testParams.depthEnabled)
        {
            // Note clear values are accessed by attachment index. The last attachment used before depth is the FDM and
            // it has a load operation. To correctly set the depth clear value, we need to push an extra one for the FDM
            // so the depth clear value sits at the  right index.
            produceSubsampledImageClearValues.push_back(emptyClearValue);
            produceSubsampledImageClearValues.push_back(depthClearValue);
        }

        const VkRenderPassBeginInfo renderPassBeginInfoProduceSubsampledImage{
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,                                     // VkStructureType sType;
            m_testParams.imagelessFramebuffer ? &renderPassAttachmentBeginInfo : nullptr, // const void* pNext;
            *m_renderPassProduceSubsampledImage,                                          // VkRenderPass renderPass;
            *m_framebufferProduceSubsampledImage,                                         // VkFramebuffer framebuffer;
            colorImageRenderArea,                                                         // VkRect2D renderArea;
            de::sizeU32(produceSubsampledImageClearValues),                               // uint32_t clearValueCount;
            de::dataOrNull(produceSubsampledImageClearValues), // const VkClearValue* pClearValues;
        };
        renderPassWrapper->cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfoProduceSubsampledImage);

        if (m_testParams.groupParams->useSecondaryCmdBuffer)
            vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_subsampledImageSecCmdBuffer);
        else
        {
            drawSubsampledImage(vk, *m_cmdBuffer);
            if (m_testParams.makeCopy)
            {
                renderPassWrapper->cmdNextSubpass(*m_cmdBuffer);
                drawCopySubsampledImage(vk, *m_cmdBuffer);
            }
        }

        renderPassWrapper->cmdEndRenderPass(*m_cmdBuffer, m_testParams.addZeroOffset, m_testParams.viewCount);
    }

    // Resample subsampled image
    if (m_testParams.subsampledLoads)
    {
        VkImageView pAttachments[] = {*m_colorImageView, **m_densityMapImageViews[1]};
        const VkRenderPassAttachmentBeginInfo renderPassAttachmentBeginInfo{
            VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            2u,                                                  // uint32_t attachmentCount;
            pAttachments                                         // const VkImageView* pAttachments;
        };

        const VkRenderPassBeginInfo renderPassBeginInfoUpdateSubsampledImage{
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,                                     // VkStructureType sType;
            m_testParams.imagelessFramebuffer ? &renderPassAttachmentBeginInfo : nullptr, // const void* pNext;
            *m_renderPassUpdateSubsampledImage,                                           // VkRenderPass renderPass;
            *m_framebufferUpdateSubsampledImage,                                          // VkFramebuffer framebuffer;
            makeRect2D(colorImageSize.width, colorImageSize.height),                      // VkRect2D renderArea;
            0u,                                                                           // uint32_t clearValueCount;
            nullptr // const VkClearValue* pClearValues;
        };
        renderPassWrapper->cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfoUpdateSubsampledImage);

        if (m_testParams.groupParams->useSecondaryCmdBuffer)
            vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_resampleSubsampledImageSecCmdBuffer);
        else
            drawResampleSubsampledImage(vk, *m_cmdBuffer);

        renderPassWrapper->cmdEndRenderPass(*m_cmdBuffer, m_testParams.addZeroOffset, m_testParams.viewCount);
    }

    // Copy subsampled image to normal image using sampler that is able to read from subsampled images
    // (subsampled image cannot be copied using vkCmdCopyImageToBuffer)
    const VkRenderPassAttachmentBeginInfo renderPassAttachmentBeginInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO, // VkStructureType sType;
        nullptr,                                             // const void* pNext;
        1u,                                                  // uint32_t attachmentCount;
        &*m_outputImageView                                  // const VkImageView* pAttachments;
    };

    const VkRenderPassBeginInfo renderPassBeginInfoOutputSubsampledImage{
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,                                     // VkStructureType sType;
        m_testParams.imagelessFramebuffer ? &renderPassAttachmentBeginInfo : nullptr, // const void* pNext;
        *m_renderPassOutputSubsampledImage,                                           // VkRenderPass renderPass;
        *m_framebufferOutputSubsampledImage,                                          // VkFramebuffer framebuffer;
        outputRenderArea,                                                             // VkRect2D renderArea;
        static_cast<uint32_t>(attachmentClearValues.size()),                          // uint32_t clearValueCount;
        attachmentClearValues.data() // const VkClearValue* pClearValues;
    };
    renderPassWrapper->cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfoOutputSubsampledImage);

    if (m_testParams.groupParams->useSecondaryCmdBuffer)
        vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_outputSubsampledImageSecCmdBuffer);
    else
        drawOutputSubsampledImage(vk, *m_cmdBuffer);

    renderPassWrapper->cmdEndRenderPass(*m_cmdBuffer);

    endCommandBuffer(vk, *m_cmdBuffer);
}

void FragmentDensityMapTestInstance::createCommandBufferForDynamicRendering(const DeviceInterface &vk,
                                                                            VkDevice vkDevice,
                                                                            const VkRect2D &dynamicDensityMapRenderArea,
                                                                            const VkRect2D &colorImageRenderArea,
                                                                            const VkRect2D &outputRenderArea)
{
    const bool isColorImageMultisampled = m_testParams.colorSamples != VK_SAMPLE_COUNT_1_BIT;
    std::vector<VkClearValue> attachmentClearValuesDDM{makeClearValueColorF32(1.0f, 1.0f, 1.0f, 1.0f)};
    const VkClearValue attachmentClearValue = makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f);
    const uint32_t attachmentCount          = 1 + m_testParams.makeCopy + isColorImageMultisampled;
    const std::vector<VkClearValue> attachmentClearValues(attachmentCount, attachmentClearValue);
    const VkImageSubresourceRange dynamicDensitMapSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0u,
                                                                   m_testParams.viewCount, 0u, 1u};
    const VkImageSubresourceRange colorSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, m_testParams.viewCount};
    const VkImageSubresourceRange outputSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};

    const VkImageMemoryBarrier dynamicDensitMapBarrier = makeImageMemoryBarrier(
        m_testParams.useMemoryAccess ? VK_ACCESS_MEMORY_READ_BIT :
                                       VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT, // VkAccessFlags srcAccessMask;
        m_testParams.useMemoryAccess ? VK_ACCESS_MEMORY_WRITE_BIT :
                                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // VkAccessFlags dstAccessMask;
        VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT,                    // VkImageLayout oldLayout;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,                            // VkImageLayout newLayout;
        **m_densityMapImages[0],                                             // VkImage image;
        dynamicDensitMapSubresourceRange // VkImageSubresourceRange subresourceRange;
    );

    const VkImageMemoryBarrier densityMapImageBarrier = makeImageMemoryBarrier(
        m_testParams.useMemoryAccess ? VK_ACCESS_MEMORY_WRITE_BIT :
                                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // VkAccessFlags srcAccessMask;
        m_testParams.useMemoryAccess ? VK_ACCESS_MEMORY_READ_BIT :
                                       VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT, // VkAccessFlags dstAccessMask;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,                                   // VkImageLayout oldLayout;
        VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT,                           // VkImageLayout newLayout;
        **m_densityMapImages[0],                                                    // VkImage image;
        colorSubresourceRange // VkImageSubresourceRange subresourceRange;
    );

    std::vector<VkImageMemoryBarrier> cbImageBarrier(
        3, makeImageMemoryBarrier(VK_ACCESS_NONE_KHR, // VkAccessFlags srcAccessMask;
                                  m_testParams.useMemoryAccess ?
                                      VK_ACCESS_MEMORY_WRITE_BIT :
                                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // VkAccessFlags dstAccessMask;
                                  VK_IMAGE_LAYOUT_UNDEFINED,                // VkImageLayout oldLayout;
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout newLayout;
                                  *m_colorImage,                            // VkImage image;
                                  colorSubresourceRange                     // VkImageSubresourceRange subresourceRange;
                                  ));
    cbImageBarrier[1].image                            = *m_colorResolvedImage;
    cbImageBarrier[1 + isColorImageMultisampled].image = *m_colorCopyImage;

    // VUID-vkCmdDraw-imageLayout-00344
    VkImage srcImage = *m_colorImage;
    if (isColorImageMultisampled)
        srcImage = *m_colorResolvedImage;
    else if (m_testParams.makeCopy)
        srcImage = *m_colorCopyImage;

    const VkImageMemoryBarrier subsampledImageBarrier = makeImageMemoryBarrier(
        m_testParams.useMemoryAccess ? VK_ACCESS_MEMORY_WRITE_BIT :
                                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // VkAccessFlags srcAccessMask;
        m_testParams.useMemoryAccess ? VK_ACCESS_MEMORY_READ_BIT :
                                       VK_ACCESS_SHADER_READ_BIT, // VkAccessFlags dstAccessMask;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,                 // VkImageLayout oldLayout;
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,                 // VkImageLayout newLayout;
        srcImage,                                                 // VkImage image;
        colorSubresourceRange                                     // VkImageSubresourceRange subresourceRange;
    );

    const VkImageMemoryBarrier outputImageBarrier = makeImageMemoryBarrier(
        VK_ACCESS_NONE_KHR, // VkAccessFlags srcAccessMask;
        m_testParams.useMemoryAccess ? VK_ACCESS_MEMORY_WRITE_BIT :
                                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // VkAccessFlags dstAccessMask;
        VK_IMAGE_LAYOUT_UNDEFINED,                                           // VkImageLayout oldLayout;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,                            // VkImageLayout newLayout;
        *m_outputImage,                                                      // VkImage image;
        outputSubresourceRange // VkImageSubresourceRange subresourceRange;
    );

    const VkRenderingFragmentDensityMapAttachmentInfoEXT densityMap0Attachment{
        VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_INFO_EXT, // VkStructureType sType;
        nullptr,                                                              // const void* pNext;
        **m_densityMapImageViews[0],                                          // VkImageView imageView;
        VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT                      // VkImageLayout imageLayout;
    };

    const VkRenderingFragmentDensityMapAttachmentInfoEXT densityMap1Attachment{
        VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_INFO_EXT,        // VkStructureType sType;
        nullptr,                                                                     // const void* pNext;
        m_testParams.subsampledLoads ? **m_densityMapImageViews[1] : VK_NULL_HANDLE, // VkImageView imageView;
        VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT                             // VkImageLayout imageLayout;
    };

    const VkRenderingAttachmentInfoKHR dynamicDensityMapColorAttachment{
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR, // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        **m_densityMapImageViews[0],                     // VkImageView imageView;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,        // VkImageLayout imageLayout;
        VK_RESOLVE_MODE_NONE,                            // VkResolveModeFlagBits resolveMode;
        VK_NULL_HANDLE,                                  // VkImageView resolveImageView;
        VK_IMAGE_LAYOUT_UNDEFINED,                       // VkImageLayout resolveImageLayout;
        VK_ATTACHMENT_LOAD_OP_CLEAR,                     // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,                    // VkAttachmentStoreOp storeOp;
        attachmentClearValuesDDM[0]                      // VkClearValue clearValue;
    };

    VkRenderingInfoKHR dynamicDensityMapRenderingInfo{
        VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
        nullptr,
        0u,                                // VkRenderingFlagsKHR flags;
        dynamicDensityMapRenderArea,       // VkRect2D renderArea;
        m_testParams.viewCount,            // uint32_t layerCount;
        m_viewMask,                        // uint32_t viewMask;
        1u,                                // uint32_t colorAttachmentCount;
        &dynamicDensityMapColorAttachment, // const VkRenderingAttachmentInfoKHR* pColorAttachments;
        nullptr,                           // const VkRenderingAttachmentInfoKHR* pDepthAttachment;
        nullptr,                           // const VkRenderingAttachmentInfoKHR* pStencilAttachment;
    };

    bool resolveFirstAttachment = isColorImageMultisampled && !m_testParams.makeCopy;
    // VUID-vkCmdBeginRendering-pRenderingInfo-09592
    const VkImageLayout firstColorAttachmentLayout =
        m_testParams.makeCopy ? VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    const VkRenderingAttachmentInfoKHR subsampledImageColorAttachments[2]{
        {
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR, // VkStructureType sType;
            nullptr,                                         // const void* pNext;
            *m_colorImageView,                               // VkImageView imageView;
            firstColorAttachmentLayout,                      // VkImageLayout imageLayout;
            resolveFirstAttachment ? VK_RESOLVE_MODE_AVERAGE_BIT :
                                     VK_RESOLVE_MODE_NONE,                       // VkResolveModeFlagBits resolveMode;
            resolveFirstAttachment ? *m_colorResolvedImageView : VK_NULL_HANDLE, // VkImageView resolveImageView;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,                            // VkImageLayout resolveImageLayout;
            VK_ATTACHMENT_LOAD_OP_CLEAR,                                         // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,                                        // VkAttachmentStoreOp storeOp;
            attachmentClearValues[0]                                             // VkClearValue clearValue;
        },
        {
            VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR, // VkStructureType sType;
            nullptr,                                         // const void* pNext;
            *m_colorCopyImageView,                           // VkImageView imageView;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,        // VkImageLayout imageLayout;
            isColorImageMultisampled ? VK_RESOLVE_MODE_AVERAGE_BIT :
                                       VK_RESOLVE_MODE_NONE,                       // VkResolveModeFlagBits resolveMode;
            isColorImageMultisampled ? *m_colorResolvedImageView : VK_NULL_HANDLE, // VkImageView resolveImageView;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,                              // VkImageLayout resolveImageLayout;
            VK_ATTACHMENT_LOAD_OP_CLEAR,                                           // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,                                          // VkAttachmentStoreOp storeOp;
            attachmentClearValues[0]                                               // VkClearValue clearValue;
        }};

    VkRenderingInfoKHR subsampledImageRenderingInfo{
        VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
        &densityMap0Attachment,
        0u,                              // VkRenderingFlagsKHR flags;
        colorImageRenderArea,            // VkRect2D renderArea;
        m_testParams.viewCount,          // uint32_t layerCount;
        m_viewMask,                      // uint32_t viewMask;
        1u + m_testParams.makeCopy,      // uint32_t colorAttachmentCount;
        subsampledImageColorAttachments, // const VkRenderingAttachmentInfoKHR* pColorAttachments;
        nullptr,                         // const VkRenderingAttachmentInfoKHR* pDepthAttachment;
        nullptr,                         // const VkRenderingAttachmentInfoKHR* pStencilAttachment;
    };

    const VkRenderingAttachmentInfoKHR resampleSubsampledImageColorAttachment{
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR, // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        *m_colorImageView,                               // VkImageView imageView;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,        // VkImageLayout imageLayout;
        VK_RESOLVE_MODE_NONE,                            // VkResolveModeFlagBits resolveMode;
        VK_NULL_HANDLE,                                  // VkImageView resolveImageView;
        VK_IMAGE_LAYOUT_UNDEFINED,                       // VkImageLayout resolveImageLayout;
        VK_ATTACHMENT_LOAD_OP_LOAD,                      // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,                    // VkAttachmentStoreOp storeOp;
        attachmentClearValues[0]                         // VkClearValue clearValue;
    };

    VkRenderingInfoKHR resampleSubsampledImageRenderingInfo{
        VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
        &densityMap1Attachment,
        0u,                                      // VkRenderingFlagsKHR flags;
        colorImageRenderArea,                    // VkRect2D renderArea;
        m_testParams.viewCount,                  // uint32_t layerCount;
        m_viewMask,                              // uint32_t viewMask;
        1u,                                      // uint32_t colorAttachmentCount;
        &resampleSubsampledImageColorAttachment, // const VkRenderingAttachmentInfoKHR* pColorAttachments;
        nullptr,                                 // const VkRenderingAttachmentInfoKHR* pDepthAttachment;
        nullptr,                                 // const VkRenderingAttachmentInfoKHR* pStencilAttachment;
    };

    const VkRenderingAttachmentInfoKHR copySubsampledColorAttachment{
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR, // VkStructureType sType;
        nullptr,                                         // const void* pNext;
        *m_outputImageView,                              // VkImageView imageView;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,        // VkImageLayout imageLayout;
        VK_RESOLVE_MODE_NONE,                            // VkResolveModeFlagBits resolveMode;
        VK_NULL_HANDLE,                                  // VkImageView resolveImageView;
        VK_IMAGE_LAYOUT_UNDEFINED,                       // VkImageLayout resolveImageLayout;
        VK_ATTACHMENT_LOAD_OP_CLEAR,                     // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,                    // VkAttachmentStoreOp storeOp;
        attachmentClearValues[0]                         // VkClearValue clearValue;
    };

    VkRenderingInfoKHR copySubsampledRenderingInfo{
        VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
        nullptr,
        0u,                             // VkRenderingFlagsKHR flags;
        outputRenderArea,               // VkRect2D renderArea;
        1u,                             // uint32_t layerCount;
        0u,                             // uint32_t viewMask;
        1u,                             // uint32_t colorAttachmentCount;
        &copySubsampledColorAttachment, // const VkRenderingAttachmentInfoKHR* pColorAttachments;
        nullptr,                        // const VkRenderingAttachmentInfoKHR* pDepthAttachment;
        nullptr,                        // const VkRenderingAttachmentInfoKHR* pStencilAttachment;
    };

    if (m_testParams.groupParams->useSecondaryCmdBuffer)
    {
        const VkFormat colorImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
        VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR, // VkStructureType sType;
            nullptr,                                                         // const void* pNext;
            0u,                                                              // VkRenderingFlagsKHR flags;
            m_viewMask,                                                      // uint32_t viewMask;
            1u,                                                              // uint32_t colorAttachmentCount;
            &m_testParams.densityMapFormat,                                  // const VkFormat* pColorAttachmentFormats;
            VK_FORMAT_UNDEFINED,                                             // VkFormat depthAttachmentFormat;
            VK_FORMAT_UNDEFINED,                                             // VkFormat stencilAttachmentFormat;
            VK_SAMPLE_COUNT_1_BIT // VkSampleCountFlagBits rasterizationSamples;
        };

        const VkCommandBufferInheritanceInfo bufferInheritanceInfo = initVulkanStructure(&inheritanceRenderingInfo);
        VkCommandBufferBeginInfo commandBufBeginParams{
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
            nullptr,                                     // const void* pNext;
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, // VkCommandBufferUsageFlags flags;
            &bufferInheritanceInfo};

        m_dynamicDensityMapSecCmdBuffer =
            allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
        m_subsampledImageSecCmdBuffer =
            allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
        m_resampleSubsampledImageSecCmdBuffer =
            allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
        m_outputSubsampledImageSecCmdBuffer =
            allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

        // Record secondary command buffers
        if (m_testParams.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
        {
            if (m_testParams.dynamicDensityMap)
            {
                vk.beginCommandBuffer(*m_dynamicDensityMapSecCmdBuffer, &commandBufBeginParams);
                vk.cmdBeginRendering(*m_dynamicDensityMapSecCmdBuffer, &dynamicDensityMapRenderingInfo);
                drawDynamicDensityMap(vk, *m_dynamicDensityMapSecCmdBuffer);
                endRendering(vk, *m_dynamicDensityMapSecCmdBuffer);
                endCommandBuffer(vk, *m_dynamicDensityMapSecCmdBuffer);
            }

            inheritanceRenderingInfo.pColorAttachmentFormats = &colorImageFormat;
            inheritanceRenderingInfo.rasterizationSamples    = m_testParams.colorSamples;
            vk.beginCommandBuffer(*m_subsampledImageSecCmdBuffer, &commandBufBeginParams);
            vk.cmdBeginRendering(*m_subsampledImageSecCmdBuffer, &subsampledImageRenderingInfo);
            drawSubsampledImage(vk, *m_subsampledImageSecCmdBuffer);
            if (m_testParams.makeCopy)
            {
                remapingBeforeCopySubsampledImage(vk, *m_subsampledImageSecCmdBuffer);
                drawCopySubsampledImage(vk, *m_subsampledImageSecCmdBuffer);
            }
            endRendering(vk, *m_subsampledImageSecCmdBuffer, m_testParams.addZeroOffset, m_testParams.viewCount);
            endCommandBuffer(vk, *m_subsampledImageSecCmdBuffer);

            if (m_testParams.subsampledLoads)
            {
                vk.beginCommandBuffer(*m_resampleSubsampledImageSecCmdBuffer, &commandBufBeginParams);
                vk.cmdBeginRendering(*m_resampleSubsampledImageSecCmdBuffer, &resampleSubsampledImageRenderingInfo);
                drawResampleSubsampledImage(vk, *m_resampleSubsampledImageSecCmdBuffer);
                endRendering(vk, *m_resampleSubsampledImageSecCmdBuffer, m_testParams.addZeroOffset,
                             m_testParams.viewCount);
                endCommandBuffer(vk, *m_resampleSubsampledImageSecCmdBuffer);
            }

            inheritanceRenderingInfo.viewMask             = 0u;
            inheritanceRenderingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            vk.beginCommandBuffer(*m_outputSubsampledImageSecCmdBuffer, &commandBufBeginParams);
            vk.cmdBeginRendering(*m_outputSubsampledImageSecCmdBuffer, &copySubsampledRenderingInfo);
            drawOutputSubsampledImage(vk, *m_outputSubsampledImageSecCmdBuffer);
            endRendering(vk, *m_outputSubsampledImageSecCmdBuffer);
            endCommandBuffer(vk, *m_outputSubsampledImageSecCmdBuffer);
        }
        else
        {
            commandBufBeginParams.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

            if (m_testParams.dynamicDensityMap)
            {
                vk.beginCommandBuffer(*m_dynamicDensityMapSecCmdBuffer, &commandBufBeginParams);
                drawDynamicDensityMap(vk, *m_dynamicDensityMapSecCmdBuffer);
                endCommandBuffer(vk, *m_dynamicDensityMapSecCmdBuffer);
            }

            inheritanceRenderingInfo.pColorAttachmentFormats = &colorImageFormat;
            inheritanceRenderingInfo.rasterizationSamples    = m_testParams.colorSamples;
            vk.beginCommandBuffer(*m_subsampledImageSecCmdBuffer, &commandBufBeginParams);
            drawSubsampledImage(vk, *m_subsampledImageSecCmdBuffer);
            endCommandBuffer(vk, *m_subsampledImageSecCmdBuffer);

            if (m_testParams.subsampledLoads)
            {
                vk.beginCommandBuffer(*m_resampleSubsampledImageSecCmdBuffer, &commandBufBeginParams);
                drawResampleSubsampledImage(vk, *m_resampleSubsampledImageSecCmdBuffer);
                endCommandBuffer(vk, *m_resampleSubsampledImageSecCmdBuffer);
            }

            inheritanceRenderingInfo.viewMask             = 0u;
            inheritanceRenderingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            vk.beginCommandBuffer(*m_outputSubsampledImageSecCmdBuffer, &commandBufBeginParams);
            drawOutputSubsampledImage(vk, *m_outputSubsampledImageSecCmdBuffer);
            endCommandBuffer(vk, *m_outputSubsampledImageSecCmdBuffer);
        }

        // Record primary command buffer
        beginCommandBuffer(vk, *m_cmdBuffer, 0u);

        // Render dynamic density map
        if (m_testParams.dynamicDensityMap)
        {
            // VUID-vkCmdPipelineBarrier-srcStageMask-03937
            // change layout of density map - after filling it layout was changed
            // to density map optimal but here we want to render values to it
            vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                  &dynamicDensitMapBarrier);

            if (m_testParams.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
                vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_dynamicDensityMapSecCmdBuffer);
            else
            {
                dynamicDensityMapRenderingInfo.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;
                vk.cmdBeginRendering(*m_cmdBuffer, &dynamicDensityMapRenderingInfo);
                vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_dynamicDensityMapSecCmdBuffer);
                endRendering(vk, *m_cmdBuffer, m_testParams.addZeroOffset, m_testParams.viewCount);
            }

            // barrier that will change layout of density map
            vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT, 0, 0, nullptr, 0, nullptr, 1,
                                  &densityMapImageBarrier);
        }

        // VUID-vkCmdPipelineBarrier-srcStageMask-03937
        // VUID-vkCmdBeginRendering-pRenderingInfo-09592
        // barrier that will change layout of color and resolve attachments
        if (m_testParams.makeCopy)
            cbImageBarrier[0].newLayout = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR;
        vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr,
                              1 + isColorImageMultisampled + m_testParams.makeCopy, cbImageBarrier.data());

        // Render subsampled image
        if (m_testParams.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
            vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_subsampledImageSecCmdBuffer);
        else
        {
            subsampledImageRenderingInfo.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;
            vk.cmdBeginRendering(*m_cmdBuffer, &subsampledImageRenderingInfo);
            vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_subsampledImageSecCmdBuffer);
            endRendering(vk, *m_cmdBuffer, m_testParams.addZeroOffset, m_testParams.viewCount);
        }

        // Resample subsampled image
        if (m_testParams.subsampledLoads)
        {
            if (m_testParams.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
                vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_resampleSubsampledImageSecCmdBuffer);
            else
            {
                resampleSubsampledImageRenderingInfo.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;
                vk.cmdBeginRendering(*m_cmdBuffer, &resampleSubsampledImageRenderingInfo);
                vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_resampleSubsampledImageSecCmdBuffer);
                endRendering(vk, *m_cmdBuffer, m_testParams.addZeroOffset, m_testParams.viewCount);
            }
        }

        // barrier that ensures writing to colour image has completed.
        vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1u,
                              &subsampledImageBarrier);

        // VUID-vkCmdPipelineBarrier-srcStageMask-03937
        // barrier that will change layout of output image
        vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1,
                              &outputImageBarrier);

        if (m_testParams.groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
            vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_outputSubsampledImageSecCmdBuffer);
        else
        {
            copySubsampledRenderingInfo.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;
            vk.cmdBeginRendering(*m_cmdBuffer, &copySubsampledRenderingInfo);
            vk.cmdExecuteCommands(*m_cmdBuffer, 1u, &*m_outputSubsampledImageSecCmdBuffer);
            endRendering(vk, *m_cmdBuffer);
        }

        endCommandBuffer(vk, *m_cmdBuffer);
    }
    else
    {
        beginCommandBuffer(vk, *m_cmdBuffer, 0u);

        // First render pass - render dynamic density map
        if (m_testParams.dynamicDensityMap)
        {
            // VUID-vkCmdPipelineBarrier-srcStageMask-03937
            // change layout of density map - after filling it layout was changed
            // to density map optimal but here we want to render values to it
            vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                  &dynamicDensitMapBarrier);

            vk.cmdBeginRendering(*m_cmdBuffer, &dynamicDensityMapRenderingInfo);
            drawDynamicDensityMap(vk, *m_cmdBuffer);
            endRendering(vk, *m_cmdBuffer);

            // barrier that will change layout of density map
            vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT, 0, 0, nullptr, 0, nullptr, 1,
                                  &densityMapImageBarrier);
        }

        // VUID-vkCmdPipelineBarrier-srcStageMask-03937
        // barrier that will change layout of color and resolve attachments
        if (m_testParams.makeCopy)
            cbImageBarrier[0].newLayout = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR;
        vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr,
                              1 + isColorImageMultisampled + m_testParams.makeCopy, cbImageBarrier.data());

        // Render subsampled image
        vk.cmdBeginRendering(*m_cmdBuffer, &subsampledImageRenderingInfo);
        drawSubsampledImage(vk, *m_cmdBuffer);
        if (m_testParams.makeCopy)
        {
            remapingBeforeCopySubsampledImage(vk, *m_cmdBuffer);
            drawCopySubsampledImage(vk, *m_cmdBuffer);
        }
        endRendering(vk, *m_cmdBuffer, m_testParams.addZeroOffset, m_testParams.viewCount);

        // Resample subsampled image
        if (m_testParams.subsampledLoads)
        {
            vk.cmdBeginRendering(*m_cmdBuffer, &resampleSubsampledImageRenderingInfo);
            drawResampleSubsampledImage(vk, *m_cmdBuffer);
            endRendering(vk, *m_cmdBuffer, m_testParams.addZeroOffset, m_testParams.viewCount);
        }

        // barrier that ensures writing to colour image has completed.
        vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                              VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1u,
                              &subsampledImageBarrier);

        // VUID-vkCmdPipelineBarrier-srcStageMask-03937
        // barrier that will change layout of output image
        vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1,
                              &outputImageBarrier);

        vk.cmdBeginRendering(*m_cmdBuffer, &copySubsampledRenderingInfo);
        drawOutputSubsampledImage(vk, *m_cmdBuffer);
        endRendering(vk, *m_cmdBuffer);

        endCommandBuffer(vk, *m_cmdBuffer);
    }
}

void FragmentDensityMapTestInstance::endRendering(const DeviceInterface &vk, VkCommandBuffer cmdBuffer,
                                                  bool addZeroOffset, uint32_t viewCount)
{
    if (addZeroOffset)
    {
        DE_ASSERT(viewCount > 0u);
        const std::vector<VkOffset2D> offsets(viewCount, VkOffset2D{0, 0});
        const VkRenderPassFragmentDensityMapOffsetEndInfoEXT offsetEndInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_OFFSET_END_INFO_EXT,
            nullptr,
            de::sizeU32(offsets),
            de::dataOrNull(offsets),
        };
        const VkRenderingEndInfoEXT renderingEndInfo = initVulkanStructureConst(&offsetEndInfo);
        vk.cmdEndRendering2KHR(cmdBuffer, &renderingEndInfo);
    }
    else
    {
        vk.cmdEndRendering(cmdBuffer);
    }
}

tcu::TestStatus FragmentDensityMapTestInstance::iterate(void)
{
    const auto &deviceHelper = getDeviceHelper(m_context);
    submitCommandsAndWait(deviceHelper.getDeviceInterface(), deviceHelper.getDevice(), deviceHelper.getQueue(),
                          m_cmdBuffer.get());

    // approximations used when coarse reconstruction is specified are implementation defined
    if (m_testParams.coarseReconstruction)
        return tcu::TestStatus::pass("Pass");

    return verifyImage(deviceHelper);
}

struct Vec4Sorter
{
    bool operator()(const tcu::Vec4 &lhs, const tcu::Vec4 &rhs) const
    {
        if (lhs.x() != rhs.x())
            return lhs.x() < rhs.x();
        if (lhs.y() != rhs.y())
            return lhs.y() < rhs.y();
        if (lhs.z() != rhs.z())
            return lhs.z() < rhs.z();
        return lhs.w() < rhs.w();
    }
};

tcu::TestStatus FragmentDensityMapTestInstance::verifyImage(const DeviceHelper &deviceHelper)
{
    const auto &vk      = deviceHelper.getDeviceInterface();
    const auto vkDevice = deviceHelper.getDevice();
    const auto qfIndex  = deviceHelper.getQueueFamilyIndex();
    const auto queue    = deviceHelper.getQueue();
    auto &memAlloc      = deviceHelper.getAllocator();
    tcu::UVec2 renderSize(m_renderSize.x(), m_renderSize.y());

    de::UniquePtr<tcu::TextureLevel> outputImage(pipeline::readColorAttachment(vk, vkDevice, queue, qfIndex, memAlloc,
                                                                               *m_outputImage, VK_FORMAT_R8G8B8A8_UNORM,
                                                                               renderSize)
                                                     .release());
    const tcu::ConstPixelBufferAccess &outputAccess(outputImage->getAccess());
    tcu::TestLog &log(m_context.getTestContext().getLog());

    // Log images
    log << tcu::TestLog::ImageSet("Result", "Result images")
        << tcu::TestLog::Image("Rendered", "Rendered output image", outputAccess) << tcu::TestLog::EndImageSet;

    int32_t noColorCount = 0;
    uint32_t estimatedColorCount =
        m_testParams.viewCount * m_testParams.fragmentArea.x() * m_testParams.fragmentArea.y();
    float densityMult = m_densityValue.x() * m_densityValue.y();

    // Create histogram of all image colors, check the value of inverted FragSizeEXT
    std::map<tcu::Vec4, uint32_t, Vec4Sorter> colorCount;
    for (int y = 0; y < outputAccess.getHeight(); y++)
    {
        for (int x = 0; x < outputAccess.getWidth(); x++)
        {
            tcu::Vec4 outputColor = outputAccess.getPixel(x, y);
            float densityClamped  = outputColor.z() * outputColor.w();

            // for multiviewport cases we check only pixels to which we render
            if (m_testParams.multiViewport && outputColor.x() < 0.01f)
            {
                ++noColorCount;
                continue;
            }

            if ((densityClamped + 0.01) < densityMult)
                return tcu::TestStatus::fail("Wrong value of FragSizeEXT variable");

            auto it = colorCount.find(outputColor);
            if (it == end(colorCount))
                it = colorCount.insert({outputColor, 0u}).first;
            it->second++;
        }
    }

    // Check if color count is the same as estimated one
    for (const auto &color : colorCount)
    {
        if (color.second > estimatedColorCount)
            return tcu::TestStatus::fail("Wrong color count");
    }

    // For multiviewport cases ~75% of fragments should be black;
    // The margin of 100 fragments is used to compensate cases where
    // we can't fit all views in a same way to final 64x64 image
    // (64 can't be evenly divide for 6 views)
    int32_t estimatedNoColorCount = m_renderSize.x() * m_renderSize.y() * 3 / 4;
    if (m_testParams.multiViewport && std::abs(noColorCount - estimatedNoColorCount) > 100)
        return tcu::TestStatus::fail("Wrong number of fragments with black color");

    return tcu::TestStatus::pass("Pass");
}

enum class OffsetType
{
    NONE     = 0,
    POSITIVE = 1,
    NEGATIVE = 2,
};

int getSign(OffsetType offsetType)
{
    switch (offsetType)
    {
    case OffsetType::NONE:
        return 0;
    case OffsetType::POSITIVE:
        return 1;
    case OffsetType::NEGATIVE:
        return -1;
    default:
        break;
    }

    DE_ASSERT(false);
    return std::numeric_limits<int>::max();
}

tcu::IVec3 getMinTexelSize(const VkPhysicalDeviceFragmentDensityMapPropertiesEXT &properties)
{
    return tcu::UVec3(std::max(properties.minFragmentDensityTexelSize.width, 1u),
                      std::max(properties.minFragmentDensityTexelSize.height, 1u), 1u)
        .asInt();
}

struct FDMOffsetBaseParams
{
    const SharedGroupParams testGroupParams;
    OffsetType horizontalOffset;
    OffsetType verticalOffset;
    bool multiView;
    bool resumeRendering;         // Only used for dynamic rendering.
    std::vector<bool> iterations; // How many times to run the main loop and if we should force no offsets with each.

    FDMOffsetBaseParams(const SharedGroupParams groupParams, OffsetType horizontalOffset_, OffsetType verticalOffset_,
                        bool multiView_, bool resumeRendering_)
        : testGroupParams(groupParams)
        , horizontalOffset(horizontalOffset_)
        , verticalOffset(verticalOffset_)
        , multiView(multiView_)
        , resumeRendering(resumeRendering_)
        , iterations(1, false)
    {
        // We do not support both at the same time currently.
        DE_ASSERT(horizontalOffset == OffsetType::NONE || verticalOffset == OffsetType::NONE);

        if (resumeRendering)
            DE_ASSERT(testGroupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING);
    }

    virtual ~FDMOffsetBaseParams() = default;

    virtual uint32_t getLayerCount() const
    {
        return (multiView ? 2u : 1u);
    }

    virtual tcu::IVec3 getFramebufferExtent() const
    {
        return tcu::IVec3(1024, 1024, 1);
    }

    virtual tcu::IVec3 getFragmentDensityMapExtent(
        const VkPhysicalDeviceFragmentDensityMapPropertiesEXT &properties) const
    {
        // Minimum texel size by default.
        const auto minTexelSize = getMinTexelSize(properties);
        const auto fbExtent     = getFramebufferExtent();
        return fbExtent / minTexelSize;
    }

    virtual std::vector<tcu::IVec2> getOffsets(const VkPhysicalDeviceFragmentDensityMapOffsetPropertiesEXT *) const = 0;

    virtual tcu::Vec4 getClearColor() const
    {
        return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    virtual tcu::Vec4 getZeroResColor() const
    {
        return tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
    }

    virtual tcu::Vec4 getHighResColor() const
    {
        return tcu::Vec4(1.0f, 0.0f, 1.0f, 1.0f);
    }

    virtual tcu::Vec4 getLowResColor() const
    {
        return tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }
};

using FDMOffsetParamsPtr = de::SharedPtr<const FDMOffsetBaseParams>;
using TextureLevelPtr    = std::unique_ptr<tcu::TextureLevel>;
using TexLevelsVec       = std::vector<TextureLevelPtr>;

class FDMOffsetBaseInstance : public vkt::TestInstance
{
public:
    FDMOffsetBaseInstance(Context &context, FDMOffsetParamsPtr params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~FDMOffsetBaseInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    struct QuadInfo
    {
        float xBegin;
        float xEnd;
        float yBegin;
        float yEnd;
    };

    virtual void prepareFDMAccess(tcu::PixelBufferAccess &fdmAccess,
                                  const std::vector<tcu::IVec2> &fdmOffsets) const = 0;
    virtual QuadInfo getQuadInfo(const tcu::IVec3 &fbExtent, const tcu::IVec3 &fdmExtent) const;
    virtual void prepareReferences(TexLevelsVec &references, const TexLevelsVec &results, const QuadInfo &quadInfo,
                                   const std::vector<tcu::IVec2> &fdmOffsets) const = 0;
    virtual void checkResults(tcu::TestLog &log, const TexLevelsVec &references, const TexLevelsVec &results,
                              const std::vector<tcu::IVec2> &fdmOffsets) const      = 0;

    FDMOffsetParamsPtr m_params;
};

class FDMOffsetBaseCase : public vkt::TestCase
{
public:
    FDMOffsetBaseCase(tcu::TestContext &testCtx, const std::string &name, FDMOffsetParamsPtr params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~FDMOffsetBaseCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;

protected:
    FDMOffsetParamsPtr m_params;
};

void FDMOffsetBaseCase::checkSupport(Context &context) const
{
#ifdef USE_QCOM_OFFSET_EXT
    context.requireDeviceFunctionality("VK_QCOM_fragment_density_map_offset");
#else
    context.requireDeviceFunctionality("VK_EXT_fragment_density_map_offset");
#endif

    if (m_params->testGroupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING)
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
    else if (m_params->testGroupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
        context.requireDeviceFunctionality("VK_KHR_create_renderpass2");

    if (m_params->multiView)
        context.requireDeviceFunctionality("VK_KHR_multiview");

    const auto &fdmoProperties = context.getFragmentDensityMapOffsetPropertiesEXT();

    {
        const auto offsets = m_params->getOffsets(nullptr);

        const auto checkOffset = [](int32_t offset, int32_t granularity, const std::string &dim)
        {
            if (granularity == 0)
            {
                std::ostringstream msg;
                msg << dim << " granularity is zero";
                TCU_FAIL(msg.str());
            }

            if (offset % granularity != 0)
            {
                std::ostringstream msg;
                msg << dim << " offset (" << offset << ") is not a multiple of the granularity (" << granularity << ")";
                TCU_THROW(NotSupportedError, msg.str());
            }
        };

        const auto zeroOffset = tcu::IVec2(0, 0);
        for (const auto &singleOffset : offsets)
        {
            if (singleOffset == zeroOffset)
                continue;

            checkOffset(singleOffset.x(), fdmoProperties.fragmentDensityOffsetGranularity.width, "Horizontal");
            checkOffset(singleOffset.y(), fdmoProperties.fragmentDensityOffsetGranularity.height, "Vertical");
        }
    }
}

void FDMOffsetBaseCase::initPrograms(SourceCollections &dst) const
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "layout (location=0) in vec4 inPos;\n"
         << "void main(void) {\n"
         << "    gl_Position = inPos;\n"
         << "}\n";
    dst.glslSources.add("vert") << glu::VertexSource(vert.str());

    std::ostringstream frag;
    frag << "#version 460\n"
         << "#extension GL_EXT_fragment_invocation_density : require\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main(void) {\n"
         << "    const vec4 zeroResColor = vec4" << m_params->getZeroResColor() << ";\n"
         << "    const vec4 highResColor = vec4" << m_params->getHighResColor() << ";\n"
         << "    const vec4 lowResColor = vec4" << m_params->getLowResColor() << ";\n"
         << "    const int area = gl_FragSizeEXT.x * gl_FragSizeEXT.y;\n"
         << "    if (area == 0) { outColor = zeroResColor; }\n"
         << "    else if (area == 1) { outColor = highResColor; }\n"
         << "    else { outColor = lowResColor; }\n"
         << "}\n";
    dst.glslSources.add("frag") << glu::FragmentSource(frag.str());

    // Shaders for copying the framebuffer to a storage texel buffer.
    const auto fbExtent = m_params->getFramebufferExtent();

    // Draws full-screen triangle.
    std::ostringstream vertCopy;
    vertCopy << "#version 460\n"
             << "vec2 positions[3] = vec2[](\n"
             << "    vec2(-1.0, -1.0),\n"
             << "    vec2( 3.0, -1.0),\n"
             << "    vec2(-1.0,  3.0)\n"
             << ");\n"
             << "void main(void) {\n"
             << "    gl_Position = vec4(positions[gl_VertexIndex % 3], 0.0, 1.0);\n"
             << "}\n";
    dst.glslSources.add("vert-copy") << glu::VertexSource(vertCopy.str());

    const auto &multiView = m_params->multiView;

    std::ostringstream fragCopy;
    fragCopy
        << "#version 460\n"
        << (multiView ? "#extension GL_EXT_multiview : require\n" : "") << "layout (set=0, binding=0) uniform "
        << (multiView ? "sampler2DArray" : "sampler2D") << " inSampler;\n"
        << "layout (set=0, binding=1, rgba8) uniform imageBuffer outImg;\n"
        << "void main (void) {\n"
        << "    const int imageWidth = " << fbExtent.x() << ";\n"
        << "    const int imageHeight = " << fbExtent.y() << ";\n"
        << "    const vec2 whVec = vec2(imageWidth, imageHeight);\n"
        << "    const int layerSize = imageWidth * imageHeight;\n"
        << "    const int viewIndex = " << (multiView ? "gl_ViewIndex" : "0") << ";\n"
        << (multiView ? "    const vec3 coord = vec3(gl_FragCoord.xy, viewIndex) / vec3(whVec, 1.0);\n" :
                        "    const vec2 coord = vec2(gl_FragCoord.xy) / whVec;\n")
        << "    const vec4 color = texture(inSampler, coord);\n"
        << "    const int storePos = layerSize * viewIndex + int(gl_FragCoord.y) * imageWidth + int(gl_FragCoord.x);\n"
        << "    imageStore(outImg, storePos, color);\n"
        << "}\n";
    dst.glslSources.add("frag-copy") << glu::FragmentSource(fragCopy.str());
}

enum class QuadPiece
{
    ALL    = 0,
    FIRST  = 1,
    SECOND = 2,
};

tcu::TestStatus FDMOffsetBaseInstance::iterate()
{
    const auto &deviceHelper = getDeviceHelper(m_context);
    const auto &vkd          = deviceHelper.getDeviceInterface();
    const auto device        = deviceHelper.getDevice();
    const auto qfIndex       = deviceHelper.getQueueFamilyIndex();
    const auto queue         = deviceHelper.getQueue();
    auto &alloc              = deviceHelper.getAllocator();

    const auto fbFlags    = (VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT | VK_IMAGE_CREATE_FRAGMENT_DENSITY_MAP_OFFSET_BIT_EXT);
    const auto fbFormat   = VK_FORMAT_R8G8B8A8_UNORM;
    const auto fbExtent   = m_params->getFramebufferExtent();
    const auto fbVkExtent = makeExtent3D(fbExtent);
    const auto fbUsage    = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    const auto &fdmProperties       = m_context.getFragmentDensityMapPropertiesEXT();
    const auto &fdmOffsetProperties = m_context.getFragmentDensityMapOffsetPropertiesEXT();
    const auto bindPoint            = VK_PIPELINE_BIND_POINT_GRAPHICS;

    const bool isDynamicRendering = (m_params->testGroupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING);
    const bool useSecondary       = m_params->testGroupParams->useSecondaryCmdBuffer;
    const bool allInSecondary =
        (useSecondary && m_params->testGroupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass);
    const bool needsIheritance     = (useSecondary && !allInSecondary);
    const bool multipleSecondaries = (useSecondary && m_params->resumeRendering && !allInSecondary);

    const auto layerCount = m_params->getLayerCount();
    const auto viewMask   = ((1u << layerCount) - 1u);
    const auto viewType   = (m_params->multiView ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
    const auto colorSRR   = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, layerCount);
    const auto colorSRL   = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, layerCount);
    const auto fdmOffsets = m_params->getOffsets(&fdmOffsetProperties);
    const tcu::IVec3 resultExtent(fbExtent.x(), fbExtent.y(), layerCount);

    const VkImageCreateInfo fbImageInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        fbFlags,
        VK_IMAGE_TYPE_2D,
        fbFormat,
        fbVkExtent,
        1u,
        layerCount,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        fbUsage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    ImageWithMemory fbImage(vkd, device, alloc, fbImageInfo, MemoryRequirement::Any);
    const auto fbView = makeImageView(vkd, device, *fbImage, viewType, fbFormat, colorSRR);

    // Storage texel buffer.
    const auto fbTcuFormat = mapVkFormat(fbFormat);
    DE_ASSERT(fbExtent.z() == 1);
    const auto texelBufferSize  = static_cast<VkDeviceSize>(tcu::getPixelSize(fbTcuFormat) * resultExtent.x() *
                                                           resultExtent.y() * resultExtent.z());
    const auto texelBufferUsage = VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
    const auto texelBufferInfo  = makeBufferCreateInfo(texelBufferSize, texelBufferUsage);
    BufferWithMemory texelBuffer(vkd, device, alloc, texelBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &bufferAlloc = texelBuffer.getAllocation();
        memset(bufferAlloc.getHostPtr(), 0, static_cast<size_t>(texelBufferSize));
    }
    const auto texelBufferView = makeBufferView(vkd, device, *texelBuffer, fbFormat, 0ull, VK_WHOLE_SIZE);

    // Sampler.
    const auto samplerFlags = VK_SAMPLER_CREATE_SUBSAMPLED_BIT_EXT;

    const VkSamplerCreateInfo samplerInfo{
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        nullptr,
        samplerFlags,
        VK_FILTER_NEAREST,
        VK_FILTER_NEAREST,
        VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        0.0f,
        VK_FALSE,
        0.0f,
        VK_FALSE,
        VK_COMPARE_OP_NEVER,
        0.0f,
        0.0f,
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        VK_FALSE,
    };

    const auto sampler = createSampler(vkd, device, &samplerInfo);

    const auto fdmFlags  = (VK_IMAGE_CREATE_FRAGMENT_DENSITY_MAP_OFFSET_BIT_EXT);
    const auto fdmFormat = VK_FORMAT_R8G8_UNORM;
    const auto fdmExtent = m_params->getFragmentDensityMapExtent(fdmProperties);
    const auto fdmUsage  = (VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    const VkImageCreateInfo fdmImageInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        fdmFlags,
        VK_IMAGE_TYPE_2D,
        fdmFormat,
        makeExtent3D(fdmExtent),
        1u,
        layerCount,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        fdmUsage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };

    ImageWithMemory fdmImage(vkd, device, alloc, fdmImageInfo, MemoryRequirement::Any);
    const auto fdmView = makeImageView(vkd, device, *fdmImage, viewType, fdmFormat, colorSRR);

    // Host fragment density map level. These values will be copied to a buffer and uploaded to the FDM map image.
    const auto fdmTcuFormat = mapVkFormat(fdmFormat);
    DE_ASSERT(fdmExtent.z() == 1);
    tcu::TextureLevel fdmLevel(fdmTcuFormat, fdmExtent.x(), fdmExtent.y(), static_cast<int>(layerCount));
    tcu::PixelBufferAccess fdmAccess = fdmLevel.getAccess();

    // Fill FDM buffer with the desired values.
    prepareFDMAccess(fdmAccess, fdmOffsets);

    // Create an auxiliary buffer and fill it with the texture level, then copy it to the FDM image.
    const auto fdmPixelSize      = tcu::getPixelSize(fdmTcuFormat);
    const auto fdmXferBufferSize = static_cast<VkDeviceSize>(fdmPixelSize * fdmExtent.x() * fdmExtent.y() * layerCount);
    const auto fdmXferBufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    const auto fdmXferBufferInfo  = makeBufferCreateInfo(fdmXferBufferSize, fdmXferBufferUsage);
    BufferWithMemory fdmXferBuffer(vkd, device, alloc, fdmXferBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &bufferAlloc = fdmXferBuffer.getAllocation();
        memcpy(bufferAlloc.getHostPtr(), fdmAccess.getDataPtr(), static_cast<size_t>(fdmXferBufferSize));
    }
    {
        CommandPoolWithBuffer cmd(vkd, device, qfIndex);
        beginCommandBuffer(vkd, *cmd.cmdBuffer);
        {
            const auto preCopyBarrier =
                makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, *fdmImage, colorSRR);
            cmdPipelineImageMemoryBarrier(vkd, *cmd.cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                          VK_PIPELINE_STAGE_TRANSFER_BIT, &preCopyBarrier);
        }
        {
            const auto copyRegion = makeBufferImageCopy(makeExtent3D(fdmExtent), colorSRL);
            vkd.cmdCopyBufferToImage(*cmd.cmdBuffer, *fdmXferBuffer, *fdmImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     1u, &copyRegion);
        }
        {
            const auto postCopyBarrier =
                makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, 0u, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT, *fdmImage, colorSRR);
            cmdPipelineImageMemoryBarrier(vkd, *cmd.cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, &postCopyBarrier);
        }
        endCommandBuffer(vkd, *cmd.cmdBuffer);
        submitCommandsAndWait(vkd, device, queue, *cmd.cmdBuffer);
    }

    // Generate geometry.
    const auto quadInfo = getQuadInfo(fbExtent, fdmExtent);

    std::vector<tcu::Vec4> vertices;
    vertices.reserve(6u);
    vertices.emplace_back(quadInfo.xBegin, quadInfo.yBegin, 0.0f, 1.0f);
    vertices.emplace_back(quadInfo.xBegin, quadInfo.yEnd, 0.0f, 1.0f);
    vertices.emplace_back(quadInfo.xEnd, quadInfo.yBegin, 0.0f, 1.0f);
    vertices.emplace_back(quadInfo.xEnd, quadInfo.yBegin, 0.0f, 1.0f);
    vertices.emplace_back(quadInfo.xBegin, quadInfo.yEnd, 0.0f, 1.0f);
    vertices.emplace_back(quadInfo.xEnd, quadInfo.yEnd, 0.0f, 1.0f);

    const auto vertexBufferSize   = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vertexBufferInfo   = makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    const auto vertexBufferOffset = static_cast<VkDeviceSize>(0);
    BufferWithMemory vertexBuffer(vkd, device, alloc, vertexBufferInfo, MemoryRequirement::HostVisible);
    {
        auto &bufferAlloc = vertexBuffer.getAllocation();
        memcpy(bufferAlloc.getHostPtr(), de::dataOrNull(vertices), de::dataSize(vertices));
    }

    // Render pass and framebuffer.
    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> framebuffer;

    if (m_params->testGroupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
    {
        const std::vector<VkAttachmentDescription2> attachments{
            // Color attachment.
            VkAttachmentDescription2{
                VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
                nullptr,
                0u,
                fbImageInfo.format,
                fbImageInfo.samples,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE,
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // Stencil.
                VK_ATTACHMENT_STORE_OP_DONT_CARE, // Stencil.
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
            // Fragment density map.
            VkAttachmentDescription2{
                VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
                nullptr,
                0u,
                fdmImageInfo.format,
                fdmImageInfo.samples,
                VK_ATTACHMENT_LOAD_OP_LOAD,
                VK_ATTACHMENT_STORE_OP_DONT_CARE,
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // Stencil.
                VK_ATTACHMENT_STORE_OP_DONT_CARE, // Stencil.
                VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT,
                VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT,
            },
        };

        const VkAttachmentReference2 colorRef{
            VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
            nullptr,
            0u,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT,
        };

        const std::vector<VkSubpassDescription2> subpasses{
            VkSubpassDescription2{
                VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
                nullptr,
                0u,
                bindPoint,
                viewMask,
                0u,
                nullptr,
                1u,
                &colorRef,
                nullptr,
                nullptr,
                0u,
                nullptr,
            },
        };

        const VkRenderPassFragmentDensityMapCreateInfoEXT rpFDMInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT,
            nullptr,
            {
                1u, // Fragment density map attachment index.
                VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT,
            },
        };

        // Render pass.
        const VkRenderPassCreateInfo2 rpCreateInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
            &rpFDMInfo,
            0u,
            de::sizeU32(attachments),
            de::dataOrNull(attachments),
            de::sizeU32(subpasses),
            de::dataOrNull(subpasses),
            0u,
            nullptr,
            0u,
            nullptr,
        };

        renderPass = createRenderPass2(vkd, device, &rpCreateInfo);

        const std::vector<VkImageView> imgViews{*fbView, *fdmView};

        const VkFramebufferCreateInfo fbCreateInfo = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            nullptr,
            0u,
            *renderPass,
            de::sizeU32(imgViews),
            de::dataOrNull(imgViews),
            fbVkExtent.width,
            fbVkExtent.height,
            1u, // Note for multiview this is still specified as 1.
        };

        framebuffer = createFramebuffer(vkd, device, &fbCreateInfo);
    }
    else if (isDynamicRendering)
    {
    }
    else
        DE_ASSERT(false);

    const VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        nullptr,
        viewMask,
        1u,
        &fbFormat,
        VK_FORMAT_UNDEFINED,
        VK_FORMAT_UNDEFINED,
    };

    const auto graphicsPipelineCreateInfoPNext = (isDynamicRendering ? &pipelineRenderingCreateInfo : nullptr);
    const auto pipelineCreateFlags =
        (isDynamicRendering ?
             static_cast<VkPipelineCreateFlags>(VK_PIPELINE_CREATE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_BIT_EXT) :
             0u);

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));
    const auto topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Graphics pipeline.
    const auto graphicsPipelineLayout = makePipelineLayout(vkd, device);
    const auto &binaries              = m_context.getBinaryCollection();
    const auto vertModule             = createShaderModule(vkd, device, binaries.get("vert"));
    const auto fragModule             = createShaderModule(vkd, device, binaries.get("frag"));
    const auto graphicsPipeline       = makeGraphicsPipeline(
        vkd, device, *graphicsPipelineLayout, *vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *fragModule,
        *renderPass, viewports, scissors, topology, 0u, 0u, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        graphicsPipelineCreateInfoPNext, pipelineCreateFlags);

    // Copy pipeline: get the framebuffer out to a storage texel buffer.
    const auto vertCopyModule = createShaderModule(vkd, device, binaries.get("vert-copy"));
    const auto fragCopyModule = createShaderModule(vkd, device, binaries.get("frag-copy"));
    const auto copyStage      = VK_SHADER_STAGE_FRAGMENT_BIT;

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER);
    const auto descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, copyStage, &sampler.get());
    setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, copyStage);
    const auto copySetLayout = setLayoutBuilder.build(vkd, device);
    const auto copySet       = makeDescriptorSet(vkd, device, *descriptorPool, *copySetLayout);

    using Location = DescriptorSetUpdateBuilder::Location;
    DescriptorSetUpdateBuilder setUpdateBuilder;
    const auto fbDescInfo = makeDescriptorImageInfo(VK_NULL_HANDLE, *fbView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    setUpdateBuilder.writeSingle(*copySet, Location::binding(0u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                 &fbDescInfo);
    setUpdateBuilder.writeSingle(*copySet, Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
                                 &texelBufferView.get());
    setUpdateBuilder.update(vkd, device);

    const VkPipelineVertexInputStateCreateInfo copyInputStateInfo = initVulkanStructure();

    const VkRenderPassMultiviewCreateInfo copyRenderPassMultiviewInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO, nullptr, 1u, &viewMask, 0u, nullptr, 0u, nullptr,
    };
    const auto copyRenderPassPnext = (m_params->multiView ? &copyRenderPassMultiviewInfo : nullptr);

    const auto copyPipelineLayout = makePipelineLayout(vkd, device, *copySetLayout);
    const auto copyRenderPass =
        makeRenderPass(vkd, device, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_CLEAR,
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                       nullptr, copyRenderPassPnext);
    const auto copyFramebuffer =
        makeFramebuffer(vkd, device, *copyRenderPass, 0u, nullptr, fbVkExtent.width, fbVkExtent.height, 1u);
    const auto copyPipeline = makeGraphicsPipeline(vkd, device, *copyPipelineLayout, *vertCopyModule, VK_NULL_HANDLE,
                                                   VK_NULL_HANDLE, VK_NULL_HANDLE, *fragCopyModule, *copyRenderPass,
                                                   viewports, scissors, topology, 0u, 0u, &copyInputStateInfo);

    const tcu::Vec4 clearColorVec(0.0f, 0.0f, 0.0f, 1.0f);
    const auto clearColor =
        makeClearValueColorF32(clearColorVec.x(), clearColorVec.y(), clearColorVec.z(), clearColorVec.w());

    // Transform to Vulkan offsets.
    std::vector<VkOffset2D> fdmVkOffsets;

    std::transform(begin(fdmOffsets), end(fdmOffsets), std::back_inserter(fdmVkOffsets),
                   [](const tcu::IVec2 &fdmOffset) { return makeOffset2D(fdmOffset.x(), fdmOffset.y()); });

    const VkRenderPassBeginInfo rpBeginInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr, *renderPass, *framebuffer, scissors.at(0u), 1u, &clearColor,
    };
    const VkSubpassBeginInfo subpassBeginInfo = {
        VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO,
        nullptr,
        (useSecondary ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE),
    };
    const VkRenderPassFragmentDensityMapOffsetEndInfoEXT fdmOffsetEndInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_OFFSET_END_INFO_EXT,
        nullptr,
        de::sizeU32(fdmVkOffsets),
        de::dataOrNull(fdmVkOffsets),
    };
    VkSubpassEndInfo subpassEndInfo = {
        VK_STRUCTURE_TYPE_SUBPASS_END_INFO,
        nullptr,
    };

    const VkRenderingFragmentDensityMapAttachmentInfoEXT renderingFDMAttachmentInfo = {
        VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_INFO_EXT,
        nullptr,
        *fdmView,
        VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT,
    };
    const VkRenderingAttachmentInfo colorAttachmentInfo = {
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        nullptr,
        *fbView,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_RESOLVE_MODE_NONE,
        VK_NULL_HANDLE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        clearColor,
    };
    const auto renderingInfoFlags =
        (needsIheritance ? static_cast<VkRenderingFlags>(VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT) : 0u);
    VkRenderingInfo renderingInfo = {
        VK_STRUCTURE_TYPE_RENDERING_INFO,
        &renderingFDMAttachmentInfo,
        renderingInfoFlags,
        scissors.at(0u),
        1u,
        viewMask,
        1u,
        &colorAttachmentInfo,
        nullptr,
        nullptr,
    };
    VkRenderingEndInfoEXT renderingEndInfo = {
        VK_STRUCTURE_TYPE_RENDERING_END_INFO_EXT,
        nullptr,
    };

    // Closures to record render pass begin, contents and end on a given command buffer.
    const auto recordBeginRenderPass = [&](VkCommandBuffer cmd, VkRenderingFlags renderingFlags)
    {
        if (isDynamicRendering)
        {
            if ((renderingFlags & VK_RENDERING_RESUMING_BIT) == 0)
            {
                // If we are resuming the render pass, we have already transitioned the layout.
                const auto colorAttAccess =
                    (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
                const auto preRenderingBarrier =
                    makeImageMemoryBarrier(0u, colorAttAccess, VK_IMAGE_LAYOUT_UNDEFINED,
                                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, *fbImage, colorSRR);
                cmdPipelineImageMemoryBarrier(vkd, cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, &preRenderingBarrier);
            }
            const auto prevFlags = renderingInfo.flags;
            renderingInfo.flags |= renderingFlags;
            vkd.cmdBeginRendering(cmd, &renderingInfo);
            renderingInfo.flags = prevFlags;
        }
        else
        {
            DE_ASSERT((renderingFlags & (VK_RENDERING_SUSPENDING_BIT | VK_RENDERING_RESUMING_BIT)) == 0);
            vkd.cmdBeginRenderPass2(cmd, &rpBeginInfo, &subpassBeginInfo);
        }
    };

    const auto recordRenderPassContents = [&](VkCommandBuffer cmd, QuadPiece quadPiece)
    {
        vkd.cmdBindPipeline(cmd, bindPoint, *graphicsPipeline);
        vkd.cmdBindVertexBuffers(cmd, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
        const uint32_t vertexCount =
            de::sizeU32(vertices) / ((quadPiece == QuadPiece::ALL) ? 1u : 2u); // Half if only one piece.
        const uint32_t firstVertex = ((quadPiece == QuadPiece::SECOND) ? (de::sizeU32(vertices) / 2u) : 0u);
        vkd.cmdDraw(cmd, vertexCount, 1u, firstVertex, 0u);
    };

    const auto recordEndRenderPass =
        [&](VkCommandBuffer cmd, const VkRenderPassFragmentDensityMapOffsetEndInfoEXT *fdmEnd)
    {
        if (isDynamicRendering)
        {
            if (fdmEnd == nullptr)
                vkd.cmdEndRendering(cmd);
            else
            {
                renderingEndInfo.pNext = fdmEnd;
                vkd.cmdEndRendering2KHR(cmd, &renderingEndInfo);
            }
        }
        else
        {
            subpassEndInfo.pNext = fdmEnd;
            vkd.cmdEndRenderPass2(cmd, &subpassEndInfo);
        }
    };

    TexLevelsVec results;

    for (const auto forceNoOffset : m_params->iterations)
    {
        // Main command buffer.
        CommandPoolWithBuffer cmd(vkd, device, qfIndex);
        const auto primary = *cmd.cmdBuffer;

        // Secondaries: we may need none, one or two, depending on the usage of secondaries and suspend/resume.
        std::vector<Move<VkCommandBuffer>> secondaries;

        if (useSecondary)
        {
            const uint32_t cmdBufferCount = (multipleSecondaries ? 2u : 1u);

            for (uint32_t i = 0u; i < cmdBufferCount; ++i)
            {
                VkRenderingFlags inhRenderingFlags = 0u;
                if (multipleSecondaries)
                    inhRenderingFlags |= ((i == 0) ? VK_RENDERING_SUSPENDING_BIT : VK_RENDERING_RESUMING_BIT);

                const VkCommandBufferInheritanceRenderingInfo inhRenderingInfo = {
                    VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO,
                    nullptr,
                    inhRenderingFlags,
                    viewMask,
                    1u,
                    &fbFormat,
                    VK_FORMAT_UNDEFINED,
                    VK_FORMAT_UNDEFINED,
                    fbImageInfo.samples,
                };
                const auto inhPNext = ((isDynamicRendering && needsIheritance) ? &inhRenderingInfo : nullptr);
                VkCommandBufferUsageFlags beginFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                if (needsIheritance)
                    beginFlags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;

                secondaries.emplace_back(
                    allocateCommandBuffer(vkd, device, *cmd.cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY));
                auto &secondary = secondaries.back();

                beginSecondaryCommandBuffer(vkd, *secondary, *renderPass, *framebuffer, beginFlags, inhPNext);
                if (allInSecondary)
                {
                    DE_ASSERT(!multipleSecondaries);
                    if (m_params->resumeRendering)
                    {
                        recordBeginRenderPass(*secondary, VK_RENDERING_SUSPENDING_BIT);
                        recordRenderPassContents(*secondary, QuadPiece::FIRST);
                        recordEndRenderPass(*secondary, (forceNoOffset ? nullptr : &fdmOffsetEndInfo));
                        recordBeginRenderPass(*secondary, VK_RENDERING_RESUMING_BIT);
                        recordRenderPassContents(*secondary, QuadPiece::SECOND);
                        recordEndRenderPass(*secondary, (forceNoOffset ? nullptr : &fdmOffsetEndInfo));
                    }
                    else
                    {
                        recordBeginRenderPass(*secondary, 0u);
                        recordRenderPassContents(*secondary, QuadPiece::ALL);
                        recordEndRenderPass(*secondary, (forceNoOffset ? nullptr : &fdmOffsetEndInfo));
                    }
                }
                else
                {
                    if (m_params->resumeRendering)
                    {
                        DE_ASSERT(multipleSecondaries);
                        recordRenderPassContents(*secondary, ((i == 0) ? QuadPiece::FIRST : QuadPiece::SECOND));
                    }
                    else
                        recordRenderPassContents(*secondary, QuadPiece::ALL);
                }
                endCommandBuffer(vkd, *secondary);
            }
        }

        beginCommandBuffer(vkd, primary);

        if (useSecondary)
        {
            if (allInSecondary)
            {
                DE_ASSERT(secondaries.size() == 1);
                vkd.cmdExecuteCommands(primary, 1u, &secondaries.front().get());
            }
            else
            {
                if (multipleSecondaries)
                {
                    DE_ASSERT(secondaries.size() == 2);

                    recordBeginRenderPass(primary, VK_RENDERING_SUSPENDING_BIT);
                    vkd.cmdExecuteCommands(primary, 1u, &secondaries.front().get());
                    recordEndRenderPass(primary, (forceNoOffset ? nullptr : &fdmOffsetEndInfo));
                    recordBeginRenderPass(primary, VK_RENDERING_RESUMING_BIT);
                    vkd.cmdExecuteCommands(primary, 1u, &secondaries.back().get());
                    recordEndRenderPass(primary, (forceNoOffset ? nullptr : &fdmOffsetEndInfo));
                }
                else
                {
                    DE_ASSERT(secondaries.size() == 1);
                    recordBeginRenderPass(primary, 0u);
                    vkd.cmdExecuteCommands(primary, 1u, &secondaries.front().get());
                    recordEndRenderPass(primary, (forceNoOffset ? nullptr : &fdmOffsetEndInfo));
                }
            }
        }
        else
        {
            if (m_params->resumeRendering)
            {
                recordBeginRenderPass(primary, VK_RENDERING_SUSPENDING_BIT);
                recordRenderPassContents(primary, QuadPiece::FIRST);
                recordEndRenderPass(primary, (forceNoOffset ? nullptr : &fdmOffsetEndInfo));
                recordBeginRenderPass(primary, VK_RENDERING_RESUMING_BIT);
                recordRenderPassContents(primary, QuadPiece::SECOND);
                recordEndRenderPass(primary, (forceNoOffset ? nullptr : &fdmOffsetEndInfo));
            }
            else
            {
                recordBeginRenderPass(primary, 0u);
                recordRenderPassContents(primary, QuadPiece::ALL);
                recordEndRenderPass(primary, (forceNoOffset ? nullptr : &fdmOffsetEndInfo));
            }
        }

        {
            // Sync color buffer writes with shader reads and change layout.
            const auto fbBarrier = makeImageMemoryBarrier(
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, *fbImage, colorSRR);
            cmdPipelineImageMemoryBarrier(vkd, primary, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, &fbBarrier);
        }
        beginRenderPass(vkd, primary, *copyRenderPass, *copyFramebuffer, scissors.at(0u));
        vkd.cmdBindPipeline(primary, bindPoint, *copyPipeline);
        vkd.cmdBindDescriptorSets(primary, bindPoint, *copyPipelineLayout, 0u, 1u, &copySet.get(), 0u, nullptr);
        vkd.cmdDraw(primary, 3u, 1u, 0u, 0u); // Single full-screen triangle. See vertex-copy shader.
        endRenderPass(vkd, primary);
        {
            // Sync texel buffer writes with host reads.
            const auto hostBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            cmdPipelineMemoryBarrier(vkd, primary, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                     &hostBarrier);
        }
        endCommandBuffer(vkd, primary);
        submitCommandsAndWait(vkd, device, queue, primary);

        invalidateAlloc(vkd, device, texelBuffer.getAllocation());
        tcu::ConstPixelBufferAccess result(fbTcuFormat, resultExtent, texelBuffer.getAllocation().getHostPtr());

        results.emplace_back(
            new tcu::TextureLevel(result.getFormat(), result.getWidth(), result.getHeight(), result.getDepth()));
        tcu::copy(results.back()->getAccess(), result);
    }

    TexLevelsVec references;
    prepareReferences(references, results, quadInfo, fdmOffsets);

    auto &log = m_context.getTestContext().getLog();
    checkResults(log, references, results, fdmOffsets);

    return tcu::TestStatus::pass("Pass");
}

FDMOffsetBaseInstance::QuadInfo FDMOffsetBaseInstance::getQuadInfo(const tcu::IVec3 &, const tcu::IVec3 &) const
{
    float xBegin = -1.0f;
    float xEnd   = 1.0f;
    float yBegin = -1.0f;
    float yEnd   = 1.0f;

    return QuadInfo{xBegin, xEnd, yBegin, yEnd};
}

struct FDMOffsetOversizedFDMParams : public FDMOffsetBaseParams
{
    bool extraLarge;

    FDMOffsetOversizedFDMParams(const SharedGroupParams groupParams, OffsetType horizontalOffset_,
                                OffsetType verticalOffset_, bool multiView_, bool resumeRendering_, bool extraLarge_)
        : FDMOffsetBaseParams(groupParams, horizontalOffset_, verticalOffset_, multiView_, resumeRendering_)
        , extraLarge(extraLarge_)
    {
    }

    // The FDM buffer will be twice as much in the interesting dimension (horizontally or vertically).
    int fdmSizeFactor(OffsetType offsetType) const
    {
        switch (offsetType)
        {
        case OffsetType::NONE:
            return 1;
        case OffsetType::POSITIVE: // fallthrough.
        case OffsetType::NEGATIVE:
            return (extraLarge ? 4 : 2);
        default:
            break;
        }

        DE_ASSERT(false);
        return std::numeric_limits<int>::max();
    }

    tcu::IVec3 getFragmentDensityMapExtent(
        const VkPhysicalDeviceFragmentDensityMapPropertiesEXT &properties) const override
    {
        const auto minTexelSize = getMinTexelSize(properties);
        const auto fbExtent     = getFramebufferExtent();
        const auto factor       = tcu::IVec3(fdmSizeFactor(horizontalOffset), fdmSizeFactor(verticalOffset), 1);
        return fbExtent / minTexelSize * factor;
    }

    // How many times the framebuffer extent to offset by.
    tcu::IVec3 getOffsetFactor() const
    {
        const auto baseFactor = (extraLarge ? 3 : 1);
        const auto factor = tcu::IVec3(getSign(horizontalOffset) * baseFactor, getSign(verticalOffset) * baseFactor, 1);
        return factor;
    }

    std::vector<tcu::IVec2> getOffsets(const VkPhysicalDeviceFragmentDensityMapOffsetPropertiesEXT *) const override
    {
        const auto fbExtent   = getFramebufferExtent();
        const auto factor     = getOffsetFactor();
        const auto realOffset = (fbExtent * factor).swizzle(0, 1);

        std::vector<tcu::IVec2> offsets;
        if (multiView)
            offsets.emplace_back(0, 0);
        offsets.push_back(realOffset);
        return offsets;
    }
};

class FDMOffsetOversizedFDMInstance : public FDMOffsetBaseInstance
{
public:
    FDMOffsetOversizedFDMInstance(Context &context, FDMOffsetParamsPtr params) : FDMOffsetBaseInstance(context, params)
    {
    }
    virtual ~FDMOffsetOversizedFDMInstance(void) = default;

protected:
    void prepareFDMAccess(tcu::PixelBufferAccess &fdmAccess, const std::vector<tcu::IVec2> &fdmOffsets) const override;
    void prepareReferences(TexLevelsVec &references, const TexLevelsVec &results, const QuadInfo &quadInfo,
                           const std::vector<tcu::IVec2> &fdmOffsets) const override;
    void checkResults(tcu::TestLog &log, const TexLevelsVec &references, const TexLevelsVec &results,
                      const std::vector<tcu::IVec2> &fdmOffsets) const override;
};

void FDMOffsetOversizedFDMInstance::prepareFDMAccess(tcu::PixelBufferAccess &fdmAccess,
                                                     const std::vector<tcu::IVec2> &fdmOffsets) const
{
    const tcu::IVec2 zeroOffset(0, 0);

    const auto fdmFormat = mapTextureFormat(fdmAccess.getFormat());
    const auto fdmExtent = fdmAccess.getSize();

    // 3 times the minimum to make sure we give the implementation ample room for choosing an area larger than 1.
    const auto highDensity = tcu::Vec2(1.0f, 1.0f);
    const auto lowDensity  = 3.0f * getFormatDelta(fdmFormat);

    const tcu::Vec4 highDensityColor(highDensity.x(), highDensity.y(), 0.0f, 0.0f);
    const tcu::Vec4 lowDensityColor(lowDensity.x(), lowDensity.y(), 0.0f, 0.0f);

    // If the offset is zero, we'll clear to 1x1 on the left/top side, and 2x2 (or larger) on the right/bottom side.
    // If the offset is nonzero, the values are reversed to make sure we sample from the right/bottom side.
    // If the offset type is negative, values are reversed.

    const auto osfdmParams = static_cast<const FDMOffsetOversizedFDMParams *>(m_params.get());
    DE_ASSERT(!!osfdmParams);
    const auto dimDivisor = (osfdmParams->extraLarge ? 4 : 2);

    for (int layer = 0; layer < fdmExtent.z(); ++layer)
    {
        const bool isZeroOffset = (fdmOffsets.at(layer) == zeroOffset);

        if (m_params->horizontalOffset != OffsetType::NONE)
        {
            const int sideWidth  = fdmExtent.x() / dimDivisor;
            const int sideHeight = fdmExtent.y();

            const auto left = tcu::getSubregion(fdmAccess, 0, 0, layer, fdmExtent.x() - sideWidth, sideHeight, 1);
            const auto right =
                tcu::getSubregion(fdmAccess, fdmExtent.x() - sideWidth, 0, layer, sideWidth, sideHeight, 1);

            const bool isNegative = (m_params->horizontalOffset == OffsetType::NEGATIVE);
            const bool leftLow    = (isNegative && !isZeroOffset);
            tcu::clear(left, (leftLow ? lowDensityColor : highDensityColor));
            tcu::clear(right, (leftLow ? highDensityColor : lowDensityColor));
        }
        else if (m_params->verticalOffset != OffsetType::NONE)
        {
            const int sideWidth  = fdmExtent.x();
            const int sideHeight = fdmExtent.y() / dimDivisor;

            const auto top = tcu::getSubregion(fdmAccess, 0, 0, layer, sideWidth, fdmExtent.y() - sideHeight, 1);
            const auto bottom =
                tcu::getSubregion(fdmAccess, 0, fdmExtent.y() - sideHeight, layer, sideWidth, sideHeight, 1);

            const bool isNegative = (m_params->verticalOffset == OffsetType::NEGATIVE);
            const bool topLow     = (isNegative && !isZeroOffset);
            tcu::clear(top, (topLow ? lowDensityColor : highDensityColor));
            tcu::clear(bottom, (topLow ? highDensityColor : lowDensityColor));
        }
        else
            DE_ASSERT(false);
    }
}

void FDMOffsetOversizedFDMInstance::prepareReferences(TexLevelsVec &references, const TexLevelsVec &results,
                                                      const QuadInfo &, const std::vector<tcu::IVec2> &) const
{
    DE_ASSERT(results.size() == 1);
    const auto &res         = *results.front();
    const auto resultFormat = res.getFormat();
    const auto resultExtent = res.getSize();

    references.clear();
    references.emplace_back(new tcu::TextureLevel(resultFormat, resultExtent.x(), resultExtent.y(), resultExtent.z()));
    tcu::clear(references.back()->getAccess(), m_params->getHighResColor());
}

void FDMOffsetOversizedFDMInstance::checkResults(tcu::TestLog &log, const TexLevelsVec &references,
                                                 const TexLevelsVec &results,
                                                 const std::vector<tcu::IVec2> &fdmOffsets) const
{
    const auto logPolicy = tcu::COMPARE_LOG_ON_ERROR;

    DE_ASSERT(results.size() == 1 && results.front() != nullptr);
    DE_ASSERT(references.size() == 1 && references.front() != nullptr);
    const auto result    = results.front()->getAccess();
    const auto reference = references.front()->getAccess();
    DE_ASSERT(result.getSize() == reference.getSize());

    bool ok = true;
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);
    const auto extent = reference.getSize();
    const tcu::IVec2 zeroOffset(0, 0);

    for (int layer = 0; layer < extent.z(); ++layer)
    {
        bool layerOK            = true;
        const bool isZeroOffset = (fdmOffsets.at(layer) == zeroOffset);
        const auto namePrefix   = "Layer" + std::to_string(layer) + "-";

        // We will only check half the image.
        if (m_params->horizontalOffset != OffsetType::NONE)
        {
            const int sideWidth  = extent.x() / 2;
            const int sideHeight = extent.y();

            const auto refLeft  = tcu::getSubregion(reference, 0, 0, layer, sideWidth, sideHeight, 1);
            const auto refRight = tcu::getSubregion(reference, sideWidth, 0, layer, sideWidth, sideHeight, 1);

            const auto resLeft  = tcu::getSubregion(result, 0, 0, layer, sideWidth, sideHeight, 1);
            const auto resRight = tcu::getSubregion(result, sideWidth, 0, layer, sideWidth, sideHeight, 1);

            if (m_params->horizontalOffset == OffsetType::NEGATIVE && !isZeroOffset)
            {
                const auto name = namePrefix + "RightSide";
                layerOK = tcu::floatThresholdCompare(log, name.c_str(), "", refRight, resRight, threshold, logPolicy);
            }
            else if (m_params->horizontalOffset == OffsetType::POSITIVE || isZeroOffset)
            {
                const auto name = namePrefix + "LeftSide";
                layerOK = tcu::floatThresholdCompare(log, name.c_str(), "", refLeft, resLeft, threshold, logPolicy);
            }
            else
                DE_ASSERT(false);
        }
        else if (m_params->verticalOffset != OffsetType::NONE)
        {
            const int sideWidth  = extent.x();
            const int sideHeight = extent.y() / 2;

            const auto refTop    = tcu::getSubregion(reference, 0, 0, layer, sideWidth, sideHeight, 1);
            const auto refBottom = tcu::getSubregion(reference, 0, sideHeight, layer, sideWidth, sideHeight, 1);

            const auto resTop    = tcu::getSubregion(result, 0, 0, layer, sideWidth, sideHeight, 1);
            const auto resBottom = tcu::getSubregion(result, 0, sideHeight, layer, sideWidth, sideHeight, 1);

            if (m_params->verticalOffset == OffsetType::NEGATIVE && !isZeroOffset)
            {
                const auto name = namePrefix + "BottomHalf";
                layerOK = tcu::floatThresholdCompare(log, name.c_str(), "", refBottom, resBottom, threshold, logPolicy);
            }
            else if (m_params->verticalOffset == OffsetType::POSITIVE || isZeroOffset)
            {
                const auto name = namePrefix + "TopHalf";
                layerOK = tcu::floatThresholdCompare(log, name.c_str(), "", refTop, resTop, threshold, logPolicy);
            }
            else
                DE_ASSERT(false);
        }
        else
            DE_ASSERT(false);

        if (!layerOK)
            ok = false;
    }

    if (!ok)
        TCU_FAIL("Unexpected result in color buffer; check log for details --");
}

class FDMOffsetOversizedFDMCase : public FDMOffsetBaseCase
{
public:
    FDMOffsetOversizedFDMCase(tcu::TestContext &testCtx, const std::string &name, FDMOffsetParamsPtr params)
        : FDMOffsetBaseCase(testCtx, name, params)
    {
    }
    virtual ~FDMOffsetOversizedFDMCase(void) = default;

    TestInstance *createInstance(Context &context) const override
    {
        return new FDMOffsetOversizedFDMInstance(context, m_params);
    }
};

struct FDMOffsetMinShiftParams : public FDMOffsetBaseParams
{
    FDMOffsetMinShiftParams(const SharedGroupParams groupParams, OffsetType horizontalOffset_,
                            OffsetType verticalOffset_, bool multiView_, bool resumeRendering_)
        : FDMOffsetBaseParams(groupParams, horizontalOffset_, verticalOffset_, multiView_, resumeRendering_)
    {
        // Two iterations in this case, with the first one not using offsets.
        iterations.at(0u) = true;
        iterations.emplace_back(false);
    }

    std::vector<tcu::IVec2> getOffsets(
        const VkPhysicalDeviceFragmentDensityMapOffsetPropertiesEXT *properties) const override
    {
        std::vector<tcu::IVec2> offsets;

        // Early return with no offsets. This is used in checkSupport because checkSupport makes sure the selected
        // offsets are a multiple of the granularity. However, in this case what we do is precisely to use offsets that
        // are multiples of the granularity at runtime, so we skip the support checks and build an offset vector that is
        // supported by design at runtime.
        if (!properties)
            return offsets;

        // Shift by the minimum amount by granularity.
        const tcu::UVec2 propertiesOffset(properties->fragmentDensityOffsetGranularity.width,
                                          properties->fragmentDensityOffsetGranularity.height);
        const tcu::IVec2 baseOffset = propertiesOffset.asInt();
        const tcu::IVec2 signs(getSign(horizontalOffset), getSign(verticalOffset));
        const tcu::IVec2 realOffset = baseOffset * signs;

        if (multiView)
            offsets.emplace_back(0, 0);
        offsets.push_back(realOffset);
        return offsets;
    }
};

class FDMOffsetMinShiftInstance : public FDMOffsetBaseInstance
{
public:
    FDMOffsetMinShiftInstance(Context &context, FDMOffsetParamsPtr params) : FDMOffsetBaseInstance(context, params)
    {
    }
    virtual ~FDMOffsetMinShiftInstance(void) = default;

protected:
    void prepareFDMAccess(tcu::PixelBufferAccess &fdmAccess, const std::vector<tcu::IVec2> &fdmOffsets) const override;
    void prepareReferences(TexLevelsVec &references, const TexLevelsVec &results, const QuadInfo &quadInfo,
                           const std::vector<tcu::IVec2> &fdmOffsets) const override;
    void checkResults(tcu::TestLog &log, const TexLevelsVec &references, const TexLevelsVec &results,
                      const std::vector<tcu::IVec2> &fdmOffsets) const override;
};

void FDMOffsetMinShiftInstance::prepareFDMAccess(tcu::PixelBufferAccess &fdmAccess,
                                                 const std::vector<tcu::IVec2> &) const
{
    const auto fdmFormat = mapTextureFormat(fdmAccess.getFormat());
    const auto fdmExtent = fdmAccess.getSize();

    // 3 times the minimum to make sure we give the implementation ample room for choosing an area larger than 1.
    const auto highDensity = tcu::Vec2(1.0f, 1.0f);
    const auto lowDensity  = 3.0f * getFormatDelta(fdmFormat);

    const tcu::Vec4 highDensityColor(highDensity.x(), highDensity.y(), 0.0f, 0.0f);
    const tcu::Vec4 lowDensityColor(lowDensity.x(), lowDensity.y(), 0.0f, 0.0f);

    // All layers in the FDM attachment will have the same contents: left/right or top/bottom split, with left/top low
    // density and right/bottom high density. For negative offsets, the density values change.
    for (int layer = 0; layer < fdmExtent.z(); ++layer)
    {
        if (m_params->horizontalOffset != OffsetType::NONE)
        {
            const bool isNegative = (m_params->horizontalOffset == OffsetType::NEGATIVE);

            const int sideWidth  = fdmExtent.x() / 2;
            const int sideHeight = fdmExtent.y();

            const auto left  = tcu::getSubregion(fdmAccess, 0, 0, layer, sideWidth, sideHeight, 1);
            const auto right = tcu::getSubregion(fdmAccess, sideWidth, 0, layer, sideWidth, sideHeight, 1);

            tcu::clear(left, (isNegative ? lowDensityColor : highDensityColor));
            tcu::clear(right, (isNegative ? highDensityColor : lowDensityColor));
        }
        else if (m_params->verticalOffset != OffsetType::NONE)
        {
            const bool isNegative = (m_params->verticalOffset == OffsetType::NEGATIVE);

            const int sideWidth  = fdmExtent.x();
            const int sideHeight = fdmExtent.y() / 2;

            const auto top    = tcu::getSubregion(fdmAccess, 0, 0, layer, sideWidth, sideHeight, 1);
            const auto bottom = tcu::getSubregion(fdmAccess, 0, sideHeight, layer, sideWidth, sideHeight, 1);

            tcu::clear(top, (isNegative ? lowDensityColor : highDensityColor));
            tcu::clear(bottom, (isNegative ? highDensityColor : lowDensityColor));
        }
        else
            DE_ASSERT(false);
    }
}

void FDMOffsetMinShiftInstance::prepareReferences(TexLevelsVec &references, const TexLevelsVec &results,
                                                  const QuadInfo &, const std::vector<tcu::IVec2> &fdmOffsets) const
{
    // In this case, we will compare results of different iterations among themselves, so the refernces vector is empty.
    references.clear();

    // To avoid a quality warning, the implementation should shift the framebuffer exactly by the specified offset in
    // the second iteration.
    DE_ASSERT(results.size() == m_params->iterations.size());
    DE_ASSERT(results.size() == 2);
    const auto &firstResult = *results.front();
    const auto extent       = firstResult.getSize();

    // The first reference image is not used, but we'll create a copy of the result for the first iteration.
    {
        references.emplace_back(new tcu::TextureLevel(firstResult.getFormat(), extent.x(), extent.y(), extent.z()));
        tcu::copy(references.back()->getAccess(), firstResult.getAccess());
    }

    // The second reference will be a shift of the first result by the exact amount of pixels in the offsets.
    {
        DE_ASSERT(static_cast<size_t>(extent.z()) == fdmOffsets.size());
        const tcu::IVec2 noOffset(0, 0);
        const auto hdColor = m_params->getHighResColor();

        references.emplace_back(new tcu::TextureLevel(firstResult.getFormat(), extent.x(), extent.y(), extent.z()));
        auto ref = references.back()->getAccess();

        for (int z = 0; z < extent.z(); ++z)
        {
            const auto &fdmOffset = fdmOffsets.at(z);

            // Copy layer without changes.
            auto dstLayer = tcu::getSubregion(ref, 0, 0, z, extent.x(), extent.y(), 1);
            auto srcLayer = tcu::getSubregion(firstResult.getAccess(), 0, 0, z, extent.x(), extent.y(), 1);
            tcu::copy(dstLayer, srcLayer);

            if (fdmOffset == noOffset)
                continue;

            DE_ASSERT(fdmOffset.x() == 0 || fdmOffset.y() == 0);

            int firstHD;
            int areaStart;
            int areaEnd;

            if (fdmOffset.x() != 0)
            {
                if (fdmOffset.x() < 0)
                {
                    for (firstHD = 0; firstHD < extent.x(); ++firstHD)
                    {
                        const auto color = dstLayer.getPixel(firstHD, 0);
                        if (color == hdColor)
                            break;
                    }

                    areaEnd   = firstHD;
                    areaStart = de::clamp(areaEnd + fdmOffset.x(), 0, extent.x() - 1);

                    if (areaStart != areaEnd)
                    {
                        const auto region =
                            tcu::getSubregion(dstLayer, areaStart, 0, (areaEnd - areaStart), extent.y());
                        tcu::clear(region, hdColor);
                    }
                }
                else
                {
                    for (firstHD = extent.x() - 1; firstHD >= 0; --firstHD)
                    {
                        const auto color = dstLayer.getPixel(firstHD, 0);
                        if (color == hdColor)
                            break;
                    }

                    areaStart = de::clamp(firstHD + 1, 0, extent.x() - 1);
                    areaEnd   = de::clamp(areaStart + fdmOffset.x(), 0, extent.x() - 1);

                    if (areaStart != areaEnd)
                    {
                        const auto region =
                            tcu::getSubregion(dstLayer, areaStart, 0, (areaEnd - areaStart), extent.y());
                        tcu::clear(region, hdColor);
                    }
                }
            }
            else
            {
                if (fdmOffset.y() < 0)
                {
                    for (firstHD = 0; firstHD < extent.y(); ++firstHD)
                    {
                        const auto color = dstLayer.getPixel(0, firstHD);
                        if (color == hdColor)
                            break;
                    }

                    areaEnd   = firstHD;
                    areaStart = de::clamp(areaEnd + fdmOffset.y(), 0, extent.y() - 1);

                    if (areaStart != areaEnd)
                    {
                        const auto region =
                            tcu::getSubregion(dstLayer, 0, areaStart, extent.x(), (areaEnd - areaStart));
                        tcu::clear(region, hdColor);
                    }
                }
                else
                {
                    for (firstHD = extent.y() - 1; firstHD >= 0; --firstHD)
                    {
                        const auto color = dstLayer.getPixel(0, firstHD);
                        if (color == hdColor)
                            break;
                    }

                    areaStart = de::clamp(firstHD + 1, 0, extent.y() - 1);
                    areaEnd   = de::clamp(areaStart + fdmOffset.y(), 0, extent.y() - 1);

                    if (areaStart != areaEnd)
                    {
                        const auto region =
                            tcu::getSubregion(dstLayer, 0, areaStart, extent.x(), (areaEnd - areaStart));
                        tcu::clear(region, hdColor);
                    }
                }
            }
        }
    }
}

void FDMOffsetMinShiftInstance::checkResults(tcu::TestLog &log, const TexLevelsVec &references,
                                             const TexLevelsVec &results,
                                             const std::vector<tcu::IVec2> &fdmOffsets) const
{
    const auto logPolicy = tcu::COMPARE_LOG_ON_ERROR;

    DE_ASSERT(results.size() == references.size());
    DE_ASSERT(results.size() == m_params->iterations.size());
    DE_ASSERT(results.size() == 2);

    const auto &firstResult     = results.front()->getAccess();
    const auto &secondResult    = results.back()->getAccess();
    const auto &secondReference = references.back()->getAccess();

    DE_ASSERT(firstResult.getSize() == secondResult.getSize());
    DE_ASSERT(firstResult.getSize() == secondReference.getSize());

    const auto extent = firstResult.getSize();

    const tcu::TextureFormat errorFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8);
    tcu::TextureLevel errorLevel(errorFormat, extent.x(), extent.y(), 1);
    auto errorMask = errorLevel.getAccess();

    const auto red        = tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
    const auto green      = tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);
    const auto hdColor    = m_params->getHighResColor();
    const auto zeroOffset = tcu::IVec2(0, 0);
    const auto threshold  = tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);

    bool ok             = true;
    bool qualityWarning = false;

    for (int layer = 0; layer < extent.z(); ++layer)
    {
        const auto setName = "Layer" + std::to_string(layer);

        // Try to check exact match first.
        bool exactMatch = false;
        {
            const auto resLayer = tcu::getSubregion(secondResult, 0, 0, layer, extent.x(), extent.y(), 1);
            const auto refLayer = tcu::getSubregion(secondReference, 0, 0, layer, extent.x(), extent.y(), 1);

            const auto loggedName = setName + "-ExactMatch";
            if (tcu::floatThresholdCompare(log, loggedName.c_str(), "", refLayer, resLayer, threshold, logPolicy))
                exactMatch = true;
        }
        if (exactMatch)
            continue;

        bool layerOK = true;
        tcu::clear(errorMask, green);

        // If we don't have an exact match, we'll return a quality warning.
        qualityWarning = true;

        const bool isZeroOffset = (fdmOffsets.at(layer) == zeroOffset);

        // Check all pixels with high density in the first iteration continue to have high density in the second one.
        for (int y = 0; y < extent.y(); ++y)
            for (int x = 0; x < extent.x(); ++x)
            {
                const auto color = firstResult.getPixel(x, y, layer);
                const auto other = secondResult.getPixel(x, y, layer);

                // For layers with zero offset, verify the first and second pass matches.
                // For layers with non-zero offset, verify all pixels with high density continue to have it.
                if ((isZeroOffset && color != other) || (!isZeroOffset && color == hdColor && other != hdColor))
                {
                    errorMask.setPixel(red, x, y);
                    layerOK = false;
                }
            }

        if (!layerOK)
            ok = false;

        if (!layerOK || logPolicy == tcu::COMPARE_LOG_EVERYTHING)
        {
            const auto layerFirst =
                tcu::getSubregion(firstResult, 0, 0, layer, firstResult.getWidth(), firstResult.getHeight(), 1);
            const auto layerSecond =
                tcu::getSubregion(secondResult, 0, 0, layer, secondResult.getWidth(), secondResult.getHeight(), 1);

            log << tcu::TestLog::ImageSet(setName, "") << tcu::TestLog::Image("FirstResult", setName, layerFirst)
                << tcu::TestLog::Image("SecondResult", setName, layerSecond)
                << tcu::TestLog::Image("ErrorMask", setName, errorMask) << tcu::TestLog::EndImageSet;
        }
    }

    if (!ok)
        TCU_FAIL("Unexpected result in color buffer; check log for details --");

    if (qualityWarning)
        TCU_THROW(QualityWarning, "Offset not applied exactly; check log for details --");
}

class FDMOffsetMinShiftCase : public FDMOffsetBaseCase
{
public:
    FDMOffsetMinShiftCase(tcu::TestContext &testCtx, const std::string &name, FDMOffsetParamsPtr params)
        : FDMOffsetBaseCase(testCtx, name, params)
    {
    }
    virtual ~FDMOffsetMinShiftCase(void) = default;

    TestInstance *createInstance(Context &context) const override
    {
        return new FDMOffsetMinShiftInstance(context, m_params);
    }
};

struct FDMOffsetClampToEdgeParams : public FDMOffsetBaseParams
{
    FDMOffsetClampToEdgeParams(const SharedGroupParams groupParams, OffsetType horizontalOffset_,
                               OffsetType verticalOffset_, bool multiView_, bool resumeRendering_)
        : FDMOffsetBaseParams(groupParams, horizontalOffset_, verticalOffset_, multiView_, resumeRendering_)
    {
    }

    virtual ~FDMOffsetClampToEdgeParams() = default;

    std::vector<tcu::IVec2> getOffsets(const VkPhysicalDeviceFragmentDensityMapOffsetPropertiesEXT *) const override
    {
        const auto fbExtent   = getFramebufferExtent();
        const auto factor     = tcu::IVec3(getSign(horizontalOffset), getSign(verticalOffset), 1);
        const auto realOffset = (fbExtent * factor).swizzle(0, 1);

        std::vector<tcu::IVec2> offsets;
        if (multiView)
            offsets.emplace_back(0, 0);
        offsets.push_back(realOffset);
        return offsets;
    }
};

class FDMOffsetClampToEdgeInstance : public FDMOffsetBaseInstance
{
public:
    FDMOffsetClampToEdgeInstance(Context &context, FDMOffsetParamsPtr params) : FDMOffsetBaseInstance(context, params)
    {
    }
    virtual ~FDMOffsetClampToEdgeInstance() = default;

protected:
    void prepareFDMAccess(tcu::PixelBufferAccess &fdmAccess, const std::vector<tcu::IVec2> &fdmOffsets) const override;
    void prepareReferences(TexLevelsVec &references, const TexLevelsVec &results, const QuadInfo &quadInfo,
                           const std::vector<tcu::IVec2> &fdmOffsets) const override;
    void checkResults(tcu::TestLog &log, const TexLevelsVec &references, const TexLevelsVec &results,
                      const std::vector<tcu::IVec2> &fdmOffsets) const override;
};

void FDMOffsetClampToEdgeInstance::prepareFDMAccess(tcu::PixelBufferAccess &fdmAccess,
                                                    const std::vector<tcu::IVec2> &fdmOffsets) const
{
    // Layers with zero offset will have the full FDM filled with high density, while the layers with nonzero offsets
    // will have one of the edges filled with high density values and the rest with low density values.
    const tcu::IVec2 zeroOffset(0, 0);

    const auto fdmFormat = mapTextureFormat(fdmAccess.getFormat());
    const auto fdmExtent = fdmAccess.getSize();

    // 3 times the minimum to make sure we give the implementation ample room for choosing an area larger than 1.
    const auto highDensity = tcu::Vec2(1.0f, 1.0f);
    const auto lowDensity  = 3.0f * getFormatDelta(fdmFormat);

    const tcu::Vec4 highDensityColor(highDensity.x(), highDensity.y(), 0.0f, 0.0f);
    const tcu::Vec4 lowDensityColor(lowDensity.x(), lowDensity.y(), 0.0f, 0.0f);

    for (int layer = 0; layer < fdmExtent.z(); ++layer)
    {
        const auto fdmOffset    = fdmOffsets.at(layer);
        const bool isZeroOffset = (fdmOffset == zeroOffset);
        const auto layerAccess  = tcu::getSubregion(fdmAccess, 0, 0, layer, fdmExtent.x(), fdmExtent.y(), 1);

        if (isZeroOffset)
        {
            tcu::clear(layerAccess, highDensityColor);
        }
        else
        {
            tcu::clear(layerAccess, lowDensityColor);
            if (fdmOffset.x() < 0)
            {
                const auto border = tcu::getSubregion(fdmAccess, fdmExtent.x() - 1, 0, layer, 1, fdmExtent.y(), 1);
                tcu::clear(border, highDensityColor);
            }
            else if (fdmOffset.x() > 0)
            {
                const auto border = tcu::getSubregion(fdmAccess, 0, 0, layer, 1, fdmExtent.y(), 1);
                tcu::clear(border, highDensityColor);
            }
            else if (fdmOffset.y() < 0)
            {
                const auto border = tcu::getSubregion(fdmAccess, 0, fdmExtent.y() - 1, layer, fdmExtent.x(), 1, 1);
                tcu::clear(border, highDensityColor);
            }
            else if (fdmOffset.y() > 0)
            {
                const auto border = tcu::getSubregion(fdmAccess, 0, 0, layer, fdmExtent.x(), 1, 1);
                tcu::clear(border, highDensityColor);
            }
        }
    }
}

void FDMOffsetClampToEdgeInstance::prepareReferences(TexLevelsVec &references, const TexLevelsVec &results,
                                                     const QuadInfo &, const std::vector<tcu::IVec2> &) const
{
    DE_ASSERT(results.size() == 1);
    const auto firstResult = *results.front();

    references.clear();
    references.emplace_back(new tcu::TextureLevel(firstResult.getFormat(), firstResult.getWidth(),
                                                  firstResult.getHeight(), firstResult.getDepth()));

    tcu::clear(references.back()->getAccess(), m_params->getHighResColor());
}

void FDMOffsetClampToEdgeInstance::checkResults(tcu::TestLog &log, const TexLevelsVec &references,
                                                const TexLevelsVec &results,
                                                const std::vector<tcu::IVec2> &fdmOffsets) const
{
    const auto logPolicy = tcu::COMPARE_LOG_ON_ERROR;

    DE_ASSERT(results.size() == 1 && results.front() != nullptr);
    DE_ASSERT(references.size() == 1 && references.front() != nullptr);
    const auto result    = results.front()->getAccess();
    const auto reference = references.front()->getAccess();
    DE_ASSERT(result.getSize() == reference.getSize());

    bool ok = true;
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);
    const auto extent = reference.getSize();
    const tcu::IVec2 zeroOffset(0, 0);

    for (int layer = 0; layer < extent.z(); ++layer)
    {
        bool layerOK            = true;
        const bool isZeroOffset = (fdmOffsets.at(layer) == zeroOffset);
        const auto namePrefix   = "Layer" + std::to_string(layer) + "-";

        // We will only check half the image.
        if (m_params->horizontalOffset != OffsetType::NONE)
        {
            const int sideWidth  = extent.x() / 2;
            const int sideHeight = extent.y();

            const auto refLeft  = tcu::getSubregion(reference, 0, 0, layer, sideWidth, sideHeight, 1);
            const auto refRight = tcu::getSubregion(reference, sideWidth, 0, layer, sideWidth, sideHeight, 1);

            const auto resLeft  = tcu::getSubregion(result, 0, 0, layer, sideWidth, sideHeight, 1);
            const auto resRight = tcu::getSubregion(result, sideWidth, 0, layer, sideWidth, sideHeight, 1);

            if (m_params->horizontalOffset == OffsetType::NEGATIVE && !isZeroOffset)
            {
                const auto name = namePrefix + "RightSide";
                layerOK = tcu::floatThresholdCompare(log, name.c_str(), "", refRight, resRight, threshold, logPolicy);
            }
            else if (m_params->horizontalOffset == OffsetType::POSITIVE || isZeroOffset)
            {
                const auto name = namePrefix + "LeftSide";
                layerOK = tcu::floatThresholdCompare(log, name.c_str(), "", refLeft, resLeft, threshold, logPolicy);
            }
            else
                DE_ASSERT(false);
        }
        else if (m_params->verticalOffset != OffsetType::NONE)
        {
            const int sideWidth  = extent.x();
            const int sideHeight = extent.y() / 2;

            const auto refTop    = tcu::getSubregion(reference, 0, 0, layer, sideWidth, sideHeight, 1);
            const auto refBottom = tcu::getSubregion(reference, 0, sideHeight, layer, sideWidth, sideHeight, 1);

            const auto resTop    = tcu::getSubregion(result, 0, 0, layer, sideWidth, sideHeight, 1);
            const auto resBottom = tcu::getSubregion(result, 0, sideHeight, layer, sideWidth, sideHeight, 1);

            if (m_params->verticalOffset == OffsetType::NEGATIVE && !isZeroOffset)
            {
                const auto name = namePrefix + "BottomHalf";
                layerOK = tcu::floatThresholdCompare(log, name.c_str(), "", refBottom, resBottom, threshold, logPolicy);
            }
            else if (m_params->verticalOffset == OffsetType::POSITIVE || isZeroOffset)
            {
                const auto name = namePrefix + "TopHalf";
                layerOK = tcu::floatThresholdCompare(log, name.c_str(), "", refTop, resTop, threshold, logPolicy);
            }
            else
                DE_ASSERT(false);
        }
        else
            DE_ASSERT(false);

        if (!layerOK)
            ok = false;
    }

    if (!ok)
        TCU_FAIL("Unexpected result in color buffer; check log for details --");
}

class FDMOffsetClampToEdgeCase : public FDMOffsetBaseCase
{
public:
    FDMOffsetClampToEdgeCase(tcu::TestContext &testCtx, const std::string &name, FDMOffsetParamsPtr params)
        : FDMOffsetBaseCase(testCtx, name, params)
    {
    }
    virtual ~FDMOffsetClampToEdgeCase(void) = default;

    TestInstance *createInstance(Context &context) const override
    {
        return new FDMOffsetClampToEdgeInstance(context, m_params);
    }
};

} // namespace

static void createChildren(tcu::TestCaseGroup *fdmTests, const SharedGroupParams groupParams)
{
    tcu::TestContext &testCtx = fdmTests->getTestContext();

    const struct
    {
        std::string name;
        uint32_t viewCount;
    } views[] = {
        {"1_view", 1},
        {"2_views", 2},
        {"4_views", 4},
        {"6_views", 6},
    };

    const struct
    {
        std::string name;
        bool makeCopy;
    } renders[] = {{"render", false}, {"render_copy", true}};

    const struct
    {
        std::string name;
        float renderSizeToDensitySize;
    } sizes[] = {{"divisible_density_size", 4.0f}, {"non_divisible_density_size", 3.75f}};

    const struct
    {
        std::string name;
        VkSampleCountFlagBits samples;
    } samples[] = {{"1_sample", VK_SAMPLE_COUNT_1_BIT},
                   {"2_samples", VK_SAMPLE_COUNT_2_BIT},
                   {"4_samples", VK_SAMPLE_COUNT_4_BIT},
                   {"8_samples", VK_SAMPLE_COUNT_8_BIT}};

    std::vector<tcu::UVec2> fragmentArea{{1, 2}, {2, 1}, {2, 2}};

    for (const auto &view : views)
    {
        if ((groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY) && view.viewCount > 1)
            continue;

        // Reduce number of tests for secondary command buffers in dynamic rendering to 1 and 2 views
        if (groupParams->useSecondaryCmdBuffer && (view.viewCount > 2))
            continue;

        de::MovePtr<tcu::TestCaseGroup> viewGroup(new tcu::TestCaseGroup(testCtx, view.name.c_str()));
        for (const auto &render : renders)
        {
            if ((groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING) && render.makeCopy &&
                groupParams->useSecondaryCmdBuffer &&
                (groupParams->secondaryCmdBufferCompletelyContainsDynamicRenderpass == false))
                continue;

            de::MovePtr<tcu::TestCaseGroup> renderGroup(new tcu::TestCaseGroup(testCtx, render.name.c_str()));
            for (const auto &size : sizes)
            {
                de::MovePtr<tcu::TestCaseGroup> sizeGroup(new tcu::TestCaseGroup(testCtx, size.name.c_str()));
                for (const auto &sample : samples)
                {
                    // Reduce number of tests for dynamic rendering cases where secondary command buffer is used
                    if (groupParams->useSecondaryCmdBuffer && (sample.samples > VK_SAMPLE_COUNT_2_BIT))
                        break;

                    de::MovePtr<tcu::TestCaseGroup> sampleGroup(new tcu::TestCaseGroup(testCtx, sample.name.c_str()));
                    for (const auto &area : fragmentArea)
                    {
                        for (const auto addZeroOffset : {false, true})
                        {
                            if (addZeroOffset && view.viewCount > 2)
                                continue;

                            if (addZeroOffset && sample.samples != VK_SAMPLE_COUNT_1_BIT &&
                                sample.samples != VK_SAMPLE_COUNT_4_BIT)
                                continue;

                            if (addZeroOffset && area != tcu::UVec2(2u, 2u))
                                continue;

                            if (addZeroOffset && groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
                                continue;

                            std::stringstream str;
                            str << "_" << area.x() << "_" << area.y();

                            if (addZeroOffset)
                                str << "_zero_offset";

                            TestParams params{
                                false,                        // bool dynamicDensityMap;
                                false,                        // bool deferredDensityMap;
                                false,                        // bool nonSubsampledImages;
                                false,                        // bool subsampledLoads;
                                false,                        // bool coarseReconstruction;
                                false,                        // bool imagelessFramebuffer;
                                false,                        // bool useMemoryAccess;
                                false,                        // bool useMaintenance5;
                                1,                            // uint32_t samplersCount;
                                view.viewCount,               // uint32_t viewCount;
                                false,                        // bool multiViewport;
                                render.makeCopy,              // bool makeCopy;
                                false,                        // bool depthEnabled;
                                addZeroOffset,                // bool addZeroOffset;
                                size.renderSizeToDensitySize, // float renderMultiplier;
                                sample.samples,               // VkSampleCountFlagBits colorSamples;
                                area,                         // tcu::UVec2 fragmentArea;
                                {16, 16},                     // tcu::UVec2 densityMapSize;
                                VK_FORMAT_R8G8_UNORM,         // VkFormat densityMapFormat;
                                VK_FORMAT_D16_UNORM,          // VkFormat depthFormat;
                                groupParams                   // SharedGroupParams groupParams;
                            };

                            sampleGroup->addChild(new FragmentDensityMapTest(
                                testCtx, std::string("static_subsampled") + str.str(), params));
                            params.deferredDensityMap = true;
                            sampleGroup->addChild(new FragmentDensityMapTest(
                                testCtx, std::string("deferred_subsampled") + str.str(), params));
                            params.deferredDensityMap = false;
                            params.dynamicDensityMap  = true;
                            sampleGroup->addChild(new FragmentDensityMapTest(
                                testCtx, std::string("dynamic_subsampled") + str.str(), params));

                            // generate nonsubsampled tests just for single view and double view cases
                            if (view.viewCount < 3)
                            {
                                params.nonSubsampledImages = true;
                                params.dynamicDensityMap   = false;
                                sampleGroup->addChild(new FragmentDensityMapTest(
                                    testCtx, std::string("static_nonsubsampled") + str.str(), params));
                                params.deferredDensityMap = true;
                                sampleGroup->addChild(new FragmentDensityMapTest(
                                    testCtx, std::string("deferred_nonsubsampled") + str.str(), params));
                                params.deferredDensityMap = false;
                                params.dynamicDensityMap  = true;
                                sampleGroup->addChild(new FragmentDensityMapTest(
                                    testCtx, std::string("dynamic_nonsubsampled") + str.str(), params));
                            }

                            // test multiviewport - each of views uses different viewport; limit number of cases to 2 samples
                            if ((groupParams->renderingType == RENDERING_TYPE_RENDERPASS2) && (!render.makeCopy) &&
                                (view.viewCount > 1) && (sample.samples == VK_SAMPLE_COUNT_2_BIT) && !addZeroOffset)
                            {
                                params.nonSubsampledImages = false;
                                params.dynamicDensityMap   = false;
                                params.deferredDensityMap  = false;
                                params.multiViewport       = true;
                                sampleGroup->addChild(new FragmentDensityMapTest(
                                    testCtx, std::string("static_subsampled") + str.str() + "_multiviewport", params));
                            }
                        }
                    }
                    sizeGroup->addChild(sampleGroup.release());
                }
                renderGroup->addChild(sizeGroup.release());
            }
            viewGroup->addChild(renderGroup.release());
        }
        fdmTests->addChild(viewGroup.release());
    }

    if (groupParams->renderingType == RENDERING_TYPE_RENDERPASS_LEGACY)
    {
        const struct
        {
            std::string name;
            VkFormat format;
        } depthFormats[] = {{"d16_unorm", VK_FORMAT_D16_UNORM},
                            {"d32_sfloat", VK_FORMAT_D32_SFLOAT},
                            {"d24_unorm_s8_uint", VK_FORMAT_D24_UNORM_S8_UINT}};

        de::MovePtr<tcu::TestCaseGroup> depthFormatGroup(new tcu::TestCaseGroup(testCtx, "depth_format"));
        for (const auto &format : depthFormats)
        {
            TestParams params{
                false,                 // bool dynamicDensityMap;
                true,                  // bool deferredDensityMap;
                false,                 // bool nonSubsampledImages;
                false,                 // bool subsampledLoads;
                false,                 // bool coarseReconstruction;
                false,                 // bool imagelessFramebuffer;
                false,                 // bool useMemoryAccess;
                false,                 // bool useMaintenance5;
                1,                     // uint32_t samplersCount;
                1,                     // uint32_t viewCount;
                false,                 // bool multiViewport;
                false,                 // bool makeCopy;
                true,                  // bool depthEnabled;
                false,                 // bool addZeroOffset;
                4.0f,                  // float renderMultiplier;
                VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits colorSamples;
                {2, 2},                // tcu::UVec2 fragmentArea;
                {16, 16},              // tcu::UVec2 densityMapSize;
                VK_FORMAT_R8G8_UNORM,  // VkFormat densityMapFormat;
                format.format,         // VkFormat depthFormat;
                groupParams            // SharedGroupParams groupParams;
            };
            depthFormatGroup->addChild(new FragmentDensityMapTest(testCtx, format.name, params));

            if (groupParams->useSecondaryCmdBuffer)
                break;
        }
        fdmTests->addChild(depthFormatGroup.release());
    }

    const struct
    {
        std::string name;
        uint32_t count;
    } subsampledSamplers[] = {{"2_subsampled_samplers", 2},
                              {"4_subsampled_samplers", 4},
                              {"6_subsampled_samplers", 6},
                              {"8_subsampled_samplers", 8}};

    de::MovePtr<tcu::TestCaseGroup> propertiesGroup(new tcu::TestCaseGroup(testCtx, "properties"));
    for (const auto &sampler : subsampledSamplers)
    {
        TestParams params{
            false,                 // bool dynamicDensityMap;
            false,                 // bool deferredDensityMap;
            false,                 // bool nonSubsampledImages;
            false,                 // bool subsampledLoads;
            false,                 // bool coarseReconstruction;
            false,                 // bool imagelessFramebuffer;
            false,                 // bool useMemoryAccess;
            false,                 // bool useMaintenance5;
            sampler.count,         // uint32_t samplersCount;
            1,                     // uint32_t viewCount;
            false,                 // bool multiViewport;
            false,                 // bool makeCopy;
            false,                 // bool depthEnabled;
            false,                 // bool addZeroOffset;
            4.0f,                  // float renderMultiplier;
            VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits colorSamples;
            {2, 2},                // tcu::UVec2 fragmentArea;
            {16, 16},              // tcu::UVec2 densityMapSize;
            VK_FORMAT_R8G8_UNORM,  // VkFormat densityMapFormat;
            VK_FORMAT_D16_UNORM,   // VkFormat depthFormat;
            groupParams            // SharedGroupParams groupParams;
        };
        propertiesGroup->addChild(new FragmentDensityMapTest(testCtx, sampler.name, params));

        // Reduce number of tests for dynamic rendering cases where secondary command buffer is used
        if (groupParams->useSecondaryCmdBuffer)
            break;
    }

    if ((groupParams->renderingType == RENDERING_TYPE_DYNAMIC_RENDERING) &&
        (groupParams->useSecondaryCmdBuffer == false))
    {
        TestParams params{
            false,                 // bool dynamicDensityMap;
            false,                 // bool deferredDensityMap;
            false,                 // bool nonSubsampledImages;
            false,                 // bool subsampledLoads;
            false,                 // bool coarseReconstruction;
            false,                 // bool imagelessFramebuffer;
            false,                 // bool useMemoryAccess;
            true,                  // bool useMaintenance5;
            1,                     // uint32_t samplersCount;
            1,                     // uint32_t viewCount;
            false,                 // bool multiViewport;
            false,                 // bool makeCopy;
            false,                 // bool depthEnabled;
            false,                 // bool addZeroOffset;
            4.0f,                  // float renderMultiplier;
            VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits colorSamples;
            {2, 2},                // tcu::UVec2 fragmentArea;
            {16, 16},              // tcu::UVec2 densityMapSize;
            VK_FORMAT_R8G8_UNORM,  // VkFormat densityMapFormat;
            VK_FORMAT_D16_UNORM,   // VkFormat depthFormat;
            groupParams            // SharedGroupParams groupParams;
        };
        propertiesGroup->addChild(new FragmentDensityMapTest(testCtx, "maintenance5", params));
    }

    if (groupParams->renderingType != RENDERING_TYPE_DYNAMIC_RENDERING)
    {
        // interaction between fragment density map and imageless framebuffer

        const struct
        {
            std::string name;
            bool useSecondaryCmdBuffer;
        } commandBufferType[] = {{"", false}, {"secondary_cmd_buff_", true}};

        for (const auto &cmdBuffType : commandBufferType)
        {
            TestParams params{
                false,                            // bool dynamicDensityMap;
                false,                            // bool deferredDensityMap;
                false,                            // bool nonSubsampledImages;
                false,                            // bool subsampledLoads;
                false,                            // bool coarseReconstruction;
                true,                             // bool imagelessFramebuffer;
                false,                            // bool useMemoryAccess;
                false,                            // bool useMaintenance5;
                1,                                // uint32_t samplersCount;
                1,                                // uint32_t viewCount;
                false,                            // bool multiViewport;
                false,                            // bool makeCopy;
                false,                            // bool depthEnabled;
                false,                            // bool addZeroOffset;
                4.0f,                             // float renderMultiplier;
                VK_SAMPLE_COUNT_1_BIT,            // VkSampleCountFlagBits colorSamples;
                {2, 2},                           // tcu::UVec2 fragmentArea;
                {16, 16},                         // tcu::UVec2 densityMapSize;
                VK_FORMAT_R8G8_UNORM,             // VkFormat densityMapFormat;
                VK_FORMAT_D16_UNORM,              // VkFormat depthFormat;
                SharedGroupParams(new GroupParams // SharedGroupParams groupParams;
                                  {
                                      groupParams->renderingType,        // RenderingType renderingType;
                                      cmdBuffType.useSecondaryCmdBuffer, // bool useSecondaryCmdBuffer;
                                      false, // bool secondaryCmdBufferCompletelyContainsDynamicRenderpass;
                                      PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC, // bool useGraphicsPipelineLibrary;
                                  })};
            std::string namePrefix = cmdBuffType.name;

            params.deferredDensityMap = false;
            params.dynamicDensityMap  = false;
            propertiesGroup->addChild(
                new FragmentDensityMapTest(testCtx, namePrefix + "imageless_framebuffer_static_subsampled", params));
            params.deferredDensityMap = true;
            propertiesGroup->addChild(
                new FragmentDensityMapTest(testCtx, namePrefix + "imageless_framebuffer_deferred_subsampled", params));
            params.deferredDensityMap = false;
            params.dynamicDensityMap  = true;
            propertiesGroup->addChild(
                new FragmentDensityMapTest(testCtx, namePrefix + "imageless_framebuffer_dynamic_subsampled", params));
        }
    }

    if (groupParams->renderingType == RENDERING_TYPE_RENDERPASS2)
    {
        TestParams params{
            false,                 // bool dynamicDensityMap;
            false,                 // bool deferredDensityMap;
            false,                 // bool nonSubsampledImages;
            true,                  // bool subsampledLoads;
            false,                 // bool coarseReconstruction;
            false,                 // bool imagelessFramebuffer;
            false,                 // bool useMemoryAccess;
            false,                 // bool useMaintenance5;
            1,                     // uint32_t samplersCount;
            2,                     // uint32_t viewCount;
            false,                 // bool multiViewport;
            false,                 // bool makeCopy;
            false,                 // bool depthEnabled;
            false,                 // bool addZeroOffset;
            4.0f,                  // float renderMultiplier;
            VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits colorSamples;
            {1, 2},                // tcu::UVec2 fragmentArea;
            {16, 16},              // tcu::UVec2 densityMapSize;
            VK_FORMAT_R8G8_UNORM,  // VkFormat densityMapFormat;
            VK_FORMAT_D16_UNORM,   // VkFormat depthFormat;
            groupParams            // SharedGroupParams groupParams;
        };
        propertiesGroup->addChild(new FragmentDensityMapTest(testCtx, "subsampled_loads", params));
        params.subsampledLoads      = false;
        params.coarseReconstruction = true;
        propertiesGroup->addChild(new FragmentDensityMapTest(testCtx, "subsampled_coarse_reconstruction", params));
        params.useMemoryAccess = true;
        propertiesGroup->addChild(new FragmentDensityMapTest(testCtx, "memory_access", params));
    }

    if (groupParams->renderingType != RENDERING_TYPE_RENDERPASS_LEGACY)
    {
        DE_ASSERT(groupParams->pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC);
        de::MovePtr<tcu::TestCaseGroup> offsetGroup(new tcu::TestCaseGroup(testCtx, "offset"));

        // Oversized FDM tests.
        {
            de::MovePtr<tcu::TestCaseGroup> oversizedFDMGroup(new tcu::TestCaseGroup(testCtx, "oversized_fdm"));

            struct
            {
                OffsetType horOffsetType;
                OffsetType vertOffsetType;
                const char *name;
            } offsetCases[] = {
                {OffsetType::NEGATIVE, OffsetType::NONE, "hor_offset_negative"},
                {OffsetType::NONE, OffsetType::NEGATIVE, "vert_offset_negative"},
            };

            for (const auto &offsetCase : offsetCases)
                for (const auto multiView : {false, true})
                    for (const auto resumeRendering : {false, true})
                        for (const bool extraLarge : {false, true})
                        {
                            if (groupParams->renderingType != RENDERING_TYPE_DYNAMIC_RENDERING && resumeRendering)
                                continue;

                            FDMOffsetParamsPtr params(new FDMOffsetOversizedFDMParams(
                                groupParams, offsetCase.horOffsetType, offsetCase.vertOffsetType, multiView,
                                resumeRendering, extraLarge));
                            const auto testName = std::string(offsetCase.name) + (multiView ? "_multiview" : "") +
                                                  (resumeRendering ? "_suspend_resume" : "") +
                                                  (extraLarge ? "_extra_large" : "");
                            oversizedFDMGroup->addChild(new FDMOffsetOversizedFDMCase(testCtx, testName, params));
                        }

            offsetGroup->addChild(oversizedFDMGroup.release());
        }

        // Minimum shift tests.
        {
            de::MovePtr<tcu::TestCaseGroup> minShiftGroup(new tcu::TestCaseGroup(testCtx, "min_shift"));

            struct
            {
                OffsetType horOffsetType;
                OffsetType vertOffsetType;
                const char *name;
            } offsetCases[] = {
                {OffsetType::POSITIVE, OffsetType::NONE, "hor_offset_positive"},
                {OffsetType::NEGATIVE, OffsetType::NONE, "hor_offset_negative"},
                {OffsetType::NONE, OffsetType::POSITIVE, "vert_offset_positive"},
                {OffsetType::NONE, OffsetType::NEGATIVE, "vert_offset_negative"},
            };

            for (const auto &offsetCase : offsetCases)
                for (const auto multiView : {false, true})
                    for (const auto resumeRendering : {false, true})
                    {
                        if (groupParams->renderingType != RENDERING_TYPE_DYNAMIC_RENDERING && resumeRendering)
                            continue;

                        FDMOffsetParamsPtr params(new FDMOffsetMinShiftParams(groupParams, offsetCase.horOffsetType,
                                                                              offsetCase.vertOffsetType, multiView,
                                                                              resumeRendering));
                        const auto testName = std::string(offsetCase.name) + (multiView ? "_multiview" : "") +
                                              (resumeRendering ? "_suspend_resume" : "");
                        minShiftGroup->addChild(new FDMOffsetMinShiftCase(testCtx, testName, params));
                    }

            offsetGroup->addChild(minShiftGroup.release());
        }

        // Clamp to edge tests.
        {
            de::MovePtr<tcu::TestCaseGroup> clampToEdgeGroup(new tcu::TestCaseGroup(testCtx, "clamp_to_edge"));

            struct
            {
                OffsetType horOffsetType;
                OffsetType vertOffsetType;
                const char *name;
            } offsetCases[] = {
                {OffsetType::POSITIVE, OffsetType::NONE, "hor_offset_positive"},
                {OffsetType::NEGATIVE, OffsetType::NONE, "hor_offset_negative"},
                {OffsetType::NONE, OffsetType::POSITIVE, "vert_offset_positive"},
                {OffsetType::NONE, OffsetType::NEGATIVE, "vert_offset_negative"},
            };

            for (const auto &offsetCase : offsetCases)
                for (const auto multiView : {false, true})
                    for (const auto resumeRendering : {false, true})
                    {
                        if (groupParams->renderingType != RENDERING_TYPE_DYNAMIC_RENDERING && resumeRendering)
                            continue;

                        FDMOffsetParamsPtr params(new FDMOffsetClampToEdgeParams(groupParams, offsetCase.horOffsetType,
                                                                                 offsetCase.vertOffsetType, multiView,
                                                                                 resumeRendering));
                        const auto testName = std::string(offsetCase.name) + (multiView ? "_multiview" : "") +
                                              (resumeRendering ? "_suspend_resume" : "");
                        clampToEdgeGroup->addChild(new FDMOffsetClampToEdgeCase(testCtx, testName, params));
                    }

            offsetGroup->addChild(clampToEdgeGroup.release());
        }

        fdmTests->addChild(offsetGroup.release());
    }

    fdmTests->addChild(propertiesGroup.release());
}

static void cleanupGroup(tcu::TestCaseGroup *group, const SharedGroupParams)
{
    DE_UNREF(group);
    // Destroy singleton objects.
    g_deviceHelperPtr.reset(nullptr);
}

tcu::TestCaseGroup *createFragmentDensityMapTests(tcu::TestContext &testCtx, const SharedGroupParams groupParams)
{
    // VK_EXT_fragment_density_map and VK_EXT_fragment_density_map2 extensions tests
    return createTestGroup(testCtx, "fragment_density_map", createChildren, groupParams, cleanupGroup);
}

} // namespace renderpass

} // namespace vkt
