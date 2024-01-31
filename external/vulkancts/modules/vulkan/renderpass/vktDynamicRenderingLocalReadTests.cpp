/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
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
 * \brief Vulkan Dynamic Rendering Local Read Tests
 *//*--------------------------------------------------------------------*/

#include "deRandom.hpp"
#include "deUniquePtr.hpp"

#include "tcuImageCompare.hpp"
#include "tcuRGBA.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"

#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vktDynamicRenderingLocalReadTests.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"
#include "vkTypeUtil.hpp"

#include <iostream>
#include <algorithm>

namespace vkt
{
namespace renderpass
{
namespace
{

using namespace vk;

enum class TestType
{
	// Test maximum number of attachments(color + depth + stencil) readback as input
	MAX_INPUT_ATTACHMENTS = 0,

	// Test maximum attachments remapped repeatedly
	MAX_ATTACHMENTS_REMAPPED_REPEATEDLY,

	// Test that color attachment locations set to ATTACHMENT_UNUSED are not written, and that writes to unmapped locations are discarded
	UNUSED_WRITEN_DISCARDED,

	// Test mapping depth + stencil to no index
	DEPTH_STENCIL_MAPPING_TO_NO_INDEX,

	// Test mapping depth + stencil to the same index
	DEPTH_STENCIL_MAPPING_TO_SAME_INDEX,

	// Test mapping depth + stencil so only one takes an index, the other does not
	DEPTH_MAPPING_STENCIL_NOT,

	// Test that blend state is using unmapped indexes
	MAPPING_NOT_AFFECTING_BLEND_STATE
};

// During test creation we dont know what is the maximal number of input attachments.
// To be able to test maximal number of attachments we need to construct shaders for all possible
// numbers of input attachments. This number must also not be greater then maxColorAttachments.
static deUint32 inputAttachmentsPossibleValues[] = { 4, 5, 6, 7, 8, 9, 10, 16, 17, 18 };

using ImageWithMemorySp		= de::SharedPtr<ImageWithMemory>;
using BufferWithMemorySp	= de::SharedPtr<BufferWithMemory>;
using VkImageViewSp			= de::SharedPtr<Move<VkImageView> >;

ImageWithMemorySp createImage(Context& context, deUint32 renderSize, VkFormat format, VkImageUsageFlags usage)
{
	const DeviceInterface&	vk			= context.getDeviceInterface();
	VkDevice				device		= context.getDevice();
	Allocator&				memAlloc	= context.getDefaultAllocator();
	VkExtent3D				extent		= makeExtent3D(renderSize, renderSize, 1u);

	const VkImageCreateInfo imageCreateInfo
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		//	VkStructureType			sType;
		DE_NULL,									//	const void*				pNext;
		0u,											//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,							//	VkImageType				imageType;
		format,										//	VkFormat				format;
		extent,										//	VkExtent3D				extent;
		1u,											//	deUint32				mipLevels;
		1u,											//	deUint32				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,						//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,					//	VkImageTiling			tiling;
		usage,										//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,					//	VkSharingMode			sharingMode;
		0u,											//	deUint32				queueFamilyIndexCount;
		DE_NULL,									//	const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,					//	VkImageLayout			initialLayout;
	};

	return ImageWithMemorySp(new ImageWithMemory(vk, device, memAlloc, imageCreateInfo, MemoryRequirement::Any));
}

class BasicLocalReadTestInstance : public vkt::TestInstance
{
public:
						BasicLocalReadTestInstance	(Context&	context,
													 TestType	testType);
protected:
	tcu::TestStatus		iterate						(void) override;

private:
	const TestType						m_testType;
	const deUint32						m_renderSize;
	VkFormat							m_dsFormat;

