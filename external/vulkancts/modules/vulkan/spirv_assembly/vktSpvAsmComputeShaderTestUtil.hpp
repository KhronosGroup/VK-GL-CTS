#ifndef _VKTSPVASMCOMPUTESHADERTESTUTIL_HPP
#define _VKTSPVASMCOMPUTESHADERTESTUTIL_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Compute Shader Based Test Case Utility Structs/Functions
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"
#include "deFloat16.h"
#include "deRandom.hpp"
#include "tcuTestLog.hpp"
#include "tcuVector.hpp"
#include "tcuTestLog.hpp"
#include "vkMemUtil.hpp"
#include "vktSpvAsmUtils.hpp"

#include <string>
#include <vector>
#include <map>

using namespace vk;

namespace vkt
{
namespace SpirVAssembly
{

enum OpAtomicType
{
    OPATOMIC_IADD = 0,
    OPATOMIC_ISUB,
    OPATOMIC_IINC,
    OPATOMIC_IDEC,
    OPATOMIC_LOAD,
    OPATOMIC_STORE,
    OPATOMIC_COMPEX,

    OPATOMIC_LAST
};

enum BufferType
{
    BUFFERTYPE_INPUT = 0,
    BUFFERTYPE_EXPECTED,
    BUFFERTYPE_ATOMIC_RET,

    BUFFERTYPE_LAST
};

static void fillRandomScalars(de::Random &rnd, int32_t minValue, int32_t maxValue, int32_t *dst, int32_t numValues)
{
    for (int i = 0; i < numValues; i++)
        dst[i] = rnd.getInt(minValue, maxValue);
}

/*--------------------------------------------------------------------*//*!
* \brief Concrete class for an input/output storage buffer object used for OpAtomic tests
*//*--------------------------------------------------------------------*/
class OpAtomicBuffer : public BufferInterface
{
public:
    OpAtomicBuffer(const uint32_t numInputElements, const uint32_t numOuptutElements, const OpAtomicType opAtomic,
                   const BufferType type)
        : m_numInputElements(numInputElements)
        , m_numOutputElements(numOuptutElements)
        , m_opAtomic(opAtomic)
        , m_type(type)
    {
    }

    void getBytes(std::vector<uint8_t> &bytes) const
    {
        std::vector<int32_t> inputInts(m_numInputElements, 0);
        de::Random rnd(m_opAtomic);

        fillRandomScalars(rnd, 1, 100, &inputInts.front(), m_numInputElements);

        // Return input values as is
        if (m_type == BUFFERTYPE_INPUT)
        {
            size_t inputSize = m_numInputElements * sizeof(int32_t);

            bytes.resize(inputSize);
            deMemcpy(&bytes.front(), &inputInts.front(), inputSize);
        }
        // Calculate expected output values
        else if (m_type == BUFFERTYPE_EXPECTED)
        {
            size_t outputSize = m_numOutputElements * sizeof(int32_t);
            bytes.resize(outputSize, 0xffu);

            for (size_t ndx = 0; ndx < m_numInputElements; ndx++)
            {
                int32_t *const bytesAsInt = reinterpret_cast<int32_t *>(&bytes.front());

                switch (m_opAtomic)
                {
                case OPATOMIC_IADD:
                    bytesAsInt[0] += inputInts[ndx];
                    break;
                case OPATOMIC_ISUB:
                    bytesAsInt[0] -= inputInts[ndx];
                    break;
                case OPATOMIC_IINC:
                    bytesAsInt[0]++;
                    break;
                case OPATOMIC_IDEC:
                    bytesAsInt[0]--;
                    break;
                case OPATOMIC_LOAD:
                    bytesAsInt[ndx] = inputInts[ndx];
                    break;
                case OPATOMIC_STORE:
                    bytesAsInt[ndx] = inputInts[ndx];
                    break;
                case OPATOMIC_COMPEX:
                    bytesAsInt[ndx] = (inputInts[ndx] % 2) == 0 ? -1 : 1;
                    break;
                default:
                    DE_FATAL("Unknown OpAtomic type");
                }
            }
        }
        else if (m_type == BUFFERTYPE_ATOMIC_RET)
        {
            bytes.resize(m_numInputElements * sizeof(int32_t), 0xff);

            if (m_opAtomic == OPATOMIC_COMPEX)
            {
                int32_t *const bytesAsInt = reinterpret_cast<int32_t *>(&bytes.front());
                for (size_t ndx = 0; ndx < m_numInputElements; ndx++)
                    bytesAsInt[ndx] = inputInts[ndx] % 2;
            }
        }
        else
            DE_FATAL("Unknown buffer type");
    }

    void getPackedBytes(std::vector<uint8_t> &bytes) const
    {
        return getBytes(bytes);
    }

    size_t getByteSize(void) const
    {
        switch (m_type)
        {
        case BUFFERTYPE_ATOMIC_RET:
        case BUFFERTYPE_INPUT:
            return m_numInputElements * sizeof(int32_t);
        case BUFFERTYPE_EXPECTED:
            return m_numOutputElements * sizeof(int32_t);
        default:
            DE_FATAL("Unknown buffer type");
            return 0;
        }
    }

