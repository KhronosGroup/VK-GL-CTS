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
 * \brief Basic cmdTraceRays* tests.
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingTraceRaysTests.hpp"

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

constexpr deUint32		kClearColorValue	= 0xFFu;
constexpr deUint32		kHitColorValue		= 2u;
constexpr deUint32		kMissColorValue		= 1u;

enum class TraceType
{
	DIRECT			= 0,
	INDIRECT_CPU	= 1,
	INDIRECT_GPU	= 2,
};

struct TestParams
{
	TraceType						traceType;
	VkTraceRaysIndirectCommandKHR	traceDimensions;	// Note: to be used for both direct and indirect variants.
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

bool isNullTrace (const VkTraceRaysIndirectCommandKHR& cmd)
{
	return (cmd.width == 0u || cmd.height == 0u || cmd.depth == 0u);
}

VkExtent3D getImageExtent (const VkTraceRaysIndirectCommandKHR& cmd)
{
	return (isNullTrace(cmd) ? makeExtent3D(8u, 8u, 1u) : makeExtent3D(cmd.width, cmd.height, cmd.depth));
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

class RayTracingTraceRaysIndirectTestCase : public TestCase
{
	public:
							RayTracingTraceRaysIndirectTestCase		(tcu::TestContext& context, const char* name, const char* desc, const TestParams data);
							~RayTracingTraceRaysIndirectTestCase	(void);

	virtual void			checkSupport								(Context& context) const;
	virtual	void			initPrograms								(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance								(Context& context) const;
private:
	TestParams				m_data;
};

class RayTracingTraceRaysIndirectTestInstance : public TestInstance
{
public:
																	RayTracingTraceRaysIndirectTestInstance			(Context& context, const TestParams& data);
																	~RayTracingTraceRaysIndirectTestInstance		(void);
	tcu::TestStatus													iterate								(void);

protected:
	std::vector<de::SharedPtr<BottomLevelAccelerationStructure>>	initBottomAccelerationStructures	(VkCommandBuffer												cmdBuffer);
	de::MovePtr<TopLevelAccelerationStructure>						initTopAccelerationStructure		(VkCommandBuffer												cmdBuffer,
																										 std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >&	bottomLevelAccelerationStructures);
	de::MovePtr<BufferWithMemory>									runTest								();
private:
	TestParams														m_data;
	const VkExtent3D												m_imageExtent;
};


RayTracingTraceRaysIndirectTestCase::RayTracingTraceRaysIndirectTestCase (tcu::TestContext& context, const char* name, const char* desc, const TestParams data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

RayTracingTraceRaysIndirectTestCase::~RayTracingTraceRaysIndirectTestCase	(void)
{
}

void RayTracingTraceRaysIndirectTestCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");
	context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

	const VkPhysicalDeviceRayTracingPipelineFeaturesKHR&	rayTracingPipelineFeaturesKHR		= context.getRayTracingPipelineFeatures();
	if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == DE_FALSE )
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

	if (rayTracingPipelineFeaturesKHR.rayTracingPipelineTraceRaysIndirect == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipelineTraceRaysIndirect");

	const VkPhysicalDeviceAccelerationStructureFeaturesKHR&	accelerationStructureFeaturesKHR	= context.getAccelerationStructureFeatures();
	if (accelerationStructureFeaturesKHR.accelerationStructure == DE_FALSE)
		TCU_THROW(TestError, "VK_KHR_ray_tracing_pipeline requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");
}

void RayTracingTraceRaysIndirectTestCase::initPrograms (SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions	buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"struct TraceRaysIndirectCommand\n"
			"{\n"
			"	uint width;\n"
			"	uint height;\n"
			"	uint depth;\n"
			"};\n"
			"layout(binding = 0) uniform IndirectCommandsUBO\n"
			"{\n"
			"	TraceRaysIndirectCommand indirectCommands;\n"
			"} ubo;\n"
			"layout(binding = 1) buffer IndirectCommandsSBO\n"
			"{\n"
			"	TraceRaysIndirectCommand indirectCommands;\n"
			"};\n"
			"void main()\n"
			"{\n"
			"  indirectCommands.width  = ubo.indirectCommands.width;\n"
			"  indirectCommands.height = ubo.indirectCommands.height;\n"
			"  indirectCommands.depth  = ubo.indirectCommands.depth;\n"
			"}\n";

		programCollection.glslSources.add("compute_indirect_command") << glu::ComputeSource(css.str()) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadEXT uvec4 hitValue;\n"
			"layout(r32ui, set = 0, binding = 0) uniform uimage3D result;\n"
			"layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
			"\n"
			"void main()\n"
			"{\n"
			"  float tmin     = 0.0;\n"
			"  float tmax     = 1.0;\n"
			"  vec3  origin   = vec3(float(gl_LaunchIDEXT.x) + 0.5f, float(gl_LaunchIDEXT.y) + 0.5f, float(gl_LaunchIDEXT.z + 0.5f));\n"
			"  vec3  direct   = vec3(0.0, 0.0, -1.0);\n"
			"  hitValue       = uvec4(0,0,0,0);\n"
			"  traceRayEXT(topLevelAS, 0, 0xFF, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
			"  imageStore(result, ivec3(gl_LaunchIDEXT), hitValue);\n"
			"}\n";
		programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
			"void main()\n"
			"{\n"
			"  hitValue = uvec4(" << kHitColorValue << ",0,0,1);\n"
			"}\n";
		programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT uvec4 hitValue;\n"
			"void main()\n"
			"{\n"
			"  hitValue = uvec4(" << kMissColorValue << ",0,0,1);\n"
			"}\n";

		programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}
}

TestInstance* RayTracingTraceRaysIndirectTestCase::createInstance (Context& context) const
{
	return new RayTracingTraceRaysIndirectTestInstance(context, m_data);
}

RayTracingTraceRaysIndirectTestInstance::RayTracingTraceRaysIndirectTestInstance (Context& context, const TestParams& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
	, m_imageExtent			(getImageExtent(data.traceDimensions))
{
}

RayTracingTraceRaysIndirectTestInstance::~RayTracingTraceRaysIndirectTestInstance (void)
{
}

std::vector<de::SharedPtr<BottomLevelAccelerationStructure> > RayTracingTraceRaysIndirectTestInstance::initBottomAccelerationStructures (VkCommandBuffer cmdBuffer)
{
	const DeviceInterface&											vkd			= m_context.getDeviceInterface();
	const VkDevice													device		= m_context.getDevice();
	Allocator&														allocator	= m_context.getDefaultAllocator();
	std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >	result;

	tcu::Vec3 v0(0.0, 1.0, 0.0);
	tcu::Vec3 v1(0.0, 0.0, 0.0);
	tcu::Vec3 v2(1.0, 1.0, 0.0);
	tcu::Vec3 v3(1.0, 0.0, 0.0);

	for (deUint32 z = 0; z < m_imageExtent.depth; ++z)
	for (deUint32 y = 0; y < m_imageExtent.height; ++y)
	for (deUint32 x = 0; x < m_imageExtent.width; ++x)
	{
		// let's build a 3D chessboard of geometries
		if (((x + y + z) % 2) == 0)
			continue;
		tcu::Vec3 xyz((float)x, (float)y, (float)z);
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
		bottomLevelAccelerationStructure->createAndBuild(vkd, device, cmdBuffer, allocator);
		result.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release()));
	}

	return result;
}

de::MovePtr<TopLevelAccelerationStructure> RayTracingTraceRaysIndirectTestInstance::initTopAccelerationStructure (VkCommandBuffer													cmdBuffer,
																													  std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >&	bottomLevelAccelerationStructures)
{
	const DeviceInterface&						vkd				= m_context.getDeviceInterface();
	const VkDevice								device			= m_context.getDevice();
	Allocator&									allocator		= m_context.getDefaultAllocator();
	const deUint32								instanceCount	= m_imageExtent.depth * m_imageExtent.height * m_imageExtent.width / 2;

	de::MovePtr<TopLevelAccelerationStructure>	result = makeTopLevelAccelerationStructure();
	result->setInstanceCount(instanceCount);

	deUint32 currentInstanceIndex = 0;

	for (deUint32 z = 0; z < m_imageExtent.depth; ++z)
	for (deUint32 y = 0; y < m_imageExtent.height; ++y)
	for (deUint32 x = 0; x < m_imageExtent.width; ++x)
	{
		if (((x + y + z) % 2) == 0)
			continue;
		result->addInstance(bottomLevelAccelerationStructures[currentInstanceIndex++]);
	}
	result->createAndBuild(vkd, device, cmdBuffer, allocator);

	return result;
}

de::MovePtr<BufferWithMemory> RayTracingTraceRaysIndirectTestInstance::runTest()
{
	const InstanceInterface&			vki									= m_context.getInstanceInterface();
	const DeviceInterface&				vkd									= m_context.getDeviceInterface();
	const VkDevice						device								= m_context.getDevice();
	const VkPhysicalDevice				physicalDevice						= m_context.getPhysicalDevice();
	const deUint32						queueFamilyIndex					= m_context.getUniversalQueueFamilyIndex();
	const VkQueue						queue								= m_context.getUniversalQueue();
	Allocator&							allocator							= m_context.getDefaultAllocator();
	const deUint32						pixelCount							= m_imageExtent.depth * m_imageExtent.height * m_imageExtent.width;
	const deUint32						shaderGroupHandleSize				= getShaderGroupSize(vki, physicalDevice);
	const deUint32						shaderGroupBaseAlignment			= getShaderGroupBaseAlignment(vki, physicalDevice);

	Move<VkDescriptorSetLayout>			computeDescriptorSetLayout;
	Move<VkDescriptorPool>				computeDescriptorPool;
	Move<VkDescriptorSet>				computeDescriptorSet;
	Move<VkPipelineLayout>				computePipelineLayout;
	Move<VkShaderModule>				computeShader;
	Move<VkPipeline>					computePipeline;

	if (m_data.traceType == TraceType::INDIRECT_GPU)
	{
		computeDescriptorSetLayout			= DescriptorSetLayoutBuilder()
													.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
													.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
													.build(vkd, device);
		computeDescriptorPool				= DescriptorPoolBuilder()
													.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
													.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
													.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
		computeDescriptorSet				= makeDescriptorSet(vkd, device, *computeDescriptorPool, *computeDescriptorSetLayout);
		computePipelineLayout				= makePipelineLayout(vkd, device, computeDescriptorSetLayout.get());

		computeShader						= createShaderModule(vkd, device, m_context.getBinaryCollection().get("compute_indirect_command"), 0);
		const VkPipelineShaderStageCreateInfo pipelineShaderStageParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
			DE_NULL,												// const void*							pNext;
			VkPipelineShaderStageCreateFlags(0u),					// VkPipelineShaderStageCreateFlags		flags;
			VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits				stage;
			*computeShader,											// VkShaderModule						module;
			"main",													// const char*							pName;
			DE_NULL,												// const VkSpecializationInfo*			pSpecializationInfo;
		};
		const VkComputePipelineCreateInfo pipelineCreateInfo =
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,		// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			VkPipelineCreateFlags(0u),							// VkPipelineCreateFlags			flags;
			pipelineShaderStageParams,							// VkPipelineShaderStageCreateInfo	stage;
			*computePipelineLayout,								// VkPipelineLayout					layout;
			DE_NULL,											// VkPipeline						basePipelineHandle;
			0,													// deInt32							basePipelineIndex;
		};

