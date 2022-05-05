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
 * \brief SPIR-V tests for VK_AMD_shader_trinary_minmax.
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmTrinaryMinMaxTests.hpp"
#include "vktTestCase.hpp"

#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"

#include "tcuStringTemplate.hpp"
#include "tcuFloat.hpp"
#include "tcuMaybe.hpp"

#include "deStringUtil.hpp"
#include "deRandom.hpp"
#include "deMemory.h"

#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <algorithm>
#include <array>
#include <memory>

namespace vkt
{
namespace SpirVAssembly
{

namespace
{

enum class OperationType
{
	MIN = 0,
	MAX = 1,
	MID = 2,
};

enum class BaseType
{
	TYPE_INT = 0,
	TYPE_UINT,
	TYPE_FLOAT,
};

// The numeric value is the size in bytes.
enum class TypeSize
{
	SIZE_8BIT	= 1,
	SIZE_16BIT	= 2,
	SIZE_32BIT	= 4,
	SIZE_64BIT	= 8,
};

// The numeric value is the number of components.
enum class AggregationType
{
	SCALAR	= 1,
	VEC2	= 2,
	VEC3	= 3,
	VEC4	= 4,
};

struct TestParams
{
	OperationType	operation;
	BaseType		baseType;
	TypeSize		typeSize;
	AggregationType	aggregation;
	deUint32		randomSeed;

	deUint32		operandSize			() const;	// In bytes.
	deUint32		numComponents		() const;	// Number of components.
	deUint32		effectiveComponents	() const;	// Effective number of components for size calculation.
	deUint32		componentSize		() const;	// In bytes.
};

deUint32 TestParams::operandSize () const
{
	return (effectiveComponents() * componentSize());
}

deUint32 TestParams::numComponents () const
{
	return static_cast<deUint32>(aggregation);
}

deUint32 TestParams::effectiveComponents () const
{
	return static_cast<deUint32>((aggregation == AggregationType::VEC3) ? AggregationType::VEC4 : aggregation);
}

deUint32 TestParams::componentSize () const
{
	return static_cast<deUint32>(typeSize);
}

template <class T>
T min3(T op1, T op2, T op3)
{
	return std::min({op1, op2, op3});
}

template <class T>
T max3(T op1, T op2, T op3)
{
	return std::max({op1, op2, op3});
}

template <class T>
T mid3(T op1, T op2, T op3)
{
	std::array<T, 3> aux{{op1, op2, op3}};
	std::sort(begin(aux), end(aux));
	return aux[1];
}

class OperationManager
{
public:
	// Operation and component index in case of error.
	using OperationComponent	= std::pair<deUint32, deUint32>;
	using ComparisonError		= tcu::Maybe<OperationComponent>;

					OperationManager	(const TestParams& params);
	void			genInputBuffer		(void* bufferPtr, deUint32 numOperations);
	void			calculateResult		(void* referenceBuffer, void* inputBuffer, deUint32 numOperations);
	ComparisonError	compareResults		(void* referenceBuffer, void* resultsBuffer, deUint32 numOperations);

private:
	using GenerateCompFunc = void (*)(de::Random&, void*); // Write a generated component to the given location.

	// Generator variants to populate input buffer.
	static void genInt8		(de::Random& rnd, void* ptr) { *reinterpret_cast<deInt8*>(ptr) = static_cast<deInt8>(rnd.getUint8()); }
	static void genUint8	(de::Random& rnd, void* ptr) { *reinterpret_cast<deUint8*>(ptr) = rnd.getUint8(); }
	static void genInt16	(de::Random& rnd, void* ptr) { *reinterpret_cast<deInt16*>(ptr) = static_cast<deInt16>(rnd.getUint16()); }
	static void genUint16	(de::Random& rnd, void* ptr) { *reinterpret_cast<deUint16*>(ptr) = rnd.getUint16(); }
	static void genInt32	(de::Random& rnd, void* ptr) { *reinterpret_cast<deInt32*>(ptr) = static_cast<deInt32>(rnd.getUint32()); }
	static void genUint32	(de::Random& rnd, void* ptr) { *reinterpret_cast<deUint32*>(ptr) = rnd.getUint32(); }
	static void genInt64	(de::Random& rnd, void* ptr) { *reinterpret_cast<deInt64*>(ptr) = static_cast<deInt64>(rnd.getUint64()); }
	static void genUint64	(de::Random& rnd, void* ptr) { *reinterpret_cast<deUint64*>(ptr) = rnd.getUint64(); }

	// Helper template for float generators.
	// T must be a tcu::Float instantiation.
	// Attempts to generate +-Inf once every 10 times and avoid denormals.
	template <class T>
	static inline void genFloat (de::Random& rnd, void *ptr)
	{
		T* valuePtr = reinterpret_cast<T*>(ptr);
		if (rnd.getInt(1, 10) == 1)
			*valuePtr = T::inf(rnd.getBool() ? 1 : -1);
		else {
			do {
				*valuePtr = T{rnd.getDouble(T::largestNormal(-1).asDouble(), T::largestNormal(1).asDouble())};
			} while (valuePtr->isDenorm());
		}
	}

	static void genFloat16	(de::Random& rnd, void* ptr) { genFloat<tcu::Float16>(rnd, ptr); }
	static void genFloat32	(de::Random& rnd, void* ptr) { genFloat<tcu::Float32>(rnd, ptr); }
	static void genFloat64	(de::Random& rnd, void* ptr) { genFloat<tcu::Float64>(rnd, ptr); }

	// An operation function writes an output value given 3 input values.
	using OperationFunc = void (*)(void*, const void*, const void*, const void*);

