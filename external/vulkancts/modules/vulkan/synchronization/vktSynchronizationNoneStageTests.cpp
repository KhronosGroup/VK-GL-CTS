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
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file vktSynchronizationNoneStageTests.cpp
 * \brief Tests for VK_PIPELINE_STAGE_NONE{_2}_KHR that iterate over each writable layout
		  and over each readable layout. Data to tested image is writen using method
		  appropriate for the writable layout and read via readable layout appropriate method.
		  Betwean read and write operation there are bariers that use none stage.
		  Implemented tests are also testing generalized layouts (VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR,
		  VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR) and access flags (MEMORY_ACCESS_READ|WRITE_BIT) to
		  test contextual synchronization introduced with VK_KHR_synchronization2 extension.
 *//*--------------------------------------------------------------------*/

#include "vktSynchronizationNoneStageTests.hpp"
#include "vktSynchronizationOperation.hpp"
#include "vktSynchronizationUtil.hpp"
#include "vktTestCase.hpp"

#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuStringTemplate.hpp"

#include "deUniquePtr.hpp"

#include <vector>

namespace vkt
{
namespace synchronization
{

using namespace vk;
using namespace de;
using namespace tcu;

namespace
{

static const deUint32 IMAGE_ASPECT_DEPTH_STENCIL	= VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
static const deUint32 IMAGE_ASPECT_ALL				= 0u;

struct TestParams
{
	SynchronizationType		type;
	bool					useGenericAccessFlags;
	VkImageLayout			writeLayout;
	VkImageAspectFlags		writeAspect;
	VkImageLayout			readLayout;
	VkImageAspectFlags		readAspect;
};

// Helper class representing image
class ImageWrapper
{
public:

			ImageWrapper	() = default;
	void	create			(Context& context, SimpleAllocator& alloc, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage);

public:
	Move<VkImage>			handle;
	MovePtr<Allocation>		memory;
};

void ImageWrapper::create(Context& context, SimpleAllocator& alloc, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage)
{
	const DeviceInterface&	vk					= context.getDeviceInterface();
	const VkDevice&			device				= context.getDevice();
	const deUint32			queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	const VkImageCreateInfo imageParams
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// sType
		DE_NULL,								// pNext
		0u,										// flags
		VK_IMAGE_TYPE_2D,						// imageType
		format,									// format
		extent,									// extent
		1u,										// mipLevels
		1u,										// arraySize
		VK_SAMPLE_COUNT_1_BIT,					// samples
		VK_IMAGE_TILING_OPTIMAL,				// tiling
		usage,									// usage
		VK_SHARING_MODE_EXCLUSIVE,				// sharingMode
		1u,										// queueFamilyIndexCount
		&queueFamilyIndex,						// pQueueFamilyIndices
		VK_IMAGE_LAYOUT_UNDEFINED,				// initialLayout
	};

	handle = createImage(vk, device, &imageParams);
	memory = alloc.allocate(getImageMemoryRequirements(vk, device, *handle), MemoryRequirement::Any);

	vk.bindImageMemory(device, *handle, memory->getMemory(), memory->getOffset());
}

// Helper class representing buffer
class BufferWrapper
{
public:

			BufferWrapper	() = default;
	void	create			(Context& context, SimpleAllocator& alloc, VkDeviceSize size, VkBufferUsageFlags usage);

public:
	Move<VkBuffer>			handle;
	MovePtr<Allocation>		memory;
};

void BufferWrapper::create(Context& context, SimpleAllocator& alloc, VkDeviceSize size, VkBufferUsageFlags usage)
{
	const DeviceInterface&		vk					= context.getDeviceInterface();
	const VkDevice&				device				= context.getDevice();
	const VkBufferCreateInfo	bufferCreateInfo	= makeBufferCreateInfo(size, usage);

	handle = createBuffer(vk, device, &bufferCreateInfo);
	memory = alloc.allocate(getBufferMemoryRequirements(vk, device, *handle), MemoryRequirement::HostVisible);

	VK_CHECK(vk.bindBufferMemory(device, *handle, memory->getMemory(), memory->getOffset()));
}

class NoneStageTestInstance : public vkt::TestInstance
{
public:
									NoneStageTestInstance			(Context&			context,
																	 const TestParams&	testParams);
	virtual							~NoneStageTestInstance			(void) = default;

	tcu::TestStatus					iterate							(void) override;

protected:

	VkAccessFlags2KHR				getAccessFlag					(VkAccessFlags2KHR					access);
	VkBufferImageCopy				buildCopyRegion					(VkExtent3D							extent,
																	 VkImageAspectFlags					aspect);
	void							buildVertexBuffer				(void);
	Move<VkRenderPass>				buildBasicRenderPass			(VkFormat							outputFormat,
																	 VkImageLayout						outputLayout,
																	 VkAttachmentLoadOp					loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE);
	Move<VkRenderPass>				buildComplexRenderPass			(VkFormat							intermediateFormat,
																	 VkImageLayout						intermediateLayout,
																	 VkImageAspectFlags					intermediateAspect,
																	 VkFormat							outputFormat,
																	 VkImageLayout						outputLayout);
	Move<VkImageView>				buildImageView					(VkImage							image,
																	 VkFormat							format,
																	 const VkImageSubresourceRange&		subresourceRange);
	Move<VkFramebuffer>				buildFramebuffer				(VkRenderPass						renderPass,
																	 const VkImageView*					outView1,
																	 const VkImageView*					outView2 = DE_NULL);
	Move<VkSampler>					buildSampler					(void);
	Move<VkDescriptorSetLayout>		buildDescriptorSetLayout		(VkDescriptorType					descriptorType);
	Move<VkDescriptorPool>			buildDescriptorPool				(VkDescriptorType					descriptorType);
	Move<VkDescriptorSet>			buildDescriptorSet				(VkDescriptorPool					descriptorPool,
																	 VkDescriptorSetLayout				descriptorSetLayout,
																	 VkDescriptorType					descriptorType,
																	 VkImageView						inputView,
																	 VkImageLayout						inputLayout,
																	 const VkSampler*					sampler = DE_NULL);
	Move<VkPipeline>				buildPipeline					(deUint32							subpass,
																	 VkImageAspectFlags					resultAspect,
																	 VkPipelineLayout					pipelineLayout,
																	 VkShaderModule						vertShaderModule,
																	 VkShaderModule						fragShaderModule,
																	 VkRenderPass						renderPass);
	bool							verifyResult					(const PixelBufferAccess&			reference,
																	 const PixelBufferAccess&			result);

private:

	const TestParams				m_testParams;

	VkFormat						m_referenceImageFormat;
	VkFormat						m_transitionImageFormat;
	VkFormat						m_readImageFormat;
	VkImageSubresourceRange			m_referenceSubresourceRange;
	VkImageSubresourceRange			m_transitionSubresourceRange;
	VkImageSubresourceRange			m_readSubresourceRange;
	VkImageAspectFlags				m_transitionImageAspect;

	VkExtent3D						m_imageExtent;
	VkImageLayout					m_writeRenderPassOutputLayout;

	// flag indicating that graphics pipeline is constructed to write data to tested image
	bool							m_usePipelineToWrite;

	// flag indicating that graphics pipeline is constructed to read data from tested image
	bool							m_usePipelineToRead;

	// flag indicating that write pipeline should be constructed in a special way to fill stencil buffer
	bool							m_useStencilDuringWrite;

	// flag indicating that read pipeline should be constructed in a special way to use input attachment as a data source
	bool							m_useInputAttachmentToRead;

	VkPipelineStageFlags2KHR		m_srcStageToNoneStageMask;
	VkAccessFlags2KHR				m_srcAccessToNoneAccessMask;
	VkPipelineStageFlags2KHR		m_dstStageFromNoneStageMask;
	VkAccessFlags2KHR				m_dstAccessFromNoneAccessMask;

	SimpleAllocator					m_alloc;

	ImageWrapper					m_referenceImage;
	VkImageUsageFlags				m_referenceImageUsage;

	// objects/variables initialized only when needed
	ImageWrapper					m_imageToWrite;
	VkImageUsageFlags				m_imageToWriteUsage;

	ImageWrapper					m_imageToRead;

	BufferWrapper					m_vertexBuffer;
	std::vector<Move<VkImageView> >	m_attachmentViews;

