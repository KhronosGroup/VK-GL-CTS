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
#include "vkImageUtil.hpp"
#include "vkTypeUtil.hpp"

#include "tcuTextureUtil.hpp"

#include "vkRayTracingUtil.hpp"

#include "deClock.h"

#include <cmath>
#include <limits>
#include <iostream>

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
	bool		deviceBuild;
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
	typedef de::SharedPtr<BottomLevelAccelerationStructure> BlasPtr;
	typedef de::SharedPtr<TopLevelAccelerationStructure>	TlasPtr;
	typedef BottomLevelAccelerationStructurePool			BlasPool;

									RayTracingBuildTestInstance			(Context& context, const CaseDef& data);
									~RayTracingBuildTestInstance		(void);
	tcu::TestStatus					iterate								(void);

protected:
	bool							verifyAllocationCount				() const;
	void							checkSupportInInstance				(void) const;
	deUint32						validateBuffer						(de::MovePtr<BufferWithMemory>		buffer);
	de::MovePtr<BufferWithMemory>	runTest								(bool								useGpuBuild,
																		 deUint32							workerThreadsCount);
	TlasPtr							initTopAccelerationStructure		(bool								useGpuBuild,
																		 deUint32							workerThreadsCount,
																		 const BlasPool&					pool);
	void							createTopAccelerationStructure		(VkCommandBuffer					cmdBuffer,
																		 TopLevelAccelerationStructure*		tlas);
	void							initBottomAccelerationStructures	(BlasPool&							pool,
																		 bool								useGpuBuild,
																		 deUint32							workerThreadsCount) const;
	void							initBottomAccelerationStructure		(BlasPtr							blas,
																		 bool								useGpuBuild,
																		 deUint32							workerThreadsCount,
																		 tcu::UVec2&						startPos,
																		 bool								triangles) const;

private:
	CaseDef							m_data;
	const VkFormat					m_format;
};

RayTracingBuildTestInstance::RayTracingBuildTestInstance (Context& context, const CaseDef& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
	, m_format				(VK_FORMAT_R32_UINT)
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

void RayTracingTestCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

	const VkPhysicalDeviceRayTracingPipelineFeaturesKHR&	rayTracingPipelineFeaturesKHR		= context.getRayTracingPipelineFeatures();
	if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == DE_FALSE )
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

	const VkPhysicalDeviceAccelerationStructureFeaturesKHR&	accelerationStructureFeaturesKHR	= context.getAccelerationStructureFeatures();
	if (accelerationStructureFeaturesKHR.accelerationStructure == DE_FALSE)
		TCU_THROW(TestError, "VK_KHR_ray_tracing_pipeline requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

	if (!m_data.deviceBuild)
	{
		context.requireDeviceFunctionality("VK_KHR_deferred_host_operations");
		if (accelerationStructureFeaturesKHR.accelerationStructureHostCommands == DE_FALSE)
			TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructureHostCommands");
	}
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

auto RayTracingBuildTestInstance::initTopAccelerationStructure (bool			useGpuBuild,
																deUint32		workerThreadsCount,
																const BlasPool&	pool) -> TlasPtr
{
	de::MovePtr<TopLevelAccelerationStructure>	result		= makeTopLevelAccelerationStructure();
	const std::vector<BlasPtr>&					blases		= pool.structures();

	result->setInstanceCount(blases.size());
	result->setBuildType(useGpuBuild ? VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR : VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR);
	result->setDeferredOperation(m_data.deferredOperation, workerThreadsCount);

	for (size_t instanceNdx = 0; instanceNdx < blases.size(); ++instanceNdx)
	{
		const bool	triangles								= (m_data.testType == TEST_TYPE_TRIANGLES) || (m_data.testType == TEST_TYPE_MIXED && (instanceNdx & 1) == 0);
		deUint32	instanceShaderBindingTableRecordOffset	= triangles ? 0 : 1;

		result->addInstance(blases[instanceNdx], vk::identityMatrix3x4, 0, 0xFF, instanceShaderBindingTableRecordOffset);
	}

	return TlasPtr(result.release());
}

void RayTracingBuildTestInstance::createTopAccelerationStructure (VkCommandBuffer					cmdBuffer,
																  TopLevelAccelerationStructure*	tlas)
{
	const DeviceInterface&						vkd			= m_context.getDeviceInterface();
	const VkDevice								device		= m_context.getDevice();
	Allocator&									allocator	= m_context.getDefaultAllocator();

	tlas->createAndBuild(vkd, device, cmdBuffer, allocator);
}

void RayTracingBuildTestInstance::initBottomAccelerationStructure (BlasPtr		blas,
																   bool			useGpuBuild,
																   deUint32		workerThreadsCount,
																   tcu::UVec2&	startPos,
																   bool			triangles) const
{
	blas->setBuildType(useGpuBuild ? VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR : VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR);
	blas->setDeferredOperation(m_data.deferredOperation, workerThreadsCount);
	blas->setGeometryCount(m_data.geometriesGroupCount);

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
			const deUint32	m	= (n + 13) % (m_data.width * m_data.height);

			if (triangles)
			{
				const float	xm	= (x0 + x1) / 2.0f;
				const float	ym	= (y0 + y1) / 2.0f;

				geometryData.push_back(tcu::Vec3(x0, y0, z));
				geometryData.push_back(tcu::Vec3(x1, ym, z));
				geometryData.push_back(tcu::Vec3(xm, y1, z));
			}
			else
			{
				geometryData.push_back(tcu::Vec3(x0, y0, z));
				geometryData.push_back(tcu::Vec3(x1, y1, z));
			}

			startPos.y() = m / m_data.width;
			startPos.x() = m % m_data.width;
		}

		blas->addGeometry(geometryData, triangles);
	}
}

