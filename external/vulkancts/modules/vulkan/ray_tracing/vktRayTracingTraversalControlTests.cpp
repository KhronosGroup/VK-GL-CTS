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
 * \brief Testing traversal control in ray tracing shaders
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingTraversalControlTests.hpp"

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
#include "deRandom.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"

#include "vkRayTracingUtil.hpp"

namespace vkt
{
namespace RayTracing
{
namespace
{
using namespace vk;
using namespace vkt;

static const VkFlags	ALL_RAY_TRACING_STAGES	= VK_SHADER_STAGE_RAYGEN_BIT_KHR
												| VK_SHADER_STAGE_ANY_HIT_BIT_KHR
												| VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
												| VK_SHADER_STAGE_MISS_BIT_KHR
												| VK_SHADER_STAGE_INTERSECTION_BIT_KHR
												| VK_SHADER_STAGE_CALLABLE_BIT_KHR;

enum HitShaderTestType
{
	HSTT_ISECT_REPORT_INTERSECTION		= 0,
	HSTT_ISECT_DONT_REPORT_INTERSECTION	= 1,
	HSTT_AHIT_PASS_THROUGH				= 2,
	HSTT_AHIT_IGNORE_INTERSECTION		= 3,
	HSTT_AHIT_TERMINATE_RAY				= 4,
	HSTT_COUNT
};

enum BottomTestType
{
	BTT_TRIANGLES,
	BTT_AABBS
};

const deUint32			TEST_WIDTH			= 8;
const deUint32			TEST_HEIGHT			= 8;

struct TestParams;

class TestConfiguration
{
public:
	virtual std::vector<de::SharedPtr<BottomLevelAccelerationStructure>>	initBottomAccelerationStructures	(Context&							context,
																												 TestParams&						testParams) = 0;
	virtual de::MovePtr<TopLevelAccelerationStructure>						initTopAccelerationStructure		(Context&							context,
																												 TestParams&						testParams,
																												 std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >&	bottomLevelAccelerationStructures) = 0;
	virtual void															initRayTracingShaders				(de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
																												 Context&							context,
																												TestParams&							testParams) = 0;
	virtual void															initShaderBindingTables				(de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
																												 Context&							context,
																												 TestParams&						testParams,
																												 VkPipeline							pipeline,
																												 deUint32							shaderGroupHandleSize,
																												 deUint32							shaderGroupBaseAlignment,
																												 de::MovePtr<BufferWithMemory>&		raygenShaderBindingTable,
																												 de::MovePtr<BufferWithMemory>&		hitShaderBindingTable,
																												 de::MovePtr<BufferWithMemory>&		missShaderBindingTable,
																												 de::MovePtr<BufferWithMemory>&		callableShaderBindingTable,
																												 VkStridedDeviceAddressRegionKHR&	raygenShaderBindingTableRegion,
																												 VkStridedDeviceAddressRegionKHR&	hitShaderBindingTableRegion,
																												 VkStridedDeviceAddressRegionKHR&	missShaderBindingTableRegion,
																												 VkStridedDeviceAddressRegionKHR&	callableShaderBindingTableRegion) = 0;
	virtual bool															verifyImage							(BufferWithMemory*					resultBuffer,
																												 Context&							context,
																												 TestParams&						testParams) = 0;
	virtual VkFormat														getResultImageFormat				() = 0;
	virtual size_t															getResultImageFormatSize			() = 0;
	virtual VkClearValue													getClearValue						() = 0;
};

struct TestParams
{
	deUint32							width;
	deUint32							height;
	HitShaderTestType					hitShaderTestType;
	BottomTestType						bottomType;
	de::SharedPtr<TestConfiguration>	testConfiguration;

};

deUint32 getShaderGroupHandleSize (const InstanceInterface&	vki,
								   const VkPhysicalDevice	physicalDevice)
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
	const VkImageCreateInfo			imageCreateInfo			=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,																// VkStructureType			sType;
		DE_NULL,																							// const void*				pNext;
		(VkImageCreateFlags)0u,																				// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_3D,																					// VkImageType				imageType;
		format,																								// VkFormat					format;
		makeExtent3D(width, height, depth),																	// VkExtent3D				extent;
		1u,																									// deUint32					mipLevels;
		1u,																									// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,																				// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,																			// VkImageTiling			tiling;
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,		// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,																			// VkSharingMode			sharingMode;
		0u,																									// deUint32					queueFamilyIndexCount;
		DE_NULL,																							// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED																			// VkImageLayout			initialLayout;
	};

