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
 * \brief Ray Tracing Build Large Shader Set tests
 *//*--------------------------------------------------------------------*/

#include "vktRayTracingBuildIndirectTests.hpp"

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

struct CaseDef
{
	deUint32	width;
	deUint32	height;
	deUint32	depth;
	deUint32	squaresGroupCount;
	deUint32	geometriesGroupCount;
	deUint32	instancesGroupCount;
};

enum ShaderGroups
{
	FIRST_GROUP		= 0,
	RAYGEN_GROUP	= FIRST_GROUP,
	MISS_GROUP,
	HIT_GROUP,
	GROUP_COUNT
};

const deUint32 HIT				= 1;
const deUint32 MISS				= 2;
const deUint32 HIT_MISS_PATTERN	= 7;

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
							   const std::string&				shaderName)
{
	Move<VkShaderModule>	raygenShader	= createShaderModule(vkd, device, collection.get(shaderName), 0);

	rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR, raygenShader, 0);

	Move<VkPipeline> pipeline = rayTracingPipeline->createPipeline(vkd, device, pipelineLayout);

	return pipeline;
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
	Move<VkShaderModule>	hitShader			= createShaderModule(vkd, device, collection.get("chit"), 0);
	Move<VkShaderModule>	missShader			= createShaderModule(vkd, device, collection.get("miss"), 0);

	rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR,		raygenShader,		raygenGroup);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,	hitShader,			hitGroup);
	rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR,			missShader,			missGroup);

	Move<VkPipeline> pipeline = rayTracingPipeline->createPipeline(vkd, device, pipelineLayout);

	return pipeline;
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

class RayTracingBuildIndirectTestInstance : public TestInstance
{
public:
													RayTracingBuildIndirectTestInstance		(Context& context, const CaseDef& data);
													~RayTracingBuildIndirectTestInstance	(void);
	tcu::TestStatus									iterate									(void);

protected:
	void											checkSupportInInstance					(void) const;
	de::MovePtr<BufferWithMemory>					prepareBuffer							(VkDeviceSize										bufferSizeBytes,
																							 const std::string&									shaderName);
	de::MovePtr<BufferWithMemory>					runTest									(const VkBuffer										indirectBottomAccelerationStructure,
																							 const VkBuffer										indirectTopAccelerationStructure);
	de::SharedPtr<TopLevelAccelerationStructure>	initTopAccelerationStructure			(VkCommandBuffer									cmdBuffer,
																							 de::SharedPtr<BottomLevelAccelerationStructure>&	bottomLevelAccelerationStructure,
																							 const VkBuffer										indirectBuffer,
																							 const VkDeviceSize									indirectBufferOffset,
																							 const deUint32										indirectBufferStride);
	de::SharedPtr<BottomLevelAccelerationStructure>	initBottomAccelerationStructure			(VkCommandBuffer									cmdBuffer,
																							 const VkBuffer										indirectBuffer,
																							 const VkDeviceSize									indirectBufferOffset,
																							 const deUint32										indirectBufferStride);
	VkBuffer										initIndirectTopAccelerationStructure	(void);
	VkBuffer										initIndirectBottomAccelerationStructure	(void);

private:
	CaseDef											m_data;
	de::MovePtr<BufferWithMemory>					m_indirectAccelerationStructureBottom;
	de::MovePtr<BufferWithMemory>					m_indirectAccelerationStructureTop;
};

RayTracingBuildIndirectTestInstance::RayTracingBuildIndirectTestInstance (Context& context, const CaseDef& data)
	: vkt::TestInstance						(context)
	, m_data								(data)
	, m_indirectAccelerationStructureBottom	()
	, m_indirectAccelerationStructureTop	()
{
}

RayTracingBuildIndirectTestInstance::~RayTracingBuildIndirectTestInstance (void)
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
	DE_ASSERT((m_data.width * m_data.height * m_data.depth) == (m_data.squaresGroupCount * m_data.geometriesGroupCount * m_data.instancesGroupCount));
}

