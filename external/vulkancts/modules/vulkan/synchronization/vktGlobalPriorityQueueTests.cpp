/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
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
 * \file  vktGlobalPriorityQueueTests.cpp
 * \brief Global Priority Queue Tests
 *//*--------------------------------------------------------------------*/

#include "vktGlobalPriorityQueueTests.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktGlobalPriorityQueueUtils.hpp"

#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "../image/vktImageTestsUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkRefUtil.hpp"

#include "vktTestCase.hpp"

#include "deDefs.h"

#include "tcuImageCompare.hpp"
#include "tcuStringTemplate.hpp"

#include <string>
#include <sstream>
#include <map>
#include <iostream>
#include <memory>
#include <numeric>

using namespace vk;

namespace vkt
{
namespace synchronization
{
namespace
{

enum class SyncType
{
    None,
    Semaphore
};

struct TestConfig
{
    VkQueueFlagBits transitionFrom;
    VkQueueFlagBits transitionTo;
    VkQueueGlobalPriorityKHR priorityFrom;
    VkQueueGlobalPriorityKHR priorityTo;
    bool enableProtected;
    bool enableSparseBinding;
    SyncType syncType;
    uint32_t width;
    uint32_t height;
    VkFormat format;
    bool selectFormat(const InstanceInterface &vk, VkPhysicalDevice dev, std::initializer_list<VkFormat> formats);
};

bool TestConfig::selectFormat(const InstanceInterface &vk, VkPhysicalDevice dev,
                              std::initializer_list<VkFormat> formats)
{
    auto doesFormatMatch = [](const VkFormat fmt) -> bool
    {
        const auto tcuFmt = mapVkFormat(fmt);
        return tcuFmt.order == tcu::TextureFormat::ChannelOrder::R;
    };
    VkFormatProperties2 props{};
    const VkFormatFeatureFlags flags = VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    for (auto i = formats.begin(); i != formats.end(); ++i)
    {
        props.sType            = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
        props.pNext            = nullptr;
        props.formatProperties = {};
        const VkFormat fmt     = *i;
        vk.getPhysicalDeviceFormatProperties2(dev, fmt, &props);
        if (doesFormatMatch(fmt) && ((props.formatProperties.optimalTilingFeatures & flags) == flags))
        {
            this->format = fmt;
            return true;
        }
    }
    return false;
}

template <class T, class P = T (*)[1]>
auto begin(void *p) -> decltype(std::begin(*std::declval<P>()))
{
    return std::begin(*static_cast<P>(p));
}

class GPQInstanceBase : public TestInstance
{
public:
    typedef std::initializer_list<VkDescriptorSetLayout> DSLayouts;
    typedef tcu::ConstPixelBufferAccess BufferAccess;

    GPQInstanceBase(Context &ctx, const TestConfig &cfg);
    template <class PushConstant = void>
    auto createPipelineLayout(DSLayouts setLayouts) const -> Move<VkPipelineLayout>;
    auto makeCommandPool(uint32_t qFamilyIndex) const -> Move<VkCommandPool>;
    auto createGraphicsPipeline(VkPipelineLayout pipelineLayout, VkRenderPass renderPass) -> Move<VkPipeline>;
    auto createComputePipeline(VkPipelineLayout pipelineLayout, bool producer) -> Move<VkPipeline>;
    auto createImage(VkImageUsageFlags usage, uint32_t queueFamilyIdx, VkQueue queue) const
        -> de::MovePtr<ImageWithMemory>;
    auto createView(VkImage image, VkImageSubresourceRange &range) const -> Move<VkImageView>;
    bool submitCommands(VkCommandBuffer producerCmd, VkCommandBuffer consumerCmd) const;

protected:
    auto createPipelineLayout(const VkPushConstantRange *pRange, DSLayouts setLayouts) const -> Move<VkPipelineLayout>;
    const TestConfig m_config;
    const SpecialDevice m_device;
    struct NamedShader
    {
        std::string name;
        Move<VkShaderModule> handle;
    } m_shaders[4];
};
GPQInstanceBase::GPQInstanceBase(Context &ctx, const TestConfig &cfg)
    : TestInstance(ctx)
    , m_config(cfg)
    , m_device(ctx, cfg.transitionFrom, cfg.transitionTo, cfg.priorityFrom, cfg.priorityTo, cfg.enableProtected,
               cfg.enableSparseBinding)
    , m_shaders()
{
    m_shaders[0].name = "vert"; // vertex
    m_shaders[1].name = "frag"; // fragment
    m_shaders[2].name = "cpyb"; // compute
    m_shaders[3].name = "cpyi"; // compute
}

de::MovePtr<ImageWithMemory> GPQInstanceBase::createImage(VkImageUsageFlags usage, uint32_t queueFamilyIdx,
                                                          VkQueue queue) const
{
    const InstanceInterface &vki = m_context.getInstanceInterface();
    const DeviceInterface &vkd   = m_context.getDeviceInterface();
    const VkPhysicalDevice phys  = m_context.getPhysicalDevice();
    const VkDevice dev           = m_device.handle;
    Allocator &alloc             = m_device.getAllocator();
    VkImageCreateFlags flags     = 0;

    if (m_config.enableProtected)
        flags |= VK_IMAGE_CREATE_PROTECTED_BIT;
    if (m_config.enableSparseBinding)
        flags |= (VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT);
    const MemoryRequirement memReqs = m_config.enableProtected ? MemoryRequirement::Protected : MemoryRequirement::Any;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext                 = nullptr;
    imageInfo.flags                 = flags;
    imageInfo.imageType             = VK_IMAGE_TYPE_2D;
    imageInfo.format                = m_config.format;
    imageInfo.extent.width          = m_config.width;
    imageInfo.extent.height         = m_config.height;
    imageInfo.extent.depth          = 1;
    imageInfo.mipLevels             = 1;
    imageInfo.arrayLayers           = 1;
    imageInfo.samples               = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling                = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage                 = usage;
    imageInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.queueFamilyIndexCount = 1;
    imageInfo.pQueueFamilyIndices   = &queueFamilyIdx;
    imageInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;

    return de::MovePtr<ImageWithMemory>(new ImageWithMemory(vki, vkd, phys, dev, alloc, imageInfo, queue, memReqs));
}

Move<VkImageView> GPQInstanceBase::createView(VkImage image, VkImageSubresourceRange &range) const
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    const VkDevice dev         = m_device.handle;

    range = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
    return makeImageView(vkd, dev, image, VK_IMAGE_VIEW_TYPE_2D, m_config.format, range);
}

Move<VkPipelineLayout> GPQInstanceBase::createPipelineLayout(const VkPushConstantRange *pRange,
                                                             DSLayouts setLayouts) const
{
    std::vector<VkDescriptorSetLayout> layouts(setLayouts.size());
    auto ii = setLayouts.begin();
    for (auto i = ii; i != setLayouts.end(); ++i)
        layouts[std::distance(ii, i)] = *i;

    VkPipelineLayoutCreateInfo info{};
    info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = VkPipelineLayoutCreateFlags(0);
    info.setLayoutCount         = static_cast<uint32_t>(layouts.size());
    info.pSetLayouts            = layouts.size() ? layouts.data() : nullptr;
    info.pushConstantRangeCount = (pRange != nullptr && pRange->size > 0) ? 1 : 0;
    info.pPushConstantRanges    = (pRange != nullptr && pRange->size > 0) ? pRange : nullptr;

    return ::vk::createPipelineLayout(m_context.getDeviceInterface(), m_device.handle, &info);
}

template <>
Move<VkPipelineLayout> DE_UNUSED_FUNCTION GPQInstanceBase::createPipelineLayout<void>(DSLayouts setLayouts) const
{
    return createPipelineLayout(nullptr, setLayouts);
}

template <class PushConstant>
Move<VkPipelineLayout> GPQInstanceBase::createPipelineLayout(DSLayouts setLayouts) const
{
    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_ALL;
    range.offset     = 0;
    range.size       = static_cast<uint32_t>(sizeof(PushConstant));
    return createPipelineLayout(&range, setLayouts);
}

Move<VkCommandPool> GPQInstanceBase::makeCommandPool(uint32_t qFamilyIndex) const
{
    const VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
                                           (m_config.enableProtected ? VK_COMMAND_POOL_CREATE_PROTECTED_BIT : 0);
    const VkCommandPoolCreateInfo commandPoolParams = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, // VkStructureType sType;
        nullptr,                                    // const void* pNext;
        flags,                                      // VkCommandPoolCreateFlags flags;
        qFamilyIndex,                               // uint32_t queueFamilyIndex;
    };

    return createCommandPool(m_context.getDeviceInterface(), m_device.handle, &commandPoolParams);
}

Move<VkPipeline> GPQInstanceBase::createGraphicsPipeline(VkPipelineLayout pipelineLayout, VkRenderPass renderPass)
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    const VkDevice dev         = m_device.handle;

    auto sh = std::find_if(std::begin(m_shaders), std::end(m_shaders),
                           [](const NamedShader &ns) { return ns.name == "vert"; });
    if (*sh->handle == VK_NULL_HANDLE)
        sh->handle = createShaderModule(vkd, dev, m_context.getBinaryCollection().get("vert"));
    VkShaderModule vertex = *sh->handle;

    sh = std::find_if(std::begin(m_shaders), std::end(m_shaders),
                      [](const NamedShader &ns) { return ns.name == "frag"; });
    if (*sh->handle == VK_NULL_HANDLE)
        sh->handle = createShaderModule(vkd, dev, m_context.getBinaryCollection().get("frag"));
    VkShaderModule fragment = *sh->handle;

    const std::vector<VkViewport> viewports{makeViewport(m_config.width, m_config.height)};
    const std::vector<VkRect2D> scissors{makeRect2D(m_config.width, m_config.height)};
    const auto vertexBinding =
        makeVertexInputBindingDescription(0u, static_cast<uint32_t>(2 * sizeof(float)), VK_VERTEX_INPUT_RATE_VERTEX);
    const auto vertexAttrib = makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32_SFLOAT, 0u);
    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{
        vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                       // const void* pNext;
        0u,                                                            // VkPipelineVertexInputStateCreateFlags flags;
        1u,                                                            // uint32_t vertexBindingDescriptionCount;
        &vertexBinding, // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        1u,             // uint32_t vertexAttributeDescriptionCount;
        &vertexAttrib   // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };

    return makeGraphicsPipeline(vkd, dev, pipelineLayout, vertex, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                fragment, renderPass, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, 0,
                                &vertexInputStateCreateInfo);
}

Move<VkPipeline> GPQInstanceBase::createComputePipeline(VkPipelineLayout pipelineLayout, bool producer)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice dev        = m_device.handle;

    const std::string compName = producer ? "cpyb" : "cpyi";
    auto comp                  = std::find_if(std::begin(m_shaders), std::end(m_shaders),
                                              [&](const NamedShader &ns) { return ns.name == compName; });
    if (*comp->handle == VK_NULL_HANDLE)
        comp->handle = createShaderModule(vk, dev, m_context.getBinaryCollection().get(compName));
    VkShaderModule compute = *comp->handle;

    VkPipelineShaderStageCreateInfo sci{};
    sci.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    sci.pNext               = nullptr;
    sci.flags               = VkPipelineShaderStageCreateFlags(0);
    sci.stage               = VK_SHADER_STAGE_COMPUTE_BIT;
    sci.module              = compute;
    sci.pName               = "main";
    sci.pSpecializationInfo = nullptr;

    VkComputePipelineCreateInfo ci{};
    ci.sType              = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    ci.pNext              = nullptr;
    ci.flags              = VkPipelineCreateFlags(0);
    ci.stage              = sci;
    ci.layout             = pipelineLayout;
    ci.basePipelineHandle = VK_NULL_HANDLE;
    ci.basePipelineIndex  = 0;

    return vk::createComputePipeline(vk, dev, VK_NULL_HANDLE, &ci, nullptr);
}

