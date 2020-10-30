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
 * \brief Acceleration Structure Null Handle Tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingNullASTests.hpp"

#include "vkDefs.hpp"

#include "vktTestCase.hpp"
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

#include "deClock.h"

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

struct CaseDef
{
	deUint32	width;
	deUint32	height;
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

struct TestDeviceFeatures
{
	VkPhysicalDeviceRobustness2FeaturesEXT				robustness2Features;
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR		rayTracingPipelineFeatures;
	VkPhysicalDeviceAccelerationStructureFeaturesKHR	accelerationStructureFeatures;
	VkPhysicalDeviceBufferDeviceAddressFeaturesKHR		deviceAddressFeatures;
	VkPhysicalDeviceFeatures2							deviceFeatures;

	void linkStructures ()
	{
		robustness2Features.pNext			= nullptr;
		rayTracingPipelineFeatures.pNext	= &robustness2Features;
		accelerationStructureFeatures.pNext	= &rayTracingPipelineFeatures;
		deviceAddressFeatures.pNext			= &accelerationStructureFeatures;
		deviceFeatures.pNext				= &deviceAddressFeatures;
	}

	TestDeviceFeatures (const InstanceInterface& vki, VkPhysicalDevice physicalDevice)
	{
		robustness2Features.sType			= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
		rayTracingPipelineFeatures.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
		accelerationStructureFeatures.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
		deviceAddressFeatures.sType			= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR;
		deviceFeatures.sType				= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

		linkStructures();
		vki.getPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures);
	}
};

struct DeviceHelper
{
	Move<VkDevice>					device;
	de::MovePtr<DeviceDriver>		vkd;
	deUint32						queueFamilyIndex;
	VkQueue							queue;
	de::MovePtr<SimpleAllocator>	allocator;

	DeviceHelper (Context& context)
	{
		const auto&	vkp				= context.getPlatformInterface();
		const auto&	vki				= context.getInstanceInterface();
		const auto	instance		= context.getInstance();
		const auto	physicalDevice	= context.getPhysicalDevice();
		const auto	queuePriority	= 1.0f;

		// Queue index first.
		queueFamilyIndex = context.getUniversalQueueFamilyIndex();

		// Get device features (these have already been checked in the test case).
		TestDeviceFeatures features(vki, physicalDevice);
		features.linkStructures();

		// Make sure uneeded robustness features are disabled.
		features.deviceFeatures.features.robustBufferAccess	= VK_FALSE;
		features.robustness2Features.robustBufferAccess2	= VK_FALSE;
		features.robustness2Features.robustImageAccess2		= VK_FALSE;

		const VkDeviceQueueCreateInfo queueInfo =
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	//	VkStructureType				sType;
			nullptr,									//	const void*					pNext;
			0u,											//	VkDeviceQueueCreateFlags	flags;
			queueFamilyIndex,							//	deUint32					queueFamilyIndex;
			1u,											//	deUint32					queueCount;
			&queuePriority,								//	const float*				pQueuePriorities;
		};

		// Required extensions.
		std::vector<const char*> requiredExtensions;
		requiredExtensions.push_back("VK_KHR_ray_tracing_pipeline");
		requiredExtensions.push_back("VK_KHR_acceleration_structure");
		requiredExtensions.push_back("VK_KHR_buffer_device_address");
		requiredExtensions.push_back("VK_KHR_deferred_host_operations");
		requiredExtensions.push_back("VK_EXT_descriptor_indexing");
		requiredExtensions.push_back("VK_KHR_spirv_1_4");
		requiredExtensions.push_back("VK_KHR_shader_float_controls");
		requiredExtensions.push_back("VK_EXT_robustness2");

		const VkDeviceCreateInfo createInfo =
		{
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,				//	VkStructureType					sType;
			features.deviceFeatures.pNext,						//	const void*						pNext;
			0u,													//	VkDeviceCreateFlags				flags;
			1u,													//	deUint32						queueCreateInfoCount;
			&queueInfo,											//	const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
			0u,													//	deUint32						enabledLayerCount;
			nullptr,											//	const char* const*				ppEnabledLayerNames;
			static_cast<deUint32>(requiredExtensions.size()),	//	deUint32						enabledExtensionCount;
			requiredExtensions.data(),							//	const char* const*				ppEnabledExtensionNames;
			&features.deviceFeatures.features,					//	const VkPhysicalDeviceFeatures*	pEnabledFeatures;
		};

