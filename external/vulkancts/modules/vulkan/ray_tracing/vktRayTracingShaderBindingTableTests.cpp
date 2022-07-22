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

/*

Test the advertised shader group handle alignment requirements work as expected. The tests will prepare shader binding tables using
shader record buffers for padding and achieving the desired alignments.

+-------------------------------------------
| Shader | Shader    | Aligned |
| Group  | Record    | Shader  | ...
| Handle | Buffer    | Group   |
|        | (padding) | Handle  |
+-------------------------------------------

The number of geometries to try (hence the number of alignments and shader record buffers to try) is 32/align + 1, so 33 in the case
of align=1, and 2 in the case of align=32. This allows us to test all possible alignment values.

Geometries are triangles put alongside the X axis. The base triangle is:

0,1|      x
   |     x x
   |    x  0.5,0.5
   |   x  x  x
   |  x       x
   | xxxxxxxxxxx
   +-------------
 0,0             1,0

A triangle surrounding point (0.5, 0.5), in the [0, 1] range of both the X and Y axis.

As more than one triangle is needed, each triangle is translated one more unit in the X axis, so each triangle is in the [i, i+1]
range. The Y axis doesn't change, triangles are always in the [0,1] range.

Triangles have Z=5, and one ray is traced per triangle, origin (i+0.5, 0.5, 0) direction (0, 0, 1), where i is gl_LaunchIDEXT.x.

For each geometry, the shader record buffer contents vary depending on the geometry index and the desired alignment (padding).

Alignment	Element Type	Element Count			Data
1			uint8_t			1						0x80 | geometryID
2			uint16_t		1						0xABC0 | geometryID
4+			uint32_t		alignment/4				For each element: 0xABCDE0F0 | (element << 8) | geometryID

The test will try to verify everything works properly and all shader record buffers can be read with the right values.

 */
struct ShaderGroupHandleAlignmentParams
{
	const uint32_t alignment;

	ShaderGroupHandleAlignmentParams (uint32_t alignment_)
		: alignment (alignment_)
	{
		DE_ASSERT(alignment >= 1u && alignment <= 32u);
		DE_ASSERT(deIsPowerOfTwo32(static_cast<int>(alignment)));
	}

	uint32_t geometryCount () const
	{
		return (32u / alignment + 1u);
	}

	uint32_t shaderRecordElementCount () const
	{
		return ((alignment <= 4u) ? 1u : (alignment / 4u));
	}

	std::string glslElementType () const
	{
		if (alignment == 1u)
			return "uint8_t";
		if (alignment == 2u)
			return "uint16_t";
		return "uint32_t";
	}

	std::string glslExtension () const
	{
		if (alignment == 1u)
			return "GL_EXT_shader_explicit_arithmetic_types_int8";
		if (alignment == 2u)
			return "GL_EXT_shader_explicit_arithmetic_types_int16";
		return "GL_EXT_shader_explicit_arithmetic_types_int32";
	}

	std::vector<uint8_t> getRecordData (uint32_t geometryID) const
	{
		std::vector<uint8_t> recordData;
		switch (alignment)
		{
		case 1u:
			recordData.push_back(static_cast<uint8_t>(0x80u | geometryID));
			break;
		case 2u:
			recordData.push_back(uint8_t{0xABu});
			recordData.push_back(static_cast<uint8_t>(0xC0u | geometryID));
			break;
		default:
			{
				const auto elemCount = shaderRecordElementCount();
				for (uint32_t i = 0u; i < elemCount; ++i)
				{
					recordData.push_back(uint8_t{0xABu});
					recordData.push_back(uint8_t{0xCDu});
					recordData.push_back(static_cast<uint8_t>(0xE0u | i));
					recordData.push_back(static_cast<uint8_t>(0xF0u | geometryID));
				}
			}
			break;
		}
		return recordData;
	}
};

class ShaderGroupHandleAlignmentCase : public TestCase
{
public:
					ShaderGroupHandleAlignmentCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const ShaderGroupHandleAlignmentParams& params)
						: TestCase	(testCtx, name, description)
						, m_params	(params)
						{
						}
	virtual			~ShaderGroupHandleAlignmentCase		(void) {}

	void			checkSupport						(Context& context) const override;
	void			initPrograms						(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance						(Context& context) const override;

protected:
	ShaderGroupHandleAlignmentParams					m_params;
};