void RayTracingBuildTestInstance::initBottomAccelerationStructures	(BlasPool&	pool,
																	 bool		useGpuBuild,
																	 deUint32	workerThreadsCount) const
{
	tcu::UVec2					startPos	{};
	const DeviceInterface&		vkd			= m_context.getDeviceInterface();
	const VkDevice				device		= m_context.getDevice();
	Allocator&					allocator	= m_context.getDefaultAllocator();
	const VkDeviceSize			maxBuffSize	= 3 * (VkDeviceSize(1) << 30); // 3GB

	for (size_t instanceNdx = 0; instanceNdx < m_data.instancesGroupCount; ++instanceNdx)	pool.add();

	const std::vector<BlasPtr>&	blases		= pool.structures();

	for (size_t instanceNdx = 0; instanceNdx < m_data.instancesGroupCount; ++instanceNdx)
	{
		const bool	triangles	= (m_data.testType == TEST_TYPE_TRIANGLES) || (m_data.testType == TEST_TYPE_MIXED && (instanceNdx & 1) == 0);
		initBottomAccelerationStructure(blases[instanceNdx], useGpuBuild, workerThreadsCount, startPos, triangles);
	}

	pool.batchCreateAdjust(vkd, device, allocator, maxBuffSize);
}

bool RayTracingBuildTestInstance::verifyAllocationCount () const
{
	BlasPool					pool					{};
	tcu::UVec2					startPos				{};
	const DeviceInterface&		vkd						= m_context.getDeviceInterface();
	const VkDevice				device					= m_context.getDevice();
	auto&						log						= m_context.getTestContext().getLog();
	const size_t				avvailableAllocCount	= m_context.getDeviceProperties().limits.maxMemoryAllocationCount;
	const VkDeviceSize			maxBufferSize			= 3 * (VkDeviceSize(1) << 30); // 3GB


	for (size_t instanceNdx = 0; instanceNdx < m_data.instancesGroupCount; ++instanceNdx)	pool.add();

	const std::vector<BlasPtr>&	blases		= pool.structures();

	for (size_t instanceNdx = 0; instanceNdx < m_data.instancesGroupCount; ++instanceNdx)
	{
		const bool	triangles	= (m_data.testType == TEST_TYPE_TRIANGLES) || (m_data.testType == TEST_TYPE_MIXED && (instanceNdx & 1) == 0);
		initBottomAccelerationStructure(blases[instanceNdx], true, 0, startPos, triangles);
	}

	const size_t	poolAllocationCount		= pool.getAllocationCount(vkd, device, maxBufferSize);
	const size_t	requiredAllocationCount = poolAllocationCount + 120;

	log << tcu::TestLog::Message
		<< "The test consumes " << poolAllocationCount
		<< " allocations out of " << avvailableAllocCount << " available"
		<< tcu::TestLog::EndMessage;

	return (requiredAllocationCount < avvailableAllocCount);
}

