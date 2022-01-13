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

#include "deClock.h"

#include <limits>

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
	deUint32	workerThreadsCount;
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
	deUint32													iterateNoWorkers					(void);
	deUint32													iterateWithWorkers					(void);
	void														checkSupportInInstance				(void) const;
	deUint32													validateBuffer						(de::MovePtr<BufferWithMemory>								buffer);
	de::MovePtr<BufferWithMemory>								runTest								(bool														useGpuBuild,
																									 deUint32													workerThreadsCount);
	de::MovePtr<TopLevelAccelerationStructure>					initTopAccelerationStructure		(VkCommandBuffer											cmdBuffer,
																									 bool														useGpuBuild,
																									 deUint32													workerThreadsCount,
																									 vector<de::SharedPtr<BottomLevelAccelerationStructure> >&	bottomLevelAccelerationStructures);
	vector<de::SharedPtr<BottomLevelAccelerationStructure>	>	initBottomAccelerationStructures	(VkCommandBuffer											cmdBuffer,
																									 bool														useGpuBuild,
																									 deUint32													workerThreadsCount);
	de::MovePtr<BottomLevelAccelerationStructure>				initBottomAccelerationStructure		(VkCommandBuffer											cmdBuffer,
																									 bool														useGpuBuild,
																									 deUint32													workerThreadsCount,
																									 tcu::UVec2&												startPos,
																									 bool														triangles);

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
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

	const VkPhysicalDeviceRayTracingPipelineFeaturesKHR&	rayTracingPipelineFeaturesKHR		= context.getRayTracingPipelineFeatures();
	if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == DE_FALSE )
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

	const VkPhysicalDeviceAccelerationStructureFeaturesKHR&	accelerationStructureFeaturesKHR	= context.getAccelerationStructureFeatures();
	if (accelerationStructureFeaturesKHR.accelerationStructure == DE_FALSE)
		TCU_THROW(TestError, "VK_KHR_ray_tracing_pipeline requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

	if (accelerationStructureFeaturesKHR.accelerationStructureHostCommands == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructureHostCommands");

	if (m_data.deferredOperation)
		context.requireDeviceFunctionality("VK_KHR_deferred_host_operations");
}

void RayTracingTestCase::initPrograms (SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions	buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
			"hitAttributeEXT vec3 attribs;\n"
			"layout(r32ui, set = 0, binding = 0) uniform uimage2D result;\n"
			"void main()\n"
			"{\n"
			"  uvec4 color = uvec4(1,0,0,1);\n"
			"  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n"
			"}\n";

		programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT dummyPayload { vec4 dummy; };\n"
			"layout(r32ui, set = 0, binding = 0) uniform uimage2D result;\n"
			"void main()\n"
			"{\n"
			"  uvec4 color = uvec4(2,0,0,1);\n"
			"  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n"
			"}\n";

		programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"hitAttributeEXT vec3 hitAttribute;\n"
			"void main()\n"
			"{\n"
			"  reportIntersectionEXT(1.0f, 0);\n"
			"}\n";

		programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;
}

TestInstance* RayTracingTestCase::createInstance (Context& context) const
{
	return new RayTracingBuildTestInstance(context, m_data);
}

de::MovePtr<TopLevelAccelerationStructure> RayTracingBuildTestInstance::initTopAccelerationStructure (VkCommandBuffer											cmdBuffer,
																									  bool														useGpuBuild,
																									  deUint32													workerThreadsCount,
																									  vector<de::SharedPtr<BottomLevelAccelerationStructure> >&	bottomLevelAccelerationStructures)
{
	const DeviceInterface&						vkd			= m_context.getDeviceInterface();
	const VkDevice								device		= m_context.getDevice();
	Allocator&									allocator	= m_context.getDefaultAllocator();
	de::MovePtr<TopLevelAccelerationStructure>	result		= makeTopLevelAccelerationStructure();

	result->setInstanceCount(bottomLevelAccelerationStructures.size());
	result->setBuildType(useGpuBuild ? VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR : VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR);
	result->setDeferredOperation(m_data.deferredOperation, workerThreadsCount);

	for (size_t instanceNdx = 0; instanceNdx < bottomLevelAccelerationStructures.size(); ++instanceNdx)
	{
		const bool	triangles								= (m_data.testType == TEST_TYPE_TRIANGLES) || (m_data.testType == TEST_TYPE_MIXED && (instanceNdx & 1) == 0);
		deUint32	instanceShaderBindingTableRecordOffset	= triangles ? 0 : 1;

		result->addInstance(bottomLevelAccelerationStructures[instanceNdx], vk::identityMatrix3x4, 0, 0xFF, instanceShaderBindingTableRecordOffset);
	}

	result->createAndBuild(vkd, device, cmdBuffer, allocator);

	return result;
}

de::MovePtr<BottomLevelAccelerationStructure> RayTracingBuildTestInstance::initBottomAccelerationStructure (VkCommandBuffer	cmdBuffer,
																											bool			useGpuBuild,
																											deUint32		workerThreadsCount,
																											tcu::UVec2&		startPos,
																											bool			triangles)
{
	const DeviceInterface&							vkd			= m_context.getDeviceInterface();
	const VkDevice									device		= m_context.getDevice();
	Allocator&										allocator	= m_context.getDefaultAllocator();
	de::MovePtr<BottomLevelAccelerationStructure>	result		= makeBottomLevelAccelerationStructure();

	result->setBuildType(useGpuBuild ? VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR : VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR);
	result->setDeferredOperation(m_data.deferredOperation, workerThreadsCount);
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
																														bool			useGpuBuild,
																														deUint32		workerThreadsCount)
{
	tcu::UVec2													startPos;
	vector<de::SharedPtr<BottomLevelAccelerationStructure> >	result;

	for (size_t instanceNdx = 0; instanceNdx < m_data.instancesGroupCount; ++instanceNdx)
	{
		const bool	triangles	= (m_data.testType == TEST_TYPE_TRIANGLES) || (m_data.testType == TEST_TYPE_MIXED && (instanceNdx & 1) == 0);
		de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure	= initBottomAccelerationStructure(cmdBuffer, useGpuBuild, workerThreadsCount, startPos, triangles);

		result.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));
	}

	return result;
}

