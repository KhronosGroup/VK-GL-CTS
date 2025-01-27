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
 * \file  vktSparseResourcesImageSparseBinding.cpp
 * \brief Sparse fully resident images with mipmaps tests
 *//*--------------------------------------------------------------------*/

#include "vktSparseResourcesBufferSparseBinding.hpp"
#include "vktSparseResourcesTestsUtil.hpp"
#include "vktSparseResourcesBase.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "tcuTextureUtil.hpp"

#include <string>
#include <vector>

using namespace vk;

namespace vkt
{
namespace sparse
{
namespace
{

enum BindType
{
    MULTIPLE_SPARSE_MEMORY_BIND = 0,
    MULTIPLE_SPARSE_IMAGE_OPAQUE_MEMORY_BIND_INFO,
    MULTIPLE_BIND_SPARSE_INFO,
};

struct TestParams
{
    ImageType imageType;
    tcu::UVec3 imageSize;
    VkFormat format;
    BindType bindType;
};

const char *toString(BindType type)
{
    const char *strTable[] = {
        "multiple_sparse_memory_bind",                   // MULTIPLE_SPARSE_MEMORY_BIND
        "multiple_sparse_image_opaque_memory_bind_info", // MULTIPLE_SPARSE_IMAGE_OPAQUE_MEMORY_BIND_INFO
        "multiple_bind_sparse_info",                     // MULTIPLE_BIND_SPARSE_INFO
    };

    return strTable[type];
}

class ImageSparseBindingCase : public TestCase
{
public:
    ImageSparseBindingCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &testParams,
                           const bool useDeviceGroups = false);

    TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    const bool m_useDeviceGroups;
    const TestParams m_params;
};

ImageSparseBindingCase::ImageSparseBindingCase(tcu::TestContext &testCtx, const std::string &name,
                                               const TestParams &testParams, const bool useDeviceGroups)

    : TestCase(testCtx, name)
    , m_useDeviceGroups(useDeviceGroups)
    , m_params(testParams)
{
}

void ImageSparseBindingCase::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SPARSE_BINDING);

#ifndef CTS_USES_VULKANSC
    if (m_params.format == VK_FORMAT_A8_UNORM_KHR)
        context.requireDeviceFunctionality("VK_KHR_maintenance5");
#endif // CTS_USES_VULKANSC

    if (!isImageSizeSupported(context.getInstanceInterface(), context.getPhysicalDevice(), m_params.imageType,
                              m_params.imageSize))
        TCU_THROW(NotSupportedError, "Image size not supported for device");

    if (formatIsR64(m_params.format))
    {
        context.requireDeviceFunctionality("VK_EXT_shader_image_atomic_int64");

        if (context.getShaderImageAtomicInt64FeaturesEXT().sparseImageInt64Atomics == VK_FALSE)
        {
            TCU_THROW(NotSupportedError, "sparseImageInt64Atomics is not supported for device");
        }
    }
}

class ImageSparseBindingInstance : public SparseResourcesBaseInstance
{
public:
    ImageSparseBindingInstance(Context &context, const TestParams &testParams, const bool useDeviceGroups);

    tcu::TestStatus iterate(void);

private:
    const bool m_useDeviceGroups;
    const TestParams m_params;
};

ImageSparseBindingInstance::ImageSparseBindingInstance(Context &context, const TestParams &testParams,
                                                       const bool useDeviceGroups)

    : SparseResourcesBaseInstance(context, useDeviceGroups)
    , m_useDeviceGroups(useDeviceGroups)
    , m_params(testParams)
{
}

