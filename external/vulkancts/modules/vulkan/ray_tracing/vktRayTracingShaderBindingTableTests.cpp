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
 * \brief Ray Tracing Shader Binding Table tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingShaderBindingTableTests.hpp"

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

enum ShaderTestType
{
	STT_HIT		= 0,
	STT_MISS	= 1,
	STT_CALL	= 2,
	STT_COUNT	= 3
};

const deUint32			CHECKERBOARD_WIDTH			= 8;
const deUint32			CHECKERBOARD_HEIGHT			= 8;
const deUint32			HIT_GEOMETRY_COUNT			= 3;
const deUint32			HIT_INSTANCE_COUNT			= 1 + CHECKERBOARD_WIDTH * CHECKERBOARD_HEIGHT / ( 2 * HIT_GEOMETRY_COUNT );

const deUint32			MAX_SBT_RECORD_OFFSET		= 3;
const deUint32			MAX_HIT_SBT_RECORD_STRIDE	= HIT_GEOMETRY_COUNT + 1;
const deUint32			SBT_RANDOM_SEED				= 1410;

struct TestParams;

class TestConfiguration
{
public:
	virtual std::vector<de::SharedPtr<BottomLevelAccelerationStructure>>	initBottomAccelerationStructures	(Context&							context,
																												 TestParams&						testParams) = 0;
	virtual de::MovePtr<TopLevelAccelerationStructure>						initTopAccelerationStructure		(Context&							context,
																												 TestParams&						testParams,
																												 std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >&	bottomLevelAccelerationStructures) = 0;
	virtual de::MovePtr<BufferWithMemory>									initUniformBuffer					(Context&							context,
																												 TestParams&						testParams) = 0;
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
	ShaderTestType						shaderTestType;
	deUint32							sbtOffset;
	bool								shaderRecordPresent;
	deUint32							sbtRecordOffset;
	deUint32							sbtRecordOffsetPassedToTraceRay;
	deUint32							sbtRecordStride;
	deUint32							sbtRecordStridePassedToTraceRay;
	de::SharedPtr<TestConfiguration>	testConfiguration;

};

std::vector<deUint32> getShaderCounts ()
{
	std::vector<deUint32> shaderCount(STT_COUNT);
	shaderCount[STT_HIT]	= HIT_INSTANCE_COUNT + HIT_GEOMETRY_COUNT * MAX_HIT_SBT_RECORD_STRIDE + MAX_SBT_RECORD_OFFSET + 1;
	shaderCount[STT_MISS]	= MAX_SBT_RECORD_OFFSET + HIT_INSTANCE_COUNT + 1;
	shaderCount[STT_CALL]	= MAX_SBT_RECORD_OFFSET + HIT_INSTANCE_COUNT + 1;
	return shaderCount;
}

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

class CheckerboardConfiguration : public TestConfiguration
{
public:
	std::vector<de::SharedPtr<BottomLevelAccelerationStructure>>	initBottomAccelerationStructures	(Context&							context,
																										 TestParams&						testParams) override;
	de::MovePtr<TopLevelAccelerationStructure>						initTopAccelerationStructure		(Context&							context,
																										 TestParams&						testParams,
																										 std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >&	bottomLevelAccelerationStructures) override;
	de::MovePtr<BufferWithMemory>									initUniformBuffer					(Context&							context,
																										 TestParams&						testParams) override;
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

std::vector<de::SharedPtr<BottomLevelAccelerationStructure> > CheckerboardConfiguration::initBottomAccelerationStructures (Context&			context,
																														   TestParams&		testParams)
{
	DE_UNREF(context);

	std::vector<tcu::Vec3> corners;
	for (deUint32 y = 0; y < testParams.height; ++y)
	for (deUint32 x = 0; x < testParams.width; ++x)
	{
		if (((x + y) % 2) == 0)
			continue;
		corners.push_back(tcu::Vec3((float)x, (float)y, 0.0f));
	}

	de::Random	rnd(SBT_RANDOM_SEED);
	rnd.shuffle(begin(corners), end(corners));

	tcu::Vec3 v0(0.0, 1.0, 0.0);
	tcu::Vec3 v1(0.0, 0.0, 0.0);
	tcu::Vec3 v2(1.0, 1.0, 0.0);
	tcu::Vec3 v3(1.0, 0.0, 0.0);
	std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >	result;

	for (size_t cornerNdx = 0; cornerNdx < corners.size(); cornerNdx += HIT_GEOMETRY_COUNT)
	{
		de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure	= makeBottomLevelAccelerationStructure();
		size_t											geometryCount						= std::min(corners.size() - cornerNdx, size_t(HIT_GEOMETRY_COUNT));
		bottomLevelAccelerationStructure->setGeometryCount(geometryCount);
		for (size_t idx = cornerNdx; idx < cornerNdx + geometryCount; ++idx)
		{
			de::SharedPtr<RaytracedGeometryBase> geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_TRIANGLES_KHR, VK_FORMAT_R32G32B32_SFLOAT, VK_INDEX_TYPE_NONE_KHR);
			geometry->addVertex(corners[idx] + v0);
			geometry->addVertex(corners[idx] + v1);
			geometry->addVertex(corners[idx] + v2);
			geometry->addVertex(corners[idx] + v2);
			geometry->addVertex(corners[idx] + v1);
			geometry->addVertex(corners[idx] + v3);
			bottomLevelAccelerationStructure->addGeometry(geometry);
		}
		result.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));
	}
	return result;
}

