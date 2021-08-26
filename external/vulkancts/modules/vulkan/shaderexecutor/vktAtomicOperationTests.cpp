/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2017 Google Inc.
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
 * \brief Atomic operations (OpAtomic*) tests.
 *//*--------------------------------------------------------------------*/

#include "vktAtomicOperationTests.hpp"
#include "vktShaderExecutor.hpp"

#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuResultCollector.hpp"

#include "deFloat16.h"
#include "deMath.hpp"
#include "deStringUtil.hpp"
#include "deSharedPtr.hpp"
#include "deRandom.hpp"
#include "deArrayUtil.hpp"

#include <string>
#include <memory>
#include <cmath>

namespace vkt
{
namespace shaderexecutor
{

namespace
{

using de::UniquePtr;
using de::MovePtr;
using std::vector;

using namespace vk;

enum class AtomicMemoryType
{
	BUFFER = 0,	// Normal buffer.
	SHARED,		// Shared global struct in a compute workgroup.
	REFERENCE,	// Buffer passed as a reference.
};

// Helper struct to indicate the shader type and if it should use shared global memory.
class AtomicShaderType
{
public:
	AtomicShaderType (glu::ShaderType type, AtomicMemoryType memoryType)
		: m_type				(type)
		, m_atomicMemoryType	(memoryType)
	{
		// Shared global memory can only be set to true with compute shaders.
		DE_ASSERT(memoryType != AtomicMemoryType::SHARED || type == glu::SHADERTYPE_COMPUTE);
	}

	glu::ShaderType		getType					(void) const	{ return m_type; }
	AtomicMemoryType	getMemoryType			(void) const	{ return m_atomicMemoryType; }

private:
	glu::ShaderType		m_type;
	AtomicMemoryType	m_atomicMemoryType;
};

// Buffer helper
class Buffer
{
public:
						Buffer				(Context& context, VkBufferUsageFlags usage, size_t size, bool useRef);

	VkBuffer			getBuffer			(void) const { return *m_buffer;					}
	void*				getHostPtr			(void) const { return m_allocation->getHostPtr();	}
	void				flush				(void);
	void				invalidate			(void);

private:
	const DeviceInterface&		m_vkd;
	const VkDevice				m_device;
	const VkQueue				m_queue;
	const deUint32				m_queueIndex;
	const Unique<VkBuffer>		m_buffer;
	const UniquePtr<Allocation>	m_allocation;
};

typedef de::SharedPtr<Buffer> BufferSp;

Move<VkBuffer> createBuffer (const DeviceInterface& vkd, VkDevice device, VkDeviceSize size, VkBufferUsageFlags usageFlags)
{
	const VkBufferCreateInfo createInfo	=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		DE_NULL,
		(VkBufferCreateFlags)0,
		size,
		usageFlags,
		VK_SHARING_MODE_EXCLUSIVE,
		0u,
		DE_NULL
	};
	return createBuffer(vkd, device, &createInfo);
}

MovePtr<Allocation> allocateAndBindMemory (const DeviceInterface& vkd, VkDevice device, Allocator& allocator, VkBuffer buffer, bool useRef)
{
	const MemoryRequirement allocationType = (MemoryRequirement::HostVisible | (useRef ? MemoryRequirement::DeviceAddress : MemoryRequirement::Any));
	MovePtr<Allocation>	alloc(allocator.allocate(getBufferMemoryRequirements(vkd, device, buffer), allocationType));

	VK_CHECK(vkd.bindBufferMemory(device, buffer, alloc->getMemory(), alloc->getOffset()));

	return alloc;
}

Buffer::Buffer (Context& context, VkBufferUsageFlags usage, size_t size, bool useRef)
	: m_vkd			(context.getDeviceInterface())
	, m_device		(context.getDevice())
	, m_queue		(context.getUniversalQueue())
	, m_queueIndex	(context.getUniversalQueueFamilyIndex())
	, m_buffer		(createBuffer			(context.getDeviceInterface(),
											 context.getDevice(),
											 (VkDeviceSize)size,
											 usage))
	, m_allocation	(allocateAndBindMemory	(context.getDeviceInterface(),
											 context.getDevice(),
											 context.getDefaultAllocator(),
											 *m_buffer,
											 useRef))
{
}

void Buffer::flush (void)
{
	flushMappedMemoryRange(m_vkd, m_device, m_allocation->getMemory(), m_allocation->getOffset(), VK_WHOLE_SIZE);
}

void Buffer::invalidate (void)
{
	const auto	cmdPool			= vk::makeCommandPool(m_vkd, m_device, m_queueIndex);
	const auto	cmdBufferPtr	= vk::allocateCommandBuffer(m_vkd, m_device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto	cmdBuffer		= cmdBufferPtr.get();
	const auto	bufferBarrier	= vk::makeBufferMemoryBarrier(VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, m_buffer.get(), 0ull, VK_WHOLE_SIZE);

	beginCommandBuffer(m_vkd, cmdBuffer);
	m_vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u, &bufferBarrier, 0u, nullptr);
	endCommandBuffer(m_vkd, cmdBuffer);
	submitCommandsAndWait(m_vkd, m_device, m_queue, cmdBuffer);

	invalidateMappedMemoryRange(m_vkd, m_device, m_allocation->getMemory(), m_allocation->getOffset(), VK_WHOLE_SIZE);
}

// Tests

enum AtomicOperation
{
	ATOMIC_OP_EXCHANGE = 0,
	ATOMIC_OP_COMP_SWAP,
	ATOMIC_OP_ADD,
	ATOMIC_OP_MIN,
	ATOMIC_OP_MAX,
	ATOMIC_OP_AND,
	ATOMIC_OP_OR,
	ATOMIC_OP_XOR,

	ATOMIC_OP_LAST
};

std::string atomicOp2Str (AtomicOperation op)
{
	static const char* const s_names[] =
	{
		"atomicExchange",
		"atomicCompSwap",
		"atomicAdd",
		"atomicMin",
		"atomicMax",
		"atomicAnd",
		"atomicOr",
		"atomicXor"
	};
	return de::getSizedArrayElement<ATOMIC_OP_LAST>(s_names, op);
}

enum
{
	NUM_ELEMENTS = 32
};

enum DataType
{
	DATA_TYPE_FLOAT16 = 0,
	DATA_TYPE_INT32,
	DATA_TYPE_UINT32,
	DATA_TYPE_FLOAT32,
	DATA_TYPE_INT64,
	DATA_TYPE_UINT64,
	DATA_TYPE_FLOAT64,

	DATA_TYPE_LAST
};

std::string dataType2Str(DataType type)
{
	static const char* const s_names[] =
	{
		"float16_t",
		"int",
		"uint",
		"float",
		"int64_t",
		"uint64_t",
		"double",
	};
	return de::getSizedArrayElement<DATA_TYPE_LAST>(s_names, type);
}

class BufferInterface
{
public:
	virtual void setBuffer(void* ptr) = 0;

	virtual size_t bufferSize() = 0;

	virtual void fillWithTestData(de::Random &rnd) = 0;

	virtual void checkResults(tcu::ResultCollector& resultCollector) = 0;

	virtual ~BufferInterface() {}
};

template<typename dataTypeT>
class TestBuffer : public BufferInterface
{
public:

