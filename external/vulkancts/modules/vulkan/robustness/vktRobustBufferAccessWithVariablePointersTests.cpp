/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
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
 * \brief Robust buffer access tests for storage buffers and
 *        storage texel buffers with variable pointers.
 *
 * \note These tests are checking if accessing a memory through a variable
 *       pointer that points outside of accessible buffer memory is robust.
 *       To do this the tests are creating proper SPIRV code that creates
 *       variable pointers. Those pointers are either pointing into a
 *       memory allocated for a buffer but "not accesible" - meaning
 *       DescriptorBufferInfo has smaller size than a memory we access in
 *       shader or entirely outside of allocated memory (i.e. buffer is
 *       256 bytes big but we are trying to access under offset of 1k from
 *       buffer start). There is a set of valid behaviours defined when
 *       robust buffer access extension is enabled described in chapter 32
 *       section 1 of Vulkan spec.
 *
 *//*--------------------------------------------------------------------*/

#include "vktRobustBufferAccessWithVariablePointersTests.hpp"
#include "vktRobustnessUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "tcuTestLog.hpp"
#include "vkDefs.hpp"
#include "deRandom.hpp"

#include <limits>
#include <sstream>

namespace vkt
{
namespace robustness
{

using namespace vk;

// keep local things local
namespace
{

// Creates a custom device with robust buffer access and variable pointer features.
Move<VkDevice> createRobustBufferAccessVariablePointersDevice (Context& context)
{
	auto pointerFeatures = context.getVariablePointersFeatures();

	VkPhysicalDeviceFeatures2 features2 = initVulkanStructure();
	features2.features = context.getDeviceFeatures();
	features2.features.robustBufferAccess = VK_TRUE;
	features2.pNext = &pointerFeatures;

	return createRobustBufferAccessDevice(context, &features2);
}

// A supplementary structures that can hold information about buffer size
struct AccessRangesData
{
	VkDeviceSize	allocSize;
	VkDeviceSize	accessRange;
	VkDeviceSize	maxAccessRange;
};

// Pointer to function that can be used to fill a buffer with some data - it is passed as an parameter to buffer creation utility function
typedef void(*FillBufferProcPtr)(void*, vk::VkDeviceSize, const void* const);

// An utility function for creating a buffer
// This function not only allocates memory for the buffer but also fills buffer up with a data
void createTestBuffer (Context&									context,
					   const vk::DeviceInterface&				deviceInterface,
					   const VkDevice&							device,
					   VkDeviceSize								accessRange,
					   VkBufferUsageFlags						usage,
					   SimpleAllocator&							allocator,
					   Move<VkBuffer>&							buffer,
					   de::MovePtr<Allocation>&					bufferAlloc,
					   AccessRangesData&						data,
					   FillBufferProcPtr						fillBufferProc,
					   const void* const						blob)
{
	const VkBufferCreateInfo	bufferParams	=
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		0u,											// VkBufferCreateFlags	flags;
		accessRange,								// VkDeviceSize			size;
		usage,										// VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32				queueFamilyIndexCount;
		DE_NULL										// const deUint32*		pQueueFamilyIndices;
	};

	buffer = createBuffer(deviceInterface, device, &bufferParams);

	VkMemoryRequirements bufferMemoryReqs		= getBufferMemoryRequirements(deviceInterface, device, *buffer);
	bufferAlloc = allocator.allocate(bufferMemoryReqs, MemoryRequirement::HostVisible);

	data.allocSize = bufferMemoryReqs.size;
	data.accessRange = accessRange;
	data.maxAccessRange = deMinu64(data.allocSize, deMinu64(bufferParams.size, accessRange));

	VK_CHECK(deviceInterface.bindBufferMemory(device, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));
#ifdef CTS_USES_VULKANSC
	if(context.getTestContext().getCommandLine().isSubProcess())
		fillBufferProc(bufferAlloc->getHostPtr(), bufferMemoryReqs.size, blob);
#else
	fillBufferProc(bufferAlloc->getHostPtr(), bufferMemoryReqs.size, blob);
	DE_UNREF(context);
#endif // CTS_USES_VULKANCSC
	flushMappedMemoryRange(deviceInterface, device, bufferAlloc->getMemory(), bufferAlloc->getOffset(), VK_WHOLE_SIZE);
}

// An adapter function matching FillBufferProcPtr interface. Fills a buffer with "randomly" generated test data matching desired format.
void populateBufferWithValues (void*				buffer,
							   VkDeviceSize			size,
							   const void* const	blob)
{
	populateBufferWithTestValues(buffer, size, *static_cast<const vk::VkFormat*>(blob));
}

// An adapter function matching FillBufferProcPtr interface. Fills a buffer with 0xBABABABABABA... pattern. Used to fill up output buffers.
// Since this pattern cannot show up in generated test data it should not show up in the valid output.
void populateBufferWithFiller (void*					buffer,
							   VkDeviceSize				size,
							   const void* const		blob)
{
	DE_UNREF(blob);
	deMemset(buffer, 0xBA, static_cast<size_t>(size));
}

// An adapter function matching FillBufferProcPtr interface. Fills a buffer with a copy of memory contents pointed to by blob.
void populateBufferWithCopy (void*					buffer,
							 VkDeviceSize			size,
							 const void* const		blob)
{
	deMemcpy(buffer, blob, static_cast<size_t>(size));
}

// A composite types used in test
// Those composites can be made of unsigned ints, signed ints or floats (except for matrices that work with floats only).
enum ShaderType
{
	SHADER_TYPE_MATRIX_COPY					= 0,
	SHADER_TYPE_VECTOR_COPY,
	SHADER_TYPE_SCALAR_COPY,

	SHADER_TYPE_COUNT
};

// We are testing reads or writes
// In case of testing reads - writes are always
enum BufferAccessType
{
	BUFFER_ACCESS_TYPE_READ_FROM_STORAGE	= 0,
	BUFFER_ACCESS_TYPE_WRITE_TO_STORAGE,
};

// Test case for checking robust buffer access with variable pointers
class RobustAccessWithPointersTest : public vkt::TestCase
{
public:
	static const deUint32		s_testArraySize;
	static const deUint32		s_numberOfBytesAccessed;

								RobustAccessWithPointersTest	(tcu::TestContext&		testContext,
																 const std::string&		name,
																 const std::string&		description,
																 VkShaderStageFlags		shaderStage,
																 ShaderType				shaderType,
																 VkFormat				bufferFormat);

	virtual						~RobustAccessWithPointersTest	(void)
	{
	}

	void						checkSupport (Context &context) const override;

protected:
	const VkShaderStageFlags	m_shaderStage;
	const ShaderType			m_shaderType;
	const VkFormat				m_bufferFormat;
};

const deUint32 RobustAccessWithPointersTest::s_testArraySize = 1024u;
const deUint32 RobustAccessWithPointersTest::s_numberOfBytesAccessed = static_cast<deUint32>(16ull * sizeof(float));

RobustAccessWithPointersTest::RobustAccessWithPointersTest(tcu::TestContext&		testContext,
	const std::string&		name,
	const std::string&		description,
	VkShaderStageFlags		shaderStage,
	ShaderType				shaderType,
	VkFormat				bufferFormat)
	: vkt::TestCase(testContext, name, description)
	, m_shaderStage(shaderStage)
	, m_shaderType(shaderType)
	, m_bufferFormat(bufferFormat)
{
	DE_ASSERT(m_shaderStage == VK_SHADER_STAGE_VERTEX_BIT || m_shaderStage == VK_SHADER_STAGE_FRAGMENT_BIT || m_shaderStage == VK_SHADER_STAGE_COMPUTE_BIT);
}

void RobustAccessWithPointersTest::checkSupport (Context &context) const
{
	const auto& pointerFeatures = context.getVariablePointersFeatures();
	if (!pointerFeatures.variablePointersStorageBuffer)
		TCU_THROW(NotSupportedError, "VariablePointersStorageBuffer SPIR-V capability not supported");

	if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") && !context.getDeviceFeatures().robustBufferAccess)
		TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: robustBufferAccess not supported by this implementation");
}

// A subclass for testing reading with variable pointers
class RobustReadTest : public RobustAccessWithPointersTest
{
public:
								RobustReadTest					(tcu::TestContext&		testContext,
																 const std::string&		name,
																 const std::string&		description,
																 VkShaderStageFlags		shaderStage,
																 ShaderType				shaderType,
																 VkFormat				bufferFormat,
																 VkDeviceSize			readAccessRange,
																 bool					accessOutOfBackingMemory);

	virtual						~RobustReadTest					(void)
	{}
	virtual TestInstance*		createInstance					(Context&				context) const;
private:
	virtual void				initPrograms					(SourceCollections&		programCollection) const;
	const VkDeviceSize			m_readAccessRange;
	const bool					m_accessOutOfBackingMemory;
};

// A subclass for testing writing with variable pointers
class RobustWriteTest : public RobustAccessWithPointersTest
{
public:
								RobustWriteTest				(tcu::TestContext&		testContext,
															 const std::string&		name,
															 const std::string&		description,
															 VkShaderStageFlags		shaderStage,
															 ShaderType				shaderType,
															 VkFormat				bufferFormat,
															 VkDeviceSize			writeAccessRange,
															 bool					accessOutOfBackingMemory);

	virtual						~RobustWriteTest			(void) {}
	virtual TestInstance*		createInstance				(Context& context) const;
private:
	virtual void				initPrograms				(SourceCollections&		programCollection) const;
	const VkDeviceSize			m_writeAccessRange;
	const bool					m_accessOutOfBackingMemory;
};

// In case I detect that some prerequisites are not fullfilled I am creating this lightweight empty test instance instead of AccessInstance. Should be bit faster that way.
class NotSupportedInstance : public vkt::TestInstance
{
public:
								NotSupportedInstance		(Context&			context,
															 const std::string&	message)
		: TestInstance(context)
		, m_notSupportedMessage(message)
	{}

	virtual						~NotSupportedInstance		(void)
	{
	}

	virtual tcu::TestStatus		iterate						(void)
	{
		TCU_THROW(NotSupportedError, m_notSupportedMessage.c_str());
	}

private:
	std::string					m_notSupportedMessage;
};

// A superclass for instances testing reading and writing
// holds all necessary object members
class AccessInstance : public vkt::TestInstance
{
public:
								AccessInstance				(Context&			context,
															 Move<VkDevice>		device,
#ifndef CTS_USES_VULKANSC
															 de::MovePtr<vk::DeviceDriver>		deviceDriver,
#else
															 de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter>	deviceDriver,
#endif // CTS_USES_VULKANSC
															 ShaderType			shaderType,
															 VkShaderStageFlags	shaderStage,
															 VkFormat			bufferFormat,
															 BufferAccessType	bufferAccessType,
															 VkDeviceSize		inBufferAccessRange,
															 VkDeviceSize		outBufferAccessRange,
															 bool				accessOutOfBackingMemory);

