/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Imagination Technologies Ltd.
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
 * \brief Robust Vertex Buffer Access Tests
 *//*--------------------------------------------------------------------*/

#include "vktRobustnessVertexAccessTests.hpp"
#include "vktRobustnessUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "tcuTestLog.hpp"
#include "deMath.h"
#include "deUniquePtr.hpp"
#include <vector>

namespace vkt
{
namespace robustness
{

using namespace vk;

typedef std::vector<VkVertexInputBindingDescription> BindingList;
typedef std::vector<VkVertexInputAttributeDescription> AttributeList;

class VertexAccessTest : public vkt::TestCase
{
public:
    VertexAccessTest(tcu::TestContext &testContext, const std::string &name, VkFormat inputFormat,
                     uint32_t numVertexValues, uint32_t numInstanceValues, uint32_t numVertices, uint32_t numInstances);

    virtual ~VertexAccessTest(void)
    {
    }

    void initPrograms(SourceCollections &programCollection) const;
    void checkSupport(Context &context) const;
    TestInstance *createInstance(Context &context) const = 0;

protected:
    const VkFormat m_inputFormat;
    const uint32_t m_numVertexValues;
    const uint32_t m_numInstanceValues;
    const uint32_t m_numVertices;
    const uint32_t m_numInstances;
};

class DrawAccessTest : public VertexAccessTest
{
public:
    DrawAccessTest(tcu::TestContext &testContext, const std::string &name, VkFormat inputFormat,
                   uint32_t numVertexValues, uint32_t numInstanceValues, uint32_t numVertices, uint32_t numInstances);

    virtual ~DrawAccessTest(void)
    {
    }
    TestInstance *createInstance(Context &context) const;

protected:
};

class DrawIndexedAccessTest : public VertexAccessTest
{
public:
    enum IndexConfig
    {
        INDEX_CONFIG_LAST_INDEX_OUT_OF_BOUNDS,
        INDEX_CONFIG_INDICES_OUT_OF_BOUNDS,
        INDEX_CONFIG_TRIANGLE_OUT_OF_BOUNDS,

        INDEX_CONFIG_COUNT
    };

    const static std::vector<uint32_t> s_indexConfigs[INDEX_CONFIG_COUNT];

    DrawIndexedAccessTest(tcu::TestContext &testContext, const std::string &name, VkFormat inputFormat,
                          IndexConfig indexConfig);

    virtual ~DrawIndexedAccessTest(void)
    {
    }
    TestInstance *createInstance(Context &context) const;

protected:
    const IndexConfig m_indexConfig;
};

class VertexAccessInstance : public vkt::TestInstance
{
public:
    VertexAccessInstance(Context &context, Move<VkDevice> device,
#ifndef CTS_USES_VULKANSC
                         de::MovePtr<vk::DeviceDriver> deviceDriver,
#else
                         de::MovePtr<CustomInstance> customInstance,
                         de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter> deviceDriver,
#endif // CTS_USES_VULKANSC
                         VkFormat inputFormat, uint32_t numVertexValues, uint32_t numInstanceValues,
                         uint32_t numVertices, uint32_t numInstances, const std::vector<uint32_t> &indices);

    virtual ~VertexAccessInstance(void);
    virtual tcu::TestStatus iterate(void);
    virtual bool verifyResult(void);

private:
    bool isValueWithinVertexBufferOrZero(void *vertexBuffer, VkDeviceSize vertexBufferSize, const void *value,
                                         uint32_t valueIndexa);

protected:
    static bool isExpectedValueFromVertexBuffer(const void *vertexBuffer, uint32_t vertexIndex, VkFormat vertexFormat,
                                                const void *value);
    static VkDeviceSize getBufferSizeInBytes(uint32_t numScalars, VkFormat format);

    virtual void initVertexIds(uint32_t *indicesPtr, size_t indexCount) = 0;
    virtual uint32_t getIndex(uint32_t vertexNum) const                 = 0;

#ifndef CTS_USES_VULKANSC
    Move<VkDevice> m_device;
    de::MovePtr<vk::DeviceDriver> m_deviceDriver;
#else
    // Construction needs to happen in this exact order to ensure proper resource destruction
    de::MovePtr<CustomInstance> m_customInstance;
    Move<VkDevice> m_device;
    de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter> m_deviceDriver;
#endif // CTS_USES_VULKANSC

    const VkFormat m_inputFormat;
    const uint32_t m_numVertexValues;
    const uint32_t m_numInstanceValues;
    const uint32_t m_numVertices;
    const uint32_t m_numInstances;
    AttributeList m_vertexInputAttributes;
    BindingList m_vertexInputBindings;

    Move<VkBuffer> m_vertexRateBuffer;
    VkDeviceSize m_vertexRateBufferSize;
    de::MovePtr<Allocation> m_vertexRateBufferAlloc;
    VkDeviceSize m_vertexRateBufferAllocSize;

    Move<VkBuffer> m_instanceRateBuffer;
    VkDeviceSize m_instanceRateBufferSize;
    de::MovePtr<Allocation> m_instanceRateBufferAlloc;
    VkDeviceSize m_instanceRateBufferAllocSize;

    Move<VkBuffer> m_vertexNumBuffer;
    VkDeviceSize m_vertexNumBufferSize;
    de::MovePtr<Allocation> m_vertexNumBufferAlloc;

    Move<VkBuffer> m_indexBuffer;
    VkDeviceSize m_indexBufferSize;
    de::MovePtr<Allocation> m_indexBufferAlloc;

    Move<VkBuffer> m_outBuffer; // SSBO
    VkDeviceSize m_outBufferSize;
    de::MovePtr<Allocation> m_outBufferAlloc;
    VkDeviceSize m_outBufferAllocSize;

    Move<VkDescriptorPool> m_descriptorPool;
    Move<VkDescriptorSetLayout> m_descriptorSetLayout;
    Move<VkDescriptorSet> m_descriptorSet;

    Move<VkFence> m_fence;
    VkQueue m_queue;

    de::MovePtr<GraphicsEnvironment> m_graphicsTestEnvironment;
};

class DrawAccessInstance : public VertexAccessInstance
{
public:
    DrawAccessInstance(Context &context, Move<VkDevice> device,
#ifndef CTS_USES_VULKANSC
                       de::MovePtr<vk::DeviceDriver> deviceDriver,
#else
                       de::MovePtr<CustomInstance> customInstance,
                       de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter> deviceDriver,
#endif // CTS_USES_VULKANSC
                       VkFormat inputFormat, uint32_t numVertexValues, uint32_t numInstanceValues, uint32_t numVertices,
                       uint32_t numInstances);

