/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 * \brief Acceleration Structure binding tests
 *//*--------------------------------------------------------------------*/

#include "vktBindingDescriptorUpdateASTests.hpp"

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
#include "vkImageUtil.hpp"
#include "deRandom.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuCommandLine.hpp"

#include "vkRayTracingUtil.hpp"

namespace vkt
{
namespace BindingModel
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

enum TestType
{
	TEST_TYPE_USING_RAY_QUERY		= 0,
	TEST_TYPE_USING_RAY_TRACING,
};

enum UpdateMethod
{
	UPDATE_METHOD_NORMAL = 0,			//!< use vkUpdateDescriptorSets				vkUpdateDescriptorSets
	UPDATE_METHOD_WITH_TEMPLATE,		//!< use descriptor update templates		vkUpdateDescriptorSetWithTemplate
	UPDATE_METHOD_WITH_PUSH,			//!< use push descriptor updates			vkCmdPushDescriptorSetKHR
	UPDATE_METHOD_WITH_PUSH_TEMPLATE,	//!< use push descriptor update templates	vkCmdPushDescriptorSetWithTemplateKHR

	UPDATE_METHOD_LAST
};

const deUint32	TEST_WIDTH			= 16u;
const deUint32	TEST_HEIGHT			= 16u;
const deUint32	FIXED_POINT_DIVISOR	= 1024 * 1024;
const float		PLAIN_Z0			= 2.0f;
const float		PLAIN_Z1			= 4.0f;

struct TestParams;

typedef void (*CheckSupportFunc)(Context& context, const TestParams& testParams);
typedef void (*InitProgramsFunc)(SourceCollections& programCollection, const TestParams& testParams);
typedef const std::string (*ShaderBodyTextFunc)(const TestParams& testParams);

struct TestParams
{
	deUint32				width;
	deUint32				height;
	deUint32				depth;
	TestType				testType;
	UpdateMethod			updateMethod;
	VkShaderStageFlagBits	stage;
	VkFormat				format;
	CheckSupportFunc		pipelineCheckSupport;
	InitProgramsFunc		pipelineInitPrograms;
	ShaderBodyTextFunc		testConfigShaderBodyText;
};


static deUint32 getShaderGroupHandleSize (const InstanceInterface&	vki,
										  const VkPhysicalDevice	physicalDevice)
{
	de::MovePtr<RayTracingProperties>	rayTracingPropertiesKHR;

	rayTracingPropertiesKHR	= makeRayTracingProperties(vki, physicalDevice);

	return rayTracingPropertiesKHR->getShaderGroupHandleSize();
}

static deUint32 getShaderGroupBaseAlignment (const InstanceInterface&	vki,
											 const VkPhysicalDevice		physicalDevice)
{
	de::MovePtr<RayTracingProperties>	rayTracingPropertiesKHR;

	rayTracingPropertiesKHR = makeRayTracingProperties(vki, physicalDevice);

	return rayTracingPropertiesKHR->getShaderGroupBaseAlignment();
}

static VkBuffer getVkBuffer (const de::MovePtr<BufferWithMemory>& buffer)
{
	VkBuffer result = (buffer.get() == DE_NULL) ? DE_NULL : buffer->get();

	return result;
}

static VkStridedDeviceAddressRegionKHR makeStridedDeviceAddressRegion (const DeviceInterface& vkd, const VkDevice device, VkBuffer buffer, deUint32 stride, deUint32 count)
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

static Move<VkPipelineLayout> makePipelineLayout (const DeviceInterface&		vk,
												  const VkDevice				device,
												  const VkDescriptorSetLayout	descriptorSetLayout0,
												  const VkDescriptorSetLayout	descriptorSetLayout1,
												  const VkDescriptorSetLayout	descriptorSetLayoutOpt = DE_NULL)
{
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts;

	descriptorSetLayouts.push_back(descriptorSetLayout0);
	descriptorSetLayouts.push_back(descriptorSetLayout1);

	if (descriptorSetLayoutOpt != DE_NULL)
		descriptorSetLayouts.push_back(descriptorSetLayoutOpt);

	return makePipelineLayout(vk, device, (deUint32)descriptorSetLayouts.size(), descriptorSetLayouts.data());
}

static VkWriteDescriptorSetAccelerationStructureKHR makeWriteDescriptorSetAccelerationStructureKHR (const VkAccelerationStructureKHR* accelerationStructureKHR)
{
	const VkWriteDescriptorSetAccelerationStructureKHR	result	=
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,	//  VkStructureType						sType;
		DE_NULL,															//  const void*							pNext;
		1u,																	//  deUint32							accelerationStructureCount;
		accelerationStructureKHR											//  const VkAccelerationStructureKHR*	pAccelerationStructures;
	};

	return result;
}

static bool isPushUpdateMethod (const UpdateMethod	updateMethod)
{
	switch (updateMethod)
	{
		case UPDATE_METHOD_NORMAL:				return false;
		case UPDATE_METHOD_WITH_TEMPLATE:		return false;
		case UPDATE_METHOD_WITH_PUSH:			return true;
		case UPDATE_METHOD_WITH_PUSH_TEMPLATE:	return true;
		default: TCU_THROW(InternalError, "Unknown update method");
	}
}

static bool isTemplateUpdateMethod (const UpdateMethod	updateMethod)
{
	switch (updateMethod)
	{
		case UPDATE_METHOD_NORMAL:				return false;
		case UPDATE_METHOD_WITH_TEMPLATE:		return true;
		case UPDATE_METHOD_WITH_PUSH:			return false;
		case UPDATE_METHOD_WITH_PUSH_TEMPLATE:	return true;
		default: TCU_THROW(InternalError, "Unknown update method");
	}
}

static Move<VkDescriptorSet> makeDescriptorSet (const DeviceInterface&		vki,
												const VkDevice				device,
												const VkDescriptorPool		descriptorPool,
												const VkDescriptorSetLayout	setLayout,
												UpdateMethod				updateMethod)
{
	const bool				pushUpdateMethod	= isPushUpdateMethod(updateMethod);
	Move<VkDescriptorSet>	descriptorSet		= pushUpdateMethod
												? vk::Move<vk::VkDescriptorSet>()
												: vk::makeDescriptorSet(vki, device, descriptorPool, setLayout, DE_NULL);

	return descriptorSet;
}

static VkImageCreateInfo makeImageCreateInfo (VkFormat			format,
											  deUint32			width,
											  deUint32			height,
											  deUint32			depth,
											  VkImageType		imageType	= VK_IMAGE_TYPE_3D,
											  VkImageUsageFlags	usageFlags	= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
{
	const VkImageCreateInfo	imageCreateInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		(VkImageCreateFlags)0u,					// VkImageCreateFlags		flags;
		imageType,								// VkImageType				imageType;
		format,									// VkFormat					format;
		makeExtent3D(width, height, depth),		// VkExtent3D				extent;
		1u,										// deUint32					mipLevels;
		1u,										// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		usageFlags,								// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		0u,										// deUint32					queueFamilyIndexCount;
		DE_NULL,								// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout;
	};

	return imageCreateInfo;
}

static const std::string getMissPassthrough (void)
{
	std::ostringstream src;

	src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
		<< "#extension GL_EXT_ray_tracing : require\n"
		<< "\n"
		<< "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "}\n";

	return src.str();
}

static const std::string getHitPassthrough (void)
{
	std::ostringstream src;

	src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
		<< "#extension GL_EXT_ray_tracing : require\n"
		<< "hitAttributeEXT vec3 attribs;\n"
		<< "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "}\n";

	return src.str();
}

static const std::string getGraphicsPassthrough (void)
{
	std::ostringstream src;

	src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
		<< "\n"
		<< "void main(void)\n"
		<< "{\n"
		<< "}\n";

	return src.str();
}

static const std::string getVertexPassthrough (void)
{
	std::ostringstream src;

	src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
		<< "\n"
		<< "layout(location = 0) in vec4 in_position;\n"
		<< "\n"
		<< "void main(void)\n"
		<< "{\n"
		<< "  gl_Position = in_position;\n"
		<< "}\n";

	return src.str();
}

static VkDescriptorSetLayoutCreateFlags getDescriptorSetLayoutCreateFlags(const UpdateMethod updateMethod)
{
	vk::VkDescriptorSetLayoutCreateFlags	extraFlags	= 0;

	if (updateMethod == UPDATE_METHOD_WITH_PUSH_TEMPLATE || updateMethod == UPDATE_METHOD_WITH_PUSH)
	{
		extraFlags |= vk::VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	}

	return extraFlags;
}

class BindingAcceleratioStructureTestInstance : public TestInstance
{
public:
																	BindingAcceleratioStructureTestInstance		(Context&			context,
																												 const TestParams&	testParams);
	virtual															~BindingAcceleratioStructureTestInstance	() {}
	virtual tcu::TestStatus											iterate										(void);

protected:
	virtual void													initPipeline								(void) = 0;
	virtual deUint32												getExtraAccelerationDescriptorCount			(void) = 0;
	virtual VkShaderStageFlags										getShaderStageFlags							(void) = 0;
	virtual VkPipelineBindPoint										getPipelineBindPoint						(void) = 0;

	virtual void													fillCommandBuffer							(VkCommandBuffer	commandBuffer) = 0;

	virtual const	VkAccelerationStructureKHR*						createAccelerationStructures				(Context&			context,
																												 TestParams&		testParams);
	virtual void													buildAccelerationStructures					(Context&			context,
																												 TestParams&		testParams,
																												 VkCommandBuffer	commandBuffer);
	virtual bool													verify										(BufferWithMemory*	resultBuffer,
																												 Context&			context,
																												 TestParams&		testParams);

	TestParams														m_testParams;

	std::vector<de::SharedPtr<BottomLevelAccelerationStructure>>	m_bottomAccelerationStructures;
	de::SharedPtr<TopLevelAccelerationStructure>					m_topAccelerationStructure;

	Move<VkDescriptorPool>											m_descriptorPool;

	Move<VkDescriptorSetLayout>										m_descriptorSetLayoutImg;
	Move<VkDescriptorSet>											m_descriptorSetImg;

	Move<VkDescriptorSetLayout>										m_descriptorSetLayoutAS;
	Move<VkDescriptorSet>											m_descriptorSetAS;

	Move<VkPipelineLayout>											m_pipelineLayout;
	Move<VkPipeline>												m_pipeline;

	Move<VkDescriptorUpdateTemplate>								m_updateTemplate;
};

BindingAcceleratioStructureTestInstance::BindingAcceleratioStructureTestInstance (Context& context, const TestParams& testParams)
	: TestInstance						(context)
	, m_testParams						(testParams)
	, m_bottomAccelerationStructures	()
	, m_topAccelerationStructure		()
	, m_descriptorPool					()
	, m_descriptorSetLayoutImg			()
	, m_descriptorSetImg				()
	, m_descriptorSetLayoutAS			()
	, m_descriptorSetAS					()
	, m_pipelineLayout					()
	, m_pipeline						()
	, m_updateTemplate					()
{
}

