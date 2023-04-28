/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2018-2024 NVIDIA Corporation
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
 * \brief Vulkan Cooperative Vector tests
 *//*--------------------------------------------------------------------*/

#include "vktCooperativeVectorBasicTests.hpp"
#include "vktCooperativeVectorUtils.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkRayTracingUtil.hpp"

#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"

#include "deDefs.h"
#include "deFloat16.h"
#include "deMath.h"
#include "deRandom.h"
#include "deSharedPtr.hpp"
#include "deString.h"

#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"

#include <string>
#include <sstream>
#include <set>
#include <algorithm>

#define COOPERATIVE_VECTOR_EXTENDED_DEBUG 1

namespace vkt
{
namespace cooperative_vector
{
namespace
{
using namespace vk;
using namespace std;

struct CaseDef
{
    VkComponentTypeKHR matrixType;
    VkCooperativeVectorMatrixLayoutNV matrixLayout[4];
    bool hostConvert;
};

class CooperativeVectorLayoutTestInstance : public TestInstance
{
public:
    CooperativeVectorLayoutTestInstance(Context &context, const CaseDef &data);
    ~CooperativeVectorLayoutTestInstance(void);
    tcu::TestStatus iterate(void);

private:
    CaseDef m_data;
};

CooperativeVectorLayoutTestInstance::CooperativeVectorLayoutTestInstance(Context &context, const CaseDef &data)
    : vkt::TestInstance(context)
    , m_data(data)
{
}

CooperativeVectorLayoutTestInstance::~CooperativeVectorLayoutTestInstance(void)
{
}

class CooperativeVectorLayoutTestCase : public TestCase
{
public:
    CooperativeVectorLayoutTestCase(tcu::TestContext &context, const char *name, const CaseDef data);
    ~CooperativeVectorLayoutTestCase(void);
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    CaseDef m_data;
};

CooperativeVectorLayoutTestCase::CooperativeVectorLayoutTestCase(tcu::TestContext &context, const char *name,
                                                                 const CaseDef data)
    : vkt::TestCase(context, name)
    , m_data(data)
{
}

CooperativeVectorLayoutTestCase::~CooperativeVectorLayoutTestCase(void)
{
}

void CooperativeVectorLayoutTestCase::checkSupport(Context &context) const
{
    if (!context.contextSupports(vk::ApiVersion(0, 1, 1, 0)))
    {
        TCU_THROW(NotSupportedError, "Vulkan 1.1 not supported");
    }

    if (!context.getCooperativeVectorFeaturesNV().cooperativeVector)
    {
        TCU_THROW(NotSupportedError, "cooperativeVector not supported");
    }

    uint32_t propertyCount = 0;
    std::vector<VkCooperativeVectorPropertiesNV> properties;
    context.getInstanceInterface().getPhysicalDeviceCooperativeVectorPropertiesNV(context.getPhysicalDevice(),
                                                                                  &propertyCount, DE_NULL);
    if (propertyCount == 0)
        TCU_THROW(NotSupportedError, "cooperative vectors not supported");

    properties.resize(propertyCount);

    for (uint32_t i = 0; i < propertyCount; ++i)
    {
        VkCooperativeVectorPropertiesNV *p = &properties[i];
        p->sType                           = VK_STRUCTURE_TYPE_COOPERATIVE_VECTOR_PROPERTIES_NV;
        p->pNext                           = DE_NULL;
    }

    context.getInstanceInterface().getPhysicalDeviceCooperativeVectorPropertiesNV(context.getPhysicalDevice(),
                                                                                  &propertyCount, properties.data());

    for (uint32_t i = 0; i < propertyCount; ++i)
    {
        VkCooperativeVectorPropertiesNV *p = &properties[i];
        if (p->matrixInterpretation == m_data.matrixType || m_data.matrixType == VK_COMPONENT_TYPE_FLOAT32_NV)
        {
            return;
        }
    }

    TCU_THROW(NotSupportedError, "matrix type not supported");
}

TestInstance *CooperativeVectorLayoutTestCase::createInstance(Context &context) const
{
    return new CooperativeVectorLayoutTestInstance(context, m_data);
}

// Test layout conversion. Convert from row-major to another layout to another
// layout then back to row major. For any of those layouts that are row- or
// col-major, verify that the values are correct.
tcu::TestStatus CooperativeVectorLayoutTestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();
    qpTestResult finalres     = QP_TEST_RESULT_PASS;

    deRandom rnd;
    deRandom_init(&rnd, 1234);

