/*-------------------------------------------------------------------------
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
 * \file
 * \brief SPIR-V Assembly Tests for Compute Shader Derivatives
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmComputeShaderDerivativesTests.hpp"

#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vktTestCase.hpp"
#include "tcuStringTemplate.hpp"

#include "vktSpvAsmUtils.hpp"

#include "vkBuilderUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"

#include <vector>

namespace vkt
{
namespace SpirVAssembly
{
using namespace vk;

namespace
{

namespace Constants
{
constexpr VkExtent3D EXTENT                         = {32u, 64u, 1u};
constexpr VkExtent3D SAMPLED_EXTENT_2D              = {4u, 4u, 1u};
constexpr VkExtent3D SAMPLED_EXTENT_1D              = {16u, 1u, 1u};
constexpr VkFormat RENDER_TARGET_FORMAT             = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat SAMPLED_FORMAT                   = VK_FORMAT_R32G32B32A32_SFLOAT;
constexpr VkImageSubresourceRange IMAGE_SRR         = {vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
constexpr VkImageSubresourceRange SAMPLED_IMAGE_SRR = {vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 2u, 0u, 1u};
const tcu::Vec4 CLR_COLOR                           = {0.0f, 0.0f, 0.0f, 1.0f};
constexpr uint32_t MIP_LEVEL_COUNT                  = 2;
constexpr VkClearColorValue CLR_COLORS[2]           = {{{0.5f, 0.5f, 0.5f, 0.5f}}, {{1.0f, 1.0f, 1.0f, 1.0f}}};
} // namespace Constants

#define DE_ENUM_COUNT(enumClass) static_cast<uint32_t>((enumClass::_ENUM_COUNT))
#define DE_ENUM_INDEX(enumVal) static_cast<uint32_t>((enumVal))
#define DE_ENUM_IN_RANGE(enumVal, enumClass) \
    DE_ASSERT(static_cast<uint32_t>((enumVal)) < static_cast<uint32_t>((enumClass::_ENUM_COUNT)))

bool compareFloats(float a, float b, float treshold = 0.01f)
{
    return std::abs(a - b) <= treshold;
}

enum class TestType : uint8_t
{
    DERIVATIVE_VALUE = 0,
    VERIFY_NDX,
    QUAD_OPERATIONS,
    LOD_SAMPLE,
    LOD_QUERY,
    _ENUM_COUNT,
};

enum class DataType : uint8_t
{
    FLOAT32 = 0,
    VEC2_FLOAT32,
    VEC3_FLOAT32,
    VEC4_FLOAT32,
    _ENUM_COUNT,
};

enum class DerivativeFeature : uint8_t
{
    LINEAR = 0,
    QUADS,
    _ENUM_COUNT,
};

enum class DerivativeVariant : uint8_t
{
    NORMAL = 0,
    FINE,
    COARSE,
    _ENUM_COUNT,
};

enum class QuadOp : uint8_t
{
    BROADCAST = 0,
    SWAP,
    _ENUM_COUNT,
};

enum class ShaderType : uint8_t
{
    COMPUTE = 0,
    MESH,
    TASK,
    _ENUM_COUNT,
};

struct ComputeShaderDerivativeTestParams
{
    tcu::UVec3 numWorkgroup;
    TestType testType;
    DerivativeVariant variant;
    DerivativeFeature feature;
    QuadOp quadOp;
    uint32_t quadNdx;
    ShaderType shaderType;
    DataType dataType;
    uint32_t mipLvl;

    ComputeShaderDerivativeTestParams()
        : numWorkgroup(1, 1, 1)
        , testType(TestType::_ENUM_COUNT)
        , variant(DerivativeVariant::_ENUM_COUNT)
        , feature(DerivativeFeature::_ENUM_COUNT)
        , quadOp(QuadOp::_ENUM_COUNT)
        , quadNdx(0)
        , shaderType(ShaderType::_ENUM_COUNT)
        , dataType(DataType::_ENUM_COUNT)
        , mipLvl(0)
    {
    }
};

const char *toString(const DataType type)
{
    DE_ENUM_IN_RANGE(type, DataType);

    static const char *const translateTable[DE_ENUM_COUNT(DataType)] = {
        "float32",      // FLOAT32
        "vec2_float32", // VEC2_FLOAT32
        "vec3_float32", // VEC3_FLOAT32
        "vec4_float32", // VEC4_FLOAT32
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *toString(const DerivativeVariant var)
{
    DE_ENUM_IN_RANGE(var, DerivativeVariant);

    static const char *const translateTable[DE_ENUM_COUNT(DerivativeVariant)] = {
        "normal", // NORMAL
        "fine",   // FINE
        "coarse", // COARSE
    };

    return translateTable[DE_ENUM_INDEX(var)];
}

const char *toString(const QuadOp type)
{
    DE_ENUM_IN_RANGE(type, QuadOp);

    static const char *const translateTable[DE_ENUM_COUNT(QuadOp)] = {
        "broadcast", // BROADCAST
        "swap",      // SWAP
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *toString(const uint32_t ndx)
{
    DE_ASSERT(ndx < 4);

    static const char *const translateTable[4] = {
        "ndx_0", // 0
        "ndx_1", // 1
        "ndx_2", // 2
        "ndx_3", // 3
    };

    return translateTable[ndx];
}

const char *toString(const ShaderType type)
{
    DE_ENUM_IN_RANGE(type, ShaderType);

    static const char *const translateTable[DE_ENUM_COUNT(ShaderType)] = {
        "compute", // COMPUTE
        "mesh",    // MESH
        "task",    // TASK
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

uint32_t getDataAlignedSizeInBytes(DataType type)
{
    DE_ENUM_IN_RANGE(type, DataType);

    static const uint32_t translateTable[DE_ENUM_COUNT(DataType)] = {
        4,  // FLOAT32
        8,  // VEC2_FLOAT32
        16, // VEC3_FLOAT32
        16, // VEC4_FLOAT32
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

uint32_t getDataAlignedComponentCount(DataType type)
{
    DE_ENUM_IN_RANGE(type, DataType);

    static const uint32_t translateTable[DE_ENUM_COUNT(DataType)] = {
        1, // FLOAT32
        2, // VEC2_FLOAT32
        4, // VEC3_FLOAT32
        4, // VEC4_FLOAT32
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *getDataType(const DataType type)
{
    DE_ENUM_IN_RANGE(type, DataType);

    static const char *const translateTable[DE_ENUM_COUNT(DataType)] = {
        "%float32",      // FLOAT32
        "%vec2_float32", // VEC2_FLOAT32
        "%vec3_float32", // VEC3_FLOAT32
        "%vec4_float32", // VEC4_FLOAT32
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *getArrayDeclaration(const DataType type)
{
    DE_ENUM_IN_RANGE(type, DataType);

    static const char *const translateTable[DE_ENUM_COUNT(DataType)] = {
        "OpTypeArray %float32      %c_uint32_16", // FLOAT32
        "OpTypeArray %vec2_float32 %c_uint32_16", // VEC2_FLOAT32
        "OpTypeArray %vec3_float32 %c_uint32_16", // VEC3_FLOAT32
        "OpTypeArray %vec4_float32 %c_uint32_16", // VEC4_FLOAT32
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

const char *getUintArrayDeclaration()
{
    return "OpTypeArray %uint32 %c_uint32_128";
}

const char *getDerivativeCapability(DerivativeFeature feature)
{
    DE_ENUM_IN_RANGE(feature, DerivativeFeature);

    static const char *const translateTable[DE_ENUM_COUNT(DerivativeFeature)] = {
        "ComputeDerivativeGroupLinearKHR", // LINEAR
        "ComputeDerivativeGroupQuadsKHR",  // QUADS
    };

    return translateTable[DE_ENUM_INDEX(feature)];
}

const char *getDerivativeExecutionMode(DerivativeFeature feature)
{
    DE_ENUM_IN_RANGE(feature, DerivativeFeature);

    static const char *const translateTable[DE_ENUM_COUNT(DerivativeFeature)] = {
        "DerivativeGroupLinearKHR", // LINEAR
        "DerivativeGroupQuadsKHR",  // QUADS
    };

    return translateTable[DE_ENUM_INDEX(feature)];
}

const char *getDxFunc(DerivativeVariant var)
{
    DE_ENUM_IN_RANGE(var, DerivativeVariant);

    static const char *const translateTable[DE_ENUM_COUNT(DerivativeVariant)] = {
        "OpDPdx",       // NORMAL
        "OpDPdxFine",   // FINE
        "OpDPdxCoarse", // COARSE
    };

    return translateTable[DE_ENUM_INDEX(var)];
}

const char *getDyFunc(DerivativeVariant var)
{
    DE_ENUM_IN_RANGE(var, DerivativeVariant);

    static const char *const translateTable[DE_ENUM_COUNT(DerivativeVariant)] = {
        "OpDPdy",       // NORMAL
        "OpDPdyFine",   // FINE
        "OpDPdyCoarse", // COARSE
    };

    return translateTable[DE_ENUM_INDEX(var)];
}

const char *getWidthFunc(DerivativeVariant var)
{
    DE_ENUM_IN_RANGE(var, DerivativeVariant);

    static const char *const translateTable[DE_ENUM_COUNT(DerivativeVariant)] = {
        "OpFwidth",       // NORMAL
        "OpFwidthFine",   // FINE
        "OpFwidthCoarse", // COARSE
    };

    return translateTable[DE_ENUM_INDEX(var)];
}

std::string getTestValueCode(DerivativeFeature feature, DerivativeVariant var, DataType type)
{
    DE_ENUM_IN_RANGE(feature, DerivativeFeature);
    DE_ENUM_IN_RANGE(var, DerivativeVariant);
    DE_ENUM_IN_RANGE(type, DataType);

    std::string linStr("");
    std::string quadStr("");

    if (var == DerivativeVariant::FINE)
    {
        // According to test plan values for fine variant are not linear
        linStr += "%masked_ndx_uint32    = OpBitwiseAnd  %uint32  %ndx_uint32 %c_uint32_3\n"
                  "%masked_ndx_float32   = OpConvertUToF %float32 %masked_ndx_uint32\n"
                  "%masked_ndx_2_float32 = OpFMul        %float32 %masked_ndx_float32 %masked_ndx_float32\n"
                  "%scalar_value         = OpFMul        %float32 %c_float32_10 %masked_ndx_2_float32\n";
        quadStr += "%masked_ndx_uint32    = OpBitwiseAnd  %uint32  %ndx_uint32 %c_uint32_1\n"
                   "%masked_ndy_uint32    = OpBitwiseAnd  %uint32  %ndy_uint32 %c_uint32_1\n"
                   "%masked_ndx_float32   = OpConvertUToF %float32 %masked_ndx_uint32\n"
                   "%masked_ndy_float32   = OpConvertUToF %float32 %masked_ndy_uint32\n"
                   "%masked_ndx_2_float32 = OpFMul        %float32 %masked_ndx_float32 %masked_ndx_float32\n"
                   "%masked_ndy_2_float32 = OpFMul        %float32 %masked_ndy_float32 %masked_ndy_float32\n"
                   "%test_x               = OpFMul        %float32 %c_float32_10 %masked_ndx_2_float32\n"
                   "%test_y               = OpFMul        %float32 %c_float32_20 %masked_ndy_2_float32\n"
                   "%scalar_value         = OpFAdd        %float32 %test_x       %test_y\n";
    }
    else
    {
        linStr += "%masked_ndx_uint32  = OpBitwiseAnd         %uint32       %ndx_uint32 %c_uint32_3\n"
                  "%masked_ndx_float32 = OpConvertUToF        %float32      %masked_ndx_uint32\n"
                  "%scalar_value       = OpFMul               %float32      %c_float32_10 %masked_ndx_float32\n";
        quadStr += "%masked_ndx_uint32  = OpBitwiseAnd         %uint32       %ndx_uint32 %c_uint32_1\n"
                   "%masked_ndy_uint32  = OpBitwiseAnd         %uint32       %ndy_uint32 %c_uint32_1\n"
                   "%masked_ndx_float32 = OpConvertUToF        %float32      %masked_ndx_uint32\n"
                   "%masked_ndy_float32 = OpConvertUToF        %float32      %masked_ndy_uint32\n"
                   "%test_x             = OpFMul               %float32      %c_float32_10 %masked_ndx_float32\n"
                   "%test_y             = OpFMul               %float32      %c_float32_20 %masked_ndy_float32\n"
                   "%scalar_value       = OpFAdd               %float32      %test_x       %test_y\n";
    }

    switch (type)
    {
    case DataType::FLOAT32:
    {
        std::string ndx = var == DerivativeVariant::FINE ? "%masked_ndx_2_float32" : "%masked_ndx_float32";

        linStr += "%test_value  = OpFMul        %float32 %c_float32_10 " + ndx + "\n";
        quadStr += "%test_value = OpFAdd        %float32 %test_x       %test_y\n";

        break;
    }
    case DataType::VEC2_FLOAT32:
    {
        linStr += "%test_value  = OpCompositeConstruct %vec2_float32 %scalar_value %scalar_value\n";
        quadStr += "%test_value = OpCompositeConstruct %vec2_float32 %scalar_value %scalar_value\n";

        break;
    }
    case DataType::VEC3_FLOAT32:
    {
        linStr += "%test_value  = OpCompositeConstruct %vec3_float32 %scalar_value %scalar_value %scalar_value\n";
        quadStr += "%test_value = OpCompositeConstruct %vec3_float32 %scalar_value %scalar_value %scalar_value\n";

        break;
    }
    case DataType::VEC4_FLOAT32:
    {
        linStr += "%test_value  = OpCompositeConstruct %vec4_float32 %scalar_value %scalar_value %scalar_value "
                  "%scalar_value\n";
        quadStr += "%test_value = OpCompositeConstruct %vec4_float32 %scalar_value %scalar_value %scalar_value "
                   "%scalar_value\n";

        break;
    }
    default:
        DE_ASSERT(false);
    }

    return feature == DerivativeFeature::LINEAR ? linStr : quadStr;
}

const char *getLinearNdxMul(TestType type)
{
    return type == TestType::VERIFY_NDX ? "%multi_ndy_uint32 = OpIMul %uint32 %ndy_uint32 %c_uint32_32\n" :
                                          "%multi_ndy_uint32 = OpIMul %uint32 %ndy_uint32 %c_uint32_4\n";
}

const char *getStoreNdx(tcu::UVec3 numWorkgroup)
{
    // If workgroup uses second dimension test needs to use recalculated array index
    return numWorkgroup.y() > 1 ? "linear_ndx" : "ndx_uint32";
}

const char *getQuadOpCode(QuadOp op)
{
    DE_ENUM_IN_RANGE(op, QuadOp);

    static const char *const translateTable[DE_ENUM_COUNT(QuadOp)] = {
        "OpGroupNonUniformQuadBroadcast", // BROADCAST
        "OpGroupNonUniformQuadSwap",      // SWAP
    };

    return translateTable[DE_ENUM_INDEX(op)];
}

const char *getQuadNdx(uint32_t ndx)
{
    DE_ASSERT(ndx < 4);

    static const char *const translateTable[4] = {
        "c_uint32_0", // 0
        "c_uint32_1", // 1
        "c_uint32_2", // 2
        "c_uint32_3", // 3
    };

    return translateTable[ndx];
}

const char *getSwapTestName(uint32_t ndx)
{
    DE_ASSERT(ndx < 3);

    static const char *const translateTable[3] = {
        "horizontal", // 0
        "vertical",   // 1
        "diagonal",   // 2
    };

    return translateTable[ndx];
}

const char *getMipTestName(uint32_t ndx)
{
    DE_ASSERT(ndx < 2);

    static const char *const translateTable[2] = {
        "mip_0", // 0
        "mip_1", // 1
    };

    return translateTable[ndx];
}

std::string genTexCoords(DerivativeFeature feature, uint32_t mipLvl)
{
    std::string multiplier = mipLvl == 0 ? "%c_float32_0_08" : "%c_float32_0_10";
    std::string retString("%masked_ndx_uint32  = OpBitwiseAnd  %uint32  %ndx_uint32 %c_uint32_1\n"
                          "%masked_ndx_float32 = OpConvertUToF %float32 %masked_ndx_uint32\n"
                          "%masked_ndy_uint32  = OpBitwiseAnd  %uint32  %ndy_uint32 %c_uint32_1\n"
                          "%masked_ndy_float32 = OpConvertUToF %float32 %masked_ndy_uint32\n");

    if (feature == DerivativeFeature::LINEAR)
    {
        retString += "%test_value = OpFMul %float32 " + multiplier + " %masked_ndx_float32\n";
    }
    else // feature == DerivativeFeature::QUADS
    {
        retString += "%scalar_x = OpFMul %float32 " + multiplier + " %masked_ndx_float32\n";
        retString += "%scalar_y = OpFMul %float32 " + multiplier + " %masked_ndy_float32\n";
        if (mipLvl == 0)
        {
            retString += "%scalar_y_multi = OpFMul %float32 %scalar_y %c_float32_2\n";
        }
        else
        {
            retString += "%scalar_y_multi = OpFMul %float32 %scalar_y %c_float32_4\n";
        }
        retString += "%scalar     = OpFAdd               %float32 %scalar_x %scalar_y_multi\n"
                     "%test_value = OpCompositeConstruct %vec2_float32      %scalar %scalar\n";
    }

    return retString;
}

const char *getImageDim(DerivativeFeature feature)
{
    DE_ENUM_IN_RANGE(feature, DerivativeFeature);

    static const char *const translateTable[DE_ENUM_COUNT(DerivativeFeature)] = {
        "1D", // LINEAR
        "2D", // QUADS
    };

    return translateTable[DE_ENUM_INDEX(feature)];
}

const char *getSampleCapability(DerivativeFeature feature)
{
    DE_ENUM_IN_RANGE(feature, DerivativeFeature);

    static const char *const translateTable[DE_ENUM_COUNT(DerivativeFeature)] = {
        "OpCapability Sampled1D", // LINEAR
        "",                       // QUADS
    };

    return translateTable[DE_ENUM_INDEX(feature)];
}

VkImageType getImageType(DerivativeFeature feature)
{
    DE_ENUM_IN_RANGE(feature, DerivativeFeature);

    static VkImageType translateTable[DE_ENUM_COUNT(DerivativeFeature)] = {
        VK_IMAGE_TYPE_1D, // LINEAR
        VK_IMAGE_TYPE_2D, // QUADS
    };

    return translateTable[DE_ENUM_INDEX(feature)];
}

VkImageViewType getImageViewType(DerivativeFeature feature)
{
    DE_ENUM_IN_RANGE(feature, DerivativeFeature);

    static VkImageViewType translateTable[DE_ENUM_COUNT(DerivativeFeature)] = {
        VK_IMAGE_VIEW_TYPE_1D, // LINEAR
        VK_IMAGE_VIEW_TYPE_2D, // QUADS
    };

    return translateTable[DE_ENUM_INDEX(feature)];
}

VkExtent3D getImageExtent(DerivativeFeature feature)
{
    DE_ENUM_IN_RANGE(feature, DerivativeFeature);

    static VkExtent3D translateTable[DE_ENUM_COUNT(DerivativeFeature)] = {
        Constants::SAMPLED_EXTENT_1D, // LINEAR
        Constants::SAMPLED_EXTENT_2D, // QUADS
    };

    return translateTable[DE_ENUM_INDEX(feature)];
}

uint32_t calculateBufferSize(TestType testType, tcu::UVec3 numWorkgroup, DataType dataType)
{
    uint32_t bufferSize = 0;
    if (testType == TestType::VERIFY_NDX)
    {
        bufferSize = numWorkgroup.x() * numWorkgroup.y() * numWorkgroup.z() * 4;
    }
    else
    {
        bufferSize = numWorkgroup.x() * numWorkgroup.y() * numWorkgroup.z() * getDataAlignedSizeInBytes(dataType);
    }

    return bufferSize;
}

VkShaderStageFlagBits getShaderStageFlagBits(ShaderType type)
{
    DE_ENUM_IN_RANGE(type, ShaderType);

    static const VkShaderStageFlagBits translateTable[DE_ENUM_COUNT(ShaderType)] = {
        VK_SHADER_STAGE_COMPUTE_BIT,  // COMPUTE
        VK_SHADER_STAGE_MESH_BIT_EXT, // MESH
        VK_SHADER_STAGE_TASK_BIT_EXT, // TASK
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

VkPipelineStageFlagBits getPipelineStageFlagBits(ShaderType type)
{
    DE_ENUM_IN_RANGE(type, ShaderType);

    static const VkPipelineStageFlagBits translateTable[DE_ENUM_COUNT(ShaderType)] = {
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // COMPUTE
        VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT, // MESH
        VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT, // TASK
    };

    return translateTable[DE_ENUM_INDEX(type)];
}

std::vector<float> getHorizontallySwappedValues(uint32_t elemCnt, DataType type, DerivativeFeature feature)
{
    std::vector<float> swapped(elemCnt);

    if (feature == DerivativeFeature::LINEAR)
    {
        switch (type)
        {
        case DataType::FLOAT32:
        {
            swapped[0] = 10.0f;
            swapped[1] = 0.0f;
            swapped[2] = 30.0f;
            swapped[3] = 20.0f;

            for (uint32_t ndx = 4; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 4];
            }

            break;
        }
        case DataType::VEC2_FLOAT32:
        {
            swapped[0] = 10.0f;
            swapped[1] = 10.0f;
            swapped[2] = 0.0f;
            swapped[3] = 0.0f;
            swapped[4] = 30.0f;
            swapped[5] = 30.0f;
            swapped[6] = 20.0f;
            swapped[7] = 20.0f;

            for (uint32_t ndx = 8; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 8];
            }

            break;
        }
        case DataType::VEC3_FLOAT32:
        {
            swapped[0]  = 10.0f;
            swapped[1]  = 10.0f;
            swapped[2]  = 10.0f;
            swapped[3]  = 10.0f;
            swapped[4]  = 0.0f;
            swapped[5]  = 0.0f;
            swapped[6]  = 0.0f;
            swapped[7]  = 0.0f;
            swapped[8]  = 30.0f;
            swapped[9]  = 30.0f;
            swapped[10] = 30.0f;
            swapped[11] = 30.0f;
            swapped[12] = 20.0f;
            swapped[13] = 20.0f;
            swapped[14] = 20.0f;
            swapped[15] = 20.0f;

            for (uint32_t ndx = 16; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 16];
            }

            for (uint32_t ndx = 0; ndx < elemCnt; ++ndx)
            {
                if ((ndx + 1) % 4 == 0)
                {
                    swapped[ndx] = 0.0f;
                }
            }

            break;
        }
        case DataType::VEC4_FLOAT32:
        {
            swapped[0]  = 10.0f;
            swapped[1]  = 10.0f;
            swapped[2]  = 10.0f;
            swapped[3]  = 10.0f;
            swapped[4]  = 0.0f;
            swapped[5]  = 0.0f;
            swapped[6]  = 0.0f;
            swapped[7]  = 0.0f;
            swapped[8]  = 30.0f;
            swapped[9]  = 30.0f;
            swapped[10] = 30.0f;
            swapped[11] = 30.0f;
            swapped[12] = 20.0f;
            swapped[13] = 20.0f;
            swapped[14] = 20.0f;
            swapped[15] = 20.0f;

            for (uint32_t ndx = 16; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 16];
            }

            break;
        }
        default:
            break;
        }
    }
    else // feature == DerivativeFeature::QUADS
    {
        switch (type)
        {
        case DataType::FLOAT32:
        {
            swapped[0] = 10.0f;
            swapped[1] = 0.0f;
            swapped[2] = 10.0f;
            swapped[3] = 0.0f;
            swapped[4] = 30.0f;
            swapped[5] = 20.0f;
            swapped[6] = 30.0f;
            swapped[7] = 20.0f;

            for (uint32_t ndx = 8; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 8];
            }

            break;
        }
        case DataType::VEC2_FLOAT32:
        {
            swapped[0]  = 10.0f;
            swapped[1]  = 10.0f;
            swapped[2]  = 0.0f;
            swapped[3]  = 0.0f;
            swapped[4]  = 10.0f;
            swapped[5]  = 10.0f;
            swapped[6]  = 0.0f;
            swapped[7]  = 0.0f;
            swapped[8]  = 30.0f;
            swapped[9]  = 30.0f;
            swapped[10] = 20.0f;
            swapped[11] = 20.0f;
            swapped[12] = 30.0f;
            swapped[13] = 30.0f;
            swapped[14] = 20.0f;
            swapped[15] = 20.0f;

            for (uint32_t ndx = 16; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 16];
            }

            break;
        }
        case DataType::VEC3_FLOAT32:
        {
            swapped[0]  = 10.0f;
            swapped[1]  = 10.0f;
            swapped[2]  = 10.0f;
            swapped[3]  = 10.0f;
            swapped[4]  = 0.0f;
            swapped[5]  = 0.0f;
            swapped[6]  = 0.0f;
            swapped[7]  = 0.0f;
            swapped[8]  = 10.0f;
            swapped[9]  = 10.0f;
            swapped[10] = 10.0f;
            swapped[11] = 10.0f;
            swapped[12] = 0.0f;
            swapped[13] = 0.0f;
            swapped[14] = 0.0f;
            swapped[15] = 0.0f;
            swapped[16] = 30.0f;
            swapped[17] = 30.0f;
            swapped[18] = 30.0f;
            swapped[19] = 30.0f;
            swapped[20] = 20.0f;
            swapped[21] = 20.0f;
            swapped[22] = 20.0f;
            swapped[23] = 20.0f;
            swapped[24] = 30.0f;
            swapped[25] = 30.0f;
            swapped[26] = 30.0f;
            swapped[27] = 30.0f;
            swapped[28] = 20.0f;
            swapped[29] = 20.0f;
            swapped[30] = 20.0f;
            swapped[31] = 20.0f;

            for (uint32_t ndx = 32; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 32];
            }

            for (uint32_t ndx = 0; ndx < elemCnt; ++ndx)
            {
                if ((ndx + 1) % 4 == 0)
                {
                    swapped[ndx] = 0.0f;
                }
            }

            break;
        }
        case DataType::VEC4_FLOAT32:
        {
            swapped[0]  = 10.0f;
            swapped[1]  = 10.0f;
            swapped[2]  = 10.0f;
            swapped[3]  = 10.0f;
            swapped[4]  = 0.0f;
            swapped[5]  = 0.0f;
            swapped[6]  = 0.0f;
            swapped[7]  = 0.0f;
            swapped[8]  = 10.0f;
            swapped[9]  = 10.0f;
            swapped[10] = 10.0f;
            swapped[11] = 10.0f;
            swapped[12] = 0.0f;
            swapped[13] = 0.0f;
            swapped[14] = 0.0f;
            swapped[15] = 0.0f;
            swapped[16] = 30.0f;
            swapped[17] = 30.0f;
            swapped[18] = 30.0f;
            swapped[19] = 30.0f;
            swapped[20] = 20.0f;
            swapped[21] = 20.0f;
            swapped[22] = 20.0f;
            swapped[23] = 20.0f;
            swapped[24] = 30.0f;
            swapped[25] = 30.0f;
            swapped[26] = 30.0f;
            swapped[27] = 30.0f;
            swapped[28] = 20.0f;
            swapped[29] = 20.0f;
            swapped[30] = 20.0f;
            swapped[31] = 20.0f;

            for (uint32_t ndx = 32; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 32];
            }

            break;
        }
        default:
            break;
        }
    }

    return swapped;
}

std::vector<float> getVerticallySwappedValues(uint32_t elemCnt, DataType type, DerivativeFeature feature)
{
    std::vector<float> swapped(elemCnt);

    if (feature == DerivativeFeature::LINEAR)
    {
        switch (type)
        {
        case DataType::FLOAT32:
        {
            swapped[0] = 20.0f;
            swapped[1] = 30.0f;
            swapped[2] = 0.0f;
            swapped[3] = 10.0f;

            for (uint32_t ndx = 4; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 4];
            }

            break;
        }
        case DataType::VEC2_FLOAT32:
        {
            swapped[0] = 20.0f;
            swapped[1] = 20.0f;
            swapped[2] = 30.0f;
            swapped[3] = 30.0f;
            swapped[4] = 0.0f;
            swapped[5] = 0.0f;
            swapped[6] = 10.0f;
            swapped[7] = 10.0f;

            for (uint32_t ndx = 8; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 8];
            }

            break;
        }
        case DataType::VEC3_FLOAT32:
        {
            swapped[0]  = 20.0f;
            swapped[1]  = 20.0f;
            swapped[2]  = 20.0f;
            swapped[3]  = 20.0f;
            swapped[4]  = 30.0f;
            swapped[5]  = 30.0f;
            swapped[6]  = 30.0f;
            swapped[7]  = 30.0f;
            swapped[8]  = 0.0f;
            swapped[9]  = 0.0f;
            swapped[10] = 0.0f;
            swapped[11] = 0.0f;
            swapped[12] = 10.0f;
            swapped[13] = 10.0f;
            swapped[14] = 10.0f;
            swapped[15] = 10.0f;

            for (uint32_t ndx = 16; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 16];
            }

            for (uint32_t ndx = 0; ndx < elemCnt; ++ndx)
            {
                if ((ndx + 1) % 4 == 0)
                {
                    swapped[ndx] = 0.0f;
                }
            }

            break;
        }
        case DataType::VEC4_FLOAT32:
        {
            swapped[0]  = 20.0f;
            swapped[1]  = 20.0f;
            swapped[2]  = 20.0f;
            swapped[3]  = 20.0f;
            swapped[4]  = 30.0f;
            swapped[5]  = 30.0f;
            swapped[6]  = 30.0f;
            swapped[7]  = 30.0f;
            swapped[8]  = 0.0f;
            swapped[9]  = 0.0f;
            swapped[10] = 0.0f;
            swapped[11] = 0.0f;
            swapped[12] = 10.0f;
            swapped[13] = 10.0f;
            swapped[14] = 10.0f;
            swapped[15] = 10.0f;

            for (uint32_t ndx = 16; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 16];
            }

            break;
        }
        default:
            break;
        }
    }
    else // feature == DerivativeFeature::QUADS
    {
        switch (type)
        {
        case DataType::FLOAT32:
        {
            swapped[0] = 20.0f;
            swapped[1] = 30.0f;
            swapped[2] = 20.0f;
            swapped[3] = 30.0f;
            swapped[4] = 0.0f;
            swapped[5] = 10.0f;
            swapped[6] = 0.0f;
            swapped[7] = 10.0f;

            for (uint32_t ndx = 8; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 8];
            }

            break;
        }
        case DataType::VEC2_FLOAT32:
        {
            swapped[0]  = 20.0f;
            swapped[1]  = 20.0f;
            swapped[2]  = 30.0f;
            swapped[3]  = 30.0f;
            swapped[4]  = 20.0f;
            swapped[5]  = 20.0f;
            swapped[6]  = 30.0f;
            swapped[7]  = 30.0f;
            swapped[8]  = 0.0f;
            swapped[9]  = 0.0f;
            swapped[10] = 10.0f;
            swapped[11] = 10.0f;
            swapped[12] = 0.0f;
            swapped[13] = 0.0f;
            swapped[14] = 10.0f;
            swapped[15] = 10.0f;

            for (uint32_t ndx = 16; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 16];
            }

            break;
        }
        case DataType::VEC3_FLOAT32:
        {
            swapped[0]  = 20.0f;
            swapped[1]  = 20.0f;
            swapped[2]  = 20.0f;
            swapped[3]  = 20.0f;
            swapped[4]  = 30.0f;
            swapped[5]  = 30.0f;
            swapped[6]  = 30.0f;
            swapped[7]  = 30.0f;
            swapped[8]  = 20.0f;
            swapped[9]  = 20.0f;
            swapped[10] = 20.0f;
            swapped[11] = 20.0f;
            swapped[12] = 30.0f;
            swapped[13] = 30.0f;
            swapped[14] = 30.0f;
            swapped[15] = 30.0f;
            swapped[16] = 0.0f;
            swapped[17] = 0.0f;
            swapped[18] = 0.0f;
            swapped[19] = 0.0f;
            swapped[20] = 10.0f;
            swapped[21] = 10.0f;
            swapped[22] = 10.0f;
            swapped[23] = 10.0f;
            swapped[24] = 0.0f;
            swapped[25] = 0.0f;
            swapped[26] = 0.0f;
            swapped[27] = 0.0f;
            swapped[28] = 10.0f;
            swapped[29] = 10.0f;
            swapped[30] = 10.0f;
            swapped[31] = 10.0f;

            for (uint32_t ndx = 32; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 32];
            }

            for (uint32_t ndx = 0; ndx < elemCnt; ++ndx)
            {
                if ((ndx + 1) % 4 == 0)
                {
                    swapped[ndx] = 0.0f;
                }
            }

            break;
        }
        case DataType::VEC4_FLOAT32:
        {
            swapped[0]  = 20.0f;
            swapped[1]  = 20.0f;
            swapped[2]  = 20.0f;
            swapped[3]  = 20.0f;
            swapped[4]  = 30.0f;
            swapped[5]  = 30.0f;
            swapped[6]  = 30.0f;
            swapped[7]  = 30.0f;
            swapped[8]  = 20.0f;
            swapped[9]  = 20.0f;
            swapped[10] = 20.0f;
            swapped[11] = 20.0f;
            swapped[12] = 30.0f;
            swapped[13] = 30.0f;
            swapped[14] = 30.0f;
            swapped[15] = 30.0f;
            swapped[16] = 0.0f;
            swapped[17] = 0.0f;
            swapped[18] = 0.0f;
            swapped[19] = 0.0f;
            swapped[20] = 10.0f;
            swapped[21] = 10.0f;
            swapped[22] = 10.0f;
            swapped[23] = 10.0f;
            swapped[24] = 0.0f;
            swapped[25] = 0.0f;
            swapped[26] = 0.0f;
            swapped[27] = 0.0f;
            swapped[28] = 10.0f;
            swapped[29] = 10.0f;
            swapped[30] = 10.0f;
            swapped[31] = 10.0f;

            for (uint32_t ndx = 32; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 32];
            }

            break;
        }
        default:
            break;
        }
    }

    return swapped;
}

std::vector<float> getDiagonallySwappedValues(uint32_t elemCnt, DataType type, DerivativeFeature feature)
{
    std::vector<float> swapped(elemCnt);

    if (feature == DerivativeFeature::LINEAR)
    {
        switch (type)
        {
        case DataType::FLOAT32:
        {
            swapped[0] = 30.0f;
            swapped[1] = 20.0f;
            swapped[2] = 10.0f;
            swapped[3] = 0.0f;

            for (uint32_t ndx = 4; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 4];
            }

            break;
        }
        case DataType::VEC2_FLOAT32:
        {
            swapped[0] = 30.0f;
            swapped[1] = 30.0f;
            swapped[2] = 20.0f;
            swapped[3] = 20.0f;
            swapped[4] = 10.0f;
            swapped[5] = 10.0f;
            swapped[6] = 0.0f;
            swapped[7] = 0.0f;

            for (uint32_t ndx = 8; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 8];
            }

            break;
        }
        case DataType::VEC3_FLOAT32:
        {
            swapped[0]  = 30.0f;
            swapped[1]  = 30.0f;
            swapped[2]  = 30.0f;
            swapped[3]  = 30.0f;
            swapped[4]  = 20.0f;
            swapped[5]  = 20.0f;
            swapped[6]  = 20.0f;
            swapped[7]  = 20.0f;
            swapped[8]  = 10.0f;
            swapped[9]  = 10.0f;
            swapped[10] = 10.0f;
            swapped[11] = 10.0f;
            swapped[12] = 0.0f;
            swapped[13] = 0.0f;
            swapped[14] = 0.0f;
            swapped[15] = 0.0f;

            for (uint32_t ndx = 16; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 16];
            }

            for (uint32_t ndx = 0; ndx < elemCnt; ++ndx)
            {
                if ((ndx + 1) % 4 == 0)
                {
                    swapped[ndx] = 0.0f;
                }
            }

            break;
        }
        case DataType::VEC4_FLOAT32:
        {
            swapped[0]  = 30.0f;
            swapped[1]  = 30.0f;
            swapped[2]  = 30.0f;
            swapped[3]  = 30.0f;
            swapped[4]  = 20.0f;
            swapped[5]  = 20.0f;
            swapped[6]  = 20.0f;
            swapped[7]  = 20.0f;
            swapped[8]  = 10.0f;
            swapped[9]  = 10.0f;
            swapped[10] = 10.0f;
            swapped[11] = 10.0f;
            swapped[12] = 0.0f;
            swapped[13] = 0.0f;
            swapped[14] = 0.0f;
            swapped[15] = 0.0f;

            for (uint32_t ndx = 16; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 16];
            }

            break;
        }
        default:
            break;
        }
    }
    else // feature == DerivativeFeature::QUADS
    {
        switch (type)
        {
        case DataType::FLOAT32:
        {
            swapped[0] = 30.0f;
            swapped[1] = 20.0f;
            swapped[2] = 30.0f;
            swapped[3] = 20.0f;
            swapped[4] = 10.0f;
            swapped[5] = 0.0f;
            swapped[6] = 10.0f;
            swapped[7] = 0.0f;

            for (uint32_t ndx = 8; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 8];
            }

            break;
        }
        case DataType::VEC2_FLOAT32:
        {
            swapped[0]  = 30.0f;
            swapped[1]  = 30.0f;
            swapped[2]  = 20.0f;
            swapped[3]  = 20.0f;
            swapped[4]  = 30.0f;
            swapped[5]  = 30.0f;
            swapped[6]  = 20.0f;
            swapped[7]  = 20.0f;
            swapped[8]  = 10.0f;
            swapped[9]  = 10.0f;
            swapped[10] = 0.0f;
            swapped[11] = 0.0f;
            swapped[12] = 10.0f;
            swapped[13] = 10.0f;
            swapped[14] = 0.0f;
            swapped[15] = 0.0f;

            for (uint32_t ndx = 16; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 16];
            }

            break;
        }
        case DataType::VEC3_FLOAT32:
        {
            swapped[0]  = 30.0f;
            swapped[1]  = 30.0f;
            swapped[2]  = 30.0f;
            swapped[3]  = 30.0f;
            swapped[4]  = 20.0f;
            swapped[5]  = 20.0f;
            swapped[6]  = 20.0f;
            swapped[7]  = 20.0f;
            swapped[8]  = 30.0f;
            swapped[9]  = 30.0f;
            swapped[10] = 30.0f;
            swapped[11] = 30.0f;
            swapped[12] = 20.0f;
            swapped[13] = 20.0f;
            swapped[14] = 20.0f;
            swapped[15] = 20.0f;
            swapped[16] = 10.0f;
            swapped[17] = 10.0f;
            swapped[18] = 10.0f;
            swapped[19] = 10.0f;
            swapped[20] = 0.0f;
            swapped[21] = 0.0f;
            swapped[22] = 0.0f;
            swapped[23] = 0.0f;
            swapped[24] = 10.0f;
            swapped[25] = 10.0f;
            swapped[26] = 10.0f;
            swapped[27] = 10.0f;
            swapped[28] = 0.0f;
            swapped[29] = 0.0f;
            swapped[30] = 0.0f;
            swapped[31] = 0.0f;

            for (uint32_t ndx = 32; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 32];
            }

            for (uint32_t ndx = 0; ndx < elemCnt; ++ndx)
            {
                if ((ndx + 1) % 4 == 0)
                {
                    swapped[ndx] = 0.0f;
                }
            }

            break;
        }
        case DataType::VEC4_FLOAT32:
        {
            swapped[0]  = 30.0f;
            swapped[1]  = 30.0f;
            swapped[2]  = 30.0f;
            swapped[3]  = 30.0f;
            swapped[4]  = 20.0f;
            swapped[5]  = 20.0f;
            swapped[6]  = 20.0f;
            swapped[7]  = 20.0f;
            swapped[8]  = 30.0f;
            swapped[9]  = 30.0f;
            swapped[10] = 30.0f;
            swapped[11] = 30.0f;
            swapped[12] = 20.0f;
            swapped[13] = 20.0f;
            swapped[14] = 20.0f;
            swapped[15] = 20.0f;
            swapped[16] = 10.0f;
            swapped[17] = 10.0f;
            swapped[18] = 10.0f;
            swapped[19] = 10.0f;
            swapped[20] = 0.0f;
            swapped[21] = 0.0f;
            swapped[22] = 0.0f;
            swapped[23] = 0.0f;
            swapped[24] = 10.0f;
            swapped[25] = 10.0f;
            swapped[26] = 10.0f;
            swapped[27] = 10.0f;
            swapped[28] = 0.0f;
            swapped[29] = 0.0f;
            swapped[30] = 0.0f;
            swapped[31] = 0.0f;

            for (uint32_t ndx = 32; ndx < elemCnt; ++ndx)
            {
                swapped[ndx] = swapped[ndx - 32];
            }

            break;
        }
        default:
            break;
        }
    }

    return swapped;
}

class ComputeShaderDerivativeInstance : public TestInstance
{
public:
    ComputeShaderDerivativeInstance(Context &ctx, const ComputeShaderDerivativeTestParams &params);
    tcu::TestStatus iterate(void);

private:
    Move<VkBuffer> createBufferAndBindMemory(AllocationMp *outMemory);
    Move<VkImage> createImageAndBindMemory(VkFormat format, VkImageType imgType, VkExtent3D extent, uint32_t mipLevels,
                                           VkImageUsageFlags usage, AllocationMp *outMemory);
    Move<VkImageView> createImageView(VkFormat format, VkImageViewType viewType, VkImageSubresourceRange range,
                                      VkImage image);
    Move<VkDescriptorSetLayout> createDescriptorSetLayout();
    Move<VkPipelineLayout> createPipelineLayout(VkDescriptorSetLayout descriptorSetLayout);
    Move<VkDescriptorPool> createDescriptorPool();
    Move<VkDescriptorSet> createDescriptorSet(VkDescriptorPool descriptorPool,
                                              VkDescriptorSetLayout descriptorSetLayout,
                                              const std::vector<VkDescriptorBufferInfo> &bufferInfos,
                                              const VkDescriptorImageInfo &imgInfo);
    Move<VkSampler> createBasicSampler();
    Move<VkPipeline> createComputePipeline(VkPipelineLayout layout, VkShaderModule module, bool forceFullSubgroup);
    Move<VkPipeline> createGraphicsPipeline(VkPipelineLayout layout, VkRenderPass renderPass, VkViewport viewport,
                                            VkRect2D scissor, VkShaderModule fragmentModule, VkShaderModule taskModule,
                                            VkShaderModule meshModule, bool forceFullSubgroup);

    bool checkResult(std::vector<AllocationMp> &allocations);

private:
    const ComputeShaderDerivativeTestParams m_params;
};

class ComputeShaderDerivativeCase : public TestCase
{
public:
    ComputeShaderDerivativeCase(tcu::TestContext &testCtx, const char *name,
                                const ComputeShaderDerivativeTestParams &params);
    void checkSupport(Context &context) const;
    void initPrograms(vk::SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &ctx) const;

private:
    const ComputeShaderDerivativeTestParams m_params;
};

ComputeShaderDerivativeInstance::ComputeShaderDerivativeInstance(Context &ctx,
                                                                 const ComputeShaderDerivativeTestParams &params)
    : TestInstance(ctx)
    , m_params(params)
{
}

Move<VkBuffer> ComputeShaderDerivativeInstance::createBufferAndBindMemory(AllocationMp *outMemory)
{
    const VkDevice &device        = m_context.getDevice();
    const DeviceInterface &vkdi   = m_context.getDeviceInterface();
    VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    Allocator &allocator          = m_context.getDefaultAllocator();
    const uint32_t bufferSize     = calculateBufferSize(m_params.testType, m_params.numWorkgroup, m_params.dataType);

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

Move<VkImage> ComputeShaderDerivativeInstance::createImageAndBindMemory(VkFormat format, VkImageType imgType,
                                                                        VkExtent3D extent, uint32_t mipLevels,
                                                                        VkImageUsageFlags usage,
                                                                        AllocationMp *outMemory)
{
    const VkDevice &device      = m_context.getDevice();
    const DeviceInterface &vkdi = m_context.getDeviceInterface();
    Allocator &allocator        = m_context.getDefaultAllocator();

    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType          sType
        nullptr,                             // const void*              pNext
        0u,                                  // VkImageCreateFlags       flags
        imgType,                             // VkImageType              imageType
        format,                              // VkFormat                 format
        extent,                              // VkExtent3D               extent
        mipLevels,                           // uint32_t                 mipLevels
        1u,                                  // uint32_t                 arrayLayers
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits    samples
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling            tiling
        usage,                               // VkImageUsageFlags        usage
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

Move<VkImageView> ComputeShaderDerivativeInstance::createImageView(VkFormat format, VkImageViewType viewType,
                                                                   VkImageSubresourceRange range, VkImage image)
{
    const VkDevice &device      = m_context.getDevice();
    const DeviceInterface &vkdi = m_context.getDeviceInterface();

    return makeImageView(vkdi, device, image, viewType, format, range);
}

Move<VkDescriptorSetLayout> ComputeShaderDerivativeInstance::createDescriptorSetLayout()
{
    const VkDevice &device      = m_context.getDevice();
    const DeviceInterface &vkdi = m_context.getDeviceInterface();

    DescriptorSetLayoutBuilder builder;

    for (uint32_t ndx = 0; ndx < 4; ++ndx)
        builder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, getShaderStageFlagBits(m_params.shaderType));
    builder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, getShaderStageFlagBits(m_params.shaderType));

    return builder.build(vkdi, device);
}

Move<VkPipelineLayout> ComputeShaderDerivativeInstance::createPipelineLayout(VkDescriptorSetLayout descriptorSetLayout)
{
    const VkDevice &device      = m_context.getDevice();
    const DeviceInterface &vkdi = m_context.getDeviceInterface();

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

Move<VkDescriptorPool> ComputeShaderDerivativeInstance::createDescriptorPool()
{
    const VkDevice &device      = m_context.getDevice();
    const DeviceInterface &vkdi = m_context.getDeviceInterface();

    DescriptorPoolBuilder builder;

    for (uint32_t ndx = 0; ndx < 4; ++ndx)
        builder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);

    builder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1); // Sampled image for LOD calculating

    return builder.build(vkdi, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);
}

Move<VkDescriptorSet> ComputeShaderDerivativeInstance::createDescriptorSet(
    VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout,
    const std::vector<VkDescriptorBufferInfo> &bufferInfos, const VkDescriptorImageInfo &imgInfo)
{
    const VkDevice &device      = m_context.getDevice();
    const DeviceInterface &vkdi = m_context.getDeviceInterface();

    const VkDescriptorSetAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType                 sType
        nullptr,                                        // const void*                     pNext
        descriptorPool,                                 // VkDescriptorPool                descriptorPool
        1,                                              // uint32_t                        descriptorSetCount
        &descriptorSetLayout,                           // const VkDescriptorSetLayout*    pSetLayouts
    };

    Move<VkDescriptorSet> descriptorSet = vk::allocateDescriptorSet(vkdi, device, &allocInfo);
    DescriptorSetUpdateBuilder builder;

    for (uint32_t ndx = 0; ndx < 4; ++ndx)
    {
        builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(ndx),
                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfos[ndx]);
    }
    builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(4),
                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imgInfo);
    builder.update(vkdi, device);

    return descriptorSet;
}

Move<VkSampler> ComputeShaderDerivativeInstance::createBasicSampler()
{
    const VkDevice &device      = m_context.getDevice();
    const DeviceInterface &vkdi = m_context.getDeviceInterface();

    const VkSamplerCreateInfo samplerInfo = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,   // VkStructureType      sType
        nullptr,                                 // const void *         pNext
        0u,                                      // VkSamplerCreateFlags flags
        VK_FILTER_NEAREST,                       // VkFilter             magFilter
        VK_FILTER_NEAREST,                       // VkFilter             minFilter
        VK_SAMPLER_MIPMAP_MODE_NEAREST,          // VkSamplerMipmapMode  mipmapMode
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeU
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeV
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,   // VkSamplerAddressMode addressModeW
        0.0f,                                    // float                mipLodBias
        VK_FALSE,                                // VkBool32             anisotropyEnable
        1.0f,                                    // float                maxAnisotropy
        VK_FALSE,                                // VkBool32             compareEnable
        VK_COMPARE_OP_ALWAYS,                    // VkCompareOp          compareOp
        0.0f,                                    // float                minLod
        1.0f,                                    // float                maxLod
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, // VkBorderColor        borderColor
        VK_FALSE,                                // VkBool32             unnormalizedCoordinates
    };

    Move<VkSampler> sampler = createSampler(vkdi, device, &samplerInfo);

    return sampler;
}

Move<VkPipeline> ComputeShaderDerivativeInstance::createComputePipeline(VkPipelineLayout layout, VkShaderModule module,
                                                                        bool forceFullSubgroup)
{
    const VkDevice &device      = m_context.getDevice();
    const DeviceInterface &vkdi = m_context.getDeviceInterface();

    const VkPipelineShaderStageCreateFlags subgroupFlags =
        forceFullSubgroup ? VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT : 0;

    const VkPipelineShaderStageCreateInfo pipelineShaderStageParams = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                             // const void* pNext;
        subgroupFlags,                                       // VkPipelineShaderStageCreateFlags flags;
        VK_SHADER_STAGE_COMPUTE_BIT,                         // VkShaderStageFlagBits stage;
        module,                                              // VkShaderModule module;
        "main",                                              // const char* pName;
        nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
    };

    const VkComputePipelineCreateInfo pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        (VkPipelineCreateFlags)0,                       // VkPipelineCreateFlags flags;
        pipelineShaderStageParams,                      // VkPipelineShaderStageCreateInfo stage;
        layout,                                         // VkPipelineLayout layout;
        VK_NULL_HANDLE,                                 // VkPipeline basePipelineHandle;
        0,                                              // int32_t basePipelineIndex;
    };

    return vk::createComputePipeline(vkdi, device, VK_NULL_HANDLE, &pipelineCreateInfo);
}

Move<VkPipeline> ComputeShaderDerivativeInstance::createGraphicsPipeline(
    VkPipelineLayout layout, VkRenderPass renderPass, VkViewport viewport, VkRect2D scissor,
    VkShaderModule fragmentModule, VkShaderModule taskModule, VkShaderModule meshModule, bool forceFullSubgroup)
{
    const VkDevice &device      = m_context.getDevice();
    const DeviceInterface &vkdi = m_context.getDeviceInterface();

    const VkPipelineShaderStageCreateFlags subgroupFlags =
        forceFullSubgroup ? VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT : 0;

    VkPipelineShaderStageCreateInfo stageInfos[3];
    uint32_t stageNdx = 0;

    if (fragmentModule != VK_NULL_HANDLE)
    {
        const VkPipelineShaderStageCreateInfo pipelineShaderStageParams = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            0,                                                   // VkPipelineShaderStageCreateFlags flags;
            VK_SHADER_STAGE_FRAGMENT_BIT,                        // VkShaderStageFlagBits stage;
            fragmentModule,                                      // VkShaderModule module;
            "main",                                              // const char* pName;
            nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
        };

        stageInfos[stageNdx++] = pipelineShaderStageParams;
    }

    if (taskModule != VK_NULL_HANDLE)
    {
        const VkPipelineShaderStageCreateInfo pipelineShaderStageParams = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            subgroupFlags,                                       // VkPipelineShaderStageCreateFlags flags;
            VK_SHADER_STAGE_TASK_BIT_EXT,                        // VkShaderStageFlagBits stage;
            taskModule,                                          // VkShaderModule module;
            "main",                                              // const char* pName;
            nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
        };

        stageInfos[stageNdx++] = pipelineShaderStageParams;
    }

    if (meshModule != VK_NULL_HANDLE)
    {
        const VkPipelineShaderStageCreateInfo pipelineShaderStageParams = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            subgroupFlags,                                       // VkPipelineShaderStageCreateFlags flags;
            VK_SHADER_STAGE_MESH_BIT_EXT,                        // VkShaderStageFlagBits stage;
            meshModule,                                          // VkShaderModule module;
            "main",                                              // const char* pName;
            nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
        };

        stageInfos[stageNdx++] = pipelineShaderStageParams;
    }

    const VkVertexInputBindingDescription vertexInputBindingDescription = {
        0u,                          // uint32_t             binding
        sizeof(tcu::Vec4),           // uint32_t             stride
        VK_VERTEX_INPUT_RATE_VERTEX, // VkVertexInputRate    inputRate
    };

    const VkVertexInputAttributeDescription vertexInputAttributeDescription = {
        0u,                            // uint32_t    location
        0u,                            // uint32_t    binding
        VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat    format
        0u                             // uint32_t    offset
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                             sType
        nullptr,                                                   // const void*                                 pNext
        (VkPipelineVertexInputStateCreateFlags)0,                  // VkPipelineVertexInputStateCreateFlags       flags
        1u,                              // uint32_t                                    vertexBindingDescriptionCount
        &vertexInputBindingDescription,  // const VkVertexInputBindingDescription*      pVertexBindingDescriptions
        1u,                              // uint32_t                                    vertexAttributeDescriptionCount
        &vertexInputAttributeDescription // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                                                     // const void*                                pNext
        0u,                                                          // VkPipelineInputAssemblyStateCreateFlags    flags
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // VkPrimitiveTopology                        topology
        VK_FALSE                             // VkBool32                                   primitiveRestartEnable
    };

    const VkPipelineViewportStateCreateInfo viewportStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType                             sType
        nullptr,                                               // const void*                                 pNext
        (VkPipelineViewportStateCreateFlags)0,                 // VkPipelineViewportStateCreateFlags          flags
        1u,        // uint32_t                                    viewportCount
        &viewport, // const VkViewport*                           pViewports
        1u,        // uint32_t                                    scissorCount
        &scissor   // const VkRect2D*                             pScissors
    };

    const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType                            sType
        nullptr,                                                    // const void*                                pNext
        0u,                                                         // VkPipelineRasterizationStateCreateFlags    flags
        VK_FALSE,                        // VkBool32                                   depthClampEnable
        VK_FALSE,                        // VkBool32                                   rasterizerDiscardEnable
        VK_POLYGON_MODE_FILL,            // VkPolygonMode                              polygonMode
        VK_CULL_MODE_NONE,               // VkCullModeFlags                            cullMode
        VK_FRONT_FACE_COUNTER_CLOCKWISE, // VkFrontFace                                frontFace
        VK_FALSE,                        // VkBool32                                   depthBiasEnable
        0.0f,                            // float                                      depthBiasConstantFactor
        0.0f,                            // float                                      depthBiasClamp
        0.0f,                            // float                                      depthBiasSlopeFactor
        1.0f                             // float                                      lineWidth
    };

    const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType                          sType
        nullptr,                                                  // const void*                              pNext
        0u,                                                       // VkPipelineMultisampleStateCreateFlags    flags
        VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits                    rasterizationSamples
        VK_FALSE,              // VkBool32                                 sampleShadingEnable
        1.0f,                  // float                                    minSampleShading
        nullptr,               // const VkSampleMask*                      pSampleMask
        VK_FALSE,              // VkBool32                                 alphaToCoverageEnable
        VK_FALSE               // VkBool32                                 alphaToOneEnable
    };

    const VkStencilOpState stencilOpState = {
        VK_STENCIL_OP_KEEP,  // VkStencilOp    failOp
        VK_STENCIL_OP_KEEP,  // VkStencilOp    passOp
        VK_STENCIL_OP_KEEP,  // VkStencilOp    depthFailOp
        VK_COMPARE_OP_NEVER, // VkCompareOp    compareOp
        0,                   // uint32_t       compareMask
        0,                   // uint32_t       writeMask
        0                    // uint32_t       reference
    };

    const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType                          sType
        nullptr,                                                    // const void*                              pNext
        0u,                                                         // VkPipelineDepthStencilStateCreateFlags   flags
        VK_FALSE,                    // VkBool32                                 depthTestEnable
        VK_FALSE,                    // VkBool32                                 depthWriteEnable
        VK_COMPARE_OP_LESS_OR_EQUAL, // VkCompareOp                              depthCompareOp
        VK_FALSE,                    // VkBool32                                 depthBoundsTestEnable
        VK_FALSE,                    // VkBool32                                 stencilTestEnable
        stencilOpState,              // VkStencilOpState                         front
        stencilOpState,              // VkStencilOpState                         back
        0.0f,                        // float                                    minDepthBounds
        1.0f,                        // float                                    maxDepthBounds
    };

    const VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
        VK_FALSE,                // VkBool32                 blendEnable
        VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            srcColorBlendFactor
        VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            dstColorBlendFactor
        VK_BLEND_OP_ADD,         // VkBlendOp                colorBlendOp
        VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            srcAlphaBlendFactor
        VK_BLEND_FACTOR_ZERO,    // VkBlendFactor            dstAlphaBlendFactor
        VK_BLEND_OP_ADD,         // VkBlendOp                alphaBlendOp
        VK_COLOR_COMPONENT_R_BIT // VkColorComponentFlags    colorWriteMask
            | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

    const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfoDefault = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType                               sType
        nullptr,                                                  // const void*                                   pNext
        0u,                                                       // VkPipelineColorBlendStateCreateFlags          flags
        VK_FALSE,                   // VkBool32                                      logicOpEnable
        VK_LOGIC_OP_CLEAR,          // VkLogicOp                                     logicOp
        1u,                         // uint32_t                                      attachmentCount
        &colorBlendAttachmentState, // const VkPipelineColorBlendAttachmentState*    pAttachments
        {0.0f, 0.0f, 0.0f, 0.0f}    // float                                         blendConstants[4]
    };

    const VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, // VkStructureType                                  sType
        nullptr,                                         // const void*                                      pNext
        0,                                               // VkPipelineCreateFlags                            flags
        stageNdx,                                        // uint32_t                                         stageCount
        stageInfos,                                      // const VkPipelineShaderStageCreateInfo*           pStages
        &vertexInputStateCreateInfoDefault,   // const VkPipelineVertexInputStateCreateInfo*      pVertexInputState
        &inputAssemblyStateCreateInfoDefault, // const VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState
        nullptr,                              // const VkPipelineTessellationStateCreateInfo*     pTessellationState
        &viewportStateCreateInfoDefault,      // const VkPipelineViewportStateCreateInfo*         pViewportState
        &rasterizationStateCreateInfoDefault, // const VkPipelineRasterizationStateCreateInfo*    pRasterizationState
        &multisampleStateCreateInfoDefault,   // const VkPipelineMultisampleStateCreateInfo*      pMultisampleState
        &depthStencilStateCreateInfoDefault,  // const VkPipelineDepthStencilStateCreateInfo*     pDepthStencilState
        &colorBlendStateCreateInfoDefault,    // const VkPipelineColorBlendStateCreateInfo*       pColorBlendState
        nullptr,                              // const VkPipelineDynamicStateCreateInfo*          pDynamicState
        layout,                               // VkPipelineLayout                                 layout
        renderPass,                           // VkRenderPass                                     renderPass
        0,                                    // uint32_t                                         subpass
        VK_NULL_HANDLE,                       // VkPipeline                                       basePipelineHandle
        0                                     // int32_t                                          basePipelineIndex;
    };

    return vk::createGraphicsPipeline(vkdi, device, VK_NULL_HANDLE, &pipelineCreateInfo);
}

bool ComputeShaderDerivativeInstance::checkResult(std::vector<AllocationMp> &allocations)
{
    bool outputMatches = true;

    switch (m_params.testType)
    {
    case TestType::DERIVATIVE_VALUE:
    {
        const uint32_t elemCnt = m_params.numWorkgroup.x() * m_params.numWorkgroup.y() * m_params.numWorkgroup.z() *
                                 getDataAlignedComponentCount(m_params.dataType);

        // Generating result
        std::vector<float> expX(elemCnt);
        std::vector<float> expY(elemCnt);
        std::vector<float> expF(elemCnt);

        if (m_params.variant != DerivativeVariant::FINE)
        {
            for (uint32_t ndx = 0; ndx < elemCnt; ++ndx)
            {
                expX[ndx] = 10.0f;
                expY[ndx] = 20.0f;
                expF[ndx] = 30.0f;

                if (((ndx + 1) % 4 == 0) && (m_params.dataType == DataType::VEC3_FLOAT32))
                {
                    expX[ndx] = 0.0f;
                    expY[ndx] = 0.0f;
                    expF[ndx] = 0.0f;
                }
            }
        }
        else
        {
            if (m_params.feature == DerivativeFeature::LINEAR)
            {
                switch (m_params.dataType)
                {
                case DataType::FLOAT32:
                {
                    for (uint32_t ndx = 0; ndx < elemCnt; ++ndx)
                    {
                        // OutputX [10.0f, 10.0f, 50.0f, 50.0f, ... ]
                        if ((((ndx + 1) % 4) == 1) || (((ndx + 1) % 4) == 2))
                        {
                            expX[ndx] = 10.0f;
                        }
                        else
                        {
                            expX[ndx] = 50.0f;
                        }

                        // OutputY [ 40.0f, 80.0f, ... ]
                        if ((((ndx + 1) % 2) == 1))
                        {
                            expY[ndx] = 40.0f;
                        }
                        else
                        {
                            expY[ndx] = 80.0f;
                        }

                        // OutputF [ 50.0f, 90.0f, 90.0f, 130.0f, ... ]
                        if ((((ndx + 1) % 4) == 1))
                        {
                            expF[ndx] = 50.0f;
                        }
                        else if ((((ndx + 1) % 4) == 0))
                        {
                            expF[ndx] = 130.0f;
                        }
                        else
                        {
                            expF[ndx] = 90.0f;
                        }
                    }

                    break;
                }
                case DataType::VEC2_FLOAT32:
                {
                    for (uint32_t ndx = 0; ndx < elemCnt; ++ndx)
                    {
                        // OutputX [10.0f, 10.0f, 10.0f, 10.0f, 50.0f, 50.0f, 50.0f, 50.0f, ... ]
                        if ((((ndx + 1) % 8) > 0) && (((ndx + 1) % 8) < 5))
                        {
                            expX[ndx] = 10.0f;
                        }
                        else
                        {
                            expX[ndx] = 50.0f;
                        }

                        // OutputY [ 40.0f, 40.0f, 80.0f, 80.0f, ... ]
                        if ((((ndx + 1) % 4) == 1) || (((ndx + 1) % 4) == 2))
                        {
                            expY[ndx] = 40.0f;
                        }
                        else
                        {
                            expY[ndx] = 80.0f;
                        }

                        // OutputF [ 50.0f, 50.0f, 90.0f, 90.0f, 90.0f, 90.0f, 130.0f, 130.0f, ... ]
                        if ((((ndx + 1) % 8) > 0) && (((ndx + 1) % 8) < 3))
                        {
                            expF[ndx] = 50.0f;
                        }
                        else if ((((ndx + 1) % 8) == 0) || (((ndx + 1) % 8) == 7))
                        {
                            expF[ndx] = 130.0f;
                        }
                        else
                        {
                            expF[ndx] = 90.0f;
                        }
                    }

                    break;
                }
                case DataType::VEC3_FLOAT32:
                {
                    for (uint32_t ndx = 0; ndx < elemCnt; ++ndx)
                    {
                        // OutputX [10.0f, 10.0f, 10.0f, 0.0f, 10.0f, 10.0f, 10.0f, 0.0f, 50.0f, 50.0f, 50.0f, 0.0f, 50.0f, 50.0f, 50.0f, 0.0f, ... ]
                        if (((ndx + 1) % 16) < 8)
                        {
                            expX[ndx] = 10.0f;
                        }
                        else
                        {
                            expX[ndx] = 50.0f;
                        }

                        // OutputY [ 40.0f, 40.0f, 40.0f, 0.0f, 80.0f, 80.0f, 80.0f, 0.0f, ... ]
                        if ((((ndx + 1) % 8) < 4))
                        {
                            expY[ndx] = 40.0f;
                        }
                        else
                        {
                            expY[ndx] = 80.0f;
                        }

                        // OutputF [ 50.0f, 50.0f, 50.0f, 0.0f, 90.0f, 90.0f, 90.0f, 0.0f, 90.0f, 90.0f, 90.0f, 0.0f, 130.0f, 130.0f, 130.0f, 0.0f, ... ]
                        if (((ndx + 1) % 16) < 4)
                        {
                            expF[ndx] = 50.0f;
                        }
                        else if (((ndx + 1) % 16) > 12)
                        {
                            expF[ndx] = 130.0f;
                        }
                        else
                        {
                            expF[ndx] = 90.0f;
                        }

                        if ((ndx + 1) % 4 == 0)
                        {
                            expX[ndx] = 0.0f;
                            expY[ndx] = 0.0f;
                            expF[ndx] = 0.0f;
                        }
                    }

                    break;
                }
                case DataType::VEC4_FLOAT32:
                {
                    for (uint32_t ndx = 0; ndx < elemCnt; ++ndx)
                    {
                        // OutputX [10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 50.0f, 50.0f, 50.0f, 50.0f, 50.0f, 50.0f, 50.0f, 0.0f, ... ]
                        if ((((ndx + 1) % 16) <= 8) && (((ndx + 1) % 16) != 0))
                        {
                            expX[ndx] = 10.0f;
                        }
                        else
                        {
                            expX[ndx] = 50.0f;
                        }

                        // OutputY [ 40.0f, 40.0f, 40.0f, 40.0f, 80.0f, 80.0f, 80.0f, 80.0f, ... ]
                        if ((((ndx + 1) % 8) <= 4) && (((ndx + 1) % 8) != 0))
                        {
                            expY[ndx] = 40.0f;
                        }
                        else
                        {
                            expY[ndx] = 80.0f;
                        }

                        // OutputF [ 50.0f, 50.0f, 50.0f, 50.0f, 90.0f, 90.0f, 90.0f, 90.0f, 90.0f, 90.0f, 90.0f, 90.0f, 130.0f, 130.0f, 130.0f, 130.0f, ... ]
                        if ((((ndx + 1) % 16) <= 4) && (((ndx + 1) % 16) != 0))
                        {
                            expF[ndx] = 50.0f;
                        }
                        else if ((((ndx + 1) % 16) > 4) && (((ndx + 1) % 16) <= 12))
                        {
                            expF[ndx] = 90.0f;
                        }
                        else
                        {
                            expF[ndx] = 130.0f;
                        }
                    }

                    break;
                }
                default:
                    break;
                }
            }
            else // m_params.feature == DerivativeFeature::QUADS
            {
                for (uint32_t ndx = 0; ndx < elemCnt; ++ndx)
                {
                    expX[ndx] = 10.0f;
                    expY[ndx] = 20.0f;
                    expF[ndx] = 30.0f;

                    if (((ndx + 1) % 4 == 0) && (m_params.dataType == DataType::VEC3_FLOAT32))
                    {
                        expX[ndx] = 0.0f;
                        expY[ndx] = 0.0f;
                        expF[ndx] = 0.0f;
                    }
                }
            }
        }

        // Retriving result from GPU
        std::vector<float> outX(elemCnt);
        const float *pHostX = static_cast<float *>(allocations[0]->getHostPtr());
        outX.assign(pHostX, pHostX + elemCnt);
        std::vector<float> outY(elemCnt);
        const float *pHostY = static_cast<float *>(allocations[1]->getHostPtr());
        outY.assign(pHostY, pHostY + elemCnt);
        std::vector<float> outF(elemCnt);
        const float *pHostF = static_cast<float *>(allocations[2]->getHostPtr());
        outF.assign(pHostF, pHostF + elemCnt);

        // Comparing results
        for (uint32_t ndx = 0; ndx < elemCnt; ++ndx)
        {
            if (expX[ndx] != outX[ndx])
            {
                outputMatches = false;

                m_context.getTestContext().getLog()
                    << tcu::TestLog::Message << "OutputBufferX got: " << ((float)outX[ndx])
                    << " expected: " << ((float)expX[ndx]) << " at position " << ndx << tcu::TestLog::EndMessage;
            }

            if (expY[ndx] != outY[ndx])
            {
                outputMatches = false;

                m_context.getTestContext().getLog()
                    << tcu::TestLog::Message << "OutputBufferY got: " << ((float)outY[ndx])
                    << " expected: " << ((float)expY[ndx]) << " at position " << ndx << tcu::TestLog::EndMessage;
            }

            if (expF[ndx] != outF[ndx])
            {
                outputMatches = false;

                m_context.getTestContext().getLog()
                    << tcu::TestLog::Message << "OutputBufferF got: " << ((float)outF[ndx])
                    << " expected: " << ((float)expF[ndx]) << " at position " << ndx << tcu::TestLog::EndMessage;
            }
        }

        break;
    }
    case TestType::VERIFY_NDX:
    {
        const uint32_t elemCnt = m_params.numWorkgroup.x() * m_params.numWorkgroup.y() * m_params.numWorkgroup.z();

        // Generating result
        std::vector<uint32_t> expI(elemCnt);
        for (uint32_t ndx = 0; ndx < elemCnt; ++ndx)
        {
            if (m_params.feature == DerivativeFeature::LINEAR)
            {
                expI[ndx] = ndx % 4;
            }
        }

        if (m_params.feature == DerivativeFeature::QUADS)
        {
            for (uint32_t ndy = 0; ndy < m_params.numWorkgroup.y(); ++ndy)
            {
                bool odd = !(ndy % 2);

                for (uint32_t ndx = 0; ndx < m_params.numWorkgroup.x(); ++ndx)
                {
                    const uint32_t linearNdx = ndy * m_params.numWorkgroup.x() + ndx;
                    expI[linearNdx]          = odd ? ndx % 2 : 2 + (ndx % 2);
                }
            }
        }

        // Retriving result from GPU
        std::vector<uint32_t> outS(elemCnt);
        const uint32_t *pHostS = static_cast<uint32_t *>(allocations[0]->getHostPtr());
        outS.assign(pHostS, pHostS + elemCnt);
        std::vector<uint32_t> outI(elemCnt);
        const uint32_t *pHostI = static_cast<uint32_t *>(allocations[1]->getHostPtr());
        outI.assign(pHostI, pHostI + elemCnt);

        // Comparing results
        for (uint32_t ndx = 0; ndx < elemCnt; ++ndx)
        {
            if (expI[ndx] != outI[ndx])
            {
                outputMatches = false;

                m_context.getTestContext().getLog()
                    << tcu::TestLog::Message << "gl_SubgroupInvocationID got: " << (outI[ndx])
                    << " expected: " << (expI[ndx]) << " at position " << ndx << tcu::TestLog::EndMessage;
            }
        }

        for (uint32_t ndy = 0; ndy < m_params.numWorkgroup.y(); ++ndy)
        {
            for (uint32_t ndx = 0; ndx < m_params.numWorkgroup.x(); ++ndx)
            {
                const uint32_t linearNdx = ndy * m_params.numWorkgroup.x() + ndx;
                const uint32_t elem0ndx  = (m_params.feature == DerivativeFeature::LINEAR) ?
                                               (linearNdx & ~3) :
                                               ((ndy & ~1) * m_params.numWorkgroup.x() + (ndx & ~1));

                if (outS[linearNdx] !=
                    outS[elem0ndx]) // Each element of the quad needs to have the same value as element 0
                {
                    outputMatches = false;
                    m_context.getTestContext().getLog()
                        << tcu::TestLog::Message << "Element (" << ndx << ", " << ndy << ")"
                        << " is subgroup " << outS[linearNdx] << " but element 0 of the quad"
                        << " is subgroup " << outS[elem0ndx] << tcu::TestLog::EndMessage;
                }
            }
        }

        break;
    }
    case TestType::QUAD_OPERATIONS:
    {
        const uint32_t elemCnt = m_params.numWorkgroup.x() * m_params.numWorkgroup.y() * m_params.numWorkgroup.z() *
                                 getDataAlignedComponentCount(m_params.dataType);

        // Generating result
        std::vector<float> exp0(elemCnt);
        if (m_params.quadOp == QuadOp::BROADCAST)
        {
            for (uint32_t ndx = 0; ndx < elemCnt; ++ndx)
            {
                exp0[ndx] = 10.0f * static_cast<float>(m_params.quadNdx);

                if (((ndx + 1) % 4 == 0) && (m_params.dataType == DataType::VEC3_FLOAT32))
                {
                    exp0[ndx] = 0.0f;
                }
            }
        }
        else // m_params.quadOp == QuadOp::SWAP
        {
            switch (m_params.quadNdx)
            {
            case 0: // Horizontal
            {
                exp0 = getHorizontallySwappedValues(elemCnt, m_params.dataType, m_params.feature);
                break;
            }
            case 1: // Vertical
            {
                exp0 = getVerticallySwappedValues(elemCnt, m_params.dataType, m_params.feature);
                break;
            }
            case 2: // Diagonal
            {
                exp0 = getDiagonallySwappedValues(elemCnt, m_params.dataType, m_params.feature);
                break;
            }
            default:
                break;
            }
        }

        // Retriving result from GPU
        std::vector<float> out0(elemCnt);
        const float *pHost0 = static_cast<float *>(allocations[0]->getHostPtr());
        out0.assign(pHost0, pHost0 + elemCnt);

        // Comparing results
        for (uint32_t ndx = 0; ndx < elemCnt; ++ndx)
        {
            if (exp0[ndx] != out0[ndx])
            {
                outputMatches = false;

                m_context.getTestContext().getLog()
                    << tcu::TestLog::Message << "OutputBufferX got: " << ((float)out0[ndx])
                    << " expected: " << ((float)exp0[ndx]) << " at position " << ndx << tcu::TestLog::EndMessage;
            }
        }

        break;
    }
    case TestType::LOD_SAMPLE:
    {
        const uint32_t elemCnt = m_params.numWorkgroup.x() * m_params.numWorkgroup.y() * m_params.numWorkgroup.z() *
                                 getDataAlignedComponentCount(DataType::VEC4_FLOAT32);

        // Generating result
        std::vector<float> exp0(elemCnt);
        for (uint32_t ndx = 0; ndx < elemCnt; ndx += 4)
        {
            deMemcpy(&exp0[ndx], &Constants::CLR_COLORS[m_params.mipLvl],
                     getDataAlignedSizeInBytes(DataType::VEC4_FLOAT32));
        }

        // Retriving result from GPU
        std::vector<float> out0(elemCnt);
        const float *pHost0 = static_cast<float *>(allocations[0]->getHostPtr());
        out0.assign(pHost0, pHost0 + elemCnt);

        // Comparing results
        for (uint32_t ndx = 0; ndx < elemCnt; ++ndx)
        {
            if (exp0[ndx] != out0[ndx])
            {
                outputMatches = false;

                m_context.getTestContext().getLog()
                    << tcu::TestLog::Message << "OutputBuffer got: " << ((float)out0[ndx])
                    << " expected: " << ((float)exp0[ndx]) << " at position " << ndx << tcu::TestLog::EndMessage;
            }
        }

        break;
    }
    case TestType::LOD_QUERY:
    {
        const uint32_t elemCnt = m_params.numWorkgroup.x() * m_params.numWorkgroup.y() * m_params.numWorkgroup.z() *
                                 getDataAlignedComponentCount(DataType::VEC2_FLOAT32);

        float lodMin, lodMax;
        if (m_params.feature == DerivativeFeature::LINEAR)
        {
            /*
                mip 0 values - [ 0.0f, 0.08, 0.0, 0.08 ]

                ds/dx = 0.08f, dt/dx = 0.0f
                ds/dy = 0.0f,  dt/dy = 0.0f

                m_ux = 1.28f
                m_uy = 0.0f

                log_2(1.28) <= LOD <= log_2(1.28 * 1.41)
                0.3561 <= LOD <= 0.8561

                =========================================

                mip 1 values - [ 0.0, 0.1, 0.0, 0.1 ]

                ds/dx = 0.1f, dt/dx = 0.0f
                ds/dy = 0.0f, dt/dy = 0.0f

                m_ux = 1.6f
                m_uy = 0.0f

                log_2(1.6) <= LOD <= log_2(1.6 * 1.41)
                0.6781 <= LOD <= 1.1781
            */

            lodMin = !m_params.mipLvl ? 0.3561f : 0.6781f;
            lodMax = !m_params.mipLvl ? 0.8561f : 1.1781f;
        }
        else // m_params.feature == DerivativeFeature::QUADS
        {
            /*
                mip 0 values - [ 0.0f, 0.08, 0.16, 0.24 ]

                ds/dx = dt/dx = 0.08f
                ds/dy = dt/dy = 0.16f

                m_ux = m_vx = 0.32f
                m_uy = m_vy = 0.64f

                log_2(0.64) <= LOD <= log_2((0.64 + 0.64) * 1.41)
                -0.6439 <= LOD <= 0.8561

                =========================================

                mip 1 values - [ 0.0, 0.1, 0.4, 0.5 ]

                ds/dx = dt/dx = 0.1f
                ds/dy = dt/dy = 0.4f

                m_ux = m_vx = 0.4f
                m_uy = m_vy = 1.6f

                log_2(1.6) <= LOD <= log_2((1.6 + 1.6) * 1.41)
                0.6781 <= LOD <= 2.1781
            */

            lodMin = !m_params.mipLvl ? -0.6439f : 0.6781f;
            lodMax = !m_params.mipLvl ? 0.8561f : 2.1781f;
        }

        // Threshold from the midpoint to cover the range [lodMin, lodMax], plus a tolerance
        // for lower accuracy hardware calculations.
        float lodThreshold = 0.015f + (lodMax - lodMin) / 2.0f;

        // Generating result
        std::vector<float> exp0(elemCnt);

        for (uint32_t ndx = 0; ndx < elemCnt; ++ndx)
        {
            if (ndx % 2 == 0)
            {
                exp0[ndx] = static_cast<float>(m_params.mipLvl);
            }
            else
            {
                exp0[ndx] = (lodMin + lodMax) / 2.0f; // Midpoint of [lodMin, lodMax]
            }
        }

        // Retriving result from GPU
        std::vector<float> out0(elemCnt);
        const float *pHost0 = static_cast<float *>(allocations[0]->getHostPtr());
        out0.assign(pHost0, pHost0 + elemCnt);

        // Comparing results
        for (uint32_t ndx = 0; ndx < elemCnt; ++ndx)
        {
            if (ndx % 2 == 0)
            {
                if (exp0[ndx] != out0[ndx])
                {
                    outputMatches = false;

                    m_context.getTestContext().getLog()
                        << tcu::TestLog::Message << "OutputBuffer got: " << ((float)out0[ndx])
                        << " expected: " << ((float)exp0[ndx]) << " at position " << ndx << tcu::TestLog::EndMessage;
                }
            }
            else
            {
                if (!compareFloats(exp0[ndx], out0[ndx], lodThreshold))
                {
                    outputMatches = false;

                    m_context.getTestContext().getLog()
                        << tcu::TestLog::Message << "OutputBuffer got: " << ((float)out0[ndx])
                        << " expected range from: " << ((float)exp0[ndx] - lodThreshold) << " to "
                        << ((float)exp0[ndx] + lodThreshold) << " at position " << ndx << tcu::TestLog::EndMessage;
                }
            }
        }

        break;
    }
    default:
        break;
    }

    return outputMatches;
}