	virtual						~AccessInstance				(void);

	virtual tcu::TestStatus		iterate						(void);

	virtual bool				verifyResult				(bool splitAccess = false);

private:
	bool						isExpectedValueFromInBuffer	(VkDeviceSize		offsetInBytes,
															 const void*		valuePtr,
															 VkDeviceSize		valueSize);
	bool						isOutBufferValueUnchanged	(VkDeviceSize		offsetInBytes,
															 VkDeviceSize		valueSize);

protected:
	Move<VkDevice>							m_device;
#ifndef CTS_USES_VULKANSC
	de::MovePtr<vk::DeviceDriver>			m_deviceDriver;
#else
	de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter>	m_deviceDriver;
#endif // CTS_USES_VULKANSC
	de::MovePtr<TestEnvironment>m_testEnvironment;

	const ShaderType			m_shaderType;
	const VkShaderStageFlags	m_shaderStage;

	const VkFormat				m_bufferFormat;
	const BufferAccessType		m_bufferAccessType;

	AccessRangesData			m_inBufferAccess;
	Move<VkBuffer>				m_inBuffer;
	de::MovePtr<Allocation>		m_inBufferAlloc;

	AccessRangesData			m_outBufferAccess;
	Move<VkBuffer>				m_outBuffer;
	de::MovePtr<Allocation>		m_outBufferAlloc;

	Move<VkBuffer>				m_indicesBuffer;
	de::MovePtr<Allocation>		m_indicesBufferAlloc;

	Move<VkDescriptorPool>		m_descriptorPool;
	Move<VkDescriptorSetLayout>	m_descriptorSetLayout;
	Move<VkDescriptorSet>		m_descriptorSet;

	Move<VkFence>				m_fence;
	VkQueue						m_queue;

	// Used when m_shaderStage == VK_SHADER_STAGE_VERTEX_BIT
	Move<VkBuffer>				m_vertexBuffer;
	de::MovePtr<Allocation>		m_vertexBufferAlloc;

	const bool					m_accessOutOfBackingMemory;
};

// A subclass for read tests
class ReadInstance: public AccessInstance
{
public:
								ReadInstance			(Context&				context,
														 Move<VkDevice>			device,
#ifndef CTS_USES_VULKANSC
														 de::MovePtr<vk::DeviceDriver>		deviceDriver,
#else
														 de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter>	deviceDriver,
#endif // CTS_USES_VULKANSC
														 ShaderType				shaderType,
														 VkShaderStageFlags		shaderStage,
														 VkFormat				bufferFormat,
														 VkDeviceSize			inBufferAccessRange,
														 bool					accessOutOfBackingMemory);

	virtual						~ReadInstance			(void) {}
};

// A subclass for write tests
class WriteInstance: public AccessInstance
{
public:
								WriteInstance			(Context&				context,
														 Move<VkDevice>			device,
#ifndef CTS_USES_VULKANSC
														 de::MovePtr<vk::DeviceDriver>		deviceDriver,
#else
														 de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter>	deviceDriver,
#endif // CTS_USES_VULKANSC
														 ShaderType				shaderType,
														 VkShaderStageFlags		shaderStage,
														 VkFormat				bufferFormat,
														 VkDeviceSize			writeBufferAccessRange,
														 bool					accessOutOfBackingMemory);

	virtual						~WriteInstance			(void) {}
};

// Automatically incremented counter.
// Each read of value bumps counter up.
class Autocounter
{
public:
								Autocounter()
		:value(0u)
	{}
	deUint32					incrementAndGetValue()
	{
		return ++value;
	}
private:
	deUint32					value;
};

// A class representing SPIRV variable.
// This class internally has an unique identificator.
// When such variable is used in shader composition routine it is mapped on a in-SPIRV-code variable name.
class Variable
{
	friend bool					operator < (const Variable& a, const Variable& b);
public:
								Variable(Autocounter& autoincrement)
		: value(autoincrement.incrementAndGetValue())
	{}
private:
	deUint32					value;
};

bool operator < (const Variable& a, const Variable& b)
{
	return a.value < b.value;
}

// A class representing SPIRV operation.
// Since those are not copyable they don't need internal id. Memory address is used instead.
class Operation
{
	friend bool					operator==(const Operation& a, const Operation& b);
public:
								Operation(const char* text)
		: value(text)
	{
	}
	const std::string&			getValue() const
	{
		return value;
	}

private:
								Operation(const Operation& other);
	const std::string			value;
};

bool operator == (const Operation& a, const Operation& b)
{
	return &a == &b; // a fast & simple address comparison - making copies was disabled
}

// A namespace containing all SPIRV operations used in those tests.
namespace op {
#define OP(name) const Operation name("Op"#name)
	OP(Capability);
	OP(Extension);
	OP(ExtInstImport);
	OP(EntryPoint);
	OP(MemoryModel);
	OP(ExecutionMode);

	OP(Decorate);
	OP(MemberDecorate);
	OP(Name);
	OP(MemberName);

	OP(TypeVoid);
	OP(TypeBool);
	OP(TypeInt);
	OP(TypeFloat);
	OP(TypeVector);
	OP(TypeMatrix);
	OP(TypeArray);
	OP(TypeStruct);
	OP(TypeFunction);
	OP(TypePointer);
	OP(TypeImage);
	OP(TypeSampledImage);

	OP(Constant);
	OP(ConstantComposite);
	OP(Variable);

	OP(Function);
	OP(FunctionEnd);
	OP(Label);
	OP(Return);

	OP(LogicalEqual);
	OP(IEqual);
	OP(Select);

	OP(AccessChain);
	OP(Load);
	OP(Store);
#undef OP
}

// A class that allows to easily compose SPIRV code.
// This class automatically keeps correct order of most of operations
// i.e. capabilities to the top,
class ShaderStream
{
public:
								ShaderStream ()
	{}
	// composes shader string out of shader substreams.
	std::string					str () const
	{
		std::stringstream stream;
		stream << capabilities.str()
			<< "; ----------------- PREAMBLE -----------------\n"
			<< preamble.str()
			<< "; ----------------- DEBUG --------------------\n"
			<< names.str()
			<< "; ----------------- DECORATIONS --------------\n"
			<< decorations.str()
			<< "; ----------------- TYPES --------------------\n"
			<< basictypes.str()
			<< "; ----------------- CONSTANTS ----------------\n"
			<< constants.str()
			<< "; ----------------- ADVANCED TYPES -----------\n"
			<< compositetypes.str()
			<< ((compositeconstants.str().length() > 0) ? "; ----------------- CONSTANTS ----------------\n" : "")
			<< compositeconstants.str()
			<< "; ----------------- VARIABLES & FUNCTIONS ----\n"
			<< shaderstream.str();
		return stream.str();
	}
	// Functions below are used to push Operations, Variables and other strings, numbers and characters to the shader.
	// Each function uses selectStream and map subroutines.
	// selectStream is used to choose a proper substream of shader.
	// E.g. if an operation is OpConstant it should be put into constants definitions stream - so selectStream will return that stream.
	// map on the other hand is used to replace Variables and Operations to their in-SPIRV-code representations.
	// for types like ints or floats map simply calls << operator to produce its string representation
	// for Operations a proper operation string is returned
	// for Variables there is a special mapping between in-C++ variable and in-SPIRV-code variable name.
	// following sequence of functions could be squashed to just two using variadic templates once we move to C++11 or higher
	// each method returns *this to allow chaining calls to these methods.
	template <typename T>
	ShaderStream&				operator () (const T& a)
	{
		selectStream(a, 0) << map(a) << '\n';
		return *this;
	}
	template <typename T1, typename T2>
	ShaderStream&				operator () (const T1& a, const T2& b)
	{
		selectStream(a, 0) << map(a) << '\t' << map(b) << '\n';
		return *this;
	}
	template <typename T1, typename T2, typename T3>
	ShaderStream&				operator () (const T1& a, const T2& b, const T3& c)
	{
		selectStream(a, c) << map(a) << '\t' << map(b) << '\t' << map(c) << '\n';
		return *this;
	}
	template <typename T1, typename T2, typename T3, typename T4>
	ShaderStream&				operator () (const T1& a, const T2& b, const T3& c, const T4& d)
	{
		selectStream(a, c) << map(a) << '\t' << map(b) << '\t' << map(c) << '\t' << map(d) << '\n';
		return *this;
	}
	template <typename T1, typename T2, typename T3, typename T4, typename T5>
	ShaderStream&				operator () (const T1& a, const T2& b, const T3& c, const T4& d, const T5& e)
	{
		selectStream(a, c) << map(a) << '\t' << map(b) << '\t' << map(c) << '\t' << map(d) << '\t' << map(e) << '\n';
		return *this;
	}
	template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
	ShaderStream&				operator () (const T1& a, const T2& b, const T3& c, const T4& d, const T5& e, const T6& f)
	{
		selectStream(a, c) << map(a) << '\t' << map(b) << '\t' << map(c) << '\t' << map(d) << '\t' << map(e) << '\t' << map(f) << '\n';
		return *this;
	}
	template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
	ShaderStream&				operator () (const T1& a, const T2& b, const  T3& c, const T4& d, const T5& e, const T6& f, const T7& g)
	{
		selectStream(a, c) << map(a) << '\t' << map(b) << '\t' << map(c) << '\t' << map(d) << '\t' << map(e) << '\t' << map(f) << '\t' << map(g) << '\n';
		return *this;
	}
	template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
	ShaderStream&				operator () (const T1& a, const T2& b, const  T3& c, const T4& d, const T5& e, const T6& f, const T7& g, const T8& h)
	{
		selectStream(a, c) << map(a) << '\t' << map(b) << '\t' << map(c) << '\t' << map(d) << '\t' << map(e) << '\t' << map(f) << '\t' << map(g) << '\t' << map(h) << '\n';
		return *this;
	}
	template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9>
	ShaderStream&				operator () (const T1& a, const T2& b, const  T3& c, const T4& d, const T5& e, const T6& f, const T7& g, const T8& h, const T9& i)
	{
		selectStream(a, c) << map(a) << '\t' << map(b) << '\t' << map(c) << '\t' << map(d) << '\t' << map(e) << '\t' << map(f) << '\t' << map(g) << '\t' << map(h) << '\t' << map(i) << '\n';
		return *this;
	}
	template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8, typename T9, typename T10>
	ShaderStream&				operator () (const T1& a, const T2& b, const  T3& c, const T4& d, const T5& e, const T6& f, const T7& g, const T8& h, const T9& i, const T10& k)
	{
		selectStream(a, c) << map(a) << '\t' << map(b) << '\t' << map(c) << '\t' << map(d) << '\t' << map(e) << '\t' << map(f) << '\t' << map(g) << '\t' << map(h) << '\t' << map(i) << '\t' << map(k) << '\n';
		return *this;
	}

