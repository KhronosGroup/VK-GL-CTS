/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \brief Test procedural geometry with complex bouding box sets
 *//*--------------------------------------------------------------------*/

#include "vktRayQueryProceduralGeometryTests.hpp"
#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkRayTracingUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTexture.hpp"
#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuFloat.hpp"

namespace vkt
{
namespace RayQuery
{
namespace
{
using namespace vk;
using namespace vkt;

enum class TestType
{
	OBJECT_BEHIND_BOUNDING_BOX = 0,
	TRIANGLE_IN_BETWEEN
};

class RayQueryProceduralGeometryTestBase : public TestInstance
{
public:

	RayQueryProceduralGeometryTestBase	(Context& context);
	~RayQueryProceduralGeometryTestBase	(void) = default;

	tcu::TestStatus iterate				(void) override;

protected:

	virtual void setupAccelerationStructures() = 0;

private:

	VkWriteDescriptorSetAccelerationStructureKHR	makeASWriteDescriptorSet	(const VkAccelerationStructureKHR* pAccelerationStructure);
	void											clearBuffer					(de::SharedPtr<BufferWithMemory> buffer, VkDeviceSize bufferSize);

protected:

	Move<VkCommandPool>		m_cmdPool;
	Move<VkCommandBuffer>	m_cmdBuffer;

	std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >	m_blasVect;
	de::SharedPtr<TopLevelAccelerationStructure>					m_referenceTLAS;
	de::SharedPtr<TopLevelAccelerationStructure>					m_resultTLAS;
};

RayQueryProceduralGeometryTestBase::RayQueryProceduralGeometryTestBase(Context& context)
	: vkt::TestInstance	(context)
	, m_referenceTLAS	(makeTopLevelAccelerationStructure().release())
	, m_resultTLAS		(makeTopLevelAccelerationStructure().release())
{
}

tcu::TestStatus RayQueryProceduralGeometryTestBase::iterate(void)
{
	const DeviceInterface&	vkd					= m_context.getDeviceInterface();
	const VkDevice			device				= m_context.getDevice();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkQueue			queue				= m_context.getUniversalQueue();
	Allocator&				allocator			= m_context.getDefaultAllocator();
	const deUint32			imageSize			= 64u;

	const Move<VkDescriptorPool> descriptorPool = DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 2u)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u)
		.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 2u);

