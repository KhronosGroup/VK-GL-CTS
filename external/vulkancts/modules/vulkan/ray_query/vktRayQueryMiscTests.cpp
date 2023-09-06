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
 * \brief Ray Query miscellaneous tests
 *//*--------------------------------------------------------------------*/

#include "vktRayQueryMiscTests.hpp"
#include "vktTestCase.hpp"

#include "vkRayTracingUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkImageUtil.hpp"

#include "tcuVector.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTextureUtil.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"

#include <sstream>
#include <limits>
#include <vector>
#include <map>

namespace vkt
{
namespace RayQuery
{

namespace
{

using namespace vk;

class DynamicIndexingCase : public vkt::TestCase
{
public:
							DynamicIndexingCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description);
	virtual					~DynamicIndexingCase	(void) {}

	virtual void			initPrograms			(vk::SourceCollections& programCollection) const override;
	virtual void			checkSupport			(Context& context) const override;
	virtual TestInstance*	createInstance			(Context& context) const override;

	// Constants and data types.
	static constexpr deUint32	kLocalSizeX	= 48u;
	static constexpr deUint32	kNumQueries	= 48u;

	// This must match the shader.
	struct InputData
	{
		deUint32 goodQueryIndex;
		deUint32 proceedQueryIndex;
	};
};

class DynamicIndexingInstance : public vkt::TestInstance
{
public:
								DynamicIndexingInstance		(Context& context);
	virtual						~DynamicIndexingInstance	(void) {}

	virtual tcu::TestStatus		iterate						(void);
};

DynamicIndexingCase::DynamicIndexingCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description)
	: vkt::TestCase (testCtx, name, description)
{}

void DynamicIndexingCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	std::ostringstream src;

	src
		<< "#version 460\n"
		<< "#extension GL_EXT_ray_query : require\n"
		<< "#extension GL_EXT_ray_tracing : require\n"
		<< "\n"
		<< "layout (local_size_x=" << kLocalSizeX << ", local_size_y=1, local_size_z=1) in; \n"
		<< "\n"
		<< "struct InputData {\n"
		<< "    uint goodQueryIndex;\n"
		<< "    uint proceedQueryIndex; // Note: same index as the one above in practice.\n"
		<< "};\n"
		<< "\n"
		<< "layout (set=0, binding=0) uniform accelerationStructureEXT topLevelAS;\n"
		<< "layout (set=0, binding=1, std430) buffer InputBlock {\n"
		<< "    InputData inputData[];\n"
		<< "} inputBlock;\n"
		<< "layout (set=0, binding=2, std430) buffer OutputBlock {\n"
		<< "    uint outputData[];\n"
		<< "} outputBlock;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "    const uint numQueries = " << kNumQueries << ";\n"
		<< "\n"
		<< "    const uint rayFlags = 0u; \n"
		<< "    const uint cullMask = 0xFFu;\n"
		<< "    const float tmin = 0.1;\n"
		<< "    const float tmax = 10.0;\n"
		<< "    const vec3 direct = vec3(0, 0, 1); \n"
		<< "\n"
		<< "    rayQueryEXT rayQueries[numQueries];\n"
		<< "    vec3 origin;\n"
		<< "\n"
		<< "    InputData inputValues = inputBlock.inputData[gl_LocalInvocationID.x];\n"
		<< "\n"
		<< "    // Initialize all queries. Only goodQueryIndex will have the right origin for a hit.\n"
		<< "    for (int i = 0; i < numQueries; i++) {\n"
		<< "        origin = ((i == inputValues.goodQueryIndex) ? vec3(0, 0, 0) : vec3(5, 5, 0));\n"
		<< "        rayQueryInitializeEXT(rayQueries[i], topLevelAS, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
		<< "    }\n"
		<< "\n"
		<< "    // Attempt to proceed with the good query to confirm a hit.\n"
		<< "    while (rayQueryProceedEXT(rayQueries[inputValues.proceedQueryIndex]))\n"
		<< "        outputBlock.outputData[gl_LocalInvocationID.x] = 1u; \n"
		<< "}\n"
		;

	programCollection.glslSources.add("comp") << glu::ComputeSource(updateRayTracingGLSL(src.str())) << buildOptions;
}

void DynamicIndexingCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_ray_query");

	const auto& rayQueryFeaturesKHR = context.getRayQueryFeatures();
	if (!rayQueryFeaturesKHR.rayQuery)
		TCU_THROW(NotSupportedError, "Ray queries not supported");

	const auto& accelerationStructureFeaturesKHR = context.getAccelerationStructureFeatures();
	if (!accelerationStructureFeaturesKHR.accelerationStructure)
		TCU_FAIL("Acceleration structures not supported but ray queries supported");
}

vkt::TestInstance* DynamicIndexingCase::createInstance (Context& context) const
{
	return new DynamicIndexingInstance(context);
}

DynamicIndexingInstance::DynamicIndexingInstance (Context& context)
	: vkt::TestInstance(context)
{}

deUint32 getRndIndex (de::Random& rng, deUint32 size)
{
	DE_ASSERT(size > 0u);
	DE_ASSERT(size <= static_cast<deUint32>(std::numeric_limits<int>::max()));

	const int	iMin = 0;
	const int	iMax = static_cast<int>(size) - 1;

	return static_cast<deUint32>(rng.getInt(iMin, iMax));
}

