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
 * \brief Ray Tracing Direction Tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingDirectionTests.hpp"
#include "vktTestCase.hpp"

#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRayTracingUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuVector.hpp"
#include "tcuMatrix.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deDefs.hpp"

#include <vector>
#include <cmath>
#include <sstream>
#include <utility>
#include <algorithm>
#include <limits>

namespace vkt
{
namespace RayTracing
{

namespace
{

using namespace vk;

using GeometryData = std::vector<tcu::Vec3>;

// Should rays be shot from inside the geometry or not?
enum class RayOriginType
{
	OUTSIDE = 0,	// Works with AABBs and triangles.
	INSIDE,			// Works with AABBs only.
};

// When rays are shot from the outside, they are expected to cross the geometry.
// When shot from the inside, they can end inside, at the edge or outside the geometry.
enum class RayEndType
{
	CROSS = 0,		// For RayOriginType::OUTSIDE.
	ZERO,			// For RayOriginType::INSIDE.
	INSIDE,			// For RayOriginType::INSIDE.
	EDGE,			// For RayOriginType::INSIDE.
	OUTSIDE,		// For RayOriginType::INSIDE.
};

struct SpaceObjects
{
	tcu::Vec3		origin;
	tcu::Vec3		direction;
	GeometryData	geometry;

	SpaceObjects (RayOriginType rayOriginType, VkGeometryTypeKHR geometryType)
		: origin	(0.0f, 0.0f, 1.0f)	// Origin of the ray at (0, 0, 1).
		, direction	(0.0f, 0.0f, 1.0f)	// Shooting towards (0, 0, 1).
		, geometry	()
	{
		DE_ASSERT(geometryType == VK_GEOMETRY_TYPE_TRIANGLES_KHR || geometryType == VK_GEOMETRY_TYPE_AABBS_KHR);
		DE_ASSERT(rayOriginType == RayOriginType::OUTSIDE || geometryType == VK_GEOMETRY_TYPE_AABBS_KHR);

		if (geometryType == VK_GEOMETRY_TYPE_TRIANGLES_KHR)
		{
			// Triangle around (0, 0, 5).
			geometry.reserve(3u);
			geometry.push_back(tcu::Vec3( 0.0f,  0.5f, 5.0f));
			geometry.push_back(tcu::Vec3(-0.5f, -0.5f, 5.0f));
			geometry.push_back(tcu::Vec3( 0.5f, -0.5f, 5.0f));
		}
		else
		{
			// AABB around (0, 0, 5) or with its back side at that distance when shot from the inside.
			geometry.reserve(2u);
			geometry.push_back(tcu::Vec3(-0.5f, -0.5f, ((rayOriginType == RayOriginType::INSIDE) ? 0.0f : 5.0f)));
			geometry.push_back(tcu::Vec3( 0.5f,  0.5f, 5.0f));
		}
	}

	static float getDefaultDistance (void)
	{
		// Consistent with the Z coordinates of the origin, direction and points in constructors.
		return 4.0f;
	}

