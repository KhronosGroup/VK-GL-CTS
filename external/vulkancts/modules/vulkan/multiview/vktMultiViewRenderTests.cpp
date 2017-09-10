/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Vulkan Multi View Render Tests
 *//*--------------------------------------------------------------------*/

#include "vktMultiViewRenderTests.hpp"
#include "vktMultiViewRenderUtil.hpp"

#include "vktTestCase.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkPrograms.hpp"
#include "vkPlatform.hpp"
#include "vkMemUtil.hpp"
#include "vkImageUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"

#include "deSharedPtr.hpp"

namespace vkt
{
namespace MultiView
{
namespace
{

using namespace vk;
using de::MovePtr;
using de::UniquePtr;
using std::vector;
using std::map;
using std::string;

enum TestType
{
	TEST_TYPE_VIEW_MASK,
	TEST_TYPE_VIEW_INDEX_IN_VERTEX,
	TEST_TYPE_VIEW_INDEX_IN_FRAGMENT,
	TEST_TYPE_VIEW_INDEX_IN_GEOMETRY,
	TEST_TYPE_VIEW_INDEX_IN_TESELLATION,
	TEST_TYPE_INPUT_ATTACHMENTS,
	TEST_TYPE_INSTANCED_RENDERING,
	TEST_TYPE_INPUT_RATE_INSTANCE,
	TEST_TYPE_DRAW_INDIRECT,
	TEST_TYPE_CLEAR_ATTACHMENTS,
	TEST_TYPE_SECONDARY_CMD_BUFFER,
	TEST_TYPE_LAST
};

struct TestParameters
{
	VkExtent3D			extent;
	vector<deUint32>	viewMasks;
	TestType			viewIndex;
};

class ImageAttachment
{
public:
				ImageAttachment	(VkDevice logicalDevice, DeviceInterface& device, Allocator& allocator, const VkExtent3D extent, VkFormat colorFormat);
	VkImageView	getImageView	(void) const
	{
		return *m_imageView;
	}
	VkImage		getImage		(void) const
	{
		return *m_image;
	}
private:
	Move<VkImage>			m_image;
	MovePtr<Allocation>		m_allocationImage;
	Move<VkImageView>		m_imageView;
};

ImageAttachment::ImageAttachment (VkDevice logicalDevice, DeviceInterface& device, Allocator& allocator, const VkExtent3D extent, VkFormat colorFormat)
{
	const VkImageSubresourceRange	colorImageSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, extent.depth);
	const VkImageCreateInfo			colorAttachmentImageInfo	= makeImageCreateInfo(VK_IMAGE_TYPE_2D, extent, colorFormat,
																VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	m_image							= createImage(device, logicalDevice, &colorAttachmentImageInfo);
	m_allocationImage				= allocator.allocate(getImageMemoryRequirements(device, logicalDevice, *m_image), MemoryRequirement::Any);
	VK_CHECK(device.bindImageMemory(logicalDevice, *m_image, m_allocationImage->getMemory(), m_allocationImage->getOffset()));
	m_imageView						= makeImageView(device, logicalDevice, *m_image, VK_IMAGE_VIEW_TYPE_2D_ARRAY, colorFormat, colorImageSubresourceRange);
}

class MultiViewRenderTestInstance : public TestInstance
{
public:
									MultiViewRenderTestInstance	(Context& context, const TestParameters& parameters);
protected:
	typedef de::SharedPtr<Unique<VkPipeline> >		PipelineSp;
	typedef de::SharedPtr<Unique<VkShaderModule> >	ShaderModuleSP;

	struct VertexData
	{
		VertexData (const tcu::Vec4 position_, const tcu::Vec4 color_)
			: position	(position_)
			, color		(color_)
		{}
		tcu::Vec4	position;
		tcu::Vec4	color;
	};

	virtual tcu::TestStatus			iterate					(void);
	virtual void					beforeDraw				(void);
	virtual void					afterDraw				(void);
	virtual void					draw					(const deUint32			subpassCount,
															 VkRenderPass			renderPass,
															 VkFramebuffer			frameBuffer,
															 vector<PipelineSp>&	pipelines);
	virtual void					createVertexData		(void);
	TestParameters					fillMissingParameters	(const TestParameters&	parameters);
	void							createVertexBuffer		(void);
	void							createMultiViewDevices	(void);
	void							createCommandBuffer		(void);
	void							madeShaderModule		(map<VkShaderStageFlagBits,ShaderModuleSP>& shaderModule, vector<VkPipelineShaderStageCreateInfo>& shaderStageParams);
	Move<VkPipeline>				makeGraphicsPipeline	(const VkRenderPass							renderPass,
															 const VkPipelineLayout						pipelineLayout,
															 const deUint32								pipelineShaderStageCount,
															 const VkPipelineShaderStageCreateInfo*		pipelineShaderStageCreate,
															 const deUint32								subpass,
															 const VkVertexInputRate					vertexInputRate = VK_VERTEX_INPUT_RATE_VERTEX);
	void							readImage				(VkImage image, const tcu::PixelBufferAccess& dst);
	bool							checkImage				(tcu::ConstPixelBufferAccess& dst);
	MovePtr<tcu::Texture2DArray>	imageData				(void);

	const TestParameters			m_parameters;
	VkFormat						m_colorFormat;
	const deUint32					m_squareCount;
	Move<VkDevice>					m_logicalDevice;
	MovePtr<DeviceInterface>		m_device;
	MovePtr<Allocator>				m_allocator;
	deUint32						m_queueFamilyIndex;
	VkQueue							m_queue;
	vector<VertexData>				m_data;
	Move<VkBuffer>					m_vertexBuffer;
	MovePtr<Allocation>				m_allocationBuffer;
	Move<VkCommandPool>				m_cmdPool;
	Move<VkCommandBuffer>			m_cmdBuffer;
	de::SharedPtr<ImageAttachment>	m_colorAttachment;
	VkBool32						m_hasMultiDrawIndirect;
};

MultiViewRenderTestInstance::MultiViewRenderTestInstance (Context& context, const TestParameters& parameters)
	: TestInstance		(context)
	, m_parameters		(fillMissingParameters(parameters))
	, m_colorFormat		(VK_FORMAT_R8G8B8A8_UNORM)
	, m_squareCount		(4u)
	,m_queueFamilyIndex	(0u)
{
	if (!de::contains(context.getDeviceExtensions().begin(), context.getDeviceExtensions().end(), "VK_KHR_multiview"))
		throw tcu::NotSupportedError("VK_KHR_multiview is not supported");

	createMultiViewDevices();

	// Color attachment
	m_colorAttachment = de::SharedPtr<ImageAttachment>(new ImageAttachment(*m_logicalDevice, *m_device, *m_allocator, m_parameters.extent, m_colorFormat));
}

tcu::TestStatus MultiViewRenderTestInstance::iterate (void)
{
	const deUint32								subpassCount				= static_cast<deUint32>(m_parameters.viewMasks.size());

	// FrameBuffer & renderPass
	Unique<VkRenderPass>						renderPass					(makeRenderPass (*m_device, *m_logicalDevice, m_colorFormat, m_parameters.viewMasks));

	vector<VkImageView>							attachments;
	attachments.push_back(m_colorAttachment->getImageView());
	Unique<VkFramebuffer>						frameBuffer					(makeFramebuffer(*m_device, *m_logicalDevice, *renderPass, attachments, m_parameters.extent.width, m_parameters.extent.height, 1u));

	// pipelineLayout
	Unique<VkPipelineLayout>					pipelineLayout				(makePipelineLayout(*m_device, *m_logicalDevice));

	// pipelines
	map<VkShaderStageFlagBits, ShaderModuleSP>	shaderModule;
	vector<PipelineSp>							pipelines(subpassCount);
	const VkVertexInputRate						vertexInputRate				= (TEST_TYPE_INPUT_RATE_INSTANCE == m_parameters.viewIndex) ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;

	{
		vector<VkPipelineShaderStageCreateInfo>	shaderStageParams;
		madeShaderModule(shaderModule, shaderStageParams);
		for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; ++subpassNdx)
			pipelines[subpassNdx] = (PipelineSp(new Unique<VkPipeline>(makeGraphicsPipeline(*renderPass, *pipelineLayout, static_cast<deUint32>(shaderStageParams.size()), shaderStageParams.data(), subpassNdx, vertexInputRate))));
	}

	createCommandBuffer();
	createVertexData();
	createVertexBuffer();

	draw(subpassCount, *renderPass, *frameBuffer, pipelines);

	{
		vector<deUint8>			pixelAccessData	(m_parameters.extent.width * m_parameters.extent.height * m_parameters.extent.depth * mapVkFormat(m_colorFormat).getPixelSize());
		tcu::PixelBufferAccess	dst				(mapVkFormat(m_colorFormat), m_parameters.extent.width, m_parameters.extent.height, m_parameters.extent.depth, pixelAccessData.data());

		readImage(m_colorAttachment->getImage(), dst);
		if (!checkImage(dst))
			return tcu::TestStatus::fail("Fail");
	}

	return tcu::TestStatus::pass("Pass");
}

void MultiViewRenderTestInstance::beforeDraw (void)
{
	const VkImageSubresourceRange	subresourceRange		=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,	//VkImageAspectFlags	aspectMask;
		0u,							//deUint32				baseMipLevel;
		1u,							//deUint32				levelCount;
		0u,							//deUint32				baseArrayLayer;
		m_parameters.extent.depth,	//deUint32				layerCount;
	};
	imageBarrier(*m_device, *m_cmdBuffer, m_colorAttachment->getImage(), subresourceRange, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT);

	const VkClearValue renderPassClearValue = makeClearValueColor(tcu::Vec4(0.0f));
	m_device->cmdClearColorImage(*m_cmdBuffer, m_colorAttachment->getImage(),  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &renderPassClearValue.color, 1, &subresourceRange);

	imageBarrier(*m_device, *m_cmdBuffer, m_colorAttachment->getImage(), subresourceRange, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

}

void MultiViewRenderTestInstance::afterDraw (void)
{
	const VkImageSubresourceRange	subresourceRange		=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,	//VkImageAspectFlags	aspectMask;
		0u,							//deUint32				baseMipLevel;
		1u,							//deUint32				levelCount;
		0u,							//deUint32				baseArrayLayer;
		m_parameters.extent.depth,	//deUint32				layerCount;
	};

	imageBarrier(*m_device, *m_cmdBuffer, m_colorAttachment->getImage(),
		subresourceRange, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
}

void MultiViewRenderTestInstance::draw (const deUint32 subpassCount,VkRenderPass renderPass, VkFramebuffer frameBuffer, vector<PipelineSp>& pipelines)
{
	const VkRect2D					renderArea				= { { 0, 0 }, { m_parameters.extent.width, m_parameters.extent.height } };
	const VkClearValue				renderPassClearValue	= makeClearValueColor(tcu::Vec4(0.0f));
	const VkDeviceSize				vertexBufferOffset		= 0u;
	const deUint32					drawCountPerSubpass		= (subpassCount == 1) ? m_squareCount : 1u;

	const VkRenderPassBeginInfo		renderPassBeginInfo		=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		renderPass,									// VkRenderPass			renderPass;
		frameBuffer,								// VkFramebuffer		framebuffer;
		renderArea,									// VkRect2D				renderArea;
		1u,											// uint32_t				clearValueCount;
		&renderPassClearValue,						// const VkClearValue*	pClearValues;
	};

	beginCommandBuffer(*m_device, *m_cmdBuffer);

	beforeDraw();

	m_device->cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	m_device->cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &(*m_vertexBuffer), &vertexBufferOffset);

