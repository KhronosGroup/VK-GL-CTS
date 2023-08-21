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
#include "tcuSurface.hpp"
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
    glu::ShaderType						invokingShader;
	bool								multipleInvocations;
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
		const VkMemoryBarrier							postCopyMemoryBarrier					= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);

		vkd.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **resultBuffer, 1u, &resultBufferImageRegion);

		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &postCopyMemoryBarrier);
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

constexpr deUint32 callableDataUintLoc = 0;
constexpr deUint32 callableDataFloatLoc = 1;
constexpr deUint32 callableDataUintOutLoc = 2;

struct CallableBuffer0
{
    deUint32 base;
    deUint32 shift;
    deUint32 offset;
    deUint32 multiplier;
};

struct CallableBuffer1
{
	float numerator;
	float denomenator;
	deUint32 power;
};

struct Ray
{
	Ray() : o(0.0f), tmin(0.0f), d(0.0f), tmax(0.0f){}
	Ray(const tcu::Vec3& io, float imin, const tcu::Vec3& id, float imax): o(io), tmin(imin), d(id), tmax(imax){}
	tcu::Vec3 o;
	float tmin;
	tcu::Vec3 d;
	float tmax;
};

constexpr float MAX_T_VALUE = 1000.0f;

void AddVertexLayers(std::vector<tcu::Vec3>* pVerts, deUint32 newLayers)
{
	size_t vertsPerLayer = pVerts->size();
	size_t totalLayers = 1 + newLayers;

	pVerts->reserve(pVerts->size() * totalLayers);

	for (size_t layer = 0; layer < newLayers; ++layer)
	{
		for (size_t vert = 0; vert < vertsPerLayer; ++vert)
		{
			bool flippedLayer = (layer % 2) == 0;
			tcu::Vec3 stage = (*pVerts)[flippedLayer ? (vertsPerLayer - vert - 1) : vert];
			++stage.z();

			pVerts->push_back(stage);
		}
	}
}

bool compareFloat(float actual, float expected)
{
	constexpr float eps = 0.01f;
	bool success = true;

	if (abs(expected - actual) > eps)
	{
		success = false;
	}

	return success;
}

class InvokeCallableShaderTestCase : public TestCase
{
	public:
							InvokeCallableShaderTestCase			(tcu::TestContext& context, const char* name, const char* desc, const TestParams& data);
							~InvokeCallableShaderTestCase			(void);

