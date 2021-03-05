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
 * \brief Ray Tracing Capture/Replay tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingCaptureReplayTests.hpp"

#include <set>
#include "vkDefs.hpp"

#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkTypeUtil.hpp"

#include "vkRayTracingUtil.hpp"

#include "tcuCommandLine.hpp"

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

static const deUint32	RTCR_DEFAULT_SIZE = 8u;
static const deUint32	RTCR_SHADER_COUNT = 4u;

enum SBTReplayTestType
{
	TEST_ACCELERATION_STRUCTURES,
	TEST_PIPELINE_SINGLE,
	TEST_PIPELINE_AFTER,
	TEST_PIPELINE_BEFORE
};

enum ASOperationTarget
{
	OT_NONE,
	OT_TOP_ACCELERATION,
	OT_BOTTOM_ACCELERATION
};

enum ASOperationType
{
	OP_NONE,
	OP_COPY,
	OP_COMPACT,
	OP_SERIALIZE
};

enum ASBottomTestType
{
	BTT_TRIANGLES,
	BTT_AABBS
};

enum ASTopTestType
{
	TTT_IDENTICAL_INSTANCES,
	TTT_DIFFERENT_INSTANCES
};

struct TestParams;

struct PipelineOutput
{
	Move<VkPipeline>				pipeline;
	de::MovePtr<BufferWithMemory>	raygenShaderBindingTable;
	de::MovePtr<BufferWithMemory>	missShaderBindingTable;
	de::MovePtr<BufferWithMemory>	hitShaderBindingTable;
	Move<VkDescriptorSet>			descriptorSet;
	de::MovePtr<BufferWithMemory>	uniformBuffer;

	VkStridedDeviceAddressRegionKHR	raygenShaderBindingTableRegion;
	VkStridedDeviceAddressRegionKHR	missShaderBindingTableRegion;
	VkStridedDeviceAddressRegionKHR	hitShaderBindingTableRegion;
	VkStridedDeviceAddressRegionKHR	callableShaderBindingTableRegion;
};

struct PipelineData
{
	PipelineData(Allocator& alloc)
		: allocator(alloc)
	{}
	VkDescriptorSetLayout			descriptorSetLayout;
	VkDescriptorPool				descriptorPool;
	VkPipelineLayout				pipelineLayout;
	Allocator&						allocator;
	PipelineOutput					pipelines[2];
};

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
																												 const DeviceInterface&				vkd,
																												 const VkDevice						device,
																												 TestParams&						testParams,
																												 bool								replay) = 0;
	virtual void															initShaderBindingTables				(de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
																												 Context&							context,
																												 const DeviceInterface&				vkd,
																												 const VkDevice						device,
																												 TestParams&						testParams,
																												 deUint32							shaderGroupHandleSize,
																												 deUint32							shaderGroupBaseAlignment,
																												 PipelineData&						pipelineData,
																												 bool								replay) = 0;
	virtual bool															verifyImage							(const std::vector<deUint32>&		captureResults,
																												 const std::vector<deUint32>&		replayResults,
																												 Context&							context,
																												 TestParams&						testParams) = 0;
	virtual VkFormat														getResultImageFormat				() = 0;
	virtual size_t															getResultImageFormatSize			() = 0;
	virtual VkClearValue													getClearValue						() = 0;
};

struct TestParams
{
	SBTReplayTestType							testType;			// SBT
	ASOperationTarget							operationTarget;	// AS
	ASOperationType								operationType;		// AS
	vk::VkAccelerationStructureBuildTypeKHR		buildType;			// AS
	ASBottomTestType							bottomType;			// AS
	ASTopTestType								topType;			// AS
	deUint32									width;
	deUint32									height;
	de::SharedPtr<TestConfiguration>			testConfiguration;
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

Move<VkQueryPool> makeQueryPool(const DeviceInterface&		vk,
								const VkDevice				device,
								const VkQueryType			queryType,
								deUint32					queryCount)
{
	const VkQueryPoolCreateInfo				queryPoolCreateInfo =
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,		// sType
		DE_NULL,										// pNext
		(VkQueryPoolCreateFlags)0,						// flags
		queryType,										// queryType
		queryCount,										// queryCount
		0u,												// pipelineStatistics
	};
	return createQueryPool(vk, device, &queryPoolCreateInfo);
}

VkDeviceAddress getAccelerationStructureDeviceAddress (const DeviceInterface&						vk,
													   const VkDevice								device,
													   VkAccelerationStructureKHR					accelerationStructure)
{
	const VkAccelerationStructureDeviceAddressInfoKHR addressInfo =
	{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,	// VkStructureType               sType;
		DE_NULL,															// const void*                   pNext;
		accelerationStructure												// VkAccelerationStructureKHR    accelerationStructure
	};
	return vk.getAccelerationStructureDeviceAddressKHR(device, &addressInfo);
}

class TestShaderBindingTablesConfiguration : public TestConfiguration
{
public:
	std::vector<de::SharedPtr<BottomLevelAccelerationStructure>>	initBottomAccelerationStructures	(Context&							context,
																										 TestParams&						testParams) override;
	de::MovePtr<TopLevelAccelerationStructure>						initTopAccelerationStructure		(Context&							context,
																										 TestParams&						testParams,
																										 std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >&	bottomLevelAccelerationStructures) override;
	void															initRayTracingShaders				(de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
																										 Context&							context,
																										 const DeviceInterface&				vkd,
																										 const VkDevice						device,
																										 TestParams&						testParams,
																										 bool								replay) override;
	void															initShaderBindingTables				(de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
																										 Context&							context,
																										 const DeviceInterface&				vkd,
																										 const VkDevice						device,
																										 TestParams&						testParams,
																										 deUint32							shaderGroupHandleSize,
																										 deUint32							shaderGroupBaseAlignment,
																										 PipelineData&						pipelineData,
																										 bool								replay) override;
	bool															verifyImage							(const std::vector<deUint32>&		captureResults,
																										 const std::vector<deUint32>&		replayResults,
																										 Context&							context,
																										 TestParams&						testParams) override;
	VkFormat														getResultImageFormat				() override;
	size_t															getResultImageFormatSize			() override;
	VkClearValue													getClearValue						() override;
protected:
	VkDeviceAddress													sbtSavedRaygenAddress	= 0u;
	VkDeviceAddress													sbtSavedMissAddress		= 0u;
	VkDeviceAddress													sbtSavedHitAddress		= 0u;
};

std::vector<de::SharedPtr<BottomLevelAccelerationStructure> > TestShaderBindingTablesConfiguration::initBottomAccelerationStructures (Context&					context,
																																	  TestParams&				testParams)
{
	DE_UNREF(context);
	std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >	result;

	tcu::Vec3 v0(0.0, 1.0, 0.0);
	tcu::Vec3 v1(0.0, 0.0, 0.0);
	tcu::Vec3 v2(1.0, 1.0, 0.0);
	tcu::Vec3 v3(1.0, 0.0, 0.0);

	for (deUint32 y = 0; y < testParams.height; ++y)
	for (deUint32 x = 0; x < testParams.width; ++x)
	{
		// let's build a chessboard of geometries
		if (((x + y) % 2) == 0)
			continue;
		tcu::Vec3 xyz((float)x, (float)y, 0.0f);
		std::vector<tcu::Vec3>	geometryData;

		de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure = makeBottomLevelAccelerationStructure();
		bottomLevelAccelerationStructure->setGeometryCount(1u);

		geometryData.push_back(xyz + v0);
		geometryData.push_back(xyz + v1);
		geometryData.push_back(xyz + v2);
		geometryData.push_back(xyz + v2);
		geometryData.push_back(xyz + v1);
		geometryData.push_back(xyz + v3);

		bottomLevelAccelerationStructure->addGeometry(geometryData, true);
		result.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));
	}

	return result;
}

de::MovePtr<TopLevelAccelerationStructure> TestShaderBindingTablesConfiguration::initTopAccelerationStructure (Context&					context,
																											   TestParams&				testParams,
																											   std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >&	bottomLevelAccelerationStructures)
{
	DE_UNREF(context);
	deUint32 instanceCount = testParams.width * testParams.height / 2;

	de::MovePtr<TopLevelAccelerationStructure>	result = makeTopLevelAccelerationStructure();
	result->setInstanceCount(instanceCount);

	deUint32				currentInstanceIndex = 0;
	VkTransformMatrixKHR	identityMatrix = { { { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f } } };

	for (deUint32 y = 0; y < testParams.height; ++y)
	{
		deUint32 shaderOffset = y % RTCR_SHADER_COUNT;
		for (deUint32 x = 0; x < testParams.width; ++x)
		{
			if (((x + y) % 2) == 0)
				continue;
			result->addInstance(bottomLevelAccelerationStructures[currentInstanceIndex++], identityMatrix, 0, 0xFF, shaderOffset);
		}
	}

	return result;
}

