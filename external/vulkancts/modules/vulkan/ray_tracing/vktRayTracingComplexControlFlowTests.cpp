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
 * \brief Ray Tracing Complex Control Flow tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingComplexControlFlowTests.hpp"

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

#include "tcuTestLog.hpp"

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

#if defined(DE_DEBUG)
static const deUint32	PUSH_CONSTANTS_COUNT	= 6;
#endif
static const deUint32	DEFAULT_CLEAR_VALUE		= 999999;

enum TestType
{
	TEST_TYPE_IF						= 0,
	TEST_TYPE_LOOP,
	TEST_TYPE_SWITCH,
	TEST_TYPE_LOOP_DOUBLE_CALL,
	TEST_TYPE_LOOP_DOUBLE_CALL_SPARSE,
	TEST_TYPE_NESTED_LOOP,
	TEST_TYPE_NESTED_LOOP_BEFORE,
	TEST_TYPE_NESTED_LOOP_AFTER,
	TEST_TYPE_FUNCTION_CALL,
	TEST_TYPE_NESTED_FUNCTION_CALL,
};

enum TestOp
{
	TEST_OP_EXECUTE_CALLABLE		= 0,
	TEST_OP_TRACE_RAY,
	TEST_OP_REPORT_INTERSECTION,
};

enum ShaderGroups
{
	FIRST_GROUP		= 0,
	RAYGEN_GROUP	= FIRST_GROUP,
	MISS_GROUP,
	HIT_GROUP,
	GROUP_COUNT
};

struct CaseDef
{
	TestType				testType;
	TestOp					testOp;
	VkShaderStageFlagBits	stage;
	deUint32				width;
	deUint32				height;
};

struct PushConstants
{
	deUint32	a;
	deUint32	b;
	deUint32	c;
	deUint32	d;
	deUint32	hitOfs;
	deUint32	miss;
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
	const VkImageUsageFlags	usage			= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	const VkImageCreateInfo	imageCreateInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		(VkImageCreateFlags)0u,					// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_3D,						// VkImageType				imageType;
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

Move<VkPipelineLayout> makePipelineLayout (const DeviceInterface&		vk,
										   const VkDevice				device,
										   const VkDescriptorSetLayout	descriptorSetLayout,
										   const deUint32				pushConstantsSize)
{
	const VkDescriptorSetLayout*		descriptorSetLayoutPtr	= (descriptorSetLayout == DE_NULL) ? DE_NULL : &descriptorSetLayout;
	const deUint32						setLayoutCount			= (descriptorSetLayout == DE_NULL) ? 0u : 1u;
	const VkPushConstantRange			pushConstantRange		=
	{
		ALL_RAY_TRACING_STAGES,		//  VkShaderStageFlags	stageFlags;
		0u,							//  deUint32			offset;
		pushConstantsSize,			//  deUint32			size;
	};
	const VkPushConstantRange*			pPushConstantRanges		= (pushConstantsSize == 0) ? DE_NULL : &pushConstantRange;
	const deUint32						pushConstantRangeCount	= (pushConstantsSize == 0) ? 0 : 1u;
	const VkPipelineLayoutCreateInfo	pipelineLayoutParams	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,										// const void*						pNext;
		0u,												// VkPipelineLayoutCreateFlags		flags;
		setLayoutCount,									// deUint32							setLayoutCount;
		descriptorSetLayoutPtr,							// const VkDescriptorSetLayout*		pSetLayouts;
		pushConstantRangeCount,							// deUint32							pushConstantRangeCount;
		pPushConstantRanges,							// const VkPushConstantRange*		pPushConstantRanges;
	};

	return createPipelineLayout(vk, device, &pipelineLayoutParams);
}

VkBuffer getVkBuffer (const de::MovePtr<BufferWithMemory>& buffer)
{
	VkBuffer result = (buffer.get() == DE_NULL) ? DE_NULL : buffer->get();

	return result;
}

VkStridedDeviceAddressRegionKHR makeStridedDeviceAddressRegion (const DeviceInterface& vkd, const VkDevice device, VkBuffer buffer, deUint32 stride, deUint32 count)
{
	if (buffer == DE_NULL)
	{
		return makeStridedDeviceAddressRegionKHR(0, 0, 0);
	}
	else
	{
		return makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, buffer, 0), stride, stride * count);
	}
}

// Function replacing all occurrences of substring with string passed in last parameter.
static inline std::string replace(const std::string& str, const std::string& from, const std::string& to)
{
	std::string result(str);

	size_t start_pos = 0;
	while((start_pos = result.find(from, start_pos)) != std::string::npos)
	{
		result.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}

	return result;
}


class RayTracingComplexControlFlowInstance : public TestInstance
{
public:
																RayTracingComplexControlFlowInstance	(Context& context, const CaseDef& data);
																~RayTracingComplexControlFlowInstance	(void);
	tcu::TestStatus												iterate									(void);

protected:
	void														calcShaderGroup							(deUint32&					shaderGroupCounter,
																										 const VkShaderStageFlags	shaders1,
																										 const VkShaderStageFlags	shaders2,
																										 const VkShaderStageFlags	shaderStageFlags,
																										 deUint32&					shaderGroup,
																										 deUint32&					shaderGroupCount) const;
	PushConstants												getPushConstants						(void) const;
	std::vector<deUint32>										getExpectedValues						(void) const;
	de::MovePtr<BufferWithMemory>								runTest									(void);
	Move<VkPipeline>											makePipeline							(de::MovePtr<RayTracingPipeline>&							rayTracingPipeline,
																										 VkPipelineLayout											pipelineLayout);
	de::MovePtr<BufferWithMemory>								createShaderBindingTable				 (const InstanceInterface&									vki,
																										 const DeviceInterface&										vkd,
																										 const VkDevice												device,
																										 const VkPhysicalDevice										physicalDevice,
																										 const VkPipeline											pipeline,
																										 Allocator&													allocator,
																										 de::MovePtr<RayTracingPipeline>&							rayTracingPipeline,
																										 const deUint32												group,
																										 const deUint32												groupCount = 1u);
	de::MovePtr<TopLevelAccelerationStructure>					initTopAccelerationStructure			(VkCommandBuffer											cmdBuffer,
																										 vector<de::SharedPtr<BottomLevelAccelerationStructure> >&	bottomLevelAccelerationStructures);
	vector<de::SharedPtr<BottomLevelAccelerationStructure>	>	initBottomAccelerationStructures		(VkCommandBuffer											cmdBuffer);
	de::MovePtr<BottomLevelAccelerationStructure>				initBottomAccelerationStructure			(VkCommandBuffer											cmdBuffer,
																										 tcu::UVec2&												startPos);

private:
	CaseDef														m_data;
	VkShaderStageFlags											m_shaders;
	VkShaderStageFlags											m_shaders2;
	deUint32													m_raygenShaderGroup;
	deUint32													m_missShaderGroup;
	deUint32													m_hitShaderGroup;
	deUint32													m_callableShaderGroup;
	deUint32													m_raygenShaderGroupCount;
	deUint32													m_missShaderGroupCount;
	deUint32													m_hitShaderGroupCount;
	deUint32													m_callableShaderGroupCount;
	deUint32													m_shaderGroupCount;
	deUint32													m_depth;
	PushConstants												m_pushConstants;
};