tcu::TestStatus BindingAcceleratioStructureTestInstance::iterate (void)
{
	const DeviceInterface&								vkd										= m_context.getDeviceInterface();
	const VkDevice										device									= m_context.getDevice();
	const VkQueue										queue									= m_context.getUniversalQueue();
	Allocator&											allocator								= m_context.getDefaultAllocator();
	const deUint32										queueFamilyIndex						= m_context.getUniversalQueueFamilyIndex();
	const bool											templateUpdateMethod					= isTemplateUpdateMethod(m_testParams.updateMethod);
	const bool											pushUpdateMethod						= isPushUpdateMethod(m_testParams.updateMethod);

	const deUint32										width									= m_testParams.width;
	const deUint32										height									= m_testParams.height;
	const deUint32										depth									= m_testParams.depth;
	const VkImageCreateInfo								imageCreateInfo							= makeImageCreateInfo(m_testParams.format, width, height, depth);
	const VkImageSubresourceRange						imageSubresourceRange					= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const de::MovePtr<ImageWithMemory>					image									= de::MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
	const Move<VkImageView>								imageView								= makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_3D, m_testParams.format, imageSubresourceRange);

	const deUint32										pixelSize								= mapVkFormat(m_testParams.format).getPixelSize();
	const VkBufferCreateInfo							resultBufferCreateInfo					= makeBufferCreateInfo(width * height * depth * pixelSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkImageSubresourceLayers						resultBufferImageSubresourceLayers		= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const VkBufferImageCopy								resultBufferImageRegion					= makeBufferImageCopy(makeExtent3D(width, height, depth), resultBufferImageSubresourceLayers);
	de::MovePtr<BufferWithMemory>						resultBuffer							= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));
	const VkDescriptorImageInfo							resultImageInfo							= makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);

	const Move<VkCommandPool>							commandPool								= createCommandPool(vkd, device, 0, queueFamilyIndex);
	const Move<VkCommandBuffer>							commandBuffer							= allocateCommandBuffer(vkd, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const VkAccelerationStructureKHR*					topAccelerationStructurePtr				= createAccelerationStructures(m_context, m_testParams);
	const VkWriteDescriptorSetAccelerationStructureKHR	writeDescriptorSetAccelerationStructure	= makeWriteDescriptorSetAccelerationStructureKHR(topAccelerationStructurePtr);
	const deUint32										accelerationStructureDescriptorCount	= 1 + getExtraAccelerationDescriptorCount();
	deUint32											updateCount								= 0;

	m_descriptorPool			= DescriptorPoolBuilder()
									.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
									.addType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, accelerationStructureDescriptorCount)
									.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u + accelerationStructureDescriptorCount);

	m_descriptorSetLayoutImg	= DescriptorSetLayoutBuilder()
									.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, getShaderStageFlags())
									.build(vkd, device);
	m_descriptorSetImg			= makeDescriptorSet(vkd, device, *m_descriptorPool, *m_descriptorSetLayoutImg);

	DescriptorSetUpdateBuilder()
		.writeSingle(*m_descriptorSetImg, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &resultImageInfo)
		.update(vkd, device);

	m_descriptorSetLayoutAS		= DescriptorSetLayoutBuilder()
									.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, getShaderStageFlags())
									.build(vkd, device, getDescriptorSetLayoutCreateFlags(m_testParams.updateMethod));
	m_descriptorSetAS			= makeDescriptorSet(vkd, device, *m_descriptorPool, *m_descriptorSetLayoutAS, m_testParams.updateMethod);

	initPipeline();

	if (m_testParams.updateMethod == UPDATE_METHOD_NORMAL)
	{
		DescriptorSetUpdateBuilder()
			.writeSingle(*m_descriptorSetAS, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &writeDescriptorSetAccelerationStructure)
			.update(vkd, device);

		updateCount++;
	}

	if (templateUpdateMethod)
	{
		const VkDescriptorUpdateTemplateType		updateTemplateType		= isPushUpdateMethod(m_testParams.updateMethod)
																			? VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR
																			: VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;
		const VkDescriptorUpdateTemplateEntry		updateTemplateEntry		=
		{
			0,												//  deUint32			dstBinding;
			0,												//  deUint32			dstArrayElement;
			1,												//  deUint32			descriptorCount;
			VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,	//  VkDescriptorType	descriptorType;
			0,												//  deUintptr			offset;
			0,												//  deUintptr			stride;
		};
		const VkDescriptorUpdateTemplateCreateInfo	templateCreateInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO_KHR,	//  VkStructureType							sType;
			DE_NULL,														//  const void*								pNext;
			0,																//  VkDescriptorUpdateTemplateCreateFlags	flags;
			1,																//  deUint32								descriptorUpdateEntryCount;
			&updateTemplateEntry,											//  const VkDescriptorUpdateTemplateEntry*	pDescriptorUpdateEntries;
			updateTemplateType,												//  VkDescriptorUpdateTemplateType			templateType;
			*m_descriptorSetLayoutAS,										//  VkDescriptorSetLayout					descriptorSetLayout;
			getPipelineBindPoint(),											//  VkPipelineBindPoint						pipelineBindPoint;
			*m_pipelineLayout,												//  VkPipelineLayout						pipelineLayout;
			0,																//  deUint32								set;
		};

		m_updateTemplate = vk::createDescriptorUpdateTemplate(vkd, device, &templateCreateInfo);

		if (!pushUpdateMethod)
		{
			vkd.updateDescriptorSetWithTemplate(device, *m_descriptorSetAS, *m_updateTemplate, topAccelerationStructurePtr);

			updateCount++;
		}
	}

	beginCommandBuffer(vkd, *commandBuffer, 0u);
	{
		{
			const VkClearValue			clearValue				= makeClearValueColorU32(0u, 0u, 0u, 0u);
			const VkImageMemoryBarrier	preImageBarrier			= makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, **image, imageSubresourceRange);
			const VkImageMemoryBarrier	postImageBarrier		= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, **image, imageSubresourceRange);

			cmdPipelineImageMemoryBarrier(vkd, *commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preImageBarrier);
			vkd.cmdClearColorImage(*commandBuffer, **image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1, &imageSubresourceRange);
			cmdPipelineImageMemoryBarrier(vkd, *commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, &postImageBarrier);

			vkd.cmdBindDescriptorSets(*commandBuffer, getPipelineBindPoint(), *m_pipelineLayout, 1, 1, &m_descriptorSetImg.get(), 0, DE_NULL);
		}

		switch (m_testParams.updateMethod)
		{
			case UPDATE_METHOD_NORMAL:			// fallthrough
			case UPDATE_METHOD_WITH_TEMPLATE:
			{
				vkd.cmdBindDescriptorSets(*commandBuffer, getPipelineBindPoint(), *m_pipelineLayout, 0, 1, &m_descriptorSetAS.get(), 0, DE_NULL);

				break;
			}

			case UPDATE_METHOD_WITH_PUSH:
			{
				DescriptorSetUpdateBuilder()
					.writeSingle(*m_descriptorSetAS, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &writeDescriptorSetAccelerationStructure)
					.updateWithPush(vkd, *commandBuffer, getPipelineBindPoint(), *m_pipelineLayout, 0, 0, 1);

				updateCount++;

				break;
			}

			case UPDATE_METHOD_WITH_PUSH_TEMPLATE:
			{
				vkd.cmdPushDescriptorSetWithTemplateKHR(*commandBuffer, *m_updateTemplate, *m_pipelineLayout, 0, topAccelerationStructurePtr);

				updateCount++;

				break;
			}

			default: TCU_THROW(InternalError, "Unknown update method");
		}

		{
			const VkMemoryBarrier		preTraceMemoryBarrier	= makeMemoryBarrier(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR);
			const VkPipelineStageFlags	dstStageFlags			= VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;

			buildAccelerationStructures(m_context, m_testParams, *commandBuffer);

			cmdPipelineMemoryBarrier(vkd, *commandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, dstStageFlags, &preTraceMemoryBarrier);
		}

		fillCommandBuffer(*commandBuffer);

		{
			const VkMemoryBarrier		postTestMemoryBarrier	= makeMemoryBarrier(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);

			cmdPipelineMemoryBarrier(vkd, *commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &postTestMemoryBarrier);
		}

		vkd.cmdCopyImageToBuffer(*commandBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **resultBuffer, 1u, &resultBufferImageRegion);
	}
	endCommandBuffer(vkd, *commandBuffer);

	if (updateCount != 1)
		TCU_THROW(InternalError, "Invalid descriptor update");

	submitCommandsAndWait(vkd, device, queue, commandBuffer.get());

	invalidateMappedMemoryRange(vkd, device, resultBuffer->getAllocation().getMemory(), resultBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);

	if (verify(resultBuffer.get(), m_context, m_testParams))
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Fail");
}

const VkAccelerationStructureKHR* BindingAcceleratioStructureTestInstance::createAccelerationStructures (Context&		context,
																										 TestParams&	testParams)
{
	DE_UNREF(testParams);

	const DeviceInterface&							vkd											= context.getDeviceInterface();
	const VkDevice									device										= context.getDevice();
	Allocator&										allocator									= context.getDefaultAllocator();
	de::MovePtr<BottomLevelAccelerationStructure>	rayQueryBottomLevelAccelerationStructure	= makeBottomLevelAccelerationStructure();
	de::MovePtr<TopLevelAccelerationStructure>		rayQueryTopLevelAccelerationStructure		= makeTopLevelAccelerationStructure();
	std::vector<tcu::Vec3>							geometryData;

	// Generate in-plain square starting at (0,0,PLAIN_Z0) and ending at (1,1,PLAIN_Z1).
	// Vertices 1,0 and 0,1 by Z axis are in the middle between PLAIN_Z0 and PLAIN_Z1
	geometryData.push_back(tcu::Vec3(0.0f, 0.0f, PLAIN_Z0));
	geometryData.push_back(tcu::Vec3(1.0f, 0.0f, (PLAIN_Z0 + PLAIN_Z1) / 2.0f));
	geometryData.push_back(tcu::Vec3(0.0f, 1.0f, (PLAIN_Z0 + PLAIN_Z1) / 2.0f));
	geometryData.push_back(tcu::Vec3(1.0f, 1.0f, PLAIN_Z1));
	geometryData.push_back(tcu::Vec3(0.0f, 1.0f, (PLAIN_Z0 + PLAIN_Z1) / 2.0f));
	geometryData.push_back(tcu::Vec3(1.0f, 0.0f, (PLAIN_Z0 + PLAIN_Z1) / 2.0f));

	rayQueryBottomLevelAccelerationStructure->setGeometryCount(1u);
	rayQueryBottomLevelAccelerationStructure->addGeometry(geometryData, true);
	rayQueryBottomLevelAccelerationStructure->create(vkd, device, allocator, 0);
	m_bottomAccelerationStructures.push_back(de::SharedPtr<BottomLevelAccelerationStructure>(rayQueryBottomLevelAccelerationStructure.release()));

	m_topAccelerationStructure = de::SharedPtr<TopLevelAccelerationStructure>(rayQueryTopLevelAccelerationStructure.release());
	m_topAccelerationStructure->addInstance(m_bottomAccelerationStructures.back());
	m_topAccelerationStructure->create(vkd, device, allocator);

	return m_topAccelerationStructure.get()->getPtr();
}

void BindingAcceleratioStructureTestInstance::buildAccelerationStructures	(Context&			context,
																			 TestParams&		testParams,
																			 VkCommandBuffer	commandBuffer)
{
	DE_UNREF(testParams);

	const DeviceInterface&	vkd		= context.getDeviceInterface();
	const VkDevice			device	= context.getDevice();

	for (size_t blStructNdx = 0; blStructNdx < m_bottomAccelerationStructures.size(); ++blStructNdx)
		m_bottomAccelerationStructures[blStructNdx]->build(vkd, device, commandBuffer);

	m_topAccelerationStructure->build(vkd, device, commandBuffer);
}

bool BindingAcceleratioStructureTestInstance::verify (BufferWithMemory*	resultBuffer,
													  Context&			context,
													  TestParams&		testParams)
{
	tcu::TestLog&			log			= context.getTestContext().getLog();
	const deUint32			width		= testParams.width;
	const deUint32			height		= testParams.height;
	const deInt32*			retrieved	= (deInt32*)resultBuffer->getAllocation().getHostPtr();
	deUint32				failures	= 0;
	deUint32				pos			= 0;
	std::vector<deInt32>	expected;

	expected.reserve(width * height);

	for (deUint32 y = 0; y < height; ++y)
	{
		const float	expectedY	= deFloatMix(PLAIN_Z0, PLAIN_Z1, (0.5f + float(y)) / float(height));

		for (deUint32 x = 0; x < width; ++x)
		{
			const float		expectedX	= deFloatMix(PLAIN_Z0, PLAIN_Z1, (0.5f + float(x)) / float(width));
			const deInt32	expectedV	= deInt32(float(FIXED_POINT_DIVISOR / 2) * (expectedX + expectedY));

			expected.push_back(expectedV);
		}
	}

	for (deUint32 y = 0; y < height; ++y)
	for (deUint32 x = 0; x < width; ++x)
	{
		if (retrieved[pos] != expected[pos])
		{
			failures++;

			if (failures < 10)
			{
				const deInt32	expectedValue	= expected[pos];
				const deInt32	retrievedValue	= retrieved[pos];

				log << tcu::TestLog::Message
					<< "At (" << x <<"," << y << ") "
					<< "expected " << std::fixed << std::setprecision(6) << std::setw(8) << float(expectedValue) / float(FIXED_POINT_DIVISOR) << " (" << expectedValue << ") "
					<< "retrieved " << std::fixed << std::setprecision(6) << std::setw(8) << float(retrievedValue) / float(FIXED_POINT_DIVISOR) << " (" << retrievedValue << ") "
					<< tcu::TestLog::EndMessage;
			}
		}

		pos++;
	}

	if (failures != 0)
	{
		for (deUint32 dumpNdx = 0; dumpNdx < 2; ++dumpNdx)
		{
			const deInt32*		data		= (dumpNdx == 0) ? expected.data() : retrieved;
			const char*			dataName	= (dumpNdx == 0) ? "Expected" : "Retrieved";
			std::ostringstream	css;

			pos = 0;

			for (deUint32 y = 0; y < height; ++y)
			{
				for (deUint32 x = 0; x < width; ++x)
				{
					if (expected[pos] != retrieved[pos])
						css << std::fixed << std::setprecision(6) << std::setw(8) << float(data[pos]) / float(FIXED_POINT_DIVISOR) << ",";
					else
						css << "________,";

					pos++;
				}

				css << std::endl;
			}

			log << tcu::TestLog::Message << dataName << ":" << tcu::TestLog::EndMessage;
			log << tcu::TestLog::Message << css.str() << tcu::TestLog::EndMessage;
		}
	}

	return (failures == 0);
}


class BindingAcceleratioStructureGraphicsTestInstance : public BindingAcceleratioStructureTestInstance
{
public:
	static void							checkSupport										(Context&			context,
																							 const TestParams&	testParams);
	static void							initPrograms										(SourceCollections&	programCollection,
																							 const TestParams&	testParams);

										BindingAcceleratioStructureGraphicsTestInstance		(Context&			context,
																							 const TestParams&	testParams);
	virtual								~BindingAcceleratioStructureGraphicsTestInstance	() {}

protected:
	virtual void						initPipeline										(void) override;
	virtual void						fillCommandBuffer									(VkCommandBuffer	commandBuffer) override;

