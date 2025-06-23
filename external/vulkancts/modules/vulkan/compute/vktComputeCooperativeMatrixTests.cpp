/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2018-2024 NVIDIA Corporation
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
 * \brief Vulkan Cooperative Matrix tests
 *//*--------------------------------------------------------------------*/

#include "vktComputeCooperativeMatrixTests.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"

#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"

#include "deDefs.h"
#include "tcuFloat.hpp"
#include "deMath.h"
#include "deRandom.h"
#include "deSharedPtr.hpp"
#include "deString.h"

#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"
#include "tcuStringTemplate.hpp"

#include <string>
#include <sstream>
#include <set>
#include <algorithm>
#include <functional>
#include <climits>
#include <cmath>

namespace vkt
{
namespace compute
{
namespace
{
using namespace vk;
using namespace std;

//#define COOPERATIVE_MATRIX_EXTENDED_DEBUG 1

//#define SIMULATE_BFLOAT16

#ifdef SIMULATE_BFLOAT16
using BFloat16 = tcu::Float16;
#else
using BFloat16 = tcu::BrainFloat16;
#endif

DE_STATIC_ASSERT((uint32_t)VK_COMPONENT_TYPE_FLOAT16_KHR == (uint32_t)VK_COMPONENT_TYPE_FLOAT16_NV);
DE_STATIC_ASSERT((uint32_t)VK_COMPONENT_TYPE_FLOAT32_KHR == (uint32_t)VK_COMPONENT_TYPE_FLOAT32_NV);
DE_STATIC_ASSERT((uint32_t)VK_COMPONENT_TYPE_FLOAT64_KHR == (uint32_t)VK_COMPONENT_TYPE_FLOAT64_NV);
DE_STATIC_ASSERT((uint32_t)VK_COMPONENT_TYPE_SINT8_KHR == (uint32_t)VK_COMPONENT_TYPE_SINT8_NV);
DE_STATIC_ASSERT((uint32_t)VK_COMPONENT_TYPE_SINT16_KHR == (uint32_t)VK_COMPONENT_TYPE_SINT16_NV);
DE_STATIC_ASSERT((uint32_t)VK_COMPONENT_TYPE_SINT32_KHR == (uint32_t)VK_COMPONENT_TYPE_SINT32_NV);
DE_STATIC_ASSERT((uint32_t)VK_COMPONENT_TYPE_SINT64_KHR == (uint32_t)VK_COMPONENT_TYPE_SINT64_NV);
DE_STATIC_ASSERT((uint32_t)VK_COMPONENT_TYPE_UINT8_KHR == (uint32_t)VK_COMPONENT_TYPE_UINT8_NV);
DE_STATIC_ASSERT((uint32_t)VK_COMPONENT_TYPE_UINT16_KHR == (uint32_t)VK_COMPONENT_TYPE_UINT16_NV);
DE_STATIC_ASSERT((uint32_t)VK_COMPONENT_TYPE_UINT32_KHR == (uint32_t)VK_COMPONENT_TYPE_UINT32_NV);
DE_STATIC_ASSERT((uint32_t)VK_COMPONENT_TYPE_UINT64_KHR == (uint32_t)VK_COMPONENT_TYPE_UINT64_NV);

DE_STATIC_ASSERT((uint32_t)VK_SCOPE_DEVICE_KHR == (uint32_t)VK_SCOPE_DEVICE_NV);
DE_STATIC_ASSERT((uint32_t)VK_SCOPE_WORKGROUP_KHR == (uint32_t)VK_SCOPE_WORKGROUP_NV);
DE_STATIC_ASSERT((uint32_t)VK_SCOPE_SUBGROUP_KHR == (uint32_t)VK_SCOPE_SUBGROUP_NV);
DE_STATIC_ASSERT((uint32_t)VK_SCOPE_QUEUE_FAMILY_KHR == (uint32_t)VK_SCOPE_QUEUE_FAMILY_NV);

typedef enum
{
    UT_NV = 0,
    UT_KHR_A,
    UT_KHR_B,
    UT_KHR_C,
    UT_KHR_Result,
} UseType;

typedef enum
{
    TT_LENGTH = 0,
    TT_CONSTANT,
    TT_CONVERT,
    TT_CONVERT_SAT,
    TT_CONVERT_ACC_TO_A,
    TT_CONVERT_ACC_TO_B,
    TT_TRANSPOSE_ACC_TO_B,
    TT_REDUCE_SUM_ROW,
    TT_REDUCE_SUM_COL,
    TT_REDUCE_SUM_ROWCOL,
    TT_REDUCE_SUM_2X2,
    TT_REDUCE_SUM_ROW_CHANGEDIM,
    TT_REDUCE_SUM_COL_CHANGEDIM,
    TT_REDUCE_SUM_ROWCOL_CHANGEDIM,
    TT_REDUCE_MIN_ROW,
    TT_REDUCE_MIN_COL,
    TT_REDUCE_MIN_ROWCOL,
    TT_REDUCE_MIN_2X2,
    TT_PER_ELEMENT_OP,
    TT_PER_ELEMENT_OP_ROW_COL,
    TT_PER_ELEMENT_OP_STRUCT,
    TT_PER_ELEMENT_OP_MAT,
    TT_COMPOSITE,
    TT_COMPOSITE_RVALUE,
    TT_ADD,
    TT_SUB,
    TT_DIV,
    TT_MUL,
    TT_NEGATE,
    TT_MATRIXTIMESSCALAR,
    TT_FUNC,
    TT_FUNC_CONST_IN,
    TT_CLAMPCONSTANT,
    TT_CLAMPTOEDGE,
    TT_CLAMPREPEAT,
    TT_CLAMPMIRRORREPEAT,
    TT_MATRIXMULADD,
    TT_COMPOSITE_ARRAY,
    TT_MATRIXMULADD_ARRAY,
    TT_MATRIXMULADD_SATURATED,
    TT_MATRIXMULADD_WRAPPING,
    TT_MATRIXMULADD_STRIDE0,
    TT_MATRIXMULADD_DEQUANT,
    TT_MULTICOMPONENT_LOAD,
    TT_MULTICOMPONENT_SAVE,
    TT_MATRIXMULADD_CROSS,
    TT_MATRIXMULADD_PUSH_CONSTANTS,
    TT_TENSORLAYOUT_1D,
    TT_TENSORLAYOUT_2D,
    TT_TENSORLAYOUT_3D,
    TT_TENSORLAYOUT_4D,
    TT_TENSORLAYOUT_5D,
    TT_TENSORLAYOUT_1D_CLIP,
    TT_TENSORLAYOUT_2D_CLIP,
    TT_TENSORLAYOUT_3D_CLIP,
    TT_TENSORLAYOUT_4D_CLIP,
    TT_TENSORLAYOUT_5D_CLIP,
    TT_SPACETODEPTH,
    TT_CONV,
} TestType;

typedef enum
{
    SC_BUFFER = 0,
    SC_WORKGROUP,
    SC_WORKGROUP_VARIABLE_POINTERS,
    SC_BUFFER_VARIABLE_POINTERS,
    SC_PHYSICAL_STORAGE_BUFFER,
} StorageClass;

typedef enum
{
    ADDR_LINEAR = 0,
    ADDR_TENSORLAYOUT,
    ADDR_BLOCKSIZE,
    ADDR_DECODE,
} AddrMethod;

enum SubgroupSizeMode
{
    SUBGROUP_SIZE_NONE = 0,
    SUBGROUP_SIZE_MIN  = 1,
    SUBGROUP_SIZE_MAX  = 2,
};

const VkFlags allShaderStages = VK_SHADER_STAGE_COMPUTE_BIT;

struct CaseDef
{
    TestType testType;
    VkScopeKHR scope;
    uint32_t subgroupsPerWorkgroupX;
    uint32_t subgroupsPerWorkgroupY;
    uint32_t workgroupsX;
    uint32_t workgroupsY;
    VkComponentTypeKHR inputType;
    VkComponentTypeKHR outputType;
    bool colMajor;
    AddrMethod addrMethod;
    StorageClass storageClass;
    UseType useType;
    SubgroupSizeMode subgroupSizeMode;
    vk::ComputePipelineConstructionType computePipelineConstructionType;
    uint32_t inputComponentCount;
    uint32_t outputComponentCount;
};

bool isKhr(UseType useType)
{
    return useType != UT_NV;
}

bool isMatrixMulAddOp(TestType testType)
{
    return testType == TT_MATRIXMULADD || testType == TT_MATRIXMULADD_ARRAY || testType == TT_MATRIXMULADD_SATURATED ||
           testType == TT_MATRIXMULADD_WRAPPING || testType == TT_MATRIXMULADD_STRIDE0 ||
           testType == TT_MATRIXMULADD_CROSS || testType == TT_MATRIXMULADD_DEQUANT ||
           testType == TT_MATRIXMULADD_PUSH_CONSTANTS;
}

bool isReduceRow(TestType testType)
{
    return testType == TT_REDUCE_SUM_ROW || testType == TT_REDUCE_MIN_ROW || testType == TT_REDUCE_SUM_ROW_CHANGEDIM;
}

bool isReduceCol(TestType testType)
{
    return testType == TT_REDUCE_SUM_COL || testType == TT_REDUCE_MIN_COL || testType == TT_REDUCE_SUM_COL_CHANGEDIM;
}

bool isReduceRowCol(TestType testType)
{
    return testType == TT_REDUCE_SUM_ROWCOL || testType == TT_REDUCE_MIN_ROWCOL ||
           testType == TT_REDUCE_SUM_ROWCOL_CHANGEDIM;
}

bool isReduce2x2(TestType testType)
{
    return testType == TT_REDUCE_SUM_2X2 || testType == TT_REDUCE_MIN_2X2;
}

bool isReduceSum(TestType testType)
{
    return testType == TT_REDUCE_SUM_ROW || testType == TT_REDUCE_SUM_COL || testType == TT_REDUCE_SUM_ROWCOL ||
           testType == TT_REDUCE_SUM_2X2 || testType == TT_REDUCE_SUM_ROW_CHANGEDIM ||
           testType == TT_REDUCE_SUM_COL_CHANGEDIM || testType == TT_REDUCE_SUM_ROWCOL_CHANGEDIM;
}

bool isReduceMin(TestType testType)
{
    return testType == TT_REDUCE_MIN_ROW || testType == TT_REDUCE_MIN_COL || testType == TT_REDUCE_MIN_ROWCOL ||
           testType == TT_REDUCE_MIN_2X2;
}

bool isReduceOp(TestType testType)
{
    return isReduceRow(testType) || isReduceCol(testType) || isReduceRowCol(testType) || isReduce2x2(testType);
}

bool isReduceChangeDim(TestType testType)
{
    return testType == TT_REDUCE_SUM_ROW_CHANGEDIM || testType == TT_REDUCE_SUM_COL_CHANGEDIM ||
           testType == TT_REDUCE_SUM_ROWCOL_CHANGEDIM;
}

uint32_t reduceMScale(TestType testType)
{
    if (testType == TT_REDUCE_SUM_COL_CHANGEDIM || testType == TT_REDUCE_SUM_ROWCOL_CHANGEDIM)
    {
        return 3;
    }
    else
    {
        return 1;
    }
}

uint32_t reduceNScale(TestType testType)
{
    if (testType == TT_REDUCE_SUM_ROW_CHANGEDIM || testType == TT_REDUCE_SUM_ROWCOL_CHANGEDIM)
    {
        return 3;
    }
    else
    {
        return 1;
    }
}

bool isClampTest(TestType testType)
{
    return testType == TT_CLAMPCONSTANT || testType == TT_CLAMPTOEDGE || testType == TT_CLAMPREPEAT ||
           testType == TT_CLAMPMIRRORREPEAT;
}

bool isTensorLayoutClipTest(TestType testType)
{
    return testType == TT_TENSORLAYOUT_1D_CLIP || testType == TT_TENSORLAYOUT_2D_CLIP ||
           testType == TT_TENSORLAYOUT_3D_CLIP || testType == TT_TENSORLAYOUT_4D_CLIP ||
           testType == TT_TENSORLAYOUT_5D_CLIP;
}

bool isTensorLayoutTest(TestType testType)
{
    return testType == TT_TENSORLAYOUT_1D || testType == TT_TENSORLAYOUT_2D || testType == TT_TENSORLAYOUT_3D ||
           testType == TT_TENSORLAYOUT_4D || testType == TT_TENSORLAYOUT_5D || isTensorLayoutClipTest(testType) ||
           testType == TT_SPACETODEPTH;
}

bool isPerElemOp(TestType testType)
{
    return testType == TT_PER_ELEMENT_OP || testType == TT_PER_ELEMENT_OP_ROW_COL ||
           testType == TT_PER_ELEMENT_OP_STRUCT || testType == TT_PER_ELEMENT_OP_MAT;
}

bool isArithmeticTest(TestType testType)
{
    return testType == TT_COMPOSITE || testType == TT_FUNC || testType == TT_FUNC_CONST_IN || testType == TT_ADD ||
           testType == TT_SUB || testType == TT_MUL || testType == TT_DIV || testType == TT_NEGATE ||
           testType == TT_MATRIXTIMESSCALAR || isPerElemOp(testType);
}

int32_t tensorLayout1dMatrixSize[][5] = {
    {32, 32},
    {64, 64},
};

int32_t tensorLayout1dDim[5] = {65536, 1, 1, 1, 1};

int32_t tensorLayout1dSpan[][5] = {
    {1024},
    {4096},
};

int32_t tensorLayout1dLoadOffsets[][5] = {
    {10000},
    {-1},
};
int32_t tensorLayout1dStoreOffsets[][5] = {
    {-1},
    {4321},
};

uint32_t tensorLayout1dNumCoords = sizeof(tensorLayout1dLoadOffsets) / sizeof(tensorLayout1dLoadOffsets[0]);

int32_t tensorLayout2dMatrixSize[][5] = {
    {32, 32},
    {64, 64},
};

int32_t tensorLayout2dDim[5] = {512, 512, 1, 1, 1};

int32_t tensorLayout2dSpan[][5] = {
    {32, 32},
    {64, 64},
};

int32_t tensorLayout2dLoadOffsets[][5] = {
    {7, 13},
    {0 + 128, 0 + 128},
};
int32_t tensorLayout2dStoreOffsets[][5] = {
    {13, 7},
    {20 + 128, 0},
};

uint32_t tensorLayout2dNumCoords = sizeof(tensorLayout2dLoadOffsets) / sizeof(tensorLayout2dLoadOffsets[0]);

int32_t tensorLayout3dDim[5] = {33, 44, 55, 1, 1};

int32_t tensorLayout3dMatrixSize[][5] = {
    {64, 32},
    {32, 32},
};

int32_t tensorLayout3dSpan[][5] = {
    {16, 16, 8},
    {8, 4, 32},
};
int32_t tensorLayout3dLoadOffsets[][5] = {
    {1, 1, 1},
    {-1, -1, -1},
};
int32_t tensorLayout3dStoreOffsets[][5] = {
    {2, 2, 2},
    {23, 2, 1},
};

uint32_t tensorLayout3dNumCoords = sizeof(tensorLayout3dLoadOffsets) / sizeof(tensorLayout3dLoadOffsets[0]);

int32_t tensorLayout4dDim[5] = {20, 25, 40, 10, 1};

int32_t tensorLayout4dMatrixSize[][5] = {
    {64, 64},
};

int32_t tensorLayout4dSpan[][5] = {
    {16, 8, 8, 4},
};
int32_t tensorLayout4dLoadOffsets[][5] = {
    {-1, -1, -1, -1},
};
int32_t tensorLayout4dStoreOffsets[][5] = {
    {1, 2, 1, 2},
};

uint32_t tensorLayout4dNumCoords = sizeof(tensorLayout4dLoadOffsets) / sizeof(tensorLayout4dLoadOffsets[0]);

int32_t tensorLayout5dDim[5] = {4, 4, 32, 16, 8};

int32_t tensorLayout5dMatrixSize[][5] = {
    {32, 32},
};

int32_t tensorLayout5dSpan[][5] = {
    {1, 4, 8, 4, 8},
};
int32_t tensorLayout5dLoadOffsets[][5] = {
    {-1, -1, -1, -1, -1},
};
int32_t tensorLayout5dStoreOffsets[][5] = {
    {1, 2, 1, 0, 1},
};

uint32_t tensorLayout5dNumCoords = sizeof(tensorLayout5dLoadOffsets) / sizeof(tensorLayout5dLoadOffsets[0]);

int32_t *GetTensorLayoutMatrixSizes(uint32_t dim, uint32_t index)
{
    switch (dim)
    {
    case 1:
        return tensorLayout1dMatrixSize[index];
    case 2:
        return tensorLayout2dMatrixSize[index];
    case 3:
        return tensorLayout3dMatrixSize[index];
    case 4:
        return tensorLayout4dMatrixSize[index];
    case 5:
        return tensorLayout5dMatrixSize[index];
    }
    DE_ASSERT(0);
    return nullptr;
}

int32_t *GetTensorLayoutDim(uint32_t dim)
{
    switch (dim)
    {
    case 1:
        return tensorLayout1dDim;
    case 2:
        return tensorLayout2dDim;
    case 3:
        return tensorLayout3dDim;
    case 4:
        return tensorLayout4dDim;
    case 5:
        return tensorLayout5dDim;
    }
    DE_ASSERT(0);
    return nullptr;
}

int32_t *GetTensorLayoutSpan(uint32_t dim, uint32_t index)
{
    switch (dim)
    {
    case 1:
        return tensorLayout1dSpan[index];
    case 2:
        return tensorLayout2dSpan[index];
    case 3:
        return tensorLayout3dSpan[index];
    case 4:
        return tensorLayout4dSpan[index];
    case 5:
        return tensorLayout5dSpan[index];
    }
    DE_ASSERT(0);
    return nullptr;
}

int32_t *GetTensorLayoutLoadOffsets(uint32_t dim, uint32_t index)
{
    switch (dim)
    {
    case 1:
        return tensorLayout1dLoadOffsets[index];
    case 2:
        return tensorLayout2dLoadOffsets[index];
    case 3:
        return tensorLayout3dLoadOffsets[index];
    case 4:
        return tensorLayout4dLoadOffsets[index];
    case 5:
        return tensorLayout5dLoadOffsets[index];
    }
    DE_ASSERT(0);
    return nullptr;
}

int32_t *GetTensorLayoutStoreOffsets(uint32_t dim, uint32_t index)
{
    switch (dim)
    {
    case 1:
        return tensorLayout1dStoreOffsets[index];
    case 2:
        return tensorLayout2dStoreOffsets[index];
    case 3:
        return tensorLayout3dStoreOffsets[index];
    case 4:
        return tensorLayout4dStoreOffsets[index];
    case 5:
        return tensorLayout5dStoreOffsets[index];
    }
    DE_ASSERT(0);
    return nullptr;
}

uint32_t GetTensorLayoutNumCoords(uint32_t dim)
{
    switch (dim)
    {
    case 1:
        return tensorLayout1dNumCoords;
    case 2:
        return tensorLayout2dNumCoords;
    case 3:
        return tensorLayout3dNumCoords;
    case 4:
        return tensorLayout4dNumCoords;
    case 5:
        return tensorLayout5dNumCoords;
    }
    DE_ASSERT(0);
    return 0;
}

uint32_t GetDim(TestType testType)
{
    switch (testType)
    {
    case TT_TENSORLAYOUT_1D:
        return 1;
    case TT_TENSORLAYOUT_2D:
        return 2;
    case TT_TENSORLAYOUT_3D:
        return 3;
    case TT_TENSORLAYOUT_4D:
        return 4;
    case TT_TENSORLAYOUT_5D:
        return 5;
    case TT_TENSORLAYOUT_1D_CLIP:
        return 1;
    case TT_TENSORLAYOUT_2D_CLIP:
        return 2;
    case TT_TENSORLAYOUT_3D_CLIP:
        return 3;
    case TT_TENSORLAYOUT_4D_CLIP:
        return 4;
    case TT_TENSORLAYOUT_5D_CLIP:
        return 5;
    default:
        DE_ASSERT(0);
        return 0;
    }
}

static constexpr uint32_t blockSize[2] = {2, 4};

template <typename T>
VkResult getCooperativeMatrixProperties(const InstanceInterface &, VkPhysicalDevice, uint32_t *, T *)
{
    TCU_THROW(InternalError, "Not Implementetd");
}

VkResult getCooperativeMatrixProperties(const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                        uint32_t *pPropertyCount, VkCooperativeMatrixPropertiesKHR *pProperties)
{
    return vki.getPhysicalDeviceCooperativeMatrixPropertiesKHR(physicalDevice, pPropertyCount, pProperties);
}

VkResult getCooperativeMatrixProperties(const InstanceInterface &vki, VkPhysicalDevice physicalDevice,
                                        uint32_t *pPropertyCount, VkCooperativeMatrixPropertiesNV *pProperties)
{
    return vki.getPhysicalDeviceCooperativeMatrixPropertiesNV(physicalDevice, pPropertyCount, pProperties);
}

VkCooperativeMatrixPropertiesKHR convertCooperativeMatrixProperties(const VkCooperativeMatrixPropertiesNV &properties)
{
    VkCooperativeMatrixPropertiesKHR result = initVulkanStructure();

    result.sType                  = (VkStructureType)properties.sType;
    result.pNext                  = (void *)properties.pNext;
    result.MSize                  = (uint32_t)properties.MSize;
    result.NSize                  = (uint32_t)properties.NSize;
    result.KSize                  = (uint32_t)properties.KSize;
    result.AType                  = (VkComponentTypeKHR)properties.AType;
    result.BType                  = (VkComponentTypeKHR)properties.BType;
    result.CType                  = (VkComponentTypeKHR)properties.CType;
    result.ResultType             = (VkComponentTypeKHR)properties.DType;
    result.saturatingAccumulation = (VkBool32)VK_FALSE;
    result.scope                  = (VkScopeKHR)properties.scope;

    return result;
}

std::vector<VkCooperativeMatrixPropertiesKHR> convertCooperativeMatrixProperties(
    const std::vector<VkCooperativeMatrixPropertiesNV> &properties)
{
    std::vector<VkCooperativeMatrixPropertiesKHR> result(properties.size());

    for (size_t i = 0; i < properties.size(); ++i)
        result[i] = convertCooperativeMatrixProperties(properties[i]);

    return result;
}

template <typename T>
void getCooperativeMatrixPropertiesAll(Context &context, std::vector<T> &properties)
{
    uint32_t propertyCount = 0;

    VK_CHECK(getCooperativeMatrixProperties(context.getInstanceInterface(), context.getPhysicalDevice(), &propertyCount,
                                            (T *)nullptr));

    if (propertyCount > 0)
    {
        const T sample = initVulkanStructureConst();

        properties.resize(propertyCount, sample);

        VK_CHECK(getCooperativeMatrixProperties(context.getInstanceInterface(), context.getPhysicalDevice(),
                                                &propertyCount, properties.data()));
    }
    else
    {
        properties.clear();
    }
}

std::vector<VkCooperativeMatrixPropertiesKHR> getCooperativeMatrixPropertiesConverted(Context &context, const bool khr)
{
    std::vector<VkCooperativeMatrixPropertiesKHR> properties;

    if (khr)
    {
        getCooperativeMatrixPropertiesAll(context, properties);
    }
    else
    {
        std::vector<VkCooperativeMatrixPropertiesNV> propertiesNV;

        getCooperativeMatrixPropertiesAll(context, propertiesNV);

        properties = convertCooperativeMatrixProperties(propertiesNV);
    }

    return properties;
}

uint32_t getSubgroupSizeFromMode(Context &context, const SubgroupSizeMode subgroupSizeMode)
{
#ifndef CTS_USES_VULKANSC
    const VkPhysicalDeviceSubgroupSizeControlProperties &subgroupSizeControlProperties =
        context.getSubgroupSizeControlProperties();
#else
    const VkPhysicalDeviceSubgroupSizeControlPropertiesEXT &subgroupSizeControlProperties =
        context.getSubgroupSizeControlProperties();
#endif // CTS_USES_VULKANSC

    switch (subgroupSizeMode)
    {
    case SUBGROUP_SIZE_MAX:
        return subgroupSizeControlProperties.maxSubgroupSize;
    case SUBGROUP_SIZE_MIN:
        return subgroupSizeControlProperties.minSubgroupSize;
    case SUBGROUP_SIZE_NONE:
        return context.getSubgroupProperties().subgroupSize;
    default:
        TCU_THROW(NotSupportedError, "Unsupported Subgroup size");
    }
}

class CooperativeMatrixTestInstance : public TestInstance
{
public:
    CooperativeMatrixTestInstance(Context &context, const CaseDef &data);
    ~CooperativeMatrixTestInstance(void);
    tcu::TestStatus iterate(void);

private:
    CaseDef m_data;
};

CooperativeMatrixTestInstance::CooperativeMatrixTestInstance(Context &context, const CaseDef &data)
    : vkt::TestInstance(context)
    , m_data(data)
{
}

CooperativeMatrixTestInstance::~CooperativeMatrixTestInstance(void)
{
}

class CooperativeMatrixTestCase : public TestCase
{
public:
    CooperativeMatrixTestCase(tcu::TestContext &context, const char *name, const CaseDef data);
    ~CooperativeMatrixTestCase(void);
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

protected:
    virtual void initProgramsGLSL(SourceCollections &programCollection) const;
    virtual void initProgramsSPIRV(SourceCollections &programCollection) const;
    CaseDef m_data;
};

CooperativeMatrixTestCase::CooperativeMatrixTestCase(tcu::TestContext &context, const char *name, const CaseDef data)
    : vkt::TestCase(context, name)
    , m_data(data)
{
}

CooperativeMatrixTestCase::~CooperativeMatrixTestCase(void)
{
}

void CooperativeMatrixTestCase::checkSupport(Context &context) const
{
    if (!context.contextSupports(vk::ApiVersion(0, 1, 1, 0)))
    {
        TCU_THROW(NotSupportedError, "Vulkan 1.1 not supported");
    }

    if (isKhr(m_data.useType))
    {
        if (!context.getCooperativeMatrixFeatures().cooperativeMatrix)
        {
            TCU_THROW(NotSupportedError,
                      "VkPhysicalDeviceCooperativeMatrixFeaturesKHR::cooperativeMatrix not supported");
        }
    }
    else
    {
        if (!context.getCooperativeMatrixFeaturesNV().cooperativeMatrix)
        {
            TCU_THROW(NotSupportedError,
                      "VkPhysicalDeviceCooperativeMatrixFeaturesNV::cooperativeMatrix not supported");
        }
    }

    if (!context.getVulkanMemoryModelFeatures().vulkanMemoryModel)
    {
        TCU_THROW(NotSupportedError, "vulkanMemoryModel not supported");
    }

    if ((m_data.storageClass == SC_WORKGROUP_VARIABLE_POINTERS || m_data.storageClass == SC_BUFFER_VARIABLE_POINTERS) &&
        !context.getVariablePointersFeatures().variablePointers)
    {
        TCU_THROW(NotSupportedError, "variable pointers not supported");
    }

    if (m_data.storageClass == SC_PHYSICAL_STORAGE_BUFFER && !context.isBufferDeviceAddressSupported())
    {
        TCU_THROW(NotSupportedError, "buffer device address not supported");
    }

    if (!context.getShaderFloat16Int8Features().shaderFloat16 &&
        (m_data.inputType == VK_COMPONENT_TYPE_FLOAT16_KHR || m_data.outputType == VK_COMPONENT_TYPE_FLOAT16_KHR))
    {
        TCU_THROW(NotSupportedError, "shaderFloat16 not supported");
    }

#define REQUIRE(FEATURE)                                             \
    context.requireDeviceFunctionality("VK_NV_cooperative_matrix2"); \
    if (!context.getCooperativeMatrix2FeaturesNV().FEATURE)          \
    {                                                                \
        TCU_THROW(NotSupportedError, #FEATURE " not supported");     \
    }

    if (m_data.scope == VK_SCOPE_WORKGROUP_KHR)
    {
        REQUIRE(cooperativeMatrixWorkgroupScope)
    }
    if (isReduceOp(m_data.testType))
    {
        REQUIRE(cooperativeMatrixReductions)
    }

    if (m_data.testType == TT_CONVERT_ACC_TO_A || m_data.testType == TT_CONVERT_ACC_TO_B ||
        m_data.testType == TT_TRANSPOSE_ACC_TO_B)
    {
        REQUIRE(cooperativeMatrixConversions)
    }

    if (isPerElemOp(m_data.testType))
    {
        REQUIRE(cooperativeMatrixPerElementOperations)
    }

    if (m_data.addrMethod != ADDR_LINEAR || isTensorLayoutTest(m_data.testType) || isClampTest(m_data.testType))
    {
        REQUIRE(cooperativeMatrixTensorAddressing);
    }

    if (isTensorLayoutTest(m_data.testType))
    {
        REQUIRE(cooperativeMatrixFlexibleDimensions);
    }

    if (m_data.addrMethod == ADDR_BLOCKSIZE || m_data.addrMethod == ADDR_DECODE)
    {
        REQUIRE(cooperativeMatrixBlockLoads);
    }

    std::vector<VkCooperativeMatrixPropertiesKHR> properties =
        getCooperativeMatrixPropertiesConverted(context, isKhr(m_data.useType));
    bool supported[2]   = {false, false};
    const auto isMMA    = isMatrixMulAddOp(m_data.testType);
    const auto isMMASat = m_data.testType == TT_MATRIXMULADD_SATURATED;

    for (size_t i = 0; i < properties.size(); ++i)
    {
        const VkCooperativeMatrixPropertiesKHR *p = &properties[i];

        if (p->scope != m_data.scope)
            continue;

        if (isMMA && isMMASat != static_cast<bool>(p->saturatingAccumulation))
            continue;

        if (isMMA)
        {
            if (p->AType == m_data.inputType && p->BType == m_data.inputType && p->CType == m_data.outputType &&
                p->ResultType == m_data.outputType)
            {
                supported[0] = supported[1] = true;
            }
        }
        else
        {
            const VkComponentTypeKHR types[2] = {m_data.inputType, m_data.outputType};
            UseType uses[2]                   = {m_data.useType, m_data.useType};
            if (m_data.testType == TT_CONVERT_ACC_TO_A)
            {
                uses[1] = UT_KHR_A;
            }
            else if (m_data.testType == TT_CONVERT_ACC_TO_B || m_data.testType == TT_TRANSPOSE_ACC_TO_B)
            {
                uses[1] = UT_KHR_B;
            }

            for (uint32_t j = 0; j < 2; ++j)
            {
                switch (uses[j])
                {
                case UT_NV:
                {
                    if (p->AType == types[j] || p->BType == types[j] || p->CType == types[j] ||
                        p->ResultType == types[j])
                        supported[j] = true;

                    break;
                }
                case UT_KHR_A:
                {
                    if (p->AType == types[j])
                        supported[j] = true;

                    break;
                }
                case UT_KHR_B:
                {
                    if (p->BType == types[j])
                        supported[j] = true;

                    break;
                }
                case UT_KHR_Result:
                {
                    if (p->ResultType == types[j])
                        supported[j] = true;

                    break;
                }
                default:
                    TCU_THROW(InternalError, "Unsupported use type");
                }
            }
        }
    }

    if (context.getCooperativeMatrix2FeaturesNV().cooperativeMatrixFlexibleDimensions)
    {
        uint32_t flexiblePropertyCount = 0;
        std::vector<VkCooperativeMatrixFlexibleDimensionsPropertiesNV> flexibleProperties;

        const InstanceInterface &vki = context.getInstanceInterface();
        VK_CHECK(vki.getPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV(context.getPhysicalDevice(),
                                                                                      &flexiblePropertyCount, nullptr));

        if (flexiblePropertyCount > 0)
        {
            const VkCooperativeMatrixFlexibleDimensionsPropertiesNV sample = initVulkanStructureConst();

            flexibleProperties.resize(flexiblePropertyCount, sample);

            VK_CHECK(vki.getPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV(
                context.getPhysicalDevice(), &flexiblePropertyCount, flexibleProperties.data()));
        }
        else
        {
            flexibleProperties.clear();
        }

        for (size_t i = 0; i < flexibleProperties.size(); ++i)
        {
            const VkCooperativeMatrixFlexibleDimensionsPropertiesNV *p = &flexibleProperties[i];

            if (p->scope != m_data.scope)
                continue;

            if (isMMA && isMMASat != static_cast<bool>(p->saturatingAccumulation))
                continue;

            if (isMMA)
            {
                if (p->AType == m_data.inputType && p->BType == m_data.inputType && p->CType == m_data.outputType &&
                    p->ResultType == m_data.outputType)
                {
                    supported[0] = supported[1] = true;
                }
            }
            else
            {
                const VkComponentTypeKHR types[2] = {m_data.inputType, m_data.outputType};
                UseType uses[2]                   = {m_data.useType, m_data.useType};
                if (m_data.testType == TT_CONVERT_ACC_TO_A)
                {
                    uses[1] = UT_KHR_A;
                }
                else if (m_data.testType == TT_CONVERT_ACC_TO_B || m_data.testType == TT_TRANSPOSE_ACC_TO_B)
                {
                    uses[1] = UT_KHR_B;
                }

                for (uint32_t j = 0; j < 2; ++j)
                {
                    switch (uses[j])
                    {
                    case UT_NV:
                        break;
                    case UT_KHR_A:
                    {
                        if (p->AType == types[j])
                            supported[j] = true;

                        break;
                    }
                    case UT_KHR_B:
                    {
                        if (p->BType == types[j])
                            supported[j] = true;

                        break;
                    }
                    case UT_KHR_Result:
                    {
                        if (p->ResultType == types[j])
                            supported[j] = true;

                        break;
                    }
                    default:
                        TCU_THROW(InternalError, "Unsupported use type");
                    }
                }
            }
        }
    }

    if (!supported[0] || !supported[1])
        TCU_THROW(NotSupportedError, "cooperative matrix combination not supported");

    checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                  m_data.computePipelineConstructionType);

#ifndef CTS_USES_VULKANSC
    auto &bfeatures16 = context.getShaderBfloat16Features();
    const bool anyBFloat16 =
        (m_data.inputType == VK_COMPONENT_TYPE_BFLOAT16_KHR || m_data.outputType == VK_COMPONENT_TYPE_BFLOAT16_KHR);
    if (anyBFloat16)
    {
        if (!bfeatures16.shaderBFloat16Type)
            TCU_THROW(NotSupportedError, "shaderBFloat16Type is not supported by device");

        if (!bfeatures16.shaderBFloat16CooperativeMatrix)
            TCU_THROW(NotSupportedError, "shaderBFloat16CooperativeMatrix is not supported by device");
    }
#endif // CTS_USES_VULKANSC

#ifndef CTS_USES_VULKANSC
    auto &features8 = context.getShaderFloat8FeaturesEXT();
    const bool anyFloat8 =
        (m_data.inputType == VK_COMPONENT_TYPE_FLOAT_E5M2_NV || m_data.outputType == VK_COMPONENT_TYPE_FLOAT_E5M2_NV ||
         m_data.inputType == VK_COMPONENT_TYPE_FLOAT_E4M3_NV || m_data.outputType == VK_COMPONENT_TYPE_FLOAT_E4M3_NV);
    if (anyFloat8)
    {
        if (!features8.shaderFloat8)
            TCU_THROW(NotSupportedError, "shaderFloat8 is not supported by device");

        if (!features8.shaderFloat8CooperativeMatrix)
            TCU_THROW(NotSupportedError, "shaderFloat8CooperativeMatrix is not supported by device");
    }
#endif // CTS_USES_VULKANSC
}

struct ComponentTypeInfo
{
    const char *typeName;
    const char *coopmatTypeName;
    const char *vecprefix;
    uint32_t bits;
    bool isSigned;
};
const std::map<VkComponentTypeKHR, ComponentTypeInfo> componentTypeInfo{
    {VK_COMPONENT_TYPE_FLOAT16_KHR, {"float16_t", "fcoopmatNV", "f16", 16, true}},
    {VK_COMPONENT_TYPE_FLOAT32_KHR, {"float32_t", "fcoopmatNV", "f32", 32, true}},
    {VK_COMPONENT_TYPE_FLOAT64_KHR, {"float64_t", "fcoopmatNV", "f64", 64, true}},
    {VK_COMPONENT_TYPE_SINT8_KHR, {"int8_t", "icoopmatNV", "i8", 8, true}},
    {VK_COMPONENT_TYPE_SINT16_KHR, {"int16_t", "icoopmatNV", "i16", 16, true}},
    {VK_COMPONENT_TYPE_SINT32_KHR, {"int32_t", "icoopmatNV", "i32", 32, true}},
    {VK_COMPONENT_TYPE_SINT64_KHR, {"int64_t", "icoopmatNV", "i64", 64, true}},
    {VK_COMPONENT_TYPE_UINT8_KHR, {"uint8_t", "ucoopmatNV", "u8", 8, false}},
    {VK_COMPONENT_TYPE_UINT16_KHR, {"uint16_t", "ucoopmatNV", "u16", 16, false}},
    {VK_COMPONENT_TYPE_UINT32_KHR, {"uint32_t", "ucoopmatNV", "u32", 32, false}},
    {VK_COMPONENT_TYPE_UINT64_KHR, {"uint64_t", "ucoopmatNV", "u64", 64, false}},
#ifndef CTS_USES_VULKANSC
    {VK_COMPONENT_TYPE_BFLOAT16_KHR, {"bfloat16_t", "fcoopmatNV", "bf16", 16, true}},
    {VK_COMPONENT_TYPE_FLOAT_E5M2_NV, {"floate5m2_t", "fcoopmatNV", "fe5m2", 8, true}},
    {VK_COMPONENT_TYPE_FLOAT_E4M3_NV, {"floate4m3_t", "fcoopmatNV", "fe4m3", 8, true}},
#endif
};

bool isFloatType(VkComponentTypeKHR t)
{
    switch (t)
    {
    case VK_COMPONENT_TYPE_FLOAT16_KHR:
    case VK_COMPONENT_TYPE_FLOAT32_KHR:
    case VK_COMPONENT_TYPE_FLOAT64_KHR:
#ifndef CTS_USES_VULKANSC
    case VK_COMPONENT_TYPE_BFLOAT16_KHR:
    case VK_COMPONENT_TYPE_FLOAT_E5M2_NV:
    case VK_COMPONENT_TYPE_FLOAT_E4M3_NV:
#endif
        return true;
    default:
        return false;
    }
}

bool isSIntType(VkComponentTypeKHR t)
{
    switch (t)
    {
    case VK_COMPONENT_TYPE_SINT8_KHR:
    case VK_COMPONENT_TYPE_SINT16_KHR:
    case VK_COMPONENT_TYPE_SINT32_KHR:
    case VK_COMPONENT_TYPE_SINT64_KHR:
        return true;
    default:
        return false;
    }
}

void CooperativeMatrixTestCase::initProgramsGLSL(SourceCollections &programCollection) const
{
    const char *suffix = (isKhr(m_data.useType) ? "" : "NV");
    const char *ext    = isKhr(m_data.useType) ? "#extension GL_KHR_cooperative_matrix : enable\n" :
                                                 "#extension GL_NV_cooperative_matrix : enable\n"
                                                 "#extension GL_NV_integer_cooperative_matrix : enable\n";
    const char *sat = (m_data.testType == TT_MATRIXMULADD_SATURATED) ? ", gl_MatrixOperandsSaturatingAccumulation" : "";
    std::stringstream css;
    css << "#version 450 core\n";
    css << "#pragma use_vulkan_memory_model\n";
    css << "#extension GL_KHR_shader_subgroup_basic : enable\n"
           "#extension GL_KHR_memory_scope_semantics : enable\n"
        << ext;
    css << "#extension GL_EXT_bfloat16 : enable\n"
           "#extension GL_EXT_float_e5m2 : enable\n"
           "#extension GL_EXT_float_e4m3 : enable\n";
    css << "#extension GL_EXT_shader_explicit_arithmetic_types : enable\n"
           "#extension GL_EXT_buffer_reference : enable\n"
           "#extension GL_NV_cooperative_matrix2 : enable\n"
           "#extension GL_EXT_bfloat16 : enable\n"
           "// strides overriden by spec constants\n"
           "layout(constant_id = 2) const int AStride = 1;\n"
           "layout(constant_id = 3) const int BStride = 1;\n"
           "layout(constant_id = 4) const int CStride = 1;\n"
           "layout(constant_id = 5) const int OStride = 1;\n"
           "layout(constant_id = 6) const int M = 1;\n"
           "layout(constant_id = 7) const int N = 1;\n"
           "layout(constant_id = 8) const int K = 1;\n"
           "layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z = 1) in;\n";

    if (m_data.testType == TT_MATRIXMULADD_PUSH_CONSTANTS)
    {
        css << "layout (push_constant, std430) uniform PCBlock {\n"
               "  int AStrideVar;\n"
               "  int BStrideVar;\n"
               "  int CStrideVar;\n"
               "  int OStrideVar;\n"
               "} pc;\n";
    }

    if (m_data.storageClass == SC_BUFFER_VARIABLE_POINTERS || m_data.storageClass == SC_WORKGROUP_VARIABLE_POINTERS)
        css << "#pragma use_variable_pointers\n";

    struct
    {
        string rows, cols;
    } dims[4];

    if (isMatrixMulAddOp(m_data.testType))
    {
        dims[0].rows = "M";
        dims[0].cols = "K";
        dims[1].rows = "K";
        dims[1].cols = "N";
        dims[2].rows = "M";
        dims[2].cols = "N";
        dims[3].rows = "M";
        dims[3].cols = "N";
    }
    else
    {
        if (isReduce2x2(m_data.testType))
        {
            dims[0].rows = "(M*2)";
            dims[0].cols = "(N*2)";
        }
        else
        {
            dims[0].rows = "M";
            dims[0].cols = "N";
        }
        dims[1].rows = "M";
        dims[1].cols = "N";
        dims[2].rows = "M";
        dims[2].cols = "N";
        if (isReduceChangeDim(m_data.testType))
        {
            dims[3].rows = "(M*" + std::to_string(reduceMScale(m_data.testType)) + ")";
            dims[3].cols = "(N*" + std::to_string(reduceNScale(m_data.testType)) + ")";
        }
        else if (m_data.testType == TT_TRANSPOSE_ACC_TO_B)
        {
            dims[2].rows = "N";
            dims[2].cols = "M";
            dims[3].rows = "N";
            dims[3].cols = "M";
        }
        else
        {
            dims[3].rows = "M";
            dims[3].cols = "N";
        }
    }

    const char *typeStrA = componentTypeInfo.at(m_data.inputType).typeName;
    const char *typeStrB = componentTypeInfo.at(m_data.inputType).typeName;
    const char *typeStrC = componentTypeInfo.at(m_data.outputType).typeName;
    const char *typeStrO = componentTypeInfo.at(m_data.outputType).typeName;
    string inputType;
    string outputType;
    string divisorA;
    string divisorB;
    string divisorC;
    string divisorO;
    string *divisors[4] = {&divisorA, &divisorB, &divisorC, &divisorO};

    string scopeStr = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ? "gl_ScopeWorkgroup" : "gl_ScopeSubgroup";

    if (m_data.testType == TT_MULTICOMPONENT_LOAD)
    {
        const char *componentSuffix = m_data.inputComponentCount == 2 ? "vec2" :
                                      m_data.inputComponentCount == 4 ? "vec4" :
                                                                        "";

        inputType = string(componentTypeInfo.at(m_data.inputType).vecprefix) + componentSuffix;

        typeStrA = inputType.c_str();
        typeStrB = inputType.c_str();
        divisorA = m_data.inputComponentCount == 2 ? "/2" : m_data.inputComponentCount == 4 ? "/4" : "";
        divisorB = divisorA;
    }

    if (m_data.testType == TT_MULTICOMPONENT_SAVE)
    {
        const char *componentSuffix = m_data.outputComponentCount == 2 ? "vec2" :
                                      m_data.outputComponentCount == 4 ? "vec4" :
                                                                         "";

        outputType = string(componentTypeInfo.at(m_data.outputType).vecprefix) + componentSuffix;

        typeStrC = outputType.c_str();
        typeStrO = outputType.c_str();
        divisorC = m_data.outputComponentCount == 2 ? "/2" : m_data.outputComponentCount == 4 ? "/4" : "";
        divisorO = divisorC;
    }

    css << "const int workgroupsX = " << m_data.workgroupsX << ";\n";
    if (m_data.scope != VK_SCOPE_WORKGROUP_KHR)
    {
        css << "const uvec2 subgroupsPerWG = uvec2(" << m_data.subgroupsPerWorkgroupX << ", "
            << m_data.subgroupsPerWorkgroupY << ");\n";
    }

    // Test loading from a struct
    string typeStrAStruct = typeStrA;
    if (m_data.storageClass != SC_WORKGROUP && m_data.storageClass != SC_WORKGROUP_VARIABLE_POINTERS &&
        m_data.addrMethod != ADDR_LINEAR)
    {
        css << "struct StructA { " << typeStrA << " y; };\n";
        typeStrAStruct = "StructA";
    }

    if (m_data.storageClass == SC_PHYSICAL_STORAGE_BUFFER)
    {
        css << "layout(buffer_reference) buffer InputA { " << typeStrAStruct << " x[]; };\n";
        css << "layout(buffer_reference) buffer InputB { " << typeStrB << " x[]; };\n";
        css << "layout(buffer_reference) buffer InputC { " << typeStrC << " x[]; };\n";
        css << "layout(buffer_reference) buffer Output { " << typeStrO << " x[]; };\n";
        css << "layout(set=0, binding=4) buffer Params { InputA inputA; InputB inputB; InputC inputC; Output outputO; "
               "} params;\n";
    }
    else
    {
        css << "layout(set=0, binding=0) coherent buffer InputA { " << typeStrAStruct << " x[]; } inputA;\n";
        css << "layout(set=0, binding=1) coherent buffer InputB { " << typeStrB << " x[]; } inputB;\n";
        css << "layout(set=0, binding=2) coherent buffer InputC { " << typeStrC << " x[]; } inputC;\n";
        css << "layout(set=0, binding=3) coherent buffer Output { " << typeStrO << " x[]; } outputO;\n";
    }

    if (m_data.storageClass == SC_WORKGROUP || m_data.storageClass == SC_WORKGROUP_VARIABLE_POINTERS)
    {
        string scale = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ? "1" : "subgroupsPerWG.x * subgroupsPerWG.y";
        css << "shared " << typeStrA << " sharedA[" << dims[0].rows << " * " << dims[0].cols << " * " << scale
            << "];\n";
        css << "shared " << typeStrB << " sharedB[" << dims[1].rows << " * " << dims[1].cols << " * " << scale
            << "];\n";
        css << "shared " << typeStrC << " sharedC[" << dims[2].rows << " * " << dims[2].cols << " * " << scale
            << "];\n";
        css << "shared " << typeStrO << " sharedO[" << dims[3].rows << " * " << dims[3].cols << " * " << scale
            << "];\n";
    }

    std::stringstream matAType, matBType, matCType, outputMatType;

    // GLSL only considers types the same if any spec constants are the same and have
    // no operations. So for 2x2 reductions, where A has M*2/N*2 rows and cols, we need
    // to put that in a variable. But we can't for other tests, where we e.g. want to
    // assign matA to matO.
    if (isReduce2x2(m_data.testType))
    {
        css << "const int ARows = " << dims[0].rows << ";\n";
        css << "const int ACols = " << dims[0].cols << ";\n";
    }
    else
    {
        css << "#define ARows " << dims[0].rows << "\n";
        css << "#define ACols " << dims[0].cols << "\n";
    }
    if (isReduceChangeDim(m_data.testType))
    {
        css << "const int ORows = " << dims[3].rows << ";\n";
        css << "const int OCols = " << dims[3].cols << ";\n";
    }
    else
    {
        css << "#define ORows " << dims[3].rows << "\n";
        css << "#define OCols " << dims[3].cols << "\n";
    }

    const char *sameType = m_data.useType == UT_KHR_A      ? "gl_MatrixUseA" :
                           m_data.useType == UT_KHR_B      ? "gl_MatrixUseB" :
                           m_data.useType == UT_KHR_Result ? "gl_MatrixUseAccumulator" :
                                                             "Invalid use";

    if (isKhr(m_data.useType))
    {
        const bool useSame = !isMatrixMulAddOp(m_data.testType);
        const char *atype  = useSame ? sameType : "gl_MatrixUseA";
        const char *btype  = useSame ? sameType : "gl_MatrixUseB";
        const char *ctype  = useSame ? sameType : "gl_MatrixUseAccumulator";
        const char *rtype  = useSame ? sameType : "gl_MatrixUseAccumulator";

        if (m_data.testType == TT_CONVERT_ACC_TO_A)
        {
            atype = "gl_MatrixUseAccumulator";
            btype = "gl_MatrixUseAccumulator";
            ctype = "gl_MatrixUseA";
            rtype = "gl_MatrixUseA";
        }
        else if (m_data.testType == TT_CONVERT_ACC_TO_B || m_data.testType == TT_TRANSPOSE_ACC_TO_B)
        {
            atype = "gl_MatrixUseAccumulator";
            btype = "gl_MatrixUseAccumulator";
            ctype = "gl_MatrixUseB";
            rtype = "gl_MatrixUseB";
        }

        matAType << "coopmat<" << componentTypeInfo.at(m_data.inputType).typeName << ", " << scopeStr
                 << ", ARows, ACols, " << atype << ">";
        matBType << "coopmat<" << componentTypeInfo.at(m_data.inputType).typeName << ", " << scopeStr << ", "
                 << dims[1].rows << ", " << dims[1].cols << ", " << btype << ">";
        matCType << "coopmat<" << componentTypeInfo.at(m_data.outputType).typeName << ", " << scopeStr << ", "
                 << dims[2].rows << ", " << dims[2].cols << ", " << ctype << ">";
        outputMatType << "coopmat<" << componentTypeInfo.at(m_data.outputType).typeName << ", " << scopeStr
                      << ", ORows, OCols, " << rtype << ">";
    }
    else
    {
        matAType << componentTypeInfo.at(m_data.inputType).coopmatTypeName << "<"
                 << componentTypeInfo.at(m_data.inputType).bits << ", " << scopeStr << ", ARows, ACols>";
        matBType << componentTypeInfo.at(m_data.inputType).coopmatTypeName << "<"
                 << componentTypeInfo.at(m_data.inputType).bits << ", " << scopeStr << ", " << dims[1].rows << ", "
                 << dims[1].cols << ">";
        matCType << componentTypeInfo.at(m_data.outputType).coopmatTypeName << "<"
                 << componentTypeInfo.at(m_data.outputType).bits << ", " << scopeStr << ", " << dims[2].rows << ", "
                 << dims[2].cols << ">";
        outputMatType << componentTypeInfo.at(m_data.outputType).coopmatTypeName << "<"
                      << componentTypeInfo.at(m_data.outputType).bits << ", " << scopeStr << ", ORows, OCols>";
    }

    css << matAType.str() << " matA;\n";
    css << matBType.str() << " matB;\n";
    css << matCType.str() << " matC;\n";
    css << outputMatType.str() << " matO;\n";

    if (m_data.testType == TT_CONSTANT)
        css << "const " << outputMatType.str() << " matConst = " << outputMatType.str() << "(1.0);\n";

    if (m_data.testType == TT_FUNC || m_data.testType == TT_FUNC_CONST_IN)
    {
        std::string qual = m_data.testType == TT_FUNC_CONST_IN ? "const in " : "";
        css << matAType.str() << " f(" << qual << matAType.str() << " m) { return -m; }\n";
    }

    if (m_data.testType == TT_PER_ELEMENT_OP || m_data.testType == TT_PER_ELEMENT_OP_MAT)
    {
        std::string type = componentTypeInfo.at(m_data.inputType).typeName;
        css << type << " elemOp(const in uint32_t row, const in uint32_t col, const in " << type << " elem, const in "
            << type
            << " other) {\n"
               "    return elem + other;\n"
               "}\n";
    }
    else if (m_data.testType == TT_PER_ELEMENT_OP_ROW_COL)
    {
        std::string type = componentTypeInfo.at(m_data.inputType).typeName;
        css << type << " elemOpRowCol(const in uint32_t row, const in uint32_t col, const in " << type
            << " elem) {\n"
               "    return elem + "
            << type
            << "(row*3 + col);\n"
               "}\n";
    }
    else if (m_data.testType == TT_PER_ELEMENT_OP_STRUCT)
    {
        std::string type = componentTypeInfo.at(m_data.inputType).typeName;
        css << "struct ParamType { " << type << " x; };\n";
        std::string paramType = "ParamType";
        css << type << " elemOp(const in uint32_t row, const in uint32_t col, const in " << type << " elem, const in "
            << paramType
            << " other) {\n"
               "    return elem + other.x;\n"
               "}\n";
    }
    else if (isReduceOp(m_data.testType))
    {
        std::string type = componentTypeInfo.at(m_data.inputType).typeName;
        css << type << " combineOp(const in " << type << " a, const in " << type << " b) {\n";
        if (isReduceSum(m_data.testType))
        {
            css << "    return a + b;\n";
        }
        else if (isReduceMin(m_data.testType))
        {
            css << "    return min(a, b);\n";
        }
        css << "}\n";
    }

    if (m_data.testType == TT_MATRIXMULADD_DEQUANT)
    {
        // 4-bit elements [0,15) with -3 bias and scale of 0.5*(msb?2:1).
        css << "layout(buffer_reference, std430, buffer_reference_align = 1) buffer decodeBuf {\n"
               "   uint8_t bits["
            << blockSize[0] * blockSize[1] / 2
            << "];\n"
               "};\n";

        css << typeStrA
            << " decodeFunc(const in decodeBuf b, const in uint32_t blockCoords[2], const in uint32_t coordInBlock[2]) "
               "{\n"
               "   uint32_t idx = coordInBlock[0] * "
            << blockSize[1]
            << " + coordInBlock[1];\n"
               "   uint32_t arrayidx = idx / 2;\n"
               "   uint32_t shift = (idx & 1) * 4;\n"
               "   int32_t bits = int32_t(b.bits[arrayidx]);\n"
               "   bits = (bits >> shift) & 0xF;\n"
               "   return "
            << typeStrA
            << "(0.5 * float((bits & 7) - 3) * (1+(bits/8)));\n"
               "}\n";
    }
    else if (m_data.addrMethod == ADDR_DECODE)
    {
        css << "layout(buffer_reference, std430, buffer_reference_align = "
            << (componentTypeInfo.at(m_data.inputType).bits / 8)
            << ") buffer decodeBuf {\n"
               "   "
            << typeStrA << " f[" << blockSize[0] * blockSize[1]
            << "];\n"
               "};\n";

        // Lookup from coord in block, and add f(blockCoords)
        css << typeStrA
            << " decodeFunc(const in decodeBuf b, const in uint32_t blockCoords[2], const in uint32_t coordInBlock[2]) "
               "{\n"
               "   return b.f[coordInBlock[0] * "
            << blockSize[1] << " + coordInBlock[1]] + " << typeStrA
            << "((2*blockCoords[0] + blockCoords[1]) & 3);\n"
               "}\n";
    }

    css << "void main()\n"
           "{\n";
    if (m_data.scope == VK_SCOPE_WORKGROUP_KHR)
    {
        css << "   uvec2 matrixID = uvec2(gl_WorkGroupID.xy);\n";
    }
    else
    {
        css <<
            // matrixID is the x,y index of the matrix owned by this subgroup.
            "   uvec2 subgroupXY = uvec2(gl_SubgroupID % subgroupsPerWG.x, gl_SubgroupID / subgroupsPerWG.x);\n"
            "   uvec2 matrixID = uvec2(gl_WorkGroupID.xy) * subgroupsPerWG + subgroupXY;\n";
    }

    if (m_data.storageClass == SC_PHYSICAL_STORAGE_BUFFER)
    {
        css << "   InputA inputA = params.inputA;\n";
        css << "   InputB inputB = params.inputB;\n";
        css << "   InputC inputC = params.inputC;\n";
        css << "   Output outputO = params.outputO;\n";
    }

    string strides[4];
    string heights[4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        if (m_data.scope == VK_SCOPE_WORKGROUP_KHR)
        {
            strides[i] =
                (m_data.colMajor ? dims[i].rows : dims[i].cols) + string(" * ") + de::toString(m_data.workgroupsX);
            heights[i] =
                (m_data.colMajor ? dims[i].cols : dims[i].rows) + string(" * ") + de::toString(m_data.workgroupsY);
        }
        else
        {
            strides[i] = (m_data.colMajor ? dims[i].rows : dims[i].cols) + string(" * ") +
                         de::toString(m_data.subgroupsPerWorkgroupX * m_data.workgroupsX);
            heights[i] = (m_data.colMajor ? dims[i].cols : dims[i].rows) + string(" * ") +
                         de::toString(m_data.subgroupsPerWorkgroupY * m_data.workgroupsY);
        }
    }

    if (m_data.addrMethod != ADDR_LINEAR)
    {
        css << "   int offset00 = int(" << (m_data.colMajor ? dims[0].cols : dims[0].rows)
            << " * matrixID.y); int offset01 = int(" << (m_data.colMajor ? dims[0].rows : dims[0].cols)
            << " * matrixID.x);\n";
        css << "   int offset10 = int(" << (m_data.colMajor ? dims[1].cols : dims[1].rows)
            << " * matrixID.y); int offset11 = int(" << (m_data.colMajor ? dims[1].rows : dims[1].cols)
            << " * matrixID.x);\n";
        css << "   int offset20 = int(" << (m_data.colMajor ? dims[2].cols : dims[2].rows)
            << " * matrixID.y); int offset21 = int(" << (m_data.colMajor ? dims[2].rows : dims[2].cols)
            << " * matrixID.x);\n";
        css << "   int offset30 = int(" << (m_data.colMajor ? dims[3].cols : dims[3].rows)
            << " * matrixID.y); int offset31 = int(" << (m_data.colMajor ? dims[3].rows : dims[3].cols)
            << " * matrixID.x);\n";

        css << "   uint span00 = " << (m_data.colMajor ? dims[0].cols : dims[0].rows)
            << "; uint span01 = " << (m_data.colMajor ? dims[0].rows : dims[0].cols) << ";\n";
        css << "   uint span10 = " << (m_data.colMajor ? dims[1].cols : dims[1].rows)
            << "; uint span11 = " << (m_data.colMajor ? dims[1].rows : dims[1].cols) << ";\n";
        css << "   uint span20 = " << (m_data.colMajor ? dims[2].cols : dims[2].rows)
            << "; uint span21 = " << (m_data.colMajor ? dims[2].rows : dims[2].cols) << ";\n";
        css << "   uint span30 = " << (m_data.colMajor ? dims[3].cols : dims[3].rows)
            << "; uint span31 = " << (m_data.colMajor ? dims[3].rows : dims[3].cols) << ";\n";
    }

    if (isClampTest(m_data.testType))
    {
        // Clamp tests adjust offset and dimensions to shrink the load boundary by 3 on each edge
        css << "   offset00 -= 3; offset01 -= 3;\n";
        css << "   offset10 -= 3; offset11 -= 3;\n";
        css << "   offset20 -= 3; offset21 -= 3;\n";
    }

    if (m_data.testType == TT_MATRIXMULADD_PUSH_CONSTANTS)
    {
        strides[0] = "pc.AStrideVar";
        strides[1] = "pc.BStrideVar";
        strides[2] = "pc.CStrideVar";
        strides[3] = "pc.OStrideVar";
    }

    // element<i> is the starting element in buffer memory.
    // elementS<i> is the starting element in shared memory.
    css << "   uint element0 = (" << strides[0] << " * " << (m_data.colMajor ? dims[0].cols : dims[0].rows)
        << " * matrixID.y + " << (m_data.colMajor ? dims[0].rows : dims[0].cols) << " * matrixID.x)" << divisorA
        << ";\n"
           "   uint element1 = ("
        << strides[1] << " * " << (m_data.colMajor ? dims[1].cols : dims[1].rows) << " * matrixID.y + "
        << (m_data.colMajor ? dims[1].rows : dims[1].cols) << " * matrixID.x)" << divisorB
        << ";\n"
           "   uint element2 = ("
        << strides[2] << " * " << (m_data.colMajor ? dims[2].cols : dims[2].rows) << " * matrixID.y + "
        << (m_data.colMajor ? dims[2].rows : dims[2].cols) << " * matrixID.x)" << divisorC
        << ";\n"
           "   uint element3 = ("
        << strides[3] << " * " << (m_data.colMajor ? dims[3].cols : dims[3].rows) << " * matrixID.y + "
        << (m_data.colMajor ? dims[3].rows : dims[3].cols) << " * matrixID.x)" << divisorO
        << ";\n"
           "   uint elementS0, elementS1, elementS2, elementS3;\n";

    // For shared memory tests, copy the matrix from buffer memory into
    // workgroup memory. For simplicity, do it all on a single thread.
    if (m_data.storageClass == SC_WORKGROUP || m_data.storageClass == SC_WORKGROUP_VARIABLE_POINTERS)
    {
        const char *name[] = {
            "sharedA",
            "sharedB",
            "sharedC",
        };
        const char *inputName[] = {
            "inputA",
            "inputB",
            "inputC",
        };
        for (uint32_t m = 0; m < 4; ++m)
        {
            string sharedStride = strides[m] + " / workgroupsX";
            if (m_data.scope == VK_SCOPE_WORKGROUP_KHR)
            {
                css << "       elementS" << m << " = 0;\n";
            }
            else
            {
                css << "       elementS" << m << " = (" << sharedStride << " * "
                    << (m_data.colMajor ? dims[m].cols : dims[m].rows) << " * subgroupXY.y + "
                    << (m_data.colMajor ? dims[m].rows : dims[m].cols) << " * subgroupXY.x)" << *divisors[m] << ";\n";
            }
        }
        css << "   if (subgroupElect()) {\n";
        // copy all three input buffers.
        for (uint32_t m = 0; m < 3; ++m)
        {
            if (m == 0 && (m_data.testType == TT_LENGTH || m_data.testType == TT_CONSTANT))
            {
                // A matrix not needed
                continue;
            }
            if (m == 1)
            {
                // B matrix not needed
                if (isReduceOp(m_data.testType) || isClampTest(m_data.testType))
                {
                    continue;
                }
                switch (m_data.testType)
                {
                case TT_CONSTANT:
                case TT_LENGTH:
                case TT_CONVERT:
                case TT_CONVERT_SAT:
                case TT_NEGATE:
                case TT_FUNC:
                case TT_FUNC_CONST_IN:
                case TT_MATRIXTIMESSCALAR:
                case TT_MULTICOMPONENT_LOAD:
                case TT_MULTICOMPONENT_SAVE:
                case TT_CONVERT_ACC_TO_A:
                case TT_CONVERT_ACC_TO_B:
                case TT_TRANSPOSE_ACC_TO_B:
                case TT_PER_ELEMENT_OP:
                case TT_PER_ELEMENT_OP_MAT:
                case TT_PER_ELEMENT_OP_STRUCT:
                case TT_PER_ELEMENT_OP_ROW_COL:
                case TT_SPACETODEPTH:
                    continue;
                default:
                    break;
                }
            }
            if (m == 2 && !isMatrixMulAddOp(m_data.testType))
            {
                // C matrix only needed for matmul
                continue;
            }
            string sharedStride = strides[m] + " / workgroupsX";
            css << "       for (int i = 0; i < " << dims[m].rows
                << "; ++i) {\n"
                   "       for (int j = 0; j < "
                << dims[m].cols
                << "; ++j) {\n"
                   "           int localElementInput = ("
                << strides[m] << " * " << (m_data.colMajor ? "j" : "i") << " + " << (m_data.colMajor ? "i" : "j") << ")"
                << *divisors[m]
                << ";\n"
                   "           int localElementShared = ("
                << sharedStride << " * " << (m_data.colMajor ? "j" : "i") << " + " << (m_data.colMajor ? "i" : "j")
                << ")" << *divisors[m]
                << ";\n"
                   "           "
                << name[m] << "[elementS" << m << " + localElementShared] = " << inputName[m] << ".x[element" << m
                << " + localElementInput];\n"
                   "       }\n"
                   "       }\n";
            strides[m] = sharedStride;
        }
        css << "   }\n";
        css << "   controlBarrier(" << scopeStr << ", " << scopeStr
            << ", gl_StorageSemanticsShared, gl_SemanticsAcquireRelease);\n";
    }

    const char *colMajorNV = (m_data.colMajor ? "true" : "false");
    const char *colMajorKHR =
        (m_data.colMajor ? "gl_CooperativeMatrixLayoutColumnMajor" : "gl_CooperativeMatrixLayoutRowMajor");
    const char *colMajor = (isKhr(m_data.useType) ? colMajorKHR : colMajorNV);

    string loadStrides[3] = {strides[0] + divisorA, strides[1] + divisorB, strides[2] + divisorC};
    // Load with a stride of 0
    if (m_data.testType == TT_MATRIXMULADD_STRIDE0)
        loadStrides[0] = loadStrides[1] = loadStrides[2] = "0";

    std::string clampString;
    switch (m_data.testType)
    {
    default:
        break;
    case TT_CLAMPCONSTANT:
        clampString = "gl_CooperativeMatrixClampModeConstantNV";
        break;
    case TT_CLAMPTOEDGE:
        clampString = "gl_CooperativeMatrixClampModeClampToEdgeNV";
        break;
    case TT_CLAMPREPEAT:
        clampString = "gl_CooperativeMatrixClampModeRepeatNV";
        break;
    case TT_CLAMPMIRRORREPEAT:
        clampString = "gl_CooperativeMatrixClampModeMirrorRepeatNV";
        break;
    }

    if (!isTensorLayoutTest(m_data.testType))
    {
        if (m_data.addrMethod != ADDR_LINEAR)
        {

            if (m_data.testType == TT_MATRIXMULADD_STRIDE0)
            {
                heights[0] = heights[1] = heights[2] = "1";
            }

            if (isClampTest(m_data.testType))
            {
                css << "   tensorLayoutNV<2, " << clampString << "> tensorLayout0 = createTensorLayoutNV(2, "
                    << clampString
                    << ");\n"
                       "   tensorLayoutNV<2, "
                    << clampString << "> tensorLayout1 = createTensorLayoutNV(2, " << clampString
                    << ");\n"
                       "   tensorLayoutNV<2, "
                    << clampString << "> tensorLayout2 = createTensorLayoutNV(2, " << clampString << ");\n";

                css << "   tensorLayout0 = setTensorLayoutDimensionNV(tensorLayout0, " << heights[0] << " - 6, "
                    << strides[0]
                    << " - 6);\n"
                       "   tensorLayout1 = setTensorLayoutDimensionNV(tensorLayout1, "
                    << heights[1] << " - 6, " << strides[1]
                    << " - 6);\n"
                       "   tensorLayout2 = setTensorLayoutDimensionNV(tensorLayout2, "
                    << heights[2] << " - 6, " << strides[2] << " - 6);\n";
                css << "   tensorLayout0 = setTensorLayoutStrideNV(tensorLayout0, " << strides[0]
                    << ", 1);\n"
                       "   tensorLayout1 = setTensorLayoutStrideNV(tensorLayout1, "
                    << strides[1]
                    << ", 1);\n"
                       "   tensorLayout2 = setTensorLayoutStrideNV(tensorLayout2, "
                    << strides[2] << ", 1);\n";
                if (m_data.inputType == VK_COMPONENT_TYPE_FLOAT32_KHR)
                {
                    css << "   tensorLayout0 = setTensorLayoutClampValueNV(tensorLayout0, floatBitsToUint(0.5));\n";
                }
                else if (m_data.inputType == VK_COMPONENT_TYPE_FLOAT16_KHR)
                {
                    // 0x3800 == 0.5f in fp16
                    css << "   tensorLayout0 = setTensorLayoutClampValueNV(tensorLayout0, 0x3800);\n";
                }
                else if (m_data.inputType == VK_COMPONENT_TYPE_BFLOAT16_KHR)
                {
                    // 0x3f00 == 0.5f in bf16
                    css << "   tensorLayout0 = setTensorLayoutClampValueNV(tensorLayout0, 0x3f00);\n";
                }
                else if (m_data.inputType == VK_COMPONENT_TYPE_FLOAT_E5M2_NV)
                {
                    // 0x38 == 0.5f in e5m2
                    css << "   tensorLayout0 = setTensorLayoutClampValueNV(tensorLayout0, 0x38);\n";
                }
                else if (m_data.inputType == VK_COMPONENT_TYPE_FLOAT_E4M3_NV)
                {
                    // 0x30 == 0.5f in e4m3
                    css << "   tensorLayout0 = setTensorLayoutClampValueNV(tensorLayout0, 0x30);\n";
                }
                else
                {
                    css << "   tensorLayout0 = setTensorLayoutClampValueNV(tensorLayout0, 17);\n";
                }
            }
            else
            {
                css << "   tensorLayoutNV<2> tensorLayout0 = createTensorLayoutNV(2);\n"
                       "   tensorLayoutNV<2> tensorLayout1 = createTensorLayoutNV(2);\n"
                       "   tensorLayoutNV<2> tensorLayout2 = createTensorLayoutNV(2);\n";

                if (m_data.addrMethod == ADDR_BLOCKSIZE || m_data.addrMethod == ADDR_DECODE)
                {
                    css << "   tensorLayout0 = setTensorLayoutBlockSizeNV(tensorLayout0, " << blockSize[0] << ", "
                        << blockSize[1]
                        << ");\n"
                           "   tensorLayout1 = setTensorLayoutBlockSizeNV(tensorLayout1, "
                        << blockSize[0] << ", " << blockSize[1] << ");\n";
                }

                css << "   tensorLayout0 = setTensorLayoutDimensionNV(tensorLayout0, " << heights[0] << ", "
                    << strides[0]
                    << ");\n"
                       "   tensorLayout1 = setTensorLayoutDimensionNV(tensorLayout1, "
                    << heights[1] << ", " << strides[1]
                    << ");\n"
                       "   tensorLayout2 = setTensorLayoutDimensionNV(tensorLayout2, "
                    << heights[2] << ", " << strides[2] << ");\n";
            }

            string viewParam0, viewParam1, viewParam2;
            string decodeFunc;

            if (m_data.testType == TT_MATRIXMULADD_STRIDE0)
            {
                if (m_data.colMajor)
                {
                    css << "   tensorViewNV<2, true, 1, 0> stride0View0 = createTensorViewNV(2, true, 1, 0);\n"
                           "   tensorViewNV<2, true, 1, 0> stride0View1 = createTensorViewNV(2, true, 1, 0);\n"
                           "   tensorViewNV<2, true, 1, 0> stride0View2 = createTensorViewNV(2, true, 1, 0);\n";
                }
                else
                {
                    css << "   tensorViewNV<2, true> stride0View0 = createTensorViewNV(2, true);\n"
                           "   tensorViewNV<2, true> stride0View1 = createTensorViewNV(2, true);\n"
                           "   tensorViewNV<2, true> stride0View2 = createTensorViewNV(2, true);\n";
                }
                css << "   stride0View0 = setTensorViewDimensionsNV(stride0View0, span00, span01);\n"
                       "   stride0View1 = setTensorViewDimensionsNV(stride0View1, span10, span11);\n"
                       "   stride0View2 = setTensorViewDimensionsNV(stride0View2, span20, span21);\n"
                       "   stride0View0 = setTensorViewStrideNV(stride0View0, 0, 1);\n"
                       "   stride0View1 = setTensorViewStrideNV(stride0View1, 0, 1);\n"
                       "   stride0View2 = setTensorViewStrideNV(stride0View2, 0, 1);\n";

                viewParam0 = ", stride0View0";
                viewParam1 = ", stride0View1";
                viewParam2 = ", stride0View2";
            }
            else if (m_data.colMajor)
            {
                css << "   tensorViewNV<2, true, 1, 0> colMajorView0 = createTensorViewNV(2, true, 1, 0);\n"
                       "   tensorViewNV<2, true, 1, 0> colMajorView1 = createTensorViewNV(2, true, 1, 0);\n"
                       "   tensorViewNV<2, true, 1, 0> colMajorView2 = createTensorViewNV(2, true, 1, 0);\n"
                       "   colMajorView0 = setTensorViewDimensionsNV(colMajorView0, span00, span01);\n"
                       "   colMajorView1 = setTensorViewDimensionsNV(colMajorView1, span10, span11);\n"
                       "   colMajorView2 = setTensorViewDimensionsNV(colMajorView2, span20, span21);\n";

                viewParam0 = ", colMajorView0";
                viewParam1 = ", colMajorView1";
                viewParam2 = ", colMajorView2";
            }

            if (m_data.addrMethod == ADDR_DECODE)
            {
                decodeFunc = ", decodeFunc";
            }

            if (m_data.storageClass == SC_WORKGROUP || m_data.storageClass == SC_WORKGROUP_VARIABLE_POINTERS)
            {
                if (m_data.scope == VK_SCOPE_WORKGROUP_KHR)
                {
                    css << "   elementS0 = elementS1 = elementS2 = 0;\n";
                }
                css << "   tensorLayout0 = sliceTensorLayoutNV(tensorLayout0, 0, span00, 0, span01);\n"
                       "   tensorLayout1 = sliceTensorLayoutNV(tensorLayout1, 0, span10, 0, span11);\n"
                       "   tensorLayout2 = sliceTensorLayoutNV(tensorLayout2, 0, span20, 0, span21);\n";
                css << "   coopMatLoadTensorNV(matA, sharedA, elementS0, tensorLayout0" << viewParam0
                    << ");\n"
                       "   coopMatLoadTensorNV(matB, sharedB, elementS1, tensorLayout1"
                    << viewParam1
                    << ");\n"
                       "   coopMatLoadTensorNV(matC, sharedC, elementS2, tensorLayout2"
                    << viewParam2 << ");\n";
            }
            else
            {
                css << "   tensorLayout0 = sliceTensorLayoutNV(tensorLayout0, offset00, span00, offset01, span01);\n"
                       "   tensorLayout1 = sliceTensorLayoutNV(tensorLayout1, offset10, span10, offset11, span11);\n"
                       "   tensorLayout2 = sliceTensorLayoutNV(tensorLayout2, offset20, span20, offset21, span21);\n";
                css << "   coopMatLoadTensorNV(matA, inputA.x, 0, tensorLayout0" << viewParam0 << decodeFunc
                    << ");\n"
                       "   coopMatLoadTensorNV(matB, inputB.x, 0, tensorLayout1"
                    << viewParam1 << decodeFunc
                    << ");\n"
                       "   coopMatLoadTensorNV(matC, inputC.x, 0, tensorLayout2"
                    << viewParam2 << ");\n";
            }
        }
        else
        {
            if (m_data.storageClass == SC_WORKGROUP || m_data.storageClass == SC_WORKGROUP_VARIABLE_POINTERS)
            {
                css << "   coopMatLoad" << suffix << "(matA, sharedA, elementS0, " << loadStrides[0] << ", " << colMajor
                    << ");\n"
                       "   coopMatLoad"
                    << suffix << "(matB, sharedB, elementS1, " << loadStrides[1] << ", " << colMajor
                    << ");\n"
                       "   coopMatLoad"
                    << suffix << "(matC, sharedC, elementS2, " << loadStrides[2] << ", " << colMajor << ");\n";
            }
            else
            {
                css << "   coopMatLoad" << suffix << "(matA, inputA.x, element0, " << loadStrides[0] << ", " << colMajor
                    << ");\n"
                       "   coopMatLoad"
                    << suffix << "(matB, inputB.x, element1, " << loadStrides[1] << ", " << colMajor
                    << ");\n"
                       "   coopMatLoad"
                    << suffix << "(matC, inputC.x, element2, " << loadStrides[2] << ", " << colMajor << ");\n";
            }
        }
    }

    if (m_data.testType == TT_COMPOSITE_ARRAY || m_data.testType == TT_MATRIXMULADD_ARRAY)
    {
        css << "   " << matAType.str() << " matAArr[2];\n    matAArr[1] = matA; matAArr[0] = " << matAType.str()
            << "(0.0);\n"
               "   "
            << matBType.str() << " matBArr[2];\n    matBArr[1] = matB; matBArr[0] = " << matBType.str()
            << "(0.0);\n"
               "   "
            << matCType.str() << " matCArr[2];\n    matCArr[1] = matC; matCArr[0] = " << matCType.str()
            << "(0.0);\n"
               "   "
            << outputMatType.str() << " matOArr[2];\n";
    }

    switch (m_data.testType)
    {
    default:
        DE_ASSERT(0);
        // fall through
    case TT_LENGTH:
        css << "   matO = " << outputMatType.str() << "(matO.length());\n";
        break;
    case TT_CONSTANT:
        css << "   matO = matConst;\n";
        break;
    case TT_CONVERT:
        css << "   matO = " << outputMatType.str() << "(matA);\n";
        break;
    case TT_CONVERT_SAT:
        css << "   saturatedConvertEXT(matO, matA);\n";
        break;
    case TT_COMPOSITE:
        css << "   " << matAType.str() << " t = " << matAType.str()
            << "(matB[0]);\n"
               "   for (int i = 1; i < matA.length(); ++i) {\n"
               "       matO[i] = matA[i] + matB[i];\n"
               "   }\n"
               "   if (matA.length() > 0)\n"
               "       matO[0] = matA[0] + t[0];\n";
        break;
    case TT_COMPOSITE_RVALUE:
        css << "   for (int i = 1; i < matA.length(); ++i) {\n"
               "       matO[i] = matA[i];\n"
               "   }\n"
               "   "
            << matAType.str()
            << " t = matB;\n"
               "   if (matA.length() > 0) {\n"
               "       matO[0] = (t = matA)[0];\n"
               "   }\n";
        break;
    case TT_COMPOSITE_ARRAY:
        css << "   for (int i = 0; i < matA.length(); ++i) {\n"
               "       matOArr[1][i] = matAArr[1][i];\n"
               "   }\n";
        break;
    case TT_ADD:
        css << "   matO = matA + matB;\n";
        break;
    case TT_SUB:
        css << "   matO = matA - matB;\n";
        break;
    case TT_DIV:
        css << "   matO = matA / matB;\n";
        break;
    case TT_MUL:
        css << "   matO = matA * matB;\n";
        break;
    case TT_NEGATE:
        css << "   matO = -matA;\n";
        break;
    case TT_FUNC:
    case TT_FUNC_CONST_IN:
        css << "   matO = f(matA);\n";
        break;
    case TT_CLAMPTOEDGE:
    case TT_CLAMPCONSTANT:
    case TT_CLAMPREPEAT:
    case TT_CLAMPMIRRORREPEAT:
        css << "   matO = matA;\n";
        break;
    case TT_MATRIXTIMESSCALAR:
        css << "   matO = (" << typeStrA << "(2.0)*matA)*" << typeStrA << "(3.0);\n";
        break;
    case TT_MATRIXMULADD_DEQUANT:
    case TT_MATRIXMULADD_CROSS:
    case TT_MATRIXMULADD_STRIDE0:
    case TT_MATRIXMULADD_WRAPPING:
    case TT_MATRIXMULADD_SATURATED:
    case TT_MATRIXMULADD_PUSH_CONSTANTS:
    case TT_MATRIXMULADD:
        css << "   matO = coopMatMulAdd" << suffix << "(matA, matB, matC" << sat << ");\n";
        break;
    case TT_MATRIXMULADD_ARRAY:
        css << "   matOArr[1] = coopMatMulAdd" << suffix << "(matAArr[1], matBArr[1], matCArr[1]);\n";
        break;
    case TT_MULTICOMPONENT_LOAD:
        css << "   matO = matA;\n";
        break;
    case TT_MULTICOMPONENT_SAVE:
        css << "   matO = matA;\n";
        break;
    case TT_CONVERT_ACC_TO_A:
    case TT_CONVERT_ACC_TO_B:
        css << "   matO = " << outputMatType.str() << "(matA);\n";
        break;
    case TT_TRANSPOSE_ACC_TO_B:
        css << "   coopMatTransposeNV(matO, matA);\n";
        break;
    case TT_REDUCE_SUM_ROW:
    case TT_REDUCE_SUM_COL:
    case TT_REDUCE_SUM_ROWCOL:
    case TT_REDUCE_SUM_2X2:
    case TT_REDUCE_SUM_ROW_CHANGEDIM:
    case TT_REDUCE_SUM_COL_CHANGEDIM:
    case TT_REDUCE_SUM_ROWCOL_CHANGEDIM:
    case TT_REDUCE_MIN_ROW:
    case TT_REDUCE_MIN_COL:
    case TT_REDUCE_MIN_ROWCOL:
    case TT_REDUCE_MIN_2X2:
    {
        string rowCol = isReduce2x2(m_data.testType) ? "gl_CooperativeMatrixReduce2x2NV" :
                        isReduceRow(m_data.testType) ? "gl_CooperativeMatrixReduceRowNV" :
                        isReduceCol(m_data.testType) ? "gl_CooperativeMatrixReduceColumnNV" :
                                                       "gl_CooperativeMatrixReduceRowAndColumnNV";

        css << "   coopMatReduceNV(matO, matA, " << rowCol << ", combineOp);\n";
    }
    break;
    case TT_PER_ELEMENT_OP:
        css << "   coopMatPerElementNV(matO, matA, elemOp, " << componentTypeInfo.at(m_data.inputType).typeName
            << "(2.0));\n";
        break;
    case TT_PER_ELEMENT_OP_MAT:
        css << "   coopMatPerElementNV(matO, matA, elemOp, " << componentTypeInfo.at(m_data.inputType).typeName
            << "(2.0) * matA);\n";
        break;
    case TT_PER_ELEMENT_OP_ROW_COL:
        css << "   coopMatPerElementNV(matO, matA, elemOpRowCol);\n";
        break;
    case TT_PER_ELEMENT_OP_STRUCT:
        css << "   ParamType p; p.x = " << componentTypeInfo.at(m_data.inputType).typeName << "(2.0);\n";
        css << "   coopMatPerElementNV(matO, matA, elemOp, p);\n";
        break;
    case TT_TENSORLAYOUT_1D:
    case TT_TENSORLAYOUT_2D:
    case TT_TENSORLAYOUT_3D:
    case TT_TENSORLAYOUT_4D:
    case TT_TENSORLAYOUT_5D:
    case TT_TENSORLAYOUT_1D_CLIP:
    case TT_TENSORLAYOUT_2D_CLIP:
    case TT_TENSORLAYOUT_3D_CLIP:
    case TT_TENSORLAYOUT_4D_CLIP:
    case TT_TENSORLAYOUT_5D_CLIP:
    {
        uint32_t dim = GetDim(m_data.testType);

        css << "   tensorLayoutNV<" << dim << ", gl_CooperativeMatrixClampModeConstantNV> t = createTensorLayoutNV("
            << dim << ", gl_CooperativeMatrixClampModeConstantNV);\n";
        if (isTensorLayoutClipTest(m_data.testType))
        {
            css << "   tensorViewNV<" << dim << "> v = createTensorViewNV(" << dim << ");\n";
        }
        for (uint32_t i = 0; i < GetTensorLayoutNumCoords(dim); ++i)
        {
            uint32_t dimFactor = isTensorLayoutClipTest(m_data.testType) ? 2 : 1;

            stringstream mattype;
            mattype << "coopmat<" << componentTypeInfo.at(m_data.inputType).typeName << ", " << scopeStr << ", "
                    << dimFactor * GetTensorLayoutMatrixSizes(dim, i)[0] << ", "
                    << dimFactor * GetTensorLayoutMatrixSizes(dim, i)[1] << ", " << sameType << ">";
            css << "   " << mattype.str() << " tempmat" << i << ";\n";

            css << "   tempmat" << i << " = " << mattype.str() << "(0.5);\n";

            if (isTensorLayoutClipTest(m_data.testType))
            {
                // clip the double-size matrix to the requested size
                css << "   v = setTensorViewClipNV(v, 1, " << GetTensorLayoutMatrixSizes(dim, i)[0] << ", 1, "
                    << GetTensorLayoutMatrixSizes(dim, i)[1] << ");\n";
            }

            css << "   t = setTensorLayoutDimensionNV(t";
            for (uint32_t j = 0; j < dim; ++j)
            {
                css << ", " << GetTensorLayoutDim(dim)[j];
            }
            css << ");\n";

            css << "   t = sliceTensorLayoutNV(t";
            for (uint32_t j = 0; j < dim; ++j)
            {
                css << ", " << GetTensorLayoutLoadOffsets(dim, i)[j] << ", " << GetTensorLayoutSpan(dim, i)[j];
            }
            css << ");\n";
            css << "   coopMatLoadTensorNV(tempmat" << i << ", inputA.x, 0, t"
                << (isTensorLayoutClipTest(m_data.testType) ? ", v" : "") << ");\n";

            css << "   t = setTensorLayoutDimensionNV(t";
            for (uint32_t j = 0; j < dim; ++j)
            {
                css << ", " << GetTensorLayoutDim(dim)[j];
            }
            css << ");\n";

            css << "   t = sliceTensorLayoutNV(t";
            for (uint32_t j = 0; j < dim; ++j)
            {
                css << ", " << GetTensorLayoutStoreOffsets(dim, i)[j] << ", " << GetTensorLayoutSpan(dim, i)[j];
            }
            css << ");\n";
            css << "   coopMatStoreTensorNV(tempmat" << i << ", outputO.x, 0, t"
                << (isTensorLayoutClipTest(m_data.testType) ? ", v" : "") << ");\n";
        }
    }
    break;
    case TT_SPACETODEPTH:
        css << "   const uint32_t H = 32;\n"
               "   const uint32_t W = 32;\n"
               "   const uint32_t NumCh = 16;\n";
        css << "   tensorLayoutNV<3> t = createTensorLayoutNV(3);\n";
        css << "   tensorViewNV<5, true, 0, 2, 1, 3, 4> v = createTensorViewNV(5, true, 0, 2, 1, 3, 4);\n";

        {
            stringstream mattype;
            mattype << "coopmat<" << componentTypeInfo.at(m_data.inputType).typeName << ", " << scopeStr
                    << ", (H/2 * W/2), (4*NumCh)," << sameType << ">";
            css << "   " << mattype.str() << " tempmat;\n";
            css << "   tempmat = " << mattype.str() << "(0.5);\n";
        }

        css << "   t = setTensorLayoutDimensionNV(t, H, W, NumCh);\n";
        css << "   v = setTensorViewDimensionsNV(v, H/2, 2, W/2, 2, NumCh);\n";

        css << "   coopMatLoadTensorNV(tempmat, inputA.x, 0, t, v);\n";

        css << "   tensorLayoutNV<2> t2 = createTensorLayoutNV(2);\n";
        css << "   t2 = setTensorLayoutDimensionNV(t2, H/2 * W/2, 4*NumCh);";

        css << "   coopMatStoreTensorNV(tempmat, outputO.x, 0, t2);\n";
        break;
    }

    if (!isTensorLayoutTest(m_data.testType))
    {
        if (m_data.testType == TT_COMPOSITE_ARRAY || m_data.testType == TT_MATRIXMULADD_ARRAY)
        {
            css << "   matOArr[0] = " << outputMatType.str() << "(0.0);\n";
            css << "   matO = matOArr[1];\n";
        }

        if (m_data.storageClass == SC_WORKGROUP || m_data.storageClass == SC_WORKGROUP_VARIABLE_POINTERS)
        {
            string sharedStride = strides[3] + " / workgroupsX";
            if (m_data.addrMethod != ADDR_LINEAR)
            {
                css << "   tensorLayoutNV<2> tensorLayout3 = createTensorLayoutNV(2);\n"
                       "   tensorLayout3 = setTensorLayoutDimensionNV(tensorLayout3, "
                    << heights[3] << ", " << sharedStride
                    << ");\n"
                       "   tensorLayout3 = sliceTensorLayoutNV(tensorLayout3, 0, span30, 0, span31);\n";

                css << "   tensorViewNV<2, false, 1, 0> colMajorView3 = createTensorViewNV(2, false, 1, 0);\n";

                if (m_data.scope == VK_SCOPE_WORKGROUP_KHR)
                {
                    css << "   elementS3 = 0;\n";
                }
                css << "   coopMatStoreTensorNV(matO, sharedO, elementS3, tensorLayout3"
                    << (m_data.colMajor ? ", colMajorView3" : "") << ");\n";
            }
            else
            {
                css << "   coopMatStore" << suffix << "(matO, sharedO, elementS3, " << sharedStride << divisorO << ", "
                    << colMajor << ");\n";
            }
            css << "   controlBarrier(" << scopeStr << ", " << scopeStr
                << ", gl_StorageSemanticsShared, gl_SemanticsAcquireRelease);\n";
            css << "   if (subgroupElect()) {\n";
            css << "       for (int i = 0; i < " << dims[3].rows
                << "; ++i) {\n"
                   "       for (int j = 0; j < "
                << dims[3].cols
                << "; ++j) {\n"
                   "           int localElementInput = ("
                << strides[3] << " * " << (m_data.colMajor ? "j" : "i") << " + " << (m_data.colMajor ? "i" : "j") << ")"
                << *divisors[3]
                << ";\n"
                   "           int localElementShared = ("
                << sharedStride << " * " << (m_data.colMajor ? "j" : "i") << " + " << (m_data.colMajor ? "i" : "j")
                << ")" << *divisors[3]
                << ";\n"
                   "           outputO.x[element3 + localElementInput] = sharedO[elementS3 + localElementShared];\n"
                   "       }\n"
                   "       }\n";
            css << "   }\n";
            strides[3] = sharedStride;
        }
        else
        {
            if (m_data.addrMethod != ADDR_LINEAR)
            {
                if (isClampTest(m_data.testType))
                {
                    css << "   tensorLayoutNV<2, " << clampString << "> tensorLayout3 = createTensorLayoutNV(2, "
                        << clampString << ");\n";

                    // Shrink the width/height by 1
                    css << "   tensorLayout3 = setTensorLayoutDimensionNV(tensorLayout3, " << heights[3] << " - 1, "
                        << strides[3] << " - 1);\n";
                    css << "   tensorLayout3 = setTensorLayoutStrideNV(tensorLayout3, " << strides[3] << ", 1);\n";
                }
                else
                {
                    css << "   tensorLayoutNV<2> tensorLayout3 = createTensorLayoutNV(2);\n"
                           "   tensorLayout3 = setTensorLayoutDimensionNV(tensorLayout3, "
                        << heights[3] << ", " << strides[3] << ");\n";
                }

                css << "   tensorLayout3 = sliceTensorLayoutNV(tensorLayout3, offset30, span30, offset31, span31);\n";

                css << "   tensorViewNV<2, false, 1, 0> colMajorView3 = createTensorViewNV(2, false, 1, 0);\n";
                css << "   coopMatStoreTensorNV(matO, outputO.x, 0, tensorLayout3"
                    << (m_data.colMajor ? ", colMajorView3" : "") << ");\n";
            }
            else
            {
                css << "   coopMatStore" << suffix << "(matO, outputO.x, element3, " << strides[3] << divisorO << ", "
                    << colMajor << ");\n";
            }
        }
    }

    css << "}\n";

    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_6, 0u);