    VkDeviceSize bufferSize = 1024 * 1024;
    de::MovePtr<BufferWithMemory> buffer;
    try
    {
        buffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
            vk, device, allocator,
            makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT),
            MemoryRequirement::HostVisible | MemoryRequirement::Cached | MemoryRequirement::Coherent |
                MemoryRequirement::DeviceAddress));
    }
    catch (const tcu::NotSupportedError &)
    {
        buffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
            vk, device, allocator,
            makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT),
            MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress));
    }

    VkBufferDeviceAddressInfo bdaInfo{
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, // VkStructureType  sType;
        DE_NULL,                                      // const void*  pNext;
        **buffer,                                     // VkBuffer            buffer
    };
    VkDeviceAddress bufferDeviceAddress;
    bufferDeviceAddress = vk.getBufferDeviceAddress(device, &bdaInfo);

    const VkQueue queue         = m_context.getUniversalQueue();
    Move<VkCommandPool> cmdPool = createCommandPool(vk, device, 0, m_context.getUniversalQueueFamilyIndex());

    VkComponentTypeKHR matrixTypes[4] = {
        m_data.matrixType,
        m_data.matrixType,
        m_data.matrixType,
        m_data.matrixType,
    };

    // Convert to fp16 for the output, since we can't write to row/col-major fp8
    if (m_data.matrixType == VK_COMPONENT_TYPE_FLOAT_E4M3_NV || m_data.matrixType == VK_COMPONENT_TYPE_FLOAT_E5M2_NV)
    {
        matrixTypes[3] = VK_COMPONENT_TYPE_FLOAT16_KHR;
    }

    for (uint32_t numRows = 1; numRows <= 32; ++numRows)
    {
        for (uint32_t numColumns = 1; numColumns <= 32; ++numColumns)
        {
            int matrixElementSize   = getComponentTypeInfo(m_data.matrixType).bits / 8;
            uint32_t rowMajorStride = (numColumns * matrixElementSize + (16 - 1)) & ~(16 - 1);
            uint32_t colMajorStride = (numRows * matrixElementSize + (16 - 1)) & ~(16 - 1);

            size_t rowMajorSize = numRows * rowMajorStride;

            uint32_t matrixOffsets[4]{};
            size_t matrixSizes[4]{};
            uint32_t matrixStrides[4]{};

            matrixOffsets[0] = 128;
            matrixStrides[0] = rowMajorStride;
            matrixSizes[0]   = rowMajorSize;

            void *ptr = buffer->getAllocation().getHostPtr();

            for (uint32_t i = 0; i < numRows; ++i)
            {
                for (uint32_t j = 0; j < numColumns; ++j)
                {
                    if (isFloatType(m_data.matrixType))
                    {
                        setDataFloatOffsetIndex(ptr, m_data.matrixType, matrixOffsets[0] + i * matrixStrides[0], j,
                                                ((float)(deRandom_getUint32(&rnd) & 0xff) - 64.0f) / 2.0f);
                    }
                    else
                    {
                        setDataIntOffsetIndex(ptr, m_data.matrixType, matrixOffsets[0] + i * matrixStrides[0], j,
                                              (deRandom_getUint32(&rnd) & 0xff));
                    }
                }
            }

            Move<VkCommandBuffer> cmdBuffer =
                allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

            beginCommandBuffer(vk, *cmdBuffer, 0u);

            for (uint32_t m = 1; m < 4; ++m)
            {
                matrixOffsets[m] = (uint32_t)(matrixOffsets[m - 1] + matrixSizes[m - 1] + (64 - 1)) & ~(64 - 1);

                matrixElementSize = getComponentTypeInfo(matrixTypes[m]).bits / 8;
                rowMajorStride    = (numColumns * matrixElementSize + (16 - 1)) & ~(16 - 1);
                colMajorStride    = (numRows * matrixElementSize + (16 - 1)) & ~(16 - 1);

                matrixStrides[m] =
                    m_data.matrixLayout[m] == VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV    ? rowMajorStride :
                    m_data.matrixLayout[m] == VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_COLUMN_MAJOR_NV ? colMajorStride :
                                                                                                    0;
                VkConvertCooperativeVectorMatrixInfoNV info = {
                    VK_STRUCTURE_TYPE_CONVERT_COOPERATIVE_VECTOR_MATRIX_INFO_NV, // VkStructureType                       sType;
                    DE_NULL,                    // void const*                           pNext;
                    matrixSizes[m - 1],         // size_t                                srcSize;
                    {0},                        // VkDeviceOrHostAddressConstKHR         srcData;
                    &matrixSizes[m],            // size_t*                               pDstSize;
                    {0},                        // VkDeviceOrHostAddressKHR              dstData;
                    matrixTypes[m - 1],         // VkComponentTypeKHR                    srcComponentType;
                    matrixTypes[m],             // VkComponentTypeKHR                    dstComponentType;
                    numRows,                    // uint32_t                              numRows;
                    numColumns,                 // uint32_t                              numColumns;
                    m_data.matrixLayout[m - 1], // VkCooperativeVectorMatrixLayoutNV     srcLayout;
                    matrixStrides[m - 1],       // size_t                                srcStride;
                    m_data.matrixLayout[m],     // VkCooperativeVectorMatrixLayoutNV     dstLayout;
                    matrixStrides[m],           // size_t                                dstStride;
                };

                VK_CHECK(vk.convertCooperativeVectorMatrixNV(device, &info));

                if (m_data.hostConvert)
                {
                    info.srcData.hostAddress = (uint8_t *)(ptr) + matrixOffsets[m - 1];
                    info.dstData.hostAddress = (uint8_t *)(ptr) + matrixOffsets[m];
                    VK_CHECK(vk.convertCooperativeVectorMatrixNV(device, &info));
                }
                else
                {
                    info.srcData.deviceAddress = bufferDeviceAddress + matrixOffsets[m - 1];
                    info.dstData.deviceAddress = bufferDeviceAddress + matrixOffsets[m];
                    vk.cmdConvertCooperativeVectorMatrixNV(*cmdBuffer, 1, &info);

                    VkMemoryBarrier2KHR memoryBarrier = {
                        VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR,                       // sType
                        DE_NULL,                                                      // pNext
                        VK_PIPELINE_STAGE_2_CONVERT_COOPERATIVE_VECTOR_MATRIX_BIT_NV, // srcStageMask
                        VK_ACCESS_2_TRANSFER_WRITE_BIT,                               // srcAccessMask
                        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,                         // dstStageMask
                        VK_ACCESS_2_SHADER_READ_BIT                                   // dstAccessMask
                    };

                    VkDependencyInfoKHR dependencyInfo{
                        VK_STRUCTURE_TYPE_DEPENDENCY_INFO, // sType
                        DE_NULL,                           // pNext
                        0,                                 //dependency flags
                        1,                                 //memory barrier count
                        &memoryBarrier,                    //memory barrier
                        0,                                 // bufferMemoryBarrierCount
                        DE_NULL,                           // pBufferMemoryBarriers
                        0,                                 // imageMemoryBarrierCount
                        DE_NULL,                           // pImageMemoryBarriers
                    };
                    vk.cmdPipelineBarrier2(*cmdBuffer, &dependencyInfo);
                }
            }

            flushAlloc(vk, device, buffer->getAllocation());

            endCommandBuffer(vk, *cmdBuffer);

            submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

            invalidateAlloc(vk, device, buffer->getAllocation());

            qpTestResult res = QP_TEST_RESULT_PASS;

            for (uint32_t i = 0; i < numRows; ++i)
            {
                for (uint32_t j = 0; j < numColumns; ++j)
                {
                    float srcF   = 0.0f;
                    int64_t srcI = 0;
                    float dstF   = 0.0f;
                    int64_t dstI = 0;
                    if (isFloatType(matrixTypes[0]))
                    {
                        srcF = getDataFloatOffsetIndex(ptr, matrixTypes[0], matrixOffsets[0] + i * matrixStrides[0], j);
                    }
                    else
                    {
                        srcI = getDataIntOffsetIndex(ptr, matrixTypes[0], matrixOffsets[0] + i * matrixStrides[0], j);
                    }

                    for (uint32_t m = 1; m < 4; ++m)
                    {
                        if (m_data.matrixLayout[m] == VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV ||
                            m_data.matrixLayout[m] == VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_COLUMN_MAJOR_NV)
                        {
                            if (isFloatType(matrixTypes[m]))
                            {
                                if (m_data.matrixLayout[m] == VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_COLUMN_MAJOR_NV)
                                {
                                    dstF = getDataFloatOffsetIndex(ptr, matrixTypes[m],
                                                                   matrixOffsets[m] + j * matrixStrides[m], i);
                                }
                                else
                                {
                                    dstF = getDataFloatOffsetIndex(ptr, matrixTypes[m],
                                                                   matrixOffsets[m] + i * matrixStrides[m], j);
                                }
                                if (srcF != dstF)
                                {
                                    //printf("numRows %d numCols %d m %d i %d j %d src %f dst %f\n", numRows, numColumns, m, i, j, srcF, dstF);
                                    res = QP_TEST_RESULT_FAIL;
                                }
                            }
                            else
                            {
                                if (m_data.matrixLayout[m] == VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_COLUMN_MAJOR_NV)
                                {
                                    dstI = getDataIntOffsetIndex(ptr, matrixTypes[m],
                                                                 matrixOffsets[m] + j * matrixStrides[m], i);
                                }
                                else
                                {
                                    dstI = getDataIntOffsetIndex(ptr, matrixTypes[m],
                                                                 matrixOffsets[m] + i * matrixStrides[m], j);
                                }
                                if (srcI != dstI)
                                {
                                    res = QP_TEST_RESULT_FAIL;
                                }
                            }
                        }
                    }
                }
            }

            if (res != QP_TEST_RESULT_PASS)
            {
                finalres = res;
            }
        }
    }
    return tcu::TestStatus(finalres, qpGetTestResultName(finalres));
}

} // namespace

