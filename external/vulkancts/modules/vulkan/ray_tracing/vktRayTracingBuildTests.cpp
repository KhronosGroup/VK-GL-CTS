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
 * \brief Ray Tracing Build tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingBuildTests.hpp"

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
	TEST_TYPE_TRIANGLES,
	TEST_TYPE_AABBS,
	TEST_TYPE_MIXED,
};

struct CaseDef
{
	TestType	testType;
	deUint32	width;
	deUint32	height;
	deUint32	squaresGroupCount;
	deUint32	geometriesGroupCount;
	deUint32	instancesGroupCount;
	bool		deferredOperation;
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

Move<VkPipeline> makePipeline (const DeviceInterface&			vkd,
							   const VkDevice					device,
							   vk::BinaryCollection&			collection,
							   de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
							   VkPipelineLayout					pipelineLayout,
							   const deUint32					raygenGroup,
							   const deUint32					missGroup,
							   const deUint32					hitGroup)
{
	Move<VkShaderModule>	raygenShader		= createShaderModule(vkd, device, collection.get("rgen"), 0);
	Move<VkShaderModule>	hitShader			= createShaderModule(vkd, device, collection.get("ahit"), 0);
	Move<VkShaderModule>	missShader			= createShaderModule(vkd, device, collection.get("miss"), 0);
	Move<VkShaderModule>	intersectionShader	= createShaderModule(vkd, device, collection.get("sect"), 0);

	rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,		raygenShader,		raygenGroup);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR,		hitShader,			hitGroup);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR,			missShader,			missGroup);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR,	intersectionShader,	hitGroup);

	Move<VkPipeline> pipeline = rayTracingPipeline->createPipeline(vkd, device, pipelineLayout);

	return pipeline;
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
	de::MovePtr<BufferWithMemory>								runTest								(bool useGpuBuild);
	de::MovePtr<TopLevelAccelerationStructure>					initTopAccelerationStructure		(VkCommandBuffer											cmdBuffer,
																									 bool														useGpuBuild,
																									 vector<de::SharedPtr<BottomLevelAccelerationStructure> >&	bottomLevelAccelerationStructures);
	vector<de::SharedPtr<BottomLevelAccelerationStructure>	>	initBottomAccelerationStructures	(VkCommandBuffer	cmdBuffer,
																									 bool				useGpuBuild);
	de::MovePtr<BottomLevelAccelerationStructure>				initBottomAccelerationStructure		(VkCommandBuffer	cmdBuffer,
																									 bool				useGpuBuild,
																									 tcu::UVec2&		startPos,
																									 bool				triangles);

private:
	CaseDef														m_data;
};

RayTracingBuildTestInstance::RayTracingBuildTestInstance (Context& context, const CaseDef& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
{
}

RayTracingBuildTestInstance::~RayTracingBuildTestInstance (void)
{
}

class RayTracingTestCase : public TestCase
{
	public:
							RayTracingTestCase	(tcu::TestContext& context, const char* name, const char* desc, const CaseDef data);
							~RayTracingTestCase	(void);

	virtual	void			initPrograms		(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance		(Context& context) const;
	virtual void			checkSupport		(Context& context) const;

private:
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
	context.requireDeviceFunctionality(getRayTracingExtensionUsed());

	const VkPhysicalDeviceRayTracingFeaturesKHR&	rayTracingFeaturesKHR = context.getRayTracingFeatures();

	if (rayTracingFeaturesKHR.rayTracing == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires rayTracingFeaturesKHR.rayTracing");

	if (rayTracingFeaturesKHR.rayTracingHostAccelerationStructureCommands == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires rayTracingFeaturesKHR.rayTracingHostAccelerationStructureCommands");

	if (m_data.deferredOperation)
		context.requireDeviceFunctionality("VK_KHR_deferred_host_operations");
}

void RayTracingTestCase::initPrograms (SourceCollections& programCollection) const
{
	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_nonuniform_qualifier : enable\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
			"hitAttributeEXT vec3 attribs;\n"
			"layout(r32ui, set = 0, binding = 0) uniform uimage2D result;\n"
			"void main()\n"
			"{\n"
			"  uvec4 color = uvec4(1,0,0,1);\n"
			"  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n"
			"}\n";

		programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(css.str()));
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_nonuniform_qualifier : enable\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT dummyPayload { vec4 dummy; };\n"
			"layout(r32ui, set = 0, binding = 0) uniform uimage2D result;\n"
			"void main()\n"
			"{\n"
			"  uvec4 color = uvec4(2,0,0,1);\n"
			"  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n"
			"}\n";

		programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(css.str()));
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_nonuniform_qualifier : enable\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"hitAttributeEXT vec3 hitAttribute;\n"
			"void main()\n"
			"{\n"
			"  reportIntersectionEXT(1.0f, 0);\n"
			"}\n";

		programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(css.str()));
	}

	programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader()));
}

