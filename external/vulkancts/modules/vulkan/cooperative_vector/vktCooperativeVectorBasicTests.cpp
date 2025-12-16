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

typedef enum
{
    TT_LENGTH = 0,
    TT_CONSTANT,
    TT_CONVERT,
    TT_COMPOSITE,
    TT_COMPOSITE_RVALUE,
    TT_VECTOR_EXTRACT,
    TT_ADD,
    TT_SUB,
    TT_MUL,
    TT_DIV,
    TT_NEGATE,
    TT_VECTORTIMESSCALAR,
    TT_FUNC,
    TT_EXP,
    TT_LOG,
    TT_TANH,
    TT_ATAN,
    TT_MIN,
    TT_MAX,
    TT_CLAMP,
    TT_STEP,
    TT_FMA,
    TT_COMPOSITE_ARRAY,
    TT_AND,
    TT_OR,
    TT_XOR,
    TT_NOT,
    TT_SHL,
    TT_SHR,
    TT_MATRIXMUL,
    TT_MATRIXMUL_TRAININGBIAS,
    TT_MATRIXMAD,
    TT_MATRIXMADTRANSPOSE,
    TT_MATRIXMUL3,
    TT_MATRIXMUL2ADD,
    TT_MATRIXMUL2ADDMUL2,
    TT_REDUCESUM,
    TT_OUTERPRODUCT,
} TestType;

static bool isMatrixMul(TestType testType)
{
    return testType == TT_MATRIXMUL || testType == TT_MATRIXMUL_TRAININGBIAS || testType == TT_MATRIXMUL3 ||
           testType == TT_MATRIXMAD || testType == TT_MATRIXMADTRANSPOSE || testType == TT_MATRIXMUL2ADD ||
           testType == TT_MATRIXMUL2ADDMUL2;
}

static bool isTraining(TestType testType)
{
    return testType == TT_REDUCESUM || testType == TT_OUTERPRODUCT;
}

static constexpr uint32_t nonuniformMatrixGroupSize = 5;
static constexpr uint32_t nonuniformBiasGroupSize   = 6;

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
    ACT_NONE          = 0,
    ACT_MUL           = 1,
    ACT_MAX           = 2,
    ACT_NONUNIF       = 3,
    ACT_DIVERGE       = 4,
    ACT_SIGMOID       = 5,
    ACT_LEAKYRELUSTEP = 6,
    ACT_LEAKYRELUMAX  = 7,
    ACT_HARDGELU      = 8,
    ACT_LOAD          = 9,
    ACT_LOAD_SHARED   = 10,
    ACT_LOAD_READONLY = 11,
} Activation;

typedef enum
{
    STAGE_COMPUTE = 0,
    STAGE_RAYGEN,
    STAGE_INTERSECT,
    STAGE_ANY_HIT,
    STAGE_CLOSEST_HIT,
    STAGE_MISS,
    STAGE_CALLABLE,
    STAGE_VERTEX,
    STAGE_FRAGMENT,
    STAGE_GEOMETRY,
    STAGE_TESS_CTRL,
    STAGE_TESS_EVAL,
    STAGE_TASK,
    STAGE_MESH,
} Stage;

typedef enum
{
    RESULT_ADDR_UNIFORM,
    RESULT_ADDR_UNIQUE,
    RESULT_ADDR_CLUSTERED,
} ResultAddress;

struct CaseDef
{
    Stage stage;
    TestType testType;
    uint32_t threadsPerWorkgroupX;
    uint32_t threadsPerWorkgroupY;
    uint32_t workgroupsX;
    uint32_t workgroupsY;
    VkComponentTypeKHR inputType;
    VkComponentTypeKHR inputInterpretation;
    VkComponentTypeKHR matrixType;
    VkComponentTypeKHR outputType;
    bool inputPacked;
    VkCooperativeVectorMatrixLayoutNV matrixLayout[3];
    bool transpose;
    StorageClass storageClass;
    uint32_t inputVectorSize;
    uint32_t outputVectorSize;
    Activation act0;
    Activation act1;
    Activation act2;
    bool nonuniformOffset;
    bool cfDivergent;
    ResultAddress resultAddr;
    bool uses64BitIndexing;
};

bool isRayTracingStageKHR(const Stage stage)
{
    switch (stage)
    {
    case STAGE_RAYGEN:
    case STAGE_INTERSECT:
    case STAGE_ANY_HIT:
    case STAGE_CLOSEST_HIT:
    case STAGE_MISS:
    case STAGE_CALLABLE:
        return true;

    default:
        return false;
    }
}

bool isMeshStage(Stage stage)
{
    return (stage == STAGE_TASK || stage == STAGE_MESH);
}

static const VkFlags ALL_RAY_TRACING_STAGES = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                                              VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
                                              VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR;

VkShaderStageFlags getAllShaderStagesFor(Stage stage)
{
    if (isRayTracingStageKHR(stage))
        return ALL_RAY_TRACING_STAGES;

    if (isMeshStage(stage))
        return (VK_SHADER_STAGE_MESH_BIT_EXT | ((stage == STAGE_TASK) ? VK_SHADER_STAGE_TASK_BIT_EXT : 0));

    return (VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_ALL_GRAPHICS);
}

VkShaderStageFlagBits getShaderStageFlag(const Stage stage)
{
    switch (stage)
    {
    case STAGE_RAYGEN:
        return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    case STAGE_ANY_HIT:
        return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    case STAGE_CLOSEST_HIT:
        return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    case STAGE_MISS:
        return VK_SHADER_STAGE_MISS_BIT_KHR;
    case STAGE_INTERSECT:
        return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
    case STAGE_CALLABLE:
        return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
    default:
        TCU_THROW(InternalError, "Unknown stage specified");
    }
}

bool usesAccelerationStructure(const Stage stage)
{
    return (isRayTracingStageKHR(stage) && stage != STAGE_RAYGEN && stage != STAGE_CALLABLE);
}

class CooperativeVectorTestInstance : public TestInstance
{
public:
    CooperativeVectorTestInstance(Context &context, const CaseDef &data);
    ~CooperativeVectorTestInstance(void);
    tcu::TestStatus iterate(void);

private:
    CaseDef m_data;
};

CooperativeVectorTestInstance::CooperativeVectorTestInstance(Context &context, const CaseDef &data)
    : vkt::TestInstance(context)
    , m_data(data)
{
}

CooperativeVectorTestInstance::~CooperativeVectorTestInstance(void)
{
}

class CooperativeVectorTestCase : public TestCase
{
public:
    CooperativeVectorTestCase(tcu::TestContext &context, const char *name, const CaseDef data);
    ~CooperativeVectorTestCase(void);
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    CaseDef m_data;
};

CooperativeVectorTestCase::CooperativeVectorTestCase(tcu::TestContext &context, const char *name, const CaseDef data)
    : vkt::TestCase(context, name)
    , m_data(data)
{
}

CooperativeVectorTestCase::~CooperativeVectorTestCase(void)
{
}

void CooperativeVectorTestCase::checkSupport(Context &context) const
{
    if (!context.contextSupports(vk::ApiVersion(0, 1, 1, 0)))
    {
        TCU_THROW(NotSupportedError, "Vulkan 1.1 not supported");
    }

    if (!context.getCooperativeVectorFeaturesNV().cooperativeVector)
    {
        TCU_THROW(NotSupportedError, "cooperativeVector not supported");
    }

#ifndef CTS_USES_VULKANSC
    if (m_data.uses64BitIndexing && !context.getShader64BitIndexingFeaturesEXT().shader64BitIndexing)
        TCU_THROW(NotSupportedError, "shader64BitIndexing not supported by this implementation");
#endif

    if (isRayTracingStageKHR(m_data.stage))
    {
        context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
        context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

        const VkPhysicalDeviceRayTracingPipelineFeaturesKHR &rayTracingPipelineFeaturesKHR =
            context.getRayTracingPipelineFeatures();
        if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == false)
            TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

        const VkPhysicalDeviceAccelerationStructureFeaturesKHR &accelerationStructureFeaturesKHR =
            context.getAccelerationStructureFeatures();
        if (accelerationStructureFeaturesKHR.accelerationStructure == false)
            TCU_THROW(TestError, "VK_KHR_ray_tracing_pipeline requires "
                                 "VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");
    }

    if (isMeshStage(m_data.stage))
    {
        const auto &meshFeatures = context.getMeshShaderFeaturesEXT();

        if (!meshFeatures.meshShader)
            TCU_THROW(NotSupportedError, "Mesh shaders not supported");

        if (m_data.stage == STAGE_TASK && !meshFeatures.taskShader)
            TCU_THROW(NotSupportedError, "Task shaders not supported");
    }

    if ((m_data.storageClass == SC_WORKGROUP_VARIABLE_POINTERS || m_data.storageClass == SC_BUFFER_VARIABLE_POINTERS) &&
        !context.getVariablePointersFeatures().variablePointers)
    {
        TCU_THROW(NotSupportedError, "variable pointers not supported");
    }

    if (!context.isBufferDeviceAddressSupported())
    {
        TCU_THROW(NotSupportedError, "buffer device address not supported");
    }

    if (!context.getShaderFloat16Int8Features().shaderFloat16 &&
        (m_data.inputType == VK_COMPONENT_TYPE_FLOAT16_KHR || m_data.matrixType == VK_COMPONENT_TYPE_FLOAT16_KHR ||
         m_data.outputType == VK_COMPONENT_TYPE_FLOAT16_KHR))
    {
        TCU_THROW(NotSupportedError, "shaderFloat16 not supported");
    }

    if (isTraining(m_data.testType) && !context.getCooperativeVectorFeaturesNV().cooperativeVectorTraining)
        TCU_THROW(NotSupportedError, "Training not supported");

    if (m_data.testType == TT_OUTERPRODUCT || m_data.testType == TT_REDUCESUM)
    {
        if (m_data.matrixType == VK_COMPONENT_TYPE_FLOAT16_KHR &&
            !context.getCooperativeVectorPropertiesNV().cooperativeVectorTrainingFloat16Accumulation)
            TCU_THROW(NotSupportedError, "cooperativeVectorTrainingFloat16Accumulation not supported");

        if (m_data.matrixType == VK_COMPONENT_TYPE_FLOAT32_KHR &&
            !context.getCooperativeVectorPropertiesNV().cooperativeVectorTrainingFloat32Accumulation)
            TCU_THROW(NotSupportedError, "cooperativeVectorTrainingFloat32Accumulation not supported");
    }

    uint32_t propertyCount = 0;
    std::vector<VkCooperativeVectorPropertiesNV> properties;
    context.getInstanceInterface().getPhysicalDeviceCooperativeVectorPropertiesNV(context.getPhysicalDevice(),
                                                                                  &propertyCount, nullptr);
    if (propertyCount == 0)
        TCU_THROW(NotSupportedError, "cooperative vectors not supported");

    bool supported[2] = {false, false};
    properties.resize(propertyCount);

    for (uint32_t i = 0; i < propertyCount; ++i)
    {
        VkCooperativeVectorPropertiesNV *p = &properties[i];
        p->sType                           = VK_STRUCTURE_TYPE_COOPERATIVE_VECTOR_PROPERTIES_NV;
        p->pNext                           = nullptr;
    }

    context.getInstanceInterface().getPhysicalDeviceCooperativeVectorPropertiesNV(context.getPhysicalDevice(),
                                                                                  &propertyCount, properties.data());

    for (uint32_t i = 0; i < propertyCount; ++i)
    {
        VkCooperativeVectorPropertiesNV *p = &properties[i];
        if (isMatrixMul(m_data.testType))
        {
            if (m_data.inputPacked)
            {
                auto const getInterp = [](VkComponentTypeKHR inputInterpretation) -> VkComponentTypeKHR
                {
                    switch (inputInterpretation)
                    {
                    case VK_COMPONENT_TYPE_SINT8_KHR:
                        return VK_COMPONENT_TYPE_SINT8_PACKED_NV;
                    case VK_COMPONENT_TYPE_UINT8_KHR:
                        return VK_COMPONENT_TYPE_UINT8_PACKED_NV;
                    default:
                        return inputInterpretation;
                    }
                };
                if (p->inputType == VK_COMPONENT_TYPE_UINT32_KHR &&
                    p->inputInterpretation == getInterp(m_data.inputInterpretation) &&
                    p->matrixInterpretation == m_data.matrixType && p->biasInterpretation == m_data.outputType &&
                    p->resultType == m_data.outputType && (m_data.testType != TT_MATRIXMADTRANSPOSE || p->transpose))
                {
                    supported[0] = supported[1] = true;
                }
            }
            else
            {
                if (p->inputType == m_data.inputType && p->inputInterpretation == m_data.inputInterpretation &&
                    p->matrixInterpretation == m_data.matrixType && p->biasInterpretation == m_data.outputType &&
                    p->resultType == m_data.outputType && (m_data.testType != TT_MATRIXMADTRANSPOSE || p->transpose))
                {
                    supported[0] = supported[1] = true;
                }
            }
        }
        else
        {
            VkComponentTypeKHR types[2] = {m_data.inputType, m_data.outputType};

            for (uint32_t j = 0; j < 2; ++j)
            {
                if (p->inputType == types[j] || p->resultType == types[j])
                {
                    supported[j] = true;
                }
            }
        }
    }

    if (!supported[0] || !supported[1])
        TCU_THROW(NotSupportedError, "cooperative vector combination not supported");
}

VkCooperativeVectorMatrixLayoutNV swapRowColMajor(VkCooperativeVectorMatrixLayoutNV layout)
{
    if (layout == VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV)
    {
        return VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_COLUMN_MAJOR_NV;
    }
    if (layout == VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_COLUMN_MAJOR_NV)
    {
        return VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV;
    }
    return layout;
}

// XXX assumes u8*u8
uint32_t getIntScaleShift(uint32_t K)
{
    K = deSmallestGreaterOrEquallPowerOfTwoU32(K);
    return deLog2Floor32(K * 256);
}

float getFloatScaleFactor(uint32_t K)
{
    uint32_t shift = getIntScaleShift(K);
    return 1.0f / (float)(1 << shift);
}

static std::string makeVecType(VkComponentTypeKHR t, uint32_t N, bool packed = false)
{
    std::stringstream ss;
    if (packed)
    {
        ss << "coopvecNV<" << getComponentTypeInfo(VK_COMPONENT_TYPE_UINT32_KHR).typeName << ", "
           << deDivRoundUp32(N, 32 / getComponentTypeInfo(t).bits) << ">";
    }
    else
    {
        ss << "coopvecNV<" << getComponentTypeInfo(t).typeName << ", " << N << ">";
    }
    return ss.str();
}

static int64_t RTNE(float x)
{
    bool half  = (x - floorf(x)) == 0.5f;
    int64_t tr = (int64_t)x;
    if (x >= 0.0f)
    {
        if (half)
        {
            return (tr & 1) ? (tr + 1) : tr;
        }
        return (int32_t)(x + 0.5f);
    }
    else
    {
        if (half)
        {
            return (tr & 1) ? (tr - 1) : tr;
        }
        return (int32_t)(x - 0.5f);
    }
}

// Use float scaling factor for hardgelu with float input type converted to int8
static bool doFloatScale(CaseDef const &data)
{
    return !isFloatType(data.outputType) && isFloatType(data.inputType) && !isFloatType(data.inputInterpretation) &&
           data.act0 == ACT_HARDGELU;
}

// Use int shift scaling for int output hardgelu
static bool doIntShift(CaseDef const &data)
{
    return !isFloatType(data.outputType) &&
           !(isFloatType(data.inputType) && !isFloatType(data.inputInterpretation) && data.act0 == ACT_HARDGELU);
}

