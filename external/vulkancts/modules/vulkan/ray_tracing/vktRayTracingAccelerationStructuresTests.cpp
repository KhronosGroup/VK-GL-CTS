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
 * \brief Ray Tracing Acceleration Structures tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingAccelerationStructuresTests.hpp"

#include "vkDefs.hpp"
#include "deClock.h"

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
#include "tcuVectorUtil.hpp"
#include "tcuTexture.hpp"
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


enum BottomTestType
{
	BTT_TRIANGLES,
	BTT_AABBS
};

enum TopTestType
{
	TTT_IDENTICAL_INSTANCES,
	TTT_DIFFERENT_INSTANCES
};

enum OperationTarget
{
	OT_NONE,
	OT_TOP_ACCELERATION,
	OT_BOTTOM_ACCELERATION
};

enum OperationType
{
	OP_NONE,
	OP_COPY,
	OP_COMPACT,
	OP_SERIALIZE
};

static const deUint32 RTAS_DEFAULT_SIZE = 8u;

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
																												 de::MovePtr<BufferWithMemory>&		missShaderBindingTable) = 0;
	virtual bool															verifyImage							(BufferWithMemory*					resultBuffer,
																												 Context&							context,
																												 TestParams&						testParams) = 0;
	virtual VkFormat														getResultImageFormat				() = 0;
	virtual size_t															getResultImageFormatSize			() = 0;
	virtual VkClearValue													getClearValue						() = 0;
};

struct TestParams
{
	vk::VkAccelerationStructureBuildTypeKHR	buildType;		// are we making AS on CPU or GPU
	VkFormat								vertexFormat;
	VkIndexType								indexType;
	BottomTestType							bottomTestType; // what kind of geometry is stored in bottom AS
	bool									bottomUsesAOP;	// does bottom AS use arrays, or arrays of pointers
	TopTestType								topTestType;	// If instances are identical then bottom geometries must have different vertices/aabbs
	bool									topUsesAOP;		// does top AS use arrays, or arrays of pointers
	VkBuildAccelerationStructureFlagsKHR	buildFlags;
	OperationTarget							operationTarget;
	OperationType							operationType;
	deUint32								width;
	deUint32								height;
	de::SharedPtr<TestConfiguration>		testConfiguration;
	deUint32								workerThreadsCount;
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
	const VkImageCreateInfo			imageCreateInfo			=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,																// VkStructureType			sType;
		DE_NULL,																							// const void*				pNext;
		(VkImageCreateFlags)0u,																				// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,																					// VkImageType				imageType;
		format,																								// VkFormat					format;
		makeExtent3D(width, height, 1u),																	// VkExtent3D				extent;
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

class CheckerboardConfiguration : public TestConfiguration
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
																										 de::MovePtr<BufferWithMemory>&		missShaderBindingTable) override;
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
	std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >	result;

	tcu::Vec3 v0(0.0, 1.0, 0.0);
	tcu::Vec3 v1(0.0, 0.0, 0.0);
	tcu::Vec3 v2(1.0, 1.0, 0.0);
	tcu::Vec3 v3(1.0, 0.0, 0.0);

	if (testParams.topTestType == TTT_DIFFERENT_INSTANCES)
	{
		de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure = makeBottomLevelAccelerationStructure();
		bottomLevelAccelerationStructure->setGeometryCount(1u);
		de::SharedPtr<RaytracedGeometryBase> geometry;
		if (testParams.bottomTestType == BTT_TRIANGLES)
		{
			geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_TRIANGLES_KHR, testParams.vertexFormat, testParams.indexType);
			if (testParams.indexType == VK_INDEX_TYPE_NONE_KHR)
			{
				geometry->addVertex(v0);
				geometry->addVertex(v1);
				geometry->addVertex(v2);
				geometry->addVertex(v2);
				geometry->addVertex(v1);
				geometry->addVertex(v3);
			}
			else // m_data.indexType != VK_INDEX_TYPE_NONE_KHR
			{
				geometry->addVertex(v0);
				geometry->addVertex(v1);
				geometry->addVertex(v2);
				geometry->addVertex(v3);

				geometry->addIndex(0);
				geometry->addIndex(1);
				geometry->addIndex(2);
				geometry->addIndex(2);
				geometry->addIndex(1);
				geometry->addIndex(3);
			}
		}
		else // m_data.bottomTestType == BTT_AABBS
		{
			geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_AABBS_KHR, testParams.vertexFormat, testParams.indexType);
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
			if (testParams.bottomTestType == BTT_TRIANGLES)
			{
				geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_TRIANGLES_KHR, testParams.vertexFormat, testParams.indexType);
				if (testParams.indexType == VK_INDEX_TYPE_NONE_KHR)
				{
					geometry->addVertex(xyz + v0);
					geometry->addVertex(xyz + v1);
					geometry->addVertex(xyz + v2);
					geometry->addVertex(xyz + v2);
					geometry->addVertex(xyz + v1);
					geometry->addVertex(xyz + v3);
				}
				else
				{
					geometry->addVertex(xyz + v0);
					geometry->addVertex(xyz + v1);
					geometry->addVertex(xyz + v2);
					geometry->addVertex(xyz + v3);

					geometry->addIndex(0);
					geometry->addIndex(1);
					geometry->addIndex(2);
					geometry->addIndex(2);
					geometry->addIndex(1);
					geometry->addIndex(3);
				}
			}
			else // testParams.bottomTestType == BTT_AABBS
			{
				geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_AABBS_KHR, testParams.vertexFormat, testParams.indexType);
				geometry->addVertex(xyz + tcu::Vec3(0.0f, 0.0f, -0.1f));
				geometry->addVertex(xyz + tcu::Vec3(1.0f, 1.0f, 0.1f));
			}

			bottomLevelAccelerationStructure->addGeometry(geometry);
			result.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));
		}
	}

	return result;
}

de::MovePtr<TopLevelAccelerationStructure> CheckerboardConfiguration::initTopAccelerationStructure (Context&		context,
																									TestParams&		testParams,
																									std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >& bottomLevelAccelerationStructures)
{
	DE_UNREF(context);
	deUint32 instanceCount = testParams.width * testParams.height / 2;

	de::MovePtr<TopLevelAccelerationStructure>	result = makeTopLevelAccelerationStructure();
	result->setInstanceCount(instanceCount);

	if (testParams.topTestType == TTT_DIFFERENT_INSTANCES)
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
	else // testParams.topTestType == TTT_IDENTICAL_INSTANCES
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

void CheckerboardConfiguration::initRayTracingShaders(de::MovePtr<RayTracingPipeline>&		rayTracingPipeline,
													  Context&								context,
													  TestParams&							testParams)
{
	DE_UNREF(testParams);
	const DeviceInterface&						vkd						= context.getDeviceInterface();
	const VkDevice								device					= context.getDevice();

	rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get("rgen"),  0), 0);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	createShaderModule(vkd, device, context.getBinaryCollection().get("chit"),  0), 1);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	createShaderModule(vkd, device, context.getBinaryCollection().get("chit"),  0), 2);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR,	createShaderModule(vkd, device, context.getBinaryCollection().get("isect"), 0), 2);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR,			createShaderModule(vkd, device, context.getBinaryCollection().get("miss"),  0), 3);
}

