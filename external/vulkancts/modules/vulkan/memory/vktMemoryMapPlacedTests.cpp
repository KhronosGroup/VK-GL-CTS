/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 The Khronos Group Inc.
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
 * \brief VK_EXT_map_memory_placed extension tests
 *//*--------------------------------------------------------------------*/

#include "vktMemoryMapPlacedTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "vkDefs.hpp"
#include "vkDeviceUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkPlatform.hpp"
#include "vkStrUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuResultCollector.hpp"
#include "tcuPlatform.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deMemory.h"
#include "deInt32.h"
#include "deSTLUtil.hpp"

#include "gluShaderProgram.hpp"

#include <string>
#include <sstream>
#include <cstdio>
#include <cinttypes>

#if (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_ANDROID)
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

using namespace vk;

namespace vkt
{
namespace memory
{
namespace
{

struct TestParams
{
    VkDeviceSize memorySize;
    bool testUnmapReserve;
    bool testGpuAccess;
};

// Helper function to verify that a memory range is still present in /proc/self/maps
// after UNMAP_RESERVE. This function checks if any region in /proc/self/maps fully
// contains the specified range [rangeStart, rangeEnd).
static void verifyRangeInProcMaps(uintptr_t rangeStart, uintptr_t rangeEnd, tcu::TestLog &log)
{
#if (DE_OS == DE_OS_UNIX) && !defined(__ANDROID__)
    FILE *mapsFile = fopen("/proc/self/maps", "r");
    if (mapsFile == nullptr)
    {
        TCU_THROW(QualityWarning, "Could not open /proc/self/maps for verification");
    }

    bool rangeFound = false;
    char line[512];

    while (fgets(line, sizeof(line), mapsFile) != nullptr)
    {
        // Use uintmax_t for portability across 32-bit and 64-bit platforms
        uintmax_t mapStart = 0, mapEnd = 0;
        if (sscanf(line, "%" SCNxMAX "-%" SCNxMAX, &mapStart, &mapEnd) == 2)
        {
            if (static_cast<uintptr_t>(mapStart) <= rangeStart && static_cast<uintptr_t>(mapEnd) >= rangeEnd)
            {
                rangeFound = true;
                break;
            }
        }
    }
    fclose(mapsFile);

    if (!rangeFound)
    {
        std::ostringstream msg;
        msg << "/proc/self/maps does not show the reserved range " << std::hex << rangeStart << "-" << rangeEnd
            << " as covered after UNMAP_RESERVE";
        TCU_THROW(QualityWarning, msg.str());
    }

    log << tcu::TestLog::Message << "UNMAP_RESERVE: range " << std::hex << rangeStart << "-" << rangeEnd << std::dec
        << " is still covered in /proc/self/maps" << tcu::TestLog::EndMessage;
#else
    DE_UNREF(rangeStart);
    DE_UNREF(rangeEnd);
    DE_UNREF(log);
#endif
}

class MapPlacedExactSizeTestInstance : public TestInstance
{
public:
    MapPlacedExactSizeTestInstance(Context &context, const TestParams &params) : TestInstance(context), m_params(params)
    {
    }