	deUint32							m_colorAttachmentCount;
	deUint32							m_inputDrawsCount;
	deUint32							m_outputDrawsCount;
	std::string							m_writeFragName;
	std::string							m_readFragName;
	std::vector<std::vector<deUint32> >	m_colorAttachmentLocations;
	std::vector<std::vector<deUint32> >	m_colorAttachmentInputIndices;
	deUint32							m_depthInputAttachmentIndex;
	deUint32							m_stencilInputAttachmentIndex;
	std::vector<deUint32>				m_expectedValues;
};

BasicLocalReadTestInstance::BasicLocalReadTestInstance(Context&		context,
													   TestType		testType)
	: vkt::TestInstance				(context)
	, m_testType					(testType)
	, m_renderSize					(16)
	, m_dsFormat					(VK_FORMAT_D24_UNORM_S8_UINT)
	, m_colorAttachmentCount		(4)
	, m_inputDrawsCount				(1)
	, m_outputDrawsCount			(1)
	, m_writeFragName				("frag0")
	, m_readFragName				("frag1")
	, m_colorAttachmentLocations	{ { 0, 1, 2, 3 } }
	, m_colorAttachmentInputIndices	{ { 0, 1, 2, 3 } }
	, m_depthInputAttachmentIndex	(4)
	, m_stencilInputAttachmentIndex	(5)
	, m_expectedValues				{ 1600 }
{
	const InstanceInterface&			vki				= m_context.getInstanceInterface();
	VkPhysicalDevice					physicalDevice	= m_context.getPhysicalDevice();
	const VkPhysicalDeviceProperties	properties		= getPhysicalDeviceProperties(vki, physicalDevice);

	// pick depth stencil format (one of those two has to be supported)
	VkImageFormatProperties imageFormatProperties;
	if (vki.getPhysicalDeviceImageFormatProperties(physicalDevice, m_dsFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, 0, &imageFormatProperties))
		m_dsFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;

	// setup test configuration - all test cases use same test instance code but with diferent parameters
	switch (m_testType)
	{
	case TestType::MAX_INPUT_ATTACHMENTS:
	{
		m_colorAttachmentCount = deMinu32(properties.limits.maxColorAttachments, properties.limits.maxPerStageDescriptorInputAttachments - 2u);

		// if this assert is trigered then shader for number m_colorAttachmentCount+2 was not prepared;
		// to fix this just add value of m_colorAttachmentCount+2 to the inputAttachmentsPossibleValues array on top of this file
		DE_ASSERT(std::find(std::begin(inputAttachmentsPossibleValues), std::end(inputAttachmentsPossibleValues), m_colorAttachmentCount + 2) != std::end(inputAttachmentsPossibleValues));

		m_writeFragName					+= "_" + std::to_string(m_colorAttachmentCount);
		m_readFragName					+= "_" + std::to_string(m_colorAttachmentCount + 2u);	// +2 because depth and stencil are read too
		m_depthInputAttachmentIndex		= m_colorAttachmentCount;
		m_stencilInputAttachmentIndex	= m_colorAttachmentCount + 1;

		m_colorAttachmentLocations[0].resize(m_colorAttachmentCount);
		m_colorAttachmentInputIndices[0].resize(m_colorAttachmentCount);
		for (deUint32 attIndex = 0; attIndex < m_colorAttachmentCount; ++attIndex)
		{
			m_colorAttachmentLocations[0][attIndex]		= attIndex;
			m_colorAttachmentInputIndices[0][attIndex]	= attIndex;
			m_expectedValues[0]							+= (attIndex + 1) * (attIndex + 1);
		}
		break;
	}
	case TestType::MAX_ATTACHMENTS_REMAPPED_REPEATEDLY:
	{
		m_colorAttachmentCount			= deMinu32(properties.limits.maxColorAttachments, properties.limits.maxPerStageDescriptorInputAttachments - 2u);
		m_inputDrawsCount				= m_colorAttachmentCount / 2u;
		m_colorAttachmentCount			= m_inputDrawsCount * 2u;
		m_outputDrawsCount				= 3;
		m_readFragName					+= "_" + std::to_string(m_colorAttachmentCount + 2u);	// +2 because depth and stencil are read too
		m_depthInputAttachmentIndex		= m_colorAttachmentCount;
		m_stencilInputAttachmentIndex	= m_colorAttachmentCount + 1;

		// each input draw uses just two color attachments; each time its different pair that is always mapped to location 0 and 1
		m_colorAttachmentLocations.clear();
		m_colorAttachmentLocations.resize(m_inputDrawsCount, std::vector<deUint32>(m_colorAttachmentCount, VK_ATTACHMENT_UNUSED));
		for (deUint32 inputDraw = 0; inputDraw < m_inputDrawsCount; ++inputDraw)
		{
			auto&		colorAttachmentLocations	= m_colorAttachmentLocations[inputDraw];
			deUint32	index						= 2u * inputDraw;
			colorAttachmentLocations[index]			= 0u;
			colorAttachmentLocations[index+1u]		= 1u;
		}

		// allocate space for all different remappings
		m_colorAttachmentInputIndices.resize(m_outputDrawsCount);
		for (auto& inputIndices : m_colorAttachmentInputIndices)
			inputIndices.resize(m_colorAttachmentCount);

		// each output draw will use all input attachments but remapped differently
		// first remapping: reversed order, from greatest index to smallest
		// second remapping: alternately takes the smallest not used index and then the largest not used index
		// third remapping: same order as index, from smallest index to greatest
		for (deUint32 attIndex = 0; attIndex < m_colorAttachmentCount; ++attIndex)
		{
			m_colorAttachmentInputIndices[0][attIndex] = m_colorAttachmentCount - 1 - attIndex;
			m_colorAttachmentInputIndices[1][attIndex] = (attIndex % 2) ? (m_colorAttachmentCount - 1 - attIndex / 2u) : (attIndex / 2u);
			m_colorAttachmentInputIndices[2][attIndex] = attIndex;
		}

		// generate same valueas for each attachment as in frag0_* shader
		deUint32 attachmentIndex = 0;
		std::vector<deUint32> valuesPerColorAttachment(m_colorAttachmentCount, 0);
		for (deUint32 inputDraw = 0; inputDraw < m_inputDrawsCount; ++inputDraw)
		{
			valuesPerColorAttachment[attachmentIndex]	= 2u * inputDraw + 1u;
			valuesPerColorAttachment[attachmentIndex+1]	= 2u * inputDraw + 2u;
			attachmentIndex += 2u;
		}

		// calculate expected values for all three output draws, same as it will be done in frag1_* shader
		m_expectedValues = { 1600 , 1600, 1600 };
		for (deUint32 outputDraw = 0; outputDraw < m_outputDrawsCount; ++outputDraw)
		{
			// each output draw uses all attachments but remaped differently
			for (deUint32 attIndex = 0; attIndex < m_colorAttachmentCount; ++attIndex)
			{
				// perform same operation as in frag1_* shader to calculate final expected value
				deUint32 attIndexRemaped = m_colorAttachmentInputIndices[outputDraw][attIndex];
				m_expectedValues[outputDraw] += (attIndex + 1u) * valuesPerColorAttachment[attIndexRemaped];
			}
		}
		break;
	}
	case TestType::UNUSED_WRITEN_DISCARDED:
	{
		m_colorAttachmentLocations[0][0]	= VK_ATTACHMENT_UNUSED;
		m_colorAttachmentLocations[0][2]	= VK_ATTACHMENT_UNUSED;
		m_expectedValues[0]					+= 20;		// 0 * 1 + 2 * 2 + 0 * 3 + 4 * 4 = 20
		break;
	}
	case TestType::DEPTH_STENCIL_MAPPING_TO_NO_INDEX:
	{
		m_colorAttachmentCount			= 2;
		m_depthInputAttachmentIndex		= VK_ATTACHMENT_UNUSED;
		m_stencilInputAttachmentIndex	= VK_ATTACHMENT_UNUSED;
		m_expectedValues[0]				= 703;
		break;
	}
	case TestType::DEPTH_STENCIL_MAPPING_TO_SAME_INDEX:
	{
		m_colorAttachmentCount			= 2;
		m_depthInputAttachmentIndex		= 2;
		m_stencilInputAttachmentIndex	= 2;
		m_expectedValues[0]				= 601;
		break;
	}
	case TestType::DEPTH_MAPPING_STENCIL_NOT:
	{
		m_depthInputAttachmentIndex		= 4;
		m_stencilInputAttachmentIndex	= VK_ATTACHMENT_UNUSED;
		m_expectedValues[0]				= 630;
		break;
	}
	default:
		DE_ASSERT(false);
		break;
	}
}

tcu::TestStatus BasicLocalReadTestInstance::iterate (void)
{
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					device				= m_context.getDevice();
	Allocator&						memAlloc			= m_context.getDefaultAllocator();
	VkQueue							queue				= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkImageSubresourceRange	colorSRR			= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const VkImageSubresourceRange	dSRR				= makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);
	const VkImageSubresourceRange	sSRR				= makeImageSubresourceRange(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 1u, 0u, 1u);
	const VkImageSubresourceRange	dsSRR				= makeImageSubresourceRange(dSRR.aspectMask | sSRR.aspectMask, 0u, 1u, 0u, 1u);
	const std::vector<VkViewport>	viewports			{ makeViewport(m_renderSize, m_renderSize) };
	const std::vector<VkRect2D>		scissors			{ makeRect2D(m_renderSize, m_renderSize) };

	// define few structures that will be modified and reused in multiple places
	VkImageMemoryBarrier colorImageBarrier = makeImageMemoryBarrier(
		0,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		0,
		colorSRR);
	VkImageMemoryBarrier dsImageBarrier = makeImageMemoryBarrier(
		0,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		0,
		dsSRR);
	VkRenderingAttachmentInfo depthStencilAttachment
	{
		VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,			// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		DE_NULL,												// VkImageView							imageView;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				// VkImageLayout						imageLayout;
		VK_RESOLVE_MODE_NONE,									// VkResolveModeFlagBits				resolveMode;
		DE_NULL,												// VkImageView							resolveImageView;
		VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout						resolveImageLayout;
		VK_ATTACHMENT_LOAD_OP_CLEAR,							// VkAttachmentLoadOp					loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,							// VkAttachmentStoreOp					storeOp;
		makeClearValueColor(tcu::Vec4(0.0f))					// VkClearValue							clearValue;
	};
	VkImageUsageFlags			imageUsage				= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	VkDescriptorImageInfo		depthImageDescriptor	(makeDescriptorImageInfo(DE_NULL, DE_NULL, VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR));
	VkDescriptorImageInfo		stencilImageDescriptor	(depthImageDescriptor);

	// construct required containers
	std::vector<ImageWithMemorySp>			images					(m_colorAttachmentCount + 1, ImageWithMemorySp());	// +1 for depth+stencil image
	std::vector<VkImageViewSp>				imageViews				(m_colorAttachmentCount + 3, VkImageViewSp());		// +3 for separate depth and stencil and depth+stencil
	std::vector<VkFormat>					colorImageFormats		(m_colorAttachmentCount, VK_FORMAT_R32_UINT);
	std::vector<VkImageMemoryBarrier>		colorImageBarriers		(m_colorAttachmentCount, colorImageBarrier);
	std::vector<VkRenderingAttachmentInfo>	colorAttachments		(m_colorAttachmentCount, depthStencilAttachment);
	std::vector<VkDescriptorImageInfo>		colorImageDescriptors	(m_colorAttachmentCount, depthImageDescriptor);
	std::vector<BufferWithMemorySp>			outputBuffers			(m_outputDrawsCount, BufferWithMemorySp());
	std::vector<Move<VkPipeline> >			writeGraphicsPipelines	(m_inputDrawsCount);
	std::vector<Move<VkPipeline> >			readGraphicsPipelines	(m_outputDrawsCount);