void CooperativeVectorTestCase::initPrograms(SourceCollections &programCollection) const
{
    std::stringstream css;
    css << "#version 460 core\n";
    css << "#pragma use_vulkan_memory_model\n";
    css << "#extension GL_KHR_shader_subgroup_basic : enable\n"
           "#extension GL_KHR_memory_scope_semantics : enable\n"
           "#extension GL_EXT_nonuniform_qualifier : enable\n"

           "#extension GL_EXT_shader_explicit_arithmetic_types : enable\n"
           "#extension GL_NV_cooperative_vector : enable\n"
           "#extension GL_EXT_buffer_reference : enable\n"
           "#extension GL_EXT_ray_tracing : enable\n"
           "#extension GL_EXT_control_flow_attributes : enable\n"
           "#extension GL_EXT_shader_64bit_indexing : enable\n";

    switch (m_data.stage)
    {
    case STAGE_COMPUTE:
        css << "layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z = 1) in;\n";
        break;
    case STAGE_INTERSECT:
        css << "hitAttributeEXT vec3 hitAttribute;\n";
        break;
    case STAGE_ANY_HIT:
    case STAGE_CLOSEST_HIT:
        css << "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
               "hitAttributeEXT vec3 hitAttribute;\n";
        break;
    case STAGE_MISS:
        css << "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n";
        break;
    case STAGE_CALLABLE:
        css << "layout(location = 0) callableDataInEXT float dummy;\n";
        break;
    case STAGE_MESH:
    case STAGE_TASK:
        css << "#extension GL_EXT_mesh_shader : enable\n";
        css << "layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z = 1) in;\n";
        break;
    case STAGE_GEOMETRY:
        css << "layout (triangles) in;\n"
            << "layout (triangle_strip, max_vertices=3) out;\n"
            << "layout (invocations = " << m_data.threadsPerWorkgroupX << ") in;\n";
        break;
    case STAGE_TESS_CTRL:
        css << "layout (vertices = " << m_data.threadsPerWorkgroupX << ") out;\n";
        break;
    case STAGE_TESS_EVAL:
        css << "layout (quads, equal_spacing, cw) in;\n";
        break;
    default:
        break;
    }

    if (m_data.storageClass == SC_BUFFER_VARIABLE_POINTERS || m_data.storageClass == SC_WORKGROUP_VARIABLE_POINTERS)
        css << "#pragma use_variable_pointers\n";

    const char *typeStrA = getComponentTypeInfo(m_data.inputType).typeName;
    const char *typeStrB = isMatrixMul(m_data.testType) ? "uint32_t" : getComponentTypeInfo(m_data.inputType).typeName;
    const char *typeStrC = isTraining(m_data.testType)  ? "uint32_t" :
                           isMatrixMul(m_data.testType) ? "uint32_t" :
                                                          getComponentTypeInfo(m_data.outputType).typeName;
    const char *typeStrO = getComponentTypeInfo(m_data.outputType).typeName;

    css << "const int workgroupsX = " << m_data.workgroupsX << ";\n";

    if (m_data.storageClass == SC_PHYSICAL_STORAGE_BUFFER)
    {
        css << "layout(buffer_reference) buffer InputA { " << typeStrA << " x[]; };\n";
        css << "layout(buffer_reference) buffer InputB { " << typeStrB << " x[]; };\n";
        css << "layout(buffer_reference) buffer InputC { " << typeStrC << " x[]; };\n";
        css << "layout(buffer_reference) buffer Output { " << typeStrO << " x[]; };\n";
        css << "layout(set=0, binding=4) buffer Params { InputA inputA; InputB inputB; InputC inputC; Output outputO; "
               "} params;\n";
        css << "InputA inputA;\n";
        css << "InputB inputB;\n";
        css << "InputC inputC;\n";
        css << "Output outputO;\n";
    }
    else
    {
        css << "layout(set=0, binding=0) readonly buffer InputA { " << typeStrA << " x[]; } inputA;\n";
        css << "layout(set=0, binding=1) readonly buffer InputB { " << typeStrB << " x[]; } inputB;\n";
        css << "layout(set=0, binding=2) buffer InputC { " << typeStrC << " x[]; } inputC;\n";
        css << "layout(set=0, binding=3) coherent buffer Output { " << typeStrO << " x[]; } outputO;\n";
    }

    css << "const uint K = " << m_data.inputVectorSize << ";\n";
    css << "const uint N = " << m_data.outputVectorSize << ";\n";

    if (m_data.act0 == ACT_LOAD_SHARED)
    {
        css << "shared " << typeStrC << " biasSh[max(K,N) + 16];\n";
    }

    uint32_t elementsPer16B = 16 * 8 / getComponentTypeInfo(m_data.inputType).bits;
    css << "const uint inputVectorPaddedElements = (K + " << elementsPer16B - 1 << ") & ~" << elementsPer16B - 1
        << ";\n";

    if (m_data.testType != TT_OUTERPRODUCT)
    {
        elementsPer16B = 16 * 8 / getComponentTypeInfo(m_data.outputType).bits;
    }
    css << "const uint outputVectorPaddedElements = (N + " << elementsPer16B - 1 << ") & ~" << elementsPer16B - 1
        << " ;\n";

    if (m_data.storageClass == SC_WORKGROUP || m_data.storageClass == SC_WORKGROUP_VARIABLE_POINTERS)
    {
        css << "shared " << typeStrA << " sharedA[" << m_data.threadsPerWorkgroupX * m_data.threadsPerWorkgroupY
            << " * inputVectorPaddedElements];\n";
        css << "shared " << typeStrO << " sharedO[" << m_data.threadsPerWorkgroupX * m_data.threadsPerWorkgroupY
            << " * outputVectorPaddedElements];\n";
    }

    std::stringstream vecAType, vecBType, outputVecType, outputVecTypeK;

    css << "layout(constant_id = 6) const uint width = 0;\n";

    switch (m_data.stage)
    {
    case STAGE_MESH:
        css << "layout(triangles) out;\n"
            << "layout(max_vertices=3, max_primitives=1) out;\n";
        // fallthrough
    case STAGE_TASK:
    case STAGE_COMPUTE:
        css << "uint globalInvocationIndex = gl_LocalInvocationIndex + "
               "gl_WorkGroupSize.x*gl_WorkGroupSize.y*(gl_WorkGroupID.x + gl_WorkGroupID.y*gl_NumWorkGroups.x);\n";
        break;
    case STAGE_VERTEX:
        css << "uint globalInvocationIndex = gl_VertexIndex;\n";
        break;
    case STAGE_FRAGMENT:
        css << "uint globalInvocationIndex = width*uint(gl_FragCoord.y) + uint(gl_FragCoord.x);\n";
        break;
    case STAGE_GEOMETRY:
        css << "uint globalInvocationIndex = " << m_data.threadsPerWorkgroupX
            << " * gl_PrimitiveIDIn + gl_InvocationID;\n";
        break;
    case STAGE_TESS_CTRL:
        css << "uint globalInvocationIndex = gl_PatchVerticesIn * gl_PrimitiveID + gl_InvocationID;\n";
        break;
    case STAGE_TESS_EVAL:
        // One 32x1 "workgroup" per tessellated quad. But we skip storing the results for some threads.
        css << "uint globalInvocationIndex = " << m_data.threadsPerWorkgroupX
            << " * gl_PrimitiveID + uint(round(gl_TessCoord.x * " << m_data.threadsPerWorkgroupX << "));\n";
        break;
    case STAGE_RAYGEN:
    case STAGE_INTERSECT:
    case STAGE_ANY_HIT:
    case STAGE_CLOSEST_HIT:
    case STAGE_MISS:
    case STAGE_CALLABLE:
        css << "uint globalInvocationIndex = gl_LaunchIDEXT.x + gl_LaunchIDEXT.y*gl_LaunchSizeEXT.x;\n";
        break;
    default:
        TCU_THROW(InternalError, "Unknown stage");
    }

    css << "uint inputBase = inputVectorPaddedElements * globalInvocationIndex;\n";
    css << "uint outputBase = outputVectorPaddedElements * globalInvocationIndex;\n";
    css << "const uint inputElementSize = " << getComponentTypeInfo(m_data.inputType).bits / 8 << ";\n";
    css << "const uint matrixElementSize = " << getComponentTypeInfo(m_data.matrixType).bits / 8 << ";\n";
    css << "const uint biasElementSize = " << getComponentTypeInfo(m_data.outputType).bits / 8 << ";\n";
    css << "const uint outputElementSize = " << getComponentTypeInfo(m_data.outputType).bits / 8 << ";\n";

    for (uint32_t i = 0; i < 3; ++i)
    {
        VkCooperativeVectorMatrixLayoutNV layout = m_data.matrixLayout[i];
        if (i == 1)
        {
            layout = swapRowColMajor(layout);
        }
        if (layout == VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_COLUMN_MAJOR_NV)
        {
            css << "const uint matrixStride" << i << " = (N*matrixElementSize + 16 - 1) & ~(16 - 1);\n";
        }
        else if (layout == VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV)
        {
            css << "const uint matrixStride" << i << " = (K*matrixElementSize + 16 - 1) & ~(16 - 1);\n";
        }
        else
        {
            css << "const uint matrixStride" << i << " = 0;\n";
        }
    }

    css << "layout(constant_id = 2) const uint layerStride = 0;\n";
    css << "layout(constant_id = 3) const uint layer0Offset = 0;\n";
    css << "layout(constant_id = 4) const uint layer1Offset = 0;\n";
    css << "layout(constant_id = 5) const uint layer2Offset = 0;\n";

    if (m_data.testType == TT_OUTERPRODUCT)
    {
        css << "layout(constant_id = 7) const uint outerProductSize = 0;\n";
    }

    css << "const uint biasStride = (N*biasElementSize + 16 - 1) & ~(16 - 1);\n";

    vecAType << makeVecType(m_data.inputType, m_data.inputVectorSize, m_data.inputPacked);
    if (m_data.testType == TT_OUTERPRODUCT)
    {
        vecBType << makeVecType(m_data.inputType, m_data.outputVectorSize, m_data.inputPacked);
    }
    else
    {
        vecBType << makeVecType(m_data.inputType, m_data.inputVectorSize, m_data.inputPacked);
    }
    outputVecType << "coopvecNV<" << getComponentTypeInfo(m_data.outputType).typeName << ", " << m_data.outputVectorSize
                  << ">";

    outputVecTypeK << "coopvecNV<" << getComponentTypeInfo(m_data.outputType).typeName << ", " << m_data.inputVectorSize
                   << ">";

    css << vecAType.str() << " vecA;\n";
    // Initialize vecB to avoid division by undef/zero.
    css << vecBType.str() << " vecB = " << vecBType.str() << "(1);\n";
    css << outputVecType.str() << " vecO;\n";

    if (m_data.testType == TT_CONSTANT)
        css << "const " << outputVecType.str() << " vecConst = " << outputVecType.str() << "(1.0);\n";

    if (m_data.testType == TT_FUNC)
        css << vecAType.str() << " f(" << vecAType.str() << " v) { return -v; }\n";

    bool usesSigmoid = (m_data.act0 == ACT_SIGMOID) || (m_data.act1 == ACT_SIGMOID) || (m_data.act2 == ACT_SIGMOID);
    if (usesSigmoid)
    {
        css << vecAType.str() << " sigmoid(" << vecAType.str() << " v) {\n";
        css << "    return " << vecAType.str() << "(1.0) / (" << vecAType.str() << "(1.0) + exp(-v));\n";
        css << "}\n";

        if (vecAType.str() != outputVecType.str())
        {
            css << outputVecType.str() << " sigmoid(" << outputVecType.str() << " v) {\n";
            css << "    return " << outputVecType.str() << "(1.0) / (" << outputVecType.str() << "(1.0) + exp(-v));\n";
            css << "}\n";
        }
    }
    bool usesCoopmix =
        (m_data.act0 == ACT_LEAKYRELUSTEP) || (m_data.act1 == ACT_LEAKYRELUSTEP) || (m_data.act2 == ACT_LEAKYRELUSTEP);
    if (usesCoopmix)
    {
        css << vecAType.str() << " coopmix(" << vecAType.str() << " x, " << vecAType.str() << " y, " << vecAType.str()
            << " a) {\n";
        css << "    return x * (" << vecAType.str() << "(1.0) - a) + y * a;\n";
        css << "}\n";

        if (vecAType.str() != outputVecType.str())
        {
            css << outputVecType.str() << " coopmix(" << outputVecType.str() << " x, " << outputVecType.str() << " y, "
                << outputVecType.str() << " a) {\n";
            css << "    return x * (" << outputVecType.str() << "(1.0) - a) + y * a;\n";
            css << "}\n";
        }
    }

    static const char *matrixLayoutStr[] = {
        "gl_CooperativeVectorMatrixLayoutRowMajorNV",
        "gl_CooperativeVectorMatrixLayoutColumnMajorNV",
        "gl_CooperativeVectorMatrixLayoutInferencingOptimalNV",
        "gl_CooperativeVectorMatrixLayoutTrainingOptimalNV",
    };

    css << "void main()\n"
           "{\n";

    if (m_data.storageClass == SC_PHYSICAL_STORAGE_BUFFER)
    {
        css << "   inputA = params.inputA;\n";
        css << "   inputB = params.inputB;\n";
        css << "   inputC = params.inputC;\n";
        css << "   outputO = params.outputO;\n";
    }

    if (m_data.stage == STAGE_TESS_EVAL)
    {
        // We tessellate with an outer level of 32. The threads we want "in the workgroup"
        // are those on the edge, with coord.x < 1 (the first 32).
        css << "   bool dontLoadStore = false;\n"
               "   if (gl_TessCoord.y != 0 || gl_TessCoord.x == 1) { dontLoadStore = true; globalInvocationIndex = 0; "
               "}\n"
               "   if (!dontLoadStore) {\n";
    }

    if (m_data.testType == TT_REDUCESUM || m_data.testType == TT_OUTERPRODUCT)
    {
        // In case of duplicate invocations, only execute each index once
        css << "   if (atomicAdd(inputC.x[globalInvocationIndex], 1) != 0) return;\n";
    }

    std::string offsetType = m_data.uses64BitIndexing ? "uint64_t" : "uint32_t";

    if (m_data.storageClass == SC_WORKGROUP || m_data.storageClass == SC_WORKGROUP_VARIABLE_POINTERS)
    {
        css << "   " << vecAType.str() << " loadTemp;\n";
        css << "   coopVecLoadNV(loadTemp, inputA.x, inputBase * inputElementSize);\n";
        css << "   coopVecStoreNV(loadTemp, sharedA, inputVectorPaddedElements * gl_LocalInvocationIndex * "
               "inputElementSize);\n";
        css << "   barrier();\n";
        css << "   coopVecLoadNV(vecA, sharedA, inputVectorPaddedElements * gl_LocalInvocationIndex * "
               "inputElementSize);\n";
    }
    else
    {
        css << "   coopVecLoadNV(vecA, inputA.x, " << offsetType << "(inputBase * inputElementSize));\n";
    }

    if (m_data.act0 == ACT_LOAD_SHARED)
    {
        css << "   if (gl_LocalInvocationIndex == 0) {\n"
               "       for (uint32_t k = 0; k < max(N,K) + 16; ++k) {\n"
               "           biasSh[k] = inputC.x[k];\n"
               "       }\n"
               "   }\n"
               "   barrier();\n";
    }

    if (m_data.testType == TT_MATRIXMUL2ADD || m_data.testType == TT_MATRIXMUL2ADDMUL2)
    {
        // vecB = vecA with components swapped pairwise
        if (m_data.inputPacked)
        {
            DE_ASSERT(m_data.inputType == VK_COMPONENT_TYPE_SINT8_KHR ||
                      m_data.inputType == VK_COMPONENT_TYPE_UINT8_KHR);
            css << "   vecB = vecA;\n";
            for (uint32_t i = 0; i < m_data.inputVectorSize / 4U; ++i)
            {
                css << "   vecB[" << i << "] = ((vecB[" << i << "] & 0xFF00FF) << 8) | ((vecB[" << i
                    << "] & 0xFF00FF00) >> 8);\n";
            }
            if ((m_data.inputVectorSize % 4) >= 2)
            {
                uint32_t n = m_data.inputVectorSize / 4;
                css << "   vecB[" << n << "] = (vecB[" << n << "] & 0xFFFF0000) | ((vecB[" << n
                    << "] & 0xFF) << 8) | ((vecB[" << n << "] & 0xFF00) >> 8);\n";
            }
        }
        else
        {
            css << "   vecB = " << vecAType.str() << "(";
            for (uint32_t i = 0; i < m_data.inputVectorSize; ++i)
            {
                uint32_t idx = i ^ 1;
                if (idx >= m_data.inputVectorSize)
                {
                    idx = i;
                }
                if (m_data.storageClass == SC_WORKGROUP || m_data.storageClass == SC_WORKGROUP_VARIABLE_POINTERS)
                {
                    css << "sharedA[inputVectorPaddedElements * gl_LocalInvocationIndex + " << idx << "]";
                }
                else
                {
                    css << "inputA.x[inputBase + " << idx << "]";
                }
                if (i != m_data.inputVectorSize - 1)
                {
                    css << ",";
                }
            }
            css << ");\n";
        }
    }

    if (m_data.testType == TT_OUTERPRODUCT)
    {
        css << "   coopVecLoadNV(vecB, inputB.x, outputBase * inputElementSize);\n";
    }
    else if (isTraining(m_data.testType))
    {
        // nothing
    }
    else if (!isMatrixMul(m_data.testType))
    {
        css << "   vecB = " << vecBType.str() << "(";
        for (uint32_t i = 0; i < m_data.inputVectorSize; ++i)
        {
            css << "inputB.x[inputBase + " << i << "]";
            if (i != m_data.inputVectorSize - 1)
            {
                css << ",";
            }
        }
        css << ");\n";
    }

    if (m_data.stage == STAGE_TESS_EVAL)
    {
        css << "   }\n";
    }

    if (m_data.testType == TT_COMPOSITE_ARRAY)
    {
        css << "   " << vecAType.str() << " vecAArr[2];\n    vecAArr[1] = vecA; vecAArr[0] = " << vecAType.str()
            << "(0.0);\n";
        css << "   " << vecBType.str() << " vecBArr[2];\n    vecBArr[1] = vecB; vecBArr[0] = " << vecAType.str()
            << "(0.0);\n";
        css << "   " << outputVecType.str() << " vecOArr[2];\n";
    }

    auto const &addActivation = [&](Activation act, const std::string &vec, const std::string &vecType, uint32_t idx)
    {
        switch (act)
        {
        default:
            DE_ASSERT(0);
        case ACT_NONE:
            break;
        case ACT_MUL:
            if (isFloatType(m_data.outputType))
            {
                css << "   " << vec << " *= " << typeStrA << "(0.5);\n";
            }
            else
            {
                css << "   " << vec << " *= " << vecType << "(2);\n";
            }
            break;
        case ACT_MAX:
            css << "   " << vec << " = max(" << vec << ", " << vecType << "(0.0));\n";
            break;
        case ACT_NONUNIF:
            if (isFloatType(m_data.outputType))
            {
                css << "   " << vec << " *= " << typeStrA << "((globalInvocationIndex % 3) / 2.0);\n";
            }
            else
            {
                css << "   " << vec << " *= " << vecType << "(globalInvocationIndex % 3);\n";
            }
            break;
        case ACT_DIVERGE:
            css << "   if ((globalInvocationIndex & 1) != 0) {\n";
            if (isFloatType(m_data.outputType))
            {
                css << "       " << vec << " *= " << typeStrA << "(0.5);\n";
            }
            else
            {
                css << "       " << vec << " *= " << vecType << "(2);\n";
            }
            css << "   }\n";
            break;
        case ACT_SIGMOID:
            css << "   " << vec << " = sigmoid(" << vec << ");\n";
            break;
        case ACT_LEAKYRELUSTEP:
            css << "   " << vec << " = coopmix(" << vecType << "(0.5)*" << vec << ", " << vec << ", step(" << vecType
                << "(0.0), " << vec << "));\n";
            break;
        case ACT_LEAKYRELUMAX:
            css << "   " << vec << " = max(" << vecType << "(0.5)*" << vec << ", " << vec << ");\n";
            break;
        case ACT_HARDGELU:
        {
            // hardgelu is x * clamp(1.f/3.f*x + 0.5f) and often has a linear scale/bias beforehand.
            // This implementation tweaks the values a bit to empirically work better with the
            // random numbers we generate:
            //    actVal0 = (1.0 / 2.0) * actVal0 + (0.75);
            //    actVal0 = min(65536, actVal0) * clamp((1.0/3.0) * actVal0 + 0.75, -4, 4);

            std::string actType = vecType;
            if (vecType.find("float") == std::string::npos)
            {
                actType = vecType.substr(0, vecType.find("<") + 1) + std::string("float32_t") +
                          vecType.substr(vecType.find(","), std::string::npos);
            }
            std::string actVal = "actVal" + std::to_string(idx);
            css << "\n"
                   "   "
                << actType << " " << actVal << " = " << actType << "(" << vec
                << ");\n"
                   "   "
                << actVal << " = " << actType << "(1.0 / 2.0) * " << actVal << " + " << actType << "(0.75);\n";
            if (vecType.find("float") == std::string::npos)
            {
                css << "   " << actVal << " = min(" << actType << "(65536), " << actVal << ") * clamp(" << actType
                    << "(1.0/3.0) * " << actVal << " + " << actType << "(0.75), " << actType << "(-4), " << actType
                    << "(4));\n";
            }
            else
            {
                css << "   " << actVal << " = min(" << actType << "(128.0), " << actVal << ") * clamp(" << actType
                    << "(1.0/3.0) * " << actVal << " + " << actType << "(0.75), " << actType << "(0), " << actType
                    << "(1));\n";
            }
            css << "   " << vec << " = " << vecType << "(" << actVal << ");\n";
            css << "\n";
        }
        break;
        case ACT_LOAD:
        case ACT_LOAD_SHARED:
        {
            std::string actType = vecType;
            std::string actVal  = "actVal" + std::to_string(idx);
            css << "   " << actType << " " << actVal << ";\n";
            if (act == ACT_LOAD)
            {
                css << "   coopVecLoadNV(" << actVal << ", inputC.x, 16*((globalInvocationIndex & 1)));\n";
            }
            else
            {
                css << "   coopVecLoadNV(" << actVal << ", biasSh, 16*((globalInvocationIndex & 1)));\n";
            }
            if (vecType.find("float") == std::string::npos)
            {
                css << "   " << actVal << " *= 16;\n";
            }
            css << "   " << vec << " = " << vec << " + " << actVal << ";\n";
        }
        break;
        case ACT_LOAD_READONLY:
            css << "   " << vec << " = " << vec << " + " << vecType << "(inputA.x[globalInvocationIndex]);\n";
            break;
        }
    };

    std::string matrixOffsetString, matrixOffsetString2, matrixOffsetString3, biasOffsetString;
    if (m_data.nonuniformOffset)
    {
        css << "   uint32_t matrixIdx = (globalInvocationIndex / " << nonuniformMatrixGroupSize << ");\n";
        matrixOffsetString  = "(matrixIdx * layerStride + layer0Offset)";
        matrixOffsetString2 = "(matrixIdx * layerStride + layer1Offset)";
        matrixOffsetString3 = "(matrixIdx * layerStride + layer2Offset)";
        css << "   uint32_t biasOffset = (globalInvocationIndex / " << nonuniformBiasGroupSize << ") * biasStride;\n";
        biasOffsetString = "(biasOffset)";
    }
    else
    {
        matrixOffsetString  = "layer0Offset";
        matrixOffsetString2 = "layer1Offset";
        matrixOffsetString3 = "layer2Offset";
        css << "   uint32_t biasOffset = 0;\n";
        biasOffsetString = "biasOffset";
    }
    matrixOffsetString = offsetType + "(" + matrixOffsetString + ")";

    if (m_data.cfDivergent)
    {
        css << "   uint32_t subgroupInvocation = gl_SubgroupInvocationID;\n"
               "   uint32_t invocationIDMasks[4] = {0x8, 0x2, 0x1, 0xFFFFFFF4};\n"
               "   for (int maskIdx = 0; maskIdx < 4; ++maskIdx) {\n"
               "       if (((1<<gl_SubgroupInvocationID) & invocationIDMasks[maskIdx]) != 0 ||\n"
               "           (maskIdx == 3 && gl_SubgroupInvocationID >= 32)) {\n";
    }

    std::string inputInterp  = getComponentTypeInfo(m_data.inputInterpretation).interpString;
    std::string inputInterp0 = getComponentTypeInfo(m_data.inputInterpretation).interpString;
    std::string matrixInterp = getComponentTypeInfo(m_data.matrixType).interpString;
    std::string biasInterp   = getComponentTypeInfo(m_data.outputType).interpString;

    if (m_data.inputPacked)
    {
        switch (m_data.inputInterpretation)
        {
        case VK_COMPONENT_TYPE_SINT8_KHR:
            inputInterp0 = "gl_ComponentTypeSignedInt8PackedNV";
            break;
        case VK_COMPONENT_TYPE_UINT8_KHR:
            inputInterp0 = "gl_ComponentTypeUnsignedInt8PackedNV";
            break;
        default:
            DE_ASSERT(0);
            break;
        }
    }

    switch (m_data.testType)
    {
    default:
        DE_ASSERT(0);
        // fall through
    case TT_LENGTH:
        css << "   vecO = " << outputVecType.str() << "(vecO.length());\n";
        break;
    case TT_CONSTANT:
        css << "   vecO = vecConst;\n";
        break;
    case TT_CONVERT:
        css << "   vecO = " << outputVecType.str() << "(vecA);\n";
        break;
    case TT_COMPOSITE:
    case TT_COMPOSITE_RVALUE:
        css << "   for (int i = 0; i < vecA.length(); ++i) {\n"
               "       vecO[i] = vecA[i] + vecB[i];\n"
               "   }\n";
        if (m_data.testType == TT_COMPOSITE_RVALUE)
        {
            css << "   " << vecAType.str()
                << " t = vecA;\n"
                   "   vecO[0] = (t += vecB)[0];\n";
            if (m_data.inputVectorSize > 1)
            {
                css << "   t = vecA;\n"
                       "   vecO[1] = (t += vecB)[1];\n";
            }
        }
        break;
    case TT_COMPOSITE_ARRAY:
        css << "   for (int i = 0; i < vecA.length(); ++i) {\n"
               "       vecOArr[1][i] = vecAArr[1][i] + vecBArr[1][i];\n"
               "   }\n";
        break;
    case TT_VECTOR_EXTRACT:
        css << "   for (int i = 0; i < vecA.length(); ++i) {\n"
               "       vecO[i] = vecA[i] + (vecB + "
            << vecAType.str()
            << "(1))[i];\n"
               "   }\n";
        break;
    case TT_ADD:
        css << "   vecO = vecA + vecB;\n";
        break;
    case TT_SUB:
        css << "   vecO = vecA - vecB;\n";
        break;
    case TT_MUL:
        css << "   vecO = vecA * vecB;\n";
        break;
    case TT_DIV:
        css << "   vecO = vecA / vecB;\n";
        break;
    case TT_NEGATE:
        css << "   vecO = -vecA;\n";
        break;
    case TT_FUNC:
        css << "   vecO = f(vecA);\n";
        break;
    case TT_VECTORTIMESSCALAR:
        css << "   vecO = (" << typeStrA << "(2.0)*vecA)*" << typeStrA << "(3.0);\n";
        break;
    case TT_EXP:
        css << "   vecO = exp(vecA * " << typeStrA << "(0.0625));\n";
        break;
    case TT_LOG:
        css << "   vecO = log(vecA + " << vecAType.str() << "(100));\n";
        break;
    case TT_TANH:
        css << "   vecO = tanh(vecA * " << typeStrA << "(0.1));\n";
        break;
    case TT_ATAN:
        css << "   vecO = atan(vecA);\n";
        break;
    case TT_MIN:
        css << "   vecO = min(min(vecA, vecB), " << vecAType.str() << "(5.0));\n";
        break;
    case TT_MAX:
        css << "   vecO = max(max(vecA, vecB), " << vecAType.str() << "(0.0));\n";
        break;
    case TT_CLAMP:
        css << "   vecO = clamp(vecA, vecB, " << vecAType.str() << "(5.0));\n";
        break;
    case TT_STEP:
        css << "   vecO = step(" << vecAType.str() << "(0.0), vecA);\n";
        break;
    case TT_FMA:
        css << "   vecO = fma(vecA, vecB, " << vecAType.str() << "(0.5));\n";
        break;
    case TT_AND:
        css << "   vecO = vecA & vecB;\n";
        break;
    case TT_OR:
        css << "   vecO = vecA | vecB;\n";
        break;
    case TT_XOR:
        css << "   vecO = vecA ^ vecB;\n";
        break;
    case TT_NOT:
        css << "   vecO = ~vecA;\n";
        break;
    case TT_SHL:
        css << "   vecO = vecA << (vecB & " << vecAType.str() << "(7));\n";
        break;
    case TT_SHR:
        css << "   vecO = vecA >> (vecB & " << vecAType.str() << "(7));\n";
        break;
    case TT_MATRIXMUL:
    case TT_MATRIXMUL_TRAININGBIAS:
        css << "   {\n";
        css << "   " << makeVecType(m_data.inputType, m_data.inputVectorSize, m_data.inputPacked)
            << " v = " << makeVecType(m_data.inputType, m_data.inputVectorSize, m_data.inputPacked) << "(vecA);\n";
        css << "   coopVecMatMulNV(vecO, v, " << inputInterp0 << ", inputB.x, " << matrixOffsetString << ", "
            << matrixInterp << ", N, K, " << matrixLayoutStr[m_data.matrixLayout[0]] << ", "
            << (m_data.transpose ? "true" : "false") << ", matrixStride0);\n";
        css << "   }\n";
        addActivation(m_data.act0, "vecO", outputVecType.str(), 0);
        break;
    case TT_MATRIXMAD:
    case TT_MATRIXMADTRANSPOSE:
        css << "   {\n";
        css << "   " << makeVecType(m_data.inputType, m_data.inputVectorSize, m_data.inputPacked)
            << " v = " << makeVecType(m_data.inputType, m_data.inputVectorSize, m_data.inputPacked) << "(vecA);\n";
        css << "   coopVecMatMulAddNV(vecO, v, " << inputInterp0 << ", inputB.x, " << matrixOffsetString << ", "
            << matrixInterp
            << ", "
               "inputC.x, "
            << biasOffsetString << ", " << biasInterp << ", N, K, " << matrixLayoutStr[m_data.matrixLayout[0]] << ", "
            << (m_data.transpose ? "true" : "false") << ", matrixStride0);\n";
        css << "   }\n";
        addActivation(m_data.act0, "vecO", outputVecType.str(), 0);
        break;
    case TT_MATRIXMUL3:
        // (NxK * (KxN * (NxK * Kx1))) -> Nx1
        css << "   " << outputVecTypeK.str() << " temp;\n";
        css << "   {\n";
        css << "   " << makeVecType(m_data.inputType, m_data.inputVectorSize, m_data.inputPacked)
            << " v = " << makeVecType(m_data.inputType, m_data.inputVectorSize, m_data.inputPacked) << "(vecA);\n";
        css << "   coopVecMatMulNV(vecO, v, " << inputInterp0 << ", inputB.x, " << matrixOffsetString << ", "
            << matrixInterp << ", N, K, " << matrixLayoutStr[m_data.matrixLayout[0]] << ", "
            << (m_data.transpose ? "true" : "false") << ", matrixStride0);\n";
        css << "   }\n";
        addActivation(m_data.act0, "vecO", outputVecType.str(), 0);
        if (doIntShift(m_data))
        {
            css << "   vecO >>= " << outputVecType.str() << "(" << getIntScaleShift(m_data.inputVectorSize) << ");\n";
        }
        css << "   {\n";
        if (doFloatScale(m_data))
        {
            css << "   " << makeVecType(m_data.inputType, m_data.outputVectorSize)
                << " v = " << makeVecType(m_data.inputType, m_data.outputVectorSize) << "(actVal0 * "
                << getFloatScaleFactor(m_data.inputVectorSize) << ");\n";
        }
        else
        {
            css << "   " << makeVecType(m_data.inputType, m_data.outputVectorSize)
                << " v = " << makeVecType(m_data.inputType, m_data.outputVectorSize) << "(vecO);\n";
        }
        css << "   coopVecMatMulNV(temp, v, " << inputInterp << ", inputB.x, " << matrixOffsetString2 << ", "
            << matrixInterp << ", K, N, " << matrixLayoutStr[m_data.matrixLayout[1]] << ", "
            << (m_data.transpose ? "true" : "false") << ", matrixStride1);\n";
        css << "   }\n";
        addActivation(m_data.act1, "temp", outputVecTypeK.str(), 1);
        if (doIntShift(m_data))
        {
            css << "   temp >>= " << outputVecTypeK.str() << "(" << getIntScaleShift(m_data.inputVectorSize) << ");\n";
        }
        css << "   {\n";
        if (doFloatScale(m_data))
        {
            css << "   " << makeVecType(m_data.inputType, m_data.inputVectorSize)
                << " v = " << makeVecType(m_data.inputType, m_data.inputVectorSize) << "(actVal1 * "
                << getFloatScaleFactor(m_data.inputVectorSize) << ");\n";
        }
        else
        {
            css << "   " << makeVecType(m_data.inputType, m_data.inputVectorSize)
                << " v = " << makeVecType(m_data.inputType, m_data.inputVectorSize) << "(temp);\n";
        }
        css << "   coopVecMatMulNV(vecO, v, " << inputInterp << ", inputB.x, " << matrixOffsetString3 << ", "
            << matrixInterp << ", N, K, " << matrixLayoutStr[m_data.matrixLayout[2]] << ", "
            << (m_data.transpose ? "true" : "false") << ", matrixStride2);\n";
        css << "   }\n";
        addActivation(m_data.act2, "vecO", outputVecType.str(), 2);
        break;
    case TT_MATRIXMUL2ADD:
    case TT_MATRIXMUL2ADDMUL2:
        // vecB = vecA with components swapped pairwise
        // temp0 = mat0 * vecA; // NxK * Kx1
        // temp1 = mat0 * vecB; // NxK * Kx1
        // temp2 = temp0 + temp1
        // temp2 = activation(temp2)
        // if (m_data.testType == TT_MATRIXMUL2ADDMUL2) {
        //   temp3 = mat1 * temp2; // KxN * Nx1
        //   temp3 = activation(temp3)
        //   vecO  = mat2 * temp3; // NxK * Kx1
        //   vecO  = activation(vecO)
        // } else {
        //   vecO = temp2
        // }

        css << "   " << outputVecType.str() << " temp0, temp1, temp2;\n";
        css << "   " << outputVecTypeK.str() << " temp3;\n";
        // temp0 = mat0 * vecA; // NxK * Kx1
        css << "   {\n";
        css << "   " << makeVecType(m_data.inputType, m_data.inputVectorSize, m_data.inputPacked)
            << " v = " << makeVecType(m_data.inputType, m_data.inputVectorSize, m_data.inputPacked) << "(vecA);\n";
        css << "   coopVecMatMulNV(temp0, v, " << inputInterp0 << ", inputB.x, " << matrixOffsetString << ", "
            << matrixInterp << ", N, K, " << matrixLayoutStr[m_data.matrixLayout[0]] << ", "
            << (m_data.transpose ? "true" : "false") << ", matrixStride0);\n";
        css << "   }\n";
        // temp1 = mat0 * vecB; // NxK * Kx1
        css << "   {\n";
        css << "   " << makeVecType(m_data.inputType, m_data.inputVectorSize, m_data.inputPacked)
            << " v = " << makeVecType(m_data.inputType, m_data.inputVectorSize, m_data.inputPacked) << "(vecB);\n";
        css << "   coopVecMatMulNV(temp1, v, " << inputInterp0 << ", inputB.x, " << matrixOffsetString << ", "
            << matrixInterp << ", N, K, " << matrixLayoutStr[m_data.matrixLayout[0]] << ", "
            << (m_data.transpose ? "true" : "false") << ", matrixStride0);\n";
        css << "   }\n";
        // temp2 = temp0 + temp1
        css << "   temp2 = temp0 + temp1;\n";
        // temp2 = activation(temp2)
        addActivation(m_data.act0, "temp2", outputVecType.str(), 0);

        if (m_data.testType == TT_MATRIXMUL2ADDMUL2)
        {
            if (doIntShift(m_data))
            {
                css << "   temp2 >>= " << outputVecType.str() << "(" << getIntScaleShift(m_data.inputVectorSize)
                    << ");\n";
            }

            // temp3 = mat1 * temp2; // KxN * Nx1
            css << "   {\n";
            if (doFloatScale(m_data))
            {
                css << "   " << makeVecType(m_data.inputType, m_data.outputVectorSize)
                    << " v = " << makeVecType(m_data.inputType, m_data.outputVectorSize) << "(actVal0 * "
                    << getFloatScaleFactor(m_data.inputVectorSize) << ");\n";
            }
            else
            {
                css << "   " << makeVecType(m_data.inputType, m_data.outputVectorSize)
                    << " v = " << makeVecType(m_data.inputType, m_data.outputVectorSize) << "(temp2);\n";
            }
            css << "   coopVecMatMulNV(temp3, v, " << inputInterp << ", inputB.x, " << matrixOffsetString2 << ", "
                << matrixInterp << ", K, N, " << matrixLayoutStr[m_data.matrixLayout[1]] << ", "
                << (m_data.transpose ? "true" : "false") << ", matrixStride1);\n";
            css << "   }\n";
            // temp3 = activation(temp3)
            addActivation(m_data.act1, "temp3", outputVecTypeK.str(), 1);
            if (doIntShift(m_data))
            {
                css << "   temp3 >>= " << outputVecTypeK.str() << "(" << getIntScaleShift(m_data.outputVectorSize)
                    << ");\n";
            }

            // vecO  = mat2 * temp3; // NxK * Kx1
            css << "   {\n";
            if (doFloatScale(m_data))
            {
                css << "   " << makeVecType(m_data.inputType, m_data.inputVectorSize)
                    << " v = " << makeVecType(m_data.inputType, m_data.inputVectorSize) << "(actVal1 * "
                    << getFloatScaleFactor(m_data.outputVectorSize) << ");\n";
            }
            else
            {
                css << "   " << makeVecType(m_data.inputType, m_data.inputVectorSize)
                    << " v = " << makeVecType(m_data.inputType, m_data.inputVectorSize) << "(temp3);\n";
            }
            css << "   coopVecMatMulNV(vecO, v, " << inputInterp << ", inputB.x, " << matrixOffsetString3 << ", "
                << matrixInterp << ", N, K, " << matrixLayoutStr[m_data.matrixLayout[2]] << ", "
                << (m_data.transpose ? "true" : "false") << ", matrixStride2);\n";
            css << "   }\n";
            // vecO  = activation(vecO)
            addActivation(m_data.act2, "vecO", outputVecType.str(), 2);
        }
        else
        {
            // vecO = temp2
            css << "   vecO = temp2;\n";
        }
        break;
    case TT_REDUCESUM:
    case TT_OUTERPRODUCT:
        if (m_data.stage == STAGE_TESS_EVAL)
        {
            css << "   if (!dontLoadStore) {\n";
        }
        switch (m_data.resultAddr)
        {
        case RESULT_ADDR_UNIFORM:
            css << "   uint index = 1;\n";
            break;
        case RESULT_ADDR_UNIQUE:
            css << "   uint index = globalInvocationIndex;\n";
            break;
        case RESULT_ADDR_CLUSTERED:
            css << "   uint index = globalInvocationIndex / 5;\n";
            break;
        default:
            DE_ASSERT(0);
            break;
        }
        switch (m_data.testType)
        {
        case TT_REDUCESUM:
            css << "   uint offset = outputVectorPaddedElements * outputElementSize * index;\n";
            css << "   coopVecReduceSumAccumulateNV(vecA, outputO.x, " << offsetType << "(offset));\n";
            break;
        case TT_OUTERPRODUCT:
            css << "   uint offset = outerProductSize * index;\n";
            css << "   coopVecOuterProductAccumulateNV(vecA, vecB, outputO.x, " << offsetType << "(offset), 0, "
                << matrixLayoutStr[m_data.matrixLayout[0]] << ", "
                << getComponentTypeInfo(m_data.outputType).interpString << ");\n";
            break;
        default:
            DE_ASSERT(0);
            break;
        }
        if (m_data.stage == STAGE_TESS_EVAL)
        {
            css << "   }\n";
        }
        break;
    }

    if (m_data.cfDivergent)
    {
        css << "       }\n"
               "   }\n";
    }

    if (m_data.testType == TT_COMPOSITE_ARRAY)
    {
        css << "   vecOArr[0] = " << outputVecType.str() << "(0.0);\n";
        css << "   vecO = vecOArr[1];\n";
    }

    if (m_data.stage == STAGE_TESS_EVAL)
    {
        css << "   if (!dontLoadStore) {\n";
    }

    if (!isTraining(m_data.testType))
    {
        if (m_data.storageClass == SC_WORKGROUP || m_data.storageClass == SC_WORKGROUP_VARIABLE_POINTERS)
        {
            css << "   barrier();\n";
            css << "   coopVecStoreNV(vecO, sharedO, outputVectorPaddedElements * gl_LocalInvocationIndex * "
                   "outputElementSize);\n";
            css << "   " << outputVecType.str() << " storeTemp;\n";
            css << "   coopVecLoadNV(storeTemp, sharedO, outputVectorPaddedElements * gl_LocalInvocationIndex * "
                   "outputElementSize);\n";
            css << "   coopVecStoreNV(storeTemp, outputO.x, outputBase * outputElementSize);\n";
        }
        else
        {
            css << "   coopVecStoreNV(vecO, outputO.x, " << offsetType << "(outputBase * outputElementSize));\n";
        }
    }

    if (m_data.stage == STAGE_TESS_EVAL)
    {
        css << "   }\n";
    }

    switch (m_data.stage)
    {
    case STAGE_INTERSECT:
        css << "  hitAttribute = vec3(0.0f, 0.0f, 0.0f);\n"
               "  reportIntersectionEXT(1.0f, 0);\n";
        break;
    case STAGE_VERTEX:
        css << "  gl_PointSize = 1.0f;\n";
        break;
    case STAGE_TASK:
        css << "  EmitMeshTasksEXT(0, 0, 0);\n";
        break;
    default:
        break;
    }

    css << "}\n";

    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0);

    switch (m_data.stage)
    {
    case STAGE_COMPUTE:
        programCollection.glslSources.add("test") << glu::ComputeSource(css.str()) << buildOptions;
        break;
    case STAGE_VERTEX:
        programCollection.glslSources.add("test") << glu::VertexSource(css.str()) << buildOptions;
        break;
    case STAGE_FRAGMENT:
    {
        std::stringstream vss;
        vss << "#version 450 core\n"
               "void main()\n"
               "{\n"
               // full-viewport quad
               "  gl_Position = vec4( 2.0*float(gl_VertexIndex&2) - 1.0, 4.0*(gl_VertexIndex&1)-1.0, 1.0 - 2.0 * "
               "float(gl_VertexIndex&1), 1);\n"
               "}\n";
        programCollection.glslSources.add("vert") << glu::VertexSource(vss.str());

        programCollection.glslSources.add("test") << glu::FragmentSource(css.str()) << buildOptions;
    }
    break;
    case STAGE_GEOMETRY:
    {
        std::stringstream vss;
        vss << "#version 450 core\n"
               "void main()\n"
               "{\n"
               "  gl_Position = vec4(0,0,0,1);\n"
               "}\n";
        programCollection.glslSources.add("vert") << glu::VertexSource(vss.str());
        programCollection.glslSources.add("test") << glu::GeometrySource(css.str()) << buildOptions;
    }
    break;
    case STAGE_TESS_CTRL:
    {
        std::stringstream vss;
        vss << "#version 450 core\n"
               "void main()\n"
               "{\n"
               "  gl_Position = vec4(0,0,0,1);\n"
               "}\n";
        programCollection.glslSources.add("vert") << glu::VertexSource(vss.str());

        std::stringstream tss;
        tss << "#version 450 core\n"
               "layout (triangles, equal_spacing, cw) in;\n"
               "void main()\n"
               "{\n"
               "}\n";
        programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tss.str());

        programCollection.glslSources.add("tesc") << glu::TessellationControlSource(css.str()) << buildOptions;
    }
    break;
    case STAGE_TESS_EVAL:
    {
        std::stringstream vss;
        vss << "#version 450 core\n"
               "void main()\n"
               "{\n"
               "  gl_Position = vec4(0,0,0,1);\n"
               "}\n";
        programCollection.glslSources.add("vert") << glu::VertexSource(vss.str());

        std::stringstream tss;
        tss << "#version 450 core\n"
               "layout (vertices = 4) out;\n"
               "void main()\n"
               "{\n"
               "  gl_TessLevelInner[0] = 1.0;\n"
               "  gl_TessLevelInner[1] = 1.0;\n"
               "  gl_TessLevelOuter[0] = 1.0;\n"
               "  gl_TessLevelOuter[1] = "
            << m_data.threadsPerWorkgroupX
            << ";\n"
               "  gl_TessLevelOuter[2] = 1.0;\n"
               "  gl_TessLevelOuter[3] = "
            << m_data.threadsPerWorkgroupX
            << ";\n"
               "}\n";
        programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tss.str());

        programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(css.str()) << buildOptions;
    }
    break;
    case STAGE_TASK:
    {
        programCollection.glslSources.add("test") << glu::TaskSource(css.str()) << buildOptions;

        std::stringstream mesh;
        mesh << "#version 450\n"
             << "#extension GL_EXT_mesh_shader : enable\n"
             << "#extension GL_EXT_nonuniform_qualifier : enable\n"
             << "layout(local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
             << "layout(triangles) out;\n"
             << "layout(max_vertices=3, max_primitives=1) out;\n"
             << "void main()\n"
             << "{\n"
             << "  SetMeshOutputsEXT(0, 0);\n"
             << "}\n";
        programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;
    }
    break;
    case STAGE_MESH:
        programCollection.glslSources.add("test") << glu::MeshSource(css.str()) << buildOptions;
        break;
    case STAGE_RAYGEN:
        programCollection.glslSources.add("test") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
        break;
    case STAGE_INTERSECT:
        programCollection.glslSources.add("rgen")
            << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader(0, 5))) << buildOptions;
        programCollection.glslSources.add("test")
            << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
        break;
    case STAGE_ANY_HIT:
        programCollection.glslSources.add("rgen")
            << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader(0, 5))) << buildOptions;
        programCollection.glslSources.add("test") << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
        break;
    case STAGE_CLOSEST_HIT:
        programCollection.glslSources.add("rgen")
            << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader(0, 5))) << buildOptions;
        programCollection.glslSources.add("test")
            << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
        break;
    case STAGE_MISS:
        programCollection.glslSources.add("rgen")
            << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader(0, 5))) << buildOptions;
        programCollection.glslSources.add("test") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
        break;
    case STAGE_CALLABLE:
    {
        std::stringstream css2;
        css2 << "#version 460 core\n"
                "#extension GL_EXT_nonuniform_qualifier : enable\n"
                "#extension GL_EXT_ray_tracing : require\n"
                "layout(location = 0) callableDataEXT float dummy;"
                "layout(set = 0, binding = 5) uniform accelerationStructureEXT topLevelAS;\n"
                "\n"
                "void main()\n"
                "{\n"
                "  executeCallableEXT(0, 0);\n"
                "}\n";

        programCollection.glslSources.add("rgen")
            << glu::RaygenSource(updateRayTracingGLSL(css2.str())) << buildOptions;
    }
        programCollection.glslSources.add("test")
            << glu::CallableSource(updateRayTracingGLSL(css.str())) << buildOptions;
        break;
    default:
        TCU_THROW(InternalError, "Unknown stage");
    }
}

