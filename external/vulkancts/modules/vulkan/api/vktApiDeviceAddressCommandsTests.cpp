/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
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
 * \brief Dedicated tests for VK_KHR_device_address_commands
 *//*--------------------------------------------------------------------*/

#include "vktApiDeviceAddressCommandsTests.hpp"
#include "vkComputePipelineConstructionUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vktTestCase.hpp"

namespace vkt::api
{
namespace
{
using namespace vk;

enum class CommandFlagTestMode
{
    // Copy to/from sparsely bound memory, with some unbound ranges of memory
    COPY_TO_MEMORY_WITH_UNBOUND_RANGES = 0,
    COPY_FROM_MEMORY_WITH_UNBOUND_RANGES,

    USE_ALL_VERTEX_INDEX_BINDS,
    BASIC_SET_STRIDE,
    COMPLEX_SET_STRIDE,

    MEMORY_RANGE_BARRIER,
};

struct TestParams
{
    CommandFlagTestMode mode;
};

class BufferAddressCommandFlagsTestInstance : public vkt::TestInstance
{
public:
    using AllocVect = std::vector<de::MovePtr<Allocation>>;

    BufferAddressCommandFlagsTestInstance(Context &context, const TestParams &params);

    tcu::TestStatus iterate(void) override;

private:
    Move<VkBuffer> constructBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBufferCreateFlags createFlag) const;
    void bindBufferMemory(VkBuffer buffer, VkDeviceSize size, AllocVect &allocVect, bool useSparse);

private:
    TestParams m_params;

    VkBufferCreateFlags m_srcBufferCreateFlag;
    VkBufferCreateFlags m_dstBufferCreateFlag;
    VkDeviceSize m_srcBufferSize;
    VkDeviceSize m_dstBufferSize;
    VkAddressCommandFlagsKHR m_srcCommandFlag;
    VkAddressCommandFlagsKHR m_dstCommandFlag;

    VkBufferCopy m_copyRegion;

    bool m_useSingleChunk;
    bool m_replaceSrcOffsetWithChunkSize;
    bool m_replaceDstOffsetWithChunkSize;

    VkDeviceSize m_chunkSize;
    AllocVect m_srcBufferAllocVect;
    AllocVect m_dstBufferAllocVect;
};

BufferAddressCommandFlagsTestInstance::BufferAddressCommandFlagsTestInstance(Context &context, const TestParams &params)
    : vkt::TestInstance(context)
    , m_params(params)
    , m_srcBufferCreateFlag(0)
    , m_dstBufferCreateFlag(0)
    , m_srcBufferSize(64ull)
    , m_dstBufferSize(64ull)
    , m_srcCommandFlag(0)
    , m_dstCommandFlag(0)
    , m_copyRegion{0u, 0u, 512u}
    , m_useSingleChunk(false)
    , m_replaceSrcOffsetWithChunkSize(false)
    , m_replaceDstOffsetWithChunkSize(false)
    , m_chunkSize(0)
{
    // setup configuration parameters based on the test mode
    switch (m_params.mode)
    {
    case CommandFlagTestMode::COPY_TO_MEMORY_WITH_UNBOUND_RANGES:
        m_srcBufferSize                 = 512ull;
        m_dstBufferSize                 = 1 << 18;
        m_dstCommandFlag                = VK_ADDRESS_COMMAND_UNKNOWN_STORAGE_BUFFER_USAGE_BIT_KHR;
        m_dstBufferCreateFlag           = VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT;
        m_copyRegion                    = {0u, 0u, 512u};
        m_replaceDstOffsetWithChunkSize = true;
        m_useSingleChunk                = true;
        break;

    default:
    case CommandFlagTestMode::COPY_FROM_MEMORY_WITH_UNBOUND_RANGES:
        m_srcBufferSize                 = 1 << 18;
        m_dstBufferSize                 = 512ull;
        m_srcCommandFlag                = 0;
        m_srcBufferCreateFlag           = VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT;
        m_copyRegion                    = {0u, 0u, 512u};
        m_replaceSrcOffsetWithChunkSize = true;
        m_useSingleChunk                = true;
        break;
    };
}

tcu::TestStatus BufferAddressCommandFlagsTestInstance::iterate()
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    auto device               = m_context.getDevice();
    auto queue                = m_context.getUniversalQueue();
    auto queueFamilyIndex     = m_context.getUniversalQueueFamilyIndex();
    auto &allocator           = m_context.getDefaultAllocator();
    VkDeviceSize size         = m_copyRegion.size;

    const bool useSparseSrc = (m_srcBufferCreateFlag & VK_BUFFER_CREATE_SPARSE_BINDING_BIT);
    const bool useSparseDst = (m_dstBufferCreateFlag & VK_BUFFER_CREATE_SPARSE_BINDING_BIT);

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    auto srcBuffer           = constructBuffer(m_srcBufferSize, usage, m_srcBufferCreateFlag);
    auto dstBuffer           = constructBuffer(m_dstBufferSize, usage, m_dstBufferCreateFlag);

    bindBufferMemory(*srcBuffer, m_srcBufferSize, m_srcBufferAllocVect, useSparseSrc);
    bindBufferMemory(*dstBuffer, m_dstBufferSize, m_dstBufferAllocVect, useSparseDst);