tcu::TestStatus ImageSparseBindingInstance::iterate(void)
{
    const InstanceInterface &instance = m_context.getInstanceInterface();

    {
        // Create logical device supporting both sparse and compute queues
        QueueRequirementsVec queueRequirements;
        queueRequirements.push_back(QueueRequirements(VK_QUEUE_SPARSE_BINDING_BIT, 1u));
        queueRequirements.push_back(QueueRequirements(VK_QUEUE_COMPUTE_BIT, 1u));

        createDeviceSupportingQueues(queueRequirements, false, m_params.format == VK_FORMAT_A8_UNORM_KHR);
    }

    const VkPhysicalDevice physicalDevice = getPhysicalDevice();
    VkImageCreateInfo imageSparseInfo;
    std::vector<DeviceMemorySp> deviceMemUniquePtrVec;

    const DeviceInterface &deviceInterface          = getDeviceInterface();
    const Queue &sparseQueue                        = getQueue(VK_QUEUE_SPARSE_BINDING_BIT, 0);
    const Queue &computeQueue                       = getQueue(VK_QUEUE_COMPUTE_BIT, 0);
    const PlanarFormatDescription formatDescription = getPlanarFormatDescription(m_params.format);

    // Go through all physical devices
    for (uint32_t physDevID = 0; physDevID < m_numPhysicalDevices; ++physDevID)
    {
        const uint32_t firstDeviceID  = physDevID;
        const uint32_t secondDeviceID = (firstDeviceID + 1) % m_numPhysicalDevices;

        imageSparseInfo.sType     = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO; //VkStructureType sType;
        imageSparseInfo.pNext     = nullptr;                             //const void* pNext;
        imageSparseInfo.flags     = VK_IMAGE_CREATE_SPARSE_BINDING_BIT;  //VkImageCreateFlags flags;
        imageSparseInfo.imageType = mapImageType(m_params.imageType);    //VkImageType imageType;
        imageSparseInfo.format    = m_params.format;                     //VkFormat format;
        imageSparseInfo.extent =
            makeExtent3D(getLayerSize(m_params.imageType, m_params.imageSize));               //VkExtent3D extent;
        imageSparseInfo.arrayLayers   = getNumLayers(m_params.imageType, m_params.imageSize); //uint32_t arrayLayers;
        imageSparseInfo.samples       = VK_SAMPLE_COUNT_1_BIT;     //VkSampleCountFlagBits samples;
        imageSparseInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;   //VkImageTiling tiling;
        imageSparseInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; //VkImageLayout initialLayout;
        imageSparseInfo.usage =
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; //VkImageUsageFlags usage;
        imageSparseInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;     //VkSharingMode sharingMode;
        imageSparseInfo.queueFamilyIndexCount = 0u;                            //uint32_t queueFamilyIndexCount;
        imageSparseInfo.pQueueFamilyIndices   = nullptr;                       //const uint32_t* pQueueFamilyIndices;

        if (m_params.imageType == IMAGE_TYPE_CUBE || m_params.imageType == IMAGE_TYPE_CUBE_ARRAY)
        {
            imageSparseInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }

        {
            VkImageFormatProperties imageFormatProperties;
            if (instance.getPhysicalDeviceImageFormatProperties(
                    physicalDevice, imageSparseInfo.format, imageSparseInfo.imageType, imageSparseInfo.tiling,
                    imageSparseInfo.usage, imageSparseInfo.flags,
                    &imageFormatProperties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
            {
                TCU_THROW(NotSupportedError, "Image format does not support sparse binding operations");
            }

            imageSparseInfo.mipLevels =
                getMipmapCount(m_params.format, formatDescription, imageFormatProperties, imageSparseInfo.extent);
        }

        // Create sparse image
        const Unique<VkImage> imageSparse(createImage(deviceInterface, getDevice(), &imageSparseInfo));

        const Unique<VkFence> imageSparseBindFence(createFence(deviceInterface, getDevice()));

        // Get sparse image general memory requirements
        const VkMemoryRequirements imageMemoryRequirements =
            getImageMemoryRequirements(deviceInterface, getDevice(), *imageSparse);

        // Check if required image memory size does not exceed device limits
        if (imageMemoryRequirements.size >
            getPhysicalDeviceProperties(instance, getPhysicalDevice(secondDeviceID)).limits.sparseAddressSpaceSize)
            TCU_THROW(NotSupportedError, "Required memory size for sparse resource exceeds device limits");

        DE_ASSERT((imageMemoryRequirements.size % imageMemoryRequirements.alignment) == 0);

        const uint32_t numSparseBinds =
            static_cast<uint32_t>(imageMemoryRequirements.size / imageMemoryRequirements.alignment);
        const uint32_t memoryType = findMatchingMemoryType(instance, getPhysicalDevice(secondDeviceID),
                                                           imageMemoryRequirements, MemoryRequirement::Any);

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
                ((peerMemoryFeatureFlags & VK_PEER_MEMORY_FEATURE_COPY_DST_BIT) == 0))
            {
                TCU_THROW(NotSupportedError, "Peer memory does not support COPY_SRC and COPY_DST");
            }
        }

        switch (m_params.bindType)
        {
        case MULTIPLE_SPARSE_MEMORY_BIND:
        {
            std::vector<VkSparseMemoryBind> sparseMemoryBinds;
            for (uint32_t sparseBindNdx = 0; sparseBindNdx < numSparseBinds; ++sparseBindNdx)
            {
                const VkSparseMemoryBind sparseMemoryBind =
                    makeSparseMemoryBind(deviceInterface, getDevice(), imageMemoryRequirements.alignment, memoryType,
                                         imageMemoryRequirements.alignment * sparseBindNdx);

                deviceMemUniquePtrVec.push_back(makeVkSharedPtr(
                    Move<VkDeviceMemory>(check<VkDeviceMemory>(sparseMemoryBind.memory),
                                         Deleter<VkDeviceMemory>(deviceInterface, getDevice(), nullptr))));

                sparseMemoryBinds.push_back(sparseMemoryBind);
            }

            // Create sparse image memory bind semaphore
            const VkSparseImageOpaqueMemoryBindInfo opaqueBindInfo = makeSparseImageOpaqueMemoryBindInfo(
                *imageSparse, static_cast<uint32_t>(sparseMemoryBinds.size()), sparseMemoryBinds.data());

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
                0u,                                                    //uint32_t bufferBindCount;
                nullptr,         //const VkSparseBufferMemoryBindInfo* pBufferBinds;
                1u,              //uint32_t imageOpaqueBindCount;
                &opaqueBindInfo, //const VkSparseImageOpaqueMemoryBindInfo* pImageOpaqueBinds;
                0u,              //uint32_t imageBindCount;
                nullptr,         //const VkSparseImageMemoryBindInfo* pImageBinds;
                0u,              //uint32_t signalSemaphoreCount;
                nullptr,         //const VkSemaphore* pSignalSemaphores;
            };

            // Submit sparse bind commands for execution
            VK_CHECK(
                deviceInterface.queueBindSparse(sparseQueue.queueHandle, 1u, &bindSparseInfo, *imageSparseBindFence));

            break;
        }
        case MULTIPLE_SPARSE_IMAGE_OPAQUE_MEMORY_BIND_INFO:
        {
            std::vector<VkSparseMemoryBind> sparseMemoryBinds;
            for (uint32_t sparseBindNdx = 0; sparseBindNdx < numSparseBinds; ++sparseBindNdx)
            {
                const VkSparseMemoryBind sparseMemoryBind =
                    makeSparseMemoryBind(deviceInterface, getDevice(), imageMemoryRequirements.alignment, memoryType,
                                         imageMemoryRequirements.alignment * sparseBindNdx);

                deviceMemUniquePtrVec.push_back(makeVkSharedPtr(
                    Move<VkDeviceMemory>(check<VkDeviceMemory>(sparseMemoryBind.memory),
                                         Deleter<VkDeviceMemory>(deviceInterface, getDevice(), nullptr))));

                sparseMemoryBinds.push_back(sparseMemoryBind);
            }

            std::vector<VkSparseImageOpaqueMemoryBindInfo> opaqueBindInfos;
            for (uint32_t sparseBindNdx = 0; sparseBindNdx < numSparseBinds; ++sparseBindNdx)
            {
                const VkSparseImageOpaqueMemoryBindInfo opaqueBindInfo =
                    makeSparseImageOpaqueMemoryBindInfo(*imageSparse, 1u, &sparseMemoryBinds[sparseBindNdx]);

                opaqueBindInfos.push_back(opaqueBindInfo);
            }

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
                0u,                                                    //uint32_t bufferBindCount;
                nullptr,                //const VkSparseBufferMemoryBindInfo* pBufferBinds;
                numSparseBinds,         //uint32_t imageOpaqueBindCount;
                opaqueBindInfos.data(), //const VkSparseImageOpaqueMemoryBindInfo* pImageOpaqueBinds;
                0u,                     //uint32_t imageBindCount;
                nullptr,                //const VkSparseImageMemoryBindInfo* pImageBinds;
                0u,                     //uint32_t signalSemaphoreCount;
                nullptr,                //const VkSemaphore* pSignalSemaphores;
            };

            // Submit sparse bind commands for execution
            VK_CHECK(
                deviceInterface.queueBindSparse(sparseQueue.queueHandle, 1u, &bindSparseInfo, *imageSparseBindFence));

            break;
        }
        case MULTIPLE_BIND_SPARSE_INFO:
        {
            std::vector<VkSparseMemoryBind> sparseMemoryBinds;
            for (uint32_t sparseBindNdx = 0; sparseBindNdx < numSparseBinds; ++sparseBindNdx)
            {
                const VkSparseMemoryBind sparseMemoryBind =
                    makeSparseMemoryBind(deviceInterface, getDevice(), imageMemoryRequirements.alignment, memoryType,
                                         imageMemoryRequirements.alignment * sparseBindNdx);

                deviceMemUniquePtrVec.push_back(makeVkSharedPtr(
                    Move<VkDeviceMemory>(check<VkDeviceMemory>(sparseMemoryBind.memory),
                                         Deleter<VkDeviceMemory>(deviceInterface, getDevice(), nullptr))));

                sparseMemoryBinds.push_back(sparseMemoryBind);
            }

            std::vector<VkSparseImageOpaqueMemoryBindInfo> opaqueBindInfos;
            for (uint32_t sparseBindNdx = 0; sparseBindNdx < numSparseBinds; ++sparseBindNdx)
            {
                const VkSparseImageOpaqueMemoryBindInfo opaqueBindInfo =
                    makeSparseImageOpaqueMemoryBindInfo(*imageSparse, 1u, &sparseMemoryBinds[sparseBindNdx]);

                opaqueBindInfos.push_back(opaqueBindInfo);
            }

            const VkDeviceGroupBindSparseInfo devGroupBindSparseInfo = {
                VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO, //VkStructureType sType;
                nullptr,                                         //const void* pNext;
                firstDeviceID,                                   //uint32_t resourceDeviceIndex;
                secondDeviceID,                                  //uint32_t memoryDeviceIndex;
            };

            std::vector<VkBindSparseInfo> bindSparseInfos;
            for (uint32_t sparseBindNdx = 0; sparseBindNdx < numSparseBinds; ++sparseBindNdx)
            {
                const VkBindSparseInfo bindSparseInfo = {
                    VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,                    //VkStructureType sType;
                    m_useDeviceGroups ? &devGroupBindSparseInfo : nullptr, //const void* pNext;
                    0u,                                                    //uint32_t waitSemaphoreCount;
                    nullptr,                                               //const VkSemaphore* pWaitSemaphores;
                    0u,                                                    //uint32_t bufferBindCount;
                    nullptr,                         //const VkSparseBufferMemoryBindInfo* pBufferBinds;
                    1u,                              //uint32_t imageOpaqueBindCount;
                    &opaqueBindInfos[sparseBindNdx], //const VkSparseImageOpaqueMemoryBindInfo* pImageOpaqueBinds;
                    0u,                              //uint32_t imageBindCount;
                    nullptr,                         //const VkSparseImageMemoryBindInfo* pImageBinds;
                    0u,                              //uint32_t signalSemaphoreCount;
                    nullptr,                         //const VkSemaphore* pSignalSemaphores;
                };

                bindSparseInfos.push_back(bindSparseInfo);
            }

            // Submit sparse bind commands for execution
            VK_CHECK(deviceInterface.queueBindSparse(sparseQueue.queueHandle, numSparseBinds, bindSparseInfos.data(),
                                                     *imageSparseBindFence));

            break;
        }
        default:
        {
            break;
            DE_ASSERT(false);
        }
        }

        // Waiting for resources to be bound
        VK_CHECK(deviceInterface.waitForFences(getDevice(), 1u, &imageSparseBindFence.get(), VK_TRUE, ~0ull));

        uint32_t imageSizeInBytes = 0;

        for (uint32_t planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
            for (uint32_t mipmapNdx = 0; mipmapNdx < imageSparseInfo.mipLevels; ++mipmapNdx)
                imageSizeInBytes +=
                    getImageMipLevelSizeInBytes(imageSparseInfo.extent, imageSparseInfo.arrayLayers, formatDescription,
                                                planeNdx, mipmapNdx, BUFFER_IMAGE_COPY_OFFSET_GRANULARITY);

        std::vector<VkBufferImageCopy> bufferImageCopy(formatDescription.numPlanes * imageSparseInfo.mipLevels);
        {
            uint32_t bufferOffset = 0;
            for (uint32_t planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
            {
                const VkImageAspectFlags aspect =
                    (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;

                for (uint32_t mipmapNdx = 0; mipmapNdx < imageSparseInfo.mipLevels; ++mipmapNdx)
                {
                    bufferImageCopy[planeNdx * imageSparseInfo.mipLevels + mipmapNdx] = {
                        bufferOffset, // VkDeviceSize bufferOffset;
                        0u,           // uint32_t bufferRowLength;
                        0u,           // uint32_t bufferImageHeight;
                        makeImageSubresourceLayers(
                            aspect, mipmapNdx, 0u,
                            imageSparseInfo.arrayLayers), // VkImageSubresourceLayers imageSubresource;
                        makeOffset3D(0, 0, 0),            // VkOffset3D imageOffset;
                        vk::getPlaneExtent(formatDescription, imageSparseInfo.extent, planeNdx,
                                           mipmapNdx) // VkExtent3D imageExtent;
                    };
                    bufferOffset += getImageMipLevelSizeInBytes(imageSparseInfo.extent, imageSparseInfo.arrayLayers,
                                                                formatDescription, planeNdx, mipmapNdx,
                                                                BUFFER_IMAGE_COPY_OFFSET_GRANULARITY);
                }
            }
        }

        // Create command buffer for compute and transfer operations
        const Unique<VkCommandPool> commandPool(
            makeCommandPool(deviceInterface, getDevice(), computeQueue.queueFamilyIndex));
        const Unique<VkCommandBuffer> commandBuffer(
            allocateCommandBuffer(deviceInterface, getDevice(), *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

        // Start recording commands
        beginCommandBuffer(deviceInterface, *commandBuffer);

        const VkBufferCreateInfo inputBufferCreateInfo =
            makeBufferCreateInfo(imageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        const Unique<VkBuffer> inputBuffer(createBuffer(deviceInterface, getDevice(), &inputBufferCreateInfo));
        const de::UniquePtr<Allocation> inputBufferAlloc(
            bindBuffer(deviceInterface, getDevice(), getAllocator(), *inputBuffer, MemoryRequirement::HostVisible));

        std::vector<uint8_t> referenceData(imageSizeInBytes);
        for (uint32_t valueNdx = 0; valueNdx < imageSizeInBytes; ++valueNdx)
        {
            referenceData[valueNdx] = static_cast<uint8_t>((valueNdx % imageMemoryRequirements.alignment) + 1u);
        }

        {
            deMemcpy(inputBufferAlloc->getHostPtr(), referenceData.data(), imageSizeInBytes);
            flushAlloc(deviceInterface, getDevice(), *inputBufferAlloc);

            const VkBufferMemoryBarrier inputBufferBarrier = makeBufferMemoryBarrier(
                VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, *inputBuffer, 0u, imageSizeInBytes);
            deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_HOST_BIT,
                                               VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 1u, &inputBufferBarrier,
                                               0u, nullptr);
        }

        {
            std::vector<VkImageMemoryBarrier> imageSparseTransferDstBarriers;

            for (uint32_t planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
            {
                const VkImageAspectFlags aspect =
                    (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;

                imageSparseTransferDstBarriers.push_back(makeImageMemoryBarrier(
                    0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    *imageSparse,
                    makeImageSubresourceRange(aspect, 0u, imageSparseInfo.mipLevels, 0u, imageSparseInfo.arrayLayers),
                    sparseQueue.queueFamilyIndex != computeQueue.queueFamilyIndex ? sparseQueue.queueFamilyIndex :
                                                                                    VK_QUEUE_FAMILY_IGNORED,
                    sparseQueue.queueFamilyIndex != computeQueue.queueFamilyIndex ? computeQueue.queueFamilyIndex :
                                                                                    VK_QUEUE_FAMILY_IGNORED));
            }
            deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                               VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr,
                                               static_cast<uint32_t>(imageSparseTransferDstBarriers.size()),
                                               imageSparseTransferDstBarriers.data());
        }

        deviceInterface.cmdCopyBufferToImage(*commandBuffer, *inputBuffer, *imageSparse,
                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                             static_cast<uint32_t>(bufferImageCopy.size()), bufferImageCopy.data());

        {
            std::vector<VkImageMemoryBarrier> imageSparseTransferSrcBarriers;

            for (uint32_t planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
            {
                const VkImageAspectFlags aspect =
                    (formatDescription.numPlanes > 1) ? getPlaneAspect(planeNdx) : VK_IMAGE_ASPECT_COLOR_BIT;

                imageSparseTransferSrcBarriers.push_back(makeImageMemoryBarrier(
                    VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *imageSparse,
                    makeImageSubresourceRange(aspect, 0u, imageSparseInfo.mipLevels, 0u, imageSparseInfo.arrayLayers)));
            }

            deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                               VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr,
                                               static_cast<uint32_t>(imageSparseTransferSrcBarriers.size()),
                                               imageSparseTransferSrcBarriers.data());
        }

        const VkBufferCreateInfo outputBufferCreateInfo =
            makeBufferCreateInfo(imageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const Unique<VkBuffer> outputBuffer(createBuffer(deviceInterface, getDevice(), &outputBufferCreateInfo));
        const de::UniquePtr<Allocation> outputBufferAlloc(
            bindBuffer(deviceInterface, getDevice(), getAllocator(), *outputBuffer, MemoryRequirement::HostVisible));

        deviceInterface.cmdCopyImageToBuffer(*commandBuffer, *imageSparse, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                             *outputBuffer, static_cast<uint32_t>(bufferImageCopy.size()),
                                             bufferImageCopy.data());

        {
            const VkBufferMemoryBarrier outputBufferBarrier = makeBufferMemoryBarrier(
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *outputBuffer, 0u, imageSizeInBytes);

            deviceInterface.cmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                               VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u, &outputBufferBarrier,
                                               0u, nullptr);
        }

        // End recording commands
        endCommandBuffer(deviceInterface, *commandBuffer);

        // Submit commands for execution and wait for completion
        submitCommandsAndWait(deviceInterface, getDevice(), computeQueue.queueHandle, *commandBuffer, 0u, nullptr,
                              nullptr, 0, nullptr, m_useDeviceGroups, firstDeviceID);

        // Retrieve data from buffer to host memory
        invalidateAlloc(deviceInterface, getDevice(), *outputBufferAlloc);

        // Wait for sparse queue to become idle
        deviceInterface.queueWaitIdle(sparseQueue.queueHandle);

        const uint8_t *outputData = static_cast<const uint8_t *>(outputBufferAlloc->getHostPtr());
        bool ignoreLsb6Bits       = areLsb6BitsDontCare(imageSparseInfo.format);
        bool ignoreLsb4Bits       = areLsb4BitsDontCare(imageSparseInfo.format);

        for (uint32_t planeNdx = 0; planeNdx < formatDescription.numPlanes; ++planeNdx)
        {
            for (uint32_t mipmapNdx = 0; mipmapNdx < imageSparseInfo.mipLevels; ++mipmapNdx)
            {
                const uint32_t mipLevelSizeInBytes = getImageMipLevelSizeInBytes(
                    imageSparseInfo.extent, imageSparseInfo.arrayLayers, formatDescription, planeNdx, mipmapNdx);
                const uint32_t bufferOffset = static_cast<uint32_t>(
                    bufferImageCopy[planeNdx * imageSparseInfo.mipLevels + mipmapNdx].bufferOffset);

                // Validate results
                for (size_t byteNdx = 0; byteNdx < mipLevelSizeInBytes; byteNdx++)
                {
                    const uint8_t res = *(outputData + bufferOffset + byteNdx);
                    const uint8_t ref = referenceData[bufferOffset + byteNdx];

                    uint8_t mask = 0xFF;

                    if (!(byteNdx & 0x01) && (ignoreLsb6Bits))
                        mask = 0xC0;
                    else if (!(byteNdx & 0x01) && (ignoreLsb4Bits))
                        mask = 0xF0;

                    if ((res & mask) != (ref & mask))
                    {
                        return tcu::TestStatus::fail("Failed");
                    }
                }
            }
        }
    }

    return tcu::TestStatus::pass("Passed");
}

TestInstance *ImageSparseBindingCase::createInstance(Context &context) const
{
    return new ImageSparseBindingInstance(context, m_params, m_useDeviceGroups);
}

std::vector<TestFormat> getSparseBindingTestFormats(ImageType imageType, bool addExtraFormat)
{
    auto formats = getTestFormats(imageType);
#ifndef CTS_USES_VULKANSC
    if (addExtraFormat)
        formats.push_back(TestFormat{VK_FORMAT_A8_UNORM_KHR});
#endif // CTS_USES_VULKANSC
    return formats;
}

} // namespace

tcu::TestCaseGroup *createImageSparseBindingTestsCommon(tcu::TestContext &testCtx,
                                                        de::MovePtr<tcu::TestCaseGroup> testGroup,
                                                        const bool useDeviceGroup = false)
{
    const std::vector<TestImageParameters> imageParameters{
        {IMAGE_TYPE_1D,
         {tcu::UVec3(512u, 1u, 1u), tcu::UVec3(1024u, 1u, 1u), tcu::UVec3(11u, 1u, 1u)},
         getSparseBindingTestFormats(IMAGE_TYPE_1D, !useDeviceGroup)},
        {IMAGE_TYPE_1D_ARRAY,
         {tcu::UVec3(512u, 1u, 64u), tcu::UVec3(1024u, 1u, 8u), tcu::UVec3(11u, 1u, 3u)},
         getSparseBindingTestFormats(IMAGE_TYPE_1D_ARRAY, !useDeviceGroup)},
        {IMAGE_TYPE_2D,
         {tcu::UVec3(512u, 256u, 1u), tcu::UVec3(1024u, 128u, 1u), tcu::UVec3(11u, 137u, 1u)},
         getSparseBindingTestFormats(IMAGE_TYPE_2D, !useDeviceGroup)},
        {IMAGE_TYPE_2D_ARRAY,
         {tcu::UVec3(512u, 256u, 6u), tcu::UVec3(1024u, 128u, 8u), tcu::UVec3(11u, 137u, 3u)},
         getSparseBindingTestFormats(IMAGE_TYPE_2D_ARRAY, !useDeviceGroup)},
        {IMAGE_TYPE_3D,
         {tcu::UVec3(512u, 256u, 6u), tcu::UVec3(1024u, 128u, 8u), tcu::UVec3(11u, 137u, 3u)},
         getSparseBindingTestFormats(IMAGE_TYPE_3D, !useDeviceGroup)},
        {IMAGE_TYPE_CUBE,
         {tcu::UVec3(256u, 256u, 1u), tcu::UVec3(128u, 128u, 1u), tcu::UVec3(137u, 137u, 1u)},
         getSparseBindingTestFormats(IMAGE_TYPE_CUBE, !useDeviceGroup)},
        {IMAGE_TYPE_CUBE_ARRAY,
         {tcu::UVec3(256u, 256u, 6u), tcu::UVec3(128u, 128u, 8u), tcu::UVec3(137u, 137u, 3u)},
         getSparseBindingTestFormats(IMAGE_TYPE_CUBE_ARRAY, !useDeviceGroup)}};

    const BindType bindTypes[] = {MULTIPLE_SPARSE_MEMORY_BIND, MULTIPLE_SPARSE_IMAGE_OPAQUE_MEMORY_BIND_INFO,
                                  MULTIPLE_BIND_SPARSE_INFO};

    for (uint32_t bindTypeNdx = 0; bindTypeNdx < DE_LENGTH_OF_ARRAY(bindTypes); ++bindTypeNdx)
    {
        de::MovePtr<tcu::TestCaseGroup> bindTypeGroup(
            new tcu::TestCaseGroup(testCtx, toString(bindTypes[bindTypeNdx])));

        for (size_t imageTypeNdx = 0; imageTypeNdx < imageParameters.size(); ++imageTypeNdx)
        {
            const ImageType imageType = imageParameters[imageTypeNdx].imageType;
            de::MovePtr<tcu::TestCaseGroup> imageTypeGroup(
                new tcu::TestCaseGroup(testCtx, getImageTypeName(imageType).c_str()));

            for (size_t formatNdx = 0; formatNdx < imageParameters[imageTypeNdx].formats.size(); ++formatNdx)
            {
                VkFormat format               = imageParameters[imageTypeNdx].formats[formatNdx].format;
                tcu::UVec3 imageSizeAlignment = getImageSizeAlignment(format);
                de::MovePtr<tcu::TestCaseGroup> formatGroup(
                    new tcu::TestCaseGroup(testCtx, getImageFormatID(format).c_str()));

                for (size_t imageSizeNdx = 0; imageSizeNdx < imageParameters[imageTypeNdx].imageSizes.size();
                     ++imageSizeNdx)
                {
                    const tcu::UVec3 imageSize = imageParameters[imageTypeNdx].imageSizes[imageSizeNdx];

                    // skip test for images with odd sizes for some YCbCr formats
                    if ((imageSize.x() % imageSizeAlignment.x()) != 0)
                        continue;
                    if ((imageSize.y() % imageSizeAlignment.y()) != 0)
                        continue;

                    std::ostringstream stream;
                    stream << imageSize.x() << "_" << imageSize.y() << "_" << imageSize.z();

                    TestParams params;
                    params.format    = format;
                    params.imageSize = imageSize;
                    params.imageType = imageType;
                    params.bindType  = bindTypes[bindTypeNdx];

                    formatGroup->addChild(new ImageSparseBindingCase(testCtx, stream.str(), params, useDeviceGroup));
                }
                imageTypeGroup->addChild(formatGroup.release());
            }
            bindTypeGroup->addChild(imageTypeGroup.release());
        }
        testGroup->addChild(bindTypeGroup.release());
    }

    return testGroup.release();
}

tcu::TestCaseGroup *createImageSparseBindingTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "image_sparse_binding"));
    return createImageSparseBindingTestsCommon(testCtx, testGroup);
}

tcu::TestCaseGroup *createDeviceGroupImageSparseBindingTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "device_group_image_sparse_binding"));
    return createImageSparseBindingTestsCommon(testCtx, testGroup, true);
}

} // namespace sparse
} // namespace vkt
