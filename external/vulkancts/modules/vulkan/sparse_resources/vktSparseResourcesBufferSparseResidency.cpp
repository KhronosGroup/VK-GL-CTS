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
 * \file  vktSparseResourcesBufferSparseResidency.cpp
 * \brief Sparse partially resident buffers tests
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesBufferSparseResidency.hpp"
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
};

enum BufferInitCommand
{
    BUFF_INIT_COPY   = 0,
    BUFF_INIT_FILL   = 1,
    BUFF_INIT_UPDATE = 2,
    BUFF_INIT_LAST   = 3
};

struct TestParams
{
    BufferInitCommand bufferInitCmd;
    bool withStrictResidency; // true = residencyNonResidentStrict enabled, false = residencyNonResidentStrict ignored
    bool isBufferNonResident; // true = completely non-resident, false = partially non-resident
    uint32_t bufferSize;
    bool isCopySrcSparse; // Applies only on copy command
                          // true = source buffer in the copy command is sparse,
                          // false = destination buffer in the copy command is sparse
    bool isMultiCopy;     // Applies only on copy command
};

struct TestPushConstants
{
    uint32_t bufferSize;
    uint32_t blockSize;
};

class BufferSparseResidencyCase : public TestCase
{
public:
    BufferSparseResidencyCase(tcu::TestContext &testCtx, const std::string &name, const uint32_t bufferSize,
                              const glu::GLSLVersion glslVersion, const bool useDeviceGroups);

    void initPrograms(SourceCollections &sourceCollections) const;
    TestInstance *createInstance(Context &context) const;

private:
    const uint32_t m_bufferSize;
    const glu::GLSLVersion m_glslVersion;
    const bool m_useDeviceGroups;
};

BufferSparseResidencyCase::BufferSparseResidencyCase(tcu::TestContext &testCtx, const std::string &name,
                                                     const uint32_t bufferSize, const glu::GLSLVersion glslVersion,
                                                     const bool useDeviceGroups)

    : TestCase(testCtx, name)
    , m_bufferSize(bufferSize)
    , m_glslVersion(glslVersion)
    , m_useDeviceGroups(useDeviceGroups)
{
}

