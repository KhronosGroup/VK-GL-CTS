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
#include "vktMultiViewRenderPassUtil.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "vktTestCase.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkPrograms.hpp"
#include "vkPlatform.hpp"
#include "vkMemUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "tcuCommandLine.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"

#include "deRandom.hpp"
#include "deMath.h"
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
	TEST_TYPE_INPUT_ATTACHMENTS_GEOMETRY,
	TEST_TYPE_INSTANCED_RENDERING,
	TEST_TYPE_INPUT_RATE_INSTANCE,
	TEST_TYPE_DRAW_INDIRECT,
	TEST_TYPE_DRAW_INDIRECT_INDEXED,
	TEST_TYPE_DRAW_INDEXED,
	TEST_TYPE_CLEAR_ATTACHMENTS,
	TEST_TYPE_SECONDARY_CMD_BUFFER,
	TEST_TYPE_SECONDARY_CMD_BUFFER_GEOMETRY,
	TEST_TYPE_POINT_SIZE,
	TEST_TYPE_MULTISAMPLE,
	TEST_TYPE_QUERIES,
	TEST_TYPE_NON_PRECISE_QUERIES,
	TEST_TYPE_READBACK_WITH_IMPLICIT_CLEAR,
	TEST_TYPE_READBACK_WITH_EXPLICIT_CLEAR,
	TEST_TYPE_DEPTH,
	TEST_TYPE_STENCIL,
	TEST_TYPE_LAST
};

enum RenderPassType
{
	RENDERPASS_TYPE_LEGACY = 0,
	RENDERPASS_TYPE_RENDERPASS2,
};

struct TestParameters
{
	VkExtent3D				extent;
	vector<deUint32>		viewMasks;
	TestType				viewIndex;
	VkSampleCountFlagBits	samples;
	VkFormat				colorFormat;
	RenderPassType			renderPassType;
};

const int	TEST_POINT_SIZE_SMALL	= 2;
const int	TEST_POINT_SIZE_WIDE	= 4;

vk::Move<vk::VkRenderPass> makeRenderPass (const DeviceInterface&		vk,
										   const VkDevice				device,
										   const VkFormat				colorFormat,
										   const vector<deUint32>&		viewMasks,
										   RenderPassType				renderPassType,
										   const VkSampleCountFlagBits	samples = VK_SAMPLE_COUNT_1_BIT,
										   const VkAttachmentLoadOp		colorLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
										   const VkFormat				dsFormat = VK_FORMAT_UNDEFINED)
{
	switch (renderPassType)
	{
		case RENDERPASS_TYPE_LEGACY:
			return MultiView::makeRenderPass<AttachmentDescription1, AttachmentReference1, SubpassDescription1, SubpassDependency1, RenderPassCreateInfo1>(vk, device, colorFormat, viewMasks, samples, colorLoadOp, dsFormat);
		case RENDERPASS_TYPE_RENDERPASS2:
			return MultiView::makeRenderPass<AttachmentDescription2, AttachmentReference2, SubpassDescription2, SubpassDependency2, RenderPassCreateInfo2>(vk, device, colorFormat, viewMasks, samples, colorLoadOp, dsFormat);
		default:
			TCU_THROW(InternalError, "Impossible");
	}
}

vk::Move<vk::VkRenderPass> makeRenderPassWithAttachments (const DeviceInterface&	vk,
														  const VkDevice			device,
														  const VkFormat			colorFormat,
														  const vector<deUint32>&	viewMasks,
														  RenderPassType			renderPassType)
{
	switch (renderPassType)
	{
		case RENDERPASS_TYPE_LEGACY:
			return MultiView::makeRenderPassWithAttachments<AttachmentDescription1, AttachmentReference1, SubpassDescription1, SubpassDependency1, RenderPassCreateInfo1>(vk, device, colorFormat, viewMasks, renderPassType == RENDERPASS_TYPE_RENDERPASS2);
		case RENDERPASS_TYPE_RENDERPASS2:
			return MultiView::makeRenderPassWithAttachments<AttachmentDescription2, AttachmentReference2, SubpassDescription2, SubpassDependency2, RenderPassCreateInfo2>(vk, device, colorFormat, viewMasks, renderPassType == RENDERPASS_TYPE_RENDERPASS2);
		default:
			TCU_THROW(InternalError, "Impossible");
	}
}

vk::Move<vk::VkRenderPass> makeRenderPassWithDepth (const DeviceInterface&	vk,
													const VkDevice			device,
													const VkFormat			colorFormat,
													const vector<deUint32>&	viewMasks,
													const					VkFormat dsFormat,
													RenderPassType			renderPassType)
{
	switch (renderPassType)
	{
		case RENDERPASS_TYPE_LEGACY:
			return MultiView::makeRenderPassWithDepth<AttachmentDescription1, AttachmentReference1, SubpassDescription1, SubpassDependency1, RenderPassCreateInfo1>(vk, device, colorFormat, viewMasks, dsFormat);
		case RENDERPASS_TYPE_RENDERPASS2:
			return MultiView::makeRenderPassWithDepth<AttachmentDescription2, AttachmentReference2, SubpassDescription2, SubpassDependency2, RenderPassCreateInfo2>(vk, device, colorFormat, viewMasks, dsFormat);
		default:
			TCU_THROW(InternalError, "Impossible");
	}
}

template<typename RenderpassSubpass>
void cmdBeginRenderPass (DeviceInterface& vkd, VkCommandBuffer cmdBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassContents contents)
{
	const typename RenderpassSubpass::SubpassBeginInfo	subpassBeginInfo	(DE_NULL, contents);

	RenderpassSubpass::cmdBeginRenderPass(vkd, cmdBuffer, pRenderPassBegin, &subpassBeginInfo);
}

void cmdBeginRenderPass (DeviceInterface& vkd, VkCommandBuffer cmdBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassContents contents, RenderPassType renderPassType)
{
	switch (renderPassType)
	{
		case RENDERPASS_TYPE_LEGACY:		cmdBeginRenderPass<RenderpassSubpass1>(vkd, cmdBuffer, pRenderPassBegin, contents);	break;
		case RENDERPASS_TYPE_RENDERPASS2:	cmdBeginRenderPass<RenderpassSubpass2>(vkd, cmdBuffer, pRenderPassBegin, contents);	break;
		default:							TCU_THROW(InternalError, "Impossible");
	}
}

template<typename RenderpassSubpass>
void cmdNextSubpass (DeviceInterface& vkd, VkCommandBuffer cmdBuffer, const VkSubpassContents contents)
{
	const typename RenderpassSubpass::SubpassBeginInfo	subpassBeginInfo	(DE_NULL, contents);
	const typename RenderpassSubpass::SubpassEndInfo	subpassEndInfo		(DE_NULL);

	RenderpassSubpass::cmdNextSubpass(vkd, cmdBuffer, &subpassBeginInfo, &subpassEndInfo);
}

void cmdNextSubpass (DeviceInterface& vkd, VkCommandBuffer cmdBuffer, const VkSubpassContents contents, RenderPassType renderPassType)
{
	switch (renderPassType)
	{
		case RENDERPASS_TYPE_LEGACY:		cmdNextSubpass<RenderpassSubpass1>(vkd, cmdBuffer, contents);	break;
		case RENDERPASS_TYPE_RENDERPASS2:	cmdNextSubpass<RenderpassSubpass2>(vkd, cmdBuffer, contents);	break;
		default:							TCU_THROW(InternalError, "Impossible");
	}
}

template<typename RenderpassSubpass>
void cmdEndRenderPass (DeviceInterface& vkd, VkCommandBuffer cmdBuffer)
{
	const typename RenderpassSubpass::SubpassEndInfo	subpassEndInfo	(DE_NULL);

	RenderpassSubpass::cmdEndRenderPass(vkd, cmdBuffer, &subpassEndInfo);
}

void cmdEndRenderPass (DeviceInterface& vkd, VkCommandBuffer cmdBuffer, RenderPassType renderPassType)
{
	switch (renderPassType)
	{
		case RENDERPASS_TYPE_LEGACY:		cmdEndRenderPass<RenderpassSubpass1>(vkd, cmdBuffer);	break;
		case RENDERPASS_TYPE_RENDERPASS2:	cmdEndRenderPass<RenderpassSubpass2>(vkd, cmdBuffer);	break;
		default:							TCU_THROW(InternalError, "Impossible");
	}
}