    template <int OpAtomic>
    static bool compareWithRetvals(const std::vector<Resource> &inputs, const std::vector<AllocationSp> &outputAllocs,
                                   const std::vector<Resource> &expectedOutputs, tcu::TestLog &log)
    {
        if (outputAllocs.size() != 2 || inputs.size() != 1)
            DE_FATAL("Wrong number of buffers to compare");

        for (size_t i = 0; i < outputAllocs.size(); ++i)
        {
            const uint32_t *values = reinterpret_cast<uint32_t *>(outputAllocs[i]->getHostPtr());

            if (i == 1 && OpAtomic != OPATOMIC_COMPEX)
            {
                // BUFFERTYPE_ATOMIC_RET for arithmetic operations must be verified manually by matching return values to inputs
                std::vector<uint8_t> inputBytes;
                inputs[0].getBytes(inputBytes);

                const uint32_t *inputValues   = reinterpret_cast<uint32_t *>(&inputBytes.front());
                const size_t inputValuesCount = inputBytes.size() / sizeof(uint32_t);

                // result of all atomic operations
                const uint32_t resultValue = *reinterpret_cast<uint32_t *>(outputAllocs[0]->getHostPtr());

                if (!compareRetVals<OpAtomic>(inputValues, inputValuesCount, resultValue, values))
                {
                    log << tcu::TestLog::Message
                        << "Wrong contents of buffer with return values after atomic operation."
                        << tcu::TestLog::EndMessage;
                    return false;
                }
            }
            else
            {
                const BufferSp &expectedOutput = expectedOutputs[i].getBuffer();
                std::vector<uint8_t> expectedBytes;

                expectedOutput->getBytes(expectedBytes);

                if (deMemCmp(&expectedBytes.front(), values, expectedBytes.size()))
                {
                    log << tcu::TestLog::Message << "Wrong contents of buffer after atomic operation"
                        << tcu::TestLog::EndMessage;
                    return false;
                }
            }
        }
        return true;
    }

    template <int OpAtomic>
    static bool compareRetVals(const uint32_t *inputValues, const size_t inputValuesCount, const uint32_t resultValue,
                               const uint32_t *returnValues)
    {
        // as the order of execution is undefined, validation of return values for atomic operations is tricky:
        // each inputValue stands for one atomic operation. Iterate through all of
        // done operations in time, each time finding one matching current result and un-doing it.

        std::vector<bool> operationsUndone(inputValuesCount, false);
        uint32_t currentResult = resultValue;

        for (size_t operationUndone = 0; operationUndone < inputValuesCount; ++operationUndone)
        {
            // find which of operations was done at this moment
            size_t ndx;
            for (ndx = 0; ndx < inputValuesCount; ++ndx)
            {
                if (operationsUndone[ndx])
                    continue;

                uint32_t previousResult = currentResult;

                switch (OpAtomic)
                {
                // operations are undone here, so the actual opeation is reversed
                case OPATOMIC_IADD:
                    previousResult -= inputValues[ndx];
                    break;
                case OPATOMIC_ISUB:
                    previousResult += inputValues[ndx];
                    break;
                case OPATOMIC_IINC:
                    previousResult--;
                    break;
                case OPATOMIC_IDEC:
                    previousResult++;
                    break;
                default:
                    DE_FATAL("Unsupported OpAtomic type for return value compare");
                }

                if (previousResult == returnValues[ndx])
                {
                    // found matching operation
                    currentResult         = returnValues[ndx];
                    operationsUndone[ndx] = true;
                    break;
                }
            }
            if (ndx == inputValuesCount)
            {
                // no operation matches the current result value
                return false;
            }
        }
        return true;
    }

private:
    const uint32_t m_numInputElements;
    const uint32_t m_numOutputElements;
    const OpAtomicType m_opAtomic;
    const BufferType m_type;
};

/*--------------------------------------------------------------------*//*!
 * \brief Concrete class for an input/output storage buffer object
 *//*--------------------------------------------------------------------*/
template <typename E>
class Buffer : public BufferInterface
{
public:
    Buffer(const std::vector<E> &elements, uint32_t padding = 0 /* in bytes */)
        : m_elements(elements)
        , m_padding(padding)
    {
    }

    void getBytes(std::vector<uint8_t> &bytes) const
    {
        const size_t count          = m_elements.size();
        const size_t perSegmentSize = sizeof(E) + m_padding;
        const size_t size           = count * perSegmentSize;

        bytes.resize(size);

        if (m_padding == 0)
        {
            deMemcpy(&bytes.front(), &m_elements.front(), size);
        }
        else
        {
            deMemset(&bytes.front(), 0xff, size);

            for (uint32_t elementIdx = 0; elementIdx < count; ++elementIdx)
                deMemcpy(&bytes[elementIdx * perSegmentSize], &m_elements[elementIdx], sizeof(E));
        }
    }

