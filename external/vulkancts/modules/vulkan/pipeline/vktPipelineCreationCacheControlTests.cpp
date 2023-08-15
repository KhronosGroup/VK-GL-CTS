/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2020 Advanced Micro Devices, Inc.
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
 */
/*!
 * \file
 * \brief Pipeline Cache Tests
 */
/*--------------------------------------------------------------------*/

#include "vktPipelineCreationCacheControlTests.hpp"

#include "deRandom.hpp"
#include "deUniquePtr.hpp"
#include "tcuStringTemplate.hpp"
#include "vkDeviceUtil.hpp"
#include "vkRefUtil.hpp"
#include "vktConstexprVectorUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"

#include <chrono>
#include <random>
#include <string>
#include <vector>

namespace vkt
{
namespace pipeline
{
namespace
{
using namespace vk;

using tcu::StringTemplate;
using tcu::TestCaseGroup;
using tcu::TestContext;
using tcu::TestStatus;

using ::std::array;
using ::std::string;
using ::std::vector;

/*--------------------------------------------------------------------*//*!
 * Elements common to all test types
 *//*--------------------------------------------------------------------*/
namespace test_common
{
using ::std::chrono::high_resolution_clock;
using ::std::chrono::microseconds;

using duration			 = high_resolution_clock::duration;
using UniquePipeline	 = Move<VkPipeline>;
using UniqueShaderModule = Move<VkShaderModule>;

/*--------------------------------------------------------------------*//*!
 * \brief Paired Vulkan API result with elapsed duration
 *//*--------------------------------------------------------------------*/
struct TimedResult
{
	VkResult result;
	duration elapsed;
};

/*--------------------------------------------------------------------*//*!
 * \brief Validation function type output from vkCreate*Pipelines()
 *
 * \param result - VkResult returned from API call
 * \param pipeliens - vector of pipelines created
 * \param elapsed - high_resolution_clock::duration of time elapsed in API
 * \param reason - output string to give the reason for failure
 *
 * \return QP_TEST_RESULT_PASS on success QP_TEST_RESULT_FAIL otherwise
 *//*--------------------------------------------------------------------*/
using Validator = qpTestResult (*)(VkResult, const vector<UniquePipeline>&, duration, string&);

static constexpr size_t VALIDATOR_ARRAY_MAX = 4;
using ValidatorArray						= ConstexprVector<Validator, VALIDATOR_ARRAY_MAX>;

/*--------------------------------------------------------------------*//*!
 * \brief Run a loop of validation tests and return the result
 *//*--------------------------------------------------------------------*/
template <typename pipelines_t, qpTestResult FAIL_RESULT = QP_TEST_RESULT_FAIL>
TestStatus validateResults(VkResult				 result,
						   const pipelines_t&	 pipelines,
						   duration				 elapsed,
						   const ValidatorArray& validators)
{
	using de::contains;
	static constexpr VkResult ALLOWED_RESULTS[] = {VK_SUCCESS, VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT};

	string reason;

	if (contains(DE_ARRAY_BEGIN(ALLOWED_RESULTS), DE_ARRAY_END(ALLOWED_RESULTS), result) == DE_FALSE)
	{
		static const StringTemplate ERROR_MSG = {"Pipeline creation returned an error result: ${0}"};
		TCU_THROW(InternalError, ERROR_MSG.format(result).c_str());
	}

	for (const auto& validator : validators)
	{
		const auto qpResult = validator(result, pipelines, elapsed, reason);
		if (qpResult != QP_TEST_RESULT_PASS)
		{
			return {qpResult, reason};
		}
	}

	return TestStatus::pass("Test passed.");
}

/*--------------------------------------------------------------------*//*!
 * \brief Generate an error if result does not match VK_RESULT
 *//*--------------------------------------------------------------------*/
template <VkResult VK_RESULT, qpTestResult FAIL_RESULT = QP_TEST_RESULT_FAIL>
qpTestResult checkResult(VkResult result, const vector<UniquePipeline>&, duration, string& reason)
{
	if (VK_RESULT != result)
	{
		static const StringTemplate ERROR_MSG = {"Got ${0}, Expected ${1}"};
		reason								  = ERROR_MSG.format(result, VK_RESULT);
		return FAIL_RESULT;
	}

	return QP_TEST_RESULT_PASS;
}

/*--------------------------------------------------------------------*//*!
 * \brief Generate an error if pipeline[INDEX] is not valid
 *//*--------------------------------------------------------------------*/
template <size_t INDEX, qpTestResult FAIL_RESULT = QP_TEST_RESULT_FAIL>
qpTestResult checkPipelineMustBeValid(VkResult, const vector<UniquePipeline>& pipelines, duration, string& reason)
{
	if (pipelines.size() <= INDEX)
	{
		static const StringTemplate ERROR_MSG = {"Index ${0} is not in created pipelines (pipelines.size(): ${1})"};
		TCU_THROW(TestError, ERROR_MSG.format(INDEX, pipelines.size()));
	}

	if (*pipelines[INDEX] == VK_NULL_HANDLE)
	{
		static const StringTemplate ERROR_MSG = {"pipelines[${0}] is not a valid VkPipeline object"};
		reason								  = ERROR_MSG.format(INDEX);
		return FAIL_RESULT;
	}

	return QP_TEST_RESULT_PASS;
}

/*--------------------------------------------------------------------*//*!
 * \brief Generate an error if pipeline[INDEX] is not VK_NULL_HANDLE
 *//*--------------------------------------------------------------------*/
template <size_t INDEX, qpTestResult FAIL_RESULT = QP_TEST_RESULT_FAIL>
qpTestResult checkPipelineMustBeNull(VkResult, const vector<UniquePipeline>& pipelines, duration, string& reason)
{
	if (pipelines.size() <= INDEX)
	{
		static const StringTemplate ERROR_MSG = {"Index ${0} is not in created pipelines (pipelines.size(): ${1})"};
		TCU_THROW(TestError, ERROR_MSG.format(INDEX, pipelines.size()));
	}

	if (*pipelines[INDEX] != VK_NULL_HANDLE)
	{
		static const StringTemplate ERROR_MSG = {"pipelines[${0}] is not VK_NULL_HANDLE"};
		reason								  = ERROR_MSG.format(INDEX);
		return FAIL_RESULT;
	}

	return QP_TEST_RESULT_PASS;
}

/*--------------------------------------------------------------------*//*!
 * \brief Generate an error if any pipeline is valid after an early-return failure
 *//*--------------------------------------------------------------------*/
template <size_t INDEX, qpTestResult FAIL_RESULT = QP_TEST_RESULT_FAIL>
qpTestResult checkPipelineNullAfterIndex(VkResult, const vector<UniquePipeline>& pipelines, duration, string& reason)
{
	if (pipelines.size() <= INDEX)
	{
		static const StringTemplate ERROR_MSG = {"Index ${0} is not in created pipelines (pipelines.size(): ${1})"};
		TCU_THROW(TestError, ERROR_MSG.format(INDEX, pipelines.size()));
	}

	if (pipelines.size() - 1 == INDEX)
	{
		static const StringTemplate ERROR_MSG = {"Index ${0} is the last pipeline, likely a malformed test case"};
		TCU_THROW(TestError, ERROR_MSG.format(INDEX));
	}

	// Only have to iterate through if the requested index is null
	if (*pipelines[INDEX] == VK_NULL_HANDLE)
	{
		for (size_t i = INDEX + 1; i < pipelines.size(); ++i)
		{
			if (*pipelines[i] != VK_NULL_HANDLE)
			{
				static const StringTemplate ERROR_MSG = {
					"pipelines[${0}] is not VK_NULL_HANDLE after a explicit early return index"};
				reason = ERROR_MSG.format(i);
				return FAIL_RESULT;
			}
		}
	}

	return QP_TEST_RESULT_PASS;
}

/*--------------------------------------------------------------------*//*!
 * Time limit constants
 *//*--------------------------------------------------------------------*/
enum ElapsedTime
{
	ELAPSED_TIME_INFINITE  = microseconds{-1}.count(),
	ELAPSED_TIME_IMMEDIATE = microseconds{500}.count(),
	ELAPSED_TIME_FAST	   = microseconds{1000}.count()
};

/*--------------------------------------------------------------------*//*!
 * \brief Generate an error if elapsed time exceeds MAX_TIME
 *//*--------------------------------------------------------------------*/
template <ElapsedTime MAX_TIME, qpTestResult FAIL_RESULT = QP_TEST_RESULT_FAIL>
qpTestResult checkElapsedTime(VkResult, const vector<UniquePipeline>&, duration elapsed, string& reason)
{
#if defined(DE_DEBUG)
	DE_UNREF(elapsed);
	DE_UNREF(reason);

	// In debug mode timing is not likely to be accurate
	return QP_TEST_RESULT_PASS;
#else

	using ::std::chrono::duration_cast;

	static constexpr microseconds ALLOWED_TIME = microseconds{MAX_TIME};

	if (elapsed > ALLOWED_TIME)
	{
		static const StringTemplate ERROR_MSG = {"pipeline creation took longer than ${0}us (actual time: ${1}us)"};
		reason = ERROR_MSG.format(ALLOWED_TIME.count(), duration_cast<microseconds>(elapsed).count());
		return FAIL_RESULT;
	}

	return QP_TEST_RESULT_PASS;
#endif
}

/*--------------------------------------------------------------------*//*!
 * \brief Test case parameters
 *//*--------------------------------------------------------------------*/
struct TestParams
{
	enum CacheType
	{
		NO_CACHE = 0,
		EXPLICIT_CACHE,
		DERIVATIVE_HANDLE,
		DERIVATIVE_INDEX
	};