	// Calculates distance to geometry edge given the direction scaling factor.
	static float getDistanceToEdge (float directionScale)
	{
		return getDefaultDistance() / directionScale;
	}
};

// Default test tolerance for distance values.
constexpr float kDefaultTolerance = 0.001f;

// Calculates appropriate values for Tmin/Tmax given the distance to the geometry edge.
std::pair<float, float> calcTminTmax (RayOriginType rayOriginType, RayEndType rayEndType, float distanceToEdge)
{
	std::pair<float, float> result;

	if (rayOriginType == RayOriginType::OUTSIDE)
	{
		DE_ASSERT(rayEndType == RayEndType::CROSS);
		const auto margin = kDefaultTolerance / 2.0f;
		result = std::make_pair(de::max(distanceToEdge - margin, 0.0f), distanceToEdge + margin);
	}
	else
	{
		result.first = 0.0f;
		switch (rayEndType)
		{
		case RayEndType::ZERO:		result.second = 0.0f;					break;
		case RayEndType::INSIDE:	result.second = distanceToEdge / 2.0f;	break;
		case RayEndType::EDGE:		result.second = distanceToEdge;			break;
		case RayEndType::OUTSIDE:	result.second = distanceToEdge + 1.0f;	break;
		default: DE_ASSERT(false); break;
		}
	}

	return result;
}

// Get matrix to scale a point with the given scale factor.
tcu::Mat3 getScaleMatrix (float scaleFactor)
{
	const float scaleDirectionMatrixData[] =
	{
		scaleFactor,		0.f,			0.f,
		0.f,				scaleFactor,	0.f,
		0.f,				0.f,			scaleFactor,
	};
	return tcu::Mat3(scaleDirectionMatrixData);
}

// Get a matrix to rotate a point around the X and Y axis by the given angles in radians.
tcu::Mat3 getRotationMatrix (float rotationX, float rotationY)
{
	const float cosA = std::cos(rotationX);
	const float sinA = std::sin(rotationX);

	const float cosB = std::cos(rotationY);
	const float sinB = std::sin(rotationY);

	const float rotationMatrixDataX[] =
	{
		1.0f, 0.0f, 0.0f,
		0.0f, cosA,-sinA,
		0.0f, sinA, cosA,
	};
	const tcu::Mat3 rotationMatrixX (rotationMatrixDataX);

	const float rotationMatrixDataY[] =
	{
		cosB, 0.0f,-sinB,
		0.0f, 1.0f, 0.0f,
		sinB, 0.0f, cosB,
	};
	const tcu::Mat3 rotationMatrixY (rotationMatrixDataY);

	return rotationMatrixX * rotationMatrixY;
}

// Converts transformation matrix to the expected KHR format.
VkTransformMatrixKHR toTransformMatrixKHR (const tcu::Mat3& mat3)
{
	VkTransformMatrixKHR result;

	deMemset(result.matrix, 0, sizeof(result.matrix));
	for (int y = 0; y < 3; ++y)
	for (int x = 0; x < 3; ++x)
		result.matrix[x][y] = mat3[x][y];

	return result;
}

struct TestParams
{
	SpaceObjects			spaceObjects;
	float					directionScale;
	float					rotationX;
	float					rotationY;
	VkShaderStageFlagBits	testStage;
	VkGeometryTypeKHR		geometryType;
	bool					useArraysOfPointers;
	bool					updateMatrixAfterBuild;
	RayOriginType			rayOriginType;
	RayEndType				rayEndtype;

	VkShaderStageFlags usedStages (void) const
	{
		VkShaderStageFlags flags = (VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | testStage);

		if (geometryType == VK_GEOMETRY_TYPE_AABBS_KHR)
			flags |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

		return flags;
	}

	// True if we are testing the intersection shader.
	bool isecMain (void) const
	{
		return (testStage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR);
	}

	// True if the intersection shader is needed as an auxiliar shader.
	bool isecAux (void) const
	{
		return (!isecMain() && geometryType == VK_GEOMETRY_TYPE_AABBS_KHR);
	}

	// True if the intersection shader is used in some capacity.
	bool isecUsed (void) const
	{
		return (isecMain() || isecAux());
	}
};

class DirectionTestCase : public vkt::TestCase
{
public:
							DirectionTestCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params);
	virtual					~DirectionTestCase		(void) {}

	virtual void			checkSupport			(Context& context) const;
	virtual void			initPrograms			(vk::SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance			(Context& context) const;

protected:
	TestParams				m_params;
};

class DirectionTestInstance : public vkt::TestInstance
{
public:
								DirectionTestInstance	(Context& context, const TestParams& params);
	virtual						~DirectionTestInstance	(void) {}