	// Helper template used below.
	template <class T, class F>
	static inline void runOpFunc (F f, void* out, const void* in1, const void* in2, const void* in3)
	{
		*reinterpret_cast<T*>(out) = f(*reinterpret_cast<const T*>(in1), *reinterpret_cast<const T*>(in2), *reinterpret_cast<const T*>(in3));
	}

	// Apply an operation in software to a given group of components and calculate result.
	static void minInt8		(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deInt8>		(min3<deInt8>,			out, in1, in2, in3); }
	static void maxInt8		(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deInt8>		(max3<deInt8>,			out, in1, in2, in3); }
	static void midInt8		(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deInt8>		(mid3<deInt8>,			out, in1, in2, in3); }
	static void minUint8	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deUint8>		(min3<deUint8>,			out, in1, in2, in3); }
	static void maxUint8	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deUint8>		(max3<deUint8>,			out, in1, in2, in3); }
	static void midUint8	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deUint8>		(mid3<deUint8>,			out, in1, in2, in3); }
	static void minInt16	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deInt16>		(min3<deInt16>,			out, in1, in2, in3); }
	static void maxInt16	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deInt16>		(max3<deInt16>,			out, in1, in2, in3); }
	static void midInt16	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deInt16>		(mid3<deInt16>,			out, in1, in2, in3); }
	static void minUint16	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deUint16>	(min3<deUint16>,		out, in1, in2, in3); }
	static void maxUint16	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deUint16>	(max3<deUint16>,		out, in1, in2, in3); }
	static void midUint16	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deUint16>	(mid3<deUint16>,		out, in1, in2, in3); }
	static void minInt32	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deInt32>		(min3<deInt32>,			out, in1, in2, in3); }
	static void maxInt32	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deInt32>		(max3<deInt32>,			out, in1, in2, in3); }
	static void midInt32	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deInt32>		(mid3<deInt32>,			out, in1, in2, in3); }
	static void minUint32	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deUint32>	(min3<deUint32>,		out, in1, in2, in3); }
	static void maxUint32	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deUint32>	(max3<deUint32>,		out, in1, in2, in3); }
	static void midUint32	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deUint32>	(mid3<deUint32>,		out, in1, in2, in3); }
	static void minInt64	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deInt64>		(min3<deInt64>,			out, in1, in2, in3); }
	static void maxInt64	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deInt64>		(max3<deInt64>,			out, in1, in2, in3); }
	static void midInt64	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deInt64>		(mid3<deInt64>,			out, in1, in2, in3); }
	static void minUint64	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deUint64>	(min3<deUint64>,		out, in1, in2, in3); }
	static void maxUint64	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deUint64>	(max3<deUint64>,		out, in1, in2, in3); }
	static void midUint64	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<deUint64>	(mid3<deUint64>,		out, in1, in2, in3); }
	static void minFloat16	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<tcu::Float16>(min3<tcu::Float16>,	out, in1, in2, in3); }
	static void maxFloat16	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<tcu::Float16>(max3<tcu::Float16>,	out, in1, in2, in3); }
	static void midFloat16	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<tcu::Float16>(mid3<tcu::Float16>,	out, in1, in2, in3); }
	static void minFloat32	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<tcu::Float32>(min3<tcu::Float32>,	out, in1, in2, in3); }
	static void maxFloat32	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<tcu::Float32>(max3<tcu::Float32>,	out, in1, in2, in3); }
	static void midFloat32	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<tcu::Float32>(mid3<tcu::Float32>,	out, in1, in2, in3); }
	static void minFloat64	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<tcu::Float64>(min3<tcu::Float64>,	out, in1, in2, in3); }
	static void maxFloat64	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<tcu::Float64>(max3<tcu::Float64>,	out, in1, in2, in3); }
	static void midFloat64	(void* out, const void* in1, const void* in2, const void* in3) { runOpFunc<tcu::Float64>(mid3<tcu::Float64>,	out, in1, in2, in3); }

	// Case for accessing the functions map.
	struct Case
	{
		BaseType		type;
		TypeSize		size;
		OperationType	operation;

		// This is required for sorting in the map.
		bool operator< (const Case& other) const
		{
			return (toArray() < other.toArray());
		}

	private:
		std::array<int, 3> toArray () const
		{
			return std::array<int, 3>{{static_cast<int>(type), static_cast<int>(size), static_cast<int>(operation)}};
		}
	};

	// Helper map to correctly choose the right generator and operation function for the specific case being tested.
	using FuncPair	= std::pair<GenerateCompFunc, OperationFunc>;
	using CaseMap	= std::map<Case, FuncPair>;

	static const CaseMap	kFunctionsMap;

	GenerateCompFunc		m_chosenGenerator;
	OperationFunc			m_chosenOperation;
	de::Random				m_random;

	const deUint32			m_operandSize;
	const deUint32			m_numComponents;
	const deUint32			m_componentSize;
};