class ImageAttachment
{
public:
				ImageAttachment	(VkDevice logicalDevice, DeviceInterface& device, Allocator& allocator, const VkExtent3D extent, VkFormat colorFormat, const VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
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

ImageAttachment::ImageAttachment (VkDevice logicalDevice, DeviceInterface& device, Allocator& allocator, const VkExtent3D extent, VkFormat colorFormat, const VkSampleCountFlagBits samples)
{
	const bool						depthStencilFormat			= isDepthStencilFormat(colorFormat);
	const VkImageAspectFlags		aspectFlags					= depthStencilFormat ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	const VkImageSubresourceRange	colorImageSubresourceRange	= makeImageSubresourceRange(aspectFlags, 0u, 1u, 0u, extent.depth);
	const VkImageUsageFlags			imageUsageFlagsDependent	= depthStencilFormat ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	const VkImageUsageFlags			imageUsageFlags				= imageUsageFlagsDependent | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	const VkImageCreateInfo			colorAttachmentImageInfo	= makeImageCreateInfo(VK_IMAGE_TYPE_2D, extent, colorFormat, imageUsageFlags, samples);

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
															 const VkVertexInputRate					vertexInputRate = VK_VERTEX_INPUT_RATE_VERTEX,
															 const bool									useDepthTest = false,
															 const bool									useStencilTest = false);
	void							readImage				(VkImage image, const tcu::PixelBufferAccess& dst);
	bool							checkImage				(tcu::ConstPixelBufferAccess& dst);
	MovePtr<tcu::Texture2DArray>	imageData				(void);
	const tcu::Vec4					getQuarterRefColor		(const deUint32 quarterNdx, const int colorNdx, const int layerNdx, const bool background = true, const deUint32 subpassNdx = 0u);
	void							appendVertex			(const tcu::Vec4& coord, const tcu::Vec4& color);
	void							setPoint				(const tcu::PixelBufferAccess& pixelBuffer, const tcu::Vec4& pointColor, const int pointSize, const int layerNdx, const deUint32 quarter);
	void							fillTriangle			(const tcu::PixelBufferAccess& pixelBuffer, const tcu::Vec4& color, const int layerNdx, const deUint32 quarter);
	void							fillLayer				(const tcu::PixelBufferAccess& pixelBuffer, const tcu::Vec4& color, const int layerNdx);
	void							fillQuarter				(const tcu::PixelBufferAccess& pixelBuffer, const tcu::Vec4& color, const int layerNdx, const deUint32 quarter, const deUint32 subpassNdx);

	const bool						m_extensionSupported;
	const TestParameters			m_parameters;
	const int						m_seed;
	const deUint32					m_squareCount;
	Move<VkDevice>					m_logicalDevice;
	MovePtr<DeviceInterface>		m_device;
	MovePtr<Allocator>				m_allocator;
	deUint32						m_queueFamilyIndex;
	VkQueue							m_queue;
	vector<tcu::Vec4>				m_vertexCoord;
	Move<VkBuffer>					m_vertexCoordBuffer;
	MovePtr<Allocation>				m_vertexCoordAlloc;
	vector<tcu::Vec4>				m_vertexColor;
	Move<VkBuffer>					m_vertexColorBuffer;
	MovePtr<Allocation>				m_vertexColorAlloc;
	vector<deUint32>				m_vertexIndices;
	Move<VkBuffer>					m_vertexIndicesBuffer;
	MovePtr<Allocation>				m_vertexIndicesAllocation;
	Move<VkCommandPool>				m_cmdPool;
	Move<VkCommandBuffer>			m_cmdBuffer;
	de::SharedPtr<ImageAttachment>	m_colorAttachment;
	VkBool32						m_hasMultiDrawIndirect;
	vector<tcu::Vec4>				m_colorTable;
};

MultiViewRenderTestInstance::MultiViewRenderTestInstance (Context& context, const TestParameters& parameters)
	: TestInstance			(context)
	, m_extensionSupported	((parameters.renderPassType == RENDERPASS_TYPE_RENDERPASS2) && context.requireDeviceFunctionality("VK_KHR_create_renderpass2"))
	, m_parameters			(fillMissingParameters(parameters))
	, m_seed				(context.getTestContext().getCommandLine().getBaseSeed())
	, m_squareCount			(4u)
	, m_queueFamilyIndex	(0u)
{
	const float v	= 0.75f;
	const float o	= 0.25f;

	m_colorTable.push_back(tcu::Vec4(v, o, o, 1.0f));
	m_colorTable.push_back(tcu::Vec4(o, v, o, 1.0f));
	m_colorTable.push_back(tcu::Vec4(o, o, v, 1.0f));
	m_colorTable.push_back(tcu::Vec4(o, v, v, 1.0f));
	m_colorTable.push_back(tcu::Vec4(v, o, v, 1.0f));
	m_colorTable.push_back(tcu::Vec4(v, v, o, 1.0f));
	m_colorTable.push_back(tcu::Vec4(o, o, o, 1.0f));
	m_colorTable.push_back(tcu::Vec4(v, v, v, 1.0f));

	createMultiViewDevices();

	// Color attachment
	m_colorAttachment = de::SharedPtr<ImageAttachment>(new ImageAttachment(*m_logicalDevice, *m_device, *m_allocator, m_parameters.extent, m_parameters.colorFormat, m_parameters.samples));
}

tcu::TestStatus MultiViewRenderTestInstance::iterate (void)
{
	const deUint32								subpassCount				= static_cast<deUint32>(m_parameters.viewMasks.size());

	// FrameBuffer & renderPass
	Unique<VkRenderPass>						renderPass					(makeRenderPass (*m_device, *m_logicalDevice, m_parameters.colorFormat, m_parameters.viewMasks, m_parameters.renderPassType));

	Unique<VkFramebuffer>						frameBuffer					(makeFramebuffer(*m_device, *m_logicalDevice, *renderPass, m_colorAttachment->getImageView(), m_parameters.extent.width, m_parameters.extent.height));

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
		vector<deUint8>			pixelAccessData	(m_parameters.extent.width * m_parameters.extent.height * m_parameters.extent.depth * mapVkFormat(m_parameters.colorFormat).getPixelSize());
		tcu::PixelBufferAccess	dst				(mapVkFormat(m_parameters.colorFormat), m_parameters.extent.width, m_parameters.extent.height, m_parameters.extent.depth, pixelAccessData.data());

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
	imageBarrier(*m_device, *m_cmdBuffer, m_colorAttachment->getImage(), subresourceRange,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		0, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	const VkClearValue renderPassClearValue = makeClearValueColor(tcu::Vec4(0.0f));
	m_device->cmdClearColorImage(*m_cmdBuffer, m_colorAttachment->getImage(),  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &renderPassClearValue.color, 1, &subresourceRange);

	imageBarrier(*m_device, *m_cmdBuffer, m_colorAttachment->getImage(), subresourceRange,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
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

	imageBarrier(*m_device, *m_cmdBuffer, m_colorAttachment->getImage(), subresourceRange,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
}

void MultiViewRenderTestInstance::draw (const deUint32 subpassCount, VkRenderPass renderPass, VkFramebuffer frameBuffer, vector<PipelineSp>& pipelines)
{
	const VkRect2D					renderArea				= { { 0, 0 }, { m_parameters.extent.width, m_parameters.extent.height } };
	const VkClearValue				renderPassClearValue	= makeClearValueColor(tcu::Vec4(0.0f));
	const VkBuffer					vertexBuffers[]			= { *m_vertexCoordBuffer, *m_vertexColorBuffer };
	const VkDeviceSize				vertexBufferOffsets[]	= {                   0u,                   0u };
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

	cmdBeginRenderPass(*m_device, *m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE, m_parameters.renderPassType);

	m_device->cmdBindVertexBuffers(*m_cmdBuffer, 0u, DE_LENGTH_OF_ARRAY(vertexBuffers), vertexBuffers, vertexBufferOffsets);

	if (m_parameters.viewIndex == TEST_TYPE_DRAW_INDEXED)
		m_device->cmdBindIndexBuffer(*m_cmdBuffer, *m_vertexIndicesBuffer, 0u, VK_INDEX_TYPE_UINT32);

	for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; subpassNdx++)
	{
		m_device->cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **pipelines[subpassNdx]);

		for (deUint32 drawNdx = 0u; drawNdx < drawCountPerSubpass; ++drawNdx)
			if (m_parameters.viewIndex == TEST_TYPE_DRAW_INDEXED)
				m_device->cmdDrawIndexed(*m_cmdBuffer, 4u, 1u, (drawNdx + subpassNdx % m_squareCount) * 4u, 0u, 0u);
			else
				m_device->cmdDraw(*m_cmdBuffer, 4u, 1u, (drawNdx + subpassNdx % m_squareCount) * 4u, 0u);

		if (subpassNdx < subpassCount - 1u)
			cmdNextSubpass(*m_device, *m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE, m_parameters.renderPassType);
	}

	cmdEndRenderPass(*m_device, *m_cmdBuffer, m_parameters.renderPassType);

	afterDraw();

	VK_CHECK(m_device->endCommandBuffer(*m_cmdBuffer));
	submitCommandsAndWait(*m_device, *m_logicalDevice, m_queue, *m_cmdBuffer);
}

void MultiViewRenderTestInstance::createVertexData (void)
{
	tcu::Vec4 color = tcu::Vec4(0.2f, 0.0f, 0.1f, 1.0f);

	appendVertex(tcu::Vec4(-1.0f,-1.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4(-1.0f, 0.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f,-1.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), color);

	color = tcu::Vec4(0.3f, 0.0f, 0.2f, 1.0f);
	appendVertex(tcu::Vec4(-1.0f, 0.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 1.0f, 1.0f, 1.0f), color);

	color = tcu::Vec4(0.4f, 0.2f, 0.3f, 1.0f);
	appendVertex(tcu::Vec4( 0.0f,-1.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 1.0f,-1.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 1.0f, 0.0f, 1.0f, 1.0f), color);

	color = tcu::Vec4(0.5f, 0.0f, 0.4f, 1.0f);
	appendVertex(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 1.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 1.0f, 0.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 1.0f, 1.0f, 1.0f, 1.0f), color);

	if (m_parameters.viewIndex == TEST_TYPE_DRAW_INDEXED || m_parameters.viewIndex == TEST_TYPE_DRAW_INDIRECT_INDEXED)
	{
		const size_t		verticesCount	= m_vertexCoord.size();
		vector<tcu::Vec4>	vertexColor		(verticesCount);
		vector<tcu::Vec4>	vertexCoord		(verticesCount);

		m_vertexIndices.clear();
		m_vertexIndices.reserve(verticesCount);
		for (deUint32 vertexIdx = 0; vertexIdx < verticesCount; ++vertexIdx)
			m_vertexIndices.push_back(vertexIdx);

		de::Random(m_seed).shuffle(m_vertexIndices.begin(), m_vertexIndices.end());

		for (deUint32 vertexIdx = 0; vertexIdx < verticesCount; ++vertexIdx)
			vertexColor[m_vertexIndices[vertexIdx]] = m_vertexColor[vertexIdx];
		m_vertexColor.assign(vertexColor.begin(), vertexColor.end());

		for (deUint32 vertexIdx = 0; vertexIdx < verticesCount; ++vertexIdx)
			vertexCoord[m_vertexIndices[vertexIdx]] = m_vertexCoord[vertexIdx];
		m_vertexCoord.assign(vertexCoord.begin(), vertexCoord.end());
	}
}

TestParameters MultiViewRenderTestInstance::fillMissingParameters (const TestParameters& parameters)
{
	if (!parameters.viewMasks.empty())
		return parameters;
	else
	{
		const InstanceInterface&			instance			= m_context.getInstanceInterface();
		const VkPhysicalDevice				physicalDevice		= m_context.getPhysicalDevice();

		VkPhysicalDeviceMultiviewProperties multiviewProperties =
		{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES_KHR,	// VkStructureType	sType;
			DE_NULL,													// void*			pNext;
			0u,															// deUint32			maxMultiviewViewCount;
			0u															// deUint32			maxMultiviewInstanceIndex;
		};

		VkPhysicalDeviceProperties2 deviceProperties2;
		deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		deviceProperties2.pNext = &multiviewProperties;

		instance.getPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);

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
	DE_ASSERT(m_vertexCoord.size() == m_vertexColor.size());
	DE_ASSERT(m_vertexCoord.size() != 0);

	const size_t	nonCoherentAtomSize	= static_cast<size_t>(m_context.getDeviceProperties().limits.nonCoherentAtomSize);

	// Upload vertex coordinates
	{
		const size_t				dataSize		= static_cast<size_t>(m_vertexCoord.size() * sizeof(m_vertexCoord[0]));
		const VkDeviceSize			bufferDataSize	= static_cast<VkDeviceSize>(deAlignSize(dataSize, nonCoherentAtomSize));
		const VkBufferCreateInfo	bufferInfo		= makeBufferCreateInfo(bufferDataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

		m_vertexCoordBuffer	= createBuffer(*m_device, *m_logicalDevice, &bufferInfo);
		m_vertexCoordAlloc	= m_allocator->allocate(getBufferMemoryRequirements(*m_device, *m_logicalDevice, *m_vertexCoordBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(m_device->bindBufferMemory(*m_logicalDevice, *m_vertexCoordBuffer, m_vertexCoordAlloc->getMemory(), m_vertexCoordAlloc->getOffset()));
		deMemcpy(m_vertexCoordAlloc->getHostPtr(), m_vertexCoord.data(), static_cast<size_t>(dataSize));
		flushAlloc(*m_device, *m_logicalDevice, *m_vertexCoordAlloc);
	}

	// Upload vertex colors
	{
		const size_t				dataSize		= static_cast<size_t>(m_vertexColor.size() * sizeof(m_vertexColor[0]));
		const VkDeviceSize			bufferDataSize	= static_cast<VkDeviceSize>(deAlignSize(dataSize, nonCoherentAtomSize));
		const VkBufferCreateInfo	bufferInfo		= makeBufferCreateInfo(bufferDataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

		m_vertexColorBuffer	= createBuffer(*m_device, *m_logicalDevice, &bufferInfo);
		m_vertexColorAlloc	= m_allocator->allocate(getBufferMemoryRequirements(*m_device, *m_logicalDevice, *m_vertexColorBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(m_device->bindBufferMemory(*m_logicalDevice, *m_vertexColorBuffer, m_vertexColorAlloc->getMemory(), m_vertexColorAlloc->getOffset()));
		deMemcpy(m_vertexColorAlloc->getHostPtr(), m_vertexColor.data(), static_cast<size_t>(dataSize));
		flushAlloc(*m_device, *m_logicalDevice, *m_vertexColorAlloc);
	}

	// Upload vertex indices
	if (m_parameters.viewIndex == TEST_TYPE_DRAW_INDEXED || m_parameters.viewIndex == TEST_TYPE_DRAW_INDIRECT_INDEXED)
	{
		const size_t				dataSize		= static_cast<size_t>(m_vertexIndices.size() * sizeof(m_vertexIndices[0]));
		const VkDeviceSize			bufferDataSize	= static_cast<VkDeviceSize>(deAlignSize(dataSize, nonCoherentAtomSize));
		const VkBufferCreateInfo	bufferInfo		= makeBufferCreateInfo(bufferDataSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

		DE_ASSERT(m_vertexIndices.size() == m_vertexCoord.size());

		m_vertexIndicesBuffer		= createBuffer(*m_device, *m_logicalDevice, &bufferInfo);
		m_vertexIndicesAllocation	= m_allocator->allocate(getBufferMemoryRequirements(*m_device, *m_logicalDevice, *m_vertexIndicesBuffer), MemoryRequirement::HostVisible);

		// Init host buffer data
		VK_CHECK(m_device->bindBufferMemory(*m_logicalDevice, *m_vertexIndicesBuffer, m_vertexIndicesAllocation->getMemory(), m_vertexIndicesAllocation->getOffset()));
		deMemcpy(m_vertexIndicesAllocation->getHostPtr(), m_vertexIndices.data(), static_cast<size_t>(dataSize));
		flushAlloc(*m_device, *m_logicalDevice, *m_vertexIndicesAllocation);
	}
	else
		DE_ASSERT(m_vertexIndices.empty());
}

void MultiViewRenderTestInstance::createMultiViewDevices (void)
{
	const InstanceInterface&				instance				= m_context.getInstanceInterface();
	const VkPhysicalDevice					physicalDevice			= m_context.getPhysicalDevice();
	const vector<VkQueueFamilyProperties>	queueFamilyProperties	= getPhysicalDeviceQueueFamilyProperties(instance, physicalDevice);

	for (; m_queueFamilyIndex < queueFamilyProperties.size(); ++m_queueFamilyIndex)
	{
		if ((queueFamilyProperties[m_queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
			break;
	}

	const float								queuePriorities			= 1.0f;
	const VkDeviceQueueCreateInfo			queueInfo				=
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,					//VkStructureType			sType;
		DE_NULL,													//const void*				pNext;
		(VkDeviceQueueCreateFlags)0u,								//VkDeviceQueueCreateFlags	flags;
		m_queueFamilyIndex,											//deUint32					queueFamilyIndex;
		1u,															//deUint32					queueCount;
		&queuePriorities											//const float*				pQueuePriorities;
	};

	VkPhysicalDeviceMultiviewFeatures		multiviewFeatures		=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR,	// VkStructureType			sType;
		DE_NULL,													// void*					pNext;
		DE_FALSE,													// VkBool32					multiview;
		DE_FALSE,													// VkBool32					multiviewGeometryShader;
		DE_FALSE,													// VkBool32					multiviewTessellationShader;
	};

	VkPhysicalDeviceFeatures2				enabledFeatures;
	enabledFeatures.sType					= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	enabledFeatures.pNext					= &multiviewFeatures;

	instance.getPhysicalDeviceFeatures2(physicalDevice, &enabledFeatures);

	if (!multiviewFeatures.multiview)
		TCU_THROW(NotSupportedError, "MultiView not supported");

	bool requiresGeomShader = (TEST_TYPE_VIEW_INDEX_IN_GEOMETRY == m_parameters.viewIndex) ||
								(TEST_TYPE_INPUT_ATTACHMENTS_GEOMETRY == m_parameters.viewIndex) ||
								(TEST_TYPE_SECONDARY_CMD_BUFFER_GEOMETRY == m_parameters.viewIndex);

	if (requiresGeomShader && !multiviewFeatures.multiviewGeometryShader)
		TCU_THROW(NotSupportedError, "Geometry shader is not supported");

	if (TEST_TYPE_VIEW_INDEX_IN_TESELLATION == m_parameters.viewIndex && !multiviewFeatures.multiviewTessellationShader)
		TCU_THROW(NotSupportedError, "Tessellation shader is not supported");

	VkPhysicalDeviceMultiviewProperties	multiviewProperties			=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES_KHR,	//VkStructureType	sType;
		DE_NULL,													//void*				pNext;
		0u,															//deUint32			maxMultiviewViewCount;
		0u															//deUint32			maxMultiviewInstanceIndex;
	};

	VkPhysicalDeviceProperties2			propertiesDeviceProperties2;
	propertiesDeviceProperties2.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	propertiesDeviceProperties2.pNext	= &multiviewProperties;

	instance.getPhysicalDeviceProperties2(physicalDevice, &propertiesDeviceProperties2);

	if (multiviewProperties.maxMultiviewViewCount < 6u)
		TCU_FAIL("maxMultiviewViewCount below min value");

	if (multiviewProperties.maxMultiviewInstanceIndex < 134217727u) //134217727u = 2^27 -1
		TCU_FAIL("maxMultiviewInstanceIndex below min value");

	if (multiviewProperties.maxMultiviewViewCount <m_parameters.extent.depth)
		TCU_THROW(NotSupportedError, "Limit MaxMultiviewViewCount to small to run this test");

	m_hasMultiDrawIndirect = enabledFeatures.features.multiDrawIndirect;

	{
		vector<const char*>				deviceExtensions;

		if (!isCoreDeviceExtension(m_context.getUsedApiVersion(), "VK_KHR_multiview"))
			deviceExtensions.push_back("VK_KHR_multiview");

		if (m_parameters.renderPassType == RENDERPASS_TYPE_RENDERPASS2)
			if (!isCoreDeviceExtension(m_context.getUsedApiVersion(), "VK_KHR_create_renderpass2"))
				deviceExtensions.push_back("VK_KHR_create_renderpass2");

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
			deviceExtensions.empty() ? DE_NULL : &deviceExtensions[0],		//const char* const*				pEnabledExtensionNames;
			DE_NULL															//const VkPhysicalDeviceFeatures*	pEnabledFeatures;
		};

		m_logicalDevice					= createCustomDevice(m_context.getTestContext().getCommandLine().isValidationEnabled(), m_context.getPlatformInterface(), m_context.getInstance(), instance, physicalDevice, &deviceInfo);
		m_device						= MovePtr<DeviceDriver>(new DeviceDriver(m_context.getPlatformInterface(), m_context.getInstance(), *m_logicalDevice));
		m_allocator						= MovePtr<Allocator>(new SimpleAllocator(*m_device, *m_logicalDevice, getPhysicalDeviceMemoryProperties(instance, physicalDevice)));
		m_device->getDeviceQueue		(*m_logicalDevice, m_queueFamilyIndex, 0u, &m_queue);
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
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,		// VkStructureType		sType;
			DE_NULL,											// const void*			pNext;
			*m_cmdPool,											// VkCommandPool		commandPool;
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,					// VkCommandBufferLevel	level;
			1u,													// deUint32				bufferCount;
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
		case TEST_TYPE_DRAW_INDIRECT_INDEXED:
		case TEST_TYPE_DRAW_INDEXED:
		case TEST_TYPE_CLEAR_ATTACHMENTS:
		case TEST_TYPE_SECONDARY_CMD_BUFFER:
		case TEST_TYPE_INPUT_ATTACHMENTS:
		case TEST_TYPE_POINT_SIZE:
		case TEST_TYPE_MULTISAMPLE:
		case TEST_TYPE_QUERIES:
		case TEST_TYPE_NON_PRECISE_QUERIES:
		case TEST_TYPE_READBACK_WITH_IMPLICIT_CLEAR:
		case TEST_TYPE_READBACK_WITH_EXPLICIT_CLEAR:
		case TEST_TYPE_DEPTH:
		case TEST_TYPE_STENCIL:
			shaderModule[VK_SHADER_STAGE_VERTEX_BIT]					= (ShaderModuleSP(new Unique<VkShaderModule>(createShaderModule(*m_device, *m_logicalDevice, m_context.getBinaryCollection().get("vertex"), 0))));
			shaderModule[VK_SHADER_STAGE_FRAGMENT_BIT]					= (ShaderModuleSP(new Unique<VkShaderModule>(createShaderModule(*m_device, *m_logicalDevice, m_context.getBinaryCollection().get("fragment"), 0))));
			break;
		case TEST_TYPE_VIEW_INDEX_IN_GEOMETRY:
		case TEST_TYPE_INPUT_ATTACHMENTS_GEOMETRY:
		case TEST_TYPE_SECONDARY_CMD_BUFFER_GEOMETRY:
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
																	const VkVertexInputRate						vertexInputRate,
																	const bool									useDepthTest,
																	const bool									useStencilTest)
{
	const VkVertexInputBindingDescription			vertexInputBindingDescriptions[]	=
	{
		{
			0u,													// binding;
			static_cast<deUint32>(sizeof(m_vertexCoord[0])),	// stride;
			vertexInputRate										// inputRate
		},
		{
			1u,													// binding;
			static_cast<deUint32>(sizeof(m_vertexColor[0])),	// stride;
			vertexInputRate										// inputRate
		}
	};

	const VkVertexInputAttributeDescription			vertexInputAttributeDescriptions[]	=
	{
		{
			0u,											// deUint32	location;
			0u,											// deUint32	binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,				// VkFormat	format;
			0u											// deUint32	offset;
		},	// VertexElementData::position
		{
			1u,											// deUint32	location;
			1u,											// deUint32	binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,				// VkFormat	format;
			0u											// deUint32	offset;
		},	// VertexElementData::color
	};

	const VkPipelineVertexInputStateCreateInfo		vertexInputStateParams				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
		NULL,															// const void*								pNext;
		0u,																// VkPipelineVertexInputStateCreateFlags	flags;
		DE_LENGTH_OF_ARRAY(vertexInputBindingDescriptions),				// deUint32									vertexBindingDescriptionCount;
		vertexInputBindingDescriptions,									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
		DE_LENGTH_OF_ARRAY(vertexInputAttributeDescriptions),			// deUint32									vertexAttributeDescriptionCount;
		vertexInputAttributeDescriptions								// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	const VkPrimitiveTopology						topology							= (TEST_TYPE_VIEW_INDEX_IN_TESELLATION == m_parameters.viewIndex) ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST :
																						  (TEST_TYPE_POINT_SIZE == m_parameters.viewIndex) ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST :
																						  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyStateParams			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		0u,																// VkPipelineInputAssemblyStateCreateFlags	flags;
		topology,														// VkPrimitiveTopology						topology;
		VK_FALSE,														// VkBool32									primitiveRestartEnable;
	};

	const VkViewport								viewport							= makeViewport(m_parameters.extent);
	const VkRect2D									scissor								= makeRect2D(m_parameters.extent);

	const VkPipelineViewportStateCreateInfo			viewportStateParams					=
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

	const VkSampleCountFlagBits						sampleCountFlagBits					= (TEST_TYPE_MULTISAMPLE == m_parameters.viewIndex) ? VK_SAMPLE_COUNT_4_BIT :
																						  VK_SAMPLE_COUNT_1_BIT;
	const VkPipelineMultisampleStateCreateInfo		multisampleStateParams				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		0u,															// VkPipelineMultisampleStateCreateFlags	flags;
		sampleCountFlagBits,										// VkSampleCountFlagBits					rasterizationSamples;
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
		useDepthTest ? VK_TRUE : VK_FALSE,							// VkBool32									depthTestEnable;
		useDepthTest ? VK_TRUE : VK_FALSE,							// VkBool32									depthWriteEnable;
		VK_COMPARE_OP_LESS_OR_EQUAL,								// VkCompareOp								depthCompareOp;
		VK_FALSE,													// VkBool32									depthBoundsTestEnable;
		useStencilTest ? VK_TRUE : VK_FALSE,						// VkBool32									stencilTestEnable;
		// VkStencilOpState front;
		{
			VK_STENCIL_OP_KEEP,					// VkStencilOp	failOp;
			VK_STENCIL_OP_INCREMENT_AND_CLAMP,	// VkStencilOp	passOp;
			VK_STENCIL_OP_KEEP,					// VkStencilOp	depthFailOp;
			VK_COMPARE_OP_ALWAYS,				// VkCompareOp	compareOp;
			~0u,								// deUint32		compareMask;
			~0u,								// deUint32		writeMask;
			0u,									// deUint32		reference;
		},
		// VkStencilOpState back;
		{
			VK_STENCIL_OP_KEEP,					// VkStencilOp	failOp;
			VK_STENCIL_OP_INCREMENT_AND_CLAMP,	// VkStencilOp	passOp;
			VK_STENCIL_OP_KEEP,					// VkStencilOp	depthFailOp;
			VK_COMPARE_OP_ALWAYS,				// VkCompareOp	compareOp;
			~0u,								// deUint32		compareMask;
			~0u,								// deUint32		writeMask;
			0u,									// deUint32		reference;
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
	const VkDeviceSize			pixelDataSize	= dst.getWidth() * dst.getHeight() * dst.getDepth() * mapVkFormat(m_parameters.colorFormat).getPixelSize();

	// Create destination buffer
	{
		const VkBufferCreateInfo bufferParams	=
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
		flushAlloc(*m_device, *m_logicalDevice, *bufferAlloc);
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

		imageBarrier (*m_device, *m_cmdBuffer, image, subresourceRange,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		m_device->cmdCopyImageToBuffer(*m_cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *buffer, 1u, &copyRegion);
		m_device->cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &bufferBarrier, 0u, DE_NULL);
	}
	VK_CHECK(m_device->endCommandBuffer(*m_cmdBuffer));
	submitCommandsAndWait(*m_device, *m_logicalDevice, m_queue, *m_cmdBuffer);

	// Read buffer data
	invalidateAlloc(*m_device, *m_logicalDevice, *bufferAlloc);
	tcu::copy(dst, tcu::ConstPixelBufferAccess(dst.getFormat(), dst.getSize(), bufferAlloc->getHostPtr()));
}

bool MultiViewRenderTestInstance::checkImage (tcu::ConstPixelBufferAccess& renderedFrame)
{
	const MovePtr<tcu::Texture2DArray>	referenceFrame	= imageData();
	const bool							result			= tcu::floatThresholdCompare(m_context.getTestContext().getLog(),
															"Result", "Image comparison result", referenceFrame->getLevel(0), renderedFrame, tcu::Vec4(0.01f), tcu::COMPARE_LOG_ON_ERROR);

	if (!result)
		for (deUint32 layerNdx = 0u; layerNdx < m_parameters.extent.depth; layerNdx++)
		{
			tcu::ConstPixelBufferAccess ref (mapVkFormat(m_parameters.colorFormat), m_parameters.extent.width, m_parameters.extent.height, 1u, referenceFrame->getLevel(0).getPixelPtr(0, 0, layerNdx));
			tcu::ConstPixelBufferAccess dst (mapVkFormat(m_parameters.colorFormat), m_parameters.extent.width, m_parameters.extent.height, 1u, renderedFrame.getPixelPtr(0 ,0, layerNdx));
			tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Result", "Image comparison result", ref, dst, tcu::Vec4(0.01f), tcu::COMPARE_LOG_EVERYTHING);
		}

	return result;
}

const tcu::Vec4 MultiViewRenderTestInstance::getQuarterRefColor (const deUint32 quarterNdx, const int colorNdx, const int layerNdx, const bool background, const deUint32 subpassNdx)
{
	switch (m_parameters.viewIndex)
	{
		case TEST_TYPE_VIEW_MASK:
			return m_vertexColor[colorNdx];

		case TEST_TYPE_DRAW_INDEXED:
			return m_vertexColor[m_vertexIndices[colorNdx]];

		case TEST_TYPE_INSTANCED_RENDERING:
			return m_vertexColor[0] + tcu::Vec4(0.0, static_cast<float>(layerNdx) * 0.10f, static_cast<float>(quarterNdx + 1u) * 0.10f, 0.0);

		case TEST_TYPE_INPUT_RATE_INSTANCE:
			return m_vertexColor[colorNdx / 4] + tcu::Vec4(0.0, static_cast<float>(layerNdx) * 0.10f, static_cast<float>(quarterNdx + 1u) * 0.10f, 0.0);

		case TEST_TYPE_DRAW_INDIRECT_INDEXED:
			return m_vertexColor[m_vertexIndices[colorNdx]] + tcu::Vec4(0.0, static_cast<float>(layerNdx) * 0.10f, 0.0, 0.0);

		case TEST_TYPE_VIEW_INDEX_IN_VERTEX:
		case TEST_TYPE_VIEW_INDEX_IN_FRAGMENT:
		case TEST_TYPE_VIEW_INDEX_IN_GEOMETRY:
		case TEST_TYPE_VIEW_INDEX_IN_TESELLATION:
		case TEST_TYPE_INPUT_ATTACHMENTS:
		case TEST_TYPE_INPUT_ATTACHMENTS_GEOMETRY:
		case TEST_TYPE_DRAW_INDIRECT:
		case TEST_TYPE_CLEAR_ATTACHMENTS:
		case TEST_TYPE_SECONDARY_CMD_BUFFER:
		case TEST_TYPE_SECONDARY_CMD_BUFFER_GEOMETRY:
			return m_vertexColor[colorNdx] + tcu::Vec4(0.0, static_cast<float>(layerNdx) * 0.10f, 0.0, 0.0);

		case TEST_TYPE_READBACK_WITH_EXPLICIT_CLEAR:
			if (background)
				return m_colorTable[4 + quarterNdx % 4];
			else
				return m_colorTable[layerNdx % 4];

		case TEST_TYPE_READBACK_WITH_IMPLICIT_CLEAR:
			if (background)
				return m_colorTable[4 + quarterNdx % 4];
			else
				return m_colorTable[0];

		case TEST_TYPE_POINT_SIZE:
		case TEST_TYPE_MULTISAMPLE:
			if (background)
				return tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
			else
				return m_vertexColor[colorNdx];

		case TEST_TYPE_DEPTH:
			if (background)
				if (subpassNdx < 4)
					return tcu::Vec4(0.66f, 0.0f, 0.0f, 1.0f);
				else
					return tcu::Vec4(0.33f, 0.0f, 0.0f, 1.0f);
			else
				return tcu::Vec4(0.99f, 0.0f, 0.0f, 1.0f);

		case TEST_TYPE_STENCIL:
			if (background)
				return tcu::Vec4(0.33f, 0.0f, 0.0f, 0.0f); // Increment value
			else
				return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);

		default:
			TCU_THROW(InternalError, "Impossible");
	}
}

void MultiViewRenderTestInstance::setPoint (const tcu::PixelBufferAccess& pixelBuffer, const tcu::Vec4& pointColor, const int pointSize, const int layerNdx, const deUint32 quarter)
{
	DE_ASSERT(TEST_POINT_SIZE_WIDE > TEST_POINT_SIZE_SMALL);

	const int	pointOffset	= 1 + TEST_POINT_SIZE_WIDE / 2 - (pointSize + 1) / 2;
	const int	offsetX		= pointOffset + static_cast<int>((quarter == 0u || quarter == 1u) ? 0 : m_parameters.extent.width / 2u);
	const int	offsetY		= pointOffset + static_cast<int>((quarter == 0u || quarter == 2u) ? 0 : m_parameters.extent.height / 2u);

	for (int y = 0; y < pointSize; ++y)
	for (int x = 0; x < pointSize; ++x)
		pixelBuffer.setPixel(pointColor, offsetX + x, offsetY + y, layerNdx);
}

void MultiViewRenderTestInstance::fillTriangle (const tcu::PixelBufferAccess& pixelBuffer, const tcu::Vec4& color, const int layerNdx, const deUint32 quarter)
{
	const int		offsetX				= static_cast<int>((quarter == 0u || quarter == 1u) ? 0 : m_parameters.extent.width / 2u);
	const int		offsetY				= static_cast<int>((quarter == 0u || quarter == 2u) ? 0 : m_parameters.extent.height / 2u);
	const int		maxY				= static_cast<int>(m_parameters.extent.height / 2u);
	const tcu::Vec4	multisampledColor	= tcu::Vec4(color[0], color[1], color[2], color[3]) * 0.5f;

	for (int y = 0; y < maxY; ++y)
	{
		for (int x = 0; x < y; ++x)
			pixelBuffer.setPixel(color, offsetX + x, offsetY + (maxY - 1) - y, layerNdx);

		// Multisampled pixel is on the triangle margin
		pixelBuffer.setPixel(multisampledColor, offsetX + y, offsetY + (maxY - 1) - y, layerNdx);
	}
}

void MultiViewRenderTestInstance::fillLayer (const tcu::PixelBufferAccess& pixelBuffer, const tcu::Vec4& color, const int layerNdx)
{
	for (deUint32 y = 0u; y < m_parameters.extent.height; ++y)
	for (deUint32 x = 0u; x < m_parameters.extent.width; ++x)
		pixelBuffer.setPixel(color, x, y, layerNdx);
}

void MultiViewRenderTestInstance::fillQuarter (const tcu::PixelBufferAccess& pixelBuffer, const tcu::Vec4& color, const int layerNdx, const deUint32 quarter, const deUint32 subpassNdx)
{
	const int h		= m_parameters.extent.height;
	const int h2	= h / 2;
	const int w		= m_parameters.extent.width;
	const int w2	= w / 2;
	int xStart		= 0;
	int xEnd		= 0;
	int yStart		= 0;
	int yEnd		= 0;

	switch (quarter)
	{
		case 0:	xStart = 0u; xEnd = w2; yStart = 0u; yEnd = h2; break;
		case 1:	xStart = 0u; xEnd = w2; yStart = h2; yEnd = h;  break;
		case 2:	xStart = w2; xEnd = w;  yStart = 0u; yEnd = h2; break;
		case 3:	xStart = w2; xEnd = w;  yStart = h2; yEnd = h;  break;
		default: TCU_THROW(InternalError, "Impossible");
	}

	if (TEST_TYPE_STENCIL == m_parameters.viewIndex || TEST_TYPE_DEPTH == m_parameters.viewIndex)
	{
		if (subpassNdx < 4)
		{	// Part A: Horizontal bars near X axis
			yStart	= h2 + (yStart - h2) / 2;
			yEnd	= h2 + (yEnd - h2) / 2;
		}
		else
		{	// Part B: Vertical bars near Y axis (drawn twice)
			xStart	= w2 + (xStart - w2) / 2;
			xEnd	= w2 + (xEnd - w2) / 2;
		}

		// Update pixels in area
		if (TEST_TYPE_STENCIL == m_parameters.viewIndex)
		{
			for (int y = yStart; y < yEnd; ++y)
			for (int x = xStart; x < xEnd; ++x)
				pixelBuffer.setPixel(pixelBuffer.getPixel(x, y, layerNdx) + color, x, y, layerNdx);
		}

		if (TEST_TYPE_DEPTH == m_parameters.viewIndex)
		{
			for (int y = yStart; y < yEnd; ++y)
			for (int x = xStart; x < xEnd; ++x)
			{
				const tcu::Vec4		currentColor	= pixelBuffer.getPixel(x, y, layerNdx);
				const tcu::Vec4&	newColor		= (currentColor[0] < color[0]) ? currentColor : color;

				pixelBuffer.setPixel(newColor, x, y, layerNdx);
			}
		}
	}
	else
	{
		for (int y = yStart; y < yEnd; ++y)
		for (int x = xStart; x < xEnd; ++x)
			pixelBuffer.setPixel(color , x, y, layerNdx);
	}
}

MovePtr<tcu::Texture2DArray> MultiViewRenderTestInstance::imageData (void)
{
	MovePtr<tcu::Texture2DArray>	referenceFrame	= MovePtr<tcu::Texture2DArray>(new tcu::Texture2DArray(mapVkFormat(m_parameters.colorFormat), m_parameters.extent.width, m_parameters.extent.height, m_parameters.extent.depth));
	const deUint32					subpassCount	= static_cast<deUint32>(m_parameters.viewMasks.size());
	referenceFrame->allocLevel(0);

	deMemset (referenceFrame->getLevel(0).getDataPtr(), 0, m_parameters.extent.width * m_parameters.extent.height * m_parameters.extent.depth* mapVkFormat(m_parameters.colorFormat).getPixelSize());

	if (TEST_TYPE_READBACK_WITH_IMPLICIT_CLEAR == m_parameters.viewIndex || TEST_TYPE_READBACK_WITH_EXPLICIT_CLEAR == m_parameters.viewIndex)
	{
		deUint32	clearedViewMask	= 0;

		// Start from last clear command color, which actually takes effect
		for (int subpassNdx = static_cast<int>(subpassCount) - 1; subpassNdx >= 0; --subpassNdx)
		{
			deUint32	subpassToClearViewMask	= m_parameters.viewMasks[subpassNdx] & ~clearedViewMask;

			if (subpassToClearViewMask == 0)
				continue;

			for (deUint32 layerNdx = 0; layerNdx < m_parameters.extent.depth; ++layerNdx)
				if ((subpassToClearViewMask & (1 << layerNdx)) != 0 && (clearedViewMask & (1 << layerNdx)) == 0)
					fillLayer(referenceFrame->getLevel(0), getQuarterRefColor(0u, 0u, subpassNdx, false), layerNdx);

			// These has been cleared. Exclude these layers from upcoming attempts to clear
			clearedViewMask |= subpassToClearViewMask;
		}
	}

	if (TEST_TYPE_DEPTH == m_parameters.viewIndex || TEST_TYPE_STENCIL == m_parameters.viewIndex)
		for (deUint32 layerNdx = 0; layerNdx < m_parameters.extent.depth; ++layerNdx)
			fillLayer(referenceFrame->getLevel(0), getQuarterRefColor(0u, 0u, 0u, false), layerNdx);

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
					const tcu::Vec4 color = getQuarterRefColor(0u, colorNdx, layerNdx, true, subpassNdx);

					fillQuarter(referenceFrame->getLevel(0), color, layerNdx, 0u, subpassNdx);
				}

				colorNdx += 4;
				if (subpassQuarterNdx == 1u || subpassCount == 1u || TEST_TYPE_INPUT_RATE_INSTANCE == m_parameters.viewIndex)
				{
					const tcu::Vec4 color = getQuarterRefColor(1u, colorNdx, layerNdx, true, subpassNdx);

					fillQuarter(referenceFrame->getLevel(0), color, layerNdx, 1u, subpassNdx);
				}

				colorNdx += 4;
				if (subpassQuarterNdx == 2u || subpassCount == 1u || TEST_TYPE_INPUT_RATE_INSTANCE == m_parameters.viewIndex)
				{
					const tcu::Vec4 color = getQuarterRefColor(2u, colorNdx, layerNdx, true, subpassNdx);

					fillQuarter(referenceFrame->getLevel(0), color, layerNdx, 2u, subpassNdx);
				}

				colorNdx += 4;
				if (subpassQuarterNdx == 3u || subpassCount == 1u || TEST_TYPE_INPUT_RATE_INSTANCE == m_parameters.viewIndex)
				{
					const tcu::Vec4 color = getQuarterRefColor(3u, colorNdx, layerNdx, true, subpassNdx);

					fillQuarter(referenceFrame->getLevel(0), color, layerNdx, 3u, subpassNdx);
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

				if (TEST_TYPE_POINT_SIZE == m_parameters.viewIndex)
				{
					const deUint32	vertexPerPrimitive	= 1u;
					const deUint32	dummyQuarterNdx		= 0u;
					const int		pointSize			= static_cast<int>(layerNdx == 0u ? TEST_POINT_SIZE_WIDE : TEST_POINT_SIZE_SMALL);

					if (subpassCount == 1)
						for (deUint32 drawNdx = 0u; drawNdx < m_squareCount; ++drawNdx)
							setPoint(referenceFrame->getLevel(0), getQuarterRefColor(dummyQuarterNdx, vertexPerPrimitive * drawNdx, layerNdx, false), pointSize, layerNdx, drawNdx);
					else
						setPoint(referenceFrame->getLevel(0), getQuarterRefColor(dummyQuarterNdx, vertexPerPrimitive * subpassQuarterNdx, layerNdx, false), pointSize, layerNdx, subpassQuarterNdx);
				}

				if (TEST_TYPE_MULTISAMPLE == m_parameters.viewIndex)
				{
					const deUint32	vertexPerPrimitive	= 3u;
					const deUint32	dummyQuarterNdx		= 0u;

					if (subpassCount == 1)
						for (deUint32 drawNdx = 0u; drawNdx < m_squareCount; ++drawNdx)
							fillTriangle(referenceFrame->getLevel(0), getQuarterRefColor(dummyQuarterNdx, vertexPerPrimitive * drawNdx, layerNdx, false), layerNdx, drawNdx);
					else
						fillTriangle(referenceFrame->getLevel(0), getQuarterRefColor(dummyQuarterNdx, vertexPerPrimitive * subpassQuarterNdx, layerNdx, false), layerNdx, subpassQuarterNdx);
				}
			}

			mask = mask >> 1;
			++layerNdx;
		}
	}
	return referenceFrame;
}

void MultiViewRenderTestInstance::appendVertex (const tcu::Vec4& coord, const tcu::Vec4& color)
{
	m_vertexCoord.push_back(coord);
	m_vertexColor.push_back(color);
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
	m_colorAttachment	= de::SharedPtr<ImageAttachment>(new ImageAttachment(*m_logicalDevice, *m_device, *m_allocator, m_parameters.extent, m_parameters.colorFormat));
	m_inputAttachment	= de::SharedPtr<ImageAttachment>(new ImageAttachment(*m_logicalDevice, *m_device, *m_allocator, m_parameters.extent, m_parameters.colorFormat));

	// FrameBuffer & renderPass
	Unique<VkRenderPass>						renderPass				(makeRenderPassWithAttachments(*m_device, *m_logicalDevice, m_parameters.colorFormat, m_parameters.viewMasks, m_parameters.renderPassType));

	vector<VkImageView>							attachments;
	attachments.push_back(m_colorAttachment->getImageView());
	attachments.push_back(m_inputAttachment->getImageView());
	Unique<VkFramebuffer>						frameBuffer				(makeFramebuffer(*m_device, *m_logicalDevice, *renderPass, static_cast<deUint32>(attachments.size()), attachments.data(), m_parameters.extent.width, m_parameters.extent.height));

	// pipelineLayout
	m_descriptorSetLayout	= makeDescriptorSetLayout(*m_device, *m_logicalDevice);
	m_pipelineLayout		= makePipelineLayout(*m_device, *m_logicalDevice, m_descriptorSetLayout.get());

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
		vector<deUint8>			pixelAccessData	(m_parameters.extent.width * m_parameters.extent.height * m_parameters.extent.depth * mapVkFormat(m_parameters.colorFormat).getPixelSize());
		tcu::PixelBufferAccess	dst				(mapVkFormat(m_parameters.colorFormat), m_parameters.extent.width, m_parameters.extent.height, m_parameters.extent.depth, pixelAccessData.data());

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

	imageBarrier(*m_device, *m_cmdBuffer, m_colorAttachment->getImage(), subresourceRange,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		0, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	const VkClearValue renderPassClearValue = makeClearValueColor(tcu::Vec4(0.0f));
	m_device->cmdClearColorImage(*m_cmdBuffer, m_colorAttachment->getImage(),  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &renderPassClearValue.color, 1, &subresourceRange);

	imageBarrier(*m_device, *m_cmdBuffer, m_colorAttachment->getImage(), subresourceRange,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
}

void MultiViewAttachmentsTestInstance::setImageData (VkImage image)
{
	const MovePtr<tcu::Texture2DArray>		data		= imageData();
	Move<VkBuffer>					buffer;
	const deUint32					bufferSize	= m_parameters.extent.width * m_parameters.extent.height * m_parameters.extent.depth * tcu::getPixelSize(mapVkFormat(m_parameters.colorFormat));
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

	const VkImageAspectFlags				formatAspect			= getAspectFlags(mapVkFormat(m_parameters.colorFormat));
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
	flushAlloc(*m_device, *m_logicalDevice, *bufferAlloc);

	beginCommandBuffer(*m_device, *m_cmdBuffer);

	m_device->cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &preBufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	imageBarrier(*m_device, *m_cmdBuffer, image, subresourceRange,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		0u, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	m_device->cmdCopyBufferToImage(*m_cmdBuffer, *buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);
	imageBarrier(*m_device, *m_cmdBuffer, image, subresourceRange,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
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
	const tcu::Vec4 color = tcu::Vec4(0.2f, 0.0f, 0.1f, 1.0f);

	appendVertex(tcu::Vec4(-1.0f,-1.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4(-1.0f, 0.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f,-1.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), color);
}

void MultiViewInstancedTestInstance::draw (const deUint32 subpassCount, VkRenderPass renderPass, VkFramebuffer frameBuffer, vector<PipelineSp>& pipelines)
{
	const VkRect2D					renderArea				= { { 0, 0 }, { m_parameters.extent.width, m_parameters.extent.height } };
	const VkClearValue				renderPassClearValue	= makeClearValueColor(tcu::Vec4(0.0f));
	const VkBuffer					vertexBuffers[]			= { *m_vertexCoordBuffer, *m_vertexColorBuffer };
	const VkDeviceSize				vertexBufferOffsets[]	= {                   0u,                   0u };
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

	cmdBeginRenderPass(*m_device, *m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE, m_parameters.renderPassType);

	m_device->cmdBindVertexBuffers(*m_cmdBuffer, 0u, DE_LENGTH_OF_ARRAY(vertexBuffers), vertexBuffers, vertexBufferOffsets);

	for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; subpassNdx++)
	{
		m_device->cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **pipelines[subpassNdx]);

		m_device->cmdDraw(*m_cmdBuffer, 4u, drawCountPerSubpass, 0u, subpassNdx % m_squareCount);

		if (subpassNdx < subpassCount - 1u)
			cmdNextSubpass(*m_device, *m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE, m_parameters.renderPassType);
	}

	cmdEndRenderPass(*m_device, *m_cmdBuffer, m_parameters.renderPassType);

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
	appendVertex(tcu::Vec4(-1.0f,-1.0f, 1.0f, 1.0f), tcu::Vec4(0.2f, 0.0f, 0.1f, 1.0f));
	appendVertex(tcu::Vec4(-1.0f, 0.0f, 1.0f, 1.0f), tcu::Vec4(0.3f, 0.0f, 0.2f, 1.0f));
	appendVertex(tcu::Vec4( 0.0f,-1.0f, 1.0f, 1.0f), tcu::Vec4(0.4f, 0.2f, 0.3f, 1.0f));
	appendVertex(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), tcu::Vec4(0.5f, 0.0f, 0.4f, 1.0f));
}

void MultiViewInputRateInstanceTestInstance::draw (const deUint32 subpassCount, VkRenderPass renderPass, VkFramebuffer frameBuffer, vector<PipelineSp>& pipelines)
{
	const VkRect2D					renderArea				= { { 0, 0 }, { m_parameters.extent.width, m_parameters.extent.height } };
	const VkClearValue				renderPassClearValue	= makeClearValueColor(tcu::Vec4(0.0f));
	const VkBuffer					vertexBuffers[]			= { *m_vertexCoordBuffer, *m_vertexColorBuffer };
	const VkDeviceSize				vertexBufferOffsets[]	= {                   0u,                   0u };
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

	cmdBeginRenderPass(*m_device, *m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE, m_parameters.renderPassType);

	m_device->cmdBindVertexBuffers(*m_cmdBuffer, 0u, DE_LENGTH_OF_ARRAY(vertexBuffers), vertexBuffers, vertexBufferOffsets);

	for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; subpassNdx++)
	{
		m_device->cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **pipelines[subpassNdx]);

		for (deUint32 drawNdx = 0u; drawNdx < drawCountPerSubpass; ++drawNdx)
			m_device->cmdDraw(*m_cmdBuffer, 4u, 4u, 0u, 0u);

		if (subpassNdx < subpassCount - 1u)
			cmdNextSubpass(*m_device, *m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE, m_parameters.renderPassType);
	}

	cmdEndRenderPass(*m_device, *m_cmdBuffer, m_parameters.renderPassType);

	afterDraw();

	VK_CHECK(m_device->endCommandBuffer(*m_cmdBuffer));
	submitCommandsAndWait(*m_device, *m_logicalDevice, m_queue, *m_cmdBuffer);
}

class MultiViewDrawIndirectTestInstance : public MultiViewRenderTestInstance
{
public:
				MultiViewDrawIndirectTestInstance	(Context& context, const TestParameters& parameters);
protected:

	void		draw								(const deUint32			subpassCount,
													 VkRenderPass			renderPass,
													 VkFramebuffer			frameBuffer,
													 vector<PipelineSp>&	pipelines);
};

MultiViewDrawIndirectTestInstance::MultiViewDrawIndirectTestInstance (Context& context, const TestParameters& parameters)
	: MultiViewRenderTestInstance	(context, parameters)
{
}

void MultiViewDrawIndirectTestInstance::draw (const deUint32 subpassCount, VkRenderPass renderPass, VkFramebuffer frameBuffer, vector<PipelineSp>& pipelines)
{
	typedef de::SharedPtr<Unique<VkBuffer> >		BufferSP;
	typedef de::SharedPtr<UniquePtr<Allocation> >	AllocationSP;

	const size_t					nonCoherentAtomSize		= static_cast<size_t>(m_context.getDeviceProperties().limits.nonCoherentAtomSize);
	const VkRect2D					renderArea				= { { 0, 0 }, { m_parameters.extent.width, m_parameters.extent.height } };
	const VkClearValue				renderPassClearValue	= makeClearValueColor(tcu::Vec4(0.0f));
	const VkBuffer					vertexBuffers[]			= { *m_vertexCoordBuffer, *m_vertexColorBuffer };
	const VkDeviceSize				vertexBufferOffsets[]	= {                   0u,                   0u };
	const deUint32					drawCountPerSubpass		= (subpassCount == 1) ? m_squareCount : 1u;
	const deUint32					strideInBuffer			= (m_parameters.viewIndex == TEST_TYPE_DRAW_INDIRECT_INDEXED)
															? static_cast<deUint32>(sizeof(vk::VkDrawIndexedIndirectCommand))
															: static_cast<deUint32>(sizeof(vk::VkDrawIndirectCommand));
	vector< BufferSP >				indirectBuffers			(subpassCount);
	vector< AllocationSP >			indirectAllocations		(subpassCount);

	for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; subpassNdx++)
	{
		vector<VkDrawIndirectCommand>			drawCommands;
		vector<VkDrawIndexedIndirectCommand>	drawCommandsIndexed;

		for (deUint32 drawNdx = 0u; drawNdx < drawCountPerSubpass; ++drawNdx)
		{
			if (m_parameters.viewIndex == TEST_TYPE_DRAW_INDIRECT_INDEXED)
			{
				const VkDrawIndexedIndirectCommand	drawCommandIndexed	=
				{
					4u,												//  deUint32	indexCount;
					1u,												//  deUint32	instanceCount;
					(drawNdx + subpassNdx % m_squareCount) * 4u,	//  deUint32	firstIndex;
					0u,												//  deInt32		vertexOffset;
					0u,												//  deUint32	firstInstance;
				};

				drawCommandsIndexed.push_back(drawCommandIndexed);
			}
			else
			{
				const VkDrawIndirectCommand	drawCommand	=
				{
					4u,												//  deUint32	vertexCount;
					1u,												//  deUint32	instanceCount;
					(drawNdx + subpassNdx % m_squareCount) * 4u,	//  deUint32	firstVertex;
					0u												//  deUint32	firstInstance;
				};

				drawCommands.push_back(drawCommand);
			}
		}

		const size_t				drawCommandsLength	= (m_parameters.viewIndex == TEST_TYPE_DRAW_INDIRECT_INDEXED)
														? drawCommandsIndexed.size()
														: drawCommands.size();
		const void*					drawCommandsDataPtr	= (m_parameters.viewIndex == TEST_TYPE_DRAW_INDIRECT_INDEXED)
														? (void*)&drawCommandsIndexed[0]
														: (void*)&drawCommands[0];
		const size_t				dataSize			= static_cast<size_t>(drawCommandsLength * strideInBuffer);
		const VkDeviceSize			bufferDataSize		= static_cast<VkDeviceSize>(deAlignSize(dataSize, nonCoherentAtomSize));
		const VkBufferCreateInfo	bufferInfo			= makeBufferCreateInfo(bufferDataSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
		Move<VkBuffer>				indirectBuffer		= createBuffer(*m_device, *m_logicalDevice, &bufferInfo);
		MovePtr<Allocation>			allocationBuffer	= m_allocator->allocate(getBufferMemoryRequirements(*m_device, *m_logicalDevice, *indirectBuffer),  MemoryRequirement::HostVisible);

		DE_ASSERT(drawCommandsLength != 0);

		VK_CHECK(m_device->bindBufferMemory(*m_logicalDevice, *indirectBuffer, allocationBuffer->getMemory(), allocationBuffer->getOffset()));

		deMemcpy(allocationBuffer->getHostPtr(), drawCommandsDataPtr, static_cast<size_t>(dataSize));

		flushAlloc(*m_device, *m_logicalDevice, *allocationBuffer);
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

	cmdBeginRenderPass(*m_device, *m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE, m_parameters.renderPassType);

	m_device->cmdBindVertexBuffers(*m_cmdBuffer, 0u, DE_LENGTH_OF_ARRAY(vertexBuffers), vertexBuffers, vertexBufferOffsets);

	if (m_parameters.viewIndex == TEST_TYPE_DRAW_INDIRECT_INDEXED)
		m_device->cmdBindIndexBuffer(*m_cmdBuffer, *m_vertexIndicesBuffer, 0u, VK_INDEX_TYPE_UINT32);

	for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; subpassNdx++)
	{
		m_device->cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **pipelines[subpassNdx]);

		if (m_hasMultiDrawIndirect)
		{
			if (m_parameters.viewIndex == TEST_TYPE_DRAW_INDIRECT_INDEXED)
				m_device->cmdDrawIndexedIndirect(*m_cmdBuffer, **indirectBuffers[subpassNdx], 0u, drawCountPerSubpass, strideInBuffer);
			else
				m_device->cmdDrawIndirect(*m_cmdBuffer, **indirectBuffers[subpassNdx], 0u, drawCountPerSubpass, strideInBuffer);
		}
		else
		{
			for (deUint32 drawNdx = 0; drawNdx < drawCountPerSubpass; drawNdx++)
			{
				if (m_parameters.viewIndex == TEST_TYPE_DRAW_INDIRECT_INDEXED)
					m_device->cmdDrawIndexedIndirect(*m_cmdBuffer, **indirectBuffers[subpassNdx], drawNdx * strideInBuffer, 1, strideInBuffer);
				else
					m_device->cmdDrawIndirect(*m_cmdBuffer, **indirectBuffers[subpassNdx], drawNdx * strideInBuffer, 1, strideInBuffer);
			}
		}

		if (subpassNdx < subpassCount - 1u)
			cmdNextSubpass(*m_device, *m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE, m_parameters.renderPassType);
	}

	cmdEndRenderPass(*m_device, *m_cmdBuffer, m_parameters.renderPassType);

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

void MultiViewClearAttachmentsTestInstance::draw (const deUint32 subpassCount, VkRenderPass renderPass, VkFramebuffer frameBuffer, vector<PipelineSp>& pipelines)
{
	const VkRect2D					renderArea				= { { 0, 0 }, { m_parameters.extent.width, m_parameters.extent.height } };
	const VkClearValue				renderPassClearValue	= makeClearValueColor(tcu::Vec4(0.0f));
	const VkBuffer					vertexBuffers[]			= { *m_vertexCoordBuffer, *m_vertexColorBuffer };
	const VkDeviceSize				vertexBufferOffsets[]	= {                   0u,                   0u };
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

	cmdBeginRenderPass(*m_device, *m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE, m_parameters.renderPassType);

	m_device->cmdBindVertexBuffers(*m_cmdBuffer, 0u, DE_LENGTH_OF_ARRAY(vertexBuffers), vertexBuffers, vertexBufferOffsets);

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
			cmdNextSubpass(*m_device, *m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE, m_parameters.renderPassType);
	}

	cmdEndRenderPass(*m_device, *m_cmdBuffer, m_parameters.renderPassType);

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

	const VkRect2D					renderArea				= { { 0, 0 }, { m_parameters.extent.width, m_parameters.extent.height } };
	const VkClearValue				renderPassClearValue	= makeClearValueColor(tcu::Vec4(0.0f));
	const VkBuffer					vertexBuffers[]			= { *m_vertexCoordBuffer, *m_vertexColorBuffer };
	const VkDeviceSize				vertexBufferOffsets[]	= {                   0u,                   0u };
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

	cmdBeginRenderPass(*m_device, *m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS, m_parameters.renderPassType);

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
		m_device->cmdBindVertexBuffers(cmdBufferSecondary.back().get()->get(), 0u, DE_LENGTH_OF_ARRAY(vertexBuffers), vertexBuffers, vertexBufferOffsets);
		m_device->cmdBindPipeline(cmdBufferSecondary.back().get()->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, **pipelines[subpassNdx]);

		for (deUint32 drawNdx = 0u; drawNdx < drawCountPerSubpass; ++drawNdx)
			m_device->cmdDraw(cmdBufferSecondary.back().get()->get(), 4u, 1u, (drawNdx + subpassNdx % m_squareCount) * 4u, 0u);

		VK_CHECK(m_device->endCommandBuffer(cmdBufferSecondary.back().get()->get()));

		m_device->cmdExecuteCommands(*m_cmdBuffer, 1u, &cmdBufferSecondary.back().get()->get());
		if (subpassNdx < subpassCount - 1u)
			cmdNextSubpass(*m_device, *m_cmdBuffer, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS, m_parameters.renderPassType);
	}

	cmdEndRenderPass(*m_device, *m_cmdBuffer, m_parameters.renderPassType);

	afterDraw();

	VK_CHECK(m_device->endCommandBuffer(*m_cmdBuffer));
	submitCommandsAndWait(*m_device, *m_logicalDevice, m_queue, *m_cmdBuffer);
}

class MultiViewPointSizeTestInstance : public MultiViewRenderTestInstance
{
public:
				MultiViewPointSizeTestInstance	(Context& context, const TestParameters& parameters);
protected:
	void		validatePointSize				(const VkPhysicalDeviceLimits& limits, const deUint32 pointSize);
	void		createVertexData				(void);
	void		draw							(const deUint32			subpassCount,
												 VkRenderPass			renderPass,
												 VkFramebuffer			frameBuffer,
												 vector<PipelineSp>&	pipelines);
};

MultiViewPointSizeTestInstance::MultiViewPointSizeTestInstance (Context& context, const TestParameters& parameters)
	: MultiViewRenderTestInstance	(context, parameters)
{
	const InstanceInterface&		vki					= m_context.getInstanceInterface();
	const VkPhysicalDevice			physDevice			= m_context.getPhysicalDevice();
	const VkPhysicalDeviceLimits	limits				= getPhysicalDeviceProperties(vki, physDevice).limits;

	validatePointSize(limits, static_cast<deUint32>(TEST_POINT_SIZE_WIDE));
	validatePointSize(limits, static_cast<deUint32>(TEST_POINT_SIZE_SMALL));
}

void MultiViewPointSizeTestInstance::validatePointSize (const VkPhysicalDeviceLimits& limits, const deUint32 pointSize)
{
	const float	testPointSizeFloat	= static_cast<float>(pointSize);
	float		granuleCount		= 0.0f;

	if (!de::inRange(testPointSizeFloat, limits.pointSizeRange[0], limits.pointSizeRange[1]))
		TCU_THROW(NotSupportedError, "Required point size is outside of the the limits range");

	granuleCount = static_cast<float>(deCeilFloatToInt32((testPointSizeFloat - limits.pointSizeRange[0]) / limits.pointSizeGranularity));

	if (limits.pointSizeRange[0] + granuleCount * limits.pointSizeGranularity != testPointSizeFloat)
		TCU_THROW(NotSupportedError, "Granuliraty does not allow to get required point size");

	DE_ASSERT(pointSize + 1 <= m_parameters.extent.width / 2);
	DE_ASSERT(pointSize + 1 <= m_parameters.extent.height / 2);
}

void MultiViewPointSizeTestInstance::createVertexData (void)
{
	const float		pixelStepX	= 2.0f / static_cast<float>(m_parameters.extent.width);
	const float		pixelStepY	= 2.0f / static_cast<float>(m_parameters.extent.height);
	const int		pointMargin	= 1 + TEST_POINT_SIZE_WIDE / 2;

	appendVertex(tcu::Vec4(-1.0f + pointMargin * pixelStepX,-1.0f + pointMargin * pixelStepY, 1.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
	appendVertex(tcu::Vec4(-1.0f + pointMargin * pixelStepX, 0.0f + pointMargin * pixelStepY, 1.0f, 1.0f), tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f));
	appendVertex(tcu::Vec4( 0.0f + pointMargin * pixelStepX,-1.0f + pointMargin * pixelStepY, 1.0f, 1.0f), tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
	appendVertex(tcu::Vec4( 0.0f + pointMargin * pixelStepX, 0.0f + pointMargin * pixelStepY, 1.0f, 1.0f), tcu::Vec4(1.0f, 0.5f, 0.3f, 1.0f));
}

void MultiViewPointSizeTestInstance::draw (const deUint32 subpassCount, VkRenderPass renderPass, VkFramebuffer frameBuffer, vector<PipelineSp>& pipelines)
{
	const VkRect2D					renderArea				= { { 0, 0 }, { m_parameters.extent.width, m_parameters.extent.height } };
	const VkClearValue				renderPassClearValue	= makeClearValueColor(tcu::Vec4(0.0f));
	const VkBuffer					vertexBuffers[]			= { *m_vertexCoordBuffer, *m_vertexColorBuffer };
	const VkDeviceSize				vertexBufferOffsets[]	= {                   0u,                   0u };
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

	cmdBeginRenderPass(*m_device, *m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE, m_parameters.renderPassType);

	m_device->cmdBindVertexBuffers(*m_cmdBuffer, 0u, DE_LENGTH_OF_ARRAY(vertexBuffers), vertexBuffers, vertexBufferOffsets);

	for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; subpassNdx++)
	{
		m_device->cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **pipelines[subpassNdx]);

		for (deUint32 drawNdx = 0u; drawNdx < drawCountPerSubpass; ++drawNdx)
			m_device->cmdDraw(*m_cmdBuffer, 1u, 1u, drawNdx + subpassNdx % m_squareCount, 0u);

		if (subpassNdx < subpassCount - 1u)
			cmdNextSubpass(*m_device, *m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE, m_parameters.renderPassType);
	}

	cmdEndRenderPass(*m_device, *m_cmdBuffer, m_parameters.renderPassType);

	afterDraw();

	VK_CHECK(m_device->endCommandBuffer(*m_cmdBuffer));
	submitCommandsAndWait(*m_device, *m_logicalDevice, m_queue, *m_cmdBuffer);
}

class MultiViewMultsampleTestInstance : public MultiViewRenderTestInstance
{
public:
					MultiViewMultsampleTestInstance	(Context& context, const TestParameters& parameters);
protected:
	tcu::TestStatus	iterate							(void);
	void			createVertexData				(void);

	void			draw							(const deUint32			subpassCount,
													 VkRenderPass			renderPass,
													 VkFramebuffer			frameBuffer,
													 vector<PipelineSp>&	pipelines);
	void			afterDraw						(void);
private:
	de::SharedPtr<ImageAttachment>	m_resolveAttachment;
};

MultiViewMultsampleTestInstance::MultiViewMultsampleTestInstance (Context& context, const TestParameters& parameters)
	: MultiViewRenderTestInstance	(context, parameters)
{
	// Color attachment
	m_resolveAttachment = de::SharedPtr<ImageAttachment>(new ImageAttachment(*m_logicalDevice, *m_device, *m_allocator, m_parameters.extent, m_parameters.colorFormat, VK_SAMPLE_COUNT_1_BIT));
}

tcu::TestStatus MultiViewMultsampleTestInstance::iterate (void)
{
	const deUint32								subpassCount				= static_cast<deUint32>(m_parameters.viewMasks.size());

	// FrameBuffer & renderPass
	Unique<VkRenderPass>						renderPass					(makeRenderPass (*m_device, *m_logicalDevice, m_parameters.colorFormat, m_parameters.viewMasks, m_parameters.renderPassType, VK_SAMPLE_COUNT_4_BIT));

	Unique<VkFramebuffer>						frameBuffer					(makeFramebuffer(*m_device, *m_logicalDevice, *renderPass, m_colorAttachment->getImageView(), m_parameters.extent.width, m_parameters.extent.height));

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
		vector<deUint8>			pixelAccessData	(m_parameters.extent.width * m_parameters.extent.height * m_parameters.extent.depth * mapVkFormat(m_parameters.colorFormat).getPixelSize());
		tcu::PixelBufferAccess	dst				(mapVkFormat(m_parameters.colorFormat), m_parameters.extent.width, m_parameters.extent.height, m_parameters.extent.depth, pixelAccessData.data());

		readImage(m_resolveAttachment->getImage(), dst);

		if (!checkImage(dst))
			return tcu::TestStatus::fail("Fail");
	}

	return tcu::TestStatus::pass("Pass");
}

void MultiViewMultsampleTestInstance::createVertexData (void)
{
	tcu::Vec4	color	= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);

	color	= tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f);
	appendVertex(tcu::Vec4(-1.0f, 0.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4(-1.0f,-1.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f,-1.0f, 1.0f, 1.0f), color);

	color	= tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f);
	appendVertex(tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4(-1.0f, 0.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), color);