    // now that we know what is chunk size we can replace copyRegion with it if needed
    if (m_replaceSrcOffsetWithChunkSize)
        m_copyRegion.srcOffset = m_chunkSize;
    if (m_replaceDstOffsetWithChunkSize)
        m_copyRegion.dstOffset = m_chunkSize;

    // create staging buffer used to upload data to sparse buffer chunk
    VkBufferCreateInfo bufferCreateInfo = initVulkanStructure();
    bufferCreateInfo.size               = std::max(m_chunkSize, m_srcBufferSize);
    bufferCreateInfo.usage              = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    BufferWithMemory stagingBuffer(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible);

    // write initial data to buffers
    const uint8_t testValue = 253;
    Allocation &srcAlloc    = useSparseSrc ? stagingBuffer.getAllocation() : *m_srcBufferAllocVect[0];
    Allocation &dstAlloc    = useSparseDst ? stagingBuffer.getAllocation() : *m_dstBufferAllocVect[0];
    deMemset(srcAlloc.getHostPtr(), testValue, static_cast<std::size_t>(size));
    flushAlloc(vk, device, srcAlloc);
    deMemset(dstAlloc.getHostPtr(), 0, static_cast<std::size_t>(size));
    flushAlloc(vk, device, dstAlloc);

    VkDeviceAddress srcBufferDeviceAddress = getBufferDeviceAddress(vk, device, *srcBuffer);
    VkDeviceAddress dstBufferDeviceAddress = getBufferDeviceAddress(vk, device, *dstBuffer);

    VkDeviceAddressRangeKHR srcAddressRange{srcBufferDeviceAddress + m_copyRegion.srcOffset, m_copyRegion.size};
    VkDeviceAddressRangeKHR dstAddressRange{dstBufferDeviceAddress + m_copyRegion.dstOffset, m_copyRegion.size};

    VkDeviceMemoryCopyKHR memoryCopy2KHR = initVulkanStructure();
    memoryCopy2KHR.srcRange              = srcAddressRange;
    memoryCopy2KHR.srcFlags              = m_srcCommandFlag;
    memoryCopy2KHR.dstRange              = dstAddressRange;
    memoryCopy2KHR.dstFlags              = m_dstCommandFlag;

    const auto cmdPool(createCommandPool(vk, device, (VkCommandPoolCreateFlags)0, queueFamilyIndex));
    const auto cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // helper function to create a buffer memory barrier
    auto cmdBufferBarrier = [&](VkPipelineStageFlags srcStageMask, VkAccessFlags srcAccessMask, VkBuffer buffer,
                                VkPipelineStageFlags dstStageMask, VkAccessFlags dstAccessMask)
    {
        const auto bb = makeBufferMemoryBarrier(srcAccessMask, dstAccessMask, buffer, 0, size);
        vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 1, &bb, 0, nullptr);
    };

    beginCommandBuffer(vk, *cmdBuffer);

    auto initialBuffer = (useSparseSrc ? *stagingBuffer : *srcBuffer);
    cmdBufferBarrier(VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_WRITE_BIT, initialBuffer,
                     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);

    // copy staging buffer to sparse buffer chunk
    if (useSparseSrc)
    {
        VkBufferCopy bufferCopy = {0, m_copyRegion.srcOffset, size};
        vk.cmdCopyBuffer(*cmdBuffer, *stagingBuffer, *srcBuffer, 1, &bufferCopy);

        cmdBufferBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, *srcBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    }

    // use cmdCopyMemoryKHR instead of vk.cmdCopyBuffer(*cmdBuffer, *srcBuffer, *dstBuffer, 1, &m_copyRegion);
    VkCopyDeviceMemoryInfoKHR memorInfo = initVulkanStructure();
    memorInfo.regionCount               = 1u;
    memorInfo.pRegions                  = &memoryCopy2KHR;
    vk.cmdCopyMemoryKHR(*cmdBuffer, &memorInfo);

    cmdBufferBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, *dstBuffer,
                     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);

    if (useSparseDst)
    {
        VkBufferCopy bufferCopy = {m_copyRegion.dstOffset, 0, size};
        vk.cmdCopyBuffer(*cmdBuffer, *dstBuffer, *stagingBuffer, 1, &bufferCopy);

        cmdBufferBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, *stagingBuffer,
                         VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT);
    }

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    m_context.resetCommandPoolForVKSC(device, *cmdPool);

    const auto &alloc = useSparseDst ? stagingBuffer.getAllocation() : *m_dstBufferAllocVect[0];
    invalidateAlloc(vk, device, alloc);

    VkDeviceSize validDataCount = 0;
    uint8_t *ptr                = (uint8_t *)alloc.getHostPtr();
    for (auto i = 0ULL; i < size; ++i)
        validDataCount += (ptr[i] == testValue);

    if (validDataCount == size)
        return tcu::TestStatus::pass("Pass");
    return tcu::TestStatus::fail("Fail");
}

Move<VkBuffer> BufferAddressCommandFlagsTestInstance::constructBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                                                      VkBufferCreateFlags createFlag) const
{
    auto device = m_context.getDevice();
    uint32_t queueFamilyIndex[]{m_context.getUniversalQueueFamilyIndex(), m_context.getSparseQueueFamilyIndex()};

    usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    auto bufferParams  = makeBufferCreateInfo(size, usage);
    bufferParams.flags = createFlag;

    if (createFlag & VK_BUFFER_CREATE_SPARSE_BINDING_BIT)
    {
        bufferParams.queueFamilyIndexCount = 2;
        bufferParams.pQueueFamilyIndices   = queueFamilyIndex;
    }

    return createBuffer(m_context.getDeviceInterface(), device, &bufferParams);
}