de::MovePtr<TopLevelAccelerationStructure> CheckerboardConfiguration::initTopAccelerationStructure (Context&		context,
																									TestParams&		testParams,
																									std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >& bottomLevelAccelerationStructures)
{
	DE_UNREF(context);
	DE_UNREF(testParams);

	de::MovePtr<TopLevelAccelerationStructure>	result						= makeTopLevelAccelerationStructure();
	deUint32									instanceCount				= deUint32(bottomLevelAccelerationStructures.size());
	result->setInstanceCount(instanceCount);

	VkTransformMatrixKHR						identityMatrix				= { { { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f } } };
	for (deUint32 i = 0; i < instanceCount; ++i)
		result->addInstance(bottomLevelAccelerationStructures[i], identityMatrix, 0u, 0xFF, (testParams.shaderTestType == STT_MISS) ? 0 : i);

	return result;
}

de::MovePtr<BufferWithMemory> CheckerboardConfiguration::initUniformBuffer (Context&		context,
																			TestParams&		testParams)
{
	const DeviceInterface&						vkd			= context.getDeviceInterface();
	const VkDevice								device		= context.getDevice();
	Allocator&									allocator	= context.getDefaultAllocator();

	const VkBufferCreateInfo		uniformBufferCreateInfo	= makeBufferCreateInfo(sizeof(tcu::UVec4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	de::MovePtr<BufferWithMemory>	uniformBuffer			= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, uniformBufferCreateInfo, MemoryRequirement::HostVisible));
	tcu::UVec4						uniformValue;			// x = sbtRecordOffset, y = sbtRecordStride, z = missIndex
	switch (testParams.shaderTestType)
	{
		case STT_HIT:
		{
			uniformValue = tcu::UVec4(testParams.sbtRecordOffsetPassedToTraceRay, testParams.sbtRecordStride, 0, 0);
			break;
		}
		case STT_MISS:
		{
			uniformValue = tcu::UVec4(0, 0, testParams.sbtRecordOffsetPassedToTraceRay, 0);
			break;
		}
		case STT_CALL:
		{
			uniformValue = tcu::UVec4(testParams.sbtRecordOffsetPassedToTraceRay, testParams.sbtRecordStride, 0, 0);
			break;
		}
		default:
			TCU_THROW(InternalError, "Wrong shader test type");
	}
	deMemcpy(uniformBuffer->getAllocation().getHostPtr(), &uniformValue, sizeof(tcu::UVec4));
	flushMappedMemoryRange(vkd, device, uniformBuffer->getAllocation().getMemory(), uniformBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);

	return uniformBuffer;
}

