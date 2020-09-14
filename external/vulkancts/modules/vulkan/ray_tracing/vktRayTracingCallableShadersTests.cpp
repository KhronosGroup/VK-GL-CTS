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
 * \brief Ray Tracing Callable Shader tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingCallableShadersTests.hpp"

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

enum CallableShaderTestType
{
	CSTT_RGEN_CALL		= 0,
	CSTT_RGEN_CALL_CALL	= 1,
	CSTT_HIT_CALL		= 2,
	CSTT_RGEN_MULTICALL	= 3,
	CSTT_COUNT
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
	CallableShaderTestType				callableShaderTestType;
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

VkImageCreateInfo makeImageCreateInfo (deUint32 width, deUint32 height, VkFormat format)
{
	const VkImageCreateInfo			imageCreateInfo			=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,																// VkStructureType			sType;
		DE_NULL,																							// const void*				pNext;
		(VkImageCreateFlags)0u,																				// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,																					// VkImageType				imageType;
		format,																								// VkFormat					format;
		makeExtent3D(width, height, 1),																		// VkExtent3D				extent;
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

	tcu::Vec3 v0(1.0, float(testParams.height) - 1.0f, 0.0);
	tcu::Vec3 v1(1.0, 1.0, 0.0);
	tcu::Vec3 v2(float(testParams.width) - 1.0f, float(testParams.height) - 1.0f, 0.0);
	tcu::Vec3 v3(float(testParams.width) - 1.0f, 1.0, 0.0);

	std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >	result;
	de::MovePtr<BottomLevelAccelerationStructure>					bottomLevelAccelerationStructure	= makeBottomLevelAccelerationStructure();
	bottomLevelAccelerationStructure->setGeometryCount(1);

	de::SharedPtr<RaytracedGeometryBase> geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_TRIANGLES_KHR, VK_FORMAT_R32G32B32_SFLOAT, VK_INDEX_TYPE_NONE_KHR);
	geometry->addVertex(v0);
	geometry->addVertex(v1);
	geometry->addVertex(v2);
	geometry->addVertex(v2);
	geometry->addVertex(v1);
	geometry->addVertex(v3);
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

	de::MovePtr<TopLevelAccelerationStructure>	result						= makeTopLevelAccelerationStructure();
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

	switch (testParams.callableShaderTestType)
	{
		case CSTT_RGEN_CALL:
		{
			rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get("rgen_call"), 0), 0);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	createShaderModule(vkd, device, context.getBinaryCollection().get("chit"), 0), 1);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR,			createShaderModule(vkd, device, context.getBinaryCollection().get("miss"), 0), 2);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get("call_0"), 0), 3);
			break;
		}
		case CSTT_RGEN_CALL_CALL:
		{
			rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get("rgen_call"), 0), 0);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	createShaderModule(vkd, device, context.getBinaryCollection().get("chit"), 0), 1);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR,			createShaderModule(vkd, device, context.getBinaryCollection().get("miss"), 0), 2);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get("call_call"), 0), 3);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get("call_0"), 0), 4);
			break;
		}
		case CSTT_HIT_CALL:
		{
			rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get("rgen"), 0), 0);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	createShaderModule(vkd, device, context.getBinaryCollection().get("chit_call"), 0), 1);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR,			createShaderModule(vkd, device, context.getBinaryCollection().get("miss_call"), 0), 2);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get("call_0"), 0), 3);
			break;
		}
		case CSTT_RGEN_MULTICALL:
		{
			rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get("rgen_multicall"), 0), 0);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	createShaderModule(vkd, device, context.getBinaryCollection().get("chit"), 0), 1);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR,			createShaderModule(vkd, device, context.getBinaryCollection().get("miss"), 0), 2);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get("call_0"), 0), 3);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get("call_1"), 0), 4);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get("call_2"), 0), 5);
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get("call_3"), 0), 6);
			break;
		}
		default:
			TCU_THROW(InternalError, "Wrong shader test type");
	}
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
	const DeviceInterface&						vkd							= context.getDeviceInterface();
	const VkDevice								device						= context.getDevice();
	Allocator&									allocator					= context.getDefaultAllocator();

	switch (testParams.callableShaderTestType)
	{
		case CSTT_RGEN_CALL:
		{
			raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
			hitShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
			missShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, 1);
			callableShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 3, 1);

			raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, callableShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			break;
		}
		case CSTT_RGEN_CALL_CALL:
		{
			raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
			hitShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
			missShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, 1);
			callableShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 3, 2);

			raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, callableShaderBindingTable->get(), 0), shaderGroupHandleSize, 2*shaderGroupHandleSize);
			break;
		}
		case CSTT_HIT_CALL:
		{
			raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
			hitShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
			missShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, 1);
			callableShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 3, 1);

			raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, callableShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			break;
		}
		case CSTT_RGEN_MULTICALL:
		{
			raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
			hitShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
			missShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, 1);
			callableShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 3, 4);

			raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, callableShaderBindingTable->get(), 0), shaderGroupHandleSize, 4*shaderGroupHandleSize);
			break;
		}
		default:
			TCU_THROW(InternalError, "Wrong shader test type");
	}
}