tcu::TestStatus ComputeShaderDerivativeInstance::iterate(void)
{
    const uint32_t queueIndex   = m_context.getUniversalQueueFamilyIndex();
    const VkDevice &device      = m_context.getDevice();
    const DeviceInterface &vkdi = m_context.getDeviceInterface();
    const VkQueue queue         = m_context.getUniversalQueue();

    // Create command pool and command buffer.
    const Move<VkCommandPool> cmdPool(makeCommandPool(vkdi, device, queueIndex));
    const Move<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vkdi, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Create pipeline layout aand resources
    AllocationMp allocations[4];
    Move<VkBuffer> buffers[4];
    std::vector<VkDescriptorBufferInfo> bufferInfos(4);
    for (uint32_t ndx = 0; ndx < 4; ++ndx)
    {
        buffers[ndx] = createBufferAndBindMemory(&allocations[ndx]);

        bufferInfos[ndx].buffer = buffers[ndx].get(); // VkBuffer        buffer
        bufferInfos[ndx].offset = 0;                  // VkDeviceSize    offset
        bufferInfos[ndx].range  = VK_WHOLE_SIZE;      // VkDeviceSize    range
    }
    AllocationMp sampledImageAlloc;
    Move<VkImage> sampledImage(createImageAndBindMemory(
        Constants::SAMPLED_FORMAT, getImageType(m_params.feature), getImageExtent(m_params.feature),
        Constants::MIP_LEVEL_COUNT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, &sampledImageAlloc));
    Move<VkImageView> sampledView(createImageView(Constants::SAMPLED_FORMAT, getImageViewType(m_params.feature),
                                                  Constants::SAMPLED_IMAGE_SRR, *sampledImage));
    Move<VkSampler> sampler(createBasicSampler());

    VkDescriptorImageInfo imgSamplerInfo = {
        *sampler,                         // VkSampler     sampler;
        *sampledView,                     // VkImageView   imageView;
        VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL // VkImageLayout imageLayout;
    };

    Move<VkDescriptorSetLayout> descriptorSetLayout(createDescriptorSetLayout());
    Move<VkPipelineLayout> pipelineLayout(createPipelineLayout(*descriptorSetLayout));
    Move<VkDescriptorPool> descriptorPool(createDescriptorPool());
    Move<VkDescriptorSet> descriptorSet(
        createDescriptorSet(*descriptorPool, *descriptorSetLayout, bufferInfos, imgSamplerInfo));

    vk::BinaryCollection &binCollection = m_context.getBinaryCollection();

    switch (m_params.shaderType)
    {
    case ShaderType::COMPUTE:
    {
        // Create shader module and pipeline
        Move<VkShaderModule> computeModule(createShaderModule(vkdi, device, binCollection.get("compute")));
        Move<VkPipeline> pipeline(
            createComputePipeline(*pipelineLayout, *computeModule, m_params.testType == TestType::VERIFY_NDX));

        // Buffer barrier
        VkBufferMemoryBarrier bufBarrier = {
            VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType    sType
            nullptr,                                 // const void*        pNext
            VK_ACCESS_NONE,                          // VkAccessFlags      srcAccessMask
            VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags      dstAccessMask
            VK_QUEUE_FAMILY_IGNORED,                 // uint32_t           srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                 // uint32_t           dstQueueFamilyIndex
            VK_NULL_HANDLE,                          // VkBuffer           buffer
            0,                                       // VkDeviceSize       offset
            VK_WHOLE_SIZE                            // VkDeviceSize       size
        };

        // Image barrier
        VkImageSubresourceRange sampledMipSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

        VkImageMemoryBarrier imgBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType         sType
            nullptr,                                // const void*             pNext
            VK_ACCESS_NONE,                         // VkAccessFlags           srcAccessMask
            VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags           dstAccessMask
            VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout           oldLayout
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout           newLayout
            VK_QUEUE_FAMILY_IGNORED,                // uint32_t                srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                // uint32_t                dstQueueFamilyIndex
            *sampledImage,                          // VkImage                 image
            Constants::SAMPLED_IMAGE_SRR            // VkImageSubresourceRange subresourceRange
        };

        // Record and submit command buffer
        beginCommandBuffer(vkdi, *cmdBuffer);

        // Clearing buffers
        for (uint32_t ndx = 0; ndx < 4; ++ndx)
        {
            // Pre clear barriers
            bufBarrier.srcAccessMask = VK_ACCESS_NONE;
            bufBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            bufBarrier.buffer        = buffers[ndx].get();
            vkdi.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                                    nullptr, 1, &bufBarrier, 0, nullptr);
            // Clearing SSBO's
            vkdi.cmdFillBuffer(*cmdBuffer, buffers[ndx].get(), 0, VK_WHOLE_SIZE, uint32_t{0x0});
            // Post clear barriers
            bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            bufBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            bufBarrier.buffer        = buffers[ndx].get();
            vkdi.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    getPipelineStageFlagBits(m_params.shaderType), 0, 0, nullptr, 1, &bufBarrier, 0,
                                    nullptr);
        }

        // Clearing image
        // Pre clear barriers
        imgBarrier.srcAccessMask = VK_ACCESS_NONE;
        imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkdi.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                                0u, 0u, 0u, 1u, &imgBarrier);
        for (uint32_t ndx = 0; ndx < Constants::MIP_LEVEL_COUNT; ++ndx)
        {
            sampledMipSRR.baseMipLevel = ndx;
            vkdi.cmdClearColorImage(*cmdBuffer, *sampledImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    &Constants::CLR_COLORS[ndx], 1, &sampledMipSRR);
        }
        // Post clearBarriers
        imgBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imgBarrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imgBarrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkdi.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                getPipelineStageFlagBits(m_params.shaderType), 0u, 0u, 0u, 0u, 0u, 1u, &imgBarrier);

        // Binding pipeline and resources
        vkdi.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
        vkdi.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1,
                                   &descriptorSet.get(), 0, nullptr);

        // Dispath
        vkdi.cmdDispatch(*cmdBuffer, 1, 1, 1);

        for (uint32_t ndx = 0; ndx < 4; ++ndx)
        {
            // Barrier to access data from host
            bufBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            bufBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            bufBarrier.buffer        = buffers[ndx].get();
            vkdi.cmdPipelineBarrier(*cmdBuffer, getPipelineStageFlagBits(m_params.shaderType),
                                    VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &bufBarrier, 0, nullptr);
        }

        endCommandBuffer(vkdi, *cmdBuffer);

        // Wait for GPU work to be done
        submitCommandsAndWait(vkdi, device, queue, *cmdBuffer);

        break;
    }
    case ShaderType::MESH:
    {
        // Primitives for mesh pipeline
        AllocationMp renderTargetAlloc;
        Move<VkImage> renderTarget(createImageAndBindMemory(Constants::RENDER_TARGET_FORMAT, VK_IMAGE_TYPE_2D,
                                                            Constants::EXTENT, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                                            &renderTargetAlloc));
        Move<VkImageView> renderTargetView(createImageView(Constants::RENDER_TARGET_FORMAT, VK_IMAGE_VIEW_TYPE_2D,
                                                           Constants::IMAGE_SRR, *renderTarget));
        Move<VkRenderPass> renderPass(makeRenderPass(vkdi, device, Constants::RENDER_TARGET_FORMAT));
        Move<VkFramebuffer> framebuffer(makeFramebuffer(vkdi, device, *renderPass, *renderTargetView,
                                                        Constants::EXTENT.width, Constants::EXTENT.height));

        VkViewport viewport = makeViewport(Constants::EXTENT);
        VkRect2D scissor    = makeRect2D(Constants::EXTENT);

        // Create shader modules and pipeline
        Move<VkShaderModule> fragmentModule(createShaderModule(vkdi, device, binCollection.get("fragment")));
        Move<VkShaderModule> meshModule(createShaderModule(vkdi, device, binCollection.get("mesh")));
        Move<VkShaderModule> taskModule;
        Move<VkPipeline> pipeline(createGraphicsPipeline(*pipelineLayout, *renderPass, viewport, scissor,
                                                         *fragmentModule, *taskModule, *meshModule,
                                                         m_params.testType == TestType::VERIFY_NDX));

        // Buffer barrier
        VkBufferMemoryBarrier bufBarrier = {
            VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType    sType
            nullptr,                                 // const void*        pNext
            VK_ACCESS_NONE,                          // VkAccessFlags      srcAccessMask
            VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags      dstAccessMask
            VK_QUEUE_FAMILY_IGNORED,                 // uint32_t           srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                 // uint32_t           dstQueueFamilyIndex
            VK_NULL_HANDLE,                          // VkBuffer           buffer
            0,                                       // VkDeviceSize       offset
            VK_WHOLE_SIZE                            // VkDeviceSize       size
        };

        // Image barrier
        VkImageSubresourceRange sampledMipSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

        VkImageMemoryBarrier imgBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType         sType
            nullptr,                                // const void*             pNext
            VK_ACCESS_NONE,                         // VkAccessFlags           srcAccessMask
            VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags           dstAccessMask
            VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout           oldLayout
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout           newLayout
            VK_QUEUE_FAMILY_IGNORED,                // uint32_t                srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                // uint32_t                dstQueueFamilyIndex
            *sampledImage,                          // VkImage                 image
            Constants::SAMPLED_IMAGE_SRR            // VkImageSubresourceRange subresourceRange
        };

        // Record and submit command buffer
        beginCommandBuffer(vkdi, *cmdBuffer);

        // Clearing buffers
        for (uint32_t ndx = 0; ndx < 4; ++ndx)
        {
            // Pre clear barriers
            bufBarrier.srcAccessMask = VK_ACCESS_NONE;
            bufBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            bufBarrier.buffer        = buffers[ndx].get();
            vkdi.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                                    nullptr, 1, &bufBarrier, 0, nullptr);
            // Clearing SSBO's
            vkdi.cmdFillBuffer(*cmdBuffer, buffers[ndx].get(), 0, VK_WHOLE_SIZE, uint32_t{0x0});
            // Post clear barriers
            bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            bufBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            bufBarrier.buffer        = buffers[ndx].get();
            vkdi.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    getPipelineStageFlagBits(m_params.shaderType), 0, 0, nullptr, 1, &bufBarrier, 0,
                                    nullptr);
        }

        // Clearing image
        // Pre clear barriers
        imgBarrier.srcAccessMask = VK_ACCESS_NONE;
        imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkdi.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                                0u, 0u, 0u, 1u, &imgBarrier);
        for (uint32_t ndx = 0; ndx < Constants::MIP_LEVEL_COUNT; ++ndx)
        {
            sampledMipSRR.baseMipLevel = ndx;
            vkdi.cmdClearColorImage(*cmdBuffer, *sampledImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    &Constants::CLR_COLORS[ndx], 1, &sampledMipSRR);
        }
        // Post clearBarriers
        imgBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imgBarrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imgBarrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkdi.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                getPipelineStageFlagBits(m_params.shaderType), 0u, 0u, 0u, 0u, 0u, 1u, &imgBarrier);

        // Binding pipeline and resources
        beginRenderPass(vkdi, *cmdBuffer, *renderPass, *framebuffer, scissor, Constants::CLR_COLOR);
        vkdi.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
        vkdi.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1,
                                   &descriptorSet.get(), 0, nullptr);

        // Mesh task
        vkdi.cmdDrawMeshTasksEXT(*cmdBuffer, 1, 1, 1);
        endRenderPass(vkdi, *cmdBuffer);

        for (uint32_t ndx = 0; ndx < 4; ++ndx)
        {
            // Barrier to access data from host
            bufBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            bufBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            bufBarrier.buffer        = buffers[ndx].get();
            vkdi.cmdPipelineBarrier(*cmdBuffer, getPipelineStageFlagBits(m_params.shaderType),
                                    VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &bufBarrier, 0, nullptr);
        }
        endCommandBuffer(vkdi, *cmdBuffer);

        // Wait for GPU work to be done
        submitCommandsAndWait(vkdi, device, queue, *cmdBuffer);

        break;
    }
    case ShaderType::TASK:
    {
        // Primitives for mesh pipeline
        AllocationMp renderTargetAlloc;
        Move<VkImage> renderTarget(createImageAndBindMemory(Constants::RENDER_TARGET_FORMAT, VK_IMAGE_TYPE_2D,
                                                            Constants::EXTENT, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                                            &renderTargetAlloc));
        Move<VkImageView> renderTargetView(createImageView(Constants::RENDER_TARGET_FORMAT, VK_IMAGE_VIEW_TYPE_2D,
                                                           Constants::IMAGE_SRR, *renderTarget));
        Move<VkRenderPass> renderPass(makeRenderPass(vkdi, device, Constants::RENDER_TARGET_FORMAT));
        Move<VkFramebuffer> framebuffer(makeFramebuffer(vkdi, device, *renderPass, *renderTargetView,
                                                        Constants::EXTENT.width, Constants::EXTENT.height));

        VkViewport viewport = makeViewport(Constants::EXTENT);
        VkRect2D scissor    = makeRect2D(Constants::EXTENT);

        // Create shader modules and pipeline
        Move<VkShaderModule> fragmentModule(createShaderModule(vkdi, device, binCollection.get("fragment")));
        Move<VkShaderModule> meshModule(createShaderModule(vkdi, device, binCollection.get("mesh")));
        Move<VkShaderModule> taskModule(createShaderModule(vkdi, device, binCollection.get("task")));
        Move<VkPipeline> pipeline(createGraphicsPipeline(*pipelineLayout, *renderPass, viewport, scissor,
                                                         *fragmentModule, *taskModule, *meshModule,
                                                         m_params.testType == TestType::VERIFY_NDX));

        // Buffer barrier
        VkBufferMemoryBarrier bufBarrier = {
            VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType    sType
            nullptr,                                 // const void*        pNext
            VK_ACCESS_NONE,                          // VkAccessFlags      srcAccessMask
            VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags      dstAccessMask
            VK_QUEUE_FAMILY_IGNORED,                 // uint32_t           srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                 // uint32_t           dstQueueFamilyIndex
            VK_NULL_HANDLE,                          // VkBuffer           buffer
            0,                                       // VkDeviceSize       offset
            VK_WHOLE_SIZE                            // VkDeviceSize       size
        };

        // Image barrier
        VkImageSubresourceRange sampledMipSRR = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

        VkImageMemoryBarrier imgBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType         sType
            nullptr,                                // const void*             pNext
            VK_ACCESS_NONE,                         // VkAccessFlags           srcAccessMask
            VK_ACCESS_TRANSFER_WRITE_BIT,           // VkAccessFlags           dstAccessMask
            VK_IMAGE_LAYOUT_UNDEFINED,              // VkImageLayout           oldLayout
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,   // VkImageLayout           newLayout
            VK_QUEUE_FAMILY_IGNORED,                // uint32_t                srcQueueFamilyIndex
            VK_QUEUE_FAMILY_IGNORED,                // uint32_t                dstQueueFamilyIndex
            *sampledImage,                          // VkImage                 image
            Constants::SAMPLED_IMAGE_SRR            // VkImageSubresourceRange subresourceRange
        };

        // Record and submit command buffer
        beginCommandBuffer(vkdi, *cmdBuffer);

        // Clearing buffers
        for (uint32_t ndx = 0; ndx < 4; ++ndx)
        {
            // Pre clear barriers
            bufBarrier.srcAccessMask = VK_ACCESS_NONE;
            bufBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            bufBarrier.buffer        = buffers[ndx].get();
            vkdi.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                                    nullptr, 1, &bufBarrier, 0, nullptr);
            // Clearing SSBO's
            vkdi.cmdFillBuffer(*cmdBuffer, buffers[ndx].get(), 0, VK_WHOLE_SIZE, uint32_t{0x0});
            // Post clear barriers
            bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            bufBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            bufBarrier.buffer        = buffers[ndx].get();
            vkdi.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    getPipelineStageFlagBits(m_params.shaderType), 0, 0, nullptr, 1, &bufBarrier, 0,
                                    nullptr);
        }

        // Clearing image
        // Pre clear barriers
        imgBarrier.srcAccessMask = VK_ACCESS_NONE;
        imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkdi.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u,
                                0u, 0u, 0u, 1u, &imgBarrier);
        for (uint32_t ndx = 0; ndx < Constants::MIP_LEVEL_COUNT; ++ndx)
        {
            sampledMipSRR.baseMipLevel = ndx;
            vkdi.cmdClearColorImage(*cmdBuffer, *sampledImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    &Constants::CLR_COLORS[ndx], 1, &sampledMipSRR);
        }
        // Post clearBarriers
        imgBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imgBarrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imgBarrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkdi.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                getPipelineStageFlagBits(m_params.shaderType), 0u, 0u, 0u, 0u, 0u, 1u, &imgBarrier);

        // Binding pipeline and resources
        beginRenderPass(vkdi, *cmdBuffer, *renderPass, *framebuffer, scissor, Constants::CLR_COLOR);
        vkdi.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
        vkdi.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1,
                                   &descriptorSet.get(), 0, nullptr);

        // Mesh task
        vkdi.cmdDrawMeshTasksEXT(*cmdBuffer, 1, 1, 1);
        endRenderPass(vkdi, *cmdBuffer);

        for (uint32_t ndx = 0; ndx < 4; ++ndx)
        {
            // Barrier to access data from host
            bufBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            bufBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            bufBarrier.buffer        = buffers[ndx].get();
            vkdi.cmdPipelineBarrier(*cmdBuffer, getPipelineStageFlagBits(m_params.shaderType),
                                    VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &bufBarrier, 0, nullptr);
        }
        endCommandBuffer(vkdi, *cmdBuffer);

        // Wait for GPU work to be done
        submitCommandsAndWait(vkdi, device, queue, *cmdBuffer);

        break;
    }
    default:
        break;
    }

    bool passed = false;

    // Check results
    std::vector<AllocationMp> allocationsVec(allocations, allocations + 4);
    passed = checkResult(allocationsVec);

    return passed ? tcu::TestStatus::pass("Passed") : tcu::TestStatus::fail("Failed");
}

