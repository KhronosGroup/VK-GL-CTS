/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
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
 * \file  vktSparseResourcesMultisampledImageSparseBinding.cpp
 * \brief Sparse resources multisampled image sparse binding tests
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesMultisampledImageSparseBinding.hpp"
#include "vktSparseResourcesTestsUtil.hpp"
#include "vktSparseResourcesBase.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBuilderUtil.hpp"

using namespace vk;

namespace vkt
{
namespace sparse
{
namespace
{

typedef de::MovePtr<vk::Allocation> AllocationMp;

uint32_t calculateBufferSize(tcu::UVec3 imgSize)
{
    return imgSize.x() * imgSize.y() * imgSize.z() * 4; // Format is R32_UINT
}

uint32_t getElemCount(tcu::UVec3 imgSize)
{
    return imgSize.x() * imgSize.y() * imgSize.z();
}

VkExtent3D get3DExtent(tcu::UVec3 imgSize)
{
    const VkExtent3D extent = {
        imgSize.x(), // uint32_t    width;
        imgSize.y(), // uint32_t    height;
        imgSize.z(), // uint32_t    depth;
    };

    return extent;
}

VkOffset3D get3DOffset(tcu::IVec3 offset)
{
    const VkOffset3D vkOffset = {
        offset.x(), // uint32_t    width;
        offset.y(), // uint32_t    height;
        offset.z(), // uint32_t    depth;
    };

    return vkOffset;
}

VkImageSubresourceRange getImageSRR()
{
    VkImageSubresourceRange range = {
        VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask
        0u,                        // uint32_t           baseMipLevel
        1u,                        // uint32_t           levelCount
        0u,                        // uint32_t           baseArrayLayer
        1u,                        // uint32_t           layerCount
    };

    return range;
}

VkImageSubresourceLayers getImageSRL()
{
    VkImageSubresourceLayers layers = {
        VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask
        0u,                        // uint32_t           baseMipLevel
        0u,                        // uint32_t           baseArrayLayer
        1u,                        // uint32_t           layerCount
    };

    return layers;
}

const char *getFormatPrefix(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R32_SFLOAT:
        return "";
    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R32_UINT:
        return "u";
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R32_SINT:
        return "i";
    default:
        break;
    }

    return "";
}

struct TestParams
{
    VkFormat format;
    VkSampleCountFlagBits sampleCount;
    tcu::UVec3 imgSize;
};

class MultisampledImageSparseBindingCase : public TestCase
{
public:
    MultisampledImageSparseBindingCase(tcu::TestContext &testCtx, const std::string &name, const TestParams params);

    void checkSupport(Context &context) const;
    void initPrograms(vk::SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const;

private:
    const TestParams m_params;
};

MultisampledImageSparseBindingCase::MultisampledImageSparseBindingCase(tcu::TestContext &testCtx,
                                                                       const std::string &name, const TestParams params)
    : TestCase(testCtx, name)
    , m_params(params)
{
}

void MultisampledImageSparseBindingCase::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);

    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    if (!isImageSizeSupported(vki, physicalDevice, IMAGE_TYPE_2D, m_params.imgSize))
        TCU_THROW(NotSupportedError, "Image size not supported for device");

    VkImageFormatProperties imageFormatProperties;
    const VkResult imageFormatResult = vki.getPhysicalDeviceImageFormatProperties(
        physicalDevice, m_params.format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT,
        (VkImageCreateFlags)0, &imageFormatProperties);

    if (imageFormatResult == VK_ERROR_FORMAT_NOT_SUPPORTED)
        TCU_THROW(NotSupportedError, "Format is not supported");