void commonPrograms(SourceCollections &sourceCollections, const uint32_t bufferSize,
                    const glu::GLSLVersion glslVersion = glu::GLSL_VERSION_440)
{
    const char *const versionDecl  = glu::getGLSLVersionDeclaration(glslVersion);
    const uint32_t iterationsCount = bufferSize / SIZE_OF_UINT_IN_SHADER;

    std::ostringstream src;

    src << versionDecl << "\n"
        << "layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
        << "layout(set = 0, binding = 0, std430) readonly buffer Input\n"
        << "{\n"
        << "    uint data[];\n"
        << "} sb_in;\n"
        << "\n"
        << "layout(set = 0, binding = 1, std430) writeonly buffer Output\n"
        << "{\n"
        << "    uint result[];\n"
        << "} sb_out;\n"
        << "\n"
        << "void main (void)\n"
        << "{\n"
        << "    for(int i=0; i<" << iterationsCount << "; ++i) \n"
        << "    {\n"
        << "        sb_out.result[i] = sb_in.data[i];"
        << "    }\n"
        << "}\n";

    sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

void BufferSparseResidencyCase::initPrograms(SourceCollections &sourceCollections) const
{
    commonPrograms(sourceCollections, m_bufferSize, m_glslVersion);
}

class BufferSparseResidencyInstance : public SparseResourcesBaseInstance
{
public:
    BufferSparseResidencyInstance(Context &context, const uint32_t bufferSize, const bool useDeviceGroups);

    tcu::TestStatus iterate(void);

private:
    const uint32_t m_bufferSize;
};

BufferSparseResidencyInstance::BufferSparseResidencyInstance(Context &context, const uint32_t bufferSize,
                                                             const bool useDeviceGroups)
    : SparseResourcesBaseInstance(context, useDeviceGroups)
    , m_bufferSize(bufferSize)
{
}

tcu::TestStatus BufferSparseResidencyInstance::iterate(void)
{
    const InstanceInterface &instance = m_context.getInstanceInterface();
    {
        // Create logical device supporting both sparse and compute operations
        QueueRequirementsVec queueRequirements;
        queueRequirements.push_back(QueueRequirements(VK_QUEUE_SPARSE_BINDING_BIT, 1u));
        queueRequirements.push_back(QueueRequirements(VK_QUEUE_COMPUTE_BIT, 1u));

        createDeviceSupportingQueues(queueRequirements);
    }
    const VkPhysicalDevice physicalDevice                     = getPhysicalDevice();
    const VkPhysicalDeviceProperties physicalDeviceProperties = getPhysicalDeviceProperties(instance, physicalDevice);

    if (!getPhysicalDeviceFeatures(instance, physicalDevice).sparseResidencyBuffer)
        TCU_THROW(NotSupportedError, "Sparse partially resident buffers not supported");

    const DeviceInterface &deviceInterface = getDeviceInterface();
    const Queue &sparseQueue               = getQueue(VK_QUEUE_SPARSE_BINDING_BIT, 0);
    const Queue &computeQueue              = getQueue(VK_QUEUE_COMPUTE_BIT, 0);

    // Go through all physical devices
    for (uint32_t physDevID = 0; physDevID < m_numPhysicalDevices; physDevID++)
    {
        const uint32_t firstDeviceID  = physDevID;
        const uint32_t secondDeviceID = (firstDeviceID + 1) % m_numPhysicalDevices;

        VkBufferCreateInfo bufferCreateInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,                                        // VkStructureType sType;
            nullptr,                                                                     // const void* pNext;
            VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT, // VkBufferCreateFlags flags;
            m_bufferSize,                                                                // VkDeviceSize size;
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,       // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                                   // VkSharingMode sharingMode;
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

        // Create sparse buffer
        const Unique<VkBuffer> sparseBuffer(createBuffer(deviceInterface, getDevice(), &bufferCreateInfo));

        // Create sparse buffer memory bind semaphore
        const Unique<VkSemaphore> bufferMemoryBindSemaphore(createSemaphore(deviceInterface, getDevice()));

        const VkMemoryRequirements bufferMemRequirements =
            getBufferMemoryRequirements(deviceInterface, getDevice(), *sparseBuffer);

        if (bufferMemRequirements.size > physicalDeviceProperties.limits.sparseAddressSpaceSize)
            TCU_THROW(NotSupportedError, "Required memory size for sparse resources exceeds device limits");

        DE_ASSERT((bufferMemRequirements.size % bufferMemRequirements.alignment) == 0);

        const uint32_t numSparseSlots =
            static_cast<uint32_t>(bufferMemRequirements.size / bufferMemRequirements.alignment);
        std::vector<DeviceMemorySp> deviceMemUniquePtrVec;

        {
            std::vector<VkSparseMemoryBind> sparseMemoryBinds;
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

            for (uint32_t sparseBindNdx = 0; sparseBindNdx < numSparseSlots; sparseBindNdx += 2)
            {
                const VkSparseMemoryBind sparseMemoryBind =
                    makeSparseMemoryBind(deviceInterface, getDevice(), bufferMemRequirements.alignment, memoryType,
                                         bufferMemRequirements.alignment * sparseBindNdx);

                deviceMemUniquePtrVec.push_back(makeVkSharedPtr(
                    Move<VkDeviceMemory>(check<VkDeviceMemory>(sparseMemoryBind.memory),
                                         Deleter<VkDeviceMemory>(deviceInterface, getDevice(), nullptr))));

                sparseMemoryBinds.push_back(sparseMemoryBind);
            }

            const VkSparseBufferMemoryBindInfo sparseBufferBindInfo = makeSparseBufferMemoryBindInfo(
                *sparseBuffer, static_cast<uint32_t>(sparseMemoryBinds.size()), &sparseMemoryBinds[0]);

            const VkDeviceGroupBindSparseInfo devGroupBindSparseInfo = {
                VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO, //VkStructureType sType;
                nullptr,                                         //const void* pNext;
                firstDeviceID,                                   //uint32_t resourceDeviceIndex;
                secondDeviceID,                                  //uint32_t memoryDeviceIndex;
            };
            const VkBindSparseInfo bindSparseInfo = {
                VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,                      //VkStructureType sType;
                usingDeviceGroups() ? &devGroupBindSparseInfo : nullptr, //const void* pNext;
                0u,                                                      //uint32_t waitSemaphoreCount;
                nullptr,                                                 //const VkSemaphore* pWaitSemaphores;
                1u,                                                      //uint32_t bufferBindCount;
                &sparseBufferBindInfo,           //const VkSparseBufferMemoryBindInfo* pBufferBinds;
                0u,                              //uint32_t imageOpaqueBindCount;
                nullptr,                         //const VkSparseImageOpaqueMemoryBindInfo* pImageOpaqueBinds;
                0u,                              //uint32_t imageBindCount;
                nullptr,                         //const VkSparseImageMemoryBindInfo* pImageBinds;
                1u,                              //uint32_t signalSemaphoreCount;
                &bufferMemoryBindSemaphore.get() //const VkSemaphore* pSignalSemaphores;
            };

            VK_CHECK(deviceInterface.queueBindSparse(sparseQueue.queueHandle, 1u, &bindSparseInfo, VK_NULL_HANDLE));
        }

        // Create input buffer
        const VkBufferCreateInfo inputBufferCreateInfo =
            makeBufferCreateInfo(m_bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        const Unique<VkBuffer> inputBuffer(createBuffer(deviceInterface, getDevice(), &inputBufferCreateInfo));
        const de::UniquePtr<Allocation> inputBufferAlloc(
            bindBuffer(deviceInterface, getDevice(), getAllocator(), *inputBuffer, MemoryRequirement::HostVisible));

        std::vector<uint8_t> referenceData;
        referenceData.resize(m_bufferSize);

        for (uint32_t valueNdx = 0; valueNdx < m_bufferSize; ++valueNdx)
        {
            referenceData[valueNdx] = static_cast<uint8_t>((valueNdx % bufferMemRequirements.alignment) + 1u);
        }

        deMemcpy(inputBufferAlloc->getHostPtr(), &referenceData[0], m_bufferSize);

        flushAlloc(deviceInterface, getDevice(), *inputBufferAlloc);

        // Create output buffer
        const VkBufferCreateInfo outputBufferCreateInfo =
            makeBufferCreateInfo(m_bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const Unique<VkBuffer> outputBuffer(createBuffer(deviceInterface, getDevice(), &outputBufferCreateInfo));
        const de::UniquePtr<Allocation> outputBufferAlloc(
            bindBuffer(deviceInterface, getDevice(), getAllocator(), *outputBuffer, MemoryRequirement::HostVisible));

        // Create command buffer for compute and data transfer operations
        const Unique<VkCommandPool> commandPool(
            makeCommandPool(deviceInterface, getDevice(), computeQueue.queueFamilyIndex));
        const Unique<VkCommandBuffer> commandBuffer(
            allocateCommandBuffer(deviceInterface, getDevice(), *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

        // Start recording compute and transfer commands
        beginCommandBuffer(deviceInterface, *commandBuffer);

        // Create descriptor set
        const Unique<VkDescriptorSetLayout> descriptorSetLayout(
            DescriptorSetLayoutBuilder()
                .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
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

        const Unique<VkDescriptorPool> descriptorPool(
            DescriptorPoolBuilder()
                .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u)
                .build(deviceInterface, getDevice(), VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

        const Unique<VkDescriptorSet> descriptorSet(
            makeDescriptorSet(deviceInterface, getDevice(), *descriptorPool, *descriptorSetLayout));

        {
            const VkDescriptorBufferInfo inputBufferInfo  = makeDescriptorBufferInfo(*inputBuffer, 0ull, m_bufferSize);
            const VkDescriptorBufferInfo sparseBufferInfo = makeDescriptorBufferInfo(*sparseBuffer, 0ull, m_bufferSize);

            DescriptorSetUpdateBuilder()
                .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &inputBufferInfo)
                .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &sparseBufferInfo)
                .update(deviceInterface, getDevice());
        }

        deviceInterface.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                                              &descriptorSet.get(), 0u, nullptr);

        {
            const VkBufferMemoryBarrier inputBufferBarrier = makeBufferMemoryBarrier(
                VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *inputBuffer, 0ull, m_bufferSize);

            deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_HOST_BIT,
                                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 1u,
                                               &inputBufferBarrier, 0u, nullptr);
        }

        deviceInterface.cmdDispatch(*commandBuffer, 1u, 1u, 1u);

        {
            const VkBufferMemoryBarrier sparseBufferBarrier = makeBufferMemoryBarrier(
                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, *sparseBuffer, 0ull, m_bufferSize);

            deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                               VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 1u,
                                               &sparseBufferBarrier, 0u, nullptr);
        }

        {
            const VkBufferCopy bufferCopy = makeBufferCopy(0u, 0u, m_bufferSize);

            deviceInterface.cmdCopyBuffer(*commandBuffer, *sparseBuffer, *outputBuffer, 1u, &bufferCopy);
        }

        {
            const VkBufferMemoryBarrier outputBufferBarrier = makeBufferMemoryBarrier(
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *outputBuffer, 0ull, m_bufferSize);

            deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                               VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u, &outputBufferBarrier,
                                               0u, nullptr);
        }

        // End recording compute and transfer commands
        endCommandBuffer(deviceInterface, *commandBuffer);

        const VkPipelineStageFlags waitStageBits[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};

        // Submit transfer commands for execution and wait for completion
        submitCommandsAndWait(deviceInterface, getDevice(), computeQueue.queueHandle, *commandBuffer, 1u,
                              &bufferMemoryBindSemaphore.get(), waitStageBits, 0, nullptr, usingDeviceGroups(),
                              firstDeviceID);

        // Retrieve data from output buffer to host memory
        invalidateAlloc(deviceInterface, getDevice(), *outputBufferAlloc);

        const uint8_t *outputData = static_cast<const uint8_t *>(outputBufferAlloc->getHostPtr());

        // Wait for sparse queue to become idle
        deviceInterface.queueWaitIdle(sparseQueue.queueHandle);

        // Compare output data with reference data
        for (uint32_t sparseBindNdx = 0; sparseBindNdx < numSparseSlots; ++sparseBindNdx)
        {
            const uint32_t alignment = static_cast<uint32_t>(bufferMemRequirements.alignment);
            const uint32_t offset    = alignment * sparseBindNdx;
            const uint32_t size      = sparseBindNdx == (numSparseSlots - 1) ? m_bufferSize % alignment : alignment;

            if (sparseBindNdx % 2u == 0u)
            {
                if (deMemCmp(&referenceData[offset], outputData + offset, size) != 0)
                    return tcu::TestStatus::fail("Failed");
            }
            else if (physicalDeviceProperties.sparseProperties.residencyNonResidentStrict)
            {
                deMemset(&referenceData[offset], 0u, size);

                if (deMemCmp(&referenceData[offset], outputData + offset, size) != 0)
                    return tcu::TestStatus::fail("Failed");
            }
        }
    }

    return tcu::TestStatus::pass("Passed");
}

TestInstance *BufferSparseResidencyCase::createInstance(Context &context) const
{
    return new BufferSparseResidencyInstance(context, m_bufferSize, m_useDeviceGroups);
}

class BufferSparseResidencyNonResidentCase : public TestCase
{
public:
    BufferSparseResidencyNonResidentCase(tcu::TestContext &testCtx, const std::string &name,
                                         const TestParams testParams);

    void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &sourceCollections) const;
    TestInstance *createInstance(Context &context) const;

private:
    void copyVerificationProgram(SourceCollections &sourceCollections) const;

    const TestParams m_testParams;
};

