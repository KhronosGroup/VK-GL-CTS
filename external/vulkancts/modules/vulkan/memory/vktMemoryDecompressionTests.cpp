/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
 * Copyright (c) 2024 NVIDIA Corporation.
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
 *
 *//*!
* \file
* \brief Memory decompression tests.
*//*--------------------------------------------------------------------*/

#include "vktMemoryDecompressionTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

using std::vector;
using tcu::TestLog;
using namespace vk;

namespace vkt
{
namespace memory
{
namespace
{
enum TestModeType
{
    TEST_MODE_DIRECT   = 1 << 0,
    TEST_MODE_INDIRECT = 1 << 1,
};

typedef struct
{
    uint32_t decompressionCount;
    uint32_t executedDecompressionCount;
    uint32_t stride;
    const char *name;
} DecompressionParams;

template <class Instance>
class DecompressionTestCase : public TestCase
{
public:
    DecompressionTestCase(tcu::TestContext &context, const char *testName, const TestModeType testMode,
                          uint32_t compressionLevel, DecompressionParams decompressionParams,
                          const char *decompressedFilename)
        : TestCase(context, testName)
    {
        m_mode                 = testMode;
        m_compressionLevel     = compressionLevel;
        m_decompressionParams  = decompressionParams;
        m_decompressedFilename = decompressedFilename;
    }

private:
    TestInstance *createInstance(Context &context) const
    {
        return new Instance(context, m_mode, m_compressionLevel, m_decompressionParams, m_decompressedFilename);
    }

    TestModeType m_mode;
    uint32_t m_compressionLevel;
    DecompressionParams m_decompressionParams;
    const char *m_decompressedFilename;
};

class MemoryDecompressionTestInstance : public TestInstance
{
public:
    MemoryDecompressionTestInstance(Context &context, const TestModeType mode, const uint32_t compressionLevel,
                                    const DecompressionParams decompressionParams, const char *decompressedFilename);
    ~MemoryDecompressionTestInstance(void);

private:
    void init(void);
    uint8_t *loadDataFromFile(const char *filename, size_t *size);
    virtual tcu::TestStatus iterate(void);
    void replaceCRLFInPlace(uint8_t *data, size_t *size);

    size_t m_compressedSize{};
    size_t m_decompressedSize{};

    uint8_t *m_compressedData{};
    uint8_t *m_decompressedData{};

    TestModeType m_testMode;
    uint32_t m_compressionLevel;
    DecompressionParams m_decompressionParams;
    const char *m_decompressedFilename;
};

MemoryDecompressionTestInstance::MemoryDecompressionTestInstance(Context &context, const TestModeType mode,
                                                                 const uint32_t compressionLevel,
                                                                 const DecompressionParams decompressionParams,
                                                                 const char *decompressedFilename)
    : TestInstance(context)
    , m_testMode(mode)
    , m_compressionLevel(compressionLevel)
    , m_decompressionParams(decompressionParams)
    , m_decompressedFilename(decompressedFilename)
{
    init();
}

MemoryDecompressionTestInstance::~MemoryDecompressionTestInstance()
{
    delete m_compressedData;
    delete m_decompressedData;
}

void MemoryDecompressionTestInstance::replaceCRLFInPlace(uint8_t *buffer, size_t *size)
{
    size_t read = 0, write = 0;
    size_t length = *size;

    while (read < length)
    {
        if (buffer[read] == '\r' && read + 1 < length && buffer[read + 1] == '\n')
        {
            buffer[write++] = '\n';
            read += 2;
        }
        else
        {
            buffer[write++] = buffer[read++];
        }
    }

    *size = write;
}

uint8_t *MemoryDecompressionTestInstance::loadDataFromFile(const char *filename, size_t *size)
{
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        TCU_THROW(TestError, "Error opening file");
        return NULL;
    }
    // Seek to the end of the file to get the size
    if (fseek(fp, 0, SEEK_END) != 0)
    {
        TCU_THROW(TestError, "Error opening file");
        fclose(fp);
        return NULL;
    }
    size_t file_size = ftell(fp);
    if (file_size == 0)
    {
        TCU_THROW(TestError, "Error: Empty file or error getting file size");
        fclose(fp);
        return NULL;
    }
    *size = file_size;
    // Rewind the file pointer to the beginning
    if (fseek(fp, 0, SEEK_SET) != 0)
    {
        TCU_THROW(TestError, "Error rewinding file");
        fclose(fp);
        return NULL;
    }
    uint8_t *data = new uint8_t[file_size];
    if (data == NULL)
    {
        TCU_THROW(TestError, "Memory allocation failed");
        fclose(fp);
        return NULL;
    }

