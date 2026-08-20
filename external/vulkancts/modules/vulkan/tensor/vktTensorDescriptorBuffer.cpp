/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 ARM Ltd.
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
 */
/*!
 * \file
 * \brief Tensor descriptor buffer tests
 */
/*--------------------------------------------------------------------*/

#include "vktTensorTests.hpp"

#include "vktTensorTestsUtil.hpp"
#include "vktTestCase.hpp"

#include "vktTestGroupUtil.hpp"
#include "shaders/vktTensorShaders.hpp"
#include "shaders/vktTensorShaderUtil.hpp"
#include "vkTensorMemoryUtil.hpp"

#include "vkBuilderUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "vkTensorWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkTensorUtil.hpp"

#include "deMemory.h"

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "tcuFunctionLibrary.hpp"
#include "tcuPlatform.hpp"
#include "tcuCommandLine.hpp"
#include "tcuResource.hpp"

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <sstream>

namespace vkt
{
namespace tensor
{

namespace
{

using namespace vk;

TensorParameters calculateMaxDimensionCountParameters(const TensorParameters &baseParameters,
                                                      const vk::InstanceInterface &vki,
                                                      const vk::VkPhysicalDevice physicalDevice)
{
    const uint32_t rank = getTensorMaxDimensionCount(vki, physicalDevice);

    TensorParameters maxRankParameters = baseParameters;

    maxRankParameters.dimensions           = TensorDimensions(rank, 1);
    maxRankParameters.dimensions[0]        = 16;
    maxRankParameters.dimensions[rank - 2] = 4;
    maxRankParameters.dimensions[rank - 1] = 8;

    return maxRankParameters;
}

template <typename T>
class DescriptorBufferTestInstance : public TestInstance
{
public:
    DescriptorBufferTestInstance(Context &testCtx, const TensorParameters &parameters, const bool &hostWrite,
                                 const uint32_t &descriptorCount, const size_t &shiftIndex)
        : TestInstance(testCtx)
        , m_parameters(parameters)
        , m_hostWrite(hostWrite)
        , m_descriptorCount(descriptorCount)
        , m_shiftIndex(shiftIndex)
    {
    }

    tcu::TestStatus iterate() override;

private:
    const TensorParameters m_parameters;
    const bool m_hostWrite;
    const uint32_t m_descriptorCount;
    const size_t m_shiftIndex;
};

template <typename T>
tcu::TestStatus DescriptorBufferTestInstance<T>::iterate()
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // Create descriptor set layout which is used for both rotate and validate pipelines