void CheckerboardConfiguration::initRayTracingShaders (de::MovePtr<RayTracingPipeline>&		rayTracingPipeline,
													   Context&								context,
													   TestParams&							testParams)
{
	const DeviceInterface&						vkd						= context.getDeviceInterface();
	const VkDevice								device					= context.getDevice();

	std::vector<deUint32>						shaderCount				= getShaderCounts();

	switch (testParams.shaderTestType)
	{
		case STT_HIT:
		{
			if (testParams.shaderRecordPresent)
			{
				// shaders: rgen, chit_shaderRecord (N times), miss_0
				rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get("rgen"), 0), 0);
				for (deUint32 idx = 0; idx < shaderCount[STT_HIT]; ++idx)
					rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get("chit_shaderRecord"), 0), 1+idx);
				rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get("miss_0"), 0), 1 + shaderCount[STT_HIT]);
			}
			else
			{
				// shaders: rgen, chit_0 .. chit_N, miss_0
				rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get("rgen"), 0), 0);
				for (deUint32 idx = 0; idx < shaderCount[STT_HIT]; ++idx)
				{
					std::stringstream csname;
					csname << "chit_" << idx;
					rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get(csname.str()), 0), 1 + idx);
				}
				rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get("miss_0"), 0), 1 + shaderCount[STT_HIT]);
			}
			rayTracingPipeline->setMaxPayloadSize(16u);
			break;
		}
		case STT_MISS:
		{
			if (testParams.shaderRecordPresent)
			{
				// shaders: rgen, chit_0, miss_shaderRecord ( N times )
				rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get("rgen"), 0), 0);
				rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get("chit_0"), 0), 1);
				for (deUint32 idx = 0; idx < shaderCount[STT_MISS]; ++idx)
					rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get("miss_shaderRecord"), 0), 2 + idx);
			}
			else
			{
				// shaders: rgen, chit_0, miss_0 .. miss_N
				rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get("rgen"), 0), 0);
				rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get("chit_0"), 0), 1);
				for (deUint32 idx = 0; idx < shaderCount[STT_MISS]; ++idx)
				{
					std::stringstream csname;
					csname << "miss_" << idx;
					rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get(csname.str()), 0), 2 + idx);
				}
			}
			rayTracingPipeline->setMaxPayloadSize(16u);
			break;
		}
		case STT_CALL:
		{
			if (testParams.shaderRecordPresent)
			{
				// shaders: rgen, chit_call_0 .. chit_call_N, miss_0, call_shaderRecord ( N times )
				rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get("rgen"), 0), 0);
				for (deUint32 idx = 0; idx < shaderCount[STT_CALL]; ++idx)
				{
					std::stringstream csname;
					csname << "chit_call_" << idx;
					rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get(csname.str()), 0), 1 + idx);
				}
				rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get("miss_0"), 0), 1 + shaderCount[STT_CALL]);
				for (deUint32 idx = 0; idx < shaderCount[STT_CALL]; ++idx)
					rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get("call_shaderRecord"), 0), 2 + shaderCount[STT_CALL] + idx);
			}
			else
			{
				// shaders: rgen, chit_call_0 .. chit_call_N, miss_0, call_0 .. call_N
				rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get("rgen"), 0), 0);
				for (deUint32 idx = 0; idx < shaderCount[STT_CALL]; ++idx)
				{
					std::stringstream csname;
					csname << "chit_call_" << idx;
					rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get(csname.str()), 0), 1 + idx);
				}
				rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get("miss_0"), 0), 1 + shaderCount[STT_CALL]);
				for (deUint32 idx = 0; idx < shaderCount[STT_CALL]; ++idx)
				{
					std::stringstream csname;
					csname << "call_" << idx;
					rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get(csname.str()), 0), 2 + shaderCount[STT_CALL] + idx);
				}
			}
			rayTracingPipeline->setMaxPayloadSize(16u);
			break;
		}
		default:
			TCU_THROW(InternalError, "Wrong shader test type");
	}
}

