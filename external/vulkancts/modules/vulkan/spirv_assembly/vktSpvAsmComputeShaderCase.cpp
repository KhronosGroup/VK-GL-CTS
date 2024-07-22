/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Google LLC
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
 * \brief Test Case Skeleton Based on Compute Shaders
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmComputeShaderCase.hpp"

#include "deSharedPtr.hpp"
#include "deSTLUtil.hpp"

#include "vktSpvAsmUtils.hpp"

#include "vkBuilderUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"

#include <cassert>

namespace
{

using namespace vk;
using std::vector;

typedef vkt::SpirVAssembly::AllocationMp AllocationMp;
typedef vkt::SpirVAssembly::AllocationSp AllocationSp;
typedef vk::Unique<VkBuffer> BufferHandleUp;
typedef vk::Unique<VkImage> ImageHandleUp;
typedef vk::Unique<VkImageView> ImageViewHandleUp;
typedef vk::Unique<VkSampler> SamplerHandleUp;
typedef de::SharedPtr<BufferHandleUp> BufferHandleSp;
typedef de::SharedPtr<ImageHandleUp> ImageHandleSp;
typedef de::SharedPtr<ImageViewHandleUp> ImageViewHandleSp;
typedef de::SharedPtr<SamplerHandleUp> SamplerHandleSp;

/*--------------------------------------------------------------------*//*!
 * \brief Create a buffer, allocate and bind memory for the buffer
 *
 * The memory is created as host visible and passed back as a vk::Allocation
 * instance via outMemory.
 *//*--------------------------------------------------------------------*/
Move<VkBuffer> createBufferAndBindMemory(vkt::Context &context, const DeviceInterface &vkdi, const VkDevice &device,
                                         VkDescriptorType dtype, Allocator &allocator, size_t numBytes,
                                         AllocationMp *outMemory, bool physStorageBuffer, bool coherent = false)
{
    VkBufferUsageFlags usageFlags = (VkBufferUsageFlags)0u;

    if (physStorageBuffer)
        usageFlags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    switch (dtype)
    {
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        usageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        break;
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        usageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        break;
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        usageFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        break;
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        usageFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        break;
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        usageFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        break;
    default:
        DE_FATAL("Not implemented");
    }

    const VkBufferCreateInfo bufferCreateInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // sType
        DE_NULL,                              // pNext
        0u,                                   // flags
        numBytes,                             // size
        usageFlags,                           // usage
        VK_SHARING_MODE_EXCLUSIVE,            // sharingMode
        0u,                                   // queueFamilyCount
        DE_NULL,                              // pQueueFamilyIndices
    };

    Move<VkBuffer> buffer(createBuffer(vkdi, device, &bufferCreateInfo));
    const VkMemoryRequirements requirements = getBufferMemoryRequirements(vkdi, device, *buffer);
    AllocationMp bufferMemory               = allocator.allocate(
        requirements, (coherent ? MemoryRequirement::Coherent : MemoryRequirement::Any) |
                          (context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address") && physStorageBuffer ?
                                             MemoryRequirement::DeviceAddress :
                                             MemoryRequirement::Any) |
                          MemoryRequirement::HostVisible);

    VK_CHECK(vkdi.bindBufferMemory(device, *buffer, bufferMemory->getMemory(), bufferMemory->getOffset()));
    *outMemory = bufferMemory;

    return buffer;
}

/*--------------------------------------------------------------------*//*!
 * \brief Create image, allocate and bind memory for the image
 *
 *//*--------------------------------------------------------------------*/
Move<VkImage> createImageAndBindMemory(const DeviceInterface &vkdi, const VkDevice &device, VkDescriptorType dtype,
                                       vk::VkFormat imageFormat, Allocator &allocator, uint32_t queueFamilyIndex,
                                       AllocationMp *outMemory)
{
    VkImageUsageFlags usageBits = (VkImageUsageFlags)0;

    switch (dtype)
    {
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        usageBits = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        break;
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        usageBits = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        break;
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        usageBits = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        break;
    default:
        DE_FATAL("Not implemented");
    }

    const VkImageCreateInfo resourceImageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        DE_NULL,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        imageFormat,                         // VkFormat format;
        {8, 8, 1},                           //  VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arraySize;
        VK_SAMPLE_COUNT_1_BIT,               // uint32_t samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        usageBits,                           //  VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        1u,                                  // uint32_t queueFamilyCount;
        &queueFamilyIndex,                   // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    // Create image
    Move<VkImage> image                     = createImage(vkdi, device, &resourceImageParams);
    const VkMemoryRequirements requirements = getImageMemoryRequirements(vkdi, device, *image);
    de::MovePtr<Allocation> imageMemory     = allocator.allocate(requirements, MemoryRequirement::Any);

    VK_CHECK(vkdi.bindImageMemory(device, *image, imageMemory->getMemory(), imageMemory->getOffset()));
    *outMemory = imageMemory;

    return image;
}