TestInstance *CooperativeVectorTestCase::createInstance(Context &context) const
{
    return new CooperativeVectorTestInstance(context, m_data);
}

#ifdef COOPERATIVE_VECTOR_EXTENDED_DEBUG
string dumpWholeMatrix(void *data, VkComponentTypeKHR dt, uint32_t matrixElemCount)
{
    bool floatType = isFloatType(dt);
    bool sIntType  = isSIntType(dt);
    std::stringstream ss;

    for (uint32_t i = 0; i < matrixElemCount; i++)
    {
        if (floatType)
            ss << getDataFloat(data, dt, i) << "\t";
        else if (sIntType)
            ss << (int32_t)getDataInt(data, dt, i) << "\t";
        else
            ss << getDataInt(data, dt, i) << "\t";
    }
    ss << std::endl;

    return ss.str();
}
#endif

void appendShaderStageCreateInfo(std::vector<VkPipelineShaderStageCreateInfo> &vec, VkShaderModule module,
                                 VkShaderStageFlagBits stage, vk::VkSpecializationInfo const *specInfo)
{
    const VkPipelineShaderStageCreateInfo info = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                             // const void* pNext;
        0u,                                                  // VkPipelineShaderStageCreateFlags flags;
        stage,                                               // VkShaderStageFlagBits stage;
        module,                                              // VkShaderModule module;
        "main",                                              // const char* pName;
        specInfo,                                            // const VkSpecializationInfo* pSpecializationInfo;
    };

    vec.push_back(info);
}