class ShaderGroupHandleAlignmentInstance : public TestInstance
{
public:
						ShaderGroupHandleAlignmentInstance	(Context& context, const ShaderGroupHandleAlignmentParams& params)
							: TestInstance	(context)
							, m_params		(params)
							{}
	virtual				~ShaderGroupHandleAlignmentInstance	(void) {}

	tcu::TestStatus		iterate								(void) override;

protected:
	ShaderGroupHandleAlignmentParams							m_params;
};

void ShaderGroupHandleAlignmentCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

	const auto&	vki				= context.getInstanceInterface();
	const auto	physicalDevice	= context.getPhysicalDevice();
	const auto	rtProperties	= makeRayTracingProperties(vki, physicalDevice);

	if (m_params.alignment < rtProperties->getShaderGroupHandleAlignment())
		TCU_THROW(NotSupportedError, "Required shader group handle alignment not supported");

	switch (m_params.alignment)
	{
	case 1u:
		{
			const auto& int8Features = context.getShaderFloat16Int8Features();
			if (!int8Features.shaderInt8)
				TCU_THROW(NotSupportedError, "shaderInt8 not supported");

			const auto& int8StorageFeatures = context.get8BitStorageFeatures();
			if (!int8StorageFeatures.storageBuffer8BitAccess)
				TCU_THROW(NotSupportedError, "storageBuffer8BitAccess not supported");
		}
		break;

	case 2u:
		{
			context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_INT16);

			const auto& int16StorageFeatures = context.get16BitStorageFeatures();
			if (!int16StorageFeatures.storageBuffer16BitAccess)
				TCU_THROW(NotSupportedError, "storageBuffer16BitAccess not supported");
		}
		break;

	default:
		break;
	}
}

void ShaderGroupHandleAlignmentCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	const auto elemType			= m_params.glslElementType();
	const auto geometryCount	= m_params.geometryCount();
	const auto elementCount		= m_params.shaderRecordElementCount();
	const auto extension		= m_params.glslExtension();

	std::ostringstream descriptors;
	descriptors
		<< "layout(set=0, binding=0) uniform accelerationStructureEXT topLevelAS;\n"
		<< "layout(set=0, binding=1, std430) buffer SSBOBlock {\n"
		<< "  " << elemType << " data[" << geometryCount << "][" << elementCount << "];\n"
		<< "} ssbo;\n"
		;
	const auto descriptorsStr = descriptors.str();

	std::ostringstream commonHeader;
	commonHeader
		<< "#version 460 core\n"
		<< "#extension GL_EXT_ray_tracing : require\n"
		<< "#extension " << extension << " : require\n"
		;
	const auto commontHeaderStr = commonHeader.str();

	std::ostringstream rgen;
	rgen
		<< commontHeaderStr
		<< "\n"
		<< descriptorsStr
		<< "layout(location=0) rayPayloadEXT vec4 unused;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "  const uint  rayFlags  = 0;\n"
		<< "  const uint  cullMask  = 0xFF;\n"
		<< "  const float tMin      = 0.0;\n"
		<< "  const float tMax      = 10.0;\n"
		<< "  const vec3  origin    = vec3(float(gl_LaunchIDEXT.x) + 0.5, 0.5, 0.0);\n"
		<< "  const vec3  direction = vec3(0.0, 0.0, 1.0);\n"
		<< "  const uint  sbtOffset = 0;\n"
		<< "  const uint  sbtStride = 1;\n"
		<< "  const uint  missIndex = 0;\n"
		<< "  traceRayEXT(topLevelAS, rayFlags, cullMask, sbtOffset, sbtStride, missIndex, origin, tMin, direction, tMax, 0);\n"
		<< "}\n"
		;

	std::ostringstream chit;
	chit
		<< commontHeaderStr
		<< "\n"
		<< descriptorsStr
		<< "layout(location=0) rayPayloadInEXT vec4 unused;\n"
		<< "layout(shaderRecordEXT, std430) buffer srbBlock {\n"
		<< "  " << elemType << " data[" << elementCount << "];\n"
		<< "} srb;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "  for (uint i = 0; i < " << elementCount << "; ++i) {\n"
		<< "    ssbo.data[gl_LaunchIDEXT.x][i] = srb.data[i];\n"
		<< "  }\n"
		<< "}\n"
		;

	std::ostringstream miss;
	miss
		<< commontHeaderStr
		<< "\n"
		<< descriptorsStr
		<< "layout(location=0) rayPayloadInEXT vec4 unused;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "}\n"
		;

	programCollection.glslSources.add("rgen") << glu::RaygenSource(rgen.str()) << buildOptions;
	programCollection.glslSources.add("chit") << glu::ClosestHitSource(chit.str()) << buildOptions;
	programCollection.glslSources.add("miss") << glu::MissSource(miss.str()) << buildOptions;
}