RayTracingTestCase::~RayTracingTestCase	(void)
{
}

void RayTracingTestCase::checkSupport(Context& context) const
{
	const VkPhysicalDeviceAccelerationStructureFeaturesKHR&	accelerationStructureFeaturesKHR = context.getAccelerationStructureFeatures();
	if (accelerationStructureFeaturesKHR.accelerationStructure == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

	const VkPhysicalDeviceRayTracingPipelineFeaturesKHR&	rayTracingPipelineFeaturesKHR = context.getRayTracingPipelineFeatures();
	if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

	if (accelerationStructureFeaturesKHR.accelerationStructureIndirectBuild == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructureIndirectBuild");
}

void RayTracingTestCase::initPrograms (SourceCollections& programCollection) const
{
	const vk::ShaderBuildOptions	buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(set = 0, binding = 0, std140) writeonly buffer OutBuf\n"
			"{\n"
			"  uvec4 accelerationStructureBuildOffsetInfoKHR[" << m_data.depth << "];\n"
			"} b_out;\n"
			"\n"
			"void main()\n"
			"{\n"
			"  for (uint i = 0; i < " << m_data.depth << "; i++)\n"
			"  {\n"
			"    uint primitiveCount  = " << m_data.width * m_data.height << "u;\n"
			"    uint primitiveOffset = " << m_data.width * m_data.height * 3u * sizeof(tcu::Vec3) << "u * i;\n"
			"    uint firstVertex     = " << 0 << "u;\n"
			"    uint transformOffset = " << 0 << "u;\n"
			"\n"
			"    b_out.accelerationStructureBuildOffsetInfoKHR[i] = uvec4(primitiveCount, primitiveOffset, firstVertex, transformOffset);\n"
			"  }\n"
			"}\n";

		programCollection.glslSources.add("wr-asb") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(set = 0, binding = 0, std140) writeonly buffer OutBuf\n"
			"{\n"
			"  uvec4 accelerationStructureBuildOffsetInfoKHR;\n"
			"} b_out;\n"
			"\n"
			"void main()\n"
			"{\n"
			"  uint primitiveCount  = " << m_data.instancesGroupCount << "u;\n"
			"  uint primitiveOffset = " << 0 << "u;\n"
			"  uint firstVertex     = " << 0 << "u;\n"
			"  uint transformOffset = " << 0 << "u;\n"
			"\n"
			"  b_out.accelerationStructureBuildOffsetInfoKHR = uvec4(primitiveCount, primitiveOffset, firstVertex, transformOffset);\n"
			"}\n";

		programCollection.glslSources.add("wr-ast") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadEXT vec3 hitValue;\n"
			"layout(set = 0, binding = 1) uniform accelerationStructureEXT topLevelAS;\n"
			"\n"
			"void main()\n"
			"{\n"
			"  uint  rayFlags = 0;\n"
			"  uint  cullMask = 0xFF;\n"
			"  float tmin     = 0.0;\n"
			"  float tmax     = 9.0;\n"
			"  float x        = (float(gl_LaunchIDEXT.x) + 0.5f) / float(gl_LaunchSizeEXT.x);\n"
			"  float y        = (float(gl_LaunchIDEXT.y) + 0.5f) / float(gl_LaunchSizeEXT.y);\n"
			"  float z        = (float(gl_LaunchIDEXT.z) + 0.5f) / float(gl_LaunchSizeEXT.z);\n"
			"  vec3  origin   = vec3(x, y, z);\n"
			"  vec3  direct   = vec3(0.0, 0.0, -1.0);\n"
			"  traceRayEXT(topLevelAS, rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
			"}\n";

		programCollection.glslSources.add("rgen") << glu::RaygenSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
			"hitAttributeEXT vec3 attribs;\n"
			"layout(set = 0, binding = 0, r32ui) uniform uimage3D result;\n"
			"void main()\n"
			"{\n"
			"  uvec4 color = uvec4(" << HIT << ",0,0,1);\n"
			"  imageStore(result, ivec3(gl_LaunchIDEXT.xyz), color);\n"
			"}\n";

		programCollection.glslSources.add("chit") << glu::ClosestHitSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}

	{
		std::stringstream css;
		css <<
			"#version 460 core\n"
			"#extension GL_EXT_ray_tracing : require\n"
			"layout(location = 0) rayPayloadInEXT dummyPayload { vec4 dummy; };\n"
			"layout(set = 0, binding = 0, r32ui) uniform uimage3D result;\n"
			"void main()\n"
			"{\n"
			"  uvec4 color = uvec4(" << MISS << ",0,0,1);\n"
			"  imageStore(result, ivec3(gl_LaunchIDEXT.xyz), color);\n"
			"}\n";

		programCollection.glslSources.add("miss") << glu::MissSource(updateRayTracingGLSL(css.str())) << buildOptions;
	}
}

TestInstance* RayTracingTestCase::createInstance (Context& context) const
{
	return new RayTracingBuildIndirectTestInstance(context, m_data);
}

de::SharedPtr<TopLevelAccelerationStructure> RayTracingBuildIndirectTestInstance::initTopAccelerationStructure (VkCommandBuffer										cmdBuffer,
																												de::SharedPtr<BottomLevelAccelerationStructure>&	bottomLevelAccelerationStructure,
																												const VkBuffer										indirectBuffer,
																												const VkDeviceSize									indirectBufferOffset,
																												const deUint32										indirectBufferStride)
{
	const DeviceInterface&						vkd			= m_context.getDeviceInterface();
	const VkDevice								device		= m_context.getDevice();
	Allocator&									allocator	= m_context.getDefaultAllocator();
	de::MovePtr<TopLevelAccelerationStructure>	result		= makeTopLevelAccelerationStructure();

	result->setInstanceCount(1);
	result->addInstance(bottomLevelAccelerationStructure);
	result->setIndirectBuildParameters(indirectBuffer, indirectBufferOffset, indirectBufferStride);

	result->createAndBuild(vkd, device, cmdBuffer, allocator);

	return de::SharedPtr<TopLevelAccelerationStructure>(result.release());
}

de::SharedPtr<BottomLevelAccelerationStructure> RayTracingBuildIndirectTestInstance::initBottomAccelerationStructure (VkCommandBuffer		cmdBuffer,
																													  const VkBuffer		indirectBuffer,
																													  const VkDeviceSize	indirectBufferOffset,
																													  const deUint32		indirectBufferStride)
{
	const DeviceInterface&							vkd			= m_context.getDeviceInterface();
	const VkDevice									device		= m_context.getDevice();
	Allocator&										allocator	= m_context.getDefaultAllocator();
	de::MovePtr<BottomLevelAccelerationStructure>	result		= makeBottomLevelAccelerationStructure();

	result->setGeometryCount(m_data.geometriesGroupCount);
	result->setIndirectBuildParameters(indirectBuffer, indirectBufferOffset, indirectBufferStride);

	for (size_t geometryNdx = 0; geometryNdx < m_data.geometriesGroupCount; ++geometryNdx)
	{
		std::vector<tcu::Vec3>	geometryData;

		geometryData.reserve(m_data.squaresGroupCount * 3u);

		tcu::UVec2	startPos	= tcu::UVec2(0u, 0u);

		for (size_t squareNdx = 0; squareNdx < m_data.squaresGroupCount; ++squareNdx)
		{
			const deUint32	n	= m_data.width * startPos.y() + startPos.x();
			const float		x0	= float(startPos.x() + 0) / float(m_data.width);
			const float		y0	= float(startPos.y() + 0) / float(m_data.height);
			const float		x1	= float(startPos.x() + 1) / float(m_data.width);
			const float		y1	= float(startPos.y() + 1) / float(m_data.height);
			const float		xm	= (x0 + x1) / 2.0f;
			const float		ym	= (y0 + y1) / 2.0f;
			const float		z	= (n % HIT_MISS_PATTERN == 0) ? +1.0f : (float(geometryNdx) + 0.25f) / float(m_data.geometriesGroupCount);

			geometryData.push_back(tcu::Vec3(x0, y0, z));
			geometryData.push_back(tcu::Vec3(xm, y1, z));
			geometryData.push_back(tcu::Vec3(x1, ym, z));

			startPos.y() = (n + 1) / m_data.width;
			startPos.x() = (n + 1) % m_data.width;
		}

		result->addGeometry(geometryData, true);
	}

	result->createAndBuild(vkd, device, cmdBuffer, allocator);

	return de::SharedPtr<BottomLevelAccelerationStructure>(result.release());
}

de::MovePtr<BufferWithMemory> RayTracingBuildIndirectTestInstance::prepareBuffer (VkDeviceSize			bufferSizeBytes,
																				  const std::string&	shaderName)
{
	const InstanceInterface&			vki									= m_context.getInstanceInterface();
	const DeviceInterface&				vkd									= m_context.getDeviceInterface();
	const VkDevice						device								= m_context.getDevice();
	const VkPhysicalDevice				physicalDevice						= m_context.getPhysicalDevice();
	const deUint32						queueFamilyIndex					= m_context.getUniversalQueueFamilyIndex();
	const VkQueue						queue								= m_context.getUniversalQueue();
	Allocator&							allocator							= m_context.getDefaultAllocator();
	const deUint32						shaderGroupHandleSize				= getShaderGroupSize(vki, physicalDevice);
	const deUint32						shaderGroupBaseAlignment			= getShaderGroupBaseAlignment(vki, physicalDevice);

	const VkBufferCreateInfo			bufferCreateInfo					= makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	de::MovePtr<BufferWithMemory>		buffer								= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress));

	const Move<VkDescriptorSetLayout>	descriptorSetLayout					= DescriptorSetLayoutBuilder()
																					.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ALL_RAY_TRACING_STAGES)
																					.build(vkd, device);
	const Move<VkDescriptorPool>		descriptorPool						= DescriptorPoolBuilder()
																					.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
																					.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const Move<VkDescriptorSet>			descriptorSet						= makeDescriptorSet(vkd, device, *descriptorPool, *descriptorSetLayout);
	const Move<VkPipelineLayout>		pipelineLayout						= makePipelineLayout(vkd, device, descriptorSetLayout.get());
	const Move<VkCommandPool>			cmdPool								= createCommandPool(vkd, device, 0, queueFamilyIndex);
	const Move<VkCommandBuffer>			cmdBuffer							= allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	const vk::VkDescriptorBufferInfo	descriptorBufferInfo				= makeDescriptorBufferInfo(**buffer, 0ull, bufferSizeBytes);

	de::MovePtr<RayTracingPipeline>		rayTracingPipeline					= de::newMovePtr<RayTracingPipeline>();
	const Move<VkPipeline>				pipeline							= makePipeline(vkd, device, m_context.getBinaryCollection(), rayTracingPipeline, *pipelineLayout, shaderName);
	const de::MovePtr<BufferWithMemory>	shaderBindingTable					= rayTracingPipeline->createShaderBindingTable(vkd, device, *pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, 0, 1);
	const VkStridedDeviceAddressRegionKHR	raygenShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(vkd, device, shaderBindingTable->get(), 0), shaderGroupHandleSize, shaderGroupHandleSize);
	const VkStridedDeviceAddressRegionKHR	missShaderBindingTableRegion		= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	const VkStridedDeviceAddressRegionKHR	hitShaderBindingTableRegion			= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	const VkStridedDeviceAddressRegionKHR	callableShaderBindingTableRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

	beginCommandBuffer(vkd, *cmdBuffer, 0u);
	{
		DescriptorSetUpdateBuilder()
			.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorBufferInfo)
			.update(vkd, device);

		vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipelineLayout, 0, 1, &descriptorSet.get(), 0, DE_NULL);

		vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipeline);

		cmdTraceRays(vkd,
			*cmdBuffer,
			&raygenShaderBindingTableRegion,
			&missShaderBindingTableRegion,
			&hitShaderBindingTableRegion,
			&callableShaderBindingTableRegion,
			1u, 1u, 1u);
	}
	endCommandBuffer(vkd, *cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

	return buffer;
}