    tcu::TestStatus iterate(void) override
    {
#if (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_ANDROID)
        const DeviceInterface &vk      = m_context.getDeviceInterface();
        const VkDevice device          = m_context.getDevice();
        const InstanceInterface &vki   = m_context.getInstanceInterface();
        const VkPhysicalDevice physDev = m_context.getPhysicalDevice();
        tcu::TestLog &log              = m_context.getTestContext().getLog();
        tcu::ResultCollector resultCollector(log);

        VkPhysicalDeviceMapMemoryPlacedPropertiesEXT placedProps = initVulkanStructure();
        VkPhysicalDeviceProperties2 props2                       = initVulkanStructure(&placedProps);

        vki.getPhysicalDeviceProperties2(physDev, &props2);

        const size_t pageSize     = static_cast<size_t>(sysconf(_SC_PAGESIZE));
        const size_t minAlignment = static_cast<size_t>(placedProps.minPlacedMemoryMapAlignment);
        const size_t alignment    = (minAlignment > pageSize) ? minAlignment : pageSize;

        log << tcu::TestLog::Message << "System page size: " << pageSize
            << ", minPlacedMemoryMapAlignment: " << minAlignment << ", using alignment: " << alignment
            << tcu::TestLog::EndMessage;

        // Round the Vulkan allocation size up to a multiple of alignment so that the
        // placed mapping covers whole pages on both ends.
        const size_t memorySize        = static_cast<size_t>(m_params.memorySize);
        const size_t remainder         = memorySize % alignment;
        const size_t alignedMemorySize = (remainder == 0) ? memorySize : (memorySize + alignment - remainder);

        // Reserved memory layout:
        // |---- guard ----|---- VkMemory ----|---- guard ----|
        const size_t guardSize = alignment;
        const size_t fileSize  = guardSize + alignment + alignedMemorySize + guardSize;

        int memfd = memfd_create("mapplaced-test", MFD_CLOEXEC);
        if (memfd < 0)
            TCU_THROW(NotSupportedError, "memfd_create failed");

        if (ftruncate(memfd, static_cast<off_t>(fileSize)) < 0)
        {
            close(memfd);
            TCU_THROW(InternalError, "ftruncate failed");
        }

        // Two independent views of the same backing file
        void *reservedMap = mmap(nullptr, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, 0);
        if (reservedMap == MAP_FAILED)
        {
            close(memfd);
            TCU_THROW(InternalError, "mmap failed for reservedMap");
        }

        void *inspectorMap = mmap(nullptr, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, 0);
        if (inspectorMap == MAP_FAILED)
        {
            munmap(reservedMap, fileSize);
            close(memfd);
            TCU_THROW(InternalError, "mmap failed for inspectorMap");
        }

        uint8_t *reservedBase  = static_cast<uint8_t *>(reservedMap);
        uint8_t *inspectorBase = static_cast<uint8_t *>(inspectorMap);

        log << tcu::TestLog::Message << "File size: " << fileSize
            << ", reservedBase: " << static_cast<void *>(reservedBase)
            << ", inspectorBase: " << static_cast<void *>(inspectorBase) << tcu::TestLog::EndMessage;

        // Align placedAddr upward to satisfy minPlacedMemoryMapAlignment:
        uint8_t *baseForAlign     = reservedBase + guardSize;
        uint8_t *placedAddr       = static_cast<uint8_t *>(deAlignPtr(baseForAlign, alignment));
        const size_t placedOffset = static_cast<size_t>(placedAddr - reservedBase);

        DE_ASSERT(placedOffset + alignedMemorySize <= fileSize);
        DE_ASSERT(deIsAlignedPtr(placedAddr, alignment));

        log << tcu::TestLog::Message << "Placed address: " << static_cast<void *>(placedAddr) << " (file offset "
            << placedOffset << ")" << tcu::TestLog::EndMessage;

        VkPhysicalDeviceMemoryProperties memProps;
        vki.getPhysicalDeviceMemoryProperties(physDev, &memProps);
        const uint32_t memTypeIndex = selectMatchingMemoryType(memProps, ~0u, MemoryRequirement::HostVisible);

        const VkMemoryAllocateInfo allocInfo = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            nullptr,
            static_cast<VkDeviceSize>(alignedMemorySize),
            memTypeIndex,
        };

        Move<VkDeviceMemory> memory = allocateMemory(vk, device, &allocInfo);

        const VkMemoryMapPlacedInfoEXT placedInfo = {
            VK_STRUCTURE_TYPE_MEMORY_MAP_PLACED_INFO_EXT,
            nullptr,
            placedAddr,
        };

        const VkMemoryMapInfoKHR mapInfo = {
            VK_STRUCTURE_TYPE_MEMORY_MAP_INFO_KHR, &placedInfo, VK_MEMORY_MAP_PLACED_BIT_EXT, *memory, 0, VK_WHOLE_SIZE,
        };

        void *mappedPtr = nullptr;
        VkResult result = vk.mapMemory2(device, &mapInfo, &mappedPtr);

        if (result != VK_SUCCESS)
        {
            munmap(inspectorMap, fileSize);
            munmap(reservedMap, fileSize);
            close(memfd);
            resultCollector.fail("vkMapMemory2 with VK_MEMORY_MAP_PLACED_BIT_EXT failed");
            return tcu::TestStatus(resultCollector.getResult(), resultCollector.getMessage());
        }

        if (mappedPtr != placedAddr)
        {
            vk.unmapMemory(device, *memory);
            munmap(inspectorMap, fileSize);
            munmap(reservedMap, fileSize);
            close(memfd);
            resultCollector.fail("Mapped address does not match requested pPlacedAddress");
            return tcu::TestStatus(resultCollector.getResult(), resultCollector.getMessage());
        }

        log << tcu::TestLog::Message << "VkDeviceMemory mapped at: " << mappedPtr << tcu::TestLog::EndMessage;

        // Fill entire range
        const uint8_t fillPattern = 0xAB;
        deMemset(reservedBase, fillPattern, fileSize);