void BufferAddressCommandFlagsTestInstance::bindBufferMemory(VkBuffer buffer, VkDeviceSize size, AllocVect &allocVect,
                                                             bool useSparse)
{
    const DeviceInterface &vk                     = m_context.getDeviceInterface();
    auto device                                   = m_context.getDevice();
    auto &allocator                               = m_context.getDefaultAllocator();
    const VkMemoryRequirements memoryRequirements = getBufferMemoryRequirements(vk, device, buffer);

    if (useSparse)
    {
        m_chunkSize                 = memoryRequirements.alignment;
        uint32_t chunkCount         = static_cast<uint32_t>(size / m_chunkSize);
        VkDeviceSize resourceOffset = 0;
        if (m_useSingleChunk)
        {
            chunkCount = 1;

            // we bind memory just to second chunk when we use single chunk
            resourceOffset = m_chunkSize;
        }

        std::vector<VkSparseMemoryBind> sparseMemoryBindVect(chunkCount);

        for (uint32_t i = 0; i < chunkCount; ++i)
        {
            allocVect.push_back(allocator.allocate(memoryRequirements, MemoryRequirement::Any));

            sparseMemoryBindVect[i] = {i * m_chunkSize + resourceOffset, // resourceOffset
                                       m_chunkSize,                      // size
                                       allocVect[i]->getMemory(),        // memory
                                       0,                                // memoryOffset
                                       0};                               // flags
        }

        const VkSparseBufferMemoryBindInfo sparseBufferMemoryBindInfo{
            buffer,
            chunkCount,
            sparseMemoryBindVect.data(),
        };

        VkBindSparseInfo bindInfo = initVulkanStructure();
        bindInfo.bufferBindCount  = 1;
        bindInfo.pBufferBinds     = &sparseBufferMemoryBindInfo;

        const Unique<VkFence> fence(createFence(vk, device));
        VK_CHECK(vk.queueBindSparse(m_context.getSparseQueue(), 1u, &bindInfo, *fence));
        VK_CHECK(vk.waitForFences(device, 1u, &fence.get(), VK_TRUE, ~0ull));
    }
    else
    {
        auto mr = MemoryRequirement::DeviceAddress | MemoryRequirement::HostVisible | MemoryRequirement::Any;
        allocVect.push_back(allocator.allocate(memoryRequirements, mr));
        auto &bAlloc = allocVect.back();
        vk.bindBufferMemory(device, buffer, bAlloc->getMemory(), bAlloc->getOffset());
    }
}

class VertexIndexBindingTestInstance : public vkt::TestInstance
{
public:
    VertexIndexBindingTestInstance(Context &context, const TestParams &params);
    ~VertexIndexBindingTestInstance() = default;

    tcu::TestStatus iterate(void) override;

protected:
    void setVertexInput(VkCommandBuffer cmdBuffer, VkDeviceSize stride) const;
    void drawUsingBasicSetStride(VkCommandBuffer cmdBuffer) const;
    void drawUsingComplexSetStride(VkCommandBuffer cmdBuffer) const;
    void drawUsingAllVertexIndexBinds(VkCommandBuffer cmdBuffer) const;

private:
    TestParams m_params;

    // configuration of test uses by default values for USE_ALL_VERTEX_INDEX_BINDS mode;
    // each test operates on one or more vertex buffers;
    // vertex buffer contain 4 vertices with 2 components each and some
    // extra unused floats defined by unusedVertexFloats;
    // number of vertex buffers is defined by the size of unusedVertexFloats
    std::vector<uint32_t> m_unusedVertexFloats{0u, 0u, 0u};
    bool m_useIndexBuffers            = true;
    bool m_useRegularPipeline         = true;
    bool m_useVertexInputDynamicState = false;

    Move<VkPipeline> m_graphicsPipeline;
    Move<VkPipeline> m_graphicsPipelineDS;

    typedef std::vector<de::MovePtr<vk::BufferWithMemory>> BufferWithMemoryVect;

    std::vector<VkDeviceSize> m_vertexBufferSizes;
    BufferWithMemoryVect m_vertexBuffers;
    BufferWithMemoryVect m_indexBuffers;
};

VertexIndexBindingTestInstance::VertexIndexBindingTestInstance(Context &context, const TestParams &params)
    : vkt::TestInstance(context)
    , m_params(params)
{
    // configure test parameters based on the test mode
    if (m_params.mode == CommandFlagTestMode::BASIC_SET_STRIDE)
    {
        m_unusedVertexFloats = {4u, 4u};
        m_useIndexBuffers    = false;
        m_useRegularPipeline = false;
    }
    else if (m_params.mode == CommandFlagTestMode::COMPLEX_SET_STRIDE)
    {
        // note first three values from m_unusedVertexFloats are used for strides calculation in drawUsingComplexSetStride
        // but remaining values in m_unusedVertexFloats are adjusted so that vertex buffers have proper values
        m_unusedVertexFloats         = {3u, 2u, 4u, 2u, 3u, 3u};
        m_useIndexBuffers            = false;
        m_useRegularPipeline         = false;
        m_useVertexInputDynamicState = true;
    }
}

