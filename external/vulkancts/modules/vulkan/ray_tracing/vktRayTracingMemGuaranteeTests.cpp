/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief Ray Tracing Watertightness tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingWatertightnessTests.hpp"

#include "vkDefs.hpp"

#include "vktTestCase.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkTypeUtil.hpp"

#include "vkRayTracingUtil.hpp"

#include "deRandom.hpp"

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

enum TestType
{
	TEST_TYPE_INSIDE_STAGE,
	TEST_TYPE_BETWEEN_STAGES,
};


struct CaseDef
{
	TestType				testType;
	VkShaderStageFlagBits	stage;
	deUint32				width;
	deUint32				height;
	deUint32				squaresGroupCount;
	deUint32				geometriesGroupCount;
	deUint32				instancesGroupCount;
};

enum ShaderGroups
{
	FIRST_GROUP		= 0,
	RAYGEN_GROUP	= FIRST_GROUP,
	MISS_GROUP,
	HIT_GROUP,
	GROUP_COUNT
};

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

VkImageCreateInfo makeImageCreateInfo (deUint32 width, deUint32 height, VkFormat format)
{
	const VkImageUsageFlags	usage			= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	const VkImageCreateInfo	imageCreateInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		(VkImageCreateFlags)0u,					// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
		format,									// VkFormat					format;
		makeExtent3D(width, height, 1u),		// VkExtent3D				extent;
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

class RayTracingBuildTestInstance : public TestInstance
{
public:
																RayTracingBuildTestInstance			(Context& context, const CaseDef& data);
																~RayTracingBuildTestInstance		(void);
	tcu::TestStatus												iterate								(void);

protected:
	void														checkSupportInInstance				(void) const;
	de::MovePtr<BufferWithMemory>								runTest								(void);
	Move<VkPipeline>											makePipeline						(de::MovePtr<RayTracingPipeline>&							rayTracingPipeline,
																									 VkPipelineLayout											pipelineLayout);
	de::MovePtr<BufferWithMemory>								createShaderBindingTable			 (const InstanceInterface&									vki,
																									 const DeviceInterface&										vkd,
																									 const VkDevice												device,
																									 const VkPhysicalDevice										physicalDevice,
																									 const VkPipeline											pipeline,
																									 Allocator&													allocator,
																									 de::MovePtr<RayTracingPipeline>&							rayTracingPipeline,
																									 const deUint32												group,
																									 const deUint32												groupCount = 1u);
	de::MovePtr<TopLevelAccelerationStructure>					initTopAccelerationStructure		(VkCommandBuffer											cmdBuffer,
																									 vector<de::SharedPtr<BottomLevelAccelerationStructure> >&	bottomLevelAccelerationStructures);
	vector<de::SharedPtr<BottomLevelAccelerationStructure>	>	initBottomAccelerationStructures	(VkCommandBuffer											cmdBuffer);
	de::MovePtr<BottomLevelAccelerationStructure>				initBottomAccelerationStructure		(VkCommandBuffer											cmdBuffer,
																									 tcu::UVec2&												startPos);

private:
	CaseDef														m_data;
	VkShaderStageFlags											m_shaders;
	VkShaderStageFlags											m_extraCallShaders;
	deUint32													m_raygenShaderGroup;
	deUint32													m_missShaderGroup;
	deUint32													m_hitShaderGroup;
	deUint32													m_callableShaderGroup;
	deUint32													m_shaderGroupCount;
};

RayTracingBuildTestInstance::RayTracingBuildTestInstance (Context& context, const CaseDef& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
	, m_shaders				(0)
	, m_extraCallShaders	(0)
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

	if (collection.contains("cal0")) m_extraCallShaders++;

	for (BinaryCollection::Iterator it =  collection.begin(); it != collection.end(); ++it)
		shaderCount++;

	if (shaderCount != m_extraCallShaders + (deUint32)dePop32(m_shaders))
		TCU_THROW(InternalError, "Unused shaders detected in the collection");

	if (0 != (m_shaders & VK_SHADER_STAGE_RAYGEN_BIT_KHR))
		m_raygenShaderGroup		= group++;

	if (0 != (m_shaders & VK_SHADER_STAGE_MISS_BIT_KHR))
		m_missShaderGroup		= group++;

	if (0 != (m_shaders & hitStages))
		m_hitShaderGroup		= group++;

	if (0 != (m_shaders & VK_SHADER_STAGE_CALLABLE_BIT_KHR) || m_extraCallShaders > 0)
		m_callableShaderGroup	= group++;

	m_shaderGroupCount = group;
}

RayTracingBuildTestInstance::~RayTracingBuildTestInstance (void)
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

	CaseDef					m_data;
};