void TestShaderBindingTablesConfiguration::initRayTracingShaders (de::MovePtr<RayTracingPipeline>&		rayTracingPipeline,
																  Context&								context,
																  const DeviceInterface&				vkd,
																  const VkDevice						device,
																  TestParams&							testParams,
																  bool									replay)
{
	DE_UNREF(testParams);
	DE_UNREF(replay);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get("rgen"), 0), 0);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get("miss"), 0), 1);
	for (deUint32 shaderNdx = 0; shaderNdx < RTCR_SHADER_COUNT; ++shaderNdx)
	{
		std::stringstream shaderName;
		shaderName << "chit" << shaderNdx;
		rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, createShaderModule(vkd, device, context.getBinaryCollection().get(shaderName.str()), 0), 2 + shaderNdx);
	}
}

void TestShaderBindingTablesConfiguration::initShaderBindingTables (de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
																	Context&							context,
																	const DeviceInterface&				vkd,
																	const VkDevice						device,
																	TestParams&							testParams,
																	deUint32							shaderGroupHandleSize,
																	deUint32							shaderGroupBaseAlignment,
																	PipelineData&						pipelineData,
																	bool								replay)
{
	DE_UNREF(context);
	const VkBufferCreateInfo				uniformBufferCreateInfo		= makeBufferCreateInfo(sizeof(deUint32), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	if (!replay) // capture phase
	{
		pipelineData.pipelines[0].pipeline								= rayTracingPipeline->createPipeline(vkd, device, pipelineData.pipelineLayout);
		pipelineData.pipelines[0].raygenShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, *(pipelineData.pipelines[0].pipeline), pipelineData.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1, VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, MemoryRequirement::DeviceAddress, 0u);
		pipelineData.pipelines[0].missShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, *(pipelineData.pipelines[0].pipeline), pipelineData.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1, VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, MemoryRequirement::DeviceAddress, 0u);
		pipelineData.pipelines[0].hitShaderBindingTable					= rayTracingPipeline->createShaderBindingTable(vkd, device, *(pipelineData.pipelines[0].pipeline), pipelineData.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, RTCR_SHADER_COUNT, VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, MemoryRequirement::DeviceAddress, 0u);
		pipelineData.pipelines[0].descriptorSet							= makeDescriptorSet(vkd, device, pipelineData.descriptorPool, pipelineData.descriptorSetLayout);
		pipelineData.pipelines[0].uniformBuffer							= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, pipelineData.allocator, uniformBufferCreateInfo, MemoryRequirement::HostVisible));
		pipelineData.pipelines[0].raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, pipelineData.pipelines[0].raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
		pipelineData.pipelines[0].missShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, pipelineData.pipelines[0].missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
		pipelineData.pipelines[0].hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, pipelineData.pipelines[0].hitShaderBindingTable->get(), 0), shaderGroupHandleSize, RTCR_SHADER_COUNT * shaderGroupHandleSize);
		pipelineData.pipelines[0].callableShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

		// capture SBT addresses
		VkBufferDeviceAddressInfo	deviceAddressInfo	=
		{
			VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,	//VkStructureType    sType;
			DE_NULL,										//const void*        pNext;
			DE_NULL											//VkBuffer           buffer;
		};
		deviceAddressInfo.buffer										= pipelineData.pipelines[0].raygenShaderBindingTable->get();
		sbtSavedRaygenAddress											= vkd.getBufferDeviceAddress( device, &deviceAddressInfo);
		deviceAddressInfo.buffer										= pipelineData.pipelines[0].missShaderBindingTable->get();
		sbtSavedMissAddress												= vkd.getBufferDeviceAddress( device, &deviceAddressInfo);
		deviceAddressInfo.buffer										= pipelineData.pipelines[0].hitShaderBindingTable->get();
		sbtSavedHitAddress												= vkd.getBufferDeviceAddress( device, &deviceAddressInfo);
	}
	else // replay phase
	{
		switch (testParams.testType)
		{
		case TEST_PIPELINE_SINGLE:
			pipelineData.pipelines[0].pipeline							= rayTracingPipeline->createPipeline(vkd, device, pipelineData.pipelineLayout);
			pipelineData.pipelines[0].raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, *(pipelineData.pipelines[0].pipeline), pipelineData.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1, 0u, 0u, MemoryRequirement::Any, sbtSavedRaygenAddress);
			pipelineData.pipelines[0].missShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, *(pipelineData.pipelines[0].pipeline), pipelineData.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1, 0u, 0u, MemoryRequirement::Any, sbtSavedMissAddress);
			pipelineData.pipelines[0].hitShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, *(pipelineData.pipelines[0].pipeline), pipelineData.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, RTCR_SHADER_COUNT, 0u, 0u, MemoryRequirement::Any, sbtSavedHitAddress);
			pipelineData.pipelines[0].descriptorSet						= makeDescriptorSet(vkd, device, pipelineData.descriptorPool, pipelineData.descriptorSetLayout);
			pipelineData.pipelines[0].uniformBuffer						= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, pipelineData.allocator, uniformBufferCreateInfo, MemoryRequirement::HostVisible));
			pipelineData.pipelines[0].raygenShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, pipelineData.pipelines[0].raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			pipelineData.pipelines[0].missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, pipelineData.pipelines[0].missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			pipelineData.pipelines[0].hitShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, pipelineData.pipelines[0].hitShaderBindingTable->get(), 0), shaderGroupHandleSize, RTCR_SHADER_COUNT * shaderGroupHandleSize);
			pipelineData.pipelines[0].callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

			break;
		case TEST_PIPELINE_AFTER:
			pipelineData.pipelines[0].pipeline							= rayTracingPipeline->createPipeline(vkd, device, pipelineData.pipelineLayout);
			pipelineData.pipelines[0].raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, *(pipelineData.pipelines[0].pipeline), pipelineData.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1, 0u, 0u, MemoryRequirement::Any, sbtSavedRaygenAddress);
			pipelineData.pipelines[0].missShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, *(pipelineData.pipelines[0].pipeline), pipelineData.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1, 0u, 0u, MemoryRequirement::Any, sbtSavedMissAddress);
			pipelineData.pipelines[0].hitShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, *(pipelineData.pipelines[0].pipeline), pipelineData.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, RTCR_SHADER_COUNT, 0u, 0u, MemoryRequirement::Any, sbtSavedHitAddress);
			pipelineData.pipelines[0].descriptorSet						= makeDescriptorSet(vkd, device, pipelineData.descriptorPool, pipelineData.descriptorSetLayout);
			pipelineData.pipelines[0].uniformBuffer						= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, pipelineData.allocator, uniformBufferCreateInfo, MemoryRequirement::HostVisible));
			pipelineData.pipelines[0].raygenShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, pipelineData.pipelines[0].raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			pipelineData.pipelines[0].missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, pipelineData.pipelines[0].missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			pipelineData.pipelines[0].hitShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, pipelineData.pipelines[0].hitShaderBindingTable->get(), 0), shaderGroupHandleSize, RTCR_SHADER_COUNT * shaderGroupHandleSize);
			pipelineData.pipelines[0].callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

			pipelineData.pipelines[1].pipeline							= rayTracingPipeline->createPipeline(vkd, device, pipelineData.pipelineLayout);
			pipelineData.pipelines[1].raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, *(pipelineData.pipelines[1].pipeline), pipelineData.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1, 0u, 0u, MemoryRequirement::Any, 0u);
			pipelineData.pipelines[1].missShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, *(pipelineData.pipelines[1].pipeline), pipelineData.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1, 0u, 0u, MemoryRequirement::Any, 0u);
			pipelineData.pipelines[1].hitShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, *(pipelineData.pipelines[1].pipeline), pipelineData.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, RTCR_SHADER_COUNT, 0u, 0u, MemoryRequirement::Any, 0u);
			pipelineData.pipelines[1].descriptorSet						= makeDescriptorSet(vkd, device, pipelineData.descriptorPool, pipelineData.descriptorSetLayout);
			pipelineData.pipelines[1].uniformBuffer						= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, pipelineData.allocator, uniformBufferCreateInfo, MemoryRequirement::HostVisible));
			pipelineData.pipelines[1].raygenShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, pipelineData.pipelines[1].raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			pipelineData.pipelines[1].missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, pipelineData.pipelines[1].missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			pipelineData.pipelines[1].hitShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, pipelineData.pipelines[1].hitShaderBindingTable->get(), 0), shaderGroupHandleSize, RTCR_SHADER_COUNT * shaderGroupHandleSize);
			pipelineData.pipelines[1].callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

			break;
		case TEST_PIPELINE_BEFORE:
			pipelineData.pipelines[0].pipeline							= rayTracingPipeline->createPipeline(vkd, device, pipelineData.pipelineLayout);
			pipelineData.pipelines[0].raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, *(pipelineData.pipelines[0].pipeline), pipelineData.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1, 0u, 0u, MemoryRequirement::Any, 0u);
			pipelineData.pipelines[0].missShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, *(pipelineData.pipelines[0].pipeline), pipelineData.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1, 0u, 0u, MemoryRequirement::Any, 0u);
			pipelineData.pipelines[0].hitShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, *(pipelineData.pipelines[0].pipeline), pipelineData.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, RTCR_SHADER_COUNT, 0u, 0u, MemoryRequirement::Any, 0u);
			pipelineData.pipelines[0].descriptorSet						= makeDescriptorSet(vkd, device, pipelineData.descriptorPool, pipelineData.descriptorSetLayout);
			pipelineData.pipelines[0].uniformBuffer						= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, pipelineData.allocator, uniformBufferCreateInfo, MemoryRequirement::HostVisible));
			pipelineData.pipelines[0].raygenShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, pipelineData.pipelines[0].raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			pipelineData.pipelines[0].missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, pipelineData.pipelines[0].missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			pipelineData.pipelines[0].hitShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, pipelineData.pipelines[0].hitShaderBindingTable->get(), 0), shaderGroupHandleSize, RTCR_SHADER_COUNT * shaderGroupHandleSize);
			pipelineData.pipelines[0].callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

			pipelineData.pipelines[1].pipeline							= rayTracingPipeline->createPipeline(vkd, device, pipelineData.pipelineLayout);
			pipelineData.pipelines[1].raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, *(pipelineData.pipelines[1].pipeline), pipelineData.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1, 0u, 0u, MemoryRequirement::Any, sbtSavedRaygenAddress);
			pipelineData.pipelines[1].missShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, *(pipelineData.pipelines[1].pipeline), pipelineData.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1, 0u, 0u, MemoryRequirement::Any, sbtSavedMissAddress);
			pipelineData.pipelines[1].hitShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, *(pipelineData.pipelines[1].pipeline), pipelineData.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, RTCR_SHADER_COUNT, 0u, 0u, MemoryRequirement::Any, sbtSavedHitAddress);
			pipelineData.pipelines[1].descriptorSet						= makeDescriptorSet(vkd, device, pipelineData.descriptorPool, pipelineData.descriptorSetLayout);
			pipelineData.pipelines[1].uniformBuffer						= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, pipelineData.allocator, uniformBufferCreateInfo, MemoryRequirement::HostVisible));
			pipelineData.pipelines[1].raygenShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, pipelineData.pipelines[1].raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			pipelineData.pipelines[1].missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, pipelineData.pipelines[1].missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
			pipelineData.pipelines[1].hitShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, pipelineData.pipelines[1].hitShaderBindingTable->get(), 0), shaderGroupHandleSize, RTCR_SHADER_COUNT * shaderGroupHandleSize);
			pipelineData.pipelines[1].callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
			break;
		default:
			TCU_THROW(InternalError, "Wrong test type");
			break;
		}
	}
}