	struct Iteration
	{
		static constexpr size_t MAX_VARIANTS = 4;
		using Variant						 = VkPipelineCreateFlags;
		using VariantArray					 = ConstexprVector<Variant, MAX_VARIANTS>;

		static constexpr Variant NORMAL		  = 0;
		static constexpr Variant NO_COMPILE	  = VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_EXT;
		static constexpr Variant EARLY_RETURN = NO_COMPILE | VK_PIPELINE_CREATE_EARLY_RETURN_ON_FAILURE_BIT_EXT;

		static constexpr VariantArray SINGLE_NORMAL						= VariantArray{NORMAL};
		static constexpr VariantArray SINGLE_NOCOMPILE					= VariantArray{NO_COMPILE};
		static constexpr VariantArray BATCH_NOCOMPILE_COMPILE_NOCOMPILE = VariantArray{NO_COMPILE, NORMAL, NO_COMPILE};
		static constexpr VariantArray BATCH_RETURN_COMPILE_NOCOMPILE = VariantArray{EARLY_RETURN, NORMAL, NO_COMPILE};

		inline constexpr Iteration() : variants{}, validators{} {}
		inline constexpr Iteration(const VariantArray& v, const ValidatorArray& f) : variants{v}, validators{f} {}

		VariantArray   variants;
		ValidatorArray validators;
	};

	static constexpr size_t MAX_ITERATIONS = 4;
	using IterationArray				   = ConstexprVector<Iteration, MAX_ITERATIONS>;

