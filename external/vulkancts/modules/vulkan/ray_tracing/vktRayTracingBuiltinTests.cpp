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
 * \brief Ray Tracing Builtin and specialization constant tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingBuiltinTests.hpp"

#include "vkDefs.hpp"

#include "vktTestCase.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"

#include "vkRayTracingUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuMatrix.hpp"

#include "deMath.h"

namespace vkt
{
namespace RayTracing
{
namespace
{
using namespace vk;
using namespace std;

static const VkFlags	ALL_RAY_TRACING_STAGES	= VK_SHADER_STAGE_RAYGEN_BIT_KHR
												| VK_SHADER_STAGE_ANY_HIT_BIT_KHR
												| VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
												| VK_SHADER_STAGE_MISS_BIT_KHR
												| VK_SHADER_STAGE_INTERSECTION_BIT_KHR
												| VK_SHADER_STAGE_CALLABLE_BIT_KHR;

enum GeomType
{
	GEOM_TYPE_TRIANGLES,
	GEOM_TYPE_AABBS,
};

enum TestId
{
	TEST_ID_LAUNCH_ID_EXT				= 0,
	TEST_ID_LAUNCH_SIZE_EXT,
	TEST_ID_PRIMITIVE_ID,
	TEST_ID_INSTANCE_ID,
	TEST_ID_INSTANCE_CUSTOM_INDEX_EXT,
	TEST_ID_GEOMETRY_INDEX_EXT,
	TEST_ID_WORLD_RAY_ORIGIN_EXT,
	TEST_ID_WORLD_RAY_DIRECTION_EXT,
	TEST_ID_OBJECT_RAY_ORIGIN_EXT,
	TEST_ID_OBJECT_RAY_DIRECTION_EXT,
	TEST_ID_RAY_T_MIN_EXT,
	TEST_ID_RAY_T_MAX_EXT,
	TEST_ID_INCOMING_RAY_FLAGS_EXT,
	TEST_ID_HIT_T_EXT,
	TEST_ID_HIT_KIND_EXT,
	TEST_ID_OBJECT_TO_WORLD_EXT,
	TEST_ID_OBJECT_TO_WORLD_3X4_EXT,
	TEST_ID_WORLD_TO_OBJECT_EXT,
	TEST_ID_WORLD_TO_OBJECT_3X4_EXT,
	TEST_ID_LAST
};

enum RayFlagBits
{
	RAY_FLAG_BIT_OPAQUE_EXT							= 0,	//  const uint gl_RayFlagsOpaqueEXT = 1U;
	RAY_FLAG_BIT_NO_OPAQUE_EXT						= 1,	//  const uint gl_RayFlagsNoOpaqueEXT = 2U;
	RAY_FLAG_BIT_TERMINATE_ON_FIRST_HIT_EXT			= 2,	//  const uint gl_RayFlagsTerminateOnFirstHitEXT = 4U;
	RAY_FLAG_BIT_SKIP_CLOSEST_HIT_SHADER_EXT		= 3,	//  const uint gl_RayFlagsSkipClosestHitShaderEXT = 8U;
	RAY_FLAG_BIT_CULL_BACK_FACING_TRIANGLES_EXT		= 4,	//  const uint gl_RayFlagsCullBackFacingTrianglesEXT = 16U;
	RAY_FLAG_BIT_CULL_FRONT_FACING_TRIANGLES_EXT	= 5,	//  const uint gl_RayFlagsCullFrontFacingTrianglesEXT = 32U;
	RAY_FLAG_BIT_CULL_OPAQUE_EXT					= 6,	//  const uint gl_RayFlagsCullOpaqueEXT = 64U;
	RAY_FLAG_BIT_CULL_NO_OPAQUE_EXT					= 7,	//  const uint gl_RayFlagsCullNoOpaqueEXT = 128U;
	RAY_FLAG_BIT_LAST_PER_TEST,
	RAY_FLAG_BIT_SKIP_TRIANGLES_EXT					= 8,	//  const uint gl_RayFlagsSkipTrianglesEXT = 256U;
	RAY_FLAG_BIT_SKIP_AABB_EXT						= 9,	//  const uint gl_RayFlagsSkipAABBEXT = 512U;
	RAY_FLAG_BIT_LAST
};

struct CaseDef
{
	TestId					id;
	const char*				name;
	deUint32				width;
	deUint32				height;
	deUint32				depth;
	deUint32				raysDepth;
	VkFormat				format;
	bool					fixedPointScalarOutput;
	bool					fixedPointVectorOutput;
	bool					fixedPointMatrixOutput;
	GeomType				geomType;
	deUint32				squaresGroupCount;
	deUint32				geometriesGroupCount;
	deUint32				instancesGroupCount;
	VkShaderStageFlagBits	stage;
	bool					rayFlagSkipTriangles;
	bool					rayFlagSkipAABSs;
	bool					opaque;
	bool					frontFace;
	VkPipelineCreateFlags	pipelineCreateFlags;
	bool					useSpecConstants;
};

const deUint32	DEFAULT_UINT_CLEAR_VALUE	= 0x8000;
const deUint32	FIXED_POINT_DIVISOR			= 1024 * 1024;
const deUint32	FIXED_POINT_ALLOWED_ERROR	= 4;

bool isPlain (const deUint32 width, const deUint32 height, const deUint32 depth)
{
	return (width == 1 || height == 1 || depth == 1);
}

deUint32 getShaderGroupSize (const InstanceInterface&	vki,
							 const VkPhysicalDevice		physicalDevice)
{
	de::MovePtr<RayTracingProperties>	rayTracingPropertiesKHR;

	rayTracingPropertiesKHR	= makeRayTracingProperties(vki, physicalDevice);
	return rayTracingPropertiesKHR->getShaderGroupHandleSize();
}

deUint32 getShaderGroupBaseAlignment (const InstanceInterface&	vki,
									  const VkPhysicalDevice	physicalDevice)
{
	de::MovePtr<RayTracingProperties>	rayTracingPropertiesKHR;

	rayTracingPropertiesKHR = makeRayTracingProperties(vki, physicalDevice);
	return rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
}

VkImageCreateInfo makeImageCreateInfo (deUint32 width, deUint32 height, deUint32 depth, VkFormat format)
{
	const VkImageType		imageType		= VK_IMAGE_TYPE_3D;
	const VkImageUsageFlags	usage			= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	const VkImageCreateInfo	imageCreateInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		(VkImageCreateFlags)0u,					// VkImageCreateFlags		flags;
		imageType,								// VkImageType				imageType;
		format,									// VkFormat					format;
		makeExtent3D(width, height, depth),		// VkExtent3D				extent;
		1u,										// deUint32					mipLevels;
		1u,										// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		usage,									// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		0u,										// deUint32					queueFamilyIndexCount;
		DE_NULL,								// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout;
	};

	return imageCreateInfo;
}

class RayTracingBuiltinLaunchTestInstance : public TestInstance
{
public:
																RayTracingBuiltinLaunchTestInstance		(Context& context, const CaseDef& data);
																~RayTracingBuiltinLaunchTestInstance	(void);
	tcu::TestStatus												iterate									(void);

protected:
	void														checkSupportInInstance					(void) const;
	Move<VkPipeline>											makePipeline							(de::MovePtr<RayTracingPipeline>&							rayTracingPipeline,
																										 VkPipelineLayout											pipelineLayout,
																										 const VkSpecializationInfo*								specializationInfo);
	std::vector<deInt32>										expectedIntValuesBuffer					(void);
	std::vector<float>											expectedFloatValuesBuffer				(void);
	std::vector<float>											expectedVectorValuesBuffer				(void);
	std::vector<float>											expectedMatrixValuesBuffer				(void);
	de::MovePtr<BufferWithMemory>								runTest									(void);
	de::MovePtr<BufferWithMemory>								createShaderBindingTable				(const InstanceInterface&									vki,
																										 const DeviceInterface&										vkd,
																										 const VkDevice												device,
																										 const VkPhysicalDevice										physicalDevice,
																										 const VkPipeline											pipeline,
																										 Allocator&													allocator,
																										 de::MovePtr<RayTracingPipeline>&							rayTracingPipeline,
																										 const deUint32												group);

	bool														validateIntBuffer						(de::MovePtr<BufferWithMemory>								buffer);
	bool														validateFloatBuffer						(de::MovePtr<BufferWithMemory>								buffer);
	bool														validateVectorBuffer					(de::MovePtr<BufferWithMemory>								buffer);
	bool														validateMatrixBuffer					(de::MovePtr<BufferWithMemory>								buffer);