void setMemory(const DeviceInterface &vkdi, const VkDevice &device, Allocation *destAlloc, size_t numBytes,
               const void *data, bool coherent = false)
{
    void *const hostPtr = destAlloc->getHostPtr();

    deMemcpy((uint8_t *)hostPtr, data, numBytes);

    if (!coherent)
        flushAlloc(vkdi, device, *destAlloc);
}

void fillMemoryWithValue(const DeviceInterface &vkdi, const VkDevice &device, Allocation *destAlloc, size_t numBytes,
                         uint8_t value, bool coherent = false)
{
    void *const hostPtr = destAlloc->getHostPtr();

    deMemset((uint8_t *)hostPtr, value, numBytes);

    if (!coherent)
        flushAlloc(vkdi, device, *destAlloc);
}

void invalidateMemory(const DeviceInterface &vkdi, const VkDevice &device, Allocation *srcAlloc, bool coherent = false)
{
    if (!coherent)
        invalidateAlloc(vkdi, device, *srcAlloc);
}

/*--------------------------------------------------------------------*//*!
 * \brief Create a descriptor set layout with the given descriptor types
 *
 * All descriptors are created for compute pipeline.
 *//*--------------------------------------------------------------------*/
Move<VkDescriptorSetLayout> createDescriptorSetLayout(const DeviceInterface &vkdi, const VkDevice &device,
                                                      const vector<VkDescriptorType> &dtypes)
{
    DescriptorSetLayoutBuilder builder;

    for (size_t bindingNdx = 0; bindingNdx < dtypes.size(); ++bindingNdx)
        builder.addSingleBinding(dtypes[bindingNdx], VK_SHADER_STAGE_COMPUTE_BIT);

    return builder.build(vkdi, device);
}

/*--------------------------------------------------------------------*//*!
 * \brief Create a pipeline layout with one descriptor set
 *//*--------------------------------------------------------------------*/
Move<VkPipelineLayout> createPipelineLayout(const DeviceInterface &vkdi, const VkDevice &device,
                                            VkDescriptorSetLayout descriptorSetLayout,
                                            const vkt::SpirVAssembly::BufferSp &pushConstants)
{
    VkPipelineLayoutCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
        DE_NULL,                                       // pNext
        (VkPipelineLayoutCreateFlags)0,
        1u,                   // descriptorSetCount
        &descriptorSetLayout, // pSetLayouts
        0u,                   // pushConstantRangeCount
        DE_NULL,              // pPushConstantRanges
    };

    VkPushConstantRange range = {
        VK_SHADER_STAGE_COMPUTE_BIT, // stageFlags
        0,                           // offset
        0,                           // size
    };

    if (pushConstants)
    {
        vector<uint8_t> pushConstantsBytes;
        pushConstants->getBytes(pushConstantsBytes);

        range.size                        = static_cast<uint32_t>(pushConstantsBytes.size());
        createInfo.pushConstantRangeCount = 1;
        createInfo.pPushConstantRanges    = &range;
    }

    return createPipelineLayout(vkdi, device, &createInfo);
}

/*--------------------------------------------------------------------*//*!
 * \brief Create a one-time descriptor pool for one descriptor set that
 * support the given descriptor types.
 *//*--------------------------------------------------------------------*/
inline Move<VkDescriptorPool> createDescriptorPool(const DeviceInterface &vkdi, const VkDevice &device,
                                                   const vector<VkDescriptorType> &dtypes)
{
    DescriptorPoolBuilder builder;

    for (size_t typeNdx = 0; typeNdx < dtypes.size(); ++typeNdx)
        builder.addType(dtypes[typeNdx], 1);

    return builder.build(vkdi, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, /* maxSets = */ 1);
}

/*--------------------------------------------------------------------*//*!
 * \brief Create a descriptor set
 *
 * The descriptor set's layout contains the given descriptor types,
 * sequentially binded to binding points starting from 0.
 *//*--------------------------------------------------------------------*/