tcu::TestStatus VertexIndexBindingTestInstance::iterate(void)
{
    DE_UNREF(m_params);

    const DeviceInterface &vk = m_context.getDeviceInterface();
    auto device               = m_context.getDevice();
    auto queue                = m_context.getUniversalQueue();
    auto queueFamilyIndex     = m_context.getUniversalQueueFamilyIndex();
    auto &allocator           = m_context.getDefaultAllocator();

    // define image for color attachment
    const VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    const VkExtent3D imageExtent{64, 8, 1};
    const auto bufferCount = m_unusedVertexFloats.size();
    auto usage             = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ImageWithBuffer colorBuffer(vk, device, allocator, imageExtent, format, usage, VK_IMAGE_TYPE_2D);

    // define buffers for vertex data
    const auto vertexBufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    auto bufferCreateInfo        = makeBufferCreateInfo(0, vertexBufferUsage);
    MemoryRequirement memReq     = MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress;
    m_vertexBuffers.resize(bufferCount);
    m_vertexBufferSizes.resize(bufferCount);
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        m_vertexBufferSizes[i] = 4u * (2u + m_unusedVertexFloats[i]) * sizeof(float);
        bufferCreateInfo.size  = m_vertexBufferSizes[i];
        m_vertexBuffers[i]     = de::MovePtr<vk::BufferWithMemory>(
            new vk::BufferWithMemory(vk, device, allocator, bufferCreateInfo, memReq));
    }

    // fill vertex buffers with data for vertical rectangles
    const float qXSize = 0.125f;
    float vertexData[]{-1.0f, 1.0f, -1.0f, -1.0f, -1.0f + qXSize, 1.0f, -1.0f + qXSize, -1.0f};
    for (uint32_t vertexBufferIndex = 0; vertexBufferIndex < bufferCount; vertexBufferIndex++)
    {
        auto &vb                     = m_vertexBuffers[vertexBufferIndex];
        auto &vertexBufferAllocation = vb->getAllocation();
        float *vertexDataPtr         = static_cast<float *>(vertexBufferAllocation.getHostPtr());

        // each vertex buffer has 4 vertex but stride between vertices is defined by unusedVertexFloats
        for (uint32_t vertexIndex = 0; vertexIndex < 4u; vertexIndex++)
        {
            vertexDataPtr[0] = vertexData[vertexIndex * 2];
            vertexDataPtr[1] = vertexData[vertexIndex * 2 + 1];
            vertexDataPtr += 2 + m_unusedVertexFloats[vertexBufferIndex];
        }

        flushAlloc(vk, device, vertexBufferAllocation);

        // move rectangle data horizontally by 0.5f for next buffer
        for (uint32_t i = 0; i < std::size(vertexData); i++)
            vertexData[i] += (i % 2) ? 0.0f : 0.25f;
    }

    // define index buffers
    if (m_useIndexBuffers)
    {
        m_indexBuffers.resize(bufferCount);

        // includes space for extra indices so that all imdex buffers are different
        const auto itemsCount  = 4u + bufferCount - 1;
        bufferCreateInfo.size  = itemsCount * sizeof(uint32_t);
        bufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        for (uint32_t i = 0; i < bufferCount; ++i)
        {
            m_indexBuffers[i] = de::MovePtr<vk::BufferWithMemory>(
                new vk::BufferWithMemory(vk, device, allocator, bufferCreateInfo, memReq));
        }

        // fill index buffers with data for rectangles
        uint32_t startIndex = 0;
        for (auto &ib : m_indexBuffers)
        {
            auto &indexBufferAllocation = ib->getAllocation();
            uint32_t *indexDataPtr      = static_cast<uint32_t *>(indexBufferAllocation.getHostPtr());
            for (uint32_t i = startIndex; i < itemsCount; ++i)
                indexDataPtr[i] = i - startIndex;
            ++startIndex;
            flushAlloc(vk, device, indexBufferAllocation);
        }
    }

    // note there is assumption here that draw call that will use m_graphicsPipeline wil use stride 8
    const VkVertexInputBindingDescription vertexBindings{0u, 8u, VK_VERTEX_INPUT_RATE_VERTEX};
    const VkVertexInputAttributeDescription vertexAttribs{0, 0, VK_FORMAT_R32G32_SFLOAT, 0u};
    VkPipelineVertexInputStateCreateInfo vertexInputState{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, 0, 1, &vertexBindings, 1, &vertexAttribs};

    auto colorView   = colorBuffer.getImageView();
    auto renderPass  = makeRenderPass(vk, device, format);
    auto framebuffer = makeFramebuffer(vk, device, *renderPass, colorView, imageExtent.width, imageExtent.height);

    // create pipelines
    auto pipelineLayout = makePipelineLayout(vk, device, VK_NULL_HANDLE);
    const std::vector<VkRect2D> scissors{makeRect2D(imageExtent.width, imageExtent.height)};
    const std::vector<VkViewport> viewports{makeViewport(imageExtent.width, imageExtent.height)};
    const auto &bc = m_context.getBinaryCollection();
    const auto vs(createShaderModule(vk, device, bc.get("vert")));
    const auto fs(createShaderModule(vk, device, bc.get("frag")));

    if (m_useRegularPipeline)
    {
        m_graphicsPipeline = makeGraphicsPipeline(vk, device, *pipelineLayout, *vs, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                                  VK_NULL_HANDLE, *fs, *renderPass, viewports, scissors,
                                                  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0, 0, &vertexInputState);
    }

    std::vector<VkDynamicState> dynamicStates{VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE};
    if (m_useVertexInputDynamicState)
        dynamicStates.push_back(VK_DYNAMIC_STATE_VERTEX_INPUT_EXT);

    const VkPipelineDynamicStateCreateInfo dynamicStateInfo{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                            nullptr, 0u, (uint32_t)std::size(dynamicStates),
                                                            dynamicStates.data()};
    m_graphicsPipelineDS =
        makeGraphicsPipeline(vk, device, *pipelineLayout, *vs, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *fs,
                             *renderPass, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0, 0,
                             &vertexInputState, nullptr, nullptr, nullptr, nullptr, &dynamicStateInfo);

    auto cmdPool   = makeCommandPool(vk, device, queueFamilyIndex);
    auto cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *cmdBuffer);

    // for test to properly work we need to clear red channel with 0
    beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, scissors[0], tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    if (m_params.mode == CommandFlagTestMode::BASIC_SET_STRIDE)
        drawUsingBasicSetStride(*cmdBuffer);
    else if (m_params.mode == CommandFlagTestMode::COMPLEX_SET_STRIDE)
        drawUsingComplexSetStride(*cmdBuffer);
    else // CommandFlagTestMode::USE_ALL_VERTEX_INDEX_BINDS
        drawUsingAllVertexIndexBinds(*cmdBuffer);

    endRenderPass(vk, *cmdBuffer);

    tcu::IVec2 imageSize(imageExtent.width, imageExtent.height);
    copyImageToBuffer(vk, *cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), imageSize,
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    // verify result
    const Allocation &bufferAllocation = colorBuffer.getBufferAllocation();
    invalidateAlloc(vk, device, bufferAllocation);
    tcu::PixelBufferAccess resultAccess(mapVkFormat(format), imageExtent.width, imageExtent.height, 1,
                                        bufferAllocation.getHostPtr());

    // check red channel of specified number of fragments to see if all quads were drawn correctly
    bool testPassed = true;
    for (size_t i = 0; i < bufferCount * 2; i++)
    {
        // grab every second fragment to check if all three quads are rendered correctly;
        // in a row we expect to have 4 red fragments and 4 black fragments interleaved three times
        uint32_t redResult = resultAccess.getPixelUint((int32_t)i * 4 + 1, imageExtent.height / 2)[0];
        if (i % 2)
        {
            // for odd indexed fragment pair we expect to have red channel value equal to 0 (clear color);
            // there are gaps between quas so that drawing full screen red quad wont pass the test and
            // so that we can clearly see which draw was actually correctly drawn
            testPassed &= (redResult < 2);
        }
        else
        {
            // for even indexed fragment pair we expect to have red value equal to 255 (quad color)
            testPassed &= (redResult > 253);
        }
    }

    if (testPassed)
        return tcu::TestStatus::pass("Pass");

    m_context.getTestContext().getLog() << tcu::TestLog::Image("Result", "", resultAccess);
    return tcu::TestStatus::fail("Fail");
}