        if (m_params.testUnmapReserve)
        {
            const VkMemoryUnmapInfoKHR unmapInfo = {
                VK_STRUCTURE_TYPE_MEMORY_UNMAP_INFO_KHR,
                nullptr,
                VK_MEMORY_UNMAP_RESERVE_BIT_EXT,
                *memory,
            };
            vk.unmapMemory2(device, &unmapInfo);

            // Verify guard pages before the Vulkan mapping via reservedMap
            bool endsOk = true;
            for (size_t i = 0; i < placedOffset && endsOk; i++)
            {
                if (reservedBase[i] != fillPattern)
                {
                    log << tcu::TestLog::Message << "Guard byte at offset " << i << " before Vulkan mapping has value "
                        << static_cast<uint32_t>(reservedBase[i]) << ", expected " << static_cast<uint32_t>(fillPattern)
                        << tcu::TestLog::EndMessage;
                    resultCollector.fail(
                        "Pages before VkDeviceMemory are not accessible or corrupted after UNMAP_RESERVE");
                    endsOk = false;
                }
            }

            // Verify guard pages after the Vulkan mapping via reservedMap
            const size_t afterStart = placedOffset + alignedMemorySize;
            for (size_t i = afterStart; i < fileSize && endsOk; i++)
            {
                if (reservedBase[i] != fillPattern)
                {
                    log << tcu::TestLog::Message << "Guard byte at offset " << i << " after Vulkan mapping has value "
                        << static_cast<uint32_t>(reservedBase[i]) << ", expected " << static_cast<uint32_t>(fillPattern)
                        << tcu::TestLog::EndMessage;
                    resultCollector.fail(
                        "Pages after VkDeviceMemory are not accessible or corrupted after UNMAP_RESERVE");
                    endsOk = false;
                }
            }

            if (endsOk)
            {
                log << tcu::TestLog::Message
                    << "UNMAP_RESERVE: guard pages before and after Vulkan mapping are accessible and correct"
                    << tcu::TestLog::EndMessage;
            }

            // Verify /proc/self/maps cover range
            verifyRangeInProcMaps(reinterpret_cast<uintptr_t>(placedAddr),
                                  reinterpret_cast<uintptr_t>(placedAddr) + alignedMemorySize, log);

            munmap(inspectorMap, fileSize);
            munmap(reservedMap, fileSize);
            close(memfd);
            return tcu::TestStatus(resultCollector.getResult(), resultCollector.getMessage());
        }

        vk.unmapMemory(device, *memory);

        // Verify via inspectorMap
        // |---- 0xAB ----|---- VkMemory ----|---- 0xAB----|

        bool passedInspection = true;

        // Guard before
        for (size_t i = 0; i < placedOffset && passedInspection; i++)
        {
            if (inspectorBase[i] != fillPattern)
            {
                log << tcu::TestLog::Message << "inspectorMap byte at offset " << i << " (guard before) is "
                    << static_cast<uint32_t>(inspectorBase[i]) << ", expected fillPattern "
                    << static_cast<uint32_t>(fillPattern) << tcu::TestLog::EndMessage;
                resultCollector.fail("Guard pages before VkDeviceMemory mapping did not receive the fill pattern");
                passedInspection = false;
            }
        }

        // Vulkan region is managed by the driver and may be backed by device memory,
        // not the memfd file, so inspectorMap should not see the fill pattern there
        if (passedInspection)
        {
            for (size_t i = placedOffset; i < placedOffset + alignedMemorySize && passedInspection; i++)
            {
                if (inspectorBase[i] == fillPattern)
                {
                    log << tcu::TestLog::Message << "inspectorMap byte at offset " << i
                        << " Vulkan region is fillPattern " << static_cast<uint32_t>(fillPattern)
                        << " driver must have mapped extra pages into the memfd backing" << tcu::TestLog::EndMessage;
                    resultCollector.fail("VkDeviceMemory placed mapping overwrote the memfd backing "
                                         "driver mapped extra or wrong pages");
                    passedInspection = false;
                }
            }
        }

        // Guard after
        if (passedInspection)
        {
            const size_t afterStart = placedOffset + alignedMemorySize;
            for (size_t i = afterStart; i < fileSize && passedInspection; i++)
            {
                if (inspectorBase[i] != fillPattern)
                {
                    log << tcu::TestLog::Message << "inspectorMap byte at offset " << i << " (guard after) is "
                        << static_cast<uint32_t>(inspectorBase[i]) << ", expected fillPattern "
                        << static_cast<uint32_t>(fillPattern) << tcu::TestLog::EndMessage;
                    resultCollector.fail("Guard pages after VkDeviceMemory mapping did not receive the fill pattern");
                    passedInspection = false;
                }
            }
        }