    size_t bytes_read = fread(data, sizeof(uint8_t), file_size, fp);
    if (bytes_read != file_size)
    {
        TCU_THROW(TestError, "Error reading file data");
    }
    fclose(fp);
    return data;
}

void MemoryDecompressionTestInstance::init(void)
{
    if (!m_context.isDeviceFunctionalitySupported("VK_EXT_memory_decompression"))
        TCU_THROW(NotSupportedError,
                  "Memory decompression tests are not supported, no memory decompression extension present.");

    const auto &decompressionFeatures = m_context.getMemoryDecompressionFeaturesEXT();
    if (!decompressionFeatures.memoryDecompression)
        TCU_THROW(NotSupportedError, "memory decompression feature not supported");

    const auto &decompressionProperties = m_context.getMemoryDecompressionPropertiesEXT();
    if (!(decompressionProperties.decompressionMethods & VK_MEMORY_DECOMPRESSION_METHOD_GDEFLATE_1_0_BIT_EXT))
        TCU_THROW(NotSupportedError, "Gdeflate 1.0 decompression format not supported");

    if (decompressionProperties.maxDecompressionIndirectCount < m_decompressionParams.decompressionCount)
        TCU_THROW(NotSupportedError, "Too many decompressions requested");

    char compressedFile[64];
    char decompressedFile[64];
    snprintf(compressedFile, 64, "./vulkan/data/gdeflate/compressed_%s_level_%u.gdef", m_decompressedFilename,
             m_compressionLevel);
    snprintf(decompressedFile, 64, "./vulkan/data/gdeflate/decompressed_%s.gdef", m_decompressedFilename);
    m_compressedData   = loadDataFromFile(compressedFile, &m_compressedSize);
    m_decompressedData = loadDataFromFile(decompressedFile, &m_decompressedSize);

    replaceCRLFInPlace(m_decompressedData, &m_decompressedSize);

    if (!m_compressedData || !m_decompressedData)
        TCU_THROW(NotSupportedError, "Could not read compressed/decompressed file");
}

tcu::TestStatus MemoryDecompressionTestInstance::iterate(void)
{
    const VkDevice device           = m_context.getDevice();
    const DeviceInterface &vkd      = m_context.getDeviceInterface();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // Number of executed decompressions must be <= total decompressions
    DE_ASSERT(m_decompressionParams.decompressionCount >= m_decompressionParams.executedDecompressionCount);

    const uint32_t stride                  = m_decompressionParams.stride;
    const size_t compressedSize            = static_cast<VkDeviceSize>(m_compressedSize);
    const size_t decompressedSizeAligned64 = 64 * ((m_decompressedSize + 63) / 64);
    const size_t totalDecompressedSize =
        static_cast<VkDeviceSize>(m_decompressionParams.decompressionCount * decompressedSizeAligned64);

    // Create buffers for compression/decompression/copy
    VkBufferUsageFlags2CreateInfoKHR bufferUsageFlags2 = vk::initVulkanStructure();

    bufferUsageFlags2.usage =
        VK_BUFFER_USAGE_2_MEMORY_DECOMPRESSION_BIT_EXT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    VkBufferCreateInfo srcBufferInfo = vk::initVulkanStructure();
    srcBufferInfo.pNext              = &bufferUsageFlags2;
    srcBufferInfo.size               = compressedSize;
    const BufferWithMemory srcBuffer(vkd, device, allocator, srcBufferInfo,
                                     MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);

    bufferUsageFlags2.usage = VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT_KHR | VK_BUFFER_USAGE_2_MEMORY_DECOMPRESSION_BIT_EXT |
                              VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT;
    VkBufferCreateInfo decompressBufferInfo = vk::initVulkanStructure();
    decompressBufferInfo.pNext              = &bufferUsageFlags2;
    decompressBufferInfo.size               = totalDecompressedSize;
    const BufferWithMemory decompressBuffer(vkd, device, allocator, decompressBufferInfo,
                                            MemoryRequirement::DeviceAddress);

    bufferUsageFlags2.usage          = VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR;
    VkBufferCreateInfo dstBufferInfo = vk::initVulkanStructure();
    dstBufferInfo.pNext              = &bufferUsageFlags2;
    dstBufferInfo.size               = totalDecompressedSize;
    const BufferWithMemory dstBuffer(vkd, device, allocator, dstBufferInfo, MemoryRequirement::HostVisible);

    const BufferWithMemory indirectBuffer(
        vkd, device, allocator,
        makeBufferCreateInfo(m_decompressionParams.decompressionCount * stride,
                             VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT),
        MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);

    const BufferWithMemory countBuffer(
        vkd, device, allocator,
        makeBufferCreateInfo(sizeof(uint32_t),
                             VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT),
        MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);

    VkBufferDeviceAddressInfo srcBufferAddressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr,
                                                   srcBuffer.get()};
    VkDeviceAddress srcBufferAddress = vkd.getBufferDeviceAddress(device, &srcBufferAddressInfo);