    if ((imageFormatProperties.sampleCounts & m_params.sampleCount) != m_params.sampleCount)
        TCU_THROW(NotSupportedError, "Requested sample count is not supported");
}

void MultisampledImageSparseBindingCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string glslStr("");
    glslStr += "#version 450\n"
               "\n"
               "layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
               "\n"
               "layout (set = 0, binding = 0, " +
               getImageFormatID(m_params.format) + ") uniform " + getFormatPrefix(m_params.format) +
               "image2DMS u_msImage;\n"
               "layout (set = 0, binding = 1, r32ui)  writeonly uniform uimage2D  u_resultImage;\n"
               "\n"
               "void main (void)\n"
               "{\n"
               "    int gx = int(gl_GlobalInvocationID.x);\n"
               "    int gy = int(gl_GlobalInvocationID.y);\n"
               "    int gz = int(gl_GlobalInvocationID.z);\n"
               "\n"
               "    imageStore(u_msImage, ivec2(gx, gy), 0," +
               getFormatPrefix(m_params.format) + "vec4(" + std::to_string(m_params.sampleCount) +
               "));\n"
               "    " +
               getFormatPrefix(m_params.format) +
               "vec4 color = imageLoad(u_msImage, ivec2(gx, gy), 0);\n"
               "    imageStore(u_resultImage, ivec2(gx, gy), uvec4(color));\n"
               "}\n";

    programCollection.glslSources.add("compute") << glu::ComputeSource(glslStr);
}

class MultisampledImageSparseBindingInstance : public SparseResourcesBaseInstance
{
public:
    MultisampledImageSparseBindingInstance(Context &context, const TestParams params);

    tcu::TestStatus iterate(void);

private:
    Move<VkBuffer> createBufferAndBindMemory(AllocationMp *outMemory);
    Move<VkImage> createImageAndBindMemory(tcu::UVec3 imgSize, VkFormat format, AllocationMp *outMemory);
    Move<VkImageView> createImageView(VkFormat format, VkImage image);
    Move<VkDescriptorSetLayout> createDescriptorSetLayout();
    Move<VkPipelineLayout> createPipelineLayout(VkDescriptorSetLayout descriptorSetLayout);
    Move<VkDescriptorPool> createDescriptorPool();
    Move<VkDescriptorSet> createDescriptorSet(VkDescriptorPool descriptorPool,
                                              VkDescriptorSetLayout descriptorSetLayout,
                                              const VkDescriptorImageInfo &msImgInfo,
                                              const VkDescriptorImageInfo &resultImgInfo);

private:
    const TestParams m_params;
};

MultisampledImageSparseBindingInstance::MultisampledImageSparseBindingInstance(Context &context,
                                                                               const TestParams params)
    : SparseResourcesBaseInstance(context, false)
    , m_params(params)
{
}