	de::MovePtr<TopLevelAccelerationStructure>					initTopAccelerationStructure			(VkCommandBuffer											cmdBuffer,
																										 vector<de::SharedPtr<BottomLevelAccelerationStructure> >&	bottomLevelAccelerationStructures);
	vector<de::SharedPtr<BottomLevelAccelerationStructure>	>	initBottomAccelerationStructures		(VkCommandBuffer	cmdBuffer);
	de::MovePtr<BottomLevelAccelerationStructure>				initBottomAccelerationStructure			(VkCommandBuffer	cmdBuffer,
																										 tcu::UVec2&		startPos);

private:
	CaseDef														m_data;
	VkShaderStageFlags											m_shaders;
	deUint32													m_raygenShaderGroup;
	deUint32													m_missShaderGroup;
	deUint32													m_hitShaderGroup;
	deUint32													m_callableShaderGroup;
	deUint32													m_shaderGroupCount;
};

RayTracingBuiltinLaunchTestInstance::RayTracingBuiltinLaunchTestInstance (Context& context, const CaseDef& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
	, m_shaders				(0)
	, m_raygenShaderGroup	(~0u)
	, m_missShaderGroup		(~0u)
	, m_hitShaderGroup		(~0u)
	, m_callableShaderGroup	(~0u)
	, m_shaderGroupCount	(0)
{
	const VkShaderStageFlags	hitStages	= VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
	BinaryCollection&			collection	= m_context.getBinaryCollection();
	deUint32					group		= 0;
	deUint32					shaderCount	= 0;

	if (collection.contains("rgen")) m_shaders |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	if (collection.contains("ahit")) m_shaders |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	if (collection.contains("chit")) m_shaders |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	if (collection.contains("miss")) m_shaders |= VK_SHADER_STAGE_MISS_BIT_KHR;
	if (collection.contains("sect")) m_shaders |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
	if (collection.contains("call")) m_shaders |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;

	for (BinaryCollection::Iterator it =  collection.begin(); it != collection.end(); ++it)
		shaderCount++;

	if (shaderCount != (deUint32)dePop32(m_shaders))
		TCU_THROW(InternalError, "Unused shaders detected in the collection");

	if (0 != (m_shaders & VK_SHADER_STAGE_RAYGEN_BIT_KHR))
		m_raygenShaderGroup		= group++;

	if (0 != (m_shaders & VK_SHADER_STAGE_MISS_BIT_KHR))
		m_missShaderGroup		= group++;

	if (0 != (m_shaders & hitStages))
		m_hitShaderGroup		= group++;

	if (0 != (m_shaders & VK_SHADER_STAGE_CALLABLE_BIT_KHR))
		m_callableShaderGroup	= group++;

	m_shaderGroupCount = group;
}

RayTracingBuiltinLaunchTestInstance::~RayTracingBuiltinLaunchTestInstance (void)
{
}

class RayTracingTestCase : public TestCase
{
	public:
										RayTracingTestCase			(tcu::TestContext& context, const char* name, const char* desc, const CaseDef data);
										~RayTracingTestCase			(void);

	virtual	void						initPrograms				(SourceCollections& programCollection) const;
	virtual TestInstance*				createInstance				(Context& context) const;
	virtual void						checkSupport				(Context& context) const;

private:
	static inline const std::string		getIntersectionPassthrough	(void);
	static inline const std::string		getMissPassthrough			(void);
	static inline const std::string		getHitPassthrough			(void);

	CaseDef								m_data;
};

RayTracingTestCase::RayTracingTestCase (tcu::TestContext& context, const char* name, const char* desc, const CaseDef data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

RayTracingTestCase::~RayTracingTestCase	(void)
{
}

void RayTracingTestCase::checkSupport(Context& context) const
{
	const bool	pipelineFlagSkipTriangles	= ((m_data.pipelineCreateFlags & VK_PIPELINE_CREATE_RAY_TRACING_SKIP_TRIANGLES_BIT_KHR) != 0);
	const bool	pipelineFlagSkipAABSs		= ((m_data.pipelineCreateFlags & VK_PIPELINE_CREATE_RAY_TRACING_SKIP_AABBS_BIT_KHR) != 0);
	const bool	cullingFlags				=  m_data.rayFlagSkipTriangles
											|| m_data.rayFlagSkipAABSs
											|| pipelineFlagSkipTriangles
											|| pipelineFlagSkipAABSs;

	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

	const VkPhysicalDeviceRayTracingPipelineFeaturesKHR&	rayTracingPipelineFeaturesKHR		= context.getRayTracingPipelineFeatures();
	if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == DE_FALSE )
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

	const VkPhysicalDeviceAccelerationStructureFeaturesKHR&	accelerationStructureFeaturesKHR	= context.getAccelerationStructureFeatures();
	if (accelerationStructureFeaturesKHR.accelerationStructure == DE_FALSE)
		TCU_THROW(TestError, "VK_KHR_ray_tracing_pipeline requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

	if (cullingFlags && rayTracingPipelineFeaturesKHR.rayTraversalPrimitiveCulling == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTraversalPrimitiveCulling");
}

const std::string RayTracingTestCase::getIntersectionPassthrough (void)
{
	const std::string intersectionPassthrough =
		"#version 460 core\n"
		"#extension GL_EXT_ray_tracing : require\n"
		"hitAttributeEXT vec3 hitAttribute;\n"
		"\n"
		"void main()\n"
		"{\n"
		"  reportIntersectionEXT(0.95f, 0x7Eu);\n"
		"}\n";

	return intersectionPassthrough;
}

const std::string RayTracingTestCase::getMissPassthrough (void)
{
	const std::string missPassthrough =
		"#version 460 core\n"
		"#extension GL_EXT_ray_tracing : require\n"
		"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
		"\n"
		"void main()\n"
		"{\n"
		"}\n";

	return missPassthrough;
}

const std::string RayTracingTestCase::getHitPassthrough (void)
{
	const std::string hitPassthrough =
		"#version 460 core\n"
		"#extension GL_EXT_ray_tracing : require\n"
		"hitAttributeEXT vec3 attribs;\n"
		"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
		"\n"
		"void main()\n"
		"{\n"
		"}\n";

	return hitPassthrough;
}

void RayTracingTestCase::initPrograms (SourceCollections& programCollection) const
{
	const bool useSC = m_data.useSpecConstants;
	DE_ASSERT(!useSC || m_data.id == TEST_ID_LAUNCH_ID_EXT);

	const vk::ShaderBuildOptions	buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	if (m_data.id == TEST_ID_LAUNCH_ID_EXT || m_data.id == TEST_ID_LAUNCH_SIZE_EXT)
	{
		const std::string	specConstants	=
			"layout (constant_id=0) const highp int factor1 = 1;\n"
			"layout (constant_id=1) const highp float factor2 = 2.0;\n"
			;

		const std::string	updateImage		=
			"  ivec3 p = ivec3(gl_LaunchIDEXT);\n"
			"  ivec3 v = ivec3(gl_" + std::string(m_data.name) + ");\n"
			"  int   r = v.x + " + (useSC ? "factor1" : "256") + " * (v.y + " + (useSC ? "int(factor2)" : "256") + " * v.z) + 1;\n"
			"  ivec4 c = ivec4(r,0,0,1);\n"
			"  imageStore(result, p, c);\n";

		switch (m_data.stage)
		{
			case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
			{
				std::stringstream css;
				css <<
					"#version 460 core\n"
					"#extension GL_EXT_ray_tracing : require\n"
					<< (useSC ? specConstants : "") <<
					"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
					"\n"
					"void main()\n"
					"{\n"
					<< updateImage <<
					"}\n";

				programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

				{
					std::stringstream css;
					css <<
						"#version 460 core\n"
						"#extension GL_EXT_ray_tracing : require\n"
						<< (useSC ? specConstants : "") <<
						"hitAttributeEXT vec3 attribs;\n"
						"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
						"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< updateImage <<
						"}\n";

					programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

				{
					std::stringstream css;
					css <<
						"#version 460 core\n"
						"#extension GL_EXT_ray_tracing : require\n"
						<< (useSC ? specConstants : "") <<
						"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
						"hitAttributeEXT vec3 attribs;\n"
						"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< updateImage <<
						"}\n";

					programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

				{
					std::stringstream css;
					css <<
						"#version 460 core\n"
						"#extension GL_EXT_ray_tracing : require\n"
						<< (useSC ? specConstants : "") <<
						"hitAttributeEXT vec3 hitAttribute;\n"
						"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< updateImage <<
						"  hitAttribute = vec3(0.0f, 0.0f, 0.0f);\n"
						"  reportIntersectionEXT(1.0f, 0);\n"
						"}\n";

					programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_MISS_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

				{
					std::stringstream css;
					css <<
						"#version 460 core\n"
						"#extension GL_EXT_ray_tracing : require\n"
						<< (useSC ? specConstants : "") <<
						"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
						"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< updateImage <<
						"}\n";

					programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
			{
				{
					std::stringstream css;
					css <<
						"#version 460 core\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"layout(location = 0) callableDataEXT float dummy;"
						"layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
						"\n"
						"void main()\n"
						"{\n"
						"  executeCallableEXT(0, 0);\n"
						"}\n";

					programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				{
					std::stringstream css;
					css <<
						"#version 460 core\n"
						"#extension GL_EXT_ray_tracing : require\n"
						<< (useSC ? specConstants : "") <<
						"layout(location = 0) callableDataInEXT float dummy;"
						"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< updateImage <<
						"}\n";

					programCollection.glslSources.add("call") << glu::CallableSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

				break;
			}

			default:
				TCU_THROW(InternalError, "Unknown stage");
		}
	}
	else if (m_data.id == TEST_ID_GEOMETRY_INDEX_EXT		||
			 m_data.id == TEST_ID_PRIMITIVE_ID				||
			 m_data.id == TEST_ID_INSTANCE_ID				||
			 m_data.id == TEST_ID_INSTANCE_CUSTOM_INDEX_EXT	||
			 m_data.id == TEST_ID_HIT_KIND_EXT				)
	{
		const std::string	conditionGeometryIndex	= "  int   n = int(gl_LaunchIDEXT.x + gl_LaunchSizeEXT.x * (gl_LaunchIDEXT.y + gl_LaunchSizeEXT.y * gl_LaunchIDEXT.z));\n"
													  "  int   m = (n / " + de::toString(m_data.squaresGroupCount) + ") % " + de::toString(m_data.geometriesGroupCount) + ";\n"
													  "  if (r == m)";
		const std::string	conditionPrimitiveId	= "  int   n = int(gl_LaunchIDEXT.x + gl_LaunchSizeEXT.x * (gl_LaunchIDEXT.y + gl_LaunchSizeEXT.y * gl_LaunchIDEXT.z));\n"
													  "  int   m = n % " + de::toString(m_data.squaresGroupCount) + ";\n"
													  "  if (r == m)";
		const std::string	condition				= (m_data.id == TEST_ID_GEOMETRY_INDEX_EXT) && (m_data.geomType == GEOM_TYPE_AABBS) ? conditionGeometryIndex
													: (m_data.id == TEST_ID_PRIMITIVE_ID) && (m_data.geomType == GEOM_TYPE_AABBS) ? conditionPrimitiveId
													: "";
		const std::string	updateImage				=
			"  ivec3 p = ivec3(gl_LaunchIDEXT);\n"
			"  int   r = int(gl_" + std::string(m_data.name) + ");\n"
			"  ivec4 c = ivec4(r,0,0,1);\n"
			+ condition + "  imageStore(result, p, c);\n";

		switch (m_data.stage)
		{
			case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

				{
					std::stringstream css;
					css <<
						"#version 460 core\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"hitAttributeEXT vec3 attribs;\n"
						"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
						"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< updateImage <<
						"}\n";

					programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

				if (m_data.geomType == GEOM_TYPE_AABBS)
					programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(getIntersectionPassthrough())) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

				{
					std::stringstream css;
					css <<
						"#version 460 core\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
						"hitAttributeEXT vec3 attribs;\n"
						"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< updateImage <<
						"}\n";

					programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

				if (m_data.geomType == GEOM_TYPE_AABBS)
				{
					const std::string intersectionShaderSingle	=
						"#version 460 core\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"hitAttributeEXT vec3 hitAttribute;\n"
						"\n"
						"void main()\n"
						"{\n"
						"  int r = int(gl_" + std::string(m_data.name) + ");\n"
						+ condition + "  reportIntersectionEXT(0.95f, 0x7Eu);\n"
						"}\n";
					const std::string intersectionShader		= condition.empty() ? getIntersectionPassthrough() : intersectionShaderSingle;

					programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(intersectionShader)) << buildOptions;
				}

				break;
			}

			case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

				{
					std::stringstream css;
					css <<
						"#version 460 core\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"hitAttributeEXT vec3 hitAttribute;\n"
						"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< updateImage <<
						"  reportIntersectionEXT(0.95f, 0);\n"
						"}\n";

					programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

				break;
			}

			default:
				TCU_THROW(InternalError, "Unknown stage");
		}
	}
	else if (m_data.id == TEST_ID_INCOMING_RAY_FLAGS_EXT)
	{
		const bool			cullingFlags			= m_data.rayFlagSkipTriangles || m_data.rayFlagSkipAABSs;
		const std::string	cullingFlagsInit		= (m_data.rayFlagSkipTriangles && m_data.rayFlagSkipAABSs) ? "gl_RayFlagsSkipTrianglesEXT|gl_RayFlagsSkipAABBEXT"
													: m_data.rayFlagSkipTriangles ? "gl_RayFlagsSkipTrianglesEXT"
													: m_data.rayFlagSkipAABSs ? "gl_RayFlagsSkipAABBEXT"
													: "gl_RayFlagsNoneEXT";
		const std::string	updateImage				=
			"  ivec3 p = ivec3(gl_LaunchIDEXT);\n"
			"  int   r = int(gl_" + std::string(m_data.name) + ");\n"
			"  ivec4 c = ivec4(r,0,0,1);\n"
			"  imageStore(result, p, c);\n";
		const std::string	intersectionShader		=
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"hitAttributeEXT vec3 hitAttribute;\n"
			"\n"
			"void main()\n"
			"{\n"
			"  uint hitKind = " + std::string(m_data.frontFace ? "0x7Eu" : "0x7Fu") + ";\n"
			"  reportIntersectionEXT(0.95f, hitKind);\n"
			"}\n";
		const std::string	raygenFlagsFragment		=
			"\n"
			"  if      (0 != (n & (1<<" + de::toString(RAY_FLAG_BIT_OPAQUE_EXT                     ) + "))) f = f | gl_RayFlagsOpaqueEXT;\n"
			"  else if (0 != (n & (1<<" + de::toString(RAY_FLAG_BIT_NO_OPAQUE_EXT                  ) + "))) f = f | gl_RayFlagsNoOpaqueEXT;\n"
			"  else if (0 != (n & (1<<" + de::toString(RAY_FLAG_BIT_CULL_OPAQUE_EXT                ) + "))) f = f | gl_RayFlagsCullOpaqueEXT;\n"
			"  else if (0 != (n & (1<<" + de::toString(RAY_FLAG_BIT_CULL_NO_OPAQUE_EXT             ) + "))) f = f | gl_RayFlagsCullNoOpaqueEXT;\n"
			"\n"
			"  if      (0 != (n & (1<<" + de::toString(RAY_FLAG_BIT_CULL_BACK_FACING_TRIANGLES_EXT ) + "))) f = f | gl_RayFlagsCullBackFacingTrianglesEXT;\n"
			"  else if (0 != (n & (1<<" + de::toString(RAY_FLAG_BIT_CULL_FRONT_FACING_TRIANGLES_EXT) + "))) f = f | gl_RayFlagsCullFrontFacingTrianglesEXT;\n"
			"\n"
			"  if      (0 != (n & (1<<" + de::toString(RAY_FLAG_BIT_TERMINATE_ON_FIRST_HIT_EXT     ) + "))) f = f | gl_RayFlagsTerminateOnFirstHitEXT;\n"
			"  if      (0 != (n & (1<<" + de::toString(RAY_FLAG_BIT_SKIP_CLOSEST_HIT_SHADER_EXT    ) + "))) f = f | gl_RayFlagsSkipClosestHitShaderEXT;\n"
			"\n";
		const std::string	raygenShader			=
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			+ (cullingFlags ? std::string("#extension GL_EXT_ray_flags_primitive_culling : require\n") : "") +
			"layout(location = 0) rayPayloadEXT vec3 hitValue;\n"
			"layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
			"\n"
			"void main()\n"
			"{\n"
			"  uint  n        = gl_LaunchIDEXT.x + gl_LaunchSizeEXT.x * (gl_LaunchIDEXT.y + gl_LaunchSizeEXT.y * gl_LaunchIDEXT.z);\n"
			"  uint  f        = " + cullingFlagsInit + ";\n"
			+ raygenFlagsFragment +
			"  uint  rayFlags = f;\n"
			"  uint  cullMask = 0xFF;\n"
			"  float tmin     = 0.0;\n"
			"  float tmax     = 9.0;\n"
			"  vec3  origin   = vec3((float(gl_LaunchIDEXT.x) + 0.5f) / float(gl_LaunchSizeEXT.x), (float(gl_LaunchIDEXT.y) + 0.5f) / float(gl_LaunchSizeEXT.y), 0.0);\n"
			"  vec3  direct   = vec3(0.0, 0.0, -1.0);\n"
			"  traceRayEXT(topLevelAS, rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
			"}\n";

		switch (m_data.stage)
		{
			case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(raygenShader)) << buildOptions;

				{
					std::stringstream css;
					css <<
						"#version 460 core\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"hitAttributeEXT vec3 hitAttribute;\n"
						"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< updateImage <<
						"}\n";

					programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(raygenShader)) << buildOptions;

				{
					std::stringstream css;
					css <<
						"#version 460 core\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"hitAttributeEXT vec3 attribs;\n"
						"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
						"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< updateImage <<
						"}\n";

					programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

				if (m_data.geomType == GEOM_TYPE_AABBS)
					programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(intersectionShader)) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(raygenShader)) << buildOptions;

				{
					std::stringstream css;
					css <<
						"#version 460 core\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
						"hitAttributeEXT vec3 attribs;\n"
						"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< updateImage <<
						"}\n";

					programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

				if (m_data.geomType == GEOM_TYPE_AABBS)
					programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(intersectionShader)) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_MISS_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(raygenShader)) << buildOptions;

				{
					std::stringstream css;
					css <<
						"#version 460 core\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< updateImage <<
						"}\n";

					programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;

				if (m_data.geomType == GEOM_TYPE_AABBS)
					programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(intersectionShader)) << buildOptions;

				break;
			}

			default:
				TCU_THROW(InternalError, "Unknown stage");
		}
	}
	else if (m_data.id == TEST_ID_HIT_T_EXT		||
			 m_data.id == TEST_ID_RAY_T_MIN_EXT	||
			 m_data.id == TEST_ID_RAY_T_MAX_EXT	)
	{
		const std::string	raygenShader		=
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadEXT vec3 hitValue;\n"
			"layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
			"\n"
			"void main()\n"
			"{\n"
			"  uint  cullMask = 0xFF;\n"
			"  float a      = float(gl_LaunchIDEXT.x) / gl_LaunchSizeEXT.x;\n"
			"  float b      = 1.0f + float(gl_LaunchIDEXT.y) / gl_LaunchSizeEXT.y;\n"
			"  float c      = 0.25f * a / b;\n"
			"  float tmin   = c;\n"
			"  float tmax   = 0.75f + c;\n"
			"  vec3  origin = vec3((float(gl_LaunchIDEXT.x) + 0.5f) / float(gl_LaunchSizeEXT.x), (float(gl_LaunchIDEXT.y) + 0.5f) / float(gl_LaunchSizeEXT.y), 0.0);\n"
			"  vec3  direct = vec3(0.0, 0.0, -1.0);\n"
			"  traceRayEXT(topLevelAS, gl_RayFlagsNoneEXT, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
			"}\n";
		const std::string	intersectionShader	=
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"hitAttributeEXT vec3 hitAttribute;\n"
			"\n"
			"void main()\n"
			"{\n"
			"  float a = float(gl_LaunchIDEXT.x) / gl_LaunchSizeEXT.x;\n"
			"  float b = 1.0f + float(gl_LaunchIDEXT.y) / gl_LaunchSizeEXT.y;\n"
			"  float c = 0.25f * a / b;\n"
			"  reportIntersectionEXT(0.03125f + c, 0);\n"
			"}\n";
		const std::string	updateImage			=
			"  ivec3 p = ivec3(gl_LaunchIDEXT);\n"
			"  int   r = int(" +de::toString(FIXED_POINT_DIVISOR) + ".0f * gl_" + std::string(m_data.name) + ");\n"
			"  ivec4 c = ivec4(r,0,0,1);\n"
			"  imageStore(result, p, c);\n";

		switch (m_data.stage)
		{
			case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(raygenShader)) << buildOptions;

				{
					std::stringstream css;
					css <<
						"#version 460 core\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"hitAttributeEXT vec3 attribs;\n"
						"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
						"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< updateImage <<
						"}\n";

					programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

				if (m_data.geomType == GEOM_TYPE_AABBS)
					programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(intersectionShader)) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(raygenShader)) << buildOptions;

				{
					std::stringstream css;
					css <<
						"#version 460 core\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
						"hitAttributeEXT vec3 attribs;\n"
						"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< updateImage <<
						"}\n";

					programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

				if (m_data.geomType == GEOM_TYPE_AABBS)
					programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(intersectionShader)) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(raygenShader)) << buildOptions;

				{
					std::stringstream css;
					css <<
						"#version 460 core\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"hitAttributeEXT vec3 hitAttribute;\n"
						"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< updateImage <<
						"\n"
						"  float a = float(gl_LaunchIDEXT.x) / gl_LaunchSizeEXT.x;\n"
						"  float b = 1.0f + float(gl_LaunchIDEXT.y) / gl_LaunchSizeEXT.y;\n"
						"  reportIntersectionEXT(0.4375f + 0.25f * a / b, 0x7Eu);\n"
						"}\n";

					programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_MISS_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(raygenShader)) << buildOptions;

				{
					std::stringstream css;
					css <<
						"#version 460 core\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< updateImage <<
						"}\n";

					programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;

				if (m_data.geomType == GEOM_TYPE_AABBS)
					programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(intersectionShader)) << buildOptions;

				break;
			}

			default:
				TCU_THROW(InternalError, "Unknown stage");
		}
	}
	else if (m_data.id == TEST_ID_WORLD_RAY_ORIGIN_EXT		||
			 m_data.id == TEST_ID_WORLD_RAY_DIRECTION_EXT	||
			 m_data.id == TEST_ID_OBJECT_RAY_ORIGIN_EXT		||
			 m_data.id == TEST_ID_OBJECT_RAY_DIRECTION_EXT	||
			 m_data.id == TEST_ID_OBJECT_TO_WORLD_EXT		||
			 m_data.id == TEST_ID_WORLD_TO_OBJECT_EXT		||
			 m_data.id == TEST_ID_OBJECT_TO_WORLD_3X4_EXT	||
			 m_data.id == TEST_ID_WORLD_TO_OBJECT_3X4_EXT	)
	{
		const bool			matrix4x3			= (m_data.id == TEST_ID_OBJECT_TO_WORLD_EXT || m_data.id == TEST_ID_WORLD_TO_OBJECT_EXT);
		const bool			matrix3x4			= (m_data.id == TEST_ID_OBJECT_TO_WORLD_3X4_EXT || m_data.id == TEST_ID_WORLD_TO_OBJECT_3X4_EXT);
		const bool			matrixOutput		= matrix4x3 || matrix3x4;
		const std::string	vectorLoop			=
			"  for (int ndx = 0; ndx < 3; ndx++)\n"
			"  {\n";
		const std::string	matrixLoop4x3		=
			"  int ndx = -1;\n"
			"  for (int row = 0; row < 3; row++)\n"
			"  for (int col = 0; col < 4; col++)\n"
			"  {\n"
			"    ndx++;\n";
		const std::string	matrixLoop3x4		=
			"  int ndx = -1;\n"
			"  for (int col = 0; col < 3; col++)\n"
			"  for (int row = 0; row < 4; row++)\n"
			"  {\n"
			"    ndx++;\n";
		const std::string	loop				=
			matrix4x3 ? matrixLoop4x3 :
			matrix3x4 ? matrixLoop3x4 :
			vectorLoop;
		const std::string	index				=
			(matrixOutput ? "[col][row]" : "[ndx]");
		const std::string	updateImage			=
			"  float k = " +de::toString(FIXED_POINT_DIVISOR) + ".0f;\n"
			+ loop +
			"    ivec3 p = ivec3(gl_LaunchIDEXT.xy, ndx);\n"
			"    float r = k * gl_" + std::string(m_data.name) + index + ";\n"
			"    ivec4 c = ivec4(int(r),0,0,1);\n"
			"    imageStore(result, p, c);\n"
			"  }\n";

		switch (m_data.stage)
		{
			case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

				{
					std::stringstream css;
					css <<
						"#version 460 core\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"hitAttributeEXT vec3 attribs;\n"
						"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
						"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< updateImage <<
						"}\n";

					programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

				if (m_data.geomType == GEOM_TYPE_AABBS)
					programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(getIntersectionPassthrough())) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

				{
					std::stringstream css;
					css <<
						"#version 460 core\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
						"hitAttributeEXT vec3 attribs;\n"
						"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< updateImage <<
						"}\n";

					programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

				if (m_data.geomType == GEOM_TYPE_AABBS)
					programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(getIntersectionPassthrough())) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

				{
					std::stringstream css;
					css <<
						"#version 460 core\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"hitAttributeEXT vec3 hitAttribute;\n"
						"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< updateImage <<
						"  reportIntersectionEXT(0.95f, 0);\n"
						"}\n";

					programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_MISS_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

				{
					std::stringstream css;
					css <<
						"#version 460 core\n"
						"#extension GL_EXT_ray_tracing : require\n"
						"layout(set = 0, binding = 0, r32i) uniform iimage3D result;\n"
						"\n"
						"void main()\n"
						"{\n"
						<< updateImage <<
						"}\n";

					programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
				}

				programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
				programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;

				if (m_data.geomType == GEOM_TYPE_AABBS)
					programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(getIntersectionPassthrough())) << buildOptions;

				break;
			}

			default:
				TCU_THROW(InternalError, "Unknown stage");
		}
	}
	else
	{
		TCU_THROW(InternalError, "Not implemented");
	}
}

TestInstance* RayTracingTestCase::createInstance (Context& context) const
{
	return new RayTracingBuiltinLaunchTestInstance(context, m_data);
}

de::MovePtr<TopLevelAccelerationStructure> RayTracingBuiltinLaunchTestInstance::initTopAccelerationStructure (VkCommandBuffer											cmdBuffer,
																											  vector<de::SharedPtr<BottomLevelAccelerationStructure> >&	bottomLevelAccelerationStructures)
{
	const DeviceInterface&						vkd				= m_context.getDeviceInterface();
	const VkDevice								device			= m_context.getDevice();
	Allocator&									allocator		= m_context.getDefaultAllocator();
	de::MovePtr<TopLevelAccelerationStructure>	result			= makeTopLevelAccelerationStructure();
	const bool									transformTest	=  m_data.id == TEST_ID_WORLD_RAY_ORIGIN_EXT
																|| m_data.id == TEST_ID_WORLD_RAY_DIRECTION_EXT
																|| m_data.id == TEST_ID_OBJECT_RAY_ORIGIN_EXT
																|| m_data.id == TEST_ID_OBJECT_RAY_DIRECTION_EXT
																|| m_data.id == TEST_ID_OBJECT_TO_WORLD_EXT
																|| m_data.id == TEST_ID_WORLD_TO_OBJECT_EXT
																|| m_data.id == TEST_ID_OBJECT_TO_WORLD_3X4_EXT
																|| m_data.id == TEST_ID_WORLD_TO_OBJECT_3X4_EXT;

	result->setInstanceCount(bottomLevelAccelerationStructures.size());

	for (size_t structNdx = 0; structNdx < bottomLevelAccelerationStructures.size(); ++structNdx)
	{
		VkTransformMatrixKHR	transform	= identityMatrix3x4;

		if (transformTest)
		{
			if (structNdx & 1)
				transform.matrix[0][3] = (1.0f /  8.0f) / float(m_data.width);

			if (structNdx & 2)
				transform.matrix[1][3] = (1.0f / 16.0f) / float(m_data.height);
		}

		result->addInstance(bottomLevelAccelerationStructures[structNdx], transform, deUint32(2 * structNdx));
	}

	result->createAndBuild(vkd, device, cmdBuffer, allocator);

	return result;
}

de::MovePtr<BottomLevelAccelerationStructure> RayTracingBuiltinLaunchTestInstance::initBottomAccelerationStructure (VkCommandBuffer	cmdBuffer,
																													tcu::UVec2&		startPos)
{
	const DeviceInterface&							vkd			= m_context.getDeviceInterface();
	const VkDevice									device		= m_context.getDevice();
	Allocator&										allocator	= m_context.getDefaultAllocator();
	de::MovePtr<BottomLevelAccelerationStructure>	result		= makeBottomLevelAccelerationStructure();

	result->setGeometryCount(m_data.geometriesGroupCount);

	if (m_data.id == TEST_ID_LAUNCH_ID_EXT || m_data.id == TEST_ID_LAUNCH_SIZE_EXT)
	{
		result->setDefaultGeometryData(m_data.stage);
	}
	else if (m_data.id == TEST_ID_GEOMETRY_INDEX_EXT		||
			 m_data.id == TEST_ID_PRIMITIVE_ID				||
			 m_data.id == TEST_ID_INSTANCE_ID				||
			 m_data.id == TEST_ID_INSTANCE_CUSTOM_INDEX_EXT	)
	{
		const bool	triangles	= (m_data.geomType == GEOM_TYPE_TRIANGLES);
		const bool	missShader	= (m_data.stage == VK_SHADER_STAGE_MISS_BIT_KHR);
		const float	z			= !missShader ? -1.0f : -100.0f;

		DE_ASSERT(m_data.squaresGroupCount != 1);

		for (size_t geometryNdx = 0; geometryNdx < m_data.geometriesGroupCount; ++geometryNdx)
		{
			std::vector<tcu::Vec3>	geometryData;

			geometryData.reserve(m_data.squaresGroupCount * (triangles ? 3u : 2u));

			for (size_t squareNdx = 0; squareNdx < m_data.squaresGroupCount; ++squareNdx)
			{
				const deUint32	n	= m_data.width * startPos.y() + startPos.x();
				const float		x0	= float(startPos.x() + 0) / float(m_data.width);
				const float		y0	= float(startPos.y() + 0) / float(m_data.height);
				const float		x1	= float(startPos.x() + 1) / float(m_data.width);
				const float		y1	= float(startPos.y() + 1) / float(m_data.height);
				const deUint32	m	= n + 1;

				if (triangles)
				{
					const float	xm	= (x0 + x1) / 2.0f;
					const float	ym	= (y0 + y1) / 2.0f;

					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(xm, y1, z));
					geometryData.push_back(tcu::Vec3(x1, ym, z));
				}
				else
				{
					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(x1, y1, z * 0.9f));
				}

				startPos.y() = m / m_data.width;
				startPos.x() = m % m_data.width;
			}

			result->addGeometry(geometryData, triangles);
		}
	}
	else if (m_data.id == TEST_ID_HIT_KIND_EXT)
	{
		const bool	triangles	= (m_data.geomType == GEOM_TYPE_TRIANGLES);
		const bool	missShader	= (m_data.stage == VK_SHADER_STAGE_MISS_BIT_KHR);
		const float	z			= !missShader ? -1.0f : -100.0f;

		DE_ASSERT(m_data.squaresGroupCount != 1);
		DE_ASSERT(m_data.geometriesGroupCount == 4);

		std::vector<tcu::Vec3>	geometryDataOpaque;
		std::vector<tcu::Vec3>	geometryDataNonOpaque;

		geometryDataOpaque.reserve(m_data.squaresGroupCount * (triangles ? 3u : 2u));
		geometryDataNonOpaque.reserve(m_data.squaresGroupCount * (triangles ? 3u : 2u));

		for (size_t geometryNdx = 0; geometryNdx < m_data.geometriesGroupCount; ++geometryNdx)
		{
			const bool				cw				= ((geometryNdx & 1) == 0) ? true : false;
			std::vector<tcu::Vec3>&	geometryData	= ((geometryNdx & 2) == 0) ? geometryDataOpaque : geometryDataNonOpaque;

			for (size_t squareNdx = 0; squareNdx < m_data.squaresGroupCount; ++squareNdx)
			{
				const deUint32	n	= m_data.width * startPos.y() + startPos.x();
				const deUint32	m	= n + 1;
				const float		x0	= float(startPos.x() + 0) / float(m_data.width);
				const float		y0	= float(startPos.y() + 0) / float(m_data.height);
				const float		x1	= float(startPos.x() + 1) / float(m_data.width);
				const float		y1	= float(startPos.y() + 1) / float(m_data.height);

				if (triangles)
				{
					const float	xm	= (x0 + x1) / 2.0f;
					const float	ym	= (y0 + y1) / 2.0f;

					if (cw)
					{
						geometryData.push_back(tcu::Vec3(x0, y0, z));
						geometryData.push_back(tcu::Vec3(x1, ym, z));
						geometryData.push_back(tcu::Vec3(xm, y1, z));
					}
					else
					{
						geometryData.push_back(tcu::Vec3(x0, y0, z));
						geometryData.push_back(tcu::Vec3(xm, y1, z));
						geometryData.push_back(tcu::Vec3(x1, ym, z));
					}
				}
				else
				{
					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(x1, y1, z * 0.9f));
				}

				startPos.y() = m / m_data.width;
				startPos.x() = m % m_data.width;
			}
		}