template <uint32_t N>
struct TestGroupCaseN
{
    uint32_t value[N];
    const char *name;
    const char *description;
};

tcu::TestCaseGroup *createCooperativeVectorMatrixLayoutTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(
        new tcu::TestCaseGroup(testCtx, "layoutconvert", "cooperative_vector matrix convert layout tests"));

    TestGroupCaseN<1> dtCases[] = {
        {{VK_COMPONENT_TYPE_FLOAT32_NV}, "float32", "float32"},
        {{VK_COMPONENT_TYPE_FLOAT16_NV}, "float16", "float16"},
        {{VK_COMPONENT_TYPE_UINT8_NV}, "uint8", "uint8"},
        {{VK_COMPONENT_TYPE_SINT8_NV}, "sint8", "sint8"},
        {{VK_COMPONENT_TYPE_FLOAT_E4M3_NV}, "floate4m3", "floate4m3"},
        {{VK_COMPONENT_TYPE_FLOAT_E5M2_NV}, "floate5m2", "floate5m2"},
    };

    TestGroupCaseN<1> colCases[] = {
        {{VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV}, "rowMajor", "Row major"},
        {{VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_COLUMN_MAJOR_NV}, "colMajor", "Column major"},
        {{VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_INFERENCING_OPTIMAL_NV}, "inferencingOptimal", "Inferencing Optimal"},
        {{VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_TRAINING_OPTIMAL_NV}, "trainingOptimal", "Training Optimal"},
    };

    TestGroupCaseN<1> hostCases[] = {
        {{false}, "device", "device"},
        {{true}, "host", "host"},
    };

    for (int hostNdx = 0; hostNdx < DE_LENGTH_OF_ARRAY(hostCases); hostNdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> hostGroup(
            new tcu::TestCaseGroup(testCtx, hostCases[hostNdx].name, hostCases[hostNdx].description));
        for (int dtNdx = 0; dtNdx < DE_LENGTH_OF_ARRAY(dtCases); dtNdx++)
        {
            de::MovePtr<tcu::TestCaseGroup> dtGroup(
                new tcu::TestCaseGroup(testCtx, dtCases[dtNdx].name, dtCases[dtNdx].description));
            for (int colNdx = 0; colNdx < DE_LENGTH_OF_ARRAY(colCases); colNdx++)
            {
                de::MovePtr<tcu::TestCaseGroup> colGroup(
                    new tcu::TestCaseGroup(testCtx, colCases[colNdx].name, colCases[colNdx].description));
                for (int colNdx2 = 0; colNdx2 < DE_LENGTH_OF_ARRAY(colCases); colNdx2++)
                {

                    if (dtCases[dtNdx].value[0] == VK_COMPONENT_TYPE_FLOAT_E4M3_NV ||
                        dtCases[dtNdx].value[0] == VK_COMPONENT_TYPE_FLOAT_E5M2_NV)
                    {
                        if (colCases[colNdx].value[0] == VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV ||
                            colCases[colNdx].value[0] == VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_COLUMN_MAJOR_NV ||
                            colCases[colNdx2].value[0] == VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV ||
                            colCases[colNdx2].value[0] == VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_COLUMN_MAJOR_NV)
                        {
                            // FP8 can only be written in optimal layout
                            continue;
                        }
                    }

                    CaseDef c = {
                        (VkComponentTypeKHR)dtCases[dtNdx].value[0], // VkComponentTypeKHR matrixType;
                        {VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV, // VkCooperativeVectorMatrixLayoutNV matrixLayout[4];
                         (VkCooperativeVectorMatrixLayoutNV)colCases[colNdx].value[0],
                         (VkCooperativeVectorMatrixLayoutNV)colCases[colNdx2].value[0],
                         VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV},
                        !!hostCases[hostNdx].value[0], // bool hostConvert;
                    };
                    colGroup->addChild(new CooperativeVectorLayoutTestCase(testCtx, colCases[colNdx2].name, c));
                }
                dtGroup->addChild(colGroup.release());
            }
            hostGroup->addChild(dtGroup.release());
        }
        group->addChild(hostGroup.release());
    }
    return group.release();
}