void VertexIndexBindingTestInstance::setVertexInput(VkCommandBuffer cmdBuffer, VkDeviceSize stride) const
{
    const DeviceInterface &vk = m_context.getDeviceInterface();

    VkVertexInputBindingDescription2EXT vertexBindingDescriptions     = initVulkanStructure();
    vertexBindingDescriptions.stride                                  = (uint32_t)stride;
    vertexBindingDescriptions.inputRate                               = VK_VERTEX_INPUT_RATE_VERTEX;
    vertexBindingDescriptions.divisor                                 = 1;
    VkVertexInputAttributeDescription2EXT vertexAttributeDescriptions = initVulkanStructure();
    vertexAttributeDescriptions.format                                = VK_FORMAT_R32G32_SFLOAT;
    vertexAttributeDescriptions.offset                                = 0;

    vk.cmdSetVertexInputEXT(cmdBuffer, 1, &vertexBindingDescriptions, 1, &vertexAttributeDescriptions);
}

void VertexIndexBindingTestInstance::drawUsingBasicSetStride(VkCommandBuffer cmdBuffer) const
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    auto device               = m_context.getDevice();

    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineDS);

    VkBuffer vertexBuffer                        = **m_vertexBuffers[0];
    auto vertexBufferAddress                     = getBufferDeviceAddress(vk, device, vertexBuffer);
    VkBindVertexBuffer3InfoKHR vertexBuffer3Info = initVulkanStructure();
    vertexBuffer3Info.addressRange               = {vertexBufferAddress, m_vertexBufferSizes[0], 24};
    vertexBuffer3Info.setStride                  = true;

    vk.cmdBindVertexBuffers3KHR(cmdBuffer, 0, 1, &vertexBuffer3Info);
    vk.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);

    // we set stride to 0 but expect 16 will be used
    vertexBuffer                   = **m_vertexBuffers[1];
    vertexBufferAddress            = getBufferDeviceAddress(vk, device, vertexBuffer);
    vertexBuffer3Info.addressRange = {vertexBufferAddress, m_vertexBufferSizes[1], 0};
    vertexBuffer3Info.setStride    = false;

    vk.cmdBindVertexBuffers3KHR(cmdBuffer, 0, 1, &vertexBuffer3Info);
    vk.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
}