	// create images and image views for color attachments and use same loop to fill other needed containers
	for (deUint32 attIndex = 0; attIndex < m_colorAttachmentCount; ++attIndex)
	{
		images[attIndex]		= createImage(m_context, m_renderSize, colorImageFormats[attIndex], imageUsage);
		imageViews[attIndex]	= VkImageViewSp(new vk::Move<VkImageView>(makeImageView(vk, device, **images[attIndex], VK_IMAGE_VIEW_TYPE_2D, colorImageFormats[attIndex], colorSRR)));

		colorImageBarriers[attIndex].image			= **images[attIndex];
		colorAttachments[attIndex].imageView		= **imageViews[attIndex];
		colorImageDescriptors[attIndex].imageView	= **imageViews[attIndex];
	}

	// create image and image views for depth/stencil attachments
	deUint32 depthIndex					= m_colorAttachmentCount;
	images[depthIndex]					= createImage(m_context, m_renderSize, m_dsFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	imageViews[depthIndex]				= VkImageViewSp(new vk::Move<VkImageView>(makeImageView(vk, device, **images[depthIndex], VK_IMAGE_VIEW_TYPE_2D, m_dsFormat, dSRR)));
	imageViews[depthIndex + 1]			= VkImageViewSp(new vk::Move<VkImageView>(makeImageView(vk, device, **images[depthIndex], VK_IMAGE_VIEW_TYPE_2D, m_dsFormat, sSRR)));
	imageViews[depthIndex + 2]			= VkImageViewSp(new vk::Move<VkImageView>(makeImageView(vk, device, **images[depthIndex], VK_IMAGE_VIEW_TYPE_2D, m_dsFormat, dsSRR)));
	dsImageBarrier.image				= **images[depthIndex];
	depthImageDescriptor.imageView		= **imageViews[depthIndex];
	stencilImageDescriptor.imageView	= **imageViews[depthIndex+1];
	depthStencilAttachment.imageView	= **imageViews[depthIndex+2];
	depthStencilAttachment.imageLayout	= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// define buffers for output
	const VkDeviceSize			outputBufferSize	= static_cast<VkDeviceSize>(m_renderSize * m_renderSize * sizeof(deUint32));
	const VkBufferCreateInfo	bufferCreateInfo	= makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	for (deUint32 buffIndex = 0 ; buffIndex < m_outputDrawsCount ; ++buffIndex)
		outputBuffers[buffIndex] = BufferWithMemorySp(new BufferWithMemory(vk, device, memAlloc, bufferCreateInfo, MemoryRequirement::HostVisible));

	// create descriptors, they are needed just for read pipelines (usually there is just one read pipeline)
	Move<VkDescriptorPool> descriptorPool = DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, (m_colorAttachmentCount + 2u) * m_outputDrawsCount)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_outputDrawsCount)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 2u * m_outputDrawsCount);

	// first descriptor set contains all input attachments
	DescriptorSetLayoutBuilder	descriptorSetLayoutABuilder;
	for (deUint32 attIndex = 0; attIndex < m_colorAttachmentCount + 2; ++attIndex)
		descriptorSetLayoutABuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
	Move<VkDescriptorSetLayout> descriptorSetLayoutA = descriptorSetLayoutABuilder.build(vk, device);

	// second and following descriptor sets contain just single output buffer each
	Move<VkDescriptorSetLayout> descriptorSetLayoutB = DescriptorSetLayoutBuilder()
							.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
							.build(vk, device);

	std::vector<Move<VkDescriptorSet> > bufferDescriptorSets(m_outputDrawsCount);
	std::vector<Move<VkDescriptorSet> > inputAttachmentsDescriptorSets(m_outputDrawsCount);
	for (deUint32 i = 0; i < m_outputDrawsCount; ++i)
	{
		inputAttachmentsDescriptorSets[i] = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayoutA);
		bufferDescriptorSets[i] = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayoutB);
	}

	// set descriptor sets for all input attachments
	using DSLocation = DescriptorSetUpdateBuilder::Location;
	DescriptorSetUpdateBuilder descriptorSetUpdateBuilder;
	for (deUint32 i = 0; i < m_outputDrawsCount; ++i)
	{
		// content of the descriptor set with input attachment bindings must be consistent with the remapping
		for (deUint32 attIndex = 0; attIndex < m_colorAttachmentCount; ++attIndex)
		{
			deUint32 remapedIndex = m_colorAttachmentInputIndices[i][attIndex];
			descriptorSetUpdateBuilder.writeSingle(*inputAttachmentsDescriptorSets[i], DSLocation::binding(attIndex), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &colorImageDescriptors[remapedIndex]);
		}
		descriptorSetUpdateBuilder.writeSingle(*inputAttachmentsDescriptorSets[i], DSLocation::binding(m_colorAttachmentCount), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &depthImageDescriptor);
		descriptorSetUpdateBuilder.writeSingle(*inputAttachmentsDescriptorSets[i], DSLocation::binding(m_colorAttachmentCount + 1), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &stencilImageDescriptor);
	}

	// set descriptor sets for all output buffers
	VkDescriptorBufferInfo bufferInfo = makeDescriptorBufferInfo(DE_NULL, 0u, VK_WHOLE_SIZE);
	for (deUint32 buffIndex = 0; buffIndex < m_outputDrawsCount; ++buffIndex)
	{
		bufferInfo.buffer = **outputBuffers[buffIndex];
		descriptorSetUpdateBuilder.writeSingle(*bufferDescriptorSets[buffIndex], DSLocation::binding(0), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo);
	}

	// update descriptor sets
	descriptorSetUpdateBuilder.update(vk, device);

	// create components for pipelines
	const VkPushConstantRange			pushConstantRange		= { VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4 };
	std::vector<VkDescriptorSetLayout>	descriptorSetLayouts	= { *descriptorSetLayoutA, *descriptorSetLayoutB };
	Move<VkPipelineLayout>				writePipelineLayout		= makePipelineLayout(vk, device, 0, DE_NULL, 1u, &pushConstantRange);
	Move<VkPipelineLayout>				readPipelineLayout		= makePipelineLayout(vk, device, descriptorSetLayouts);
	Move<VkShaderModule>				vertShaderModule		= createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
	Move<VkShaderModule>				writeFragShaderModule	= createShaderModule(vk, device, m_context.getBinaryCollection().get(m_writeFragName), 0);
	Move<VkShaderModule>				readFragShaderModule	= createShaderModule(vk, device, m_context.getBinaryCollection().get(m_readFragName), 0);

	// define empty VertexInputState, full screen quad will be generated in vertex shader
	const VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();

	// define ColorBlendState so that we can write to multiple color attachments
	const VkPipelineColorBlendAttachmentState			colorBlendAttachmentState	{ 0, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 0xf };
	std::vector<VkPipelineColorBlendAttachmentState>	colorBlendAttachmentStates	(m_colorAttachmentCount, colorBlendAttachmentState);
	VkPipelineColorBlendStateCreateInfo					colorBlendStateCreateInfo	= initVulkanStructure();
	colorBlendStateCreateInfo.attachmentCount	= (deUint32)colorBlendAttachmentStates.size();
	colorBlendStateCreateInfo.pAttachments		= colorBlendAttachmentStates.data();

	// define DepthStencilState so that we can write to depth and stencil attachments
	const VkStencilOpState stencilOpState
	{
		VK_STENCIL_OP_KEEP,											// VkStencilOp								failOp
		VK_STENCIL_OP_INCREMENT_AND_CLAMP,							// VkStencilOp								passOp
		VK_STENCIL_OP_KEEP,											// VkStencilOp								depthFailOp
		VK_COMPARE_OP_ALWAYS,										// VkCompareOp								compareOp
		0xffu,														// deUint32									compareMask
		0xffu,														// deUint32									writeMask
		0															// deUint32									reference
	};
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType
		DE_NULL,													// const void*								pNext
		0u,															// VkPipelineDepthStencilStateCreateFlags	flags
		VK_TRUE,													// VkBool32									depthTestEnable
		VK_TRUE,													// VkBool32									depthWriteEnable
		VK_COMPARE_OP_GREATER,										// VkCompareOp								depthCompareOp
		VK_FALSE,													// VkBool32									depthBoundsTestEnable
		VK_TRUE,													// VkBool32									stencilTestEnable
		stencilOpState,												// VkStencilOpState							front
		stencilOpState,												// VkStencilOpState							back
		0.0f,														// float									minDepthBounds
		1.0f,														// float									maxDepthBounds
	};

	VkRenderingAttachmentLocationInfoKHR renderingAttachmentLocationInfo
	{
		VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_LOCATION_INFO_KHR,
		DE_NULL,
		m_colorAttachmentCount,
		DE_NULL
	};
	VkRenderingInputAttachmentIndexInfoKHR renderingInputAttachmentIndexInfo
	{
		VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO_KHR,
		DE_NULL,
		m_colorAttachmentCount,
		DE_NULL,
		&m_depthInputAttachmentIndex,
		&m_stencilInputAttachmentIndex
	};
	VkPipelineRenderingCreateInfo renderingCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
		&renderingAttachmentLocationInfo,
		0u,
		(deUint32)colorImageFormats.size(),
		colorImageFormats.data(),
		m_dsFormat,
		m_dsFormat
	};

	// create write pipelines that writes to color attachments
	for (deUint32 pipelineIndex = 0; pipelineIndex < m_inputDrawsCount; ++pipelineIndex)
	{
		renderingAttachmentLocationInfo.pColorAttachmentLocations = m_colorAttachmentLocations[pipelineIndex].data();
		writeGraphicsPipelines[pipelineIndex] = makeGraphicsPipeline(vk, device, *writePipelineLayout, *vertShaderModule, DE_NULL, DE_NULL, DE_NULL,
																	 *writeFragShaderModule, DE_NULL, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
																	 0, 0, &vertexInputState, DE_NULL, DE_NULL, &depthStencilStateCreateInfo,
																	 &colorBlendStateCreateInfo, DE_NULL, &renderingCreateInfo);

		// writte to depth and stencil only in first pipeline
		depthStencilStateCreateInfo.depthTestEnable		= DE_FALSE;
		depthStencilStateCreateInfo.stencilTestEnable	= DE_FALSE;
	}

	// read pipelines need input attachments remaping
	renderingCreateInfo.pNext = &renderingInputAttachmentIndexInfo;

	for (deUint32 pipelineIndex = 0; pipelineIndex < m_outputDrawsCount; ++pipelineIndex)
	{
		renderingInputAttachmentIndexInfo.pColorAttachmentInputIndices	= m_colorAttachmentInputIndices[pipelineIndex].data();
		readGraphicsPipelines[pipelineIndex] = makeGraphicsPipeline(vk, device, *readPipelineLayout, *vertShaderModule, DE_NULL, DE_NULL, DE_NULL,
																	*readFragShaderModule, DE_NULL, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
																	1, 0, &vertexInputState, DE_NULL, DE_NULL, DE_NULL, &colorBlendStateCreateInfo, DE_NULL, &renderingCreateInfo);
	}

	Move<VkCommandPool>		commandPool		= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	Move<VkCommandBuffer>	commandBuffer	= allocateCommandBuffer(vk, device, *commandPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	VkCommandBuffer			cmdBuffer		= *commandBuffer;

	VkRenderingInfo renderingInfo
	{
		VK_STRUCTURE_TYPE_RENDERING_INFO,
		DE_NULL,
		0u,											// VkRenderingFlags						flags
		scissors[0],								// VkRect2D								renderArea
		1u,											// deUint32								layerCount
		0u,											// deUint32								viewMask
		(deUint32)colorAttachments.size(),			// deUint32								colorAttachmentCount
		colorAttachments.data(),					// const VkRenderingAttachmentInfo*		pColorAttachments
		&depthStencilAttachment,					// const VkRenderingAttachmentInfo*		pDepthAttachment
		&depthStencilAttachment						// const VkRenderingAttachmentInfo*		pStencilAttachment
	};

	// record commands
	beginCommandBuffer(vk, cmdBuffer);

	// transition all images to proper layouts
	vk.cmdPipelineBarrier(cmdBuffer, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, DE_NULL, 0, DE_NULL, (deUint32)colorImageBarriers.size(), colorImageBarriers.data());
	vk.cmdPipelineBarrier(cmdBuffer, 0, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, DE_NULL, 0, DE_NULL, 1u, &dsImageBarrier);

	vk.cmdBeginRendering(cmdBuffer, &renderingInfo);

	// draw using write pipelines
	for (deUint32 pipelineIndex = 0; pipelineIndex < m_inputDrawsCount; ++pipelineIndex)
	{
		renderingAttachmentLocationInfo.pColorAttachmentLocations = m_colorAttachmentLocations[pipelineIndex].data();

		vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *writeGraphicsPipelines[pipelineIndex]);
		vk.cmdPushConstants(cmdBuffer, *writePipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4, &pipelineIndex);
		vk.cmdSetRenderingAttachmentLocationsKHR(cmdBuffer, &renderingAttachmentLocationInfo);
		vk.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
	}

	// reuse existing barrier structures to finish rendering before next subpass
	dsImageBarrier.oldLayout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	dsImageBarrier.newLayout		= VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR;
	dsImageBarrier.srcAccessMask	= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dsImageBarrier.dstAccessMask	= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	for (auto& barrier : colorImageBarriers)
	{
		barrier.oldLayout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier.newLayout		= VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR;
		barrier.srcAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask	= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	}
	vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, DE_NULL, 0, DE_NULL, (deUint32)colorImageBarriers.size(), colorImageBarriers.data());
	vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, DE_NULL, 0, DE_NULL, 1u, &dsImageBarrier);

	// draw using read pipelines
	for (deUint32 pipelineIndex = 0; pipelineIndex < m_outputDrawsCount; ++pipelineIndex)
	{
		VkDescriptorSet descriptorSets[] = { *inputAttachmentsDescriptorSets[pipelineIndex], *bufferDescriptorSets[pipelineIndex] };
		renderingInputAttachmentIndexInfo.pColorAttachmentInputIndices = m_colorAttachmentInputIndices[pipelineIndex].data();

		vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *readGraphicsPipelines[pipelineIndex]);
		vk.cmdSetRenderingInputAttachmentIndicesKHR(cmdBuffer, &renderingInputAttachmentIndexInfo);
		vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *readPipelineLayout, 0u, 2u, descriptorSets, 0u, DE_NULL);
		vk.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);
	}

	vk.cmdEndRendering(cmdBuffer);

	endCommandBuffer(vk, cmdBuffer);
	submitCommandsAndWait(vk, device, queue, cmdBuffer);

	// verify all output buffers
	bool	allOk	= true;
	auto&	log		= m_context.getTestContext().getLog();
	DE_ASSERT(m_expectedValues.size() == m_outputDrawsCount);
	for (deUint32 buffIndex = 0; buffIndex < m_outputDrawsCount; ++buffIndex)
	{
		// get output buffer
		const Allocation& bufferAllocation = outputBuffers[buffIndex]->getAllocation();
		invalidateAlloc(vk, device, bufferAllocation);

		// validate result
		const deUint32* bufferPtr = static_cast<deUint32*>(bufferAllocation.getHostPtr());
		const deUint32 expected = m_expectedValues[buffIndex];
		for (deUint32 i = 0 ; i < m_renderSize * m_renderSize; ++i)
		{
			if (bufferPtr[i] != expected)
			{
				log << tcu::TestLog::Message << "Result for buffer " << buffIndex
					<< ": expected " << expected
					<< " got " << bufferPtr[i] << " at index " << i
					<< tcu::TestLog::EndMessage;
				allOk = false;
				break;
			}
		}
	}

	if (allOk)
		return tcu::TestStatus::pass("Pass");

	const VkBufferCreateInfo	attBufferCreateInfo	= makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	BufferWithMemorySp			attBuffer			(new BufferWithMemory(vk, device, memAlloc, attBufferCreateInfo, MemoryRequirement::HostVisible));
	vk::VkBufferImageCopy		region				{ 0, 0, 0, { 1, 0, 0, 1 }, { 0, 0, 0 }, { 1, 1, 1 } };
	auto&						barrier				= colorImageBarriers[0];

	// reuse first barrier structure
	barrier.oldLayout = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	// log content of each color attachment
	for (deUint32 attIndex = 0; attIndex < m_colorAttachmentCount; ++attIndex)
	{
		barrier.image	= **images[attIndex];
		commandBuffer	= allocateCommandBuffer(vk, device, *commandPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		cmdBuffer		= *commandBuffer;

		beginCommandBuffer(vk, cmdBuffer);
		vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, 1, &barrier);
		vk.cmdCopyImageToBuffer(cmdBuffer, barrier.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **attBuffer, 1, &region);
		endCommandBuffer(vk, cmdBuffer);

		submitCommandsAndWait(vk, device, queue, cmdBuffer);

		const Allocation& bufferAllocation = attBuffer->getAllocation();
		invalidateAlloc(vk, device, bufferAllocation);
		const deUint32* bufferPtr = static_cast<deUint32*>(bufferAllocation.getHostPtr());
		log << tcu::TestLog::Message << "Color attachment " << attIndex << " contains: " << bufferPtr[0] << tcu::TestLog::EndMessage;
	}

	return tcu::TestStatus::fail("Fail");
}