    VkBufferDeviceAddressInfo decompressBufferAddressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr,
                                                          decompressBuffer.get()};
    VkDeviceAddress decompressBufferAddress = vkd.getBufferDeviceAddress(device, &decompressBufferAddressInfo);

    VkBufferDeviceAddressInfo indirectBufferAddressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr,
                                                        indirectBuffer.get()};
    VkDeviceAddress indirectBufferAddress = vkd.getBufferDeviceAddress(device, &indirectBufferAddressInfo);

    VkBufferDeviceAddressInfo countBufferAddressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr,
                                                     countBuffer.get()};
    VkDeviceAddress countBufferAddress = vkd.getBufferDeviceAddress(device, &countBufferAddressInfo);

    // Create decompression parameters
    std::vector<VkDecompressMemoryRegionEXT> decompressRegions(m_decompressionParams.decompressionCount);
    for (uint32_t t = 0; t < m_decompressionParams.decompressionCount; t++)
    {
        decompressRegions[t].compressedSize   = compressedSize;
        decompressRegions[t].decompressedSize = m_decompressedSize;
        decompressRegions[t].srcAddress       = srcBufferAddress;
        decompressRegions[t].dstAddress       = decompressBufferAddress + t * decompressedSizeAligned64;
    }

    {
        const Allocation &bufferAllocation = srcBuffer.getAllocation();
        invalidateAlloc(vkd, device, bufferAllocation);
        deMemcpy(bufferAllocation.getHostPtr(), m_compressedData, compressedSize);
    }

    {
        const Allocation &bufferAllocation = dstBuffer.getAllocation();
        invalidateAlloc(vkd, device, bufferAllocation);
        deMemset(bufferAllocation.getHostPtr(), 0xFF, totalDecompressedSize);
    }

    {
        const Allocation &bufferAllocation = indirectBuffer.getAllocation();
        invalidateAlloc(vkd, device, bufferAllocation);
        uint8_t *hostPtr = (uint8_t *)bufferAllocation.getHostPtr();
        for (uint32_t t = 0; t < m_decompressionParams.decompressionCount; t++)
        {
            deMemcpy(hostPtr + t * stride, &decompressRegions[t], sizeof(VkDecompressMemoryRegionEXT));
        }
    }

    {
        const Allocation &bufferAllocation = countBuffer.getAllocation();
        invalidateAlloc(vkd, device, bufferAllocation);
        uint32_t *ptr = (uint32_t *)bufferAllocation.getHostPtr();
        *ptr          = m_decompressionParams.executedDecompressionCount;
    }

    const Unique<VkCommandPool> cmdPool(makeCommandPool(vkd, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    beginCommandBuffer(vkd, *cmdBuffer);

    // Decompress
    if (m_testMode == TEST_MODE_DIRECT)
    {
        const VkDecompressMemoryInfoEXT decompressioninfo = {
            VK_STRUCTURE_TYPE_DECOMPRESS_MEMORY_INFO_EXT, nullptr, VK_MEMORY_DECOMPRESSION_METHOD_GDEFLATE_1_0_BIT_EXT,
            m_decompressionParams.executedDecompressionCount, decompressRegions.data()};
        vkd.cmdDecompressMemoryEXT(*cmdBuffer, &decompressioninfo);
    }
    else
    {
        vkd.cmdDecompressMemoryIndirectCountEXT(*cmdBuffer, VK_MEMORY_DECOMPRESSION_METHOD_GDEFLATE_1_0_BIT_EXT,
                                                indirectBufferAddress, countBufferAddress,
                                                m_decompressionParams.decompressionCount, stride);
    }

    // Add barrier after decompression
    {
        VkMemoryBarrier2 barrier     = {VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                        NULL,
                                        VK_PIPELINE_STAGE_2_MEMORY_DECOMPRESSION_BIT_EXT,
                                        VK_ACCESS_2_MEMORY_DECOMPRESSION_WRITE_BIT_EXT,
                                        VK_PIPELINE_STAGE_2_MEMORY_DECOMPRESSION_BIT_EXT,
                                        VK_ACCESS_2_MEMORY_DECOMPRESSION_READ_BIT_EXT};
        VkDependencyInfoKHR dep_info = {
            VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR, // VkStructureType sType;
            nullptr,                               // const void* pNext;
            0u,                                    // VkDependencyFlags dependencyFlags;
            1u,                                    // uint32_t memoryBarrierCount;
            &barrier,                              // const VkMemoryBarrier2KHR* pMemoryBarriers;
            0u,                                    // uint32_t bufferMemoryBarrierCount;
            nullptr,                               // const VkBufferMemoryBarrier2KHR* pBufferMemoryBarriers;
            0u,                                    // uint32_t imageMemoryBarrierCount;
            nullptr,                               // const VkImageMemoryBarrier2KHR* pImageMemoryBarriers;
        };
        vkd.cmdPipelineBarrier2(*cmdBuffer, &dep_info);
    }

    // Copy decompressed data to dstBuffer
    {
        VkBufferCopy bufferRegion{};
        bufferRegion.size = totalDecompressedSize;
        vkd.cmdCopyBuffer(*cmdBuffer, decompressBuffer.get(), dstBuffer.get(), 1, &bufferRegion);
    }

    endCommandBuffer(vkd, *cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, *cmdBuffer);

    // Validate results
    bool passed = true;
    {
        const Allocation &bufferAllocation = dstBuffer.getAllocation();
        invalidateAlloc(vkd, device, bufferAllocation);
        uint8_t *bufferPtr = static_cast<uint8_t *>(bufferAllocation.getHostPtr());
        for (uint32_t t = 0; t < m_decompressionParams.executedDecompressionCount; t++)
        {
            passed &= !(deMemCmp(bufferPtr + t * decompressedSizeAligned64, m_decompressedData, m_decompressedSize));
        }
        for (uint32_t t = m_decompressionParams.executedDecompressionCount;
             t < m_decompressionParams.decompressionCount; t++)
        {
            passed &= !!(deMemCmp(bufferPtr + t * decompressedSizeAligned64, m_decompressedData, m_decompressedSize));
        }
    }
    if (passed)
    {
        return tcu::TestStatus(QP_TEST_RESULT_PASS, "Test passed");
    }
    return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Test failed");
}

} // namespace