		DE_ASSERT(startPos.y() == m_data.height && startPos.x() == 0);

		result->addGeometry(geometryDataOpaque, triangles, (VkGeometryFlagsKHR)VK_GEOMETRY_OPAQUE_BIT_KHR);
		result->addGeometry(geometryDataNonOpaque, triangles, (VkGeometryFlagsKHR)0);
	}
	else if (m_data.id == TEST_ID_INCOMING_RAY_FLAGS_EXT)
	{
		const bool					triangles		= (m_data.geomType == GEOM_TYPE_TRIANGLES);
		const bool					missShader		= (m_data.stage == VK_SHADER_STAGE_MISS_BIT_KHR);
		const float					z				= !missShader ? -1.0f : -100.0f;
		const VkGeometryFlagsKHR	geometryFlags	= m_data.opaque ? static_cast<VkGeometryFlagsKHR>(VK_GEOMETRY_OPAQUE_BIT_KHR) : static_cast<VkGeometryFlagsKHR>(0);
		const bool					cw				= m_data.frontFace;
		std::vector<tcu::Vec3>		geometryData;

		DE_ASSERT(m_data.geometriesGroupCount == 1);
		DE_ASSERT(m_data.squaresGroupCount != 1);

		geometryData.reserve(m_data.squaresGroupCount * (triangles ? 3u : 2u));

		for (size_t squareNdx = 0; squareNdx < m_data.squaresGroupCount; ++squareNdx)
		{
			const deUint32	n	= m_data.width * startPos.y() + startPos.x();
			const deUint32	m	= n + 1;
			const float		x0	= float(startPos.x() + 0) / float(m_data.width);
			const float		y0	= float(startPos.y() + 0) / float(m_data.height);
			const float		x1	= float(startPos.x() + 1) / float(m_data.width);
			const float		y1	= float(startPos.y() + 1) / float(m_data.height);

			if (triangles)
			{
				const float	xm	= (x0 + x1) / 2.0f;
				const float	ym	= (y0 + y1) / 2.0f;

				if (cw)
				{
					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(x1, ym, z));
					geometryData.push_back(tcu::Vec3(xm, y1, z));
				}
				else
				{
					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(xm, y1, z));
					geometryData.push_back(tcu::Vec3(x1, ym, z));
				}
			}
			else
			{
				geometryData.push_back(tcu::Vec3(x0, y0, z));
				geometryData.push_back(tcu::Vec3(x1, y1, z * 0.9f));
			}

			startPos.y() = m / m_data.width;
			startPos.x() = m % m_data.width;
		}

		DE_ASSERT(startPos.y() == m_data.height && startPos.x() == 0);

		result->addGeometry(geometryData, triangles, geometryFlags);
	}
	else if (m_data.id == TEST_ID_HIT_T_EXT		||
			 m_data.id == TEST_ID_RAY_T_MIN_EXT	||
			 m_data.id == TEST_ID_RAY_T_MAX_EXT	)
	{
		const bool	triangles	= (m_data.geomType == GEOM_TYPE_TRIANGLES);
		const bool	missShader	= (m_data.stage == VK_SHADER_STAGE_MISS_BIT_KHR);
		const bool	sectShader	= (m_data.stage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR);
		const bool	maxTTest	= (m_data.id == TEST_ID_RAY_T_MAX_EXT);

		DE_ASSERT(m_data.squaresGroupCount != 1);

		for (size_t geometryNdx = 0; geometryNdx < m_data.geometriesGroupCount; ++geometryNdx)
		{
			std::vector<tcu::Vec3>	geometryData;

			geometryData.reserve(m_data.squaresGroupCount * (triangles ? 3u : 2u));

			for (size_t squareNdx = 0; squareNdx < m_data.squaresGroupCount; ++squareNdx)
			{
				const deUint32	n			= m_data.width * startPos.y() + startPos.x();
				const deUint32	m			= n + 1;
				const bool		shiftRight	= sectShader && maxTTest && (0 == (startPos.y() & 1)) && (0 == (startPos.x() & 1));
				const deUint32	xo			= shiftRight ? 1 : 0;
				const float		x0			= float(startPos.x() + 0 + xo) / float(m_data.width);
				const float		y0			= float(startPos.y() + 0) / float(m_data.height);
				const float		x1			= float(startPos.x() + 1 + xo) / float(m_data.width);
				const float		y1			= float(startPos.y() + 1) / float(m_data.height);
				const float		a			= x0;
				const float		b			= 1.0f + y0;
				const float		c			= 0.03125f + 0.25f * a / b;
				const float		z			= !missShader ? -c : -100.0f;

				if (triangles)
				{
					const float	xm	= (x0 + x1) / 2.0f;
					const float	ym	= (y0 + y1) / 2.0f;

					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(xm, y1, z));
					geometryData.push_back(tcu::Vec3(x1, ym, z));
				}
				else
				{
					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(x1, y1, z * 0.9f));
				}

				startPos.y() = m / m_data.width;
				startPos.x() = m % m_data.width;
			}

			result->addGeometry(geometryData, triangles);
		}
	}
	else if (m_data.id == TEST_ID_WORLD_RAY_ORIGIN_EXT		||
			 m_data.id == TEST_ID_WORLD_RAY_DIRECTION_EXT	||
			 m_data.id == TEST_ID_OBJECT_RAY_ORIGIN_EXT		||
			 m_data.id == TEST_ID_OBJECT_RAY_DIRECTION_EXT	||
			 m_data.id == TEST_ID_OBJECT_TO_WORLD_EXT		||
			 m_data.id == TEST_ID_WORLD_TO_OBJECT_EXT		||
			 m_data.id == TEST_ID_OBJECT_TO_WORLD_3X4_EXT	||
			 m_data.id == TEST_ID_WORLD_TO_OBJECT_3X4_EXT	)
	{
		const bool				triangles		= m_data.geomType == GEOM_TYPE_TRIANGLES;
		const float				y0				= float(startPos.y() + 0) / float(m_data.height);
		const float				y1				= float(startPos.y() + 1) / float(m_data.height);
		const bool				missShader		= (m_data.stage == VK_SHADER_STAGE_MISS_BIT_KHR);
		const float				z				= !missShader ? -1.0f : -100.0f;
		std::vector<tcu::Vec3>	geometryData;

		if (triangles)
		{
			geometryData.push_back(tcu::Vec3(-1.0f, y1, z));
			geometryData.push_back(tcu::Vec3(-1.0f, y0, z));
			geometryData.push_back(tcu::Vec3(+1.0f, y0, z));
			geometryData.push_back(tcu::Vec3(-1.0f, y1, z));
			geometryData.push_back(tcu::Vec3(+1.0f, y0, z));
			geometryData.push_back(tcu::Vec3(+1.0f, y1, z));
		}
		else
		{
			geometryData.reserve(2);

			geometryData.push_back(tcu::Vec3(-1.0f, y0, z));
			geometryData.push_back(tcu::Vec3(+1.0f, y1, z));
		}

		DE_ASSERT(startPos.y() < m_data.height);

		startPos.y()++;

		result->addGeometry(geometryData, triangles);
	}
	else
	{
		TCU_THROW(InternalError, "Not implemented");
	}

	result->createAndBuild(vkd, device, cmdBuffer, allocator);

	return result;
}

