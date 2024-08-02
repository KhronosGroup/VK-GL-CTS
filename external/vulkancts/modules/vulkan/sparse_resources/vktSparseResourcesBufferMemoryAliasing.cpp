/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \file  vktSparseResourcesBufferMemoryAliasing.cpp
 * \brief Sparse buffer memory aliasing tests
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesBufferMemoryAliasing.hpp"
#include "vktSparseResourcesTestsUtil.hpp"
#include "vktSparseResourcesBase.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include <string>
#include <vector>

using namespace vk;

namespace vkt
{
namespace sparse
{
namespace
{

enum ShaderParameters
{
    SIZE_OF_UINT_IN_SHADER = 4u,
    MODULO_DIVISOR         = 1024u
};

tcu::UVec3 computeWorkGroupSize(const uint32_t numInvocations)
{
    const uint32_t maxComputeWorkGroupInvocations = 128u;
    const tcu::UVec3 maxComputeWorkGroupSize      = tcu::UVec3(128u, 128u, 64u);
    uint32_t numInvocationsLeft                   = numInvocations;

    const uint32_t xWorkGroupSize =
        std::min(std::min(numInvocationsLeft, maxComputeWorkGroupSize.x()), maxComputeWorkGroupInvocations);
    numInvocationsLeft = numInvocationsLeft / xWorkGroupSize + ((numInvocationsLeft % xWorkGroupSize) ? 1u : 0u);

    const uint32_t yWorkGroupSize = std::min(std::min(numInvocationsLeft, maxComputeWorkGroupSize.y()),
                                             maxComputeWorkGroupInvocations / xWorkGroupSize);
    numInvocationsLeft = numInvocationsLeft / yWorkGroupSize + ((numInvocationsLeft % yWorkGroupSize) ? 1u : 0u);

    const uint32_t zWorkGroupSize = std::min(std::min(numInvocationsLeft, maxComputeWorkGroupSize.z()),
                                             maxComputeWorkGroupInvocations / (xWorkGroupSize * yWorkGroupSize));
    numInvocationsLeft = numInvocationsLeft / zWorkGroupSize + ((numInvocationsLeft % zWorkGroupSize) ? 1u : 0u);

    return tcu::UVec3(xWorkGroupSize, yWorkGroupSize, zWorkGroupSize);
}

class BufferSparseMemoryAliasingCase : public TestCase
{
public:
    BufferSparseMemoryAliasingCase(tcu::TestContext &testCtx, const std::string &name, const uint32_t bufferSize,
                                   const glu::GLSLVersion glslVersion, const bool useDeviceGroups);