namespace
{

struct CaseDef2
{
    VkComponentTypeKHR matrixType[2];
    bool hostConvert;
};

class CooperativeVectorTypeConversionTestInstance : public TestInstance
{
public:
    CooperativeVectorTypeConversionTestInstance(Context &context, const CaseDef2 &data);
    ~CooperativeVectorTypeConversionTestInstance(void);
    tcu::TestStatus iterate(void);

private:
    CaseDef2 m_data;
};

CooperativeVectorTypeConversionTestInstance::CooperativeVectorTypeConversionTestInstance(Context &context,
                                                                                         const CaseDef2 &data)
    : vkt::TestInstance(context)
    , m_data(data)
{
}

CooperativeVectorTypeConversionTestInstance::~CooperativeVectorTypeConversionTestInstance(void)
{
}

class CooperativeVectorTypeConversionTestCase : public TestCase
{
public:
    CooperativeVectorTypeConversionTestCase(tcu::TestContext &context, const char *name, const CaseDef2 data);
    ~CooperativeVectorTypeConversionTestCase(void);
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    CaseDef2 m_data;
};

CooperativeVectorTypeConversionTestCase::CooperativeVectorTypeConversionTestCase(tcu::TestContext &context,
                                                                                 const char *name, const CaseDef2 data)
    : vkt::TestCase(context, name)
    , m_data(data)
{
}

CooperativeVectorTypeConversionTestCase::~CooperativeVectorTypeConversionTestCase(void)
{
}

void CooperativeVectorTypeConversionTestCase::checkSupport(Context &context) const
{
    if (!context.contextSupports(vk::ApiVersion(0, 1, 1, 0)))
    {
        TCU_THROW(NotSupportedError, "Vulkan 1.1 not supported");
    }

    if (!context.getCooperativeVectorFeaturesNV().cooperativeVector)
    {
        TCU_THROW(NotSupportedError, "cooperativeVector not supported");
    }

    uint32_t propertyCount = 0;
    std::vector<VkCooperativeVectorPropertiesNV> properties;
    context.getInstanceInterface().getPhysicalDeviceCooperativeVectorPropertiesNV(context.getPhysicalDevice(),
                                                                                  &propertyCount, DE_NULL);
    if (propertyCount == 0)
        TCU_THROW(NotSupportedError, "cooperative vectors not supported");

    properties.resize(propertyCount);

    for (uint32_t i = 0; i < propertyCount; ++i)
    {
        VkCooperativeVectorPropertiesNV *p = &properties[i];
        p->sType                           = VK_STRUCTURE_TYPE_COOPERATIVE_VECTOR_PROPERTIES_NV;
        p->pNext                           = DE_NULL;
    }

    context.getInstanceInterface().getPhysicalDeviceCooperativeVectorPropertiesNV(context.getPhysicalDevice(),
                                                                                  &propertyCount, properties.data());

    bool supported[2]{};

    for (uint32_t i = 0; i < propertyCount; ++i)
    {
        VkCooperativeVectorPropertiesNV *p = &properties[i];
        if (p->matrixInterpretation == m_data.matrixType[0] || m_data.matrixType[0] == VK_COMPONENT_TYPE_FLOAT32_NV)
        {
            supported[0] = true;
        }
        if (p->matrixInterpretation == m_data.matrixType[1])
        {
            supported[1] = true;
        }
    }

    if (!supported[0] || !supported[1])
        TCU_THROW(NotSupportedError, "matrix type not supported");
}

TestInstance *CooperativeVectorTypeConversionTestCase::createInstance(Context &context) const
{
    return new CooperativeVectorTypeConversionTestInstance(context, m_data);
}

// Test type conversion. Generate an input 1xN matrix that has all possible
// values of the input type, convert it to another type in optimal layout,
// then convert to fp16 in row-major layout and verify the values are as
// expected.
tcu::TestStatus CooperativeVectorTypeConversionTestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();