	Move<VkDescriptorSetLayout> descriptorSetLayout = DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_COMPUTE_BIT)	// as with single/four aabb's
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)				// ssbo with result/reference values
		.build(vkd, device);

	const Move<VkDescriptorSet>			referenceDescriptorSet	= makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout);
	const Move<VkDescriptorSet>			resultDescriptorSet		= makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout);

	const VkDeviceSize					resultBufferSize		= imageSize * imageSize * sizeof(int);
	const VkBufferCreateInfo			resultBufferCreateInfo	= makeBufferCreateInfo(resultBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	de::SharedPtr<BufferWithMemory>		referenceBuffer			= de::SharedPtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));
	de::SharedPtr<BufferWithMemory>		resultBuffer			= de::SharedPtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));

	Move<VkShaderModule>				shaderModule		= createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0u);
	const Move<VkPipelineLayout>		pipelineLayout		= makePipelineLayout(vkd, device, descriptorSetLayout.get());
	const VkComputePipelineCreateInfo	pipelineCreateInfo
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,			// VkStructureType						sType
		DE_NULL,												// const void*							pNext
		0u,														// VkPipelineCreateFlags				flags
		{														// VkPipelineShaderStageCreateInfo		stage
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			DE_NULL,
			(VkPipelineShaderStageCreateFlags)0,
			VK_SHADER_STAGE_COMPUTE_BIT,
			*shaderModule,
			"main",
			DE_NULL
		},
		*pipelineLayout,										// VkPipelineLayout						layout
		DE_NULL,												// VkPipeline							basePipelineHandle
		0,														// deInt32								basePipelineIndex
	};
	Move<VkPipeline> pipeline = createComputePipeline(vkd, device, DE_NULL, &pipelineCreateInfo);

	m_cmdPool	= createCommandPool(vkd, device, 0, queueFamilyIndex);
	m_cmdBuffer	= allocateCommandBuffer(vkd, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	// clear result and reference buffers
	clearBuffer(resultBuffer, resultBufferSize);
	clearBuffer(referenceBuffer, resultBufferSize);

	beginCommandBuffer(vkd, *m_cmdBuffer, 0u);
	{
		setupAccelerationStructures();

		// update descriptor sets
		{
			typedef DescriptorSetUpdateBuilder::Location DSL;

			const VkWriteDescriptorSetAccelerationStructureKHR	referenceAS		= makeASWriteDescriptorSet(m_referenceTLAS->getPtr());
			const VkDescriptorBufferInfo						referenceSSBO	= makeDescriptorBufferInfo(**referenceBuffer, 0u, VK_WHOLE_SIZE);
			DescriptorSetUpdateBuilder()
				.writeSingle(*referenceDescriptorSet, DSL::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &referenceAS)
				.writeSingle(*referenceDescriptorSet, DSL::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &referenceSSBO)
				.update(vkd, device);

			const VkWriteDescriptorSetAccelerationStructureKHR	resultAS	= makeASWriteDescriptorSet(m_resultTLAS->getPtr());
			const VkDescriptorBufferInfo						resultSSBO	= makeDescriptorBufferInfo(**resultBuffer, 0u, VK_WHOLE_SIZE);
			DescriptorSetUpdateBuilder()
				.writeSingle(*resultDescriptorSet, DSL::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &resultAS)
				.writeSingle(*resultDescriptorSet, DSL::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultSSBO)
				.update(vkd, device);
		}

		// wait for data transfers
		const VkMemoryBarrier bufferUploadBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
		cmdPipelineMemoryBarrier(vkd, *m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, &bufferUploadBarrier, 1u);

		// wait for as build
		const VkMemoryBarrier asBuildBarrier = makeMemoryBarrier(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_SHADER_READ_BIT);
		cmdPipelineMemoryBarrier(vkd, *m_cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, &asBuildBarrier, 1u);

		vkd.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);

		// generate reference
		vkd.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1, &referenceDescriptorSet.get(), 0, DE_NULL);
		vkd.cmdDispatch(*m_cmdBuffer, imageSize, imageSize, 1);

		// generate result
		vkd.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1, &resultDescriptorSet.get(), 0, DE_NULL);
		vkd.cmdDispatch(*m_cmdBuffer, imageSize, imageSize, 1);

		const VkMemoryBarrier postTraceMemoryBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
		cmdPipelineMemoryBarrier(vkd, *m_cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);
	}
	endCommandBuffer(vkd, *m_cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, m_cmdBuffer.get());

	// verify result buffer
	auto referenceAllocation = referenceBuffer->getAllocation();
	invalidateMappedMemoryRange(vkd, device, referenceAllocation.getMemory(), referenceAllocation.getOffset(), resultBufferSize);

	auto resultAllocation = resultBuffer->getAllocation();
	invalidateMappedMemoryRange(vkd, device, resultAllocation.getMemory(), resultAllocation.getOffset(), resultBufferSize);

	tcu::TextureFormat		imageFormat		(vk::mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM));
	tcu::PixelBufferAccess	referenceAccess	(imageFormat, imageSize, imageSize, 1, referenceAllocation.getHostPtr());
	tcu::PixelBufferAccess	resultAccess	(imageFormat, imageSize, imageSize, 1, resultAllocation.getHostPtr());

	if (tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Result comparison", "", referenceAccess, resultAccess, tcu::UVec4(0), tcu::COMPARE_LOG_EVERYTHING))
		return tcu::TestStatus::pass("Pass");
	return tcu::TestStatus::fail("Fail");
}

VkWriteDescriptorSetAccelerationStructureKHR RayQueryProceduralGeometryTestBase::makeASWriteDescriptorSet(const VkAccelerationStructureKHR* pAccelerationStructure)
{
	return
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	// VkStructureType						sType
		DE_NULL,															// const void*							pNext
		1u,																	// deUint32								accelerationStructureCount
		pAccelerationStructure												// const VkAccelerationStructureKHR*	pAccelerationStructures
	};
}