VkPipelineStageFlags queueFlagBitToPipelineStage(VkQueueFlagBits bit);
bool GPQInstanceBase::submitCommands(VkCommandBuffer producerCmd, VkCommandBuffer consumerCmd) const
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    const VkDevice dev         = m_device.handle;

    Move<VkSemaphore> sem       = createSemaphore(vkd, dev);
    Move<VkFence> consumerFence = createFence(vkd, dev);

    VkProtectedSubmitInfo protectedSubmitInfo{
        VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO, // VkStructureType sType;
        nullptr,                                 // void* pNext;
        VK_TRUE                                  // VkBool32 protectedSubmit;
    };

    const VkSubmitInfo producerSubmitInfo{
        VK_STRUCTURE_TYPE_SUBMIT_INFO,                              // VkStructureType sType;
        m_config.enableProtected ? &protectedSubmitInfo : nullptr,  // const void* pNext;
        0,                                                          // uint32_t waitSemaphoreCount;
        nullptr,                                                    // const VkSemaphore* pWaitSemaphores;
        nullptr,                                                    // const VkPipelineStageFlags* pWaitDstStageMask;
        1u,                                                         // uint32_t commandBufferCount;
        &producerCmd,                                               // const VkCommandBuffer* pCommandBuffers;
        m_config.syncType == SyncType::None ? 0u : 1u,              // uint32_t signalSemaphoreCount;
        m_config.syncType == SyncType::None ? nullptr : &sem.get(), // const VkSemaphore* pSignalSemaphores;
    };

    const VkPipelineStageFlags dstWaitStages =
        VK_PIPELINE_STAGE_TRANSFER_BIT | queueFlagBitToPipelineStage(m_config.transitionTo);
    const VkSubmitInfo consumerSubmitInfo{
        VK_STRUCTURE_TYPE_SUBMIT_INFO,                              // VkStructureType sType;
        m_config.enableProtected ? &protectedSubmitInfo : nullptr,  // const void* pNext;
        m_config.syncType == SyncType::None ? 0u : 1u,              // uint32_t waitSemaphoreCount;
        m_config.syncType == SyncType::None ? nullptr : &sem.get(), // const VkSemaphore* pWaitSemaphores;
        m_config.syncType == SyncType::None ? nullptr :
                                              &dstWaitStages, // const VkPipelineStageFlags* pWaitDstStageMask;
        1u,                                                   // uint32_t commandBufferCount;
        &consumerCmd,                                         // const VkCommandBuffer* pCommandBuffers;
        0,                                                    // uint32_t signalSemaphoreCount;
        nullptr,                                              // const VkSemaphore* pSignalSemaphores;
    };

    // 10 second timeout in case we fail protected memory validation and infinitely loop.
    const uint64_t timeout     = 10ull * 1000ull * 1000ull * 1000ull;
    VkResult queueSubmitResult = VK_SUCCESS;

    Move<VkFence> producerFence = createFence(vkd, dev);
    VK_CHECK(vkd.queueSubmit(m_device.queueFrom, 1u, &producerSubmitInfo, *producerFence));
    VK_CHECK(vkd.waitForFences(dev, 1u, &producerFence.get(), true, timeout));
    VK_CHECK(vkd.queueSubmit(m_device.queueTo, 1u, &consumerSubmitInfo, *consumerFence));
    queueSubmitResult = vkd.waitForFences(dev, 1u, &consumerFence.get(), true, timeout);

    return (queueSubmitResult == VK_SUCCESS);
}

template <VkQueueFlagBits, VkQueueFlagBits>
class GPQInstance;
#define DECLARE_INSTANCE(flagsFrom_, flagsTo_)                                       \
    template <>                                                                      \
    class GPQInstance<flagsFrom_, flagsTo_> : public GPQInstanceBase                 \
    {                                                                                \
    public:                                                                          \
        GPQInstance(Context &ctx, const TestConfig &cfg) : GPQInstanceBase(ctx, cfg) \
        {                                                                            \
        }                                                                            \
        virtual tcu::TestStatus iterate(void) override;                              \
    }

DECLARE_INSTANCE(VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT);
DECLARE_INSTANCE(VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT);

class GPQCase;
typedef TestInstance *(GPQCase::*CreateInstanceProc)(Context &) const;
typedef std::pair<VkQueueFlagBits, VkQueueFlagBits> CreateInstanceKey;
typedef std::map<CreateInstanceKey, CreateInstanceProc> CreateInstanceMap;
#define MAPENTRY(from_, to_) m_createInstanceMap[{from_, to_}] = &GPQCase::createInstance<from_, to_>

class GPQCase : public TestCase
{
public:
    GPQCase(tcu::TestContext &ctx, const std::string &name, const TestConfig &cfg);
    void initPrograms(SourceCollections &programs) const override;
    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;
    static uint32_t testValue;

private:
    template <VkQueueFlagBits, VkQueueFlagBits>
    TestInstance *createInstance(Context &context) const;
    mutable TestConfig m_config;
    CreateInstanceMap m_createInstanceMap;
};
uint32_t GPQCase::testValue = 113;

GPQCase::GPQCase(tcu::TestContext &ctx, const std::string &name, const TestConfig &cfg)
    : TestCase(ctx, name)
    , m_config(cfg)
    , m_createInstanceMap()
{
    MAPENTRY(VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT);
    MAPENTRY(VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT);
}

VkPipelineStageFlags queueFlagBitToPipelineStage(VkQueueFlagBits bit)
{
    switch (bit)
    {
    case VK_QUEUE_COMPUTE_BIT:
        return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    case VK_QUEUE_GRAPHICS_BIT:
        return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    default:
        DE_ASSERT(VK_FALSE);
    }
    return VK_QUEUE_FLAG_BITS_MAX_ENUM;
}

template <VkQueueFlagBits flagsFrom, VkQueueFlagBits flagsTo>
TestInstance *GPQCase::createInstance(Context &context) const
{
    return new GPQInstance<flagsFrom, flagsTo>(context, m_config);
}

TestInstance *GPQCase::createInstance(Context &context) const
{
    const CreateInstanceKey key(m_config.transitionFrom, m_config.transitionTo);
    return (this->*(m_createInstanceMap.at(key)))(context);
}

std::ostream &operator<<(std::ostream &str, const VkQueueFlagBits &bit)
{
    const char *s = nullptr;
    const auto d  = std::to_string(bit);
    switch (bit)
    {
    case VK_QUEUE_GRAPHICS_BIT:
        s = "VK_QUEUE_GRAPHICS_BIT";
        break;
    case VK_QUEUE_COMPUTE_BIT:
        s = "VK_QUEUE_COMPUTE_BIT";
        break;
    case VK_QUEUE_TRANSFER_BIT:
        s = "VK_QUEUE_TRANSFER_BIT";
        break;
    case VK_QUEUE_SPARSE_BINDING_BIT:
        s = "VK_QUEUE_SPARSE_BINDING_BIT";
        break;
    case VK_QUEUE_PROTECTED_BIT:
        s = "VK_QUEUE_PROTECTED_BIT";
        break;
    default:
        s = d.c_str();
        break;
    }
    return (str << s);
}

void GPQCase::checkSupport(Context &context) const
{
    const InstanceInterface &vki = context.getInstanceInterface();
    const VkPhysicalDevice dev   = context.getPhysicalDevice();

    context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");
    context.requireDeviceFunctionality("VK_EXT_global_priority_query");
    context.requireDeviceFunctionality("VK_EXT_global_priority");

    if (!m_config.selectFormat(vki, dev,
                               {VK_FORMAT_R32_SINT, VK_FORMAT_R32_UINT, VK_FORMAT_R8_SINT, VK_FORMAT_R8_UINT}))
    {
        TCU_THROW(NotSupportedError, "Unable to find a proper format");
    }

    VkPhysicalDeviceProtectedMemoryFeatures memFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES,
                                                        nullptr, VK_FALSE};
    VkPhysicalDeviceFeatures2 devFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &memFeatures, {}};
    vki.getPhysicalDeviceFeatures2(dev, &devFeatures);

    if (m_config.enableProtected && (VK_FALSE == memFeatures.protectedMemory))
    {
        TCU_THROW(NotSupportedError, "Queue families with VK_QUEUE_PROTECTED_BIT not supported");
    }

    const VkBool32 sparseEnabled = devFeatures.features.sparseBinding & devFeatures.features.sparseResidencyBuffer &
                                   devFeatures.features.sparseResidencyImage2D;
    if (m_config.enableSparseBinding && (VK_FALSE == sparseEnabled))
    {
        TCU_THROW(NotSupportedError, "Queue families with VK_QUEUE_SPARSE_BINDING_BIT not supported");
    }

    auto assertUnavailableQueue = [](const uint32_t qIdx, VkQueueFlagBits qfb, VkQueueGlobalPriorityKHR qgp)
    {
        if (qIdx == INVALID_UINT32)
        {
            std::ostringstream buf;
            buf << "Unable to find queue " << qfb << " with priority " << qgp;
            buf.flush();
            TCU_THROW(NotSupportedError, buf.str());
        }
    };

    VkQueueFlags flagsFrom = m_config.transitionFrom;
    VkQueueFlags flagsTo   = m_config.transitionTo;
    if (m_config.enableProtected)
    {
        flagsFrom |= VK_QUEUE_PROTECTED_BIT;
        flagsTo |= VK_QUEUE_PROTECTED_BIT;
    }
    if (m_config.enableSparseBinding)
    {
        flagsFrom |= VK_QUEUE_SPARSE_BINDING_BIT;
        flagsTo |= VK_QUEUE_SPARSE_BINDING_BIT;
    }

    const uint32_t queueFromIndex = findQueueFamilyIndex(vki, dev, m_config.priorityFrom, flagsFrom,
                                                         SpecialDevice::getColissionFlags(flagsFrom), INVALID_UINT32);
    assertUnavailableQueue(queueFromIndex, m_config.transitionFrom, m_config.priorityFrom);

    const uint32_t queueToIndex = findQueueFamilyIndex(vki, dev, m_config.priorityTo, flagsTo,
                                                       SpecialDevice::getColissionFlags(flagsTo), queueFromIndex);
    assertUnavailableQueue(queueToIndex, m_config.transitionTo, m_config.priorityTo);

    if (queueFromIndex == queueToIndex)
    {
        std::ostringstream buf;
        buf << "Unable to find separate queues " << m_config.transitionFrom << " and " << m_config.transitionTo;
        buf.flush();
        TCU_THROW(NotSupportedError, buf.str());
    }
}

