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
#include "deRandom.h"

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
#include "vkRayTracingUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTexture.hpp"
#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuFloat.hpp"

#include <set>
#include <limits>

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

enum class InstanceCullFlags
{
	NONE,
	CULL_DISABLE,
	COUNTERCLOCKWISE,
	ALL,
};

enum class EmptyAccelerationStructureCase
{
	NOT_EMPTY				= 0,
	INACTIVE_TRIANGLES		= 1,
	INACTIVE_INSTANCES		= 2,
	NO_GEOMETRIES_BOTTOM	= 3,	// geometryCount zero when building.
	NO_PRIMITIVES_BOTTOM	= 4,	// primitiveCount zero when building.
	NO_PRIMITIVES_TOP		= 5,	// primitiveCount zero when building.
};

enum class InstanceCustomIndexCase
{
	NONE			= 0,
	CLOSEST_HIT		= 1,
	ANY_HIT			= 2,
	INTERSECTION	= 3,
};

static const deUint32 RTAS_DEFAULT_SIZE = 8u;

// Chosen to have the most significant bit set to 1 when represented using 24 bits.
// This will make sure the instance custom index will not be sign-extended by mistake.
constexpr deUint32 INSTANCE_CUSTOM_INDEX_BASE = 0x807f00u;

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
	bool									padVertices;
	VkIndexType								indexType;
	BottomTestType							bottomTestType; // what kind of geometry is stored in bottom AS
	InstanceCullFlags						cullFlags;		// Flags for instances, if needed.
	bool									bottomUsesAOP;	// does bottom AS use arrays, or arrays of pointers
	bool									bottomGeneric;	// Bottom created as generic AS type.
	TopTestType								topTestType;	// If instances are identical then bottom geometries must have different vertices/aabbs
	bool									topUsesAOP;		// does top AS use arrays, or arrays of pointers
	bool									topGeneric;		// Top created as generic AS type.
	VkBuildAccelerationStructureFlagsKHR	buildFlags;
	OperationTarget							operationTarget;
	OperationType							operationType;
	deUint32								width;
	deUint32								height;
	de::SharedPtr<TestConfiguration>		testConfiguration;
	deUint32								workerThreadsCount;
	EmptyAccelerationStructureCase			emptyASCase;
	InstanceCustomIndexCase					instanceCustomIndexCase;
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

VkGeometryInstanceFlagsKHR getCullFlags (InstanceCullFlags flags)
{
	VkGeometryInstanceFlagsKHR cullFlags = 0u;

	if (flags == InstanceCullFlags::CULL_DISABLE || flags == InstanceCullFlags::ALL)
		cullFlags |= VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

	if (flags == InstanceCullFlags::COUNTERCLOCKWISE || flags == InstanceCullFlags::ALL)
		cullFlags |= VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR;

	return cullFlags;
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

	// Cull flags can only be used with triangles.
	DE_ASSERT(testParams.cullFlags == InstanceCullFlags::NONE || testParams.bottomTestType == BTT_TRIANGLES);

	// Checkerboard configuration does not support empty geometry tests.
	DE_ASSERT(testParams.emptyASCase == EmptyAccelerationStructureCase::NOT_EMPTY);

	std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >	result;

	const auto instanceFlags = getCullFlags(testParams.cullFlags);

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
			geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_TRIANGLES_KHR, testParams.vertexFormat, testParams.indexType, testParams.padVertices);
			if (testParams.indexType == VK_INDEX_TYPE_NONE_KHR)
			{
				if (instanceFlags == 0u)
				{
					geometry->addVertex(v0);
					geometry->addVertex(v1);
					geometry->addVertex(v2);
					geometry->addVertex(v2);
					geometry->addVertex(v1);
					geometry->addVertex(v3);
				}
				else // Counterclockwise so the flags will be needed for the geometry to be visible.
				{
					geometry->addVertex(v2);
					geometry->addVertex(v1);
					geometry->addVertex(v0);
					geometry->addVertex(v3);
					geometry->addVertex(v1);
					geometry->addVertex(v2);
				}
			}
			else // m_data.indexType != VK_INDEX_TYPE_NONE_KHR
			{
				geometry->addVertex(v0);
				geometry->addVertex(v1);
				geometry->addVertex(v2);
				geometry->addVertex(v3);

				if (instanceFlags == 0u)
				{
					geometry->addIndex(0);
					geometry->addIndex(1);
					geometry->addIndex(2);
					geometry->addIndex(2);
					geometry->addIndex(1);
					geometry->addIndex(3);
				}
				else // Counterclockwise so the flags will be needed for the geometry to be visible.
				{
					geometry->addIndex(2);
					geometry->addIndex(1);
					geometry->addIndex(0);
					geometry->addIndex(3);
					geometry->addIndex(1);
					geometry->addIndex(2);
				}
			}
		}
		else // m_data.bottomTestType == BTT_AABBS
		{
			geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_AABBS_KHR, testParams.vertexFormat, testParams.indexType, testParams.padVertices);

			if (!testParams.padVertices)
			{
				// Single AABB.
				geometry->addVertex(tcu::Vec3(0.0f, 0.0f, -0.1f));
				geometry->addVertex(tcu::Vec3(1.0f, 1.0f,  0.1f));
			}
			else
			{
				// Multiple AABBs covering the same space.
				geometry->addVertex(tcu::Vec3(0.0f, 0.0f, -0.1f));
				geometry->addVertex(tcu::Vec3(0.5f, 0.5f,  0.1f));

				geometry->addVertex(tcu::Vec3(0.5f, 0.5f, -0.1f));
				geometry->addVertex(tcu::Vec3(1.0f, 1.0f,  0.1f));

				geometry->addVertex(tcu::Vec3(0.0f, 0.5f, -0.1f));
				geometry->addVertex(tcu::Vec3(0.5f, 1.0f,  0.1f));

				geometry->addVertex(tcu::Vec3(0.5f, 0.0f, -0.1f));
				geometry->addVertex(tcu::Vec3(1.0f, 0.5f,  0.1f));
			}
		}

		bottomLevelAccelerationStructure->addGeometry(geometry);

		if (testParams.instanceCustomIndexCase == InstanceCustomIndexCase::ANY_HIT)
			geometry->setGeometryFlags(VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR);

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
				geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_TRIANGLES_KHR, testParams.vertexFormat, testParams.indexType, testParams.padVertices);
				if (testParams.indexType == VK_INDEX_TYPE_NONE_KHR)
				{
					if (instanceFlags == 0u)
					{
						geometry->addVertex(xyz + v0);
						geometry->addVertex(xyz + v1);
						geometry->addVertex(xyz + v2);
						geometry->addVertex(xyz + v2);
						geometry->addVertex(xyz + v1);
						geometry->addVertex(xyz + v3);
					}
					else // Counterclockwise so the flags will be needed for the geometry to be visible.
					{
						geometry->addVertex(xyz + v2);
						geometry->addVertex(xyz + v1);
						geometry->addVertex(xyz + v0);
						geometry->addVertex(xyz + v3);
						geometry->addVertex(xyz + v1);
						geometry->addVertex(xyz + v2);
					}
				}
				else
				{
					geometry->addVertex(xyz + v0);
					geometry->addVertex(xyz + v1);
					geometry->addVertex(xyz + v2);
					geometry->addVertex(xyz + v3);

					if (instanceFlags == 0u)
					{
						geometry->addIndex(0);
						geometry->addIndex(1);
						geometry->addIndex(2);
						geometry->addIndex(2);
						geometry->addIndex(1);
						geometry->addIndex(3);
					}
					else // Counterclockwise so the flags will be needed for the geometry to be visible.
					{
						geometry->addIndex(2);
						geometry->addIndex(1);
						geometry->addIndex(0);
						geometry->addIndex(3);
						geometry->addIndex(1);
						geometry->addIndex(2);
					}
				}
			}
			else // testParams.bottomTestType == BTT_AABBS
			{
				geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_AABBS_KHR, testParams.vertexFormat, testParams.indexType, testParams.padVertices);

				if (!testParams.padVertices)
				{
					// Single AABB.
					geometry->addVertex(xyz + tcu::Vec3(0.0f, 0.0f, -0.1f));
					geometry->addVertex(xyz + tcu::Vec3(1.0f, 1.0f,  0.1f));
				}
				else
				{
					// Multiple AABBs covering the same space.
					geometry->addVertex(xyz + tcu::Vec3(0.0f, 0.0f, -0.1f));
					geometry->addVertex(xyz + tcu::Vec3(0.5f, 0.5f,  0.1f));

					geometry->addVertex(xyz + tcu::Vec3(0.5f, 0.5f, -0.1f));
					geometry->addVertex(xyz + tcu::Vec3(1.0f, 1.0f,  0.1f));

					geometry->addVertex(xyz + tcu::Vec3(0.0f, 0.5f, -0.1f));
					geometry->addVertex(xyz + tcu::Vec3(0.5f, 1.0f,  0.1f));

					geometry->addVertex(xyz + tcu::Vec3(0.5f, 0.0f, -0.1f));
					geometry->addVertex(xyz + tcu::Vec3(1.0f, 0.5f,  0.1f));
				}
			}

			bottomLevelAccelerationStructure->addGeometry(geometry);

			if (testParams.instanceCustomIndexCase == InstanceCustomIndexCase::ANY_HIT)
				geometry->setGeometryFlags(VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR);

			result.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));
		}
	}

	return result;
}

de::MovePtr<TopLevelAccelerationStructure> CheckerboardConfiguration::initTopAccelerationStructure (Context&		context,
																									TestParams&		testParams,
																									std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >& bottomLevelAccelerationStructures)
{
	// Checkerboard configuration does not support empty geometry tests.
	DE_ASSERT(testParams.emptyASCase == EmptyAccelerationStructureCase::NOT_EMPTY);

	DE_UNREF(context);

	const auto instanceCount = testParams.width * testParams.height / 2u;
	const auto instanceFlags = getCullFlags(testParams.cullFlags);

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
			const deUint32 instanceCustomIndex = ((testParams.instanceCustomIndexCase != InstanceCustomIndexCase::NONE) ? (INSTANCE_CUSTOM_INDEX_BASE + x + y) : 0u);
			result->addInstance(bottomLevelAccelerationStructures[0], transformMatrixKHR, instanceCustomIndex, 0xFFu, 0u, instanceFlags);
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
			const deUint32 instanceCustomIndex = ((testParams.instanceCustomIndexCase != InstanceCustomIndexCase::NONE) ? (INSTANCE_CUSTOM_INDEX_BASE + x + y) : 0u);
			result->addInstance(bottomLevelAccelerationStructures[currentInstanceIndex++], identityMatrix3x4, instanceCustomIndex, 0xFFu, 0u, instanceFlags);
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

	const bool useAnyHit		= (testParams.instanceCustomIndexCase == InstanceCustomIndexCase::ANY_HIT);
	const auto hitShaderStage	= (useAnyHit ? VK_SHADER_STAGE_ANY_HIT_BIT_KHR : VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
	const auto hitShaderName	= (useAnyHit ? "ahit" : "chit");

	rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,		createShaderModule(vkd, device, context.getBinaryCollection().get("rgen"),  0), 0);
	rayTracingPipeline->addShader(hitShaderStage,						createShaderModule(vkd, device, context.getBinaryCollection().get(hitShaderName),  0), 1);
	rayTracingPipeline->addShader(hitShaderStage,						createShaderModule(vkd, device, context.getBinaryCollection().get(hitShaderName),  0), 2);
	if (testParams.bottomTestType == BTT_AABBS)
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
	// Checkerboard configuration does not support empty geometry tests.
	DE_ASSERT(testParams.emptyASCase == EmptyAccelerationStructureCase::NOT_EMPTY);

	DE_UNREF(context);
	const auto*						bufferPtr	= (deInt32*)resultBuffer->getAllocation().getHostPtr();
	deUint32						pos			= 0;
	deUint32						failures	= 0;

	// verify results - each test case should generate checkerboard pattern
	for (deUint32 y = 0; y < testParams.height; ++y)
	for (deUint32 x = 0; x < testParams.width; ++x)
	{
		// The hit value should match the shader code.
		const deInt32 hitValue			= ((testParams.instanceCustomIndexCase != InstanceCustomIndexCase::NONE) ? static_cast<deInt32>(INSTANCE_CUSTOM_INDEX_BASE + x + y) : 2);
		const deInt32 expectedResult	= ((x + y) % 2) ? hitValue : 1;

		if (bufferPtr[pos] != expectedResult)
			failures++;

		++pos;
	}
	return failures == 0;
}