    const Unique<VkDescriptorSetLayout> pipelineDescSetLayout(
        DescriptorSetLayoutBuilder()
            .addArrayBinding(VK_DESCRIPTOR_TYPE_TENSOR_ARM, m_descriptorCount, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(vk, device, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT));

    // Create descriptor buffer

    // descriptor buffer and output buffer
    const MemoryRequirement bufferMemReq = MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress;

    // Get memory size for descriptor set layout

    VkDeviceSize descBufferSize{};
    vk.getDescriptorSetLayoutSizeEXT(device, *pipelineDescSetLayout, &descBufferSize);

    const auto descBufferUsage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT |
                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    const VkBufferCreateInfo descBufferCreateInfo = makeBufferCreateInfo(descBufferSize, descBufferUsage);
    BufferWithMemory descBuffer(vk, device, allocator, descBufferCreateInfo, bufferMemReq);
    auto *descBufferHostPtr = static_cast<char *>(descBuffer.getAllocation().getHostPtr());

    // Get descriptor buffer device info for pipeline binding

    VkBufferDeviceAddressInfo descBufferDeviceAddressInfo = initVulkanStructure();
    descBufferDeviceAddressInfo.buffer                    = *descBuffer;

    auto descBufferDeviceAddress = vk.getBufferDeviceAddress(device, &descBufferDeviceAddressInfo);

    VkDescriptorBufferBindingInfoEXT descBufferBindingInfo = initVulkanStructure();
    descBufferBindingInfo.address                          = descBufferDeviceAddress;
    descBufferBindingInfo.usage                            = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

    // Create containers for tensors, tensor views and expected tensor elements

    std::vector<de::MovePtr<TensorWithMemory>> tensors(m_descriptorCount);
    std::vector<Move<vk::VkTensorViewARM>> tensorViews(m_descriptorCount);
    std::vector<StridedMemoryUtils<T>> testTensorData(m_descriptorCount);
    std::vector<StridedMemoryUtils<T>> expectedTensorData(m_descriptorCount);

    // Fill tensors

    for (uint32_t i = 0; i < m_descriptorCount; ++i)
    {
        const VkTensorDescriptionARM tensorDesc =
            makeTensorDescription(m_parameters.tiling, m_parameters.format, m_parameters.dimensions,
                                  m_parameters.strides, VK_TENSOR_USAGE_SHADER_BIT_ARM);
        VkTensorCreateInfoARM tensorCreateInfo = makeTensorCreateInfo(&tensorDesc);
        tensors[i]                             = de::MovePtr<TensorWithMemory>(
            new TensorWithMemory(vk, device, allocator, tensorCreateInfo, vk::MemoryRequirement::Any));
        tensorViews[i] = Move<VkTensorViewARM>(makeTensorView(vk, device, tensors[i]->get(), m_parameters.format));
        expectedTensorData[i] = StridedMemoryUtils<T>(m_parameters.dimensions, m_parameters.strides);
        testTensorData[i]     = StridedMemoryUtils<T>(m_parameters.dimensions, m_parameters.strides);

        // expected values for host written descriptor buffer are set to descriptor index
        // expected values for device written descriptor buffer are set to descriptor index shifted left by shift index
        const T expectedValue = static_cast<T>(i);
        // If shift index is larger than descriptor count, normalise
        size_t norm_shiftIndex =
            (m_descriptorCount < m_shiftIndex) ? static_cast<size_t>(m_shiftIndex % m_descriptorCount) : m_shiftIndex;
        const T fillValue =
            m_hostWrite ? expectedValue : static_cast<T>((i + m_descriptorCount - norm_shiftIndex) % m_descriptorCount);

        testTensorData[i].fill(fillValue);
        expectedTensorData[i].fill(expectedValue);
        uploadToTensor(vk, device, allocator, queue, queueFamilyIndex, *tensors[i], testTensorData[i].data(),
                       testTensorData[i].memorySize());
    }

    // Get tensor descriptor info

    const auto &tensorDescBufferProperties = m_context.getDescriptorBufferTensorPropertiesARM();
    const size_t tensorDescSize            = tensorDescBufferProperties.tensorDescriptorSize;

    // Get memory offset for tensor array descriptor binding

    VkDeviceSize tensorBindingOffset{};
    vk.getDescriptorSetLayoutBindingOffsetEXT(device, *pipelineDescSetLayout, 0, &tensorBindingOffset);

    // Copy tensor descriptor into descriptor buffer

    for (uint32_t i = 0; i < m_descriptorCount; ++i)
    {
        VkDescriptorGetTensorInfoARM tensorDescInfo = initVulkanStructure();
        tensorDescInfo.tensorView                   = tensorViews[i].get();

        VkDescriptorGetInfoEXT descInfo = initVulkanStructure();
        descInfo.type                   = VK_DESCRIPTOR_TYPE_TENSOR_ARM;
        descInfo.pNext                  = &tensorDescInfo;

        const auto arrayElementOffset = tensorBindingOffset + (i * tensorDescSize);
        vk.getDescriptorEXT(device, &descInfo, tensorDescSize, descBufferHostPtr + arrayElementOffset);
    }

    // Create output buffer to write elements for tensors

    const size_t tensorElementCount = static_cast<size_t>(std::accumulate(
        m_parameters.dimensions.cbegin(), m_parameters.dimensions.cend(), int64_t{1}, std::multiplies<int64_t>()));
    const size_t totalElementCount  = tensorElementCount * m_descriptorCount;
    const size_t outputBufferSize   = totalElementCount * sizeof(T);

    const auto outputBufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    const VkBufferCreateInfo outputBufferCreateInfo = makeBufferCreateInfo(outputBufferSize, outputBufferUsage);
    const BufferWithMemory outputBuffer(vk, device, allocator, outputBufferCreateInfo, bufferMemReq);

    // Clear output buffer memory

    {
        const Allocation &outputBufferAllocation = outputBuffer.getAllocation();
        StridedMemoryUtils<T> outputBufferMemory({static_cast<int64_t>(totalElementCount)}, {},
                                                 outputBufferAllocation.getHostPtr());
        outputBufferMemory.clear();
        flushAlloc(vk, device, outputBufferAllocation);
    }

    // Create output buffer descriptor info

    VkBufferDeviceAddressInfo outputBufferDeviceAddressInfo = initVulkanStructure();
    outputBufferDeviceAddressInfo.buffer                    = *outputBuffer;
    auto outputBufferDeviceAddress = vk.getBufferDeviceAddress(device, &outputBufferDeviceAddressInfo);

    VkDescriptorAddressInfoEXT outputBufferDescAddress = initVulkanStructure();
    outputBufferDescAddress.address                    = outputBufferDeviceAddress;
    outputBufferDescAddress.range                      = outputBufferSize;

    VkDescriptorGetInfoEXT outputBufferDescInfo = initVulkanStructure();
    outputBufferDescInfo.type                   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    outputBufferDescInfo.data.pStorageBuffer    = &outputBufferDescAddress;

    // Get output buffer descriptor info

    const auto &descBufferProperties  = m_context.getDescriptorBufferPropertiesEXT();
    const size_t outputBufferDescSize = descBufferProperties.storageBufferDescriptorSize;

    // Get memory offset for output buffer binding

    VkDeviceSize outputBufferBindingOffset{};
    vk.getDescriptorSetLayoutBindingOffsetEXT(device, *pipelineDescSetLayout, 1, &outputBufferBindingOffset);

    //  Copy output buffer descriptor into descriptor buffer

    vk.getDescriptorEXT(device, &outputBufferDescInfo, outputBufferDescSize,
                        descBufferHostPtr + outputBufferBindingOffset);
    flushAlloc(vk, device, descBuffer.getAllocation());

    // Perform Computation
    {
        // Create a command pool

        const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));