void CheckerboardConfiguration::initShaderBindingTables (de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
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

	std::vector<deUint32>						shaderCount					= getShaderCounts();

	// shaderBindingTableOffset must be multiple of shaderGroupBaseAlignment
	deUint32									shaderBindingTableOffset	= testParams.sbtOffset * shaderGroupBaseAlignment;

	// ShaderRecordKHR size must be multiple of shaderGroupHandleSize
	deUint32									shaderRecordAlignedSize		= deAlign32(shaderGroupHandleSize + deUint32(sizeof(tcu::UVec4)), shaderGroupHandleSize);
	switch (testParams.shaderTestType)
	{
		case STT_HIT:
		{
			raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1 );
			if(testParams.shaderRecordPresent)
				hitShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, shaderCount[STT_HIT], 0u, 0u, MemoryRequirement::Any, 0u, shaderBindingTableOffset, sizeof(tcu::UVec4));
			else
				hitShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, shaderCount[STT_HIT], 0u, 0u, MemoryRequirement::Any, 0u, shaderBindingTableOffset);
			missShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1 + shaderCount[STT_HIT], 1 );

			raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			if (testParams.shaderRecordPresent)
				hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), shaderBindingTableOffset), shaderRecordAlignedSize, shaderCount[STT_HIT] * shaderRecordAlignedSize);
			else
				hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), shaderBindingTableOffset), shaderGroupHandleSize, shaderCount[STT_HIT] * shaderGroupHandleSize);
			missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

			// fill ShaderRecordKHR data
			if (testParams.shaderRecordPresent)
			{
				deUint8* hitAddressBegin = (deUint8*)hitShaderBindingTable->getAllocation().getHostPtr() + shaderBindingTableOffset;

				for (size_t idx = 0; idx < shaderCount[STT_HIT]; ++idx)
				{
					deUint8* shaderRecordAddress = hitAddressBegin + idx * shaderRecordAlignedSize + size_t(shaderGroupHandleSize);
					tcu::UVec4 shaderRecord(deUint32(idx), 0, 0, 0);
					deMemcpy(shaderRecordAddress, &shaderRecord, sizeof(tcu::UVec4));
				}

				flushMappedMemoryRange(vkd, device, hitShaderBindingTable->getAllocation().getMemory(), hitShaderBindingTable->getAllocation().getOffset(), VK_WHOLE_SIZE);
			}
			break;
		}
		case STT_MISS:
		{
			raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1 );
			hitShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1 );
			if (testParams.shaderRecordPresent)
				missShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, shaderCount[STT_MISS], 0u, 0u, MemoryRequirement::Any, 0u, shaderBindingTableOffset, sizeof(tcu::UVec4));
			else
				missShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, shaderCount[STT_MISS], 0u, 0u, MemoryRequirement::Any, 0u, shaderBindingTableOffset);

			raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0), 0, shaderGroupHandleSize);
			if (testParams.shaderRecordPresent)
				missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), shaderBindingTableOffset), shaderRecordAlignedSize, shaderCount[STT_MISS] * shaderRecordAlignedSize);
			else
				missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), shaderBindingTableOffset), shaderGroupHandleSize, shaderCount[STT_MISS] * shaderGroupHandleSize);
			callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

			if (testParams.shaderRecordPresent)
			{
				deUint8* missAddressBegin = (deUint8*)missShaderBindingTable->getAllocation().getHostPtr() + shaderBindingTableOffset;

				for (size_t idx = 0; idx < shaderCount[STT_MISS]; ++idx)
				{
					deUint8* shaderRecordAddress = missAddressBegin + idx * shaderRecordAlignedSize + size_t(shaderGroupHandleSize);
					tcu::UVec4 shaderRecord(deUint32(idx), 0, 0, 0);
					deMemcpy(shaderRecordAddress, &shaderRecord, sizeof(tcu::UVec4));
				}

				flushMappedMemoryRange(vkd, device, missShaderBindingTable->getAllocation().getMemory(), missShaderBindingTable->getAllocation().getOffset(), VK_WHOLE_SIZE);
			}
			break;
		}
		case STT_CALL:
		{
			raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1 );
			hitShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, shaderCount[STT_CALL]);
			missShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1 + shaderCount[STT_CALL], 1 );
			if (testParams.shaderRecordPresent)
				callableShaderBindingTable = rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2 + shaderCount[STT_CALL], shaderCount[STT_CALL], 0u, 0u, MemoryRequirement::Any, 0u, shaderBindingTableOffset, sizeof(tcu::UVec4));
			else
				callableShaderBindingTable = rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2 + shaderCount[STT_CALL], shaderCount[STT_CALL], 0u, 0u, MemoryRequirement::Any, 0u, shaderBindingTableOffset);

			raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderCount[STT_CALL] * shaderGroupHandleSize);
			missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			if (testParams.shaderRecordPresent)
				callableShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, callableShaderBindingTable->get(), shaderBindingTableOffset), shaderRecordAlignedSize, shaderCount[STT_CALL] * shaderRecordAlignedSize);
			else
				callableShaderBindingTableRegion = makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, callableShaderBindingTable->get(), shaderBindingTableOffset), shaderGroupHandleSize, shaderCount[STT_CALL] * shaderGroupHandleSize);

			if (testParams.shaderRecordPresent)
			{
				deUint8* callAddressBegin	= (deUint8*)callableShaderBindingTable->getAllocation().getHostPtr() + shaderBindingTableOffset;

				for (size_t idx = 0; idx < shaderCount[STT_CALL]; ++idx)
				{
					deUint8* shaderRecordAddress = callAddressBegin + idx * shaderRecordAlignedSize + size_t(shaderGroupHandleSize);
					tcu::UVec4 shaderRecord(deUint32(idx), 0, 0, 0);
					deMemcpy(shaderRecordAddress, &shaderRecord, sizeof(tcu::UVec4));
				}
				flushMappedMemoryRange(vkd, device, callableShaderBindingTable->getAllocation().getMemory(), callableShaderBindingTable->getAllocation().getOffset(), VK_WHOLE_SIZE);
			}
			break;
		}
		default:
			TCU_THROW(InternalError, "Wrong shader test type");
	}
}

