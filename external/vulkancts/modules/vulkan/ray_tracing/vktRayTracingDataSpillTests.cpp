/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2020 Valve Corporation.
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
 * \brief Ray Tracing Data Spill tests
 *//*--------------------------------------------------------------------*/
#include "vktRayTracingDataSpillTests.hpp"
#include "vktTestCase.hpp"

#include "vkRayTracingUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuStringTemplate.hpp"
#include "tcuFloat.hpp"

#include "deUniquePtr.hpp"
#include "deSTLUtil.hpp"

#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <array>
#include <utility>

using namespace vk;

namespace vkt
{
namespace RayTracing
{

namespace
{

// The type of shader call that will be used.
enum class CallType
{
	TRACE_RAY = 0,
	EXECUTE_CALLABLE,
	REPORT_INTERSECTION,
};

// The type of data that will be checked.
enum class DataType
{
	// These can be made an array or vector.
	INT32 = 0,
	UINT32,
	INT64,
	UINT64,
	INT16,
	UINT16,
	INT8,
	UINT8,
	FLOAT32,
	FLOAT64,
	FLOAT16,

	// These are standalone, so the vector type should be scalar.
	STRUCT,
	IMAGE,
	SAMPLER,
	SAMPLED_IMAGE,
	PTR_IMAGE,
	PTR_SAMPLER,
	PTR_SAMPLED_IMAGE,
	PTR_TEXEL,
	OP_NULL,
	OP_UNDEF,
};

// The type of vector in use.
enum class VectorType
{
	SCALAR	= 1,
	V2		= 2,
	V3		= 3,
	V4		= 4,
	A5		= 5,
};

struct InputStruct
{
	deUint32	uintPart;
	float		floatPart;
};

constexpr auto			kImageFormat		= VK_FORMAT_R32_UINT;
const auto				kImageExtent		= makeExtent3D(1u, 1u, 1u);

// For samplers.
const VkImageUsageFlags	kSampledImageUsage	= (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
constexpr size_t		kNumImages			= 4u;
constexpr size_t		kNumSamplers		= 4u;
constexpr size_t		kNumCombined		= 2u;
constexpr size_t		kNumAloneImages		= kNumImages - kNumCombined;
constexpr size_t		kNumAloneSamplers	= kNumSamplers - kNumCombined;

// For storage images.
const VkImageUsageFlags	kStorageImageUsage	= (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT);

// For the pipeline interface tests.
constexpr size_t		kNumStorageValues	= 6u;
constexpr deUint32		kShaderRecordSize	= sizeof(tcu::UVec4);

// Get the effective vector length in memory.
size_t getEffectiveVectorLength (VectorType vectorType)
{
	return ((vectorType == VectorType::V3) ? static_cast<size_t>(4) : static_cast<size_t>(vectorType));
}

// Get the corresponding element size.
VkDeviceSize getElementSize(DataType dataType, VectorType vectorType)
{
	const size_t	length		= getEffectiveVectorLength(vectorType);
	size_t			dataSize	= 0u;

	switch (dataType)
	{
	case DataType::INT32:			dataSize = sizeof(deInt32);			break;
	case DataType::UINT32:			dataSize = sizeof(deUint32);		break;
	case DataType::INT64:			dataSize = sizeof(deInt64);			break;
	case DataType::UINT64:			dataSize = sizeof(deUint64);		break;
	case DataType::INT16:			dataSize = sizeof(deInt16);			break;
	case DataType::UINT16:			dataSize = sizeof(deUint16);		break;
	case DataType::INT8:			dataSize = sizeof(deInt8);			break;
	case DataType::UINT8:			dataSize = sizeof(deUint8);			break;
	case DataType::FLOAT32:			dataSize = sizeof(tcu::Float32);	break;
	case DataType::FLOAT64:			dataSize = sizeof(tcu::Float64);	break;
	case DataType::FLOAT16:			dataSize = sizeof(tcu::Float16);	break;
	case DataType::STRUCT:			dataSize = sizeof(InputStruct);		break;
	case DataType::IMAGE:				// fallthrough.
	case DataType::SAMPLER:				// fallthrough.
	case DataType::SAMPLED_IMAGE:		// fallthrough.
	case DataType::PTR_IMAGE:			// fallthrough.
	case DataType::PTR_SAMPLER:			// fallthrough.
	case DataType::PTR_SAMPLED_IMAGE:	// fallthrough.
									dataSize = sizeof(tcu::Float32);	break;
	case DataType::PTR_TEXEL:		dataSize = sizeof(deInt32);			break;
	case DataType::OP_NULL:				// fallthrough.
	case DataType::OP_UNDEF:			// fallthrough.
									dataSize = sizeof(deUint32);		break;
	default: DE_ASSERT(false); break;
	}

	return static_cast<VkDeviceSize>(dataSize * length);
}

// Proper stage for generating default geometry.
VkShaderStageFlagBits getShaderStageForGeometry (CallType type_)
{
	VkShaderStageFlagBits bits = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;

	switch (type_)
	{
	case CallType::TRACE_RAY:			bits = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;		break;
	case CallType::EXECUTE_CALLABLE:	bits = VK_SHADER_STAGE_CALLABLE_BIT_KHR;		break;
	case CallType::REPORT_INTERSECTION:	bits = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;	break;
	default: DE_ASSERT(false); break;
	}

	DE_ASSERT(bits != VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM);
	return bits;
}

VkShaderStageFlags getShaderStages (CallType type_)
{
	VkShaderStageFlags flags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	switch (type_)
	{
	case CallType::EXECUTE_CALLABLE:
		flags |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;
		break;
	case CallType::TRACE_RAY:
		flags |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		break;
	case CallType::REPORT_INTERSECTION:
		flags |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
		flags |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
		break;
	default:
		DE_ASSERT(false);
		break;
	}

	return flags;
}

// Some test types need additional descriptors with samplers, images and combined image samplers.
bool samplersNeeded (DataType dataType)
{
	bool needed = false;

	switch (dataType)
	{
	case DataType::IMAGE:
	case DataType::SAMPLER:
	case DataType::SAMPLED_IMAGE:
	case DataType::PTR_IMAGE:
	case DataType::PTR_SAMPLER:
	case DataType::PTR_SAMPLED_IMAGE:
		needed = true;
		break;
	default:
		break;
	}

	return needed;
}

// Some test types need an additional descriptor with a storage image.
bool storageImageNeeded (DataType dataType)
{
	return (dataType == DataType::PTR_TEXEL);
}

// Returns two strings:
//		.first is an optional GLSL additional type declaration (for structs, basically).
//		.second is the value declaration inside the input block.
std::pair<std::string, std::string> getGLSLInputValDecl (DataType dataType, VectorType vectorType)
{
	using TypePair	= std::pair<DataType, VectorType>;
	using TypeMap	= std::map<TypePair, std::string>;

	const std::string	varName		= "val";
	const auto			dataTypeIdx	= static_cast<int>(dataType);

	if (dataTypeIdx >= static_cast<int>(DataType::INT32) && dataTypeIdx <= static_cast<int>(DataType::FLOAT16))
	{
		// Note: A5 uses the same type as the scalar version. The array suffix will be added below.
		const TypeMap map =
		{
			std::make_pair(std::make_pair(DataType::INT32,		VectorType::SCALAR),	"int32_t"),
			std::make_pair(std::make_pair(DataType::INT32,		VectorType::V2),		"i32vec2"),
			std::make_pair(std::make_pair(DataType::INT32,		VectorType::V3),		"i32vec3"),
			std::make_pair(std::make_pair(DataType::INT32,		VectorType::V4),		"i32vec4"),
			std::make_pair(std::make_pair(DataType::INT32,		VectorType::A5),		"int32_t"),
			std::make_pair(std::make_pair(DataType::UINT32,		VectorType::SCALAR),	"uint32_t"),
			std::make_pair(std::make_pair(DataType::UINT32,		VectorType::V2),		"u32vec2"),
			std::make_pair(std::make_pair(DataType::UINT32,		VectorType::V3),		"u32vec3"),
			std::make_pair(std::make_pair(DataType::UINT32,		VectorType::V4),		"u32vec4"),
			std::make_pair(std::make_pair(DataType::UINT32,		VectorType::A5),		"uint32_t"),
			std::make_pair(std::make_pair(DataType::INT64,		VectorType::SCALAR),	"int64_t"),
			std::make_pair(std::make_pair(DataType::INT64,		VectorType::V2),		"i64vec2"),
			std::make_pair(std::make_pair(DataType::INT64,		VectorType::V3),		"i64vec3"),
			std::make_pair(std::make_pair(DataType::INT64,		VectorType::V4),		"i64vec4"),
			std::make_pair(std::make_pair(DataType::INT64,		VectorType::A5),		"int64_t"),
			std::make_pair(std::make_pair(DataType::UINT64,		VectorType::SCALAR),	"uint64_t"),
			std::make_pair(std::make_pair(DataType::UINT64,		VectorType::V2),		"u64vec2"),
			std::make_pair(std::make_pair(DataType::UINT64,		VectorType::V3),		"u64vec3"),
			std::make_pair(std::make_pair(DataType::UINT64,		VectorType::V4),		"u64vec4"),
			std::make_pair(std::make_pair(DataType::UINT64,		VectorType::A5),		"uint64_t"),
			std::make_pair(std::make_pair(DataType::INT16,		VectorType::SCALAR),	"int16_t"),
			std::make_pair(std::make_pair(DataType::INT16,		VectorType::V2),		"i16vec2"),
			std::make_pair(std::make_pair(DataType::INT16,		VectorType::V3),		"i16vec3"),
			std::make_pair(std::make_pair(DataType::INT16,		VectorType::V4),		"i16vec4"),
			std::make_pair(std::make_pair(DataType::INT16,		VectorType::A5),		"int16_t"),
			std::make_pair(std::make_pair(DataType::UINT16,		VectorType::SCALAR),	"uint16_t"),
			std::make_pair(std::make_pair(DataType::UINT16,		VectorType::V2),		"u16vec2"),
			std::make_pair(std::make_pair(DataType::UINT16,		VectorType::V3),		"u16vec3"),
			std::make_pair(std::make_pair(DataType::UINT16,		VectorType::V4),		"u16vec4"),
			std::make_pair(std::make_pair(DataType::UINT16,		VectorType::A5),		"uint16_t"),
			std::make_pair(std::make_pair(DataType::INT8,		VectorType::SCALAR),	"int8_t"),
			std::make_pair(std::make_pair(DataType::INT8,		VectorType::V2),		"i8vec2"),
			std::make_pair(std::make_pair(DataType::INT8,		VectorType::V3),		"i8vec3"),
			std::make_pair(std::make_pair(DataType::INT8,		VectorType::V4),		"i8vec4"),
			std::make_pair(std::make_pair(DataType::INT8,		VectorType::A5),		"int8_t"),
			std::make_pair(std::make_pair(DataType::UINT8,		VectorType::SCALAR),	"uint8_t"),
			std::make_pair(std::make_pair(DataType::UINT8,		VectorType::V2),		"u8vec2"),
			std::make_pair(std::make_pair(DataType::UINT8,		VectorType::V3),		"u8vec3"),
			std::make_pair(std::make_pair(DataType::UINT8,		VectorType::V4),		"u8vec4"),
			std::make_pair(std::make_pair(DataType::UINT8,		VectorType::A5),		"uint8_t"),
			std::make_pair(std::make_pair(DataType::FLOAT32,	VectorType::SCALAR),	"float32_t"),
			std::make_pair(std::make_pair(DataType::FLOAT32,	VectorType::V2),		"f32vec2"),
			std::make_pair(std::make_pair(DataType::FLOAT32,	VectorType::V3),		"f32vec3"),
			std::make_pair(std::make_pair(DataType::FLOAT32,	VectorType::V4),		"f32vec4"),
			std::make_pair(std::make_pair(DataType::FLOAT32,	VectorType::A5),		"float32_t"),
			std::make_pair(std::make_pair(DataType::FLOAT64,	VectorType::SCALAR),	"float64_t"),
			std::make_pair(std::make_pair(DataType::FLOAT64,	VectorType::V2),		"f64vec2"),
			std::make_pair(std::make_pair(DataType::FLOAT64,	VectorType::V3),		"f64vec3"),
			std::make_pair(std::make_pair(DataType::FLOAT64,	VectorType::V4),		"f64vec4"),
			std::make_pair(std::make_pair(DataType::FLOAT64,	VectorType::A5),		"float64_t"),
			std::make_pair(std::make_pair(DataType::FLOAT16,	VectorType::SCALAR),	"float16_t"),
			std::make_pair(std::make_pair(DataType::FLOAT16,	VectorType::V2),		"f16vec2"),
			std::make_pair(std::make_pair(DataType::FLOAT16,	VectorType::V3),		"f16vec3"),
			std::make_pair(std::make_pair(DataType::FLOAT16,	VectorType::V4),		"f16vec4"),
			std::make_pair(std::make_pair(DataType::FLOAT16,	VectorType::A5),		"float16_t"),
		};

		const auto key		= std::make_pair(dataType, vectorType);
		const auto found	= map.find(key);

		DE_ASSERT(found != end(map));

		const auto baseType		= found->second;
		const std::string decl	= baseType + " " + varName + ((vectorType == VectorType::A5) ? "[5]" : "") + ";";

		return std::make_pair(std::string(), decl);
	}
	else if (dataType == DataType::STRUCT)
	{
		return std::make_pair(std::string("struct InputStruct { uint val1; float val2; };\n"), std::string("InputStruct val;"));
	}
	else if (samplersNeeded(dataType))
	{
		return std::make_pair(std::string(), std::string("float val;"));
	}
	else if (storageImageNeeded(dataType))
	{
		return std::make_pair(std::string(), std::string("int val;"));
	}
	else if (dataType == DataType::OP_NULL || dataType == DataType::OP_UNDEF)
	{
		return std::make_pair(std::string(), std::string("uint val;"));
	}

	// Unreachable.
	DE_ASSERT(false);
	return std::make_pair(std::string(), std::string());
}

class DataSpillTestCase : public vkt::TestCase
{
public:
	struct TestParams
	{
		CallType	callType;
		DataType	dataType;
		VectorType	vectorType;
	};

							DataSpillTestCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& testParams);
	virtual					~DataSpillTestCase		(void) {}

	virtual void			initPrograms			(vk::SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance			(Context& context) const;
	virtual void			checkSupport			(Context& context) const;

private:
	TestParams				m_params;
};

class DataSpillTestInstance : public vkt::TestInstance
{
public:
	using TestParams = DataSpillTestCase::TestParams;

								DataSpillTestInstance	(Context& context, const TestParams& testParams);
	virtual						~DataSpillTestInstance	(void) {}

	virtual tcu::TestStatus		iterate					(void);

private:
	TestParams					m_params;
};


DataSpillTestCase::DataSpillTestCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& testParams)
	: vkt::TestCase	(testCtx, name, description)
	, m_params		(testParams)
{
	switch (m_params.dataType)
	{
	case DataType::STRUCT:
	case DataType::IMAGE:
	case DataType::SAMPLER:
	case DataType::SAMPLED_IMAGE:
	case DataType::PTR_IMAGE:
	case DataType::PTR_SAMPLER:
	case DataType::PTR_SAMPLED_IMAGE:
	case DataType::PTR_TEXEL:
	case DataType::OP_NULL:
	case DataType::OP_UNDEF:
		DE_ASSERT(m_params.vectorType == VectorType::SCALAR);
		break;
	default:
		break;
	}

	// The code assumes at most one of these is needed.
	DE_ASSERT(!(samplersNeeded(m_params.dataType) && storageImageNeeded(m_params.dataType)));
}

TestInstance* DataSpillTestCase::createInstance (Context& context) const
{
	return new DataSpillTestInstance(context, m_params);
}

DataSpillTestInstance::DataSpillTestInstance (Context& context, const TestParams& testParams)
	: vkt::TestInstance	(context)
	, m_params			(testParams)
{
}

// General checks for all tests.
void commonCheckSupport (Context& context)
{
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

	const auto& rtFeatures = context.getRayTracingPipelineFeatures();
	if (!rtFeatures.rayTracingPipeline)
		TCU_THROW(NotSupportedError, "Ray Tracing pipelines not supported");

	const auto& asFeatures = context.getAccelerationStructureFeatures();
	if (!asFeatures.accelerationStructure)
		TCU_FAIL("VK_KHR_acceleration_structure supported without accelerationStructure support");

}

void DataSpillTestCase::checkSupport (Context& context) const
{
	// General checks first.
	commonCheckSupport(context);

	const auto& features			= context.getDeviceFeatures();
	const auto& featuresStorage16	= context.get16BitStorageFeatures();
	const auto& featuresF16I8		= context.getShaderFloat16Int8Features();
	const auto& featuresStorage8	= context.get8BitStorageFeatures();

	if (m_params.dataType == DataType::INT64 || m_params.dataType == DataType::UINT64)
	{
		if (!features.shaderInt64)
			TCU_THROW(NotSupportedError, "64-bit integers not supported");
	}
	else if (m_params.dataType == DataType::INT16 || m_params.dataType == DataType::UINT16)
	{
		context.requireDeviceFunctionality("VK_KHR_16bit_storage");

		if (!features.shaderInt16)
			TCU_THROW(NotSupportedError, "16-bit integers not supported");

		if (!featuresStorage16.storageBuffer16BitAccess)
			TCU_THROW(NotSupportedError, "16-bit storage buffer access not supported");
	}
	else if (m_params.dataType == DataType::INT8 || m_params.dataType == DataType::UINT8)
	{
		context.requireDeviceFunctionality("VK_KHR_shader_float16_int8");
		context.requireDeviceFunctionality("VK_KHR_8bit_storage");

		if (!featuresF16I8.shaderInt8)
			TCU_THROW(NotSupportedError, "8-bit integers not supported");

		if (!featuresStorage8.storageBuffer8BitAccess)
			TCU_THROW(NotSupportedError, "8-bit storage buffer access not supported");
	}
	else if (m_params.dataType == DataType::FLOAT64)
	{
		if (!features.shaderFloat64)
			TCU_THROW(NotSupportedError, "64-bit floats not supported");
	}
	else if (m_params.dataType == DataType::FLOAT16)
	{
		context.requireDeviceFunctionality("VK_KHR_shader_float16_int8");
		context.requireDeviceFunctionality("VK_KHR_16bit_storage");

		if (!featuresF16I8.shaderFloat16)
			TCU_THROW(NotSupportedError, "16-bit floats not supported");

		if (!featuresStorage16.storageBuffer16BitAccess)
			TCU_THROW(NotSupportedError, "16-bit storage buffer access not supported");
	}
	else if (samplersNeeded(m_params.dataType))
	{
		context.requireDeviceFunctionality("VK_EXT_descriptor_indexing");
		const auto indexingFeatures = context.getDescriptorIndexingFeatures();
		if (!indexingFeatures.shaderSampledImageArrayNonUniformIndexing)
			TCU_THROW(NotSupportedError, "No support for non-uniform sampled image arrays");
	}
}

void DataSpillTestCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	const vk::SpirVAsmBuildOptions	spvBuildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, true);