// This map is used to choose how to generate inputs for each case and which operation to run on the CPU to calculate the reference
// results for the generated inputs.
const OperationManager::CaseMap OperationManager::kFunctionsMap =
{
	{ { BaseType::TYPE_INT,		TypeSize::SIZE_8BIT,	OperationType::MIN }, { genInt8,	minInt8		} },
	{ { BaseType::TYPE_INT,		TypeSize::SIZE_8BIT,	OperationType::MAX }, { genInt8,	maxInt8		} },
	{ { BaseType::TYPE_INT,		TypeSize::SIZE_8BIT,	OperationType::MID }, { genInt8,	midInt8		} },
	{ { BaseType::TYPE_INT,		TypeSize::SIZE_16BIT,	OperationType::MIN }, { genInt16,	minInt16	} },
	{ { BaseType::TYPE_INT,		TypeSize::SIZE_16BIT,	OperationType::MAX }, { genInt16,	maxInt16	} },
	{ { BaseType::TYPE_INT,		TypeSize::SIZE_16BIT,	OperationType::MID }, { genInt16,	midInt16	} },
	{ { BaseType::TYPE_INT,		TypeSize::SIZE_32BIT,	OperationType::MIN }, { genInt32,	minInt32	} },
	{ { BaseType::TYPE_INT,		TypeSize::SIZE_32BIT,	OperationType::MAX }, { genInt32,	maxInt32	} },
	{ { BaseType::TYPE_INT,		TypeSize::SIZE_32BIT,	OperationType::MID }, { genInt32,	midInt32	} },
	{ { BaseType::TYPE_INT,		TypeSize::SIZE_64BIT,	OperationType::MIN }, { genInt64,	minInt64	} },
	{ { BaseType::TYPE_INT,		TypeSize::SIZE_64BIT,	OperationType::MAX }, { genInt64,	maxInt64	} },
	{ { BaseType::TYPE_INT,		TypeSize::SIZE_64BIT,	OperationType::MID }, { genInt64,	midInt64	} },
	{ { BaseType::TYPE_UINT,	TypeSize::SIZE_8BIT,	OperationType::MIN }, { genUint8,	minUint8	} },
	{ { BaseType::TYPE_UINT,	TypeSize::SIZE_8BIT,	OperationType::MAX }, { genUint8,	maxUint8	} },
	{ { BaseType::TYPE_UINT,	TypeSize::SIZE_8BIT,	OperationType::MID }, { genUint8,	midUint8	} },
	{ { BaseType::TYPE_UINT,	TypeSize::SIZE_16BIT,	OperationType::MIN }, { genUint16,	minUint16	} },
	{ { BaseType::TYPE_UINT,	TypeSize::SIZE_16BIT,	OperationType::MAX }, { genUint16,	maxUint16	} },
	{ { BaseType::TYPE_UINT,	TypeSize::SIZE_16BIT,	OperationType::MID }, { genUint16,	midUint16	} },
	{ { BaseType::TYPE_UINT,	TypeSize::SIZE_32BIT,	OperationType::MIN }, { genUint32,	minUint32	} },
	{ { BaseType::TYPE_UINT,	TypeSize::SIZE_32BIT,	OperationType::MAX }, { genUint32,	maxUint32	} },
	{ { BaseType::TYPE_UINT,	TypeSize::SIZE_32BIT,	OperationType::MID }, { genUint32,	midUint32	} },
	{ { BaseType::TYPE_UINT,	TypeSize::SIZE_64BIT,	OperationType::MIN }, { genUint64,	minUint64	} },
	{ { BaseType::TYPE_UINT,	TypeSize::SIZE_64BIT,	OperationType::MAX }, { genUint64,	maxUint64	} },
	{ { BaseType::TYPE_UINT,	TypeSize::SIZE_64BIT,	OperationType::MID }, { genUint64,	midUint64	} },
	{ { BaseType::TYPE_FLOAT,	TypeSize::SIZE_16BIT,	OperationType::MIN }, { genFloat16,	minFloat16	} },
	{ { BaseType::TYPE_FLOAT,	TypeSize::SIZE_16BIT,	OperationType::MAX }, { genFloat16,	maxFloat16	} },
	{ { BaseType::TYPE_FLOAT,	TypeSize::SIZE_16BIT,	OperationType::MID }, { genFloat16,	midFloat16	} },
	{ { BaseType::TYPE_FLOAT,	TypeSize::SIZE_32BIT,	OperationType::MIN }, { genFloat32,	minFloat32	} },
	{ { BaseType::TYPE_FLOAT,	TypeSize::SIZE_32BIT,	OperationType::MAX }, { genFloat32,	maxFloat32	} },
	{ { BaseType::TYPE_FLOAT,	TypeSize::SIZE_32BIT,	OperationType::MID }, { genFloat32,	midFloat32	} },
	{ { BaseType::TYPE_FLOAT,	TypeSize::SIZE_64BIT,	OperationType::MIN }, { genFloat64,	minFloat64	} },
	{ { BaseType::TYPE_FLOAT,	TypeSize::SIZE_64BIT,	OperationType::MAX }, { genFloat64,	maxFloat64	} },
	{ { BaseType::TYPE_FLOAT,	TypeSize::SIZE_64BIT,	OperationType::MID }, { genFloat64,	midFloat64	} },
};

OperationManager::OperationManager (const TestParams& params)
	: m_chosenGenerator	{nullptr}
	, m_chosenOperation	{nullptr}
	, m_random			{params.randomSeed}
	, m_operandSize		{params.operandSize()}
	, m_numComponents	{params.numComponents()}
	, m_componentSize	{params.componentSize()}
{
	// Choose generator and CPU operation from the map.
	const Case paramCase{params.baseType, params.typeSize, params.operation};
	const auto iter = kFunctionsMap.find(paramCase);

	DE_ASSERT(iter != kFunctionsMap.end());
	m_chosenGenerator = iter->second.first;
	m_chosenOperation = iter->second.second;
}

// See TrinaryMinMaxCase::initPrograms for a description of the input buffer format.
// Generates inputs with the chosen generator.
void OperationManager::genInputBuffer (void* bufferPtr, deUint32 numOperations)
{
	const deUint32	numOperands	= numOperations * 3u;
	char*			byteBuffer	= reinterpret_cast<char*>(bufferPtr);

	for (deUint32 opIdx = 0u; opIdx < numOperands; ++opIdx)
	{
		char* compPtr = byteBuffer;
		for (deUint32 compIdx = 0u; compIdx < m_numComponents; ++compIdx)
		{
			m_chosenGenerator(m_random, reinterpret_cast<void*>(compPtr));
			compPtr += m_componentSize;
		}
		byteBuffer += m_operandSize;
	}
}