	for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; subpassNdx++)
	{
		m_device->cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **pipelines[subpassNdx]);

		for (deUint32 drawNdx = 0u; drawNdx < drawCountPerSubpass; ++drawNdx)
			m_device->cmdDraw(*m_cmdBuffer, 4u, 1u, (drawNdx + subpassNdx % m_squareCount) * 4u, 0u);

		if (subpassNdx < subpassCount - 1u)
			m_device->cmdNextSubpass(*m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
	}

	m_device->cmdEndRenderPass(*m_cmdBuffer);

	afterDraw();

	VK_CHECK(m_device->endCommandBuffer(*m_cmdBuffer));
	submitCommandsAndWait(*m_device, *m_logicalDevice, m_queue, *m_cmdBuffer);
}

void MultiViewRenderTestInstance::createVertexData (void)
{
	tcu::Vec4 color = tcu::Vec4(0.2f, 0.0f, 0.1f, 1.0f);
	m_data.push_back(VertexData(tcu::Vec4(-1.0f,-1.0f, 1.0f, 1.0f), color));
	m_data.push_back(VertexData(tcu::Vec4(-1.0f, 0.0f, 1.0f, 1.0f), color));
	m_data.push_back(VertexData(tcu::Vec4( 0.0f,-1.0f, 1.0f, 1.0f), color));
	m_data.push_back(VertexData(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), color));

	color = tcu::Vec4(0.3f, 0.0f, 0.2f, 1.0f);
	m_data.push_back(VertexData(tcu::Vec4(-1.0f, 0.0f, 1.0f, 1.0f), color));
	m_data.push_back(VertexData(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), color));
	m_data.push_back(VertexData(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), color));
	m_data.push_back(VertexData(tcu::Vec4( 0.0f, 1.0f, 1.0f, 1.0f), color));

	color = tcu::Vec4(0.4f, 0.2f, 0.3f, 1.0f);
	m_data.push_back(VertexData(tcu::Vec4( 0.0f,-1.0f, 1.0f, 1.0f), color));
	m_data.push_back(VertexData(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), color));
	m_data.push_back(VertexData(tcu::Vec4( 1.0f,-1.0f, 1.0f, 1.0f), color));
	m_data.push_back(VertexData(tcu::Vec4( 1.0f, 0.0f, 1.0f, 1.0f), color));

	color = tcu::Vec4(0.5f, 0.0f, 0.4f, 1.0f);
	m_data.push_back(VertexData(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), color));
	m_data.push_back(VertexData(tcu::Vec4( 0.0f, 1.0f, 1.0f, 1.0f), color));
	m_data.push_back(VertexData(tcu::Vec4( 1.0f, 0.0f, 1.0f, 1.0f), color));
	m_data.push_back(VertexData(tcu::Vec4( 1.0f, 1.0f, 1.0f, 1.0f), color));
}

TestParameters MultiViewRenderTestInstance::fillMissingParameters (const TestParameters& parameters)
{
	if (!parameters.viewMasks.empty())
		return parameters;
	else
	{
		if (!de::contains(m_context.getDeviceExtensions().begin(), m_context.getDeviceExtensions().end(), "VK_KHR_multiview"))
			throw tcu::NotSupportedError("VK_KHR_multiview is not supported");

		const InstanceInterface&	instance		= m_context.getInstanceInterface();
		const VkPhysicalDevice		physicalDevice	= m_context.getPhysicalDevice();

		VkPhysicalDeviceMultiviewPropertiesKHR multiviewProperties =
		{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES_KHR,	// VkStructureType	sType;
			DE_NULL,													// void*			pNext;
			0u,															// deUint32			maxMultiviewViewCount;
			0u															// deUint32			maxMultiviewInstanceIndex;
		};

		VkPhysicalDeviceProperties2KHR deviceProperties2;
		deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
		deviceProperties2.pNext = &multiviewProperties;

		instance.getPhysicalDeviceProperties2KHR(physicalDevice, &deviceProperties2);

		TestParameters newParameters = parameters;
		newParameters.extent.depth = multiviewProperties.maxMultiviewViewCount;

		vector<deUint32> viewMasks(multiviewProperties.maxMultiviewViewCount);
		for (deUint32 i = 0; i < multiviewProperties.maxMultiviewViewCount; i++)
			viewMasks[i] = 1 << i;
		newParameters.viewMasks = viewMasks;

		return newParameters;
	}
}

void MultiViewRenderTestInstance::createVertexBuffer (void)
{
	const VkDeviceSize						vertexDataSize	= static_cast<VkDeviceSize>(deAlignSize(static_cast<size_t>( m_data.size() * sizeof(VertexData)),
															static_cast<size_t>(m_context.getDeviceProperties().limits.nonCoherentAtomSize)));
	const VkBufferCreateInfo				bufferInfo		= makeBufferCreateInfo(vertexDataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	m_vertexBuffer		= createBuffer(*m_device, *m_logicalDevice, &bufferInfo);
	m_allocationBuffer	= m_allocator->allocate(getBufferMemoryRequirements(*m_device, *m_logicalDevice, *m_vertexBuffer),  MemoryRequirement::HostVisible);

	// Init host buffer data
	VK_CHECK(m_device->bindBufferMemory(*m_logicalDevice, *m_vertexBuffer, m_allocationBuffer->getMemory(), m_allocationBuffer->getOffset()));
	deMemcpy(m_allocationBuffer->getHostPtr(), m_data.data(), static_cast<size_t>(vertexDataSize));
	flushMappedMemoryRange(*m_device, *m_logicalDevice, m_allocationBuffer->getMemory(), m_allocationBuffer->getOffset(), static_cast<size_t>(vertexDataSize));
}

void MultiViewRenderTestInstance::createMultiViewDevices (void)
{
	const InstanceInterface&				instance				= m_context.getInstanceInterface();
	const VkPhysicalDevice					physicalDevice			= m_context.getPhysicalDevice();
	const vector<VkQueueFamilyProperties>	queueFamilyProperties	= getPhysicalDeviceQueueFamilyProperties(instance, physicalDevice);

	for (; m_queueFamilyIndex < queueFamilyProperties.size(); ++m_queueFamilyIndex)
	{
		if (queueFamilyProperties[m_queueFamilyIndex].queueFlags | VK_QUEUE_GRAPHICS_BIT )
			break;
	}

	const float								queuePriorities			= 1.0f;
	const VkDeviceQueueCreateInfo			queueInfo				=
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	//VkStructureType			sType;
		DE_NULL,									//const void*				pNext;
		(VkDeviceQueueCreateFlags)0u,				//VkDeviceQueueCreateFlags	flags;
		m_queueFamilyIndex,							//deUint32					queueFamilyIndex;
		1u,											//deUint32					queueCount;
		&queuePriorities							//const float*				pQueuePriorities;
	};

	VkPhysicalDeviceMultiviewFeaturesKHR	multiviewFeatures		=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR,	// VkStructureType	sType;
		DE_NULL,													// void*			pNext;
		DE_FALSE,													// VkBool32			multiview;
		DE_FALSE,													// VkBool32			multiviewGeometryShader;
		DE_FALSE,													// VkBool32			multiviewTessellationShader;
	};

	VkPhysicalDeviceFeatures2KHR			enabledFeatures;
	enabledFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
	enabledFeatures.pNext = &multiviewFeatures;

	instance.getPhysicalDeviceFeatures2KHR(physicalDevice, &enabledFeatures);

	if (!multiviewFeatures.multiview)
		TCU_THROW(NotSupportedError, "MultiView not supported");

	bool requiresGeomShader = (TEST_TYPE_VIEW_INDEX_IN_GEOMETRY == m_parameters.viewIndex) ||
								(TEST_TYPE_INPUT_ATTACHMENTS == m_parameters.viewIndex) ||
								(TEST_TYPE_SECONDARY_CMD_BUFFER == m_parameters.viewIndex) ||
								(TEST_TYPE_CLEAR_ATTACHMENTS == m_parameters.viewIndex);

	if (requiresGeomShader && !multiviewFeatures.multiviewGeometryShader)
		TCU_THROW(NotSupportedError, "Geometry shader is not supported");

	if (TEST_TYPE_VIEW_INDEX_IN_TESELLATION == m_parameters.viewIndex && !multiviewFeatures.multiviewTessellationShader)
		TCU_THROW(NotSupportedError, "Tessellation shader is not supported");

	VkPhysicalDeviceMultiviewPropertiesKHR	multiviewProperties		=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES_KHR,	//VkStructureType	sType;
		DE_NULL,													//void*				pNext;
		0u,															//deUint32			maxMultiviewViewCount;
		0u															//deUint32			maxMultiviewInstanceIndex;
	};

	VkPhysicalDeviceProperties2KHR			propertiesDeviceProperties2;
	propertiesDeviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
	propertiesDeviceProperties2.pNext = &multiviewProperties;

	instance.getPhysicalDeviceProperties2KHR(physicalDevice, &propertiesDeviceProperties2);

	if (multiviewProperties.maxMultiviewViewCount < 6u)
		TCU_FAIL("maxMultiviewViewCount below min value");

	if (multiviewProperties.maxMultiviewInstanceIndex < 134217727u) //134217727u = 2^27 -1
		TCU_FAIL("maxMultiviewInstanceIndex below min value");

	if (multiviewProperties.maxMultiviewViewCount <m_parameters.extent.depth)
		TCU_THROW(NotSupportedError, "Limit MaxMultiviewViewCount to small to run this test");

	m_hasMultiDrawIndirect = enabledFeatures.features.multiDrawIndirect;

	{
		const vector<string>&			deviceExtensions	= m_context.getDeviceExtensions();
		vector<const char*>				charDevExtensions;

		for (std::size_t ndx = 0; ndx < deviceExtensions.size(); ++ndx)
			charDevExtensions.push_back(deviceExtensions[ndx].c_str());

		const VkDeviceCreateInfo		deviceInfo			=
		{
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,							//VkStructureType					sType;
			&enabledFeatures,												//const void*						pNext;
			0u,																//VkDeviceCreateFlags				flags;
			1u,																//deUint32							queueCreateInfoCount;
			&queueInfo,														//const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
			0u,																//deUint32							enabledLayerCount;
			DE_NULL,														//const char* const*				ppEnabledLayerNames;
			static_cast<deUint32>(deviceExtensions.size()),					//deUint32							enabledExtensionCount;
			charDevExtensions.empty() ? DE_NULL : &charDevExtensions[0],	//const char* const*				pEnabledExtensionNames;
			DE_NULL															//const VkPhysicalDeviceFeatures*	pEnabledFeatures;
		};

		m_logicalDevice	= createDevice(instance, physicalDevice, &deviceInfo);
		m_device	= MovePtr<DeviceDriver>(new DeviceDriver(instance, *m_logicalDevice));
		m_allocator		= MovePtr<Allocator>(new SimpleAllocator(*m_device, *m_logicalDevice, getPhysicalDeviceMemoryProperties(instance, physicalDevice)));
		m_device->getDeviceQueue(*m_logicalDevice, m_queueFamilyIndex, 0u, &m_queue);
	}
}

void MultiViewRenderTestInstance::createCommandBuffer (void)
{
	// cmdPool
	{
		const VkCommandPoolCreateInfo cmdPoolParams =
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,			// VkStructureType		sType;
			DE_NULL,											// const void*			pNext;
			VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,	// VkCmdPoolCreateFlags	flags;
			m_queueFamilyIndex,									// deUint32				queueFamilyIndex;
		};
		m_cmdPool = createCommandPool(*m_device, *m_logicalDevice, &cmdPoolParams);
	}

	// cmdBuffer
	{
		const VkCommandBufferAllocateInfo cmdBufferAllocateInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			*m_cmdPool,										// VkCommandPool			commandPool;
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCommandBufferLevel		level;
			1u,												// deUint32					bufferCount;
		};
		m_cmdBuffer	= allocateCommandBuffer(*m_device, *m_logicalDevice, &cmdBufferAllocateInfo);
	}
}