RayTracingTestCase::RayTracingTestCase (tcu::TestContext& context, const char* name, const char* desc, const CaseDef data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
	DE_ASSERT((m_data.width * m_data.height) == (m_data.squaresGroupCount * m_data.geometriesGroupCount * m_data.instancesGroupCount));
}

RayTracingTestCase::~RayTracingTestCase	(void)
{
}

void RayTracingTestCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

	const VkPhysicalDeviceRayTracingPipelineFeaturesKHR&	rayTracingPipelineFeaturesKHR		= context.getRayTracingPipelineFeatures();
	if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == DE_FALSE )
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

	const VkPhysicalDeviceAccelerationStructureFeaturesKHR&	accelerationStructureFeaturesKHR	= context.getAccelerationStructureFeatures();
	if (accelerationStructureFeaturesKHR.accelerationStructure == DE_FALSE)
		TCU_THROW(TestError, "VK_KHR_ray_tracing_pipeline requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");
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
	const vk::ShaderBuildOptions	buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	const std::string	imageQualifiers		= (m_data.testType == TEST_TYPE_BETWEEN_STAGES ? " shadercallcoherent " : "");
	const std::string	glslExtensions		= (m_data.testType == TEST_TYPE_BETWEEN_STAGES ? "#extension GL_KHR_memory_scope_semantics : require\n" : "");
	const bool			calleeIsAnyHit		= (m_data.stage == VK_SHADER_STAGE_INTERSECTION_BIT_KHR);
	const std::string	repackInstruction	= calleeIsAnyHit
											? "reportIntersectionEXT(0.95f, 0u)"
											: "executeCallableEXT(0, 0)";
	const std::string	updateBarrierCaller	= (m_data.testType == TEST_TYPE_BETWEEN_STAGES ? "  memoryBarrier(gl_ScopeShaderCallEXT, gl_StorageSemanticsImage, gl_SemanticsRelease);\n" : "");
	const std::string	updateBarrierCallee	= (m_data.testType == TEST_TYPE_BETWEEN_STAGES ? "  memoryBarrier(gl_ScopeShaderCallEXT, gl_StorageSemanticsImage, gl_SemanticsAcquire);\n" : "");
	const std::string	updateImage0		=
		"  uint  r = uint(gl_LaunchIDEXT.x + gl_LaunchSizeEXT.x * gl_LaunchIDEXT.y);\n"
		"  uvec4 c = uvec4(r, 0, 0, 1);\n"
		"  imageStore(result, ivec2(gl_LaunchIDEXT), c);\n"
		"\n"
		+ updateBarrierCaller +
		"\n"
		"  " + repackInstruction + ";\n";
	const std::string	updateImage1		=
		"  uint  d = imageLoad(result, ivec2(gl_LaunchIDEXT)).x;\n"
		"  imageStore(result, ivec2(gl_LaunchIDEXT), uvec4(d + 1, 0, 0, 1));\n";
	const std::string	updateImageCaller	= updateImage0 + (m_data.testType == TEST_TYPE_INSIDE_STAGE ? updateImage1 : "");
	const std::string	updateImageCallee	= (m_data.testType == TEST_TYPE_BETWEEN_STAGES ? updateImage1 : "");
	const std::string	calleeShaderParam	= calleeIsAnyHit ? "" : "layout(location = 0) callableDataInEXT float dummy;\n";
	const std::string	calleeShader		=
		"#version 460 core\n"
		"#extension GL_EXT_ray_tracing : require\n"
		+ glslExtensions
		+ calleeShaderParam +
		"layout(set = 0, binding = 0, r32ui) uniform uimage2D result;\n"
		"\n"
		"void main()\n"
		"{\n"
		+ updateBarrierCallee
		+ updateImageCallee +
		"}\n";

	switch (m_data.stage)
	{
		case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_tracing : require\n"
				+ glslExtensions +
				"layout(set = 0, binding = 0, r32ui)" + imageQualifiers + "uniform uimage2D result;\n"
				"layout(location = 0) callableDataEXT float dummy;\n"
				"\n"
				"void main()\n"
				"{\n"
				<< updateImageCaller <<
				"}\n";

			programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
			programCollection.glslSources.add("cal0") << glu::CallableSource(updateRayTracingGLSL(calleeShader)) << buildOptions;

			break;
		}

		case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
		{
			programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_tracing : require\n"
				+ glslExtensions +
				"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
				"hitAttributeEXT vec3 attribs;\n"
				"layout(r32ui, set = 0, binding = 0)" + imageQualifiers + "uniform uimage2D result;\n"
				"layout(location = 0) callableDataEXT float dummy;\n"
				"\n"
				"void main()\n"
				"{\n"
				<< updateImageCaller <<
				"}\n";

			programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
			programCollection.glslSources.add("cal0") << glu::CallableSource(updateRayTracingGLSL(calleeShader)) << buildOptions;

			programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
			programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;
			programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(getIntersectionPassthrough())) << buildOptions;

			break;
		}

		case VK_SHADER_STAGE_MISS_BIT_KHR:
		{
			programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_tracing : require\n"
				+ glslExtensions +
				"layout(r32ui, set = 0, binding = 0)" + imageQualifiers + "uniform uimage2D result;\n"
				"layout(location = 0) callableDataEXT float dummy;\n"
				"\n"
				"void main()\n"
				"{\n"
				<< updateImageCaller <<
				"}\n";

			programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
			programCollection.glslSources.add("cal0") << glu::CallableSource(updateRayTracingGLSL(calleeShader)) << buildOptions;

			programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
			programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
			programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(getIntersectionPassthrough())) << buildOptions;

			break;
		}

		case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
		{
			programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_ray_tracing : require\n"
				+ glslExtensions +
				"layout(r32ui, set = 0, binding = 0)" + imageQualifiers + "uniform uimage2D result;\n"
				"\n"
				"void main()\n"
				"{\n"
				<< updateImageCaller <<
				"}\n";

			programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
			programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(calleeShader)) << buildOptions;

			programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(getHitPassthrough())) << buildOptions;
			programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(getMissPassthrough())) << buildOptions;

			break;
		}

		case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
		{
			{
				std::stringstream css;
				css <<
					"#version 460 core\n"
					"#extension GL_EXT_ray_tracing : require\n"
					+ glslExtensions +
					"layout(location = 0) callableDataEXT float dummy;\n"
					"layout(set = 0, binding = 0, r32ui)" + imageQualifiers + "uniform uimage2D result;\n"
					"\n"
					"void main()\n"
					"{\n"
					"  executeCallableEXT(1, 0);\n"
					"}\n";

				programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
			}

			{
				std::stringstream css;
				css <<
					"#version 460 core\n"
					"#extension GL_EXT_ray_tracing : require\n"
					+ glslExtensions +
					"layout(location = 1) callableDataInEXT float dummyIn;\n"
					"layout(location = 0) callableDataEXT float dummyOut;\n"
					"layout(set = 0, binding = 0, r32ui)" + imageQualifiers + "uniform uimage2D result;\n"
					"\n"
					"void main()\n"
					"{\n"
					<< updateImageCaller <<
					"}\n";

				programCollection.glslSources.add("call") << glu::CallableSource(updateRayTracingGLSL(css.str())) << buildOptions;
			}

			programCollection.glslSources.add("cal0") << glu::CallableSource(updateRayTracingGLSL(calleeShader)) << buildOptions;

			break;
		}

		default:
			TCU_THROW(InternalError, "Unknown stage");
	}
}