vector<de::SharedPtr<BottomLevelAccelerationStructure> > RayTracingBuiltinLaunchTestInstance::initBottomAccelerationStructures (VkCommandBuffer	cmdBuffer)
{
	tcu::UVec2													startPos;
	vector<de::SharedPtr<BottomLevelAccelerationStructure> >	result;

	for (size_t instanceNdx = 0; instanceNdx < m_data.instancesGroupCount; ++instanceNdx)
	{
		de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure	= initBottomAccelerationStructure(cmdBuffer, startPos);

		result.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));
	}

	return result;
}

Move<VkPipeline> RayTracingBuiltinLaunchTestInstance::makePipeline (de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
																	VkPipelineLayout					pipelineLayout,
																	const VkSpecializationInfo*			specializationInfo)
{
	const DeviceInterface&	vkd			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();
	vk::BinaryCollection&	collection	= m_context.getBinaryCollection();

	if (0 != (m_shaders & VK_SHADER_STAGE_RAYGEN_BIT_KHR))			rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR		, createShaderModule(vkd, device, collection.get("rgen"), 0), m_raygenShaderGroup, specializationInfo);
	if (0 != (m_shaders & VK_SHADER_STAGE_ANY_HIT_BIT_KHR))			rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR		, createShaderModule(vkd, device, collection.get("ahit"), 0), m_hitShaderGroup, specializationInfo);
	if (0 != (m_shaders & VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR))		rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR	, createShaderModule(vkd, device, collection.get("chit"), 0), m_hitShaderGroup, specializationInfo);
	if (0 != (m_shaders & VK_SHADER_STAGE_MISS_BIT_KHR))			rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR			, createShaderModule(vkd, device, collection.get("miss"), 0), m_missShaderGroup, specializationInfo);
	if (0 != (m_shaders & VK_SHADER_STAGE_INTERSECTION_BIT_KHR))	rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR	, createShaderModule(vkd, device, collection.get("sect"), 0), m_hitShaderGroup, specializationInfo);
	if (0 != (m_shaders & VK_SHADER_STAGE_CALLABLE_BIT_KHR))		rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR		, createShaderModule(vkd, device, collection.get("call"), 0), m_callableShaderGroup, specializationInfo);

	if (m_data.pipelineCreateFlags != 0)
		rayTracingPipeline->setCreateFlags(m_data.pipelineCreateFlags);

	Move<VkPipeline> pipeline = rayTracingPipeline->createPipeline(vkd, device, pipelineLayout);

	return pipeline;
}

de::MovePtr<BufferWithMemory> RayTracingBuiltinLaunchTestInstance::createShaderBindingTable (const InstanceInterface&			vki,
																							 const DeviceInterface&				vkd,
																							 const VkDevice						device,
																							 const VkPhysicalDevice				physicalDevice,
																							 const VkPipeline					pipeline,
																							 Allocator&							allocator,
																							 de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
																							 const deUint32						group)
{
	de::MovePtr<BufferWithMemory>	shaderBindingTable;

	if (group < m_shaderGroupCount)
	{
		const deUint32	shaderGroupHandleSize		= getShaderGroupSize(vki, physicalDevice);
		const deUint32	shaderGroupBaseAlignment	= getShaderGroupBaseAlignment(vki, physicalDevice);

		shaderBindingTable = rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, group, 1u);
	}

	return shaderBindingTable;
}

// Provides two spec constants, one integer and one float, both with value 256.
class SpecConstantsHelper
{
public:
								SpecConstantsHelper		();
	const VkSpecializationInfo&	getSpecializationInfo	(void) const;
private:
	std::vector<deUint8>					m_data;
	std::vector<VkSpecializationMapEntry>	m_mapEntries;
	VkSpecializationInfo					m_specInfo;
};

SpecConstantsHelper::SpecConstantsHelper ()
	: m_data		()
	, m_mapEntries	()
{
	// To make things interesting, make both data unaligned and add some padding.
	const deInt32	value1	= 256;
	const float		value2	= 256.0f;

	const size_t	offset1	= 1u;							// Offset of 1 byte.
	const size_t	offset2	= 1u + sizeof(value1) + 2u;		// Offset of 3 bytes plus the size of value1.

	m_data.resize(sizeof(value1) + sizeof(value2) + 5u);	// Some extra padding at the end too.
	deMemcpy(&m_data[offset1], &value1, sizeof(value1));
	deMemcpy(&m_data[offset2], &value2, sizeof(value2));

	// Map entries.
	m_mapEntries.reserve(2u);
	m_mapEntries.push_back({ 0u, static_cast<deUint32>(offset1), static_cast<deUintptr>(sizeof(value1)) });
	m_mapEntries.push_back({ 1u, static_cast<deUint32>(offset2), static_cast<deUintptr>(sizeof(value2))	});

	// Specialization info.
	m_specInfo.mapEntryCount	= static_cast<deUint32>(m_mapEntries.size());
	m_specInfo.pMapEntries		= m_mapEntries.data();
	m_specInfo.dataSize			= static_cast<deUintptr>(m_data.size());
	m_specInfo.pData			= m_data.data();
}

const VkSpecializationInfo& SpecConstantsHelper::getSpecializationInfo (void) const
{
	return m_specInfo;
}