    virtual ~DrawAccessInstance(void)
    {
    }

protected:
    virtual void initVertexIds(uint32_t *indicesPtr, size_t indexCount);
    virtual uint32_t getIndex(uint32_t vertexNum) const;
};

class DrawIndexedAccessInstance : public VertexAccessInstance
{
public:
    DrawIndexedAccessInstance(Context &context, Move<VkDevice> device,
#ifndef CTS_USES_VULKANSC
                              de::MovePtr<vk::DeviceDriver> deviceDriver,
#else
                              de::MovePtr<CustomInstance> customInstance,
                              de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter> deviceDriver,
#endif // CTS_USES_VULKANSC
                              VkFormat inputFormat, uint32_t numVertexValues, uint32_t numInstanceValues,
                              uint32_t numVertices, uint32_t numInstances, const std::vector<uint32_t> &indices);

    virtual ~DrawIndexedAccessInstance(void)
    {
    }

protected:
    virtual void initVertexIds(uint32_t *indicesPtr, size_t indexCount);
    virtual uint32_t getIndex(uint32_t vertexNum) const;

    const std::vector<uint32_t> m_indices;
};

// VertexAccessTest

VertexAccessTest::VertexAccessTest(tcu::TestContext &testContext, const std::string &name, VkFormat inputFormat,
                                   uint32_t numVertexValues, uint32_t numInstanceValues, uint32_t numVertices,
                                   uint32_t numInstances)

    : vkt::TestCase(testContext, name)
    , m_inputFormat(inputFormat)
    , m_numVertexValues(numVertexValues)
    , m_numInstanceValues(numInstanceValues)
    , m_numVertices(numVertices)
    , m_numInstances(numInstances)
{
}

void VertexAccessTest::checkSupport(Context &context) const
{
    if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") &&
        !context.getDeviceFeatures().robustBufferAccess)
        TCU_THROW(NotSupportedError,
                  "VK_KHR_portability_subset: robustBufferAccess not supported by this implementation");
}

void VertexAccessTest::initPrograms(SourceCollections &programCollection) const
{
    std::ostringstream attributeDeclaration;
    std::ostringstream attributeUse;

    std::ostringstream vertexShaderSource;
    std::ostringstream fragmentShaderSource;

    std::ostringstream attributeTypeStr;
    const int numChannels              = getNumUsedChannels(mapVkFormat(m_inputFormat).order);
    const uint32_t numScalarsPerVertex = numChannels * 3; // Use 3 identical attributes
    uint32_t numValues                 = 0;

    const bool isR64 = (m_inputFormat == VK_FORMAT_R64_UINT || m_inputFormat == VK_FORMAT_R64_SINT);

    if (numChannels == 1)
    {
        if (isUintFormat(m_inputFormat))
            attributeTypeStr << "uint";
        else if (isIntFormat(m_inputFormat))
            attributeTypeStr << "int";
        else
            attributeTypeStr << "float";

        attributeTypeStr << (isR64 ? "64_t" : " ");
    }
    else
    {
        if (isUintFormat(m_inputFormat))
            attributeTypeStr << "uvec";
        else if (isIntFormat(m_inputFormat))
            attributeTypeStr << "ivec";
        else
            attributeTypeStr << "vec";

        attributeTypeStr << numChannels;
    }

    for (int attrNdx = 0; attrNdx < 3; attrNdx++)
    {
        attributeDeclaration << "layout(location = " << attrNdx << ") in " << attributeTypeStr.str() << " attr"
                             << attrNdx << ";\n";

        for (int chanNdx = 0; chanNdx < numChannels; chanNdx++)
        {
            attributeUse << "\toutData[(gl_InstanceIndex * " << numScalarsPerVertex * m_numVertices
                         << ") + (vertexNum * " << numScalarsPerVertex << " + " << numValues++ << ")] = attr"
                         << attrNdx;

            if (numChannels == 1)
                attributeUse << ";\n";
            else
                attributeUse << "[" << chanNdx << "];\n";
        }
    }

    attributeDeclaration << "layout(location = 3) in int vertexNum;\n";

    attributeUse << "\n";

    std::string outType = "";
    if (isUintFormat(m_inputFormat))
        outType = "uint";
    else if (isIntFormat(m_inputFormat))
        outType = "int";
    else
        outType = "float";

    outType += isR64 ? "64_t" : "";

    std::string extensions = "";
    std::string version    = "#version 310 es\n";
    if (isR64)
    {
        extensions = "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require\n";
        version    = "#version 440\n";
    }

    vertexShaderSource << version << "precision highp float;\n"
                       << extensions << attributeDeclaration.str()
                       << "layout(set = 0, binding = 0, std430) buffer outBuffer\n"
                          "{\n"
                          "\t"
                       << outType << " outData[" << (m_numVertices * numValues) * m_numInstances
                       << "];\n"
                          "};\n\n"
                          "void main (void)\n"
                          "{\n"
                       << attributeUse.str()
                       << "\tgl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
                          "}\n";

    programCollection.glslSources.add("vertex") << glu::VertexSource(vertexShaderSource.str());

    fragmentShaderSource << "#version 310 es\n"
                            "precision highp float;\n"
                            "layout(location = 0) out vec4 fragColor;\n"
                            "void main (void)\n"
                            "{\n"
                            "\tfragColor = vec4(1.0);\n"
                            "}\n";

    programCollection.glslSources.add("fragment") << glu::FragmentSource(fragmentShaderSource.str());
}

// DrawAccessTest

DrawAccessTest::DrawAccessTest(tcu::TestContext &testContext, const std::string &name, VkFormat inputFormat,
                               uint32_t numVertexValues, uint32_t numInstanceValues, uint32_t numVertices,
                               uint32_t numInstances)