TestInstance* RayTracingTestCase::createInstance (Context& context) const
{
	return new RayTracingBuildTestInstance(context, m_data);
}

Move<VkPipeline> RayTracingBuildTestInstance::makePipeline (de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
															VkPipelineLayout					pipelineLayout)
{
	const DeviceInterface&	vkd			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();
	vk::BinaryCollection&	collection	= m_context.getBinaryCollection();

	if (0 != (m_shaders & VK_SHADER_STAGE_RAYGEN_BIT_KHR))			rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR		, createShaderModule(vkd, device, collection.get("rgen"), 0), m_raygenShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_ANY_HIT_BIT_KHR))			rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR		, createShaderModule(vkd, device, collection.get("ahit"), 0), m_hitShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR))		rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR	, createShaderModule(vkd, device, collection.get("chit"), 0), m_hitShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_MISS_BIT_KHR))			rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR			, createShaderModule(vkd, device, collection.get("miss"), 0), m_missShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_INTERSECTION_BIT_KHR))	rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR	, createShaderModule(vkd, device, collection.get("sect"), 0), m_hitShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_CALLABLE_BIT_KHR))		rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR		, createShaderModule(vkd, device, collection.get("call"), 0), m_callableShaderGroup + 1);
	if (m_extraCallShaders)											rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR		, createShaderModule(vkd, device, collection.get("cal0"), 0), m_callableShaderGroup);

	Move<VkPipeline> pipeline = rayTracingPipeline->createPipeline(vkd, device, pipelineLayout);

	return pipeline;
}

