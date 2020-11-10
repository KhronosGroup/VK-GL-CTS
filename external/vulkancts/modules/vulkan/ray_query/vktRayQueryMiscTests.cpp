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

#include "tcuVector.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"

#include <sstream>
#include <limits>
#include <vector>

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
	static constexpr deUint32	kLocalSizeX	= 128u;
	static constexpr deUint32	kNumQueries	= 128u;

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

} // anonymous

tcu::TestCaseGroup*	createMiscTests	(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "misc", "Miscellaneous ray query tests"));

	group->addChild(new DynamicIndexingCase(testCtx, "dynamic_indexing", "Dynamic indexing of ray queries"));

	return group.release();
}

} // RayQuery
} // vkt