	TestBuffer(AtomicOperation	atomicOp)
		: m_atomicOp(atomicOp)
	{}

	template<typename T>
	struct BufferData
	{
		// Use half the number of elements for inout to cause overlap between atomic operations.
		// Each inout element at index i will have two atomic operations using input from
		// indices i and i + NUM_ELEMENTS / 2.
		T			inout[NUM_ELEMENTS / 2];
		T			input[NUM_ELEMENTS];
		T			compare[NUM_ELEMENTS];
		T			output[NUM_ELEMENTS];
		T			invocationHitCount[NUM_ELEMENTS];
		deInt32		index;
	};

	virtual void setBuffer(void* ptr)
	{
		m_ptr = static_cast<BufferData<dataTypeT>*>(ptr);
	}

	virtual size_t bufferSize()
	{
		return sizeof(BufferData<dataTypeT>);
	}

	virtual void fillWithTestData(de::Random &rnd)
	{
		dataTypeT pattern;
		deMemset(&pattern, 0xcd, sizeof(dataTypeT));

		for (int i = 0; i < NUM_ELEMENTS / 2; i++)
		{
			m_ptr->inout[i] = static_cast<dataTypeT>(rnd.getUint64());
			// The first half of compare elements match with every even index.
			// The second half matches with odd indices. This causes the
			// overlapping operations to only select one.
			m_ptr->compare[i] = m_ptr->inout[i] + (i % 2);
			m_ptr->compare[i + NUM_ELEMENTS / 2] = m_ptr->inout[i] + 1 - (i % 2);
		}
		for (int i = 0; i < NUM_ELEMENTS; i++)
		{
			m_ptr->input[i] = static_cast<dataTypeT>(rnd.getUint64());
			m_ptr->output[i] = pattern;
			m_ptr->invocationHitCount[i] = 0;
		}
		m_ptr->index = 0;

		// Take a copy to be used when calculating expected values.
		m_original = *m_ptr;
	}

	virtual void checkResults(tcu::ResultCollector&	resultCollector)
	{
		checkOperation(m_original, *m_ptr, resultCollector);
	}

	template<typename T>
	struct Expected
	{
		T m_inout;
		T m_output[2];

		Expected (T inout, T output0, T output1)
		: m_inout(inout)
		{
			m_output[0] = output0;
			m_output[1] = output1;
		}

		bool compare (T inout, T output0, T output1)
		{
			return (deMemCmp((const void*)&m_inout, (const void*)&inout, sizeof(inout)) == 0
					&& deMemCmp((const void*)&m_output[0], (const void*)&output0, sizeof(output0)) == 0
					&& deMemCmp((const void*)&m_output[1], (const void*)&output1, sizeof(output1)) == 0);
		}
	};

	void checkOperation	(const BufferData<dataTypeT>&	original,
						 const BufferData<dataTypeT>&	result,
						 tcu::ResultCollector&			resultCollector);

	const AtomicOperation	m_atomicOp;

	BufferData<dataTypeT>* m_ptr;
	BufferData<dataTypeT>  m_original;

};

template<typename T>
bool nanSafeSloppyEquals(T x, T y)
{
	if (deIsIEEENaN(x) && deIsIEEENaN(y))
		return true;

	if (deIsIEEENaN(x) || deIsIEEENaN(y))
		return false;

	return fabs(deToDouble(x) - deToDouble(y)) < 0.00001;
}

template<typename dataTypeT>
class TestBufferFloatingPoint : public BufferInterface
{
public:

	TestBufferFloatingPoint(AtomicOperation	atomicOp)
		: m_atomicOp(atomicOp)
	{}

	template<typename T>
	struct BufferDataFloatingPoint
	{
		// Use half the number of elements for inout to cause overlap between atomic operations.
		// Each inout element at index i will have two atomic operations using input from
		// indices i and i + NUM_ELEMENTS / 2.
		T			inout[NUM_ELEMENTS / 2];
		T			input[NUM_ELEMENTS];
		T			compare[NUM_ELEMENTS];
		T			output[NUM_ELEMENTS];
		deInt32		invocationHitCount[NUM_ELEMENTS];
		deInt32		index;
	};

	virtual void setBuffer(void* ptr)
	{
		m_ptr = static_cast<BufferDataFloatingPoint<dataTypeT>*>(ptr);
	}

	virtual size_t bufferSize()
	{
		return sizeof(BufferDataFloatingPoint<dataTypeT>);
	}

	virtual void fillWithTestData(de::Random& rnd)
	{
		dataTypeT pattern;
		deMemset(&pattern, 0xcd, sizeof(dataTypeT));

		for (int i = 0; i < NUM_ELEMENTS / 2; i++)
		{
			m_ptr->inout[i] = deToFloatType<dataTypeT>(rnd.getFloat());
			// These aren't used by any of the float tests
			m_ptr->compare[i] = deToFloatType<dataTypeT>(0.0);
		}
		// Add special cases for NaN and +/-0
		// 0: min(sNaN, x)
		m_ptr->inout[0] = deSignalingNaN<dataTypeT>();
		// 1: min(x, sNaN)
		m_ptr->input[1 * 2 + 0] = deSignalingNaN<dataTypeT>();
		// 2: min(qNaN, x)
		m_ptr->inout[2] = deQuietNaN<dataTypeT>();
		// 3: min(x, qNaN)
		m_ptr->input[3 * 2 + 0] = deQuietNaN<dataTypeT>();
		// 4: min(NaN, NaN)
		m_ptr->inout[4] = deSignalingNaN<dataTypeT>();
		m_ptr->input[4 * 2 + 0] = deQuietNaN<dataTypeT>();
		m_ptr->input[4 * 2 + 1] = deQuietNaN<dataTypeT>();
		// 5: min(+0, -0)
		m_ptr->inout[5] = deToFloatType<dataTypeT>(-0.0);
		m_ptr->input[5 * 2 + 0] = deToFloatType<dataTypeT>(0.0);
		m_ptr->input[5 * 2 + 1] = deToFloatType<dataTypeT>(0.0);

		for (int i = 0; i < NUM_ELEMENTS; i++)
		{
			m_ptr->input[i] = deToFloatType<dataTypeT>(rnd.getFloat());
			m_ptr->output[i] = pattern;
			m_ptr->invocationHitCount[i] = 0;
		}

		m_ptr->index = 0;

		// Take a copy to be used when calculating expected values.
		m_original = *m_ptr;
	}

	virtual void checkResults(tcu::ResultCollector& resultCollector)
	{
		checkOperationFloatingPoint(m_original, *m_ptr, resultCollector);
	}

	template<typename T>
	struct Expected
	{
		T m_inout;
		T m_output[2];

		Expected(T inout, T output0, T output1)
			: m_inout(inout)
		{
			m_output[0] = output0;
			m_output[1] = output1;
		}

		bool compare(T inout, T output0, T output1)
		{
			return nanSafeSloppyEquals(m_inout, inout) &&
			       nanSafeSloppyEquals(m_output[0], output0) &&
			       nanSafeSloppyEquals(m_output[1], output1);
		}
	};

	void checkOperationFloatingPoint(const BufferDataFloatingPoint<dataTypeT>& original,
		const BufferDataFloatingPoint<dataTypeT>& result,
		tcu::ResultCollector& resultCollector);