	std::ostringstream spvTemplateStream;

	// This SPIR-V template will be used to generate shaders for different
	// stages (raygen, callable, etc). The basic mechanism uses 3 SSBOs: one
	// used strictly as an input, one to write the check result, and one to
	// verify the shader call has taken place. The latter two SSBOs contain just
	// a single uint, but the input SSBO typically contains other type of data
	// that will be filled from the test instance with predetermined values. The
	// shader will expect this data to have specific values that can be combined
	// some way to give an expected result (e.g. by adding the 4 components if
	// it's a vec4). This result will be used in the shader call to make sure
	// input values are read *before* the call. After the shader call has taken
	// place, the shader will attempt to read the input buffer again and verify
	// the value is still correct and matches the previous one. If the result
	// matches, it will write a confirmation value in the check buffer. In the
	// mean time, the callee will write a confirmation value in the callee
	// buffer to verify the shader call took place.
	//
	// Some test variants use samplers, images or sampled images. These need
	// additional bindings of different types and the interesting value is
	// typically placed in the image instead of the input buffer, while the
	// input buffer is used for sampling coordinates instead.
	//
	// Some important SPIR-V template variables:
	//
	// - INPUT_BUFFER_VALUE_TYPE will contain the type of input buffer data.
	// - CALC_ZERO_FOR_CALLABLE is expected to contain instructions that will
	//   calculate a value of zero to be used in the shader call instruction.
	//   This value should be derived from the input data.
	// - CALL_STATEMENTS will contain the shader call instructions.
	// - CALC_EQUAL_STATEMENT is expected to contain instructions that will
	//   set %equal to true as a %bool if the before- and after- data match.
	//
	// - %input_val_ptr contains the pointer to the input value.
	// - %input_val_before contains the value read before the call.
	// - %input_val_after contains the value read after the call.

	spvTemplateStream
		<< "                                  OpCapability RayTracingKHR\n"
		<< "${EXTRA_CAPABILITIES}"
		<< "                                  OpExtension \"SPV_KHR_ray_tracing\"\n"
		<< "${EXTRA_EXTENSIONS}"
		<< "                                  OpMemoryModel Logical GLSL450\n"
		<< "                                  OpEntryPoint ${ENTRY_POINT} %main \"main\" %topLevelAS %calleeBuffer %outputBuffer %inputBuffer${MAIN_INTERFACE_EXTRAS}\n"
		<< "${INTERFACE_DECORATIONS}"
		<< "                                  OpMemberDecorate %InputBlock 0 Offset 0\n"
		<< "                                  OpDecorate %InputBlock Block\n"
		<< "                                  OpDecorate %inputBuffer DescriptorSet 0\n"
		<< "                                  OpDecorate %inputBuffer Binding 3\n"
		<< "                                  OpMemberDecorate %OutputBlock 0 Offset 0\n"
		<< "                                  OpDecorate %OutputBlock Block\n"
		<< "                                  OpDecorate %outputBuffer DescriptorSet 0\n"
		<< "                                  OpDecorate %outputBuffer Binding 2\n"
		<< "                                  OpMemberDecorate %CalleeBlock 0 Offset 0\n"
		<< "                                  OpDecorate %CalleeBlock Block\n"
		<< "                                  OpDecorate %calleeBuffer DescriptorSet 0\n"
		<< "                                  OpDecorate %calleeBuffer Binding 1\n"
		<< "                                  OpDecorate %topLevelAS DescriptorSet 0\n"
		<< "                                  OpDecorate %topLevelAS Binding 0\n"
		<< "${EXTRA_BINDINGS}"
		<< "                          %void = OpTypeVoid\n"
		<< "                     %void_func = OpTypeFunction %void\n"
		<< "                           %int = OpTypeInt 32 1\n"
		<< "                          %uint = OpTypeInt 32 0\n"
		<< "                         %int_0 = OpConstant %int 0\n"
		<< "                        %uint_0 = OpConstant %uint 0\n"
		<< "                        %uint_1 = OpConstant %uint 1\n"
		<< "                        %uint_2 = OpConstant %uint 2\n"
		<< "                        %uint_3 = OpConstant %uint 3\n"
		<< "                        %uint_4 = OpConstant %uint 4\n"
		<< "                        %uint_5 = OpConstant %uint 5\n"
		<< "                      %uint_255 = OpConstant %uint 255\n"
		<< "                          %bool = OpTypeBool\n"
		<< "                         %float = OpTypeFloat 32\n"
		<< "                       %float_0 = OpConstant %float 0\n"
		<< "                       %float_1 = OpConstant %float 1\n"
		<< "                       %float_9 = OpConstant %float 9\n"
		<< "                     %float_0_5 = OpConstant %float 0.5\n"
		<< "                      %float_n1 = OpConstant %float -1\n"
		<< "                       %v3float = OpTypeVector %float 3\n"
		<< "                  %origin_const = OpConstantComposite %v3float %float_0_5 %float_0_5 %float_0\n"
		<< "               %direction_const = OpConstantComposite %v3float %float_0 %float_0 %float_n1\n"
		<< "${EXTRA_TYPES_AND_CONSTANTS}"
		<< "                 %data_func_ptr = OpTypePointer Function ${INPUT_BUFFER_VALUE_TYPE}\n"
		<< "${INTERFACE_TYPES_AND_VARIABLES}"
		<< "                    %InputBlock = OpTypeStruct ${INPUT_BUFFER_VALUE_TYPE}\n"
		<< " %_ptr_StorageBuffer_InputBlock = OpTypePointer StorageBuffer %InputBlock\n"
		<< "                   %inputBuffer = OpVariable %_ptr_StorageBuffer_InputBlock StorageBuffer\n"
		<< "        %data_storagebuffer_ptr = OpTypePointer StorageBuffer ${INPUT_BUFFER_VALUE_TYPE}\n"
		<< "                   %OutputBlock = OpTypeStruct %uint\n"
		<< "%_ptr_StorageBuffer_OutputBlock = OpTypePointer StorageBuffer %OutputBlock\n"
		<< "                  %outputBuffer = OpVariable %_ptr_StorageBuffer_OutputBlock StorageBuffer\n"
		<< "       %_ptr_StorageBuffer_uint = OpTypePointer StorageBuffer %uint\n"
		<< "                   %CalleeBlock = OpTypeStruct %uint\n"
		<< "%_ptr_StorageBuffer_CalleeBlock = OpTypePointer StorageBuffer %CalleeBlock\n"
		<< "                  %calleeBuffer = OpVariable %_ptr_StorageBuffer_CalleeBlock StorageBuffer\n"
		<< "                       %as_type = OpTypeAccelerationStructureKHR\n"
		<< "        %as_uniformconstant_ptr = OpTypePointer UniformConstant %as_type\n"
		<< "                    %topLevelAS = OpVariable %as_uniformconstant_ptr UniformConstant\n"
		<< "${EXTRA_BINDING_VARIABLES}"
		<< "                          %main = OpFunction %void None %void_func\n"
		<< "                    %main_label = OpLabel\n"
		<< "${EXTRA_FUNCTION_VARIABLES}"
		<< "                 %input_val_ptr = OpAccessChain %data_storagebuffer_ptr %inputBuffer %int_0\n"
		<< "                %output_val_ptr = OpAccessChain %_ptr_StorageBuffer_uint %outputBuffer %int_0\n"
		// Note we use Volatile to load the input buffer value before and after the call statements.
		<< "              %input_val_before = OpLoad ${INPUT_BUFFER_VALUE_TYPE} %input_val_ptr Volatile\n"
		<< "${CALC_ZERO_FOR_CALLABLE}"
		<< "${CALL_STATEMENTS}"
		<< "               %input_val_after = OpLoad ${INPUT_BUFFER_VALUE_TYPE} %input_val_ptr Volatile\n"
		<< "${CALC_EQUAL_STATEMENT}"
		<< "                    %output_val = OpSelect %uint %equal %uint_1 %uint_0\n"
		<< "                                  OpStore %output_val_ptr %output_val\n"
		<< "                                  OpReturn\n"
		<< "                                  OpFunctionEnd\n"
		;

	const tcu::StringTemplate spvTemplate (spvTemplateStream.str());

	std::map<std::string, std::string>	subs;
	std::string							componentTypeName;
	std::string							opEqual;
	const int							numComponents		= static_cast<int>(m_params.vectorType);
	const auto							isArray				= (numComponents > static_cast<int>(VectorType::V4));
	const auto							numComponentsStr	= de::toString(numComponents);

	subs["EXTRA_CAPABILITIES"]			= "";
	subs["EXTRA_EXTENSIONS"]			= "";
	subs["EXTRA_TYPES_AND_CONSTANTS"]	= "";
	subs["EXTRA_FUNCTION_VARIABLES"]	= "";
	subs["EXTRA_BINDINGS"]				= "";
	subs["EXTRA_BINDING_VARIABLES"]		= "";
	subs["EXTRA_FUNCTIONS"]				= "";

	// Take into account some of these substitutions will be updated after the if-block.