VkFormat CheckerboardConfiguration::getResultImageFormat()
{
	return VK_FORMAT_R32_SINT;
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

	// No other cases supported for the single triangle configuration.
	DE_ASSERT(testParams.instanceCustomIndexCase == InstanceCustomIndexCase::NONE);

	std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >	result;

	de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure = makeBottomLevelAccelerationStructure();
	bottomLevelAccelerationStructure->setGeometryCount(1u);

	de::SharedPtr<RaytracedGeometryBase> geometry;
	geometry = makeRaytracedGeometry(VK_GEOMETRY_TYPE_TRIANGLES_KHR, testParams.vertexFormat, testParams.indexType);

	auto customVertices(vertices);

	if (testParams.emptyASCase == EmptyAccelerationStructureCase::INACTIVE_TRIANGLES)
	{
		const auto nanValue = tcu::Float32::nan().asFloat();
		for (auto& vtx : customVertices)
			vtx.x() = nanValue;
	}

	for (auto it = begin(customVertices), eit = end(customVertices); it != eit; ++it)
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

	// Unsupported in this configuration.
	DE_ASSERT(testParams.instanceCustomIndexCase == InstanceCustomIndexCase::NONE);

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
			float	refValue	= ((inTriangle && testParams.emptyASCase == EmptyAccelerationStructureCase::NOT_EMPTY) ? 1.0f+z : 0.0f);
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

void commonASTestsCheckSupport(Context& context)
{
	context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

	const VkPhysicalDeviceRayTracingPipelineFeaturesKHR&	rayTracingPipelineFeaturesKHR = context.getRayTracingPipelineFeatures();
	if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

	const VkPhysicalDeviceAccelerationStructureFeaturesKHR&	accelerationStructureFeaturesKHR = context.getAccelerationStructureFeatures();
	if (accelerationStructureFeaturesKHR.accelerationStructure == DE_FALSE)
		TCU_THROW(TestError, "VK_KHR_ray_tracing_pipeline requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");
}

class RayTracingASBasicTestCase : public TestCase
{
public:
																	RayTracingASBasicTestCase			(tcu::TestContext& context, const char* name, const char* desc, const TestParams& data);
																	~RayTracingASBasicTestCase			(void);

	void															checkSupport						(Context& context) const override;
	void															initPrograms						(SourceCollections& programCollection) const override;
	TestInstance*													createInstance						(Context& context) const override;
protected:
	TestParams														m_data;
};

// Same as RayTracingASBasicTestCase but it will only initialize programs for SingleTriangleConfiguration and use hand-tuned SPIR-V
// assembly.
class RayTracingASFuncArgTestCase : public RayTracingASBasicTestCase
{
public:
																	RayTracingASFuncArgTestCase			(tcu::TestContext& context, const char* name, const char* desc, const TestParams& data);
																	~RayTracingASFuncArgTestCase		(void) {}

	void															initPrograms						(SourceCollections& programCollection) const override;
};

class RayTracingASBasicTestInstance : public TestInstance
{
public:
																	RayTracingASBasicTestInstance		(Context& context, const TestParams& data);
																	~RayTracingASBasicTestInstance		(void) = default;
	tcu::TestStatus													iterate								(void) override;

protected:
	bool															iterateNoWorkers					(void);
	bool															iterateWithWorkers					(void);
	de::MovePtr<BufferWithMemory>									runTest								(const deUint32 workerThreadsCount);
private:
	TestParams														m_data;
};

RayTracingASBasicTestCase::RayTracingASBasicTestCase (tcu::TestContext& context, const char* name, const char* desc, const TestParams& data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

RayTracingASBasicTestCase::~RayTracingASBasicTestCase	(void)
{
}

void RayTracingASBasicTestCase::checkSupport(Context& context) const
{
	commonASTestsCheckSupport(context);

	const VkPhysicalDeviceAccelerationStructureFeaturesKHR&	accelerationStructureFeaturesKHR = context.getAccelerationStructureFeatures();
	if (m_data.buildType == VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR && accelerationStructureFeaturesKHR.accelerationStructureHostCommands == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructureHostCommands");

	// Check supported vertex format.
	checkAccelerationStructureVertexBufferFormat(context.getInstanceInterface(), context.getPhysicalDevice(), m_data.vertexFormat);
}

void RayTracingASBasicTestCase::initPrograms (SourceCollections& programCollection) const
{
	bool storeInRGen = false;
	bool storeInAHit = false;
	bool storeInCHit = false;
	bool storeInISec = false;

	switch (m_data.instanceCustomIndexCase)
	{
	case InstanceCustomIndexCase::NONE:			storeInRGen = true;	break;
	case InstanceCustomIndexCase::CLOSEST_HIT:	storeInCHit = true; break;
	case InstanceCustomIndexCase::ANY_HIT:		storeInAHit = true;	break;
	case InstanceCustomIndexCase::INTERSECTION:	storeInISec = true; break;
	default: DE_ASSERT(false); break;
	}

	const std::string				imageDeclaration	= "layout(r32i, set = 0, binding = 0) uniform iimage2D result;\n";
	const std::string				storeCustomIndex	= "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), ivec4(gl_InstanceCustomIndexEXT, 0, 0, 1));\n";
	const vk::ShaderBuildOptions	buildOptions		(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	{
		std::stringstream css;
		css
			<< "#version 460 core\n"
			<< "#extension GL_EXT_ray_tracing : require\n"
			<< "layout(location = 0) rayPayloadEXT ivec4 hitValue;\n";

		if (storeInRGen)
			css << imageDeclaration;

		css
			<< "layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
			<< "\n"
			<< "void main()\n"
			<< "{\n"
			<< "  float tmin      = 0.0;\n"
			<< "  float tmax      = 1.0;\n"
			<< "  vec3  origin    = vec3(float(gl_LaunchIDEXT.x) + 0.5f, float(gl_LaunchIDEXT.y) + 0.5f, 0.5);\n"
			<< "  vec3  direction = vec3(0.0,0.0,-1.0);\n"
			<< "  hitValue        = ivec4(0,0,0,0);\n"
			<< "  traceRayEXT(topLevelAS, " << ((m_data.cullFlags == InstanceCullFlags::NONE) ? "0" : "gl_RayFlagsCullBackFacingTrianglesEXT") << ", 0xFF, 0, 0, 0, origin, tmin, direction, tmax, 0);\n";

		if (storeInRGen)
			css << "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), hitValue);\n";

		css << "}\n";

		programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css
			<< "#version 460 core\n"
			<< "#extension GL_EXT_ray_tracing : require\n"
			<< "layout(location = 0) rayPayloadInEXT ivec4 hitValue;\n";

		if (storeInCHit)
			css << imageDeclaration;

		css
			<< "void main()\n"
			<< "{\n"
			<< "  hitValue = ivec4(2,0,0,1);\n";

		if (storeInCHit)
			css << storeCustomIndex;

		css << "}\n";

		programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	if (storeInAHit)
	{
		std::stringstream css;
		css
			<< "#version 460 core\n"
			<< "#extension GL_EXT_ray_tracing : require\n"
			<< imageDeclaration
			<< "void main()\n"
			<< "{\n"
			<< storeCustomIndex
			<< "}\n";

		programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css
			<< "#version 460 core\n"
			<< "#extension GL_EXT_ray_tracing : require\n"
			<< "hitAttributeEXT ivec4 hitAttribute;\n";

		if (storeInISec)
			css << imageDeclaration;

		css
			<< "void main()\n"
			<< "{\n"
			<< "  hitAttribute = ivec4(0,0,0,0);\n"
			<< "  reportIntersectionEXT(0.5f, 0);\n";

		if (storeInISec)
			css << storeCustomIndex;

		css << "}\n";

		programCollection.glslSources.add("isect") << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css
			<< "#version 460 core\n"
			<< "#extension GL_EXT_ray_tracing : require\n"
			<< "layout(location = 0) rayPayloadInEXT ivec4 hitValue;\n";

		if (!storeInRGen)
			css << imageDeclaration;

		css
			<< "void main()\n"
			<< "{\n"
			<< "  hitValue = ivec4(1,0,0,1);\n";

		if (!storeInRGen)
			css << "  imageStore(result, ivec2(gl_LaunchIDEXT.xy), hitValue);\n";

		css << "}\n";

		programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
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
		programCollection.glslSources.add("rgen_depth") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
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

		programCollection.glslSources.add("chit_depth") << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
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

		programCollection.glslSources.add("miss_depth") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}
}

TestInstance* RayTracingASBasicTestCase::createInstance (Context& context) const
{
	return new RayTracingASBasicTestInstance(context, m_data);
}

RayTracingASFuncArgTestCase::RayTracingASFuncArgTestCase (tcu::TestContext& context, const char* name, const char* desc, const TestParams& data)
	: RayTracingASBasicTestCase (context, name, desc, data)
{
}

void RayTracingASFuncArgTestCase::initPrograms (SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	const vk::SpirVAsmBuildOptions	spvBuildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, true);

	{
		// The SPIR-V assembly below is based on the following GLSL code. Some
		// modifications have been made to make traceRaysBottomWrapper take a bare
		// acceleration structure as its argument instead of a pointer to it, so we can
		// test passing a pointer and a bare value in the same test.
		//
		//	#version 460 core
		//	#extension GL_EXT_ray_tracing : require
		//	layout(location = 0) rayPayloadEXT vec4 hitValue;
		//	layout(r32f, set = 0, binding = 0) uniform image2D result;
		//	layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;
		//
		//	void traceRaysBottomWrapper(
		//	  accelerationStructureEXT topLevel,
		//	  uint rayFlags,
		//	  uint cullMask,
		//	  uint sbtRecordOffset,
		//	  uint sbtRecordStride,
		//	  uint missIndex,
		//	  vec3 origin,
		//	  float Tmin,
		//	  vec3 direction,
		//	  float Tmax)
		//	{
		//	  traceRayEXT(topLevel, rayFlags, cullMask, sbtRecordOffset, sbtRecordStride, missIndex, origin, Tmin, direction, Tmax, 0);
		//	}
		//
		//	void traceRaysTopWrapper(
		//	  accelerationStructureEXT topLevel,
		//	  uint rayFlags,
		//	  uint cullMask,
		//	  uint sbtRecordOffset,
		//	  uint sbtRecordStride,
		//	  uint missIndex,
		//	  vec3 origin,
		//	  float Tmin,
		//	  vec3 direction,
		//	  float Tmax)
		//	{
		//	  traceRaysBottomWrapper(topLevel, rayFlags, cullMask, sbtRecordOffset, sbtRecordStride, missIndex, origin, Tmin, direction, Tmax);
		//	}
		//
		//	vec3 calculateOrigin(vec3 zeroOrigin, vec3 xAxis, vec3 yAxis)
		//	{
		//	  return zeroOrigin + (float(gl_LaunchIDEXT.x)/float(gl_LaunchSizeEXT.x-1)) * xAxis + (float(gl_LaunchIDEXT.y)/float(gl_LaunchSizeEXT.y-1)) * yAxis;
		//	}
		//
		//	void main()
		//	{
		//	  float tmin      = 0.0;
		//	  float tmax      = 2.0;
		//	  vec3  origin    = calculateOrigin( vec3(0.1,0.1,1.0), vec3(0.2,0.0,0.0), vec3(0.0,0.2,0.0) );
		//	  vec3  direction = vec3(0.0,0.0,-1.0);
		//	  hitValue        = vec4(0.0,0.0,0.0,0.0);
		//	  traceRaysTopWrapper(topLevelAS, 0, 0xFF, 0, 0, 0, origin, tmin, direction, tmax);
		//	  imageStore(result, ivec2(gl_LaunchIDEXT.xy), hitValue);
		//	}

		std::ostringstream rgen;
		rgen
			<< "; SPIR-V\n"
			<< "; Version: 1.4\n"
			<< "; Generator: Khronos Glslang Reference Front End; 10\n"
			<< "; Bound: 156\n"
			<< "; Schema: 0\n"
			<< "OpCapability RayTracingKHR\n"
			<< "OpExtension \"SPV_KHR_ray_tracing\"\n"
			<< "%1 = OpExtInstImport \"GLSL.std.450\"\n"
			<< "OpMemoryModel Logical GLSL450\n"
			<< "OpEntryPoint RayGenerationKHR %4 \"main\" %59 %82 %88 %130 %148\n"
			<< "OpDecorate %59 Location 0\n"
			<< "OpDecorate %82 BuiltIn LaunchIdKHR\n"
			<< "OpDecorate %88 BuiltIn LaunchSizeKHR\n"
			<< "OpDecorate %130 DescriptorSet 0\n"
			<< "OpDecorate %130 Binding 1\n"
			<< "OpDecorate %148 DescriptorSet 0\n"
			<< "OpDecorate %148 Binding 0\n"
			<< "%2 = OpTypeVoid\n"
			<< "%3 = OpTypeFunction %2\n"

			// This is the bare type.
			<< "%6 = OpTypeAccelerationStructureKHR\n"

			// This is the pointer type.
			<< "%7 = OpTypePointer UniformConstant %6\n"

			<< "%8 = OpTypeInt 32 0\n"
			<< "%9 = OpTypePointer Function %8\n"
			<< "%10 = OpTypeFloat 32\n"
			<< "%11 = OpTypeVector %10 3\n"
			<< "%12 = OpTypePointer Function %11\n"
			<< "%13 = OpTypePointer Function %10\n"

			// This is the type for traceRaysTopWrapper and also the original traceRaysBottomWrapper.
			<< "%14 = OpTypeFunction %2 %7 %9 %9 %9 %9 %9 %12 %13 %12 %13\n"

			// This is the modified type to take a bare AS as the first argument, for the modified version of traceRaysBottomWrapper.
			<< "%14b = OpTypeFunction %2 %6 %9 %9 %9 %9 %9 %12 %13 %12 %13\n"

			<< "%39 = OpTypeFunction %11 %12 %12 %12\n"
			<< "%55 = OpTypeInt 32 1\n"
			<< "%56 = OpConstant %55 0\n"
			<< "%57 = OpTypeVector %10 4\n"
			<< "%58 = OpTypePointer RayPayloadKHR %57\n"
			<< "%59 = OpVariable %58 RayPayloadKHR\n"
			<< "%80 = OpTypeVector %8 3\n"
			<< "%81 = OpTypePointer Input %80\n"
			<< "%82 = OpVariable %81 Input\n"
			<< "%83 = OpConstant %8 0\n"
			<< "%84 = OpTypePointer Input %8\n"
			<< "%88 = OpVariable %81 Input\n"
			<< "%91 = OpConstant %8 1\n"
			<< "%112 = OpConstant %10 0\n"
			<< "%114 = OpConstant %10 2\n"
			<< "%116 = OpConstant %10 0.100000001\n"
			<< "%117 = OpConstant %10 1\n"
			<< "%118 = OpConstantComposite %11 %116 %116 %117\n"
			<< "%119 = OpConstant %10 0.200000003\n"
			<< "%120 = OpConstantComposite %11 %119 %112 %112\n"
			<< "%121 = OpConstantComposite %11 %112 %119 %112\n"
			<< "%127 = OpConstant %10 -1\n"
			<< "%128 = OpConstantComposite %11 %112 %112 %127\n"
			<< "%129 = OpConstantComposite %57 %112 %112 %112 %112\n"
			<< "%130 = OpVariable %7 UniformConstant\n"
			<< "%131 = OpConstant %8 255\n"
			<< "%146 = OpTypeImage %10 2D 0 0 0 2 R32f\n"
			<< "%147 = OpTypePointer UniformConstant %146\n"
			<< "%148 = OpVariable %147 UniformConstant\n"
			<< "%150 = OpTypeVector %8 2\n"
			<< "%153 = OpTypeVector %55 2\n"

			// This is main().
			<< "%4 = OpFunction %2 None %3\n"
			<< "%5 = OpLabel\n"
			<< "%111 = OpVariable %13 Function\n"
			<< "%113 = OpVariable %13 Function\n"
			<< "%115 = OpVariable %12 Function\n"
			<< "%122 = OpVariable %12 Function\n"
			<< "%123 = OpVariable %12 Function\n"
			<< "%124 = OpVariable %12 Function\n"
			<< "%126 = OpVariable %12 Function\n"
			<< "%132 = OpVariable %9 Function\n"
			<< "%133 = OpVariable %9 Function\n"
			<< "%134 = OpVariable %9 Function\n"
			<< "%135 = OpVariable %9 Function\n"
			<< "%136 = OpVariable %9 Function\n"
			<< "%137 = OpVariable %12 Function\n"
			<< "%139 = OpVariable %13 Function\n"
			<< "%141 = OpVariable %12 Function\n"
			<< "%143 = OpVariable %13 Function\n"
			<< "OpStore %111 %112\n"
			<< "OpStore %113 %114\n"
			<< "OpStore %122 %118\n"
			<< "OpStore %123 %120\n"
			<< "OpStore %124 %121\n"
			<< "%125 = OpFunctionCall %11 %43 %122 %123 %124\n"
			<< "OpStore %115 %125\n"
			<< "OpStore %126 %128\n"
			<< "OpStore %59 %129\n"
			<< "OpStore %132 %83\n"
			<< "OpStore %133 %131\n"
			<< "OpStore %134 %83\n"
			<< "OpStore %135 %83\n"
			<< "OpStore %136 %83\n"
			<< "%138 = OpLoad %11 %115\n"
			<< "OpStore %137 %138\n"
			<< "%140 = OpLoad %10 %111\n"
			<< "OpStore %139 %140\n"
			<< "%142 = OpLoad %11 %126\n"
			<< "OpStore %141 %142\n"
			<< "%144 = OpLoad %10 %113\n"
			<< "OpStore %143 %144\n"
			<< "%145 = OpFunctionCall %2 %37 %130 %132 %133 %134 %135 %136 %137 %139 %141 %143\n"
			<< "%149 = OpLoad %146 %148\n"
			<< "%151 = OpLoad %80 %82\n"
			<< "%152 = OpVectorShuffle %150 %151 %151 0 1\n"
			<< "%154 = OpBitcast %153 %152\n"
			<< "%155 = OpLoad %57 %59\n"
			<< "OpImageWrite %149 %154 %155\n"
			<< "OpReturn\n"
			<< "OpFunctionEnd\n"

			// This is traceRaysBottomWrapper, doing the OpTraceRayKHR call.
			// We have modified the type so it takes a bare AS as the first argument.
			// %25 = OpFunction %2 None %14
			<< "%25 = OpFunction %2 None %14b\n"

			// Also the type of the first argument here.
			// %15 = OpFunctionParameter %7
			<< "%15 = OpFunctionParameter %6\n"

			<< "%16 = OpFunctionParameter %9\n"
			<< "%17 = OpFunctionParameter %9\n"
			<< "%18 = OpFunctionParameter %9\n"
			<< "%19 = OpFunctionParameter %9\n"
			<< "%20 = OpFunctionParameter %9\n"
			<< "%21 = OpFunctionParameter %12\n"
			<< "%22 = OpFunctionParameter %13\n"
			<< "%23 = OpFunctionParameter %12\n"
			<< "%24 = OpFunctionParameter %13\n"
			<< "%26 = OpLabel\n"

			// We no longer need to dereference the pointer here.
			// %45 = OpLoad %6 %15

			<< "%46 = OpLoad %8 %16\n"
			<< "%47 = OpLoad %8 %17\n"
			<< "%48 = OpLoad %8 %18\n"
			<< "%49 = OpLoad %8 %19\n"
			<< "%50 = OpLoad %8 %20\n"
			<< "%51 = OpLoad %11 %21\n"
			<< "%52 = OpLoad %10 %22\n"
			<< "%53 = OpLoad %11 %23\n"
			<< "%54 = OpLoad %10 %24\n"

			// And we can use the first argument here directly.
			// OpTraceRayKHR %45 %46 %47 %48 %49 %50 %51 %52 %53 %54 %59
			<< "OpTraceRayKHR %15 %46 %47 %48 %49 %50 %51 %52 %53 %54 %59\n"

			<< "OpReturn\n"
			<< "OpFunctionEnd\n"

			// This is traceRaysTopWrapper, which calls traceRaysBottomWrapper.
			<< "%37 = OpFunction %2 None %14\n"

			// First argument, pointer to AS.
			<< "%27 = OpFunctionParameter %7\n"

			<< "%28 = OpFunctionParameter %9\n"
			<< "%29 = OpFunctionParameter %9\n"
			<< "%30 = OpFunctionParameter %9\n"
			<< "%31 = OpFunctionParameter %9\n"
			<< "%32 = OpFunctionParameter %9\n"
			<< "%33 = OpFunctionParameter %12\n"
			<< "%34 = OpFunctionParameter %13\n"
			<< "%35 = OpFunctionParameter %12\n"
			<< "%36 = OpFunctionParameter %13\n"
			<< "%38 = OpLabel\n"
			<< "%60 = OpVariable %9 Function\n"
			<< "%62 = OpVariable %9 Function\n"
			<< "%64 = OpVariable %9 Function\n"
			<< "%66 = OpVariable %9 Function\n"
			<< "%68 = OpVariable %9 Function\n"
			<< "%70 = OpVariable %12 Function\n"
			<< "%72 = OpVariable %13 Function\n"
			<< "%74 = OpVariable %12 Function\n"
			<< "%76 = OpVariable %13 Function\n"

			// Dereference the pointer to pass the AS as the first argument.
			<< "%27b = OpLoad %6 %27\n"

			<< "%61 = OpLoad %8 %28\n"
			<< "OpStore %60 %61\n"
			<< "%63 = OpLoad %8 %29\n"
			<< "OpStore %62 %63\n"
			<< "%65 = OpLoad %8 %30\n"
			<< "OpStore %64 %65\n"
			<< "%67 = OpLoad %8 %31\n"
			<< "OpStore %66 %67\n"
			<< "%69 = OpLoad %8 %32\n"
			<< "OpStore %68 %69\n"
			<< "%71 = OpLoad %11 %33\n"
			<< "OpStore %70 %71\n"
			<< "%73 = OpLoad %10 %34\n"
			<< "OpStore %72 %73\n"
			<< "%75 = OpLoad %11 %35\n"
			<< "OpStore %74 %75\n"
			<< "%77 = OpLoad %10 %36\n"
			<< "OpStore %76 %77\n"

			// %2 is void, %25 is traceRaysBottomWrapper and %27 was the first argument.
			// We need to pass the loaded AS instead.
			// %78 = OpFunctionCall %2 %25 %27 %60 %62 %64 %66 %68 %70 %72 %74 %76
			<< "%78 = OpFunctionCall %2 %25 %27b %60 %62 %64 %66 %68 %70 %72 %74 %76\n"

			<< "OpReturn\n"
			<< "OpFunctionEnd\n"

			// This is calculateOrigin().
			<< "%43 = OpFunction %11 None %39\n"
			<< "%40 = OpFunctionParameter %12\n"
			<< "%41 = OpFunctionParameter %12\n"
			<< "%42 = OpFunctionParameter %12\n"
			<< "%44 = OpLabel\n"
			<< "%79 = OpLoad %11 %40\n"
			<< "%85 = OpAccessChain %84 %82 %83\n"
			<< "%86 = OpLoad %8 %85\n"
			<< "%87 = OpConvertUToF %10 %86\n"
			<< "%89 = OpAccessChain %84 %88 %83\n"
			<< "%90 = OpLoad %8 %89\n"
			<< "%92 = OpISub %8 %90 %91\n"
			<< "%93 = OpConvertUToF %10 %92\n"
			<< "%94 = OpFDiv %10 %87 %93\n"
			<< "%95 = OpLoad %11 %41\n"
			<< "%96 = OpVectorTimesScalar %11 %95 %94\n"
			<< "%97 = OpFAdd %11 %79 %96\n"
			<< "%98 = OpAccessChain %84 %82 %91\n"
			<< "%99 = OpLoad %8 %98\n"
			<< "%100 = OpConvertUToF %10 %99\n"
			<< "%101 = OpAccessChain %84 %88 %91\n"
			<< "%102 = OpLoad %8 %101\n"
			<< "%103 = OpISub %8 %102 %91\n"
			<< "%104 = OpConvertUToF %10 %103\n"
			<< "%105 = OpFDiv %10 %100 %104\n"
			<< "%106 = OpLoad %11 %42\n"
			<< "%107 = OpVectorTimesScalar %11 %106 %105\n"
			<< "%108 = OpFAdd %11 %97 %107\n"
			<< "OpReturnValue %108\n"
			<< "OpFunctionEnd\n"
			;

		programCollection.spirvAsmSources.add("rgen_depth") << spvBuildOptions << rgen.str();
	}

	// chit_depth and miss_depth below have been left untouched.

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

		programCollection.glslSources.add("chit_depth") << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
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

		programCollection.glslSources.add("miss_depth") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}
}

RayTracingASBasicTestInstance::RayTracingASBasicTestInstance (Context& context, const TestParams& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
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

	const VkStridedDeviceAddressRegionKHR	raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(),	0),	shaderGroupHandleSize,	shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR	missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(),		0),	shaderGroupHandleSize,	shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR	hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(),		0),	shaderGroupHandleSize,	shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR	callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL,																	0,						0);

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
		const VkImageMemoryBarrier				postImageBarrier = makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
			**image, imageSubresourceRange);
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, &postImageBarrier);

		// build bottom level acceleration structures and their copies ( only when we are testing copying bottom level acceleration structures )
		bool									bottomCompact		= m_data.operationType == OP_COMPACT && m_data.operationTarget == OT_BOTTOM_ACCELERATION;
		bool									bottomSerial		= m_data.operationType == OP_SERIALIZE && m_data.operationTarget == OT_BOTTOM_ACCELERATION;
		const bool								buildWithoutGeom	= (m_data.emptyASCase == EmptyAccelerationStructureCase::NO_GEOMETRIES_BOTTOM);
		const bool								bottomNoPrimitives	= (m_data.emptyASCase == EmptyAccelerationStructureCase::NO_PRIMITIVES_BOTTOM);
		const bool								topNoPrimitives		= (m_data.emptyASCase == EmptyAccelerationStructureCase::NO_PRIMITIVES_TOP);
		const bool								inactiveInstances	= (m_data.emptyASCase == EmptyAccelerationStructureCase::INACTIVE_INSTANCES);
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
			blas->setCreateGeneric					(m_data.bottomGeneric);
			blas->setBuildWithoutGeometries			(buildWithoutGeom);
			blas->setBuildWithoutPrimitives			(bottomNoPrimitives);
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
				for (size_t i = 0; i < bottomLevelAccelerationStructures.size(); ++i)
				{
					de::MovePtr<BottomLevelAccelerationStructure> asCopy = makeBottomLevelAccelerationStructure();
					asCopy->setDeferredOperation(htCopy, workerThreadsCount);
					asCopy->setBuildType(m_data.buildType);
					asCopy->setBuildFlags(m_data.buildFlags);
					asCopy->setUseArrayOfPointers(m_data.bottomUsesAOP);
					asCopy->setCreateGeneric(m_data.bottomGeneric);
					asCopy->setBuildWithoutGeometries(buildWithoutGeom);
					asCopy->setBuildWithoutPrimitives(bottomNoPrimitives);
					asCopy->createAndCopyFrom(vkd, device, *cmdBuffer, allocator, bottomLevelAccelerationStructures[i].get(), 0u, 0u);
					bottomLevelAccelerationStructureCopies.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(asCopy.release()));
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
					asCopy->setCreateGeneric(m_data.bottomGeneric);
					asCopy->setBuildWithoutGeometries(buildWithoutGeom);
					asCopy->setBuildWithoutPrimitives(bottomNoPrimitives);
					asCopy->createAndCopyFrom(vkd, device, *cmdBuffer, allocator, bottomLevelAccelerationStructures[i].get(), bottomBlasCompactSize[i], 0u);
					bottomLevelAccelerationStructureCopies.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(asCopy.release()));
				}
				break;
			}
			case OP_SERIALIZE:
			{
				//bottomLevelAccelerationStructureCopies = m_data.testConfiguration->initBottomAccelerationStructures(m_context, m_data);
				for (size_t i = 0; i < bottomLevelAccelerationStructures.size(); ++i)
				{
					de::SharedPtr<SerialStorage> storage ( new SerialStorage(vkd, device, allocator, m_data.buildType, bottomBlasSerialSize[i]));

					bottomLevelAccelerationStructures[i]->setDeferredOperation(htSerialize, workerThreadsCount);
					bottomLevelAccelerationStructures[i]->serialize(vkd, device, *cmdBuffer, storage.get());
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
					asCopy->setBuildFlags(m_data.buildFlags);
					asCopy->setUseArrayOfPointers(m_data.bottomUsesAOP);
					asCopy->setCreateGeneric(m_data.bottomGeneric);
					asCopy->setBuildWithoutGeometries(buildWithoutGeom);
					asCopy->setBuildWithoutPrimitives(bottomNoPrimitives);
					asCopy->setDeferredOperation(htSerialize, workerThreadsCount);
					asCopy->createAndDeserializeFrom(vkd, device, *cmdBuffer, allocator, storage.get(), 0u);
					bottomLevelAccelerationStructureCopies.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(asCopy.release()));
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
		topLevelAccelerationStructure->setBuildWithoutPrimitives	(topNoPrimitives);
		topLevelAccelerationStructure->setUseArrayOfPointers		(m_data.topUsesAOP);
		topLevelAccelerationStructure->setCreateGeneric				(m_data.topGeneric);
		topLevelAccelerationStructure->setInactiveInstances			(inactiveInstances);
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
					topLevelAccelerationStructureCopy = makeTopLevelAccelerationStructure();
					topLevelAccelerationStructureCopy->setDeferredOperation(htCopy, workerThreadsCount);
					topLevelAccelerationStructureCopy->setBuildType(m_data.buildType);
					topLevelAccelerationStructureCopy->setBuildFlags(m_data.buildFlags);
					topLevelAccelerationStructureCopy->setBuildWithoutPrimitives(topNoPrimitives);
					topLevelAccelerationStructureCopy->setInactiveInstances(inactiveInstances);
					topLevelAccelerationStructureCopy->setUseArrayOfPointers(m_data.topUsesAOP);
					topLevelAccelerationStructureCopy->setCreateGeneric(m_data.topGeneric);
					topLevelAccelerationStructureCopy->createAndCopyFrom(vkd, device, *cmdBuffer, allocator, topLevelAccelerationStructure.get(), 0u, 0u);
					break;
				}
				case OP_COMPACT:
				{
					topLevelAccelerationStructureCopy = makeTopLevelAccelerationStructure();
					topLevelAccelerationStructureCopy->setBuildType(m_data.buildType);
					topLevelAccelerationStructureCopy->setBuildFlags(m_data.buildFlags);
					topLevelAccelerationStructureCopy->setBuildWithoutPrimitives(topNoPrimitives);
					topLevelAccelerationStructureCopy->setInactiveInstances(inactiveInstances);
					topLevelAccelerationStructureCopy->setUseArrayOfPointers(m_data.topUsesAOP);
					topLevelAccelerationStructureCopy->setCreateGeneric(m_data.topGeneric);
					topLevelAccelerationStructureCopy->createAndCopyFrom(vkd, device, *cmdBuffer, allocator, topLevelAccelerationStructure.get(), topBlasCompactSize[0], 0u);
					break;
				}
				case OP_SERIALIZE:
				{
					de::SharedPtr<SerialStorage> storage = de::SharedPtr<SerialStorage>(new SerialStorage(vkd, device, allocator, m_data.buildType, topBlasSerialSize[0]));

					topLevelAccelerationStructure->setDeferredOperation(htSerialize, workerThreadsCount);
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
					topLevelAccelerationStructureCopy->setBuildFlags(m_data.buildFlags);
					topLevelAccelerationStructureCopy->setBuildWithoutPrimitives(topNoPrimitives);
					topLevelAccelerationStructureCopy->setInactiveInstances(inactiveInstances);
					topLevelAccelerationStructureCopy->setUseArrayOfPointers(m_data.topUsesAOP);
					topLevelAccelerationStructureCopy->setCreateGeneric(m_data.topGeneric);
					topLevelAccelerationStructureCopy->setDeferredOperation(htSerialize, workerThreadsCount);
					topLevelAccelerationStructureCopy->createAndDeserializeFrom(vkd, device, *cmdBuffer, allocator, storage.get(), 0u);
					break;
				}
				default:
					DE_ASSERT(DE_FALSE);
			}
			topLevelRayTracedPtr = topLevelAccelerationStructureCopy.get();
		}

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

		const VkMemoryBarrier				postTraceMemoryBarrier	= makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
		const VkMemoryBarrier				postCopyMemoryBarrier	= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);

		vkd.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **resultBuffer, 1u, &resultBufferImageRegion);

		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &postCopyMemoryBarrier);
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