void VertexIndexBindingTestInstance::drawUsingComplexSetStride(VkCommandBuffer cmdBuffer) const
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    auto device               = m_context.getDevice();

    const VkDeviceSize vertexStrideA = (2u + m_unusedVertexFloats[0]) * sizeof(float);
    const VkDeviceSize vertexStrideB = (2u + m_unusedVertexFloats[1]) * sizeof(float);
    const VkDeviceSize vertexStrideC = (2u + m_unusedVertexFloats[2]) * sizeof(float);

    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineDS);

    // use vkCmdSetVertexInputEXT to set stride to vertexStrideA
    VkDeviceSize vertexOffset = 0;
    VkBuffer vertexBuffer     = **m_vertexBuffers[0];
    setVertexInput(cmdBuffer, vertexStrideA);
    vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, &vertexOffset);
    vk.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);

    // use vkCmdBindVertexBuffers2 to set stride to vertexStrideB
    vertexBuffer = **m_vertexBuffers[1];
    vk.cmdBindVertexBuffers2(cmdBuffer, 0u, 1u, &vertexBuffer, &vertexOffset, &m_vertexBufferSizes[1], &vertexStrideB);
    vk.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);

    // use vkCmdBindVertexBuffers3 to set stride to vertexStrideC
    vertexBuffer                                 = **m_vertexBuffers[2];
    auto vertexBufferAddress                     = getBufferDeviceAddress(vk, device, vertexBuffer);
    VkBindVertexBuffer3InfoKHR vertexBuffer3Info = initVulkanStructure();
    vertexBuffer3Info.addressRange               = {vertexBufferAddress, m_vertexBufferSizes[2], vertexStrideC};
    vertexBuffer3Info.setStride                  = true;
    vk.cmdBindVertexBuffers3KHR(cmdBuffer, 0, 1, &vertexBuffer3Info);
    vk.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);

    // use vkCmdBindVertexBuffers2 again to set stride to vertexStrideB
    vertexBuffer = **m_vertexBuffers[3];
    vk.cmdBindVertexBuffers2(cmdBuffer, 0u, 1u, &vertexBuffer, &vertexOffset, &m_vertexBufferSizes[3], &vertexStrideB);
    vk.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);

    // use cmdBindVertexBuffers again to set stride to vertexStrideA
    vertexBuffer = **m_vertexBuffers[4];
    setVertexInput(cmdBuffer, vertexStrideA);
    vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, &vertexOffset);
    vk.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);

    // use vkCmdBindVertexBuffers3 with setStride false; we expect that strideA will be used
    vertexBuffer                   = **m_vertexBuffers[5];
    vertexBufferAddress            = getBufferDeviceAddress(vk, device, vertexBuffer);
    vertexBuffer3Info.setStride    = false;
    vertexBuffer3Info.addressRange = {vertexBufferAddress, m_vertexBufferSizes[5], 0};
    vk.cmdBindVertexBuffers3KHR(cmdBuffer, 0, 1, &vertexBuffer3Info);
    vk.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
}

void VertexIndexBindingTestInstance::drawUsingAllVertexIndexBinds(VkCommandBuffer cmdBuffer) const
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    auto device               = m_context.getDevice();

    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline);

    // draw first quad using v1 vertex and index buffer bind commands
    VkDeviceSize vertexOffset = 0;
    VkBuffer vertexBuffer     = **m_vertexBuffers[0];
    VkBuffer indexBuffer      = **m_indexBuffers[0];
    vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, &vertexOffset);
    vk.cmdBindIndexBuffer(cmdBuffer, **m_indexBuffers[0], 0, VK_INDEX_TYPE_UINT32);
    vk.cmdDrawIndexed(cmdBuffer, 4u, 1u, 0u, 0u, 0u);

    vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineDS);

    // draw second quad using v2 vertex and index buffer bind commands;
    // note that index buffer deliberately uses offset to make sure prober buffer is used
    vertexBuffer               = **m_vertexBuffers[1];
    indexBuffer                = **m_indexBuffers[1];
    VkDeviceSize indexDataSize = 4u * sizeof(uint32_t);
    VkDeviceSize vertexStride  = 2 * sizeof(float);
    vk.cmdBindVertexBuffers2(cmdBuffer, 0u, 1u, &vertexBuffer, &vertexOffset, &m_vertexBufferSizes[1], &vertexStride);
    vk.cmdBindIndexBuffer2(cmdBuffer, indexBuffer, sizeof(uint32_t), indexDataSize, VK_INDEX_TYPE_UINT32);
    vk.cmdDrawIndexed(cmdBuffer, 4u, 1u, 0u, 0u, 0u);

    // draw third quad using v3 vertex and index buffer bind commands
    vertexBuffer                                   = **m_vertexBuffers[2];
    indexBuffer                                    = **m_indexBuffers[2];
    auto vertexBufferAddress                       = getBufferDeviceAddress(vk, device, vertexBuffer);
    auto indexBufferAddress                        = getBufferDeviceAddress(vk, device, indexBuffer);
    VkBindVertexBuffer3InfoKHR vertexBuffer3Info   = initVulkanStructure();
    vertexBuffer3Info.addressRange                 = {vertexBufferAddress, m_vertexBufferSizes[2], 0};
    VkBindIndexBuffer3InfoKHR bindIndexBuffer3Info = initVulkanStructure();
    bindIndexBuffer3Info.addressRange              = {indexBufferAddress + 2 * sizeof(uint32_t), indexDataSize};
    bindIndexBuffer3Info.indexType                 = VK_INDEX_TYPE_UINT32;
    vk.cmdBindVertexBuffers3KHR(cmdBuffer, 0, 1, &vertexBuffer3Info);
    vk.cmdBindIndexBuffer3KHR(cmdBuffer, &bindIndexBuffer3Info);
    vk.cmdDrawIndexed(cmdBuffer, 4u, 1u, 0u, 0u, 0u);
}