void GPQCase::initPrograms(SourceCollections &programs) const
{
    const std::string producerComp(R"glsl(
    #version 450
    layout(binding=0) buffer S { float src[]; };
    layout(binding=1) buffer D { float dst[]; };
    layout(binding=2) buffer ProtectedHelper
    {
        highp uint zero; // set to 0
        highp uint unusedOut;
    } helper;
    layout(local_size_x=1,local_size_y=1) in;
    void main() {
        helper.zero = 0;
        dst[gl_GlobalInvocationID.x] = src[gl_GlobalInvocationID.x];
    }
    )glsl");

    const tcu::StringTemplate consumerComp(R"glsl(
    #version 450
    layout(local_size_x=1,local_size_y=1) in;
    layout(${IMAGE_FORMAT}, binding=0) readonly uniform ${IMAGE_TYPE} srcImage;
    layout(binding=1) writeonly coherent buffer Pixels { uint data[]; } dstBuffer;
    void main()
    {
        ivec2 srcIdx = ivec2(gl_GlobalInvocationID.xy);
        int   width  = imageSize(srcImage).x;
        int   dstIdx = int(gl_GlobalInvocationID.y * width + gl_GlobalInvocationID.x);
        dstBuffer.data[dstIdx] = uint(imageLoad(srcImage, srcIdx).r) == ${TEST_VALUE} ? 1 : 0;
    }
    )glsl");

    const tcu::StringTemplate protectedConsumerComp(R"glsl(
    #version 450
    layout(local_size_x=1,local_size_y=1) in;
    layout(${IMAGE_FORMAT}, binding=0) readonly coherent uniform ${IMAGE_TYPE} srcImage;
    layout(binding=1) coherent buffer ProtectedHelper
    {
        highp uint zero; // set to 0
        highp uint unusedOut;
    } helper;

    void error()
    {
        for (uint x = 0; x < 10; x += helper.zero)
        {
            atomicAdd(helper.unusedOut, 1u);
        }
    }

    void main()
    {
        ivec2 srcIdx = ivec2(gl_GlobalInvocationID.xy);

        // To match the non-protected validation, we only validate (0, 0).
        if (srcIdx == ivec2(0, 0))
        {
            if (uint(imageLoad(srcImage, srcIdx).r) != ${TEST_VALUE})
            {
                error();
            }
        }
    }
    )glsl");

    const std::string vert(R"glsl(
    #version 450
    layout(location = 0) in vec2 pos;
    void main()
    {
       gl_Position = vec4(pos, 0.0, 1.01);
    }
    )glsl");

    const tcu::StringTemplate frag(R"glsl(
    #version 450
    layout(location = 0) out ${COLOR_TYPE} color;
    void main()
    {
       color = ${COLOR_TYPE}(${TEST_VALUE},0,0,1);
    }
    )glsl");

    const auto format      = mapVkFormat(m_config.format);
    const auto imageFormat = image::getShaderImageFormatQualifier(format);
    const auto imageType   = image::getShaderImageType(format, image::ImageType::IMAGE_TYPE_2D, false);
    const auto colorType   = image::getGlslAttachmentType(m_config.format); // ivec4

    const std::map<std::string, std::string> abbreviations{
        {std::string("TEST_VALUE"), std::to_string(testValue)},
        {std::string("IMAGE_FORMAT"), std::string(imageFormat)},
        {std::string("IMAGE_TYPE"), std::string(imageType)},
        {std::string("COLOR_TYPE"), std::string(colorType)},
    };

    programs.glslSources.add("cpyb") << glu::ComputeSource(producerComp);
    if (m_config.enableProtected)
        programs.glslSources.add("cpyi") << glu::ComputeSource(protectedConsumerComp.specialize(abbreviations));
    else
        programs.glslSources.add("cpyi") << glu::ComputeSource(consumerComp.specialize(abbreviations));
    programs.glslSources.add("vert") << glu::VertexSource(vert);
    programs.glslSources.add("frag") << glu::FragmentSource(frag.specialize(abbreviations));
}

tcu::TestStatus GPQInstance<VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT>::iterate(void)
{
    if (VK_SUCCESS != m_device.createResult)
    {
        if (VK_ERROR_NOT_PERMITTED_KHR == m_device.createResult)
            return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                                   "Custom device creation returned " +
                                       std::string(getResultName(m_device.createResult)));
        throw NotSupportedError(m_device.createResult, getResultName(m_device.createResult), m_device.createExpression,
                                m_device.createFileName, m_device.createFileLine);
    }

    const InstanceInterface &vki = m_context.getInstanceInterface();
    const DeviceInterface &vkd   = m_context.getDeviceInterface();
    const VkPhysicalDevice phys  = m_context.getPhysicalDevice();
    const VkDevice device        = m_device.handle;
    Allocator &allocator         = m_device.getAllocator();
    const uint32_t producerIndex = m_device.queueFamilyIndexFrom;
    const uint32_t consumerIndex = m_device.queueFamilyIndexTo;
    const std::vector<uint32_t> producerIndices{producerIndex};
    const std::vector<uint32_t> consumerIndices{consumerIndex};
    const std::vector<uint32_t> helperIndices{producerIndex, consumerIndex};
    const VkQueue producerQueue = m_device.queueFrom;
    const VkQueue consumerQueue = m_device.queueTo;

    // stagging buffer for vertices
    const std::vector<float> positions{+1.f, -1.f, -1.f, -1.f, 0.f, +1.f};
    const VkBufferCreateInfo posBuffInfo =
        makeBufferCreateInfo(positions.size() * sizeof(float), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, producerIndices);
    BufferWithMemory positionsBuffer(vki, vkd, phys, device, allocator, posBuffInfo, MemoryRequirement::HostVisible);
    std::copy_n(positions.data(), positions.size(), begin<float>(positionsBuffer.getHostPtr()));
    const VkDescriptorBufferInfo posDsBuffInfo =
        makeDescriptorBufferInfo(positionsBuffer.get(), 0, positionsBuffer.getSize());

    // vertex buffer
    VkBufferCreateFlags vertCreateFlags = 0;
    if (m_config.enableProtected)
        vertCreateFlags |= VK_BUFFER_CREATE_PROTECTED_BIT;
    if (m_config.enableSparseBinding)
        vertCreateFlags |= VK_BUFFER_CREATE_SPARSE_BINDING_BIT;
    const VkBufferUsageFlags vertBuffUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    const MemoryRequirement vertMemReqs =
        (m_config.enableProtected ? MemoryRequirement::Protected : MemoryRequirement::Any);
    const VkBufferCreateInfo vertBuffInfo =
        makeBufferCreateInfo(positionsBuffer.getSize(), vertBuffUsage, producerIndices, vertCreateFlags);
    const BufferWithMemory vertexBuffer(vki, vkd, phys, device, allocator, vertBuffInfo, vertMemReqs, producerQueue);
    const VkDescriptorBufferInfo vertDsBuffInfo =
        makeDescriptorBufferInfo(vertexBuffer.get(), 0ull, vertexBuffer.getSize());

    // helper buffer for the protected memory variant
    const uint32_t helperBuffSize = (uint32_t)(2 * sizeof(uint32_t));
    const VkBufferCreateInfo helperBuffInfo =
        makeBufferCreateInfo(helperBuffSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, helperIndices,
                             m_config.enableProtected ? VK_BUFFER_CREATE_PROTECTED_BIT : 0);
    BufferWithMemory helperBuffer(vki, vkd, phys, device, allocator, helperBuffInfo,
                                  m_config.enableProtected ? MemoryRequirement::Protected : MemoryRequirement::Any);
    const VkDescriptorBufferInfo helperDsBuffInfo = makeDescriptorBufferInfo(helperBuffer.get(), 0, helperBuffSize);

    // descriptor set for stagging and vertex buffers
    Move<VkDescriptorPool> producerDsPool =
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    Move<VkDescriptorSetLayout> producerDsLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL)
            .build(vkd, device);
    Move<VkDescriptorSet> producerDs = makeDescriptorSet(vkd, device, *producerDsPool, *producerDsLayout);
    DescriptorSetUpdateBuilder()
        .writeSingle(*producerDs, DescriptorSetUpdateBuilder::Location::binding(0), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     &posDsBuffInfo)
        .writeSingle(*producerDs, DescriptorSetUpdateBuilder::Location::binding(1), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     &vertDsBuffInfo)
        .writeSingle(*producerDs, DescriptorSetUpdateBuilder::Location::binding(2), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     &helperDsBuffInfo)
        .update(vkd, device);

    // consumer image
    const uint32_t clearComp      = 97;
    const VkClearValue clearColor = makeClearValueColorU32(clearComp, clearComp, clearComp, clearComp);
    VkImageSubresourceRange imageResourceRange{};
    const VkImageUsageFlags imageUsage = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
    de::MovePtr<ImageWithMemory> image = createImage(imageUsage, consumerIndex, consumerQueue);
    Move<VkImageView> view             = createView(**image, imageResourceRange);
    Move<VkRenderPass> renderPass      = makeRenderPass(vkd, device, m_config.format);
    Move<VkFramebuffer> framebuffer = makeFramebuffer(vkd, device, *renderPass, *view, m_config.width, m_config.height);
    const VkDescriptorImageInfo imageDsInfo = makeDescriptorImageInfo(VK_NULL_HANDLE, *view, VK_IMAGE_LAYOUT_GENERAL);
    const VkImageMemoryBarrier imageReadyBarrier = makeImageMemoryBarrier(
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL, **image, imageResourceRange, consumerIndex, consumerIndex);

    // stagging buffer for result
    const VkDeviceSize resultBuffSize =
        (m_config.width * m_config.height * mapVkFormat(m_config.format).getPixelSize());
    const VkBufferCreateInfo resultBuffInfo =
        makeBufferCreateInfo(resultBuffSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, consumerIndices);
    BufferWithMemory resultBuffer(vki, vkd, phys, device, allocator, resultBuffInfo, MemoryRequirement::HostVisible);
    const VkDescriptorBufferInfo resultDsBuffInfo = makeDescriptorBufferInfo(resultBuffer.get(), 0ull, resultBuffSize);
    const VkMemoryBarrier resultReadyBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);

    // descriptor set for consumer image and result buffer
    Move<VkDescriptorPool> consumerDsPool =
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    Move<VkDescriptorSetLayout> consumerDsLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL)
            .build(vkd, device);
    Move<VkDescriptorSet> consumerDs = makeDescriptorSet(vkd, device, *consumerDsPool, *consumerDsLayout);

    DescriptorSetUpdateBuilder()
        .writeSingle(*consumerDs, DescriptorSetUpdateBuilder::Location::binding(0), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                     &imageDsInfo)
        .writeSingle(*consumerDs, DescriptorSetUpdateBuilder::Location::binding(1), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     m_config.enableProtected ? &helperDsBuffInfo : &resultDsBuffInfo)
        .update(vkd, device);

    Move<VkPipelineLayout> producerLayout = createPipelineLayout<>({*producerDsLayout});
    Move<VkPipeline> producerPipeline     = createComputePipeline(*producerLayout, true);

    Move<VkPipelineLayout> consumerLayout = createPipelineLayout<>({*consumerDsLayout});
    Move<VkPipeline> consumerPipeline     = createGraphicsPipeline(*consumerLayout, *renderPass);

    Move<VkPipelineLayout> resultLayout = createPipelineLayout<>({*consumerDsLayout});
    Move<VkCommandPool> resultPool      = makeCommandPool(consumerIndex);
    Move<VkPipeline> resultPipeline     = createComputePipeline(*resultLayout, false);

    Move<VkCommandPool> producerPool = makeCommandPool(producerIndex);
    Move<VkCommandPool> consumerPool = makeCommandPool(consumerIndex);
    Move<VkCommandBuffer> producerCmd =
        allocateCommandBuffer(vkd, device, *producerPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    Move<VkCommandBuffer> consumerCmd =
        allocateCommandBuffer(vkd, device, *consumerPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vkd, *producerCmd);
    vkd.cmdBindPipeline(*producerCmd, VK_PIPELINE_BIND_POINT_COMPUTE, *producerPipeline);
    vkd.cmdBindDescriptorSets(*producerCmd, VK_PIPELINE_BIND_POINT_COMPUTE, *producerLayout, 0, 1, &(*producerDs), 0,
                              nullptr);
    vkd.cmdDispatch(*producerCmd, uint32_t(positions.size()), 1, 1);
    endCommandBuffer(vkd, *producerCmd);

    beginCommandBuffer(vkd, *consumerCmd);
    vkd.cmdBindPipeline(*consumerCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *consumerPipeline);
    vkd.cmdBindPipeline(*consumerCmd, VK_PIPELINE_BIND_POINT_COMPUTE, *resultPipeline);
    vkd.cmdBindDescriptorSets(*consumerCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *consumerLayout, 0, 1, &(*consumerDs), 0,
                              nullptr);
    vkd.cmdBindDescriptorSets(*consumerCmd, VK_PIPELINE_BIND_POINT_COMPUTE, *resultLayout, 0, 1, &(*consumerDs), 0,
                              nullptr);
    vkd.cmdBindVertexBuffers(*consumerCmd, 0, 1, vertexBuffer.getPtr(), &static_cast<const VkDeviceSize &>(0));

    beginRenderPass(vkd, *consumerCmd, *renderPass, *framebuffer, makeRect2D(m_config.width, m_config.height),
                    clearColor);
    vkd.cmdDraw(*consumerCmd, uint32_t(positions.size()), 1, 0, 0);
    endRenderPass(vkd, *consumerCmd);
    vkd.cmdPipelineBarrier(*consumerCmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &imageReadyBarrier);

    vkd.cmdDispatch(*consumerCmd, m_config.width, m_config.height, 1);
    vkd.cmdPipelineBarrier(*consumerCmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                           &resultReadyBarrier, 0u, nullptr, 0u, nullptr);
    endCommandBuffer(vkd, *consumerCmd);

    bool submitSuccess = submitCommands(*producerCmd, *consumerCmd);
    resultBuffer.invalidateAlloc(vkd, device);

    // For protected memory variant, we cannot actually get any information about the memory. If the shader runs to
    // completion it is a pass, if it loops indefinitely it's a fail.
    if (m_config.enableProtected)
    {
        return (submitSuccess ? tcu::TestStatus::pass("Validation compute shader ran successfully") :
                                tcu::TestStatus::fail("Validation compute shader failed to run to completion"));
    }
    else
    {
        const tcu::ConstPixelBufferAccess resultBufferAccess(mapVkFormat(m_config.format), m_config.width,
                                                             m_config.height, 1, resultBuffer.getHostPtr());
        const uint32_t resultValue   = resultBufferAccess.getPixelUint(0, 0).x();
        const uint32_t expectedValue = 1;
        const bool ok                = (resultValue == expectedValue);
        if (!ok)
        {
            m_context.getTestContext().getLog() << tcu::TestLog::Message << "Expected value: " << expectedValue
                                                << ", got " << resultValue << tcu::TestLog::EndMessage;
        }

        return ok ? tcu::TestStatus::pass("") : tcu::TestStatus::fail("");
    }
}