// Tests dynamic indexing of acceleration structures
class RayTracingASDynamicIndexingTestCase : public TestCase
{
public:
						RayTracingASDynamicIndexingTestCase			(tcu::TestContext& context, const char* name);
						~RayTracingASDynamicIndexingTestCase		(void) = default;

	void				checkSupport								(Context& context) const override;
	void				initPrograms								(SourceCollections& programCollection) const override;
	TestInstance*		createInstance								(Context& context) const override;
};

class RayTracingASDynamicIndexingTestInstance : public TestInstance
{
public:
						RayTracingASDynamicIndexingTestInstance		(Context& context);
						~RayTracingASDynamicIndexingTestInstance	(void) = default;
	tcu::TestStatus		iterate										(void) override;
};

RayTracingASDynamicIndexingTestCase::RayTracingASDynamicIndexingTestCase(tcu::TestContext& context, const char* name)
	: TestCase(context, name, "")
{
}

void RayTracingASDynamicIndexingTestCase::checkSupport(Context& context) const
{
	commonASTestsCheckSupport(context);
	context.requireDeviceFunctionality("VK_EXT_descriptor_indexing");
}

void RayTracingASDynamicIndexingTestCase::initPrograms(SourceCollections& programCollection) const
{
	const vk::SpirVAsmBuildOptions spvBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, true);
	const vk::ShaderBuildOptions glslBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	// raygen shader is defined in spir-v as it requires possing pointer to TLAS that was read from ssbo;
	// original spir-v code was generated using following glsl code but resulting spir-v code was modiifed
	//
	// #version 460 core
	// #extension GL_EXT_ray_tracing : require
	// #extension GL_EXT_nonuniform_qualifier : enable
	// #define ARRAY_SIZE 500
	// layout(location = 0) rayPayloadEXT uvec2 payload;	// offset and flag indicating if we are using descriptors or pointers

	// layout(set = 0, binding = 0) uniform accelerationStructureEXT tlasArray[ARRAY_SIZE];
	// layout(set = 0, binding = 1) readonly buffer topLevelASPointers {
	//     uvec2 ptr[];
	// } tlasPointers;
	// layout(set = 0, binding = 2) readonly buffer topLevelASIndices {
	//     uint idx[];
	// } tlasIndices;
	// layout(set = 0, binding = 3, std430) writeonly buffer Result {
	//     uint value[];
	// } result;

	// void main()
	// {
	//   float tmin            = 0.0;\n"
	//   float tmax            = 2.0;\n"
	//   vec3  origin          = vec3(0.25f, 0.5f, 1.0);\n"
	//   vec3  direction       = vec3(0.0,0.0,-1.0);\n"
	//   uint  activeTlasIndex = gl_LaunchIDEXT.x;\n"
	//   uint  activeTlasCount = gl_LaunchSizeEXT.x;\n"
	//   uint  tlasIndex       = tlasIndices.idx[nonuniformEXT(activeTlasIndex)];\n"

	//   atomicAdd(result.value[nonuniformEXT(activeTlasIndex)], 2);\n"
	//   payload = uvec2(activeTlasIndex + activeTlasCount.x, 0);\n"
	//   traceRayEXT(tlasArray[nonuniformEXT(tlasIndex)], gl_RayFlagsCullBackFacingTrianglesEXT, 0xFF, 0, 0, 0, origin, tmin, direction, tmax, 0);\n"

	//   atomicAdd(result.value[nonuniformEXT(activeTlasIndex + activeTlasCount * 2)], 5);\n"
	//   payload = uvec2(activeTlasIndex + activeTlasCount * 3, 1);\n"
	//   traceRayEXT(tlasArray[nonuniformEXT(tlasIndex)], gl_RayFlagsCullBackFacingTrianglesEXT, 0xFF, 0, 0, 0, origin, tmin, direction, tmax, 0);				// used to generate initial spirv
	//   //traceRayEXT(*tlasPointers.ptr[nonuniformEXT(tlasIndex)], gl_RayFlagsCullBackFacingTrianglesEXT, 0xFF, 0, 0, 0, origin, tmin, direction, tmax, 0);	// not available in glsl but should be done in spirv
	// };

	const std::string rgenSource =
		"OpCapability RayTracingKHR\n"
		"OpCapability ShaderNonUniform\n"
		"OpExtension \"SPV_EXT_descriptor_indexing\"\n"
		"OpExtension \"SPV_KHR_ray_tracing\"\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint RayGenerationKHR %4 \"main\" %27 %33 %var_tlas_indices %var_result %60 %var_as_arr_ptr %var_as_pointers_ssbo\n"
		"OpDecorate %27 BuiltIn LaunchIdNV\n"
		"OpDecorate %33 BuiltIn LaunchSizeNV\n"
		"OpDecorate %37 ArrayStride 4\n"
		"OpMemberDecorate %38 0 NonWritable\n"
		"OpMemberDecorate %38 0 Offset 0\n"
		"OpDecorate %38 Block\n"
		"OpDecorate %var_tlas_indices DescriptorSet 0\n"
		"OpDecorate %var_tlas_indices Binding 2\n"
		"OpDecorate %44 NonUniform\n"
		"OpDecorate %46 NonUniform\n"
		"OpDecorate %47 NonUniform\n"
		"OpDecorate %48 ArrayStride 4\n"
		"OpMemberDecorate %49 0 NonReadable\n"
		"OpMemberDecorate %49 0 Offset 0\n"
		"OpDecorate %49 Block\n"
		"OpDecorate %var_result DescriptorSet 0\n"
		"OpDecorate %var_result Binding 3\n"
		"OpDecorate %53 NonUniform\n"
		"OpDecorate %60 Location 0\n"
		"OpDecorate %var_as_arr_ptr DescriptorSet 0\n"
		"OpDecorate %var_as_arr_ptr Binding 0\n"
		"OpDecorate %71 NonUniform\n"
		"OpDecorate %73 NonUniform\n"
		"OpDecorate %74 NonUniform\n"
		"OpDecorate %85 NonUniform\n"
		"OpDecorate %as_index NonUniform\n"
		"OpDecorate %as_device_addres NonUniform\n"
		"OpDecorate %104 ArrayStride 8\n"
		"OpMemberDecorate %105 0 NonWritable\n"
		"OpMemberDecorate %105 0 Offset 0\n"
		"OpDecorate %105 Block\n"
		"OpDecorate %var_as_pointers_ssbo DescriptorSet 0\n"
		"OpDecorate %var_as_pointers_ssbo Binding 1\n"
		// types, constants and variables
		"%2								= OpTypeVoid\n"
		"%3								= OpTypeFunction %2\n"
		"%6								= OpTypeFloat 32\n"
		"%7								= OpTypePointer Function %6\n"
		"%9								= OpConstant %6 0\n"
		"%11							= OpConstant %6 2\n"
		"%12							= OpTypeVector %6 3\n"
		"%13							= OpTypePointer Function %12\n"
		"%15							= OpConstant %6 0.25\n"
		"%16							= OpConstant %6 0.5\n"
		"%17							= OpConstant %6 1\n"
		"%18							= OpConstantComposite %12 %15 %16 %17\n"
		"%20							= OpConstant %6 -1\n"
		"%21							= OpConstantComposite %12 %9 %9 %20\n"
		"%type_uint32					= OpTypeInt 32 0\n"
		"%23							= OpTypePointer Function %type_uint32\n"
		"%25							= OpTypeVector %type_uint32 3\n"
		"%26							= OpTypePointer Input %25\n"
		"%27							= OpVariable %26 Input\n"
		"%28							= OpConstant %type_uint32 0\n"
		"%29							= OpTypePointer Input %type_uint32\n"
		"%33							= OpVariable %26 Input\n"
		"%37							= OpTypeRuntimeArray %type_uint32\n"
		"%38							= OpTypeStruct %37\n"
		"%39							= OpTypePointer StorageBuffer %38\n"
		"%var_tlas_indices				= OpVariable %39 StorageBuffer\n"
		"%type_int32					= OpTypeInt 32 1\n"
		"%c_int32_0						= OpConstant %type_int32 0\n"
		"%45							= OpTypePointer StorageBuffer %type_uint32\n"
		"%48							= OpTypeRuntimeArray %type_uint32\n"
		"%49							= OpTypeStruct %48\n"
		"%50							= OpTypePointer StorageBuffer %49\n"
		"%var_result					= OpVariable %50 StorageBuffer\n"
		"%55							= OpConstant %type_uint32 2\n"
		"%56							= OpConstant %type_uint32 1\n"
		"%58							= OpTypeVector %type_uint32 2\n"
		"%59							= OpTypePointer RayPayloadNV %58\n"
		"%60							= OpVariable %59 RayPayloadNV\n"
		"%type_as						= OpTypeAccelerationStructureKHR\n"
		"%66							= OpConstant %type_uint32 500\n"
		"%67							= OpTypeArray %type_as %66\n"
		"%68							= OpTypePointer UniformConstant %67\n"
		"%var_as_arr_ptr				= OpVariable %68 UniformConstant\n"
		"%72							= OpTypePointer UniformConstant %type_as\n"
		"%75							= OpConstant %type_uint32 16\n"
		"%76							= OpConstant %type_uint32 255\n"
		"%87							= OpConstant %type_uint32 5\n"
		"%91							= OpConstant %type_uint32 3\n"

		// <changed_section>
		"%104							= OpTypeRuntimeArray %58\n"
		"%105							= OpTypeStruct %104\n"
		"%106							= OpTypePointer StorageBuffer %105\n"
		"%var_as_pointers_ssbo			= OpVariable %106 StorageBuffer\n"
		"%type_uint64_ssbo_ptr			= OpTypePointer StorageBuffer %58\n"
		// </changed_section>

		// void main()
		"%4								= OpFunction %2 None %3\n"
		"%5								= OpLabel\n"
		"%8								= OpVariable %7 Function\n"
		"%10							= OpVariable %7 Function\n"
		"%14							= OpVariable %13 Function\n"
		"%19							= OpVariable %13 Function\n"
		"%24							= OpVariable %23 Function\n"
		"%32							= OpVariable %23 Function\n"
		"%36							= OpVariable %23 Function\n"
		"OpStore %8 %9\n"
		"OpStore %10 %11\n"
		"OpStore %14 %18\n"
		"OpStore %19 %21\n"
		"%30							= OpAccessChain %29 %27 %28\n"
		"%31							= OpLoad %type_uint32 %30\n"
		"OpStore %24 %31\n"
		"%34							= OpAccessChain %29 %33 %28\n"
		"%35							= OpLoad %type_uint32 %34\n"
		"OpStore %32 %35\n"
		"%43							= OpLoad %type_uint32 %24\n"
		"%44							= OpCopyObject %type_uint32 %43\n"
		"%46							= OpAccessChain %45 %var_tlas_indices %c_int32_0 %44\n"
		"%47							= OpLoad %type_uint32 %46\n"
		"OpStore %36 %47\n"
		// atomicAdd
		"%52							= OpLoad %type_uint32 %24\n"
		"%53							= OpCopyObject %type_uint32 %52\n"
		"%54							= OpAccessChain %45 %var_result %c_int32_0 %53\n"
		"%57							= OpAtomicIAdd %type_uint32 %54 %56 %28 %55\n"
		// setup payload
		"%61							= OpLoad %type_uint32 %24\n"
		"%62							= OpLoad %type_uint32 %32\n"
		"%63							= OpIAdd %type_uint32 %61 %62\n"
		"%64							= OpCompositeConstruct %58 %63 %28\n"
		"OpStore %60 %64\n"
		// trace rays using tlas from array
		"%70							= OpLoad %type_uint32 %36\n"
		"%71							= OpCopyObject %type_uint32 %70\n"
		"%73							= OpAccessChain %72 %var_as_arr_ptr %71\n"
		"%74							= OpLoad %type_as %73\n"
		"%77							= OpLoad %12 %14\n"
		"%78							= OpLoad %6 %8\n"
		"%79							= OpLoad %12 %19\n"
		"%80							= OpLoad %6 %10\n"
		"OpTraceRayKHR %74 %75 %76 %28 %28 %28 %77 %78 %79 %80 %60\n"
		// atomicAdd
		"%81							= OpLoad %type_uint32 %24\n"
		"%82							= OpLoad %type_uint32 %32\n"
		"%83							= OpIMul %type_uint32 %82 %55\n"
		"%84							= OpIAdd %type_uint32 %81 %83\n"
		"%85							= OpCopyObject %type_uint32 %84\n"
		"%86							= OpAccessChain %45 %var_result %c_int32_0 %85\n"
		"%88							= OpAtomicIAdd %type_uint32 %86 %56 %28 %87\n"
		// setup payload
		"%89							= OpLoad %type_uint32 %24\n"
		"%90							= OpLoad %type_uint32 %32\n"
		"%92							= OpIMul %type_uint32 %90 %91\n"
		"%93							= OpIAdd %type_uint32 %89 %92\n"
		"%94							= OpCompositeConstruct %58 %93 %56\n"
		"OpStore %60 %94\n"
		// trace rays using pointers to tlas
		"%95							= OpLoad %type_uint32 %36\n"
		"%as_index						= OpCopyObject %type_uint32 %95\n"

		// <changed_section> OLD
		"%as_device_addres_ptr			= OpAccessChain %type_uint64_ssbo_ptr %var_as_pointers_ssbo %c_int32_0 %as_index\n"
		"%as_device_addres				= OpLoad %58 %as_device_addres_ptr\n"
		"%as_to_use						= OpConvertUToAccelerationStructureKHR %type_as %as_device_addres\n"
		// </changed_section>

		"%99							= OpLoad %12 %14\n"
		"%100							= OpLoad %6 %8\n"
		"%101							= OpLoad %12 %19\n"
		"%102							= OpLoad %6 %10\n"
		"OpTraceRayKHR %as_to_use %75 %76 %28 %28 %28 %99 %100 %101 %102 %60\n"
		"OpReturn\n"
		"OpFunctionEnd\n";
	programCollection.spirvAsmSources.add("rgen") << rgenSource << spvBuildOptions;

	std::string chitSource =
		"#version 460 core\n"
		"#extension GL_EXT_ray_tracing : require\n"
		"#extension GL_EXT_nonuniform_qualifier : enable\n"
		"layout(location = 0) rayPayloadInEXT uvec2 payload;\n"
		"\n"
		"layout(set = 0, binding = 3) writeonly buffer Result {\n"
		"    uint value[];\n"
		"} result;\n"
		"void main()\n"
		"{\n"
		     // payload.y is 0 or 1 so we will add 3 or 7 (just two prime numbers)
		"    atomicAdd(result.value[nonuniformEXT(payload.x)], 3 + payload.y * 4);\n"
		"}\n";
	programCollection.glslSources.add("chit") << glu::ClosestHitSource(chitSource) << glslBuildOptions;
}