RayTracingComplexControlFlowInstance::RayTracingComplexControlFlowInstance (Context& context, const CaseDef& data)
	: vkt::TestInstance				(context)
	, m_data						(data)
	, m_shaders						(0)
	, m_shaders2					(0)
	, m_raygenShaderGroup			(~0u)
	, m_missShaderGroup				(~0u)
	, m_hitShaderGroup				(~0u)
	, m_callableShaderGroup			(~0u)
	, m_raygenShaderGroupCount		(0)
	, m_missShaderGroupCount		(0)
	, m_hitShaderGroupCount			(0)
	, m_callableShaderGroupCount	(0)
	, m_shaderGroupCount			(0)
	, m_depth						(16)
	, m_pushConstants				(getPushConstants())
{
	const VkShaderStageFlags	hitStages	= VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
	BinaryCollection&			collection	= m_context.getBinaryCollection();
	deUint32					shaderCount	= 0;

	if (collection.contains("rgen")) m_shaders |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	if (collection.contains("ahit")) m_shaders |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	if (collection.contains("chit")) m_shaders |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	if (collection.contains("miss")) m_shaders |= VK_SHADER_STAGE_MISS_BIT_KHR;
	if (collection.contains("sect")) m_shaders |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
	if (collection.contains("call")) m_shaders |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;

	if (collection.contains("ahit2")) m_shaders2 |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	if (collection.contains("chit2")) m_shaders2 |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	if (collection.contains("miss2")) m_shaders2 |= VK_SHADER_STAGE_MISS_BIT_KHR;
	if (collection.contains("sect2")) m_shaders2 |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

	if (collection.contains("cal0")) m_shaders2 |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;

	for (BinaryCollection::Iterator it = collection.begin(); it != collection.end(); ++it)
		shaderCount++;

	if (shaderCount != (deUint32)dePop32(m_shaders) + (deUint32)dePop32(m_shaders2))
		TCU_THROW(InternalError, "Unused shaders detected in the collection");

	calcShaderGroup(m_shaderGroupCount, m_shaders, m_shaders2, VK_SHADER_STAGE_RAYGEN_BIT_KHR,   m_raygenShaderGroup,   m_raygenShaderGroupCount);
	calcShaderGroup(m_shaderGroupCount, m_shaders, m_shaders2, VK_SHADER_STAGE_MISS_BIT_KHR,     m_missShaderGroup,     m_missShaderGroupCount);
	calcShaderGroup(m_shaderGroupCount, m_shaders, m_shaders2, hitStages,                        m_hitShaderGroup,      m_hitShaderGroupCount);
	calcShaderGroup(m_shaderGroupCount, m_shaders, m_shaders2, VK_SHADER_STAGE_CALLABLE_BIT_KHR, m_callableShaderGroup, m_callableShaderGroupCount);
}

RayTracingComplexControlFlowInstance::~RayTracingComplexControlFlowInstance (void)
{
}

void RayTracingComplexControlFlowInstance::calcShaderGroup (deUint32&					shaderGroupCounter,
															const VkShaderStageFlags	shaders1,
															const VkShaderStageFlags	shaders2,
															const VkShaderStageFlags	shaderStageFlags,
															deUint32&					shaderGroup,
															deUint32&					shaderGroupCount) const
{
	const deUint32	shader1Count = ((shaders1 & shaderStageFlags) != 0) ? 1 : 0;
	const deUint32	shader2Count = ((shaders2 & shaderStageFlags) != 0) ? 1 : 0;

	shaderGroupCount = shader1Count + shader2Count;

	if (shaderGroupCount != 0)
	{
		shaderGroup			= shaderGroupCounter;
		shaderGroupCounter += shaderGroupCount;
	}
}

Move<VkPipeline> RayTracingComplexControlFlowInstance::makePipeline (de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
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

	if (0 != (m_shaders2 & VK_SHADER_STAGE_CALLABLE_BIT_KHR))		rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR		, createShaderModule(vkd, device, collection.get("cal0"), 0), m_callableShaderGroup);
	if (0 != (m_shaders2 & VK_SHADER_STAGE_ANY_HIT_BIT_KHR))		rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR		, createShaderModule(vkd, device, collection.get("ahit2"), 0), m_hitShaderGroup + 1);
	if (0 != (m_shaders2 & VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR))	rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR	, createShaderModule(vkd, device, collection.get("chit2"), 0), m_hitShaderGroup + 1);
	if (0 != (m_shaders2 & VK_SHADER_STAGE_MISS_BIT_KHR))			rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR			, createShaderModule(vkd, device, collection.get("miss2"), 0), m_missShaderGroup + 1);
	if (0 != (m_shaders2 & VK_SHADER_STAGE_INTERSECTION_BIT_KHR))	rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR	, createShaderModule(vkd, device, collection.get("sect2"), 0), m_hitShaderGroup + 1);

	if (m_data.testOp == TEST_OP_TRACE_RAY && m_data.stage != VK_SHADER_STAGE_RAYGEN_BIT_KHR)
		rayTracingPipeline->setMaxRecursionDepth(2);

	Move<VkPipeline> pipeline = rayTracingPipeline->createPipeline(vkd, device, pipelineLayout);

	return pipeline;
}

de::MovePtr<BufferWithMemory> RayTracingComplexControlFlowInstance::createShaderBindingTable (const InstanceInterface&			vki,
																							  const DeviceInterface&			vkd,
																							  const VkDevice					device,
																							  const VkPhysicalDevice			physicalDevice,
																							  const VkPipeline					pipeline,
																							  Allocator&						allocator,
																							  de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
																							  const deUint32					group,
																							  const deUint32					groupCount)
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


de::MovePtr<TopLevelAccelerationStructure> RayTracingComplexControlFlowInstance::initTopAccelerationStructure (VkCommandBuffer												cmdBuffer,
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

de::MovePtr<BottomLevelAccelerationStructure> RayTracingComplexControlFlowInstance::initBottomAccelerationStructure (VkCommandBuffer	cmdBuffer,
																													 tcu::UVec2&		startPos)
{
	const DeviceInterface&							vkd				= m_context.getDeviceInterface();
	const VkDevice									device			= m_context.getDevice();
	Allocator&										allocator		= m_context.getDefaultAllocator();
	de::MovePtr<BottomLevelAccelerationStructure>	result			= makeBottomLevelAccelerationStructure();
	const float										z				= (m_data.stage == VK_SHADER_STAGE_MISS_BIT_KHR) ? +1.0f : -1.0f;
	std::vector<tcu::Vec3>							geometryData;

	DE_UNREF(startPos);

	result->setGeometryCount(1);
	geometryData.push_back(tcu::Vec3(0.0f, 0.0f, z));
	geometryData.push_back(tcu::Vec3(1.0f, 1.0f, z));
	result->addGeometry(geometryData, false);
	result->createAndBuild(vkd, device, cmdBuffer, allocator);

	return result;
}

vector<de::SharedPtr<BottomLevelAccelerationStructure> > RayTracingComplexControlFlowInstance::initBottomAccelerationStructures (VkCommandBuffer	cmdBuffer)
{
	tcu::UVec2													startPos;
	vector<de::SharedPtr<BottomLevelAccelerationStructure> >	result;
	de::MovePtr<BottomLevelAccelerationStructure>				bottomLevelAccelerationStructure	= initBottomAccelerationStructure(cmdBuffer, startPos);

	result.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));

	return result;
}

PushConstants RayTracingComplexControlFlowInstance::getPushConstants (void) const
{
	const			deUint32	hitOfs	= 1;
	const			deUint32	miss	= 1;
	PushConstants	result;

	switch (m_data.testType)
	{
		case TEST_TYPE_IF:
		{
			result = { 32 | 8 | 1, 10000, 0x0F, 0xF0, hitOfs, miss };

			break;
		}
		case TEST_TYPE_LOOP:
		{
			result = { 8, 10000, 0x0F, 100000, hitOfs, miss };

			break;
		}
		case TEST_TYPE_SWITCH:
		{
			result = { 3, 10000, 0x07, 100000, hitOfs, miss };

			break;
		}
		case TEST_TYPE_LOOP_DOUBLE_CALL:
		{
			result = { 7, 10000, 0x0F, 0xF0, hitOfs, miss };

			break;
		}
		case TEST_TYPE_LOOP_DOUBLE_CALL_SPARSE:
		{
			result = { 16, 5, 0x0F, 0xF0, hitOfs, miss };

			break;
		}
		case TEST_TYPE_NESTED_LOOP:
		{
			result = { 8, 5, 0x0F, 0x09, hitOfs, miss };

			break;
		}
		case TEST_TYPE_NESTED_LOOP_BEFORE:
		{
			result = { 9, 16, 0x0F, 10, hitOfs, miss };

			break;
		}
		case TEST_TYPE_NESTED_LOOP_AFTER:
		{
			result = { 9, 16, 0x0F, 10, hitOfs, miss };

			break;
		}
		case TEST_TYPE_FUNCTION_CALL:
		{
			result = { 0xFFB, 16, 10, 100000, hitOfs, miss };

			break;
		}
		case TEST_TYPE_NESTED_FUNCTION_CALL:
		{
			result = { 0xFFB, 16, 10, 100000, hitOfs, miss };

			break;
		}

		default:
			TCU_THROW(InternalError, "Unknown testType");
	}

	return result;
}