        if (passedInspection)
        {
            log << tcu::TestLog::Message
                << "Exact size test passed: guard pages have fillPattern, Vulkan region does not"
                << tcu::TestLog::EndMessage;
        }

        {
            const VkMemoryMapPlacedInfoEXT rePlacedInfo = {
                VK_STRUCTURE_TYPE_MEMORY_MAP_PLACED_INFO_EXT,
                nullptr,
                placedAddr,
            };
            const VkMemoryMapInfoKHR reMapInfo = {
                VK_STRUCTURE_TYPE_MEMORY_MAP_INFO_KHR,
                &rePlacedInfo,
                VK_MEMORY_MAP_PLACED_BIT_EXT,
                *memory,
                0,
                VK_WHOLE_SIZE,
            };
            void *remappedPtr = nullptr;
            if (vk.mapMemory2(device, &reMapInfo, &remappedPtr) == VK_SUCCESS && remappedPtr == placedAddr)
            {
                const uint32_t verifyPattern = 0xCAFEBABE;
                deMemcpy(remappedPtr, &verifyPattern, sizeof(verifyPattern));
                uint32_t readback = 0;
                deMemcpy(&readback, remappedPtr, sizeof(readback));
                if (readback != verifyPattern)
                    resultCollector.fail("CPU read/write verification via re-mapped VkDeviceMemory failed");
                else
                    log << tcu::TestLog::Message << "CPU read/write via re-mapped memory passed"
                        << tcu::TestLog::EndMessage;
                vk.unmapMemory(device, *memory);
            }
            else
            {
                log << tcu::TestLog::Message
                    << "Note: re-map for CPU verification skipped (could not map at same address again)"
                    << tcu::TestLog::EndMessage;
            }
        }

        munmap(inspectorMap, fileSize);
        munmap(reservedMap, fileSize);
        close(memfd);
        return tcu::TestStatus(resultCollector.getResult(), resultCollector.getMessage());
#else
        DE_UNREF(m_params);
        TCU_THROW(NotSupportedError, "Test requires Linux/Android with memfd_create support");
#endif
    }

private:
    const TestParams m_params;
};

class MapPlacedGpuAccessTestInstance : public TestInstance
{
public:
    MapPlacedGpuAccessTestInstance(Context &context, const TestParams &params) : TestInstance(context), m_params(params)
    {
    }

    tcu::TestStatus iterate(void) override
    {
#if (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_ANDROID)
        const DeviceInterface &vk       = m_context.getDeviceInterface();
        const VkDevice device           = m_context.getDevice();
        const InstanceInterface &vki    = m_context.getInstanceInterface();
        const VkPhysicalDevice physDev  = m_context.getPhysicalDevice();
        const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
        tcu::TestLog &log               = m_context.getTestContext().getLog();
        tcu::ResultCollector resultCollector(log);

        VkPhysicalDeviceMapMemoryPlacedPropertiesEXT placedProps = initVulkanStructure();
        VkPhysicalDeviceProperties2 props2                       = initVulkanStructure(&placedProps);

        vki.getPhysicalDeviceProperties2(physDev, &props2);

        const size_t pageSize     = static_cast<size_t>(sysconf(_SC_PAGESIZE));
        const size_t minAlignment = static_cast<size_t>(placedProps.minPlacedMemoryMapAlignment);
        const size_t alignment    = (minAlignment > pageSize) ? minAlignment : pageSize;

        VkPhysicalDeviceMemoryProperties memProps;
        vki.getPhysicalDeviceMemoryProperties(physDev, &memProps);

        const uint32_t deviceLocalHostVisible =
            getCompatibleMemoryTypes(memProps, MemoryRequirement::HostVisible | MemoryRequirement::Local);
        const uint32_t memTypeIndex = (deviceLocalHostVisible != 0) ?
                                          deCtz32(deviceLocalHostVisible) :
                                          selectMatchingMemoryType(memProps, ~0u, MemoryRequirement::HostVisible);

        const VkDeviceSize bufferSize = m_params.memorySize;

        const VkBufferCreateInfo bufferInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType
            nullptr,                              // const void* pNext
            0u,                                   // VkBufferCreateFlags flags
            bufferSize,                           // VkDeviceSize size
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT, // VkBufferUsageFlags usage
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode
            1u,                                   // uint32_t queueFamilyIndexCount
            &queueFamilyIndex,                    // const uint32_t* pQueueFamilyIndices
        };

        Move<VkBuffer> buffer = createBuffer(vk, device, &bufferInfo);

        VkMemoryRequirements memReqs;
        vk.getBufferMemoryRequirements(device, *buffer, &memReqs);

        if ((memReqs.memoryTypeBits & (1u << memTypeIndex)) == 0)
        {
            TCU_THROW(NotSupportedError, "Chosen memory type not compatible with buffer");
        }