tcu::TestStatus CooperativeVectorTestInstance::iterate(void)
{
    const DeviceInterface &vk             = m_context.getDeviceInterface();
    const VkDevice device                 = m_context.getDevice();
    Allocator &allocator                  = m_context.getDefaultAllocator();
    MemoryRequirement memoryDeviceAddress = MemoryRequirement::DeviceAddress;
    qpTestResult finalres                 = QP_TEST_RESULT_PASS;
    tcu::TestLog &log                     = m_context.getTestContext().getLog();

    uint32_t shaderGroupHandleSize    = 0;
    uint32_t shaderGroupBaseAlignment = 1;

    deRandom rnd;
    deRandom_init(&rnd, 1234);

    if (isRayTracingStageKHR(m_data.stage))
    {
        de::MovePtr<RayTracingProperties> rayTracingPropertiesKHR;

        rayTracingPropertiesKHR =
            makeRayTracingProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice());
        shaderGroupHandleSize    = rayTracingPropertiesKHR->getShaderGroupHandleSize();
        shaderGroupBaseAlignment = rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
    }

    VkPipelineBindPoint bindPoint;

    switch (m_data.stage)
    {
    case STAGE_COMPUTE:
        bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
        break;
    default:
        bindPoint = isRayTracingStageKHR(m_data.stage) ? VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR :
                                                         VK_PIPELINE_BIND_POINT_GRAPHICS;
        break;
    }

    {
        uint32_t N, K;
        N = m_data.outputVectorSize;
        K = m_data.inputVectorSize;

        log << tcu::TestLog::Message << "Testing N = " << N << ", K = " << K << tcu::TestLog::EndMessage;

        VkComponentTypeKHR dataTypes[4];
        for (uint32_t i = 0; i < 4; ++i)
        {
            if (isMatrixMul(m_data.testType))
            {
                dataTypes[i] = (i == 0) ? m_data.inputType : (i == 1) ? m_data.matrixType : m_data.outputType;
            }
            else
            {
                dataTypes[i] = (i < 2) ? m_data.inputType : m_data.outputType;
            }
        }

        uint32_t layerSizesRaw[3]   = {};
        uint32_t layerSizes[3]      = {};
        uint32_t matrixStride[3]    = {};
        uint32_t layerOffsets[3]    = {};
        uint32_t layerOffsetsRaw[3] = {};
        uint32_t numLayersInNetwork =
            (m_data.testType == TT_MATRIXMUL3 || m_data.testType == TT_MATRIXMUL2ADDMUL2) ? 3 : 1;
        uint32_t totalLayerSize = 0;

        for (uint32_t i = 0; i < numLayersInNetwork; ++i)
        {
            layerOffsetsRaw[i] = layerOffsets[i] = totalLayerSize;

            uint32_t numRows    = ((i == 1) ^ m_data.transpose) ? K : N;
            uint32_t numColumns = ((i == 1) ^ m_data.transpose) ? N : K;

            // Matrix size for matmul test types
            int matrixElementSize = getComponentTypeInfo(dataTypes[1]).bits / 8;
            uint32_t matrixSize, matrixSizeRaw;
            if (m_data.matrixLayout[i] == VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV)
            {
                matrixStride[i] = (numColumns * matrixElementSize + 16 - 1) & ~(16 - 1);
                matrixSize      = matrixStride[i] * numRows;

                totalLayerSize += matrixSize;
                layerSizesRaw[i] = layerSizes[i] = matrixSize;
            }
            else if (m_data.matrixLayout[i] == VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_COLUMN_MAJOR_NV)
            {
                matrixStride[i] = (numRows * matrixElementSize + 16 - 1) & ~(16 - 1);
                matrixSize      = matrixStride[i] * numColumns;

                totalLayerSize += matrixSize;
                layerSizesRaw[i] = layerSizes[i] = matrixSize;
            }
            else
            {
                matrixStride[i]  = (numColumns * matrixElementSize + 16 - 1) & ~(16 - 1);
                matrixSizeRaw    = matrixStride[i] * numRows;
                layerSizesRaw[i] = matrixSizeRaw;
                layerOffsets[i] += layerSizesRaw[i];

                layerOffsets[i] = (layerOffsets[i] + 63) & ~63;

                size_t dstSize = 0;

                VkConvertCooperativeVectorMatrixInfoNV info = {
                    VK_STRUCTURE_TYPE_CONVERT_COOPERATIVE_VECTOR_MATRIX_INFO_NV, // VkStructureType                       sType;
                    nullptr,           // void const*                           pNext;
                    layerSizesRaw[i],  // size_t                                srcSize;
                    {0},               // VkDeviceOrHostAddressConstKHR         srcData;
                    &dstSize,          // size_t*                               pDstSize;
                    {0},               // VkDeviceOrHostAddressKHR              dstData;
                    m_data.matrixType, // VkComponentTypeKHR                    srcComponentType;
                    m_data.matrixType, // VkComponentTypeKHR                    dstComponentType;
                    numRows,           // uint32_t                              numRows;
                    numColumns,        // uint32_t                              numColumns;
                    VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV, // VkCooperativeVectorMatrixLayoutNV     srcLayout;
                    matrixStride[i],        // size_t                                srcStride;
                    m_data.matrixLayout[i], // VkCooperativeVectorMatrixLayoutNV     dstLayout;
                    0,                      // size_t                                dstStride;
                };

                VK_CHECK(vk.convertCooperativeVectorMatrixNV(device, &info));
                layerSizes[i] = (uint32_t)dstSize;
                totalLayerSize += matrixSizeRaw + layerSizes[i];
            }
            totalLayerSize = (totalLayerSize + 63) & ~63;
        }

        int biasElementSize = getComponentTypeInfo(dataTypes[2]).bits / 8;
        uint32_t biasStride = (N * biasElementSize + 16 - 1) & ~(16 - 1);

        uint32_t elementsPer16B             = 16 * 8 / getComponentTypeInfo(m_data.inputType).bits;
        uint32_t inputVectorPaddedElements  = ((K + (elementsPer16B - 1)) & ~(elementsPer16B - 1));
        elementsPer16B                      = 16 * 8 / getComponentTypeInfo(m_data.outputType).bits;
        uint32_t outputVectorPaddedElements = ((N + (elementsPer16B - 1)) & ~(elementsPer16B - 1));

        uint32_t elementSize[4]{};
        VkDeviceSize bufferSizes[5];
        de::MovePtr<BufferWithMemory> buffers[5];
        vk::VkDescriptorBufferInfo bufferDescriptors[5];
        VkDeviceAddress bufferDeviceAddress[5];
        uint32_t totalElements[4] = {inputVectorPaddedElements, inputVectorPaddedElements, biasStride / biasElementSize,
                                     outputVectorPaddedElements};

        uint32_t totalInvocations =
            m_data.threadsPerWorkgroupX * m_data.threadsPerWorkgroupY * m_data.workgroupsX * m_data.workgroupsY;

        size_t outerProductSize = 0;
        if (m_data.testType == TT_OUTERPRODUCT)
        {
            VkConvertCooperativeVectorMatrixInfoNV info = {
                VK_STRUCTURE_TYPE_CONVERT_COOPERATIVE_VECTOR_MATRIX_INFO_NV, // VkStructureType                       sType;
                nullptr,           // void const*                           pNext;
                0,                 // size_t                                srcSize;
                {0},               // VkDeviceOrHostAddressConstKHR         srcData;
                &outerProductSize, // size_t*                               pDstSize;
                {0},               // VkDeviceOrHostAddressKHR              dstData;
                dataTypes[3],      // VkComponentTypeKHR                    srcComponentType;
                dataTypes[3],      // VkComponentTypeKHR                    dstComponentType;
                K,                 // uint32_t                              numRows;
                N,                 // uint32_t                              numColumns;
                VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV,  // VkCooperativeVectorMatrixLayoutNV     srcLayout;
                N * (getComponentTypeInfo(dataTypes[3]).bits / 8), // size_t                                srcStride;
                m_data.matrixLayout[0],                            // VkCooperativeVectorMatrixLayoutNV     dstLayout;
                0,                                                 // size_t                                dstStride;
            };

            VK_CHECK(vk.convertCooperativeVectorMatrixNV(device, &info));

            elementsPer16B             = 16 * 8 / getComponentTypeInfo(m_data.inputType).bits;
            outputVectorPaddedElements = ((N + (elementsPer16B - 1)) & ~(elementsPer16B - 1));
            totalElements[1]           = outputVectorPaddedElements;
            totalElements[3] = deDivRoundUp32((uint32_t)outerProductSize, getComponentTypeInfo(dataTypes[3]).bits / 8);
        }
        // Holds atomic flag bit for each invocation
        if (isTraining(m_data.testType))
        {
            totalElements[2] = 1;
        }

        for (uint32_t i = 0; i < 5; ++i)
        {
            if (i < 4)
            {
                elementSize[i] = getComponentTypeInfo(dataTypes[i]).bits / 8;

                if (isTraining(m_data.testType))
                {
                    elementSize[2] = 4;
                }

                if (i == 1 && isMatrixMul(m_data.testType))
                {
                    uint32_t numWeightSets =
                        (totalInvocations + nonuniformMatrixGroupSize - 1) / nonuniformMatrixGroupSize;
                    totalElements[i] = numWeightSets * totalLayerSize / elementSize[i];
                }
                else if ((m_data.testType == TT_MATRIXMAD || m_data.testType == TT_MATRIXMADTRANSPOSE) && (i == 2))
                {
                    uint32_t numBiasVectors =
                        (totalInvocations + nonuniformBiasGroupSize - 1) / nonuniformBiasGroupSize;
                    totalElements[i] = numBiasVectors * biasStride / elementSize[i];
                }
                else
                {
                    totalElements[i] *= totalInvocations;
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
                                                             VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT),
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
                                                             VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT),
                    MemoryRequirement::HostVisible | memoryDeviceAddress));
            }

            bufferDescriptors[i] = makeDescriptorBufferInfo(**buffers[i], 0, bufferSizes[i]);

            VkBufferDeviceAddressInfo info{
                VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, // VkStructureType  sType;
                nullptr,                                      // const void*  pNext;
                **buffers[i],                                 // VkBuffer            buffer
            };
            bufferDeviceAddress[i] = vk.getBufferDeviceAddress(device, &info);
        }

        void *ptrs[5];
        for (uint32_t i = 0; i < 5; ++i)
        {
            ptrs[i] = buffers[i]->getAllocation().getHostPtr();
        }

        const VkQueue queue             = m_context.getUniversalQueue();
        Move<VkCommandPool> cmdPool     = createCommandPool(vk, device, 0, m_context.getUniversalQueueFamilyIndex());
        Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        beginCommandBuffer(vk, *cmdBuffer, 0u);

        vk::DescriptorSetLayoutBuilder layoutBuilder;

        VkFlags allShaderStages = getAllShaderStagesFor(m_data.stage);

        layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);
        layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);
        layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);
        layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);
        layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, allShaderStages);

        if (usesAccelerationStructure(m_data.stage))
        {
            layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, allShaderStages);
        }

        vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(layoutBuilder.build(vk, device));

        vk::DescriptorPoolBuilder poolBuilder;
        poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5u);
        if (usesAccelerationStructure(m_data.stage))
        {
            poolBuilder.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1u);
        }

        vk::Unique<vk::VkDescriptorPool> descriptorPool(
            poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
        vk::Unique<vk::VkDescriptorSet> descriptorSet(
            makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

        vk::DescriptorSetUpdateBuilder setUpdateBuilder;
        if (m_data.storageClass == SC_PHYSICAL_STORAGE_BUFFER)
        {
            VkDeviceAddress *addrsInMemory = (VkDeviceAddress *)ptrs[4];
            for (uint32_t i = 0; i < 4; ++i)
            {
                addrsInMemory[i] = bufferDeviceAddress[i];
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

        // Create ray tracing structures
        de::MovePtr<vk::BottomLevelAccelerationStructure> bottomLevelAccelerationStructure;
        de::MovePtr<vk::TopLevelAccelerationStructure> topLevelAccelerationStructure;
        VkStridedDeviceAddressRegionKHR raygenShaderBindingTableRegion   = makeStridedDeviceAddressRegionKHR(0, 0, 0);
        VkStridedDeviceAddressRegionKHR missShaderBindingTableRegion     = makeStridedDeviceAddressRegionKHR(0, 0, 0);
        VkStridedDeviceAddressRegionKHR hitShaderBindingTableRegion      = makeStridedDeviceAddressRegionKHR(0, 0, 0);
        VkStridedDeviceAddressRegionKHR callableShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(0, 0, 0);

        if (usesAccelerationStructure(m_data.stage))
        {
            // Create bottom level acceleration structure
            {
                AccelerationStructBufferProperties bufferProps;
                bufferProps.props.residency = ResourceResidency::TRADITIONAL;

                bottomLevelAccelerationStructure = makeBottomLevelAccelerationStructure();

                bottomLevelAccelerationStructure->setDefaultGeometryData(getShaderStageFlag(m_data.stage));

                bottomLevelAccelerationStructure->createAndBuild(vk, device, *cmdBuffer, allocator, bufferProps);
            }

            // Create top level acceleration structure
            {
                AccelerationStructBufferProperties bufferProps;
                bufferProps.props.residency   = ResourceResidency::TRADITIONAL;
                topLevelAccelerationStructure = makeTopLevelAccelerationStructure();

                topLevelAccelerationStructure->setInstanceCount(1);
                topLevelAccelerationStructure->addInstance(
                    de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));

                topLevelAccelerationStructure->createAndBuild(vk, device, *cmdBuffer, allocator, bufferProps);
            }

            VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureWriteDescriptorSet = {
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR, //  VkStructureType sType;
                nullptr,                                                           //  const void* pNext;
                1,                                       //  uint32_t accelerationStructureCount;
                topLevelAccelerationStructure->getPtr(), //  const VkAccelerationStructureKHR* pAccelerationStructures;
            };

            setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(5),
                                         VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                                         &accelerationStructureWriteDescriptorSet);
        }

        setUpdateBuilder.update(vk, device);

        const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
            nullptr,                                       // pNext
            (VkPipelineLayoutCreateFlags)0,
            1,                          // setLayoutCount
            &descriptorSetLayout.get(), // pSetLayouts
            0u,                         // pushConstantRangeCount
            nullptr,                    // pPushConstantRanges
        };

        Move<VkPipelineLayout> pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);

        de::MovePtr<BufferWithMemory> sbtBuffer;
        de::MovePtr<BufferWithMemory> raygenShaderBindingTable;
        de::MovePtr<BufferWithMemory> missShaderBindingTable;
        de::MovePtr<BufferWithMemory> hitShaderBindingTable;
        de::MovePtr<BufferWithMemory> callableShaderBindingTable;
        de::MovePtr<RayTracingPipeline> rayTracingPipeline;

        const uint32_t specData[] = {
            m_data.threadsPerWorkgroupX,
            m_data.threadsPerWorkgroupY,
            totalLayerSize,
            layerOffsets[0],
            layerOffsets[1],
            layerOffsets[2],
            m_data.threadsPerWorkgroupX * m_data.workgroupsX,
            (uint32_t)outerProductSize,
        };

        const vk::VkSpecializationMapEntry entries[] = {
            {0, (uint32_t)(sizeof(uint32_t) * 0), sizeof(uint32_t)},
            {1, (uint32_t)(sizeof(uint32_t) * 1), sizeof(uint32_t)},
            {2, (uint32_t)(sizeof(uint32_t) * 2), sizeof(uint32_t)},
            {3, (uint32_t)(sizeof(uint32_t) * 3), sizeof(uint32_t)},
            {4, (uint32_t)(sizeof(uint32_t) * 4), sizeof(uint32_t)},
            {5, (uint32_t)(sizeof(uint32_t) * 5), sizeof(uint32_t)},
            {6, (uint32_t)(sizeof(uint32_t) * 6), sizeof(uint32_t)},
            {7, (uint32_t)(sizeof(uint32_t) * 7), sizeof(uint32_t)},
        };

        const vk::VkSpecializationInfo specInfo = {
            sizeof(specData) / sizeof(specData[0]), // mapEntryCount
            entries,                                // pMapEntries
            sizeof(specData),                       // dataSize
            specData                                // pData
        };

        for (uint32_t i = 0; i < 4; ++i)
        {
            for (uint32_t j = 0; j < totalElements[i]; ++j)
            {
                if (isFloatType(dataTypes[i]))
                {
                    if (!isMatrixMul(m_data.testType) && !isTraining(m_data.testType) && m_data.testType != TT_MUL &&
                        m_data.testType != TT_FMA)
                        setDataFloat(ptrs[i], dataTypes[i], j,
                                     ((float)(deRandom_getUint32(&rnd) & 0xff) - 64.0f) / 2.0f);
                    else if (m_data.testType == TT_MATRIXMUL3 || m_data.testType == TT_MATRIXMUL2ADDMUL2 ||
                             isTraining(m_data.testType))
                        setDataFloat(ptrs[i], dataTypes[i], j, ((float)(deRandom_getUint32(&rnd) & 0x3) - 1.0f) / 2.0f);
                    else if (i == 0 && !isFloatType(m_data.inputInterpretation))
                    {
                        setDataFloat(ptrs[i], dataTypes[i], j, ((float)(deRandom_getUint32(&rnd) & 0x7) - 3.0f));
                    }
                    else
                    {
                        setDataFloat(ptrs[i], dataTypes[i], j, ((float)(deRandom_getUint32(&rnd) & 0xf) - 4.0f) / 2.0f);
                    }
                    if (isTraining(m_data.testType) && i >= 2)
                    {
                        setDataFloat(ptrs[i], dataTypes[i], j, 0.0f);
                    }
                }
                else
                {
                    int bias = -128;
                    // Don't generate huge uint values that will overflow fp16
                    if (!isSIntType(dataTypes[i]) && dataTypes[3] == VK_COMPONENT_TYPE_FLOAT16_NV)
                    {
                        bias = 0;
                    }
                    setDataInt(ptrs[i], dataTypes[i], j, (deRandom_getUint32(&rnd) & 0xff) + bias);
                }
            }
        }

        uint32_t numWeightSets = (totalInvocations + nonuniformMatrixGroupSize - 1) / nonuniformMatrixGroupSize;
        if (isMatrixMul(m_data.testType))
        {
            for (uint32_t i = 0; i < numLayersInNetwork; ++i)
            {
                if (m_data.matrixLayout[i] == VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_INFERENCING_OPTIMAL_NV ||
                    m_data.matrixLayout[i] == VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_TRAINING_OPTIMAL_NV)
                {

                    uint32_t numRows    = ((i == 1) ^ m_data.transpose) ? K : N;
                    uint32_t numColumns = ((i == 1) ^ m_data.transpose) ? N : K;

                    size_t dstSize = layerSizes[i];

                    VkConvertCooperativeVectorMatrixInfoNV info = {
                        VK_STRUCTURE_TYPE_CONVERT_COOPERATIVE_VECTOR_MATRIX_INFO_NV, // VkStructureType                       sType;
                        nullptr,           // void const*                           pNext;
                        layerSizesRaw[i],  // size_t                                srcSize;
                        {0},               // VkDeviceOrHostAddressConstKHR         srcData;
                        &dstSize,          // size_t*                               pDstSize;
                        {0},               // VkDeviceOrHostAddressKHR              dstData;
                        m_data.matrixType, // VkComponentTypeKHR                    srcComponentType;
                        m_data.matrixType, // VkComponentTypeKHR                    dstComponentType;
                        numRows,           // uint32_t                              numRows;
                        numColumns,        // uint32_t                              numColumns;
                        VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV, // VkCooperativeVectorMatrixLayoutNV     srcLayout;
                        matrixStride[i],        // size_t                                srcStride;
                        m_data.matrixLayout[i], // VkCooperativeVectorMatrixLayoutNV     dstLayout;
                        0,                      // size_t                                dstStride;
                    };

                    bool deviceConvert = (N > 20) && (m_data.testType != TT_MATRIXMUL_TRAININGBIAS);

                    vector<VkConvertCooperativeVectorMatrixInfoNV> infos(numWeightSets, info);
                    for (uint32_t w = 0; w < numWeightSets; ++w)
                    {
                        uint32_t offsetRaw = w * totalLayerSize + layerOffsetsRaw[i];
                        uint32_t offsetOpt = w * totalLayerSize + layerOffsets[i];

                        DE_ASSERT(offsetOpt + dstSize <= bufferSizes[1]);

                        if (deviceConvert)
                        {
                            info.srcData.deviceAddress = bufferDeviceAddress[1] + offsetRaw;
                            info.dstData.deviceAddress = bufferDeviceAddress[1] + offsetOpt;
                        }
                        else
                        {
                            info.srcData.hostAddress = (uint8_t *)(ptrs[1]) + offsetRaw;
                            info.dstData.hostAddress = (uint8_t *)(ptrs[1]) + offsetOpt;
                            VK_CHECK(vk.convertCooperativeVectorMatrixNV(device, &info));

                            if (m_data.testType == TT_MATRIXMUL_TRAININGBIAS)
                            {
                                // Add a component-wise bias to each element, even padding elements.
                                // This is to test that padding values don't affect the results.
                                uint32_t numElements =
                                    (uint32_t)dstSize / (getComponentTypeInfo(m_data.matrixType).bits / 8);
                                for (uint32_t e = 0; e < numElements; ++e)
                                {
                                    DE_ASSERT(isFloatType(m_data.matrixType));
                                    float f = getDataFloat(info.dstData.hostAddress, m_data.matrixType, e);
                                    f += 1.0f;
                                    setDataFloat(info.dstData.hostAddress, m_data.matrixType, e, f);
                                }
                            }
                        }
                        infos[w] = info;
                    }

                    if (deviceConvert)
                    {
                        vk.cmdConvertCooperativeVectorMatrixNV(*cmdBuffer, numWeightSets, infos.data());

                        VkMemoryBarrier2KHR memoryBarrier = {
                            VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR,                       // sType
                            nullptr,                                                      // pNext
                            VK_PIPELINE_STAGE_2_CONVERT_COOPERATIVE_VECTOR_MATRIX_BIT_NV, // srcStageMask
                            VK_ACCESS_2_TRANSFER_WRITE_BIT,                               // srcAccessMask
                            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,                         // dstStageMask
                            VK_ACCESS_2_SHADER_READ_BIT                                   // dstAccessMask
                        };

                        VkDependencyInfoKHR dependencyInfo{
                            VK_STRUCTURE_TYPE_DEPENDENCY_INFO, // sType
                            nullptr,                           // pNext
                            0,                                 //dependency flags
                            1,                                 //memory barrier count
                            &memoryBarrier,                    //memory barrier
                            0,                                 // bufferMemoryBarrierCount
                            nullptr,                           // pBufferMemoryBarriers
                            0,                                 // imageMemoryBarrierCount
                            nullptr,                           // pImageMemoryBarriers
                        };
                        vk.cmdPipelineBarrier2(*cmdBuffer, &dependencyInfo);
                    }
                }
            }
        }

        flushAlloc(vk, device, buffers[0]->getAllocation());
        flushAlloc(vk, device, buffers[1]->getAllocation());
        flushAlloc(vk, device, buffers[2]->getAllocation());
        flushAlloc(vk, device, buffers[3]->getAllocation());

        Move<VkPipeline> pipeline;
        Move<VkRenderPass> renderPass;
        Move<VkFramebuffer> framebuffer;

        const void *pNext = nullptr;
#ifndef CTS_USES_VULKANSC
        VkPipelineCreateFlags2CreateInfo pipelineFlags2CreateInfo = initVulkanStructure();
        if (m_data.uses64BitIndexing)
        {
            pipelineFlags2CreateInfo.flags = VK_PIPELINE_CREATE_2_64_BIT_INDEXING_BIT_EXT;
            pNext                          = &pipelineFlags2CreateInfo;
        }