	color	= tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
	appendVertex(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f,-1.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 1.0f,-1.0f, 1.0f, 1.0f), color);

	color	= tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
	appendVertex(tcu::Vec4( 0.0f, 1.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 0.0f, 1.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 1.0f, 0.0f, 1.0f, 1.0f), color);
}

void MultiViewMultsampleTestInstance::draw (const deUint32 subpassCount, VkRenderPass renderPass, VkFramebuffer frameBuffer, vector<PipelineSp>& pipelines)
{
	const VkRect2D					renderArea				= { { 0, 0 }, { m_parameters.extent.width, m_parameters.extent.height } };
	const VkClearValue				renderPassClearValue	= makeClearValueColor(tcu::Vec4(0.0f));
	const VkBuffer					vertexBuffers[]			= { *m_vertexCoordBuffer, *m_vertexColorBuffer };
	const VkDeviceSize				vertexBufferOffsets[]	= {                   0u,                   0u };
	const deUint32					drawCountPerSubpass		= (subpassCount == 1) ? m_squareCount : 1u;
	const deUint32					vertexPerPrimitive		= 3u;
	const VkImageSubresourceLayers	subresourceLayer		=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,	//  VkImageAspectFlags	aspectMask;
		0u,							//  deUint32			mipLevel;
		0u,							//  deUint32			baseArrayLayer;
		m_parameters.extent.depth,	//  deUint32			layerCount;
	};
	const VkImageResolve			imageResolveRegion		=
	{
		subresourceLayer,															//  VkImageSubresourceLayers	srcSubresource;
		makeOffset3D(0, 0, 0),														//  VkOffset3D					srcOffset;
		subresourceLayer,															//  VkImageSubresourceLayers	dstSubresource;
		makeOffset3D(0, 0, 0),														//  VkOffset3D					dstOffset;
		makeExtent3D(m_parameters.extent.width, m_parameters.extent.height, 1u),	//  VkExtent3D					extent;
	};

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

	cmdBeginRenderPass(*m_device, *m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE, m_parameters.renderPassType);

	m_device->cmdBindVertexBuffers(*m_cmdBuffer, 0u, DE_LENGTH_OF_ARRAY(vertexBuffers), vertexBuffers, vertexBufferOffsets);

	for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; subpassNdx++)
	{
		m_device->cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **pipelines[subpassNdx]);

		for (deUint32 drawNdx = 0u; drawNdx < drawCountPerSubpass; ++drawNdx)
			m_device->cmdDraw(*m_cmdBuffer, vertexPerPrimitive, 1u, (drawNdx + subpassNdx % m_squareCount) * vertexPerPrimitive, 0u);

		if (subpassNdx < subpassCount - 1u)
			cmdNextSubpass(*m_device, *m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE, m_parameters.renderPassType);
	}

	cmdEndRenderPass(*m_device, *m_cmdBuffer, m_parameters.renderPassType);

	afterDraw();

	m_device->cmdResolveImage(*m_cmdBuffer, m_colorAttachment->getImage(), VK_IMAGE_LAYOUT_GENERAL, m_resolveAttachment->getImage(), VK_IMAGE_LAYOUT_GENERAL, 1u, &imageResolveRegion);

	VK_CHECK(m_device->endCommandBuffer(*m_cmdBuffer));
	submitCommandsAndWait(*m_device, *m_logicalDevice, m_queue, *m_cmdBuffer);
}