    : VertexAccessTest(testContext, name, inputFormat, numVertexValues, numInstanceValues, numVertices, numInstances)
{
}

TestInstance *DrawAccessTest::createInstance(Context &context) const
{
#ifdef CTS_USES_VULKANSC
    de::MovePtr<CustomInstance> customInstance(new CustomInstance(createCustomInstanceFromContext(context)));
#endif // CTS_USES_VULKANSC

    Move<VkDevice> device = createRobustBufferAccessDevice(context
#ifdef CTS_USES_VULKANSC
                                                           ,
                                                           *customInstance
#endif // CTS_USES_VULKANSC
    );
#ifndef CTS_USES_VULKANSC
    de::MovePtr<vk::DeviceDriver> deviceDriver = de::MovePtr<DeviceDriver>(
        new DeviceDriver(context.getPlatformInterface(), context.getInstance(), *device, context.getUsedApiVersion(),
                         context.getTestContext().getCommandLine()));
#else
    de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter> deviceDriver =
        de::MovePtr<DeviceDriverSC, DeinitDeviceDeleter>(
            new DeviceDriverSC(context.getPlatformInterface(), *customInstance, *device,
                               context.getTestContext().getCommandLine(), context.getResourceInterface(),
                               context.getDeviceVulkanSC10Properties(), context.getDeviceProperties(),
                               context.getUsedApiVersion()),
            vk::DeinitDeviceDeleter(context.getResourceInterface().get(), *device));
#endif // CTS_USES_VULKANSC

    return new DrawAccessInstance(context, device,
#ifdef CTS_USES_VULKANSC
                                  customInstance,
#endif // CTS_USES_VULKANSC
                                  deviceDriver, m_inputFormat, m_numVertexValues, m_numInstanceValues, m_numVertices,
                                  m_numInstances);
}

// DrawIndexedAccessTest

const uint32_t lastIndexOutOfBounds[] = {
    0, 1, 2, 3, 4, 100, // Indices of 100 and above are out of bounds
};
const uint32_t indicesOutOfBounds[] = {
    0, 100, 2, 101, 3, 102, // Indices of 100 and above are out of bounds
};
const uint32_t triangleOutOfBounds[] = {
    100, 101, 102, 3, 4, 5, // Indices of 100 and above are out of bounds
};

const std::vector<uint32_t> DrawIndexedAccessTest::s_indexConfigs[INDEX_CONFIG_COUNT] = {
    std::vector<uint32_t>(lastIndexOutOfBounds, lastIndexOutOfBounds + DE_LENGTH_OF_ARRAY(lastIndexOutOfBounds)),
    std::vector<uint32_t>(indicesOutOfBounds, indicesOutOfBounds + DE_LENGTH_OF_ARRAY(indicesOutOfBounds)),
    std::vector<uint32_t>(triangleOutOfBounds, triangleOutOfBounds + DE_LENGTH_OF_ARRAY(triangleOutOfBounds)),
};

DrawIndexedAccessTest::DrawIndexedAccessTest(tcu::TestContext &testContext, const std::string &name,
                                             VkFormat inputFormat, IndexConfig indexConfig)

    : VertexAccessTest(testContext, name, inputFormat,
                       getNumUsedChannels(mapVkFormat(inputFormat).order) *
                           (uint32_t)s_indexConfigs[indexConfig].size() * 2, // numVertexValues
                       getNumUsedChannels(mapVkFormat(inputFormat).order),   // numInstanceValues
                       (uint32_t)s_indexConfigs[indexConfig].size(),         // numVertices
                       1)                                                    // numInstances
    , m_indexConfig(indexConfig)
{
}

TestInstance *DrawIndexedAccessTest::createInstance(Context &context) const
{
#ifdef CTS_USES_VULKANSC
    de::MovePtr<CustomInstance> customInstance(new CustomInstance(createCustomInstanceFromContext(context)));
#endif // CTS_USES_VULKANSC

    Move<VkDevice> device = createRobustBufferAccessDevice(context
#ifdef CTS_USES_VULKANSC
                                                           ,
                                                           *customInstance
#endif // CTS_USES_VULKANSC
    );
#ifndef CTS_USES_VULKANSC
    de::MovePtr<vk::DeviceDriver> deviceDriver = de::MovePtr<DeviceDriver>(
        new DeviceDriver(context.getPlatformInterface(), context.getInstance(), *device, context.getUsedApiVersion(),
                         context.getTestContext().getCommandLine()));
#else
    de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter> deviceDriver =
        de::MovePtr<DeviceDriverSC, DeinitDeviceDeleter>(
            new DeviceDriverSC(context.getPlatformInterface(), context.getInstance(), *device,
                               context.getTestContext().getCommandLine(), context.getResourceInterface(),
                               context.getDeviceVulkanSC10Properties(), context.getDeviceProperties(),
                               context.getUsedApiVersion()),
            vk::DeinitDeviceDeleter(context.getResourceInterface().get(), *device));
#endif // CTS_USES_VULKANSC

    return new DrawIndexedAccessInstance(context, device,
#ifdef CTS_USES_VULKANSC
                                         customInstance,
#endif // CTS_USES_VULKANSC
                                         deviceDriver, m_inputFormat, m_numVertexValues, m_numInstanceValues,
                                         m_numVertices, m_numInstances, s_indexConfigs[m_indexConfig]);
}

// VertexAccessInstance

VertexAccessInstance::VertexAccessInstance(Context &context, Move<VkDevice> device,
#ifndef CTS_USES_VULKANSC
                                           de::MovePtr<vk::DeviceDriver> deviceDriver,
#else
                                           de::MovePtr<CustomInstance> customInstance,
                                           de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter> deviceDriver,
#endif // CTS_USES_VULKANSC
                                           VkFormat inputFormat, uint32_t numVertexValues, uint32_t numInstanceValues,
                                           uint32_t numVertices, uint32_t numInstances,
                                           const std::vector<uint32_t> &indices)