de::MovePtr<BufferWithMemory> RayTracingBuiltinLaunchTestInstance::runTest (void)
{
	const InstanceInterface&			vki									= m_context.getInstanceInterface();
	const DeviceInterface&				vkd									= m_context.getDeviceInterface();
	const VkDevice						device								= m_context.getDevice();
	const VkPhysicalDevice				physicalDevice						= m_context.getPhysicalDevice();
	const deUint32						queueFamilyIndex					= m_context.getUniversalQueueFamilyIndex();
	const VkQueue						queue								= m_context.getUniversalQueue();
	Allocator&							allocator							= m_context.getDefaultAllocator();
	const deUint32						shaderGroupHandleSize				= getShaderGroupSize(vki, physicalDevice);
	const VkFormat						format								= m_data.format;
	const deUint32						pixelSize							= tcu::getPixelSize(mapVkFormat(format));
	const deUint32						pixelCount							= m_data.width * m_data.height * m_data.depth;

	const Move<VkDescriptorSetLayout>	descriptorSetLayout					= DescriptorSetLayoutBuilder()
																					.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, ALL_RAY_TRACING_STAGES)
																					.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, ALL_RAY_TRACING_STAGES)
																					.build(vkd, device);
	const Move<VkDescriptorPool>		descriptorPool						= DescriptorPoolBuilder()
																					.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
																					.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
																					.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const Move<VkDescriptorSet>			descriptorSet						= makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout);
	const Move<VkPipelineLayout>		pipelineLayout						= makePipelineLayout(vkd, device, descriptorSetLayout.get());
	const Move<VkCommandPool>			cmdPool								= createCommandPool(vkd, device, 0, queueFamilyIndex);
	const Move<VkCommandBuffer>			cmdBuffer							= allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	de::MovePtr<RayTracingPipeline>		rayTracingPipeline					= de::newMovePtr<RayTracingPipeline>();
	const SpecConstantsHelper			specConstantHelper;
	const VkSpecializationInfo*			specializationInfo					= (m_data.useSpecConstants ? &specConstantHelper.getSpecializationInfo() : nullptr);
	const Move<VkPipeline>				pipeline							= makePipeline(rayTracingPipeline, *pipelineLayout, specializationInfo);
	const de::MovePtr<BufferWithMemory>	raygenShaderBindingTable			= createShaderBindingTable(vki, vkd, device, physicalDevice, *pipeline, allocator, rayTracingPipeline, m_raygenShaderGroup);
	const de::MovePtr<BufferWithMemory>	missShaderBindingTable				= createShaderBindingTable(vki, vkd, device, physicalDevice, *pipeline, allocator, rayTracingPipeline, m_missShaderGroup);
	const de::MovePtr<BufferWithMemory>	hitShaderBindingTable				= createShaderBindingTable(vki, vkd, device, physicalDevice, *pipeline, allocator, rayTracingPipeline, m_hitShaderGroup);
	const de::MovePtr<BufferWithMemory>	callableShaderBindingTable			= createShaderBindingTable(vki, vkd, device, physicalDevice, *pipeline, allocator, rayTracingPipeline, m_callableShaderGroup);

	const VkStridedDeviceAddressRegionKHR	raygenShaderBindingTableRegion		= raygenShaderBindingTable.get() != NULL ? makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize) : makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	const VkStridedDeviceAddressRegionKHR	missShaderBindingTableRegion		= missShaderBindingTable.get() != NULL ? makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize) : makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	const VkStridedDeviceAddressRegionKHR	hitShaderBindingTableRegion			= hitShaderBindingTable.get() != NULL ? makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize) : makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	const VkStridedDeviceAddressRegionKHR	callableShaderBindingTableRegion	= callableShaderBindingTable.get() != NULL ? makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, callableShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize) : makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

	const VkImageCreateInfo				imageCreateInfo						= makeImageCreateInfo(m_data.width, m_data.height, m_data.depth, format);
	const VkImageSubresourceRange		imageSubresourceRange				= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0, 1u);
	const de::MovePtr<ImageWithMemory>	image								= de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
	const Move<VkImageView>				imageView							= makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_3D, format, imageSubresourceRange);

	const VkBufferCreateInfo			bufferCreateInfo					= makeBufferCreateInfo(pixelCount * pixelSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkImageSubresourceLayers		bufferImageSubresourceLayers		= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const VkBufferImageCopy				bufferImageRegion					= makeBufferImageCopy(makeExtent3D(m_data.width, m_data.height, m_data.depth), bufferImageSubresourceLayers);
	de::MovePtr<BufferWithMemory>		buffer								= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));

	const VkDescriptorImageInfo			descriptorImageInfo					= makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);

	const VkImageMemoryBarrier			preImageBarrier						= makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT,
																				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
																				**image, imageSubresourceRange);
	const VkImageMemoryBarrier			postImageBarrier					= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
																				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
																				**image, imageSubresourceRange);
	const VkMemoryBarrier				postTraceMemoryBarrier				= makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
	const VkMemoryBarrier				postCopyMemoryBarrier				= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	const VkClearValue					clearValue							= makeClearValueColorU32(DEFAULT_UINT_CLEAR_VALUE, DEFAULT_UINT_CLEAR_VALUE, DEFAULT_UINT_CLEAR_VALUE, 255u);

	vector<de::SharedPtr<BottomLevelAccelerationStructure> >	bottomLevelAccelerationStructures;
	de::MovePtr<TopLevelAccelerationStructure>					topLevelAccelerationStructure;

	beginCommandBuffer(vkd, *cmdBuffer, 0u);
	{
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preImageBarrier);
		vkd.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1, &imageSubresourceRange);
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, &postImageBarrier);

		bottomLevelAccelerationStructures	= initBottomAccelerationStructures(*cmdBuffer);
		topLevelAccelerationStructure		= initTopAccelerationStructure(*cmdBuffer, bottomLevelAccelerationStructures);

		const TopLevelAccelerationStructure*			topLevelAccelerationStructurePtr		= topLevelAccelerationStructure.get();
		VkWriteDescriptorSetAccelerationStructureKHR	accelerationStructureWriteDescriptorSet	=
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	//  VkStructureType						sType;
			DE_NULL,															//  const void*							pNext;
			1u,																	//  deUint32							accelerationStructureCount;
			topLevelAccelerationStructurePtr->getPtr(),							//  const VkAccelerationStructureKHR*	pAccelerationStructures;
		};

		DescriptorSetUpdateBuilder()
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfo)
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelerationStructureWriteDescriptorSet)
			.update(vkd, device);

		vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipelineLayout, 0, 1, &descriptorSet.get(), 0, DE_NULL);

		vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipeline);

		cmdTraceRays(vkd,
			*cmdBuffer,
			&raygenShaderBindingTableRegion,
			&missShaderBindingTableRegion,
			&hitShaderBindingTableRegion,
			&callableShaderBindingTableRegion,
			m_data.width, m_data.height, m_data.raysDepth);

		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);

		vkd.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **buffer, 1u, &bufferImageRegion);

		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &postCopyMemoryBarrier);
	}
	endCommandBuffer(vkd, *cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

	invalidateMappedMemoryRange(vkd, device, buffer->getAllocation().getMemory(), buffer->getAllocation().getOffset(), VK_WHOLE_SIZE);

	return buffer;
}

void checkFormatSupported(Context& context, VkFormat format, VkImageUsageFlags usage, const VkExtent3D& extent)
{
	VkResult					result;
	VkImageFormatProperties		properties;

	result = context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(context.getPhysicalDevice(), format, VK_IMAGE_TYPE_3D, VK_IMAGE_TILING_OPTIMAL, usage, 0, &properties);

	if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
	{
		std::ostringstream msg;

		msg << "Format " << format << " not supported for usage flags 0x" << std::hex << usage;

		TCU_THROW(NotSupportedError, msg.str());
	}

	if (properties.maxExtent.width < extent.width || properties.maxExtent.height < extent.height || properties.maxExtent.depth < extent.depth)
		TCU_THROW(NotSupportedError, "Image size is too large for this format");

	VK_CHECK(result);
}

void RayTracingBuiltinLaunchTestInstance::checkSupportInInstance (void) const
{
	const InstanceInterface&				vki						= m_context.getInstanceInterface();
	const VkPhysicalDevice					physicalDevice			= m_context.getPhysicalDevice();
	const vk::VkPhysicalDeviceProperties&	properties				= m_context.getDeviceProperties();
	const deUint32							requiredAllocations		= 8u
																	+ TopLevelAccelerationStructure::getRequiredAllocationCount()
																	+ m_data.instancesGroupCount * BottomLevelAccelerationStructure::getRequiredAllocationCount();
	const de::MovePtr<RayTracingProperties>	rayTracingProperties	= makeRayTracingProperties(vki, physicalDevice);
	const VkExtent3D						extent					= makeExtent3D(m_data.width, m_data.height, m_data.depth);

	checkFormatSupported(m_context, m_data.format, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, extent);

	if (rayTracingProperties->getMaxPrimitiveCount() < 2 * m_data.squaresGroupCount)
		TCU_THROW(NotSupportedError, "Triangles required more than supported");

	if (rayTracingProperties->getMaxGeometryCount() < m_data.geometriesGroupCount)
		TCU_THROW(NotSupportedError, "Geometries required more than supported");

	if (rayTracingProperties->getMaxInstanceCount() < m_data.instancesGroupCount)
		TCU_THROW(NotSupportedError, "Instances required more than supported");

	if (properties.limits.maxMemoryAllocationCount < requiredAllocations)
		TCU_THROW(NotSupportedError, "Test requires more allocations allowed");
}

std::vector<deInt32> RayTracingBuiltinLaunchTestInstance::expectedIntValuesBuffer (void)
{
	deUint32				pos		= 0;
	std::vector<deInt32>	result;

	result.reserve(m_data.depth * m_data.height * m_data.width);

	if (m_data.id == TEST_ID_LAUNCH_ID_EXT)
	{
		for (deUint32 z = 0; z < m_data.depth; ++z)
		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
			result.push_back(deInt32(x + 256 * (y + 256 * z)) + 1);
	}
	else if (m_data.id == TEST_ID_LAUNCH_SIZE_EXT)
	{
		const deUint32				expectedValue	= m_data.width + 256 * (m_data.height + 256 * m_data.depth);
		const std::vector<deInt32>	result2			(m_data.depth * m_data.height * m_data.width, deInt32(expectedValue) + 1);

		result = result2;
	}
	else if (m_data.id == TEST_ID_GEOMETRY_INDEX_EXT)
	{
		for (deUint32 z = 0; z < m_data.depth; ++z)
		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
			result.push_back(deInt32((pos++ / m_data.squaresGroupCount) % m_data.geometriesGroupCount));
	}
	else if (m_data.id == TEST_ID_PRIMITIVE_ID)
	{
		for (deUint32 z = 0; z < m_data.depth; ++z)
		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
			result.push_back(deInt32(pos++ % m_data.squaresGroupCount));
	}
	else if (m_data.id == TEST_ID_INSTANCE_ID)
	{
		for (deUint32 z = 0; z < m_data.depth; ++z)
		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
			result.push_back(deInt32(pos++ / (m_data.squaresGroupCount * m_data.geometriesGroupCount)));
	}
	else if (m_data.id == TEST_ID_INSTANCE_CUSTOM_INDEX_EXT)
	{
		for (deUint32 z = 0; z < m_data.depth; ++z)
		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
			result.push_back(deInt32(2 * (pos++ / (m_data.squaresGroupCount * m_data.geometriesGroupCount))));
	}
	else if (m_data.id == TEST_ID_INCOMING_RAY_FLAGS_EXT)
	{
		DE_ASSERT(m_data.squaresGroupCount == (1<<RAY_FLAG_BIT_LAST_PER_TEST));
		DE_ASSERT(DEFAULT_UINT_CLEAR_VALUE != (1<<RAY_FLAG_BIT_LAST_PER_TEST));

		for (deUint32 z = 0; z < m_data.depth; ++z)
		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
		{
			const deUint32	n						= x + m_data.width * (y + m_data.height * z);
			const bool		rayOpaque				= (0 != (n & (1<<RAY_FLAG_BIT_OPAQUE_EXT                     )));
			const bool		rayNoOpaque				= (0 != (n & (1<<RAY_FLAG_BIT_NO_OPAQUE_EXT                  ))) && !rayOpaque;
			const bool		rayTerminateOnFirstHit	= (0 != (n & (1<<RAY_FLAG_BIT_TERMINATE_ON_FIRST_HIT_EXT     )));
			const bool		raySkipClosestHitShader	= (0 != (n & (1<<RAY_FLAG_BIT_SKIP_CLOSEST_HIT_SHADER_EXT    )));
			const bool		rayCullBack				= (0 != (n & (1<<RAY_FLAG_BIT_CULL_BACK_FACING_TRIANGLES_EXT )));
			const bool		rayCullFront			= (0 != (n & (1<<RAY_FLAG_BIT_CULL_FRONT_FACING_TRIANGLES_EXT))) && !rayCullBack;
			const bool		rayCullOpaque			= (0 != (n & (1<<RAY_FLAG_BIT_CULL_OPAQUE_EXT                ))) && !rayOpaque && !rayNoOpaque;
			const bool		rayCullNoOpaque			= (0 != (n & (1<<RAY_FLAG_BIT_CULL_NO_OPAQUE_EXT             ))) && !rayOpaque && !rayNoOpaque && !rayCullOpaque;
			const bool		raySkipTriangles		= m_data.rayFlagSkipTriangles;
			const bool		raySkipAABBs			= m_data.rayFlagSkipAABSs;
			const bool		pipelineSkipTriangles	= (m_data.pipelineCreateFlags & VK_PIPELINE_CREATE_RAY_TRACING_SKIP_TRIANGLES_BIT_KHR) != 0;
			const bool		pipelineSkipAABBs		= (m_data.pipelineCreateFlags & VK_PIPELINE_CREATE_RAY_TRACING_SKIP_AABBS_BIT_KHR) != 0;
			const bool		cullingTest				= m_data.rayFlagSkipTriangles || m_data.rayFlagSkipAABSs || pipelineSkipTriangles || pipelineSkipAABBs;
			const bool		geometryFrontFace		= m_data.frontFace;
			const bool		geometryOpaque			= m_data.opaque;
			const bool		geometryTriangles		= (m_data.geomType == GEOM_TYPE_TRIANGLES) ? true : false;
			const bool		geometryAABBs			= (m_data.geomType == GEOM_TYPE_AABBS) ? true : false;
			deUint32		v						= 0
													| (rayOpaque                ? (1<<RAY_FLAG_BIT_OPAQUE_EXT                     ) : 0)
													| (rayNoOpaque              ? (1<<RAY_FLAG_BIT_NO_OPAQUE_EXT                  ) : 0)
													| (rayTerminateOnFirstHit   ? (1<<RAY_FLAG_BIT_TERMINATE_ON_FIRST_HIT_EXT     ) : 0)
													| (raySkipClosestHitShader  ? (1<<RAY_FLAG_BIT_SKIP_CLOSEST_HIT_SHADER_EXT    ) : 0)
													| (rayCullBack              ? (1<<RAY_FLAG_BIT_CULL_BACK_FACING_TRIANGLES_EXT ) : 0)
													| (rayCullFront             ? (1<<RAY_FLAG_BIT_CULL_FRONT_FACING_TRIANGLES_EXT) : 0)
													| (rayCullOpaque            ? (1<<RAY_FLAG_BIT_CULL_OPAQUE_EXT                ) : 0)
													| (rayCullNoOpaque          ? (1<<RAY_FLAG_BIT_CULL_NO_OPAQUE_EXT             ) : 0)
													| (raySkipTriangles         ? (1<<RAY_FLAG_BIT_SKIP_TRIANGLES_EXT             ) : 0)
													| (raySkipAABBs             ? (1<<RAY_FLAG_BIT_SKIP_AABB_EXT                  ) : 0);

			if (m_data.stage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR && raySkipClosestHitShader)
				v = DEFAULT_UINT_CLEAR_VALUE;

			if (m_data.stage == VK_SHADER_STAGE_ANY_HIT_BIT_KHR && (rayOpaque || (geometryOpaque && !rayOpaque && !rayNoOpaque)))
				v = DEFAULT_UINT_CLEAR_VALUE;

			if (geometryOpaque)
			{
				if (rayCullOpaque)
					if (m_data.stage != VK_SHADER_STAGE_MISS_BIT_KHR)
						v = DEFAULT_UINT_CLEAR_VALUE;
			}
			else
			{
				if (rayCullNoOpaque)
					if (m_data.stage != VK_SHADER_STAGE_MISS_BIT_KHR)
						v = DEFAULT_UINT_CLEAR_VALUE;
			}

			if (geometryTriangles)
			{
				if (geometryFrontFace)
				{
					if (rayCullFront)
						if (m_data.stage != VK_SHADER_STAGE_MISS_BIT_KHR)
							v = DEFAULT_UINT_CLEAR_VALUE;
				}
				else
				{
					if (rayCullBack)
						if (m_data.stage != VK_SHADER_STAGE_MISS_BIT_KHR)
							v = DEFAULT_UINT_CLEAR_VALUE;
				}
			}

			if (cullingTest)
			{
				if (m_data.stage != VK_SHADER_STAGE_MISS_BIT_KHR)
				{
					if (geometryTriangles)
					{
						if (raySkipTriangles || pipelineSkipTriangles)
							v = DEFAULT_UINT_CLEAR_VALUE;
					}

					if (geometryAABBs)
					{
						if (raySkipAABBs || pipelineSkipAABBs)
							v = DEFAULT_UINT_CLEAR_VALUE;
					}
				}
			}

			result.push_back(deInt32(v));
		}
	}
	else if (m_data.id == TEST_ID_HIT_KIND_EXT)
	{
		for (deUint32 z = 0; z < m_data.depth; ++z)
		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
		{
			const deUint32	n				= x + m_data.width * (y + m_data.height * z);
			const deUint32	geometryNdx		= n / m_data.squaresGroupCount;
			const deUint32	hitKind			= ((geometryNdx & 1) == 0) ? 0xFEu : 0xFFu;
			const bool		geometryOpaque	= ((geometryNdx & 2) == 0) ? true : false;
			deUint32		v				= (m_data.geomType == GEOM_TYPE_TRIANGLES) ? hitKind : 0x7Eu;

			if (m_data.stage == VK_SHADER_STAGE_ANY_HIT_BIT_KHR && geometryOpaque)
				v = DEFAULT_UINT_CLEAR_VALUE;

			result.push_back(deInt32(v));
		}
	}
	else
	{
		TCU_THROW(InternalError, "Not implemented");
	}

	return result;
}