	if (m_params.dataType == DataType::INT32)
	{
		componentTypeName = "int";

		subs["INPUT_BUFFER_VALUE_TYPE"]		=	"%int";
		subs["EXTRA_TYPES_AND_CONSTANTS"]	+=	"                        %int_37 = OpConstant %int 37\n";
		subs["CALC_ZERO_FOR_CALLABLE"]		=	"                      %zero_int = OpISub %int %input_val_before %int_37\n"
												"             %zero_for_callable = OpBitcast %uint %zero_int\n";
	}
	else if (m_params.dataType == DataType::UINT32)
	{
		componentTypeName = "uint";

		subs["INPUT_BUFFER_VALUE_TYPE"]		=	"%uint";
		subs["EXTRA_TYPES_AND_CONSTANTS"]	+=	"                       %uint_37 = OpConstant %uint 37\n";
		subs["CALC_ZERO_FOR_CALLABLE"]		=	"             %zero_for_callable = OpISub %uint %input_val_before %uint_37\n";
	}
	else if (m_params.dataType == DataType::INT64)
	{
		componentTypeName = "long";

		subs["EXTRA_CAPABILITIES"]			+=	"                                  OpCapability Int64\n";
		subs["INPUT_BUFFER_VALUE_TYPE"]		=	"%long";
		subs["EXTRA_TYPES_AND_CONSTANTS"]	+=	"                          %long = OpTypeInt 64 1\n"
												"                       %long_37 = OpConstant %long 37\n";
		subs["CALC_ZERO_FOR_CALLABLE"]		=	"                     %zero_long = OpISub %long %input_val_before %long_37\n"
												"             %zero_for_callable = OpSConvert %uint %zero_long\n";
	}
	else if (m_params.dataType == DataType::UINT64)
	{
		componentTypeName = "ulong";

		subs["EXTRA_CAPABILITIES"]			+=	"                                  OpCapability Int64\n";
		subs["INPUT_BUFFER_VALUE_TYPE"]		=	"%ulong";
		subs["EXTRA_TYPES_AND_CONSTANTS"]	+=	"                         %ulong = OpTypeInt 64 0\n"
												"                      %ulong_37 = OpConstant %ulong 37\n";
		subs["CALC_ZERO_FOR_CALLABLE"]		=	"                    %zero_ulong = OpISub %ulong %input_val_before %ulong_37\n"
												"             %zero_for_callable = OpUConvert %uint %zero_ulong\n";
	}
	else if (m_params.dataType == DataType::INT16)
	{
		componentTypeName = "short";

		subs["EXTRA_CAPABILITIES"]			+=	"                                  OpCapability Int16\n"
												"                                  OpCapability StorageBuffer16BitAccess\n";
		subs["EXTRA_EXTENSIONS"]			+=	"                                  OpExtension \"SPV_KHR_16bit_storage\"\n";
		subs["INPUT_BUFFER_VALUE_TYPE"]		=	"%short";
		subs["EXTRA_TYPES_AND_CONSTANTS"]	+=	"                         %short = OpTypeInt 16 1\n"
												"                      %short_37 = OpConstant %short 37\n";
		subs["CALC_ZERO_FOR_CALLABLE"]		=	"                    %zero_short = OpISub %short %input_val_before %short_37\n"
												"             %zero_for_callable = OpSConvert %uint %zero_short\n";
	}
	else if (m_params.dataType == DataType::UINT16)
	{
		componentTypeName = "ushort";

		subs["EXTRA_CAPABILITIES"]			+=	"                                  OpCapability Int16\n"
												"                                  OpCapability StorageBuffer16BitAccess\n";
		subs["EXTRA_EXTENSIONS"]			+=	"                                  OpExtension \"SPV_KHR_16bit_storage\"\n";
		subs["INPUT_BUFFER_VALUE_TYPE"]		=	"%ushort";
		subs["EXTRA_TYPES_AND_CONSTANTS"]	+=	"                        %ushort = OpTypeInt 16 0\n"
												"                     %ushort_37 = OpConstant %ushort 37\n";
		subs["CALC_ZERO_FOR_CALLABLE"]		=	"                   %zero_ushort = OpISub %ushort %input_val_before %ushort_37\n"
												"             %zero_for_callable = OpUConvert %uint %zero_ushort\n";
	}
	else if (m_params.dataType == DataType::INT8)
	{
		componentTypeName = "char";

		subs["EXTRA_CAPABILITIES"]			+=	"                                  OpCapability Int8\n"
												"                                  OpCapability StorageBuffer8BitAccess\n";
		subs["EXTRA_EXTENSIONS"]			+=	"                                  OpExtension \"SPV_KHR_8bit_storage\"\n";
		subs["INPUT_BUFFER_VALUE_TYPE"]		=	"%char";
		subs["EXTRA_TYPES_AND_CONSTANTS"]	+=	"                          %char = OpTypeInt 8 1\n"
												"                       %char_37 = OpConstant %char 37\n";
		subs["CALC_ZERO_FOR_CALLABLE"]		=	"                     %zero_char = OpISub %char %input_val_before %char_37\n"
												"             %zero_for_callable = OpSConvert %uint %zero_char\n";
	}
	else if (m_params.dataType == DataType::UINT8)
	{
		componentTypeName = "uchar";

		subs["EXTRA_CAPABILITIES"]			+=	"                                  OpCapability Int8\n"
												"                                  OpCapability StorageBuffer8BitAccess\n";
		subs["EXTRA_EXTENSIONS"]			+=	"                                  OpExtension \"SPV_KHR_8bit_storage\"\n";
		subs["INPUT_BUFFER_VALUE_TYPE"]		=	"%uchar";
		subs["EXTRA_TYPES_AND_CONSTANTS"]	+=	"                         %uchar = OpTypeInt 8 0\n"
												"                      %uchar_37 = OpConstant %uchar 37\n";
		subs["CALC_ZERO_FOR_CALLABLE"]		=	"                    %zero_uchar = OpISub %uchar %input_val_before %uchar_37\n"
												"             %zero_for_callable = OpUConvert %uint %zero_uchar\n";
	}
	else if (m_params.dataType == DataType::FLOAT32)
	{
		componentTypeName = "float";

		subs["INPUT_BUFFER_VALUE_TYPE"]		=	"%float";
		subs["EXTRA_TYPES_AND_CONSTANTS"]	+=	"                      %float_37 = OpConstant %float 37\n";
		subs["CALC_ZERO_FOR_CALLABLE"]		=	"                    %zero_float = OpFSub %float %input_val_before %float_37\n"
												"             %zero_for_callable = OpConvertFToU %uint %zero_float\n";
	}
	else if (m_params.dataType == DataType::FLOAT64)
	{
		componentTypeName = "double";

		subs["EXTRA_CAPABILITIES"]			+=	"                                  OpCapability Float64\n";
		subs["INPUT_BUFFER_VALUE_TYPE"]		=	"%double";
		subs["EXTRA_TYPES_AND_CONSTANTS"]	+=	"                        %double = OpTypeFloat 64\n"
												"                     %double_37 = OpConstant %double 37\n";
		subs["CALC_ZERO_FOR_CALLABLE"]		=	"                   %zero_double = OpFSub %double %input_val_before %double_37\n"
												"             %zero_for_callable = OpConvertFToU %uint %zero_double\n";
	}
	else if (m_params.dataType == DataType::FLOAT16)
	{
		componentTypeName = "half";

		subs["EXTRA_CAPABILITIES"]			+=	"                                  OpCapability Float16\n"
												"                                  OpCapability StorageBuffer16BitAccess\n";
		subs["EXTRA_EXTENSIONS"]			+=	"                                  OpExtension \"SPV_KHR_16bit_storage\"\n";
		subs["INPUT_BUFFER_VALUE_TYPE"]		=	"%half";
		subs["EXTRA_TYPES_AND_CONSTANTS"]	+=	"                          %half = OpTypeFloat 16\n"
												"                       %half_37 = OpConstant %half 37\n";
		subs["CALC_ZERO_FOR_CALLABLE"]		=	"                     %zero_half = OpFSub %half %input_val_before %half_37\n"
												"             %zero_for_callable = OpConvertFToU %uint %zero_half\n";
	}
	else if (m_params.dataType == DataType::STRUCT)
	{
		componentTypeName = "InputStruct";

		subs["INPUT_BUFFER_VALUE_TYPE"]		=	"%InputStruct";
		subs["EXTRA_TYPES_AND_CONSTANTS"]	+=	"                   %InputStruct = OpTypeStruct %uint %float\n"
												"                      %float_37 = OpConstant %float 37\n"
												"            %uint_part_ptr_type = OpTypePointer StorageBuffer %uint\n"
												"           %float_part_ptr_type = OpTypePointer StorageBuffer %float\n"
												"       %uint_part_func_ptr_type = OpTypePointer Function %uint\n"
												"      %float_part_func_ptr_type = OpTypePointer Function %float\n"
												"    %input_struct_func_ptr_type = OpTypePointer Function %InputStruct\n"
												;
		subs["INTERFACE_DECORATIONS"]		=	"                                  OpMemberDecorate %InputStruct 0 Offset 0\n"
												"                                  OpMemberDecorate %InputStruct 1 Offset 4\n";

		// Sum struct members, then substract constant and convert to uint.
		subs["CALC_ZERO_FOR_CALLABLE"]		=	"                 %uint_part_ptr = OpAccessChain %uint_part_ptr_type %input_val_ptr %uint_0\n"
												"                %float_part_ptr = OpAccessChain %float_part_ptr_type %input_val_ptr %uint_1\n"
												"                     %uint_part = OpLoad %uint %uint_part_ptr\n"
												"                    %float_part = OpLoad %float %float_part_ptr\n"
												"                 %uint_as_float = OpConvertUToF %float %uint_part\n"
												"                    %member_sum = OpFAdd %float %float_part %uint_as_float\n"
												"                    %zero_float = OpFSub %float %member_sum %float_37\n"
												"             %zero_for_callable = OpConvertFToU %uint %zero_float\n"
												;
	}
	else if (samplersNeeded(m_params.dataType))
	{
		// These tests will use additional bindings as arrays of 2 elements:
		// - 1 array of samplers.
		// - 1 array of images.
		// - 1 array of combined image samplers.
		// Input values are typically used as texture coordinates (normally zeros)
		// Pixels will contain the expected values instead of them being in the input buffer.

		subs["INPUT_BUFFER_VALUE_TYPE"]		=	"%float";
		subs["EXTRA_CAPABILITIES"]			+=	"                                  OpCapability SampledImageArrayNonUniformIndexing\n";
		subs["EXTRA_EXTENSIONS"]			+=	"                                  OpExtension \"SPV_EXT_descriptor_indexing\"\n";
		subs["MAIN_INTERFACE_EXTRAS"]		+=	" %sampledTexture %textureSampler %combinedImageSampler";
		subs["EXTRA_BINDINGS"]				+=	"                                  OpDecorate %sampledTexture DescriptorSet 0\n"
												"                                  OpDecorate %sampledTexture Binding 4\n"
												"                                  OpDecorate %textureSampler DescriptorSet 0\n"
												"                                  OpDecorate %textureSampler Binding 5\n"
												"                                  OpDecorate %combinedImageSampler DescriptorSet 0\n"
												"                                  OpDecorate %combinedImageSampler Binding 6\n";
		subs["EXTRA_TYPES_AND_CONSTANTS"]	+=	"                       %uint_37 = OpConstant %uint 37\n"
												"                        %v4uint = OpTypeVector %uint 4\n"
												"                       %v2float = OpTypeVector %float 2\n"
												"                    %image_type = OpTypeImage %uint 2D 0 0 0 1 Unknown\n"
												"              %image_array_type = OpTypeArray %image_type %uint_2\n"
												"  %image_array_type_uniform_ptr = OpTypePointer UniformConstant %image_array_type\n"
												"        %image_type_uniform_ptr = OpTypePointer UniformConstant %image_type\n"
												"                  %sampler_type = OpTypeSampler\n"
												"            %sampler_array_type = OpTypeArray %sampler_type %uint_2\n"
												"%sampler_array_type_uniform_ptr = OpTypePointer UniformConstant %sampler_array_type\n"
												"      %sampler_type_uniform_ptr = OpTypePointer UniformConstant %sampler_type\n"
												"            %sampled_image_type = OpTypeSampledImage %image_type\n"
												"      %sampled_image_array_type = OpTypeArray %sampled_image_type %uint_2\n"
												"%sampled_image_array_type_uniform_ptr = OpTypePointer UniformConstant %sampled_image_array_type\n"
												"%sampled_image_type_uniform_ptr = OpTypePointer UniformConstant %sampled_image_type\n"
												;
		subs["EXTRA_BINDING_VARIABLES"]		+=	"                %sampledTexture = OpVariable %image_array_type_uniform_ptr UniformConstant\n"
												"                %textureSampler = OpVariable %sampler_array_type_uniform_ptr UniformConstant\n"
												"          %combinedImageSampler = OpVariable %sampled_image_array_type_uniform_ptr UniformConstant\n"
												;

		if (m_params.dataType == DataType::IMAGE || m_params.dataType == DataType::SAMPLER)
		{
			// Use the first sampler and sample from the first image.
			subs["CALC_ZERO_FOR_CALLABLE"]	+=	"%image_0_ptr = OpAccessChain %image_type_uniform_ptr %sampledTexture %uint_0\n"
												"%sampler_0_ptr = OpAccessChain %sampler_type_uniform_ptr %textureSampler %uint_0\n"
												"%sampler_0 = OpLoad %sampler_type %sampler_0_ptr\n"
												"%image_0 = OpLoad %image_type %image_0_ptr\n"
												"%sampled_image_0 = OpSampledImage %sampled_image_type %image_0 %sampler_0\n"
												"%texture_coords_0 = OpCompositeConstruct %v2float %input_val_before %input_val_before\n"
												"%pixel_vec_0 = OpImageSampleExplicitLod %v4uint %sampled_image_0 %texture_coords_0 Lod|ZeroExtend %float_0\n"
												"%pixel_0 = OpCompositeExtract %uint %pixel_vec_0 0\n"
												"%zero_for_callable = OpISub %uint %pixel_0 %uint_37\n"
												;
		}
		else if (m_params.dataType == DataType::SAMPLED_IMAGE)
		{
			// Use the first combined image sampler.
			subs["CALC_ZERO_FOR_CALLABLE"]	+=	"%sampled_image_0_ptr = OpAccessChain %sampled_image_type_uniform_ptr %combinedImageSampler %uint_0\n"
												"%sampled_image_0 = OpLoad %sampled_image_type %sampled_image_0_ptr\n"
												"%texture_coords_0 = OpCompositeConstruct %v2float %input_val_before %input_val_before\n"
												"%pixel_vec_0 = OpImageSampleExplicitLod %v4uint %sampled_image_0 %texture_coords_0 Lod|ZeroExtend %float_0\n"
												"%pixel_0 = OpCompositeExtract %uint %pixel_vec_0 0\n"
												"%zero_for_callable = OpISub %uint %pixel_0 %uint_37\n"
												;
		}
		else if (m_params.dataType == DataType::PTR_IMAGE)
		{
			// We attempt to create the second pointer before the call.
			subs["CALC_ZERO_FOR_CALLABLE"]		+=	"%image_0_ptr = OpAccessChain %image_type_uniform_ptr %sampledTexture %uint_0\n"
													"%image_1_ptr = OpAccessChain %image_type_uniform_ptr %sampledTexture %uint_1\n"
													"%image_0 = OpLoad %image_type %image_0_ptr\n"
													"%sampler_0_ptr = OpAccessChain %sampler_type_uniform_ptr %textureSampler %uint_0\n"
													"%sampler_0 = OpLoad %sampler_type %sampler_0_ptr\n"
													"%sampled_image_0 = OpSampledImage %sampled_image_type %image_0 %sampler_0\n"
													"%texture_coords_0 = OpCompositeConstruct %v2float %input_val_before %input_val_before\n"
													"%pixel_vec_0 = OpImageSampleExplicitLod %v4uint %sampled_image_0 %texture_coords_0 Lod|ZeroExtend %float_0\n"
													"%pixel_0 = OpCompositeExtract %uint %pixel_vec_0 0\n"
													"%zero_for_callable = OpISub %uint %pixel_0 %uint_37\n"
													;
		}
		else if (m_params.dataType == DataType::PTR_SAMPLER)
		{
			// We attempt to create the second pointer before the call.
			subs["CALC_ZERO_FOR_CALLABLE"]		+=	"%sampler_0_ptr = OpAccessChain %sampler_type_uniform_ptr %textureSampler %uint_0\n"
													"%sampler_1_ptr = OpAccessChain %sampler_type_uniform_ptr %textureSampler %uint_1\n"
													"%sampler_0 = OpLoad %sampler_type %sampler_0_ptr\n"
													"%image_0_ptr = OpAccessChain %image_type_uniform_ptr %sampledTexture %uint_0\n"
													"%image_0 = OpLoad %image_type %image_0_ptr\n"
													"%sampled_image_0 = OpSampledImage %sampled_image_type %image_0 %sampler_0\n"
													"%texture_coords_0 = OpCompositeConstruct %v2float %input_val_before %input_val_before\n"
													"%pixel_vec_0 = OpImageSampleExplicitLod %v4uint %sampled_image_0 %texture_coords_0 Lod|ZeroExtend %float_0\n"
													"%pixel_0 = OpCompositeExtract %uint %pixel_vec_0 0\n"
													"%zero_for_callable = OpISub %uint %pixel_0 %uint_37\n"
													;
		}
		else if (m_params.dataType == DataType::PTR_SAMPLED_IMAGE)
		{
			// We attempt to create the second pointer before the call.
			subs["CALC_ZERO_FOR_CALLABLE"]		+=	"%sampled_image_0_ptr = OpAccessChain %sampled_image_type_uniform_ptr %combinedImageSampler %uint_0\n"
													"%sampled_image_1_ptr = OpAccessChain %sampled_image_type_uniform_ptr %combinedImageSampler %uint_1\n"
													"%sampled_image_0 = OpLoad %sampled_image_type %sampled_image_0_ptr\n"
													"%texture_coords_0 = OpCompositeConstruct %v2float %input_val_before %input_val_before\n"
													"%pixel_vec_0 = OpImageSampleExplicitLod %v4uint %sampled_image_0 %texture_coords_0 Lod|ZeroExtend %float_0\n"
													"%pixel_0 = OpCompositeExtract %uint %pixel_vec_0 0\n"
													"%zero_for_callable = OpISub %uint %pixel_0 %uint_37\n"
													;
		}
		else
		{
			DE_ASSERT(false);
		}
	}
	else if (storageImageNeeded(m_params.dataType))
	{
		subs["INPUT_BUFFER_VALUE_TYPE"]		=	"%int";
		subs["MAIN_INTERFACE_EXTRAS"]		+=	" %storageImage";
		subs["EXTRA_BINDINGS"]				+=	"                                  OpDecorate %storageImage DescriptorSet 0\n"
												"                                  OpDecorate %storageImage Binding 4\n"
												;
		subs["EXTRA_TYPES_AND_CONSTANTS"]	+=	"                       %uint_37 = OpConstant %uint 37\n"
												"                         %v2int = OpTypeVector %int 2\n"
												"                    %image_type = OpTypeImage %uint 2D 0 0 0 2 R32ui\n"
												"        %image_type_uniform_ptr = OpTypePointer UniformConstant %image_type\n"
												"                  %uint_img_ptr = OpTypePointer Image %uint\n"
												;
		subs["EXTRA_BINDING_VARIABLES"]		+=	"                  %storageImage = OpVariable %image_type_uniform_ptr UniformConstant\n"
												;

		// Load value from the image, expecting it to be 37 and swapping it with 5.
		subs["CALC_ZERO_FOR_CALLABLE"]	+=	"%coords = OpCompositeConstruct %v2int %input_val_before %input_val_before\n"
											"%texel_ptr = OpImageTexelPointer %uint_img_ptr %storageImage %coords %uint_0\n"
											"%texel_value = OpAtomicCompareExchange %uint %texel_ptr %uint_1 %uint_0 %uint_0 %uint_5 %uint_37\n"
											"%zero_for_callable = OpISub %uint %texel_value %uint_37\n"
											;
	}
	else if (m_params.dataType == DataType::OP_NULL)
	{
		subs["INPUT_BUFFER_VALUE_TYPE"]		=	"%uint";
		subs["EXTRA_TYPES_AND_CONSTANTS"]	+=	"                       %uint_37 = OpConstant %uint 37\n"
												"                 %constant_null = OpConstantNull %uint\n"
												;

		// Create a local copy of the null constant global object to work with it.
		subs["CALC_ZERO_FOR_CALLABLE"]	+=	"%constant_null_copy = OpCopyObject %uint %constant_null\n"
											"%is_37_before = OpIEqual %bool %input_val_before %uint_37\n"
											"%zero_for_callable = OpSelect %uint %is_37_before %constant_null_copy %uint_5\n"
											;
	}
	else if (m_params.dataType == DataType::OP_UNDEF)
	{
		subs["INPUT_BUFFER_VALUE_TYPE"]		=	"%uint";
		subs["EXTRA_TYPES_AND_CONSTANTS"]	+=	"                       %uint_37 = OpConstant %uint 37\n"
												;

		// Extract an undef value and write it to the output buffer to make sure it's used before the call. The value will be overwritten later.
		subs["CALC_ZERO_FOR_CALLABLE"]	+=	"%undef_var = OpUndef %uint\n"
											"%undef_val_before = OpCopyObject %uint %undef_var\n"
											"OpStore %output_val_ptr %undef_val_before Volatile\n"
											"%zero_for_callable = OpISub %uint %uint_37 %input_val_before\n"
											;
	}
	else
	{
		DE_ASSERT(false);
	}