void CheckerboardConfiguration::initShaderBindingTables(de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
														Context&							context,
														TestParams&							testParams,
														VkPipeline							pipeline,
														deUint32							shaderGroupHandleSize,
														deUint32							shaderGroupBaseAlignment,
														de::MovePtr<BufferWithMemory>&		raygenShaderBindingTable,
														de::MovePtr<BufferWithMemory>&		hitShaderBindingTable,
														de::MovePtr<BufferWithMemory>&		missShaderBindingTable)
{
	const DeviceInterface&						vkd						= context.getDeviceInterface();
	const VkDevice								device					= context.getDevice();
	Allocator&									allocator				= context.getDefaultAllocator();

	raygenShaderBindingTable											= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1 );
	if(testParams.bottomTestType == BTT_AABBS)
		hitShaderBindingTable											= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, 1 );
	else // testParams.bottomTestType == BTT_TRIANGLES
		hitShaderBindingTable											= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1 );
	missShaderBindingTable												= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 3, 1 );
}

bool CheckerboardConfiguration::verifyImage(BufferWithMemory* resultBuffer, Context& context, TestParams& testParams)
{
	DE_UNREF(context);
	const deUint32*						bufferPtr	= (deUint32*)resultBuffer->getAllocation().getHostPtr();
	deUint32							pos			= 0;
	deUint32							failures	= 0;

	// verify results - each test case should generate checkerboard pattern
	for (deUint32 y = 0; y < testParams.height; ++y)
	for (deUint32 x = 0; x < testParams.width; ++x)
	{
		deUint32 expectedResult = ((x + y) % 2) ? 2 : 1;
		if (bufferPtr[pos] != expectedResult)
			failures++;
		++pos;
	}
	return failures == 0;
}

VkFormat CheckerboardConfiguration::getResultImageFormat()
{
	return VK_FORMAT_R32_UINT;
}

size_t CheckerboardConfiguration::getResultImageFormatSize()
{
	return sizeof(deUint32);
}

VkClearValue CheckerboardConfiguration::getClearValue()
{
	return makeClearValueColorU32(0xFF, 0u, 0u, 0u);
}

class SingleTriangleConfiguration : public TestConfiguration
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
																										 de::MovePtr<BufferWithMemory>&		missShaderBindingTable) override;
	bool															verifyImage							(BufferWithMemory*					resultBuffer,
																										 Context&							context,
																										 TestParams&						testParams) override;
	VkFormat														getResultImageFormat				() override;
	size_t															getResultImageFormatSize			() override;
	VkClearValue													getClearValue						() override;

	// well, actually we have 2 triangles, but we ignore the first one ( see raygen shader for this configuration )
	const std::vector<tcu::Vec3> vertices =
	{
		tcu::Vec3(0.0f, 0.0f, -0.1f),
		tcu::Vec3(-0.1f, 0.0f, 0.0f),
		tcu::Vec3(0.0f, -0.1f, 0.0f),
		tcu::Vec3(0.0f, 0.0f, 0.0f),
		tcu::Vec3(0.5f, 0.0f, -0.5f),
		tcu::Vec3(0.0f, 0.5f, -0.5f),
	};

	const std::vector<deUint32> indices =
	{
		3,
		4,
		5
	};
};

std::vector<de::SharedPtr<BottomLevelAccelerationStructure> > SingleTriangleConfiguration::initBottomAccelerationStructures (Context&			context,
																															 TestParams&		testParams)
{
	DE_UNREF(context);
	std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >	result;

	de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure = makeBottomLevelAccelerationStructure();
	bottomLevelAccelerationStructure->setGeometryCount(1u);

	de::SharedPtr<RaytracedGeometryBase> geometry;
	geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_TRIANGLES_KHR, testParams.vertexFormat, testParams.indexType);
	for (auto it = begin(vertices), eit = end(vertices); it != eit; ++it)
		geometry->addVertex(*it);
	if (testParams.indexType != VK_INDEX_TYPE_NONE_KHR)
	{
		for (auto it = begin(indices), eit = end(indices); it != eit; ++it)
			geometry->addIndex(*it);
	}
	bottomLevelAccelerationStructure->addGeometry(geometry);
	result.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));

	return result;
}

de::MovePtr<TopLevelAccelerationStructure> SingleTriangleConfiguration::initTopAccelerationStructure (Context&			context,
																									  TestParams&		testParams,
																									  std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >& bottomLevelAccelerationStructures)
{
	DE_UNREF(context);
	DE_UNREF(testParams);

	de::MovePtr<TopLevelAccelerationStructure>	result = makeTopLevelAccelerationStructure();
	result->setInstanceCount(1u);

	result->addInstance(bottomLevelAccelerationStructures[0]);

	return result;
}

void SingleTriangleConfiguration::initRayTracingShaders(de::MovePtr<RayTracingPipeline>&		rayTracingPipeline,
														Context&								context,
														TestParams&								testParams)
{
	DE_UNREF(testParams);
	const DeviceInterface&						vkd						= context.getDeviceInterface();
	const VkDevice								device					= context.getDevice();

	rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get("rgen_depth"),  0), 0);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	createShaderModule(vkd, device, context.getBinaryCollection().get("chit_depth"),  0), 1);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR,			createShaderModule(vkd, device, context.getBinaryCollection().get("miss_depth"),  0), 2);
}

void SingleTriangleConfiguration::initShaderBindingTables(de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
														  Context&							context,
														  TestParams&						testParams,
														  VkPipeline						pipeline,
														  deUint32							shaderGroupHandleSize,
														  deUint32							shaderGroupBaseAlignment,
														  de::MovePtr<BufferWithMemory>&	raygenShaderBindingTable,
														  de::MovePtr<BufferWithMemory>&	hitShaderBindingTable,
														  de::MovePtr<BufferWithMemory>&	missShaderBindingTable)
{
	DE_UNREF(testParams);
	const DeviceInterface&						vkd						= context.getDeviceInterface();
	const VkDevice								device					= context.getDevice();
	Allocator&									allocator				= context.getDefaultAllocator();

	raygenShaderBindingTable											= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1 );
	hitShaderBindingTable												= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1 );
	missShaderBindingTable												= rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, 1 );
}

bool pointInTriangle2D(const tcu::Vec3& p, const tcu::Vec3& p0, const tcu::Vec3& p1, const tcu::Vec3& p2)
{
	float s = p0.y() * p2.x() - p0.x() * p2.y() + (p2.y() - p0.y()) * p.x() + (p0.x() - p2.x()) * p.y();
	float t = p0.x() * p1.y() - p0.y() * p1.x() + (p0.y() - p1.y()) * p.x() + (p1.x() - p0.x()) * p.y();

	if ((s < 0) != (t < 0))
		return false;

	float a = -p1.y() * p2.x() + p0.y() * (p2.x() - p1.x()) + p0.x() * (p1.y() - p2.y()) + p1.x() * p2.y();

	return a < 0 ?
		(s <= 0 && s + t >= a) :
		(s >= 0 && s + t <= a);
}