	virtual tcu::TestStatus		iterate					(void);

protected:
	TestParams					m_params;
};


DirectionTestCase::DirectionTestCase(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
	: vkt::TestCase	(testCtx, name, description)
	, m_params		(params)
{}

void DirectionTestCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");
}

// Push constants. They need to match the shaders.
// Note: origin and direction will be used as a Vec3. Declaring them as Vec4 eases matching alignments.
struct PushConstants
{
	tcu::Vec4	origin;
	tcu::Vec4	direction;
	float		tmix;
	float		tmax;
};

tcu::Vec4 toVec4 (const tcu::Vec3& vec3)
{
	return tcu::Vec4(vec3.x(), vec3.y(), vec3.z(), 0.0f);
}

void DirectionTestCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions buildOptions (programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	std::ostringstream rgen;
	rgen
		<< "#version 460 core\n"
		<< "#extension GL_EXT_ray_tracing : require\n"
		<< "layout(location=0) rayPayloadEXT vec3 hitValue;\n"
		<< "layout(set=0, binding=0) uniform accelerationStructureEXT topLevelAS;\n"
		// Needs to match the PushConstants struct above.
		<< "layout(push_constant, std430) uniform PushConstants {\n"
		<< "  vec4 origin;\n"
		<< "  vec4 direction;\n"
		<< "  float tmin;\n"
		<< "  float tmax;\n"
		<< "} pc;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "  const uint cullMask = 0xFF;\n"
		<< "  traceRayEXT(topLevelAS, gl_RayFlagsNoneEXT, cullMask, 0, 0, 0, pc.origin.xyz, pc.tmin, pc.direction.xyz, pc.tmax, 0);\n"
		<< "}\n"
		;

	programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(rgen.str())) << buildOptions;

	const bool			isecTest	= m_params.isecMain();
	const std::string	bufferDecl	= "layout(set=0, binding=1, std430) buffer OutBuffer { float val; } outBuffer;\n";

	std::ostringstream isec;
	isec
		<< "#version 460 core\n"
		<< "#extension GL_EXT_ray_tracing : require\n"
		<< "hitAttributeEXT vec3 hitAttribute;\n"
		<< (isecTest ? bufferDecl : "")
		<< "void main()\n"
		<< "{\n"
		<< "  hitAttribute = vec3(0.0f, 0.0f, 0.0f);\n"
		<< (isecTest ? "  outBuffer.val = gl_RayTminEXT;\n" : "")
		<< "  reportIntersectionEXT(gl_RayTminEXT, 0);\n"
		<< "}\n"
		;

	std::ostringstream hits;
	hits
		<< "#version 460 core\n"
		<< "#extension GL_EXT_ray_tracing : require\n"
		<< "layout(location=0) rayPayloadInEXT vec3 hitValue;\n"
		<< "hitAttributeEXT vec3 attribs;\n"
		<< bufferDecl
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "  outBuffer.val = gl_HitTEXT;\n"
		<< "}\n"
		;

	switch (m_params.testStage)
	{
	case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
		programCollection.glslSources.add("hits") << glu::ClosestHitSource(updateRayTracingGLSL(hits.str())) << buildOptions;
		break;
	case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
		programCollection.glslSources.add("hits") << glu::AnyHitSource(updateRayTracingGLSL(hits.str())) << buildOptions;
		break;
	case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
		programCollection.glslSources.add("isec") << glu::IntersectionSource(updateRayTracingGLSL(isec.str())) << buildOptions;
		break;
	default:
		DE_ASSERT(false);
		break;
	}

	// Also add the intersection shader if needed for AABBs.
	if (m_params.isecAux())
		programCollection.glslSources.add("isec") << glu::IntersectionSource(updateRayTracingGLSL(isec.str())) << buildOptions;

	std::ostringstream miss;
	miss
		<< "#version 460 core\n"
		<< "#extension GL_EXT_ray_tracing : require\n"
		<< "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
		<< bufferDecl
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "  outBuffer.val = -10000.0f;\n"
		<< "}\n";

	programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(miss.str())) << buildOptions;
}

TestInstance* DirectionTestCase::createInstance (Context& context) const
{
	return new DirectionTestInstance(context, m_params);
}

DirectionTestInstance::DirectionTestInstance (Context& context, const TestParams& params)
	: vkt::TestInstance	(context)
	, m_params			(params)
{}

tcu::TestStatus DirectionTestInstance::iterate (void)
{
	const auto&	vki		= m_context.getInstanceInterface();
	const auto	physDev	= m_context.getPhysicalDevice();
	const auto&	vkd		= m_context.getDeviceInterface();
	const auto	device	= m_context.getDevice();
	auto&		alloc	= m_context.getDefaultAllocator();
	const auto	qIndex	= m_context.getUniversalQueueFamilyIndex();
	const auto	queue	= m_context.getUniversalQueue();
	const auto	stages	= m_params.usedStages();
	const auto	pcSize	= static_cast<deUint32>(sizeof(PushConstants));

	const auto	scaleMatrix		= getScaleMatrix(m_params.directionScale);
	const auto	rotationMatrix	= getRotationMatrix(m_params.rotationX, m_params.rotationY);
	const auto	transformMatrix	= toTransformMatrixKHR(rotationMatrix);

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, qIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);