de::MovePtr<BufferWithMemory> RayTracingBuildTestInstance::runTest (bool useGpuBuild, deUint32 workerThreadsCount)
{
	const InstanceInterface&					vki									= m_context.getInstanceInterface();
	const DeviceInterface&						vkd									= m_context.getDeviceInterface();
	const VkDevice								device								= m_context.getDevice();
	const VkPhysicalDevice						physicalDevice						= m_context.getPhysicalDevice();
	const deUint32								queueFamilyIndex					= m_context.getUniversalQueueFamilyIndex();
	const VkQueue								queue								= m_context.getUniversalQueue();
	Allocator&									allocator							= m_context.getDefaultAllocator();
	const deUint32								pixelCount							= m_data.width * m_data.height;
	const deUint32								shaderGroupHandleSize				= getShaderGroupSize(vki, physicalDevice);
	const deUint32								shaderGroupBaseAlignment			= getShaderGroupBaseAlignment(vki, physicalDevice);

	const Move<VkDescriptorSetLayout>			descriptorSetLayout					= DescriptorSetLayoutBuilder()
																							.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, ALL_RAY_TRACING_STAGES)
																							.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, ALL_RAY_TRACING_STAGES)
																							.build(vkd, device);
	const Move<VkDescriptorPool>				descriptorPool						= DescriptorPoolBuilder()
																							.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
																							.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
																							.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const Move<VkDescriptorSet>					descriptorSet						= makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout);
	const Move<VkPipelineLayout>				pipelineLayout						= makePipelineLayout(vkd, device, descriptorSetLayout.get());
	const Move<VkCommandPool>					cmdPool								= createCommandPool(vkd, device, 0, queueFamilyIndex);
	const Move<VkCommandBuffer>					cmdBuffer							= allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	de::MovePtr<RayTracingPipeline>				rayTracingPipeline					= de::newMovePtr<RayTracingPipeline>();
	Move<VkShaderModule>						raygenShader						= createShaderModule(vkd, device, m_context.getBinaryCollection().get("rgen"), 0);
	Move<VkShaderModule>						hitShader							= createShaderModule(vkd, device, m_context.getBinaryCollection().get("ahit"), 0);
	Move<VkShaderModule>						missShader							= createShaderModule(vkd, device, m_context.getBinaryCollection().get("miss"), 0);
	Move<VkShaderModule>						intersectionShader					= createShaderModule(vkd, device, m_context.getBinaryCollection().get("sect"), 0);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,		*raygenShader,			0u);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR,		*hitShader,				1u);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR,		*hitShader,				2u);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, *intersectionShader,	2u);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR,			*missShader,			3u);
	Move<VkPipeline>							pipeline							= rayTracingPipeline->createPipeline(vkd, device, *pipelineLayout);
	const de::MovePtr<BufferWithMemory>			raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0u, 1u);
	const de::MovePtr<BufferWithMemory>			hitShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1u, 2u);
	const de::MovePtr<BufferWithMemory>			missShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 3u, 1u);
	const VkStridedDeviceAddressRegionKHR		raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR		hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, 2u * shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR		missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR		callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

	const VkImageCreateInfo						imageCreateInfo						= makeImageCreateInfo(m_data.width, m_data.height, m_format);
	const VkImageSubresourceRange				imageSubresourceRange				= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0, 1u);
	const de::MovePtr<ImageWithMemory>			image								= de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
	const Move<VkImageView>						imageView							= makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_2D, m_format, imageSubresourceRange);

	const VkBufferCreateInfo					bufferCreateInfo					= makeBufferCreateInfo(pixelCount*sizeof(deUint32), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkImageSubresourceLayers				bufferImageSubresourceLayers		= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const VkBufferImageCopy						bufferImageRegion					= makeBufferImageCopy(makeExtent3D(m_data.width, m_data.height, 1u), bufferImageSubresourceLayers);
	de::MovePtr<BufferWithMemory>				buffer								= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));

	const VkDescriptorImageInfo					descriptorImageInfo					= makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);

	const VkImageMemoryBarrier					preImageBarrier						= makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT,
																						VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
																						**image, imageSubresourceRange);
	const VkImageMemoryBarrier					postImageBarrier					= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
																						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
																						**image, imageSubresourceRange);
	const VkMemoryBarrier						postTraceMemoryBarrier				= makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
	const VkMemoryBarrier						postCopyMemoryBarrier				= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	const VkClearValue							clearValue							= makeClearValueColorU32(5u, 5u, 5u, 255u);

	TlasPtr										topLevelAccelerationStructure;
	BottomLevelAccelerationStructurePool		blasPool;

	initBottomAccelerationStructures(blasPool, useGpuBuild, workerThreadsCount);
	blasPool.batchBuild(vkd, device, *cmdPool, queue);

	beginCommandBuffer(vkd, *cmdBuffer, 0u);
	{
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preImageBarrier);
		vkd.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1, &imageSubresourceRange);
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, &postImageBarrier);

		topLevelAccelerationStructure = initTopAccelerationStructure(useGpuBuild, workerThreadsCount, blasPool);
		createTopAccelerationStructure(*cmdBuffer, topLevelAccelerationStructure.get());

		VkWriteDescriptorSetAccelerationStructureKHR	accelerationStructureWriteDescriptorSet	=
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	//  VkStructureType						sType;
			DE_NULL,															//  const void*							pNext;
			1u,																	//  deUint32							accelerationStructureCount;
			topLevelAccelerationStructure->getPtr(),							//  const VkAccelerationStructureKHR*	pAccelerationStructures;
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
	de::MovePtr<RayTracingProperties>		rayTracingProperties	= makeRayTracingProperties(vki, physicalDevice);

	if (rayTracingProperties->getMaxPrimitiveCount() < m_data.squaresGroupCount)
		TCU_THROW(NotSupportedError, "Triangles required more than supported");

	if (rayTracingProperties->getMaxGeometryCount() < m_data.geometriesGroupCount)
		TCU_THROW(NotSupportedError, "Geometries required more than supported");

	if (rayTracingProperties->getMaxInstanceCount() < m_data.instancesGroupCount)
		TCU_THROW(NotSupportedError, "Instances required more than supported");

	if (!verifyAllocationCount())
		TCU_THROW(NotSupportedError, "Memory allocations required more than supported");
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