    programCollection.glslSources.add("test") << glu::ComputeSource(css.str()) << buildOptions;
}

std::string getTypeName(const VkComponentTypeKHR type)
{
    switch (type)
    {
    case VK_COMPONENT_TYPE_SINT8_KHR:
        return "s8";
    case VK_COMPONENT_TYPE_SINT32_KHR:
        return "s32";
    case VK_COMPONENT_TYPE_UINT8_KHR:
        return "u8";
    case VK_COMPONENT_TYPE_UINT32_KHR:
        return "u32";
    default:
        TCU_THROW(InternalError, "Support for this type is not implemented");
    }
}

size_t getTypeWidth(const VkComponentTypeKHR type)
{
    switch (type)
    {
    case VK_COMPONENT_TYPE_SINT8_KHR:
        return 1;
    case VK_COMPONENT_TYPE_SINT32_KHR:
        return 4;
    case VK_COMPONENT_TYPE_UINT8_KHR:
        return 1;
    case VK_COMPONENT_TYPE_UINT32_KHR:
        return 4;
    default:
        TCU_THROW(InternalError, "Support for this type is not implemented");
    }
}

std::string getOppositeSignednessTypeName(const VkComponentTypeKHR type)
{
    std::string result = getTypeName(type);

    if (result[0] == 'u')
        result[0] = 's';
    else if (result[0] == 's')
        result[0] = 'u';
    else
        TCU_THROW(InternalError, "Support for this type is not implemented");

    return result;
}

void CooperativeMatrixTestCase::initProgramsSPIRV(SourceCollections &programCollection) const
{
    std::string dims[4] = {
        m_data.colMajor ? "M" : "K",
        m_data.colMajor ? "K" : "N",
        m_data.colMajor ? "M" : "N",
        m_data.colMajor ? "M" : "N",
    };
    //  #version 450 core
    //  #pragma use_vulkan_memory_model
    //  #extension GL_KHR_shader_subgroup_basic : enable
    //  #extension GL_KHR_memory_scope_semantics : enable
    //  #extension GL_KHR_cooperative_matrix : enable
    //  #extension GL_EXT_shader_explicit_arithmetic_types : enable
    //  #extension GL_EXT_buffer_reference : enable
    //  // strides overriden by spec constants
    //  layout(constant_id = 6) const int M = 1;
    //  layout(constant_id = 7) const int N = 1;
    //  layout(constant_id = 8) const int K = 1;
    //  layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z = 1) in;
    //  const int workgroupsX = 4;
    //  const uvec2 subgroupsPerWG = uvec2(2, 2);
    //  layout(set=0, binding=0) coherent buffer InputA { int8_t x[]; } inputA;
    //  layout(set=0, binding=1) coherent buffer InputB { int8_t x[]; } inputB;
    //  layout(set=0, binding=2) coherent buffer InputC { int32_t x[]; } inputC;
    //  layout(set=0, binding=3) coherent buffer Output { int32_t x[]; } outputO;
    //  coopmat<int8_t, gl_ScopeSubgroup, M, K, gl_MatrixUseA> matA;
    //  coopmat<int8_t, gl_ScopeSubgroup, K, N, gl_MatrixUseB> matB;
    //  coopmat<int32_t, gl_ScopeSubgroup, M, N, gl_MatrixUseAccumulator> matC;
    //  coopmat<int32_t, gl_ScopeSubgroup, M, N, gl_MatrixUseAccumulator> matO;
    //  void main()
    //  {
    //     uvec2 subgroupXY = uvec2(gl_SubgroupID % subgroupsPerWG.x, gl_SubgroupID / subgroupsPerWG.x);
    //     uvec2 matrixID = uvec2(gl_WorkGroupID.xy) * subgroupsPerWG + subgroupXY;
    //     uint element0 = (K * 8 * M * matrixID.y + K * matrixID.x);
    //     uint element1 = (N * 8 * K * matrixID.y + N * matrixID.x);
    //     uint element2 = (N * 8 * M * matrixID.y + N * matrixID.x);
    //     uint element3 = (N * 8 * M * matrixID.y + N * matrixID.x);
    //     uint elementS0, elementS1, elementS2, elementS3;
    //     coopMatLoad(matA, inputA.x, element0, K * 8, gl_CooperativeMatrixLayoutRowMajor);
    //     coopMatLoad(matB, inputB.x, element1, N * 8, gl_CooperativeMatrixLayoutRowMajor);
    //     coopMatLoad(matC, inputC.x, element2, N * 8, gl_CooperativeMatrixLayoutRowMajor);
    //     matO = coopMatMulAdd(matA, matB, matC);
    //     coopMatStore(matO, outputO.x, element3, N * 8, gl_CooperativeMatrixLayoutRowMajor);
    //  }
    const char *shaderTemplateGlobalString =
        "OpCapability Shader\n"
        "OpCapability Int8\n"
        "OpCapability GroupNonUniform\n"
        "OpCapability StorageBuffer8BitAccess\n"
        "OpCapability VulkanMemoryModel\n"
        "OpCapability CooperativeMatrixKHR\n"
        "OpExtension \"SPV_KHR_8bit_storage\"\n"
        "OpExtension \"SPV_KHR_cooperative_matrix\"\n"
        "OpExtension \"SPV_KHR_vulkan_memory_model\"\n"
        "%1 = OpExtInstImport \"GLSL.std.450\"\n"
        "OpMemoryModel Logical Vulkan\n"
        "OpEntryPoint GLCompute %main \"main\" %gl_SubgroupId %gl_WorkgroupID\n"
        "OpExecutionMode %main LocalSize 1 1 1\n"
        "OpDecorate %gl_SubgroupId BuiltIn SubgroupId\n"
        "OpDecorate %gl_WorkgroupID BuiltIn WorkgroupId\n"

        "OpDecorate %local_size_x SpecId 0\n"
        "OpDecorate %local_size_y SpecId 1\n"
        "OpDecorate %M            SpecId 6\n"
        "OpDecorate %N            SpecId 7\n"
        "OpDecorate %K            SpecId 8\n"

        "OpDecorate %inputA_x ArrayStride ${STRIDE_A}\n"
        "OpMemberDecorate %inputA_struct 0 Offset 0\n"
        "OpDecorate %inputA_struct Block\n"
        "OpDecorate %inputA_var DescriptorSet 0\n"
        "OpDecorate %inputA_var Binding 0\n"

        "OpDecorate %inputB_x ArrayStride ${STRIDE_B}\n"
        "OpMemberDecorate %inputB_struct 0 Offset 0\n"
        "OpDecorate %inputB_struct Block\n"
        "OpDecorate %inputB_var DescriptorSet 0\n"
        "OpDecorate %inputB_var Binding 1\n"

        "OpDecorate %inputC_x ArrayStride ${STRIDE_C}\n"
        "OpMemberDecorate %inputC_struct 0 Offset 0\n"
        "OpDecorate %inputC_struct Block\n"
        "OpDecorate %inputC_var DescriptorSet 0\n"
        "OpDecorate %inputC_var Binding 2\n"

        "OpDecorate %outputO_x ArrayStride ${STRIDE_R}\n"
        "OpMemberDecorate %outputO_struct 0 Offset 0\n"
        "OpDecorate %outputO_struct Block\n"
        "OpDecorate %outputO_var DescriptorSet 0\n"
        "OpDecorate %outputO_var Binding 3\n"

        "OpDecorate %wg_size BuiltIn WorkgroupSize\n"
        "%void            = OpTypeVoid\n"
        "%voidfunc        = OpTypeFunction %void\n"
        "%u8              = OpTypeInt 8 0\n"
        "%s8              = OpTypeInt 8 1\n"
        "%u32             = OpTypeInt 32 0\n"
        "%s32             = OpTypeInt 32 1\n"
        "%uvec2           = OpTypeVector %u32 2\n"
        "%uvec3           = OpTypeVector %u32 3\n"
        "%piu32           = OpTypePointer Input %u32\n"
        "%gl_SubgroupId   = OpVariable %piu32 Input\n"
        "%c0u             = OpConstant %u32 0\n"
        "%c1u             = OpConstant %u32 1\n"
        "%c2u             = OpConstant %u32 2\n"
        "%c3u             = OpConstant %u32 3\n"
        "%c5u             = OpConstant %u32 5\n"
        "%c8s             = OpConstant %s32 8\n"
        "%c0s             = OpConstant %s32 0\n"
        "%layout          = OpConstant %s32 ${LAYOUT}\n"
        "%piuvec3         = OpTypePointer Input %uvec3\n"
        "%gl_WorkgroupID  = OpVariable %piuvec3 Input\n"
        "%csubgroupsPerWG = OpConstantComposite %uvec2 %c2u %c2u\n"
        "%K               = OpSpecConstant %s32 1\n"
        "%M               = OpSpecConstant %s32 1\n"
        "%N               = OpSpecConstant %s32 1\n"
        "%Ku              = OpSpecConstantOp %u32 IAdd %K %c0u\n"
        "%Mu              = OpSpecConstantOp %u32 IAdd %M %c0u\n"
        "%Nu              = OpSpecConstantOp %u32 IAdd %N %c0u\n"

        "%k8              = OpSpecConstantOp %s32 IMul %K %c8s\n"
        "%mk8             = OpSpecConstantOp %s32 IMul %k8 %M\n"
        "%mk8u            = OpSpecConstantOp %u32 IAdd %mk8 %c0u\n"

        "%n8              = OpSpecConstantOp %s32 IMul %N %c8s\n"
        "%nk8             = OpSpecConstantOp %s32 IMul %n8 %K\n"
        "%nk8u            = OpSpecConstantOp %u32 IAdd %nk8 %c0u\n"

        "%nm8             = OpSpecConstantOp %s32 IMul %n8 %M\n"
        "%nm8u            = OpSpecConstantOp %u32 IAdd %nm8 %c0u\n"

        "%strideAs        = OpSpecConstantOp %s32 IMul %${MULT_A} %c8s\n"
        "%strideA         = OpSpecConstantOp %u32 IAdd %strideAs %c0u\n"
        "%strideBs        = OpSpecConstantOp %s32 IMul %${MULT_B} %c8s\n"
        "%strideB         = OpSpecConstantOp %u32 IAdd %strideBs %c0u\n"
        "%strideCs        = OpSpecConstantOp %s32 IMul %${MULT_C} %c8s\n"
        "%strideC         = OpSpecConstantOp %u32 IAdd %strideCs %c0u\n"
        "%strideRs        = OpSpecConstantOp %s32 IMul %${MULT_R} %c8s\n"
        "%strideR         = OpSpecConstantOp %u32 IAdd %strideRs %c0u\n"

        "%psbmat_s8       = OpTypePointer StorageBuffer %s8\n"
        "%psbmat_s32      = OpTypePointer StorageBuffer %s32\n"
        "%psbmat_u8       = OpTypePointer StorageBuffer %u8\n"
        "%psbmat_u32      = OpTypePointer StorageBuffer %u32\n"

        "%matA            = OpTypeCooperativeMatrixKHR %${A_ELEM_TYPE} %c3u %M %K %c0u\n"
        "%inputA_x        = OpTypeRuntimeArray %${A_ELEM_TYPE}\n"
        "%inputA_struct   = OpTypeStruct %inputA_x\n"
        "%inputA_ptr      = OpTypePointer StorageBuffer %inputA_struct\n"
        "%inputA_var      = OpVariable %inputA_ptr StorageBuffer\n"

        "%matB            = OpTypeCooperativeMatrixKHR %${B_ELEM_TYPE} %c3u %K %N %c1u\n"
        "%inputB_x        = OpTypeRuntimeArray %${B_ELEM_TYPE}\n"
        "%inputB_struct   = OpTypeStruct %inputB_x\n"
        "%inputB_ptr      = OpTypePointer StorageBuffer %inputB_struct\n"
        "%inputB_var      = OpVariable %inputB_ptr StorageBuffer\n"

        "%matS            = OpTypeCooperativeMatrixKHR %${S_ELEM_TYPE} %c3u %M %N %c2u\n"
        "%matU            = OpTypeCooperativeMatrixKHR %${U_ELEM_TYPE} %c3u %M %N %c2u\n"

        "%inputC_x        = OpTypeRuntimeArray %${C_ELEM_TYPE}\n"
        "%inputC_struct   = OpTypeStruct %inputC_x\n"
        "%inputC_ptr      = OpTypePointer StorageBuffer %inputC_struct\n"
        "%inputC_var      = OpVariable %inputC_ptr StorageBuffer\n"

        "%outputO_x       = OpTypeRuntimeArray %${R_ELEM_TYPE}\n"
        "%outputO_struct  = OpTypeStruct %outputO_x\n"
        "%outputO_ptr     = OpTypePointer StorageBuffer %outputO_struct\n"
        "%outputO_var     = OpVariable %outputO_ptr StorageBuffer\n"

        "%local_size_x           = OpSpecConstant %u32 1\n"
        "%local_size_y           = OpSpecConstant %u32 1\n"
        "%wg_size                = OpSpecConstantComposite %uvec3 %local_size_x %local_size_y %c1u\n"
        "%main                   = OpFunction %void None %voidfunc\n"
        "%label                  = OpLabel\n"
        "%gl_SubgroupId_         = OpLoad %u32 %gl_SubgroupId\n"
        "%subgroupXY_x           = OpUMod %u32 %gl_SubgroupId_ %c2u\n"
        "%subgroupXY_y           = OpUDiv %u32 %gl_SubgroupId_ %c2u\n"
        "%subgroupXY_uvec2       = OpCompositeConstruct %uvec2 %subgroupXY_x %subgroupXY_y\n"
        "%gl_WorkgroupID_uvec3   = OpLoad %uvec3 %gl_WorkgroupID\n"
        "%gl_WorkgroupID_uvec2   = OpVectorShuffle %uvec2 %gl_WorkgroupID_uvec3 %gl_WorkgroupID_uvec3 0 1\n"
        "%2xgl_WorkgroupID_uvec2 = OpIMul %uvec2 %gl_WorkgroupID_uvec2 %csubgroupsPerWG\n"
        "%matrixID               = OpIAdd %uvec2 %2xgl_WorkgroupID_uvec2 %subgroupXY_uvec2\n"
        "%matrixID_x             = OpCompositeExtract %u32 %matrixID 0\n"
        "%matrixID_y             = OpCompositeExtract %u32 %matrixID 1\n"

        "%e0a      = OpIMul %u32 %mk8u %matrixID_y\n"
        "%e0b      = OpIMul %u32 %${MULT_A}u %matrixID_x\n"
        "%element0 = OpIAdd %u32 %e0a %e0b\n"

        "%e1a      = OpIMul %u32 %nk8u %matrixID_y\n"
        "%e1b      = OpIMul %u32 %${MULT_B}u %matrixID_x\n"
        "%element1 = OpIAdd %u32 %e1a %e1b\n"

        "%e2a      = OpIMul %u32 %nm8u %matrixID_y\n"
        "%e2b      = OpIMul %u32 %${MULT_C}u %matrixID_x\n"
        "%element2 = OpIAdd %u32 %e2a %e2b\n"

        "%e3a      = OpIMul %u32 %nm8u %matrixID_y\n"
        "%e3b      = OpIMul %u32 %${MULT_R}u %matrixID_x\n"
        "%element3 = OpIAdd %u32 %e3a %e3b\n"

        "%Aij      = OpAccessChain %psbmat_${A_ELEM_TYPE} %inputA_var %c0s %element0\n"
        "%Aij_mat  = OpCooperativeMatrixLoadKHR %matA %Aij %layout %strideA MakePointerVisible|NonPrivatePointer %c5u\n"

        "%Bij      = OpAccessChain %psbmat_${B_ELEM_TYPE} %inputB_var %c0s %element1\n"
        "%Bij_mat  = OpCooperativeMatrixLoadKHR %matB %Bij %layout %strideB MakePointerVisible|NonPrivatePointer %c5u\n"

        "%Cij      = OpAccessChain %psbmat_${C_ELEM_TYPE} %inputC_var %c0s %element2\n"
        "%Cij_mat  = OpCooperativeMatrixLoadKHR %${C_TYPE} %Cij %layout %strideC MakePointerVisible|NonPrivatePointer "
        "%c5u\n"

        "%matR     = OpCooperativeMatrixMulAddKHR %${R_TYPE} %Aij_mat %Bij_mat %Cij_mat ${SIGNEDNESS}\n"

        "%Rij_mat  = OpAccessChain %psbmat_${R_ELEM_TYPE} %outputO_var %c0s %element3\n"
        "OpCooperativeMatrixStoreKHR %Rij_mat %matR %layout %strideR MakePointerAvailable|NonPrivatePointer %c5u\n"

        "OpReturn\n"
        "OpFunctionEnd\n";
    const tcu::StringTemplate shaderTemplateGlobal(shaderTemplateGlobalString);
    const vk::SpirVAsmBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3);
    const string spirMatATypeName =
        m_data.useType == UT_KHR_A ? getOppositeSignednessTypeName(m_data.inputType) : getTypeName(m_data.inputType);
    const string spirMatBTypeName =
        m_data.useType == UT_KHR_B ? getOppositeSignednessTypeName(m_data.inputType) : getTypeName(m_data.inputType);
    const string spirMatCTypeName = m_data.useType == UT_KHR_C ? (isSIntType(m_data.outputType) ? "matU" : "matS") :
                                                                 (isSIntType(m_data.outputType) ? "matS" : "matU");
    const string spirMatRTypeName = m_data.useType == UT_KHR_Result ?
                                        (isSIntType(m_data.outputType) ? "matU" : "matS") :
                                        (isSIntType(m_data.outputType) ? "matS" : "matU");
    const string spirMatSTypeName = isSIntType(m_data.outputType) ? getTypeName(m_data.outputType) :
                                                                    getOppositeSignednessTypeName(m_data.outputType);
    const string spirMatUTypeName = isSIntType(m_data.outputType) ? getOppositeSignednessTypeName(m_data.outputType) :
                                                                    getTypeName(m_data.outputType);
    string signedness             = string(isSIntType(m_data.inputType) ? "|MatrixASignedComponentsKHR" : "") +
                        string(isSIntType(m_data.inputType) ? "|MatrixBSignedComponentsKHR" : "") +
                        string(isSIntType(m_data.outputType) ? "|MatrixCSignedComponentsKHR" : "") +
                        string(isSIntType(m_data.outputType) ? "|MatrixResultSignedComponentsKHR" : "");
    map<string, string> attributes;