	return imageCreateInfo;
}

class SingleSquareConfiguration : public TestConfiguration
{
public:
	std::vector<de::SharedPtr<BottomLevelAccelerationStructure>>	initBottomAccelerationStructures	(Context&							context,
																										 TestParams&						testParams) override;
	de::MovePtr<TopLevelAccelerationStructure>						initTopAccelerationStructure		(Context&							context,
																										 TestParams&						testParams,
																										 std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >&	bottomLevelAccelerationStructures) override;
	void															initRayTracingShaders				(de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
																										 Context&							context,
																										 TestParams&						testParams) override;
	void															initShaderBindingTables				(de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
																										 Context&							context,
																										 TestParams&						testParams,
																										 VkPipeline							pipeline,
																										 deUint32							shaderGroupHandleSize,
																										 deUint32							shaderGroupBaseAlignment,
																										 de::MovePtr<BufferWithMemory>&		raygenShaderBindingTable,
																										 de::MovePtr<BufferWithMemory>&		hitShaderBindingTable,
																										 de::MovePtr<BufferWithMemory>&		missShaderBindingTable,
																										 de::MovePtr<BufferWithMemory>&		callableShaderBindingTable,
																										 VkStridedDeviceAddressRegionKHR&	raygenShaderBindingTableRegion,
																										 VkStridedDeviceAddressRegionKHR&	hitShaderBindingTableRegion,
																										 VkStridedDeviceAddressRegionKHR&	missShaderBindingTableRegion,
																										 VkStridedDeviceAddressRegionKHR&	callableShaderBindingTableRegion) override;
	bool															verifyImage							(BufferWithMemory*					resultBuffer,
																										 Context&							context,
																										 TestParams&						testParams) override;
	VkFormat														getResultImageFormat				() override;
	size_t															getResultImageFormatSize			() override;
	VkClearValue													getClearValue						() override;
};

std::vector<de::SharedPtr<BottomLevelAccelerationStructure> > SingleSquareConfiguration::initBottomAccelerationStructures (Context&			context,
																														   TestParams&		testParams)
{
	DE_UNREF(context);

	std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >	result;
	de::MovePtr<BottomLevelAccelerationStructure>					bottomLevelAccelerationStructure	= makeBottomLevelAccelerationStructure();
	bottomLevelAccelerationStructure->setGeometryCount(1);

	de::SharedPtr<RaytracedGeometryBase> geometry;
	if (testParams.bottomType == BTT_TRIANGLES)
	{
		tcu::Vec3 v0(1.0f, float(testParams.height) - 1.0f, 0.0f);
		tcu::Vec3 v1(1.0f, 1.0f, 0.0f);
		tcu::Vec3 v2(float(testParams.width) - 1.0f, float(testParams.height) - 1.0f, 0.0f);
		tcu::Vec3 v3(float(testParams.width) - 1.0f, 1.0f, 0.0f);

		geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_TRIANGLES_KHR, VK_FORMAT_R32G32B32_SFLOAT, VK_INDEX_TYPE_NONE_KHR);
		geometry->addVertex(v0);
		geometry->addVertex(v1);
		geometry->addVertex(v2);
		geometry->addVertex(v2);
		geometry->addVertex(v1);
		geometry->addVertex(v3);
	}
	else // testParams.bottomType != BTT_TRIANGLES
	{
		tcu::Vec3 v0(1.0f, 1.0f, -0.1f);
		tcu::Vec3 v1(float(testParams.width) - 1.0f, float(testParams.height) - 1.0f, 0.1f);

		geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_AABBS_KHR, VK_FORMAT_R32G32B32_SFLOAT, VK_INDEX_TYPE_NONE_KHR);
		geometry->addVertex(v0);
		geometry->addVertex(v1);
	}
	bottomLevelAccelerationStructure->addGeometry(geometry);

	result.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));

	return result;
}

de::MovePtr<TopLevelAccelerationStructure> SingleSquareConfiguration::initTopAccelerationStructure (Context&		context,
																									TestParams&		testParams,
																									std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >& bottomLevelAccelerationStructures)
{
	DE_UNREF(context);
	DE_UNREF(testParams);

	de::MovePtr<TopLevelAccelerationStructure>	result	= makeTopLevelAccelerationStructure();
	result->setInstanceCount(1);
	result->addInstance(bottomLevelAccelerationStructures[0]);

	return result;
}