tcu::TestStatus DynamicIndexingInstance::iterate (void)
{
	using InputData = DynamicIndexingCase::InputData;
	constexpr auto	kLocalSizeX		= DynamicIndexingCase::kLocalSizeX;
	constexpr auto	kNumQueries		= DynamicIndexingCase::kNumQueries;

	const auto&	vkd		= m_context.getDeviceInterface();
	const auto	device	= m_context.getDevice();
	auto&		alloc	= m_context.getDefaultAllocator();
	const auto	queue	= m_context.getUniversalQueue();
	const auto	qIndex	= m_context.getUniversalQueueFamilyIndex();

	de::Random rng (1604936737u);
	InputData inputDataArray[kLocalSizeX];
	deUint32 outputDataArray[kLocalSizeX];

	// Prepare input buffer.
	for (int i = 0; i < DE_LENGTH_OF_ARRAY(inputDataArray); ++i)
	{
		// The two values will contain the same query index.
		inputDataArray[i].goodQueryIndex	= getRndIndex(rng, kNumQueries);
		inputDataArray[i].proceedQueryIndex	= inputDataArray[i].goodQueryIndex;
	}

	const auto			inputBufferSize		= static_cast<VkDeviceSize>(sizeof(inputDataArray));
	const auto			inputBufferInfo		= makeBufferCreateInfo(inputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory	inputBuffer			(vkd, device, alloc, inputBufferInfo, MemoryRequirement::HostVisible);
	auto&				inputBufferAlloc	= inputBuffer.getAllocation();
	void*				inputBufferPtr		= inputBufferAlloc.getHostPtr();

	deMemcpy(inputBufferPtr, inputDataArray, static_cast<size_t>(inputBufferSize));
	flushAlloc(vkd, device, inputBufferAlloc);

	// Prepare output buffer.
	const auto			outputBufferSize	= static_cast<VkDeviceSize>(sizeof(outputDataArray));
	const auto			outputBufferInfo	= makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory	outputBuffer		(vkd, device, alloc, outputBufferInfo, MemoryRequirement::HostVisible);
	auto&				outputBufferAlloc	= outputBuffer.getAllocation();
	void*				outputBufferPtr		= outputBufferAlloc.getHostPtr();

	deMemset(outputBufferPtr, 0, static_cast<size_t>(outputBufferSize));
	flushAlloc(vkd, device, outputBufferAlloc);

	// Prepare acceleration structures.
	const auto cmdPool		= makeCommandPool(vkd, device, qIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();
	beginCommandBuffer(vkd, cmdBuffer);

	de::SharedPtr<TopLevelAccelerationStructure>	topLevelAS		(makeTopLevelAccelerationStructure().release());
	de::SharedPtr<BottomLevelAccelerationStructure>	bottomLevelAS	(makeBottomLevelAccelerationStructure().release());

	// These need to match the origin and direction in the shader for a hit.
	const std::vector<tcu::Vec3> vertices =
	{
		tcu::Vec3(-1.0f, -1.0f, 1.0f),
		tcu::Vec3(-1.0f,  1.0f, 1.0f),
		tcu::Vec3( 1.0f, -1.0f, 1.0f),

		tcu::Vec3(-1.0f,  1.0f, 1.0f),
		tcu::Vec3( 1.0f,  1.0f, 1.0f),
		tcu::Vec3( 1.0f, -1.0f, 1.0f),
	};

	bottomLevelAS->addGeometry(vertices, /*triangles*/true, VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR);
	bottomLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc);

	topLevelAS->addInstance(bottomLevelAS);
	topLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc);

	// Descriptor set layout.
	const VkShaderStageFlagBits stageBit = VK_SHADER_STAGE_COMPUTE_BIT;

	DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, stageBit);
	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stageBit);
	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stageBit);
	const auto descriptorSetLayout = layoutBuilder.build(vkd, device);

	// Shader module.
	const auto shaderModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0u);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(vkd, device, descriptorSetLayout.get());

	const VkPipelineShaderStageCreateInfo shaderStageInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,												//	const void*							pNext;
		0u,														//	VkPipelineShaderStageCreateFlags	flags;
		stageBit,												//	VkShaderStageFlagBits				stage;
		shaderModule.get(),										//	VkShaderModule						module;
		"main",													//	const char*							pName;
		nullptr,												//	const VkSpecializationInfo*			pSpecializationInfo;
	};

	const VkComputePipelineCreateInfo pipelineInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	//	VkStructureType					sType;
		nullptr,										//	const void*						pNext;
		0u,												//	VkPipelineCreateFlags			flags;
		shaderStageInfo,								//	VkPipelineShaderStageCreateInfo	stage;
		pipelineLayout.get(),							//	VkPipelineLayout				layout;
		DE_NULL,										//	VkPipeline						basePipelineHandle;
		0,												//	deInt32							basePipelineIndex;
	};

	const auto pipeline = createComputePipeline(vkd, device, DE_NULL, &pipelineInfo);

	// Create and update descriptor set.
	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u);

	const auto descriptorPool	= poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const auto descriptorSetPtr	= makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());
	const auto descriptorSet	= descriptorSetPtr.get();

	const VkWriteDescriptorSetAccelerationStructureKHR asWrite =
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	//	VkStructureType						sType;
		nullptr,															//	const void*							pNext;
		1u,																	//	deUint32							accelerationStructureCount;
		topLevelAS->getPtr(),												//	const VkAccelerationStructureKHR*	pAccelerationStructures;
	};

	const auto inputBufferWriteInfo		= makeDescriptorBufferInfo(inputBuffer.get(), 0ull, inputBufferSize);
	const auto outputBufferWriteInfo	= makeDescriptorBufferInfo(outputBuffer.get(), 0ull, outputBufferSize);

	DescriptorSetUpdateBuilder updateBuilder;
	updateBuilder.writeSingle(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &asWrite);
	updateBuilder.writeSingle(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &inputBufferWriteInfo);
	updateBuilder.writeSingle(descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputBufferWriteInfo);
	updateBuilder.update(vkd, device);

	// Use pipeline.
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.get(), 0u, 1u, &descriptorSet, 0u, nullptr);
	vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);

	const auto memBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &memBarrier, 0u, nullptr, 0u, nullptr);

	// Submit recorded commands.
	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Check output buffer.
	invalidateAlloc(vkd, device, outputBufferAlloc);
	deMemcpy(outputDataArray, outputBufferPtr, static_cast<size_t>(outputBufferSize));

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(outputDataArray); ++i)
	{
		constexpr auto	expected	= 1u;
		const auto&		value		= outputDataArray[i];

		if (value != expected)
		{
			std::ostringstream msg;
			msg << "Unexpected value found at position " << i << " in the output buffer: expected " << expected << " but found " << value;
			TCU_FAIL(msg.str());
		}
	}

	return tcu::TestStatus::pass("Pass");
}