tcu::TestStatus MultisampledImageSparseBindingInstance::iterate(void)
{
    const InstanceInterface &instance = m_context.getInstanceInterface();

    {
        // Create logical device supporting both sparse and compute queues
        QueueRequirementsVec queueRequirements;
        queueRequirements.push_back(QueueRequirements(VK_QUEUE_SPARSE_BINDING_BIT, 1u));
        queueRequirements.push_back(QueueRequirements(VK_QUEUE_COMPUTE_BIT, 1u));

        createDeviceSupportingQueues(queueRequirements);
    }

    const VkPhysicalDevice physicalDevice = getPhysicalDevice();
    std::vector<DeviceMemorySp> deviceMemUniquePtrVec;

    const DeviceInterface &deviceInterface = getDeviceInterface();
    const Queue &sparseQueue               = getQueue(VK_QUEUE_SPARSE_BINDING_BIT, 0);
    const Queue &computeQueue              = getQueue(VK_QUEUE_COMPUTE_BIT, 0);

    // Sparse multisampled image create info
    VkImageCreateInfo imageSparseInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType          sType
        nullptr,                             // const void*              pNext
        VK_IMAGE_CREATE_SPARSE_BINDING_BIT,  // VkImageCreateFlags       flags
        VK_IMAGE_TYPE_2D,                    // VkImageType              imageType
        m_params.format,                     // VkFormat                 format
        makeExtent3D(m_params.imgSize),      // VkExtent3D               extent
        1u,                                  // uint32_t                 mipLevels
        1u,                                  // uint32_t                 arrayLayers
        m_params.sampleCount,                // VkSampleCountFlagBits    samples
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling            tiling
        VK_IMAGE_USAGE_STORAGE_BIT,          // VkImageUsageFlags        usage
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode            sharingMode
        0u,                                  // uint32_t                 queueFamilyIndexCount
        nullptr,                             // const uint32_t*          pQueueFamilyIndices
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout            initialLayout
    };

    {
        VkImageFormatProperties imageFormatProperties;
        if (instance.getPhysicalDeviceImageFormatProperties(
                physicalDevice, imageSparseInfo.format, imageSparseInfo.imageType, imageSparseInfo.tiling,
                imageSparseInfo.usage, imageSparseInfo.flags, &imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
        {
            TCU_THROW(NotSupportedError, "Image format does not support sparse binding operations");
        }
    }

    // Create sparse image
    const Move<VkImage> msSparseImage(createImage(deviceInterface, getDevice(), &imageSparseInfo));

    // Sparse binding semaphore
    const Move<VkSemaphore> sparseBindSemaphore(createSemaphore(deviceInterface, getDevice()));

    // Get sparse Memory requirements
    const VkMemoryRequirements sparseMemRequirements =
        getImageMemoryRequirements(deviceInterface, getDevice(), msSparseImage.get());

    // Check if required image memory size does not exceed device limits
    if (sparseMemRequirements.size >
        getPhysicalDeviceProperties(instance, getPhysicalDevice()).limits.sparseAddressSpaceSize)
        TCU_THROW(NotSupportedError, "Required memory size for sparse resource exceeds device limits");

    DE_ASSERT((sparseMemRequirements.size % sparseMemRequirements.alignment) == 0);

    // Binding sparse image
    {
        std::vector<VkSparseMemoryBind> sparseMemoryBinds;
        const uint32_t numSparseBinds =
            static_cast<uint32_t>(sparseMemRequirements.size / sparseMemRequirements.alignment);
        const uint32_t memoryType =
            findMatchingMemoryType(instance, getPhysicalDevice(), sparseMemRequirements, MemoryRequirement::Any);

        if (memoryType == NO_MATCH_FOUND)
            return tcu::TestStatus::fail("No matching memory type found");

        for (uint32_t sparseBindNdx = 0; sparseBindNdx < numSparseBinds; ++sparseBindNdx)
        {
            const VkSparseMemoryBind sparseMemoryBind =
                makeSparseMemoryBind(deviceInterface, getDevice(), sparseMemRequirements.alignment, memoryType,
                                     sparseMemRequirements.alignment * sparseBindNdx);

            deviceMemUniquePtrVec.push_back(
                makeVkSharedPtr(Move<VkDeviceMemory>(check<VkDeviceMemory>(sparseMemoryBind.memory),
                                                     Deleter<VkDeviceMemory>(deviceInterface, getDevice(), nullptr))));

            sparseMemoryBinds.push_back(sparseMemoryBind);
        }

        const VkSparseImageOpaqueMemoryBindInfo opaqueBindInfo = makeSparseImageOpaqueMemoryBindInfo(
            msSparseImage.get(), static_cast<uint32_t>(sparseMemoryBinds.size()), sparseMemoryBinds.data());

        const VkBindSparseInfo bindSparseInfo = {
            VK_STRUCTURE_TYPE_BIND_SPARSE_INFO, //VkStructureType    sType;
            nullptr,                            //const void*        pNext;
            0u,                                 //uint32_t           waitSemaphoreCount;
            nullptr,                            //const VkSemaphore* pWaitSemaphores;
            0u,                                 //uint32_t           bufferBindCount;
            nullptr,                            //const VkSparseBufferMemoryBindInfo*      pBufferBinds;
            1u,                                 //uint32_t                                 imageOpaqueBindCount;
            &opaqueBindInfo,                    //const VkSparseImageOpaqueMemoryBindInfo* pImageOpaqueBinds;
            0u,                                 //uint32_t                                 imageBindCount;
            nullptr,                            //const VkSparseImageMemoryBindInfo*       pImageBinds;
            1u,                                 //uint32_t                                 signalSemaphoreCount;
            &sparseBindSemaphore.get()          //const VkSemaphore*                       pSignalSemaphores;
        };

        VK_CHECK(deviceInterface.queueBindSparse(sparseQueue.queueHandle, 1u, &bindSparseInfo, VK_NULL_HANDLE));

        // Create sparse command pool and command buffer for empty submission
        const Move<VkCommandPool> sparseCmdPool(
            makeCommandPool(deviceInterface, getDevice(), sparseQueue.queueFamilyIndex));
        const Move<VkCommandBuffer> sparseCmdBuffer(
            allocateCommandBuffer(deviceInterface, getDevice(), sparseCmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY));
        // Empty cmd buffer for submission
        beginCommandBuffer(deviceInterface, sparseCmdBuffer.get());
        endCommandBuffer(deviceInterface, sparseCmdBuffer.get());
        const VkPipelineStageFlags stageBits[] = {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT};
        submitCommandsAndWait(deviceInterface, getDevice(), sparseQueue.queueHandle, sparseCmdBuffer.get(), 1u,
                              &sparseBindSemaphore.get(), stageBits);
        deviceInterface.queueWaitIdle(sparseQueue.queueHandle);
    }

    // Create sparse image view
    const Move<VkImageView> msSparseImageView(createImageView(m_params.format, msSparseImage.get()));

    // Create "resolve" image
    AllocationMp resultImageMemory;
    const Move<VkImage> resultImage(createImageAndBindMemory(m_params.imgSize, VK_FORMAT_R32_UINT, &resultImageMemory));

    // Create result image view
    const Move<VkImageView> resultImageView(createImageView(VK_FORMAT_R32_UINT, resultImage.get()));

    // Create result buffer
    AllocationMp resultBufferMemory;
    const Move<VkBuffer> resultBuffer(createBufferAndBindMemory(&resultBufferMemory));

    // Create descriptor set layout
    const Move<VkDescriptorSetLayout> setLayout(createDescriptorSetLayout());

    // Create pipeline layout
    const Move<VkPipelineLayout> pipelineLayout(createPipelineLayout(setLayout.get()));

    // Create descriptor pool
    const Move<VkDescriptorPool> descriptorPool(createDescriptorPool());

    // Create and write descriptor set
    VkDescriptorImageInfo msImgInfo = {
        VK_NULL_HANDLE,          // VkSampler        sampler
        msSparseImageView.get(), // VkImageView      imageView
        VK_IMAGE_LAYOUT_GENERAL, // VkImageLayout    imageLayout
    };

    VkDescriptorImageInfo resultImgInfo = {
        VK_NULL_HANDLE,          // VkSampler        sampler
        resultImageView.get(),   // VkImageView      imageView
        VK_IMAGE_LAYOUT_GENERAL, // VkImageLayout    imageLayout
    };

    const Move<VkDescriptorSet> descriptorSet(
        createDescriptorSet(descriptorPool.get(), setLayout.get(), msImgInfo, resultImgInfo));

    // Create compute pipeline
    vk::BinaryCollection &binCollection = m_context.getBinaryCollection();
    Move<VkShaderModule> computeModule(createShaderModule(deviceInterface, getDevice(), binCollection.get("compute")));
    Move<VkPipeline> pipeline(makeComputePipeline(deviceInterface, getDevice(), *pipelineLayout, *computeModule));

    // Create command pool and command buffer
    const Move<VkCommandPool> cmdPool(makeCommandPool(deviceInterface, getDevice(), computeQueue.queueFamilyIndex));
    const Move<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(deviceInterface, getDevice(), cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Recording commands
    beginCommandBuffer(deviceInterface, cmdBuffer.get());

    // Pre clear barriers
    VkImageMemoryBarrier imgBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType         sType
        nullptr,                                // const void*             pNext
        VK_ACCESS_NONE,                         // VkAccessFlags           srcAccessMask
        VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags           dstAccessMask
        VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout           oldLayout
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout           newLayout
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                srcQueueFamilyIndex
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t                dstQueueFamilyIndex
        resultImage.get(),                      // VkImage                 image
        getImageSRR(),                          // VkImageSubresourceRange subresourceRange
    };
    deviceInterface.cmdPipelineBarrier(cmdBuffer.get(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1u, &imgBarrier);

    // Clearing images
    VkClearColorValue initZeroValue = {{0, 0, 0, 0}};
    VkImageSubresourceRange range   = getImageSRR();
    deviceInterface.cmdClearColorImage(cmdBuffer.get(), resultImage.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       &initZeroValue, 1, &range);

    // Pre read barrier
    imgBarrier.srcAccessMask = VK_ACCESS_NONE;
    imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    imgBarrier.oldLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
    imgBarrier.newLayout     = VK_IMAGE_LAYOUT_GENERAL;
    imgBarrier.image         = msSparseImage.get();
    deviceInterface.cmdPipelineBarrier(cmdBuffer.get(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1u,
                                       &imgBarrier);

    // Pre write barrier
    imgBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imgBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    imgBarrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imgBarrier.newLayout     = VK_IMAGE_LAYOUT_GENERAL;
    imgBarrier.image         = resultImage.get();
    deviceInterface.cmdPipelineBarrier(cmdBuffer.get(), VK_PIPELINE_STAGE_TRANSFER_BIT,
                                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1u,
                                       &imgBarrier);

    // Binding pipeline and resources
    deviceInterface.cmdBindPipeline(cmdBuffer.get(), VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.get());
    deviceInterface.cmdBindDescriptorSets(cmdBuffer.get(), VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.get(), 0, 1,
                                          &descriptorSet.get(), 0, nullptr);

    // Dispatch
    deviceInterface.cmdDispatch(cmdBuffer.get(), m_params.imgSize.x(), m_params.imgSize.y(), m_params.imgSize.z());

    // Post write barrier
    imgBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    imgBarrier.oldLayout     = VK_IMAGE_LAYOUT_GENERAL;
    imgBarrier.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    imgBarrier.image         = resultImage.get();
    deviceInterface.cmdPipelineBarrier(cmdBuffer.get(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1u, &imgBarrier);
    // Copy result image to buffer
    const VkBufferImageCopy cpyInfo = {
        0u,                            // VkDeviceSize             bufferOffset
        0u,                            // uint32_t                 bufferRowLength
        0u,                            // uint32_t                 bufferImageHeight
        getImageSRL(),                 // VkImageSubresourceLayers imageSubresource
        get3DOffset({0, 0, 0}),        // VkOffset3D               imageOffset
        get3DExtent(m_params.imgSize), // VkExtent3D               imageExtent
    };
    deviceInterface.cmdCopyImageToBuffer(cmdBuffer.get(), resultImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                         resultBuffer.get(), 1u, &cpyInfo);

    endCommandBuffer(deviceInterface, cmdBuffer.get());

    // Submit commands for execution and wait for completion
    submitCommandsAndWait(deviceInterface, getDevice(), computeQueue.queueHandle, cmdBuffer.get(), 0u, nullptr, nullptr,
                          0, nullptr);

    // Retrieve data from buffer to host memory
    invalidateAlloc(deviceInterface, getDevice(), *resultBufferMemory.get());

    // Wait for compute queue to become idle
    deviceInterface.queueWaitIdle(computeQueue.queueHandle);

    const uint32_t elemCnt = getElemCount(m_params.imgSize);
    std::vector<uint32_t> out(elemCnt);
    const uint32_t *pHost = static_cast<uint32_t *>(resultBufferMemory->getHostPtr());
    out.assign(pHost, pHost + elemCnt);

    bool passed = true;
    for (uint32_t ndx = 0; ndx < elemCnt; ++ndx)
    {
        if (out[ndx] != m_params.sampleCount)
        {
            passed = false;
        }
    }

    return passed ? tcu::TestStatus::pass("Passed") : tcu::TestStatus::fail("Failed");
}

Move<VkBuffer> MultisampledImageSparseBindingInstance::createBufferAndBindMemory(AllocationMp *outMemory)
{
    const VkDevice &device        = getDevice();
    const DeviceInterface &vkdi   = getDeviceInterface();
    VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    Allocator &allocator          = getAllocator();
    const uint32_t bufferSize     = calculateBufferSize(m_params.imgSize);

    const VkBufferCreateInfo bufferCreateInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType        sType
        nullptr,                              // const void*            pNext
        0,                                    // VkBufferCreateFlags    flags
        bufferSize,                           // VkDeviceSize           size
        usageFlags,                           // VkBufferUsageFlags     usage
        VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode          sharingMode
        0,                                    // uint32_t               queueFamilyIndexCount
        nullptr,                              // const uint32_t*        pQueueFamilyIndices
    };

    Move<VkBuffer> buffer(vk::createBuffer(vkdi, device, &bufferCreateInfo));
    const VkMemoryRequirements requirements = getBufferMemoryRequirements(vkdi, device, *buffer);
    AllocationMp bufferMemory               = allocator.allocate(requirements, MemoryRequirement::HostVisible);

    VK_CHECK(vkdi.bindBufferMemory(device, *buffer, bufferMemory->getMemory(), bufferMemory->getOffset()));
    *outMemory = bufferMemory;

    return buffer;
}

Move<VkImage> MultisampledImageSparseBindingInstance::createImageAndBindMemory(tcu::UVec3 imgSize, VkFormat format,
                                                                               AllocationMp *outMemory)
{
    const VkDevice &device      = getDevice();
    const DeviceInterface &vkdi = getDeviceInterface();
    Allocator &allocator        = getAllocator();
    VkImageUsageFlags usageFlags =
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType          sType
        nullptr,                             // const void*              pNext
        0u,                                  // VkImageCreateFlags       flags
        VK_IMAGE_TYPE_2D,                    // VkImageType              imageType
        format,                              // VkFormat                 format
        get3DExtent(imgSize),                // VkExtent3D               extent
        1u,                                  // uint32_t                 mipLevels
        1u,                                  // uint32_t                 arrayLayers
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits    samples
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling            tiling
        usageFlags,                          // VkImageUsageFlags        usage
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode            sharingMode
        0u,                                  // uint32_t                 queueFamilyIndexCount
        nullptr,                             // const uint32_t*          pQueueFamilyIndices
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout            initialLayout
    };

    Move<VkImage> image(vk::createImage(vkdi, device, &imageCreateInfo));
    const VkMemoryRequirements requirements = getImageMemoryRequirements(vkdi, device, *image);
    AllocationMp imageMemory                = allocator.allocate(requirements, MemoryRequirement::Any);

    VK_CHECK(vkdi.bindImageMemory(device, *image, imageMemory->getMemory(), imageMemory->getOffset()));
    *outMemory = imageMemory;

    return image;
}

Move<VkImageView> MultisampledImageSparseBindingInstance::createImageView(VkFormat format, VkImage image)
{
    const VkDevice &device      = getDevice();
    const DeviceInterface &vkdi = getDeviceInterface();

    return makeImageView(vkdi, device, image, VK_IMAGE_VIEW_TYPE_2D, format, getImageSRR());
}

Move<VkDescriptorSetLayout> MultisampledImageSparseBindingInstance::createDescriptorSetLayout()
{
    const VkDevice &device      = getDevice();
    const DeviceInterface &vkdi = getDeviceInterface();

    DescriptorSetLayoutBuilder builder;
    builder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);

    return builder.build(vkdi, device);
}

Move<VkPipelineLayout> MultisampledImageSparseBindingInstance::createPipelineLayout(
    VkDescriptorSetLayout descriptorSetLayout)
{
    const VkDevice &device      = getDevice();
    const DeviceInterface &vkdi = getDeviceInterface();

    const VkPipelineLayoutCreateInfo createInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType                 sType
        nullptr,                                       // const void*                     pNext
        (VkPipelineLayoutCreateFlags)0,                // VkPipelineLayoutCreateFlags     flags
        1,                                             // uint32_t                        setLayoutCount
        &descriptorSetLayout,                          // const VkDescriptorSetLayout*    pSetLayouts
        0,                                             // uint32_t                        pushConstantRangeCount
        nullptr,                                       // const VkPushConstantRange*      pPushConstantRanges
    };

    return vk::createPipelineLayout(vkdi, device, &createInfo);
}