	// Build acceleration structures.
	auto topLevelAS		= makeTopLevelAccelerationStructure();
	auto bottomLevelAS	= makeBottomLevelAccelerationStructure();

	const bool							isTriangles		= (m_params.geometryType == VK_GEOMETRY_TYPE_TRIANGLES_KHR);
	const VkGeometryInstanceFlagsKHR	instanceFlags	= (isTriangles ? VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR : 0);

	bottomLevelAS->addGeometry(m_params.spaceObjects.geometry, isTriangles, VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR);
	bottomLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc);

	de::SharedPtr<BottomLevelAccelerationStructure> blasSharedPtr (bottomLevelAS.release());
	topLevelAS->setUseArrayOfPointers(m_params.useArraysOfPointers);
	topLevelAS->setUsePPGeometries(m_params.useArraysOfPointers);
	topLevelAS->setInstanceCount(1);
	{
		const auto& initialMatrix = (m_params.updateMatrixAfterBuild ? identityMatrix3x4 : transformMatrix);
		topLevelAS->addInstance(blasSharedPtr, initialMatrix, 0, 0xFFu, 0u, instanceFlags);
	}
	topLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc);
	if (m_params.updateMatrixAfterBuild)
		topLevelAS->updateInstanceMatrix(vkd, device, 0u, transformMatrix);

	// Create output buffer.
	const auto			bufferSize			= static_cast<VkDeviceSize>(sizeof(float));
	const auto			bufferCreateInfo	= makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory	buffer				(vkd, device, alloc, bufferCreateInfo, MemoryRequirement::HostVisible);
	auto&				bufferAlloc			= buffer.getAllocation();

	// Fill output buffer with an initial value.
	deMemset(bufferAlloc.getHostPtr(), 0, sizeof(float));
	flushAlloc(vkd, device, bufferAlloc);

	// Descriptor set layout and pipeline layout.
	DescriptorSetLayoutBuilder setLayoutBuilder;
	setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, stages);
	setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stages);
	const auto setLayout = setLayoutBuilder.build(vkd, device);

	const VkPushConstantRange pcRange =
	{
		stages,	//	VkShaderStageFlags	stageFlags;
		0u,		//	deUint32			offset;
		pcSize,	//	deUint32			size;
	};

	const VkPipelineLayoutCreateInfo pipelineLayoutInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	//	VkStructureType					sType;
		nullptr,										//	const void*						pNext;
		0u,												//	VkPipelineLayoutCreateFlags		flags;
		1u,												//	deUint32						setLayoutCount;
		&setLayout.get(),								//	const VkDescriptorSetLayout*	pSetLayouts;
		1u,												//	deUint32						pushConstantRangeCount;
		&pcRange,										//	const VkPushConstantRange*		pPushConstantRanges;
	};
	const auto pipelineLayout = createPipelineLayout(vkd, device, &pipelineLayoutInfo);

	// Descriptor pool and set.
	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u);
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

		const auto bufferDescInfo = makeDescriptorBufferInfo(buffer.get(), 0ull, VK_WHOLE_SIZE);

		DescriptorSetUpdateBuilder updateBuilder;
		updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelDescInfo);
		updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescInfo);
		updateBuilder.update(vkd, device);
	}

	// Shader modules.
	Move<VkShaderModule> rgenModule;
	Move<VkShaderModule> missModule;
	Move<VkShaderModule> hitsModule;
	Move<VkShaderModule> isecModule;

	rgenModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("rgen"), 0);
	missModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("miss"), 0);

	if (!m_params.isecMain())
		hitsModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("hits"), 0);

	if (m_params.isecUsed())
		isecModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("isec"), 0);

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

	{
		const auto	hitModuleCount		= (m_params.isecAux() ? 2 : 1);
		const auto	rayTracingPipeline	= de::newMovePtr<RayTracingPipeline>();

		rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenModule, 0);
		rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, missModule, 1);

		if (!m_params.isecMain())
			rayTracingPipeline->addShader(m_params.testStage, hitsModule, 2);

		if (m_params.isecUsed())
			rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, isecModule, 2);

		pipeline = rayTracingPipeline->createPipeline(vkd, device, pipelineLayout.get());

		raygenSBT		= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
		raygenSBTRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenSBT->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);

		missSBT			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
		missSBTRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missSBT->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);

		hitSBT			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, 1);
		hitSBTRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitSBT->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize * hitModuleCount);
	}

	// Push constants.
	const auto			rotatedOrigin		= m_params.spaceObjects.origin * rotationMatrix;
	const auto			finalDirection		= m_params.spaceObjects.direction * scaleMatrix * rotationMatrix;
	const auto			distanceToEdge		= SpaceObjects::getDistanceToEdge(m_params.directionScale);
	const auto			tMinMax				= calcTminTmax(m_params.rayOriginType, m_params.rayEndtype, distanceToEdge);
	const PushConstants	pcData				=
	{
		toVec4(rotatedOrigin),	//	tcu::Vec4	origin;
		toVec4(finalDirection),	//	tcu::Vec4	direction;
		tMinMax.first,			//	float		tmix;
		tMinMax.second,			//	float		tmax;
	};

	// Trace rays.
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), stages, 0u, pcSize, &pcData);
	vkd.cmdTraceRaysKHR(cmdBuffer, &raygenSBTRegion, &missSBTRegion, &hitSBTRegion, &callableSBTRegion, 1u, 1u, 1u);

	// Barrier for the output buffer.
	const auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &bufferBarrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Read value back from the buffer.
	float bufferValue = 0.0f;
	invalidateAlloc(vkd, device, bufferAlloc);
	deMemcpy(&bufferValue, bufferAlloc.getHostPtr(), sizeof(bufferValue));

	if (m_params.rayEndtype == RayEndType::CROSS)
	{
		// Shooting from the ouside.
		if (de::abs(bufferValue - distanceToEdge) > kDefaultTolerance)
		{
			std::ostringstream msg;
			msg << "Result distance (" << bufferValue << ") differs from expected distance (" << distanceToEdge << ", tolerance " << kDefaultTolerance << ")";
			TCU_FAIL(msg.str());
		}
	}
	else
	{
		// Rays are shot from inside AABBs, rayTMin should be zero and the reported hit distance.
		if (bufferValue != 0.0f)
		{
			std::ostringstream msg;
			msg << "Result distance nonzero (" << bufferValue << ")";
			TCU_FAIL(msg.str());
		}
	}

	return tcu::TestStatus::pass("Pass");
}

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