Move<VkDescriptorSet> createDescriptorSet(const DeviceInterface &vkdi, const VkDevice &device, VkDescriptorPool pool,
                                          VkDescriptorSetLayout layout, const vector<VkDescriptorType> &dtypes,
                                          const vector<VkDescriptorBufferInfo> &descriptorInfos,
                                          const vector<VkDescriptorImageInfo> &descriptorImageInfos)
{
    DE_ASSERT(dtypes.size() == descriptorInfos.size() + descriptorImageInfos.size());

    const VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, DE_NULL, pool, 1u,
                                                   &layout};

    Move<VkDescriptorSet> descriptorSet = allocateDescriptorSet(vkdi, device, &allocInfo);
    DescriptorSetUpdateBuilder builder;

    uint32_t bufferNdx = 0u;
    uint32_t imageNdx  = 0u;

    for (uint32_t descriptorNdx = 0; descriptorNdx < dtypes.size(); ++descriptorNdx)
    {
        switch (dtypes[descriptorNdx])
        {
        // Write buffer descriptor
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(descriptorNdx),
                                dtypes[descriptorNdx], &descriptorInfos[bufferNdx++]);
            break;

        // Write image/sampler descriptor
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(descriptorNdx),
                                dtypes[descriptorNdx], &descriptorImageInfos[imageNdx++]);
            break;

        default:
            DE_FATAL("Not implemented");
        }
    }
    builder.update(vkdi, device);

    return descriptorSet;
}

/*--------------------------------------------------------------------*//*!
 * \brief Create a compute pipeline based on the given shader
 *//*--------------------------------------------------------------------*/
Move<VkPipeline> createComputePipeline(const DeviceInterface &vkdi, const VkDevice &device,
                                       VkPipelineLayout pipelineLayout, VkShaderModule shader, const char *entryPoint,
                                       const vkt::SpirVAssembly::SpecConstants &specConstants)
{
    const uint32_t numSpecConstants = (uint32_t)specConstants.getValuesCount();
    vector<VkSpecializationMapEntry> entries;
    VkSpecializationInfo specInfo;
    size_t offset = 0;

    if (numSpecConstants != 0)
    {
        entries.resize(numSpecConstants);

        for (uint32_t ndx = 0; ndx < numSpecConstants; ++ndx)
        {
            const size_t valueSize = specConstants.getValueSize(ndx);

            entries[ndx].constantID = ndx;
            entries[ndx].offset     = static_cast<uint32_t>(offset);
            entries[ndx].size       = valueSize;

            offset += valueSize;
        }

        specInfo.mapEntryCount = numSpecConstants;
        specInfo.pMapEntries   = &entries[0];
        specInfo.dataSize      = offset;
        specInfo.pData         = specConstants.getValuesBuffer();
    }

    const VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // sType
        DE_NULL,                                             // pNext
        (VkPipelineShaderStageCreateFlags)0,                 // flags
        VK_SHADER_STAGE_COMPUTE_BIT,                         // stage
        shader,                                              // module
        entryPoint,                                          // pName
        (numSpecConstants == 0) ? DE_NULL : &specInfo,       // pSpecializationInfo
    };
    const VkComputePipelineCreateInfo pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // sType
        DE_NULL,                                        // pNext
        (VkPipelineCreateFlags)0,
        pipelineShaderStageCreateInfo, // cs
        pipelineLayout,                // layout
        VK_NULL_HANDLE,                // basePipelineHandle
        0u,                            // basePipelineIndex
    };

    return createComputePipeline(vkdi, device, VK_NULL_HANDLE, &pipelineCreateInfo);
}

} // namespace