	// returns true if two variables has the same in-SPIRV-code names
	bool						areSame (const Variable a, const Variable b)
	{
		VariableIt varA = vars.find(a);
		VariableIt varB = vars.find(b);
		return varA != vars.end() && varB != vars.end() && varA->second == varB->second;
	}

	// makes variable 'a' in-SPIRV-code name to be the same as variable 'b' in-SPIRV-code name
	void						makeSame (const Variable a, const Variable b)
	{
		VariableIt varB = vars.find(b);
		if (varB != vars.end())
		{
			std::pair<VariableIt, bool> inserted = vars.insert(std::make_pair(a, varB->second));
			if (!inserted.second)
				inserted.first->second = varB->second;
		}
	}
private:
	// generic version of map (tries to push whatever came to stringstream to get its string representation)
	template <typename T>
	std::string					map (const T& a)
	{
		std::stringstream temp;
		temp << a;
		return temp.str();
	}

	// looks for mapping of c++ Variable object onto in-SPIRV-code name.
	// if there was not yet such mapping generated a new mapping is created based on incremented local counter.
	std::string					map (const Variable& a)
	{
		VariableIt var = vars.find(a);
		if (var != vars.end())
			return var->second;
		std::stringstream temp;
		temp << '%';
		temp.width(4);
		temp.fill('0');
		temp << std::hex << varCounter.incrementAndGetValue();
		vars.insert(std::make_pair(a, temp.str()));
		return temp.str();
	}

	// a simple specification for Operation
	std::string					map (const Operation& a)
	{
		return a.getValue();
	}

	// a specification for char* - faster than going through stringstream << operator
	std::string					map (const char*& a)
	{
		return std::string(a);
	}

	// a specification for char - faster than going through stringstream << operator
	std::string					map (const char& a)
	{
		return std::string(1, a);
	}

	// a generic version of selectStream - used when neither 1st nor 3rd SPIRV line token is Operation.
	// In general should never happen.
	// All SPIRV lines are constructed in a one of two forms:
	// Variable = Operation operands...
	// or
	// Operation operands...
	// So operation is either 1st or 3rd token.
	template <typename T0, typename T1>
	std::stringstream&			selectStream (const T0& op0, const T1& op1)
	{
		DE_UNREF(op0);
		DE_UNREF(op1);
		return shaderstream;
	}

	// Specialisation for Operation being 1st parameter
	// Certain operations make the SPIRV code line to be pushed to different substreams.
	template <typename T1>
	std::stringstream&			selectStream (const Operation& op, const T1& op1)
	{
		DE_UNREF(op1);
		if (op == op::Decorate || op == op::MemberDecorate)
			return decorations;
		if (op == op::Name || op == op::MemberName)
			return names;
		if (op == op::Capability || op == op::Extension)
			return capabilities;
		if (op == op::MemoryModel || op == op::ExecutionMode || op == op::EntryPoint)
			return preamble;
		return shaderstream;
	}

	// Specialisation for Operation being 3rd parameter
	// Certain operations make the SPIRV code line to be pushed to different substreams.
	// If we would like to use this way of generating SPIRV we could use this method as SPIRV line validation point
	// e.g. here instead of heving partial specialisation I could specialise for T0 being Variable since this has to match Variable = Operation operands...
	template <typename T0>
	std::stringstream&			selectStream (const T0& op0, const Operation& op)
	{
		DE_UNREF(op0);
		if (op == op::ExtInstImport)
			return preamble;
		if (op == op::TypeVoid || op == op::TypeBool || op == op::TypeInt || op == op::TypeFloat || op == op::TypeVector || op == op::TypeMatrix)
			return basictypes;
		if (op == op::TypeArray || op == op::TypeStruct || op == op::TypeFunction || op == op::TypePointer || op == op::TypeImage || op == op::TypeSampledImage)
			return compositetypes;
		if (op == op::Constant)
			return constants;
		if (op == op::ConstantComposite)
			return compositeconstants;
		return shaderstream;
	}

	typedef std::map<Variable, std::string>	VariablesPack;
	typedef VariablesPack::iterator			VariableIt;

	// local mappings between c++ Variable objects and in-SPIRV-code names
	VariablesPack				vars;

	// shader substreams
	std::stringstream			capabilities;
	std::stringstream			preamble;
	std::stringstream			names;
	std::stringstream			decorations;
	std::stringstream			basictypes;
	std::stringstream			constants;
	std::stringstream			compositetypes;
	std::stringstream			compositeconstants;
	std::stringstream			shaderstream;