de::MovePtr<BufferWithMemory> RayTracingBuildTestInstance::runTest (bool useGpuBuild, deUint32 workerThreadsCount)
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
	Move<VkShaderModule>				raygenShader						= createShaderModule(vkd, device, m_context.getBinaryCollection().get("rgen"), 0);
	de::SharedPtr<Move<VkShaderModule>>	hitShader							= makeVkSharedPtr(createShaderModule(vkd, device, m_context.getBinaryCollection().get("ahit"), 0));
	Move<VkShaderModule>				missShader							= createShaderModule(vkd, device, m_context.getBinaryCollection().get("miss"), 0);
	Move<VkShaderModule>				intersectionShader					= createShaderModule(vkd, device, m_context.getBinaryCollection().get("sect"), 0);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,		raygenShader,		0u);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR,		hitShader,			1u);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR,		hitShader,			2u);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, intersectionShader, 2u);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR,			missShader,			3u);
	Move<VkPipeline> pipeline = rayTracingPipeline->createPipeline(vkd, device, *pipelineLayout);
	const de::MovePtr<BufferWithMemory>	raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0u, 1u);
	const de::MovePtr<BufferWithMemory>	hitShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1u, 2u);
	const de::MovePtr<BufferWithMemory>	missShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 3u, 1u);
	const VkStridedDeviceAddressRegionKHR	raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR	hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, 2u * shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR	missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR	callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

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
	const VkImageMemoryBarrier			postImageBarrier					= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
																				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
																				**image, imageSubresourceRange);
	const VkMemoryBarrier				postTraceMemoryBarrier				= makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
	const VkMemoryBarrier				postCopyMemoryBarrier				= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	const VkClearValue					clearValue							= makeClearValueColorU32(5u, 5u, 5u, 255u);

	vector<de::SharedPtr<BottomLevelAccelerationStructure> >	bottomLevelAccelerationStructures;
	de::MovePtr<TopLevelAccelerationStructure>					topLevelAccelerationStructure;

	beginCommandBuffer(vkd, *cmdBuffer, 0u);
	{
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preImageBarrier);
		vkd.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1, &imageSubresourceRange);
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, &postImageBarrier);

		bottomLevelAccelerationStructures = initBottomAccelerationStructures(*cmdBuffer, useGpuBuild, workerThreadsCount);
		topLevelAccelerationStructure = initTopAccelerationStructure(*cmdBuffer, useGpuBuild, workerThreadsCount, bottomLevelAccelerationStructures);

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

		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);

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