void MultiViewRenderTestInstance::madeShaderModule (map<VkShaderStageFlagBits, ShaderModuleSP>& shaderModule, vector<VkPipelineShaderStageCreateInfo>& shaderStageParams)
{
	// create shaders modules
	switch (m_parameters.viewIndex)
	{
		case TEST_TYPE_VIEW_MASK:
		case TEST_TYPE_VIEW_INDEX_IN_VERTEX:
		case TEST_TYPE_VIEW_INDEX_IN_FRAGMENT:
		case TEST_TYPE_INSTANCED_RENDERING:
		case TEST_TYPE_INPUT_RATE_INSTANCE:
		case TEST_TYPE_DRAW_INDIRECT:
			shaderModule[VK_SHADER_STAGE_VERTEX_BIT]					= (ShaderModuleSP(new Unique<VkShaderModule>(createShaderModule(*m_device, *m_logicalDevice, m_context.getBinaryCollection().get("vertex"), 0))));
			shaderModule[VK_SHADER_STAGE_FRAGMENT_BIT]					= (ShaderModuleSP(new Unique<VkShaderModule>(createShaderModule(*m_device, *m_logicalDevice, m_context.getBinaryCollection().get("fragment"), 0))));
			break;
		case TEST_TYPE_VIEW_INDEX_IN_GEOMETRY:
		case TEST_TYPE_INPUT_ATTACHMENTS:
		case TEST_TYPE_CLEAR_ATTACHMENTS:
		case TEST_TYPE_SECONDARY_CMD_BUFFER:
			shaderModule[VK_SHADER_STAGE_VERTEX_BIT]					= (ShaderModuleSP(new Unique<VkShaderModule>(createShaderModule(*m_device, *m_logicalDevice, m_context.getBinaryCollection().get("vertex"), 0))));
			shaderModule[VK_SHADER_STAGE_GEOMETRY_BIT]					= (ShaderModuleSP(new Unique<VkShaderModule>(createShaderModule(*m_device, *m_logicalDevice, m_context.getBinaryCollection().get("geometry"), 0))));
			shaderModule[VK_SHADER_STAGE_FRAGMENT_BIT]					= (ShaderModuleSP(new Unique<VkShaderModule>(createShaderModule(*m_device, *m_logicalDevice, m_context.getBinaryCollection().get("fragment"), 0))));
			break;
		case TEST_TYPE_VIEW_INDEX_IN_TESELLATION:
			shaderModule[VK_SHADER_STAGE_VERTEX_BIT]					= (ShaderModuleSP(new Unique<VkShaderModule>(createShaderModule(*m_device, *m_logicalDevice, m_context.getBinaryCollection().get("vertex"), 0))));
			shaderModule[VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT]		= (ShaderModuleSP(new Unique<VkShaderModule>(createShaderModule(*m_device, *m_logicalDevice, m_context.getBinaryCollection().get("tessellation_control"), 0))));
			shaderModule[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT]	= (ShaderModuleSP(new Unique<VkShaderModule>(createShaderModule(*m_device, *m_logicalDevice, m_context.getBinaryCollection().get("tessellation_evaluation"), 0))));
			shaderModule[VK_SHADER_STAGE_FRAGMENT_BIT]					= (ShaderModuleSP(new Unique<VkShaderModule>(createShaderModule(*m_device, *m_logicalDevice, m_context.getBinaryCollection().get("fragment"), 0))));
			break;
		default:
			DE_ASSERT(0);
		break;
	};

	VkPipelineShaderStageCreateInfo	pipelineShaderStage	=
	{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
			DE_NULL,												// const void*							pNext;
			(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags		flags;
			(VkShaderStageFlagBits)0,								// VkShaderStageFlagBits				stage;
			(VkShaderModule)0,										// VkShaderModule						module;
			"main",													// const char*							pName;
			(const VkSpecializationInfo*)DE_NULL,					// const VkSpecializationInfo*			pSpecializationInfo;
	};

	for (map<VkShaderStageFlagBits, ShaderModuleSP>::iterator it=shaderModule.begin(); it!=shaderModule.end(); ++it)
	{
		pipelineShaderStage.stage	= it->first;
		pipelineShaderStage.module	= **it->second;
		shaderStageParams.push_back(pipelineShaderStage);
	}
}

Move<VkPipeline> MultiViewRenderTestInstance::makeGraphicsPipeline (const VkRenderPass							renderPass,
																	const VkPipelineLayout						pipelineLayout,
																	const deUint32								pipelineShaderStageCount,
																	const VkPipelineShaderStageCreateInfo*		pipelineShaderStageCreate,
																	const deUint32								subpass,
																	const VkVertexInputRate						vertexInputRate)
{
	const VkVertexInputBindingDescription			vertexInputBindingDescription		=
	{
		0u,											// binding;
		static_cast<deUint32>(sizeof(VertexData)),	// stride;
		vertexInputRate								// inputRate
	};

	const VkVertexInputAttributeDescription			vertexInputAttributeDescriptions[]	=
	{
		{
			0u,
			0u,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			0u
		},	// VertexElementData::position
		{
			1u,
			0u,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			static_cast<deUint32>(sizeof(tcu::Vec4))
		},	// VertexElementData::color
	};

	const VkPipelineVertexInputStateCreateInfo		vertexInputStateParams				=
	{																	// sType;
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// pNext;
		NULL,															// flags;
		0u,																// vertexBindingDescriptionCount;
		1u,																// pVertexBindingDescriptions;
		&vertexInputBindingDescription,									// vertexAttributeDescriptionCount;
		2u,																// pVertexAttributeDescriptions;
		vertexInputAttributeDescriptions
	};


	const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyStateParams			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,																				// VkStructureType							sType;
		DE_NULL,																																	// const void*								pNext;
		0u,																																			// VkPipelineInputAssemblyStateCreateFlags	flags;
		(TEST_TYPE_VIEW_INDEX_IN_TESELLATION == m_parameters.viewIndex) ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,	// VkPrimitiveTopology						topology;
		VK_FALSE,																																	// VkBool32									primitiveRestartEnable;
	};

	const VkViewport								viewport						=
	{
		0.0f,								// float	originX;
		0.0f,								// float	originY;
		(float)m_parameters.extent.width,	// float	width;
		(float)m_parameters.extent.height,	// float	height;
		0.0f,								// float	minDepth;
		1.0f								// float	maxDepth;
	};

	const VkRect2D									scissor							=
	{
		{ 0, 0 },													// VkOffset2D	offset;
		{ m_parameters.extent.width, m_parameters.extent.height }	// VkExtent2D	extent;
	};

	const VkPipelineViewportStateCreateInfo		viewportStateParams						=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		0u,														// VkPipelineViewportStateCreateFlags	flags;
		1u,														// deUint32								viewportCount;
		&viewport,												// const VkViewport*					pViewports;
		1u,														// deUint32								scissorCount;
		&scissor												// const VkRect2D*						pScissors;
	};

	const VkPipelineRasterizationStateCreateInfo	rasterStateParams					=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		0u,															// VkPipelineRasterizationStateCreateFlags	flags;
		VK_FALSE,													// VkBool32									depthClampEnable;
		VK_FALSE,													// VkBool32									rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,										// VkPolygonMode							polygonMode;
		VK_CULL_MODE_NONE,											// VkCullModeFlags							cullMode;
		VK_FRONT_FACE_COUNTER_CLOCKWISE,							// VkFrontFace								frontFace;
		VK_FALSE,													// VkBool32									depthBiasEnable;
		0.0f,														// float									depthBiasConstantFactor;
		0.0f,														// float									depthBiasClamp;
		0.0f,														// float									depthBiasSlopeFactor;
		1.0f,														// float									lineWidth;
	};

	const VkPipelineMultisampleStateCreateInfo		multisampleStateParams				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		0u,															// VkPipelineMultisampleStateCreateFlags	flags;
		VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits					rasterizationSamples;
		VK_FALSE,													// VkBool32									sampleShadingEnable;
		0.0f,														// float									minSampleShading;
		DE_NULL,													// const VkSampleMask*						pSampleMask;
		VK_FALSE,													// VkBool32									alphaToCoverageEnable;
		VK_FALSE,													// VkBool32									alphaToOneEnable;
	};

	VkPipelineDepthStencilStateCreateInfo			depthStencilStateParams				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		0u,															// VkPipelineDepthStencilStateCreateFlags	flags;
		VK_TRUE,													// VkBool32									depthTestEnable;
		VK_TRUE,													// VkBool32									depthWriteEnable;
		VK_COMPARE_OP_LESS_OR_EQUAL,								// VkCompareOp								depthCompareOp;
		VK_FALSE,													// VkBool32									depthBoundsTestEnable;
		VK_FALSE,													// VkBool32									stencilTestEnable;
		// VkStencilOpState front;
		{
			VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
			VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
			0u,						// deUint32		compareMask;
			0u,						// deUint32		writeMask;
			0u,						// deUint32		reference;
		},
		// VkStencilOpState back;
		{
			VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
			VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
			0u,						// deUint32		compareMask;
			0u,						// deUint32		writeMask;
			0u,						// deUint32		reference;
		},
		0.0f,	// float	minDepthBounds;
		1.0f,	// float	maxDepthBounds;
	};

	const VkPipelineColorBlendAttachmentState		colorBlendAttachmentState			=
	{
		VK_FALSE,								// VkBool32					blendEnable;
		VK_BLEND_FACTOR_SRC_ALPHA,				// VkBlendFactor			srcColorBlendFactor;
		VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,	// VkBlendFactor			dstColorBlendFactor;
		VK_BLEND_OP_ADD,						// VkBlendOp				colorBlendOp;
		VK_BLEND_FACTOR_ONE,					// VkBlendFactor			srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ONE,					// VkBlendFactor			dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,						// VkBlendOp				alphaBlendOp;
		VK_COLOR_COMPONENT_R_BIT |				// VkColorComponentFlags	colorWriteMask;
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT
	};

	const VkPipelineColorBlendStateCreateInfo		colorBlendStateParams				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		0u,															// VkPipelineColorBlendStateCreateFlags			flags;
		VK_FALSE,													// VkBool32										logicOpEnable;
		VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
		1u,															// deUint32										attachmentCount;
		&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },									// float										blendConst[4];
	};

	VkPipelineTessellationStateCreateInfo			TessellationState					=
	{
		VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		(VkPipelineTessellationStateCreateFlags)0,					// VkPipelineTessellationStateCreateFlags	flags;
		4u															// deUint32									patchControlPoints;
	};

	const VkGraphicsPipelineCreateInfo				graphicsPipelineParams				=
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,												// VkStructureType									sType;
		DE_NULL,																						// const void*										pNext;
		(VkPipelineCreateFlags)0u,																		// VkPipelineCreateFlags							flags;
		pipelineShaderStageCount,																		// deUint32											stageCount;
		pipelineShaderStageCreate,																		// const VkPipelineShaderStageCreateInfo*			pStages;
		&vertexInputStateParams,																		// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
		&inputAssemblyStateParams,																		// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
		(TEST_TYPE_VIEW_INDEX_IN_TESELLATION == m_parameters.viewIndex)? &TessellationState : DE_NULL,	// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
		&viewportStateParams,																			// const VkPipelineViewportStateCreateInfo*			pViewportState;
		&rasterStateParams,																				// const VkPipelineRasterizationStateCreateInfo*	pRasterState;
		&multisampleStateParams,																		// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
		&depthStencilStateParams,																		// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
		&colorBlendStateParams,																			// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
		(const VkPipelineDynamicStateCreateInfo*)DE_NULL,												// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
		pipelineLayout,																					// VkPipelineLayout									layout;
		renderPass,																						// VkRenderPass										renderPass;
		subpass,																						// deUint32											subpass;
		0u,																								// VkPipeline										basePipelineHandle;
		0,																								// deInt32											basePipelineIndex;
	};

	return createGraphicsPipeline(*m_device, *m_logicalDevice, DE_NULL, &graphicsPipelineParams);
}