		// Create custom device and related objects.
		device		= createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), vkp, instance, vki, physicalDevice, &createInfo);
		vkd			= de::MovePtr<DeviceDriver>(new DeviceDriver(vkp, instance, device.get()));
		queue		= getDeviceQueue(*vkd, *device, queueFamilyIndex, 0u);
		allocator	= de::MovePtr<SimpleAllocator>(new SimpleAllocator(*vkd, device.get(), getPhysicalDeviceMemoryProperties(vki, physicalDevice)));
	}
};

class RayTracingBuildTestInstance : public TestInstance
{
public:
									RayTracingBuildTestInstance			(Context& context, const CaseDef& data);
									~RayTracingBuildTestInstance		(void);
	tcu::TestStatus					iterate								(void);

protected:
	deUint32						validateBuffer						(de::MovePtr<BufferWithMemory> buffer);
	de::MovePtr<BufferWithMemory>	runTest								(DeviceHelper& deviceHelper);

private:
	CaseDef							m_data;
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
}

RayTracingTestCase::~RayTracingTestCase	(void)
{
}

void RayTracingTestCase::checkSupport(Context& context) const
{
	const auto&	vki					= context.getInstanceInterface();
	const auto	physicalDevice		= context.getPhysicalDevice();
	const auto	supportedExtensions	= enumerateDeviceExtensionProperties(vki, physicalDevice, nullptr);

	if (!isExtensionSupported(supportedExtensions, RequiredExtension("VK_KHR_ray_tracing_pipeline")))
		TCU_THROW(NotSupportedError, "VK_KHR_ray_tracing_pipeline not supported");

	// VK_KHR_acceleration_structure is required by VK_KHR_ray_tracing_pipeline.
	if (!isExtensionSupported(supportedExtensions, RequiredExtension("VK_KHR_acceleration_structure")))
		TCU_FAIL("VK_KHR_acceleration_structure not supported but VK_KHR_ray_tracing_pipeline supported");

	// VK_KHR_deferred_host_operations is required by VK_KHR_ray_tracing_pipeline.
	if (!isExtensionSupported(supportedExtensions, RequiredExtension("VK_KHR_deferred_host_operations")))
		TCU_FAIL("VK_KHR_deferred_host_operations not supported but VK_KHR_ray_tracing_pipeline supported");

	// VK_KHR_buffer_device_address is required by VK_KHR_acceleration_structure.
	if (!isExtensionSupported(supportedExtensions, RequiredExtension("VK_KHR_buffer_device_address")))
		TCU_FAIL("VK_KHR_buffer_device_address not supported but VK_KHR_acceleration_structure supported");

	if (!isExtensionSupported(supportedExtensions, RequiredExtension("VK_EXT_robustness2")))
		TCU_THROW(NotSupportedError, "VK_EXT_robustness2 not supported");

	// Required extensions supported: check features.
	TestDeviceFeatures testFeatures(vki, physicalDevice);

	if (!testFeatures.rayTracingPipelineFeatures.rayTracingPipeline)
		TCU_THROW(NotSupportedError, "Ray tracing pipelines not supported");

	if (!testFeatures.robustness2Features.nullDescriptor)
		TCU_THROW(NotSupportedError, "Null descriptors not supported");
}