TestInstance* RayTracingASDynamicIndexingTestCase::createInstance(Context& context) const
{
	return new RayTracingASDynamicIndexingTestInstance(context);
}

RayTracingASDynamicIndexingTestInstance::RayTracingASDynamicIndexingTestInstance(Context& context)
	: vkt::TestInstance(context)
{
}

tcu::TestStatus RayTracingASDynamicIndexingTestInstance::iterate(void)
{
	const InstanceInterface&	vki							= m_context.getInstanceInterface();
	const DeviceInterface&		vkd							= m_context.getDeviceInterface();
	const VkDevice				device						= m_context.getDevice();
	const VkPhysicalDevice		physicalDevice				= m_context.getPhysicalDevice();
	const deUint32				queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
	const VkQueue				queue						= m_context.getUniversalQueue();
	Allocator&					allocator					= m_context.getDefaultAllocator();
	const deUint32				shaderGroupHandleSize		= getShaderGroupSize(vki, physicalDevice);
	const deUint32				shaderGroupBaseAlignment	= getShaderGroupBaseAlignment(vki, physicalDevice);
	const deUint32				tlasCount					= 500;	// changing this will require also changing shaders
	const deUint32				activeTlasCount				= 32;	// number of tlas out of <tlasCount> that will be active

	const Move<VkDescriptorSetLayout> descriptorSetLayout = DescriptorSetLayoutBuilder()
		.addArrayBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, tlasCount, ALL_RAY_TRACING_STAGES)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ALL_RAY_TRACING_STAGES)				// pointers to all acceleration structures
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ALL_RAY_TRACING_STAGES)				// ssbo with indices of all acceleration structures
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ALL_RAY_TRACING_STAGES)				// ssbo with result values
		.build(vkd, device);

	const Move<VkDescriptorPool> descriptorPool = DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, tlasCount)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const Move<VkDescriptorSet> descriptorSet = makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout);

	de::MovePtr<RayTracingPipeline> rayTracingPipeline = de::newMovePtr<RayTracingPipeline>();
	rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,      createShaderModule(vkd, device, m_context.getBinaryCollection().get("rgen"), 0), 0);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, createShaderModule(vkd, device, m_context.getBinaryCollection().get("chit"), 0), 1);

	const Move<VkPipelineLayout>			pipelineLayout						= makePipelineLayout(vkd, device, descriptorSetLayout.get());
	Move<VkPipeline>						pipeline							= rayTracingPipeline->createPipeline(vkd, device, *pipelineLayout);
	de::MovePtr<BufferWithMemory>			raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
	de::MovePtr<BufferWithMemory>			hitShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1);

	const VkStridedDeviceAddressRegionKHR	raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR	missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	const VkStridedDeviceAddressRegionKHR	hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR	callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

	const VkDeviceSize						pointerBufferSize		= tlasCount * sizeof(VkDeviceAddress);
	const VkBufferCreateInfo				pointerBufferCreateInfo	= makeBufferCreateInfo(pointerBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	de::MovePtr<BufferWithMemory>			pointerBuffer			= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, pointerBufferCreateInfo, MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress));

	const VkDeviceSize						indicesBufferSize		= activeTlasCount * sizeof(deUint32);
	const VkBufferCreateInfo				indicesBufferCreateInfo	= makeBufferCreateInfo(indicesBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	de::MovePtr<BufferWithMemory>			indicesBuffer			= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, indicesBufferCreateInfo, MemoryRequirement::HostVisible));

	const VkDeviceSize						resultBufferSize		= activeTlasCount * sizeof(deUint32) * 4;
	const VkBufferCreateInfo				resultBufferCreateInfo	= makeBufferCreateInfo(resultBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	de::MovePtr<BufferWithMemory>			resultBuffer			= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));

	const Move<VkCommandPool>				cmdPool					= createCommandPool(vkd, device, 0, queueFamilyIndex);
	const Move<VkCommandBuffer>				cmdBuffer				= allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	de::SharedPtr<BottomLevelAccelerationStructure>				blas = de::SharedPtr<BottomLevelAccelerationStructure>(makeBottomLevelAccelerationStructure().release());
	std::vector<de::MovePtr<TopLevelAccelerationStructure>>		tlasVect(tlasCount);
	std::vector<VkDeviceAddress>								tlasPtrVect(tlasCount);
	std::vector<VkAccelerationStructureKHR>						tlasVkVect;

	// randomly scatter active AS across the range
	deRandom rnd;
	deRandom_init(&rnd, 123);
	std::set<deUint32> asIndicesSet;
	while (asIndicesSet.size() < activeTlasCount)
		asIndicesSet.insert(deRandom_getUint32(&rnd) % tlasCount);

	// fill indices buffer
	deUint32 helperIndex = 0;
	auto& indicesBufferAlloc	= indicesBuffer->getAllocation();
	deUint32* indicesBufferPtr	= reinterpret_cast<deUint32*>(indicesBufferAlloc.getHostPtr());
	std::for_each(asIndicesSet.begin(), asIndicesSet.end(),
		[&helperIndex, indicesBufferPtr](const deUint32& index)
		{
			indicesBufferPtr[helperIndex++] = index;
		});
	vk::flushAlloc(vkd, device, indicesBufferAlloc);

	// clear result buffer
	auto& resultBufferAlloc		= resultBuffer->getAllocation();
	void* resultBufferPtr		= resultBufferAlloc.getHostPtr();
	deMemset(resultBufferPtr, 0, static_cast<size_t>(resultBufferSize));
	vk::flushAlloc(vkd, device, resultBufferAlloc);

	beginCommandBuffer(vkd, *cmdBuffer, 0u);
	{
		// build bottom level acceleration structure
		blas->setGeometryData(
			{
				{ 0.0, 0.0, 0.0 },
				{ 1.0, 0.0, 0.0 },
				{ 0.0, 1.0, 0.0 },
			},
			true,
			VK_GEOMETRY_OPAQUE_BIT_KHR
		);

		blas->createAndBuild(vkd, device, *cmdBuffer, allocator);

		// build top level acceleration structures
		for (deUint32 tlasIndex = 0; tlasIndex < tlasCount; ++tlasIndex)
		{
			auto& tlas = tlasVect[tlasIndex];
			tlas = makeTopLevelAccelerationStructure();
			tlas->setInstanceCount(1);
			tlas->addInstance(blas);
			if (!asIndicesSet.count(tlasIndex))
			{
				// tlas that are not in asIndicesSet should be empty but it is hard to do
				// that with current cts utils so we are marking them as inactive instead
				tlas->setInactiveInstances(true);
			}
			tlas->createAndBuild(vkd, device, *cmdBuffer, allocator);

			// get acceleration structure device address
			const VkAccelerationStructureDeviceAddressInfoKHR addressInfo =
			{
				VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,	// VkStructureType				sType
				DE_NULL,															// const void*					pNext
				*tlas->getPtr()														// VkAccelerationStructureKHR	accelerationStructure
			};
			VkDeviceAddress vkda = vkd.getAccelerationStructureDeviceAddressKHR(device, &addressInfo);
			tlasPtrVect[tlasIndex] = vkda;
		}

		// fill pointer buffer
		vkd.cmdUpdateBuffer(*cmdBuffer, **pointerBuffer, 0, pointerBufferSize, tlasPtrVect.data());

		// wait for data transfers
		const VkMemoryBarrier bufferUploadBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, &bufferUploadBarrier, 1u);

		// wait for as build
		const VkMemoryBarrier asBuildBarrier = makeMemoryBarrier(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);
		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, &asBuildBarrier, 1u);

		tlasVkVect.reserve(tlasCount);
		for (auto& tlas : tlasVect)
			tlasVkVect.push_back(*tlas->getPtr());

		VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureWriteDescriptorSet =
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	// VkStructureType						sType;
			DE_NULL,															// const void*							pNext;
			tlasCount,															// deUint32								accelerationStructureCount;
			tlasVkVect.data(),													// const VkAccelerationStructureKHR*	pAccelerationStructures;
		};

		const vk::VkDescriptorBufferInfo pointerBufferInfo	= makeDescriptorBufferInfo(**pointerBuffer, 0u, VK_WHOLE_SIZE);
		const vk::VkDescriptorBufferInfo indicesBufferInfo	= makeDescriptorBufferInfo(**indicesBuffer, 0u, VK_WHOLE_SIZE);
		const vk::VkDescriptorBufferInfo resultInfo			= makeDescriptorBufferInfo(**resultBuffer,  0u, VK_WHOLE_SIZE);

		DescriptorSetUpdateBuilder()
			.writeArray (*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, tlasCount, &accelerationStructureWriteDescriptorSet)
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &pointerBufferInfo)
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &indicesBufferInfo)
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(3u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultInfo)
			.update(vkd, device);

		vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipelineLayout, 0, 1, &descriptorSet.get(), 0, DE_NULL);

		vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipeline);

		cmdTraceRays(vkd,
			*cmdBuffer,
			&raygenShaderBindingTableRegion,
			&missShaderBindingTableRegion,
			&hitShaderBindingTableRegion,
			&callableShaderBindingTableRegion,
			activeTlasCount, 1, 1);

		const VkMemoryBarrier postTraceMemoryBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);
	}
	endCommandBuffer(vkd, *cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

	invalidateMappedMemoryRange(vkd, device, resultBuffer->getAllocation().getMemory(), resultBuffer->getAllocation().getOffset(), resultBufferSize);

	// verify result buffer
	deUint32		failures	= 0;
	const deUint32*	resultPtr	= reinterpret_cast<deUint32*>(resultBuffer->getAllocation().getHostPtr());
	for (deUint32 index = 0; index < activeTlasCount; ++index)
	{
		failures += (resultPtr[0 * activeTlasCount + index] != 2) +
					(resultPtr[1 * activeTlasCount + index] != 3) +
					(resultPtr[2 * activeTlasCount + index] != 5) +
					(resultPtr[3 * activeTlasCount + index] != 7);
	}

	if (failures)
		return tcu::TestStatus::fail(de::toString(failures) + " failures, " + de::toString(4 * activeTlasCount - failures) + " are ok");
	return tcu::TestStatus::pass("Pass");
}

}	// anonymous

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
		{ VK_BUILD_ACCELERATION_STRUCTURE_LOW_MEMORY_BIT_KHR,			"lowmemory" },
	};

	struct
	{
		bool		padVertices;
		const char*	name;
	} paddingType[] =
	{
		{ false,	"nopadding"	},
		{ true,		"padded"	},
	};

	struct
	{
		bool		topGeneric;
		bool		bottomGeneric;
		const char*	suffix;
	} createGenericParams[] =
	{
		{	false,	false,	""					},
		{	false,	true,	"_bottomgeneric"	},
		{	true,	false,	"_topgeneric"		},
		{	true,	true,	"_bothgeneric"		},
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

				for (int paddingTypeIdx = 0; paddingTypeIdx < DE_LENGTH_OF_ARRAY(paddingType); ++paddingTypeIdx)
				{
					de::MovePtr<tcu::TestCaseGroup> paddingGroup(new tcu::TestCaseGroup(group->getTestContext(), paddingType[paddingTypeIdx].name, ""));

					for (size_t optimizationNdx = 0; optimizationNdx < DE_LENGTH_OF_ARRAY(optimizationTypes); ++optimizationNdx)
					{
						for (size_t updateNdx = 0; updateNdx < DE_LENGTH_OF_ARRAY(updateTypes); ++updateNdx)
						{
							for (size_t compactionNdx = 0; compactionNdx < DE_LENGTH_OF_ARRAY(compactionTypes); ++compactionNdx)
							{
								for (size_t lowMemoryNdx = 0; lowMemoryNdx < DE_LENGTH_OF_ARRAY(lowMemoryTypes); ++lowMemoryNdx)
								{
									for (int createGenericIdx = 0; createGenericIdx < DE_LENGTH_OF_ARRAY(createGenericParams); ++createGenericIdx)
									{
										std::string testName =
											std::string(optimizationTypes[optimizationNdx].name) + "_" +
											std::string(updateTypes[updateNdx].name) + "_" +
											std::string(compactionTypes[compactionNdx].name) + "_" +
											std::string(lowMemoryTypes[lowMemoryNdx].name) +
											std::string(createGenericParams[createGenericIdx].suffix);

										TestParams testParams
										{
											buildTypes[buildTypeNdx].buildType,
											VK_FORMAT_R32G32B32_SFLOAT,
											paddingType[paddingTypeIdx].padVertices,
											VK_INDEX_TYPE_NONE_KHR,
											bottomTestTypes[bottomNdx].testType,
											InstanceCullFlags::NONE,
											bottomTestTypes[bottomNdx].usesAOP,
											createGenericParams[createGenericIdx].bottomGeneric,
											topTestTypes[topNdx].testType,
											topTestTypes[topNdx].usesAOP,
											createGenericParams[createGenericIdx].topGeneric,
											optimizationTypes[optimizationNdx].flags | updateTypes[updateNdx].flags | compactionTypes[compactionNdx].flags | lowMemoryTypes[lowMemoryNdx].flags,
											OT_NONE,
											OP_NONE,
											RTAS_DEFAULT_SIZE,
											RTAS_DEFAULT_SIZE,
											de::SharedPtr<TestConfiguration>(new CheckerboardConfiguration()),
											0u,
											EmptyAccelerationStructureCase::NOT_EMPTY,
											InstanceCustomIndexCase::NONE,
										};
										paddingGroup->addChild(new RayTracingASBasicTestCase(group->getTestContext(), testName.c_str(), "", testParams));
									}
								}
							}
						}
					}
					topGroup->addChild(paddingGroup.release());
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
	struct
	{
		vk::VkAccelerationStructureBuildTypeKHR				buildType;
		const char*											name;
	} buildTypes[] =
	{
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR,	"cpu_built"	},
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,	"gpu_built"	},
	};

	const VkFormat vertexFormats[] =
	{
		// Mandatory formats.
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R16G16_SNORM,
		VK_FORMAT_R16G16B16A16_SNORM,

		// Additional formats.
		VK_FORMAT_R8G8_SNORM,
		VK_FORMAT_R8G8B8_SNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R16G16B16_SNORM,
		VK_FORMAT_R16G16B16_SFLOAT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_FORMAT_R64G64_SFLOAT,
		VK_FORMAT_R64G64B64_SFLOAT,
		VK_FORMAT_R64G64B64A64_SFLOAT,
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

	struct
	{
		bool		padVertices;
		const char*	name;
	} paddingType[] =
	{
		{ false,	"nopadding"	},
		{ true,		"padded"	},
	};

	for (size_t buildTypeNdx = 0; buildTypeNdx < DE_LENGTH_OF_ARRAY(buildTypes); ++buildTypeNdx)
	{
		de::MovePtr<tcu::TestCaseGroup> buildGroup(new tcu::TestCaseGroup(group->getTestContext(), buildTypes[buildTypeNdx].name, ""));

		for (size_t vertexFormatNdx = 0; vertexFormatNdx < DE_LENGTH_OF_ARRAY(vertexFormats); ++vertexFormatNdx)
		{
			const auto format		= vertexFormats[vertexFormatNdx];
			const auto formatName	= getFormatSimpleName(format);

			de::MovePtr<tcu::TestCaseGroup> vertexFormatGroup(new tcu::TestCaseGroup(group->getTestContext(), formatName.c_str(), ""));

			for (int paddingIdx = 0; paddingIdx < DE_LENGTH_OF_ARRAY(paddingType); ++paddingIdx)
			{
				de::MovePtr<tcu::TestCaseGroup> paddingGroup(new tcu::TestCaseGroup(group->getTestContext(), paddingType[paddingIdx].name, ""));

				for (size_t indexFormatNdx = 0; indexFormatNdx < DE_LENGTH_OF_ARRAY(indexFormats); ++indexFormatNdx)
				{
					TestParams testParams
					{
						buildTypes[buildTypeNdx].buildType,
						format,
						paddingType[paddingIdx].padVertices,
						indexFormats[indexFormatNdx].indexType,
						BTT_TRIANGLES,
						InstanceCullFlags::NONE,
						false,
						false,
						TTT_IDENTICAL_INSTANCES,
						false,
						false,
						VkBuildAccelerationStructureFlagsKHR(0u),
						OT_NONE,
						OP_NONE,
						RTAS_DEFAULT_SIZE,
						RTAS_DEFAULT_SIZE,
						de::SharedPtr<TestConfiguration>(new SingleTriangleConfiguration()),
						0u,
						EmptyAccelerationStructureCase::NOT_EMPTY,
						InstanceCustomIndexCase::NONE,
					};
					paddingGroup->addChild(new RayTracingASBasicTestCase(group->getTestContext(), indexFormats[indexFormatNdx].name, "", testParams));
				}
				vertexFormatGroup->addChild(paddingGroup.release());
			}
			buildGroup->addChild(vertexFormatGroup.release());
		}
		group->addChild(buildGroup.release());
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
						false,
						VK_INDEX_TYPE_NONE_KHR,
						bottomTestTypes[testTypeNdx].testType,
						InstanceCullFlags::NONE,
						false,
						false,
						topTest,
						false,
						false,
						VkBuildAccelerationStructureFlagsKHR(0u),
						operationTargets[operationTargetNdx].operationTarget,
						operationTypes[operationTypeNdx].operationType,
						RTAS_DEFAULT_SIZE,
						RTAS_DEFAULT_SIZE,
						de::SharedPtr<TestConfiguration>(new CheckerboardConfiguration()),
						workerThreads,
						EmptyAccelerationStructureCase::NOT_EMPTY,
						InstanceCustomIndexCase::NONE,
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

void addFuncArgTests (tcu::TestCaseGroup* group)
{
	const struct
	{
		vk::VkAccelerationStructureBuildTypeKHR				buildType;
		const char*											name;
	} buildTypes[] =
	{
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR,	"cpu_built"	},
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,	"gpu_built"	},
	};

	auto& ctx = group->getTestContext();

	for (int buildTypeNdx = 0; buildTypeNdx < DE_LENGTH_OF_ARRAY(buildTypes); ++buildTypeNdx)
	{
		TestParams testParams
		{
			buildTypes[buildTypeNdx].buildType,
			VK_FORMAT_R32G32B32_SFLOAT,
			false,
			VK_INDEX_TYPE_NONE_KHR,
			BTT_TRIANGLES,
			InstanceCullFlags::NONE,
			false,
			false,
			TTT_IDENTICAL_INSTANCES,
			false,
			false,
			VkBuildAccelerationStructureFlagsKHR(0u),
			OT_NONE,
			OP_NONE,
			RTAS_DEFAULT_SIZE,
			RTAS_DEFAULT_SIZE,
			de::SharedPtr<TestConfiguration>(new SingleTriangleConfiguration()),
			0u,
			EmptyAccelerationStructureCase::NOT_EMPTY,
			InstanceCustomIndexCase::NONE,
		};

		group->addChild(new RayTracingASFuncArgTestCase(ctx, buildTypes[buildTypeNdx].name, "", testParams));
	}
}

