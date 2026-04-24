/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 Valve Corporation.
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \brief SPIR-V tests for VK_KHR_maintenance9 bitwise ops vectorization.
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmMaint9VectorizationTests.hpp"
#include "vktTestCase.hpp"

#include <vkBufferWithMemory.hpp>
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "deSTLUtil.hpp"
#include "deUniquePtr.hpp"
#include "deRandom.hpp"

#include <vector>
#include <sstream>
#include <string>
#include <utility>
#include <algorithm>
#include <memory>
#include <bitset>

namespace vkt
{
namespace SpirVAssembly
{

namespace
{

using namespace vk;

enum class BitOp
{
    COUNT = 0, // Operands: 0 -> result, 1 -> base
    REVERSE,   // Operands: 0 -> result, 1 -> base
    INSERT,    // Operands: 0 -> result, 1 -> base, 2 -> insert, 3 -> offset, 4 -> count
    S_EXTRACT, // Operands: 0 -> result, 1 -> base, 2 -> offset, 3 -> count
    U_EXTRACT, // Operands: 0 -> result, 1 -> base, 2 -> offset, 3 -> count
};

std::string getSpvOpName(BitOp op)
{
    if (op == BitOp::COUNT)
        return "OpBitCount";
    if (op == BitOp::REVERSE)
        return "OpBitReverse";
    if (op == BitOp::INSERT)
        return "OpBitFieldInsert";
    if (op == BitOp::S_EXTRACT)
        return "OpBitFieldSExtract";
    if (op == BitOp::U_EXTRACT)
        return "OpBitFieldUExtract";

    DE_ASSERT(false);
    return "";
}

uint32_t getOperandCount(BitOp bitOp)
{
    if (bitOp == BitOp::COUNT || bitOp == BitOp::REVERSE)
        return 2u;
    else if (bitOp == BitOp::INSERT)
        return 5u;
    else if (bitOp == BitOp::S_EXTRACT || bitOp == BitOp::U_EXTRACT)
        return 4u;

    DE_ASSERT(false);
    return 0u;
}

enum class BitSize
{
    INVALID = 0,
    BIT8    = 8,
    BIT16   = 16,
    BIT32   = 32,
    BIT64   = 64,
};

struct OperandType
{
    bool isVector;
    bool isSigned;
    BitSize bitSize;
    std::string name; // Helps generate code but does not participate in operator==.

    OperandType(bool isVector_, bool isSigned_, BitSize bitSize_, std::string name_)
        : isVector(isVector_)
        , isSigned(isSigned_)
        , bitSize(bitSize_)
        , name(std::move(name_))
    {
        DE_ASSERT(!name.empty());
    }

    bool operator==(const OperandType &other) const
    {
        return (isVector == other.isVector && isSigned == other.isSigned && bitSize == other.bitSize);
    }

    std::string getSpvAsmTypePrefix() const
    {
        return std::string("%") + (isSigned ? "i" : "u") + std::to_string(static_cast<int>(bitSize)) +
               (isVector ? "vec4" : "scalar");
    }

    uint32_t getDataSizeBytes() const
    {
        return static_cast<uint32_t>((static_cast<int>(bitSize) / 8) * (isVector ? 4 : 1));
    }

    uint32_t getSpvAlignment() const
    {
        // Alignments larger than 64 bits are not used.
        return std::min(getDataSizeBytes(), 16u);
    }
};

using OperandList = std::vector<OperandType>;

std::string getOperandListTestName(const OperandList &opList)
{
    std::string name;
    for (const auto &operand : opList)
        name += (name.empty() ? "" : "-") + operand.name + "_" + (operand.isVector ? "v" : "s") +
                std::to_string(static_cast<int>(operand.bitSize)) + (operand.isSigned ? "i" : "u");
    return name;
}

struct TestParams
{
    BitOp bitOp;
    OperandList operandList; // Note: the first operand is always considered the out one (the result).

    TestParams(BitOp bitOp_, OperandList operandList_) : bitOp(bitOp_), operandList(std::move(operandList_))
    {
        uint32_t expectedOperandCount = getOperandCount(bitOp);
        DE_ASSERT(de::sizeU32(operandList) == expectedOperandCount);
        DE_UNREF(expectedOperandCount);

        if (bitOp == BitOp::COUNT)
        {
            DE_ASSERT(operandList.at(0).isVector == operandList.at(1).isVector);
        }
        else if (bitOp == BitOp::REVERSE)
        {
            DE_ASSERT(operandList.at(0) == operandList.at(1));
        }
        else if (bitOp == BitOp::INSERT)
        {
            DE_ASSERT(operandList.at(0) == operandList.at(1));
            DE_ASSERT(operandList.at(0) == operandList.at(2));
            DE_ASSERT(!operandList.at(3).isVector); // Offset must be scalar.
            DE_ASSERT(!operandList.at(4).isVector); // Count must be a scalar.
        }
        else if (bitOp == BitOp::S_EXTRACT || bitOp == BitOp::U_EXTRACT)
        {
            DE_ASSERT(operandList.at(0) == operandList.at(1));
            DE_ASSERT(!operandList.at(2).isVector); // Offset must be scalar.
            DE_ASSERT(!operandList.at(3).isVector); // Count must be a scalar.
        }
        else
            DE_ASSERT(false);
    }

protected:
    bool useBitSize(BitSize bitSize) const
    {
        for (const auto &operand : operandList)
        {
            if (operand.bitSize == bitSize)
                return true;
        }
        return false;
    }

public:
    bool use64Bit() const
    {
        return useBitSize(BitSize::BIT64);
    }
    bool use32Bit() const
    {
        return useBitSize(BitSize::BIT32);
    }
    bool use16Bit() const
    {
        return useBitSize(BitSize::BIT16);
    }
    bool use8Bit() const
    {
        return useBitSize(BitSize::BIT8);
    }

    bool requiresMaint9() const
    {
        return operandList[1].bitSize != BitSize::BIT32;
    }

    uint32_t getRandomSeed() const
    {
        uint32_t seed = (static_cast<uint32_t>(bitOp) << 24);

        for (size_t i = 0; i < operandList.size(); ++i)
        {
            const auto &operand = operandList.at(i);
            // Note we do not use the signed boolean, so similar tests would have the same pseudorandom numbers.
            const auto opSeed =
                (((static_cast<uint32_t>(operand.isVector) << 8u) | static_cast<uint32_t>(operand.bitSize)) << i);
            seed += opSeed;
        }

        return seed;
    }

    uint32_t getWorkGroupSize() const
    {
        return 64u;
    }
};

class M9V_Instance : public vkt::TestInstance
{
public:
    M9V_Instance(Context &context, const TestParams &params) : vkt::TestInstance(context), m_params(params)
    {
    }
    virtual ~M9V_Instance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const TestParams m_params;
};

class M9V_Case : public vkt::TestCase
{
public:
    M9V_Case(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~M9V_Case(void) = default;

    void checkSupport(Context &context) const;
    void initPrograms(vk::SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const
    {
        return new M9V_Instance(context, m_params);
    }

protected:
    const TestParams m_params;
};

void M9V_Case::checkSupport(Context &context) const
{
    // We use some SPIR-V 1.6 features, so we depend on Vulkan 1.3.
    if (context.getUsedApiVersion() < VK_API_VERSION_1_3)
        TCU_THROW(NotSupportedError, "Vulkan 1.3 required");

    // Requires maintenance 9 for the bitwise ops.
    if (m_params.requiresMaint9())
        context.requireDeviceFunctionality("VK_KHR_maintenance9");

    const auto &vk12Features = context.getDeviceVulkan12Features();
    const auto &vk11Features = context.getDeviceVulkan11Features();

    // We pass buffers using BDA to prevent scalarization.
    if (!vk12Features.bufferDeviceAddress)
        TCU_THROW(NotSupportedError, "bufferDeviceAddress not supported");

    // We use the scalar block layout for most tests. Maybe some of them do not need it, but this is widely supported.
    if (!vk12Features.scalarBlockLayout)
        TCU_THROW(NotSupportedError, "scalarBlockLayout not supported");

    // Base shader types support checks.
    if (m_params.use64Bit())
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_INT64);

    if (m_params.use16Bit())
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_INT16);