    deRandom rnd;
    deRandom_init(&rnd, 1234);

    uint32_t srcElementSize;
    switch (m_data.matrixType[0])
    {
    case VK_COMPONENT_TYPE_FLOAT32_NV:
        srcElementSize = 4;
        break;
    case VK_COMPONENT_TYPE_FLOAT16_NV:
        srcElementSize = 2;
        break;
    default:
        srcElementSize = 1;
        break;
    }
    uint32_t dstElementSize = (srcElementSize == 4) ? 4 : 2;
    VkComponentTypeKHR dstComponentType =
        (dstElementSize == 4) ? VK_COMPONENT_TYPE_FLOAT32_NV : VK_COMPONENT_TYPE_FLOAT16_NV;

    uint32_t numElements = (srcElementSize == 4) ? (1 << 16) : (1 << (8 * srcElementSize));

    size_t optimalSize                          = 0;
    VkConvertCooperativeVectorMatrixInfoNV info = {
        VK_STRUCTURE_TYPE_CONVERT_COOPERATIVE_VECTOR_MATRIX_INFO_NV, // VkStructureType                       sType;
        DE_NULL,                                                     // void const*                           pNext;
        numElements * srcElementSize,                                // size_t                                srcSize;
        {0},                                                         // VkDeviceOrHostAddressConstKHR         srcData;
        &optimalSize,                                                // size_t*                               pDstSize;
        {0},                                                         // VkDeviceOrHostAddressKHR              dstData;
        m_data.matrixType[0],                             // VkComponentTypeKHR                    srcComponentType;
        m_data.matrixType[1],                             // VkComponentTypeKHR                    dstComponentType;
        1,                                                // uint32_t                              numRows;
        numElements,                                      // uint32_t                              numColumns;
        VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV, // VkCooperativeVectorMatrixLayoutNV     srcLayout;
        numElements * srcElementSize,                     // size_t                                srcStride;
        VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_INFERENCING_OPTIMAL_NV, // VkCooperativeVectorMatrixLayoutNV     dstLayout;
        0,                                                          // size_t                                dstStride;
    };