void MultiViewMultsampleTestInstance::afterDraw (void)
{
	const VkImageSubresourceRange	subresourceRange		=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,	//  VkImageAspectFlags	aspectMask;
		0u,							//  deUint32			baseMipLevel;
		1u,							//  deUint32			levelCount;
		0u,							//  deUint32			baseArrayLayer;
		m_parameters.extent.depth,	//  deUint32			layerCount;
	};

	imageBarrier(*m_device, *m_cmdBuffer, m_colorAttachment->getImage(), subresourceRange,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	imageBarrier(*m_device, *m_cmdBuffer, m_resolveAttachment->getImage(), subresourceRange,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		0u, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
}

class MultiViewQueriesTestInstance : public MultiViewRenderTestInstance
{
public:
						MultiViewQueriesTestInstance	(Context& context, const TestParameters& parameters);
protected:
	tcu::TestStatus		iterate							(void);
	void				createVertexData				(void);

	void				draw							(const deUint32			subpassCount,
														 VkRenderPass			renderPass,
														 VkFramebuffer			frameBuffer,
														 vector<PipelineSp>&	pipelines);
	deUint32			getUsedViewsCount				(const deUint32			viewMaskIndex);
	deUint32			getQueryCountersNumber			();
private:
	const deUint32				m_verticesPerPrimitive;
	const VkQueryControlFlags	m_occlusionQueryFlags;
	deUint64					m_timestampMask;
	vector<deUint64>			m_timestampStartValues;
	vector<deUint64>			m_timestampEndValues;
	vector<deBool>				m_counterSeriesStart;
	vector<deBool>				m_counterSeriesEnd;
	vector<deUint64>			m_occlusionValues;
	vector<deUint64>			m_occlusionExpectedValues;
	deUint32					m_occlusionObjectsOffset;
	vector<deUint64>			m_occlusionObjectPixelsCount;
};

MultiViewQueriesTestInstance::MultiViewQueriesTestInstance (Context& context, const TestParameters& parameters)
	: MultiViewRenderTestInstance	(context, parameters)
	, m_verticesPerPrimitive		(4u)
	, m_occlusionQueryFlags			((parameters.viewIndex == TEST_TYPE_QUERIES) * VK_QUERY_CONTROL_PRECISE_BIT)
{
	// Generate the timestamp mask
	const std::vector<VkQueueFamilyProperties>	queueProperties = vk::getPhysicalDeviceQueueFamilyProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice());

	if(queueProperties[0].timestampValidBits == 0)
		TCU_THROW(NotSupportedError, "Device does not support timestamp.");

	m_timestampMask = 0xFFFFFFFFFFFFFFFFull >> (64 - queueProperties[0].timestampValidBits);
}

