/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Valve Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Tests using non-uniform arguments with traceRayExt().
 *//*--------------------------------------------------------------------*/

#include "vktRayQueryNonUniformArgsTests.hpp"
#include "vktTestCase.hpp"

#include "vkRayTracingUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuTestLog.hpp"

#include <vector>
#include <iostream>

namespace vkt
{
namespace RayQuery
{
namespace
{

using namespace vk;

// Causes for hitting the miss shader due to argument values.
enum class MissCause
{
	NONE = 0,
	FLAGS,
	CULL_MASK,
	ORIGIN,
	TMIN,
	DIRECTION,
	TMAX,
	CAUSE_COUNT,
};

struct NonUniformParams
{
	MissCause	missCause;
};

class NonUniformArgsCase : public TestCase
{
public:
							NonUniformArgsCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const NonUniformParams& params);
	virtual					~NonUniformArgsCase		(void) {}

	virtual void			checkSupport			(Context& context) const;
	virtual void			initPrograms			(vk::SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance			(Context& context) const;

protected:
	NonUniformParams		m_params;
};

class NonUniformArgsInstance : public TestInstance
{
public:
								NonUniformArgsInstance	(Context& context, const NonUniformParams& params);
	virtual						~NonUniformArgsInstance	(void) {}

	virtual tcu::TestStatus		iterate					(void);

protected:
	NonUniformParams			m_params;
};

NonUniformArgsCase::NonUniformArgsCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const NonUniformParams& params)
	: TestCase	(testCtx, name, description)
	, m_params	(params)
{}

void NonUniformArgsCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_ray_query");
}

struct ArgsBufferData
{
	tcu::Vec4	origin;
	tcu::Vec4	direction;
	float		Tmin;
	float		Tmax;
	deUint32	rayFlags;
	deUint32	cullMask;
};

void NonUniformArgsCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	std::ostringstream comp;
	comp
		<< "#version 460 core\n"
		<< "#extension GL_EXT_ray_query : require\n"
		<< "\n"
		<< "layout(local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
		<< "\n"
		<< "layout(set=0, binding=0) uniform accelerationStructureEXT topLevelAS;\n"
		<< "layout(set=0, binding=1, std430) buffer ArgumentsBlock {\n" // Must match ArgsBufferData.
		<< "  vec4  origin;\n"
		<< "  vec4  direction;\n"
		<< "  float Tmin;\n"
		<< "  float Tmax;\n"
		<< "  uint  rayFlags;\n"
		<< "  uint  cullMask;\n"
		<< "} args;\n"
		<< "layout(set=0, binding=2, std430) buffer ResultBlock {\n"
		<< "  uint candidateFound;\n"
		<< "} result;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "  uint candidateFoundVal = 0u;\n"
		<< "  rayQueryEXT rq;\n"
		<< "  rayQueryInitializeEXT(rq, topLevelAS, args.rayFlags, args.cullMask, args.origin.xyz, args.Tmin, args.direction.xyz, args.Tmax);\n"
		<< "  while (rayQueryProceedEXT(rq)) {\n"
		<< "    const uint candidateType = rayQueryGetIntersectionTypeEXT(rq, false);\n"
		<< "    if (candidateType == gl_RayQueryCandidateIntersectionTriangleEXT) {\n"
		<< "      candidateFoundVal = 1u;\n"
		<< "    }\n"
		<< "    else if (candidateType == gl_RayQueryCandidateIntersectionAABBEXT) {\n"
		<< "      candidateFoundVal = 2u;\n"
		<< "      break;\n"
		<< "    }\n"
		<< "    else {\n"
		<< "      candidateFoundVal = 3u;\n"
		<< "      break;\n"
		<< "    }\n"
		<< "  }\n"
		<< "  result.candidateFound = candidateFoundVal;\n"
		<< "}\n";
		;

	programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str()) << buildOptions;
}

TestInstance* NonUniformArgsCase::createInstance (Context& context) const
{
	return new NonUniformArgsInstance(context, m_params);
}

NonUniformArgsInstance::NonUniformArgsInstance (Context& context, const NonUniformParams& params)
	: TestInstance	(context)
	, m_params		(params)
{}