bool SingleTriangleConfiguration::verifyImage(BufferWithMemory* resultBuffer, Context& context, TestParams& testParams)
{
	tcu::TextureFormat			imageFormat		= vk::mapVkFormat(getResultImageFormat());
	tcu::TextureFormat			vertexFormat	= vk::mapVkFormat(testParams.vertexFormat);
	tcu::ConstPixelBufferAccess	resultAccess	(imageFormat, testParams.width, testParams.height, 1, resultBuffer->getAllocation().getHostPtr());

	std::vector<float>			reference		(testParams.width * testParams.height);
	tcu::PixelBufferAccess		referenceAccess	(imageFormat, testParams.width, testParams.height, 1, reference.data());

	// verify results
	tcu::Vec3					v0				= vertices[3];
	tcu::Vec3					v1				= vertices[4];
	tcu::Vec3					v2				= vertices[5];
	const int					numChannels		= tcu::getNumUsedChannels(vertexFormat.order);
	if (numChannels < 3)
	{
		v0.z() = 0.0f;
		v1.z() = 0.0f;
		v2.z() = 0.0f;
	}
	tcu::Vec3					abc				= tcu::cross((v2 - v0), (v1 - v0));

	for (deUint32 j = 0; j < testParams.height; ++j)
	{
		float y = 0.1f + 0.2f * float(j) / float(testParams.height - 1);
		for (deUint32 i = 0; i < testParams.width; ++i)
		{
			float	x			= 0.1f + 0.2f * float(i) / float(testParams.width - 1);
			float	z			= (abc.x()*x + abc.y()*y) / abc.z();
			bool	inTriangle	= pointInTriangle2D(tcu::Vec3(x, y, z), v0, v1, v2);
			float	refValue	= inTriangle ? 1.0f+z : 0.0f;
			referenceAccess.setPixel(tcu::Vec4(refValue, 0.0f, 0.0f, 1.0f), i, j);
		}
	}
	return tcu::floatThresholdCompare(context.getTestContext().getLog(), "Result comparison", "", referenceAccess, resultAccess, tcu::Vec4(0.01f), tcu::COMPARE_LOG_EVERYTHING);
}

VkFormat SingleTriangleConfiguration::getResultImageFormat()
{
	return VK_FORMAT_R32_SFLOAT;
}

size_t SingleTriangleConfiguration::getResultImageFormatSize()
{
	return sizeof(float);
}

VkClearValue SingleTriangleConfiguration::getClearValue()
{
	return makeClearValueColorF32(32.0f, 0.0f, 0.0f, 0.0f);
}

class RayTracingASBasicTestCase : public TestCase
{
	public:
																	RayTracingASBasicTestCase			(tcu::TestContext& context, const char* name, const char* desc, const TestParams data);
																	~RayTracingASBasicTestCase			(void);

	void															checkSupport						(Context& context) const override;
	void															initPrograms						(SourceCollections& programCollection) const override;
	TestInstance*													createInstance						(Context& context) const override;
private:
	TestParams														m_data;
};

class RayTracingASBasicTestInstance : public TestInstance
{
public:
																	RayTracingASBasicTestInstance		(Context& context, const TestParams& data);
																	~RayTracingASBasicTestInstance		(void);
	tcu::TestStatus													iterate								(void) override;

protected:
	bool															iterateNoWorkers					(void);
	bool															iterateWithWorkers					(void);
	de::MovePtr<BufferWithMemory>									runTest								(const deUint32 workerThreadsCount);
private:
	TestParams														m_data;
};

RayTracingASBasicTestCase::RayTracingASBasicTestCase (tcu::TestContext& context, const char* name, const char* desc, const TestParams data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

RayTracingASBasicTestCase::~RayTracingASBasicTestCase	(void)
{
}

void RayTracingASBasicTestCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality(getRayTracingExtensionUsed());

	const VkPhysicalDeviceRayTracingFeaturesKHR&	rayTracingFeaturesKHR = context.getRayTracingFeatures();

	if (rayTracingFeaturesKHR.rayTracing == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires rayTracingFeaturesKHR.rayTracing");

	if (m_data.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR && rayTracingFeaturesKHR.rayTracingHostAccelerationStructureCommands == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires rayTracingFeaturesKHR.rayTracingHostAccelerationStructureCommands");
}

void RayTracingASBasicTestCase::initPrograms (SourceCollections& programCollection) const
{
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
			"  float tmin      = 0.0;\n"
			"  float tmax      = 1.0;\n"
			"  vec3  origin    = vec3(float(gl_LaunchIDEXT.x) + 0.5f, float(gl_LaunchIDEXT.y) + 0.5f, 0.5);\n"
			"  vec3  direction = vec3(0.0,0.0,-1.0);\n"
			"  hitValue        = uvec4(0,0,0,0);\n"
			"  traceRayEXT(topLevelAS, 0, 0xFF, 0, 0, 0, origin, tmin, direction, tmax, 0);\n"
			"  imageStore(result, ivec2(gl_LaunchIDEXT.xy), hitValue);\n"
			"}\n";
		programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(css.str()));
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
			"void main()\n"
			"{\n"
			"  hitValue = uvec4(2,0,0,1);\n"
			"}\n";

		programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(css.str()));
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

		programCollection.glslSources.add("isect") << glu::IntersectionSource(updateRayTracingGLSL(css.str()));
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

		programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(css.str()));
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadEXT vec4 hitValue;\n"
			"layout(r32f, set = 0, binding = 0) uniform image2D result;\n"
			"layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
			"\n"
			"vec3 calculateOrigin(vec3 zeroOrigin, vec3 xAxis, vec3 yAxis)\n"
			"{\n"
			"  return zeroOrigin + (float(gl_LaunchIDEXT.x)/float(gl_LaunchSizeEXT.x-1)) * xAxis + (float(gl_LaunchIDEXT.y)/float(gl_LaunchSizeEXT.y-1)) * yAxis;\n"
			"}\n"
			"\n"
			"void main()\n"
			"{\n"
			"  float tmin      = 0.0;\n"
			"  float tmax      = 2.0;\n"
			"  vec3  origin    = calculateOrigin( vec3(0.1,0.1,1.0), vec3(0.2,0.0,0.0), vec3(0.0,0.2,0.0) );\n"
			"  vec3  direction = vec3(0.0,0.0,-1.0);\n"
			"  hitValue        = vec4(0.0,0.0,0.0,0.0);\n"
			"  traceRayEXT(topLevelAS, 0, 0xFF, 0, 0, 0, origin, tmin, direction, tmax, 0);\n"
			"  imageStore(result, ivec2(gl_LaunchIDEXT.xy), hitValue);\n"
			"}\n";
		programCollection.glslSources.add("rgen_depth") << glu::RaygenSource(updateRayTracingGLSL(css.str()));
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT vec4 hitValue;\n"
			"void main()\n"
			"{\n"
			"  hitValue = vec4(gl_RayTmaxEXT,0.0,0.0,1.0);\n"
			"}\n";

		programCollection.glslSources.add("chit_depth") << glu::ClosestHitSource(updateRayTracingGLSL(css.str()));
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT vec4 hitValue;\n"
			"void main()\n"
			"{\n"
			"  hitValue = vec4(0.0,0.0,0.0,1.0);\n"
			"}\n";

		programCollection.glslSources.add("miss_depth") << glu::MissSource(updateRayTracingGLSL(css.str()));
	}
}

TestInstance* RayTracingASBasicTestCase::createInstance (Context& context) const
{
	return new RayTracingASBasicTestInstance(context, m_data);
}