deUint32 RayTracingBuildTestInstance::validateBuffer (de::MovePtr<BufferWithMemory>	buffer)
{
	const deUint32*	bufferPtr	= (deUint32*)buffer->getAllocation().getHostPtr();
	deUint32		failures	= 0;
	deUint32		pos			= 0;

	for (deUint32 y = 0; y < m_data.height; ++y)
	for (deUint32 x = 0; x < m_data.width; ++x)
	{
		const deUint32	anyHitValue		= 1;
		const deUint32	missValue		= 2;

		const deUint32	n				= m_data.width * y + x;
		const deUint32	expectedValue	= (n % 7 == 0) ? missValue : anyHitValue;

		if (bufferPtr[pos] != expectedValue)
		{
			if (m_data.testType == TEST_TYPE_AABBS || m_data.testType == TEST_TYPE_MIXED)
			{
				// In the case of AABB geometries, implementations may increase their size in
				// an acceleration structure in order to mitigate precision issues. This may
				// result in false positives being reported to the application."

				if (bufferPtr[pos] != anyHitValue)
				{
					failures++;
				}
			}
			else
			{
				failures++;
			}
		}

		++pos;
	}

	return failures;
}

deUint32 RayTracingBuildTestInstance::iterateWithWorkers (void)
{
	const deUint64					singleThreadTimeStart	= deGetMicroseconds();
	de::MovePtr<BufferWithMemory>	singleThreadBufferCPU	= runTest(false, 0);
	const deUint32					singleThreadFailures	= validateBuffer(singleThreadBufferCPU);
	const deUint64					singleThreadTime		= deGetMicroseconds() - singleThreadTimeStart;

	deUint64						multiThreadTimeStart	= deGetMicroseconds();
	de::MovePtr<BufferWithMemory>	multiThreadBufferCPU	= runTest(false, m_data.workerThreadsCount);
	const deUint32					multiThreadFailures		= validateBuffer(multiThreadBufferCPU);
	deUint64						multiThreadTime			= deGetMicroseconds() - multiThreadTimeStart;
	const deUint64					multiThreadTimeOut		= 10 * singleThreadTime;

	const deUint32					failures				= singleThreadFailures + multiThreadFailures;

	DE_ASSERT(multiThreadTimeOut > singleThreadTime);

	if (multiThreadTime > multiThreadTimeOut)
	{
		string failMsg	= "Time of multithreaded test execution " + de::toString(multiThreadTime) +
						  " that is longer than expected execution time " + de::toString(multiThreadTimeOut);

		TCU_FAIL(failMsg);
	}

	return failures;
}

deUint32 RayTracingBuildTestInstance::iterateNoWorkers (void)
{
	de::MovePtr<BufferWithMemory>	bufferGPU		= runTest(true, 0);
	de::MovePtr<BufferWithMemory>	bufferCPU		= runTest(false, 0);
	const deUint32					failuresGPU		= validateBuffer(bufferGPU);
	const deUint32					failuresCPU		= validateBuffer(bufferCPU);
	const deUint32					failures		= failuresGPU + failuresCPU;

	return failures;
}

tcu::TestStatus RayTracingBuildTestInstance::iterate (void)
{
	checkSupportInInstance();

	const deUint32	failures	= m_data.workerThreadsCount == 0
								? iterateNoWorkers()
								: iterateWithWorkers();

	if (failures == 0)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("failures=" + de::toString(failures));
}

}	// anonymous