bool SingleSquareConfiguration::verifyImage (BufferWithMemory* resultBuffer, Context& context, TestParams& testParams)
{
	// create result image
	tcu::TextureFormat			imageFormat						= vk::mapVkFormat(getResultImageFormat());
	tcu::ConstPixelBufferAccess	resultAccess(imageFormat, testParams.width, testParams.height, 1, resultBuffer->getAllocation().getHostPtr());

	// create reference image
	std::vector<deUint32>		reference(testParams.width * testParams.height);
	tcu::PixelBufferAccess		referenceAccess(imageFormat, testParams.width, testParams.height, 1, reference.data());

	tcu::UVec4 missValue, hitValue;

	// clear reference image with hit and miss values ( hit works only for tests calling traceRayEXT in rgen shader )
	switch (testParams.callableShaderTestType)
	{
		case CSTT_RGEN_CALL:
			missValue	= tcu::UVec4(1, 0, 0, 0);
			hitValue	= tcu::UVec4(1, 0, 0, 0);
			break;
		case CSTT_RGEN_CALL_CALL:
			missValue	= tcu::UVec4(1, 0, 0, 0);
			hitValue	= tcu::UVec4(1, 0, 0, 0);
			break;
		case CSTT_HIT_CALL:
			missValue	= tcu::UVec4(1, 0, 0, 0);
			hitValue	= tcu::UVec4(2, 0, 0, 0);
			break;
		case CSTT_RGEN_MULTICALL:
			missValue	= tcu::UVec4(16, 0, 0, 0);
			hitValue	= tcu::UVec4(16, 0, 0, 0);
			break;
		default:
			TCU_THROW(InternalError, "Wrong shader test type");
	}

	tcu::clear(referenceAccess, missValue);
	for (deUint32 y = 1; y < testParams.width - 1; ++y)
	for (deUint32 x = 1; x < testParams.height - 1; ++x)
		referenceAccess.setPixel(hitValue, x, y);

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

class CallableShaderTestCase : public TestCase
{
	public:
							CallableShaderTestCase			(tcu::TestContext& context, const char* name, const char* desc, const TestParams data);
							~CallableShaderTestCase			(void);

	virtual void			checkSupport								(Context& context) const;
	virtual	void			initPrograms								(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance								(Context& context) const;
private:
	TestParams				m_data;
};

class CallableShaderTestInstance : public TestInstance
{
public:
																	CallableShaderTestInstance	(Context& context, const TestParams& data);
																	~CallableShaderTestInstance	(void);
	tcu::TestStatus													iterate									(void);

protected:
	de::MovePtr<BufferWithMemory>									runTest									();
private:
	TestParams														m_data;
};

CallableShaderTestCase::CallableShaderTestCase (tcu::TestContext& context, const char* name, const char* desc, const TestParams data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

CallableShaderTestCase::~CallableShaderTestCase (void)
{
}

void CallableShaderTestCase::checkSupport (Context& context) const
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

void CallableShaderTestCase::initPrograms (SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions	buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadEXT uvec4 hitValue;\n"
			"layout(r32ui, set = 0, binding = 0) uniform uimage2D result;\n"
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
			"  imageStore(result, ivec2(gl_LaunchIDEXT.xy), hitValue);\n"
			"}\n";
		programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) callableDataEXT uvec4 value;\n"
			"layout(r32ui, set = 0, binding = 0) uniform uimage2D result;\n"
			"layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
			"\n"
			"void main()\n"
			"{\n"
			"  executeCallableEXT(0, 0);\n"
			"  imageStore(result, ivec2(gl_LaunchIDEXT.xy), value);\n"
			"}\n";
		programCollection.glslSources.add("rgen_call") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"struct CallValue\n"
			"{\n"
			"  ivec4 a;\n"
			"  vec4  b;\n"
			"};\n"
			"layout(location = 0) callableDataEXT uvec4 value0;\n"
			"layout(location = 1) callableDataEXT uint value1;\n"
			"layout(location = 2) callableDataEXT CallValue value2;\n"
			"layout(location = 4) callableDataEXT vec3 value3;\n"
			"layout(r32ui, set = 0, binding = 0) uniform uimage2D result;\n"
			"layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
			"\n"
			"void main()\n"
			"{\n"
			"  executeCallableEXT(0, 0);\n"
			"  executeCallableEXT(1, 1);\n"
			"  executeCallableEXT(2, 2);\n"
			"  executeCallableEXT(3, 4);\n"
			"  uint resultValue = value0.x + value1 + value2.a.x * uint(floor(value2.b.y)) + uint(floor(value3.z));\n"
			"  imageStore(result, ivec2(gl_LaunchIDEXT.xy), uvec4(resultValue, 0, 0, 0));\n"
			"}\n";
		programCollection.glslSources.add("rgen_multicall") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
			"void main()\n"
			"{\n"
			"  hitValue = uvec4(1,0,0,1);\n"
			"}\n";

		programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) callableDataEXT uvec4 value;\n"
			"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
			"void main()\n"
			"{\n"
			"  executeCallableEXT(0, 0);\n"
			"  hitValue = value;\n"
			"  hitValue.x = hitValue.x + 1;\n"
			"}\n";

		programCollection.glslSources.add("chit_call") << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
			"void main()\n"
			"{\n"
			"  hitValue = uvec4(0,0,0,1);\n"
			"}\n";

		programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) callableDataEXT uvec4 value;\n"
			"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
			"void main()\n"
			"{\n"
			"  executeCallableEXT(0, 0);\n"
			"  hitValue = value;\n"
			"}\n";

		programCollection.glslSources.add("miss_call") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	std::vector<std::string> callableDataDefinition =
	{
		"layout(location = 0) callableDataInEXT uvec4 result;\n",
		"layout(location = 1) callableDataInEXT uint result;\n",
		"struct CallValue\n{\n  ivec4 a;\n  vec4  b;\n};\nlayout(location = 2) callableDataInEXT CallValue result;\n",
		"layout(location = 4) callableDataInEXT vec3 result;\n"
	};

	std::vector<std::string> callableDataComputation =
	{
		"  result = uvec4(1,0,0,1);\n",
		"  result = 2;\n",
		"  result.a = ivec4(3,0,0,1);\n  result.b = vec4(1.0, 3.2, 0.0, 1);\n",
		"  result = vec3(0.0, 0.0, 4.3);\n",
	};

	for (deUint32 idx = 0; idx < callableDataDefinition.size(); ++idx)
	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			<< callableDataDefinition[idx] <<
			"void main()\n"
			"{\n"
			<< callableDataComputation[idx] <<
			"}\n";
		std::stringstream csname;
		csname << "call_" << idx;

		programCollection.glslSources.add(csname.str()) << glu::CallableSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) callableDataInEXT uvec4 result;\n"
			"layout(location = 1) callableDataEXT uvec4 info;\n"
			"void main()\n"
			"{\n"
			"  executeCallableEXT(1, 1);\n"
			"  result = info;\n"
			"}\n";

		programCollection.glslSources.add("call_call") << glu::CallableSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}
}