TestInstance* RayTracingTestCase::createInstance (Context& context) const
{
	return new RayTracingBuildTestInstance(context, m_data);
}

de::MovePtr<TopLevelAccelerationStructure> RayTracingBuildTestInstance::initTopAccelerationStructure (VkCommandBuffer											cmdBuffer,
																									  bool														useGpuBuild,
																									  vector<de::SharedPtr<BottomLevelAccelerationStructure> >&	bottomLevelAccelerationStructures)
{
	const DeviceInterface&						vkd			= m_context.getDeviceInterface();
	const VkDevice								device		= m_context.getDevice();
	Allocator&									allocator	= m_context.getDefaultAllocator();
	de::MovePtr<TopLevelAccelerationStructure>	result		= makeTopLevelAccelerationStructure();

	result->setInstanceCount(bottomLevelAccelerationStructures.size());
	result->setBuildType(useGpuBuild ? VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR : VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR);
	result->setDeferredOperation(m_data.deferredOperation);

	for (size_t structNdx = 0; structNdx < bottomLevelAccelerationStructures.size(); ++structNdx)
		result->addInstance(bottomLevelAccelerationStructures[structNdx]);

	result->createAndBuild(vkd, device, cmdBuffer, allocator);

	return result;
}

de::MovePtr<BottomLevelAccelerationStructure> RayTracingBuildTestInstance::initBottomAccelerationStructure (VkCommandBuffer	cmdBuffer,
																											bool			useGpuBuild,
																											tcu::UVec2&		startPos,
																											bool			triangles)
{
	const DeviceInterface&							vkd			= m_context.getDeviceInterface();
	const VkDevice									device		= m_context.getDevice();
	Allocator&										allocator	= m_context.getDefaultAllocator();
	de::MovePtr<BottomLevelAccelerationStructure>	result		= makeBottomLevelAccelerationStructure();

	result->setBuildType(useGpuBuild ? VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR : VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR);
	result->setDeferredOperation(m_data.deferredOperation);
	result->setGeometryCount(m_data.geometriesGroupCount);

	for (size_t geometryNdx = 0; geometryNdx < m_data.geometriesGroupCount; ++geometryNdx)
	{
		std::vector<tcu::Vec3>	geometryData;

		geometryData.reserve(m_data.squaresGroupCount * (triangles ? 3u : 2u));

		for (size_t squareNdx = 0; squareNdx < m_data.squaresGroupCount; ++squareNdx)
		{
			const deUint32	n	= m_data.width * startPos.y() + startPos.x();
			const float		x0	= float(startPos.x() + 0) / float(m_data.width);
			const float		y0	= float(startPos.y() + 0) / float(m_data.height);
			const float		x1	= float(startPos.x() + 1) / float(m_data.width);
			const float		y1	= float(startPos.y() + 1) / float(m_data.height);
			const float		z	= (n % 7 == 0) ? +1.0f : -1.0f;
			const deUint32	m	= (13 * (n + 1)) % (m_data.width * m_data.height);

			if (triangles)
			{
				const float	xm	= (x0 + x1) / 2.0f;
				const float	ym	= (y0 + y1) / 2.0f;

				geometryData.push_back(tcu::Vec3(x0, y0, z));
				geometryData.push_back(tcu::Vec3(xm, y1, z));
				geometryData.push_back(tcu::Vec3(x1, ym, z));

				if (m_data.squaresGroupCount == 1)
				{
					geometryData.push_back(tcu::Vec3(x0, y0, z));
					geometryData.push_back(tcu::Vec3(x1, ym, z));
					geometryData.push_back(tcu::Vec3(xm, y1, z));
				}
			}
			else
			{
				geometryData.push_back(tcu::Vec3(x0, y0, z));
				geometryData.push_back(tcu::Vec3(x1, y1, z));
			}

			startPos.y() = m / m_data.width;
			startPos.x() = m % m_data.width;
		}

		result->addGeometry(geometryData, triangles);
	}

	result->createAndBuild(vkd, device, cmdBuffer, allocator);

	return result;
}