using namespace tcu;

struct HelperInvocationsParamDefs
{
	enum DfStyle
	{
		Regular,
		Coarse,
		Fine
	};

	enum FuncType
	{
		LINEAR,
		QUADRATIC,
		CUBIC
	};

	typedef float (*F1D)(float);
	struct func2D_t {
		F1D			first;
		F1D			second;
	};
	struct func2D_mask {
		FuncType	first;
		FuncType	second;
	};
	struct test_mode_t {
		func2D_t	funcs;
		func2D_mask	types;
	};

	static float linear(float x)	{ return x; }
	static float quadratic(float x)	{ return (x * x); }
	static float cubic(float x)		{ return (x * x * x * 0.5f); }

	static float combine(const func2D_t& f2D, float x, float y)
	{
		DE_ASSERT( (f2D.first) && (f2D.second) );
		const float z = ((*f2D.first)(x) + (*f2D.second)(y)) / 2.0f;
		return z;
	}

	static constexpr func2D_t FUNC_LINEAR_QUADRATIC			= { linear,		quadratic	};
	static constexpr func2D_t FUNC_LINEAR_CUBIC				= { linear,		cubic		};
	static constexpr func2D_t FUNC_CUBIC_QUADRATIC			= { cubic,		quadratic	};
	#ifdef ENABLE_ALL_HELPER_COMBINATIONS
	static constexpr func2D_t FUNC_LINEAR_LINEAR			= { linear,		linear		};
	static constexpr func2D_t FUNC_QUADRATIC_LINEAR			= { quadratic,	linear		};
	static constexpr func2D_t FUNC_QUADRATIC_QUADRATIC		= { quadratic,	quadratic	};
	static constexpr func2D_t FUNC_QUADRATIC_CUBIC			= { quadratic,	cubic		};
	static constexpr func2D_t FUNC_CUBIC_LINEAR				= { cubic,		linear		};
	static constexpr func2D_t FUNC_CUBIC_CUBIC				= { cubic,		cubic		};
	#endif

	static constexpr func2D_mask MASK_LINEAR_QUADRATIC		= { LINEAR,		QUADRATIC	};
	static constexpr func2D_mask MASK_LINEAR_CUBIC			= { LINEAR,		CUBIC		};
	static constexpr func2D_mask MASK_CUBIC_QUADRATIC		= { CUBIC,		QUADRATIC	};
	#ifdef ENABLE_ALL_HELPER_COMBINATIONS
	static constexpr func2D_mask MASK_LINEAR_LINEAR			= { LINEAR,		LINEAR		};
	static constexpr func2D_mask MASK_QUADRATIC_LINEAR		= { QUADRATIC,	LINEAR		};
	static constexpr func2D_mask MASK_QUADRATIC_QUADRATIC	= { QUADRATIC,	QUADRATIC	};
	static constexpr func2D_mask MASK_QUADRATIC_CUBIC		= { QUADRATIC,	CUBIC		};
	static constexpr func2D_mask MASK_CUBIC_LINEAR			= { CUBIC,		LINEAR		};
	static constexpr func2D_mask MASK_CUBIC_CUBIC			= { CUBIC,		CUBIC		};
	#endif

	static constexpr test_mode_t MODE_LINEAR_QUADRATIC		= { FUNC_LINEAR_QUADRATIC,		MASK_LINEAR_QUADRATIC	};
	static constexpr test_mode_t MODE_LINEAR_CUBIC			= { FUNC_LINEAR_CUBIC,			MASK_LINEAR_CUBIC		};
	static constexpr test_mode_t MODE_CUBIC_QUADRATIC		= { FUNC_CUBIC_QUADRATIC,		MASK_CUBIC_QUADRATIC	};
	#ifdef ENABLE_ALL_HELPER_COMBINATIONS
	static constexpr test_mode_t MODE_LINEAR_LINEAR			= { FUNC_LINEAR_LINEAR,			MASK_LINEAR_LINEAR		};
	static constexpr test_mode_t MODE_QUADRATIC_LINEAR		= { FUNC_QUADRATIC_LINEAR,		MASK_QUADRATIC_LINEAR	};
	static constexpr test_mode_t MODE_QUADRATIC_QUADRATIC	= { FUNC_QUADRATIC_QUADRATIC,	MASK_QUADRATIC_QUADRATIC};
	static constexpr test_mode_t MODE_QUADRATIC_CUBIC		= { FUNC_QUADRATIC_CUBIC,		MASK_QUADRATIC_CUBIC	};
	static constexpr test_mode_t MODE_CUBIC_LINEAR			= { FUNC_CUBIC_LINEAR,			MASK_CUBIC_LINEAR		};
	static constexpr test_mode_t MODE_CUBIC_CUBIC			= { FUNC_CUBIC_CUBIC,			MASK_CUBIC_CUBIC		};
	#endif
};