tcu::TestStatus MultiViewQueriesTestInstance::iterate (void)
{
	const deUint32								subpassCount			= static_cast<deUint32>(m_parameters.viewMasks.size());
	Unique<VkRenderPass>						renderPass				(makeRenderPass (*m_device, *m_logicalDevice, m_parameters.colorFormat, m_parameters.viewMasks, m_parameters.renderPassType));
	Unique<VkFramebuffer>						frameBuffer				(makeFramebuffer(*m_device, *m_logicalDevice, *renderPass, m_colorAttachment->getImageView(), m_parameters.extent.width, m_parameters.extent.height));
	Unique<VkPipelineLayout>					pipelineLayout			(makePipelineLayout(*m_device, *m_logicalDevice));
	vector<PipelineSp>							pipelines				(subpassCount);
	deUint64									occlusionValue			= 0;
	deUint64									occlusionExpectedValue	= 0;
	map<VkShaderStageFlagBits, ShaderModuleSP>	shaderModule;

	{
		vector<VkPipelineShaderStageCreateInfo>	shaderStageParams;

		madeShaderModule(shaderModule, shaderStageParams);
		for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; ++subpassNdx)
			pipelines[subpassNdx] = (PipelineSp(new Unique<VkPipeline>(makeGraphicsPipeline(*renderPass, *pipelineLayout, static_cast<deUint32>(shaderStageParams.size()), shaderStageParams.data(), subpassNdx))));
	}

	createCommandBuffer();
	createVertexData();
	createVertexBuffer();

	draw(subpassCount, *renderPass, *frameBuffer, pipelines);

	DE_ASSERT(!m_occlusionValues.empty());
	DE_ASSERT(m_occlusionValues.size() == m_occlusionExpectedValues.size());
	DE_ASSERT(m_occlusionValues.size() == m_counterSeriesEnd.size());
	for (size_t ndx = 0; ndx < m_counterSeriesEnd.size(); ++ndx)
	{
		occlusionValue			+= m_occlusionValues[ndx];
		occlusionExpectedValue	+= m_occlusionExpectedValues[ndx];

		if (m_counterSeriesEnd[ndx])
		{
			if (m_parameters.viewIndex == TEST_TYPE_QUERIES)
			{
				if (occlusionExpectedValue != occlusionValue)
					return tcu::TestStatus::fail("occlusion, result:" + de::toString(occlusionValue) + ", expected:" + de::toString(occlusionExpectedValue));
			}
			else // verify non precise occlusion query
			{
				if (occlusionValue == 0)
					return tcu::TestStatus::fail("occlusion, result: 0, expected non zero value");
			}
		}
	}

	DE_ASSERT(!m_timestampStartValues.empty());
	DE_ASSERT(m_timestampStartValues.size() == m_timestampEndValues.size());
	DE_ASSERT(m_timestampStartValues.size() == m_counterSeriesStart.size());
	for (size_t ndx = 0; ndx < m_timestampStartValues.size(); ++ndx)
	{
		if (m_counterSeriesStart[ndx])
		{
			if (m_timestampEndValues[ndx] > 0 && m_timestampEndValues[ndx] >= m_timestampStartValues[ndx])
				continue;
		}
		else
		{
			if (m_timestampEndValues[ndx] > 0 && m_timestampEndValues[ndx] >= m_timestampStartValues[ndx])
				continue;

			if (m_timestampEndValues[ndx] == 0 && m_timestampStartValues[ndx] == 0)
				continue;
		}

		return tcu::TestStatus::fail("timestamp");
	}

	return tcu::TestStatus::pass("Pass");
}