tcu::TestStatus GPQInstance<VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT>::iterate(void)
{
    if (VK_SUCCESS != m_device.createResult)
    {
        if (VK_ERROR_NOT_PERMITTED_KHR == m_device.createResult)
            return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                                   "Custom device creation returned " +
                                       std::string(getResultName(m_device.createResult)));
        throw NotSupportedError(m_device.createResult, getResultName(m_device.createResult), m_device.createExpression,
                                m_device.createFileName, m_device.createFileLine);
    }

    const InstanceInterface &vki = m_context.getInstanceInterface();
    const DeviceInterface &vkd   = m_context.getDeviceInterface();
    const VkPhysicalDevice phys  = m_context.getPhysicalDevice();
    const VkDevice device        = m_device.handle;
    Allocator &allocator         = m_device.getAllocator();
    const uint32_t producerIndex = m_device.queueFamilyIndexFrom;
    const uint32_t consumerIndex = m_device.queueFamilyIndexTo;
    const std::vector<uint32_t> producerIndices{producerIndex};
    const std::vector<uint32_t> consumerIndices{consumerIndex};
    const std::vector<uint32_t> helperIndices{producerIndex, consumerIndex};
    const VkQueue producerQueue = m_device.queueFrom;

    // stagging buffer for vertices
    const std::vector<float> positions{+1.f, -1.f, -1.f, -1.f, 0.f, +1.f};
    const VkBufferCreateInfo positionBuffInfo =
        makeBufferCreateInfo(positions.size() * sizeof(float), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, producerIndices);
    BufferWithMemory positionsBuffer(vki, vkd, phys, device, allocator, positionBuffInfo,
                                     MemoryRequirement::HostVisible);
    std::copy_n(positions.data(), positions.size(), begin<float>(positionsBuffer.getHostPtr()));
    const VkDescriptorBufferInfo posDsBuffInfo =
        makeDescriptorBufferInfo(positionsBuffer.get(), 0, positionsBuffer.getSize());

    // vertex buffer
    VkBufferCreateFlags vertCreateFlags = 0;
    if (m_config.enableProtected)
        vertCreateFlags |= VK_BUFFER_CREATE_PROTECTED_BIT;
    if (m_config.enableSparseBinding)
        vertCreateFlags |= VK_BUFFER_CREATE_SPARSE_BINDING_BIT;
    const VkBufferUsageFlags vertBuffUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    const MemoryRequirement vertMemReqs =
        (m_config.enableProtected ? MemoryRequirement::Protected : MemoryRequirement::Any);
    const VkBufferCreateInfo vertBuffInfo =
        makeBufferCreateInfo(positionsBuffer.getSize(), vertBuffUsage, producerIndices, vertCreateFlags);
    const BufferWithMemory vertexBuffer(vki, vkd, phys, device, allocator, vertBuffInfo, vertMemReqs, producerQueue);
    const VkDescriptorBufferInfo vertDsBuffInfo =
        makeDescriptorBufferInfo(vertexBuffer.get(), 0ull, vertexBuffer.getSize());
    const VkBufferMemoryBarrier producerReadyBarrier =
        makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, vertexBuffer.get(), 0,
                                vertexBuffer.getSize(), producerIndex, producerIndex);

    // helper buffer for the protected memory variant
    const uint32_t helperBuffSize = (uint32_t)(2 * sizeof(uint32_t));
    const VkBufferCreateInfo helperBuffInfo =
        makeBufferCreateInfo(helperBuffSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, helperIndices,
                             m_config.enableProtected ? VK_BUFFER_CREATE_PROTECTED_BIT : 0);
    BufferWithMemory helperBuffer(vki, vkd, phys, device, allocator, helperBuffInfo,
                                  m_config.enableProtected ? MemoryRequirement::Protected : MemoryRequirement::Any);
    const VkDescriptorBufferInfo helperDsBuffInfo = makeDescriptorBufferInfo(helperBuffer.get(), 0ull, helperBuffSize);

    // descriptor set for stagging and vertex buffers
    Move<VkDescriptorPool> producerDsPool =
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    Move<VkDescriptorSetLayout> producerDsLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL)
            .build(vkd, device);
    Move<VkDescriptorSet> producerDs = makeDescriptorSet(vkd, device, *producerDsPool, *producerDsLayout);
    DescriptorSetUpdateBuilder()
        .writeSingle(*producerDs, DescriptorSetUpdateBuilder::Location::binding(0), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     &posDsBuffInfo)
        .writeSingle(*producerDs, DescriptorSetUpdateBuilder::Location::binding(1), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     &vertDsBuffInfo)
        .writeSingle(*producerDs, DescriptorSetUpdateBuilder::Location::binding(2), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     &helperDsBuffInfo)
        .update(vkd, device);

    // producer image
    const uint32_t clearComp      = 97;
    const VkClearValue clearColor = makeClearValueColorU32(clearComp, clearComp, clearComp, clearComp);
    VkImageSubresourceRange imageResourceRange{};
    const VkImageUsageFlags imageUsage = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
    de::MovePtr<ImageWithMemory> image = createImage(imageUsage, producerIndex, producerQueue);
    Move<VkImageView> view             = createView(**image, imageResourceRange);
    Move<VkRenderPass> renderPass      = makeRenderPass(vkd, device, m_config.format);
    Move<VkFramebuffer> framebuffer = makeFramebuffer(vkd, device, *renderPass, *view, m_config.width, m_config.height);
    const VkDescriptorImageInfo imageDsInfo = makeDescriptorImageInfo(VK_NULL_HANDLE, *view, VK_IMAGE_LAYOUT_GENERAL);
    const VkImageMemoryBarrier imageReadyBarrier = makeImageMemoryBarrier(
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL, **image, imageResourceRange, producerIndex, producerIndex);

    // stagging buffer for result
    const VkDeviceSize resultBufferSize =
        (m_config.width * m_config.height * mapVkFormat(m_config.format).getPixelSize());
    const VkBufferCreateInfo resultBufferInfo =
        makeBufferCreateInfo(resultBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, consumerIndices);
    BufferWithMemory resultBuffer(vki, vkd, phys, device, allocator, resultBufferInfo, MemoryRequirement::HostVisible);
    const VkDescriptorBufferInfo resultDsBuffInfo =
        makeDescriptorBufferInfo(resultBuffer.get(), 0ull, resultBufferSize);
    const VkBufferMemoryBarrier resultReadyBarrier =
        makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, resultBuffer.get(), 0,
                                resultBufferSize, consumerIndex, consumerIndex);

    // descriptor set for consumer image and result buffer
    Move<VkDescriptorPool> consumerDsPool =
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    Move<VkDescriptorSetLayout> consumerDsLayout =
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL)
            .build(vkd, device);
    Move<VkDescriptorSet> consumerDs = makeDescriptorSet(vkd, device, *consumerDsPool, *consumerDsLayout);

    DescriptorSetUpdateBuilder()
        .writeSingle(*consumerDs, DescriptorSetUpdateBuilder::Location::binding(0), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                     &imageDsInfo)
        .writeSingle(*consumerDs, DescriptorSetUpdateBuilder::Location::binding(1), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                     m_config.enableProtected ? &helperDsBuffInfo : &resultDsBuffInfo)
        .update(vkd, device);

    Move<VkPipelineLayout> producer1Layout = createPipelineLayout<>({*producerDsLayout});
    Move<VkPipeline> producer1Pipeline     = createComputePipeline(*producer1Layout, true);
    Move<VkPipelineLayout> producer2Layout = createPipelineLayout<>({});
    Move<VkPipeline> producer2Pipeline     = createGraphicsPipeline(*producer2Layout, *renderPass);

    Move<VkPipelineLayout> consumerLayout = createPipelineLayout<>({*consumerDsLayout});
    Move<VkPipeline> consumerPipeline     = createComputePipeline(*consumerLayout, false);

    Move<VkCommandPool> producerPool = makeCommandPool(producerIndex);
    Move<VkCommandPool> consumerPool = makeCommandPool(consumerIndex);
    Move<VkCommandBuffer> producerCmd =
        allocateCommandBuffer(vkd, device, *producerPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    Move<VkCommandBuffer> consumerCmd =
        allocateCommandBuffer(vkd, device, *consumerPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vkd, *producerCmd);
    vkd.cmdBindPipeline(*producerCmd, VK_PIPELINE_BIND_POINT_COMPUTE, *producer1Pipeline);
    vkd.cmdBindPipeline(*producerCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *producer2Pipeline);
    vkd.cmdBindVertexBuffers(*producerCmd, 0, 1, vertexBuffer.getPtr(), &static_cast<const VkDeviceSize &>(0));
    vkd.cmdBindDescriptorSets(*producerCmd, VK_PIPELINE_BIND_POINT_COMPUTE, *producer1Layout, 0, 1, &producerDs.get(),
                              0, nullptr);
    vkd.cmdDispatch(*producerCmd, uint32_t(positions.size()), 1, 1);
    vkd.cmdPipelineBarrier(*producerCmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0,
                           nullptr, 1, &producerReadyBarrier, 0, nullptr);
    beginRenderPass(vkd, *producerCmd, *renderPass, *framebuffer, makeRect2D(m_config.width, m_config.height),
                    clearColor);
    vkd.cmdDraw(*producerCmd, uint32_t(positions.size()), 1, 0, 0);
    endRenderPass(vkd, *producerCmd);
    vkd.cmdPipelineBarrier(*producerCmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0u, nullptr, 0u, nullptr, 1u, &imageReadyBarrier);
    endCommandBuffer(vkd, *producerCmd);

    beginCommandBuffer(vkd, *consumerCmd);
    vkd.cmdBindPipeline(*consumerCmd, VK_PIPELINE_BIND_POINT_COMPUTE, *consumerPipeline);
    vkd.cmdBindDescriptorSets(*consumerCmd, VK_PIPELINE_BIND_POINT_COMPUTE, *consumerLayout, 0, 1, &consumerDs.get(), 0,
                              nullptr);
    vkd.cmdDispatch(*consumerCmd, m_config.width, m_config.height, 1);
    vkd.cmdPipelineBarrier(*consumerCmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,
                           nullptr, 1, &resultReadyBarrier, 0, nullptr);
    endCommandBuffer(vkd, *consumerCmd);

    bool submitSuccess = submitCommands(*producerCmd, *consumerCmd);
    resultBuffer.invalidateAlloc(vkd, device);

    // For protected memory variant, we cannot actually get any information about the memory. If the shader runs to
    // completion it is a pass, if it loops indefinitely it's a fail.
    if (m_config.enableProtected)
    {
        return (submitSuccess ? tcu::TestStatus::pass("Validation compute shader ran successfully") :
                                tcu::TestStatus::fail("Validation compute shader failed to run to completion"));
    }
    else
    {
        const tcu::ConstPixelBufferAccess resultBufferAccess(mapVkFormat(m_config.format), m_config.width,
                                                             m_config.height, 1, resultBuffer.getHostPtr());
        const uint32_t resultValue   = resultBufferAccess.getPixelUint(0, 0).x();
        const uint32_t expectedValue = 1;
        const bool ok                = (resultValue == expectedValue);
        if (!ok)
        {
            m_context.getTestContext().getLog() << tcu::TestLog::Message << "Expected value: " << expectedValue
                                                << ", got " << resultValue << tcu::TestLog::EndMessage;
        }

        return ok ? tcu::TestStatus::pass("") : tcu::TestStatus::fail("");
    }
}