    attributes["A_ELEM_TYPE"] = spirMatATypeName;
    attributes["B_ELEM_TYPE"] = spirMatBTypeName;
    attributes["C_ELEM_TYPE"] = getTypeName(m_data.outputType);
    attributes["R_ELEM_TYPE"] = getTypeName(m_data.outputType);
    attributes["S_ELEM_TYPE"] = spirMatSTypeName;
    attributes["U_ELEM_TYPE"] = spirMatUTypeName;
    attributes["C_TYPE"]      = spirMatCTypeName;
    attributes["R_TYPE"]      = spirMatRTypeName;
    attributes["SIGNEDNESS"]  = signedness.empty() ? "" : signedness.substr(1);
    attributes["MULT_A"]      = dims[0];
    attributes["MULT_B"]      = dims[1];
    attributes["MULT_C"]      = dims[2];
    attributes["MULT_R"]      = dims[3];
    attributes["STRIDE_A"]    = de::toString(getTypeWidth(m_data.inputType));
    attributes["STRIDE_B"]    = de::toString(getTypeWidth(m_data.inputType));
    attributes["STRIDE_C"]    = de::toString(getTypeWidth(m_data.outputType));
    attributes["STRIDE_R"]    = de::toString(getTypeWidth(m_data.outputType));
    attributes["LAYOUT"]      = m_data.colMajor ? "1" : "0";