	// Comparison statement for data before and after the call.
	switch (m_params.dataType)
	{
	case DataType::INT32:
	case DataType::UINT32:
	case DataType::INT64:
	case DataType::UINT64:
	case DataType::INT16:
	case DataType::UINT16:
	case DataType::INT8:
	case DataType::UINT8:
		opEqual = "OpIEqual";
		break;
	case DataType::FLOAT32:
	case DataType::FLOAT64:
	case DataType::FLOAT16:
		opEqual = "OpFOrdEqual";
		break;
	case DataType::STRUCT:
	case DataType::IMAGE:
	case DataType::SAMPLER:
	case DataType::SAMPLED_IMAGE:
	case DataType::PTR_IMAGE:
	case DataType::PTR_SAMPLER:
	case DataType::PTR_SAMPLED_IMAGE:
	case DataType::PTR_TEXEL:
	case DataType::OP_NULL:
	case DataType::OP_UNDEF:
		// These needs special code for the comparison.
		opEqual = "INVALID";
		break;
	default:
		DE_ASSERT(false);
		break;
	}

	if (m_params.dataType == DataType::STRUCT)
	{
		// We need to store the before and after values in a variable in order to be able to access each member individually without accessing the StorageBuffer again.
		subs["EXTRA_FUNCTION_VARIABLES"]	=	"         %input_val_func_before = OpVariable %input_struct_func_ptr_type Function\n"
												"          %input_val_func_after = OpVariable %input_struct_func_ptr_type Function\n"
												;
		subs["CALC_EQUAL_STATEMENT"]		=	"                                  OpStore %input_val_func_before %input_val_before\n"
												"                                  OpStore %input_val_func_after %input_val_after\n"
												"     %uint_part_func_before_ptr = OpAccessChain %uint_part_func_ptr_type %input_val_func_before %uint_0\n"
												"    %float_part_func_before_ptr = OpAccessChain %float_part_func_ptr_type %input_val_func_before %uint_1\n"
												"      %uint_part_func_after_ptr = OpAccessChain %uint_part_func_ptr_type %input_val_func_after %uint_0\n"
												"     %float_part_func_after_ptr = OpAccessChain %float_part_func_ptr_type %input_val_func_after %uint_1\n"
												"              %uint_part_before = OpLoad %uint %uint_part_func_before_ptr\n"
												"             %float_part_before = OpLoad %float %float_part_func_before_ptr\n"
												"               %uint_part_after = OpLoad %uint %uint_part_func_after_ptr\n"
												"              %float_part_after = OpLoad %float %float_part_func_after_ptr\n"
												"                    %uint_equal = OpIEqual %bool %uint_part_before %uint_part_after\n"
												"                   %float_equal = OpFOrdEqual %bool %float_part_before %float_part_after\n"
												"                         %equal = OpLogicalAnd %bool %uint_equal %float_equal\n"
												;
	}
	else if (m_params.dataType == DataType::IMAGE)
	{
		// Use the same image and the second sampler with different coordinates (actually the same).
		subs["CALC_EQUAL_STATEMENT"]	+=	"%sampler_1_ptr = OpAccessChain %sampler_type_uniform_ptr %textureSampler %uint_1\n"
											"%sampler_1 = OpLoad %sampler_type %sampler_1_ptr\n"
											"%sampled_image_1 = OpSampledImage %sampled_image_type %image_0 %sampler_1\n"
											"%texture_coords_1 = OpCompositeConstruct %v2float %input_val_after %input_val_after\n"
											"%pixel_vec_1 = OpImageSampleExplicitLod %v4uint %sampled_image_1 %texture_coords_1 Lod|ZeroExtend %float_0\n"
											"%pixel_1 = OpCompositeExtract %uint %pixel_vec_1 0\n"
											"%equal = OpIEqual %bool %pixel_0 %pixel_1\n"
											;
	}
	else if (m_params.dataType == DataType::SAMPLER)
	{
		// Use the same sampler and sample from the second image with different coordinates (but actually the same).
		subs["CALC_EQUAL_STATEMENT"]	+=	"%image_1_ptr = OpAccessChain %image_type_uniform_ptr %sampledTexture %uint_1\n"
											"%image_1 = OpLoad %image_type %image_1_ptr\n"
											"%sampled_image_1 = OpSampledImage %sampled_image_type %image_1 %sampler_0\n"
											"%texture_coords_1 = OpCompositeConstruct %v2float %input_val_after %input_val_after\n"
											"%pixel_vec_1 = OpImageSampleExplicitLod %v4uint %sampled_image_1 %texture_coords_1 Lod|ZeroExtend %float_0\n"
											"%pixel_1 = OpCompositeExtract %uint %pixel_vec_1 0\n"
											"%equal = OpIEqual %bool %pixel_0 %pixel_1\n"
											;
	}
	else if (m_params.dataType == DataType::SAMPLED_IMAGE)
	{
		// Reuse the same combined image sampler with different coordinates (actually the same).
		subs["CALC_EQUAL_STATEMENT"]	+=	"%texture_coords_1 = OpCompositeConstruct %v2float %input_val_after %input_val_after\n"
											"%pixel_vec_1 = OpImageSampleExplicitLod %v4uint %sampled_image_0 %texture_coords_1 Lod|ZeroExtend %float_0\n"
											"%pixel_1 = OpCompositeExtract %uint %pixel_vec_1 0\n"
											"%equal = OpIEqual %bool %pixel_0 %pixel_1\n"
											;
	}
	else if (m_params.dataType == DataType::PTR_IMAGE)
	{
		// We attempt to use the second pointer only after the call.
		subs["CALC_EQUAL_STATEMENT"]	+=	"%image_1 = OpLoad %image_type %image_1_ptr\n"
											"%sampled_image_1 = OpSampledImage %sampled_image_type %image_1 %sampler_0\n"
											"%texture_coords_1 = OpCompositeConstruct %v2float %input_val_after %input_val_after\n"
											"%pixel_vec_1 = OpImageSampleExplicitLod %v4uint %sampled_image_1 %texture_coords_1 Lod|ZeroExtend %float_0\n"
											"%pixel_1 = OpCompositeExtract %uint %pixel_vec_1 0\n"
											"%equal = OpIEqual %bool %pixel_0 %pixel_1\n"
											;

	}
	else if (m_params.dataType == DataType::PTR_SAMPLER)
	{
		// We attempt to use the second pointer only after the call.
		subs["CALC_EQUAL_STATEMENT"]	+=	"%sampler_1 = OpLoad %sampler_type %sampler_1_ptr\n"
											"%sampled_image_1 = OpSampledImage %sampled_image_type %image_0 %sampler_1\n"
											"%texture_coords_1 = OpCompositeConstruct %v2float %input_val_after %input_val_after\n"
											"%pixel_vec_1 = OpImageSampleExplicitLod %v4uint %sampled_image_1 %texture_coords_1 Lod|ZeroExtend %float_0\n"
											"%pixel_1 = OpCompositeExtract %uint %pixel_vec_1 0\n"
											"%equal = OpIEqual %bool %pixel_0 %pixel_1\n"
											;
	}
	else if (m_params.dataType == DataType::PTR_SAMPLED_IMAGE)
	{
		// We attempt to use the second pointer only after the call.
		subs["CALC_EQUAL_STATEMENT"]	+=	"%sampled_image_1 = OpLoad %sampled_image_type %sampled_image_1_ptr\n"
											"%texture_coords_1 = OpCompositeConstruct %v2float %input_val_after %input_val_after\n"
											"%pixel_vec_1 = OpImageSampleExplicitLod %v4uint %sampled_image_1 %texture_coords_1 Lod|ZeroExtend %float_0\n"
											"%pixel_1 = OpCompositeExtract %uint %pixel_vec_1 0\n"
											"%equal = OpIEqual %bool %pixel_0 %pixel_1\n"
											;
	}
	else if (m_params.dataType == DataType::PTR_TEXEL)
	{
		// Check value 5 was stored properly.
		subs["CALC_EQUAL_STATEMENT"]	+=	"%stored_val = OpAtomicLoad %uint %texel_ptr %uint_1 %uint_0\n"
											"%equal = OpIEqual %bool %stored_val %uint_5\n"
											;
	}
	else if (m_params.dataType == DataType::OP_NULL)
	{
		// Reuse the null constant after the call.
		subs["CALC_EQUAL_STATEMENT"]	+=	"%is_37_after = OpIEqual %bool %input_val_after %uint_37\n"
											"%writeback_val = OpSelect %uint %is_37_after %constant_null_copy %uint_5\n"
											"OpStore %input_val_ptr %writeback_val Volatile\n"
											"%readback_val = OpLoad %uint %input_val_ptr Volatile\n"
											"%equal = OpIEqual %bool %readback_val %uint_0\n"
											;
	}
	else if (m_params.dataType == DataType::OP_UNDEF)
	{
		// Extract another undef value and write it to the input buffer. It will not be checked later.
		subs["CALC_EQUAL_STATEMENT"]	+=	"%undef_val_after = OpCopyObject %uint %undef_var\n"
											"OpStore %input_val_ptr %undef_val_after Volatile\n"
											"%equal = OpIEqual %bool %input_val_after %input_val_before\n"
											;
	}
	else
	{
		subs["CALC_EQUAL_STATEMENT"]	+=	"                         %equal = " + opEqual + " %bool %input_val_before %input_val_after\n";
	}

	// Modifications for vectors and arrays.
	if (numComponents > 1)
	{
		const std::string	vectorTypeName		= "v" + numComponentsStr + componentTypeName;
		const std::string	opType				= (isArray ? "OpTypeArray" : "OpTypeVector");
		const std::string	componentCountStr	= (isArray ? ("%uint_" + numComponentsStr) : numComponentsStr);

		// Some extra types are needed.
		if (!(m_params.dataType == DataType::FLOAT32 && m_params.vectorType == VectorType::V3))
		{
			// Note: v3float is already defined in the shader by default.
			subs["EXTRA_TYPES_AND_CONSTANTS"] += "%" + vectorTypeName + " = " + opType + " %" + componentTypeName + " " + componentCountStr + "\n";
		}
		subs["EXTRA_TYPES_AND_CONSTANTS"]	+=	"%v" + numComponentsStr + "bool = " + opType + " %bool " + componentCountStr + "\n";
		subs["EXTRA_TYPES_AND_CONSTANTS"]	+=	"%comp_ptr = OpTypePointer StorageBuffer %" + componentTypeName + "\n";

		// The input value in the buffer has a different type.
		subs["INPUT_BUFFER_VALUE_TYPE"]		=	"%" + vectorTypeName;

		// Overwrite the way we calculate the zero used in the call.

		// Proper operations for adding, substracting and converting components.
		std::string opAdd;
		std::string opSub;
		std::string opConvert;

		switch (m_params.dataType)
		{
		case DataType::INT32:
		case DataType::UINT32:
		case DataType::INT64:
		case DataType::UINT64:
		case DataType::INT16:
		case DataType::UINT16:
		case DataType::INT8:
		case DataType::UINT8:
			opAdd = "OpIAdd";
			opSub = "OpISub";
			break;
		case DataType::FLOAT32:
		case DataType::FLOAT64:
		case DataType::FLOAT16:
			opAdd = "OpFAdd";
			opSub = "OpFSub";
			break;
		default:
			DE_ASSERT(false);
			break;
		}

		switch (m_params.dataType)
		{
		case DataType::UINT32:
			opConvert = "OpCopyObject";
			break;
		case DataType::INT32:
			opConvert = "OpBitcast";
			break;
		case DataType::INT64:
		case DataType::INT16:
		case DataType::INT8:
			opConvert = "OpSConvert";
			break;
		case DataType::UINT64:
		case DataType::UINT16:
		case DataType::UINT8:
			opConvert = "OpUConvert";
			break;
		case DataType::FLOAT32:
		case DataType::FLOAT64:
		case DataType::FLOAT16:
			opConvert = "OpConvertFToU";
			break;
		default:
			DE_ASSERT(false);
			break;
		}

		std::ostringstream zeroForCallable;

		// Create pointers to components and load components.
		for (int i = 0; i < numComponents; ++i)
		{
			zeroForCallable
				<< "%component_ptr_" << i << " = OpAccessChain %comp_ptr %input_val_ptr %uint_" << i << "\n"
				<< "%component_" << i << " = OpLoad %" << componentTypeName << " %component_ptr_" << i << "\n"
				;
		}

		// Sum components together in %total_sum.
		for (int i = 1; i < numComponents; ++i)
		{
			const std::string previous		= ((i == 1) ? "%component_0" : ("%partial_" + de::toString(i-1)));
			const std::string resultName	= ((i == (numComponents - 1)) ? "%total_sum" : ("%partial_" + de::toString(i)));
			zeroForCallable << resultName << " = " << opAdd << " %" << componentTypeName << " %component_" << i << " " << previous << "\n";
		}

		// Recalculate the zero.
		zeroForCallable
			<< "%zero_" << componentTypeName << " = " << opSub << " %" << componentTypeName << " %total_sum %" << componentTypeName << "_37\n"
			<< "%zero_for_callable = " << opConvert << " %uint %zero_" << componentTypeName << "\n"
			;

		// Finally replace the zero_for_callable statements with the special version for vectors.
		subs["CALC_ZERO_FOR_CALLABLE"] = zeroForCallable.str();

		// Rework comparison statements.
		if (isArray)
		{
			// Arrays need to be compared per-component.
			std::ostringstream calcEqual;

			for (int i = 0; i < numComponents; ++i)
			{
				calcEqual
					<< "%component_after_" << i << " = OpLoad %" << componentTypeName << " %component_ptr_" << i << "\n"
					<< "%equal_" << i << " = " << opEqual << " %bool %component_" << i << " %component_after_" << i << "\n";
				if (i > 0)
					calcEqual << "%and_" << i << " = OpLogicalAnd %bool %equal_" << (i - 1) << " %equal_" << i << "\n";
				if (i == numComponents - 1)
					calcEqual << "%equal = OpCopyObject %bool %and_" << i << "\n";
			}

			subs["CALC_EQUAL_STATEMENT"] = calcEqual.str();
		}
		else
		{
			// Vectors can be compared using a bool vector and OpAll.
			subs["CALC_EQUAL_STATEMENT"] =	"                  %equal_vector = " + opEqual + " %v" + numComponentsStr + "bool %input_val_before %input_val_after\n";
			subs["CALC_EQUAL_STATEMENT"] +=	"                         %equal = OpAll %bool %equal_vector\n";
		}
	}

	if (isArray)
	{
		// Arrays need an ArrayStride decoration.
		std::ostringstream interfaceDecorations;
		interfaceDecorations << "OpDecorate %v" << numComponentsStr << componentTypeName << " ArrayStride " << getElementSize(m_params.dataType, VectorType::SCALAR) << "\n";
		subs["INTERFACE_DECORATIONS"] = interfaceDecorations.str();
	}

	const auto inputBlockDecls = getGLSLInputValDecl(m_params.dataType, m_params.vectorType);

	std::ostringstream glslBindings;
	glslBindings
		<< inputBlockDecls.first // Additional data types needed.
		<< "layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;\n"
		<< "layout(set = 0, binding = 1) buffer CalleeBlock { uint val; } calleeBuffer;\n"
		<< "layout(set = 0, binding = 2) buffer OutputBlock { uint val; } outputBuffer;\n"
		<< "layout(set = 0, binding = 3) buffer InputBlock { " << inputBlockDecls.second << " } inputBuffer;\n"
		;

	if (samplersNeeded(m_params.dataType))
	{
		glslBindings
			<< "layout(set = 0, binding = 4) uniform utexture2D sampledTexture[2];\n"
			<< "layout(set = 0, binding = 5) uniform sampler textureSampler[2];\n"
			<< "layout(set = 0, binding = 6) uniform usampler2D combinedImageSampler[2];\n"
			;
	}
	else if (storageImageNeeded(m_params.dataType))
	{
		glslBindings
			<< "layout(set = 0, binding = 4, r32ui) uniform uimage2D storageImage;\n"
			;
	}

	const auto glslBindingsStr	=	glslBindings.str();
	const auto glslHeaderStr	=	"#version 460 core\n"
									"#extension GL_EXT_ray_tracing : require\n"
									"#extension GL_EXT_shader_explicit_arithmetic_types : require\n";