tcu::TestCaseGroup *createMemoryDecompressionTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "decompression"));

    typedef struct
    {
        TestModeType testMode;
        const char *name;
    } TestModes;

    typedef struct
    {
        uint32_t compressionLevel;
        const char *name;
    } CompressionLevel;

    const TestModes testModes[] = {
        {TEST_MODE_DIRECT, "direct"},
        {TEST_MODE_INDIRECT, "indirect"},
    };
    const CompressionLevel levels[] = {
        {0, "compression_level_0"}, {6, "compression_level_6"}, {12, "compression_level_12"}};
    const char *decompressedSizes[] = {
        "17k",
        "64k",
    };

    const DecompressionParams decompressionParams[] = {
        {1, 1, sizeof(VkDecompressMemoryRegionEXT), "count_1_1"},
        {20, 12, sizeof(VkDecompressMemoryRegionEXT), "count_20_12"},
        {30, 30, sizeof(VkDecompressMemoryRegionEXT) + 64, "count_30_30_longstride"},
        {32, 32, sizeof(VkDecompressMemoryRegionEXT), "count_32_32"},
        {64, 64, sizeof(VkDecompressMemoryRegionEXT), "count_64_64"},
        {128, 128, sizeof(VkDecompressMemoryRegionEXT), "count_128_128"},
    };

    for (int modeIdx = 0; modeIdx < DE_LENGTH_OF_ARRAY(testModes); modeIdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> modeGroup(new tcu::TestCaseGroup(testCtx, testModes[modeIdx].name));
        for (int levelIdx = 0; levelIdx < DE_LENGTH_OF_ARRAY(levels); levelIdx++)
        {
            de::MovePtr<tcu::TestCaseGroup> levelGroup(new tcu::TestCaseGroup(testCtx, levels[levelIdx].name));
            for (int countIdx = 0; countIdx < DE_LENGTH_OF_ARRAY(decompressionParams); countIdx++)
            {
                de::MovePtr<tcu::TestCaseGroup> countGroup(
                    new tcu::TestCaseGroup(testCtx, decompressionParams[countIdx].name));
                for (int sizeIdx = 0; sizeIdx < DE_LENGTH_OF_ARRAY(decompressedSizes); sizeIdx++)
                {
                    char testName[32];
                    snprintf(testName, 32, "decompressed_size_%s", decompressedSizes[sizeIdx]);
                    countGroup->addChild(new DecompressionTestCase<MemoryDecompressionTestInstance>(
                        testCtx, testName, testModes[modeIdx].testMode, levels[levelIdx].compressionLevel,
                        decompressionParams[countIdx], decompressedSizes[sizeIdx]));
                }
                levelGroup->addChild(countGroup.release());
            }
            modeGroup->addChild(levelGroup.release());
        }
        group->addChild(modeGroup.release());
    }

    return group.release();
}

} // namespace memory
} // namespace vkt