void SingleSquareConfiguration::initRayTracingShaders (de::MovePtr<RayTracingPipeline>&		rayTracingPipeline,
													   Context&								context,
													   TestParams&							testParams)
{
	const DeviceInterface&						vkd						= context.getDeviceInterface();
	const VkDevice								device					= context.getDevice();

	const std::vector<std::vector<std::string>> shaderNames =
	{
		{	"rgen",	"isect_report",			"ahit",					"chit",	"miss" },
		{	"rgen",	"isect_pass_through",	"ahit",					"chit",	"miss" },
		{	"rgen",	"isect_report",			"ahit_pass_through",	"chit",	"miss" },
		{	"rgen",	"isect_report",			"ahit_ignore",			"chit",	"miss" },
		{	"rgen",	"isect_report",			"ahit_terminate",		"chit",	"miss" },
	};
	rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get(shaderNames[testParams.hitShaderTestType][0]), 0), 0);
	if(testParams.bottomType == BTT_AABBS)
		rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR,	createShaderModule(vkd, device, context.getBinaryCollection().get(shaderNames[testParams.hitShaderTestType][1]), 0), 1);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get(shaderNames[testParams.hitShaderTestType][2]), 0), 1);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	createShaderModule(vkd, device, context.getBinaryCollection().get(shaderNames[testParams.hitShaderTestType][3]), 0), 1);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR,			createShaderModule(vkd, device, context.getBinaryCollection().get(shaderNames[testParams.hitShaderTestType][4]), 0), 2);
}

void SingleSquareConfiguration::initShaderBindingTables (de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
														 Context&							context,
														 TestParams&						testParams,
														 VkPipeline							pipeline,
														 deUint32							shaderGroupHandleSize,
														 deUint32							shaderGroupBaseAlignment,
														 de::MovePtr<BufferWithMemory>&		raygenShaderBindingTable,
														 de::MovePtr<BufferWithMemory>&		hitShaderBindingTable,
														 de::MovePtr<BufferWithMemory>&		missShaderBindingTable,
														 de::MovePtr<BufferWithMemory>&		callableShaderBindingTable,
														 VkStridedDeviceAddressRegionKHR&	raygenShaderBindingTableRegion,
														 VkStridedDeviceAddressRegionKHR&	hitShaderBindingTableRegion,
														 VkStridedDeviceAddressRegionKHR&	missShaderBindingTableRegion,
														 VkStridedDeviceAddressRegionKHR&	callableShaderBindingTableRegion)
{
	DE_UNREF(testParams);
	DE_UNREF(callableShaderBindingTable);

	const DeviceInterface&	vkd			= context.getDeviceInterface();
	const VkDevice			device		= context.getDevice();
	Allocator&				allocator	= context.getDefaultAllocator();

	raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
	hitShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
	missShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, 1);

	raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
}

bool SingleSquareConfiguration::verifyImage (BufferWithMemory* resultBuffer, Context& context, TestParams& testParams)
{
	// create result image
	tcu::TextureFormat			imageFormat						= vk::mapVkFormat(getResultImageFormat());
	tcu::ConstPixelBufferAccess	resultAccess(imageFormat, testParams.width, testParams.height, 2, resultBuffer->getAllocation().getHostPtr());

	// create reference image
	std::vector<deUint32>		reference(testParams.width * testParams.height * 2);
	tcu::PixelBufferAccess		referenceAccess(imageFormat, testParams.width, testParams.height, 2, reference.data());

	// clear reference image with hit and miss values
	// Reference image has two layers:
	//   - ahit shader writes results to layer 0
	//   - chit shader writes results to layer 1
	//   - miss shader writes results to layer 0
	//   - rays that missed on layer 0 - should have value 0 on layer 1
	tcu::UVec4 missValue0 = tcu::UVec4(4, 0, 0, 0);
	tcu::UVec4 missValue1 = tcu::UVec4(0, 0, 0, 0);
	tcu::UVec4 hitValue0, hitValue1;
	switch (testParams.hitShaderTestType)
	{
		case HSTT_ISECT_REPORT_INTERSECTION:
			hitValue0	= tcu::UVec4(1, 0, 0, 0);	// ahit returns 1
			hitValue1	= tcu::UVec4(3, 0, 0, 0);	// chit returns 3
			break;
		case HSTT_ISECT_DONT_REPORT_INTERSECTION:
			hitValue0	= missValue0;				// no ahit - results should report miss value
			hitValue1	= missValue1;				// no chit - results should report miss value
			break;
		case HSTT_AHIT_PASS_THROUGH:
			hitValue0	= tcu::UVec4(0, 0, 0, 0);	// empty ahit shader. Initial value from rgen written to result
			hitValue1	= tcu::UVec4(3, 0, 0, 0);	// chit returns 3
			break;
		case HSTT_AHIT_IGNORE_INTERSECTION:
			hitValue0	= missValue0;				// ahit ignores intersection - results should report miss value
			hitValue1	= missValue1;				// no chit - results should report miss value
			break;
		case HSTT_AHIT_TERMINATE_RAY:
			hitValue0	= tcu::UVec4(1, 0, 0, 0);	// ahit should return 1. If it returned 2, then terminateRayEXT did not terminate ahit shader
			hitValue1	= tcu::UVec4(3, 0, 0, 0);	// chit returns 3
			break;
		default:
			TCU_THROW(InternalError, "Wrong shader test type");
	}

	tcu::clear(referenceAccess, missValue0);
	for (deUint32 y = 0; y < testParams.width; ++y)
	for (deUint32 x = 0; x < testParams.height; ++x)
		referenceAccess.setPixel(missValue1, x, y, 1);

	for (deUint32 y = 1; y < testParams.width - 1; ++y)
	for (deUint32 x = 1; x < testParams.height - 1; ++x)
	{
		referenceAccess.setPixel(hitValue0, x, y, 0);
		referenceAccess.setPixel(hitValue1, x, y, 1);
	}

	// compare result and reference
	return tcu::intThresholdCompare(context.getTestContext().getLog(), "Result comparison", "", referenceAccess, resultAccess, tcu::UVec4(0), tcu::COMPARE_LOG_RESULT);
}