class MappingWithBlendStateTestInstance : public vkt::TestInstance
{
public:
	MappingWithBlendStateTestInstance	(Context&			context,
										 const TestType		testType);
protected:
	tcu::TestStatus		iterate			(void) override;

private:
	const deUint32 m_renderSize;
};

MappingWithBlendStateTestInstance::MappingWithBlendStateTestInstance(Context& context, const TestType testType)
	: vkt::TestInstance		(context)
	, m_renderSize			(16)
{
	DE_UNREF(testType);
}

tcu::TestStatus MappingWithBlendStateTestInstance::iterate()
{
	const DeviceInterface&			vk						= m_context.getDeviceInterface();
	const VkDevice					device					= m_context.getDevice();
	Allocator&						memAlloc				= m_context.getDefaultAllocator();
	VkQueue							queue					= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const VkFormat					imageFormat				= VK_FORMAT_R8G8B8A8_UNORM;
	const tcu::TextureFormat		textureFormat			= mapVkFormat(imageFormat);
	const deUint32					colorAttachmentCount	= 4u;

	const VkImageSubresourceRange	colorSRR				= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const VkImageSubresourceLayers	colorSL					= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const VkBufferImageCopy			copyRegion				= makeBufferImageCopy({ m_renderSize, m_renderSize, 1u }, colorSL);

	Move<VkCommandPool>				commandPool				= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	Move<VkCommandBuffer>			commandBuffer			= allocateCommandBuffer(vk, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	VkCommandBuffer					cmdBuffer				= *commandBuffer;

	const std::vector<VkViewport>	viewports				{ makeViewport(m_renderSize, m_renderSize) };
	const std::vector<VkRect2D>		scissors				{ makeRect2D(m_renderSize, m_renderSize) };

	const tcu::Vec4 clearValues[]	// blend src color
	{
		{ 0.5f, 0.5f, 0.4f, 1.0f },
		{ 0.2f, 0.4f, 0.3f, 0.1f },
		{ 0.2f, 1.0f, 0.5f, 0.8f },
		{ 1.0f, 0.1f, 0.3f, 0.5f },
	};
	DE_ASSERT(DE_LENGTH_OF_ARRAY(clearValues) == colorAttachmentCount);

	const VkPipelineColorBlendAttachmentState colorBlendAttachmentStates[]
	{
		{ 1, VK_BLEND_FACTOR_SRC_ALPHA,				VK_BLEND_FACTOR_DST_ALPHA,				VK_BLEND_OP_ADD,		VK_BLEND_FACTOR_SRC_ALPHA,				VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_OP_MAX,		0xf },
		{ 1, VK_BLEND_FACTOR_ONE,					VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,	VK_BLEND_OP_SUBTRACT,	VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,	VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_OP_ADD,		0xf },
		{ 1, VK_BLEND_FACTOR_ONE,					VK_BLEND_FACTOR_ONE,					VK_BLEND_OP_MIN,		VK_BLEND_FACTOR_SRC_ALPHA,				VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_OP_MAX,		0xf },
		{ 1, VK_BLEND_FACTOR_SRC_COLOR,				VK_BLEND_FACTOR_DST_COLOR,				VK_BLEND_OP_MAX,		VK_BLEND_FACTOR_SRC_ALPHA,				VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_OP_MIN,		0xf },
	};
	DE_ASSERT(DE_LENGTH_OF_ARRAY(colorBlendAttachmentStates) == colorAttachmentCount);

	const deUint32 colorAttachmentLocations[]{ 3, 0, 2, 1 };
	DE_ASSERT(DE_LENGTH_OF_ARRAY(colorAttachmentLocations) == colorAttachmentCount);

	VkRenderingAttachmentInfo colorAttachment
	{
		VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,				// VkStructureType			sType;
		DE_NULL,													// const void*				pNext;
		DE_NULL,													// VkImageView				imageView;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,					// VkImageLayout			imageLayout;
		VK_RESOLVE_MODE_NONE,										// VkResolveModeFlagBits	resolveMode;
		DE_NULL,													// VkImageView				resolveImageView;
		VK_IMAGE_LAYOUT_UNDEFINED,									// VkImageLayout			resolveImageLayout;
		VK_ATTACHMENT_LOAD_OP_CLEAR,								// VkAttachmentLoadOp		loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,								// VkAttachmentStoreOp		storeOp;
		makeClearValueColor(tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f))		// VkClearValue				clearValue;
	};
	VkImageMemoryBarrier		imageMemoryBarrier	= makeImageMemoryBarrier(0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
																			 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
																			 DE_NULL, colorSRR);
	const VkDeviceSize			outputBufferSize	= (VkDeviceSize)m_renderSize * m_renderSize * tcu::getPixelSize(textureFormat);
	const VkBufferCreateInfo	outputBufferInfo	= makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

	// construct required containers
	std::vector<ImageWithMemorySp>			images					(colorAttachmentCount, ImageWithMemorySp());
	std::vector<VkImageViewSp>				imageViews				(colorAttachmentCount, VkImageViewSp());
	std::vector<VkFormat>					colorImageFormats		(colorAttachmentCount, imageFormat);
	std::vector<VkRenderingAttachmentInfo>	colorAttachments		(colorAttachmentCount, colorAttachment);
	std::vector<VkImageMemoryBarrier>		colorImageBarriers		(colorAttachmentCount, imageMemoryBarrier);
	std::vector <BufferWithMemorySp>		outputBuffers			(colorAttachmentCount, BufferWithMemorySp());

	for (deUint32 i = 0; i < colorAttachmentCount; ++i)
	{
		// create images and image views for input attachments
		images[i]			= createImage(m_context, m_renderSize, imageFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
		imageViews[i]		= VkImageViewSp(new vk::Move<VkImageView>(makeImageView(vk, device, **images[i], VK_IMAGE_VIEW_TYPE_2D, imageFormat, colorSRR)));

		colorAttachments[i].imageView	= **imageViews[i];
		colorAttachments[i].clearValue	= makeClearValueColor(clearValues[i]);
		colorImageBarriers[i].image		= **images[i];

		// create output buffers that will be used to get attachments data
		outputBuffers[i]	= BufferWithMemorySp(new BufferWithMemory(vk, device, memAlloc, outputBufferInfo, MemoryRequirement::HostVisible));
	}

	Move<VkPipelineLayout>	pipelineLayout		= makePipelineLayout(vk, device, DE_NULL);
	Move<VkShaderModule>	vertShaderModule	= createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
	Move<VkShaderModule>	fragShaderModule	= createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0);

	// define empty VertexInputState, full screen quad will be generated in vertex shader
	const VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();

	// define ColorBlendState so that we can write to multiple color attachments
	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = initVulkanStructure();
	colorBlendStateCreateInfo.attachmentCount	= colorAttachmentCount;
	colorBlendStateCreateInfo.pAttachments		= colorBlendAttachmentStates;

	VkRenderingAttachmentLocationInfoKHR renderingAttachmentLocations
	{
		VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_LOCATION_INFO_KHR,
		DE_NULL,
		colorAttachmentCount,
		colorAttachmentLocations
	};
	VkPipelineRenderingCreateInfo renderingCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
		&renderingAttachmentLocations,
		0u,
		colorAttachmentCount,
		colorImageFormats.data(),
		VK_FORMAT_UNDEFINED,
		VK_FORMAT_UNDEFINED
	};

	Move<VkPipeline> graphicsPipeline = makeGraphicsPipeline(vk, device, *pipelineLayout, *vertShaderModule, DE_NULL, DE_NULL, DE_NULL,
															 *fragShaderModule, DE_NULL, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
															 0, 0, &vertexInputState, DE_NULL, DE_NULL, DE_NULL, &colorBlendStateCreateInfo, DE_NULL,
															 &renderingCreateInfo);

	VkRenderingInfo renderingInfo
	{
		VK_STRUCTURE_TYPE_RENDERING_INFO,
		DE_NULL,
		0u,											// VkRenderingFlags						flags
		scissors[0],								// VkRect2D								renderArea
		1u,											// deUint32								layerCount
		0u,											// deUint32								viewMask
		colorAttachmentCount,						// deUint32								colorAttachmentCount
		colorAttachments.data(),					// const VkRenderingAttachmentInfo*		pColorAttachments
		DE_NULL,									// const VkRenderingAttachmentInfo*		pDepthAttachment
		DE_NULL										// const VkRenderingAttachmentInfo*		pStencilAttachment
	};

	// record primary command buffer
	beginCommandBuffer(vk, cmdBuffer);

	// transfer layout to color attachment optimal
	vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_NONE, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0u, 0u, 0u, 0u, colorAttachmentCount, colorImageBarriers.data());

	vk.cmdBeginRendering(cmdBuffer, &renderingInfo);

	// remaping should affect to which attachments shader writes but not blend state
	vk.cmdSetRenderingAttachmentLocationsKHR(cmdBuffer, &renderingAttachmentLocations);

	vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);
	vk.cmdDraw(cmdBuffer, 4u, 1u, 0u, 0u);

	vk.cmdEndRendering(cmdBuffer);

	// transition colorbuffers layout to transfer source optimal
	for (deUint32 i = 0; i < colorAttachmentCount; ++i)
	{
		colorImageBarriers[i].srcAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		colorImageBarriers[i].dstAccessMask	= VK_ACCESS_TRANSFER_READ_BIT;
		colorImageBarriers[i].oldLayout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorImageBarriers[i].newLayout		= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	}
	vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, 0u, 0u, 0u, colorAttachmentCount, colorImageBarriers.data());

	// read back color images
	for (deUint32 i = 0; i < colorAttachmentCount; ++i)
		vk.cmdCopyImageToBuffer(cmdBuffer, **images[i], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **outputBuffers[i], 1u, &copyRegion);

	endCommandBuffer(vk, cmdBuffer);
	submitCommandsAndWait(vk, device, queue, cmdBuffer);

	const tcu::Vec4 expectedLeftSideColors[]
	{
		{ 0.50f, 0.98f, 0.72f, 1.00f },
		{ 0.42f, 0.44f, 0.63f, 0.17f },
		{ 0.10f, 0.30f, 0.50f, 0.80f },
		{ 1.00f, 0.40f, 0.30f, 0.50f },
	};
	DE_ASSERT(DE_LENGTH_OF_ARRAY(expectedLeftSideColors) == colorAttachmentCount);

	bool				testPassed		(true);
	const tcu::Vec4		colorPrecision	(0.05f);
	auto&				log				(m_context.getTestContext().getLog());

	// verify result
	for (deUint32 i = 0; i < colorAttachmentCount; ++i)
	{
		bool	colorIsCorrect	= true;
		auto&	allocation		= outputBuffers[i]->getAllocation();
		invalidateAlloc(vk, device, allocation);

		tcu::ConstPixelBufferAccess outputAccess(textureFormat, m_renderSize, m_renderSize, 1u, allocation.getHostPtr());

		// check one fragment from the left side of image (after blending)
		tcu::Vec4 fragmentColorOnLeftSide = outputAccess.getPixel(i, i, 0);
		if (tcu::boolAny(tcu::greaterThan(tcu::absDiff(fragmentColorOnLeftSide, expectedLeftSideColors[i]), colorPrecision)))
			colorIsCorrect = false;

		// check one fragment from the right side of image (clear color should be there)
		tcu::Vec4 fragmentColorOnRightSide(outputAccess.getPixel(m_renderSize - 1 - i, m_renderSize - 1 - i, 0));
		if (tcu::boolAny(tcu::greaterThan(tcu::absDiff(fragmentColorOnRightSide, clearValues[i]), colorPrecision)))
			colorIsCorrect = false;

		if (!colorIsCorrect)
		{
			std::string imageName = std::string("Attachment") + std::to_string(i);
			log << tcu::TestLog::Image(imageName, imageName, outputAccess);
			testPassed = false;
		}
	}

	if (testPassed)
		return tcu::TestStatus::pass("Pass");

	return tcu::TestStatus::fail("Fail");
}