de::MovePtr<BufferWithMemory> RayTracingComplexControlFlowInstance::runTest (void)
{
	const InstanceInterface&				vki									= m_context.getInstanceInterface();
	const DeviceInterface&					vkd									= m_context.getDeviceInterface();
	const VkDevice							device								= m_context.getDevice();
	const VkPhysicalDevice					physicalDevice						= m_context.getPhysicalDevice();
	const deUint32							queueFamilyIndex					= m_context.getUniversalQueueFamilyIndex();
	const VkQueue							queue								= m_context.getUniversalQueue();
	Allocator&								allocator							= m_context.getDefaultAllocator();
	const VkFormat							format								= VK_FORMAT_R32_UINT;
	const deUint32							pushConstants[]						= { m_pushConstants.a, m_pushConstants.b, m_pushConstants.c, m_pushConstants.d, m_pushConstants.hitOfs, m_pushConstants.miss };
	const deUint32							pushConstantsSize					= sizeof(pushConstants);
	const deUint32							pixelCount							= m_data.width * m_data.height * m_depth;
	const deUint32							shaderGroupHandleSize				= getShaderGroupSize(vki, physicalDevice);

	const Move<VkDescriptorSetLayout>		descriptorSetLayout					= DescriptorSetLayoutBuilder()
																						.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, ALL_RAY_TRACING_STAGES)
																						.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, ALL_RAY_TRACING_STAGES)
																						.build(vkd, device);
	const Move<VkDescriptorPool>			descriptorPool						= DescriptorPoolBuilder()
																						.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
																						.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
																						.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const Move<VkDescriptorSet>				descriptorSet						= makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout);
	const Move<VkPipelineLayout>			pipelineLayout						= makePipelineLayout(vkd, device, descriptorSetLayout.get(), pushConstantsSize);
	const Move<VkCommandPool>				cmdPool								= createCommandPool(vkd, device, 0, queueFamilyIndex);
	const Move<VkCommandBuffer>				cmdBuffer							= allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	de::MovePtr<RayTracingPipeline>			rayTracingPipeline					= de::newMovePtr<RayTracingPipeline>();
	const Move<VkPipeline>					pipeline							= makePipeline(rayTracingPipeline, *pipelineLayout);
	const de::MovePtr<BufferWithMemory>		raygenShaderBindingTable			= createShaderBindingTable(vki, vkd, device, physicalDevice, *pipeline, allocator, rayTracingPipeline, m_raygenShaderGroup, m_raygenShaderGroupCount);
	const de::MovePtr<BufferWithMemory>		missShaderBindingTable				= createShaderBindingTable(vki, vkd, device, physicalDevice, *pipeline, allocator, rayTracingPipeline, m_missShaderGroup, m_missShaderGroupCount);
	const de::MovePtr<BufferWithMemory>		hitShaderBindingTable				= createShaderBindingTable(vki, vkd, device, physicalDevice, *pipeline, allocator, rayTracingPipeline, m_hitShaderGroup, m_hitShaderGroupCount);
	const de::MovePtr<BufferWithMemory>		callableShaderBindingTable			= createShaderBindingTable(vki, vkd, device, physicalDevice, *pipeline, allocator, rayTracingPipeline, m_callableShaderGroup, m_callableShaderGroupCount);

	const VkStridedDeviceAddressRegionKHR	raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegion(vkd, device, getVkBuffer(raygenShaderBindingTable),   shaderGroupHandleSize, m_raygenShaderGroupCount);
	const VkStridedDeviceAddressRegionKHR	missShaderBindingTableRegion		= makeStridedDeviceAddressRegion(vkd, device, getVkBuffer(missShaderBindingTable),     shaderGroupHandleSize, m_missShaderGroupCount);
	const VkStridedDeviceAddressRegionKHR	hitShaderBindingTableRegion			= makeStridedDeviceAddressRegion(vkd, device, getVkBuffer(hitShaderBindingTable),      shaderGroupHandleSize, m_hitShaderGroupCount);
	const VkStridedDeviceAddressRegionKHR	callableShaderBindingTableRegion	= makeStridedDeviceAddressRegion(vkd, device, getVkBuffer(callableShaderBindingTable), shaderGroupHandleSize, m_callableShaderGroupCount);

	const VkImageCreateInfo					imageCreateInfo						= makeImageCreateInfo(m_data.width, m_data.height, m_depth, format);
	const VkImageSubresourceRange			imageSubresourceRange				= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0, 1u);
	const de::MovePtr<ImageWithMemory>		image								= de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
	const Move<VkImageView>					imageView							= makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_3D, format, imageSubresourceRange);

	const VkBufferCreateInfo				bufferCreateInfo					= makeBufferCreateInfo(pixelCount*sizeof(deUint32), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkImageSubresourceLayers			bufferImageSubresourceLayers		= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const VkBufferImageCopy					bufferImageRegion					= makeBufferImageCopy(makeExtent3D(m_data.width, m_data.height, m_depth), bufferImageSubresourceLayers);
	de::MovePtr<BufferWithMemory>			buffer								= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));

	const VkDescriptorImageInfo				descriptorImageInfo					= makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);

	const VkImageMemoryBarrier				preImageBarrier						= makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT,
																					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
																					**image, imageSubresourceRange);
	const VkImageMemoryBarrier				postImageBarrier					= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
																					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
																					**image, imageSubresourceRange);
	const VkMemoryBarrier					preTraceMemoryBarrier				= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
	const VkMemoryBarrier					postTraceMemoryBarrier				= makeMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
	const VkMemoryBarrier					postCopyMemoryBarrier				= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	const VkClearValue						clearValue							= makeClearValueColorU32(DEFAULT_CLEAR_VALUE, 0u, 0u, 255u);

	vector<de::SharedPtr<BottomLevelAccelerationStructure> >	bottomLevelAccelerationStructures;
	de::MovePtr<TopLevelAccelerationStructure>					topLevelAccelerationStructure;

	DE_ASSERT(DE_LENGTH_OF_ARRAY(pushConstants) == PUSH_CONSTANTS_COUNT);

	beginCommandBuffer(vkd, *cmdBuffer, 0u);
	{
		vkd.cmdPushConstants(*cmdBuffer, *pipelineLayout, ALL_RAY_TRACING_STAGES, 0, pushConstantsSize, &m_pushConstants);

		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preImageBarrier);
		vkd.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1, &imageSubresourceRange);
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, ALL_RAY_TRACING_STAGES, &postImageBarrier);

		bottomLevelAccelerationStructures = initBottomAccelerationStructures(*cmdBuffer);
		topLevelAccelerationStructure = initTopAccelerationStructure(*cmdBuffer, bottomLevelAccelerationStructures);

		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, ALL_RAY_TRACING_STAGES, &preTraceMemoryBarrier);

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

		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, ALL_RAY_TRACING_STAGES, VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);

		vkd.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **buffer, 1u, &bufferImageRegion);

		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &postCopyMemoryBarrier);
	}
	endCommandBuffer(vkd, *cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

	invalidateMappedMemoryRange(vkd, device, buffer->getAllocation().getMemory(), buffer->getAllocation().getOffset(), pixelCount * sizeof(deUint32));

	return buffer;
}

