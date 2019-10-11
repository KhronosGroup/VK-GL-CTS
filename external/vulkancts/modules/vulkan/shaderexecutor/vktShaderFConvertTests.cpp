/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Valve Corporation.
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief OpFConvert tests.
 *//*--------------------------------------------------------------------*/

#include "vktShaderFConvertTests.hpp"
#include "vktTestCase.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkPrograms.hpp"

#include "deDefs.hpp"
#include "deRandom.hpp"

#include "tcuFloat.hpp"
#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"

#include <vector>
#include <iterator>
#include <algorithm>
#include <memory>
#include <sstream>
#include <iomanip>
#include <string>
#include <limits>

namespace vkt
{
namespace shaderexecutor
{

namespace
{

constexpr deUint32	kRandomSeed								= 0xdeadbeef;
constexpr size_t	kRandomSourcesPerType					= 240;
constexpr size_t	kMinVectorLength						= 1;
constexpr size_t	kMaxVectorLength						= 4;
constexpr size_t	kArrayAlignment							= 16;					// Bytes.
constexpr size_t	kEffectiveLength[kMaxVectorLength + 1]	= { 0, 1, 2, 4, 4 };	// Effective length of a vector of size i.
constexpr size_t	kGCFNumFloats							= 12;					// Greatest Common Factor of the number of floats in a test.

// Get a random normal number.
// Works for implementations of tcu::Float as T.
template <class T>
T getRandomNormal (de::Random& rnd)
{
	static constexpr typename T::StorageType	kLeadingMantissaBit	= (static_cast<typename T::StorageType>(1) << T::MANTISSA_BITS);
	static constexpr int						kSignValues[]		= { -1, 1 };

	int						signBit		= rnd.getInt(0, 1);
	int						exponent	= rnd.getInt(1 - T::EXPONENT_BIAS, T::EXPONENT_BIAS + 1);
	typename T::StorageType	mantissa	= static_cast<typename T::StorageType>(rnd.getUint64() & static_cast<deUint64>(kLeadingMantissaBit - 1));

	// Construct number.
	return T::construct(kSignValues[signBit], exponent, (kLeadingMantissaBit | mantissa));
}

// Get a list of hand-picked interesting samples for tcu::Float class T.
template <class T>
const std::vector<T>& interestingSamples ()
{
	static const std::vector<T> samples =
	{
		T::zero				(-1),
		T::zero				( 1),
		//T::inf				(-1),
		//T::inf				( 1),
		//T::nan				(  ),
		T::largestNormal	(-1),
		T::largestNormal	( 1),
		T::smallestNormal	(-1),
		T::smallestNormal	( 1),
	};

	return samples;
}

// Get some random interesting numbers.
// Works for implementations of tcu::Float as T.
template <class T>
std::vector<T> getRandomInteresting (de::Random& rnd, size_t numSamples)
{
	auto&			samples = interestingSamples<T>();
	std::vector<T>	result;

	result.reserve(numSamples);
	std::generate_n(std::back_inserter(result), numSamples, [&rnd, &samples]() { return rnd.choose<T>(begin(samples), end(samples)); });

	return result;
}

// Helper class to build each vector only once in a thread-safe way.
template <class T>
struct StaticVectorHelper
{
	std::vector<T> v;