	const AtomicOperation	m_atomicOp;

	BufferDataFloatingPoint<dataTypeT>* m_ptr;
	BufferDataFloatingPoint<dataTypeT>  m_original;

};

static BufferInterface* createTestBuffer(DataType type, AtomicOperation atomicOp)
{
	switch (type)
	{
	case DATA_TYPE_FLOAT16:
		return new TestBufferFloatingPoint<deFloat16>(atomicOp);
	case DATA_TYPE_INT32:
		return new TestBuffer<deInt32>(atomicOp);
	case DATA_TYPE_UINT32:
		return new TestBuffer<deUint32>(atomicOp);
	case DATA_TYPE_FLOAT32:
		return new TestBufferFloatingPoint<float>(atomicOp);
	case DATA_TYPE_INT64:
		return new TestBuffer<deInt64>(atomicOp);
	case DATA_TYPE_UINT64:
		return new TestBuffer<deUint64>(atomicOp);
	case DATA_TYPE_FLOAT64:
		return new TestBufferFloatingPoint<double>(atomicOp);
	default:
		DE_ASSERT(false);
		return DE_NULL;
	}
}

// Use template to handle both signed and unsigned cases. SPIR-V should
// have separate operations for both.
template<typename T>
void TestBuffer<T>::checkOperation (const BufferData<T>&	original,
									const BufferData<T>&	result,
									tcu::ResultCollector&	resultCollector)
{
	// originalInout = original inout
	// input0 = input at index i
	// iinput1 = input at index i + NUM_ELEMENTS / 2
	//
	// atomic operation will return the memory contents before
	// the operation and this is stored as output. Two operations
	// are executed for each InOut value (using input0 and input1).
	//
	// Since there is an overlap of two operations per each
	// InOut element, the outcome of the resulting InOut and
	// the outputs of the operations have two result candidates
	// depending on the execution order. Verification passes
	// if the results match one of these options.

	for (int elementNdx = 0; elementNdx < NUM_ELEMENTS / 2; elementNdx++)
	{
		// Needed when reinterpeting the data as signed values.
		const T originalInout	= *reinterpret_cast<const T*>(&original.inout[elementNdx]);
		const T input0			= *reinterpret_cast<const T*>(&original.input[elementNdx]);
		const T input1			= *reinterpret_cast<const T*>(&original.input[elementNdx + NUM_ELEMENTS / 2]);

		// Expected results are collected to this vector.
		vector<Expected<T> > exp;

		switch (m_atomicOp)
		{
			case ATOMIC_OP_ADD:
			{
				exp.push_back(Expected<T>(originalInout + input0 + input1, originalInout, originalInout + input0));
				exp.push_back(Expected<T>(originalInout + input0 + input1, originalInout + input1, originalInout));
			}
			break;

			case ATOMIC_OP_AND:
			{
				exp.push_back(Expected<T>(originalInout & input0 & input1, originalInout, originalInout & input0));
				exp.push_back(Expected<T>(originalInout & input0 & input1, originalInout & input1, originalInout));
			}
			break;

			case ATOMIC_OP_OR:
			{
				exp.push_back(Expected<T>(originalInout | input0 | input1, originalInout, originalInout | input0));
				exp.push_back(Expected<T>(originalInout | input0 | input1, originalInout | input1, originalInout));
			}
			break;

			case ATOMIC_OP_XOR:
			{
				exp.push_back(Expected<T>(originalInout ^ input0 ^ input1, originalInout, originalInout ^ input0));
				exp.push_back(Expected<T>(originalInout ^ input0 ^ input1, originalInout ^ input1, originalInout));
			}
			break;

			case ATOMIC_OP_MIN:
			{
				exp.push_back(Expected<T>(de::min(de::min(originalInout, input0), input1), originalInout, de::min(originalInout, input0)));
				exp.push_back(Expected<T>(de::min(de::min(originalInout, input0), input1), de::min(originalInout, input1), originalInout));
			}
			break;

			case ATOMIC_OP_MAX:
			{
				exp.push_back(Expected<T>(de::max(de::max(originalInout, input0), input1), originalInout, de::max(originalInout, input0)));
				exp.push_back(Expected<T>(de::max(de::max(originalInout, input0), input1), de::max(originalInout, input1), originalInout));
			}
			break;

			case ATOMIC_OP_EXCHANGE:
			{
				exp.push_back(Expected<T>(input1, originalInout, input0));
				exp.push_back(Expected<T>(input0, input1, originalInout));
			}
			break;

			case ATOMIC_OP_COMP_SWAP:
			{
				if (elementNdx % 2 == 0)
				{
					exp.push_back(Expected<T>(input0, originalInout, input0));
					exp.push_back(Expected<T>(input0, originalInout, originalInout));
				}
				else
				{
					exp.push_back(Expected<T>(input1, input1, originalInout));
					exp.push_back(Expected<T>(input1, originalInout, originalInout));
				}
			}
			break;


			default:
				DE_FATAL("Unexpected atomic operation.");
				break;
		}

		const T resIo		= result.inout[elementNdx];
		const T resOutput0	= result.output[elementNdx];
		const T resOutput1	= result.output[elementNdx + NUM_ELEMENTS / 2];


		if (!exp[0].compare(resIo, resOutput0, resOutput1) && !exp[1].compare(resIo, resOutput0, resOutput1))
		{
			std::ostringstream errorMessage;
			errorMessage	<< "ERROR: Result value check failed at index " << elementNdx
							<< ". Expected one of the two outcomes: InOut = " << tcu::toHex(exp[0].m_inout)
							<< ", Output0 = " << tcu::toHex(exp[0].m_output[0]) << ", Output1 = "
							<< tcu::toHex(exp[0].m_output[1]) << ", or InOut = " << tcu::toHex(exp[1].m_inout)
							<< ", Output0 = " << tcu::toHex(exp[1].m_output[0]) << ", Output1 = "
							<< tcu::toHex(exp[1].m_output[1]) << ". Got: InOut = " << tcu::toHex(resIo)
							<< ", Output0 = " << tcu::toHex(resOutput0) << ", Output1 = "
							<< tcu::toHex(resOutput1) << ". Using Input0 = " << tcu::toHex(original.input[elementNdx])
							<< " and Input1 = " << tcu::toHex(original.input[elementNdx + NUM_ELEMENTS / 2]) << ".";

			resultCollector.fail(errorMessage.str());
		}
	}
}

template<typename T>
void handleExceptionalFloatMinMaxValues(vector<T> &values, T x, T y)
{

	if (deIsSignalingNaN(x) && deIsSignalingNaN(y))
	{
		values.push_back(deQuietNaN<T>());
		values.push_back(deSignalingNaN<T>());
	}
	else if (deIsSignalingNaN(x))
	{
		values.push_back(deQuietNaN<T>());
		values.push_back(deSignalingNaN<T>());
		if (!deIsIEEENaN(y))
			values.push_back(y);
	}
	else if (deIsSignalingNaN(y))
	{
		values.push_back(deQuietNaN<T>());
		values.push_back(deSignalingNaN<T>());
		if (!deIsIEEENaN(x))
			values.push_back(x);
	}
	else if (deIsIEEENaN(x) && deIsIEEENaN(y))
	{
		// Both quiet NaNs
		values.push_back(deQuietNaN<T>());
	}
	else if (deIsIEEENaN(x))
	{
		// One quiet NaN and one non-NaN.
		values.push_back(y);
	}
	else if (deIsIEEENaN(y))
	{
		// One quiet NaN and one non-NaN.
		values.push_back(x);
	}
	else if ((deIsPositiveZero(x) && deIsNegativeZero(y)) || (deIsNegativeZero(x) && deIsPositiveZero(y)))
	{
		values.push_back(deToFloatType<T>(0.0));
		values.push_back(deToFloatType<T>(-0.0));
	}
}

template<typename T>
T floatAdd(T x, T y)
{
	if (deIsIEEENaN(x) || deIsIEEENaN(y))
		return deQuietNaN<T>();
	return deToFloatType<T>(deToDouble(x) + deToDouble(y));
}

template<typename T>
vector<T> floatMinValues(T x, T y)
{
	vector<T> values;
	handleExceptionalFloatMinMaxValues(values, x, y);
	if (values.empty())
	{
		values.push_back(deToDouble(x) < deToDouble(y) ? x : y);
	}
	return values;
}

template<typename T>
vector<T> floatMaxValues(T x, T y)
{
	vector<T> values;
	handleExceptionalFloatMinMaxValues(values, x, y);
	if (values.empty())
	{
		values.push_back(deToDouble(x) > deToDouble(y) ? x : y);
	}
	return values;
}

// Use template to handle both float and double cases. SPIR-V should
// have separate operations for both.
template<typename T>
void TestBufferFloatingPoint<T>::checkOperationFloatingPoint(const BufferDataFloatingPoint<T>& original,
	const BufferDataFloatingPoint<T>& result,
	tcu::ResultCollector& resultCollector)
{
	// originalInout = original inout
	// input0 = input at index i
	// iinput1 = input at index i + NUM_ELEMENTS / 2
	//
	// atomic operation will return the memory contents before
	// the operation and this is stored as output. Two operations
	// are executed for each InOut value (using input0 and input1).
	//
	// Since there is an overlap of two operations per each
	// InOut element, the outcome of the resulting InOut and
	// the outputs of the operations have two result candidates
	// depending on the execution order. Verification passes
	// if the results match one of these options.

	for (int elementNdx = 0; elementNdx < NUM_ELEMENTS / 2; elementNdx++)
	{
		// Needed when reinterpeting the data as signed values.
		const T originalInout = *reinterpret_cast<const T*>(&original.inout[elementNdx]);
		const T input0 = *reinterpret_cast<const T*>(&original.input[elementNdx]);
		const T input1 = *reinterpret_cast<const T*>(&original.input[elementNdx + NUM_ELEMENTS / 2]);

		// Expected results are collected to this vector.
		vector<Expected<T> > exp;

		switch (m_atomicOp)
		{
		case ATOMIC_OP_ADD:
		{
			exp.push_back(Expected<T>(floatAdd(floatAdd(originalInout, input0), input1), originalInout, floatAdd(originalInout, input0)));
			exp.push_back(Expected<T>(floatAdd(floatAdd(originalInout, input0), input1), floatAdd(originalInout, input1), originalInout));
		}
		break;

		case ATOMIC_OP_MIN:
		{
			// The case where input0 is combined first
			vector<T> minOriginalAndInput0 = floatMinValues(originalInout, input0);
			for (T x : minOriginalAndInput0)
			{
				vector<T> minAll = floatMinValues(x, input1);
				for (T y : minAll)
				{
					exp.push_back(Expected<T>(y, originalInout, x));
				}
			}

			// The case where input1 is combined first
			vector<T> minOriginalAndInput1 = floatMinValues(originalInout, input1);
			for (T x : minOriginalAndInput1)
			{
				vector<T> minAll = floatMinValues(x, input0);
				for (T y : minAll)
				{
					exp.push_back(Expected<T>(y, x, originalInout));
				}
			}
		}
		break;

		case ATOMIC_OP_MAX:
		{
			// The case where input0 is combined first
			vector<T> minOriginalAndInput0 = floatMaxValues(originalInout, input0);
			for (T x : minOriginalAndInput0)
			{
				vector<T> minAll = floatMaxValues(x, input1);
				for (T y : minAll)
				{
					exp.push_back(Expected<T>(y, originalInout, x));
				}
			}

			// The case where input1 is combined first
			vector<T> minOriginalAndInput1 = floatMaxValues(originalInout, input1);
			for (T x : minOriginalAndInput1)
			{
				vector<T> minAll = floatMaxValues(x, input0);
				for (T y : minAll)
				{
					exp.push_back(Expected<T>(y, x, originalInout));
				}
			}
		}
		break;

		case ATOMIC_OP_EXCHANGE:
		{
			exp.push_back(Expected<T>(input1, originalInout, input0));
			exp.push_back(Expected<T>(input0, input1, originalInout));
		}
		break;

		default:
			DE_FATAL("Unexpected atomic operation.");
			break;
		}

		const T resIo = result.inout[elementNdx];
		const T resOutput0 = result.output[elementNdx];
		const T resOutput1 = result.output[elementNdx + NUM_ELEMENTS / 2];


		bool hasMatch = false;
		for (Expected<T> e : exp)
		{
			if (e.compare(resIo, resOutput0, resOutput1))
			{
				hasMatch = true;
				break;
			}
		}
		if (!hasMatch)
		{
			std::ostringstream errorMessage;
			errorMessage << "ERROR: Result value check failed at index " << elementNdx
				<< ". Expected one of the outcomes:";

			bool first = true;
			for (Expected<T> e : exp)
			{
				if (!first)
					errorMessage << ", or";
				first = false;

				errorMessage << " InOut = " << e.m_inout
					<< ", Output0 = " << e.m_output[0]
					<< ", Output1 = " << e.m_output[1];
			}

			errorMessage << ". Got: InOut = " << resIo
				<< ", Output0 = " << resOutput0
				<< ", Output1 = " << resOutput1
				<< ". Using Input0 = " << original.input[elementNdx]
				<< " and Input1 = " << original.input[elementNdx + NUM_ELEMENTS / 2] << ".";

			resultCollector.fail(errorMessage.str());
		}
	}
}

class AtomicOperationCaseInstance : public TestInstance
{
public:
									AtomicOperationCaseInstance		(Context&			context,
																	 const ShaderSpec&	shaderSpec,
																	 AtomicShaderType	shaderType,
																	 DataType			dataType,
																	 AtomicOperation	atomicOp);