// Generate a list of scaling factors suitable for the tests.
std::vector<float> generateScalingFactors (de::Random& rnd)
{
	const float	kMinScalingFactor			= 0.5f;
	const float	kMaxScalingFactor			= 10.0f;
	const int	kNumRandomScalingFactors	= 5;

	// Scaling factors: 1.0 and some randomly-generated ones.
	std::vector<float> scalingFactors;

	scalingFactors.reserve(kNumRandomScalingFactors + 1);
	scalingFactors.push_back(1.0f);

	for (int i = 0; i < kNumRandomScalingFactors; ++i)
		scalingFactors.push_back(rnd.getFloat() * (kMaxScalingFactor - kMinScalingFactor) + kMinScalingFactor);

	return scalingFactors;
}

// Generate a list of rotation angles suitable for the tests.
std::vector<std::pair<float, float>> generateRotationAngles (de::Random& rnd)
{
	const float	kPi2				= DE_PI * 2.0f;
	const int	kNumRandomRotations	= 4;

	// Rotations: 0.0 on both axis and some randomly-generated ones.
	std::vector<std::pair<float, float>> rotationAngles;

	rotationAngles.reserve(kNumRandomRotations + 1);
	rotationAngles.push_back(std::make_pair(0.0f, 0.0f));

	for (int i = 0; i < kNumRandomRotations; ++i)
		rotationAngles.push_back(std::make_pair(rnd.getFloat() * kPi2, rnd.getFloat() * kPi2));

	return rotationAngles;
}

} // anonymous