	StaticVectorHelper (de::Random& rnd)
	{
		v.reserve(kRandomSourcesPerType);
		for (size_t i = 0; i < kRandomSourcesPerType; ++i)
			v.push_back(getRandomNormal<T>(rnd));
	}
};

// Get a list of random normal input values for type T.
template <class T>
const std::vector<T>& getRandomNormals (de::Random& rnd)
{
	static StaticVectorHelper<T> helper(rnd);
	return helper.v;
}

// Convert a vector of tcu::Float elements of type T1 to type T2.
template <class T1, class T2>
std::vector<T2> convertVector (const std::vector<T1>& orig)
{
	std::vector<T2> result;
	result.reserve(orig.size());

	std::transform(begin(orig), end(orig), std::back_inserter(result),
		[](T1 f) { return T2::convert(f); });

	return result;
}

// Get converted normal values for other tcu::Float types smaller than T, which should be exact conversions when converting back to
// those types.
template <class T>
std::vector<T> getOtherNormals (de::Random& rnd);

template<>
std::vector<tcu::Float16> getOtherNormals<tcu::Float16> (de::Random&)
{
	// Nothing below tcu::Float16.
	return std::vector<tcu::Float16>();
}

template<>
std::vector<tcu::Float32> getOtherNormals<tcu::Float32> (de::Random& rnd)
{
	// The ones from tcu::Float16.
	return convertVector<tcu::Float16, tcu::Float32>(getRandomNormals<tcu::Float16>(rnd));
}

template<>
std::vector<tcu::Float64> getOtherNormals<tcu::Float64> (de::Random& rnd)
{
	// The ones from both tcu::Float16 and tcu::Float64.
	auto v1 = convertVector<tcu::Float16, tcu::Float64>(getRandomNormals<tcu::Float16>(rnd));
	auto v2 = convertVector<tcu::Float32, tcu::Float64>(getRandomNormals<tcu::Float32>(rnd));

	v1.reserve(v1.size() + v2.size());
	std::copy(begin(v2), end(v2), std::back_inserter(v1));
	return v1;
}

// Get the full list of input values for type T.
template <class T>
std::vector<T> getInputValues (de::Random& rnd)
{
	auto&	interesting		= interestingSamples<T>();
	auto&	normals			= getRandomNormals<T>(rnd);
	auto	otherNormals	= getOtherNormals<T>(rnd);

	const size_t numValues		= interesting.size() + normals.size() + otherNormals.size();
	const size_t extraValues	= numValues % kGCFNumFloats;
	const size_t needed			= ((extraValues == 0) ? 0 : (kGCFNumFloats - extraValues));

	auto extra = getRandomInteresting<T> (rnd, needed);

	std::vector<T> values;
	values.reserve(interesting.size() + normals.size() + otherNormals.size() + extra.size());

	std::copy(begin(interesting),	end(interesting),	std::back_inserter(values));
	std::copy(begin(normals),		end(normals),		std::back_inserter(values));
	std::copy(begin(otherNormals),	end(otherNormals),	std::back_inserter(values));
	std::copy(begin(extra),			end(extra),			std::back_inserter(values));

	// Shuffle samples around a bit to make it more interesting.
	rnd.shuffle(begin(values), end(values));

	return values;
}

// This singleton makes sure generated samples are stable no matter the test order.
class InputGenerator
{
public:
	static const InputGenerator& getInstance ()
	{
		static InputGenerator instance;
		return instance;
	}

	const std::vector<tcu::Float16>& getInputValues16 () const
	{
		return m_values16;
	}

	const std::vector<tcu::Float32>& getInputValues32 () const
	{
		return m_values32;
	}

	const std::vector<tcu::Float64>& getInputValues64 () const
	{
		return m_values64;
	}

private:
	InputGenerator ()
		: m_rnd(kRandomSeed)
		, m_values16(getInputValues<tcu::Float16>(m_rnd))
		, m_values32(getInputValues<tcu::Float32>(m_rnd))
		, m_values64(getInputValues<tcu::Float64>(m_rnd))
	{
	}

	// Cannot copy or assign.
	InputGenerator(const InputGenerator&)				= delete;
	InputGenerator& operator=(const InputGenerator&)	= delete;