tcu::TestStatus RayTracingBuildTestInstance::iterate (void)
{
	checkSupportInInstance();

	const deUint32	failures = validateBuffer(runTest(m_data.deviceBuild, m_data.workerThreadsCount));

	return (failures == 0) ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("failures=" + de::toString(failures));
}

}	// anonymous

static void buildTest (tcu::TestCaseGroup* testParentGroup, deUint32 threadsCount, bool deviceBuild)
{
	const char*		tests[]	=
	{
		"level_primitives",
		"level_geometries",
		"level_instances"
	};
	const deUint32		sizes[]				= { 4, 16, 64, 256, 1024 };
	const deUint32		factors[]			= { 1, 4 };
	const bool			deferredOperation	= threadsCount != 0;
	tcu::TestContext&	testCtx				= testParentGroup->getTestContext();

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
				TEST_TYPE_TRIANGLES,	//  TestType	testType;
				sizes[sizesNdx],		//  deUint32	width;
				sizes[sizesNdx],		//  deUint32	height;
				squaresGroupCount,		//  deUint32	squaresGroupCount;
				geometriesGroupCount,	//  deUint32	geometriesGroupCount;
				instancesGroupCount,	//  deUint32	instancesGroupCount;
				deferredOperation,		//  bool		deferredOperation;
				threadsCount,			//  deUint32	workerThreadsCount;
				deviceBuild				//  bool		deviceBuild;
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
				TEST_TYPE_AABBS,		//  TestType	testType;
				sizes[sizesNdx],		//  deUint32	width;
				sizes[sizesNdx],		//  deUint32	height;
				squaresGroupCount,		//  deUint32	squaresGroupCount;
				geometriesGroupCount,	//  deUint32	geometriesGroupCount;
				instancesGroupCount,	//  deUint32	instancesGroupCount;
				deferredOperation,		//  bool		deferredOperation;
				threadsCount,			//  deUint32	workerThreadsCount;
				deviceBuild				//  bool		deviceBuild;
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
				TEST_TYPE_MIXED,		//  TestType	testType;
				sizes[sizesNdx],		//  deUint32	width;
				sizes[sizesNdx],		//  deUint32	height;
				squaresGroupCount,		//  deUint32	squaresGroupCount;
				geometriesGroupCount,	//  deUint32	geometriesGroupCount;
				instancesGroupCount,	//  deUint32	instancesGroupCount;
				deferredOperation,		//  bool		deferredOperation;
				threadsCount,			//  deUint32	workerThreadsCount;
				deviceBuild				//  bool		deviceBuild;
			};
			const std::string	suffix		= de::toString(caseDef.instancesGroupCount) + '_' + de::toString(caseDef.geometriesGroupCount) + '_' + de::toString(caseDef.squaresGroupCount);
			const std::string	testName	= "mixed_" + suffix;

			if (squaresGroupCount < 2 || geometriesGroupCount < 2 || instancesGroupCount < 2)
				continue;

			group->addChild(new RayTracingTestCase(testCtx, testName.c_str(), "", caseDef));
		}

		testParentGroup->addChild(group.release());
	}
}