constexpr HelperInvocationsParamDefs::test_mode_t HelperInvocationsParamDefs::MODE_LINEAR_QUADRATIC;
constexpr HelperInvocationsParamDefs::test_mode_t HelperInvocationsParamDefs::MODE_LINEAR_CUBIC;
constexpr HelperInvocationsParamDefs::test_mode_t HelperInvocationsParamDefs::MODE_CUBIC_QUADRATIC;
#ifdef ENABLE_ALL_HELPER_COMBINATIONS
constexpr HelperInvocationsParamDefs::test_mode_t HelperInvocationsParamDefs::MODE_LINEAR_LINEAR;
constexpr HelperInvocationsParamDefs::test_mode_t HelperInvocationsParamDefs::MODE_QUADRATIC_LINEAR;
constexpr HelperInvocationsParamDefs::test_mode_t HelperInvocationsParamDefs::MODE_QUADRATIC_QUADRATIC;
constexpr HelperInvocationsParamDefs::test_mode_t HelperInvocationsParamDefs::MODE_QUADRATIC_CUBIC;
constexpr HelperInvocationsParamDefs::test_mode_t HelperInvocationsParamDefs::MODE_CUBIC_LINEAR;
constexpr HelperInvocationsParamDefs::test_mode_t HelperInvocationsParamDefs::MODE_CUBIC_CUBIC;
#endif

struct HelperInvocationsParams : HelperInvocationsParamDefs
{
	test_mode_t						mode;
	std::pair<deUint32, deUint32>	screen;
	std::pair<deUint32, deUint32>	model;
	DfStyle							style;
	bool							buildGPU;
};

class HelperInvocationsCase : public TestCase
{
public:
	HelperInvocationsCase					(TestContext&					testCtx,
											 const HelperInvocationsParams&	params,
											 const std::string&				name);
	virtual void			initPrograms	(SourceCollections&				programs) const override;
	virtual TestInstance*	createInstance	(Context&						context) const override;
	virtual void			checkSupport	(Context&						context) const override;

private:
	HelperInvocationsParams	m_params;
};

class HelperInvocationsInstance : public TestInstance
{
public:
	typedef de::MovePtr<TopLevelAccelerationStructure> TopLevelAccelerationStructurePtr;
	enum Points {
		Vertices,
		Coords,
		Centers
	};

	HelperInvocationsInstance				(Context&									context,
											 const HelperInvocationsParams&				params);
	virtual	TestStatus		iterate			(void) override;
	static	auto			createSurface	(const Points								points,
											 const deUint32								divX,
											 const deUint32								divY,
											 const HelperInvocationsParams::func2D_t&	f2D,
											 bool										clockWise = false) -> std::vector<Vec3>;
	VkImageCreateInfo		makeImgInfo		(deUint32									queueFamilyIndexCount,
											 const deUint32*							pQueueFamilyIndices) const;
	Move<VkPipeline>		makePipeline	(const DeviceInterface&						vk,
											 const VkDevice								device,
											 const VkPipelineLayout						pipelineLayout,
											 const VkShaderModule						vertexShader,
											 const VkShaderModule						fragmentShader,
											 const VkRenderPass							renderPass) const;
	auto					makeResultBuff	(const DeviceInterface&						vk,
											 const VkDevice								device,
											 Allocator&									allocator) const -> de::MovePtr<BufferWithMemory>;
	auto					makeAttribBuff	(const DeviceInterface&						vk,
											 const VkDevice								device,
											 Allocator&									allocator,
											 const std::vector<Vec3>&					vertices,
											 const std::vector<Vec3>&					coords,
											 const std::vector<Vec3>&					centers) const -> de::MovePtr<BufferWithMemory>;
	auto					createAccStructs(const DeviceInterface&						vk,
											 const VkDevice								device,
											 Allocator&									allocator,
											 const VkCommandBuffer						cmdBuffer,
											 const std::vector<Vec3>					coords) const -> TopLevelAccelerationStructurePtr;
protected:
	bool					verifyResult	(const DeviceInterface&						vk,
											 const VkDevice								device,
											 const BufferWithMemory&					buffer) const;
	bool onlyPipeline();
private:
	VkFormat				m_format;
	HelperInvocationsParams	m_params;
};

HelperInvocationsCase::HelperInvocationsCase (TestContext&						testCtx,
											  const HelperInvocationsParams&	params,
											  const std::string&				name)
	: TestCase	(testCtx, name, std::string())
	, m_params	(params)
{
}

TestInstance* HelperInvocationsCase::createInstance (Context& context) const
{
	return new HelperInvocationsInstance(context, m_params);
}

void HelperInvocationsCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_ray_query");

	const auto& rayQueryFeaturesKHR					= context.getRayQueryFeatures();
	const auto& accelerationStructureFeaturesKHR	= context.getAccelerationStructureFeatures();

	if (!rayQueryFeaturesKHR.rayQuery)
		TCU_THROW(NotSupportedError, "Ray queries not supported");

	if (!accelerationStructureFeaturesKHR.accelerationStructure)
		TCU_THROW(NotSupportedError, "Acceleration structures not supported but ray queries supported");

	if (m_params.buildGPU == false && accelerationStructureFeaturesKHR.accelerationStructureHostCommands == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceAccelerationStructureFeaturesKHR::accelerationStructureHostCommands");
}