    void initPrograms(SourceCollections &sourceCollections) const;
    TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    const uint32_t m_bufferSizeInBytes;
    const glu::GLSLVersion m_glslVersion;
    const bool m_useDeviceGroups;
};

BufferSparseMemoryAliasingCase::BufferSparseMemoryAliasingCase(tcu::TestContext &testCtx, const std::string &name,
                                                               const uint32_t bufferSize,
                                                               const glu::GLSLVersion glslVersion,
                                                               const bool useDeviceGroups)
    : TestCase(testCtx, name)
    , m_bufferSizeInBytes(bufferSize)
    , m_glslVersion(glslVersion)
    , m_useDeviceGroups(useDeviceGroups)
{
}

void BufferSparseMemoryAliasingCase::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_ALIASED);
}

void BufferSparseMemoryAliasingCase::initPrograms(SourceCollections &sourceCollections) const
{
    // Create compute program
    const char *const versionDecl  = glu::getGLSLVersionDeclaration(m_glslVersion);
    const uint32_t numInvocations  = m_bufferSizeInBytes / SIZE_OF_UINT_IN_SHADER;
    const tcu::UVec3 workGroupSize = computeWorkGroupSize(numInvocations);

    std::ostringstream src;
    src << versionDecl << "\n"
        << "layout (local_size_x = " << workGroupSize.x() << ", local_size_y = " << workGroupSize.y()
        << ", local_size_z = " << workGroupSize.z() << ") in;\n"
        << "layout(set = 0, binding = 0, std430) writeonly buffer Output\n"
        << "{\n"
        << "    uint result[];\n"
        << "} sb_out;\n"
        << "\n"
        << "void main (void)\n"
        << "{\n"
        << "    uint index = gl_GlobalInvocationID.x + (gl_GlobalInvocationID.y + "
           "gl_GlobalInvocationID.z*gl_NumWorkGroups.y*gl_WorkGroupSize.y)*gl_NumWorkGroups.x*gl_WorkGroupSize.x;\n"
        << "    if ( index < " << m_bufferSizeInBytes / SIZE_OF_UINT_IN_SHADER << "u )\n"
        << "    {\n"
        << "        sb_out.result[index] = index % " << MODULO_DIVISOR << "u;\n"
        << "    }\n"
        << "}\n";

    sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

class BufferSparseMemoryAliasingInstance : public SparseResourcesBaseInstance
{
public:
    BufferSparseMemoryAliasingInstance(Context &context, const uint32_t bufferSize, const bool useDeviceGroups);

    tcu::TestStatus iterate(void);

private:
    const uint32_t m_bufferSizeInBytes;
    const uint32_t m_useDeviceGroups;
};

BufferSparseMemoryAliasingInstance::BufferSparseMemoryAliasingInstance(Context &context, const uint32_t bufferSize,
                                                                       const bool useDeviceGroups)
    : SparseResourcesBaseInstance(context, useDeviceGroups)
    , m_bufferSizeInBytes(bufferSize)
    , m_useDeviceGroups(useDeviceGroups)
{
}

tcu::TestStatus BufferSparseMemoryAliasingInstance::iterate(void)
{
    const InstanceInterface &instance = m_context.getInstanceInterface();
    {
        // Create logical device supporting both sparse and compute operations
        QueueRequirementsVec queueRequirements;
        queueRequirements.push_back(QueueRequirements(VK_QUEUE_SPARSE_BINDING_BIT, 1u));
        queueRequirements.push_back(QueueRequirements(VK_QUEUE_COMPUTE_BIT, 1u));

        createDeviceSupportingQueues(queueRequirements);
    }
    const vk::VkPhysicalDevice &physicalDevice = getPhysicalDevice();
    const DeviceInterface &deviceInterface     = getDeviceInterface();
    const Queue &sparseQueue                   = getQueue(VK_QUEUE_SPARSE_BINDING_BIT, 0);
    const Queue &computeQueue                  = getQueue(VK_QUEUE_COMPUTE_BIT, 0);

    // Go through all physical devices
    for (uint32_t physDevID = 0; physDevID < m_numPhysicalDevices; physDevID++)
    {
        const uint32_t firstDeviceID  = physDevID;
        const uint32_t secondDeviceID = (firstDeviceID + 1) % m_numPhysicalDevices;

        VkBufferCreateInfo bufferCreateInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,                                      // VkStructureType sType;
            nullptr,                                                                   // const void* pNext;
            VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_ALIASED_BIT, // VkBufferCreateFlags flags;
            m_bufferSizeInBytes,                                                       // VkDeviceSize size;
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,     // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                                 // VkSharingMode sharingMode;
            0u,     // uint32_t queueFamilyIndexCount;
            nullptr // const uint32_t* pQueueFamilyIndices;
        };

        const uint32_t queueFamilyIndices[] = {sparseQueue.queueFamilyIndex, computeQueue.queueFamilyIndex};

        if (sparseQueue.queueFamilyIndex != computeQueue.queueFamilyIndex)
        {
            bufferCreateInfo.sharingMode           = VK_SHARING_MODE_CONCURRENT;
            bufferCreateInfo.queueFamilyIndexCount = 2u;
            bufferCreateInfo.pQueueFamilyIndices   = queueFamilyIndices;
        }

        // Create sparse buffers
        const Unique<VkBuffer> sparseBufferWrite(createBuffer(deviceInterface, getDevice(), &bufferCreateInfo));
        const Unique<VkBuffer> sparseBufferRead(createBuffer(deviceInterface, getDevice(), &bufferCreateInfo));

        // Create sparse buffers memory bind semaphore
        const Unique<VkSemaphore> bufferMemoryBindSemaphore(createSemaphore(deviceInterface, getDevice()));

        const VkMemoryRequirements bufferMemRequirements =
            getBufferMemoryRequirements(deviceInterface, getDevice(), *sparseBufferWrite);

        if (bufferMemRequirements.size >
            getPhysicalDeviceProperties(instance, physicalDevice).limits.sparseAddressSpaceSize)
            TCU_THROW(NotSupportedError, "Required memory size for sparse resources exceeds device limits");

        DE_ASSERT((bufferMemRequirements.size % bufferMemRequirements.alignment) == 0);

        const uint32_t memoryType = findMatchingMemoryType(instance, getPhysicalDevice(secondDeviceID),
                                                           bufferMemRequirements, MemoryRequirement::Any);

        if (memoryType == NO_MATCH_FOUND)
            return tcu::TestStatus::fail("No matching memory type found");

        if (firstDeviceID != secondDeviceID)
        {
            VkPeerMemoryFeatureFlags peerMemoryFeatureFlags = (VkPeerMemoryFeatureFlags)0;
            const uint32_t heapIndex =
                getHeapIndexForMemoryType(instance, getPhysicalDevice(secondDeviceID), memoryType);
            deviceInterface.getDeviceGroupPeerMemoryFeatures(getDevice(), heapIndex, firstDeviceID, secondDeviceID,
                                                             &peerMemoryFeatureFlags);

            if (((peerMemoryFeatureFlags & VK_PEER_MEMORY_FEATURE_COPY_SRC_BIT) == 0) ||
                ((peerMemoryFeatureFlags & VK_PEER_MEMORY_FEATURE_GENERIC_DST_BIT) == 0))
            {
                TCU_THROW(NotSupportedError, "Peer memory does not support COPY_SRC and GENERIC_DST");
            }
        }

        const VkSparseMemoryBind sparseMemoryBind =
            makeSparseMemoryBind(deviceInterface, getDevice(), bufferMemRequirements.size, memoryType, 0u);

        Move<VkDeviceMemory> deviceMemoryPtr(check<VkDeviceMemory>(sparseMemoryBind.memory),
                                             Deleter<VkDeviceMemory>(deviceInterface, getDevice(), nullptr));

        {
            const VkSparseBufferMemoryBindInfo sparseBufferMemoryBindInfo[2] = {
                makeSparseBufferMemoryBindInfo(*sparseBufferWrite, //VkBuffer buffer;
                                               1u,                 //uint32_t bindCount;
                                               &sparseMemoryBind   //const VkSparseMemoryBind* Binds;
                                               ),

                makeSparseBufferMemoryBindInfo(*sparseBufferRead, //VkBuffer buffer;
                                               1u,                //uint32_t bindCount;
                                               &sparseMemoryBind  //const VkSparseMemoryBind* Binds;
                                               )};

            const VkDeviceGroupBindSparseInfo devGroupBindSparseInfo = {
                VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO, //VkStructureType sType;
                nullptr,                                         //const void* pNext;
                firstDeviceID,                                   //uint32_t resourceDeviceIndex;
                secondDeviceID,                                  //uint32_t memoryDeviceIndex;
            };

            const VkBindSparseInfo bindSparseInfo = {
                VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,                    //VkStructureType sType;
                m_useDeviceGroups ? &devGroupBindSparseInfo : nullptr, //const void* pNext;
                0u,                                                    //uint32_t waitSemaphoreCount;
                nullptr,                                               //const VkSemaphore* pWaitSemaphores;
                2u,                                                    //uint32_t bufferBindCount;
                sparseBufferMemoryBindInfo,      //const VkSparseBufferMemoryBindInfo* pBufferBinds;
                0u,                              //uint32_t imageOpaqueBindCount;
                nullptr,                         //const VkSparseImageOpaqueMemoryBindInfo* pImageOpaqueBinds;
                0u,                              //uint32_t imageBindCount;
                nullptr,                         //const VkSparseImageMemoryBindInfo* pImageBinds;
                1u,                              //uint32_t signalSemaphoreCount;
                &bufferMemoryBindSemaphore.get() //const VkSemaphore* pSignalSemaphores;
            };

            // Submit sparse bind commands for execution
            VK_CHECK(deviceInterface.queueBindSparse(sparseQueue.queueHandle, 1u, &bindSparseInfo, VK_NULL_HANDLE));
        }

        // Create output buffer
        const VkBufferCreateInfo outputBufferCreateInfo =
            makeBufferCreateInfo(m_bufferSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const Unique<VkBuffer> outputBuffer(createBuffer(deviceInterface, getDevice(), &outputBufferCreateInfo));
        const de::UniquePtr<Allocation> outputBufferAlloc(
            bindBuffer(deviceInterface, getDevice(), getAllocator(), *outputBuffer, MemoryRequirement::HostVisible));

        // Create command buffer for compute and data transfer operations
        const Unique<VkCommandPool> commandPool(
            makeCommandPool(deviceInterface, getDevice(), computeQueue.queueFamilyIndex));
        const Unique<VkCommandBuffer> commandBuffer(
            allocateCommandBuffer(deviceInterface, getDevice(), *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

        // Start recording commands
        beginCommandBuffer(deviceInterface, *commandBuffer);

        // Create descriptor set
        const Unique<VkDescriptorSetLayout> descriptorSetLayout(
            DescriptorSetLayoutBuilder()
                .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                .build(deviceInterface, getDevice()));

        // Create compute pipeline
        const Unique<VkShaderModule> shaderModule(
            createShaderModule(deviceInterface, getDevice(), m_context.getBinaryCollection().get("comp"), 0));
        const Unique<VkPipelineLayout> pipelineLayout(
            makePipelineLayout(deviceInterface, getDevice(), *descriptorSetLayout));
        const Unique<VkPipeline> computePipeline(
            makeComputePipeline(deviceInterface, getDevice(), *pipelineLayout, *shaderModule));

        deviceInterface.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);

        // Create descriptor set
        const Unique<VkDescriptorPool> descriptorPool(
            DescriptorPoolBuilder()
                .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
                .build(deviceInterface, getDevice(), VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

        const Unique<VkDescriptorSet> descriptorSet(
            makeDescriptorSet(deviceInterface, getDevice(), *descriptorPool, *descriptorSetLayout));

        {
            const VkDescriptorBufferInfo sparseBufferInfo =
                makeDescriptorBufferInfo(*sparseBufferWrite, 0u, m_bufferSizeInBytes);

            DescriptorSetUpdateBuilder()
                .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &sparseBufferInfo)
                .update(deviceInterface, getDevice());
        }

        deviceInterface.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                                              &descriptorSet.get(), 0u, nullptr);

        {
            uint32_t numInvocationsLeft               = m_bufferSizeInBytes / SIZE_OF_UINT_IN_SHADER;
            const tcu::UVec3 workGroupSize            = computeWorkGroupSize(numInvocationsLeft);
            const tcu::UVec3 maxComputeWorkGroupCount = tcu::UVec3(65535u, 65535u, 65535u);

            numInvocationsLeft -= workGroupSize.x() * workGroupSize.y() * workGroupSize.z();

            const uint32_t xWorkGroupCount = std::min(numInvocationsLeft, maxComputeWorkGroupCount.x());
            numInvocationsLeft =
                numInvocationsLeft / xWorkGroupCount + ((numInvocationsLeft % xWorkGroupCount) ? 1u : 0u);
            const uint32_t yWorkGroupCount = std::min(numInvocationsLeft, maxComputeWorkGroupCount.y());
            numInvocationsLeft =
                numInvocationsLeft / yWorkGroupCount + ((numInvocationsLeft % yWorkGroupCount) ? 1u : 0u);
            const uint32_t zWorkGroupCount = std::min(numInvocationsLeft, maxComputeWorkGroupCount.z());
            numInvocationsLeft =
                numInvocationsLeft / zWorkGroupCount + ((numInvocationsLeft % zWorkGroupCount) ? 1u : 0u);

            if (numInvocationsLeft != 1u)
                TCU_THROW(NotSupportedError, "Buffer size is not supported");

            deviceInterface.cmdDispatch(*commandBuffer, xWorkGroupCount, yWorkGroupCount, zWorkGroupCount);
        }

        {
            const VkBufferMemoryBarrier sparseBufferWriteBarrier = makeBufferMemoryBarrier(
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, *sparseBufferWrite, 0ull, m_bufferSizeInBytes);

            deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                               VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 1u,
                                               &sparseBufferWriteBarrier, 0u, nullptr);
        }

        {
            const VkBufferCopy bufferCopy = makeBufferCopy(0u, 0u, m_bufferSizeInBytes);

            deviceInterface.cmdCopyBuffer(*commandBuffer, *sparseBufferRead, *outputBuffer, 1u, &bufferCopy);
        }

        {
            const VkBufferMemoryBarrier outputBufferHostBarrier = makeBufferMemoryBarrier(
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *outputBuffer, 0ull, m_bufferSizeInBytes);

            deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                               VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u,
                                               &outputBufferHostBarrier, 0u, nullptr);
        }

        // End recording commands
        endCommandBuffer(deviceInterface, *commandBuffer);

        // The stage at which execution is going to wait for finish of sparse binding operations
        const VkPipelineStageFlags waitStageBits[] = {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};

        // Submit commands for execution and wait for completion
        // In case of device groups, submit on the physical device with the resource
        submitCommandsAndWait(deviceInterface, getDevice(), computeQueue.queueHandle, *commandBuffer, 1u,
                              &bufferMemoryBindSemaphore.get(), waitStageBits, 0, nullptr, m_useDeviceGroups,
                              firstDeviceID);

        // Retrieve data from output buffer to host memory
        invalidateAlloc(deviceInterface, getDevice(), *outputBufferAlloc);

        const uint8_t *outputData = static_cast<const uint8_t *>(outputBufferAlloc->getHostPtr());

        // Wait for sparse queue to become idle
        deviceInterface.queueWaitIdle(sparseQueue.queueHandle);

        // Prepare reference data
        std::vector<uint8_t> referenceData;
        referenceData.resize(m_bufferSizeInBytes);

        std::vector<uint32_t> referenceDataBlock;
        referenceDataBlock.resize(MODULO_DIVISOR);

        for (uint32_t valueNdx = 0; valueNdx < MODULO_DIVISOR; ++valueNdx)
        {
            referenceDataBlock[valueNdx] = valueNdx % MODULO_DIVISOR;
        }

        const uint32_t fullBlockSizeInBytes = MODULO_DIVISOR * SIZE_OF_UINT_IN_SHADER;
        const uint32_t lastBlockSizeInBytes = m_bufferSizeInBytes % fullBlockSizeInBytes;
        const uint32_t numberOfBlocks = m_bufferSizeInBytes / fullBlockSizeInBytes + (lastBlockSizeInBytes ? 1u : 0u);

        for (uint32_t blockNdx = 0; blockNdx < numberOfBlocks; ++blockNdx)
        {
            const uint32_t offset = blockNdx * fullBlockSizeInBytes;
            deMemcpy(&referenceData[0] + offset, &referenceDataBlock[0],
                     ((offset + fullBlockSizeInBytes) <= m_bufferSizeInBytes) ? fullBlockSizeInBytes :
                                                                                lastBlockSizeInBytes);
        }

        // Compare reference data with output data
        if (deMemCmp(&referenceData[0], outputData, m_bufferSizeInBytes) != 0)
            return tcu::TestStatus::fail("Failed");
    }
    return tcu::TestStatus::pass("Passed");
}

TestInstance *BufferSparseMemoryAliasingCase::createInstance(Context &context) const
{
    return new BufferSparseMemoryAliasingInstance(context, m_bufferSizeInBytes, m_useDeviceGroups);
}

} // namespace