	if (m_params.callType == CallType::TRACE_RAY)
	{
		subs["ENTRY_POINT"]						=	"RayGenerationKHR";
		subs["MAIN_INTERFACE_EXTRAS"]			+=	" %hitValue";
		subs["INTERFACE_DECORATIONS"]			+=	"                                  OpDecorate %hitValue Location 0\n";
		subs["INTERFACE_TYPES_AND_VARIABLES"]	=	"                   %payload_ptr = OpTypePointer RayPayloadKHR %v3float\n"
													"                      %hitValue = OpVariable %payload_ptr RayPayloadKHR\n";
		subs["CALL_STATEMENTS"]					=	"                      %as_value = OpLoad %as_type %topLevelAS\n"
													"                                  OpTraceRayKHR %as_value %uint_0 %uint_255 %zero_for_callable %zero_for_callable %zero_for_callable %origin_const %float_0 %direction_const %float_9 %hitValue\n";

		const auto rgen = spvTemplate.specialize(subs);
		programCollection.spirvAsmSources.add("rgen") << rgen << spvBuildOptions;

		std::stringstream chit;
		chit
			<< glslHeaderStr
			<< "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
			<< "hitAttributeEXT vec3 attribs;\n"
			<< glslBindingsStr
			<< "void main()\n"
			<< "{\n"
			<< "    calleeBuffer.val = 1u;\n"
			<< "}\n"
			;
		programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(chit.str())) << buildOptions;
	}
	else if (m_params.callType == CallType::EXECUTE_CALLABLE)
	{
		subs["ENTRY_POINT"]						=	"RayGenerationKHR";
		subs["MAIN_INTERFACE_EXTRAS"]			+=	" %callableData";
		subs["INTERFACE_DECORATIONS"]			+=	"                                  OpDecorate %callableData Location 0\n";
		subs["INTERFACE_TYPES_AND_VARIABLES"]	=	"             %callable_data_ptr = OpTypePointer CallableDataKHR %float\n"
													"                  %callableData = OpVariable %callable_data_ptr CallableDataKHR\n";
		subs["CALL_STATEMENTS"]					=	"                                  OpExecuteCallableKHR %zero_for_callable %callableData\n";

		const auto rgen = spvTemplate.specialize(subs);
		programCollection.spirvAsmSources.add("rgen") << rgen << spvBuildOptions;

		std::ostringstream call;
		call
			<< glslHeaderStr
			<< "layout(location = 0) callableDataInEXT float callableData;\n"
			<< glslBindingsStr
			<< "void main()\n"
			<< "{\n"
			<< "    calleeBuffer.val = 1u;\n"
			<< "}\n"
			;

		programCollection.glslSources.add("call") << glu::CallableSource(updateRayTracingGLSL(call.str())) << buildOptions;
	}
	else if (m_params.callType == CallType::REPORT_INTERSECTION)
	{
		subs["ENTRY_POINT"]						=	"IntersectionKHR";
		subs["MAIN_INTERFACE_EXTRAS"]			+=	" %attribs";
		subs["INTERFACE_DECORATIONS"]			+=	"";
		subs["INTERFACE_TYPES_AND_VARIABLES"]	=	"             %hit_attribute_ptr = OpTypePointer HitAttributeKHR %v3float\n"
													"                       %attribs = OpVariable %hit_attribute_ptr HitAttributeKHR\n";
		subs["CALL_STATEMENTS"]					=	"              %intersection_ret = OpReportIntersectionKHR %bool %float_1 %zero_for_callable\n";

		const auto rint = spvTemplate.specialize(subs);
		programCollection.spirvAsmSources.add("rint") << rint << spvBuildOptions;

		std::ostringstream rgen;
		rgen
			<< glslHeaderStr
			<< "layout(location = 0) rayPayloadEXT vec3 hitValue;\n"
			<< glslBindingsStr
			<< "void main()\n"
			<< "{\n"
			<< "  traceRayEXT(topLevelAS, 0u, 0xFFu, 0, 0, 0, vec3(0.5, 0.5, 0.0), 0.0, vec3(0.0, 0.0, -1.0), 9.0, 0);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(rgen.str())) << buildOptions;

		std::stringstream ahit;
		ahit
			<< glslHeaderStr
			<< "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
			<< "hitAttributeEXT vec3 attribs;\n"
			<< glslBindingsStr
			<< "void main()\n"
			<< "{\n"
			<< "    calleeBuffer.val = 1u;\n"
			<< "}\n"
			;
		programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(ahit.str())) << buildOptions;
	}
	else
	{
		DE_ASSERT(false);
	}
}

using v2i32 = tcu::Vector<deInt32, 2>;
using v3i32 = tcu::Vector<deInt32, 3>;
using v4i32 = tcu::Vector<deInt32, 4>;
using a5i32 = std::array<deInt32, 5>;

using v2u32 = tcu::Vector<deUint32, 2>;
using v3u32 = tcu::Vector<deUint32, 3>;
using v4u32 = tcu::Vector<deUint32, 4>;
using a5u32 = std::array<deUint32, 5>;

using v2i64 = tcu::Vector<deInt64, 2>;
using v3i64 = tcu::Vector<deInt64, 3>;
using v4i64 = tcu::Vector<deInt64, 4>;
using a5i64 = std::array<deInt64, 5>;

using v2u64 = tcu::Vector<deUint64, 2>;
using v3u64 = tcu::Vector<deUint64, 3>;
using v4u64 = tcu::Vector<deUint64, 4>;
using a5u64 = std::array<deUint64, 5>;

using v2i16 = tcu::Vector<deInt16, 2>;
using v3i16 = tcu::Vector<deInt16, 3>;
using v4i16 = tcu::Vector<deInt16, 4>;
using a5i16 = std::array<deInt16, 5>;

using v2u16 = tcu::Vector<deUint16, 2>;
using v3u16 = tcu::Vector<deUint16, 3>;
using v4u16 = tcu::Vector<deUint16, 4>;
using a5u16 = std::array<deUint16, 5>;

using v2i8 = tcu::Vector<deInt8, 2>;
using v3i8 = tcu::Vector<deInt8, 3>;
using v4i8 = tcu::Vector<deInt8, 4>;
using a5i8 = std::array<deInt8, 5>;

using v2u8 = tcu::Vector<deUint8, 2>;
using v3u8 = tcu::Vector<deUint8, 3>;
using v4u8 = tcu::Vector<deUint8, 4>;
using a5u8 = std::array<deUint8, 5>;

using v2f32 = tcu::Vector<tcu::Float32, 2>;
using v3f32 = tcu::Vector<tcu::Float32, 3>;
using v4f32 = tcu::Vector<tcu::Float32, 4>;
using a5f32 = std::array<tcu::Float32, 5>;

using v2f64 = tcu::Vector<tcu::Float64, 2>;
using v3f64 = tcu::Vector<tcu::Float64, 3>;
using v4f64 = tcu::Vector<tcu::Float64, 4>;
using a5f64 = std::array<tcu::Float64, 5>;

using v2f16 = tcu::Vector<tcu::Float16, 2>;
using v3f16 = tcu::Vector<tcu::Float16, 3>;
using v4f16 = tcu::Vector<tcu::Float16, 4>;
using a5f16 = std::array<tcu::Float16, 5>;

// Scalar types get filled with value 37, matching the value that will be substracted in the shader.
#define GEN_SCALAR_FILL(DATA_TYPE)											\
	do {																	\
		const auto inputBufferValue = static_cast<DATA_TYPE>(37.0);			\
		deMemcpy(bufferPtr, &inputBufferValue, sizeof(inputBufferValue));	\
	} while (0)

// Vector types get filled with values that add up to 37, matching the value that will be substracted in the shader.
#define GEN_V2_FILL(DATA_TYPE)												\
	do {																	\
		DATA_TYPE inputBufferValue;											\
		inputBufferValue.x() = static_cast<DATA_TYPE::Element>(21.0);		\
		inputBufferValue.y() = static_cast<DATA_TYPE::Element>(16.0);		\
		deMemcpy(bufferPtr, &inputBufferValue, sizeof(inputBufferValue));	\
	} while (0)

#define GEN_V3_FILL(DATA_TYPE)												\
	do {																	\
		DATA_TYPE inputBufferValue;											\
		inputBufferValue.x() = static_cast<DATA_TYPE::Element>(11.0);		\
		inputBufferValue.y() = static_cast<DATA_TYPE::Element>(19.0);		\
		inputBufferValue.z() = static_cast<DATA_TYPE::Element>(7.0);		\
		deMemcpy(bufferPtr, &inputBufferValue, sizeof(inputBufferValue));	\
	} while (0)

#define GEN_V4_FILL(DATA_TYPE)												\
	do {																	\
		DATA_TYPE inputBufferValue;											\
		inputBufferValue.x() = static_cast<DATA_TYPE::Element>(9.0);		\
		inputBufferValue.y() = static_cast<DATA_TYPE::Element>(11.0);		\
		inputBufferValue.z() = static_cast<DATA_TYPE::Element>(3.0);		\
		inputBufferValue.w() = static_cast<DATA_TYPE::Element>(14.0);		\
		deMemcpy(bufferPtr, &inputBufferValue, sizeof(inputBufferValue));	\
	} while (0)

#define GEN_A5_FILL(DATA_TYPE)															\
	do {																				\
		DATA_TYPE inputBufferValue;														\
		inputBufferValue[0] = static_cast<DATA_TYPE::value_type>(13.0);					\
		inputBufferValue[1] = static_cast<DATA_TYPE::value_type>(6.0);					\
		inputBufferValue[2] = static_cast<DATA_TYPE::value_type>(2.0);					\
		inputBufferValue[3] = static_cast<DATA_TYPE::value_type>(5.0);					\
		inputBufferValue[4] = static_cast<DATA_TYPE::value_type>(11.0);					\
		deMemcpy(bufferPtr, inputBufferValue.data(), de::dataSize(inputBufferValue));	\
	} while (0)

void fillInputBuffer (DataType dataType, VectorType vectorType, void* bufferPtr)
{
	if (vectorType == VectorType::SCALAR)
	{
		if		(dataType == DataType::INT32)	GEN_SCALAR_FILL(deInt32);
		else if	(dataType == DataType::UINT32)	GEN_SCALAR_FILL(deUint32);
		else if	(dataType == DataType::INT64)	GEN_SCALAR_FILL(deInt64);
		else if	(dataType == DataType::UINT64)	GEN_SCALAR_FILL(deUint64);
		else if	(dataType == DataType::INT16)	GEN_SCALAR_FILL(deInt16);
		else if	(dataType == DataType::UINT16)	GEN_SCALAR_FILL(deUint16);
		else if	(dataType == DataType::INT8)	GEN_SCALAR_FILL(deInt8);
		else if	(dataType == DataType::UINT8)	GEN_SCALAR_FILL(deUint8);
		else if	(dataType == DataType::FLOAT32)	GEN_SCALAR_FILL(tcu::Float32);
		else if	(dataType == DataType::FLOAT64)	GEN_SCALAR_FILL(tcu::Float64);
		else if	(dataType == DataType::FLOAT16)	GEN_SCALAR_FILL(tcu::Float16);
		else if (dataType == DataType::STRUCT)
		{
			InputStruct data = { 12u, 25.0f };
			deMemcpy(bufferPtr, &data, sizeof(data));
		}
		else if (dataType == DataType::OP_NULL)		GEN_SCALAR_FILL(deUint32);
		else if (dataType == DataType::OP_UNDEF)	GEN_SCALAR_FILL(deUint32);
		else
		{
			DE_ASSERT(false);
		}
	}
	else if (vectorType == VectorType::V2)
	{
		if		(dataType == DataType::INT32)	GEN_V2_FILL(v2i32);
		else if	(dataType == DataType::UINT32)	GEN_V2_FILL(v2u32);
		else if	(dataType == DataType::INT64)	GEN_V2_FILL(v2i64);
		else if	(dataType == DataType::UINT64)	GEN_V2_FILL(v2u64);
		else if	(dataType == DataType::INT16)	GEN_V2_FILL(v2i16);
		else if	(dataType == DataType::UINT16)	GEN_V2_FILL(v2u16);
		else if	(dataType == DataType::INT8)	GEN_V2_FILL(v2i8);
		else if	(dataType == DataType::UINT8)	GEN_V2_FILL(v2u8);
		else if	(dataType == DataType::FLOAT32)	GEN_V2_FILL(v2f32);
		else if	(dataType == DataType::FLOAT64)	GEN_V2_FILL(v2f64);
		else if	(dataType == DataType::FLOAT16)	GEN_V2_FILL(v2f16);
		else
		{
			DE_ASSERT(false);
		}
	}
	else if (vectorType == VectorType::V3)
	{
		if		(dataType == DataType::INT32)	GEN_V3_FILL(v3i32);
		else if	(dataType == DataType::UINT32)	GEN_V3_FILL(v3u32);
		else if	(dataType == DataType::INT64)	GEN_V3_FILL(v3i64);
		else if	(dataType == DataType::UINT64)	GEN_V3_FILL(v3u64);
		else if	(dataType == DataType::INT16)	GEN_V3_FILL(v3i16);
		else if	(dataType == DataType::UINT16)	GEN_V3_FILL(v3u16);
		else if	(dataType == DataType::INT8)	GEN_V3_FILL(v3i8);
		else if	(dataType == DataType::UINT8)	GEN_V3_FILL(v3u8);
		else if	(dataType == DataType::FLOAT32)	GEN_V3_FILL(v3f32);
		else if	(dataType == DataType::FLOAT64)	GEN_V3_FILL(v3f64);
		else if	(dataType == DataType::FLOAT16)	GEN_V3_FILL(v3f16);
		else
		{
			DE_ASSERT(false);
		}
	}
	else if (vectorType == VectorType::V4)
	{
		if		(dataType == DataType::INT32)	GEN_V4_FILL(v4i32);
		else if	(dataType == DataType::UINT32)	GEN_V4_FILL(v4u32);
		else if	(dataType == DataType::INT64)	GEN_V4_FILL(v4i64);
		else if	(dataType == DataType::UINT64)	GEN_V4_FILL(v4u64);
		else if	(dataType == DataType::INT16)	GEN_V4_FILL(v4i16);
		else if	(dataType == DataType::UINT16)	GEN_V4_FILL(v4u16);
		else if	(dataType == DataType::INT8)	GEN_V4_FILL(v4i8);
		else if	(dataType == DataType::UINT8)	GEN_V4_FILL(v4u8);
		else if	(dataType == DataType::FLOAT32)	GEN_V4_FILL(v4f32);
		else if	(dataType == DataType::FLOAT64)	GEN_V4_FILL(v4f64);
		else if	(dataType == DataType::FLOAT16)	GEN_V4_FILL(v4f16);
		else
		{
			DE_ASSERT(false);
		}
	}
	else if (vectorType == VectorType::A5)
	{
		if		(dataType == DataType::INT32)	GEN_A5_FILL(a5i32);
		else if	(dataType == DataType::UINT32)	GEN_A5_FILL(a5u32);
		else if	(dataType == DataType::INT64)	GEN_A5_FILL(a5i64);
		else if	(dataType == DataType::UINT64)	GEN_A5_FILL(a5u64);
		else if	(dataType == DataType::INT16)	GEN_A5_FILL(a5i16);
		else if	(dataType == DataType::UINT16)	GEN_A5_FILL(a5u16);
		else if	(dataType == DataType::INT8)	GEN_A5_FILL(a5i8);
		else if	(dataType == DataType::UINT8)	GEN_A5_FILL(a5u8);
		else if	(dataType == DataType::FLOAT32)	GEN_A5_FILL(a5f32);
		else if	(dataType == DataType::FLOAT64)	GEN_A5_FILL(a5f64);
		else if	(dataType == DataType::FLOAT16)	GEN_A5_FILL(a5f16);
		else
		{
			DE_ASSERT(false);
		}
	}
	else
	{
		DE_ASSERT(false);
	}
}