Move<VkDescriptorPool> MultisampledImageSparseBindingInstance::createDescriptorPool()
{
    const VkDevice &device      = getDevice();
    const DeviceInterface &vkdi = getDeviceInterface();

    DescriptorPoolBuilder builder;
    builder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1); // Multisampled image
    builder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1); // Result image

    return builder.build(vkdi, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);
}

Move<VkDescriptorSet> MultisampledImageSparseBindingInstance::createDescriptorSet(
    VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout, const VkDescriptorImageInfo &msImgInfo,
    const VkDescriptorImageInfo &resultImgInfo)
{
    const VkDevice &device      = getDevice();
    const DeviceInterface &vkdi = getDeviceInterface();

    const VkDescriptorSetAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType                 sType
        nullptr,                                        // const void*                     pNext
        descriptorPool,                                 // VkDescriptorPool                descriptorPool
        1,                                              // uint32_t                        descriptorSetCount
        &descriptorSetLayout,                           // const VkDescriptorSetLayout*    pSetLayouts
    };

    Move<VkDescriptorSet> descriptorSet = vk::allocateDescriptorSet(vkdi, device, &allocInfo);
    DescriptorSetUpdateBuilder builder;
    builder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0),
                        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &msImgInfo);
    builder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1),
                        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &resultImgInfo);
    builder.update(vkdi, device);

    return descriptorSet;
}