    if (m_params.use8Bit() && !vk12Features.shaderInt8)
        TCU_THROW(NotSupportedError, "shaderInt8 not supported");

    // Storage access support checks.
    if (m_params.use16Bit() && !vk11Features.storageBuffer16BitAccess)
        TCU_THROW(NotSupportedError, "storageBuffer16BitAccess not supported");

    if (m_params.use8Bit() && !vk12Features.storageBuffer8BitAccess)
        TCU_THROW(NotSupportedError, "storageBuffer8BitAccess not supported");
}

void M9V_Case::initPrograms(vk::SourceCollections &programCollection) const
{
    const auto use8Bit  = m_params.use8Bit();
    const auto use16Bit = m_params.use16Bit();
    const auto use32Bit = m_params.use32Bit();
    const auto use64Bit = m_params.use64Bit();

    std::ostringstream comp;
    comp << "                                  OpCapability Shader\n"
         << "\n"
         << (use64Bit ? "                                  OpCapability Int64\n" : "") << "\n"
         << (use16Bit ? "                                  OpCapability Int16\n" : "")
         << (use16Bit ? "                                  OpCapability StorageBuffer16BitAccess\n" : "") << "\n"
         << (use8Bit ? "                                  OpCapability Int8\n" : "")
         << (use8Bit ? "                                  OpCapability StorageBuffer8BitAccess\n" : "") << "\n"
         << "                                  ; Allows using buffer device addresses\n"
         << "                                  OpCapability PhysicalStorageBufferAddresses\n"
         << "\n"
         << (use16Bit ? "                                  OpExtension \"SPV_KHR_16bit_storage\"\n" : "") << "\n"
         << (use8Bit ? "                                  OpExtension \"SPV_KHR_8bit_storage\"\n" : "") << "\n"
         << "                                  OpExtension \"SPV_KHR_physical_storage_buffer\"\n"
         << "                                  OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
         << "\n"
         << "                   %std450insts = OpExtInstImport \"GLSL.std.450\"\n"
         << "                                  OpMemoryModel PhysicalStorageBuffer64 GLSL450\n"
         << "                                  OpEntryPoint GLCompute %main \"main\" %gl_LocalInvocationIndex "
            "%references\n"
         << "                                  OpExecutionMode %main LocalSize " << m_params.getWorkGroupSize()
         << " 1 1\n"
         << "                                  OpSource GLSL 460\n"
         << "                                  OpDecorate %gl_LocalInvocationIndex BuiltIn LocalInvocationIndex\n"
         << "                                  OpDecorate %ReferencesBlock Block\n"
         << "\n";

    // We need to make this list as long as the number of members in the references block structure.
    // The references block structure includes a buffer reference for each of the operands.
    {
        const size_t ptrSize = 8;
        for (size_t i = 0; i < m_params.operandList.size(); ++i)
            comp << "                                  OpMemberDecorate %ReferencesBlock " << i << " Offset "
                 << (i * ptrSize) << "\n";
    }

    comp << "\n"
         << (use8Bit ? "                                  OpDecorate %u8scalar_array ArrayStride 1\n" : "")
         << (use8Bit ? "                                  OpDecorate %u8scalar_array_struct Block\n" : "")
         << (use8Bit ? "                                  OpMemberDecorate %u8scalar_array_struct 0 Offset 0\n" : "")
         << (use8Bit ? "                                  OpDecorate %i8scalar_array ArrayStride 1\n" : "")
         << (use8Bit ? "                                  OpDecorate %i8scalar_array_struct Block\n" : "")
         << (use8Bit ? "                                  OpMemberDecorate %i8scalar_array_struct 0 Offset 0\n" : "")
         << "\n"
         << (use16Bit ? "                                  OpDecorate %u16scalar_array ArrayStride 2\n" : "")
         << (use16Bit ? "                                  OpDecorate %u16scalar_array_struct Block\n" : "")
         << (use16Bit ? "                                  OpMemberDecorate %u16scalar_array_struct 0 Offset 0\n" : "")
         << (use16Bit ? "                                  OpDecorate %i16scalar_array ArrayStride 2\n" : "")
         << (use16Bit ? "                                  OpDecorate %i16scalar_array_struct Block\n" : "")
         << (use16Bit ? "                                  OpMemberDecorate %i16scalar_array_struct 0 Offset 0\n" : "")
         << "\n"
         << (use32Bit ? "                                  OpDecorate %u32scalar_array ArrayStride 4\n" : "")
         << (use32Bit ? "                                  OpDecorate %u32scalar_array_struct Block\n" : "")
         << (use32Bit ? "                                  OpMemberDecorate %u32scalar_array_struct 0 Offset 0\n" : "")
         << (use32Bit ? "                                  OpDecorate %i32scalar_array ArrayStride 4\n" : "")
         << (use32Bit ? "                                  OpDecorate %i32scalar_array_struct Block\n" : "")
         << (use32Bit ? "                                  OpMemberDecorate %i32scalar_array_struct 0 Offset 0\n" : "")
         << "\n"
         << (use64Bit ? "                                  OpDecorate %u64scalar_array ArrayStride 8\n" : "")
         << (use64Bit ? "                                  OpDecorate %u64scalar_array_struct Block\n" : "")
         << (use64Bit ? "                                  OpMemberDecorate %u64scalar_array_struct 0 Offset 0\n" : "")
         << (use64Bit ? "                                  OpDecorate %i64scalar_array ArrayStride 8\n" : "")
         << (use64Bit ? "                                  OpDecorate %i64scalar_array_struct Block\n" : "")
         << (use64Bit ? "                                  OpMemberDecorate %i64scalar_array_struct 0 Offset 0\n" : "")
         << "\n"
         << (use8Bit ? "                                  OpDecorate %u8vec4_array ArrayStride 4\n" : "")
         << (use8Bit ? "                                  OpDecorate %u8vec4_array_struct Block\n" : "")
         << (use8Bit ? "                                  OpMemberDecorate %u8vec4_array_struct 0 Offset 0\n" : "")
         << (use8Bit ? "                                  OpDecorate %i8vec4_array ArrayStride 4\n" : "")
         << (use8Bit ? "                                  OpDecorate %i8vec4_array_struct Block\n" : "")
         << (use8Bit ? "                                  OpMemberDecorate %i8vec4_array_struct 0 Offset 0\n" : "")
         << "\n"
         << (use16Bit ? "                                  OpDecorate %u16vec4_array ArrayStride 8\n" : "")
         << (use16Bit ? "                                  OpDecorate %u16vec4_array_struct Block\n" : "")
         << (use16Bit ? "                                  OpMemberDecorate %u16vec4_array_struct 0 Offset 0\n" : "")
         << (use16Bit ? "                                  OpDecorate %i16vec4_array ArrayStride 8\n" : "")
         << (use16Bit ? "                                  OpDecorate %i16vec4_array_struct Block\n" : "")
         << (use16Bit ? "                                  OpMemberDecorate %i16vec4_array_struct 0 Offset 0\n" : "")
         << "\n"
         << (use32Bit ? "                                  OpDecorate %u32vec4_array ArrayStride 16\n" : "")
         << (use32Bit ? "                                  OpDecorate %u32vec4_array_struct Block\n" : "")
         << (use32Bit ? "                                  OpMemberDecorate %u32vec4_array_struct 0 Offset 0\n" : "")
         << (use32Bit ? "                                  OpDecorate %i32vec4_array ArrayStride 16\n" : "")
         << (use32Bit ? "                                  OpDecorate %i32vec4_array_struct Block\n" : "")
         << (use32Bit ? "                                  OpMemberDecorate %i32vec4_array_struct 0 Offset 0\n" : "")
         << "\n"
         << (use64Bit ? "                                  OpDecorate %u64vec4_array ArrayStride 32\n" : "")
         << (use64Bit ? "                                  OpDecorate %u64vec4_array_struct Block\n" : "")
         << (use64Bit ? "                                  OpMemberDecorate %u64vec4_array_struct 0 Offset 0\n" : "")
         << (use64Bit ? "                                  OpDecorate %i64vec4_array ArrayStride 32\n" : "")
         << (use64Bit ? "                                  OpDecorate %i64vec4_array_struct Block\n" : "")
         << (use64Bit ? "                                  OpMemberDecorate %i64vec4_array_struct 0 Offset 0\n" : "")
         << "\n"
         << "                                  OpDecorate %references Binding 0\n"
         << "                                  OpDecorate %references DescriptorSet 0\n"
         << "\n"
         << "                          %void = OpTypeVoid\n"
         << "                     %void_func = OpTypeFunction %void\n"
         << "\n"
         << "                                  ; Scalar types\n"
         << (use8Bit ? "                      %u8scalar = OpTypeInt 8 0\n" : "")
         << (use8Bit ? "                      %i8scalar = OpTypeInt 8 1\n" : "")
         << (use16Bit ? "                     %u16scalar = OpTypeInt 16 0\n" : "")
         << (use16Bit ? "                     %i16scalar = OpTypeInt 16 1\n" : "")
         << "                     %u32scalar = OpTypeInt 32 0\n"
         << "                     %i32scalar = OpTypeInt 32 1\n"
         << (use64Bit ? "                     %u64scalar = OpTypeInt 64 0\n" : "") << "\n"
         << (use64Bit ? "                     %i64scalar = OpTypeInt 64 1\n" : "") << "\n"
         << "                                  ; Vector types\n"
         << (use8Bit ? "                        %u8vec4 = OpTypeVector %u8scalar 4\n" : "")
         << (use16Bit ? "                       %u16vec4 = OpTypeVector %u16scalar 4\n" : "")
         << (use32Bit ? "                       %u32vec4 = OpTypeVector %u32scalar 4\n" : "")
         << (use64Bit ? "                       %u64vec4 = OpTypeVector %u64scalar 4\n" : "") << "\n"
         << (use8Bit ? "                        %i8vec4 = OpTypeVector %i8scalar 4\n" : "")
         << (use16Bit ? "                       %i16vec4 = OpTypeVector %i16scalar 4\n" : "")
         << (use32Bit ? "                       %i32vec4 = OpTypeVector %i32scalar 4\n" : "")
         << (use64Bit ? "                       %i64vec4 = OpTypeVector %i64scalar 4\n" : "") << "\n"
         << "                                  ; Scalar array types\n"
         << (use8Bit ? "                %u8scalar_array = OpTypeRuntimeArray %u8scalar\n" : "")
         << (use16Bit ? "               %u16scalar_array = OpTypeRuntimeArray %u16scalar\n" : "")
         << (use32Bit ? "               %u32scalar_array = OpTypeRuntimeArray %u32scalar\n" : "")
         << (use64Bit ? "               %u64scalar_array = OpTypeRuntimeArray %u64scalar\n" : "") << "\n"
         << (use8Bit ? "                %i8scalar_array = OpTypeRuntimeArray %i8scalar\n" : "")
         << (use16Bit ? "               %i16scalar_array = OpTypeRuntimeArray %i16scalar\n" : "")
         << (use32Bit ? "               %i32scalar_array = OpTypeRuntimeArray %i32scalar\n" : "")
         << (use64Bit ? "               %i64scalar_array = OpTypeRuntimeArray %i64scalar\n" : "") << "\n"
         << "                                  ; Vector array types\n"
         << (use8Bit ? "                  %u8vec4_array = OpTypeRuntimeArray %u8vec4\n" : "")
         << (use16Bit ? "                 %u16vec4_array = OpTypeRuntimeArray %u16vec4\n" : "")
         << (use32Bit ? "                 %u32vec4_array = OpTypeRuntimeArray %u32vec4\n" : "")
         << (use64Bit ? "                 %u64vec4_array = OpTypeRuntimeArray %u64vec4\n" : "") << "\n"
         << (use8Bit ? "                  %i8vec4_array = OpTypeRuntimeArray %i8vec4\n" : "")
         << (use16Bit ? "                 %i16vec4_array = OpTypeRuntimeArray %i16vec4\n" : "")
         << (use32Bit ? "                 %i32vec4_array = OpTypeRuntimeArray %i32vec4\n" : "")
         << (use64Bit ? "                 %i64vec4_array = OpTypeRuntimeArray %i64vec4\n" : "") << "\n"
         << "                                  ; Structures containing arrays of scalars\n"
         << (use8Bit ? "         %u8scalar_array_struct = OpTypeStruct %u8scalar_array\n" : "")
         << (use16Bit ? "        %u16scalar_array_struct = OpTypeStruct %u16scalar_array\n" : "")
         << (use32Bit ? "        %u32scalar_array_struct = OpTypeStruct %u32scalar_array\n" : "")
         << (use64Bit ? "        %u64scalar_array_struct = OpTypeStruct %u64scalar_array\n" : "") << "\n"
         << (use8Bit ? "         %i8scalar_array_struct = OpTypeStruct %i8scalar_array\n" : "")
         << (use16Bit ? "        %i16scalar_array_struct = OpTypeStruct %i16scalar_array\n" : "")
         << (use32Bit ? "        %i32scalar_array_struct = OpTypeStruct %i32scalar_array\n" : "")
         << (use64Bit ? "        %i64scalar_array_struct = OpTypeStruct %i64scalar_array\n" : "") << "\n"
         << "                                  ; Structures containing arrays of vectors\n"
         << (use8Bit ? "           %u8vec4_array_struct = OpTypeStruct %u8vec4_array\n" : "")
         << (use16Bit ? "          %u16vec4_array_struct = OpTypeStruct %u16vec4_array\n" : "")
         << (use32Bit ? "          %u32vec4_array_struct = OpTypeStruct %u32vec4_array\n" : "")
         << (use64Bit ? "          %u64vec4_array_struct = OpTypeStruct %u64vec4_array\n" : "") << "\n"
         << (use8Bit ? "           %i8vec4_array_struct = OpTypeStruct %i8vec4_array\n" : "")
         << (use16Bit ? "          %i16vec4_array_struct = OpTypeStruct %i16vec4_array\n" : "")
         << (use32Bit ? "          %i32vec4_array_struct = OpTypeStruct %i32vec4_array\n" : "")
         << (use64Bit ? "          %i64vec4_array_struct = OpTypeStruct %i64vec4_array\n" : "") << "\n"
         << "                                  ; Pointers to all those structures, as stored in the references buffer\n"
         << (use8Bit ? "                                  OpTypeForwardPointer %u8scalar_array_struct_ptr "
                       "PhysicalStorageBuffer\n" :
                       "")
         << (use16Bit ? "                                  OpTypeForwardPointer %u16scalar_array_struct_ptr "
                        "PhysicalStorageBuffer\n" :
                        "")
         << (use32Bit ? "                                  OpTypeForwardPointer %u32scalar_array_struct_ptr "
                        "PhysicalStorageBuffer\n" :
                        "")
         << (use64Bit ? "                                  OpTypeForwardPointer %u64scalar_array_struct_ptr "
                        "PhysicalStorageBuffer\n" :
                        "")
         << (use8Bit ? "                                  OpTypeForwardPointer %u8vec4_array_struct_ptr "
                       "PhysicalStorageBuffer\n" :
                       "")
         << (use16Bit ? "                                  OpTypeForwardPointer %u16vec4_array_struct_ptr "
                        "PhysicalStorageBuffer\n" :
                        "")
         << (use32Bit ? "                                  OpTypeForwardPointer %u32vec4_array_struct_ptr "
                        "PhysicalStorageBuffer\n" :
                        "")
         << (use64Bit ? "                                  OpTypeForwardPointer %u64vec4_array_struct_ptr "
                        "PhysicalStorageBuffer\n" :
                        "")
         << (use8Bit ? "                                  OpTypeForwardPointer %i8scalar_array_struct_ptr "
                       "PhysicalStorageBuffer\n" :
                       "")
         << (use16Bit ? "                                  OpTypeForwardPointer %i16scalar_array_struct_ptr "
                        "PhysicalStorageBuffer\n" :
                        "")
         << (use32Bit ? "                                  OpTypeForwardPointer %i32scalar_array_struct_ptr "
                        "PhysicalStorageBuffer\n" :
                        "")
         << (use64Bit ? "                                  OpTypeForwardPointer %i64scalar_array_struct_ptr "
                        "PhysicalStorageBuffer\n" :
                        "")
         << (use8Bit ? "                                  OpTypeForwardPointer %i8vec4_array_struct_ptr "
                       "PhysicalStorageBuffer\n" :
                       "")
         << (use16Bit ? "                                  OpTypeForwardPointer %i16vec4_array_struct_ptr "
                        "PhysicalStorageBuffer\n" :
                        "")
         << (use32Bit ? "                                  OpTypeForwardPointer %i32vec4_array_struct_ptr "
                        "PhysicalStorageBuffer\n" :
                        "")
         << (use64Bit ? "                                  OpTypeForwardPointer %i64vec4_array_struct_ptr "
                        "PhysicalStorageBuffer\n" :
                        "")
         << "\n"
         << (use8Bit ?
                 "     %u8scalar_array_struct_ptr = OpTypePointer PhysicalStorageBuffer %u8scalar_array_struct\n" :
                 "")
         << (use16Bit ?
                 "    %u16scalar_array_struct_ptr = OpTypePointer PhysicalStorageBuffer %u16scalar_array_struct\n" :
                 "")
         << (use32Bit ?
                 "    %u32scalar_array_struct_ptr = OpTypePointer PhysicalStorageBuffer %u32scalar_array_struct\n" :
                 "")
         << (use64Bit ?
                 "    %u64scalar_array_struct_ptr = OpTypePointer PhysicalStorageBuffer %u64scalar_array_struct\n" :
                 "")
         << (use8Bit ? "       %u8vec4_array_struct_ptr = OpTypePointer PhysicalStorageBuffer %u8vec4_array_struct\n" :
                       "")
         << (use16Bit ?
                 "      %u16vec4_array_struct_ptr = OpTypePointer PhysicalStorageBuffer %u16vec4_array_struct\n" :
                 "")
         << (use32Bit ?
                 "      %u32vec4_array_struct_ptr = OpTypePointer PhysicalStorageBuffer %u32vec4_array_struct\n" :
                 "")
         << (use64Bit ?
                 "      %u64vec4_array_struct_ptr = OpTypePointer PhysicalStorageBuffer %u64vec4_array_struct\n" :
                 "")
         << (use8Bit ?
                 "     %i8scalar_array_struct_ptr = OpTypePointer PhysicalStorageBuffer %i8scalar_array_struct\n" :
                 "")
         << (use16Bit ?
                 "    %i16scalar_array_struct_ptr = OpTypePointer PhysicalStorageBuffer %i16scalar_array_struct\n" :
                 "")
         << (use32Bit ?
                 "    %i32scalar_array_struct_ptr = OpTypePointer PhysicalStorageBuffer %i32scalar_array_struct\n" :
                 "")
         << (use64Bit ?
                 "    %i64scalar_array_struct_ptr = OpTypePointer PhysicalStorageBuffer %i64scalar_array_struct\n" :
                 "")
         << (use8Bit ? "       %i8vec4_array_struct_ptr = OpTypePointer PhysicalStorageBuffer %i8vec4_array_struct\n" :
                       "")
         << (use16Bit ?
                 "      %i16vec4_array_struct_ptr = OpTypePointer PhysicalStorageBuffer %i16vec4_array_struct\n" :
                 "")
         << (use32Bit ?
                 "      %i32vec4_array_struct_ptr = OpTypePointer PhysicalStorageBuffer %i32vec4_array_struct\n" :
                 "")
         << (use64Bit ?
                 "      %i64vec4_array_struct_ptr = OpTypePointer PhysicalStorageBuffer %i64vec4_array_struct\n" :
                 "")
         << "\n"
         << "                                  ; Pointers to types in the physical storage buffers\n"
         << "                                  ; These are used to load and store values from and to the physical "
            "storage buffers\n"
         << (use8Bit ? "                  %u8scalar_ptr = OpTypePointer PhysicalStorageBuffer %u8scalar\n" : "")
         << (use16Bit ? "                 %u16scalar_ptr = OpTypePointer PhysicalStorageBuffer %u16scalar\n" : "")
         << (use32Bit ? "                 %u32scalar_ptr = OpTypePointer PhysicalStorageBuffer %u32scalar\n" : "")
         << (use64Bit ? "                 %u64scalar_ptr = OpTypePointer PhysicalStorageBuffer %u64scalar\n" : "")
         << "\n"
         << (use8Bit ? "                  %i8scalar_ptr = OpTypePointer PhysicalStorageBuffer %i8scalar\n" : "")
         << (use16Bit ? "                 %i16scalar_ptr = OpTypePointer PhysicalStorageBuffer %i16scalar\n" : "")
         << (use32Bit ? "                 %i32scalar_ptr = OpTypePointer PhysicalStorageBuffer %i32scalar\n" : "")
         << (use64Bit ? "                 %i64scalar_ptr = OpTypePointer PhysicalStorageBuffer %i64scalar\n" : "")
         << "\n"
         << (use8Bit ? "                    %u8vec4_ptr = OpTypePointer PhysicalStorageBuffer %u8vec4\n" : "")
         << (use16Bit ? "                   %u16vec4_ptr = OpTypePointer PhysicalStorageBuffer %u16vec4\n" : "")
         << (use32Bit ? "                   %u32vec4_ptr = OpTypePointer PhysicalStorageBuffer %u32vec4\n" : "")
         << (use64Bit ? "                   %u64vec4_ptr = OpTypePointer PhysicalStorageBuffer %u64vec4\n" : "") << "\n"
         << (use8Bit ? "                    %i8vec4_ptr = OpTypePointer PhysicalStorageBuffer %i8vec4\n" : "")
         << (use16Bit ? "                   %i16vec4_ptr = OpTypePointer PhysicalStorageBuffer %i16vec4\n" : "")
         << (use32Bit ? "                   %i32vec4_ptr = OpTypePointer PhysicalStorageBuffer %i32vec4\n" : "")
         << (use64Bit ? "                   %i64vec4_ptr = OpTypePointer PhysicalStorageBuffer %i64vec4\n" : "") << "\n"
         << "            %u32scalar_func_ptr = OpTypePointer Function %u32scalar\n"
         << "           %u32scalar_input_ptr = OpTypePointer Input %u32scalar\n"
         << "            %i32scalar_func_ptr = OpTypePointer Function %i32scalar\n"
         << "           %i32scalar_input_ptr = OpTypePointer Input %i32scalar\n"
         << "\n"
         << "       %gl_LocalInvocationIndex = OpVariable %u32scalar_input_ptr Input\n"
         << "\n";

    // References block will need to have the appropriate number of members to reflect all inputs and outputs.
    {
        const auto genPtrName = [&](const OperandType &operand)
        { return operand.getSpvAsmTypePrefix() + "_array_struct_ptr"; };

        std::string referencesBlockPtrTypes;
        for (const auto &operand : m_params.operandList)
            referencesBlockPtrTypes += " " + genPtrName(operand);
        comp << "               %ReferencesBlock = OpTypeStruct" + referencesBlockPtrTypes;
    }
    comp
        << "\n"
        << "          %references_block_ptr = OpTypePointer StorageBuffer %ReferencesBlock\n"
        << "                    %references = OpVariable %references_block_ptr StorageBuffer\n"
        << "\n"
        << "                                  ; Pointers to the array struct pointers (i.e. the pointers in the "
           "storage buffer that point to the other buffers)\n"
        << (use8Bit ? " %u8scalar_array_struct_ptr_ptr = OpTypePointer StorageBuffer %u8scalar_array_struct_ptr\n" : "")
        << (use16Bit ? "%u16scalar_array_struct_ptr_ptr = OpTypePointer StorageBuffer %u16scalar_array_struct_ptr\n" :
                       "")
        << (use32Bit ? "%u32scalar_array_struct_ptr_ptr = OpTypePointer StorageBuffer %u32scalar_array_struct_ptr\n" :
                       "")
        << (use64Bit ? "%u64scalar_array_struct_ptr_ptr = OpTypePointer StorageBuffer %u64scalar_array_struct_ptr\n" :
                       "")
        << "\n"
        << (use8Bit ? " %i8scalar_array_struct_ptr_ptr = OpTypePointer StorageBuffer %i8scalar_array_struct_ptr\n" : "")
        << (use16Bit ? "%i16scalar_array_struct_ptr_ptr = OpTypePointer StorageBuffer %i16scalar_array_struct_ptr\n" :
                       "")
        << (use32Bit ? "%i32scalar_array_struct_ptr_ptr = OpTypePointer StorageBuffer %i32scalar_array_struct_ptr\n" :
                       "")
        << (use64Bit ? "%i64scalar_array_struct_ptr_ptr = OpTypePointer StorageBuffer %i64scalar_array_struct_ptr\n" :
                       "")
        << "\n"
        << (use8Bit ? "   %u8vec4_array_struct_ptr_ptr = OpTypePointer StorageBuffer %u8vec4_array_struct_ptr\n" : "")
        << (use16Bit ? "  %u16vec4_array_struct_ptr_ptr = OpTypePointer StorageBuffer %u16vec4_array_struct_ptr\n" : "")
        << (use32Bit ? "  %u32vec4_array_struct_ptr_ptr = OpTypePointer StorageBuffer %u32vec4_array_struct_ptr\n" : "")
        << (use64Bit ? "  %u64vec4_array_struct_ptr_ptr = OpTypePointer StorageBuffer %u64vec4_array_struct_ptr\n" : "")
        << (use8Bit ? "   %i8vec4_array_struct_ptr_ptr = OpTypePointer StorageBuffer %i8vec4_array_struct_ptr\n" : "")
        << (use16Bit ? "  %i16vec4_array_struct_ptr_ptr = OpTypePointer StorageBuffer %i16vec4_array_struct_ptr\n" : "")
        << (use32Bit ? "  %i32vec4_array_struct_ptr_ptr = OpTypePointer StorageBuffer %i32vec4_array_struct_ptr\n" : "")
        << (use64Bit ? "  %i64vec4_array_struct_ptr_ptr = OpTypePointer StorageBuffer %i64vec4_array_struct_ptr\n" : "")
        << "\n"
        << "                                  ; Integer constants\n";

    {
        for (size_t i = 0; i < m_params.operandList.size(); ++i)
            comp << "                         %int_" << i << " = OpConstant %i32scalar " << i << "\n";
    }

    comp << "\n"
         << "                          %main = OpFunction %void None %void_func\n"
         << "                    %main_label = OpLabel\n"
         << "                           %idx = OpLoad %u32scalar %gl_LocalInvocationIndex\n"
         << "\n";

    {
        // Pointers to load the operation arguments.
        for (size_t i = 0; i < m_params.operandList.size(); ++i)
        {
            const auto &operand   = m_params.operandList.at(i);
            const auto typePrefix = operand.getSpvAsmTypePrefix();
            comp << "%" << operand.name << "_buffer_ref_ptr = OpAccessChain " << typePrefix
                 << "_array_struct_ptr_ptr %references %int_" << i << "\n"
                 << "%" << operand.name << "_buffer_ptr = OpLoad " << typePrefix << "_array_struct_ptr %"
                 << operand.name << "_buffer_ref_ptr\n"
                 << "                    %" << operand.name << "_ptr = OpAccessChain " << typePrefix << "_ptr %"
                 << operand.name << "_buffer_ptr %int_0 %idx\n"
                 << "\n";
        }
    }

    {
        // Load arguments. Skip the first one, which will be the output.
        for (size_t i = 1; i < m_params.operandList.size(); ++i)
        {
            const auto &operand   = m_params.operandList.at(i);
            const auto typePrefix = operand.getSpvAsmTypePrefix();
            const auto alignment  = operand.getSpvAlignment();

            comp << "%" << operand.name << " = OpLoad " << typePrefix << " %" << operand.name << "_ptr Aligned "
                 << alignment << "\n";
        }
    }
    comp << "\n";
    {
        // Run the operation with all the arguments, then store the result.
        const auto &operand   = m_params.operandList.front();
        const auto typePrefix = operand.getSpvAsmTypePrefix();
        const auto alignment  = operand.getSpvAlignment();

        std::string argList;
        for (size_t i = 1; i < m_params.operandList.size(); ++i)
            argList += " %" + m_params.operandList.at(i).name;

        comp << "%" << operand.name << " = " << getSpvOpName(m_params.bitOp) << " " << typePrefix << argList << "\n"
             << "                                  OpStore %" << operand.name << "_ptr %" << operand.name << " Aligned "
             << alignment << "\n";
    }
    comp << "\n"
         << "                                  OpReturn\n"
         << "                                  OpFunctionEnd\n";

    const auto vkVersion = programCollection.usedVulkanVersion;
    const SpirVAsmBuildOptions buildOptions(vkVersion, SPIRV_VERSION_1_6, false, false, true);

    programCollection.spirvAsmSources.add("comp") << comp.str() << buildOptions;
}

union ComponentValue
{
    uint8_t value8;
    uint16_t value16;
    uint32_t value32;
    uint64_t value64;
    uint8_t bytes[8];
};

struct OperandValue
{
    BitSize bitSize;
    bool isVector;
    ComponentValue components[4];