	// local incremented counter
	Autocounter					varCounter;
};

// A suppliementary class to group frequently used Variables together
class Variables
{
public:
								Variables (Autocounter &autoincrement)
		: version(autoincrement)
		, mainFunc(autoincrement)
		, mainFuncLabel(autoincrement)
		, voidFuncVoid(autoincrement)
		, copy_type(autoincrement)
		, copy_type_vec(autoincrement)
		, buffer_type_vec(autoincrement)
		, copy_type_ptr(autoincrement)
		, buffer_type(autoincrement)
		, voidId(autoincrement)
		, v4f32(autoincrement)
		, v4s32(autoincrement)
		, v4u32(autoincrement)
		, v4s64(autoincrement)
		, v4u64(autoincrement)
		, s32(autoincrement)
		, f32(autoincrement)
		, u32(autoincrement)
		, s64(autoincrement)
		, u64(autoincrement)
		, boolean(autoincrement)
		, array_content_type(autoincrement)
		, s32_type_ptr(autoincrement)
		, dataSelectorStructPtrType(autoincrement)
		, dataSelectorStructPtr(autoincrement)
		, dataArrayType(autoincrement)
		, dataInput(autoincrement)
		, dataInputPtrType(autoincrement)
		, dataInputType(autoincrement)
		, dataInputSampledType(autoincrement)
		, dataOutput(autoincrement)
		, dataOutputPtrType(autoincrement)
		, dataOutputType(autoincrement)
		, dataSelectorStructType(autoincrement)
		, input(autoincrement)
		, inputPtr(autoincrement)
		, output(autoincrement)
		, outputPtr(autoincrement)
	{
		for (deUint32 i = 0; i < 32; ++i)
			constants.push_back(Variable(autoincrement));
	}
	const Variable				version;
	const Variable				mainFunc;
	const Variable				mainFuncLabel;
	const Variable				voidFuncVoid;
	std::vector<Variable>		constants;
	const Variable				copy_type;
	const Variable				copy_type_vec;
	const Variable				buffer_type_vec;
	const Variable				copy_type_ptr;
	const Variable				buffer_type;
	const Variable				voidId;
	const Variable				v4f32;
	const Variable				v4s32;
	const Variable				v4u32;
	const Variable				v4s64;
	const Variable				v4u64;
	const Variable				s32;
	const Variable				f32;
	const Variable				u32;
	const Variable				s64;
	const Variable				u64;
	const Variable				boolean;
	const Variable				array_content_type;
	const Variable				s32_type_ptr;
	const Variable				dataSelectorStructPtrType;
	const Variable				dataSelectorStructPtr;
	const Variable				dataArrayType;
	const Variable				dataInput;
	const Variable				dataInputPtrType;
	const Variable				dataInputType;
	const Variable				dataInputSampledType;
	const Variable				dataOutput;
	const Variable				dataOutputPtrType;
	const Variable				dataOutputType;
	const Variable				dataSelectorStructType;
	const Variable				input;
	const Variable				inputPtr;
	const Variable				output;
	const Variable				outputPtr;
};

// A routing generating SPIRV code for all test cases in this group
std::string MakeShader(VkShaderStageFlags shaderStage, ShaderType shaderType, VkFormat bufferFormat, bool reads, bool unused)
{
	const bool					isR64				= (bufferFormat == VK_FORMAT_R64_UINT || bufferFormat == VK_FORMAT_R64_SINT);
	// faster to write
	const char					is					= '=';

	// variables require such counter to generate their unique ids. Since there is possibility that in the future this code will
	// run parallel this counter is made local to this function body to be safe.
	Autocounter					localcounter;

	// A frequently used Variables (gathered into this single object for readability)
	Variables					var					(localcounter);

	// A SPIRV code builder
	ShaderStream				shaderSource;

	// A basic preamble of SPIRV shader. Turns on required capabilities and extensions.
	shaderSource
	(op::Capability, "Shader")
	(op::Capability, "VariablePointersStorageBuffer");

	if (isR64)
	{
		shaderSource
		(op::Capability, "Int64");
	}

	shaderSource
	(op::Extension, "\"SPV_KHR_storage_buffer_storage_class\"")
	(op::Extension, "\"SPV_KHR_variable_pointers\"")
	(var.version, is, op::ExtInstImport, "\"GLSL.std.450\"")
	(op::MemoryModel, "Logical", "GLSL450");

	// Use correct entry point definition depending on shader stage
	if (shaderStage == VK_SHADER_STAGE_COMPUTE_BIT)
	{
		shaderSource
		(op::EntryPoint, "GLCompute", var.mainFunc, "\"main\"")
		(op::ExecutionMode, var.mainFunc, "LocalSize", 1, 1, 1);
	}
	else if (shaderStage == VK_SHADER_STAGE_VERTEX_BIT)
	{
		shaderSource
		(op::EntryPoint, "Vertex", var.mainFunc, "\"main\"", var.input, var.output)
		(op::Decorate, var.output, "BuiltIn", "Position")
		(op::Decorate, var.input, "Location", 0);
	}
	else if (shaderStage == VK_SHADER_STAGE_FRAGMENT_BIT)
	{
		shaderSource
		(op::EntryPoint, "Fragment", var.mainFunc, "\"main\"", var.output)
		(op::ExecutionMode, var.mainFunc, "OriginUpperLeft")
		(op::Decorate, var.output, "Location", 0);
	}

	// If we are testing vertex shader or fragment shader we need to provide the other one for the pipeline too.
	// So the not tested one is 'unused'. It is then a minimal/simplest possible pass-through shader.
	// If we are testing compute shader we dont need unused shader at all.
	if (unused)
	{
		if (shaderStage == VK_SHADER_STAGE_FRAGMENT_BIT)
		{
			shaderSource
			(var.voidId, is, op::TypeVoid)
			(var.voidFuncVoid, is, op::TypeFunction, var.voidId)
			(var.f32, is, op::TypeFloat, 32)
			(var.v4f32, is, op::TypeVector, var.f32, 4)
			(var.outputPtr, is, op::TypePointer, "Output", var.v4f32)
			(var.output, is, op::Variable, var.outputPtr, "Output")
			(var.constants[6], is, op::Constant, var.f32, 1)
			(var.constants[7], is, op::ConstantComposite, var.v4f32, var.constants[6], var.constants[6], var.constants[6], var.constants[6])
			(var.mainFunc, is, op::Function, var.voidId, "None", var.voidFuncVoid)
			(var.mainFuncLabel, is, op::Label);
		}
		else if (shaderStage == VK_SHADER_STAGE_VERTEX_BIT)
		{
			shaderSource
			(var.voidId, is, op::TypeVoid)
			(var.voidFuncVoid, is, op::TypeFunction , var.voidId)
			(var.f32, is, op::TypeFloat, 32)
			(var.v4f32, is, op::TypeVector , var.f32, 4)
			(var.outputPtr, is, op::TypePointer, "Output" , var.v4f32)
			(var.output, is, op::Variable , var.outputPtr, "Output")
			(var.inputPtr, is, op::TypePointer, "Input" , var.v4f32)
			(var.input, is, op::Variable , var.inputPtr, "Input")
			(var.mainFunc, is, op::Function , var.voidId, "None", var.voidFuncVoid)
			(var.mainFuncLabel, is, op::Label);
		}
	}
	else // this is a start of actual shader that tests variable pointers
	{
		shaderSource
		(op::Decorate, var.dataInput, "DescriptorSet", 0)
		(op::Decorate, var.dataInput, "Binding", 0)

		(op::Decorate, var.dataOutput, "DescriptorSet", 0)
		(op::Decorate, var.dataOutput, "Binding", 1);

		// for scalar types and vector types we use 1024 element array of 4 elements arrays of 4-component vectors
		// so the stride of internal array is size of 4-component vector
		if (shaderType == SHADER_TYPE_SCALAR_COPY || shaderType == SHADER_TYPE_VECTOR_COPY)
		{
			if (isR64)
			{
				shaderSource
				(op::Decorate, var.array_content_type, "ArrayStride", 32);
			}
			else
			{
				shaderSource
				(op::Decorate, var.array_content_type, "ArrayStride", 16);
			}
		}

		if (isR64)
		{
			shaderSource
			(op::Decorate, var.dataArrayType, "ArrayStride", 128);
		}
		else
		{
			// for matrices we use array of 4x4-component matrices
			// stride of outer array is then 64 in every case
			shaderSource
			(op::Decorate, var.dataArrayType, "ArrayStride", 64);
		}

		// an output block
		shaderSource
		(op::MemberDecorate, var.dataOutputType, 0, "Offset", 0)
		(op::Decorate, var.dataOutputType, "Block")

		// an input block. Marked readonly.
		(op::MemberDecorate, var.dataInputType, 0, "NonWritable")
		(op::MemberDecorate, var.dataInputType, 0, "Offset", 0)
		(op::Decorate, var.dataInputType, "Block")

		//a special structure matching data in one of our buffers.
		// member at 0 is an index to read position
		// member at 1 is an index to write position
		// member at 2 is always zero. It is used to perform OpSelect. I used value coming from buffer to avoid incidental optimisations that could prune OpSelect if the value was compile time known.
		(op::MemberDecorate, var.dataSelectorStructType, 0, "Offset", 0)
		(op::MemberDecorate, var.dataSelectorStructType, 1, "Offset", 4)
		(op::MemberDecorate, var.dataSelectorStructType, 2, "Offset", 8)
		(op::Decorate, var.dataSelectorStructType, "Block")

		// binding to matching buffer
		(op::Decorate, var.dataSelectorStructPtr, "DescriptorSet", 0)
		(op::Decorate, var.dataSelectorStructPtr, "Binding", 2)

		// making composite types used in shader
		(var.voidId, is, op::TypeVoid)
		(var.voidFuncVoid, is, op::TypeFunction, var.voidId)

		(var.boolean, is, op::TypeBool)

		(var.f32, is, op::TypeFloat, 32)
		(var.s32, is, op::TypeInt, 32, 1)
		(var.u32, is, op::TypeInt, 32, 0);

		if (isR64)
		{
			shaderSource
			(var.s64, is, op::TypeInt, 64, 1)
			(var.u64, is, op::TypeInt, 64, 0);
		}

		shaderSource
		(var.v4f32, is, op::TypeVector, var.f32, 4)
		(var.v4s32, is, op::TypeVector, var.s32, 4)
		(var.v4u32, is, op::TypeVector, var.u32, 4);

		if (isR64)
		{
			shaderSource
			(var.v4s64, is, op::TypeVector, var.s64, 4)
			(var.v4u64, is, op::TypeVector, var.u64, 4);
		}

		// since the shared tests scalars, vectors, matrices of ints, uints and floats I am generating alternative names for some of the types so I can use those and not need to use "if" everywhere.
		// A Variable mappings will make sure the proper variable name is used
		// below is a first part of aliasing types based on int, uint, float
		switch (bufferFormat)
		{
		case vk::VK_FORMAT_R32_SINT:
			shaderSource.makeSame(var.buffer_type, var.s32);
			shaderSource.makeSame(var.buffer_type_vec, var.v4s32);
			break;
		case vk::VK_FORMAT_R32_UINT:
			shaderSource.makeSame(var.buffer_type, var.u32);
			shaderSource.makeSame(var.buffer_type_vec, var.v4u32);
			break;
		case vk::VK_FORMAT_R32_SFLOAT:
			shaderSource.makeSame(var.buffer_type, var.f32);
			shaderSource.makeSame(var.buffer_type_vec, var.v4f32);
			break;
		case vk::VK_FORMAT_R64_SINT:
			shaderSource.makeSame(var.buffer_type, var.s64);
			shaderSource.makeSame(var.buffer_type_vec, var.v4s64);
			break;
		case vk::VK_FORMAT_R64_UINT:
			shaderSource.makeSame(var.buffer_type, var.u64);
			shaderSource.makeSame(var.buffer_type_vec, var.v4u64);
			break;
		default:
			// to prevent compiler from complaining not all cases are handled (but we should not get here).
			deAssertFail("This point should be not reachable with correct program flow.", __FILE__, __LINE__);
			break;
		}

		// below is a second part that aliases based on scalar, vector, matrix
		switch (shaderType)
		{
		case SHADER_TYPE_SCALAR_COPY:
			shaderSource.makeSame(var.copy_type, var.buffer_type);
			break;
		case SHADER_TYPE_VECTOR_COPY:
			shaderSource.makeSame(var.copy_type, var.buffer_type_vec);
			break;
		case SHADER_TYPE_MATRIX_COPY:
			if (bufferFormat != VK_FORMAT_R32_SFLOAT)
				TCU_THROW(NotSupportedError, "Matrices can be used only with floating point types.");
			shaderSource
			(var.copy_type, is, op::TypeMatrix, var.buffer_type_vec, 4);
			break;
		default:
			// to prevent compiler from complaining not all cases are handled (but we should not get here).
			deAssertFail("This point should be not reachable with correct program flow.", __FILE__, __LINE__);
			break;
		}

		// I will need some constants so lets add them to shader source
		shaderSource
		(var.constants[0], is, op::Constant, var.s32, 0)
		(var.constants[1], is, op::Constant, var.s32, 1)
		(var.constants[2], is, op::Constant, var.s32, 2)
		(var.constants[3], is, op::Constant, var.s32, 3)
		(var.constants[4], is, op::Constant, var.u32, 4)
		(var.constants[5], is, op::Constant, var.u32, 1024);

		// for fragment shaders I need additionally a constant vector (output "colour") so lets make it
		if (shaderStage == VK_SHADER_STAGE_FRAGMENT_BIT)
		{
			shaderSource
			(var.constants[6], is, op::Constant, var.f32, 1)
			(var.constants[7], is, op::ConstantComposite, var.v4f32, var.constants[6], var.constants[6], var.constants[6], var.constants[6]);
		}

		// additional alias for the type of content of this 1024-element outer array.
		if (shaderType == SHADER_TYPE_SCALAR_COPY || shaderType == SHADER_TYPE_VECTOR_COPY)
		{
			shaderSource
			(var.array_content_type, is, op::TypeArray, var.buffer_type_vec, var.constants[4]);
		}
		else
		{
			shaderSource.makeSame(var.array_content_type, var.copy_type);
		}

		// Lets create pointer types to the input data type, output data type and a struct
		// This must be distinct types due to different type decorations
		// Lets make also actual poiters to the data
		shaderSource
		(var.dataArrayType, is, op::TypeArray, var.array_content_type, var.constants[5])
		(var.dataInputType, is, op::TypeStruct, var.dataArrayType)
		(var.dataOutputType, is, op::TypeStruct, var.dataArrayType)
		(var.dataInputPtrType, is, op::TypePointer, "StorageBuffer", var.dataInputType)
		(var.dataOutputPtrType, is, op::TypePointer, "StorageBuffer", var.dataOutputType)
		(var.dataInput, is, op::Variable, var.dataInputPtrType, "StorageBuffer")
		(var.dataOutput, is, op::Variable, var.dataOutputPtrType, "StorageBuffer")
		(var.dataSelectorStructType, is, op::TypeStruct, var.s32, var.s32, var.s32)
		(var.dataSelectorStructPtrType, is, op::TypePointer, "Uniform", var.dataSelectorStructType)
		(var.dataSelectorStructPtr, is, op::Variable, var.dataSelectorStructPtrType, "Uniform");

		// we need also additional pointers to fullfil stage requirements on shaders inputs and outputs
		if (shaderStage == VK_SHADER_STAGE_VERTEX_BIT)
		{
			shaderSource
			(var.inputPtr, is, op::TypePointer, "Input", var.v4f32)
			(var.input, is, op::Variable, var.inputPtr, "Input")
			(var.outputPtr, is, op::TypePointer, "Output", var.v4f32)
			(var.output, is, op::Variable, var.outputPtr, "Output");
		}
		else if (shaderStage == VK_SHADER_STAGE_FRAGMENT_BIT)
		{
			shaderSource
			(var.outputPtr, is, op::TypePointer, "Output", var.v4f32)
			(var.output, is, op::Variable, var.outputPtr, "Output");
		}

		shaderSource
		(var.copy_type_ptr, is, op::TypePointer, "StorageBuffer", var.copy_type)
		(var.s32_type_ptr, is, op::TypePointer, "Uniform", var.s32);

		// Make a shader main function
		shaderSource
		(var.mainFunc, is, op::Function, var.voidId, "None", var.voidFuncVoid)
		(var.mainFuncLabel, is, op::Label);

		Variable copyFromPtr(localcounter), copyToPtr(localcounter), zeroPtr(localcounter);
		Variable copyFrom(localcounter), copyTo(localcounter), zero(localcounter);

		// Lets load data from our auxiliary buffer with reading index, writing index and zero.
		shaderSource
		(copyToPtr, is, op::AccessChain, var.s32_type_ptr, var.dataSelectorStructPtr, var.constants[1])
		(copyTo, is, op::Load, var.s32, copyToPtr)
		(copyFromPtr, is, op::AccessChain, var.s32_type_ptr, var.dataSelectorStructPtr, var.constants[0])
		(copyFrom, is, op::Load, var.s32, copyFromPtr)
		(zeroPtr, is, op::AccessChain, var.s32_type_ptr, var.dataSelectorStructPtr, var.constants[2])
		(zero, is, op::Load, var.s32, zeroPtr);

		// let start copying data using variable pointers
		switch (shaderType)
		{
		case SHADER_TYPE_SCALAR_COPY:
			for (int i = 0; i < 4; ++i)
			{
				for (int j = 0; j < 4; ++j)
				{
					Variable actualLoadChain(localcounter), actualStoreChain(localcounter), loadResult(localcounter);
					Variable selection(localcounter);
					Variable lcA(localcounter), lcB(localcounter), scA(localcounter), scB(localcounter);

					shaderSource
					(selection, is, op::IEqual, var.boolean, zero, var.constants[0]);

					if (reads)
					{
						// if we check reads we use variable pointers only for reading part
						shaderSource
						(lcA, is, op::AccessChain, var.copy_type_ptr, var.dataInput, var.constants[0], copyFrom, var.constants[i], var.constants[j])
						(lcB, is, op::AccessChain, var.copy_type_ptr, var.dataInput, var.constants[0], copyFrom, var.constants[i], var.constants[j])
						// actualLoadChain will be a variable pointer as it was created through OpSelect
						(actualLoadChain, is, op::Select, var.copy_type_ptr, selection, lcA, lcB)
						// actualStoreChain will be a regular pointer
						(actualStoreChain, is, op::AccessChain, var.copy_type_ptr, var.dataOutput, var.constants[0], copyTo, var.constants[i], var.constants[j]);
					}
					else
					{
						// if we check writes we use variable pointers only for writing part only
						shaderSource
						// actualLoadChain will be regular regualar pointer
						(actualLoadChain, is, op::AccessChain, var.copy_type_ptr, var.dataInput, var.constants[0], copyFrom, var.constants[i], var.constants[j])
						(scA, is, op::AccessChain, var.copy_type_ptr, var.dataOutput, var.constants[0], copyTo, var.constants[i], var.constants[j])
						(scB, is, op::AccessChain, var.copy_type_ptr, var.dataOutput, var.constants[0], copyTo, var.constants[i], var.constants[j])
						// actualStoreChain will be a variable pointer as it was created through OpSelect
						(actualStoreChain, is, op::Select, var.copy_type_ptr, selection, scA, scB);
					}
					// do actual copying
					shaderSource
					(loadResult, is, op::Load, var.copy_type, actualLoadChain)
					(op::Store, actualStoreChain, loadResult);
				}
			}
			break;
		// cases below have the same logic as the one above - just we are copying bigger chunks of data with every load/store pair
		case SHADER_TYPE_VECTOR_COPY:
			for (int i = 0; i < 4; ++i)
			{
				Variable actualLoadChain(localcounter), actualStoreChain(localcounter), loadResult(localcounter);
				Variable selection(localcounter);
				Variable lcA(localcounter), lcB(localcounter), scA(localcounter), scB(localcounter);

				shaderSource
				(selection, is, op::IEqual, var.boolean, zero, var.constants[0]);

				if (reads)
				{
					shaderSource
					(lcA, is, op::AccessChain, var.copy_type_ptr, var.dataInput, var.constants[0], copyFrom, var.constants[i])
					(lcB, is, op::AccessChain, var.copy_type_ptr, var.dataInput, var.constants[0], copyFrom, var.constants[i])
					(actualLoadChain, is, op::Select, var.copy_type_ptr, selection, lcA, lcB)
					(actualStoreChain, is, op::AccessChain, var.copy_type_ptr, var.dataOutput, var.constants[0], copyTo, var.constants[i]);
				}
				else
				{
					shaderSource
					(actualLoadChain, is, op::AccessChain, var.copy_type_ptr, var.dataInput, var.constants[0], copyFrom, var.constants[i])
					(scA, is, op::AccessChain, var.copy_type_ptr, var.dataOutput, var.constants[0], copyTo, var.constants[i])
					(scB, is, op::AccessChain, var.copy_type_ptr, var.dataOutput, var.constants[0], copyTo, var.constants[i])
					(actualStoreChain, is, op::Select, var.copy_type_ptr, selection, scA, scB);
				}

				shaderSource
				(loadResult, is, op::Load, var.copy_type, actualLoadChain)
				(op::Store, actualStoreChain, loadResult);
			}
			break;
		case SHADER_TYPE_MATRIX_COPY:
			{
				Variable actualLoadChain(localcounter), actualStoreChain(localcounter), loadResult(localcounter);
				Variable selection(localcounter);
				Variable lcA(localcounter), lcB(localcounter), scA(localcounter), scB(localcounter);

				shaderSource
				(selection, is, op::IEqual, var.boolean, zero, var.constants[0]);

				if (reads)
				{
					shaderSource
					(lcA, is, op::AccessChain, var.copy_type_ptr, var.dataInput, var.constants[0], copyFrom)
					(lcB, is, op::AccessChain, var.copy_type_ptr, var.dataInput, var.constants[0], copyFrom)
					(actualLoadChain, is, op::Select, var.copy_type_ptr, selection, lcA, lcB)
					(actualStoreChain, is, op::AccessChain, var.copy_type_ptr, var.dataOutput, var.constants[0], copyTo);
				}
				else
				{
					shaderSource
					(actualLoadChain, is, op::AccessChain, var.copy_type_ptr, var.dataInput, var.constants[0], copyFrom)
					(scA, is, op::AccessChain, var.copy_type_ptr, var.dataOutput, var.constants[0], copyTo)
					(scB, is, op::AccessChain, var.copy_type_ptr, var.dataOutput, var.constants[0], copyTo)
					(actualStoreChain, is, op::Select, var.copy_type_ptr, selection, scA, scB);
				}

				shaderSource
				(loadResult, is, op::Load, var.copy_type, actualLoadChain)
				(op::Store, actualStoreChain, loadResult);
			}
			break;
		default:
			// to prevent compiler from complaining not all cases are handled (but we should not get here).
			deAssertFail("This point should be not reachable with correct program flow.", __FILE__, __LINE__);
			break;
		}
	}

	// This is common for test shaders and unused ones
	// We need to fill stage ouput from shader properly
	// output vertices positions in vertex shader
	if (shaderStage == VK_SHADER_STAGE_VERTEX_BIT)
	{
		Variable inputValue(localcounter), outputLocation(localcounter);
		shaderSource
		(inputValue, is, op::Load, var.v4f32, var.input)
		(outputLocation, is, op::AccessChain, var.outputPtr, var.output)
		(op::Store, outputLocation, inputValue);
	}
	// output colour in fragment shader
	else if (shaderStage == VK_SHADER_STAGE_FRAGMENT_BIT)
	{
		shaderSource
		(op::Store, var.output, var.constants[7]);
	}

	// We are done. Lets close main function body
	shaderSource
	(op::Return)
	(op::FunctionEnd);

	return shaderSource.str();
}

RobustReadTest::RobustReadTest (tcu::TestContext&		testContext,
								const std::string&		name,
								const std::string&		description,
								VkShaderStageFlags		shaderStage,
								ShaderType				shaderType,
								VkFormat				bufferFormat,
								VkDeviceSize			readAccessRange,
								bool					accessOutOfBackingMemory)
	: RobustAccessWithPointersTest	(testContext, name, description, shaderStage, shaderType, bufferFormat)
	, m_readAccessRange				(readAccessRange)
	, m_accessOutOfBackingMemory	(accessOutOfBackingMemory)
{
}

TestInstance* RobustReadTest::createInstance (Context& context) const
{
	auto device = createRobustBufferAccessVariablePointersDevice(context);
#ifndef CTS_USES_VULKANSC
	de::MovePtr<vk::DeviceDriver>	deviceDriver = de::MovePtr<DeviceDriver>(new DeviceDriver(context.getPlatformInterface(), context.getInstance(), *device, context.getUsedApiVersion()));
#else
	de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter>	deviceDriver = de::MovePtr<DeviceDriverSC, DeinitDeviceDeleter>(new DeviceDriverSC(context.getPlatformInterface(), context.getInstance(), *device, context.getTestContext().getCommandLine(), context.getResourceInterface(), context.getDeviceVulkanSC10Properties(), context.getDeviceProperties(), context.getUsedApiVersion()), vk::DeinitDeviceDeleter(context.getResourceInterface().get(), *device));
#endif // CTS_USES_VULKANSC

	return new ReadInstance(context, device, deviceDriver, m_shaderType, m_shaderStage, m_bufferFormat, m_readAccessRange, m_accessOutOfBackingMemory);
}

void RobustReadTest::initPrograms(SourceCollections&	programCollection) const
{
	if (m_shaderStage == VK_SHADER_STAGE_COMPUTE_BIT)
	{
		programCollection.spirvAsmSources.add("compute") << MakeShader(VK_SHADER_STAGE_COMPUTE_BIT, m_shaderType, m_bufferFormat, true, false);
	}
	else
	{
		programCollection.spirvAsmSources.add("vertex") << MakeShader(VK_SHADER_STAGE_VERTEX_BIT, m_shaderType, m_bufferFormat, true, m_shaderStage != VK_SHADER_STAGE_VERTEX_BIT);
		programCollection.spirvAsmSources.add("fragment") << MakeShader(VK_SHADER_STAGE_FRAGMENT_BIT, m_shaderType, m_bufferFormat, true, m_shaderStage != VK_SHADER_STAGE_FRAGMENT_BIT);
	}
}

RobustWriteTest::RobustWriteTest (tcu::TestContext&		testContext,
								  const std::string&	name,
								  const std::string&	description,
								  VkShaderStageFlags	shaderStage,
								  ShaderType			shaderType,
								  VkFormat				bufferFormat,
								  VkDeviceSize			writeAccessRange,
								  bool					accessOutOfBackingMemory)