vector<de::SharedPtr<BottomLevelAccelerationStructure> > RayTracingBuildTestInstance::initBottomAccelerationStructures (VkCommandBuffer	cmdBuffer,
																														bool			useGpuBuild)
{
	tcu::UVec2													startPos;
	vector<de::SharedPtr<BottomLevelAccelerationStructure> >	result;

	for (size_t instanceNdx = 0; instanceNdx < m_data.instancesGroupCount; ++instanceNdx)
	{
		const bool	triangles	= (m_data.testType == TEST_TYPE_TRIANGLES) || (m_data.testType == TEST_TYPE_MIXED && (instanceNdx & 1) == 0);
		de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure	= initBottomAccelerationStructure(cmdBuffer, useGpuBuild, startPos, triangles);

		result.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));
	}

	return result;
}

de::MovePtr<BufferWithMemory> RayTracingBuildTestInstance::runTest (bool useGpuBuild)
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
	const deUint32						shaderGroupBaseAlignment			= getShaderGroupBaseAlignment(vki, physicalDevice);

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

	de::MovePtr<RayTracingPipeline>		rayTracingPipeline					= de::newMovePtr<RayTracingPipeline>();
	const Move<VkPipeline>				pipeline							= makePipeline(vkd, device, m_context.getBinaryCollection(), rayTracingPipeline, *pipelineLayout, RAYGEN_GROUP, MISS_GROUP, HIT_GROUP);
	const de::MovePtr<BufferWithMemory>	raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, RAYGEN_GROUP, 1u);
	const de::MovePtr<BufferWithMemory>	missShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, MISS_GROUP, 1u);
	const de::MovePtr<BufferWithMemory>	hitShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, HIT_GROUP, 1u);
	const VkStridedBufferRegionKHR		raygenShaderBindingTableRegion		= makeStridedBufferRegionKHR(raygenShaderBindingTable->get(), 0, 0, shaderGroupHandleSize);
	const VkStridedBufferRegionKHR		missShaderBindingTableRegion		= makeStridedBufferRegionKHR(missShaderBindingTable->get(), 0, 0, shaderGroupHandleSize);
	const VkStridedBufferRegionKHR		hitShaderBindingTableRegion			= makeStridedBufferRegionKHR(hitShaderBindingTable->get(), 0, 0, shaderGroupHandleSize);
	const VkStridedBufferRegionKHR		callableShaderBindingTableRegion	= makeStridedBufferRegionKHR(DE_NULL, 0, 0, 0);

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
	const VkImageMemoryBarrier			postImageBarrier					= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
																				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
																				**image, imageSubresourceRange);
	const VkMemoryBarrier				preTraceMemoryBarrier				= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
	const VkMemoryBarrier				postTraceMemoryBarrier				= makeMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
	const VkMemoryBarrier				postCopyMemoryBarrier				= makeMemoryBarrier(VK_ACCESS_TRANSFER_READ_BIT, 0);
	const VkClearValue					clearValue							= makeClearValueColorU32(5u, 5u, 5u, 255u);

	vector<de::SharedPtr<BottomLevelAccelerationStructure> >	bottomLevelAccelerationStructures;
	de::MovePtr<TopLevelAccelerationStructure>					topLevelAccelerationStructure;

	beginCommandBuffer(vkd, *cmdBuffer, 0u);
	{
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preImageBarrier);
		vkd.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1, &imageSubresourceRange);
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, ALL_RAY_TRACING_STAGES, &postImageBarrier);

		bottomLevelAccelerationStructures = initBottomAccelerationStructures(*cmdBuffer, useGpuBuild);
		topLevelAccelerationStructure = initTopAccelerationStructure(*cmdBuffer, useGpuBuild, bottomLevelAccelerationStructures);

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

	const de::MovePtr<BufferWithMemory>	bufferGPU		= runTest(true);
	const deUint32*						bufferPtrGPU	= (deUint32*)bufferGPU->getAllocation().getHostPtr();
	const de::MovePtr<BufferWithMemory>	bufferCPU		= runTest(false);
	const deUint32*						bufferPtrCPU	= (deUint32*)bufferCPU->getAllocation().getHostPtr();
	deUint32							failures		= 0;
	deUint32							pos				= 0;

	for (deUint32 y = 0; y < m_data.height; ++y)
	for (deUint32 x = 0; x < m_data.width; ++x)
	{
		const deUint32	n				= m_data.width * y + x;
		const deUint32	expectedValue	= (n % 7 == 0) ? 2 : 1;

		if (bufferPtrGPU[pos] != expectedValue || bufferPtrCPU[pos] != expectedValue)
			failures++;

		++pos;
	}

	if (failures == 0)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Fail (failures=" + de::toString(failures) + ")");
}

}	// anonymous

