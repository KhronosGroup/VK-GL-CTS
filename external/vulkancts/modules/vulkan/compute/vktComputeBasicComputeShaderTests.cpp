/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 The Android Open Source Project
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Compute Shader Tests
 *//*--------------------------------------------------------------------*/

#include "vktComputeBasicComputeShaderTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktComputeTestsUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktAmberTestCase.hpp"

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
#include "vkDeviceUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkSafetyCriticalUtil.hpp"
#include "vkImageWithMemory.hpp"

#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"
#include "tcuMaybe.hpp"

#include "deMath.h"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include <vector>
#include <memory>

using namespace vk;

namespace vkt
{
namespace compute
{
namespace
{

template <typename T, int size>
T multiplyComponents(const tcu::Vector<T, size> &v)
{
    T accum = 1;
    for (int i = 0; i < size; ++i)
        accum *= v[i];
    return accum;
}

template <typename T>
inline T squared(const T &a)
{
    return a * a;
}

inline VkImageCreateInfo make2DImageCreateInfo(const tcu::IVec2 &imageSize, const VkImageUsageFlags usage)
{
    const VkImageCreateInfo imageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,               // VkStructureType sType;
        DE_NULL,                                           // const void* pNext;
        0u,                                                // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                                  // VkImageType imageType;
        VK_FORMAT_R32_UINT,                                // VkFormat format;
        vk::makeExtent3D(imageSize.x(), imageSize.y(), 1), // VkExtent3D extent;
        1u,                                                // uint32_t mipLevels;
        1u,                                                // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,                             // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,                           // VkImageTiling tiling;
        usage,                                             // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,                         // VkSharingMode sharingMode;
        0u,                                                // uint32_t queueFamilyIndexCount;
        DE_NULL,                                           // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,                         // VkImageLayout initialLayout;
    };
    return imageParams;
}

inline VkBufferImageCopy makeBufferImageCopy(const tcu::IVec2 &imageSize)
{
    return compute::makeBufferImageCopy(vk::makeExtent3D(imageSize.x(), imageSize.y(), 1), 1u);
}

enum BufferType
{
    BUFFER_TYPE_UNIFORM,
    BUFFER_TYPE_SSBO,
};

class SharedVarTest : public vkt::TestCase
{
public:
    SharedVarTest(tcu::TestContext &testCtx, const std::string &name, const tcu::IVec3 &localSize,
                  const tcu::IVec3 &workSize,
                  const vk::ComputePipelineConstructionType computePipelineConstructionType);

    virtual void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &sourceCollections) const;
    TestInstance *createInstance(Context &context) const;

private:
    const tcu::IVec3 m_localSize;
    const tcu::IVec3 m_workSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

class SharedVarTestInstance : public vkt::TestInstance
{
public:
    SharedVarTestInstance(Context &context, const tcu::IVec3 &localSize, const tcu::IVec3 &workSize,
                          const vk::ComputePipelineConstructionType computePipelineConstructionType);

    tcu::TestStatus iterate(void);

private:
    const tcu::IVec3 m_localSize;
    const tcu::IVec3 m_workSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

SharedVarTest::SharedVarTest(tcu::TestContext &testCtx, const std::string &name, const tcu::IVec3 &localSize,
                             const tcu::IVec3 &workSize,
                             const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestCase(testCtx, name)
    , m_localSize(localSize)
    , m_workSize(workSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

void SharedVarTest::checkSupport(Context &context) const
{
    checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                  m_computePipelineConstructionType);
}

void SharedVarTest::initPrograms(SourceCollections &sourceCollections) const
{
    const int workGroupSize  = multiplyComponents(m_localSize);
    const int workGroupCount = multiplyComponents(m_workSize);
    const int numValues      = workGroupSize * workGroupCount;

    std::ostringstream src;
    src << "#version 310 es\n"
        << "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y()
        << ", local_size_z = " << m_localSize.z() << ") in;\n"
        << "layout(binding = 0) writeonly buffer Output {\n"
        << "    uint values[" << numValues << "];\n"
        << "} sb_out;\n\n"
        << "shared uint offsets[" << workGroupSize << "];\n\n"
        << "void main (void) {\n"
        << "    uint localSize  = gl_WorkGroupSize.x*gl_WorkGroupSize.y*gl_WorkGroupSize.z;\n"
        << "    uint globalNdx  = gl_NumWorkGroups.x*gl_NumWorkGroups.y*gl_WorkGroupID.z + "
           "gl_NumWorkGroups.x*gl_WorkGroupID.y + gl_WorkGroupID.x;\n"
        << "    uint globalOffs = localSize*globalNdx;\n"
        << "    uint localOffs  = gl_WorkGroupSize.x*gl_WorkGroupSize.y*gl_LocalInvocationID.z + "
           "gl_WorkGroupSize.x*gl_LocalInvocationID.y + gl_LocalInvocationID.x;\n"
        << "\n"
        << "    offsets[localSize-localOffs-1u] = globalOffs + localOffs*localOffs;\n"
        << "    memoryBarrierShared();\n"
        << "    barrier();\n"
        << "    sb_out.values[globalOffs + localOffs] = offsets[localOffs];\n"
        << "}\n";

    sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance *SharedVarTest::createInstance(Context &context) const
{
    return new SharedVarTestInstance(context, m_localSize, m_workSize, m_computePipelineConstructionType);
}

SharedVarTestInstance::SharedVarTestInstance(Context &context, const tcu::IVec3 &localSize, const tcu::IVec3 &workSize,
                                             const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestInstance(context)
    , m_localSize(localSize)
    , m_workSize(workSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

tcu::TestStatus SharedVarTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    const int workGroupSize  = multiplyComponents(m_localSize);
    const int workGroupCount = multiplyComponents(m_workSize);

    // Create a buffer and host-visible memory for it

    const VkDeviceSize bufferSizeBytes = sizeof(uint32_t) * workGroupSize * workGroupCount;
    const BufferWithMemory buffer(vk, device, allocator,
                                  makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                  MemoryRequirement::HostVisible);

    // Create descriptor set

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    const VkDescriptorBufferInfo descriptorInfo = makeDescriptorBufferInfo(*buffer, 0ull, bufferSizeBytes);
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
        .update(vk, device);

    // Perform the computation

    ComputePipelineWrapper pipeline(vk, device, m_computePipelineConstructionType,
                                    m_context.getBinaryCollection().get("comp"));
    pipeline.setDescriptorSetLayout(descriptorSetLayout.get());
    pipeline.buildPipeline();

    const VkBufferMemoryBarrier computeFinishBarrier =
        makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer, 0ull, bufferSizeBytes);

    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Start recording commands

    beginCommandBuffer(vk, *cmdBuffer);

    pipeline.bind(*cmdBuffer);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getPipelineLayout(), 0u, 1u,
                             &descriptorSet.get(), 0u, DE_NULL);

    vk.cmdDispatch(*cmdBuffer, m_workSize.x(), m_workSize.y(), m_workSize.z());

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &computeFinishBarrier, 0,
                          (const VkImageMemoryBarrier *)DE_NULL);

    endCommandBuffer(vk, *cmdBuffer);

    // Wait for completion

    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    // Validate the results

    const Allocation &bufferAllocation = buffer.getAllocation();
    invalidateAlloc(vk, device, bufferAllocation);

    const uint32_t *bufferPtr = static_cast<uint32_t *>(bufferAllocation.getHostPtr());

    for (int groupNdx = 0; groupNdx < workGroupCount; ++groupNdx)
    {
        const int globalOffset = groupNdx * workGroupSize;
        for (int localOffset = 0; localOffset < workGroupSize; ++localOffset)
        {
            const uint32_t res = bufferPtr[globalOffset + localOffset];
            const uint32_t ref = globalOffset + squared(workGroupSize - localOffset - 1);

            if (res != ref)
            {
                std::ostringstream msg;
                msg << "Comparison failed for Output.values[" << (globalOffset + localOffset) << "]";
                return tcu::TestStatus::fail(msg.str());
            }
        }
    }
    return tcu::TestStatus::pass("Compute succeeded");
}

class SharedVarAtomicOpTest : public vkt::TestCase
{
public:
    SharedVarAtomicOpTest(tcu::TestContext &testCtx, const std::string &name, const tcu::IVec3 &localSize,
                          const tcu::IVec3 &workSize,
                          const vk::ComputePipelineConstructionType computePipelineConstructionType);

    virtual void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &sourceCollections) const;
    TestInstance *createInstance(Context &context) const;

private:
    const tcu::IVec3 m_localSize;
    const tcu::IVec3 m_workSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

class SharedVarAtomicOpTestInstance : public vkt::TestInstance
{
public:
    SharedVarAtomicOpTestInstance(Context &context, const tcu::IVec3 &localSize, const tcu::IVec3 &workSize,
                                  const vk::ComputePipelineConstructionType computePipelineConstructionType);

    tcu::TestStatus iterate(void);

private:
    const tcu::IVec3 m_localSize;
    const tcu::IVec3 m_workSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

SharedVarAtomicOpTest::SharedVarAtomicOpTest(tcu::TestContext &testCtx, const std::string &name,
                                             const tcu::IVec3 &localSize, const tcu::IVec3 &workSize,
                                             const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestCase(testCtx, name)
    , m_localSize(localSize)
    , m_workSize(workSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

void SharedVarAtomicOpTest::checkSupport(Context &context) const
{
    checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                  m_computePipelineConstructionType);
}

void SharedVarAtomicOpTest::initPrograms(SourceCollections &sourceCollections) const
{
    const int workGroupSize  = multiplyComponents(m_localSize);
    const int workGroupCount = multiplyComponents(m_workSize);
    const int numValues      = workGroupSize * workGroupCount;

    std::ostringstream src;
    src << "#version 310 es\n"
        << "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y()
        << ", local_size_z = " << m_localSize.z() << ") in;\n"
        << "layout(binding = 0) writeonly buffer Output {\n"
        << "    uint values[" << numValues << "];\n"
        << "} sb_out;\n\n"
        << "shared uint count;\n\n"
        << "void main (void) {\n"
        << "    uint localSize  = gl_WorkGroupSize.x*gl_WorkGroupSize.y*gl_WorkGroupSize.z;\n"
        << "    uint globalNdx  = gl_NumWorkGroups.x*gl_NumWorkGroups.y*gl_WorkGroupID.z + "
           "gl_NumWorkGroups.x*gl_WorkGroupID.y + gl_WorkGroupID.x;\n"
        << "    uint globalOffs = localSize*globalNdx;\n"
        << "\n"
        << "    count = 0u;\n"
        << "    memoryBarrierShared();\n"
        << "    barrier();\n"
        << "    uint oldVal = atomicAdd(count, 1u);\n"
        << "    sb_out.values[globalOffs+oldVal] = oldVal+1u;\n"
        << "}\n";

    sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance *SharedVarAtomicOpTest::createInstance(Context &context) const
{
    return new SharedVarAtomicOpTestInstance(context, m_localSize, m_workSize, m_computePipelineConstructionType);
}

SharedVarAtomicOpTestInstance::SharedVarAtomicOpTestInstance(
    Context &context, const tcu::IVec3 &localSize, const tcu::IVec3 &workSize,
    const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestInstance(context)
    , m_localSize(localSize)
    , m_workSize(workSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

tcu::TestStatus SharedVarAtomicOpTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    const int workGroupSize  = multiplyComponents(m_localSize);
    const int workGroupCount = multiplyComponents(m_workSize);

    // Create a buffer and host-visible memory for it

    const VkDeviceSize bufferSizeBytes = sizeof(uint32_t) * workGroupSize * workGroupCount;
    const BufferWithMemory buffer(vk, device, allocator,
                                  makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                  MemoryRequirement::HostVisible);

    // Create descriptor set

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    const VkDescriptorBufferInfo descriptorInfo = makeDescriptorBufferInfo(*buffer, 0ull, bufferSizeBytes);
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
        .update(vk, device);

    // Perform the computation

    ComputePipelineWrapper pipeline(vk, device, m_computePipelineConstructionType,
                                    m_context.getBinaryCollection().get("comp"));
    pipeline.setDescriptorSetLayout(descriptorSetLayout.get());
    pipeline.buildPipeline();

    const VkBufferMemoryBarrier computeFinishBarrier =
        makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer, 0ull, bufferSizeBytes);

    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Start recording commands

    beginCommandBuffer(vk, *cmdBuffer);

    pipeline.bind(*cmdBuffer);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getPipelineLayout(), 0u, 1u,
                             &descriptorSet.get(), 0u, DE_NULL);

    vk.cmdDispatch(*cmdBuffer, m_workSize.x(), m_workSize.y(), m_workSize.z());

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1u, &computeFinishBarrier, 0,
                          (const VkImageMemoryBarrier *)DE_NULL);

    endCommandBuffer(vk, *cmdBuffer);

    // Wait for completion

    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    // Validate the results

    const Allocation &bufferAllocation = buffer.getAllocation();
    invalidateAlloc(vk, device, bufferAllocation);

    const uint32_t *bufferPtr = static_cast<uint32_t *>(bufferAllocation.getHostPtr());

    for (int groupNdx = 0; groupNdx < workGroupCount; ++groupNdx)
    {
        const int globalOffset = groupNdx * workGroupSize;
        for (int localOffset = 0; localOffset < workGroupSize; ++localOffset)
        {
            const uint32_t res = bufferPtr[globalOffset + localOffset];
            const uint32_t ref = localOffset + 1;

            if (res != ref)
            {
                std::ostringstream msg;
                msg << "Comparison failed for Output.values[" << (globalOffset + localOffset) << "]";
                return tcu::TestStatus::fail(msg.str());
            }
        }
    }
    return tcu::TestStatus::pass("Compute succeeded");
}

class SSBOLocalBarrierTest : public vkt::TestCase
{
public:
    SSBOLocalBarrierTest(tcu::TestContext &testCtx, const std::string &name, const tcu::IVec3 &localSize,
                         const tcu::IVec3 &workSize,
                         const vk::ComputePipelineConstructionType computePipelineConstructionType);

    virtual void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &sourceCollections) const;
    TestInstance *createInstance(Context &context) const;

private:
    const tcu::IVec3 m_localSize;
    const tcu::IVec3 m_workSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

class SSBOLocalBarrierTestInstance : public vkt::TestInstance
{
public:
    SSBOLocalBarrierTestInstance(Context &context, const tcu::IVec3 &localSize, const tcu::IVec3 &workSize,
                                 const vk::ComputePipelineConstructionType computePipelineConstructionType);

    tcu::TestStatus iterate(void);

private:
    const tcu::IVec3 m_localSize;
    const tcu::IVec3 m_workSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

SSBOLocalBarrierTest::SSBOLocalBarrierTest(tcu::TestContext &testCtx, const std::string &name,
                                           const tcu::IVec3 &localSize, const tcu::IVec3 &workSize,
                                           const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestCase(testCtx, name)
    , m_localSize(localSize)
    , m_workSize(workSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

void SSBOLocalBarrierTest::checkSupport(Context &context) const
{
    checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                  m_computePipelineConstructionType);
}

void SSBOLocalBarrierTest::initPrograms(SourceCollections &sourceCollections) const
{
    const int workGroupSize  = multiplyComponents(m_localSize);
    const int workGroupCount = multiplyComponents(m_workSize);
    const int numValues      = workGroupSize * workGroupCount;

    std::ostringstream src;
    src << "#version 310 es\n"
        << "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y()
        << ", local_size_z = " << m_localSize.z() << ") in;\n"
        << "layout(binding = 0) coherent buffer Output {\n"
        << "    uint values[" << numValues << "];\n"
        << "} sb_out;\n\n"
        << "void main (void) {\n"
        << "    uint localSize  = gl_WorkGroupSize.x*gl_WorkGroupSize.y*gl_WorkGroupSize.z;\n"
        << "    uint globalNdx  = gl_NumWorkGroups.x*gl_NumWorkGroups.y*gl_WorkGroupID.z + "
           "gl_NumWorkGroups.x*gl_WorkGroupID.y + gl_WorkGroupID.x;\n"
        << "    uint globalOffs = localSize*globalNdx;\n"
        << "    uint localOffs  = gl_WorkGroupSize.x*gl_WorkGroupSize.y*gl_LocalInvocationID.z + "
           "gl_WorkGroupSize.x*gl_LocalInvocationID.y + gl_LocalInvocationID.x;\n"
        << "\n"
        << "    sb_out.values[globalOffs + localOffs] = globalOffs;\n"
        << "    memoryBarrierBuffer();\n"
        << "    barrier();\n"
        << "    sb_out.values[globalOffs + ((localOffs+1u)%localSize)] += localOffs;\n" // += so we read and write
        << "    memoryBarrierBuffer();\n"
        << "    barrier();\n"
        << "    sb_out.values[globalOffs + ((localOffs+2u)%localSize)] += localOffs;\n"
        << "}\n";

    sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance *SSBOLocalBarrierTest::createInstance(Context &context) const
{
    return new SSBOLocalBarrierTestInstance(context, m_localSize, m_workSize, m_computePipelineConstructionType);
}

SSBOLocalBarrierTestInstance::SSBOLocalBarrierTestInstance(
    Context &context, const tcu::IVec3 &localSize, const tcu::IVec3 &workSize,
    const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestInstance(context)
    , m_localSize(localSize)
    , m_workSize(workSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

tcu::TestStatus SSBOLocalBarrierTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    const int workGroupSize  = multiplyComponents(m_localSize);
    const int workGroupCount = multiplyComponents(m_workSize);

    // Create a buffer and host-visible memory for it

    const VkDeviceSize bufferSizeBytes = sizeof(uint32_t) * workGroupSize * workGroupCount;
    const BufferWithMemory buffer(vk, device, allocator,
                                  makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                  MemoryRequirement::HostVisible);

    // Create descriptor set

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    const VkDescriptorBufferInfo descriptorInfo = makeDescriptorBufferInfo(*buffer, 0ull, bufferSizeBytes);
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
        .update(vk, device);

    // Perform the computation

    ComputePipelineWrapper pipeline(vk, device, m_computePipelineConstructionType,
                                    m_context.getBinaryCollection().get("comp"));
    pipeline.setDescriptorSetLayout(descriptorSetLayout.get());
    pipeline.buildPipeline();

    const VkBufferMemoryBarrier computeFinishBarrier =
        makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer, 0ull, bufferSizeBytes);

    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Start recording commands

    beginCommandBuffer(vk, *cmdBuffer);

    pipeline.bind(*cmdBuffer);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getPipelineLayout(), 0u, 1u,
                             &descriptorSet.get(), 0u, DE_NULL);

    vk.cmdDispatch(*cmdBuffer, m_workSize.x(), m_workSize.y(), m_workSize.z());

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &computeFinishBarrier, 0,
                          (const VkImageMemoryBarrier *)DE_NULL);

    endCommandBuffer(vk, *cmdBuffer);

    // Wait for completion

    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    // Validate the results

    const Allocation &bufferAllocation = buffer.getAllocation();
    invalidateAlloc(vk, device, bufferAllocation);

    const uint32_t *bufferPtr = static_cast<uint32_t *>(bufferAllocation.getHostPtr());

    for (int groupNdx = 0; groupNdx < workGroupCount; ++groupNdx)
    {
        const int globalOffset = groupNdx * workGroupSize;
        for (int localOffset = 0; localOffset < workGroupSize; ++localOffset)
        {
            const uint32_t res = bufferPtr[globalOffset + localOffset];
            const int offs0    = localOffset - 1 < 0 ? ((localOffset + workGroupSize - 1) % workGroupSize) :
                                                       ((localOffset - 1) % workGroupSize);
            const int offs1    = localOffset - 2 < 0 ? ((localOffset + workGroupSize - 2) % workGroupSize) :
                                                       ((localOffset - 2) % workGroupSize);
            const uint32_t ref = static_cast<uint32_t>(globalOffset + offs0 + offs1);

            if (res != ref)
            {
                std::ostringstream msg;
                msg << "Comparison failed for Output.values[" << (globalOffset + localOffset) << "]";
                return tcu::TestStatus::fail(msg.str());
            }
        }
    }
    return tcu::TestStatus::pass("Compute succeeded");
}

class CopyImageToSSBOTest : public vkt::TestCase
{
public:
    CopyImageToSSBOTest(tcu::TestContext &testCtx, const std::string &name, const tcu::IVec2 &localSize,
                        const tcu::IVec2 &imageSize,
                        const vk::ComputePipelineConstructionType computePipelineConstructionType);

    virtual void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &sourceCollections) const;
    TestInstance *createInstance(Context &context) const;

private:
    const tcu::IVec2 m_localSize;
    const tcu::IVec2 m_imageSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

class CopyImageToSSBOTestInstance : public vkt::TestInstance
{
public:
    CopyImageToSSBOTestInstance(Context &context, const tcu::IVec2 &localSize, const tcu::IVec2 &imageSize,
                                const vk::ComputePipelineConstructionType computePipelineConstructionType);

    tcu::TestStatus iterate(void);

private:
    const tcu::IVec2 m_localSize;
    const tcu::IVec2 m_imageSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

CopyImageToSSBOTest::CopyImageToSSBOTest(tcu::TestContext &testCtx, const std::string &name,
                                         const tcu::IVec2 &localSize, const tcu::IVec2 &imageSize,
                                         const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestCase(testCtx, name)
    , m_localSize(localSize)
    , m_imageSize(imageSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
    DE_ASSERT(m_imageSize.x() % m_localSize.x() == 0);
    DE_ASSERT(m_imageSize.y() % m_localSize.y() == 0);
}

void CopyImageToSSBOTest::checkSupport(Context &context) const
{
    checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                  m_computePipelineConstructionType);
}

void CopyImageToSSBOTest::initPrograms(SourceCollections &sourceCollections) const
{
    std::ostringstream src;
    src << "#version 310 es\n"
        << "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y() << ") in;\n"
        << "layout(binding = 1, r32ui) readonly uniform highp uimage2D u_srcImg;\n"
        << "layout(binding = 0) writeonly buffer Output {\n"
        << "    uint values[" << (m_imageSize.x() * m_imageSize.y()) << "];\n"
        << "} sb_out;\n\n"
        << "void main (void) {\n"
        << "    uint stride = gl_NumWorkGroups.x*gl_WorkGroupSize.x;\n"
        << "    uint value  = imageLoad(u_srcImg, ivec2(gl_GlobalInvocationID.xy)).x;\n"
        << "    sb_out.values[gl_GlobalInvocationID.y*stride + gl_GlobalInvocationID.x] = value;\n"
        << "}\n";

    sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance *CopyImageToSSBOTest::createInstance(Context &context) const
{
    return new CopyImageToSSBOTestInstance(context, m_localSize, m_imageSize, m_computePipelineConstructionType);
}

CopyImageToSSBOTestInstance::CopyImageToSSBOTestInstance(
    Context &context, const tcu::IVec2 &localSize, const tcu::IVec2 &imageSize,
    const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestInstance(context)
    , m_localSize(localSize)
    , m_imageSize(imageSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

tcu::TestStatus CopyImageToSSBOTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // Create an image

    const VkImageCreateInfo imageParams =
        make2DImageCreateInfo(m_imageSize, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
    const ImageWithMemory image(vk, device, allocator, imageParams, MemoryRequirement::Any);

    const VkImageSubresourceRange subresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const Unique<VkImageView> imageView(
        makeImageView(vk, device, *image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32_UINT, subresourceRange));

    // Staging buffer (source data for image)

    const uint32_t imageArea           = multiplyComponents(m_imageSize);
    const VkDeviceSize bufferSizeBytes = sizeof(uint32_t) * imageArea;

    const BufferWithMemory stagingBuffer(vk, device, allocator,
                                         makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                                         MemoryRequirement::HostVisible);

    // Populate the staging buffer with test data
    {
        de::Random rnd(0xab2c7);
        const Allocation &stagingBufferAllocation = stagingBuffer.getAllocation();
        uint32_t *bufferPtr                       = static_cast<uint32_t *>(stagingBufferAllocation.getHostPtr());
        for (uint32_t i = 0; i < imageArea; ++i)
            *bufferPtr++ = rnd.getUint32();

        flushAlloc(vk, device, stagingBufferAllocation);
    }

    // Create a buffer to store shader output

    const BufferWithMemory outputBuffer(vk, device, allocator,
                                        makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                        MemoryRequirement::HostVisible);

    // Create descriptor set

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    // Set the bindings

    const VkDescriptorImageInfo imageDescriptorInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, *imageView, VK_IMAGE_LAYOUT_GENERAL);
    const VkDescriptorBufferInfo bufferDescriptorInfo = makeDescriptorBufferInfo(*outputBuffer, 0ull, bufferSizeBytes);

    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo)
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageDescriptorInfo)
        .update(vk, device);