de::MovePtr<BufferWithMemory> RayTracingBuildTestInstance::createShaderBindingTable (const InstanceInterface&			vki,
																					 const DeviceInterface&				vkd,
																					 const VkDevice						device,
																					 const VkPhysicalDevice				physicalDevice,
																					 const VkPipeline					pipeline,
																					 Allocator&							allocator,
																					 de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
																					 const deUint32						group,
																					 const deUint32						groupCount)
{
	de::MovePtr<BufferWithMemory>	shaderBindingTable;

	if (group < m_shaderGroupCount)
	{
		const deUint32	shaderGroupHandleSize		= getShaderGroupSize(vki, physicalDevice);
		const deUint32	shaderGroupBaseAlignment	= getShaderGroupBaseAlignment(vki, physicalDevice);

		shaderBindingTable = rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, group, groupCount);
	}

	return shaderBindingTable;
}


de::MovePtr<TopLevelAccelerationStructure> RayTracingBuildTestInstance::initTopAccelerationStructure (VkCommandBuffer											cmdBuffer,
																									  vector<de::SharedPtr<BottomLevelAccelerationStructure> >&	bottomLevelAccelerationStructures)
{
	const DeviceInterface&						vkd			= m_context.getDeviceInterface();
	const VkDevice								device		= m_context.getDevice();
	Allocator&									allocator	= m_context.getDefaultAllocator();
	de::MovePtr<TopLevelAccelerationStructure>	result		= makeTopLevelAccelerationStructure();

	result->setInstanceCount(bottomLevelAccelerationStructures.size());

	for (size_t structNdx = 0; structNdx < bottomLevelAccelerationStructures.size(); ++structNdx)
		result->addInstance(bottomLevelAccelerationStructures[structNdx]);

	result->createAndBuild(vkd, device, cmdBuffer, allocator);

	return result;
}