	void								initVertexBuffer									(void);
	Move<VkPipeline>					makeGraphicsPipeline								(void);

	virtual deUint32					getExtraAccelerationDescriptorCount					(void) override									{ return 0; }
	virtual VkShaderStageFlags			getShaderStageFlags									(void) override									{ return VK_SHADER_STAGE_ALL_GRAPHICS; }
	virtual VkPipelineBindPoint			getPipelineBindPoint								(void) override									{ return VK_PIPELINE_BIND_POINT_GRAPHICS; }

	VkFormat							m_framebufferFormat;
	Move<VkImage>						m_framebufferImage;
	de::MovePtr<Allocation>				m_framebufferImageAlloc;
	Move<VkImageView>					m_framebufferAttachment;

	Move<VkShaderModule>				m_vertShaderModule;
	Move<VkShaderModule>				m_geomShaderModule;
	Move<VkShaderModule>				m_tescShaderModule;
	Move<VkShaderModule>				m_teseShaderModule;
	Move<VkShaderModule>				m_fragShaderModule;

	Move<VkRenderPass>					m_renderPass;
	Move<VkFramebuffer>					m_framebuffer;

	deUint32							m_vertexCount;
	Move<VkBuffer>						m_vertexBuffer;
	de::MovePtr<Allocation>				m_vertexBufferAlloc;
};

BindingAcceleratioStructureGraphicsTestInstance::BindingAcceleratioStructureGraphicsTestInstance (Context&			context,
																								  const TestParams&	testParams)
	: BindingAcceleratioStructureTestInstance	(context, testParams)
	, m_framebufferFormat						(VK_FORMAT_R8G8B8A8_UNORM)
	, m_framebufferImage						()
	, m_framebufferImageAlloc					()
	, m_framebufferAttachment					()
	, m_vertShaderModule						()
	, m_geomShaderModule						()
	, m_tescShaderModule						()
	, m_teseShaderModule						()
	, m_fragShaderModule						()
	, m_renderPass								()
	, m_framebuffer								()
	, m_vertexCount								(0)
	, m_vertexBuffer							()
	, m_vertexBufferAlloc						()
{
}

void BindingAcceleratioStructureGraphicsTestInstance::checkSupport (Context&			context,
																	const TestParams&	testParams)
{
	switch (testParams.stage)
	{
	case VK_SHADER_STAGE_VERTEX_BIT:
	case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
	case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
	case VK_SHADER_STAGE_GEOMETRY_BIT:
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS);
		break;
	default:
		break;
	}

	switch (testParams.stage)
	{
	case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
	case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
		break;
	case VK_SHADER_STAGE_GEOMETRY_BIT:
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
		break;
	default:
		break;
	}
}

void BindingAcceleratioStructureGraphicsTestInstance::initPrograms (SourceCollections&	programCollection,
																	const TestParams&	testParams)
{
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	const std::string				testShaderBody	= testParams.testConfigShaderBodyText(testParams);

	switch (testParams.stage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:
		{
			{
				std::ostringstream src;
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_ray_query : require\n"
					<< "#extension GL_EXT_ray_tracing : require\n"
					<< "\n"
					<< "layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;\n"
					<< "layout(set = 1, binding = 0, r32i) uniform iimage3D result;\n"
					<< "\n"
					<< "void testFunc(ivec3 pos, ivec3 size)\n"
					<< "{\n"
					<< testShaderBody
					<< "}\n"
					<< "\n"
					<< "void main(void)\n"
					<< "{\n"
					<< "  const int   posId    = int(gl_VertexIndex / 3);\n"
					<< "  const int   vertId   = int(gl_VertexIndex % 3);\n"
					<< "  const ivec3 size     = ivec3(" << testParams.width << ", " << testParams.height << ", 1);\n"
					<< "  const ivec3 pos      = ivec3(posId % size.x, posId / size.x, 0);\n"
					<< "\n"
					<< "  if (vertId == 0)\n"
					<< "  {\n"
					<< "    testFunc(pos, size);\n"
					<< "  }\n"
					<< "}\n";

				programCollection.glslSources.add("vert") << glu::VertexSource(src.str()) << buildOptions;
			}

			programCollection.glslSources.add("frag") << glu::FragmentSource(getGraphicsPassthrough()) << buildOptions;

			break;
		}

		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
		{
			{
				std::ostringstream src;
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "\n"
					<< "layout(location = 0) in vec4 in_position;\n"
					<< "out gl_PerVertex\n"
					<< "{\n"
					<< "  vec4 gl_Position;\n"
					<< "};\n"
					<< "\n"
					<< "void main(void)\n"
					<< "{\n"
					<< "  gl_Position = in_position;\n"
					<< "}\n";

				programCollection.glslSources.add("vert") << glu::VertexSource(src.str()) << buildOptions;
			}

			{
				std::ostringstream src;
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_tessellation_shader : require\n"
					<< "#extension GL_EXT_ray_query : require\n"
					<< "\n"
					<< "layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;\n"
					<< "layout(set = 1, binding = 0, r32i) uniform iimage3D result;\n"
					<< "\n"
					<< "in gl_PerVertex\n"
					<< "{\n"
					<< "  vec4 gl_Position;\n"
					<< "} gl_in[];\n"
					<< "layout(vertices = 3) out;\n"
					<< "out gl_PerVertex\n"
					<< "{\n"
					<< "  vec4 gl_Position;\n"
					<< "} gl_out[];\n"
					<< "\n"
					<< "void testFunc(ivec3 pos, ivec3 size)\n"
					<< "{\n"
					<< testShaderBody
					<< "}\n"
					<< "\n"
					<< "void main(void)\n"
					<< "{\n"
					<< "\n"
					<< "  if (gl_InvocationID == 0)\n"
					<< "  {\n"
					<< "    const ivec3 size = ivec3(" << testParams.width << ", " << testParams.height << ", 1);\n"
					<< "    int index = int(gl_in[gl_InvocationID].gl_Position.z);\n"
					<< "    int x = index % size.x;\n"
					<< "    int y = index / size.y;\n"
					<< "    const ivec3 pos = ivec3(x, y, 0);\n"
					<< "    testFunc(pos, size);\n"
					<< "  }\n"
					<< "\n"
					<< "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
					<< "  gl_TessLevelInner[0] = 1;\n"
					<< "  gl_TessLevelInner[1] = 1;\n"
					<< "  gl_TessLevelOuter[gl_InvocationID] = 1;\n"
					<< "}\n";

				programCollection.glslSources.add("tesc") << glu::TessellationControlSource(src.str()) << buildOptions;
			}

			{
				std::ostringstream src;
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_tessellation_shader : require\n"
					<< "layout(triangles, equal_spacing, ccw) in;\n"
					<< "\n"
					<< "in gl_PerVertex\n"
					<< "{\n"
					<< "  vec4 gl_Position;\n"
					<< "} gl_in[];\n"
					<< "\n"
					<< "void main(void)\n"
					<< "{\n"
					<< "  gl_Position = gl_in[0].gl_Position;\n"
					<< "}\n";

				programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str()) << buildOptions;
			}

			break;
		}

		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
		{
			{
				std::ostringstream src;
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "\n"
					<< "layout(location = 0) in vec4 in_position;\n"
					<< "out gl_PerVertex"
					<< "{\n"
					<< "  vec4 gl_Position;\n"
					<< "};\n"
					<< "\n"
					<< "void main(void)\n"
					<< "{\n"
					<< "  gl_Position = in_position;\n"
					<< "}\n";

				programCollection.glslSources.add("vert") << glu::VertexSource(src.str()) << buildOptions;
			}

			{
				std::ostringstream src;
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_tessellation_shader : require\n"
					<< "\n"
					<< "in gl_PerVertex\n"
					<< "{\n"
					<< "  vec4 gl_Position;\n"
					<< "} gl_in[];\n"
					<< "layout(vertices = 3) out;\n"
					<< "out gl_PerVertex\n"
					<< "{\n"
					<< "  vec4 gl_Position;\n"
					<< "} gl_out[];\n"
					<< "\n"
					<< "void main(void)\n"
					<< "{\n"
					<< "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
					<< "  gl_TessLevelInner[0] = 1;\n"
					<< "  gl_TessLevelInner[1] = 1;\n"
					<< "  gl_TessLevelOuter[gl_InvocationID] = 1;\n"
					<< "}\n";

				programCollection.glslSources.add("tesc") << glu::TessellationControlSource(src.str()) << buildOptions;
			}

			{
				std::ostringstream src;
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_tessellation_shader : require\n"
					<< "#extension GL_EXT_ray_query : require\n"
					<< "\n"
					<< "layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;\n"
					<< "layout(set = 1, binding = 0, r32i) uniform iimage3D result;\n"
					<< "\n"
					<< "layout(triangles, equal_spacing, ccw) in;\n"
					<< "in gl_PerVertex\n"
					<< "{\n"
					<< "  vec4 gl_Position;\n"
					<< "} gl_in[];\n"
					<< "\n"
					<< "void testFunc(ivec3 pos, ivec3 size)\n"
					<< "{\n"
					<< testShaderBody
					<< "}\n"
					<< "\n"
					<< "void main(void)\n"
					<< "{\n"
					<< "	const ivec3 size = ivec3(" << testParams.width << ", " << testParams.height << ", 1);\n"
					<< "	int index = int(gl_in[0].gl_Position.z);\n"
					<< "	int x = index % size.x;\n"
					<< "	int y = index / size.y;\n"
					<< "	const ivec3 pos = ivec3(x, y, 0);\n"
					<< "	testFunc(pos, size);\n"
					<< "	gl_Position = gl_in[0].gl_Position;\n"
					<< "}\n";

				programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str()) << buildOptions;
			}

			break;
		}

		case VK_SHADER_STAGE_GEOMETRY_BIT:
		{
			programCollection.glslSources.add("vert") << glu::VertexSource(getVertexPassthrough()) << buildOptions;

			{
				std::ostringstream src;
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_ray_query : require\n"
					<< "\n"
					<< "layout(triangles) in;\n"
					<< "layout(points, max_vertices = 1) out;\n"
					<< "\n"
					<< "layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;\n"
					<< "layout(set = 1, binding = 0, r32i) uniform iimage3D result;\n"
					<< "\n"
					<< "void testFunc(ivec3 pos, ivec3 size)\n"
					<< "{\n"
					<< testShaderBody
					<< "}\n"
					<< "\n"
					<< "void main(void)\n"
					<< "{\n"
					<< "  const int   posId    = int(gl_PrimitiveIDIn);\n"
					<< "  const ivec3 size     = ivec3(" << testParams.width << ", " << testParams.height << ", 1);\n"
					<< "  const ivec3 pos      = ivec3(posId % size.x, posId / size.x, 0);\n"
					<< "\n"
					<< "  testFunc(pos, size);\n"
					<< "  gl_PointSize = 1.0;\n"
					<< "}\n";

				programCollection.glslSources.add("geom") << glu::GeometrySource(src.str()) << buildOptions;
			}

			break;
		}

		case VK_SHADER_STAGE_FRAGMENT_BIT:
		{
			programCollection.glslSources.add("vert") << glu::VertexSource(getVertexPassthrough()) << buildOptions;

			{
				std::ostringstream src;
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_ray_query : require\n"
					<< "\n"
					<< "layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;\n"
					<< "layout(set = 1, binding = 0, r32i) uniform iimage3D result;\n"
					<< "\n"
					<< "void testFunc(ivec3 pos, ivec3 size)\n"
					<< "{\n"
					<< testShaderBody
					<< "}\n"
					<< "\n"
					<< "void main(void)\n"
					<< "{\n"
					<< "  const ivec3 size     = ivec3(" << testParams.width << ", " << testParams.height << ", 1);\n"
					<< "  const ivec3 pos      = ivec3(int(gl_FragCoord.x - 0.5f), int(gl_FragCoord.y - 0.5f), 0);\n"
					<< "\n"
					<< "  testFunc(pos, size);\n"
					<< "}\n";

				programCollection.glslSources.add("frag") << glu::FragmentSource(src.str()) << buildOptions;
			}

			break;
		}

		default:
			TCU_THROW(InternalError, "Unknown stage");
	}
}