    const std::string shaderCode = shaderTemplateGlobal.specialize(attributes);

    programCollection.spirvAsmSources.add("test") << shaderCode << buildOptions;
}

void CooperativeMatrixTestCase::initPrograms(SourceCollections &programCollection) const
{
    if (m_data.testType == TT_MATRIXMULADD_CROSS)
        initProgramsSPIRV(programCollection);
    else
        initProgramsGLSL(programCollection);
}

TestInstance *CooperativeMatrixTestCase::createInstance(Context &context) const
{
    return new CooperativeMatrixTestInstance(context, m_data);
}

void setDataFloat(void *base, const VkComponentTypeKHR dt, uint32_t i, float value)
{
    if (dt == VK_COMPONENT_TYPE_FLOAT32_KHR)
    {
        ((float *)base)[i] = value;
    }
#ifndef CTS_USES_VULKANSC
    else if (dt == VK_COMPONENT_TYPE_BFLOAT16_KHR)
    {
        ((tcu::float16_t *)base)[i] = BFloat16(value).bits();
    }
    else if (dt == VK_COMPONENT_TYPE_FLOAT_E5M2_NV)
    {
        ((tcu::FloatE5M2::StorageType *)base)[i] = tcu::FloatE5M2(value).bits();
    }
    else if (dt == VK_COMPONENT_TYPE_FLOAT_E4M3_NV)
    {
        ((tcu::FloatE4M3::StorageType *)base)[i] = tcu::FloatE4M3(value).bits();
    }
#endif
    else
    {
        DE_ASSERT(dt == VK_COMPONENT_TYPE_FLOAT16_KHR);
        ((tcu::float16_t *)base)[i] = tcu::Float16(value).bits();
    }
}

float getDataFloat(void *base, const VkComponentTypeKHR dt, uint32_t i)
{
    if (dt == VK_COMPONENT_TYPE_FLOAT32_KHR)
    {
        return ((float *)base)[i];
    }
#ifndef CTS_USES_VULKANSC
    else if (dt == VK_COMPONENT_TYPE_BFLOAT16_KHR)
    {
        return BFloat16(((const tcu::float16_t *)base)[i]).asFloat();
    }
    else if (dt == VK_COMPONENT_TYPE_FLOAT_E5M2_NV)
    {
        return tcu::FloatE5M2(((const tcu::FloatE5M2::StorageType *)base)[i]).asFloat();
    }
    else if (dt == VK_COMPONENT_TYPE_FLOAT_E4M3_NV)
    {
        return tcu::FloatE4M3(((const tcu::FloatE4M3::StorageType *)base)[i]).asFloat();
    }
#endif
    else
    {
        DE_ASSERT(dt == VK_COMPONENT_TYPE_FLOAT16_KHR);
        return tcu::Float16(((const tcu::float16_t *)base)[i]).asFloat();
    }
}

void setDataInt(void *base, VkComponentTypeKHR dt, uint32_t i, uint32_t value)
{
    DE_ASSERT(componentTypeInfo.at(dt).bits <= 32);

    switch (dt)
    {
    case VK_COMPONENT_TYPE_UINT8_KHR:
        ((uint8_t *)base)[i] = (uint8_t)value;
        break;
    case VK_COMPONENT_TYPE_UINT16_KHR:
        ((uint16_t *)base)[i] = (uint16_t)value;
        break;
    case VK_COMPONENT_TYPE_UINT32_KHR:
        ((uint32_t *)base)[i] = (uint32_t)value;
        break;
    case VK_COMPONENT_TYPE_SINT8_KHR:
        ((int8_t *)base)[i] = (int8_t)value;
        break;
    case VK_COMPONENT_TYPE_SINT16_KHR:
        ((int16_t *)base)[i] = (int16_t)value;
        break;
    case VK_COMPONENT_TYPE_SINT32_KHR:
        ((int32_t *)base)[i] = (int32_t)value;
        break;
    default:
        TCU_THROW(InternalError, "Unsupported type");
    }
}

uint32_t getDataInt(void *base, VkComponentTypeKHR dt, uint32_t i)
{
    DE_ASSERT(componentTypeInfo.at(dt).bits <= 32);

    switch (dt)
    {
    case VK_COMPONENT_TYPE_UINT8_KHR:
        return ((uint8_t *)base)[i];
    case VK_COMPONENT_TYPE_UINT16_KHR:
        return ((uint16_t *)base)[i];
    case VK_COMPONENT_TYPE_UINT32_KHR:
        return ((uint32_t *)base)[i];
    case VK_COMPONENT_TYPE_SINT8_KHR:
        return ((int8_t *)base)[i];
    case VK_COMPONENT_TYPE_SINT16_KHR:
        return ((int16_t *)base)[i];
    case VK_COMPONENT_TYPE_SINT32_KHR:
        return ((int32_t *)base)[i];
    default:
        TCU_THROW(InternalError, "Unsupported type");
    }
}

template <typename T>
T getDataConvertedToT(void *base, VkComponentTypeKHR dt, uint32_t i)
{
    DE_ASSERT(componentTypeInfo.at(dt).bits <= 32);

    switch (dt)
    {
    case VK_COMPONENT_TYPE_UINT8_KHR:
        return (T)((uint8_t *)base)[i];
    case VK_COMPONENT_TYPE_UINT16_KHR:
        return (T)((uint16_t *)base)[i];
    case VK_COMPONENT_TYPE_UINT32_KHR:
        return (T)((uint32_t *)base)[i];
    case VK_COMPONENT_TYPE_SINT8_KHR:
        return (T)((int8_t *)base)[i];
    case VK_COMPONENT_TYPE_SINT16_KHR:
        return (T)((int16_t *)base)[i];
    case VK_COMPONENT_TYPE_SINT32_KHR:
        return (T)((int32_t *)base)[i];
    case VK_COMPONENT_TYPE_FLOAT32_KHR:
    {
        float temp = ((float *)base)[i];
        if (std::numeric_limits<T>::min() == 0)
            temp = std::max(temp, 0.0f);
        return (T)temp;
    }
    case VK_COMPONENT_TYPE_FLOAT16_KHR:
    {
        float temp = tcu::Float16(((tcu::float16_t *)base)[i]).asFloat();
        if (std::numeric_limits<T>::min() == 0)
            temp = std::max(temp, 0.0f);
        return (T)temp;
    }
#ifndef CTS_USES_VULKANSC
    case VK_COMPONENT_TYPE_BFLOAT16_KHR:
    {
        float temp = BFloat16(((typename BFloat16::StorageType *)base)[i]).asFloat();
        if (std::numeric_limits<T>::min() == 0)
            temp = std::max(temp, 0.0f);
        return (T)temp;
    }
    case VK_COMPONENT_TYPE_FLOAT_E5M2_NV:
    {
        float temp = tcu::FloatE5M2(((typename tcu::FloatE5M2::StorageType *)base)[i]).asFloat();
        if (std::numeric_limits<T>::min() == 0)
            temp = std::max(temp, 0.0f);
        return (T)temp;
    }
    case VK_COMPONENT_TYPE_FLOAT_E4M3_NV:
    {
        float temp = tcu::FloatE4M3(((typename tcu::FloatE4M3::StorageType *)base)[i]).asFloat();
        if (std::numeric_limits<T>::min() == 0)
            temp = std::max(temp, 0.0f);
        return (T)temp;
    }
#endif
    default:
        TCU_THROW(InternalError, "Unsupported type");
    }
}

template <typename T>
T satAdd(T a, T b)
{
    if (a > 0)
    {
        if (b > std::numeric_limits<T>::max() - a)
            return std::numeric_limits<T>::max();
    }
    else if (b < std::numeric_limits<T>::min() - a)
    {
        return std::numeric_limits<T>::min();
    }

    return (T)(a + b);
}

uint32_t satAddData(VkComponentTypeKHR dt, uint32_t a, uint32_t b)
{
    DE_ASSERT(componentTypeInfo.at(dt).bits <= 32);

    switch (dt)
    {
    case VK_COMPONENT_TYPE_UINT8_KHR:
        return deMinu32(a + b, std::numeric_limits<uint8_t>::max());
    case VK_COMPONENT_TYPE_UINT16_KHR:
        return deMinu32(a + b, std::numeric_limits<uint16_t>::max());
    case VK_COMPONENT_TYPE_UINT32_KHR:
        return (a + b >= a) ? a + b : std::numeric_limits<uint32_t>::max();
    case VK_COMPONENT_TYPE_SINT8_KHR:
        return (uint32_t)satAdd((int8_t)a, (int8_t)b);
    case VK_COMPONENT_TYPE_SINT16_KHR:
        return (uint32_t)satAdd((int16_t)a, (int16_t)b);
    case VK_COMPONENT_TYPE_SINT32_KHR:
        return (uint32_t)satAdd((int32_t)a, (int32_t)b);
    default:
        TCU_THROW(InternalError, "Unsupported type");
    }
}

uint32_t getLimit(VkComponentTypeKHR dt, bool positive)
{
    DE_ASSERT(componentTypeInfo.at(dt).bits <= 32);

    switch (dt)
    {
    case VK_COMPONENT_TYPE_UINT8_KHR:
        return uint32_t(positive ? std::numeric_limits<uint8_t>::max() : std::numeric_limits<uint8_t>::min());
    case VK_COMPONENT_TYPE_UINT16_KHR:
        return uint32_t(positive ? std::numeric_limits<uint16_t>::max() : std::numeric_limits<uint16_t>::min());
    case VK_COMPONENT_TYPE_UINT32_KHR:
        return uint32_t(positive ? std::numeric_limits<uint32_t>::max() : std::numeric_limits<uint32_t>::min());
    case VK_COMPONENT_TYPE_SINT8_KHR:
        return uint32_t(positive ? std::numeric_limits<int8_t>::max() : std::numeric_limits<int8_t>::min());
    case VK_COMPONENT_TYPE_SINT16_KHR:
        return uint32_t(positive ? std::numeric_limits<int16_t>::max() : std::numeric_limits<int16_t>::min());
    case VK_COMPONENT_TYPE_SINT32_KHR:
        return uint32_t(positive ? std::numeric_limits<int32_t>::max() : std::numeric_limits<int32_t>::min());
    default:
        TCU_THROW(InternalError, "Unsupported type");
    }
}

void setSingleElementInt(void *data, VkComponentTypeKHR dt, uint32_t start, uint32_t count, uint32_t step, uint32_t at,
                         uint32_t val)
{
    for (uint32_t i = 0; i < count; i++)
        setDataInt(data, dt, start + i * step, (i == at) ? val : 0);
}

#ifdef COOPERATIVE_MATRIX_EXTENDED_DEBUG
string dumpWholeMatrix(void *data, VkComponentTypeKHR dt, bool colMajor, uint32_t matrixElemCount, uint32_t stride)
{
    const uint32_t rowsCount = colMajor ? stride : matrixElemCount / stride;
    const uint32_t colsCount = colMajor ? matrixElemCount / stride : stride;
    bool floatType           = isFloatType(dt);
    bool sIntType            = isSIntType(dt);
    std::stringstream ss;

    DE_ASSERT(rowsCount * colsCount == matrixElemCount);

    for (uint32_t r = 0; r < rowsCount; r++)
    {
        for (uint32_t c = 0; c < colsCount; c++)
        {
            const uint32_t i = colMajor ? rowsCount * c + r : colsCount * r + c;

            if (floatType)
                ss << getDataFloat(data, dt, i) << "\t";
            else if (sIntType)
                ss << (int32_t)getDataInt(data, dt, i) << "\t";
            else
                ss << getDataInt(data, dt, i) << "\t";
        }

        ss << std::endl;
    }

    return ss.str();
}
#endif

tcu::TestStatus CooperativeMatrixTestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();
    MemoryRequirement memoryDeviceAddress =
        m_data.storageClass == SC_PHYSICAL_STORAGE_BUFFER &&
                m_context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address") ?
            MemoryRequirement::DeviceAddress :
            MemoryRequirement::Any;
    qpTestResult finalres       = QP_TEST_RESULT_NOT_SUPPORTED;
    tcu::TestLog &log           = m_context.getTestContext().getLog();
    const bool saturated        = (m_data.testType == TT_MATRIXMULADD_SATURATED);
    const uint32_t subgroupSize = getSubgroupSizeFromMode(m_context, m_data.subgroupSizeMode);
    const float epsilon         = 1.0f / float(1ull << 17); // 131072 is epsilon circa 1e-5
    vk::VkPhysicalDeviceProperties vkproperties;
    const bool coopMat2Supported = m_context.isDeviceFunctionalitySupported("VK_NV_cooperative_matrix2");

    m_context.getInstanceInterface().getPhysicalDeviceProperties(m_context.getPhysicalDevice(), &vkproperties);

    deRandom rnd;
    deRandom_init(&rnd, 1234);

    std::vector<VkCooperativeMatrixPropertiesKHR> properties =
        getCooperativeMatrixPropertiesConverted(m_context, isKhr(m_data.useType));

    struct TestTuple
    {
        TestTuple()
        {
        }
        TestTuple(uint32_t m, uint32_t n, uint32_t k, uint32_t w) : M(m), N(n), K(k), workgroupSize(w)
        {
        }

        bool operator<(const TestTuple &other) const
        {
            return workgroupSize < other.workgroupSize || (workgroupSize == other.workgroupSize && M < other.M) ||
                   (workgroupSize == other.workgroupSize && M == other.M && N < other.N) ||
                   (workgroupSize == other.workgroupSize && M == other.M && N == other.N && K < other.K);
        }

        uint32_t M, N, K, workgroupSize;
    };

    std::vector<VkCooperativeMatrixFlexibleDimensionsPropertiesNV> flexibleProperties;
    if (m_context.getCooperativeMatrix2FeaturesNV().cooperativeMatrixFlexibleDimensions)
    {
        uint32_t flexiblePropertyCount = 0;

        const InstanceInterface &vki = m_context.getInstanceInterface();
        VK_CHECK(vki.getPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV(m_context.getPhysicalDevice(),
                                                                                      &flexiblePropertyCount, nullptr));

        if (flexiblePropertyCount > 0)
        {
            const VkCooperativeMatrixFlexibleDimensionsPropertiesNV sample = initVulkanStructureConst();

            flexibleProperties.resize(flexiblePropertyCount, sample);

            VK_CHECK(vki.getPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV(
                m_context.getPhysicalDevice(), &flexiblePropertyCount, flexibleProperties.data()));
        }
        else
        {
            flexibleProperties.clear();
        }
    }

    set<TestTuple> testSizes;