	virtual tcu::TestStatus			iterate							(void);

private:
	const ShaderSpec&				m_shaderSpec;
	AtomicShaderType				m_shaderType;
	const DataType					m_dataType;
	AtomicOperation					m_atomicOp;

};

AtomicOperationCaseInstance::AtomicOperationCaseInstance (Context&				context,
														  const ShaderSpec&		shaderSpec,
														  AtomicShaderType		shaderType,
														  DataType				dataType,
														  AtomicOperation		atomicOp)
	: TestInstance	(context)
	, m_shaderSpec	(shaderSpec)
	, m_shaderType	(shaderType)
	, m_dataType	(dataType)
	, m_atomicOp	(atomicOp)
{
}

tcu::TestStatus AtomicOperationCaseInstance::iterate(void)
{
	de::UniquePtr<BufferInterface>	testBuffer	(createTestBuffer(m_dataType, m_atomicOp));
	tcu::TestLog&					log			= m_context.getTestContext().getLog();
	const DeviceInterface&			vkd			= m_context.getDeviceInterface();
	const VkDevice					device		= m_context.getDevice();
	de::Random						rnd			(0x62a15e34);
	const bool						useRef		= (m_shaderType.getMemoryType() == AtomicMemoryType::REFERENCE);
	const VkDescriptorType			descType	= (useRef ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	const VkBufferUsageFlags		usageFlags	= (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | (useRef ? static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) : 0u));

	// The main buffer will hold test data. When using buffer references, the buffer's address will be indirectly passed as part of
	// a uniform buffer. If not, it will be passed directly as a descriptor.
	Buffer							buffer		(m_context, usageFlags, testBuffer->bufferSize(), useRef);
	std::unique_ptr<Buffer>			auxBuffer;

	if (useRef)
	{
		// Pass the main buffer address inside a uniform buffer.
		const VkBufferDeviceAddressInfo addressInfo =
		{
			VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,	//	VkStructureType	sType;
			nullptr,										//	const void*		pNext;
			buffer.getBuffer(),								//	VkBuffer		buffer;
		};
		const auto address = vkd.getBufferDeviceAddress(device, &addressInfo);

		auxBuffer.reset(new Buffer(m_context, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(address), false));
		deMemcpy(auxBuffer->getHostPtr(), &address, sizeof(address));
		auxBuffer->flush();
	}

	testBuffer->setBuffer(buffer.getHostPtr());
	testBuffer->fillWithTestData(rnd);

	buffer.flush();

	Move<VkDescriptorSetLayout>	extraResourcesLayout;
	Move<VkDescriptorPool>		extraResourcesSetPool;
	Move<VkDescriptorSet>		extraResourcesSet;

	const VkDescriptorSetLayoutBinding bindings[] =
	{
		{ 0u, descType, 1, VK_SHADER_STAGE_ALL, DE_NULL }
	};

	const VkDescriptorSetLayoutCreateInfo	layoutInfo	=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		DE_NULL,
		(VkDescriptorSetLayoutCreateFlags)0u,
		DE_LENGTH_OF_ARRAY(bindings),
		bindings
	};