void BindingAcceleratioStructureGraphicsTestInstance::initVertexBuffer (void)
{
	const DeviceInterface&	vkd			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();
	const deUint32			width		= m_testParams.width;
	const deUint32			height		= m_testParams.height;
	Allocator&				allocator	= m_context.getDefaultAllocator();
	std::vector<tcu::Vec4>	vertices;

	switch (m_testParams.stage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
		case VK_SHADER_STAGE_GEOMETRY_BIT:
		{
			float z = 0.0f;

			vertices.reserve(3 * height * width);

			for (deUint32 y = 0; y < height; ++y)
			for (deUint32 x = 0; x < width; ++x)
			{
				const float	x0	= float(x + 0) / float(width);
				const float	y0	= float(y + 0) / float(height);
				const float	x1	= float(x + 1) / float(width);
				const float	y1	= float(y + 1) / float(height);
				const float	xm	= (x0 + x1) / 2.0f;
				const float	ym	= (y0 + y1) / 2.0f;

				vertices.push_back(tcu::Vec4(x0, y0, z, 1.0f));
				vertices.push_back(tcu::Vec4(xm, y1, z, 1.0f));
				vertices.push_back(tcu::Vec4(x1, ym, z, 1.0f));

				z += 1.f;
			}

			break;
		}

		case VK_SHADER_STAGE_FRAGMENT_BIT:
		{
			const float		z = 1.0f;
			const tcu::Vec4	a = tcu::Vec4(-1.0f, -1.0f, z, 1.0f);
			const tcu::Vec4	b = tcu::Vec4(+1.0f, -1.0f, z, 1.0f);
			const tcu::Vec4	c = tcu::Vec4(-1.0f, +1.0f, z, 1.0f);
			const tcu::Vec4	d = tcu::Vec4(+1.0f, +1.0f, z, 1.0f);

			vertices.push_back(a);
			vertices.push_back(b);
			vertices.push_back(c);

			vertices.push_back(b);
			vertices.push_back(c);
			vertices.push_back(d);

			break;
		}

		default:
			TCU_THROW(InternalError, "Unknown stage");

	}

	// Initialize vertex buffer
	{
		const VkDeviceSize			vertexBufferSize		= sizeof(vertices[0][0]) * vertices[0].SIZE * vertices.size();
		const VkBufferCreateInfo	vertexBufferCreateInfo	= makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

		m_vertexCount		= static_cast<deUint32>(vertices.size());
		m_vertexBuffer		= createBuffer(vkd, device, &vertexBufferCreateInfo);
		m_vertexBufferAlloc	= bindBuffer(vkd, device, allocator, *m_vertexBuffer, vk::MemoryRequirement::HostVisible);

		deMemcpy(m_vertexBufferAlloc->getHostPtr(), vertices.data(), (size_t)vertexBufferSize);
		flushAlloc(vkd, device, *m_vertexBufferAlloc);
	}
}

Move<VkPipeline> BindingAcceleratioStructureGraphicsTestInstance::makeGraphicsPipeline (void)
{
	const DeviceInterface&			vkd					= m_context.getDeviceInterface();
	const VkDevice					device				= m_context.getDevice();
	const bool						tessStageTest		= (m_testParams.stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT || m_testParams.stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
	const VkPrimitiveTopology		topology			= tessStageTest ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	const deUint32					patchControlPoints	= tessStageTest ? 3 : 0;
	const std::vector<VkViewport>	viewports			(1, makeViewport(m_testParams.width, m_testParams.height));
	const std::vector<VkRect2D>		scissors			(1, makeRect2D(m_testParams.width, m_testParams.height));

	return vk::makeGraphicsPipeline	(vkd,
									 device,
									 *m_pipelineLayout,
									 *m_vertShaderModule,
									 *m_tescShaderModule,
									 *m_teseShaderModule,
									 *m_geomShaderModule,
									 *m_fragShaderModule,
									 *m_renderPass,
									 viewports,
									 scissors,
									 topology,
									 0,
									 patchControlPoints);
}

void BindingAcceleratioStructureGraphicsTestInstance::initPipeline (void)
{
	const DeviceInterface&	vkd			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();
	Allocator&				allocator	= m_context.getDefaultAllocator();
	vk::BinaryCollection&	collection	= m_context.getBinaryCollection();
	VkShaderStageFlags		shaders		= static_cast<VkShaderStageFlags>(0);
	deUint32				shaderCount	= 0;

	if (collection.contains("vert")) shaders |= VK_SHADER_STAGE_VERTEX_BIT;
	if (collection.contains("geom")) shaders |= VK_SHADER_STAGE_GEOMETRY_BIT;
	if (collection.contains("tesc")) shaders |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
	if (collection.contains("tese")) shaders |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
	if (collection.contains("frag")) shaders |= VK_SHADER_STAGE_FRAGMENT_BIT;

	for (BinaryCollection::Iterator it = collection.begin(); it != collection.end(); ++it)
		shaderCount++;

	if (shaderCount != (deUint32)dePop32(shaders))
		TCU_THROW(InternalError, "Unused shaders detected in the collection");

	if (0 != (shaders & VK_SHADER_STAGE_VERTEX_BIT))					m_vertShaderModule = createShaderModule(vkd, device, collection.get("vert"), 0);
	if (0 != (shaders & VK_SHADER_STAGE_GEOMETRY_BIT))					m_geomShaderModule = createShaderModule(vkd, device, collection.get("geom"), 0);
	if (0 != (shaders & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT))		m_tescShaderModule = createShaderModule(vkd, device, collection.get("tesc"), 0);
	if (0 != (shaders & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))	m_teseShaderModule = createShaderModule(vkd, device, collection.get("tese"), 0);
	if (0 != (shaders & VK_SHADER_STAGE_FRAGMENT_BIT))					m_fragShaderModule = createShaderModule(vkd, device, collection.get("frag"), 0);

	m_framebufferImage		= makeImage				(vkd, device, makeImageCreateInfo(m_framebufferFormat, m_testParams.width, m_testParams.height, 1u, VK_IMAGE_TYPE_2D, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT));
	m_framebufferImageAlloc	= bindImage				(vkd, device, allocator, *m_framebufferImage, MemoryRequirement::Any);
	m_framebufferAttachment	= makeImageView			(vkd, device, *m_framebufferImage, VK_IMAGE_VIEW_TYPE_2D, m_framebufferFormat, makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));
	m_renderPass			= makeRenderPass		(vkd, device, m_framebufferFormat);
	m_framebuffer			= makeFramebuffer		(vkd, device, *m_renderPass, *m_framebufferAttachment, m_testParams.width, m_testParams.height);
	m_pipelineLayout		= makePipelineLayout	(vkd, device, m_descriptorSetLayoutAS.get(), m_descriptorSetLayoutImg.get());
	m_pipeline				= makeGraphicsPipeline	();

	initVertexBuffer();
}

void BindingAcceleratioStructureGraphicsTestInstance::fillCommandBuffer (VkCommandBuffer	commandBuffer)
{
	const DeviceInterface&	vkd					= m_context.getDeviceInterface();
	const VkDeviceSize		vertexBufferOffset	= 0;

	vkd.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
	vkd.cmdBindVertexBuffers(commandBuffer, 0u, 1u, &m_vertexBuffer.get(), &vertexBufferOffset);

	beginRenderPass(vkd, commandBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, m_testParams.width, m_testParams.height), tcu::UVec4());

	vkd.cmdDraw(commandBuffer, m_vertexCount, 1u, 0u, 0u);

	endRenderPass(vkd, commandBuffer);
}

class BindingAcceleratioStructureComputeTestInstance : public BindingAcceleratioStructureTestInstance
{
public:
									BindingAcceleratioStructureComputeTestInstance	(Context&			context,
																					 const TestParams&	testParams);

	virtual							~BindingAcceleratioStructureComputeTestInstance	() {}

	static void						checkSupport									(Context&			context,
																					 const TestParams&	testParams);
	static void						initPrograms									(SourceCollections&	programCollection,
																					 const TestParams&	testParams);

protected:
	virtual void					initPipeline									(void) override;
	virtual void					fillCommandBuffer								(VkCommandBuffer	commandBuffer) override;

	virtual deUint32				getExtraAccelerationDescriptorCount				(void) override									{ return 0; }
	virtual VkShaderStageFlags		getShaderStageFlags								(void) override									{ return VK_SHADER_STAGE_COMPUTE_BIT; }
	virtual VkPipelineBindPoint		getPipelineBindPoint							(void) override									{ return VK_PIPELINE_BIND_POINT_COMPUTE; }

	Move<VkShaderModule>			m_shaderModule;
};

BindingAcceleratioStructureComputeTestInstance::BindingAcceleratioStructureComputeTestInstance (Context&			context,
																								const TestParams&	testParams)
	: BindingAcceleratioStructureTestInstance	(context, testParams)
	, m_shaderModule	()
{
}

void BindingAcceleratioStructureComputeTestInstance::checkSupport (Context&				context,
																   const TestParams&	testParams)
{
	DE_UNREF(context);
	DE_UNREF(testParams);
}

void BindingAcceleratioStructureComputeTestInstance::initPrograms (SourceCollections&	programCollection,
																   const TestParams&	testParams)
{
	const vk::ShaderBuildOptions	buildOptions		(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	const std::string				testShaderBody		= testParams.testConfigShaderBodyText(testParams);
	const std::string				testBody			=
		"  ivec3       pos      = ivec3(gl_WorkGroupID);\n"
		"  ivec3       size     = ivec3(gl_NumWorkGroups);\n"
		+ testShaderBody;

	switch (testParams.stage)
	{
		case VK_SHADER_STAGE_COMPUTE_BIT:
		{
			std::stringstream css;
			css << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
				<< "#extension GL_EXT_ray_query : require\n"
				<< "\n"
				<< "layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;\n"
				<< "layout(set = 1, binding = 0, r32i) uniform iimage3D result;\n"
				<< "\n"
				<< "void main()\n"
				<< "{\n"
				<< testBody
				<< "}\n";

			programCollection.glslSources.add("comp") << glu::ComputeSource(css.str()) << buildOptions;

			break;
		}

		default:
			TCU_THROW(InternalError, "Unknown stage");
	}
}

void BindingAcceleratioStructureComputeTestInstance::initPipeline (void)
{
	const DeviceInterface&	vkd			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();
	vk::BinaryCollection&	collection	= m_context.getBinaryCollection();

	m_shaderModule		= createShaderModule(vkd, device, collection.get("comp"), 0);
	m_pipelineLayout	= makePipelineLayout(vkd, device, m_descriptorSetLayoutAS.get(), m_descriptorSetLayoutImg.get());
	m_pipeline			= makeComputePipeline(vkd, device, *m_pipelineLayout, *m_shaderModule);
}

void BindingAcceleratioStructureComputeTestInstance::fillCommandBuffer (VkCommandBuffer	commandBuffer)
{
	const DeviceInterface&	vkd	= m_context.getDeviceInterface();

	vkd.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline.get());

	vkd.cmdDispatch(commandBuffer, m_testParams.width, m_testParams.height, 1);
}

class BindingAcceleratioStructureRayTracingTestInstance : public BindingAcceleratioStructureTestInstance
{
public:
													BindingAcceleratioStructureRayTracingTestInstance	(Context&							context,
																										 const TestParams&					testParams);
	virtual											~BindingAcceleratioStructureRayTracingTestInstance	() {}

	static void										checkSupport										(Context&							context,
																										 const TestParams&					testParams);
	static void										initPrograms										(SourceCollections&					programCollection,
																										 const TestParams&					testParams);

protected:
	virtual void									initPipeline										(void) override;
	virtual void									fillCommandBuffer									(VkCommandBuffer					commandBuffer) override;

	de::MovePtr<BufferWithMemory>					createShaderBindingTable							(const InstanceInterface&			vki,
																										 const DeviceInterface&				vkd,
																										 const VkDevice						device,
																										 const VkPhysicalDevice				physicalDevice,
																										 const VkPipeline					pipeline,
																										 Allocator&							allocator,
																										 de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
																										 const deUint32						group);

	virtual deUint32								getExtraAccelerationDescriptorCount					(void) override													{ return 1; }
	virtual VkShaderStageFlags						getShaderStageFlags									(void) override													{ return ALL_RAY_TRACING_STAGES; }
	virtual VkPipelineBindPoint						getPipelineBindPoint								(void) override													{ return VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR; }

	deUint32										m_shaders;
	deUint32										m_raygenShaderGroup;
	deUint32										m_missShaderGroup;
	deUint32										m_hitShaderGroup;
	deUint32										m_callableShaderGroup;
	deUint32										m_shaderGroupCount;

	Move<VkDescriptorSetLayout>						m_descriptorSetLayoutSvc;
	Move<VkDescriptorSet>							m_descriptorSetSvc;

	de::MovePtr<RayTracingPipeline>					m_rayTracingPipeline;

	de::MovePtr<BufferWithMemory>					m_raygenShaderBindingTable;
	de::MovePtr<BufferWithMemory>					m_hitShaderBindingTable;
	de::MovePtr<BufferWithMemory>					m_missShaderBindingTable;
	de::MovePtr<BufferWithMemory>					m_callableShaderBindingTable;

	VkStridedDeviceAddressRegionKHR					m_raygenShaderBindingTableRegion;
	VkStridedDeviceAddressRegionKHR					m_missShaderBindingTableRegion;
	VkStridedDeviceAddressRegionKHR					m_hitShaderBindingTableRegion;
	VkStridedDeviceAddressRegionKHR					m_callableShaderBindingTableRegion;

	de::SharedPtr<BottomLevelAccelerationStructure>	m_bottomLevelAccelerationStructure;
	de::SharedPtr<TopLevelAccelerationStructure>	m_topLevelAccelerationStructure;
};