std::vector<float> RayTracingBuiltinLaunchTestInstance::expectedFloatValuesBuffer (void)
{
	std::vector<float>	result;

	result.reserve(m_data.depth * m_data.height * m_data.width);

	if (m_data.id == TEST_ID_HIT_T_EXT)
	{
		for (deUint32 z = 0; z < m_data.depth; ++z)
		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
		{
			const float	a	= float(x) / float(m_data.width);
			const float	b	= 1.0f + float(y) / float(m_data.height);
			const float	f	= 0.03125f + 0.25f * a / b;

			result.push_back(f);
		}
	}
	else if (m_data.id == TEST_ID_RAY_T_MIN_EXT)
	{
		for (deUint32 z = 0; z < m_data.depth; ++z)
		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
		{
			const float	a	= float(x) / float(m_data.width);
			const float	b	= 1.0f + float(y) / float(m_data.height);
			const float	f	= 0.25f * a / b;

			result.push_back(f);
		}
	}
	else if (m_data.id == TEST_ID_RAY_T_MAX_EXT)
	{
		for (deUint32 z = 0; z < m_data.depth; ++z)
		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
		{
			const float	a				= float(x) / float(m_data.width);
			const float	b				= 1.0f + float(y) / float(m_data.height);
			const float	c				= 0.25f * a / b;
			// In a miss shader, the value is identical to the parameter passed into traceRayEXT().
			const float	m				= 0.75f + c;
			// In the closest-hit shader, the value reflects the closest distance to the intersected primitive.
			// In the any-hit shader, it reflects the distance to the primitive currently being intersected.
			// In the intersection shader, it reflects the distance to the closest primitive intersected so far.
			const float	n				= 0.03125f + c;
			const bool	normalRow		= (y & 1) != 0;
			const bool	doublePrimitive	= (x & 1) != 0;
			const float	s				= normalRow ? m
										: doublePrimitive ? 0.4375f + c
										: float(DEFAULT_UINT_CLEAR_VALUE) / float(FIXED_POINT_DIVISOR);
			const float	f				= (m_data.stage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR) ? s
										: (m_data.stage == VK_SHADER_STAGE_MISS_BIT_KHR) ? m
										: n;

			result.push_back(f);
		}
	}
	else
	{
		TCU_THROW(InternalError, "Not implemented");
	}

	return result;
}

std::vector<float> RayTracingBuiltinLaunchTestInstance::expectedVectorValuesBuffer (void)
{
	const deUint32		imageDepth		= 4;
	const deUint32		expectedFloats	= imageDepth * m_data.height * m_data.width;
	std::vector<float>	result			(expectedFloats, float(DEFAULT_UINT_CLEAR_VALUE) / float(FIXED_POINT_DIVISOR));

	if (m_data.id == TEST_ID_WORLD_RAY_ORIGIN_EXT)
	{
		deUint32	pos	= 0;

		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
			result[pos++] = (0.5f + float(x)) / float(m_data.width);

		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
			result[pos++] = (0.5f + float(y)) / float(m_data.height);

		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
			result[pos++] = 0.0f;
	}
	else if (m_data.id == TEST_ID_WORLD_RAY_DIRECTION_EXT)
	{
		deUint32	pos	= 0;

		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
			result[pos++] = 0.0f;

		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
			result[pos++] = 0.0f;

		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
			result[pos++] = -1.0f;
	}
	else if (m_data.id == TEST_ID_OBJECT_RAY_ORIGIN_EXT)
	{
		deUint32	pos	= 0;

		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
		{
			const deUint32	instanceId	= y / (m_data.height / m_data.instancesGroupCount);
			const float		offset		= (instanceId & 1) ? 1.0f / 8.0f : 0.0f;

			result[pos++] = (0.5f + float(x) - offset) / float(m_data.width);
		}

		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
		{
			const deUint32	instanceId	= y / (m_data.height / m_data.instancesGroupCount);
			const float		offset		= (instanceId & 2) ? 1.0f / 16.0f : 0.0f;

			result[pos++] = (0.5f + float(y) - offset) / float(m_data.height);
		}

		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
			result[pos++] = 0.0f;
	}
	else if (m_data.id == TEST_ID_OBJECT_RAY_DIRECTION_EXT)
	{
		deUint32	pos	= 0;

		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
			result[pos++] = 0.0f;

		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
			result[pos++] = 0.0f;

		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
			result[pos++] = -1.0f;
	}
	else
	{
		TCU_THROW(InternalError, "Not implemented");
	}

	return result;
}

std::vector<float> RayTracingBuiltinLaunchTestInstance::expectedMatrixValuesBuffer (void)
{
	const deUint32		colCount		= 4;
	const deUint32		rowCount		= 3;
	const deUint32		imageDepth		= 4 * 4;
	const deUint32		zStride			= m_data.height * m_data.width;
	const deUint32		expectedFloats	= imageDepth * m_data.height * m_data.width;
	std::vector<float>	result			(expectedFloats, float(DEFAULT_UINT_CLEAR_VALUE) / float(FIXED_POINT_DIVISOR));

	if (m_data.id == TEST_ID_OBJECT_TO_WORLD_EXT ||
		m_data.id == TEST_ID_WORLD_TO_OBJECT_EXT ||
		m_data.id == TEST_ID_OBJECT_TO_WORLD_3X4_EXT ||
		m_data.id == TEST_ID_WORLD_TO_OBJECT_3X4_EXT)
	{
		const int	translateColumnNumber	= 3;
		const float translateSign			= (m_data.id == TEST_ID_WORLD_TO_OBJECT_EXT || m_data.id == TEST_ID_WORLD_TO_OBJECT_3X4_EXT) ? -1.0f : +1.0f;
		const float translateX				= translateSign * (1.0f / 8.0f) / float(m_data.width);
		const float translateY				= translateSign * (1.0f / 16.0f) / float(m_data.height);

		for (deUint32 y = 0; y < m_data.height; ++y)
		{
			const deUint32	instanceId	= y / (m_data.height / m_data.instancesGroupCount);

			for (deUint32 x = 0; x < m_data.width; ++x)
			{
				tcu::Matrix<float, rowCount, colCount>	m;
				const deUint32							elem0Pos	= x + m_data.width * y;

				if (instanceId & 1)
					m[translateColumnNumber][0] = translateX;

				if (instanceId & 2)
					m[translateColumnNumber][1] = translateY;

				for (deUint32 rowNdx = 0; rowNdx < rowCount; ++rowNdx)
				for (deUint32 colNdx = 0; colNdx < colCount; ++colNdx)
				{
					const deUint32	z	= rowNdx * colCount + colNdx;
					const deUint32	pos	= elem0Pos + zStride * z;

					result[pos] = m[colNdx][rowNdx];
				}
			}
		}
	}
	else
	{
		TCU_THROW(InternalError, "Not implemented");
	}

	return result;
}

bool RayTracingBuiltinLaunchTestInstance::validateIntBuffer (de::MovePtr<BufferWithMemory> buffer)
{
	const deInt32*			bufferPtr		= (deInt32*)buffer->getAllocation().getHostPtr();
	const vector<deInt32>	expectedValues	= expectedIntValuesBuffer();
	tcu::TestLog&			log				= m_context.getTestContext().getLog();
	deUint32				failures		= 0;
	deUint32				pos				= 0;

	for (deUint32 z = 0; z < m_data.depth; ++z)
	for (deUint32 y = 0; y < m_data.height; ++y)
	for (deUint32 x = 0; x < m_data.width; ++x)
	{
		if (bufferPtr[pos] != expectedValues[pos])
			failures++;

		++pos;
	}

	if (failures != 0)
	{
		const char * names[] = { "Retrieved:", "Expected:" };
		for (deUint32 n = 0; n < 2; ++n)
		{
			const deInt32*		loggedData = (n == 0) ? bufferPtr : expectedValues.data();
			std::stringstream	css;

			pos = 0;

			for (deUint32 z = 0; z < m_data.depth; ++z)
			for (deUint32 y = 0; y < m_data.height; ++y)
			{
				for (deUint32 x = 0; x < m_data.width; ++x)
				{
					if (bufferPtr[pos] == expectedValues[pos])
						css << "____,";
					else
						css << std::hex << std::setw(4) << loggedData[pos] << ",";

					pos++;
				}

				css << std::endl;
			}

			log << tcu::TestLog::Message << names[n] << tcu::TestLog::EndMessage;
			log << tcu::TestLog::Message << css.str() << tcu::TestLog::EndMessage;
		}
	}

	return failures == 0;
}

bool RayTracingBuiltinLaunchTestInstance::validateFloatBuffer (de::MovePtr<BufferWithMemory> buffer)
{
	const float			eps				= float(FIXED_POINT_ALLOWED_ERROR) / float(FIXED_POINT_DIVISOR);
	const deInt32*		bufferPtr		= (deInt32*)buffer->getAllocation().getHostPtr();
	const vector<float>	expectedValues	= expectedFloatValuesBuffer();
	tcu::TestLog&		log				= m_context.getTestContext().getLog();
	deUint32			failures		= 0;
	deUint32			pos				= 0;

	for (deUint32 z = 0; z < m_data.depth; ++z)
	for (deUint32 y = 0; y < m_data.height; ++y)
	for (deUint32 x = 0; x < m_data.width; ++x)
	{
		const float retrievedValue	= float(bufferPtr[pos]) / float(FIXED_POINT_DIVISOR);

		if (deFloatAbs(retrievedValue - expectedValues[pos]) > eps)
			failures++;

		++pos;
	}

	if (failures != 0)
	{
		const char * names[] = { "Retrieved:", "Expected:" };

		for (deUint32 n = 0; n < 2; ++n)
		{
			std::stringstream	css;

			pos = 0;

			for (deUint32 z = 0; z < m_data.depth; ++z)
			for (deUint32 y = 0; y < m_data.height; ++y)
			{
				for (deUint32 x = 0; x < m_data.width; ++x)
				{
					const float	retrievedValue	= float(bufferPtr[pos]) / float(FIXED_POINT_DIVISOR);
					const float	expectedValue	= expectedValues[pos];

					if (deFloatAbs(retrievedValue - expectedValue) > eps)
						css << std::setprecision(8) << std::setw(12) << (n == 0 ? retrievedValue : expectedValue) << ",";
					else
						css << "____________,";

					pos++;
				}

				css << std::endl;
			}

			log << tcu::TestLog::Message << names[n] << tcu::TestLog::EndMessage;
			log << tcu::TestLog::Message << css.str() << tcu::TestLog::EndMessage;
		}
	}

	return failures == 0;
}

bool RayTracingBuiltinLaunchTestInstance::validateVectorBuffer (de::MovePtr<BufferWithMemory> buffer)
{
	const float			eps				= float(FIXED_POINT_ALLOWED_ERROR) / float(FIXED_POINT_DIVISOR);
	const deInt32*		bufferPtr		= (deInt32*)buffer->getAllocation().getHostPtr();
	const vector<float>	expectedValues	= expectedVectorValuesBuffer();
	const deUint32		depth			= 3u; // vec3
	tcu::TestLog&		log				= m_context.getTestContext().getLog();
	deUint32			failures		= 0;
	deUint32			pos				= 0;

	DE_ASSERT(depth <= m_data.depth);

	for (deUint32 z = 0; z < depth; ++z)
	for (deUint32 y = 0; y < m_data.height; ++y)
	for (deUint32 x = 0; x < m_data.width; ++x)
	{
		const float retrievedValue	= float(bufferPtr[pos]) / float(FIXED_POINT_DIVISOR);

		if (deFloatAbs(retrievedValue - expectedValues[pos]) > eps)
			failures++;

		++pos;
	}

	if (failures != 0)
	{
		const char*			names[] = { "Retrieved", "Expected " };
		std::stringstream	css;

		for (deUint32 y = 0; y < m_data.height; ++y)
		{
			for (deUint32 x = 0; x < m_data.width; ++x)
			{
				for (deUint32 n = 0; n < 2; ++n)
				{
					css << names[n] << " at (" << x << "," << y << ") {";

					for (deUint32 z = 0; z < depth; ++z)
					{
						pos = x + m_data.width * (y + m_data.height * z);

						const float	retrievedValue	= float(bufferPtr[pos]) / float(FIXED_POINT_DIVISOR);
						const float	expectedValue	= expectedValues[pos];

						if (deFloatAbs(retrievedValue - expectedValue) > eps)
							css << std::setprecision(8) << std::setw(12) << (n == 0 ? retrievedValue : expectedValue) << ",";
						else
							css << "____________,";
					}

					css << "}" << std::endl;
				}
			}
		}

		log << tcu::TestLog::Message << css.str() << tcu::TestLog::EndMessage;
	}

	return failures == 0;
}