        // Prepare the command buffer

        const Unique<VkCommandBuffer> cmdBuffer(
            allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

        // Start recording

        beginCommandBuffer(vk, *cmdBuffer);

        // Create rotate pipeline

        // Descriptor Buffer Offsets set in the command buffer

        uint32_t bufferIndex = 0;
        VkDeviceSize offset  = 0;

        if (!m_hostWrite)
        {
            // Bind descriptor buffer

            vk.cmdBindDescriptorBuffersEXT(*cmdBuffer, 1, &descBufferBindingInfo);

            // Build shader

            const ProgramBinary &rotatePipelineBinary = m_context.getBinaryCollection().get("rotate");
            const Unique<VkShaderModule> rotatePipelineShaderModule(
                createShaderModule(vk, device, rotatePipelineBinary, 0u));

            // Setup rotate pipeline

            // Tensor Descriptor Address supplied at runtime

            const VkDeviceAddress tensorDescriptorAddress = descBufferDeviceAddress + tensorBindingOffset;
            const VkPushConstantRange descBuffPushRange =
                makePushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0u, sizeof(tensorDescriptorAddress));

            const Unique<VkPipelineLayout> rotatePipelineLayout(
                makePipelineLayout(vk, device, *pipelineDescSetLayout, &descBuffPushRange));

            // Tensor Descriptor Size supplied at pipeline creation

            const uint32_t specData               = static_cast<uint32_t>(tensorDescSize);
            VkSpecializationMapEntry specMapEntry = makeSpecializationMapEntry(0u, 0u, sizeof(specData));
            VkSpecializationInfo specInfo = makeSpecializationInfo(1u, &specMapEntry, sizeof(specData), &specData);

            const Unique<VkPipeline> rotatePipeline(
                makeComputePipeline(vk, device, *rotatePipelineLayout, VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
                                    nullptr, *rotatePipelineShaderModule, 0u, &specInfo));

            // Bind rotate pipeline

            vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *rotatePipeline);

            vk.cmdSetDescriptorBufferOffsetsEXT(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *rotatePipelineLayout, 0, 1,
                                                &bufferIndex, &offset);

            // Push constants