RayTracingASBasicTestInstance::RayTracingASBasicTestInstance (Context& context, const TestParams& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
{
}

RayTracingASBasicTestInstance::~RayTracingASBasicTestInstance (void)
{
}

de::MovePtr<BufferWithMemory> RayTracingASBasicTestInstance::runTest(const deUint32 workerThreadsCount)
{
	const InstanceInterface&			vki									= m_context.getInstanceInterface();
	const DeviceInterface&				vkd									= m_context.getDeviceInterface();
	const VkDevice						device								= m_context.getDevice();
	const VkPhysicalDevice				physicalDevice						= m_context.getPhysicalDevice();
	const deUint32						queueFamilyIndex					= m_context.getUniversalQueueFamilyIndex();
	const VkQueue						queue								= m_context.getUniversalQueue();
	Allocator&							allocator							= m_context.getDefaultAllocator();
	const deUint32						pixelCount							= m_data.width * m_data.height;
	const deUint32						shaderGroupHandleSize				= getShaderGroupSize(vki, physicalDevice);
	const deUint32						shaderGroupBaseAlignment			= getShaderGroupBaseAlignment(vki, physicalDevice);
	const bool							htCopy								= (workerThreadsCount != 0) && (m_data.operationType == OP_COPY);
	const bool							htSerialize							= (workerThreadsCount != 0) && (m_data.operationType == OP_SERIALIZE);

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
	m_data.testConfiguration->initShaderBindingTables(rayTracingPipeline, m_context, m_data, *pipeline, shaderGroupHandleSize, shaderGroupBaseAlignment, raygenShaderBindingTable, hitShaderBindingTable, missShaderBindingTable);

	const VkStridedBufferRegionKHR		raygenShaderBindingTableRegion		= makeStridedBufferRegionKHR(raygenShaderBindingTable->get(),	0,	shaderGroupHandleSize,	shaderGroupHandleSize);
	const VkStridedBufferRegionKHR		missShaderBindingTableRegion		= makeStridedBufferRegionKHR(missShaderBindingTable->get(),		0,	shaderGroupHandleSize,	shaderGroupHandleSize);
	const VkStridedBufferRegionKHR		hitShaderBindingTableRegion			= makeStridedBufferRegionKHR(hitShaderBindingTable->get(),		0,	shaderGroupHandleSize,	shaderGroupHandleSize);
	const VkStridedBufferRegionKHR		callableShaderBindingTableRegion	= makeStridedBufferRegionKHR(DE_NULL,							0,	0,						0);

	const VkFormat						imageFormat							= m_data.testConfiguration->getResultImageFormat();
	const VkImageCreateInfo				imageCreateInfo						= makeImageCreateInfo(m_data.width, m_data.height, imageFormat);
	const VkImageSubresourceRange		imageSubresourceRange				= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0, 1u);
	const de::MovePtr<ImageWithMemory>	image								= de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
	const Move<VkImageView>				imageView							= makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_2D, imageFormat, imageSubresourceRange);

	const VkBufferCreateInfo			resultBufferCreateInfo				= makeBufferCreateInfo(pixelCount*m_data.testConfiguration->getResultImageFormatSize(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkImageSubresourceLayers		resultBufferImageSubresourceLayers	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const VkBufferImageCopy				resultBufferImageRegion				= makeBufferImageCopy(makeExtent3D(m_data.width, m_data.height, 1u), resultBufferImageSubresourceLayers);
	de::MovePtr<BufferWithMemory>		resultBuffer						= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));

	const VkDescriptorImageInfo			descriptorImageInfo					= makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);

	const Move<VkCommandPool>			cmdPool								= createCommandPool(vkd, device, 0, queueFamilyIndex);
	const Move<VkCommandBuffer>			cmdBuffer							= allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	std::vector<de::SharedPtr<BottomLevelAccelerationStructure>>	bottomLevelAccelerationStructures;
	de::MovePtr<TopLevelAccelerationStructure>						topLevelAccelerationStructure;
	std::vector<de::SharedPtr<BottomLevelAccelerationStructure>>	bottomLevelAccelerationStructureCopies;
	de::MovePtr<TopLevelAccelerationStructure>						topLevelAccelerationStructureCopy;
	std::vector<de::SharedPtr<SerialStorage>>						bottomSerialized;
	std::vector<de::SharedPtr<SerialStorage>>						topSerialized;
	std::vector<VkDeviceSize>			accelerationCompactedSizes;
	std::vector<VkDeviceSize>			accelerationSerialSizes;
	Move<VkQueryPool>					m_queryPoolCompact;
	Move<VkQueryPool>					m_queryPoolSerial;

	beginCommandBuffer(vkd, *cmdBuffer, 0u);
	{
		const VkImageMemoryBarrier				preImageBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			**image, imageSubresourceRange);
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preImageBarrier);
		const VkClearValue						clearValue = m_data.testConfiguration->getClearValue();
		vkd.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1, &imageSubresourceRange);
		const VkImageMemoryBarrier				postImageBarrier = makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
			**image, imageSubresourceRange);
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, ALL_RAY_TRACING_STAGES, &postImageBarrier);

		// build bottom level acceleration structures and their copies ( only when we are testing copying bottom level acceleration structures )
		bool									bottomCompact		= m_data.operationType == OP_COMPACT && m_data.operationTarget == OT_BOTTOM_ACCELERATION;
		bool									bottomSerial		= m_data.operationType == OP_SERIALIZE && m_data.operationTarget == OT_BOTTOM_ACCELERATION;
		bottomLevelAccelerationStructures							= m_data.testConfiguration->initBottomAccelerationStructures(m_context, m_data);
		VkBuildAccelerationStructureFlagsKHR	allowCompactionFlag	= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
		VkBuildAccelerationStructureFlagsKHR	emptyCompactionFlag	= VkBuildAccelerationStructureFlagsKHR(0);
		VkBuildAccelerationStructureFlagsKHR	bottomCompactFlags	= (bottomCompact ? allowCompactionFlag : emptyCompactionFlag);
		VkBuildAccelerationStructureFlagsKHR	bottomBuildFlags	= m_data.buildFlags | bottomCompactFlags;
		std::vector<VkAccelerationStructureKHR>	accelerationStructureHandles;
		std::vector<VkDeviceSize>				bottomBlasCompactSize;
		std::vector<VkDeviceSize>				bottomBlasSerialSize;

		for (auto& blas : bottomLevelAccelerationStructures)
		{
			blas->setBuildType						(m_data.buildType);
			blas->setBuildFlags						(bottomBuildFlags);
			blas->setUseArrayOfPointers				(m_data.bottomUsesAOP);
			blas->createAndBuild					(vkd, device, *cmdBuffer, allocator);
			accelerationStructureHandles.push_back	(*(blas->getPtr()));
		}

		if (m_data.operationType == OP_COMPACT)
		{
			deUint32 queryCount	= (m_data.operationTarget == OT_BOTTOM_ACCELERATION) ? deUint32(bottomLevelAccelerationStructures.size()) : 1u;
			if (m_data.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
				m_queryPoolCompact = makeQueryPool(vkd, device, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryCount);
			if (m_data.operationTarget == OT_BOTTOM_ACCELERATION)
				queryAccelerationStructureSize(vkd, device, *cmdBuffer, accelerationStructureHandles, m_data.buildType, m_queryPoolCompact.get(), VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, 0u, bottomBlasCompactSize);
		}
		if (m_data.operationType == OP_SERIALIZE)
		{
			deUint32 queryCount	= (m_data.operationTarget == OT_BOTTOM_ACCELERATION) ? deUint32(bottomLevelAccelerationStructures.size()) : 1u;
			if (m_data.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
				m_queryPoolSerial = makeQueryPool(vkd, device, VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR, queryCount);
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
				bottomLevelAccelerationStructureCopies = m_data.testConfiguration->initBottomAccelerationStructures(m_context, m_data);
				for (size_t i = 0; i < bottomLevelAccelerationStructures.size(); ++i)
				{
					bottomLevelAccelerationStructureCopies[i]->setDeferredOperation(htCopy, workerThreadsCount);
					bottomLevelAccelerationStructureCopies[i]->setBuildType(m_data.buildType);
					bottomLevelAccelerationStructureCopies[i]->setBuildFlags(m_data.buildFlags);
					bottomLevelAccelerationStructureCopies[i]->setUseArrayOfPointers(m_data.bottomUsesAOP);
					bottomLevelAccelerationStructureCopies[i]->createAndCopyFrom(vkd, device, *cmdBuffer, allocator, 0u, bottomLevelAccelerationStructures[i].get(), 0u);
				}
				break;
			}
			case OP_COMPACT:
			{
				for (size_t i = 0; i < bottomLevelAccelerationStructures.size(); ++i)
				{
					de::MovePtr<BottomLevelAccelerationStructure> asCopy = makeBottomLevelAccelerationStructure();
					asCopy->setBuildType(m_data.buildType);
					asCopy->setBuildFlags(m_data.buildFlags);
					asCopy->setUseArrayOfPointers(m_data.bottomUsesAOP);
					asCopy->createAndCopyFrom(vkd, device, *cmdBuffer, allocator, 0u, bottomLevelAccelerationStructures[i].get(), bottomBlasCompactSize[i]);
					bottomLevelAccelerationStructureCopies.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(asCopy.release()));
				}
				break;
			}
			case OP_SERIALIZE:
			{
				bottomLevelAccelerationStructureCopies = m_data.testConfiguration->initBottomAccelerationStructures(m_context, m_data);
				for (size_t i = 0; i < bottomLevelAccelerationStructures.size(); ++i)
				{
					de::SharedPtr<SerialStorage> storage ( new SerialStorage(vkd, device, allocator, m_data.buildType, bottomBlasSerialSize[i]));

					bottomLevelAccelerationStructures[i]->setDeferredOperation(htSerialize, workerThreadsCount);
					bottomLevelAccelerationStructures[i]->serialize(vkd, device, *cmdBuffer, storage.get());
					bottomSerialized.push_back(storage);

					if (m_data.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
					{
						const VkMemoryBarrier	serializeMemoryBarrier = makeMemoryBarrier(VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT);
						cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &serializeMemoryBarrier);
					}

					bottomLevelAccelerationStructureCopies[i]->setBuildType(m_data.buildType);
					bottomLevelAccelerationStructureCopies[i]->setBuildFlags(m_data.buildFlags);
					bottomLevelAccelerationStructureCopies[i]->setUseArrayOfPointers(m_data.bottomUsesAOP);
					bottomLevelAccelerationStructureCopies[i]->setDeferredOperation(htSerialize, workerThreadsCount);
					bottomLevelAccelerationStructureCopies[i]->createAndDeserializeFrom(vkd, device, *cmdBuffer, allocator, 0u, storage.get());
				}
				break;
			}
			default:
				DE_ASSERT(DE_FALSE);
			}
			bottomLevelAccelerationStructuresPtr = &bottomLevelAccelerationStructureCopies;
		}

		// build top level acceleration structures and their copies ( only when we are testing copying top level acceleration structures )
		bool									topCompact			= m_data.operationType == OP_COMPACT && m_data.operationTarget == OT_TOP_ACCELERATION;
		bool									topSerial			= m_data.operationType == OP_SERIALIZE && m_data.operationTarget == OT_TOP_ACCELERATION;
		VkBuildAccelerationStructureFlagsKHR	topCompactFlags		= (topCompact ? allowCompactionFlag : emptyCompactionFlag);
		VkBuildAccelerationStructureFlagsKHR	topBuildFlags		= m_data.buildFlags | topCompactFlags;
		std::vector<VkAccelerationStructureKHR> topLevelStructureHandles;
		std::vector<VkDeviceSize>				topBlasCompactSize;
		std::vector<VkDeviceSize>				topBlasSerialSize;

		topLevelAccelerationStructure								= m_data.testConfiguration->initTopAccelerationStructure(m_context, m_data, *bottomLevelAccelerationStructuresPtr);
		topLevelAccelerationStructure->setBuildType					(m_data.buildType);
		topLevelAccelerationStructure->setBuildFlags				(topBuildFlags);
		topLevelAccelerationStructure->setUseArrayOfPointers		(m_data.topUsesAOP);
		topLevelAccelerationStructure->createAndBuild				(vkd, device, *cmdBuffer, allocator);
		topLevelStructureHandles.push_back							(*(topLevelAccelerationStructure->getPtr()));

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
					topLevelAccelerationStructureCopy = m_data.testConfiguration->initTopAccelerationStructure(m_context, m_data, *bottomLevelAccelerationStructuresPtr);
					topLevelAccelerationStructureCopy->setDeferredOperation(htCopy, workerThreadsCount);
					topLevelAccelerationStructureCopy->setBuildType(m_data.buildType);
					topLevelAccelerationStructureCopy->setBuildFlags(m_data.buildFlags);
					topLevelAccelerationStructureCopy->setUseArrayOfPointers(m_data.topUsesAOP);
					topLevelAccelerationStructureCopy->createAndCopyFrom(vkd, device, *cmdBuffer, allocator, 0u, topLevelAccelerationStructure.get(), 0u);
					break;
				}
				case OP_COMPACT:
				{
					topLevelAccelerationStructureCopy = makeTopLevelAccelerationStructure();
					topLevelAccelerationStructureCopy->setBuildType(m_data.buildType);
					topLevelAccelerationStructureCopy->setBuildFlags(m_data.buildFlags);
					topLevelAccelerationStructureCopy->setUseArrayOfPointers(m_data.topUsesAOP);
					topLevelAccelerationStructureCopy->createAndCopyFrom(vkd, device, *cmdBuffer, allocator, 0u, topLevelAccelerationStructure.get(), topBlasCompactSize[0]);
					break;
				}
				case OP_SERIALIZE:
				{
					topLevelAccelerationStructureCopy = m_data.testConfiguration->initTopAccelerationStructure(m_context, m_data, *bottomLevelAccelerationStructuresPtr);

					de::SharedPtr<SerialStorage> storage( new SerialStorage(vkd, device, allocator, m_data.buildType, topBlasSerialSize[0]));

					topLevelAccelerationStructure->setDeferredOperation(htSerialize, workerThreadsCount);
					topLevelAccelerationStructure->serialize(vkd, device, *cmdBuffer, storage.get());
					topSerialized.push_back(storage);

					if (m_data.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR)
					{
						const VkMemoryBarrier	serializeMemoryBarrier = makeMemoryBarrier(VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT);
						cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &serializeMemoryBarrier);
					}

					topLevelAccelerationStructureCopy->setBuildType(m_data.buildType);
					topLevelAccelerationStructureCopy->setBuildFlags(m_data.buildFlags);
					topLevelAccelerationStructureCopy->setUseArrayOfPointers(m_data.topUsesAOP);
					topLevelAccelerationStructureCopy->setDeferredOperation(htSerialize, workerThreadsCount);
					topLevelAccelerationStructureCopy->createAndDeserializeFrom(vkd, device, *cmdBuffer, allocator, 0u, storage.get());
					break;
				}
				default:
					DE_ASSERT(DE_FALSE);
			}
			topLevelRayTracedPtr = topLevelAccelerationStructureCopy.get();
		}

		const VkMemoryBarrier				preTraceMemoryBarrier				= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, ALL_RAY_TRACING_STAGES, &preTraceMemoryBarrier);

		VkWriteDescriptorSetAccelerationStructureKHR	accelerationStructureWriteDescriptorSet	=
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	//  VkStructureType						sType;
			DE_NULL,															//  const void*							pNext;
			1u,																	//  deUint32							accelerationStructureCount;
			topLevelRayTracedPtr->getPtr(),										//  const VkAccelerationStructureKHR*	pAccelerationStructures;
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

		const VkMemoryBarrier				postTraceMemoryBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, ALL_RAY_TRACING_STAGES, VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);

		vkd.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **resultBuffer, 1u, &resultBufferImageRegion);
	}
	endCommandBuffer(vkd, *cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

	invalidateMappedMemoryRange(vkd, device, resultBuffer->getAllocation().getMemory(), resultBuffer->getAllocation().getOffset(), pixelCount * sizeof(deUint32));

	return resultBuffer;
}