de::MovePtr<BottomLevelAccelerationStructure> RayTracingBuildTestInstance::initBottomAccelerationStructure (VkCommandBuffer	cmdBuffer,
																											tcu::UVec2&		startPos)
{
	const DeviceInterface&							vkd			= m_context.getDeviceInterface();
	const VkDevice									device		= m_context.getDevice();
	Allocator&										allocator	= m_context.getDefaultAllocator();
	de::MovePtr<BottomLevelAccelerationStructure>	result		= makeBottomLevelAccelerationStructure();

	result->setGeometryCount(m_data.geometriesGroupCount);

	for (size_t geometryNdx = 0; geometryNdx < m_data.geometriesGroupCount; ++geometryNdx)
	{
		const float				z				= (m_data.stage == VK_SHADER_STAGE_MISS_BIT_KHR) ? +1.0f : -1.0f;
		std::vector<tcu::Vec3>	geometryData;

		geometryData.reserve(2u * m_data.squaresGroupCount);

		for (size_t squareNdx = 0; squareNdx < m_data.squaresGroupCount; ++squareNdx)
		{
			const deUint32	n	= m_data.width * startPos.y() + startPos.x();
			const float		x0	= float(startPos.x() + 0) / float(m_data.width);
			const float		y0	= float(startPos.y() + 0) / float(m_data.height);
			const float		x1	= float(startPos.x() + 1) / float(m_data.width);
			const float		y1	= float(startPos.y() + 1) / float(m_data.height);
			const deUint32	m	= (73 * (n + 1)) % (m_data.width * m_data.height);

			geometryData.push_back(tcu::Vec3(x0, y0, z));
			geometryData.push_back(tcu::Vec3(x1, y1, z));

			startPos.y() = m / m_data.width;
			startPos.x() = m % m_data.width;
		}

		result->addGeometry(geometryData, false);
	}

	result->createAndBuild(vkd, device, cmdBuffer, allocator);

	return result;
}

vector<de::SharedPtr<BottomLevelAccelerationStructure> > RayTracingBuildTestInstance::initBottomAccelerationStructures (VkCommandBuffer	cmdBuffer)
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