BufferSparseResidencyNonResidentCase::BufferSparseResidencyNonResidentCase(tcu::TestContext &testCtx,
                                                                           const std::string &name,
                                                                           const TestParams testParams)

    : TestCase(testCtx, name)
    , m_testParams(testParams)
{
}

void BufferSparseResidencyNonResidentCase::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);

    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_BUFFER);

    if (m_testParams.withStrictResidency && !context.getDeviceProperties().sparseProperties.residencyNonResidentStrict)
        TCU_THROW(NotSupportedError, "Property residencyNonResidentStrict is not supported");
}

void BufferSparseResidencyNonResidentCase::copyVerificationProgram(SourceCollections &sourceCollections) const
{
    std::ostringstream src;

    src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
        << "layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"

        << "layout(set = 0, binding = 0, std430) readonly buffer Input\n"
        << "{\n"
        << "    volatile uint data[];\n"
        << "} sbIn;\n"
        << "\n"
        << "layout(set = 0, binding = 1, std430) writeonly buffer Output\n"
        << "{\n"
        << "    bool result;\n"
        << "} sbOut;\n"
        << "\n"
        << "layout (push_constant, std430) uniform PushConstants\n"
        << "{\n"
        << "    uint bufferSize;\n"
        << "    uint blockSize;\n"
        << "} pc;\n"
        << "\n"
        << "void main (void)\n"
        << "{\n"
        << "    bool ok = true;\n"
        << "    uint bufferSizeInt = pc.bufferSize /" << SIZE_OF_UINT_IN_SHADER << ";\n"
        << "    uint blockSizeInt = pc.blockSize /" << SIZE_OF_UINT_IN_SHADER << ";\n"
        << "    for (uint offset = 0; offset < bufferSizeInt; offset += blockSizeInt)\n"
        << "    {\n"
        << "        uint val = sbIn.data[offset] & 0xFF;\n;";

    if (!m_testParams.isBufferNonResident)
    {
        src << "        uint idx = offset / blockSizeInt;\n"

            << "        if (mod(idx, 2) == 0)\n"
            << "            ok = ok && (val != 0);\n"
            << "        else\n";
    }

    src << "            ok = ok && (val == 0);\n"
        << "    }\n"
        << "\n"
        << "    sbOut.result = ok;\n"
        << "}\n";

    sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

void BufferSparseResidencyNonResidentCase::initPrograms(SourceCollections &sourceCollections) const
{
    if ((m_testParams.bufferInitCmd == BUFF_INIT_COPY) && !m_testParams.isCopySrcSparse)
        copyVerificationProgram(sourceCollections);
    else
        commonPrograms(sourceCollections, m_testParams.bufferSize);
}

class BufferSparseResidencyNonResidentInstance : public SparseResourcesBaseInstance
{
public:
    BufferSparseResidencyNonResidentInstance(Context &context, const TestParams testParams);

    tcu::TestStatus iterate(void);

private:
    const TestParams m_testParams;
};

BufferSparseResidencyNonResidentInstance::BufferSparseResidencyNonResidentInstance(Context &context,
                                                                                   const TestParams testParams)
    : SparseResourcesBaseInstance(context, false /*useDeviceGroups*/)
    , m_testParams(testParams)
{
}

tcu::TestStatus BufferSparseResidencyNonResidentInstance::iterate(void)
{
    const InstanceInterface &instance = m_context.getInstanceInterface();

    // Try to use transfer queue (if available) for copy, fill and update operations
    const VkQueueFlagBits cmdQueueBit =
        (m_testParams.bufferInitCmd == BUFF_INIT_COPY) ? VK_QUEUE_COMPUTE_BIT : VK_QUEUE_TRANSFER_BIT;

    // Initialize fill value for fill command
    const uint32_t fillValue = 0xAAAAAAAA;

    {
        // Create logical device supporting both sparse and compute operations
        QueueRequirementsVec queueRequirements;
        if (!m_testParams.isBufferNonResident)
            queueRequirements.push_back(QueueRequirements(VK_QUEUE_SPARSE_BINDING_BIT, 1u));

        queueRequirements.push_back(QueueRequirements(cmdQueueBit, 1u));

        createDeviceSupportingQueues(queueRequirements);
    }

    const VkPhysicalDevice physicalDevice                     = getPhysicalDevice();
    const VkPhysicalDeviceProperties physicalDeviceProperties = getPhysicalDeviceProperties(instance, physicalDevice);
    const DeviceInterface &deviceInterface                    = getDeviceInterface();

    VkBufferCreateInfo bufferCreateInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,                                        // VkStructureType sType;
        nullptr,                                                                     // const void* pNext;
        VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT, // VkBufferCreateFlags flags;
        m_testParams.bufferSize,                                                     // VkDeviceSize size;
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT, // VkBufferUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
        0u,                                   // uint32_t queueFamilyIndexCount;
        nullptr                               // const uint32_t* pQueueFamilyIndices;
    };

    uint32_t queueFamilyIndices[2]; // 0: sparse, 1: transfer or compute queue
    if (!m_testParams.isBufferNonResident)
    {
        const Queue &sparseQueue = getQueue(VK_QUEUE_SPARSE_BINDING_BIT, 0);
        const Queue &cmdQueue    = getQueue(cmdQueueBit, 0);

        queueFamilyIndices[0] = sparseQueue.queueFamilyIndex;
        queueFamilyIndices[1] = cmdQueue.queueFamilyIndex;

        if (sparseQueue.queueFamilyIndex != cmdQueue.queueFamilyIndex)
        {
            bufferCreateInfo.sharingMode           = VK_SHARING_MODE_CONCURRENT;
            bufferCreateInfo.queueFamilyIndexCount = 2u;
            bufferCreateInfo.pQueueFamilyIndices   = queueFamilyIndices;
        }
    }

    // Create sparse buffer
    const Unique<VkBuffer> sparseBuffer(createBuffer(deviceInterface, getDevice(), &bufferCreateInfo));

    const VkMemoryRequirements bufferMemRequirements =
        getBufferMemoryRequirements(deviceInterface, getDevice(), *sparseBuffer);

    if (!m_testParams.isBufferNonResident &&
        (bufferMemRequirements.size > physicalDeviceProperties.limits.sparseAddressSpaceSize))
        TCU_THROW(NotSupportedError, "Required memory size for sparse resources exceeds device limits");

    DE_ASSERT((bufferMemRequirements.size % bufferMemRequirements.alignment) == 0);

    // Create sparse buffer memory bind semaphore
    const Unique<VkSemaphore> bufferMemoryBindSemaphore(createSemaphore(deviceInterface, getDevice()));

    const uint32_t numSparseSlots =
        (!m_testParams.isBufferNonResident ?
             static_cast<uint32_t>(bufferMemRequirements.size / bufferMemRequirements.alignment) :
             0u);

    std::vector<DeviceMemorySp> deviceMemUniquePtrVec;

    // Bind sparse memory if partially non-resident buffer
    if (!m_testParams.isBufferNonResident)
    {
        const Queue &sparseQueue = getQueue(VK_QUEUE_SPARSE_BINDING_BIT, 0);

        {
            std::vector<VkSparseMemoryBind> sparseMemoryBinds;

            const uint32_t memoryType = findMatchingMemoryType(instance, getPhysicalDevice(/*secondDeviceID*/),
                                                               bufferMemRequirements, MemoryRequirement::Any);

            if (memoryType == NO_MATCH_FOUND)
                return tcu::TestStatus::fail("No matching memory type found");

            for (uint32_t sparseBindNdx = 0; sparseBindNdx < numSparseSlots; sparseBindNdx += 2)
            {
                const VkSparseMemoryBind sparseMemoryBind =
                    makeSparseMemoryBind(deviceInterface, getDevice(), bufferMemRequirements.alignment, memoryType,
                                         bufferMemRequirements.alignment * sparseBindNdx);

                deviceMemUniquePtrVec.push_back(makeVkSharedPtr(
                    Move<VkDeviceMemory>(check<VkDeviceMemory>(sparseMemoryBind.memory),
                                         Deleter<VkDeviceMemory>(deviceInterface, getDevice(), nullptr))));

                sparseMemoryBinds.push_back(sparseMemoryBind);
            }

            const VkSparseBufferMemoryBindInfo sparseBufferBindInfo = makeSparseBufferMemoryBindInfo(
                *sparseBuffer, static_cast<uint32_t>(sparseMemoryBinds.size()), &sparseMemoryBinds[0]);

            const VkBindSparseInfo bindSparseInfo = {
                VK_STRUCTURE_TYPE_BIND_SPARSE_INFO, //VkStructureType sType;
                nullptr,                            //const void* pNext;
                0u,                                 //uint32_t waitSemaphoreCount;
                nullptr,                            //const VkSemaphore* pWaitSemaphores;
                1u,                                 //uint32_t bufferBindCount;
                &sparseBufferBindInfo,              //const VkSparseBufferMemoryBindInfo* pBufferBinds;
                0u,                                 //uint32_t imageOpaqueBindCount;
                nullptr,                            //const VkSparseImageOpaqueMemoryBindInfo* pImageOpaqueBinds;
                0u,                                 //uint32_t imageBindCount;
                nullptr,                            //const VkSparseImageMemoryBindInfo* pImageBinds;
                1u,                                 //uint32_t signalSemaphoreCount;
                &bufferMemoryBindSemaphore.get()    //const VkSemaphore* pSignalSemaphores;
            };

            VK_CHECK(deviceInterface.queueBindSparse(sparseQueue.queueHandle, 1u, &bindSparseInfo, VK_NULL_HANDLE));
        }

        // Wait for sparse queue to become idle
        deviceInterface.queueWaitIdle(sparseQueue.queueHandle);
    }

    const bool isCopyCmd          = (m_testParams.bufferInitCmd == BUFF_INIT_COPY);
    const bool isInputDescSparse  = (isCopyCmd && !m_testParams.isCopySrcSparse) ? true : false;
    const bool isOutputDescSparse = (isCopyCmd && m_testParams.isCopySrcSparse) || (!isCopyCmd) ? true : false;

    // Create input buffer for reading in shader or copy command
    const VkBufferCreateInfo inputBufferCreateInfo = makeBufferCreateInfo(
        m_testParams.bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    const Unique<VkBuffer> inputBuffer(createBuffer(deviceInterface, getDevice(), &inputBufferCreateInfo));
    const de::UniquePtr<Allocation> inputBufferAlloc(
        bindBuffer(deviceInterface, getDevice(), getAllocator(), *inputBuffer, MemoryRequirement::HostVisible));

    std::vector<uint8_t> referenceData;
    referenceData.resize(m_testParams.bufferSize);

    for (uint32_t valueNdx = 0; valueNdx < m_testParams.bufferSize; ++valueNdx)
    {
        referenceData[valueNdx] = static_cast<uint8_t>((valueNdx % bufferMemRequirements.alignment) + 1u);
    }

    deMemcpy(inputBufferAlloc->getHostPtr(), &referenceData[0], m_testParams.bufferSize);

    flushAlloc(deviceInterface, getDevice(), *inputBufferAlloc);

    // Create output buffer
    const VkDeviceSize outputBufferSizeBytes = m_testParams.bufferSize;
    const VkBufferCreateInfo outputBufferCreateInfo =
        makeBufferCreateInfo(outputBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const Unique<VkBuffer> outputBuffer(createBuffer(deviceInterface, getDevice(), &outputBufferCreateInfo));
    const de::UniquePtr<Allocation> outputBufferAlloc(
        bindBuffer(deviceInterface, getDevice(), getAllocator(), *outputBuffer, MemoryRequirement::HostVisible));

    // Initialize output buffer with all 0xFF
    {
        std::vector<uint8_t> randomData(m_testParams.bufferSize, 0xFF);

        deMemcpy(outputBufferAlloc->getHostPtr(), &randomData[0], m_testParams.bufferSize);

        flushAlloc(deviceInterface, getDevice(), *outputBufferAlloc);
    }

    const VkDeviceSize outputTestBufferSizeBytes = sizeof(uint32_t);
    const VkBufferCreateInfo outputTestBufferCreateInfo =
        makeBufferCreateInfo(outputTestBufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const Unique<VkBuffer> outputTestBuffer(createBuffer(deviceInterface, getDevice(), &outputTestBufferCreateInfo));
    const de::UniquePtr<Allocation> outputTestBufferAlloc(
        bindBuffer(deviceInterface, getDevice(), getAllocator(), *outputTestBuffer, MemoryRequirement::HostVisible));

    const Queue &cmdQueue = getQueue(cmdQueueBit, 0);

    // Create command buffer for compute and data transfer operations
    const Unique<VkCommandPool> commandPool(makeCommandPool(deviceInterface, getDevice(), cmdQueue.queueFamilyIndex));
    const Unique<VkCommandBuffer> commandBuffer(
        allocateCommandBuffer(deviceInterface, getDevice(), *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Start recording compute and transfer commands
    beginCommandBuffer(deviceInterface, *commandBuffer);

    // Create objects for compute pipeline
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(deviceInterface, getDevice()));

    const Unique<VkShaderModule> shaderModule(
        createShaderModule(deviceInterface, getDevice(), m_context.getBinaryCollection().get("comp"), 0));

    // Push constant range
    const VkPushConstantRange pcRange = {
        VK_SHADER_STAGE_COMPUTE_BIT,                      // VkShaderStageFlags stageFlags;
        0u,                                               // uint32_t offset;
        static_cast<uint32_t>(sizeof(TestPushConstants)), // uint32_t size;
    };

    const VkPushConstantRange *pushConstRange = &pcRange;
    Unique<VkPipelineLayout> pipelineLayout(
        makePipelineLayout(deviceInterface, getDevice(), *descriptorSetLayout, pushConstRange));

    const Unique<VkPipeline> computePipeline(
        makeComputePipeline(deviceInterface, getDevice(), *pipelineLayout, *shaderModule));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u)
            .build(deviceInterface, getDevice(), VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(
        makeDescriptorSet(deviceInterface, getDevice(), *descriptorPool, *descriptorSetLayout));

    const VkDescriptorBufferInfo inBufferInfo =
        makeDescriptorBufferInfo(isInputDescSparse ? *sparseBuffer : *inputBuffer, 0ull, m_testParams.bufferSize);

    const VkDescriptorBufferInfo outBufferInfo = makeDescriptorBufferInfo(
        isOutputDescSparse ? *sparseBuffer : (isInputDescSparse ? *outputTestBuffer : *outputBuffer), 0ull,
        isOutputDescSparse ? m_testParams.bufferSize :
                             (isInputDescSparse ? outputTestBufferSizeBytes : outputBufferSizeBytes));

    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &inBufferInfo)
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outBufferInfo)
        .update(deviceInterface, getDevice());

    // Update output buffer before being written over
    {
        const VkBufferMemoryBarrier outputBufferBarrier = makeBufferMemoryBarrier(
            VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, *outputBuffer, 0ull, m_testParams.bufferSize);

        deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                           0u, 0u, nullptr, 1u, &outputBufferBarrier, 0u, nullptr);
    }

    // Fill command buffer based on buffer commands
    switch (m_testParams.bufferInitCmd)
    {
    case BUFF_INIT_COPY:
    {
        if (!m_testParams.isCopySrcSparse)
        {
            // Update input buffer before being read in the transfer
            {
                const VkBufferMemoryBarrier inputBufferBarrier = makeBufferMemoryBarrier(
                    VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, *inputBuffer, 0ull, m_testParams.bufferSize);

                deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_HOST_BIT,
                                                   VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 1u,
                                                   &inputBufferBarrier, 0u, nullptr);
            }

            // Copy input buffer to sparse buffer with copy command
            {
                const VkBufferCopy bufferCopy = makeBufferCopy(0u, 0u, m_testParams.bufferSize);

                deviceInterface.cmdCopyBuffer(*commandBuffer, *inputBuffer, *sparseBuffer, 1u, &bufferCopy);
            }

            // Update sparse buffer before being read and verified in the shader
            {
                const VkBufferMemoryBarrier sparseBufferBarrier =
                    makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *sparseBuffer,
                                            0ull, m_testParams.bufferSize);

                deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 1u,
                                                   &sparseBufferBarrier, 0u, nullptr);
            }

            deviceInterface.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);

            deviceInterface.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u,
                                                  1u, &descriptorSet.get(), 0u, nullptr);

            {
                // Push constant data
                const TestPushConstants pushConstants = {m_testParams.bufferSize,
                                                         static_cast<uint32_t>(bufferMemRequirements.alignment)};
                deviceInterface.cmdPushConstants(*commandBuffer, *pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u,
                                                 static_cast<uint32_t>(sizeof(pushConstants)), &pushConstants);
            }
            // Read sparse buffer in the compute shader and output verification result
            deviceInterface.cmdDispatch(*commandBuffer, 1u, 1u, 1u);

            // Update output buffer before being read on the host
            {
                const VkBufferMemoryBarrier outputTestBufferBarrier =
                    makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *outputTestBuffer,
                                            0ull, outputTestBufferSizeBytes);

                deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                   VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u,
                                                   &outputTestBufferBarrier, 0u, nullptr);
            }
        }
        else
        {
            if (!m_testParams.isMultiCopy)
            {
                // Update input buffer before being read in the shader
                {
                    const VkBufferMemoryBarrier inputBufferBarrier =
                        makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *inputBuffer, 0ull,
                                                m_testParams.bufferSize);

                    deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_HOST_BIT,
                                                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 1u,
                                                       &inputBufferBarrier, 0u, nullptr);
                }

                deviceInterface.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);

                deviceInterface.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout,
                                                      0u, 1u, &descriptorSet.get(), 0u, nullptr);

                // Copy input buffer to sparse buffer in the compute shader
                deviceInterface.cmdDispatch(*commandBuffer, 1u, 1u, 1u);

                // Update sparse buffer before being read in the transfer
                {
                    const VkBufferMemoryBarrier sparseBufferBarrier =
                        makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, *sparseBuffer,
                                                0ull, m_testParams.bufferSize);

                    deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 1u,
                                                       &sparseBufferBarrier, 0u, nullptr);
                }

                // Copy sparse buffer to output buffer with copy command
                {
                    const VkBufferCopy bufferCopy = makeBufferCopy(0u, 0u, m_testParams.bufferSize);

                    deviceInterface.cmdCopyBuffer(*commandBuffer, *sparseBuffer, *outputBuffer, 1u, &bufferCopy);
                }

                // Update output buffer before being read on the host
                {
                    const VkBufferMemoryBarrier outputBufferBarrier =
                        makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *outputBuffer,
                                                0ull, m_testParams.bufferSize);

                    deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                       VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u,
                                                       &outputBufferBarrier, 0u, nullptr);
                }
            }
            else
            {
                // MultiCopy case:
                // Sparse buffer is completely non-resident

                // Copy multiple small regions of sparse buffer to output buffer with copy command
                std::vector<VkBufferCopy> regions;
                regions.push_back(makeBufferCopy(0u, 0u, 4u));
                regions.push_back(makeBufferCopy(2u, 8u, 4u));
                regions.push_back(makeBufferCopy(0u, 18u, 4u));

                deviceInterface.cmdCopyBuffer(*commandBuffer, *sparseBuffer, *outputBuffer, de::sizeU32(regions),
                                              de::dataOrNull(regions));

                // Update output buffer before being read on the host
                {
                    const VkBufferMemoryBarrier outputBufferBarrier =
                        makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *outputBuffer,
                                                0ull, m_testParams.bufferSize);

                    deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                       VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u,
                                                       &outputBufferBarrier, 0u, nullptr);
                }
            }
        }
    }
    break;
    case BUFF_INIT_FILL:
    {
        // Fill sparse buffer with fill command
        deviceInterface.cmdFillBuffer(*commandBuffer, *sparseBuffer, 0u, m_testParams.bufferSize, fillValue);

        // Update sparse buffer before being read in the transfer
        {
            const VkBufferMemoryBarrier sparseBufferBarrier =
                makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, *sparseBuffer, 0ull,
                                        m_testParams.bufferSize);

            deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                               VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 1u,
                                               &sparseBufferBarrier, 0u, nullptr);
        }

        // Copy sparse buffer to output buffer with copy command
        {
            const VkBufferCopy bufferCopy = makeBufferCopy(0u, 0u, m_testParams.bufferSize);

            deviceInterface.cmdCopyBuffer(*commandBuffer, *sparseBuffer, *outputBuffer, 1u, &bufferCopy);
        }

        // Update output buffer before being read on the host
        {
            const VkBufferMemoryBarrier outputBufferBarrier = makeBufferMemoryBarrier(
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *outputBuffer, 0ull, m_testParams.bufferSize);

            deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                               VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u, &outputBufferBarrier,
                                               0u, nullptr);
        }
    }
    break;
    case BUFF_INIT_UPDATE:
    {
        const uint32_t chunkSize = 65536u;

        for (uint32_t dataOffset = 0; dataOffset < m_testParams.bufferSize; dataOffset += chunkSize)
        {
            uint32_t size = chunkSize;

            if (m_testParams.bufferSize - dataOffset < chunkSize)
                size = m_testParams.bufferSize - dataOffset;

            deviceInterface.cmdUpdateBuffer(*commandBuffer, *sparseBuffer, dataOffset, size,
                                            &referenceData[dataOffset]);
        }

        // Update sparse buffer before being read in the transfer
        {
            const VkBufferMemoryBarrier sparseBufferBarrier =
                makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, *sparseBuffer, 0ull,
                                        m_testParams.bufferSize);

            deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                               VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 1u,
                                               &sparseBufferBarrier, 0u, nullptr);
        }

        // Copy sparse buffer to output buffer with copy command
        {
            const VkBufferCopy bufferCopy = makeBufferCopy(0u, 0u, m_testParams.bufferSize);

            deviceInterface.cmdCopyBuffer(*commandBuffer, *sparseBuffer, *outputBuffer, 1u, &bufferCopy);
        }

        // Update output buffer before being read on the host
        {
            const VkBufferMemoryBarrier outputBufferBarrier = makeBufferMemoryBarrier(
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *outputBuffer, 0ull, m_testParams.bufferSize);

            deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                               VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u, &outputBufferBarrier,
                                               0u, nullptr);
        }
    }
    break;
    default:
        DE_ASSERT(false);
    }

    // End recording compute and transfer commands
    endCommandBuffer(deviceInterface, *commandBuffer);

    const VkPipelineStageFlags waitStageBits[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};

    // Submit transfer commands for execution and wait for completion
    const uint32_t waitSemaphoreCount = (!m_testParams.isBufferNonResident ? 1u : 0u);
    const VkSemaphore *waitSemaphore = (!m_testParams.isBufferNonResident ? &bufferMemoryBindSemaphore.get() : nullptr);
    submitCommandsAndWait(deviceInterface, getDevice(), cmdQueue.queueHandle, *commandBuffer, waitSemaphoreCount,
                          waitSemaphore, waitStageBits, 0, nullptr);

    if ((m_testParams.bufferInitCmd == BUFF_INIT_COPY) && !m_testParams.isCopySrcSparse)
    {
        // Retrieve data from output buffer to host memory
        invalidateAlloc(deviceInterface, getDevice(), *outputTestBufferAlloc);

        const uint32_t *outputData = static_cast<const uint32_t *>(outputTestBufferAlloc->getHostPtr());

        if (*outputData == 0u)
            return tcu::TestStatus::fail("Failed");
    }
    else if ((m_testParams.bufferInitCmd == BUFF_INIT_COPY) && m_testParams.isMultiCopy)
    {
        // Retrieve data from output buffer to host memory
        invalidateAlloc(deviceInterface, getDevice(), *outputBufferAlloc);

        const uint8_t *outputData = static_cast<const uint8_t *>(outputBufferAlloc->getHostPtr());

        for (uint32_t byteIdx = 0; byteIdx < 32u; byteIdx++)
        {
            const uint32_t nullMask = 0xFFC3F0F0;
            uint32_t resultValue    = outputData[byteIdx];
            const bool hasValue     = (nullMask & (1u << byteIdx));
            uint32_t referenceValue = hasValue ? 0xFF : 0x00;

            if (!m_testParams.withStrictResidency && !hasValue)
                continue;

            if (resultValue != referenceValue)
                return tcu::TestStatus::fail("Failed");
        }
    }
    else
    {
        // Retrieve data from output buffer to host memory
        invalidateAlloc(deviceInterface, getDevice(), *outputBufferAlloc);

        const uint8_t *outputData = static_cast<const uint8_t *>(outputBufferAlloc->getHostPtr());

        // Compare output data with reference data
        if (!m_testParams.isBufferNonResident)
        {
            for (uint32_t sparseBindNdx = 0; sparseBindNdx < numSparseSlots; ++sparseBindNdx)
            {
                const uint32_t alignment = static_cast<uint32_t>(bufferMemRequirements.alignment);
                const uint32_t offset    = alignment * sparseBindNdx;
                const uint32_t size =
                    sparseBindNdx == (numSparseSlots - 1) ? m_testParams.bufferSize % alignment : alignment;

                if (sparseBindNdx % 2u == 0u)
                {
                    if (m_testParams.bufferInitCmd == BUFF_INIT_FILL)
                        deMemset(&referenceData[offset], (fillValue & 0xffu), size);

                    if (deMemCmp(&referenceData[offset], outputData + offset, size) != 0)
                        return tcu::TestStatus::fail("Failed");
                }
                else if (m_testParams.withStrictResidency)
                {
                    deMemset(&referenceData[offset], 0u, size);

                    if (deMemCmp(&referenceData[offset], outputData + offset, size) != 0)
                        return tcu::TestStatus::fail("Failed");
                }
            }
        }
        else if (m_testParams.withStrictResidency)
        {
            // Compare full buffer
            deMemset(&referenceData[0], 0u, m_testParams.bufferSize);
            if (deMemCmp(&referenceData[0], outputData, m_testParams.bufferSize) != 0)
                return tcu::TestStatus::fail("Failed");
        }
    }

    return tcu::TestStatus::pass("Passed");
}