		computePipeline = vk::createComputePipeline(vkd, device, (VkPipelineCache)0u, &pipelineCreateInfo);
	}

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
	rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,		createShaderModule(vkd, device, m_context.getBinaryCollection().get("rgen"), 0), 0);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	createShaderModule(vkd, device, m_context.getBinaryCollection().get("chit"), 0), 1);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR,			createShaderModule(vkd, device, m_context.getBinaryCollection().get("miss"), 0), 2);
	Move<VkPipeline>					pipeline							= rayTracingPipeline->createPipeline(vkd, device, *pipelineLayout);

	const de::MovePtr<BufferWithMemory>	raygenShaderBindingTable			= rayTracingPipeline->createShaderBindingTable(vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1 );
	const de::MovePtr<BufferWithMemory>	hitShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 1, 1 );
	const de::MovePtr<BufferWithMemory>	missShaderBindingTable				= rayTracingPipeline->createShaderBindingTable(vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 2, 1 );

	const VkStridedDeviceAddressRegionKHR	raygenShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, raygenShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR	missShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, missShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR	hitShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, hitShaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR	callableShaderBindingTableRegion= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

	const VkFormat						imageFormat							= VK_FORMAT_R32_UINT;
	const VkImageCreateInfo				imageCreateInfo						= makeImageCreateInfo(m_imageExtent.width, m_imageExtent.height, m_imageExtent.depth, imageFormat);
	const VkImageSubresourceRange		imageSubresourceRange				= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0, 1u);
	const de::MovePtr<ImageWithMemory>	image								= de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
	const Move<VkImageView>				imageView							= makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_3D, imageFormat, imageSubresourceRange);

	const VkBufferCreateInfo			resultBufferCreateInfo				= makeBufferCreateInfo(pixelCount*sizeof(deUint32), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkImageSubresourceLayers		resultBufferImageSubresourceLayers	= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const VkBufferImageCopy				resultBufferImageRegion				= makeBufferImageCopy(m_imageExtent, resultBufferImageSubresourceLayers);
	de::MovePtr<BufferWithMemory>		resultBuffer						= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));

	const VkDescriptorImageInfo			descriptorImageInfo					= makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);

	// create indirect command buffer and fill it with parameter values
	de::MovePtr<BufferWithMemory>		indirectBuffer;
	de::MovePtr<BufferWithMemory>		uniformBuffer;

	if (m_data.traceType != TraceType::DIRECT)
	{
		const bool							indirectGpu							= (m_data.traceType == TraceType::INDIRECT_GPU);
		VkBufferUsageFlags					indirectBufferUsageFlags			= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | ( indirectGpu ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : VK_BUFFER_USAGE_TRANSFER_DST_BIT );
		const VkBufferCreateInfo			indirectBufferCreateInfo			= makeBufferCreateInfo(sizeof(VkTraceRaysIndirectCommandKHR), indirectBufferUsageFlags);
		vk::MemoryRequirement				indirectBufferMemoryRequirement		= MemoryRequirement::DeviceAddress | ( indirectGpu ? MemoryRequirement::Any : MemoryRequirement::HostVisible );
		indirectBuffer															= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, indirectBufferCreateInfo, indirectBufferMemoryRequirement));
	}

	if (m_data.traceType == TraceType::INDIRECT_GPU)
	{
		const VkBufferCreateInfo			uniformBufferCreateInfo			= makeBufferCreateInfo(sizeof(VkTraceRaysIndirectCommandKHR), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		uniformBuffer														= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, uniformBufferCreateInfo, MemoryRequirement::HostVisible));
		deMemcpy(uniformBuffer->getAllocation().getHostPtr(), &m_data.traceDimensions, sizeof(VkTraceRaysIndirectCommandKHR));
		flushMappedMemoryRange(vkd, device, uniformBuffer->getAllocation().getMemory(), uniformBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);
	}
	else if (m_data.traceType == TraceType::INDIRECT_CPU)
	{
		deMemcpy(indirectBuffer->getAllocation().getHostPtr(), &m_data.traceDimensions, sizeof(VkTraceRaysIndirectCommandKHR));
		flushMappedMemoryRange(vkd, device, indirectBuffer->getAllocation().getMemory(), indirectBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);
	}

	const Move<VkCommandPool>			cmdPool								= createCommandPool(vkd, device, 0, queueFamilyIndex);
	const Move<VkCommandBuffer>			cmdBuffer							= allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	std::vector<de::SharedPtr<BottomLevelAccelerationStructure> >	bottomLevelAccelerationStructures;
	de::MovePtr<TopLevelAccelerationStructure>						topLevelAccelerationStructure;

	beginCommandBuffer(vkd, *cmdBuffer, 0u);
	{
		const VkImageMemoryBarrier			preImageBarrier						= makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT,
																					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
																					**image, imageSubresourceRange);
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preImageBarrier);

		const VkClearValue					clearValue							= makeClearValueColorU32(kClearColorValue, 0u, 0u, 0u);
		vkd.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1, &imageSubresourceRange);

		const VkImageMemoryBarrier			postImageBarrier					= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
																					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
																					**image, imageSubresourceRange);
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, &postImageBarrier);

		bottomLevelAccelerationStructures	= initBottomAccelerationStructures(*cmdBuffer);
		topLevelAccelerationStructure		= initTopAccelerationStructure(*cmdBuffer, bottomLevelAccelerationStructures);

		if (m_data.traceType == TraceType::INDIRECT_GPU)
		{
			const VkDescriptorBufferInfo	uniformBufferDescriptorInfo		= makeDescriptorBufferInfo(uniformBuffer->get(), 0ull, sizeof(VkTraceRaysIndirectCommandKHR));
			const VkDescriptorBufferInfo	indirectBufferDescriptorInfo	= makeDescriptorBufferInfo(indirectBuffer->get(), 0ull, sizeof(VkTraceRaysIndirectCommandKHR));
			DescriptorSetUpdateBuilder()
				.writeSingle(*computeDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBufferDescriptorInfo)
				.writeSingle(*computeDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &indirectBufferDescriptorInfo)
				.update(vkd, device);

			vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
			vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipelineLayout, 0u, 1u, &computeDescriptorSet.get(), 0u, DE_NULL);
			vkd.cmdDispatch(*cmdBuffer, 1, 1, 1);

			const VkBufferMemoryBarrier		fillIndirectBufferMemoryBarrier	= makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
																				indirectBuffer->get(), 0ull, sizeof(VkTraceRaysIndirectCommandKHR));
			cmdPipelineBufferMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, &fillIndirectBufferMemoryBarrier);
		}

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

		// Both calls should give the same results.
		if (m_data.traceType == TraceType::DIRECT)
		{
			cmdTraceRays(vkd,
				*cmdBuffer,
				&raygenShaderBindingTableRegion,
				&missShaderBindingTableRegion,
				&hitShaderBindingTableRegion,
				&callableShaderBindingTableRegion,
				m_data.traceDimensions.width, m_data.traceDimensions.height, m_data.traceDimensions.depth);
		}
		else
		{
			cmdTraceRaysIndirect(vkd,
				*cmdBuffer,
				&raygenShaderBindingTableRegion,
				&missShaderBindingTableRegion,
				&hitShaderBindingTableRegion,
				&callableShaderBindingTableRegion,
				getBufferDeviceAddress(vkd, device, indirectBuffer->get(), 0));
		}

		const VkMemoryBarrier							postTraceMemoryBarrier					= makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);

		vkd.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **resultBuffer, 1u, &resultBufferImageRegion);
	}
	endCommandBuffer(vkd, *cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

	invalidateMappedMemoryRange(vkd, device, resultBuffer->getAllocation().getMemory(), resultBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);

	return resultBuffer;
}