void addInstanceTriangleCullingTests (tcu::TestCaseGroup* group)
{
	const struct
	{
		InstanceCullFlags	cullFlags;
		std::string			name;
	} cullFlags[] =
	{
		{ InstanceCullFlags::NONE,				"noflags"		},
		{ InstanceCullFlags::COUNTERCLOCKWISE,	"ccw"			},
		{ InstanceCullFlags::CULL_DISABLE,		"nocull"		},
		{ InstanceCullFlags::ALL,				"ccw_nocull"	},
	};

	const struct
	{
		TopTestType	topType;
		std::string	name;
	} topType[] =
	{
		{ TTT_DIFFERENT_INSTANCES, "transformed"	},	// Each instance has its own transformation matrix.
		{ TTT_IDENTICAL_INSTANCES, "notransform"	},	// "Identical" instances, different geometries.
	};

	const struct
	{
		vk::VkAccelerationStructureBuildTypeKHR	buildType;
		std::string								name;
	} buildTypes[] =
	{
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR,	"cpu_built"	},
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,	"gpu_built"	},
	};

	const struct
	{
		VkIndexType	indexType;
		std::string	name;
	} indexFormats[] =
	{
		{ VK_INDEX_TYPE_NONE_KHR ,	"index_none"	},
		{ VK_INDEX_TYPE_UINT16 ,	"index_uint16"	},
		{ VK_INDEX_TYPE_UINT32 ,	"index_uint32"	},
	};

	auto& ctx = group->getTestContext();

	for (int buildTypeIdx = 0; buildTypeIdx < DE_LENGTH_OF_ARRAY(buildTypes); ++buildTypeIdx)
	{
		de::MovePtr<tcu::TestCaseGroup> buildTypeGroup(new tcu::TestCaseGroup(ctx, buildTypes[buildTypeIdx].name.c_str(), ""));

		for (int indexFormatIdx = 0; indexFormatIdx < DE_LENGTH_OF_ARRAY(indexFormats); ++indexFormatIdx)
		{
			de::MovePtr<tcu::TestCaseGroup> indexTypeGroup(new tcu::TestCaseGroup(ctx, indexFormats[indexFormatIdx].name.c_str(), ""));

			for (int topTypeIdx = 0; topTypeIdx < DE_LENGTH_OF_ARRAY(topType); ++topTypeIdx)
			{
				for (int cullFlagsIdx = 0; cullFlagsIdx < DE_LENGTH_OF_ARRAY(cullFlags); ++cullFlagsIdx)
				{
					const std::string testName = topType[topTypeIdx].name + "_" + cullFlags[cullFlagsIdx].name;

					TestParams testParams
					{
						buildTypes[buildTypeIdx].buildType,
						VK_FORMAT_R32G32B32_SFLOAT,
						false,
						indexFormats[indexFormatIdx].indexType,
						BTT_TRIANGLES,
						cullFlags[cullFlagsIdx].cullFlags,
						false,
						false,
						topType[topTypeIdx].topType,
						false,
						false,
						VkBuildAccelerationStructureFlagsKHR(0u),
						OT_NONE,
						OP_NONE,
						RTAS_DEFAULT_SIZE,
						RTAS_DEFAULT_SIZE,
						de::SharedPtr<TestConfiguration>(new CheckerboardConfiguration()),
						0u,
						EmptyAccelerationStructureCase::NOT_EMPTY,
						InstanceCustomIndexCase::NONE,
					};
					indexTypeGroup->addChild(new RayTracingASBasicTestCase(ctx, testName.c_str(), "", testParams));
				}
			}
			buildTypeGroup->addChild(indexTypeGroup.release());
		}
		group->addChild(buildTypeGroup.release());
	}
}