bool RayTracingBuiltinLaunchTestInstance::validateMatrixBuffer (de::MovePtr<BufferWithMemory> buffer)
{
	const float			eps				= float(FIXED_POINT_ALLOWED_ERROR) / float(FIXED_POINT_DIVISOR);
	const deInt32*		bufferPtr		= (deInt32*)buffer->getAllocation().getHostPtr();
	const vector<float>	expectedValues	= expectedMatrixValuesBuffer();
	const deUint32		depth			= 12u; // mat3x4 or mat4x3
	tcu::TestLog&		log				= m_context.getTestContext().getLog();
	deUint32			failures		= 0;
	deUint32			pos				= 0;

	DE_ASSERT(depth <= m_data.depth);

	for (deUint32 z = 0; z < depth; ++z)
	for (deUint32 y = 0; y < m_data.height; ++y)
	for (deUint32 x = 0; x < m_data.width; ++x)
	{
		const float retrievedValue	= float(bufferPtr[pos]) / float(FIXED_POINT_DIVISOR);

		if (deFloatAbs(retrievedValue - expectedValues[pos]) > eps)
			failures++;

		++pos;
	}

	if (failures != 0)
	{
		const char*			names[] = { "Retrieved", "Expected" };
		std::stringstream	css;

		for (deUint32 y = 0; y < m_data.height; ++y)
		{
			for (deUint32 x = 0; x < m_data.width; ++x)
			{
				css << "At (" << x << "," << y << ")" << std::endl;
				for (deUint32 n = 0; n < 2; ++n)
				{
					css << names[n] << std::endl << "{" << std::endl;

					for (deUint32 z = 0; z < depth; ++z)
					{
						pos = x + m_data.width * (y + m_data.height * z);

						const float	retrievedValue	= float(bufferPtr[pos]) / float(FIXED_POINT_DIVISOR);
						const float	expectedValue	= expectedValues[pos];

						if (z % 4 == 0)
							css << "    {";

						if (deFloatAbs(retrievedValue - expectedValue) > eps)
							css << std::setprecision(5) << std::setw(9) << (n == 0 ? retrievedValue : expectedValue) << ",";
						else
							css << "_________,";

						if (z % 4 == 3)
							css << "}" << std::endl;
					}

					css << "}" << std::endl;
				}
			}
		}

		log << tcu::TestLog::Message << css.str() << tcu::TestLog::EndMessage;
	}

	return failures == 0;
}

tcu::TestStatus RayTracingBuiltinLaunchTestInstance::iterate (void)
{
	checkSupportInInstance();

	de::MovePtr<BufferWithMemory>	buffer	= runTest();
	const bool						ok		= m_data.fixedPointMatrixOutput ? validateMatrixBuffer(buffer)
											: m_data.fixedPointVectorOutput ? validateVectorBuffer(buffer)
											: m_data.fixedPointScalarOutput ? validateFloatBuffer(buffer)
											: validateIntBuffer(buffer);

	if (ok)
		return tcu::TestStatus::pass("pass");
	else
		return tcu::TestStatus::fail("fail");
}

static const struct Stages
{
	const char*				name;
	VkShaderStageFlagBits	stage;
}
stages[]
{
	{ "rgen", VK_SHADER_STAGE_RAYGEN_BIT_KHR		},
	{ "ahit", VK_SHADER_STAGE_ANY_HIT_BIT_KHR		},
	{ "chit", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR	},
	{ "sect", VK_SHADER_STAGE_INTERSECTION_BIT_KHR	},
	{ "miss", VK_SHADER_STAGE_MISS_BIT_KHR			},
	{ "call", VK_SHADER_STAGE_CALLABLE_BIT_KHR		},
};

static const struct GeomTypes
{
	const char*	name;
	GeomType	geomType;
}
geomTypes[] =
{
	{ "triangles",	GEOM_TYPE_TRIANGLES	},
	{ "aabs",		GEOM_TYPE_AABBS		},
};

void createLaunchTests (tcu::TestContext& testCtx, tcu::TestCaseGroup* builtinGroup, TestId id, const char* name, const VkShaderStageFlags shaderStageFlags)
{
	const struct
	{
		deUint32	width;
		deUint32	height;
		deUint32	depth;
	}
	sizes[] =
	{
		{     1,     1,     1 },
		{    16,    16,    16 },
		{   256,   256,     1 },
		{ 16384,     1,     1 },
		{     1, 16384,     1 },
		{     1,     1, 16384 },
		{   128,   128,   128 },
		{  2048,  4096,     1 },
		{   317,  3331,     1 },
		{     1,  1331,   111 },
	};

	de::MovePtr<tcu::TestCaseGroup>		group	(new tcu::TestCaseGroup(testCtx, de::toLower(name).c_str(), ""));

	for (size_t stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stages); ++stageNdx)
	{
		if ((shaderStageFlags & stages[stageNdx].stage) == 0)
			continue;

		for (size_t sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizes); ++sizeNdx)
		{
			const deUint32	width					= sizes[sizeNdx].width;
			const deUint32	height					= sizes[sizeNdx].height;
			const deUint32	depth					= sizes[sizeNdx].depth;
			const bool		plain					= isPlain(width, height, depth);
			const deUint32	k						= (plain ? 1 : 6);
			const deUint32	largestGroup			= k * width * height * depth;
			const deUint32	squaresGroupCount		= largestGroup;
			const deUint32	geometriesGroupCount	= 1;
			const deUint32	instancesGroupCount		= 1;
			const CaseDef	caseDef					=
			{
				id,						//  TestId					id;
				name,					//  const char*				name;
				width,					//  deUint32				width;
				height,					//  deUint32				height;
				depth,					//  deUint32				depth;
				depth,					//  deUint32				raysDepth;
				VK_FORMAT_R32_SINT,		//  VkFormat				format;
				false,					//  bool					fixedPointScalarOutput;
				false,					//  bool					fixedPointVectorOutput;
				false,					//  bool					fixedPointMatrixOutput;
				GEOM_TYPE_TRIANGLES,	//  GeomType				geomType;
				squaresGroupCount,		//  deUint32				squaresGroupCount;
				geometriesGroupCount,	//  deUint32				geometriesGroupCount;
				instancesGroupCount,	//  deUint32				instancesGroupCount;
				stages[stageNdx].stage,	//  VkShaderStageFlagBits	stage;
				false,					//  bool					skipTriangles;
				false,					//  bool					skipAABSs;
				false,					//  bool					opaque;
				false,					//  bool					frontFace;
				0u,						//  VkPipelineCreateFlags	pipelineCreateFlags;
				false,					//	bool					useSpecConstants;
			};
			const std::string	suffix		= de::toString(caseDef.width) + '_' + de::toString(caseDef.height) + '_' + de::toString(caseDef.depth);
			const std::string	testName	= string(stages[stageNdx].name) + '_' + suffix;

			group->addChild(new RayTracingTestCase(testCtx, testName.c_str(), "", caseDef));
		}
	}

	builtinGroup->addChild(group.release());
}

void createScalarTests (tcu::TestContext& testCtx, tcu::TestCaseGroup* builtinGroup, TestId id, const char* name, const VkShaderStageFlags shaderStageFlags)
{
	const struct
	{
		deUint32	width;
		deUint32	height;
		TestId		id;
	}
	sizes[] =
	{
		{  16,  16, TEST_ID_HIT_KIND_EXT	},
		{  16,  16, TEST_ID_HIT_T_EXT		},
		{  16,  16, TEST_ID_RAY_T_MIN_EXT	},
		{  16,  16, TEST_ID_RAY_T_MAX_EXT	},
		{  32,  32, TEST_ID_LAST			},
		{  64,  64, TEST_ID_LAST			},
		{ 256, 256, TEST_ID_LAST			},
	};
	const bool		fourGeometryGroups		=  id == TEST_ID_HIT_KIND_EXT
											|| id == TEST_ID_HIT_T_EXT
											|| id == TEST_ID_RAY_T_MIN_EXT
											|| id == TEST_ID_RAY_T_MAX_EXT;
	const bool		fixedPointScalarOutput	=  id == TEST_ID_HIT_T_EXT
											|| id == TEST_ID_RAY_T_MIN_EXT
											|| id == TEST_ID_RAY_T_MAX_EXT;
	const deUint32	imageDepth				= 1;
	const deUint32	rayDepth				= 1;

	de::MovePtr<tcu::TestCaseGroup>		group	(new tcu::TestCaseGroup(testCtx, de::toLower(name).c_str(), ""));

	for (size_t geomTypesNdx = 0; geomTypesNdx < DE_LENGTH_OF_ARRAY(geomTypes); ++geomTypesNdx)
	for (size_t stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stages); ++stageNdx)
	{
		const GeomType	geomType	= geomTypes[geomTypesNdx].geomType;

		if ((shaderStageFlags & stages[stageNdx].stage) == 0)
			continue;

		if (stages[stageNdx].stage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR && geomTypes[geomTypesNdx].geomType == GEOM_TYPE_TRIANGLES)
			continue;

		bool testAdded				= false;
		bool generalTestsStarted	= false;

		for (size_t sizesNdx = 0; sizesNdx < DE_LENGTH_OF_ARRAY(sizes); ++sizesNdx)
		{
			const bool		specializedTest			= (sizes[sizesNdx].id != TEST_ID_LAST);
			const deUint32	width					= sizes[sizesNdx].width;
			const deUint32	height					= sizes[sizesNdx].height;
			const deUint32	instancesGroupCount		= fourGeometryGroups ? 1 : 4;
			const deUint32	geometriesGroupCount	= fourGeometryGroups ? 4 : 8;
			const deUint32	largestGroup			= width * height / geometriesGroupCount / instancesGroupCount;
			const deUint32	squaresGroupCount		= largestGroup;
			const CaseDef	caseDef					=
			{
				id,						//  TestId					id;
				name,					//  const char*				name;
				width,					//  deUint32				width;
				height,					//  deUint32				height;
				imageDepth,				//  deUint32				depth;
				rayDepth,				//  deUint32				raysDepth;
				VK_FORMAT_R32_SINT,		//  VkFormat				format;
				fixedPointScalarOutput,	//  bool					fixedPointScalarOutput;
				false,					//  bool					fixedPointVectorOutput;
				false,					//  bool					fixedPointMatrixOutput;
				geomType,				//  GeomType				geomType;
				squaresGroupCount,		//  deUint32				squaresGroupCount;
				geometriesGroupCount,	//  deUint32				geometriesGroupCount;
				instancesGroupCount,	//  deUint32				instancesGroupCount;
				stages[stageNdx].stage,	//  VkShaderStageFlagBits	stage;
				false,					//  bool					skipTriangles;
				false,					//  bool					skipAABSs;
				false,					//  bool					opaque;
				false,					//  bool					frontFace;
				0u,						//  VkPipelineCreateFlags	pipelineCreateFlags;
				false,					//	bool					useSpecConstants;
			};
			const std::string	suffix		= '_' + de::toString(caseDef.width) + '_' + de::toString(caseDef.height);
			const std::string	testName	= string(stages[stageNdx].name) + '_' + geomTypes[geomTypesNdx].name + (specializedTest ? "" : suffix);

			if (specializedTest)
			{
				DE_UNREF(generalTestsStarted);
				DE_ASSERT(!generalTestsStarted);

				if (sizes[sizesNdx].id != id)
					continue;
			}
			else
			{
				generalTestsStarted = true;
			}

			group->addChild(new RayTracingTestCase(testCtx, testName.c_str(), "", caseDef));
			testAdded = true;

			if (specializedTest)
				break;
		}

		DE_ASSERT(testAdded);
		DE_UNREF(testAdded);
	}

	builtinGroup->addChild(group.release());
}