    if (isTensorLayoutTest(m_data.testType))
    {
        for (auto const &prop : flexibleProperties)
        {
            auto const *p = &prop;
            if (m_data.scope == p->scope)
            {
                // placeholder matrix size. The test defines the real sizes elsewhere
                testSizes.insert(TestTuple(32, 32, 32, p->workgroupInvocations));
            }
        }
    }
    else if (m_data.useType != UT_NV)
    {
        auto shmemOK = [&](uint32_t M, uint32_t N, uint32_t K) -> bool
        {
            uint32_t maxMatrixElements = max(M * N, max(M * K, K * N));

            if (isReduce2x2(m_data.testType))
            {
                // A matrix is 4x larger
                maxMatrixElements *= 4;
            }
            if (isReduceChangeDim(m_data.testType))
            {
                // A matrix is 3-9x larger
                maxMatrixElements *= reduceMScale(m_data.testType) * reduceNScale(m_data.testType);
            }

            if (m_data.scope == VK_SCOPE_SUBGROUP_KHR)
            {
                maxMatrixElements *= m_data.subgroupsPerWorkgroupX * m_data.subgroupsPerWorkgroupY;
            }

            int32_t maxSharedMem = m_context.getDeviceProperties().limits.maxComputeSharedMemorySize;

            if (coopMat2Supported && m_data.scope == VK_SCOPE_WORKGROUP_KHR)
            {
                // reserved for implementation
                maxSharedMem -=
                    m_context.getCooperativeMatrix2PropertiesNV().cooperativeMatrixWorkgroupScopeReservedSharedMemory;
            }

            if (m_data.storageClass == SC_WORKGROUP || m_data.storageClass == SC_WORKGROUP_VARIABLE_POINTERS)
            {
                return (int32_t)(maxMatrixElements * 2 *
                                 (componentTypeInfo.at(m_data.inputType).bits * m_data.inputComponentCount +
                                  componentTypeInfo.at(m_data.outputType).bits * m_data.outputComponentCount) /
                                 8) <= maxSharedMem;
            }

            return true;
        };
        if (m_context.getCooperativeMatrix2FeaturesNV().cooperativeMatrixFlexibleDimensions)
        {
            const auto isMMA    = isMatrixMulAddOp(m_data.testType);
            const auto isMMASat = m_data.testType == TT_MATRIXMULADD_SATURATED;

            std::vector<TestTuple> sizes;
            for (auto const &prop : flexibleProperties)
            {
                auto const *p = &prop;

                uint32_t MGranularity = 0;
                uint32_t NGranularity = 0;
                uint32_t KGranularity = 0;
                bool ok               = false;

                if (p->scope != m_data.scope)
                    continue;

                if (isMMA && isMMASat != static_cast<bool>(p->saturatingAccumulation))
                    continue;

                if (isMMA)
                {
                    if (p->AType == m_data.inputType && p->BType == m_data.inputType && p->CType == m_data.outputType &&
                        p->ResultType == m_data.outputType)
                    {
                        ok           = true;
                        MGranularity = p->MGranularity;
                        NGranularity = p->NGranularity;
                        KGranularity = p->KGranularity;
                    }
                }
                else
                {
                    const VkComponentTypeKHR types[2] = {m_data.inputType, m_data.outputType};
                    UseType uses[2]                   = {m_data.useType, m_data.useType};
                    if (m_data.testType == TT_CONVERT_ACC_TO_A)
                    {
                        uses[1] = UT_KHR_A;
                    }
                    else if (m_data.testType == TT_CONVERT_ACC_TO_B || m_data.testType == TT_TRANSPOSE_ACC_TO_B)
                    {
                        uses[1] = UT_KHR_B;
                    }

                    auto const &SetGranularity = [&](const VkCooperativeMatrixFlexibleDimensionsPropertiesNV *p2,
                                                     VkComponentTypeKHR type, UseType use)
                    {
                        ok = false;
                        switch (use)
                        {
                        case UT_NV:
                            break;
                        case UT_KHR_A:
                        {
                            if (p2->AType == type)
                            {
                                ok           = true;
                                MGranularity = std::max(MGranularity, p2->MGranularity);
                                NGranularity = std::max(NGranularity, p2->KGranularity);
                            }

                            break;
                        }
                        case UT_KHR_B:
                        {
                            if (p2->BType == type)
                            {
                                ok = true;
                                if (m_data.testType == TT_TRANSPOSE_ACC_TO_B)
                                {
                                    MGranularity = std::max(MGranularity, p2->NGranularity);
                                    NGranularity = std::max(NGranularity, p2->KGranularity);
                                }
                                else
                                {
                                    MGranularity = std::max(MGranularity, p2->KGranularity);
                                    NGranularity = std::max(NGranularity, p2->NGranularity);
                                }
                            }

                            break;
                        }
                        case UT_KHR_Result:
                        {
                            if (p2->ResultType == type)
                            {
                                ok           = true;
                                MGranularity = std::max(MGranularity, p2->MGranularity);
                                NGranularity = std::max(NGranularity, p2->NGranularity);
                            }

                            break;
                        }
                        default:
                            TCU_THROW(InternalError, "Unsupported use type");
                        }
                    };

                    SetGranularity(p, types[0], uses[0]);

                    if (!ok)
                    {
                        continue;
                    }

                    // Need to find a "matching" property for the other use/type
                    // and take the max of the granularities
                    for (auto const &prop2 : flexibleProperties)
                    {
                        auto const *p2 = &prop2;

                        if (p2->scope != m_data.scope || p2->workgroupInvocations != p->workgroupInvocations)
                            continue;

                        SetGranularity(p2, types[1], uses[1]);

                        if (ok)
                        {
                            break;
                        }
                    }
                }
                if (ok)
                {
                    DE_ASSERT(MGranularity && NGranularity && (!isMMA || KGranularity));

                    sizes.emplace_back(1U * MGranularity, 1U * NGranularity, 1U * KGranularity,
                                       p->workgroupInvocations);
                    if (m_data.storageClass != SC_WORKGROUP && m_data.storageClass != SC_WORKGROUP_VARIABLE_POINTERS)
                    {
                        sizes.emplace_back(3U * MGranularity, 1U * NGranularity, 1U * KGranularity,
                                           p->workgroupInvocations);
                        sizes.emplace_back(1U * MGranularity, 3U * NGranularity, 1U * KGranularity,
                                           p->workgroupInvocations);
                        if (isMatrixMulAddOp(m_data.testType))
                        {
                            sizes.emplace_back(2U * MGranularity, 2U * NGranularity, 3U * KGranularity,
                                               p->workgroupInvocations);
                            sizes.emplace_back(1U * MGranularity, 1U * NGranularity, 3U * KGranularity,
                                               p->workgroupInvocations);
                        }
                    }
                }
            }

            for (auto &s : sizes)
            {
                if (shmemOK(s.M, s.N, s.K))
                {
                    testSizes.insert(s);
                }
            }
        }
    }
    if (!isTensorLayoutTest(m_data.testType))
    {
        if (isMatrixMulAddOp(m_data.testType))
        {
            for (size_t i = 0; i < properties.size(); ++i)
            {
                VkCooperativeMatrixPropertiesKHR *p = &properties[i];

                if (p->AType == m_data.inputType && p->BType == m_data.inputType && p->CType == m_data.outputType &&
                    p->ResultType == m_data.outputType && p->scope == m_data.scope)
                {
                    testSizes.insert(TestTuple(p->MSize, p->NSize, p->KSize, 0));
                }
            }
        }
        else
        {
            set<TestTuple> typeSizes[2];
            VkComponentTypeKHR types[2] = {m_data.inputType, m_data.outputType};
            UseType uses[2]             = {m_data.useType, m_data.useType};
            if (m_data.testType == TT_CONVERT_ACC_TO_A)
            {
                uses[1] = UT_KHR_A;
            }
            else if (m_data.testType == TT_CONVERT_ACC_TO_B || m_data.testType == TT_TRANSPOSE_ACC_TO_B)
            {
                uses[1] = UT_KHR_B;
            }

            for (uint32_t i = 0; i < properties.size(); ++i)
            {
                VkCooperativeMatrixPropertiesKHR *p = &properties[i];

                if (p->scope != m_data.scope)
                    continue;

                for (uint32_t j = 0; j < 2; ++j)
                {
                    // For these tests, m_data.M/N are always the matrix size. Check if they match
                    // any input or output in the list.
                    if ((uses[j] == UT_KHR_A || uses[j] == UT_NV) && p->AType == types[j])
                        typeSizes[j].insert(TestTuple(p->MSize, p->KSize, 0, 0));
                    if ((uses[j] == UT_KHR_B || uses[j] == UT_NV) && p->BType == types[j])
                    {
                        if (m_data.testType == TT_TRANSPOSE_ACC_TO_B)
                        {
                            typeSizes[j].insert(TestTuple(p->NSize, p->KSize, 0, 0));
                        }
                        else
                        {
                            typeSizes[j].insert(TestTuple(p->KSize, p->NSize, 0, 0));
                        }
                    }
                    if ((uses[j] == UT_KHR_Result || uses[j] == UT_NV) &&
                        (p->CType == types[j] || p->ResultType == types[j]))
                        typeSizes[j].insert(TestTuple(p->MSize, p->NSize, 0, 0));
                }
            }
            // Test those sizes that are supported for both the input and output type.
            std::set_intersection(typeSizes[0].begin(), typeSizes[0].end(), typeSizes[1].begin(), typeSizes[1].end(),
                                  std::inserter(testSizes, testSizes.begin()));
        }
    }

    properties.resize(0);