// See TrinaryMinMaxCase::initPrograms for a description of the input and output buffer formats.
// Calculates reference results on the CPU using the chosen operation and the input buffer.
void OperationManager::calculateResult (void* referenceBuffer, void* inputBuffer, deUint32 numOperations)
{
	char* outputByte	= reinterpret_cast<char*>(referenceBuffer);
	char* inputByte		= reinterpret_cast<char*>(inputBuffer);

	for (deUint32 opIdx = 0u; opIdx < numOperations; ++opIdx)
	{
		char* res = outputByte;
		char* op1 = inputByte;
		char* op2 = inputByte + m_operandSize;
		char* op3 = inputByte + m_operandSize * 2u;

		for (deUint32 compIdx = 0u; compIdx < m_numComponents; ++compIdx)
		{
			m_chosenOperation(
				reinterpret_cast<void*>(res),
				reinterpret_cast<void*>(op1),
				reinterpret_cast<void*>(op2),
				reinterpret_cast<void*>(op3));

			res += m_componentSize;
			op1 += m_componentSize;
			op2 += m_componentSize;
			op3 += m_componentSize;
		}

		outputByte	+= m_operandSize;
		inputByte	+= m_operandSize * 3u;
	}
}

// See TrinaryMinMaxCase::initPrograms for a description of the output buffer format.
OperationManager::ComparisonError OperationManager::compareResults (void* referenceBuffer, void* resultsBuffer, deUint32 numOperations)
{
	char* referenceBytes	= reinterpret_cast<char*>(referenceBuffer);
	char* resultsBytes		= reinterpret_cast<char*>(resultsBuffer);

	for (deUint32 opIdx = 0u; opIdx < numOperations; ++opIdx)
	{
		char *refCompBytes = referenceBytes;
		char *resCompBytes = resultsBytes;

		for (deUint32 compIdx = 0u; compIdx < m_numComponents; ++compIdx)
		{
			if (deMemCmp(refCompBytes, resCompBytes, m_componentSize) != 0)
				return tcu::just(OperationComponent(opIdx, compIdx));
			refCompBytes += m_componentSize;
			resCompBytes += m_componentSize;
		}
		referenceBytes += m_operandSize;
		resultsBytes += m_operandSize;
	}

	return tcu::nothing<OperationComponent>();
}

class TrinaryMinMaxCase : public vkt::TestCase
{
public:
	using ReplacementsMap = std::map<std::string, std::string>;

							TrinaryMinMaxCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params);
	virtual					~TrinaryMinMaxCase		(void) {}

	virtual void			initPrograms			(vk::SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance			(Context& context) const;
	virtual void			checkSupport			(Context& context) const;
	ReplacementsMap			getSpirVReplacements	(void) const;

	static const deUint32	kArraySize;
private:
	TestParams				m_params;
};

const deUint32 TrinaryMinMaxCase::kArraySize = 100u;

class TrinaryMinMaxInstance : public vkt::TestInstance
{
public:
								TrinaryMinMaxInstance	(Context& context, const TestParams& params);
	virtual						~TrinaryMinMaxInstance	(void) {}

	virtual tcu::TestStatus		iterate					(void);

private:
	TestParams	m_params;
};

TrinaryMinMaxCase::TrinaryMinMaxCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
	: vkt::TestCase	(testCtx, name, description)
	, m_params		(params)
{}

TestInstance* TrinaryMinMaxCase::createInstance (Context& context) const
{
	return new TrinaryMinMaxInstance{context, m_params};
}

void TrinaryMinMaxCase::checkSupport (Context& context) const
{
	// These are always required.
	context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");
	context.requireDeviceFunctionality("VK_KHR_storage_buffer_storage_class");
	context.requireDeviceFunctionality("VK_AMD_shader_trinary_minmax");

	const auto devFeatures			= context.getDeviceFeatures();
	const auto storage16BitFeatures	= context.get16BitStorageFeatures();
	const auto storage8BitFeatures	= context.get8BitStorageFeatures();
	const auto shaderFeatures		= context.getShaderFloat16Int8Features();

	// Storage features.
	if (m_params.typeSize == TypeSize::SIZE_8BIT)
	{
		// We will be using 8-bit types in storage buffers.
		context.requireDeviceFunctionality("VK_KHR_8bit_storage");
		if (!storage8BitFeatures.storageBuffer8BitAccess)
			TCU_THROW(NotSupportedError, "8-bit storage buffer access not supported");
	}
	else if (m_params.typeSize == TypeSize::SIZE_16BIT)
	{
		// We will be using 16-bit types in storage buffers.
		context.requireDeviceFunctionality("VK_KHR_16bit_storage");
		if (!storage16BitFeatures.storageBuffer16BitAccess)
			TCU_THROW(NotSupportedError, "16-bit storage buffer access not supported");
	}

	// Shader type features.
	if (m_params.baseType == BaseType::TYPE_INT || m_params.baseType == BaseType::TYPE_UINT)
	{
		if (m_params.typeSize == TypeSize::SIZE_8BIT && !shaderFeatures.shaderInt8)
			TCU_THROW(NotSupportedError, "8-bit integers not supported in shaders");
		else if (m_params.typeSize == TypeSize::SIZE_16BIT && !devFeatures.shaderInt16)
			TCU_THROW(NotSupportedError, "16-bit integers not supported in shaders");
		else if (m_params.typeSize == TypeSize::SIZE_64BIT && !devFeatures.shaderInt64)
			TCU_THROW(NotSupportedError, "64-bit integers not supported in shaders");
	}
	else // BaseType::TYPE_FLOAT
	{
		DE_ASSERT(m_params.typeSize != TypeSize::SIZE_8BIT);
		if (m_params.typeSize == TypeSize::SIZE_16BIT && !shaderFeatures.shaderFloat16)
			TCU_THROW(NotSupportedError, "16-bit floats not supported in shaders");
		else if (m_params.typeSize == TypeSize::SIZE_64BIT && !devFeatures.shaderFloat64)
			TCU_THROW(NotSupportedError, "64-bit floats not supported in shaders");
	}
}