	extraResourcesLayout = createDescriptorSetLayout(vkd, device, &layoutInfo);

	const VkDescriptorPoolSize poolSizes[] =
	{
		{ descType, 1u }
	};

	const VkDescriptorPoolCreateInfo poolInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		DE_NULL,
		(VkDescriptorPoolCreateFlags)VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		1u,		// maxSets
		DE_LENGTH_OF_ARRAY(poolSizes),
		poolSizes
	};

	extraResourcesSetPool = createDescriptorPool(vkd, device, &poolInfo);

	const VkDescriptorSetAllocateInfo allocInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		DE_NULL,
		*extraResourcesSetPool,
		1u,
		&extraResourcesLayout.get()
	};

	extraResourcesSet = allocateDescriptorSet(vkd, device, &allocInfo);

	VkDescriptorBufferInfo bufferInfo;
	bufferInfo.buffer	= (useRef ? auxBuffer->getBuffer() : buffer.getBuffer());
	bufferInfo.offset	= 0u;
	bufferInfo.range	= VK_WHOLE_SIZE;

	const VkWriteDescriptorSet descriptorWrite =
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		DE_NULL,
		*extraResourcesSet,
		0u,		// dstBinding
		0u,		// dstArrayElement
		1u,
		descType,
		(const VkDescriptorImageInfo*)DE_NULL,
		&bufferInfo,
		(const VkBufferView*)DE_NULL
	};

	vkd.updateDescriptorSets(device, 1u, &descriptorWrite, 0u, DE_NULL);

	// Storage for output varying data.
	std::vector<deUint32>	outputs		(NUM_ELEMENTS);
	std::vector<void*>		outputPtr	(NUM_ELEMENTS);

	for (size_t i = 0; i < NUM_ELEMENTS; i++)
	{
		outputs[i] = 0xcdcdcdcd;
		outputPtr[i] = &outputs[i];
	}

	const int					numWorkGroups	= ((m_shaderType.getMemoryType() == AtomicMemoryType::SHARED) ? 1 : static_cast<int>(NUM_ELEMENTS));
	UniquePtr<ShaderExecutor>	executor		(createExecutor(m_context, m_shaderType.getType(), m_shaderSpec, *extraResourcesLayout));

	executor->execute(numWorkGroups, DE_NULL, &outputPtr[0], *extraResourcesSet);
	buffer.invalidate();

	tcu::ResultCollector resultCollector(log);

	// Check the results of the atomic operation
	testBuffer->checkResults(resultCollector);

	return tcu::TestStatus(resultCollector.getResult(), resultCollector.getMessage());
}

class AtomicOperationCase : public TestCase
{
public:
							AtomicOperationCase		(tcu::TestContext&		testCtx,
													 const char*			name,
													 const char*			description,
													 AtomicShaderType		type,
													 DataType				dataType,
													 AtomicOperation		atomicOp);
	virtual					~AtomicOperationCase	(void);

	virtual TestInstance*	createInstance			(Context& ctx) const;
	virtual void			checkSupport			(Context& ctx) const;
	virtual void			initPrograms			(vk::SourceCollections& programCollection) const
	{
		generateSources(m_shaderType.getType(), m_shaderSpec, programCollection);
	}

private:

	void					createShaderSpec();
	ShaderSpec				m_shaderSpec;
	const AtomicShaderType	m_shaderType;
	const DataType			m_dataType;
	const AtomicOperation	m_atomicOp;
};

AtomicOperationCase::AtomicOperationCase (tcu::TestContext&	testCtx,
										  const char*		name,
										  const char*		description,
										  AtomicShaderType	shaderType,
										  DataType			dataType,
										  AtomicOperation	atomicOp)
	: TestCase			(testCtx, name, description)
	, m_shaderType		(shaderType)
	, m_dataType		(dataType)
	, m_atomicOp		(atomicOp)
{
	createShaderSpec();
	init();
}

AtomicOperationCase::~AtomicOperationCase (void)
{
}

TestInstance* AtomicOperationCase::createInstance (Context& ctx) const
{
	return new AtomicOperationCaseInstance(ctx, m_shaderSpec, m_shaderType, m_dataType, m_atomicOp);
}