tcu::TestStatus DataSpillTestInstance::iterate (void)
{
	const auto& vki						= m_context.getInstanceInterface();
	const auto	physicalDevice			= m_context.getPhysicalDevice();
	const auto&	vkd						= m_context.getDeviceInterface();
	const auto	device					= m_context.getDevice();
	const auto	queue					= m_context.getUniversalQueue();
	const auto	familyIndex				= m_context.getUniversalQueueFamilyIndex();
	auto&		alloc					= m_context.getDefaultAllocator();
	const auto	shaderStages			= getShaderStages(m_params.callType);

	// Command buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, familyIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);

	// Callee, input and output buffers.
	const auto calleeBufferSize	= getElementSize(DataType::UINT32, VectorType::SCALAR);
	const auto outputBufferSize	= getElementSize(DataType::UINT32, VectorType::SCALAR);
	const auto inputBufferSize	= getElementSize(m_params.dataType, m_params.vectorType);

	const auto calleeBufferInfo	= makeBufferCreateInfo(calleeBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	const auto outputBufferInfo	= makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	const auto inputBufferInfo	= makeBufferCreateInfo(inputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	BufferWithMemory calleeBuffer	(vkd, device, alloc, calleeBufferInfo, MemoryRequirement::HostVisible);
	BufferWithMemory outputBuffer	(vkd, device, alloc, outputBufferInfo, MemoryRequirement::HostVisible);
	BufferWithMemory inputBuffer	(vkd, device, alloc, inputBufferInfo, MemoryRequirement::HostVisible);

	// Fill buffers with values.
	auto& calleeBufferAlloc	= calleeBuffer.getAllocation();
	auto* calleeBufferPtr	= calleeBufferAlloc.getHostPtr();
	auto& outputBufferAlloc	= outputBuffer.getAllocation();
	auto* outputBufferPtr	= outputBufferAlloc.getHostPtr();
	auto& inputBufferAlloc	= inputBuffer.getAllocation();
	auto* inputBufferPtr	= inputBufferAlloc.getHostPtr();

	deMemset(calleeBufferPtr, 0, static_cast<size_t>(calleeBufferSize));
	deMemset(outputBufferPtr, 0, static_cast<size_t>(outputBufferSize));

	if (samplersNeeded(m_params.dataType) || storageImageNeeded(m_params.dataType))
	{
		// The input buffer for these cases will be filled with zeros (sampling coordinates), and the input textures will contain the interesting input value.
		deMemset(inputBufferPtr, 0, static_cast<size_t>(inputBufferSize));
	}
	else
	{
		// We want to fill the input buffer with values that will be consistently used in the shader to obtain a result of zero.
		fillInputBuffer(m_params.dataType, m_params.vectorType, inputBufferPtr);
	}

	flushAlloc(vkd, device, calleeBufferAlloc);
	flushAlloc(vkd, device, outputBufferAlloc);
	flushAlloc(vkd, device, inputBufferAlloc);

	// Acceleration structures.
	de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure;
	de::MovePtr<TopLevelAccelerationStructure>		topLevelAccelerationStructure;

	bottomLevelAccelerationStructure = makeBottomLevelAccelerationStructure();
	bottomLevelAccelerationStructure->setDefaultGeometryData(getShaderStageForGeometry(m_params.callType), VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR);
	bottomLevelAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, alloc);

	topLevelAccelerationStructure = makeTopLevelAccelerationStructure();
	topLevelAccelerationStructure->setInstanceCount(1);
	topLevelAccelerationStructure->addInstance(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));
	topLevelAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, alloc);

	// Get some ray tracing properties.
	deUint32 shaderGroupHandleSize		= 0u;
	deUint32 shaderGroupBaseAlignment	= 1u;
	{
		const auto rayTracingPropertiesKHR	= makeRayTracingProperties(vki, physicalDevice);
		shaderGroupHandleSize				= rayTracingPropertiesKHR->getShaderGroupHandleSize();
		shaderGroupBaseAlignment			= rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
	}

	// Textures and samplers if needed.
	de::MovePtr<BufferWithMemory>				textureData;
	std::vector<de::MovePtr<ImageWithMemory>>	textures;
	std::vector<Move<VkImageView>>				textureViews;
	std::vector<Move<VkSampler>>				samplers;

	if (samplersNeeded(m_params.dataType) || storageImageNeeded(m_params.dataType))
	{
		// Create texture data with the expected contents.
		{
			const auto textureDataSize			= static_cast<VkDeviceSize>(sizeof(deUint32));
			const auto textureDataCreateInfo	= makeBufferCreateInfo(textureDataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

			textureData = de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, alloc, textureDataCreateInfo, MemoryRequirement::HostVisible));
			auto& textureDataAlloc = textureData->getAllocation();
			auto* textureDataPtr = textureDataAlloc.getHostPtr();

			fillInputBuffer(DataType::UINT32, VectorType::SCALAR, textureDataPtr);
			flushAlloc(vkd, device, textureDataAlloc);
		}

		// Images will be created like this with different usages.
		VkImageCreateInfo imageCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
			nullptr,								//	const void*				pNext;
			0u,										//	VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
			kImageFormat,							//	VkFormat				format;
			kImageExtent,							//	VkExtent3D				extent;
			1u,										//	deUint32				mipLevels;
			1u,										//	deUint32				arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
			kSampledImageUsage,						//	VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
			0u,										//	deUint32				queueFamilyIndexCount;
			nullptr,								//	const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
		};

		const auto imageSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
		const auto imageSubresourceLayers	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);

		if (samplersNeeded(m_params.dataType))
		{
			// All samplers will be created like this.
			const VkSamplerCreateInfo samplerCreateInfo =
			{
				VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,	//	VkStructureType			sType;
				nullptr,								//	const void*				pNext;
				0u,										//	VkSamplerCreateFlags	flags;
				VK_FILTER_NEAREST,						//	VkFilter				magFilter;
				VK_FILTER_NEAREST,						//	VkFilter				minFilter;
				VK_SAMPLER_MIPMAP_MODE_NEAREST,			//	VkSamplerMipmapMode		mipmapMode;
				VK_SAMPLER_ADDRESS_MODE_REPEAT,			//	VkSamplerAddressMode	addressModeU;
				VK_SAMPLER_ADDRESS_MODE_REPEAT,			//	VkSamplerAddressMode	addressModeV;
				VK_SAMPLER_ADDRESS_MODE_REPEAT,			//	VkSamplerAddressMode	addressModeW;
				0.0,									//	float					mipLodBias;
				VK_FALSE,								//	VkBool32				anisotropyEnable;
				1.0f,									//	float					maxAnisotropy;
				VK_FALSE,								//	VkBool32				compareEnable;
				VK_COMPARE_OP_ALWAYS,					//	VkCompareOp				compareOp;
				0.0f,									//	float					minLod;
				1.0f,									//	float					maxLod;
				VK_BORDER_COLOR_INT_OPAQUE_BLACK,		//	VkBorderColor			borderColor;
				VK_FALSE,								//	VkBool32				unnormalizedCoordinates;
			};

			// Create textures and samplers.
			for (size_t i = 0; i < kNumImages; ++i)
			{
				textures.emplace_back(new ImageWithMemory(vkd, device, alloc, imageCreateInfo, MemoryRequirement::Any));
				textureViews.emplace_back(makeImageView(vkd, device, textures.back()->get(), VK_IMAGE_VIEW_TYPE_2D, kImageFormat, imageSubresourceRange));
			}

			for (size_t i = 0; i < kNumSamplers; ++i)
				samplers.emplace_back(createSampler(vkd, device, &samplerCreateInfo));

			// Make sure texture data is available in the transfer stage.
			const auto textureDataBarrier = makeMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
			vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 1u, &textureDataBarrier, 0u, nullptr, 0u, nullptr);

			const auto bufferImageCopy = makeBufferImageCopy(kImageExtent, imageSubresourceLayers);

			// Fill textures with data and prepare them for the ray tracing pipeline stages.
			for (size_t i = 0; i < kNumImages; ++i)
			{
				const auto texturePreCopyBarrier	= makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, textures[i]->get(), imageSubresourceRange);
				const auto texturePostCopyBarrier	= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, textures[i]->get(), imageSubresourceRange);

				vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &texturePreCopyBarrier);
				vkd.cmdCopyBufferToImage(cmdBuffer, textureData->get(), textures[i]->get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &bufferImageCopy);
				vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0u, 0u, nullptr, 0u, nullptr, 1u, &texturePostCopyBarrier);
			}
		}
		else if (storageImageNeeded(m_params.dataType))
		{
			// Image will be used for storage.
			imageCreateInfo.usage = kStorageImageUsage;

			textures.emplace_back(new ImageWithMemory(vkd, device, alloc, imageCreateInfo, MemoryRequirement::Any));
			textureViews.emplace_back(makeImageView(vkd, device, textures.back()->get(), VK_IMAGE_VIEW_TYPE_2D, kImageFormat, imageSubresourceRange));

			// Make sure texture data is available in the transfer stage.
			const auto textureDataBarrier = makeMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
			vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 1u, &textureDataBarrier, 0u, nullptr, 0u, nullptr);

			const auto bufferImageCopy			= makeBufferImageCopy(kImageExtent, imageSubresourceLayers);
			const auto texturePreCopyBarrier	= makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, textures.back()->get(), imageSubresourceRange);
			const auto texturePostCopyBarrier	= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, textures.back()->get(), imageSubresourceRange);

			// Fill texture with data and prepare them for the ray tracing pipeline stages.
			vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &texturePreCopyBarrier);
			vkd.cmdCopyBufferToImage(cmdBuffer, textureData->get(), textures.back()->get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &bufferImageCopy);
			vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0u, 0u, nullptr, 0u, nullptr, 1u, &texturePostCopyBarrier);
		}
		else
		{
			DE_ASSERT(false);
		}
	}

	// Descriptor set layout.
	DescriptorSetLayoutBuilder dslBuilder;
	dslBuilder.addBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1u, shaderStages, nullptr);
	dslBuilder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, shaderStages, nullptr);	// Callee buffer.
	dslBuilder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, shaderStages, nullptr);	// Output buffer.
	dslBuilder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, shaderStages, nullptr);	// Input buffer.
	if (samplersNeeded(m_params.dataType))
	{
		dslBuilder.addBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2u, shaderStages, nullptr);
		dslBuilder.addBinding(VK_DESCRIPTOR_TYPE_SAMPLER, 2u, shaderStages, nullptr);
		dslBuilder.addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2u, shaderStages, nullptr);
	}
	else if (storageImageNeeded(m_params.dataType))
	{
		dslBuilder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u, shaderStages, nullptr);
	}
	const auto descriptorSetLayout = dslBuilder.build(vkd, device);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(vkd, device, descriptorSetLayout.get());

	// Descriptor pool and set.
	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3u);
	if (samplersNeeded(m_params.dataType))
	{
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2u);
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_SAMPLER, 2u);
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2u);
	}
	else if (storageImageNeeded(m_params.dataType))
	{
		poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1u);
	}
	const auto descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const auto descriptorSet = makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());

	// Update descriptor set.
	{
		const VkWriteDescriptorSetAccelerationStructureKHR writeASInfo =
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
			nullptr,
			1u,
			topLevelAccelerationStructure.get()->getPtr(),
		};

		DescriptorSetUpdateBuilder updateBuilder;

		const auto ds = descriptorSet.get();

		const auto calleeBufferDescriptorInfo	= makeDescriptorBufferInfo(calleeBuffer.get(), 0ull, VK_WHOLE_SIZE);
		const auto outputBufferDescriptorInfo	= makeDescriptorBufferInfo(outputBuffer.get(), 0ull, VK_WHOLE_SIZE);
		const auto inputBufferDescriptorInfo	= makeDescriptorBufferInfo(inputBuffer.get(), 0ull, VK_WHOLE_SIZE);

		updateBuilder.writeSingle(ds, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &writeASInfo);
		updateBuilder.writeSingle(ds, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &calleeBufferDescriptorInfo);
		updateBuilder.writeSingle(ds, DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputBufferDescriptorInfo);
		updateBuilder.writeSingle(ds, DescriptorSetUpdateBuilder::Location::binding(3u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &inputBufferDescriptorInfo);

		if (samplersNeeded(m_params.dataType))
		{
			// Update textures, samplers and combined image samplers.
			std::vector<VkDescriptorImageInfo> textureDescInfos;
			std::vector<VkDescriptorImageInfo> textureSamplerInfos;
			std::vector<VkDescriptorImageInfo> combinedSamplerInfos;

			for (size_t i = 0; i < kNumAloneImages; ++i)
				textureDescInfos.push_back(makeDescriptorImageInfo(DE_NULL, textureViews[i].get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
			for (size_t i = 0; i < kNumAloneSamplers; ++i)
				textureSamplerInfos.push_back(makeDescriptorImageInfo(samplers[i].get(), DE_NULL, VK_IMAGE_LAYOUT_UNDEFINED));

			for (size_t i = 0; i < kNumCombined; ++i)
				combinedSamplerInfos.push_back(makeDescriptorImageInfo(samplers[i + kNumAloneSamplers].get(), textureViews[i + kNumAloneImages].get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));

			updateBuilder.writeArray(ds, DescriptorSetUpdateBuilder::Location::binding(4u), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, kNumAloneImages, textureDescInfos.data());
			updateBuilder.writeArray(ds, DescriptorSetUpdateBuilder::Location::binding(5u), VK_DESCRIPTOR_TYPE_SAMPLER, kNumAloneSamplers, textureSamplerInfos.data());
			updateBuilder.writeArray(ds, DescriptorSetUpdateBuilder::Location::binding(6u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kNumCombined, combinedSamplerInfos.data());
		}
		else if (storageImageNeeded(m_params.dataType))
		{
			const auto storageImageDescriptorInfo = makeDescriptorImageInfo(DE_NULL, textureViews.back().get(), VK_IMAGE_LAYOUT_GENERAL);
			updateBuilder.writeSingle(ds, DescriptorSetUpdateBuilder::Location::binding(4u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &storageImageDescriptorInfo);
		}

		updateBuilder.update(vkd, device);
	}

	// Create raytracing pipeline and shader binding tables.
	Move<VkPipeline>				pipeline;

	de::MovePtr<BufferWithMemory>	raygenShaderBindingTable;
	de::MovePtr<BufferWithMemory>	missShaderBindingTable;
	de::MovePtr<BufferWithMemory>	hitShaderBindingTable;
	de::MovePtr<BufferWithMemory>	callableShaderBindingTable;

	VkStridedDeviceAddressRegionKHR	raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR	missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR	hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR	callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

	{
		const auto rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();
		const auto callType = m_params.callType;

		// Every case uses a ray generation shader.
		rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, createShaderModule(vkd, device, m_context.getBinaryCollection().get("rgen"), 0), 0);

		if (callType == CallType::TRACE_RAY)
		{
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, createShaderModule(vkd, device, m_context.getBinaryCollection().get("chit"), 0), 1);
		}
		else if (callType == CallType::EXECUTE_CALLABLE)
		{
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR, createShaderModule(vkd, device, m_context.getBinaryCollection().get("call"), 0), 1);
		}
		else if (callType == CallType::REPORT_INTERSECTION)
		{
			rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, createShaderModule(vkd, device, m_context.getBinaryCollection().get("rint"), 0), 1);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR, createShaderModule(vkd, device, m_context.getBinaryCollection().get("ahit"), 0), 1);
		}
		else
		{
			DE_ASSERT(false);
		}

		pipeline = rayTracingPipeline->createPipeline(vkd, device, pipelineLayout.get());

		raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
		raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);

		if (callType == CallType::EXECUTE_CALLABLE)
		{
			callableShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
			callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, callableShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
		}
		else if (callType == CallType::TRACE_RAY || callType == CallType::REPORT_INTERSECTION)
		{
			hitShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
			hitShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
		}
		else
		{
			DE_ASSERT(false);
		}
	}

	// Use ray tracing pipeline.
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdTraceRaysKHR(cmdBuffer, &raygenShaderBindingTableRegion, &missShaderBindingTableRegion, &hitShaderBindingTableRegion, &callableShaderBindingTableRegion, 1u, 1u, 1u);

	// Synchronize output and callee buffers.
	const auto memBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &memBarrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Verify output and callee buffers.
	invalidateAlloc(vkd, device, outputBufferAlloc);
	invalidateAlloc(vkd, device, calleeBufferAlloc);

	std::map<std::string, void*> bufferPtrs;
	bufferPtrs["output"] = outputBufferPtr;
	bufferPtrs["callee"] = calleeBufferPtr;

	for (const auto& ptr : bufferPtrs)
	{
		const auto& bufferName	= ptr.first;
		const auto& bufferPtr	= ptr.second;

		deUint32 outputVal;
		deMemcpy(&outputVal, bufferPtr, sizeof(outputVal));

		if (outputVal != 1u)
			return tcu::TestStatus::fail("Unexpected value found in " + bufferName + " buffer: " + de::toString(outputVal));
	}

	return tcu::TestStatus::pass("Pass");
}

enum class InterfaceType
{
	RAY_PAYLOAD = 0,
	CALLABLE_DATA,
	HIT_ATTRIBUTES,
	SHADER_RECORD_BUFFER_RGEN,
	SHADER_RECORD_BUFFER_CALL,
	SHADER_RECORD_BUFFER_MISS,
	SHADER_RECORD_BUFFER_HIT,
};

// Separate class to ease testing pipeline interface variables.
class DataSpillPipelineInterfaceTestCase : public vkt::TestCase
{
public:
	struct TestParams
	{
		InterfaceType	interfaceType;
	};

							DataSpillPipelineInterfaceTestCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& testParams);
	virtual					~DataSpillPipelineInterfaceTestCase		(void) {}

	virtual void			initPrograms							(vk::SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance							(Context& context) const;
	virtual void			checkSupport							(Context& context) const;

private:
	TestParams				m_params;
};