TrinaryMinMaxCase::ReplacementsMap TrinaryMinMaxCase::getSpirVReplacements (void) const
{
	ReplacementsMap replacements;

	// Capabilities and extensions.
	if (m_params.baseType == BaseType::TYPE_INT || m_params.baseType == BaseType::TYPE_UINT)
	{
		if (m_params.typeSize == TypeSize::SIZE_8BIT)
			replacements["CAPABILITIES"]	+= "OpCapability Int8\n";
		else if (m_params.typeSize == TypeSize::SIZE_16BIT)
			replacements["CAPABILITIES"]	+= "OpCapability Int16\n";
		else if (m_params.typeSize == TypeSize::SIZE_64BIT)
			replacements["CAPABILITIES"]	+= "OpCapability Int64\n";
	}
	else // BaseType::TYPE_FLOAT
	{
		if (m_params.typeSize == TypeSize::SIZE_16BIT)
			replacements["CAPABILITIES"]	+= "OpCapability Float16\n";
		else if (m_params.typeSize == TypeSize::SIZE_64BIT)
			replacements["CAPABILITIES"]	+= "OpCapability Float64\n";
	}

	if (m_params.typeSize == TypeSize::SIZE_8BIT)
	{
		replacements["CAPABILITIES"]		+= "OpCapability StorageBuffer8BitAccess\n";
		replacements["EXTENSIONS"]			+= "OpExtension \"SPV_KHR_8bit_storage\"\n";
	}
	else if (m_params.typeSize == TypeSize::SIZE_16BIT)
	{
		replacements["CAPABILITIES"]		+= "OpCapability StorageBuffer16BitAccess\n";
		replacements["EXTENSIONS"]			+= "OpExtension \"SPV_KHR_16bit_storage\"\n";
	}

	// Operand size in bytes.
	const deUint32 opSize				= m_params.operandSize();
	replacements["OPERAND_SIZE"]		= de::toString(opSize);
	replacements["OPERAND_SIZE_2TIMES"]	= de::toString(opSize * 2u);
	replacements["OPERAND_SIZE_3TIMES"]	= de::toString(opSize * 3u);

	// Array size.
	replacements["ARRAY_SIZE"]			= de::toString(kArraySize);

	// Types and operand type: define the base integer or float type and the vector type if needed, then set the operand type replacement.
	const std::string vecSize	= de::toString(m_params.numComponents());
	const std::string bitSize	= de::toString(m_params.componentSize() * 8u);

	if (m_params.baseType == BaseType::TYPE_INT || m_params.baseType == BaseType::TYPE_UINT)
	{
		const std::string	signBit		= (m_params.baseType == BaseType::TYPE_INT ? "1" : "0");
		const std::string	typePrefix	= (m_params.baseType == BaseType::TYPE_UINT ? "u" : "");
		std::string			baseTypeName;

		// 32-bit integers are already defined in the default shader text.
		if (m_params.typeSize != TypeSize::SIZE_32BIT)
		{
			baseTypeName = typePrefix + "int" + bitSize + "_t";
			replacements["TYPES"] += "%" + baseTypeName + " = OpTypeInt " + bitSize + " " + signBit + "\n";
		}
		else
		{
			baseTypeName = typePrefix + "int";
		}

		if (m_params.aggregation == AggregationType::SCALAR)
		{
			replacements["OPERAND_TYPE"] = "%" + baseTypeName;
		}
		else
		{
			const std::string typeName = "%v" + vecSize + baseTypeName;
			// %v3uint is already defined in the default shader text.
			if (m_params.baseType != BaseType::TYPE_UINT || m_params.typeSize != TypeSize::SIZE_32BIT || m_params.aggregation != AggregationType::VEC3)
			{
				replacements["TYPES"] += typeName + " = OpTypeVector %" + baseTypeName + " " + vecSize + "\n";
			}
			replacements["OPERAND_TYPE"] = typeName;
		}
	}
	else // BaseType::TYPE_FLOAT
	{
		const std::string baseTypeName = "float" + bitSize + "_t";
		replacements["TYPES"] += "%" + baseTypeName + " = OpTypeFloat " + bitSize + "\n";

		if (m_params.aggregation == AggregationType::SCALAR)
		{
			replacements["OPERAND_TYPE"] = "%" + baseTypeName;
		}
		else
		{
			const std::string typeName = "%v" + vecSize + baseTypeName;
			replacements["TYPES"] += typeName + " = OpTypeVector %" + baseTypeName + " " + vecSize + "\n";
			replacements["OPERAND_TYPE"] = typeName;
		}
	}

	// Operation name.
	const static std::vector<std::string> opTypeStr	= { "Min", "Max", "Mid" };
	const static std::vector<std::string> opPrefix	= { "S", "U", "F" };
	replacements["OPERATION_NAME"] = opPrefix[static_cast<int>(m_params.baseType)] + opTypeStr[static_cast<int>(m_params.operation)] + "3AMD";

	return replacements;
}