void AtomicOperationCase::checkSupport (Context& ctx) const
{
	if ((m_dataType == DATA_TYPE_INT64) || (m_dataType == DATA_TYPE_UINT64))
	{
		ctx.requireDeviceFunctionality("VK_KHR_shader_atomic_int64");

		const auto atomicInt64Features	= ctx.getShaderAtomicInt64Features();
		const bool isSharedMemory		= (m_shaderType.getMemoryType() == AtomicMemoryType::SHARED);

		if (!isSharedMemory && atomicInt64Features.shaderBufferInt64Atomics == VK_FALSE)
		{
			TCU_THROW(NotSupportedError, "VkShaderAtomicInt64: 64-bit integer atomic operations not supported for buffers");
		}
		if (isSharedMemory && atomicInt64Features.shaderSharedInt64Atomics == VK_FALSE)
		{
			TCU_THROW(NotSupportedError, "VkShaderAtomicInt64: 64-bit integer atomic operations not supported for shared memory");
		}
	}

	if (m_dataType == DATA_TYPE_FLOAT16)
	{
		ctx.requireDeviceFunctionality("VK_EXT_shader_atomic_float2");
		if (m_atomicOp == ATOMIC_OP_ADD)
		{
			if (m_shaderType.getMemoryType() == AtomicMemoryType::SHARED)
			{
				if (!ctx.getShaderAtomicFloat2FeaturesEXT().shaderSharedFloat16AtomicAdd)
				{
					TCU_THROW(NotSupportedError, "VkShaderAtomicFloat16: 16-bit floating point shared add atomic operation not supported");
				}
			}
			else
			{
				if (!ctx.getShaderAtomicFloat2FeaturesEXT().shaderBufferFloat16AtomicAdd)
				{
					TCU_THROW(NotSupportedError, "VkShaderAtomicFloat16: 16-bit floating point buffer add atomic operation not supported");
				}
			}
		}
		if (m_atomicOp == ATOMIC_OP_MIN || m_atomicOp == ATOMIC_OP_MAX)
		{
			if (m_shaderType.getMemoryType() == AtomicMemoryType::SHARED)
			{
				if (!ctx.getShaderAtomicFloat2FeaturesEXT().shaderSharedFloat16AtomicMinMax)
				{
					TCU_THROW(NotSupportedError, "VkShaderAtomicFloat16: 16-bit floating point shared min/max atomic operation not supported");
				}
			}
			else
			{
				if (!ctx.getShaderAtomicFloat2FeaturesEXT().shaderBufferFloat16AtomicMinMax)
				{
					TCU_THROW(NotSupportedError, "VkShaderAtomicFloat16: 16-bit floating point buffer min/max atomic operation not supported");
				}
			}
		}
		if (m_atomicOp == ATOMIC_OP_EXCHANGE)
		{
			if (m_shaderType.getMemoryType() == AtomicMemoryType::SHARED)
			{
				if (!ctx.getShaderAtomicFloat2FeaturesEXT().shaderSharedFloat16Atomics)
				{
					TCU_THROW(NotSupportedError, "VkShaderAtomicFloat16: 16-bit floating point shared atomic operations not supported");
				}
			}
			else
			{
				if (!ctx.getShaderAtomicFloat2FeaturesEXT().shaderBufferFloat16Atomics)
				{
					TCU_THROW(NotSupportedError, "VkShaderAtomicFloat16: 16-bit floating point buffer atomic operations not supported");
				}
			}
		}
	}

	if (m_dataType == DATA_TYPE_FLOAT32)
	{
		ctx.requireDeviceFunctionality("VK_EXT_shader_atomic_float");
		if (m_atomicOp == ATOMIC_OP_ADD)
		{
			if (m_shaderType.getMemoryType() == AtomicMemoryType::SHARED)
			{
				if (!ctx.getShaderAtomicFloatFeaturesEXT().shaderSharedFloat32AtomicAdd)
				{
					TCU_THROW(NotSupportedError, "VkShaderAtomicFloat32: 32-bit floating point shared add atomic operation not supported");
				}
			}
			else
			{
				if (!ctx.getShaderAtomicFloatFeaturesEXT().shaderBufferFloat32AtomicAdd)
				{
					TCU_THROW(NotSupportedError, "VkShaderAtomicFloat32: 32-bit floating point buffer add atomic operation not supported");
				}
			}
		}
		if (m_atomicOp == ATOMIC_OP_MIN || m_atomicOp == ATOMIC_OP_MAX)
		{
			ctx.requireDeviceFunctionality("VK_EXT_shader_atomic_float2");
			if (m_shaderType.getMemoryType() == AtomicMemoryType::SHARED)
			{
				if (!ctx.getShaderAtomicFloat2FeaturesEXT().shaderSharedFloat32AtomicMinMax)
				{
					TCU_THROW(NotSupportedError, "VkShaderAtomicFloat32: 32-bit floating point shared min/max atomic operation not supported");
				}
			}
			else
			{
				if (!ctx.getShaderAtomicFloat2FeaturesEXT().shaderBufferFloat32AtomicMinMax)
				{
					TCU_THROW(NotSupportedError, "VkShaderAtomicFloat32: 32-bit floating point buffer min/max atomic operation not supported");
				}
			}
		}
		if (m_atomicOp == ATOMIC_OP_EXCHANGE)
		{
			if (m_shaderType.getMemoryType() == AtomicMemoryType::SHARED)
			{
				if (!ctx.getShaderAtomicFloatFeaturesEXT().shaderSharedFloat32Atomics)
				{
					TCU_THROW(NotSupportedError, "VkShaderAtomicFloat32: 32-bit floating point shared atomic operations not supported");
				}
			}
			else
			{
				if (!ctx.getShaderAtomicFloatFeaturesEXT().shaderBufferFloat32Atomics)
				{
					TCU_THROW(NotSupportedError, "VkShaderAtomicFloat32: 32-bit floating point buffer atomic operations not supported");
				}
			}
		}
	}

	if (m_dataType == DATA_TYPE_FLOAT64)
	{
		ctx.requireDeviceFunctionality("VK_EXT_shader_atomic_float");
		if (m_atomicOp == ATOMIC_OP_ADD)
		{
			if (m_shaderType.getMemoryType() == AtomicMemoryType::SHARED)
			{
				if (!ctx.getShaderAtomicFloatFeaturesEXT().shaderSharedFloat64AtomicAdd)
				{
					TCU_THROW(NotSupportedError, "VkShaderAtomicFloat64: 64-bit floating point shared add atomic operation not supported");
				}
			}
			else
			{
				if (!ctx.getShaderAtomicFloatFeaturesEXT().shaderBufferFloat64AtomicAdd)
				{
					TCU_THROW(NotSupportedError, "VkShaderAtomicFloat64: 64-bit floating point buffer add atomic operation not supported");
				}
			}
		}
		if (m_atomicOp == ATOMIC_OP_MIN || m_atomicOp == ATOMIC_OP_MAX)
		{
			ctx.requireDeviceFunctionality("VK_EXT_shader_atomic_float2");
			if (m_shaderType.getMemoryType() == AtomicMemoryType::SHARED)
			{
				if (!ctx.getShaderAtomicFloat2FeaturesEXT().shaderSharedFloat64AtomicMinMax)
				{
					TCU_THROW(NotSupportedError, "VkShaderAtomicFloat64: 64-bit floating point shared min/max atomic operation not supported");
				}
			}
			else
			{
				if (!ctx.getShaderAtomicFloat2FeaturesEXT().shaderBufferFloat64AtomicMinMax)
				{
					TCU_THROW(NotSupportedError, "VkShaderAtomicFloat64: 64-bit floating point buffer min/max atomic operation not supported");
				}
			}
		}
		if (m_atomicOp == ATOMIC_OP_EXCHANGE)
		{
			if (m_shaderType.getMemoryType() == AtomicMemoryType::SHARED)
			{
				if (!ctx.getShaderAtomicFloatFeaturesEXT().shaderSharedFloat64Atomics)
				{
					TCU_THROW(NotSupportedError, "VkShaderAtomicFloat64: 64-bit floating point shared atomic operations not supported");
				}
			}
			else
			{
				if (!ctx.getShaderAtomicFloatFeaturesEXT().shaderBufferFloat64Atomics)
				{
					TCU_THROW(NotSupportedError, "VkShaderAtomicFloat64: 64-bit floating point buffer atomic operations not supported");
				}
			}
		}
	}

	if (m_shaderType.getMemoryType() == AtomicMemoryType::REFERENCE)
	{
		ctx.requireDeviceFunctionality("VK_KHR_buffer_device_address");
	}

	// Check stores and atomic operation support.
	switch (m_shaderType.getType())
	{
	case glu::SHADERTYPE_VERTEX:
	case glu::SHADERTYPE_TESSELLATION_CONTROL:
	case glu::SHADERTYPE_TESSELLATION_EVALUATION:
	case glu::SHADERTYPE_GEOMETRY:
		if (!ctx.getDeviceFeatures().vertexPipelineStoresAndAtomics)
			TCU_THROW(NotSupportedError, "Stores and atomic operations are not supported in Vertex, Tessellation, and Geometry shader.");
		break;
	case glu::SHADERTYPE_FRAGMENT:
		if (!ctx.getDeviceFeatures().fragmentStoresAndAtomics)
			TCU_THROW(NotSupportedError, "Stores and atomic operations are not supported in fragment shader.");
		break;
	case glu::SHADERTYPE_COMPUTE:
		break;
	default:
		DE_FATAL("Unsupported shader type");
	}

	checkSupportShader(ctx, m_shaderType.getType());
}