tcu::TestCaseGroup*	createBuildTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> buildGroup(new tcu::TestCaseGroup(testCtx, "build", "Ray tracing build tests"));

	const char*		tests[]	=
	{
		"level_primitives",
		"level_geometries",
		"level_instances"
	};
	const deUint32	sizes[]		= { 4, 16, 64, 256, 1024 };
	const deUint32	factors[]	= { 1, 4 };
	const deUint32	threads[]	= { 0, 1, 2, 3, 4, 8, std::numeric_limits<deUint32>::max() };

	for (size_t threadNdx = 0; threadNdx <= DE_LENGTH_OF_ARRAY(threads); ++threadNdx)
	{
		const bool						defferedOperation	= threadNdx != DE_LENGTH_OF_ARRAY(threads);
		const deUint32					threadsCount		= threadNdx < DE_LENGTH_OF_ARRAY(threads) ? threads[threadNdx] : 0;
		const string					groupName			= !defferedOperation ? "gpu_cpu"
															: threadsCount == 0 ? "gpu_cpuht"
															: threadsCount == std::numeric_limits<deUint32>::max() ? "cpuht_max"
															: "cpuht_" + de::toString(threadsCount);
		const string					groupDesc			= !defferedOperation ? "Compare results of run with acceleration structures build on GPU and CPU"
															: threadsCount > 0 ? "Compare results of run with acceleration structures build on GPU and using host threading"
															: "Run acceleration structures build using host threading";

		const bool						deviceBuild			= !defferedOperation || threadsCount == 0;

		de::MovePtr<tcu::TestCaseGroup>	groupGpuCpuHt		(new tcu::TestCaseGroup(testCtx, groupName.c_str(), groupDesc.c_str()));

		for (size_t testsNdx = 0; testsNdx < DE_LENGTH_OF_ARRAY(tests); ++testsNdx)
		{
			de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, tests[testsNdx], ""));

			for (size_t factorNdx = 0; factorNdx < DE_LENGTH_OF_ARRAY(factors); ++factorNdx)
			for (size_t sizesNdx = 0; sizesNdx < DE_LENGTH_OF_ARRAY(sizes); ++sizesNdx)
			{
				if (deviceBuild && sizes[sizesNdx] > 256)
					continue;

				const deUint32	factor					= factors[factorNdx];
				const deUint32	largestGroup			= sizes[sizesNdx] * sizes[sizesNdx] / factor / factor;
				const deUint32	squaresGroupCount		= testsNdx == 0 ? largestGroup : factor;
				const deUint32	geometriesGroupCount	= testsNdx == 1 ? largestGroup : factor;
				const deUint32	instancesGroupCount		= testsNdx == 2 ? largestGroup : factor;
				const CaseDef	caseDef					=
				{
					TEST_TYPE_TRIANGLES,	//  TestType	testType;
					sizes[sizesNdx],		//  deUint32	width;
					sizes[sizesNdx],		//  deUint32	height;
					squaresGroupCount,		//  deUint32	squaresGroupCount;
					geometriesGroupCount,	//  deUint32	geometriesGroupCount;
					instancesGroupCount,	//  deUint32	instancesGroupCount;
					defferedOperation,		//  bool		deferredOperation;
					threadsCount			//  deUint32	workerThreadsCount;
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
				if (deviceBuild && sizes[sizesNdx] > 256)
					continue;

				const deUint32	factor					= factors[factorNdx];
				const deUint32	largestGroup			= sizes[sizesNdx] * sizes[sizesNdx] / factor / factor;
				const deUint32	squaresGroupCount		= testsNdx == 0 ? largestGroup : factor;
				const deUint32	geometriesGroupCount	= testsNdx == 1 ? largestGroup : factor;
				const deUint32	instancesGroupCount		= testsNdx == 2 ? largestGroup : factor;
				const CaseDef	caseDef					=
				{
					TEST_TYPE_AABBS,		//  TestType	testType;
					sizes[sizesNdx],		//  deUint32	width;
					sizes[sizesNdx],		//  deUint32	height;
					squaresGroupCount,		//  deUint32	squaresGroupCount;
					geometriesGroupCount,	//  deUint32	geometriesGroupCount;
					instancesGroupCount,	//  deUint32	instancesGroupCount;
					defferedOperation,		//  bool		deferredOperation;
					threadsCount			//  deUint32	workerThreadsCount;
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
				if (deviceBuild && sizes[sizesNdx] > 256)
					continue;

				const deUint32	factor					= factors[factorNdx];
				const deUint32	largestGroup			= sizes[sizesNdx] * sizes[sizesNdx] / factor / factor;
				const deUint32	squaresGroupCount		= testsNdx == 0 ? largestGroup : factor;
				const deUint32	geometriesGroupCount	= testsNdx == 1 ? largestGroup : factor;
				const deUint32	instancesGroupCount		= testsNdx == 2 ? largestGroup : factor;
				const CaseDef	caseDef					=
				{
					TEST_TYPE_MIXED,		//  TestType	testType;
					sizes[sizesNdx],		//  deUint32	width;
					sizes[sizesNdx],		//  deUint32	height;
					squaresGroupCount,		//  deUint32	squaresGroupCount;
					geometriesGroupCount,	//  deUint32	geometriesGroupCount;
					instancesGroupCount,	//  deUint32	instancesGroupCount;
					defferedOperation,		//  bool		deferredOperation;
					threadsCount			//  deUint32	workerThreadsCount;
				};
				const std::string	suffix		= de::toString(caseDef.instancesGroupCount) + '_' + de::toString(caseDef.geometriesGroupCount) + '_' + de::toString(caseDef.squaresGroupCount);
				const std::string	testName	= "mixed_" + suffix;

				if (squaresGroupCount < 2 || geometriesGroupCount < 2 || instancesGroupCount < 2)
					continue;

				group->addChild(new RayTracingTestCase(testCtx, testName.c_str(), "", caseDef));
			}

			groupGpuCpuHt->addChild(group.release());
		}

		buildGroup->addChild(groupGpuCpuHt.release());
	}

	return buildGroup.release();
}

}	// RayTracing
}	// vkt