	virtual void			checkSupport								(Context& context) const;
	virtual	void			initPrograms								(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance								(Context& context) const;
private:
	TestParams			params;
};

class InvokeCallableShaderTestInstance : public TestInstance
{
public:
																	InvokeCallableShaderTestInstance	(Context& context, const TestParams& data);
																	~InvokeCallableShaderTestInstance	(void);
	tcu::TestStatus													iterate									(void);

private:
	TestParams													params;
};

InvokeCallableShaderTestCase::InvokeCallableShaderTestCase (tcu::TestContext& context, const char* name, const char* desc, const TestParams& data)
	: vkt::TestCase	(context, name, desc)
	, params		(data)
{
}

InvokeCallableShaderTestCase::~InvokeCallableShaderTestCase (void)
{
}

void InvokeCallableShaderTestCase::checkSupport (Context& context) const
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

//resultData:
// x - value0
// y - value1
// z - value2
// w - closestT
bool verifyResultData(const tcu::Vec4* resultData, deUint32 index, bool hit, const TestParams& params)
{
	bool success = true;

	float refValue0 = 0.0f;
	float refValue1 = 0.0f;
	float refValue2 = 0.0f;

	if (hit)
	{
		switch (params.invokingShader)
		{
		case glu::SHADERTYPE_RAYGEN :
		case glu::SHADERTYPE_CLOSEST_HIT:
		case glu::SHADERTYPE_CALLABLE:
			refValue0 = 133.0f;
			break;
		case glu::SHADERTYPE_MISS:
			break;
		default:
			TCU_THROW(InternalError, "Wrong shader invoking type");
			break;
		}

		if (params.multipleInvocations)
		{
			switch (params.invokingShader)
			{
			case glu::SHADERTYPE_RAYGEN:
				refValue1 = 17.64f;
				refValue2 = 35.28f;
				break;
			case glu::SHADERTYPE_CLOSEST_HIT:
				refValue1 = 17.64f;
				refValue2 = index < 4 ? 35.28f : 8.82f;
				break;
			case glu::SHADERTYPE_CALLABLE:
				refValue1 = 17.64f;
				refValue2 = index < 6 ? 35.28f : 8.82f;
				break;
			case glu::SHADERTYPE_MISS:
				break;
			default:
				TCU_THROW(InternalError, "Wrong shader invoking type");
				break;
			}
		}

		if (resultData->w() != 2.0f)
		{
			success = false;
		}
	}

	if (!hit)
	{
		switch (params.invokingShader)
		{
		case glu::SHADERTYPE_RAYGEN:
		case glu::SHADERTYPE_MISS:
		case glu::SHADERTYPE_CALLABLE:
			refValue0 = 133.0f;
			break;
		case glu::SHADERTYPE_CLOSEST_HIT:
			break;
		default:
			TCU_THROW(InternalError, "Wrong shader invoking type");
			break;
		}

		if (params.multipleInvocations)
		{
			switch (params.invokingShader)
			{
			case glu::SHADERTYPE_RAYGEN:
				refValue1 = 17.64f;
				refValue2 = 8.82f;
				break;
			case glu::SHADERTYPE_MISS:
				refValue1 = 17.64f;
				refValue2 = index < 10 ? 35.28f : 8.82f;
				break;
			case glu::SHADERTYPE_CALLABLE:
				refValue1 = 17.64f;
				refValue2 = index < 6 ? 35.28f : 8.82f;
				break;
			case glu::SHADERTYPE_CLOSEST_HIT:
				break;
			default:
				TCU_THROW(InternalError, "Wrong shader invoking type");
				break;
			}
		}

		if (resultData->w() != MAX_T_VALUE)
		{
			success = false;
		}
	}

	if ((!compareFloat(resultData->x(), refValue0)) ||
		(!compareFloat(resultData->y(), refValue1)) ||
		(!compareFloat(resultData->z(), refValue2)))
	{
		success = false;
	}

	return success;
}

std::string getRayGenSource(bool invokeCallable, bool multiInvoke)
{
	std::ostringstream src;
	src <<
		"struct Payload { uint lastShader; float closestT; };\n"
		"layout(location = 0) rayPayloadEXT Payload payload;\n";

	if (invokeCallable)
	{
		src <<
			"#define CALLABLE_DATA_UINT_LOC " << callableDataUintLoc << "\n"
			"layout(location = CALLABLE_DATA_UINT_LOC) callableDataEXT uint callableDataUint;\n";

		if (multiInvoke)
		{
			src <<
				"#define CALLABLE_DATA_FLOAT_LOC " << callableDataFloatLoc << "\n"
				"layout(location = CALLABLE_DATA_FLOAT_LOC) callableDataEXT float callableDataFloat;\n";
		}
	}

	src <<
		"void main() {\n"
		"   uint index = launchIndex();\n"
		"   Ray ray = rays[index];\n"
	    "   results[index].value0 = 0;\n"
		"   results[index].value1 = 0;\n"
		"   results[index].value2 = 0;\n";

	if (invokeCallable)
	{
		src <<
			"   callableDataUint = " << "0" << ";\n"
			"   executeCallableEXT(0, CALLABLE_DATA_UINT_LOC);\n"
			"   results[index].value0 = float(callableDataUint);\n";

		if (multiInvoke)
		{
			src <<
				"   callableDataFloat = 0.0;\n"
				"   executeCallableEXT(1, CALLABLE_DATA_FLOAT_LOC);\n"
				"   results[index].value1 = callableDataFloat;\n";
		}
	}

	src <<
		"   payload.lastShader = " << glu::SHADERTYPE_RAYGEN << ";\n"
		"   payload.closestT = " << MAX_T_VALUE << ";\n"
		"   traceRayEXT(scene, 0x0, 0xff, 0, 0, 0, ray.pos, " << "ray.tmin" << ", ray.dir, ray.tmax, 0);\n";

	if (invokeCallable && multiInvoke)
	{
		src <<
			"   executeCallableEXT(payload.lastShader == " << glu::SHADERTYPE_CLOSEST_HIT << " ? 1 : 2, CALLABLE_DATA_FLOAT_LOC);\n"
			"   results[index].value2 = callableDataFloat;\n";
	}

	src <<
		"   results[index].closestT = payload.closestT;\n"
		"}";

	return src.str();
}

std::string getClosestHitSource(bool invokeCallable, bool multiInvoke)
{
	std::ostringstream src;
	src <<
		"struct Payload { uint lastShader; float closestT; };\n"
		"layout(location = 0) rayPayloadInEXT Payload payload;\n";

	if (invokeCallable)
	{
		src <<
			"#define CALLABLE_DATA_UINT_LOC " << callableDataUintLoc << "\n"
			"layout(location = CALLABLE_DATA_UINT_LOC) callableDataEXT uint callableDataUint;\n";

		if (multiInvoke)
		{
			src <<
				"#define CALLABLE_DATA_FLOAT_LOC " << callableDataFloatLoc << "\n"
				"layout(location = CALLABLE_DATA_FLOAT_LOC) callableDataEXT float callableDataFloat;\n";
		}
	}

	src <<
		"void main() {\n"
		"   payload.lastShader = " << glu::SHADERTYPE_CLOSEST_HIT << ";\n"
		"   payload.closestT = gl_HitTEXT;\n";

	if (invokeCallable)
	{
		src <<
			"   uint index = launchIndex();\n"
			"   callableDataUint = 0;\n"
			"   executeCallableEXT(0, CALLABLE_DATA_UINT_LOC);\n"
			"   results[index].value0 = float(callableDataUint);\n";

		if (multiInvoke)
		{
			src <<
				"   callableDataFloat = 0.0;\n"
				"   executeCallableEXT(1, CALLABLE_DATA_FLOAT_LOC);\n"
				"   results[index].value1 = callableDataFloat;\n"
				"   executeCallableEXT(index < 4 ? 1 : 2, CALLABLE_DATA_FLOAT_LOC);\n"
				"   results[index].value2 = callableDataFloat;\n";
		}
	}

	src <<
		"}";

	return src.str();
}

std::string getMissSource(bool invokeCallable, bool multiInvoke)
{
	std::ostringstream src;
	src <<
		"struct Payload { uint lastShader; float closestT; };\n"
		"layout(location = 0) rayPayloadInEXT Payload payload;\n";

	if (invokeCallable)
	{
		src <<
			"#define CALLABLE_DATA_UINT_LOC " << callableDataUintLoc << "\n"
			"layout(location = CALLABLE_DATA_UINT_LOC) callableDataEXT uint callableDataUint;\n";

		if (multiInvoke)
		{
			src <<
				"#define CALLABLE_DATA_FLOAT_LOC " << callableDataFloatLoc << "\n"
				"layout(location = CALLABLE_DATA_FLOAT_LOC) callableDataEXT float callableDataFloat;\n";
		}
	}

	src <<
		"void main() {\n"
		"   payload.lastShader = " << glu::SHADERTYPE_MISS << ";\n";

	if (invokeCallable)
	{
		src <<
			"   uint index = launchIndex();\n"
			"   callableDataUint = 0;\n"
			"   executeCallableEXT(0, CALLABLE_DATA_UINT_LOC);\n"
			"   results[index].value0 = float(callableDataUint);\n";

		if (multiInvoke)
		{
			src <<
				"   callableDataFloat = 0.0;\n"
				"   executeCallableEXT(1, CALLABLE_DATA_FLOAT_LOC);\n"
				"   results[index].value1 = callableDataFloat;\n"
				"   executeCallableEXT(index < 10 ? 1 : 2, CALLABLE_DATA_FLOAT_LOC);\n"
				"   results[index].value2 = callableDataFloat;\n";
		}
	}

	src <<
		"}";

	return src.str();
}

std::string getCallableSource(bool invokeCallable, bool multiInvoke)
{
	std::ostringstream src;
	src <<
		"#define CALLABLE_DATA_UINT_LOC " << callableDataUintLoc << "\n"
		"layout(location = CALLABLE_DATA_UINT_LOC) callableDataInEXT uint callableDataUintIn;\n";

	if (invokeCallable)
	{
		src << "#define CALLABLE_DATA_UINT_OUT_LOC " << callableDataUintOutLoc << "\n"
			<< "layout(location = CALLABLE_DATA_UINT_OUT_LOC) callableDataEXT uint callableDataUint;\n";

		if (multiInvoke)
		{
			src <<
				"#define CALLABLE_DATA_FLOAT_LOC " << callableDataFloatLoc << "\n"
				"layout(location = CALLABLE_DATA_FLOAT_LOC) callableDataEXT float callableDataFloat;\n";
		}
	}

	src <<
		"void main() {\n";

	if (invokeCallable)
	{
		src <<
			"   uint index = launchIndex();\n"
			"   callableDataUint = 0;\n"
			"   executeCallableEXT(1, CALLABLE_DATA_UINT_OUT_LOC);\n"
			"   callableDataUintIn = callableDataUint;\n";

		if (multiInvoke)
		{
			src <<
				"   callableDataFloat = 0.0;\n"
				"   executeCallableEXT(2, CALLABLE_DATA_FLOAT_LOC);\n"
				"   results[index].value1 = callableDataFloat;\n"
				"   executeCallableEXT(index < 6 ? 2 : 3, CALLABLE_DATA_FLOAT_LOC);\n"
				"   results[index].value2 = callableDataFloat;\n";
		}
	}

	src <<
		"}";

	return src.str();
}

constexpr deUint32 DefaultResultBinding = 0;
constexpr deUint32 DefaultSceneBinding = 1;
constexpr deUint32 DefaultRaysBinding = 2;

enum ShaderSourceFlag
{
	DEFINE_RAY = 0x1,
	DEFINE_RESULT_BUFFER = 0x2,
	DEFINE_SCENE = 0x4,
	DEFINE_RAY_BUFFER = 0x8,
	DEFINE_SIMPLE_BINDINGS = DEFINE_RESULT_BUFFER | DEFINE_SCENE | DEFINE_RAY_BUFFER
};

static inline std::string generateShaderSource(const char* body, const char* resultType = "", deUint32 flags = 0, const char* prefix = "")
{
	std::ostringstream src;
	src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n";

	src << "#extension GL_EXT_ray_tracing : enable\n";

	src << prefix << "\n";

	if (flags & DEFINE_SIMPLE_BINDINGS)
		flags |= DEFINE_RAY_BUFFER;

	if (flags & DEFINE_RAY_BUFFER)
		flags |= DEFINE_RAY;

	if (flags & DEFINE_RAY)
	{
		src << "struct Ray { vec3 pos; float tmin; vec3 dir; float tmax; };\n";
	}

	if (flags & DEFINE_RESULT_BUFFER)
		src << "layout(std430, set = 0, binding = " << DefaultResultBinding << ") buffer Results { " << resultType << " results[]; };\n";

	if (flags & DEFINE_SCENE)
	{
		src << "layout(set = 0, binding = " << DefaultSceneBinding << ") uniform accelerationStructureEXT scene;\n";
	}

	if (flags & DEFINE_RAY_BUFFER)
		src << "layout(std430, set = 0, binding = " << DefaultRaysBinding << ") buffer Rays { Ray rays[]; };\n";

	src << "uint launchIndex() { return gl_LaunchIDEXT.z*gl_LaunchSizeEXT.x*gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y*gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x; }\n";

	src << body;

	return src.str();
}

template<typename T> inline void addShaderSource(SourceCollections& programCollection, const char* identifier,
											const char* body, const char* resultType = "", deUint32 flags = 0,
                                            const char* prefix = "", deUint32 validatorOptions = 0U)
{
	std::string text = generateShaderSource(body, resultType, flags, prefix);

	const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, validatorOptions, true);
	programCollection.glslSources.add(identifier) << T(text) << buildOptions;
}