void MultiViewRenderTestInstance::readImage (VkImage image, const tcu::PixelBufferAccess& dst)
{
	Move<VkBuffer>				buffer;
	MovePtr<Allocation>			bufferAlloc;
	const VkDeviceSize			pixelDataSize	= dst.getWidth() * dst.getHeight() * dst.getDepth() * mapVkFormat(m_colorFormat).getPixelSize();

	// Create destination buffer
	{
		const VkBufferCreateInfo bufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u,										// VkBufferCreateFlags	flags;
			pixelDataSize,							// VkDeviceSize			size;
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,		// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
			1u,										// deUint32				queueFamilyIndexCount;
			&m_queueFamilyIndex,					// const deUint32*		pQueueFamilyIndices;
		};

		buffer		= createBuffer(*m_device, *m_logicalDevice, &bufferParams);
		bufferAlloc	= m_allocator->allocate(getBufferMemoryRequirements(*m_device, *m_logicalDevice, *buffer), MemoryRequirement::HostVisible);
		VK_CHECK(m_device->bindBufferMemory(*m_logicalDevice, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));

		deMemset(bufferAlloc->getHostPtr(), 0, static_cast<size_t>(pixelDataSize));
		flushMappedMemoryRange(*m_device, *m_logicalDevice, bufferAlloc->getMemory(), bufferAlloc->getOffset(), pixelDataSize);
	}

	const VkBufferMemoryBarrier	bufferBarrier	=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
		*buffer,									// VkBuffer			buffer;
		0u,											// VkDeviceSize		offset;
		pixelDataSize								// VkDeviceSize		size;
	};

	// Copy image to buffer
	const VkImageAspectFlags	aspect			= getAspectFlags(dst.getFormat());
	const VkBufferImageCopy		copyRegion		=
	{
		0u,										// VkDeviceSize				bufferOffset;
		(deUint32)dst.getWidth(),				// deUint32					bufferRowLength;
		(deUint32)dst.getHeight(),				// deUint32					bufferImageHeight;
		{
			aspect,								// VkImageAspectFlags		aspect;
			0u,									// deUint32					mipLevel;
			0u,									// deUint32					baseArrayLayer;
			m_parameters.extent.depth,			// deUint32					layerCount;
		},										// VkImageSubresourceLayers	imageSubresource;
		{ 0, 0, 0 },							// VkOffset3D				imageOffset;
		{ m_parameters.extent.width, m_parameters.extent.height, 1u }	// VkExtent3D				imageExtent;
	};

	beginCommandBuffer (*m_device, *m_cmdBuffer);
	{
		VkImageSubresourceRange	subresourceRange	=
		{
			aspect,						// VkImageAspectFlags	aspectMask;
			0u,							// deUint32				baseMipLevel;
			1u,							// deUint32				mipLevels;
			0u,							// deUint32				baseArraySlice;
			m_parameters.extent.depth,	// deUint32				arraySize;
		};

		imageBarrier (*m_device, *m_cmdBuffer, image, subresourceRange, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		m_device->cmdCopyImageToBuffer(*m_cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *buffer, 1u, &copyRegion);
		m_device->cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &bufferBarrier, 0u, DE_NULL);
	}
	VK_CHECK(m_device->endCommandBuffer(*m_cmdBuffer));
	submitCommandsAndWait(*m_device, *m_logicalDevice, m_queue, *m_cmdBuffer);

	// Read buffer data
	invalidateMappedMemoryRange(*m_device, *m_logicalDevice, bufferAlloc->getMemory(), bufferAlloc->getOffset(), pixelDataSize);
	tcu::copy(dst, tcu::ConstPixelBufferAccess(dst.getFormat(), dst.getSize(), bufferAlloc->getHostPtr()));
}

bool MultiViewRenderTestInstance::checkImage (tcu::ConstPixelBufferAccess& renderedFrame)
{
	const MovePtr<tcu::Texture2DArray>	referenceFrame	= imageData();

	if (tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Result", "Image comparison result", referenceFrame->getLevel(0), renderedFrame, tcu::Vec4(0.01f), tcu::COMPARE_LOG_ON_ERROR))
		return true;

	for (deUint32 layerNdx = 0u; layerNdx < m_parameters.extent.depth; layerNdx++)
	{
		tcu::ConstPixelBufferAccess ref (mapVkFormat(m_colorFormat), m_parameters.extent.width, m_parameters.extent.height, 1u, referenceFrame->getLevel(0).getPixelPtr(0, 0, layerNdx));
		tcu::ConstPixelBufferAccess dst (mapVkFormat(m_colorFormat), m_parameters.extent.width, m_parameters.extent.height, 1u, renderedFrame.getPixelPtr(0 ,0, layerNdx));
		tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Result", "Image comparison result", ref, dst, tcu::Vec4(0.01f), tcu::COMPARE_LOG_EVERYTHING);
	}

	return false;
}

MovePtr<tcu::Texture2DArray> MultiViewRenderTestInstance::imageData (void)
{
	MovePtr<tcu::Texture2DArray>	referenceFrame	= MovePtr<tcu::Texture2DArray>(new tcu::Texture2DArray(mapVkFormat(m_colorFormat), m_parameters.extent.width, m_parameters.extent.height, m_parameters.extent.depth));
	const deUint32					subpassCount	= static_cast<deUint32>(m_parameters.viewMasks.size());
	referenceFrame->allocLevel(0);

	deMemset (referenceFrame->getLevel(0).getDataPtr(), 0, m_parameters.extent.width * m_parameters.extent.height * m_parameters.extent.depth* mapVkFormat(m_colorFormat).getPixelSize());

	for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; subpassNdx++)
	{
		int			layerNdx	= 0;
		deUint32	mask		= m_parameters.viewMasks[subpassNdx];

		while (mask > 0u)
		{
			int colorNdx	= 0;
			if (mask & 1u)
			{
				if (TEST_TYPE_CLEAR_ATTACHMENTS == m_parameters.viewIndex)
				{
					struct ColorDataRGBA
					{
						deUint8	r;
						deUint8	g;
						deUint8	b;
						deUint8	a;
					};

					ColorDataRGBA	clear		=
					{
						tcu::floatToU8 (1.0f),
						tcu::floatToU8 (0.0f),
						tcu::floatToU8 (0.0f),
						tcu::floatToU8 (1.0f)
					};

					ColorDataRGBA*	dataSrc		= (ColorDataRGBA*)referenceFrame->getLevel(0).getPixelPtr(0, 0, layerNdx);
					ColorDataRGBA*	dataDes		= dataSrc + 1;
					deUint32		copySize	= 1u;
					deUint32		layerSize	= m_parameters.extent.width * m_parameters.extent.height - copySize;
					deMemcpy(dataSrc, &clear, sizeof(ColorDataRGBA));

					while (layerSize > 0)
					{
						deMemcpy(dataDes, dataSrc, copySize * sizeof(ColorDataRGBA));
						dataDes = dataDes + copySize;
						layerSize = layerSize - copySize;
						copySize = 2u * copySize;
						if (copySize >= layerSize)
							copySize = layerSize;
					}
				}

				const deUint32 subpassQuarterNdx = subpassNdx % m_squareCount;
				if (subpassQuarterNdx == 0u || TEST_TYPE_INPUT_RATE_INSTANCE == m_parameters.viewIndex)
				{
					const tcu::Vec4 color = (TEST_TYPE_VIEW_MASK == m_parameters.viewIndex) ? m_data[colorNdx].color :
											(TEST_TYPE_INSTANCED_RENDERING == m_parameters.viewIndex) ? m_data[0].color + tcu::Vec4(0.0, static_cast<float>(layerNdx) * 0.10f, 0.10f, 0.0) :
											(TEST_TYPE_INPUT_RATE_INSTANCE == m_parameters.viewIndex) ? m_data[colorNdx / 4].color + tcu::Vec4(0.0, static_cast<float>(layerNdx) * 0.10f, 0.10f, 0.0) :
											m_data[colorNdx].color + tcu::Vec4(0.0, static_cast<float>(layerNdx) * 0.10f, 0.0, 0.0);
					for (deUint32 y = 0u; y < m_parameters.extent.height/2u; ++y)
					for (deUint32 x = 0u; x < m_parameters.extent.width/2u; ++x)
							referenceFrame->getLevel(0).setPixel(color, x, y, layerNdx);
				}

				colorNdx += 4;
				if (subpassQuarterNdx == 1u || subpassCount == 1u || TEST_TYPE_INPUT_RATE_INSTANCE == m_parameters.viewIndex)
				{
					const tcu::Vec4 color = (TEST_TYPE_VIEW_MASK == m_parameters.viewIndex) ? m_data[colorNdx].color :
											(TEST_TYPE_INSTANCED_RENDERING == m_parameters.viewIndex) ? m_data[0].color + tcu::Vec4(0.0, static_cast<float>(layerNdx) * 0.10f, 0.20f, 0.0) :
											(TEST_TYPE_INPUT_RATE_INSTANCE == m_parameters.viewIndex) ? m_data[colorNdx / 4].color + tcu::Vec4(0.0, static_cast<float>(layerNdx) * 0.10f, 0.20f, 0.0) :
											m_data[colorNdx].color + tcu::Vec4(0.0, static_cast<float>(layerNdx) * 0.10f, 0.0, 0.0);
					for (deUint32 y = m_parameters.extent.height/2u; y < m_parameters.extent.height; ++y)
					for (deUint32 x = 0u; x < m_parameters.extent.width/2u; ++x)
						referenceFrame->getLevel(0).setPixel(color , x, y, layerNdx);
				}

				colorNdx += 4;
				if (subpassQuarterNdx == 2u || subpassCount == 1u || TEST_TYPE_INPUT_RATE_INSTANCE == m_parameters.viewIndex)
				{
					const tcu::Vec4 color = (TEST_TYPE_VIEW_MASK == m_parameters.viewIndex) ? m_data[colorNdx].color :
											(TEST_TYPE_INSTANCED_RENDERING == m_parameters.viewIndex) ? m_data[0].color + tcu::Vec4(0.0, static_cast<float>(layerNdx) * 0.10f, 0.30f, 0.0) :
											(TEST_TYPE_INPUT_RATE_INSTANCE == m_parameters.viewIndex) ? m_data[colorNdx / 4].color + tcu::Vec4(0.0, static_cast<float>(layerNdx) * 0.10f, 0.30f, 0.0) :
											m_data[colorNdx].color + tcu::Vec4(0.0, static_cast<float>(layerNdx) * 0.10f, 0.0, 0.0);
					for (deUint32 y = 0u; y < m_parameters.extent.height/2u; ++y)
					for (deUint32 x =  m_parameters.extent.width/2u; x < m_parameters.extent.width; ++x)
							referenceFrame->getLevel(0).setPixel(color, x, y, layerNdx);
				}

				colorNdx += 4;
				if (subpassQuarterNdx == 3u || subpassCount == 1u || TEST_TYPE_INPUT_RATE_INSTANCE == m_parameters.viewIndex)
				{
					const tcu::Vec4 color = (TEST_TYPE_VIEW_MASK == m_parameters.viewIndex) ? m_data[colorNdx].color :
											(TEST_TYPE_INSTANCED_RENDERING == m_parameters.viewIndex) ? m_data[0].color + tcu::Vec4(0.0, static_cast<float>(layerNdx) * 0.10f,  0.40f, 0.0) :
											(TEST_TYPE_INPUT_RATE_INSTANCE == m_parameters.viewIndex) ? m_data[colorNdx / 4].color + tcu::Vec4(0.0, static_cast<float>(layerNdx) * 0.10f, 0.40f, 0.0) :
											m_data[colorNdx].color + tcu::Vec4(0.0, static_cast<float>(layerNdx) * 0.10f, 0.0, 0.0);
					for (deUint32 y =  m_parameters.extent.height/2u; y < m_parameters.extent.height; ++y)
					for (deUint32 x =  m_parameters.extent.width/2u; x < m_parameters.extent.width; ++x)
							referenceFrame->getLevel(0).setPixel(color, x, y, layerNdx);
				}

				if (TEST_TYPE_CLEAR_ATTACHMENTS == m_parameters.viewIndex)
				{
					const tcu::Vec4	color	(0.0f, 0.0f, 1.0f, 1.0f);
					const int		maxY	= static_cast<int>(static_cast<float>(m_parameters.extent.height) * 0.75f);
					const int		maxX	= static_cast<int>(static_cast<float>(m_parameters.extent.width) * 0.75f);
					for (int y = static_cast<int>(m_parameters.extent.height / 4u); y < maxY; ++y)
					for (int x = static_cast<int>(m_parameters.extent.width / 4u); x < maxX; ++x)
						referenceFrame->getLevel(0).setPixel(color, x, y, layerNdx);
				}
			}

			mask = mask >> 1;
			++layerNdx;
		}
	}
	return referenceFrame;
}