    // Perform the computation
    {
        ComputePipelineWrapper pipeline(vk, device, m_computePipelineConstructionType,
                                        m_context.getBinaryCollection().get("comp"));
        pipeline.setDescriptorSetLayout(descriptorSetLayout.get());
        pipeline.buildPipeline();

        const VkBufferMemoryBarrier computeFinishBarrier = makeBufferMemoryBarrier(
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *outputBuffer, 0ull, bufferSizeBytes);
        const tcu::IVec2 workSize = m_imageSize / m_localSize;

        // Prepare the command buffer

        const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
        const Unique<VkCommandBuffer> cmdBuffer(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

        // Start recording commands

        beginCommandBuffer(vk, *cmdBuffer);

        pipeline.bind(*cmdBuffer);
        vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getPipelineLayout(), 0u, 1u,
                                 &descriptorSet.get(), 0u, DE_NULL);

        const std::vector<VkBufferImageCopy> bufferImageCopy(1, makeBufferImageCopy(m_imageSize));
        copyBufferToImage(vk, *cmdBuffer, *stagingBuffer, bufferSizeBytes, bufferImageCopy, VK_IMAGE_ASPECT_COLOR_BIT,
                          1, 1, *image, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        vk.cmdDispatch(*cmdBuffer, workSize.x(), workSize.y(), 1u);
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                              (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &computeFinishBarrier, 0,
                              (const VkImageMemoryBarrier *)DE_NULL);

        endCommandBuffer(vk, *cmdBuffer);

        // Wait for completion

        submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    }

    // Validate the results

    const Allocation &outputBufferAllocation = outputBuffer.getAllocation();
    invalidateAlloc(vk, device, outputBufferAllocation);

    const uint32_t *bufferPtr    = static_cast<uint32_t *>(outputBufferAllocation.getHostPtr());
    const uint32_t *refBufferPtr = static_cast<uint32_t *>(stagingBuffer.getAllocation().getHostPtr());

    for (uint32_t ndx = 0; ndx < imageArea; ++ndx)
    {
        const uint32_t res = *(bufferPtr + ndx);
        const uint32_t ref = *(refBufferPtr + ndx);

        if (res != ref)
        {
            std::ostringstream msg;
            msg << "Comparison failed for Output.values[" << ndx << "]";
            return tcu::TestStatus::fail(msg.str());
        }
    }
    return tcu::TestStatus::pass("Compute succeeded");
}

class CopySSBOToImageTest : public vkt::TestCase
{
public:
    CopySSBOToImageTest(tcu::TestContext &testCtx, const std::string &name, const tcu::IVec2 &localSize,
                        const tcu::IVec2 &imageSize,
                        const vk::ComputePipelineConstructionType computePipelineConstructionType);

    virtual void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &sourceCollections) const;
    TestInstance *createInstance(Context &context) const;

private:
    const tcu::IVec2 m_localSize;
    const tcu::IVec2 m_imageSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

class CopySSBOToImageTestInstance : public vkt::TestInstance
{
public:
    CopySSBOToImageTestInstance(Context &context, const tcu::IVec2 &localSize, const tcu::IVec2 &imageSize,
                                const vk::ComputePipelineConstructionType computePipelineConstructionType);

    tcu::TestStatus iterate(void);

private:
    const tcu::IVec2 m_localSize;
    const tcu::IVec2 m_imageSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

CopySSBOToImageTest::CopySSBOToImageTest(tcu::TestContext &testCtx, const std::string &name,
                                         const tcu::IVec2 &localSize, const tcu::IVec2 &imageSize,
                                         const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestCase(testCtx, name)
    , m_localSize(localSize)
    , m_imageSize(imageSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
    DE_ASSERT(m_imageSize.x() % m_localSize.x() == 0);
    DE_ASSERT(m_imageSize.y() % m_localSize.y() == 0);
}

void CopySSBOToImageTest::checkSupport(Context &context) const
{
    checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                  m_computePipelineConstructionType);
}

void CopySSBOToImageTest::initPrograms(SourceCollections &sourceCollections) const
{
    std::ostringstream src;
    src << "#version 310 es\n"
        << "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y() << ") in;\n"
        << "layout(binding = 1, r32ui) writeonly uniform highp uimage2D u_dstImg;\n"
        << "layout(binding = 0) readonly buffer Input {\n"
        << "    uint values[" << (m_imageSize.x() * m_imageSize.y()) << "];\n"
        << "} sb_in;\n\n"
        << "void main (void) {\n"
        << "    uint stride = gl_NumWorkGroups.x*gl_WorkGroupSize.x;\n"
        << "    uint value  = sb_in.values[gl_GlobalInvocationID.y*stride + gl_GlobalInvocationID.x];\n"
        << "    imageStore(u_dstImg, ivec2(gl_GlobalInvocationID.xy), uvec4(value, 0, 0, 0));\n"
        << "}\n";

    sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance *CopySSBOToImageTest::createInstance(Context &context) const
{
    return new CopySSBOToImageTestInstance(context, m_localSize, m_imageSize, m_computePipelineConstructionType);
}

CopySSBOToImageTestInstance::CopySSBOToImageTestInstance(
    Context &context, const tcu::IVec2 &localSize, const tcu::IVec2 &imageSize,
    const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestInstance(context)
    , m_localSize(localSize)
    , m_imageSize(imageSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

tcu::TestStatus CopySSBOToImageTestInstance::iterate(void)
{
    ContextCommonData data     = m_context.getContextCommonData();
    const DeviceInterface &vkd = data.vkd;

    // Create an image, a view, and the output buffer
    const VkImageSubresourceRange subresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    ImageWithBuffer imageWithBuffer(
        vkd, data.device, data.allocator, vk::makeExtent3D(m_imageSize.x(), m_imageSize.y(), 1), VK_FORMAT_R32_UINT,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT, vk::VK_IMAGE_TYPE_2D, subresourceRange);

    const uint32_t imageArea           = multiplyComponents(m_imageSize);
    const VkDeviceSize bufferSizeBytes = sizeof(uint32_t) * imageArea;

    const BufferWithMemory inputBuffer(vkd, data.device, data.allocator,
                                       makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                       MemoryRequirement::HostVisible);

    // Populate the buffer with test data
    {
        de::Random rnd(0x77238ac2);
        const Allocation &inputBufferAllocation = inputBuffer.getAllocation();
        uint32_t *bufferPtr                     = static_cast<uint32_t *>(inputBufferAllocation.getHostPtr());
        for (uint32_t i = 0; i < imageArea; ++i)
            *bufferPtr++ = rnd.getUint32();

        flushAlloc(vkd, data.device, inputBufferAllocation);
    }

    // Create descriptor set
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vkd, data.device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .build(vkd, data.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(
        makeDescriptorSet(vkd, data.device, *descriptorPool, *descriptorSetLayout));

    // Set the bindings

    const VkDescriptorImageInfo imageDescriptorInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, imageWithBuffer.getImageView(), VK_IMAGE_LAYOUT_GENERAL);
    const VkDescriptorBufferInfo bufferDescriptorInfo = makeDescriptorBufferInfo(*inputBuffer, 0ull, bufferSizeBytes);

    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo)
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageDescriptorInfo)
        .update(vkd, data.device);

    // Perform the computation
    {
        ComputePipelineWrapper pipeline(vkd, data.device, m_computePipelineConstructionType,
                                        m_context.getBinaryCollection().get("comp"));
        pipeline.setDescriptorSetLayout(descriptorSetLayout.get());
        pipeline.buildPipeline();

        const VkBufferMemoryBarrier inputBufferPostHostWriteBarrier = makeBufferMemoryBarrier(
            VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *inputBuffer, 0ull, bufferSizeBytes);

        const VkImageMemoryBarrier imageLayoutBarrier =
            makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                   imageWithBuffer.getImage(), subresourceRange);

        const tcu::IVec2 workSize = m_imageSize / m_localSize;

        // Prepare the command buffer

        const Unique<VkCommandPool> cmdPool(makeCommandPool(vkd, data.device, data.qfIndex));
        const Unique<VkCommandBuffer> cmdBuffer(
            allocateCommandBuffer(vkd, data.device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

        // Start recording commands

        beginCommandBuffer(vkd, *cmdBuffer);

        pipeline.bind(*cmdBuffer);
        vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getPipelineLayout(), 0u, 1u,
                                  &descriptorSet.get(), 0u, DE_NULL);

        vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                               (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1,
                               &inputBufferPostHostWriteBarrier, 1, &imageLayoutBarrier);
        vkd.cmdDispatch(*cmdBuffer, workSize.x(), workSize.y(), 1u);

        copyImageToBuffer(vkd, *cmdBuffer, imageWithBuffer.getImage(), imageWithBuffer.getBuffer(), m_imageSize,
                          VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

        endCommandBuffer(vkd, *cmdBuffer);

        // Wait for completion

        submitCommandsAndWait(vkd, data.device, data.queue, *cmdBuffer);
    }

    // Validate the results

    const Allocation &outputBufferAllocation = imageWithBuffer.getBufferAllocation();
    invalidateAlloc(vkd, data.device, outputBufferAllocation);

    const uint32_t *bufferPtr    = static_cast<uint32_t *>(outputBufferAllocation.getHostPtr());
    const uint32_t *refBufferPtr = static_cast<uint32_t *>(inputBuffer.getAllocation().getHostPtr());

    for (uint32_t ndx = 0; ndx < imageArea; ++ndx)
    {
        const uint32_t res = *(bufferPtr + ndx);
        const uint32_t ref = *(refBufferPtr + ndx);

        if (res != ref)
        {
            std::ostringstream msg;
            msg << "Comparison failed for pixel " << ndx;
            return tcu::TestStatus::fail(msg.str());
        }
    }
    return tcu::TestStatus::pass("Compute succeeded");
}

class BufferToBufferInvertTest : public vkt::TestCase
{
public:
    virtual void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &sourceCollections) const;
    TestInstance *createInstance(Context &context) const;

    static BufferToBufferInvertTest *UBOToSSBOInvertCase(
        tcu::TestContext &testCtx, const std::string &name, const uint32_t numValues, const tcu::IVec3 &localSize,
        const tcu::IVec3 &workSize, const vk::ComputePipelineConstructionType computePipelineConstructionType);

    static BufferToBufferInvertTest *CopyInvertSSBOCase(
        tcu::TestContext &testCtx, const std::string &name, const uint32_t numValues, const tcu::IVec3 &localSize,
        const tcu::IVec3 &workSize, const vk::ComputePipelineConstructionType computePipelineConstructionType);

private:
    BufferToBufferInvertTest(tcu::TestContext &testCtx, const std::string &name, const uint32_t numValues,
                             const tcu::IVec3 &localSize, const tcu::IVec3 &workSize, const BufferType bufferType,
                             const vk::ComputePipelineConstructionType computePipelineConstructionType);

    const BufferType m_bufferType;
    const uint32_t m_numValues;
    const tcu::IVec3 m_localSize;
    const tcu::IVec3 m_workSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

class BufferToBufferInvertTestInstance : public vkt::TestInstance
{
public:
    BufferToBufferInvertTestInstance(Context &context, const uint32_t numValues, const tcu::IVec3 &localSize,
                                     const tcu::IVec3 &workSize, const BufferType bufferType,
                                     const vk::ComputePipelineConstructionType computePipelineConstructionType);

    tcu::TestStatus iterate(void);

private:
    const BufferType m_bufferType;
    const uint32_t m_numValues;
    const tcu::IVec3 m_localSize;
    const tcu::IVec3 m_workSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

BufferToBufferInvertTest::BufferToBufferInvertTest(
    tcu::TestContext &testCtx, const std::string &name, const uint32_t numValues, const tcu::IVec3 &localSize,
    const tcu::IVec3 &workSize, const BufferType bufferType,
    const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestCase(testCtx, name)
    , m_bufferType(bufferType)
    , m_numValues(numValues)
    , m_localSize(localSize)
    , m_workSize(workSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
    DE_ASSERT(m_numValues % (multiplyComponents(m_workSize) * multiplyComponents(m_localSize)) == 0);
    DE_ASSERT(m_bufferType == BUFFER_TYPE_UNIFORM || m_bufferType == BUFFER_TYPE_SSBO);
}

BufferToBufferInvertTest *BufferToBufferInvertTest::UBOToSSBOInvertCase(
    tcu::TestContext &testCtx, const std::string &name, const uint32_t numValues, const tcu::IVec3 &localSize,
    const tcu::IVec3 &workSize, const vk::ComputePipelineConstructionType computePipelineConstructionType)
{
    return new BufferToBufferInvertTest(testCtx, name, numValues, localSize, workSize, BUFFER_TYPE_UNIFORM,
                                        computePipelineConstructionType);
}

BufferToBufferInvertTest *BufferToBufferInvertTest::CopyInvertSSBOCase(
    tcu::TestContext &testCtx, const std::string &name, const uint32_t numValues, const tcu::IVec3 &localSize,
    const tcu::IVec3 &workSize, const vk::ComputePipelineConstructionType computePipelineConstructionType)
{
    return new BufferToBufferInvertTest(testCtx, name, numValues, localSize, workSize, BUFFER_TYPE_SSBO,
                                        computePipelineConstructionType);
}

void BufferToBufferInvertTest::checkSupport(Context &context) const
{
    checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                  m_computePipelineConstructionType);
}

void BufferToBufferInvertTest::initPrograms(SourceCollections &sourceCollections) const
{
    std::ostringstream src;
    if (m_bufferType == BUFFER_TYPE_UNIFORM)
    {
        src << "#version 310 es\n"
            << "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y()
            << ", local_size_z = " << m_localSize.z() << ") in;\n"
            << "layout(binding = 0) readonly uniform Input {\n"
            << "    uint values[" << m_numValues << "];\n"
            << "} ub_in;\n"
            << "layout(binding = 1, std140) writeonly buffer Output {\n"
            << "    uint values[" << m_numValues << "];\n"
            << "} sb_out;\n"
            << "void main (void) {\n"
            << "    uvec3 size           = gl_NumWorkGroups * gl_WorkGroupSize;\n"
            << "    uint numValuesPerInv = uint(ub_in.values.length()) / (size.x*size.y*size.z);\n"
            << "    uint groupNdx        = size.x*size.y*gl_GlobalInvocationID.z + size.x*gl_GlobalInvocationID.y + "
               "gl_GlobalInvocationID.x;\n"
            << "    uint offset          = numValuesPerInv*groupNdx;\n"
            << "\n"
            << "    for (uint ndx = 0u; ndx < numValuesPerInv; ndx++)\n"
            << "        sb_out.values[offset + ndx] = ~ub_in.values[offset + ndx];\n"
            << "}\n";
    }
    else if (m_bufferType == BUFFER_TYPE_SSBO)
    {
        src << "#version 310 es\n"
            << "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y()
            << ", local_size_z = " << m_localSize.z() << ") in;\n"
            << "layout(binding = 0, std140) readonly buffer Input {\n"
            << "    uint values[" << m_numValues << "];\n"
            << "} sb_in;\n"
            << "layout (binding = 1, std140) writeonly buffer Output {\n"
            << "    uint values[" << m_numValues << "];\n"
            << "} sb_out;\n"
            << "void main (void) {\n"
            << "    uvec3 size           = gl_NumWorkGroups * gl_WorkGroupSize;\n"
            << "    uint numValuesPerInv = uint(sb_in.values.length()) / (size.x*size.y*size.z);\n"
            << "    uint groupNdx        = size.x*size.y*gl_GlobalInvocationID.z + size.x*gl_GlobalInvocationID.y + "
               "gl_GlobalInvocationID.x;\n"
            << "    uint offset          = numValuesPerInv*groupNdx;\n"
            << "\n"
            << "    for (uint ndx = 0u; ndx < numValuesPerInv; ndx++)\n"
            << "        sb_out.values[offset + ndx] = ~sb_in.values[offset + ndx];\n"
            << "}\n";
    }

    sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance *BufferToBufferInvertTest::createInstance(Context &context) const
{
    return new BufferToBufferInvertTestInstance(context, m_numValues, m_localSize, m_workSize, m_bufferType,
                                                m_computePipelineConstructionType);
}

BufferToBufferInvertTestInstance::BufferToBufferInvertTestInstance(
    Context &context, const uint32_t numValues, const tcu::IVec3 &localSize, const tcu::IVec3 &workSize,
    const BufferType bufferType, const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestInstance(context)
    , m_bufferType(bufferType)
    , m_numValues(numValues)
    , m_localSize(localSize)
    , m_workSize(workSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

tcu::TestStatus BufferToBufferInvertTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // Customize the test based on buffer type

    const VkBufferUsageFlags inputBufferUsageFlags =
        (m_bufferType == BUFFER_TYPE_UNIFORM ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const VkDescriptorType inputBufferDescriptorType =
        (m_bufferType == BUFFER_TYPE_UNIFORM ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    const uint32_t randomSeed = (m_bufferType == BUFFER_TYPE_UNIFORM ? 0x111223f : 0x124fef);

    // Create an input buffer

    const VkDeviceSize bufferSizeBytes = sizeof(tcu::UVec4) * m_numValues;
    const BufferWithMemory inputBuffer(vk, device, allocator,
                                       makeBufferCreateInfo(bufferSizeBytes, inputBufferUsageFlags),
                                       MemoryRequirement::HostVisible);

    // Fill the input buffer with data
    {
        de::Random rnd(randomSeed);
        const Allocation &inputBufferAllocation = inputBuffer.getAllocation();
        tcu::UVec4 *bufferPtr                   = static_cast<tcu::UVec4 *>(inputBufferAllocation.getHostPtr());
        for (uint32_t i = 0; i < m_numValues; ++i)
            bufferPtr[i].x() = rnd.getUint32();

        flushAlloc(vk, device, inputBufferAllocation);
    }

    // Create an output buffer

    const BufferWithMemory outputBuffer(vk, device, allocator,
                                        makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                        MemoryRequirement::HostVisible);

    // Create descriptor set

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(inputBufferDescriptorType, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(inputBufferDescriptorType)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    const VkDescriptorBufferInfo inputBufferDescriptorInfo =
        makeDescriptorBufferInfo(*inputBuffer, 0ull, bufferSizeBytes);
    const VkDescriptorBufferInfo outputBufferDescriptorInfo =
        makeDescriptorBufferInfo(*outputBuffer, 0ull, bufferSizeBytes);
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), inputBufferDescriptorType,
                     &inputBufferDescriptorInfo)
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputBufferDescriptorInfo)
        .update(vk, device);

    // Perform the computation

    ComputePipelineWrapper pipeline(vk, device, m_computePipelineConstructionType,
                                    m_context.getBinaryCollection().get("comp"));
    pipeline.setDescriptorSetLayout(descriptorSetLayout.get());
    pipeline.buildPipeline();

    const VkBufferMemoryBarrier hostWriteBarrier = makeBufferMemoryBarrier(
        VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *inputBuffer, 0ull, bufferSizeBytes);

    const VkBufferMemoryBarrier shaderWriteBarrier = makeBufferMemoryBarrier(
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *outputBuffer, 0ull, bufferSizeBytes);

    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Start recording commands

    beginCommandBuffer(vk, *cmdBuffer);

    pipeline.bind(*cmdBuffer);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getPipelineLayout(), 0u, 1u,
                             &descriptorSet.get(), 0u, DE_NULL);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &hostWriteBarrier, 0,
                          (const VkImageMemoryBarrier *)DE_NULL);
    vk.cmdDispatch(*cmdBuffer, m_workSize.x(), m_workSize.y(), m_workSize.z());
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &shaderWriteBarrier, 0,
                          (const VkImageMemoryBarrier *)DE_NULL);

    endCommandBuffer(vk, *cmdBuffer);

    // Wait for completion

    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    // Validate the results

    const Allocation &outputBufferAllocation = outputBuffer.getAllocation();
    invalidateAlloc(vk, device, outputBufferAllocation);

    const tcu::UVec4 *bufferPtr    = static_cast<tcu::UVec4 *>(outputBufferAllocation.getHostPtr());
    const tcu::UVec4 *refBufferPtr = static_cast<tcu::UVec4 *>(inputBuffer.getAllocation().getHostPtr());

    for (uint32_t ndx = 0; ndx < m_numValues; ++ndx)
    {
        const uint32_t res = bufferPtr[ndx].x();
        const uint32_t ref = ~refBufferPtr[ndx].x();

        if (res != ref)
        {
            std::ostringstream msg;
            msg << "Comparison failed for Output.values[" << ndx << "]";
            return tcu::TestStatus::fail(msg.str());
        }
    }
    return tcu::TestStatus::pass("Compute succeeded");
}

class InvertSSBOInPlaceTest : public vkt::TestCase
{
public:
    InvertSSBOInPlaceTest(tcu::TestContext &testCtx, const std::string &name, const uint32_t numValues,
                          const bool sized, const tcu::IVec3 &localSize, const tcu::IVec3 &workSize,
                          const vk::ComputePipelineConstructionType computePipelineConstructionType);

    virtual void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &sourceCollections) const;
    TestInstance *createInstance(Context &context) const;

private:
    const uint32_t m_numValues;
    const bool m_sized;
    const tcu::IVec3 m_localSize;
    const tcu::IVec3 m_workSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

class InvertSSBOInPlaceTestInstance : public vkt::TestInstance
{
public:
    InvertSSBOInPlaceTestInstance(Context &context, const uint32_t numValues, const tcu::IVec3 &localSize,
                                  const tcu::IVec3 &workSize,
                                  const vk::ComputePipelineConstructionType computePipelineConstructionType);

    tcu::TestStatus iterate(void);

private:
    const uint32_t m_numValues;
    const tcu::IVec3 m_localSize;
    const tcu::IVec3 m_workSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

InvertSSBOInPlaceTest::InvertSSBOInPlaceTest(tcu::TestContext &testCtx, const std::string &name,
                                             const uint32_t numValues, const bool sized, const tcu::IVec3 &localSize,
                                             const tcu::IVec3 &workSize,
                                             const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestCase(testCtx, name)
    , m_numValues(numValues)
    , m_sized(sized)
    , m_localSize(localSize)
    , m_workSize(workSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
    DE_ASSERT(m_numValues % (multiplyComponents(m_workSize) * multiplyComponents(m_localSize)) == 0);
}

void InvertSSBOInPlaceTest::checkSupport(Context &context) const
{
    checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                  m_computePipelineConstructionType);
}

void InvertSSBOInPlaceTest::initPrograms(SourceCollections &sourceCollections) const
{
    std::ostringstream src;
    src << "#version 310 es\n"
        << "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y()
        << ", local_size_z = " << m_localSize.z() << ") in;\n"
        << "layout(binding = 0) buffer InOut {\n"
        << "    uint values[" << (m_sized ? de::toString(m_numValues) : "") << "];\n"
        << "} sb_inout;\n"
        << "void main (void) {\n"
        << "    uvec3 size           = gl_NumWorkGroups * gl_WorkGroupSize;\n"
        << "    uint numValuesPerInv = uint(sb_inout.values.length()) / (size.x*size.y*size.z);\n"
        << "    uint groupNdx        = size.x*size.y*gl_GlobalInvocationID.z + size.x*gl_GlobalInvocationID.y + "
           "gl_GlobalInvocationID.x;\n"
        << "    uint offset          = numValuesPerInv*groupNdx;\n"
        << "\n"
        << "    for (uint ndx = 0u; ndx < numValuesPerInv; ndx++)\n"
        << "        sb_inout.values[offset + ndx] = ~sb_inout.values[offset + ndx];\n"
        << "}\n";

    sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance *InvertSSBOInPlaceTest::createInstance(Context &context) const
{
    return new InvertSSBOInPlaceTestInstance(context, m_numValues, m_localSize, m_workSize,
                                             m_computePipelineConstructionType);
}

InvertSSBOInPlaceTestInstance::InvertSSBOInPlaceTestInstance(
    Context &context, const uint32_t numValues, const tcu::IVec3 &localSize, const tcu::IVec3 &workSize,
    const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestInstance(context)
    , m_numValues(numValues)
    , m_localSize(localSize)
    , m_workSize(workSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

tcu::TestStatus InvertSSBOInPlaceTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // Create an input/output buffer

    const VkDeviceSize bufferSizeBytes = sizeof(uint32_t) * m_numValues;
    const BufferWithMemory buffer(vk, device, allocator,
                                  makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                  MemoryRequirement::HostVisible);

    // Fill the buffer with data

    typedef std::vector<uint32_t> data_vector_t;
    data_vector_t inputData(m_numValues);

    {
        de::Random rnd(0x82ce7f);
        const Allocation &bufferAllocation = buffer.getAllocation();
        uint32_t *bufferPtr                = static_cast<uint32_t *>(bufferAllocation.getHostPtr());
        for (uint32_t i = 0; i < m_numValues; ++i)
            inputData[i] = *bufferPtr++ = rnd.getUint32();

        flushAlloc(vk, device, bufferAllocation);
    }

    // Create descriptor set

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    const VkDescriptorBufferInfo bufferDescriptorInfo = makeDescriptorBufferInfo(*buffer, 0ull, bufferSizeBytes);
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo)
        .update(vk, device);

    // Perform the computation

    ComputePipelineWrapper pipeline(vk, device, m_computePipelineConstructionType,
                                    m_context.getBinaryCollection().get("comp"));
    pipeline.setDescriptorSetLayout(descriptorSetLayout.get());
    pipeline.buildPipeline();

    const VkBufferMemoryBarrier hostWriteBarrier =
        makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *buffer, 0ull, bufferSizeBytes);

    const VkBufferMemoryBarrier shaderWriteBarrier =
        makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer, 0ull, bufferSizeBytes);

    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Start recording commands

    beginCommandBuffer(vk, *cmdBuffer);

    pipeline.bind(*cmdBuffer);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getPipelineLayout(), 0u, 1u,
                             &descriptorSet.get(), 0u, DE_NULL);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &hostWriteBarrier, 0,
                          (const VkImageMemoryBarrier *)DE_NULL);
    vk.cmdDispatch(*cmdBuffer, m_workSize.x(), m_workSize.y(), m_workSize.z());
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &shaderWriteBarrier, 0,
                          (const VkImageMemoryBarrier *)DE_NULL);