bool CheckerboardConfiguration::verifyImage (BufferWithMemory* resultBuffer, Context& context, TestParams& testParams)
{
	// create result image
	tcu::TextureFormat			imageFormat						= vk::mapVkFormat(getResultImageFormat());
	tcu::ConstPixelBufferAccess	resultAccess(imageFormat, testParams.width, testParams.height, 1, resultBuffer->getAllocation().getHostPtr());

	// recreate geometry indices and instance offsets
	std::vector<tcu::UVec4> corners;
	for (deUint32 y = 0; y < testParams.height; ++y)
	for (deUint32 x = 0; x < testParams.width; ++x)
	{
		if (((x + y) % 2) == 0)
			continue;
		corners.push_back(tcu::UVec4(x, y, 0, 0));
	}
	de::Random					rnd(SBT_RANDOM_SEED);
	rnd.shuffle(begin(corners), end(corners));

	deUint32					instanceOffset	= 0;
	for (size_t cornerNdx = 0; cornerNdx < corners.size(); cornerNdx += HIT_GEOMETRY_COUNT, ++instanceOffset)
	{
		size_t											geometryCount = std::min(corners.size() - cornerNdx, size_t(HIT_GEOMETRY_COUNT));
		deUint32 geometryIndex = 0;
		for (size_t idx = cornerNdx; idx < cornerNdx + geometryCount; ++idx, ++geometryIndex)
		{
			corners[idx].z() = instanceOffset;
			corners[idx].w() = geometryIndex;
		}
	}

	std::vector<deUint32>		reference(testParams.width * testParams.height);
	tcu::PixelBufferAccess		referenceAccess(imageFormat, testParams.width, testParams.height, 1, reference.data());
	// clear image with miss values
	tcu::UVec4 missValue((testParams.shaderTestType == STT_MISS) ? testParams.sbtRecordOffset : 0, 0, 0, 0);
	tcu::clear(referenceAccess, missValue);

	// for each pixel - set its color to proper value
	for (const auto& pixel : corners)
	{
		deUint32 shaderIndex;
		switch (testParams.shaderTestType)
		{
		case STT_HIT:
		{
			shaderIndex = testParams.sbtRecordOffset + pixel.z() + pixel.w() * testParams.sbtRecordStride;
			break;
		}
		case STT_MISS:
		{
			shaderIndex = 0;// pixel.z();
			break;
		}
		case STT_CALL:
		{
			shaderIndex = testParams.sbtRecordOffset + pixel.z() + pixel.w() * testParams.sbtRecordStride;
			break;
		}
		default:
			TCU_THROW(InternalError, "Wrong shader test type");
		}

		referenceAccess.setPixel(tcu::UVec4(shaderIndex, 0, 0, 0), pixel.x(), pixel.y());
	}

	// compare result and reference
	return tcu::intThresholdCompare(context.getTestContext().getLog(), "Result comparison", "", referenceAccess, resultAccess, tcu::UVec4(0), tcu::COMPARE_LOG_RESULT);
}

