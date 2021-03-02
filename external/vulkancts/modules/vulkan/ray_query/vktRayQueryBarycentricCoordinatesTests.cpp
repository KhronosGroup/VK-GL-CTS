/*-------------------------------------------------------------------------
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
 * \brief Ray Query Barycentric Coordinates Tests
 *//*--------------------------------------------------------------------*/

#include "vktRayQueryBarycentricCoordinatesTests.hpp"
#include "vktTestCase.hpp"

#include "vkRayTracingUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"

#include <sstream>
#include <vector>

namespace vkt
{
namespace RayQuery
{

namespace
{

using namespace vk;

struct TestParams
{
	deUint32				seed;
};

constexpr float		kZCoord		= 5.0f;
constexpr float		kXYCoordAbs	= 1.0f;

constexpr float		kThreshold	= 0.001f;				// For the resulting coordinates.
constexpr float		kTMin		= 1.0f - kThreshold;	// Require the same precision in T.
constexpr float		kTMax		= 1.0f + kThreshold;	// Ditto.
constexpr deUint32	kNumRays	= 20u;

class BarycentricCoordinatesCase : public TestCase
{
public:
							BarycentricCoordinatesCase	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params);
	virtual					~BarycentricCoordinatesCase	(void) {}

	virtual void			checkSupport				(Context& context) const;
	virtual void			initPrograms				(vk::SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance				(Context& context) const;

protected:
	TestParams				m_params;
};

class BarycentricCoordinatesInstance : public TestInstance
{
public:
								BarycentricCoordinatesInstance	(Context& context, const TestParams& params);
	virtual						~BarycentricCoordinatesInstance	(void) {}

	virtual tcu::TestStatus		iterate							(void);

protected:
	TestParams					m_params;
};

BarycentricCoordinatesCase::BarycentricCoordinatesCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
	: TestCase	(testCtx, name, description)
	, m_params	(params)
{}

void BarycentricCoordinatesCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_ray_query");
}

void BarycentricCoordinatesCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions buildOptions (programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	std::ostringstream comp;
	comp
		<< "#version 460 core\n"
		<< "#extension GL_EXT_ray_query : require\n"
		<< "\n"
		<< "layout(local_size_x=" << kNumRays << ", local_size_y=1, local_size_z=1) in;\n"
		<< "\n"
		<< "layout(set=0, binding=0) uniform accelerationStructureEXT topLevelAS;\n"
		<< "layout(set=0, binding=1) uniform RayDirections {\n"
		<< "  vec4 values[" << kNumRays << "];\n"
		<< "} directions;\n"
		<< "layout(set=0, binding=2, std430) buffer OutputBarycentrics {\n"
		<< "  vec4 values[" << kNumRays << "];\n"
		<< "} coordinates;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "  const uint  cullMask  = 0xFF;\n"
		<< "  const vec3  origin    = vec3(0.0, 0.0, 0.0);\n"
		<< "  const vec3  direction = directions.values[gl_LocalInvocationID.x].xyz;\n"
		<< "  const float tMin      = " << kTMin << ";\n"
		<< "  const float tMax      = " << kTMax << ";\n"
		<< "  vec4        outputVal = vec4(-1.0, -1.0, -1.0, -1.0);\n"
		<< "  rayQueryEXT rq;\n"
		<< "  rayQueryInitializeEXT(rq, topLevelAS, gl_RayFlagsNoneEXT, cullMask, origin, tMin, direction, tMax);\n"
		<< "  while (rayQueryProceedEXT(rq)) {\n"
		<< "    if (rayQueryGetIntersectionTypeEXT(rq, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {\n"
		<< "      outputVal = vec4(rayQueryGetIntersectionBarycentricsEXT(rq, false), 0.0, 0.0);\n"
		<< "    }\n"
		<< "  }\n"
		<< "  coordinates.values[gl_LocalInvocationID.x] = vec4(outputVal);\n"
		<< "}\n"
		;

	programCollection.glslSources.add("comp") << glu::ComputeSource(updateRayTracingGLSL(comp.str())) << buildOptions;
}

TestInstance* BarycentricCoordinatesCase::createInstance (Context& context) const
{
	return new BarycentricCoordinatesInstance(context, m_params);
}

BarycentricCoordinatesInstance::BarycentricCoordinatesInstance (Context& context, const TestParams& params)
	: TestInstance	(context)
	, m_params		(params)
{}

// Calculates coordinates in a triangle given barycentric coordinates b and c.
tcu::Vec3 calcCoordinates (const std::vector<tcu::Vec3>& triangle, float b, float c)
{
	DE_ASSERT(triangle.size() == 3u);
	DE_ASSERT(b > 0.0f);
	DE_ASSERT(c > 0.0f);
	DE_ASSERT(b + c < 1.0f);

	const float a = 1.0f - b - c;
	DE_ASSERT(a > 0.0f);

	return triangle[0] * a + triangle[1] * b + triangle[2] * c;
}

// Return a, b, c with a close to 1.0f and (b, c) close to 0.0f.
tcu::Vec3 getBarycentricVertex (void)
{
	const float a = 0.999f;
	const float aux = 1.0f - a;
	const float b = aux / 2.0f;
	const float c = b;

	return tcu::Vec3(a, b, c);
}

tcu::Vec4 extendToV4 (const tcu::Vec3& vec3)
{
	return tcu::Vec4(vec3.x(), vec3.y(), vec3.z(), 0.0f);
}

tcu::TestStatus BarycentricCoordinatesInstance::iterate (void)
{
	const auto&	vkd		= m_context.getDeviceInterface();
	const auto	device	= m_context.getDevice();
	auto&		alloc	= m_context.getDefaultAllocator();
	const auto	qIndex	= m_context.getUniversalQueueFamilyIndex();
	const auto	queue	= m_context.getUniversalQueue();
	const auto	stages	= VK_SHADER_STAGE_COMPUTE_BIT;

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, qIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);