void MultiViewQueriesTestInstance::createVertexData (void)
{
	tcu::Vec4 color = tcu::Vec4(0.2f, 0.0f, 0.1f, 1.0f);

	appendVertex(tcu::Vec4(-1.0f,-1.0f, 0.0f, 1.0f), color);
	appendVertex(tcu::Vec4(-1.0f, 0.0f, 0.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f,-1.0f, 0.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f), color);

	color = tcu::Vec4(0.3f, 0.0f, 0.2f, 1.0f);
	appendVertex(tcu::Vec4(-1.0f, 0.0f, 0.0f, 1.0f), color);
	appendVertex(tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 1.0f, 0.0f, 1.0f), color);

	color = tcu::Vec4(0.4f, 0.2f, 0.3f, 1.0f);
	appendVertex(tcu::Vec4( 0.0f,-1.0f, 0.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 1.0f,-1.0f, 0.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 1.0f, 0.0f, 0.0f, 1.0f), color);

	color = tcu::Vec4(0.5f, 0.0f, 0.4f, 1.0f);
	appendVertex(tcu::Vec4( 0.0f, 0.0f, 0.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 1.0f, 0.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 1.0f, 0.0f, 0.0f, 1.0f), color);
	appendVertex(tcu::Vec4( 1.0f, 1.0f, 0.0f, 1.0f), color);

	// Create occluded square objects as zoom out of main
	const deUint32	mainObjectsVerticesCount		= static_cast<deUint32>(m_vertexCoord.size());
	const deUint32	mainObjectsCount				= mainObjectsVerticesCount / m_verticesPerPrimitive;
	const deUint32	occlusionObjectMultiplierX[]	= { 1, 2, 2, 1 };
	const deUint32	occlusionObjectMultiplierY[]	= { 1, 1, 3, 3 };
	const deUint32	occlusionObjectDivisor			= 4u;
	const float		occlusionObjectDivisorFloat		= static_cast<float>(occlusionObjectDivisor);

	DE_ASSERT(0 == m_parameters.extent.width  % (2 * occlusionObjectDivisor));
	DE_ASSERT(0 == m_parameters.extent.height % (2 * occlusionObjectDivisor));
	DE_ASSERT(DE_LENGTH_OF_ARRAY(occlusionObjectMultiplierX) == mainObjectsCount);
	DE_ASSERT(DE_LENGTH_OF_ARRAY(occlusionObjectMultiplierY) == mainObjectsCount);

	for (size_t objectNdx = 0; objectNdx < mainObjectsCount; ++objectNdx)
	{
		const size_t	objectStart			= objectNdx * m_verticesPerPrimitive;
		const float		xRatio				= static_cast<float>(occlusionObjectMultiplierX[objectNdx]) / occlusionObjectDivisorFloat;
		const float		yRatio				= static_cast<float>(occlusionObjectMultiplierY[objectNdx]) / occlusionObjectDivisorFloat;
		const double	areaRatio			= static_cast<double>(xRatio) * static_cast<double>(yRatio);
		const deUint64	occludedPixelsCount	= static_cast<deUint64>(areaRatio * (m_parameters.extent.width / 2) * (m_parameters.extent.height / 2));

		m_occlusionObjectPixelsCount.push_back(occludedPixelsCount);

		for (size_t vertexNdx = 0; vertexNdx < m_verticesPerPrimitive; ++vertexNdx)
		{
			const float		occludedObjectVertexXCoord	= m_vertexCoord[objectStart + vertexNdx][0] * xRatio;
			const float		occludedObjectVertexYCoord	= m_vertexCoord[objectStart + vertexNdx][1] * yRatio;
			const tcu::Vec4	occludedObjectVertexCoord	= tcu::Vec4(occludedObjectVertexXCoord, occludedObjectVertexYCoord, 1.0f, 1.0f);

			appendVertex(occludedObjectVertexCoord, m_vertexColor[objectStart + vertexNdx]);
		}
	}

	m_occlusionObjectsOffset = mainObjectsVerticesCount;
}

void MultiViewQueriesTestInstance::draw (const deUint32 subpassCount, VkRenderPass renderPass, VkFramebuffer frameBuffer, vector<PipelineSp>& pipelines)
{
	const VkRect2D				renderArea						= { { 0, 0 }, { m_parameters.extent.width, m_parameters.extent.height } };
	const VkClearValue			renderPassClearValue			= makeClearValueColor(tcu::Vec4(0.0f));
	const VkBuffer				vertexBuffers[]					= { *m_vertexCoordBuffer, *m_vertexColorBuffer };
	const VkDeviceSize			vertexBufferOffsets[]			= {                   0u,                   0u };
	const deUint32				drawCountPerSubpass				= (subpassCount == 1) ? m_squareCount : 1u;
	const deUint32				queryCountersNumber				= (subpassCount == 1) ? m_squareCount * getUsedViewsCount(0) : getQueryCountersNumber();
	const VkRenderPassBeginInfo	renderPassBeginInfo				=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	//  VkStructureType		sType;
		DE_NULL,									//  const void*			pNext;
		renderPass,									//  VkRenderPass		renderPass;
		frameBuffer,								//  VkFramebuffer		framebuffer;
		renderArea,									//  VkRect2D			renderArea;
		1u,											//  uint32_t			clearValueCount;
		&renderPassClearValue,						//  const VkClearValue*	pClearValues;
	};
	const VkQueryPoolCreateInfo	occlusionQueryPoolCreateInfo	=
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,	//  VkStructureType					sType;
		DE_NULL,									//  const void*						pNext;
		(VkQueryPoolCreateFlags)0,					//  VkQueryPoolCreateFlags			flags;
		VK_QUERY_TYPE_OCCLUSION,					//  VkQueryType						queryType;
		queryCountersNumber,						//  deUint32						queryCount;
		0u,											//  VkQueryPipelineStatisticFlags	pipelineStatistics;
	};
	const VkQueryPoolCreateInfo	timestampQueryPoolCreateInfo	=
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,	//  VkStructureType					sType;
		DE_NULL,									//  const void*						pNext;
		(VkQueryPoolCreateFlags)0,					//  VkQueryPoolCreateFlags			flags;
		VK_QUERY_TYPE_TIMESTAMP,					//  VkQueryType						queryType;
		queryCountersNumber,						//  deUint32						queryCount;
		0u,											//  VkQueryPipelineStatisticFlags	pipelineStatistics;
	};
	const Unique<VkQueryPool>	occlusionQueryPool				(createQueryPool(*m_device, *m_logicalDevice, &occlusionQueryPoolCreateInfo));
	const Unique<VkQueryPool>	timestampStartQueryPool			(createQueryPool(*m_device, *m_logicalDevice, &timestampQueryPoolCreateInfo));
	const Unique<VkQueryPool>	timestampEndQueryPool			(createQueryPool(*m_device, *m_logicalDevice, &timestampQueryPoolCreateInfo));
	deUint32					queryStartIndex					= 0;

	beginCommandBuffer(*m_device, *m_cmdBuffer);

	beforeDraw();

	// Query pools must be reset before use
	m_device->cmdResetQueryPool(*m_cmdBuffer, *occlusionQueryPool, queryStartIndex, queryCountersNumber);
	m_device->cmdResetQueryPool(*m_cmdBuffer, *timestampStartQueryPool, queryStartIndex, queryCountersNumber);
	m_device->cmdResetQueryPool(*m_cmdBuffer, *timestampEndQueryPool, queryStartIndex, queryCountersNumber);

	cmdBeginRenderPass(*m_device, *m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE, m_parameters.renderPassType);

	m_device->cmdBindVertexBuffers(*m_cmdBuffer, 0u, DE_LENGTH_OF_ARRAY(vertexBuffers), vertexBuffers, vertexBufferOffsets);

	m_occlusionExpectedValues.reserve(queryCountersNumber);
	m_counterSeriesStart.reserve(queryCountersNumber);
	m_counterSeriesEnd.reserve(queryCountersNumber);

	for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; subpassNdx++)
	{
		deUint32	queryCountersToUse	= getUsedViewsCount(subpassNdx);

		m_device->cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **pipelines[subpassNdx]);

		for (deUint32 drawNdx = 0u; drawNdx < drawCountPerSubpass; ++drawNdx)
		{
			const deUint32 primitiveNumber	= drawNdx + subpassNdx % m_squareCount;
			const deUint32 firstVertex		= primitiveNumber * m_verticesPerPrimitive;

			m_device->cmdWriteTimestamp(*m_cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, *timestampStartQueryPool, queryStartIndex);
			{
				m_device->cmdDraw(*m_cmdBuffer, m_verticesPerPrimitive, 1u, firstVertex, 0u);

				// Render occluded object
				m_device->cmdBeginQuery(*m_cmdBuffer, *occlusionQueryPool, queryStartIndex, m_occlusionQueryFlags);
				m_device->cmdDraw(*m_cmdBuffer, m_verticesPerPrimitive, 1u, m_occlusionObjectsOffset + firstVertex, 0u);
				m_device->cmdEndQuery(*m_cmdBuffer, *occlusionQueryPool, queryStartIndex);

				for (deUint32 viewMaskNdx = 0; viewMaskNdx < queryCountersToUse; ++viewMaskNdx)
				{
					m_occlusionExpectedValues.push_back(m_occlusionObjectPixelsCount[primitiveNumber]);
					m_counterSeriesStart.push_back(viewMaskNdx == 0);
					m_counterSeriesEnd.push_back(viewMaskNdx + 1 == queryCountersToUse);
				}
			}
			m_device->cmdWriteTimestamp(*m_cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, *timestampEndQueryPool, queryStartIndex);

			queryStartIndex += queryCountersToUse;
		}

		if (subpassNdx < subpassCount - 1u)
			cmdNextSubpass(*m_device, *m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE, m_parameters.renderPassType);
	}

	DE_ASSERT(queryStartIndex == queryCountersNumber);

	cmdEndRenderPass(*m_device, *m_cmdBuffer, m_parameters.renderPassType);

	afterDraw();

	VK_CHECK(m_device->endCommandBuffer(*m_cmdBuffer));
	submitCommandsAndWait(*m_device, *m_logicalDevice, m_queue, *m_cmdBuffer);

	m_occlusionValues.resize(queryCountersNumber, 0ull);
	m_device->getQueryPoolResults(*m_logicalDevice, *occlusionQueryPool, 0u, queryCountersNumber, sizeof(deUint64) * queryCountersNumber, (void*)&m_occlusionValues[0], sizeof(deUint64), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

	m_timestampStartValues.resize(queryCountersNumber, 0ull);
	m_device->getQueryPoolResults(*m_logicalDevice, *timestampStartQueryPool, 0u, queryCountersNumber, sizeof(deUint64) * queryCountersNumber, (void*)&m_timestampStartValues[0], sizeof(deUint64), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
	for (deUint32 ndx = 0; ndx < m_timestampStartValues.size(); ++ndx)
		m_timestampStartValues[ndx] &= m_timestampMask;

	m_timestampEndValues.resize(queryCountersNumber, 0ull);
	m_device->getQueryPoolResults(*m_logicalDevice, *timestampEndQueryPool, 0u, queryCountersNumber, sizeof(deUint64) * queryCountersNumber, (void*)&m_timestampEndValues[0], sizeof(deUint64), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
	for (deUint32 ndx = 0; ndx < m_timestampEndValues.size(); ++ndx)
		m_timestampEndValues[ndx] &= m_timestampMask;
}

deUint32 MultiViewQueriesTestInstance::getUsedViewsCount (const deUint32 viewMaskIndex)
{
	deUint32 result = 0;

	for (deUint32 viewMask = m_parameters.viewMasks[viewMaskIndex]; viewMask != 0; viewMask >>= 1)
		if ((viewMask & 1) != 0)
			result++;

	return result;
}

deUint32 MultiViewQueriesTestInstance::getQueryCountersNumber ()
{
	deUint32 result = 0;

	for (deUint32 i = 0; i < m_parameters.viewMasks.size(); ++i)
		result += getUsedViewsCount(i);

	return result;
}

class MultiViewReadbackTestInstance : public MultiViewRenderTestInstance
{
public:
						MultiViewReadbackTestInstance	(Context& context, const TestParameters& parameters);
protected:
	tcu::TestStatus		iterate							(void);
	void				drawClears						(const deUint32				subpassCount,
														 VkRenderPass				renderPass,
														 VkFramebuffer				frameBuffer,
														 vector<PipelineSp>&		pipelines,
														 const bool					clearPass);
	void				clear							(const VkCommandBuffer		commandBuffer,
														 const VkRect2D&			clearRect2D,
														 const tcu::Vec4&			clearColor);
private:
	vector<VkRect2D>	m_quarters;
};

MultiViewReadbackTestInstance::MultiViewReadbackTestInstance (Context& context, const TestParameters& parameters)
	: MultiViewRenderTestInstance	(context, parameters)
{
	const deUint32 halfWidth	= m_parameters.extent.width / 2;
	const deUint32 halfHeight	= m_parameters.extent.height / 2;

	for (deInt32 x = 0; x < 2; ++x)
	for (deInt32 y = 0; y < 2; ++y)
	{
		const deInt32	offsetX	= static_cast<deInt32>(halfWidth) * x;
		const deInt32	offsetY	= static_cast<deInt32>(halfHeight) * y;
		const VkRect2D	area	= { { offsetX, offsetY}, {halfWidth, halfHeight} };

		m_quarters.push_back(area);
	}
}

tcu::TestStatus MultiViewReadbackTestInstance::iterate (void)
{
	const deUint32	subpassCount	= static_cast<deUint32>(m_parameters.viewMasks.size());

	createCommandBuffer();

	for (deUint32 pass = 0; pass < 2; ++pass)
	{
		const bool									fullClearPass	= (pass == 0);
		const VkAttachmentLoadOp					loadOp			= (!fullClearPass) ? VK_ATTACHMENT_LOAD_OP_LOAD :
																	  (m_parameters.viewIndex == TEST_TYPE_READBACK_WITH_IMPLICIT_CLEAR) ? VK_ATTACHMENT_LOAD_OP_CLEAR :
																	  (m_parameters.viewIndex == TEST_TYPE_READBACK_WITH_EXPLICIT_CLEAR) ? VK_ATTACHMENT_LOAD_OP_DONT_CARE :
																	  VK_ATTACHMENT_LOAD_OP_LAST;
		Unique<VkRenderPass>						renderPass		(makeRenderPass (*m_device, *m_logicalDevice, m_parameters.colorFormat, m_parameters.viewMasks, m_parameters.renderPassType, VK_SAMPLE_COUNT_1_BIT, loadOp));
		Unique<VkFramebuffer>						frameBuffer		(makeFramebuffer(*m_device, *m_logicalDevice, *renderPass, m_colorAttachment->getImageView(), m_parameters.extent.width, m_parameters.extent.height));
		Unique<VkPipelineLayout>					pipelineLayout	(makePipelineLayout(*m_device, *m_logicalDevice));
		vector<PipelineSp>							pipelines		(subpassCount);
		map<VkShaderStageFlagBits, ShaderModuleSP>	shaderModule;

		{
			vector<VkPipelineShaderStageCreateInfo>	shaderStageParams;
			madeShaderModule(shaderModule, shaderStageParams);
			for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; ++subpassNdx)
				pipelines[subpassNdx] = (PipelineSp(new Unique<VkPipeline>(makeGraphicsPipeline(*renderPass, *pipelineLayout, static_cast<deUint32>(shaderStageParams.size()), shaderStageParams.data(), subpassNdx))));
		}

		drawClears(subpassCount, *renderPass, *frameBuffer, pipelines, fullClearPass);
	}

	{
		vector<deUint8>			pixelAccessData	(m_parameters.extent.width * m_parameters.extent.height * m_parameters.extent.depth * mapVkFormat(m_parameters.colorFormat).getPixelSize());
		tcu::PixelBufferAccess	dst				(mapVkFormat(m_parameters.colorFormat), m_parameters.extent.width, m_parameters.extent.height, m_parameters.extent.depth, pixelAccessData.data());

		readImage(m_colorAttachment->getImage(), dst);

		if (!checkImage(dst))
			return tcu::TestStatus::fail("Fail");
	}

	return tcu::TestStatus::pass("Pass");
}

void MultiViewReadbackTestInstance::drawClears (const deUint32 subpassCount, VkRenderPass renderPass, VkFramebuffer frameBuffer, vector<PipelineSp>& pipelines, const bool clearPass)
{
	const VkRect2D					renderArea				= { { 0, 0 }, { m_parameters.extent.width, m_parameters.extent.height } };
	const VkClearValue				renderPassClearValue	= makeClearValueColor(m_colorTable[0]);
	const deUint32					drawCountPerSubpass		= (subpassCount == 1) ? m_squareCount : 1u;
	const bool						withClearColor			= (clearPass && m_parameters.viewIndex == TEST_TYPE_READBACK_WITH_IMPLICIT_CLEAR);
	const VkRenderPassBeginInfo		renderPassBeginInfo		=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,			//  VkStructureType		sType;
		DE_NULL,											//  const void*			pNext;
		renderPass,											//  VkRenderPass		renderPass;
		frameBuffer,										//  VkFramebuffer		framebuffer;
		renderArea,											//  VkRect2D			renderArea;
		withClearColor ? 1u : 0u,							//  uint32_t			clearValueCount;
		withClearColor ? &renderPassClearValue : DE_NULL,	//  const VkClearValue*	pClearValues;
	};

	beginCommandBuffer(*m_device, *m_cmdBuffer);

	if (clearPass)
		beforeDraw();

	cmdBeginRenderPass(*m_device, *m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE, m_parameters.renderPassType);

	for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; subpassNdx++)
	{
		m_device->cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **pipelines[subpassNdx]);

		if (clearPass)
		{
			if (m_parameters.viewIndex == TEST_TYPE_READBACK_WITH_EXPLICIT_CLEAR)
				clear(*m_cmdBuffer, renderArea, m_colorTable[subpassNdx % 4]);
		}
		else
		{
			for (deUint32 drawNdx = 0u; drawNdx < drawCountPerSubpass; ++drawNdx)
			{
				const deUint32 primitiveNumber	= drawNdx + subpassNdx % m_squareCount;

				clear(*m_cmdBuffer, m_quarters[primitiveNumber], m_colorTable[4 + primitiveNumber]);
			}
		}

		if (subpassNdx < subpassCount - 1u)
			cmdNextSubpass(*m_device, *m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE, m_parameters.renderPassType);
	}

	cmdEndRenderPass(*m_device, *m_cmdBuffer, m_parameters.renderPassType);

	if (!clearPass)
		afterDraw();

	VK_CHECK(m_device->endCommandBuffer(*m_cmdBuffer));
	submitCommandsAndWait(*m_device, *m_logicalDevice, m_queue, *m_cmdBuffer);
}