TestInstance *MultisampledImageSparseBindingCase::createInstance(Context &context) const
{
    return new MultisampledImageSparseBindingInstance(context, m_params);
}

} // namespace

tcu::TestCaseGroup *createSparseResourcesMultisampledImageCommonTests(tcu::TestContext &testCtx,
                                                                      de::MovePtr<tcu::TestCaseGroup> testGroup)
{
    static const VkFormat formats[] = {
        VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R32_SFLOAT,

        VK_FORMAT_R32G32B32A32_UINT,   VK_FORMAT_R16G16B16A16_UINT,   VK_FORMAT_R8G8B8A8_UINT, VK_FORMAT_R32_UINT,

        VK_FORMAT_R32G32B32A32_SINT,   VK_FORMAT_R16G16B16A16_SINT,   VK_FORMAT_R8G8B8A8_SINT, VK_FORMAT_R32_SINT,
    };

    static const VkSampleCountFlagBits samples[] = {
        VK_SAMPLE_COUNT_2_BIT,  VK_SAMPLE_COUNT_4_BIT,  VK_SAMPLE_COUNT_8_BIT,
        VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_32_BIT, VK_SAMPLE_COUNT_64_BIT,
    };

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(formats); ++ndx)
    {
        de::MovePtr<tcu::TestCaseGroup> formatGroup(
            new tcu::TestCaseGroup(testCtx, getImageFormatID(formats[ndx]).c_str()));

        for (uint32_t ndy = 0; ndy < DE_LENGTH_OF_ARRAY(samples); ++ndy)
        {
            const std::string samplesCaseName = "samples_" + de::toString(samples[ndy]);

            TestParams params;
            params.format      = formats[ndx];
            params.sampleCount = samples[ndy];
            params.imgSize     = tcu::UVec3(256, 128, 1);

            formatGroup->addChild(new MultisampledImageSparseBindingCase(testCtx, samplesCaseName.c_str(), params));
        }

        testGroup->addChild(formatGroup.release());
    }

    return testGroup.release();
}

tcu::TestCaseGroup *createSparseResourcesMultisampledImageSparseBindingTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "multisampled_image_sparse_binding"));
    return createSparseResourcesMultisampledImageCommonTests(testCtx, testGroup);
}

} // namespace sparse
} // namespace vkt