    : vkt::TestInstance(context)
#ifdef CTS_USES_VULKANSC
    , m_customInstance(customInstance)
#endif // CTS_USES_VULKANSC
    , m_device(device)
    , m_deviceDriver(deviceDriver)
    , m_inputFormat(inputFormat)
    , m_numVertexValues(numVertexValues)
    , m_numInstanceValues(numInstanceValues)
    , m_numVertices(numVertices)
    , m_numInstances(numInstances)
{
    const DeviceInterface &vk       = *m_deviceDriver;
    const uint32_t queueFamilyIndex = context.getUniversalQueueFamilyIndex();
    const auto &vki                 = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice =
        chooseDevice(vki, context.getInstance(), context.getTestContext().getCommandLine());
    SimpleAllocator memAlloc(vk, *m_device, getPhysicalDeviceMemoryProperties(vki, physicalDevice));
    const uint32_t formatSizeInBytes = tcu::getPixelSize(mapVkFormat(m_inputFormat));

    // Check storage support
    if (!context.getDeviceFeatures().vertexPipelineStoresAndAtomics)
    {
        TCU_THROW(NotSupportedError, "Stores not supported in vertex stage");
    }

    if (m_inputFormat == VK_FORMAT_R64_UINT || m_inputFormat == VK_FORMAT_R64_SINT)
    {
        const VkFormatProperties formatProperties =
            getPhysicalDeviceFormatProperties(vki, physicalDevice, m_inputFormat);
        context.requireDeviceFunctionality("VK_EXT_shader_image_atomic_int64");

        if ((formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT) !=
            VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT)
            TCU_THROW(NotSupportedError, "VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT not supported");
    }

    const VkVertexInputAttributeDescription attributes[] = {
        // input rate: vertex
        {
            0u,            // uint32_t location;
            0u,            // uint32_t binding;
            m_inputFormat, // VkFormat format;
            0u,            // uint32_t offset;
        },
        {
            1u,                // uint32_t location;
            0u,                // uint32_t binding;
            m_inputFormat,     // VkFormat format;
            formatSizeInBytes, // uint32_t offset;
        },

        // input rate: instance
        {
            2u,            // uint32_t location;
            1u,            // uint32_t binding;
            m_inputFormat, // VkFormat format;
            0u,            // uint32_t offset;
        },

        // Attribute for vertex number
        {
            3u,                 // uint32_t location;
            2u,                 // uint32_t binding;
            VK_FORMAT_R32_SINT, // VkFormat format;
            0,                  // uint32_t offset;
        },
    };

    const VkVertexInputBindingDescription bindings[] = {
        {
            0u,                         // uint32_t binding;
            formatSizeInBytes * 2,      // uint32_t stride;
            VK_VERTEX_INPUT_RATE_VERTEX // VkVertexInputRate inputRate;
        },
        {
            1u,                           // uint32_t binding;
            formatSizeInBytes,            // uint32_t stride;
            VK_VERTEX_INPUT_RATE_INSTANCE // VkVertexInputRate inputRate;
        },
        {
            2u,                         // uint32_t binding;
            sizeof(int32_t),            // uint32_t stride;
            VK_VERTEX_INPUT_RATE_VERTEX // VkVertexInputRate inputRate;
        },
    };

    m_vertexInputBindings =
        std::vector<VkVertexInputBindingDescription>(bindings, bindings + DE_LENGTH_OF_ARRAY(bindings));
    m_vertexInputAttributes =
        std::vector<VkVertexInputAttributeDescription>(attributes, attributes + DE_LENGTH_OF_ARRAY(attributes));

    // Create vertex buffer for vertex input rate
    {
        VkMemoryRequirements bufferMemoryReqs;

        m_vertexRateBufferSize = getBufferSizeInBytes(
            m_numVertexValues, m_inputFormat); // All formats used in this test suite are 32-bit based.

        const VkBufferCreateInfo vertexRateBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            m_vertexRateBufferSize,               // VkDeviceSize size;
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,    // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            1u,                                   // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
        };

        m_vertexRateBuffer          = createBuffer(vk, *m_device, &vertexRateBufferParams);
        bufferMemoryReqs            = getBufferMemoryRequirements(vk, *m_device, *m_vertexRateBuffer);
        m_vertexRateBufferAllocSize = bufferMemoryReqs.size;
        m_vertexRateBufferAlloc     = memAlloc.allocate(bufferMemoryReqs, MemoryRequirement::HostVisible);

        VK_CHECK(vk.bindBufferMemory(*m_device, *m_vertexRateBuffer, m_vertexRateBufferAlloc->getMemory(),
                                     m_vertexRateBufferAlloc->getOffset()));
        populateBufferWithTestValues(m_vertexRateBufferAlloc->getHostPtr(), (uint32_t)m_vertexRateBufferAllocSize,
                                     m_inputFormat);
        flushMappedMemoryRange(vk, *m_device, m_vertexRateBufferAlloc->getMemory(),
                               m_vertexRateBufferAlloc->getOffset(), VK_WHOLE_SIZE);
    }

    // Create vertex buffer for instance input rate
    {
        VkMemoryRequirements bufferMemoryReqs;

        m_instanceRateBufferSize = getBufferSizeInBytes(
            m_numInstanceValues, m_inputFormat); // All formats used in this test suite are 32-bit based.

        const VkBufferCreateInfo instanceRateBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            m_instanceRateBufferSize,             // VkDeviceSize size;
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,    // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            1u,                                   // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
        };

        m_instanceRateBuffer          = createBuffer(vk, *m_device, &instanceRateBufferParams);
        bufferMemoryReqs              = getBufferMemoryRequirements(vk, *m_device, *m_instanceRateBuffer);
        m_instanceRateBufferAllocSize = bufferMemoryReqs.size;
        m_instanceRateBufferAlloc     = memAlloc.allocate(bufferMemoryReqs, MemoryRequirement::HostVisible);