bool TestShaderBindingTablesConfiguration::verifyImage (const std::vector<deUint32>&		captureResults,
														const std::vector<deUint32>&		replayResults,
														Context&							context,
														TestParams&							testParams)
{
	DE_UNREF(context);

	deUint32							pipelineCount	= (testParams.testType == TEST_PIPELINE_SINGLE) ? 1u : 2u;
	deUint32							imageSize		= testParams.height * testParams.width;
	deUint32							failures		= 0;

	// verify results - each test case should generate checkerboard pattern
	for (deUint32 pipelineNdx = 0; pipelineNdx < pipelineCount; ++pipelineNdx)
	for (deUint32 pos = 0; pos < imageSize; ++pos)
	{
		if (captureResults[pos] != replayResults[pipelineNdx*imageSize + pos])
			failures++;
	}
	return failures == 0;
}

VkFormat TestShaderBindingTablesConfiguration::getResultImageFormat ()
{
	return VK_FORMAT_R32_UINT;
}

size_t TestShaderBindingTablesConfiguration::getResultImageFormatSize ()
{
	return sizeof(deUint32);
}

VkClearValue TestShaderBindingTablesConfiguration::getClearValue ()
{
	return makeClearValueColorU32(0xFF, 0u, 0u, 0u);
}

class TestAccelerationStructuresConfiguration : public TestConfiguration
{
public:
	std::vector<de::SharedPtr<BottomLevelAccelerationStructure>>	initBottomAccelerationStructures	(Context&							context,
																										 TestParams&						testParams) override;
	de::MovePtr<TopLevelAccelerationStructure>						initTopAccelerationStructure		(Context&							context,
																										 TestParams&						testParams,
																										 std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >&	bottomLevelAccelerationStructures) override;
	void															initRayTracingShaders				(de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
																										 Context&							context,
																										 const DeviceInterface&				vkd,
																										 const VkDevice						device,
																										 TestParams&						testParams,
																										 bool								replay) override;
	void															initShaderBindingTables				(de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
																										 Context&							context,
																										 const DeviceInterface&				vkd,
																										 const VkDevice						device,
																										 TestParams&						testParams,
																										 deUint32							shaderGroupHandleSize,
																										 deUint32							shaderGroupBaseAlignment,
																										 PipelineData&						pipelineData,
																										 bool								replay) override;
	bool															verifyImage							(const std::vector<deUint32>&		captureResults,
																										 const std::vector<deUint32>&		replayResults,
																										 Context&							context,
																										 TestParams&						testParams) override;
	VkFormat														getResultImageFormat				() override;
	size_t															getResultImageFormatSize			() override;
	VkClearValue													getClearValue						() override;
protected:
	VkDeviceAddress													sbtSavedRaygenAddress	= 0u;
	VkDeviceAddress													sbtSavedMissAddress		= 0u;
	VkDeviceAddress													sbtSavedHitAddress		= 0u;
};

std::vector<de::SharedPtr<BottomLevelAccelerationStructure>> TestAccelerationStructuresConfiguration::initBottomAccelerationStructures (Context&				context,
																																		 TestParams&			testParams)
{
	DE_UNREF(context);
	std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >	result;

	tcu::Vec3 v0(0.0, 1.0, 0.0);
	tcu::Vec3 v1(0.0, 0.0, 0.0);
	tcu::Vec3 v2(1.0, 1.0, 0.0);
	tcu::Vec3 v3(1.0, 0.0, 0.0);

	if (testParams.topType == TTT_DIFFERENT_INSTANCES)
	{
		de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure = makeBottomLevelAccelerationStructure();
		bottomLevelAccelerationStructure->setGeometryCount(1u);
		de::SharedPtr<RaytracedGeometryBase> geometry;
		if (testParams.bottomType == BTT_TRIANGLES)
		{
			geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_TRIANGLES_KHR, VK_FORMAT_R32G32B32_SFLOAT, VK_INDEX_TYPE_NONE_KHR);
			geometry->addVertex(v0);
			geometry->addVertex(v1);
			geometry->addVertex(v2);
			geometry->addVertex(v2);
			geometry->addVertex(v1);
			geometry->addVertex(v3);
		}
		else // m_data.bottomType == BTT_AABBS
		{
			geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_AABBS_KHR, VK_FORMAT_R32G32B32_SFLOAT, VK_INDEX_TYPE_NONE_KHR);
			geometry->addVertex(tcu::Vec3(0.0f, 0.0f, -0.1f));
			geometry->addVertex(tcu::Vec3(1.0f, 1.0f, 0.1f));
		}

		bottomLevelAccelerationStructure->addGeometry(geometry);
		result.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));
	}
	else // m_data.topTestType == TTT_IDENTICAL_INSTANCES
	{
		// triangle and aabb tests use geometries/aabbs with different vertex positions and the same identity matrix in each instance data
		for (deUint32 y = 0; y < testParams.height; ++y)
			for (deUint32 x = 0; x < testParams.width; ++x)
			{
				// let's build a chessboard of geometries
				if (((x + y) % 2) == 0)
					continue;
				tcu::Vec3 xyz((float)x, (float)y, 0.0f);

				de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure = makeBottomLevelAccelerationStructure();
				bottomLevelAccelerationStructure->setGeometryCount(1u);

				de::SharedPtr<RaytracedGeometryBase> geometry;
				if (testParams.bottomType == BTT_TRIANGLES)
				{
					geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_TRIANGLES_KHR, VK_FORMAT_R32G32B32_SFLOAT, VK_INDEX_TYPE_NONE_KHR);
					geometry->addVertex(xyz + v0);
					geometry->addVertex(xyz + v1);
					geometry->addVertex(xyz + v2);
					geometry->addVertex(xyz + v2);
					geometry->addVertex(xyz + v1);
					geometry->addVertex(xyz + v3);
				}
				else // testParams.bottomTestType == BTT_AABBS
				{
					geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_AABBS_KHR, VK_FORMAT_R32G32B32_SFLOAT, VK_INDEX_TYPE_NONE_KHR);
					geometry->addVertex(xyz + tcu::Vec3(0.0f, 0.0f, -0.1f));
					geometry->addVertex(xyz + tcu::Vec3(1.0f, 1.0f, 0.1f));
				}

				bottomLevelAccelerationStructure->addGeometry(geometry);
				result.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));
			}
	}

	return result;
}