void TrinaryMinMaxCase::initPrograms (vk::SourceCollections& programCollection) const
{
	// The shader below uses an input buffer at set 0 binding 0 and an output buffer at set 0 binding 1. Their structure is similar
	// to the code below:
	//
	//      struct Operands {
	//              <type> op1;
	//              <type> op2;
	//              <type> op3;
	//      };
	//
	//      layout (set=0, binding=0, std430) buffer InputBlock {
	//              Operands operands[<arraysize>];
	//      };
	//
	//      layout (set=0, binding=1, std430) buffer OutputBlock {
	//              <type> result[<arraysize>];
	//      };
	//
	// Where <type> can be int8_t, uint32_t, float, etc. So in the input buffer the operands are "grouped" per operation and can
	// have several components each and the output buffer contains an array of results, one per trio of input operands.

	std::ostringstream shaderStr;
	shaderStr
		<< "; SPIR-V\n"
		<< "; Version: 1.5\n"
		<< "                            OpCapability Shader\n"
		<< "${CAPABILITIES:opt}"
		<< "                            OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
		<< "                            OpExtension \"SPV_AMD_shader_trinary_minmax\"\n"
		<< "${EXTENSIONS:opt}"
		<< "                  %std450 = OpExtInstImport \"GLSL.std.450\"\n"
		<< "                 %trinary = OpExtInstImport \"SPV_AMD_shader_trinary_minmax\"\n"
		<< "                            OpMemoryModel Logical GLSL450\n"
		<< "                            OpEntryPoint GLCompute %main \"main\" %gl_GlobalInvocationID %output_buffer %input_buffer\n"
		<< "                            OpExecutionMode %main LocalSize 1 1 1\n"
		<< "                            OpDecorate %gl_GlobalInvocationID BuiltIn GlobalInvocationId\n"
		<< "                            OpDecorate %results_array_t ArrayStride ${OPERAND_SIZE}\n"
		<< "                            OpMemberDecorate %OutputBlock 0 Offset 0\n"
		<< "                            OpDecorate %OutputBlock Block\n"
		<< "                            OpDecorate %output_buffer DescriptorSet 0\n"
		<< "                            OpDecorate %output_buffer Binding 1\n"
		<< "                            OpMemberDecorate %Operands 0 Offset 0\n"
		<< "                            OpMemberDecorate %Operands 1 Offset ${OPERAND_SIZE}\n"
		<< "                            OpMemberDecorate %Operands 2 Offset ${OPERAND_SIZE_2TIMES}\n"
		<< "                            OpDecorate %_arr_Operands_arraysize ArrayStride ${OPERAND_SIZE_3TIMES}\n"
		<< "                            OpMemberDecorate %InputBlock 0 Offset 0\n"
		<< "                            OpDecorate %InputBlock Block\n"
		<< "                            OpDecorate %input_buffer DescriptorSet 0\n"
		<< "                            OpDecorate %input_buffer Binding 0\n"
		<< "                            OpDecorate %gl_WorkGroupSize BuiltIn WorkgroupSize\n"
		<< "                    %void = OpTypeVoid\n"
		<< "                %voidfunc = OpTypeFunction %void\n"
		<< "                     %int = OpTypeInt 32 1\n"
		<< "                    %uint = OpTypeInt 32 0\n"
		<< "                  %v3uint = OpTypeVector %uint 3\n"
		<< "${TYPES:opt}"
		<< "                   %int_0 = OpConstant %int 0\n"
		<< "                   %int_1 = OpConstant %int 1\n"
		<< "                   %int_2 = OpConstant %int 2\n"
		<< "                  %uint_1 = OpConstant %uint 1\n"
		<< "                  %uint_0 = OpConstant %uint 0\n"
		<< "               %arraysize = OpConstant %uint ${ARRAY_SIZE}\n"
		<< "      %_ptr_Function_uint = OpTypePointer Function %uint\n"
		<< "       %_ptr_Input_v3uint = OpTypePointer Input %v3uint\n"
		<< "   %gl_GlobalInvocationID = OpVariable %_ptr_Input_v3uint Input\n"
		<< "         %_ptr_Input_uint = OpTypePointer Input %uint\n"
		<< "         %results_array_t = OpTypeArray ${OPERAND_TYPE} %arraysize\n"
		<< "                %Operands = OpTypeStruct ${OPERAND_TYPE} ${OPERAND_TYPE} ${OPERAND_TYPE}\n"
		<< " %_arr_Operands_arraysize = OpTypeArray %Operands %arraysize\n"
		<< "             %OutputBlock = OpTypeStruct %results_array_t\n"
		<< "              %InputBlock = OpTypeStruct %_arr_Operands_arraysize\n"
		<< "%_ptr_Uniform_OutputBlock = OpTypePointer StorageBuffer %OutputBlock\n"
		<< " %_ptr_Uniform_InputBlock = OpTypePointer StorageBuffer %InputBlock\n"
		<< "           %output_buffer = OpVariable %_ptr_Uniform_OutputBlock StorageBuffer\n"
		<< "            %input_buffer = OpVariable %_ptr_Uniform_InputBlock StorageBuffer\n"
		<< "              %optype_ptr = OpTypePointer StorageBuffer ${OPERAND_TYPE}\n"
		<< "        %gl_WorkGroupSize = OpConstantComposite %v3uint %uint_1 %uint_1 %uint_1\n"
		<< "                    %main = OpFunction %void None %voidfunc\n"
		<< "               %mainlabel = OpLabel\n"
		<< "                 %gidxptr = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %uint_0\n"
		<< "                     %idx = OpLoad %uint %gidxptr\n"
		<< "                  %op1ptr = OpAccessChain %optype_ptr %input_buffer %int_0 %idx %int_0\n"
		<< "                     %op1 = OpLoad ${OPERAND_TYPE} %op1ptr\n"
		<< "                  %op2ptr = OpAccessChain %optype_ptr %input_buffer %int_0 %idx %int_1\n"
		<< "                     %op2 = OpLoad ${OPERAND_TYPE} %op2ptr\n"
		<< "                  %op3ptr = OpAccessChain %optype_ptr %input_buffer %int_0 %idx %int_2\n"
		<< "                     %op3 = OpLoad ${OPERAND_TYPE} %op3ptr\n"
		<< "                  %result = OpExtInst ${OPERAND_TYPE} %trinary ${OPERATION_NAME} %op1 %op2 %op3\n"
		<< "               %resultptr = OpAccessChain %optype_ptr %output_buffer %int_0 %idx\n"
		<< "                            OpStore %resultptr %result\n"
		<< "                            OpReturn\n"
		<< "                            OpFunctionEnd\n"
		;

	const tcu::StringTemplate		shaderTemplate	{shaderStr.str()};
	const vk::SpirVAsmBuildOptions	buildOptions	{VK_MAKE_VERSION(1, 2, 0), vk::SPIRV_VERSION_1_5};

	programCollection.spirvAsmSources.add("comp", &buildOptions) << shaderTemplate.specialize(getSpirVReplacements());
}