de::MovePtr<BufferWithMemory> RayTracingBuildIndirectTestInstance::runTest (const VkBuffer	indirectBottomAccelerationStructure,
																			const VkBuffer	indirectTopAccelerationStructure)
{
	const InstanceInterface&			vki									= m_context.getInstanceInterface();
	const DeviceInterface&				vkd									= m_context.getDeviceInterface();
	const VkDevice						device								= m_context.getDevice();
	const VkPhysicalDevice				physicalDevice						= m_context.getPhysicalDevice();
	const deUint32						queueFamilyIndex					= m_context.getUniversalQueueFamilyIndex();
	const VkQueue						queue								= m_context.getUniversalQueue();
	Allocator&							allocator							= m_context.getDefaultAllocator();
	const VkFormat						format								= VK_FORMAT_R32_UINT;
	const deUint32						pixelCount							= m_data.width * m_data.height * m_data.depth;
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

	const VkImageCreateInfo				imageCreateInfo						= makeImageCreateInfo(m_data.width, m_data.height, m_data.depth, format);
	const VkImageSubresourceRange		imageSubresourceRange				= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0, 1u);
	const de::MovePtr<ImageWithMemory>	image								= de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
	const Move<VkImageView>				imageView							= makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_3D, format, imageSubresourceRange);

	const VkBufferCreateInfo			bufferCreateInfo					= makeBufferCreateInfo(pixelCount * sizeof(deUint32), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkImageSubresourceLayers		bufferImageSubresourceLayers		= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const VkBufferImageCopy				bufferImageRegion					= makeBufferImageCopy(makeExtent3D(m_data.width, m_data.height, m_data.depth), bufferImageSubresourceLayers);
	de::MovePtr<BufferWithMemory>		buffer								= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));

	const VkDescriptorImageInfo			descriptorImageInfo					= makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);

	const VkImageMemoryBarrier			preImageBarrier						= makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT,
																				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
																				**image, imageSubresourceRange);
	const VkImageMemoryBarrier			postImageBarrier					= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
																				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
																				**image, imageSubresourceRange);
	const VkMemoryBarrier				postTraceMemoryBarrier				= makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
	const VkMemoryBarrier				postCopyMemoryBarrier				= makeMemoryBarrier(VK_ACCESS_TRANSFER_READ_BIT, 0);
	const VkClearValue					clearValue							= makeClearValueColorU32(5u, 5u, 5u, 255u);
	const deUint32						indirectAccelerationStructureStride	= sizeof(VkAccelerationStructureBuildRangeInfoKHR);

	de::SharedPtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure;
	de::SharedPtr<TopLevelAccelerationStructure>	topLevelAccelerationStructure;

	beginCommandBuffer(vkd, *cmdBuffer, 0u);
	{
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preImageBarrier);
		vkd.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1, &imageSubresourceRange);
		cmdPipelineImageMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, &postImageBarrier);

		bottomLevelAccelerationStructure	= initBottomAccelerationStructure(*cmdBuffer, indirectBottomAccelerationStructure, 0, indirectAccelerationStructureStride);
		topLevelAccelerationStructure		= initTopAccelerationStructure(*cmdBuffer, bottomLevelAccelerationStructure, indirectTopAccelerationStructure, 0, indirectAccelerationStructureStride);

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
			m_data.width, m_data.height, m_data.depth);

		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, &postTraceMemoryBarrier);

		vkd.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **buffer, 1u, &bufferImageRegion);

		cmdPipelineMemoryBarrier(vkd, *cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &postCopyMemoryBarrier);
	}
	endCommandBuffer(vkd, *cmdBuffer);

	submitCommandsAndWait(vkd, device, queue, cmdBuffer.get());

	invalidateMappedMemoryRange(vkd, device, buffer->getAllocation().getMemory(), buffer->getAllocation().getOffset(), pixelCount * sizeof(deUint32));

	return buffer;
}