tcu::TestCaseGroup*	createDirectionLengthTests(tcu::TestContext& testCtx)
{
	GroupPtr directionGroup (new tcu::TestCaseGroup(testCtx, "direction_length", "Test direction vector length when tracing rays"));

	struct
	{
		VkShaderStageFlagBits	hitStage;
		const char*				name;
	} stages[] =
	{
		{ VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	"chit" },
		{ VK_SHADER_STAGE_ANY_HIT_BIT_KHR,		"ahit" },
		{ VK_SHADER_STAGE_INTERSECTION_BIT_KHR,	"isec" },
	};

	struct
	{
		VkGeometryTypeKHR		geometryType;
		const char*				name;
	} geometryTypes[] =
	{
		{ VK_GEOMETRY_TYPE_TRIANGLES_KHR,	"triangles"	},
		{ VK_GEOMETRY_TYPE_AABBS_KHR,		"aabbs"		},
	};

	de::Random	rnd(1613648516u);
	deUint32	caseCounter = 0u;

	// Scaling factors and rotation angles.
	const auto scalingFactors = generateScalingFactors(rnd);
	const auto rotationAngles = generateRotationAngles(rnd);

	for (int stageIdx = 0; stageIdx < DE_LENGTH_OF_ARRAY(stages); ++stageIdx)
	{
		const auto& stageData = stages[stageIdx];
		GroupPtr stageGroup (new tcu::TestCaseGroup(testCtx, stageData.name, ""));

		for (int geometryTypeIdx = 0; geometryTypeIdx < DE_LENGTH_OF_ARRAY(geometryTypes); ++geometryTypeIdx)
		{
			const auto& gType = geometryTypes[geometryTypeIdx];

			// We cannot test triangles with the ray intersection stage.
			if (gType.geometryType == VK_GEOMETRY_TYPE_TRIANGLES_KHR && stageData.hitStage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR)
				continue;

			GroupPtr geomGroup (new tcu::TestCaseGroup(testCtx, gType.name, ""));

			for (size_t scalingIdx = 0; scalingIdx < scalingFactors.size(); ++scalingIdx)
			{
				const auto scale		= scalingFactors[scalingIdx];
				const auto scaleName	= "scaling_factor_" + de::toString(scalingIdx);
				GroupPtr factorGroup (new tcu::TestCaseGroup(testCtx, scaleName.c_str(), ""));

				for (size_t rotationIdx = 0; rotationIdx < rotationAngles.size(); ++rotationIdx)
				{
					const auto angles		= rotationAngles[rotationIdx];
					const auto angleName	= "rotation_" + de::toString(rotationIdx);
					const auto geometryType	= gType.geometryType;
					const auto rayOrigType	= RayOriginType::OUTSIDE;
					const auto rayEndType	= RayEndType::CROSS;

					SpaceObjects spaceObjects(rayOrigType, geometryType);

					TestParams params =
					{
						spaceObjects,				//		SpaceObjects			spaceObjects;
						scale,						//		float					directionScale;
						angles.first,				//		float					rotationX;
						angles.second,				//		float					rotationY;
						stageData.hitStage,			//		VkShaderStageFlagBits	hitStage;
						geometryType,				//		VkGeometryTypeKHR		geometryType;
						// Use arrays of pointers when building the TLAS in every other test.
						(caseCounter % 2u == 0u),	//		bool					useArraysOfPointers;
						// Sometimes, update matrix after building the lop level AS and before submitting the command buffer.
						(caseCounter % 3u == 0u),	//		bool					updateMatrixAfterBuild;
						rayOrigType,				//		RayOriginType			rayOriginType;
						rayEndType,					//		RayEndType				rayEndType;
					};
					++caseCounter;

					factorGroup->addChild(new DirectionTestCase(testCtx, angleName, "", params));
				}

				geomGroup->addChild(factorGroup.release());
			}

			stageGroup->addChild(geomGroup.release());
		}

		directionGroup->addChild(stageGroup.release());
	}

	return directionGroup.release();
}