        VK_CHECK(vk.bindBufferMemory(*m_device, *m_instanceRateBuffer, m_instanceRateBufferAlloc->getMemory(),
                                     m_instanceRateBufferAlloc->getOffset()));
        populateBufferWithTestValues(m_instanceRateBufferAlloc->getHostPtr(), (uint32_t)m_instanceRateBufferAllocSize,
                                     m_inputFormat);
        flushMappedMemoryRange(vk, *m_device, m_instanceRateBufferAlloc->getMemory(),
                               m_instanceRateBufferAlloc->getOffset(), VK_WHOLE_SIZE);
    }

    // Create vertex buffer that stores the vertex number (from 0 to m_numVertices - 1)
    {
        m_vertexNumBufferSize = 128 * sizeof(int32_t); // Allocate enough device memory for all indices (0 to 127).

        const VkBufferCreateInfo vertexNumBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            m_vertexNumBufferSize,                // VkDeviceSize size;
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,    // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            1u,                                   // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
        };

        m_vertexNumBuffer      = createBuffer(vk, *m_device, &vertexNumBufferParams);
        m_vertexNumBufferAlloc = memAlloc.allocate(getBufferMemoryRequirements(vk, *m_device, *m_vertexNumBuffer),
                                                   MemoryRequirement::HostVisible);

        VK_CHECK(vk.bindBufferMemory(*m_device, *m_vertexNumBuffer, m_vertexNumBufferAlloc->getMemory(),
                                     m_vertexNumBufferAlloc->getOffset()));
    }

    // Create index buffer if required
    if (!indices.empty())
    {
        m_indexBufferSize = sizeof(uint32_t) * indices.size();

        const VkBufferCreateInfo indexBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            m_indexBufferSize,                    // VkDeviceSize size;
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,     // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            1u,                                   // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
        };

        m_indexBuffer      = createBuffer(vk, *m_device, &indexBufferParams);
        m_indexBufferAlloc = memAlloc.allocate(getBufferMemoryRequirements(vk, *m_device, *m_indexBuffer),
                                               MemoryRequirement::HostVisible);

        VK_CHECK(vk.bindBufferMemory(*m_device, *m_indexBuffer, m_indexBufferAlloc->getMemory(),
                                     m_indexBufferAlloc->getOffset()));
        deMemcpy(m_indexBufferAlloc->getHostPtr(), indices.data(), (size_t)m_indexBufferSize);
        flushMappedMemoryRange(vk, *m_device, m_indexBufferAlloc->getMemory(), m_indexBufferAlloc->getOffset(),
                               VK_WHOLE_SIZE);
    }

    // Create result ssbo
    {
        const int numChannels = getNumUsedChannels(mapVkFormat(m_inputFormat).order);

        m_outBufferSize = getBufferSizeInBytes(m_numVertices * m_numInstances * numChannels * 3, VK_FORMAT_R32_UINT);

        const VkBufferCreateInfo outBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            m_outBufferSize,                      // VkDeviceSize size;
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,   // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            1u,                                   // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
        };

        m_outBuffer                             = createBuffer(vk, *m_device, &outBufferParams);
        const VkMemoryRequirements requirements = getBufferMemoryRequirements(vk, *m_device, *m_outBuffer);
        m_outBufferAlloc                        = memAlloc.allocate(requirements, MemoryRequirement::HostVisible);
        m_outBufferAllocSize                    = requirements.size;

        VK_CHECK(
            vk.bindBufferMemory(*m_device, *m_outBuffer, m_outBufferAlloc->getMemory(), m_outBufferAlloc->getOffset()));
        deMemset(m_outBufferAlloc->getHostPtr(), 0xFF, (size_t)m_outBufferSize);
        flushMappedMemoryRange(vk, *m_device, m_outBufferAlloc->getMemory(), m_outBufferAlloc->getOffset(),
                               VK_WHOLE_SIZE);
    }

    // Create descriptor set data
    {
        DescriptorPoolBuilder descriptorPoolBuilder;
        descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u);
        m_descriptorPool =
            descriptorPoolBuilder.build(vk, *m_device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

        DescriptorSetLayoutBuilder setLayoutBuilder;
        setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
        m_descriptorSetLayout = setLayoutBuilder.build(vk, *m_device);

        const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType sType;
            nullptr,                                        // const void* pNext;
            *m_descriptorPool,                              // VkDescriptorPool desciptorPool;
            1u,                                             // uint32_t setLayoutCount;
            &m_descriptorSetLayout.get()                    // const VkDescriptorSetLayout* pSetLayouts;
        };

        m_descriptorSet = allocateDescriptorSet(vk, *m_device, &descriptorSetAllocateInfo);

        const VkDescriptorBufferInfo outBufferDescriptorInfo =
            makeDescriptorBufferInfo(*m_outBuffer, 0ull, VK_WHOLE_SIZE);

        DescriptorSetUpdateBuilder setUpdateBuilder;
        setUpdateBuilder.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0),
                                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outBufferDescriptorInfo);
        setUpdateBuilder.update(vk, *m_device);
    }

    // Create fence
    {
        const VkFenceCreateInfo fenceParams = {
            VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, // VkStructureType sType;
            nullptr,                             // const void* pNext;
            0u                                   // VkFenceCreateFlags flags;
        };

        m_fence = createFence(vk, *m_device, &fenceParams);
    }

    // Get queue
    vk.getDeviceQueue(*m_device, queueFamilyIndex, 0, &m_queue);

    // Setup graphics test environment
    {
        GraphicsEnvironment::DrawConfig drawConfig;

        drawConfig.vertexBuffers.push_back(*m_vertexRateBuffer);
        drawConfig.vertexBuffers.push_back(*m_instanceRateBuffer);
        drawConfig.vertexBuffers.push_back(*m_vertexNumBuffer);

        drawConfig.vertexCount   = m_numVertices;
        drawConfig.instanceCount = m_numInstances;
        drawConfig.indexBuffer   = *m_indexBuffer;
        drawConfig.indexCount    = (uint32_t)(m_indexBufferSize / sizeof(uint32_t));

        m_graphicsTestEnvironment = de::MovePtr<GraphicsEnvironment>(new GraphicsEnvironment(
            m_context, *m_deviceDriver, *m_device, *m_descriptorSetLayout, *m_descriptorSet,
            GraphicsEnvironment::VertexBindings(bindings, bindings + DE_LENGTH_OF_ARRAY(bindings)),
            GraphicsEnvironment::VertexAttributes(attributes, attributes + DE_LENGTH_OF_ARRAY(attributes)),
            drawConfig));
    }
}

VertexAccessInstance::~VertexAccessInstance(void)
{
}

tcu::TestStatus VertexAccessInstance::iterate(void)
{
    const DeviceInterface &vk           = *m_deviceDriver;
    const vk::VkCommandBuffer cmdBuffer = m_graphicsTestEnvironment->getCommandBuffer();

    // Initialize vertex ids
    {
        uint32_t *bufferPtr = reinterpret_cast<uint32_t *>(m_vertexNumBufferAlloc->getHostPtr());
        deMemset(bufferPtr, 0, (size_t)m_vertexNumBufferSize);

        initVertexIds(bufferPtr, (size_t)(m_vertexNumBufferSize / sizeof(uint32_t)));

        flushMappedMemoryRange(vk, *m_device, m_vertexNumBufferAlloc->getMemory(), m_vertexNumBufferAlloc->getOffset(),
                               VK_WHOLE_SIZE);
    }

    // Submit command buffer
    {
        const VkSubmitInfo submitInfo = {
            VK_STRUCTURE_TYPE_SUBMIT_INFO, // VkStructureType sType;
            nullptr,                       // const void* pNext;
            0u,                            // uint32_t waitSemaphoreCount;
            nullptr,                       // const VkSemaphore* pWaitSemaphores;
            nullptr,                       // const VkPIpelineStageFlags* pWaitDstStageMask;
            1u,                            // uint32_t commandBufferCount;
            &cmdBuffer,                    // const VkCommandBuffer* pCommandBuffers;
            0u,                            // uint32_t signalSemaphoreCount;
            nullptr                        // const VkSemaphore* pSignalSemaphores;
        };

        VK_CHECK(vk.resetFences(*m_device, 1, &m_fence.get()));
        VK_CHECK(vk.queueSubmit(m_queue, 1, &submitInfo, *m_fence));
        VK_CHECK(vk.waitForFences(*m_device, 1, &m_fence.get(), true, ~(0ull) /* infinity */));
    }

    // Prepare result buffer for read
    {
        const VkMappedMemoryRange outBufferRange = {
            VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, //  VkStructureType sType;
            nullptr,                               //  const void* pNext;
            m_outBufferAlloc->getMemory(),         //  VkDeviceMemory mem;
            0ull,                                  //  VkDeviceSize offset;
            m_outBufferAllocSize,                  //  VkDeviceSize size;
        };

        VK_CHECK(vk.invalidateMappedMemoryRanges(*m_device, 1u, &outBufferRange));
    }

    if (verifyResult())
        return tcu::TestStatus::pass("All values OK");
    else
        return tcu::TestStatus::fail("Invalid value(s) found");
}