void InvokeCallableShaderTestCase::initPrograms (SourceCollections& programCollection) const
{
	addShaderSource<glu::RaygenSource>(programCollection, "build-raygen",
		getRayGenSource(false, false).c_str(), "Result", DEFINE_RAY_BUFFER | DEFINE_SIMPLE_BINDINGS,
		"struct Result { float value0; float value1; float value2; float closestT;};");

	addShaderSource<glu::RaygenSource>(programCollection, "build-raygen-invoke-callable",
		getRayGenSource(true, false).c_str(), "Result", DEFINE_RAY_BUFFER | DEFINE_SIMPLE_BINDINGS,
		"struct Result { float value0; float value1; float value2; float closestT;};");

	addShaderSource<glu::ClosestHitSource>(programCollection, "build-closesthit",
		getClosestHitSource(false, false).c_str(), "", 0, "");

	addShaderSource<glu::MissSource>(programCollection, "build-miss",
		getMissSource(false, false).c_str(), "", 0, "");

	const std::string RAY_PAYLOAD		    = "rayPayloadEXT";
	const std::string TRACE_RAY			    = "traceRayEXT";
	const std::string RAY_PAYLOAD_IN	    = "rayPayloadInEXT";
	const std::string HIT_ATTRIBUTE		    = "hitAttributeEXT";
    const std::string REPORT_INTERSECTION   = "reportIntersectionEXT";

    const std::string SHADER_RECORD         = "shaderRecordEXT";
	const std::string CALLABLE_DATA_IN	    = "callableDataInEXT";
	const std::string CALLABLE_DATA		    = "callableDataEXT";
	const std::string EXECUTE_CALLABE	    = "executeCallableEXT";

	std::ostringstream src;
    src <<
		"#define CALLABLE_DATA_UINT_LOC " << callableDataUintLoc << "\n"
		"layout(location = CALLABLE_DATA_UINT_LOC) callableDataInEXT uint callableDataUint;\n"
        "layout(" << SHADER_RECORD << ") buffer callableBuffer\n"
        "{\n"
        "   uint base;\n"
        "   uint shift;\n"
        "   uint offset;\n"
        "   uint multiplier;\n"
        "};\n"
        "void main() {\n"
        "   callableDataUint += ((base << shift) + offset) * multiplier;\n"
        "}";

    addShaderSource<glu::CallableSource>(programCollection, "build-callable-0", src.str().c_str(),
        "", 0, "");

	if (params.multipleInvocations)
	{
		switch (params.invokingShader)
		{
		case glu::SHADERTYPE_RAYGEN:
			addShaderSource<glu::RaygenSource>(programCollection, "build-raygen-invoke-callable-multi",
				getRayGenSource(true, true).c_str(), "Result", DEFINE_RAY_BUFFER | DEFINE_SIMPLE_BINDINGS,
				"struct Result { float value0; float value1; float value2; float closestT;};");

			break;
		case glu::SHADERTYPE_CLOSEST_HIT:
			addShaderSource<glu::ClosestHitSource>(programCollection, "build-closesthit-invoke-callable-multi",
				getClosestHitSource(true, true).c_str(), "Result", DEFINE_RESULT_BUFFER,
				"struct Result { float value0; float value1; float value2; float closestT;};");

			break;
		case glu::SHADERTYPE_MISS:
			addShaderSource<glu::MissSource>(programCollection, "build-miss-invoke-callable-multi",
				getMissSource(true, true).c_str(), "Result", DEFINE_RESULT_BUFFER,
				"struct Result { float value0; float value1; float value2; float closestT;};");

			break;
		case glu::SHADERTYPE_CALLABLE:
			addShaderSource<glu::CallableSource>(programCollection, "build-callable-invoke-callable-multi",
				getCallableSource(true, true).c_str(), "Result", DEFINE_RESULT_BUFFER,
				"struct Result { float value0; float value1; float value2; float closestT;};");

			break;
		default:
			TCU_THROW(InternalError, "Wrong shader invoking type");
			break;
		}

		src.str(std::string());
		src <<
			"#define CALLABLE_DATA_FLOAT_LOC " << callableDataFloatLoc << "\n"
			"layout(location = CALLABLE_DATA_FLOAT_LOC) callableDataInEXT float callableDataFloat;\n"
			"layout(" << SHADER_RECORD << ") buffer callableBuffer\n"
			"{\n"
			"   float numerator;\n"
			"   float denomenator;\n"
			"   uint power;\n"
			"   uint reserved;\n"
			"};\n"
			"void main() {\n"
			"   float base = numerator / denomenator;\n"
			"   float result = 1;\n"
			"   for (uint i = 0; i < power; ++i)\n"
			"   {\n"
			"      result *= base;\n"
			"   }\n"
			"   callableDataFloat += result;\n"
			"}";

		addShaderSource<glu::CallableSource>(programCollection, "build-callable-1", src.str().c_str(),
            "", 0, "");

		src.str(std::string());
		src <<
			"#define CALLABLE_DATA_FLOAT_LOC " << callableDataFloatLoc << "\n"
			"layout(location = CALLABLE_DATA_FLOAT_LOC) callableDataInEXT float callableDataFloat;\n"
			"void main() {\n"
			"   callableDataFloat /= 2.0f;\n"
			"}";

		addShaderSource<glu::CallableSource>(programCollection, "build-callable-2", src.str().c_str(),
            "", 0, "");
	}
	else
	{
		switch (params.invokingShader)
		{
		case glu::SHADERTYPE_RAYGEN:
			// Always defined since it's needed to invoke callable shaders that invoke other callable shaders

			break;
		case glu::SHADERTYPE_CLOSEST_HIT:
			addShaderSource<glu::ClosestHitSource>(programCollection, "build-closesthit-invoke-callable",
				getClosestHitSource(true, false).c_str(), "Result", DEFINE_RESULT_BUFFER,
				"struct Result { float value0; float value1; float value2; float closestT;};");

			break;
		case glu::SHADERTYPE_MISS:
			addShaderSource<glu::MissSource>(programCollection, "build-miss-invoke-callable",
				getMissSource(true, false).c_str(), "Result", DEFINE_RESULT_BUFFER,
				"struct Result { float value0; float value1; float value2; float closestT;};");

			break;
		case glu::SHADERTYPE_CALLABLE:
			addShaderSource<glu::CallableSource>(programCollection, "build-callable-invoke-callable",
				getCallableSource(true, false).c_str(), "Result", DEFINE_RESULT_BUFFER,
				"struct Result { float value0; float value1; float value2; float closestT;};");

			break;
		default:
			TCU_THROW(InternalError, "Wrong shader invoking type");
			break;
		}
	}
}