void AtomicOperationCase::createShaderSpec (void)
{
	const AtomicMemoryType memoryType = m_shaderType.getMemoryType();

	// Global declarations.
	std::ostringstream shaderTemplateGlobalStream;

	// Structure in use for atomic operations.
	shaderTemplateGlobalStream
		<< "${EXTENSIONS}\n"
		<< "\n"
		<< "struct AtomicStruct\n"
		<< "{\n"
		<< "    ${DATATYPE} inoutValues[${N}/2];\n"
		<< "    ${DATATYPE} inputValues[${N}];\n"
		<< "    ${DATATYPE} compareValues[${N}];\n"
		<< "    ${DATATYPE} outputValues[${N}];\n"
		<< "    int invocationHitCount[${N}];\n"
		<< "    int index;\n"
		<< "};\n"
		<< "\n"
		;

	// The name dance and declarations below will make sure the structure that will be used with atomic operations can be accessed
	// as "buf.data", which is the name used in the atomic operation statements.
	//
	// * When using a buffer directly, RESULT_BUFFER_NAME will be "buf" and the inner struct will be "data".
	// * When using a workgroup-shared global variable, the "data" struct will be nested in an auxiliar "buf" struct.
	// * When using buffer references, the uniform buffer reference will be called "buf" and its contents "data".
	//
	if (memoryType != AtomicMemoryType::REFERENCE)
	{
		shaderTemplateGlobalStream
			<< "layout (set = ${SETIDX}, binding = 0) buffer AtomicBuffer {\n"
			<< "    AtomicStruct data;\n"
			<< "} ${RESULT_BUFFER_NAME};\n"
			<< "\n"
			;

		// When using global shared memory in the compute variant, invocations will use a shared global structure instead of a
		// descriptor set as the sources and results of each tested operation.
		if (memoryType == AtomicMemoryType::SHARED)
		{
			shaderTemplateGlobalStream
				<< "shared struct { AtomicStruct data; } buf;\n"
				<< "\n"
				;
		}
	}
	else
	{
		shaderTemplateGlobalStream
			<< "layout (buffer_reference) buffer AtomicBuffer {\n"
			<< "    AtomicStruct data;\n"
			<< "};\n"
			<< "\n"
			<< "layout (set = ${SETIDX}, binding = 0) uniform References {\n"
			<< "    AtomicBuffer buf;\n"
			<< "};\n"
			<< "\n"
			;
	}

	const auto					shaderTemplateGlobalString	= shaderTemplateGlobalStream.str();
	const tcu::StringTemplate	shaderTemplateGlobal		(shaderTemplateGlobalString);

	// Shader body for the non-vertex case.
	std::ostringstream nonVertexShaderTemplateStream;

	if (memoryType == AtomicMemoryType::SHARED)
	{
		// Invocation zero will initialize the shared structure from the descriptor set.
		nonVertexShaderTemplateStream
			<< "if (gl_LocalInvocationIndex == 0u)\n"
			<< "{\n"
			<< "    buf.data = ${RESULT_BUFFER_NAME}.data;\n"
			<< "}\n"
			<< "barrier();\n"
			;
	}

	if (m_shaderType.getType() == glu::SHADERTYPE_FRAGMENT)
	{
		nonVertexShaderTemplateStream
			<< "if (!gl_HelperInvocation) {\n"
			<< "    int idx = atomicAdd(buf.data.index, 1);\n"
			<< "    buf.data.outputValues[idx] = ${ATOMICOP}(buf.data.inoutValues[idx % (${N}/2)], ${COMPARE_ARG}buf.data.inputValues[idx]);\n"
			<< "}\n"
			;
	}
	else
	{
		nonVertexShaderTemplateStream
			<< "if (atomicAdd(buf.data.invocationHitCount[0], 1) < ${N})\n"
			<< "{\n"
			<< "    int idx = atomicAdd(buf.data.index, 1);\n"
			<< "    buf.data.outputValues[idx] = ${ATOMICOP}(buf.data.inoutValues[idx % (${N}/2)], ${COMPARE_ARG}buf.data.inputValues[idx]);\n"
			<< "}\n"
			;
	}

	if (memoryType == AtomicMemoryType::SHARED)
	{
		// Invocation zero will copy results back to the descriptor set.
		nonVertexShaderTemplateStream
			<< "barrier();\n"
			<< "if (gl_LocalInvocationIndex == 0u)\n"
			<< "{\n"
			<< "    ${RESULT_BUFFER_NAME}.data = buf.data;\n"
			<< "}\n"
			;
	}

	const auto					nonVertexShaderTemplateStreamStr	= nonVertexShaderTemplateStream.str();
	const tcu::StringTemplate	nonVertexShaderTemplateSrc			(nonVertexShaderTemplateStreamStr);

	// Shader body for the vertex case.
	const tcu::StringTemplate vertexShaderTemplateSrc(
		"int idx = gl_VertexIndex;\n"
		"if (atomicAdd(buf.data.invocationHitCount[idx], 1) == 0)\n"
		"{\n"
		"    buf.data.outputValues[idx] = ${ATOMICOP}(buf.data.inoutValues[idx % (${N}/2)], ${COMPARE_ARG}buf.data.inputValues[idx]);\n"
		"}\n");

	// Extensions.
	std::ostringstream extensions;

	if ((m_dataType == DATA_TYPE_INT64) || (m_dataType == DATA_TYPE_UINT64))
	{
		extensions
			<< "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable\n"
			<< "#extension GL_EXT_shader_atomic_int64 : enable\n"
			;
	}
	else if ((m_dataType == DATA_TYPE_FLOAT16) || (m_dataType == DATA_TYPE_FLOAT32) || (m_dataType == DATA_TYPE_FLOAT64))
	{
		extensions
			<< "#extension GL_EXT_shader_explicit_arithmetic_types_float16 : enable\n"
			<< "#extension GL_EXT_shader_atomic_float : enable\n"
			<< "#extension GL_EXT_shader_atomic_float2 : enable\n"
			<< "#extension GL_KHR_memory_scope_semantics : enable\n"
			;
	}

	if (memoryType == AtomicMemoryType::REFERENCE)
	{
		extensions << "#extension GL_EXT_buffer_reference : require\n";
	}

	// Specializations.
	std::map<std::string, std::string> specializations;

	specializations["EXTENSIONS"]			= extensions.str();
	specializations["DATATYPE"]				= dataType2Str(m_dataType);
	specializations["ATOMICOP"]				= atomicOp2Str(m_atomicOp);
	specializations["SETIDX"]				= de::toString((int)EXTRA_RESOURCES_DESCRIPTOR_SET_INDEX);
	specializations["N"]					= de::toString((int)NUM_ELEMENTS);
	specializations["COMPARE_ARG"]			= ((m_atomicOp == ATOMIC_OP_COMP_SWAP) ? "buf.data.compareValues[idx], " : "");
	specializations["RESULT_BUFFER_NAME"]	= ((memoryType == AtomicMemoryType::SHARED) ? "result" : "buf");

	// Shader spec.
	m_shaderSpec.outputs.push_back(Symbol("outData", glu::VarType(glu::TYPE_UINT, glu::PRECISION_HIGHP)));
	m_shaderSpec.glslVersion		= glu::GLSL_VERSION_450;
	m_shaderSpec.globalDeclarations	= shaderTemplateGlobal.specialize(specializations);
	m_shaderSpec.source				= ((m_shaderType.getType() == glu::SHADERTYPE_VERTEX)
										? vertexShaderTemplateSrc.specialize(specializations)
										: nonVertexShaderTemplateSrc.specialize(specializations));

	if (memoryType == AtomicMemoryType::SHARED)
	{
		// When using global shared memory, use a single workgroup and an appropriate number of local invocations.
		m_shaderSpec.localSizeX = static_cast<int>(NUM_ELEMENTS);
	}
}