    uint32_t getComponentCount() const
    {
        return (isVector ? 4u : 1u);
    }

    OperandValue() : bitSize(BitSize::INVALID), isVector(false)
    {
    }

    // Build operand value from a value in a buffer, typically.
    OperandValue(BitSize bitSize_, bool isVector_, const uint8_t *bytes) : bitSize(bitSize_), isVector(isVector_)
    {
        const auto componentCount = getComponentCount();
        const auto componentBytes = static_cast<uint32_t>(bitSize) / 8u;

        for (uint32_t i = 0u; i < componentCount; ++i)
        {
            const uint8_t *componentData = bytes + i * componentBytes;
            memcpy(components[i].bytes, componentData, componentBytes);
        }
    }

    bool operator==(const OperandValue &other) const
    {
        DE_ASSERT(bitSize == other.bitSize);
        DE_ASSERT(isVector == other.isVector);

        const auto componentCount = getComponentCount();
        bool equal                = true;

        for (uint32_t i = 0u; i < componentCount; ++i)
        {
            if (bitSize == BitSize::BIT8)
                equal = (equal && components[i].value8 == other.components[i].value8);
            else if (bitSize == BitSize::BIT16)
                equal = (equal && components[i].value16 == other.components[i].value16);
            else if (bitSize == BitSize::BIT32)
                equal = (equal && components[i].value32 == other.components[i].value32);
            else if (bitSize == BitSize::BIT64)
                equal = (equal && components[i].value64 == other.components[i].value64);
            else
                DE_ASSERT(false);
        }

        return equal;
    }