void addBufferSparseMemoryAliasingTests(tcu::TestCaseGroup *group, const bool useDeviceGroups)
{
    group->addChild(new BufferSparseMemoryAliasingCase(group->getTestContext(), "buffer_size_2_10", 1 << 10,
                                                       glu::GLSL_VERSION_440, useDeviceGroups));
    group->addChild(new BufferSparseMemoryAliasingCase(group->getTestContext(), "buffer_size_2_12", 1 << 12,
                                                       glu::GLSL_VERSION_440, useDeviceGroups));
    group->addChild(new BufferSparseMemoryAliasingCase(group->getTestContext(), "buffer_size_2_16", 1 << 16,
                                                       glu::GLSL_VERSION_440, useDeviceGroups));
    group->addChild(new BufferSparseMemoryAliasingCase(group->getTestContext(), "buffer_size_2_17", 1 << 17,
                                                       glu::GLSL_VERSION_440, useDeviceGroups));
    group->addChild(new BufferSparseMemoryAliasingCase(group->getTestContext(), "buffer_size_2_20", 1 << 20,
                                                       glu::GLSL_VERSION_440, useDeviceGroups));
    group->addChild(new BufferSparseMemoryAliasingCase(group->getTestContext(), "buffer_size_2_24", 1 << 24,
                                                       glu::GLSL_VERSION_440, useDeviceGroups));
}

} // namespace sparse
} // namespace vkt