        const VkMemoryAllocateInfo allocInfo = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            nullptr,
            memReqs.size,
            memTypeIndex,
        };

        Move<VkDeviceMemory> memory = allocateMemory(vk, device, &allocInfo);

        VK_CHECK(vk.bindBufferMemory(device, *buffer, *memory, 0));

        // Reserved address layout:
        // |--- slack ---|---- VkMemory ----|
        const size_t reserveSize = static_cast<size_t>(memReqs.size) + alignment;
        void *reservedRange      = mmap(nullptr, reserveSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (reservedRange == MAP_FAILED)
            TCU_THROW(InternalError, "Failed to reserve address range");

        void *reservedAddr          = deAlignPtr(reservedRange, alignment);
        const uintptr_t alignedAddr = reinterpret_cast<uintptr_t>(reservedAddr);

        const VkMemoryMapPlacedInfoEXT placedInfo = {
            VK_STRUCTURE_TYPE_MEMORY_MAP_PLACED_INFO_EXT,
            nullptr,
            reservedAddr,
        };

        const VkMemoryMapInfoKHR mapInfo = {
            VK_STRUCTURE_TYPE_MEMORY_MAP_INFO_KHR, &placedInfo, VK_MEMORY_MAP_PLACED_BIT_EXT, *memory, 0, VK_WHOLE_SIZE,
        };

        void *mappedPtr = nullptr;
        VkResult result = vk.mapMemory2(device, &mapInfo, &mappedPtr);

        if (result != VK_SUCCESS)
        {
            munmap(reservedRange, reserveSize);
            resultCollector.fail("Failed to map memory with MAP_PLACED_BIT");
            return tcu::TestStatus(resultCollector.getResult(), resultCollector.getMessage());
        }

        // CPU write
        const uint32_t numElements           = static_cast<uint32_t>(bufferSize / sizeof(uint32_t));
        uint32_t *data                       = static_cast<uint32_t *>(mappedPtr);
        const VkMemoryPropertyFlags memFlags = memProps.memoryTypes[memTypeIndex].propertyFlags;

        for (uint32_t i = 0; i < numElements; i++)
            data[i] = i;

        if ((memFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
        {
            const VkMappedMemoryRange range = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, *memory, 0,
                                               VK_WHOLE_SIZE};
            VK_CHECK(vk.flushMappedMemoryRanges(device, 1, &range));
        }

        const VkDescriptorSetLayoutBinding binding = {
            0u,                                // binding
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // descriptorType
            1u,                                // descriptorCount
            VK_SHADER_STAGE_COMPUTE_BIT,       // stageFlags
            nullptr,                           // pImmutableSamplers
        };
        const VkDescriptorSetLayoutCreateInfo dsLayoutInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0u, 1u, &binding,
        };
        Move<VkDescriptorSetLayout> dsLayout = createDescriptorSetLayout(vk, device, &dsLayoutInfo);

        const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0u, 1u, &*dsLayout, 0u, nullptr,
        };
        Move<VkPipelineLayout> pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutInfo);

        Move<VkShaderModule> shaderModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"));