TestInstance *BufferSparseResidencyNonResidentCase::createInstance(Context &context) const
{
    return new BufferSparseResidencyNonResidentInstance(context, m_testParams);
}

std::string getbufferInitCmdName(BufferInitCommand cmd)
{
    const std::string cmdName[BUFF_INIT_LAST] = {"copy", "fill", "update"};
    return cmdName[cmd];
}

} // namespace

void addBufferSparseResidencyTests(tcu::TestCaseGroup *group, const bool useDeviceGroups)
{
    group->addChild(new BufferSparseResidencyCase(group->getTestContext(), "buffer_size_2_10", 1 << 10,
                                                  glu::GLSL_VERSION_440, useDeviceGroups));
    group->addChild(new BufferSparseResidencyCase(group->getTestContext(), "buffer_size_2_12", 1 << 12,
                                                  glu::GLSL_VERSION_440, useDeviceGroups));
    group->addChild(new BufferSparseResidencyCase(group->getTestContext(), "buffer_size_2_16", 1 << 16,
                                                  glu::GLSL_VERSION_440, useDeviceGroups));
    group->addChild(new BufferSparseResidencyCase(group->getTestContext(), "buffer_size_2_17", 1 << 17,
                                                  glu::GLSL_VERSION_440, useDeviceGroups));
    group->addChild(new BufferSparseResidencyCase(group->getTestContext(), "buffer_size_2_20", 1 << 20,
                                                  glu::GLSL_VERSION_440, useDeviceGroups));
    group->addChild(new BufferSparseResidencyCase(group->getTestContext(), "buffer_size_2_24", 1 << 24,
                                                  glu::GLSL_VERSION_440, useDeviceGroups));

    if (!useDeviceGroups)
    {
        // Tests different reads/writes with sparse buffers that are partially resident
        // or not resident at all
        for (uint32_t cmdNdx = BUFF_INIT_COPY; cmdNdx < BUFF_INIT_LAST; cmdNdx++)
        {
            for (const uint32_t bufferSize : {1 << 10, 1 << 16, 1 << 24})
            {
                for (const auto strictResidency : {true, false})
                {
                    for (const auto bufferNonResidency : {true, false})
                    {
                        TestParams testParams = {static_cast<BufferInitCommand>(cmdNdx),
                                                 strictResidency,
                                                 bufferNonResidency,
                                                 bufferSize,
                                                 true /* isCopySrcSparse */,
                                                 false /* isMultiCopy */};

                        const std::string testNameP1 = std::string("non_resident_buffer") + "_" +
                                                       (strictResidency ? "strict_" : "") +
                                                       getbufferInitCmdName(testParams.bufferInitCmd);
                        const std::string testNameP2 = std::string("_alloc_") +
                                                       (bufferNonResidency ? "none" : "partial") + "_" +
                                                       de::toString(bufferSize);
                        {
                            const std::string testName =
                                testNameP1 + ((cmdNdx == BUFF_INIT_COPY) ? "_src" : "") + testNameP2;
                            group->addChild(new BufferSparseResidencyNonResidentCase(group->getTestContext(), testName,
                                                                                     testParams));
                        }

                        if (!strictResidency)
                            continue;

                        if (cmdNdx == BUFF_INIT_COPY)
                        {
                            testParams.isCopySrcSparse = false; // sparse buffer is destination of copy
                            const std::string testName = testNameP1 + "_dest" + testNameP2;

                            group->addChild(new BufferSparseResidencyNonResidentCase(group->getTestContext(), testName,
                                                                                     testParams));
                        }
                    }
                }
            }
        }

        // Test multiple small aligned and unaligned copies from sparse buffer
        {
            for (const auto strictResidency : {true, false})
            {
                TestParams testParams = {BufferInitCommand::BUFF_INIT_COPY,  strictResidency,
                                         true /* completely non-resident */, 1 << 16 /* bufferSize */,
                                         true /* isCopySrcSparse */,         true /* isMultiCopy */};

                const std::string testName = std::string("non_resident_buffer") + (strictResidency ? "_strict" : "") +
                                             "_multi_" + getbufferInitCmdName(BufferInitCommand::BUFF_INIT_COPY);

                group->addChild(
                    new BufferSparseResidencyNonResidentCase(group->getTestContext(), testName, testParams));
            }
        }
    }
}
} // namespace sparse
} // namespace vkt