VkFormat SingleSquareConfiguration::getResultImageFormat ()
{
	return VK_FORMAT_R32_UINT;
}

size_t SingleSquareConfiguration::getResultImageFormatSize ()
{
	return sizeof(deUint32);
}

VkClearValue SingleSquareConfiguration::getClearValue ()
{
	return makeClearValueColorU32(0xFF, 0u, 0u, 0u);
}

class TraversalControlTestCase : public TestCase
{
	public:
							TraversalControlTestCase					(tcu::TestContext& context, const char* name, const char* desc, const TestParams data);
							~TraversalControlTestCase					(void);

	virtual void			checkSupport								(Context& context) const;
	virtual	void			initPrograms								(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance								(Context& context) const;
private:
	TestParams				m_data;
};

class TraversalControlTestInstance : public TestInstance
{
public:
																	TraversalControlTestInstance	(Context& context, const TestParams& data);
																	~TraversalControlTestInstance	(void);
	tcu::TestStatus													iterate									(void);

protected:
	de::MovePtr<BufferWithMemory>									runTest									();
private:
	TestParams														m_data;
};

TraversalControlTestCase::TraversalControlTestCase (tcu::TestContext& context, const char* name, const char* desc, const TestParams data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

TraversalControlTestCase::~TraversalControlTestCase (void)
{
}

void TraversalControlTestCase::checkSupport (Context& context) const
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

void TraversalControlTestCase::initPrograms (SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions	buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadEXT uvec4 hitValue;\n"
			"layout(r32ui, set = 0, binding = 0) uniform uimage3D result;\n"
			"layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
			"\n"
			"void main()\n"
			"{\n"
			"  float tmin     = 0.0;\n"
			"  float tmax     = 1.0;\n"
			"  vec3  origin   = vec3(float(gl_LaunchIDEXT.x) + 0.5f, float(gl_LaunchIDEXT.y) + 0.5f, 0.5f);\n"
			"  vec3  direct   = vec3(0.0, 0.0, -1.0);\n"
			"  hitValue       = uvec4(0,0,0,0);\n"
			"  traceRayEXT(topLevelAS, 0, 0xFF, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
			"  imageStore(result, ivec3(gl_LaunchIDEXT.xy, 0), uvec4(hitValue.x, 0, 0, 0));\n"
			"  imageStore(result, ivec3(gl_LaunchIDEXT.xy, 1), uvec4(hitValue.y, 0, 0, 0));\n"
			"}\n";
		programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"hitAttributeEXT uvec4 hitAttribute;\n"
			"void main()\n"
			"{\n"
			"  hitAttribute = uvec4(0,0,0,0);\n"
			"  reportIntersectionEXT(0.5f, 0);\n"
			"}\n";

		programCollection.glslSources.add("isect_report") << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"void main()\n"
			"{\n"
			"}\n";

		programCollection.glslSources.add("isect_pass_through") << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
			"void main()\n"
			"{\n"
			"  hitValue.x = 1;\n"
			"}\n";

		programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"void main()\n"
			"{\n"
			"}\n";

		programCollection.glslSources.add("ahit_pass_through") << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
			"void main()\n"
			"{\n"
			"  hitValue.x = 1;\n"
			"  ignoreIntersectionEXT;\n"
			"  hitValue.x = 2;\n"
			"}\n";

		programCollection.glslSources.add("ahit_ignore") << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
			"void main()\n"
			"{\n"
			"  hitValue.x = 1;\n"
			"  terminateRayEXT;\n"
			"  hitValue.x = 2;\n"
			"}\n";

		programCollection.glslSources.add("ahit_terminate") << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
			"void main()\n"
			"{\n"
			"  hitValue.y = 3;\n"
			"}\n";

		programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
			"void main()\n"
			"{\n"
			"  hitValue.x = 4;\n"
			"}\n";

		programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}
}

TestInstance* TraversalControlTestCase::createInstance (Context& context) const
{
	return new TraversalControlTestInstance(context, m_data);
}

TraversalControlTestInstance::TraversalControlTestInstance (Context& context, const TestParams& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
{
}

TraversalControlTestInstance::~TraversalControlTestInstance (void)
{
}

de::MovePtr<BufferWithMemory> TraversalControlTestInstance::runTest ()
{
	const InstanceInterface&			vki									= m_context.getInstanceInterface();
	const DeviceInterface&				vkd									= m_context.getDeviceInterface();
	const VkDevice						device								= m_context.getDevice();
	const VkPhysicalDevice				physicalDevice						= m_context.getPhysicalDevice();
	const deUint32						queueFamilyIndex					= m_context.getUniversalQueueFamilyIndex();
	const VkQueue						queue								= m_context.getUniversalQueue();
	Allocator&							allocator							= m_context.getDefaultAllocator();

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

	de::MovePtr<RayTracingPipeline>		rayTracingPipeline					= de::newMovePtr<RayTracingPipeline>();
	m_data.testConfiguration->initRayTracingShaders(rayTracingPipeline, m_context, m_data);
	Move<VkPipeline>					pipeline							= rayTracingPipeline->createPipeline(vkd, device, *pipelineLayout);

	de::MovePtr<BufferWithMemory>		raygenShaderBindingTable;
	de::MovePtr<BufferWithMemory>		hitShaderBindingTable;
	de::MovePtr<BufferWithMemory>		missShaderBindingTable;
	de::MovePtr<BufferWithMemory>		callableShaderBindingTable;
	VkStridedDeviceAddressRegionKHR		raygenShaderBindingTableRegion;
	VkStridedDeviceAddressRegionKHR		hitShaderBindingTableRegion;
	VkStridedDeviceAddressRegionKHR		missShaderBindingTableRegion;
	VkStridedDeviceAddressRegionKHR		callableShaderBindingTableRegion;
	m_data.testConfiguration->initShaderBindingTables(rayTracingPipeline, m_context, m_data, *pipeline, getShaderGroupHandleSize(vki, physicalDevice), getShaderGroupBaseAlignment(vki, physicalDevice), raygenShaderBindingTable, hitShaderBindingTable, missShaderBindingTable, callableShaderBindingTable, raygenShaderBindingTableRegion, hitShaderBindingTableRegion, missShaderBindingTableRegion, callableShaderBindingTableRegion);

	const VkFormat						imageFormat							= m_data.testConfiguration->getResultImageFormat();
	const VkImageCreateInfo				imageCreateInfo						= makeImageCreateInfo(m_data.width, m_data.height, 2, imageFormat);
	const VkImageSubresourceRange		imageSubresourceRange				= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const de::MovePtr<ImageWithMemory>	image								= de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
	const Move<VkImageView>				imageView							= makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_3D, imageFormat, imageSubresourceRange);

	const VkBufferCreateInfo			resultBufferCreateInfo				= makeBufferCreateInfo(m_data.width * m_data.height * 2 * m_data.testConfiguration->getResultImageFormatSize(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkImageSubresourceLayers		resultBufferImageSubresourceLayers	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const VkBufferImageCopy				resultBufferImageRegion				= makeBufferImageCopy(makeExtent3D(m_data.width, m_data.height, 2), resultBufferImageSubresourceLayers);
	de::MovePtr<BufferWithMemory>		resultBuffer						= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));

	const VkDescriptorImageInfo			descriptorImageInfo					= makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);