	: RobustAccessWithPointersTest	(testContext, name, description, shaderStage, shaderType, bufferFormat)
	, m_writeAccessRange			(writeAccessRange)
	, m_accessOutOfBackingMemory	(accessOutOfBackingMemory)
{
}

TestInstance* RobustWriteTest::createInstance (Context& context) const
{
	auto device = createRobustBufferAccessVariablePointersDevice(context);
#ifndef CTS_USES_VULKANSC
	de::MovePtr<vk::DeviceDriver>	deviceDriver = de::MovePtr<DeviceDriver>(new DeviceDriver(context.getPlatformInterface(), context.getInstance(), *device, context.getUsedApiVersion()));
#else
	de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter>	deviceDriver = de::MovePtr<DeviceDriverSC, DeinitDeviceDeleter>(new DeviceDriverSC(context.getPlatformInterface(), context.getInstance(), *device, context.getTestContext().getCommandLine(), context.getResourceInterface(), context.getDeviceVulkanSC10Properties(), context.getDeviceProperties(), context.getUsedApiVersion()), vk::DeinitDeviceDeleter(context.getResourceInterface().get(), *device));
#endif // CTS_USES_VULKANSC

	return new WriteInstance(context, device, deviceDriver, m_shaderType, m_shaderStage, m_bufferFormat, m_writeAccessRange, m_accessOutOfBackingMemory);
}

void RobustWriteTest::initPrograms(SourceCollections&	programCollection) const
{
	if (m_shaderStage == VK_SHADER_STAGE_COMPUTE_BIT)
	{
		programCollection.spirvAsmSources.add("compute") << MakeShader(VK_SHADER_STAGE_COMPUTE_BIT, m_shaderType, m_bufferFormat, false, false);
	}
	else
	{
		programCollection.spirvAsmSources.add("vertex") << MakeShader(VK_SHADER_STAGE_VERTEX_BIT, m_shaderType, m_bufferFormat, false, m_shaderStage != VK_SHADER_STAGE_VERTEX_BIT);
		programCollection.spirvAsmSources.add("fragment") << MakeShader(VK_SHADER_STAGE_FRAGMENT_BIT, m_shaderType, m_bufferFormat, false, m_shaderStage != VK_SHADER_STAGE_FRAGMENT_BIT);
	}
}

AccessInstance::AccessInstance (Context&			context,
								Move<VkDevice>		device,
#ifndef CTS_USES_VULKANSC
								de::MovePtr<vk::DeviceDriver>		deviceDriver,
#else
								de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter>	deviceDriver,
#endif // CTS_USES_VULKANSC

								ShaderType			shaderType,
								VkShaderStageFlags	shaderStage,
								VkFormat			bufferFormat,
								BufferAccessType	bufferAccessType,
								VkDeviceSize		inBufferAccessRange,
								VkDeviceSize		outBufferAccessRange,
								bool				accessOutOfBackingMemory)
	: vkt::TestInstance				(context)
	, m_device						(device)
	, m_deviceDriver				(deviceDriver)
	, m_shaderType					(shaderType)
	, m_shaderStage					(shaderStage)
	, m_bufferFormat				(bufferFormat)
	, m_bufferAccessType			(bufferAccessType)
	, m_accessOutOfBackingMemory	(accessOutOfBackingMemory)
{
	tcu::TestLog&									log						= context.getTestContext().getLog();
	const DeviceInterface&							vk						= *m_deviceDriver;
	const auto&										vki						= context.getInstanceInterface();
	const auto										instance				= context.getInstance();
	const deUint32									queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	const VkPhysicalDevice							physicalDevice			= chooseDevice(vki, instance, context.getTestContext().getCommandLine());
	SimpleAllocator									memAlloc				(vk, *m_device, getPhysicalDeviceMemoryProperties(vki, physicalDevice));

	DE_ASSERT(RobustAccessWithPointersTest::s_numberOfBytesAccessed % sizeof(deUint32) == 0);
	DE_ASSERT(inBufferAccessRange <= RobustAccessWithPointersTest::s_numberOfBytesAccessed);
	DE_ASSERT(outBufferAccessRange <= RobustAccessWithPointersTest::s_numberOfBytesAccessed);

	if (m_bufferFormat == VK_FORMAT_R64_UINT || m_bufferFormat == VK_FORMAT_R64_SINT)
	{
		context.requireDeviceFunctionality("VK_EXT_shader_image_atomic_int64");
	}

	// Check storage support
	if (shaderStage == VK_SHADER_STAGE_VERTEX_BIT)
	{
		if (!context.getDeviceFeatures().vertexPipelineStoresAndAtomics)
		{
			TCU_THROW(NotSupportedError, "Stores not supported in vertex stage");
		}
	}
	else if (shaderStage == VK_SHADER_STAGE_FRAGMENT_BIT)
	{
		if (!context.getDeviceFeatures().fragmentStoresAndAtomics)
		{
			TCU_THROW(NotSupportedError, "Stores not supported in fragment stage");
		}
	}

	createTestBuffer(context, vk, *m_device, inBufferAccessRange, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, memAlloc, m_inBuffer, m_inBufferAlloc, m_inBufferAccess, &populateBufferWithValues, &m_bufferFormat);
	createTestBuffer(context, vk, *m_device, outBufferAccessRange, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, memAlloc, m_outBuffer, m_outBufferAlloc, m_outBufferAccess, &populateBufferWithFiller, DE_NULL);

	deInt32 indices[] = {
		(m_accessOutOfBackingMemory && (m_bufferAccessType == BUFFER_ACCESS_TYPE_READ_FROM_STORAGE)) ? static_cast<deInt32>(RobustAccessWithPointersTest::s_testArraySize) - 1 : 0,
		(m_accessOutOfBackingMemory && (m_bufferAccessType == BUFFER_ACCESS_TYPE_WRITE_TO_STORAGE)) ? static_cast<deInt32>(RobustAccessWithPointersTest::s_testArraySize) - 1 : 0,
		0
	};
	AccessRangesData indicesAccess;
	createTestBuffer(context, vk, *m_device, 3 * sizeof(deInt32), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, memAlloc, m_indicesBuffer, m_indicesBufferAlloc, indicesAccess, &populateBufferWithCopy, &indices);

	log << tcu::TestLog::Message << "input  buffer - alloc size: " << m_inBufferAccess.allocSize << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "input  buffer - max access range: " << m_inBufferAccess.maxAccessRange << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "output buffer - alloc size: " << m_outBufferAccess.allocSize << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "output buffer - max access range: " << m_outBufferAccess.maxAccessRange << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "indices - input offset: " << indices[0] << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "indices - output offset: " << indices[1] << tcu::TestLog::EndMessage;
	log << tcu::TestLog::Message << "indices - additional: " << indices[2] << tcu::TestLog::EndMessage;

	// Create descriptor data
	{
		DescriptorPoolBuilder						descriptorPoolBuilder;
		descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u);
		descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u);
		descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u);
		m_descriptorPool = descriptorPoolBuilder.build(vk, *m_device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

		DescriptorSetLayoutBuilder					setLayoutBuilder;
		setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);
		setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);
		setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL);
		m_descriptorSetLayout = setLayoutBuilder.build(vk, *m_device);

		const VkDescriptorSetAllocateInfo			descriptorSetAllocateInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,		// VkStructureType	sType;
			DE_NULL,								// const void*					pNext;
			*m_descriptorPool,						// VkDescriptorPool				descriptorPool;
			1u,										// deUint32						setLayoutCount;
			&m_descriptorSetLayout.get()			// const VkDescriptorSetLayout*	pSetLayouts;
		};

		m_descriptorSet = allocateDescriptorSet(vk, *m_device, &descriptorSetAllocateInfo);

		const VkDescriptorBufferInfo				inBufferDescriptorInfo			= makeDescriptorBufferInfo(*m_inBuffer, 0ull, m_inBufferAccess.accessRange);
		const VkDescriptorBufferInfo				outBufferDescriptorInfo			= makeDescriptorBufferInfo(*m_outBuffer, 0ull, m_outBufferAccess.accessRange);
		const VkDescriptorBufferInfo				indicesBufferDescriptorInfo		= makeDescriptorBufferInfo(*m_indicesBuffer, 0ull, 12ull);

		DescriptorSetUpdateBuilder					setUpdateBuilder;
		setUpdateBuilder.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &inBufferDescriptorInfo);
		setUpdateBuilder.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outBufferDescriptorInfo);
		setUpdateBuilder.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &indicesBufferDescriptorInfo);
		setUpdateBuilder.update(vk, *m_device);
	}

	// Create fence
	{
		const VkFenceCreateInfo fenceParams =
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType			sType;
			DE_NULL,								// const void*				pNext;
			0u										// VkFenceCreateFlags		flags;
		};

		m_fence = createFence(vk, *m_device, &fenceParams);
	}

	// Get queue
	vk.getDeviceQueue(*m_device, queueFamilyIndex, 0, &m_queue);

	if (m_shaderStage == VK_SHADER_STAGE_COMPUTE_BIT)
	{
		m_testEnvironment = de::MovePtr<TestEnvironment>(new ComputeEnvironment(m_context, *m_deviceDriver, *m_device, *m_descriptorSetLayout, *m_descriptorSet));
	}
	else
	{
		using tcu::Vec4;

		const VkVertexInputBindingDescription		vertexInputBindingDescription =
		{
			0u,										// deUint32					binding;
			sizeof(tcu::Vec4),						// deUint32					strideInBytes;
			VK_VERTEX_INPUT_RATE_VERTEX				// VkVertexInputStepRate	inputRate;
		};

		const VkVertexInputAttributeDescription		vertexInputAttributeDescription =
		{
			0u,										// deUint32	location;
			0u,										// deUint32	binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,			// VkFormat	format;
			0u										// deUint32	offset;
		};

		AccessRangesData							vertexAccess;
		const Vec4									vertices[] =
		{
			Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
			Vec4(-1.0f,  1.0f, 0.0f, 1.0f),
			Vec4( 1.0f, -1.0f, 0.0f, 1.0f),
		};
		const VkDeviceSize							vertexBufferSize = static_cast<VkDeviceSize>(sizeof(vertices));
		createTestBuffer(context, vk, *m_device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, memAlloc, m_vertexBuffer, m_vertexBufferAlloc, vertexAccess, &populateBufferWithCopy, &vertices);

		const GraphicsEnvironment::DrawConfig		drawWithOneVertexBuffer =
		{
			std::vector<VkBuffer>(1, *m_vertexBuffer), // std::vector<VkBuffer>	vertexBuffers;
			DE_LENGTH_OF_ARRAY(vertices),			// deUint32					vertexCount;
			1,										// deUint32					instanceCount;
			DE_NULL,								// VkBuffer					indexBuffer;
			0u,										// deUint32					indexCount;
		};

		m_testEnvironment = de::MovePtr<TestEnvironment>(new GraphicsEnvironment(m_context,
																				 *m_deviceDriver,
																				 *m_device,
																				 *m_descriptorSetLayout,
																				 *m_descriptorSet,
																				 GraphicsEnvironment::VertexBindings(1, vertexInputBindingDescription),
																				 GraphicsEnvironment::VertexAttributes(1, vertexInputAttributeDescription),
																				 drawWithOneVertexBuffer));
	}
}