#endif

        if (m_data.stage == STAGE_COMPUTE)
        {
            const Unique<VkShaderModule> shader(
                createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0));

            const VkPipelineShaderStageCreateInfo shaderCreateInfo = {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                nullptr,
                (VkPipelineShaderStageCreateFlags)0,
                VK_SHADER_STAGE_COMPUTE_BIT, // stage
                *shader,                     // shader
                "main",
                &specInfo, // pSpecializationInfo
            };

            // Enable robustness for ACT_LOAD_READONLY pipelines, if supported
            VkPipelineRobustnessCreateInfoEXT robustnessCreateInfo = initVulkanStructure();
            if (m_data.act0 == ACT_LOAD_READONLY && m_context.getPipelineRobustnessFeatures().pipelineRobustness)
            {
                robustnessCreateInfo.pNext          = pNext;
                robustnessCreateInfo.storageBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2;
                pNext                               = &robustnessCreateInfo;
            }

            const VkComputePipelineCreateInfo pipelineCreateInfo = {
                VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                pNext,
                0u,               // flags
                shaderCreateInfo, // cs
                *pipelineLayout,  // layout
                VK_NULL_HANDLE,   // basePipelineHandle
                0u,               // basePipelineIndex
            };
            pipeline = createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo, NULL);
        }
        else if (m_data.stage == STAGE_RAYGEN)
        {
            rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

            rayTracingPipeline->addShader(
                VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0), 0, &specInfo);

            pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout, {}, VK_NULL_HANDLE, pNext);

            raygenShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
                vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
            raygenShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(
                getBufferDeviceAddress(vk, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize,
                shaderGroupHandleSize);
        }
        else if (m_data.stage == STAGE_INTERSECT)
        {
            rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

            rayTracingPipeline->addShader(
                VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                createShaderModule(vk, device, m_context.getBinaryCollection().get("rgen"), 0), 0, &specInfo);
            rayTracingPipeline->addShader(
                VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
                createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0), 1, &specInfo);

            pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout, {}, VK_NULL_HANDLE, pNext);

            raygenShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
                vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
            raygenShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(
                getBufferDeviceAddress(vk, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize,
                shaderGroupHandleSize);

            hitShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
                vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
            hitShaderBindingTableRegion =
                makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, hitShaderBindingTable->get(), 0),
                                                  shaderGroupHandleSize, shaderGroupHandleSize);
        }
        else if (m_data.stage == STAGE_ANY_HIT)
        {
            rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

            rayTracingPipeline->addShader(
                VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                createShaderModule(vk, device, m_context.getBinaryCollection().get("rgen"), 0), 0, &specInfo);
            rayTracingPipeline->addShader(
                VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
                createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0), 1, &specInfo);

            pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout, {}, VK_NULL_HANDLE, pNext);

            raygenShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
                vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
            raygenShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(
                getBufferDeviceAddress(vk, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize,
                shaderGroupHandleSize);

            hitShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
                vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
            hitShaderBindingTableRegion =
                makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, hitShaderBindingTable->get(), 0),
                                                  shaderGroupHandleSize, shaderGroupHandleSize);
        }
        else if (m_data.stage == STAGE_CLOSEST_HIT)
        {
            rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

            rayTracingPipeline->addShader(
                VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                createShaderModule(vk, device, m_context.getBinaryCollection().get("rgen"), 0), 0, &specInfo);
            rayTracingPipeline->addShader(
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0), 1, &specInfo);

            pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout, {}, VK_NULL_HANDLE, pNext);

            raygenShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
                vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
            raygenShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(
                getBufferDeviceAddress(vk, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize,
                shaderGroupHandleSize);

            hitShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
                vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
            hitShaderBindingTableRegion =
                makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, hitShaderBindingTable->get(), 0),
                                                  shaderGroupHandleSize, shaderGroupHandleSize);
        }
        else if (m_data.stage == STAGE_MISS)
        {
            rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

            rayTracingPipeline->addShader(
                VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                createShaderModule(vk, device, m_context.getBinaryCollection().get("rgen"), 0), 0, &specInfo);
            rayTracingPipeline->addShader(
                VK_SHADER_STAGE_MISS_BIT_KHR,
                createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0), 1, &specInfo);

            pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout, {}, VK_NULL_HANDLE, pNext);

            raygenShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
                vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
            raygenShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(
                getBufferDeviceAddress(vk, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize,
                shaderGroupHandleSize);

            missShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
                vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
            missShaderBindingTableRegion =
                makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, missShaderBindingTable->get(), 0),
                                                  shaderGroupHandleSize, shaderGroupHandleSize);
        }
        else if (m_data.stage == STAGE_CALLABLE)
        {
            rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

            rayTracingPipeline->addShader(
                VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                createShaderModule(vk, device, m_context.getBinaryCollection().get("rgen"), 0), 0, &specInfo);
            rayTracingPipeline->addShader(
                VK_SHADER_STAGE_CALLABLE_BIT_KHR,
                createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0), 1, &specInfo);

            pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout, {}, VK_NULL_HANDLE, pNext);

            raygenShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
                vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
            raygenShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(
                getBufferDeviceAddress(vk, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize,
                shaderGroupHandleSize);

            callableShaderBindingTable = rayTracingPipeline->createShaderBindingTable(
                vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
            callableShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(
                getBufferDeviceAddress(vk, device, callableShaderBindingTable->get(), 0), shaderGroupHandleSize,
                shaderGroupHandleSize);
        }
        else
        {

            const VkSubpassDescription subpassDesc = {
                (VkSubpassDescriptionFlags)0,    // VkSubpassDescriptionFlags    flags
                VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint            pipelineBindPoint
                0u,                              // uint32_t                        inputAttachmentCount
                nullptr,                         // const VkAttachmentReference*    pInputAttachments
                0u,                              // uint32_t                        colorAttachmentCount
                nullptr,                         // const VkAttachmentReference*    pColorAttachments
                nullptr,                         // const VkAttachmentReference*    pResolveAttachments
                nullptr,                         // const VkAttachmentReference*    pDepthStencilAttachment
                0u,                              // uint32_t                        preserveAttachmentCount
                nullptr                          // const uint32_t*                pPreserveAttachments
            };

            const VkRenderPassCreateInfo renderPassParams = {
                VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureTypei                    sType
                nullptr,                                   // const void*                        pNext
                (VkRenderPassCreateFlags)0,                // VkRenderPassCreateFlags            flags
                0u,                                        // uint32_t                            attachmentCount
                nullptr,                                   // const VkAttachmentDescription*    pAttachments
                1u,                                        // uint32_t                            subpassCount
                &subpassDesc,                              // const VkSubpassDescription*        pSubpasses
                0u,                                        // uint32_t                            dependencyCount
                nullptr                                    // const VkSubpassDependency*        pDependencies
            };

            renderPass = createRenderPass(vk, device, &renderPassParams);

            const vk::VkFramebufferCreateInfo framebufferParams = {
                vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // sType
                nullptr,                                       // pNext
                (vk::VkFramebufferCreateFlags)0,
                *renderPass,                                      // renderPass
                0U,                                               // attachmentCount
                nullptr,                                          // pAttachments
                m_data.threadsPerWorkgroupX * m_data.workgroupsX, // width
                m_data.threadsPerWorkgroupY * m_data.workgroupsY, // height
                1u,                                               // layers
            };

            framebuffer = createFramebuffer(vk, device, &framebufferParams);

            // Note: vertex input state and input assembly state will not be used for mesh pipelines.

            const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
                VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                                   // const void* pNext;
                (VkPipelineVertexInputStateCreateFlags)0, // VkPipelineVertexInputStateCreateFlags flags;
                0u,                                       // uint32_t vertexBindingDescriptionCount;
                nullptr, // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
                0u,      // uint32_t vertexAttributeDescriptionCount;
                nullptr  // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
            };

            const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
                VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                                     // const void* pNext;
                (VkPipelineInputAssemblyStateCreateFlags)0, // VkPipelineInputAssemblyStateCreateFlags flags;
                (m_data.stage == STAGE_VERTEX) ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST :
                (m_data.stage == STAGE_TESS_CTRL || m_data.stage == STAGE_TESS_EVAL) ?
                                                 VK_PRIMITIVE_TOPOLOGY_PATCH_LIST :
                                                 VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // VkPrimitiveTopology topology;
                VK_FALSE // VkBool32 primitiveRestartEnable;
            };

            const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
                VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                                    // const void* pNext;
                (VkPipelineRasterizationStateCreateFlags)0,            // VkPipelineRasterizationStateCreateFlags flags;
                VK_FALSE,                                              // VkBool32 depthClampEnable;
                (m_data.stage != STAGE_FRAGMENT) ? VK_TRUE : VK_FALSE, // VkBool32 rasterizerDiscardEnable;
                VK_POLYGON_MODE_FILL,                                  // VkPolygonMode polygonMode;
                VK_CULL_MODE_NONE,                                     // VkCullModeFlags cullMode;
                VK_FRONT_FACE_CLOCKWISE,                               // VkFrontFace frontFace;
                VK_FALSE,                                              // VkBool32 depthBiasEnable;
                0.0f,                                                  // float depthBiasConstantFactor;
                0.0f,                                                  // float depthBiasClamp;
                0.0f,                                                  // float depthBiasSlopeFactor;
                1.0f                                                   // float lineWidth;
            };

            const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
                VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType                            sType
                nullptr,               // const void*                                pNext
                0u,                    // VkPipelineMultisampleStateCreateFlags    flags
                VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits                    rasterizationSamples
                VK_FALSE,              // VkBool32                                    sampleShadingEnable
                1.0f,                  // float                                    minSampleShading
                nullptr,               // const VkSampleMask*                        pSampleMask
                VK_FALSE,              // VkBool32                                    alphaToCoverageEnable
                VK_FALSE               // VkBool32                                    alphaToOneEnable
            };

            VkViewport viewport = makeViewport(m_data.threadsPerWorkgroupX * m_data.workgroupsX,
                                               m_data.threadsPerWorkgroupY * m_data.workgroupsY);
            VkRect2D scissor    = makeRect2D(m_data.threadsPerWorkgroupX * m_data.workgroupsX,
                                             m_data.threadsPerWorkgroupY * m_data.workgroupsY);

            const VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
                VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType                            sType
                nullptr,                               // const void*                                pNext
                (VkPipelineViewportStateCreateFlags)0, // VkPipelineViewportStateCreateFlags        flags
                1u,                                    // uint32_t                                    viewportCount
                &viewport,                             // const VkViewport*                        pViewports
                1u,                                    // uint32_t                                    scissorCount
                &scissor                               // const VkRect2D*                            pScissors
            };

            const VkPipelineTessellationStateCreateInfo tessellationStateCreateInfo = {
                VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                                   // const void* pNext;
                0u,                          // VkPipelineTessellationStateCreateFlags flags;
                m_data.threadsPerWorkgroupX, // uint32_t patchControlPoints;
            };

            Move<VkShaderModule> fs;
            Move<VkShaderModule> vs;
            Move<VkShaderModule> tcs;
            Move<VkShaderModule> tes;
            Move<VkShaderModule> gs;
            Move<VkShaderModule> ms;
            Move<VkShaderModule> ts;

            const auto &binaries = m_context.getBinaryCollection();

            std::vector<VkPipelineShaderStageCreateInfo> stageCreateInfos;

            if (m_data.stage == STAGE_VERTEX)
            {
                vs = createShaderModule(vk, device, binaries.get("test"));
                appendShaderStageCreateInfo(stageCreateInfos, vs.get(), VK_SHADER_STAGE_VERTEX_BIT, &specInfo);
            }
            else if (m_data.stage == STAGE_FRAGMENT)
            {
                vs = createShaderModule(vk, device, binaries.get("vert"));
                fs = createShaderModule(vk, device, binaries.get("test"));
                appendShaderStageCreateInfo(stageCreateInfos, vs.get(), VK_SHADER_STAGE_VERTEX_BIT, &specInfo);
                appendShaderStageCreateInfo(stageCreateInfos, fs.get(), VK_SHADER_STAGE_FRAGMENT_BIT, &specInfo);
            }
            else if (m_data.stage == STAGE_GEOMETRY)
            {
                vs = createShaderModule(vk, device, binaries.get("vert"));
                gs = createShaderModule(vk, device, binaries.get("test"));
                appendShaderStageCreateInfo(stageCreateInfos, vs.get(), VK_SHADER_STAGE_VERTEX_BIT, &specInfo);
                appendShaderStageCreateInfo(stageCreateInfos, gs.get(), VK_SHADER_STAGE_GEOMETRY_BIT, &specInfo);
            }
            else if (m_data.stage == STAGE_TESS_CTRL || m_data.stage == STAGE_TESS_EVAL)
            {
                vs  = createShaderModule(vk, device, binaries.get("vert"));
                tcs = createShaderModule(vk, device, binaries.get("tesc"));
                tes = createShaderModule(vk, device, binaries.get("tese"));
                appendShaderStageCreateInfo(stageCreateInfos, vs.get(), VK_SHADER_STAGE_VERTEX_BIT, &specInfo);
                appendShaderStageCreateInfo(stageCreateInfos, tcs.get(), VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                                            &specInfo);
                appendShaderStageCreateInfo(stageCreateInfos, tes.get(), VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                                            &specInfo);
            }
            else if (m_data.stage == STAGE_TASK)
            {
                ts = createShaderModule(vk, device, binaries.get("test"));
                ms = createShaderModule(vk, device, binaries.get("mesh"));
                appendShaderStageCreateInfo(stageCreateInfos, ts.get(), vk::VK_SHADER_STAGE_TASK_BIT_EXT, &specInfo);
                appendShaderStageCreateInfo(stageCreateInfos, ms.get(), VK_SHADER_STAGE_MESH_BIT_EXT, &specInfo);
            }
            else if (m_data.stage == STAGE_MESH)
            {
                ms = createShaderModule(vk, device, binaries.get("test"));
                appendShaderStageCreateInfo(stageCreateInfos, ms.get(), VK_SHADER_STAGE_MESH_BIT_EXT, &specInfo);
            }

            const VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
                VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, // VkStructureType sType;
                pNext,                                           // const void* pNext;
                (VkPipelineCreateFlags)0,                        // VkPipelineCreateFlags flags;
                static_cast<uint32_t>(stageCreateInfos.size()),  // uint32_t stageCount;
                de::dataOrNull(stageCreateInfos),                // const VkPipelineShaderStageCreateInfo* pStages;
                &vertexInputStateCreateInfo,   // const VkPipelineVertexInputStateCreateInfo* pVertexInputState;
                &inputAssemblyStateCreateInfo, // const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState;
                &tessellationStateCreateInfo,  // const VkPipelineTessellationStateCreateInfo* pTessellationState;
                &viewportStateCreateInfo,      // const VkPipelineViewportStateCreateInfo* pViewportState;
                &rasterizationStateCreateInfo, // const VkPipelineRasterizationStateCreateInfo* pRasterizationState;
                &multisampleStateCreateInfo,   // const VkPipelineMultisampleStateCreateInfo* pMultisampleState;
                nullptr,                       // const VkPipelineDepthStencilStateCreateInfo* pDepthStencilState;
                nullptr,                       // const VkPipelineColorBlendStateCreateInfo* pColorBlendState;
                nullptr,                       // const VkPipelineDynamicStateCreateInfo* pDynamicState;
                pipelineLayout.get(),          // VkPipelineLayout layout;
                renderPass.get(),              // VkRenderPass renderPass;
                0u,                            // uint32_t subpass;
                VK_NULL_HANDLE,                // VkPipeline basePipelineHandle;
                0                              // int basePipelineIndex;
            };

            pipeline = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &graphicsPipelineCreateInfo);
        }

        vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *pipelineLayout, 0u, 1, &*descriptorSet, 0u, nullptr);
        vk.cmdBindPipeline(*cmdBuffer, bindPoint, *pipeline);

        if (m_data.stage == STAGE_COMPUTE)
        {
            vk.cmdDispatch(*cmdBuffer, m_data.workgroupsX, m_data.workgroupsY, 1);
        }
        else if (isRayTracingStageKHR(m_data.stage))
        {
            cmdTraceRays(vk, *cmdBuffer, &raygenShaderBindingTableRegion, &missShaderBindingTableRegion,
                         &hitShaderBindingTableRegion, &callableShaderBindingTableRegion,
                         m_data.workgroupsX * m_data.threadsPerWorkgroupX,
                         m_data.workgroupsY * m_data.threadsPerWorkgroupY, 1);
        }
        else
        {
            beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer,
                            makeRect2D(m_data.threadsPerWorkgroupX * m_data.workgroupsX,
                                       m_data.threadsPerWorkgroupY * m_data.workgroupsY),
                            0, nullptr, VK_SUBPASS_CONTENTS_INLINE);
            // Draw a point cloud for vertex shader testing, points forming patches for tessellation testing,
            // and a single quad for fragment shader testing
            if (m_data.stage == STAGE_VERTEX || m_data.stage == STAGE_TESS_CTRL || m_data.stage == STAGE_TESS_EVAL)
            {
                vk.cmdDraw(*cmdBuffer,
                           m_data.threadsPerWorkgroupX * m_data.workgroupsX * m_data.threadsPerWorkgroupY *
                               m_data.workgroupsY,
                           1u, 0u, 0u);
            }
            else if (m_data.stage == STAGE_GEOMETRY)
            {
                // Topology is triangle strips, so launch N+2 vertices to form N triangles.
                vk.cmdDraw(*cmdBuffer, m_data.workgroupsX * m_data.workgroupsY + 2u, 1u, 0u, 0u);
            }
            else if (m_data.stage == STAGE_FRAGMENT)
            {
                vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);
            }
            else if (isMeshStage(m_data.stage))
            {
                vk.cmdDrawMeshTasksEXT(*cmdBuffer, m_data.workgroupsX, m_data.workgroupsY, 1u);
            }
            endRenderPass(vk, *cmdBuffer);
        }

        endCommandBuffer(vk, *cmdBuffer);

        submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

        invalidateAlloc(vk, device, buffers[3]->getAllocation());

        qpTestResult res = QP_TEST_RESULT_PASS;

        if (isFloatType(dataTypes[3]))
        {
            if (m_data.testType == TT_OUTERPRODUCT)
            {
                uint32_t numInvocations = totalInvocations;
                for (uint32_t i = 0; i < numInvocations; ++i)
                {
                    size_t dstSize = 0;

                    VkConvertCooperativeVectorMatrixInfoNV info = {
                        VK_STRUCTURE_TYPE_CONVERT_COOPERATIVE_VECTOR_MATRIX_INFO_NV, // VkStructureType                       sType;
                        nullptr,                // void const*                           pNext;
                        (size_t)bufferSizes[3], // size_t                                srcSize;
                        {0},                    // VkDeviceOrHostAddressConstKHR         srcData;
                        &dstSize,               // size_t*                               pDstSize;
                        {0},                    // VkDeviceOrHostAddressKHR              dstData;
                        dataTypes[3],           // VkComponentTypeKHR                    srcComponentType;
                        dataTypes[3],           // VkComponentTypeKHR                    dstComponentType;
                        K,                      // uint32_t                              numRows;
                        N,                      // uint32_t                              numColumns;
                        m_data.matrixLayout[0], // VkCooperativeVectorMatrixLayoutNV     srcLayout;
                        0,                      // size_t                                srcStride;
                        VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV, // VkCooperativeVectorMatrixLayoutNV     dstLayout;
                        N * elementSize[3], // size_t                                dstStride;
                    };

                    VK_CHECK(vk.convertCooperativeVectorMatrixNV(device, &info));

                    uint32_t index = 0;
                    switch (m_data.resultAddr)
                    {
                    case RESULT_ADDR_UNIFORM:
                        index = 1;
                        break;
                    case RESULT_ADDR_UNIQUE:
                        index = i;
                        break;
                    case RESULT_ADDR_CLUSTERED:
                        index = i / 5;
                        break;
                    default:
                        DE_ASSERT(0);
                        break;
                    }

                    std::vector<uint8_t> readBack(dstSize);
                    info.srcData.hostAddress = (const uint8_t *)ptrs[3] + outerProductSize * index;
                    info.dstData.hostAddress = readBack.data();
                    VK_CHECK(vk.convertCooperativeVectorMatrixNV(device, &info));

                    float output;
                    switch (m_data.resultAddr)
                    {
                    case RESULT_ADDR_UNIFORM:
                    {
                        for (uint32_t k = 0; k < K; ++k)
                        {
                            for (uint32_t n = 0; n < N; ++n)
                            {
                                output    = getDataFloatOffsetIndex(readBack.data(), dataTypes[3], 0, k * N + n);
                                float ref = 0;
                                for (uint32_t inv = 0; inv < numInvocations; ++inv)
                                {
                                    float Ak = getDataFloat(ptrs[0], dataTypes[0], inv * inputVectorPaddedElements + k);
                                    float Bn =
                                        getDataFloat(ptrs[1], dataTypes[1], inv * outputVectorPaddedElements + n);
                                    ref += Ak * Bn;
                                }
                                if (output != ref)
                                {
                                    //printf("i %d k %d n %d ref %f output %f\n", i, k, n, ref, output);
                                    res = QP_TEST_RESULT_FAIL;
                                }
                            }
                        }
                        // The i loop is unnecessary, skip the remaining iterations
                        i = numInvocations - 1;
                    }
                    break;
                    case RESULT_ADDR_UNIQUE:
                    {
                        for (uint32_t k = 0; k < K; ++k)
                        {
                            for (uint32_t n = 0; n < N; ++n)
                            {
                                output    = getDataFloatOffsetIndex(readBack.data(), dataTypes[3], 0, k * N + n);
                                float Ak  = getDataFloat(ptrs[0], dataTypes[0], i * inputVectorPaddedElements + k);
                                float Bn  = getDataFloat(ptrs[1], dataTypes[1], i * outputVectorPaddedElements + n);
                                float ref = Ak * Bn;
                                if (output != ref)
                                {
                                    //printf("i %d k %d n %d ref %f output %f\n", i, k, n, ref, output);
                                    res = QP_TEST_RESULT_FAIL;
                                }
                            }
                        }
                    }
                    break;
                    case RESULT_ADDR_CLUSTERED:
                    {
                        for (uint32_t k = 0; k < K; ++k)
                        {
                            for (uint32_t n = 0; n < N; ++n)
                            {
                                output    = getDataFloatOffsetIndex(readBack.data(), dataTypes[3], 0, k * N + n);
                                float ref = 0;
                                for (uint32_t inv = (i / 5) * 5; inv < (i / 5 + 1) * 5; ++inv)
                                {
                                    if (inv < numInvocations)
                                    {
                                        float Ak =
                                            getDataFloat(ptrs[0], dataTypes[0], inv * inputVectorPaddedElements + k);
                                        float Bn =
                                            getDataFloat(ptrs[1], dataTypes[1], inv * outputVectorPaddedElements + n);
                                        ref += Ak * Bn;
                                    }
                                }
                                if (output != ref)
                                {
                                    //printf("i %d k %d n %d ref %f output %f\n", i, k, n, ref, output);
                                    res = QP_TEST_RESULT_FAIL;
                                }
                            }
                        }
                    }
                    break;
                    }
                }
            }
            else if (!isMatrixMul(m_data.testType))
            {
                uint32_t numInvocations = totalInvocations;
                for (uint32_t i = 0; i < numInvocations; ++i)
                {
                    for (uint32_t j = 0; j < N; ++j)
                    {
                        float inputA, inputB, output;
                        if (isFloatType(dataTypes[0]))
                        {
                            inputA = getDataFloat(ptrs[0], dataTypes[0], i * inputVectorPaddedElements + j);
                            inputB = getDataFloat(ptrs[1], dataTypes[1], i * inputVectorPaddedElements + j);
                        }
                        else
                        {
                            inputA = (float)getDataInt(ptrs[0], dataTypes[0], i * inputVectorPaddedElements + j);
                            inputB = (float)getDataInt(ptrs[1], dataTypes[1], i * inputVectorPaddedElements + j);
                        }
                        output = getDataFloat(ptrs[3], dataTypes[3], i * outputVectorPaddedElements + j);
                        switch (m_data.testType)
                        {
                        case TT_LENGTH:
                            if (output != (float)K)
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        case TT_CONSTANT:
                            if (output != 1.0f)
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        case TT_CONVERT:
                            if (output != inputA)
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        case TT_COMPOSITE:
                        case TT_COMPOSITE_RVALUE:
                        case TT_COMPOSITE_ARRAY:
                        case TT_ADD:
                            if (output != inputA + inputB)
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        case TT_VECTOR_EXTRACT:
                            if (output != inputA + inputB + 1.0f)
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        case TT_SUB:
                            if (output != inputA - inputB)
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        case TT_MUL:
                            if (output != inputA * inputB)
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        case TT_DIV:
                        {
                            float ulp = (m_data.inputType == VK_COMPONENT_TYPE_FLOAT16_NV) ?
                                            1.0f / 1024.0f :
                                            1.0f / (8.0f * 1024.0f * 1024.0f);
                            // division allows 2.5ulp, but we'll use 3.
                            ulp *= 3;
                            if (inputB != 0 && fabs(output - inputA / inputB) > ulp * fabs(inputA / inputB))
                                res = QP_TEST_RESULT_FAIL;
                        }
                        break;
                        case TT_NEGATE:
                        case TT_FUNC:
                            if (output != -inputA)
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        case TT_VECTORTIMESSCALAR:
                            if (output != 6.0 * inputA)
                                res = QP_TEST_RESULT_FAIL;
                            break;

                        case TT_EXP:
                        {
                            float ref = expf(inputA * 0.0625f);
                            if (fabs(output - ref) / ref > 0.01)
                            {
                                //printf("ref %f output %f\n", ref, output);
                                res = QP_TEST_RESULT_FAIL;
                            }
                            break;
                        }
                        case TT_LOG:
                        {
                            float ref = logf(inputA + 100);
                            if (fabs(output - ref) / ref > 0.01)
                            {
                                //printf("ref %f output %f\n", ref, output);
                                res = QP_TEST_RESULT_FAIL;
                            }
                            break;
                        }
                        case TT_TANH:
                        {
                            float ref = tanhf(inputA * 0.1f);
                            if (output != ref && fabs(output - ref) / ref > 0.01)
                            {
                                //printf("ref %f output %f\n", ref, output);
                                res = QP_TEST_RESULT_FAIL;
                            }
                            break;
                        }
                        case TT_ATAN:
                        {
                            float ref = atanf(inputA);
                            if (output != ref && fabs(output - ref) / ref > 0.01)
                            {
                                //printf("ref %f output %f\n", ref, output);
                                res = QP_TEST_RESULT_FAIL;
                            }
                            break;
                        }
                        case TT_MIN:
                        {
                            float ref = std::min(std::min(inputA, inputB), 5.0f);
                            if (output != ref && fabs(output - ref) / ref > 0.01)
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        }
                        case TT_MAX:
                        {
                            float ref = std::max(std::max(inputA, inputB), 0.0f);
                            if (output != ref && fabs(output - ref) / ref > 0.01)
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        }
                        case TT_CLAMP:
                        {
                            float ref = std::min(std::max(inputA, inputB), 5.0f);
                            if (output != ref && fabs(output - ref) / ref > 0.01)
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        }
                        case TT_STEP:
                        {
                            float ref = inputA < 0.0f ? 0.0f : 1.0f;
                            if (output != ref)
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        }
                        case TT_FMA:
                        {
                            float ref = inputA * inputB + 0.5f;
                            if (output != ref)
                            {
                                //printf("ref %f output %f\n", ref, output);
                                res = QP_TEST_RESULT_FAIL;
                            }
                            break;
                        }
                        case TT_REDUCESUM:
                        {
                            switch (m_data.resultAddr)
                            {
                            case RESULT_ADDR_UNIFORM:
                            {
                                output    = getDataFloat(ptrs[3], dataTypes[3], 1 * inputVectorPaddedElements + j);
                                float ref = 0;
                                for (uint32_t k = 0; k < numInvocations; ++k)
                                {
                                    ref += getDataFloat(ptrs[0], dataTypes[0], k * inputVectorPaddedElements + j);
                                }

                                if (output != ref)
                                {
                                    //printf("i %d j %d ref %f output %f\n", i, j, ref, output);
                                    res = QP_TEST_RESULT_FAIL;
                                }
                                // The i loop is unnecessary, skip the remaining iterations
                                i = numInvocations - 1;
                            }
                            break;
                            case RESULT_ADDR_UNIQUE:
                            {
                                float ref = getDataFloat(ptrs[0], dataTypes[0], i * inputVectorPaddedElements + j);
                                if (output != ref)
                                {
                                    //printf("i %d j %d ref %f output %f\n", i, j, ref, output);
                                    res = QP_TEST_RESULT_FAIL;
                                }
                            }
                            break;
                            case RESULT_ADDR_CLUSTERED:
                            {
                                output = getDataFloat(ptrs[3], dataTypes[3], (i / 5) * inputVectorPaddedElements + j);
                                float ref = 0;
                                for (uint32_t k = (i / 5) * 5; k < (i / 5 + 1) * 5; ++k)
                                {
                                    if (k < numInvocations)
                                    {
                                        ref += getDataFloat(ptrs[0], dataTypes[0], k * inputVectorPaddedElements + j);
                                    }
                                }
                                if (output != ref)
                                {
                                    //printf("i %d j %d ref %f output %f\n", i, j, ref, output);
                                    res = QP_TEST_RESULT_FAIL;
                                }
                            }
                            break;
                            }
                            break;
                        }

                        default:
                            break;
                        }
                    }
                }
            }
            else
            {
                uint32_t numInvocations = totalInvocations;
                for (uint32_t inv = 0; inv < numInvocations; ++inv)
                {
                    // First try with quantization. If that fails, then for FP8 try again
                    // without quantization (really, with quantization to FP16).
                    for (int32_t doQuantize = 1; doQuantize >= 0; --doQuantize)
                    {
                        uint32_t inputAIndex = inv * inputVectorPaddedElements;
                        uint32_t outputIndex = inv * outputVectorPaddedElements;
                        uint32_t matrixIndex = m_data.nonuniformOffset ? (inv / nonuniformMatrixGroupSize) : 0;
                        uint32_t biasIndex   = m_data.nonuniformOffset ? (inv / nonuniformBiasGroupSize) : 0;

                        uint32_t matrixOffset  = matrixIndex * totalLayerSize + layerOffsetsRaw[0];
                        uint32_t matrixOffset2 = matrixIndex * totalLayerSize + layerOffsetsRaw[1];
                        uint32_t matrixOffset3 = matrixIndex * totalLayerSize + layerOffsetsRaw[2];
                        uint32_t biasOffset    = biasIndex * biasStride;

                        vector<float> tempK(K);
                        vector<float> tempN(N);
                        for (uint32_t k = 0; k < K; ++k)
                        {
                            tempK[k] = getDataFloat(ptrs[0], dataTypes[0], inputAIndex + k);
                        }
                        auto const matmul = [&](VkCooperativeVectorMatrixLayoutNV matrixLayout, uint32_t inDim,
                                                std::vector<float> const &inArray, uint32_t outDim,
                                                std::vector<float> &outArray, uint32_t mOffset, uint32_t layer,
                                                bool transpose = false)
                        {
                            bool columnMajor =
                                (matrixLayout == VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_COLUMN_MAJOR_NV) ^ transpose;
                            for (uint32_t o = 0; o < outDim; ++o)
                            {
                                float ref = 0;
                                for (uint32_t in = 0; in < inDim; ++in)
                                {
                                    float inputA = inArray[in];
                                    uint32_t offset =
                                        columnMajor ? (in * matrixStride[layer]) : (o * matrixStride[layer]);
                                    uint32_t index = columnMajor ? o : in;
                                    float inputB =
                                        getDataFloatOffsetIndex(ptrs[1], dataTypes[1], offset + mOffset, index);
                                    if (m_data.testType == TT_MATRIXMUL_TRAININGBIAS)
                                    {
                                        inputB += 1.0f;
                                        // quantize to the matrix type
                                        uint32_t temp;
                                        setDataFloat(&temp, dataTypes[1], 0, inputB);
                                        inputB = getDataFloat(&temp, dataTypes[1], 0);
                                    }
                                    ref += inputA * inputB;
                                }
                                outArray[o] = ref;
                            }
                        };
                        auto const &addActivation = [ptrs, dataTypes](Activation act, uint32_t inDim,
                                                                      std::vector<float> &inArray,
                                                                      uint32_t globalInvocationIndex, uint32_t idx)
                        {
                            DE_UNREF(idx);
                            switch (act)
                            {
                            default:
                                DE_ASSERT(0);
                            case ACT_NONE:
                                break;
                            case ACT_MUL:
                                for (uint32_t i = 0; i < inDim; ++i)
                                {
                                    inArray[i] *= 0.5f;
                                }
                                break;
                            case ACT_MAX:
                                for (uint32_t i = 0; i < inDim; ++i)
                                {
                                    inArray[i] = std::max(inArray[i], 0.0f);
                                }
                                break;
                            case ACT_NONUNIF:
                                for (uint32_t i = 0; i < inDim; ++i)
                                {
                                    inArray[i] *= float((globalInvocationIndex % 3) / 2.0);
                                }
                                break;
                            case ACT_DIVERGE:
                                for (uint32_t i = 0; i < inDim; ++i)
                                {
                                    if ((globalInvocationIndex & 1) != 0)
                                    {
                                        inArray[i] *= 0.5f;
                                    }
                                }
                                break;
                            case ACT_SIGMOID:
                                for (uint32_t i = 0; i < inDim; ++i)
                                {
                                    inArray[i] = 1.f / (1.f + expf(-inArray[i]));
                                }
                                break;
                            case ACT_LEAKYRELUSTEP:
                            case ACT_LEAKYRELUMAX:
                                for (uint32_t i = 0; i < inDim; ++i)
                                {
                                    inArray[i] = (inArray[i] < 0.0f) ? 0.5f * inArray[i] : inArray[i];
                                }
                                break;
                            case ACT_HARDGELU:
                                for (uint32_t i = 0; i < inDim; ++i)
                                {
                                    inArray[i] = inArray[i] / 2.0f + 0.75f;
                                    inArray[i] = std::min(inArray[i], 128.0f) *
                                                 std::min(1.0f, std::max(0.0f, inArray[i] / 3.0f + 0.75f));
                                }
                                break;
                            case ACT_LOAD:
                            case ACT_LOAD_SHARED:
                                for (uint32_t i = 0; i < inDim; ++i)
                                {
                                    float inputC = getDataFloatOffsetIndex(ptrs[2], dataTypes[2],
                                                                           16 * (globalInvocationIndex & 1), i);
                                    inArray[i] += inputC;
                                }
                                break;
                            case ACT_LOAD_READONLY:
                                for (uint32_t i = 0; i < inDim; ++i)
                                {
                                    float inputA =
                                        getDataFloatOffsetIndex(ptrs[0], dataTypes[0], 0, globalInvocationIndex);
                                    inArray[i] += inputA;
                                }
                                break;
                            }
                        };
                        auto const addBias =
                            [ptrs, dataTypes](uint32_t outDim, std::vector<float> &outArray, uint32_t biasOffset2)
                        {
                            for (uint32_t o = 0; o < outDim; ++o)
                            {
                                float inputC = getDataFloatOffsetIndex(ptrs[2], dataTypes[2], biasOffset2, o);
                                outArray[o] += inputC;
                            }
                        };

                        auto const quantize = [&](uint32_t dim, std::vector<float> &arr)
                        {
                            VkComponentTypeKHR inputInterpretation = m_data.inputInterpretation;
                            if (!doQuantize)
                            {
                                inputInterpretation = VK_COMPONENT_TYPE_FLOAT16_NV;
                            }
                            for (uint32_t o = 0; o < dim; ++o)
                            {
                                float before = arr[o];
                                uint32_t temp;
                                setDataFloat(&temp, inputInterpretation, 0, before);
                                float after = getDataFloat(&temp, inputInterpretation, 0);
                                arr[o]      = after;
                            }
                        };

                        switch (m_data.testType)
                        {
                        case TT_MATRIXMAD:
                        case TT_MATRIXMADTRANSPOSE:
                            quantize(K, tempK);
                            matmul(m_data.matrixLayout[0], K, tempK, N, tempN, matrixOffset, 0,
                                   m_data.testType == TT_MATRIXMADTRANSPOSE);
                            addBias(N, tempN, biasOffset);
                            addActivation(m_data.act0, N, tempN, inv, 0);
                            break;
                        case TT_MATRIXMUL:
                        case TT_MATRIXMUL_TRAININGBIAS:
                            quantize(K, tempK);
                            matmul(m_data.matrixLayout[0], K, tempK, N, tempN, matrixOffset, 0);
                            addActivation(m_data.act0, N, tempN, inv, 0);
                            break;
                        case TT_MATRIXMUL3:
                            quantize(K, tempK);
                            matmul(m_data.matrixLayout[0], K, tempK, N, tempN, matrixOffset, 0);
                            addActivation(m_data.act0, N, tempN, inv, 0);
                            quantize(N, tempN);
                            matmul(m_data.matrixLayout[1], N, tempN, K, tempK, matrixOffset2, 1);
                            addActivation(m_data.act1, K, tempK, inv, 1);
                            quantize(K, tempK);
                            matmul(m_data.matrixLayout[2], K, tempK, N, tempN, matrixOffset3, 2);
                            addActivation(m_data.act2, N, tempN, inv, 2);
                            break;
                        case TT_MATRIXMUL2ADD:
                        case TT_MATRIXMUL2ADDMUL2:
                        {
                            // vecB = vecA with components swapped pairwise
                            // temp0 = mat0 * vecA; // NxK * Kx1
                            // temp1 = mat0 * vecB; // NxK * Kx1
                            // temp2 = temp0 + temp1
                            // temp2 = activation(temp2)
                            // if (m_data.testType == TT_MATRIXMUL2ADDMUL2) {
                            //   temp3 = mat1 * temp2; // KxN * Nx1
                            //   temp3 = activation(temp3)
                            //   vecO  = mat2 * temp3; // NxK * Kx1
                            //   vecO  = activation(vecO)
                            // } else {
                            //   vecO = temp2
                            // }
                            vector<float> vecA = tempK;
                            vector<float> vecB = tempK;
                            for (uint32_t k = 0; k < K; ++k)
                            {
                                uint32_t idx = k ^ 1;
                                if (idx >= K)
                                    idx = k;
                                vecB[k] = vecA[idx];
                            }
                            vector<float> temp0(N), temp1(N), temp2(N);
                            vector<float> temp3(K);
                            quantize(K, vecA);
                            matmul(m_data.matrixLayout[0], K, vecA, N, temp0, matrixOffset, 0);
                            quantize(K, vecB);
                            matmul(m_data.matrixLayout[0], K, vecB, N, temp1, matrixOffset, 0);
                            for (size_t n = 0; n < temp0.size(); n++)
                            {
                                temp2[n] = temp0[n] + temp1[n];
                            }
                            addActivation(m_data.act0, N, temp2, inv, 0);
                            if (m_data.testType == TT_MATRIXMUL2ADDMUL2)
                            {
                                quantize(N, temp2);
                                matmul(m_data.matrixLayout[1], N, temp2, K, temp3, matrixOffset2, 1);
                                addActivation(m_data.act1, K, temp3, inv, 1);
                                quantize(K, temp3);
                                matmul(m_data.matrixLayout[2], K, temp3, N, tempN, matrixOffset3, 2);
                                addActivation(m_data.act2, N, tempN, inv, 2);
                            }
                            else
                            {
                                tempN = temp2;
                            }
                            break;
                        }
                        default:
                            break;
                        }

                        qpTestResult tempRes = QP_TEST_RESULT_PASS;
                        for (uint32_t n = 0; n < N; ++n)
                        {
                            float ref    = tempN[n];
                            float output = getDataFloat(ptrs[3], dataTypes[3], outputIndex + n);
                            //printf("i %d n %d ref %f output %f\n", inv, n, ref, output);
                            if (output != ref)
                            {
                                if (m_data.act0 == ACT_SIGMOID)
                                {
                                    if (fabs(output - ref) > 0.01f)
                                    {
                                        //printf("ref %f output %f\n", ref, output);
                                        tempRes = QP_TEST_RESULT_FAIL;
                                    }
                                }
                                else if (m_data.testType == TT_MATRIXMUL3 || m_data.testType == TT_MATRIXMUL2ADDMUL2 ||
                                         m_data.testType == TT_MATRIXMUL2ADD || m_data.act0 == ACT_HARDGELU ||
                                         m_data.testType == TT_MATRIXMUL_TRAININGBIAS || K > 64)
                                {
                                    // Three matrix multiplies can lead to loss of precision for fp16.
                                    // Fail if the relative error is > X%.
                                    float denom         = (fabs(ref) < 0.5f) ? 5.0f : fabs(ref);
                                    float err           = fabs(output - ref) / denom;
                                    float relativeLimit = N * K > 200 ? 0.06f : 0.01f;

                                    if (err > relativeLimit)
                                    {
                                        if ((m_data.act0 == ACT_LEAKYRELUSTEP || m_data.act0 == ACT_LEAKYRELUMAX ||
                                             m_data.testType == TT_MATRIXMUL2ADDMUL2 || m_data.act0 == ACT_HARDGELU) &&
                                            fabs(output - ref) < 0.1f)
                                        {
                                            //printf("warn: ref %f output %f\n", ref, output);
                                        }
                                        else
                                        {
                                            //printf("i %d n %d ref %f output %f\n", inv, n, ref, output);
                                            tempRes = QP_TEST_RESULT_FAIL;
                                        }
                                    }
                                    else
                                    {
                                        //printf("warn: ref %f output %f\n", ref, output);
                                    }
                                }
                                else
                                {
                                    //printf("i %d n %d ref %f output %f\n", inv, n, ref, output);
                                    tempRes = QP_TEST_RESULT_FAIL;
                                }
                            }
                        }
                        if (tempRes == QP_TEST_RESULT_PASS)
                        {
                            break;
                        }
                        // If FP8 fails on the first try, with quantization, then try again without.
                        // But if it's not FP8, then call it a failure and don't retry.
                        if (doQuantize == 0 || (m_data.inputInterpretation != VK_COMPONENT_TYPE_FLOAT_E4M3_NV &&
                                                m_data.inputInterpretation != VK_COMPONENT_TYPE_FLOAT_E5M2_NV))
                        {
                            res = QP_TEST_RESULT_FAIL;
                            break;
                        }
                    }
                }
            }
        }
        else
        {
            if (!isMatrixMul(m_data.testType))
            {
                uint32_t numInvocations = totalInvocations;
                for (uint32_t i = 0; i < numInvocations; ++i)
                {
                    for (uint32_t j = 0; j < N; ++j)
                    {
                        int64_t inputA, inputB;
                        if (isFloatType(dataTypes[0]))
                        {
                            inputA = (int64_t)getDataFloat(ptrs[0], dataTypes[0], i * inputVectorPaddedElements + j);
                            inputB = (int64_t)getDataFloat(ptrs[1], dataTypes[1], i * inputVectorPaddedElements + j);
                        }
                        else
                        {
                            inputA = getDataInt(ptrs[0], dataTypes[0], i * inputVectorPaddedElements + j);
                            inputB = getDataInt(ptrs[1], dataTypes[1], i * inputVectorPaddedElements + j);
                        }
                        int64_t output = getDataInt(ptrs[3], dataTypes[3], i * outputVectorPaddedElements + j);
                        switch (m_data.testType)
                        {
                        case TT_LENGTH:
                            if (output != K)
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        case TT_CONSTANT:
                            if (output != 1)
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        case TT_CONVERT:
                            if (!isSIntType(dataTypes[3]))
                            {
                                if (inputA < 0)
                                    inputA = 0;
                            }
                            if (output != truncInt(inputA, dataTypes[3]))
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        case TT_COMPOSITE:
                        case TT_COMPOSITE_RVALUE:
                        case TT_COMPOSITE_ARRAY:
                        case TT_ADD:
                            if (output != truncInt(inputA + inputB, dataTypes[3]))
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        case TT_VECTOR_EXTRACT:
                            if (output != truncInt(inputA + inputB + 1, dataTypes[3]))
                                res = QP_TEST_RESULT_FAIL;
                            break;
                            break;
                        case TT_SUB:
                            if (output != truncInt(inputA - inputB, dataTypes[3]))
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        case TT_MUL:
                            if (output != truncInt(inputA * inputB, dataTypes[3]))
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        case TT_DIV:
                        {
                            if (inputB != 0 && output != truncInt(inputA / inputB, dataTypes[3]))
                                res = QP_TEST_RESULT_FAIL;
                        }
                        break;
                        case TT_NEGATE:
                        case TT_FUNC:
                            if (output != truncInt(-inputA, dataTypes[3]))
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        case TT_VECTORTIMESSCALAR:
                            if (output != truncInt(6 * inputA, dataTypes[3]))
                                res = QP_TEST_RESULT_FAIL;
                            break;

                        case TT_MIN:
                        {
                            int64_t ref = truncInt(std::min(std::min(inputA, inputB), int64_t{5}), dataTypes[3]);
                            if (output != ref)
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        }
                        case TT_MAX:
                        {
                            int64_t ref = truncInt(std::max(std::max(inputA, inputB), int64_t{0}), dataTypes[3]);
                            if (output != ref)
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        }
                        case TT_CLAMP:
                        {
                            int64_t ref = truncInt(std::min(std::max(inputA, inputB), int64_t{5}), dataTypes[3]);
                            if (output != ref)
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        }
                        case TT_AND:
                            if (output != truncInt(inputA & inputB, dataTypes[3]))
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        case TT_OR:
                            if (output != truncInt(inputA | inputB, dataTypes[3]))
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        case TT_XOR:
                            if (output != truncInt(inputA ^ inputB, dataTypes[3]))
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        case TT_NOT:
                            if (output != truncInt(~inputA, dataTypes[3]))
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        case TT_SHL:
                            if (output != truncInt(inputA << (inputB & 7), dataTypes[3]))
                                res = QP_TEST_RESULT_FAIL;
                            break;
                        case TT_SHR:
                            if (output != truncInt(inputA >> (inputB & 7), dataTypes[3]))
                                res = QP_TEST_RESULT_FAIL;
                            break;

                        default:
                            break;
                        }
                    }
                }
            }
            else
            {
                uint32_t numInvocations = totalInvocations;
                for (uint32_t inv = 0; inv < numInvocations; ++inv)
                {
                    uint32_t inputAIndex = inv * inputVectorPaddedElements;
                    uint32_t outputIndex = inv * outputVectorPaddedElements;
                    uint32_t matrixIndex = m_data.nonuniformOffset ? (inv / nonuniformMatrixGroupSize) : 0;
                    uint32_t biasIndex   = m_data.nonuniformOffset ? (inv / nonuniformBiasGroupSize) : 0;

                    uint32_t matrixOffset  = matrixIndex * totalLayerSize + layerOffsetsRaw[0];
                    uint32_t matrixOffset2 = matrixIndex * totalLayerSize + layerOffsetsRaw[1];
                    uint32_t matrixOffset3 = matrixIndex * totalLayerSize + layerOffsetsRaw[2];
                    uint32_t biasOffset    = biasIndex * biasStride;

                    vector<int64_t> tempK(K);
                    vector<int64_t> tempN(N);
                    for (uint32_t k = 0; k < K; ++k)
                    {
                        tempK[k] = getDataInt(ptrs[0], dataTypes[0], inputAIndex + k);
                    }
                    auto const matmul = [&](VkCooperativeVectorMatrixLayoutNV matrixLayout, uint32_t inDim,
                                            std::vector<int64_t> const &inArray, uint32_t outDim,
                                            std::vector<int64_t> &outArray, uint32_t mOffset, uint32_t layer,
                                            bool transpose = false)
                    {
                        bool columnMajor =
                            (matrixLayout == VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_COLUMN_MAJOR_NV) ^ transpose;
                        for (uint32_t o = 0; o < outDim; ++o)
                        {
                            int64_t ref = 0;
                            for (uint32_t in = 0; in < inDim; ++in)
                            {
                                int64_t inputA  = inArray[in];
                                uint32_t offset = columnMajor ? (in * matrixStride[layer]) : (o * matrixStride[layer]);
                                uint32_t index  = columnMajor ? o : in;
                                int64_t inputB  = getDataIntOffsetIndex(ptrs[1], dataTypes[1], offset + mOffset, index);
                                if (m_data.testType == TT_MATRIXMUL_TRAININGBIAS)
                                {
                                    if (inputB < 0x7F)
                                    {
                                        inputB += 1;
                                    }
                                }
                                ref += inputA * inputB;
                            }
                            outArray[o] = ref;
                        }
                    };
                    auto const &addActivation = [ptrs, dataTypes](Activation act, uint32_t inDim,
                                                                  std::vector<int64_t> &inArray,
                                                                  uint32_t globalInvocationIndex, uint32_t idx)
                    {
                        DE_UNREF(idx);
                        switch (act)
                        {
                        default:
                            DE_ASSERT(0);
                        case ACT_NONE:
                            break;
                        case ACT_MUL:
                            for (uint32_t i = 0; i < inDim; ++i)
                            {
                                inArray[i] *= 2;
                            }
                            break;
                        case ACT_MAX:
                            for (uint32_t i = 0; i < inDim; ++i)
                            {
                                inArray[i] = std::max(inArray[i], int64_t{0});
                            }
                            break;
                        case ACT_NONUNIF:
                            for (uint32_t i = 0; i < inDim; ++i)
                            {
                                inArray[i] *= (globalInvocationIndex % 3);
                            }
                            break;
                        case ACT_DIVERGE:
                            for (uint32_t i = 0; i < inDim; ++i)
                            {
                                if ((globalInvocationIndex & 1) != 0)
                                {
                                    inArray[i] *= 2;
                                }
                            }
                            break;
                        case ACT_HARDGELU:
                            for (uint32_t i = 0; i < inDim; ++i)
                            {
                                float temp = (float)inArray[i];
                                temp       = temp / 2.0f + 0.75f;
                                temp = std::min(temp, 65536.0f) * std::min(4.0f, std::max(-4.0f, temp / 3.0f + 0.75f));
                                inArray[i] = (int64_t)temp;
                            }
                            break;
                        case ACT_SIGMOID:
                            DE_ASSERT(0);
                            break;
                        case ACT_LEAKYRELUSTEP:
                        case ACT_LEAKYRELUMAX:
                            DE_ASSERT(0);
                            break;
                        case ACT_LOAD:
                        case ACT_LOAD_SHARED:
                            for (uint32_t i = 0; i < inDim; ++i)
                            {
                                int64_t inputC =
                                    getDataIntOffsetIndex(ptrs[2], dataTypes[2], 16 * (globalInvocationIndex & 1), i);
                                inArray[i] += 16 * inputC;
                            }
                            break;
                        case ACT_LOAD_READONLY:
                            for (uint32_t i = 0; i < inDim; ++i)
                            {
                                int64_t inputA = getDataIntOffsetIndex(ptrs[2], dataTypes[2], 0, globalInvocationIndex);
                                inArray[i] += inputA;
                            }
                            break;
                        }
                    };
                    auto const addBias =
                        [ptrs, dataTypes](uint32_t outDim, std::vector<int64_t> &outArray, uint32_t biasOffset2)
                    {
                        for (uint32_t o = 0; o < outDim; ++o)
                        {
                            int64_t inputC = getDataIntOffsetIndex(ptrs[2], dataTypes[2], biasOffset2, o);
                            outArray[o] += inputC;
                        }
                    };

                    int64_t clampMin = INT64_MIN, clampMax = INT64_MAX;
                    // Implicit conversions via inputInterpretation are clamped
                    if (m_data.inputType != m_data.inputInterpretation)
                    {
                        if (isSIntType(m_data.inputInterpretation))
                        {
                            clampMax = (1LL << (getComponentTypeInfo(m_data.inputInterpretation).bits - 1)) - 1;
                            clampMin = -clampMax - 1;
                        }
                        else
                        {
                            clampMax = (1LL << getComponentTypeInfo(m_data.inputInterpretation).bits) - 1;
                            clampMin = 0;
                        }
                    }
                    // Explicit conversions in the shader source truncate the high bits
                    int64_t clampMask = ~0LL;
                    if (m_data.inputType != m_data.outputType && !isFloatType(m_data.inputType))
                    {
                        clampMask = (1LL << getComponentTypeInfo(m_data.inputType).bits) - 1;
                    }
                    auto trunc = [&](int64_t v) -> int64_t
                    {
                        v &= clampMask;
                        if (isSIntType(m_data.inputType))
                        {
                            // sign extend
                            v <<= 64 - getComponentTypeInfo(m_data.inputType).bits;
                            v >>= 64 - getComponentTypeInfo(m_data.inputType).bits;
                        }
                        v = std::min(v, clampMax);
                        v = std::max(v, clampMin);
                        return v;
                    };

                    switch (m_data.testType)
                    {
                    case TT_MATRIXMAD:
                    case TT_MATRIXMADTRANSPOSE:
                        matmul(m_data.matrixLayout[0], K, tempK, N, tempN, matrixOffset, 0,
                               m_data.testType == TT_MATRIXMADTRANSPOSE);
                        addBias(N, tempN, biasOffset);
                        addActivation(m_data.act0, N, tempN, inv, 0);
                        break;
                    case TT_MATRIXMUL:
                    case TT_MATRIXMUL_TRAININGBIAS:
                        matmul(m_data.matrixLayout[0], K, tempK, N, tempN, matrixOffset, 0);
                        addActivation(m_data.act0, N, tempN, inv, 0);
                        break;
                    case TT_MATRIXMUL3:
                    {
                        matmul(m_data.matrixLayout[0], K, tempK, N, tempN, matrixOffset, 0);
                        addActivation(m_data.act0, N, tempN, inv, 0);
                        uint32_t scale = getIntScaleShift(m_data.inputVectorSize);

                        for (size_t n = 0; n < tempN.size(); n++)
                        {
                            if (doFloatScale(m_data))
                            {
                                tempN[n] = RTNE((float)tempN[n] * getFloatScaleFactor(m_data.inputVectorSize));
                            }
                            else if (doIntShift(m_data))
                            {
                                tempN[n] >>= scale;
                            }
                            tempN[n] = trunc(tempN[n]);
                        }

                        matmul(m_data.matrixLayout[1], N, tempN, K, tempK, matrixOffset2, 1);
                        addActivation(m_data.act1, K, tempK, inv, 1);

                        for (size_t n = 0; n < tempK.size(); n++)
                        {
                            if (doFloatScale(m_data))
                            {
                                tempK[n] = RTNE((float)tempK[n] * getFloatScaleFactor(m_data.inputVectorSize));
                            }
                            else if (doIntShift(m_data))
                            {
                                tempK[n] >>= scale;
                            }
                            tempK[n] = trunc(tempK[n]);
                        }

                        matmul(m_data.matrixLayout[2], K, tempK, N, tempN, matrixOffset3, 2);
                        addActivation(m_data.act2, N, tempN, inv, 2);
                        break;
                    }
                    case TT_MATRIXMUL2ADD:
                    case TT_MATRIXMUL2ADDMUL2:
                    {
                        // vecB = vecA with components swapped pairwise
                        // temp0 = mat0 * vecA; // NxK * Kx1
                        // temp1 = mat0 * vecB; // NxK * Kx1
                        // temp2 = temp0 + temp1
                        // temp2 = activation(temp2)
                        // if (m_data.testType == TT_MATRIXMUL2ADDMUL2) {
                        //   temp3 = mat1 * temp2; // KxN * Nx1
                        //   temp3 = activation(temp3)
                        //   vecO  = mat2 * temp3; // NxK * Kx1
                        //   vecO  = activation(vecO)
                        // } else {
                        //   vecO = temp2
                        // }
                        vector<int64_t> vecA = tempK;
                        vector<int64_t> vecB = tempK;
                        for (uint32_t k = 0; k < K; ++k)
                        {
                            uint32_t idx = k ^ 1;
                            if (idx >= K)
                                idx = k;
                            vecB[k] = vecA[idx];
                        }
                        vector<int64_t> temp0(N), temp1(N), temp2(N);
                        vector<int64_t> temp3(K);
                        matmul(m_data.matrixLayout[0], K, vecA, N, temp0, matrixOffset, 0);
                        matmul(m_data.matrixLayout[0], K, vecB, N, temp1, matrixOffset, 0);
                        for (size_t n = 0; n < temp0.size(); n++)
                        {
                            temp2[n] = temp0[n] + temp1[n];
                        }
                        addActivation(m_data.act0, N, temp2, inv, 0);

                        if (m_data.testType == TT_MATRIXMUL2ADDMUL2)
                        {
                            uint32_t scale = getIntScaleShift(K);
                            for (size_t n = 0; n < temp2.size(); n++)
                            {
                                if (doFloatScale(m_data))
                                {
                                    temp2[n] = RTNE((float)temp2[n] * getFloatScaleFactor(K));
                                }
                                else if (doIntShift(m_data))
                                {
                                    temp2[n] >>= scale;
                                }
                                temp2[n] = trunc(temp2[n]);
                            }
                            matmul(m_data.matrixLayout[1], N, temp2, K, temp3, matrixOffset2, 1);
                            addActivation(m_data.act1, K, temp3, inv, 1);

                            scale = getIntScaleShift(N);
                            for (size_t n = 0; n < temp3.size(); n++)
                            {
                                if (doFloatScale(m_data))
                                {
                                    temp3[n] = RTNE((float)temp3[n] * getFloatScaleFactor(N));
                                }
                                else if (doIntShift(m_data))
                                {
                                    temp3[n] >>= scale;
                                }
                                temp3[n] = trunc(temp3[n]);
                            }
                            matmul(m_data.matrixLayout[2], K, temp3, N, tempN, matrixOffset3, 2);
                            addActivation(m_data.act2, N, tempN, inv, 2);
                        }
                        else
                        {
                            tempN = temp2;
                        }
                        break;
                    }
                    default:
                        break;
                    }

                    for (uint32_t n = 0; n < N; ++n)
                    {
                        int64_t ref    = tempN[n];
                        int64_t output = getDataInt(ptrs[3], dataTypes[3], outputIndex + n);
                        //printf("i %d n %d ref %d output %d\n", inv, n, (int32_t)ref, (int32_t)output);
                        if ((int32_t)output != (int32_t)ref)
                        {
                            //printf("i %d n %d ref %d output %d\n", inv, n, (int32_t)ref, (int32_t)output);
                            res = QP_TEST_RESULT_FAIL;
                        }
                    }
                }
            }
        }
        if (res != QP_TEST_RESULT_PASS)
        {
            log << tcu::TestLog::Message << "failed with N = " << N << ", K = " << K << tcu::TestLog::EndMessage;
            finalres = res;

#ifdef COOPERATIVE_VECTOR_EXTENDED_DEBUG
            for (int i = 0; i < 4; i++)
            {
                const char *matrixNames[] = {"A", "B", "C", "D"};

                log << tcu::TestLog::Message << "Matrix " << matrixNames[i] << "[count=" << totalElements[i] << "]:\n"
                    << dumpWholeMatrix(ptrs[i], dataTypes[i], totalElements[i]) << tcu::TestLog::EndMessage;
            }
#endif
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

tcu::TestCaseGroup *createCooperativeVectorBasicTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "basic", "cooperative_vector tests"));

    typedef struct
    {
        uint32_t value;
        const char *name;
        const char *description;
    } TestGroupCase;

    TestGroupCase ttCases[] = {
        {TT_LENGTH, "length", ".length()"},
        {TT_CONSTANT, "constant", "OpConstantComposite"},
        {TT_CONVERT, "convert", "OpFConvert/OpSConvert/OpUConvert"},
        {TT_COMPOSITE, "composite", "OpCompositeConstruct"},
        {TT_COMPOSITE_RVALUE, "composite_rvalue", "OpCompositeExtract"},
        {TT_VECTOR_EXTRACT, "vector_extract", "OpVectorExtractDynamic"},
        {TT_ADD, "add", "OpFAdd/OpIAdd"},
        {TT_SUB, "sub", "OpFSub/OpISub"},
        {TT_MUL, "mul", "OpFMul/OpIMul"},
        {TT_DIV, "div", "OpFDiv/OpSDiv/OpUDiv"},
        {TT_NEGATE, "negate", "OpFNegate/OpSNegate"},
        {TT_VECTORTIMESSCALAR, "vectortimesscalar", "OpVectorTimesScalar"},
        {TT_EXP, "exp", "Exp"},
        {TT_LOG, "log", "Log"},
        {TT_TANH, "tanh", "Tanh"},
        {TT_ATAN, "atan", "ATan"},
        {TT_MIN, "min", "FMin"},
        {TT_MAX, "max", "FMax"},
        {TT_CLAMP, "clamp", "FClamp"},
        {TT_STEP, "step", "Step"},
        {TT_FMA, "fma", "Fma"},
        {TT_FUNC, "func", "OpFunctionParameter"},
        {TT_AND, "and", "OpBitwiseAnd"},
        {TT_OR, "or", "OpBitwiseOr"},
        {TT_XOR, "xor", "OpBitwiseXor"},
        {TT_NOT, "not", "OpNot"},
        {TT_SHL, "shl", "OpShiftLeftLogical"},
        {TT_SHR, "shr", "OpShiftRightLogical/Arithmetic"},
        {TT_COMPOSITE_ARRAY, "composite_array", "OpCompositeConstruct w/array"},
    };

    TestGroupCaseN<2> dtCases[] = {
        {{VK_COMPONENT_TYPE_FLOAT16_NV, VK_COMPONENT_TYPE_FLOAT16_NV}, "float16_float16", "float16_float16"},
        {{VK_COMPONENT_TYPE_UINT8_KHR, VK_COMPONENT_TYPE_UINT8_KHR}, "uint8_uint8", "uint8_uint8"},
        {{VK_COMPONENT_TYPE_UINT8_KHR, VK_COMPONENT_TYPE_UINT32_KHR}, "uint8_uint32", "uint8_uint32"},
        {{VK_COMPONENT_TYPE_UINT32_KHR, VK_COMPONENT_TYPE_UINT8_KHR}, "uint32_uint8", "uint32_uint8"},
        {{VK_COMPONENT_TYPE_SINT8_KHR, VK_COMPONENT_TYPE_SINT8_KHR}, "sint8_sint8", "sint8_sint8"},
        {{VK_COMPONENT_TYPE_SINT8_KHR, VK_COMPONENT_TYPE_SINT32_KHR}, "sint8_sint32", "sint8_sint32"},
        {{VK_COMPONENT_TYPE_SINT32_KHR, VK_COMPONENT_TYPE_SINT8_KHR}, "sint32_sint8", "sint32_sint8"},
        {{VK_COMPONENT_TYPE_FLOAT16_NV, VK_COMPONENT_TYPE_UINT8_KHR}, "float16_uint8", "float16_uint8"},
        {{VK_COMPONENT_TYPE_FLOAT16_NV, VK_COMPONENT_TYPE_SINT8_KHR}, "float16_sint8", "float16_sint8"},
        {{VK_COMPONENT_TYPE_FLOAT16_NV, VK_COMPONENT_TYPE_UINT32_KHR}, "float16_uint32", "float16_uint32"},
        {{VK_COMPONENT_TYPE_FLOAT16_NV, VK_COMPONENT_TYPE_SINT32_KHR}, "float16_sint32", "float16_sint32"},
        {{VK_COMPONENT_TYPE_UINT8_KHR, VK_COMPONENT_TYPE_FLOAT16_NV}, "uint8_float16", "uint8_float16"},
        {{VK_COMPONENT_TYPE_SINT8_KHR, VK_COMPONENT_TYPE_FLOAT16_NV}, "sint8_float16", "sint8_float16"},
        {{VK_COMPONENT_TYPE_UINT32_KHR, VK_COMPONENT_TYPE_FLOAT16_NV}, "uint32_float16", "uint8_float16"},
        {{VK_COMPONENT_TYPE_SINT32_KHR, VK_COMPONENT_TYPE_FLOAT16_NV}, "sint32_float16", "sint8_float16"},
        {{VK_COMPONENT_TYPE_FLOAT16_NV, VK_COMPONENT_TYPE_FLOAT32_NV}, "float16_float32", "float16_float32"},
        {{VK_COMPONENT_TYPE_FLOAT32_NV, VK_COMPONENT_TYPE_FLOAT16_NV}, "float32_float16", "float32_float16"},
        {{VK_COMPONENT_TYPE_FLOAT32_NV, VK_COMPONENT_TYPE_FLOAT32_NV}, "float32_float32", "float32_float32"},
    };

    TestGroupCaseN<2> sizeCases[] = {
        {{1, 1}, "components1", "1 components"},     {{2, 2}, "components2", "2 components"},
        {{3, 3}, "components3", "3 components"},     {{4, 4}, "components4", "4 components"},
        {{5, 5}, "components5", "5 components"},     {{6, 6}, "components6", "6 components"},
        {{7, 7}, "components7", "7 components"},     {{8, 8}, "components8", "8 components"},
        {{9, 9}, "components9", "9 components"},     {{31, 31}, "components31", "31 components"},
        {{65, 65}, "components65", "65 components"},
    };

    TestGroupCase scCases[] = {
        {SC_BUFFER, "buffer", "SSBO"},
        {SC_WORKGROUP, "workgroup", "shared memory"},
        {SC_BUFFER_VARIABLE_POINTERS, "buffer_varptr", "SSBO w/variable pointers"},
        {SC_WORKGROUP_VARIABLE_POINTERS, "workgroup_varptr", "shared memory w/variable pointers"},
        {SC_PHYSICAL_STORAGE_BUFFER, "physical_buffer", "physical_storage_buffer"},
    };

    TestGroupCase stageCases[] = {
        {STAGE_COMPUTE, "compute", "compute"},
        {STAGE_RAYGEN, "raygen", "raygen"},
        {STAGE_INTERSECT, "isect", "intersect"},
        {STAGE_ANY_HIT, "ahit", "any_hit"},
        {STAGE_CLOSEST_HIT, "chit", "closest_hit"},
        {STAGE_MISS, "miss", "miss"},
        {STAGE_CALLABLE, "callable", "callable"},
        {STAGE_VERTEX, "vertex", "vertex"},
        {STAGE_FRAGMENT, "fragment", "fragment"},
        {STAGE_GEOMETRY, "geometry", "geometry"},
        {STAGE_TESS_CTRL, "tessctrl", "tessctrl"},
        {STAGE_TESS_EVAL, "tesseval", "tesseval"},
        {STAGE_TASK, "task", "task"},
        {STAGE_MESH, "mesh", "mesh"},
    };

    for (int ttNdx = 0; ttNdx < DE_LENGTH_OF_ARRAY(ttCases); ttNdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> ttGroup(
            new tcu::TestCaseGroup(testCtx, ttCases[ttNdx].name, ttCases[ttNdx].description));
        for (int dtNdx = 0; dtNdx < DE_LENGTH_OF_ARRAY(dtCases); dtNdx++)
        {
            de::MovePtr<tcu::TestCaseGroup> dtGroup(
                new tcu::TestCaseGroup(testCtx, dtCases[dtNdx].name, dtCases[dtNdx].description));
            for (int scNdx = 0; scNdx < DE_LENGTH_OF_ARRAY(scCases); scNdx++)
            {
                de::MovePtr<tcu::TestCaseGroup> scGroup(
                    new tcu::TestCaseGroup(testCtx, scCases[scNdx].name, scCases[scNdx].description));
                for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizeCases); sizeNdx++)
                {
                    de::MovePtr<tcu::TestCaseGroup> sizeGroup(
                        new tcu::TestCaseGroup(testCtx, sizeCases[sizeNdx].name, sizeCases[sizeNdx].description));
                    for (int stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stageCases); stageNdx++)
                    {
                        TestType testType             = (TestType)ttCases[ttNdx].value;
                        VkComponentTypeKHR inputType  = (VkComponentTypeKHR)dtCases[dtNdx].value[0];
                        VkComponentTypeKHR outputType = (VkComponentTypeKHR)dtCases[dtNdx].value[1];

                        if ((scCases[scNdx].value == SC_WORKGROUP ||
                             scCases[scNdx].value == SC_WORKGROUP_VARIABLE_POINTERS) &&
                            stageCases[stageNdx].value != STAGE_COMPUTE)
                            continue;

                        // reduce test count
                        if (stageCases[stageNdx].value != STAGE_COMPUTE &&
                            (isSIntType(inputType) || isSIntType(outputType)) && sizeCases[sizeNdx].value[0] != 65)
                            continue;

                        // reduce test count
                        if (sizeCases[sizeNdx].value[0] != 31 && stageCases[stageNdx].value != STAGE_COMPUTE)
                        {
                            continue;
                        }

                        if (!isMatrixMul(testType) && testType != TT_CONVERT && inputType != outputType)
                            continue;

                        if (testType == TT_CONVERT && inputType == outputType)
                            continue;

                        if (isMatrixMul(testType) &&
                            getComponentTypeInfo(inputType).bits > getComponentTypeInfo(outputType).bits)
                            continue;

                        // Only run physical storage buffer and variable pointer tests for 31x31, to reduce test count
                        if ((scCases[scNdx].value == SC_PHYSICAL_STORAGE_BUFFER ||
                             scCases[scNdx].value == SC_BUFFER_VARIABLE_POINTERS) &&
                            !(sizeCases[sizeNdx].value[0] == 31 && sizeCases[sizeNdx].value[1] == 31))
                        {
                            continue;
                        }

                        if (!isFloatType(inputType) || !isFloatType(outputType))
                        {
                            switch (testType)
                            {
                            case TT_LENGTH:
                            case TT_CONSTANT:
                            case TT_CONVERT:
                            case TT_COMPOSITE:
                            case TT_COMPOSITE_RVALUE:
                            case TT_VECTOR_EXTRACT:
                            case TT_ADD:
                            case TT_SUB:
                            case TT_MUL:
                            case TT_DIV:
                            case TT_NEGATE:
                            case TT_VECTORTIMESSCALAR:
                            case TT_MIN:
                            case TT_MAX:
                            case TT_FUNC:
                            case TT_COMPOSITE_ARRAY:
                            case TT_CLAMP:
                            case TT_AND:
                            case TT_OR:
                            case TT_XOR:
                            case TT_NOT:
                            case TT_SHL:
                            case TT_SHR:
                                // supported for integer types
                                break;
                            case TT_EXP:
                            case TT_LOG:
                            case TT_TANH:
                            case TT_ATAN:
                            case TT_STEP:
                            case TT_FMA:
                                // unsupported for integer types
                                continue;
                            default:
                                DE_ASSERT(0);
                                break;
                            }
                        }
                        if (isFloatType(inputType) || isFloatType(outputType))
                        {
                            switch (testType)
                            {
                            case TT_AND:
                            case TT_OR:
                            case TT_XOR:
                            case TT_NOT:
                            case TT_SHL:
                            case TT_SHR:
                                // unsupported for float types
                                continue;
                            default:
                                break;
                            }
                        }

                        uint32_t threadsPerWorkgroupX = 8u;
                        uint32_t threadsPerWorkgroupY = 8u;
                        uint32_t workgroupsX          = 2u;
                        uint32_t workgroupsY          = 2u;

                        if (stageCases[stageNdx].value == STAGE_GEOMETRY ||
                            stageCases[stageNdx].value == STAGE_TESS_CTRL ||
                            stageCases[stageNdx].value == STAGE_TESS_EVAL || stageCases[stageNdx].value == STAGE_TASK ||
                            stageCases[stageNdx].value == STAGE_MESH)
                        {
                            threadsPerWorkgroupX = 32u;
                            threadsPerWorkgroupY = 1u;
                        }

                        CaseDef c = {
                            (Stage)stageCases[stageNdx].value, // Stage stage;
                            testType,                          // TestType testtype;
                            threadsPerWorkgroupX,              // uint32_t threadsPerWorkgroupX;
                            threadsPerWorkgroupY,              // uint32_t threadsPerWorkgroupY;
                            workgroupsX,                       // uint32_t workgroupsX;
                            workgroupsY,                       // uint32_t workgroupsY;
                            (VkComponentTypeKHR)inputType,     // VkComponentTypeKHR inputType;
                            (VkComponentTypeKHR)inputType,     // VkComponentTypeKHR inputInterpretation;
                            (VkComponentTypeKHR)inputType,     // VkComponentTypeKHR matrixType;
                            (VkComponentTypeKHR)outputType,    // VkComponentTypeKHR outputType;
                            false,                             // bool inputPacked;
                            {VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV}, // VkCooperativeVectorMatrixLayoutNV matrixLayout;
                            false,                                              // bool transpose;
                            (StorageClass)scCases[scNdx].value,                 // StorageClass storageClass;
                            sizeCases[sizeNdx].value[0],                        // uint32_t inputVectorSize;
                            sizeCases[sizeNdx].value[1],                        // uint32_t outputVectorSize;
                            ACT_NONE,                                           // Activation act0;
                            ACT_NONE,                                           // Activation act1;
                            ACT_NONE,                                           // Activation act2;
                            false,                                              // bool nonuniformOffset;
                            false,                                              // bool cfDivergent;
                            RESULT_ADDR_UNIFORM,                                // ResultAddress resultAddr;
                            false,                                              // bool uses64BitIndexing;
                        };
                        sizeGroup->addChild(new CooperativeVectorTestCase(testCtx, stageCases[stageNdx].name, c));
                    }
                    scGroup->addChild(sizeGroup.release());
                }
                dtGroup->addChild(scGroup.release());
            }
            ttGroup->addChild(dtGroup.release());
        }
        group->addChild(ttGroup.release());
    }
    return group.release();
}