    bool operator!=(const OperandValue &other) const
    {
        return !(*this == other);
    }

    std::vector<uint8_t> toBytes() const
    {
        const auto byteSize   = static_cast<uint32_t>(bitSize) / 8u;
        const auto elemCount  = getComponentCount();
        const auto totalBytes = byteSize * elemCount;

        std::vector<uint8_t> bytes;
        bytes.reserve(totalBytes);

        // 1 or 4 elements.
        for (uint32_t i = 0u; i < elemCount; ++i)
        {
            const auto &value = components[i];
            bytes.insert(bytes.end(), value.bytes, value.bytes + byteSize);
        }

        return bytes;
    }

    uint64_t getValueAsU64(uint32_t component = 0) const
    {
        uint64_t ret = 0;

        if (component != 0)
            DE_ASSERT(isVector);

        switch (bitSize)
        {
        case BitSize::BIT8:
            ret = components[component].value8;
            break;
        case BitSize::BIT16:
            ret = components[component].value16;
            break;
        case BitSize::BIT32:
            ret = components[component].value32;
            break;
        case BitSize::BIT64:
            ret = components[component].value64;
            break;
        default:
            DE_ASSERT(false);
        }

        return ret;
    }

    void setValueAsU64(uint64_t value, uint32_t component = 0)
    {
        if (component != 0)
            DE_ASSERT(isVector);

        switch (bitSize)
        {
        case BitSize::BIT8:
            DE_ASSERT(value <= std::numeric_limits<uint8_t>::max());
            components[component].value8 = static_cast<uint8_t>(value);
            break;
        case BitSize::BIT16:
            DE_ASSERT(value <= std::numeric_limits<uint16_t>::max());
            components[component].value16 = static_cast<uint16_t>(value);
            break;
        case BitSize::BIT32:
            DE_ASSERT(value <= std::numeric_limits<uint32_t>::max());
            components[component].value32 = static_cast<uint32_t>(value);
            break;
        case BitSize::BIT64:
            components[component].value64 = value;
            break;
        default:
            DE_ASSERT(false);
            break;
        }
    }