bool RayTracingASBasicTestInstance::iterateNoWorkers (void)
{
	// run test using arrays of pointers
	const de::MovePtr<BufferWithMemory>	buffer		= runTest(0);

	return m_data.testConfiguration->verifyImage(buffer.get(), m_context, m_data);
}

bool RayTracingASBasicTestInstance::iterateWithWorkers (void)
{
	const deUint64					singleThreadTimeStart	= deGetMicroseconds();
	de::MovePtr<BufferWithMemory>	singleThreadBufferCPU	= runTest(0);
	const bool						singleThreadValidation	= m_data.testConfiguration->verifyImage(singleThreadBufferCPU.get(), m_context, m_data);
	const deUint64					singleThreadTime		= deGetMicroseconds() - singleThreadTimeStart;

	deUint64						multiThreadTimeStart	= deGetMicroseconds();
	de::MovePtr<BufferWithMemory>	multiThreadBufferCPU	= runTest(m_data.workerThreadsCount);
	const bool						multiThreadValidation	= m_data.testConfiguration->verifyImage(multiThreadBufferCPU.get(), m_context, m_data);
	deUint64						multiThreadTime			= deGetMicroseconds() - multiThreadTimeStart;
	const deUint64					multiThreadTimeOut		= 10 * singleThreadTime;

	const deUint32					result					= singleThreadValidation && multiThreadValidation;

	if (multiThreadTime > multiThreadTimeOut)
	{
		std::string failMsg	= "Time of multithreaded test execution " + de::toString(multiThreadTime) +
							  " that is longer than expected execution time " + de::toString(multiThreadTimeOut);

		TCU_FAIL(failMsg);
	}

	return result;
}

