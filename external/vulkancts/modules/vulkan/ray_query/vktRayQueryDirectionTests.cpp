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
 * \brief Ray Query Direction Tests
 *//*--------------------------------------------------------------------*/

#include "vktRayQueryDirectionTests.hpp"
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
namespace RayQuery
{

namespace
{

using namespace vk;

using GeometryData = std::vector<tcu::Vec3>;

struct SpaceObjects
{
	tcu::Vec3		origin;
	tcu::Vec3		direction;
	GeometryData	geometry;

	SpaceObjects (VkGeometryTypeKHR geometryType)
		: origin	(0.0f, 0.0f, 1.0f)	// Origin of the ray at (0, 0, 1).
		, direction	(0.0f, 0.0f, 1.0f)	// Shooting towards (0, 0, 1).
		, geometry	()
	{
		// Triangle or AABB around (0, 0, 5).
		DE_ASSERT(geometryType == VK_GEOMETRY_TYPE_TRIANGLES_KHR || geometryType == VK_GEOMETRY_TYPE_AABBS_KHR);
		if (geometryType == VK_GEOMETRY_TYPE_TRIANGLES_KHR)
		{
			geometry.reserve(3u);
			geometry.push_back(tcu::Vec3( 0.0f,  0.5f, 5.0f));
			geometry.push_back(tcu::Vec3(-0.5f, -0.5f, 5.0f));
			geometry.push_back(tcu::Vec3( 0.5f, -0.5f, 5.0f));
		}
		else
		{
			geometry.reserve(2u);
			geometry.push_back(tcu::Vec3(-0.5f, -0.5f, 5.0f));
			geometry.push_back(tcu::Vec3( 0.5f,  0.5f, 5.0f));
		}
	}

	static float getDefaultDistance (void)
	{
		// Consistent with the Z coordinates of the origin, direction and points in the default constructor.
		return 4.0f;
	}

	// Calculates expected distance given the direction scaling factor.
	static float getExpectedDistance (float directionScale)
	{
		return getDefaultDistance() / directionScale;
	}
};

// Default test tolerance for distance values.
constexpr float kDefaultTolerance = 0.001f;

// Calculates appropriate values for Tmin/Tmax given the expected distance.
std::pair<float, float> calcTminTmax (float expectedDistance)
{
	const auto margin = kDefaultTolerance / 2.0f;
	return std::make_pair(de::max(expectedDistance - margin, 0.0f), expectedDistance + margin);
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
	VkGeometryTypeKHR		geometryType;
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
	context.requireDeviceFunctionality("VK_KHR_ray_query");
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

	std::ostringstream comp;
	comp
		<< "#version 460 core\n"
		<< "#extension GL_EXT_ray_query : require\n"
		<< "\n"
		<< "layout(local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
		<< "\n"
		<< "layout(set=0, binding=0) uniform accelerationStructureEXT topLevelAS;\n"
		<< "layout(set=0, binding=1, std430) buffer OutBuffer { float val; } outBuffer;\n"
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
		<< "  const uint  cullMask = 0xFF;\n"
		<< "  float       outVal   = -10000.0f;\n"
		<< "  rayQueryEXT rq;\n"
		<< "  rayQueryInitializeEXT(rq, topLevelAS, gl_RayFlagsNoneEXT, cullMask, pc.origin.xyz, pc.tmin, pc.direction.xyz, pc.tmax);\n"
		<< "  while (rayQueryProceedEXT(rq)) {\n"
		<< "    const uint candidateType = rayQueryGetIntersectionTypeEXT(rq, false);\n"
		<< "    if (candidateType == gl_RayQueryCandidateIntersectionTriangleEXT) {\n"
		<< "      outVal = rayQueryGetIntersectionTEXT(rq, false);\n"
		<< "    }\n"
		<< "    else if (candidateType == gl_RayQueryCandidateIntersectionAABBEXT) {\n"
		<< "      outVal = pc.tmax;\n"
		<< "    }\n"
		<< "  }\n"
		<< "  outBuffer.val = outVal;\n"
		<< "}\n"
		;

	programCollection.glslSources.add("comp") << glu::ComputeSource(updateRayTracingGLSL(comp.str())) << buildOptions;
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
	const auto&	vkd		= m_context.getDeviceInterface();
	const auto	device	= m_context.getDevice();
	auto&		alloc	= m_context.getDefaultAllocator();
	const auto	qIndex	= m_context.getUniversalQueueFamilyIndex();
	const auto	queue	= m_context.getUniversalQueue();
	const auto	stages	= VK_SHADER_STAGE_COMPUTE_BIT;
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
	topLevelAS->setInstanceCount(1);
	topLevelAS->addInstance(blasSharedPtr, transformMatrix, 0, 0xFFu, 0u, instanceFlags);
	topLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc);

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