ComputeShaderDerivativeCase::ComputeShaderDerivativeCase(tcu::TestContext &testCtx, const char *name,
                                                         const ComputeShaderDerivativeTestParams &params)
    : TestCase(testCtx, name)
    , m_params(params)
{
}

void ComputeShaderDerivativeCase::checkSupport(Context &context) const
{
    // For derivative support
    context.requireDeviceFunctionality("VK_KHR_compute_shader_derivatives");

    const VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR &derivativesFeature =
        context.getComputeShaderDerivativesFeatures();

    if (m_params.feature == DerivativeFeature::LINEAR)
    {
        if (!derivativesFeature.computeDerivativeGroupLinear)
            TCU_THROW(NotSupportedError, "computeDerivativeGroupLinear feature is not supported");
    }
    else //    m_params.feature == DerivativeFeature::QUADS
    {
        if (!derivativesFeature.computeDerivativeGroupQuads)
            TCU_THROW(NotSupportedError, "computeDerivativeGroupQuads feature is not supported");
    }

    // For mesh shading support
    if (m_params.shaderType != ShaderType::COMPUTE)
    {
        const VkPhysicalDeviceComputeShaderDerivativesPropertiesKHR &derivativeProps =
            context.getComputeShaderDerivativesProperties();

        if (!derivativeProps.meshAndTaskShaderDerivatives)
            TCU_THROW(NotSupportedError, "derivative operations in mesh and task shader are not supported");

        context.requireDeviceFunctionality("VK_EXT_mesh_shader");

        const VkPhysicalDeviceMeshShaderFeaturesEXT &meshFeature = context.getMeshShaderFeaturesEXT();

        if (m_params.shaderType == ShaderType::MESH)
        {
            if (!meshFeature.meshShader)
                TCU_THROW(NotSupportedError, "mesh shader feature is not supported");
        }
        else if (m_params.shaderType == ShaderType::TASK)
        {
            if (!meshFeature.meshShader)
                TCU_THROW(NotSupportedError, "mesh shader feature is not supported");

            if (!meshFeature.taskShader)
                TCU_THROW(NotSupportedError, "task shader feature is not supported");
        }
    }

    // For subgroup operations
    if ((m_params.testType == TestType::VERIFY_NDX) || (m_params.testType == TestType::QUAD_OPERATIONS))
    {
        if (context.getEquivalentApiVersion() < VK_API_VERSION_1_1)
            TCU_THROW(NotSupportedError, "Profile not supported");

        const VkPhysicalDeviceSubgroupProperties &subgroupProps = context.getSubgroupProperties();

        if (!(subgroupProps.supportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT))
            TCU_THROW(NotSupportedError, "basic subgroup operations are not supported");

        if (m_params.testType == TestType::QUAD_OPERATIONS)
        {
            if (!(subgroupProps.supportedOperations & VK_SUBGROUP_FEATURE_QUAD_BIT))
                TCU_THROW(NotSupportedError, "quad operations are not supported");
        }

        if (!(subgroupProps.supportedStages & getShaderStageFlagBits(m_params.shaderType)))
            TCU_THROW(NotSupportedError, "requested subgroup operations are not supported in " +
                                             std::string(toString(m_params.shaderType)) + " stage");

        // VUID-VkPipelineShaderStageCreateInfo-flags-02759
        if (m_params.testType == TestType::VERIFY_NDX)
        {
            if (m_params.numWorkgroup.x() % subgroupProps.subgroupSize != 0)
                TCU_THROW(NotSupportedError, "workgroup X dimension (" + std::to_string(m_params.numWorkgroup.x()) +
                                                 ") is not a multiple of subgroupSize (" +
                                                 std::to_string(subgroupProps.subgroupSize) + ")");
        }
    }
}

void ComputeShaderDerivativeCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::string testShaderStr("");
    std::string meshShaderStr(
        "OpCapability MeshShadingEXT\n"
        "OpExtension \"SPV_EXT_mesh_shader\"\n"
        "OpMemoryModel Logical GLSL450\n"
        "OpEntryPoint MeshEXT %main \"main\" %gl_MeshVerticesEXT %gl_PrimitiveTriangleIndicesEXT\n"
        "OpExecutionMode %main LocalSize 1 1 1\n"
        "OpExecutionMode %main OutputVertices 3\n"
        "OpExecutionMode %main OutputPrimitivesEXT 1\n"
        "OpExecutionMode %main OutputTrianglesEXT\n"

        // Decorations
        "OpMemberDecorate %gl_MeshPerVertexEXT 0 BuiltIn Position\n"
        "OpMemberDecorate %gl_MeshPerVertexEXT 1 BuiltIn PointSize\n"
        "OpMemberDecorate %gl_MeshPerVertexEXT 2 BuiltIn ClipDistance\n"
        "OpMemberDecorate %gl_MeshPerVertexEXT 3 BuiltIn CullDistance\n"
        "OpDecorate       %gl_MeshPerVertexEXT Block\n"
        "OpDecorate       %gl_PrimitiveTriangleIndicesEXT BuiltIn PrimitiveTriangleIndicesEXT\n"

        // Types
        "%void         = OpTypeVoid\n"
        "%void_func    = OpTypeFunction %void\n"
        "%uint32       = OpTypeInt      32      0\n"
        "%float32      = OpTypeFloat    32\n"
        "%vec3_uint32  = OpTypeVector   %uint32  3\n"
        "%vec2_float32 = OpTypeVector   %float32 2\n"
        "%vec3_float32 = OpTypeVector   %float32 3\n"
        "%vec4_float32 = OpTypeVector   %float32 4\n"

        // Constants
        "%c_uint32_0     = OpConstant          %uint32  0\n"
        "%c_uint32_1     = OpConstant          %uint32  1\n"
        "%c_uint32_2     = OpConstant          %uint32  2\n"
        "%c_uint32_3     = OpConstant          %uint32  3\n"
        "%c_uint32_4     = OpConstant          %uint32  4\n"
        "%c_uint32_16    = OpConstant          %uint32  16\n"
        "%c_uint32_32    = OpConstant          %uint32  32\n"
        "%c_uint32_128   = OpConstant          %uint32  128\n"
        "%c_float32_0    = OpConstant          %float32 0\n"
        "%c_float32_0_5  = OpConstant          %float32 0.5\n"
        "%c_float32_n0_5 = OpConstant          %float32 -0.5\n"
        "%c_float32_10   = OpConstant          %float32 10\n"
        "%c_float32_20   = OpConstant          %float32 20\n"
        "%c_vertex_0     = OpConstantComposite %vec4_float32 %c_float32_n0_5 %c_float32_n0_5 %c_float32_0 "
        "%c_float32_0\n"
        "%c_vertex_1     = OpConstantComposite %vec4_float32 %c_float32_0_5  %c_float32_n0_5 %c_float32_0 "
        "%c_float32_0\n"
        "%c_vertex_2     = OpConstantComposite %vec4_float32 %c_float32_0    %c_float32_0_5  %c_float32_0 "
        "%c_float32_0\n"
        "%c_indices      = OpConstantComposite %vec3_uint32  %c_uint32_0     %c_uint32_1     %c_uint32_2\n"

        // Arrays
        "%array_float32_1     = OpTypeArray %float32     %c_uint32_1\n"
        "%array_vec3_uint32_1 = OpTypeArray %vec3_uint32 %c_uint32_1\n"

        // Structs
        "%gl_MeshPerVertexEXT = OpTypeStruct %vec4_float32 %float32 %array_float32_1 %array_float32_1\n"

        // Arrays
        "%array_gl_MeshPerVertexEXT_3 = OpTypeArray %gl_MeshPerVertexEXT %c_uint32_3\n"

        // Pointers
        "%vec4_float32_output_ptr                = OpTypePointer Output        %vec4_float32\n"
        "%vec3_uint32_output_ptr                 = OpTypePointer Output        %vec3_uint32\n"
        "%array_vec3_uint32_output_ptr           = OpTypePointer Output        %array_vec3_uint32_1\n"
        "%array_gl_MeshPerVertexEXT_3_output_ptr = OpTypePointer Output        %array_gl_MeshPerVertexEXT_3\n"

        // Variables
        "%gl_MeshVerticesEXT             = OpVariable %array_gl_MeshPerVertexEXT_3_output_ptr Output\n"
        "%gl_PrimitiveTriangleIndicesEXT = OpVariable %array_vec3_uint32_output_ptr           Output\n"

        // Main
        "%main               = OpFunction %void None %void_func\n"
        "%label_main         = OpLabel\n"
        // Mesh output
        "                      OpSetMeshOutputsEXT %c_uint32_3 %c_uint32_1\n"
        "%position_loc_0     = OpAccessChain %vec4_float32_output_ptr %gl_MeshVerticesEXT %c_uint32_0 "
        "%c_uint32_0\n"
        "                      OpStore       %position_loc_0          %c_vertex_0\n"
        "%position_loc_1     = OpAccessChain %vec4_float32_output_ptr %gl_MeshVerticesEXT %c_uint32_1 "
        "%c_uint32_0\n"
        "                      OpStore       %position_loc_1          %c_vertex_1\n"
        "%position_loc_2     = OpAccessChain %vec4_float32_output_ptr %gl_MeshVerticesEXT %c_uint32_2 "
        "%c_uint32_0\n"
        "                      OpStore       %position_loc_2          %c_vertex_2\n"
        "%indices_loc        = OpAccessChain %vec3_uint32_output_ptr  %gl_PrimitiveTriangleIndicesEXT "
        "%c_uint32_0\n"
        "                      OpStore       %indices_loc             %c_indices\n"

        "                      OpReturn\n"
        "                      OpFunctionEnd\n");

    std::string fragmentShaderStr("OpCapability Shader\n"
                                  "OpMemoryModel Logical GLSL450\n"
                                  "OpEntryPoint Fragment %main \"main\" %frag_color\n"
                                  "OpExecutionMode %main OriginUpperLeft\n"

                                  // Decorations
                                  "OpDecorate %frag_color Location 0\n"

                                  // Types
                                  "%void         = OpTypeVoid\n"
                                  "%void_func    = OpTypeFunction %void\n"
                                  "%float32      = OpTypeFloat    32\n"
                                  "%vec4_float32 = OpTypeVector   %float32 4\n"

                                  // Constants
                                  "%c_float32_0    = OpConstant %float32 0\n"
                                  "%c_float32_1    = OpConstant %float32 1\n"
                                  "%c_output_color = OpConstantComposite %vec4_float32 "
                                  "%c_float32_1 %c_float32_0 %c_float32_0 %c_float32_1\n"

                                  // Pointers
                                  "%vec4_float32_output_ptr = OpTypePointer Output %vec4_float32\n"

                                  // Variables
                                  "%frag_color = OpVariable %vec4_float32_output_ptr Output\n"

                                  // Main
                                  "%main       = OpFunction %void None %void_func\n"
                                  "%label_main = OpLabel\n"
                                  "              OpStore %frag_color %c_output_color\n"
                                  "              OpReturn\n"
                                  "              OpFunctionEnd\n");

    switch (m_params.shaderType)
    {
    case ShaderType::COMPUTE:
    {
        // Universal compute shader
        testShaderStr =
            "OpCapability Shader\n"
            "OpCapability ${capability}\n"
            "${sampleCap:opt}\n"
            "${queryCap:opt}\n"
            "OpCapability DerivativeControl\n"
            "OpCapability GroupNonUniformQuad\n"
            "OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
            "OpExtension \"SPV_KHR_compute_shader_derivatives\"\n"
            "OpMemoryModel Logical GLSL450\n"
            "OpEntryPoint GLCompute %main \"main\" %gl_LocalInvocationID %gl_SubgroupID %gl_SubgroupInvocationID\n"
            "OpExecutionMode %main LocalSize ${x} ${y} ${z}\n"
            "OpExecutionMode %main ${executionMode}\n"

            // Decorations
            "OpDecorate      %gl_LocalInvocationID    BuiltIn     LocalInvocationId\n"
            "OpDecorate      %gl_SubgroupID           BuiltIn     SubgroupId\n"
            "OpDecorate      %gl_SubgroupInvocationID BuiltIn     SubgroupLocalInvocationId\n"
            "OpDecorate      %out_array               ArrayStride ${arrayStride}\n"
            // Output X
            "OpMemberDecorate %out_x 0 Offset 0\n"
            "OpDecorate       %out_x Block\n"
            "OpDecorate       %out_x_var DescriptorSet 0\n"
            "OpDecorate       %out_x_var Binding       0\n"
            // Output Y
            "OpMemberDecorate %out_y 0 Offset 0\n"
            "OpDecorate       %out_y Block\n"
            "OpDecorate       %out_y_var DescriptorSet 0\n"
            "OpDecorate       %out_y_var Binding       1\n"
            // Output F
            "OpMemberDecorate %out_f 0 Offset 0\n"
            "OpDecorate       %out_f Block\n"
            "OpDecorate       %out_f_var DescriptorSet 0\n"
            "OpDecorate       %out_f_var Binding       2\n"
            "${decorations:opt}\n"

            // Types
            "%void         = OpTypeVoid\n"
            "%void_func    = OpTypeFunction %void\n"
            "%uint32       = OpTypeInt      32       0\n"
            "%vec3_uint32  = OpTypeVector   %uint32  3\n"
            "%float32      = OpTypeFloat    32\n"
            "%vec2_float32 = OpTypeVector   %float32 2\n"
            "%vec3_float32 = OpTypeVector   %float32 3\n"
            "%vec4_float32 = OpTypeVector   %float32 4\n"

            // Constants
            "%c_uint32_0     = OpConstant %uint32  0\n"
            "%c_uint32_1     = OpConstant %uint32  1\n"
            "%c_uint32_2     = OpConstant %uint32  2\n"
            "%c_uint32_3     = OpConstant %uint32  3\n"
            "%c_uint32_4     = OpConstant %uint32  4\n"
            "%c_uint32_16    = OpConstant %uint32  16\n"
            "%c_uint32_32    = OpConstant %uint32  32\n"
            "%c_uint32_128   = OpConstant %uint32  128\n"
            "%c_float32_2    = OpConstant %float32 2\n"
            "%c_float32_3    = OpConstant %float32 3\n"
            "%c_float32_4    = OpConstant %float32 4\n"
            "%c_float32_10   = OpConstant %float32 10\n"
            "%c_float32_20   = OpConstant %float32 20\n"
            "%c_float32_0_08 = OpConstant %float32 0.08\n"
            "%c_float32_0_10 = OpConstant %float32 0.10\n"
            "%c_float32_0_12 = OpConstant %float32 0.12\n"

            // Arrays
            "%out_array = ${arrayDeclaration}\n"

            // Structs
            "%out_x = OpTypeStruct %out_array\n"
            "%out_y = OpTypeStruct %out_array\n"
            "%out_f = OpTypeStruct %out_array\n"

            // Pointers
            "%uint32_input_ptr              = OpTypePointer Input         %uint32\n"
            "%vec3_uint32_input_ptr         = OpTypePointer Input         %vec3_uint32\n"

            "%out_x_storage_buffer_ptr      = OpTypePointer StorageBuffer %out_x\n"
            "%out_y_storage_buffer_ptr      = OpTypePointer StorageBuffer %out_y\n"
            "%out_f_storage_buffer_ptr      = OpTypePointer StorageBuffer %out_f\n"
            "${dataType}_storage_buffer_ptr = OpTypePointer StorageBuffer ${dataType}\n"

            // Variables
            "%gl_LocalInvocationID    = OpVariable %vec3_uint32_input_ptr    Input\n"
            "%gl_SubgroupID           = OpVariable %uint32_input_ptr         Input\n"
            "%gl_SubgroupInvocationID = OpVariable %uint32_input_ptr         Input\n"
            "%out_x_var               = OpVariable %out_x_storage_buffer_ptr StorageBuffer\n"
            "%out_y_var               = OpVariable %out_y_storage_buffer_ptr StorageBuffer\n"
            "%out_f_var               = OpVariable %out_f_storage_buffer_ptr StorageBuffer\n"

            "${images:opt}\n"

            // Main
            "%main               = OpFunction %void None %void_func\n"
            "%label_main         = OpLabel\n"
            // Quering GroupThreadID
            "%gl_LocalInvocationID_x = OpAccessChain %uint32_input_ptr %gl_LocalInvocationID   %c_uint32_0\n"
            "%ndx_uint32             = OpLoad        %uint32           %gl_LocalInvocationID_x\n"
            "%gl_LocalInvocationID_y = OpAccessChain %uint32_input_ptr %gl_LocalInvocationID   %c_uint32_1\n"
            "%ndy_uint32             = OpLoad        %uint32           %gl_LocalInvocationID_y\n"
            "${linearNdxMul}\n"
            "%linear_ndx             = OpIAdd        %uint32           %ndx_uint32 %multi_ndy_uint32\n"
            // Generating test values
            "${testValueCode:opt}\n"
            // Calculating derivatives
            "${testLogicCode}\n"
            // Storing values in output buffer
            "${storeCode}\n"

            "                      OpReturn\n"
            "                      OpFunctionEnd\n";

        break;
    }
    case ShaderType::MESH:
    {
        // Universal mesh shader
        testShaderStr =
            "OpCapability MeshShadingEXT\n"
            "OpCapability ${capability}\n"
            "${sampleCap:opt}\n"
            "${queryCap:opt}\n"
            "OpCapability DerivativeControl\n"
            "OpCapability GroupNonUniformQuad\n"
            "OpExtension \"SPV_EXT_mesh_shader\"\n"
            "OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
            "OpExtension \"SPV_KHR_compute_shader_derivatives\"\n"
            "OpMemoryModel Logical GLSL450\n"
            "OpEntryPoint MeshEXT %main \"main\" %out_x_var %out_y_var %out_f_var %gl_LocalInvocationID %gl_SubgroupID "
            "%gl_SubgroupInvocationID ${interface:opt} %gl_MeshVerticesEXT %gl_PrimitiveTriangleIndicesEXT\n"
            "OpExecutionMode %main LocalSize ${x} ${y} ${z}\n"
            "OpExecutionMode %main ${executionMode}\n"
            "OpExecutionMode %main OutputVertices 3\n"
            "OpExecutionMode %main OutputPrimitivesEXT 1\n"
            "OpExecutionMode %main OutputTrianglesEXT\n"

            // Decorations
            "OpMemberDecorate %gl_MeshPerVertexEXT 0 BuiltIn Position\n"
            "OpMemberDecorate %gl_MeshPerVertexEXT 1 BuiltIn PointSize\n"
            "OpMemberDecorate %gl_MeshPerVertexEXT 2 BuiltIn ClipDistance\n"
            "OpMemberDecorate %gl_MeshPerVertexEXT 3 BuiltIn CullDistance\n"
            "OpDecorate       %gl_MeshPerVertexEXT Block\n"
            "OpDecorate       %gl_PrimitiveTriangleIndicesEXT BuiltIn     PrimitiveTriangleIndicesEXT\n"
            "OpDecorate       %gl_LocalInvocationID           BuiltIn     LocalInvocationId\n"
            "OpDecorate       %gl_SubgroupID                  BuiltIn     SubgroupId\n"
            "OpDecorate       %gl_SubgroupInvocationID        BuiltIn     SubgroupLocalInvocationId\n"
            "OpDecorate       %out_array                      ArrayStride ${arrayStride}\n"
            // Output X
            "OpMemberDecorate %out_x 0 Offset 0\n"
            "OpDecorate       %out_x Block\n"
            "OpDecorate       %out_x_var DescriptorSet 0\n"
            "OpDecorate       %out_x_var Binding       0\n"
            // Output Y
            "OpMemberDecorate %out_y 0 Offset 0\n"
            "OpDecorate       %out_y Block\n"
            "OpDecorate       %out_y_var DescriptorSet 0\n"
            "OpDecorate       %out_y_var Binding       1\n"
            // Output F
            "OpMemberDecorate %out_f 0 Offset 0\n"
            "OpDecorate       %out_f Block\n"
            "OpDecorate       %out_f_var DescriptorSet 0\n"
            "OpDecorate       %out_f_var Binding       2\n"
            "${decorations:opt}\n"

            // Types
            "%void         = OpTypeVoid\n"
            "%void_func    = OpTypeFunction %void\n"
            "%uint32       = OpTypeInt      32       0\n"
            "%vec3_uint32  = OpTypeVector   %uint32  3\n"
            "%float32      = OpTypeFloat    32\n"
            "%vec2_float32 = OpTypeVector   %float32 2\n"
            "%vec3_float32 = OpTypeVector   %float32 3\n"
            "%vec4_float32 = OpTypeVector   %float32 4\n"

            // Constants
            "%c_uint32_0     = OpConstant          %uint32  0\n"
            "%c_uint32_1     = OpConstant          %uint32  1\n"
            "%c_uint32_2     = OpConstant          %uint32  2\n"
            "%c_uint32_3     = OpConstant          %uint32  3\n"
            "%c_uint32_4     = OpConstant          %uint32  4\n"
            "%c_uint32_16    = OpConstant          %uint32  16\n"
            "%c_uint32_32    = OpConstant          %uint32  32\n"
            "%c_uint32_128   = OpConstant          %uint32  128\n"
            "%c_float32_0    = OpConstant          %float32 0\n"
            "%c_float32_0_5  = OpConstant          %float32 0.5\n"
            "%c_float32_n0_5 = OpConstant          %float32 -0.5\n"
            "%c_float32_2    = OpConstant          %float32 2\n"
            "%c_float32_3    = OpConstant          %float32 3\n"
            "%c_float32_4    = OpConstant          %float32 4\n"
            "%c_float32_10   = OpConstant          %float32 10\n"
            "%c_float32_20   = OpConstant          %float32 20\n"
            "%c_float32_0_08 = OpConstant %float32 0.08\n"
            "%c_float32_0_10 = OpConstant %float32 0.10\n"
            "%c_float32_0_12 = OpConstant %float32 0.12\n"
            "%c_vertex_0     = OpConstantComposite %vec4_float32 %c_float32_n0_5 %c_float32_n0_5 %c_float32_0 "
            "%c_float32_0\n"
            "%c_vertex_1     = OpConstantComposite %vec4_float32 %c_float32_0_5  %c_float32_n0_5 %c_float32_0 "
            "%c_float32_0\n"
            "%c_vertex_2     = OpConstantComposite %vec4_float32 %c_float32_0    %c_float32_0_5  %c_float32_0 "
            "%c_float32_0\n"
            "%c_indices      = OpConstantComposite %vec3_uint32  %c_uint32_0     %c_uint32_1     %c_uint32_2\n"

            // Arrays
            "%out_array           = ${arrayDeclaration}\n"
            "%array_float32_1     = OpTypeArray %float32     %c_uint32_1\n"
            "%array_vec3_uint32_1 = OpTypeArray %vec3_uint32 %c_uint32_1\n"

            // Structs
            "%out_x               = OpTypeStruct %out_array\n"
            "%out_y               = OpTypeStruct %out_array\n"
            "%out_f               = OpTypeStruct %out_array\n"
            "%gl_MeshPerVertexEXT = OpTypeStruct %vec4_float32 %float32 %array_float32_1 %array_float32_1\n"

            // Arrays
            "%array_gl_MeshPerVertexEXT_3 = OpTypeArray %gl_MeshPerVertexEXT %c_uint32_3\n"

            // Pointers
            "%uint32_input_ptr                       = OpTypePointer Input         %uint32\n"
            "%vec3_uint32_input_ptr                  = OpTypePointer Input         %vec3_uint32\n"
            "%out_x_storage_buffer_ptr               = OpTypePointer StorageBuffer %out_x\n"
            "%out_y_storage_buffer_ptr               = OpTypePointer StorageBuffer %out_y\n"
            "%out_f_storage_buffer_ptr               = OpTypePointer StorageBuffer %out_f\n"
            "${dataType}_storage_buffer_ptr          = OpTypePointer StorageBuffer ${dataType}\n"
            "%vec4_float32_output_ptr                = OpTypePointer Output        %vec4_float32\n"
            "%vec3_uint32_output_ptr                 = OpTypePointer Output        %vec3_uint32\n"
            "%array_vec3_uint32_output_ptr           = OpTypePointer Output        %array_vec3_uint32_1\n"
            "%array_gl_MeshPerVertexEXT_3_output_ptr = OpTypePointer Output        %array_gl_MeshPerVertexEXT_3\n"

            // Variables
            "%gl_LocalInvocationID           = OpVariable %vec3_uint32_input_ptr                  Input\n"
            "%gl_SubgroupID                  = OpVariable %uint32_input_ptr                       Input\n"
            "%gl_SubgroupInvocationID        = OpVariable %uint32_input_ptr                       Input\n"
            "%out_x_var                      = OpVariable %out_x_storage_buffer_ptr               StorageBuffer\n"
            "%out_y_var                      = OpVariable %out_y_storage_buffer_ptr               StorageBuffer\n"
            "%out_f_var                      = OpVariable %out_f_storage_buffer_ptr               StorageBuffer\n"
            "%gl_MeshVerticesEXT             = OpVariable %array_gl_MeshPerVertexEXT_3_output_ptr Output\n"
            "%gl_PrimitiveTriangleIndicesEXT = OpVariable %array_vec3_uint32_output_ptr           Output\n"

            "${images:opt}\n"

            // Main
            "%main               = OpFunction %void None %void_func\n"
            "%label_main         = OpLabel\n"
            // Quering GroupThreadID
            "%gl_LocalInvocationID_x = OpAccessChain %uint32_input_ptr %gl_LocalInvocationID   %c_uint32_0\n"
            "%ndx_uint32             = OpLoad        %uint32           %gl_LocalInvocationID_x\n"
            "%gl_LocalInvocationID_y = OpAccessChain %uint32_input_ptr %gl_LocalInvocationID   %c_uint32_1\n"
            "%ndy_uint32             = OpLoad        %uint32           %gl_LocalInvocationID_y\n"
            "${linearNdxMul}\n"
            "%linear_ndx             = OpIAdd        %uint32           %ndx_uint32 %multi_ndy_uint32\n"
            // Generating test values
            "${testValueCode:opt}\n"
            // Calculating derivatives
            "${testLogicCode}\n"
            // Storing values in output buffer
            "${storeCode}\n"
            // Mesh output
            "                      OpSetMeshOutputsEXT %c_uint32_3 %c_uint32_1\n"
            "%position_loc_0     = OpAccessChain %vec4_float32_output_ptr %gl_MeshVerticesEXT %c_uint32_0 "
            "%c_uint32_0\n"
            "                      OpStore       %position_loc_0          %c_vertex_0\n"
            "%position_loc_1     = OpAccessChain %vec4_float32_output_ptr %gl_MeshVerticesEXT %c_uint32_1 "
            "%c_uint32_0\n"
            "                      OpStore       %position_loc_1          %c_vertex_1\n"
            "%position_loc_2     = OpAccessChain %vec4_float32_output_ptr %gl_MeshVerticesEXT %c_uint32_2 "
            "%c_uint32_0\n"
            "                      OpStore       %position_loc_2          %c_vertex_2\n"
            "%indices_loc        = OpAccessChain %vec3_uint32_output_ptr  %gl_PrimitiveTriangleIndicesEXT "
            "%c_uint32_0\n"
            "                      OpStore       %indices_loc             %c_indices\n"

            "                      OpReturn\n"
            "                      OpFunctionEnd\n";

        break;
    }
    case ShaderType::TASK:
    {
        // Universal task shader
        testShaderStr =
            "OpCapability MeshShadingEXT\n"
            "OpCapability ${capability}\n"
            "${sampleCap:opt}\n"
            "${queryCap:opt}\n"
            "OpCapability DerivativeControl\n"
            "OpCapability GroupNonUniformQuad\n"
            "OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
            "OpExtension \"SPV_KHR_compute_shader_derivatives\"\n"
            "OpExtension \"SPV_EXT_mesh_shader\"\n"
            "OpMemoryModel Logical GLSL450\n"
            "OpEntryPoint TaskEXT %main \"main\" %out_x_var %out_y_var %out_f_var %gl_LocalInvocationID %gl_SubgroupID "
            "%gl_SubgroupInvocationID ${interface:opt}\n"
            "OpExecutionMode %main LocalSize ${x} ${y} ${z}\n"
            "OpExecutionMode %main ${executionMode}\n"

            // Decorations
            "OpDecorate      %gl_LocalInvocationID    BuiltIn     LocalInvocationId\n"
            "OpDecorate      %gl_SubgroupID           BuiltIn     SubgroupId\n"
            "OpDecorate      %gl_SubgroupInvocationID BuiltIn     SubgroupLocalInvocationId\n"
            "OpDecorate      %out_array               ArrayStride ${arrayStride}\n"
            // Output X
            "OpMemberDecorate %out_x 0 Offset 0\n"
            "OpDecorate       %out_x Block\n"
            "OpDecorate       %out_x_var DescriptorSet 0\n"
            "OpDecorate       %out_x_var Binding       0\n"
            // Output Y
            "OpMemberDecorate %out_y 0 Offset 0\n"
            "OpDecorate       %out_y Block\n"
            "OpDecorate       %out_y_var DescriptorSet 0\n"
            "OpDecorate       %out_y_var Binding       1\n"
            // Output F
            "OpMemberDecorate %out_f 0 Offset 0\n"
            "OpDecorate       %out_f Block\n"
            "OpDecorate       %out_f_var DescriptorSet 0\n"
            "OpDecorate       %out_f_var Binding       2\n"
            "${decorations:opt}\n"

            // Types
            "%void         = OpTypeVoid\n"
            "%void_func    = OpTypeFunction %void\n"
            "%uint32       = OpTypeInt      32      0\n"
            "%float32      = OpTypeFloat    32\n"
            "%vec3_uint32  = OpTypeVector   %uint32  3\n"
            "%vec2_float32 = OpTypeVector   %float32 2\n"
            "%vec3_float32 = OpTypeVector   %float32 3\n"
            "%vec4_float32 = OpTypeVector   %float32 4\n"

            // Constants
            "%c_uint32_0     = OpConstant %uint32  0\n"
            "%c_uint32_1     = OpConstant %uint32  1\n"
            "%c_uint32_2     = OpConstant %uint32  2\n"
            "%c_uint32_3     = OpConstant %uint32  3\n"
            "%c_uint32_4     = OpConstant %uint32  4\n"
            "%c_uint32_16    = OpConstant %uint32  16\n"
            "%c_uint32_32    = OpConstant %uint32  32\n"
            "%c_uint32_128   = OpConstant %uint32  128\n"
            "%c_float32_2    = OpConstant %float32 2\n"
            "%c_float32_3    = OpConstant %float32 3\n"
            "%c_float32_4    = OpConstant %float32 4\n"
            "%c_float32_10   = OpConstant %float32 10\n"
            "%c_float32_20   = OpConstant %float32 20\n"
            "%c_float32_0_08 = OpConstant %float32 0.08\n"
            "%c_float32_0_10 = OpConstant %float32 0.10\n"
            "%c_float32_0_12 = OpConstant %float32 0.12\n"

            // Arrays
            "%out_array = ${arrayDeclaration}\n"

            // Structs
            "%out_x = OpTypeStruct %out_array\n"
            "%out_y = OpTypeStruct %out_array\n"
            "%out_f = OpTypeStruct %out_array\n"

            // Pointers
            "%uint32_input_ptr              = OpTypePointer Input         %uint32\n"
            "%vec3_uint32_input_ptr         = OpTypePointer Input         %vec3_uint32\n"
            "%out_x_storage_buffer_ptr      = OpTypePointer StorageBuffer %out_x\n"
            "%out_y_storage_buffer_ptr      = OpTypePointer StorageBuffer %out_y\n"
            "%out_f_storage_buffer_ptr      = OpTypePointer StorageBuffer %out_f\n"
            "${dataType}_storage_buffer_ptr = OpTypePointer StorageBuffer ${dataType}\n"

            // Variables
            "%gl_LocalInvocationID    = OpVariable %vec3_uint32_input_ptr    Input\n"
            "%gl_SubgroupID           = OpVariable %uint32_input_ptr         Input\n"
            "%gl_SubgroupInvocationID = OpVariable %uint32_input_ptr         Input\n"
            "%out_x_var               = OpVariable %out_x_storage_buffer_ptr StorageBuffer\n"
            "%out_y_var               = OpVariable %out_y_storage_buffer_ptr StorageBuffer\n"
            "%out_f_var               = OpVariable %out_f_storage_buffer_ptr StorageBuffer\n"

            "${images:opt}\n"

            // Main
            "%main               = OpFunction %void None %void_func\n"
            "%label_main         = OpLabel\n"
            // Quering GroupThreadID
            "%gl_LocalInvocationID_x = OpAccessChain %uint32_input_ptr %gl_LocalInvocationID   %c_uint32_0\n"
            "%ndx_uint32             = OpLoad        %uint32           %gl_LocalInvocationID_x\n"
            "%gl_LocalInvocationID_y = OpAccessChain %uint32_input_ptr %gl_LocalInvocationID   %c_uint32_1\n"
            "%ndy_uint32             = OpLoad        %uint32           %gl_LocalInvocationID_y\n"
            "${linearNdxMul}\n"
            "%linear_ndx             = OpIAdd        %uint32           %ndx_uint32 %multi_ndy_uint32\n"
            // Generating test values
            "${testValueCode:opt}\n"
            // Calculating derivatives
            "${testLogicCode}\n"
            // Storing values in output buffer
            "${storeCode}\n"
            // Task output
            "                      OpEmitMeshTasksEXT %c_uint32_1 %c_uint32_1 %c_uint32_1\n"

            "                      OpFunctionEnd\n";

        break;
    }
    default:
        break;
    }

    // Createing shaders from templates
    switch (m_params.testType)
    {
    case TestType::DERIVATIVE_VALUE:
    {
        tcu::StringTemplate preTemp(testShaderStr);

        std::map<std::string, std::string> specMap;
        specMap["x"]                = std::to_string(m_params.numWorkgroup.x());
        specMap["y"]                = std::to_string(m_params.numWorkgroup.y());
        specMap["z"]                = std::to_string(m_params.numWorkgroup.z());
        specMap["capability"]       = getDerivativeCapability(m_params.feature);
        specMap["executionMode"]    = getDerivativeExecutionMode(m_params.feature);
        specMap["arrayStride"]      = std::to_string(getDataAlignedSizeInBytes(m_params.dataType));
        specMap["arrayDeclaration"] = getArrayDeclaration(m_params.dataType);
        specMap["testValueCode"]    = getTestValueCode(m_params.feature, m_params.variant, m_params.dataType);
        specMap["dataType"]         = getDataType(m_params.dataType);
        specMap["linearNdxMul"]     = getLinearNdxMul(m_params.testType);
        specMap["testLogicCode"]    = "%dx                 = ${dxFunc}     ${dataType}       %test_value\n"
                                      "%dy                 = ${dyFunc}     ${dataType}       %test_value\n"
                                      "%fwidth             = ${dwidthFunc} ${dataType}       %test_value\n";
        specMap["storeCode"] =
            "%out_x_loc          = OpAccessChain ${dataType}_storage_buffer_ptr %out_x_var %c_uint32_0 "
            "%${storeNdx}\n"
            "                      OpStore       %out_x_loc                     %dx\n"
            "%out_y_loc          = OpAccessChain ${dataType}_storage_buffer_ptr %out_y_var %c_uint32_0 "
            "%${storeNdx}\n"
            "                      OpStore       %out_y_loc                     %dy\n"
            "%out_f_loc          = OpAccessChain ${dataType}_storage_buffer_ptr %out_f_var %c_uint32_0 "
            "%${storeNdx}\n"
            "                      OpStore       %out_f_loc                     %fwidth\n";

        tcu::StringTemplate specTemp(preTemp.specialize(specMap));
        specMap.clear();
        specMap["dataType"]   = getDataType(m_params.dataType);
        specMap["dxFunc"]     = getDxFunc(m_params.variant);
        specMap["dyFunc"]     = getDyFunc(m_params.variant);
        specMap["dwidthFunc"] = getWidthFunc(m_params.variant);
        specMap["storeNdx"]   = getStoreNdx(m_params.numWorkgroup);

        testShaderStr = specTemp.specialize(specMap);

        break;
    }
    case TestType::VERIFY_NDX:
    {
        tcu::StringTemplate preTemp(testShaderStr);

        std::map<std::string, std::string> specMap;
        specMap["x"]                = std::to_string(m_params.numWorkgroup.x());
        specMap["y"]                = std::to_string(m_params.numWorkgroup.y());
        specMap["z"]                = std::to_string(m_params.numWorkgroup.z());
        specMap["capability"]       = getDerivativeCapability(m_params.feature);
        specMap["executionMode"]    = getDerivativeExecutionMode(m_params.feature);
        specMap["arrayDeclaration"] = getUintArrayDeclaration();
        specMap["arrayStride"]      = std::to_string(4);
        specMap["dataType"]         = "%uint32";
        specMap["linearNdxMul"]     = getLinearNdxMul(m_params.testType);
        specMap["testLogicCode"] =
            "%gl_SubgroupID_loc           = OpAccessChain %uint32_input_ptr          %gl_SubgroupID\n"
            "%subgroup_val                = OpLoad        %uint32                    %gl_SubgroupID_loc\n"
            "%subgroup_loc                = OpAccessChain %uint32_storage_buffer_ptr %out_x_var      "
            "%c_uint32_0 %${storeNdx}\n"
            "%gl_SubgroupInvocationID_loc = OpAccessChain %uint32_input_ptr          %gl_SubgroupInvocationID\n"
            "%invocation_val              = OpLoad        %uint32                    %gl_SubgroupInvocationID_loc\n"
            "%rem_val                     = OpSRem        %uint32                    %invocation_val %c_uint32_4\n"
            "%invocation_loc              = OpAccessChain %uint32_storage_buffer_ptr %out_y_var    "
            "%c_uint32_0 %${storeNdx}\n";
        specMap["storeCode"] = "            OpStore       %subgroup_loc              %subgroup_val\n"
                               "            OpStore       %invocation_loc            %rem_val\n";

        tcu::StringTemplate specTemp(preTemp.specialize(specMap));
        specMap.clear();
        specMap["storeNdx"] = getStoreNdx(m_params.numWorkgroup);

        testShaderStr = specTemp.specialize(specMap);

        break;
    }
    case TestType::QUAD_OPERATIONS:
    {
        tcu::StringTemplate preTemp(testShaderStr);

        std::map<std::string, std::string> specMap;
        specMap["x"]                = std::to_string(m_params.numWorkgroup.x());
        specMap["y"]                = std::to_string(m_params.numWorkgroup.y());
        specMap["z"]                = std::to_string(m_params.numWorkgroup.z());
        specMap["capability"]       = getDerivativeCapability(m_params.feature);
        specMap["executionMode"]    = getDerivativeExecutionMode(m_params.feature);
        specMap["arrayStride"]      = std::to_string(getDataAlignedSizeInBytes(m_params.dataType));
        specMap["arrayDeclaration"] = getArrayDeclaration(m_params.dataType);
        specMap["dataType"]         = getDataType(m_params.dataType);
        specMap["linearNdxMul"]     = getLinearNdxMul(m_params.testType);
        specMap["testValueCode"]    = getTestValueCode(m_params.feature, DerivativeVariant::NORMAL, m_params.dataType);
        specMap["testLogicCode"]    = "%store_value = ${quadOp} ${dataType} %c_uint32_3 %test_value %${quadNdx}\n";
        specMap["storeCode"] =
            "%out_x_loc = OpAccessChain ${dataType}_storage_buffer_ptr %out_x_var %c_uint32_0 %${storeNdx}\n"
            "             OpStore       %out_x_loc                     %store_value\n";

        tcu::StringTemplate specTemp(preTemp.specialize(specMap));
        specMap.clear();
        specMap["storeNdx"] = getStoreNdx(m_params.numWorkgroup);
        specMap["quadOp"]   = getQuadOpCode(m_params.quadOp);
        specMap["quadNdx"]  = getQuadNdx(m_params.quadNdx);
        specMap["dataType"] = getDataType(m_params.dataType);
        specMap["storeNdx"] = getStoreNdx(m_params.numWorkgroup);

        testShaderStr = specTemp.specialize(specMap);

        break;
    }
    case TestType::LOD_SAMPLE:
    {
        tcu::StringTemplate preTemp(testShaderStr);

        std::map<std::string, std::string> specMap;
        specMap["x"]                = std::to_string(m_params.numWorkgroup.x());
        specMap["y"]                = std::to_string(m_params.numWorkgroup.y());
        specMap["z"]                = std::to_string(m_params.numWorkgroup.z());
        specMap["capability"]       = getDerivativeCapability(m_params.feature);
        specMap["sampleCap"]        = getSampleCapability(m_params.feature);
        specMap["executionMode"]    = getDerivativeExecutionMode(m_params.feature);
        specMap["interface"]        = "%sampled_image_var";
        specMap["dataType"]         = getDataType(DataType::VEC4_FLOAT32);
        specMap["linearNdxMul"]     = getLinearNdxMul(m_params.testType);
        specMap["arrayDeclaration"] = getArrayDeclaration(DataType::VEC4_FLOAT32);
        specMap["arrayStride"]      = std::to_string(getDataAlignedSizeInBytes(DataType::VEC4_FLOAT32));
        specMap["decorations"]      = "OpDecorate       %sampled_image_var DescriptorSet 0\n"
                                      "OpDecorate       %sampled_image_var Binding       4\n";
        specMap["images"]           = "%image         = OpTypeImage        %float32 ${dim} 0 0 0 1 Unknown\n"
                                      "%sampled_image = OpTypeSampledImage %image\n"
                                      "%float32_uniform_constant_ptr       = OpTypePointer UniformConstant %float32\n"
                                      "%sampled_image_uniform_constant_ptr = OpTypePointer UniformConstant %sampled_image\n"
                                      "%sampled_image_var    = OpVariable %sampled_image_uniform_constant_ptr UniformConstant\n";
        specMap["testValueCode"]    = genTexCoords(m_params.feature, m_params.mipLvl);
        specMap["testLogicCode"] =
            "%sampled_image_loc = OpLoad                   %sampled_image %sampled_image_var\n"
            "%store_value       = OpImageSampleImplicitLod %vec4_float32  %sampled_image_loc %test_value\n";
        specMap["storeCode"] =
            "%out_x_loc = OpAccessChain %vec4_float32_storage_buffer_ptr   %out_x_var     %c_uint32_0 "
            "%${storeNdx}\n"
            "             OpStore       %out_x_loc %store_value\n";

        tcu::StringTemplate specTemp(preTemp.specialize(specMap));
        specMap.clear();
        specMap["dim"]      = getImageDim(m_params.feature);
        specMap["storeNdx"] = getStoreNdx(m_params.numWorkgroup);

        testShaderStr = specTemp.specialize(specMap);

        break;
    }
    case TestType::LOD_QUERY:
    {
        tcu::StringTemplate preTemp(testShaderStr);

        std::map<std::string, std::string> specMap;
        specMap["x"]                = std::to_string(m_params.numWorkgroup.x());
        specMap["y"]                = std::to_string(m_params.numWorkgroup.y());
        specMap["z"]                = std::to_string(m_params.numWorkgroup.z());
        specMap["capability"]       = getDerivativeCapability(m_params.feature);
        specMap["sampleCap"]        = getSampleCapability(m_params.feature);
        specMap["queryCap"]         = "OpCapability ImageQuery\n";
        specMap["executionMode"]    = getDerivativeExecutionMode(m_params.feature);
        specMap["interface"]        = "%sampled_image_var";
        specMap["dataType"]         = getDataType(DataType::VEC2_FLOAT32);
        specMap["linearNdxMul"]     = getLinearNdxMul(m_params.testType);
        specMap["arrayDeclaration"] = getArrayDeclaration(DataType::VEC2_FLOAT32);
        specMap["arrayStride"]      = std::to_string(getDataAlignedSizeInBytes(DataType::VEC2_FLOAT32));
        specMap["decorations"]      = "OpDecorate       %sampled_image_var DescriptorSet 0\n"
                                      "OpDecorate       %sampled_image_var Binding       4\n";
        specMap["images"]           = "%image         = OpTypeImage        %float32 ${dim} 0 0 0 1 Unknown\n"
                                      "%sampled_image = OpTypeSampledImage %image\n"
                                      "%float32_uniform_constant_ptr       = OpTypePointer UniformConstant %float32\n"
                                      "%sampled_image_uniform_constant_ptr = OpTypePointer UniformConstant %sampled_image\n"
                                      "%sampled_image_var    = OpVariable %sampled_image_uniform_constant_ptr UniformConstant\n";
        specMap["testValueCode"]    = genTexCoords(m_params.feature, m_params.mipLvl);
        specMap["testLogicCode"] =
            "%sampled_image_loc = OpLoad %sampled_image %sampled_image_var\n"
            "%store_value       = OpImageQueryLod %vec2_float32 %sampled_image_loc %test_value\n";
        specMap["storeCode"] =
            "%out_x_loc         = OpAccessChain %vec2_float32_storage_buffer_ptr   %out_x_var     %c_uint32_0 "
            "%${storeNdx}\n"
            "                     OpStore       %out_x_loc %store_value\n";

        tcu::StringTemplate specTemp(preTemp.specialize(specMap));
        specMap.clear();
        specMap["dim"]      = getImageDim(m_params.feature);
        specMap["storeNdx"] = getStoreNdx(m_params.numWorkgroup);

        testShaderStr = specTemp.specialize(specMap);

        break;
    }
    default:
        break;
    }

    // Create proper programs
    switch (m_params.shaderType)
    {
    case ShaderType::COMPUTE:
    {
        programCollection.spirvAsmSources.add("compute")
            << testShaderStr.c_str() << SpirVAsmBuildOptions(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3);

        break;
    }
    case ShaderType::MESH:
    {
        programCollection.spirvAsmSources.add("mesh")
            << testShaderStr.c_str()
            << SpirVAsmBuildOptions(programCollection.usedVulkanVersion,
                                    SPIRV_VERSION_1_4); // Mesh shaders requires SPIRV 1.4

        programCollection.spirvAsmSources.add("fragment")
            << fragmentShaderStr.c_str()
            << SpirVAsmBuildOptions(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3);

        break;
    }
    case ShaderType::TASK:
    {
        programCollection.spirvAsmSources.add("mesh")
            << meshShaderStr.c_str()
            << SpirVAsmBuildOptions(programCollection.usedVulkanVersion,
                                    SPIRV_VERSION_1_4); // Mesh shaders requires SPIRV 1.4

        programCollection.spirvAsmSources.add("task")
            << testShaderStr.c_str()
            << SpirVAsmBuildOptions(programCollection.usedVulkanVersion,
                                    SPIRV_VERSION_1_4); // Task shaders requires SPIRV 1.4

        programCollection.spirvAsmSources.add("fragment")
            << fragmentShaderStr.c_str()
            << SpirVAsmBuildOptions(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3);

        break;
    }
    default:
        break;
    }
}