VkFormat CheckerboardConfiguration::getResultImageFormat ()
{
	return VK_FORMAT_R32_UINT;
}

size_t CheckerboardConfiguration::getResultImageFormatSize ()
{
	return sizeof(deUint32);
}

VkClearValue CheckerboardConfiguration::getClearValue ()
{
	return makeClearValueColorU32(0xFF, 0u, 0u, 0u);
}

class ShaderBindingTableIndexingTestCase : public TestCase
{
	public:
							ShaderBindingTableIndexingTestCase			(tcu::TestContext& context, const char* name, const char* desc, const TestParams data);
							~ShaderBindingTableIndexingTestCase			(void);

	virtual void			checkSupport								(Context& context) const;
	virtual	void			initPrograms								(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance								(Context& context) const;
private:
	TestParams				m_data;
};

class ShaderBindingTableIndexingTestInstance : public TestInstance
{
public:
																	ShaderBindingTableIndexingTestInstance	(Context& context, const TestParams& data);
																	~ShaderBindingTableIndexingTestInstance	(void);
	tcu::TestStatus													iterate									(void);

protected:
	de::MovePtr<BufferWithMemory>									runTest									();
private:
	TestParams														m_data;
};

ShaderBindingTableIndexingTestCase::ShaderBindingTableIndexingTestCase (tcu::TestContext& context, const char* name, const char* desc, const TestParams data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

ShaderBindingTableIndexingTestCase::~ShaderBindingTableIndexingTestCase (void)
{
}

void ShaderBindingTableIndexingTestCase::checkSupport (Context& context) const
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

void ShaderBindingTableIndexingTestCase::initPrograms (SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions	buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	std::vector<deUint32>	shaderCount	= getShaderCounts();

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadEXT uvec4 hitValue;\n"
			"layout(r32ui, set = 0, binding = 0) uniform uimage2D result;\n"
			"layout(set = 0, binding = 1) uniform TraceRaysParamsUBO\n"
			"{\n"
			"	uvec4 trParams; // x = sbtRecordOffset, y = sbtRecordStride, z = missIndex\n"
			"};\n"
			"layout(set = 0, binding = 2) uniform accelerationStructureEXT topLevelAS;\n"
			"\n"
			"void main()\n"
			"{\n"
			"  float tmin     = 0.0;\n"
			"  float tmax     = 1.0;\n"
			"  vec3  origin   = vec3(float(gl_LaunchIDEXT.x) + 0.5f, float(gl_LaunchIDEXT.y) + 0.5f, 0.5f);\n"
			"  vec3  direct   = vec3(0.0, 0.0, -1.0);\n"
			"  hitValue       = uvec4(0,0,0,0);\n"
			"  traceRayEXT(topLevelAS, 0, 0xFF, trParams.x, trParams.y, trParams.z, origin, tmin, direct, tmax, 0);\n"
			"  imageStore(result, ivec2(gl_LaunchIDEXT.xy), hitValue);\n"
			"}\n";
		programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	for(deUint32 idx = 0; idx < shaderCount[STT_HIT]; ++idx)
	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
			"void main()\n"
			"{\n"
			"  hitValue = uvec4("<< idx << ",0,0,1);\n"
			"}\n";
		std::stringstream csname;
		csname << "chit_" << idx;

		programCollection.glslSources.add(csname.str()) << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(shaderRecordEXT) buffer block\n"
			"{\n"
			"  uvec4 info;\n"
			"};\n"
			"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
			"void main()\n"
			"{\n"
			"  hitValue = info;\n"
			"}\n";
		programCollection.glslSources.add("chit_shaderRecord") << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	for (deUint32 idx = 0; idx < shaderCount[STT_CALL]; ++idx)
	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) callableDataEXT uvec4 value;\n"
			"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
			"void main()\n"
			"{\n"
			"  executeCallableEXT(" << idx << ", 0);\n"
			"  hitValue = value;\n"
			"}\n";
		std::stringstream csname;
		csname << "chit_call_" << idx;

		programCollection.glslSources.add(csname.str()) << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	for (deUint32 idx = 0; idx < shaderCount[STT_MISS]; ++idx)
	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
			"void main()\n"
			"{\n"
			"  hitValue = uvec4(" << idx <<",0,0,1);\n"
			"}\n";
		std::stringstream csname;
		csname << "miss_" << idx;

		programCollection.glslSources.add(csname.str()) << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(shaderRecordEXT) buffer block\n"
			"{\n"
			"  uvec4 info;\n"
			"};\n"
			"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
			"void main()\n"
			"{\n"
			"  hitValue = info;\n"
			"}\n";

		programCollection.glslSources.add("miss_shaderRecord") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	for (deUint32 idx = 0; idx < shaderCount[STT_CALL]; ++idx)
	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) callableDataInEXT uvec4 result;\n"
			"void main()\n"
			"{\n"
			"  result = uvec4(" << idx << ",0,0,1);\n"
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
			"layout(shaderRecordEXT) buffer block\n"
			"{\n"
			"  uvec4 info;\n"
			"};\n"
			"layout(location = 0) callableDataInEXT uvec4 result;\n"
			"void main()\n"
			"{\n"
			"  result = info;\n"
			"}\n";