AccessInstance::~AccessInstance()
{
}

// Verifies if the buffer has the value initialized by BufferAccessInstance::populateReadBuffer at a given offset.
bool AccessInstance::isExpectedValueFromInBuffer (VkDeviceSize	offsetInBytes,
												  const void*	valuePtr,
												  VkDeviceSize	valueSize)
{
	DE_ASSERT(offsetInBytes % 4 == 0);
	DE_ASSERT(offsetInBytes < m_inBufferAccess.allocSize);
	DE_ASSERT(valueSize == 4ull || valueSize == 8ull);

	const deUint32 valueIndex = deUint32(offsetInBytes / 4) + 2;

	if (isUintFormat(m_bufferFormat))
	{
		const deUint32 expectedValues[2] = { valueIndex, valueIndex + 1u };
		return !deMemCmp(valuePtr, &expectedValues, (size_t)valueSize);
	}
	else if (isIntFormat(m_bufferFormat))
	{
		const deInt32 value				= -deInt32(valueIndex);
		const deInt32 expectedValues[2]	= { value, value - 1 };
		return !deMemCmp(valuePtr, &expectedValues, (size_t)valueSize);
	}
	else if (isFloatFormat(m_bufferFormat))
	{
		DE_ASSERT(valueSize == 4ull);
		const float value = float(valueIndex);
		return !deMemCmp(valuePtr, &value, (size_t)valueSize);
	}
	else
	{
		DE_ASSERT(false);
		return false;
	}
}