	// Build acceleration structures.
	auto topLevelAS		= makeTopLevelAccelerationStructure();
	auto bottomLevelAS	= makeBottomLevelAccelerationStructure();

	const std::vector<tcu::Vec3> triangle =
	{
		tcu::Vec3(        0.0f, -kXYCoordAbs, kZCoord),
		tcu::Vec3(-kXYCoordAbs,  kXYCoordAbs, kZCoord),
		tcu::Vec3( kXYCoordAbs,  kXYCoordAbs, kZCoord),
	};

	bottomLevelAS->addGeometry(triangle, true/*is triangles*/, VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR);
	bottomLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc);
	de::SharedPtr<BottomLevelAccelerationStructure> blasSharedPtr (bottomLevelAS.release());

	topLevelAS->setInstanceCount(1);
	topLevelAS->addInstance(blasSharedPtr, identityMatrix3x4, 0, 0xFFu, 0u, VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR);
	topLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc);

	// Uniform buffer for directions.
	const auto directionsBufferSize		= static_cast<VkDeviceSize>(sizeof(tcu::Vec4) * kNumRays);
	const auto directionsBufferInfo		= makeBufferCreateInfo(directionsBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	BufferWithMemory directionsBuffer	(vkd, device, alloc, directionsBufferInfo, MemoryRequirement::HostVisible);
	auto& directionsBufferAlloc			= directionsBuffer.getAllocation();
	void* directionsBufferData			= directionsBufferAlloc.getHostPtr();

	// Generate rays towards the 3 triangle coordinates (avoiding exact vertices) and additional coordinates.
	std::vector<tcu::Vec4> directions;
	std::vector<tcu::Vec4> expectedOutputCoordinates;
	directions.reserve(kNumRays);
	expectedOutputCoordinates.reserve(kNumRays);

	const auto barycentricABC = getBarycentricVertex();

	directions.push_back(extendToV4(calcCoordinates(triangle, barycentricABC.x(), barycentricABC.y())));
	directions.push_back(extendToV4(calcCoordinates(triangle, barycentricABC.y(), barycentricABC.x())));
	directions.push_back(extendToV4(calcCoordinates(triangle, barycentricABC.y(), barycentricABC.z())));

	expectedOutputCoordinates.push_back(tcu::Vec4(barycentricABC.x(), barycentricABC.y(), 0.0f, 0.0f));
	expectedOutputCoordinates.push_back(tcu::Vec4(barycentricABC.y(), barycentricABC.x(), 0.0f, 0.0f));
	expectedOutputCoordinates.push_back(tcu::Vec4(barycentricABC.y(), barycentricABC.z(), 0.0f, 0.0f));

	de::Random rnd(m_params.seed);
	while (directions.size() < kNumRays)
	{
		// Avoid 0.0 when choosing b and c.
		float b;
		while ((b = rnd.getFloat()) == 0.0f)
			;
		float c;
		while ((c = rnd.getFloat(0.0f, 1.0f - b)) == 0.0f)
			;

		expectedOutputCoordinates.push_back(tcu::Vec4(b, c, 0.0f, 0.0f));
		directions.push_back(extendToV4(calcCoordinates(triangle, b, c)));
	}

	deMemcpy(directionsBufferData, directions.data(), directionsBufferSize);
	flushAlloc(vkd, device, directionsBufferAlloc);

	// Storage buffer for output barycentric coordinates.
	const auto barycoordsBufferSize		= static_cast<VkDeviceSize>(sizeof(tcu::Vec4) * kNumRays);
	const auto barycoordsBufferInfo		= makeBufferCreateInfo(barycoordsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory barycoordsBuffer	(vkd, device, alloc, barycoordsBufferInfo, MemoryRequirement::HostVisible);
	auto& barycoordsBufferAlloc			= barycoordsBuffer.getAllocation();
	void* barycoordsBufferData			= barycoordsBufferAlloc.getHostPtr();
	deMemset(barycoordsBufferData, 0, static_cast<size_t>(barycoordsBufferSize));
	flushAlloc(vkd, device, barycoordsBufferAlloc);

	// Descriptor set layout.
	DescriptorSetLayoutBuilder dsLayoutBuilder;
	dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, stages);
	dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stages);
	dsLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stages);
	const auto setLayout = dsLayoutBuilder.build(vkd, device);

	// Pipeline layout.
	const auto pipelineLayout = makePipelineLayout(vkd, device, setLayout.get());

	// Descriptor pool and set.
	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
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
		const auto uniformBufferInfo = makeDescriptorBufferInfo(directionsBuffer.get(), 0ull, VK_WHOLE_SIZE);
		const auto storageBufferInfo = makeDescriptorBufferInfo(barycoordsBuffer.get(), 0ull, VK_WHOLE_SIZE);

		DescriptorSetUpdateBuilder updateBuilder;
		updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelDescInfo);
		updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBufferInfo);
		updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &storageBufferInfo);
		updateBuilder.update(vkd, device);
	}

	// Shader module.
	const auto compModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0);

	// Pipeline.
	const VkPipelineShaderStageCreateInfo shaderInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,												//	const void*							pNext;
		0u,														//	VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_COMPUTE_BIT,							//	VkShaderStageFlagBits				stage;
		compModule.get(),										//	VkShaderModule						module;
		"main",													//	const char*							pName;
		nullptr,												//	const VkSpecializationInfo*			pSpecializationInfo;
	};
	const VkComputePipelineCreateInfo pipelineInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	//	VkStructureType					sType;
		nullptr,										//	const void*						pNext;
		0u,												//	VkPipelineCreateFlags			flags;
		shaderInfo,										//	VkPipelineShaderStageCreateInfo	stage;
		pipelineLayout.get(),							//	VkPipelineLayout				layout;
		DE_NULL,										//	VkPipeline						basePipelineHandle;
		0,												//	deInt32							basePipelineIndex;
	};
	const auto pipeline = createComputePipeline(vkd, device, DE_NULL, &pipelineInfo);

	// Dispatch work with ray queries.
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);

	// Barrier for the output buffer.
	const auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &bufferBarrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Verify results.
	std::vector<tcu::Vec4>	outputData				(expectedOutputCoordinates.size());
	const auto				barycoordsBufferSizeSz	= static_cast<size_t>(barycoordsBufferSize);

	invalidateAlloc(vkd, device, barycoordsBufferAlloc);
	DE_ASSERT(de::dataSize(outputData) == barycoordsBufferSizeSz);
	deMemcpy(outputData.data(), barycoordsBufferData, barycoordsBufferSizeSz);

	for (size_t i = 0; i < outputData.size(); ++i)
	{
		const auto& outVal		= outputData[i];
		const auto& expectedVal	= expectedOutputCoordinates[i];

		if (outVal.z() != 0.0f || outVal.w() != 0.0f || de::abs(outVal.x() - expectedVal.x()) > kThreshold || de::abs(outVal.y() - expectedVal.y()) > kThreshold)
		{
			std::ostringstream msg;
			msg << "Unexpected value found for ray " << i << ": expected " << expectedVal << " and found " << outVal << ";";
			TCU_FAIL(msg.str());
		}
	}

	return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup*	createBarycentricCoordinatesTests (tcu::TestContext& testCtx)
{
	using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

	GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "barycentric_coordinates", "Test barycentric coordinates reported by the ray query"));

	deUint32 seed = 1614674687u;
	mainGroup->addChild(new BarycentricCoordinatesCase(testCtx, "compute", "", TestParams{seed++}));

	return mainGroup.release();
}

} // RayQuery
} // vkt