	std::string						m_writeFragShaderName;
	Move<VkShaderModule>			m_writeVertShaderModule;
	Move<VkShaderModule>			m_writeFragShaderModule;
	Move<VkRenderPass>				m_writeRenderPass;
	Move<VkSampler>					m_writeSampler;
	Move<VkDescriptorSetLayout>		m_writeDescriptorSetLayout;
	Move<VkDescriptorPool>			m_writeDescriptorPool;
	Move<VkDescriptorSet>			m_writeDescriptorSet;
	Move<VkPipelineLayout>			m_writePipelineLayout;
	Move<VkPipeline>				m_writePipeline;
	Move<VkFramebuffer>				m_writeFramebuffer;

	std::string						m_readFragShaderName;
	Move<VkShaderModule>			m_readVertShaderModule;
	Move<VkShaderModule>			m_readFragShaderModule;
	Move<VkShaderModule>			m_readFragShaderModule2;
	Move<VkRenderPass>				m_readRenderPass;
	Move<VkSampler>					m_readSampler;
	Move<VkDescriptorSetLayout>		m_readDescriptorSetLayout;
	Move<VkDescriptorPool>			m_readDescriptorPool;
	Move<VkDescriptorSet>			m_readDescriptorSet;
	Move<VkPipelineLayout>			m_readPipelineLayout;
	Move<VkPipeline>				m_readPipeline;
	Move<VkFramebuffer>				m_readFramebuffer;
};

NoneStageTestInstance::NoneStageTestInstance(Context& context, const TestParams& testParams)
	: vkt::TestInstance(context)
	, m_testParams		(testParams)
	, m_imageExtent		{ 32, 32, 1 }
	, m_alloc			(m_context.getDeviceInterface(),
		m_context.getDevice(),
		getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()))
{
	// note: for clarity whole configuration of whats going on in iterate method was moved here

	const auto writeLayout	= m_testParams.writeLayout;
	const auto writeAspect	= m_testParams.writeAspect;
	const auto readLayout	= m_testParams.readLayout;
	const auto readAspect	= m_testParams.readAspect;

	// When testing depth stencil combined images, the stencil aspect is only tested when depth aspect is in ATTACHMENT_OPTIMAL layout.
	// - it is invalid to read depth using sampler or input attachment in such layout
	const auto readStencilFromCombinedDepthStencil = (readLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL);

	// select format that will be used for test
	if ((writeAspect == VK_IMAGE_ASPECT_DEPTH_BIT) || (readAspect == VK_IMAGE_ASPECT_DEPTH_BIT))
	{
		m_transitionImageFormat			= VK_FORMAT_D32_SFLOAT;
		m_transitionImageAspect			= VK_IMAGE_ASPECT_DEPTH_BIT;
		m_writeRenderPassOutputLayout	= VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	}
	else if ((writeAspect == VK_IMAGE_ASPECT_STENCIL_BIT) || (readAspect == VK_IMAGE_ASPECT_STENCIL_BIT))
	{
		m_transitionImageFormat			= VK_FORMAT_S8_UINT;
		m_transitionImageAspect			= VK_IMAGE_ASPECT_STENCIL_BIT;
		m_writeRenderPassOutputLayout	= VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
	}
	else if ((writeAspect == IMAGE_ASPECT_DEPTH_STENCIL) || (readAspect == IMAGE_ASPECT_DEPTH_STENCIL))
	{
		m_transitionImageFormat			= VK_FORMAT_D24_UNORM_S8_UINT;
		m_writeRenderPassOutputLayout	= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		if (readStencilFromCombinedDepthStencil)
		{
			m_transitionImageAspect = VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		else
		{
			// note: in test we focus only on depth aspect; no need to check both in those cases
			m_transitionImageAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		}
	}
	else
	{
		m_transitionImageFormat			= VK_FORMAT_R8G8B8A8_UNORM;
		m_transitionImageAspect			= VK_IMAGE_ASPECT_COLOR_BIT;
		m_writeRenderPassOutputLayout	= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	m_referenceSubresourceRange		= { m_transitionImageAspect, 0u, 1u, 0u, 1u };
	m_transitionSubresourceRange	= { m_transitionImageAspect, 0u, 1u, 0u, 1u };
	m_readSubresourceRange			= { m_transitionImageAspect, 0u, 1u, 0u, 1u };
	m_referenceImageFormat			= m_transitionImageFormat;
	m_readImageFormat				= m_transitionImageFormat;
	m_referenceImageUsage			= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	m_imageToWriteUsage				= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	// pipeline is not created for transfer and general layouts (general layouts in tests follow same path as transfer layouts)
	m_usePipelineToWrite	= (writeLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) && (writeLayout != VK_IMAGE_LAYOUT_GENERAL);
	m_usePipelineToRead		= (readLayout  != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) && (readLayout  != VK_IMAGE_LAYOUT_GENERAL);
	m_useStencilDuringWrite = false;

	m_srcStageToNoneStageMask		= VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
	m_srcAccessToNoneAccessMask		= getAccessFlag(VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR);
	m_dstStageFromNoneStageMask		= VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
	m_dstAccessFromNoneAccessMask	= getAccessFlag(VK_ACCESS_2_TRANSFER_READ_BIT_KHR);

	// when graphics pipelines are not created only image with gradient is used for test
	if (!m_usePipelineToWrite && !m_usePipelineToRead)
	{
		m_referenceImageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		return;
	}

	if (m_usePipelineToWrite)
	{
		// depth/stencil layouts need diferent configuration
		if (writeAspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
		{
			if ((writeAspect & VK_IMAGE_ASPECT_DEPTH_BIT) && !readStencilFromCombinedDepthStencil)
			{
				m_referenceImageFormat					 = VK_FORMAT_R32_SFLOAT;
				m_referenceImageUsage					|= VK_IMAGE_USAGE_SAMPLED_BIT;
				m_referenceSubresourceRange.aspectMask	 = VK_IMAGE_ASPECT_COLOR_BIT;
				m_writeFragShaderName					 = "frag-color-to-depth";
			}
			else
			{
				m_referenceImageUsage					 = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
				m_useStencilDuringWrite					 = true;
				m_writeFragShaderName					 = "frag-color-to-stencil";
			}

			m_srcStageToNoneStageMask		 = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR;
			m_srcAccessToNoneAccessMask		 = getAccessFlag(VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR);
			m_imageToWriteUsage				|= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
		else
		{
			m_referenceImageUsage			|= VK_IMAGE_USAGE_SAMPLED_BIT;
			m_srcStageToNoneStageMask		 = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
			m_srcAccessToNoneAccessMask		 = getAccessFlag(VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR);
			m_writeFragShaderName			 = "frag-color";
			m_imageToWriteUsage				|= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}
	}

	if (m_usePipelineToRead)
	{
		m_dstStageFromNoneStageMask		= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;
		m_dstAccessFromNoneAccessMask	= getAccessFlag(VK_ACCESS_2_SHADER_READ_BIT_KHR);

		m_readSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		if (((readAspect | writeAspect) & VK_IMAGE_ASPECT_DEPTH_BIT) && !readStencilFromCombinedDepthStencil)
			m_readImageFormat = VK_FORMAT_R32_SFLOAT;
		else if ((readAspect | writeAspect) & VK_IMAGE_ASPECT_STENCIL_BIT)
			m_readImageFormat = VK_FORMAT_R8_UINT;

		// for layouts that operate on depth or stencil (not depth_stencil) use input attachment to read
		if ((readAspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) && (readAspect != IMAGE_ASPECT_DEPTH_STENCIL))
		{
			m_useInputAttachmentToRead		 = true;
			m_readFragShaderName			 = "frag-depth-or-stencil-to-color";
			m_imageToWriteUsage				|= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
			m_dstAccessFromNoneAccessMask	 = getAccessFlag(VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT_KHR);

			if (!m_usePipelineToWrite)
				m_referenceImageUsage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
		}
		else // use image sampler for color and depth_stencil layouts
		{
			m_useInputAttachmentToRead		 = false;
			m_readFragShaderName			 = "frag-color";
			m_referenceImageUsage			|= VK_IMAGE_USAGE_SAMPLED_BIT;
			m_imageToWriteUsage				|= VK_IMAGE_USAGE_SAMPLED_BIT;

			// for depth_stencil layouts we need to have depth_stencil_attachment usage
			if (!m_usePipelineToWrite && (readAspect & VK_IMAGE_ASPECT_STENCIL_BIT))
				m_referenceImageUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

			// when we read stencil as color we need to use usampler2D
			if ((readAspect | writeAspect) == VK_IMAGE_ASPECT_STENCIL_BIT || (readAspect == IMAGE_ASPECT_DEPTH_STENCIL && readStencilFromCombinedDepthStencil))
				m_readFragShaderName		 = "frag-stencil-to-color";
		}
	}
}

VkAccessFlags2KHR NoneStageTestInstance::getAccessFlag(VkAccessFlags2KHR access)
{
	if (m_testParams.useGenericAccessFlags)
	{
		switch (access)
		{
		case VK_ACCESS_2_HOST_READ_BIT_KHR:
		case VK_ACCESS_2_TRANSFER_READ_BIT_KHR:
		case VK_ACCESS_2_SHADER_READ_BIT_KHR:
		case VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT_KHR:
			return VK_ACCESS_2_MEMORY_READ_BIT_KHR;

		case VK_ACCESS_2_HOST_WRITE_BIT_KHR:
		case VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR:
		case VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR:
		case VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR:
				return VK_ACCESS_2_MEMORY_WRITE_BIT_KHR;

		default:
			TCU_THROW(TestError, "Unhandled access flag");
		}
	}
	return access;
}

VkBufferImageCopy NoneStageTestInstance::buildCopyRegion(VkExtent3D extent, VkImageAspectFlags aspect)
{
	return
	{
		0u,								// VkDeviceSize					bufferOffset
		extent.width,					// deUint32						bufferRowLength
		extent.height,					// deUint32						bufferImageHeight
		{ aspect, 0u, 0u, 1u },			// VkImageSubresourceLayers		imageSubresource
		{ 0, 0, 0 },					// VkOffset3D					imageOffset
		extent							// VkExtent3D					imageExtent
	};
}

void NoneStageTestInstance::buildVertexBuffer()
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice&			device	= m_context.getDevice();

	std::vector<float> vertices
	{
		 1.0f,  1.0f, 0.0f, 1.0f,
		-1.0f,  1.0f, 0.0f, 1.0f,
		 1.0f, -1.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 1.0f,
	};
	m_vertexBuffer.create(m_context, m_alloc, sizeof(float) * vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	deMemcpy(m_vertexBuffer.memory->getHostPtr(), vertices.data(), vertices.size() * sizeof(float));
	flushAlloc(vk, device, *m_vertexBuffer.memory);
}

Move<VkRenderPass> NoneStageTestInstance::buildBasicRenderPass(VkFormat outputFormat, VkImageLayout outputLayout, VkAttachmentLoadOp loadOp)
{
	// output color/depth attachment
	VkAttachmentDescription2 attachmentDescription
	{
		VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,				// VkStructureType					sType
		DE_NULL,												// const void*						pNext
		(VkAttachmentDescriptionFlags)0,						// VkAttachmentDescriptionFlags		flags
		outputFormat,											// VkFormat							format
		VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits			samples
		loadOp,													// VkAttachmentLoadOp				loadOp
		VK_ATTACHMENT_STORE_OP_STORE,							// VkAttachmentStoreOp				storeOp
		loadOp,													// VkAttachmentLoadOp				stencilLoadOp
		VK_ATTACHMENT_STORE_OP_STORE,							// VkAttachmentStoreOp				stencilStoreOp
		VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout					initialLayout
		outputLayout											// VkImageLayout					finalLayout
	};

	VkImageAspectFlags		imageAspect		= getImageAspectFlags(mapVkFormat(outputFormat));
	VkAttachmentReference2	attachmentRef
	{
		VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, DE_NULL, 0u, outputLayout, imageAspect
	};

	VkAttachmentReference2* pColorAttachment			= DE_NULL;
	VkAttachmentReference2* pDepthStencilAttachment		= DE_NULL;
	if (imageAspect == VK_IMAGE_ASPECT_COLOR_BIT)
		pColorAttachment = &attachmentRef;
	else
		pDepthStencilAttachment = &attachmentRef;

	VkSubpassDescription2 subpassDescription
	{
		VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
		DE_NULL,
		(VkSubpassDescriptionFlags)0,							// VkSubpassDescriptionFlags		flags
		VK_PIPELINE_BIND_POINT_GRAPHICS,						// VkPipelineBindPoint				pipelineBindPoint
		0u,														// deUint32							viewMask
		0u,														// deUint32							inputAttachmentCount
		DE_NULL,												// const VkAttachmentReference2*	pInputAttachments
		!!pColorAttachment,										// deUint32							colorAttachmentCount
		pColorAttachment,										// const VkAttachmentReference2*	pColorAttachments
		DE_NULL,												// const VkAttachmentReference2*	pResolveAttachments
		pDepthStencilAttachment,								// const VkAttachmentReference2*	pDepthStencilAttachment
		0u,														// deUint32							preserveAttachmentCount
		DE_NULL													// const deUint32*					pPreserveAttachments
	};

	const VkRenderPassCreateInfo2 renderPassInfo
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
		DE_NULL,												// const void*						pNext
		(VkRenderPassCreateFlags)0,								// VkRenderPassCreateFlags			flags
		1u,														// deUint32							attachmentCount
		&attachmentDescription,									// const VkAttachmentDescription*	pAttachments
		1u,														// deUint32							subpassCount
		&subpassDescription,									// const VkSubpassDescription*		pSubpasses
		0u,														// deUint32							dependencyCount
		DE_NULL,												// const VkSubpassDependency*		pDependencies
		0u,														// deUint32							correlatedViewMaskCount
		DE_NULL													// const deUint32*					pCorrelatedViewMasks
	};

	return vk::createRenderPass2(m_context.getDeviceInterface(), m_context.getDevice(), &renderPassInfo);
}

Move<VkRenderPass> NoneStageTestInstance::buildComplexRenderPass(VkFormat intermediateFormat,	VkImageLayout intermediateLayout, VkImageAspectFlags intermediateAspect,
																 VkFormat outputFormat,			VkImageLayout outputLayout)
{
	std::vector<VkAttachmentDescription2> attachmentDescriptions
	{
		// depth/stencil attachment (when used in read pipeline it loads data filed in write pipeline)
		{
			VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,				// VkStructureType					sType
			DE_NULL,												// const void*						pNext
			(VkAttachmentDescriptionFlags)0,						// VkAttachmentDescriptionFlags		flags
			intermediateFormat,										// VkFormat							format
			VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits			samples
			VK_ATTACHMENT_LOAD_OP_LOAD,								// VkAttachmentLoadOp				loadOp
			VK_ATTACHMENT_STORE_OP_STORE,							// VkAttachmentStoreOp				storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,						// VkAttachmentLoadOp				stencilLoadOp
			VK_ATTACHMENT_STORE_OP_STORE,							// VkAttachmentStoreOp				stencilStoreOp
			intermediateLayout,										// VkImageLayout					initialLayout
			intermediateLayout										// VkImageLayout					finalLayout
		},
		// color attachment
		{
			VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,				// VkStructureType					sType
			DE_NULL,												// const void*						pNext
			(VkAttachmentDescriptionFlags)0,						// VkAttachmentDescriptionFlags		flags
			outputFormat,											// VkFormat							format
			VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits			samples
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,						// VkAttachmentLoadOp				loadOp
			VK_ATTACHMENT_STORE_OP_STORE,							// VkAttachmentStoreOp				storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,						// VkAttachmentLoadOp				stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,						// VkAttachmentStoreOp				stencilStoreOp
			VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout					initialLayout
			outputLayout											// VkImageLayout					finalLayout
		}
	};

	VkImageAspectFlags					outputAspect		= getImageAspectFlags(mapVkFormat(outputFormat));
	std::vector<VkAttachmentReference2>	attachmentRefs
	{
		{ VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, DE_NULL, 0u, intermediateLayout, intermediateAspect },
		{ VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, DE_NULL, 1u, outputLayout, outputAspect }
	};

	VkAttachmentReference2* pDepthStencilAttachment = &attachmentRefs[0];
	VkAttachmentReference2* pColorAttachment		= &attachmentRefs[1];

	std::vector<VkSubpassDescription2> subpassDescriptions
	{
		{
			VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
			DE_NULL,
			(VkSubpassDescriptionFlags)0,						// VkSubpassDescriptionFlags		flags
			VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint				pipelineBindPoint
			0u,													// deUint32							viewMask
			1u,													// deUint32							inputAttachmentCount
			pDepthStencilAttachment,							// const VkAttachmentReference2*	pInputAttachments
			1u,													// deUint32							colorAttachmentCount
			pColorAttachment,									// const VkAttachmentReference2*	pColorAttachments
			DE_NULL,											// const VkAttachmentReference2*	pResolveAttachments
			DE_NULL,											// const VkAttachmentReference2*	pDepthStencilAttachment
			0u,													// deUint32							preserveAttachmentCount
			DE_NULL												// deUint32*						pPreserveAttachments
		}
	};

	const VkRenderPassCreateInfo2 renderPassInfo
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
		DE_NULL,												// const void*						pNext
		(VkRenderPassCreateFlags)0,								// VkRenderPassCreateFlags			flags
		(deUint32)attachmentDescriptions.size(),				// deUint32							attachmentCount
		attachmentDescriptions.data(),							// const VkAttachmentDescription*	pAttachments
		(deUint32)subpassDescriptions.size(),					// deUint32							subpassCount
		subpassDescriptions.data(),								// const VkSubpassDescription*		pSubpasses
		0u,														// deUint32							dependencyCount
		DE_NULL,												// const VkSubpassDependency*		pDependencies
		0u,														// deUint32							correlatedViewMaskCount
		DE_NULL													// const deUint32*					pCorrelatedViewMasks
	};

	return vk::createRenderPass2(m_context.getDeviceInterface(), m_context.getDevice(), &renderPassInfo);
}

Move<VkImageView> NoneStageTestInstance::buildImageView(VkImage image, VkFormat format, const VkImageSubresourceRange& subresourceRange)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice&			device	= m_context.getDevice();

	const VkImageViewCreateInfo imageViewParams
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,				// VkStructureType				sType
		DE_NULL,												// const void*					pNext
		0u,														// VkImageViewCreateFlags		flags
		image,													// VkImage						image
		VK_IMAGE_VIEW_TYPE_2D,									// VkImageViewType				viewType
		format,													// VkFormat						format
		makeComponentMappingRGBA(),								// VkComponentMapping			components
		subresourceRange,										// VkImageSubresourceRange		subresourceRange
	};

	return createImageView(vk, device, &imageViewParams);
}

Move<VkFramebuffer> NoneStageTestInstance::buildFramebuffer(VkRenderPass renderPass, const VkImageView* outView1, const VkImageView* outView2)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice&			device	= m_context.getDevice();