bool AccessInstance::isOutBufferValueUnchanged (VkDeviceSize offsetInBytes, VkDeviceSize valueSize)
{
	DE_ASSERT(valueSize <= 8);
	const deUint8 *const	outValuePtr		= (deUint8*)m_outBufferAlloc->getHostPtr() + offsetInBytes;
	const deUint64			defaultValue	= 0xBABABABABABABABAull;

	return !deMemCmp(outValuePtr, &defaultValue, (size_t)valueSize);
}

tcu::TestStatus AccessInstance::iterate (void)
{
	const DeviceInterface&		vk			= *m_deviceDriver;
	const vk::VkCommandBuffer	cmdBuffer	= m_testEnvironment->getCommandBuffer();

	// Submit command buffer
	{
		const VkSubmitInfo	submitInfo	=
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType				sType;
			DE_NULL,						// const void*					pNext;
			0u,								// deUint32						waitSemaphoreCount;
			DE_NULL,						// const VkSemaphore*			pWaitSemaphores;
			DE_NULL,						// const VkPIpelineStageFlags*	pWaitDstStageMask;
			1u,								// deUint32						commandBufferCount;
			&cmdBuffer,						// const VkCommandBuffer*		pCommandBuffers;
			0u,								// deUint32						signalSemaphoreCount;
			DE_NULL							// const VkSemaphore*			pSignalSemaphores;
		};

		VK_CHECK(vk.resetFences(*m_device, 1, &m_fence.get()));
		VK_CHECK(vk.queueSubmit(m_queue, 1, &submitInfo, *m_fence));
		VK_CHECK(vk.waitForFences(*m_device, 1, &m_fence.get(), true, ~(0ull) /* infinity */));
	}

	// Prepare result buffer for read
	{
		const VkMappedMemoryRange	outBufferRange	=
		{
			VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,	//  VkStructureType	sType;
			DE_NULL,								//  const void*		pNext;
			m_outBufferAlloc->getMemory(),			//  VkDeviceMemory	mem;
			0ull,									//  VkDeviceSize	offset;
			m_outBufferAccess.allocSize,			//  VkDeviceSize	size;
		};

		VK_CHECK(vk.invalidateMappedMemoryRanges(*m_device, 1u, &outBufferRange));
	}

	if (verifyResult())
		return tcu::TestStatus::pass("All values OK");
	else
		return tcu::TestStatus::fail("Invalid value(s) found");
}

bool AccessInstance::verifyResult (bool splitAccess)
{
	std::ostringstream	logMsg;
	tcu::TestLog&		log					= m_context.getTestContext().getLog();
	const bool			isReadAccess		= (m_bufferAccessType == BUFFER_ACCESS_TYPE_READ_FROM_STORAGE);
	const void*			inDataPtr			= m_inBufferAlloc->getHostPtr();
	const void*			outDataPtr			= m_outBufferAlloc->getHostPtr();
	bool				allOk				= true;
	deUint32			valueNdx			= 0;
	const VkDeviceSize	maxAccessRange		= isReadAccess ? m_inBufferAccess.maxAccessRange : m_outBufferAccess.maxAccessRange;
	const bool			isR64				= (m_bufferFormat == VK_FORMAT_R64_UINT || m_bufferFormat == VK_FORMAT_R64_SINT);
	const deUint32		unsplitElementSize	= (isR64 ? 8u : 4u);
	const deUint32		elementSize			= ((isR64 && !splitAccess) ? 8u : 4u);

	for (VkDeviceSize offsetInBytes = 0; offsetInBytes < m_outBufferAccess.allocSize; offsetInBytes += elementSize)
	{
		const deUint8*		outValuePtr		= static_cast<const deUint8*>(outDataPtr) + offsetInBytes;
		const size_t		outValueSize	= static_cast<size_t>(deMinu64(elementSize, (m_outBufferAccess.allocSize - offsetInBytes)));

		if (offsetInBytes >= RobustAccessWithPointersTest::s_numberOfBytesAccessed)
		{
			// The shader will only write 16 values into the result buffer. The rest of the values
			// should remain unchanged or may be modified if we are writing out of bounds.
			if (!isOutBufferValueUnchanged(offsetInBytes, outValueSize)
				&& (isReadAccess || !isValueWithinBufferOrZero(inDataPtr, m_inBufferAccess.allocSize, outValuePtr, 4)))
			{
				logMsg << "\nValue " << valueNdx++ << " has been modified with an unknown value: " << *(static_cast<const deUint32*>(static_cast<const void*>(outValuePtr)));
				allOk = false;
			}
		}
		else
		{
			const deInt32	distanceToOutOfBounds	= static_cast<deInt32>(maxAccessRange) - static_cast<deInt32>(offsetInBytes);
			bool			isOutOfBoundsAccess		= false;

			logMsg << "\n" << valueNdx++ << ": ";

			logValue(logMsg, outValuePtr, m_bufferFormat, outValueSize);

			if (m_accessOutOfBackingMemory)
				isOutOfBoundsAccess = true;

			// Check if the shader operation accessed an operand located less than 16 bytes away
			// from the out of bounds address. Less than 32 bytes away for 64 bit accesses.
			if (!isOutOfBoundsAccess && distanceToOutOfBounds < (isR64 ? 32 : 16))
			{
				deUint32 operandSize = 0;

				switch (m_shaderType)
				{
					case SHADER_TYPE_SCALAR_COPY:
						operandSize		= unsplitElementSize; // Size of scalar
						break;

					case SHADER_TYPE_VECTOR_COPY:
						operandSize		= unsplitElementSize * 4; // Size of vec4
						break;

					case SHADER_TYPE_MATRIX_COPY:
						operandSize		= unsplitElementSize * 16; // Size of mat4
						break;

					default:
						DE_ASSERT(false);
				}

				isOutOfBoundsAccess = (((offsetInBytes / operandSize) + 1) * operandSize > maxAccessRange);
			}

			if (isOutOfBoundsAccess)
			{
				logMsg << " (out of bounds " << (isReadAccess ? "read": "write") << ")";

				const bool	isValuePartiallyOutOfBounds = ((distanceToOutOfBounds > 0) && ((deUint32)distanceToOutOfBounds < elementSize));
				bool		isValidValue				= false;

				if (isValuePartiallyOutOfBounds && !m_accessOutOfBackingMemory)
				{
					// The value is partially out of bounds

					bool	isOutOfBoundsPartOk  = true;
					bool	isWithinBoundsPartOk = true;

					deUint32 inBoundPartSize = distanceToOutOfBounds;

					// For cases that partial element is out of bound, the part within the buffer allocated memory can be buffer content per spec.
					// We need to check it as a whole part.
					if (offsetInBytes + elementSize > m_inBufferAccess.allocSize)
					{
						inBoundPartSize = static_cast<deInt32>(m_inBufferAccess.allocSize) - static_cast<deInt32>(offsetInBytes);
					}

					if (isReadAccess)
					{
						isWithinBoundsPartOk	= isValueWithinBufferOrZero(inDataPtr, m_inBufferAccess.allocSize, outValuePtr, inBoundPartSize);
						isOutOfBoundsPartOk		= isValueWithinBufferOrZero(inDataPtr, m_inBufferAccess.allocSize, (deUint8*)outValuePtr + inBoundPartSize, outValueSize - inBoundPartSize);
					}
					else
					{
						isWithinBoundsPartOk	= isValueWithinBufferOrZero(inDataPtr, m_inBufferAccess.allocSize, outValuePtr, inBoundPartSize)
												  || isOutBufferValueUnchanged(offsetInBytes, inBoundPartSize);

						isOutOfBoundsPartOk		= isValueWithinBufferOrZero(inDataPtr, m_inBufferAccess.allocSize, (deUint8*)outValuePtr + inBoundPartSize, outValueSize - inBoundPartSize)
												  || isOutBufferValueUnchanged(offsetInBytes + inBoundPartSize, outValueSize - inBoundPartSize);
					}

					logMsg << ", first " << distanceToOutOfBounds << " byte(s) " << (isWithinBoundsPartOk ? "OK": "wrong");
					logMsg << ", last " << outValueSize - distanceToOutOfBounds << " byte(s) " << (isOutOfBoundsPartOk ? "OK": "wrong");

					isValidValue	= isWithinBoundsPartOk && isOutOfBoundsPartOk;
				}
				else
				{
					if (isReadAccess)
					{
						isValidValue	= isValueWithinBufferOrZero(inDataPtr, m_inBufferAccess.allocSize, outValuePtr, outValueSize);
					}
					else
					{
						isValidValue	= isOutBufferValueUnchanged(offsetInBytes, outValueSize);

						if (!isValidValue)
						{
							// Out of bounds writes may modify values withing the memory ranges bound to the buffer
							isValidValue	= isValueWithinBufferOrZero(inDataPtr, m_inBufferAccess.allocSize, outValuePtr, outValueSize);

							if (isValidValue)
								logMsg << ", OK, written within the memory range bound to the buffer";
						}
					}
				}

				if (!isValidValue && !splitAccess)
				{
					// Check if we are satisfying the [0, 0, 0, x] pattern, where x may be either 0 or 1,
					// or the maximum representable positive integer value (if the format is integer-based).

					const bool	canMatchVec4Pattern	= (isReadAccess
													&& !isValuePartiallyOutOfBounds
													&& (m_shaderType == SHADER_TYPE_VECTOR_COPY)
													&& (offsetInBytes / elementSize + 1) % 4 == 0);
					bool		matchesVec4Pattern	= false;

					if (canMatchVec4Pattern)
					{
						matchesVec4Pattern = verifyOutOfBoundsVec4(outValuePtr - 3u * elementSize, m_bufferFormat);
					}

					if (!canMatchVec4Pattern || !matchesVec4Pattern)
					{
						logMsg << ". Failed: ";

						if (isReadAccess)
						{
							logMsg << "expected value within the buffer range or 0";

							if (canMatchVec4Pattern)
								logMsg << ", or the [0, 0, 0, x] pattern";
						}
						else
						{
							logMsg << "written out of the range";
						}

						allOk = false;
					}
				}
			}
			else // We are within bounds
			{
				if (isReadAccess)
				{
					if (!isExpectedValueFromInBuffer(offsetInBytes, outValuePtr, elementSize))
					{
						logMsg << ", Failed: unexpected value";
						allOk = false;
					}
				}
				else
				{
					// Out of bounds writes may change values within the bounds.
					if (!isValueWithinBufferOrZero(inDataPtr, m_inBufferAccess.accessRange, outValuePtr, elementSize))
					{
						logMsg << ", Failed: unexpected value";
						allOk = false;
					}
				}
			}
		}
	}

	log << tcu::TestLog::Message << logMsg.str() << tcu::TestLog::EndMessage;

	if (!allOk && unsplitElementSize > 4u && !splitAccess)
	{
		// "Non-atomic accesses to storage buffers that are a multiple of 32 bits may be decomposed into 32-bit accesses that are individually bounds-checked."
		return verifyResult(true/*splitAccess*/);
	}

	return allOk;
}