void HelperInvocationsCase::initPrograms (SourceCollections& programs) const
{
	const ShaderBuildOptions buildOptions(programs.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	std::string vertexCode (
	R"(
	#version 460
	#extension GL_EXT_ray_query : require
	#extension GL_EXT_ray_tracing : require

	layout(location = 0) in vec3 pos;
	layout(location = 1) in vec3 inCoord;
	layout(location = 2) in vec3 inCenter;
	layout(location = 0) out vec3 outCoord;
	layout(location = 1) out vec3 outCenter;

	void main()
	{
		gl_PointSize = 1.0;
		gl_Position = vec4(pos.xyz, 1.0);
		outCoord = inCoord;
		outCenter = inCenter;
	}
	)");
	programs.glslSources.add("vert") << glu::VertexSource(vertexCode) << buildOptions;

	StringTemplate fragmentCode(
	R"(
	#version 460
	#extension GL_EXT_ray_query : require
	#extension GL_EXT_ray_tracing : require

	#define LINEAR    0
	#define QUADRATIC 1
	#define CUBIC     2

	layout(push_constant) uniform PC {
		int fun_x;
		int fun_y;
		float width;
		float height;
	} params;
	layout(location = 0) in vec3 coord;
	layout(location = 1) in vec3 center;
	layout(location = 0) out vec4 color;
	layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;

	float d_linear   (in float t) { return 0.5; }			// (x/2)'
	float d_quadratic(in float t) { return t; }				// (x^2/2)'
	float d_cubic    (in float t) { return 0.75 * t * t; }  // (x^3/4)'

	float derivate(in int fun, in float u)
	{
		switch (fun)
		{
			case LINEAR:	return d_linear(u);
			case QUADRATIC:	return d_quadratic(u);
			case CUBIC:		return d_cubic(u);
		}
		return -1.0;
	}
	void main()
	{
		const uint rayFlags	= 0u;
		const uint cullMask	= 0xFFu;
		const float tmin	= 0.0;
		const float tmax	= 10.0;
		const vec3 direct	= vec3(0.0, 0.0, 1.0);
		const vec3 origin	= vec3(center.x, center.y, -1.0);

		rayQueryEXT query;
		rayQueryInitializeEXT(query, topLevelAS, rayFlags, cullMask, origin, tmin, direct, tmax);

		color = vec4(-1.0, -1.0, -1.0, -1.0);

		while (rayQueryProceedEXT(query)) {
			if (rayQueryGetIntersectionTypeEXT(query, false)
				== gl_RayQueryCandidateIntersectionTriangleEXT)
			{
				float vx = derivate(params.fun_x, coord.x);
				float vy = derivate(params.fun_y, coord.y);
				float dx = ${DFDX}(coord.x);
				float dy = ${DFDY}(coord.y);
				float dzx = ${DFDX}(coord.z);
				float dzy = ${DFDY}(coord.z);
				float dfx = dzx / dx;
				float dfy = dzy / dy;
				float cx = dfx - vx;
				float cy = dfy - vy;

				color = vec4(cx, cy, sign(dx-abs(cx)), sign(dy-abs(cy)));
			}
			else
			{
				color = vec4(0.0, 0.0, -1.0, -1.0);
			}
			rayQueryConfirmIntersectionEXT(query);
		}
	})");

	std::map<std::string, std::string> m;
	switch (m_params.style)
	{
	case HelperInvocationsParams::DfStyle::Regular:
		m["DFDX"] = "dFdx";
		m["DFDY"] = "dFdy";
		break;
	case HelperInvocationsParams::DfStyle::Coarse:
		m["DFDX"] = "dFdxCoarse";
		m["DFDY"] = "dFdyCoarse";
		break;
	case HelperInvocationsParams::DfStyle::Fine:
		m["DFDX"] = "dFdxFine";
		m["DFDY"] = "dFdyFine";
		break;
	}

	programs.glslSources.add("frag") << glu::FragmentSource(fragmentCode.specialize(m)) << buildOptions;
}

HelperInvocationsInstance::HelperInvocationsInstance (Context& context, const HelperInvocationsParams& params)
	: TestInstance	(context)
	, m_format		(VK_FORMAT_R32G32B32A32_SFLOAT)
	, m_params		(params)
{
}

std::vector<Vec3> HelperInvocationsInstance::createSurface (const Points points, const deUint32 divX, const deUint32 divY, const HelperInvocationsParams::func2D_t& f2D, bool clockWise)
{
	std::vector<Vec3> s;
	const float dx = (points == Points::Vertices ? 2.0f : 1.0f) / float(divX);
	const float dy = (points == Points::Vertices ? 2.0f : 1.0f) / float(divY);
	// Z is always scaled to range (0,1)
	auto z = [&](const deUint32 n, const deUint32 m) -> float
	{
		const float x = float(n) / float(divX);
		const float y = float(m) / float(divY);
		return HelperInvocationsParams::combine(f2D, x,y);
	};
	float y = (points == Points::Vertices) ? -1.0f : 0.0f;
	for (deUint32 j = 0; j < divY; ++j)
	{
		const float ny = ((j + 1) < divY) ? (y + dy) : 1.f;
		float x = (points == Points::Vertices) ? -1.0f : 0.0f;

		for (deUint32 i = 0; i < divX; ++i)
		{
			const float nx = ((i + 1) < divX) ? (x + dx) : 1.f;

			const Vec3	p0( x,  y, z( i,   j   ));
			const Vec3	p1(nx,  y, z( i+1 ,j   ));
			const Vec3	p2(nx, ny, z( i+1, j+1 ));
			const Vec3	p3( x, ny, z( i,   j+1 ));

			if (points == Points::Centers)
			{
				const float cx1 = (p0.x() + p1.x() + p2.x()) / 3.0f;
				const float cy1 = (p0.y() + p1.y() + p2.y()) / 3.0f;
				const float cz1 = (p0.z() + p1.z() + p2.z()) / 3.0f;
				const float cx2 = (p0.x() + p2.x() + p3.x()) / 3.0f;
				const float cy2 = (p0.y() + p2.y() + p3.y()) / 3.0f;
				const float cz2 = (p0.z() + p2.z() + p3.z()) / 3.0f;

				s.emplace_back(cx1, cy1, cz1); s.emplace_back(cx1, cy1, cz1); s.emplace_back(cx1, cy1, cz1);
				s.emplace_back(cx2, cy2, cz2); s.emplace_back(cx2, cy2, cz2); s.emplace_back(cx2, cy2, cz2);
			}
			else if (clockWise)
			{
				s.push_back(p0); s.push_back(p3); s.push_back(p2);
				s.push_back(p0); s.push_back(p2); s.push_back(p1);
			}
			else
			{
				s.push_back(p0); s.push_back(p1); s.push_back(p2);
				s.push_back(p2); s.push_back(p3); s.push_back(p0);
			}

			x = nx;
		}
		y = ny;
	}
	return s;
}

VkImageCreateInfo HelperInvocationsInstance::makeImgInfo (deUint32			queueFamilyIndexCount,
														  const deUint32*	pQueueFamilyIndices) const
{
	const VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	return
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// sType;
		nullptr,								// pNext;
		VkImageCreateFlags(0),					// flags;
		VK_IMAGE_TYPE_2D,						// imageType;
		m_format,								// format;
		{
			m_params.screen.first,
			m_params.screen.second,
			1u
		},										// extent;
		1u,										// mipLevels;
		1u,										// arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					// samples;
		VK_IMAGE_TILING_OPTIMAL,				// tiling;
		usage,									// usage;
		VK_SHARING_MODE_EXCLUSIVE,				// sharingMode;
		queueFamilyIndexCount,					// queueFamilyIndexCount;
		pQueueFamilyIndices,					// pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED				// initialLayout;
	};
}