	std::vector<VkImageView> imageViews = { *outView1 };
	if (outView2)
		imageViews.push_back(*outView2);

	const VkFramebufferCreateInfo framebufferParams
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,				// VkStructureType				sType
		DE_NULL,												// const void*					pNext
		0u,														// VkFramebufferCreateFlags		flags
		renderPass,												// VkRenderPass					renderPass
		(deUint32)imageViews.size(),							// deUint32						attachmentCount
		imageViews.data(),										// const VkImageView*			pAttachments
		m_imageExtent.width,									// deUint32						width
		m_imageExtent.height,									// deUint32						height
		1u,														// deUint32						layers
	};
	return createFramebuffer(vk, device, &framebufferParams);
}

Move<VkSampler> NoneStageTestInstance::buildSampler()
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice&			device	= m_context.getDevice();

	const VkSamplerCreateInfo samplerInfo
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,					// VkStructureType				sType
		DE_NULL,												// const void*					pNext
		0u,														// VkSamplerCreateFlags			flags
		VK_FILTER_NEAREST,										// VkFilter						magFilter
		VK_FILTER_NEAREST,										// VkFilter						minFilter
		VK_SAMPLER_MIPMAP_MODE_NEAREST,							// VkSamplerMipmapMode			mipmapMode
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,					// VkSamplerAddressMode			addressModeU
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,					// VkSamplerAddressMode			addressModeV
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,					// VkSamplerAddressMode			addressModeW
		0.0f,													// float						mipLodBias
		VK_FALSE,												// VkBool32						anisotropyEnable
		1.0f,													// float						maxAnisotropy
		DE_FALSE,												// VkBool32						compareEnable
		VK_COMPARE_OP_ALWAYS,									// VkCompareOp					compareOp
		0.0f,													// float						minLod
		0.0f,													// float						maxLod
		VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,				// VkBorderColor				borderColor
		VK_FALSE,												// VkBool32						unnormalizedCoords
	};
	return createSampler(vk, device, &samplerInfo);
}