BindingAcceleratioStructureRayTracingTestInstance::BindingAcceleratioStructureRayTracingTestInstance (Context&			context,
																									  const TestParams&	testParams)
	: BindingAcceleratioStructureTestInstance	(context, testParams)
	, m_shaders									(0)
	, m_raygenShaderGroup						(~0u)
	, m_missShaderGroup							(~0u)
	, m_hitShaderGroup							(~0u)
	, m_callableShaderGroup						(~0u)
	, m_shaderGroupCount						(0)

	, m_descriptorSetLayoutSvc					()
	, m_descriptorSetSvc						()

	, m_rayTracingPipeline						()

	, m_raygenShaderBindingTable				()
	, m_hitShaderBindingTable					()
	, m_missShaderBindingTable					()
	, m_callableShaderBindingTable				()

	, m_raygenShaderBindingTableRegion			()
	, m_missShaderBindingTableRegion			()
	, m_hitShaderBindingTableRegion				()
	, m_callableShaderBindingTableRegion		()

	, m_bottomLevelAccelerationStructure		()
	, m_topLevelAccelerationStructure			()
{
}

void BindingAcceleratioStructureRayTracingTestInstance::checkSupport (Context&			context,
																	  const TestParams&	testParams)
{
	DE_UNREF(testParams);

	context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

	const VkPhysicalDeviceRayTracingPipelineFeaturesKHR&	rayTracingPipelineFeaturesKHR = context.getRayTracingPipelineFeatures();

	if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");
}

void BindingAcceleratioStructureRayTracingTestInstance::initPrograms (SourceCollections&	programCollection,
																	  const TestParams&		testParams)
{
	const vk::ShaderBuildOptions	buildOptions				(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	const std::string				testShaderBody				= testParams.testConfigShaderBodyText(testParams);
	const std::string				testBody					=
		"  ivec3       pos      = ivec3(gl_LaunchIDEXT);\n"
		"  ivec3       size     = ivec3(gl_LaunchSizeEXT);\n"
		+ testShaderBody;
	const std::string				commonRayGenerationShader	=
		std::string(glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460)) + "\n"
		"#extension GL_EXT_ray_tracing : require\n"
		"\n"
		"layout(location = 0) rayPayloadEXT vec3 hitValue;\n"
		"layout(set = 2, binding = 0) uniform accelerationStructureEXT topLevelAS;\n"
		"\n"
		"void main()\n"
		"{\n"
		"  uint  rayFlags = 0;\n"
		"  uint  cullMask = 0xFF;\n"
		"  float tmin     = 0.0;\n"
		"  float tmax     = 9.0;\n"
		"  vec3  origin   = vec3((float(gl_LaunchIDEXT.x) + 0.5f) / float(gl_LaunchSizeEXT.x), (float(gl_LaunchIDEXT.y) + 0.5f) / float(gl_LaunchSizeEXT.y), 0.0);\n"
		"  vec3  direct   = vec3(0.0, 0.0, -1.0);\n"
		"  traceRayEXT(topLevelAS, rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
		"}\n";

	switch (testParams.stage)
	{
		case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
		{
			std::stringstream css;
			css << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
				<< "#extension GL_EXT_ray_tracing : require\n"
				<< "#extension GL_EXT_ray_query : require\n"
				<< "\n"
				<< "layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;\n"
				<< "layout(set = 1, binding = 0, r32i) uniform iimage3D result;\n"
				<< "\n"
				<< "void main()\n"
				<< "{\n"
				<< testBody
				<< "}\n";

			programCollection.glslSources.add("rgen") << glu::RaygenSource(css.str()) << buildOptions;

			break;
		}

		case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
		{
			programCollection.glslSources.add("rgen") << glu::RaygenSource(commonRayGenerationShader) << buildOptions;

			{
				std::stringstream css;
				css << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_ray_tracing : require\n"
					<< "#extension GL_EXT_ray_query : require\n"
					<< "\n"
					<< "hitAttributeEXT vec3 attribs;\n"
					<< "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
					<< "\n"
					<< "layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;\n"
					<< "layout(set = 1, binding = 0, r32i) uniform iimage3D result;\n"
					<< "\n"
					<< "void main()\n"
					<< "{\n"
					<< testBody
					<< "}\n";

				programCollection.glslSources.add("ahit") << glu::AnyHitSource(css.str()) << buildOptions;
			}

			programCollection.glslSources.add("chit") << glu::ClosestHitSource(getHitPassthrough()) << buildOptions;
			programCollection.glslSources.add("miss") << glu::MissSource(getMissPassthrough()) << buildOptions;

			break;
		}

		case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
		{
			programCollection.glslSources.add("rgen") << glu::RaygenSource(commonRayGenerationShader) << buildOptions;

			{
				std::stringstream css;
				css << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_ray_tracing : require\n"
					<< "#extension GL_EXT_ray_query : require\n"
					<< "\n"
					<< "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
					<< "hitAttributeEXT vec3 attribs;\n"
					<< "\n"
					<< "layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;\n"
					<< "layout(set = 1, binding = 0, r32i) uniform iimage3D result;\n"
					<< "\n"
					<< "void main()\n"
					<< "{\n"
					<< testBody
					<< "}\n";

				programCollection.glslSources.add("chit") << glu::ClosestHitSource(css.str()) << buildOptions;
			}

			programCollection.glslSources.add("ahit") << glu::AnyHitSource(getHitPassthrough()) << buildOptions;
			programCollection.glslSources.add("miss") << glu::MissSource(getMissPassthrough()) << buildOptions;

			break;
		}

		case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
		{
			programCollection.glslSources.add("rgen") << glu::RaygenSource(commonRayGenerationShader) << buildOptions;

			{
				std::stringstream css;
				css << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_ray_tracing : require\n"
					<< "#extension GL_EXT_ray_query : require\n"
					<< "hitAttributeEXT vec3 hitAttribute;\n"
					<< "\n"
					<< "layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;\n"
					<< "layout(set = 1, binding = 0, r32i) uniform iimage3D result;\n"
					<< "\n"
					<< "void main()\n"
					<< "{\n"
					<< testBody
					<< "  hitAttribute = vec3(0.0f, 0.0f, 0.0f);\n"
					<< "  reportIntersectionEXT(1.0f, 0);\n"
					<< "}\n";

				programCollection.glslSources.add("sect") << glu::IntersectionSource(css.str()) << buildOptions;
			}

			programCollection.glslSources.add("ahit") << glu::AnyHitSource(getHitPassthrough()) << buildOptions;
			programCollection.glslSources.add("chit") << glu::ClosestHitSource(getHitPassthrough()) << buildOptions;
			programCollection.glslSources.add("miss") << glu::MissSource(getMissPassthrough()) << buildOptions;

			break;
		}

		case VK_SHADER_STAGE_MISS_BIT_KHR:
		{
			programCollection.glslSources.add("rgen") << glu::RaygenSource(commonRayGenerationShader) << buildOptions;

			{
				std::stringstream css;
				css << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_ray_tracing : require\n"
					<< "#extension GL_EXT_ray_query : require\n"
					<< "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
					<< "\n"
					<< "layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;\n"
					<< "layout(set = 1, binding = 0, r32i) uniform iimage3D result;\n"
					<< "\n"
					<< "void main()\n"
					<< "{\n"
					<< testBody
					<< "}\n";

				programCollection.glslSources.add("miss") << glu::MissSource(css.str()) << buildOptions;
			}

			programCollection.glslSources.add("ahit") << glu::AnyHitSource(getHitPassthrough()) << buildOptions;
			programCollection.glslSources.add("chit") << glu::ClosestHitSource(getHitPassthrough()) << buildOptions;

			break;
		}

		case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
		{
			{
				std::stringstream css;
				css << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_ray_tracing : require\n"
					<< "#extension GL_EXT_ray_query : require\n"
					<< "\n"
					<< "layout(location = 0) callableDataEXT float dummy;"
					<< "layout(set = 2, binding = 0) uniform accelerationStructureEXT topLevelAS;\n"
					<< "\n"
					<< "void main()\n"
					<< "{\n"
					<< "  executeCallableEXT(0, 0);\n"
					<< "}\n";

				programCollection.glslSources.add("rgen") << glu::RaygenSource(css.str()) << buildOptions;
			}

			{
				std::stringstream css;
				css << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_ray_tracing : require\n"
					<< "#extension GL_EXT_ray_query : require\n"
					<< "layout(location = 0) callableDataInEXT float dummy;"
					<< "\n"
					<< "layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;\n"
					<< "layout(set = 1, binding = 0, r32i) uniform iimage3D result;\n"
					<< "\n"
					<< "void main()\n"
					<< "{\n"
					<< testBody
					<< "}\n";

				programCollection.glslSources.add("call") << glu::CallableSource(css.str()) << buildOptions;
			}

			programCollection.glslSources.add("ahit") << glu::AnyHitSource(getHitPassthrough()) << buildOptions;
			programCollection.glslSources.add("chit") << glu::ClosestHitSource(getHitPassthrough()) << buildOptions;
			programCollection.glslSources.add("miss") << glu::MissSource(getMissPassthrough()) << buildOptions;

			break;
		}

		default:
			TCU_THROW(InternalError, "Unknown stage");
	}
}

void BindingAcceleratioStructureRayTracingTestInstance::initPipeline (void)
{
	const InstanceInterface&	vki						= m_context.getInstanceInterface();
	const DeviceInterface&		vkd						= m_context.getDeviceInterface();
	const VkDevice				device					= m_context.getDevice();
	const VkPhysicalDevice		physicalDevice			= m_context.getPhysicalDevice();
	vk::BinaryCollection&		collection				= m_context.getBinaryCollection();
	Allocator&					allocator				= m_context.getDefaultAllocator();
	const deUint32				shaderGroupHandleSize	= getShaderGroupHandleSize(vki, physicalDevice);
	const VkShaderStageFlags	hitStages				= VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
	deUint32					shaderCount				= 0;

	m_shaderGroupCount = 0;

	if (collection.contains("rgen")) m_shaders |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	if (collection.contains("ahit")) m_shaders |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	if (collection.contains("chit")) m_shaders |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	if (collection.contains("miss")) m_shaders |= VK_SHADER_STAGE_MISS_BIT_KHR;
	if (collection.contains("sect")) m_shaders |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
	if (collection.contains("call")) m_shaders |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;

	for (BinaryCollection::Iterator it = collection.begin(); it != collection.end(); ++it)
		shaderCount++;

	if (shaderCount != (deUint32)dePop32(m_shaders))
		TCU_THROW(InternalError, "Unused shaders detected in the collection");

	if (0 != (m_shaders & VK_SHADER_STAGE_RAYGEN_BIT_KHR))
		m_raygenShaderGroup		= m_shaderGroupCount++;

	if (0 != (m_shaders & VK_SHADER_STAGE_MISS_BIT_KHR))
		m_missShaderGroup		= m_shaderGroupCount++;

	if (0 != (m_shaders & hitStages))
		m_hitShaderGroup		= m_shaderGroupCount++;

	if (0 != (m_shaders & VK_SHADER_STAGE_CALLABLE_BIT_KHR))
		m_callableShaderGroup	= m_shaderGroupCount++;

	m_rayTracingPipeline		= de::newMovePtr<RayTracingPipeline>();

	m_descriptorSetLayoutSvc	= DescriptorSetLayoutBuilder()
									.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, ALL_RAY_TRACING_STAGES)
									.build(vkd, device);
	m_descriptorSetSvc			= makeDescriptorSet(vkd, device, *m_descriptorPool, *m_descriptorSetLayoutSvc);

	if (0 != (m_shaders & VK_SHADER_STAGE_RAYGEN_BIT_KHR))			m_rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR			, createShaderModule(vkd, device, collection.get("rgen"), 0), m_raygenShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_ANY_HIT_BIT_KHR))			m_rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR			, createShaderModule(vkd, device, collection.get("ahit"), 0), m_hitShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR))		m_rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR		, createShaderModule(vkd, device, collection.get("chit"), 0), m_hitShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_MISS_BIT_KHR))			m_rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR			, createShaderModule(vkd, device, collection.get("miss"), 0), m_missShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_INTERSECTION_BIT_KHR))	m_rayTracingPipeline->addShader(VK_SHADER_STAGE_INTERSECTION_BIT_KHR	, createShaderModule(vkd, device, collection.get("sect"), 0), m_hitShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_CALLABLE_BIT_KHR))		m_rayTracingPipeline->addShader(VK_SHADER_STAGE_CALLABLE_BIT_KHR		, createShaderModule(vkd, device, collection.get("call"), 0), m_callableShaderGroup);

	m_pipelineLayout					= makePipelineLayout(vkd, device, m_descriptorSetLayoutAS.get(), m_descriptorSetLayoutImg.get(), m_descriptorSetLayoutSvc.get());
	m_pipeline							= m_rayTracingPipeline->createPipeline(vkd, device, *m_pipelineLayout);

	m_raygenShaderBindingTable			= createShaderBindingTable(vki, vkd, device, physicalDevice, *m_pipeline, allocator, m_rayTracingPipeline, m_raygenShaderGroup);
	m_missShaderBindingTable			= createShaderBindingTable(vki, vkd, device, physicalDevice, *m_pipeline, allocator, m_rayTracingPipeline, m_missShaderGroup);
	m_hitShaderBindingTable				= createShaderBindingTable(vki, vkd, device, physicalDevice, *m_pipeline, allocator, m_rayTracingPipeline, m_hitShaderGroup);
	m_callableShaderBindingTable		= createShaderBindingTable(vki, vkd, device, physicalDevice, *m_pipeline, allocator, m_rayTracingPipeline, m_callableShaderGroup);

	m_raygenShaderBindingTableRegion	= makeStridedDeviceAddressRegion(vkd, device, getVkBuffer(m_raygenShaderBindingTable),		shaderGroupHandleSize, 1);
	m_missShaderBindingTableRegion		= makeStridedDeviceAddressRegion(vkd, device, getVkBuffer(m_missShaderBindingTable),		shaderGroupHandleSize, 1);
	m_hitShaderBindingTableRegion		= makeStridedDeviceAddressRegion(vkd, device, getVkBuffer(m_hitShaderBindingTable),			shaderGroupHandleSize, 1);
	m_callableShaderBindingTableRegion	= makeStridedDeviceAddressRegion(vkd, device, getVkBuffer(m_callableShaderBindingTable),	shaderGroupHandleSize, 1);
}

