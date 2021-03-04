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

#include "vktRayTracingNonUniformArgsTests.hpp"
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
namespace RayTracing
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
	bool miss;

	struct
	{
		deUint32	rayTypeCount;
		deUint32	rayType;
	} hitParams;

	struct
	{
		MissCause	missCause;
		deUint32	missIndex;
	} missParams;
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
	context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
}

struct ArgsBufferData
{
	tcu::Vec4	origin;
	tcu::Vec4	direction;
	float		Tmin;
	float		Tmax;
	deUint32	rayFlags;
	deUint32	cullMask;
	deUint32	sbtRecordOffset;
	deUint32	sbtRecordStride;
	deUint32	missIndex;
};

void NonUniformArgsCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	std::ostringstream descriptors;
	descriptors
		<< "layout(set=0, binding=0) uniform accelerationStructureEXT topLevelAS;\n"
		<< "layout(set=0, binding=1, std430) buffer ArgumentsBlock {\n" // Must match ArgsBufferData.
		<< "  vec4  origin;\n"
		<< "  vec4  direction;\n"
		<< "  float Tmin;\n"
		<< "  float Tmax;\n"
		<< "  uint  rayFlags;\n"
		<< "  uint  cullMask;\n"
		<< "  uint  sbtRecordOffset;\n"
		<< "  uint  sbtRecordStride;\n"
		<< "  uint  missIndex;\n"
		<< "} args;\n"
		<< "layout(set=0, binding=2, std430) buffer ResultBlock {\n"
		<< "  uint shaderId;\n"
		<< "} result;\n"
		;
	const auto descriptorsStr = descriptors.str();

	std::ostringstream rgen;
	rgen
		<< "#version 460 core\n"
		<< "#extension GL_EXT_ray_tracing : require\n"
		<< "\n"
		<< descriptorsStr
		<< "layout(location=0) rayPayloadEXT vec4 unused;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "  traceRayEXT(topLevelAS,\n"
		<< "    args.rayFlags,\n"
		<< "    args.cullMask,\n"
		<< "    args.sbtRecordOffset,\n"
		<< "    args.sbtRecordStride,\n"
		<< "    args.missIndex,\n"
		<< "    args.origin.xyz,\n"
		<< "    args.Tmin,\n"
		<< "    args.direction.xyz,\n"
		<< "    args.Tmax,\n"
		<< "    0);\n"
		<< "}\n";
		;

	std::ostringstream chit;
	chit
		<< "#version 460 core\n"
		<< "#extension GL_EXT_ray_tracing : require\n"
		<< "\n"
		<< descriptorsStr
		<< "layout(constant_id=0) const uint chitShaderId = 0;\n"
		<< "layout(location=0) rayPayloadInEXT vec4 unused;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "  result.shaderId = chitShaderId;\n"
		<< "}\n";
		;

	std::ostringstream miss;
	miss
		<< "#version 460 core\n"
		<< "#extension GL_EXT_ray_tracing : require\n"
		<< "\n"
		<< descriptorsStr
		<< "layout(constant_id=0) const uint missShaderId = 0;\n"
		<< "layout(location=0) rayPayloadInEXT vec4 unused;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "  result.shaderId = missShaderId;\n"
		<< "}\n";
		;

	programCollection.glslSources.add("rgen") << glu::RaygenSource(rgen.str()) << buildOptions;
	programCollection.glslSources.add("chit") << glu::ClosestHitSource(chit.str()) << buildOptions;
	programCollection.glslSources.add("miss") << glu::MissSource(miss.str()) << buildOptions;
}

TestInstance* NonUniformArgsCase::createInstance (Context& context) const
{
	return new NonUniformArgsInstance(context, m_params);
}

NonUniformArgsInstance::NonUniformArgsInstance (Context& context, const NonUniformParams& params)
	: TestInstance	(context)
	, m_params		(params)
{}

deUint32 joinMostLeast (deUint32 most, deUint32 least)
{
	constexpr auto kMaxUint16 = static_cast<deUint32>(std::numeric_limits<deUint16>::max());
	DE_UNREF(kMaxUint16); // For release builds.
	DE_ASSERT(most <= kMaxUint16 && least <= kMaxUint16);
	return ((most << 16) | least);
}

deUint32 makeMissId (deUint32 missIndex)
{
	// 1 on the highest 16 bits for miss shaders.
	return joinMostLeast(1u, missIndex);
}