Move<VkPipeline> HelperInvocationsInstance::makePipeline (const DeviceInterface&	vk,
														  const VkDevice			device,
														  const VkPipelineLayout	pipelineLayout,
														  const VkShaderModule		vertexShader,
														  const VkShaderModule		fragmentShader,
														  const VkRenderPass		renderPass) const
{
	DE_ASSERT(sizeof(Vec3) == mapVkFormat(VK_FORMAT_R32G32B32_SFLOAT).getPixelSize());

	const std::vector<VkViewport>					viewports			{ makeViewport(m_params.screen.first, m_params.screen.second) };
	const std::vector<VkRect2D>						scissors			{ makeRect2D(m_params.screen.first, m_params.screen.second) };

	const VkVertexInputBindingDescription			vertexInputBindingDescription
	{
		0u,														// deUint32             binding
		deUint32(sizeof(Vec3) * 3u),							// deUint32             stride
		VK_VERTEX_INPUT_RATE_VERTEX,							// VkVertexInputRate    inputRate
	};

	const VkVertexInputAttributeDescription			vertexInputAttributeDescription[]
	{
		{
			0u,													// deUint32    location
			0u,													// deUint32    binding
			VK_FORMAT_R32G32B32_SFLOAT,							// VkFormat    format
			0u													// deUint32    offset
		},														// vertices
		{
			1u,													// deUint32    location
			0u,													// deUint32    binding
			VK_FORMAT_R32G32B32_SFLOAT,							// VkFormat    format
			deUint32(sizeof(Vec3))								// deUint32    offset
		},														// coords
		{
			2u,													// deUint32    location
			0u,													// deUint32    binding
			VK_FORMAT_R32G32B32_SFLOAT,							// VkFormat    format
			deUint32(sizeof(Vec3) * 2u)							// deUint32    offset
		}														// centers
	};

	const VkPipelineVertexInputStateCreateInfo		vertexInputStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType                             sType
		nullptr,													// const void*                                 pNext
		(VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags       flags
		1u,															// deUint32                                    vertexBindingDescriptionCount
		&vertexInputBindingDescription,								// const VkVertexInputBindingDescription*      pVertexBindingDescriptions
		DE_LENGTH_OF_ARRAY(vertexInputAttributeDescription),		// deUint32                                    vertexAttributeDescriptionCount
		vertexInputAttributeDescription								// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
	};

	return makeGraphicsPipeline(vk, device, pipelineLayout,
								vertexShader, DE_NULL, DE_NULL, DE_NULL, fragmentShader,
								renderPass, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
								0u, 0u, &vertexInputStateCreateInfo);
}

de::MovePtr<TopLevelAccelerationStructure> HelperInvocationsInstance::createAccStructs (const DeviceInterface&	vk,
																						const VkDevice			device,
																						Allocator&				allocator,
																						const VkCommandBuffer	cmdBuffer,
																						const std::vector<Vec3>	coords) const
{
	const VkAccelerationStructureBuildTypeKHR		buildType	= m_params.buildGPU
																	? VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR
																	: VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR;
	de::MovePtr<TopLevelAccelerationStructure>		tlas		= makeTopLevelAccelerationStructure();
	de::MovePtr<BottomLevelAccelerationStructure>	blas		= makeBottomLevelAccelerationStructure();

	blas->setBuildType(buildType);
	blas->addGeometry(coords, true, VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR);
	blas->createAndBuild(vk, device, cmdBuffer, allocator);

	tlas->setBuildType(buildType);
	tlas->addInstance(de::SharedPtr<BottomLevelAccelerationStructure>(blas.release()));
	tlas->createAndBuild(vk, device, cmdBuffer, allocator);

	return tlas;
}

de::MovePtr<BufferWithMemory> HelperInvocationsInstance::makeAttribBuff	(const DeviceInterface&		vk,
																		 const VkDevice				device,
																		 Allocator&					allocator,
																		 const std::vector<Vec3>&	vertices,
																		 const std::vector<Vec3>&	coords,
																		 const std::vector<Vec3>&	centers) const
{
	DE_ASSERT(sizeof(Vec3) == mapVkFormat(VK_FORMAT_R32G32B32_SFLOAT).getPixelSize());
	const deUint32					count				= deUint32(vertices.size());
	DE_ASSERT( count && (count == coords.size()) && (count == centers.size()) );
	const VkDeviceSize				bufferSize			= 3 * count * sizeof(Vec3);
	const VkBufferCreateInfo		bufferCreateInfo	= makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	de::MovePtr<BufferWithMemory>	buffer				(new BufferWithMemory(vk, device, allocator, bufferCreateInfo, MemoryRequirement::Coherent | MemoryRequirement::HostVisible));

	Allocation&						allocation			= buffer->getAllocation();
	Vec3*							data				= static_cast<Vec3*>(allocation.getHostPtr());
	for (deUint32 c = 0; c < count; ++c)
	{
		data[3*c] = vertices.at(c);
		data[3*c+1] = coords.at(c);
		data[3*c+2] = centers.at(c);
	}
	flushMappedMemoryRange(vk, device, allocation.getMemory(), 0u, bufferSize);

	return buffer;
}