    endCommandBuffer(vk, *cmdBuffer);

    // Wait for completion

    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    // Validate the results

    const Allocation &bufferAllocation = buffer.getAllocation();
    invalidateAlloc(vk, device, bufferAllocation);

    const uint32_t *bufferPtr = static_cast<uint32_t *>(bufferAllocation.getHostPtr());

    for (uint32_t ndx = 0; ndx < m_numValues; ++ndx)
    {
        const uint32_t res = bufferPtr[ndx];
        const uint32_t ref = ~inputData[ndx];

        if (res != ref)
        {
            std::ostringstream msg;
            msg << "Comparison failed for InOut.values[" << ndx << "]";
            return tcu::TestStatus::fail(msg.str());
        }
    }
    return tcu::TestStatus::pass("Compute succeeded");
}

class WriteToMultipleSSBOTest : public vkt::TestCase
{
public:
    WriteToMultipleSSBOTest(tcu::TestContext &testCtx, const std::string &name, const uint32_t numValues,
                            const bool sized, const tcu::IVec3 &localSize, const tcu::IVec3 &workSize,
                            const vk::ComputePipelineConstructionType computePipelineConstructionType);

    virtual void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &sourceCollections) const;
    TestInstance *createInstance(Context &context) const;

private:
    const uint32_t m_numValues;
    const bool m_sized;
    const tcu::IVec3 m_localSize;
    const tcu::IVec3 m_workSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

class WriteToMultipleSSBOTestInstance : public vkt::TestInstance
{
public:
    WriteToMultipleSSBOTestInstance(Context &context, const uint32_t numValues, const tcu::IVec3 &localSize,
                                    const tcu::IVec3 &workSize,
                                    const vk::ComputePipelineConstructionType computePipelineConstructionType);

    tcu::TestStatus iterate(void);

private:
    const uint32_t m_numValues;
    const tcu::IVec3 m_localSize;
    const tcu::IVec3 m_workSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

WriteToMultipleSSBOTest::WriteToMultipleSSBOTest(
    tcu::TestContext &testCtx, const std::string &name, const uint32_t numValues, const bool sized,
    const tcu::IVec3 &localSize, const tcu::IVec3 &workSize,
    const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestCase(testCtx, name)
    , m_numValues(numValues)
    , m_sized(sized)
    , m_localSize(localSize)
    , m_workSize(workSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
    DE_ASSERT(m_numValues % (multiplyComponents(m_workSize) * multiplyComponents(m_localSize)) == 0);
}

void WriteToMultipleSSBOTest::checkSupport(Context &context) const
{
    checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                  m_computePipelineConstructionType);
}

void WriteToMultipleSSBOTest::initPrograms(SourceCollections &sourceCollections) const
{
    std::ostringstream src;
    src << "#version 310 es\n"
        << "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y()
        << ", local_size_z = " << m_localSize.z() << ") in;\n"
        << "layout(binding = 0) writeonly buffer Out0 {\n"
        << "    uint values[" << (m_sized ? de::toString(m_numValues) : "") << "];\n"
        << "} sb_out0;\n"
        << "layout(binding = 1) writeonly buffer Out1 {\n"
        << "    uint values[" << (m_sized ? de::toString(m_numValues) : "") << "];\n"
        << "} sb_out1;\n"
        << "void main (void) {\n"
        << "    uvec3 size      = gl_NumWorkGroups * gl_WorkGroupSize;\n"
        << "    uint groupNdx   = size.x*size.y*gl_GlobalInvocationID.z + size.x*gl_GlobalInvocationID.y + "
           "gl_GlobalInvocationID.x;\n"
        << "\n"
        << "    {\n"
        << "        uint numValuesPerInv = uint(sb_out0.values.length()) / (size.x*size.y*size.z);\n"
        << "        uint offset          = numValuesPerInv*groupNdx;\n"
        << "\n"
        << "        for (uint ndx = 0u; ndx < numValuesPerInv; ndx++)\n"
        << "            sb_out0.values[offset + ndx] = offset + ndx;\n"
        << "    }\n"
        << "    {\n"
        << "        uint numValuesPerInv = uint(sb_out1.values.length()) / (size.x*size.y*size.z);\n"
        << "        uint offset          = numValuesPerInv*groupNdx;\n"
        << "\n"
        << "        for (uint ndx = 0u; ndx < numValuesPerInv; ndx++)\n"
        << "            sb_out1.values[offset + ndx] = uint(sb_out1.values.length()) - offset - ndx;\n"
        << "    }\n"
        << "}\n";

    sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance *WriteToMultipleSSBOTest::createInstance(Context &context) const
{
    return new WriteToMultipleSSBOTestInstance(context, m_numValues, m_localSize, m_workSize,
                                               m_computePipelineConstructionType);
}

WriteToMultipleSSBOTestInstance::WriteToMultipleSSBOTestInstance(
    Context &context, const uint32_t numValues, const tcu::IVec3 &localSize, const tcu::IVec3 &workSize,
    const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestInstance(context)
    , m_numValues(numValues)
    , m_localSize(localSize)
    , m_workSize(workSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

tcu::TestStatus WriteToMultipleSSBOTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // Create two output buffers

    const VkDeviceSize bufferSizeBytes = sizeof(uint32_t) * m_numValues;
    const BufferWithMemory buffer0(vk, device, allocator,
                                   makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                   MemoryRequirement::HostVisible);
    const BufferWithMemory buffer1(vk, device, allocator,
                                   makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                   MemoryRequirement::HostVisible);

    // Create descriptor set

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    const VkDescriptorBufferInfo buffer0DescriptorInfo = makeDescriptorBufferInfo(*buffer0, 0ull, bufferSizeBytes);
    const VkDescriptorBufferInfo buffer1DescriptorInfo = makeDescriptorBufferInfo(*buffer1, 0ull, bufferSizeBytes);
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &buffer0DescriptorInfo)
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &buffer1DescriptorInfo)
        .update(vk, device);

    // Perform the computation

    ComputePipelineWrapper pipeline(vk, device, m_computePipelineConstructionType,
                                    m_context.getBinaryCollection().get("comp"));
    pipeline.setDescriptorSetLayout(descriptorSetLayout.get());
    pipeline.buildPipeline();

    const VkBufferMemoryBarrier shaderWriteBarriers[] = {
        makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer0, 0ull, bufferSizeBytes),
        makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer1, 0ull, bufferSizeBytes)};

    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Start recording commands

    beginCommandBuffer(vk, *cmdBuffer);

    pipeline.bind(*cmdBuffer);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getPipelineLayout(), 0u, 1u,
                             &descriptorSet.get(), 0u, DE_NULL);

    vk.cmdDispatch(*cmdBuffer, m_workSize.x(), m_workSize.y(), m_workSize.z());
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL,
                          DE_LENGTH_OF_ARRAY(shaderWriteBarriers), shaderWriteBarriers, 0,
                          (const VkImageMemoryBarrier *)DE_NULL);

    endCommandBuffer(vk, *cmdBuffer);

    // Wait for completion

    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    // Validate the results
    {
        const Allocation &buffer0Allocation = buffer0.getAllocation();
        invalidateAlloc(vk, device, buffer0Allocation);
        const uint32_t *buffer0Ptr = static_cast<uint32_t *>(buffer0Allocation.getHostPtr());

        for (uint32_t ndx = 0; ndx < m_numValues; ++ndx)
        {
            const uint32_t res = buffer0Ptr[ndx];
            const uint32_t ref = ndx;

            if (res != ref)
            {
                std::ostringstream msg;
                msg << "Comparison failed for Out0.values[" << ndx << "] res=" << res << " ref=" << ref;
                return tcu::TestStatus::fail(msg.str());
            }
        }
    }
    {
        const Allocation &buffer1Allocation = buffer1.getAllocation();
        invalidateAlloc(vk, device, buffer1Allocation);
        const uint32_t *buffer1Ptr = static_cast<uint32_t *>(buffer1Allocation.getHostPtr());

        for (uint32_t ndx = 0; ndx < m_numValues; ++ndx)
        {
            const uint32_t res = buffer1Ptr[ndx];
            const uint32_t ref = m_numValues - ndx;

            if (res != ref)
            {
                std::ostringstream msg;
                msg << "Comparison failed for Out1.values[" << ndx << "] res=" << res << " ref=" << ref;
                return tcu::TestStatus::fail(msg.str());
            }
        }
    }
    return tcu::TestStatus::pass("Compute succeeded");
}

class SSBOBarrierTest : public vkt::TestCase
{
public:
    SSBOBarrierTest(tcu::TestContext &testCtx, const std::string &name, const tcu::IVec3 &workSize,
                    const vk::ComputePipelineConstructionType computePipelineConstructionType);

    virtual void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &sourceCollections) const;
    TestInstance *createInstance(Context &context) const;

private:
    const tcu::IVec3 m_workSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

class SSBOBarrierTestInstance : public vkt::TestInstance
{
public:
    SSBOBarrierTestInstance(Context &context, const tcu::IVec3 &workSize,
                            const vk::ComputePipelineConstructionType computePipelineConstructionType);

    tcu::TestStatus iterate(void);

private:
    const tcu::IVec3 m_workSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

SSBOBarrierTest::SSBOBarrierTest(tcu::TestContext &testCtx, const std::string &name, const tcu::IVec3 &workSize,
                                 const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestCase(testCtx, name)
    , m_workSize(workSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

void SSBOBarrierTest::checkSupport(Context &context) const
{
    checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                  m_computePipelineConstructionType);
}

void SSBOBarrierTest::initPrograms(SourceCollections &sourceCollections) const
{
    sourceCollections.glslSources.add("comp0")
        << glu::ComputeSource("#version 310 es\n"
                              "layout (local_size_x = 1) in;\n"
                              "layout(binding = 2) readonly uniform Constants {\n"
                              "    uint u_baseVal;\n"
                              "};\n"
                              "layout(binding = 1) writeonly buffer Output {\n"
                              "    uint values[];\n"
                              "};\n"
                              "void main (void) {\n"
                              "    uint offset = gl_NumWorkGroups.x*gl_NumWorkGroups.y*gl_WorkGroupID.z + "
                              "gl_NumWorkGroups.x*gl_WorkGroupID.y + gl_WorkGroupID.x;\n"
                              "    values[offset] = u_baseVal + offset;\n"
                              "}\n");

    sourceCollections.glslSources.add("comp1")
        << glu::ComputeSource("#version 310 es\n"
                              "layout (local_size_x = 1) in;\n"
                              "layout(binding = 1) readonly buffer Input {\n"
                              "    uint values[];\n"
                              "};\n"
                              "layout(binding = 0) coherent buffer Output {\n"
                              "    uint sum;\n"
                              "};\n"
                              "void main (void) {\n"
                              "    uint offset = gl_NumWorkGroups.x*gl_NumWorkGroups.y*gl_WorkGroupID.z + "
                              "gl_NumWorkGroups.x*gl_WorkGroupID.y + gl_WorkGroupID.x;\n"
                              "    uint value  = values[offset];\n"
                              "    atomicAdd(sum, value);\n"
                              "}\n");
}

TestInstance *SSBOBarrierTest::createInstance(Context &context) const
{
    return new SSBOBarrierTestInstance(context, m_workSize, m_computePipelineConstructionType);
}

SSBOBarrierTestInstance::SSBOBarrierTestInstance(
    Context &context, const tcu::IVec3 &workSize,
    const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestInstance(context)
    , m_workSize(workSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

tcu::TestStatus SSBOBarrierTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // Create a work buffer used by both shaders

    const int workGroupCount               = multiplyComponents(m_workSize);
    const VkDeviceSize workBufferSizeBytes = sizeof(uint32_t) * workGroupCount;
    const BufferWithMemory workBuffer(vk, device, allocator,
                                      makeBufferCreateInfo(workBufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                      MemoryRequirement::Any);

    // Create an output buffer

    const VkDeviceSize outputBufferSizeBytes = sizeof(uint32_t);
    const BufferWithMemory outputBuffer(vk, device, allocator,
                                        makeBufferCreateInfo(outputBufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                        MemoryRequirement::HostVisible);

    // Initialize atomic counter value to zero
    {
        const Allocation &outputBufferAllocation = outputBuffer.getAllocation();
        uint32_t *outputBufferPtr                = static_cast<uint32_t *>(outputBufferAllocation.getHostPtr());
        *outputBufferPtr                         = 0;
        flushAlloc(vk, device, outputBufferAllocation);
    }

    // Create a uniform buffer (to pass uniform constants)

    const VkDeviceSize uniformBufferSizeBytes = sizeof(uint32_t);
    const BufferWithMemory uniformBuffer(
        vk, device, allocator, makeBufferCreateInfo(uniformBufferSizeBytes, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
        MemoryRequirement::HostVisible);

    // Set the constants in the uniform buffer

    const uint32_t baseValue = 127;
    {
        const Allocation &uniformBufferAllocation = uniformBuffer.getAllocation();
        uint32_t *uniformBufferPtr                = static_cast<uint32_t *>(uniformBufferAllocation.getHostPtr());
        uniformBufferPtr[0]                       = baseValue;

        flushAlloc(vk, device, uniformBufferAllocation);
    }

    // Create descriptor set

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u)
            .addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    const VkDescriptorBufferInfo workBufferDescriptorInfo =
        makeDescriptorBufferInfo(*workBuffer, 0ull, workBufferSizeBytes);
    const VkDescriptorBufferInfo outputBufferDescriptorInfo =
        makeDescriptorBufferInfo(*outputBuffer, 0ull, outputBufferSizeBytes);
    const VkDescriptorBufferInfo uniformBufferDescriptorInfo =
        makeDescriptorBufferInfo(*uniformBuffer, 0ull, uniformBufferSizeBytes);
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputBufferDescriptorInfo)
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &workBufferDescriptorInfo)
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2u),
                     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBufferDescriptorInfo)
        .update(vk, device);

    // Perform the computation

    ComputePipelineWrapper pipeline0(vk, device, m_computePipelineConstructionType,
                                     m_context.getBinaryCollection().get("comp0"));
    pipeline0.setDescriptorSetLayout(descriptorSetLayout.get());
    pipeline0.buildPipeline();

    ComputePipelineWrapper pipeline1(vk, device, m_computePipelineConstructionType,
                                     m_context.getBinaryCollection().get("comp1"));
    pipeline1.setDescriptorSetLayout(descriptorSetLayout.get());
    pipeline1.buildPipeline();

    const VkBufferMemoryBarrier writeUniformConstantsBarrier = makeBufferMemoryBarrier(
        VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT, *uniformBuffer, 0ull, uniformBufferSizeBytes);

    const VkBufferMemoryBarrier betweenShadersBarrier = makeBufferMemoryBarrier(
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *workBuffer, 0ull, workBufferSizeBytes);

    const VkBufferMemoryBarrier afterComputeBarrier = makeBufferMemoryBarrier(
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *outputBuffer, 0ull, outputBufferSizeBytes);

    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Start recording commands

    beginCommandBuffer(vk, *cmdBuffer);

    pipeline0.bind(*cmdBuffer);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline0.getPipelineLayout(), 0u, 1u,
                             &descriptorSet.get(), 0u, DE_NULL);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &writeUniformConstantsBarrier,
                          0, (const VkImageMemoryBarrier *)DE_NULL);

    vk.cmdDispatch(*cmdBuffer, m_workSize.x(), m_workSize.y(), m_workSize.z());
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &betweenShadersBarrier, 0,
                          (const VkImageMemoryBarrier *)DE_NULL);

    // Switch to the second shader program
    pipeline1.bind(*cmdBuffer);

    vk.cmdDispatch(*cmdBuffer, m_workSize.x(), m_workSize.y(), m_workSize.z());
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &afterComputeBarrier, 0,
                          (const VkImageMemoryBarrier *)DE_NULL);

    endCommandBuffer(vk, *cmdBuffer);

    // Wait for completion

    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    // Validate the results

    const Allocation &outputBufferAllocation = outputBuffer.getAllocation();
    invalidateAlloc(vk, device, outputBufferAllocation);

    const uint32_t *bufferPtr = static_cast<uint32_t *>(outputBufferAllocation.getHostPtr());
    const uint32_t res        = *bufferPtr;
    uint32_t ref              = 0;

    for (int ndx = 0; ndx < workGroupCount; ++ndx)
        ref += baseValue + ndx;

    if (res != ref)
    {
        std::ostringstream msg;
        msg << "ERROR: comparison failed, expected " << ref << ", got " << res;
        return tcu::TestStatus::fail(msg.str());
    }
    return tcu::TestStatus::pass("Compute succeeded");
}

class ImageAtomicOpTest : public vkt::TestCase
{
public:
    ImageAtomicOpTest(tcu::TestContext &testCtx, const std::string &name, const uint32_t localSize,
                      const tcu::IVec2 &imageSize,
                      const vk::ComputePipelineConstructionType computePipelineConstructionType);

    virtual void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &sourceCollections) const;
    TestInstance *createInstance(Context &context) const;

private:
    const uint32_t m_localSize;
    const tcu::IVec2 m_imageSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

class ImageAtomicOpTestInstance : public vkt::TestInstance
{
public:
    ImageAtomicOpTestInstance(Context &context, const uint32_t localSize, const tcu::IVec2 &imageSize,
                              const vk::ComputePipelineConstructionType computePipelineConstructionType);

    tcu::TestStatus iterate(void);

private:
    const uint32_t m_localSize;
    const tcu::IVec2 m_imageSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

ImageAtomicOpTest::ImageAtomicOpTest(tcu::TestContext &testCtx, const std::string &name, const uint32_t localSize,
                                     const tcu::IVec2 &imageSize,
                                     const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestCase(testCtx, name)
    , m_localSize(localSize)
    , m_imageSize(imageSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

void ImageAtomicOpTest::checkSupport(Context &context) const
{
    checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                  m_computePipelineConstructionType);
}

void ImageAtomicOpTest::initPrograms(SourceCollections &sourceCollections) const
{
    std::ostringstream src;
    src << "#version 310 es\n"
        << "#extension GL_OES_shader_image_atomic : require\n"
        << "layout (local_size_x = " << m_localSize << ") in;\n"
        << "layout(binding = 1, r32ui) coherent uniform highp uimage2D u_dstImg;\n"
        << "layout(binding = 0) readonly buffer Input {\n"
        << "    uint values[" << (multiplyComponents(m_imageSize) * m_localSize) << "];\n"
        << "} sb_in;\n\n"
        << "void main (void) {\n"
        << "    uint stride = gl_NumWorkGroups.x*gl_WorkGroupSize.x;\n"
        << "    uint value  = sb_in.values[gl_GlobalInvocationID.y*stride + gl_GlobalInvocationID.x];\n"
        << "\n"
        << "    if (gl_LocalInvocationIndex == 0u)\n"
        << "        imageStore(u_dstImg, ivec2(gl_WorkGroupID.xy), uvec4(0));\n"
        << "    memoryBarrierImage();\n"
        << "    barrier();\n"
        << "    imageAtomicAdd(u_dstImg, ivec2(gl_WorkGroupID.xy), value);\n"
        << "}\n";

    sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance *ImageAtomicOpTest::createInstance(Context &context) const
{
    return new ImageAtomicOpTestInstance(context, m_localSize, m_imageSize, m_computePipelineConstructionType);
}

ImageAtomicOpTestInstance::ImageAtomicOpTestInstance(
    Context &context, const uint32_t localSize, const tcu::IVec2 &imageSize,
    const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestInstance(context)
    , m_localSize(localSize)
    , m_imageSize(imageSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

tcu::TestStatus ImageAtomicOpTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // Create an image

    const VkImageCreateInfo imageParams =
        make2DImageCreateInfo(m_imageSize, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
    const ImageWithMemory image(vk, device, allocator, imageParams, MemoryRequirement::Any);

    const VkImageSubresourceRange subresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const Unique<VkImageView> imageView(
        makeImageView(vk, device, *image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32_UINT, subresourceRange));

    // Input buffer

    const uint32_t numInputValues           = multiplyComponents(m_imageSize) * m_localSize;
    const VkDeviceSize inputBufferSizeBytes = sizeof(uint32_t) * numInputValues;

    const BufferWithMemory inputBuffer(vk, device, allocator,
                                       makeBufferCreateInfo(inputBufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                       MemoryRequirement::HostVisible);

    // Populate the input buffer with test data
    {
        de::Random rnd(0x77238ac2);
        const Allocation &inputBufferAllocation = inputBuffer.getAllocation();
        uint32_t *bufferPtr                     = static_cast<uint32_t *>(inputBufferAllocation.getHostPtr());
        for (uint32_t i = 0; i < numInputValues; ++i)
            *bufferPtr++ = rnd.getUint32();

        flushAlloc(vk, device, inputBufferAllocation);
    }

    // Create a buffer to store shader output (copied from image data)

    const uint32_t imageArea                 = multiplyComponents(m_imageSize);
    const VkDeviceSize outputBufferSizeBytes = sizeof(uint32_t) * imageArea;
    const BufferWithMemory outputBuffer(vk, device, allocator,
                                        makeBufferCreateInfo(outputBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                                        MemoryRequirement::HostVisible);

    // Create descriptor set

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    // Set the bindings

    const VkDescriptorImageInfo imageDescriptorInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, *imageView, VK_IMAGE_LAYOUT_GENERAL);
    const VkDescriptorBufferInfo bufferDescriptorInfo =
        makeDescriptorBufferInfo(*inputBuffer, 0ull, inputBufferSizeBytes);

    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo)
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageDescriptorInfo)
        .update(vk, device);

    // Perform the computation
    {
        ComputePipelineWrapper pipeline(vk, device, m_computePipelineConstructionType,
                                        m_context.getBinaryCollection().get("comp"));
        pipeline.setDescriptorSetLayout(descriptorSetLayout.get());
        pipeline.buildPipeline();

        const VkBufferMemoryBarrier inputBufferPostHostWriteBarrier = makeBufferMemoryBarrier(
            VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *inputBuffer, 0ull, inputBufferSizeBytes);

        const VkImageMemoryBarrier imageLayoutBarrier =
            makeImageMemoryBarrier((VkAccessFlags)0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_GENERAL, *image, subresourceRange);

        // Prepare the command buffer

        const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
        const Unique<VkCommandBuffer> cmdBuffer(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

        // Start recording commands

        beginCommandBuffer(vk, *cmdBuffer);

        pipeline.bind(*cmdBuffer);
        vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getPipelineLayout(), 0u, 1u,
                                 &descriptorSet.get(), 0u, DE_NULL);

        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1,
                              &inputBufferPostHostWriteBarrier, 1, &imageLayoutBarrier);
        vk.cmdDispatch(*cmdBuffer, m_imageSize.x(), m_imageSize.y(), 1u);

        copyImageToBuffer(vk, *cmdBuffer, *image, *outputBuffer, m_imageSize, VK_ACCESS_SHADER_WRITE_BIT,
                          VK_IMAGE_LAYOUT_GENERAL);

        endCommandBuffer(vk, *cmdBuffer);

        // Wait for completion

        submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    }

    // Validate the results

    const Allocation &outputBufferAllocation = outputBuffer.getAllocation();
    invalidateAlloc(vk, device, outputBufferAllocation);

    const uint32_t *bufferPtr    = static_cast<uint32_t *>(outputBufferAllocation.getHostPtr());
    const uint32_t *refBufferPtr = static_cast<uint32_t *>(inputBuffer.getAllocation().getHostPtr());

    for (uint32_t pixelNdx = 0; pixelNdx < imageArea; ++pixelNdx)
    {
        const uint32_t res = bufferPtr[pixelNdx];
        uint32_t ref       = 0;

        for (uint32_t offs = 0; offs < m_localSize; ++offs)
            ref += refBufferPtr[pixelNdx * m_localSize + offs];

        if (res != ref)
        {
            std::ostringstream msg;
            msg << "Comparison failed for pixel " << pixelNdx;
            return tcu::TestStatus::fail(msg.str());
        }
    }
    return tcu::TestStatus::pass("Compute succeeded");
}

class ImageBarrierTest : public vkt::TestCase
{
public:
    ImageBarrierTest(tcu::TestContext &testCtx, const std::string &name, const tcu::IVec2 &imageSize,
                     const vk::ComputePipelineConstructionType computePipelineConstructionType);

    virtual void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &sourceCollections) const;
    TestInstance *createInstance(Context &context) const;

private:
    const tcu::IVec2 m_imageSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

class ImageBarrierTestInstance : public vkt::TestInstance
{
public:
    ImageBarrierTestInstance(Context &context, const tcu::IVec2 &imageSize,
                             const vk::ComputePipelineConstructionType computePipelineConstructionType);

    tcu::TestStatus iterate(void);

private:
    const tcu::IVec2 m_imageSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

ImageBarrierTest::ImageBarrierTest(tcu::TestContext &testCtx, const std::string &name, const tcu::IVec2 &imageSize,
                                   const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestCase(testCtx, name)
    , m_imageSize(imageSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

void ImageBarrierTest::checkSupport(Context &context) const
{
    checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                  m_computePipelineConstructionType);
}

void ImageBarrierTest::initPrograms(SourceCollections &sourceCollections) const
{
    sourceCollections.glslSources.add("comp0")
        << glu::ComputeSource("#version 310 es\n"
                              "layout (local_size_x = 1) in;\n"
                              "layout(binding = 2) readonly uniform Constants {\n"
                              "    uint u_baseVal;\n"
                              "};\n"
                              "layout(binding = 1, r32ui) writeonly uniform highp uimage2D u_img;\n"
                              "void main (void) {\n"
                              "    uint offset = gl_NumWorkGroups.x*gl_NumWorkGroups.y*gl_WorkGroupID.z + "
                              "gl_NumWorkGroups.x*gl_WorkGroupID.y + gl_WorkGroupID.x;\n"
                              "    imageStore(u_img, ivec2(gl_WorkGroupID.xy), uvec4(offset + u_baseVal, 0, 0, 0));\n"
                              "}\n");

    sourceCollections.glslSources.add("comp1")
        << glu::ComputeSource("#version 310 es\n"
                              "layout (local_size_x = 1) in;\n"
                              "layout(binding = 1, r32ui) readonly uniform highp uimage2D u_img;\n"
                              "layout(binding = 0) coherent buffer Output {\n"
                              "    uint sum;\n"
                              "};\n"
                              "void main (void) {\n"
                              "    uint value = imageLoad(u_img, ivec2(gl_WorkGroupID.xy)).x;\n"
                              "    atomicAdd(sum, value);\n"
                              "}\n");
}

TestInstance *ImageBarrierTest::createInstance(Context &context) const
{
    return new ImageBarrierTestInstance(context, m_imageSize, m_computePipelineConstructionType);
}

ImageBarrierTestInstance::ImageBarrierTestInstance(
    Context &context, const tcu::IVec2 &imageSize,
    const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestInstance(context)
    , m_imageSize(imageSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

tcu::TestStatus ImageBarrierTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // Create an image used by both shaders

    const VkImageCreateInfo imageParams = make2DImageCreateInfo(m_imageSize, VK_IMAGE_USAGE_STORAGE_BIT);
    const ImageWithMemory image(vk, device, allocator, imageParams, MemoryRequirement::Any);

    const VkImageSubresourceRange subresourceRange =
        makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const Unique<VkImageView> imageView(
        makeImageView(vk, device, *image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32_UINT, subresourceRange));

    // Create an output buffer

    const VkDeviceSize outputBufferSizeBytes = sizeof(uint32_t);
    const BufferWithMemory outputBuffer(vk, device, allocator,
                                        makeBufferCreateInfo(outputBufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                        MemoryRequirement::HostVisible);

    // Initialize atomic counter value to zero
    {
        const Allocation &outputBufferAllocation = outputBuffer.getAllocation();
        uint32_t *outputBufferPtr                = static_cast<uint32_t *>(outputBufferAllocation.getHostPtr());
        *outputBufferPtr                         = 0;
        flushAlloc(vk, device, outputBufferAllocation);
    }

    // Create a uniform buffer (to pass uniform constants)

    const VkDeviceSize uniformBufferSizeBytes = sizeof(uint32_t);
    const BufferWithMemory uniformBuffer(
        vk, device, allocator, makeBufferCreateInfo(uniformBufferSizeBytes, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
        MemoryRequirement::HostVisible);

    // Set the constants in the uniform buffer

    const uint32_t baseValue = 127;
    {
        const Allocation &uniformBufferAllocation = uniformBuffer.getAllocation();
        uint32_t *uniformBufferPtr                = static_cast<uint32_t *>(uniformBufferAllocation.getHostPtr());
        uniformBufferPtr[0]                       = baseValue;

        flushAlloc(vk, device, uniformBufferAllocation);
    }

    // Create descriptor set

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    const VkDescriptorImageInfo imageDescriptorInfo =
        makeDescriptorImageInfo(VK_NULL_HANDLE, *imageView, VK_IMAGE_LAYOUT_GENERAL);
    const VkDescriptorBufferInfo outputBufferDescriptorInfo =
        makeDescriptorBufferInfo(*outputBuffer, 0ull, outputBufferSizeBytes);
    const VkDescriptorBufferInfo uniformBufferDescriptorInfo =
        makeDescriptorBufferInfo(*uniformBuffer, 0ull, uniformBufferSizeBytes);
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputBufferDescriptorInfo)
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageDescriptorInfo)
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2u),
                     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBufferDescriptorInfo)
        .update(vk, device);

    // Perform the computation

    ComputePipelineWrapper pipeline0(vk, device, m_computePipelineConstructionType,
                                     m_context.getBinaryCollection().get("comp0"));
    pipeline0.setDescriptorSetLayout(descriptorSetLayout.get());
    pipeline0.buildPipeline();
    ComputePipelineWrapper pipeline1(vk, device, m_computePipelineConstructionType,
                                     m_context.getBinaryCollection().get("comp1"));
    pipeline1.setDescriptorSetLayout(descriptorSetLayout.get());
    pipeline1.buildPipeline();

    const VkBufferMemoryBarrier writeUniformConstantsBarrier = makeBufferMemoryBarrier(
        VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT, *uniformBuffer, 0ull, uniformBufferSizeBytes);

    const VkImageMemoryBarrier imageLayoutBarrier =
        makeImageMemoryBarrier(0u, 0u, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, *image, subresourceRange);

    const VkImageMemoryBarrier imageBarrierBetweenShaders =
        makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                               VK_IMAGE_LAYOUT_GENERAL, *image, subresourceRange);

    const VkBufferMemoryBarrier afterComputeBarrier = makeBufferMemoryBarrier(
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *outputBuffer, 0ull, outputBufferSizeBytes);

    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Start recording commands

    beginCommandBuffer(vk, *cmdBuffer);

    pipeline0.bind(*cmdBuffer);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline0.getPipelineLayout(), 0u, 1u,
                             &descriptorSet.get(), 0u, DE_NULL);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &writeUniformConstantsBarrier,
                          1, &imageLayoutBarrier);

    vk.cmdDispatch(*cmdBuffer, m_imageSize.x(), m_imageSize.y(), 1u);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 0,
                          (const VkBufferMemoryBarrier *)DE_NULL, 1, &imageBarrierBetweenShaders);