		programCollection.glslSources.add("call_shaderRecord") << glu::CallableSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}
}

TestInstance* ShaderBindingTableIndexingTestCase::createInstance (Context& context) const
{
	return new ShaderBindingTableIndexingTestInstance(context, m_data);
}

ShaderBindingTableIndexingTestInstance::ShaderBindingTableIndexingTestInstance (Context& context, const TestParams& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
{
}

ShaderBindingTableIndexingTestInstance::~ShaderBindingTableIndexingTestInstance (void)
{
}

de::MovePtr<BufferWithMemory> ShaderBindingTableIndexingTestInstance::runTest ()
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
																					.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ALL_RAY_TRACING_STAGES)
																					.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, ALL_RAY_TRACING_STAGES)
																					.build(vkd, device);
	const Move<VkDescriptorPool>		descriptorPool						= DescriptorPoolBuilder()
																					.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
																					.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
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
	de::MovePtr<BufferWithMemory>									uniformBuffer;

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

		uniformBuffer															= m_data.testConfiguration->initUniformBuffer(m_context, m_data);
		VkDescriptorBufferInfo							uniformBufferInfo		= makeDescriptorBufferInfo(uniformBuffer->get(), 0ull, sizeof(tcu::UVec4));

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
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBufferInfo)
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelerationStructureWriteDescriptorSet)
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

tcu::TestStatus ShaderBindingTableIndexingTestInstance::iterate (void)
{
	// run test using arrays of pointers
	const de::MovePtr<BufferWithMemory>	buffer		= runTest();

	if (!m_data.testConfiguration->verifyImage(buffer.get(), m_context, m_data))
		return tcu::TestStatus::fail("Fail");
	return tcu::TestStatus::pass("Pass");
}

}	// anonymous