TestInstance* CallableShaderTestCase::createInstance (Context& context) const
{
	return new CallableShaderTestInstance(context, m_data);
}

CallableShaderTestInstance::CallableShaderTestInstance (Context& context, const TestParams& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
{
}

CallableShaderTestInstance::~CallableShaderTestInstance (void)
{
}

de::MovePtr<BufferWithMemory> CallableShaderTestInstance::runTest ()
{
	const InstanceInterface&			vki									= m_context.getInstanceInterface();
	const DeviceInterface&				vkd									= m_context.getDeviceInterface();
	const VkDevice						device								= m_context.getDevice();
	const VkPhysicalDevice				physicalDevice						= m_context.getPhysicalDevice();
	const deUint32						queueFamilyIndex					= m_context.getUniversalQueueFamilyIndex();
	const VkQueue						queue								= m_context.getUniversalQueue();
	Allocator&							allocator							= m_context.getDefaultAllocator();
	const deUint32						pixelCount							= m_data.width * m_data.height * 1;

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
	const VkImageCreateInfo				imageCreateInfo						= makeImageCreateInfo(m_data.width, m_data.height, imageFormat);
	const VkImageSubresourceRange		imageSubresourceRange				= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const de::MovePtr<ImageWithMemory>	image								= de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
	const Move<VkImageView>				imageView							= makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_2D, imageFormat, imageSubresourceRange);

	const VkBufferCreateInfo			resultBufferCreateInfo				= makeBufferCreateInfo(pixelCount*m_data.testConfiguration->getResultImageFormatSize(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkImageSubresourceLayers		resultBufferImageSubresourceLayers	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const VkBufferImageCopy				resultBufferImageRegion				= makeBufferImageCopy(makeExtent3D(m_data.width, m_data.height, 1), resultBufferImageSubresourceLayers);
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

tcu::TestStatus CallableShaderTestInstance::iterate (void)
{
	// run test using arrays of pointers
	const de::MovePtr<BufferWithMemory>	buffer		= runTest();

	if (!m_data.testConfiguration->verifyImage(buffer.get(), m_context, m_data))
		return tcu::TestStatus::fail("Fail");
	return tcu::TestStatus::pass("Pass");
}

}	// anonymous

tcu::TestCaseGroup*	createCallableShadersTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "callable_shader", "Tests veryfying callable shaders"));

	struct CallableShaderTestTypeData
	{
		CallableShaderTestType					shaderTestType;
		const char*								name;
	} callableShaderTestTypes[] =
	{
		{ CSTT_RGEN_CALL,		"rgen_call"			},
		{ CSTT_RGEN_CALL_CALL,	"rgen_call_call"	},
		{ CSTT_HIT_CALL,		"hit_call"			},
		{ CSTT_RGEN_MULTICALL,	"rgen_multicall"	},
	};

	for (size_t shaderTestNdx = 0; shaderTestNdx < DE_LENGTH_OF_ARRAY(callableShaderTestTypes); ++shaderTestNdx)
	{
		TestParams testParams
		{
			TEST_WIDTH,
			TEST_HEIGHT,
			callableShaderTestTypes[shaderTestNdx].shaderTestType,
			de::SharedPtr<TestConfiguration>(new SingleSquareConfiguration())
		};
		group->addChild(new CallableShaderTestCase(group->getTestContext(), callableShaderTestTypes[shaderTestNdx].name, "", testParams));
	}

	return group.release();
}

}	// RayTracing

}	// vkt