	de::Random					m_rnd;
	std::vector<tcu::Float16>	m_values16;
	std::vector<tcu::Float32>	m_values32;
	std::vector<tcu::Float64>	m_values64;
};

// Check single result is as expected.
// Works for implementations of tcu::Float as T1 and T2.
template <class T1, class T2>
bool validConversion (const T1& orig, const T2& result)
{
	const T2	acceptedResults[]	= { T2::convert(orig, tcu::ROUND_DOWNWARD), T2::convert(orig, tcu::ROUND_UPWARD) };
	bool		valid				= false;

	for (const auto& validResult : acceptedResults)
	{
		if (validResult.isNaN() && result.isNaN())
			valid = true;
		else if (validResult.isInf() && result.isInf())
			valid = true;
		else if (validResult.isZero() && result.isZero())
			valid = true;
		else if (validResult.isDenorm() && (result.isDenorm() || result.isZero()))
			valid = true;
		else if (validResult.bits() == result.bits()) // Exact conversion, up or down.
			valid = true;
	}

	return valid;
}

// Check results vector is as expected.
template <class T1, class T2>
bool validConversion (const std::vector<T1>& orig, const std::vector<T2>& converted, tcu::TestLog& log)
{
	DE_ASSERT(orig.size() == converted.size());

	bool allValid = true;

	for (size_t i = 0; i < orig.size(); ++i)
	{
		const bool valid = validConversion(orig[i], converted[i]);

		{
			const double origD = orig[i].asDouble();
			const double convD = converted[i].asDouble();

			std::ostringstream msg;
			msg << "[" << i << "] "
				<< std::setprecision(std::numeric_limits<double>::digits10 + 2) << std::scientific
				<< origD << " converted to " << convD << ": " << (valid ? "OK" : "FAILURE");

			log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
		}

		if (!valid)
			allValid = false;
	}

	return allValid;
}

// Helps calculate buffer sizes and other parameters for the given number of values and vector length using a given floating point
// type. This is mostly used in packFloats() below, but we also need this information in the iterate() method for the test instance,
// so it has been separated.
struct BufferSizeInfo
{
	template <class T>
	static BufferSizeInfo calculate (size_t numValues_, size_t vectorLength_)
	{
		// The vector length must be a known number.
		DE_ASSERT(vectorLength_ >= kMinVectorLength && vectorLength_ <= kMaxVectorLength);
		// The number of values must be appropriate for the vector length.
		DE_ASSERT(numValues_ % vectorLength_ == 0);

		BufferSizeInfo info;

		info.numValues		= numValues_;
		info.vectorLength	= vectorLength_;
		info.totalVectors	= numValues_ / vectorLength_;

		const size_t elementSize		= sizeof(typename T::StorageType);
		const size_t effectiveLength	= kEffectiveLength[vectorLength_];
		const size_t vectorSize			= elementSize * effectiveLength;
		const size_t extraBytes			= vectorSize % kArrayAlignment;

		info.vectorStrideBytes	= vectorSize + ((extraBytes == 0) ? 0 : (kArrayAlignment - extraBytes));
		info.memorySizeBytes	= info.vectorStrideBytes * info.totalVectors;

		return info;
	}

	size_t numValues;
	size_t vectorLength;
	size_t totalVectors;
	size_t vectorStrideBytes;
	size_t memorySizeBytes;
};

// Pack an array of tcu::Float values into a buffer to be read from a shader, as if it was an array of vectors with each vector
// having size vectorLength (e.g. 3 for a vec3). Note: assumes std140.
template <class T>
std::vector<deUint8> packFloats (const std::vector<T>& values, size_t vectorLength)
{
	BufferSizeInfo sizeInfo = BufferSizeInfo::calculate<T>(values.size(), vectorLength);

	std::vector<deUint8> memory(sizeInfo.memorySizeBytes);
	for (size_t i = 0; i < sizeInfo.totalVectors; ++i)
	{
		T* vectorPtr = reinterpret_cast<T*>(memory.data() + sizeInfo.vectorStrideBytes * i);
		for (size_t j = 0; j < vectorLength; ++j)
			vectorPtr[j] = values[i*vectorLength + j];
	}

	return memory;
}

// Unpack an array of vectors into an array of values, undoing what packFloats would do.
// expectedNumValues is used for verification.
template <class T>
std::vector<T> unpackFloats (const std::vector<deUint8>& memory, size_t vectorLength, size_t expectedNumValues)
{
	DE_ASSERT(vectorLength >= kMinVectorLength && vectorLength <= kMaxVectorLength);

	const size_t effectiveLength	= kEffectiveLength[vectorLength];
	const size_t elementSize		= sizeof(typename T::StorageType);
	const size_t vectorSize			= elementSize * effectiveLength;
	const size_t extraBytes			= vectorSize % kArrayAlignment;
	const size_t vectorBlockSize	= vectorSize + ((extraBytes == 0) ? 0 : (kArrayAlignment - extraBytes));

	DE_ASSERT(memory.size() % vectorBlockSize == 0);
	const size_t numStoredVectors	= memory.size() / vectorBlockSize;
	const size_t numStoredValues	= numStoredVectors * vectorLength;

	DE_UNREF(expectedNumValues); // For release builds.
	DE_ASSERT(numStoredValues == expectedNumValues);
	std::vector<T> values;
	values.reserve(numStoredValues);

	for (size_t i = 0; i < numStoredVectors; ++i)
	{
		const T* vectorPtr = reinterpret_cast<const T*>(memory.data() + vectorBlockSize * i);
		for (size_t j = 0; j < vectorLength; ++j)
			values.push_back(vectorPtr[j]);
	}

	return values;
}

enum FloatType
{
	FLOAT_TYPE_16_BITS = 0,
	FLOAT_TYPE_32_BITS,
	FLOAT_TYPE_64_BITS,
	FLOAT_TYPE_MAX_ENUM,
};

static const char* const kFloatNames[FLOAT_TYPE_MAX_ENUM] =
{
	"f16",
	"f32",
	"f64",
};

static const char* const kGLSLTypes[][kMaxVectorLength + 1] =
{
	{ nullptr, "float16_t",	"f16vec2",	"f16vec3",	"f16vec4"	},
	{ nullptr, "float",		"vec2",		"vec3",		"vec4"		},
	{ nullptr, "double",	"dvec2",	"dvec3",	"dvec4"		},
};

struct TestParams
{
	FloatType	from;
	FloatType	to;
	size_t		vectorLength;