void createRayFlagsTests (tcu::TestContext& testCtx, tcu::TestCaseGroup* builtinGroup, TestId id, const char* name, const VkShaderStageFlags shaderStageFlags)
{
	const deUint32	width		= 16;
	const deUint32	height		= 16;
	const deUint32	imageDepth	= 1;
	const deUint32	rayDepth	= 1;

	const struct Opaques
	{
		const char*	name;
		bool		flag;
	}
	opaques[] =
	{
		{ "opaque",		true	},
		{ "noopaque",	false	},
	};
	const struct Faces
	{
		const char*	name;
		bool		flag;
	}
	faces[] =
	{
		{ "frontface",	true	},
		{ "backface",	false	},
	};
	const struct SkipRayFlags
	{
		const char*	name;
		bool		skipTriangles;
		bool		skipAABBs;
	}
	skipRayFlags[] =
	{
		{ "raynoskipflags",			false,	false	},
		{ "rayskiptriangles",		true,	false	},
		{ "rayskipaabbs",			false,	true	},
	};
	const struct PipelineFlags
	{
		const char*				name;
		VkPipelineCreateFlags	flag;
	}
	pipelineFlags[] =
	{
		{ "pipelinenoskipflags",	static_cast<VkPipelineCreateFlags>(0)																		},
		{ "pipelineskiptriangles",	VK_PIPELINE_CREATE_RAY_TRACING_SKIP_TRIANGLES_BIT_KHR														},
		{ "pipelineskipaabbs",		VK_PIPELINE_CREATE_RAY_TRACING_SKIP_AABBS_BIT_KHR															},
	};

	de::MovePtr<tcu::TestCaseGroup>	group	(new tcu::TestCaseGroup(testCtx, de::toLower(name).c_str(), ""));

	for (size_t geomTypesNdx = 0; geomTypesNdx < DE_LENGTH_OF_ARRAY(geomTypes); ++geomTypesNdx)
	{
		const GeomType					geomType	= geomTypes[geomTypesNdx].geomType;
		de::MovePtr<tcu::TestCaseGroup>	geomGroup	(new tcu::TestCaseGroup(testCtx, geomTypes[geomTypesNdx].name, ""));

		for (size_t skipRayFlagsNdx = 0; skipRayFlagsNdx < DE_LENGTH_OF_ARRAY(skipRayFlags); ++skipRayFlagsNdx)
		{
			de::MovePtr<tcu::TestCaseGroup>	rayFlagsGroup	(new tcu::TestCaseGroup(testCtx, skipRayFlags[skipRayFlagsNdx].name, ""));

			for (size_t pipelineFlagsNdx = 0; pipelineFlagsNdx < DE_LENGTH_OF_ARRAY(pipelineFlags); ++pipelineFlagsNdx)
			{
				const bool skipTriangles	= (skipRayFlags[skipRayFlagsNdx].skipTriangles	|| (pipelineFlags[pipelineFlagsNdx].flag & VK_PIPELINE_CREATE_RAY_TRACING_SKIP_TRIANGLES_BIT_KHR));
				const bool skipAABBs		= (skipRayFlags[skipRayFlagsNdx].skipAABBs		|| (pipelineFlags[pipelineFlagsNdx].flag & VK_PIPELINE_CREATE_RAY_TRACING_SKIP_AABBS_BIT_KHR));

				// Skipping both triangles and AABBs is not legal according to the spec.
				if (skipTriangles && skipAABBs)
					continue;

				de::MovePtr<tcu::TestCaseGroup>	pipelineFlagsGroup	(new tcu::TestCaseGroup(testCtx, pipelineFlags[pipelineFlagsNdx].name, ""));

				for (size_t opaquesNdx = 0; opaquesNdx < DE_LENGTH_OF_ARRAY(opaques); ++opaquesNdx)
				for (size_t facesNdx = 0; facesNdx < DE_LENGTH_OF_ARRAY(faces); ++facesNdx)
				{
					const std::string				geomPropertiesGroupName	= string(opaques[opaquesNdx].name) + '_' + string(faces[facesNdx].name);
					de::MovePtr<tcu::TestCaseGroup>	geomPropertiesGroup		(new tcu::TestCaseGroup(testCtx, geomPropertiesGroupName.c_str(), ""));

					for (size_t stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stages); ++stageNdx)
					{
						if ((shaderStageFlags & stages[stageNdx].stage) == 0)
							continue;

						if (stages[stageNdx].stage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR && geomTypes[geomTypesNdx].geomType == GEOM_TYPE_TRIANGLES)
							continue;

						const deUint32		instancesGroupCount		= 1;
						const deUint32		geometriesGroupCount	= 1;
						const deUint32		largestGroup			= width * height / geometriesGroupCount / instancesGroupCount;
						const deUint32		squaresGroupCount		= largestGroup;
						const CaseDef		caseDef					=
						{
							id,												//  TestId					id;
							name,											//  const char*				name;
							width,											//  deUint32				width;
							height,											//  deUint32				height;
							imageDepth,										//  deUint32				depth;
							rayDepth,										//  deUint32				raysDepth;
							VK_FORMAT_R32_SINT,								//  VkFormat				format;
							false,											//  bool					fixedPointScalarOutput;
							false,											//  bool					fixedPointVectorOutput;
							false,											//  bool					fixedPointMatrixOutput;
							geomType,										//  GeomType				geomType;
							squaresGroupCount,								//  deUint32				squaresGroupCount;
							geometriesGroupCount,							//  deUint32				geometriesGroupCount;
							instancesGroupCount,							//  deUint32				instancesGroupCount;
							stages[stageNdx].stage,							//  VkShaderStageFlagBits	stage;
							skipRayFlags[skipRayFlagsNdx].skipTriangles,	//  bool					skipTriangles;
							skipRayFlags[skipRayFlagsNdx].skipAABBs,		//  bool					skipAABSs;
							opaques[opaquesNdx].flag,						//  bool					opaque;
							faces[facesNdx].flag,							//  bool					frontFace;
							pipelineFlags[pipelineFlagsNdx].flag,			//  VkPipelineCreateFlags	pipelineCreateFlags;
							false,											//	bool					useSpecConstants;
						};
						const std::string	testName				= string(stages[stageNdx].name) ;

						geomPropertiesGroup->addChild(new RayTracingTestCase(testCtx, testName.c_str(), "", caseDef));
					}

					pipelineFlagsGroup->addChild(geomPropertiesGroup.release());
				}

				rayFlagsGroup->addChild(pipelineFlagsGroup.release());
			}

			geomGroup->addChild(rayFlagsGroup.release());
		}

		group->addChild(geomGroup.release());
	}

	builtinGroup->addChild(group.release());
}

void createMultiOutputTests (tcu::TestContext& testCtx, tcu::TestCaseGroup* builtinGroup, TestId id, const char* name, const VkShaderStageFlags shaderStageFlags)
{
	const bool		fixedPointVectorOutput	=  id == TEST_ID_WORLD_RAY_ORIGIN_EXT
											|| id == TEST_ID_WORLD_RAY_DIRECTION_EXT
											|| id == TEST_ID_OBJECT_RAY_ORIGIN_EXT
											|| id == TEST_ID_OBJECT_RAY_DIRECTION_EXT;
	const bool		fixedPointMatrixOutput	=  id == TEST_ID_OBJECT_TO_WORLD_EXT
											|| id == TEST_ID_WORLD_TO_OBJECT_EXT
											|| id == TEST_ID_OBJECT_TO_WORLD_3X4_EXT
											|| id == TEST_ID_WORLD_TO_OBJECT_3X4_EXT;
	const deUint32	imageDepth				= fixedPointMatrixOutput ? 4 * 4
											: fixedPointVectorOutput ? 4
											: 0;
	const deUint32	rayDepth				= 1;

	de::MovePtr<tcu::TestCaseGroup>		group	(new tcu::TestCaseGroup(testCtx, de::toLower(name).c_str(), ""));

	DE_ASSERT(imageDepth != 0);

	for (size_t geomTypesNdx = 0; geomTypesNdx < DE_LENGTH_OF_ARRAY(geomTypes); ++geomTypesNdx)
	for (size_t stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stages); ++stageNdx)
	{
		const GeomType	geomType	= geomTypes[geomTypesNdx].geomType;

		if ((shaderStageFlags & stages[stageNdx].stage) == 0)
			continue;

		if (stages[stageNdx].stage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR && geomTypes[geomTypesNdx].geomType == GEOM_TYPE_TRIANGLES)
			continue;

		const deUint32		width					= 4;
		const deUint32		height					= 4;
		const deUint32		instancesGroupCount		= 4;
		const deUint32		geometriesGroupCount	= 1;
		const deUint32		largestGroup			= width * height / geometriesGroupCount / instancesGroupCount;
		const deUint32		squaresGroupCount		= largestGroup;
		const CaseDef		caseDef					=
		{
			id,						//  TestId					id;
			name,					//  const char*				name;
			width,					//  deUint32				width;
			height,					//  deUint32				height;
			imageDepth,				//  deUint32				depth;
			rayDepth,				//  deUint32				raysDepth;
			VK_FORMAT_R32_SINT,		//  VkFormat				format;
			false,					//  bool					fixedPointScalarOutput;
			fixedPointVectorOutput,	//  bool					fixedPointVectorOutput;
			fixedPointMatrixOutput,	//  bool					fixedPointMatrixOutput;
			geomType,				//  GeomType				geomType;
			squaresGroupCount,		//  deUint32				squaresGroupCount;
			geometriesGroupCount,	//  deUint32				geometriesGroupCount;
			instancesGroupCount,	//  deUint32				instancesGroupCount;
			stages[stageNdx].stage,	//  VkShaderStageFlagBits	stage;
			false,					//  bool					rayFlagSkipTriangles;
			false,					//  bool					rayFlagSkipAABSs;
			false,					//  bool					opaque;
			false,					//  bool					frontFace;
			0u,						//  VkPipelineCreateFlags	pipelineCreateFlags;
			false,					//	bool					useSpecConstants;
		};
		const std::string	testName				= string(stages[stageNdx].name) + '_' + geomTypes[geomTypesNdx].name;

		group->addChild(new RayTracingTestCase(testCtx, testName.c_str(), "", caseDef));
	}

	builtinGroup->addChild(group.release());
}
}	// anonymous

tcu::TestCaseGroup*	createBuiltinTests (tcu::TestContext& testCtx)
{
	typedef void CreateBuiltinTestsFunc (tcu::TestContext& testCtx, tcu::TestCaseGroup* group, TestId id, const char* name, const VkShaderStageFlags);

	const VkShaderStageFlagBits	R	= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	const VkShaderStageFlagBits	A	= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	const VkShaderStageFlagBits	C	= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	const VkShaderStageFlagBits	M	= VK_SHADER_STAGE_MISS_BIT_KHR;
	const VkShaderStageFlagBits	I	= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
	const VkShaderStageFlagBits	L	= VK_SHADER_STAGE_CALLABLE_BIT_KHR;

	const struct
	{
		TestId					id;
		const char*				name;
		VkShaderStageFlags		stages;
		CreateBuiltinTestsFunc*	createBuiltinTestsFunc;
	}
	tests[] =
	{
		{ TEST_ID_LAUNCH_ID_EXT,				"LaunchIDEXT"			,	R	|	A	|	C	|	I	|	M	|	L	, createLaunchTests			},
		{ TEST_ID_LAUNCH_SIZE_EXT,				"LaunchSizeEXT"			,	R	|	A	|	C	|	I	|	M	|	L	, createLaunchTests			},
		{ TEST_ID_PRIMITIVE_ID,					"PrimitiveID"			,			A	|	C	|	I					, createScalarTests			},
		{ TEST_ID_INSTANCE_ID,					"InstanceID"			,			A	|	C	|	I					, createScalarTests			},
		{ TEST_ID_INSTANCE_CUSTOM_INDEX_EXT,	"InstanceCustomIndexEXT",			A	|	C	|	I					, createScalarTests			},
		{ TEST_ID_GEOMETRY_INDEX_EXT,			"GeometryIndexEXT"		,			A	|	C	|	I					, createScalarTests			},
		{ TEST_ID_WORLD_RAY_ORIGIN_EXT,			"WorldRayOriginEXT"		,			A	|	C	|	I	|	M			, createMultiOutputTests	},
		{ TEST_ID_WORLD_RAY_DIRECTION_EXT,		"WorldRayDirectionEXT"	,			A	|	C	|	I	|	M			, createMultiOutputTests	},
		{ TEST_ID_OBJECT_RAY_ORIGIN_EXT,		"ObjectRayOriginEXT"	,			A	|	C	|	I					, createMultiOutputTests	},
		{ TEST_ID_OBJECT_RAY_DIRECTION_EXT,		"ObjectRayDirectionEXT"	,			A	|	C	|	I					, createMultiOutputTests	},
		{ TEST_ID_RAY_T_MIN_EXT,				"RayTminEXT"			,			A	|	C	|	I	|	M			, createScalarTests			},
		{ TEST_ID_RAY_T_MAX_EXT,				"RayTmaxEXT"			,			A	|	C	|	I	|	M			, createScalarTests			},
		{ TEST_ID_INCOMING_RAY_FLAGS_EXT,		"IncomingRayFlagsEXT"	,			A	|	C	|	I	|	M			, createRayFlagsTests		},
		{ TEST_ID_HIT_T_EXT,					"HitTEXT"				,			A	|	C							, createScalarTests			},
		{ TEST_ID_HIT_KIND_EXT,					"HitKindEXT"			,			A	|	C							, createScalarTests			},
		{ TEST_ID_OBJECT_TO_WORLD_EXT,			"ObjectToWorldEXT"		,			A	|	C	|	I					, createMultiOutputTests	},
		{ TEST_ID_WORLD_TO_OBJECT_EXT,			"WorldToObjectEXT"		,			A	|	C	|	I					, createMultiOutputTests	},
        { TEST_ID_OBJECT_TO_WORLD_3X4_EXT,		"ObjectToWorld3x4EXT"	,			A	|	C	|	I					, createMultiOutputTests	},
        { TEST_ID_WORLD_TO_OBJECT_3X4_EXT,		"WorldToObject3x4EXT"	,			A	|	C	|	I					, createMultiOutputTests	},
	};

	de::MovePtr<tcu::TestCaseGroup> builtinGroup(new tcu::TestCaseGroup(testCtx, "builtin", "Ray tracing shader builtin tests"));

	for (size_t testNdx = 0; testNdx < DE_LENGTH_OF_ARRAY(tests); ++testNdx)
		tests[testNdx].createBuiltinTestsFunc(testCtx, builtinGroup.get(), tests[testNdx].id, tests[testNdx].name, tests[testNdx].stages);

	return builtinGroup.release();
}

tcu::TestCaseGroup* createSpecConstantTests	(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "spec_constants", "Test using spec constants in ray tracing shader stages"));

	const VkShaderStageFlags	stageFlags				= VK_SHADER_STAGE_RAYGEN_BIT_KHR
														| VK_SHADER_STAGE_ANY_HIT_BIT_KHR
														| VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
														| VK_SHADER_STAGE_MISS_BIT_KHR
														| VK_SHADER_STAGE_INTERSECTION_BIT_KHR
														| VK_SHADER_STAGE_CALLABLE_BIT_KHR;
	const deUint32				width					= 256u;
	const deUint32				height					= 256u;
	const deUint32				depth					= 1u;
	const bool					plain					= isPlain(width, height, depth);
	const deUint32				k						= (plain ? 1 : 6);
	const deUint32				largestGroup			= k * width * height * depth;
	const deUint32				squaresGroupCount		= largestGroup;
	const deUint32				geometriesGroupCount	= 1;
	const deUint32				instancesGroupCount		= 1;

	for (int stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stages); ++stageNdx)
	{
		if ((stageFlags & stages[stageNdx].stage) == 0)
			continue;

		const CaseDef caseDef =
		{
			TEST_ID_LAUNCH_ID_EXT,	//  TestId					id;
			"LaunchIDEXT",			//  const char*				name;
			width,					//  deUint32				width;
			height,					//  deUint32				height;
			depth,					//  deUint32				depth;
			depth,					//  deUint32				raysDepth;
			VK_FORMAT_R32_SINT,		//  VkFormat				format;
			false,					//  bool					fixedPointScalarOutput;
			false,					//  bool					fixedPointVectorOutput;
			false,					//  bool					fixedPointMatrixOutput;
			GEOM_TYPE_TRIANGLES,	//  GeomType				geomType;
			squaresGroupCount,		//  deUint32				squaresGroupCount;
			geometriesGroupCount,	//  deUint32				geometriesGroupCount;
			instancesGroupCount,	//  deUint32				instancesGroupCount;
			stages[stageNdx].stage,	//  VkShaderStageFlagBits	stage;
			false,					//  bool					skipTriangles;
			false,					//  bool					skipAABSs;
			false,					//  bool					opaque;
			false,					//  bool					frontFace;
			0u,						//  VkPipelineCreateFlags	pipelineCreateFlags;
			true,					//	bool					useSpecConstants;
		};

		group->addChild(new RayTracingTestCase(testCtx, stages[stageNdx].name, "", caseDef));
	}

	return group.release();
}

}	// RayTracing
}	// vkt