TestInstance *ComputeShaderDerivativeCase::createInstance(Context &ctx) const
{
    return new ComputeShaderDerivativeInstance(ctx, m_params);
}

} // namespace

tcu::TestCaseGroup *createComputeShaderDerivativesTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> computeShaderDerivatives(
        new tcu::TestCaseGroup(testCtx, "compute_shader_derivatives"));

    ShaderType shaderTypes[]     = {ShaderType::COMPUTE, ShaderType::MESH, ShaderType::TASK};
    DerivativeVariant variants[] = {DerivativeVariant::NORMAL, DerivativeVariant::FINE, DerivativeVariant::COARSE};
    DataType dataTypes[] = {DataType::FLOAT32, DataType::VEC2_FLOAT32, DataType::VEC3_FLOAT32, DataType::VEC4_FLOAT32};

    for (uint32_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(shaderTypes); ++ndx)
    {
        de::MovePtr<tcu::TestCaseGroup> shaderGroup(new tcu::TestCaseGroup(testCtx, toString(shaderTypes[ndx])));

        // Test for proper derivative value
        {
            de::MovePtr<tcu::TestCaseGroup> derivativeValue(new tcu::TestCaseGroup(testCtx, "derivative_value"));

            for (uint32_t ndy = 0; ndy < DE_LENGTH_OF_ARRAY(variants); ++ndy)
            {
                de::MovePtr<tcu::TestCaseGroup> variantGroup(new tcu::TestCaseGroup(testCtx, toString(variants[ndy])));

                for (uint32_t ndz = 0; ndz < DE_LENGTH_OF_ARRAY(dataTypes); ++ndz)
                {
                    de::MovePtr<tcu::TestCaseGroup> dataTypeGroup(
                        new tcu::TestCaseGroup(testCtx, toString(dataTypes[ndz])));

                    {
                        de::MovePtr<tcu::TestCaseGroup> linear(new tcu::TestCaseGroup(testCtx, "linear"));

                        {
                            ComputeShaderDerivativeTestParams params;
                            params.numWorkgroup = tcu::UVec3(16, 1, 1);
                            params.testType     = TestType::DERIVATIVE_VALUE;
                            params.shaderType   = shaderTypes[ndx];
                            params.variant      = variants[ndy];
                            params.dataType     = dataTypes[ndz];
                            params.feature      = DerivativeFeature::LINEAR;

                            linear->addChild(new ComputeShaderDerivativeCase(testCtx, "16_1_1", params));
                        }

                        {
                            ComputeShaderDerivativeTestParams params;
                            params.numWorkgroup = tcu::UVec3(4, 4, 1);
                            params.testType     = TestType::DERIVATIVE_VALUE;
                            params.shaderType   = shaderTypes[ndx];
                            params.variant      = variants[ndy];
                            params.dataType     = dataTypes[ndz];
                            params.feature      = DerivativeFeature::LINEAR;

                            linear->addChild(new ComputeShaderDerivativeCase(testCtx, "4_4_1", params));
                        }

                        dataTypeGroup->addChild(linear.release());
                    }

                    {
                        de::MovePtr<tcu::TestCaseGroup> quads(new tcu::TestCaseGroup(testCtx, "quads"));

                        ComputeShaderDerivativeTestParams params;
                        params.numWorkgroup = tcu::UVec3(4, 4, 1);
                        params.testType     = TestType::DERIVATIVE_VALUE;
                        params.shaderType   = shaderTypes[ndx];
                        params.variant      = variants[ndy];
                        params.dataType     = dataTypes[ndz];
                        params.feature      = DerivativeFeature::QUADS;

                        quads->addChild(new ComputeShaderDerivativeCase(testCtx, "4_4_1", params));

                        dataTypeGroup->addChild(quads.release());
                    }

                    variantGroup->addChild(dataTypeGroup.release());
                }

                derivativeValue->addChild(variantGroup.release());
            }

            shaderGroup->addChild(derivativeValue.release());
        }

        // Test for proper indices in compute like shaders
        {
            de::MovePtr<tcu::TestCaseGroup> verifyNdx(new tcu::TestCaseGroup(testCtx, "verify_ndx"));

            {
                de::MovePtr<tcu::TestCaseGroup> linear(new tcu::TestCaseGroup(testCtx, "linear"));

                {
                    ComputeShaderDerivativeTestParams params;
                    params.numWorkgroup = tcu::UVec3(128, 1, 1);
                    params.testType     = TestType::VERIFY_NDX;
                    params.shaderType   = shaderTypes[ndx];
                    params.feature      = DerivativeFeature::LINEAR;

                    linear->addChild(new ComputeShaderDerivativeCase(testCtx, "128_1_1", params));
                }

                {
                    ComputeShaderDerivativeTestParams params;
                    params.numWorkgroup = tcu::UVec3(32, 4, 1);
                    params.testType     = TestType::VERIFY_NDX;
                    params.shaderType   = shaderTypes[ndx];
                    params.feature      = DerivativeFeature::LINEAR;

                    linear->addChild(new ComputeShaderDerivativeCase(testCtx, "32_4_1", params));
                }

                verifyNdx->addChild(linear.release());
            }

            {
                de::MovePtr<tcu::TestCaseGroup> quads(new tcu::TestCaseGroup(testCtx, "quads"));

                ComputeShaderDerivativeTestParams params;
                params.numWorkgroup = tcu::UVec3(32, 4, 1);
                params.testType     = TestType::VERIFY_NDX;
                params.shaderType   = shaderTypes[ndx];
                params.feature      = DerivativeFeature::QUADS;

                quads->addChild(new ComputeShaderDerivativeCase(testCtx, "32_4_1", params));

                verifyNdx->addChild(quads.release());
            }

            shaderGroup->addChild(verifyNdx.release());
        }

        // Test for quad operations
        {
            de::MovePtr<tcu::TestCaseGroup> quadOpGroup(new tcu::TestCaseGroup(testCtx, "quad_op"));

            // Broadcasting
            {
                de::MovePtr<tcu::TestCaseGroup> broadcastGroup(
                    new tcu::TestCaseGroup(testCtx, toString(QuadOp::BROADCAST)));

                for (uint32_t ndy = 0; ndy < DE_LENGTH_OF_ARRAY(dataTypes); ++ndy)
                {
                    de::MovePtr<tcu::TestCaseGroup> dataTypeGroup(
                        new tcu::TestCaseGroup(testCtx, toString(dataTypes[ndy])));

                    {
                        de::MovePtr<tcu::TestCaseGroup> linear(new tcu::TestCaseGroup(testCtx, "linear"));
                        de::MovePtr<tcu::TestCaseGroup> linearWrkGrp(new tcu::TestCaseGroup(testCtx, "16_1_1"));
                        de::MovePtr<tcu::TestCaseGroup> quadWrkGrp(new tcu::TestCaseGroup(testCtx, "4_4_1"));

                        for (uint32_t ndz = 0; ndz < 4; ++ndz)
                        {
                            {
                                ComputeShaderDerivativeTestParams params;
                                params.numWorkgroup = tcu::UVec3(16, 1, 1);
                                params.testType     = TestType::QUAD_OPERATIONS;
                                params.shaderType   = shaderTypes[ndx];
                                params.dataType     = dataTypes[ndy];
                                params.quadOp       = QuadOp::BROADCAST;
                                params.quadNdx      = ndz;
                                params.feature      = DerivativeFeature::LINEAR;

                                linearWrkGrp->addChild(new ComputeShaderDerivativeCase(testCtx, toString(ndz), params));
                            }

                            {
                                ComputeShaderDerivativeTestParams params;
                                params.numWorkgroup = tcu::UVec3(4, 4, 1);
                                params.testType     = TestType::QUAD_OPERATIONS;
                                params.shaderType   = shaderTypes[ndx];
                                params.dataType     = dataTypes[ndy];
                                params.quadOp       = QuadOp::BROADCAST;
                                params.quadNdx      = ndz;
                                params.feature      = DerivativeFeature::LINEAR;

                                quadWrkGrp->addChild(new ComputeShaderDerivativeCase(testCtx, toString(ndz), params));
                            }
                        }

                        linear->addChild(linearWrkGrp.release());
                        linear->addChild(quadWrkGrp.release());
                        dataTypeGroup->addChild(linear.release());
                    }

                    {
                        de::MovePtr<tcu::TestCaseGroup> quads(new tcu::TestCaseGroup(testCtx, "quads"));
                        de::MovePtr<tcu::TestCaseGroup> quadWrkGrp(new tcu::TestCaseGroup(testCtx, "4_4_1"));

                        for (uint32_t ndz = 0; ndz < 4; ++ndz)
                        {
                            ComputeShaderDerivativeTestParams params;
                            params.numWorkgroup = tcu::UVec3(4, 4, 1);
                            params.testType     = TestType::QUAD_OPERATIONS;
                            params.shaderType   = shaderTypes[ndx];
                            params.dataType     = dataTypes[ndy];
                            params.quadOp       = QuadOp::BROADCAST;
                            params.quadNdx      = ndz;
                            params.feature      = DerivativeFeature::QUADS;

                            quadWrkGrp->addChild(new ComputeShaderDerivativeCase(testCtx, toString(ndz), params));
                        }

                        quads->addChild(quadWrkGrp.release());
                        dataTypeGroup->addChild(quads.release());
                    }

                    broadcastGroup->addChild(dataTypeGroup.release());
                }

                quadOpGroup->addChild(broadcastGroup.release());
            }

            // Swapping
            {
                de::MovePtr<tcu::TestCaseGroup> swapGroup(new tcu::TestCaseGroup(testCtx, toString(QuadOp::SWAP)));

                for (uint32_t ndy = 0; ndy < DE_LENGTH_OF_ARRAY(dataTypes); ++ndy)
                {
                    de::MovePtr<tcu::TestCaseGroup> dataTypeGroup(
                        new tcu::TestCaseGroup(testCtx, toString(dataTypes[ndy])));

                    {
                        de::MovePtr<tcu::TestCaseGroup> linear(new tcu::TestCaseGroup(testCtx, "linear"));
                        de::MovePtr<tcu::TestCaseGroup> linearWrkGrp(new tcu::TestCaseGroup(testCtx, "16_1_1"));
                        de::MovePtr<tcu::TestCaseGroup> quadWrkGrp(new tcu::TestCaseGroup(testCtx, "4_4_1"));

                        for (uint32_t ndz = 0; ndz < 3; ++ndz)
                        {
                            std::string testName = getSwapTestName(ndz);

                            {
                                ComputeShaderDerivativeTestParams params;
                                params.numWorkgroup = tcu::UVec3(16, 1, 1);
                                params.testType     = TestType::QUAD_OPERATIONS;
                                params.shaderType   = shaderTypes[ndx];
                                params.dataType     = dataTypes[ndy];
                                params.quadOp       = QuadOp::SWAP;
                                params.quadNdx      = ndz;
                                params.feature      = DerivativeFeature::LINEAR;

                                linearWrkGrp->addChild(
                                    new ComputeShaderDerivativeCase(testCtx, testName.c_str(), params));
                            }

                            {
                                ComputeShaderDerivativeTestParams params;
                                params.numWorkgroup = tcu::UVec3(4, 4, 1);
                                params.testType     = TestType::QUAD_OPERATIONS;
                                params.shaderType   = shaderTypes[ndx];
                                params.dataType     = dataTypes[ndy];
                                params.quadOp       = QuadOp::SWAP;
                                params.quadNdx      = ndz;
                                params.feature      = DerivativeFeature::LINEAR;

                                quadWrkGrp->addChild(
                                    new ComputeShaderDerivativeCase(testCtx, testName.c_str(), params));
                            }
                        }

                        linear->addChild(linearWrkGrp.release());
                        linear->addChild(quadWrkGrp.release());
                        dataTypeGroup->addChild(linear.release());
                    }

                    {
                        de::MovePtr<tcu::TestCaseGroup> quads(new tcu::TestCaseGroup(testCtx, "quads"));
                        de::MovePtr<tcu::TestCaseGroup> quadWrkGrp(new tcu::TestCaseGroup(testCtx, "4_4_1"));

                        for (uint32_t ndz = 0; ndz < 3; ++ndz)
                        {
                            std::string testName = getSwapTestName(ndz);

                            ComputeShaderDerivativeTestParams params;
                            params.numWorkgroup = tcu::UVec3(4, 4, 1);
                            params.testType     = TestType::QUAD_OPERATIONS;
                            params.shaderType   = shaderTypes[ndx];
                            params.dataType     = dataTypes[ndy];
                            params.quadOp       = QuadOp::SWAP;
                            params.quadNdx      = ndz;
                            params.feature      = DerivativeFeature::QUADS;

                            quadWrkGrp->addChild(new ComputeShaderDerivativeCase(testCtx, testName.c_str(), params));
                        }

                        quads->addChild(quadWrkGrp.release());
                        dataTypeGroup->addChild(quads.release());
                    }

                    swapGroup->addChild(dataTypeGroup.release());
                }

                quadOpGroup->addChild(swapGroup.release());
            }

            shaderGroup->addChild(quadOpGroup.release());
        }

        // Test for LOD operations
        {
            de::MovePtr<tcu::TestCaseGroup> lodOps(new tcu::TestCaseGroup(testCtx, "lod_op"));

            // Sampling
            {
                de::MovePtr<tcu::TestCaseGroup> sampling(new tcu::TestCaseGroup(testCtx, "sample"));

                {
                    de::MovePtr<tcu::TestCaseGroup> linear(new tcu::TestCaseGroup(testCtx, "linear"));
                    de::MovePtr<tcu::TestCaseGroup> linearWrkGrp(new tcu::TestCaseGroup(testCtx, "16_1_1"));
                    de::MovePtr<tcu::TestCaseGroup> quadWrkGrp(new tcu::TestCaseGroup(testCtx, "4_4_1"));

                    for (uint32_t ndz = 0; ndz < 2; ++ndz)
                    {
                        std::string testName = getMipTestName(ndz);

                        {
                            ComputeShaderDerivativeTestParams params;
                            params.numWorkgroup = tcu::UVec3(16, 1, 1);
                            params.testType     = TestType::LOD_SAMPLE;
                            params.shaderType   = shaderTypes[ndx];
                            params.feature      = DerivativeFeature::LINEAR;
                            params.dataType     = DataType::VEC4_FLOAT32;
                            params.mipLvl       = ndz;

                            linearWrkGrp->addChild(new ComputeShaderDerivativeCase(testCtx, testName.c_str(), params));
                        }

                        {
                            ComputeShaderDerivativeTestParams params;
                            params.numWorkgroup = tcu::UVec3(4, 4, 1);
                            params.testType     = TestType::LOD_SAMPLE;
                            params.shaderType   = shaderTypes[ndx];
                            params.feature      = DerivativeFeature::LINEAR;
                            params.dataType     = DataType::VEC4_FLOAT32;
                            params.mipLvl       = ndz;

                            quadWrkGrp->addChild(new ComputeShaderDerivativeCase(testCtx, testName.c_str(), params));
                        }
                    }

                    linear->addChild(linearWrkGrp.release());
                    linear->addChild(quadWrkGrp.release());
                    sampling->addChild(linear.release());
                }

                {
                    de::MovePtr<tcu::TestCaseGroup> quads(new tcu::TestCaseGroup(testCtx, "quads"));
                    de::MovePtr<tcu::TestCaseGroup> quadWrkGrp(new tcu::TestCaseGroup(testCtx, "4_4_1"));

                    for (uint32_t ndz = 0; ndz < 2; ++ndz)
                    {
                        std::string testName = getMipTestName(ndz);

                        ComputeShaderDerivativeTestParams params;
                        params.numWorkgroup = tcu::UVec3(4, 4, 1);
                        params.testType     = TestType::LOD_SAMPLE;
                        params.shaderType   = shaderTypes[ndx];
                        params.feature      = DerivativeFeature::QUADS;
                        params.dataType     = DataType::VEC4_FLOAT32;
                        params.mipLvl       = ndz;

                        quadWrkGrp->addChild(new ComputeShaderDerivativeCase(testCtx, testName.c_str(), params));
                    }

                    quads->addChild(quadWrkGrp.release());
                    sampling->addChild(quads.release());
                }

                lodOps->addChild(sampling.release());
            }

            // Querying
            {
                de::MovePtr<tcu::TestCaseGroup> querying(new tcu::TestCaseGroup(testCtx, "query"));

                {
                    de::MovePtr<tcu::TestCaseGroup> linear(new tcu::TestCaseGroup(testCtx, "linear"));
                    de::MovePtr<tcu::TestCaseGroup> linearWrkGrp(new tcu::TestCaseGroup(testCtx, "16_1_1"));
                    de::MovePtr<tcu::TestCaseGroup> quadWrkGrp(new tcu::TestCaseGroup(testCtx, "4_4_1"));

                    for (uint32_t ndz = 0; ndz < 2; ++ndz)
                    {
                        std::string testName = getMipTestName(ndz);

                        {
                            ComputeShaderDerivativeTestParams params;
                            params.numWorkgroup = tcu::UVec3(16, 1, 1);
                            params.testType     = TestType::LOD_QUERY;
                            params.shaderType   = shaderTypes[ndx];
                            params.feature      = DerivativeFeature::LINEAR;
                            params.dataType     = DataType::VEC2_FLOAT32;
                            params.mipLvl       = ndz;

                            linearWrkGrp->addChild(new ComputeShaderDerivativeCase(testCtx, testName.c_str(), params));
                        }

                        {
                            ComputeShaderDerivativeTestParams params;
                            params.numWorkgroup = tcu::UVec3(4, 4, 1);
                            params.testType     = TestType::LOD_QUERY;
                            params.shaderType   = shaderTypes[ndx];
                            params.feature      = DerivativeFeature::LINEAR;
                            params.dataType     = DataType::VEC2_FLOAT32;
                            params.mipLvl       = ndz;

                            quadWrkGrp->addChild(new ComputeShaderDerivativeCase(testCtx, testName.c_str(), params));
                        }
                    }

                    linear->addChild(linearWrkGrp.release());
                    linear->addChild(quadWrkGrp.release());
                    querying->addChild(linear.release());
                }

                {
                    de::MovePtr<tcu::TestCaseGroup> quads(new tcu::TestCaseGroup(testCtx, "quads"));
                    de::MovePtr<tcu::TestCaseGroup> quadWrkGrp(new tcu::TestCaseGroup(testCtx, "4_4_1"));

                    for (uint32_t ndz = 0; ndz < 2; ++ndz)
                    {
                        std::string testName = getMipTestName(ndz);

                        ComputeShaderDerivativeTestParams params;
                        params.numWorkgroup = tcu::UVec3(4, 4, 1);
                        params.testType     = TestType::LOD_QUERY;
                        params.shaderType   = shaderTypes[ndx];
                        params.feature      = DerivativeFeature::QUADS;
                        params.dataType     = DataType::VEC2_FLOAT32;
                        params.mipLvl       = ndz;

                        quadWrkGrp->addChild(new ComputeShaderDerivativeCase(testCtx, testName.c_str(), params));
                    }

                    quads->addChild(quadWrkGrp.release());
                    querying->addChild(quads.release());
                }

                lodOps->addChild(querying.release());
            }

            shaderGroup->addChild(lodOps.release());
        }

        computeShaderDerivatives->addChild(shaderGroup.release());
    }

    return computeShaderDerivatives.release();
}

} // namespace SpirVAssembly
} // namespace vkt