        const VkComputePipelineCreateInfo pipelineInfo = {
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            nullptr,
            0u,
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0u, VK_SHADER_STAGE_COMPUTE_BIT,
             *shaderModule, "main", nullptr},
            *pipelineLayout,
            VK_NULL_HANDLE,
            0,
        };
        Move<VkPipeline> pipeline = createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineInfo);

        const VkDescriptorPoolSize poolSize       = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u};
        const VkDescriptorPoolCreateInfo poolInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            nullptr,
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            1u,
            1u,
            &poolSize,
        };
        Move<VkDescriptorPool> descriptorPool = createDescriptorPool(vk, device, &poolInfo);

        const VkDescriptorSetAllocateInfo dsAllocInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, *descriptorPool, 1u, &*dsLayout,
        };
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VK_CHECK(vk.allocateDescriptorSets(device, &dsAllocInfo, &descriptorSet));

        const VkDescriptorBufferInfo bufferDescInfo = {*buffer, 0u, VK_WHOLE_SIZE};
        const VkWriteDescriptorSet writeDs          = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet,   0u,      0u, 1u,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,      nullptr, &bufferDescInfo, nullptr,
        };
        vk.updateDescriptorSets(device, 1u, &writeDs, 0u, nullptr);

        const VkCommandPoolCreateInfo cmdPoolInfo = {
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            nullptr,
            VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            queueFamilyIndex,
        };
        Move<VkCommandPool> cmdPool = createCommandPool(vk, device, &cmdPoolInfo);

        const VkCommandBufferAllocateInfo cmdBufInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1u,
        };
        VkCommandBuffer cmdBuf = VK_NULL_HANDLE;
        VK_CHECK(vk.allocateCommandBuffers(device, &cmdBufInfo, &cmdBuf));

        const VkCommandBufferBeginInfo beginInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            nullptr,
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            nullptr,
        };
        VK_CHECK(vk.beginCommandBuffer(cmdBuf, &beginInfo));

        {
            const VkBufferMemoryBarrier barrier = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                nullptr,
                VK_ACCESS_HOST_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                *buffer,
                0u,
                VK_WHOLE_SIZE,
            };
            vk.cmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u,
                                  nullptr, 1u, &barrier, 0u, nullptr);
        }

        vk.cmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
        vk.cmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet, 0u,
                                 nullptr);

        // GPU write
        const uint32_t groupCount = (numElements + 63u) / 64u;
        vk.cmdDispatch(cmdBuf, groupCount, 1u, 1u);

        {
            const VkBufferMemoryBarrier barrier = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                nullptr,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_HOST_READ_BIT,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                *buffer,
                0u,
                VK_WHOLE_SIZE,
            };
            vk.cmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u,
                                  nullptr, 1u, &barrier, 0u, nullptr);
        }

        VK_CHECK(vk.endCommandBuffer(cmdBuf));

        const VkSubmitInfo submitInfo = {
            VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0u, nullptr, nullptr, 1u, &cmdBuf, 0u, nullptr,
        };
        const VkQueue queue = m_context.getUniversalQueue();
        VK_CHECK(vk.queueSubmit(queue, 1u, &submitInfo, VK_NULL_HANDLE));
        VK_CHECK(vk.queueWaitIdle(queue));

        // CPU readback
        if ((memFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
        {
            const VkMappedMemoryRange range = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, *memory, 0,
                                               VK_WHOLE_SIZE};
            VK_CHECK(vk.invalidateMappedMemoryRanges(device, 1, &range));
        }

        // Expected: values[i] = i + 1  (CPU wrote i, GPU added 1)
        bool dataCorrect = true;
        for (uint32_t i = 0; i < numElements && dataCorrect; i++)
        {
            const uint32_t expected = i + 1u;
            if (data[i] != expected)
            {
                log << tcu::TestLog::Message << "Data mismatch at index " << i << ": expected " << expected << ", got "
                    << data[i] << tcu::TestLog::EndMessage;
                dataCorrect = false;
            }
        }

        if (!dataCorrect)
            resultCollector.fail("GPU access verification failed: values[i] != i+1 after compute dispatch");
        else
            log << tcu::TestLog::Message << "GPU access test passed: all " << numElements
                << " values correctly incremented by GPU" << tcu::TestLog::EndMessage;

        // Unmap padding regions
        const uintptr_t placedStart  = alignedAddr;
        const uintptr_t placedEnd    = placedStart + static_cast<uintptr_t>(memReqs.size);
        const uintptr_t reserveStart = reinterpret_cast<uintptr_t>(reservedRange);
        const size_t prefixSize      = static_cast<size_t>(placedStart - reserveStart);
        const size_t suffixSize      = reserveSize - prefixSize - static_cast<size_t>(memReqs.size);
        if (prefixSize > 0)
            munmap(reservedRange, prefixSize);
        if (suffixSize > 0)
            munmap(reinterpret_cast<void *>(placedEnd), suffixSize);

        return tcu::TestStatus(resultCollector.getResult(), resultCollector.getMessage());
#else
        DE_UNREF(m_params);
        TCU_THROW(NotSupportedError, "Test requires POSIX system calls (Linux/Android only)");
#endif
    }

private:
    const TestParams m_params;
};

class MapNormalUnmapReserveTestInstance : public TestInstance
{
public:
    MapNormalUnmapReserveTestInstance(Context &context, const TestParams &params)
        : TestInstance(context)
        , m_params(params)
    {
    }

    tcu::TestStatus iterate(void) override
    {
#if (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_ANDROID)
        const DeviceInterface &vk      = m_context.getDeviceInterface();
        const VkDevice device          = m_context.getDevice();
        const InstanceInterface &vki   = m_context.getInstanceInterface();
        const VkPhysicalDevice physDev = m_context.getPhysicalDevice();
        tcu::TestLog &log              = m_context.getTestContext().getLog();
        tcu::ResultCollector resultCollector(log);

        VkPhysicalDeviceMemoryProperties memProps;
        vki.getPhysicalDeviceMemoryProperties(physDev, &memProps);
        const uint32_t memTypeIndex = selectMatchingMemoryType(memProps, ~0u, MemoryRequirement::HostVisible);

        const size_t pageSize       = static_cast<size_t>(sysconf(_SC_PAGESIZE));
        const size_t alignedMemSize = deAlignSize(static_cast<size_t>(m_params.memorySize), pageSize);

        const VkMemoryAllocateInfo allocInfo = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            nullptr,
            static_cast<VkDeviceSize>(alignedMemSize),
            memTypeIndex,
        };
        Move<VkDeviceMemory> memory = allocateMemory(vk, device, &allocInfo);