deUint32 makeChitId (deUint32 chitIndex)
{
	// 2 on the highest 16 bits for closest hit shaders.
	return joinMostLeast(2u, chitIndex);
}

tcu::TestStatus NonUniformArgsInstance::iterate (void)
{
	const auto&	vki		= m_context.getInstanceInterface();
	const auto	physDev	= m_context.getPhysicalDevice();
	const auto&	vkd		= m_context.getDeviceInterface();
	const auto	device	= m_context.getDevice();
	auto&		alloc	= m_context.getDefaultAllocator();
	const auto	qIndex	= m_context.getUniversalQueueFamilyIndex();
	const auto	queue	= m_context.getUniversalQueue();
	const auto	stages	= (VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR);

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

	// Fill output buffer with an initial value.
	deMemset(outputBufferAlloc.getHostPtr(), 0, static_cast<size_t>(outputBufferSize));
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
	auto rgenModule = makeVkSharedPtr(createShaderModule(vkd, device, m_context.getBinaryCollection().get("rgen"), 0));
	auto missModule = makeVkSharedPtr(createShaderModule(vkd, device, m_context.getBinaryCollection().get("miss"), 0));
	auto chitModule = makeVkSharedPtr(createShaderModule(vkd, device, m_context.getBinaryCollection().get("chit"), 0));

	// Get some ray tracing properties.
	deUint32 shaderGroupHandleSize		= 0u;
	deUint32 shaderGroupBaseAlignment	= 1u;
	{
		const auto rayTracingPropertiesKHR	= makeRayTracingProperties(vki, physDev);
		shaderGroupHandleSize				= rayTracingPropertiesKHR->getShaderGroupHandleSize();
		shaderGroupBaseAlignment			= rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
	}

	// Create raytracing pipeline and shader binding tables.
	Move<VkPipeline>				pipeline;

	de::MovePtr<BufferWithMemory>	raygenSBT;
	de::MovePtr<BufferWithMemory>	missSBT;
	de::MovePtr<BufferWithMemory>	hitSBT;
	de::MovePtr<BufferWithMemory>	callableSBT;

	VkStridedDeviceAddressRegionKHR	raygenSBTRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR	missSBTRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR	hitSBTRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR	callableSBTRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

	// Generate ids for the closest hit and miss shaders according to the test parameters.
	DE_ASSERT(m_params.hitParams.rayTypeCount > 0u);
	DE_ASSERT(m_params.hitParams.rayType < m_params.hitParams.rayTypeCount);
	DE_ASSERT(geometries.size() > 0u);

	std::vector<deUint32> missShaderIds;
	for (deUint32 missIdx = 0; missIdx <= m_params.missParams.missIndex; ++missIdx)
		missShaderIds.push_back(makeMissId(missIdx));

	deUint32				chitCounter		= 0u;
	std::vector<deUint32>	chitShaderIds;

	for (size_t geoIdx = 0; geoIdx < geometries.size(); ++geoIdx)
	for (deUint32 rayIdx = 0; rayIdx < m_params.hitParams.rayTypeCount; ++rayIdx)
		chitShaderIds.push_back(makeChitId(chitCounter++));

	{
		const auto						rayTracingPipeline		= de::newMovePtr<RayTracingPipeline>();
		const VkSpecializationMapEntry	specializationMapEntry	=
		{
			0u,											//	deUint32	constantID;
			0u,											//	deUint32	offset;
			static_cast<deUintptr>(sizeof(deUint32)),	//	deUintptr	size;
		};
		VkSpecializationInfo			specInfo				=
		{
			1u,											//	deUint32						mapEntryCount;
			&specializationMapEntry,					//	const VkSpecializationMapEntry*	pMapEntries;
			static_cast<deUintptr>(sizeof(deUint32)),	//	deUintptr						dataSize;
			nullptr,									//	const void*						pData;
		};

		std::vector<VkSpecializationInfo> specInfos;
		specInfos.reserve(missShaderIds.size() + chitShaderIds.size());

		deUint32 shaderGroupIdx = 0u;
		rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenModule, shaderGroupIdx++);

		for (size_t missIdx = 0; missIdx < missShaderIds.size(); ++missIdx)
		{
			specInfo.pData = &missShaderIds.at(missIdx);
			specInfos.push_back(specInfo);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, missModule, shaderGroupIdx++, &specInfos.back());
		}

		const auto firstChitGroup = shaderGroupIdx;

		for (size_t chitIdx = 0; chitIdx < chitShaderIds.size(); ++chitIdx)
		{
			specInfo.pData = &chitShaderIds.at(chitIdx);
			specInfos.push_back(specInfo);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, chitModule, shaderGroupIdx++, &specInfos.back());
		}

		pipeline = rayTracingPipeline->createPipeline(vkd, device, pipelineLayout.get());

		raygenSBT		= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 0u, 1u);
		raygenSBTRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenSBT->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);

		missSBT			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 1u, static_cast<deUint32>(missShaderIds.size()));
		missSBTRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missSBT->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize * missShaderIds.size());

		hitSBT			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, firstChitGroup, static_cast<deUint32>(chitShaderIds.size()));
		hitSBTRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitSBT->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize * chitShaderIds.size());
	}

	// Fill input buffer values.
	{
		DE_ASSERT(!(m_params.miss && m_params.missParams.missCause == MissCause::NONE));

		const ArgsBufferData argsBufferData =
		{
			((m_params.miss && m_params.missParams.missCause == MissCause::ORIGIN)		? kBadOrigin	: kGoodOrigin),
			((m_params.miss && m_params.missParams.missCause == MissCause::DIRECTION)	? kBadDirection	: kGoodDirection),
			((m_params.miss && m_params.missParams.missCause == MissCause::TMIN)		? kBadTmin		: kGoodTmin),
			((m_params.miss && m_params.missParams.missCause == MissCause::TMAX)		? kBadTmax		: kGoodTmax),
			((m_params.miss && m_params.missParams.missCause == MissCause::FLAGS)		? kBadFlags		: kGoodFlags),
			((m_params.miss && m_params.missParams.missCause == MissCause::CULL_MASK)	? kBadCullMask	: kGoodCullMask),
			m_params.hitParams.rayType,
			m_params.hitParams.rayTypeCount,
			m_params.missParams.missIndex,
		};

		deMemcpy(inputBufferAlloc.getHostPtr(), &argsBufferData, sizeof(argsBufferData));
		flushAlloc(vkd, device, inputBufferAlloc);
	}

	// Trace rays.
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdTraceRaysKHR(cmdBuffer, &raygenSBTRegion, &missSBTRegion, &hitSBTRegion, &callableSBTRegion, 1u, 1u, 1u);

	// Barrier for the output buffer.
	const auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &bufferBarrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Check output value.
	invalidateAlloc(vkd, device, outputBufferAlloc);
	deUint32 outputVal = std::numeric_limits<deUint32>::max();
	deMemcpy(&outputVal, outputBufferAlloc.getHostPtr(), sizeof(outputVal));
	const auto expectedVal = (m_params.miss ? makeMissId(m_params.missParams.missIndex) : makeChitId(m_params.hitParams.rayTypeCount + m_params.hitParams.rayType));

	std::ostringstream msg;
	msg << "Output value: 0x" << std::hex << outputVal << " (expected 0x" << expectedVal << ")";

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

	// Closest hit cases.
	{
		NonUniformParams params;
		params.miss = false;
		params.missParams.missIndex = 0u;
		params.missParams.missCause = MissCause::NONE;

		for (deUint32 typeCount = 1u; typeCount <= 4u; ++typeCount)
		{
			params.hitParams.rayTypeCount = typeCount;
			for (deUint32 rayType = 0u; rayType < typeCount; ++rayType)
			{
				params.hitParams.rayType = rayType;
				nonUniformGroup->addChild(new NonUniformArgsCase(testCtx, "chit_" + de::toString(typeCount) + "_types_" + de::toString(rayType), "", params));
			}
		}
	}

	// Miss cases.
	{
		NonUniformParams params;
		params.miss = true;
		params.hitParams.rayTypeCount = 1u;
		params.hitParams.rayType = 0u;

		for (int causeIdx = static_cast<int>(MissCause::NONE) + 1; causeIdx < static_cast<int>(MissCause::CAUSE_COUNT); ++causeIdx)
		{
			params.missParams.missCause = static_cast<MissCause>(causeIdx);
			params.missParams.missIndex = static_cast<deUint32>(causeIdx-1);
			nonUniformGroup->addChild(new NonUniformArgsCase(testCtx, "miss_cause_" + de::toString(causeIdx), "", params));
		}
	}

	return nonUniformGroup.release();
}

}	// RayTracing
}	// vkt