void RayTracingBuildIndirectTestInstance::checkSupportInInstance (void) const
{
	const InstanceInterface&			vki						= m_context.getInstanceInterface();
	const VkPhysicalDevice				physicalDevice			= m_context.getPhysicalDevice();
	de::MovePtr<RayTracingProperties>	rayTracingProperties	= makeRayTracingProperties(vki, physicalDevice);

	if (rayTracingProperties->getMaxPrimitiveCount() < m_data.squaresGroupCount)
		TCU_THROW(NotSupportedError, "Triangles required more than supported");

	if (rayTracingProperties->getMaxGeometryCount() < m_data.geometriesGroupCount)
		TCU_THROW(NotSupportedError, "Geometries required more than supported");

	if (rayTracingProperties->getMaxInstanceCount() < m_data.instancesGroupCount)
		TCU_THROW(NotSupportedError, "Instances required more than supported");
}

VkBuffer	RayTracingBuildIndirectTestInstance::initIndirectTopAccelerationStructure (void)
{
	VkBuffer result	= DE_NULL;

	m_indirectAccelerationStructureTop	= prepareBuffer(sizeof(VkAccelerationStructureBuildRangeInfoKHR), "wr-ast");
	result								= **m_indirectAccelerationStructureTop;

	return result;
}

VkBuffer	RayTracingBuildIndirectTestInstance::initIndirectBottomAccelerationStructure (void)
{
	VkBuffer result	= DE_NULL;

	m_indirectAccelerationStructureBottom	= prepareBuffer(sizeof(VkAccelerationStructureBuildRangeInfoKHR) * m_data.geometriesGroupCount, "wr-asb");
	result									= **m_indirectAccelerationStructureBottom;

	return result;
}