de::MovePtr<TopLevelAccelerationStructure> TestAccelerationStructuresConfiguration::initTopAccelerationStructure (Context&					context,
																												  TestParams&				testParams,
																												  std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >&	bottomLevelAccelerationStructures)
{
	DE_UNREF(context);

	deUint32 instanceCount = testParams.width * testParams.height / 2;

	de::MovePtr<TopLevelAccelerationStructure>	result = makeTopLevelAccelerationStructure();
	result->setInstanceCount(instanceCount);

	if (testParams.topType == TTT_DIFFERENT_INSTANCES)
	{

		for (deUint32 y = 0; y < testParams.height; ++y)
		for (deUint32 x = 0; x < testParams.width; ++x)
		{
			if (((x + y) % 2) == 0)
				continue;
			const VkTransformMatrixKHR			transformMatrixKHR =
			{
				{								//  float	matrix[3][4];
					{ 1.0f, 0.0f, 0.0f, (float)x },
					{ 0.0f, 1.0f, 0.0f, (float)y },
					{ 0.0f, 0.0f, 1.0f, 0.0f },
				}
			};
			result->addInstance(bottomLevelAccelerationStructures[0], transformMatrixKHR);
		}
	}
	else // testParams.topType == TTT_IDENTICAL_INSTANCES
	{
		deUint32 currentInstanceIndex = 0;

		for (deUint32 y = 0; y < testParams.height; ++y)
		for (deUint32 x = 0; x < testParams.width; ++x)
		{
			if (((x + y) % 2) == 0)
				continue;
			result->addInstance(bottomLevelAccelerationStructures[currentInstanceIndex++]);
		}
	}

	return result;
}

void TestAccelerationStructuresConfiguration::initRayTracingShaders (de::MovePtr<RayTracingPipeline>&		rayTracingPipeline,
																	 Context&								context,
																	 const DeviceInterface&					vkd,
																	 const VkDevice							device,
																	 TestParams&							testParams,
																	 bool									replay)
{
	DE_UNREF(testParams);
	DE_UNREF(replay);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get("rgen"),  0), 0);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	createShaderModule(vkd, device, context.getBinaryCollection().get("chit1"),  0), 1);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	createShaderModule(vkd, device, context.getBinaryCollection().get("chit1"),  0), 2);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR,	createShaderModule(vkd, device, context.getBinaryCollection().get("isect"), 0), 2);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR,			createShaderModule(vkd, device, context.getBinaryCollection().get("miss"),  0), 3);

}

void TestAccelerationStructuresConfiguration::initShaderBindingTables (de::MovePtr<RayTracingPipeline>&		rayTracingPipeline,
																	   Context&								context,
																	   const DeviceInterface&				vkd,
																	   const VkDevice						device,
																	   TestParams&							testParams,
																	   deUint32								shaderGroupHandleSize,
																	   deUint32								shaderGroupBaseAlignment,
																	   PipelineData&						pipelineData,
																	   bool									replay)
{
	DE_UNREF(context);
	DE_UNREF(replay);
	const VkBufferCreateInfo				uniformBufferCreateInfo		= makeBufferCreateInfo(sizeof(deUint32), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	pipelineData.pipelines[0].pipeline									= rayTracingPipeline->createPipeline(vkd, device, pipelineData.pipelineLayout);
	pipelineData.pipelines[0].raygenShaderBindingTable					= rayTracingPipeline->createShaderBindingTable(vkd, device, *(pipelineData.pipelines[0].pipeline), pipelineData.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1 );
	if(testParams.bottomType == BTT_AABBS)
		pipelineData.pipelines[0].hitShaderBindingTable					= rayTracingPipeline->createShaderBindingTable(vkd, device, *(pipelineData.pipelines[0].pipeline), pipelineData.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, 1 );
	else // testParams.bottomType == BTT_TRIANGLES
		pipelineData.pipelines[0].hitShaderBindingTable					= rayTracingPipeline->createShaderBindingTable(vkd, device, *(pipelineData.pipelines[0].pipeline), pipelineData.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1 );
	pipelineData.pipelines[0].missShaderBindingTable					= rayTracingPipeline->createShaderBindingTable(vkd, device, *(pipelineData.pipelines[0].pipeline), pipelineData.allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 3, 1 );
	pipelineData.pipelines[0].descriptorSet								= makeDescriptorSet(vkd, device, pipelineData.descriptorPool, pipelineData.descriptorSetLayout);
	pipelineData.pipelines[0].uniformBuffer								= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, pipelineData.allocator, uniformBufferCreateInfo, MemoryRequirement::HostVisible));
	pipelineData.pipelines[0].raygenShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, pipelineData.pipelines[0].raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	pipelineData.pipelines[0].missShaderBindingTableRegion				= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, pipelineData.pipelines[0].missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	pipelineData.pipelines[0].hitShaderBindingTableRegion				= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, pipelineData.pipelines[0].hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	pipelineData.pipelines[0].callableShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

}

bool TestAccelerationStructuresConfiguration::verifyImage (const std::vector<deUint32>&		captureResults,
														   const std::vector<deUint32>&		replayResults,
														   Context&							context,
														   TestParams&						testParams)
{
	DE_UNREF(context);

	deUint32							imageSize		= testParams.height * testParams.width;
	deUint32							failures		= 0;

	// verify results - each test case should generate checkerboard pattern
	for (deUint32 pos = 0; pos < imageSize; ++pos)
	{
		if (captureResults[pos] != replayResults[pos])
			failures++;
	}
	return failures == 0;
}

VkFormat TestAccelerationStructuresConfiguration::getResultImageFormat ()
{
	return VK_FORMAT_R32_UINT;
}

size_t TestAccelerationStructuresConfiguration::getResultImageFormatSize ()
{
	return sizeof(deUint32);
}

VkClearValue TestAccelerationStructuresConfiguration::getClearValue ()
{
	return makeClearValueColorU32(0xFF, 0u, 0u, 0u);
}

class RayTracingCaptureReplayTestCase : public TestCase
{
	public:
							RayTracingCaptureReplayTestCase				(tcu::TestContext& context, const char* name, const char* desc, const TestParams& data);
							~RayTracingCaptureReplayTestCase			(void);

	virtual void			checkSupport								(Context& context) const;
	virtual	void			initPrograms								(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance								(Context& context) const;
private:
	TestParams				m_data;
};

class RayTracingCaptureReplayTestInstance : public TestInstance
{
public:
																	RayTracingCaptureReplayTestInstance		(Context& context, const TestParams& data);
																	~RayTracingCaptureReplayTestInstance	(void);
	tcu::TestStatus													iterate									(void);

protected:
	std::vector<deUint32>											runTest									(bool replay);
private:
	TestParams														m_data;
	std::vector<VkDeviceAddress>									buildBLASAddresses;
	std::vector<VkDeviceAddress>									copyBLASAddresses;
	VkDeviceAddress													buildTLASAddress;
	VkDeviceAddress													copyTLASAddress;
};

RayTracingCaptureReplayTestCase::RayTracingCaptureReplayTestCase (tcu::TestContext& context, const char* name, const char* desc, const TestParams& data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

RayTracingCaptureReplayTestCase::~RayTracingCaptureReplayTestCase	(void)
{
}

void RayTracingCaptureReplayTestCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_buffer_device_address");
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

	const VkPhysicalDeviceRayTracingPipelineFeaturesKHR&	rayTracingPipelineFeaturesKHR		= context.getRayTracingPipelineFeatures();
	if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == DE_FALSE )
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