de::MovePtr<BufferWithMemory> HelperInvocationsInstance::makeResultBuff	(const DeviceInterface& vk,
																		 const VkDevice			device,
																		 Allocator&				allocator) const
{
	const TextureFormat				texFormat			= mapVkFormat(m_format);
	const VkDeviceSize				bufferSize			= (m_params.screen.first * m_params.screen.second * texFormat.getPixelSize());
	const VkBufferCreateInfo		bufferCreateInfo	= makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	de::MovePtr<BufferWithMemory>	buffer				(new BufferWithMemory(vk, device, allocator, bufferCreateInfo, MemoryRequirement::Coherent | MemoryRequirement::HostVisible));

	Allocation&						allocation			= buffer->getAllocation();
	PixelBufferAccess				pixels				(texFormat, m_params.screen.first, m_params.screen.second, 1u, allocation.getHostPtr());

	for (deUint32 y = 0; y < m_params.screen.second; ++y)
	{
		for (deUint32 x = 0; x < m_params.screen.first; ++x)
		{
			pixels.setPixel(Vec4(0.0f, 0.0f, 0.0f, -1.0f), x, y);
		}
	}
	flushMappedMemoryRange(vk, device, allocation.getMemory(), 0u, bufferSize);

	return buffer;
}

bool HelperInvocationsInstance::verifyResult (const DeviceInterface&	vk,
											  const VkDevice			device,
											  const BufferWithMemory&	buffer) const
{
	int						invalid	= 0;
	Allocation&				alloc	= buffer.getAllocation();
	invalidateMappedMemoryRange(vk, device, alloc.getMemory(), 0u, VK_WHOLE_SIZE);
	ConstPixelBufferAccess	pixels	(mapVkFormat(m_format), m_params.screen.first, m_params.screen.second, 1u, alloc.getHostPtr());

	for (deUint32 y = 0; y < m_params.screen.second; ++y)
	{
		for (deUint32 x = 0; x < m_params.screen.first; ++x)
		{
			const Vec4 px = pixels.getPixel(x,y);
			if (px.z() < 0.0f || px.w() < 0.0f)
				invalid += 1;
		}
	}

	return (0 == invalid);
}

VkWriteDescriptorSetAccelerationStructureKHR makeAccStructDescriptorWrite (const VkAccelerationStructureKHR* ptr, deUint32 count = 1u)
{
	return {
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	//	VkStructureType						sType;
		nullptr,															//	const void*							pNext;
		count,																//	deUint32							accelerationStructureCount;
		ptr};																//	const VkAccelerationStructureKHR*	pAccelerationStructures;
};

TestStatus HelperInvocationsInstance::iterate (void)
{
	const VkDevice							device						= m_context.getDevice();
	const DeviceInterface&					vk							= m_context.getDeviceInterface();
	Allocator&								allocator					= m_context.getDefaultAllocator();
	const deUint32							queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
	const VkQueue							queue						= m_context.getUniversalQueue();

	const VkRect2D							renderArea					= makeRect2D(m_params.screen.first, m_params.screen.second);
	const VkImageCreateInfo					imageCreateInfo				= makeImgInfo(1, &queueFamilyIndex);
	const de::MovePtr<ImageWithMemory>		image						(new ImageWithMemory(vk, device, allocator, imageCreateInfo, MemoryRequirement::Any));
	const VkImageSubresourceRange			imageSubresourceRange		= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0, 1u);
	const Move<VkImageView>					view						= makeImageView(vk, device, **image, VK_IMAGE_VIEW_TYPE_2D, m_format, imageSubresourceRange);
	const Move<VkRenderPass>				renderPass					= makeRenderPass(vk, device, m_format);
	const Move<VkFramebuffer>				frameBuffer					= makeFramebuffer(vk, device, *renderPass, *view, m_params.screen.first, m_params.screen.second);
	const de::MovePtr<BufferWithMemory>		resultBuffer				= makeResultBuff(vk, device, allocator);
	const VkImageSubresourceLayers			imageSubresourceLayers		= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const VkBufferImageCopy					bufferCopyImageRegion		= makeBufferImageCopy(makeExtent3D(UVec3(m_params.screen.first, m_params.screen.second, 1u)), imageSubresourceLayers);

	const HelperInvocationsParams::func2D_t	funcs						= m_params.mode.funcs;
	struct PushConstants
	{
		int fun_x, fun_y;
	} const                                 pushConstants				{ m_params.mode.types.first, m_params.mode.types.second };
	const VkPushConstantRange				pushConstantRange			{ VK_SHADER_STAGE_FRAGMENT_BIT, 0u, uint32_t(sizeof(pushConstants)) };
	const std::vector<Vec3>					vertices					= createSurface(Points::Vertices, m_params.model.first, m_params.model.second, funcs);
	const std::vector<Vec3>					coords						= createSurface(Points::Coords, m_params.model.first, m_params.model.second, funcs);
	const std::vector<Vec3>					centers						= createSurface(Points::Centers, m_params.model.first, m_params.model.second, funcs);
	const de::MovePtr<BufferWithMemory>		attribBuffer				= makeAttribBuff(vk, device, allocator, vertices, coords, centers);

	TopLevelAccelerationStructurePtr		topAccStruct				{};
	Move<VkDescriptorSetLayout>				descriptorLayout			= DescriptorSetLayoutBuilder()
																			.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_FRAGMENT_BIT)
																			.build(vk, device);
	Move<VkDescriptorPool>					descriptorPool				= DescriptorPoolBuilder()
																			.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
																			.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	Move<VkDescriptorSet>					descriptorSet				= makeDescriptorSet(vk, device, *descriptorPool, *descriptorLayout);

	Move<VkShaderModule>					vertexShader				= createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u);
	Move<VkShaderModule>					fragmentShader				= createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u);
	Move<VkPipelineLayout>					pipelineLayout				= makePipelineLayout(vk, device, 1u, &descriptorLayout.get(), 1u, &pushConstantRange);
	Move<VkPipeline>						pipeline					= makePipeline(vk, device, *pipelineLayout, *vertexShader, *fragmentShader, *renderPass);
	const Move<VkCommandPool>				cmdPool						= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	const Move<VkCommandBuffer>				cmdBuffer					= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	const Vec4								clearColor					( 0.1f, 0.2f, 0.3f, 0.4f );
	const VkImageMemoryBarrier				postDrawImageBarrier		= makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
																								 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
																								 **image, imageSubresourceRange);
	const VkMemoryBarrier					postCopyMemoryBarrier		= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);

	beginCommandBuffer(vk, *cmdBuffer, 0u);

		topAccStruct = createAccStructs(vk, device, allocator, *cmdBuffer, coords);
		const auto accStructWrite = makeAccStructDescriptorWrite(topAccStruct->getPtr());
		DescriptorSetUpdateBuilder().writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
												 VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accStructWrite).update(vk, device);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &static_cast<const VkBuffer&>(**attribBuffer), &static_cast<const VkDeviceSize&>(0u));
		vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0u, uint32_t(sizeof(pushConstants)), &pushConstants);
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, nullptr);

		beginRenderPass(vk, *cmdBuffer, *renderPass, *frameBuffer, renderArea, clearColor);
			vk.cmdDraw(*cmdBuffer, uint32_t(vertices.size()), 1u, 0u, 0u);
		endRenderPass(vk, *cmdBuffer);

		cmdPipelineImageMemoryBarrier(vk, *cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &postDrawImageBarrier);
		vk.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **resultBuffer, 1u, &bufferCopyImageRegion);
		cmdPipelineMemoryBarrier(vk, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &postCopyMemoryBarrier);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	return verifyResult(vk, device, *resultBuffer) ? TestStatus::pass("") : TestStatus::fail("");
}

} // anonymous