tcu::TestCaseGroup*	createBuildTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> buildGroup(new tcu::TestCaseGroup(testCtx, "build", "Ray tracing build tests"));

	const deUint32	threads[]	= { 0, 1, 2, 3, 4, 8, std::numeric_limits<deUint32>::max() };

	for (const auto threadCount : threads)
	{
		auto buildTargeGroup = [&](bool deviceBuild) -> void
		{
			DE_ASSERT(!(threadCount != 0 && deviceBuild));

			string	groupName, groupDesc;
			if (deviceBuild)
			{
				groupName = "gpu";
				groupDesc = "Compare results of run with acceleration structures build on GPU";
			}
			else
			{
				groupName = "cpu";
				groupDesc = "Compare results of run with acceleration structures build on CPU";
			}

			if (threadCount != 0)
			{
				groupName += threadCount == std::numeric_limits<deUint32>::max()
												? "ht_max" : "ht_" + de::toString(threadCount);
				groupDesc = "Compare results of run with acceleration structures build on CPU and using host threading";
			}

			de::MovePtr<tcu::TestCaseGroup> groupGpuCpuHt(new tcu::TestCaseGroup(testCtx, groupName.c_str(), groupDesc.c_str()));
			buildTest(groupGpuCpuHt.get(), threadCount, deviceBuild);
			buildGroup->addChild(groupGpuCpuHt.release());
		};

		if (threadCount == 0)
		{
			buildTargeGroup(true);
		}
		buildTargeGroup(false);
	}

	return buildGroup.release();
}

}	// RayTracing
}	// vkt