    // Switch to the second shader program
    pipeline1.bind(*cmdBuffer);

    vk.cmdDispatch(*cmdBuffer, m_imageSize.x(), m_imageSize.y(), 1u);
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &afterComputeBarrier, 0,
                          (const VkImageMemoryBarrier *)DE_NULL);

    endCommandBuffer(vk, *cmdBuffer);

    // Wait for completion

    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    // Validate the results

    const Allocation &outputBufferAllocation = outputBuffer.getAllocation();
    invalidateAlloc(vk, device, outputBufferAllocation);

    const int numValues       = multiplyComponents(m_imageSize);
    const uint32_t *bufferPtr = static_cast<uint32_t *>(outputBufferAllocation.getHostPtr());
    const uint32_t res        = *bufferPtr;
    uint32_t ref              = 0;

    for (int ndx = 0; ndx < numValues; ++ndx)
        ref += baseValue + ndx;

    if (res != ref)
    {
        std::ostringstream msg;
        msg << "ERROR: comparison failed, expected " << ref << ", got " << res;
        return tcu::TestStatus::fail(msg.str());
    }
    return tcu::TestStatus::pass("Compute succeeded");
}

class ComputeTestInstance : public vkt::TestInstance
{
public:
    ComputeTestInstance(Context &context, vk::ComputePipelineConstructionType computePipelineConstructionType,
                        bool useMaintenance5)
        : TestInstance(context)
        , m_numPhysDevices(1)
        , m_queueFamilyIndex(0)
        , m_computePipelineConstructionType(computePipelineConstructionType)
        , m_maintenance5(useMaintenance5)
    {
        createDeviceGroup();
    }

    ~ComputeTestInstance()
    {
    }

    void createDeviceGroup(void);
    const vk::DeviceInterface &getDeviceInterface(void)
    {
        return *m_deviceDriver;
    }
    vk::VkInstance getInstance(void)
    {
        return m_deviceGroupInstance;
    }
    vk::VkDevice getDevice(void)
    {
        return *m_logicalDevice;
    }
    vk::VkPhysicalDevice getPhysicalDevice(uint32_t i = 0)
    {
        return m_physicalDevices[i];
    }

protected:
    uint32_t m_numPhysDevices;
    uint32_t m_queueFamilyIndex;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
    bool m_maintenance5;

private:
    CustomInstance m_deviceGroupInstance;
    vk::Move<vk::VkDevice> m_logicalDevice;
    std::vector<vk::VkPhysicalDevice> m_physicalDevices;
#ifndef CTS_USES_VULKANSC
    de::MovePtr<vk::DeviceDriver> m_deviceDriver;
#else
    de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter> m_deviceDriver;
#endif // CTS_USES_VULKANSC
};

void ComputeTestInstance::createDeviceGroup(void)
{
    const tcu::CommandLine &cmdLine = m_context.getTestContext().getCommandLine();
    const uint32_t devGroupIdx      = cmdLine.getVKDeviceGroupId() - 1;
    const uint32_t physDeviceIdx    = cmdLine.getVKDeviceId() - 1;
    const float queuePriority       = 1.0f;
    const std::vector<std::string> requiredExtensions(1, "VK_KHR_device_group_creation");
    m_deviceGroupInstance = createCustomInstanceWithExtensions(m_context, requiredExtensions);
    std::vector<VkPhysicalDeviceGroupProperties> devGroupProperties =
        enumeratePhysicalDeviceGroups(m_context.getInstanceInterface(), m_deviceGroupInstance);
    m_numPhysDevices = devGroupProperties[devGroupIdx].physicalDeviceCount;
    std::vector<const char *> deviceExtensions;

    if (!isCoreDeviceExtension(m_context.getUsedApiVersion(), "VK_KHR_device_group"))
        deviceExtensions.push_back("VK_KHR_device_group");

    if (m_maintenance5)
        deviceExtensions.push_back("VK_KHR_maintenance5");

    //m_ma

    VkDeviceGroupDeviceCreateInfo deviceGroupInfo = {
        VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO,   //stype
        DE_NULL,                                             //pNext
        devGroupProperties[devGroupIdx].physicalDeviceCount, //physicalDeviceCount
        devGroupProperties[devGroupIdx].physicalDevices      //physicalDevices
    };
    const InstanceDriver &instance(m_deviceGroupInstance.getDriver());
    VkPhysicalDeviceFeatures2 deviceFeatures2 = initVulkanStructure();
    const VkPhysicalDeviceFeatures deviceFeatures =
        getPhysicalDeviceFeatures(instance, deviceGroupInfo.pPhysicalDevices[physDeviceIdx]);
    const std::vector<VkQueueFamilyProperties> queueProps = getPhysicalDeviceQueueFamilyProperties(
        instance, devGroupProperties[devGroupIdx].physicalDevices[physDeviceIdx]);

    deviceFeatures2.features = deviceFeatures;

#ifndef CTS_USES_VULKANSC
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures = initVulkanStructure();
    dynamicRenderingFeatures.dynamicRendering                            = VK_TRUE;
    VkPhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeatures = initVulkanStructure(&dynamicRenderingFeatures);
    shaderObjectFeatures.shaderObject                            = VK_TRUE;
    if (m_computePipelineConstructionType)
    {
        deviceExtensions.push_back("VK_EXT_shader_object");
        deviceFeatures2.pNext = &shaderObjectFeatures;
    }
#endif

    m_physicalDevices.resize(m_numPhysDevices);
    for (uint32_t physDevIdx = 0; physDevIdx < m_numPhysDevices; physDevIdx++)
        m_physicalDevices[physDevIdx] = devGroupProperties[devGroupIdx].physicalDevices[physDevIdx];

    for (size_t queueNdx = 0; queueNdx < queueProps.size(); queueNdx++)
    {
        if (queueProps[queueNdx].queueFlags & VK_QUEUE_COMPUTE_BIT)
            m_queueFamilyIndex = (uint32_t)queueNdx;
    }

    VkDeviceQueueCreateInfo queueInfo = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                                    // const void* pNext;
        (VkDeviceQueueCreateFlags)0u,               // VkDeviceQueueCreateFlags flags;
        m_queueFamilyIndex,                         // uint32_t queueFamilyIndex;
        1u,                                         // uint32_t queueCount;
        &queuePriority                              // const float* pQueuePriorities;
    };

    void *pNext = &deviceGroupInfo;
    if (deviceFeatures2.pNext != DE_NULL)
        deviceGroupInfo.pNext = &deviceFeatures2;

#ifdef CTS_USES_VULKANSC
    VkDeviceObjectReservationCreateInfo memReservationInfo = cmdLine.isSubProcess() ?
                                                                 m_context.getResourceInterface()->getStatMax() :
                                                                 resetDeviceObjectReservationCreateInfo();
    memReservationInfo.pNext                               = pNext;
    pNext                                                  = &memReservationInfo;

    VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
    sc10Features.pNext                              = pNext;
    pNext                                           = &sc10Features;
    VkPipelineCacheCreateInfo pcCI;
    std::vector<VkPipelinePoolSize> poolSizes;
    if (cmdLine.isSubProcess())
    {
        if (m_context.getResourceInterface()->getCacheDataSize() > 0)
        {
            pcCI = {
                VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType sType;
                DE_NULL,                                      // const void* pNext;
                VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
                    VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT, // VkPipelineCacheCreateFlags flags;
                m_context.getResourceInterface()->getCacheDataSize(),     // uintptr_t initialDataSize;
                m_context.getResourceInterface()->getCacheData()          // const void* pInitialData;
            };
            memReservationInfo.pipelineCacheCreateInfoCount = 1;
            memReservationInfo.pPipelineCacheCreateInfos    = &pcCI;
        }

        poolSizes = m_context.getResourceInterface()->getPipelinePoolSizes();
        if (!poolSizes.empty())
        {
            memReservationInfo.pipelinePoolSizeCount = uint32_t(poolSizes.size());
            memReservationInfo.pPipelinePoolSizes    = poolSizes.data();
        }
    }

#endif // CTS_USES_VULKANSC

    const VkDeviceCreateInfo deviceInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, // VkStructureType sType;
        pNext,                                // const void* pNext;
        (VkDeviceCreateFlags)0,               // VkDeviceCreateFlags flags;
        1u,                                   // uint32_t queueCreateInfoCount;
        &queueInfo,                           // const VkDeviceQueueCreateInfo* pQueueCreateInfos;
        0u,                                   // uint32_t enabledLayerCount;
        DE_NULL,                              // const char* const* ppEnabledLayerNames;
        uint32_t(deviceExtensions.size()),    // uint32_t enabledExtensionCount;
        (deviceExtensions.empty() ? DE_NULL : &deviceExtensions[0]), // const char* const* ppEnabledExtensionNames;
        deviceFeatures2.pNext == DE_NULL ? &deviceFeatures :
                                           DE_NULL, // const VkPhysicalDeviceFeatures* pEnabledFeatures;
    };

    m_logicalDevice = createCustomDevice(m_context.getTestContext().getCommandLine().isValidationEnabled(),
                                         m_context.getPlatformInterface(), m_deviceGroupInstance, instance,
                                         deviceGroupInfo.pPhysicalDevices[physDeviceIdx], &deviceInfo);
#ifndef CTS_USES_VULKANSC
    m_deviceDriver = de::MovePtr<DeviceDriver>(new DeviceDriver(m_context.getPlatformInterface(), m_deviceGroupInstance,
                                                                *m_logicalDevice, m_context.getUsedApiVersion(),
                                                                m_context.getTestContext().getCommandLine()));
#else
    m_deviceDriver = de::MovePtr<DeviceDriverSC, DeinitDeviceDeleter>(
        new DeviceDriverSC(m_context.getPlatformInterface(), m_context.getInstance(), *m_logicalDevice,
                           m_context.getTestContext().getCommandLine(), m_context.getResourceInterface(),
                           m_context.getDeviceVulkanSC10Properties(), m_context.getDeviceProperties(),
                           m_context.getUsedApiVersion()),
        vk::DeinitDeviceDeleter(m_context.getResourceInterface().get(), *m_logicalDevice));
#endif // CTS_USES_VULKANSC
}

class DispatchBaseTest : public vkt::TestCase
{
public:
    DispatchBaseTest(tcu::TestContext &testCtx, const std::string &name, const uint32_t numValues,
                     const tcu::IVec3 &localsize, const tcu::IVec3 &worksize, const tcu::IVec3 &splitsize,
                     const vk::ComputePipelineConstructionType computePipelineConstructionType,
                     const bool useMaintenance5);

    virtual void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &sourceCollections) const;
    TestInstance *createInstance(Context &context) const;

private:
    const uint32_t m_numValues;
    const tcu::IVec3 m_localSize;
    const tcu::IVec3 m_workSize;
    const tcu::IVec3 m_splitSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
    const bool m_useMaintenance5;
};

class DispatchBaseTestInstance : public ComputeTestInstance
{
public:
    DispatchBaseTestInstance(Context &context, const uint32_t numValues, const tcu::IVec3 &localsize,
                             const tcu::IVec3 &worksize, const tcu::IVec3 &splitsize,
                             const vk::ComputePipelineConstructionType computePipelineConstructionType,
                             const bool useMaintenance5);

    bool isInputVectorValid(const tcu::IVec3 &small, const tcu::IVec3 &big);
    tcu::TestStatus iterate(void);

private:
    const uint32_t m_numValues;
    const tcu::IVec3 m_localSize;
    const tcu::IVec3 m_workSize;
    const tcu::IVec3 m_splitWorkSize;
    const bool m_useMaintenance5;
};

DispatchBaseTest::DispatchBaseTest(tcu::TestContext &testCtx, const std::string &name, const uint32_t numValues,
                                   const tcu::IVec3 &localsize, const tcu::IVec3 &worksize, const tcu::IVec3 &splitsize,
                                   const vk::ComputePipelineConstructionType computePipelineConstructionType,
                                   const bool useMaintenance5)
    : TestCase(testCtx, name)
    , m_numValues(numValues)
    , m_localSize(localsize)
    , m_workSize(worksize)
    , m_splitSize(splitsize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
    , m_useMaintenance5(useMaintenance5)
{
}

void DispatchBaseTest::checkSupport(Context &context) const
{
    checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                  m_computePipelineConstructionType);
    if (m_useMaintenance5)
        context.requireDeviceFunctionality("VK_KHR_maintenance5");
}

void DispatchBaseTest::initPrograms(SourceCollections &sourceCollections) const
{
    std::ostringstream src;
    src << "#version 310 es\n"
        << "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y()
        << ", local_size_z = " << m_localSize.z() << ") in;\n"

        << "layout(binding = 0) buffer InOut {\n"
        << "    uint values[" << de::toString(m_numValues) << "];\n"
        << "} sb_inout;\n"

        << "layout(binding = 1) readonly uniform uniformInput {\n"
        << "    uvec3 gridSize;\n"
        << "} ubo_in;\n"

        << "void main (void) {\n"
        << "    uvec3 size = ubo_in.gridSize * gl_WorkGroupSize;\n"
        << "    uint numValuesPerInv = uint(sb_inout.values.length()) / (size.x*size.y*size.z);\n"
        << "    uint index = size.x*size.y*gl_GlobalInvocationID.z + size.x*gl_GlobalInvocationID.y + "
           "gl_GlobalInvocationID.x;\n"
        << "    uint offset = numValuesPerInv*index;\n"
        << "    for (uint ndx = 0u; ndx < numValuesPerInv; ndx++)\n"
        << "        sb_inout.values[offset + ndx] = ~sb_inout.values[offset + ndx];\n"
        << "}\n";

    sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance *DispatchBaseTest::createInstance(Context &context) const
{
    return new DispatchBaseTestInstance(context, m_numValues, m_localSize, m_workSize, m_splitSize,
                                        m_computePipelineConstructionType, m_useMaintenance5);
}

DispatchBaseTestInstance::DispatchBaseTestInstance(
    Context &context, const uint32_t numValues, const tcu::IVec3 &localsize, const tcu::IVec3 &worksize,
    const tcu::IVec3 &splitsize, const vk::ComputePipelineConstructionType computePipelineConstructionType,
    const bool useMaintenance5)

    : ComputeTestInstance(context, computePipelineConstructionType, useMaintenance5)
    , m_numValues(numValues)
    , m_localSize(localsize)
    , m_workSize(worksize)
    , m_splitWorkSize(splitsize)
    , m_useMaintenance5(useMaintenance5)
{
    // For easy work distribution across physical devices:
    // WorkSize should be a multiple of SplitWorkSize only in the X component
    if ((!isInputVectorValid(m_splitWorkSize, m_workSize)) || (m_workSize.x() <= m_splitWorkSize.x()) ||
        (m_workSize.y() != m_splitWorkSize.y()) || (m_workSize.z() != m_splitWorkSize.z()))
        TCU_THROW(TestError, "Invalid Input.");

    // For easy work distribution within the same physical device:
    // SplitWorkSize should be a multiple of localSize in Y or Z component
    if ((!isInputVectorValid(m_localSize, m_splitWorkSize)) || (m_localSize.x() != m_splitWorkSize.x()) ||
        (m_localSize.y() >= m_splitWorkSize.y()) || (m_localSize.z() >= m_splitWorkSize.z()))
        TCU_THROW(TestError, "Invalid Input.");

    if ((multiplyComponents(m_workSize) / multiplyComponents(m_splitWorkSize)) < (int32_t)m_numPhysDevices)
        TCU_THROW(TestError, "Not enough work to distribute across all physical devices.");

    uint32_t totalWork = multiplyComponents(m_workSize) * multiplyComponents(m_localSize);
    if ((totalWork > numValues) || (numValues % totalWork != 0))
        TCU_THROW(TestError, "Buffer too small/not aligned to cover all values.");
}

bool DispatchBaseTestInstance::isInputVectorValid(const tcu::IVec3 &small, const tcu::IVec3 &big)
{
    if (((big.x() < small.x()) || (big.y() < small.y()) || (big.z() < small.z())) ||
        ((big.x() % small.x() != 0) || (big.y() % small.y() != 0) || (big.z() % small.z() != 0)))
        return false;
    return true;
}

tcu::TestStatus DispatchBaseTestInstance::iterate(void)
{
    const DeviceInterface &vk = getDeviceInterface();
    const VkDevice device     = getDevice();
    const VkQueue queue       = getDeviceQueue(vk, device, m_queueFamilyIndex, 0);
    SimpleAllocator allocator(vk, device,
                              getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), getPhysicalDevice()));
    uint32_t totalWorkloadSize = 0;

    // Create an uniform and input/output buffer
    const uint32_t uniformBufSize             = 3; // Pass the compute grid size
    const VkDeviceSize uniformBufferSizeBytes = sizeof(uint32_t) * uniformBufSize;
    const BufferWithMemory uniformBuffer(
        vk, device, allocator, makeBufferCreateInfo(uniformBufferSizeBytes, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
        MemoryRequirement::HostVisible);

    const VkDeviceSize bufferSizeBytes = sizeof(uint32_t) * m_numValues;
    const BufferWithMemory buffer(vk, device, allocator,
                                  makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                  MemoryRequirement::HostVisible);

    // Fill the buffers with data
    typedef std::vector<uint32_t> data_vector_t;
    data_vector_t uniformInputData(uniformBufSize);
    data_vector_t inputData(m_numValues);

    {
        const Allocation &bufferAllocation = uniformBuffer.getAllocation();
        uint32_t *bufferPtr                = static_cast<uint32_t *>(bufferAllocation.getHostPtr());
        uniformInputData[0] = *bufferPtr++ = m_workSize.x();
        uniformInputData[1] = *bufferPtr++ = m_workSize.y();
        uniformInputData[2] = *bufferPtr++ = m_workSize.z();
        flushAlloc(vk, device, bufferAllocation);
    }

    {
        de::Random rnd(0x82ce7f);
        const Allocation &bufferAllocation = buffer.getAllocation();
        uint32_t *bufferPtr                = static_cast<uint32_t *>(bufferAllocation.getHostPtr());
        for (uint32_t i = 0; i < m_numValues; ++i)
            inputData[i] = *bufferPtr++ = rnd.getUint32();

        flushAlloc(vk, device, bufferAllocation);
    }

    // Create descriptor set
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    const VkDescriptorBufferInfo bufferDescriptorInfo = makeDescriptorBufferInfo(*buffer, 0ull, bufferSizeBytes);
    const VkDescriptorBufferInfo uniformBufferDescriptorInfo =
        makeDescriptorBufferInfo(*uniformBuffer, 0ull, uniformBufferSizeBytes);

    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo)
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBufferDescriptorInfo)
        .update(vk, device);

    ComputePipelineWrapper pipeline(vk, device, m_computePipelineConstructionType,
                                    m_context.getBinaryCollection().get("comp"));
    pipeline.setDescriptorSetLayout(descriptorSetLayout.get());
    pipeline.setPipelineCreateFlags(VK_PIPELINE_CREATE_DISPATCH_BASE);