	const Move<VkCommandPool>			cmdPool								= createCommandPool(vkd, device, 0, queueFamilyIndex);
	const Move<VkCommandBuffer>			cmdBuffer							= allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >	bottomLevelAccelerationStructures;
	de::MovePtr<TopLevelAccelerationStructure>						topLevelAccelerationStructure;

	beginCommandBuffer(vkd, *cmdBuffer, 0u);
	{
		const VkImageMemoryBarrier			preImageBarrier						= makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT,
																					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
																					**image, imageSubresourceRange);
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preImageBarrier);

		const VkClearValue					clearValue							= m_data.testConfiguration->getClearValue();
		vkd.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1, &imageSubresourceRange);

		const VkImageMemoryBarrier			postImageBarrier					= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
																					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
																					**image, imageSubresourceRange);
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, &postImageBarrier);

		bottomLevelAccelerationStructures										= m_data.testConfiguration->initBottomAccelerationStructures(m_context, m_data);
		for (auto& blas : bottomLevelAccelerationStructures)
			blas->createAndBuild(vkd, device, *cmdBuffer, allocator);
		topLevelAccelerationStructure											= m_data.testConfiguration->initTopAccelerationStructure(m_context, m_data, bottomLevelAccelerationStructures);
		topLevelAccelerationStructure->createAndBuild(vkd, device, *cmdBuffer, allocator);

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

		const VkMemoryBarrier							postTraceMemoryBarrier					= makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);

		vkd.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **resultBuffer, 1u, &resultBufferImageRegion);
	}
	endCommandBuffer(vkd, *cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

	invalidateMappedMemoryRange(vkd, device, resultBuffer->getAllocation().getMemory(), resultBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);

	return resultBuffer;
}