void BindingAcceleratioStructureRayTracingTestInstance::fillCommandBuffer (VkCommandBuffer	commandBuffer)
{
	const DeviceInterface&							vkd									= m_context.getDeviceInterface();
	const VkDevice									device								= m_context.getDevice();
	Allocator&										allocator							= m_context.getDefaultAllocator();
	de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure	= makeBottomLevelAccelerationStructure();
	de::MovePtr<TopLevelAccelerationStructure>		topLevelAccelerationStructure		= makeTopLevelAccelerationStructure();

	m_bottomLevelAccelerationStructure = de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release());
	m_bottomLevelAccelerationStructure->setDefaultGeometryData(m_testParams.stage);
	m_bottomLevelAccelerationStructure->createAndBuild(vkd, device, commandBuffer, allocator);

	m_topLevelAccelerationStructure = de::SharedPtr<TopLevelAccelerationStructure>(topLevelAccelerationStructure.release());
	m_topLevelAccelerationStructure->setInstanceCount(1);
	m_topLevelAccelerationStructure->addInstance(m_bottomLevelAccelerationStructure);
	m_topLevelAccelerationStructure->createAndBuild(vkd, device, commandBuffer, allocator);

	const TopLevelAccelerationStructure*				topLevelAccelerationStructurePtr		= m_topLevelAccelerationStructure.get();
	const VkWriteDescriptorSetAccelerationStructureKHR	accelerationStructureWriteDescriptorSet	= makeWriteDescriptorSetAccelerationStructureKHR(topLevelAccelerationStructurePtr->getPtr());

	DescriptorSetUpdateBuilder()
		.writeSingle(*m_descriptorSetSvc, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelerationStructureWriteDescriptorSet)
		.update(vkd, device);

	vkd.cmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *m_pipelineLayout, 2, 1, &m_descriptorSetSvc.get(), 0, DE_NULL);

	vkd.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline.get());

	cmdTraceRays(vkd,
		commandBuffer,
		&m_raygenShaderBindingTableRegion,
		&m_missShaderBindingTableRegion,
		&m_hitShaderBindingTableRegion,
		&m_callableShaderBindingTableRegion,
		m_testParams.width, m_testParams.height, 1);
}

de::MovePtr<BufferWithMemory> BindingAcceleratioStructureRayTracingTestInstance::createShaderBindingTable (const InstanceInterface&			vki,
																										   const DeviceInterface&			vkd,
																										   const VkDevice					device,
																										   const VkPhysicalDevice			physicalDevice,
																										   const VkPipeline					pipeline,
																										   Allocator&						allocator,
																										   de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
																										   const deUint32					group)
{
	de::MovePtr<BufferWithMemory>	shaderBindingTable;

	if (group < m_shaderGroupCount)
	{
		const deUint32	shaderGroupHandleSize		= getShaderGroupHandleSize(vki, physicalDevice);
		const deUint32	shaderGroupBaseAlignment	= getShaderGroupBaseAlignment(vki, physicalDevice);

		shaderBindingTable = rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, group, 1u);
	}

	return shaderBindingTable;
}


class BindingAcceleratioStructureRayTracingRayTracingTestInstance : public BindingAcceleratioStructureTestInstance
{
public:
													BindingAcceleratioStructureRayTracingRayTracingTestInstance		(Context&							context,
																													 const TestParams&					testParams);
	virtual											~BindingAcceleratioStructureRayTracingRayTracingTestInstance	() {}

	static void										checkSupport													(Context&							context,
																													 const TestParams&					testParams);
	static void										initPrograms													(SourceCollections&					programCollection,
																													 const TestParams&					testParams);

protected:
	virtual void									initPipeline													(void) override;
	virtual void									fillCommandBuffer												(VkCommandBuffer					commandBuffer) override;

	void											calcShaderGroup													(deUint32&							shaderGroupCounter,
																													 const VkShaderStageFlags			shaders1,
																													 const VkShaderStageFlags			shaders2,
																													 const VkShaderStageFlags			shaderStageFlags,
																													 deUint32&							shaderGroup,
																													 deUint32&							shaderGroupCount) const;


	de::MovePtr<BufferWithMemory>					createShaderBindingTable										(const InstanceInterface&			vki,
																													 const DeviceInterface&				vkd,
																													 const VkDevice						device,
																													 const VkPhysicalDevice				physicalDevice,
																													 const VkPipeline					pipeline,
																													 Allocator&							allocator,
																													 de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
																													 const deUint32						group,
																													 const deUint32						groupCount = 1);

	virtual deUint32								getExtraAccelerationDescriptorCount								(void) override													{ return 1; }
	virtual VkShaderStageFlags						getShaderStageFlags												(void) override													{ return ALL_RAY_TRACING_STAGES; }
	virtual VkPipelineBindPoint						getPipelineBindPoint											(void) override													{ return VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR; }

	deUint32										m_shaders;
	deUint32										m_raygenShaderGroup;
	deUint32										m_missShaderGroup;
	deUint32										m_hitShaderGroup;
	deUint32										m_callableShaderGroup;
	deUint32										m_shaderGroupCount;

	Move<VkDescriptorSetLayout>						m_descriptorSetLayoutSvc;
	Move<VkDescriptorSet>							m_descriptorSetSvc;

	de::MovePtr<RayTracingPipeline>					m_rayTracingPipeline;

	de::MovePtr<BufferWithMemory>					m_raygenShaderBindingTable;
	de::MovePtr<BufferWithMemory>					m_hitShaderBindingTable;
	de::MovePtr<BufferWithMemory>					m_missShaderBindingTable;
	de::MovePtr<BufferWithMemory>					m_callableShaderBindingTable;

	VkStridedDeviceAddressRegionKHR					m_raygenShaderBindingTableRegion;
	VkStridedDeviceAddressRegionKHR					m_missShaderBindingTableRegion;
	VkStridedDeviceAddressRegionKHR					m_hitShaderBindingTableRegion;
	VkStridedDeviceAddressRegionKHR					m_callableShaderBindingTableRegion;

	de::SharedPtr<BottomLevelAccelerationStructure>	m_bottomLevelAccelerationStructure;
	de::SharedPtr<TopLevelAccelerationStructure>	m_topLevelAccelerationStructure;
};

BindingAcceleratioStructureRayTracingRayTracingTestInstance::BindingAcceleratioStructureRayTracingRayTracingTestInstance (Context&			context,
																														  const TestParams&	testParams)
	: BindingAcceleratioStructureTestInstance	(context, testParams)
	, m_shaders									(0)
	, m_raygenShaderGroup						(~0u)
	, m_missShaderGroup							(~0u)
	, m_hitShaderGroup							(~0u)
	, m_callableShaderGroup						(~0u)
	, m_shaderGroupCount						(0)

	, m_descriptorSetLayoutSvc					()
	, m_descriptorSetSvc						()

	, m_rayTracingPipeline						()

	, m_raygenShaderBindingTable				()
	, m_hitShaderBindingTable					()
	, m_missShaderBindingTable					()
	, m_callableShaderBindingTable				()

	, m_raygenShaderBindingTableRegion			()
	, m_missShaderBindingTableRegion			()
	, m_hitShaderBindingTableRegion				()
	, m_callableShaderBindingTableRegion		()

	, m_bottomLevelAccelerationStructure		()
	, m_topLevelAccelerationStructure			()
{
}

void BindingAcceleratioStructureRayTracingRayTracingTestInstance::checkSupport (Context&			context,
																				const TestParams&	testParams)
{
	DE_UNREF(testParams);

	context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

	const VkPhysicalDeviceRayTracingPipelineFeaturesKHR&	rayTracingPipelineFeaturesKHR = context.getRayTracingPipelineFeatures();

	if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == DE_FALSE)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");
	const VkPhysicalDeviceRayTracingPipelinePropertiesKHR&	rayTracingPipelinePropertiesKHR = context.getRayTracingPipelineProperties();
	if (rayTracingPipelinePropertiesKHR.maxRayRecursionDepth < 2 && testParams.testType == TEST_TYPE_USING_RAY_TRACING && (testParams.stage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR || testParams.stage == VK_SHADER_STAGE_MISS_BIT_KHR))
		TCU_THROW(NotSupportedError, "rayTracingPipelinePropertiesKHR.maxRayRecursionDepth is smaller than required");
}

void BindingAcceleratioStructureRayTracingRayTracingTestInstance::initPrograms (SourceCollections&	programCollection,
																				const TestParams&	testParams)
{
	const vk::ShaderBuildOptions	buildOptions				(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
	const std::string				testShaderBody				= testParams.testConfigShaderBodyText(testParams);
	const std::string				testBody					=
		"  ivec3       pos      = ivec3(gl_LaunchIDEXT);\n"
		"  ivec3       size     = ivec3(gl_LaunchSizeEXT);\n"
		+ testShaderBody;
	const std::string				testOutClosestHitShader		=
		std::string(glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460)) + "\n"
		"#extension GL_EXT_ray_tracing : require\n"
		"\n"
		"hitAttributeEXT vec3 attribs;\n"
		"layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
		"layout(set = 1, binding = 0, r32i) uniform iimage3D result;\n"
		"\n"
		"void main()\n"
		"{\n"
		+ testBody +
		"}\n";
	const std::string				testInShaderFragment		=
		"  uint  rayFlags = 0;\n"
		"  uint  cullMask = 0xFF;\n"
		"  float tmin     = 0.0;\n"
		"  float tmax     = 9.0;\n"
		"  vec3  origin   = vec3((float(gl_LaunchIDEXT.x) + 0.5f) / float(gl_LaunchSizeEXT.x), (float(gl_LaunchIDEXT.y) + 0.5f) / float(gl_LaunchSizeEXT.y), 0.0);\n"
		"  vec3  direct   = vec3(0.0, 0.0, 1.0);\n"
		"\n"
		"  traceRayEXT(topLevelAS, rayFlags, cullMask, 1, 0, 1, origin, tmin, direct, tmax, 0);\n";
	const std::string				commonRayGenerationShader	=
		std::string(glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460)) + "\n"
		"#extension GL_EXT_ray_tracing : require\n"
		"\n"
		"layout(location = 0) rayPayloadEXT vec3 hitValue;\n"
		"layout(set = 1, binding = 0, r32i) uniform iimage3D result;\n"
		"layout(set = 2, binding = 0) uniform accelerationStructureEXT topLevelAS;\n"
		"\n"
		"void main()\n"
		"{\n"
		"  uint  rayFlags = 0;\n"
		"  uint  cullMask = 0xFF;\n"
		"  float tmin     = 0.0;\n"
		"  float tmax     = 9.0;\n"
		"  vec3  origin   = vec3((float(gl_LaunchIDEXT.x) + 0.5f) / float(gl_LaunchSizeEXT.x), (float(gl_LaunchIDEXT.y) + 0.5f) / float(gl_LaunchSizeEXT.y), 0.0);\n"
		"  vec3  direct   = vec3(0.0, 0.0, -1.0);\n"
		"\n"
		"  traceRayEXT(topLevelAS, rayFlags, cullMask, 0, 0, 0, origin, tmin, direct, tmax, 0);\n"
		"}\n";

	programCollection.glslSources.add("chit0") << glu::ClosestHitSource(testOutClosestHitShader) << buildOptions;
	programCollection.glslSources.add("ahit0") << glu::AnyHitSource(getHitPassthrough()) << buildOptions;
	programCollection.glslSources.add("miss0") << glu::MissSource(getMissPassthrough()) << buildOptions;

	switch (testParams.stage)
	{
		case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
		{
			{
				std::stringstream css;
				css << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_ray_tracing : require\n"
					<< "\n"
					<< "layout(location = 0) rayPayloadEXT vec3 hitValue;\n"
					<< "layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;\n"
					<< "layout(set = 1, binding = 0, r32i) uniform iimage3D result;\n"
					<< "\n"
					<< "void main()\n"
					<< "{\n"
					<< testInShaderFragment
					<< "}\n";

				programCollection.glslSources.add("rgen") << glu::RaygenSource(css.str()) << buildOptions;
			}

			programCollection.glslSources.add("chit") << glu::ClosestHitSource(getHitPassthrough()) << buildOptions;
			programCollection.glslSources.add("ahit") << glu::AnyHitSource(getHitPassthrough()) << buildOptions;
			programCollection.glslSources.add("miss") << glu::MissSource(getMissPassthrough()) << buildOptions;

			break;
		}

		case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
		{
			programCollection.glslSources.add("rgen") << glu::RaygenSource(commonRayGenerationShader) << buildOptions;

			{
				std::stringstream css;
				css << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_ray_tracing : require\n"
					<< "\n"
					<< "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
					<< "hitAttributeEXT vec3 attribs;\n"
					<< "\n"
					<< "layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;\n"
					<< "layout(set = 1, binding = 0, r32i) uniform iimage3D result;\n"
					<< "\n"
					<< "void main()\n"
					<< "{\n"
					<< testInShaderFragment
					<< "}\n";

				programCollection.glslSources.add("chit") << glu::ClosestHitSource(css.str()) << buildOptions;
			}

			programCollection.glslSources.add("ahit") << glu::AnyHitSource(getHitPassthrough()) << buildOptions;
			programCollection.glslSources.add("miss") << glu::MissSource(getMissPassthrough()) << buildOptions;

			break;
		}

		case VK_SHADER_STAGE_MISS_BIT_KHR:
		{
			programCollection.glslSources.add("rgen") << glu::RaygenSource(commonRayGenerationShader) << buildOptions;

			{
				std::stringstream css;
				css << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_460) << "\n"
					<< "#extension GL_EXT_ray_tracing : require\n"
					<< "layout(location = 0) rayPayloadInEXT vec3 hitValue;\n"
					<< "\n"
					<< "layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;\n"
					<< "layout(set = 1, binding = 0, r32i) uniform iimage3D result;\n"
					<< "\n"
					<< "void main()\n"
					<< "{\n"
					<< testInShaderFragment
					<< "}\n";

				programCollection.glslSources.add("miss") << glu::MissSource(css.str()) << buildOptions;
			}

			programCollection.glslSources.add("ahit") << glu::AnyHitSource(getHitPassthrough()) << buildOptions;
			programCollection.glslSources.add("chit") << glu::ClosestHitSource(getHitPassthrough()) << buildOptions;

			break;
		}

		default:
			TCU_THROW(InternalError, "Unknown stage");
	}
}