bool VertexAccessInstance::verifyResult(void)
{
    std::ostringstream logMsg;
    const DeviceInterface &vk          = *m_deviceDriver;
    tcu::TestLog &log                  = m_context.getTestContext().getLog();
    const uint32_t numChannels         = getNumUsedChannels(mapVkFormat(m_inputFormat).order);
    const uint32_t numScalarsPerVertex = numChannels * 3; // Use 3 identical attributes
    void *outDataPtr                   = m_outBufferAlloc->getHostPtr();
    const uint32_t outValueSize        = static_cast<uint32_t>(
        (m_inputFormat == VK_FORMAT_R64_UINT || m_inputFormat == VK_FORMAT_R64_SINT) ? sizeof(uint64_t) :
                                                                                              sizeof(uint32_t));
    bool allOk = true;

    const VkMappedMemoryRange outBufferRange = {
        VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, // VkStructureType sType;
        nullptr,                               // const void* pNext;
        m_outBufferAlloc->getMemory(),         // VkDeviceMemory mem;
        m_outBufferAlloc->getOffset(),         // VkDeviceSize offset;
        m_outBufferAllocSize,                  // VkDeviceSize size;
    };

    VK_CHECK(vk.invalidateMappedMemoryRanges(*m_device, 1u, &outBufferRange));

    for (uint32_t valueNdx = 0; valueNdx < m_outBufferSize / outValueSize; valueNdx++)
    {
        uint32_t numInBufferValues;
        void *inBufferPtr;
        VkDeviceSize inBufferAllocSize;
        uint32_t inBufferValueIndex;
        bool isOutOfBoundsAccess      = false;
        const uint32_t attributeIndex = (valueNdx / numChannels) % 3;
        uint32_t *ptr32               = (uint32_t *)outDataPtr;
        uint64_t *ptr64               = (uint64_t *)outDataPtr;
        const void *outValuePtr =
            ((m_inputFormat == VK_FORMAT_R64_UINT || m_inputFormat == VK_FORMAT_R64_SINT) ? (void *)(ptr64 + valueNdx) :
                                                                                            (void *)(ptr32 + valueNdx));

        if (attributeIndex == 2)
        {
            // Instance rate
            const uint32_t elementIndex = valueNdx / (numScalarsPerVertex * m_numVertices); // instance id

            numInBufferValues  = m_numInstanceValues;
            inBufferPtr        = m_instanceRateBufferAlloc->getHostPtr();
            inBufferAllocSize  = m_instanceRateBufferAllocSize;
            inBufferValueIndex = (elementIndex * numChannels) + (valueNdx % numScalarsPerVertex) - (2 * numChannels);
        }
        else
        {
            // Vertex rate
            const uint32_t vertexNdx    = valueNdx / numScalarsPerVertex;
            const uint32_t instanceNdx  = vertexNdx / m_numVertices;
            const uint32_t elementIndex = valueNdx / numScalarsPerVertex; // vertex id

            numInBufferValues  = m_numVertexValues;
            inBufferPtr        = m_vertexRateBufferAlloc->getHostPtr();
            inBufferAllocSize  = m_vertexRateBufferAllocSize;
            inBufferValueIndex = (getIndex(elementIndex) * (numChannels * 2)) + (valueNdx % numScalarsPerVertex) -
                                 instanceNdx * (m_numVertices * numChannels * 2);

            // Binding 0 contains two attributes, so bounds checking for attribute 0 must also consider attribute 1 to determine if the binding is out of bounds.
            if ((attributeIndex == 0) && (numInBufferValues >= numChannels))
                numInBufferValues -= numChannels;
        }

        isOutOfBoundsAccess = (inBufferValueIndex >= numInBufferValues);

        const int32_t distanceToOutOfBounds =
            (int32_t)outValueSize * ((int32_t)numInBufferValues - (int32_t)inBufferValueIndex);

        if (!isOutOfBoundsAccess && (distanceToOutOfBounds < 16))
            isOutOfBoundsAccess = (((inBufferValueIndex / numChannels) + 1) * numChannels > numInBufferValues);

        // Log value information
        {
            // Vertex separator
            if (valueNdx && valueNdx % numScalarsPerVertex == 0)
                logMsg << "\n";

            logMsg << "\n" << valueNdx << ": Value ";

            // Result index and value
            if (m_inputFormat == VK_FORMAT_A2B10G10R10_UNORM_PACK32)
                logValue(logMsg, outValuePtr, VK_FORMAT_R32_SFLOAT, 4);
            else
                logValue(logMsg, outValuePtr, m_inputFormat, 4);

            // Attribute name
            logMsg << "\tfrom attr" << attributeIndex;
            if (numChannels > 1)
                logMsg << "[" << valueNdx % numChannels << "]";

            // Input rate
            if (attributeIndex == 2)
                logMsg << "\tinstance rate";
            else
                logMsg << "\tvertex rate";
        }

        if (isOutOfBoundsAccess)
        {
            const bool isValidValue =
                isValueWithinVertexBufferOrZero(inBufferPtr, inBufferAllocSize, outValuePtr, inBufferValueIndex);

            logMsg << "\t(out of bounds)";

            if (!isValidValue)
            {
                // Check if we are satisfying the [0, 0, 0, x] pattern, where x may be either 0 or 1,
                // or the maximum representable positive integer value (if the format is integer-based).

                const bool canMatchVec4Pattern =
                    ((valueNdx % numChannels == 3) || m_inputFormat == VK_FORMAT_A2B10G10R10_UNORM_PACK32);
                bool matchesVec4Pattern = false;

                if (canMatchVec4Pattern)
                {
                    if (m_inputFormat == VK_FORMAT_A2B10G10R10_UNORM_PACK32)
                        matchesVec4Pattern = verifyOutOfBoundsVec4(outValuePtr, m_inputFormat);
                    else
                        matchesVec4Pattern = verifyOutOfBoundsVec4(((uint32_t *)outValuePtr) - 3, m_inputFormat);
                }

                if (!canMatchVec4Pattern || !matchesVec4Pattern)
                {
                    logMsg << ", Failed: expected a value within the buffer range or 0";

                    if (canMatchVec4Pattern)
                        logMsg << ", or the [0, 0, 0, x] pattern";

                    allOk = false;
                }
            }
        }
        else if (!isExpectedValueFromVertexBuffer(inBufferPtr, inBufferValueIndex, m_inputFormat, outValuePtr))
        {
            logMsg << ", Failed: unexpected value";
            allOk = false;
        }
    }
    log << tcu::TestLog::Message << logMsg.str() << tcu::TestLog::EndMessage;

    return allOk;
}