    for (auto &testSize : testSizes)
    {
        // When testing a multiply, MxNxK is the type of matrix multiply.
        // Otherwise, MxN is the size of the input/output matrices
        uint32_t M, N, K;
        M = testSize.M;
        N = testSize.N;
        K = testSize.K;

        log << tcu::TestLog::Message << "Testing M = " << M << ", N = " << N << ", K = " << K
            << ", WG = " << testSize.workgroupSize << tcu::TestLog::EndMessage;

        struct
        {
            uint32_t rows, cols;
        } dims[4];

        if (isMatrixMulAddOp(m_data.testType))
        {
            dims[0].rows = M;
            dims[0].cols = K;
            dims[1].rows = K;
            dims[1].cols = N;
            dims[2].rows = M;
            dims[2].cols = N;
            dims[3].rows = M;
            dims[3].cols = N;
        }
        else
        {
            if (isReduce2x2(m_data.testType))
            {
                dims[0].rows = M * 2;
                dims[0].cols = N * 2;
            }
            else
            {
                dims[0].rows = M;
                dims[0].cols = N;
            }
            dims[1].rows = M;
            dims[1].cols = N;
            dims[2].rows = M;
            dims[2].cols = N;
            if (isReduceChangeDim(m_data.testType))
            {
                dims[3].rows = M * reduceMScale(m_data.testType);
                dims[3].cols = N * reduceNScale(m_data.testType);
            }
            else if (m_data.testType == TT_TRANSPOSE_ACC_TO_B)
            {
                dims[2].rows = N;
                dims[2].cols = M;
                dims[3].rows = N;
                dims[3].cols = M;
            }
            else
            {
                dims[3].rows = M;
                dims[3].cols = N;
            }
        }

        VkComponentTypeKHR dataTypes[4];
        size_t elementSize[4];
        VkDeviceSize bufferSizes[5];
        de::MovePtr<BufferWithMemory> buffers[5];
        vk::VkDescriptorBufferInfo bufferDescriptors[5];
        uint32_t strides[4]; // in elements
        uint32_t loadStrides[4];
        uint32_t totalElements[4];
        size_t sharedMemoryUsage[4];
        size_t totalSharedMemoryUsage = 0;

        for (uint32_t i = 0; i < 5; ++i)
        {
            if (i < 4)
            {
                // A/B use input type, C/D use output type
                dataTypes[i]   = (i < 2) ? m_data.inputType : m_data.outputType;
                elementSize[i] = componentTypeInfo.at(dataTypes[i]).bits / 8;

                strides[i] = (m_data.colMajor ? dims[i].rows : dims[i].cols) * m_data.workgroupsX;
                if (m_data.scope != VK_SCOPE_WORKGROUP_KHR)
                {
                    strides[i] *= m_data.subgroupsPerWorkgroupX;
                }
                loadStrides[i]   = strides[i];
                totalElements[i] = strides[i] * (m_data.colMajor ? dims[i].cols : dims[i].rows) * m_data.workgroupsY;
                sharedMemoryUsage[i] = dims[i].cols * dims[i].rows * m_data.subgroupsPerWorkgroupX *
                                       m_data.subgroupsPerWorkgroupY * elementSize[i] *
                                       ((i < 2) ? m_data.inputComponentCount : m_data.outputComponentCount);

                // Check there is enough shared memory supported
                if ((m_data.useType != UT_NV) &&
                    ((m_data.storageClass == SC_WORKGROUP) || (m_data.storageClass == SC_WORKGROUP_VARIABLE_POINTERS)))
                {
                    totalSharedMemoryUsage += sharedMemoryUsage[i];
                    if (totalSharedMemoryUsage > vkproperties.limits.maxComputeSharedMemorySize)
                        throw tcu::NotSupportedError("Not enough shared memory supported.");
                }

                if (m_data.testType == TT_MATRIXMULADD_DEQUANT && i < 2)
                {
                    // logical type is the inputType, but encoded as 4bpp so takes 1/4 the storage
                    DE_ASSERT(m_data.inputType == VK_COMPONENT_TYPE_FLOAT16_KHR ||
                              m_data.inputType == VK_COMPONENT_TYPE_BFLOAT16_KHR ||
                              m_data.inputType == VK_COMPONENT_TYPE_FLOAT_E5M2_NV ||
                              m_data.inputType == VK_COMPONENT_TYPE_FLOAT_E4M3_NV);
                    totalElements[i] /= (componentTypeInfo.at(dataTypes[i]).bits / 8) * 2;
                }

                if (m_data.scope != VK_SCOPE_WORKGROUP_KHR)
                {
                    totalElements[i] *= m_data.subgroupsPerWorkgroupY;
                }

                if (isTensorLayoutTest(m_data.testType))
                {
                    // sized for 128x128 matrix, scaled up by 4 workgroups in x and y
                    totalElements[i] = 512 * 512;
                }

                bufferSizes[i] = totalElements[i] * elementSize[i];
            }
            else
            {
                bufferSizes[4] = sizeof(VkDeviceAddress) * 4;
            }

            try
            {
                buffers[i] = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
                    vk, device, allocator,
                    makeBufferCreateInfo(bufferSizes[i], VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                             (memoryDeviceAddress == MemoryRequirement::DeviceAddress ?
                                                                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT :
                                                                  0)),
                    MemoryRequirement::HostVisible | MemoryRequirement::Cached | MemoryRequirement::Coherent |
                        memoryDeviceAddress));
            }
            catch (const tcu::NotSupportedError &)
            {
                buffers[i] = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
                    vk, device, allocator,
                    makeBufferCreateInfo(bufferSizes[i], VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                             (memoryDeviceAddress == MemoryRequirement::DeviceAddress ?
                                                                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT :
                                                                  0)),
                    MemoryRequirement::HostVisible | memoryDeviceAddress));
            }

            bufferDescriptors[i] = makeDescriptorBufferInfo(**buffers[i], 0, bufferSizes[i]);
        }

        // Load with a stride of 0
        if (m_data.testType == TT_MATRIXMULADD_STRIDE0)
            loadStrides[0] = loadStrides[1] = loadStrides[2] = loadStrides[3] = 0;

        void *ptrs[5];
        for (uint32_t i = 0; i < 5; ++i)
        {
            ptrs[i] = buffers[i]->getAllocation().getHostPtr();
        }

        vk::DescriptorSetLayoutBuilder layoutBuilder;

        layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);
        layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);
        layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);
        layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);
        layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);

        vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(layoutBuilder.build(vk, device));

        vk::Unique<vk::VkDescriptorPool> descriptorPool(
            vk::DescriptorPoolBuilder()
                .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5u)
                .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
        vk::Unique<vk::VkDescriptorSet> descriptorSet(
            makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

        vk::DescriptorSetUpdateBuilder setUpdateBuilder;
        if (m_data.storageClass == SC_PHYSICAL_STORAGE_BUFFER)
        {
            VkBufferDeviceAddressInfo info{
                VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, // VkStructureType  sType;
                nullptr,                                      // const void*  pNext;
                VK_NULL_HANDLE,                               // VkBuffer            buffer
            };
            VkDeviceAddress *addrsInMemory = (VkDeviceAddress *)ptrs[4];
            for (uint32_t i = 0; i < 4; ++i)
            {
                info.buffer          = **buffers[i];
                VkDeviceAddress addr = vk.getBufferDeviceAddress(device, &info);
                addrsInMemory[i]     = addr;
            }
            setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(4),
                                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[4]);
        }
        else
        {
            setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0),
                                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[0]);
            setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1),
                                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[1]);
            setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(2),
                                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[2]);
            setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(3),
                                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[3]);
        }

        setUpdateBuilder.update(vk, device);

        const VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

        const uint32_t specData[9] = {
            (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ? testSize.workgroupSize :
                                                       (subgroupSize * m_data.subgroupsPerWorkgroupX),
            (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ? 1 : m_data.subgroupsPerWorkgroupY,
            strides[0],
            strides[1],
            strides[2],
            strides[3],
            M,
            N,
            K,
        };

        const vk::VkSpecializationMapEntry entries[9] = {
            {0, (uint32_t)(sizeof(uint32_t) * 0), sizeof(uint32_t)},
            {1, (uint32_t)(sizeof(uint32_t) * 1), sizeof(uint32_t)},
            {2, (uint32_t)(sizeof(uint32_t) * 2), sizeof(uint32_t)},
            {3, (uint32_t)(sizeof(uint32_t) * 3), sizeof(uint32_t)},
            {4, (uint32_t)(sizeof(uint32_t) * 4), sizeof(uint32_t)},
            {5, (uint32_t)(sizeof(uint32_t) * 5), sizeof(uint32_t)},
            {6, (uint32_t)(sizeof(uint32_t) * 6), sizeof(uint32_t)},
            {7, (uint32_t)(sizeof(uint32_t) * 7), sizeof(uint32_t)},
            {8, (uint32_t)(sizeof(uint32_t) * 8), sizeof(uint32_t)},
        };

        const vk::VkSpecializationInfo specInfo = {
            DE_LENGTH_OF_ARRAY(entries), // mapEntryCount
            entries,                     // pMapEntries
            sizeof(specData),            // dataSize
            specData                     // pData
        };

        std::vector<float> specialFloats;
        for (int sign : {-1, 1})
        {
            specialFloats.push_back(tcu::Float32::inf(sign).asFloat());
            specialFloats.push_back(tcu::Float32::largestNormal(sign).asFloat());
            specialFloats.push_back(tcu::Float16::largestNormal(sign).asFloat());
            specialFloats.push_back(tcu::BrainFloat16::largestNormal(sign).asFloat());
            specialFloats.push_back(tcu::FloatE5M2::largestNormal(sign).asFloat());
            specialFloats.push_back(tcu::FloatE4M3::largestNormal(sign).asFloat());
            specialFloats.push_back(tcu::FloatE5M2::largestNormal(sign).asFloat() * 2);
            specialFloats.push_back(tcu::FloatE4M3::largestNormal(sign).asFloat() * 2);
            specialFloats.push_back(tcu::FloatE5M2::largestNormal(sign).asFloat() * 0.75f);
            specialFloats.push_back(tcu::FloatE4M3::largestNormal(sign).asFloat() * 0.75f);
            specialFloats.push_back(tcu::Float16::largestNormal(sign).asFloat() * 2);
            specialFloats.push_back(tcu::Float16::largestNormal(sign).asFloat() * 0.75f);
        }

        for (uint32_t i = 0; i < 4; ++i)
            for (uint32_t j = 0; j < totalElements[i]; ++j)
            {
                if (isFloatType(dataTypes[i]))
                {
                    if ((isTensorLayoutTest(m_data.testType) || isClampTest(m_data.testType)) && i == 3)
                    {
                        setDataFloat(ptrs[i], dataTypes[i], j, 14.0);
                    }
                    else if ((m_data.testType == TT_CONVERT || m_data.testType == TT_CONVERT_SAT) &&
                             isFloatType(dataTypes[3]))
                    {
                        if (j < specialFloats.size())
                        {
                            setDataFloat(ptrs[i], dataTypes[i], j, specialFloats[j]);
                        }
                        else
                        {
                            setDataFloat(ptrs[i], dataTypes[i], j,
                                         ((float)(deRandom_getUint32(&rnd) & 0xff) - 64.0f) / 2.0f);
                        }
                    }
                    else if (!isMatrixMulAddOp(m_data.testType) && !isReduceSum(m_data.testType))
                        setDataFloat(ptrs[i], dataTypes[i], j,
                                     ((float)(deRandom_getUint32(&rnd) & 0xff) - 64.0f) / 2.0f);
                    else if (m_data.testType == TT_MATRIXMULADD_DEQUANT && i < 2)
                    {
                        // Each "element" still accounts for 16bpp, but it's stored quantized
                        // so we just want a random 16b pattern.
                        if (componentTypeInfo.at(dataTypes[i]).bits == 16)
                        {
                            uint32_t value = (deRandom_getUint32(&rnd) & 0xffff);
                            setDataInt(ptrs[i], VK_COMPONENT_TYPE_UINT16_KHR, j, value);
                        }
                        else
                        {
                            DE_ASSERT(componentTypeInfo.at(dataTypes[i]).bits == 8);
                            uint32_t value = (deRandom_getUint32(&rnd) & 0xff);
                            setDataInt(ptrs[i], VK_COMPONENT_TYPE_UINT8_KHR, j, value);
                        }
                    }
                    else if (m_data.outputType == VK_COMPONENT_TYPE_BFLOAT16_KHR)
                    {
                        setDataFloat(ptrs[i], dataTypes[i], j, ((float)(deRandom_getUint32(&rnd) & 0x7) - 3.0f) / 2.0f);
                    }
                    else if (m_data.outputType == VK_COMPONENT_TYPE_BFLOAT16_KHR)
                    {
                        setDataFloat(ptrs[i], dataTypes[i], j, ((float)(deRandom_getUint32(&rnd) & 0x7) - 3.0f) / 2.0f);
                    }
                    else
                    {
                        setDataFloat(ptrs[i], dataTypes[i], j, ((float)(deRandom_getUint32(&rnd) & 0xf) - 4.0f) / 2.0f);
                    }
                }
                else
                {
                    if (m_data.testType == TT_MATRIXMULADD_WRAPPING)
                    {
                        // Choose matrix values that should cause overflow and underflow, to
                        // verify wrapping behavior. Use the full range of values for A and B.
                        // For matrix C, use values clustered near where the type wraps (zero
                        // for unsigned, 2^(N-1) for signed).
                        uint32_t bits = componentTypeInfo.at(dataTypes[i]).bits;
                        uint32_t value;
                        if (i == 2)
                        {
                            value = (deRandom_getUint32(&rnd) & 0xff) - 128;
                            if (componentTypeInfo.at(dataTypes[i]).isSigned)
                                value += (1U << (bits - 1));
                        }
                        else
                        {
                            uint32_t mask = (bits == 32) ? 0xFFFFFFFFU : ((1U << bits) - 1U);
                            value         = deRandom_getUint32(&rnd) & mask;
                        }
                        setDataInt(ptrs[i], dataTypes[i], j, value);
                    }
                    else if (m_data.testType == TT_MATRIXMULADD_SATURATED)
                    {
                        setDataInt(ptrs[i], dataTypes[i], j, 0);
                    }
                    else if ((isTensorLayoutTest(m_data.testType) || isClampTest(m_data.testType)) && i == 3)
                    {
                        setDataInt(ptrs[i], dataTypes[i], j, 123);
                    }
                    else if (m_data.testType == TT_DIV)
                    {
                        uint32_t value = (deRandom_getUint32(&rnd) & 0xff) - 128;
                        if (isSIntType(dataTypes[3]) && i == 1)
                        {
                            if (value == 0)
                            {
                                // Divide by 0 is undefined behaviour.
                                value = 1; // Arbitrarily set to 1.
                            }
                            else
                            {
                                // It is also an undefined behaviour if value is
                                // -1 and the numerator (corresponding matA
                                // value) is the minimum representable value.
                                uint32_t bits = componentTypeInfo.at(dataTypes[i]).bits;
                                uint32_t mask = bits == 32 ? ~0 : ((1 << bits) - 1);
                                // A and B matrices have same datatype and size.
                                uint32_t matAVal = getDataInt(ptrs[0], dataTypes[0], j);
                                if (((value & mask) == (uint32_t)((1 << bits) - 1)) &&
                                    ((matAVal & mask) == (uint32_t)(1 << (bits - 1))))
                                {
                                    value = 1; // Arbitrarily set to 1.
                                }
                            }
                        }
                        setDataInt(ptrs[i], dataTypes[i], j, value);
                    }
                    else
                    {
                        uint32_t value = (deRandom_getUint32(&rnd) & 0xff) - 128;
                        setDataInt(ptrs[i], dataTypes[i], j, value);
                    }
                }
            }

        if (m_data.testType == TT_MATRIXMULADD_SATURATED)
        {
            // Set 1st row of A to 1,0,0...
            setSingleElementInt(ptrs[0], dataTypes[0], 0, dims[0].cols, (m_data.colMajor ? strides[0] : 1), 0, 1);

            // Set 1st column of B to 1,0,0...
            setSingleElementInt(ptrs[1], dataTypes[1], 0, dims[1].rows, (m_data.colMajor ? 1 : strides[1]), 0, 1);

            // Set C element at {0,0} to maximum type value, thus we will have overflow at plus operation in D=A*B+C for this element
            setDataInt(ptrs[2], dataTypes[2], 0, getLimit(dataTypes[2], true));

            // Check underflow if all involved elements support negative values
            if (isSIntType(dataTypes[1]) && isSIntType(dataTypes[2]) && isSIntType(dataTypes[3]))
            {
                // Set 2nd row of A to 0,1,0,0...
                setSingleElementInt(ptrs[0], dataTypes[0], (m_data.colMajor ? 1 : strides[0]), dims[0].cols,
                                    (m_data.colMajor ? strides[0] : 1), 1, 1);

                // Set 2nd column of B to 0,-1,0,0...
                setSingleElementInt(ptrs[1], dataTypes[1], (m_data.colMajor ? strides[1] : 1), dims[1].rows,
                                    (m_data.colMajor ? 1 : strides[1]), 1, -1);

                // Set C element at {1,1} to minimum type value, thus we will have underflow at plus operation in D=A*B+C for this element
                setDataInt(ptrs[2], dataTypes[2], strides[2] + 1, getLimit(dataTypes[2], false));
            }
        }

        flushAlloc(vk, device, buffers[0]->getAllocation());
        flushAlloc(vk, device, buffers[1]->getAllocation());
        flushAlloc(vk, device, buffers[2]->getAllocation());
        flushAlloc(vk, device, buffers[3]->getAllocation());

        ComputePipelineWrapper pipeline(vk, device, m_data.computePipelineConstructionType,
                                        m_context.getBinaryCollection().get("test"));
        pipeline.setDescriptorSetLayout(descriptorSetLayout.get());
        if (m_data.testType == TT_MATRIXMULADD_PUSH_CONSTANTS)
            pipeline.addPushConstantRange(makePushConstantRange(
                vk::VK_SHADER_STAGE_COMPUTE_BIT, 0u, (uint32_t)(DE_LENGTH_OF_ARRAY(strides) * sizeof(strides[0]))));
        pipeline.setSpecializationInfo(specInfo);
        pipeline.setSubgroupSize(m_data.subgroupSizeMode == SUBGROUP_SIZE_NONE ?
                                     0 :
                                     getSubgroupSizeFromMode(m_context, m_data.subgroupSizeMode));
        pipeline.buildPipeline();

        const VkQueue queue             = m_context.getUniversalQueue();
        Move<VkCommandPool> cmdPool     = createCommandPool(vk, device, 0, m_context.getUniversalQueueFamilyIndex());
        Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        beginCommandBuffer(vk, *cmdBuffer, 0u);

        vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, pipeline.getPipelineLayout(), 0u, 1, &*descriptorSet, 0u,
                                 nullptr);
        if (m_data.testType == TT_MATRIXMULADD_PUSH_CONSTANTS)
            vk.cmdPushConstants(*cmdBuffer, pipeline.getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                (uint32_t)(DE_LENGTH_OF_ARRAY(strides) * sizeof(strides[0])), strides);

        pipeline.bind(*cmdBuffer);

        // tensorlayout test has larger number of workgroups to allocate more memory
        // but only needs to launch one workgroup
        uint32_t workgroupsX = m_data.workgroupsX;
        uint32_t workgroupsY = m_data.workgroupsY;
        if (isTensorLayoutTest(m_data.testType))
        {
            workgroupsX = 1u;
            workgroupsY = 1u;
        }

        vk.cmdDispatch(*cmdBuffer, workgroupsX, workgroupsY, 1);

        const VkMemoryBarrier barrier = {
            VK_STRUCTURE_TYPE_MEMORY_BARRIER, // sType
            nullptr,                          // pNext
            VK_ACCESS_SHADER_WRITE_BIT,       // srcAccessMask
            VK_ACCESS_HOST_READ_BIT,          // dstAccessMask
        };
        vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                              (VkDependencyFlags)0, 1, &barrier, 0, nullptr, 0, nullptr);

        endCommandBuffer(vk, *cmdBuffer);

        submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

        invalidateAlloc(vk, device, buffers[3]->getAllocation());

        qpTestResult res = QP_TEST_RESULT_PASS;

        if (m_data.testType == TT_CONVERT || m_data.testType == TT_CONVERT_SAT)
        {
            for (uint32_t i = 0; i < totalElements[3]; ++i)
            {
                // Store results as double, which has enough range to hold all the other types exactly.
                double inputA, output;

                // This loads the data according to dataTypes[0], and then converts to the template parameter type
                switch (dataTypes[3])
                {
                case VK_COMPONENT_TYPE_UINT8_KHR:
                    inputA = getDataConvertedToT<uint8_t>(ptrs[0], dataTypes[0], i);
                    break;
                case VK_COMPONENT_TYPE_UINT16_KHR:
                    inputA = getDataConvertedToT<uint16_t>(ptrs[0], dataTypes[0], i);
                    break;
                case VK_COMPONENT_TYPE_UINT32_KHR:
                    inputA = getDataConvertedToT<uint32_t>(ptrs[0], dataTypes[0], i);
                    break;
                case VK_COMPONENT_TYPE_SINT8_KHR:
                    inputA = getDataConvertedToT<int8_t>(ptrs[0], dataTypes[0], i);
                    break;
                case VK_COMPONENT_TYPE_SINT16_KHR:
                    inputA = getDataConvertedToT<int16_t>(ptrs[0], dataTypes[0], i);
                    break;
                case VK_COMPONENT_TYPE_SINT32_KHR:
                    inputA = getDataConvertedToT<int32_t>(ptrs[0], dataTypes[0], i);
                    break;
                case VK_COMPONENT_TYPE_FLOAT32_KHR:
                case VK_COMPONENT_TYPE_FLOAT_E5M2_NV:
                case VK_COMPONENT_TYPE_FLOAT_E4M3_NV:
                case VK_COMPONENT_TYPE_FLOAT16_KHR:
                    inputA = getDataConvertedToT<float>(ptrs[0], dataTypes[0], i);
                    break;
                default:
                    TCU_THROW(InternalError, "Unexpected type");
                }

                double inputAConverted = inputA;

                switch (dataTypes[3])
                {
                case VK_COMPONENT_TYPE_UINT8_KHR:
                    output = getDataConvertedToT<uint8_t>(ptrs[3], dataTypes[3], i);
                    break;
                case VK_COMPONENT_TYPE_UINT16_KHR:
                    output = getDataConvertedToT<uint16_t>(ptrs[3], dataTypes[3], i);
                    break;
                case VK_COMPONENT_TYPE_UINT32_KHR:
                    output = getDataConvertedToT<uint32_t>(ptrs[3], dataTypes[3], i);
                    break;
                case VK_COMPONENT_TYPE_SINT8_KHR:
                    output = getDataConvertedToT<int8_t>(ptrs[3], dataTypes[3], i);
                    break;
                case VK_COMPONENT_TYPE_SINT16_KHR:
                    output = getDataConvertedToT<int16_t>(ptrs[3], dataTypes[3], i);
                    break;
                case VK_COMPONENT_TYPE_SINT32_KHR:
                    output = getDataConvertedToT<int32_t>(ptrs[3], dataTypes[3], i);
                    break;
                case VK_COMPONENT_TYPE_FLOAT32_KHR:
                    output = getDataConvertedToT<float>(ptrs[3], dataTypes[3], i);
                    break;
                case VK_COMPONENT_TYPE_FLOAT16_KHR:
                {
                    output          = getDataConvertedToT<float>(ptrs[3], dataTypes[3], i);
                    inputAConverted = tcu::Float16(inputAConverted).asDouble();
                    break;
                }
                case VK_COMPONENT_TYPE_FLOAT_E5M2_NV:
                {
                    output          = getDataConvertedToT<float>(ptrs[3], dataTypes[3], i);
                    inputAConverted = tcu::FloatE5M2(inputAConverted).asDouble();
                    break;
                }
                case VK_COMPONENT_TYPE_FLOAT_E4M3_NV:
                {
                    output          = getDataConvertedToT<float>(ptrs[3], dataTypes[3], i);
                    inputAConverted = tcu::FloatE4M3(inputAConverted).asDouble();
                    break;
                }
                default:
                    TCU_THROW(InternalError, "Unexpected type");
                }

                if (m_data.testType == TT_CONVERT_SAT)
                {
                    switch (dataTypes[3])
                    {
                    case VK_COMPONENT_TYPE_FLOAT_E5M2_NV:
                    {
                        if (fabs(inputA) > tcu::FloatE5M2::largestNormal(1).asFloat())
                        {
                            inputAConverted = tcu::FloatE5M2::largestNormal(inputA > 0 ? 1 : -1).asFloat();
                        }
                        break;
                    }
                    case VK_COMPONENT_TYPE_FLOAT_E4M3_NV:
                    {
                        if (fabs(inputA) > tcu::FloatE4M3::largestNormal(1).asFloat())
                        {
                            inputAConverted = tcu::FloatE4M3::largestNormal(inputA > 0 ? 1 : -1).asFloat();
                        }
                        break;
                    }
                    default:
                        break;
                    }
                }

                if (inputAConverted != output && !(std::isnan(inputAConverted) && std::isnan(output)))
                {
                    //printf("i %d inputA %f inputAConverted %f output %f\n", i, inputA, inputAConverted, output);
                    res = QP_TEST_RESULT_FAIL;
                    break;
                }
            }
        }
        else if (isFloatType(dataTypes[0]))
        {
            if (isReduceOp(m_data.testType))
            {
                uint32_t numMatrixX = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsX :
                                          (m_data.subgroupsPerWorkgroupX * m_data.workgroupsX);
                uint32_t numMatrixY = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsY :
                                          (m_data.subgroupsPerWorkgroupY * m_data.workgroupsY);
                for (uint32_t mX = 0; mX < numMatrixX; ++mX)
                {
                    for (uint32_t mY = 0; mY < numMatrixY; ++mY)
                    {
                        auto const getA = [&](uint32_t i, uint32_t j) -> float
                        {
                            uint32_t ij;
                            if (m_data.colMajor)
                                ij = mX * dims[0].rows + i + strides[0] * mY * dims[0].cols + loadStrides[0] * j;
                            else
                                ij = mX * dims[0].cols + j + strides[0] * mY * dims[0].rows + loadStrides[0] * i;

                            float Aij = getDataFloat(ptrs[0], dataTypes[0], ij);
                            return Aij;
                        };

                        auto const getD = [&](uint32_t i, uint32_t j) -> float
                        {
                            uint32_t ij;
                            // When loading with stride 0, ij for matrix D is different from matrix C
                            if (m_data.colMajor)
                                ij = mX * dims[3].rows + i + strides[3] * (mY * dims[3].cols + j);
                            else
                                ij = mX * dims[3].cols + j + strides[3] * (mY * dims[3].rows + i);

                            float Dij = getDataFloat(ptrs[3], dataTypes[3], ij);
                            return Dij;
                        };

                        std::function<float(float, float)> Combine;
                        float identity;
                        if (isReduceSum(m_data.testType))
                        {
                            Combine  = [](float a, float b) { return a + b; };
                            identity = 0;
                        }
                        else if (isReduceMin(m_data.testType))
                        {
                            Combine  = [](float a, float b) { return std::min(a, b); };
                            identity = std::numeric_limits<float>::max();
                        }
                        else
                        {
                            Combine  = [](float a, float b) { return std::max(a, b); };
                            identity = -std::numeric_limits<float>::max();
                        }

                        uint32_t outputM = M * reduceMScale(m_data.testType);
                        uint32_t outputN = N * reduceNScale(m_data.testType);
                        if (isReduceRow(m_data.testType))
                        {
                            for (uint32_t i = 0; i < M; ++i)
                            {
                                float ref = identity;
                                for (uint32_t j = 0; j < N; ++j)
                                {
                                    ref = Combine(ref, getA(i, j));
                                }
                                for (uint32_t j = 0; j < outputN; ++j)
                                {
                                    float Dij = getD(i, j);
                                    if (fabs(ref - Dij) / (fabs(ref) + 0.001) > 3.0 / 1024)
                                    {
                                        //printf("mX %d mY %d i %d j %d ref %f Dij %f\n", mX, mY, i, j, ref, Dij);
                                        res = QP_TEST_RESULT_FAIL;
                                    }
                                    float Di0 = getD(i, 0);
                                    if (Dij != Di0)
                                    {
                                        //printf("mX %d mY %d i %d j %d Di0 %f Dij %f\n", mX, mY, i, j, Di0, Dij);
                                        res = QP_TEST_RESULT_FAIL;
                                    }
                                }
                            }
                        }
                        else if (isReduceCol(m_data.testType))
                        {
                            for (uint32_t j = 0; j < N; ++j)
                            {
                                float ref = identity;
                                for (uint32_t i = 0; i < M; ++i)
                                {
                                    ref = Combine(ref, getA(i, j));
                                }
                                for (uint32_t i = 0; i < outputM; ++i)
                                {
                                    float Dij = getD(i, j);
                                    if (fabs(ref - Dij) / (fabs(ref) + 0.001) > 3.0 / 1024)
                                    {
                                        //printf("mX %d mY %d i %d j %d ref %f Dij %f\n", mX, mY, i, j, ref, Dij);
                                        res = QP_TEST_RESULT_FAIL;
                                    }
                                    float D0j = getD(0, j);
                                    if (Dij != D0j)
                                    {
                                        //printf("mX %d mY %d i %d j %d D0j %f Dij %f\n", mX, mY, i, j, D0j, Dij);
                                        res = QP_TEST_RESULT_FAIL;
                                    }
                                }
                            }
                        }
                        else if (isReduceRowCol(m_data.testType))
                        {
                            float ref = identity;
                            for (uint32_t i = 0; i < M; ++i)
                            {
                                for (uint32_t j = 0; j < N; ++j)
                                {
                                    ref = Combine(ref, getA(i, j));
                                }
                            }
                            for (uint32_t i = 0; i < outputM; ++i)
                            {
                                for (uint32_t j = 0; j < outputN; ++j)
                                {
                                    float Dij = getD(i, j);
                                    if (fabs(ref - Dij) / (fabs(ref) + 0.001) > 3.0 / 1024)
                                    {
                                        //printf("mX %d mY %d i %d j %d ref %f Dij %f\n", mX, mY, i, j, ref, Dij);
                                        res = QP_TEST_RESULT_FAIL;
                                    }
                                    float D00 = getD(0, 0);
                                    if (Dij != D00)
                                    {
                                        //printf("mX %d mY %d i %d j %d D00 %f Dij %f\n", mX, mY, i, j, D00, Dij);
                                        res = QP_TEST_RESULT_FAIL;
                                    }
                                }
                            }
                        }
                        else if (isReduce2x2(m_data.testType))
                        {
                            for (uint32_t j = 0; j < N; ++j)
                            {
                                for (uint32_t i = 0; i < M; ++i)
                                {
                                    float ref = identity;
                                    ref       = Combine(ref, getA(i * 2 + 0, j * 2 + 0));
                                    ref       = Combine(ref, getA(i * 2 + 0, j * 2 + 1));
                                    ref       = Combine(ref, getA(i * 2 + 1, j * 2 + 0));
                                    ref       = Combine(ref, getA(i * 2 + 1, j * 2 + 1));

                                    float Dij = getD(i, j);
                                    if (ref != Dij)
                                    {
                                        //printf("mX %d mY %d i %d j %d ref %f Dij %f\n", mX, mY, i, j, ref, Dij);
                                        res = QP_TEST_RESULT_FAIL;
                                    }
                                }
                            }
                        }
                        else
                        {
                            DE_ASSERT(0);
                        }
                    }
                }
            }
            else if (m_data.testType == TT_TRANSPOSE_ACC_TO_B)
            {
                uint32_t ij;
                uint32_t numMatrixX = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsX :
                                          (m_data.subgroupsPerWorkgroupX * m_data.workgroupsX);
                uint32_t numMatrixY = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsY :
                                          (m_data.subgroupsPerWorkgroupY * m_data.workgroupsY);
                for (uint32_t mX = 0; mX < numMatrixX; ++mX)
                {
                    for (uint32_t mY = 0; mY < numMatrixY; ++mY)
                    {
                        for (uint32_t i = 0; i < M; ++i)
                        {
                            for (uint32_t j = 0; j < N; ++j)
                            {
                                // for row-major, src is MxN, so row,col = i,j
                                if (m_data.colMajor)
                                    ij = mX * M + i + strides[0] * mY * N + loadStrides[0] * j;
                                else
                                    ij = mX * N + j + strides[0] * mY * M + loadStrides[0] * i;

                                float ref = getDataFloat(ptrs[0], dataTypes[0], ij);

                                // for row-major, dst is NxM, so row,col = j,i
                                if (m_data.colMajor)
                                    ij = mX * N + j + strides[3] * (mY * M + i);
                                else
                                    ij = mX * M + i + strides[3] * (mY * N + j);

                                float Dij = getDataFloat(ptrs[3], dataTypes[3], ij);

                                uint32_t temp;
                                setDataFloat(&temp, dataTypes[3], 0, ref);
                                float convertedRef;
                                convertedRef = getDataFloat(&temp, dataTypes[3], 0);

                                if (convertedRef != Dij)
                                {
                                    res = QP_TEST_RESULT_FAIL;
                                }
                            }
                        }
                    }
                }
            }
            else if (m_data.testType == TT_SPACETODEPTH)
            {
                uint32_t H = 32;
                uint32_t W = 32;
                uint32_t C = 16;
                for (uint32_t h = 0; h < H; ++h)
                {
                    for (uint32_t w = 0; w < W; ++w)
                    {
                        for (uint32_t c = 0; c < C; ++c)
                        {
                            uint32_t inputIndex  = (h * W + w) * C + c;
                            uint32_t outputIndex = ((h / 2) * W / 2 + w / 2) * 4 * C + ((h & 1) * 2 + (w & 1)) * C + c;
                            float ref            = getDataFloat(ptrs[0], dataTypes[0], inputIndex);
                            float output         = getDataFloat(ptrs[3], dataTypes[3], outputIndex);
                            if (ref != output)
                            {
                                //printf("h %d w %d c %d ref %f output %f\n", h, w, c, ref, output);
                                res = QP_TEST_RESULT_FAIL;
                            }
                        }
                    }
                }
            }
            else if (isTensorLayoutTest(m_data.testType))
            {
                uint32_t dim = GetDim(m_data.testType);
                for (int32_t i0 = 0; i0 < GetTensorLayoutDim(dim)[0]; ++i0)
                {
                    for (int32_t i1 = 0; i1 < GetTensorLayoutDim(dim)[1]; ++i1)
                    {
                        for (int32_t i2 = 0; i2 < GetTensorLayoutDim(dim)[2]; ++i2)
                        {
                            for (int32_t i3 = 0; i3 < GetTensorLayoutDim(dim)[3]; ++i3)
                            {
                                for (int32_t i4 = 0; i4 < GetTensorLayoutDim(dim)[4]; ++i4)
                                {
                                    int32_t tensorCoord[5] = {i0, i1, i2, i3, i4};
                                    uint32_t index         = 0;
                                    for (uint32_t k = 0; k < dim; ++k)
                                    {
                                        index = index * GetTensorLayoutDim(dim)[k] + tensorCoord[k];
                                    }
                                    float ref    = 14.0f;
                                    float output = getDataFloat(ptrs[3], dataTypes[3], index);
                                    // If the dest coord is in one of the store rectangles, compute
                                    // a different reference value.
                                    for (uint32_t r = 0; r < GetTensorLayoutNumCoords(dim); ++r)
                                    {
                                        bool inStoreRect = true;
                                        for (uint32_t k = 0; k < dim; ++k)
                                        {
                                            if ((int32_t)tensorCoord[k] < GetTensorLayoutStoreOffsets(dim, r)[k] ||
                                                (int32_t)tensorCoord[k] >= GetTensorLayoutStoreOffsets(dim, r)[k] +
                                                                               GetTensorLayoutSpan(dim, r)[k])
                                            {
                                                inStoreRect = false;
                                            }
                                        }

                                        if (inStoreRect)
                                        {
                                            int32_t loadCoord[5] = {i0, i1, i2, i3, i4};
                                            for (uint32_t k = 0; k < dim; ++k)
                                            {
                                                loadCoord[k] = loadCoord[k] - GetTensorLayoutStoreOffsets(dim, r)[k] +
                                                               GetTensorLayoutLoadOffsets(dim, r)[k];
                                            }
                                            bool OOB = false;
                                            // gl_CooperativeMatrixClampModeConstant bounds checking
                                            for (uint32_t k = 0; k < dim; ++k)
                                            {
                                                if (loadCoord[k] < 0 || loadCoord[k] >= GetTensorLayoutDim(dim)[k])
                                                {
                                                    OOB = true;
                                                }
                                            }
                                            if (OOB)
                                            {
                                                ref = 0.0f;
                                            }
                                            else
                                            {
                                                index = 0;
                                                for (uint32_t k = 0; k < dim; ++k)
                                                {
                                                    index = index * GetTensorLayoutDim(dim)[k] + loadCoord[k];
                                                }
                                                ref = getDataFloat(ptrs[0], dataTypes[0], index);
                                            }
                                            break;
                                        }
                                    }
                                    if (ref != output)
                                    {
                                        //printf("tensorCoord {%d, %d, %d, %d, %d} ref %f output %f\n", tensorCoord[0], tensorCoord[1], tensorCoord[2], tensorCoord[3], tensorCoord[4], ref, output);
                                        res = QP_TEST_RESULT_FAIL;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else if (m_data.testType == TT_PER_ELEMENT_OP_ROW_COL)
            {
                uint32_t ij;
                uint32_t numMatrixX = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsX :
                                          (m_data.subgroupsPerWorkgroupX * m_data.workgroupsX);
                uint32_t numMatrixY = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsY :
                                          (m_data.subgroupsPerWorkgroupY * m_data.workgroupsY);
                for (uint32_t mX = 0; mX < numMatrixX; ++mX)
                {
                    for (uint32_t mY = 0; mY < numMatrixY; ++mY)
                    {
                        for (uint32_t i = 0; i < M; ++i)
                        {
                            for (uint32_t j = 0; j < N; ++j)
                            {
                                if (m_data.colMajor)
                                    ij = mX * M + i + strides[0] * mY * N + loadStrides[0] * j;
                                else
                                    ij = mX * N + j + strides[0] * mY * M + loadStrides[0] * i;

                                float ref = getDataFloat(ptrs[0], dataTypes[0], ij);

                                float Dij = getDataFloat(ptrs[3], dataTypes[3], ij);

                                if (ref + (float)(i * 3 + j) != Dij)
                                {
                                    res = QP_TEST_RESULT_FAIL;
                                }
                            }
                        }
                    }
                }
            }
            else if (isClampTest(m_data.testType))
            {
                uint32_t ij;
                uint32_t numMatrixX = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsX :
                                          (m_data.subgroupsPerWorkgroupX * m_data.workgroupsX);
                uint32_t numMatrixY = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsY :
                                          (m_data.subgroupsPerWorkgroupY * m_data.workgroupsY);
                uint32_t fullDimX   = numMatrixX * (m_data.colMajor ? dims[0].rows : dims[0].cols);
                uint32_t fullDimY   = numMatrixY * (m_data.colMajor ? dims[0].cols : dims[0].rows);
                uint32_t dimX       = fullDimX - 6;
                uint32_t dimY       = fullDimY - 6;
                for (uint32_t mX = 0; mX < numMatrixX; ++mX)
                {
                    for (uint32_t mY = 0; mY < numMatrixY; ++mY)
                    {
                        for (uint32_t i = 0; i < M; ++i)
                        {
                            for (uint32_t j = 0; j < N; ++j)
                            {
                                int32_t i2;
                                int32_t j2;
                                bool OOBLoad  = false;
                                bool OOBStore = false;

                                if (m_data.colMajor)
                                {
                                    i2       = mX * M + i;
                                    j2       = mY * N + j;
                                    ij       = i2 + strides[3] * j2;
                                    OOBStore = i2 == (int32_t)fullDimX - 1 || j2 == (int32_t)fullDimY - 1;
                                }
                                else
                                {
                                    i2       = mY * M + i;
                                    j2       = mX * N + j;
                                    ij       = j2 + strides[3] * i2;
                                    OOBStore = i2 == (int32_t)fullDimY - 1 || j2 == (int32_t)fullDimX - 1;
                                }

                                float Dij = getDataFloat(ptrs[3], dataTypes[3], ij);

                                auto const mod = [](int32_t n, int32_t d) -> int32_t
                                {
                                    // works for the range of values we use
                                    return (n + d) % d;
                                };

                                i2 -= 3;
                                j2 -= 3;
                                uint32_t dimI = m_data.colMajor ? dimX : dimY;
                                uint32_t dimJ = m_data.colMajor ? dimY : dimX;
                                switch (m_data.testType)
                                {
                                case TT_CLAMPCONSTANT:
                                    OOBLoad = i2 < 0 || j2 < 0 || i2 >= (int32_t)dimI || j2 >= (int32_t)dimJ;
                                    break;
                                case TT_CLAMPTOEDGE:
                                    i2 = std::min(std::max(i2, 0), (int32_t)dimI - 1);
                                    j2 = std::min(std::max(j2, 0), (int32_t)dimJ - 1);
                                    break;
                                case TT_CLAMPREPEAT:
                                    i2 = mod(i2, dimI);
                                    j2 = mod(j2, dimJ);
                                    break;
                                case TT_CLAMPMIRRORREPEAT:
                                    i2 = mod(i2, (2 * dimI - 2));
                                    i2 = (i2 >= (int32_t)dimI) ? (2 * dimI - 2 - i2) : i2;
                                    j2 = mod(j2, (2 * dimJ - 2));
                                    j2 = (j2 >= (int32_t)dimJ) ? (2 * dimJ - 2 - j2) : j2;
                                    break;
                                default:
                                    DE_ASSERT(0);
                                    break;
                                }

                                if (m_data.colMajor)
                                {
                                    ij = i2 + strides[0] * j2;
                                }
                                else
                                {
                                    ij = j2 + strides[0] * i2;
                                }

                                float ref = OOBStore ? 14.0f : OOBLoad ? 0.5f : getDataFloat(ptrs[0], dataTypes[0], ij);

                                if (ref != Dij)
                                {
                                    //printf("fail ");
                                    res = QP_TEST_RESULT_FAIL;
                                }
                                //printf("i %d j %d ref %f Dij %f\n", i, j, ref, Dij);
                            }
                        }
                    }
                }
            }
            else if ((m_data.addrMethod == ADDR_BLOCKSIZE || m_data.addrMethod == ADDR_DECODE) &&
                     m_data.testType != TT_MATRIXMULADD_DEQUANT)
            {
                uint32_t ij, blockij;
                uint32_t numMatrixX = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsX :
                                          (m_data.subgroupsPerWorkgroupX * m_data.workgroupsX);
                uint32_t numMatrixY = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsY :
                                          (m_data.subgroupsPerWorkgroupY * m_data.workgroupsY);
                for (uint32_t mX = 0; mX < numMatrixX; ++mX)
                {
                    for (uint32_t mY = 0; mY < numMatrixY; ++mY)
                    {
                        for (uint32_t i = 0; i < M; ++i)
                        {
                            for (uint32_t j = 0; j < N; ++j)
                            {
                                uint32_t blockCoords[2];
                                if (m_data.colMajor)
                                {
                                    blockCoords[0] = (mY * N + j) / blockSize[0];
                                    blockCoords[1] = (mX * M + i) / blockSize[1];
                                    blockij        = blockCoords[1] + (strides[0] / blockSize[1]) * blockCoords[0];
                                    if (m_data.addrMethod == ADDR_DECODE)
                                    {
                                        blockij *= blockSize[0] * blockSize[1];
                                        blockij += (j % blockSize[0]) * blockSize[1] + (i % blockSize[1]);
                                    }
                                    ij = mX * M + i + strides[0] * mY * N + loadStrides[0] * j;
                                }
                                else
                                {
                                    blockCoords[0] = (mY * M + i) / blockSize[0];
                                    blockCoords[1] = (mX * N + j) / blockSize[1];
                                    blockij        = blockCoords[1] + (strides[0] / blockSize[1]) * blockCoords[0];
                                    if (m_data.addrMethod == ADDR_DECODE)
                                    {
                                        blockij *= blockSize[0] * blockSize[1];
                                        blockij += (i % blockSize[0]) * blockSize[1] + (j % blockSize[1]);
                                    }
                                    ij = mX * N + j + strides[0] * mY * M + loadStrides[0] * i;
                                }

                                float ref = getDataFloat(ptrs[0], dataTypes[0], blockij);

                                if (m_data.addrMethod == ADDR_DECODE)
                                {
                                    ref += (float)((2 * blockCoords[0] + blockCoords[1]) & 3);
                                }

                                float Dij = getDataFloat(ptrs[3], dataTypes[3], ij);

                                if (m_data.testType == TT_NEGATE)
                                {
                                    ref = -ref;
                                }
                                else
                                {
                                    DE_ASSERT(0);
                                }

                                if (ref != Dij)
                                {
                                    //printf("fail ");
                                    res = QP_TEST_RESULT_FAIL;
                                }
                                //printf("mX %d mY %d i %d j %d ref %f D %f\n", mX, mY, i, j, ref, Dij);
                            }
                        }
                    }
                }
            }
            else if (!isMatrixMulAddOp(m_data.testType))
            {
                for (uint32_t i = 0; i < totalElements[3]; ++i)
                {
                    float inputA = getDataFloat(ptrs[0], dataTypes[0], i);
                    float inputB = getDataFloat(ptrs[1], dataTypes[1], i);
                    float output = getDataFloat(ptrs[3], dataTypes[3], i);
                    switch (m_data.testType)
                    {
                    case TT_LENGTH:
                        if (output < 1.0f || output > (float)(N * M))
                            res = QP_TEST_RESULT_FAIL;
                        if (m_data.scope == VK_SCOPE_SUBGROUP_KHR)
                        {
                            // We expect the matrix to be spread evenly across invocations, it is
                            // surprising (but not necessarily illegal) if not
                            if (output != (float)(N * M / subgroupSize) && res == QP_TEST_RESULT_PASS)
                            {
                                res = QP_TEST_RESULT_QUALITY_WARNING;
                            }
                        }
                        break;
                    case TT_CONSTANT:
                        if (output != 1.0f)
                            res = QP_TEST_RESULT_FAIL;
                        break;
                    case TT_COMPOSITE:
                    case TT_ADD:
                        if (output != inputA + inputB)
                            res = QP_TEST_RESULT_FAIL;
                        break;
                    case TT_COMPOSITE_ARRAY:
                    case TT_COMPOSITE_RVALUE:
                        if (output != inputA)
                            res = QP_TEST_RESULT_FAIL;
                        break;
                    case TT_SUB:
                        if (output != inputA - inputB)
                            res = QP_TEST_RESULT_FAIL;
                        break;
                    case TT_DIV:
                    {
                        float ulp = (m_data.inputType == VK_COMPONENT_TYPE_FLOAT16_KHR) ?
                                        1.0f / 1024.0f :
                                        1.0f / (8.0f * 1024.0f * 1024.0f);
                        // division allows 2.5ulp, but we'll use 3.
                        ulp *= 3;
                        if (inputB != 0 && fabs(output - inputA / inputB) > ulp * fabs(inputA / inputB))
                            res = QP_TEST_RESULT_FAIL;
                    }
                    break;
                    case TT_MUL:
                    {
                        if (dataTypes[0] == VK_COMPONENT_TYPE_FLOAT16_KHR)
                        {
                            const float expected32          = inputA * inputB;
                            const tcu::float16_t expected16 = tcu::Float16(expected32).bits();
                            const float expected            = tcu::Float16(expected16).asFloat();

                            if (output != expected)
                                res = QP_TEST_RESULT_FAIL;
                        }
                        else
                        {
                            if (output != inputA * inputB)
                                res = QP_TEST_RESULT_FAIL;
                        }
                        break;
                    }
                    case TT_NEGATE:
                    case TT_FUNC:
                    case TT_FUNC_CONST_IN:
                        if (output != -inputA)
                            res = QP_TEST_RESULT_FAIL;
                        break;
                    case TT_MATRIXTIMESSCALAR:
                        if (output != 6.0 * inputA)
                            res = QP_TEST_RESULT_FAIL;
                        break;
                    case TT_MULTICOMPONENT_LOAD:
                    {
                        if (output != inputA)
                            res = QP_TEST_RESULT_FAIL;
                        break;
                    }
                    case TT_MULTICOMPONENT_SAVE:
                    case TT_CONVERT_ACC_TO_A:
                    case TT_CONVERT_ACC_TO_B:
                    {
                        uint32_t temp;
                        setDataFloat(&temp, dataTypes[3], 0, inputA);
                        float convertedInput;
                        convertedInput = getDataFloat(&temp, dataTypes[3], 0);
                        if (output != convertedInput)
                        {
                            //printf("i %d inputA %f convertedInput %f output %f\n", i, inputA, convertedInput, output);
                            res = QP_TEST_RESULT_FAIL;
                        }
                        break;
                    }
                    case TT_PER_ELEMENT_OP:
                    case TT_PER_ELEMENT_OP_STRUCT:
                        if (output != inputA + 2.0)
                            res = QP_TEST_RESULT_FAIL;
                        break;
                    case TT_PER_ELEMENT_OP_MAT:
                        if (output != 3 * inputA)
                            res = QP_TEST_RESULT_FAIL;
                        break;
                    default:
                        TCU_THROW(InternalError, "Unimplemented");
                    }
                }
            }
            else
            {
                uint32_t ik, kj, ij;
                uint32_t numMatrixX = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsX :
                                          (m_data.subgroupsPerWorkgroupX * m_data.workgroupsX);
                uint32_t numMatrixY = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsY :
                                          (m_data.subgroupsPerWorkgroupY * m_data.workgroupsY);
                for (uint32_t mX = 0; mX < numMatrixX; ++mX)
                {
                    for (uint32_t mY = 0; mY < numMatrixY; ++mY)
                    {
                        for (uint32_t i = 0; i < M; ++i)
                        {
                            for (uint32_t j = 0; j < N; ++j)
                            {
                                float ref = 0;
                                for (uint32_t k = 0; k < K; ++k)
                                {
                                    float Aik, Bkj;
                                    if (m_data.testType == TT_MATRIXMULADD_DEQUANT)
                                    {
                                        uint32_t idxInBlock, idx, arrayidx, shift;
                                        int32_t value;

                                        // Blocks are stored in row-major order. Compute index of the block
                                        // and index within block.
                                        DE_ASSERT(!m_data.colMajor);
                                        uint32_t blockik = ((mX * K + k) / blockSize[1]) +
                                                           (strides[0] / blockSize[1]) * ((mY * M + i) / blockSize[0]);

                                        idxInBlock = (i % blockSize[0]) * blockSize[1] + (k % blockSize[1]);

                                        // Compute block index (idx) and extract a 4bpp element from the block
                                        idx      = blockik * blockSize[0] * blockSize[1] + idxInBlock;
                                        arrayidx = idx / 2;
                                        shift    = (idx & 1) * 4;
                                        value    = getDataInt(ptrs[0], VK_COMPONENT_TYPE_UINT8_KHR, arrayidx);
                                        value    = (value >> shift) & 0xF;
                                        // decode
                                        Aik = 0.5f * (float)(((value & 7) - 3) * (1 + value / 8));

                                        // Repeat for B matrix
                                        uint32_t blockkj = ((mX * N + j) / blockSize[1]) +
                                                           (strides[1] / blockSize[1]) * ((mY * K + k) / blockSize[0]);

                                        idxInBlock = (k % blockSize[0]) * blockSize[1] + (j % blockSize[1]);

                                        idx      = blockkj * blockSize[0] * blockSize[1] + idxInBlock;
                                        arrayidx = idx / 2;
                                        shift    = (idx & 1) * 4;
                                        value    = getDataInt(ptrs[1], VK_COMPONENT_TYPE_UINT8_KHR, arrayidx);
                                        value    = (value >> shift) & 0xF;
                                        Bkj      = 0.5f * (float)(((value & 7) - 3) * (1 + value / 8));
                                    }
                                    else
                                    {
                                        if (m_data.colMajor)
                                            ik = mX * M + i + strides[0] * mY * K + loadStrides[0] * k;
                                        else
                                            ik = mX * K + k + strides[0] * mY * M + loadStrides[0] * i;

                                        Aik = getDataFloat(ptrs[0], dataTypes[0], ik);

                                        if (m_data.colMajor)
                                            kj = mX * K + k + strides[1] * mY * N + loadStrides[1] * j;
                                        else
                                            kj = mX * N + j + strides[1] * mY * K + loadStrides[1] * k;

                                        Bkj = getDataFloat(ptrs[1], dataTypes[1], kj);
                                    }

                                    ref += Aik * Bkj;
                                }

                                if (m_data.colMajor)
                                    ij = mX * M + i + strides[2] * mY * N + loadStrides[2] * j;
                                else
                                    ij = mX * N + j + strides[2] * mY * M + loadStrides[2] * i;

                                float Cij = getDataFloat(ptrs[2], dataTypes[2], ij);

                                ref += Cij;

                                // When loading with stride 0, ij for matrix D is different from matrix C
                                if (m_data.colMajor)
                                    ij = mX * M + i + strides[2] * (mY * N + j);
                                else
                                    ij = mX * N + j + strides[2] * (mY * M + i);

                                float Dij = getDataFloat(ptrs[3], dataTypes[3], ij);

                                //printf("i %d j %d ref %f Dij %f\n", i, j, ref, Dij);

                                if (fabs(ref - Dij) > epsilon)
                                {
                                    if (max(max(M, N), K) >= 48)
                                    {
                                        if (fabs(ref - Dij) / (fabs(ref) + 0.001) > 3.0 / 1024)
                                        {
                                            //printf("ref %f Dij %f\n", ref, Dij);
                                            res = QP_TEST_RESULT_FAIL;
                                        }
                                    }
                                    else
                                    {
                                        //printf("i %d j %d ref %f Dij %f\n", i, j, ref, Dij);
                                        res = QP_TEST_RESULT_FAIL;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
            if (isReduceOp(m_data.testType))
            {
                uint32_t numMatrixX = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsX :
                                          (m_data.subgroupsPerWorkgroupX * m_data.workgroupsX);
                uint32_t numMatrixY = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsY :
                                          (m_data.subgroupsPerWorkgroupY * m_data.workgroupsY);
                int resultSize      = componentTypeInfo.at(dataTypes[3]).bits;
                uint32_t mask       = resultSize == 32 ? ~0 : ((1 << resultSize) - 1);
                for (uint32_t mX = 0; mX < numMatrixX; ++mX)
                {
                    for (uint32_t mY = 0; mY < numMatrixY; ++mY)
                    {
                        bool isSigned   = componentTypeInfo.at(dataTypes[0]).isSigned;
                        auto const getA = [&](uint32_t i, uint32_t j) -> int64_t
                        {
                            uint32_t ij;
                            if (m_data.colMajor)
                                ij = mX * dims[0].rows + i + strides[0] * mY * dims[0].cols + loadStrides[0] * j;
                            else
                                ij = mX * dims[0].cols + j + strides[0] * mY * dims[0].rows + loadStrides[0] * i;

                            uint32_t Aij = getDataInt(ptrs[0], dataTypes[0], ij);
                            if (isSigned)
                            {
                                return (int64_t)(int32_t)Aij;
                            }
                            else
                            {
                                return (int64_t)Aij;
                            }
                        };

                        auto const getD = [&](uint32_t i, uint32_t j) -> int64_t
                        {
                            uint32_t ij;
                            // When loading with stride 0, ij for matrix D is different from matrix C
                            if (m_data.colMajor)
                                ij = mX * dims[3].rows + i + strides[3] * (mY * dims[3].cols + j);
                            else
                                ij = mX * dims[3].cols + j + strides[3] * (mY * dims[3].rows + i);

                            uint32_t Dij = getDataInt(ptrs[3], dataTypes[3], ij);
                            if (isSigned)
                            {
                                return (int64_t)(int32_t)Dij;
                            }
                            else
                            {
                                return (int64_t)Dij;
                            }
                        };

                        std::function<int64_t(int64_t, int64_t)> Combine;
                        int64_t identity;
                        if (isReduceSum(m_data.testType))
                        {
                            Combine  = [](int64_t a, int64_t b) { return a + b; };
                            identity = 0;
                        }
                        else if (isReduceMin(m_data.testType))
                        {
                            Combine  = [](int64_t a, int64_t b) { return std::min(a, b); };
                            identity = std::numeric_limits<int64_t>::max();
                        }
                        else
                        {
                            Combine  = [](int64_t a, int64_t b) { return std::max(a, b); };
                            identity = -std::numeric_limits<int64_t>::max();
                        }

                        uint32_t outputM = M * reduceMScale(m_data.testType);
                        uint32_t outputN = N * reduceNScale(m_data.testType);
                        if (isReduceRow(m_data.testType))
                        {
                            for (uint32_t i = 0; i < M; ++i)
                            {
                                int64_t ref = identity;
                                for (uint32_t j = 0; j < N; ++j)
                                {
                                    ref = Combine(ref, getA(i, j));
                                }
                                for (uint32_t j = 0; j < outputN; ++j)
                                {
                                    int64_t Dij = getD(i, j);
                                    if ((ref & mask) != (Dij & mask))
                                    {
                                        //printf("mX %d mY %d i %d j %d ref %d Dij %d\n", mX, mY, i, j, (int)ref, (int)Dij);
                                        res = QP_TEST_RESULT_FAIL;
                                    }
                                    int64_t Di0 = getD(i, 0);
                                    if (Dij != Di0)
                                    {
                                        //printf("mX %d mY %d i %d j %d Di0 %d Dij %d\n", mX, mY, i, j, (int)Di0, (int)Dij);
                                        res = QP_TEST_RESULT_FAIL;
                                    }
                                }
                            }
                        }
                        else if (isReduceCol(m_data.testType))
                        {
                            for (uint32_t j = 0; j < N; ++j)
                            {
                                int64_t ref = identity;
                                for (uint32_t i = 0; i < M; ++i)
                                {
                                    ref = Combine(ref, getA(i, j));
                                }
                                for (uint32_t i = 0; i < outputM; ++i)
                                {
                                    int64_t Dij = getD(i, j);
                                    if ((ref & mask) != (Dij & mask))
                                    {
                                        //printf("mX %d mY %d i %d j %d ref %d Dij %d\n", mX, mY, i, j, (int)ref, (int)Dij);
                                        res = QP_TEST_RESULT_FAIL;
                                    }
                                    int64_t D0j = getD(0, j);
                                    if (Dij != D0j)
                                    {
                                        //printf("mX %d mY %d i %d j %d D0j %d Dij %d\n", mX, mY, i, j, (int)D0j, (int)Dij);
                                        res = QP_TEST_RESULT_FAIL;
                                    }
                                }
                            }
                        }
                        else if (isReduceRowCol(m_data.testType))
                        {
                            int64_t ref = identity;
                            for (uint32_t i = 0; i < M; ++i)
                            {
                                for (uint32_t j = 0; j < N; ++j)
                                {
                                    ref = Combine(ref, getA(i, j));
                                }
                            }
                            for (uint32_t i = 0; i < outputM; ++i)
                            {
                                for (uint32_t j = 0; j < outputN; ++j)
                                {
                                    int64_t Dij = getD(i, j);
                                    if ((ref & mask) != (Dij & mask))
                                    {
                                        //printf("mX %d mY %d i %d j %d ref %d Dij %d\n", mX, mY, i, j, (int)ref, (int)Dij);
                                        res = QP_TEST_RESULT_FAIL;
                                    }
                                    int64_t D00 = getD(0, 0);
                                    if (Dij != D00)
                                    {
                                        //printf("mX %d mY %d i %d j %d D00 %d Dij %d\n", mX, mY, i, j, (int)D00, (int)Dij);
                                        res = QP_TEST_RESULT_FAIL;
                                    }
                                }
                            }
                        }
                        else if (isReduce2x2(m_data.testType))
                        {
                            for (uint32_t j = 0; j < N; ++j)
                            {
                                for (uint32_t i = 0; i < M; ++i)
                                {
                                    int64_t ref = identity;
                                    ref         = Combine(ref, getA(i * 2 + 0, j * 2 + 0));
                                    ref         = Combine(ref, getA(i * 2 + 0, j * 2 + 1));
                                    ref         = Combine(ref, getA(i * 2 + 1, j * 2 + 0));
                                    ref         = Combine(ref, getA(i * 2 + 1, j * 2 + 1));

                                    int64_t Dij = getD(i, j);
                                    if ((ref & mask) != (Dij & mask))
                                    {
                                        //printf("mX %d mY %d i %d j %d ref %d Dij %d\n", mX, mY, i, j, (int)ref, (int)Dij);
                                        res = QP_TEST_RESULT_FAIL;
                                    }
                                }
                            }
                        }
                        else
                        {
                            DE_ASSERT(0);
                        }
                    }
                }
            }
            else if (m_data.testType == TT_TRANSPOSE_ACC_TO_B)
            {
                uint32_t ij;
                uint32_t numMatrixX = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsX :
                                          (m_data.subgroupsPerWorkgroupX * m_data.workgroupsX);
                uint32_t numMatrixY = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsY :
                                          (m_data.subgroupsPerWorkgroupY * m_data.workgroupsY);
                int resultSize      = componentTypeInfo.at(dataTypes[3]).bits;
                uint32_t mask       = resultSize == 32 ? ~0 : ((1 << resultSize) - 1);

                for (uint32_t mX = 0; mX < numMatrixX; ++mX)
                {
                    for (uint32_t mY = 0; mY < numMatrixY; ++mY)
                    {
                        for (uint32_t i = 0; i < M; ++i)
                        {
                            for (uint32_t j = 0; j < N; ++j)
                            {
                                // for row-major, src is MxN, so row,col = i,j
                                if (m_data.colMajor)
                                    ij = mX * M + i + strides[0] * mY * N + loadStrides[0] * j;
                                else
                                    ij = mX * N + j + strides[0] * mY * M + loadStrides[0] * i;

                                uint32_t ref = getDataInt(ptrs[0], dataTypes[0], ij);

                                // for row-major, dst is NxM, so row,col = j,i
                                if (m_data.colMajor)
                                    ij = mX * N + j + strides[3] * (mY * M + i);
                                else
                                    ij = mX * M + i + strides[3] * (mY * N + j);

                                uint32_t Dij = getDataInt(ptrs[3], dataTypes[3], ij);

                                if ((ref & mask) != (Dij & mask))
                                {
                                    res = QP_TEST_RESULT_FAIL;
                                }
                            }
                        }
                    }
                }
            }
            else if (m_data.testType == TT_SPACETODEPTH)
            {
                uint32_t H = 32;
                uint32_t W = 32;
                uint32_t C = 16;
                for (uint32_t h = 0; h < H; ++h)
                {
                    for (uint32_t w = 0; w < W; ++w)
                    {
                        for (uint32_t c = 0; c < C; ++c)
                        {
                            uint32_t inputIndex  = (h * W + w) * C + c;
                            uint32_t outputIndex = ((h / 2) * W / 2 + w / 2) * 4 * C + ((h & 1) * 2 + (w & 1)) * C + c;
                            uint32_t ref         = getDataInt(ptrs[0], dataTypes[0], inputIndex);
                            uint32_t output      = getDataInt(ptrs[3], dataTypes[3], outputIndex);
                            if (ref != output)
                            {
                                //printf("h %d w %d c %d ref %d output %d\n", h, w, c, ref, output);
                                res = QP_TEST_RESULT_FAIL;
                            }
                        }
                    }
                }
            }
            else if (isTensorLayoutTest(m_data.testType))
            {
                uint32_t dim = GetDim(m_data.testType);
                for (int32_t i0 = 0; i0 < GetTensorLayoutDim(dim)[0]; ++i0)
                {
                    for (int32_t i1 = 0; i1 < GetTensorLayoutDim(dim)[1]; ++i1)
                    {
                        for (int32_t i2 = 0; i2 < GetTensorLayoutDim(dim)[2]; ++i2)
                        {
                            for (int32_t i3 = 0; i3 < GetTensorLayoutDim(dim)[3]; ++i3)
                            {
                                for (int32_t i4 = 0; i4 < GetTensorLayoutDim(dim)[4]; ++i4)
                                {
                                    int32_t tensorCoord[5] = {i0, i1, i2, i3, i4};
                                    uint32_t index         = 0;
                                    for (uint32_t k = 0; k < dim; ++k)
                                    {
                                        index = index * GetTensorLayoutDim(dim)[k] + tensorCoord[k];
                                    }
                                    uint32_t ref    = 123;
                                    uint32_t output = getDataInt(ptrs[3], dataTypes[3], index);
                                    // If the dest coord is in one of the store rectangles, compute
                                    // a different reference value.
                                    for (uint32_t r = 0; r < GetTensorLayoutNumCoords(dim); ++r)
                                    {
                                        bool inStoreRect = true;
                                        for (uint32_t k = 0; k < dim; ++k)
                                        {
                                            if ((int32_t)tensorCoord[k] < GetTensorLayoutStoreOffsets(dim, r)[k] ||
                                                (int32_t)tensorCoord[k] >= GetTensorLayoutStoreOffsets(dim, r)[k] +
                                                                               GetTensorLayoutSpan(dim, r)[k])
                                            {
                                                inStoreRect = false;
                                            }
                                        }

                                        if (inStoreRect)
                                        {
                                            int32_t loadCoord[5] = {i0, i1, i2, i3, i4};
                                            for (uint32_t k = 0; k < dim; ++k)
                                            {
                                                loadCoord[k] = loadCoord[k] - GetTensorLayoutStoreOffsets(dim, r)[k] +
                                                               GetTensorLayoutLoadOffsets(dim, r)[k];
                                            }
                                            bool OOB = false;
                                            // gl_CooperativeMatrixClampModeConstant bounds checking
                                            for (uint32_t k = 0; k < dim; ++k)
                                            {
                                                if (loadCoord[k] < 0 || loadCoord[k] >= GetTensorLayoutDim(dim)[k])
                                                {
                                                    OOB = true;
                                                }
                                            }
                                            if (OOB)
                                            {
                                                ref = 0;
                                            }
                                            else
                                            {
                                                index = 0;
                                                for (uint32_t k = 0; k < dim; ++k)
                                                {
                                                    index = index * GetTensorLayoutDim(dim)[k] + loadCoord[k];
                                                }
                                                ref = getDataInt(ptrs[0], dataTypes[0], index);
                                            }
                                            break;
                                        }
                                    }
                                    if (ref != output)
                                    {
                                        //printf("tensorCoord {%d, %d, %d, %d, %d} ref %d output %d\n", tensorCoord[0], tensorCoord[1], tensorCoord[2], tensorCoord[3], tensorCoord[4], ref, output);
                                        res = QP_TEST_RESULT_FAIL;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else if (m_data.testType == TT_PER_ELEMENT_OP_ROW_COL)
            {
                uint32_t ij;
                uint32_t numMatrixX = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsX :
                                          (m_data.subgroupsPerWorkgroupX * m_data.workgroupsX);
                uint32_t numMatrixY = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsY :
                                          (m_data.subgroupsPerWorkgroupY * m_data.workgroupsY);
                int resultSize      = componentTypeInfo.at(dataTypes[3]).bits;
                uint32_t mask       = resultSize == 32 ? ~0 : ((1 << resultSize) - 1);
                for (uint32_t mX = 0; mX < numMatrixX; ++mX)
                {
                    for (uint32_t mY = 0; mY < numMatrixY; ++mY)
                    {
                        for (uint32_t i = 0; i < M; ++i)
                        {
                            for (uint32_t j = 0; j < N; ++j)
                            {
                                if (m_data.colMajor)
                                    ij = mX * M + i + strides[0] * mY * N + loadStrides[0] * j;
                                else
                                    ij = mX * N + j + strides[0] * mY * M + loadStrides[0] * i;

                                uint32_t ref = getDataInt(ptrs[0], dataTypes[0], ij);

                                uint32_t Dij = getDataInt(ptrs[3], dataTypes[3], ij);

                                if (((ref + (i * 3 + j)) & mask) != (Dij & mask))
                                {
                                    res = QP_TEST_RESULT_FAIL;
                                }
                            }
                        }
                    }
                }
            }
            else if (isClampTest(m_data.testType))
            {
                uint32_t ij;
                uint32_t numMatrixX = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsX :
                                          (m_data.subgroupsPerWorkgroupX * m_data.workgroupsX);
                uint32_t numMatrixY = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsY :
                                          (m_data.subgroupsPerWorkgroupY * m_data.workgroupsY);
                uint32_t fullDimX   = numMatrixX * (m_data.colMajor ? dims[0].rows : dims[0].cols);
                uint32_t fullDimY   = numMatrixY * (m_data.colMajor ? dims[0].cols : dims[0].rows);
                uint32_t dimX       = fullDimX - 6;
                uint32_t dimY       = fullDimY - 6;
                for (uint32_t mX = 0; mX < numMatrixX; ++mX)
                {
                    for (uint32_t mY = 0; mY < numMatrixY; ++mY)
                    {
                        for (uint32_t i = 0; i < M; ++i)
                        {
                            for (uint32_t j = 0; j < N; ++j)
                            {
                                int32_t i2;
                                int32_t j2;
                                bool OOBLoad  = false;
                                bool OOBStore = false;

                                if (m_data.colMajor)
                                {
                                    i2       = mX * M + i;
                                    j2       = mY * N + j;
                                    ij       = i2 + strides[3] * j2;
                                    OOBStore = i2 == (int32_t)fullDimX - 1 || j2 == (int32_t)fullDimY - 1;
                                }
                                else
                                {
                                    i2       = mY * M + i;
                                    j2       = mX * N + j;
                                    ij       = j2 + strides[3] * i2;
                                    OOBStore = i2 == (int32_t)fullDimY - 1 || j2 == (int32_t)fullDimX - 1;
                                }

                                uint32_t Dij = getDataInt(ptrs[3], dataTypes[3], ij);

                                auto const mod = [](int32_t n, int32_t d) -> int32_t
                                {
                                    // works for the range of values we use
                                    return (n + d) % d;
                                };

                                i2 -= 3;
                                j2 -= 3;
                                uint32_t dimI = m_data.colMajor ? dimX : dimY;
                                uint32_t dimJ = m_data.colMajor ? dimY : dimX;
                                switch (m_data.testType)
                                {
                                case TT_CLAMPCONSTANT:
                                    OOBLoad = i2 < 0 || j2 < 0 || i2 >= (int32_t)dimI || j2 >= (int32_t)dimJ;
                                    break;
                                case TT_CLAMPTOEDGE:
                                    i2 = std::min(std::max(i2, 0), (int32_t)dimI - 1);
                                    j2 = std::min(std::max(j2, 0), (int32_t)dimJ - 1);
                                    break;
                                case TT_CLAMPREPEAT:
                                    i2 = mod(i2, dimI);
                                    j2 = mod(j2, dimJ);
                                    break;
                                case TT_CLAMPMIRRORREPEAT:
                                    i2 = mod(i2, (2 * dimI - 2));
                                    i2 = (i2 >= (int32_t)dimI) ? (2 * dimI - 2 - i2) : i2;
                                    j2 = mod(j2, (2 * dimJ - 2));
                                    j2 = (j2 >= (int32_t)dimJ) ? (2 * dimJ - 2 - j2) : j2;
                                    break;
                                default:
                                    DE_ASSERT(0);
                                    break;
                                }

                                if (m_data.colMajor)
                                {
                                    ij = i2 + strides[0] * j2;
                                }
                                else
                                {
                                    ij = j2 + strides[0] * i2;
                                }

                                uint32_t ref = OOBStore ? 123 : OOBLoad ? 17 : getDataInt(ptrs[0], dataTypes[0], ij);

                                if (ref != Dij)
                                {
                                    res = QP_TEST_RESULT_FAIL;
                                }
                            }
                        }
                    }
                }
            }
            else if (m_data.addrMethod == ADDR_BLOCKSIZE || m_data.addrMethod == ADDR_DECODE)
            {
                uint32_t ij;
                uint32_t blockij;
                uint32_t numMatrixX = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsX :
                                          (m_data.subgroupsPerWorkgroupX * m_data.workgroupsX);
                uint32_t numMatrixY = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsY :
                                          (m_data.subgroupsPerWorkgroupY * m_data.workgroupsY);
                int resultSize      = componentTypeInfo.at(dataTypes[3]).bits;
                uint32_t mask       = resultSize == 32 ? ~0 : ((1 << resultSize) - 1);
                for (uint32_t mX = 0; mX < numMatrixX; ++mX)
                {
                    for (uint32_t mY = 0; mY < numMatrixY; ++mY)
                    {
                        for (uint32_t i = 0; i < M; ++i)
                        {
                            for (uint32_t j = 0; j < N; ++j)
                            {
                                uint32_t blockCoords[2];
                                if (m_data.colMajor)
                                {
                                    blockCoords[0] = (mY * N + j) / blockSize[0];
                                    blockCoords[1] = (mX * M + i) / blockSize[1];
                                    blockij        = blockCoords[1] + (strides[0] / blockSize[1]) * blockCoords[0];
                                    if (m_data.addrMethod == ADDR_DECODE)
                                    {
                                        blockij *= blockSize[0] * blockSize[1];
                                        blockij += (j % blockSize[0]) * blockSize[1] + (i % blockSize[1]);
                                    }
                                    ij = mX * M + i + strides[0] * mY * N + loadStrides[0] * j;
                                }
                                else
                                {
                                    blockCoords[0] = (mY * M + i) / blockSize[0];
                                    blockCoords[1] = (mX * N + j) / blockSize[1];
                                    blockij        = blockCoords[1] + (strides[0] / blockSize[1]) * blockCoords[0];
                                    if (m_data.addrMethod == ADDR_DECODE)
                                    {
                                        blockij *= blockSize[0] * blockSize[1];
                                        blockij += (i % blockSize[0]) * blockSize[1] + (j % blockSize[1]);
                                    }
                                    ij = mX * N + j + strides[0] * mY * M + loadStrides[0] * i;
                                }

                                uint32_t ref = getDataInt(ptrs[0], dataTypes[0], blockij);

                                if (m_data.addrMethod == ADDR_DECODE)
                                {
                                    ref += (2 * blockCoords[0] + blockCoords[1]) & 3;
                                }

                                uint32_t Dij = getDataInt(ptrs[3], dataTypes[3], ij);

                                if (m_data.testType == TT_NEGATE)
                                {
                                    ref = -(int32_t)ref;
                                }
                                else
                                {
                                    DE_ASSERT(0);
                                }

                                if ((ref & mask) != (Dij & mask))
                                {
                                    res = QP_TEST_RESULT_FAIL;
                                }
                            }
                        }
                    }
                }
            }
            else if (!isMatrixMulAddOp(m_data.testType))
            {
                for (uint32_t i = 0; i < totalElements[3]; ++i)
                {
                    uint32_t inputA = getDataInt(ptrs[0], dataTypes[0], i);
                    uint32_t inputB = getDataInt(ptrs[1], dataTypes[1], i);
                    uint32_t output = getDataInt(ptrs[3], dataTypes[3], i);
                    int resultSize  = componentTypeInfo.at(dataTypes[3]).bits;
                    uint32_t mask   = resultSize == 32 ? ~0 : ((1 << resultSize) - 1);
                    switch (m_data.testType)
                    {
                    case TT_LENGTH:
                        if (output < 1 || output > N * M)
                            res = QP_TEST_RESULT_FAIL;
                        if (m_data.scope == VK_SCOPE_SUBGROUP_KHR)
                        {
                            // We expect the matrix to be spread evenly across invocations, it is
                            // surprising (but not necessarily illegal) if not
                            if (output != N * M / subgroupSize && res == QP_TEST_RESULT_PASS)
                            {
                                res = QP_TEST_RESULT_QUALITY_WARNING;
                            }
                        }
                        break;
                    case TT_CONSTANT:
                        if (output != 1)
                            res = QP_TEST_RESULT_FAIL;
                        break;
                    case TT_COMPOSITE:
                    case TT_ADD:
                        if ((output & mask) != ((inputA + inputB) & mask))
                        {
                            res = QP_TEST_RESULT_FAIL;
                        }
                        break;
                    case TT_COMPOSITE_ARRAY:
                    case TT_COMPOSITE_RVALUE:
                        if ((output & mask) != (inputA & mask))
                            res = QP_TEST_RESULT_FAIL;
                        break;
                    case TT_SUB:
                        if ((output & mask) != ((inputA - inputB) & mask))
                            res = QP_TEST_RESULT_FAIL;
                        break;
                    case TT_DIV:
                    {
                        if (isSIntType(dataTypes[3]))
                        {
                            // Assert conditions that lead to undefined behaviour.
                            DE_ASSERT(inputB != 0);
                            DE_ASSERT(!((inputA & mask) == (uint32_t)(1 << (resultSize - 1)) &&
                                        (inputB & mask) == (uint32_t)((1 << resultSize) - 1)));

                            if (((int32_t)output & mask) != (((int32_t)inputA / (int32_t)inputB) & mask))
                                res = QP_TEST_RESULT_FAIL;
                        }
                        else
                        {
                            if (inputB != 0 && output != inputA / inputB)
                                res = QP_TEST_RESULT_FAIL;
                        }
                    }
                    break;
                    case TT_MUL:
                    {
                        if (((int32_t)output & mask) != (((int32_t)inputA * (int32_t)inputB) & mask))
                        {
                            res = QP_TEST_RESULT_FAIL;
                        }

                        break;
                    }
                    case TT_NEGATE:
                    case TT_FUNC:
                    case TT_FUNC_CONST_IN:
                        if ((output & mask) != ((-(int32_t)inputA) & mask))
                            res = QP_TEST_RESULT_FAIL;
                        break;
                    case TT_MATRIXTIMESSCALAR:
                        if ((output & mask) != ((6 * inputA) & mask))
                        {
                            res = QP_TEST_RESULT_FAIL;
                        }
                        break;
                    case TT_MULTICOMPONENT_LOAD:
                    {
                        if (output != inputA)
                            res = QP_TEST_RESULT_FAIL;
                        break;
                    }
                    case TT_CONVERT_ACC_TO_A:
                    case TT_CONVERT_ACC_TO_B:
                    case TT_MULTICOMPONENT_SAVE:
                    {
                        if ((output & mask) != (inputA & mask))
                        {
                            //printf("fail ");
                            res = QP_TEST_RESULT_FAIL;
                        }
                        //printf("i %d inputA %d output %d\n", i, inputA, output);
                        break;
                    }
                    case TT_PER_ELEMENT_OP:
                    case TT_PER_ELEMENT_OP_STRUCT:
                        if ((output & mask) != ((inputA + 2) & mask))
                        {
                            res = QP_TEST_RESULT_FAIL;
                        }
                        break;
                    case TT_PER_ELEMENT_OP_MAT:
                        if ((output & mask) != ((inputA * 3) & mask))
                        {
                            res = QP_TEST_RESULT_FAIL;
                        }
                        break;
                    default:
                        TCU_THROW(InternalError, "Unimplemented");
                    }
                }
            }
            else
            {
                uint32_t ik, kj, ij;
                uint32_t numMatrixX = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsX :
                                          (m_data.subgroupsPerWorkgroupX * m_data.workgroupsX);
                uint32_t numMatrixY = (m_data.scope == VK_SCOPE_WORKGROUP_KHR) ?
                                          m_data.workgroupsY :
                                          (m_data.subgroupsPerWorkgroupY * m_data.workgroupsY);
                for (uint32_t mX = 0; mX < numMatrixX; ++mX)
                {
                    for (uint32_t mY = 0; mY < numMatrixY; ++mY)
                    {
                        for (uint32_t i = 0; i < M; ++i)
                        {
                            for (uint32_t j = 0; j < N; ++j)
                            {
                                uint32_t ref = 0;

                                for (uint32_t k = 0; k < K; ++k)
                                {
                                    if (m_data.colMajor)
                                        ik = mX * M + i + strides[0] * mY * K + loadStrides[0] * k;
                                    else
                                        ik = mX * K + k + strides[0] * mY * M + loadStrides[0] * i;

                                    uint32_t Aik = getDataInt(ptrs[0], dataTypes[0], ik);

                                    if (m_data.colMajor)
                                        kj = mX * K + k + strides[1] * mY * N + loadStrides[1] * j;
                                    else
                                        kj = mX * N + j + strides[1] * mY * K + loadStrides[1] * k;

                                    uint32_t Bkj = getDataInt(ptrs[1], dataTypes[1], kj);

                                    ref += Aik * Bkj;
                                }

                                if (m_data.colMajor)
                                    ij = mX * M + i + strides[2] * mY * N + loadStrides[2] * j;
                                else
                                    ij = mX * N + j + strides[2] * mY * M + loadStrides[2] * i;

                                uint32_t Cij = getDataInt(ptrs[2], dataTypes[2], ij);

                                if (saturated)
                                {
                                    ref = satAddData(dataTypes[2], ref, Cij);
                                }
                                else
                                {
                                    ref += Cij;
                                    // truncate the result to the size of C's type.
                                    uint32_t bits = componentTypeInfo.at(dataTypes[3]).bits;
                                    uint32_t mask = (bits == 32) ? 0xFFFFFFFFU : ((1U << bits) - 1U);
                                    ref &= mask;
                                }

                                // When loading with stride 0, ij for matrix D is different from matrix C
                                if (m_data.colMajor)
                                    ij = mX * M + i + strides[2] * (mY * N + j);
                                else
                                    ij = mX * N + j + strides[2] * (mY * M + i);

                                uint32_t Dij = getDataInt(ptrs[3], dataTypes[3], ij);

                                if (ref != Dij)
                                {
                                    res = QP_TEST_RESULT_FAIL;
                                }
                            }
                        }
                    }
                }
            }
        }

        if (res != QP_TEST_RESULT_PASS)
        {
            finalres = res;

            log << tcu::TestLog::Message << "failed with M = " << M << ", N = " << N << ", K = " << K
                << ", WG = " << testSize.workgroupSize << tcu::TestLog::EndMessage;

#ifdef COOPERATIVE_MATRIX_EXTENDED_DEBUG
            for (int i = 0; i < 4; i++)
            {
                const char *matrixNames[] = {"A", "B", "C", "D"};

                log << tcu::TestLog::Message << "Matrix " << matrixNames[i]
                    << "[rows=" << m_data.subgroupsPerWorkgroupY * m_data.workgroupsY * dims[i].rows
                    << ", cols=" << m_data.subgroupsPerWorkgroupX * m_data.workgroupsX * dims[i].cols << "]:\n"
                    << dumpWholeMatrix(ptrs[i], dataTypes[i], m_data.colMajor, totalElements[i], strides[i])
                    << tcu::TestLog::EndMessage;
            }
#endif
        }
        else
        {
            if (finalres == QP_TEST_RESULT_NOT_SUPPORTED)
                finalres = res;
        }
    }

    return tcu::TestStatus(finalres, qpGetTestResultName(finalres));
}

const char *getUseType(UseType useType)
{
    switch (useType)
    {
    case UT_NV:
        return "nv";
    case UT_KHR_A:
        return "khr_a";
    case UT_KHR_B:
        return "khr_b";
    case UT_KHR_C:
        return "khr_c";
    case UT_KHR_Result:
        return "khr_r";
    default:
        TCU_THROW(InternalError, "Unknown use type");
    }
}

tcu::TestCaseGroup *createCooperativeMatrixTestsInternal(
    tcu::TestContext &testCtx, vk::ComputePipelineConstructionType computePipelineConstructionType,
    const UseType useType)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, getUseType(useType)));

    typedef struct
    {
        uint32_t value;
        const char *name;
    } TestGroupCase;

    struct DataTypeTestGroupCase
    {
        VkComponentTypeKHR value[2];
        const char *name;
        uint32_t inComponentCount  = 1u;
        uint32_t outComponentCount = 1u;
    };

    typedef struct
    {
        SubgroupSizeMode value;
        const char *name;
    } SubGroubSizes;

    typedef struct
    {
        const char *name;
        const char *description;
        uint32_t componentCount;
    } MulticomponentTypes;

    typedef struct
    {
        const char *name;
        const char *description;
        TestType testType;
    } IOTypes;

    TestGroupCase ttCases[] = {
        // OpCooperativeMatrixLength
        {TT_LENGTH, "length"},
        // OpConstantComposite
        {TT_CONSTANT, "constant"},
        // OpCompositeConstruct
        {TT_COMPOSITE, "composite"},
        // OpCompositeExtract
        {TT_COMPOSITE_RVALUE, "composite_rvalue"},
        // OpFAdd/OpIAdd
        {TT_ADD, "add"},
        // OpFSub/OpISub
        {TT_SUB, "sub"},
        // OpFDiv/OpSDiv/OpUDiv
        {TT_DIV, "div"},
        // OpFMul/OpIMul
        {TT_MUL, "mul"},
        // OpFNegate/OpSNegate
        {TT_NEGATE, "negate"},
        // OpMatrixTimesScalar
        {TT_MATRIXTIMESSCALAR, "matrixtimesscalar"},
        // OpFunctionParameter (pass by pointer)
        {TT_FUNC, "func"},
        // OpFunctionParameter (pass by value)
        {TT_FUNC_CONST_IN, "func_const_in"},
        // OpCooperativeMatrixMulAdd
        {TT_MATRIXMULADD, "matrixmuladd"},
        // OpCompositeConstruct w/array
        {TT_COMPOSITE_ARRAY, "composite_array"},
        // OpCooperativeMatrixMulAdd w/array
        {TT_MATRIXMULADD_ARRAY, "matrixmuladd_array"},
        // OpCooperativeMatrixMulAdd w/saturations
        {TT_MATRIXMULADD_SATURATED, "matrixmuladd_saturated"},
        // OpCooperativeMatrixMulAdd w/wrapping
        {TT_MATRIXMULADD_WRAPPING, "matrixmuladd_wrapping"},
        // OpCooperativeMatrixMulAdd w/stride==0
        {TT_MATRIXMULADD_STRIDE0, "matrixmuladd_stride0"},
        // OpCooperativeMatrixMulAdd
        {TT_MATRIXMULADD_CROSS, "matrixmuladd_cross"},
        // OpCooperativeMatrixMulAdd w/decode
        {TT_MATRIXMULADD_DEQUANT, "matrixmuladd_dequant"},
        // OpCooperativeMatrixMulAdd
        {TT_MATRIXMULADD_PUSH_CONSTANTS, "matrixmuladd_push"},
        // OpConvertCooperativeMatrixNV
        {TT_CONVERT_ACC_TO_A, "convert_acc_to_a"},
        {TT_CONVERT_ACC_TO_B, "convert_acc_to_b"},
        // OpTransposeCooperativeMatrixNV
        {TT_TRANSPOSE_ACC_TO_B, "transpose_acc_to_b"},
        // OpCooperativeMatrixReduceNV
        {TT_REDUCE_SUM_ROW, "reduce_sum_row"},
        {TT_REDUCE_SUM_COL, "reduce_sum_col"},
        {TT_REDUCE_SUM_ROWCOL, "reduce_sum_rowcol"},
        {TT_REDUCE_SUM_2X2, "reduce_sum_2x2"},
        {TT_REDUCE_SUM_ROW_CHANGEDIM, "reduce_sum_row_changedim"},
        {TT_REDUCE_SUM_COL_CHANGEDIM, "reduce_sum_col_changedim"},
        {TT_REDUCE_SUM_ROWCOL_CHANGEDIM, "reduce_sum_rowcol_changedim"},
        {TT_REDUCE_MIN_ROW, "reduce_min_row"},
        {TT_REDUCE_MIN_COL, "reduce_min_col"},
        {TT_REDUCE_MIN_ROWCOL, "reduce_min_rowcol"},
        {TT_REDUCE_MIN_2X2, "reduce_min_2x2"},

        {TT_PER_ELEMENT_OP, "per_element_op"},
        {TT_PER_ELEMENT_OP_ROW_COL, "per_element_op_row_col"},
        {TT_PER_ELEMENT_OP_STRUCT, "per_element_op_struct"},
        {TT_PER_ELEMENT_OP_MAT, "per_element_op_mat"},

        {TT_TENSORLAYOUT_1D, "tensorlayout1d"},
        {TT_TENSORLAYOUT_2D, "tensorlayout2d"},
        {TT_TENSORLAYOUT_3D, "tensorlayout3d"},
        {TT_TENSORLAYOUT_4D, "tensorlayout4d"},
        {TT_TENSORLAYOUT_5D, "tensorlayout5d"},
        {TT_TENSORLAYOUT_1D_CLIP, "tensorlayout1dclip"},
        {TT_TENSORLAYOUT_2D_CLIP, "tensorlayout2dclip"},
        {TT_TENSORLAYOUT_3D_CLIP, "tensorlayout3dclip"},
        {TT_TENSORLAYOUT_4D_CLIP, "tensorlayout4dclip"},
        {TT_TENSORLAYOUT_5D_CLIP, "tensorlayout5dclip"},
        {TT_SPACETODEPTH, "spacetodepth"},

        {TT_CLAMPCONSTANT, "clampconstant"},
        {TT_CLAMPTOEDGE, "clamptoedge"},
        {TT_CLAMPREPEAT, "clamprepeat"},
        {TT_CLAMPMIRRORREPEAT, "clampmirrorrepeat"},
    };
    const std::vector<DataTypeTestGroupCase> dtCases{
        // A/B are fp32 C/D are fp32
        {{VK_COMPONENT_TYPE_FLOAT32_KHR, VK_COMPONENT_TYPE_FLOAT32_KHR}, "float32_float32"},
        // A/B are fp32 C/D are fp16
        {{VK_COMPONENT_TYPE_FLOAT32_KHR, VK_COMPONENT_TYPE_FLOAT16_KHR}, "float32_float16"},
        // A/B are fp16 C/D are fp32
        {{VK_COMPONENT_TYPE_FLOAT16_KHR, VK_COMPONENT_TYPE_FLOAT32_KHR}, "float16_float32"},
        // A/B are fp16 C/D are fp16
        {{VK_COMPONENT_TYPE_FLOAT16_KHR, VK_COMPONENT_TYPE_FLOAT16_KHR}, "float16_float16"},
        // A/B are u8 C/D are u8
        {{VK_COMPONENT_TYPE_UINT8_KHR, VK_COMPONENT_TYPE_UINT8_KHR}, "uint8_uint8"},
        // A/B are u8 C/D are u32
        {{VK_COMPONENT_TYPE_UINT8_KHR, VK_COMPONENT_TYPE_UINT32_KHR}, "uint8_uint32"},
        // A/B are s8 C/D are s8
        {{VK_COMPONENT_TYPE_SINT8_KHR, VK_COMPONENT_TYPE_SINT8_KHR}, "sint8_sint8"},
        // A/B are s8 C/D are s32
        {{VK_COMPONENT_TYPE_SINT8_KHR, VK_COMPONENT_TYPE_SINT32_KHR}, "sint8_sint32"},
        // A/B are u8 C/D are s32
        {{VK_COMPONENT_TYPE_UINT8_KHR, VK_COMPONENT_TYPE_SINT32_KHR}, "uint8_sint32"},
        // A/B are u32 C/D are u32
        {{VK_COMPONENT_TYPE_UINT32_KHR, VK_COMPONENT_TYPE_UINT32_KHR}, "uint32_uint32"},
        // A/B are u32 C/D are u8
        {{VK_COMPONENT_TYPE_UINT32_KHR, VK_COMPONENT_TYPE_UINT8_KHR}, "uint32_uint8"},
        // A/B are s32 C/D are s32
        {{VK_COMPONENT_TYPE_SINT32_KHR, VK_COMPONENT_TYPE_SINT32_KHR}, "sint32_sint32"},
        // A/B are s32 C/D are s8
        {{VK_COMPONENT_TYPE_SINT32_KHR, VK_COMPONENT_TYPE_SINT8_KHR}, "sint32_sint8"},
#ifndef CTS_USES_VULKANSC
#ifdef SIMULATE_BFLOAT16
#define SIM_COMPONENT_TYPE_BFLOAT16_KHR VK_COMPONENT_TYPE_FLOAT16_KHR
#else
#define SIM_COMPONENT_TYPE_BFLOAT16_KHR VK_COMPONENT_TYPE_BFLOAT16_KHR
#endif
        // A/B are bfp16 C/D are fp32
        {{SIM_COMPONENT_TYPE_BFLOAT16_KHR, VK_COMPONENT_TYPE_FLOAT32_KHR}, "bfloat16_float32"},
        // A/B are bfp16 C/D are bfp16
        {{SIM_COMPONENT_TYPE_BFLOAT16_KHR, SIM_COMPONENT_TYPE_BFLOAT16_KHR}, "bfloat16_bfloat16"},
        {{VK_COMPONENT_TYPE_FLOAT32_KHR, SIM_COMPONENT_TYPE_BFLOAT16_KHR}, "float32_bfloat16"},

        {{VK_COMPONENT_TYPE_FLOAT_E5M2_NV, VK_COMPONENT_TYPE_FLOAT_E5M2_NV}, "floate5m2_floate5m2"},
        {{VK_COMPONENT_TYPE_FLOAT_E4M3_NV, VK_COMPONENT_TYPE_FLOAT_E4M3_NV}, "floate4m3_floate4m3"},
        {{VK_COMPONENT_TYPE_FLOAT_E5M2_NV, VK_COMPONENT_TYPE_FLOAT16_KHR}, "floate5m2_float16"},
        {{VK_COMPONENT_TYPE_FLOAT_E4M3_NV, VK_COMPONENT_TYPE_FLOAT16_KHR}, "floate4m3_float16"},
        {{VK_COMPONENT_TYPE_FLOAT_E5M2_NV, VK_COMPONENT_TYPE_FLOAT32_KHR}, "floate5m2_float32"},
        {{VK_COMPONENT_TYPE_FLOAT_E4M3_NV, VK_COMPONENT_TYPE_FLOAT32_KHR}, "floate4m3_float32"},
        {{VK_COMPONENT_TYPE_FLOAT16_KHR, VK_COMPONENT_TYPE_FLOAT_E5M2_NV}, "float16_floate5m2"},
        {{VK_COMPONENT_TYPE_FLOAT16_KHR, VK_COMPONENT_TYPE_FLOAT_E4M3_NV}, "float16_floate4m3"},
        {{VK_COMPONENT_TYPE_FLOAT32_KHR, VK_COMPONENT_TYPE_FLOAT_E5M2_NV}, "float32_floate5m2"},
        {{VK_COMPONENT_TYPE_FLOAT32_KHR, VK_COMPONENT_TYPE_FLOAT_E4M3_NV}, "float32_floate4m3"},
#endif
    };

    SubGroubSizes sgsCases[] = {
        // Default subgroup size
        {SUBGROUP_SIZE_NONE, ""},
        // Minimum subgroup size
        {SUBGROUP_SIZE_MIN, "_min"},
        // Maximum subgroup size
        {SUBGROUP_SIZE_MAX, "_max"},
    };

    TestGroupCase colCases[] = {
        {0, "rowmajor"},
        {1, "colmajor"},
    };

    TestGroupCase addrCases[] = {
        {ADDR_LINEAR, "linear"},
        {ADDR_TENSORLAYOUT, "tensorlayout"},
        {ADDR_BLOCKSIZE, "blocksize"},
        {ADDR_DECODE, "decode"},
    };

    TestGroupCase scopeCases[] = {
        {VK_SCOPE_SUBGROUP_KHR, "subgroupscope"},
        {VK_SCOPE_WORKGROUP_KHR, "workgroupscope"},
    };

    TestGroupCase scCases[] = {
        // SSBO
        {SC_BUFFER, "buffer"},
        // shared memory
        {SC_WORKGROUP, "workgroup"},
        // SSBO w/variable pointers
        {SC_BUFFER_VARIABLE_POINTERS, "buffer_varptr"},
        // shared memory w/variable pointers
        {SC_WORKGROUP_VARIABLE_POINTERS, "workgroup_varptr"},
        // physical_storage_buffer
        {SC_PHYSICAL_STORAGE_BUFFER, "physical_buffer"},
    };

    // Types tested for conversions. Excludes 64b types.
    VkComponentTypeKHR allTypes[] = {
        VK_COMPONENT_TYPE_FLOAT16_KHR,   VK_COMPONENT_TYPE_FLOAT32_KHR, VK_COMPONENT_TYPE_SINT8_KHR,
        VK_COMPONENT_TYPE_SINT16_KHR,    VK_COMPONENT_TYPE_SINT32_KHR,  VK_COMPONENT_TYPE_UINT8_KHR,
        VK_COMPONENT_TYPE_UINT16_KHR,    VK_COMPONENT_TYPE_UINT32_KHR,  VK_COMPONENT_TYPE_FLOAT_E5M2_NV,
        VK_COMPONENT_TYPE_FLOAT_E4M3_NV,
    };

    // Types tested for load/store from/into multicomponent types
    MulticomponentTypes multicomponentTypes[] = {
        {"vec2", "2-component vector type as input or output", 2},
        {"vec4", "4-component vector type as input or output", 4},
    };

    // Types tested for load/store from/into multicomponent types
    IOTypes ioTypes[] = {
        {"load", "Test multicomponent type as input in load operation", TT_MULTICOMPONENT_LOAD},
        {"save", "Test multicomponent type as output in store operation", TT_MULTICOMPONENT_SAVE},
    };

    for (int scopeNdx = 0; scopeNdx < DE_LENGTH_OF_ARRAY(scopeCases); scopeNdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> scopeGroup(new tcu::TestCaseGroup(testCtx, scopeCases[scopeNdx].name));
        if (useType == UT_NV && scopeCases[scopeNdx].value == VK_SCOPE_WORKGROUP_KHR)
        {
            continue;
        }

        for (int ttNdx = 0; ttNdx < DE_LENGTH_OF_ARRAY(ttCases); ttNdx++)
        {
            const TestType testType = (TestType)ttCases[ttNdx].value;

            for (int sgsNdx = 0; sgsNdx < DE_LENGTH_OF_ARRAY(sgsCases); sgsNdx++)
            {
                if (testType != TT_MATRIXMULADD && sgsCases[sgsNdx].value != SUBGROUP_SIZE_NONE)
                    continue;

                if (testType == TT_MATRIXMULADD && sgsCases[sgsNdx].value != SUBGROUP_SIZE_NONE && useType == UT_NV)
                    continue;

                const string name = string(ttCases[ttNdx].name) + sgsCases[sgsNdx].name;
                de::MovePtr<tcu::TestCaseGroup> ttGroup(new tcu::TestCaseGroup(testCtx, name.c_str()));

                for (auto idtCaseBegin = dtCases.begin(), idtCase = idtCaseBegin; idtCase != dtCases.end(); ++idtCase)
                {
                    const int dtNdx = (int)std::distance(idtCaseBegin, idtCase);

                    de::MovePtr<tcu::TestCaseGroup> dtGroup(new tcu::TestCaseGroup(testCtx, dtCases[dtNdx].name));
                    for (int scNdx = 0; scNdx < DE_LENGTH_OF_ARRAY(scCases); scNdx++)
                    {
                        de::MovePtr<tcu::TestCaseGroup> scGroup(new tcu::TestCaseGroup(testCtx, scCases[scNdx].name));
                        for (int colNdx = 0; colNdx < DE_LENGTH_OF_ARRAY(colCases); colNdx++)
                        {
                            de::MovePtr<tcu::TestCaseGroup> colGroup(
                                new tcu::TestCaseGroup(testCtx, colCases[colNdx].name));
                            for (int addrNdx = 0; addrNdx < DE_LENGTH_OF_ARRAY(addrCases); addrNdx++)
                            {
                                const VkComponentTypeKHR inputType  = dtCases[dtNdx].value[0];
                                const VkComponentTypeKHR outputType = dtCases[dtNdx].value[1];
                                const uint32_t inComponentCount     = dtCases[dtNdx].inComponentCount;
                                const uint32_t outComponentCount    = dtCases[dtNdx].outComponentCount;
                                const bool isMatrixMul              = isMatrixMulAddOp(testType);

                                if (testType == TT_MATRIXMULADD_CROSS)
                                {
                                    if (isFloatType(inputType) || isFloatType(outputType) || useType == UT_NV ||
                                        scCases[scNdx].value != SC_BUFFER)
                                        continue;

                                    // handwritten spir-v would need to be ported
                                    if (scopeCases[scopeNdx].value == VK_SCOPE_WORKGROUP_KHR)
                                        continue;
                                }
                                else
                                {
                                    // Rest of tests do not run on matrix C
                                    if (useType == UT_KHR_C)
                                    {
                                        continue;
                                    }

                                    // useType isn't used for matrixmul shaders. Don't generate 3 copies of those tests.
                                    if (isMatrixMul && (useType == UT_KHR_A || useType == UT_KHR_B))
                                    {
                                        continue;
                                    }

                                    // NV extension doesn't support mixing signedness
                                    if (isMatrixMul && (useType == UT_NV) &&
                                        isSIntType(inputType) != isSIntType(outputType))
                                    {
                                        continue;
                                    }

                                    if (isMatrixMul &&
                                        componentTypeInfo.at(inputType).bits > componentTypeInfo.at(outputType).bits)
                                        continue;

                                    if (inputType == VK_COMPONENT_TYPE_BFLOAT16_KHR ||
                                        outputType == VK_COMPONENT_TYPE_BFLOAT16_KHR ||
                                        inputType == VK_COMPONENT_TYPE_FLOAT_E5M2_NV ||
                                        outputType == VK_COMPONENT_TYPE_FLOAT_E5M2_NV ||
                                        inputType == VK_COMPONENT_TYPE_FLOAT_E4M3_NV ||
                                        outputType == VK_COMPONENT_TYPE_FLOAT_E4M3_NV)
                                    {
                                        if (useType == UT_NV)
                                            continue;

                                        if (isArithmeticTest(testType))
                                            continue;
                                    }
                                }

                                if (testType == TT_MATRIXMULADD_DEQUANT)
                                {
                                    if (inputType != VK_COMPONENT_TYPE_FLOAT16_KHR
#ifndef CTS_USES_VULKANSC
                                        && inputType != VK_COMPONENT_TYPE_BFLOAT16_KHR &&
                                        inputType != VK_COMPONENT_TYPE_FLOAT_E5M2_NV &&
                                        inputType != VK_COMPONENT_TYPE_FLOAT_E4M3_NV
#endif
                                    )
                                    {
                                        continue;
                                    }
                                    if (addrCases[addrNdx].value != ADDR_DECODE)
                                    {
                                        continue;
                                    }
                                    if (colCases[colNdx].value)
                                    {
                                        // row major only, for now
                                        continue;
                                    }
                                }

                                if ((addrCases[addrNdx].value == ADDR_BLOCKSIZE ||
                                     addrCases[addrNdx].value == ADDR_DECODE) &&
                                    testType != TT_NEGATE && testType != TT_MATRIXMULADD_DEQUANT)
                                {
                                    // only certain tests ported to handle blocksize
                                    continue;
                                }

                                if ((addrCases[addrNdx].value == ADDR_BLOCKSIZE ||
                                     addrCases[addrNdx].value == ADDR_DECODE) &&
                                    (scCases[scNdx].value == SC_WORKGROUP ||
                                     scCases[scNdx].value == SC_WORKGROUP_VARIABLE_POINTERS))
                                {
                                    // copying into shared memory not ported to handle block size
                                    continue;
                                }

                                if (!isMatrixMul && testType != TT_CONVERT_ACC_TO_A &&
                                    testType != TT_CONVERT_ACC_TO_B && testType != TT_TRANSPOSE_ACC_TO_B &&
                                    inputType != outputType)
                                    continue;

                                if (testType == TT_MUL && useType == UT_NV)
                                    continue;

                                if (testType == TT_MATRIXMULADD_SATURATED &&
                                    (isFloatType(inputType) || useType == UT_NV))
                                    continue;

                                if (testType == TT_MATRIXMULADD_WRAPPING &&
                                    (isFloatType(inputType) || useType == UT_NV))
                                    continue;

                                if (testType == TT_MATRIXMULADD_STRIDE0 && useType == UT_NV)
                                    continue;

                                if (testType == TT_LENGTH && useType != UT_NV &&
                                    (outputType == VK_COMPONENT_TYPE_SINT8_KHR ||
                                     outputType == VK_COMPONENT_TYPE_UINT8_KHR))
                                    continue;

                                if (testType == TT_MATRIXMULADD_PUSH_CONSTANTS &&
                                    addrCases[addrNdx].value != ADDR_LINEAR)
                                    continue;

                                if (useType == UT_NV && (addrCases[addrNdx].value != ADDR_LINEAR ||
                                                         isReduceOp(testType) || isPerElemOp(testType)))
                                {
                                    continue;
                                }

                                if ((testType == TT_CONVERT_ACC_TO_A || testType == TT_CONVERT_ACC_TO_B ||
                                     testType == TT_TRANSPOSE_ACC_TO_B) &&
                                    useType != UT_KHR_Result)
                                {
                                    // These tests hardcode the use, no need to repeat them three times
                                    continue;
                                }

                                if (isReduceOp(testType) && (useType == UT_KHR_A || useType == UT_KHR_B))
                                {
                                    continue;
                                }

                                if (isReduceOp(testType) && inputType != outputType)
                                {
                                    continue;
                                }

                                if (isTensorLayoutTest(testType) &&
                                    (colCases[colNdx].value || scCases[scNdx].value == SC_WORKGROUP ||
                                     scCases[scNdx].value == SC_WORKGROUP_VARIABLE_POINTERS ||
                                     scCases[scNdx].value == SC_PHYSICAL_STORAGE_BUFFER ||
                                     addrCases[addrNdx].value == ADDR_LINEAR))
                                {
                                    continue;
                                }

                                if ((scCases[scNdx].value == SC_BUFFER_VARIABLE_POINTERS ||
                                     scCases[scNdx].value == SC_WORKGROUP_VARIABLE_POINTERS) &&
                                    (!(testType == TT_MATRIXMULADD || testType == TT_MUL) ||
                                     sgsCases[sgsNdx].value != SUBGROUP_SIZE_NONE))
                                {
                                    // trim test count
                                    continue;
                                }

                                if (colCases[colNdx].value && !(isMatrixMul || testType == TT_MUL))
                                {
                                    // trim test count
                                    continue;
                                }

                                if (scCases[scNdx].value == SC_WORKGROUP ||
                                    scCases[scNdx].value == SC_WORKGROUP_VARIABLE_POINTERS ||
                                    addrCases[addrNdx].value == ADDR_LINEAR)
                                {
                                    if (isClampTest(testType))
                                    {
                                        continue;
                                    }
                                }

                                uint32_t workgroupsX = 4u;
                                uint32_t workgroupsY = 4u;

                                uint32_t subgroupsPerWorkgroupX = 2;
                                uint32_t subgroupsPerWorkgroupY = 2;

                                // The program is meant to be run once
                                if (isTensorLayoutTest(testType))
                                {
                                    subgroupsPerWorkgroupX = 1;
                                    subgroupsPerWorkgroupY = 1;
                                    workgroupsX            = 1u;
                                    workgroupsY            = 1u;
                                }

                                CaseDef c = {
                                    testType, //  TestType testtype;
                                    (VkScopeKHR)scopeCases[scopeNdx]
                                        .value,                           //  VkScopeKHR                          scope;
                                    subgroupsPerWorkgroupX,               //  uint32_t subgroupsPerWorkgroupX;
                                    subgroupsPerWorkgroupY,               //  uint32_t subgroupsPerWorkgroupY;
                                    workgroupsX,                          //  uint32_t workgroupsX;
                                    workgroupsY,                          //  uint32_t workgroupsY;
                                    inputType,                            //  VkComponentTypeKHR inputType;
                                    outputType,                           //  VkComponentTypeKHR outputType;
                                    !!colCases[colNdx].value,             //  bool colMajor;
                                    (AddrMethod)addrCases[addrNdx].value, //  AddrMethod addrMethod;
                                    (StorageClass)scCases[scNdx].value,   //  StorageClass storageClass;
                                    useType,                              //  UseType useType;
                                    sgsCases[sgsNdx].value,               //  SubgroupSizeMode subgroupSizeMode;
                                    computePipelineConstructionType, //  vk::ComputePipelineConstructionType computePipelineConstructionType;
                                    inComponentCount,  //  uint32_t inputComponentCount;
                                    outComponentCount, //  uint32_t outputComponentCount;
                                };
                                colGroup->addChild(new CooperativeMatrixTestCase(testCtx, addrCases[addrNdx].name, c));
                            }
                            scGroup->addChild(colGroup.release());
                        }
                        dtGroup->addChild(scGroup.release());
                    }
                    ttGroup->addChild(dtGroup.release());
                }
                scopeGroup->addChild(ttGroup.release());
            }
        }

        if (useType != UT_KHR_C)
        {
            for (bool sat : {false, true})
            {
                const string name = string("convert") + (sat ? "_sat" : "");
                const string desc = string("OpFConvert/OpSConvert/OpUConvert/OpBitcast");
                de::MovePtr<tcu::TestCaseGroup> ttGroup(new tcu::TestCaseGroup(testCtx, name.c_str()));

                for (int dtNdx1 = 0; dtNdx1 < DE_LENGTH_OF_ARRAY(allTypes); dtNdx1++)
                {
                    for (int dtNdx2 = 0; dtNdx2 < DE_LENGTH_OF_ARRAY(allTypes); dtNdx2++)
                    {
                        const VkComponentTypeKHR inputType  = (VkComponentTypeKHR)allTypes[dtNdx1];
                        const VkComponentTypeKHR outputType = (VkComponentTypeKHR)allTypes[dtNdx2];

                        if (sat)
                        {
                            if (inputType == VK_COMPONENT_TYPE_FLOAT_E5M2_NV ||
                                inputType == VK_COMPONENT_TYPE_FLOAT_E4M3_NV)
                            {
                                continue;
                            }
                            if (outputType != VK_COMPONENT_TYPE_FLOAT_E5M2_NV &&
                                outputType != VK_COMPONENT_TYPE_FLOAT_E4M3_NV)
                            {
                                continue;
                            }
                        }

                        const string name2 = string("input_") + string(componentTypeInfo.at(inputType).typeName) +
                                             string("_output_") + string(componentTypeInfo.at(outputType).typeName);
                        de::MovePtr<tcu::TestCaseGroup> dtGroup(new tcu::TestCaseGroup(testCtx, name2.c_str()));
                        for (int scNdx = 0; scNdx < DE_LENGTH_OF_ARRAY(scCases); scNdx++)
                        {
                            de::MovePtr<tcu::TestCaseGroup> scGroup(
                                new tcu::TestCaseGroup(testCtx, scCases[scNdx].name));
                            for (int colNdx = 0; colNdx < DE_LENGTH_OF_ARRAY(colCases); colNdx++)
                            {
                                if (scCases[scNdx].value == SC_BUFFER_VARIABLE_POINTERS ||
                                    scCases[scNdx].value == SC_WORKGROUP_VARIABLE_POINTERS)
                                {
                                    // trim test count
                                    continue;
                                }

                                if (colCases[colNdx].value)
                                {
                                    // trim test count
                                    continue;
                                }

                                AddrMethod addrMethod = (scopeCases[scopeNdx].value == VK_SCOPE_WORKGROUP_KHR) ?
                                                            ADDR_TENSORLAYOUT :
                                                            ADDR_LINEAR;

                                CaseDef c = {
                                    sat ? TT_CONVERT_SAT : TT_CONVERT,      //  TestType testtype;
                                    (VkScopeKHR)scopeCases[scopeNdx].value, //  VkScopeKHR                      scope;
                                    2u,                                     //  uint32_t subgroupsPerWorkgroupX;
                                    2u,                                     //  uint32_t subgroupsPerWorkgroupY;
                                    4u,                                     //  uint32_t workgroupsX;
                                    4u,                                     //  uint32_t workgroupsY;
                                    inputType,                              //  VkComponentTypeKHR inputType;
                                    outputType,                             //  VkComponentTypeKHR outputType;
                                    !!colCases[colNdx].value,               //  bool colMajor;
                                    addrMethod,                             //  AddrMethod addrMethod;
                                    (StorageClass)scCases[scNdx].value,     //  StorageClass storageClass;
                                    useType,                                //  UseType useType;
                                    SUBGROUP_SIZE_NONE,                     //  SubgroupSizeMode subgroupSizeMode;
                                    computePipelineConstructionType, //  vk::ComputePipelineConstructionType computePipelineConstructionType;
                                    1,                               //  uint32_t inputComponentCount;
                                    1,                               //  uint32_t outputComponentCount;
                                };

                                scGroup->addChild(new CooperativeMatrixTestCase(testCtx, colCases[colNdx].name, c));
                            }
                            dtGroup->addChild(scGroup.release());
                        }
                        ttGroup->addChild(dtGroup.release());
                    }
                }
                scopeGroup->addChild(ttGroup.release());
            }
        }

        if (useType != UT_NV && useType != UT_KHR_C)
        {
            de::MovePtr<tcu::TestCaseGroup> ttGroup(
                new tcu::TestCaseGroup(testCtx, "multicomponent", "Multicomponent types tests"));
            for (int ctNdx = 0; ctNdx < DE_LENGTH_OF_ARRAY(multicomponentTypes); ctNdx++)
            {
                de::MovePtr<tcu::TestCaseGroup> ctGroup(new tcu::TestCaseGroup(testCtx, multicomponentTypes[ctNdx].name,
                                                                               multicomponentTypes[ctNdx].description));
                const uint32_t componentCount = multicomponentTypes[ctNdx].componentCount;

                for (int ioNdx = 0; ioNdx < DE_LENGTH_OF_ARRAY(ioTypes); ioNdx++)
                {
                    de::MovePtr<tcu::TestCaseGroup> ioGroup(
                        new tcu::TestCaseGroup(testCtx, ioTypes[ioNdx].name, ioTypes[ioNdx].description));
                    const TestType testType             = ioTypes[ioNdx].testType;
                    const uint32_t inputComponentCount  = testType == TT_MULTICOMPONENT_LOAD ? componentCount : 1;
                    const uint32_t outputComponentCount = testType == TT_MULTICOMPONENT_LOAD ? 1 : componentCount;

                    for (int dtNdx = 0; dtNdx < DE_LENGTH_OF_ARRAY(allTypes); dtNdx++)
                    {
                        const VkComponentTypeKHR inputType = allTypes[dtNdx];
                        const string name                  = componentTypeInfo.at(inputType).typeName;

                        de::MovePtr<tcu::TestCaseGroup> dtGroup(new tcu::TestCaseGroup(testCtx, name.c_str(), ""));
                        for (int scNdx = 0; scNdx < DE_LENGTH_OF_ARRAY(scCases); scNdx++)
                        {
                            de::MovePtr<tcu::TestCaseGroup> scGroup(
                                new tcu::TestCaseGroup(testCtx, scCases[scNdx].name, ""));
                            for (int colNdx = 0; colNdx < DE_LENGTH_OF_ARRAY(colCases); colNdx++)
                            {
                                AddrMethod addrMethod = (scopeCases[scopeNdx].value == VK_SCOPE_WORKGROUP_KHR) ?
                                                            ADDR_TENSORLAYOUT :
                                                            ADDR_LINEAR;

                                if (colCases[colNdx].value)
                                {
                                    // trim test count
                                    continue;
                                }

                                CaseDef c = {
                                    testType,                               //  TestType testtype;
                                    (VkScopeKHR)scopeCases[scopeNdx].value, //  VkScopeKHR                      scope;
                                    2u,                                     //  uint32_t subgroupsPerWorkgroupX;
                                    2u,                                     //  uint32_t subgroupsPerWorkgroupY;
                                    4u,                                     //  uint32_t workgroupsX;
                                    4u,                                     //  uint32_t workgroupsY;
                                    inputType,                              //  VkComponentTypeKHR inputType;
                                    inputType,                              //  VkComponentTypeKHR outputType;
                                    !!colCases[colNdx].value,               //  bool colMajor;
                                    addrMethod,                             //  AddrMethod addrMethod;
                                    (StorageClass)scCases[scNdx].value,     //  StorageClass storageClass;
                                    useType,                                //  UseType useType;
                                    SUBGROUP_SIZE_NONE,                     //  SubgroupSizeMode subgroupSizeMode;
                                    computePipelineConstructionType, //  vk::ComputePipelineConstructionType computePipelineConstructionType;
                                    inputComponentCount,  //  uint32_t inputComponentCount;
                                    outputComponentCount, //  uint32_t outputComponentCount;
                                };

                                scGroup->addChild(new CooperativeMatrixTestCase(testCtx, colCases[colNdx].name, c));
                            }
                            dtGroup->addChild(scGroup.release());
                        }
                        ioGroup->addChild(dtGroup.release());
                    }
                    ctGroup->addChild(ioGroup.release());
                }
                ttGroup->addChild(ctGroup.release());
            }
            scopeGroup->addChild(ttGroup.release());
        }
        group->addChild(scopeGroup.release());
    }

    return group.release();
}
} // namespace

tcu::TestCaseGroup *createCooperativeMatrixTests(tcu::TestContext &testCtx,
                                                 vk::ComputePipelineConstructionType computePipelineConstructionType)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "cooperative_matrix"));

    group->addChild(createCooperativeMatrixTestsInternal(testCtx, computePipelineConstructionType, UT_NV));
    group->addChild(createCooperativeMatrixTestsInternal(testCtx, computePipelineConstructionType, UT_KHR_A));
    group->addChild(createCooperativeMatrixTestsInternal(testCtx, computePipelineConstructionType, UT_KHR_B));
    group->addChild(createCooperativeMatrixTestsInternal(testCtx, computePipelineConstructionType, UT_KHR_C));
    group->addChild(createCooperativeMatrixTestsInternal(testCtx, computePipelineConstructionType, UT_KHR_Result));

    return group.release();
}

} // namespace compute
} // namespace vkt