std::vector<deUint32> RayTracingComplexControlFlowInstance::getExpectedValues (void) const
{
	const deUint32				plainSize		= m_data.width * m_data.height;
	const deUint32				plain8Ofs		= 8 * plainSize;
	const struct PushConstants&	p				= m_pushConstants;
	const deUint32				pushConstants[]	= { 0, m_pushConstants.a, m_pushConstants.b, m_pushConstants.c, m_pushConstants.d, m_pushConstants.hitOfs, m_pushConstants.miss };
	const deUint32				resultSize		= plainSize * m_depth;
	const bool					fixed			= m_data.testOp == TEST_OP_REPORT_INTERSECTION;
	std::vector<deUint32>		result			(resultSize, DEFAULT_CLEAR_VALUE);
	deUint32					v0;
	deUint32					v1;
	deUint32					v2;
	deUint32					v3;

	switch (m_data.testType)
	{
		case TEST_TYPE_IF:
		{
			for (deUint32 id = 0; id < plainSize; ++id)
			{
				v2 = v3 = p.b;

				if ((p.a & id) != 0)
				{
					v0 = p.c & id;
					v1 = (p.d & id) + 1;

					result[plain8Ofs + id] = v0;
					if (!fixed) v0++;
				}
				else
				{
					v0 = p.d & id;
					v1 = (p.c & id) + 1;

					if (!fixed)
					{
						result[plain8Ofs + id] = v1;
						v1++;
					}
					else
						result[plain8Ofs + id] = v0;
				}

				result[id] = v0 + v1 + v2 + v3;
			}

			break;
		}
		case TEST_TYPE_LOOP:
		{
			for (deUint32 id = 0; id < plainSize; ++id)
			{
				result[id] = 0;

				v1 = v3 = p.b;

				for (deUint32 n = 0; n < p.a; n++)
				{
					v0 = (p.c & id) + n;

					result[((n % 8) + 8) * plainSize + id] = v0;
					if (!fixed) v0++;

					result[id] += v0 + v1 + v3;
				}
			}

			break;
		}
		case TEST_TYPE_SWITCH:
		{
			for (deUint32 id = 0; id < plainSize; ++id)
			{
				switch (p.a & id)
				{
					case 0: { v1 = v2 = v3 = p.b; v0 = p.c & id; break; }
					case 1: { v0 = v2 = v3 = p.b; v1 = p.c & id; break; }
					case 2: { v0 = v1 = v3 = p.b; v2 = p.c & id; break; }
					case 3: { v0 = v1 = v2 = p.b; v3 = p.c & id; break; }
					default: { v0 = v1 = v2 = v3 = 0; break; }
				};

				if (!fixed)
					result[plain8Ofs + id] = p.c & id;
				else
					result[plain8Ofs + id] = v0;

				result[id] = v0 + v1 + v2 + v3;

				if (!fixed) result[id]++;
			}

			break;
		}
		case TEST_TYPE_LOOP_DOUBLE_CALL:
		{
			for (deUint32 id = 0; id < plainSize; ++id)
			{
				result[id] = 0;

				v3 = p.b;

				for (deUint32 x = 0; x < p.a; x++)
				{
					v0 = (p.c & id) + x;
					v1 = (p.d & id) + x + 1;

					result[(((2 * x + 0) % 8) + 8) * plainSize + id] = v0;
					if (!fixed) v0++;

					if (!fixed)
					{
						result[(((2 * x + 1) % 8) + 8) * plainSize + id] = v1;
						v1++;
					}

					result[id] += v0 + v1 + v3;
				}
			}

			break;
		}
		case TEST_TYPE_LOOP_DOUBLE_CALL_SPARSE:
		{
			for (deUint32 id = 0; id < plainSize; ++id)
			{
				result[id] = 0;

				v3 = p.a + p.b;

				for (deUint32 x = 0; x < p.a; x++)
				{
					if ((x & p.b) != 0)
					{
						v0 = (p.c & id) + x;
						v1 = (p.d & id) + x + 1;

						result[(((2 * x + 0) % 8) + 8) * plainSize + id] = v0;
						if (!fixed) v0++;

						if (!fixed)
						{
							result[(((2 * x + 1) % 8) + 8) * plainSize + id] = v1;
							v1++;
						}

						result[id] += v0 + v1 + v3;
					}
				}
			}

			break;
		}
		case TEST_TYPE_NESTED_LOOP:
		{
			for (deUint32 id = 0; id < plainSize; ++id)
			{
				result[id] = 0;

				v1 = v3 = p.b;

				for (deUint32 y = 0; y < p.a; y++)
				for (deUint32 x = 0; x < p.a; x++)
				{
					const deUint32 n = x + y * p.a;

					if ((n & p.d) != 0)
					{
						v0 = (p.c & id) + n;

						result[((n % 8) + 8) * plainSize + id] = v0;
						if (!fixed) v0++;

						result[id] += v0 + v1 + v3;
					}
				}
			}

			break;
		}
		case TEST_TYPE_NESTED_LOOP_BEFORE:
		{
			for (deUint32 id = 0; id < plainSize; ++id)
			{
				result[id] = 0;

				for (deUint32 y = 0; y < p.d; y++)
				for (deUint32 x = 0; x < p.d; x++)
				{
					if (((x + y * p.a) & p.b) != 0)
						result[id] += (x + y);
				}

				v1 = v3 = p.a;

				for (deUint32 x = 0; x < p.b; x++)
				{
					if ((x & p.a) != 0)
					{
						v0 = p.c & id;

						result[((x % 8) + 8) * plainSize + id] = v0;
						if (!fixed) v0++;

						result[id] += v0 + v1 + v3;
					}
				}
			}

			break;
		}
		case TEST_TYPE_NESTED_LOOP_AFTER:
		{
			for (deUint32 id = 0; id < plainSize; ++id)
			{
				result[id] = 0;

				v1 = v3 = p.a;

				for (deUint32 x = 0; x < p.b; x++)
				{
					if ((x & p.a) != 0)
					{
						v0 = p.c & id;

						result[((x % 8) + 8) * plainSize + id] = v0;
						if (!fixed) v0++;

						result[id] += v0 + v1 + v3;
					}
				}

				for (deUint32 y = 0; y < p.d; y++)
				for (deUint32 x = 0; x < p.d; x++)
				{
					if (((x + y * p.a) & p.b) != 0)
						result[id] += (x + y);
				}
			}

			break;
		}
		case TEST_TYPE_FUNCTION_CALL:
		{
			deUint32 a[42];

			for (deUint32 id = 0; id < plainSize; ++id)
			{
				deUint32 r = 0;
				deUint32 i;

				v0 = p.a & id;
				v1 = v3 = p.d;

				for (i = 0; i < DE_LENGTH_OF_ARRAY(a); i++)
					a[i] = p.c * i;

				result[plain8Ofs + id] = v0;
				if (!fixed) v0++;

				for (i = 0; i < DE_LENGTH_OF_ARRAY(a); i++)
					r += a[i];

				result[id] = (r + i) + v0 + v1 + v3;
			}

			break;
		}
		case TEST_TYPE_NESTED_FUNCTION_CALL:
		{
			deUint32 a[14];
			deUint32 b[256];

			for (deUint32 id = 0; id < plainSize; ++id)
			{
				deUint32 r = 0;
				deUint32 i;
				deUint32 t = 0;
				deUint32 j;

				v0 = p.a & id;
				v3 = p.d;

				for (j = 0; j < DE_LENGTH_OF_ARRAY(b); j++)
					b[j] = p.c * j;

				v1 = p.b;

				for (i = 0; i < DE_LENGTH_OF_ARRAY(a); i++)
					a[i] = p.c * i;

				result[plain8Ofs + id] = v0;
				if (!fixed) v0++;

				for (i = 0; i < DE_LENGTH_OF_ARRAY(a); i++)
					r += a[i];

				for (j = 0; j < DE_LENGTH_OF_ARRAY(b); j++)
					t += b[j];

				result[id] = (r + i) + (t + j) + v0 + v1 + v3;
			}

			break;
		}

		default:
			TCU_THROW(InternalError, "Unknown testType");
	}

	{
		const deUint32	startOfs	= 7 * plainSize;

		for (deUint32 n = 0; n < plainSize; ++n)
			result[startOfs + n] = n;
	}

	for (deUint32 z = 1; z < DE_LENGTH_OF_ARRAY(pushConstants); ++z)
	{
		const deUint32	startOfs		= z * plainSize;
		const deUint32	pushConstant	= pushConstants[z];

		for (deUint32 n = 0; n < plainSize; ++n)
			result[startOfs + n] = pushConstant;
	}

	return result;
}