void MultiViewReadbackTestInstance::clear (const VkCommandBuffer commandBuffer, const VkRect2D& clearRect2D, const tcu::Vec4& clearColor)
{
	const VkClearRect		clearRect		=
	{
		clearRect2D,						//  VkRect2D	rect
		0u,									//  deUint32	baseArrayLayer
		1u,									//  deUint32	layerCount
	};
	const VkClearAttachment	clearAttachment	=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,			//  VkImageAspectFlags	aspectMask
		0u,									//  deUint32			colorAttachment
		makeClearValueColor(clearColor)		//  VkClearValue		clearValue
	};

	m_device->cmdClearAttachments(commandBuffer, 1u, &clearAttachment, 1u, &clearRect);
}

class MultiViewDepthStencilTestInstance : public MultiViewRenderTestInstance
{
public:
						MultiViewDepthStencilTestInstance	(Context& context, const TestParameters& parameters);
protected:
	tcu::TestStatus		iterate								(void);
	void				createVertexData					(void);

	void				draw								(const deUint32					subpassCount,
															 VkRenderPass					renderPass,
															 VkFramebuffer					frameBuffer,
															 vector<PipelineSp>&			pipelines);
	void				beforeDraw							(void);
	void				afterDraw							(void);
	vector<VkImageView>	makeAttachmentsVector				(void);
	void				readImage							(VkImage						image,
															 const tcu::PixelBufferAccess&	dst);
private:
	VkFormat						m_dsFormat;
	de::SharedPtr<ImageAttachment>	m_dsAttachment;
	bool							m_depthTest;
	bool							m_stencilTest;
};

MultiViewDepthStencilTestInstance::MultiViewDepthStencilTestInstance (Context& context, const TestParameters& parameters)
	: MultiViewRenderTestInstance	(context, parameters)
	, m_dsFormat					(VK_FORMAT_UNDEFINED)
	, m_depthTest					(m_parameters.viewIndex == TEST_TYPE_DEPTH)
	, m_stencilTest					(m_parameters.viewIndex == TEST_TYPE_STENCIL)
{
	const VkFormat formats[] = { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT };

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(formats); ++ndx)
	{
		const VkFormat				format				= formats[ndx];
		const VkFormatProperties	formatProperties	= getPhysicalDeviceFormatProperties(context.getInstanceInterface(), context.getPhysicalDevice(), format);

		if ((formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
		{
			m_dsFormat = format;

			break;
		}
	}

	if (m_dsFormat == VK_FORMAT_UNDEFINED)
		TCU_FAIL("Supported depth/stencil format not found, that violates specification");

	// Depth/stencil attachment
	m_dsAttachment = de::SharedPtr<ImageAttachment>(new ImageAttachment(*m_logicalDevice, *m_device, *m_allocator, m_parameters.extent, m_dsFormat));
}

vector<VkImageView>	MultiViewDepthStencilTestInstance::makeAttachmentsVector (void)
{
	vector<VkImageView> attachments;

	attachments.push_back(m_colorAttachment->getImageView());
	attachments.push_back(m_dsAttachment->getImageView());

	return attachments;
}

void MultiViewDepthStencilTestInstance::readImage (VkImage image, const tcu::PixelBufferAccess& dst)
{
	const VkFormat				bufferFormat	= m_depthTest ? getDepthBufferFormat(m_dsFormat) :
												  m_stencilTest ? getStencilBufferFormat(m_dsFormat) :
												  VK_FORMAT_UNDEFINED;
	const deUint32				imagePixelSize	= static_cast<deUint32>(tcu::getPixelSize(mapVkFormat(bufferFormat)));
	const VkDeviceSize			pixelDataSize	= dst.getWidth() * dst.getHeight() * dst.getDepth() * imagePixelSize;
	const tcu::TextureFormat	tcuBufferFormat	= mapVkFormat(bufferFormat);
	Move<VkBuffer>				buffer;
	MovePtr<Allocation>			bufferAlloc;

	// Create destination buffer
	{
		const VkBufferCreateInfo bufferParams	=
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

		deMemset(bufferAlloc->getHostPtr(), 0xCC, static_cast<size_t>(pixelDataSize));
		flushAlloc(*m_device, *m_logicalDevice, *bufferAlloc);
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
	const VkImageAspectFlags	aspect			= m_depthTest ? static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT) :
												  m_stencilTest ? static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_STENCIL_BIT) :
												  static_cast<VkImageAspectFlags>(0u);
	const VkBufferImageCopy		copyRegion		=
	{
		0u,											// VkDeviceSize				bufferOffset;
		(deUint32)dst.getWidth(),					// deUint32					bufferRowLength;
		(deUint32)dst.getHeight(),					// deUint32					bufferImageHeight;
		{
			aspect,									// VkImageAspectFlags		aspect;
			0u,										// deUint32					mipLevel;
			0u,										// deUint32					baseArrayLayer;
			m_parameters.extent.depth,				// deUint32					layerCount;
		},											// VkImageSubresourceLayers	imageSubresource;
		{ 0, 0, 0 },								// VkOffset3D				imageOffset;
		{											// VkExtent3D				imageExtent;
			m_parameters.extent.width,
			m_parameters.extent.height,
			1u
		}
	};

	beginCommandBuffer (*m_device, *m_cmdBuffer);
	{
		m_device->cmdCopyImageToBuffer(*m_cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *buffer, 1u, &copyRegion);
		m_device->cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &bufferBarrier, 0u, DE_NULL);
	}
	VK_CHECK(m_device->endCommandBuffer(*m_cmdBuffer));
	submitCommandsAndWait(*m_device, *m_logicalDevice, m_queue, *m_cmdBuffer);

	// Read buffer data
	invalidateAlloc(*m_device, *m_logicalDevice, *bufferAlloc);

	if (m_depthTest)
	{
		// Translate depth into color space
		tcu::ConstPixelBufferAccess	pixelBuffer	(tcuBufferFormat, dst.getSize(), bufferAlloc->getHostPtr());

		for (int z = 0; z < pixelBuffer.getDepth(); z++)
		for (int y = 0; y < pixelBuffer.getHeight(); y++)
		for (int x = 0; x < pixelBuffer.getWidth(); x++)
		{
			const float		depth	= pixelBuffer.getPixDepth(x, y, z);
			const tcu::Vec4	color	= tcu::Vec4(depth, 0.0f, 0.0f, 1.0f);

			dst.setPixel(color, x, y, z);
		}
	}

	if (m_stencilTest)
	{
		// Translate stencil into color space
		tcu::ConstPixelBufferAccess	pixelBuffer	(tcuBufferFormat, dst.getSize(), bufferAlloc->getHostPtr());
		const tcu::Vec4				baseColor		= getQuarterRefColor(0u, 0u, 0u, false);
		const tcu::Vec4				colorStep		= getQuarterRefColor(0u, 0u, 0u, true);
		const tcu::Vec4				colorMap[4]		=
		{
			baseColor,
			tcu::Vec4(1.0f * colorStep[0], 0.0f, 0.0f, 1.0),
			tcu::Vec4(2.0f * colorStep[0], 0.0f, 0.0f, 1.0),
			tcu::Vec4(3.0f * colorStep[0], 0.0f, 0.0f, 1.0),
		};
		const tcu::Vec4				invalidColor	= tcu::Vec4(0.0f);

		for (int z = 0; z < pixelBuffer.getDepth(); z++)
		for (int y = 0; y < pixelBuffer.getHeight(); y++)
		for (int x = 0; x < pixelBuffer.getWidth(); x++)
		{
			const int			stencilInt	= pixelBuffer.getPixStencil(x, y, z);
			const tcu::Vec4&	color		= de::inRange(stencilInt, 0, DE_LENGTH_OF_ARRAY(colorMap)) ? colorMap[stencilInt] : invalidColor;

			dst.setPixel(color, x, y, z);
		}
	}

}

tcu::TestStatus MultiViewDepthStencilTestInstance::iterate (void)
{
	const deUint32								subpassCount				= static_cast<deUint32>(m_parameters.viewMasks.size());
	Unique<VkRenderPass>						renderPass					(makeRenderPassWithDepth (*m_device, *m_logicalDevice, m_parameters.colorFormat, m_parameters.viewMasks, m_dsFormat, m_parameters.renderPassType));
	vector<VkImageView>							attachments					(makeAttachmentsVector());
	Unique<VkFramebuffer>						frameBuffer					(makeFramebuffer(*m_device, *m_logicalDevice, *renderPass, static_cast<deUint32>(attachments.size()), attachments.data(), m_parameters.extent.width, m_parameters.extent.height, 1u));
	Unique<VkPipelineLayout>					pipelineLayout				(makePipelineLayout(*m_device, *m_logicalDevice));
	map<VkShaderStageFlagBits, ShaderModuleSP>	shaderModule;
	vector<PipelineSp>							pipelines(subpassCount);

	{
		vector<VkPipelineShaderStageCreateInfo>	shaderStageParams;
		madeShaderModule(shaderModule, shaderStageParams);
		for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; ++subpassNdx)
			pipelines[subpassNdx] = (PipelineSp(new Unique<VkPipeline>(makeGraphicsPipeline(*renderPass, *pipelineLayout, static_cast<deUint32>(shaderStageParams.size()), shaderStageParams.data(),
				subpassNdx, VK_VERTEX_INPUT_RATE_VERTEX, m_depthTest, m_stencilTest))));
	}

	createCommandBuffer();
	createVertexData();
	createVertexBuffer();

	draw(subpassCount, *renderPass, *frameBuffer, pipelines);

	{
		vector<deUint8>			pixelAccessData	(m_parameters.extent.width * m_parameters.extent.height * m_parameters.extent.depth * mapVkFormat(m_parameters.colorFormat).getPixelSize());
		tcu::PixelBufferAccess	dst				(mapVkFormat(m_parameters.colorFormat), m_parameters.extent.width, m_parameters.extent.height, m_parameters.extent.depth, pixelAccessData.data());

		readImage(m_dsAttachment->getImage(), dst);

		if (!checkImage(dst))
			return tcu::TestStatus::fail("Fail");
	}

	return tcu::TestStatus::pass("Pass");
}

void MultiViewDepthStencilTestInstance::createVertexData (void)
{
/*
	partA

	ViewMasks
	0011
	0110
	1100
	1001

	Layer3  Layer2  Layer1  Layer0
	  ^       ^       ^       ^
	00|10   00|10   01|00   01|00
	00|10   00|10   01|00   01|00
	--+-->  --+-->  --+-->  --+-->
	00|10   01|00   01|00   00|10
	00|10   01|00   01|00   00|10


	partB

	ViewMasks
	0110
	1100
	1001
	0011

	Layer3  Layer2  Layer1  Layer0
	  ^       ^       ^       ^
	00|00   00|00   00|00   00|00
	00|22   22|00   22|00   00|22
	--+-->  --+-->  --+-->  --+-->
	22|00   22|00   00|22   00|22
	00|00   00|00   00|00   00|00

	Final
	Layer3  Layer2  Layer1  Layer0
	  ^       ^       ^       ^
	00|10   00|10   01|00   01|00
	00|32   22|10   23|00   01|22
	--+-->  --+-->  --+-->  --+-->
	22|10   23|00   01|22   00|32
	00|10   01|00   01|00   00|10
*/
	tcu::Vec4	color	(0.0f, 0.0f, 0.0f, 1.0f); // is not essential in this test

	const tcu::Vec4	partAReference	= getQuarterRefColor(0u, 0u, 0u, true, 0u);
	const tcu::Vec4	partBReference	= getQuarterRefColor(0u, 0u, 0u, true, 4u);
	const float		depthA			= partAReference[0];
	const float		depthB			= partBReference[0];

	// part A
	appendVertex(tcu::Vec4(-1.0f,-0.5f, depthA, 1.0f), color);
	appendVertex(tcu::Vec4(-1.0f, 0.0f, depthA, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f,-0.5f, depthA, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 0.0f, depthA, 1.0f), color);

	appendVertex(tcu::Vec4(-1.0f, 0.0f, depthA, 1.0f), color);
	appendVertex(tcu::Vec4(-1.0f, 0.5f, depthA, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 0.0f, depthA, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 0.5f, depthA, 1.0f), color);

	appendVertex(tcu::Vec4( 0.0f,-0.5f, depthA, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 0.0f, depthA, 1.0f), color);
	appendVertex(tcu::Vec4( 1.0f,-0.5f, depthA, 1.0f), color);
	appendVertex(tcu::Vec4( 1.0f, 0.0f, depthA, 1.0f), color);

	appendVertex(tcu::Vec4( 0.0f, 0.0f, depthA, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 0.5f, depthA, 1.0f), color);
	appendVertex(tcu::Vec4( 1.0f, 0.0f, depthA, 1.0f), color);
	appendVertex(tcu::Vec4( 1.0f, 0.5f, depthA, 1.0f), color);

	// part B
	appendVertex(tcu::Vec4(-0.5f,-1.0f, depthB, 1.0f), color);
	appendVertex(tcu::Vec4(-0.5f, 0.0f, depthB, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f,-1.0f, depthB, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 0.0f, depthB, 1.0f), color);

	appendVertex(tcu::Vec4(-0.5f, 0.0f, depthB, 1.0f), color);
	appendVertex(tcu::Vec4(-0.5f, 1.0f, depthB, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 0.0f, depthB, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 1.0f, depthB, 1.0f), color);

	appendVertex(tcu::Vec4( 0.0f,-1.0f, depthB, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 0.0f, depthB, 1.0f), color);
	appendVertex(tcu::Vec4( 0.5f,-1.0f, depthB, 1.0f), color);
	appendVertex(tcu::Vec4( 0.5f, 0.0f, depthB, 1.0f), color);

	appendVertex(tcu::Vec4( 0.0f, 0.0f, depthB, 1.0f), color);
	appendVertex(tcu::Vec4( 0.0f, 1.0f, depthB, 1.0f), color);
	appendVertex(tcu::Vec4( 0.5f, 0.0f, depthB, 1.0f), color);
	appendVertex(tcu::Vec4( 0.5f, 1.0f, depthB, 1.0f), color);
}