tcu::TestStatus RayTracingASBasicTestInstance::iterate (void)
{
	bool result;

	if (m_data.workerThreadsCount != 0)
		result = iterateWithWorkers();
	else
		result = iterateNoWorkers();

	if (result)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Fail");
}

}	// anonymous

struct MemoryRequirementsTestParams
{
	VkAccelerationStructureMemoryRequirementsTypeKHR asMemReqType;
};


class RayTracingASMemoryRequirementsTestCase : public TestCase
{
	public:
																	RayTracingASMemoryRequirementsTestCase		(tcu::TestContext& context, const char* name, const char* desc, const MemoryRequirementsTestParams& data);
																	~RayTracingASMemoryRequirementsTestCase		(void);

	void															checkSupport								(Context& context) const override;
	void															initPrograms								(SourceCollections& programCollection) const override;
	TestInstance*													createInstance								(Context& context) const override;
private:
	MemoryRequirementsTestParams									m_data;
};

class RayTracingASMemoryRequirementsTestInstance : public TestInstance
{
public:
																	RayTracingASMemoryRequirementsTestInstance	(Context& context, const MemoryRequirementsTestParams& data);
																	~RayTracingASMemoryRequirementsTestInstance	(void);
	tcu::TestStatus													iterate										(void) override;
private:
	MemoryRequirementsTestParams									m_data;
};