tcu::TestStatus RayTracingComplexControlFlowInstance::iterate (void)
{
	const de::MovePtr<BufferWithMemory>	buffer		= runTest();
	const deUint32*						bufferPtr	= (deUint32*)buffer->getAllocation().getHostPtr();
	const vector<deUint32>				expected	= getExpectedValues();
	tcu::TestLog&						log			= m_context.getTestContext().getLog();
	deUint32							failures	= 0;
	deUint32							pos			= 0;

	for (deUint32 z = 0; z < m_depth; ++z)
	for (deUint32 y = 0; y < m_data.height; ++y)
	for (deUint32 x = 0; x < m_data.width; ++x)
	{
		if (bufferPtr[pos] != expected[pos])
			failures++;

		++pos;
	}

	if (failures != 0)
	{
		deUint32			pos0	= 0;
		deUint32			pos1	= 0;
		std::stringstream	css;

		for (deUint32 z = 0; z < m_depth; ++z)
		{
			css << "z=" << z << std::endl;

			for (deUint32 y = 0; y < m_data.height; ++y)
			{
				for (deUint32 x = 0; x < m_data.width; ++x)
					css << std::setw(6) << bufferPtr[pos0++] << ' ';

				css << "    ";

				for (deUint32 x = 0; x < m_data.width; ++x)
					css << std::setw(6) << expected[pos1++] << ' ';

				css << std::endl;
			}

			css << std::endl;
		}

		log << tcu::TestLog::Message << css.str() << tcu::TestLog::EndMessage;
	}

	if (failures == 0)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("failures=" + de::toString(failures));
}

class ComplexControlFlowTestCase : public TestCase
{
	public:
										ComplexControlFlowTestCase	(tcu::TestContext& context, const char* name, const char* desc, const CaseDef data);
										~ComplexControlFlowTestCase	(void);

	virtual	void						initPrograms				(SourceCollections& programCollection) const;
	virtual TestInstance*				createInstance				(Context& context) const;
	virtual void						checkSupport				(Context& context) const;

private:
	static inline const std::string		getIntersectionPassthrough	(void);
	static inline const std::string		getMissPassthrough			(void);
	static inline const std::string		getHitPassthrough			(void);

	CaseDef								m_data;
};