void RayQueryProceduralGeometryTestBase::clearBuffer(de::SharedPtr<BufferWithMemory> buffer, VkDeviceSize bufferSize)
{
	const DeviceInterface&	vkd				= m_context.getDeviceInterface();
	const VkDevice			device			= m_context.getDevice();
	auto&					bufferAlloc		= buffer->getAllocation();
	void*					bufferPtr		= bufferAlloc.getHostPtr();

	deMemset(bufferPtr, 1, static_cast<size_t>(bufferSize));
	vk::flushAlloc(vkd, device, bufferAlloc);
}

class ObjectBehindBoundingBoxInstance : public RayQueryProceduralGeometryTestBase
{
public:

	ObjectBehindBoundingBoxInstance(Context& context);
	void setupAccelerationStructures() override;
};

ObjectBehindBoundingBoxInstance::ObjectBehindBoundingBoxInstance(Context& context)
	: RayQueryProceduralGeometryTestBase(context)
{
}

void ObjectBehindBoundingBoxInstance::setupAccelerationStructures()
{
	const DeviceInterface&	vkd			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();
	Allocator&				allocator	= m_context.getDefaultAllocator();

	// build reference acceleration structure - single aabb big enough to fit whole procedural geometry
	de::SharedPtr<BottomLevelAccelerationStructure> referenceBLAS(makeBottomLevelAccelerationStructure().release());
	referenceBLAS->setGeometryData(
		{
			{  0.0,  0.0, -64.0 },
			{ 64.0, 64.0, -16.0 },
		},
		false,
		0
		);
	referenceBLAS->createAndBuild(vkd, device, *m_cmdBuffer, allocator);
	m_blasVect.push_back(referenceBLAS);

	m_referenceTLAS->setInstanceCount(1);
	m_referenceTLAS->addInstance(m_blasVect.back());
	m_referenceTLAS->createAndBuild(vkd, device, *m_cmdBuffer, allocator);

	// build result acceleration structure - wall of 4 aabb's and generated object is actualy behind it (as it is just 1.0 unit thick)
	de::SharedPtr<BottomLevelAccelerationStructure> resultBLAS(makeBottomLevelAccelerationStructure().release());
	resultBLAS->setGeometryData(
		{
			{  0.0,  0.0, 0.0 },	// |  |
			{ 32.0, 32.0, 1.0 },	// |* |
			{ 32.0,  0.0, 0.0 },	//    |  |
			{ 64.0, 32.0, 1.0 },	//    | *|
			{  0.0, 32.0, 0.0 },	// |* |
			{ 32.0, 64.0, 1.0 },	// |  |
			{ 32.0, 32.0, 0.0 },	//    | *|
			{ 64.0, 64.0, 1.0 },	//    |  |
		},
		false,
		0
		);
	resultBLAS->createAndBuild(vkd, device, *m_cmdBuffer, allocator);
	m_blasVect.push_back(resultBLAS);

	m_resultTLAS->setInstanceCount(1);
	m_resultTLAS->addInstance(m_blasVect.back());
	m_resultTLAS->createAndBuild(vkd, device, *m_cmdBuffer, allocator);
}

class TriangleInBeteenInstance : public RayQueryProceduralGeometryTestBase
{
public:

	TriangleInBeteenInstance(Context& context);
	void setupAccelerationStructures() override;
};

TriangleInBeteenInstance::TriangleInBeteenInstance(Context& context)
	: RayQueryProceduralGeometryTestBase(context)
{
}