tcu::TestStatus TraversalControlTestInstance::iterate (void)
{
	// run test using arrays of pointers
	const de::MovePtr<BufferWithMemory>	buffer		= runTest();

	if (!m_data.testConfiguration->verifyImage(buffer.get(), m_context, m_data))
		return tcu::TestStatus::fail("Fail");
	return tcu::TestStatus::pass("Pass");
}

}	// anonymous

tcu::TestCaseGroup*	createTraversalControlTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "traversal_control", "Tests verifying traversal control in RT hit shaders"));

	struct HitShaderTestTypeData
	{
		HitShaderTestType						shaderTestType;
		bool									onlyAabbTest;
		const char*								name;
	} hitShaderTestTypes[] =
	{
		{ HSTT_ISECT_REPORT_INTERSECTION,		true,	"isect_report_intersection"			},
		{ HSTT_ISECT_DONT_REPORT_INTERSECTION,	true,	"isect_dont_report_intersection"	},
		{ HSTT_AHIT_PASS_THROUGH,				false,	"ahit_pass_through"					},
		{ HSTT_AHIT_IGNORE_INTERSECTION,		false,	"ahit_ignore_intersection"			},
		{ HSTT_AHIT_TERMINATE_RAY,				false,	"ahit_terminate_ray"				},
	};

	struct
	{
		BottomTestType										testType;
		const char*											name;
	} bottomTestTypes[] =
	{
		{ BTT_TRIANGLES,									"triangles" },
		{ BTT_AABBS,										"aabbs" },
	};


	for (size_t shaderTestNdx = 0; shaderTestNdx < DE_LENGTH_OF_ARRAY(hitShaderTestTypes); ++shaderTestNdx)
	{
		de::MovePtr<tcu::TestCaseGroup> testTypeGroup(new tcu::TestCaseGroup(group->getTestContext(), hitShaderTestTypes[shaderTestNdx].name, ""));

		for (size_t testTypeNdx = 0; testTypeNdx < DE_LENGTH_OF_ARRAY(bottomTestTypes); ++testTypeNdx)
		{
			if (hitShaderTestTypes[shaderTestNdx].onlyAabbTest && bottomTestTypes[testTypeNdx].testType != BTT_AABBS)
				continue;

			TestParams testParams
			{
				TEST_WIDTH,
				TEST_HEIGHT,
				hitShaderTestTypes[shaderTestNdx].shaderTestType,
				bottomTestTypes[testTypeNdx].testType,
				de::SharedPtr<TestConfiguration>(new SingleSquareConfiguration())
			};
			testTypeGroup->addChild(new TraversalControlTestCase(group->getTestContext(), bottomTestTypes[testTypeNdx].name, "", testParams));
		}
		group->addChild(testTypeGroup.release());

	}

	return group.release();
}

}	// RayTracing

}	// vkt