TrinaryMinMaxInstance::TrinaryMinMaxInstance (Context& context, const TestParams& params)
	: vkt::TestInstance	(context)
	, m_params			(params)
{}

tcu::TestStatus TrinaryMinMaxInstance::iterate (void)
{
	const auto&	vkd			= m_context.getDeviceInterface();
	const auto	device		= m_context.getDevice();
	auto&		allocator	= m_context.getDefaultAllocator();
	const auto	queue		= m_context.getUniversalQueue();
	const auto	queueIndex	= m_context.getUniversalQueueFamilyIndex();

	constexpr auto kNumOperations = TrinaryMinMaxCase::kArraySize;

	const vk::VkDeviceSize kInputBufferSize		= static_cast<vk::VkDeviceSize>(kNumOperations * 3u * m_params.operandSize());
	const vk::VkDeviceSize kOutputBufferSize	= static_cast<vk::VkDeviceSize>(kNumOperations * m_params.operandSize()); // Single output per operation.

	// Create input, output and reference buffers.
	auto inputBufferInfo	= vk::makeBufferCreateInfo(kInputBufferSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	auto outputBufferInfo	= vk::makeBufferCreateInfo(kOutputBufferSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	vk::BufferWithMemory	inputBuffer		{vkd, device, allocator, inputBufferInfo,	vk::MemoryRequirement::HostVisible};
	vk::BufferWithMemory	outputBuffer	{vkd, device, allocator, outputBufferInfo,	vk::MemoryRequirement::HostVisible};
	std::unique_ptr<char[]>	referenceBuffer	{new char[static_cast<size_t>(kOutputBufferSize)]};

	// Fill buffers with initial contents.
	auto& inputAlloc	= inputBuffer.getAllocation();
	auto& outputAlloc	= outputBuffer.getAllocation();

	void* inputBufferPtr		= static_cast<deUint8*>(inputAlloc.getHostPtr()) + inputAlloc.getOffset();
	void* outputBufferPtr		= static_cast<deUint8*>(outputAlloc.getHostPtr()) + outputAlloc.getOffset();
	void* referenceBufferPtr	= referenceBuffer.get();

	deMemset(inputBufferPtr, 0, static_cast<size_t>(kInputBufferSize));
	deMemset(outputBufferPtr, 0, static_cast<size_t>(kOutputBufferSize));
	deMemset(referenceBufferPtr, 0, static_cast<size_t>(kOutputBufferSize));

	// Generate input buffer and calculate reference results.
	OperationManager opMan{m_params};
	opMan.genInputBuffer(inputBufferPtr, kNumOperations);
	opMan.calculateResult(referenceBufferPtr, inputBufferPtr, kNumOperations);

	// Flush buffer memory before starting.
	vk::flushAlloc(vkd, device, inputAlloc);
	vk::flushAlloc(vkd, device, outputAlloc);

	// Descriptor set layout.
	vk::DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);
	layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);
	auto descriptorSetLayout = layoutBuilder.build(vkd, device);

	// Descriptor pool.
	vk::DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u);
	auto descriptorPool = poolBuilder.build(vkd, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	// Descriptor set.
	const auto descriptorSet = vk::makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());

	// Update descriptor set using the buffers.
	const auto inputBufferDescriptorInfo	= vk::makeDescriptorBufferInfo(inputBuffer.get(), 0ull, VK_WHOLE_SIZE);
	const auto outputBufferDescriptorInfo	= vk::makeDescriptorBufferInfo(outputBuffer.get(), 0ull, VK_WHOLE_SIZE);

	vk::DescriptorSetUpdateBuilder updateBuilder;
	updateBuilder.writeSingle(descriptorSet.get(), vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &inputBufferDescriptorInfo);
	updateBuilder.writeSingle(descriptorSet.get(), vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputBufferDescriptorInfo);
	updateBuilder.update(vkd, device);

	// Create compute pipeline.
	auto shaderModule = vk::createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0u);
	auto pipelineLayout = vk::makePipelineLayout(vkd, device, descriptorSetLayout.get());

	const vk::VkComputePipelineCreateInfo pipelineCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		nullptr,
		0u,															// flags
		{															// compute shader
			vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
			nullptr,													// const void*							pNext;
			0u,															// VkPipelineShaderStageCreateFlags		flags;
			vk::VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits				stage;
			shaderModule.get(),											// VkShaderModule						module;
			"main",														// const char*							pName;
			nullptr,													// const VkSpecializationInfo*			pSpecializationInfo;
		},
		pipelineLayout.get(),										// layout
		DE_NULL,													// basePipelineHandle
		0,															// basePipelineIndex
	};
	auto pipeline = vk::createComputePipeline(vkd, device, DE_NULL, &pipelineCreateInfo);

	// Synchronization barriers.
	auto inputBufferHostToDevBarrier	= vk::makeBufferMemoryBarrier(vk::VK_ACCESS_HOST_WRITE_BIT, vk::VK_ACCESS_SHADER_READ_BIT, inputBuffer.get(), 0ull, VK_WHOLE_SIZE);
	auto outputBufferHostToDevBarrier	= vk::makeBufferMemoryBarrier(vk::VK_ACCESS_HOST_WRITE_BIT, vk::VK_ACCESS_SHADER_WRITE_BIT, outputBuffer.get(), 0ull, VK_WHOLE_SIZE);
	auto outputBufferDevToHostBarrier	= vk::makeBufferMemoryBarrier(vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_HOST_READ_BIT, outputBuffer.get(), 0ull, VK_WHOLE_SIZE);

	// Command buffer.
	auto cmdPool		= vk::makeCommandPool(vkd, device, queueIndex);
	auto cmdBufferPtr	= vk::allocateCommandBuffer(vkd, device, cmdPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	auto cmdBuffer		= cmdBufferPtr.get();

	// Record and submit commands.
	vk::beginCommandBuffer(vkd, cmdBuffer);
		vkd.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.get());
		vkd.cmdBindDescriptorSets(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.get(), 0, 1u, &descriptorSet.get(), 0u, nullptr);
		vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 1u, &inputBufferHostToDevBarrier, 0u, nullptr);
		vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 1u, &outputBufferHostToDevBarrier, 0u, nullptr);
		vkd.cmdDispatch(cmdBuffer, kNumOperations, 1u, 1u);
		vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u, &outputBufferDevToHostBarrier, 0u, nullptr);
	vk::endCommandBuffer(vkd, cmdBuffer);
	vk::submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Verify output buffer contents.
	vk::invalidateAlloc(vkd, device, outputAlloc);

	const auto error = opMan.compareResults(referenceBufferPtr, outputBufferPtr, kNumOperations);

	if (!error)
		return tcu::TestStatus::pass("Pass");

	std::ostringstream msg;
	msg << "Value mismatch at operation " << error.get().first << " in component " << error.get().second;
	return tcu::TestStatus::fail(msg.str());
}

} // anonymous