#ifndef CTS_USES_VULKANSC
    if (m_useMaintenance5)
    {
        VkPipelineCreateFlags2CreateInfoKHR pipelineFlags2CreateInfo = initVulkanStructure();
        pipelineFlags2CreateInfo.flags                               = VK_PIPELINE_CREATE_2_DISPATCH_BASE_BIT_KHR;
        pipeline.setPipelineCreatePNext(&pipelineFlags2CreateInfo);
        pipeline.setPipelineCreateFlags(0);
    }
#else
    DE_UNREF(m_useMaintenance5);
#endif // CTS_USES_VULKANSC

    pipeline.buildPipeline();

    const VkBufferMemoryBarrier hostWriteBarrier =
        makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *buffer, 0ull, bufferSizeBytes);
    const VkBufferMemoryBarrier hostUniformWriteBarrier = makeBufferMemoryBarrier(
        VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT, *uniformBuffer, 0ull, uniformBufferSizeBytes);

    const VkBufferMemoryBarrier shaderWriteBarrier =
        makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer, 0ull, bufferSizeBytes);

    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, m_queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Start recording commands
    beginCommandBuffer(vk, *cmdBuffer);

    pipeline.bind(*cmdBuffer);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getPipelineLayout(), 0u, 1u,
                             &descriptorSet.get(), 0u, DE_NULL);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &hostUniformWriteBarrier, 0,
                          (const VkImageMemoryBarrier *)DE_NULL);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &hostWriteBarrier, 0,
                          (const VkImageMemoryBarrier *)DE_NULL);

    // Split the workload across all physical devices based on m_splitWorkSize.x()
    for (uint32_t physDevIdx = 0; physDevIdx < m_numPhysDevices; physDevIdx++)
    {
        uint32_t baseGroupX = physDevIdx * m_splitWorkSize.x();
        uint32_t baseGroupY = 0;
        uint32_t baseGroupZ = 0;

        // Split the workload within the physical device based on m_localSize.y() and m_localSize.z()
        for (int32_t localIdxY = 0; localIdxY < (m_splitWorkSize.y() / m_localSize.y()); localIdxY++)
        {
            for (int32_t localIdxZ = 0; localIdxZ < (m_splitWorkSize.z() / m_localSize.z()); localIdxZ++)
            {
                uint32_t offsetX = baseGroupX;
                uint32_t offsetY = baseGroupY + localIdxY * m_localSize.y();
                uint32_t offsetZ = baseGroupZ + localIdxZ * m_localSize.z();

                uint32_t localSizeX =
                    (physDevIdx == (m_numPhysDevices - 1)) ? m_workSize.x() - baseGroupX : m_localSize.x();
                uint32_t localSizeY = m_localSize.y();
                uint32_t localSizeZ = m_localSize.z();

                totalWorkloadSize += (localSizeX * localSizeY * localSizeZ);
                vk.cmdDispatchBase(*cmdBuffer, offsetX, offsetY, offsetZ, localSizeX, localSizeY, localSizeZ);
            }
        }
    }

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &shaderWriteBarrier, 0,
                          (const VkImageMemoryBarrier *)DE_NULL);

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    if (totalWorkloadSize != uint32_t(multiplyComponents(m_workSize)))
        TCU_THROW(TestError, "Not covering the entire workload.");

    // Validate the results
    const Allocation &bufferAllocation = buffer.getAllocation();
    invalidateAlloc(vk, device, bufferAllocation);
    const uint32_t *bufferPtr = static_cast<uint32_t *>(bufferAllocation.getHostPtr());

    for (uint32_t ndx = 0; ndx < m_numValues; ++ndx)
    {
        const uint32_t res = bufferPtr[ndx];
        const uint32_t ref = ~inputData[ndx];

        if (res != ref)
        {
            std::ostringstream msg;
            msg << "Comparison failed for InOut.values[" << ndx << "]";
            return tcu::TestStatus::fail(msg.str());
        }
    }
    return tcu::TestStatus::pass("Compute succeeded");
}

class DeviceIndexTest : public vkt::TestCase
{
public:
    DeviceIndexTest(tcu::TestContext &testCtx, const std::string &name, const uint32_t numValues,
                    const tcu::IVec3 &localsize, const tcu::IVec3 &splitsize,
                    const vk::ComputePipelineConstructionType computePipelineConstructionType);

    virtual void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &sourceCollections) const;
    TestInstance *createInstance(Context &context) const;

private:
    const uint32_t m_numValues;
    const tcu::IVec3 m_localSize;
    const tcu::IVec3 m_workSize;
    const tcu::IVec3 m_splitSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

class DeviceIndexTestInstance : public ComputeTestInstance
{
public:
    DeviceIndexTestInstance(Context &context, const uint32_t numValues, const tcu::IVec3 &localsize,
                            const tcu::IVec3 &worksize,
                            const vk::ComputePipelineConstructionType computePipelineConstructionType);
    tcu::TestStatus iterate(void);

private:
    const uint32_t m_numValues;
    const tcu::IVec3 m_localSize;
    tcu::IVec3 m_workSize;
};

DeviceIndexTest::DeviceIndexTest(tcu::TestContext &testCtx, const std::string &name, const uint32_t numValues,
                                 const tcu::IVec3 &localsize, const tcu::IVec3 &worksize,
                                 const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestCase(testCtx, name)
    , m_numValues(numValues)
    , m_localSize(localsize)
    , m_workSize(worksize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

void DeviceIndexTest::checkSupport(Context &context) const
{
    checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                  m_computePipelineConstructionType);
}

void DeviceIndexTest::initPrograms(SourceCollections &sourceCollections) const
{
    std::ostringstream src;
    src << "#version 310 es\n"
        << "#extension GL_EXT_device_group : require\n"
        << "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y()
        << ", local_size_z = " << m_localSize.z() << ") in;\n"

        << "layout(binding = 0) buffer InOut {\n"
        << "    uint values[" << de::toString(m_numValues) << "];\n"
        << "} sb_inout;\n"

        << "layout(binding = 1) readonly uniform uniformInput {\n"
        << "    uint baseOffset[1+" << VK_MAX_DEVICE_GROUP_SIZE << "];\n"
        << "} ubo_in;\n"

        << "void main (void) {\n"
        << "    uvec3 size = gl_NumWorkGroups * gl_WorkGroupSize;\n"
        << "    uint numValuesPerInv = uint(sb_inout.values.length()) / (size.x*size.y*size.z);\n"
        << "    uint index = size.x*size.y*gl_GlobalInvocationID.z + size.x*gl_GlobalInvocationID.y + "
           "gl_GlobalInvocationID.x;\n"
        << "    uint offset = numValuesPerInv*index;\n"
        << "    for (uint ndx = 0u; ndx < numValuesPerInv; ndx++)\n"
        << "        sb_inout.values[offset + ndx] = ubo_in.baseOffset[0] + ubo_in.baseOffset[gl_DeviceIndex + 1];\n"
        << "}\n";

    sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance *DeviceIndexTest::createInstance(Context &context) const
{
    return new DeviceIndexTestInstance(context, m_numValues, m_localSize, m_workSize,
                                       m_computePipelineConstructionType);
}

DeviceIndexTestInstance::DeviceIndexTestInstance(
    Context &context, const uint32_t numValues, const tcu::IVec3 &localsize, const tcu::IVec3 &worksize,
    const vk::ComputePipelineConstructionType computePipelineConstructionType)

    : ComputeTestInstance(context, computePipelineConstructionType, false)
    , m_numValues(numValues)
    , m_localSize(localsize)
    , m_workSize(worksize)
{
}

tcu::TestStatus DeviceIndexTestInstance::iterate(void)
{
    const DeviceInterface &vk = getDeviceInterface();
    const VkDevice device     = getDevice();
    const VkQueue queue       = getDeviceQueue(vk, device, m_queueFamilyIndex, 0);
    SimpleAllocator allocator(vk, device,
                              getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), getPhysicalDevice()));
    const uint32_t allocDeviceMask = (1 << m_numPhysDevices) - 1;
    de::Random rnd(0x82ce7f);
    Move<VkBuffer> sboBuffer;
    vk::Move<vk::VkDeviceMemory> sboBufferMemory;

    // Create an uniform and output buffer
    const uint32_t uniformBufSize             = 4 * (1 + VK_MAX_DEVICE_GROUP_SIZE);
    const VkDeviceSize uniformBufferSizeBytes = sizeof(uint32_t) * uniformBufSize;
    const BufferWithMemory uniformBuffer(
        vk, device, allocator, makeBufferCreateInfo(uniformBufferSizeBytes, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
        MemoryRequirement::HostVisible);

    const VkDeviceSize bufferSizeBytes = sizeof(uint32_t) * m_numValues;
    const BufferWithMemory checkBuffer(vk, device, allocator,
                                       makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT),
                                       MemoryRequirement::HostVisible);

    // create SBO buffer
    {
        const VkBufferCreateInfo sboBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,                                  // sType
            DE_NULL,                                                               // pNext
            0u,                                                                    // flags
            (VkDeviceSize)bufferSizeBytes,                                         // size
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, // usage
            VK_SHARING_MODE_EXCLUSIVE,                                             // sharingMode
            1u,                                                                    // queueFamilyIndexCount
            &m_queueFamilyIndex,                                                   // pQueueFamilyIndices
        };
        sboBuffer = createBuffer(vk, device, &sboBufferParams);

        VkMemoryRequirements memReqs = getBufferMemoryRequirements(vk, device, sboBuffer.get());
        uint32_t memoryTypeNdx       = 0;
        const VkPhysicalDeviceMemoryProperties deviceMemProps =
            getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), getPhysicalDevice());
        for (memoryTypeNdx = 0; memoryTypeNdx < deviceMemProps.memoryTypeCount; memoryTypeNdx++)
        {
            if ((memReqs.memoryTypeBits & (1u << memoryTypeNdx)) != 0 &&
                (deviceMemProps.memoryTypes[memoryTypeNdx].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ==
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                break;
        }
        if (memoryTypeNdx == deviceMemProps.memoryTypeCount)
            TCU_THROW(NotSupportedError, "No compatible memory type found");

        const VkMemoryAllocateFlagsInfo allocDeviceMaskInfo = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, // sType
            DE_NULL,                                      // pNext
            VK_MEMORY_ALLOCATE_DEVICE_MASK_BIT,           // flags
            allocDeviceMask,                              // deviceMask
        };

        VkMemoryAllocateInfo allocInfo = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, // sType
            &allocDeviceMaskInfo,                   // pNext
            memReqs.size,                           // allocationSize
            memoryTypeNdx,                          // memoryTypeIndex
        };

        sboBufferMemory = allocateMemory(vk, device, &allocInfo);
        VK_CHECK(vk.bindBufferMemory(device, *sboBuffer, sboBufferMemory.get(), 0));
    }

    // Fill the buffers with data
    typedef std::vector<uint32_t> data_vector_t;
    data_vector_t uniformInputData(uniformBufSize, 0);

    {
        const Allocation &bufferAllocation = uniformBuffer.getAllocation();
        uint32_t *bufferPtr                = static_cast<uint32_t *>(bufferAllocation.getHostPtr());
        for (uint32_t i = 0; i < uniformBufSize; ++i)
            uniformInputData[i] = *bufferPtr++ = rnd.getUint32() / 10; // divide to prevent overflow in addition

        flushAlloc(vk, device, bufferAllocation);
    }

    // Create descriptor set
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    const VkDescriptorBufferInfo bufferDescriptorInfo = makeDescriptorBufferInfo(*sboBuffer, 0ull, bufferSizeBytes);
    const VkDescriptorBufferInfo uniformBufferDescriptorInfo =
        makeDescriptorBufferInfo(*uniformBuffer, 0ull, uniformBufferSizeBytes);

    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo)
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBufferDescriptorInfo)
        .update(vk, device);

    ComputePipelineWrapper pipeline(vk, device, m_computePipelineConstructionType,
                                    m_context.getBinaryCollection().get("comp"));
    pipeline.setDescriptorSetLayout(descriptorSetLayout.get());
    pipeline.buildPipeline();

    const VkBufferMemoryBarrier hostUniformWriteBarrier = makeBufferMemoryBarrier(
        VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT, *uniformBuffer, 0ull, uniformBufferSizeBytes);
    const VkBufferMemoryBarrier shaderWriteBarrier = makeBufferMemoryBarrier(
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, *sboBuffer, 0ull, bufferSizeBytes);

    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, m_queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Verify multiple device masks
    for (uint32_t physDevMask = 1; physDevMask < (1u << m_numPhysDevices); physDevMask++)
    {
        uint32_t constantValPerLoop = 0;
        {
            const Allocation &bufferAllocation = uniformBuffer.getAllocation();
            uint32_t *bufferPtr                = static_cast<uint32_t *>(bufferAllocation.getHostPtr());
            constantValPerLoop = *bufferPtr = rnd.getUint32() / 10; // divide to prevent overflow in addition
            flushAlloc(vk, device, bufferAllocation);
        }
        beginCommandBuffer(vk, *cmdBuffer);

        pipeline.bind(*cmdBuffer);
        vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getPipelineLayout(), 0u, 1u,
                                 &descriptorSet.get(), 0u, DE_NULL);
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &hostUniformWriteBarrier, 0,
                              (const VkImageMemoryBarrier *)DE_NULL);

        vk.cmdSetDeviceMask(*cmdBuffer, physDevMask);
        vk.cmdDispatch(*cmdBuffer, m_workSize.x(), m_workSize.y(), m_workSize.z());

        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &shaderWriteBarrier, 0,
                              (const VkImageMemoryBarrier *)DE_NULL);

        endCommandBuffer(vk, *cmdBuffer);
        submitCommandsAndWait(vk, device, queue, *cmdBuffer, true, physDevMask);
        m_context.resetCommandPoolForVKSC(device, *cmdPool);

        // Validate the results on all physical devices where compute shader was launched
        const VkBufferMemoryBarrier srcBufferBarrier = makeBufferMemoryBarrier(
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, *sboBuffer, 0ull, bufferSizeBytes);
        const VkBufferMemoryBarrier dstBufferBarrier = makeBufferMemoryBarrier(
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *checkBuffer, 0ull, bufferSizeBytes);
        const VkBufferCopy copyParams = {
            (VkDeviceSize)0u, // srcOffset
            (VkDeviceSize)0u, // dstOffset
            bufferSizeBytes   // size
        };

        for (uint32_t physDevIdx = 0; physDevIdx < m_numPhysDevices; physDevIdx++)
        {
            if (!(1 << physDevIdx & physDevMask))
                continue;

            const uint32_t deviceMask = 1 << physDevIdx;

            beginCommandBuffer(vk, *cmdBuffer);
            vk.cmdSetDeviceMask(*cmdBuffer, deviceMask);
            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &srcBufferBarrier, 0,
                                  (const VkImageMemoryBarrier *)DE_NULL);
            vk.cmdCopyBuffer(*cmdBuffer, *sboBuffer, *checkBuffer, 1, &copyParams);
            vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                  (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &dstBufferBarrier, 0,
                                  (const VkImageMemoryBarrier *)DE_NULL);

            endCommandBuffer(vk, *cmdBuffer);
            submitCommandsAndWait(vk, device, queue, *cmdBuffer, true, deviceMask);

            const Allocation &bufferAllocation = checkBuffer.getAllocation();
            invalidateAlloc(vk, device, bufferAllocation);
            const uint32_t *bufferPtr = static_cast<uint32_t *>(bufferAllocation.getHostPtr());

            for (uint32_t ndx = 0; ndx < m_numValues; ++ndx)
            {
                const uint32_t res = bufferPtr[ndx];
                const uint32_t ref = constantValPerLoop + uniformInputData[4 * (physDevIdx + 1)];

                if (res != ref)
                {
                    std::ostringstream msg;
                    msg << "Comparison failed on physical device " << getPhysicalDevice(physDevIdx) << " ( deviceMask "
                        << deviceMask << " ) for InOut.values[" << ndx << "]";
                    return tcu::TestStatus::fail(msg.str());
                }
            }
        }
    }

    return tcu::TestStatus::pass("Compute succeeded");
}

class ConcurrentCompute : public vkt::TestCase
{
public:
    ConcurrentCompute(tcu::TestContext &testCtx, const std::string &name,
                      const vk::ComputePipelineConstructionType computePipelineConstructionType);

    virtual void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &sourceCollections) const;
    TestInstance *createInstance(Context &context) const;

    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

class ConcurrentComputeInstance : public vkt::TestInstance
{
public:
    ConcurrentComputeInstance(Context &context,
                              const vk::ComputePipelineConstructionType computePipelineConstructionType);

