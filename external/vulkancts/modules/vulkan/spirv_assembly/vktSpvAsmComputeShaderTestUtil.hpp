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
#include <functional>

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

enum AtomicOpType
{
	OP_ATOMIC_ADD = 0,
	OP_ATOMIC_SUBTRACT,
	OP_ATOMIC_INCREMENT,
	OP_ATOMIC_DECREMENT,
	OP_ATOMIC_LOAD,
	OP_ATOMIC_STORE,
	OP_ATOMIC_EXCHANGE,
	OP_ATOMIC_COMPARE_EXCHANGE,
	OP_ATOMIC_MIN,
	OP_ATOMIC_MAX,
	OP_ATOMIC_AND,
	OP_ATOMIC_OR,
	OP_ATOMIC_XOR,

	OP_ATOMIC_LAST
};

enum BufferType
{
	BUFFERTYPE_INPUT = 0,
	BUFFERTYPE_EXPECTED,
	BUFFERTYPE_ATOMIC_RET,

	BUFFERTYPE_LAST
};

static void fillRandomScalars (de::Random& rnd, deInt32 minValue, deInt32 maxValue, deInt32* dst, deInt32 numValues)
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
						OpAtomicBuffer		(const deUint32 numInputElements, const deUint32 numOuptutElements, const OpAtomicType opAtomic, const BufferType type)
							: m_numInputElements	(numInputElements)
							, m_numOutputElements	(numOuptutElements)
							, m_opAtomic			(opAtomic)
							, m_type				(type)
						{}

	void getBytes (std::vector<deUint8>& bytes) const
	{
		std::vector<deInt32>	inputInts	(m_numInputElements, 0);
		de::Random				rnd			(m_opAtomic);

		fillRandomScalars(rnd, 1, 100, &inputInts.front(), m_numInputElements);

		// Return input values as is
		if (m_type == BUFFERTYPE_INPUT)
		{
			size_t					inputSize	= m_numInputElements * sizeof(deInt32);

			bytes.resize(inputSize);
			deMemcpy(&bytes.front(), &inputInts.front(), inputSize);
		}
		// Calculate expected output values
		else if (m_type == BUFFERTYPE_EXPECTED)
		{
			size_t					outputSize	= m_numOutputElements * sizeof(deInt32);
			bytes.resize(outputSize, 0xffu);

			for (size_t ndx = 0; ndx < m_numInputElements; ndx++)
			{
				deInt32* const bytesAsInt = reinterpret_cast<deInt32*>(&bytes.front());

				switch (m_opAtomic)
				{
					case OPATOMIC_IADD:		bytesAsInt[0] += inputInts[ndx];						break;
					case OPATOMIC_ISUB:		bytesAsInt[0] -= inputInts[ndx];						break;
					case OPATOMIC_IINC:		bytesAsInt[0]++;										break;
					case OPATOMIC_IDEC:		bytesAsInt[0]--;										break;
					case OPATOMIC_LOAD:		bytesAsInt[ndx] = inputInts[ndx];						break;
					case OPATOMIC_STORE:	bytesAsInt[ndx] = inputInts[ndx];						break;
					case OPATOMIC_COMPEX:	bytesAsInt[ndx] = (inputInts[ndx] % 2) == 0 ? -1 : 1;	break;
					default:				DE_FATAL("Unknown OpAtomic type");
				}
			}
		}
		else if (m_type == BUFFERTYPE_ATOMIC_RET)
		{
			bytes.resize(m_numInputElements * sizeof(deInt32), 0xff);

			if (m_opAtomic == OPATOMIC_COMPEX)
			{
				deInt32* const bytesAsInt = reinterpret_cast<deInt32*>(&bytes.front());
				for (size_t ndx = 0; ndx < m_numInputElements; ndx++)
					bytesAsInt[ndx] = inputInts[ndx] % 2;
			}
		}
		else
			DE_FATAL("Unknown buffer type");
	}

	void getPackedBytes (std::vector<deUint8>& bytes) const
	{
		return getBytes(bytes);
	}

	size_t getByteSize (void) const
	{
		switch (m_type)
		{
			case BUFFERTYPE_ATOMIC_RET:
			case BUFFERTYPE_INPUT:
				return m_numInputElements * sizeof(deInt32);
			case BUFFERTYPE_EXPECTED:
				return m_numOutputElements * sizeof(deInt32);
			default:
				DE_FATAL("Unknown buffer type");
				return 0;
		}
	}

	template <int OpAtomic>
	static bool compareWithRetvals (const std::vector<Resource>& inputs, const std::vector<AllocationSp>& outputAllocs, const std::vector<Resource>& expectedOutputs, tcu::TestLog& log)
	{
		if (outputAllocs.size() != 2 || inputs.size() != 1)
			DE_FATAL("Wrong number of buffers to compare");

		for (size_t i = 0; i < outputAllocs.size(); ++i)
		{
			const deUint32*	values = reinterpret_cast<deUint32*>(outputAllocs[i]->getHostPtr());

			if (i == 1 && OpAtomic != OPATOMIC_COMPEX)
			{
				// BUFFERTYPE_ATOMIC_RET for arithmetic operations must be verified manually by matching return values to inputs
				std::vector<deUint8>	inputBytes;
				inputs[0].getBytes(inputBytes);

				const deUint32*			inputValues			= reinterpret_cast<deUint32*>(&inputBytes.front());
				const size_t			inputValuesCount	= inputBytes.size() / sizeof(deUint32);

				// result of all atomic operations
				const deUint32			resultValue			= *reinterpret_cast<deUint32*>(outputAllocs[0]->getHostPtr());

				if (!compareRetVals<OpAtomic>(inputValues, inputValuesCount, resultValue, values))
				{
					log << tcu::TestLog::Message << "Wrong contents of buffer with return values after atomic operation." << tcu::TestLog::EndMessage;
					return false;
				}
			}
			else
			{
				const BufferSp&			expectedOutput = expectedOutputs[i].getBuffer();
				std::vector<deUint8>	expectedBytes;

				expectedOutput->getBytes(expectedBytes);

				if (deMemCmp(&expectedBytes.front(), values, expectedBytes.size()))
				{
					log << tcu::TestLog::Message << "Wrong contents of buffer after atomic operation" << tcu::TestLog::EndMessage;
					return false;
				}
			}
		}
		return true;
	}

	template <int OpAtomic>
	static bool compareRetVals (const deUint32* inputValues, const size_t inputValuesCount, const deUint32 resultValue, const deUint32* returnValues)
	{
		// as the order of execution is undefined, validation of return values for atomic operations is tricky:
		// each inputValue stands for one atomic operation. Iterate through all of
		// done operations in time, each time finding one matching current result and un-doing it.

		std::vector<bool>		operationsUndone (inputValuesCount, false);
		deUint32				currentResult	 = resultValue;

		for (size_t operationUndone = 0; operationUndone < inputValuesCount; ++operationUndone)
		{
			// find which of operations was done at this moment
			size_t ndx;
			for (ndx = 0; ndx < inputValuesCount; ++ndx)
			{
				if (operationsUndone[ndx]) continue;

				deUint32 previousResult = currentResult;

				switch (OpAtomic)
				{
					// operations are undone here, so the actual opeation is reversed
					case OPATOMIC_IADD:		previousResult -= inputValues[ndx];						break;
					case OPATOMIC_ISUB:		previousResult += inputValues[ndx];						break;
					case OPATOMIC_IINC:		previousResult--;										break;
					case OPATOMIC_IDEC:		previousResult++;										break;
					default:				DE_FATAL("Unsupported OpAtomic type for return value compare");
				}

				if (previousResult == returnValues[ndx])
				{
					// found matching operation
					currentResult			= returnValues[ndx];
					operationsUndone[ndx]	= true;
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
	const deUint32		m_numInputElements;
	const deUint32		m_numOutputElements;
	const OpAtomicType	m_opAtomic;
	const BufferType	m_type;
};

// Describes sequence of operations performed on buffer data
struct AtomicOpDesc
{
	size_t			elemIndex;		// Specifies index of element to preform atomic operation on to.
	AtomicOpType	type;			// Specifies atomic operation type to perform.
	double			userData0;		// Specifies additional operation data.
	double			userData1;		// Specifies additional operation data.
};

template<class T>
inline T LoadStore(T a)
{
	return a;
}

template<class T>
inline T Increment(T a)
{
	return ++a;
}

template<class T>
inline T Decrement(T a)
{
	return --a;
}

template<class T>
inline T Add(T a, T b)
{
	return static_cast<T>(a + b);
}

template<class T>
inline T Subtract(T a, T b)
{
	return static_cast<T>(a - b);
}

template<class T>
inline T Min(T a, T b)
{
	return a < b ? a : b;
}

template<class T>
inline T Max(T a, T b)
{
	return a > b ? a : b;
}

template<class T>
inline T And(T a, T b)
{
	return a & b;
}

template<>
inline float And<float>(float, float)
{
	return 0;
}

template<>
inline double And<double>(double, double)
{
	return 0;
}

template<class T>
inline T Or(T a, T b)
{
	return a | b;
}

template<>
inline float Or<float>(float, float)
{
	return 0;
}

template<>
inline double Or<double>(double, double)
{
	return 0;
}

template<class T>
inline T Xor(T a, T b)
{
	return a ^ b;
}

template<>
inline float Xor<float>(float, float)
{
	return 0;
}

template<>
inline double Xor<double>(double, double)
{
	return 0;
}

template<class T>
inline T Exchange(T a)
{
	return a;
}

template<class T>
inline T CompareExchange(T a, T b, T comp)
{
	return a == comp ? b : a;
}

/*----------------------------------------------------------------------------------------*//*!
 * \brief Concrete class for an input/output storage buffer object for atomic operations
 *//*----------------------------------------------------------------------------------------*/
template<class T>
class AtomicBuffer : public BufferInterface
{
public:
	AtomicBuffer	(const std::vector<T>& elements, const std::vector<AtomicOpDesc>& atomicOpDescs)
					: m_elements(elements)
					, m_atomicOpDescs(atomicOpDescs)
					{}

	void getBytes (std::vector<deUint8>& bytes) const
	{
		const size_t	size	= m_elements.size() * sizeof(T);
		const deUint8	initVal	= 0xffu;		// Initial value for all GPU atomic operations
		bytes.resize(size, initVal);

		for (size_t ndx = 0; ndx < m_atomicOpDescs.size(); ndx++)
		{
			const AtomicOpType	type		= m_atomicOpDescs[ndx].type;
			T* const			bytesAsT	= reinterpret_cast<T*>(&bytes.front());
			const size_t		elemNdx		= m_atomicOpDescs[ndx].elemIndex;
			DE_ASSERT(elemNdx < m_elements.size());
			const T				value		= static_cast<T>(m_atomicOpDescs[ndx].userData0);
			const T				comp		= static_cast<T>(m_atomicOpDescs[ndx].userData1);

			switch (type)
			{
			case OP_ATOMIC_LOAD:
			case OP_ATOMIC_STORE:
			{
				bytesAsT[elemNdx]	= LoadStore(value);
				break;
			}
			case OP_ATOMIC_ADD:
			{
				bytesAsT[elemNdx]	= Add(bytesAsT[elemNdx], value);
				break;
			}
			case OP_ATOMIC_SUBTRACT:
			{
				bytesAsT[elemNdx]	= Subtract(bytesAsT[elemNdx], value);
				break;
			}
			case OP_ATOMIC_INCREMENT:
			{
				bytesAsT[elemNdx]	= Increment(bytesAsT[elemNdx]);
				break;
			}
			case OP_ATOMIC_DECREMENT:
			{
				bytesAsT[elemNdx]	= Decrement(bytesAsT[elemNdx]);
				break;
			}
			case OP_ATOMIC_MIN:
			{
				bytesAsT[elemNdx]	= Min(bytesAsT[elemNdx], value);
				break;
			}
			case OP_ATOMIC_MAX:
			{
				bytesAsT[elemNdx]	= Max(bytesAsT[elemNdx], value);
				break;
			}
			case OP_ATOMIC_AND:
			{
				bytesAsT[elemNdx]	= And(bytesAsT[elemNdx], value);
				break;
			}
			case OP_ATOMIC_OR:
			{
				bytesAsT[elemNdx]	= Or(bytesAsT[elemNdx], value);
				break;
			}
			case OP_ATOMIC_XOR:
			{
				bytesAsT[elemNdx]	= Xor(bytesAsT[elemNdx], value);
				break;
			}
			case OP_ATOMIC_EXCHANGE:
			{
				bytesAsT[elemNdx]	= Exchange(value);
				break;
			}
			case OP_ATOMIC_COMPARE_EXCHANGE:
			{
				bytesAsT[elemNdx]	= CompareExchange(bytesAsT[elemNdx], value, comp);
				break;
			}
			default:
				break;
			}
		}
	}

	void getPackedBytes (std::vector<deUint8>& bytes) const
	{
		getBytes(bytes);
	}

	size_t getByteSize (void) const
	{
		return m_elements.size() * sizeof(T);
	}

private:
	std::vector<T>				m_elements;
	std::vector<AtomicOpDesc>	m_atomicOpDescs;
};

/*--------------------------------------------------------------------*//*!
 * \brief Concrete class for an input/output storage buffer object
 *//*--------------------------------------------------------------------*/
template<typename E>
class Buffer : public BufferInterface
{
public:
	Buffer	(const std::vector<E>& elements, deUint32 padding = 0 /* in bytes */)
			: m_elements(elements)
			, m_padding(padding)
			{}

	void getBytes (std::vector<deUint8>& bytes) const
	{
		const size_t	count			= m_elements.size();
		const size_t	perSegmentSize	= sizeof(E) + m_padding;
		const size_t	size			= count * perSegmentSize;

		bytes.resize(size);

		if (m_padding == 0)
		{
			deMemcpy(&bytes.front(), &m_elements.front(), size);
		}
		else
		{
			deMemset(&bytes.front(), 0xff, size);

			for (deUint32 elementIdx = 0; elementIdx < count; ++elementIdx)
				deMemcpy(&bytes[elementIdx * perSegmentSize], &m_elements[elementIdx], sizeof(E));
		}
	}

	void getPackedBytes (std::vector<deUint8>& bytes) const
	{
		const size_t size = m_elements.size() * sizeof(E);

		bytes.resize(size);

		deMemcpy(&bytes.front(), &m_elements.front(), size);
	}

	size_t getByteSize (void) const
	{
		return m_elements.size() * (sizeof(E) + m_padding);
	}

private:
	std::vector<E>		m_elements;
	deUint32			m_padding;
};

DE_STATIC_ASSERT(sizeof(tcu::Vec4) == 4 * sizeof(float));

typedef Buffer<float>		Float32Buffer;
typedef Buffer<deFloat16>	Float16Buffer;
typedef Buffer<double>		Float64Buffer;
typedef Buffer<deInt64>		Int64Buffer;
typedef Buffer<deInt32>		Int32Buffer;
typedef Buffer<deInt16>		Int16Buffer;
typedef Buffer<deInt8>		Int8Buffer;
typedef Buffer<deUint8>		Uint8Buffer;
typedef Buffer<deUint16>	Uint16Buffer;
typedef Buffer<deUint32>	Uint32Buffer;
typedef Buffer<deUint64>	Uint64Buffer;
typedef Buffer<tcu::Vec4>	Vec4Buffer;

typedef bool (*ComputeVerifyBinaryFunc) (const ProgramBinary&	binary);

/*--------------------------------------------------------------------*//*!
 * \brief Specification for a compute shader.
 *
 * This struct bundles SPIR-V assembly code, input and expected output
 * together.
 *//*--------------------------------------------------------------------*/
struct ComputeShaderSpec
{
	std::string								assembly;
	std::string								entryPoint;
	std::vector<Resource>					inputs;
	std::vector<Resource>					outputs;
	vk::VkFormat							inputFormat = vk::VK_FORMAT_R32G32B32A32_SFLOAT;
	tcu::IVec3								numWorkGroups;
	SpecConstants							specConstants;
	BufferSp								pushConstants;
	std::vector<std::string>				extensions;
	VulkanFeatures							requestedVulkanFeatures;
	qpTestResult							failResult;
	std::string								failMessage;
	// If null, a default verification will be performed by comparing the memory pointed to by outputAllocations
	// and the contents of expectedOutputs. Otherwise the function pointed to by verifyIO will be called.
	// If true is returned, then the test case is assumed to have passed, if false is returned, then the test
	// case is assumed to have failed. Exact meaning of failure can be customized with failResult.
	VerifyIOFunc							verifyIO;
	ComputeVerifyBinaryFunc					verifyBinary;
	SpirvVersion							spirvVersion;
	bool									coherentMemory;
	bool									usesPhysStorageBuffer;

											ComputeShaderSpec (void)
												: entryPoint					("main")
												, pushConstants					(DE_NULL)
												, requestedVulkanFeatures		()
												, failResult					(QP_TEST_RESULT_FAIL)
												, failMessage					("Output doesn't match with expected")
												, verifyIO						(DE_NULL)
												, verifyBinary					(DE_NULL)
												, spirvVersion					(SPIRV_VERSION_1_0)
												, coherentMemory				(false)
												, usesPhysStorageBuffer			(false)
											{}
};

/*--------------------------------------------------------------------*//*!
 * \brief Helper functions for SPIR-V assembly shared by various tests
 *//*--------------------------------------------------------------------*/

std::string getComputeAsmShaderPreamble				(const std::string& capabilities				= "",
													 const std::string& extensions					= "",
													 const std::string& exeModes					= "",
													 const std::string& extraEntryPoints			= "",
													 const std::string& extraEntryPointsArguments	= "");
const char* getComputeAsmShaderPreambleWithoutLocalSize         (void);
std::string getComputeAsmCommonTypes				(std::string blockStorageClass = "Uniform");
const char*	getComputeAsmCommonInt64Types			(void);

/*--------------------------------------------------------------------*//*!
 * Declares two uniform variables (indata, outdata) of type
 * "struct { float[] }". Depends on type "f32arr" (for "float[]").
 *//*--------------------------------------------------------------------*/
std::string getComputeAsmInputOutputBuffer			(std::string blockStorageClass = "Uniform");
/*--------------------------------------------------------------------*//*!
 * Declares buffer type and layout for uniform variables indata and
 * outdata. Both of them are SSBO bounded to descriptor set 0.
 * indata is at binding point 0, while outdata is at 1.
 *//*--------------------------------------------------------------------*/
std::string getComputeAsmInputOutputBufferTraits	(std::string blockStorageClass = "BufferBlock");

bool verifyOutput									(const std::vector<Resource>&,
													const std::vector<AllocationSp>&	outputAllocs,
													const std::vector<Resource>&		expectedOutputs,
													tcu::TestLog&						log);

													// Creates vertex-shader assembly by specializing a boilerplate StringTemplate

std::string makeComputeShaderAssembly(const std::map<std::string, std::string>& fragments);

} // SpirVAssembly
} // vkt

#endif // _VKTSPVASMCOMPUTESHADERTESTUTIL_HPP