tcu::TestCaseGroup*	createInsideAABBsTests(tcu::TestContext& testCtx)
{
	GroupPtr insideAABBsGroup (new tcu::TestCaseGroup(testCtx, "inside_aabbs", "Test shooting rays that start inside AABBs"));

	struct
	{
		VkShaderStageFlagBits	hitStage;
		const char*				name;
	} stages[] =
	{
		{ VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	"chit" },
		{ VK_SHADER_STAGE_ANY_HIT_BIT_KHR,		"ahit" },
		{ VK_SHADER_STAGE_INTERSECTION_BIT_KHR,	"isec" },
	};

	struct
	{
		RayEndType				rayEndType;
		const char*				name;
	} rayEndCases[] =
	{
		{	RayEndType::ZERO,			"tmax_zero"	},
		{	RayEndType::INSIDE,			"inside"	},
		{	RayEndType::EDGE,			"edge"		},
		{	RayEndType::OUTSIDE,		"outside"	},
	};

	de::Random rnd(1621936010u);

	// Scaling factors and rotation angles.
	const auto scalingFactors = generateScalingFactors(rnd);
	const auto rotationAngles = generateRotationAngles(rnd);

	for (int stageIdx = 0; stageIdx < DE_LENGTH_OF_ARRAY(stages); ++stageIdx)
	{
		const auto& stageData = stages[stageIdx];
		GroupPtr stageGroup (new tcu::TestCaseGroup(testCtx, stageData.name, ""));

		for (int rayEndCaseIdx = 0; rayEndCaseIdx < DE_LENGTH_OF_ARRAY(rayEndCases); ++rayEndCaseIdx)
		{
			const auto&			rayEndCase	= rayEndCases[rayEndCaseIdx];
			const std::string	rayEndName	= std::string("ray_end_") + rayEndCase.name;
			GroupPtr			rayEndGroup	(new tcu::TestCaseGroup(testCtx, rayEndName.c_str(), ""));

			for (size_t scalingIdx = 0; scalingIdx < scalingFactors.size(); ++scalingIdx)
			{
				const auto scale		= scalingFactors[scalingIdx];
				const auto scaleName	= "scaling_factor_" + de::toString(scalingIdx);
				GroupPtr factorGroup (new tcu::TestCaseGroup(testCtx, scaleName.c_str(), ""));

				for (size_t rotationIdx = 0; rotationIdx < rotationAngles.size(); ++rotationIdx)
				{
					const auto angles		= rotationAngles[rotationIdx];
					const auto angleName	= "rotation_" + de::toString(rotationIdx);
					const auto geometryType	= VK_GEOMETRY_TYPE_AABBS_KHR;
					const auto rayOrigType	= RayOriginType::INSIDE;

					SpaceObjects spaceObjects(rayOrigType, geometryType);

					TestParams params =
					{
						spaceObjects,					//		SpaceObjects			spaceObjects;
						scale,							//		float					directionScale;
						angles.first,					//		float					rotationX;
						angles.second,					//		float					rotationY;
						stageData.hitStage,				//		VkShaderStageFlagBits	hitStage;
						geometryType,					//		VkGeometryTypeKHR		geometryType;
						false,							//		bool					useArraysOfPointers;
						false,							//		bool					updateMatrixAfterBuild;
						rayOrigType,					//		RayOriginType			rayOriginType;
						rayEndCase.rayEndType,			//		RayEndType				rayEndType;
					};

					factorGroup->addChild(new DirectionTestCase(testCtx, angleName, "", params));
				}

				rayEndGroup->addChild(factorGroup.release());
			}

			stageGroup->addChild(rayEndGroup.release());
		}

		insideAABBsGroup->addChild(stageGroup.release());
	}

	return insideAABBsGroup.release();
}

} // RayTracing
} // vkt