class MultiViewAttachmentsTestInstance : public MultiViewRenderTestInstance
{
public:
						MultiViewAttachmentsTestInstance	(Context& context, const TestParameters& parameters);
protected:
	tcu::TestStatus		iterate								(void);
	void				beforeDraw							(void);
	void				setImageData						(VkImage image);
	de::SharedPtr<ImageAttachment>	m_inputAttachment;
	Move<VkDescriptorPool>			m_descriptorPool;
	Move<VkDescriptorSet>			m_descriptorSet;
	Move<VkDescriptorSetLayout>		m_descriptorSetLayout;
	Move<VkPipelineLayout>			m_pipelineLayout;

};

MultiViewAttachmentsTestInstance::MultiViewAttachmentsTestInstance (Context& context, const TestParameters& parameters)
	: MultiViewRenderTestInstance	(context, parameters)
{
}

tcu::TestStatus MultiViewAttachmentsTestInstance::iterate (void)
{
	const deUint32								subpassCount			= static_cast<deUint32>(m_parameters.viewMasks.size());
	// All color attachment
	m_colorAttachment	= de::SharedPtr<ImageAttachment>(new ImageAttachment(*m_logicalDevice, *m_device, *m_allocator, m_parameters.extent, m_colorFormat));
	m_inputAttachment	= de::SharedPtr<ImageAttachment>(new ImageAttachment(*m_logicalDevice, *m_device, *m_allocator, m_parameters.extent, m_colorFormat));

	// FrameBuffer & renderPass
	Unique<VkRenderPass>						renderPass				(makeRenderPassWithAttachments(*m_device, *m_logicalDevice, m_colorFormat, m_parameters.viewMasks));

	vector<VkImageView>							attachments;
	attachments.push_back(m_colorAttachment->getImageView());
	attachments.push_back(m_inputAttachment->getImageView());
	Unique<VkFramebuffer>						frameBuffer				(makeFramebuffer(*m_device, *m_logicalDevice, *renderPass, attachments, m_parameters.extent.width, m_parameters.extent.height, 1u));

	// pipelineLayout
	m_descriptorSetLayout	= makeDescriptorSetLayout(*m_device, *m_logicalDevice);
	m_pipelineLayout		= makePipelineLayout(*m_device, *m_logicalDevice, &m_descriptorSetLayout.get());

	// pipelines
	map<VkShaderStageFlagBits, ShaderModuleSP>	shaderModule;
	vector<PipelineSp>							pipelines(subpassCount);

	{
		vector<VkPipelineShaderStageCreateInfo>	shaderStageParams;
		madeShaderModule(shaderModule, shaderStageParams);
		for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; ++subpassNdx)
			pipelines[subpassNdx] = (PipelineSp(new Unique<VkPipeline>(makeGraphicsPipeline(*renderPass, *m_pipelineLayout, static_cast<deUint32>(shaderStageParams.size()), shaderStageParams.data(), subpassNdx))));
	}

	createVertexData();
	createVertexBuffer();

	createCommandBuffer();
	setImageData(m_inputAttachment->getImage());
	draw(subpassCount, *renderPass, *frameBuffer, pipelines);

	{
		vector<deUint8>			pixelAccessData	(m_parameters.extent.width * m_parameters.extent.height * m_parameters.extent.depth * mapVkFormat(m_colorFormat).getPixelSize());
		tcu::PixelBufferAccess	dst				(mapVkFormat(m_colorFormat), m_parameters.extent.width, m_parameters.extent.height, m_parameters.extent.depth, pixelAccessData.data());

		readImage (m_colorAttachment->getImage(), dst);
		if (!checkImage(dst))
			return tcu::TestStatus::fail("Fail");
	}

	return tcu::TestStatus::pass("Pass");
}

void MultiViewAttachmentsTestInstance::beforeDraw (void)
{
	const VkDescriptorPoolSize poolSize =
	{
		vk::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
		1u
	};

	const VkDescriptorPoolCreateInfo createInfo =
	{
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		DE_NULL,
		VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		1u,
		1u,
		&poolSize
	};

	m_descriptorPool = createDescriptorPool(*m_device, *m_logicalDevice, &createInfo);

	const VkDescriptorSetAllocateInfo	allocateInfo =
	{
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		DE_NULL,
		*m_descriptorPool,
		1u,
		&m_descriptorSetLayout.get()
	};

	m_descriptorSet	= vk::allocateDescriptorSet(*m_device, *m_logicalDevice, &allocateInfo);

	const VkDescriptorImageInfo	imageInfo =
	{
		(VkSampler)0,
		m_inputAttachment->getImageView(),
		VK_IMAGE_LAYOUT_GENERAL
	};

	const VkWriteDescriptorSet	write =
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	//VkStructureType				sType;
		DE_NULL,								//const void*					pNext;
		*m_descriptorSet,						//VkDescriptorSet				dstSet;
		0u,										//deUint32						dstBinding;
		0u,										//deUint32						dstArrayElement;
		1u,										//deUint32						descriptorCount;
		VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,	//VkDescriptorType				descriptorType;
		&imageInfo,								//const VkDescriptorImageInfo*	pImageInfo;
		DE_NULL,								//const VkDescriptorBufferInfo*	pBufferInfo;
		DE_NULL,								//const VkBufferView*			pTexelBufferView;
	};

	m_device->updateDescriptorSets(*m_logicalDevice, (deUint32)1u, &write, 0u, DE_NULL);

	const VkImageSubresourceRange	subresourceRange	=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,	//VkImageAspectFlags	aspectMask;
		0u,							//deUint32				baseMipLevel;
		1u,							//deUint32				levelCount;
		0u,							//deUint32				baseArrayLayer;
		m_parameters.extent.depth,	//deUint32				layerCount;
	};
	m_device->cmdBindDescriptorSets(*m_cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0u, 1u, &(*m_descriptorSet), 0u, NULL);

	imageBarrier(*m_device, *m_cmdBuffer, m_colorAttachment->getImage(), subresourceRange, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT);

	const VkClearValue renderPassClearValue = makeClearValueColor(tcu::Vec4(0.0f));
	m_device->cmdClearColorImage(*m_cmdBuffer, m_colorAttachment->getImage(),  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &renderPassClearValue.color, 1, &subresourceRange);

	imageBarrier(*m_device, *m_cmdBuffer, m_colorAttachment->getImage(), subresourceRange, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

	imageBarrier(*m_device, *m_cmdBuffer,  m_inputAttachment->getImage(), subresourceRange, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT);
}

void MultiViewAttachmentsTestInstance::setImageData (VkImage image)
{
	const MovePtr<tcu::Texture2DArray>		data		= imageData();
	Move<VkBuffer>					buffer;
	const deUint32					bufferSize	= m_parameters.extent.width * m_parameters.extent.height * m_parameters.extent.depth * tcu::getPixelSize(mapVkFormat(m_colorFormat));
	MovePtr<Allocation>				bufferAlloc;

	// Create source buffer
	{
		const VkBufferCreateInfo		bufferParams			=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			bufferSize,									// VkDeviceSize			size;
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyIndexCount;
			&m_queueFamilyIndex,						// const deUint32*		pQueueFamilyIndices;
		};

		buffer		= createBuffer(*m_device, *m_logicalDevice, &bufferParams);
		bufferAlloc = m_allocator->allocate(getBufferMemoryRequirements(*m_device, *m_logicalDevice, *buffer), MemoryRequirement::HostVisible);
		VK_CHECK(m_device->bindBufferMemory(*m_logicalDevice, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));
	}

	// Barriers for copying buffer to image
	const VkBufferMemoryBarrier				preBufferBarrier		=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,		// VkStructureType	sType;
		DE_NULL,										// const void*		pNext;
		VK_ACCESS_HOST_WRITE_BIT,						// VkAccessFlags	srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,					// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32			dstQueueFamilyIndex;
		*buffer,										// VkBuffer			buffer;
		0u,												// VkDeviceSize		offset;
		bufferSize										// VkDeviceSize		size;
	};

	const VkImageAspectFlags				formatAspect			= getAspectFlags(mapVkFormat(m_colorFormat));
	VkImageSubresourceRange					subresourceRange		=
	{												// VkImageSubresourceRange	subresourceRange;
		formatAspect,				// VkImageAspectFlags	aspect;
		0u,							// deUint32				baseMipLevel;
		1u,							// deUint32				mipLevels;
		0u,							// deUint32				baseArraySlice;
		m_parameters.extent.depth,	// deUint32				arraySize;
	};

	const VkBufferImageCopy					copyRegion				=
	{
		0u,															// VkDeviceSize				bufferOffset;
		(deUint32)data->getLevel(0).getWidth(),						// deUint32					bufferRowLength;
		(deUint32)data->getLevel(0).getHeight(),					// deUint32					bufferImageHeight;
		{
			VK_IMAGE_ASPECT_COLOR_BIT,								// VkImageAspectFlags		aspect;
			0u,														// deUint32					mipLevel;
			0u,														// deUint32					baseArrayLayer;
			m_parameters.extent.depth,								// deUint32					layerCount;
		},															// VkImageSubresourceLayers	imageSubresource;
		{ 0, 0, 0 },												// VkOffset3D				imageOffset;
		{m_parameters.extent.width, m_parameters.extent.height, 1u}	// VkExtent3D				imageExtent;
	};

	// Write buffer data
	deMemcpy(bufferAlloc->getHostPtr(), data->getLevel(0).getDataPtr(), bufferSize);
	flushMappedMemoryRange(*m_device, *m_logicalDevice, bufferAlloc->getMemory(), bufferAlloc->getOffset(), bufferSize);

	beginCommandBuffer(*m_device, *m_cmdBuffer);

	m_device->cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &preBufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	imageBarrier(*m_device, *m_cmdBuffer, image, subresourceRange,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	m_device->cmdCopyBufferToImage(*m_cmdBuffer, *buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);
	imageBarrier(*m_device, *m_cmdBuffer, image, subresourceRange,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
	VK_CHECK(m_device->endCommandBuffer(*m_cmdBuffer));

	submitCommandsAndWait(*m_device, *m_logicalDevice, m_queue, *m_cmdBuffer);
}

class MultiViewInstancedTestInstance : public MultiViewRenderTestInstance
{
public:
						MultiViewInstancedTestInstance	(Context& context, const TestParameters& parameters);
protected:
	void				createVertexData				(void);
	void				draw							(const deUint32			subpassCount,
														 VkRenderPass			renderPass,
														 VkFramebuffer			frameBuffer,
														 vector<PipelineSp>&	pipelines);
};

MultiViewInstancedTestInstance::MultiViewInstancedTestInstance (Context& context, const TestParameters& parameters)
	: MultiViewRenderTestInstance	(context, parameters)
{
}
void MultiViewInstancedTestInstance::createVertexData (void)
{
	tcu::Vec4 color = tcu::Vec4(0.2f, 0.0f, 0.1f, 1.0f);
	m_data.push_back(VertexData(tcu::Vec4(-1.0f,-1.0f, 1.0f, 1.0f), color));
	m_data.push_back(VertexData(tcu::Vec4(-1.0f, 0.0f, 1.0f, 1.0f), color));
	m_data.push_back(VertexData(tcu::Vec4( 0.0f,-1.0f, 1.0f, 1.0f), color));
	m_data.push_back(VertexData(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), color));
}