Move<VkDescriptorSetLayout> NoneStageTestInstance::buildDescriptorSetLayout(VkDescriptorType descriptorType)
{
	const DeviceInterface&		vk		= m_context.getDeviceInterface();
	const VkDevice&				device	= m_context.getDevice();

	return
		DescriptorSetLayoutBuilder()
		.addSingleSamplerBinding(descriptorType, VK_SHADER_STAGE_FRAGMENT_BIT, DE_NULL)
		.build(vk, device);
}

Move<VkDescriptorPool> NoneStageTestInstance::buildDescriptorPool(VkDescriptorType descriptorType)
{
	const DeviceInterface&		vk		= m_context.getDeviceInterface();
	const VkDevice&				device	= m_context.getDevice();

	return
		DescriptorPoolBuilder()
		.addType(descriptorType, 1u)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
}

Move<VkDescriptorSet> NoneStageTestInstance::buildDescriptorSet(VkDescriptorPool		descriptorPool,
																VkDescriptorSetLayout	descriptorSetLayout,
																VkDescriptorType		descriptorType,
																VkImageView				inputView,
																VkImageLayout			inputLayout,
																const VkSampler*		sampler)
{
	const DeviceInterface&		vk				= m_context.getDeviceInterface();
	const VkDevice&				device			= m_context.getDevice();

	const VkDescriptorImageInfo inputImageInfo	= makeDescriptorImageInfo(sampler ? *sampler : 0u, inputView, inputLayout);
	Move<VkDescriptorSet>		descriptorSet	= makeDescriptorSet(vk, device, descriptorPool, descriptorSetLayout);

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType, &inputImageInfo)
		.update(vk, device);

	return descriptorSet;
}

Move<VkPipeline> NoneStageTestInstance::buildPipeline(deUint32				subpass,
													  VkImageAspectFlags	resultAspect,
													  VkPipelineLayout		pipelineLayout,
													  VkShaderModule		vertShaderModule,
													  VkShaderModule		fragShaderModule,
													  VkRenderPass			renderPass)
{
	const DeviceInterface&			vk				= m_context.getDeviceInterface();
	const VkDevice&					device			= m_context.getDevice();
	const std::vector<VkViewport>	viewports		{ makeViewport(m_imageExtent) };
	const std::vector<VkRect2D>		scissors		{ makeRect2D(m_imageExtent) };
	const bool						useDepth		= resultAspect & VK_IMAGE_ASPECT_DEPTH_BIT;
	const bool						useStencil		= resultAspect & VK_IMAGE_ASPECT_STENCIL_BIT;

	const VkStencilOpState stencilOpState = makeStencilOpState(
		VK_STENCIL_OP_REPLACE,											// stencil fail
		VK_STENCIL_OP_REPLACE,											// depth & stencil pass
		VK_STENCIL_OP_REPLACE,											// depth only fail
		VK_COMPARE_OP_ALWAYS,											// compare op
		1u,																// compare mask
		1u,																// write mask
		1u);															// reference

	const VkPipelineDepthStencilStateCreateInfo	depthStencilStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,		// VkStructureType								sType
		DE_NULL,														// const void*									pNext
		(VkPipelineDepthStencilStateCreateFlags)0,						// VkPipelineDepthStencilStateCreateFlags		flags
		useDepth,														// VkBool32										depthTestEnable
		useDepth,														// VkBool32										depthWriteEnable
		VK_COMPARE_OP_ALWAYS,											// VkCompareOp									depthCompareOp
		VK_FALSE,														// VkBool32										depthBoundsTestEnable
		useStencil,														// VkBool32										stencilTestEnable
		stencilOpState,													// VkStencilOpState								front
		stencilOpState,													// VkStencilOpState								back
		0.0f,															// float										minDepthBounds
		1.0f,															// float										maxDepthBounds
	};

	return makeGraphicsPipeline(
		vk,																// DeviceInterface&								vk
		device,															// VkDevice										device
		pipelineLayout,													// VkPipelineLayout								pipelineLayout
		vertShaderModule,												// VkShaderModule								vertexShaderModule
		DE_NULL,														// VkShaderModule								tessellationControlModule
		DE_NULL,														// VkShaderModule								tessellationEvalModule
		DE_NULL,														// VkShaderModule								geometryShaderModule
		fragShaderModule,												// VkShaderModule								fragmentShaderModule
		renderPass,														// VkRenderPass									renderPass
		viewports,														// std::vector<VkViewport>&						viewports
		scissors,														// std::vector<VkRect2D>&						scissors
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,							// VkPrimitiveTopology							topology
		subpass,														// deUint32										subpass
		0u,																// deUint32										patchControlPoints
		DE_NULL,														// VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
		DE_NULL,														// VkPipelineRasterizationStateCreateInfo*		rasterizationStateCreateInfo
		DE_NULL,														// VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
		&depthStencilStateCreateInfo);									// VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo
}