	std::string	getInputTypeStr		() const
	{
		DE_ASSERT(from >= 0 && from < FLOAT_TYPE_MAX_ENUM);
		DE_ASSERT(vectorLength >= kMinVectorLength && vectorLength <= kMaxVectorLength);
		return kGLSLTypes[from][vectorLength];
	}

	std::string getOutputTypeStr	() const
	{
		DE_ASSERT(to >= 0 && to < FLOAT_TYPE_MAX_ENUM);
		DE_ASSERT(vectorLength >= kMinVectorLength && vectorLength <= kMaxVectorLength);
		return kGLSLTypes[to][vectorLength];
	}
};

class FConvertTestInstance : public TestInstance
{
public:
							FConvertTestInstance	(Context& context, const TestParams& params)
								: TestInstance(context)
								, m_params(params)
								{}

	virtual tcu::TestStatus	iterate					(void);

private:
	TestParams	m_params;
};

class FConvertTestCase : public TestCase
{
public:
								FConvertTestCase	(tcu::TestContext& context, const std::string& name, const std::string& desc, const TestParams& params)
									: TestCase	(context, name, desc)
									, m_params	(params)
									{}

								~FConvertTestCase	(void) {}
	virtual TestInstance*		createInstance		(Context& context) const { return new FConvertTestInstance(context, m_params); }
	virtual	void				initPrograms		(vk::SourceCollections& programCollection) const;
	virtual void				checkSupport		(Context& context) const;

private:
	TestParams	m_params;
};

void FConvertTestCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const std::string		inputType		= m_params.getInputTypeStr();
	const std::string		outputType		= m_params.getOutputTypeStr();
	const InputGenerator&	inputGenerator	= InputGenerator::getInstance();

	size_t numValues = 0;
	switch (m_params.from)
	{
	case FLOAT_TYPE_16_BITS:
		numValues = inputGenerator.getInputValues16().size();
		break;
	case FLOAT_TYPE_32_BITS:
		numValues = inputGenerator.getInputValues32().size();
		break;
	case FLOAT_TYPE_64_BITS:
		numValues = inputGenerator.getInputValues64().size();
		break;
	default:
		DE_ASSERT(false);
		break;
	}

	const size_t arraySize = numValues / m_params.vectorLength;

	std::ostringstream shader;

	shader
		<< "#version 450 core\n"
		<< ((m_params.from == FLOAT_TYPE_16_BITS || m_params.to == FLOAT_TYPE_16_BITS) ?
			"#extension GL_EXT_shader_16bit_storage: require\n"					// This is needed to use 16-bit float types in buffers.
			"#extension GL_EXT_shader_explicit_arithmetic_types: require\n"		// This is needed for some conversions.
			: "")
		<< "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
		<< "layout(set = 0, binding = 0, std140) buffer issbodef { " << inputType << " val[" << arraySize << "]; } issbo;\n"
		<< "layout(set = 0, binding = 1, std140) buffer ossbodef { " << outputType << " val[" << arraySize << "]; } ossbo;\n"
		<< "void main()\n"
		<< "{\n"
		<< "	ossbo.val[gl_WorkGroupID.x] = " << outputType << "(issbo.val[gl_WorkGroupID.x]);\n"
		<< "}\n";