void MultiViewInstancedTestInstance::draw (const deUint32 subpassCount,VkRenderPass renderPass, VkFramebuffer frameBuffer, vector<PipelineSp>& pipelines)
{
	const VkRect2D					renderArea				= { { 0, 0 }, { m_parameters.extent.width, m_parameters.extent.height } };
	const VkClearValue				renderPassClearValue	= makeClearValueColor(tcu::Vec4(0.0f));
	const VkDeviceSize				vertexBufferOffset		= 0u;
	const deUint32					drawCountPerSubpass		= (subpassCount == 1) ? m_squareCount : 1u;

	const VkRenderPassBeginInfo		renderPassBeginInfo		=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		renderPass,									// VkRenderPass			renderPass;
		frameBuffer,								// VkFramebuffer		framebuffer;
		renderArea,									// VkRect2D				renderArea;
		1u,											// uint32_t				clearValueCount;
		&renderPassClearValue,						// const VkClearValue*	pClearValues;
	};

	beginCommandBuffer(*m_device, *m_cmdBuffer);

	beforeDraw();

	m_device->cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	m_device->cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &(*m_vertexBuffer), &vertexBufferOffset);

	for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; subpassNdx++)
	{
		m_device->cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **pipelines[subpassNdx]);

		m_device->cmdDraw(*m_cmdBuffer, 4u, drawCountPerSubpass, 0u, subpassNdx % m_squareCount);

		if (subpassNdx < subpassCount - 1u)
			m_device->cmdNextSubpass(*m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
	}

	m_device->cmdEndRenderPass(*m_cmdBuffer);

	afterDraw();

	VK_CHECK(m_device->endCommandBuffer(*m_cmdBuffer));
	submitCommandsAndWait(*m_device, *m_logicalDevice, m_queue, *m_cmdBuffer);
}

class MultiViewInputRateInstanceTestInstance : public MultiViewRenderTestInstance
{
public:
				MultiViewInputRateInstanceTestInstance	(Context& context, const TestParameters& parameters);
protected:
	void		createVertexData						(void);
	void		draw									(const deUint32			subpassCount,
														 VkRenderPass			renderPass,
														 VkFramebuffer			frameBuffer,
														 vector<PipelineSp>&	pipelines);
};

MultiViewInputRateInstanceTestInstance::MultiViewInputRateInstanceTestInstance (Context& context, const TestParameters& parameters)
	: MultiViewRenderTestInstance	(context, parameters)
{
}

void MultiViewInputRateInstanceTestInstance::createVertexData (void)
{
	tcu::Vec4 color = tcu::Vec4(0.2f, 0.0f, 0.1f, 1.0f);
	m_data.push_back(VertexData(tcu::Vec4(-1.0f,-1.0f, 1.0f, 1.0f), color));

	color = tcu::Vec4(0.3f, 0.0f, 0.2f, 1.0f);
	m_data.push_back(VertexData(tcu::Vec4(-1.0f, 0.0f, 1.0f, 1.0f), color));

	color = tcu::Vec4(0.4f, 0.2f, 0.3f, 1.0f);
	m_data.push_back(VertexData(tcu::Vec4( 0.0f,-1.0f, 1.0f, 1.0f), color));

	color = tcu::Vec4(0.5f, 0.0f, 0.4f, 1.0f);
	m_data.push_back(VertexData(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), color));
}

void MultiViewInputRateInstanceTestInstance::draw (const deUint32 subpassCount,VkRenderPass renderPass, VkFramebuffer frameBuffer, vector<PipelineSp>& pipelines)
{
	const VkRect2D					renderArea				= { { 0, 0 }, { m_parameters.extent.width, m_parameters.extent.height } };
	const VkClearValue				renderPassClearValue	= makeClearValueColor(tcu::Vec4(0.0f));
	const VkDeviceSize				vertexBufferOffset		= 0u;
	const deUint32					drawCountPerSubpass		= (subpassCount == 1) ? m_squareCount : 1u;

	const VkRenderPassBeginInfo		renderPassBeginInfo		=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		renderPass,									// VkRenderPass			renderPass;
		frameBuffer,								// VkFramebuffer		framebuffer;
		renderArea,									// VkRect2D				renderArea;
		1u,											// uint32_t				clearValueCount;
		&renderPassClearValue,						// const VkClearValue*	pClearValues;
	};

	beginCommandBuffer(*m_device, *m_cmdBuffer);

	beforeDraw();

	m_device->cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	m_device->cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &(*m_vertexBuffer), &vertexBufferOffset);

	for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; subpassNdx++)
	{
		m_device->cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **pipelines[subpassNdx]);

		for (deUint32 drawNdx = 0u; drawNdx < drawCountPerSubpass; ++drawNdx)
			m_device->cmdDraw(*m_cmdBuffer, 4u, 4u, 0u, 0u);

		if (subpassNdx < subpassCount - 1u)
			m_device->cmdNextSubpass(*m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
	}

	m_device->cmdEndRenderPass(*m_cmdBuffer);

	afterDraw();

	VK_CHECK(m_device->endCommandBuffer(*m_cmdBuffer));
	submitCommandsAndWait(*m_device, *m_logicalDevice, m_queue, *m_cmdBuffer);
}

class MultiViewIDrawIndirectTestInstance : public MultiViewRenderTestInstance
{
public:
				MultiViewIDrawIndirectTestInstance	(Context& context, const TestParameters& parameters);
protected:
	void		draw									(const deUint32			subpassCount,
														 VkRenderPass			renderPass,
														 VkFramebuffer			frameBuffer,
														 vector<PipelineSp>&	pipelines);
};

MultiViewIDrawIndirectTestInstance::MultiViewIDrawIndirectTestInstance (Context& context, const TestParameters& parameters)
	: MultiViewRenderTestInstance	(context, parameters)
{
}

void MultiViewIDrawIndirectTestInstance::draw (const deUint32 subpassCount,VkRenderPass renderPass, VkFramebuffer frameBuffer, vector<PipelineSp>& pipelines)
{
	typedef de::SharedPtr<Unique<VkBuffer> >		BufferSP;
	typedef de::SharedPtr<UniquePtr<Allocation> >	AllocationSP;

	const VkRect2D					renderArea				= { { 0, 0 }, { m_parameters.extent.width, m_parameters.extent.height } };
	const VkClearValue				renderPassClearValue	= makeClearValueColor(tcu::Vec4(0.0f));
	const VkDeviceSize				vertexBufferOffset		= 0u;
	const deUint32					drawCountPerSubpass		= (subpassCount == 1) ? m_squareCount : 1u;
	vector< BufferSP >				indirectBuffers(subpassCount);
	vector< AllocationSP >			indirectAllocations(subpassCount);
	deUint32						strideInBuffer = (deUint32)sizeof(vk::VkDrawIndirectCommand);

	for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; subpassNdx++)
	{
		vector<VkDrawIndirectCommand>	drawCommands;
		for (deUint32 drawNdx = 0u; drawNdx < drawCountPerSubpass; ++drawNdx)
		{
			const VkDrawIndirectCommand	drawCommand =
			{
				4u,												//deUint32	vertexCount;
				1u,												//deUint32	instanceCount;
				(drawNdx + subpassNdx % m_squareCount) * 4u,	//deUint32	firstVertex;
				0u												//deUint32	firstInstance;
			};
			drawCommands.push_back(drawCommand);
		}

		const VkDeviceSize			bufferSize			=	static_cast<VkDeviceSize>(deAlignSize(static_cast<size_t>(drawCommands.size() * strideInBuffer),
															static_cast<size_t>(m_context.getDeviceProperties().limits.nonCoherentAtomSize)));
		const VkBufferCreateInfo	bufferInfo			= makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
		Move<VkBuffer>				indirectBuffer		= createBuffer(*m_device, *m_logicalDevice, &bufferInfo);
		MovePtr<Allocation>			allocationBuffer	= m_allocator->allocate(getBufferMemoryRequirements(*m_device, *m_logicalDevice, *m_vertexBuffer),  MemoryRequirement::HostVisible);
		VK_CHECK(m_device->bindBufferMemory(*m_logicalDevice, *indirectBuffer, allocationBuffer->getMemory(), allocationBuffer->getOffset()));

		deMemcpy(allocationBuffer->getHostPtr(), &drawCommands[0], static_cast<size_t>(bufferSize));

		flushMappedMemoryRange(*m_device, *m_logicalDevice, allocationBuffer->getMemory(), allocationBuffer->getOffset(), static_cast<size_t>(bufferSize));
		indirectBuffers[subpassNdx] = (BufferSP(new Unique<VkBuffer>(indirectBuffer)));
		indirectAllocations[subpassNdx] = (AllocationSP(new UniquePtr<Allocation>(allocationBuffer)));
	}

	const VkRenderPassBeginInfo		renderPassBeginInfo		=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		renderPass,									// VkRenderPass			renderPass;
		frameBuffer,								// VkFramebuffer		framebuffer;
		renderArea,									// VkRect2D				renderArea;
		1u,											// uint32_t				clearValueCount;
		&renderPassClearValue,						// const VkClearValue*	pClearValues;
	};

	beginCommandBuffer(*m_device, *m_cmdBuffer);

	beforeDraw();

	m_device->cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	m_device->cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &(*m_vertexBuffer), &vertexBufferOffset);

	for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; subpassNdx++)
	{
		m_device->cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **pipelines[subpassNdx]);

		if (m_hasMultiDrawIndirect)
		{
			m_device->cmdDrawIndirect(*m_cmdBuffer, **indirectBuffers[subpassNdx], 0u, drawCountPerSubpass, strideInBuffer);
		}
		else
		{
			for (deUint32 drawNdx = 0; drawNdx < drawCountPerSubpass; drawNdx++)
			{
				m_device->cmdDrawIndirect(*m_cmdBuffer, **indirectBuffers[subpassNdx], drawNdx * strideInBuffer, 1, strideInBuffer);
			}
		}

		if (subpassNdx < subpassCount - 1u)
			m_device->cmdNextSubpass(*m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
	}

	m_device->cmdEndRenderPass(*m_cmdBuffer);

	afterDraw();

	VK_CHECK(m_device->endCommandBuffer(*m_cmdBuffer));
	submitCommandsAndWait(*m_device, *m_logicalDevice, m_queue, *m_cmdBuffer);
}

class MultiViewClearAttachmentsTestInstance : public MultiViewRenderTestInstance
{
public:
				MultiViewClearAttachmentsTestInstance	(Context& context, const TestParameters& parameters);
protected:
	void		draw									(const deUint32			subpassCount,
														 VkRenderPass			renderPass,
														 VkFramebuffer			frameBuffer,
														 vector<PipelineSp>&	pipelines);
};