tcu::TestCaseGroup* createTrinaryMinMaxGroup (tcu::TestContext& testCtx)
{
	deUint32 seed = 0xFEE768FCu;
	de::MovePtr<tcu::TestCaseGroup> group{new tcu::TestCaseGroup{testCtx, "amd_trinary_minmax", "Tests for VK_AMD_trinary_minmax operations"}};

	static const std::vector<std::pair<OperationType, std::string>> operationTypes =
	{
		{ OperationType::MIN, "min3" },
		{ OperationType::MAX, "max3" },
		{ OperationType::MID, "mid3" },
	};

	static const std::vector<std::pair<BaseType, std::string>> baseTypes =
	{
		{ BaseType::TYPE_INT,	"i" },
		{ BaseType::TYPE_UINT,	"u" },
		{ BaseType::TYPE_FLOAT,	"f" },
	};

	static const std::vector<std::pair<TypeSize, std::string>> typeSizes =
	{
		{ TypeSize::SIZE_8BIT,	"8"		},
		{ TypeSize::SIZE_16BIT,	"16"	},
		{ TypeSize::SIZE_32BIT,	"32"	},
		{ TypeSize::SIZE_64BIT,	"64"	},
	};

	static const std::vector<std::pair<AggregationType, std::string>> aggregationTypes =
	{
		{ AggregationType::SCALAR,	"scalar"	},
		{ AggregationType::VEC2,	"vec2"		},
		{ AggregationType::VEC3,	"vec3"		},
		{ AggregationType::VEC4,	"vec4"		},
	};

	for (const auto& opType : operationTypes)
	{
		const std::string opDesc = "Tests for " + opType.second + " operation";
		de::MovePtr<tcu::TestCaseGroup> opGroup{new tcu::TestCaseGroup{testCtx, opType.second.c_str(), opDesc.c_str()}};

		for (const auto& baseType : baseTypes)
		for (const auto& typeSize : typeSizes)
		{
			// There are no 8-bit floats.
			if (baseType.first == BaseType::TYPE_FLOAT && typeSize.first == TypeSize::SIZE_8BIT)
				continue;

			const std::string typeName = baseType.second + typeSize.second;
			const std::string typeDesc = "Tests using " + typeName + " data";

			de::MovePtr<tcu::TestCaseGroup> typeGroup{new tcu::TestCaseGroup{testCtx, typeName.c_str(), typeDesc.c_str()}};

			for (const auto& aggType : aggregationTypes)
			{
				const TestParams params =
				{
					opType.first,		// OperationType	operation;
					baseType.first,		// BaseType			baseType;
					typeSize.first,		// TypeSize			typeSize;
					aggType.first,		// AggregationType	aggregation;
					seed++,				// deUint32			randomSeed;
				};
				typeGroup->addChild(new TrinaryMinMaxCase{testCtx, aggType.second, "", params});
			}

			opGroup->addChild(typeGroup.release());
		}

		group->addChild(opGroup.release());
	}

	return group.release();
}

} // SpirVAssembly
} // vkt