bool VertexAccessInstance::isValueWithinVertexBufferOrZero(void *vertexBuffer, VkDeviceSize vertexBufferSize,
                                                           const void *value, uint32_t valueIndex)
{
    if (m_inputFormat == VK_FORMAT_A2B10G10R10_UNORM_PACK32)
    {
        const float normValue      = *reinterpret_cast<const float *>(value);
        const uint32_t scalarIndex = valueIndex % 4;
        const bool isAlpha         = (scalarIndex == 3);
        uint32_t encodedValue;

        if (isAlpha)
            encodedValue = deMin32(uint32_t(deFloatRound(normValue * 0x3u)), 0x3u);
        else
            encodedValue = deMin32(uint32_t(deFloatRound(normValue * 0x3FFu)), 0x3FFu);

        if (encodedValue == 0)
            return true;

        for (uint32_t i = 0; i < vertexBufferSize / 4; i++)
        {
            const uint32_t packedValue = reinterpret_cast<uint32_t *>(vertexBuffer)[i];
            uint32_t unpackedValue;

            if (scalarIndex < 3)
                unpackedValue = (packedValue >> (10 * scalarIndex)) & 0x3FFu;
            else
                unpackedValue = (packedValue >> 30) & 0x3u;

            if (unpackedValue == encodedValue)
                return true;
        }

        return false;
    }
    else
    {
        return isValueWithinBufferOrZero(vertexBuffer, vertexBufferSize, value, sizeof(uint32_t));
    }
}

bool VertexAccessInstance::isExpectedValueFromVertexBuffer(const void *vertexBuffer, uint32_t vertexIndex,
                                                           VkFormat vertexFormat, const void *value)
{
    if (isUintFormat(vertexFormat))
    {
        if (vertexFormat == VK_FORMAT_R64_UINT || vertexFormat == VK_FORMAT_R64_SINT)
        {
            const uint64_t *bufferPtr = reinterpret_cast<const uint64_t *>(vertexBuffer);
            return bufferPtr[vertexIndex] == *reinterpret_cast<const uint64_t *>(value);
        }
        else
        {
            const uint32_t *bufferPtr = reinterpret_cast<const uint32_t *>(vertexBuffer);
            return bufferPtr[vertexIndex] == *reinterpret_cast<const uint32_t *>(value);
        }
    }
    else if (isIntFormat(vertexFormat))
    {
        if (vertexFormat == VK_FORMAT_R64_UINT || vertexFormat == VK_FORMAT_R64_SINT)
        {
            const int64_t *bufferPtr = reinterpret_cast<const int64_t *>(vertexBuffer);
            return bufferPtr[vertexIndex] == *reinterpret_cast<const int64_t *>(value);
        }
        else
        {
            const int32_t *bufferPtr = reinterpret_cast<const int32_t *>(vertexBuffer);
            return bufferPtr[vertexIndex] == *reinterpret_cast<const int32_t *>(value);
        }
    }
    else if (isFloatFormat(vertexFormat))
    {
        const float *bufferPtr = reinterpret_cast<const float *>(vertexBuffer);

        return areEqual(bufferPtr[vertexIndex], *reinterpret_cast<const float *>(value));
    }
    else if (vertexFormat == VK_FORMAT_A2B10G10R10_UNORM_PACK32)
    {
        const uint32_t *bufferPtr  = reinterpret_cast<const uint32_t *>(vertexBuffer);
        const uint32_t packedValue = bufferPtr[vertexIndex / 4];
        const uint32_t scalarIndex = vertexIndex % 4;
        float normValue;

        if (scalarIndex < 3)
            normValue = float((packedValue >> (10 * scalarIndex)) & 0x3FFu) / 0x3FFu;
        else
            normValue = float(packedValue >> 30) / 0x3u;

        return areEqual(normValue, *reinterpret_cast<const float *>(value));
    }

    DE_ASSERT(false);
    return false;
}

VkDeviceSize VertexAccessInstance::getBufferSizeInBytes(uint32_t numScalars, VkFormat format)
{
    if (isUintFormat(format) || isIntFormat(format) || isFloatFormat(format))
    {
        return numScalars * ((format == VK_FORMAT_R64_UINT || format == VK_FORMAT_R64_SINT) ? 8 : 4);
    }
    else if (format == VK_FORMAT_A2B10G10R10_UNORM_PACK32)
    {
        DE_ASSERT(numScalars % 4 == 0);
        return numScalars;
    }

    DE_ASSERT(false);
    return 0;
}

// DrawAccessInstance

DrawAccessInstance::DrawAccessInstance(Context &context, Move<VkDevice> device,
#ifndef CTS_USES_VULKANSC
                                       de::MovePtr<vk::DeviceDriver> deviceDriver,
#else
                                       de::MovePtr<CustomInstance> customInstance,
                                       de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter> deviceDriver,
#endif // CTS_USES_VULKANSC
                                       VkFormat inputFormat, uint32_t numVertexValues, uint32_t numInstanceValues,
                                       uint32_t numVertices, uint32_t numInstances)
    : VertexAccessInstance(context, device,
#ifdef CTS_USES_VULKANSC
                           customInstance,
#endif // CTS_USES_VULKANSC
                           deviceDriver, inputFormat, numVertexValues, numInstanceValues, numVertices, numInstances,
                           std::vector<uint32_t>()) // No index buffer
{
}