tcu::TestStatus RayTracingTraceRaysIndirectTestInstance::iterate (void)
{
	// run test using arrays of pointers
	const de::MovePtr<BufferWithMemory>	buffer		= runTest();
	const deUint32*						bufferPtr	= (deUint32*)buffer->getAllocation().getHostPtr();
	const bool							noWrites	= isNullTrace(m_data.traceDimensions);

	deUint32							failures		= 0;
	deUint32							pos				= 0;

	// verify results
	for (deUint32 z = 0; z < m_imageExtent.depth; ++z)
	for (deUint32 y = 0; y < m_imageExtent.height; ++y)
	for (deUint32 x = 0; x < m_imageExtent.width; ++x)
	{
		const deUint32 expectedResult = (noWrites ? kClearColorValue : (((x + y + z) % 2) ? kHitColorValue : kMissColorValue));
		if (bufferPtr[pos] != expectedResult)
			failures++;
		++pos;
	}

	if (failures == 0)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Fail (failures=" + de::toString(failures) + ")");
}

std::string makeDimensionsName (const VkTraceRaysIndirectCommandKHR& cmd)
{
	std::ostringstream name;
	name << cmd.width << "_" << cmd.height << "_" << cmd.depth;
	return name.str();
}

}	// anonymous

tcu::TestCaseGroup*	createTraceRaysTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "trace_rays_cmds", "Tests veryfying vkCmdTraceRays* commands"));

	struct BufferSourceTypeData
	{
		TraceType								traceType;
		const char*								name;
	} bufferSourceTypes[] =
	{
		{ TraceType::DIRECT,		"direct"			},
		{ TraceType::INDIRECT_CPU,	"indirect_cpu"		},
		{ TraceType::INDIRECT_GPU,	"indirect_gpu"		},
	};

	const VkTraceRaysIndirectCommandKHR traceDimensions[] =
	{
		{  0,  0, 0 },
		{  0,  1, 1 },
		{  1,  0, 1 },
		{  1,  1, 0 },
		{  8,  1, 1 },
		{  8,  8, 1 },
		{  8,  8, 8 },
		{ 11,  1, 1 },
		{ 11, 13, 1 },
		{ 11, 13, 5 },
	};

	for (size_t bufferSourceNdx = 0; bufferSourceNdx < DE_LENGTH_OF_ARRAY(bufferSourceTypes); ++bufferSourceNdx)
	{
		de::MovePtr<tcu::TestCaseGroup> bufferSourceGroup(new tcu::TestCaseGroup(group->getTestContext(), bufferSourceTypes[bufferSourceNdx].name, ""));

		for (size_t traceDimensionsIdx = 0; traceDimensionsIdx < DE_LENGTH_OF_ARRAY(traceDimensions); ++traceDimensionsIdx)
		{
			TestParams testParams
			{
				bufferSourceTypes[bufferSourceNdx].traceType,
				traceDimensions[traceDimensionsIdx],
			};
			const auto testName = makeDimensionsName(traceDimensions[traceDimensionsIdx]);
			bufferSourceGroup->addChild(new RayTracingTraceRaysIndirectTestCase(group->getTestContext(), testName.c_str(), "", testParams));
		}

		group->addChild(bufferSourceGroup.release());
	}

	return group.release();
}

}	// RayTracing

}	// vkt