void RayTracingTestCase::initPrograms (SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions	buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);

	programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(getCommonRayGenerationShader())) << buildOptions;

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_nonuniform_qualifier : enable\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(r32ui, set = 0, binding = 0) uniform uimage2D result;\n"
			"hitAttributeEXT vec3 hitAttribute;\n"
			"void main()\n"
			"{\n"
			"  reportIntersectionEXT(1.0f, 0);\n"
			"  uvec4 color = uvec4(1,0,0,1);\n"
			"  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n"
			"}\n";

		programCollection.glslSources.add("sect") << glu::IntersectionSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

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
			"  uvec4 color = uvec4(2,0,0,1);\n"
			"  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n"
			"}\n";

		programCollection.glslSources.add("ahit") << glu::AnyHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

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
			"  uvec4 color = uvec4(3,0,0,1);\n"
			"  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n"
			"}\n";

		programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
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
			"  uvec4 color = uvec4(4,0,0,1);\n"
			"  imageStore(result, ivec2(gl_LaunchIDEXT.xy), color);\n"
			"}\n";

		programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}
}

TestInstance* RayTracingTestCase::createInstance (Context& context) const
{
	return new RayTracingBuildTestInstance(context, m_data);
}

de::MovePtr<BufferWithMemory> RayTracingBuildTestInstance::runTest (DeviceHelper& deviceHelper)
{
	const InstanceInterface&			vki									= m_context.getInstanceInterface();
	const VkPhysicalDevice				physicalDevice						= m_context.getPhysicalDevice();
	const DeviceDriver&					vkd									= *deviceHelper.vkd;
	const VkDevice						device								= *deviceHelper.device;
	const deUint32						queueFamilyIndex					= deviceHelper.queueFamilyIndex;
	const VkQueue						queue								= deviceHelper.queue;
	SimpleAllocator&					allocator							= *deviceHelper.allocator;
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

	const VkStridedDeviceAddressRegionKHR	raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR	missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR	hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
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
	const VkImageMemoryBarrier			postImageBarrier					= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
																				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
																				**image, imageSubresourceRange);
	const VkMemoryBarrier				postTraceMemoryBarrier				= makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
	const VkMemoryBarrier				postCopyMemoryBarrier				= makeMemoryBarrier(VK_ACCESS_TRANSFER_READ_BIT, 0);
	const VkClearValue					clearValue							= makeClearValueColorU32(5u, 5u, 5u, 255u);
	const VkAccelerationStructureKHR	topLevelAccelerationStructure		= DE_NULL;

	beginCommandBuffer(vkd, *cmdBuffer, 0u);
	{
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preImageBarrier);
		vkd.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1, &imageSubresourceRange);
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, &postImageBarrier);

		VkWriteDescriptorSetAccelerationStructureKHR	accelerationStructureWriteDescriptorSet	=
		{
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	//  VkStructureType						sType;
			DE_NULL,															//  const void*							pNext;
			1u,																	//  deUint32							accelerationStructureCount;
			&topLevelAccelerationStructure,										//  const VkAccelerationStructureKHR*	pAccelerationStructures;
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

deUint32 RayTracingBuildTestInstance::validateBuffer (de::MovePtr<BufferWithMemory>	buffer)
{
	const deUint32*	bufferPtr		= (deUint32*)buffer->getAllocation().getHostPtr();
	const deUint32	expectedValue	= 4;
	deUint32		failures		= 0;
	deUint32		pos				= 0;

	for (deUint32 y = 0; y < m_data.height; ++y)
	for (deUint32 x = 0; x < m_data.width; ++x)
	{
		if (bufferPtr[pos] != expectedValue)
			failures++;

		++pos;
	}

	return failures;
}

tcu::TestStatus RayTracingBuildTestInstance::iterate (void)
{
	DeviceHelper					deviceHelper	(m_context);
	de::MovePtr<BufferWithMemory>	buffer			= runTest(deviceHelper);
	const deUint32					failures		= validateBuffer(buffer);

	if (failures == 0)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("failures=" + de::toString(failures));
}

}	// anonymous

tcu::TestCaseGroup*	createNullAccelerationStructureTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "null_as", "Null Acceleration Structure is accepted as 'always miss' case"));

	const CaseDef	caseDef					=
	{
		8,		//  deUint32	width;
		8,		//  deUint32	height;
	};
	group->addChild(new RayTracingTestCase(testCtx, "test", "", caseDef));

	return group.release();
}

}	// RayTracing
}	// vkt