	const char*		name;
	const char*		description;
	CacheType		cacheType;
	IterationArray	iterations;
	bool			useMaintenance5;
};

/*--------------------------------------------------------------------*//*!
 * \brief Verify extension and feature support
 *//*--------------------------------------------------------------------*/
void checkSupport(Context& context, const TestParams& params)
{
	static constexpr char EXT_NAME[] = "VK_EXT_pipeline_creation_cache_control";
	if (!context.requireDeviceFunctionality(EXT_NAME))
	{
		TCU_THROW(NotSupportedError, "Extension 'VK_EXT_pipeline_creation_cache_control' is not supported");
	}

	const auto features = context.getPipelineCreationCacheControlFeatures();
	if (features.pipelineCreationCacheControl == DE_FALSE)
	{
		TCU_THROW(NotSupportedError, "Feature 'pipelineCreationCacheControl' is not enabled");
	}

	if (params.useMaintenance5)
		context.requireDeviceFunctionality("VK_KHR_maintenance5");
}

/*--------------------------------------------------------------------*//*!
 * \brief Generate a random floating point number as a string
 *//*--------------------------------------------------------------------*/
float randomFloat()
{
#if !defined(DE_DEBUG)
	static de::Random state = {::std::random_device{}()};
#else
	static de::Random state = {0xDEADBEEF};
#endif

	return state.getFloat();
}

/*--------------------------------------------------------------------*//*!
 * \brief Get a string of VkResults from a vector
 *//*--------------------------------------------------------------------*/
string getResultsString(const vector<VkResult>& results)
{
	using ::std::ostringstream;

	ostringstream output;

	output << "results[" << results.size() << "]={ ";

	if (!results.empty())
	{
		output << results[0];
	}

	for (size_t i = 1; i < results.size(); ++i)
	{
		output << ", " << results[i];
	}

	output << " }";

	return output.str();
}

/*--------------------------------------------------------------------*//*!
 * \brief Cast a pointer to an the expected SPIRV type
 *//*--------------------------------------------------------------------*/
template <typename _t>
inline const deUint32* shader_cast(const _t* ptr)
{
	return reinterpret_cast<const deUint32*>(ptr);
}

/*--------------------------------------------------------------------*//*!
 * \brief Capture a container of Vulkan handles into Move<> types
 *//*--------------------------------------------------------------------*/
template <typename input_container_t,
		  typename handle_t	 = typename input_container_t::value_type,
		  typename move_t	 = Move<handle_t>,
		  typename deleter_t = Deleter<handle_t>,
		  typename output_t	 = vector<move_t>>
output_t wrapHandles(const DeviceInterface&		  vk,
					 VkDevice					  device,
					 const input_container_t&	  input,
					 const VkAllocationCallbacks* allocator = DE_NULL)
{
	using ::std::begin;
	using ::std::end;
	using ::std::transform;

	auto output = output_t{};
	output.resize(input.size());

	struct Predicate
	{
		deleter_t deleter;
		move_t	  operator()(handle_t v)
		{
			return (v != VK_NULL_HANDLE) ? move_t{check(v), deleter} : move_t{};
		}
	};

	const auto wrapHandle = Predicate{deleter_t{vk, device, allocator}};

	transform(begin(input), end(input), begin(output), wrapHandle);

	return output;
}

/*--------------------------------------------------------------------*//*!
 * \brief create vkPipelineCache for test params
 *//*--------------------------------------------------------------------*/
Move<VkPipelineCache> createPipelineCache(const DeviceInterface& vk, VkDevice device, const TestParams& params)
{
	if (params.cacheType != TestParams::EXPLICIT_CACHE)
	{
		return {};
	}

	static constexpr auto cacheInfo = VkPipelineCacheCreateInfo{
		VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, //sType
		DE_NULL,									  //pNext
		VkPipelineCacheCreateFlags{},				  //flags
		deUintptr{0},								  //initialDataSize
		DE_NULL										  //pInitialData
	};

	return createPipelineCache(vk, device, &cacheInfo);
}

/*--------------------------------------------------------------------*//*!
 * \brief create VkPipelineLayout with descriptor sets from test parameters
 *//*--------------------------------------------------------------------*/
Move<VkPipelineLayout> createPipelineLayout(const DeviceInterface&				 vk,
											VkDevice							 device,
											const vector<VkDescriptorSetLayout>& setLayouts,
											const TestParams&)
{
	const auto layoutCreateInfo = VkPipelineLayoutCreateInfo{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
		DE_NULL,									   // pNext
		VkPipelineLayoutCreateFlags{},				   // flags
		static_cast<deUint32>(setLayouts.size()),	   // setLayoutCount
		setLayouts.data(),							   // pSetLayouts
		deUint32{0u},								   // pushConstantRangeCount
		DE_NULL,									   // pPushConstantRanges
	};

	return createPipelineLayout(vk, device, &layoutCreateInfo);
}

/*--------------------------------------------------------------------*//*!
 * \brief create basic VkPipelineLayout from test parameters
 *//*--------------------------------------------------------------------*/
Move<VkPipelineLayout> createPipelineLayout(const DeviceInterface& vk, VkDevice device, const TestParams&)
{
	static constexpr auto layoutCreateInfo = VkPipelineLayoutCreateInfo{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
		DE_NULL,									   // pNext
		VkPipelineLayoutCreateFlags{},				   // flags
		deUint32{0u},								   // setLayoutCount
		DE_NULL,									   // pSetLayouts
		deUint32{0u},								   // pushConstantRangeCount
		DE_NULL,									   // pPushConstantRanges
	};

	return createPipelineLayout(vk, device, &layoutCreateInfo);
}

/*--------------------------------------------------------------------*//*!
 * \brief Create array of shader modules
 *//*--------------------------------------------------------------------*/
vector<UniqueShaderModule> createShaderModules(const DeviceInterface&	  vk,
											   VkDevice					  device,
											   const BinaryCollection&	  collection,
											   const vector<const char*>& names)
{
	auto output = vector<UniqueShaderModule>{};
	output.reserve(names.size());

	for (const auto& name : names)
	{
		const auto& binary	   = collection.get(name);
		const auto	createInfo = VkShaderModuleCreateInfo{
			 VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, // sType
			 DE_NULL,									  // pNext
			 VkShaderModuleCreateFlags{},				  // flags
			 binary.getSize(),							  // codeSize
			 shader_cast(binary.getBinary())			  // pCode
		 };

		output.push_back(createShaderModule(vk, device, &createInfo));
	}

	return output;
}

/*--------------------------------------------------------------------*//*!
 * \brief Create array of shader binding stages
 *//*--------------------------------------------------------------------*/
vector<VkPipelineShaderStageCreateInfo> createShaderStages(const vector<Move<VkShaderModule>>&	modules,
														   const vector<VkShaderStageFlagBits>& stages)
{
	DE_ASSERT(modules.size() == stages.size());

	auto output = vector<VkPipelineShaderStageCreateInfo>{};
	output.reserve(modules.size());

	int i = 0;

	for (const auto& module : modules)
	{
		const auto stageInfo = VkPipelineShaderStageCreateInfo{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // sType
			DE_NULL,											 // pNext
			VkPipelineShaderStageCreateFlags{},					 // flags
			stages[i++],										 // stage
			*module,											 // module
			"main",												 // pName
			DE_NULL												 // pSpecializationInfo
		};

		output.push_back(stageInfo);
	}

	return output;
}

} // namespace test_common

/*--------------------------------------------------------------------*//*!
 * \brief Graphics pipeline specific testing
 *//*--------------------------------------------------------------------*/
namespace graphics_tests
{
using namespace test_common;

/*--------------------------------------------------------------------*//*!
 * \brief Common graphics pipeline create info initialization
 *//*--------------------------------------------------------------------*/
VkGraphicsPipelineCreateInfo getPipelineCreateInfoCommon()
{
	static constexpr auto VERTEX_BINDING = VkVertexInputBindingDescription{
		deUint32{0u},				// binding
		sizeof(float[4]),			// stride
		VK_VERTEX_INPUT_RATE_VERTEX // inputRate
	};

	static constexpr auto VERTEX_ATTRIBUTE = VkVertexInputAttributeDescription{
		deUint32{0u},				   // location
		deUint32{0u},				   // binding
		VK_FORMAT_R32G32B32A32_SFLOAT, // format
		deUint32{0u}				   // offset
	};

	static constexpr auto VERTEX_INPUT_STATE = VkPipelineVertexInputStateCreateInfo{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // sType
		DE_NULL,												   // pNext
		VkPipelineVertexInputStateCreateFlags{},				   // flags
		deUint32{1u},											   // vertexBindingDescriptionCount
		&VERTEX_BINDING,										   // pVertexBindingDescriptions
		deUint32{1u},											   // vertexAttributeDescriptionCount
		&VERTEX_ATTRIBUTE										   // pVertexAttributeDescriptions
	};

	static constexpr auto IA_STATE = VkPipelineInputAssemblyStateCreateInfo{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // sType
		DE_NULL,													 // pNext
		VkPipelineInputAssemblyStateCreateFlags{},					 // flags
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,						 // topology
		VK_TRUE														 // primitiveRestartEnable
	};

	static constexpr auto TESSALATION_STATE = VkPipelineTessellationStateCreateInfo{
		VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO, // sType
		DE_NULL,																 // pNext
		VkPipelineTessellationStateCreateFlags{},								 // flags
		deUint32{0u}															 // patchControlPoints
	};

	static constexpr auto VIEWPORT = VkViewport{
		0.f, // x
		0.f, // y
		1.f, // width
		1.f, // height
		0.f, // minDepth
		1.f	 // maxDept
	};

	static constexpr auto SCISSOR_RECT = VkRect2D{
		{0, 0},	   // offset
		{256, 256} // extent
	};

	static constexpr auto VIEWPORT_STATE = VkPipelineViewportStateCreateInfo{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // sType
		DE_NULL,											   // pNext
		VkPipelineViewportStateCreateFlags{},				   // flags
		deUint32{1u},										   // viewportCount
		&VIEWPORT,											   // pViewports
		deUint32{1u},										   // scissorCount
		&SCISSOR_RECT										   // pScissors
	};

	static constexpr auto RASTERIZATION_STATE = VkPipelineRasterizationStateCreateInfo{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // sType
		DE_NULL,													// pNext
		VkPipelineRasterizationStateCreateFlags{},					// flags
		VK_FALSE,													// depthClampEnable
		VK_TRUE,													// rasterizerDiscardEnable
		VK_POLYGON_MODE_FILL,										// polygonMode
		VK_CULL_MODE_NONE,											// cullMode
		VK_FRONT_FACE_CLOCKWISE,									// frontFace
		VK_FALSE,													// depthBiasEnable
		0.f,														// depthBiasConstantFactor
		0.f,														// depthBiasClamp
		0.f,														// depthBiasSlopeFactor
		1.f															// lineWidth
	};

	static constexpr auto SAMPLE_MASK = VkSampleMask{};

	static constexpr auto MULTISAMPLE_STATE = VkPipelineMultisampleStateCreateInfo{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // sType
		DE_NULL,												  // pNext
		VkPipelineMultisampleStateCreateFlags{},				  // flags
		VK_SAMPLE_COUNT_1_BIT,									  // rasterizationSamples
		VK_FALSE,												  // sampleShadingEnable
		0.f,													  // minSampleShading
		&SAMPLE_MASK,											  // pSampleMask
		VK_FALSE,												  // alphaToCoverageEnable
		VK_FALSE												  // alphaToOneEnable
	};

	static constexpr auto STENCIL_OP_STATE = VkStencilOpState{
		VK_STENCIL_OP_ZERO,	  // failOp
		VK_STENCIL_OP_ZERO,	  // passOp
		VK_STENCIL_OP_ZERO,	  // depthFailOp
		VK_COMPARE_OP_ALWAYS, // compareOp
		deUint32{0u},		  // compareMask
		deUint32{0u},		  // writeMask
		deUint32{0u}		  // reference
	};

	static constexpr auto DEPTH_STENCIL_STATE = VkPipelineDepthStencilStateCreateInfo{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // sType
		DE_NULL,													// pNext
		VkPipelineDepthStencilStateCreateFlags{},					// flags
		VK_FALSE,													// depthTestEnable
		VK_FALSE,													// depthWriteEnable
		VK_COMPARE_OP_ALWAYS,										// depthCompareOp
		VK_FALSE,													// depthBoundsTestEnable
		VK_FALSE,													// stencilTestEnable
		STENCIL_OP_STATE,											// front
		STENCIL_OP_STATE,											// back
		0.f,														// minDepthBounds
		1.f															// maxDepthBounds
	};

	static constexpr auto COLOR_FLAGS_ALL = VkColorComponentFlags{VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
																  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

	static constexpr auto COLOR_BLEND_ATTACH_STATE = VkPipelineColorBlendAttachmentState{
		VK_FALSE,			  // blendEnable
		VK_BLEND_FACTOR_ONE,  // srcColorBlendFactor
		VK_BLEND_FACTOR_ZERO, // dstColorBlendFactor
		VK_BLEND_OP_ADD,	  // colorBlendOp
		VK_BLEND_FACTOR_ONE,  // srcAlphaBlendFactor
		VK_BLEND_FACTOR_ZERO, // dstAlphaBlendFactor
		VK_BLEND_OP_ADD,	  // alphaBlendOp
		COLOR_FLAGS_ALL		  // colorWriteMask
	};

	static constexpr auto COLOR_BLEND_STATE = VkPipelineColorBlendStateCreateInfo{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // sType
		DE_NULL,												  // pNext
		VkPipelineColorBlendStateCreateFlags{},					  // flags
		VK_FALSE,												  // logicOpEnable
		VK_LOGIC_OP_SET,										  // logicOp
		deUint32{1u},											  // attachmentCount
		&COLOR_BLEND_ATTACH_STATE,								  // pAttachments
		{0.f, 0.f, 0.f, 0.f}									  // blendConstants[4]
	};

	static constexpr auto DYNAMIC_STATE = VkPipelineDynamicStateCreateInfo{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, // sType;
		DE_NULL,											  // pNext;
		VkPipelineDynamicStateCreateFlags{},				  // flags;
		deUint32{0u},										  // dynamicStateCount;
		DE_NULL												  // pDynamicStates;
	};

	return VkGraphicsPipelineCreateInfo{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, // sType
		DE_NULL,										 // pNext
		VkPipelineCreateFlags{},						 // flags
		deUint32{0u},									 // stageCount
		DE_NULL,										 // pStages
		&VERTEX_INPUT_STATE,							 // pVertexInputState
		&IA_STATE,										 // pInputAssemblyState
		&TESSALATION_STATE,								 // pTessellationState
		&VIEWPORT_STATE,								 // pViewportState
		&RASTERIZATION_STATE,							 // pRasterizationState
		&MULTISAMPLE_STATE,								 // pMultisampleState
		&DEPTH_STENCIL_STATE,							 // pDepthStencilState
		&COLOR_BLEND_STATE,								 // pColorBlendState
		&DYNAMIC_STATE,									 // pDynamicState
		VK_NULL_HANDLE,									 // layout
		VK_NULL_HANDLE,									 // renderPass
		deUint32{0u},									 // subpass
		VK_NULL_HANDLE,									 // basePipelineHandle
		deInt32{-1}										 // basePipelineIndex
	};
}

/*--------------------------------------------------------------------*//*!
 * \brief create VkGraphicsPipelineCreateInfo structs from test iteration
 *//*--------------------------------------------------------------------*/
vector<VkGraphicsPipelineCreateInfo> createPipelineCreateInfos(const TestParams::Iteration&		   iteration,
															   const VkGraphicsPipelineCreateInfo& base,
															   VkPipeline						   basePipeline,
															   const TestParams&				   testParameter)
{
	auto output = vector<VkGraphicsPipelineCreateInfo>{};
	output.reserve(iteration.variants.size());

	deInt32 count			  = 0;
	deInt32 basePipelineIndex = -1;

	for (VkPipelineCreateFlags flags : iteration.variants)
	{
		const auto curIndex	  = count++;
		auto	   createInfo = base;

		if (testParameter.cacheType == TestParams::DERIVATIVE_INDEX)
		{
			if (flags & VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_EXT)
			{
				if (basePipelineIndex != -1)
				{
					flags |= VK_PIPELINE_CREATE_DERIVATIVE_BIT;
				}
			}
			else
			{
				flags |= VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;

				if (basePipelineIndex == -1)
				{
					basePipelineIndex = curIndex;
				}
			}
		}

		createInfo.flags			  = flags;
		createInfo.basePipelineHandle = basePipeline;
		createInfo.basePipelineIndex  = basePipelineIndex;

		output.push_back(createInfo);
	}

	return output;
}

/*--------------------------------------------------------------------*//*!
 * \brief create VkRenderPass object for Graphics test
 *//*--------------------------------------------------------------------*/
Move<VkRenderPass> createRenderPass(const DeviceInterface& vk, VkDevice device, const TestParams&)
{
	static constexpr auto COLOR_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;

	static constexpr auto COLOR_ATTACHMENT_REF = VkAttachmentReference{
		deUint32{0u},							 // attachment
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // layout
	};

	static constexpr auto SUBPASS = VkSubpassDescription{
		VkSubpassDescriptionFlags{},	 // flags
		VK_PIPELINE_BIND_POINT_GRAPHICS, // pipelineBindPoint
		deUint32{0u},					 // inputAttachmentCount
		DE_NULL,						 // pInputAttachments
		deUint32{1u},					 // colorAttachmentCount
		&COLOR_ATTACHMENT_REF,			 // pColorAttachments
		DE_NULL,						 // pResolveAttachments
		DE_NULL,						 // pDepthStencilAttachment
		deUint32{0u},					 // preserveAttachmentCount
		DE_NULL							 // pPreserveAttachments
	};

	static constexpr auto COLOR_ATTACHMENT = VkAttachmentDescription{
		VkAttachmentDescriptionFlags{},	  // flags
		COLOR_FORMAT,					  // format
		VK_SAMPLE_COUNT_1_BIT,			  // samples
		VK_ATTACHMENT_LOAD_OP_CLEAR,	  // loadOp
		VK_ATTACHMENT_STORE_OP_STORE,	  // storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE, // stencilStoreOp
		VK_IMAGE_LAYOUT_UNDEFINED,		  // initialLayout
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR	  // finalLayout
	};

	static constexpr auto RENDER_PASS_CREATE_INFO = VkRenderPassCreateInfo{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // sType
		DE_NULL,								   // pNext
		VkRenderPassCreateFlags{},				   // flags
		deUint32{1u},							   // attachmentCount
		&COLOR_ATTACHMENT,						   // pAttachments
		deUint32{1u},							   // subpassCount
		&SUBPASS,								   // pSubpasses
		deUint32{0u},							   // dependencyCount
		DE_NULL									   // pDependencies
	};

	return createRenderPass(vk, device, &RENDER_PASS_CREATE_INFO);
}

/*--------------------------------------------------------------------*//*!
 * \brief Initialize shader programs
 *//*--------------------------------------------------------------------*/
void initPrograms(SourceCollections& dst, const TestParams&)
{
	using ::glu::FragmentSource;
	using ::glu::VertexSource;

	// Vertex Shader
	static const StringTemplate VS_TEXT = {"#version 310 es\n"
										   "layout(location = 0) in vec4 position;\n"
										   "layout(location = 0) out vec3 vertColor;\n"
										   "void main (void)\n"
										   "{\n"
										   "  gl_Position = position;\n"
										   "  vertColor = vec3(${0}, ${1}, ${2});\n"
										   "}\n"};

	// Fragment Shader
	static const StringTemplate FS_TEXT = {"#version 310 es\n"
										   "precision highp float;\n"
										   "layout(location = 0) in vec3 vertColor;\n"
										   "layout(location = 0) out vec4 outColor;\n"
										   "void main (void)\n"
										   "{\n"
										   "  const vec3 fragColor = vec3(${0}, ${1}, ${2});\n"
										   "  outColor = vec4((fragColor + vertColor) * 0.5, 1.0);\n"
										   "}\n"};

	dst.glslSources.add("vertex") << VertexSource{VS_TEXT.format(randomFloat(), randomFloat(), randomFloat())};
	dst.glslSources.add("fragment") << FragmentSource{FS_TEXT.format(randomFloat(), randomFloat(), randomFloat())};
}

/*--------------------------------------------------------------------*//*!
 * \brief return both result and elapsed time from pipeline creation
 *//*--------------------------------------------------------------------*/
template <typename create_infos_t, typename pipelines_t>
TimedResult timePipelineCreation(const DeviceInterface&		  vk,
								 const VkDevice				  device,
								 const VkPipelineCache		  cache,
								 const create_infos_t&		  createInfos,
								 pipelines_t&				  pipelines,
								 const VkAllocationCallbacks* pAllocator = DE_NULL)
{
	DE_ASSERT(createInfos.size() <= pipelines.size());

	const auto timeStart = high_resolution_clock::now();
	const auto result	 = vk.createGraphicsPipelines(
		   device, cache, static_cast<deUint32>(createInfos.size()), createInfos.data(), pAllocator, pipelines.data());
	const auto elapsed = high_resolution_clock::now() - timeStart;
	return {result, elapsed};
}

/*--------------------------------------------------------------------*//*!
 * \brief Test instance function
 *//*--------------------------------------------------------------------*/
TestStatus testInstance(Context& context, const TestParams& testParameter)
{
	const auto& vk			  = context.getDeviceInterface();
	const auto	device		  = context.getDevice();
	const auto	pipelineCache = createPipelineCache(vk, device, testParameter);
	const auto	layout		  = createPipelineLayout(vk, device, testParameter);
	const auto	renderPass	  = createRenderPass(vk, device, testParameter);
	const auto	modules		  = createShaderModules(vk, device, context.getBinaryCollection(), {"vertex", "fragment"});
	const auto	shaderStages  = createShaderStages(modules, {VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT});

	// Placeholder for base pipeline if using cacheType == DERIVATIVE_HANDLE
	auto basePipeline = UniquePipeline{};

	auto baseCreateInfo		  = getPipelineCreateInfoCommon();
	baseCreateInfo.layout	  = layout.get();
	baseCreateInfo.renderPass = renderPass.get();
	baseCreateInfo.stageCount = static_cast<deUint32>(shaderStages.size());
	baseCreateInfo.pStages	  = shaderStages.data();

	auto results = vector<VkResult>{};
	results.reserve(testParameter.iterations.size());

	for (const auto& i : testParameter.iterations)
	{
		auto createInfos	= createPipelineCreateInfos(i, baseCreateInfo, basePipeline.get(), testParameter);
		auto created		= vector<VkPipeline>{};
		created.resize(createInfos.size());

#ifndef CTS_USES_VULKANSC
		std::vector<VkPipelineCreateFlags2CreateInfoKHR> flags2CreateInfo(created.size(), { VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR, 0, 0 });
		if (testParameter.useMaintenance5)
		{
			for (deUint32 ci = 0; ci < createInfos.size(); ++ci)
			{
				flags2CreateInfo[ci].flags	= translateCreateFlag(createInfos[ci].flags);
				flags2CreateInfo[ci].pNext  = createInfos[ci].pNext;
				createInfos[ci].flags		= 0;
				createInfos[ci].pNext		= &flags2CreateInfo[ci];
			}
		}
#endif // CTS_USES_VULKANSC

		const auto timedResult = timePipelineCreation(vk, device, pipelineCache.get(), createInfos, created);
		auto	   pipelines   = wrapHandles(vk, device, created);

		const auto status = validateResults(timedResult.result, pipelines, timedResult.elapsed, i.validators);
		if (status.getCode() != QP_TEST_RESULT_PASS)
		{
			return status;
		}

		if ((testParameter.cacheType == TestParams::DERIVATIVE_HANDLE) && (*basePipeline == VK_NULL_HANDLE))
		{
			for (auto& pipeline : pipelines)
			{
				if (*pipeline != VK_NULL_HANDLE)
				{
					basePipeline = pipeline;
					break;
				}
			}
		}

		results.push_back(timedResult.result);
	}

	static const StringTemplate PASS_MSG = {"Test Passed. ${0}"};
	return TestStatus::pass(PASS_MSG.format(getResultsString(results)));
}

} // namespace graphics_tests

/*--------------------------------------------------------------------*//*!
 * \brief Compute pipeline specific testing
 *//*--------------------------------------------------------------------*/
namespace compute_tests
{
using namespace test_common;

/*--------------------------------------------------------------------*//*!
 * \brief create VkComputePipelineCreateInfo structs from test iteration
 *//*--------------------------------------------------------------------*/
vector<VkComputePipelineCreateInfo> createPipelineCreateInfos(const TestParams::Iteration&		 iteration,
															  const VkComputePipelineCreateInfo& base,
															  VkPipeline						 basePipeline,
															  const TestParams&					 testParameter)
{
	auto output = vector<VkComputePipelineCreateInfo>{};
	output.reserve(iteration.variants.size());

	deInt32 count			  = 0;
	deInt32 basePipelineIndex = -1;

	for (VkPipelineCreateFlags flags : iteration.variants)
	{
		const auto curIndex	  = count++;
		auto	   createInfo = base;

		if (testParameter.cacheType == TestParams::DERIVATIVE_INDEX)
		{
			if (flags & VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_EXT)
			{
				if (basePipelineIndex != -1)
				{
					flags |= VK_PIPELINE_CREATE_DERIVATIVE_BIT;
				}
			}
			else
			{
				flags |= VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;

				if (basePipelineIndex == -1)
				{
					basePipelineIndex = curIndex;
				}
			}
		}

		createInfo.flags			  = flags;
		createInfo.basePipelineHandle = basePipeline;
		createInfo.basePipelineIndex  = basePipelineIndex;

		output.push_back(createInfo);
	}

	return output;
}

/*--------------------------------------------------------------------*//*!
 * \brief create compute descriptor set layout
 *//*--------------------------------------------------------------------*/
Move<VkDescriptorSetLayout> createDescriptorSetLayout(const DeviceInterface& vk, VkDevice device, const TestParams&)
{
	static constexpr auto DESCRIPTOR_SET_LAYOUT_BINDING = VkDescriptorSetLayoutBinding{
		deUint32{0u},					   // binding
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // descriptorType
		deUint32{1u},					   // descriptorCount
		VK_SHADER_STAGE_COMPUTE_BIT,	   // stageFlags
		DE_NULL							   // pImmutableSamplers
	};

	static constexpr auto DESCRIPTOR_SET_LAYOUT_CREATE_INFO = VkDescriptorSetLayoutCreateInfo{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // sType
		DE_NULL,											 // pNext
		VkDescriptorSetLayoutCreateFlags{},					 // flags
		deUint32{1u},										 // bindingCount
		&DESCRIPTOR_SET_LAYOUT_BINDING						 // pBindings
	};

	return createDescriptorSetLayout(vk, device, &DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
}

/*--------------------------------------------------------------------*//*!
 * \brief Initialize shader programs
 *//*--------------------------------------------------------------------*/
void initPrograms(SourceCollections& dst, const TestParams&)
{
	using ::glu::ComputeSource;

	static const StringTemplate CS_TEXT = {"#version 450\n"
										   "precision highp float;\n"
										   "layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;\n"
										   "layout (std140, binding = 0) buffer buf { vec3 data[]; };\n"
										   "void main (void)\n"
										   "{\n"
										   "  data[gl_GlobalInvocationID.x] = vec3(${0}, ${1}, ${2});\n"
										   "}\n"};

	dst.glslSources.add("compute")
		<< ComputeSource{CS_TEXT.format(randomFloat(), randomFloat(), randomFloat())};
}

/*--------------------------------------------------------------------*//*!
 * \brief return both result and elapsed time from pipeline creation
 *//*--------------------------------------------------------------------*/
template <typename create_infos_t, typename pipelines_t>
TimedResult timePipelineCreation(const DeviceInterface&		  vk,
								 const VkDevice				  device,
								 const VkPipelineCache		  cache,
								 const create_infos_t&		  createInfos,
								 pipelines_t&				  pipelines,
								 const VkAllocationCallbacks* pAllocator = DE_NULL)
{
	DE_ASSERT(createInfos.size() <= pipelines.size());

	const auto timeStart = high_resolution_clock::now();
	const auto result	 = vk.createComputePipelines(
		   device, cache, static_cast<deUint32>(createInfos.size()), createInfos.data(), pAllocator, pipelines.data());
	const auto elapsed = high_resolution_clock::now() - timeStart;
	return {result, elapsed};
}

/*--------------------------------------------------------------------*//*!
 * \brief Test instance function
 *//*--------------------------------------------------------------------*/
TestStatus testInstance(Context& context, const TestParams& testParameter)
{
	const auto& vk					= context.getDeviceInterface();
	const auto	device				= context.getDevice();
	const auto	pipelineCache		= createPipelineCache(vk, device, testParameter);
	const auto	descriptorSetLayout = createDescriptorSetLayout(vk, device, testParameter);
	const auto	pipelineLayout		= createPipelineLayout(vk, device, {descriptorSetLayout.get()}, testParameter);
	const auto	modules				= createShaderModules(vk, device, context.getBinaryCollection(), {"compute"});
	const auto	shaderStages		= createShaderStages(modules, {VK_SHADER_STAGE_COMPUTE_BIT});

	// Placeholder for base pipeline if using cacheType == DERIVATIVE_HANDLE
	auto basePipeline = UniquePipeline{};

	const auto baseCreateInfo = VkComputePipelineCreateInfo{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // sType
		DE_NULL,										// pNext
		VkPipelineCreateFlags{},						// flags
		shaderStages[0],								// stage
		pipelineLayout.get(),							// layout
		VK_NULL_HANDLE,									// basePipelineHandle
		deInt32{-1}										// basePipelineIndex
	};

	auto results = vector<VkResult>{};
	results.reserve(testParameter.iterations.size());

	for (const auto& i : testParameter.iterations)
	{
		const auto createInfos = createPipelineCreateInfos(i, baseCreateInfo, basePipeline.get(), testParameter);
		auto	   created	   = vector<VkPipeline>{};
		created.resize(createInfos.size());

		const auto timedResult = timePipelineCreation(vk, device, pipelineCache.get(), createInfos, created);
		auto	   pipelines   = wrapHandles(vk, device, created);

		const auto status = validateResults(timedResult.result, pipelines, timedResult.elapsed, i.validators);
		if (status.getCode() != QP_TEST_RESULT_PASS)
		{
			return status;
		}

		if ((testParameter.cacheType == TestParams::DERIVATIVE_HANDLE) && (*basePipeline == VK_NULL_HANDLE))
		{
			for (auto& pipeline : pipelines)
			{
				if (*pipeline != VK_NULL_HANDLE)
				{
					basePipeline = pipeline;
					break;
				}
			}
		}

		results.push_back(timedResult.result);
	}

	static const StringTemplate PASS_MSG = {"Test Passed. ${0}"};
	return TestStatus::pass(PASS_MSG.format(getResultsString(results)));
}

} // namespace compute_tests

using namespace test_common;

// Disable formatting on this next block for readability
// clang-format off
/*--------------------------------------------------------------------*//*!
 * \brief Duplicate single pipeline recreation with explicit caching
 *//*--------------------------------------------------------------------*/
static constexpr TestParams DUPLICATE_SINGLE_RECREATE_EXPLICIT_CACHING =
{
	"duplicate_single_recreate_explicit_caching",
	"Duplicate single pipeline recreation with explicit caching",
	TestParams::EXPLICIT_CACHE,
	TestParams::IterationArray
	{
		TestParams::Iteration{
			// Iteration [0]: Force compilation of pipeline
			TestParams::Iteration::SINGLE_NORMAL,
			ValidatorArray{
				// Fail if result is not VK_SUCCESS
				checkResult<VK_SUCCESS>,
				// Fail if pipeline is not valid
				checkPipelineMustBeValid<0>
			}
		},
		TestParams::Iteration{
			// Iteration [1]: Request compilation of same pipeline without compile
			TestParams::Iteration::SINGLE_NOCOMPILE,
			ValidatorArray{
				// Warn if result is not VK_SUCCESS
				checkResult<VK_SUCCESS, QP_TEST_RESULT_COMPATIBILITY_WARNING>,
				// Warn if pipeline is not valid
				checkPipelineMustBeValid<0, QP_TEST_RESULT_COMPATIBILITY_WARNING>,
				// Warn if pipeline took too long
				checkElapsedTime<ELAPSED_TIME_FAST, QP_TEST_RESULT_QUALITY_WARNING>
			}
		}
	},
	false
};

/*--------------------------------------------------------------------*//*!
 * \brief Duplicate single pipeline recreation with no explicit cache
 *//*--------------------------------------------------------------------*/
static constexpr TestParams DUPLICATE_SINGLE_RECREATE_NO_CACHING =
{
	"duplicate_single_recreate_no_caching",
	"Duplicate single pipeline recreation with no explicit cache",
	TestParams::NO_CACHE,
	TestParams::IterationArray{
		TestParams::Iteration{
			// Iteration [0]: Force compilation of pipeline
			TestParams::Iteration::SINGLE_NORMAL,
			ValidatorArray{
				// Fail if result is not VK_SUCCESS
				checkResult<VK_SUCCESS>,
				// Fail if pipeline is not valid
				checkPipelineMustBeValid<0>
			}
		},
		TestParams::Iteration{
			// Iteration [1]: Request compilation of same pipeline without compile
			TestParams::Iteration::SINGLE_NOCOMPILE,
			ValidatorArray{
				// Warn if pipeline took too long
				checkElapsedTime<ELAPSED_TIME_FAST, QP_TEST_RESULT_QUALITY_WARNING>
			}
		}
	},
	false
};

/*--------------------------------------------------------------------*//*!
 * \brief Duplicate single pipeline recreation using derivative pipelines
 *//*--------------------------------------------------------------------*/
static constexpr TestParams DUPLICATE_SINGLE_RECREATE_DERIVATIVE =
{
	"duplicate_single_recreate_derivative",
	"Duplicate single pipeline recreation using derivative pipelines",
	TestParams::DERIVATIVE_HANDLE,
	TestParams::IterationArray{
		TestParams::Iteration{
			// Iteration [0]: Force compilation of pipeline
			TestParams::Iteration::SINGLE_NORMAL,
			ValidatorArray{
				// Fail if result is not VK_SUCCESS
				checkResult<VK_SUCCESS>,
				// Fail if pipeline is not valid
				checkPipelineMustBeValid<0>
				}
			},
		TestParams::Iteration{
			// Iteration [1]: Request compilation of same pipeline without compile
			TestParams::Iteration::SINGLE_NOCOMPILE,
			ValidatorArray{
				// Warn if pipeline took too long
				checkElapsedTime<ELAPSED_TIME_FAST, QP_TEST_RESULT_QUALITY_WARNING>
			}
		}
	},
	false
};

/*--------------------------------------------------------------------*//*!
 * \brief Single creation of never before seen pipeline without compile
 *//*--------------------------------------------------------------------*/
static constexpr TestParams SINGLE_PIPELINE_NO_COMPILE =
{
	"single_pipeline_no_compile",
	"Single creation of never before seen pipeline without compile",
	TestParams::NO_CACHE,
	TestParams::IterationArray{
		TestParams::Iteration{
			TestParams::Iteration::SINGLE_NOCOMPILE,
			ValidatorArray{
				// Warn if pipeline took too long
				checkElapsedTime<ELAPSED_TIME_IMMEDIATE, QP_TEST_RESULT_QUALITY_WARNING>
			}
		}
	},
	false
};

/*--------------------------------------------------------------------*//*!
 * \brief Batch creation of duplicate pipelines with explicit caching
 *//*--------------------------------------------------------------------*/
static constexpr TestParams DUPLICATE_BATCH_PIPELINES_EXPLICIT_CACHE =
{
	"duplicate_batch_pipelines_explicit_cache",
	"Batch creation of duplicate pipelines with explicit caching",
	TestParams::EXPLICIT_CACHE,
	TestParams::IterationArray{
		TestParams::Iteration{
			TestParams::Iteration::BATCH_NOCOMPILE_COMPILE_NOCOMPILE,
			ValidatorArray{
				// Fail if pipeline[1] is not valid
				checkPipelineMustBeValid<1>,
				// Warn if result is not VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT
				checkResult<VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT, QP_TEST_RESULT_COMPATIBILITY_WARNING>,
				// Warn if pipelines[0] is not VK_NULL_HANDLE
				checkPipelineMustBeNull<0, QP_TEST_RESULT_COMPATIBILITY_WARNING>,
				// Warn if pipelines[2] is not valid
				checkPipelineMustBeValid<2, QP_TEST_RESULT_COMPATIBILITY_WARNING>
			}
		}
	},
	false
};

/*--------------------------------------------------------------------*//*!
 * \brief Batch creation of duplicate pipelines with no caching
 *//*--------------------------------------------------------------------*/
static constexpr TestParams DUPLICATE_BATCH_PIPELINES_NO_CACHE =
{
	"duplicate_batch_pipelines_no_cache",
	"Batch creation of duplicate pipelines with no caching",
	TestParams::NO_CACHE,
	TestParams::IterationArray{
		TestParams::Iteration{
			TestParams::Iteration::BATCH_NOCOMPILE_COMPILE_NOCOMPILE,
			ValidatorArray{
				// Fail if pipeline[1] is not valid
				checkPipelineMustBeValid<1>,
				// Warn if result is not VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT
				checkResult<VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT, QP_TEST_RESULT_COMPATIBILITY_WARNING>,
				// Warn if pipelines[0] is not VK_NULL_HANDLE
				checkPipelineMustBeNull<0, QP_TEST_RESULT_COMPATIBILITY_WARNING>
			}
		}
	},
	false
};

/*--------------------------------------------------------------------*//*!
 * \brief Batch creation of duplicate pipelines with derivative pipeline index
 *//*--------------------------------------------------------------------*/
static constexpr TestParams DUPLICATE_BATCH_PIPELINES_DERIVATIVE_INDEX =
{
	"duplicate_batch_pipelines_derivative_index",
	"Batch creation of duplicate pipelines with derivative pipeline index",
	TestParams::DERIVATIVE_INDEX,
	TestParams::IterationArray{
		TestParams::Iteration{
			TestParams::Iteration::BATCH_NOCOMPILE_COMPILE_NOCOMPILE,
			ValidatorArray{
				// Fail if pipeline[1] is not valid
				checkPipelineMustBeValid<1>,
				// Warn if result is not VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT
				checkResult<VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT, QP_TEST_RESULT_COMPATIBILITY_WARNING>,
				// Warn if pipelines[0] is not VK_NULL_HANDLE
				checkPipelineMustBeNull<0, QP_TEST_RESULT_COMPATIBILITY_WARNING>
			}
		}
	},
	false
};

/*--------------------------------------------------------------------*//*!
 * \brief Batch creation of pipelines with early return
 *//*--------------------------------------------------------------------*/
static constexpr TestParams BATCH_PIPELINES_EARLY_RETURN =
{
	"batch_pipelines_early_return",
	"Batch creation of pipelines with early return",
	TestParams::NO_CACHE,
	TestParams::IterationArray{
		TestParams::Iteration{
			TestParams::Iteration::BATCH_RETURN_COMPILE_NOCOMPILE,
			ValidatorArray{
				// fail if a valid pipeline follows the early-return failure
				checkPipelineNullAfterIndex<0>,
				// Warn if return was not immediate
				checkElapsedTime<ELAPSED_TIME_IMMEDIATE, QP_TEST_RESULT_QUALITY_WARNING>,
				// Warn if pipelines[0] is not VK_NULL_HANDLE
				checkPipelineMustBeNull<0, QP_TEST_RESULT_COMPATIBILITY_WARNING>,
				// Warn if result is not VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT
				checkResult<VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT, QP_TEST_RESULT_COMPATIBILITY_WARNING>
			}
		}
	},
	false
};

/*--------------------------------------------------------------------*//*!
 * \brief Batch creation of pipelines with early return using VkPipelineCreateFlagBits2KHR from maintenance5
 *//*--------------------------------------------------------------------*/
static constexpr TestParams BATCH_PIPELINES_EARLY_RETURN_MAINTENANCE_5
{
	"batch_pipelines_early_return_maintenance5",
	"Batch creation of pipelines with early return and maintenance5",
	TestParams::NO_CACHE,
	TestParams::IterationArray{
		TestParams::Iteration{
			TestParams::Iteration::BATCH_RETURN_COMPILE_NOCOMPILE,
			ValidatorArray{
				// fail if a valid pipeline follows the early-return failure
				checkPipelineNullAfterIndex<0>,
				// Warn if return was not immediate
				checkElapsedTime<ELAPSED_TIME_IMMEDIATE, QP_TEST_RESULT_QUALITY_WARNING>,
				// Warn if pipelines[0] is not VK_NULL_HANDLE
				checkPipelineMustBeNull<0, QP_TEST_RESULT_COMPATIBILITY_WARNING>,
				// Warn if result is not VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT
				checkResult<VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT, QP_TEST_RESULT_COMPATIBILITY_WARNING>
			}
		}
	},
	true,
};

/*--------------------------------------------------------------------*//*!
 * \brief Full array of test cases
 *//*--------------------------------------------------------------------*/
static constexpr TestParams TEST_CASES[] =
{
	SINGLE_PIPELINE_NO_COMPILE,
	BATCH_PIPELINES_EARLY_RETURN,
	DUPLICATE_SINGLE_RECREATE_EXPLICIT_CACHING,
	DUPLICATE_SINGLE_RECREATE_NO_CACHING,
	DUPLICATE_SINGLE_RECREATE_DERIVATIVE,
	DUPLICATE_BATCH_PIPELINES_EXPLICIT_CACHE,
	DUPLICATE_BATCH_PIPELINES_NO_CACHE,
	DUPLICATE_BATCH_PIPELINES_DERIVATIVE_INDEX,

#ifndef CTS_USES_VULKANSC
	BATCH_PIPELINES_EARLY_RETURN_MAINTENANCE_5,
#endif // CTS_USES_VULKANSC

};
// clang-format on

/*--------------------------------------------------------------------*//*!
 * \brief Variadic version of de::newMovePtr
 *//*--------------------------------------------------------------------*/
template <typename T, typename... args_t>
inline de::MovePtr<T> newMovePtr(args_t&&... args)
{
	return de::MovePtr<T>(new T(::std::forward<args_t>(args)...));
}

/*--------------------------------------------------------------------*//*!
 * \brief Make test group consisting of graphics pipeline tests
 *//*--------------------------------------------------------------------*/
void addGraphicsPipelineTests(TestCaseGroup& group)
{
	using namespace graphics_tests;

	auto tests = newMovePtr<TestCaseGroup>(
		group.getTestContext(), "graphics_pipelines", "Test pipeline creation cache control with graphics pipelines");

	for (const auto& params : TEST_CASES)
	{
		addFunctionCaseWithPrograms<const TestParams&>(
			tests.get(), params.name, params.description, checkSupport, initPrograms, testInstance, params);
	}

	group.addChild(tests.release());
}

/*--------------------------------------------------------------------*//*!
 * \brief Make test group consisting of compute pipeline tests
 *//*--------------------------------------------------------------------*/
void addComputePipelineTests(TestCaseGroup& group)
{
	using namespace compute_tests;

	auto tests = newMovePtr<TestCaseGroup>(
		group.getTestContext(), "compute_pipelines", "Test pipeline creation cache control with compute pipelines");

	for (const auto& params : TEST_CASES)
	{
		addFunctionCaseWithPrograms<const TestParams&>(
			tests.get(), params.name, params.description, checkSupport, initPrograms, testInstance, params);
	}

	group.addChild(tests.release());
}

} // namespace

/*--------------------------------------------------------------------*//*!
 * \brief Make pipeline creation cache control test group
 *//*--------------------------------------------------------------------*/
TestCaseGroup* createCacheControlTests(TestContext& testCtx)
{
	auto tests = newMovePtr<TestCaseGroup>(testCtx, "creation_cache_control", "pipeline creation cache control tests");

	addGraphicsPipelineTests(*tests);
	addComputePipelineTests(*tests);

	return tests.release();
}

} // namespace pipeline

} // namespace vkt