	const auto pcRange = makePushConstantRange(stages, 0u, pcSize);
	const auto pipelineLayout = makePipelineLayout(vkd, device, 1u, &setLayout.get(), 1u, &pcRange);

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

	// Shader module and pipeline.
	const auto compModule = createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0);

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
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,			//	VkStructureType					sType;
		nullptr,												//	const void*						pNext;
		0u,														//	VkPipelineCreateFlags			flags;
		shaderInfo,												//	VkPipelineShaderStageCreateInfo	stage;
		pipelineLayout.get(),									//	VkPipelineLayout				layout;
		DE_NULL,												//	VkPipeline						basePipelineHandle;
		0,														//	deInt32							basePipelineIndex;
	};
	const auto pipeline = createComputePipeline(vkd, device, DE_NULL, &pipelineInfo);

	// Push constants.
	const auto			rotatedOrigin		= m_params.spaceObjects.origin * rotationMatrix;
	const auto			finalDirection		= m_params.spaceObjects.direction * scaleMatrix * rotationMatrix;
	const auto			expectedDistance	= SpaceObjects::getExpectedDistance(m_params.directionScale);
	const auto			tMinMax				= calcTminTmax(expectedDistance);
	const PushConstants	pcData				=
	{
		toVec4(rotatedOrigin),	//	tcu::Vec4	origin;
		toVec4(finalDirection),	//	tcu::Vec4	direction;
		tMinMax.first,			//	float		tmix;
		tMinMax.second,			//	float		tmax;
	};

	// Trace rays.
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), stages, 0u, pcSize, &pcData);
	vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);

	// Barrier for the output buffer.
	const auto bufferBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &bufferBarrier, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Read value back from the buffer.
	float bufferValue = 0.0f;
	invalidateAlloc(vkd, device, bufferAlloc);
	deMemcpy(&bufferValue, bufferAlloc.getHostPtr(), sizeof(bufferValue));

	if (de::abs(bufferValue - expectedDistance) > kDefaultTolerance)
	{
		std::ostringstream msg;
		msg << "Result distance (" << bufferValue << ") differs from expected distance (" << expectedDistance << ", tolerance " << kDefaultTolerance << ")";
		TCU_FAIL(msg.str());
	}

	return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup*	createDirectionTests(tcu::TestContext& testCtx)
{
	using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

	GroupPtr directionGroup (new tcu::TestCaseGroup(testCtx, "direction_length", "Test direction vector length when using ray queries"));

	struct
	{
		VkGeometryTypeKHR		geometryType;
		const char*				name;
	} geometryTypes[] =
	{
		{ VK_GEOMETRY_TYPE_TRIANGLES_KHR,	"triangles"	},
		{ VK_GEOMETRY_TYPE_AABBS_KHR,		"aabbs"		},
	};

	const float kPi2				= DE_PI * 2.0f;
	const float kMinScalingFactor	= 0.5f;
	const float kMaxScalingFactor	= 10.0f;

	const int kNumRandomScalingFactors	= 5;
	const int kNumRandomRotations		= 4;

	de::Random rnd(1614686501u);

	// Scaling factors: 1.0 and some randomly-generated ones.
	std::vector<float> scalingFactors;

	scalingFactors.reserve(kNumRandomScalingFactors + 1);
	scalingFactors.push_back(1.0f);

	for (int i = 0; i < kNumRandomScalingFactors; ++i)
		scalingFactors.push_back(rnd.getFloat() * (kMaxScalingFactor - kMinScalingFactor) + kMinScalingFactor);

	// Rotations: 0.0 on both axis and some randomly-generated ones.
	std::vector<std::pair<float, float>> rotationAngles;

	rotationAngles.reserve(kNumRandomRotations + 1);
	rotationAngles.push_back(std::make_pair(0.0f, 0.0f));

	for (int i = 0; i < kNumRandomRotations; ++i)
		rotationAngles.push_back(std::make_pair(rnd.getFloat() * kPi2, rnd.getFloat() * kPi2));

	for (int geometryTypeIdx = 0; geometryTypeIdx < DE_LENGTH_OF_ARRAY(geometryTypes); ++geometryTypeIdx)
	{
		const auto& gType = geometryTypes[geometryTypeIdx];

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

				SpaceObjects spaceObjects(geometryType);

				TestParams params =
				{
					spaceObjects,			//		SpaceObjects			spaceObjects;
					scale,					//		float					directionScale;
					angles.first,			//		float					rotationX;
					angles.second,			//		float					rotationY;
					geometryType,			//		VkGeometryTypeKHR		geometryType;
				};

				factorGroup->addChild(new DirectionTestCase(testCtx, angleName, "", params));
			}

			geomGroup->addChild(factorGroup.release());
		}

		directionGroup->addChild(geomGroup.release());
	}

	return directionGroup.release();
}

} // RayQuery
} // vkt