        void *mappedPtr = nullptr;
        VK_CHECK(vk.mapMemory(device, *memory, 0u, VK_WHOLE_SIZE, 0u, &mappedPtr));

        // Record the address range that is now mapped
        const uintptr_t rangeStart = reinterpret_cast<uintptr_t>(mappedPtr);
        const uintptr_t rangeEnd   = rangeStart + alignedMemSize;

        log << tcu::TestLog::Message << "Normal map: address " << mappedPtr << ", size " << alignedMemSize
            << tcu::TestLog::EndMessage;

        deMemset(mappedPtr, 0xCD, alignedMemSize);

        const VkMemoryUnmapInfoKHR unmapInfo = {
            VK_STRUCTURE_TYPE_MEMORY_UNMAP_INFO_KHR,
            nullptr,
            VK_MEMORY_UNMAP_RESERVE_BIT_EXT,
            *memory,
        };
        VK_CHECK(vk.unmapMemory2(device, &unmapInfo));

        log << tcu::TestLog::Message << "vkUnmapMemory2 with UNMAP_RESERVE done" << tcu::TestLog::EndMessage;

        // Verify mmap(NULL) not land in the reserved range
        {
            void *probAddr = mmap(nullptr, pageSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (probAddr != MAP_FAILED)
            {
                const uintptr_t probStart = reinterpret_cast<uintptr_t>(probAddr);
                const bool overlaps       = (probStart < rangeEnd) && (probStart + pageSize > rangeStart);
                munmap(probAddr, pageSize);
                if (overlaps)
                {
                    resultCollector.fail("mmap(NULL) returned an address inside the UNMAP_RESERVE range range was not "
                                         "properly reserved");
                }
                else
                {
                    log << tcu::TestLog::Message << "mmap probe did not overlap the reserved range (good)"
                        << tcu::TestLog::EndMessage;
                }
            }
        }

        // Verify /proc/self/maps cover range
        verifyRangeInProcMaps(rangeStart, rangeEnd, log);

        return tcu::TestStatus(resultCollector.getResult(), resultCollector.getMessage());
#else
        DE_UNREF(m_params);
        TCU_THROW(NotSupportedError, "Test requires Linux/Android");
#endif
    }

private:
    const TestParams m_params;
};

class MapPlacedTestCase : public TestCase
{
public:
    MapPlacedTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params, bool gpuAccessTest)
        : TestCase(testCtx, name)
        , m_params(params)
        , m_gpuAccessTest(gpuAccessTest)
    {
    }

    void checkSupport(Context &context) const override
    {
#if (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_ANDROID)
        context.requireDeviceFunctionality("VK_EXT_map_memory_placed");
        context.requireDeviceFunctionality("VK_KHR_map_memory2");

        const InstanceInterface &vki   = context.getInstanceInterface();
        const VkPhysicalDevice physDev = context.getPhysicalDevice();

        VkPhysicalDeviceMapMemoryPlacedFeaturesEXT placedFeatures = initVulkanStructure();
        VkPhysicalDeviceFeatures2 features2                       = initVulkanStructure(&placedFeatures);

        vki.getPhysicalDeviceFeatures2(physDev, &features2);

        if (!placedFeatures.memoryMapPlaced)
            TCU_THROW(NotSupportedError, "memoryMapPlaced feature not supported");

        if (m_params.testUnmapReserve && !placedFeatures.memoryUnmapReserve)
            TCU_THROW(NotSupportedError, "memoryUnmapReserve feature not supported");
#else
        DE_UNREF(context);
        TCU_THROW(NotSupportedError, "Test requires POSIX system calls (Linux/Android only)");
#endif
    }

    TestInstance *createInstance(Context &context) const override
    {
        if (m_gpuAccessTest)
            return new MapPlacedGpuAccessTestInstance(context, m_params);
        else
            return new MapPlacedExactSizeTestInstance(context, m_params);
    }

    void initPrograms(vk::SourceCollections &programCollection) const override
    {
        if (m_gpuAccessTest)
        {
            programCollection.glslSources.add("comp")
                << glu::ComputeSource("#version 450\n"
                                      "layout(local_size_x = 64) in;\n"
                                      "layout(set = 0, binding = 0) buffer Data {\n"
                                      "    uint values[];\n"
                                      "};\n"
                                      "void main() {\n"
                                      "    uint idx = gl_GlobalInvocationID.x;\n"
                                      "    if (idx < values.length()) {\n"
                                      "        values[idx] = values[idx] + 1;\n"
                                      "    }\n"
                                      "}\n");
        }
    }