    std::string toString() const
    {
        const auto componentCount = getComponentCount();
        std::ostringstream repr;

        if (isVector)
            repr << "(";
        for (uint32_t i = 0u; i < componentCount; ++i)
            repr << (i == 0u ? "" : ", ") << getValueAsU64(i);
        if (isVector)
            repr << ")";

        return repr.str();
    }
};

OperandValue genSingleOperand(de::Random &rnd, BitSize bitSize, bool isVector, tcu::Maybe<std::pair<int, int>> minMax)
{
    OperandValue ret;
    ret.bitSize  = bitSize;
    ret.isVector = isVector;
    memset(ret.components, 0, sizeof(ret.components));

    const auto elemCount = (isVector ? 4 : 1);
    for (int i = 0; i < elemCount; ++i)
    {
        const bool restricted   = static_cast<bool>(minMax);
        const int restrictedVal = (restricted ? rnd.getInt(minMax->first, minMax->second) : 0);

        if (bitSize == BitSize::BIT8)
            ret.components[i].value8 = (restricted ? static_cast<uint8_t>(restrictedVal) : rnd.getUint8());
        else if (bitSize == BitSize::BIT16)
            ret.components[i].value16 = (restricted ? static_cast<uint16_t>(restrictedVal) : rnd.getUint16());
        else if (bitSize == BitSize::BIT32)
            ret.components[i].value32 = (restricted ? static_cast<uint32_t>(restrictedVal) : rnd.getUint32());
        else if (bitSize == BitSize::BIT64)
            ret.components[i].value64 = (restricted ? static_cast<uint64_t>(restrictedVal) : rnd.getUint64());
        else
            DE_ASSERT(false);
    }

    return ret;
}

using OperandValuesVec = std::vector<OperandValue>;

OperandValuesVec genValuesForOp(de::Random &rnd, const TestParams &params)
{
    OperandValuesVec values;
    values.reserve(params.operandList.size());

    if (params.bitOp == BitOp::COUNT || params.bitOp == BitOp::REVERSE)
    {
        for (const auto &operand : params.operandList)
            values.push_back(genSingleOperand(rnd, operand.bitSize, operand.isVector, tcu::Nothing));
    }
    else
    {
        // Careful with the offset and count (last 2 operands).
        DE_ASSERT(params.operandList.size() > 2);
        for (size_t i = 0; i < params.operandList.size() - 2; ++i)
        {
            const auto &operand = params.operandList.at(i);
            values.push_back(genSingleOperand(rnd, operand.bitSize, operand.isVector, tcu::Nothing));
        }

        const auto componentBits = static_cast<int>(params.operandList.front().bitSize);

        {
            const auto &offsetOperand = params.operandList.at(params.operandList.size() - 2);
            DE_ASSERT(offsetOperand.name == "offset");
            DE_ASSERT(!offsetOperand.isVector);
            values.push_back(
                genSingleOperand(rnd, offsetOperand.bitSize, false, tcu::just(std::make_pair(0, componentBits))));
        }
        {
            const auto &countOperand = params.operandList.back();

            DE_ASSERT(countOperand.name == "count");
            DE_ASSERT(!countOperand.isVector);
            const auto offset64 = values.back().getValueAsU64();
            DE_ASSERT(offset64 <= static_cast<uint64_t>(componentBits));
            const auto offset = static_cast<int>(offset64);

            values.push_back(genSingleOperand(rnd, countOperand.bitSize, false,
                                              tcu::just(std::make_pair(0, componentBits - offset))));
        }
    }

    return values;
}

uint64_t singleBitCount(uint64_t value)
{
    std::bitset<64> bits(value);
    return static_cast<uint64_t>(bits.count());
}

uint64_t singleBitReverse(uint64_t value, uint32_t bitCount)
{
    uint64_t result = 0u;

    for (uint32_t i = 0u; i < bitCount; ++i)
    {
        uint64_t bit = ((value >> i) & 1);
        result |= (bit << (bitCount - 1u - i));
    }

    return result;
}

uint64_t singleBitFieldInsert(uint64_t base, uint64_t insert, uint64_t offset, uint64_t count)
{
    // insertMask: bits [0,count-1] all set to 1, the rest 0.
    // actualInsert: preserve only the bits of the insert that are set in insertMask.
    // baseMask: all bits to 1 except count bits at offset, which are 0.
    // result: mask the base with baseMask to disable those bits, then bit-or them with actualInsert at the offset.
    constexpr uint64_t kOne = uint64_t{1};
    uint64_t insertMask     = ((count >= 64) ? std::numeric_limits<uint64_t>::max() : ((kOne << count) - kOne));
    uint64_t actualInsert   = (insert & insertMask);
    uint64_t baseMask       = ~(insertMask << offset);
    uint64_t result         = ((base & baseMask) | (actualInsert << offset));
    return result;
}

uint64_t singleBitFieldExtract(bool signedExtraction, uint64_t base, uint64_t offset, uint64_t count)
{
    constexpr uint64_t kOne = uint64_t{1};
    const auto baseMask     = ((count >= 64) ? std::numeric_limits<uint64_t>::max() : ((kOne << count) - kOne));
    const auto fieldMask    = (baseMask << offset);
    auto extractedBits      = ((base & fieldMask) >> offset);
    if (signedExtraction && count > 0)
    {
        const auto signBit = ((extractedBits >> (count - 1)) & 1);
        if (signBit)
            extractedBits |= (~baseMask);
    }
    return extractedBits;
}

OperandValue calcOpBitCount(const OperandValuesVec &operandValues)
{
    DE_ASSERT(operandValues.size() == 2);

    const auto &base          = operandValues.back();
    const auto componentCount = base.getComponentCount();

    OperandValue result = operandValues.front();
    DE_ASSERT(result.getComponentCount() == componentCount);

    for (uint32_t i = 0u; i < componentCount; ++i)
        result.setValueAsU64(singleBitCount(base.getValueAsU64(i)), i);

    return result;
}

OperandValue calcOpBitReverse(const OperandValuesVec &operandValues)
{
    DE_ASSERT(operandValues.size() == 2);

    const auto &base          = operandValues.back();
    const auto componentCount = base.getComponentCount();

    OperandValue result = operandValues.front();
    DE_ASSERT(result.getComponentCount() == componentCount);

    for (uint32_t i = 0u; i < componentCount; ++i)
        result.setValueAsU64(singleBitReverse(base.getValueAsU64(i), static_cast<uint32_t>(base.bitSize)), i);

    return result;
}

OperandValue calcOpBitFieldInsert(const OperandValuesVec &operandValues)
{
    DE_ASSERT(operandValues.size() == 5);

    const auto &base   = operandValues.at(1);
    const auto &insert = operandValues.at(2);
    const auto &offset = operandValues.at(3);
    const auto &count  = operandValues.at(4);

    const auto componentCount = base.getComponentCount();

    OperandValue result = operandValues.front();
    DE_ASSERT(result.getComponentCount() == componentCount);

    for (uint32_t i = 0u; i < componentCount; ++i)
        result.setValueAsU64(singleBitFieldInsert(base.getValueAsU64(i), insert.getValueAsU64(i),
                                                  offset.getValueAsU64(), count.getValueAsU64()),
                             i);

    return result;
}

uint64_t maskToBitWidth(uint64_t value, BitSize bitSize)
{
    if (bitSize == BitSize::BIT8)
        return (value & uint64_t{0xFFu});
    if (bitSize == BitSize::BIT16)
        return (value & uint64_t{0xFFFFu});
    if (bitSize == BitSize::BIT32)
        return (value & uint64_t{0xFFFFFFFFu});
    return value;
}

OperandValue calcOpBitFieldSExtract(const OperandValuesVec &operandValues)
{
    DE_ASSERT(operandValues.size() == 4);

    const auto &base   = operandValues.at(1);
    const auto &offset = operandValues.at(2);
    const auto &count  = operandValues.at(3);

    const auto componentCount = base.getComponentCount();

    OperandValue result = operandValues.front();
    DE_ASSERT(result.getComponentCount() == componentCount);

    for (uint32_t i = 0u; i < componentCount; ++i)
    {
        const auto extractedValue =
            singleBitFieldExtract(true, base.getValueAsU64(i), offset.getValueAsU64(), count.getValueAsU64());
        // When extracting values as uint64_t with sign extension, we may end up with large numbers.
        // The setValueAsU64 call below will assert in those cases, so we need to mask the bits to the needed bit width.
        result.setValueAsU64(maskToBitWidth(extractedValue, base.bitSize), i);
    }

    return result;
}

OperandValue calcOpBitFieldUExtract(const OperandValuesVec &operandValues)
{
    DE_ASSERT(operandValues.size() == 4);

    const auto &base   = operandValues.at(1);
    const auto &offset = operandValues.at(2);
    const auto &count  = operandValues.at(3);

    const auto componentCount = base.getComponentCount();

    OperandValue result = operandValues.front();
    DE_ASSERT(result.getComponentCount() == componentCount);

    for (uint32_t i = 0u; i < componentCount; ++i)
        result.setValueAsU64(
            singleBitFieldExtract(false, base.getValueAsU64(i), offset.getValueAsU64(), count.getValueAsU64()), i);

    return result;
}

OperandValue calcOp(BitOp bitOp, const OperandValuesVec &operandValues)
{
    if (bitOp == BitOp::COUNT)
        return calcOpBitCount(operandValues);
    else if (bitOp == BitOp::REVERSE)
        return calcOpBitReverse(operandValues);
    else if (bitOp == BitOp::INSERT)
        return calcOpBitFieldInsert(operandValues);
    else if (bitOp == BitOp::S_EXTRACT)
        return calcOpBitFieldSExtract(operandValues);
    else if (bitOp == BitOp::U_EXTRACT)
        return calcOpBitFieldUExtract(operandValues);
    else
        DE_ASSERT(false);
    return OperandValue();
}

std::string opToString(BitOp bitOp, const OperandValuesVec &operandValues)
{
    std::string result = getSpvOpName(bitOp) + "(";
    for (size_t i = 1; i < operandValues.size(); ++i)
        result += (i == 1 ? "" : ", ") + operandValues.at(i).toString();
    result += ")";
    return result;
}

tcu::TestStatus M9V_Instance::iterate(void)
{
    const auto ctx             = m_context.getContextCommonData();
    const auto seed            = m_params.getRandomSeed();
    const auto wgSize          = m_params.getWorkGroupSize();
    const auto opBufferUsage   = (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    const auto opBufferMemReqs = (MemoryRequirement::DeviceAddress | MemoryRequirement::HostVisible);

    using BufferWithMemoryPtr = std::unique_ptr<BufferWithMemory>;

    // We will store the values for each operand in a separate buffer. Buffer 0 will always be the result buffer.
    // When running the shader, invocation N will do:
    //
    //     resultsBuffer[N] = OpSomething operand1Buffer[N] operand2Buffer[N] ...
    //
    // These buffers will all be passed as a reference inside another buffer, i.e., the descriptor set will only contain
    // a storage buffer descriptor, and this storage buffer contains buffer addresses for the result and all the
    // operands the operation needs. The goal is preventing shader compiler scalarization on some implementations.
    std::vector<BufferWithMemoryPtr> buffers;
    buffers.reserve(m_params.operandList.size());

    for (const auto &operand : m_params.operandList)
    {
        const auto bufferSize = static_cast<VkDeviceSize>(operand.getDataSizeBytes() * wgSize);
        const auto bufferInfo = makeBufferCreateInfo(bufferSize, opBufferUsage);
        buffers.emplace_back(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, bufferInfo, opBufferMemReqs));
    }

    // Generate pseudorandom data for each buffer.
    de::Random rnd(seed);

    // Each element of this vector will contain the operands for a single operation.
    std::vector<OperandValuesVec> operations;
    operations.reserve(wgSize);

    for (uint32_t i = 0u; i < wgSize; ++i)
    {
        auto values = genValuesForOp(rnd, m_params);
        DE_ASSERT(values.size() == buffers.size());

        // Copy operands, as bytes, to the corresponding buffer.
        for (size_t j = 0; j < buffers.size(); ++j)
        {
            const auto bytes         = values.at(j).toBytes();
            const auto prevByteCount = bytes.size() * static_cast<size_t>(i);
            uint8_t *hostPtr         = reinterpret_cast<uint8_t *>(buffers.at(j)->getAllocation().getHostPtr());
            memcpy(hostPtr + prevByteCount, de::dataOrNull(bytes), de::dataSize(bytes));
        }

        operations.emplace_back(std::move(values));
    }

    for (const auto &buffer : buffers)
    {
        flushAlloc(ctx.vkd, ctx.device, buffer->getAllocation());
    }

    // Prepare storage buffer with buffer references.
    const auto referencesBufferSize  = static_cast<VkDeviceSize>(buffers.size() * sizeof(VkDeviceAddress));
    const auto referencesBufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    const auto referencesBufferInfo  = makeBufferCreateInfo(referencesBufferSize, referencesBufferUsage);
    BufferWithMemory referencesBuffer(ctx.vkd, ctx.device, ctx.allocator, referencesBufferInfo,
                                      MemoryRequirement::HostVisible);
    auto &referencesBufferAlloc = referencesBuffer.getAllocation();
    {
        std::vector<VkDeviceAddress> addresses;
        addresses.reserve(buffers.size());
        for (const auto &buffer : buffers)
        {
            const VkBufferDeviceAddressInfo addressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr,
                                                        buffer->get()};
            addresses.push_back(ctx.vkd.getBufferDeviceAddress(ctx.device, &addressInfo));
        }

        memcpy(referencesBufferAlloc.getHostPtr(), de::dataOrNull(addresses), de::dataSize(addresses));
        flushAlloc(ctx.vkd, ctx.device, referencesBufferAlloc);
    }

