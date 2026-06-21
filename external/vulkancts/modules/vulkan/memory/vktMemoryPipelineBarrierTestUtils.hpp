#ifndef _VKTMEMORYPIPELINEBARRIERTESTUTILS_HPP
#define _VKTMEMORYPIPELINEBARRIERTESTUTILS_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Pipeline barrier tests - shared base utilities
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

#include "vktTestCase.hpp"

#include "vkDefs.hpp"
#include "vkPlatform.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkPrograms.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuMaybe.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuResultCollector.hpp"
#include "tcuTexture.hpp"
#include "tcuImageCompare.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deRandom.hpp"

#include "deInt32.h"
#include "deMath.h"
#include "deMemory.h"

#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace vkt
{
namespace memory
{
namespace pipelinebarrier
{

using tcu::Maybe;
using tcu::TestLog;

using de::MovePtr;

using std::map;
using std::pair;
using std::set;
using std::string;
using std::vector;

using tcu::ConstPixelBufferAccess;
using tcu::IVec2;
using tcu::PixelBufferAccess;
using tcu::TextureFormat;
using tcu::TextureLevel;
using tcu::UVec2;
using tcu::UVec4;
using tcu::Vec4;

#define ONE_MEGABYTE 1024 * 1024
#define DEFAULT_VERTEX_BUFFER_STRIDE 2
#define ALTERNATIVE_VERTEX_BUFFER_STRIDE 4

enum
{
    MAX_UNIFORM_BUFFER_SIZE = 1024,
    MAX_STORAGE_BUFFER_SIZE = (1 << 28),
    MAX_SIZE                = (128 * 1024)
};

// \todo [mika] Add to utilities
template <typename T>
T divRoundUp(const T &a, const T &b)
{
    return (a / b) + (a % b == 0 ? 0 : 1);
}

enum Usage
{
    // Mapped host read and write
    USAGE_HOST_READ  = (0x1u << 0),
    USAGE_HOST_WRITE = (0x1u << 1),

    // Copy and other transfer operations
    USAGE_TRANSFER_SRC = (0x1u << 2),
    USAGE_TRANSFER_DST = (0x1u << 3),

    // Buffer usage flags
    USAGE_INDEX_BUFFER  = (0x1u << 4),
    USAGE_VERTEX_BUFFER = (0x1u << 5),

    USAGE_UNIFORM_BUFFER = (0x1u << 6),
    USAGE_STORAGE_BUFFER = (0x1u << 7),

    USAGE_UNIFORM_TEXEL_BUFFER = (0x1u << 8),
    USAGE_STORAGE_TEXEL_BUFFER = (0x1u << 9),

    // \todo [2016-03-09 mika] This is probably almost impossible to do
    USAGE_INDIRECT_BUFFER = (0x1u << 10),

    // Texture usage flags
    USAGE_SAMPLED_IMAGE            = (0x1u << 11),
    USAGE_STORAGE_IMAGE            = (0x1u << 12),
    USAGE_COLOR_ATTACHMENT         = (0x1u << 13),
    USAGE_INPUT_ATTACHMENT         = (0x1u << 14),
    USAGE_DEPTH_STENCIL_ATTACHMENT = (0x1u << 15),
};
// Sequential access enums
enum Access
{
    ACCESS_INDIRECT_COMMAND_READ_BIT = 0,
    ACCESS_INDEX_READ_BIT,
    ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
    ACCESS_UNIFORM_READ_BIT,
    ACCESS_INPUT_ATTACHMENT_READ_BIT,
    ACCESS_SHADER_READ_BIT,
    ACCESS_SHADER_WRITE_BIT,
    ACCESS_COLOR_ATTACHMENT_READ_BIT,
    ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
    ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    ACCESS_TRANSFER_READ_BIT,
    ACCESS_TRANSFER_WRITE_BIT,
    ACCESS_HOST_READ_BIT,
    ACCESS_HOST_WRITE_BIT,
    ACCESS_MEMORY_READ_BIT,
    ACCESS_MEMORY_WRITE_BIT,

    ACCESS_LAST
};
// Sequential stage enums
enum PipelineStage
{
    PIPELINESTAGE_TOP_OF_PIPE_BIT = 0,
    PIPELINESTAGE_BOTTOM_OF_PIPE_BIT,
    PIPELINESTAGE_DRAW_INDIRECT_BIT,
    PIPELINESTAGE_VERTEX_INPUT_BIT,
    PIPELINESTAGE_VERTEX_SHADER_BIT,
    PIPELINESTAGE_TESSELLATION_CONTROL_SHADER_BIT,
    PIPELINESTAGE_TESSELLATION_EVALUATION_SHADER_BIT,
    PIPELINESTAGE_GEOMETRY_SHADER_BIT,
    PIPELINESTAGE_FRAGMENT_SHADER_BIT,
    PIPELINESTAGE_EARLY_FRAGMENT_TESTS_BIT,
    PIPELINESTAGE_LATE_FRAGMENT_TESTS_BIT,
    PIPELINESTAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    PIPELINESTAGE_COMPUTE_SHADER_BIT,
    PIPELINESTAGE_TRANSFER_BIT,
    PIPELINESTAGE_HOST_BIT,

    PIPELINESTAGE_LAST
};

// Test backend selects the queue and the set of operations used by the generator.
enum TestBackend
{
    BACKEND_GRAPHICS = 0,
    BACKEND_COMPUTE,
    BACKEND_TRANSFER,
};

inline Usage operator|(Usage a, Usage b)
{
    return (Usage)((uint32_t)a | (uint32_t)b);
}

inline Usage operator&(Usage a, Usage b)
{
    return (Usage)((uint32_t)a & (uint32_t)b);
}

// Shared free-function helpers (defined in vktMemoryPipelineBarrierTestUtils.cpp).
bool supportsDeviceBufferWrites(Usage usage);
bool supportsDeviceImageWrites(Usage usage);
Access accessFlagToAccess(vk::VkAccessFlagBits flag);
PipelineStage pipelineStageFlagToPipelineStage(vk::VkPipelineStageFlagBits flag);
string usageToName(Usage usage);
vk::VkBufferUsageFlags usageToBufferUsageFlags(Usage usage);
vk::VkImageUsageFlags usageToImageUsageFlags(Usage usage);
vk::VkPipelineStageFlags usageToStageFlags(Usage usage, TestBackend backend);
vk::VkAccessFlags usageToAccessFlags(Usage usage);
vk::Move<vk::VkCommandBuffer> createBeginCommandBuffer(const vk::DeviceInterface &vkd, vk::VkDevice device,
                                                       vk::VkCommandPool pool, vk::VkCommandBufferLevel level);
vk::Move<vk::VkBuffer> createBuffer(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkDeviceSize size,
                                    vk::VkBufferUsageFlags usage, vk::VkSharingMode sharingMode,
                                    const vector<uint32_t> &queueFamilies);
vk::Move<vk::VkDeviceMemory> allocMemory(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkDeviceSize size,
                                         uint32_t memoryTypeIndex);
vk::Move<vk::VkDeviceMemory> bindBufferMemory(const vk::InstanceInterface &vki, const vk::DeviceInterface &vkd,
                                              vk::VkPhysicalDevice physicalDevice, vk::VkDevice device,
                                              vk::VkBuffer buffer, vk::VkMemoryPropertyFlags properties);
vk::Move<vk::VkDeviceMemory> bindImageMemory(const vk::InstanceInterface &vki, const vk::DeviceInterface &vkd,
                                             vk::VkPhysicalDevice physicalDevice, vk::VkDevice device,
                                             vk::VkImage image, vk::VkMemoryPropertyFlags properties);
void *mapMemory(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkDeviceMemory memory, vk::VkDeviceSize size);
void *mapMemoryWholeRange(const vk::InstanceInterface &vki, const vk::DeviceInterface &vkd,
                          vk::VkPhysicalDevice physicalDevice, vk::VkDevice device, vk::VkBuffer buffer,
                          vk::VkDeviceMemory memory, vk::VkDeviceSize logicalSize);
vk::VkDeviceSize findMaxBufferSize(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkBufferUsageFlags usage,
                                   vk::VkSharingMode sharingMode, const vector<uint32_t> &queueFamilies,
                                   vk::VkDeviceSize memorySize, uint32_t memoryTypeIndex);
vk::VkDeviceSize roundBufferSizeToWxHx4(vk::VkDeviceSize size);
IVec2 findMaxRGBA8ImageSize(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::VkImageUsageFlags usage,
                            vk::VkSharingMode sharingMode, const vector<uint32_t> &queueFamilies,
                            vk::VkDeviceSize memorySize, uint32_t memoryTypeIndex);
vk::VkAccessFlags getWriteAccessFlags(void);
bool isWriteAccess(vk::VkAccessFlagBits access);
size_t getNumberOfSupportedLayouts(Usage usage);
vk::VkImageLayout getRandomNextLayout(de::Random &rng, Usage usage, vk::VkImageLayout previousLayout);
void removeIllegalAccessFlags(vk::VkAccessFlags &accessflags, vk::VkPipelineStageFlags stageflags);

struct TestConfig
{
    Usage usage;
    uint32_t vertexBufferStride;
    vk::VkDeviceSize size;
    vk::VkSharingMode sharing;
    TestBackend backend;
};

class ReferenceMemory
{
public:
    ReferenceMemory(size_t size);

    void set(size_t pos, uint8_t val);
    uint8_t get(uint64_t pos) const;
    bool isDefined(uint64_t pos) const;

    void setDefined(size_t offset, size_t size, const void *data);
    void setUndefined(size_t offset, size_t size);
    void setData(size_t offset, size_t size, const void *data);

    size_t getSize(void) const
    {
        return m_data.size();
    }

private:
    vector<uint8_t> m_data;
    vector<uint64_t> m_defined;
};
class Memory
{
public:
    Memory(const vk::InstanceInterface &vki, const vk::DeviceInterface &vkd, vk::VkPhysicalDevice physicalDevice,
           vk::VkDevice device, vk::VkDeviceSize size, uint32_t memoryTypeIndex, vk::VkDeviceSize maxBufferSize,
           int32_t maxImageWidth, int32_t maxImageHeight);

    vk::VkDeviceSize getSize(void) const
    {
        return m_size;
    }
    vk::VkDeviceSize getMaxBufferSize(void) const
    {
        return m_maxBufferSize;
    }
    bool getSupportBuffers(void) const
    {
        return m_maxBufferSize > 0;
    }

    int32_t getMaxImageWidth(void) const
    {
        return m_maxImageWidth;
    }
    int32_t getMaxImageHeight(void) const
    {
        return m_maxImageHeight;
    }
    bool getSupportImages(void) const
    {
        return m_maxImageWidth > 0;
    }

    const vk::VkMemoryType &getMemoryType(void) const
    {
        return m_memoryType;
    }
    uint32_t getMemoryTypeIndex(void) const
    {
        return m_memoryTypeIndex;
    }
    vk::VkDeviceMemory getMemory(void) const
    {
        return *m_memory;
    }

private:
    const vk::VkDeviceSize m_size;
    const uint32_t m_memoryTypeIndex;
    const vk::VkMemoryType m_memoryType;
    const vk::Unique<vk::VkDeviceMemory> m_memory;
    const vk::VkDeviceSize m_maxBufferSize;
    const int32_t m_maxImageWidth;
    const int32_t m_maxImageHeight;
};
class Context
{
public:
    Context(const vk::InstanceInterface &vki, const vk::DeviceInterface &vkd, vk::VkPhysicalDevice physicalDevice,
            vk::VkDevice device, vk::VkQueue queue, uint32_t queueFamilyIndex,
            const vector<pair<uint32_t, vk::VkQueue>> &queues, const vk::BinaryCollection &binaryCollection)
        : m_vki(vki)
        , m_vkd(vkd)
        , m_physicalDevice(physicalDevice)
        , m_device(device)
        , m_queue(queue)
        , m_queueFamilyIndex(queueFamilyIndex)
        , m_queues(queues)
        , m_commandPool(
              createCommandPool(vkd, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex))
        , m_binaryCollection(binaryCollection)
    {
        for (size_t queueNdx = 0; queueNdx < m_queues.size(); queueNdx++)
            m_queueFamilies.push_back(m_queues[queueNdx].first);
    }

    const vk::InstanceInterface &getInstanceInterface(void) const
    {
        return m_vki;
    }
    vk::VkPhysicalDevice getPhysicalDevice(void) const
    {
        return m_physicalDevice;
    }
    vk::VkDevice getDevice(void) const
    {
        return m_device;
    }
    const vk::DeviceInterface &getDeviceInterface(void) const
    {
        return m_vkd;
    }
    vk::VkQueue getQueue(void) const
    {
        return m_queue;
    }
    uint32_t getQueueFamily(void) const
    {
        return m_queueFamilyIndex;
    }
    const vector<pair<uint32_t, vk::VkQueue>> &getQueues(void) const
    {
        return m_queues;
    }
    const vector<uint32_t> getQueueFamilies(void) const
    {
        return m_queueFamilies;
    }
    vk::VkCommandPool getCommandPool(void) const
    {
        return *m_commandPool;
    }
    const vk::BinaryCollection &getBinaryCollection(void) const
    {
        return m_binaryCollection;
    }

private:
    const vk::InstanceInterface &m_vki;
    const vk::DeviceInterface &m_vkd;
    const vk::VkPhysicalDevice m_physicalDevice;
    const vk::VkDevice m_device;
    const vk::VkQueue m_queue;
    const uint32_t m_queueFamilyIndex;
    const vector<pair<uint32_t, vk::VkQueue>> m_queues;
    const vk::Unique<vk::VkCommandPool> m_commandPool;
    const vk::BinaryCollection &m_binaryCollection;
    vector<uint32_t> m_queueFamilies;
};

class PrepareContext
{
public:
    PrepareContext(const Context &context, const Memory &memory) : m_context(context), m_memory(memory)
    {
    }

    const Memory &getMemory(void) const
    {
        return m_memory;
    }
    const Context &getContext(void) const
    {
        return m_context;
    }
    const vk::BinaryCollection &getBinaryCollection(void) const
    {
        return m_context.getBinaryCollection();
    }

    void setBuffer(vk::Move<vk::VkBuffer> buffer, vk::VkDeviceSize size)
    {
        DE_ASSERT(!m_currentImage);
        DE_ASSERT(!m_currentBuffer);

        m_currentBuffer     = buffer;
        m_currentBufferSize = size;
    }

    vk::VkBuffer getBuffer(void) const
    {
        return *m_currentBuffer;
    }
    vk::VkDeviceSize getBufferSize(void) const
    {
        DE_ASSERT(m_currentBuffer);
        return m_currentBufferSize;
    }

    void releaseBuffer(void)
    {
        m_currentBuffer.disown();
    }

    void setImage(vk::Move<vk::VkImage> image, vk::VkImageLayout layout, vk::VkDeviceSize memorySize, int32_t width,
                  int32_t height)
    {
        DE_ASSERT(!m_currentImage);
        DE_ASSERT(!m_currentBuffer);

        m_currentImage           = image;
        m_currentImageMemorySize = memorySize;
        m_currentImageLayout     = layout;
        m_currentImageWidth      = width;
        m_currentImageHeight     = height;
    }

    void setImageLayout(vk::VkImageLayout layout)
    {
        DE_ASSERT(m_currentImage);
        m_currentImageLayout = layout;
    }

    vk::VkImage getImage(void) const
    {
        return *m_currentImage;
    }
    int32_t getImageWidth(void) const
    {
        DE_ASSERT(m_currentImage);
        return m_currentImageWidth;
    }
    int32_t getImageHeight(void) const
    {
        DE_ASSERT(m_currentImage);
        return m_currentImageHeight;
    }
    vk::VkDeviceSize getImageMemorySize(void) const
    {
        DE_ASSERT(m_currentImage);
        return m_currentImageMemorySize;
    }

    void releaseImage(void)
    {
        m_currentImage.disown();
    }

    vk::VkImageLayout getImageLayout(void) const
    {
        DE_ASSERT(m_currentImage);
        return m_currentImageLayout;
    }

private:
    const Context &m_context;
    const Memory &m_memory;

    vk::Move<vk::VkBuffer> m_currentBuffer;
    vk::VkDeviceSize m_currentBufferSize;

    vk::Move<vk::VkImage> m_currentImage;
    vk::VkDeviceSize m_currentImageMemorySize;
    vk::VkImageLayout m_currentImageLayout;
    int32_t m_currentImageWidth;
    int32_t m_currentImageHeight;
};

class ExecuteContext
{
public:
    ExecuteContext(const Context &context) : m_context(context)
    {
    }

    const Context &getContext(void) const
    {
        return m_context;
    }
    void setMapping(void *ptr)
    {
        m_mapping = ptr;
    }
    void *getMapping(void) const
    {
        return m_mapping;
    }

private:
    const Context &m_context;
    void *m_mapping;
};

class VerifyContext
{
public:
    VerifyContext(TestLog &log, tcu::ResultCollector &resultCollector, const Context &context, vk::VkDeviceSize size)
        : m_log(log)
        , m_resultCollector(resultCollector)
        , m_context(context)
        , m_reference((size_t)size)
    {
    }

    const Context &getContext(void) const
    {
        return m_context;
    }
    TestLog &getLog(void) const
    {
        return m_log;
    }
    tcu::ResultCollector &getResultCollector(void) const
    {
        return m_resultCollector;
    }

    ReferenceMemory &getReference(void)
    {
        return m_reference;
    }
    TextureLevel &getReferenceImage(void)
    {
        return m_referenceImage;
    }

private:
    TestLog &m_log;
    tcu::ResultCollector &m_resultCollector;
    const Context &m_context;
    ReferenceMemory m_reference;
    TextureLevel m_referenceImage;
};

class Command
{
public:
    // Constructor should allocate all non-vulkan resources.
    virtual ~Command(void)
    {
    }

    // Get name of the command
    virtual const char *getName(void) const = 0;

    // Log prepare operations
    virtual void logPrepare(TestLog &, size_t) const
    {
    }
    // Log executed operations
    virtual void logExecute(TestLog &, size_t) const
    {
    }

    // Log executed operations in submission order, marking the first sub-command
    // that introduced a verification failure with a ">>> FIRST FAILURE HERE" line.
    // Returns true if a marker was emitted at or below this command. The default
    // behaves like logExecute() and never marks; container commands override it.
    virtual bool logExecuteFailureTrace(TestLog &log, size_t commandIndex, const string &failureMessage) const
    {
        DE_UNREF(failureMessage);
        logExecute(log, commandIndex);
        return false;
    }

    // Prepare should allocate all vulkan resources and resources that require
    // that buffer or memory has been already allocated. This should build all
    // command buffers etc.
    virtual void prepare(PrepareContext &)
    {
    }

    // Execute command. Write or read mapped memory, submit commands to queue
    // etc.
    virtual void execute(ExecuteContext &)
    {
    }

    // Verify that results are correct.
    virtual void verify(VerifyContext &, size_t)
    {
    }

protected:
    // Allow only inheritance
    Command(void)
    {
    }

private:
    // Disallow copying
    Command(const Command &);
    Command &operator&(const Command &);
};
class SubmitContext
{
public:
    SubmitContext(const PrepareContext &context, const vk::VkCommandBuffer commandBuffer)
        : m_context(context)
        , m_commandBuffer(commandBuffer)
    {
    }

    const Memory &getMemory(void) const
    {
        return m_context.getMemory();
    }
    const Context &getContext(void) const
    {
        return m_context.getContext();
    }
    vk::VkCommandBuffer getCommandBuffer(void) const
    {
        return m_commandBuffer;
    }

    vk::VkBuffer getBuffer(void) const
    {
        return m_context.getBuffer();
    }
    vk::VkDeviceSize getBufferSize(void) const
    {
        return m_context.getBufferSize();
    }

    vk::VkImage getImage(void) const
    {
        return m_context.getImage();
    }
    int32_t getImageWidth(void) const
    {
        return m_context.getImageWidth();
    }
    int32_t getImageHeight(void) const
    {
        return m_context.getImageHeight();
    }

private:
    const PrepareContext &m_context;
    const vk::VkCommandBuffer m_commandBuffer;
};

class CmdCommand
{
public:
    virtual ~CmdCommand(void)
    {
    }
    virtual const char *getName(void) const = 0;

    // Log things that are done during prepare
    virtual void logPrepare(TestLog &, size_t) const
    {
    }
    // Log submitted calls etc.
    virtual void logSubmit(TestLog &, size_t) const
    {
    }

    // See Command::logExecuteFailureTrace. Same contract for sub-commands.
    virtual bool logSubmitFailureTrace(TestLog &log, size_t commandIndex, const string &failureMessage) const
    {
        DE_UNREF(failureMessage);
        logSubmit(log, commandIndex);
        return false;
    }

    // Allocate vulkan resources and prepare for submit.
    virtual void prepare(PrepareContext &)
    {
    }

    // Submit commands to command buffer.
    virtual void submit(SubmitContext &)
    {
    }

    // Verify results
    virtual void verify(VerifyContext &, size_t)
    {
    }
};
enum Op
{
    OP_MAP,
    OP_UNMAP,

    OP_MAP_FLUSH,
    OP_MAP_INVALIDATE,

    OP_MAP_READ,
    OP_MAP_WRITE,
    OP_MAP_MODIFY,

    OP_BUFFER_CREATE,
    OP_BUFFER_DESTROY,
    OP_BUFFER_BINDMEMORY,

    OP_QUEUE_WAIT_FOR_IDLE,
    OP_DEVICE_WAIT_FOR_IDLE,

    OP_COMMAND_BUFFER_BEGIN,
    OP_COMMAND_BUFFER_END,

    // Secondary, non render pass command buffers
    // Render pass secondary command buffers are not currently covered
    OP_SECONDARY_COMMAND_BUFFER_BEGIN,
    OP_SECONDARY_COMMAND_BUFFER_END,

    // Buffer transfer operations
    OP_BUFFER_FILL,
    OP_BUFFER_UPDATE,

    OP_BUFFER_COPY_TO_BUFFER,
    OP_BUFFER_COPY_FROM_BUFFER,

    OP_BUFFER_COPY_TO_IMAGE,
    OP_BUFFER_COPY_FROM_IMAGE,

    OP_IMAGE_CREATE,
    OP_IMAGE_DESTROY,
    OP_IMAGE_BINDMEMORY,

    OP_IMAGE_TRANSITION_LAYOUT,

    OP_IMAGE_COPY_TO_BUFFER,
    OP_IMAGE_COPY_FROM_BUFFER,

    OP_IMAGE_COPY_TO_IMAGE,
    OP_IMAGE_COPY_FROM_IMAGE,

    OP_IMAGE_BLIT_TO_IMAGE,
    OP_IMAGE_BLIT_FROM_IMAGE,

    OP_IMAGE_RESOLVE,

    OP_PIPELINE_BARRIER_GLOBAL,
    OP_PIPELINE_BARRIER_BUFFER,
    OP_PIPELINE_BARRIER_IMAGE,

    // Renderpass operations
    OP_RENDERPASS_BEGIN,
    OP_RENDERPASS_END,

    // Commands inside render pass
    OP_RENDER_VERTEX_BUFFER,
    OP_RENDER_INDEX_BUFFER,

    OP_RENDER_VERTEX_UNIFORM_BUFFER,
    OP_RENDER_FRAGMENT_UNIFORM_BUFFER,

    OP_RENDER_VERTEX_UNIFORM_TEXEL_BUFFER,
    OP_RENDER_FRAGMENT_UNIFORM_TEXEL_BUFFER,

    OP_RENDER_VERTEX_STORAGE_BUFFER,
    OP_RENDER_FRAGMENT_STORAGE_BUFFER,

    OP_RENDER_VERTEX_STORAGE_TEXEL_BUFFER,
    OP_RENDER_FRAGMENT_STORAGE_TEXEL_BUFFER,

    OP_RENDER_VERTEX_STORAGE_IMAGE,
    OP_RENDER_FRAGMENT_STORAGE_IMAGE,

    OP_RENDER_VERTEX_SAMPLED_IMAGE,
    OP_RENDER_FRAGMENT_SAMPLED_IMAGE,

    // Compute pass operations (compute backend only)
    OP_COMPUTEPASS_BEGIN,
    OP_COMPUTEPASS_END,

    // Commands inside compute pass
    OP_COMPUTE_UNIFORM_BUFFER,
    OP_COMPUTE_UNIFORM_TEXEL_BUFFER,
    OP_COMPUTE_STORAGE_BUFFER,
    OP_COMPUTE_STORAGE_TEXEL_BUFFER,
    OP_COMPUTE_STORAGE_IMAGE,
    OP_COMPUTE_SAMPLED_IMAGE,
};

enum Stage
{
    STAGE_HOST,
    STAGE_COMMAND_BUFFER,
    STAGE_SECONDARY_COMMAND_BUFFER,

    STAGE_RENDER_PASS,
    STAGE_COMPUTE_PASS
};
class CacheState
{
public:
    CacheState(vk::VkPipelineStageFlags allowedStages, vk::VkAccessFlags allowedAccesses);

    bool isValid(vk::VkPipelineStageFlagBits stage, vk::VkAccessFlagBits access) const;

    void perform(vk::VkPipelineStageFlagBits stage, vk::VkAccessFlagBits access);

    void submitCommandBuffer(void);
    void waitForIdle(void);

    void getFullBarrier(vk::VkPipelineStageFlags &srcStages, vk::VkAccessFlags &srcAccesses,
                        vk::VkPipelineStageFlags &dstStages, vk::VkAccessFlags &dstAccesses) const;

    void barrier(vk::VkPipelineStageFlags srcStages, vk::VkAccessFlags srcAccesses, vk::VkPipelineStageFlags dstStages,
                 vk::VkAccessFlags dstAccesses);

    void imageLayoutBarrier(vk::VkPipelineStageFlags srcStages, vk::VkAccessFlags srcAccesses,
                            vk::VkPipelineStageFlags dstStages, vk::VkAccessFlags dstAccesses);

    void checkImageLayoutBarrier(vk::VkPipelineStageFlags srcStages, vk::VkAccessFlags srcAccesses,
                                 vk::VkPipelineStageFlags dstStages, vk::VkAccessFlags dstAccesses);

    // Everything is clean and there is no need for barriers
    bool isClean(void) const;

    vk::VkPipelineStageFlags getAllowedStages(void) const
    {
        return m_allowedStages;
    }
    vk::VkAccessFlags getAllowedAcceses(void) const
    {
        return m_allowedAccesses;
    }

private:
    // Limit which stages and accesses are used by the CacheState tracker
    const vk::VkPipelineStageFlags m_allowedStages;
    const vk::VkAccessFlags m_allowedAccesses;

    // [dstStage][srcStage][dstAccess] = srcAccesses
    // In stage dstStage write srcAccesses from srcStage are not yet available for dstAccess
    vk::VkAccessFlags m_unavailableWriteOperations[PIPELINESTAGE_LAST][PIPELINESTAGE_LAST][ACCESS_LAST];
    // Latest pipeline transition is not available in stage
    bool m_unavailableLayoutTransition[PIPELINESTAGE_LAST];
    // [dstStage] = dstAccesses
    // In stage dstStage ops with dstAccesses are not yet visible
    vk::VkAccessFlags m_invisibleOperations[PIPELINESTAGE_LAST];

    // [dstStage] = srcStage
    // Memory operation in srcStage have not completed before dstStage
    vk::VkPipelineStageFlags m_incompleteOperations[PIPELINESTAGE_LAST];
};
struct State
{
    State(Usage usage, TestBackend backend, uint32_t seed)
        : stage(STAGE_HOST)
        , cache(usageToStageFlags(usage, backend), usageToAccessFlags(usage))
        , rng(seed)
        , mapped(false)
        , hostInvalidated(true)
        , hostFlushed(true)
        , memoryDefined(false)
        , hasBuffer(false)
        , hasBoundBufferMemory(false)
        , hasImage(false)
        , hasBoundImageMemory(false)
        , imageLayout(vk::VK_IMAGE_LAYOUT_UNDEFINED)
        , imageDefined(false)
        , queueIdle(true)
        , deviceIdle(true)
        , commandBufferIsEmpty(true)
        , primaryCommandBufferIsEmpty(true)
        , renderPassIsEmpty(true)
    {
    }

    Stage stage;
    CacheState cache;
    de::Random rng;

    bool mapped;
    bool hostInvalidated;
    bool hostFlushed;
    bool memoryDefined;

    bool hasBuffer;
    bool hasBoundBufferMemory;

    bool hasImage;
    bool hasBoundImageMemory;
    vk::VkImageLayout imageLayout;
    bool imageDefined;

    bool queueIdle;
    bool deviceIdle;

    bool commandBufferIsEmpty;

    // a copy of commandBufferIsEmpty value, when secondary command buffer is in use
    bool primaryCommandBufferIsEmpty;

    bool renderPassIsEmpty;
};

// Generator entry points shared by all backends (defined in vktMemoryPipelineBarrierTestUtils.cpp).
void getAvailableOps(const State &state, bool supportsBuffers, bool supportsImages, Usage usage, TestBackend backend,
                     vector<Op> &ops);
void applyOp(State &state, const Memory &memory, Op op, Usage usage);

// Backend hook: builds the commands recorded inside a render pass. Implemented in
// vktMemoryPipelineBarrierGraphicsTests.cpp and called by createCmdCommands().
de::MovePtr<CmdCommand> createRenderPassCommands(const Memory &memory, de::Random &nextOpRng, State &state,
                                                 const TestConfig &testConfig, size_t &opNdx, size_t opCount);

// Backend hook: builds the commands recorded inside a compute pass. Implemented in
// vktMemoryPipelineBarrierComputeTests.cpp and called by createCmdCommands().
de::MovePtr<CmdCommand> createComputeCommands(const Memory &memory, de::Random &nextOpRng, State &state,
                                              const TestConfig &testConfig, size_t &opNdx, size_t opCount);

class MemoryTestInstance : public TestInstance
{
public:
    typedef bool (MemoryTestInstance::*StageFunc)(void);

    MemoryTestInstance(::vkt::Context &context, const TestConfig &config);
    ~MemoryTestInstance(void);

    tcu::TestStatus iterate(void);

private:
    const TestConfig m_config;
    const size_t m_iterationCount;
    const size_t m_opCount;
    const vk::VkPhysicalDeviceMemoryProperties m_memoryProperties;
    uint32_t m_memoryTypeNdx;
    size_t m_iteration;
    StageFunc m_stage;
    tcu::ResultCollector m_resultCollector;

    vector<Command *> m_commands;
    MovePtr<Memory> m_memory;
    MovePtr<Context> m_renderContext;
    MovePtr<PrepareContext> m_prepareContext;

    bool nextIteration(void);
    bool nextMemoryType(void);

    bool createCommandsAndAllocateMemory(void);
    bool prepare(void);
    bool execute(void);
    bool verify(void);
    void resetResources(void);

    // Logs the prepare/execute description of m_commands[0, prepareCount)/[0, executeCount).
    // Only called on failure paths, so passing test cases don't bloat the log.
    void logCommandTrace(TestLog &log, size_t prepareCount, size_t executeCount) const;

    // Logs the full command trace in submission order (barriers included) and marks
    // the sub-command that first introduced a verification failure. failingCmdNdx is
    // the top-level command that flipped the result; failureMessage is its mismatch
    // description. Only called when a verification mismatch is detected.
    void logFailureTrace(TestLog &log, size_t failingCmdNdx, const string &failureMessage) const;
};

} // namespace pipelinebarrier
} // namespace memory
} // namespace vkt

#endif // _VKTMEMORYPIPELINEBARRIERTESTUTILS_HPP