// Best-effort attempt at triggering task preemption. Create a large workload and a small one. Submit the large workload
// to a lower-priority queue and the small workload to a higher-priority queue. Verify both jobs complete successfully.
class PreemptionInstance : public vkt::TestInstance
{
public:
    enum class QueueType
    {
        GRAPHICS = 0,
        COMPUTE,
        COMPUTE_EXCLUSIVE,
        TRANSFER,
        TRANSFER_EXCLUSIVE,
    };

    static constexpr uint32_t kLocalSize = 32u;

    struct Params
    {
        QueueType queueA;
        VkQueueGlobalPriorityKHR priorityA;

        QueueType queueB;
        VkQueueGlobalPriorityKHR priorityB;

        bool doublePreemption;

        bool needsGraphics() const
        {
            return anyQueueNeeds(QueueType::GRAPHICS);
        }

        bool needsCompute() const
        {
            return anyQueueNeeds(QueueType::COMPUTE) || anyQueueNeeds(QueueType::COMPUTE_EXCLUSIVE);
        }

        bool needsTransfer() const
        {
            return anyQueueNeeds(QueueType::TRANSFER) || anyQueueNeeds(QueueType::TRANSFER_EXCLUSIVE);
        }

    protected:
        bool anyQueueNeeds(QueueType queueType) const
        {
            return (queueA == queueType || queueB == queueType);
        }
    };

    PreemptionInstance(Context &context, const Params &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~PreemptionInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const Params m_params;
};

class PreemptionCase : public vkt::TestCase
{
public:
    PreemptionCase(tcu::TestContext &testCtx, const std::string &name, const PreemptionInstance::Params &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~PreemptionCase(void) = default;

    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;

protected:
    const PreemptionInstance::Params m_params;
};

TestInstance *PreemptionCase::createInstance(Context &context) const
{
    return new PreemptionInstance(context, m_params);
}

void PreemptionCase::initPrograms(vk::SourceCollections &programCollection) const
{
    if (m_params.needsGraphics())
    {
        std::ostringstream vert;
        vert << "#version 460\n"
             << "layout (location=0) in vec4 inPos;\n"
             << "void main(void) {\n"
             << "    gl_Position = inPos;\n"
             << "    gl_PointSize = 1.0;\n"
             << "}\n";
        programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

        std::ostringstream frag;
        frag << "#version 460\n"
             << "layout (location=0) out vec4 outColor;\n"
             << "layout (push_constant, std430) uniform PCBlock {\n"
             << "    uint width;\n"
             << "    uint height;\n"
             << "} pc;\n"
             << "void main(void) {\n"
             << "    const float green = gl_FragCoord.x / float(pc.width);\n"
             << "    const float blue  = gl_FragCoord.y / float(pc.height);\n"
             << "    outColor = vec4(0.0, green, blue, 1.0);\n"
             << "}\n";
        programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
    }

    if (m_params.needsCompute())
    {
        std::ostringstream comp;
        comp << "#version 460\n"
             << "layout (local_size_x=" << PreemptionInstance::kLocalSize << ") in;\n"
             << "layout (set=0, binding=0, std430) buffer OutputBlock {\n"
             << "    uint values[];\n"
             << "} ob;\n"
             << "\n"
             << "uint getWorkGroupSize (void) {\n"
             << "    const uint workGroupSize = gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z;\n"
             << "    return workGroupSize;\n"
             << "}\n"
             << "\n"
             << "uint getWorkGroupIndex (void) {\n"
             << "    const uint workGroupIndex = gl_NumWorkGroups.x * gl_NumWorkGroups.y * gl_WorkGroupID.z +\n"
             << "                                gl_NumWorkGroups.x * gl_WorkGroupID.y +\n"
             << "                                gl_WorkGroupID.x;\n"
             << "    return workGroupIndex;\n"
             << "}\n"
             << "\n"
             << "uint getGlobalInvocationIndex (void) {\n"
             << "    const uint globalInvocationIndex = getWorkGroupIndex() * getWorkGroupSize() + "
                "gl_LocalInvocationIndex;\n"
             << "    return globalInvocationIndex;\n"
             << "}\n"
             << "\n"
             << "void main(void) {\n"
             << "    const uint index = getGlobalInvocationIndex();\n"
             << "    ob.values[index] = index;\n"
             << "}\n";
        programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
    }
}

tcu::Maybe<uint32_t> findQueueByTypeAndPriority(const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                                PreemptionInstance::QueueType queueType,
                                                VkQueueGlobalPriorityKHR priority)
{
    std::vector<VkQueueFamilyProperties2> qfProperties2;
    std::vector<VkQueueFamilyGlobalPriorityPropertiesKHR> qfGlobalPriorities;

    uint32_t queueFamilyPropertyCount = 0u;
    vki.getPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyPropertyCount, nullptr);

    if (queueFamilyPropertyCount == 0u)
        TCU_FAIL("queueFamilyPropertyCount is zero");

    // Resize and initialize arrays setting pNext properly.
    qfProperties2.resize(queueFamilyPropertyCount);
    qfGlobalPriorities.resize(queueFamilyPropertyCount);

    for (uint32_t i = 0u; i < queueFamilyPropertyCount; ++i)
    {
        qfGlobalPriorities.at(i) = initVulkanStructure();
        qfProperties2.at(i)      = initVulkanStructure(&qfGlobalPriorities.at(i));
    }

    vki.getPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyPropertyCount,
                                                de::dataOrNull(qfProperties2));

    bool found       = false;
    uint32_t qfIndex = 0u;