    const auto descType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto shaderStages = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_COMPUTE_BIT);

    DescriptorSetLayoutBuilder setLayoutBuilder;
    setLayoutBuilder.addSingleBinding(descType, shaderStages);
    const auto setLayout      = setLayoutBuilder.build(ctx.vkd, ctx.device);
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *setLayout);

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descType);
    const auto descPool = poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet = makeDescriptorSet(ctx.vkd, ctx.device, *descPool, *setLayout);

    const auto binding = DescriptorSetUpdateBuilder::Location::binding;
    DescriptorSetUpdateBuilder updateBuilder;
    const auto referencesBufferDescInfo = makeDescriptorBufferInfo(*referencesBuffer, 0ull, VK_WHOLE_SIZE);
    updateBuilder.writeSingle(*descriptorSet, binding(0u), descType, &referencesBufferDescInfo);
    updateBuilder.update(ctx.vkd, ctx.device);

    const auto &binaries  = m_context.getBinaryCollection();
    const auto compShader = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));
    const auto pipeline   = makeComputePipeline(ctx.vkd, ctx.device, *pipelineLayout, *compShader);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    ctx.vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u,
                                  &descriptorSet.get(), 0u, nullptr);
    ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    ctx.vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);
    {
        const auto barrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 &barrier);
    }
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    auto &resultsBufferAlloc = buffers.front()->getAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, resultsBufferAlloc);

    const auto resultsData    = reinterpret_cast<const uint8_t *>(resultsBufferAlloc.getHostPtr());
    const auto &resultOperand = m_params.operandList.front();
    const auto resultSize     = resultOperand.getDataSizeBytes();

    bool fail = false;
    auto &log = m_context.getTestContext().getLog();

    for (uint32_t i = 0u; i < wgSize; ++i)
    {
        const auto &operation = operations.at(i);
        const auto expected   = calcOp(m_params.bitOp, operation);

        const OperandValue result{resultOperand.bitSize, resultOperand.isVector, resultsData + i * resultSize};

        if (expected != result)
        {
            fail = true;
            std::ostringstream msg;
            msg << "Unexpected value at index " << i << ": " << opToString(m_params.bitOp, operation)
                << " expected result " << expected.toString() << " but found " << result.toString();
            log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
        }
    }

    if (fail)
        TCU_FAIL("Some results differ from the expected values; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

using TestCaseGroupPtr = de::MovePtr<tcu::TestCaseGroup>;

} // namespace