tcu::TestStatus NonUniformArgsInstance::iterate (void)
{
	const auto&	vkd		= m_context.getDeviceInterface();
	const auto	device	= m_context.getDevice();
	auto&		alloc	= m_context.getDefaultAllocator();
	const auto	qIndex	= m_context.getUniversalQueueFamilyIndex();
	const auto	queue	= m_context.getUniversalQueue();
	const auto	stages	= VK_SHADER_STAGE_COMPUTE_BIT;

	// Geometry data constants.
	const std::vector<tcu::Vec3> kOffscreenTriangle =
	{
		// Triangle around (x=0, y=2) z=-5
		tcu::Vec3( 0.0f, 2.5f, -5.0f),
		tcu::Vec3(-0.5f, 1.5f, -5.0f),
		tcu::Vec3( 0.5f, 1.5f, -5.0f),
	};
	const std::vector<tcu::Vec3> kOnscreenTriangle =
	{
		// Triangle around (x=0, y=2) z=5
		tcu::Vec3( 0.0f, 2.5f, 5.0f),
		tcu::Vec3(-0.5f, 1.5f, 5.0f),
		tcu::Vec3( 0.5f, 1.5f, 5.0f),
	};
	const tcu::Vec4		kGoodOrigin		(0.0f, 2.0f, 0.0f, 0.0f);	// Around (x=0, y=2) z=0.
	const tcu::Vec4		kBadOrigin		(0.0f, 8.0f, 0.0f, 0.0f);	// Too high, around (x=0, y=8) depth 0.
	const tcu::Vec4		kGoodDirection	(0.0f, 0.0f, 1.0f, 0.0f);	// Towards +z.
	const tcu::Vec4		kBadDirection	(1.0f, 0.0f, 0.0f, 0.0f);	// Towards +x.
	const float			kGoodTmin		= 4.0f;						// Good to travel from z=0 to z=5.
	const float			kGoodTmax		= 6.0f;						// Ditto.
	const float			kBadTmin		= 5.5f;						// Tmin after triangle.
	const float			kBadTmax		= 4.5f;						// Tmax before triangle.
	const deUint32		kGoodFlags		= 0u;						// MaskNone
	const deUint32		kBadFlags		= 256u;						// SkipTrianglesKHR
	const deUint32		kGoodCullMask	= 0x0Fu;					// Matches instance.
	const deUint32		kBadCullMask	= 0xF0u;					// Does not match instance.

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, qIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);

	// Build acceleration structures.
	auto topLevelAS		= makeTopLevelAccelerationStructure();
	auto bottomLevelAS	= makeBottomLevelAccelerationStructure();

	// Putting the offscreen triangle first makes sure hits have a geometryIndex=1, meaning sbtRecordStride matters.
	std::vector<const std::vector<tcu::Vec3>*> geometries;
	geometries.push_back(&kOffscreenTriangle);
	geometries.push_back(&kOnscreenTriangle);

	for (const auto& geometryPtr : geometries)
		bottomLevelAS->addGeometry(*geometryPtr, true /* is triangles */);

	bottomLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc);

	de::SharedPtr<BottomLevelAccelerationStructure> blasSharedPtr (bottomLevelAS.release());
	topLevelAS->setInstanceCount(1);
	topLevelAS->addInstance(blasSharedPtr, identityMatrix3x4, 0u, kGoodCullMask, 0u, VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR);
	topLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc);

	// Input storage buffer.
	const auto			inputBufferSize		= static_cast<VkDeviceSize>(sizeof(ArgsBufferData));
	const auto			inputBufferInfo		= makeBufferCreateInfo(inputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory	inputBuffer			(vkd, device, alloc, inputBufferInfo, MemoryRequirement::HostVisible);
	auto&				inputBufferAlloc	= inputBuffer.getAllocation();

	// Output storage buffer.
	const auto			outputBufferSize	= static_cast<VkDeviceSize>(sizeof(deUint32));
	const auto			outputBufferInfo	= makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory	outputBuffer		(vkd, device, alloc, outputBufferInfo, MemoryRequirement::HostVisible);
	auto&				outputBufferAlloc	= outputBuffer.getAllocation();

	// Fill output buffer with an initial invalid value.
	deMemset(outputBufferAlloc.getHostPtr(), 42, static_cast<size_t>(outputBufferSize));
	flushAlloc(vkd, device, outputBufferAlloc);

	// Descriptor set layout and pipeline layout.
	DescriptorSetLayoutBuilder setLayoutBuilder;
	setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, stages);
	setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stages);
	setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stages);
	const auto setLayout		= setLayoutBuilder.build(vkd, device);
	const auto pipelineLayout	= makePipelineLayout(vkd, device, setLayout.get());

	// Descriptor pool and set.
	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u);
	const auto descriptorPool	= poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const auto descriptorSet	= makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get());

	// Update descriptor set.
	{
		const VkWriteDescriptorSetAccelerationStructureKHR accelDescInfo =
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
			nullptr,
			1u,
			topLevelAS.get()->getPtr(),
		};

		const auto inputBufferDescInfo	= makeDescriptorBufferInfo(inputBuffer.get(), 0ull, VK_WHOLE_SIZE);
		const auto outputBufferDescInfo	= makeDescriptorBufferInfo(outputBuffer.get(), 0ull, VK_WHOLE_SIZE);

		DescriptorSetUpdateBuilder updateBuilder;
		updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelDescInfo);
		updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &inputBufferDescInfo);
		updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputBufferDescInfo);
		updateBuilder.update(vkd, device);
	}

	// Shader modules.
	const auto compModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0u);

	// Generate ids for the closest hit and miss shaders according to the test parameters.
	DE_ASSERT(geometries.size() > 0u);

	const VkPipelineShaderStageCreateInfo stageCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,												//	const void*							pNext;
		0u,														//	VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_COMPUTE_BIT,							//	VkShaderStageFlagBits				stage;
		compModule.get(),										//	VkShaderModule						module;
		"main",													//	const char*							pName;
		nullptr,												//	const VkSpecializationInfo*			pSpecializationInfo;
	};

	const VkComputePipelineCreateInfo pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,			//	VkStructureType					sType;
		nullptr,												//	const void*						pNext;
		0u,														//	VkPipelineCreateFlags			flags;
		stageCreateInfo,										//	VkPipelineShaderStageCreateInfo	stage;
		pipelineLayout.get(),									//	VkPipelineLayout				layout;
		DE_NULL,												//	VkPipeline						basePipelineHandle;
		0,														//	deInt32							basePipelineIndex;
	};

	const auto pipeline = createComputePipeline(vkd, device, DE_NULL, &pipelineCreateInfo);

	// Fill input buffer values.
	{
		const ArgsBufferData argsBufferData =
		{
			((m_params.missCause == MissCause::ORIGIN)		? kBadOrigin	: kGoodOrigin),
			((m_params.missCause == MissCause::DIRECTION)	? kBadDirection	: kGoodDirection),
			((m_params.missCause == MissCause::TMIN)		? kBadTmin		: kGoodTmin),
			((m_params.missCause == MissCause::TMAX)		? kBadTmax		: kGoodTmax),
			((m_params.missCause == MissCause::FLAGS)		? kBadFlags		: kGoodFlags),
			((m_params.missCause == MissCause::CULL_MASK)	? kBadCullMask	: kGoodCullMask),
		};

		deMemcpy(inputBufferAlloc.getHostPtr(), &argsBufferData, sizeof(argsBufferData));
		flushAlloc(vkd, device, inputBufferAlloc);
	}

	// Trace rays.
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);

	// Barrier for the output buffer.
	const auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &bufferBarrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Check output value.
	invalidateAlloc(vkd, device, outputBufferAlloc);
	deUint32 outputVal = std::numeric_limits<deUint32>::max();
	deMemcpy(&outputVal, outputBufferAlloc.getHostPtr(), sizeof(outputVal));
	const auto expectedVal = ((m_params.missCause == MissCause::NONE) ? 1u : 0u);

	std::ostringstream msg;
	msg << "Output value: " << outputVal << " (expected " << expectedVal << ")";

	if (outputVal != expectedVal)
		return tcu::TestStatus::fail(msg.str());

	auto& log = m_context.getTestContext().getLog();
	log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;

	return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup*	createNonUniformArgsTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> nonUniformGroup(new tcu::TestCaseGroup(testCtx, "non_uniform_args", "Test non-uniform arguments in traceRayExt()"));

	NonUniformParams params;
	for (int causeIdx = static_cast<int>(MissCause::NONE); causeIdx < static_cast<int>(MissCause::CAUSE_COUNT); ++causeIdx)
	{
		params.missCause = static_cast<MissCause>(causeIdx);
		const std::string testName = ((params.missCause == MissCause::NONE) ? std::string("no_miss") : "miss_cause_" + de::toString(causeIdx));
		nonUniformGroup->addChild(new NonUniformArgsCase(testCtx, testName, "", params));
	}

	return nonUniformGroup.release();
}

}	// RayQuery
}	// vkt