class DataSpillPipelineInterfaceTestInstance : public vkt::TestInstance
{
public:
	using TestParams = DataSpillPipelineInterfaceTestCase::TestParams;

						DataSpillPipelineInterfaceTestInstance	(Context& context, const TestParams& testParams);
						~DataSpillPipelineInterfaceTestInstance	(void) {}

	tcu::TestStatus		iterate									(void);

private:
	TestParams			m_params;
};

DataSpillPipelineInterfaceTestCase::DataSpillPipelineInterfaceTestCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& testParams)
	: vkt::TestCase	(testCtx, name, description)
	, m_params		(testParams)
{
}

TestInstance* DataSpillPipelineInterfaceTestCase::createInstance (Context& context) const
{
	return new DataSpillPipelineInterfaceTestInstance (context, m_params);
}

DataSpillPipelineInterfaceTestInstance::DataSpillPipelineInterfaceTestInstance (Context& context, const TestParams& testParams)
	: vkt::TestInstance	(context)
	, m_params			(testParams)
{
}

void DataSpillPipelineInterfaceTestCase::checkSupport (Context& context) const
{
	commonCheckSupport(context);
}

void DataSpillPipelineInterfaceTestCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions buildOptions (programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	const std::string glslHeader =
		"#version 460 core\n"
		"#extension GL_EXT_ray_tracing : require\n"
		;

	const std::string glslBindings =
		"layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;\n"
		"layout(set = 0, binding = 1) buffer StorageBlock { uint val[" + std::to_string(kNumStorageValues) + "]; } storageBuffer;\n"
		;

	if (m_params.interfaceType == InterfaceType::RAY_PAYLOAD)
	{
		// The closest hit shader will store 100 in the second array position.
		// The ray gen shader will store 103 in the first array position using the hitValue after the traceRayExt() call.

		std::ostringstream rgen;
		rgen
			<< glslHeader
			<< "layout(location = 0) rayPayloadEXT vec3 hitValue;\n"
			<< glslBindings
			<< "void main()\n"
			<< "{\n"
			<< "  hitValue = vec3(10.0, 30.0, 60.0);\n"
			<< "  traceRayEXT(topLevelAS, 0u, 0xFFu, 0, 0, 0, vec3(0.5, 0.5, 0.0), 0.0, vec3(0.0, 0.0, -1.0), 9.0, 0);\n"
			<< "  storageBuffer.val[0] = uint(hitValue.x + hitValue.y + hitValue.z);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(rgen.str())) << buildOptions;

		std::stringstream chit;
		chit
			<< glslHeader
			<< "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
			<< "hitAttributeEXT vec3 attribs;\n"
			<< glslBindings
			<< "void main()\n"
			<< "{\n"
			<< "  storageBuffer.val[1] = uint(hitValue.x + hitValue.y + hitValue.z);\n"
			<< "  hitValue = vec3(hitValue.x + 1.0, hitValue.y + 1.0, hitValue.z + 1.0);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(chit.str())) << buildOptions;
	}
	else if (m_params.interfaceType == InterfaceType::CALLABLE_DATA)
	{
		// The callable shader shader will store 100 in the second array position.
		// The ray gen shader will store 200 in the first array position using the callable data after the executeCallableEXT() call.

		std::ostringstream rgen;
		rgen
			<< glslHeader
			<< "layout(location = 0) callableDataEXT float callableData;\n"
			<< glslBindings
			<< "void main()\n"
			<< "{\n"
			<< "  callableData = 100.0;\n"
			<< "  executeCallableEXT(0, 0);\n"
			<< "  storageBuffer.val[0] = uint(callableData);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(rgen.str())) << buildOptions;

		std::ostringstream call;
		call
			<< glslHeader
			<< "layout(location = 0) callableDataInEXT float callableData;\n"
			<< glslBindings
			<< "void main()\n"
			<< "{\n"
			<< "    storageBuffer.val[1] = uint(callableData);\n"
			<< "    callableData = callableData * 2.0;\n"
			<< "}\n"
			;

		programCollection.glslSources.add("call") << glu::CallableSource(updateRayTracingGLSL(call.str())) << buildOptions;
	}
	else if (m_params.interfaceType == InterfaceType::HIT_ATTRIBUTES)
	{
		// The ray gen shader will store value 300 in the first storage buffer position.
		// The intersection shader will store value 315 in the second storage buffer position.
		// The closes hit shader will store value 330 in the third storage buffer position using the hit attributes.

		std::ostringstream rgen;
		rgen
			<< glslHeader
			<< "layout(location = 0) rayPayloadEXT vec3 hitValue;\n"
			<< glslBindings
			<< "void main()\n"
			<< "{\n"
			<< "  traceRayEXT(topLevelAS, 0u, 0xFFu, 0, 0, 0, vec3(0.5, 0.5, 0.0), 0.0, vec3(0.0, 0.0, -1.0), 9.0, 0);\n"
			<< "  storageBuffer.val[0] = 300u;\n"
			<< "}\n"
			;
		programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(rgen.str())) << buildOptions;

		std::stringstream rint;
		rint
			<< glslHeader
			<< "hitAttributeEXT vec3 attribs;\n"
			<< glslBindings
			<< "void main()\n"
			<< "{\n"
			<< "  attribs = vec3(140.0, 160.0, 30.0);\n"
			<< "  storageBuffer.val[1] = 315u;\n"
			<< "  reportIntersectionEXT(1.0f, 0);\n"
			<< "}\n"
			;

		programCollection.glslSources.add("rint") << glu::IntersectionSource(updateRayTracingGLSL(rint.str())) << buildOptions;

		std::stringstream chit;
		chit
			<< glslHeader
			<< "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
			<< "hitAttributeEXT vec3 attribs;\n"
			<< glslBindings
			<< "void main()\n"
			<< "{\n"
			<< "  storageBuffer.val[2] = uint(attribs.x + attribs.y + attribs.z);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(chit.str())) << buildOptions;

	}
	else if (m_params.interfaceType == InterfaceType::SHADER_RECORD_BUFFER_RGEN)
	{
		// The ray gen shader will have a uvec4 in the shader record buffer with contents 400, 401, 402, 403.
		// The shader will call a callable shader indicating a position in that vec4 (0, 1, 2, 3). For example, let's use position 1.
		// The callable shader will return the indicated position+1 modulo 4, so it will return 2 in our case.
		// *After* returning from the callable shader, the raygen shader will use that reply to access position 2 and write a 402 in the first output buffer position.
		// The callable shader will store 450 in the second output buffer position.

		std::ostringstream rgen;
		rgen
			<< glslHeader
			<< "layout(shaderRecordEXT) buffer ShaderRecordStruct {\n"
			<< "  uvec4 info;\n"
			<< "};\n"
			<< "layout(location = 0) callableDataEXT uint callableData;\n"
			<< glslBindings
			<< "void main()\n"
			<< "{\n"
			<< "  callableData = 1u;"
			<< "  executeCallableEXT(0, 0);\n"
			<< "  if      (callableData == 0u) storageBuffer.val[0] = info.x;\n"
			<< "  else if (callableData == 1u) storageBuffer.val[0] = info.y;\n"
			<< "  else if (callableData == 2u) storageBuffer.val[0] = info.z;\n"
			<< "  else if (callableData == 3u) storageBuffer.val[0] = info.w;\n"
			<< "}\n"
			;
		programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(rgen.str())) << buildOptions;

		std::ostringstream call;
		call
			<< glslHeader
			<< "layout(location = 0) callableDataInEXT uint callableData;\n"
			<< glslBindings
			<< "void main()\n"
			<< "{\n"
			<< "    storageBuffer.val[1] = 450u;\n"
			<< "    callableData = (callableData + 1u) % 4u;\n"
			<< "}\n"
			;

		programCollection.glslSources.add("call") << glu::CallableSource(updateRayTracingGLSL(call.str())) << buildOptions;
	}
	else if (m_params.interfaceType == InterfaceType::SHADER_RECORD_BUFFER_CALL)
	{
		// Similar to the previous case, with a twist:
		//   * rgen passes the vector position.
		//   * call increases that by one.
		//   * subcall increases again and does the modulo operation, also writing 450 in the third output buffer value.
		//   * call is the one accessing the vector at the returned position, writing 403 in this case to the second output buffer value.
		//   * call passes this value back doubled to rgen, which writes it to the first output buffer value (806).

		std::ostringstream rgen;
		rgen
			<< glslHeader
			<< "layout(location = 0) callableDataEXT uint callableData;\n"
			<< glslBindings
			<< "void main()\n"
			<< "{\n"
			<< "  callableData = 1u;\n"
			<< "  executeCallableEXT(0, 0);\n"
			<< "  storageBuffer.val[0] = callableData;\n"
			<< "}\n"
			;
		programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(rgen.str())) << buildOptions;

		std::ostringstream call;
		call
			<< glslHeader
			<< "layout(shaderRecordEXT) buffer ShaderRecordStruct {\n"
			<< "  uvec4 info;\n"
			<< "};\n"
			<< "layout(location = 0) callableDataInEXT uint callableDataIn;\n"
			<< "layout(location = 1) callableDataEXT uint callableDataOut;\n"
			<< glslBindings
			<< "void main()\n"
			<< "{\n"
			<< "  callableDataOut = callableDataIn + 1u;\n"
			<< "  executeCallableEXT(1, 1);\n"
			<< "  uint outputBufferValue = 777u;\n"
			<< "  if      (callableDataOut == 0u) outputBufferValue = info.x;\n"
			<< "  else if (callableDataOut == 1u) outputBufferValue = info.y;\n"
			<< "  else if (callableDataOut == 2u) outputBufferValue = info.z;\n"
			<< "  else if (callableDataOut == 3u) outputBufferValue = info.w;\n"
			<< "  storageBuffer.val[1] = outputBufferValue;\n"
			<< "  callableDataIn = outputBufferValue * 2u;\n"
			<< "}\n"
			;

		programCollection.glslSources.add("call") << glu::CallableSource(updateRayTracingGLSL(call.str())) << buildOptions;

		std::ostringstream subcall;
		subcall
			<< glslHeader
			<< "layout(location = 1) callableDataInEXT uint callableData;\n"
			<< glslBindings
			<< "void main()\n"
			<< "{\n"
			<< "  callableData = (callableData + 1u) % 4u;\n"
			<< "  storageBuffer.val[2] = 450u;\n"
			<< "}\n"
			;

		programCollection.glslSources.add("subcall") << glu::CallableSource(updateRayTracingGLSL(subcall.str())) << buildOptions;
	}
	else if (m_params.interfaceType == InterfaceType::SHADER_RECORD_BUFFER_MISS || m_params.interfaceType == InterfaceType::SHADER_RECORD_BUFFER_HIT)
	{
		// Similar to the previous one, but the intermediate call shader has been replaced with a miss or closest hit shader.
		// The rgen shader will communicate with the miss/chit shader using the ray payload instead of the callable data.
		// Also, the initial position will be 2, so it will wrap around in this case. The numbers will also change.

		std::ostringstream rgen;
		rgen
			<< glslHeader
			<< "layout(location = 0) rayPayloadEXT uint rayPayload;\n"
			<< glslBindings
			<< "void main()\n"
			<< "{\n"
			<< "  rayPayload = 2u;\n"
			<< "  traceRayEXT(topLevelAS, 0u, 0xFFu, 0, 0, 0, vec3(0.5, 0.5, 0.0), 0.0, vec3(0.0, 0.0, -1.0), 9.0, 0);\n"
			<< "  storageBuffer.val[0] = rayPayload;\n"
			<< "}\n"
			;
		programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(rgen.str())) << buildOptions;

		std::ostringstream chitOrMiss;
		chitOrMiss
			<< glslHeader
			<< "layout(shaderRecordEXT) buffer ShaderRecordStruct {\n"
			<< "  uvec4 info;\n"
			<< "};\n"
			<< "layout(location = 0) rayPayloadInEXT uint rayPayload;\n"
			<< "layout(location = 0) callableDataEXT uint callableData;\n"
			<< glslBindings
			<< "void main()\n"
			<< "{\n"
			<< "  callableData = rayPayload + 1u;\n"
			<< "  executeCallableEXT(0, 0);\n"
			<< "  uint outputBufferValue = 777u;\n"
			<< "  if      (callableData == 0u) outputBufferValue = info.x;\n"
			<< "  else if (callableData == 1u) outputBufferValue = info.y;\n"
			<< "  else if (callableData == 2u) outputBufferValue = info.z;\n"
			<< "  else if (callableData == 3u) outputBufferValue = info.w;\n"
			<< "  storageBuffer.val[1] = outputBufferValue;\n"
			<< "  rayPayload = outputBufferValue * 3u;\n"
			<< "}\n"
			;

		if (m_params.interfaceType == InterfaceType::SHADER_RECORD_BUFFER_MISS)
			programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(chitOrMiss.str())) << buildOptions;
		else if (m_params.interfaceType == InterfaceType::SHADER_RECORD_BUFFER_HIT)
			programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(chitOrMiss.str())) << buildOptions;
		else
			DE_ASSERT(false);

		std::ostringstream call;
		call
			<< glslHeader
			<< "layout(location = 0) callableDataInEXT uint callableData;\n"
			<< glslBindings
			<< "void main()\n"
			<< "{\n"
			<< "    storageBuffer.val[2] = 490u;\n"
			<< "    callableData = (callableData + 1u) % 4u;\n"
			<< "}\n"
			;

		programCollection.glslSources.add("call") << glu::CallableSource(updateRayTracingGLSL(call.str())) << buildOptions;
	}
	else
	{
		DE_ASSERT(false);
	}
}

VkShaderStageFlags getShaderStages (InterfaceType type_)
{
	VkShaderStageFlags flags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	switch (type_)
	{
	case InterfaceType::HIT_ATTRIBUTES:
		flags |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
		// fallthrough.
	case InterfaceType::RAY_PAYLOAD:
		flags |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		break;
	case InterfaceType::CALLABLE_DATA:
	case InterfaceType::SHADER_RECORD_BUFFER_RGEN:
	case InterfaceType::SHADER_RECORD_BUFFER_CALL:
		flags |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;
		break;
	case InterfaceType::SHADER_RECORD_BUFFER_MISS:
		flags |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;
		flags |= VK_SHADER_STAGE_MISS_BIT_KHR;
		break;
	case InterfaceType::SHADER_RECORD_BUFFER_HIT:
		flags |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;
		flags |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		break;
	default:
		DE_ASSERT(false);
		break;
	}

	return flags;
}

// Proper stage for generating default geometry.
VkShaderStageFlagBits getShaderStageForGeometry (InterfaceType type_)
{
	VkShaderStageFlagBits bits = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;

	switch (type_)
	{
	case InterfaceType::HIT_ATTRIBUTES:				bits = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;	break;
	case InterfaceType::RAY_PAYLOAD:				bits = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;		break;
	case InterfaceType::CALLABLE_DATA:				bits = VK_SHADER_STAGE_CALLABLE_BIT_KHR;		break;
	case InterfaceType::SHADER_RECORD_BUFFER_RGEN:	bits = VK_SHADER_STAGE_CALLABLE_BIT_KHR;		break;
	case InterfaceType::SHADER_RECORD_BUFFER_CALL:	bits = VK_SHADER_STAGE_CALLABLE_BIT_KHR;		break;
	case InterfaceType::SHADER_RECORD_BUFFER_MISS:	bits = VK_SHADER_STAGE_MISS_BIT_KHR;			break;
	case InterfaceType::SHADER_RECORD_BUFFER_HIT:	bits = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;		break;
	default: DE_ASSERT(false); break;
	}

	DE_ASSERT(bits != VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM);
	return bits;
}

void createSBTWithShaderRecord (const DeviceInterface& vkd, VkDevice device, vk::Allocator &alloc,
								VkPipeline pipeline, RayTracingPipeline* rayTracingPipeline,
								deUint32 shaderGroupHandleSize, deUint32 shaderGroupBaseAlignment,
								deUint32 firstGroup, deUint32 groupCount,
								de::MovePtr<BufferWithMemory>& shaderBindingTable,
								VkStridedDeviceAddressRegionKHR& shaderBindingTableRegion)
{
	const auto alignedSize		= de::roundUp(shaderGroupHandleSize + kShaderRecordSize, shaderGroupHandleSize);
	shaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, firstGroup, groupCount, 0u, 0u, MemoryRequirement::Any, 0u, 0u, kShaderRecordSize);
	shaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, shaderBindingTable->get(), 0), alignedSize, groupCount * alignedSize);

	// Fill shader record buffer data.
	// Note we will only fill the first shader record after the handle.
	const tcu::UVec4	shaderRecordData	(400u, 401u, 402u, 403u);
	auto&				sbtAlloc			= shaderBindingTable->getAllocation();
	auto*				dataPtr				= reinterpret_cast<deUint8*>(sbtAlloc.getHostPtr()) + shaderGroupHandleSize;

	DE_STATIC_ASSERT(sizeof(shaderRecordData) == static_cast<size_t>(kShaderRecordSize));
	deMemcpy(dataPtr, &shaderRecordData, sizeof(shaderRecordData));
}