void BindingAcceleratioStructureRayTracingRayTracingTestInstance::calcShaderGroup (deUint32&				shaderGroupCounter,
																				   const VkShaderStageFlags	shaders1,
																				   const VkShaderStageFlags	shaders2,
																				   const VkShaderStageFlags	shaderStageFlags,
																				   deUint32&				shaderGroup,
																				   deUint32&				shaderGroupCount) const
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

void BindingAcceleratioStructureRayTracingRayTracingTestInstance::initPipeline (void)
{
	const InstanceInterface&	vki						= m_context.getInstanceInterface();
	const DeviceInterface&		vkd						= m_context.getDeviceInterface();
	const VkDevice				device					= m_context.getDevice();
	const VkPhysicalDevice		physicalDevice			= m_context.getPhysicalDevice();
	vk::BinaryCollection&		collection				= m_context.getBinaryCollection();
	Allocator&					allocator				= m_context.getDefaultAllocator();
	const deUint32				shaderGroupHandleSize	= getShaderGroupHandleSize(vki, physicalDevice);
	const VkShaderStageFlags	hitStages				= VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
	deUint32					shaderCount				= 0;
	VkShaderStageFlags			shaders0				= static_cast<VkShaderStageFlags>(0);
	deUint32					raygenShaderGroupCount	= 0;
	deUint32					hitShaderGroupCount		= 0;
	deUint32					missShaderGroupCount	= 0;

	if (collection.contains("rgen")) m_shaders |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	if (collection.contains("ahit")) m_shaders |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	if (collection.contains("chit")) m_shaders |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	if (collection.contains("miss")) m_shaders |= VK_SHADER_STAGE_MISS_BIT_KHR;

	if (collection.contains("ahit0")) shaders0 |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	if (collection.contains("chit0")) shaders0 |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	if (collection.contains("miss0")) shaders0 |= VK_SHADER_STAGE_MISS_BIT_KHR;

	for (BinaryCollection::Iterator it = collection.begin(); it != collection.end(); ++it)
		shaderCount++;

	if (shaderCount != (deUint32)(dePop32(m_shaders) + dePop32(shaders0)))
		TCU_THROW(InternalError, "Unused shaders detected in the collection");

	calcShaderGroup(m_shaderGroupCount, m_shaders, shaders0, VK_SHADER_STAGE_RAYGEN_BIT_KHR, m_raygenShaderGroup, raygenShaderGroupCount);
	calcShaderGroup(m_shaderGroupCount, m_shaders, shaders0, VK_SHADER_STAGE_MISS_BIT_KHR,   m_missShaderGroup,   missShaderGroupCount);
	calcShaderGroup(m_shaderGroupCount, m_shaders, shaders0, hitStages,                      m_hitShaderGroup,    hitShaderGroupCount);

	m_rayTracingPipeline		= de::newMovePtr<RayTracingPipeline>();

	m_descriptorSetLayoutSvc	= DescriptorSetLayoutBuilder()
									.addSingleBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, ALL_RAY_TRACING_STAGES)
									.build(vkd, device);
	m_descriptorSetSvc			= makeDescriptorSet(vkd, device, *m_descriptorPool, *m_descriptorSetLayoutSvc);

	if (0 != (m_shaders & VK_SHADER_STAGE_RAYGEN_BIT_KHR))			m_rayTracingPipeline->addShader(VK_SHADER_STAGE_RAYGEN_BIT_KHR			, createShaderModule(vkd, device, collection.get("rgen"), 0), m_raygenShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_ANY_HIT_BIT_KHR))			m_rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR			, createShaderModule(vkd, device, collection.get("ahit"), 0), m_hitShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR))		m_rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR		, createShaderModule(vkd, device, collection.get("chit"), 0), m_hitShaderGroup);
	if (0 != (m_shaders & VK_SHADER_STAGE_MISS_BIT_KHR))			m_rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR			, createShaderModule(vkd, device, collection.get("miss"), 0), m_missShaderGroup);

	// The "chit" and "miss" cases both generate more rays from their shaders.
	if (m_testParams.testType == TEST_TYPE_USING_RAY_TRACING && (m_testParams.stage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR || m_testParams.stage == VK_SHADER_STAGE_MISS_BIT_KHR))
		m_rayTracingPipeline->setMaxRecursionDepth(2u);

	if (0 != (shaders0 & VK_SHADER_STAGE_ANY_HIT_BIT_KHR))			m_rayTracingPipeline->addShader(VK_SHADER_STAGE_ANY_HIT_BIT_KHR			, createShaderModule(vkd, device, collection.get("ahit0"), 0), m_hitShaderGroup + 1);
	if (0 != (shaders0 & VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR))		m_rayTracingPipeline->addShader(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR		, createShaderModule(vkd, device, collection.get("chit0"), 0), m_hitShaderGroup + 1);
	if (0 != (shaders0 & VK_SHADER_STAGE_MISS_BIT_KHR))				m_rayTracingPipeline->addShader(VK_SHADER_STAGE_MISS_BIT_KHR			, createShaderModule(vkd, device, collection.get("miss0"), 0), m_missShaderGroup + 1);

	m_pipelineLayout					= makePipelineLayout(vkd, device, m_descriptorSetLayoutAS.get(), m_descriptorSetLayoutImg.get(), m_descriptorSetLayoutSvc.get());
	m_pipeline							= m_rayTracingPipeline->createPipeline(vkd, device, *m_pipelineLayout);

	m_raygenShaderBindingTable			= createShaderBindingTable(vki, vkd, device, physicalDevice, *m_pipeline, allocator, m_rayTracingPipeline, m_raygenShaderGroup,   raygenShaderGroupCount);
	m_missShaderBindingTable			= createShaderBindingTable(vki, vkd, device, physicalDevice, *m_pipeline, allocator, m_rayTracingPipeline, m_missShaderGroup,     missShaderGroupCount);
	m_hitShaderBindingTable				= createShaderBindingTable(vki, vkd, device, physicalDevice, *m_pipeline, allocator, m_rayTracingPipeline, m_hitShaderGroup,      hitShaderGroupCount);

	m_raygenShaderBindingTableRegion	= makeStridedDeviceAddressRegion(vkd, device, getVkBuffer(m_raygenShaderBindingTable),	shaderGroupHandleSize, raygenShaderGroupCount);
	m_missShaderBindingTableRegion		= makeStridedDeviceAddressRegion(vkd, device, getVkBuffer(m_missShaderBindingTable),	shaderGroupHandleSize, missShaderGroupCount);
	m_hitShaderBindingTableRegion		= makeStridedDeviceAddressRegion(vkd, device, getVkBuffer(m_hitShaderBindingTable),		shaderGroupHandleSize, hitShaderGroupCount);
	m_callableShaderBindingTableRegion	= makeStridedDeviceAddressRegion(vkd, device, DE_NULL, 0, 0);
}

void BindingAcceleratioStructureRayTracingRayTracingTestInstance::fillCommandBuffer (VkCommandBuffer	commandBuffer)
{
	const DeviceInterface&							vkd									= m_context.getDeviceInterface();
	const VkDevice									device								= m_context.getDevice();
	Allocator&										allocator							= m_context.getDefaultAllocator();
	de::MovePtr<BottomLevelAccelerationStructure>	bottomLevelAccelerationStructure	= makeBottomLevelAccelerationStructure();
	de::MovePtr<TopLevelAccelerationStructure>		topLevelAccelerationStructure		= makeTopLevelAccelerationStructure();

	m_bottomLevelAccelerationStructure = de::SharedPtr<BottomLevelAccelerationStructure>(bottomLevelAccelerationStructure.release());
	m_bottomLevelAccelerationStructure->setDefaultGeometryData(m_testParams.stage);
	m_bottomLevelAccelerationStructure->createAndBuild(vkd, device, commandBuffer, allocator);

	m_topLevelAccelerationStructure = de::SharedPtr<TopLevelAccelerationStructure>(topLevelAccelerationStructure.release());
	m_topLevelAccelerationStructure->setInstanceCount(1);
	m_topLevelAccelerationStructure->addInstance(m_bottomLevelAccelerationStructure);
	m_topLevelAccelerationStructure->createAndBuild(vkd, device, commandBuffer, allocator);

	const TopLevelAccelerationStructure*				topLevelAccelerationStructurePtr		= m_topLevelAccelerationStructure.get();
	const VkWriteDescriptorSetAccelerationStructureKHR	accelerationStructureWriteDescriptorSet	= makeWriteDescriptorSetAccelerationStructureKHR(topLevelAccelerationStructurePtr->getPtr());

	DescriptorSetUpdateBuilder()
		.writeSingle(*m_descriptorSetSvc, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &accelerationStructureWriteDescriptorSet)
		.update(vkd, device);

	vkd.cmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *m_pipelineLayout, 2, 1, &m_descriptorSetSvc.get(), 0, DE_NULL);

	vkd.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline.get());

	cmdTraceRays(vkd,
		commandBuffer,
		&m_raygenShaderBindingTableRegion,
		&m_missShaderBindingTableRegion,
		&m_hitShaderBindingTableRegion,
		&m_callableShaderBindingTableRegion,
		m_testParams.width, m_testParams.height, 1);
}

de::MovePtr<BufferWithMemory> BindingAcceleratioStructureRayTracingRayTracingTestInstance::createShaderBindingTable (const InstanceInterface&			vki,
																													 const DeviceInterface&				vkd,
																													 const VkDevice						device,
																													 const VkPhysicalDevice				physicalDevice,
																													 const VkPipeline					pipeline,
																													 Allocator&							allocator,
																													 de::MovePtr<RayTracingPipeline>&	rayTracingPipeline,
																													 const deUint32						group,
																													 const deUint32						groupCount)
{
	de::MovePtr<BufferWithMemory>	shaderBindingTable;

	if (group < m_shaderGroupCount)
	{
		const deUint32	shaderGroupHandleSize		= getShaderGroupHandleSize(vki, physicalDevice);
		const deUint32	shaderGroupBaseAlignment	= getShaderGroupBaseAlignment(vki, physicalDevice);

		shaderBindingTable = rayTracingPipeline->createShaderBindingTable(vkd, device, pipeline, allocator, shaderGroupHandleSize, shaderGroupBaseAlignment, group, groupCount);
	}

	return shaderBindingTable;
}


const std::string getRayQueryShaderBodyText (const TestParams& testParams)
{
	DE_UNREF(testParams);

	const std::string result =
		"  const float mult     = " + de::toString(FIXED_POINT_DIVISOR) + ".0f;\n"
		"  uint        rayFlags = 0;\n"
		"  uint        cullMask = 0xFF;\n"
		"  float       tmin     = 0.0;\n"
		"  float       tmax     = 9.0;\n"
		"  vec3        origin   = vec3((float(pos.x) + 0.5f) / float(size.x), (float(pos.y) + 0.5f) / float(size.y), 0.0);\n"
		"  vec3        direct   = vec3(0.0, 0.0, 1.0);\n"
		"  int         value    = 0;\n"
		"  rayQueryEXT rayQuery;\n"
		"\n"
		"  rayQueryInitializeEXT(rayQuery, tlas, rayFlags, cullMask, origin, tmin, direct, tmax);\n"
		"\n"
		"  while(rayQueryProceedEXT(rayQuery))\n"
		"  {\n"
		"    if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT)\n"
		"    {\n"
		"      const float t = rayQueryGetIntersectionTEXT(rayQuery, false);"
		"\n"
		"      value = int(round(mult * t));\n"
		"    }\n"
		"  }\n"
		"\n"
		"  imageStore(result, pos, ivec4(value, 0, 0, 0));\n";

	return result;
}