void addAtomicOperationTests (tcu::TestCaseGroup* atomicOperationTestsGroup)
{
	tcu::TestContext& testCtx = atomicOperationTestsGroup->getTestContext();

	static const struct
	{
		glu::ShaderType		type;
		const char*			name;
	} shaderTypes[] =
	{
		{ glu::SHADERTYPE_VERTEX,							"vertex"			},
		{ glu::SHADERTYPE_FRAGMENT,							"fragment"			},
		{ glu::SHADERTYPE_GEOMETRY,							"geometry"			},
		{ glu::SHADERTYPE_TESSELLATION_CONTROL,				"tess_ctrl"			},
		{ glu::SHADERTYPE_TESSELLATION_EVALUATION,			"tess_eval"			},
		{ glu::SHADERTYPE_COMPUTE,							"compute"			},
	};

	static const struct
	{
		AtomicMemoryType	type;
		const char*			suffix;
	} kMemoryTypes[] =
	{
		{ AtomicMemoryType::BUFFER,		""				},
		{ AtomicMemoryType::SHARED,		"_shared"		},
		{ AtomicMemoryType::REFERENCE,	"_reference"	},
	};

	static const struct
	{
		DataType		dataType;
		const char*		name;
		const char*		description;
	} dataSign[] =
	{
		{ DATA_TYPE_FLOAT16,"float16",			"Tests using 16-bit float data"				},
		{ DATA_TYPE_INT32,	"signed",			"Tests using signed data (int)"				},
		{ DATA_TYPE_UINT32,	"unsigned",			"Tests using unsigned data (uint)"			},
		{ DATA_TYPE_FLOAT32,"float32",			"Tests using 32-bit float data"				},
		{ DATA_TYPE_INT64,	"signed64bit",		"Tests using 64 bit signed data (int64)"	},
		{ DATA_TYPE_UINT64,	"unsigned64bit",	"Tests using 64 bit unsigned data (uint64)"	},
		{ DATA_TYPE_FLOAT64,"float64",			"Tests using 64-bit float data)"			}
	};

	static const struct
	{
		AtomicOperation		value;
		const char*			name;
	} atomicOp[] =
	{
		{ ATOMIC_OP_EXCHANGE,	"exchange"	},
		{ ATOMIC_OP_COMP_SWAP,	"comp_swap"	},
		{ ATOMIC_OP_ADD,		"add"		},
		{ ATOMIC_OP_MIN,		"min"		},
		{ ATOMIC_OP_MAX,		"max"		},
		{ ATOMIC_OP_AND,		"and"		},
		{ ATOMIC_OP_OR,			"or"		},
		{ ATOMIC_OP_XOR,		"xor"		}
	};

	for (int opNdx = 0; opNdx < DE_LENGTH_OF_ARRAY(atomicOp); opNdx++)
	{
		for (int signNdx = 0; signNdx < DE_LENGTH_OF_ARRAY(dataSign); signNdx++)
		{
			for (int shaderTypeNdx = 0; shaderTypeNdx < DE_LENGTH_OF_ARRAY(shaderTypes); shaderTypeNdx++)
			{
				// Only ADD and EXCHANGE are supported on floating-point
				if (dataSign[signNdx].dataType == DATA_TYPE_FLOAT16 || dataSign[signNdx].dataType == DATA_TYPE_FLOAT32 || dataSign[signNdx].dataType == DATA_TYPE_FLOAT64)
				{
					if (atomicOp[opNdx].value != ATOMIC_OP_ADD &&
					    atomicOp[opNdx].value != ATOMIC_OP_MIN &&
					    atomicOp[opNdx].value != ATOMIC_OP_MAX &&
					    atomicOp[opNdx].value != ATOMIC_OP_EXCHANGE)
					{
						continue;
					}
				}

				for (int memoryTypeNdx = 0; memoryTypeNdx < DE_LENGTH_OF_ARRAY(kMemoryTypes); ++memoryTypeNdx)
				{
					// Shared memory only available in compute shaders.
					if (kMemoryTypes[memoryTypeNdx].type == AtomicMemoryType::SHARED && shaderTypes[shaderTypeNdx].type != glu::SHADERTYPE_COMPUTE)
						continue;

					const std::string description	= std::string("Tests atomic operation ") + atomicOp2Str(atomicOp[opNdx].value) + std::string(".");
					const std::string name			= std::string(atomicOp[opNdx].name) + "_" + std::string(dataSign[signNdx].name) + "_" + std::string(shaderTypes[shaderTypeNdx].name) + kMemoryTypes[memoryTypeNdx].suffix;

					atomicOperationTestsGroup->addChild(new AtomicOperationCase(testCtx, name.c_str(), description.c_str(), AtomicShaderType(shaderTypes[shaderTypeNdx].type, kMemoryTypes[memoryTypeNdx].type), dataSign[signNdx].dataType, atomicOp[opNdx].value));
				}
			}
		}
	}
}

} // anonymous

tcu::TestCaseGroup* createAtomicOperationTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "atomic_operations", "Atomic Operation Tests", addAtomicOperationTests);
}

} // shaderexecutor
} // vkt