	programCollection.glslSources.add("comp") << glu::ComputeSource(shader.str());
}

void FConvertTestCase::checkSupport (Context& context) const
{
	if (m_params.from == FLOAT_TYPE_64_BITS || m_params.to == FLOAT_TYPE_64_BITS)
	{
		// Check for 64-bit float support.
		auto features = context.getDeviceFeatures();
		if (!features.shaderFloat64)
			TCU_THROW(NotSupportedError, "64-bit floats not supported in shader code");
	}

	if (m_params.from == FLOAT_TYPE_16_BITS || m_params.to == FLOAT_TYPE_16_BITS)
	{
		// Check for 16-bit float support.
		auto& features16 = context.getShaderFloat16Int8Features();
		if (!features16.shaderFloat16)
			TCU_THROW(NotSupportedError, "16-bit floats not supported in shader code");

		auto& storage16 = context.get16BitStorageFeatures();
		if (!storage16.storageBuffer16BitAccess)
			TCU_THROW(NotSupportedError, "16-bit floats not supported for storage buffers");
	}
}

tcu::TestStatus FConvertTestInstance::iterate (void)
{
	BufferSizeInfo			inputBufferSizeInfo;
	BufferSizeInfo			outputBufferSizeInfo;
	std::vector<deUint8>	inputMemory;

	// Calculate buffer sizes and convert input values to a packed input memory format, depending on the input and output types.
	switch (m_params.from)
	{
	case FLOAT_TYPE_16_BITS:
		{
			auto& inputValues = InputGenerator::getInstance().getInputValues16();
			inputBufferSizeInfo = BufferSizeInfo::calculate<tcu::Float16>(inputValues.size(), m_params.vectorLength);
			switch (m_params.to)
			{
			case FLOAT_TYPE_32_BITS:
				outputBufferSizeInfo = BufferSizeInfo::calculate<tcu::Float32>(inputValues.size(), m_params.vectorLength);
				break;
			case FLOAT_TYPE_64_BITS:
				outputBufferSizeInfo = BufferSizeInfo::calculate<tcu::Float64>(inputValues.size(), m_params.vectorLength);
				break;
			default:
				DE_ASSERT(false);
				break;
			}
			inputMemory = packFloats(inputValues, m_params.vectorLength);
		}
		break;

	case FLOAT_TYPE_32_BITS:
		{
			auto& inputValues = InputGenerator::getInstance().getInputValues32();
			inputBufferSizeInfo = BufferSizeInfo::calculate<tcu::Float32>(inputValues.size(), m_params.vectorLength);
			switch (m_params.to)
			{
			case FLOAT_TYPE_16_BITS:
				outputBufferSizeInfo = BufferSizeInfo::calculate<tcu::Float16>(inputValues.size(), m_params.vectorLength);
				break;
			case FLOAT_TYPE_64_BITS:
				outputBufferSizeInfo = BufferSizeInfo::calculate<tcu::Float64>(inputValues.size(), m_params.vectorLength);
				break;
			default:
				DE_ASSERT(false);
				break;
			}
			inputMemory = packFloats(inputValues, m_params.vectorLength);
		}
		break;

	case FLOAT_TYPE_64_BITS:
		{
			auto& inputValues = InputGenerator::getInstance().getInputValues64();
			inputBufferSizeInfo = BufferSizeInfo::calculate<tcu::Float64>(inputValues.size(), m_params.vectorLength);
			switch (m_params.to)
			{
			case FLOAT_TYPE_16_BITS:
				outputBufferSizeInfo = BufferSizeInfo::calculate<tcu::Float16>(inputValues.size(), m_params.vectorLength);
				break;
			case FLOAT_TYPE_32_BITS:
				outputBufferSizeInfo = BufferSizeInfo::calculate<tcu::Float32>(inputValues.size(), m_params.vectorLength);
				break;
			default:
				DE_ASSERT(false);
				break;
			}
			inputMemory = packFloats(inputValues, m_params.vectorLength);
		}
		break;

	default:
		DE_ASSERT(false);
		break;
	}

	// Prepare input and output buffers.
	auto&	vkd			= m_context.getDeviceInterface();
	auto	device		= m_context.getDevice();
	auto&	allocator	= m_context.getDefaultAllocator();

	de::MovePtr<vk::BufferWithMemory> inputBuffer(
		new vk::BufferWithMemory(vkd, device, allocator,
								 vk::makeBufferCreateInfo(inputBufferSizeInfo.memorySizeBytes, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
								 vk::MemoryRequirement::HostVisible)
	);

	de::MovePtr<vk::BufferWithMemory> outputBuffer(
		new vk::BufferWithMemory(vkd, device, allocator,
								 vk::makeBufferCreateInfo(outputBufferSizeInfo.memorySizeBytes, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
								 vk::MemoryRequirement::HostVisible)
	);

	// Copy values to input buffer.
	{
		auto& alloc = inputBuffer->getAllocation();
		deMemcpy(reinterpret_cast<deUint8*>(alloc.getHostPtr()) + alloc.getOffset(), inputMemory.data(), inputMemory.size());
		vk::flushAlloc(vkd, device, alloc);
	}

	// Create an array with the input and output buffers to make it easier to iterate below.
	const vk::VkBuffer buffers[] = { inputBuffer->get(), outputBuffer->get() };

	// Create descriptor set layout.
	std::vector<vk::VkDescriptorSetLayoutBinding> bindings;
	for (int i = 0; i < DE_LENGTH_OF_ARRAY(buffers); ++i)
	{
		const vk::VkDescriptorSetLayoutBinding binding =
		{
			static_cast<deUint32>(i),								// uint32_t              binding;
			vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,					// VkDescriptorType      descriptorType;
			1u,														// uint32_t              descriptorCount;
			vk::VK_SHADER_STAGE_COMPUTE_BIT,						// VkShaderStageFlags    stageFlags;
			DE_NULL,													// const VkSampler*      pImmutableSamplers;
		};
		bindings.push_back(binding);
	}

	const vk::VkDescriptorSetLayoutCreateInfo layoutCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType                        sType;
		DE_NULL,													// const void*                            pNext;
		0,															// VkDescriptorSetLayoutCreateFlags       flags;
		static_cast<deUint32>(bindings.size()),						// uint32_t                               bindingCount;
		bindings.data()												// const VkDescriptorSetLayoutBinding*    pBindings;
	};
	auto descriptorSetLayout = vk::createDescriptorSetLayout(vkd, device, &layoutCreateInfo);

	// Create descriptor set.
	vk::DescriptorPoolBuilder poolBuilder;
	for (const auto& b : bindings)
		poolBuilder.addType(b.descriptorType, 1u);
	auto descriptorPool = poolBuilder.build(vkd, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	const vk::VkDescriptorSetAllocateInfo allocateInfo =
	{
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// VkStructureType                 sType;
		DE_NULL,											// const void*                     pNext;
		*descriptorPool,									// VkDescriptorPool                descriptorPool;
		1u,													// uint32_t                        descriptorSetCount;
		&descriptorSetLayout.get()							// const VkDescriptorSetLayout*    pSetLayouts;
	};
	auto descriptorSet = vk::allocateDescriptorSet(vkd, device, &allocateInfo);

	// Update descriptor set.
	std::vector<vk::VkDescriptorBufferInfo>	descriptorBufferInfos;
	std::vector<vk::VkWriteDescriptorSet>	descriptorWrites;

	for (const auto& buffer : buffers)
	{
		const vk::VkDescriptorBufferInfo bufferInfo =
		{
			buffer,			// VkBuffer        buffer;
			0u,				// VkDeviceSize    offset;
			VK_WHOLE_SIZE,	// VkDeviceSize    range;
		};
		descriptorBufferInfos.push_back(bufferInfo);
	}

	for (size_t i = 0; i < bindings.size(); ++i)
	{
		const vk::VkWriteDescriptorSet write =
		{
			vk::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType                  sType;
			DE_NULL,									// const void*                      pNext;
			*descriptorSet,								// VkDescriptorSet                  dstSet;
			static_cast<deUint32>(i),					// uint32_t                         dstBinding;
			0u,											// uint32_t                         dstArrayElement;
			1u,											// uint32_t                         descriptorCount;
			bindings[i].descriptorType,					// VkDescriptorType                 descriptorType;
			DE_NULL,									// const VkDescriptorImageInfo*     pImageInfo;
			&descriptorBufferInfos[i],					// const VkDescriptorBufferInfo*    pBufferInfo;
			DE_NULL,									// const VkBufferView*              pTexelBufferView;
		};
		descriptorWrites.push_back(write);
	}
	vkd.updateDescriptorSets(device, static_cast<deUint32>(descriptorWrites.size()), descriptorWrites.data(), 0u, DE_NULL);

	// Prepare barriers in advance so data is visible to the shaders and the host.
	std::vector<vk::VkBufferMemoryBarrier> hostToDevBarriers;
	std::vector<vk::VkBufferMemoryBarrier> devToHostBarriers;
	for (int i = 0; i < DE_LENGTH_OF_ARRAY(buffers); ++i)
	{
		const vk::VkBufferMemoryBarrier hostToDev =
		{
			vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,						// VkStructureType	sType;
			DE_NULL,															// const void*		pNext;
			vk::VK_ACCESS_HOST_WRITE_BIT,										// VkAccessFlags	srcAccessMask;
			(vk::VK_ACCESS_SHADER_READ_BIT | vk::VK_ACCESS_SHADER_WRITE_BIT),	// VkAccessFlags	dstAccessMask;
			VK_QUEUE_FAMILY_IGNORED,											// deUint32			srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,											// deUint32			dstQueueFamilyIndex;
			buffers[i],															// VkBuffer			buffer;
			0u,																	// VkDeviceSize		offset;
			VK_WHOLE_SIZE,														// VkDeviceSize		size;
		};
		hostToDevBarriers.push_back(hostToDev);

		const vk::VkBufferMemoryBarrier devToHost =
		{
			vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,						// VkStructureType	sType;
			DE_NULL,															// const void*		pNext;
			vk::VK_ACCESS_SHADER_WRITE_BIT,										// VkAccessFlags	srcAccessMask;
			vk::VK_ACCESS_HOST_READ_BIT,										// VkAccessFlags	dstAccessMask;
			VK_QUEUE_FAMILY_IGNORED,											// deUint32			srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,											// deUint32			dstQueueFamilyIndex;
			buffers[i],															// VkBuffer			buffer;
			0u,																	// VkDeviceSize		offset;
			VK_WHOLE_SIZE,														// VkDeviceSize		size;
		};
		devToHostBarriers.push_back(devToHost);
	}

	// Create command pool and command buffer.
	auto queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

	const vk::VkCommandPoolCreateInfo cmdPoolCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,	// VkStructureType				sType;
		DE_NULL,										// const void*					pNext;
		vk::VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,		// VkCommandPoolCreateFlags		flags;
		queueFamilyIndex,								// deUint32						queueFamilyIndex;
	};
	auto cmdPool = vk::createCommandPool(vkd, device, &cmdPoolCreateInfo);

	const vk::VkCommandBufferAllocateInfo cmdBufferAllocateInfo =
	{
		vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
		DE_NULL,											// const void*				pNext;
		*cmdPool,											// VkCommandPool			commandPool;
		vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCommandBufferLevel		level;
		1u,													// deUint32					commandBufferCount;
	};
	auto cmdBuffer = vk::allocateCommandBuffer(vkd, device, &cmdBufferAllocateInfo);

	// Create pipeline layout.
	const vk::VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		0,													// VkPipelineLayoutCreateFlags		flags;
		1u,													// deUint32							setLayoutCount;
		&descriptorSetLayout.get(),							// const VkDescriptorSetLayout*		pSetLayouts;
		0u,													// deUint32							pushConstantRangeCount;
		DE_NULL,											// const VkPushConstantRange*		pPushConstantRanges;
	};
	auto pipelineLayout = vk::createPipelineLayout(vkd, device, &pipelineLayoutCreateInfo);

	// Create compute pipeline.
	const vk::Unique<vk::VkShaderModule> shader(vk::createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0));