const std::string getRayTracingShaderBodyText (const TestParams& testParams)
{
	DE_UNREF(testParams);

	const std::string result =
		"  const float mult     = " + de::toString(FIXED_POINT_DIVISOR) + ".0f;\n"
		"  int         value    = int(round(mult * gl_HitTEXT));\n"
		"\n"
		"  imageStore(result, pos, ivec4(value, 0, 0, 0));\n";

	return result;
}

class BindingAccelerationStructureTestCase : public TestCase
{
	public:
							BindingAccelerationStructureTestCase	(tcu::TestContext& context, const char* name, const char* desc, const TestParams testParams);
							~BindingAccelerationStructureTestCase	(void);

	virtual void			checkSupport							(Context& context) const;
	virtual	void			initPrograms							(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance							(Context& context) const;

private:
	TestParams				m_testParams;
};

BindingAccelerationStructureTestCase::BindingAccelerationStructureTestCase (tcu::TestContext& context, const char* name, const char* desc, const TestParams testParams)
	: vkt::TestCase	(context, name, desc)
	, m_testParams		(testParams)
{
}

BindingAccelerationStructureTestCase::~BindingAccelerationStructureTestCase (void)
{
}

void BindingAccelerationStructureTestCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_acceleration_structure");

	const VkPhysicalDeviceAccelerationStructureFeaturesKHR&	accelerationStructureFeaturesKHR	= context.getAccelerationStructureFeatures();
	if (accelerationStructureFeaturesKHR.accelerationStructure == DE_FALSE)
		TCU_THROW(TestError, "Requires VkPhysicalDeviceAccelerationStructureFeaturesKHR.accelerationStructure");

	switch (m_testParams.testType)
	{
		case TEST_TYPE_USING_RAY_QUERY:
		{
			context.requireDeviceFunctionality("VK_KHR_ray_query");

			const VkPhysicalDeviceRayQueryFeaturesKHR&	rayQueryFeaturesKHR	= context.getRayQueryFeatures();

			if (rayQueryFeaturesKHR.rayQuery == DE_FALSE)
				TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayQueryFeaturesKHR.rayQuery");

			break;
		}

		case TEST_TYPE_USING_RAY_TRACING:
		{
			context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

			const VkPhysicalDeviceRayTracingPipelineFeaturesKHR&	rayTracingPipelineFeaturesKHR = context.getRayTracingPipelineFeatures();

			if (rayTracingPipelineFeaturesKHR.rayTracingPipeline == DE_FALSE)
				TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceRayTracingPipelineFeaturesKHR.rayTracingPipeline");

			break;
		}

		default:
			TCU_THROW(InternalError, "Unknown test type");
	}

	switch (m_testParams.updateMethod)
	{
		case UPDATE_METHOD_NORMAL:
		{
			break;
		}

		case UPDATE_METHOD_WITH_TEMPLATE:
		{
			context.requireDeviceFunctionality("VK_KHR_descriptor_update_template");

			break;
		}

		case UPDATE_METHOD_WITH_PUSH:
		{
			context.requireDeviceFunctionality("VK_KHR_push_descriptor");

			break;
		}

		case UPDATE_METHOD_WITH_PUSH_TEMPLATE:
		{
			context.requireDeviceFunctionality("VK_KHR_push_descriptor");
			context.requireDeviceFunctionality("VK_KHR_descriptor_update_template");

			break;
		}

		default:
			TCU_THROW(InternalError, "Unknown update method");
	}

	m_testParams.pipelineCheckSupport(context, m_testParams);
}

TestInstance* BindingAccelerationStructureTestCase::createInstance (Context& context) const
{
	switch (m_testParams.testType)
	{
		case TEST_TYPE_USING_RAY_QUERY:
		{
			switch (m_testParams.stage)
			{
				case VK_SHADER_STAGE_VERTEX_BIT:
				case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
				case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
				case VK_SHADER_STAGE_GEOMETRY_BIT:
				case VK_SHADER_STAGE_FRAGMENT_BIT:
				{
					return new BindingAcceleratioStructureGraphicsTestInstance(context, m_testParams);
				}

				case VK_SHADER_STAGE_COMPUTE_BIT:
				{
					return new BindingAcceleratioStructureComputeTestInstance(context, m_testParams);
				}

				case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
				case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
				case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
				case VK_SHADER_STAGE_MISS_BIT_KHR:
				case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
				case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
				{
					return new BindingAcceleratioStructureRayTracingTestInstance(context, m_testParams);
				}

				default:
					TCU_THROW(InternalError, "Unknown shader stage");
			}
		}

		case TEST_TYPE_USING_RAY_TRACING:
		{
			return new BindingAcceleratioStructureRayTracingRayTracingTestInstance(context, m_testParams);
		}

		default:
			TCU_THROW(InternalError, "Unknown shader stage");
	}
}

void BindingAccelerationStructureTestCase::initPrograms (SourceCollections& programCollection) const
{
	m_testParams.pipelineInitPrograms(programCollection, m_testParams);
}

static inline CheckSupportFunc getPipelineRayQueryCheckSupport (const VkShaderStageFlagBits stage)
{
	switch (stage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
		case VK_SHADER_STAGE_GEOMETRY_BIT:
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return BindingAcceleratioStructureGraphicsTestInstance::checkSupport;

		case VK_SHADER_STAGE_COMPUTE_BIT:
			return BindingAcceleratioStructureComputeTestInstance::checkSupport;

		case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
		case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
		case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
		case VK_SHADER_STAGE_MISS_BIT_KHR:
		case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
		case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
			return BindingAcceleratioStructureRayTracingTestInstance::checkSupport;

		default:
			TCU_THROW(InternalError, "Unknown shader stage");
	}
}

static inline CheckSupportFunc getPipelineRayTracingCheckSupport (const VkShaderStageFlagBits stage)
{
	DE_UNREF(stage);

	return BindingAcceleratioStructureRayTracingRayTracingTestInstance::checkSupport;
}

static inline InitProgramsFunc getPipelineRayQueryInitPrograms (const VkShaderStageFlagBits stage)
{
	switch (stage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
		case VK_SHADER_STAGE_GEOMETRY_BIT:
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return BindingAcceleratioStructureGraphicsTestInstance::initPrograms;

		case VK_SHADER_STAGE_COMPUTE_BIT:
			return BindingAcceleratioStructureComputeTestInstance::initPrograms;

		case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
		case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
		case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
		case VK_SHADER_STAGE_MISS_BIT_KHR:
		case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
		case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
			return BindingAcceleratioStructureRayTracingTestInstance::initPrograms;

		default:
			TCU_THROW(InternalError, "Unknown shader stage");
	}
}

static inline InitProgramsFunc getPipelineRayTracingInitPrograms (const VkShaderStageFlagBits stage)
{
	DE_UNREF(stage);

	return BindingAcceleratioStructureRayTracingRayTracingTestInstance::initPrograms;
}

static inline ShaderBodyTextFunc getShaderBodyTextFunc (const TestType testType)
{
	switch (testType)
	{
		case TEST_TYPE_USING_RAY_QUERY:		return getRayQueryShaderBodyText;
		case TEST_TYPE_USING_RAY_TRACING:	return getRayTracingShaderBodyText;
		default:							TCU_THROW(InternalError, "Unknown test type");
	}
}

}	// anonymous

tcu::TestCaseGroup* createDescriptorUpdateASTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group	(new tcu::TestCaseGroup(testCtx, "acceleration_structure", "Tests acceleration structure descriptor updates"));

	const struct TestTypes
	{
		TestType	testType;
		const char*	name;
	}
	testTypes[] =
	{
		{ TEST_TYPE_USING_RAY_QUERY,	"ray_query"		},
		{ TEST_TYPE_USING_RAY_TRACING,	"ray_tracing"	},
	};
	const struct UpdateMethods
	{
		const UpdateMethod	method;
		const char*			name;
		const char*			description;
	}
	updateMethods[] =
	{
		{ UPDATE_METHOD_NORMAL,				"regular",				"Use regular descriptor updates"		},
		{ UPDATE_METHOD_WITH_TEMPLATE,		"with_template",		"Use descriptor update templates"		},
		{ UPDATE_METHOD_WITH_PUSH,			"with_push",			"Use push descriptor updates"			},
		{ UPDATE_METHOD_WITH_PUSH_TEMPLATE,	"with_push_template",	"Use push descriptor update templates"	},
	};
	const struct PipelineStages
	{
		VkShaderStageFlagBits	stage;
		const char*				name;
		const bool				rayTracing;
	}
	pipelineStages[] =
	{
		{ VK_SHADER_STAGE_VERTEX_BIT,					"vert",	false	},
		{ VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,		"tesc",	false	},
		{ VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,	"tese",	false	},
		{ VK_SHADER_STAGE_GEOMETRY_BIT,					"geom",	false	},
		{ VK_SHADER_STAGE_FRAGMENT_BIT,					"frag",	false	},
		{ VK_SHADER_STAGE_COMPUTE_BIT,					"comp",	false	},
		{ VK_SHADER_STAGE_RAYGEN_BIT_KHR,				"rgen",	true	},
		{ VK_SHADER_STAGE_ANY_HIT_BIT_KHR,				"ahit",	false	},
		{ VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,			"chit",	true	},
		{ VK_SHADER_STAGE_MISS_BIT_KHR,					"miss",	true	},
		{ VK_SHADER_STAGE_INTERSECTION_BIT_KHR,			"sect",	false	},
		{ VK_SHADER_STAGE_CALLABLE_BIT_KHR,				"call",	false	},
	};

	for (size_t testTypeNdx = 0; testTypeNdx < DE_LENGTH_OF_ARRAY(testTypes); ++testTypeNdx)
	{
		de::MovePtr<tcu::TestCaseGroup>	testTypeGroup		(new tcu::TestCaseGroup(group->getTestContext(), testTypes[testTypeNdx].name, ""));
		const TestType					testType			= testTypes[testTypeNdx].testType;
		const ShaderBodyTextFunc		shaderBodyTextFunc	= getShaderBodyTextFunc(testType);
		const deUint32					imageDepth			= 1;

		for (size_t updateMethodsNdx = 0; updateMethodsNdx < DE_LENGTH_OF_ARRAY(updateMethods); ++updateMethodsNdx)
		{
			de::MovePtr<tcu::TestCaseGroup>	updateMethodsGroup		(new tcu::TestCaseGroup(group->getTestContext(), updateMethods[updateMethodsNdx].name, updateMethods[updateMethodsNdx].description));
			const UpdateMethod				updateMethod			= updateMethods[updateMethodsNdx].method;

			for (size_t pipelineStageNdx = 0; pipelineStageNdx < DE_LENGTH_OF_ARRAY(pipelineStages); ++pipelineStageNdx)
			{
				const VkShaderStageFlagBits	stage					= pipelineStages[pipelineStageNdx].stage;
				const CheckSupportFunc		pipelineCheckSupport	= (testType == TEST_TYPE_USING_RAY_QUERY)
																	? getPipelineRayQueryCheckSupport(stage)
																	: getPipelineRayTracingCheckSupport(stage);
				const InitProgramsFunc		pipelineInitPrograms	= (testType == TEST_TYPE_USING_RAY_QUERY)
																	? getPipelineRayQueryInitPrograms(stage)
																	: getPipelineRayTracingInitPrograms(stage);

				if (testType == TEST_TYPE_USING_RAY_TRACING && !pipelineStages[pipelineStageNdx].rayTracing)
					continue;

				const TestParams	testParams	=
				{
					TEST_WIDTH,				//  deUint32				width;
					TEST_HEIGHT,			//  deUint32				height;
					imageDepth,				//  deUint32				depth;
					testType,				//  TestType				testType;
					updateMethod,			//  UpdateMethod			updateMethod;
					stage,					//  VkShaderStageFlagBits	stage;
					VK_FORMAT_R32_SINT,		//  VkFormat				format;
					pipelineCheckSupport,	//  CheckSupportFunc		pipelineCheckSupport;
					pipelineInitPrograms,	//  InitProgramsFunc		pipelineInitPrograms;
					shaderBodyTextFunc,		//  ShaderTestTextFunc		testConfigShaderBodyText;
				};

				updateMethodsGroup->addChild(new BindingAccelerationStructureTestCase(group->getTestContext(), pipelineStages[pipelineStageNdx].name, "", testParams));
			}

			testTypeGroup->addChild(updateMethodsGroup.release());
		}

		group->addChild(testTypeGroup.release());
	}

	return group.release();
}

}	// RayQuery
}	// vkt