    VK_CHECK(vk.convertCooperativeVectorMatrixNV(device, &info));

    VkDeviceSize bufferSize = numElements * (srcElementSize + dstElementSize) + optimalSize;

    uint32_t optimalOffset = numElements * srcElementSize;
    uint32_t dstOffset     = (uint32_t)(optimalOffset + optimalSize);

    de::MovePtr<BufferWithMemory> buffer;
    try
    {
        buffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
            vk, device, allocator,
            makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT),
            MemoryRequirement::HostVisible | MemoryRequirement::Cached | MemoryRequirement::Coherent |
                MemoryRequirement::DeviceAddress));
    }
    catch (const tcu::NotSupportedError &)
    {
        buffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
            vk, device, allocator,
            makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT),
            MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress));
    }

    VkBufferDeviceAddressInfo bdinfo{
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, // VkStructureType  sType;
        DE_NULL,                                      // const void*  pNext;
        **buffer,                                     // VkBuffer            buffer
    };
    VkDeviceAddress bufferDeviceAddress;
    bufferDeviceAddress = vk.getBufferDeviceAddress(device, &bdinfo);

    const VkQueue queue         = m_context.getUniversalQueue();
    Move<VkCommandPool> cmdPool = createCommandPool(vk, device, 0, m_context.getUniversalQueueFamilyIndex());

    void *ptr = buffer->getAllocation().getHostPtr();

    switch (srcElementSize)
    {
    case 4:
        for (uint32_t i = 0; i < numElements; ++i)
        {
            ((uint32_t *)ptr)[i] = i << 16;
        }
        break;
    case 2:
        for (uint32_t i = 0; i < numElements; ++i)
        {
            ((uint16_t *)ptr)[i] = (uint16_t)i;
        }
        break;
    case 1:
        for (uint32_t i = 0; i < numElements; ++i)
        {
            ((uint8_t *)ptr)[i] = (uint8_t)i;
        }
        break;
    }

    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *cmdBuffer, 0u);

    size_t dstSize = optimalSize;
    info           = {
        VK_STRUCTURE_TYPE_CONVERT_COOPERATIVE_VECTOR_MATRIX_INFO_NV, // VkStructureType                       sType;
        DE_NULL,                      // void const*                           pNext;
        numElements * srcElementSize, // size_t                                srcSize;
        {0},                          // VkDeviceOrHostAddressConstKHR         srcData;
        &dstSize,                     // size_t*                               pDstSize;
        {0},                          // VkDeviceOrHostAddressKHR              dstData;
        m_data.matrixType[0],         // VkComponentTypeKHR                    srcComponentType;
        m_data.matrixType[1],         // VkComponentTypeKHR                    dstComponentType;
        1,                            // uint32_t                              numRows;
        numElements,                  // uint32_t                              numColumns;
        VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV, // VkCooperativeVectorMatrixLayoutNV     srcLayout;
        numElements * srcElementSize,                     // size_t                                srcStride;
        VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_INFERENCING_OPTIMAL_NV, // VkCooperativeVectorMatrixLayoutNV     dstLayout;
        0, // size_t                                dstStride;
    };

    if (m_data.hostConvert)
    {
        info.srcData.hostAddress = (uint8_t *)(ptr);
        info.dstData.hostAddress = (uint8_t *)(ptr) + optimalOffset;
        VK_CHECK(vk.convertCooperativeVectorMatrixNV(device, &info));
    }
    else
    {
        info.srcData.deviceAddress = bufferDeviceAddress;
        info.dstData.deviceAddress = bufferDeviceAddress + optimalOffset;
        vk.cmdConvertCooperativeVectorMatrixNV(*cmdBuffer, 1, &info);

        VkMemoryBarrier2KHR memoryBarrier = {
            VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR,                       // sType
            DE_NULL,                                                      // pNext
            VK_PIPELINE_STAGE_2_CONVERT_COOPERATIVE_VECTOR_MATRIX_BIT_NV, // srcStageMask
            VK_ACCESS_2_TRANSFER_WRITE_BIT,                               // srcAccessMask
            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,                         // dstStageMask
            VK_ACCESS_2_SHADER_READ_BIT                                   // dstAccessMask
        };

        VkDependencyInfoKHR dependencyInfo{
            VK_STRUCTURE_TYPE_DEPENDENCY_INFO, // sType
            DE_NULL,                           // pNext
            0,                                 //dependency flags
            1,                                 //memory barrier count
            &memoryBarrier,                    //memory barrier
            0,                                 // bufferMemoryBarrierCount
            DE_NULL,                           // pBufferMemoryBarriers
            0,                                 // imageMemoryBarrierCount
            DE_NULL,                           // pImageMemoryBarriers
        };
        vk.cmdPipelineBarrier2(*cmdBuffer, &dependencyInfo);
    }

    dstSize = numElements * dstElementSize;
    info    = {
        VK_STRUCTURE_TYPE_CONVERT_COOPERATIVE_VECTOR_MATRIX_INFO_NV, // VkStructureType                       sType;
        DE_NULL,                                                     // void const*                           pNext;
        optimalSize,          // size_t                                srcSize;
        {0},                  // VkDeviceOrHostAddressConstKHR         srcData;
        &dstSize,             // size_t*                               pDstSize;
        {0},                  // VkDeviceOrHostAddressKHR              dstData;
        m_data.matrixType[1], // VkComponentTypeKHR                    srcComponentType;
        dstComponentType,     // VkComponentTypeKHR                    dstComponentType;
        1,                    // uint32_t                              numRows;
        numElements,          // uint32_t                              numColumns;
        VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_INFERENCING_OPTIMAL_NV, // VkCooperativeVectorMatrixLayoutNV     srcLayout;
        0,                                                // size_t                                srcStride;
        VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV, // VkCooperativeVectorMatrixLayoutNV     dstLayout;
        numElements * dstElementSize,                     // size_t                                dstStride;
    };

    if (m_data.hostConvert)
    {
        info.srcData.hostAddress = (uint8_t *)(ptr) + optimalOffset;
        info.dstData.hostAddress = (uint8_t *)(ptr) + dstOffset;
        VK_CHECK(vk.convertCooperativeVectorMatrixNV(device, &info));
    }
    else
    {
        info.srcData.deviceAddress = bufferDeviceAddress + optimalOffset;
        info.dstData.deviceAddress = bufferDeviceAddress + dstOffset;
        vk.cmdConvertCooperativeVectorMatrixNV(*cmdBuffer, 1, &info);
    }

    flushAlloc(vk, device, buffer->getAllocation());

    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

    invalidateAlloc(vk, device, buffer->getAllocation());

    qpTestResult res = QP_TEST_RESULT_PASS;

    uint8_t *dstPtr = (uint8_t *)ptr + dstOffset;
    for (uint32_t i = 0; i < numElements; ++i)
    {
        float src    = getDataFloat(ptr, m_data.matrixType[0], i);
        float output = getDataFloat(dstPtr, dstComponentType, i);

        uint32_t temp = 0;
        setDataFloat(&temp, m_data.matrixType[1], 0, src);
        float ref = getDataFloat(&temp, m_data.matrixType[1], 0);
        if (ref != output && !(deFloatIsIEEENaN(ref) && deFloatIsIEEENaN(output)))
        {
            uint32_t result = 0;
            deMemcpy(&result, &dstPtr[i * dstElementSize], dstElementSize);
            //printf("i %04x src %f ref %f output %f ref %04x output %04x\n", i, src, ref, output, temp, result);
            res = QP_TEST_RESULT_FAIL;
        }
    }

    return tcu::TestStatus(res, qpGetTestResultName(res));
}

} // namespace