MultiViewClearAttachmentsTestInstance::MultiViewClearAttachmentsTestInstance (Context& context, const TestParameters& parameters)
	: MultiViewRenderTestInstance	(context, parameters)
{
}

void MultiViewClearAttachmentsTestInstance::draw (const deUint32 subpassCount,VkRenderPass renderPass, VkFramebuffer frameBuffer, vector<PipelineSp>& pipelines)
{
	const VkRect2D					renderArea				= { { 0, 0 }, { m_parameters.extent.width, m_parameters.extent.height } };
	const VkClearValue				renderPassClearValue	= makeClearValueColor(tcu::Vec4(0.0f));
	const VkDeviceSize				vertexBufferOffset		= 0u;
	const deUint32					drawCountPerSubpass		= (subpassCount == 1) ? m_squareCount : 1u;

	const VkRenderPassBeginInfo		renderPassBeginInfo		=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		renderPass,									// VkRenderPass			renderPass;
		frameBuffer,								// VkFramebuffer		framebuffer;
		renderArea,									// VkRect2D				renderArea;
		1u,											// uint32_t				clearValueCount;
		&renderPassClearValue,						// const VkClearValue*	pClearValues;
	};

	beginCommandBuffer(*m_device, *m_cmdBuffer);

	beforeDraw();

	m_device->cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	m_device->cmdBindVertexBuffers(*m_cmdBuffer, 0u, 1u, &(*m_vertexBuffer), &vertexBufferOffset);

	for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; subpassNdx++)
	{
		VkClearAttachment	clearAttachment	=
		{
			VK_IMAGE_ASPECT_COLOR_BIT,								// VkImageAspectFlags	aspectMask
			0u,														// deUint32				colorAttachment
			makeClearValueColor(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f))	// VkClearValue			clearValue
		};

		const VkOffset2D	offset[2]		=
		{
			{0, 0},
			{static_cast<deInt32>(static_cast<float>(m_parameters.extent.width) * 0.25f), static_cast<deInt32>(static_cast<float>(m_parameters.extent.height) * 0.25f)}
		};

		const VkExtent2D	extent[2]		=
		{
			{m_parameters.extent.width, m_parameters.extent.height},
			{static_cast<deUint32>(static_cast<float>(m_parameters.extent.width) * 0.5f), static_cast<deUint32>(static_cast<float>(m_parameters.extent.height) * 0.5f)}
		};

		const VkRect2D		rect2D[2]		=
		{
			{offset[0], extent[0]},
			{offset[1], extent[1]}
		};

		VkClearRect			clearRect		=
		{
			rect2D[0],	// VkRect2D	rect
			0u,			// deUint32	baseArrayLayer
			1u,			// deUint32	layerCount
		};

		m_device->cmdClearAttachments(*m_cmdBuffer, 1u, &clearAttachment, 1u, &clearRect);
		m_device->cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **pipelines[subpassNdx]);

		for (deUint32 drawNdx = 0u; drawNdx < drawCountPerSubpass; ++drawNdx)
			m_device->cmdDraw(*m_cmdBuffer, 4u, 1u, (drawNdx + subpassNdx % m_squareCount) * 4u, 0u);

		clearRect.rect = rect2D[1];
		clearAttachment.clearValue = makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
		m_device->cmdClearAttachments(*m_cmdBuffer, 1u, &clearAttachment, 1u, &clearRect);

		if (subpassNdx < subpassCount - 1u)
			m_device->cmdNextSubpass(*m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
	}

	m_device->cmdEndRenderPass(*m_cmdBuffer);

	afterDraw();

	VK_CHECK(m_device->endCommandBuffer(*m_cmdBuffer));
	submitCommandsAndWait(*m_device, *m_logicalDevice, m_queue, *m_cmdBuffer);
}


class MultiViewSecondaryCommandBufferTestInstance : public MultiViewRenderTestInstance
{
public:
				MultiViewSecondaryCommandBufferTestInstance	(Context& context, const TestParameters& parameters);
protected:
	void		draw										(const deUint32			subpassCount,
															 VkRenderPass			renderPass,
															 VkFramebuffer			frameBuffer,
															 vector<PipelineSp>&	pipelines);
};

MultiViewSecondaryCommandBufferTestInstance::MultiViewSecondaryCommandBufferTestInstance (Context& context, const TestParameters& parameters)
	: MultiViewRenderTestInstance	(context, parameters)
{
}

void MultiViewSecondaryCommandBufferTestInstance::draw (const deUint32 subpassCount, VkRenderPass renderPass, VkFramebuffer frameBuffer, vector<PipelineSp>& pipelines)
{
	typedef de::SharedPtr<Unique<VkCommandBuffer> >	VkCommandBufferSp;

	const VkRect2D						renderArea				= { { 0, 0 }, { m_parameters.extent.width, m_parameters.extent.height } };
	const VkClearValue					renderPassClearValue	= makeClearValueColor(tcu::Vec4(0.0f));
	const VkDeviceSize					vertexBufferOffset		= 0u;
	const deUint32						drawCountPerSubpass		= (subpassCount == 1) ? m_squareCount : 1u;

	const VkRenderPassBeginInfo			renderPassBeginInfo		=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		renderPass,									// VkRenderPass			renderPass;
		frameBuffer,								// VkFramebuffer		framebuffer;
		renderArea,									// VkRect2D				renderArea;
		1u,											// uint32_t				clearValueCount;
		&renderPassClearValue,						// const VkClearValue*	pClearValues;
	};

	beginCommandBuffer(*m_device, *m_cmdBuffer);

	beforeDraw();

	m_device->cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

	//Create secondary buffer
	const VkCommandBufferAllocateInfo	cmdBufferAllocateInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		*m_cmdPool,										// VkCommandPool			commandPool;
		VK_COMMAND_BUFFER_LEVEL_SECONDARY,				// VkCommandBufferLevel		level;
		1u,												// deUint32					bufferCount;
	};
	vector<VkCommandBufferSp>	cmdBufferSecondary;

	for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; subpassNdx++)
	{
		cmdBufferSecondary.push_back(VkCommandBufferSp(new Unique<VkCommandBuffer>(allocateCommandBuffer(*m_device, *m_logicalDevice, &cmdBufferAllocateInfo))));

		beginSecondaryCommandBuffer(*m_device, cmdBufferSecondary.back().get()->get(), renderPass, subpassNdx, frameBuffer);
		m_device->cmdBindVertexBuffers(cmdBufferSecondary.back().get()->get(), 0u, 1u, &(*m_vertexBuffer), &vertexBufferOffset);
		m_device->cmdBindPipeline(cmdBufferSecondary.back().get()->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, **pipelines[subpassNdx]);

		for (deUint32 drawNdx = 0u; drawNdx < drawCountPerSubpass; ++drawNdx)
			m_device->cmdDraw(cmdBufferSecondary.back().get()->get(), 4u, 1u, (drawNdx + subpassNdx % m_squareCount) * 4u, 0u);

		VK_CHECK(m_device->endCommandBuffer(cmdBufferSecondary.back().get()->get()));

		m_device->cmdExecuteCommands(*m_cmdBuffer, 1u, &cmdBufferSecondary.back().get()->get());
		if (subpassNdx < subpassCount - 1u)
			m_device->cmdNextSubpass(*m_cmdBuffer, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
	}

	m_device->cmdEndRenderPass(*m_cmdBuffer);

	afterDraw();

	VK_CHECK(m_device->endCommandBuffer(*m_cmdBuffer));
	submitCommandsAndWait(*m_device, *m_logicalDevice, m_queue, *m_cmdBuffer);
}

class MultiViewRenderTestsCase : public vkt::TestCase
{
public:
	MultiViewRenderTestsCase (tcu::TestContext &context, const char *name, const char *description, const TestParameters& parameters)
		: TestCase			(context, name, description)
		, m_parameters		(parameters)
	{
	}
private:
	const TestParameters	m_parameters;

	vkt::TestInstance*	createInstance		(vkt::Context& context) const
	{
		if (TEST_TYPE_INPUT_ATTACHMENTS == m_parameters.viewIndex)
			return new MultiViewAttachmentsTestInstance(context, m_parameters);

		if (TEST_TYPE_INSTANCED_RENDERING == m_parameters.viewIndex)
			return new MultiViewInstancedTestInstance(context, m_parameters);

		if (TEST_TYPE_INPUT_RATE_INSTANCE == m_parameters.viewIndex)
			return new MultiViewInputRateInstanceTestInstance(context, m_parameters);

		if (TEST_TYPE_DRAW_INDIRECT == m_parameters.viewIndex)
			return new	MultiViewIDrawIndirectTestInstance(context, m_parameters);

		if (TEST_TYPE_CLEAR_ATTACHMENTS == m_parameters.viewIndex)
			return new	MultiViewClearAttachmentsTestInstance(context, m_parameters);

		if (TEST_TYPE_SECONDARY_CMD_BUFFER == m_parameters.viewIndex)
			return new	MultiViewSecondaryCommandBufferTestInstance(context, m_parameters);

		return new MultiViewRenderTestInstance(context, m_parameters);
	}