namespace vkt
{
namespace SpirVAssembly
{

// ComputeShaderTestCase implementations

SpvAsmComputeShaderCase::SpvAsmComputeShaderCase(tcu::TestContext &testCtx, const char *name,
                                                 const ComputeShaderSpec &spec)
    : TestCase(testCtx, name)
    , m_shaderSpec(spec)
{
}

void SpvAsmComputeShaderCase::checkSupport(Context &context) const
{
    if (getMinRequiredVulkanVersion(m_shaderSpec.spirvVersion) > context.getUsedApiVersion())
    {
        TCU_THROW(NotSupportedError, std::string("Vulkan higher than or equal to " +
                                                 getVulkanName(getMinRequiredVulkanVersion(m_shaderSpec.spirvVersion)) +
                                                 " is required for this test to run")
                                         .c_str());
    }

    // Check all required extensions are supported
    for (const auto &ext : m_shaderSpec.extensions)
        context.requireDeviceFunctionality(ext);

    // Core features
    // Check that we're not skipping tests needlessly based on things that don't affect compute.
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.fullDrawIndexUint32 == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.independentBlend == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.geometryShader == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.tessellationShader == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.sampleRateShading == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.dualSrcBlend == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.logicOp == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.multiDrawIndirect == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.drawIndirectFirstInstance == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.depthClamp == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.depthBiasClamp == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.fillModeNonSolid == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.depthBounds == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.wideLines == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.largePoints == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.alphaToOne == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.multiViewport == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.occlusionQueryPrecise == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.vertexPipelineStoresAndAtomics == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.fragmentStoresAndAtomics == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.shaderTessellationAndGeometryPointSize == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.shaderClipDistance == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.shaderCullDistance == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.sparseBinding == false);
    assert(m_shaderSpec.requestedVulkanFeatures.coreFeatures.variableMultisampleRate == false);

    const char *unsupportedFeature = DE_NULL;
    if (!isVulkanFeaturesSupported(context, m_shaderSpec.requestedVulkanFeatures, &unsupportedFeature))
        TCU_THROW(NotSupportedError,
                  std::string("At least following requested feature is not supported: ") + unsupportedFeature);