            vk.cmdPushConstants(*cmdBuffer, *rotatePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0u,
                                sizeof(tensorDescriptorAddress), &tensorDescriptorAddress);

            // Dispatch

            const uint32_t rotateDispatchCountX =
                singleDimensionWorkgroupCount(static_cast<uint32_t>(tensorDescSize), 128);
            DE_ASSERT(rotateDispatchCountX <= dispatchWorkgroupCountLimit);
            const uint32_t rotateDispatchCountY = static_cast<uint32_t>(std::gcd(m_descriptorCount, m_shiftIndex));
            DE_ASSERT(rotateDispatchCountY <= dispatchWorkgroupCountLimit);
            vk.cmdDispatch(*cmdBuffer, rotateDispatchCountX, rotateDispatchCountY, 1u);

            // Descriptor Buffer Memory Barrier

            const VkMemoryBarrier2KHR memoryBarrier = makeMemoryBarrier2(
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_2_DESCRIPTOR_BUFFER_READ_BIT_EXT);

            VkDependencyInfo dependencyInfo   = initVulkanStructure();
            dependencyInfo.memoryBarrierCount = 1u;
            dependencyInfo.pMemoryBarriers    = &memoryBarrier;

            // Pipeline barrier

            vk.cmdPipelineBarrier2(*cmdBuffer, &dependencyInfo);

            // End command buffer recording

            endCommandBuffer(vk, cmdBuffer.get());

            // Wait for completion

            submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

            // Reset command buffer for next dispatch

            vk.resetCommandBuffer(*cmdBuffer, 0u);

            // Start recording

            beginCommandBuffer(vk, *cmdBuffer);
        }

        // Create validation pipeline

        // Build shader

        const ProgramBinary &validatePipelineBinary = m_context.getBinaryCollection().get("validate");
        const Unique<VkShaderModule> validatePipelineShaderModule(
            createShaderModule(vk, device, validatePipelineBinary, 0u));

        // Setup pipeline

        const Unique<VkPipelineLayout> validatePipelineLayout(makePipelineLayout(vk, device, *pipelineDescSetLayout));
        const Unique<VkPipeline> validatePipeline(makeComputePipeline(vk, device, *validatePipelineLayout,
                                                                      VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
                                                                      nullptr, *validatePipelineShaderModule, 0u));

        // Bind pipeline

        vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *validatePipeline);

        // Bind descriptor buffer

        vk.cmdBindDescriptorBuffersEXT(*cmdBuffer, 1, &descBufferBindingInfo);

        vk.cmdSetDescriptorBufferOffsetsEXT(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *validatePipelineLayout, 0, 1,
                                            &bufferIndex, &offset);

        // Dispatch

        const uint32_t validateDispatchCountX =
            singleDimensionWorkgroupCount(static_cast<uint32_t>(tensorElementCount), 128);
        DE_ASSERT(validateDispatchCountX <= dispatchWorkgroupCountLimit);
        const uint32_t validateDispatchCountY = m_descriptorCount;
        DE_ASSERT(validateDispatchCountY <= dispatchWorkgroupCountLimit);
        vk.cmdDispatch(*cmdBuffer, validateDispatchCountX, validateDispatchCountY, 1u);

        // Buffer memory barrier

        const VkBufferMemoryBarrier outputBufferBarrier = makeBufferMemoryBarrier(
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *outputBuffer, 0u, outputBufferSize);

        // Pipeline barrier

        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,
                              nullptr, 1, &outputBufferBarrier, 0, nullptr);

        // End command buffer recording

        endCommandBuffer(vk, cmdBuffer.get());

        // Wait for completion

        submitCommandsAndWait(vk, device, queue, cmdBuffer.get());
    }

    // Validate the result

    {
        const Allocation &outputBufferAllocation = outputBuffer.getAllocation();
        invalidateAlloc(vk, device, outputBufferAllocation);

        // For each tensor descriptor, every element written to the buffer must match expected tensor element

        for (uint32_t i = 0; i < m_descriptorCount; ++i)
        {
            const auto &expected = expectedTensorData[i];
            T *base              = static_cast<T *>(outputBufferAllocation.getHostPtr()) + expected.elementCount() * i;

            StridedMemoryUtils<T> outputBufferMemoryView({static_cast<int64_t>(expected.elementCount())}, {}, base);

            auto status = compareStridedMemory(expected, outputBufferMemoryView);
            if (status.isFail())
            {
                return status;
            }
        }
    }

    return tcu::TestStatus::pass("Tensor test succeeded");
};