de::MovePtr<BufferWithMemory> RayTracingBuildTestInstance::runTest (void)
{
	const InstanceInterface&			vki									= m_context.getInstanceInterface();
	const DeviceInterface&				vkd									= m_context.getDeviceInterface();
	const VkDevice						device								= m_context.getDevice();
	const VkPhysicalDevice				physicalDevice						= m_context.getPhysicalDevice();
	const deUint32						queueFamilyIndex					= m_context.getUniversalQueueFamilyIndex();
	const VkQueue						queue								= m_context.getUniversalQueue();
	Allocator&							allocator							= m_context.getDefaultAllocator();
	const VkFormat						format								= VK_FORMAT_R32_UINT;
	const deUint32						pixelCount							= m_data.width * m_data.height;
	const deUint32						shaderGroupHandleSize				= getShaderGroupSize(vki, physicalDevice);

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

	const deUint32						callableGroups						= m_extraCallShaders + ((m_shaders & VK_SHADER_STAGE_CALLABLE_BIT_KHR) != 0 ? 1 : 0);
	de::MovePtr<RayTracingPipeline>		rayTracingPipeline					= de::newMovePtr<RayTracingPipeline>();
	const Move<VkPipeline>				pipeline							= makePipeline(rayTracingPipeline, *pipelineLayout);
	const de::MovePtr<BufferWithMemory>	raygenShaderBindingTable			= createShaderBindingTable(vki, vkd, device, physicalDevice, *pipeline, allocator, rayTracingPipeline, m_raygenShaderGroup);
	const de::MovePtr<BufferWithMemory>	missShaderBindingTable				= createShaderBindingTable(vki, vkd, device, physicalDevice, *pipeline, allocator, rayTracingPipeline, m_missShaderGroup);
	const de::MovePtr<BufferWithMemory>	hitShaderBindingTable				= createShaderBindingTable(vki, vkd, device, physicalDevice, *pipeline, allocator, rayTracingPipeline, m_hitShaderGroup);
	const de::MovePtr<BufferWithMemory>	callableShaderBindingTable			= createShaderBindingTable(vki, vkd, device, physicalDevice, *pipeline, allocator, rayTracingPipeline, m_callableShaderGroup, callableGroups);

	const VkStridedDeviceAddressRegionKHR	raygenShaderBindingTableRegion		= raygenShaderBindingTable.get() != NULL ? makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize) : makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	const VkStridedDeviceAddressRegionKHR	missShaderBindingTableRegion		= missShaderBindingTable.get() != NULL ? makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize) : makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	const VkStridedDeviceAddressRegionKHR	hitShaderBindingTableRegion			= hitShaderBindingTable.get() != NULL ? makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize) : makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	const VkStridedDeviceAddressRegionKHR	callableShaderBindingTableRegion	= callableShaderBindingTable.get() != NULL ? makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, callableShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize * callableGroups) : makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

	const VkImageCreateInfo				imageCreateInfo						= makeImageCreateInfo(m_data.width, m_data.height, format);
	const VkImageSubresourceRange		imageSubresourceRange				= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0, 1u);
	const de::MovePtr<ImageWithMemory>	image								= de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
	const Move<VkImageView>				imageView							= makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_2D, format, imageSubresourceRange);

	const VkBufferCreateInfo			bufferCreateInfo					= makeBufferCreateInfo(pixelCount*sizeof(deUint32), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkImageSubresourceLayers		bufferImageSubresourceLayers		= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const VkBufferImageCopy				bufferImageRegion					= makeBufferImageCopy(makeExtent3D(m_data.width, m_data.height, 1u), bufferImageSubresourceLayers);
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
	const VkClearValue					clearValue							= makeClearValueColorU32(1000000u, 0u, 0u, 255u);

	vector<de::SharedPtr<BottomLevelAccelerationStructure> >	bottomLevelAccelerationStructures;
	de::MovePtr<TopLevelAccelerationStructure>					topLevelAccelerationStructure;

	beginCommandBuffer(vkd, *cmdBuffer, 0u);
	{
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preImageBarrier);
		vkd.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1, &imageSubresourceRange);
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, &postImageBarrier);

		bottomLevelAccelerationStructures = initBottomAccelerationStructures(*cmdBuffer);
		topLevelAccelerationStructure = initTopAccelerationStructure(*cmdBuffer, bottomLevelAccelerationStructures);

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
			m_data.width, m_data.height, 1);

		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);

		vkd.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **buffer, 1u, &bufferImageRegion);

		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &postCopyMemoryBarrier);
	}
	endCommandBuffer(vkd, *cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

	invalidateMappedMemoryRange(vkd, device, buffer->getAllocation().getMemory(), buffer->getAllocation().getOffset(), pixelCount * sizeof(deUint32));

	return buffer;
}

void RayTracingBuildTestInstance::checkSupportInInstance (void) const
{
	const InstanceInterface&				vki						= m_context.getInstanceInterface();
	const VkPhysicalDevice					physicalDevice			= m_context.getPhysicalDevice();
	const vk::VkPhysicalDeviceProperties&	properties				= m_context.getDeviceProperties();
	const deUint32							requiredAllocations		= 8u
																	+ TopLevelAccelerationStructure::getRequiredAllocationCount()
																	+ m_data.instancesGroupCount * BottomLevelAccelerationStructure::getRequiredAllocationCount();
	de::MovePtr<RayTracingProperties>		rayTracingProperties	= makeRayTracingProperties(vki, physicalDevice);

	if (rayTracingProperties->getMaxPrimitiveCount() < m_data.squaresGroupCount)
		TCU_THROW(NotSupportedError, "Triangles required more than supported");

	if (rayTracingProperties->getMaxGeometryCount() < m_data.geometriesGroupCount)
		TCU_THROW(NotSupportedError, "Geometries required more than supported");

	if (rayTracingProperties->getMaxInstanceCount() < m_data.instancesGroupCount)
		TCU_THROW(NotSupportedError, "Instances required more than supported");

	if (properties.limits.maxMemoryAllocationCount < requiredAllocations)
		TCU_THROW(NotSupportedError, "Test requires more allocations allowed");
}