void addDynamicIndexingTests (tcu::TestCaseGroup* group)
{
	auto& ctx = group->getTestContext();
	group->addChild(new RayTracingASDynamicIndexingTestCase(ctx, "dynamic_indexing"));
}

void addEmptyAccelerationStructureTests (tcu::TestCaseGroup* group)
{
	const struct
	{
		vk::VkAccelerationStructureBuildTypeKHR				buildType;
		std::string											name;
	} buildTypes[] =
	{
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR,	"cpu_built"	},
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,	"gpu_built"	},
	};

	const struct
	{
		VkIndexType								indexType;
		std::string								name;
	} indexFormats[] =
	{
		{ VK_INDEX_TYPE_NONE_KHR,				"index_none"	},
		{ VK_INDEX_TYPE_UINT16,					"index_uint16"	},
		{ VK_INDEX_TYPE_UINT32,					"index_uint32"	},
	};

	const struct
	{
		EmptyAccelerationStructureCase	emptyASCase;
		std::string						name;
	} emptyCases[] =
	{
		{ EmptyAccelerationStructureCase::INACTIVE_TRIANGLES,	"inactive_triangles"	},
		{ EmptyAccelerationStructureCase::INACTIVE_INSTANCES,	"inactive_instances"	},
		{ EmptyAccelerationStructureCase::NO_GEOMETRIES_BOTTOM,	"no_geometries_bottom"	},
		{ EmptyAccelerationStructureCase::NO_PRIMITIVES_TOP,	"no_primitives_top"		},
		{ EmptyAccelerationStructureCase::NO_PRIMITIVES_BOTTOM,	"no_primitives_bottom"	},
	};

	auto& ctx = group->getTestContext();

	for (int buildTypeIdx = 0; buildTypeIdx < DE_LENGTH_OF_ARRAY(buildTypes); ++buildTypeIdx)
	{
		de::MovePtr<tcu::TestCaseGroup> buildTypeGroup(new tcu::TestCaseGroup(ctx, buildTypes[buildTypeIdx].name.c_str(), ""));

		for (int indexFormatIdx = 0; indexFormatIdx < DE_LENGTH_OF_ARRAY(indexFormats); ++indexFormatIdx)
		{
			de::MovePtr<tcu::TestCaseGroup> indexTypeGroup(new tcu::TestCaseGroup(ctx, indexFormats[indexFormatIdx].name.c_str(), ""));

			for (int emptyCaseIdx = 0; emptyCaseIdx < DE_LENGTH_OF_ARRAY(emptyCases); ++emptyCaseIdx)
			{
				TestParams testParams
				{
					buildTypes[buildTypeIdx].buildType,
					VK_FORMAT_R32G32B32_SFLOAT,
					false,
					indexFormats[indexFormatIdx].indexType,
					BTT_TRIANGLES,
					InstanceCullFlags::NONE,
					false,
					false,
					TTT_IDENTICAL_INSTANCES,
					false,
					false,
					VkBuildAccelerationStructureFlagsKHR(0u),
					OT_NONE,
					OP_NONE,
					RTAS_DEFAULT_SIZE,
					RTAS_DEFAULT_SIZE,
					de::SharedPtr<TestConfiguration>(new SingleTriangleConfiguration()),
					0u,
					emptyCases[emptyCaseIdx].emptyASCase,
					InstanceCustomIndexCase::NONE,
				};
				indexTypeGroup->addChild(new RayTracingASBasicTestCase(ctx, emptyCases[emptyCaseIdx].name.c_str(), "", testParams));
			}
			buildTypeGroup->addChild(indexTypeGroup.release());
		}
		group->addChild(buildTypeGroup.release());
	}
}