// BufferReadInstance

ReadInstance::ReadInstance (Context&				context,
							Move<VkDevice>			device,
#ifndef CTS_USES_VULKANSC
							de::MovePtr<vk::DeviceDriver>	deviceDriver,
#else
							de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter>	deviceDriver,
#endif // CTS_USES_VULKANSC
							ShaderType				shaderType,
							VkShaderStageFlags		shaderStage,
							VkFormat				bufferFormat,
							//bool					readFromStorage,
							VkDeviceSize			inBufferAccessRange,
							bool					accessOutOfBackingMemory)

	: AccessInstance	(context, device, deviceDriver, shaderType, shaderStage, bufferFormat,
						 BUFFER_ACCESS_TYPE_READ_FROM_STORAGE,
						 inBufferAccessRange, RobustAccessWithPointersTest::s_numberOfBytesAccessed,
						 accessOutOfBackingMemory)
{
}

// BufferWriteInstance

WriteInstance::WriteInstance (Context&				context,
							  Move<VkDevice>		device,
#ifndef CTS_USES_VULKANSC
							  de::MovePtr<vk::DeviceDriver>		deviceDriver,
#else
							  de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter>	deviceDriver,
#endif // CTS_USES_VULKANSC
							  ShaderType			shaderType,
							  VkShaderStageFlags	shaderStage,
							  VkFormat				bufferFormat,
							  VkDeviceSize			writeBufferAccessRange,
							  bool					accessOutOfBackingMemory)

	: AccessInstance	(context, device, deviceDriver, shaderType, shaderStage, bufferFormat,
						 BUFFER_ACCESS_TYPE_WRITE_TO_STORAGE,
						 RobustAccessWithPointersTest::s_numberOfBytesAccessed, writeBufferAccessRange,
						 accessOutOfBackingMemory)
{
}

} // unnamed namespace

tcu::TestCaseGroup* createBufferAccessWithVariablePointersTests(tcu::TestContext& testCtx)
{
	// Lets make group for the tests
	de::MovePtr<tcu::TestCaseGroup> bufferAccessWithVariablePointersTests	(new tcu::TestCaseGroup(testCtx, "through_pointers", ""));

	// Lets add subgroups to better organise tests
	de::MovePtr<tcu::TestCaseGroup> computeWithVariablePointersTests		(new tcu::TestCaseGroup(testCtx, "compute", ""));
	de::MovePtr<tcu::TestCaseGroup> computeReads							(new tcu::TestCaseGroup(testCtx, "reads", ""));
	de::MovePtr<tcu::TestCaseGroup> computeWrites							(new tcu::TestCaseGroup(testCtx, "writes", ""));

	de::MovePtr<tcu::TestCaseGroup> graphicsWithVariablePointersTests		(new tcu::TestCaseGroup(testCtx, "graphics", ""));
	de::MovePtr<tcu::TestCaseGroup> graphicsReads							(new tcu::TestCaseGroup(testCtx, "reads", ""));
	de::MovePtr<tcu::TestCaseGroup> graphicsReadsVertex						(new tcu::TestCaseGroup(testCtx, "vertex", ""));
	de::MovePtr<tcu::TestCaseGroup> graphicsReadsFragment					(new tcu::TestCaseGroup(testCtx, "fragment", ""));
	de::MovePtr<tcu::TestCaseGroup> graphicsWrites							(new tcu::TestCaseGroup(testCtx, "writes", ""));
	de::MovePtr<tcu::TestCaseGroup> graphicsWritesVertex					(new tcu::TestCaseGroup(testCtx, "vertex", ""));
	de::MovePtr<tcu::TestCaseGroup> graphicsWritesFragment					(new tcu::TestCaseGroup(testCtx, "fragment", ""));

	// A struct for describing formats
	struct Formats
	{
		const VkFormat		value;
		const char * const	name;
	};

	const Formats			bufferFormats[]			=
	{
		{ VK_FORMAT_R32_SINT,		"s32" },
		{ VK_FORMAT_R32_UINT,		"u32" },
		{ VK_FORMAT_R32_SFLOAT,		"f32" },
		{ VK_FORMAT_R64_SINT,		"s64" },
		{ VK_FORMAT_R64_UINT,		"u64" },
	};
	const deUint8			bufferFormatsCount		= static_cast<deUint8>(DE_LENGTH_OF_ARRAY(bufferFormats));

	// Amounts of data to copy
	const VkDeviceSize		rangeSizes[]			=
	{
		1ull, 3ull, 4ull, 16ull, 32ull
	};
	const deUint8			rangeSizesCount			= static_cast<deUint8>(DE_LENGTH_OF_ARRAY(rangeSizes));

	// gather above data into one array
	const struct ShaderTypes
	{
		const ShaderType			value;
		const char * const			name;
		const Formats* const		formats;
		const deUint8				formatsCount;
		const VkDeviceSize* const	sizes;
		const deUint8				sizesCount;
	}						types[]					=
	{
		{ SHADER_TYPE_VECTOR_COPY,	"vec4",		bufferFormats,			bufferFormatsCount,			rangeSizes,			rangeSizesCount },
		{ SHADER_TYPE_SCALAR_COPY,	"scalar",	bufferFormats,			bufferFormatsCount,			rangeSizes,			rangeSizesCount }
	};

	// Specify to which subgroups put various tests
	const struct ShaderStages
	{
		VkShaderStageFlags					stage;
		de::MovePtr<tcu::TestCaseGroup>&	reads;
		de::MovePtr<tcu::TestCaseGroup>&	writes;
	}						stages[]				=
	{
		{ VK_SHADER_STAGE_VERTEX_BIT,		graphicsReadsVertex,	graphicsWritesVertex },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,		graphicsReadsFragment,	graphicsWritesFragment },
		{ VK_SHADER_STAGE_COMPUTE_BIT,		computeReads,			computeWrites }
	};

	// Eventually specify if memory used should be in the "inaccesible" portion of buffer or entirely outside of buffer
	const char* const		backingMemory[]			= { "in_memory", "out_of_memory" };

	for (deInt32 stageId = 0; stageId < DE_LENGTH_OF_ARRAY(stages); ++stageId)
		for (int i = 0; i < DE_LENGTH_OF_ARRAY(types); ++i)
			for (int j = 0; j < types[i].formatsCount; ++j)
				for (int k = 0; k < types[i].sizesCount; ++k)
					for (int s = 0; s < DE_LENGTH_OF_ARRAY(backingMemory); ++s)
					{
						std::ostringstream	name;
						name << types[i].sizes[k] << "B_" << backingMemory[s] << "_with_" << types[i].name << '_' << types[i].formats[j].name;
						stages[stageId].reads->addChild(new RobustReadTest(testCtx, name.str().c_str(), "", stages[stageId].stage, types[i].value, types[i].formats[j].value, types[i].sizes[k], s != 0));
					}

	for (deInt32 stageId = 0; stageId < DE_LENGTH_OF_ARRAY(stages); ++stageId)
		for (int i=0; i<DE_LENGTH_OF_ARRAY(types); ++i)
			for (int j=0; j<types[i].formatsCount; ++j)
				for (int k = 0; k<types[i].sizesCount; ++k)
					for (int s = 0; s < DE_LENGTH_OF_ARRAY(backingMemory); ++s)
					{
						std::ostringstream	name;
						name << types[i].sizes[k] << "B_" << backingMemory[s] << "_with_" << types[i].name << '_' << types[i].formats[j].name;
						stages[stageId].writes->addChild(new RobustWriteTest(testCtx, name.str().c_str(), "", stages[stageId].stage, types[i].value, types[i].formats[j].value, types[i].sizes[k], s != 0));
					}

	graphicsReads->addChild(graphicsReadsVertex.release());
	graphicsReads->addChild(graphicsReadsFragment.release());

	graphicsWrites->addChild(graphicsWritesVertex.release());
	graphicsWrites->addChild(graphicsWritesFragment.release());

	graphicsWithVariablePointersTests->addChild(graphicsReads.release());
	graphicsWithVariablePointersTests->addChild(graphicsWrites.release());

	computeWithVariablePointersTests->addChild(computeReads.release());
	computeWithVariablePointersTests->addChild(computeWrites.release());

	bufferAccessWithVariablePointersTests->addChild(graphicsWithVariablePointersTests.release());
	bufferAccessWithVariablePointersTests->addChild(computeWithVariablePointersTests.release());

	return bufferAccessWithVariablePointersTests.release();
}

} // robustness
} // vkt