tcu::TestStatus NoneStageTestInstance::iterate(void)
{
	const DeviceInterface&		vk						= m_context.getDeviceInterface();
	const VkDevice&				device					= m_context.getDevice();
	const deUint32				queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	VkQueue						queue					= m_context.getUniversalQueue();
	Move<VkCommandPool>			cmdPool					= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	Move<VkCommandBuffer>		cmdBuffer				= makeCommandBuffer(vk, device, *cmdPool);
	const VkDeviceSize			vertexBufferOffset		= 0;
	ImageWrapper*				transitionImagePtr		= &m_referenceImage;
	ImageWrapper*				imageToVerifyPtr		= &m_referenceImage;
	const deUint32				imageSizeInBytes		= m_imageExtent.width * m_imageExtent.height * 4;
	SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(m_testParams.type, vk, false);
	const VkRect2D				renderArea				= makeRect2D(0, 0, m_imageExtent.width, m_imageExtent.height);
	const VkBufferImageCopy		transitionCopyRegion	= buildCopyRegion(m_imageExtent, m_transitionImageAspect);
	const VkBufferImageCopy		colorCopyRegion			= buildCopyRegion(m_imageExtent, VK_IMAGE_ASPECT_COLOR_BIT);

	// create image that will have gradient (without data atm)
	m_referenceImage.create(m_context, m_alloc, m_referenceImageFormat, m_imageExtent, m_referenceImageUsage);

	// create buffer used for gradient data source
	BufferWrapper srcBuffer;
	srcBuffer.create(m_context, m_alloc, imageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	// generate gradient
	std::vector<deUint32>	referenceData	(m_imageExtent.width * m_imageExtent.height);
	tcu::TextureFormat		referenceFormat	(mapVkFormat(m_referenceImageFormat));
	if (m_testParams.readLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL)
	{
		// when testing stencil aspect of depth stencil combined image, prepare reference date only with stencil,
		// as the copy operation (used when m_usePipelineToWrite == false) sources just one aspect

		// this format is used for tcu operations only - does not need to be supported by the Vulkan implementation
		referenceFormat = mapVkFormat(VK_FORMAT_S8_UINT);
	}
	PixelBufferAccess		referencePBA	(referenceFormat, m_imageExtent.width, m_imageExtent.height, m_imageExtent.depth, referenceData.data());
	fillWithComponentGradients(referencePBA, tcu::Vec4(0.0f), tcu::Vec4(1.0f));
	deMemcpy(srcBuffer.memory->getHostPtr(), referenceData.data(), static_cast<size_t>(imageSizeInBytes));
	flushAlloc(vk, device, *srcBuffer.memory);

	// create buffer for result transfer
	BufferWrapper			dstBuffer;
	tcu::TextureFormat		resultFormat	(mapVkFormat(m_readImageFormat));
	dstBuffer.create(m_context, m_alloc, imageSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

	if (m_usePipelineToWrite || m_usePipelineToRead)
	{
		buildVertexBuffer();

		// create image view for reference image (its always at index 0)
		m_attachmentViews.push_back(buildImageView(*m_referenceImage.handle, m_referenceImageFormat, m_referenceSubresourceRange));

		// create graphics pipeline used to write image data
		if (m_usePipelineToWrite)
		{
			// create image that will be used as attachment to write to
			m_imageToWrite.create(m_context, m_alloc, m_transitionImageFormat, m_imageExtent, m_imageToWriteUsage);
			m_attachmentViews.push_back(buildImageView(*m_imageToWrite.handle, m_transitionImageFormat, m_transitionSubresourceRange));

			m_writeVertShaderModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
			m_writeFragShaderModule = createShaderModule(vk, device, m_context.getBinaryCollection().get(m_writeFragShaderName), 0);

			if (m_useStencilDuringWrite)
			{
				// this is used only for cases where writable layout is VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL
				// in this case generated gradient is only used for verification
				m_writeRenderPass			= buildBasicRenderPass(m_transitionImageFormat, m_writeRenderPassOutputLayout, VK_ATTACHMENT_LOAD_OP_CLEAR);
				m_writePipelineLayout		= makePipelineLayout(vk, device, DE_NULL);
				m_writePipeline				= buildPipeline(0u, m_transitionImageAspect, *m_writePipelineLayout,
															*m_writeVertShaderModule, *m_writeFragShaderModule, *m_writeRenderPass);
				m_writeFramebuffer			= buildFramebuffer(*m_writeRenderPass, &m_attachmentViews[1].get());
			}
			else
			{
				m_writeRenderPass			= buildBasicRenderPass(m_transitionImageFormat, m_writeRenderPassOutputLayout);
				m_writeSampler				= buildSampler();
				m_writeDescriptorSetLayout	= buildDescriptorSetLayout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
				m_writeDescriptorPool		= buildDescriptorPool(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
				m_writeDescriptorSet		= buildDescriptorSet(*m_writeDescriptorPool, *m_writeDescriptorSetLayout, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
																 *m_attachmentViews[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &m_writeSampler.get());
				m_writePipelineLayout		= makePipelineLayout(vk, device, *m_writeDescriptorSetLayout);
				m_writePipeline				= buildPipeline(0u, m_transitionImageAspect, *m_writePipelineLayout,
															*m_writeVertShaderModule, *m_writeFragShaderModule, *m_writeRenderPass);
				m_writeFramebuffer			= buildFramebuffer(*m_writeRenderPass, &m_attachmentViews[1].get());
			}

			transitionImagePtr	= &m_imageToWrite;
			imageToVerifyPtr	= &m_imageToWrite;
		}

		// create graphics pipeline used to read image data
		if (m_usePipelineToRead)
		{
			m_imageToRead.create(m_context, m_alloc, m_readImageFormat, m_imageExtent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
			m_attachmentViews.push_back(buildImageView(*m_imageToRead.handle, m_readImageFormat, m_readSubresourceRange));
			imageToVerifyPtr = &m_imageToRead;

			m_readVertShaderModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
			m_readFragShaderModule = createShaderModule(vk, device, m_context.getBinaryCollection().get(m_readFragShaderName), 0);

			if (m_useInputAttachmentToRead)
			{
				m_readDescriptorSetLayout	= buildDescriptorSetLayout(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
				m_readDescriptorPool		= buildDescriptorPool(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
				m_readDescriptorSet			= buildDescriptorSet(*m_readDescriptorPool, *m_readDescriptorSetLayout, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
																 *m_attachmentViews[m_usePipelineToWrite], m_testParams.readLayout);
				m_readRenderPass			= buildComplexRenderPass(m_transitionImageFormat, m_testParams.readLayout, m_transitionImageAspect,
																	 m_readImageFormat, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
				m_readFramebuffer			= buildFramebuffer(*m_readRenderPass, &m_attachmentViews[m_usePipelineToWrite].get(),
																				  &m_attachmentViews[m_usePipelineToWrite+1].get());
				m_readPipelineLayout		= makePipelineLayout(vk, device, *m_readDescriptorSetLayout);
				m_readPipeline				= buildPipeline(0u, VK_IMAGE_ASPECT_COLOR_BIT, *m_readPipelineLayout,
															*m_readVertShaderModule, *m_readFragShaderModule, *m_readRenderPass);
			}
			else
			{
				m_readSampler				= buildSampler();
				m_readDescriptorSetLayout	= buildDescriptorSetLayout(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
				m_readDescriptorPool		= buildDescriptorPool(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
				m_readDescriptorSet			= buildDescriptorSet(*m_readDescriptorPool, *m_readDescriptorSetLayout, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
																 *m_attachmentViews[m_usePipelineToWrite], m_testParams.readLayout, &m_readSampler.get());
				m_readRenderPass			= buildBasicRenderPass(m_readImageFormat, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
				m_readFramebuffer			= buildFramebuffer(*m_readRenderPass, &m_attachmentViews[m_usePipelineToWrite + 1].get());
				m_readPipelineLayout		= makePipelineLayout(vk, device, *m_readDescriptorSetLayout);
				m_readPipeline				= buildPipeline(0u, m_transitionImageAspect, *m_readPipelineLayout,
															*m_readVertShaderModule, *m_readFragShaderModule, *m_readRenderPass);
			}
		}
	}

	beginCommandBuffer(vk, *cmdBuffer);

	// write data from buffer with gradient to image (for stencil_attachment cases we dont need to do that)
	if (!m_useStencilDuringWrite)
	{
		// wait for reference data to be in buffer
		const VkBufferMemoryBarrier2KHR preBufferMemoryBarrier2 = makeBufferMemoryBarrier2(
			VK_PIPELINE_STAGE_2_HOST_BIT_KHR,					// VkPipelineStageFlags2KHR			srcStageMask
			getAccessFlag(VK_ACCESS_2_HOST_WRITE_BIT_KHR),		// VkAccessFlags2KHR				srcAccessMask
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,				// VkPipelineStageFlags2KHR			dstStageMask
			getAccessFlag(VK_ACCESS_2_TRANSFER_READ_BIT_KHR),	// VkAccessFlags2KHR				dstAccessMask
			*srcBuffer.handle,									// VkBuffer							buffer
			0u,													// VkDeviceSize						offset
			imageSizeInBytes									// VkDeviceSize						size
		);

		VkImageLayout copyBufferToImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		if(m_testParams.writeLayout == VK_IMAGE_LAYOUT_GENERAL)
			copyBufferToImageLayout = VK_IMAGE_LAYOUT_GENERAL;

		// change image layout so that we can copy to it data from buffer
		const VkImageMemoryBarrier2KHR preImageMemoryBarrier2 = makeImageMemoryBarrier2(
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,				// VkPipelineStageFlags2KHR			srcStageMask
			getAccessFlag(VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR),	// VkAccessFlags2KHR				srcAccessMask
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,				// VkPipelineStageFlags2KHR			dstStageMask
			getAccessFlag(VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR),	// VkAccessFlags2KHR				dstAccessMask
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout					oldLayout
			copyBufferToImageLayout,							// VkImageLayout					newLayout
			*m_referenceImage.handle,							// VkImage							image
			m_referenceSubresourceRange							// VkImageSubresourceRange			subresourceRange
		);
		VkDependencyInfoKHR buffDependencyInfo = makeCommonDependencyInfo(DE_NULL, &preBufferMemoryBarrier2, &preImageMemoryBarrier2);
		synchronizationWrapper->cmdPipelineBarrier(*cmdBuffer, &buffDependencyInfo);

		const VkBufferImageCopy* copyRegion = m_usePipelineToWrite ? &colorCopyRegion : &transitionCopyRegion;
		vk.cmdCopyBufferToImage(*cmdBuffer, *srcBuffer.handle, *m_referenceImage.handle, copyBufferToImageLayout, 1u, copyRegion);
	}

	if (m_usePipelineToWrite)
	{
		// wait till data is transfered to image (in all cases except when stencil_attachment is tested)
		if (!m_useStencilDuringWrite)
		{
			const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
				VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,				// VkPipelineStageFlags2KHR			srcStageMask
				getAccessFlag(VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR),	// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,		// VkPipelineStageFlags2KHR			dstStageMask
				getAccessFlag(VK_ACCESS_2_SHADER_READ_BIT_KHR),		// VkAccessFlags2KHR				dstAccessMask
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,				// VkImageLayout					oldLayout
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,			// VkImageLayout					newLayout
				*m_referenceImage.handle,							// VkImage							image
				m_referenceSubresourceRange							// VkImageSubresourceRange			subresourceRange
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(*cmdBuffer, &dependencyInfo);
		}

		beginRenderPass(vk, *cmdBuffer, *m_writeRenderPass, *m_writeFramebuffer, renderArea, tcu::Vec4(0.0f ,0.0f, 0.0f, 1.0f));

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_writePipeline);
		vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &m_vertexBuffer.handle.get(), &vertexBufferOffset);
		if (m_useStencilDuringWrite)
		{
			// when writing to stencil buffer draw single triangle (to simulate gradient over 1bit)
			vk.cmdDraw(*cmdBuffer, 3u, 1u, 0u, 0u);
		}
		else
		{
			vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_writePipelineLayout, 0, 1, &m_writeDescriptorSet.get(), 0, DE_NULL);
			vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);
		}

		endRenderPass(vk, *cmdBuffer);
	}

	// use none stage to wait till data is transfered to image
	{
		const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
			m_srcStageToNoneStageMask,							// VkPipelineStageFlags2KHR			srcStageMask
			m_srcAccessToNoneAccessMask,						// VkAccessFlags2KHR				srcAccessMask
			VK_PIPELINE_STAGE_2_NONE_KHR,						// VkPipelineStageFlags2KHR			dstStageMask
			VK_ACCESS_2_NONE_KHR,								// VkAccessFlags2KHR				dstAccessMask
			m_testParams.writeLayout,							// VkImageLayout					oldLayout
			m_testParams.writeLayout,							// VkImageLayout					newLayout
			*transitionImagePtr->handle,						// VkImage							image
			m_transitionSubresourceRange						// VkImageSubresourceRange			subresourceRange
		);
		VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
		synchronizationWrapper->cmdPipelineBarrier(*cmdBuffer, &dependencyInfo);
	}

	// use all commands stage to change image layout
	{
		const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,			// VkPipelineStageFlags2KHR			srcStageMask
			VK_ACCESS_2_NONE_KHR,								// VkAccessFlags2KHR				srcAccessMask
			m_dstStageFromNoneStageMask,						// VkPipelineStageFlags2KHR			dstStageMask
			m_dstAccessFromNoneAccessMask,						// VkAccessFlags2KHR				dstAccessMask
			m_testParams.writeLayout,							// VkImageLayout					oldLayout
			m_testParams.readLayout,							// VkImageLayout					newLayout
			*transitionImagePtr->handle,						// VkImage							image
			m_transitionSubresourceRange						// VkImageSubresourceRange			subresourceRange
		);
		VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
		synchronizationWrapper->cmdPipelineBarrier(*cmdBuffer, &dependencyInfo);
	}

	VkImageLayout copyImageToBufferLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	if (m_testParams.readLayout == VK_IMAGE_LAYOUT_GENERAL)
		copyImageToBufferLayout = VK_IMAGE_LAYOUT_GENERAL;

	if (m_usePipelineToRead)
	{
		beginRenderPass(vk, *cmdBuffer, *m_readRenderPass, *m_readFramebuffer, renderArea);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_readPipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_readPipelineLayout, 0, 1, &m_readDescriptorSet.get(), 0, DE_NULL);
		vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &m_vertexBuffer.handle.get(), &vertexBufferOffset);
		vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);

		endRenderPass(vk, *cmdBuffer);

		// wait till data is transfered to image
		const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,	// VkPipelineStageFlags2KHR			srcStageMask
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,				// VkAccessFlags2KHR				srcAccessMask
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,					// VkPipelineStageFlags2KHR			dstStageMask
			getAccessFlag(VK_ACCESS_2_TRANSFER_READ_BIT_KHR),		// VkAccessFlags2KHR				dstAccessMask
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				// VkImageLayout					oldLayout
			copyImageToBufferLayout,								// VkImageLayout					newLayout
			*imageToVerifyPtr->handle,								// VkImage							image
			m_readSubresourceRange									// VkImageSubresourceRange			subresourceRange
		);
		VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
		synchronizationWrapper->cmdPipelineBarrier(*cmdBuffer, &dependencyInfo);
	}

	// read back image
	{
		const VkBufferImageCopy* copyRegion = m_usePipelineToRead ? &colorCopyRegion : &transitionCopyRegion;
		vk.cmdCopyImageToBuffer(*cmdBuffer, *imageToVerifyPtr->handle, copyImageToBufferLayout, *dstBuffer.handle, 1u, copyRegion);

		const VkBufferMemoryBarrier2KHR postBufferMemoryBarrier2 = makeBufferMemoryBarrier2(
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,				// VkPipelineStageFlags2KHR			srcStageMask
			getAccessFlag(VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR),	// VkAccessFlags2KHR				srcAccessMask
			VK_PIPELINE_STAGE_2_HOST_BIT_KHR,					// VkPipelineStageFlags2KHR			dstStageMask
			getAccessFlag(VK_ACCESS_2_HOST_READ_BIT_KHR),		// VkAccessFlags2KHR				dstAccessMask
			*dstBuffer.handle,									// VkBuffer							buffer
			0u,													// VkDeviceSize						offset
			imageSizeInBytes									// VkDeviceSize						size
		);
		VkDependencyInfoKHR bufDependencyInfo = makeCommonDependencyInfo(DE_NULL, &postBufferMemoryBarrier2);
		synchronizationWrapper->cmdPipelineBarrier(*cmdBuffer, &bufDependencyInfo);
	}

	endCommandBuffer(vk, *cmdBuffer);

	Move<VkFence>					fence			= createFence(vk, device);
	VkCommandBufferSubmitInfoKHR	cmdBuffersInfo	= makeCommonCommandBufferSubmitInfo(*cmdBuffer);
	synchronizationWrapper->addSubmitInfo(0u, DE_NULL, 1u, &cmdBuffersInfo, 0u, DE_NULL);
	VK_CHECK(synchronizationWrapper->queueSubmit(queue, *fence));
	VK_CHECK(vk.waitForFences(device, 1, &fence.get(), VK_TRUE, ~0ull));

	// read image data
	invalidateAlloc(vk, device, *dstBuffer.memory);
	PixelBufferAccess resultPBA(resultFormat, m_imageExtent.width, m_imageExtent.height, m_imageExtent.depth, dstBuffer.memory->getHostPtr());

	// if result/reference is depth-stencil format then focus only on tested component
	if (isCombinedDepthStencilType(referenceFormat.type))
		referencePBA = getEffectiveDepthStencilAccess(referencePBA, (m_referenceSubresourceRange.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) ? tcu::Sampler::MODE_DEPTH : tcu::Sampler::MODE_STENCIL);
	if (isCombinedDepthStencilType(resultFormat.type))
		resultPBA = getEffectiveDepthStencilAccess(resultPBA, (m_readSubresourceRange.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) ? tcu::Sampler::MODE_DEPTH : tcu::Sampler::MODE_STENCIL);

	if (verifyResult(referencePBA, resultPBA))
		return TestStatus::pass("Pass");
	return TestStatus::fail("Fail");
}

bool NoneStageTestInstance::verifyResult(const PixelBufferAccess& reference, const PixelBufferAccess& result)
{
	TestLog& log = m_context.getTestContext().getLog();

	const auto forceStencil = (m_testParams.readLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL);

	if (isIntFormat(m_referenceImageFormat) || isUintFormat(m_referenceImageFormat) || forceStencil)
	{
		// special case for stencil (1bit gradient - top-left of image is 0, bottom-right is 1)

		bool				isResultCorrect = true;
		TextureLevel		errorMaskStorage(TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8),
											 m_imageExtent.width, m_imageExtent.height, 1);
		PixelBufferAccess	errorMask		= errorMaskStorage.getAccess();

		for (deUint32 y = 0; y < m_imageExtent.height; y++)
			for (deUint32 x = 0; x < m_imageExtent.width; x++)
			{
				// skip textels on diagonal (gradient lights texels on diagonal and stencil operation in test does not)
				if ((x + y) == (m_imageExtent.width - 1))
				{
					errorMask.setPixel(IVec4(0, 0xff, 0, 0xff), x, y, 0);
					continue;
				}

				IVec4	refPix = reference.getPixelInt(x, y, 0);
				IVec4	cmpPix = result.getPixelInt(x, y, 0);
				bool	isOk = (refPix[0] == cmpPix[0]);
				errorMask.setPixel(isOk ? IVec4(0, 0xff, 0, 0xff) : IVec4(0xff, 0, 0, 0xff), x, y, 0);
				isResultCorrect &= isOk;
			}

		Vec4 pixelBias(0.0f);
		Vec4 pixelScale(1.0f);
		if (isResultCorrect)
		{
			log << TestLog::ImageSet("Image comparison", "")
				<< TestLog::Image("Result", "Result", result, pixelScale, pixelBias)
				<< TestLog::EndImageSet;
			return true;
		}

		log << TestLog::ImageSet("Image comparison", "")
			<< TestLog::Image("Result", "Result", result, pixelScale, pixelBias)
			<< TestLog::Image("Reference", "Reference", reference, pixelScale, pixelBias)
			<< TestLog::Image("ErrorMask", "Error mask", errorMask)
			<< TestLog::EndImageSet;
		return false;
	}

	return floatThresholdCompare(log, "Image comparison", "", reference, result, tcu::Vec4(0.01f), tcu::COMPARE_LOG_RESULT);
}