RayTracingASMemoryRequirementsTestCase::RayTracingASMemoryRequirementsTestCase (tcu::TestContext& context, const char* name, const char* desc, const MemoryRequirementsTestParams& data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

RayTracingASMemoryRequirementsTestCase::~RayTracingASMemoryRequirementsTestCase (void)
{
}

void RayTracingASMemoryRequirementsTestCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality(getRayTracingExtensionUsed());

	const VkPhysicalDeviceRayTracingFeaturesKHR&	rayTracingFeaturesKHR = context.getRayTracingFeatures();

	if (rayTracingFeaturesKHR.rayTracing == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires rayTracingFeaturesKHR.rayTracing");
}

void RayTracingASMemoryRequirementsTestCase::initPrograms (SourceCollections& programCollection) const
{
	DE_UNREF(programCollection);
}

TestInstance* RayTracingASMemoryRequirementsTestCase::createInstance (Context& context) const
{
	return new RayTracingASMemoryRequirementsTestInstance(context, m_data);
}

RayTracingASMemoryRequirementsTestInstance::RayTracingASMemoryRequirementsTestInstance(Context& context, const MemoryRequirementsTestParams& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
{
}

RayTracingASMemoryRequirementsTestInstance::~RayTracingASMemoryRequirementsTestInstance(void)
{
}

tcu::TestStatus RayTracingASMemoryRequirementsTestInstance::iterate (void)
{
	const DeviceInterface&									vkd												= m_context.getDeviceInterface();
	const VkDevice											device											= m_context.getDevice();

	const VkAccelerationStructureCreateGeometryTypeInfoKHR	accelerationStructureCreateGeometryTypeInfoKHR	=
	{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_GEOMETRY_TYPE_INFO_KHR,	//  VkStructureType		sType;
		DE_NULL,																//  const void*			pNext;
		VK_GEOMETRY_TYPE_TRIANGLES_KHR,											//  VkGeometryTypeKHR	geometryType;
		128,																	//  deUint32			maxPrimitiveCount;
		VK_INDEX_TYPE_UINT32,													//  VkIndexType			indexType;
		128,																	//  deUint32			maxVertexCount;
		VK_FORMAT_R32G32B32_SFLOAT,												//  VkFormat			vertexFormat;
		DE_FALSE																//  VkBool32			allowsTransforms;
	};

	const VkAccelerationStructureCreateInfoKHR				accelerationStructureCreateInfoKHR				=
	{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,				//  VkStructureType											sType;
		DE_NULL,																//  const void*												pNext;
		0,																		//  VkDeviceSize											compactedSize;
		VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,						//  VkAccelerationStructureTypeKHR							type;
		0,																		//  VkBuildAccelerationStructureFlagsKHR					flags;
		1,																		//  deUint32												maxGeometryCount;
		&accelerationStructureCreateGeometryTypeInfoKHR,						//  const VkAccelerationStructureCreateGeometryTypeInfoKHR*	pGeometryInfos;
		0u																		//  VkDeviceAddress											deviceAddress;
	};
	Move<VkAccelerationStructureKHR>						m_accelerationStructureKHR						= createAccelerationStructureKHR(vkd, device, &accelerationStructureCreateInfoKHR, DE_NULL);

	const VkAccelerationStructureMemoryRequirementsInfoKHR	accelerationStructureMemoryRequirementsInfoKHR	=
	{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_KHR,	//  VkStructureType										sType;
		DE_NULL,																//  const void*											pNext;
		m_data.asMemReqType,													//  VkAccelerationStructureMemoryRequirementsTypeKHR	type;
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,						//  VkAccelerationStructureBuildTypeKHR					buildType;
		m_accelerationStructureKHR.get()										//  VkAccelerationStructureKHR							accelerationStructure;
	};
	VkMemoryRequirements2									memoryRequirements2								=
	{
		VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,	//  VkStructureType			sType;
		DE_NULL,									//  void*					pNext;
		{0, 0, 0}									//  VkMemoryRequirements	memoryRequirements;
	};
	vkd.getAccelerationStructureMemoryRequirementsKHR(device, &accelerationStructureMemoryRequirementsInfoKHR, &memoryRequirements2);

	if(memoryRequirements2.memoryRequirements.alignment != 0)
		return tcu::TestStatus::fail("Fail");

	if (memoryRequirements2.memoryRequirements.memoryTypeBits != 0)
		return tcu::TestStatus::fail("Fail");

	return tcu::TestStatus::pass("Pass");
}

void addBasicBuildingTests(tcu::TestCaseGroup* group)
{
	struct
	{
		vk::VkAccelerationStructureBuildTypeKHR	buildType;
		const char*								name;
	} buildTypes[] =
	{
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR,				"cpu_built"	},
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,				"gpu_built"	},
	};

	struct
	{
		BottomTestType							testType;
		bool									usesAOP;
		const char*								name;
	} bottomTestTypes[] =
	{
		{ BTT_TRIANGLES,	false,										"triangles" },
		{ BTT_TRIANGLES,	true,										"triangles_aop" },
		{ BTT_AABBS,		false,										"aabbs" },
		{ BTT_AABBS,		true,										"aabbs_aop" },
	};

	struct
	{
		TopTestType								testType;
		bool									usesAOP;
		const char*								name;
	} topTestTypes[] =
	{
		{ TTT_IDENTICAL_INSTANCES,	false,								"identical_instances" },
		{ TTT_IDENTICAL_INSTANCES,	true,								"identical_instances_aop" },
		{ TTT_DIFFERENT_INSTANCES,	false,								"different_instances" },
		{ TTT_DIFFERENT_INSTANCES,	true,								"different_instances_aop" },
	};

	struct BuildFlagsData
	{
		VkBuildAccelerationStructureFlagsKHR	flags;
		const char*								name;
	};

	BuildFlagsData optimizationTypes[] =
	{
		{ VkBuildAccelerationStructureFlagsKHR(0u),						"0" },
		{ VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,	"fasttrace" },
		{ VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR,	"fastbuild" },
	};

	BuildFlagsData updateTypes[] =
	{
		{ VkBuildAccelerationStructureFlagsKHR(0u),						"0" },
		{ VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,			"update" },
	};

	BuildFlagsData compactionTypes[] =
	{
		{ VkBuildAccelerationStructureFlagsKHR(0u),						"0" },
		{ VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,		"compaction" },
	};

	BuildFlagsData lowMemoryTypes[] =
	{
		{ VkBuildAccelerationStructureFlagsKHR(0u),						"0" },
		{ VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,		"lowmemory" },
	};

	for (size_t buildTypeNdx = 0; buildTypeNdx < DE_LENGTH_OF_ARRAY(buildTypes); ++buildTypeNdx)
	{
		de::MovePtr<tcu::TestCaseGroup> buildGroup(new tcu::TestCaseGroup(group->getTestContext(), buildTypes[buildTypeNdx].name, ""));

		for (size_t bottomNdx = 0; bottomNdx < DE_LENGTH_OF_ARRAY(bottomTestTypes); ++bottomNdx)
		{
			de::MovePtr<tcu::TestCaseGroup> bottomGroup(new tcu::TestCaseGroup(group->getTestContext(), bottomTestTypes[bottomNdx].name, ""));

			for (size_t topNdx = 0; topNdx < DE_LENGTH_OF_ARRAY(topTestTypes); ++topNdx)
			{
				de::MovePtr<tcu::TestCaseGroup> topGroup(new tcu::TestCaseGroup(group->getTestContext(), topTestTypes[topNdx].name, ""));

				for (size_t optimizationNdx = 0; optimizationNdx < DE_LENGTH_OF_ARRAY(optimizationTypes); ++optimizationNdx)
				{
					for (size_t updateNdx = 0; updateNdx < DE_LENGTH_OF_ARRAY(updateTypes); ++updateNdx)
					{
						for (size_t compactionNdx = 0; compactionNdx < DE_LENGTH_OF_ARRAY(compactionTypes); ++compactionNdx)
						{
							for (size_t lowMemoryNdx = 0; lowMemoryNdx < DE_LENGTH_OF_ARRAY(lowMemoryTypes); ++lowMemoryNdx)
							{
								std::string testName =
									std::string(optimizationTypes[optimizationNdx].name) + "_" +
									std::string(updateTypes[updateNdx].name) + "_" +
									std::string(compactionTypes[compactionNdx].name) + "_" +
									std::string(lowMemoryTypes[lowMemoryNdx].name);

								TestParams testParams
								{
									buildTypes[buildTypeNdx].buildType,
									VK_FORMAT_R32G32B32_SFLOAT,
									VK_INDEX_TYPE_NONE_KHR,
									bottomTestTypes[bottomNdx].testType,
									bottomTestTypes[bottomNdx].usesAOP,
									topTestTypes[topNdx].testType,
									topTestTypes[topNdx].usesAOP,
									optimizationTypes[optimizationNdx].flags | updateTypes[updateNdx].flags | compactionTypes[compactionNdx].flags | lowMemoryTypes[lowMemoryNdx].flags,
									OT_NONE,
									OP_NONE,
									RTAS_DEFAULT_SIZE,
									RTAS_DEFAULT_SIZE,
									de::SharedPtr<TestConfiguration>(new CheckerboardConfiguration()),
									0
								};
								topGroup->addChild(new RayTracingASBasicTestCase(group->getTestContext(), testName.c_str(), "", testParams));
							}
						}
					}
				}
				bottomGroup->addChild(topGroup.release());
			}
			buildGroup->addChild(bottomGroup.release());
		}
		group->addChild(buildGroup.release());
	}
}