tcu::TestCaseGroup*	createShaderBindingTableTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "shader_binding_table", "Tests veryfying shader binding tables"));

	struct ShaderTestTypeData
	{
		ShaderTestType							shaderTestType;
		const char*								name;
	} shaderTestTypes[] =
	{
		{ STT_HIT,		"indexing_hit"	},
		{ STT_MISS,		"indexing_miss"	},
		{ STT_CALL,		"indexing_call"	},
	};

	struct ShaderBufferOffsetData
	{
		deUint32								sbtOffset;
		const char*								name;
	} shaderBufferOffsets[] =
	{
		{ 0u,	"sbt_offset_0"	},
		{ 4u,	"sbt_offset_4"	},
		{ 7u,	"sbt_offset_7"	},
		{ 16u,	"sbt_offset_16"	},
	};

	struct ShaderRecordData
	{
		bool									present;
		const char*								name;
	} shaderRecords[] =
	{
		{ false,	"no_shaderrecord"	},
		{ true,		"shaderrecord"		},
	};

	for (size_t shaderTestNdx = 0; shaderTestNdx < DE_LENGTH_OF_ARRAY(shaderTestTypes); ++shaderTestNdx)
	{
		de::MovePtr<tcu::TestCaseGroup> shaderTestGroup(new tcu::TestCaseGroup(group->getTestContext(), shaderTestTypes[shaderTestNdx].name, ""));

		for (size_t sbtOffsetNdx = 0; sbtOffsetNdx < DE_LENGTH_OF_ARRAY(shaderBufferOffsets); ++sbtOffsetNdx)
		{
			de::MovePtr<tcu::TestCaseGroup> sbtOffsetGroup(new tcu::TestCaseGroup(group->getTestContext(), shaderBufferOffsets[sbtOffsetNdx].name, ""));

			for (size_t shaderRecordNdx = 0; shaderRecordNdx < DE_LENGTH_OF_ARRAY(shaderRecords); ++shaderRecordNdx)
			{
				de::MovePtr<tcu::TestCaseGroup> shaderRecordGroup(new tcu::TestCaseGroup(group->getTestContext(), shaderRecords[shaderRecordNdx].name, ""));

				deUint32		maxSbtRecordStride				= (shaderTestTypes[shaderTestNdx].shaderTestType == STT_HIT) ? MAX_HIT_SBT_RECORD_STRIDE + 1 : 1;
				deUint32		maxSbtRecordOffset				= MAX_SBT_RECORD_OFFSET;
				const deUint32	maxSbtRecordOffsetWithExtraBits = (shaderTestTypes[shaderTestNdx].shaderTestType == STT_MISS)	? MAX_SBT_RECORD_OFFSET | (~((1u << 16) - 1))  //< Only 16 least significant bits matter for miss indices
																																: MAX_SBT_RECORD_OFFSET | (~((1u << 4)  - 1)); //< Only 4 least significant bits matter for SBT record offsets

				for (deUint32 sbtRecordOffset = 0; sbtRecordOffset <= maxSbtRecordOffset; ++sbtRecordOffset)
				for (deUint32 sbtRecordStride = 0; sbtRecordStride <= maxSbtRecordStride; ++sbtRecordStride)
				{
					if ((shaderTestTypes[shaderTestNdx].shaderTestType	!= STT_HIT)				&&
						(sbtRecordStride								== maxSbtRecordStride))
					{
						continue;
					}

					TestParams testParams
					{
						CHECKERBOARD_WIDTH,
						CHECKERBOARD_HEIGHT,
						shaderTestTypes[shaderTestNdx].shaderTestType,
						shaderBufferOffsets[sbtOffsetNdx].sbtOffset,
						shaderRecords[shaderRecordNdx].present,
						sbtRecordOffset,
						(sbtRecordOffset == maxSbtRecordOffset)	? maxSbtRecordOffsetWithExtraBits
																: sbtRecordOffset,
						//< Only first 4 least significant bits matter for SBT record stride
						sbtRecordStride,
						(sbtRecordStride == maxSbtRecordStride)	? maxSbtRecordStride | (~((1u << 4) - 1))
																: sbtRecordStride,
						de::SharedPtr<TestConfiguration>(new CheckerboardConfiguration())
					};

					std::stringstream str;
					str << sbtRecordOffset << "_" << sbtRecordStride;

					if (testParams.sbtRecordStride != testParams.sbtRecordStridePassedToTraceRay)
					{
						str << "_extraSBTRecordStrideBits";
					}

					if (testParams.sbtRecordOffset != testParams.sbtRecordOffsetPassedToTraceRay)
					{
						str << "_extrabits";
					}

					shaderRecordGroup->addChild(new ShaderBindingTableIndexingTestCase(group->getTestContext(), str.str().c_str(), "", testParams));
				}

				sbtOffsetGroup->addChild(shaderRecordGroup.release());
			}

			shaderTestGroup->addChild(sbtOffsetGroup.release());
		}

		group->addChild(shaderTestGroup.release());
	}

	return group.release();
}

}	// RayTracing

}	// vkt