class MemoryRangeBarrierBetweenOperationsTestInstance : public vkt::TestInstance
{
public:
    MemoryRangeBarrierBetweenOperationsTestInstance(Context &context);
    ~MemoryRangeBarrierBetweenOperationsTestInstance() = default;

    tcu::TestStatus iterate(void) override;
};

MemoryRangeBarrierBetweenOperationsTestInstance::MemoryRangeBarrierBetweenOperationsTestInstance(Context &context)
    : vkt::TestInstance(context)
{
}

tcu::TestStatus MemoryRangeBarrierBetweenOperationsTestInstance::iterate()
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();
    const uint32_t groupCount = 16;

    VkDeviceSize outputOffset = 16; // test non-zero offset in buffer address commands
    VkDeviceSize outputSize   = groupCount * sizeof(uint32_t);
    VkDeviceSize bufferSize   = outputSize + outputOffset;
    auto usage                = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    MemoryRequirement memReq = MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress;

    auto bufferCreateInfo = makeBufferCreateInfo(bufferSize, usage);
    const BufferWithMemory outputBuffer(vk, device, allocator, bufferCreateInfo, memReq);
    auto outputBufferAddress = getBufferDeviceAddress(vk, device, *outputBuffer);

    // create descriptor set
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const auto descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
    const auto outputBufferDescriptorInfo(makeDescriptorBufferInfo(*outputBuffer, outputOffset, outputSize));

    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputBufferDescriptorInfo)
        .update(vk, device);

    auto &bc               = m_context.getBinaryCollection();
    const auto compModule0 = createShaderModule(vk, device, bc.get("comp0"));
    const auto compModule1 = createShaderModule(vk, device, bc.get("comp1"));

    const auto pipelineLayout = makePipelineLayout(vk, device, *descriptorSetLayout);
    const auto pipeline0      = makeComputePipeline(vk, device, *pipelineLayout, *compModule0);
    const auto pipeline1      = makeComputePipeline(vk, device, *pipelineLayout, *compModule1);

    VkMemoryRangeBarrierKHR memoryRangeBarrier{VK_STRUCTURE_TYPE_MEMORY_RANGE_BARRIER_KHR,
                                               nullptr,
                                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
                                               VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
                                               VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                                               VK_QUEUE_FAMILY_IGNORED,
                                               VK_QUEUE_FAMILY_IGNORED,
                                               {outputBufferAddress + outputOffset, outputSize},
                                               VK_ADDRESS_COMMAND_STORAGE_BUFFER_USAGE_BIT_KHR};

    VkMemoryRangeBarriersInfoKHR memoryRangeBarriersInfo = initVulkanStructure();
    memoryRangeBarriersInfo.memoryRangeBarrierCount      = 1u;
    memoryRangeBarriersInfo.pMemoryRangeBarriers         = &memoryRangeBarrier;
    VkDependencyInfo depInfo                             = initVulkanStructure(&memoryRangeBarriersInfo);

    auto bpc = VK_PIPELINE_BIND_POINT_COMPUTE;
    const auto cmdPool(makeCommandPool(vk, device, m_context.getUniversalQueueFamilyIndex()));
    const auto cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vk, *cmdBuffer);

    vk.cmdBindDescriptorSets(*cmdBuffer, bpc, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u, nullptr);

    vk.cmdBindPipeline(*cmdBuffer, bpc, *pipeline0);
    vk.cmdDispatch(*cmdBuffer, groupCount, 1, 1);

    // test that memory range barrier properly works between two dispatches
    vk.cmdPipelineBarrier2(*cmdBuffer, &depInfo);

    vk.cmdBindPipeline(*cmdBuffer, bpc, *pipeline1);
    vk.cmdDispatch(*cmdBuffer, 1, groupCount, 1);

    memoryRangeBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
    memoryRangeBarrier.dstStageMask  = VK_PIPELINE_STAGE_2_HOST_BIT_KHR;
    memoryRangeBarrier.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT_KHR;
    vk.cmdPipelineBarrier2(*cmdBuffer, &depInfo);

    endCommandBuffer(vk, *cmdBuffer);

    VkCommandBufferSubmitInfoKHR cmdBufferInfo = initVulkanStructure();
    cmdBufferInfo.commandBuffer                = *cmdBuffer;
    VkSubmitInfo2 submitInfo2                  = initVulkanStructure();
    submitInfo2.commandBufferInfoCount         = 1;
    submitInfo2.pCommandBufferInfos            = &cmdBufferInfo;

    auto queue = m_context.getUniversalQueue();
    vk.queueSubmit2(queue, 1u, &submitInfo2, VK_NULL_HANDLE);
    vk.queueWaitIdle(queue);

    const Allocation &outputAllocation = outputBuffer.getAllocation();
    invalidateAlloc(vk, device, outputAllocation);

    auto &log                 = m_context.getTestContext().getLog();
    const uint32_t *bufferPtr = static_cast<uint32_t *>(outputAllocation.getHostPtr());
    bufferPtr += (outputOffset / sizeof(uint32_t));
    for (uint32_t i = 0; i < groupCount; ++i)
    {
        uint32_t expected = i + 3;
        if (bufferPtr[i] != expected)
        {
            log << tcu::TestLog::Message << "bufferPtr[" << i << "] = " << bufferPtr[i] << ", expected " << expected
                << tcu::TestLog::EndMessage;
            return tcu::TestStatus::fail("Fail");
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class BufferAddressCommandTestCase : public vkt::TestCase
{
public:
    BufferAddressCommandTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }

    virtual ~BufferAddressCommandTestCase(void) = default;

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;

private:
    TestParams m_params;
};

void BufferAddressCommandTestCase::checkSupport(Context &context) const
{
    using TM = CommandFlagTestMode;

    context.requireDeviceFunctionality("VK_KHR_device_address_commands");
    if (m_params.mode == TM::COPY_TO_MEMORY_WITH_UNBOUND_RANGES || m_params.mode == TM::COMPLEX_SET_STRIDE)
        context.requireDeviceFunctionality("VK_EXT_vertex_input_dynamic_state");
    else if (m_params.mode == TM::MEMORY_RANGE_BARRIER)
        context.requireDeviceFunctionality("VK_KHR_synchronization2");

    if ((m_params.mode == TM::COPY_TO_MEMORY_WITH_UNBOUND_RANGES) ||
        (m_params.mode == TM::COPY_FROM_MEMORY_WITH_UNBOUND_RANGES))
    {
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_BUFFER);
    }
}

void BufferAddressCommandTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    auto &glslSources = programCollection.glslSources;

    if ((m_params.mode == CommandFlagTestMode::USE_ALL_VERTEX_INDEX_BINDS) ||
        (m_params.mode == CommandFlagTestMode::BASIC_SET_STRIDE) ||
        (m_params.mode == CommandFlagTestMode::COMPLEX_SET_STRIDE))
    {
        glslSources.add("vert") << glu::VertexSource(R"(
            #version 450
            layout(location = 0) in vec2 position;
            void main()
            {
                gl_Position = vec4(position, 0.0, 1.0);
            })");
        glslSources.add("frag") << glu::FragmentSource(R"(
            #version 450
            layout(location = 0) out vec4 out_color;
            void main()
            {
                out_color = vec4(1.0, 0.0, 0.0, 1.0);
            })");
    }
    else if (m_params.mode == CommandFlagTestMode::MEMORY_RANGE_BARRIER)
    {
        glslSources.add("comp0") << glu::ComputeSource(R"(
            #version 450
            layout (local_size_x = 1) in;
            layout(binding = 0) writeonly buffer Output {
                uint v[];
            };
            void main (void) {
                v[gl_WorkGroupID.x] = gl_WorkGroupID.x;
            })");

        glslSources.add("comp1") << glu::ComputeSource(R"(
            #version 450
            layout (local_size_x = 1) in;
            layout(binding = 0) buffer Output {
                uint v[];
            };
            void main (void) {
                v[gl_WorkGroupID.y] += 3;
            })");
    }
}

TestInstance *BufferAddressCommandTestCase::createInstance(Context &context) const
{
    if ((m_params.mode == CommandFlagTestMode::COPY_TO_MEMORY_WITH_UNBOUND_RANGES) ||
        (m_params.mode == CommandFlagTestMode::COPY_FROM_MEMORY_WITH_UNBOUND_RANGES))
        return new BufferAddressCommandFlagsTestInstance(context, m_params);

    if (m_params.mode == CommandFlagTestMode::MEMORY_RANGE_BARRIER)
        return new MemoryRangeBarrierBetweenOperationsTestInstance(context);

    return new VertexIndexBindingTestInstance(context, m_params);
}

} // namespace

tcu::TestCaseGroup *createDeviceAddressCommandsTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> mainGroup(new tcu::TestCaseGroup(testCtx, "device_address"));
    de::MovePtr<tcu::TestCaseGroup> miscTests(new tcu::TestCaseGroup(testCtx, "misc"));

    using TM = CommandFlagTestMode;
    const std::vector<std::pair<std::string, TM>> caseVect{
        {"copy_to_memory_with_unbound_ranges", TM::COPY_TO_MEMORY_WITH_UNBOUND_RANGES},
        {"copy_from_memory_with_unbound_ranges", TM::COPY_FROM_MEMORY_WITH_UNBOUND_RANGES},
        {"use_all_vertex_index_binds", TM::USE_ALL_VERTEX_INDEX_BINDS},
        {"basic_set_stride", TM::BASIC_SET_STRIDE},
        {"complex_set_stride", TM::COMPLEX_SET_STRIDE},
        {"memory_range_barrier", TM::MEMORY_RANGE_BARRIER}};
    for (auto &[name, mode] : caseVect)
    {
        TestParams params{
            mode,
        };
        miscTests->addChild(new BufferAddressCommandTestCase(testCtx, name, params));
    }
    mainGroup->addChild(miscTests.release());

    return mainGroup.release();
}

} // namespace vkt::api