class NoneStageTestCase : public vkt::TestCase
{
public:
							NoneStageTestCase		(tcu::TestContext&		testContext,
													 const std::string&		name,
													 const TestParams&		testParams);
							~NoneStageTestCase		(void) = default;

	void					initPrograms			(SourceCollections&		sourceCollections) const override;
	TestInstance*			createInstance			(Context&				context) const override;
	void					checkSupport			(Context&				context) const override;

private:
	const TestParams		m_testParams;
};

NoneStageTestCase::NoneStageTestCase(tcu::TestContext&	testContext, const std::string&	name, const TestParams&	testParams)
	: vkt::TestCase	(testContext, name, "")
	, m_testParams	(testParams)
{
}

void NoneStageTestCase::initPrograms(SourceCollections& sourceCollections) const
{
	const auto writeLayout	= m_testParams.writeLayout;
	const auto writeAspect	= m_testParams.writeAspect;
	const auto readLayout	= m_testParams.readLayout;
	const auto readAspect	= m_testParams.readAspect;

	// for tests that use only transfer and general layouts we don't create pipeline
	if (((writeLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) || (readLayout == VK_IMAGE_LAYOUT_GENERAL)) &&
		((writeLayout == VK_IMAGE_LAYOUT_GENERAL) || (readLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)))
		return;

	sourceCollections.glslSources.add("vert") << glu::VertexSource(
		"#version 450\n"
		"layout(location = 0) in  vec4 inPosition;\n"
		"layout(location = 0) out vec2 outUV;\n"
		"void main(void)\n"
		"{\n"
		"  outUV = vec2(inPosition.x * 0.5 + 0.5, inPosition.y * 0.5 + 0.5);\n"
		"  gl_Position = inPosition;\n"
		"}\n"
	);

	sourceCollections.glslSources.add("frag-color") << glu::FragmentSource(
		"#version 450\n"
		"layout(binding = 0) uniform sampler2D u_sampler;\n"
		"layout(location = 0) in vec2 inUV;\n"
		"layout(location = 0) out vec4 fragColor;\n"
		"void main(void)\n"
		"{\n"
		"  fragColor = texture(u_sampler, inUV);\n"
		"}\n"
	);

	if (writeAspect & VK_IMAGE_ASPECT_DEPTH_BIT)
	{
		sourceCollections.glslSources.add("frag-color-to-depth") << glu::FragmentSource(
			"#version 450\n"
			"layout(binding = 0) uniform sampler2D u_sampler;\n"
			"layout(location = 0) in vec2 inUV;\n"
			"void main(void)\n"
			"{\n"
			"  gl_FragDepth = texture(u_sampler, inUV).r;\n"
			"}\n"
		);
	}

	if (writeAspect & VK_IMAGE_ASPECT_STENCIL_BIT)
	{
		sourceCollections.glslSources.add("frag-color-to-stencil") << glu::FragmentSource(
			"#version 450\n"
			"void main(void)\n"
			"{\n"
			"}\n"
		);
	}
	if ((readLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) &&
		(readLayout != VK_IMAGE_LAYOUT_GENERAL) &&
		((readAspect | writeAspect) == VK_IMAGE_ASPECT_STENCIL_BIT || (readAspect == IMAGE_ASPECT_DEPTH_STENCIL && m_testParams.readLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL)))
	{
		// use usampler2D and uvec4 for color
		sourceCollections.glslSources.add("frag-stencil-to-color") << glu::FragmentSource(
			"#version 450\n"
			"layout(binding = 0) uniform usampler2D u_sampler;\n"
			"layout(location = 0) in vec2 inUV;\n"
			"layout(location = 0) out uvec4 fragColor;\n"
			"void main(void)\n"
			"{\n"
			"  fragColor = texture(u_sampler, inUV);\n"
			"}\n"
		);
	}

	if (readAspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
	{
		// for stencil only cases we need to use usubpassInput (for depth and depth_stencil we need to use subpassInput)
		const bool									readDepth			= readAspect & VK_IMAGE_ASPECT_DEPTH_BIT;
		const std::map<std::string, std::string>	specializations
		{
			{ "SUBPASS_INPUT",	(readDepth ? "subpassInput"	: "usubpassInput")	},
			{ "VALUE_TYPE",		(readDepth ? "float"		: "uint")			}
		};

		std::string source =
			"#version 450\n"
			"layout (input_attachment_index = 0, binding = 0) uniform ${SUBPASS_INPUT} depthOrStencilInput;\n"
			"layout(location = 0) in vec2 inUV;\n"
			"layout(location = 0) out ${VALUE_TYPE} fragColor;\n"
			"void main (void)\n"
			"{\n"
			"  fragColor = subpassLoad(depthOrStencilInput).x;\n"
			"}\n";
		sourceCollections.glslSources.add("frag-depth-or-stencil-to-color")
			<< glu::FragmentSource(tcu::StringTemplate(source).specialize(specializations));
	}
}

TestInstance* NoneStageTestCase::createInstance(Context& context) const
{
	return new NoneStageTestInstance(context, m_testParams);
}

void NoneStageTestCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_synchronization2");

	const auto writeAspect	= m_testParams.writeAspect;
	const auto readAspect	= m_testParams.readAspect;

	// check whether implementation supports separate depth/stencil layouts
	if (((writeAspect == VK_IMAGE_ASPECT_DEPTH_BIT) && (readAspect == VK_IMAGE_ASPECT_DEPTH_BIT)) ||
		((writeAspect == VK_IMAGE_ASPECT_STENCIL_BIT) && (readAspect == VK_IMAGE_ASPECT_STENCIL_BIT)))
	{
		if(!context.getSeparateDepthStencilLayoutsFeatures().separateDepthStencilLayouts)
			TCU_THROW(NotSupportedError, "Implementation does not support separateDepthStencilLayouts");
	}

	const auto writeLayout	= m_testParams.writeLayout;
	const auto readLayout	= m_testParams.readLayout;
	bool usePipelineToWrite	= (writeLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) && (writeLayout != VK_IMAGE_LAYOUT_GENERAL);
	bool usePipelineToRead	= (readLayout  != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) && (readLayout  != VK_IMAGE_LAYOUT_GENERAL);

	if (!usePipelineToWrite && !usePipelineToRead)
		return;

	VkFormat transitionImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
	if ((writeAspect == VK_IMAGE_ASPECT_DEPTH_BIT) || (readAspect == VK_IMAGE_ASPECT_DEPTH_BIT))
		transitionImageFormat = VK_FORMAT_D32_SFLOAT;
	else if ((writeAspect == VK_IMAGE_ASPECT_STENCIL_BIT) || (readAspect == VK_IMAGE_ASPECT_STENCIL_BIT))
		transitionImageFormat = VK_FORMAT_S8_UINT;
	else if ((writeAspect == IMAGE_ASPECT_DEPTH_STENCIL) || (readAspect == IMAGE_ASPECT_DEPTH_STENCIL))
		transitionImageFormat = VK_FORMAT_D24_UNORM_S8_UINT;

	struct FormatToCheck
	{
		VkFormat			format;
		VkImageUsageFlags	usage;
	};
	std::vector<FormatToCheck> formatsToCheck
	{
		// reference image
		{ transitionImageFormat, (VkImageUsageFlags)VK_IMAGE_USAGE_TRANSFER_DST_BIT },

		// image to write
		{ transitionImageFormat, (VkImageUsageFlags)VK_IMAGE_USAGE_TRANSFER_SRC_BIT }
	};

	// note: conditions here are analogic to conditions in test case constructor
	//       everything not needed was cout out leaving only logic related to
	//       m_referenceImage and m_imageToWrite
	if (usePipelineToWrite)
	{
		if (writeAspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
		{
			if (writeAspect & VK_IMAGE_ASPECT_DEPTH_BIT)
			{
				formatsToCheck[0].format = VK_FORMAT_R32_SFLOAT;
				formatsToCheck[0].usage	|= VK_IMAGE_USAGE_SAMPLED_BIT;
			}
			else
				formatsToCheck[0].usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

			formatsToCheck[1].usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
		else
		{
			formatsToCheck[0].usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
			formatsToCheck[1].usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}
	}

	if (usePipelineToRead)
	{
		// for layouts that operate on depth or stencil (not depth_stencil) use input attachment to read
		if ((readAspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) && (readAspect != IMAGE_ASPECT_DEPTH_STENCIL))
		{
			formatsToCheck[1].usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

			if (!usePipelineToWrite)
				formatsToCheck[0].usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
		}
		else // use image sampler for color and depth_stencil layouts
		{
			formatsToCheck[0].usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

			// for depth_stencil layouts we need to have depth_stencil_attachment usage
			if (!usePipelineToWrite && (readAspect & VK_IMAGE_ASPECT_STENCIL_BIT))
				formatsToCheck[0].usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
	}

	// it simplifies logic to pop image to write then to add conditions everywhere above
	if (!usePipelineToWrite)
		formatsToCheck.pop_back();

	for (const auto& formatData : formatsToCheck)
	{
		VkImageFormatProperties			properties;
		const vk::InstanceInterface&	vki = context.getInstanceInterface();
		if (vki.getPhysicalDeviceImageFormatProperties(context.getPhysicalDevice(),
													   formatData.format,
													   VK_IMAGE_TYPE_2D,
													   VK_IMAGE_TILING_OPTIMAL,
													   formatData.usage,
													   0,
													   &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED)
		{
			std::string error = std::string("Format (") +
								vk::getFormatName(formatData.format) + ") doesn't support required capabilities.";
			TCU_THROW(NotSupportedError, error.c_str());
		}
	}
}

} // anonymous

tcu::TestCaseGroup* createNoneStageTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> noneStageTests(new tcu::TestCaseGroup(testCtx, "none_stage", ""));

	struct LayoutData
	{
		VkImageLayout		token;
		VkImageAspectFlags	aspect;
		std::string			name;
	};

	const std::vector<LayoutData> writableLayoutsData
	{
		{ VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,							IMAGE_ASPECT_ALL,				"transfer_dst" },
		{ VK_IMAGE_LAYOUT_GENERAL,										IMAGE_ASPECT_ALL,				"general" },
		{ VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,						VK_IMAGE_ASPECT_COLOR_BIT,		"color_attachment" },
		{ VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,				IMAGE_ASPECT_DEPTH_STENCIL,		"depth_stencil_attachment" },
		{ VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,						VK_IMAGE_ASPECT_DEPTH_BIT,		"depth_attachment" },
		{ VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,					VK_IMAGE_ASPECT_STENCIL_BIT,	"stencil_attachment" },
		{ VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,						VK_IMAGE_ASPECT_COLOR_BIT,		"generic_color_attachment" },
		{ VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,						VK_IMAGE_ASPECT_DEPTH_BIT,		"generic_depth_attachment" },
		{ VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,						VK_IMAGE_ASPECT_STENCIL_BIT,	"generic_stencil_attachment" },
		{ VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,						IMAGE_ASPECT_DEPTH_STENCIL,		"generic_depth_stencil_attachment" },
	};
	const std::vector<LayoutData> readableLayoutsData
	{
		{ VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,							IMAGE_ASPECT_ALL,				"transfer_src" },
		{ VK_IMAGE_LAYOUT_GENERAL,										IMAGE_ASPECT_ALL,				"general" },
		{ VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,						IMAGE_ASPECT_ALL,				"shader_read" },
		{ VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,				IMAGE_ASPECT_DEPTH_STENCIL,		"depth_stencil_read" },
		{ VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,	IMAGE_ASPECT_DEPTH_STENCIL,		"depth_read_stencil_attachment" },
		{ VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL,	IMAGE_ASPECT_DEPTH_STENCIL,		"depth_attachment_stencil_read" },
		{ VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,						VK_IMAGE_ASPECT_DEPTH_BIT,		"depth_read" },
		{ VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL,					VK_IMAGE_ASPECT_STENCIL_BIT,	"stencil_read" },
		{ VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR,						IMAGE_ASPECT_ALL,				"generic_color_read" },
		{ VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR,						VK_IMAGE_ASPECT_DEPTH_BIT,		"generic_depth_read" },
		{ VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR,						VK_IMAGE_ASPECT_STENCIL_BIT,	"generic_stencil_read" },
		{ VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR,						IMAGE_ASPECT_DEPTH_STENCIL,		"generic_depth_stencil_read" },
	};

	struct SynchronizationData
	{
		SynchronizationType		type;
		std::string				casePrefix;
		bool					useGenericAccessFlags;
	};
	std::vector<SynchronizationData> synchronizationData
	{
		{ SynchronizationType::SYNCHRONIZATION2,	"",					true },
		{ SynchronizationType::SYNCHRONIZATION2,	"old_access_",		false },

		// using legacy synchronization structures with NONE_STAGE
		{ SynchronizationType::LEGACY,				"legacy_",			false }
	};

	for (const auto& syncData : synchronizationData)
	{
		for (const auto& writeData : writableLayoutsData)
		{
			for (const auto& readData : readableLayoutsData)
			{
				if (readData.aspect && writeData.aspect &&
				   (readData.aspect != writeData.aspect))
					continue;

				const std::string name = syncData.casePrefix + writeData.name + "_to_" + readData.name;
				noneStageTests->addChild(new NoneStageTestCase(testCtx, name, {
					syncData.type,
					syncData.useGenericAccessFlags,
					writeData.token,
					writeData.aspect,
					readData.token,
					readData.aspect
				}));
			}
		}
	}

	return noneStageTests.release();
}

} // synchronization
} // vkt