void addInstanceIndexTests (tcu::TestCaseGroup* group)
{
	const struct
	{
		vk::VkAccelerationStructureBuildTypeKHR				buildType;
		std::string											name;
	} buildTypes[] =
	{
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR,	"cpu_built"	},
		{ VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,	"gpu_built"	},
	};

	const struct
	{
		InstanceCustomIndexCase						customIndexCase;
		std::string									name;
	} customIndexCases[] =
	{
		{ InstanceCustomIndexCase::NONE,			"no_instance_index"	},
		{ InstanceCustomIndexCase::ANY_HIT,			"ahit"				},
		{ InstanceCustomIndexCase::CLOSEST_HIT,		"chit"				},
		{ InstanceCustomIndexCase::INTERSECTION,	"isec"				},
	};

	auto& ctx = group->getTestContext();

	for (int buildTypeIdx = 0; buildTypeIdx < DE_LENGTH_OF_ARRAY(buildTypes); ++buildTypeIdx)
	{
		de::MovePtr<tcu::TestCaseGroup> buildTypeGroup(new tcu::TestCaseGroup(ctx, buildTypes[buildTypeIdx].name.c_str(), ""));

		for (int customIndexCaseIdx = 0; customIndexCaseIdx < DE_LENGTH_OF_ARRAY(customIndexCases); ++customIndexCaseIdx)
		{
			const auto&	idxCase				= customIndexCases[customIndexCaseIdx].customIndexCase;
			const auto	bottomGeometryType	= ((idxCase == InstanceCustomIndexCase::INTERSECTION) ? BTT_AABBS : BTT_TRIANGLES);

			TestParams testParams
			{
				buildTypes[buildTypeIdx].buildType,
				VK_FORMAT_R32G32B32_SFLOAT,
				false,
				VK_INDEX_TYPE_NONE_KHR,
				bottomGeometryType,
				InstanceCullFlags::NONE,
				false,
				false,
				TTT_IDENTICAL_INSTANCES,
				false,
				false,
				VkBuildAccelerationStructureFlagsKHR(0u),
				OT_NONE,
				OP_NONE,
				RTAS_DEFAULT_SIZE,
				RTAS_DEFAULT_SIZE,
				de::SharedPtr<TestConfiguration>(new CheckerboardConfiguration()),
				0u,
				EmptyAccelerationStructureCase::NOT_EMPTY,
				customIndexCases[customIndexCaseIdx].customIndexCase,
			};
			buildTypeGroup->addChild(new RayTracingASBasicTestCase(ctx, customIndexCases[customIndexCaseIdx].name.c_str(), "", testParams));
		}
		group->addChild(buildTypeGroup.release());
	}
}