void MultiViewDepthStencilTestInstance::draw (const deUint32 subpassCount, VkRenderPass renderPass, VkFramebuffer frameBuffer, vector<PipelineSp>& pipelines)
{
	const VkRect2D					renderArea				= { { 0, 0 }, { m_parameters.extent.width, m_parameters.extent.height } };
	const VkClearValue				renderPassClearValue	= makeClearValueColor(tcu::Vec4(0.0f));
	const VkBuffer					vertexBuffers[]			= { *m_vertexCoordBuffer, *m_vertexColorBuffer };
	const VkDeviceSize				vertexBufferOffsets[]	= {                   0u,                   0u };
	const deUint32					drawCountPerSubpass		= (subpassCount == 1) ? m_squareCount : 1u;
	const deUint32					vertexPerPrimitive		= 4u;
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

	cmdBeginRenderPass(*m_device, *m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE, m_parameters.renderPassType);

	m_device->cmdBindVertexBuffers(*m_cmdBuffer, 0u, DE_LENGTH_OF_ARRAY(vertexBuffers), vertexBuffers, vertexBufferOffsets);

	for (deUint32 subpassNdx = 0u; subpassNdx < subpassCount; subpassNdx++)
	{
		deUint32 firstVertexOffset = (subpassNdx < 4) ? 0u : m_squareCount * vertexPerPrimitive;

		m_device->cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **pipelines[subpassNdx]);

		for (deUint32 drawNdx = 0u; drawNdx < drawCountPerSubpass; ++drawNdx)
			m_device->cmdDraw(*m_cmdBuffer, vertexPerPrimitive, 1u, firstVertexOffset + (drawNdx + subpassNdx % m_squareCount) * vertexPerPrimitive, 0u);

		if (subpassNdx < subpassCount - 1u)
			cmdNextSubpass(*m_device, *m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE, m_parameters.renderPassType);
	}

	cmdEndRenderPass(*m_device, *m_cmdBuffer, m_parameters.renderPassType);

	afterDraw();

	VK_CHECK(m_device->endCommandBuffer(*m_cmdBuffer));
	submitCommandsAndWait(*m_device, *m_logicalDevice, m_queue, *m_cmdBuffer);
}

void MultiViewDepthStencilTestInstance::beforeDraw (void)
{
	MultiViewRenderTestInstance::beforeDraw();

	const VkImageSubresourceRange	subresourceRange		=
	{
		VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,	//VkImageAspectFlags	aspectMask;
		0u,															//deUint32				baseMipLevel;
		1u,															//deUint32				levelCount;
		0u,															//deUint32				baseArrayLayer;
		m_parameters.extent.depth,									//deUint32				layerCount;
	};
	imageBarrier(*m_device, *m_cmdBuffer, m_dsAttachment->getImage(), subresourceRange,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		0, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	const tcu::Vec4		baseColor	= getQuarterRefColor(0u, 0u, 0u, false);
	const float			clearDepth	= baseColor[0];
	const VkClearValue	clearValue	= makeClearValueDepthStencil(clearDepth, 0);

	m_device->cmdClearDepthStencilImage(*m_cmdBuffer, m_dsAttachment->getImage(),  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.depthStencil, 1, &subresourceRange);

	imageBarrier(*m_device, *m_cmdBuffer, m_dsAttachment->getImage(), subresourceRange,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
}

void MultiViewDepthStencilTestInstance::afterDraw (void)
{
	MultiViewRenderTestInstance::afterDraw();

	const VkImageSubresourceRange	dsSubresourceRange		=
	{
		VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,	//  VkImageAspectFlags	aspectMask;
		0u,															//  deUint32			baseMipLevel;
		1u,															//  deUint32			levelCount;
		0u,															//  deUint32			baseArrayLayer;
		m_parameters.extent.depth,									//  deUint32			layerCount;
	};

	imageBarrier(*m_device, *m_cmdBuffer, m_dsAttachment->getImage(), dsSubresourceRange,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
}

class MultiViewRenderTestsCase : public vkt::TestCase
{
public:
	MultiViewRenderTestsCase (tcu::TestContext &context, const char *name, const char *description, const TestParameters& parameters)
		: TestCase			(context, name, description)
		, m_parameters		(parameters)
	{
		DE_ASSERT(m_parameters.extent.width == m_parameters.extent.height);
	}
private:
	const TestParameters	m_parameters;

	vkt::TestInstance*	createInstance		(vkt::Context& context) const
	{
		if (TEST_TYPE_INPUT_ATTACHMENTS == m_parameters.viewIndex ||
			TEST_TYPE_INPUT_ATTACHMENTS_GEOMETRY == m_parameters.viewIndex)
			return new MultiViewAttachmentsTestInstance(context, m_parameters);

		if (TEST_TYPE_INSTANCED_RENDERING == m_parameters.viewIndex)
			return new MultiViewInstancedTestInstance(context, m_parameters);

		if (TEST_TYPE_INPUT_RATE_INSTANCE == m_parameters.viewIndex)
			return new MultiViewInputRateInstanceTestInstance(context, m_parameters);

		if (TEST_TYPE_DRAW_INDIRECT == m_parameters.viewIndex ||
			TEST_TYPE_DRAW_INDIRECT_INDEXED == m_parameters.viewIndex)
			return new MultiViewDrawIndirectTestInstance(context, m_parameters);

		if (TEST_TYPE_CLEAR_ATTACHMENTS == m_parameters.viewIndex)
			return new MultiViewClearAttachmentsTestInstance(context, m_parameters);

		if (TEST_TYPE_SECONDARY_CMD_BUFFER == m_parameters.viewIndex ||
			TEST_TYPE_SECONDARY_CMD_BUFFER_GEOMETRY == m_parameters.viewIndex)
			return new MultiViewSecondaryCommandBufferTestInstance(context, m_parameters);

		if (TEST_TYPE_POINT_SIZE == m_parameters.viewIndex)
			return new MultiViewPointSizeTestInstance(context, m_parameters);

		if (TEST_TYPE_MULTISAMPLE == m_parameters.viewIndex)
			return new MultiViewMultsampleTestInstance(context, m_parameters);

		if (TEST_TYPE_QUERIES == m_parameters.viewIndex ||
			TEST_TYPE_NON_PRECISE_QUERIES == m_parameters.viewIndex)
			return new MultiViewQueriesTestInstance(context, m_parameters);

		if (TEST_TYPE_VIEW_MASK == m_parameters.viewIndex ||
			TEST_TYPE_VIEW_INDEX_IN_VERTEX == m_parameters.viewIndex ||
			TEST_TYPE_VIEW_INDEX_IN_FRAGMENT == m_parameters.viewIndex ||
			TEST_TYPE_VIEW_INDEX_IN_GEOMETRY == m_parameters.viewIndex ||
			TEST_TYPE_VIEW_INDEX_IN_TESELLATION == m_parameters.viewIndex ||
			TEST_TYPE_DRAW_INDEXED == m_parameters.viewIndex)
			return new MultiViewRenderTestInstance(context, m_parameters);

		if (TEST_TYPE_READBACK_WITH_IMPLICIT_CLEAR == m_parameters.viewIndex ||
			TEST_TYPE_READBACK_WITH_EXPLICIT_CLEAR == m_parameters.viewIndex)
			return new MultiViewReadbackTestInstance(context, m_parameters);

		if (TEST_TYPE_DEPTH == m_parameters.viewIndex ||
			TEST_TYPE_STENCIL == m_parameters.viewIndex)
			return new MultiViewDepthStencilTestInstance(context, m_parameters);

		TCU_THROW(InternalError, "Unknown test type");
	}

	virtual void		checkSupport		(Context& context) const
	{
		context.requireDeviceFunctionality("VK_KHR_multiview");
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
		else if (TEST_TYPE_POINT_SIZE == m_parameters.viewIndex)
		{
			std::ostringstream source;
			source	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
					<< "#extension GL_EXT_multiview : enable\n"
					<< "layout(location = 0) in highp vec4 in_position;\n"
					<< "layout(location = 1) in highp vec4 in_color;\n"
					<< "layout(location = 0) out vec4 out_color;\n"
					<< "void main (void)\n"
					<< "{\n"
					<< "	gl_Position = in_position;\n"
					<< "	if (gl_ViewIndex == 0)\n"
					<< "		gl_PointSize = " << de::floatToString(static_cast<float>(TEST_POINT_SIZE_WIDE), 1) << "f;\n"
					<< "	else\n"
					<< "		gl_PointSize = " << de::floatToString(static_cast<float>(TEST_POINT_SIZE_SMALL), 1) << "f;\n"
					<< "	out_color = in_color;\n"
					<< "}\n";
			programCollection.glslSources.add("vertex") << glu::VertexSource(source.str());
		}
		else
		{
			const bool generateColor	=  (TEST_TYPE_VIEW_INDEX_IN_VERTEX == m_parameters.viewIndex)
										|| (TEST_TYPE_DRAW_INDIRECT == m_parameters.viewIndex)
										|| (TEST_TYPE_DRAW_INDIRECT_INDEXED == m_parameters.viewIndex)
										|| (TEST_TYPE_CLEAR_ATTACHMENTS == m_parameters.viewIndex);
			std::ostringstream source;
			source	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
					<< "#extension GL_EXT_multiview : enable\n"
					<< "layout(location = 0) in highp vec4 in_position;\n"
					<< "layout(location = 1) in vec4 in_color;\n"
					<< "layout(location = 0) out vec4 out_color;\n"
					<< "void main (void)\n"
					<< "{\n"
					<< "	gl_Position = in_position;\n";
				if (generateColor)
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

		if (TEST_TYPE_VIEW_INDEX_IN_GEOMETRY		== m_parameters.viewIndex ||
			TEST_TYPE_INPUT_ATTACHMENTS_GEOMETRY	== m_parameters.viewIndex ||
			TEST_TYPE_SECONDARY_CMD_BUFFER_GEOMETRY	== m_parameters.viewIndex)
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
				if (TEST_TYPE_VIEW_INDEX_IN_FRAGMENT == m_parameters.viewIndex ||
					TEST_TYPE_SECONDARY_CMD_BUFFER == m_parameters.viewIndex)
					source << "	out_color = in_color + vec4(0.0, gl_ViewIndex * 0.10f, 0.0, 0.0);\n";
				else
					source << "	out_color = in_color;\n";
			source	<< "}\n";
			programCollection.glslSources.add("fragment") << glu::FragmentSource(source.str());
		}
	}
};
} //anonymous

static std::string createViewMasksName(const std::vector<deUint32>& viewMasks)
{
	std::ostringstream		masks;

	for (size_t ndx = 0u; ndx < viewMasks.size(); ++ndx)
	{
		masks << viewMasks[ndx];
		if (viewMasks.size() - 1 != ndx)
			masks << "_";
	}

	return masks.str();
}

static std::vector<deUint32> tripleDepthStencilMasks(std::vector<deUint32>& baseMasks)
{
	std::vector<deUint32> tripledMasks(baseMasks);
	std::vector<deUint32> partBMasks;

	// a,b,c,d  =>  b,c,d,a
	partBMasks.insert(partBMasks.end(), baseMasks.begin() + 1, baseMasks.end());
	partBMasks.push_back(baseMasks[0]);

	tripledMasks.insert(tripledMasks.end(), partBMasks.begin(), partBMasks.end());
	tripledMasks.insert(tripledMasks.end(), partBMasks.begin(), partBMasks.end());

	return tripledMasks;
}

void multiViewRenderCreateTests (tcu::TestCaseGroup* group)
{
	const deUint32				testCaseCount				= 7u;
	const string				shaderName[TEST_TYPE_LAST]	=
	{
		"masks",
		"vertex_shader",
		"fragment_shader",
		"geometry_shader",
		"tesellation_shader",
		"input_attachments",
		"input_attachments_geometry",
		"instanced",
		"input_instance",
		"draw_indirect",
		"draw_indirect_indexed",
		"draw_indexed",
		"clear_attachments",
		"secondary_cmd_buffer",
		"secondary_cmd_buffer_geometry",
		"point_size",
		"multisample",
		"queries",
		"non_precise_queries",
		"readback_implicit_clear",
		"readback_explicit_clear",
		"depth",
		"stencil",
	};
	const VkExtent3D			extent3D[testCaseCount]		=
	{
		{16u,	16u,	4u},
		{64u,	64u,	8u},
		{128u,	128u,	4u},
		{32u,	32u,	5u},
		{64u,	64u,	6u},
		{32u,	32u,	4u},
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

	viewMasks[5].push_back(5u);		//0101
	viewMasks[5].push_back(10u);	//1010
	viewMasks[5].push_back(5u);		//0101
	viewMasks[5].push_back(10u);	//1010

	const deUint32 minSupportedMultiviewViewCount	= 6u;
	const deUint32 maxViewMask						= (1u << minSupportedMultiviewViewCount) - 1u;

	for (deUint32 mask = 1u; mask <= maxViewMask; mask = mask << 1u)
		viewMasks[testCaseCount - 1].push_back(mask);

	vector<deUint32>			depthStencilMasks;

	depthStencilMasks.push_back(3u);	// 0011
	depthStencilMasks.push_back(6u);	// 0110
	depthStencilMasks.push_back(12u);	// 1100
	depthStencilMasks.push_back(9u);	// 1001

	for (int renderPassTypeNdx = 0; renderPassTypeNdx < 2; ++renderPassTypeNdx)
	{
		RenderPassType				renderPassType		((renderPassTypeNdx == 0) ? RENDERPASS_TYPE_LEGACY : RENDERPASS_TYPE_RENDERPASS2);
		MovePtr<tcu::TestCaseGroup>	groupRenderPass2	((renderPassTypeNdx == 0) ? DE_NULL : new tcu::TestCaseGroup(group->getTestContext(), "renderpass2", "RenderPass2 index tests"));
		tcu::TestCaseGroup*			targetGroup			((renderPassTypeNdx == 0) ? group : groupRenderPass2.get());
		tcu::TestContext&			testCtx				(targetGroup->getTestContext());
		MovePtr<tcu::TestCaseGroup>	groupViewIndex		(new tcu::TestCaseGroup(testCtx, "index", "ViewIndex rendering tests."));

		for (int testTypeNdx = TEST_TYPE_VIEW_MASK; testTypeNdx < TEST_TYPE_LAST; ++testTypeNdx)
		{
			MovePtr<tcu::TestCaseGroup>	groupShader			(new tcu::TestCaseGroup(testCtx, shaderName[testTypeNdx].c_str(), ""));
			const TestType				testType			= static_cast<TestType>(testTypeNdx);
			const VkSampleCountFlagBits	sampleCountFlags	= (testType == TEST_TYPE_MULTISAMPLE) ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT;
			const VkFormat				colorFormat			= (testType == TEST_TYPE_MULTISAMPLE) ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;

			if (testTypeNdx == TEST_TYPE_DEPTH || testTypeNdx == TEST_TYPE_STENCIL)
			{
				const VkExtent3D		dsTestExtent3D	= { 64u, 64u, 4u };
				const TestParameters	parameters		= { dsTestExtent3D, tripleDepthStencilMasks(depthStencilMasks), testType, sampleCountFlags, colorFormat, renderPassType };
				const std::string		testName		= createViewMasksName(parameters.viewMasks);

				groupShader->addChild(new MultiViewRenderTestsCase(testCtx, testName.c_str(), "", parameters));
			}
			else
			{
				for (deUint32 testCaseNdx = 0u; testCaseNdx < testCaseCount; ++testCaseNdx)
				{
					const TestParameters	parameters	=	{ extent3D[testCaseNdx], viewMasks[testCaseNdx], testType, sampleCountFlags, colorFormat, renderPassType };
					const std::string		testName	=	createViewMasksName(parameters.viewMasks);

					groupShader->addChild(new MultiViewRenderTestsCase(testCtx, testName.c_str(), "", parameters));
				}

				// maxMultiviewViewCount case
				{
					const VkExtent3D		incompleteExtent3D	= { 16u, 16u, 0u };
					const vector<deUint32>	dummyMasks;
					const TestParameters	parameters			= { incompleteExtent3D, dummyMasks, testType, sampleCountFlags, colorFormat, renderPassType };

					groupShader->addChild(new MultiViewRenderTestsCase(testCtx, "max_multi_view_view_count", "", parameters));
				}
			}

			switch (testType)
			{
				case TEST_TYPE_VIEW_MASK:
				case TEST_TYPE_INPUT_ATTACHMENTS:
				case TEST_TYPE_INPUT_ATTACHMENTS_GEOMETRY:
				case TEST_TYPE_INSTANCED_RENDERING:
				case TEST_TYPE_INPUT_RATE_INSTANCE:
				case TEST_TYPE_DRAW_INDIRECT:
				case TEST_TYPE_DRAW_INDIRECT_INDEXED:
				case TEST_TYPE_DRAW_INDEXED:
				case TEST_TYPE_CLEAR_ATTACHMENTS:
				case TEST_TYPE_SECONDARY_CMD_BUFFER:
				case TEST_TYPE_SECONDARY_CMD_BUFFER_GEOMETRY:
				case TEST_TYPE_POINT_SIZE:
				case TEST_TYPE_MULTISAMPLE:
				case TEST_TYPE_QUERIES:
				case TEST_TYPE_NON_PRECISE_QUERIES:
				case TEST_TYPE_READBACK_WITH_IMPLICIT_CLEAR:
				case TEST_TYPE_READBACK_WITH_EXPLICIT_CLEAR:
				case TEST_TYPE_DEPTH:
				case TEST_TYPE_STENCIL:
					targetGroup->addChild(groupShader.release());
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

		targetGroup->addChild(groupViewIndex.release());

		if (renderPassTypeNdx == 1)
			group->addChild(groupRenderPass2.release());
	}
}

} //MultiView
} //vkt