	if (m_data.testType == TEST_PIPELINE_BEFORE && rayTracingPipelineFeaturesKHR.rayTracingPipelineShaderGroupHandleCaptureReplayMixed == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipelineShaderGroupHandleCaptureReplayMixed");

	if (m_data.testType != TEST_ACCELERATION_STRUCTURES && rayTracingPipelineFeaturesKHR.rayTracingPipelineShaderGroupHandleCaptureReplay == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipelineShaderGroupHandleCaptureReplay");

	const VkPhysicalDeviceAccelerationStructureFeaturesKHR&	accelerationStructureFeaturesKHR	= context.getAccelerationStructureFeatures();
	if (accelerationStructureFeaturesKHR.accelerationStructure == DE_FALSE)
		TCU_THROW(TestError, "VK_KHR_ray_tracing_pipeline requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

	if (m_data.testType == TEST_ACCELERATION_STRUCTURES && accelerationStructureFeaturesKHR.accelerationStructureCaptureReplay == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructureCaptureReplay");

	if (m_data.testType == TEST_ACCELERATION_STRUCTURES && m_data.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR && accelerationStructureFeaturesKHR.accelerationStructureHostCommands == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructureHostCommands");

	const VkPhysicalDeviceBufferDeviceAddressFeatures&		bufferDeviceAddressFeatures = context.getBufferDeviceAddressFeatures();

	if (bufferDeviceAddressFeatures.bufferDeviceAddressCaptureReplay == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires bufferDeviceAddressFeatures.bufferDeviceAddressCaptureReplay");
}

void RayTracingCaptureReplayTestCase::initPrograms (SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions	buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadEXT uvec4 hitValue;\n"
			"layout(set = 0, binding = 0) uniform UniformParams\n"
			"{\n"
			"  uint targetLayer;\n"
			"} uniformParams;\n"
			"layout(r32ui, set = 0, binding = 1) uniform uimage3D result;\n"
			"layout(set = 0, binding = 2) uniform accelerationStructureEXT topLevelAS;\n"
			"\n"
			"void main()\n"
			"{\n"
			"  float tmin     = 0.0;\n"
			"  float tmax     = 1.0;\n"
			"  vec3  origin   = vec3(float(gl_LaunchIDEXT.x) + 0.5f, float(gl_LaunchIDEXT.y) + 0.5f, 0.5);\n"
			"  vec3  direct   = vec3(0.0, 0.0, -1.0);\n"
			"  hitValue       = uvec4(0,0,0,0);\n"
			"  traceRayEXT(topLevelAS, 0, 0xFF, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
			"  imageStore(result, ivec3(gl_LaunchIDEXT.xy, uniformParams.targetLayer), hitValue);\n"
			"}\n";
		programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	for (deUint32 shaderNdx = 0; shaderNdx < RTCR_SHADER_COUNT; ++shaderNdx)
	{
		deUint32 colorValue = 2 * (shaderNdx + 1);
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
			"void main()\n"
			"{\n"
			"  hitValue = uvec4(" << colorValue << ",0,0,1);\n"
			"}\n";
		std::stringstream shaderName;
		shaderName << "chit" << shaderNdx;

		programCollection.glslSources.add(shaderName.str()) << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
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

		programCollection.glslSources.add("isect") << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
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

		programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}
}

std::vector<std::string> removeExtensions (const std::vector<std::string>& a, const std::vector<const char*>& b)
{
	std::vector<std::string>	res;
	std::set<std::string>		removeExts	(b.begin(), b.end());

	for (std::vector<std::string>::const_iterator aIter = a.begin(); aIter != a.end(); ++aIter)
	{
		if (!de::contains(removeExts, *aIter))
			res.push_back(*aIter);
	}
	return res;
}

TestInstance* RayTracingCaptureReplayTestCase::createInstance (Context& context) const
{
	return new RayTracingCaptureReplayTestInstance(context, m_data);
}

RayTracingCaptureReplayTestInstance::RayTracingCaptureReplayTestInstance (Context& context, const TestParams& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
{
}

RayTracingCaptureReplayTestInstance::~RayTracingCaptureReplayTestInstance (void)
{
}

std::vector<deUint32> RayTracingCaptureReplayTestInstance::runTest(bool replay)
{
	const deUint32 NO_MATCH_FOUND = ~((deUint32)0);

	// For this test we need to create separate device with ray tracing features and buffer device address features enabled
	const PlatformInterface&				vkp									= m_context.getPlatformInterface();
	const InstanceInterface&				vki									= m_context.getInstanceInterface();
	const VkInstance						instance							= m_context.getInstance();
	const VkPhysicalDevice					physicalDevice						= m_context.getPhysicalDevice();
	const auto								validationEnabled					= m_context.getTestContext().getCommandLine().isValidationEnabled();

	VkQueue									queue								= DE_NULL;
	deUint32								queueFamilyIndex					= NO_MATCH_FOUND;

	std::vector<VkQueueFamilyProperties>	queueFamilyProperties = getPhysicalDeviceQueueFamilyProperties(vki, physicalDevice);
	for (deUint32 queueNdx = 0; queueNdx < queueFamilyProperties.size(); ++queueNdx)
	{
		if (queueFamilyProperties[queueNdx].queueFlags & ( VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT ))
		{
			if (queueFamilyIndex == NO_MATCH_FOUND)
				queueFamilyIndex = queueNdx;
		}
	}
	if (queueFamilyIndex == NO_MATCH_FOUND)
		TCU_THROW(NotSupportedError, "Could not create queue");

	const float								queuePriority						= 1.0f;
	VkDeviceQueueCreateInfo					queueInfo;
	deMemset(&queueInfo, 0, sizeof(queueInfo));
	queueInfo.sType																= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.pNext																= DE_NULL;
	queueInfo.flags																= (VkDeviceQueueCreateFlags)0u;
	queueInfo.queueFamilyIndex													= queueFamilyIndex;
	queueInfo.queueCount														= 1;
	queueInfo.pQueuePriorities													= &queuePriority;

	VkPhysicalDeviceRayTracingPipelineFeaturesKHR		rayTracingFeaturesKHR;
	rayTracingFeaturesKHR.sType													= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	rayTracingFeaturesKHR.pNext													= DE_NULL;

	VkPhysicalDeviceAccelerationStructureFeaturesKHR	accelerationStructureFeaturesKHR;
	accelerationStructureFeaturesKHR.sType										= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	accelerationStructureFeaturesKHR.pNext										= &rayTracingFeaturesKHR;

	VkPhysicalDeviceBufferDeviceAddressFeatures			bufferDeviceAddressFeatures;
	bufferDeviceAddressFeatures.sType											= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
	bufferDeviceAddressFeatures.pNext											= &accelerationStructureFeaturesKHR;

	VkPhysicalDeviceFeatures2				deviceFeatures2;
	deviceFeatures2.sType														= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	deviceFeatures2.pNext														= &bufferDeviceAddressFeatures;
	vki.getPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);

	// skip core device extensions according to API version
	std::vector<const char*>				coreExtensions;
	getCoreDeviceExtensions(m_context.getUsedApiVersion(), coreExtensions);
	std::vector<std::string>				nonCoreDeviceExtensions				(removeExtensions(m_context.getDeviceExtensions(), coreExtensions));
	std::vector<const char*>				nonCoreDeviceExtensionsC;

	// ppEnabledExtensionNames must not contain both VK_KHR_buffer_device_address and VK_EXT_buffer_device_address
	if ( ( de::contains(begin(coreExtensions), end(coreExtensions), "VK_KHR_buffer_device_address") ||
		   de::contains(begin(nonCoreDeviceExtensions), end(nonCoreDeviceExtensions), "VK_KHR_buffer_device_address") ) &&
		 de::contains(begin(nonCoreDeviceExtensions), end(nonCoreDeviceExtensions), "VK_EXT_buffer_device_address") )
		std::for_each(begin(nonCoreDeviceExtensions), end(nonCoreDeviceExtensions), [&nonCoreDeviceExtensionsC](const std::string& text) { if (text != "VK_EXT_buffer_device_address") nonCoreDeviceExtensionsC.push_back(text.c_str()); });
	else
		std::for_each(begin(nonCoreDeviceExtensions), end(nonCoreDeviceExtensions), [&nonCoreDeviceExtensionsC](const std::string& text) { nonCoreDeviceExtensionsC.push_back(text.c_str()); });

	VkDeviceCreateInfo						deviceInfo;
	deMemset(&deviceInfo, 0, sizeof(deviceInfo));
	deviceInfo.sType															= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pNext															= &deviceFeatures2;
	deviceInfo.enabledExtensionCount											= deUint32(nonCoreDeviceExtensionsC.size());
	deviceInfo.ppEnabledExtensionNames											= nonCoreDeviceExtensionsC.data();
	deviceInfo.enabledLayerCount												= 0u;
	deviceInfo.ppEnabledLayerNames												= DE_NULL;
	deviceInfo.pEnabledFeatures													= DE_NULL;
	deviceInfo.queueCreateInfoCount												= 1;
	deviceInfo.pQueueCreateInfos												= &queueInfo;
	Move<VkDevice>							testDevice							= createCustomDevice(validationEnabled, vkp, m_context.getInstance(), vki, physicalDevice, &deviceInfo);
	VkDevice								device								= *testDevice;
	DeviceDriver							vkd									(vkp, instance, device);

	vkd.getDeviceQueue(device, queueFamilyIndex, 0, &queue);

	// create memory allocator for new VkDevice
	VkPhysicalDeviceMemoryProperties		memoryProperties					= getPhysicalDeviceMemoryProperties(vki, physicalDevice);
	de::UniquePtr<vk::Allocator>			allocator							(new SimpleAllocator(vkd, device, memoryProperties));

	// Create common pipeline layout for all raytracing pipelines
	const Move<VkDescriptorSetLayout>		descriptorSetLayout					= DescriptorSetLayoutBuilder()
																					.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ALL_RAY_TRACING_STAGES)
																					.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, ALL_RAY_TRACING_STAGES)
																					.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, ALL_RAY_TRACING_STAGES)
																					.build(vkd, device);
	deUint32								pipelineCount						= (!replay || ( m_data.testType == TEST_PIPELINE_SINGLE) || (m_data.testType == TEST_ACCELERATION_STRUCTURES)) ? 1u : 2u;
	const Move<VkDescriptorPool>			descriptorPool						= DescriptorPoolBuilder()
																						.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, pipelineCount)
																						.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, pipelineCount)
																						.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, pipelineCount)
																						.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, pipelineCount);
	const Move<VkPipelineLayout>			pipelineLayout						= makePipelineLayout(vkd, device, descriptorSetLayout.get());

	// All pipelines will be using the same set of shaders and shader groups.
	// Single RayTracingPipeline object will be enough to define it
	de::MovePtr<RayTracingPipeline>			rayTracingPipeline					= de::newMovePtr<RayTracingPipeline>();
	m_data.testConfiguration->initRayTracingShaders(rayTracingPipeline, m_context, vkd, device, m_data, replay);

	// Capture phase ( replay==false ):
	// - TEST_ACCELERATION_STRUCTURES:
	//   - build/copy/compact/serialize structure, record addresses
	// - TEST_PIPELINE_SINGLE:
	// - TEST_PIPELINE_AFTER:
	// - TEST_PIPELINE_BEFORE:
	//   - single pipeline records addresses and fills test data
	// Replay phase ( replay==true ):
	// - TEST_ACCELERATION_STRUCTURES:
	//   - build/copy/compact/serialize structure with addresses captured previously
	// - TEST_PIPELINE_SINGLE:
	//   - single pipeline with addresses captured previously - writes into first image layer
	// - TEST_PIPELINE_AFTER:
	//   - first pipeline with addresses captured previously - writes into first image layer
	//   - second pipeline created without captured addresses - writes into second image layer
	// - TEST_PIPELINE_BEFORE:
	//   - first pipeline created without captured addresses - writes into first image layer
	//   - second pipeline with addresses captured previously - writes into second image layer
	//
	// Comparing results in all tests: all layers must be identical to the layer from capture phase

	PipelineData							pipelineData(*allocator);
	pipelineData.pipelineLayout													= *pipelineLayout;
	pipelineData.descriptorSetLayout											= *descriptorSetLayout;
	pipelineData.descriptorPool													= *descriptorPool;
	const deUint32							shaderGroupHandleSize				= getShaderGroupSize(vki, physicalDevice);
	const deUint32							shaderGroupBaseAlignment			= getShaderGroupBaseAlignment(vki, physicalDevice);
	m_data.testConfiguration->initShaderBindingTables(rayTracingPipeline, m_context, vkd, device, m_data, shaderGroupHandleSize, shaderGroupBaseAlignment, pipelineData, replay);

	const VkFormat							imageFormat							= m_data.testConfiguration->getResultImageFormat();
	const VkImageCreateInfo					imageCreateInfo						= makeImageCreateInfo(m_data.width, m_data.height, pipelineCount, imageFormat);
	const VkImageSubresourceRange			imageSubresourceRange				= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0, 1u);
	const de::MovePtr<ImageWithMemory>		image								= de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, *allocator, imageCreateInfo, MemoryRequirement::Any));
	const Move<VkImageView>					imageView							= makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_3D, imageFormat, imageSubresourceRange);
	const VkDescriptorImageInfo				descriptorImageInfo					= makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);

	const deUint32							pixelCount							= m_data.width * m_data.height * pipelineCount;
	const VkBufferCreateInfo				resultBufferCreateInfo				= makeBufferCreateInfo(pixelCount*m_data.testConfiguration->getResultImageFormatSize(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkImageSubresourceLayers			resultBufferImageSubresourceLayers	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const VkBufferImageCopy					resultBufferImageRegion				= makeBufferImageCopy(makeExtent3D(m_data.width, m_data.height, pipelineCount), resultBufferImageSubresourceLayers);
	de::MovePtr<BufferWithMemory>			resultBuffer						= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, *allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));

	const Move<VkCommandPool>				cmdPool								= createCommandPool(vkd, device, 0, queueFamilyIndex);
	const Move<VkCommandBuffer>				cmdBuffer							= allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	std::vector<de::SharedPtr<BottomLevelAccelerationStructure>>	bottomLevelAccelerationStructures;
	de::MovePtr<TopLevelAccelerationStructure>						topLevelAccelerationStructure;
	std::vector<de::SharedPtr<BottomLevelAccelerationStructure>>	bottomLevelAccelerationStructureCopies;
	de::MovePtr<TopLevelAccelerationStructure>						topLevelAccelerationStructureCopy;
	std::vector<de::SharedPtr<SerialStorage>>						bottomSerialized;
	std::vector<de::SharedPtr<SerialStorage>>						topSerialized;
	Move<VkQueryPool>												m_queryPoolCompact;
	Move<VkQueryPool>												m_queryPoolSerial;

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

		// build bottom level acceleration structures and their copies ( only when we are testing copying bottom level acceleration structures )
		bool									bottomCompact		= m_data.testType == TEST_ACCELERATION_STRUCTURES && m_data.operationType == OP_COMPACT && m_data.operationTarget == OT_BOTTOM_ACCELERATION;
		bool									bottomSerial		= m_data.testType == TEST_ACCELERATION_STRUCTURES && m_data.operationType == OP_SERIALIZE && m_data.operationTarget == OT_BOTTOM_ACCELERATION;
		bottomLevelAccelerationStructures							= m_data.testConfiguration->initBottomAccelerationStructures(m_context, m_data);
		VkBuildAccelerationStructureFlagsKHR	allowCompactionFlag	= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
		VkBuildAccelerationStructureFlagsKHR	emptyCompactionFlag	= VkBuildAccelerationStructureFlagsKHR(0);
		VkBuildAccelerationStructureFlagsKHR	bottomBuildFlags	= (bottomCompact ? allowCompactionFlag : emptyCompactionFlag);
		std::vector<VkAccelerationStructureKHR>	accelerationStructureHandles;
		std::vector<VkDeviceSize>				bottomBlasCompactSize;
		std::vector<VkDeviceSize>				bottomBlasSerialSize;

		for (size_t idx=0; idx < bottomLevelAccelerationStructures.size(); ++idx)
		{
			bottomLevelAccelerationStructures[idx]->setBuildFlags	(bottomBuildFlags);
			bottomLevelAccelerationStructures[idx]->setBuildType	(m_data.buildType);
			VkDeviceAddress	deviceAddress							= ( m_data.testType == TEST_ACCELERATION_STRUCTURES && replay ) ? buildBLASAddresses[idx] : 0u;
			if ( m_data.testType == TEST_ACCELERATION_STRUCTURES && replay )
				bottomLevelAccelerationStructures[idx]->setCreateFlags	( VK_ACCELERATION_STRUCTURE_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_KHR);
			bottomLevelAccelerationStructures[idx]->createAndBuild	(vkd, device, *cmdBuffer, *allocator, deviceAddress);
			accelerationStructureHandles.push_back					(*(bottomLevelAccelerationStructures[idx]->getPtr()));
			if (m_data.testType == TEST_ACCELERATION_STRUCTURES && !replay)
				buildBLASAddresses.push_back(getAccelerationStructureDeviceAddress(vkd, device, *(bottomLevelAccelerationStructures[idx]->getPtr())));
		}

		if (m_data.operationType == OP_COMPACT)
		{
			deUint32 queryCount	= (m_data.operationTarget == OT_BOTTOM_ACCELERATION) ? deUint32(bottomLevelAccelerationStructures.size()) : 1u;
			if (m_data.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
				m_queryPoolCompact	= makeQueryPool(vkd, device, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryCount);
			if (m_data.operationTarget == OT_BOTTOM_ACCELERATION)
				queryAccelerationStructureSize(vkd, device, *cmdBuffer, accelerationStructureHandles, m_data.buildType, m_queryPoolCompact.get(), VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, 0u, bottomBlasCompactSize);
		}
		if (m_data.operationType == OP_SERIALIZE)
		{
			deUint32 queryCount	= (m_data.operationTarget == OT_BOTTOM_ACCELERATION) ? deUint32(bottomLevelAccelerationStructures.size()) : 1u;
			if (m_data.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
				m_queryPoolSerial	= makeQueryPool(vkd, device, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR, queryCount);
			if (m_data.operationTarget == OT_BOTTOM_ACCELERATION)
				queryAccelerationStructureSize(vkd, device, *cmdBuffer, accelerationStructureHandles, m_data.buildType, m_queryPoolSerial.get(), VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR, 0u, bottomBlasSerialSize);
		}

		// if AS is built on GPU and we are planning to make a compact copy of it or serialize / deserialize it - we have to have download query results to CPU
		if ((m_data.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR) && (bottomCompact || bottomSerial))
		{
			endCommandBuffer(vkd, *cmdBuffer);

			submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

			if (bottomCompact)
				VK_CHECK(vkd.getQueryPoolResults(device, *m_queryPoolCompact, 0u, deUint32(bottomBlasCompactSize.size()), sizeof(VkDeviceSize) * bottomBlasCompactSize.size(), bottomBlasCompactSize.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
			if (bottomSerial)
				VK_CHECK(vkd.getQueryPoolResults(device, *m_queryPoolSerial, 0u, deUint32(bottomBlasSerialSize.size()), sizeof(VkDeviceSize) * bottomBlasSerialSize.size(), bottomBlasSerialSize.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));

			vkd.resetCommandPool(device, *cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
			beginCommandBuffer(vkd, *cmdBuffer, 0u);
		}

		auto bottomLevelAccelerationStructuresPtr								= &bottomLevelAccelerationStructures;
		if (m_data.operationType != OP_NONE && m_data.operationTarget == OT_BOTTOM_ACCELERATION)
		{
			switch (m_data.operationType)
			{
			case OP_COPY:
			{
				for (size_t idx = 0; idx < bottomLevelAccelerationStructures.size(); ++idx)
				{
					de::MovePtr<BottomLevelAccelerationStructure> asCopy = makeBottomLevelAccelerationStructure();
					asCopy->setBuildType(m_data.buildType);
					VkDeviceAddress	deviceAddress = replay ? copyBLASAddresses[idx] : 0u;
					if (replay)
						asCopy->setCreateFlags(VK_ACCELERATION_STRUCTURE_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_KHR);
					asCopy->createAndCopyFrom(vkd, device, *cmdBuffer, *allocator, bottomLevelAccelerationStructures[idx].get(), 0u, deviceAddress);
					bottomLevelAccelerationStructureCopies.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(asCopy.release()));
					if (!replay)
						copyBLASAddresses.push_back(getAccelerationStructureDeviceAddress(vkd, device, *(bottomLevelAccelerationStructureCopies[idx]->getPtr())));
				}
				break;
			}
			case OP_COMPACT:
			{
				for (size_t idx = 0; idx < bottomLevelAccelerationStructures.size(); ++idx)
				{
					de::MovePtr<BottomLevelAccelerationStructure> asCopy = makeBottomLevelAccelerationStructure();
					asCopy->setBuildType(m_data.buildType);
					VkDeviceAddress	deviceAddress = replay ? copyBLASAddresses[idx] : 0u;
					if (replay)
						asCopy->setCreateFlags(VK_ACCELERATION_STRUCTURE_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_KHR);
					asCopy->createAndCopyFrom(vkd, device, *cmdBuffer, *allocator, bottomLevelAccelerationStructures[idx].get(), bottomBlasCompactSize[idx], deviceAddress);
					bottomLevelAccelerationStructureCopies.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(asCopy.release()));
					if (!replay)
						copyBLASAddresses.push_back(getAccelerationStructureDeviceAddress(vkd, device, *(bottomLevelAccelerationStructureCopies[idx]->getPtr())));
				}
				break;
			}
			case OP_SERIALIZE:
			{
				for (size_t idx = 0; idx < bottomLevelAccelerationStructures.size(); ++idx)
				{
					de::SharedPtr<SerialStorage> storage(new SerialStorage(vkd, device, *allocator, m_data.buildType, bottomBlasSerialSize[idx]));
					bottomLevelAccelerationStructures[idx]->serialize(vkd, device, *cmdBuffer, storage.get());
					bottomSerialized.push_back(storage);

					if (m_data.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
					{
						endCommandBuffer(vkd, *cmdBuffer);

						submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

						vkd.resetCommandPool(device, *cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
						beginCommandBuffer(vkd, *cmdBuffer, 0u);
					}

					de::MovePtr<BottomLevelAccelerationStructure> asCopy = makeBottomLevelAccelerationStructure();
					asCopy->setBuildType(m_data.buildType);
					VkDeviceAddress	deviceAddress = replay ? copyBLASAddresses[idx] : 0u;
					if (replay)
						asCopy->setCreateFlags(VK_ACCELERATION_STRUCTURE_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_KHR);
					asCopy->createAndDeserializeFrom(vkd, device, *cmdBuffer, *allocator, storage.get(), deviceAddress);
					bottomLevelAccelerationStructureCopies.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(asCopy.release()));
					if (!replay)
						copyBLASAddresses.push_back(getAccelerationStructureDeviceAddress(vkd, device, *(bottomLevelAccelerationStructureCopies[idx]->getPtr())));
				}
				break;
			}
			default:
				DE_ASSERT(DE_FALSE);
			}
			bottomLevelAccelerationStructuresPtr = &bottomLevelAccelerationStructureCopies;
		}

		// build top level acceleration structures and their copies ( only when we are testing copying top level acceleration structures )
		bool									topCompact			= m_data.testType == TEST_ACCELERATION_STRUCTURES && m_data.operationType == OP_COMPACT && m_data.operationTarget == OT_TOP_ACCELERATION;
		bool									topSerial			= m_data.testType == TEST_ACCELERATION_STRUCTURES && m_data.operationType == OP_SERIALIZE && m_data.operationTarget == OT_TOP_ACCELERATION;
		VkBuildAccelerationStructureFlagsKHR	topBuildFlags		= (topCompact ? allowCompactionFlag : emptyCompactionFlag);
		std::vector<VkAccelerationStructureKHR> topLevelStructureHandles;
		std::vector<VkDeviceSize>				topBlasCompactSize;
		std::vector<VkDeviceSize>				topBlasSerialSize;

		topLevelAccelerationStructure								= m_data.testConfiguration->initTopAccelerationStructure(m_context, m_data, *bottomLevelAccelerationStructuresPtr);
		topLevelAccelerationStructure->setBuildFlags				(topBuildFlags);
		topLevelAccelerationStructure->setBuildType					(m_data.buildType);
		VkDeviceAddress							deviceAddressBuild	= ( m_data.testType == TEST_ACCELERATION_STRUCTURES && replay ) ? buildTLASAddress : 0u;
		if (m_data.testType == TEST_ACCELERATION_STRUCTURES && replay)
			topLevelAccelerationStructure->setCreateFlags			(VK_ACCELERATION_STRUCTURE_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_KHR);
		topLevelAccelerationStructure->createAndBuild				(vkd, device, *cmdBuffer, *allocator, deviceAddressBuild);
		topLevelStructureHandles.push_back							(*(topLevelAccelerationStructure->getPtr()));
		if (m_data.testType == TEST_ACCELERATION_STRUCTURES && !replay)
			buildTLASAddress = getAccelerationStructureDeviceAddress(vkd, device, *(topLevelAccelerationStructure->getPtr()));

		if (topCompact)
			queryAccelerationStructureSize(vkd, device, *cmdBuffer, topLevelStructureHandles, m_data.buildType, m_queryPoolCompact.get(), VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, 0u, topBlasCompactSize);
		if (topSerial)
			queryAccelerationStructureSize(vkd, device, *cmdBuffer, topLevelStructureHandles, m_data.buildType, m_queryPoolSerial.get(), VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR, 0u, topBlasSerialSize);

		// if AS is built on GPU and we are planning to make a compact copy of it or serialize / deserialize it - we have to have download query results to CPU
		if ((m_data.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR) && (topCompact || topSerial))
		{
			endCommandBuffer(vkd, *cmdBuffer);

			submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

			if (topCompact)
				VK_CHECK(vkd.getQueryPoolResults(device, *m_queryPoolCompact, 0u, deUint32(topBlasCompactSize.size()), sizeof(VkDeviceSize) * topBlasCompactSize.size(), topBlasCompactSize.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
			if (topSerial)
				VK_CHECK(vkd.getQueryPoolResults(device, *m_queryPoolSerial, 0u, deUint32(topBlasSerialSize.size()), sizeof(VkDeviceSize) * topBlasSerialSize.size(), topBlasSerialSize.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));

			vkd.resetCommandPool(device, *cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
			beginCommandBuffer(vkd, *cmdBuffer, 0u);
		}

		const TopLevelAccelerationStructure*			topLevelRayTracedPtr	= topLevelAccelerationStructure.get();
		if (m_data.operationType != OP_NONE && m_data.operationTarget == OT_TOP_ACCELERATION)
		{
			switch (m_data.operationType)
			{
				case OP_COPY:
				{
					topLevelAccelerationStructureCopy				= makeTopLevelAccelerationStructure();
					topLevelAccelerationStructureCopy->setBuildType	(m_data.buildType);
					VkDeviceAddress			deviceAddress			= replay ? copyTLASAddress : 0u;
					if(replay)
						topLevelAccelerationStructureCopy->setCreateFlags(VK_ACCELERATION_STRUCTURE_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_KHR);
					topLevelAccelerationStructureCopy->createAndCopyFrom(vkd, device, *cmdBuffer, *allocator, topLevelAccelerationStructure.get(), 0u, deviceAddress);
					if (!replay)
						copyTLASAddress = getAccelerationStructureDeviceAddress(vkd, device, *(topLevelAccelerationStructureCopy->getPtr()));
					break;
				}
				case OP_COMPACT:
				{
					topLevelAccelerationStructureCopy				= makeTopLevelAccelerationStructure();
					topLevelAccelerationStructureCopy->setBuildType	(m_data.buildType);
					VkDeviceAddress			deviceAddress			= replay ? copyTLASAddress : 0u;
					if(replay)
						topLevelAccelerationStructureCopy->setCreateFlags(VK_ACCELERATION_STRUCTURE_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_KHR);
					topLevelAccelerationStructureCopy->createAndCopyFrom(vkd, device, *cmdBuffer, *allocator, topLevelAccelerationStructure.get(), topBlasCompactSize[0], deviceAddress);
					if (!replay)
						copyTLASAddress = getAccelerationStructureDeviceAddress(vkd, device, *(topLevelAccelerationStructureCopy->getPtr()));
					break;
				}
				case OP_SERIALIZE:
				{
					de::SharedPtr<SerialStorage> storage( new SerialStorage(vkd, device, *allocator, m_data.buildType, topBlasSerialSize[0]));
					topLevelAccelerationStructure->serialize(vkd, device, *cmdBuffer, storage.get());
					topSerialized.push_back(storage);

					if (m_data.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
					{
						endCommandBuffer(vkd, *cmdBuffer);

						submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

						vkd.resetCommandPool(device, *cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
						beginCommandBuffer(vkd, *cmdBuffer, 0u);
					}

					topLevelAccelerationStructureCopy = makeTopLevelAccelerationStructure();
					topLevelAccelerationStructureCopy->setBuildType(m_data.buildType);
					VkDeviceAddress			deviceAddress			= replay ? copyTLASAddress : 0u;
					if(replay)
						topLevelAccelerationStructureCopy->setCreateFlags(VK_ACCELERATION_STRUCTURE_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_KHR);
					topLevelAccelerationStructureCopy->createAndDeserializeFrom(vkd, device, *cmdBuffer, *allocator, storage.get(), deviceAddress);
					if (!replay)
						copyTLASAddress = getAccelerationStructureDeviceAddress(vkd, device, *(topLevelAccelerationStructureCopy->getPtr()));
					break;
				}
				default:
					DE_ASSERT(DE_FALSE);
			}
			topLevelRayTracedPtr = topLevelAccelerationStructureCopy.get();
		}

		// copy layer index into uniform buffer
		for (deUint32 i = 0; i < pipelineCount; ++i)
		{
			deMemcpy(pipelineData.pipelines[i].uniformBuffer->getAllocation().getHostPtr(), &i, sizeof(deUint32));
			flushMappedMemoryRange(vkd, device, pipelineData.pipelines[i].uniformBuffer->getAllocation().getMemory(), pipelineData.pipelines[i].uniformBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);
		}

		const VkMemoryBarrier				preTraceMemoryBarrier				= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, &preTraceMemoryBarrier);

		VkWriteDescriptorSetAccelerationStructureKHR							accelerationStructureWriteDescriptorSet	=
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	//  VkStructureType						sType;
			DE_NULL,															//  const void*							pNext;
			1u,																	//  deUint32							accelerationStructureCount;
			topLevelRayTracedPtr->getPtr()										//  const VkAccelerationStructureKHR*	pAccelerationStructures;
		};

		for( deUint32 i=0; i<pipelineCount; ++i )
		{
			VkDescriptorBufferInfo	uniformBufferInfo = makeDescriptorBufferInfo(pipelineData.pipelines[i].uniformBuffer->get(), 0ull, sizeof(deUint32));

			DescriptorSetUpdateBuilder()
				.writeSingle(*(pipelineData.pipelines[i].descriptorSet), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBufferInfo)
				.writeSingle(*(pipelineData.pipelines[i].descriptorSet), DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfo)
				.writeSingle(*(pipelineData.pipelines[i].descriptorSet), DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelerationStructureWriteDescriptorSet)
				.update(vkd, device);

			vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipelineLayout, 0, 1, &(pipelineData.pipelines[i].descriptorSet.get()), 0, DE_NULL);

			vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *(pipelineData.pipelines[i].pipeline));

			cmdTraceRays(vkd,
				*cmdBuffer,
				&(pipelineData.pipelines[i].raygenShaderBindingTableRegion),
				&(pipelineData.pipelines[i].missShaderBindingTableRegion),
				&(pipelineData.pipelines[i].hitShaderBindingTableRegion),
				&(pipelineData.pipelines[i].callableShaderBindingTableRegion),
				m_data.width, m_data.height, 1);
		}

		const VkMemoryBarrier													postTraceMemoryBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);

		vkd.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **resultBuffer, 1u, &resultBufferImageRegion);
	}
	endCommandBuffer(vkd, *cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

	invalidateMappedMemoryRange(vkd, device, resultBuffer->getAllocation().getMemory(), resultBuffer->getAllocation().getOffset(), pixelCount * sizeof(deUint32));

	std::vector<deUint32> result(pixelCount);
	deMemcpy(result.data(), resultBuffer->getAllocation().getHostPtr(), pixelCount * sizeof(deUint32));
	return result;
}

tcu::TestStatus RayTracingCaptureReplayTestInstance::iterate (void)
{
	// run test capturing different elements
	const std::vector<deUint32>	captureResults		= runTest(false);

	// run test that replays different elements
	const std::vector<deUint32>	replayResults		= runTest(true);

	if (!m_data.testConfiguration->verifyImage(captureResults, replayResults, m_context, m_data))
		return tcu::TestStatus::fail("Fail");
	return tcu::TestStatus::pass("Pass");
}

}	// anonymous

void addReplayShaderBindingTablesTests(tcu::TestCaseGroup* group)
{
	struct
	{
		SBTReplayTestType	testType;
		const char*			name;
		const char*			description;
	} testTypes[] =
	{
		{ TEST_PIPELINE_SINGLE,		"pipeline_single",			"Capture-replay scenario with single captured pipeline" },
		{ TEST_PIPELINE_AFTER	,	"pipeline_after_captured",	"Not captured pipeline created after captured one" },
		{ TEST_PIPELINE_BEFORE,		"pipeline_before_captured",	"Not captured pipeline created before captured one" },
	};

	for (size_t testTypeNdx = 0; testTypeNdx < DE_LENGTH_OF_ARRAY(testTypes); ++testTypeNdx)
	{
		TestParams testParams
		{
			testTypes[testTypeNdx].testType,
			OT_NONE,
			OP_NONE,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			BTT_TRIANGLES,
			TTT_IDENTICAL_INSTANCES,
			RTCR_DEFAULT_SIZE,
			RTCR_DEFAULT_SIZE,
			de::SharedPtr<TestConfiguration>(new TestShaderBindingTablesConfiguration())
		};
		group->addChild(new RayTracingCaptureReplayTestCase(group->getTestContext(), testTypes[testTypeNdx].name, testTypes[testTypeNdx].description, testParams));
	}
}

void addReplayAccelerationStruturesTests(tcu::TestCaseGroup* group)
{
	struct
	{
		ASOperationType										operationType;
		const char*											name;
	} operationTypes[] =
	{
		{ OP_NONE,											"building"		},
		{ OP_COPY,											"copy"			},
		{ OP_COMPACT,										"compaction"	},
		{ OP_SERIALIZE,										"serialization"	},
	};

	struct
	{
		vk::VkAccelerationStructureBuildTypeKHR				buildType;
		const char*											name;
	} buildTypes[] =
	{
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR,	"cpu_built"	},
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,	"gpu_built"	},
	};

	struct
	{
		ASOperationTarget									operationTarget;
		const char*											name;
	} operationTargets[] =
	{
		{ OT_TOP_ACCELERATION,								"top_acceleration_structure"		},
		{ OT_BOTTOM_ACCELERATION,							"bottom_acceleration_structure"	},
	};

	struct
	{
		ASBottomTestType									testType;
		const char*											name;
	} bottomTestTypes[] =
	{
		{ BTT_TRIANGLES,									"triangles" },
		{ BTT_AABBS,										"aabbs" },
	};

	for (size_t operationTypeNdx = 0; operationTypeNdx < DE_LENGTH_OF_ARRAY(operationTypes); ++operationTypeNdx)
	{
		de::MovePtr<tcu::TestCaseGroup> operationTypeGroup(new tcu::TestCaseGroup(group->getTestContext(), operationTypes[operationTypeNdx].name, ""));

		for (size_t buildTypeNdx = 0; buildTypeNdx < DE_LENGTH_OF_ARRAY(buildTypes); ++buildTypeNdx)
		{
			de::MovePtr<tcu::TestCaseGroup> buildGroup(new tcu::TestCaseGroup(group->getTestContext(), buildTypes[buildTypeNdx].name, ""));

			for (size_t operationTargetNdx = 0; operationTargetNdx < DE_LENGTH_OF_ARRAY(operationTargets); ++operationTargetNdx)
			{
				de::MovePtr<tcu::TestCaseGroup> operationTargetGroup(new tcu::TestCaseGroup(group->getTestContext(), operationTargets[operationTargetNdx].name, ""));

				for (size_t testTypeNdx = 0; testTypeNdx < DE_LENGTH_OF_ARRAY(bottomTestTypes); ++testTypeNdx)
				{
					ASTopTestType topTest = (operationTargets[operationTargetNdx].operationTarget == OT_TOP_ACCELERATION) ? TTT_DIFFERENT_INSTANCES : TTT_IDENTICAL_INSTANCES;

					TestParams testParams
					{
						TEST_ACCELERATION_STRUCTURES,
						operationTargets[operationTargetNdx].operationTarget,
						operationTypes[operationTypeNdx].operationType,
						buildTypes[buildTypeNdx].buildType,
						bottomTestTypes[testTypeNdx].testType,
						topTest,
						RTCR_DEFAULT_SIZE,
						RTCR_DEFAULT_SIZE,
						de::SharedPtr<TestConfiguration>(new TestAccelerationStructuresConfiguration())
					};
					operationTargetGroup->addChild(new RayTracingCaptureReplayTestCase(group->getTestContext(), bottomTestTypes[testTypeNdx].name, "", testParams));
				}
				buildGroup->addChild(operationTargetGroup.release());
			}
			operationTypeGroup->addChild(buildGroup.release());
		}
		group->addChild(operationTypeGroup.release());
	}
}

tcu::TestCaseGroup*	createCaptureReplayTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "capture_replay", "Capture-replay capabilities"));

	addTestGroup(group.get(), "shader_binding_tables", "Test replaying shader binding tables", addReplayShaderBindingTablesTests);
	addTestGroup(group.get(), "acceleration_structures", "Test replaying acceleration structure", addReplayAccelerationStruturesTests);

	return group.release();
}

}	// RayTracing

}	// vkt