tcu::TestCaseGroup *createCooperativeVectorMatrixTypeConversionTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(
        new tcu::TestCaseGroup(testCtx, "typeconvert", "cooperative_vector matrix convert type tests"));

    TestGroupCaseN<2> dtCases[] = {
        {{VK_COMPONENT_TYPE_FLOAT32_NV, VK_COMPONENT_TYPE_FLOAT16_NV}, "float32tofloat16", "float32tofloat16"},
        {{VK_COMPONENT_TYPE_FLOAT32_NV, VK_COMPONENT_TYPE_FLOAT_E4M3_NV}, "float32tofloate4m3", "float32tofloate4m3"},
        {{VK_COMPONENT_TYPE_FLOAT32_NV, VK_COMPONENT_TYPE_FLOAT_E5M2_NV}, "float32tofloate5m2", "float32tofloate5m2"},
        {{VK_COMPONENT_TYPE_FLOAT16_NV, VK_COMPONENT_TYPE_FLOAT16_NV}, "float16tofloat16", "float16tofloat16"},
        {{VK_COMPONENT_TYPE_FLOAT16_NV, VK_COMPONENT_TYPE_FLOAT_E4M3_NV}, "float16tofloate4m3", "float16tofloate4m3"},
        {{VK_COMPONENT_TYPE_FLOAT16_NV, VK_COMPONENT_TYPE_FLOAT_E5M2_NV}, "float16tofloate5m2", "float16tofloate5m2"},
        {{VK_COMPONENT_TYPE_FLOAT_E4M3_NV, VK_COMPONENT_TYPE_FLOAT16_NV}, "floate4m3tofloat16", "floate4m3tofloat16"},
        {{VK_COMPONENT_TYPE_FLOAT_E5M2_NV, VK_COMPONENT_TYPE_FLOAT16_NV}, "floate5m2tofloat16", "floate5m2tofloat16"},
        {{VK_COMPONENT_TYPE_FLOAT_E4M3_NV, VK_COMPONENT_TYPE_FLOAT_E4M3_NV},
         "floate4m3tofloate4m3",
         "floate4m3tofloate4m3"},
        {{VK_COMPONENT_TYPE_FLOAT_E5M2_NV, VK_COMPONENT_TYPE_FLOAT_E5M2_NV},
         "floate5m2tofloate5m2",
         "floate5m2tofloate5m2"},
    };

    TestGroupCaseN<1> hostCases[] = {
        {{false}, "device", "device"},
        {{true}, "host", "host"},
    };

    for (int hostNdx = 0; hostNdx < DE_LENGTH_OF_ARRAY(hostCases); hostNdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> hostGroup(
            new tcu::TestCaseGroup(testCtx, hostCases[hostNdx].name, hostCases[hostNdx].description));
        for (int dtNdx = 0; dtNdx < DE_LENGTH_OF_ARRAY(dtCases); dtNdx++)
        {
            CaseDef2 c = {
                {(VkComponentTypeKHR)dtCases[dtNdx].value[0], // VkComponentTypeKHR matrixType;
                 (VkComponentTypeKHR)dtCases[dtNdx].value[1]},
                !!hostCases[hostNdx].value[0], // bool hostConvert;
            };
            hostGroup->addChild(new CooperativeVectorTypeConversionTestCase(testCtx, dtCases[dtNdx].name, c));
        }
        group->addChild(hostGroup.release());
    }
    return group.release();
}

} // namespace cooperative_vector
} // namespace vkt