template <typename T>
class DescriptorBufferTestCase : public TestCase
{
private:
    std::string buildTestName(const TensorParameters &parameters, const bool &hostWrite,
                              const uint32_t &descriptorCount)
    {
        // Generate string
        std::ostringstream name;
        name << (hostWrite ? "host_" : "device_");
        name << "write_";
        name << descriptorCount << "_descriptors_";
        name << paramsToString(parameters);
        return name.str();
    }

public:
    DescriptorBufferTestCase(tcu::TestContext &testCtx, const TensorParameters &parameters, const bool &hostWrite,
                             const uint32_t &descriptorCount, const size_t &shiftIndex = 0)
        : TestCase(testCtx, buildTestName(parameters, hostWrite, descriptorCount))
        , m_parameters(parameters)
        , m_hostWrite(hostWrite)
        , m_descriptorCount(descriptorCount)
        , m_shiftIndex(shiftIndex)
    {
    }

    TestInstance *createInstance(Context &ctx) const override
    {
        const TensorParameters *parameters = &m_parameters;

        // If no tensor shape was provided, it is a test of the maximum dimension count
        // Query the max dimension count the implementation supports and set up a shape accordingly
        TensorParameters maxRankParameters{};
        if (m_parameters.rank() == 0)
        {
            maxRankParameters =
                calculateMaxDimensionCountParameters(m_parameters, ctx.getInstanceInterface(), ctx.getPhysicalDevice());

            parameters = &maxRankParameters;
        }

        return new DescriptorBufferTestInstance<T>(ctx, *parameters, m_hostWrite, m_descriptorCount, m_shiftIndex);
    }

    void checkSupport(Context &context) const override
    {
        // Tensor Checks

        context.requireDeviceFunctionality("VK_ARM_tensors");

        requireTensorShapeSupported(context, m_parameters);

        if (!formatSupportTensorFlags(context, m_parameters.format, m_parameters.tiling,
                                      VK_FORMAT_FEATURE_2_TENSOR_SHADER_BIT_ARM))
        {
            TCU_THROW(NotSupportedError, "Format not supported");
        }

        if (!deviceSupportsShaderTensorAccess(context))
        {
            TCU_THROW(NotSupportedError, "Device does not support shader tensor access");
        }

        if (!deviceSupportsShaderStagesTensorAccess(context, VK_SHADER_STAGE_COMPUTE_BIT))
        {
            TCU_THROW(NotSupportedError, "Device does not support shader tensor access in compute shader stage");
        }

        if (!deviceSupportsShaderStorageTensorArrayDynamicIndexing(context))
        {
            TCU_THROW(NotSupportedError, "Device does not support indexing arrays of storage tensors using dynamically "
                                         "uniform integer expressions in shader code");
        }

        // Descriptor Buffer Checks

        context.requireDeviceFunctionality("VK_EXT_descriptor_buffer");

        if (!context.getDescriptorBufferFeaturesEXT().descriptorBuffer)
        {
            TCU_THROW(NotSupportedError, "descriptorBuffer feature is not supported");
        }

        const InstanceInterface &vki          = context.getInstanceInterface();
        const VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

        VkPhysicalDeviceDescriptorBufferTensorFeaturesARM descriptorBufferTensorFeatures = initVulkanStructure();
        VkPhysicalDeviceFeatures2 features = initVulkanStructure(&descriptorBufferTensorFeatures);
        vki.getPhysicalDeviceFeatures2(physicalDevice, &features);

        if (!descriptorBufferTensorFeatures.descriptorBufferTensorDescriptors)
        {
            TCU_THROW(NotSupportedError,
                      "Device does not support putting shader-accessible tensor descriptors directly in memory");
        }

        // 8-bit Shader Code Support

        const bool support8BitFormat =
            m_parameters.format == VK_FORMAT_R8_UINT || m_parameters.format == VK_FORMAT_R8_SINT;

        if (!context.getDeviceVulkan12Features().shaderInt8 && (!m_hostWrite || support8BitFormat))
        {
            TCU_THROW(NotSupportedError, "8-bit integers (signed and unsigned) are not supported in shader code");
        }

        if (!context.getDeviceVulkan12Features().storageBuffer8BitAccess && (!m_hostWrite || support8BitFormat))
        {
            TCU_THROW(NotSupportedError, "8-bit integer members are not supported in storage buffer objects");
        }

        // Buffer Device Address Check

        if (!context.getBufferDeviceAddressFeatures().bufferDeviceAddress)
        {
            TCU_THROW(
                NotSupportedError,
                "Device does not support accessing buffer memory in shaders as storage buffers via a queried address");
        }
    }