tcu::TestCaseGroup*	createBuildTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> buildGroup(new tcu::TestCaseGroup(testCtx, "build", "Ray tracing build tests"));
	de::MovePtr<tcu::TestCaseGroup> cpuGroup(new tcu::TestCaseGroup(testCtx, "gpu_cpu", "Compare results of run with acceleration structures build on GPU and CPU"));
	de::MovePtr<tcu::TestCaseGroup> mtGroup(new tcu::TestCaseGroup(testCtx, "gpu_cpuht", "Compare results of run with acceleration structures build on GPU and using host threading"));

	const char*		tests[]	=
	{
		"level_primitives",
		"level_geometries",
		"level_instances"
	};
	const deUint32	sizes[]		= { 4, 16, 64, 256, 1024 };
	const deUint32	factors[]	= { 1, 4 };

	for (size_t targetNdx = 0; targetNdx < 2; ++targetNdx)
	{
		const bool	defferedOperation	= (targetNdx == 1);

		for (size_t testsNdx = 0; testsNdx < DE_LENGTH_OF_ARRAY(tests); ++testsNdx)
		{
			de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, tests[testsNdx], ""));

			for (size_t factorNdx = 0; factorNdx < DE_LENGTH_OF_ARRAY(factors); ++factorNdx)
			for (size_t sizesNdx = 0; sizesNdx < DE_LENGTH_OF_ARRAY(sizes); ++sizesNdx)
			{
				const deUint32	factor					= factors[factorNdx];
				const deUint32	largestGroup			= sizes[sizesNdx] * sizes[sizesNdx] / factor / factor;
				const deUint32	squaresGroupCount		= testsNdx == 0 ? largestGroup : factor;
				const deUint32	geometriesGroupCount	= testsNdx == 1 ? largestGroup : factor;
				const deUint32	instancesGroupCount		= testsNdx == 2 ? largestGroup : factor;
				const CaseDef	caseDef					=
				{
					TEST_TYPE_TRIANGLES,
					sizes[sizesNdx],
					sizes[sizesNdx],
					squaresGroupCount,
					geometriesGroupCount,
					instancesGroupCount,
					defferedOperation
				};
				const std::string	suffix		= de::toString(caseDef.instancesGroupCount) + '_' + de::toString(caseDef.geometriesGroupCount) + '_' + de::toString(caseDef.squaresGroupCount);
				const std::string	testName	= "triangles_" + suffix;

				if (squaresGroupCount == 0 || geometriesGroupCount == 0 || instancesGroupCount == 0)
					continue;

				group->addChild(new RayTracingTestCase(testCtx, testName.c_str(), "", caseDef));
			}

			for (size_t factorNdx = 0; factorNdx < DE_LENGTH_OF_ARRAY(factors); ++factorNdx)
			for (size_t sizesNdx = 0; sizesNdx < DE_LENGTH_OF_ARRAY(sizes); ++sizesNdx)
			{
				const deUint32	factor					= factors[factorNdx];
				const deUint32	largestGroup			= sizes[sizesNdx] * sizes[sizesNdx] / factor / factor;
				const deUint32	squaresGroupCount		= testsNdx == 0 ? largestGroup : factor;
				const deUint32	geometriesGroupCount	= testsNdx == 1 ? largestGroup : factor;
				const deUint32	instancesGroupCount		= testsNdx == 2 ? largestGroup : factor;
				const CaseDef	caseDef					=
				{
					TEST_TYPE_AABBS,
					sizes[sizesNdx],
					sizes[sizesNdx],
					squaresGroupCount,
					geometriesGroupCount,
					instancesGroupCount,
					defferedOperation
				};
				const std::string	suffix		= de::toString(caseDef.instancesGroupCount) + '_' + de::toString(caseDef.geometriesGroupCount) + '_' + de::toString(caseDef.squaresGroupCount);
				const std::string	testName	= "aabbs_" + suffix;

				if (squaresGroupCount == 0 || geometriesGroupCount == 0 || instancesGroupCount == 0)
					continue;

				group->addChild(new RayTracingTestCase(testCtx, testName.c_str(), "", caseDef));
			}

			for (size_t factorNdx = 0; factorNdx < DE_LENGTH_OF_ARRAY(factors); ++factorNdx)
			for (size_t sizesNdx = 0; sizesNdx < DE_LENGTH_OF_ARRAY(sizes); ++sizesNdx)
			{
				const deUint32	factor					= factors[factorNdx];
				const deUint32	largestGroup			= sizes[sizesNdx] * sizes[sizesNdx] / factor / factor;
				const deUint32	squaresGroupCount		= testsNdx == 0 ? largestGroup : factor;
				const deUint32	geometriesGroupCount	= testsNdx == 1 ? largestGroup : factor;
				const deUint32	instancesGroupCount		= testsNdx == 2 ? largestGroup : factor;
				const CaseDef	caseDef					=
				{
					TEST_TYPE_MIXED,
					sizes[sizesNdx],
					sizes[sizesNdx],
					squaresGroupCount,
					geometriesGroupCount,
					instancesGroupCount,
					defferedOperation
				};
				const std::string	suffix		= de::toString(caseDef.instancesGroupCount) + '_' + de::toString(caseDef.geometriesGroupCount) + '_' + de::toString(caseDef.squaresGroupCount);
				const std::string	testName	= "mixed_" + suffix;

				if (squaresGroupCount < 2 || geometriesGroupCount < 2 || instancesGroupCount < 2)
					continue;

				group->addChild(new RayTracingTestCase(testCtx, testName.c_str(), "", caseDef));
			}

			if (defferedOperation)
				mtGroup->addChild(group.release());
			else
				cpuGroup->addChild(group.release());
		}
	}

	buildGroup->addChild(cpuGroup.release());
	buildGroup->addChild(mtGroup.release());

	return buildGroup.release();
}

}	// RayTracing
}	// vkt