    // Extension features
    if (m_shaderSpec.usesPhysStorageBuffer && !context.isBufferDeviceAddressSupported())
        TCU_THROW(NotSupportedError, "Request physical storage buffer feature not supported");
}

void SpvAsmComputeShaderCase::initPrograms(SourceCollections &programCollection) const
{
    const auto &extensions  = m_shaderSpec.extensions;
    const bool allowSpirv14 = (std::find(extensions.begin(), extensions.end(), "VK_KHR_spirv_1_4") != extensions.end());
    const bool allowMaintenance4 =
        (std::find(extensions.begin(), extensions.end(), "VK_KHR_maintenance4") != extensions.end());

    programCollection.spirvAsmSources.add("compute")
        << m_shaderSpec.assembly.c_str()
        << SpirVAsmBuildOptions(programCollection.usedVulkanVersion, m_shaderSpec.spirvVersion, allowSpirv14,
                                allowMaintenance4);
}

TestInstance *SpvAsmComputeShaderCase::createInstance(Context &ctx) const
{
    return new SpvAsmComputeShaderInstance(ctx, m_shaderSpec);
}

// ComputeShaderTestInstance implementations

SpvAsmComputeShaderInstance::SpvAsmComputeShaderInstance(Context &ctx, const ComputeShaderSpec &spec)
    : TestInstance(ctx)
    , m_shaderSpec(spec)
{
}

VkImageUsageFlags getMatchingComputeImageUsageFlags(VkDescriptorType dType)
{
    switch (dType)
    {
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        return VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    default:
        DE_FATAL("Not implemented");
    }
    return (VkImageUsageFlags)0;
}

tcu::TestStatus SpvAsmComputeShaderInstance::iterate(void)
{
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    const VkDevice &device          = m_context.getDevice();
    const DeviceInterface &vkdi     = m_context.getDeviceInterface();
    Allocator &allocator            = m_context.getDefaultAllocator();
    const VkQueue queue             = m_context.getUniversalQueue();

    vector<AllocationSp> inputAllocs;
    vector<AllocationSp> outputAllocs;
    vector<BufferHandleSp> inputBuffers;
    vector<ImageHandleSp> inputImages;
    vector<ImageViewHandleSp> inputImageViews;
    vector<SamplerHandleSp> inputSamplers;
    vector<BufferHandleSp> outputBuffers;
    vector<VkDescriptorBufferInfo> descriptorInfos;
    vector<VkDescriptorImageInfo> descriptorImageInfos;
    vector<VkDescriptorType> descriptorTypes;

    DE_ASSERT(!m_shaderSpec.outputs.empty());

    // Create command pool and command buffer

    const Unique<VkCommandPool> cmdPool(
        createCommandPool(vkdi, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
    Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vkdi, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Create buffer and image objects, allocate storage, and create view for all input/output buffers and images.

    for (uint32_t inputNdx = 0; inputNdx < m_shaderSpec.inputs.size(); ++inputNdx)
    {
        const VkDescriptorType descType = m_shaderSpec.inputs[inputNdx].getDescriptorType();

        const bool hasImage = (descType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) ||
                              (descType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) ||
                              (descType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        const bool hasSampler = (descType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) ||
                                (descType == VK_DESCRIPTOR_TYPE_SAMPLER) ||
                                (descType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        descriptorTypes.push_back(descType);

        // Buffer
        if (!hasImage && !hasSampler)
        {
            const BufferSp &input = m_shaderSpec.inputs[inputNdx].getBuffer();
            vector<uint8_t> inputBytes;

            input->getBytes(inputBytes);

            const size_t numBytes = inputBytes.size();

            AllocationMp bufferAlloc;
            BufferHandleUp *buffer = new BufferHandleUp(
                createBufferAndBindMemory(m_context, vkdi, device, descType, allocator, numBytes, &bufferAlloc,
                                          m_shaderSpec.usesPhysStorageBuffer, m_shaderSpec.coherentMemory));

            setMemory(vkdi, device, &*bufferAlloc, numBytes, &inputBytes.front(), m_shaderSpec.coherentMemory);
            inputBuffers.push_back(BufferHandleSp(buffer));
            inputAllocs.push_back(de::SharedPtr<Allocation>(bufferAlloc.release()));
        }
        // Image
        else if (hasImage)
        {
            const BufferSp &input = m_shaderSpec.inputs[inputNdx].getBuffer();
            vector<uint8_t> inputBytes;

            input->getBytes(inputBytes);

            const size_t numBytes = inputBytes.size();

            AllocationMp bufferAlloc;
            BufferHandleUp *buffer =
                new BufferHandleUp(createBufferAndBindMemory(m_context, vkdi, device, descType, allocator, numBytes,
                                                             &bufferAlloc, m_shaderSpec.usesPhysStorageBuffer));

            AllocationMp imageAlloc;
            ImageHandleUp *image = new ImageHandleUp(createImageAndBindMemory(
                vkdi, device, descType, m_shaderSpec.inputFormat, allocator, queueFamilyIndex, &imageAlloc));

            setMemory(vkdi, device, &*bufferAlloc, numBytes, &inputBytes.front());

            inputBuffers.push_back(BufferHandleSp(buffer));
            inputAllocs.push_back(de::SharedPtr<Allocation>(bufferAlloc.release()));

            inputImages.push_back(ImageHandleSp(image));
            inputAllocs.push_back(de::SharedPtr<Allocation>(imageAlloc.release()));

            const VkImageLayout imageLayout    = (descType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) ?
                                                     VK_IMAGE_LAYOUT_GENERAL :
                                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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
                {8, 8, 1}                      // VkExtent3D imageExtent;
            };
            vector<VkBufferImageCopy> copyRegions;
            copyRegions.push_back(copyRegion);

            copyBufferToImage(vkdi, device, queue, queueFamilyIndex, buffer->get(), (uint32_t)numBytes, copyRegions,
                              DE_NULL, VK_IMAGE_ASPECT_COLOR_BIT, 1u, 1u, image->get(), imageLayout);
        }
    }

    uint32_t imageNdx  = 0u;
    uint32_t bufferNdx = 0u;

    for (uint32_t inputNdx = 0; inputNdx < descriptorTypes.size(); ++inputNdx)
    {
        const VkDescriptorType descType = descriptorTypes[inputNdx];

        const bool hasImage = (descType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) ||
                              (descType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) ||
                              (descType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        const bool hasSampler = (descType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) ||
                                (descType == VK_DESCRIPTOR_TYPE_SAMPLER) ||
                                (descType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        // Create image view and sampler
        if (hasImage)
        {
            const VkImageViewCreateInfo imgViewParams = {
                VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
                DE_NULL,                                  // const void* pNext;
                0u,                                       // VkImageViewCreateFlags flags;
                **inputImages[imageNdx++],                // VkImage image;
                VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
                m_shaderSpec.inputFormat,                 // VkFormat format;
                {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
                 VK_COMPONENT_SWIZZLE_A}, // VkChannelMapping channels;
                {
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0u,                        // uint32_t baseMipLevel;
                    1u,                        // uint32_t mipLevels;
                    0u,                        // uint32_t baseArrayLayer;
                    1u,                        // uint32_t arraySize;
                },                             // VkImageSubresourceRange subresourceRange;
            };

            Move<VkImageView> imgView(createImageView(vkdi, device, &imgViewParams));
            inputImageViews.push_back(ImageViewHandleSp(new ImageViewHandleUp(imgView)));
        }

        if (hasSampler)
        {
            const VkSamplerCreateInfo samplerParams = {
                VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, // VkStructureType sType;
                DE_NULL,                               // const void* pNext;
                0,                                     // VkSamplerCreateFlags flags;
                VK_FILTER_NEAREST,                     // VkFilter                    magFilter:
                VK_FILTER_NEAREST,                     // VkFilter minFilter;
                VK_SAMPLER_MIPMAP_MODE_NEAREST,        // VkSamplerMipmapMode mipmapMode;
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, // VkSamplerAddressMode addressModeU;
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, // VkSamplerAddressMode addressModeV;
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, // VkSamplerAddressMode addressModeW;
                0.0f,                                  // float mipLodBias;
                VK_FALSE,                              // VkBool32 anistoropyEnable;
                1.0f,                                  // float maxAnisotropy;
                VK_FALSE,                              // VkBool32 compareEnable;
                VK_COMPARE_OP_ALWAYS,                  // VkCompareOp compareOp;
                0.0f,                                  // float minLod;
                0.0f,                                  // float maxLod;
                VK_BORDER_COLOR_INT_OPAQUE_BLACK,      // VkBorderColor borderColor;
                VK_FALSE                               // VkBool32 unnormalizedCoordinates;
            };

            Move<VkSampler> sampler(createSampler(vkdi, device, &samplerParams));
            inputSamplers.push_back(SamplerHandleSp(new SamplerHandleUp(sampler)));
        }

        // Create descriptor buffer and image infos
        switch (descType)
        {
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        {
            const VkDescriptorBufferInfo bufInfo = {
                **inputBuffers[bufferNdx++], // VkBuffer buffer;
                0,                           // VkDeviceSize offset;
                VK_WHOLE_SIZE,               // VkDeviceSize size;
            };

            descriptorInfos.push_back(bufInfo);
            break;
        }

        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        {
            const VkDescriptorImageInfo imgInfo = {
                VK_NULL_HANDLE,           // VkSampler sampler;
                **inputImageViews.back(), // VkImageView imageView;
                VK_IMAGE_LAYOUT_GENERAL   // VkImageLayout imageLayout;
            };

            descriptorImageInfos.push_back(imgInfo);
            break;
        }

        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        {
            const VkDescriptorImageInfo imgInfo = {
                VK_NULL_HANDLE,                          // VkSampler sampler;
                **inputImageViews.back(),                // VkImageView imageView;
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL // VkImageLayout imageLayout;
            };

            descriptorImageInfos.push_back(imgInfo);
            break;
        }

        case VK_DESCRIPTOR_TYPE_SAMPLER:
        {
            const VkDescriptorImageInfo imgInfo = {
                **inputSamplers.back(), // VkSampler sampler;
                VK_NULL_HANDLE,         // VkImageView imageView;
                VK_IMAGE_LAYOUT_GENERAL // VkImageLayout imageLayout;
            };

            descriptorImageInfos.push_back(imgInfo);
            break;
        }

        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        {
            const VkDescriptorImageInfo imgInfo = {
                **inputSamplers.back(),                  // VkSampler sampler;
                **inputImageViews.back(),                // VkImageView imageView;
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL // VkImageLayout imageLayout;
            };

            descriptorImageInfos.push_back(imgInfo);
            break;
        }

        default:
            DE_FATAL("Not implemented");
        }
    }

    for (uint32_t outputNdx = 0; outputNdx < m_shaderSpec.outputs.size(); ++outputNdx)
    {
        DE_ASSERT(m_shaderSpec.outputs[outputNdx].getDescriptorType() == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

        descriptorTypes.push_back(m_shaderSpec.outputs[outputNdx].getDescriptorType());

        AllocationMp alloc;
        const BufferSp &output = m_shaderSpec.outputs[outputNdx].getBuffer();
        vector<uint8_t> outputBytes;

        output->getBytes(outputBytes);

        const size_t numBytes  = outputBytes.size();
        BufferHandleUp *buffer = new BufferHandleUp(
            createBufferAndBindMemory(m_context, vkdi, device, descriptorTypes.back(), allocator, numBytes, &alloc,
                                      m_shaderSpec.usesPhysStorageBuffer, m_shaderSpec.coherentMemory));

        fillMemoryWithValue(vkdi, device, &*alloc, numBytes, 0xff, m_shaderSpec.coherentMemory);
        descriptorInfos.push_back(vk::makeDescriptorBufferInfo(**buffer, 0u, numBytes));
        outputBuffers.push_back(BufferHandleSp(buffer));
        outputAllocs.push_back(de::SharedPtr<Allocation>(alloc.release()));
    }

    std::vector<VkDeviceAddress> gpuAddrs;
    // Query the buffer device addresses, write them into a new buffer, and replace
    // all the descriptors with just a desciptor to this new buffer.
    if (m_shaderSpec.usesPhysStorageBuffer)
    {
        VkBufferDeviceAddressInfo info{
            VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, // VkStructureType sType;
            DE_NULL,                                      // const void* pNext;
            VK_NULL_HANDLE,                               // VkBuffer            buffer
        };

        for (uint32_t inputNdx = 0; inputNdx < m_shaderSpec.inputs.size(); ++inputNdx)
        {
            info.buffer          = **inputBuffers[inputNdx];
            VkDeviceAddress addr = vkdi.getBufferDeviceAddress(device, &info);

            gpuAddrs.push_back(addr);
        }
        for (uint32_t outputNdx = 0; outputNdx < m_shaderSpec.outputs.size(); ++outputNdx)
        {
            info.buffer          = **outputBuffers[outputNdx];
            VkDeviceAddress addr = vkdi.getBufferDeviceAddress(device, &info);

            gpuAddrs.push_back(addr);
        }

        descriptorInfos.clear();
        descriptorTypes.clear();
        descriptorTypes.push_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        const size_t numBytes = gpuAddrs.size() * sizeof(VkDeviceAddress);

        AllocationMp bufferAlloc;
        BufferHandleUp *buffer = new BufferHandleUp(
            createBufferAndBindMemory(m_context, vkdi, device, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allocator, numBytes,
                                      &bufferAlloc, false, m_shaderSpec.coherentMemory));

        setMemory(vkdi, device, &*bufferAlloc, numBytes, &gpuAddrs.front(), m_shaderSpec.coherentMemory);
        inputBuffers.push_back(BufferHandleSp(buffer));
        inputAllocs.push_back(de::SharedPtr<Allocation>(bufferAlloc.release()));

        descriptorInfos.push_back(vk::makeDescriptorBufferInfo(**buffer, 0u, numBytes));
    }

    // Create layouts and descriptor set.

    Unique<VkDescriptorSetLayout> descriptorSetLayout(createDescriptorSetLayout(vkdi, device, descriptorTypes));
    Unique<VkPipelineLayout> pipelineLayout(
        createPipelineLayout(vkdi, device, *descriptorSetLayout, m_shaderSpec.pushConstants));
    Unique<VkDescriptorPool> descriptorPool(createDescriptorPool(vkdi, device, descriptorTypes));
    Unique<VkDescriptorSet> descriptorSet(createDescriptorSet(vkdi, device, *descriptorPool, *descriptorSetLayout,
                                                              descriptorTypes, descriptorInfos, descriptorImageInfos));

    // Create compute shader and pipeline.

    const ProgramBinary &binary = m_context.getBinaryCollection().get("compute");
    if (m_shaderSpec.verifyBinary && !m_shaderSpec.verifyBinary(binary))
    {
        return tcu::TestStatus::fail("Binary verification of SPIR-V in the test failed");
    }
    Unique<VkShaderModule> module(createShaderModule(vkdi, device, binary, (VkShaderModuleCreateFlags)0u));

    Unique<VkPipeline> computePipeline(createComputePipeline(
        vkdi, device, *pipelineLayout, *module, m_shaderSpec.entryPoint.c_str(), m_shaderSpec.specConstants));

    // Create command buffer and record commands

    const tcu::IVec3 &numWorkGroups = m_shaderSpec.numWorkGroups;

    beginCommandBuffer(vkdi, *cmdBuffer);
    vkdi.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
    vkdi.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1, &descriptorSet.get(),
                               0, DE_NULL);
    if (m_shaderSpec.pushConstants)
    {
        vector<uint8_t> pushConstantsBytes;
        m_shaderSpec.pushConstants->getBytes(pushConstantsBytes);

        const uint32_t size = static_cast<uint32_t>(pushConstantsBytes.size());
        const void *data    = &pushConstantsBytes.front();

        vkdi.cmdPushConstants(*cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, /* offset = */ 0,
                              /* size = */ size, data);
    }
    vkdi.cmdDispatch(*cmdBuffer, numWorkGroups.x(), numWorkGroups.y(), numWorkGroups.z());

    // Insert a barrier so data written by the shader is available to the host
    for (uint32_t outputBufferNdx = 0; outputBufferNdx < outputBuffers.size(); ++outputBufferNdx)
    {
        const VkBufferMemoryBarrier buf_barrier = {
            VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, //    VkStructureType    sType;
            DE_NULL,                                 //    const void*        pNext;
            VK_ACCESS_SHADER_WRITE_BIT,              //    VkAccessFlags      srcAccessMask;
            VK_ACCESS_HOST_READ_BIT,                 //    VkAccessFlags      dstAccessMask;
            VK_QUEUE_FAMILY_IGNORED,                 //    uint32_t           srcQueueFamilyIndex;
            VK_QUEUE_FAMILY_IGNORED,                 //    uint32_t           dstQueueFamilyIndex;
            **outputBuffers[outputBufferNdx],        //    VkBuffer           buffer;
            0,                                       //    VkDeviceSize       offset;
            VK_WHOLE_SIZE                            //    VkDeviceSize       size;
        };

        vkdi.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0,
                                DE_NULL, 1, &buf_barrier, 0, DE_NULL);
    }
    endCommandBuffer(vkdi, *cmdBuffer);

    submitCommandsAndWait(vkdi, device, queue, *cmdBuffer);
    m_context.resetCommandPoolForVKSC(device, *cmdPool);

    // Invalidate output memory ranges before checking on host.
    for (size_t outputNdx = 0; outputNdx < m_shaderSpec.outputs.size(); ++outputNdx)
    {
        invalidateMemory(vkdi, device, outputAllocs[outputNdx].get(), m_shaderSpec.coherentMemory);
    }

    // Check output.
    if (m_shaderSpec.verifyIO)
    {
        if (!(*m_shaderSpec.verifyIO)(m_shaderSpec.inputs, outputAllocs, m_shaderSpec.outputs,
                                      m_context.getTestContext().getLog()))
            return tcu::TestStatus(m_shaderSpec.failResult, m_shaderSpec.failMessage);
    }
    else
    {
        for (size_t outputNdx = 0; outputNdx < m_shaderSpec.outputs.size(); ++outputNdx)
        {
            const BufferSp &expectedOutput = m_shaderSpec.outputs[outputNdx].getBuffer();
            vector<uint8_t> expectedBytes;

            expectedOutput->getBytes(expectedBytes);

            if (deMemCmp(&expectedBytes.front(), outputAllocs[outputNdx]->getHostPtr(), expectedBytes.size()))
            {
                const size_t errorsMax     = 16u;
                const uint8_t *ptrHost     = static_cast<uint8_t *>(outputAllocs[outputNdx]->getHostPtr());
                const uint8_t *ptrExpected = static_cast<uint8_t *>(&expectedBytes.front());
                size_t errors              = 0u;
                size_t ndx                 = 0u;

                for (; ndx < expectedBytes.size(); ++ndx)
                {
                    if (ptrHost[ndx] != ptrExpected[ndx])
                        break;
                }

                for (; ndx < expectedBytes.size(); ++ndx)
                {
                    if (ptrHost[ndx] != ptrExpected[ndx])
                    {
                        m_context.getTestContext().getLog()
                            << tcu::TestLog::Message << "OutputBuffer:" << outputNdx
                            << " got:" << ((uint32_t)ptrHost[ndx]) << " expected:" << ((uint32_t)ptrExpected[ndx])
                            << " at byte " << ndx << tcu::TestLog::EndMessage;
                        errors++;

                        if (errors >= errorsMax)
                        {
                            m_context.getTestContext().getLog()
                                << tcu::TestLog::Message << "Maximum error count reached (" << errors
                                << "). Stop output." << tcu::TestLog::EndMessage;
                            break;
                        }
                    }
                }

                return tcu::TestStatus(m_shaderSpec.failResult, m_shaderSpec.failMessage);
            }
        }
    }

    return tcu::TestStatus::pass("Output match with expected");
}

} // namespace SpirVAssembly
} // namespace vkt