private:
    const TestParams m_params;
    const bool m_gpuAccessTest;
};

class MapNormalUnmapReserveTestCase : public TestCase
{
public:
    MapNormalUnmapReserveTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : TestCase(testCtx, name)
        , m_params(params)
    {
    }

    void checkSupport(Context &context) const override
    {
#if (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_ANDROID)
        context.requireDeviceFunctionality("VK_EXT_map_memory_placed");
        context.requireDeviceFunctionality("VK_KHR_map_memory2");

        const InstanceInterface &vki   = context.getInstanceInterface();
        const VkPhysicalDevice physDev = context.getPhysicalDevice();

        VkPhysicalDeviceMapMemoryPlacedFeaturesEXT placedFeatures = initVulkanStructure();
        VkPhysicalDeviceFeatures2 features2                       = initVulkanStructure(&placedFeatures);

        vki.getPhysicalDeviceFeatures2(physDev, &features2);

        if (!placedFeatures.memoryUnmapReserve)
            TCU_THROW(NotSupportedError, "memoryUnmapReserve feature not supported");
#else
        DE_UNREF(context);
        TCU_THROW(NotSupportedError, "Test requires Linux/Android");
#endif
    }

    TestInstance *createInstance(Context &context) const override
    {
        return new MapNormalUnmapReserveTestInstance(context, m_params);
    }

private:
    const TestParams m_params;
};

} // namespace

tcu::TestCaseGroup *createMapPlacedTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "map_placed"));

    const VkDeviceSize sizes[] = {
        4096,        // 1 page
        8192,        // 2 pages
        65536,       // 16 pages
        1024 * 1024, // 1 MB
    };

    // Use double mmap strategy to verify that the driver maps exactly the
    // expected number of pages, no extra padding on either side
    {
        de::MovePtr<tcu::TestCaseGroup> exactGroup(new tcu::TestCaseGroup(testCtx, "exact_size"));

        for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(sizes); i++)
        {
            TestParams params      = {sizes[i], false, false};
            const std::string name = std::string("size_") + de::toString(sizes[i]);
            exactGroup->addChild(new MapPlacedTestCase(testCtx, name, params, false));
        }

        group->addChild(exactGroup.release());
    }

    // CPU writes data through the placed map, GPU increments each value via compute
    // shader, CPU reads back and verifies. Confirms that the placed mapping is
    // accessible to both the CPU and the GPU without corruption
    {
        de::MovePtr<tcu::TestCaseGroup> gpuGroup(new tcu::TestCaseGroup(testCtx, "gpu_access"));

        TestParams params = {65536, false, true};
        gpuGroup->addChild(new MapPlacedTestCase(testCtx, "read_write", params, true));

        group->addChild(gpuGroup.release());
    }

    // MAP_PLACED + UNMAP_RESERVE with exact size verification
    // after UNMAP_RESERVE, verifies that only the Vulkan region is reserved
    // and that guard pages on both sides are still accessible.
    {
        de::MovePtr<tcu::TestCaseGroup> unmapReserveGroup(new tcu::TestCaseGroup(testCtx, "unmap_reserve"));

        for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(sizes); i++)
        {
            TestParams params      = {sizes[i], true, false};
            const std::string name = std::string("size_") + de::toString(sizes[i]);
            unmapReserveGroup->addChild(new MapPlacedTestCase(testCtx, name, params, false));
        }

        group->addChild(unmapReserveGroup.release());
    }

    // Map a memory object with the normal vkMapMemory then unmap with UNMAP_RESERVE
    // verifies that the address range remains reserved after the unmap
    // so that it cannot be recycled by a subsequent mmap
    {
        de::MovePtr<tcu::TestCaseGroup> normalUnmapGroup(new tcu::TestCaseGroup(testCtx, "normal_unmap_reserve"));

        for (size_t i = 0; i < DE_LENGTH_OF_ARRAY(sizes); i++)
        {
            TestParams params      = {sizes[i], false, false};
            const std::string name = std::string("size_") + de::toString(sizes[i]);
            normalUnmapGroup->addChild(new MapNormalUnmapReserveTestCase(testCtx, name, params));
        }

        group->addChild(normalUnmapGroup.release());
    }

    return group.release();
}

} // namespace memory
} // namespace vkt