TestCaseGroup* addHelperInvocationsTests(TestContext& testCtx)
{
	std::pair<bool, const char*> const builds[]
	{
		{ true,		"gpu"	},
		{ false,	"cpu"	}
	};

	std::pair<HelperInvocationsParams::DfStyle, const char*> const styles[]
	{
		{ HelperInvocationsParams::Regular,	"regular"	},
		{ HelperInvocationsParams::Coarse,	"coarse"	},
		{ HelperInvocationsParams::Fine,	"fine"		}
	};

	std::pair<HelperInvocationsParams::test_mode_t, const char*> const modes[] =
	{
		{ HelperInvocationsParams::MODE_LINEAR_QUADRATIC	, "linear_quadratic"	},
		{ HelperInvocationsParams::MODE_LINEAR_CUBIC		, "linear_cubic"		},
		{ HelperInvocationsParams::MODE_CUBIC_QUADRATIC		, "cubic_quadratic"		},
#ifdef ENABLE_ALL_HELPER_COMBINATIONS
		{ HelperInvocationsParams::MODE_LINEAR_LINEAR		, "linear_linear"		},
		{ HelperInvocationsParams::MODE_QUADRATIC_LINEAR	, "quadratic_linear"	},
		{ HelperInvocationsParams::MODE_QUADRATIC_QUADRATIC	, "quadratic_quadratic"	},
		{ HelperInvocationsParams::MODE_QUADRATIC_CUBIC		, "quadratic_cubic"		},
		{ HelperInvocationsParams::MODE_CUBIC_LINEAR		, "cubic_linear"		},
		{ HelperInvocationsParams::MODE_CUBIC_CUBIC			, "cubic_cubic"			},
#endif
	};

	std::pair<deUint32, deUint32> const screens[]
	{
		{ 64, 64 }, { 32, 64 }
	};

	std::pair<deUint32, deUint32> const models[]
	{
		{ 64, 64 }, { 64, 32 }
	};

	auto makeTestName = [](const std::pair<deUint32, deUint32>& d) -> std::string
	{
		return std::to_string(d.first) + "x" + std::to_string(d.second);
	};

	auto rootGroup = new TestCaseGroup(testCtx, "helper_invocations", "Ray query helper invocation tests");
	for (auto& build : builds)
	{
		auto buildGroup = new tcu::TestCaseGroup(testCtx, build.second, "");
		for (auto& style : styles)
		{
			auto styleGroup = new tcu::TestCaseGroup(testCtx, style.second, "");
			for (auto& mode : modes)
			{
				auto modeGroup = new tcu::TestCaseGroup(testCtx, mode.second, "");
				for (auto& screen : screens)
				{
					auto screenGroup = new TestCaseGroup(testCtx, makeTestName(screen).c_str(), "");
					for (auto& model : models)
					{
						HelperInvocationsParams p;
						p.mode		= mode.first;
						p.screen	= screen;
						p.model		= model;
						p.style		= style.first;
						p.buildGPU	= build.first;

						screenGroup->addChild(new HelperInvocationsCase(testCtx, p, makeTestName(model)));
					}
					modeGroup->addChild(screenGroup);
				}
				styleGroup->addChild(modeGroup);
			}
			buildGroup->addChild(styleGroup);
		}
		rootGroup->addChild(buildGroup);
	}
	return rootGroup;
}

tcu::TestCaseGroup*	createMiscTests	(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "misc", "Miscellaneous ray query tests"));

	group->addChild(new DynamicIndexingCase(testCtx, "dynamic_indexing", "Dynamic indexing of ray queries"));

	return group.release();
}

} // RayQuery
} // vkt