void DrawAccessInstance::initVertexIds(uint32_t *indicesPtr, size_t indexCount)
{
    for (uint32_t i = 0; i < indexCount; i++)
        indicesPtr[i] = i;
}

uint32_t DrawAccessInstance::getIndex(uint32_t vertexNum) const
{
    return vertexNum;
}

// DrawIndexedAccessInstance

DrawIndexedAccessInstance::DrawIndexedAccessInstance(Context &context, Move<VkDevice> device,
#ifndef CTS_USES_VULKANSC
                                                     de::MovePtr<vk::DeviceDriver> deviceDriver,
#else
                                                     de::MovePtr<CustomInstance> customInstance,
                                                     de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter>
                                                         deviceDriver,
#endif // CTS_USES_VULKANSC
                                                     VkFormat inputFormat, uint32_t numVertexValues,
                                                     uint32_t numInstanceValues, uint32_t numVertices,
                                                     uint32_t numInstances, const std::vector<uint32_t> &indices)
    : VertexAccessInstance(context, device,
#ifdef CTS_USES_VULKANSC
                           customInstance,
#endif // CTS_USES_VULKANSC
                           deviceDriver, inputFormat, numVertexValues, numInstanceValues, numVertices, numInstances,
                           indices)
    , m_indices(indices)
{
}

void DrawIndexedAccessInstance::initVertexIds(uint32_t *indicesPtr, size_t indexCount)
{
    DE_UNREF(indexCount);

    for (uint32_t i = 0; i < m_indices.size(); i++)
    {
        DE_ASSERT(m_indices[i] < indexCount);

        indicesPtr[m_indices[i]] = i;
    }
}

uint32_t DrawIndexedAccessInstance::getIndex(uint32_t vertexNum) const
{
    DE_ASSERT(vertexNum < (uint32_t)m_indices.size());

    return m_indices[vertexNum];
}

// Test node creation functions

static tcu::TestCaseGroup *createDrawTests(tcu::TestContext &testCtx, VkFormat format)
{
    struct TestConfig
    {
        std::string name;
        VkFormat inputFormat;
        uint32_t numVertexValues;
        uint32_t numInstanceValues;
        uint32_t numVertices;
        uint32_t numInstances;
    };

    const uint32_t numChannels = getNumUsedChannels(mapVkFormat(format).order);

    const TestConfig testConfigs[] = {
        // name                                            format    numVertexValues            numInstanceValues    numVertices        numInstances
        // Create data for 6 vertices, draw 9 vertices
        {"vertex_out_of_bounds", format, numChannels * 2 * 6, numChannels, 9, 1},
        // Create data for half a vertex, draw 3 vertices
        {"vertex_incomplete", format, numChannels, numChannels, 3, 1},
        // Create data for 1 instance, draw 3 instances
        {"instance_out_of_bounds", format, numChannels * 2 * 9, numChannels, 3, 3},
    };

    de::MovePtr<tcu::TestCaseGroup> drawTests(new tcu::TestCaseGroup(testCtx, "draw"));

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(testConfigs); i++)
    {
        const TestConfig &config = testConfigs[i];

        drawTests->addChild(new DrawAccessTest(testCtx, config.name, config.inputFormat, config.numVertexValues,
                                               config.numInstanceValues, config.numVertices, config.numInstances));
    }

    return drawTests.release();
}

static tcu::TestCaseGroup *createDrawIndexedTests(tcu::TestContext &testCtx, VkFormat format)
{
    struct TestConfig
    {
        std::string name;
        VkFormat inputFormat;
        DrawIndexedAccessTest::IndexConfig indexConfig;
    };

    const TestConfig testConfigs[] = {
        // name                                format        indexConfig
        // Only last index is out of bounds
        {"last_index_out_of_bounds", format, DrawIndexedAccessTest::INDEX_CONFIG_LAST_INDEX_OUT_OF_BOUNDS},
        // Random indices out of bounds
        {"indices_out_of_bounds", format, DrawIndexedAccessTest::INDEX_CONFIG_INDICES_OUT_OF_BOUNDS},
        // First triangle is out of bounds
        {"triangle_out_of_bounds", format, DrawIndexedAccessTest::INDEX_CONFIG_TRIANGLE_OUT_OF_BOUNDS},
    };

    de::MovePtr<tcu::TestCaseGroup> drawTests(new tcu::TestCaseGroup(testCtx, "draw_indexed"));

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(testConfigs); i++)
    {
        const TestConfig &config = testConfigs[i];

        drawTests->addChild(new DrawIndexedAccessTest(testCtx, config.name, config.inputFormat, config.indexConfig));
    }

    return drawTests.release();
}

static void addVertexFormatTests(tcu::TestContext &testCtx, tcu::TestCaseGroup *parentGroup)
{
    const VkFormat vertexFormats[] = {VK_FORMAT_R32_UINT,
                                      VK_FORMAT_R32_SINT,
                                      VK_FORMAT_R32_SFLOAT,
                                      VK_FORMAT_R32G32_UINT,
                                      VK_FORMAT_R32G32_SINT,
                                      VK_FORMAT_R32G32_SFLOAT,
                                      VK_FORMAT_R32G32B32_UINT,
                                      VK_FORMAT_R32G32B32_SINT,
                                      VK_FORMAT_R32G32B32_SFLOAT,
                                      VK_FORMAT_R32G32B32A32_UINT,
                                      VK_FORMAT_R32G32B32A32_SINT,
                                      VK_FORMAT_R32G32B32A32_SFLOAT,
                                      VK_FORMAT_R64_UINT,
                                      VK_FORMAT_R64_SINT,

                                      VK_FORMAT_A2B10G10R10_UNORM_PACK32};

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(vertexFormats); i++)
    {
        const std::string formatName = getFormatName(vertexFormats[i]);
        de::MovePtr<tcu::TestCaseGroup> formatGroup(
            new tcu::TestCaseGroup(testCtx, de::toLower(formatName.substr(10)).c_str()));

        formatGroup->addChild(createDrawTests(testCtx, vertexFormats[i]));
        formatGroup->addChild(createDrawIndexedTests(testCtx, vertexFormats[i]));

        parentGroup->addChild(formatGroup.release());
    }
}

tcu::TestCaseGroup *createVertexAccessTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> vertexAccessTests(new tcu::TestCaseGroup(testCtx, "vertex_access"));

    addVertexFormatTests(testCtx, vertexAccessTests.get());

    return vertexAccessTests.release();
}

} // namespace robustness
} // namespace vkt