tcu::TestCaseGroup *createCooperativeVectorMatrixMulTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(
        new tcu::TestCaseGroup(testCtx, "matmul", "cooperative_vector matrix multiply tests"));

    typedef struct
    {
        uint32_t value;
        const char *name;
        const char *description;
    } TestGroupCase;

    TestGroupCase ttCases[] = {
        {TT_MATRIXMUL, "matrixmul", "OpCooperativeVectorMatrixMulNV"},
        {TT_MATRIXMAD, "matrixmuladd", "OpCooperativeVectorMatrixMulAddNV"},
        {TT_MATRIXMADTRANSPOSE, "matrixmuladdtranspose", "OpCooperativeVectorMatrixMulAddNV"},
        {TT_MATRIXMUL3, "matrixmul3", "OpCooperativeVectorMatrixMulNV"},
        {TT_MATRIXMUL2ADDMUL2, "matrixmul2addmul2", "OpCooperativeVectorMatrixMulNV"},
        {TT_MATRIXMUL2ADD, "matrixmul2add", "OpCooperativeVectorMatrixMulNV"},
        {TT_MATRIXMUL_TRAININGBIAS, "matrixmultrainingbias", "Training layout with componentwise bias"},
    };

    TestGroupCaseN<5> dtCases[] = {
        //  inputType,                      inputInterpretation,            matrixInterpretation,           outputType,                     packed
        {{VK_COMPONENT_TYPE_FLOAT16_NV, VK_COMPONENT_TYPE_FLOAT16_NV, VK_COMPONENT_TYPE_FLOAT16_NV,
          VK_COMPONENT_TYPE_FLOAT16_NV, VK_FALSE},
         "float16_float16_float16_float16",
         "float16_float16_float16_float16"},
        {{VK_COMPONENT_TYPE_UINT8_KHR, VK_COMPONENT_TYPE_UINT8_KHR, VK_COMPONENT_TYPE_UINT8_KHR,
          VK_COMPONENT_TYPE_UINT32_KHR, VK_FALSE},
         "uint8_uint8_uint8_uint32",
         "uint8_uint8_uint8_uint32"},
        {{VK_COMPONENT_TYPE_UINT8_KHR, VK_COMPONENT_TYPE_UINT8_KHR, VK_COMPONENT_TYPE_UINT8_KHR,
          VK_COMPONENT_TYPE_SINT32_KHR, VK_FALSE},
         "uint8_uint8_uint8_sint32",
         "uint8_uint8_uint8_sint32"},
        {{VK_COMPONENT_TYPE_UINT8_KHR, VK_COMPONENT_TYPE_UINT8_KHR, VK_COMPONENT_TYPE_SINT8_KHR,
          VK_COMPONENT_TYPE_SINT32_KHR, VK_FALSE},
         "uint8_uint8_sint8_sint32",
         "uint8_uint8_sint8_sint32"},
        {{VK_COMPONENT_TYPE_SINT8_KHR, VK_COMPONENT_TYPE_SINT8_KHR, VK_COMPONENT_TYPE_UINT8_KHR,
          VK_COMPONENT_TYPE_SINT32_KHR, VK_FALSE},
         "sint8_sint8_uint8_sint32",
         "sint8_sint8_uint8_sint32"},
        {{VK_COMPONENT_TYPE_SINT8_KHR, VK_COMPONENT_TYPE_SINT8_KHR, VK_COMPONENT_TYPE_SINT8_KHR,
          VK_COMPONENT_TYPE_SINT32_KHR, VK_FALSE},
         "sint8_sint8_sint8_sint32",
         "sint8_sint8_sint8_sint32"},
        {{VK_COMPONENT_TYPE_SINT8_KHR, VK_COMPONENT_TYPE_SINT8_KHR, VK_COMPONENT_TYPE_SINT8_KHR,
          VK_COMPONENT_TYPE_SINT32_KHR, VK_TRUE},
         "sint8packed_sint8_sint8_sint32",
         "sint8packed_sint8_sint8_sint32"},
        {{VK_COMPONENT_TYPE_UINT8_KHR, VK_COMPONENT_TYPE_UINT8_KHR, VK_COMPONENT_TYPE_SINT8_KHR,
          VK_COMPONENT_TYPE_SINT32_KHR, VK_TRUE},
         "uint8packed_uint8_sint8_sint32",
         "uint8packed_uint8_sint8_sint32"},
        {{VK_COMPONENT_TYPE_SINT32_KHR, VK_COMPONENT_TYPE_SINT8_KHR, VK_COMPONENT_TYPE_SINT8_KHR,
          VK_COMPONENT_TYPE_SINT32_KHR, VK_FALSE},
         "sint32_sint8_sint8_sint32",
         "sint32_sint8_sint8_sint32"},
        {{VK_COMPONENT_TYPE_FLOAT32_KHR, VK_COMPONENT_TYPE_SINT8_KHR, VK_COMPONENT_TYPE_SINT8_KHR,
          VK_COMPONENT_TYPE_SINT32_KHR, VK_FALSE},
         "float32_sint8_sint8_sint32",
         "float32_sint8_sint8_sint32"},
        {{VK_COMPONENT_TYPE_FLOAT16_NV, VK_COMPONENT_TYPE_FLOAT_E4M3_NV, VK_COMPONENT_TYPE_FLOAT_E4M3_NV,
          VK_COMPONENT_TYPE_FLOAT16_NV, VK_FALSE},
         "float16_floate4m3_floate4m3_float16",
         "float16_floate4m3_floate4m3_float16"},
        {{VK_COMPONENT_TYPE_FLOAT16_NV, VK_COMPONENT_TYPE_FLOAT_E5M2_NV, VK_COMPONENT_TYPE_FLOAT_E5M2_NV,
          VK_COMPONENT_TYPE_FLOAT16_NV, VK_FALSE},
         "float16_floate5m2_floate5m2_float16",
         "float16_floate5m2_floate5m2_float16"},
    };

    // Names are "NxK"
    TestGroupCaseN<2> sizeCases[] = {
        {{1, 1}, "1x1", "1 component input (K), 1 component output (N)"},
        {{2, 2}, "2x2", "2 component input (K), 2 component output (N)"},
        {{10, 1}, "10x1", "1 component input (K), 10 component output (N)"},
        {{1, 10}, "1x10", "10 component input (K), 1 component output (N)"},
        {{40, 5}, "40x5", "5 component input (K), 40 component output (N)"},
        {{5, 40}, "5x40", "40 component input (K), 5 component output (N)"},
        {{8, 8}, "8x8", "8 component input (K), 8 component output (N)"},
        {{16, 8}, "16x8", "8 component input (K), 16 component output (N)"},
        {{8, 16}, "8x16", "16 component input (K), 8 component output (N)"},
        {{16, 16}, "16x16", "16 component input (K), 16 component output (N)"},
        {{7, 13}, "7x13", "13 component input (K), 7 component output (N)"},
        {{32, 32}, "32x32", "32 component input (K), 32 component output (N)"},
        {{21, 35}, "21x35", "35 component input (K), 21 component output (N)"},
        {{19, 51}, "19x51", "51 component input (K), 19 component output (N)"},
        {{51, 19}, "51x19", "19 component input (K), 51 component output (N)"},
        {{128, 128}, "128x128", "128 component input (K), 128 component output (N)"},
    };

    TestGroupCaseN<3> actCases[] = {
        {{ACT_NONE, ACT_NONE, ACT_NONE}, "no_activation", ""},
        {{ACT_MUL, ACT_MUL, ACT_MUL}, "actmul", ""},
        {{ACT_MAX, ACT_MAX, ACT_MAX}, "actmax", ""},
        {{ACT_NONUNIF, ACT_NONUNIF, ACT_NONUNIF}, "actnonuniform", ""},
        {{ACT_DIVERGE, ACT_DIVERGE, ACT_DIVERGE}, "actdivergent", ""},
        {{ACT_SIGMOID, ACT_SIGMOID, ACT_SIGMOID}, "actsigmoid", ""},
        {{ACT_LEAKYRELUSTEP, ACT_LEAKYRELUSTEP, ACT_LEAKYRELUSTEP}, "actleakyrelustep", ""},
        {{ACT_LEAKYRELUMAX, ACT_LEAKYRELUMAX, ACT_LEAKYRELUMAX}, "actleakyrelumax", ""},
        {{ACT_HARDGELU, ACT_HARDGELU, ACT_HARDGELU}, "acthardgelu", ""},
        {{ACT_LOAD, ACT_LOAD, ACT_LOAD}, "actload", ""},
        {{ACT_LOAD_SHARED, ACT_LOAD_SHARED, ACT_LOAD_SHARED}, "actloadshared", ""},
        {{ACT_LOAD_READONLY, ACT_LOAD_READONLY, ACT_LOAD_READONLY}, "actloadreadonly", ""},
    };

    TestGroupCase colCases[] = {
        {VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV, "rowMajor", "Row major"},
        {VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_COLUMN_MAJOR_NV, "colMajor", "Column major"},
        {VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_INFERENCING_OPTIMAL_NV, "inferencingOptimal", "Inferencing Optimal"},
        {VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_TRAINING_OPTIMAL_NV, "trainingOptimal", "Training Optimal"},
    };

    TestGroupCase nonunifCases[] = {
        {0, "uniformoffset", "uniformoffset"},
        {1, "nonuniformoffset", "nonuniformoffset"},
    };

    TestGroupCase cfCases[] = {
        {0, "cfuniform", "control flow uniform"},
        {1, "cfdivergent", "control flow divergent"},
    };

    TestGroupCase scCases[] = {
        {SC_BUFFER, "buffer", "SSBO"},
        {SC_WORKGROUP, "workgroup", "shared memory"},
        {SC_BUFFER_VARIABLE_POINTERS, "buffer_varptr", "SSBO w/variable pointers"},
        {SC_WORKGROUP_VARIABLE_POINTERS, "workgroup_varptr", "shared memory w/variable pointers"},
        {SC_PHYSICAL_STORAGE_BUFFER, "physical_buffer", "physical_storage_buffer"},
    };

    TestGroupCaseN<3> stageCases[] = {
        {{STAGE_COMPUTE, 71, 2}, "compute71x2", "compute71x2"},
        {{STAGE_RAYGEN, 71, 2}, "raygen71x2", "raygen71x2"},
        {{STAGE_INTERSECT, 71, 2}, "isect71x2", "intersect71x2"},
        {{STAGE_ANY_HIT, 71, 2}, "ahit71x2", "any_hit71x2"},
        {{STAGE_CLOSEST_HIT, 71, 2}, "chit71x2", "closest_hit71x2"},
        {{STAGE_MISS, 71, 2}, "miss71x2", "miss71x2"},
        {{STAGE_CALLABLE, 71, 2}, "callable71x2", "callable71x2"},
        {{STAGE_VERTEX, 71, 1}, "vertex71x1", "vertex71x1"},
        {{STAGE_FRAGMENT, 13, 8}, "fragment13x8", "fragment13x8"},
        {{STAGE_GEOMETRY, 32, 1}, "geometry32x1", "geometry32x1"},
        {{STAGE_TESS_CTRL, 32, 1}, "tessctrl32x1", "tessctrl32x1"},
        {{STAGE_TESS_EVAL, 32, 1}, "tesseval32x1", "tesseval32x1"},
        {{STAGE_TASK, 37, 2}, "task37x2", "task37x2"},
        {{STAGE_MESH, 37, 2}, "mesh37x2", "mesh37x2"},
        {{STAGE_TASK, 31, 1}, "task31x1", "task31x1"},
        {{STAGE_MESH, 31, 1}, "mesh31x1", "mesh31x1"},
    };

    for (int ttNdx = 0; ttNdx < DE_LENGTH_OF_ARRAY(ttCases); ttNdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> ttGroup(
            new tcu::TestCaseGroup(testCtx, ttCases[ttNdx].name, ttCases[ttNdx].description));
        for (int dtNdx = 0; dtNdx < DE_LENGTH_OF_ARRAY(dtCases); dtNdx++)
        {
            de::MovePtr<tcu::TestCaseGroup> dtGroup(
                new tcu::TestCaseGroup(testCtx, dtCases[dtNdx].name, dtCases[dtNdx].description));
            for (int scNdx = 0; scNdx < DE_LENGTH_OF_ARRAY(scCases); scNdx++)
            {
                de::MovePtr<tcu::TestCaseGroup> scGroup(
                    new tcu::TestCaseGroup(testCtx, scCases[scNdx].name, scCases[scNdx].description));
                for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizeCases); sizeNdx++)
                {
                    de::MovePtr<tcu::TestCaseGroup> sizeGroup(
                        new tcu::TestCaseGroup(testCtx, sizeCases[sizeNdx].name, sizeCases[sizeNdx].description));
                    for (int actNdx = 0; actNdx < DE_LENGTH_OF_ARRAY(actCases); actNdx++)
                    {
                        de::MovePtr<tcu::TestCaseGroup> activationGroup(
                            new tcu::TestCaseGroup(testCtx, actCases[actNdx].name, actCases[actNdx].description));
                        for (int nuNdx = 0; nuNdx < DE_LENGTH_OF_ARRAY(nonunifCases); nuNdx++)
                        {
                            de::MovePtr<tcu::TestCaseGroup> nonunifGroup(new tcu::TestCaseGroup(
                                testCtx, nonunifCases[nuNdx].name, nonunifCases[nuNdx].description));
                            for (int cfNdx = 0; cfNdx < DE_LENGTH_OF_ARRAY(cfCases); cfNdx++)
                            {
                                de::MovePtr<tcu::TestCaseGroup> cfGroup(
                                    new tcu::TestCaseGroup(testCtx, cfCases[cfNdx].name, cfCases[cfNdx].description));
                                for (int colNdx = 0; colNdx < DE_LENGTH_OF_ARRAY(colCases); colNdx++)
                                {
                                    de::MovePtr<tcu::TestCaseGroup> colGroup(new tcu::TestCaseGroup(
                                        testCtx, colCases[colNdx].name, colCases[colNdx].description));
                                    for (int stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stageCases); stageNdx++)
                                    {
                                        TestType testType            = (TestType)ttCases[ttNdx].value;
                                        VkComponentTypeKHR inputType = (VkComponentTypeKHR)dtCases[dtNdx].value[0];
                                        VkComponentTypeKHR inputInterpretation =
                                            (VkComponentTypeKHR)dtCases[dtNdx].value[1];
                                        VkComponentTypeKHR matrixType = (VkComponentTypeKHR)dtCases[dtNdx].value[2];
                                        VkComponentTypeKHR outputType = (VkComponentTypeKHR)dtCases[dtNdx].value[3];

                                        if ((scCases[scNdx].value == SC_WORKGROUP ||
                                             scCases[scNdx].value == SC_WORKGROUP_VARIABLE_POINTERS) &&
                                            stageCases[stageNdx].value[0] != STAGE_COMPUTE)
                                        {
                                            continue;
                                        }

                                        if (!(colCases[colNdx].value ==
                                                  VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_INFERENCING_OPTIMAL_NV ||
                                              colCases[colNdx].value ==
                                                  VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_TRAINING_OPTIMAL_NV))
                                        {

                                            // Transpose is not supported for row/col-major
                                            if (testType == TT_MATRIXMADTRANSPOSE)
                                            {
                                                continue;
                                            }
                                            // FP8 matrix must be optimal
                                            if (matrixType == VK_COMPONENT_TYPE_FLOAT_E4M3_NV ||
                                                matrixType == VK_COMPONENT_TYPE_FLOAT_E5M2_NV)
                                            {
                                                continue;
                                            }
                                        }

                                        if (!isFloatType(outputType))
                                        {
                                            // Some activations not supported for integer types
                                            switch (actCases[actNdx].value[0])
                                            {
                                            case ACT_SIGMOID:
                                            case ACT_LEAKYRELUSTEP:
                                            case ACT_LEAKYRELUMAX:
                                                continue;
                                            }
                                        }

                                        switch (actCases[actNdx].value[0])
                                        {
                                        case ACT_SIGMOID:
                                        case ACT_HARDGELU:
                                            // Nonlinear activation functions introduce imprecision which can be magnified
                                            // with quantization to small types. Skip for now.
                                            if (inputInterpretation == VK_COMPONENT_TYPE_FLOAT_E4M3_NV ||
                                                inputInterpretation == VK_COMPONENT_TYPE_FLOAT_E5M2_NV)
                                            {
                                                continue;
                                            }
                                            break;
                                        default:
                                            break;
                                        }

                                        if (stageCases[stageNdx].value[0] != STAGE_COMPUTE &&
                                            (isSIntType(inputType) != isSIntType(matrixType)) &&
                                            !(sizeCases[sizeNdx].value[0] == 21 && sizeCases[sizeNdx].value[1] == 35))
                                            continue;

                                        if (actCases[actNdx].value[0] == ACT_LOAD_READONLY && !isFloatType(outputType))
                                        {
                                            continue;
                                        }
                                        // Limit combinations of tests we run with each activation function.
                                        // Run mul everywhere. Run load for all dimensions. Run hardgelu with
                                        // all sizes for float input type. Otherwise, run all activations only
                                        // for 40x5 (chosen arbitrarily).
                                        switch (actCases[actNdx].value[0])
                                        {
                                        case ACT_MUL:
                                            break;
                                        case ACT_LOAD:
                                            if ((stageCases[stageNdx].value[0] == STAGE_COMPUTE ||
                                                 stageCases[stageNdx].value[0] == STAGE_CLOSEST_HIT ||
                                                 stageCases[stageNdx].value[0] == STAGE_VERTEX) &&
                                                nonunifCases[nuNdx].value &&
                                                colCases[colNdx].value ==
                                                    VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_INFERENCING_OPTIMAL_NV &&
                                                !(scCases[scNdx].value == SC_WORKGROUP ||
                                                  scCases[scNdx].value == SC_WORKGROUP_VARIABLE_POINTERS))
                                            {
                                                break;
                                            }
                                            continue;
                                        case ACT_LOAD_SHARED:
                                            if (stageCases[stageNdx].value[0] == STAGE_COMPUTE &&
                                                colCases[colNdx].value ==
                                                    VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_INFERENCING_OPTIMAL_NV &&
                                                (scCases[scNdx].value == SC_BUFFER ||
                                                 scCases[scNdx].value == SC_WORKGROUP))
                                            {
                                                break;
                                            }
                                            continue;
                                        case ACT_HARDGELU:
                                            if (inputType == VK_COMPONENT_TYPE_FLOAT32_KHR)
                                            {
                                                break;
                                            }
                                            // fallthrough
                                        default:
                                            if (sizeCases[sizeNdx].value[0] == 40 && sizeCases[sizeNdx].value[1] == 5)
                                            {
                                                break;
                                            }
                                            continue;
                                        }

                                        // Only run physical storage buffer and variable pointer tests for 16x16, to reduce test count
                                        if ((scCases[scNdx].value == SC_PHYSICAL_STORAGE_BUFFER ||
                                             scCases[scNdx].value == SC_BUFFER_VARIABLE_POINTERS ||
                                             scCases[scNdx].value == SC_WORKGROUP_VARIABLE_POINTERS) &&
                                            !(sizeCases[sizeNdx].value[0] == 16 && sizeCases[sizeNdx].value[1] == 16))
                                        {
                                            continue;
                                        }

                                        // reduce test count
                                        if (ttCases[ttNdx].value != TT_MATRIXMUL2ADDMUL2 &&
                                            scCases[scNdx].value != SC_BUFFER)
                                        {
                                            continue;
                                        }

                                        // reduce test count
                                        if ((ttCases[ttNdx].value == TT_MATRIXMUL2ADD ||
                                             ttCases[ttNdx].value == TT_MATRIXMUL) &&
                                            stageCases[stageNdx].value[0] != STAGE_COMPUTE)
                                        {
                                            continue;
                                        }

                                        // reduce test count
                                        if (stageCases[stageNdx].value[0] != STAGE_COMPUTE &&
                                            sizeCases[sizeNdx].value[0] * sizeCases[sizeNdx].value[1] == 51 * 19)
                                        {
                                            continue;
                                        }

                                        // Only run uniformoffset tests for 16x16, to reduce test count
                                        if (nonunifCases[nuNdx].value == false &&
                                            !(sizeCases[sizeNdx].value[0] == 16 && sizeCases[sizeNdx].value[1] == 16))
                                        {
                                            continue;
                                        }

                                        // Only run control flow divergence tests for 21x35, to reduce test count
                                        if (cfCases[cfNdx].value != 0 &&
                                            !(sizeCases[sizeNdx].value[0] == 21 && sizeCases[sizeNdx].value[1] == 35))
                                        {
                                            continue;
                                        }

                                        // Only run non-inferencing layouts in compute/intersect/fragment, to reduce test count
                                        if (colCases[colNdx].value !=
                                                VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_INFERENCING_OPTIMAL_NV &&
                                            !(stageCases[stageNdx].value[0] == STAGE_COMPUTE ||
                                              stageCases[stageNdx].value[0] == STAGE_INTERSECT ||
                                              stageCases[stageNdx].value[0] == STAGE_FRAGMENT))
                                        {
                                            continue;
                                        }

                                        if (ttCases[ttNdx].value == TT_MATRIXMUL_TRAININGBIAS &&
                                            colCases[colNdx].value !=
                                                VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_TRAINING_OPTIMAL_NV)
                                        {
                                            continue;
                                        }

                                        // Spec only allows manually modifying the training layout for these types
                                        if (ttCases[ttNdx].value == TT_MATRIXMUL_TRAININGBIAS &&
                                            matrixType != VK_COMPONENT_TYPE_FLOAT16_KHR &&
                                            matrixType != VK_COMPONENT_TYPE_FLOAT32_KHR)
                                        {
                                            continue;
                                        }

                                        // Test max size, but few variations because it'll be slower.
                                        if (sizeCases[sizeNdx].value[0] == 128)
                                        {
                                            if (ttCases[ttNdx].value != TT_MATRIXMUL ||
                                                actCases[actNdx].value[0] != ACT_MUL)
                                            {
                                                continue;
                                            }
                                        }

                                        uint32_t threadsPerWorkgroupX = stageCases[stageNdx].value[1];
                                        uint32_t threadsPerWorkgroupY = stageCases[stageNdx].value[2];
                                        uint32_t workgroupsX          = 2u;
                                        uint32_t workgroupsY          = 2u;

                                        CaseDef c = {
                                            (Stage)stageCases[stageNdx].value[0], // Stage stage;
                                            testType,                             // TestType testtype;
                                            threadsPerWorkgroupX,                 // uint32_t threadsPerWorkgroupX;
                                            threadsPerWorkgroupY,                 // uint32_t threadsPerWorkgroupY;
                                            workgroupsX,                          // uint32_t workgroupsX;
                                            workgroupsY,                          // uint32_t workgroupsY;
                                            (VkComponentTypeKHR)inputType,        // VkComponentTypeKHR inputType;
                                            (VkComponentTypeKHR)
                                                inputInterpretation,        // VkComponentTypeKHR inputInterpretation;
                                            (VkComponentTypeKHR)matrixType, // VkComponentTypeKHR matrixType;
                                            (VkComponentTypeKHR)outputType, // VkComponentTypeKHR outputType;
                                            !!dtCases[dtNdx].value[4],      // bool inputPacked;
                                            {
                                                (VkCooperativeVectorMatrixLayoutNV)colCases[colNdx].value,
                                                swapRowColMajor(
                                                    (VkCooperativeVectorMatrixLayoutNV)colCases[colNdx].value),
                                                (VkCooperativeVectorMatrixLayoutNV)colCases[colNdx].value,
                                            }, // VkCooperativeVectorMatrixLayoutNV matrixLayout;
                                            testType == TT_MATRIXMADTRANSPOSE,     // bool transpose;
                                            (StorageClass)scCases[scNdx].value,    // StorageClass storageClass;
                                            sizeCases[sizeNdx].value[1],           // uint32_t inputVectorSize;
                                            sizeCases[sizeNdx].value[0],           // uint32_t outputVectorSize;
                                            (Activation)actCases[actNdx].value[0], // Activation act0;
                                            (Activation)actCases[actNdx].value[1], // Activation act1;
                                            (Activation)actCases[actNdx].value[2], // Activation act2;
                                            !!nonunifCases[nuNdx].value,           // bool nonuniformOffset;
                                            !!cfCases[cfNdx].value,                // bool cfDivergent;
                                            RESULT_ADDR_UNIFORM,                   // ResultAddress resultAddr;
                                            false,                                 // bool uses64BitIndexing;
                                        };
                                        colGroup->addChild(
                                            new CooperativeVectorTestCase(testCtx, stageCases[stageNdx].name, c));
                                    }
                                    cfGroup->addChild(colGroup.release());
                                }
                                nonunifGroup->addChild(cfGroup.release());
                            }
                            activationGroup->addChild(nonunifGroup.release());
                        }
                        sizeGroup->addChild(activationGroup.release());
                    }
                    scGroup->addChild(sizeGroup.release());
                }
                dtGroup->addChild(scGroup.release());
            }
            ttGroup->addChild(dtGroup.release());
        }
        group->addChild(ttGroup.release());
    }

    de::MovePtr<tcu::TestCaseGroup> group64(new tcu::TestCaseGroup(testCtx, "64b_indexing"));

    // 64bit indexing test cases
    for (int stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stageCases); stageNdx++)
    {
        uint32_t threadsPerWorkgroupX = stageCases[stageNdx].value[1];
        uint32_t threadsPerWorkgroupY = stageCases[stageNdx].value[2];
        uint32_t workgroupsX          = 2u;
        uint32_t workgroupsY          = 2u;

        CaseDef c = {
            (Stage)stageCases[stageNdx].value[0], // Stage stage;
            TT_MATRIXMAD,                         // TestType testtype;
            threadsPerWorkgroupX,                 // uint32_t threadsPerWorkgroupX;
            threadsPerWorkgroupY,                 // uint32_t threadsPerWorkgroupY;
            workgroupsX,                          // uint32_t workgroupsX;
            workgroupsY,                          // uint32_t workgroupsY;
            VK_COMPONENT_TYPE_FLOAT16_NV,         // VkComponentTypeKHR inputType;
            VK_COMPONENT_TYPE_FLOAT16_NV,         // VkComponentTypeKHR inputInterpretation;
            VK_COMPONENT_TYPE_FLOAT16_NV,         // VkComponentTypeKHR matrixType;
            VK_COMPONENT_TYPE_FLOAT16_NV,         // VkComponentTypeKHR outputType;
            false,                                // bool inputPacked;
            {
                VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_INFERENCING_OPTIMAL_NV,
                VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_INFERENCING_OPTIMAL_NV,
                VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_INFERENCING_OPTIMAL_NV,
            },                   // VkCooperativeVectorMatrixLayoutNV matrixLayout;
            false,               // bool transpose;
            SC_BUFFER,           // StorageClass storageClass;
            5,                   // uint32_t inputVectorSize;
            5,                   // uint32_t outputVectorSize;
            ACT_NONE,            // Activation act0;
            ACT_NONE,            // Activation act1;
            ACT_NONE,            // Activation act2;
            false,               // bool nonuniformOffset;
            false,               // bool cfDivergent;
            RESULT_ADDR_UNIFORM, // ResultAddress resultAddr;
            true,                // bool uses64BitIndexing;
        };
        std::string name = std::string("muladd_") + stageCases[stageNdx].name;
        group64->addChild(new CooperativeVectorTestCase(testCtx, name.c_str(), c));
    }
    group->addChild(group64.release());

    return group.release();
}