tcu::TestStatus RayTracingBuildIndirectTestInstance::iterate (void)
{
	checkSupportInInstance();

	const VkBuffer						indirectAccelerationStructureBottom	= initIndirectBottomAccelerationStructure();
	const VkBuffer						indirectAccelerationStructureTop	= initIndirectTopAccelerationStructure();
	const de::MovePtr<BufferWithMemory>	buffer								= runTest(indirectAccelerationStructureBottom, indirectAccelerationStructureTop);
	const deUint32*						bufferPtr							= (deUint32*)buffer->getAllocation().getHostPtr();
	deUint32							failures							= 0;

	for (deUint32 z = 0; z < m_data.depth; ++z)
	{
		const deUint32*	bufferPtrLevel	= &bufferPtr[z * m_data.height * m_data.width];

		for (deUint32 y = 0; y < m_data.height; ++y)
		for (deUint32 x = 0; x < m_data.width; ++x)
		{
			const deUint32	n				= m_data.width * y + x;
			const deUint32	expectedValue	= (n % HIT_MISS_PATTERN == 0) ? MISS : HIT;

			if (bufferPtrLevel[n] != expectedValue)
				failures++;
		}
	}

	if (failures == 0)
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("failures=" + de::toString(failures));
}

}	// anonymous

tcu::TestCaseGroup*	createBuildIndirectTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "indirect", "Build acceleration structure indirect ray tracing tests"));

	const deUint32	width					= 512u;
	const deUint32	height					= 128u;
	const deUint32	depth					= 4u;
	const deUint32	largestGroup			= width * height;
	const deUint32	squaresGroupCount		= largestGroup;
	const deUint32	geometriesGroupCount	= depth;
	const deUint32	instancesGroupCount		= 1;
	const CaseDef	caseDef					=
	{
		width,
		height,
		depth,
		squaresGroupCount,
		geometriesGroupCount,
		instancesGroupCount,
	};
	const std::string	testName			= "build_structure";

	group->addChild(new RayTracingTestCase(testCtx, testName.c_str(), "", caseDef));

	return group.release();
}

}	// RayTracing
}	// vkt