	const vk::VkComputePipelineCreateInfo computeCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	// VkStructureType                    sType;
		DE_NULL,											// const void*                        pNext;
		0,													// VkPipelineCreateFlags              flags;
		{													// VkPipelineShaderStageCreateInfo    stage;
			vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType                     sType;
			DE_NULL,													// const void*                         pNext;
			0,															// VkPipelineShaderStageCreateFlags    flags;
			vk::VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits               stage;
			*shader,													// VkShaderModule                      module;
			"main",														// const char*                         pName;
			DE_NULL,													// const VkSpecializationInfo*         pSpecializationInfo;
		},
		*pipelineLayout,									// VkPipelineLayout                   layout;
		DE_NULL,											// VkPipeline                         basePipelineHandle;
		0,													// int32_t                            basePipelineIndex;
	};
	auto computePipeline = vk::createComputePipeline(vkd, device, DE_NULL, &computeCreateInfo);

	// Run the shader.
	vk::beginCommandBuffer(vkd, *cmdBuffer);
		vkd.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
		vkd.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1u, &descriptorSet.get(), 0u, DE_NULL);
		vkd.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0u, DE_NULL, static_cast<deUint32>(hostToDevBarriers.size()), hostToDevBarriers.data(), 0u, DE_NULL);
		vkd.cmdDispatch(*cmdBuffer, static_cast<deUint32>(inputBufferSizeInfo.totalVectors), 1u, 1u);
		vkd.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0, 0u, DE_NULL, static_cast<deUint32>(devToHostBarriers.size()), devToHostBarriers.data(), 0u, DE_NULL);
	vk::endCommandBuffer(vkd, *cmdBuffer);
	vk::submitCommandsAndWait(vkd, device, m_context.getUniversalQueue(), *cmdBuffer);

	// Invalidate output allocation.
	vk::invalidateAlloc(vkd, device, outputBuffer->getAllocation());

	// Copy output buffer data.
	std::vector<deUint8> outputMemory(outputBufferSizeInfo.memorySizeBytes);
	{
		auto& alloc = outputBuffer->getAllocation();
		deMemcpy(outputMemory.data(), reinterpret_cast<deUint8*>(alloc.getHostPtr()) + alloc.getOffset(), outputBufferSizeInfo.memorySizeBytes);
	}

	// Unpack and verify output data.
	auto& testLog = m_context.getTestContext().getLog();
	bool conversionOk = false;
	switch (m_params.to)
	{
	case FLOAT_TYPE_16_BITS:
		{
			auto outputValues = unpackFloats<tcu::Float16>(outputMemory, m_params.vectorLength, inputBufferSizeInfo.numValues);
			switch (m_params.from)
			{
			case FLOAT_TYPE_32_BITS:
				{
					auto& inputValues = InputGenerator::getInstance().getInputValues32();
					conversionOk = validConversion(inputValues, outputValues, testLog);
				}
				break;

			case FLOAT_TYPE_64_BITS:
				{
					auto& inputValues = InputGenerator::getInstance().getInputValues64();
					conversionOk = validConversion(inputValues, outputValues, testLog);
				}
				break;

			default:
				DE_ASSERT(false);
				break;
			}
		}
		break;

	case FLOAT_TYPE_32_BITS:
		{
			auto outputValues = unpackFloats<tcu::Float32>(outputMemory, m_params.vectorLength, inputBufferSizeInfo.numValues);
			switch (m_params.from)
			{
			case FLOAT_TYPE_16_BITS:
				{
					auto& inputValues = InputGenerator::getInstance().getInputValues16();
					conversionOk = validConversion(inputValues, outputValues, testLog);
				}
				break;

			case FLOAT_TYPE_64_BITS:
				{
					auto& inputValues = InputGenerator::getInstance().getInputValues64();
					conversionOk = validConversion(inputValues, outputValues, testLog);
				}
				break;

			default:
				DE_ASSERT(false);
				break;
			}
		}
		break;

	case FLOAT_TYPE_64_BITS:
		{
			auto outputValues = unpackFloats<tcu::Float64>(outputMemory, m_params.vectorLength, inputBufferSizeInfo.numValues);
			switch (m_params.from)
			{
			case FLOAT_TYPE_16_BITS:
				{
					auto& inputValues = InputGenerator::getInstance().getInputValues16();
					conversionOk = validConversion(inputValues, outputValues, testLog);
				}
				break;

			case FLOAT_TYPE_32_BITS:
				{
					auto& inputValues = InputGenerator::getInstance().getInputValues32();
					conversionOk = validConversion(inputValues, outputValues, testLog);
				}
				break;

			default:
				DE_ASSERT(false);
				break;
			}
		}
		break;

	default:
		DE_ASSERT(false);
		break;
	}

	return (conversionOk ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Fail"));
}

} // anonymous

tcu::TestCaseGroup*	createPrecisionFconvertGroup (tcu::TestContext& testCtx)
{
	tcu::TestCaseGroup* newGroup = new tcu::TestCaseGroup(testCtx, "precision_fconvert", "OpFConvert precision tests");

	for (int i = 0; i < FLOAT_TYPE_MAX_ENUM; ++i)
	for (int j = 0; j < FLOAT_TYPE_MAX_ENUM; ++j)
	for (size_t k = kMinVectorLength; k <= kMaxVectorLength; ++k)
	{
		// No actual conversion if the types are the same.
		if (i == j)
			continue;

		TestParams params = {
			static_cast<FloatType>(i),
			static_cast<FloatType>(j),
			k,
		};

		std::string testName = std::string() + kFloatNames[i] + "_to_" + kFloatNames[j] + "_size_" + std::to_string(k);
		std::string testDescription = std::string("Conversion from ") + kFloatNames[i] + " to " + kFloatNames[j] + " with vectors of size " + std::to_string(k);

		newGroup->addChild(new FConvertTestCase(testCtx, testName, testDescription, params));
	}

	return newGroup;
}

} // shaderexecutor
} // vkt