TestInstance* ShaderGroupHandleAlignmentCase::createInstance (Context& context) const
{
	return new ShaderGroupHandleAlignmentInstance(context, m_params);
}

tcu::TestStatus ShaderGroupHandleAlignmentInstance::iterate (void)
{
	const auto&	vki			= m_context.getInstanceInterface();
	const auto	physDev		= m_context.getPhysicalDevice();
	const auto&	vkd			= m_context.getDeviceInterface();
	const auto	device		= m_context.getDevice();
	auto&		alloc		= m_context.getDefaultAllocator();
	const auto	qIndex		= m_context.getUniversalQueueFamilyIndex();
	const auto	queue		= m_context.getUniversalQueue();
	const auto	stages		= (VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR);
	const auto	geoCount	= m_params.geometryCount();
	const auto	triangleZ	= 5.0f;

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, qIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);

	// Build acceleration structures.
	auto topLevelAS		= makeTopLevelAccelerationStructure();
	auto bottomLevelAS	= makeBottomLevelAccelerationStructure();

	// Create the needed amount of geometries (triangles) with the right coordinates.
	const tcu::Vec3	baseLocation	(0.5f, 0.5f, triangleZ);
	const float		vertexOffset	= 0.25f; // From base location, to build a triangle around it.

	for (uint32_t i = 0; i < geoCount; ++i)
	{
		// Triangle "center" or base location.
		const tcu::Vec3					triangleLocation (baseLocation.x() + static_cast<float>(i), baseLocation.y(), baseLocation.z());

		// Actual triangle.
		const std::vector<tcu::Vec3>	triangle
		{
			tcu::Vec3(triangleLocation.x() - vertexOffset, triangleLocation.y() - vertexOffset, triangleLocation.z()),
			tcu::Vec3(triangleLocation.x() + vertexOffset, triangleLocation.y() - vertexOffset, triangleLocation.z()),
			tcu::Vec3(triangleLocation.x(),                triangleLocation.y() + vertexOffset, triangleLocation.z()),
		};

		bottomLevelAS->addGeometry(triangle, true/*triangles*/);
	}

	bottomLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc);

	de::SharedPtr<BottomLevelAccelerationStructure> blasSharedPtr (bottomLevelAS.release());
	topLevelAS->setInstanceCount(1);
	topLevelAS->addInstance(blasSharedPtr, identityMatrix3x4, 0u, 0xFF, 0u, VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR);
	topLevelAS->createAndBuild(vkd, device, cmdBuffer, alloc);

	// Get some ray tracing properties.
	uint32_t shaderGroupHandleSize		= 0u;
	uint32_t shaderGroupBaseAlignment	= 1u;
	{
		const auto rayTracingPropertiesKHR	= makeRayTracingProperties(vki, physDev);
		shaderGroupHandleSize				= rayTracingPropertiesKHR->getShaderGroupHandleSize();
		shaderGroupBaseAlignment			= rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
	}

	// SSBO to copy results over from the shaders.
	const auto			shaderRecordSize	= m_params.alignment;
	const auto			hitSBTStride		= shaderGroupHandleSize + shaderRecordSize;
	const auto			ssboSize			= static_cast<VkDeviceSize>(geoCount * hitSBTStride);
	const auto			ssboInfo			= makeBufferCreateInfo(ssboSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory	ssbo				(vkd, device, alloc, ssboInfo, MemoryRequirement::HostVisible);
	auto&				ssboAlloc			= ssbo.getAllocation();
	void*				ssboData			= ssboAlloc.getHostPtr();

	deMemset(ssboData, 0, static_cast<size_t>(ssboSize));

	// Descriptor set layout and pipeline layout.
	DescriptorSetLayoutBuilder setLayoutBuilder;
	setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, stages);
	setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stages);
	const auto setLayout		= setLayoutBuilder.build(vkd, device);
	const auto pipelineLayout	= makePipelineLayout(vkd, device, setLayout.get());

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

		const auto ssboDescInfo = makeDescriptorBufferInfo(ssbo.get(), 0ull, ssboSize);

		DescriptorSetUpdateBuilder updateBuilder;
		updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelDescInfo);
		updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &ssboDescInfo);
		updateBuilder.update(vkd, device);
	}

	// Shader modules.
	auto rgenModule = makeVkSharedPtr(createShaderModule(vkd, device, m_context.getBinaryCollection().get("rgen"), 0));
	auto missModule = makeVkSharedPtr(createShaderModule(vkd, device, m_context.getBinaryCollection().get("miss"), 0));
	auto chitModule = makeVkSharedPtr(createShaderModule(vkd, device, m_context.getBinaryCollection().get("chit"), 0));

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

	// Create shader record buffer data.
	using DataVec = std::vector<uint8_t>;

	std::vector<DataVec> srbData;
	for (uint32_t i = 0; i < geoCount; ++i)
	{
		srbData.emplace_back(m_params.getRecordData(i));
	}

	std::vector<const void*> srbDataPtrs;
	srbDataPtrs.reserve(srbData.size());
	std::transform(begin(srbData), end(srbData), std::back_inserter(srbDataPtrs), [](const DataVec& data) { return data.data(); });

	// Generate ids for the closest hit and miss shaders according to the test parameters.
	{
		const auto rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();

		rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgenModule, 0u);
		rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, missModule, 1u);

		for (uint32_t i = 0; i < geoCount; ++i)
			rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, chitModule, 2u + i);

		pipeline = rayTracingPipeline->createPipeline(vkd, device, pipelineLayout.get());

		raygenSBT		= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 0u, 1u);
		raygenSBTRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenSBT->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);

		missSBT			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 1u, 1u);
		missSBTRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missSBT->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);

		hitSBT			= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline.get(), alloc, shaderGroupHandleSize, shaderGroupBaseAlignment, 2u, geoCount,
																	   0u, 0u, MemoryRequirement::Any, 0u, 0u, shaderRecordSize, srbDataPtrs.data(), false/*autoalign*/);
		hitSBTRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitSBT->get(), 0), hitSBTStride, hitSBTStride*geoCount);
	}

	// Trace rays and verify ssbo contents.
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdTraceRaysKHR(cmdBuffer, &raygenSBTRegion, &missSBTRegion, &hitSBTRegion, &callableSBTRegion, geoCount, 1u, 1u);
	const auto shaderToHostBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	cmdPipelineMemoryBarrier(vkd, cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_HOST_BIT, &shaderToHostBarrier);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	invalidateAlloc(vkd, device, ssboAlloc);

	// Verify SSBO.
	const auto	ssboDataAsBytes	= reinterpret_cast<const uint8_t*>(ssboData);
	size_t		ssboDataIdx		= 0u;
	bool		fail			= false;
	auto&		log				= m_context.getTestContext().getLog();

	for (const auto& dataVec : srbData)
		for (const uint8_t byte : dataVec)
		{
			const uint8_t outputByte = ssboDataAsBytes[ssboDataIdx++];
			if (byte != outputByte)
			{
				std::ostringstream msg;
				msg
					<< std::hex << std::setfill('0')
					<< "Unexpectd output data: "
					<< "0x" << std::setw(2) << static_cast<int>(outputByte)
					<< " vs "
					<< "0x" << std::setw(2) << static_cast<int>(byte)
					;
				log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
				fail = true;
			}
		}

	if (fail)
		return tcu::TestStatus::fail("Unexpected output data found; check log for details");
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

	{
		const uint32_t					kAlignments[]			= { 1u, 2u, 4u, 8u, 16u, 32u };
		de::MovePtr<tcu::TestCaseGroup>	handleAlignmentGroup	(new tcu::TestCaseGroup(testCtx, "handle_alignment", "Test allowed handle alignments"));

		for (const auto alignment : kAlignments)
		{
			const auto alignStr = std::to_string(alignment);
			const auto testName = "alignment_" + alignStr;
			const auto testDesc = "Check aligning shader group handles to " + alignStr + " bytes";

			handleAlignmentGroup->addChild(new ShaderGroupHandleAlignmentCase(testCtx, testName, testDesc, ShaderGroupHandleAlignmentParams{alignment}));
		}

		group->addChild(handleAlignmentGroup.release());
	}

	return group.release();
}

}	// RayTracing

}	// vkt