    tcu::TestStatus iterate(void);

private:
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

ConcurrentCompute::ConcurrentCompute(tcu::TestContext &testCtx, const std::string &name,
                                     const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestCase(testCtx, name)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

void ConcurrentCompute::checkSupport(Context &context) const
{
    checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                  m_computePipelineConstructionType);
}

void ConcurrentCompute::initPrograms(SourceCollections &sourceCollections) const
{
    std::ostringstream src;
    src << "#version 310 es\n"
        << "layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
        << "layout(binding = 0) buffer InOut {\n"
        << "    uint values[1024];\n"
        << "} sb_inout;\n"
        << "void main (void) {\n"
        << "    uvec3 size           = gl_NumWorkGroups * gl_WorkGroupSize;\n"
        << "    uint numValuesPerInv = uint(sb_inout.values.length()) / (size.x*size.y*size.z);\n"
        << "    uint groupNdx        = size.x*size.y*gl_GlobalInvocationID.z + size.x*gl_GlobalInvocationID.y + "
           "gl_GlobalInvocationID.x;\n"
        << "    uint offset          = numValuesPerInv*groupNdx;\n"
        << "\n"
        << "    for (uint ndx = 0u; ndx < numValuesPerInv; ndx++)\n"
        << "        sb_inout.values[offset + ndx] = ~sb_inout.values[offset + ndx];\n"
        << "}\n";

    sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance *ConcurrentCompute::createInstance(Context &context) const
{
    return new ConcurrentComputeInstance(context, m_computePipelineConstructionType);
}

ConcurrentComputeInstance::ConcurrentComputeInstance(
    Context &context, const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestInstance(context)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

tcu::TestStatus ConcurrentComputeInstance::iterate(void)
{
    enum
    {
        NO_MATCH_FOUND = ~((uint32_t)0),
        ERROR_NONE     = 0,
        ERROR_WAIT     = 1,
        ERROR_ORDER    = 2
    };

    struct Queues
    {
        VkQueue queue;
        uint32_t queueFamilyIndex;
    };

    // const DeviceInterface& vk = m_context.getDeviceInterface();
    const uint32_t numValues = 1024;
    const CustomInstance instance(createCustomInstanceFromContext(m_context));
    const InstanceDriver &instanceDriver(instance.getDriver());
    const VkPhysicalDevice physicalDevice =
        chooseDevice(instanceDriver, instance, m_context.getTestContext().getCommandLine());
    tcu::TestLog &log = m_context.getTestContext().getLog();
    vk::Move<vk::VkDevice> logicalDevice;
    std::vector<VkQueueFamilyProperties> queueFamilyProperties;
    VkDeviceCreateInfo deviceInfo;
    VkPhysicalDeviceFeatures2 deviceFeatures2 = initVulkanStructure();
    VkPhysicalDeviceFeatures deviceFeatures;
    const float queuePriorities[2] = {1.0f, 0.0f};
    VkDeviceQueueCreateInfo queueInfos[2];
    Queues queues[2] = {{DE_NULL, (uint32_t)NO_MATCH_FOUND}, {DE_NULL, (uint32_t)NO_MATCH_FOUND}};

    queueFamilyProperties = getPhysicalDeviceQueueFamilyProperties(instanceDriver, physicalDevice);

    for (uint32_t queueNdx = 0; queueNdx < queueFamilyProperties.size(); ++queueNdx)
    {
        if (queueFamilyProperties[queueNdx].queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            if (NO_MATCH_FOUND == queues[0].queueFamilyIndex)
                queues[0].queueFamilyIndex = queueNdx;

            if (queues[0].queueFamilyIndex != queueNdx || queueFamilyProperties[queueNdx].queueCount > 1u)
            {
                queues[1].queueFamilyIndex = queueNdx;
                break;
            }
        }
    }

    if (queues[0].queueFamilyIndex == NO_MATCH_FOUND || queues[1].queueFamilyIndex == NO_MATCH_FOUND)
        TCU_THROW(NotSupportedError, "Queues couldn't be created");

    for (int queueNdx = 0; queueNdx < 2; ++queueNdx)
    {
        VkDeviceQueueCreateInfo queueInfo;
        deMemset(&queueInfo, 0, sizeof(queueInfo));

        queueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.pNext            = DE_NULL;
        queueInfo.flags            = (VkDeviceQueueCreateFlags)0u;
        queueInfo.queueFamilyIndex = queues[queueNdx].queueFamilyIndex;
        queueInfo.queueCount       = (queues[0].queueFamilyIndex == queues[1].queueFamilyIndex) ? 2 : 1;
        queueInfo.pQueuePriorities = (queueInfo.queueCount == 2) ? queuePriorities : &queuePriorities[queueNdx];

        queueInfos[queueNdx] = queueInfo;

        if (queues[0].queueFamilyIndex == queues[1].queueFamilyIndex)
            break;
    }

    void *pNext = DE_NULL;

    deMemset(&deviceInfo, 0, sizeof(deviceInfo));
    instanceDriver.getPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

    deviceFeatures2.features = deviceFeatures;

    std::vector<const char *> deviceExtensions;

#ifndef CTS_USES_VULKANSC
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures = initVulkanStructure();
    dynamicRenderingFeatures.dynamicRendering                            = VK_TRUE;
    VkPhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeatures = initVulkanStructure(&dynamicRenderingFeatures);
    shaderObjectFeatures.shaderObject                            = VK_TRUE;

    if (m_computePipelineConstructionType != COMPUTE_PIPELINE_CONSTRUCTION_TYPE_PIPELINE)
    {
        deviceExtensions.push_back("VK_EXT_shader_object");
        deviceFeatures2.pNext = &shaderObjectFeatures;
        pNext                 = &deviceFeatures2;
    }
#endif

#ifdef CTS_USES_VULKANSC
    VkDeviceObjectReservationCreateInfo memReservationInfo =
        m_context.getTestContext().getCommandLine().isSubProcess() ? m_context.getResourceInterface()->getStatMax() :
                                                                     resetDeviceObjectReservationCreateInfo();
    memReservationInfo.pNext = pNext;
    pNext                    = &memReservationInfo;

    VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
    sc10Features.pNext                              = pNext;
    pNext                                           = &sc10Features;

    VkPipelineCacheCreateInfo pcCI;
    std::vector<VkPipelinePoolSize> poolSizes;
    if (m_context.getTestContext().getCommandLine().isSubProcess())
    {
        if (m_context.getResourceInterface()->getCacheDataSize() > 0)
        {
            pcCI = {
                VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType sType;
                DE_NULL,                                      // const void* pNext;
                VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
                    VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT, // VkPipelineCacheCreateFlags flags;
                m_context.getResourceInterface()->getCacheDataSize(),     // uintptr_t initialDataSize;
                m_context.getResourceInterface()->getCacheData()          // const void* pInitialData;
            };
            memReservationInfo.pipelineCacheCreateInfoCount = 1;
            memReservationInfo.pPipelineCacheCreateInfos    = &pcCI;
        }

        poolSizes = m_context.getResourceInterface()->getPipelinePoolSizes();
        if (!poolSizes.empty())
        {
            memReservationInfo.pipelinePoolSizeCount = uint32_t(poolSizes.size());
            memReservationInfo.pPipelinePoolSizes    = poolSizes.data();
        }
    }
#endif // CTS_USES_VULKANSC

    deviceInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext                   = pNext;
    deviceInfo.enabledExtensionCount   = (uint32_t)deviceExtensions.size();
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceInfo.enabledLayerCount       = 0u;
    deviceInfo.ppEnabledLayerNames     = DE_NULL;
    deviceInfo.pEnabledFeatures        = (deviceFeatures2.pNext == DE_NULL) ? &deviceFeatures : DE_NULL;
    deviceInfo.queueCreateInfoCount    = (queues[0].queueFamilyIndex == queues[1].queueFamilyIndex) ? 1 : 2;
    deviceInfo.pQueueCreateInfos       = queueInfos;

    logicalDevice =
        createCustomDevice(m_context.getTestContext().getCommandLine().isValidationEnabled(),
                           m_context.getPlatformInterface(), instance, instanceDriver, physicalDevice, &deviceInfo);

#ifndef CTS_USES_VULKANSC
    de::MovePtr<vk::DeviceDriver> deviceDriver = de::MovePtr<DeviceDriver>(
        new DeviceDriver(m_context.getPlatformInterface(), instance, *logicalDevice, m_context.getUsedApiVersion(),
                         m_context.getTestContext().getCommandLine()));
#else
    de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter> deviceDriver =
        de::MovePtr<DeviceDriverSC, DeinitDeviceDeleter>(
            new DeviceDriverSC(m_context.getPlatformInterface(), instance, *logicalDevice,
                               m_context.getTestContext().getCommandLine(), m_context.getResourceInterface(),
                               m_context.getDeviceVulkanSC10Properties(), m_context.getDeviceProperties(),
                               m_context.getUsedApiVersion()),
            vk::DeinitDeviceDeleter(m_context.getResourceInterface().get(), *logicalDevice));
#endif // CTS_USES_VULKANSC
    vk::DeviceInterface &vk = *deviceDriver;

    for (uint32_t queueReqNdx = 0; queueReqNdx < 2; ++queueReqNdx)
    {
        if (queues[0].queueFamilyIndex == queues[1].queueFamilyIndex)
            vk.getDeviceQueue(*logicalDevice, queues[queueReqNdx].queueFamilyIndex, queueReqNdx,
                              &queues[queueReqNdx].queue);
        else
            vk.getDeviceQueue(*logicalDevice, queues[queueReqNdx].queueFamilyIndex, 0u, &queues[queueReqNdx].queue);
    }

    // Create an input/output buffers
    const VkPhysicalDeviceMemoryProperties memoryProperties =
        vk::getPhysicalDeviceMemoryProperties(instanceDriver, physicalDevice);

    de::MovePtr<SimpleAllocator> allocator =
        de::MovePtr<SimpleAllocator>(new SimpleAllocator(vk, *logicalDevice, memoryProperties));
    const VkDeviceSize bufferSizeBytes = sizeof(uint32_t) * numValues;
    const BufferWithMemory buffer1(vk, *logicalDevice, *allocator,
                                   makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                   MemoryRequirement::HostVisible);
    const BufferWithMemory buffer2(vk, *logicalDevice, *allocator,
                                   makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                   MemoryRequirement::HostVisible);

    // Fill the buffers with data

    typedef std::vector<uint32_t> data_vector_t;
    data_vector_t inputData(numValues);

    {
        de::Random rnd(0x82ce7f);
        const Allocation &bufferAllocation1 = buffer1.getAllocation();
        const Allocation &bufferAllocation2 = buffer2.getAllocation();
        uint32_t *bufferPtr1                = static_cast<uint32_t *>(bufferAllocation1.getHostPtr());
        uint32_t *bufferPtr2                = static_cast<uint32_t *>(bufferAllocation2.getHostPtr());

        for (uint32_t i = 0; i < numValues; ++i)
        {
            uint32_t val  = rnd.getUint32();
            inputData[i]  = val;
            *bufferPtr1++ = val;
            *bufferPtr2++ = val;
        }

        flushAlloc(vk, *logicalDevice, bufferAllocation1);
        flushAlloc(vk, *logicalDevice, bufferAllocation2);
    }

    // Create descriptor sets

    const Unique<VkDescriptorSetLayout> descriptorSetLayout1(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, *logicalDevice));

    const Unique<VkDescriptorPool> descriptorPool1(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vk, *logicalDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet1(
        makeDescriptorSet(vk, *logicalDevice, *descriptorPool1, *descriptorSetLayout1));

    const VkDescriptorBufferInfo bufferDescriptorInfo1 = makeDescriptorBufferInfo(*buffer1, 0ull, bufferSizeBytes);
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet1, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo1)
        .update(vk, *logicalDevice);

    const Unique<VkDescriptorSetLayout> descriptorSetLayout2(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, *logicalDevice));

    const Unique<VkDescriptorPool> descriptorPool2(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vk, *logicalDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet2(
        makeDescriptorSet(vk, *logicalDevice, *descriptorPool2, *descriptorSetLayout2));

    const VkDescriptorBufferInfo bufferDescriptorInfo2 = makeDescriptorBufferInfo(*buffer2, 0ull, bufferSizeBytes);
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet2, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo2)
        .update(vk, *logicalDevice);

    // Perform the computation

    const Unique<VkShaderModule> shaderModule(
        createShaderModule(vk, *logicalDevice, m_context.getBinaryCollection().get("comp"), 0u));

    ComputePipelineWrapper pipeline1(vk, *logicalDevice, m_computePipelineConstructionType,
                                     m_context.getBinaryCollection().get("comp"));
    pipeline1.setDescriptorSetLayout(*descriptorSetLayout1);
    pipeline1.buildPipeline();
    const VkBufferMemoryBarrier hostWriteBarrier1 =
        makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *buffer1, 0ull, bufferSizeBytes);
    const VkBufferMemoryBarrier shaderWriteBarrier1 =
        makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer1, 0ull, bufferSizeBytes);
    const Unique<VkCommandPool> cmdPool1(makeCommandPool(vk, *logicalDevice, queues[0].queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer1(
        allocateCommandBuffer(vk, *logicalDevice, *cmdPool1, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    ComputePipelineWrapper pipeline2(vk, *logicalDevice, m_computePipelineConstructionType,
                                     m_context.getBinaryCollection().get("comp"));
    pipeline2.setDescriptorSetLayout(*descriptorSetLayout2);
    pipeline2.buildPipeline();
    const VkBufferMemoryBarrier hostWriteBarrier2 =
        makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *buffer2, 0ull, bufferSizeBytes);
    const VkBufferMemoryBarrier shaderWriteBarrier2 =
        makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer2, 0ull, bufferSizeBytes);
    const Unique<VkCommandPool> cmdPool2(makeCommandPool(vk, *logicalDevice, queues[1].queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer2(
        allocateCommandBuffer(vk, *logicalDevice, *cmdPool2, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Command buffer 1

    beginCommandBuffer(vk, *cmdBuffer1);
    pipeline1.bind(*cmdBuffer1);
    vk.cmdBindDescriptorSets(*cmdBuffer1, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline1.getPipelineLayout(), 0u, 1u,
                             &descriptorSet1.get(), 0u, DE_NULL);
    vk.cmdPipelineBarrier(*cmdBuffer1, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &hostWriteBarrier1, 0,
                          (const VkImageMemoryBarrier *)DE_NULL);
    vk.cmdDispatch(*cmdBuffer1, 1, 1, 1);
    vk.cmdPipelineBarrier(*cmdBuffer1, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &shaderWriteBarrier1, 0,
                          (const VkImageMemoryBarrier *)DE_NULL);
    endCommandBuffer(vk, *cmdBuffer1);

    // Command buffer 2

    beginCommandBuffer(vk, *cmdBuffer2);
    pipeline2.bind(*cmdBuffer2);
    vk.cmdBindDescriptorSets(*cmdBuffer2, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline2.getPipelineLayout(), 0u, 1u,
                             &descriptorSet2.get(), 0u, DE_NULL);
    vk.cmdPipelineBarrier(*cmdBuffer2, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &hostWriteBarrier2, 0,
                          (const VkImageMemoryBarrier *)DE_NULL);
    vk.cmdDispatch(*cmdBuffer2, 1, 1, 1);
    vk.cmdPipelineBarrier(*cmdBuffer2, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &shaderWriteBarrier2, 0,
                          (const VkImageMemoryBarrier *)DE_NULL);
    endCommandBuffer(vk, *cmdBuffer2);

    VkSubmitInfo submitInfo1 = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO,         // sType
        DE_NULL,                               // pNext
        0u,                                    // waitSemaphoreCount
        DE_NULL,                               // pWaitSemaphores
        (const VkPipelineStageFlags *)DE_NULL, // pWaitDstStageMask
        1u,                                    // commandBufferCount
        &cmdBuffer1.get(),                     // pCommandBuffers
        0u,                                    // signalSemaphoreCount
        DE_NULL                                // pSignalSemaphores
    };

    VkSubmitInfo submitInfo2 = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO,         // sType
        DE_NULL,                               // pNext
        0u,                                    // waitSemaphoreCount
        DE_NULL,                               // pWaitSemaphores
        (const VkPipelineStageFlags *)DE_NULL, // pWaitDstStageMask
        1u,                                    // commandBufferCount
        &cmdBuffer2.get(),                     // pCommandBuffers
        0u,                                    // signalSemaphoreCount
        DE_NULL                                // pSignalSemaphores
    };

    // Wait for completion
    const Unique<VkFence> fence1(createFence(vk, *logicalDevice));
    const Unique<VkFence> fence2(createFence(vk, *logicalDevice));

    VK_CHECK(vk.queueSubmit(queues[0].queue, 1u, &submitInfo1, *fence1));
    VK_CHECK(vk.queueSubmit(queues[1].queue, 1u, &submitInfo2, *fence2));

    int err = ERROR_NONE;

    // First wait for the low-priority queue
    if (VK_SUCCESS != vk.waitForFences(*logicalDevice, 1u, &fence2.get(), true, ~0ull))
        err = ERROR_WAIT;

    // If the high-priority queue hasn't finished, we have a problem.
    if (VK_SUCCESS != vk.getFenceStatus(*logicalDevice, fence1.get()))
        if (err == ERROR_NONE)
            err = ERROR_ORDER;

    // Wait for the high-priority fence so we don't get errors on teardown.
    vk.waitForFences(*logicalDevice, 1u, &fence1.get(), true, ~0ull);

    // If we fail() before waiting for all of the fences, error will come from
    // teardown instead of the error we want.

    if (err == ERROR_WAIT)
    {
        return tcu::TestStatus::fail("Failed waiting for low-priority queue fence.");
    }

    // Validate the results

    const Allocation &bufferAllocation1 = buffer1.getAllocation();
    invalidateAlloc(vk, *logicalDevice, bufferAllocation1);
    const uint32_t *bufferPtr1 = static_cast<uint32_t *>(bufferAllocation1.getHostPtr());

    const Allocation &bufferAllocation2 = buffer2.getAllocation();
    invalidateAlloc(vk, *logicalDevice, bufferAllocation2);
    const uint32_t *bufferPtr2 = static_cast<uint32_t *>(bufferAllocation2.getHostPtr());

    for (uint32_t ndx = 0; ndx < numValues; ++ndx)
    {
        const uint32_t res1 = bufferPtr1[ndx];
        const uint32_t res2 = bufferPtr2[ndx];
        const uint32_t inp  = inputData[ndx];
        const uint32_t ref  = ~inp;

        if (res1 != ref || res1 != res2)
        {
            std::ostringstream msg;
            msg << "Comparison failed for InOut.values[" << ndx << "] ref:" << ref << " res1:" << res1
                << " res2:" << res2 << " inp:" << inp;
            return tcu::TestStatus::fail(msg.str());
        }
    }

    if (err == ERROR_ORDER)
        log << tcu::TestLog::Message
            << "Note: Low-priority queue was faster than high-priority one. This is not an error, but priorities may "
               "be inverted."
            << tcu::TestLog::EndMessage;

    return tcu::TestStatus::pass("Test passed");
}

class EmptyWorkGroupCase : public vkt::TestCase
{
public:
    EmptyWorkGroupCase(tcu::TestContext &testCtx, const std::string &name, const tcu::UVec3 &dispatchSize,
                       const vk::ComputePipelineConstructionType computePipelineConstructionType);
    virtual ~EmptyWorkGroupCase(void)
    {
    }

    virtual void checkSupport(Context &context) const override;
    TestInstance *createInstance(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;

protected:
    const tcu::UVec3 m_dispatchSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

class EmptyWorkGroupInstance : public vkt::TestInstance
{
public:
    EmptyWorkGroupInstance(Context &context, const tcu::UVec3 &dispatchSize,
                           const vk::ComputePipelineConstructionType computePipelineConstructionType)
        : vkt::TestInstance(context)
        , m_dispatchSize(dispatchSize)
        , m_computePipelineConstructionType(computePipelineConstructionType)
    {
    }
    virtual ~EmptyWorkGroupInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

protected:
    const tcu::UVec3 m_dispatchSize;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

EmptyWorkGroupCase::EmptyWorkGroupCase(tcu::TestContext &testCtx, const std::string &name,
                                       const tcu::UVec3 &dispatchSize,
                                       const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : vkt::TestCase(testCtx, name)
    , m_dispatchSize(dispatchSize)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
    DE_ASSERT(m_dispatchSize.x() == 0u || m_dispatchSize.y() == 0u || m_dispatchSize.z() == 0u);
}

void EmptyWorkGroupCase::checkSupport(Context &context) const
{
    checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                  m_computePipelineConstructionType);
}

TestInstance *EmptyWorkGroupCase::createInstance(Context &context) const
{
    return new EmptyWorkGroupInstance(context, m_dispatchSize, m_computePipelineConstructionType);
}

void EmptyWorkGroupCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream comp;
    comp << "#version 450\n"
         << "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
         << "layout (set=0, binding=0) buffer VerificationBlock { uint value; } verif;\n"
         << "void main () { atomicAdd(verif.value, 1u); }\n";
    programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

tcu::TestStatus EmptyWorkGroupInstance::iterate(void)
{
    const auto &vkd       = m_context.getDeviceInterface();
    const auto device     = m_context.getDevice();
    auto &alloc           = m_context.getDefaultAllocator();
    const auto queueIndex = m_context.getUniversalQueueFamilyIndex();
    const auto queue      = m_context.getUniversalQueue();

    const auto verifBufferSize = static_cast<VkDeviceSize>(sizeof(uint32_t));
    const auto verifBufferInfo = makeBufferCreateInfo(verifBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory verifBuffer(vkd, device, alloc, verifBufferInfo, MemoryRequirement::HostVisible);
    auto &verifBufferAlloc = verifBuffer.getAllocation();
    void *verifBufferPtr   = verifBufferAlloc.getHostPtr();

    deMemset(verifBufferPtr, 0, static_cast<size_t>(verifBufferSize));
    flushAlloc(vkd, device, verifBufferAlloc);

    DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    const auto descriptorSetLayout = layoutBuilder.build(vkd, device);

    ComputePipelineWrapper pipeline(vkd, device, m_computePipelineConstructionType,
                                    m_context.getBinaryCollection().get("comp"));
    pipeline.setDescriptorSetLayout(descriptorSetLayout.get());
    pipeline.buildPipeline();

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    const auto descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet  = makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());

    DescriptorSetUpdateBuilder updateBuilder;
    const auto verifBufferDescInfo = makeDescriptorBufferInfo(verifBuffer.get(), 0ull, verifBufferSize);
    updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                              VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &verifBufferDescInfo);
    updateBuilder.update(vkd, device);

    const auto cmdPool      = makeCommandPool(vkd, device, queueIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    beginCommandBuffer(vkd, cmdBuffer);
    pipeline.bind(cmdBuffer);
    vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getPipelineLayout(), 0u, 1u,
                              &descriptorSet.get(), 0u, nullptr);
    vkd.cmdDispatch(cmdBuffer, m_dispatchSize.x(), m_dispatchSize.y(), m_dispatchSize.z());

    const auto readWriteAccess  = (VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT);
    const auto computeToCompute = makeMemoryBarrier(readWriteAccess, readWriteAccess);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0U,
                           1u, &computeToCompute, 0u, nullptr, 0u, nullptr);

    vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);

    const auto computeToHost = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                           &computeToHost, 0u, nullptr, 0u, nullptr);

    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    uint32_t value;
    invalidateAlloc(vkd, device, verifBufferAlloc);
    deMemcpy(&value, verifBufferPtr, sizeof(value));

    if (value != 1u)
    {
        std::ostringstream msg;
        msg << "Unexpected value found in buffer: " << value << " while expecting 1";
        TCU_FAIL(msg.str());
    }

    return tcu::TestStatus::pass("Pass");
}

class MaxWorkGroupSizeTest : public vkt::TestCase
{
public:
    enum class Axis
    {
        X = 0,
        Y = 1,
        Z = 2
    };

    struct Params
    {
        // Which axis to maximize.
        Axis axis;
    };

    MaxWorkGroupSizeTest(tcu::TestContext &testCtx, const std::string &name, const Params &params,
                         const vk::ComputePipelineConstructionType computePipelineConstructionType);
    virtual ~MaxWorkGroupSizeTest(void)
    {
    }

    virtual void initPrograms(vk::SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

    // Helper to transform the axis value to an index.
    static int getIndex(Axis axis);

    // Helper returning the number of invocations according to the test parameters.
    static uint32_t getInvocations(const Params &params, const vk::InstanceInterface &vki,
                                   vk::VkPhysicalDevice physicalDevice,
                                   const vk::VkPhysicalDeviceProperties *devProperties = nullptr);

    // Helper returning the buffer size needed to this test.
    static uint32_t getSSBOSize(uint32_t invocations);

private:
    Params m_params;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

class MaxWorkGroupSizeInstance : public vkt::TestInstance
{
public:
    MaxWorkGroupSizeInstance(Context &context, const MaxWorkGroupSizeTest::Params &params,
                             const vk::ComputePipelineConstructionType computePipelineConstructionType);
    virtual ~MaxWorkGroupSizeInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

private:
    MaxWorkGroupSizeTest::Params m_params;
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

int MaxWorkGroupSizeTest::getIndex(Axis axis)
{
    const int ret = static_cast<int>(axis);
    DE_ASSERT(ret >= static_cast<int>(Axis::X) && ret <= static_cast<int>(Axis::Z));
    return ret;
}

uint32_t MaxWorkGroupSizeTest::getInvocations(const Params &params, const vk::InstanceInterface &vki,
                                              vk::VkPhysicalDevice physicalDevice,
                                              const vk::VkPhysicalDeviceProperties *devProperties)
{
    const auto axis = getIndex(params.axis);

    if (devProperties)
        return devProperties->limits.maxComputeWorkGroupSize[axis];
    return vk::getPhysicalDeviceProperties(vki, physicalDevice).limits.maxComputeWorkGroupSize[axis];
}

uint32_t MaxWorkGroupSizeTest::getSSBOSize(uint32_t invocations)
{
    return invocations * static_cast<uint32_t>(sizeof(uint32_t));
}

MaxWorkGroupSizeTest::MaxWorkGroupSizeTest(tcu::TestContext &testCtx, const std::string &name, const Params &params,
                                           const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : vkt::TestCase(testCtx, name)
    , m_params(params)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

void MaxWorkGroupSizeTest::initPrograms(vk::SourceCollections &programCollection) const
{
    std::ostringstream shader;

    // The actual local sizes will be set using spec constants when running the test instance.
    shader << "#version 450\n"
           << "\n"
           << "layout(constant_id=0) const int local_size_x_val = 1;\n"
           << "layout(constant_id=1) const int local_size_y_val = 1;\n"
           << "layout(constant_id=2) const int local_size_z_val = 1;\n"
           << "\n"
           << "layout(local_size_x_id=0, local_size_y_id=1, local_size_z_id=2) in;\n"
           << "\n"
           << "layout(set=0, binding=0) buffer StorageBuffer {\n"
           << "    uint values[];\n"
           << "} ssbo;\n"
           << "\n"
           << "void main() {\n"
           << "    ssbo.values[gl_LocalInvocationIndex] = 1u;\n"
           << "}\n";

    programCollection.glslSources.add("comp") << glu::ComputeSource(shader.str());
}

TestInstance *MaxWorkGroupSizeTest::createInstance(Context &context) const
{
    return new MaxWorkGroupSizeInstance(context, m_params, m_computePipelineConstructionType);
}

void MaxWorkGroupSizeTest::checkSupport(Context &context) const
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    const auto properties  = vk::getPhysicalDeviceProperties(vki, physicalDevice);
    const auto invocations = getInvocations(m_params, vki, physicalDevice, &properties);

    if (invocations > properties.limits.maxComputeWorkGroupInvocations)
        TCU_FAIL("Reported workgroup size limit in the axis is greater than the global invocation limit");

    if (properties.limits.maxStorageBufferRange / static_cast<uint32_t>(sizeof(uint32_t)) < invocations)
        TCU_THROW(NotSupportedError, "Maximum supported storage buffer range too small");

    checkShaderObjectRequirements(vki, physicalDevice, m_computePipelineConstructionType);
}

MaxWorkGroupSizeInstance::MaxWorkGroupSizeInstance(
    Context &context, const MaxWorkGroupSizeTest::Params &params,
    const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : vkt::TestInstance(context)
    , m_params(params)
    , m_computePipelineConstructionType(computePipelineConstructionType)
{
}

tcu::TestStatus MaxWorkGroupSizeInstance::iterate(void)
{
    const auto &vki           = m_context.getInstanceInterface();
    const auto &vkd           = m_context.getDeviceInterface();
    const auto physicalDevice = m_context.getPhysicalDevice();
    const auto device         = m_context.getDevice();
    auto &alloc               = m_context.getDefaultAllocator();
    const auto queueIndex     = m_context.getUniversalQueueFamilyIndex();
    const auto queue          = m_context.getUniversalQueue();
    auto &log                 = m_context.getTestContext().getLog();

    const auto axis        = MaxWorkGroupSizeTest::getIndex(m_params.axis);
    const auto invocations = MaxWorkGroupSizeTest::getInvocations(m_params, vki, physicalDevice);
    const auto ssboSize    = static_cast<vk::VkDeviceSize>(MaxWorkGroupSizeTest::getSSBOSize(invocations));

    log << tcu::TestLog::Message << "Running test with " << invocations << " invocations on axis " << axis
        << " using a storage buffer size of " << ssboSize << " bytes" << tcu::TestLog::EndMessage;

    // Main SSBO buffer.
    const auto ssboInfo = vk::makeBufferCreateInfo(ssboSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    vk::BufferWithMemory ssbo(vkd, device, alloc, ssboInfo, vk::MemoryRequirement::HostVisible);

    // Descriptor set layouts.
    vk::DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);
    const auto descriptorSetLayout = layoutBuilder.build(vkd, device);

    // Specialization constants: set the number of invocations in the appropriate local size id.
    const auto entrySize          = static_cast<uintptr_t>(sizeof(int32_t));
    int32_t specializationData[3] = {1, 1, 1};
    specializationData[axis]      = static_cast<int32_t>(invocations);

    const vk::VkSpecializationMapEntry specializationMaps[3] = {
        {
            0u,        // uint32_t constantID;
            0u,        // uint32_t offset;
            entrySize, // uintptr_t size;
        },
        {
            1u,                               // uint32_t constantID;
            static_cast<uint32_t>(entrySize), // uint32_t offset;
            entrySize,                        // uintptr_t size;
        },
        {
            2u,                                    // uint32_t constantID;
            static_cast<uint32_t>(entrySize * 2u), // uint32_t offset;
            entrySize,                             // uintptr_t size;
        },
    };

    const vk::VkSpecializationInfo specializationInfo = {
        3u,                                                 // uint32_t mapEntryCount;
        specializationMaps,                                 // const VkSpecializationMapEntry* pMapEntries;
        static_cast<uintptr_t>(sizeof(specializationData)), // uintptr_t dataSize;
        specializationData,                                 // const void* pData;
    };

    ComputePipelineWrapper testPipeline(vkd, device, m_computePipelineConstructionType,
                                        m_context.getBinaryCollection().get("comp"));
    testPipeline.setDescriptorSetLayout(descriptorSetLayout.get());
    testPipeline.setSpecializationInfo(specializationInfo);
    testPipeline.buildPipeline();

    // Create descriptor pool and set.
    vk::DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    const auto descriptorPool =
        poolBuilder.build(vkd, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet = vk::makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());

    // Update descriptor set.
    const vk::VkDescriptorBufferInfo ssboBufferInfo = {
        ssbo.get(),    // VkBuffer buffer;
        0u,            // VkDeviceSize offset;
        VK_WHOLE_SIZE, // VkDeviceSize range;
    };

    vk::DescriptorSetUpdateBuilder updateBuilder;
    updateBuilder.writeSingle(descriptorSet.get(), vk::DescriptorSetUpdateBuilder::Location::binding(0u),
                              vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &ssboBufferInfo);
    updateBuilder.update(vkd, device);

    // Clear buffer.
    auto &ssboAlloc = ssbo.getAllocation();
    void *ssboPtr   = ssboAlloc.getHostPtr();
    deMemset(ssboPtr, 0, static_cast<size_t>(ssboSize));
    vk::flushAlloc(vkd, device, ssboAlloc);

    // Run pipelines.
    const auto cmdPool = vk::makeCommandPool(vkd, device, queueIndex);
    const auto cmdBUfferPtr =
        vk::allocateCommandBuffer(vkd, device, cmdPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer = cmdBUfferPtr.get();

    vk::beginCommandBuffer(vkd, cmdBuffer);

    // Run the main test shader.
    const auto hostToComputeBarrier = vk::makeBufferMemoryBarrier(
        vk::VK_ACCESS_HOST_WRITE_BIT, vk::VK_ACCESS_SHADER_WRITE_BIT, ssbo.get(), 0ull, VK_WHOLE_SIZE);
    vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u,
                           nullptr, 1u, &hostToComputeBarrier, 0u, nullptr);

    testPipeline.bind(cmdBuffer);
    vkd.cmdBindDescriptorSets(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, testPipeline.getPipelineLayout(), 0u, 1u,
                              &descriptorSet.get(), 0u, nullptr);
    vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);

    const auto computeToHostBarrier = vk::makeBufferMemoryBarrier(
        vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_HOST_READ_BIT, ssbo.get(), 0ull, VK_WHOLE_SIZE);
    vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u,
                           nullptr, 1u, &computeToHostBarrier, 0u, nullptr);

    vk::endCommandBuffer(vkd, cmdBuffer);
    vk::submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    // Verify buffer contents.
    vk::invalidateAlloc(vkd, device, ssboAlloc);
    std::unique_ptr<uint32_t[]> valuesArray(new uint32_t[invocations]);
    uint32_t *valuesPtr = valuesArray.get();
    deMemcpy(valuesPtr, ssboPtr, static_cast<size_t>(ssboSize));

    std::string errorMsg;
    bool ok = true;

    for (size_t i = 0; i < invocations; ++i)
    {
        if (valuesPtr[i] != 1u)
        {
            ok       = false;
            errorMsg = "Found invalid value for invocation index " + de::toString(i) + ": expected 1u and found " +
                       de::toString(valuesPtr[i]);
            break;
        }
    }

    if (!ok)
        return tcu::TestStatus::fail(errorMsg);
    return tcu::TestStatus::pass("Pass");
}

namespace EmptyShaderTest
{

void checkSupport(Context &context, vk::ComputePipelineConstructionType computePipelineConstructionType)
{
    checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                  computePipelineConstructionType);
}

void createProgram(SourceCollections &dst, vk::ComputePipelineConstructionType)
{
    dst.glslSources.add("comp") << glu::ComputeSource("#version 310 es\n"
                                                      "layout (local_size_x = 1) in;\n"
                                                      "void main (void) {}\n");
}

tcu::TestStatus createTest(Context &context, vk::ComputePipelineConstructionType computePipelineConstructionType)
{
    const DeviceInterface &vk       = context.getDeviceInterface();
    const VkDevice device           = context.getDevice();
    const VkQueue queue             = context.getUniversalQueue();
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();

    ComputePipelineWrapper pipeline(vk, device, computePipelineConstructionType,
                                    context.getBinaryCollection().get("comp"));
    pipeline.buildPipeline();

    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Start recording commands

    beginCommandBuffer(vk, *cmdBuffer);

    pipeline.bind(*cmdBuffer);

    const tcu::IVec3 workGroups(1, 1, 1);
    vk.cmdDispatch(*cmdBuffer, workGroups.x(), workGroups.y(), workGroups.z());

    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    return tcu::TestStatus::pass("Compute succeeded");
}

} // namespace EmptyShaderTest

namespace ComputeOnlyQueueTests
{

tcu::Maybe<uint32_t> getComputeOnlyQueueFamily(Context &context)
{
    bool foundQueue = false;
    uint32_t index  = 0;

    auto queueFamilies =
        getPhysicalDeviceQueueFamilyProperties(context.getInstanceInterface(), context.getPhysicalDevice());

    for (const auto &queueFamily : queueFamilies)
    {
        if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) && !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            foundQueue = true;
            break;
        }
        else
        {
            index++;
        }
    }
    if (!foundQueue)
    {
        return tcu::Maybe<uint32_t>();
    }
    else
    {
        return index;
    }
}

// Creates a device that has a queue for compute capabilities without graphics.
Move<VkDevice> createComputeOnlyDevice(vk::VkInstance instance, const InstanceInterface &instanceDriver,
                                       const VkPhysicalDevice physicalDevice, Context &context,
                                       uint32_t &queueFamilyIndex)
{
    const auto queueFamilies = getPhysicalDeviceQueueFamilyProperties(instanceDriver, physicalDevice);

    // One queue family without a graphics bit should be found, since this is checked in checkSupport.
    queueFamilyIndex = getComputeOnlyQueueFamily(context).get();

    const float queuePriority                            = 1.0f;
    const VkDeviceQueueCreateInfo deviceQueueCreateInfos = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                    // const void* pNext;
        (VkDeviceQueueCreateFlags)0u,               // VkDeviceQueueCreateFlags flags;
        queueFamilyIndex,                           // uint32_t queueFamilyIndex;
        1u,                                         // uint32_t queueCount;
        &queuePriority,                             // const float* pQueuePriorities;
    };

    void *pNext = nullptr;
#ifdef CTS_USES_VULKANSC
    VkDeviceObjectReservationCreateInfo memReservationInfo = context.getTestContext().getCommandLine().isSubProcess() ?
                                                                 context.getResourceInterface()->getStatMax() :
                                                                 resetDeviceObjectReservationCreateInfo();
    pNext                                                  = &memReservationInfo;

    VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
    sc10Features.pNext                              = pNext;
    pNext                                           = &sc10Features;

    VkPipelineCacheCreateInfo pcCI;
    std::vector<VkPipelinePoolSize> poolSizes;
    if (context.getTestContext().getCommandLine().isSubProcess())
    {
        if (context.getResourceInterface()->getCacheDataSize() > 0)
        {
            pcCI = {
                VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                      // const void* pNext;
                VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
                    VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT, // VkPipelineCacheCreateFlags flags;
                context.getResourceInterface()->getCacheDataSize(),       // uintptr_t initialDataSize;
                context.getResourceInterface()->getCacheData()            // const void* pInitialData;
            };
            memReservationInfo.pipelineCacheCreateInfoCount = 1;
            memReservationInfo.pPipelineCacheCreateInfos    = &pcCI;
        }
        poolSizes = context.getResourceInterface()->getPipelinePoolSizes();
        if (!poolSizes.empty())
        {
            memReservationInfo.pipelinePoolSizeCount = uint32_t(poolSizes.size());
            memReservationInfo.pPipelinePoolSizes    = poolSizes.data();
        }
    }
#endif // CTS_USES_VULKANSC
    const VkDeviceCreateInfo deviceCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, // VkStructureType sType;
        pNext,                                // const void* pNext;
        (VkDeviceCreateFlags)0u,              // VkDeviceCreateFlags flags;
        1,                                    // uint32_t queueCreateInfoCount;
        &deviceQueueCreateInfos,              // const VkDeviceQueueCreateInfo* pQueueCreateInfos;
        0u,                                   // uint32_t enabledLayerCount;
        nullptr,                              // const char* const* ppEnabledLayerNames;
        0,                                    // uint32_t enabledExtensionCount;
        nullptr,                              // const char* const* ppEnabledExtensionNames;
        nullptr,                              // const VkPhysicalDeviceFeatures* pEnabledFeatures;
    };

    return vkt::createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(),
                                   context.getPlatformInterface(), instance, instanceDriver, physicalDevice,
                                   &deviceCreateInfo);
}