void TriangleInBeteenInstance::setupAccelerationStructures()
{
	const DeviceInterface&	vkd			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();
	Allocator&				allocator	= m_context.getDefaultAllocator();

	de::SharedPtr<BottomLevelAccelerationStructure> triangleBLAS(makeBottomLevelAccelerationStructure().release());
	triangleBLAS->setGeometryData(
		{
			{ 16.0, 16.0, -8.0 },
			{ 56.0, 32.0, -8.0 },
			{ 32.0, 48.0, -8.0 },
		},
		true,
		VK_GEOMETRY_OPAQUE_BIT_KHR
		);
	triangleBLAS->createAndBuild(vkd, device, *m_cmdBuffer, allocator);
	m_blasVect.push_back(triangleBLAS);

	de::SharedPtr<BottomLevelAccelerationStructure> fullElipsoidBLAS(makeBottomLevelAccelerationStructure().release());
	fullElipsoidBLAS->setGeometryData(
		{
			{  0.0,  0.0, -64.0 },
			{ 64.0, 64.0, -16.0 },
		},
		false,
		0
		);
	fullElipsoidBLAS->createAndBuild(vkd, device, *m_cmdBuffer, allocator);
	m_blasVect.push_back(fullElipsoidBLAS);

	// build reference acceleration structure - triangle and a single aabb big enough to fit whole procedural geometry
	m_referenceTLAS->setInstanceCount(2);
	m_referenceTLAS->addInstance(fullElipsoidBLAS);
	m_referenceTLAS->addInstance(triangleBLAS);
	m_referenceTLAS->createAndBuild(vkd, device, *m_cmdBuffer, allocator);

	de::SharedPtr<BottomLevelAccelerationStructure> elipsoidWallBLAS(makeBottomLevelAccelerationStructure().release());
	elipsoidWallBLAS->setGeometryData(
		{
			{  0.0,  0.0, 0.0 },	// |*  |
			{ 20.0, 64.0, 1.0 },
			{ 20.0,  0.0, 0.0 },	// | * |
			{ 44.0, 64.0, 1.0 },
			{ 44.0,  0.0, 0.0 },	// |  *|
			{ 64.0, 64.0, 1.0 },
		},
		false,
		0
		);
	elipsoidWallBLAS->createAndBuild(vkd, device, *m_cmdBuffer, allocator);
	m_blasVect.push_back(elipsoidWallBLAS);

	// build result acceleration structure - triangle and a three aabb's (they are in front of triangle but generate intersections behind it)
	m_resultTLAS->setInstanceCount(2);
	m_resultTLAS->addInstance(elipsoidWallBLAS);
	m_resultTLAS->addInstance(triangleBLAS);
	m_resultTLAS->createAndBuild(vkd, device, *m_cmdBuffer, allocator);
}

class RayQueryProceduralGeometryTestCase : public TestCase
{
public:
	RayQueryProceduralGeometryTestCase		(tcu::TestContext& context, const char* name, TestType testType);
	~RayQueryProceduralGeometryTestCase		(void) = default;

	void				checkSupport			(Context& context) const override;
	void				initPrograms			(SourceCollections& programCollection) const override;
	TestInstance*		createInstance			(Context& context) const override;

protected:
	TestType m_testType;
};

RayQueryProceduralGeometryTestCase::RayQueryProceduralGeometryTestCase(tcu::TestContext& context, const char* name, TestType testType)
	: TestCase		(context, name, "")
	, m_testType	(testType)
{
}

void RayQueryProceduralGeometryTestCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_ray_query");

	if (!context.getRayQueryFeatures().rayQuery)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayQueryFeaturesKHR.rayQuery");

	if (!context.getAccelerationStructureFeatures().accelerationStructure)
		TCU_THROW(TestError, "Requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");
}