tcu::TestStatus RayTracingBuildTestInstance::iterate (void)
{
	checkSupportInInstance();

	const de::MovePtr<BufferWithMemory>	buffer		= runTest();
	const deUint32*						bufferPtr	= (deUint32*)buffer->getAllocation().getHostPtr();
	deUint32							failures	= 0;
	deUint32							pos			= 0;

	for (deUint32 y = 0; y < m_data.height; ++y)
	{
		for (deUint32 x = 0; x < m_data.width; ++x)
		{
			const deUint32	expectedValue	= pos + 1;

			if (bufferPtr[pos] != expectedValue)
				failures++;

			++pos;
		}
	}

	if (failures == 0)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("failures=" + de::toString(failures));
}

}	// anonymous
tcu::TestCaseGroup*	createMemGuaranteeTests (tcu::TestContext& testCtx)
{
	static const struct
	{
		const char*				name;
		VkShaderStageFlagBits	stage;
	}
	stages[]
	{
		{ "rgen", VK_SHADER_STAGE_RAYGEN_BIT_KHR		},
		{ "chit", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR	},
		{ "sect", VK_SHADER_STAGE_INTERSECTION_BIT_KHR	},
		{ "miss", VK_SHADER_STAGE_MISS_BIT_KHR			},
		{ "call", VK_SHADER_STAGE_CALLABLE_BIT_KHR		},
	};

	static const struct
	{
		const char*	name;
		TestType	testType;
	}
	testTypes[]
	{
		{ "inside",		TEST_TYPE_INSIDE_STAGE		},
		{ "between",	TEST_TYPE_BETWEEN_STAGES	},
	};

	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "memguarantee", "Ray tracing memory guarantee tests"));

	for (size_t testTypeNdx = 0; testTypeNdx < DE_LENGTH_OF_ARRAY(testTypes); ++testTypeNdx)
	{
		de::MovePtr<tcu::TestCaseGroup> testTypeGroup(new tcu::TestCaseGroup(testCtx, testTypes[testTypeNdx].name, ""));

		for (size_t stagesNdx = 0; stagesNdx < DE_LENGTH_OF_ARRAY(stages); ++stagesNdx)
		{
			const deUint32	width					= 16u;
			const deUint32	height					= 16u;
			const deUint32	geometriesGroupCount	= 4;
			const deUint32	instancesGroupCount		= 8;
			const deUint32	squaresGroupCount		= width * height / geometriesGroupCount / instancesGroupCount;
			const CaseDef	caseDef					=
			{
				testTypes[testTypeNdx].testType,	//  TestType				testType;
				stages[stagesNdx].stage,			//  VkShaderStageFlagBits	stage;
				width,								//  deUint32				width;
				height,								//  deUint32				height;
				squaresGroupCount,					//  deUint32				squaresGroupCount;
				geometriesGroupCount,				//  deUint32				geometriesGroupCount;
				instancesGroupCount,				//  deUint32				instancesGroupCount;
			};
			const std::string	testName	= de::toString(stages[stagesNdx].name);

			testTypeGroup->addChild(new RayTracingTestCase(testCtx, testName.c_str(), "", caseDef));
		}

		group->addChild(testTypeGroup.release());
	}

	return group.release();
}

}	// RayTracing
}	// vkt