class LocalReadTestCase : public vkt::TestCase
{
public:
					LocalReadTestCase	(tcu::TestContext&		context,
										 const std::string&		name,
										 TestType				testType);
	virtual			~LocalReadTestCase	(void) = default;

protected:
	void			checkSupport		(Context&				context) const override;
	void			initPrograms		(SourceCollections&		programCollection) const override;
	TestInstance*	createInstance		(Context&				context) const override;

	TestType		m_testType;
};

LocalReadTestCase::LocalReadTestCase(tcu::TestContext&		context,
									 const std::string&		name,
									 TestType				testType)
	: vkt::TestCase		(context, name)
	, m_testType		(testType)
{
}

void LocalReadTestCase::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_dynamic_rendering_local_read");
}

void LocalReadTestCase::initPrograms (SourceCollections& programCollection) const
{
	// vertex shader generates fullscreen quad
	std::string vertSrc(
		"#version 450\n"
		"void main (void)\n"
		"{\n"
		"  const float x = (-1.0+2.0*((gl_VertexIndex & 2)>>1));\n"
		"  const float y = ( 1.0-2.0* (gl_VertexIndex % 2));\n"
		"  gl_Position = vec4(x, y, 0.6, 1.0);\n"
		"}\n");
	auto& glslSources = programCollection.glslSources;
	glslSources.add("vert") << glu::VertexSource(vertSrc);

	// helper lambda that generates fragment shader that writes to specified number of color attachments
	auto generateWriteFragSource = [](deUint32 colorAttachmentCount)
	{
		std::stringstream fragSrc;
		fragSrc << "#version 450\n"
				   "layout(push_constant) uniform InputDraw {\n"
				   "  uint count;\n"
				   "} inputDraw;\n";
		for (deUint32 i = 0; i < colorAttachmentCount; ++i)
			fragSrc << "layout(location=" << i << ") out uint outColor" << i << ";\n";
		fragSrc << "void main()\n{\n";
		for (deUint32 i = 0; i < colorAttachmentCount; ++i)
			fragSrc << "  outColor" << i << " = 2u * inputDraw.count + " << i + 1 << ";\n";
		fragSrc << "}\n";
		return fragSrc.str();
	};

	// helper lambda that generates fragment shader that reads from specified number of input attachments
	auto generateReadFragSource = [](deUint32 inputAttachmentCount, bool useStencil = true)
	{
		deUint32 colorInputAttachmentCount = inputAttachmentCount - 1u - useStencil;
		std::stringstream fragSrc;
		fragSrc << "#version 450\n";
		for (deUint32 i = 0; i < colorInputAttachmentCount; ++i)
			fragSrc << "layout(input_attachment_index=" << i << ", binding=" << i << ") uniform usubpassInput inColor" << i << ";\n";

		fragSrc << "layout(input_attachment_index = " << colorInputAttachmentCount << ", binding = " << colorInputAttachmentCount << ") uniform subpassInput inDepth;\n";
		if (useStencil)
			fragSrc << "layout(input_attachment_index = " << colorInputAttachmentCount + 1 << ", binding = " << colorInputAttachmentCount + 1 << ") uniform usubpassInput inStencil;\n";

		fragSrc << "layout(set=1, binding=0, std430) writeonly buffer Output\n{\n"
			"  uint v[];\n"
			"} outBuffer;\n"
			"void main()\n{\n"
			"  uint result = 0;\n";
		for (deUint32 i = 0; i < colorInputAttachmentCount; ++i)
			fragSrc << "  result = result + " << i + 1 << " * subpassLoad(inColor" << i << ").x; \n";
		fragSrc << "  result = result + uint(subpassLoad(inDepth).x * 1000);\n";			// 0.6*1000
		if (useStencil)
			fragSrc << "  result = result + uint(subpassLoad(inStencil).x * 1000);\n";		// 1 * 1000
		fragSrc << "  const uvec2 i = uvec2(trunc(gl_FragCoord.xy));\n"
			"  outBuffer.v[i.x+i.y*16] = result;\n"
			"}\n";
		return fragSrc.str();
	};

	if (m_testType == TestType::MAX_INPUT_ATTACHMENTS)
	{
		// generate fragment shader for maximal number of input attachments;
		// during test execution proper shader will be picked
		for (deUint32 inputAttachmentCount : inputAttachmentsPossibleValues)
		{
			// subtract 2 because we will write to depth and stencil and those
			// attachments will later consume 2 input attachment slots
			deUint32 colorAttachmentCount = inputAttachmentCount - 2u;
			glslSources.add(std::string("frag0_") + std::to_string(colorAttachmentCount)) << glu::FragmentSource(generateWriteFragSource(colorAttachmentCount));
		}
	}
	else if (m_testType == TestType::MAX_ATTACHMENTS_REMAPPED_REPEATEDLY)
	{
		glslSources.add("frag0") << glu::FragmentSource(generateWriteFragSource(2));
	}
	else if (m_testType == TestType::UNUSED_WRITEN_DISCARDED)
	{
		glslSources.add("frag0") << glu::FragmentSource(generateWriteFragSource(4));
		glslSources.add("frag1") << glu::FragmentSource(generateReadFragSource(6));
	}
	else if (m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_NO_INDEX)
	{
		glslSources.add("frag0") << glu::FragmentSource(generateWriteFragSource(2));

		// If depthInputAttachmentIndex and stencilInputAttachmentIndex are set to VK_ATTACHMENT_UNUSED
		// it means that these are only accessible in the shader if the shader does not associate these
		// input attachments with an InputAttachmentIndex.

		// NOTE at the memoment glslang doesn't support input attachments without
		// input_attachment_index qualifiers

		//std::string fragSrc(
		//	"#version 450\n"
		//	"layout(input_attachment_index=0, binding=0) uniform usubpassInput inColor0;\n"
		//	"layout(input_attachment_index=1, binding=1) uniform usubpassInput inColor1;\n"
		//	"layout(binding=2) uniform  subpassInput inDepth;\n"
		//	"layout(binding=3) uniform usubpassInput inStencil;\n"
		//	"layout(set=1, binding=0, std430) writeonly buffer Output\n{\n"
		//	"  uint v[];\n"
		//	"} outBuffer;\n"
		//	"void main()\n{\n"
		//	"  uint result = subpassLoad(inColor0).x + subpassLoad(inColor1).x;\n"	// 1 + 2
		//	"  result = result + uint(subpassLoad(inDepth).x * 1000);\n"			// 0.6*1000
		//	"  result = result + uint(subpassLoad(inStencil).x * 100);\n"			// 1*100
		//	"  const uvec2 i = uvec2(trunc(gl_FragCoord.xy));\n"
		//	"  outBuffer.v[i.x+i.y*16] = result;\n"
		//	"}\n");
		//glslSources.add("frag1") << glu::FragmentSource(fragSrc);

		programCollection.spirvAsmSources.add("frag1") <<
			"OpCapability Shader\n"
			"OpCapability InputAttachment\n"
			"%1 = OpExtInstImport \"GLSL.std.450\";\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint Fragment %4 \"main\" %51\n"
			"OpExecutionMode %4 OriginUpperLeft\n"
			"OpDecorate %11 DescriptorSet 0\n"
			"OpDecorate %11 Binding 0\n"
			"OpDecorate %11 InputAttachmentIndex 0\n"
			"OpDecorate %21 DescriptorSet 0\n"
			"OpDecorate %21 Binding 1\n"
			"OpDecorate %21 InputAttachmentIndex 1\n"
			"OpDecorate %30 DescriptorSet 0\n"
			"OpDecorate %30 Binding 2\n"
			//"OpDecorate %30 InputAttachmentIndex 2\n"
			"OpDecorate %40 DescriptorSet 0\n"
			"OpDecorate %40 Binding 3\n"
			//"OpDecorate %40 InputAttachmentIndex 3\n"
			"OpDecorate %51 BuiltIn FragCoord\n"
			"OpDecorate %57 ArrayStride 4\n"
			"OpMemberDecorate %58 0 NonReadable\n"
			"OpMemberDecorate %58 0 Offset 0\n"
			"OpDecorate %58 BufferBlock\n"
			"OpDecorate %60 DescriptorSet 1\n"
			"OpDecorate %60 Binding 0\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeInt 32 0\n"
			"%7 = OpTypePointer Function %6\n"
			"%9 = OpTypeImage %6 SubpassData 0 0 0 2 Unknown\n"
			"%10 = OpTypePointer UniformConstant %9\n"
			"%11 = OpVariable %10 UniformConstant\n"
			"%13 = OpTypeInt 32 1\n"
			"%14 = OpConstant %13 0\n"
			"%15 = OpTypeVector %13 2\n"
			"%16 = OpConstantComposite %15 %14 %14\n"
			"%17 = OpTypeVector %6 4\n"
			"%19 = OpConstant %6 0\n"
			"%21 = OpVariable %10 UniformConstant\n"
			"%27 = OpTypeFloat 32\n"
			"%28 = OpTypeImage %27 SubpassData 0 0 0 2 Unknown\n"
			"%29 = OpTypePointer UniformConstant %28\n"
			"%30 = OpVariable %29 UniformConstant\n"
			"%32 = OpTypeVector %27 4\n"
			"%35 = OpConstant %27 1000\n"
			"%40 = OpVariable %10 UniformConstant\n"
			"%44 = OpConstant %6 100\n"
			"%47 = OpTypeVector %6 2\n"
			"%48 = OpTypePointer Function %47\n"
			"%50 = OpTypePointer Input %32\n"
			"%51 = OpVariable %50 Input\n"
			"%52 = OpTypeVector %27 2\n"
			"%57 = OpTypeRuntimeArray %6\n"
			"%58 = OpTypeStruct %57\n"
			"%59 = OpTypePointer Uniform %58\n"
			"%60 = OpVariable %59 Uniform\n"
			"%63 = OpConstant %6 1\n"
			"%66 = OpConstant %6 16\n"
			"%70 = OpTypePointer Uniform %6\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%8 = OpVariable %7 Function\n"
			"%49 = OpVariable %48 Function\n"
			"%12 = OpLoad %9 %11\n"
			"%18 = OpImageRead %17 %12 %16\n"
			"%20 = OpCompositeExtract %6 %18 0\n"
			"%22 = OpLoad %9 %21\n"
			"%23 = OpImageRead %17 %22 %16\n"
			"%24 = OpCompositeExtract %6 %23 0\n"
			"%25 = OpIAdd %6 %20 %24\n"
			"OpStore %8 %25\n"
			"%26 = OpLoad %6 %8\n"
			"%31 = OpLoad %28 %30\n"
			"%33 = OpImageRead %32 %31 %16\n"
			"%34 = OpCompositeExtract %27 %33 0\n"
			"%36 = OpFMul %27 %34 %35\n"
			"%37 = OpConvertFToU %6 %36\n"
			"%38 = OpIAdd %6 %26 %37\n"
			"OpStore %8 %38\n"
			"%39 = OpLoad %6 %8\n"
			"%41 = OpLoad %9 %40\n"
			"%42 = OpImageRead %17 %41 %16\n"
			"%43 = OpCompositeExtract %6 %42 0\n"
			"%45 = OpIMul %6 %43 %44\n"
			"%46 = OpIAdd %6 %39 %45\n"
			"OpStore %8 %46\n"
			"%53 = OpLoad %32 %51\n"
			"%54 = OpVectorShuffle %52 %53 %53 0 1\n"
			"%55 = OpExtInst %52 %1 Trunc %54\n"
			"%56 = OpConvertFToU %47 %55\n"
			"OpStore %49 %56\n"
			"%61 = OpAccessChain %7 %49 %19\n"
			"%62 = OpLoad %6 %61\n"
			"%64 = OpAccessChain %7 %49 %63\n"
			"%65 = OpLoad %6 %64\n"
			"%67 = OpIMul %6 %65 %66\n"
			"%68 = OpIAdd %6 %62 %67\n"
			"%69 = OpLoad %6 %8\n"
			"%71 = OpAccessChain %70 %60 %14 %68\n"
			"OpStore %71 %69\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
	}
	else if (m_testType == TestType::DEPTH_MAPPING_STENCIL_NOT)
	{
		glslSources.add("frag0") << glu::FragmentSource(generateWriteFragSource(4));
		glslSources.add("frag1") << glu::FragmentSource(generateReadFragSource(5, false));
	}
	else if (m_testType == TestType::DEPTH_STENCIL_MAPPING_TO_SAME_INDEX)
	{
		std::string fragSrc(
			"#version 450\n"
			"layout(input_attachment_index = 0, binding = 0) uniform usubpassInput inColor0;\n"
			"layout(input_attachment_index = 1, binding = 1) uniform usubpassInput inColor1;\n"
			"layout(input_attachment_index = 2, binding = 2) uniform  subpassInput inDepth;\n"
			"layout(input_attachment_index = 2, binding = 3) uniform usubpassInput inStencil;\n"
			"layout(set=1, binding=0, std430) writeonly buffer Output\n{\n"
			"  uint v[];\n"
			"} outBuffer;\n"
			"void main()\n{\n"
			"  const uvec2 i = uvec2(trunc(gl_FragCoord.xy));\n"
			"  outBuffer.v[i.x+i.y*16] = uint(subpassLoad(inDepth).x * 1000) + subpassLoad(inStencil).x;\n"
			"}\n");
		glslSources.add("frag0") << glu::FragmentSource(generateWriteFragSource(2));
		glslSources.add("frag1") << glu::FragmentSource(fragSrc);
	}

	if ((m_testType == TestType::MAX_INPUT_ATTACHMENTS) || (m_testType == TestType::MAX_ATTACHMENTS_REMAPPED_REPEATEDLY))
	{
		// generate fragment shaders for all posible number of input attachments;
		// during test execution proper shader will be picked
		for (deUint32 inputAttachmentCount : inputAttachmentsPossibleValues)
			glslSources.add(std::string("frag1_") + std::to_string(inputAttachmentCount)) << glu::FragmentSource(generateReadFragSource(inputAttachmentCount));
	}

	if (m_testType == TestType::MAPPING_NOT_AFFECTING_BLEND_STATE)
	{
		std::string fragSrc(
			"#version 450\n"
			"layout(location = 0) out vec4 outColor0;\n"
			"layout(location = 1) out vec4 outColor1;\n"
			"layout(location = 2) out vec4 outColor2;\n"
			"layout(location = 3) out vec4 outColor3;\n"
			"void main()\n{\n"
			"  if (gl_FragCoord.x > 8.0)\n"
			"    discard;\n"
			"  outColor0 = vec4(0.6, 0.8, 0.9, 0.2);\n"		// used for attachment 1
			"  outColor1 = vec4(0.6, 0.4, 0.2, 0.6);\n"		// used for attachment 3
			"  outColor2 = vec4(0.1, 0.3, 0.6, 0.2);\n"		// used for attachment 2
			"  outColor3 = vec4(0.0, 0.6, 0.4, 0.8);\n"		// used for attachment 0
			"}\n");
		glslSources.add("frag") << glu::FragmentSource(fragSrc);
	}
}