class SecondaryCommandBufferComputeOnlyTest : public vkt::TestCase
{
public:
    SecondaryCommandBufferComputeOnlyTest(tcu::TestContext &context, const std::string &name)
        : vkt::TestCase(context, name){};

    void initPrograms(SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &context) const override;
    void checkSupport(Context &context) const override;
};

class SecondaryCommandBufferComputeOnlyTestInstance : public vkt::TestInstance
{
public:
    SecondaryCommandBufferComputeOnlyTestInstance(Context &context)
        : vkt::TestInstance(context)
#ifdef CTS_USES_VULKANSC
        , m_customInstance(createCustomInstanceFromContext(context))
#endif // CTS_USES_VULKANSC
              {};
    virtual tcu::TestStatus iterate(void);

protected:
#ifdef CTS_USES_VULKANSC
    const CustomInstance m_customInstance;
#endif // CTS_USES_VULKANSC
};

void SecondaryCommandBufferComputeOnlyTest::initPrograms(SourceCollections &collection) const
{
    {
        std::ostringstream src;
        src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
            << "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
            << "layout(set = 0, binding = 0, std430) buffer Out\n"
            << "{\n"
            << "    uint data[];\n"
            << "};\n"
            << "void main (void)\n"
            << "{\n"
            << "data[0] = 1;"
            << "}\n";
        collection.glslSources.add("comp") << glu::ComputeSource(src.str());
    }
}

TestInstance *SecondaryCommandBufferComputeOnlyTest::createInstance(Context &context) const
{
    return new SecondaryCommandBufferComputeOnlyTestInstance(context);
}

void SecondaryCommandBufferComputeOnlyTest::checkSupport(Context &context) const
{
    // Find at least one queue family that supports compute queue but does NOT support graphics queue.
    if (!getComputeOnlyQueueFamily(context))
        TCU_THROW(NotSupportedError, "No queue family found that only supports compute queue.");
}

tcu::TestStatus SecondaryCommandBufferComputeOnlyTestInstance::iterate()
{
    VkDevice device;
    uint32_t queueFamilyIndex;
#ifdef CTS_USES_VULKANSC
    const vk::InstanceInterface &vki = m_customInstance.getDriver();
    const VkPhysicalDevice physDevice =
        chooseDevice(vki, m_customInstance, m_context.getTestContext().getCommandLine());
    auto customDevice = createComputeOnlyDevice(m_customInstance, vki, physDevice, m_context, queueFamilyIndex);
    de::MovePtr<DeviceDriverSC, DeinitDeviceDeleter> deviceDriver;
#else
    const InstanceInterface &vki      = m_context.getInstanceInterface();
    const VkPhysicalDevice physDevice = m_context.getPhysicalDevice();
    auto customDevice = createComputeOnlyDevice(m_context.getInstance(), vki, physDevice, m_context, queueFamilyIndex);
    de::MovePtr<DeviceDriver> deviceDriver;
#endif // CTS_USES_VULKANSC

    device = customDevice.get();

#ifndef CTS_USES_VULKANSC
    deviceDriver = de::MovePtr<DeviceDriver>(new DeviceDriver(m_context.getPlatformInterface(), m_context.getInstance(),
                                                              device, m_context.getUsedApiVersion(),
                                                              m_context.getTestContext().getCommandLine()));
#else
    deviceDriver = de::MovePtr<DeviceDriverSC, DeinitDeviceDeleter>(
        new DeviceDriverSC(m_context.getPlatformInterface(), m_customInstance, device,
                           m_context.getTestContext().getCommandLine(), m_context.getResourceInterface(),
                           m_context.getDeviceVulkanSC10Properties(), m_context.getDeviceProperties(),
                           m_context.getUsedApiVersion()),
        DeinitDeviceDeleter(m_context.getResourceInterface().get(), device));
#endif // CTS_USES_VULKANSC

    const DeviceInterface &vkdi = *deviceDriver;

    auto queue = getDeviceQueue(vkdi, device, queueFamilyIndex, 0u);
    auto allocator =
        de::MovePtr<Allocator>(new SimpleAllocator(vkdi, device, getPhysicalDeviceMemoryProperties(vki, physDevice)));

    const auto bufferSize = static_cast<VkDeviceSize>(sizeof(uint32_t));
    BufferWithMemory buffer(vkdi, device, *allocator.get(),
                            makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                            MemoryRequirement::HostVisible);
    auto &bufferAlloc = buffer.getAllocation();
    void *bufferData  = bufferAlloc.getHostPtr();
    deMemset(bufferData, 0, sizeof(uint32_t));
    flushAlloc(vkdi, device, bufferAlloc);

    DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
    Unique<VkDescriptorSetLayout> descriptorSetLayout(layoutBuilder.build(vkdi, device));

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    const auto descriptorPool = poolBuilder.build(vkdi, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);
    const auto descriptorSetBuffer = makeDescriptorSet(vkdi, device, descriptorPool.get(), descriptorSetLayout.get());

    // Update descriptor sets.
    DescriptorSetUpdateBuilder updater;

    const auto bufferInfo = makeDescriptorBufferInfo(buffer.get(), 0ull, bufferSize);
    updater.writeSingle(descriptorSetBuffer.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo);

    updater.update(vkdi, device);

    auto shader = createShaderModule(vkdi, device, m_context.getBinaryCollection().get("comp"));
    // Create compute pipeline
    const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vkdi, device, *descriptorSetLayout));
    const Unique<VkPipeline> computePipeline(makeComputePipeline(vkdi, device, *pipelineLayout, *shader));

    // Create command buffer
    const Unique<VkCommandPool> cmdPool(makeCommandPool(vkdi, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vkdi, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const Unique<VkCommandBuffer> cmdBuffer2(
        allocateCommandBuffer(vkdi, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY));

    const VkCommandBufferInheritanceInfo bufferInheritanceInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, // VkStructureType sType;
        nullptr,                                           // const void* pNext;
        VK_NULL_HANDLE,                                    // VkRenderPass renderPass;
        0u,                                                // uint32_t subpass;
        VK_NULL_HANDLE,                                    // VkFramebuffer framebuffer;
        VK_FALSE,                                          // VkBool32 occlusionQueryEnable;
        (VkQueryControlFlags)0u,                           // VkQueryControlFlags queryFlags;
        (VkQueryPipelineStatisticFlags)0u                  // VkQueryPipelineStatisticFlags pipelineStatistics;
    };

    VkCommandBufferUsageFlags usageFlags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    const VkCommandBufferBeginInfo commandBufBeginParams{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        usageFlags,                                  // VkCommandBufferUsageFlags flags;
        &bufferInheritanceInfo};

    beginCommandBuffer(vkdi, cmdBuffer.get());
    vkdi.beginCommandBuffer(cmdBuffer2.get(), &commandBufBeginParams);
    vkdi.cmdBindPipeline(cmdBuffer2.get(), VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.get());
    vkdi.cmdBindDescriptorSets(cmdBuffer2.get(), VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.get(), 0u, 1,
                               &descriptorSetBuffer.get(), 0u, nullptr);
    vkdi.cmdDispatch(cmdBuffer2.get(), 1, 1, 1);
    endCommandBuffer(vkdi, cmdBuffer2.get());
    vkdi.cmdExecuteCommands(cmdBuffer.get(), 1, &cmdBuffer2.get());
    const VkBufferMemoryBarrier renderBufferBarrier =
        makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, buffer.get(), 0ull, bufferSize);
    cmdPipelineBufferMemoryBarrier(vkdi, cmdBuffer.get(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                   VK_PIPELINE_STAGE_HOST_BIT, &renderBufferBarrier);
    endCommandBuffer(vkdi, cmdBuffer.get());
    submitCommandsAndWait(vkdi, device, queue, cmdBuffer.get());

    invalidateAlloc(vkdi, device, bufferAlloc);

    uint32_t result = 0;
    deMemcpy(&result, bufferData, sizeof(uint32_t));
    if (result != 1)
    {
        return tcu::TestStatus::pass("value of buffer unexpected");
    }

    return tcu::TestStatus::pass("passed");
}

}; // namespace ComputeOnlyQueueTests

enum CompositeType
{
    VECTOR,
    MATRIX,
    ARRAY,
    ARRAY_ARRAY,
    STRUCT,
    STRUCT_STRUCT,
    COOPMAT,
};
enum InstType
{
    VALUE,
    CONSTANT,
    SPECCONSTANT,
};

class ReplicatedCompositesTest : public vkt::TestCase
{
public:
    ReplicatedCompositesTest(tcu::TestContext &testCtx, const std::string &name, const CompositeType compositeType,
                             const InstType instType,
                             const vk::ComputePipelineConstructionType computePipelineConstructionType);

    virtual void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &sourceCollections) const;
    TestInstance *createInstance(Context &context) const;

private:
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
    CompositeType m_compositeType;
    InstType m_instType;
};

class ReplicatedCompositesTestInstance : public vkt::TestInstance
{
public:
    ReplicatedCompositesTestInstance(Context &context, const CompositeType compositeType, const InstType instType,
                                     const vk::ComputePipelineConstructionType computePipelineConstructionType);

    tcu::TestStatus iterate(void);

private:
    vk::ComputePipelineConstructionType m_computePipelineConstructionType;
    CompositeType m_compositeType;
    InstType m_instType;
};

ReplicatedCompositesTest::ReplicatedCompositesTest(
    tcu::TestContext &testCtx, const std::string &name, const CompositeType compositeType, const InstType instType,
    const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestCase(testCtx, name)
    , m_computePipelineConstructionType(computePipelineConstructionType)
    , m_compositeType(compositeType)
    , m_instType(instType)
{
}

void ReplicatedCompositesTest::checkSupport(Context &context) const
{
    checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                  m_computePipelineConstructionType);

#ifndef CTS_USES_VULKANSC
    if (!context.getShaderReplicatedCompositesFeaturesEXT().shaderReplicatedComposites)
    {
        TCU_THROW(NotSupportedError, "shaderReplicatedComposites not supported");
    }

    if (m_compositeType == COOPMAT)
    {
        const InstanceInterface &vki = context.getInstanceInterface();
        if (!context.getCooperativeMatrixFeatures().cooperativeMatrix)
        {
            TCU_THROW(NotSupportedError,
                      "VkPhysicalDeviceCooperativeMatrixFeaturesKHR::cooperativeMatrix not supported");
        }

        uint32_t propertyCount = 0;

        VK_CHECK(
            vki.getPhysicalDeviceCooperativeMatrixPropertiesKHR(context.getPhysicalDevice(), &propertyCount, DE_NULL));

        const VkCooperativeMatrixPropertiesKHR initStruct = initVulkanStructureConst();

        std::vector<VkCooperativeMatrixPropertiesKHR> properties(propertyCount, initStruct);

        VK_CHECK(vki.getPhysicalDeviceCooperativeMatrixPropertiesKHR(context.getPhysicalDevice(), &propertyCount,
                                                                     properties.data()));

        bool foundFp16 = false;
        for (size_t i = 0; i < properties.size(); ++i)
        {
            const VkCooperativeMatrixPropertiesKHR *p = &properties[i];

            if (p->scope != VK_SCOPE_SUBGROUP_KHR)
                continue;

            if (p->AType == VK_COMPONENT_TYPE_FLOAT16_KHR)
                foundFp16 = true;
        }
        if (!foundFp16)
        {
            TCU_THROW(NotSupportedError, "cooperativeMatrix float16 not supported");
        }
    }
#endif // CTS_USES_VULKANSC
}