    void getPackedBytes(std::vector<uint8_t> &bytes) const
    {
        const size_t size = m_elements.size() * sizeof(E);

        bytes.resize(size);

        deMemcpy(&bytes.front(), &m_elements.front(), size);
    }

    size_t getByteSize(void) const
    {
        return m_elements.size() * (sizeof(E) + m_padding);
    }

private:
    std::vector<E> m_elements;
    uint32_t m_padding;
};

DE_STATIC_ASSERT(sizeof(tcu::Vec4) == 4 * sizeof(float));

typedef Buffer<float> Float32Buffer;
typedef Buffer<deFloat16> Float16Buffer;
typedef Buffer<double> Float64Buffer;
typedef Buffer<int64_t> Int64Buffer;
typedef Buffer<int32_t> Int32Buffer;
typedef Buffer<int16_t> Int16Buffer;
typedef Buffer<int8_t> Int8Buffer;
typedef Buffer<uint8_t> Uint8Buffer;
typedef Buffer<uint16_t> Uint16Buffer;
typedef Buffer<uint32_t> Uint32Buffer;
typedef Buffer<uint64_t> Uint64Buffer;
typedef Buffer<tcu::Vec4> Vec4Buffer;

typedef bool (*ComputeVerifyBinaryFunc)(const ProgramBinary &binary);

/*--------------------------------------------------------------------*//*!
 * \brief Specification for a compute shader.
 *
 * This struct bundles SPIR-V assembly code, input and expected output
 * together.
 *//*--------------------------------------------------------------------*/
struct ComputeShaderSpec
{
    std::string assembly;
    std::string entryPoint;
    std::vector<Resource> inputs;
    std::vector<Resource> outputs;
    vk::VkFormat inputFormat = vk::VK_FORMAT_R32G32B32A32_SFLOAT;
    tcu::IVec3 numWorkGroups;
    SpecConstants specConstants;
    BufferSp pushConstants;
    std::vector<std::string> extensions;
    VulkanFeatures requestedVulkanFeatures;
    qpTestResult failResult;
    std::string failMessage;
    // If null, a default verification will be performed by comparing the memory pointed to by outputAllocations
    // and the contents of expectedOutputs. Otherwise the function pointed to by verifyIO will be called.
    // If true is returned, then the test case is assumed to have passed, if false is returned, then the test
    // case is assumed to have failed. Exact meaning of failure can be customized with failResult.
    VerifyIOFunc verifyIO;
    ComputeVerifyBinaryFunc verifyBinary;
    SpirvVersion spirvVersion;
    bool coherentMemory;
    bool usesPhysStorageBuffer;
    const bool graphicsFeaturesRequired;

    ComputeShaderSpec(void)
        : entryPoint("main")
        , pushConstants(nullptr)
        , requestedVulkanFeatures()
        , failResult(QP_TEST_RESULT_FAIL)
        , failMessage("Output doesn't match with expected")
        , verifyIO(nullptr)
        , verifyBinary(nullptr)
        , spirvVersion(SPIRV_VERSION_1_0)
        , coherentMemory(false)
        , usesPhysStorageBuffer(false)
        , graphicsFeaturesRequired(false)
    {
    }
};

/*--------------------------------------------------------------------*//*!
 * \brief Helper functions for SPIR-V assembly shared by various tests
 *//*--------------------------------------------------------------------*/

std::string getComputeAsmShaderPreamble(const std::string &capabilities = "", const std::string &extensions = "",
                                        const std::string &exeModes = "", const std::string &extraEntryPoints = "",
                                        const std::string &extraEntryPointsArguments = "");
const char *getComputeAsmShaderPreambleWithoutLocalSize(void);
std::string getComputeAsmCommonTypes(std::string blockStorageClass = "Uniform");
const char *getComputeAsmCommonInt64Types(void);

/*--------------------------------------------------------------------*//*!
 * Declares two uniform variables (indata, outdata) of type
 * "struct { float[] }". Depends on type "f32arr" (for "float[]").
 *//*--------------------------------------------------------------------*/
std::string getComputeAsmInputOutputBuffer(std::string blockStorageClass = "Uniform");
/*--------------------------------------------------------------------*//*!
 * Declares buffer type and layout for uniform variables indata and
 * outdata. Both of them are SSBO bounded to descriptor set 0.
 * indata is at binding point 0, while outdata is at 1.
 *//*--------------------------------------------------------------------*/
std::string getComputeAsmInputOutputBufferTraits(std::string blockStorageClass = "BufferBlock");

bool verifyOutput(const std::vector<Resource> &, const std::vector<AllocationSp> &outputAllocs,
                  const std::vector<Resource> &expectedOutputs, tcu::TestLog &log);

// Creates vertex-shader assembly by specializing a boilerplate StringTemplate

std::string makeComputeShaderAssembly(const std::map<std::string, std::string> &fragments);

} // namespace SpirVAssembly
} // namespace vkt

#endif // _VKTSPVASMCOMPUTESHADERTESTUTIL_HPP