void addVertexIndexFormatsTests(tcu::TestCaseGroup* group)
{
	// skip two formats not handled by NV
	struct
	{
		VkFormat								format;
		const char*								name;
	} vertexFormats[] =
	{
		{ VK_FORMAT_R32G32_SFLOAT,				"r32g32_sfloat"			},
		{ VK_FORMAT_R32G32B32_SFLOAT,			"r32g32b32_sfloat"		},
		{ VK_FORMAT_R16G16_SFLOAT,				"r16g16_sfloat"			},
		{ VK_FORMAT_R16G16B16A16_SFLOAT,		"r16g16b16a16_sfloat"	},
		{ VK_FORMAT_R16G16_SNORM,				"r16g16_snorm"			},
		{ VK_FORMAT_R16G16B16A16_SNORM,			"r16g16b16a16_snorm"	},
	};

	struct
	{
		VkIndexType								indexType;
		const char*								name;
	} indexFormats[] =
	{
		{ VK_INDEX_TYPE_NONE_KHR ,				"index_none"		},
		{ VK_INDEX_TYPE_UINT16 ,				"index_uint16"	},
		{ VK_INDEX_TYPE_UINT32 ,				"index_uint32"	},
	};

	for (size_t vertexFormatNdx = 0; vertexFormatNdx < DE_LENGTH_OF_ARRAY(vertexFormats); ++vertexFormatNdx)
	{
		de::MovePtr<tcu::TestCaseGroup> vertexFormatGroup(new tcu::TestCaseGroup(group->getTestContext(), vertexFormats[vertexFormatNdx].name, ""));

		for (size_t indexFormatNdx = 0; indexFormatNdx < DE_LENGTH_OF_ARRAY(indexFormats); ++indexFormatNdx)
		{
			TestParams testParams
			{
				VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR,
				vertexFormats[vertexFormatNdx].format,
				indexFormats[indexFormatNdx].indexType,
				BTT_TRIANGLES,
				false,
				TTT_IDENTICAL_INSTANCES,
				false,
				VkBuildAccelerationStructureFlagsKHR(0u),
				OT_NONE,
				OP_NONE,
				RTAS_DEFAULT_SIZE,
				RTAS_DEFAULT_SIZE,
				de::SharedPtr<TestConfiguration>(new SingleTriangleConfiguration()),
				0
			};
			vertexFormatGroup->addChild(new RayTracingASBasicTestCase(group->getTestContext(), indexFormats[indexFormatNdx].name, "", testParams));
		}
		group->addChild(vertexFormatGroup.release());
	}
}

void addOperationTestsImpl (tcu::TestCaseGroup* group, const deUint32 workerThreads)
{
	struct
	{
		OperationType										operationType;
		const char*											name;
	} operationTypes[] =
	{
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
		OperationTarget										operationTarget;
		const char*											name;
	} operationTargets[] =
	{
		{ OT_TOP_ACCELERATION,								"top_acceleration_structure"		},
		{ OT_BOTTOM_ACCELERATION,							"bottom_acceleration_structure"	},
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

	for (size_t operationTypeNdx = 0; operationTypeNdx < DE_LENGTH_OF_ARRAY(operationTypes); ++operationTypeNdx)
	{
		if (workerThreads > 0)
			if (operationTypes[operationTypeNdx].operationType != OP_COPY && operationTypes[operationTypeNdx].operationType != OP_SERIALIZE)
				continue;

		de::MovePtr<tcu::TestCaseGroup> operationTypeGroup(new tcu::TestCaseGroup(group->getTestContext(), operationTypes[operationTypeNdx].name, ""));

		for (size_t buildTypeNdx = 0; buildTypeNdx < DE_LENGTH_OF_ARRAY(buildTypes); ++buildTypeNdx)
		{
			if (workerThreads > 0 && buildTypes[buildTypeNdx].buildType != VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR)
				continue;

			de::MovePtr<tcu::TestCaseGroup> buildGroup(new tcu::TestCaseGroup(group->getTestContext(), buildTypes[buildTypeNdx].name, ""));

			for (size_t operationTargetNdx = 0; operationTargetNdx < DE_LENGTH_OF_ARRAY(operationTargets); ++operationTargetNdx)
			{
				de::MovePtr<tcu::TestCaseGroup> operationTargetGroup(new tcu::TestCaseGroup(group->getTestContext(), operationTargets[operationTargetNdx].name, ""));

				for (size_t testTypeNdx = 0; testTypeNdx < DE_LENGTH_OF_ARRAY(bottomTestTypes); ++testTypeNdx)
				{
					TopTestType topTest = (operationTargets[operationTargetNdx].operationTarget == OT_TOP_ACCELERATION) ? TTT_DIFFERENT_INSTANCES : TTT_IDENTICAL_INSTANCES;

					TestParams testParams
					{
						buildTypes[buildTypeNdx].buildType,
						VK_FORMAT_R32G32B32_SFLOAT,
						VK_INDEX_TYPE_NONE_KHR,
						bottomTestTypes[testTypeNdx].testType,
						false,
						topTest,
						false,
						VkBuildAccelerationStructureFlagsKHR(0u),
						operationTargets[operationTargetNdx].operationTarget,
						operationTypes[operationTypeNdx].operationType,
						RTAS_DEFAULT_SIZE,
						RTAS_DEFAULT_SIZE,
						de::SharedPtr<TestConfiguration>(new CheckerboardConfiguration()),
						workerThreads
					};
					operationTargetGroup->addChild(new RayTracingASBasicTestCase(group->getTestContext(), bottomTestTypes[testTypeNdx].name, "", testParams));
				}
				buildGroup->addChild(operationTargetGroup.release());
			}
			operationTypeGroup->addChild(buildGroup.release());
		}
		group->addChild(operationTypeGroup.release());
	}
}

void addRequirementsTests(tcu::TestCaseGroup* group)
{
	group->addChild(new RayTracingASMemoryRequirementsTestCase(group->getTestContext(), "memory_build_scratch", "", MemoryRequirementsTestParams{ VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_KHR }));
	group->addChild(new RayTracingASMemoryRequirementsTestCase(group->getTestContext(), "memory_update_scratch", "", MemoryRequirementsTestParams{ VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_UPDATE_SCRATCH_KHR }));
}

void addOperationTests (tcu::TestCaseGroup* group)
{
	addOperationTestsImpl(group, 0);
}

void addHostThreadingOperationTests (tcu::TestCaseGroup* group)
{
	const deUint32	threads[]	= { 1, 2, 3, 4, 8, std::numeric_limits<deUint32>::max() };

	for (size_t threadsNdx = 0; threadsNdx < DE_LENGTH_OF_ARRAY(threads); ++threadsNdx)
	{
		const std::string groupName = threads[threadsNdx] != std::numeric_limits<deUint32>::max()
									? de::toString(threads[threadsNdx])
									: "max";

		de::MovePtr<tcu::TestCaseGroup> threadGroup(new tcu::TestCaseGroup(group->getTestContext(), groupName.c_str(), ""));

		addOperationTestsImpl(threadGroup.get(), threads[threadsNdx]);

		group->addChild(threadGroup.release());
	}
}

tcu::TestCaseGroup*	createAccelerationStructuresTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "acceleration_structures", "Acceleration structure tests"));

	addTestGroup(group.get(), "flags", "Test building AS with different build types, build flags and geometries/instances using arrays or arrays of pointers", addBasicBuildingTests);
	addTestGroup(group.get(), "format", "Test building AS with different vertex and index formats", addVertexIndexFormatsTests);
	addTestGroup(group.get(), "operations", "Test copying, compaction and serialization of AS", addOperationTests);
	addTestGroup(group.get(), "requirements", "Test other requirements", addRequirementsTests);
	addTestGroup(group.get(), "host_threading", "Test host threading operations", addHostThreadingOperationTests);

	return group.release();
}

}	// RayTracing

}	// vkt