tcu::TestCaseGroup *createCooperativeVectorTrainingTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "training", "cooperative_vector tests"));

    typedef struct
    {
        uint32_t value;
        const char *name;
        const char *description;
    } TestGroupCase;

    TestGroupCase ttCases[] = {
        {TT_REDUCESUM, "reducesum", "reducesum"},
        {TT_OUTERPRODUCT, "outerproduct", "outerproduct"},
    };

    TestGroupCase dtCases[] = {
        {VK_COMPONENT_TYPE_FLOAT16_NV, "float16", "float16"},
        {VK_COMPONENT_TYPE_FLOAT32_NV, "float32", "float32"},
    };

    TestGroupCaseN<2> sizeCasesReduce[] = {
        {{1, 1}, "components1", "1 components"},     {{2, 2}, "components2", "2 components"},
        {{3, 3}, "components3", "3 components"},     {{4, 4}, "components4", "4 components"},
        {{5, 5}, "components5", "5 components"},     {{6, 6}, "components6", "6 components"},
        {{7, 7}, "components7", "7 components"},     {{8, 8}, "components8", "8 components"},
        {{9, 9}, "components9", "9 components"},     {{31, 31}, "components31", "31 components"},
        {{65, 65}, "components65", "65 components"},
    };
    uint32_t numSizeReduce = DE_LENGTH_OF_ARRAY(sizeCasesReduce);

    // Names are "NxK"
    TestGroupCaseN<2> sizeCasesOuter[] = {
        {{1, 1}, "1x1", "1 component input (K), 1 component output (N)"},
        {{2, 2}, "2x2", "2 component input (K), 2 component output (N)"},
        {{10, 1}, "10x1", "1 component input (K), 10 component output (N)"},
        {{1, 10}, "1x10", "10 component input (K), 1 component output (N)"},
        {{40, 5}, "40x5", "5 component input (K), 40 component output (N)"},
        {{5, 40}, "5x40", "40 component input (K), 5 component output (N)"},
        {{8, 8}, "8x8", "8 component input (K), 8 component output (N)"},
        {{16, 8}, "16x8", "8 component input (K), 16 component output (N)"},
        {{8, 16}, "8x16", "16 component input (K), 8 component output (N)"},
        {{16, 16}, "16x16", "16 component input (K), 16 component output (N)"},
        {{7, 13}, "7x13", "13 component input (K), 7 component output (N)"},
        {{32, 32}, "32x32", "32 component input (K), 32 component output (N)"},
        {{21, 35}, "21x35", "35 component input (K), 21 component output (N)"},
        {{19, 51}, "19x51", "51 component input (K), 19 component output (N)"},
        {{51, 19}, "51x19", "19 component input (K), 51 component output (N)"},
    };
    uint32_t numSizeOuter = DE_LENGTH_OF_ARRAY(sizeCasesOuter);

    TestGroupCase scCases[] = {
        {SC_BUFFER, "buffer", "SSBO"},
        {SC_BUFFER_VARIABLE_POINTERS, "buffer_varptr", "SSBO w/variable pointers"},
        {SC_PHYSICAL_STORAGE_BUFFER, "physical_buffer", "physical_storage_buffer"},
    };

    TestGroupCase colCases[] = {
        {VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_TRAINING_OPTIMAL_NV, "trainingOptimal", "Training Optimal"},
    };

    TestGroupCaseN<3> stageCases[] = {
        {{STAGE_COMPUTE, 71, 2}, "compute71x2", "compute71x2"},
        {{STAGE_RAYGEN, 71, 2}, "raygen71x2", "raygen71x2"},
        {{STAGE_INTERSECT, 71, 2}, "isect71x2", "intersect71x2"},
        {{STAGE_ANY_HIT, 71, 2}, "ahit71x2", "any_hit71x2"},
        {{STAGE_CLOSEST_HIT, 71, 2}, "chit71x2", "closest_hit71x2"},
        {{STAGE_MISS, 71, 2}, "miss71x2", "miss71x2"},
        {{STAGE_CALLABLE, 71, 2}, "callable71x2", "callable71x2"},
        {{STAGE_VERTEX, 71, 1}, "vertex71x1", "vertex71x1"},
        {{STAGE_FRAGMENT, 13, 8}, "fragment13x8", "fragment13x8"},
        {{STAGE_GEOMETRY, 32, 1}, "geometry32x1", "geometry32x1"},
        {{STAGE_TESS_CTRL, 32, 1}, "tessctrl32x1", "tessctrl32x1"},
        {{STAGE_TESS_EVAL, 32, 1}, "tesseval32x1", "tesseval32x1"},
        {{STAGE_TASK, 37, 2}, "task37x2", "task37x2"},
        {{STAGE_MESH, 37, 2}, "mesh37x2", "mesh37x2"},
        {{STAGE_TASK, 31, 1}, "task31x1", "task31x1"},
        {{STAGE_MESH, 31, 1}, "mesh31x1", "mesh31x1"},
    };

    TestGroupCase nonunifCases[] = {
        {RESULT_ADDR_UNIFORM, "resultuniform", "resultuniform"},
        {RESULT_ADDR_UNIQUE, "resultunique", "resultunique"},
        {RESULT_ADDR_CLUSTERED, "resultclustered", "resultclustered"},
    };

    TestGroupCase cfCases[] = {
        {0, "cfuniform", "control flow uniform"},
        {1, "cfdivergent", "control flow divergent"},
    };

    for (int ttNdx = 0; ttNdx < DE_LENGTH_OF_ARRAY(ttCases); ttNdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> ttGroup(
            new tcu::TestCaseGroup(testCtx, ttCases[ttNdx].name, ttCases[ttNdx].description));
        for (int dtNdx = 0; dtNdx < DE_LENGTH_OF_ARRAY(dtCases); dtNdx++)
        {
            TestType testType = (TestType)ttCases[ttNdx].value;
            de::MovePtr<tcu::TestCaseGroup> dtGroup(
                new tcu::TestCaseGroup(testCtx, dtCases[dtNdx].name, dtCases[dtNdx].description));
            for (int scNdx = 0; scNdx < DE_LENGTH_OF_ARRAY(scCases); scNdx++)
            {
                de::MovePtr<tcu::TestCaseGroup> scGroup(
                    new tcu::TestCaseGroup(testCtx, scCases[scNdx].name, scCases[scNdx].description));

                TestGroupCaseN<2> *sizeCases = (testType == TT_REDUCESUM) ? sizeCasesReduce : sizeCasesOuter;
                int numSizes                 = (testType == TT_REDUCESUM) ? numSizeReduce : numSizeOuter;
                for (int sizeNdx = 0; sizeNdx < numSizes; sizeNdx++)
                {
                    de::MovePtr<tcu::TestCaseGroup> sizeGroup(
                        new tcu::TestCaseGroup(testCtx, sizeCases[sizeNdx].name, sizeCases[sizeNdx].description));
                    for (int nuNdx = 0; nuNdx < DE_LENGTH_OF_ARRAY(nonunifCases); nuNdx++)
                    {
                        de::MovePtr<tcu::TestCaseGroup> nonunifGroup(
                            new tcu::TestCaseGroup(testCtx, nonunifCases[nuNdx].name, nonunifCases[nuNdx].description));
                        for (int cfNdx = 0; cfNdx < DE_LENGTH_OF_ARRAY(cfCases); cfNdx++)
                        {
                            de::MovePtr<tcu::TestCaseGroup> cfGroup(
                                new tcu::TestCaseGroup(testCtx, cfCases[cfNdx].name, cfCases[cfNdx].description));
                            for (int colNdx = 0; colNdx < DE_LENGTH_OF_ARRAY(colCases); colNdx++)
                            {
                                de::MovePtr<tcu::TestCaseGroup> colGroup(new tcu::TestCaseGroup(
                                    testCtx, colCases[colNdx].name, colCases[colNdx].description));
                                for (int stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stageCases); stageNdx++)
                                {
                                    VkComponentTypeKHR inputType  = (VkComponentTypeKHR)dtCases[dtNdx].value;
                                    VkComponentTypeKHR outputType = (VkComponentTypeKHR)dtCases[dtNdx].value;

                                    if (testType == TT_OUTERPRODUCT)
                                    {
                                        inputType = VK_COMPONENT_TYPE_FLOAT16_NV;
                                    }

                                    uint32_t threadsPerWorkgroupX = stageCases[stageNdx].value[1];
                                    uint32_t threadsPerWorkgroupY = stageCases[stageNdx].value[2];
                                    uint32_t workgroupsX          = 2u;
                                    uint32_t workgroupsY          = 2u;

                                    CaseDef c = {
                                        (Stage)stageCases[stageNdx].value[0], // Stage stage;
                                        testType,                             // TestType testtype;
                                        threadsPerWorkgroupX,                 // uint32_t threadsPerWorkgroupX;
                                        threadsPerWorkgroupY,                 // uint32_t threadsPerWorkgroupY;
                                        workgroupsX,                          // uint32_t workgroupsX;
                                        workgroupsY,                          // uint32_t workgroupsY;
                                        (VkComponentTypeKHR)inputType,        // VkComponentTypeKHR inputType;
                                        (VkComponentTypeKHR)inputType,        // VkComponentTypeKHR inputInterpretation;
                                        (VkComponentTypeKHR)outputType,       // VkComponentTypeKHR matrixType;
                                        (VkComponentTypeKHR)outputType,       // VkComponentTypeKHR outputType;
                                        false,                                // bool inputPacked;
                                        {
                                            (VkCooperativeVectorMatrixLayoutNV)colCases[colNdx].value,
                                        },     // VkCooperativeVectorMatrixLayoutNV matrixLayout;
                                        false, // bool transpose;
                                        (StorageClass)scCases[scNdx].value,       // StorageClass storageClass;
                                        sizeCases[sizeNdx].value[1],              // uint32_t inputVectorSize;
                                        sizeCases[sizeNdx].value[0],              // uint32_t outputVectorSize;
                                        ACT_NONE,                                 // Activation act0;
                                        ACT_NONE,                                 // Activation act1;
                                        ACT_NONE,                                 // Activation act2;
                                        !!nonunifCases[nuNdx].value,              // bool nonuniformOffset;
                                        !!cfCases[cfNdx].value,                   // bool cfDivergent;
                                        (ResultAddress)nonunifCases[nuNdx].value, // ResultAddress resultAddr;
                                        false,                                    // bool uses64BitIndexing;
                                    };
                                    colGroup->addChild(
                                        new CooperativeVectorTestCase(testCtx, stageCases[stageNdx].name, c));
                                }
                                cfGroup->addChild(colGroup.release());
                            }
                            nonunifGroup->addChild(cfGroup.release());
                        }
                        sizeGroup->addChild(nonunifGroup.release());
                    }
                    scGroup->addChild(sizeGroup.release());
                }
                dtGroup->addChild(scGroup.release());
            }
            ttGroup->addChild(dtGroup.release());
        }
        group->addChild(ttGroup.release());
    }

    de::MovePtr<tcu::TestCaseGroup> group64(new tcu::TestCaseGroup(testCtx, "64b_indexing"));

    // 64bit indexing test cases
    for (int stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stageCases); stageNdx++)
    {
        uint32_t threadsPerWorkgroupX = stageCases[stageNdx].value[1];
        uint32_t threadsPerWorkgroupY = stageCases[stageNdx].value[2];
        uint32_t workgroupsX          = 2u;
        uint32_t workgroupsY          = 2u;

        CaseDef c = {
            (Stage)stageCases[stageNdx].value[0], // Stage stage;
            TT_REDUCESUM,                         // TestType testtype;
            threadsPerWorkgroupX,                 // uint32_t threadsPerWorkgroupX;
            threadsPerWorkgroupY,                 // uint32_t threadsPerWorkgroupY;
            workgroupsX,                          // uint32_t workgroupsX;
            workgroupsY,                          // uint32_t workgroupsY;
            VK_COMPONENT_TYPE_FLOAT16_NV,         // VkComponentTypeKHR inputType;
            VK_COMPONENT_TYPE_FLOAT16_NV,         // VkComponentTypeKHR inputInterpretation;
            VK_COMPONENT_TYPE_FLOAT16_NV,         // VkComponentTypeKHR matrixType;
            VK_COMPONENT_TYPE_FLOAT16_NV,         // VkComponentTypeKHR outputType;
            false,                                // bool inputPacked;
            {
                VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_INFERENCING_OPTIMAL_NV,
                VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_INFERENCING_OPTIMAL_NV,
                VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_INFERENCING_OPTIMAL_NV,
            },                   // VkCooperativeVectorMatrixLayoutNV matrixLayout;
            false,               // bool transpose;
            SC_BUFFER,           // StorageClass storageClass;
            5,                   // uint32_t inputVectorSize;
            5,                   // uint32_t outputVectorSize;
            ACT_NONE,            // Activation act0;
            ACT_NONE,            // Activation act1;
            ACT_NONE,            // Activation act2;
            false,               // bool nonuniformOffset;
            false,               // bool cfDivergent;
            RESULT_ADDR_UNIFORM, // ResultAddress resultAddr;
            true,                // bool uses64BitIndexing;
        };
        std::string name = std::string("reducesum_") + stageCases[stageNdx].name;
        group64->addChild(new CooperativeVectorTestCase(testCtx, name.c_str(), c));

        c.testType = TT_OUTERPRODUCT;
        name       = std::string("outerproduct_") + stageCases[stageNdx].name;
        group64->addChild(new CooperativeVectorTestCase(testCtx, name.c_str(), c));
    }
    group->addChild(group64.release());

    return group.release();
}

} // namespace cooperative_vector
} // namespace vkt