void ReplicatedCompositesTest::initPrograms(SourceCollections &sourceCollections) const
{
    std::ostringstream src;
    src << "#version 460 core\n"
        << "#extension GL_EXT_scalar_block_layout : enable\n"
        << "#extension GL_KHR_cooperative_matrix : enable\n"
        << "#extension GL_EXT_shader_explicit_arithmetic_types_float16 : enable\n"
        << "#extension GL_KHR_memory_scope_semantics : enable\n"
        << "#extension GL_EXT_spec_constant_composites : enable\n"
        << "#pragma use_replicated_composites\n"
        << "layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
        << "layout(binding = 0, scalar) buffer Output {\n";

    switch (m_compositeType)
    {
    case VECTOR:
        src << "float vec[4];\n";
        break;
    case MATRIX:
        src << "float mat[4*4];\n";
        break;
    case ARRAY:
        src << "uint arr[3];\n";
        break;
    case ARRAY_ARRAY:
        src << "uint arrarr[6];\n";
        break;
    case STRUCT:
        src << "uint str[3];\n";
        break;
    case STRUCT_STRUCT:
        src << "uint str[6];\n";
        break;
    case COOPMAT:
        src << "float mat[2];\n";
        break;
    default:
        DE_ASSERT(0);
        break;
    }
    src << "} sb_out;\n\n";

    if (m_compositeType == COOPMAT)
    {
        src << "layout(constant_id = 1) const uint rows = 1;\n"
            << "layout(constant_id = 2) const uint cols = 1;\n";
    }

    if (m_instType != VALUE)
    {
        if (m_instType == SPECCONSTANT)
        {
            src << "layout(constant_id = 0) ";
        }
        switch (m_compositeType)
        {
        case VECTOR:
            src << "const float one = 1.0;\n"
                << "const vec4 vec = vec4(one);\n";
            break;
        case MATRIX:
            src << "const float one = 1.0;\n"
                << "const vec4 vec = vec4(one);\n"
                << "const mat4 mat = mat4(vec, vec, vec, vec);\n";
            break;
        case ARRAY:
            src << "const uint three = 3;\n"
                << "const uint arr[3] = {three, three, three};\n";
            break;
        case ARRAY_ARRAY:
            src << "const uint three = 3;\n"
                << "const uint arr[3] = {three, three, three};\n"
                << "const uint arrarr[2][3] = {arr, arr};\n";
            break;
        case STRUCT:
            src << "const uint six = 6;\n"
                << "struct S { uint a; uint b; uint c; };\n"
                << "const S str = S(six, six, six);\n\n";
            break;
        case STRUCT_STRUCT:
            src << "const uint six = 6;\n"
                << "struct S { uint a; uint b; uint c; };\n"
                << "struct SS { S a; S b; };\n"
                << "const S str = S(six, six, six);\n"
                << "const SS str2 = SS(str, str);\n\n";
            break;
        case COOPMAT:
            src << "const float one = 1.0;\n"
                << "const coopmat<float16_t, gl_ScopeSubgroup, rows, cols, gl_MatrixUseA> mat = coopmat<float16_t, "
                   "gl_ScopeSubgroup, rows, cols, gl_MatrixUseA>(one);\n";
            break;
        default:
            DE_ASSERT(0);
            break;
        }
    }
    src << "void main (void) {\n";

    if (m_instType == VALUE)
    {
        switch (m_compositeType)
        {
        case VECTOR:
            src << "    float one = 1.0;\n"
                << "    vec4 vec = vec4(one);\n";
            break;
        case MATRIX:
            src << "    float one = 1.0;\n"
                << "    vec4 vec = vec4(one);\n"
                << "    mat4 mat = mat4(vec, vec, vec, vec);\n";
            break;
        case ARRAY:
            src << "    uint three = 3;\n"
                << "    uint arr[3] = {three, three, three};\n";
            break;
        case ARRAY_ARRAY:
            src << "    uint three = 3;\n"
                << "    uint arr[3] = {three, three, three};\n"
                << "    uint arrarr[2][3] = {arr, arr};\n";
            break;
        case STRUCT:
            src << "    uint six = 6;\n"
                << "    struct S { uint a; uint b; uint c; };\n"
                << "    S str = S(six, six, six);\n\n";
            break;
        case STRUCT_STRUCT:
            src << "    uint six = 6;\n"
                << "    struct S { uint a; uint b; uint c; };\n"
                << "    struct SS { S a; S b; };\n"
                << "    S str = S(six, six, six);\n"
                << "    SS str2 = SS(str, str);\n\n";
            break;
        case COOPMAT:
            src << "    float one = 1.0;\n"
                << "    coopmat<float16_t, gl_ScopeSubgroup, rows, cols, gl_MatrixUseA> mat = coopmat<float16_t, "
                   "gl_ScopeSubgroup, rows, cols, gl_MatrixUseA>(one);\n";
            break;
        default:
            DE_ASSERT(0);
            break;
        }
    }
    switch (m_compositeType)
    {
    case VECTOR:
        src << "    sb_out.vec[0] = vec[0];\n"
            << "    sb_out.vec[1] = vec[1];\n"
            << "    sb_out.vec[2] = vec[2];\n"
            << "    sb_out.vec[3] = vec[3];\n";
        break;
    case MATRIX:
        src << "    sb_out.mat[0] = mat[0][0];\n"
            << "    sb_out.mat[1] = mat[0][1];\n"
            << "    sb_out.mat[2] = mat[0][2];\n"
            << "    sb_out.mat[3] = mat[0][3];\n"
            << "    sb_out.mat[4] = mat[1][0];\n"
            << "    sb_out.mat[5] = mat[1][1];\n"
            << "    sb_out.mat[6] = mat[1][2];\n"
            << "    sb_out.mat[7] = mat[1][3];\n"
            << "    sb_out.mat[8] = mat[2][0];\n"
            << "    sb_out.mat[9] = mat[2][1];\n"
            << "    sb_out.mat[10] = mat[2][2];\n"
            << "    sb_out.mat[11] = mat[2][3];\n"
            << "    sb_out.mat[12] = mat[3][0];\n"
            << "    sb_out.mat[13] = mat[3][1];\n"
            << "    sb_out.mat[14] = mat[3][2];\n"
            << "    sb_out.mat[15] = mat[3][3];\n";
        break;
    case ARRAY:
        src << "    sb_out.arr[0] = arr[0];\n"
            << "    sb_out.arr[1] = arr[1];\n"
            << "    sb_out.arr[2] = arr[2];\n";
        break;
    case ARRAY_ARRAY:
        src << "    sb_out.arrarr[0] = arrarr[0][0];\n"
            << "    sb_out.arrarr[1] = arrarr[0][1];\n"
            << "    sb_out.arrarr[2] = arrarr[0][2];\n"
            << "    sb_out.arrarr[3] = arrarr[1][0];\n"
            << "    sb_out.arrarr[4] = arrarr[1][1];\n"
            << "    sb_out.arrarr[5] = arrarr[1][2];\n";
        break;
    case STRUCT:
        src << "    sb_out.str[0] = str.a;\n"
            << "    sb_out.str[1] = str.b;\n"
            << "    sb_out.str[2] = str.c;\n";
        break;
    case STRUCT_STRUCT:
        src << "    sb_out.str[0] = str2.a.a;\n"
            << "    sb_out.str[1] = str2.a.b;\n"
            << "    sb_out.str[2] = str2.a.c;\n"
            << "    sb_out.str[3] = str2.b.a;\n"
            << "    sb_out.str[4] = str2.b.b;\n"
            << "    sb_out.str[5] = str2.b.c;\n";
        break;
    case COOPMAT:
        src << "    sb_out.mat[0] = float(mat[0]);\n"
            << "    sb_out.mat[1] = (mat.length() > 1) ? float(mat[1]) : float(mat[0]);\n";
        break;
    default:
        DE_ASSERT(0);
        break;
    }
    src << "}\n";

    sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance *ReplicatedCompositesTest::createInstance(Context &context) const
{
    return new ReplicatedCompositesTestInstance(context, m_compositeType, m_instType,
                                                m_computePipelineConstructionType);
}

ReplicatedCompositesTestInstance::ReplicatedCompositesTestInstance(
    Context &context, const CompositeType compositeType, const InstType instType,
    const vk::ComputePipelineConstructionType computePipelineConstructionType)
    : TestInstance(context)
    , m_computePipelineConstructionType(computePipelineConstructionType)
    , m_compositeType(compositeType)
    , m_instType(instType)
{
}

tcu::TestStatus ReplicatedCompositesTestInstance::iterate(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // Create a buffer and host-visible memory for it

    const VkDeviceSize bufferSizeBytes = 256;
    const BufferWithMemory buffer(vk, device, allocator,
                                  makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                  MemoryRequirement::HostVisible);

    const Allocation &bufferAllocation = buffer.getAllocation();
    deMemset(bufferAllocation.getHostPtr(), 0, bufferSizeBytes);

    flushAlloc(vk, device, bufferAllocation);
    // Create descriptor set

    const Unique<VkDescriptorSetLayout> descriptorSetLayout(
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device));

    const Unique<VkDescriptorPool> descriptorPool(
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    const VkDescriptorBufferInfo descriptorInfo = makeDescriptorBufferInfo(*buffer, 0ull, bufferSizeBytes);
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
        .update(vk, device);

    // Perform the computation
    ComputePipelineWrapper pipeline(vk, device, m_computePipelineConstructionType,
                                    m_context.getBinaryCollection().get("comp"));
    pipeline.setDescriptorSetLayout(descriptorSetLayout.get());

    uint32_t coopmatRows = 0, coopmatCols = 0;
#ifndef CTS_USES_VULKANSC
    if (m_compositeType == COOPMAT)
    {
        const InstanceInterface &vki = m_context.getInstanceInterface();
        uint32_t propertyCount       = 0;

        VK_CHECK(vki.getPhysicalDeviceCooperativeMatrixPropertiesKHR(m_context.getPhysicalDevice(), &propertyCount,
                                                                     DE_NULL));

        const VkCooperativeMatrixPropertiesKHR initStruct = initVulkanStructureConst();

        std::vector<VkCooperativeMatrixPropertiesKHR> properties(propertyCount, initStruct);

        VK_CHECK(vki.getPhysicalDeviceCooperativeMatrixPropertiesKHR(m_context.getPhysicalDevice(), &propertyCount,
                                                                     properties.data()));

        for (size_t i = 0; i < properties.size(); ++i)
        {
            const VkCooperativeMatrixPropertiesKHR *p = &properties[i];

            if (p->scope != VK_SCOPE_SUBGROUP_KHR)
                continue;

            if (p->AType == VK_COMPONENT_TYPE_FLOAT16_KHR)
            {
                if (p->MSize * p->KSize > coopmatRows * coopmatCols)
                {
                    coopmatRows = p->MSize;
                    coopmatCols = p->KSize;
                }
            }
        }
        DE_ASSERT(coopmatRows * coopmatCols > 0);
    }
#endif // CTS_USES_VULKANSC

    uint32_t specializationData[3]                           = {deFloatBitsToUint32(2.0f), coopmatRows, coopmatCols};
    const vk::VkSpecializationMapEntry specializationMaps[3] = {
        {
            0u,               // uint32_t constantID;
            0u,               // uint32_t offset;
            sizeof(uint32_t), // uintptr_t size;
        },
        {
            1u,               // uint32_t constantID;
            4u,               // uint32_t offset;
            sizeof(uint32_t), // uintptr_t size;
        },
        {
            2u,               // uint32_t constantID;
            8u,               // uint32_t offset;
            sizeof(uint32_t), // uintptr_t size;
        },
    };
    const vk::VkSpecializationInfo specializationInfo = {
        3u,                                                 // uint32_t mapEntryCount;
        specializationMaps,                                 // const VkSpecializationMapEntry* pMapEntries;
        static_cast<uintptr_t>(sizeof(specializationData)), // uintptr_t dataSize;
        specializationData,                                 // const void* pData;
    };
    pipeline.setSpecializationInfo(specializationInfo);
    pipeline.buildPipeline();

    const VkBufferMemoryBarrier computeFinishBarrier =
        makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer, 0ull, bufferSizeBytes);

    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Start recording commands

    beginCommandBuffer(vk, *cmdBuffer);

    pipeline.bind(*cmdBuffer);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getPipelineLayout(), 0u, 1u,
                             &descriptorSet.get(), 0u, DE_NULL);

    vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                          (VkDependencyFlags)0, 0, (const VkMemoryBarrier *)DE_NULL, 1, &computeFinishBarrier, 0,
                          (const VkImageMemoryBarrier *)DE_NULL);

    endCommandBuffer(vk, *cmdBuffer);

    // Wait for completion

    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    // Validate the results

    invalidateAlloc(vk, device, bufferAllocation);

    const void *outStruct = bufferAllocation.getHostPtr();

    {
        float vecelem       = m_instType == SPECCONSTANT ? 2.0f : 1.0f;
        float vecRef[4]     = {vecelem, vecelem, vecelem, vecelem};
        float matRef[4 * 4] = {vecelem, vecelem, vecelem, vecelem, vecelem, vecelem, vecelem, vecelem,
                               vecelem, vecelem, vecelem, vecelem, vecelem, vecelem, vecelem, vecelem};
        float coopmatRef[2] = {vecelem, vecelem};

        uint32_t arrElem      = m_instType == SPECCONSTANT ? deFloatBitsToUint32(2.0f) : 3;
        uint32_t arrRef[3]    = {arrElem, arrElem, arrElem};
        uint32_t arrarrRef[6] = {arrElem, arrElem, arrElem, arrElem, arrElem, arrElem};

        uint32_t strElem      = m_instType == SPECCONSTANT ? deFloatBitsToUint32(2.0f) : 6;
        uint32_t strRef[3]    = {strElem, strElem, strElem};
        uint32_t strstrRef[6] = {strElem, strElem, strElem, strElem, strElem, strElem};

        const void *ref  = DE_NULL;
        size_t sizeofref = 0;

        switch (m_compositeType)
        {
        case VECTOR:
            ref       = vecRef;
            sizeofref = sizeof(vecRef);
            break;
        case MATRIX:
            ref       = matRef;
            sizeofref = sizeof(matRef);
            break;
        case ARRAY:
            ref       = arrRef;
            sizeofref = sizeof(arrRef);
            break;
        case ARRAY_ARRAY:
            ref       = arrarrRef;
            sizeofref = sizeof(arrarrRef);
            break;
        case STRUCT:
            ref       = strRef;
            sizeofref = sizeof(strRef);
            break;
        case STRUCT_STRUCT:
            ref       = strstrRef;
            sizeofref = sizeof(strstrRef);
            break;
        case COOPMAT:
            ref       = coopmatRef;
            sizeofref = sizeof(coopmatRef);
            break;
        default:
            DE_ASSERT(0);
            break;
        }
        DE_ASSERT(sizeofref <= bufferSizeBytes);

        if (deMemCmp(outStruct, ref, sizeofref) != 0)
        {
            return tcu::TestStatus::fail("Comparison failed");
        }
    }
    return tcu::TestStatus::pass("Compute succeeded");
}

} // namespace

tcu::TestCaseGroup *createBasicComputeShaderTests(tcu::TestContext &testCtx,
                                                  vk::ComputePipelineConstructionType computePipelineConstructionType)
{
    // Basic compute tests
    de::MovePtr<tcu::TestCaseGroup> basicComputeTests(new tcu::TestCaseGroup(testCtx, "basic"));

    // Shader that does nothing
    addFunctionCaseWithPrograms(basicComputeTests.get(), "empty_shader", EmptyShaderTest::checkSupport,
                                EmptyShaderTest::createProgram, EmptyShaderTest::createTest,
                                computePipelineConstructionType);

    // Concurrent compute test
    basicComputeTests->addChild(new ConcurrentCompute(testCtx, "concurrent_compute", computePipelineConstructionType));

    // Use an empty workgroup with size 0 on the X axis
    basicComputeTests->addChild(
        new EmptyWorkGroupCase(testCtx, "empty_workgroup_x", tcu::UVec3(0u, 2u, 3u), computePipelineConstructionType));
    // Use an empty workgroup with size 0 on the Y axis
    basicComputeTests->addChild(
        new EmptyWorkGroupCase(testCtx, "empty_workgroup_y", tcu::UVec3(2u, 0u, 3u), computePipelineConstructionType));
    // Use an empty workgroup with size 0 on the Z axis
    basicComputeTests->addChild(
        new EmptyWorkGroupCase(testCtx, "empty_workgroup_z", tcu::UVec3(2u, 3u, 0u), computePipelineConstructionType));
    // Use an empty workgroup with size 0 on the X, Y and Z axes
    basicComputeTests->addChild(new EmptyWorkGroupCase(testCtx, "empty_workgroup_all", tcu::UVec3(0u, 0u, 0u),
                                                       computePipelineConstructionType));

    // Use the maximum work group size on the X axis
    basicComputeTests->addChild(new MaxWorkGroupSizeTest(testCtx, "max_local_size_x",
                                                         MaxWorkGroupSizeTest::Params{MaxWorkGroupSizeTest::Axis::X},
                                                         computePipelineConstructionType));
    // Use the maximum work group size on the Y axis
    basicComputeTests->addChild(new MaxWorkGroupSizeTest(testCtx, "max_local_size_y",
                                                         MaxWorkGroupSizeTest::Params{MaxWorkGroupSizeTest::Axis::Y},
                                                         computePipelineConstructionType));
    // Use the maximum work group size on the Z axis
    basicComputeTests->addChild(new MaxWorkGroupSizeTest(testCtx, "max_local_size_z",
                                                         MaxWorkGroupSizeTest::Params{MaxWorkGroupSizeTest::Axis::Z},
                                                         computePipelineConstructionType));

    // Concurrent compute test
    basicComputeTests->addChild(BufferToBufferInvertTest::UBOToSSBOInvertCase(
        testCtx, "ubo_to_ssbo_single_invocation", 256, tcu::IVec3(1, 1, 1), tcu::IVec3(1, 1, 1),
        computePipelineConstructionType));
    basicComputeTests->addChild(BufferToBufferInvertTest::UBOToSSBOInvertCase(testCtx, "ubo_to_ssbo_single_group", 1024,
                                                                              tcu::IVec3(2, 1, 4), tcu::IVec3(1, 1, 1),
                                                                              computePipelineConstructionType));
    basicComputeTests->addChild(BufferToBufferInvertTest::UBOToSSBOInvertCase(
        testCtx, "ubo_to_ssbo_multiple_invocations", 1024, tcu::IVec3(1, 1, 1), tcu::IVec3(2, 4, 1),
        computePipelineConstructionType));
    basicComputeTests->addChild(
        BufferToBufferInvertTest::UBOToSSBOInvertCase(testCtx, "ubo_to_ssbo_multiple_groups", 1024, tcu::IVec3(1, 4, 2),
                                                      tcu::IVec3(2, 2, 4), computePipelineConstructionType));

    // Concurrent compute test
    basicComputeTests->addChild(
        BufferToBufferInvertTest::CopyInvertSSBOCase(testCtx, "copy_ssbo_single_invocation", 256, tcu::IVec3(1, 1, 1),
                                                     tcu::IVec3(1, 1, 1), computePipelineConstructionType));
    basicComputeTests->addChild(BufferToBufferInvertTest::CopyInvertSSBOCase(
        testCtx, "copy_ssbo_multiple_invocations", 1024, tcu::IVec3(1, 1, 1), tcu::IVec3(2, 4, 1),
        computePipelineConstructionType));
    basicComputeTests->addChild(BufferToBufferInvertTest::CopyInvertSSBOCase(testCtx, "copy_ssbo_multiple_groups", 1024,
                                                                             tcu::IVec3(1, 4, 2), tcu::IVec3(2, 2, 4),
                                                                             computePipelineConstructionType));

    // Read and write same SSBO
    basicComputeTests->addChild(new InvertSSBOInPlaceTest(testCtx, "ssbo_rw_single_invocation", 256, true,
                                                          tcu::IVec3(1, 1, 1), tcu::IVec3(1, 1, 1),
                                                          computePipelineConstructionType));
    basicComputeTests->addChild(new InvertSSBOInPlaceTest(testCtx, "ssbo_rw_multiple_groups", 1024, true,
                                                          tcu::IVec3(1, 4, 2), tcu::IVec3(2, 2, 4),
                                                          computePipelineConstructionType));
    basicComputeTests->addChild(new InvertSSBOInPlaceTest(testCtx, "ssbo_unsized_arr_single_invocation", 256, false,
                                                          tcu::IVec3(1, 1, 1), tcu::IVec3(1, 1, 1),
                                                          computePipelineConstructionType));
    basicComputeTests->addChild(new InvertSSBOInPlaceTest(testCtx, "ssbo_unsized_arr_multiple_groups", 1024, false,
                                                          tcu::IVec3(1, 4, 2), tcu::IVec3(2, 2, 4),
                                                          computePipelineConstructionType));

    // Write to multiple SSBOs
    basicComputeTests->addChild(new WriteToMultipleSSBOTest(testCtx, "write_multiple_arr_single_invocation", 256, true,
                                                            tcu::IVec3(1, 1, 1), tcu::IVec3(1, 1, 1),
                                                            computePipelineConstructionType));
    basicComputeTests->addChild(new WriteToMultipleSSBOTest(testCtx, "write_multiple_arr_multiple_groups", 1024, true,
                                                            tcu::IVec3(1, 4, 2), tcu::IVec3(2, 2, 4),
                                                            computePipelineConstructionType));
    basicComputeTests->addChild(new WriteToMultipleSSBOTest(testCtx, "write_multiple_unsized_arr_single_invocation",
                                                            256, false, tcu::IVec3(1, 1, 1), tcu::IVec3(1, 1, 1),
                                                            computePipelineConstructionType));
    basicComputeTests->addChild(new WriteToMultipleSSBOTest(testCtx, "write_multiple_unsized_arr_multiple_groups", 1024,
                                                            false, tcu::IVec3(1, 4, 2), tcu::IVec3(2, 2, 4),
                                                            computePipelineConstructionType));

    // SSBO local barrier usage
    basicComputeTests->addChild(new SSBOLocalBarrierTest(testCtx, "ssbo_local_barrier_single_invocation",
                                                         tcu::IVec3(1, 1, 1), tcu::IVec3(1, 1, 1),
                                                         computePipelineConstructionType));
    basicComputeTests->addChild(new SSBOLocalBarrierTest(testCtx, "ssbo_local_barrier_single_group",
                                                         tcu::IVec3(3, 2, 5), tcu::IVec3(1, 1, 1),
                                                         computePipelineConstructionType));
    basicComputeTests->addChild(new SSBOLocalBarrierTest(testCtx, "ssbo_local_barrier_multiple_groups",
                                                         tcu::IVec3(3, 4, 1), tcu::IVec3(2, 7, 3),
                                                         computePipelineConstructionType));

    // SSBO memory barrier usage
    basicComputeTests->addChild(
        new SSBOBarrierTest(testCtx, "ssbo_cmd_barrier_single", tcu::IVec3(1, 1, 1), computePipelineConstructionType));
    basicComputeTests->addChild(new SSBOBarrierTest(testCtx, "ssbo_cmd_barrier_multiple", tcu::IVec3(11, 5, 7),
                                                    computePipelineConstructionType));

    // Basic shared variable usage
    basicComputeTests->addChild(new SharedVarTest(testCtx, "shared_var_single_invocation", tcu::IVec3(1, 1, 1),
                                                  tcu::IVec3(1, 1, 1), computePipelineConstructionType));
    basicComputeTests->addChild(new SharedVarTest(testCtx, "shared_var_single_group", tcu::IVec3(3, 2, 5),
                                                  tcu::IVec3(1, 1, 1), computePipelineConstructionType));
    basicComputeTests->addChild(new SharedVarTest(testCtx, "shared_var_multiple_invocations", tcu::IVec3(1, 1, 1),
                                                  tcu::IVec3(2, 5, 4), computePipelineConstructionType));
    basicComputeTests->addChild(new SharedVarTest(testCtx, "shared_var_multiple_groups", tcu::IVec3(3, 4, 1),
                                                  tcu::IVec3(2, 7, 3), computePipelineConstructionType));

    // Atomic operation with shared var
    basicComputeTests->addChild(new SharedVarAtomicOpTest(testCtx, "shared_atomic_op_single_invocation",
                                                          tcu::IVec3(1, 1, 1), tcu::IVec3(1, 1, 1),
                                                          computePipelineConstructionType));
    basicComputeTests->addChild(new SharedVarAtomicOpTest(testCtx, "shared_atomic_op_single_group", tcu::IVec3(3, 2, 5),
                                                          tcu::IVec3(1, 1, 1), computePipelineConstructionType));
    basicComputeTests->addChild(new SharedVarAtomicOpTest(testCtx, "shared_atomic_op_multiple_invocations",
                                                          tcu::IVec3(1, 1, 1), tcu::IVec3(2, 5, 4),
                                                          computePipelineConstructionType));
    basicComputeTests->addChild(new SharedVarAtomicOpTest(testCtx, "shared_atomic_op_multiple_groups",
                                                          tcu::IVec3(3, 4, 1), tcu::IVec3(2, 7, 3),
                                                          computePipelineConstructionType));

    // Image to SSBO copy
    basicComputeTests->addChild(new CopyImageToSSBOTest(testCtx, "copy_image_to_ssbo_small", tcu::IVec2(1, 1),
                                                        tcu::IVec2(64, 64), computePipelineConstructionType));
    basicComputeTests->addChild(new CopyImageToSSBOTest(testCtx, "copy_image_to_ssbo_large", tcu::IVec2(2, 4),
                                                        tcu::IVec2(512, 512), computePipelineConstructionType));

    // SSBO to image copy
    basicComputeTests->addChild(new CopySSBOToImageTest(testCtx, "copy_ssbo_to_image_small", tcu::IVec2(1, 1),
                                                        tcu::IVec2(64, 64), computePipelineConstructionType));
    basicComputeTests->addChild(new CopySSBOToImageTest(testCtx, "copy_ssbo_to_image_large", tcu::IVec2(2, 4),
                                                        tcu::IVec2(512, 512), computePipelineConstructionType));

    // Atomic operation with image
    basicComputeTests->addChild(new ImageAtomicOpTest(testCtx, "image_atomic_op_local_size_1", 1, tcu::IVec2(64, 64),
                                                      computePipelineConstructionType));
    basicComputeTests->addChild(new ImageAtomicOpTest(testCtx, "image_atomic_op_local_size_8", 8, tcu::IVec2(64, 64),
                                                      computePipelineConstructionType));

    // Image barrier
    basicComputeTests->addChild(
        new ImageBarrierTest(testCtx, "image_barrier_single", tcu::IVec2(1, 1), computePipelineConstructionType));
    basicComputeTests->addChild(
        new ImageBarrierTest(testCtx, "image_barrier_multiple", tcu::IVec2(64, 64), computePipelineConstructionType));

    // Test secondary command buffers in compute only queues
    basicComputeTests->addChild(
        new ComputeOnlyQueueTests::SecondaryCommandBufferComputeOnlyTest(testCtx, "secondary_compute_only_queue"));

#ifndef CTS_USES_VULKANSC
    for (uint32_t i = 0; i < 3; ++i)
    {
        const char *instStr[3] = {"value", "constant", "specconstant"};
        std::string name;
        name = std::string("replicated_composites_vector_") + instStr[i];
        basicComputeTests->addChild(
            new ReplicatedCompositesTest(testCtx, name.c_str(), VECTOR, (InstType)i, computePipelineConstructionType));
        name = std::string("replicated_composites_matrix_") + instStr[i];
        basicComputeTests->addChild(
            new ReplicatedCompositesTest(testCtx, name.c_str(), MATRIX, (InstType)i, computePipelineConstructionType));
        name = std::string("replicated_composites_array_") + instStr[i];
        basicComputeTests->addChild(
            new ReplicatedCompositesTest(testCtx, name.c_str(), ARRAY, (InstType)i, computePipelineConstructionType));
        name = std::string("replicated_composites_array_array_") + instStr[i];
        basicComputeTests->addChild(new ReplicatedCompositesTest(testCtx, name.c_str(), ARRAY_ARRAY, (InstType)i,
                                                                 computePipelineConstructionType));
        name = std::string("replicated_composites_struct_") + instStr[i];
        basicComputeTests->addChild(
            new ReplicatedCompositesTest(testCtx, name.c_str(), STRUCT, (InstType)i, computePipelineConstructionType));
        name = std::string("replicated_composites_struct_struct_") + instStr[i];
        basicComputeTests->addChild(new ReplicatedCompositesTest(testCtx, name.c_str(), STRUCT_STRUCT, (InstType)i,
                                                                 computePipelineConstructionType));
        name = std::string("replicated_composites_coopmat_") + instStr[i];
        basicComputeTests->addChild(
            new ReplicatedCompositesTest(testCtx, name.c_str(), COOPMAT, (InstType)i, computePipelineConstructionType));
    }

    if (!isComputePipelineConstructionTypeShaderObject(computePipelineConstructionType))
    {
        basicComputeTests->addChild(
            cts_amber::createAmberTestCase(testCtx, "write_ssbo_array", "", "compute", "write_ssbo_array.amber"));
        basicComputeTests->addChild(
            cts_amber::createAmberTestCase(testCtx, "branch_past_barrier", "", "compute", "branch_past_barrier.amber"));
        basicComputeTests->addChild(cts_amber::createAmberTestCase(
            testCtx, "webgl_spirv_loop",
            "Simple SPIR-V loop from a WebGL example that caused problems in some implementations", "compute",
            "webgl_spirv_loop.amber"));

        {
            cts_amber::AmberTestCase *testCase = cts_amber::createAmberTestCase(
                testCtx, "pk_immediate", "Immediate/inline arguments to packed 16-bit operations", "compute",
                "pk-immediate.amber");
            testCase->addRequirement("Storage16BitFeatures.storageBuffer16BitAccess");
            testCase->addRequirement("Float16Int8Features.shaderFloat16");
            testCase->addRequirement("Features.shaderInt16");
            testCase->addPropertyRequirement("FloatControlsProperties.shaderDenormPreserveFloat16");
            basicComputeTests->addChild(testCase);
        }

        {
            cts_amber::AmberTestCase *testCase = cts_amber::createAmberTestCase(
                testCtx, "pkadd_immediate", "Immediate/inline arguments to packed 16-bit operations", "compute",
                "pkadd-immediate.amber");
            testCase->addRequirement("Features.shaderInt16");
            testCase->addRequirement("Storage16BitFeatures.storageBuffer16BitAccess");
            basicComputeTests->addChild(testCase);
        }
    }
#endif

    return basicComputeTests.release();
}

tcu::TestCaseGroup *createBasicDeviceGroupComputeShaderTests(
    tcu::TestContext &testCtx, vk::ComputePipelineConstructionType computePipelineConstructionType)
{
    de::MovePtr<tcu::TestCaseGroup> deviceGroupComputeTests(new tcu::TestCaseGroup(testCtx, "device_group"));

    deviceGroupComputeTests->addChild(new DispatchBaseTest(testCtx, "dispatch_base", 32768, tcu::IVec3(4, 2, 4),
                                                           tcu::IVec3(16, 8, 8), tcu::IVec3(4, 8, 8),
                                                           computePipelineConstructionType, false));
#ifndef CTS_USES_VULKANSC
    deviceGroupComputeTests->addChild(new DispatchBaseTest(testCtx, "dispatch_base_maintenance5", 32768,
                                                           tcu::IVec3(4, 2, 4), tcu::IVec3(16, 8, 8),
                                                           tcu::IVec3(4, 8, 8), computePipelineConstructionType, true));
#endif
    deviceGroupComputeTests->addChild(new DeviceIndexTest(testCtx, "device_index", 96, tcu::IVec3(3, 2, 1),
                                                          tcu::IVec3(2, 4, 1), computePipelineConstructionType));

    return deviceGroupComputeTests.release();
}
} // namespace compute
} // namespace vkt