ComplexControlFlowTestCase::ComplexControlFlowTestCase (tcu::TestContext& context, const char* name, const char* desc, const CaseDef data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

ComplexControlFlowTestCase::~ComplexControlFlowTestCase	(void)
{
}

void ComplexControlFlowTestCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");

	const VkPhysicalDeviceAccelerationStructureFeaturesKHR&	accelerationStructureFeaturesKHR = context.getAccelerationStructureFeatures();

	if (accelerationStructureFeaturesKHR.accelerationStructure == DE_FALSE)
		TCU_THROW(TestError, "VK_KHR_ray_tracing_pipeline requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

	context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

	const VkPhysicalDeviceRayTracingPipelineFeaturesKHR&	rayTracingPipelineFeaturesKHR = context.getRayTracingPipelineFeatures();

	if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

	const VkPhysicalDeviceRayTracingPipelinePropertiesKHR&	rayTracingPipelinePropertiesKHR = context.getRayTracingPipelineProperties();

	if (m_data.testOp == TEST_OP_TRACE_RAY && m_data.stage != VK_SHADER_STAGE_RAYGEN_BIT_KHR)
	{
		if (rayTracingPipelinePropertiesKHR.maxRayRecursionDepth < 2)
			TCU_THROW(NotSupportedError, "rayTracingPipelinePropertiesKHR.maxRayRecursionDepth is smaller than required");
	}
}


const std::string ComplexControlFlowTestCase::getIntersectionPassthrough (void)
{
	const std::string intersectionPassthrough =
		"#version 460 core\n"
		"#extension GL_EXT_nonuniform_qualifier : enable\n"
		"#extension GL_EXT_ray_tracing : require\n"
		"hitAttributeEXT vec3 hitAttribute;\n"
		"\n"
		"void main()\n"
		"{\n"
		"  reportIntersectionEXT(0.95f, 0u);\n"
		"}\n";

	return intersectionPassthrough;
}

const std::string ComplexControlFlowTestCase::getMissPassthrough (void)
{
	const std::string missPassthrough =
		"#version 460 core\n"
		"#extension GL_EXT_nonuniform_qualifier : enable\n"
		"#extension GL_EXT_ray_tracing : require\n"
		"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
		"\n"
		"void main()\n"
		"{\n"
		"}\n";

	return missPassthrough;
}

const std::string ComplexControlFlowTestCase::getHitPassthrough (void)
{
	const std::string hitPassthrough =
		"#version 460 core\n"
		"#extension GL_EXT_nonuniform_qualifier : enable\n"
		"#extension GL_EXT_ray_tracing : require\n"
		"hitAttributeEXT vec3 attribs;\n"
		"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
		"\n"
		"void main()\n"
		"{\n"
		"}\n";

	return hitPassthrough;
}

void ComplexControlFlowTestCase::initPrograms (SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions	buildOptions			(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	const std::string				calleeMainPart			=
		"  uint z = (inValue.x % 8) + 8;\n"
		"  uint v = inValue.y;\n"
		"  uint n = gl_LaunchIDEXT.x + gl_LaunchSizeEXT.x * gl_LaunchIDEXT.y;\n"
		"  imageStore(resultImage, ivec3(gl_LaunchIDEXT.x, gl_LaunchIDEXT.y, z), uvec4(v, 0, 0, 1));\n"
		"  imageStore(resultImage, ivec3(gl_LaunchIDEXT.x, gl_LaunchIDEXT.y, 7), uvec4(n, 0, 0, 1));\n";
	const std::string				idTemplate				= "$";
	const std::string				shaderCallInstruction	= (m_data.testOp == TEST_OP_EXECUTE_CALLABLE)    ? "executeCallableEXT(0, " + idTemplate + ")"
															: (m_data.testOp == TEST_OP_TRACE_RAY)           ? "traceRayEXT(as, 0, 0xFF, p.hitOfs, 0, p.miss, vec3((gl_LaunchIDEXT.x) + vec3(0.5f)) / vec3(gl_LaunchSizeEXT), 1.0f, vec3(0.0f, 0.0f, 1.0f), 100.0f, " + idTemplate + ")"
															: (m_data.testOp == TEST_OP_REPORT_INTERSECTION) ? "reportIntersectionEXT(1.0f, 0u)"
															: "TEST_OP_NOT_IMPLEMENTED_FAILURE";
	std::string						declsPreMain			=
		"#version 460 core\n"
		"#extension GL_EXT_nonuniform_qualifier : enable\n"
		"#extension GL_EXT_ray_tracing : require\n"
		"\n"
		"layout(set = 0, binding = 0, r32ui) uniform uimage3D resultImage;\n"
		"layout(set = 0, binding = 1) uniform accelerationStructureEXT as;\n"
		"\n"
		"layout(push_constant) uniform TestParams\n"
		"{\n"
		"    uint a;\n"
		"    uint b;\n"
		"    uint c;\n"
		"    uint d;\n"
		"    uint hitOfs;\n"
		"    uint miss;\n"
		"} p;\n";
	std::string						declsInMainBeforeOp		=
		"  uint result = 0;\n"
		"  uint id = uint(gl_LaunchIDEXT.x + gl_LaunchSizeEXT.x * gl_LaunchIDEXT.y);\n";
	std::string						declsInMainAfterOp		=
		"  imageStore(resultImage, ivec3(gl_LaunchIDEXT.x, gl_LaunchIDEXT.y, 0), uvec4(result, 0, 0, 1));\n"
		"  imageStore(resultImage, ivec3(gl_LaunchIDEXT.x, gl_LaunchIDEXT.y, 1), uvec4(p.a, 0, 0, 1));\n"
		"  imageStore(resultImage, ivec3(gl_LaunchIDEXT.x, gl_LaunchIDEXT.y, 2), uvec4(p.b, 0, 0, 1));\n"
		"  imageStore(resultImage, ivec3(gl_LaunchIDEXT.x, gl_LaunchIDEXT.y, 3), uvec4(p.c, 0, 0, 1));\n"
		"  imageStore(resultImage, ivec3(gl_LaunchIDEXT.x, gl_LaunchIDEXT.y, 4), uvec4(p.d, 0, 0, 1));\n"
		"  imageStore(resultImage, ivec3(gl_LaunchIDEXT.x, gl_LaunchIDEXT.y, 5), uvec4(p.hitOfs, 0, 0, 1));\n"
		"  imageStore(resultImage, ivec3(gl_LaunchIDEXT.x, gl_LaunchIDEXT.y, 6), uvec4(p.miss, 0, 0, 1));\n";
	std::string						opInMain				= "";
	std::string						opPreMain				= "";

	DE_ASSERT(!declsPreMain.empty() && PUSH_CONSTANTS_COUNT == 6);

	switch (m_data.testType)
	{
		case TEST_TYPE_IF:
		{
			opInMain =
				"  v2 = v3 = uvec2(0, p.b);\n"
				"\n"
				"  if ((p.a & id) != 0)\n"
				"      { v0 = uvec2(0, p.c & id); v1 = uvec2(0, (p.d & id) + 1);" + replace(shaderCallInstruction, idTemplate, "0") + "; }\n"
				"  else\n"
				"      { v0 = uvec2(0, p.d & id); v1 = uvec2(0, (p.c & id) + 1);" + replace(shaderCallInstruction, idTemplate, "1") + "; }\n"
				"\n"
				"  result = v0.y + v1.y + v2.y + v3.y;\n";

			break;
		}
		case TEST_TYPE_LOOP:
		{
			opInMain =
				"  v1 = v3 = uvec2(0, p.b);\n"
				"\n"
				"  for (uint x = 0; x < p.a; x++)\n"
				"  {\n"
				"    v0 = uvec2(x, (p.c & id) + x);\n"
				"    " + replace(shaderCallInstruction, idTemplate, "0") + ";\n"
				"    result += v0.y + v1.y + v3.y;\n"
				"  }\n";

			break;
		}
		case TEST_TYPE_SWITCH:
		{
			opInMain =
				"  switch (p.a & id)\n"
				"  {\n"
				"    case 0: { v1 = v2 = v3 = uvec2(0, p.b); v0 = uvec2(0, p.c & id); " + replace(shaderCallInstruction, idTemplate, "0") + "; break; }\n"
				"    case 1: { v0 = v2 = v3 = uvec2(0, p.b); v1 = uvec2(0, p.c & id); " + replace(shaderCallInstruction, idTemplate, "1") + "; break; }\n"
				"    case 2: { v0 = v1 = v3 = uvec2(0, p.b); v2 = uvec2(0, p.c & id); " + replace(shaderCallInstruction, idTemplate, "2") + "; break; }\n"
				"    case 3: { v0 = v1 = v2 = uvec2(0, p.b); v3 = uvec2(0, p.c & id); " + replace(shaderCallInstruction, idTemplate, "3") + "; break; }\n"
				"    default: break;\n"
				"  }\n"
				"\n"
				"  result = v0.y + v1.y + v2.y + v3.y;\n";

			break;
		}
		case TEST_TYPE_LOOP_DOUBLE_CALL:
		{
			opInMain =
				"  v3 = uvec2(0, p.b);\n"
				"  for (uint x = 0; x < p.a; x++)\n"
				"  {\n"
				"    v0 = uvec2(2 * x + 0, (p.c & id) + x);\n"
				"    v1 = uvec2(2 * x + 1, (p.d & id) + x + 1);\n"
				"    " + replace(shaderCallInstruction, idTemplate, "0") + ";\n"
				"    " + replace(shaderCallInstruction, idTemplate, "1") + ";\n"
				"    result += v0.y + v1.y + v3.y;\n"
				"  }\n";

			break;
		}
		case TEST_TYPE_LOOP_DOUBLE_CALL_SPARSE:
		{
			opInMain =
				"  v3 = uvec2(0, p.a + p.b);\n"
				"  for (uint x = 0; x < p.a; x++)\n"
				"    if ((x & p.b) != 0)\n"
				"    {\n"
				"      v0 = uvec2(2 * x + 0, (p.c & id) + x + 0);\n"
				"      v1 = uvec2(2 * x + 1, (p.d & id) + x + 1);\n"
				"      " + replace(shaderCallInstruction, idTemplate, "0") + ";\n"
				"      " + replace(shaderCallInstruction, idTemplate, "1") + ";\n"
				"      result += v0.y + v1.y + v3.y;\n"
				"    }\n"
				"\n";

			break;
		}
		case TEST_TYPE_NESTED_LOOP:
		{
			opInMain =
				"  v1 = v3 = uvec2(0, p.b);\n"
				"  for (uint y = 0; y < p.a; y++)\n"
				"  for (uint x = 0; x < p.a; x++)\n"
				"  {\n"
				"    uint n = x + y * p.a;\n"
				"    if ((n & p.d) != 0)\n"
				"    {\n"
				"      v0 = uvec2(n, (p.c & id) + (x + y * p.a));\n"
				"      "+ replace(shaderCallInstruction, idTemplate, "0") + ";\n"
				"      result += v0.y + v1.y + v3.y;\n"
				"    }\n"
				"  }\n"
				"\n";

			break;
		}
		case TEST_TYPE_NESTED_LOOP_BEFORE:
		{
			opInMain =
				"  for (uint y = 0; y < p.d; y++)\n"
				"  for (uint x = 0; x < p.d; x++)\n"
				"    if (((x + y * p.a) & p.b) != 0)\n"
				"      result += (x + y);\n"
				"\n"
				"  v1 = v3 = uvec2(0, p.a);\n"
				"\n"
				"  for (uint x = 0; x < p.b; x++)\n"
				"    if ((x & p.a) != 0)\n"
				"    {\n"
				"      v0 = uvec2(x, p.c & id);\n"
				"      " + replace(shaderCallInstruction, idTemplate, "0") + ";\n"
				"      result += v0.y + v1.y + v3.y;\n"
				"    }\n";

			break;
		}
		case TEST_TYPE_NESTED_LOOP_AFTER:
		{
			opInMain =
				"  v1 = v3 = uvec2(0, p.a); \n"
				"  for (uint x = 0; x < p.b; x++)\n"
				"    if ((x & p.a) != 0)\n"
				"    {\n"
				"      v0 = uvec2(x, p.c & id);\n"
				"      " + replace(shaderCallInstruction, idTemplate, "0") + ";\n"
				"      result += v0.y + v1.y + v3.y;\n"
				"    }\n"
				"\n"
				"  for (uint y = 0; y < p.d; y++)\n"
				"  for (uint x = 0; x < p.d; x++)\n"
				"    if (((x + y * p.a) & p.b) != 0)\n"
				"      result += x + y;\n";

			break;
		}
		case TEST_TYPE_FUNCTION_CALL:
		{
			opPreMain =
				"uint f1(void)\n"
				"{\n"
				"  uint i, r = 0;\n"
				"  uint a[42];\n"
				"\n"
				"  for (i = 0; i < a.length(); i++) a[i] = p.c * i;\n"
				"\n"
				"  " + replace(shaderCallInstruction, idTemplate, "0") + ";\n"
				"\n"
				"  for (i = 0; i < a.length(); i++) r += a[i];\n"
				"\n"
				"  return r + i;\n"
				"}\n";
			opInMain =
				"  v0 = uvec2(0, p.a & id); v1 = v3 = uvec2(0, p.d);\n"
				"  result = f1() + v0.y + v1.y + v3.y;\n";

			break;
		}
		case TEST_TYPE_NESTED_FUNCTION_CALL:
		{
			opPreMain =
				"uint f0(void)\n"
				"{\n"
				"  uint i, r = 0;\n"
				"  uint a[14];\n"
				"\n"
				"  for (i = 0; i < a.length(); i++) a[i] = p.c * i;\n"
				"\n"
				"  " + replace(shaderCallInstruction, idTemplate, "0") + ";\n"
				"\n"
				"  for (i = 0; i < a.length(); i++) r += a[i];\n"
				"\n"
				"  return r + i;\n"
				"}\n"
				"\n"
				"uint f1(void)\n"
				"{\n"
				"  uint j, t = 0;\n"
				"  uint b[256];\n"
				"\n"
				"  for (j = 0; j < b.length(); j++) b[j] = p.c * j;\n"
				"\n"
				"  v1 = uvec2(0, p.b);\n"
				"\n"
				"  t += f0();\n"
				"\n"
				"  for (j = 0; j < b.length(); j++) t += b[j];\n"
				"\n"
				"  return t + j;\n"
				"}\n";
			opInMain =
				"  v0 = uvec2(0, p.a & id); v3 = uvec2(0, p.d);\n"
				"  result = f1() + v0.y + v1.y + v3.y;\n";

			break;
		}

		default:
			TCU_THROW(InternalError, "Unknown testType");
	}

	if (m_data.testOp == TEST_OP_EXECUTE_CALLABLE)
	{
		const std::string	calleeShader			=
			"#version 460 core\n"
			"#extension GL_EXT_nonuniform_qualifier : enable\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"\n"
			"layout(set = 0, binding = 0, r32ui) uniform uimage3D resultImage;\n"
			"layout(location = 0) callableDataInEXT uvec2 inValue;\n"
			"\n"
			"void main()\n"
			"{\n"
			+ calleeMainPart +
			"  inValue.y++;\n"
			"}\n";

		declsPreMain +=
			"layout(location = 0) callableDataEXT uvec2 v0;\n"
			"layout(location = 1) callableDataEXT uvec2 v1;\n"
			"layout(location = 2) callableDataEXT uvec2 v2;\n"
			"layout(location = 3) callableDataEXT uvec2 v3;\n"
			"\n";

		switch (m_data.stage)
		{
			case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
			{
				std::stringstream css;
				css << declsPreMain
					<< opPreMain
					<< "\n"
					<< "void main()\n"
					<< "{\n"
					<< declsInMainBeforeOp
					<< opInMain // executeCallableEXT
					<< declsInMainAfterOp
					<< "}\n";

				programCollection.glslSources.add("rgen") << glu::RaygenSource(css.str()) << buildOptions;
				programCollection.glslSources.add("cal0") << glu::CallableSource(calleeShader) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(getCommonRayGenerationShader()) << buildOptions;

				std::stringstream css;
				css << declsPreMain
					<< "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
					<< "hitAttributeEXT vec3 attribs;\n"
					<< "\n"
					<< opPreMain
					<< "\n"
					<< "void main()\n"
					<< "{\n"
					<< declsInMainBeforeOp
					<< opInMain // executeCallableEXT
					<< declsInMainAfterOp
					<< "}\n";

				programCollection.glslSources.add("chit") << glu::ClosestHitSource(css.str()) << buildOptions;
				programCollection.glslSources.add("cal0") << glu::CallableSource(calleeShader) << buildOptions;

				programCollection.glslSources.add("ahit") << glu::AnyHitSource(getHitPassthrough()) << buildOptions;
				programCollection.glslSources.add("miss") << glu::MissSource(getMissPassthrough()) << buildOptions;
				programCollection.glslSources.add("sect") << glu::IntersectionSource(getIntersectionPassthrough()) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_MISS_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(getCommonRayGenerationShader()) << buildOptions;

				std::stringstream css;
				css << declsPreMain
					<< opPreMain
					<< "\n"
					<< "void main()\n"
					<< "{\n"
					<< declsInMainBeforeOp
					<< opInMain // executeCallableEXT
					<< declsInMainAfterOp
					<< "}\n";

				programCollection.glslSources.add("miss") << glu::MissSource(css.str()) << buildOptions;
				programCollection.glslSources.add("cal0") << glu::CallableSource(calleeShader) << buildOptions;

				programCollection.glslSources.add("ahit") << glu::AnyHitSource(getHitPassthrough()) << buildOptions;
				programCollection.glslSources.add("chit") << glu::ClosestHitSource(getHitPassthrough()) << buildOptions;
				programCollection.glslSources.add("sect") << glu::IntersectionSource(getIntersectionPassthrough()) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
			{
				{
					std::stringstream css;
					css << "#version 460 core\n"
						<< "#extension GL_EXT_nonuniform_qualifier : enable\n"
						<< "#extension GL_EXT_ray_tracing : require\n"
						<< "\n"
						<< "layout(location = 4) callableDataEXT float dummy;\n"
						<< "layout(set = 0, binding = 0, r32ui) uniform uimage3D resultImage;\n"
						<< "\n"
						<< "void main()\n"
						<< "{\n"
						<< "  executeCallableEXT(1, 4);\n"
						<< "}\n";

					programCollection.glslSources.add("rgen") << glu::RaygenSource(css.str()) << buildOptions;
				}

				{
					std::stringstream css;
					css << declsPreMain
						<< "layout(location = 4) callableDataInEXT float dummyIn;\n"
						<< opPreMain
						<< "\n"
						<< "void main()\n"
						<< "{\n"
						<< declsInMainBeforeOp
						<< opInMain // executeCallableEXT
						<< declsInMainAfterOp
						<< "}\n";

					programCollection.glslSources.add("call") << glu::CallableSource(css.str()) << buildOptions;
				}

				programCollection.glslSources.add("cal0") << glu::CallableSource(calleeShader) << buildOptions;

				break;
			}

			default:
				TCU_THROW(InternalError, "Unknown stage");
		}
	}
	else if (m_data.testOp == TEST_OP_TRACE_RAY)
	{
		const std::string	missShader	=
			"#version 460 core\n"
			"#extension GL_EXT_nonuniform_qualifier : enable\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"\n"
			"layout(set = 0, binding = 0, r32ui) uniform uimage3D resultImage;\n"
			"layout(location = 0) rayPayloadInEXT uvec2 inValue;\n"
			"\n"
			"void main()\n"
			"{\n"
			+ calleeMainPart +
			"  inValue.y++;\n"
			"}\n";

		declsPreMain +=
			"layout(location = 0) rayPayloadEXT uvec2 v0;\n"
			"layout(location = 1) rayPayloadEXT uvec2 v1;\n"
			"layout(location = 2) rayPayloadEXT uvec2 v2;\n"
			"layout(location = 3) rayPayloadEXT uvec2 v3;\n";

		switch (m_data.stage)
		{
			case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
			{
				std::stringstream css;
				css << declsPreMain
					<< opPreMain
					<< "\n"
					<< "void main()\n"
					<< "{\n"
					<< declsInMainBeforeOp
					<< opInMain // traceRayEXT
					<< declsInMainAfterOp
					<< "}\n";

				programCollection.glslSources.add("rgen") << glu::RaygenSource(css.str()) << buildOptions;

				programCollection.glslSources.add("miss") << glu::MissSource(getMissPassthrough()) << buildOptions;
				programCollection.glslSources.add("ahit") << glu::AnyHitSource(getHitPassthrough()) << buildOptions;
				programCollection.glslSources.add("chit") << glu::ClosestHitSource(getHitPassthrough()) << buildOptions;
				programCollection.glslSources.add("sect") << glu::IntersectionSource(getIntersectionPassthrough()) << buildOptions;

				programCollection.glslSources.add("miss2") << glu::MissSource(missShader) << buildOptions;
				programCollection.glslSources.add("ahit2") << glu::AnyHitSource(getHitPassthrough()) << buildOptions;
				programCollection.glslSources.add("chit2") << glu::ClosestHitSource(getHitPassthrough()) << buildOptions;
				programCollection.glslSources.add("sect2") << glu::IntersectionSource(getIntersectionPassthrough()) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(getCommonRayGenerationShader()) << buildOptions;

				std::stringstream css;
				css << declsPreMain
					<< opPreMain
					<< "\n"
					<< "void main()\n"
					<< "{\n"
					<< declsInMainBeforeOp
					<< opInMain // traceRayEXT
					<< declsInMainAfterOp
					<< "}\n";

				programCollection.glslSources.add("chit") << glu::ClosestHitSource(css.str()) << buildOptions;

				programCollection.glslSources.add("miss") << glu::MissSource(getMissPassthrough()) << buildOptions;
				programCollection.glslSources.add("ahit") << glu::AnyHitSource(getHitPassthrough()) << buildOptions;
				programCollection.glslSources.add("sect") << glu::IntersectionSource(getIntersectionPassthrough()) << buildOptions;

				programCollection.glslSources.add("miss2") << glu::MissSource(missShader) << buildOptions;
				programCollection.glslSources.add("ahit2") << glu::AnyHitSource(getHitPassthrough()) << buildOptions;
				programCollection.glslSources.add("chit2") << glu::ClosestHitSource(getHitPassthrough()) << buildOptions;
				programCollection.glslSources.add("sect2") << glu::IntersectionSource(getIntersectionPassthrough()) << buildOptions;

				break;
			}

			case VK_SHADER_STAGE_MISS_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(getCommonRayGenerationShader()) << buildOptions;

				std::stringstream css;
				css << declsPreMain
					<< opPreMain
					<< "\n"
					<< "void main()\n"
					<< "{\n"
					<< declsInMainBeforeOp
					<< opInMain // traceRayEXT
					<< declsInMainAfterOp
					<< "}\n";

				programCollection.glslSources.add("miss") << glu::MissSource(css.str()) << buildOptions;

				programCollection.glslSources.add("ahit") << glu::AnyHitSource(getHitPassthrough()) << buildOptions;
				programCollection.glslSources.add("chit") << glu::ClosestHitSource(getHitPassthrough()) << buildOptions;
				programCollection.glslSources.add("sect") << glu::IntersectionSource(getIntersectionPassthrough()) << buildOptions;

				programCollection.glslSources.add("miss2") << glu::MissSource(missShader) << buildOptions;
				programCollection.glslSources.add("ahit2") << glu::AnyHitSource(getHitPassthrough()) << buildOptions;
				programCollection.glslSources.add("chit2") << glu::ClosestHitSource(getHitPassthrough()) << buildOptions;
				programCollection.glslSources.add("sect2") << glu::IntersectionSource(getIntersectionPassthrough()) << buildOptions;

				break;
			}

			default:
				TCU_THROW(InternalError, "Unknown stage");
		}
	}
	else if (m_data.testOp == TEST_OP_REPORT_INTERSECTION)
	{
		const std::string	anyHitShader		=
			"#version 460 core\n"
			"#extension GL_EXT_nonuniform_qualifier : enable\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"\n"
			"layout(set = 0, binding = 0, r32ui) uniform uimage3D resultImage;\n"
			"hitAttributeEXT block { uvec2 inValue; };\n"
			"\n"
			"void main()\n"
			"{\n"
			+ calleeMainPart +
			"}\n";

		declsPreMain +=
			"hitAttributeEXT block { uvec2 v0; };\n"
			"uvec2 v1;\n"
			"uvec2 v2;\n"
			"uvec2 v3;\n";

		switch (m_data.stage)
		{
			case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
			{
				programCollection.glslSources.add("rgen") << glu::RaygenSource(getCommonRayGenerationShader()) << buildOptions;

				std::stringstream css;
				css << declsPreMain
					<< opPreMain
					<< "\n"
					<< "void main()\n"
					<< "{\n"
					<< declsInMainBeforeOp
					<< opInMain // reportIntersectionEXT
					<< declsInMainAfterOp
					<< "}\n";

				programCollection.glslSources.add("sect") << glu::IntersectionSource(css.str()) << buildOptions;
				programCollection.glslSources.add("ahit") << glu::AnyHitSource(anyHitShader) << buildOptions;

				programCollection.glslSources.add("chit") << glu::ClosestHitSource(getHitPassthrough()) << buildOptions;
				programCollection.glslSources.add("miss") << glu::MissSource(getMissPassthrough()) << buildOptions;

				break;
			}

			default:
				TCU_THROW(InternalError, "Unknown stage");
		}
	}
	else
	{
		TCU_THROW(InternalError, "Unknown operation");
	}
}

TestInstance* ComplexControlFlowTestCase::createInstance (Context& context) const
{
	return new RayTracingComplexControlFlowInstance(context, m_data);
}

}	// anonymous

tcu::TestCaseGroup*	createComplexControlFlowTests (tcu::TestContext& testCtx)
{
	const VkShaderStageFlagBits	R	= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	const VkShaderStageFlagBits	A	= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	const VkShaderStageFlagBits	C	= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	const VkShaderStageFlagBits	M	= VK_SHADER_STAGE_MISS_BIT_KHR;
	const VkShaderStageFlagBits	I	= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
	const VkShaderStageFlagBits	L	= VK_SHADER_STAGE_CALLABLE_BIT_KHR;

	DE_UNREF(A);

	static const struct
	{
		const char*				name;
		VkShaderStageFlagBits	stage;
	}
	testStages[]
	{
		{ "rgen", VK_SHADER_STAGE_RAYGEN_BIT_KHR		},
		{ "chit", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR	},
		{ "ahit", VK_SHADER_STAGE_ANY_HIT_BIT_KHR		},
		{ "sect", VK_SHADER_STAGE_INTERSECTION_BIT_KHR	},
		{ "miss", VK_SHADER_STAGE_MISS_BIT_KHR			},
		{ "call", VK_SHADER_STAGE_CALLABLE_BIT_KHR		},
	};
	static const struct
	{
		const char*			name;
		TestOp				op;
		VkShaderStageFlags	applicableInStages;
	}
	testOps[]
	{
		{ "execute_callable",		TEST_OP_EXECUTE_CALLABLE,		R |    C | M     | L },
		{ "trace_ray",				TEST_OP_TRACE_RAY,				R |    C | M         },
		{ "report_intersection",	TEST_OP_REPORT_INTERSECTION,	               I     },
	};
	static const struct
	{
		const char*	name;
		TestType	testType;
	}
	testTypes[]
	{
		{ "if",							TEST_TYPE_IF						},
		{ "loop",						TEST_TYPE_LOOP						},
		{ "switch",						TEST_TYPE_SWITCH					},
		{ "loop_double_call",			TEST_TYPE_LOOP_DOUBLE_CALL			},
		{ "loop_double_call_sparse",	TEST_TYPE_LOOP_DOUBLE_CALL_SPARSE	},
		{ "nested_loop",				TEST_TYPE_NESTED_LOOP				},
		{ "nested_loop_loop_before",	TEST_TYPE_NESTED_LOOP_BEFORE		},
		{ "nested_loop_loop_after",		TEST_TYPE_NESTED_LOOP_AFTER			},
		{ "function_call",				TEST_TYPE_FUNCTION_CALL				},
		{ "nested_function_call",		TEST_TYPE_NESTED_FUNCTION_CALL		},
	};

	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "complexcontrolflow", "Ray tracing complex control flow tests"));

	for (size_t testTypeNdx = 0; testTypeNdx < DE_LENGTH_OF_ARRAY(testTypes); ++testTypeNdx)
	{
		const TestType					testType		= testTypes[testTypeNdx].testType;
		de::MovePtr<tcu::TestCaseGroup> testTypeGroup	(new tcu::TestCaseGroup(testCtx, testTypes[testTypeNdx].name, ""));

		for (size_t testOpNdx = 0; testOpNdx < DE_LENGTH_OF_ARRAY(testOps); ++testOpNdx)
		{
			const TestOp					testOp		= testOps[testOpNdx].op;
			de::MovePtr<tcu::TestCaseGroup> testOpGroup	(new tcu::TestCaseGroup(testCtx, testOps[testOpNdx].name, ""));

			for (size_t testStagesNdx = 0; testStagesNdx < DE_LENGTH_OF_ARRAY(testStages); ++testStagesNdx)
			{
				const VkShaderStageFlagBits	testStage				= testStages[testStagesNdx].stage;
				const std::string			testName				= de::toString(testStages[testStagesNdx].name);
				const deUint32				width					= 4u;
				const deUint32				height					= 4u;
				const CaseDef				caseDef					=
				{
					testType,				//  TestType				testType;
					testOp,					//  TestOp					testOp;
					testStage,				//  VkShaderStageFlagBits	stage;
					width,					//  deUint32				width;
					height,					//  deUint32				height;
				};

				if ((testOps[testOpNdx].applicableInStages & static_cast<VkShaderStageFlags>(testStage)) == 0)
					continue;

				testOpGroup->addChild(new ComplexControlFlowTestCase(testCtx, testName.c_str(), "", caseDef));
			}

			testTypeGroup->addChild(testOpGroup.release());
		}

		group->addChild(testTypeGroup.release());
	}

	return group.release();
}

}	// RayTracing
}	// vkt