TestInstance* InvokeCallableShaderTestCase::createInstance (Context& context) const
{
	return new InvokeCallableShaderTestInstance(context, params);
}

InvokeCallableShaderTestInstance::InvokeCallableShaderTestInstance (Context& context, const TestParams& data)
	: vkt::TestInstance		(context)
	, params				(data)
{
}

InvokeCallableShaderTestInstance::~InvokeCallableShaderTestInstance (void)
{
}

tcu::TestStatus InvokeCallableShaderTestInstance::iterate()
{
	const VkDevice device = m_context.getDevice();
	const DeviceInterface& vk = m_context.getDeviceInterface();
	const InstanceInterface& vki = m_context.getInstanceInterface();
	Allocator& allocator = m_context.getDefaultAllocator();
	de::MovePtr<RayTracingProperties> rayTracingProperties = makeRayTracingProperties(vki, m_context.getPhysicalDevice());

	vk::Move<VkDescriptorPool>		descriptorPool;
	vk::Move<VkDescriptorSetLayout> descriptorSetLayout;
	std::vector<vk::Move<VkDescriptorSet>>		descriptorSet;
	vk::Move<VkPipelineLayout>		pipelineLayout;

	vk::DescriptorPoolBuilder descriptorPoolBuilder;

	deUint32 storageBufCount = 0;

	const VkDescriptorType accelType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

	storageBufCount += 1;

	storageBufCount += 1;

	descriptorPoolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, storageBufCount);

	descriptorPoolBuilder.addType(accelType, 1);

	descriptorPool = descriptorPoolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);

	vk::DescriptorSetLayoutBuilder setLayoutBuilder;

	const deUint32 AllStages = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
							   VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
							   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR;

	setLayoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, AllStages);
	setLayoutBuilder.addSingleBinding(accelType, AllStages);
	setLayoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, AllStages);

	descriptorSetLayout = setLayoutBuilder.build(vk, device);

	const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,		// VkStructureType				sType;
		DE_NULL,											// const void*					pNext;
		*descriptorPool,									// VkDescriptorPool				descriptorPool;
		1u,													// deUint32						setLayoutCount;
		&descriptorSetLayout.get()							// const VkDescriptorSetLayout*	pSetLayouts;
	};

	descriptorSet.push_back(allocateDescriptorSet(vk, device, &descriptorSetAllocateInfo));

	const VkPipelineLayoutCreateInfo pipelineLayoutInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
		DE_NULL,											// const void*					pNext;
		(VkPipelineLayoutCreateFlags)0,						// VkPipelineLayoutCreateFlags	flags;
		1u,													// deUint32						setLayoutCount;
		&descriptorSetLayout.get(),							// const VkDescriptorSetLayout*	pSetLayouts;
		0u,													// deUint32						pushConstantRangeCount;
		nullptr,											// const VkPushConstantRange*	pPushConstantRanges;
	};

	pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutInfo);

	std::string raygenId = "build-raygen";
	std::string missId = "build-miss";
	std::string closestHitId = "build-closesthit";
	std::vector<std::string> callableIds;

	switch (params.invokingShader)
	{
	case glu::SHADERTYPE_RAYGEN:
	{
		raygenId.append("-invoke-callable");

		if (params.multipleInvocations)
		{
			raygenId.append("-multi");
		}
		break;
	}
	case glu::SHADERTYPE_MISS:
	{
		missId.append("-invoke-callable");

		if (params.multipleInvocations)
		{
			missId.append("-multi");
		}
		break;
	}
	case glu::SHADERTYPE_CLOSEST_HIT:
	{
		closestHitId.append("-invoke-callable");

		if (params.multipleInvocations)
		{
			closestHitId.append("-multi");
		}
		break;
	}
	case glu::SHADERTYPE_CALLABLE:
	{
		raygenId.append("-invoke-callable");
		std::string callableId("build-callable-invoke-callable");

		if (params.multipleInvocations)
		{
			callableId.append("-multi");
		}

		callableIds.push_back(callableId);
		break;
	}
	default:
	{
		TCU_THROW(InternalError, "Wrong shader invoking type");
		break;
	}
	}

	callableIds.push_back("build-callable-0");
	if (params.multipleInvocations)
	{
		callableIds.push_back("build-callable-1");
		callableIds.push_back("build-callable-2");
	}

	de::MovePtr<RayTracingPipeline>	rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();
	rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,		createShaderModule(vk, device, m_context.getBinaryCollection().get(raygenId.c_str()), 0), 0);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR,			createShaderModule(vk, device, m_context.getBinaryCollection().get(missId.c_str()), 0), 1);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	createShaderModule(vk, device, m_context.getBinaryCollection().get(closestHitId.c_str()), 0), 2);
	deUint32 callableGroup = 3;
	for (auto& callableId : callableIds)
	{
		rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR,	createShaderModule(vk, device, m_context.getBinaryCollection().get(callableId.c_str()), 0), callableGroup);
		++callableGroup;
	}
	Move<VkPipeline> pipeline = rayTracingPipeline->createPipeline(vk, device, *pipelineLayout);

	CallableBuffer0 callableBuffer0 = { 1, 4, 3, 7 };
	CallableBuffer1 callableBuffer1 = { 10.5, 2.5, 2 };

	size_t MaxBufferSize = std::max(sizeof(callableBuffer0), sizeof(callableBuffer1));
	deUint32 shaderGroupHandleSize = rayTracingProperties->getShaderGroupHandleSize();
	deUint32 shaderGroupBaseAlignment = rayTracingProperties->getShaderGroupBaseAlignment();
	size_t shaderStride = deAlign32(shaderGroupHandleSize + (deUint32)MaxBufferSize, shaderGroupHandleSize);

	de::MovePtr<BufferWithMemory> raygenShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
	de::MovePtr<BufferWithMemory> missShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);
	de::MovePtr<BufferWithMemory> hitShaderBindingTable					= rayTracingPipeline->createShaderBindingTable(vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, 1);
	de::MovePtr<BufferWithMemory> callableShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vk, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 3, (deUint32)callableIds.size(),
																													   0U, 0U, MemoryRequirement::Any, 0U, 0U, (deUint32)MaxBufferSize, nullptr, true);

	VkStridedDeviceAddressRegionKHR raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	VkStridedDeviceAddressRegionKHR missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	VkStridedDeviceAddressRegionKHR hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	VkStridedDeviceAddressRegionKHR callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vk, device, callableShaderBindingTable->get(), 0), shaderStride, shaderGroupHandleSize);

	size_t callableCount = 0;

	if (params.invokingShader == glu::SHADERTYPE_CALLABLE)
	{
		callableCount++;
	}

	deMemcpy((deUint8*)callableShaderBindingTable->getAllocation().getHostPtr() +
		(shaderStride * (callableCount)) + shaderGroupHandleSize,
		&callableBuffer0,
		sizeof(CallableBuffer0));
	callableCount++;

	if (params.multipleInvocations)
	{
		deMemcpy((deUint8*)callableShaderBindingTable->getAllocation().getHostPtr() +
			(shaderStride * (callableCount)) + shaderGroupHandleSize,
			&callableBuffer1,
			sizeof(CallableBuffer1));
		callableCount++;
	}

	flushMappedMemoryRange(vk, device, callableShaderBindingTable->getAllocation().getMemory(), callableShaderBindingTable->getAllocation().getOffset(), VK_WHOLE_SIZE);

	//                 {I}
	// (-2,1) (-1,1)  (0,1)  (1,1)  (2,1)
	//    X------X------X------X------X
	//    |\     |\     |\     |\     |
	//    | \ {B}| \ {D}| \ {F}| \ {H}|
	// {K}|  \   |  \   |  \   |  \   |{L}
	//    |   \  |   \  |   \  |   \  |
	//    |{A} \ |{C} \ |{E} \ |{G} \ |
	//    |     \|     \|     \|     \|
	//    X------X------X------X------X
	// (-2,-1)(-1,-1) (0,-1) (1,-1) (2,-1)
	//                 {J}
	//
	// A, B, E, and F are initially opaque
	// A and C are forced opaque
	// E and G are forced non-opaque

	std::vector<Ray> rays = {
		Ray{ tcu::Vec3(-1.67f, -0.33f, 0.0f), 0.0f, tcu::Vec3(0.0f, 0.0f, 1.0f), MAX_T_VALUE }, // {A}
		Ray{ tcu::Vec3(-1.33f,  0.33f, 0.0f), 0.0f, tcu::Vec3(0.0f, 0.0f, 1.0f), MAX_T_VALUE }, // {B}
		Ray{ tcu::Vec3(-0.67f, -0.33f, 0.0f), 0.0f, tcu::Vec3(0.0f, 0.0f, 1.0f), MAX_T_VALUE }, // {C}
		Ray{ tcu::Vec3(-0.33f,  0.33f, 0.0f), 0.0f, tcu::Vec3(0.0f, 0.0f, 1.0f), MAX_T_VALUE }, // {D}
		Ray{ tcu::Vec3( 0.33f, -0.33f, 0.0f), 0.0f, tcu::Vec3(0.0f, 0.0f, 1.0f), MAX_T_VALUE }, // {E}
		Ray{ tcu::Vec3( 0.67f,  0.33f, 0.0f), 0.0f, tcu::Vec3(0.0f, 0.0f, 1.0f), MAX_T_VALUE }, // {F}
		Ray{ tcu::Vec3( 1.33f, -0.33f, 0.0f), 0.0f, tcu::Vec3(0.0f, 0.0f, 1.0f), MAX_T_VALUE }, // {G}
		Ray{ tcu::Vec3( 1.67f,  0.33f, 0.0f), 0.0f, tcu::Vec3(0.0f, 0.0f, 1.0f), MAX_T_VALUE }, // {H}
		Ray{ tcu::Vec3( 0.0f,   1.01f, 0.0f), 0.0f, tcu::Vec3(0.0f, 0.0f, 1.0f), MAX_T_VALUE }, // {I}
		Ray{ tcu::Vec3( 0.0f,  -1.01f, 0.0f), 0.0f, tcu::Vec3(0.0f, 0.0f, 1.0f), MAX_T_VALUE }, // {J}
		Ray{ tcu::Vec3(-2.01f,  0.0f,  0.0f), 0.0f, tcu::Vec3(0.0f, 0.0f, 1.0f), MAX_T_VALUE }, // {K}
		Ray{ tcu::Vec3( 2.01f,  0.0f,  0.0f), 0.0f, tcu::Vec3(0.0f, 0.0f, 1.0f), MAX_T_VALUE }  // {L}
	};

	// B & F
	std::vector<tcu::Vec3> blas0VertsOpaque = {
		{ -2.0f,  1.0f, 2.0f },
		{ -1.0f, -1.0f, 2.0f },
		{ -1.0f,  1.0f, 2.0f },
		{  0.0f,  1.0f, 2.0f },
		{  1.0f, -1.0f, 2.0f },
		{  1.0f,  1.0f, 2.0f }
	};

	// D & H
	std::vector<tcu::Vec3> blas0VertsNoOpaque = {
		{ -1.0f,  1.0f, 2.0f },
		{  0.0f, -1.0f, 2.0f },
		{  0.0f,  1.0f, 2.0f },
		{  1.0f,  1.0f, 2.0f },
		{  2.0f, -1.0f, 2.0f },
		{  2.0f,  1.0f, 2.0f }
	};

	// A
	std::vector<tcu::Vec3> blas1VertsOpaque = {
		{ -2.0f,  1.0f, 2.0f },
		{ -2.0f, -1.0f, 2.0f },
		{ -1.0f, -1.0f, 2.0f }
	};

	// C
	std::vector<tcu::Vec3> blas1VertsNoOpaque = {
		{ -1.0f,  1.0f, 2.0f },
		{ -1.0f, -1.0f, 2.0f },
		{  0.0f, -1.0f, 2.0f }
	};

	// E
	std::vector<tcu::Vec3> blas2VertsOpaque = {
		{  0.0f,  1.0f, 2.0f },
		{  0.0f, -1.0f, 2.0f },
		{  1.0f, -1.0f, 2.0f }
	};

	// G
	std::vector<tcu::Vec3> blas2VertsNoOpaque = {
		{  1.0f,  1.0f, 2.0f },
		{  1.0f, -1.0f, 2.0f },
		{  2.0f, -1.0f, 2.0f }
	};

	AddVertexLayers(&blas0VertsOpaque, 1);
	AddVertexLayers(&blas0VertsNoOpaque, 1);
	AddVertexLayers(&blas1VertsOpaque, 1);
	AddVertexLayers(&blas1VertsNoOpaque, 1);
	AddVertexLayers(&blas2VertsOpaque, 1);
	AddVertexLayers(&blas2VertsNoOpaque, 1);

	std::vector<tcu::Vec3> verts;
	verts.reserve(
		blas0VertsOpaque.size() + blas0VertsNoOpaque.size() +
		blas1VertsOpaque.size() + blas1VertsNoOpaque.size() +
		blas2VertsOpaque.size() + blas2VertsNoOpaque.size());
	verts.insert(verts.end(), blas0VertsOpaque.begin(), blas0VertsOpaque.end());
	verts.insert(verts.end(), blas0VertsNoOpaque.begin(), blas0VertsNoOpaque.end());
	verts.insert(verts.end(), blas1VertsOpaque.begin(), blas1VertsOpaque.end());
	verts.insert(verts.end(), blas1VertsNoOpaque.begin(), blas1VertsNoOpaque.end());
	verts.insert(verts.end(), blas2VertsOpaque.begin(), blas2VertsOpaque.end());
	verts.insert(verts.end(), blas2VertsNoOpaque.begin(), blas2VertsNoOpaque.end());

	tcu::Surface resultImage(static_cast<int>(rays.size()), 1);

	const VkBufferCreateInfo			resultBufferCreateInfo			= makeBufferCreateInfo(rays.size() * sizeof(tcu::Vec4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	de::MovePtr<BufferWithMemory>		resultBuffer					= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vk, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));
	const VkDescriptorBufferInfo		resultDescriptorInfo			= makeDescriptorBufferInfo(resultBuffer->get(), 0, VK_WHOLE_SIZE);

	const VkBufferCreateInfo			rayBufferCreateInfo				= makeBufferCreateInfo(rays.size() * sizeof(Ray), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	de::MovePtr<BufferWithMemory>		rayBuffer						= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vk, device, allocator, rayBufferCreateInfo, MemoryRequirement::HostVisible));
	const VkDescriptorBufferInfo		rayDescriptorInfo				= makeDescriptorBufferInfo(rayBuffer->get(), 0, VK_WHOLE_SIZE);
	memcpy(rayBuffer->getAllocation().getHostPtr(), &rays[0], rays.size() * sizeof(Ray));
	flushMappedMemoryRange(vk, device, rayBuffer->getAllocation().getMemory(), rayBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);

	const Move<VkCommandPool>			cmdPool							= createCommandPool(vk, device, 0, m_context.getUniversalQueueFamilyIndex());
	const Move<VkCommandBuffer>			cmdBuffer						= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	beginCommandBuffer(vk, *cmdBuffer);

	de::SharedPtr<BottomLevelAccelerationStructure>	blas0 = de::SharedPtr<BottomLevelAccelerationStructure>(makeBottomLevelAccelerationStructure().release());
	blas0->setGeometryCount(2);
	blas0->addGeometry(blas0VertsOpaque, true, VK_GEOMETRY_OPAQUE_BIT_KHR);
	blas0->addGeometry(blas0VertsNoOpaque, true, 0U);
	blas0->createAndBuild(vk, device, *cmdBuffer, allocator);

	de::SharedPtr<BottomLevelAccelerationStructure>	blas1 = de::SharedPtr<BottomLevelAccelerationStructure>(makeBottomLevelAccelerationStructure().release());
	blas1->setGeometryCount(2);
	blas1->addGeometry(blas1VertsOpaque, true, VK_GEOMETRY_OPAQUE_BIT_KHR);
	blas1->addGeometry(blas1VertsNoOpaque, true, 0U);
	blas1->createAndBuild(vk, device, *cmdBuffer, allocator);

	de::SharedPtr<BottomLevelAccelerationStructure>	blas2 = de::SharedPtr<BottomLevelAccelerationStructure>(makeBottomLevelAccelerationStructure().release());
	blas2->setGeometryCount(2);
	blas2->addGeometry(blas2VertsOpaque, true, VK_GEOMETRY_OPAQUE_BIT_KHR);
	blas2->addGeometry(blas2VertsNoOpaque, true, 0U);
	blas2->createAndBuild(vk, device, *cmdBuffer, allocator);

	de::MovePtr<TopLevelAccelerationStructure>	tlas	= makeTopLevelAccelerationStructure();
	tlas->setInstanceCount(3);
	tlas->addInstance(blas0);
	tlas->addInstance(blas1);
	tlas->addInstance(blas2);
	tlas->createAndBuild(vk, device, *cmdBuffer, allocator);

	VkWriteDescriptorSetAccelerationStructureKHR	accelerationStructureWriteDescriptorSet	=
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	//  VkStructureType						sType;
		DE_NULL,															//  const void*							pNext;
		1u,																	//  deUint32							accelerationStructureCount;
		tlas->getPtr(),														//  const VkAccelerationStructureKHR*	pAccelerationStructures;
	};

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet[0], DescriptorSetUpdateBuilder::Location::binding(DefaultResultBinding), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultDescriptorInfo)
		.writeSingle(*descriptorSet[0], DescriptorSetUpdateBuilder::Location::binding(DefaultSceneBinding), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelerationStructureWriteDescriptorSet)
		.writeSingle(*descriptorSet[0], DescriptorSetUpdateBuilder::Location::binding(DefaultRaysBinding), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &rayDescriptorInfo)
		.update(vk, device);

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipeline);
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipelineLayout, 0, 1, &descriptorSet[0].get(), 0, DE_NULL);

	cmdTraceRays(vk,
		*cmdBuffer,
		&raygenShaderBindingTableRegion,
		&missShaderBindingTableRegion,
		&hitShaderBindingTableRegion,
		&callableShaderBindingTableRegion,
		static_cast<deUint32>(rays.size()), 1, 1);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *cmdBuffer);

	invalidateMappedMemoryRange(vk, device, resultBuffer->getAllocation().getMemory(), resultBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);

	//                 {I}
	// (-2,1) (-1,1)  (0,1)  (1,1)  (2,1)
	//    X------X------X------X------X
	//    |\     |\     |\     |\     |
	//    | \ {B}| \ {D}| \ {F}| \ {H}|
	// {K}|  \   |  \   |  \   |  \   |{L}
	//    |   \  |   \  |   \  |   \  |
	//    |{A} \ |{C} \ |{E} \ |{G} \ |
	//    |     \|     \|     \|     \|
	//    X------X------X------X------X
	// (-2,-1)(-1,-1) (0,-1) (1,-1) (2,-1)
	//                 {J}
	// A, B, E, and F are opaque
	// A and C are forced opaque
	// E and G are forced non-opaque

	std::vector<bool> hits = { true, true, true, true, true, true, true, true, false, false, false, false };
	std::vector<bool> opaques = { true, true, true, false, false, true, false, false, true, true, true, true };


	union
	{
		bool     mismatch[32];
		deUint32 mismatchAll;
	};
	mismatchAll = 0;

	tcu::Vec4* resultData = (tcu::Vec4*)resultBuffer->getAllocation().getHostPtr();

	for (int index = 0; index < resultImage.getWidth(); ++index)
	{
		if (verifyResultData(&resultData[index], index, hits[index], params))
		{
			resultImage.setPixel(index, 0, tcu::RGBA(255, 0, 0, 255));
		}
		else
		{
			mismatch[index] = true;
			resultImage.setPixel(index, 0, tcu::RGBA(0, 0, 0, 255));
		}
	}

	// Write Image
	m_context.getTestContext().getLog() << tcu::TestLog::ImageSet("Result of rendering", "Result of rendering")
	<< tcu::TestLog::Image("Result", "Result", resultImage)
	<< tcu::TestLog::EndImageSet;

	if (mismatchAll != 0)
		TCU_FAIL("Result data did not match expected output");

	return tcu::TestStatus::pass("pass");
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
			de::SharedPtr<TestConfiguration>(new SingleSquareConfiguration()),
			glu::SHADERTYPE_LAST,
			false
		};
		group->addChild(new CallableShaderTestCase(group->getTestContext(), callableShaderTestTypes[shaderTestNdx].name, "", testParams));
	}

    bool                multipleInvocations[]     = { false               , true };
    std::string         multipleInvocationsText[] = { "_single_invocation", "_multiple_invocations" };
    // Callable shaders cannot be called from any-hit shader per GLSL_NV_ray_tracing spec. Assuming same will hold for KHR version.
    glu::ShaderType     invokingShaders[]         = { glu::SHADERTYPE_RAYGEN, glu::SHADERTYPE_CALLABLE, glu::SHADERTYPE_CLOSEST_HIT, glu::SHADERTYPE_MISS };
    std::string         invokingShadersText[]     = { "_invoked_via_raygen" , "_invoked_via_callable" , "_invoked_via_closest_hit" , "_invoked_via_miss" };

    for (int j = 0; j < 4; ++j)
    {
		TestParams params;

        std::string name("callable_shader");

        params.invokingShader = invokingShaders[j];
        name.append(invokingShadersText[j]);

        for (int k = 0; k < 2; ++k)
        {
			std::string nameFull(name);

            params.multipleInvocations = multipleInvocations[k];
			nameFull.append(multipleInvocationsText[k]);

			group->addChild(new InvokeCallableShaderTestCase(group->getTestContext(), nameFull.c_str(), "", params));
        }
    }

	return group.release();
}

}	// RayTracing

}	// vkt