void RayQueryProceduralGeometryTestCase::initPrograms(SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions glslBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	std::string compSource =
		"#version 460 core\n"
		"#extension GL_EXT_ray_query : require\n"

		"layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;\n"
		"layout(set = 0, binding = 1, std430) writeonly buffer Result {\n"
		"    int value[];\n"
		"} result;\n"

		"void main()\n"
		"{\n"
		"  float tmin          = 0.0;\n"
		"  float tmax          = 50.0;\n"
		"  vec3  rayOrigin     = vec3(float(gl_GlobalInvocationID.x) + 0.5f, float(gl_GlobalInvocationID.y) + 0.5f, 2.0);\n"
		"  vec3  rayDir        = vec3(0.0,0.0,-1.0);\n"
		"  uint  resultIndex   = gl_GlobalInvocationID.x + gl_GlobalInvocationID.y * gl_NumWorkGroups.x;\n"
		"  int   payload       = 30;\n"

		// elipsoid center and radii
		"  vec3 elipsoidOrigin = vec3(32.0, 32.0, -30.0);\n"
		"  vec3 elipsoidRadii  = vec3(30.0, 15.0, 5.0);\n"

		"  rayQueryEXT rq;\n"
		"  rayQueryInitializeEXT(rq, tlas, gl_RayFlagsCullBackFacingTrianglesEXT, 0xFF, rayOrigin, tmin, rayDir, tmax);\n"

		"  while (rayQueryProceedEXT(rq))\n"
		"  {\n"
		"    uint intersectionType = rayQueryGetIntersectionTypeEXT(rq, false);\n"
		"    if (intersectionType == gl_RayQueryCandidateIntersectionAABBEXT)\n"
		"    {\n"
		// simplify to ray sphere intersection
		"      vec3  eliDir = rayOrigin - elipsoidOrigin;\n"
		"      vec3  eliS   = eliDir / elipsoidRadii;\n"
		"      vec3  rayS   = rayDir / elipsoidRadii;\n"

		"      float a = dot(rayS, rayS);\n"
		"      float b = dot(eliS, rayS);\n"
		"      float c = dot(eliS, eliS);\n"
		"      float h = b * b - a * (c - 1.0);\n"
		"      if (h >= 0.0)\n"
		"        rayQueryGenerateIntersectionEXT(rq, (-b - sqrt(h)) / a);\n"
		"    }\n"
		"    else if (intersectionType == gl_RayQueryCandidateIntersectionTriangleEXT)\n"
		"    {\n"
		"      payload = 250;\n"
		"      rayQueryConfirmIntersectionEXT(rq);\n"
		"    }\n"
		"  }\n"
		"  if (rayQueryGetIntersectionTypeEXT(rq, true) != gl_RayQueryCommittedIntersectionNoneEXT)\n"
		"  {\n"
		"    int instanceId = rayQueryGetIntersectionInstanceIdEXT(rq, true);\n"
		"    if (instanceId > -1)\n"
		"    {\n"
		"      float hitT      = rayQueryGetIntersectionTEXT(rq, true);\n"
		"      vec3  lightDir  = normalize(vec3(0.0, 0.0, 1.0));\n"
		"      vec3  hitPos    = rayOrigin + hitT * rayDir;\n"
		"      vec3  hitNormal = normalize((hitPos - elipsoidOrigin) / elipsoidRadii);\n"
		"      payload = 50 + int(200.0 * clamp(dot(hitNormal, lightDir), 0.0, 1.0));\n"
		"    }\n"
		"  }\n"

		// to be able to display result in cherry this is interpreated as r8g8b8a8 during verification
		// we are using only red but we need to add alpha (note: r and a may be swapped depending on endianness)
		"  result.value[resultIndex] = payload + 0xFF000000;\n"
		"};\n";
	programCollection.glslSources.add("comp") << glu::ComputeSource(compSource) << glslBuildOptions;
}

TestInstance* RayQueryProceduralGeometryTestCase::createInstance(Context& context) const
{
	if (m_testType == TestType::TRIANGLE_IN_BETWEEN)
		return new TriangleInBeteenInstance(context);

	// TestType::OBJECT_BEHIND_BOUNDING_BOX
	return new ObjectBehindBoundingBoxInstance(context);
}

}	// anonymous

tcu::TestCaseGroup*	createProceduralGeometryTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "procedural_geometry", "Test procedural geometry with complex bouding box sets"));

	group->addChild(new RayQueryProceduralGeometryTestCase(testCtx, "object_behind_bounding_boxes",	TestType::OBJECT_BEHIND_BOUNDING_BOX));
	group->addChild(new RayQueryProceduralGeometryTestCase(testCtx, "triangle_in_between",			TestType::TRIANGLE_IN_BETWEEN));

	return group.release();
}

}	// RayQuery

}	// vkt