tcu::TestCaseGroup*	createAccelerationStructuresTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "acceleration_structures", "Acceleration structure tests"));

	addTestGroup(group.get(), "flags", "Test building AS with different build types, build flags and geometries/instances using arrays or arrays of pointers", addBasicBuildingTests);
	addTestGroup(group.get(), "format", "Test building AS with different vertex and index formats", addVertexIndexFormatsTests);
	addTestGroup(group.get(), "operations", "Test copying, compaction and serialization of AS", addOperationTests);
	addTestGroup(group.get(), "host_threading", "Test host threading operations", addHostThreadingOperationTests);
	addTestGroup(group.get(), "function_argument", "Test using AS as function argument using both pointers and bare values", addFuncArgTests);
	addTestGroup(group.get(), "instance_triangle_culling", "Test building AS with counterclockwise triangles and/or disabling face culling", addInstanceTriangleCullingTests);
	addTestGroup(group.get(), "dynamic_indexing", "Exercise dynamic indexing of acceleration structures", addDynamicIndexingTests);
	addTestGroup(group.get(), "empty", "Test building empty acceleration structures using different methods", addEmptyAccelerationStructureTests);
	addTestGroup(group.get(), "instance_index", "Test using different values for the instance index and checking them in shaders", addInstanceIndexTests);

	return group.release();
}

}	// RayTracing

}	// vkt