tcu::TestStatus DataSpillPipelineInterfaceTestInstance::iterate (void)
{
	const auto& vki						= m_context.getInstanceInterface();
	const auto	physicalDevice			= m_context.getPhysicalDevice();
	const auto&	vkd						= m_context.getDeviceInterface();
	const auto	device					= m_context.getDevice();
	const auto	queue					= m_context.getUniversalQueue();
	const auto	familyIndex				= m_context.getUniversalQueueFamilyIndex();
	auto&		alloc					= m_context.getDefaultAllocator();
	const auto	shaderStages			= getShaderStages(m_params.interfaceType);

	// Command buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, familyIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);

	// Storage buffer.
	std::array<deUint32, kNumStorageValues>	storageBufferData;
	const auto								storageBufferSize	= de::dataSize(storageBufferData);
	const auto								storagebufferInfo	= makeBufferCreateInfo(storageBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory						storageBuffer		(vkd, device, alloc, storagebufferInfo, MemoryRequirement::HostVisible);

	// Zero-out buffer.
	auto& storageBufferAlloc	= storageBuffer.getAllocation();
	auto* storageBufferPtr		= storageBufferAlloc.getHostPtr();
	deMemset(storageBufferPtr, 0, storageBufferSize);
	flushAlloc(vkd, device, storageBufferAlloc);

	// Acceleration structures.
	de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure;
	de::MovePtr<TopLevelAccelerationStructure>		topLevelAccelerationStructure;

	bottomLevelAccelerationStructure = makeBottomLevelAccelerationStructure();
	bottomLevelAccelerationStructure->setDefaultGeometryData(getShaderStageForGeometry(m_params.interfaceType), VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR);
	bottomLevelAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, alloc);

	topLevelAccelerationStructure = makeTopLevelAccelerationStructure();
	topLevelAccelerationStructure->setInstanceCount(1);
	topLevelAccelerationStructure->addInstance(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));
	topLevelAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, alloc);

	// Get some ray tracing properties.
	deUint32 shaderGroupHandleSize		= 0u;
	deUint32 shaderGroupBaseAlignment	= 1u;
	{
		const auto rayTracingPropertiesKHR	= makeRayTracingProperties(vki, physicalDevice);
		shaderGroupHandleSize				= rayTracingPropertiesKHR->getShaderGroupHandleSize();
		shaderGroupBaseAlignment			= rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
	}

	// Descriptor set layout.
	DescriptorSetLayoutBuilder dslBuilder;
	dslBuilder.addBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1u, shaderStages, nullptr);
	dslBuilder.addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u, shaderStages, nullptr);	// Callee buffer.
	const auto descriptorSetLayout = dslBuilder.build(vkd, device);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(vkd, device, descriptorSetLayout.get());

	// Descriptor pool and set.
	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	const auto descriptorPool	= poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const auto descriptorSet	= makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());

	// Update descriptor set.
	{
		const VkWriteDescriptorSetAccelerationStructureKHR writeASInfo =
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
			nullptr,
			1u,
			topLevelAccelerationStructure.get()->getPtr(),
		};

		const auto	ds							= descriptorSet.get();
		const auto	storageBufferDescriptorInfo	= makeDescriptorBufferInfo(storageBuffer.get(), 0ull, VK_WHOLE_SIZE);

		DescriptorSetUpdateBuilder updateBuilder;
		updateBuilder.writeSingle(ds, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &writeASInfo);
		updateBuilder.writeSingle(ds, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &storageBufferDescriptorInfo);
		updateBuilder.update(vkd, device);
	}

	// Create raytracing pipeline and shader binding tables.
	const auto						interfaceType	= m_params.interfaceType;
	Move<VkPipeline>				pipeline;

	de::MovePtr<BufferWithMemory>	raygenShaderBindingTable;
	de::MovePtr<BufferWithMemory>	missShaderBindingTable;
	de::MovePtr<BufferWithMemory>	hitShaderBindingTable;
	de::MovePtr<BufferWithMemory>	callableShaderBindingTable;

	VkStridedDeviceAddressRegionKHR	raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR	missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR	hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR	callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

	{
		const auto rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

		// Every case uses a ray generation shader.
		rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, createShaderModule(vkd, device, m_context.getBinaryCollection().get("rgen"), 0), 0);

		if (interfaceType == InterfaceType::RAY_PAYLOAD)
		{
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, createShaderModule(vkd, device, m_context.getBinaryCollection().get("chit"), 0), 1);
		}
		else if (interfaceType == InterfaceType::CALLABLE_DATA || interfaceType == InterfaceType::SHADER_RECORD_BUFFER_RGEN)
		{
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR, createShaderModule(vkd, device, m_context.getBinaryCollection().get("call"), 0), 1);
		}
		else if (interfaceType == InterfaceType::HIT_ATTRIBUTES)
		{
			rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, createShaderModule(vkd, device, m_context.getBinaryCollection().get("rint"), 0), 1);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, createShaderModule(vkd, device, m_context.getBinaryCollection().get("chit"), 0), 1);
		}
		else if (interfaceType == InterfaceType::SHADER_RECORD_BUFFER_CALL)
		{
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR, createShaderModule(vkd, device, m_context.getBinaryCollection().get("call"), 0), 1);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR, createShaderModule(vkd, device, m_context.getBinaryCollection().get("subcall"), 0), 2);
		}
		else if (interfaceType == InterfaceType::SHADER_RECORD_BUFFER_MISS)
		{
			rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, createShaderModule(vkd, device, m_context.getBinaryCollection().get("miss"), 0), 1);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR, createShaderModule(vkd, device, m_context.getBinaryCollection().get("call"), 0), 2);
		}
		else if (interfaceType == InterfaceType::SHADER_RECORD_BUFFER_HIT)
		{
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, createShaderModule(vkd, device, m_context.getBinaryCollection().get("chit"), 0), 1);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR, createShaderModule(vkd, device, m_context.getBinaryCollection().get("call"), 0), 2);
		}
		else
		{
			DE_ASSERT(false);
		}

		pipeline = rayTracingPipeline->createPipeline(vkd, device, pipelineLayout.get());

		if (interfaceType == InterfaceType::SHADER_RECORD_BUFFER_RGEN)
		{
			createSBTWithShaderRecord (vkd, device, alloc, pipeline.get(), rayTracingPipeline.get(), shaderGroupHandleSize, shaderGroupBaseAlignment,
									   0u, 1u, raygenShaderBindingTable, raygenShaderBindingTableRegion);
		}
		else
		{
			raygenShaderBindingTable		= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
			raygenShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
		}


		if (interfaceType == InterfaceType::CALLABLE_DATA || interfaceType == InterfaceType::SHADER_RECORD_BUFFER_RGEN)
		{
			callableShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
			callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, callableShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
		}
		else if (interfaceType == InterfaceType::RAY_PAYLOAD || interfaceType == InterfaceType::HIT_ATTRIBUTES)
		{
			hitShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
			hitShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
		}
		else if (interfaceType == InterfaceType::SHADER_RECORD_BUFFER_CALL)
		{
			createSBTWithShaderRecord (vkd, device, alloc, pipeline.get(), rayTracingPipeline.get(), shaderGroupHandleSize, shaderGroupBaseAlignment,
									   1u, 2u, callableShaderBindingTable, callableShaderBindingTableRegion);
		}
		else if (interfaceType == InterfaceType::SHADER_RECORD_BUFFER_MISS)
		{
			createSBTWithShaderRecord (vkd, device, alloc, pipeline.get(), rayTracingPipeline.get(), shaderGroupHandleSize, shaderGroupBaseAlignment,
									   1u, 1u, missShaderBindingTable, missShaderBindingTableRegion);

			// Callable shader table.
			callableShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, 1);
			callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, callableShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
		}
		else if (interfaceType == InterfaceType::SHADER_RECORD_BUFFER_HIT)
		{
			createSBTWithShaderRecord (vkd, device, alloc, pipeline.get(), rayTracingPipeline.get(), shaderGroupHandleSize, shaderGroupBaseAlignment,
									   1u, 1u, hitShaderBindingTable, hitShaderBindingTableRegion);

			// Callable shader table.
			callableShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, 1);
			callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, callableShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
		}
		else
		{
			DE_ASSERT(false);
		}
	}

	// Use ray tracing pipeline.
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdTraceRaysKHR(cmdBuffer, &raygenShaderBindingTableRegion, &missShaderBindingTableRegion, &hitShaderBindingTableRegion, &callableShaderBindingTableRegion, 1u, 1u, 1u);

	// Synchronize output and callee buffers.
	const auto memBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &memBarrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Verify storage buffer.
	invalidateAlloc(vkd, device, storageBufferAlloc);
	deMemcpy(storageBufferData.data(), storageBufferPtr, storageBufferSize);

	// These values must match what the shaders store.
	std::vector<deUint32> expectedData;
	if (interfaceType == InterfaceType::RAY_PAYLOAD)
	{
		expectedData.push_back(103u);
		expectedData.push_back(100u);
	}
	else if (interfaceType == InterfaceType::CALLABLE_DATA)
	{
		expectedData.push_back(200u);
		expectedData.push_back(100u);
	}
	else if (interfaceType == InterfaceType::HIT_ATTRIBUTES)
	{
		expectedData.push_back(300u);
		expectedData.push_back(315u);
		expectedData.push_back(330u);
	}
	else if (interfaceType == InterfaceType::SHADER_RECORD_BUFFER_RGEN)
	{
		expectedData.push_back(402u);
		expectedData.push_back(450u);
	}
	else if (interfaceType == InterfaceType::SHADER_RECORD_BUFFER_CALL)
	{
		expectedData.push_back(806u);
		expectedData.push_back(403u);
		expectedData.push_back(450u);
	}
	else if (interfaceType == InterfaceType::SHADER_RECORD_BUFFER_MISS || interfaceType == InterfaceType::SHADER_RECORD_BUFFER_HIT)
	{
		expectedData.push_back(1200u);
		expectedData.push_back( 400u);
		expectedData.push_back( 490u);
	}
	else
	{
		DE_ASSERT(false);
	}

	size_t pos;
	for (pos = 0u; pos < expectedData.size(); ++pos)
	{
		const auto& stored		= storageBufferData.at(pos);
		const auto& expected	= expectedData.at(pos);
		if (stored != expected)
		{
			std::ostringstream msg;
			msg << "Unexpected output value found at position " << pos << " (expected " << expected << " but got " << stored << ")";
			return tcu::TestStatus::fail(msg.str());
		}
	}

	// Expect zeros in unused positions, as filled on the host.
	for (; pos < storageBufferData.size(); ++pos)
	{
		const auto& stored = storageBufferData.at(pos);
		if (stored != 0u)
		{
			std::ostringstream msg;
			msg << "Unexpected output value found at position " << pos << " (expected 0 but got " << stored << ")";
			return tcu::TestStatus::fail(msg.str());
		}
	}

	return tcu::TestStatus::pass("Pass");
}

} // anonymous namespace

tcu::TestCaseGroup*	createDataSpillTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "data_spill", "Ray tracing tests for data spilling and unspilling around shader calls"));

	struct
	{
		CallType callType;
		const char* name;
	} callTypes[] =
	{
		{ CallType::EXECUTE_CALLABLE,		"execute_callable"		},
		{ CallType::TRACE_RAY,				"trace_ray"				},
		{ CallType::REPORT_INTERSECTION,	"report_intersection"	},
	};

	struct
	{
		DataType dataType;
		const char* name;
	} dataTypes[] =
	{
		{ DataType::INT32,				"int32"			},
		{ DataType::UINT32,				"uint32"		},
		{ DataType::INT64,				"int64"			},
		{ DataType::UINT64,				"uint64"		},
		{ DataType::INT16,				"int16"			},
		{ DataType::UINT16,				"uint16"		},
		{ DataType::INT8,				"int8"			},
		{ DataType::UINT8,				"uint8"			},
		{ DataType::FLOAT32,			"float32"		},
		{ DataType::FLOAT64,			"float64"		},
		{ DataType::FLOAT16,			"float16"		},
		{ DataType::STRUCT,				"struct"		},
		{ DataType::SAMPLER,			"sampler"		},
		{ DataType::IMAGE,				"image"			},
		{ DataType::SAMPLED_IMAGE,		"combined"		},
		{ DataType::PTR_IMAGE,			"ptr_image"		},
		{ DataType::PTR_SAMPLER,		"ptr_sampler"	},
		{ DataType::PTR_SAMPLED_IMAGE,	"ptr_combined"	},
		{ DataType::PTR_TEXEL,			"ptr_texel"		},
		{ DataType::OP_NULL,			"op_null"		},
		{ DataType::OP_UNDEF,			"op_undef"		},
	};

	struct
	{
		VectorType vectorType;
		const char* prefix;
	} vectorTypes[] =
	{
		{ VectorType::SCALAR,	""		},
		{ VectorType::V2,		"v2"	},
		{ VectorType::V3,		"v3"	},
		{ VectorType::V4,		"v4"	},
		{ VectorType::A5,		"a5"	},
	};

	for (int callTypeIdx = 0; callTypeIdx < DE_LENGTH_OF_ARRAY(callTypes); ++callTypeIdx)
	{
		const auto& entryCallTypes = callTypes[callTypeIdx];

		de::MovePtr<tcu::TestCaseGroup> callTypeGroup(new tcu::TestCaseGroup(testCtx, entryCallTypes.name, ""));
		for (int dataTypeIdx = 0; dataTypeIdx < DE_LENGTH_OF_ARRAY(dataTypes); ++dataTypeIdx)
		{
			const auto& entryDataTypes = dataTypes[dataTypeIdx];

			for (int vectorTypeIdx = 0; vectorTypeIdx < DE_LENGTH_OF_ARRAY(vectorTypes); ++vectorTypeIdx)
			{
				const auto& entryVectorTypes = vectorTypes[vectorTypeIdx];

				if ((samplersNeeded(entryDataTypes.dataType)
					 || storageImageNeeded(entryDataTypes.dataType)
					 || entryDataTypes.dataType == DataType::STRUCT
					 || entryDataTypes.dataType == DataType::OP_NULL
					 || entryDataTypes.dataType == DataType::OP_UNDEF)
					&& entryVectorTypes.vectorType != VectorType::SCALAR)
				{
					continue;
				}

				DataSpillTestCase::TestParams params;
				params.callType		= entryCallTypes.callType;
				params.dataType		= entryDataTypes.dataType;
				params.vectorType	= entryVectorTypes.vectorType;

				const auto testName = std::string(entryVectorTypes.prefix) + entryDataTypes.name;

				callTypeGroup->addChild(new DataSpillTestCase(testCtx, testName, "", params));
			}
		}

		group->addChild(callTypeGroup.release());
	}

	// Pipeline interface tests.
	de::MovePtr<tcu::TestCaseGroup> pipelineInterfaceGroup(new tcu::TestCaseGroup(testCtx, "pipeline_interface", "Test data spilling and unspilling of pipeline interface variables"));

	struct
	{
		InterfaceType	interfaceType;
		const char*		name;
	} interfaceTypes[] =
	{
		{ InterfaceType::RAY_PAYLOAD,				"ray_payload"				},
		{ InterfaceType::CALLABLE_DATA,				"callable_data"				},
		{ InterfaceType::HIT_ATTRIBUTES,			"hit_attributes"			},
		{ InterfaceType::SHADER_RECORD_BUFFER_RGEN,	"shader_record_buffer_rgen"	},
		{ InterfaceType::SHADER_RECORD_BUFFER_CALL,	"shader_record_buffer_call"	},
		{ InterfaceType::SHADER_RECORD_BUFFER_MISS,	"shader_record_buffer_miss"	},
		{ InterfaceType::SHADER_RECORD_BUFFER_HIT,	"shader_record_buffer_hit"	},
	};

	for (int idx = 0; idx < DE_LENGTH_OF_ARRAY(interfaceTypes); ++idx)
	{
		const auto&										entry	= interfaceTypes[idx];
		DataSpillPipelineInterfaceTestCase::TestParams	params;

		params.interfaceType = entry.interfaceType;

		pipelineInterfaceGroup->addChild(new DataSpillPipelineInterfaceTestCase(testCtx, entry.name, "", params));
	}

	group->addChild(pipelineInterfaceGroup.release());

	return group.release();
}

} // RayTracing
} // vkt