    for (qfIndex = 0u; qfIndex < queueFamilyPropertyCount; ++qfIndex)
    {
        bool skip              = true;
        const auto &properties = qfProperties2.at(qfIndex);

        if (queueType == PreemptionInstance::QueueType::GRAPHICS)
        {
            if ((properties.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u)
                skip = false;
        }
        else if (queueType == PreemptionInstance::QueueType::COMPUTE)
        {
            if ((properties.queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0u)
                skip = false;
        }
        else if (queueType == PreemptionInstance::QueueType::COMPUTE_EXCLUSIVE)
        {
            if ((properties.queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0u &&
                (properties.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0u)
                skip = false;
        }
        else if (queueType == PreemptionInstance::QueueType::TRANSFER)
        {
            if ((properties.queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0u)
                skip = false;
        }
        else if (queueType == PreemptionInstance::QueueType::TRANSFER_EXCLUSIVE)
        {
            if ((properties.queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0u &&
                (properties.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0u &&
                (properties.queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0u)
                skip = false;
        }
        else
            DE_ASSERT(false);

        if (skip)
            continue;

        const auto &priorities = qfGlobalPriorities.at(qfIndex);
        for (uint32_t priorityIdx = 0u; priorityIdx < priorities.priorityCount; ++priorityIdx)
        {
            if (priorities.priorities[priorityIdx] == priority)
            {
                found = true;
                break;
            }
        }

        if (found)
            break;
    }

    if (found)
        return tcu::just(qfIndex);
    return tcu::Nothing;
}

std::string getQueueTypeName(PreemptionInstance::QueueType queueType)
{
    static const std::map<PreemptionInstance::QueueType, std::string> queueTypeNames{
        std::make_pair(PreemptionInstance::QueueType::GRAPHICS, std::string("graphics")),
        std::make_pair(PreemptionInstance::QueueType::COMPUTE, std::string("compute")),
        std::make_pair(PreemptionInstance::QueueType::COMPUTE_EXCLUSIVE, std::string("exclusive-compute")),
        std::make_pair(PreemptionInstance::QueueType::TRANSFER, std::string("transfer")),
        std::make_pair(PreemptionInstance::QueueType::TRANSFER_EXCLUSIVE, std::string("exclusive-transfer")),
    };

    const auto capabilityIter = queueTypeNames.find(queueType);
    DE_ASSERT(capabilityIter != queueTypeNames.end());

    return capabilityIter->second;
}

std::string getPriorityName(VkQueueGlobalPriorityKHR priority)
{
    static const std::map<VkQueueGlobalPriorityKHR, std::string> priorityNames{
        std::make_pair(VK_QUEUE_GLOBAL_PRIORITY_LOW_KHR, std::string("low")),
        std::make_pair(VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_KHR, std::string("medium")),
        std::make_pair(VK_QUEUE_GLOBAL_PRIORITY_HIGH_KHR, std::string("high")),
        std::make_pair(VK_QUEUE_GLOBAL_PRIORITY_REALTIME_KHR, std::string("realtime")),
    };

    const auto priorityNameIter = priorityNames.find(priority);
    DE_ASSERT(priorityNameIter != priorityNames.end());

    return priorityNameIter->second;
}

void throwNotSupported(PreemptionInstance::QueueType queueType, VkQueueGlobalPriorityKHR priority)
{
    const auto queueTypeName = getQueueTypeName(queueType);
    const auto priorityName  = getPriorityName(priority);

    std::ostringstream msg;
    msg << "Unable to find queue supporting " << queueTypeName << " and priority " << priorityName;
    TCU_THROW(NotSupportedError, msg.str());
}

void PreemptionCase::checkSupport(Context &context) const
{
    context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");
    context.requireDeviceFunctionality("VK_KHR_global_priority");

    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    auto resultA = findQueueByTypeAndPriority(vki, physicalDevice, m_params.queueA, m_params.priorityA);
    if (!static_cast<bool>(resultA))
        throwNotSupported(m_params.queueA, m_params.priorityA);

    auto resultB = findQueueByTypeAndPriority(vki, physicalDevice, m_params.queueB, m_params.priorityB);
    if (!static_cast<bool>(resultB))
        throwNotSupported(m_params.queueB, m_params.priorityB);
}

class DeviceHelper
{
protected:
    CustomInstance m_customInstance;
    VkPhysicalDevice m_physicalDevice;
    uint32_t m_qfIndex;
    Move<VkDevice> m_customDevice;
    std::unique_ptr<DeviceInterface> m_vkd;
    std::unique_ptr<SimpleAllocator> m_allocator;
    VkQueue m_queue;

    DeviceHelper()
        : m_customInstance()
        , m_physicalDevice(VK_NULL_HANDLE)
        , m_qfIndex(~0u)
        , m_customDevice()
        , m_vkd()
        , m_allocator()
        , m_queue(VK_NULL_HANDLE)
    {
    }

public:
    DeviceHelper(Context &context, PreemptionInstance::QueueType queueType, VkQueueGlobalPriorityKHR priority)
        : DeviceHelper()
    {
        const float numericPriority = 1.0f; // This is the classic priority.

        const VkDeviceQueueGlobalPriorityCreateInfoKHR queuePriority = {
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_KHR,
            nullptr,
            priority,
        };

        VkDeviceQueueCreateInfo queueCreateInfo = {
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            &queuePriority,
            0u,
            this->m_qfIndex, // Bad value from the default constructor.
            1u,
            &numericPriority,
        };

        const auto &features = context.getDeviceFeatures();

        const std::vector<const char *> extensions{
            "VK_KHR_global_priority",
        };

        const VkDeviceCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            nullptr,
            0u,
            1u,
            &queueCreateInfo,
            0u,
            nullptr,
            de::sizeU32(extensions),
            de::dataOrNull(extensions),
            &features,
        };

        const auto apiVersion = context.getUsedApiVersion();
        const auto &cmdLine   = context.getTestContext().getCommandLine();

        auto instance   = createCustomInstanceWithExtensions(context, context.getInstanceExtensions());
        const auto &vki = instance.getDriver();

        uint32_t physicalDeviceCount = 0u;
        VK_CHECK(vki.enumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr));
        DE_ASSERT(physicalDeviceCount > 0u);

        std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount, VK_NULL_HANDLE);
        VK_CHECK(vki.enumeratePhysicalDevices(instance, &physicalDeviceCount, de::dataOrNull(physicalDevices)));
        const auto physDev = physicalDevices.at(cmdLine.getVKDeviceId() - 1);

        const auto qfIndexMaybe = findQueueByTypeAndPriority(vki, physDev, queueType, priority);
        DE_ASSERT(static_cast<bool>(qfIndexMaybe));
        const auto queueFamilyIndex = qfIndexMaybe.get();

        // Overwrite bad value.
        queueCreateInfo.queueFamilyIndex = queueFamilyIndex;

        const auto validationEnabled = context.getTestContext().getCommandLine().isValidationEnabled();
        const auto &vkp              = context.getPlatformInterface();

        Move<VkDevice> device;
        try
        {
            device = createCustomDevice(validationEnabled, vkp, instance, instance.getDriver(), physDev, &createInfo);
        }
        catch (vk::Error &err)
        {
            const auto result = err.getError();
            if (result == VK_ERROR_NOT_PERMITTED_KHR || result == VK_ERROR_INITIALIZATION_FAILED)
            {
                std::ostringstream msg;
                msg << "Got " << result << " when attempting to create device";
                TCU_THROW(NotSupportedError, msg.str());
            }
            throw;
        }

        // Save created data.
        m_customInstance.swap(instance);
        m_physicalDevice = physDev;
        m_qfIndex        = queueFamilyIndex;
        m_customDevice   = device;
        m_vkd.reset(new DeviceDriver(vkp, m_customInstance, m_customDevice.get(), apiVersion, cmdLine));
        const auto memProperties = getPhysicalDeviceMemoryProperties(m_customInstance.getDriver(), m_physicalDevice);
        m_allocator.reset(new SimpleAllocator(*m_vkd, m_customDevice.get(), memProperties));
        m_queue = getDeviceQueue(*m_vkd, m_customDevice.get(), m_qfIndex, 0u);
    }

    const InstanceInterface &getInstanceInterface() const
    {
        return m_customInstance.getDriver();
    }
    VkInstance getInstance() const
    {
        return m_customInstance;
    } // Uses conversion operator.
    VkPhysicalDevice getPhysicalDevice() const
    {
        return m_physicalDevice;
    }
    uint32_t getQueueFamilyIndex() const
    {
        return m_qfIndex;
    }
    const DeviceInterface &getDeviceInterface() const
    {
        return *m_vkd;
    }
    VkDevice getDevice() const
    {
        return m_customDevice.get();
    }
    Allocator &getAllocator() const
    {
        return *m_allocator;
    }
    VkQueue getQueue() const
    {
        return m_queue;
    }
};

struct WorkLoadData
{
    Move<VkShaderModule> vertModule;
    Move<VkShaderModule> fragModule;
    Move<VkShaderModule> compModule;

    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> framebuffer;

    Move<VkDescriptorSetLayout> setLayout;
    Move<VkPipelineLayout> pipelineLayout;
    Move<VkPipeline> pipeline;

    Move<VkDescriptorPool> descriptorPool;
    Move<VkDescriptorSet> descriptorSet;

    Move<VkCommandPool> commandPool;
    Move<VkCommandBuffer> commandBuffer;

    WorkLoadData(PreemptionInstance::QueueType queueType, const DeviceInterface &vkd, VkDevice device, uint32_t qfIndex,
                 const BinaryCollection &binaries, VkFormat colorFormat, VkImage image, VkImageView imageView,
                 VkBuffer imageBuffer, const VkExtent3D &extent, VkBuffer vertexBuffer, VkBuffer compOutputBuffer,
                 VkBuffer transferInBuffer, VkBuffer transferOutBuffer)
    {
        WorkLoadData &data = *this;

        const bool graphics = (queueType == PreemptionInstance::QueueType::GRAPHICS);
        const bool compute  = (queueType == PreemptionInstance::QueueType::COMPUTE ||
                              queueType == PreemptionInstance::QueueType::COMPUTE_EXCLUSIVE);
        const bool transfer = (queueType == PreemptionInstance::QueueType::TRANSFER ||
                               queueType == PreemptionInstance::QueueType::TRANSFER_EXCLUSIVE);

        const auto graphicsPCSize   = static_cast<uint32_t>(sizeof(tcu::UVec2)); // Must match frag shader.
        const auto graphicsPCStages = VK_SHADER_STAGE_FRAGMENT_BIT;
        const auto graphicsPCRange  = makePushConstantRange(graphicsPCStages, 0u, graphicsPCSize);

        if (graphics)
        {
            data.vertModule = createShaderModule(vkd, device, binaries.get("vert"));
            data.fragModule = createShaderModule(vkd, device, binaries.get("frag"));

            data.renderPass  = makeRenderPass(vkd, device, colorFormat);
            data.framebuffer = makeFramebuffer(vkd, device, *data.renderPass, imageView, extent.width, extent.height);

            const std::vector<VkViewport> viewports(1u, makeViewport(extent));
            const std::vector<VkRect2D> scissors(1u, makeRect2D(extent));

            data.pipelineLayout = makePipelineLayout(vkd, device, VK_NULL_HANDLE, &graphicsPCRange);

            data.pipeline = makeGraphicsPipeline(vkd, device, *data.pipelineLayout, *data.vertModule, VK_NULL_HANDLE,
                                                 VK_NULL_HANDLE, VK_NULL_HANDLE, *data.fragModule, *data.renderPass,
                                                 viewports, scissors, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
        }
        else if (compute)
        {
            DescriptorSetLayoutBuilder setLayoutBuilder;
            setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
            data.setLayout      = setLayoutBuilder.build(vkd, device);
            data.pipelineLayout = makePipelineLayout(vkd, device, *data.setLayout);

            data.compModule = createShaderModule(vkd, device, binaries.get("comp"));
            data.pipeline   = makeComputePipeline(vkd, device, *data.pipelineLayout, *data.compModule);

            DescriptorPoolBuilder poolBuilder;
            poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            data.descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
            data.descriptorSet  = makeDescriptorSet(vkd, device, *data.descriptorPool, *data.setLayout);

            DescriptorSetUpdateBuilder updateBuilder;
            DE_ASSERT(compOutputBuffer != VK_NULL_HANDLE);
            const auto descInfo = makeDescriptorBufferInfo(compOutputBuffer, 0ull, VK_WHOLE_SIZE);
            updateBuilder.writeSingle(*data.descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                                      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descInfo);
            updateBuilder.update(vkd, device);
        }
        else if (transfer)
        {
            // No need for pipelines, descriptor sets, etc.
        }
        else
            DE_ASSERT(false);

        data.commandPool     = makeCommandPool(vkd, device, qfIndex);
        data.commandBuffer   = allocateCommandBuffer(vkd, device, *data.commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        const auto cmdBuffer = *data.commandBuffer;

        beginCommandBuffer(vkd, cmdBuffer, 0u);

        if (graphics)
        {
            const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
            const tcu::UVec2 extentVec(extent.width, extent.height);
            const auto copyExtent                 = extentVec.asInt();
            const VkDeviceSize vertexBufferOffset = 0ull;
            const auto vertexCount                = extent.width * extent.height * extent.depth;

            beginRenderPass(vkd, cmdBuffer, *data.renderPass, *data.framebuffer, makeRect2D(extent), clearColor);
            vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *data.pipeline);
            DE_ASSERT(vertexBuffer != VK_NULL_HANDLE);
            vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);
            vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0u, graphicsPCSize,
                                 &extentVec);
            vkd.cmdDraw(cmdBuffer, vertexCount, 1u, 0u, 0u);
            endRenderPass(vkd, cmdBuffer);
            copyImageToBuffer(vkd, cmdBuffer, image, imageBuffer, copyExtent);
        }
        else if (compute)
        {
            const auto totalInvocations = extent.width * extent.height * extent.depth;
            DE_ASSERT(totalInvocations % PreemptionInstance::kLocalSize == 0u);
            const auto wgCount = totalInvocations / PreemptionInstance::kLocalSize;

            vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *data.pipelineLayout, 0u, 1u,
                                      &data.descriptorSet.get(), 0u, nullptr);
            vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *data.pipeline);
            vkd.cmdDispatch(cmdBuffer, wgCount, 1u, 1u);

            const auto preHostBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                     &preHostBarrier);
        }
        else if (transfer)
        {
            DE_ASSERT(transferInBuffer != VK_NULL_HANDLE);
            DE_ASSERT(transferOutBuffer != VK_NULL_HANDLE);

            const auto transferCount = extent.height * extent.depth;
            const auto stride        = static_cast<VkDeviceSize>(sizeof(uint32_t) * extent.width);

            for (uint32_t i = 0u; i < transferCount; ++i)
            {
                const auto offset     = i * stride;
                const auto copyRegion = makeBufferCopy(offset, offset, stride);
                vkd.cmdCopyBuffer(cmdBuffer, transferInBuffer, transferOutBuffer, 1u, &copyRegion);
            }
        }
        else
            DE_ASSERT(false);

        endCommandBuffer(vkd, cmdBuffer);
    }
};

using ImageWithBufferPtr  = std::unique_ptr<vk::ImageWithBuffer>;
using BufferWithMemoryPtr = std::unique_ptr<vk::BufferWithMemory>;

BufferWithMemoryPtr makeBlankBuffer(const DeviceInterface &vkd, VkDevice dev, Allocator &allocator,
                                    const VkBufferCreateInfo &createInfo, const MemoryRequirement &memReq)
{
    BufferWithMemoryPtr buffer(new vk::BufferWithMemory(vkd, dev, allocator, createInfo, memReq));

    auto &allocation = buffer->getAllocation();
    void *dataPtr    = allocation.getHostPtr();
    deMemset(dataPtr, 0, static_cast<size_t>(createInfo.size));

    return buffer;
}

uint32_t getTransferValueOffset()
{
    return 1000u;
}

BufferWithMemoryPtr makePrefilledBuffer(const DeviceInterface &vkd, VkDevice dev, Allocator &allocator,
                                        const VkBufferCreateInfo &createInfo, const MemoryRequirement &memReq)
{
    const uint32_t valueOffset   = getTransferValueOffset();
    const VkDeviceSize valueSize = static_cast<VkDeviceSize>(sizeof(uint32_t));

    DE_ASSERT(createInfo.size % valueSize == 0ull);
    const auto itemCount = createInfo.size / valueSize;
    std::vector<uint32_t> values(static_cast<size_t>(itemCount), 0u);
    std::iota(begin(values), end(values), valueOffset);

    BufferWithMemoryPtr buffer(new vk::BufferWithMemory(vkd, dev, allocator, createInfo, memReq));
    auto &allocation = buffer->getAllocation();
    void *dataPtr    = allocation.getHostPtr();
    deMemcpy(dataPtr, de::dataOrNull(values), de::dataSize(values));

    return buffer;
}

// Creates a vertex buffer with coordinates for one point per pixel.
BufferWithMemoryPtr makeVertexBuffer(const DeviceInterface &vkd, VkDevice dev, Allocator &allocator,
                                     const tcu::IVec3 &extent)
{
    DE_ASSERT(extent.z() == 1);

    const auto extentU    = extent.asUint();
    const auto extentF    = extent.asFloat();
    const auto pixelCount = extentU.x() * extentU.y();

    std::vector<tcu::Vec4> vertices;
    vertices.reserve(pixelCount);

    for (int y = 0; y < extent.y(); ++y)
        for (int x = 0; x < extent.x(); ++x)
        {
            const float xCoord = ((static_cast<float>(x) + 0.5f) / extentF.x()) * 2.0f - 1.0f;
            const float yCoord = ((static_cast<float>(y) + 0.5f) / extentF.y()) * 2.0f - 1.0f;
            vertices.emplace_back(xCoord, yCoord, 0.0f, 1.0f);
        }

    const auto bufferSize  = de::dataSize(vertices);
    const auto bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    const auto memReqs     = MemoryRequirement::HostVisible;
    const auto createInfo  = makeBufferCreateInfo(bufferSize, bufferUsage);
    BufferWithMemoryPtr buffer(new vk::BufferWithMemory(vkd, dev, allocator, createInfo, memReqs));

    auto &allocation = buffer->getAllocation();
    void *dataPtr    = allocation.getHostPtr();
    deMemcpy(dataPtr, de::dataOrNull(vertices), bufferSize);

    return buffer;
}

bool verifyIncreasingValues(tcu::TestLog &log, const std::string &bufferName, const vk::BufferWithMemory &buffer,
                            VkDeviceSize size, uint32_t valueOffset)
{
    const auto itemSize = sizeof(uint32_t);
    const auto sizeSz   = static_cast<size_t>(size);

    DE_ASSERT(sizeSz % itemSize == 0u);
    const auto itemCount = sizeSz / itemSize;

    std::vector<uint32_t> items(itemCount, 0u);
    auto &allocation = buffer.getAllocation();
    void *dataPtr    = allocation.getHostPtr();

    deMemcpy(de::dataOrNull(items), dataPtr, sizeSz);

    bool good = true;
    for (size_t i = 0u; i < itemCount; ++i)
    {
        const auto &result  = items.at(i);
        const auto expected = valueOffset + i;

        if (result != expected)
        {
            log << tcu::TestLog::Message << "Unexpected value in buffer " << bufferName << " item " << i
                << ": expected " << expected << " but found " << result;
            good = false;
        }
    }
    return good;
}

bool verifyGradient(tcu::TestLog &log, const std::string &imageName, const VkFormat format, vk::ImageWithBuffer &image,
                    const tcu::IVec3 &extent)
{
    const auto tcuFormat   = mapVkFormat(format);
    const auto floatExtent = extent.asFloat();

    tcu::TextureLevel referenceLevel(tcuFormat, extent.x(), extent.y(), extent.z());
    tcu::PixelBufferAccess referenceAccess = referenceLevel.getAccess();

    for (int y = 0; y < extent.y(); ++y)
        for (int x = 0; x < extent.x(); ++x)
        {
            const float green = (static_cast<float>(x) + 0.5f) / floatExtent.x();
            const float blue  = (static_cast<float>(y) + 0.5f) / floatExtent.y();
            const tcu::Vec4 color(0.0f, green, blue, 1.0f);

            referenceAccess.setPixel(color, x, y);
        }

    const tcu::ConstPixelBufferAccess resultAccess(tcuFormat, extent, image.getBufferAllocation().getHostPtr());

    DE_ASSERT(format == VK_FORMAT_R8G8B8A8_UNORM); // Otherwise thresholds would have to change.
    const float threshold = 0.005f;                // 1/255 < 0.005 < 2/255
    const tcu::Vec4 thresholdVec(0.0f, threshold, threshold, 0.0f);

    return tcu::floatThresholdCompare(log, imageName.c_str(), "", referenceAccess, resultAccess, thresholdVec,
                                      tcu::COMPARE_LOG_ON_ERROR);
}

tcu::TestStatus PreemptionInstance::iterate(void)
{
    // A is always the big workload device, and B the small workload device.
    DeviceHelper deviceA(m_context, m_params.queueA, m_params.priorityA);
    DeviceHelper deviceB(m_context, m_params.queueB, m_params.priorityB);

    const auto &vkdA = deviceA.getDeviceInterface();
    const auto &vkdB = deviceB.getDeviceInterface();

    const auto devA = deviceA.getDevice();
    const auto devB = deviceB.getDevice();

    auto &allocatorA = deviceA.getAllocator();
    auto &allocatorB = deviceB.getAllocator();

    const auto qfIndexA = deviceA.getQueueFamilyIndex();
    const auto qfIndexB = deviceB.getQueueFamilyIndex();

    const auto queueA = deviceA.getQueue();
    const auto queueB = deviceB.getQueue();

    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    const tcu::IVec3 largeExtent(512, 512, 1);
    const tcu::IVec3 smallExtent(8, 8, 1);

    const auto largeApiExtent = makeExtent3D(largeExtent);
    const auto smallApiExtent = makeExtent3D(smallExtent);

    const auto largeItemCount = largeApiExtent.width * largeApiExtent.height * largeApiExtent.depth;
    const auto smallItemCount = smallApiExtent.width * smallApiExtent.height * smallApiExtent.depth;

    const auto largeBufferSize = static_cast<VkDeviceSize>(largeItemCount * sizeof(uint32_t));
    const auto smallBufferSize = static_cast<VkDeviceSize>(smallItemCount * sizeof(uint32_t));

    const auto largeCompBufferCreateInfo = makeBufferCreateInfo(largeBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const auto smallCompBufferCreateInfo = makeBufferCreateInfo(smallBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    const auto largeXferBufferCreateInfo =
        makeBufferCreateInfo(largeBufferSize, (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT));
    const auto smallXferBufferCreateInfo =
        makeBufferCreateInfo(smallBufferSize, (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT));

    ImageWithBufferPtr imageA;
    ImageWithBufferPtr imageB;

    BufferWithMemoryPtr compOutBufferA;
    BufferWithMemoryPtr compOutBufferB;

    BufferWithMemoryPtr transferInBufferA;
    BufferWithMemoryPtr transferInBufferB;

    BufferWithMemoryPtr transferOutBufferA;
    BufferWithMemoryPtr transferOutBufferB;

    BufferWithMemoryPtr vertBufferA;
    BufferWithMemoryPtr vertBufferB;

    const bool graphicsA = (m_params.queueA == QueueType::GRAPHICS);
    const bool graphicsB = (m_params.queueB == QueueType::GRAPHICS);

    const bool computeA = (m_params.queueA == QueueType::COMPUTE || m_params.queueA == QueueType::COMPUTE_EXCLUSIVE);
    const bool computeB = (m_params.queueB == QueueType::COMPUTE || m_params.queueB == QueueType::COMPUTE_EXCLUSIVE);

    const bool transferA = (m_params.queueA == QueueType::TRANSFER || m_params.queueA == QueueType::TRANSFER_EXCLUSIVE);
    const bool transferB = (m_params.queueB == QueueType::TRANSFER || m_params.queueB == QueueType::TRANSFER_EXCLUSIVE);

    if (graphicsA)
    {
        imageA.reset(
            new vk::ImageWithBuffer(vkdA, devA, allocatorA, largeApiExtent, colorFormat, colorUsage, VK_IMAGE_TYPE_2D));
        vertBufferA = makeVertexBuffer(vkdA, devA, allocatorA, largeExtent);
    }
    else if (computeA)
        compOutBufferA =
            makeBlankBuffer(vkdA, devA, allocatorA, largeCompBufferCreateInfo, MemoryRequirement::HostVisible);
    else if (transferA)
    {
        transferInBufferA =
            makePrefilledBuffer(vkdA, devA, allocatorA, largeXferBufferCreateInfo, MemoryRequirement::HostVisible);
        transferOutBufferA =
            makeBlankBuffer(vkdA, devA, allocatorA, largeXferBufferCreateInfo, MemoryRequirement::HostVisible);
    }
    else
        DE_ASSERT(false);

    if (graphicsB)
    {
        imageB.reset(
            new vk::ImageWithBuffer(vkdB, devB, allocatorB, smallApiExtent, colorFormat, colorUsage, VK_IMAGE_TYPE_2D));
        vertBufferB = makeVertexBuffer(vkdB, devB, allocatorB, smallExtent);
    }
    else if (computeB)
        compOutBufferB =
            makeBlankBuffer(vkdB, devB, allocatorB, smallCompBufferCreateInfo, MemoryRequirement::HostVisible);
    else if (transferB)
    {
        transferInBufferB =
            makePrefilledBuffer(vkdB, devB, allocatorB, smallXferBufferCreateInfo, MemoryRequirement::HostVisible);
        transferOutBufferB =
            makeBlankBuffer(vkdB, devB, allocatorB, smallXferBufferCreateInfo, MemoryRequirement::HostVisible);
    }
    else
        DE_ASSERT(false);

    const auto &binaries = m_context.getBinaryCollection();

    WorkLoadData wlDataA(
        m_params.queueA, vkdA, devA, qfIndexA, binaries, colorFormat, (imageA ? imageA->getImage() : VK_NULL_HANDLE),
        (imageA ? imageA->getImageView() : VK_NULL_HANDLE), (imageA ? imageA->getBuffer() : VK_NULL_HANDLE),
        largeApiExtent, (vertBufferA ? vertBufferA->get() : VK_NULL_HANDLE),
        (compOutBufferA ? compOutBufferA->get() : VK_NULL_HANDLE),
        (transferInBufferA ? transferInBufferA->get() : VK_NULL_HANDLE),
        (transferOutBufferA ? transferOutBufferA->get() : VK_NULL_HANDLE));

    WorkLoadData wlDataB(
        m_params.queueB, vkdB, devB, qfIndexB, binaries, colorFormat, (imageB ? imageB->getImage() : VK_NULL_HANDLE),
        (imageB ? imageB->getImageView() : VK_NULL_HANDLE), (imageB ? imageB->getBuffer() : VK_NULL_HANDLE),
        smallApiExtent, (vertBufferB ? vertBufferB->get() : VK_NULL_HANDLE),
        (compOutBufferB ? compOutBufferB->get() : VK_NULL_HANDLE),
        (transferInBufferB ? transferInBufferB->get() : VK_NULL_HANDLE),
        (transferOutBufferB ? transferOutBufferB->get() : VK_NULL_HANDLE));

    // Submit both workloads, with the large one first.
    const auto fenceA = submitCommands(vkdA, devA, queueA, *wlDataA.commandBuffer);
    const auto fenceB = submitCommands(vkdB, devB, queueB, *wlDataB.commandBuffer);

    const uint64_t infiniteTimeout = ~0ull;
    VK_CHECK(vkdB.waitForFences(devB, 1u, &fenceB.get(), VK_TRUE, infiniteTimeout));
    if (m_params.doublePreemption)
    {
        const auto newFenceB = submitCommands(vkdB, devB, queueB, *wlDataB.commandBuffer);
        VK_CHECK(vkdB.waitForFences(devB, 1u, &newFenceB.get(), VK_TRUE, infiniteTimeout));
    }
    VK_CHECK(vkdA.waitForFences(devA, 1u, &fenceA.get(), VK_TRUE, infiniteTimeout));

    // Verify output data.
    bool okGraphicsA = true;
    bool okCompA     = true;
    bool okXferA     = true;
    bool okGraphicsB = true;
    bool okCompB     = true;
    bool okXferB     = true;

    auto &log = m_context.getTestContext().getLog();

    if (compOutBufferA)
    {
        invalidateAlloc(vkdA, devA, compOutBufferA->getAllocation());
        okCompA = verifyIncreasingValues(log, "A", *compOutBufferA, largeBufferSize, 0u);
    }

    if (compOutBufferB)
    {
        invalidateAlloc(vkdB, devB, compOutBufferB->getAllocation());
        okCompB = verifyIncreasingValues(log, "B", *compOutBufferB, smallBufferSize, 0u);
    }

    if (imageA)
    {
        invalidateAlloc(vkdA, devA, imageA->getBufferAllocation());
        okGraphicsA = verifyGradient(log, "ImageA", colorFormat, *imageA, largeExtent);
    }

    if (imageB)
    {
        invalidateAlloc(vkdB, devB, imageB->getBufferAllocation());
        okGraphicsB = verifyGradient(log, "ImageB", colorFormat, *imageB, smallExtent);
    }

    if (transferOutBufferA)
    {
        invalidateAlloc(vkdA, devA, transferOutBufferA->getAllocation());
        okXferA = verifyIncreasingValues(log, "A", *transferOutBufferA, largeBufferSize, getTransferValueOffset());
    }

    if (transferOutBufferB)
    {
        invalidateAlloc(vkdB, devB, transferOutBufferB->getAllocation());
        okXferB = verifyIncreasingValues(log, "B", *transferOutBufferB, smallBufferSize, getTransferValueOffset());
    }

    if (!(okGraphicsA && okGraphicsB && okCompA && okCompB && okXferA && okXferB))
        return tcu::TestStatus::fail("Failed; check log for details");
    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createGlobalPriorityQueueTests(tcu::TestContext &testCtx)
{
    typedef std::pair<VkQueueFlagBits, const char *> TransitionItem;
    TransitionItem const transitions[]{
        {VK_QUEUE_GRAPHICS_BIT, "graphics"},
        {VK_QUEUE_COMPUTE_BIT, "compute"},
    };

    auto mkGroupName = [](const TransitionItem &from, const TransitionItem &to) -> std::string
    { return std::string("from_") + from.second + std::string("_to_") + to.second; };

    std::pair<VkQueueFlags, const char *> const modifiers[]{
        {0, "no_modifiers"}, {VK_QUEUE_SPARSE_BINDING_BIT, "sparse"}, {VK_QUEUE_PROTECTED_BIT, "protected"}};

    std::pair<VkQueueGlobalPriorityKHR, const char *> const prios[]{
        {VK_QUEUE_GLOBAL_PRIORITY_LOW_KHR, "low"},
        {VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_KHR, "medium"},
        {VK_QUEUE_GLOBAL_PRIORITY_HIGH_KHR, "high"},
        {VK_QUEUE_GLOBAL_PRIORITY_REALTIME_KHR, "realtime"},
    };

    std::pair<SyncType, const char *> const syncs[]{
        {SyncType::None, "no_sync"},
        {SyncType::Semaphore, "semaphore"},
    };

    const uint32_t dim0 = 34;
    const uint32_t dim1 = 25;
    bool swap           = true;

    auto rootGroup = new tcu::TestCaseGroup(testCtx, "global_priority_transition");

    for (const auto &prio : prios)
    {
        auto prioGroup = new tcu::TestCaseGroup(testCtx, prio.second);

        for (const auto &sync : syncs)
        {
            auto syncGroup = new tcu::TestCaseGroup(testCtx, sync.second);

            for (const auto &mod : modifiers)
            {
                auto modGroup = new tcu::TestCaseGroup(testCtx, mod.second);

                for (const auto &transitionFrom : transitions)
                {
                    for (const auto &transitionTo : transitions)
                    {
                        if (transitionFrom != transitionTo)
                        {
                            TestConfig cfg{};
                            cfg.transitionFrom      = transitionFrom.first;
                            cfg.transitionTo        = transitionTo.first;
                            cfg.priorityFrom        = prio.first;
                            cfg.priorityTo          = prio.first;
                            cfg.syncType            = sync.first;
                            cfg.enableProtected     = (mod.first & VK_QUEUE_PROTECTED_BIT) != 0;
                            cfg.enableSparseBinding = (mod.first & VK_QUEUE_SPARSE_BINDING_BIT) != 0;
                            // Note that format is changing in GPQCase::checkSupport(...)
                            cfg.format = VK_FORMAT_R32G32B32A32_SFLOAT;
                            cfg.width  = swap ? dim0 : dim1;
                            cfg.height = swap ? dim1 : dim0;

                            swap ^= true;

                            modGroup->addChild(new GPQCase(testCtx, mkGroupName(transitionFrom, transitionTo), cfg));
                        }
                    }
                }
                syncGroup->addChild(modGroup);
            }
            prioGroup->addChild(syncGroup);
        }
        rootGroup->addChild(prioGroup);
    }

    {
        de::MovePtr<tcu::TestCaseGroup> preemptionGroup(new tcu::TestCaseGroup(testCtx, "preemption"));

        constexpr auto GRAPHICS           = PreemptionInstance::QueueType::GRAPHICS;
        constexpr auto COMPUTE            = PreemptionInstance::QueueType::COMPUTE;
        constexpr auto COMPUTE_EXCLUSIVE  = PreemptionInstance::QueueType::COMPUTE_EXCLUSIVE;
        constexpr auto TRANSFER           = PreemptionInstance::QueueType::TRANSFER;
        constexpr auto TRANSFER_EXCLUSIVE = PreemptionInstance::QueueType::TRANSFER_EXCLUSIVE;

        constexpr auto PRIORITY_LOW      = VK_QUEUE_GLOBAL_PRIORITY_LOW_KHR;
        constexpr auto PRIORITY_MEDIUM   = VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_KHR;
        constexpr auto PRIORITY_HIGH     = VK_QUEUE_GLOBAL_PRIORITY_HIGH_KHR;
        constexpr auto PRIORITY_REALTIME = VK_QUEUE_GLOBAL_PRIORITY_REALTIME_KHR;

        using QueueTypeVec = std::vector<PreemptionInstance::QueueType>;
        using PriorityVec  = std::vector<VkQueueGlobalPriorityKHR>;

        const QueueTypeVec kQueueTypes{GRAPHICS, COMPUTE, COMPUTE_EXCLUSIVE, TRANSFER, TRANSFER_EXCLUSIVE};
        const PriorityVec kPriorities{PRIORITY_LOW, PRIORITY_MEDIUM, PRIORITY_HIGH, PRIORITY_REALTIME};

        for (const auto queueTypeA : kQueueTypes)
            for (const auto queueTypeB : kQueueTypes)
                for (const auto priorityA : kPriorities)
                    for (const auto priorityB : kPriorities)
                    {
                        // These variants would not cause preemption.
                        if (priorityA >= priorityB)
                            continue;

                        for (const auto doublePreemption : {false, true})
                        {

                            const PreemptionInstance::Params params{
                                queueTypeA, priorityA, queueTypeB, priorityB, doublePreemption,
                            };

                            const std::string testName =
                                getQueueTypeName(queueTypeA) + "_" + getPriorityName(priorityA) + "_to_" +
                                getQueueTypeName(queueTypeB) + "_" + getPriorityName(priorityB) +
                                (doublePreemption ? "_double_preemption" : "");

                            preemptionGroup->addChild(new PreemptionCase(testCtx, testName, params));
                        }
                    }

        rootGroup->addChild(preemptionGroup.release());
    }

    return rootGroup;
}

} // namespace synchronization
} // namespace vkt