    void initPrograms(vk::SourceCollections &programCollection) const override
    {
        size_t rank = m_parameters.rank();

        // If no tensor shape was provided, it is a test of the maximum dimension count
        // Query the max dimension count the implementation supports
        if (rank == 0)
        {
            de::SharedPtr<const ContextManager> contextManager = getContextManager();

            // We can't generate the source for max dimension count without a
            // context which has a physical device
            if (!contextManager || contextManager->getPhysicalDevice() == VK_NULL_HANDLE)
            {
                return;
            }

            rank =
                getTensorMaxDimensionCount(contextManager->getInstanceInterface(), contextManager->getPhysicalDevice());
        }

        programCollection.glslSources.add("rotate")
            << glu::ComputeSource(
                   genShaderTensorDescriptorBufferRotate(static_cast<size_t>(m_descriptorCount), m_shiftIndex))
            << vk::ShaderBuildOptions(programCollection.usedVulkanVersion,
                                      vk::getBaselineSpirvVersion(programCollection.usedVulkanVersion),
                                      vk::ShaderBuildOptions::FLAG_ALLOW_STD430_UBOS);
        programCollection.glslSources.add("validate") << glu::ComputeSource(
            genShaderTensorDescriptorBufferAccess(rank, m_parameters.format, static_cast<size_t>(m_descriptorCount)));
    }

private:
    const TensorParameters m_parameters;
    const bool m_hostWrite;
    const uint32_t m_descriptorCount;
    const size_t m_shiftIndex;
};

template <VkFormat Format>
void addDescriptorBufferTests(tcu::TestCaseGroup &testCaseGroup)
{
    const TensorDimensions &shape_min = {1};
    using T                           = typename VkFormatToHostType<Format>::HostType;

    // Implicitly packed linear
    const TensorParameters min_rank_tensor_params{Format, VK_TENSOR_TILING_LINEAR_ARM, shape_min, {}};

    // Max rank tensor dimension creation is deferred
    const TensorParameters empty_rank_tensor_params{Format, VK_TENSOR_TILING_LINEAR_ARM, {}, {}};

    // Host written descriptor buffer
    testCaseGroup.addChild(
        new DescriptorBufferTestCase<T>(testCaseGroup.getTestContext(), min_rank_tensor_params, true, 5));

    testCaseGroup.addChild(
        new DescriptorBufferTestCase<T>(testCaseGroup.getTestContext(), empty_rank_tensor_params, true, 7));

    // Device written descriptor buffer
    testCaseGroup.addChild(
        new DescriptorBufferTestCase<T>(testCaseGroup.getTestContext(), min_rank_tensor_params, false, 6, 7));

    testCaseGroup.addChild(
        new DescriptorBufferTestCase<T>(testCaseGroup.getTestContext(), empty_rank_tensor_params, false, 4, 5));
}

} // namespace

template <VkFormat... Formats>
void addDescriptorBufferTestsForFormats(tcu::TestCaseGroup &testCaseGroup)
{
    (addDescriptorBufferTests<Formats>(testCaseGroup), ...);
}

tcu::TestCaseGroup *createTensorDescriptorBufferTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(
        new tcu::TestCaseGroup(testCtx, "descriptor_buffer", "Tensor descriptor buffer tests"));

    addDescriptorBufferTestsForFormats<TENSOR_FORMATS_REGULAR_INTS>(*group);

    return group.release();
}

} // namespace tensor
} // namespace vkt