tcu::TestCaseGroup *createMaint9VectorizationTests(tcu::TestContext &testCtx)
{
    TestCaseGroupPtr mainGroup{new tcu::TestCaseGroup{testCtx, "maint9_vectorization"}};

    // OpBitCount
    {
        TestCaseGroupPtr bitCountGroup{new tcu::TestCaseGroup{testCtx, "bit_count"}};

        for (const bool isVector : {false, true})
            for (const auto baseBits : {BitSize::BIT8, BitSize::BIT16, BitSize::BIT32, BitSize::BIT64})
                for (const auto resultBits : {BitSize::BIT8, BitSize::BIT16, BitSize::BIT32, BitSize::BIT64})
                    for (const bool isSigned : {false, true})
                    {
                        const OperandList operands{
                            {isVector, isSigned, resultBits, "result"},
                            {isVector, isSigned, baseBits, "base"},
                        };
                        const TestParams params{BitOp::COUNT, operands};
                        const auto testName = getOperandListTestName(operands);
                        bitCountGroup->addChild(new M9V_Case(testCtx, testName, params));
                    }

        mainGroup->addChild(bitCountGroup.release());
    }

    // OpBitReverse
    {
        TestCaseGroupPtr bitReverseGroup{new tcu::TestCaseGroup{testCtx, "bit_reverse"}};

        for (const bool isVector : {false, true})
            for (const auto baseBits : {BitSize::BIT8, BitSize::BIT16, BitSize::BIT32, BitSize::BIT64})
                for (const bool isSigned : {false, true})
                {
                    const OperandList operands{
                        {isVector, isSigned, baseBits, "result"},
                        {isVector, isSigned, baseBits, "base"},
                    };
                    const TestParams params{BitOp::REVERSE, operands};
                    const auto testName = getOperandListTestName(operands);
                    bitReverseGroup->addChild(new M9V_Case(testCtx, testName, params));
                }

        mainGroup->addChild(bitReverseGroup.release());
    }

    // OpBitFieldInsert
    {
        TestCaseGroupPtr bitFieldInsertGroup(new tcu::TestCaseGroup(testCtx, "bit_field_insert"));

        for (const bool isVector : {false, true})
            for (const auto baseBits : {BitSize::BIT8, BitSize::BIT16, BitSize::BIT32, BitSize::BIT64})
                for (const auto offsetBits : {BitSize::BIT8, BitSize::BIT16, BitSize::BIT32, BitSize::BIT64})
                    for (const auto countBits : {BitSize::BIT8, BitSize::BIT16, BitSize::BIT32, BitSize::BIT64})
                        for (const bool signedBase : {false, true})
                            for (const bool signedOffset : {false, true})
                                for (const bool signedCount : {false, true})
                                {
                                    const OperandList operands{
                                        {isVector, signedBase, baseBits, "result"},
                                        {isVector, signedBase, baseBits, "base"},
                                        {isVector, signedBase, baseBits, "insert"},
                                        {false, signedOffset, offsetBits, "offset"},
                                        {false, signedCount, countBits, "count"},
                                    };
                                    const TestParams params{BitOp::INSERT, operands};
                                    const auto testName = getOperandListTestName(operands);
                                    bitFieldInsertGroup->addChild(new M9V_Case(testCtx, testName, params));
                                }

        mainGroup->addChild(bitFieldInsertGroup.release());
    }

    // OpBitFieldSExtract
    {
        TestCaseGroupPtr bitFieldSExtractGroup(new tcu::TestCaseGroup(testCtx, "bit_field_s_extract"));

        for (const bool isVector : {false, true})
            for (const auto baseBits : {BitSize::BIT8, BitSize::BIT16, BitSize::BIT32, BitSize::BIT64})
                for (const auto offsetBits : {BitSize::BIT8, BitSize::BIT16, BitSize::BIT32, BitSize::BIT64})
                    for (const auto countBits : {BitSize::BIT8, BitSize::BIT16, BitSize::BIT32, BitSize::BIT64})
                        for (const bool signedBase : {false, true})
                            for (const bool signedOffset : {false, true})
                                for (const bool signedCount : {false, true})
                                {
                                    const OperandList operands{
                                        {isVector, signedBase, baseBits, "result"},
                                        {isVector, signedBase, baseBits, "base"},
                                        {false, signedOffset, offsetBits, "offset"},
                                        {false, signedCount, countBits, "count"},
                                    };
                                    const TestParams params{BitOp::S_EXTRACT, operands};
                                    const auto testName = getOperandListTestName(operands);
                                    bitFieldSExtractGroup->addChild(new M9V_Case(testCtx, testName, params));
                                }

        mainGroup->addChild(bitFieldSExtractGroup.release());
    }

    // OpBitFieldUExtract
    {
        TestCaseGroupPtr bitFieldUExtractGroup(new tcu::TestCaseGroup(testCtx, "bit_field_u_extract"));

        for (const bool isVector : {false, true})
            for (const auto baseBits : {BitSize::BIT8, BitSize::BIT16, BitSize::BIT32, BitSize::BIT64})
                for (const auto offsetBits : {BitSize::BIT8, BitSize::BIT16, BitSize::BIT32, BitSize::BIT64})
                    for (const auto countBits : {BitSize::BIT8, BitSize::BIT16, BitSize::BIT32, BitSize::BIT64})
                        for (const bool signedBase : {false, true})
                            for (const bool signedOffset : {false, true})
                                for (const bool signedCount : {false, true})
                                {
                                    const OperandList operands{
                                        {isVector, signedBase, baseBits, "result"},
                                        {isVector, signedBase, baseBits, "base"},
                                        {false, signedOffset, offsetBits, "offset"},
                                        {false, signedCount, countBits, "count"},
                                    };
                                    const TestParams params{BitOp::U_EXTRACT, operands};
                                    const auto testName = getOperandListTestName(operands);
                                    bitFieldUExtractGroup->addChild(new M9V_Case(testCtx, testName, params));
                                }

        mainGroup->addChild(bitFieldUExtractGroup.release());
    }

    return mainGroup.release();
}

} // namespace SpirVAssembly
} // namespace vkt