TestInstance* LocalReadTestCase::createInstance (Context& context) const
{
	if (m_testType == TestType::MAPPING_NOT_AFFECTING_BLEND_STATE)
		return new MappingWithBlendStateTestInstance(context, m_testType);

	return new BasicLocalReadTestInstance(context, m_testType);
}

}	// anonymous

tcu::TestCaseGroup* createDynamicRenderingLocalReadTests(tcu::TestContext& testCtx)
{
	struct TestConfig
	{
		std::string		name;
		TestType		testType;
	};
	std::vector<TestConfig> testConfigs
	{
		{ "max_input_attachments",									TestType::MAX_INPUT_ATTACHMENTS },
		{ "max_attachments_remapped_repeatedly",					TestType::MAX_ATTACHMENTS_REMAPPED_REPEATEDLY },
		{ "unused_writen_discarded",								TestType::UNUSED_WRITEN_DISCARDED },
		{ "depth_stencil_mapping_to_no_index",						TestType::DEPTH_STENCIL_MAPPING_TO_NO_INDEX },
		{ "depth_stencil_mapping_to_same_index",					TestType::DEPTH_STENCIL_MAPPING_TO_SAME_INDEX },
		{ "depth_mapping_stencil_not",								TestType::DEPTH_MAPPING_STENCIL_NOT },
		{ "mapping_not_affecting_blend_state",						TestType::MAPPING_NOT_AFFECTING_BLEND_STATE },
	};

	de::MovePtr<tcu::TestCaseGroup> mainGroup (new tcu::TestCaseGroup(testCtx, "local_read", "Test dynamic rendering local read"));

	for (const auto& testConfig : testConfigs)
		mainGroup->addChild(new LocalReadTestCase(testCtx, testConfig.name, testConfig.testType));

	return mainGroup.release();
}

} // renderpass
} // vkt