	void				initPrograms		(SourceCollections& programCollection) const
	{
		// Create vertex shader
		if (TEST_TYPE_INSTANCED_RENDERING == m_parameters.viewIndex)
		{
			std::ostringstream source;
			source	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
					<< "#extension GL_EXT_multiview : enable\n"
					<< "layout(location = 0) in highp vec4 in_position;\n"
					<< "layout(location = 1) in vec4 in_color;\n"
					<< "layout(location = 0) out vec4 out_color;\n"
					<< "void main (void)\n"
					<< "{\n"
					<< "	int modInstance = gl_InstanceIndex % 4;\n"
					<< "	int instance    = gl_InstanceIndex + 1;\n"
					<< "	gl_Position = in_position;\n"
					<< "	if (modInstance == 1)\n"
					<< "		gl_Position = in_position + vec4(0.0f, 1.0f, 0.0f, 0.0f);\n"
					<< "	if (modInstance == 2)\n"
					<< "		gl_Position = in_position + vec4(1.0f, 0.0f, 0.0f, 0.0f);\n"
					<< "	if (modInstance == 3)\n"
					<< "		gl_Position =  in_position + vec4(1.0f, 1.0f, 0.0f, 0.0f);\n"
					<< "	out_color = in_color + vec4(0.0f, gl_ViewIndex * 0.10f, instance * 0.10f, 0.0f);\n"
					<< "}\n";
			programCollection.glslSources.add("vertex") << glu::VertexSource(source.str());
		}
		else if (TEST_TYPE_INPUT_RATE_INSTANCE == m_parameters.viewIndex)
		{
			std::ostringstream source;
			source	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
					<< "#extension GL_EXT_multiview : enable\n"
					<< "layout(location = 0) in highp vec4 in_position;\n"
					<< "layout(location = 1) in vec4 in_color;\n"
					<< "layout(location = 0) out vec4 out_color;\n"
					<< "void main (void)\n"
					<< "{\n"
					<< "	int instance = gl_InstanceIndex + 1;\n"
					<< "	gl_Position = in_position;\n"
					<< "	if (gl_VertexIndex == 1)\n"
					<< "		gl_Position.y += 1.0f;\n"
					<< "	else if (gl_VertexIndex == 2)\n"
					<< "		gl_Position.x += 1.0f;\n"
					<< "	else if (gl_VertexIndex == 3)\n"
					<< "	{\n"
					<< "		gl_Position.x += 1.0f;\n"
					<< "		gl_Position.y += 1.0f;\n"
					<< "	}\n"
					<< "	out_color = in_color + vec4(0.0f, gl_ViewIndex * 0.10f, instance * 0.10f, 0.0f);\n"
					<< "}\n";
			programCollection.glslSources.add("vertex") << glu::VertexSource(source.str());
		}
		else
		{
			std::ostringstream source;
			source	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
					<< "#extension GL_EXT_multiview : enable\n"
					<< "layout(location = 0) in highp vec4 in_position;\n"
					<< "layout(location = 1) in vec4 in_color;\n"
					<< "layout(location = 0) out vec4 out_color;\n"
					<< "void main (void)\n"
					<< "{\n"
					<< "	gl_Position = in_position;\n";
				if (TEST_TYPE_VIEW_INDEX_IN_VERTEX == m_parameters.viewIndex || TEST_TYPE_DRAW_INDIRECT == m_parameters.viewIndex)
					source << "	out_color = in_color + vec4(0.0, gl_ViewIndex * 0.10f, 0.0, 0.0);\n";
				else
					source << "	out_color = in_color;\n";
			source	<< "}\n";
			programCollection.glslSources.add("vertex") << glu::VertexSource(source.str());
		}

		if (TEST_TYPE_VIEW_INDEX_IN_TESELLATION == m_parameters.viewIndex)
		{// Tessellation control & evaluation
			std::ostringstream source_tc;
			source_tc	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
						<< "#extension GL_EXT_multiview : enable\n"
						<< "#extension GL_EXT_tessellation_shader : require\n"
						<< "layout(vertices = 4) out;\n"
						<< "layout(location = 0) in vec4 in_color[];\n"
						<< "layout(location = 0) out vec4 out_color[];\n"
						<< "\n"
						<< "void main (void)\n"
						<< "{\n"
						<< "	if ( gl_InvocationID == 0 )\n"
						<< "	{\n"
						<< "		gl_TessLevelInner[0] = 4.0f;\n"
						<< "		gl_TessLevelInner[1] = 4.0f;\n"
						<< "		gl_TessLevelOuter[0] = 4.0f;\n"
						<< "		gl_TessLevelOuter[1] = 4.0f;\n"
						<< "		gl_TessLevelOuter[2] = 4.0f;\n"
						<< "		gl_TessLevelOuter[3] = 4.0f;\n"
						<< "	}\n"
						<< "	out_color[gl_InvocationID] = in_color[gl_InvocationID];\n"
						<< "	gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
						<< "}\n";
			programCollection.glslSources.add("tessellation_control") << glu::TessellationControlSource(source_tc.str());

			std::ostringstream source_te;
			source_te	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
						<< "#extension GL_EXT_multiview : enable\n"
						<< "#extension GL_EXT_tessellation_shader : require\n"
						<< "layout( quads, equal_spacing, ccw ) in;\n"
						<< "layout(location = 0) in vec4 in_color[];\n"
						<< "layout(location = 0) out vec4 out_color;\n"
						<< "void main (void)\n"
						<< "{\n"
						<< "	const float u = gl_TessCoord.x;\n"
						<< "	const float v = gl_TessCoord.y;\n"
						<< "	const float w = gl_TessCoord.z;\n"
						<< "	gl_Position = (1 - u) * (1 - v) * gl_in[0].gl_Position +(1 - u) * v * gl_in[1].gl_Position + u * (1 - v) * gl_in[2].gl_Position + u * v * gl_in[3].gl_Position;\n"
						<< "	out_color = in_color[0]+ vec4(0.0, gl_ViewIndex * 0.10f, 0.0, 0.0);\n"
						<< "}\n";
			programCollection.glslSources.add("tessellation_evaluation") << glu::TessellationEvaluationSource(source_te.str());
		}

		if (TEST_TYPE_VIEW_INDEX_IN_GEOMETRY	== m_parameters.viewIndex ||
			TEST_TYPE_INPUT_ATTACHMENTS			== m_parameters.viewIndex ||
			TEST_TYPE_CLEAR_ATTACHMENTS			== m_parameters.viewIndex ||
			TEST_TYPE_SECONDARY_CMD_BUFFER		== m_parameters.viewIndex)
		{// Geometry Shader
			std::ostringstream	source;
			source	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
					<< "#extension GL_EXT_multiview : enable\n"
					<< "layout(triangles) in;\n"
					<< "layout(triangle_strip, max_vertices = 16) out;\n"
					<< "layout(location = 0) in vec4 in_color[];\n"
					<< "layout(location = 0) out vec4 out_color;\n"
					<< "void main (void)\n"
					<< "{\n"
					<< "	out_color = in_color[0] + vec4(0.0, gl_ViewIndex * 0.10f, 0.0, 0.0);\n"
					<< "	gl_Position = gl_in[0].gl_Position;\n"
					<< "	EmitVertex();\n"
					<< "	out_color = in_color[0] + vec4(0.0, gl_ViewIndex * 0.10f, 0.0, 0.0);\n"
					<< "	gl_Position = gl_in[1].gl_Position;\n"
					<< "	EmitVertex();\n"
					<< "	out_color = in_color[0] + vec4(0.0, gl_ViewIndex * 0.10f, 0.0, 0.0);\n"
					<< "	gl_Position = gl_in[2].gl_Position;\n"
					<< "	EmitVertex();\n"
					<< "	out_color = in_color[0] + vec4(0.0, gl_ViewIndex * 0.10f, 0.0, 0.0);\n"
					<< "	gl_Position = vec4(gl_in[2].gl_Position.x, gl_in[1].gl_Position.y, 1.0, 1.0);\n"
					<< "	EmitVertex();\n"
					<< "	EndPrimitive();\n"
					<< "}\n";
			programCollection.glslSources.add("geometry") << glu::GeometrySource(source.str());
		}

		if (TEST_TYPE_INPUT_ATTACHMENTS == m_parameters.viewIndex)
		{// Create fragment shader read/write attachment
			std::ostringstream source;
			source	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
					<< "#extension GL_EXT_multiview : enable\n"
					<< "layout(location = 0) in vec4 in_color;\n"
					<< "layout(location = 0) out vec4 out_color;\n"
					<< "layout(input_attachment_index = 0, set=0, binding=0) uniform highp subpassInput in_color_attachment;\n"
					<< "void main()\n"
					<<"{\n"
					<< "	out_color = vec4(subpassLoad(in_color_attachment));\n"
					<< "}\n";
			programCollection.glslSources.add("fragment") << glu::FragmentSource(source.str());
		}
		else
		{// Create fragment shader
			std::ostringstream source;
			source	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
					<< "#extension GL_EXT_multiview : enable\n"
					<< "layout(location = 0) in vec4 in_color;\n"
					<< "layout(location = 0) out vec4 out_color;\n"
					<< "void main()\n"
					<<"{\n";
				if (TEST_TYPE_VIEW_INDEX_IN_FRAGMENT == m_parameters.viewIndex)
					source << "	out_color = in_color + vec4(0.0, gl_ViewIndex * 0.10f, 0.0, 0.0);\n";
				else
					source << "	out_color = in_color;\n";
			source	<< "}\n";
			programCollection.glslSources.add("fragment") << glu::FragmentSource(source.str());
		}
	}
};
} //anonymous

void multiViewRenderCreateTests (tcu::TestCaseGroup* group)
{
	tcu::TestContext&			testCtx						= group->getTestContext();
	const deUint32				testCaseCount				= 6u;
	MovePtr<tcu::TestCaseGroup>	groupViewIndex				(new tcu::TestCaseGroup(testCtx, "index", "ViewIndex rendering tests."));
	const string				shaderName[TEST_TYPE_LAST]	=
	{
		"masks",
		"vertex_shader",
		"fragment_shader",
		"geometry_shader",
		"tesellation_shader",
		"input_attachments",
		"instanced",
		"input_instance",
		"draw_indirect",
		"clear_attachments",
		"secondary_cmd_buffer"
	};
	const VkExtent3D			extent3D[testCaseCount]		=
	{
		{16u,	16u,	4u},
		{64u,	64u,	8u},
		{128u,	128u,	4u},
		{32u,	32u,	5u},
		{64u,	64u,	6u},
		{16u,	16u,	10u},
	};
	vector<deUint32>			viewMasks[testCaseCount];

	viewMasks[0].push_back(15u);	//1111

	viewMasks[1].push_back(8u);		//1000

	viewMasks[2].push_back(1u);		//0001
	viewMasks[2].push_back(2u);		//0010
	viewMasks[2].push_back(4u);		//0100
	viewMasks[2].push_back(8u);		//1000

	viewMasks[3].push_back(15u);	//1111
	viewMasks[3].push_back(15u);	//1111
	viewMasks[3].push_back(15u);	//1111
	viewMasks[3].push_back(15u);	//1111

	viewMasks[4].push_back(8u);		//1000
	viewMasks[4].push_back(1u);		//0001
	viewMasks[4].push_back(1u);		//0001
	viewMasks[4].push_back(8u);		//1000

	const deUint32 minSupportedMultiviewViewCount	= 6u;
	const deUint32 maxViewMask						= (1u << minSupportedMultiviewViewCount) - 1u;

	for (deUint32 mask = 1u; mask <= maxViewMask; mask = mask << 1u)
		viewMasks[5].push_back(mask);

	for (int testTypeNdx = TEST_TYPE_VIEW_MASK; testTypeNdx < TEST_TYPE_LAST; ++testTypeNdx)
	{
		MovePtr<tcu::TestCaseGroup>	groupShader	(new tcu::TestCaseGroup(testCtx, shaderName[testTypeNdx].c_str(), ""));
		for (deUint32 testCaseNdx = 0u; testCaseNdx < testCaseCount; ++testCaseNdx)
		{
			const TestParameters	parameters		=	{extent3D[testCaseNdx], viewMasks[testCaseNdx], (TestType)testTypeNdx};
			std::ostringstream		masks;
			const deUint32			viewMaksSize	=	static_cast<deUint32>(viewMasks[testCaseNdx].size());

			for (deUint32 ndx = 0u; ndx < viewMaksSize; ++ndx)
			{
				masks<<viewMasks[testCaseNdx][ndx];
				if (viewMaksSize - 1 != ndx)
					masks<<"_";
			}
			groupShader->addChild(new MultiViewRenderTestsCase(testCtx, masks.str().c_str(), "", parameters));
		}

		// maxMultiviewViewCount case
		{
			const VkExtent3D		incompleteExtent3D	= { 16u, 16u, 0u };
			const vector<deUint32>	dummyMasks;
			const TestParameters	parameters			= { incompleteExtent3D, dummyMasks, (TestType)testTypeNdx };

			groupShader->addChild(new MultiViewRenderTestsCase(testCtx, "max_multi_view_view_count", "", parameters));
		}

		switch (testTypeNdx)
		{
			case TEST_TYPE_VIEW_MASK:
			case TEST_TYPE_INPUT_ATTACHMENTS:
			case TEST_TYPE_INSTANCED_RENDERING:
			case TEST_TYPE_INPUT_RATE_INSTANCE:
			case TEST_TYPE_DRAW_INDIRECT:
			case TEST_TYPE_CLEAR_ATTACHMENTS:
			case TEST_TYPE_SECONDARY_CMD_BUFFER:
				group->addChild(groupShader.release());
				break;
			case TEST_TYPE_VIEW_INDEX_IN_VERTEX:
			case TEST_TYPE_VIEW_INDEX_IN_FRAGMENT:
			case TEST_TYPE_VIEW_INDEX_IN_GEOMETRY:
			case TEST_TYPE_VIEW_INDEX_IN_TESELLATION:
				groupViewIndex->addChild(groupShader.release());
				break;
			default:
				DE_ASSERT(0);
				break;
		};
	}

	group->addChild(groupViewIndex.release());
}

} //MultiView
} //vkt

