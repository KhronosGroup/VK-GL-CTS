/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 Google Inc.
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
 * \brief Pipeline tests
 *//*--------------------------------------------------------------------*/

#include "vktApiPipelineTests.hpp"
#include "vktTestCaseUtil.hpp"

#include "deUniquePtr.hpp"
#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "tcuTestLog.hpp"

#include <vector>
#include <sstream>

namespace vkt
{
namespace api
{

namespace
{

using namespace std;
using namespace vk;

VkFormat getRenderTargetFormat (const InstanceInterface& vk, const VkPhysicalDevice& device)
{
	const VkFormatFeatureFlags	featureFlags		= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
	VkFormatProperties			formatProperties;

	vk.getPhysicalDeviceFormatProperties(device, VK_FORMAT_B8G8R8A8_UNORM, &formatProperties);

	if ((formatProperties.linearTilingFeatures & featureFlags) || (formatProperties.optimalTilingFeatures & featureFlags))
		return VK_FORMAT_B8G8R8A8_UNORM;

	vk.getPhysicalDeviceFormatProperties(device, VK_FORMAT_R8G8B8A8_UNORM, &formatProperties);

	if ((formatProperties.linearTilingFeatures & featureFlags) || formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
		return VK_FORMAT_R8G8B8A8_UNORM;

	TCU_THROW(NotSupportedError, "Device does not support VK_FORMAT_B8G8R8A8_UNORM nor VK_FORMAT_R8G8B8A8_UNORM");

	return VK_FORMAT_UNDEFINED;
}

Move<VkCommandBuffer> createCommandBuffer (const DeviceInterface& vkd, VkDevice device, VkCommandPool commandPool)
{
	const VkCommandBufferAllocateInfo allocateInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType         sType;
		DE_NULL,										// const void*             pNext;
		commandPool,									// VkCommandPool           commandPool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCommandBufferLevel    level;
		1u												// deUint32                commandBufferCount;
	};

	return allocateCommandBuffer(vkd, device, &allocateInfo);
}

enum DrawTriangleMode
{
	DTM_DESTROY_RENDER_PASS_AFTER_CREATING_PIPELINE = 0,
	DTM_DESTROY_PIPELINE_LAYOUT_AFTER_CREATING_PIPELINE,
};

tcu::TestStatus drawTriangleTest (Context& context, DrawTriangleMode mode)
{
	const DeviceInterface&							vk								= context.getDeviceInterface();
	const InstanceInterface&						vki								= context.getInstanceInterface();
	const VkDevice									device							= context.getDevice();
	const VkPhysicalDevice							physicalDevice					= context.getPhysicalDevice();

	const VkFormat									format							= getRenderTargetFormat(vki, physicalDevice);
	const VkFormatProperties						formatProperties				(getPhysicalDeviceFormatProperties(vki, physicalDevice, format));
	const VkImageTiling								imageTiling						= (formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) ? VK_IMAGE_TILING_LINEAR
																					: (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) ? VK_IMAGE_TILING_OPTIMAL
																					: VK_CORE_IMAGE_TILING_LAST;

	const VkImageCreateInfo							attachmentImageCreateInfo		=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType          sType;
		DE_NULL,								// const void*              pNext;
		(VkImageCreateFlags)0u,					// VkImageCreateFlags       flags;
		VK_IMAGE_TYPE_2D,						// VkImageType              imageType;
		format,									// VkFormat                 format;
		{
			256u,	// deUint32    width;
			256u,	// deUint32    height;
			1u		// deUint32    depth;
		},										// VkExtent3D               extent;
		1u,										// deUint32                 mipLevels;
		1u,										// deUint32                 arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits    samples;
		imageTiling,							// VkImageTiling            tiling;
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT,		// VkImageUsageFlags        usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode            sharingMode;
		0u,										// deUint32                 queueFamilyIndexCount;
		DE_NULL,								// const deUint32*          pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout            initialLayout;
	};

	const Unique<VkImage>							attachmentImage					(createImage(vk, device, &attachmentImageCreateInfo));
	de::MovePtr<Allocation>							attachmentImageMemory			= context.getDefaultAllocator().allocate(getImageMemoryRequirements(vk, device, *attachmentImage), MemoryRequirement::Any);

	VK_CHECK(vk.bindImageMemory(device, *attachmentImage, attachmentImageMemory->getMemory(), attachmentImageMemory->getOffset()));

	const tcu::TextureFormat						resultFormat					= mapVkFormat(format);
	const VkDeviceSize								imageSizeBytes					= (VkDeviceSize)(resultFormat.getPixelSize() * 256 * 256);
	const VkBufferCreateInfo						readImageBufferParams
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		0u,											// VkBufferCreateFlags	flags;
		imageSizeBytes,								// VkDeviceSize			size;
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,			// VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
		0u,											// deUint32				queueFamilyCount;
		DE_NULL,									// const deUint32*		pQueueFamilyIndices;
	};
	const Unique<VkBuffer>				readImageBuffer(createBuffer(vk, device, &readImageBufferParams));
	const de::UniquePtr<Allocation>		readImageBufferMemory(context.getDefaultAllocator().allocate(getBufferMemoryRequirements(vk, device, *readImageBuffer), MemoryRequirement::HostVisible));

	VK_CHECK(vk.bindBufferMemory(device, *readImageBuffer, readImageBufferMemory->getMemory(), readImageBufferMemory->getOffset()));

	const deUint32									queueFamilyIndex				= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo					commandPoolParams				=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,		// VkStructureType             sType;
		DE_NULL,										// const void*                 pNext;
		(VkCommandPoolCreateFlags)0u,					// VkCommandPoolCreateFlags    flags;
		queueFamilyIndex								// deUint32                    queueFamilyIndex;
	};

	const Unique<VkCommandPool>						commandPool						(createCommandPool(vk, device, &commandPoolParams, DE_NULL));
	const Unique<VkCommandBuffer>					commandBuffer					(createCommandBuffer(vk, device, commandPool.get()));

	const VkCommandBufferBeginInfo					commandBufferBeginInfo			=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType                          sType;
		DE_NULL,										// const void*                              pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,	// VkCommandBufferUsageFlags                flags;
		DE_NULL											// const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
	};

	VK_CHECK(vk.beginCommandBuffer(commandBuffer.get(), &commandBufferBeginInfo));

	const VkAttachmentDescription					attachmentDescription			=
	{
		(VkAttachmentDescriptionFlags)0u,			// VkAttachmentDescriptionFlags    flags;
		format,										// VkFormat                        format;
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits           samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp              loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp             storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp              stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp             stencilStoreOp;
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout                   initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout                   finalLayout;
	};

	const VkAttachmentReference						attachmentReference				=
	{
		0u,											// deUint32			attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout;
	};

	const VkSubpassDescription						subpassDescription				=
	{
		(VkSubpassDescriptionFlags)0u,		// VkSubpassDescriptionFlags       flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint             pipelineBindPoint
		0u,									// deUint32                        inputAttachmentCount
		DE_NULL,							// const VkAttachmentReference*    pInputAttachments
		1u,									// deUint32                        colorAttachmentCount
		&attachmentReference,				// const VkAttachmentReference*    pColorAttachments
		DE_NULL,							// const VkAttachmentReference*    pResolveAttachments
		DE_NULL,							// const VkAttachmentReference*    pDepthStencilAttachment
		0u,									// deUint32                        preserveAttachmentCount
		DE_NULL								// const deUint32*                 pPreserveAttachments
	};

	const VkRenderPassCreateInfo					renderPassCreateInfo			=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,		// VkStructureType                   sType;
		DE_NULL,										// const void*                       pNext;
		(VkRenderPassCreateFlags)0u,					// VkRenderPassCreateFlags           flags;
		1u,												// deUint32                          attachmentCount
		&attachmentDescription,							// const VkAttachmentDescription*    pAttachments
		1u,												// deUint32                          subpassCount
		&subpassDescription,							// const VkSubpassDescription*       pSubpasses
		0u,												// deUint32                          dependencyCount
		DE_NULL											// const VkSubpassDependency*        pDependencies
	};

	// When testing renderpass lifetime - create two compatible renderpasses, otherwise use just renderPassB
	VkRenderPass renderPassA;
	if (DTM_DESTROY_RENDER_PASS_AFTER_CREATING_PIPELINE == mode)
		VK_CHECK(vk.createRenderPass(device, &renderPassCreateInfo, DE_NULL, &renderPassA));

	const Move<VkRenderPass>						renderPassB						(createRenderPass(vk, device, &renderPassCreateInfo));

	const VkImageViewCreateInfo						attachmentImageViewCreateInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType            sType;
		DE_NULL,										// const void*                pNext;
		(VkImageViewCreateFlags)0u,						// VkImageViewCreateFlags     flags;
		attachmentImage.get(),							// VkImage                    image;
		VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType            viewType;
		format,											// VkFormat                   format;
		{
			VK_COMPONENT_SWIZZLE_R,			// VkComponentSwizzle    r;
			VK_COMPONENT_SWIZZLE_G,			// VkComponentSwizzle    g;
			VK_COMPONENT_SWIZZLE_B,			// VkComponentSwizzle    b;
			VK_COMPONENT_SWIZZLE_A			// VkComponentSwizzle    a;
		},											// VkComponentMapping         components;
		{
			VK_IMAGE_ASPECT_COLOR_BIT,		// VkImageAspectFlags    aspectMask;
			0u,								// deUint32              baseMipLevel;
			1u,								// deUint32              levelCount;
			0u,								// deUint32              baseArrayLayer;
			1u								// deUint32              layerCount;
		}											// VkImageSubresourceRange    subresourceRange;
	};

	const Unique<VkImageView>						attachmentImageView				(createImageView(vk, device, &attachmentImageViewCreateInfo));

	const VkFramebufferCreateInfo					framebufferCreateInfo			=
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,		// VkStructureType             sType;
		DE_NULL,										// const void*                 pNext;
		(VkFramebufferCreateFlags)0u,					// VkFramebufferCreateFlags    flags;
		renderPassB.get(),								// VkRenderPass                renderPass;
		1u,												// deUint32                    attachmentCount;
		&attachmentImageView.get(),						// const VkImageView*          pAttachments;
		256u,											// deUint32                    width;
		256u,											// deUint32                    height;
		1u												// deUint32                    layers;
	};

	const Unique<VkFramebuffer>						frameBuffer						(createFramebuffer(vk, device, &framebufferCreateInfo));

	const Unique<VkShaderModule>					vertexShaderModule				(createShaderModule(vk, device, context.getBinaryCollection().get("vertex"), 0));
	const Unique<VkShaderModule>					fragmentShaderModule			(createShaderModule(vk, device, context.getBinaryCollection().get("fragment"), 0));

	const VkPipelineLayoutCreateInfo				pipelineLayoutCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType                     sType;
		DE_NULL,											// const void*                         pNext;
		(VkPipelineLayoutCreateFlags)0u,						// VkPipelineLayoutCreateFlags         flags;
		0u,													// deUint32                            setLayoutCount;
		DE_NULL,											// const VkDescriptorSetLayout*        pSetLayouts;
		0u,													// deUint32                            pushConstantRangeCount;
		DE_NULL												// const VkPushConstantRange*          pPushConstantRanges;
	};

	Move<VkPipelineLayout>							pipelineLayout					(createPipelineLayout(vk, device, &pipelineLayoutCreateInfo));

	const VkPipelineVertexInputStateCreateInfo		vertexInputStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType                             sType;
		DE_NULL,														// const void*                                 pNext;
		(VkPipelineVertexInputStateCreateFlags)0u,						// VkPipelineVertexInputStateCreateFlags       flags;
		0u,																// deUint32                                    vertexBindingDescriptionCount;
		DE_NULL,														// const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
		0u,																// deUint32                                    vertexAttributeDescriptionCount;
		DE_NULL															// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
	};

	const VkViewport								viewport						=
	{
		0.0f,	// float    x;
		0.0f,	// float    y;
		64.0f,	// float    width;
		64.0f,	// float    height;
		0.0f,	// float    minDepth;
		0.0f,	// float    maxDepth;
	};

	const std::vector<VkViewport>					viewports						(1, viewport);
	const std::vector<VkRect2D>						scissors						(1, makeRect2D(0, 0, 64u, 64u));

	const VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType                            sType;
		DE_NULL,														// const void*                                pNext;
		(VkPipelineRasterizationStateCreateFlags)0u,					// VkPipelineRasterizationStateCreateFlags    flags;
		VK_FALSE,														// VkBool32                                   depthClampEnable;
		VK_FALSE,														// VkBool32                                   rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,											// VkPolygonMode                              polygonMode;
		VK_CULL_MODE_NONE,												// VkCullModeFlags                            cullMode;
		VK_FRONT_FACE_CLOCKWISE,										// VkFrontFace                                frontFace;
		VK_FALSE,														// VkBool32                                   depthBiasEnable;
		0.0f,															// float                                      depthBiasConstantFactor;
		0.0f,															// float                                      depthBiasClamp;
		0.0f,															// float                                      depthBiasSlopeFactor;
		1.0f															// float                                      lineWidth;
	};

	const VkPipelineColorBlendAttachmentState		colorBlendAttachmentState		=
	{
		VK_FALSE,						// VkBool32                 blendEnable;
		VK_BLEND_FACTOR_ZERO,			// VkBlendFactor            srcColorBlendFactor;
		VK_BLEND_FACTOR_ZERO,			// VkBlendFactor            dstColorBlendFactor;
		VK_BLEND_OP_ADD,				// VkBlendOp                colorBlendOp;
		VK_BLEND_FACTOR_ZERO,			// VkBlendFactor            srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ZERO,			// VkBlendFactor            dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,				// VkBlendOp                alphaBlendOp;
		(VkColorComponentFlags)0xF		// VkColorComponentFlags    colorWriteMask;
	};

	const VkPipelineColorBlendStateCreateInfo		colorBlendStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,		// VkStructureType                               sType;
		DE_NULL,														// const void*                                   pNext;
		(VkPipelineColorBlendStateCreateFlags)0u,						// VkPipelineColorBlendStateCreateFlags          flags;
		VK_FALSE,														// VkBool32                                      logicOpEnable;
		VK_LOGIC_OP_CLEAR,												// VkLogicOp                                     logicOp;
		1u,																// deUint32                                      attachmentCount;
		&colorBlendAttachmentState,										// const VkPipelineColorBlendAttachmentState*    pAttachments;
		{ 1.0f, 1.0f, 1.0f, 1.0f }										// float                                         blendConstants[4];
	};

	VkRenderPass renderPassUsedForPipeline = *renderPassB;
	if (DTM_DESTROY_RENDER_PASS_AFTER_CREATING_PIPELINE == mode)
		renderPassUsedForPipeline = renderPassA;

	const Unique<VkPipeline>						graphicsPipeline				(makeGraphicsPipeline(vk,									// const DeviceInterface&                        vk
																										  device,								// const VkDevice                                device
																										  *pipelineLayout,						// const VkPipelineLayout                        pipelineLayout
																										  *vertexShaderModule,					// const VkShaderModule                          vertexShaderModule
																										  DE_NULL,								// const VkShaderModule                          tessellationControlModule
																										  DE_NULL,								// const VkShaderModule                          tessellationEvalModule
																										  DE_NULL,								// const VkShaderModule                          geometryShaderModule
																										  *fragmentShaderModule,				// const VkShaderModule                          fragmentShaderModule
																										  renderPassUsedForPipeline,			// const VkRenderPass                            renderPass
																										  viewports,							// const std::vector<VkViewport>&                viewports
																										  scissors,								// const std::vector<VkRect2D>&                  scissors
																										  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,	// const VkPrimitiveTopology                     topology
																										  0u,									// const deUint32                                subpass
																										  0u,									// const deUint32                                patchControlPoints
																										  &vertexInputStateCreateInfo,			// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
																										  &rasterizationStateCreateInfo,		// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
																										  DE_NULL,								// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
																										  DE_NULL,								// const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
																										  &colorBlendStateCreateInfo));			// const VkPipelineColorBlendStateCreateInfo*    colorBlendStateCreateInfo

	if (DTM_DESTROY_PIPELINE_LAYOUT_AFTER_CREATING_PIPELINE == mode)
		pipelineLayout = decltype(pipelineLayout)();

	beginRenderPass(vk, *commandBuffer, renderPassB.get(), frameBuffer.get(), makeRect2D(0, 0, 256u, 256u), tcu::Vec4(0.25f, 0.25f, 0.25f, 0.0f));

	vk.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.get());

	// Destroy the renderpass that was used to create the graphics pipeline
	if (DTM_DESTROY_RENDER_PASS_AFTER_CREATING_PIPELINE == mode)
		vk.destroyRenderPass(device, renderPassA, DE_NULL);

	vk.cmdDraw(*commandBuffer, 3u, 1u, 0u, 0u);

	endRenderPass(vk, *commandBuffer);

	// Copy image to buffer
	{
		copyImageToBuffer(vk, *commandBuffer, *attachmentImage, *readImageBuffer, tcu::IVec2(256, 256));
	}

	VK_CHECK(vk.endCommandBuffer(*commandBuffer));

	const VkSubmitInfo								submitInfo						=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,		// VkStructureType                sType;
		DE_NULL,							// const void*                    pNext;
		0u,									// deUint32                       waitSemaphoreCount;
		DE_NULL,							// const VkSemaphore*             pWaitSemaphores;
		DE_NULL,							// const VkPipelineStageFlags*    pWaitDstStageMask;
		1u,									// deUint32                       commandBufferCount;
		&commandBuffer.get(),				// const VkCommandBuffer*         pCommandBuffers;
		0u,									// deUint32                       signalSemaphoreCount;
		DE_NULL								// const VkSemaphore*             pSignalSemaphores;
	};

	vk::VkQueue queue = context.getUniversalQueue();

	VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, DE_NULL));
	VK_CHECK(vk.queueWaitIdle(queue));
	VK_CHECK(vk.deviceWaitIdle(device));

	invalidateAlloc(vk, device, *readImageBufferMemory);
	const tcu::ConstPixelBufferAccess resultAccess(resultFormat, 256, 256, 1, readImageBufferMemory->getHostPtr());

	// check just one pixel
	tcu::Vec4 pixel = resultAccess.getPixel(1, 1);
	if ((pixel.x() < 0.9f) || (pixel.y() > 0.1f) || (pixel.z() > 0.1f) || (pixel.w() < 0.9f))
		return tcu::TestStatus::fail("Fail");

	tcu::TestLog& log = context.getTestContext().getLog();
	log << tcu::TestLog::ImageSet("Image verification", "")
		<< tcu::TestLog::Image("Result", "Result", resultAccess, tcu::Vec4(1.0f), tcu::Vec4(0.0f))
		<< tcu::TestLog::EndImageSet;

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus renderpassLifetimeTest(Context& context)
{
	return drawTriangleTest(context, DTM_DESTROY_RENDER_PASS_AFTER_CREATING_PIPELINE);
}

void createDrawTriangleSource (SourceCollections& dst)
{
	dst.glslSources.add("vertex") << glu::VertexSource(
		"#version 310 es\n"
		"void main (void)\n"
		"{\n"
		"    gl_Position = vec4(float(1 - 2 * int(gl_VertexIndex != 1)),\n"
		"                       float(1 - 2 * int(gl_VertexIndex > 0)), 0.0, 1.0);\n"
		"}\n");

	dst.glslSources.add("fragment") << glu::FragmentSource(
		"#version 310 es\n"
		"layout (location = 0) out highp vec4 color;\n"
		"void main (void)\n"
		"{\n"
		"    color = vec4(1.0, 0.0, 0.0, 1.0);\n"
		"}\n");
}

void changeColorAttachmentImageLayout (const DeviceInterface& vk, VkCommandBuffer commandBuffer, VkImage image)
{
	const VkImageMemoryBarrier		imageMemoryBarrier		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType            sType;
		DE_NULL,									// const void*                pNext;
		(VkAccessFlags)0u,							// VkAccessFlags              srcAccessMask;
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// VkAccessFlags              dstAccessMask;
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout              oldLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout              newLayout;
		0u,											// deUint32                   srcQueueFamilyIndex;
		0u,											// deUint32                   dstQueueFamilyIndex;
		image,										// VkImage                    image;
		{
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags    aspectMask;
			0u,							// deUint32              baseMipLevel;
			1u,							// deUint32              levelCount;
			0u,							// deUint32              baseArrayLayer;(
			1u							// deUint32              layerCount;
		}											// VkImageSubresourceRange    subresourceRange;
	};

	vk.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, (VkDependencyFlagBits)0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &imageMemoryBarrier);
}

Move<VkRenderPass> createSimpleRenderPass (const DeviceInterface& vk, const VkDevice& device, VkFormat format, VkAttachmentLoadOp loadOp, VkAttachmentLoadOp stencilLoadOp, VkAttachmentStoreOp stencilStoreOp, VkImageLayout layout)
{
	const VkAttachmentDescription	attachmentDescription	=
	{
		(VkAttachmentDescriptionFlags)0u,	// VkAttachmentDescriptionFlags    flags;
		format,								// VkFormat                        format;
		VK_SAMPLE_COUNT_1_BIT,				// VkSampleCountFlagBits           samples;
		loadOp,								// VkAttachmentLoadOp              loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,		// VkAttachmentStoreOp             storeOp;
		stencilLoadOp,						// VkAttachmentLoadOp              stencilLoadOp;
		stencilStoreOp,						// VkAttachmentStoreOp             stencilStoreOp;
		VK_IMAGE_LAYOUT_UNDEFINED,			// VkImageLayout                   initialLayout;
		layout								// VkImageLayout                   finalLayout;
	};

	const VkAttachmentReference		attachmentReference		=
	{
		0u,		// deUint32			attachment;
		layout	// VkImageLayout	layout;
	};

	const VkSubpassDescription		subpassDescription		=
	{
		(VkSubpassDescriptionFlags)0u,		// VkSubpassDescriptionFlags       flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint             pipelineBindPoint
		0u,									// deUint32                        inputAttachmentCount
		DE_NULL,							// const VkAttachmentReference*    pInputAttachments
		1u,									// deUint32                        colorAttachmentCount
		&attachmentReference,				// const VkAttachmentReference*    pColorAttachments
		DE_NULL,							// const VkAttachmentReference*    pResolveAttachments
		DE_NULL,							// const VkAttachmentReference*    pDepthStencilAttachment
		0u,									// deUint32                        preserveAttachmentCount
		DE_NULL								// const deUint32*                 pPreserveAttachments
	};

	const VkRenderPassCreateInfo	renderPassCreateInfo	=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType                   sType;
		DE_NULL,									// const void*                       pNext;
		(VkRenderPassCreateFlags)0u,				// VkRenderPassCreateFlags           flags;
		1u,											// deUint32                          attachmentCount
		&attachmentDescription,						// const VkAttachmentDescription*    pAttachments
		1u,											// deUint32                          subpassCount
		&subpassDescription,						// const VkSubpassDescription*       pSubpasses
		0u,											// deUint32                          dependencyCount
		DE_NULL										// const VkSubpassDependency*        pDependencies
	};

	return createRenderPass(vk, device, &renderPassCreateInfo);
}

// This test has the same functionality as VkLayerTest.RenderPassInUseDestroyedSignaled
tcu::TestStatus framebufferCompatibleRenderPassTest (Context& context)
{
	const DeviceInterface&			vk						= context.getDeviceInterface();
	const InstanceInterface&		vki						= context.getInstanceInterface();
	const VkDevice					device					= context.getDevice();
	const VkPhysicalDevice			physicalDevice			= context.getPhysicalDevice();
	const VkQueue					queue					= context.getUniversalQueue();
	const deUint32					queueFamilyIndex		= context.getUniversalQueueFamilyIndex();

	const VkCommandPoolCreateInfo	commandPoolParams		=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,	// VkStructureType             sType;
		DE_NULL,									// const void*                 pNext;
		(VkCommandPoolCreateFlags)0u,				// VkCommandPoolCreateFlags    flags;
		queueFamilyIndex							// deUint32                    queueFamilyIndex;
	};

	const Unique<VkCommandPool>		commandPool				(createCommandPool(vk, device, &commandPoolParams, DE_NULL));
	const Unique<VkCommandBuffer>	commandBuffer			(createCommandBuffer(vk, device, commandPool.get()));

	const VkCommandBufferBeginInfo	commandBufferBeginInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType                          sType;
		DE_NULL,										// const void*                              pNext;
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,	// VkCommandBufferUsageFlags                flags;
		DE_NULL											// const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
	};

	VK_CHECK(vk.beginCommandBuffer(commandBuffer.get(), &commandBufferBeginInfo));

	const VkFormat					format					= getRenderTargetFormat(vki, physicalDevice);
	const VkFormatProperties		formatProperties		(getPhysicalDeviceFormatProperties(vki, physicalDevice, format));
	const VkImageTiling				imageTiling				= (formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) ? VK_IMAGE_TILING_LINEAR
															: (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) ? VK_IMAGE_TILING_OPTIMAL
															: VK_CORE_IMAGE_TILING_LAST;

	const VkImageCreateInfo			imageCreateInfo			=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType          sType;
		DE_NULL,								// const void*              pNext;
		(VkImageCreateFlags)0u,					// VkImageCreateFlags       flags;
		VK_IMAGE_TYPE_2D,						// VkImageType              imageType;
		format,									// VkFormat                 format;
		{
			256u,	// deUint32    width;
			256u,	// deUint32    height;
			1u		// deUint32    depth;
		},										// VkExtent3D               extent;
		1u,										// deUint32                 mipLevels;
		1u,										// deUint32                 arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits    samples;
		imageTiling,							// VkImageTiling            tiling;
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,	// VkImageUsageFlags        usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode            sharingMode;
		0u,										// deUint32                 queueFamilyIndexCount;
		DE_NULL,								// const deUint32*          pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout            initialLayout;
	};

	const Unique<VkImage>			attachmentImage			(createImage(vk, device, &imageCreateInfo));
	de::MovePtr<Allocation>			attachmentImageMemory	= context.getDefaultAllocator().allocate(getImageMemoryRequirements(vk, device, *attachmentImage), MemoryRequirement::Any);

	VK_CHECK(vk.bindImageMemory(device, *attachmentImage, attachmentImageMemory->getMemory(), attachmentImageMemory->getOffset()));

	changeColorAttachmentImageLayout(vk, commandBuffer.get(), attachmentImage.get());

	const VkImageViewCreateInfo		imageViewCreateInfo		=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType            sType;
		DE_NULL,									// const void*                pNext;
		(VkImageViewCreateFlags)0u,					// VkImageViewCreateFlags     flags;
		attachmentImage.get(),						// VkImage                    image;
		VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType            viewType;
		format,										// VkFormat                   format;
		{
			VK_COMPONENT_SWIZZLE_R,	// VkComponentSwizzle    r;
			VK_COMPONENT_SWIZZLE_G,	// VkComponentSwizzle    g;
			VK_COMPONENT_SWIZZLE_B,	// VkComponentSwizzle    b;
			VK_COMPONENT_SWIZZLE_A	// VkComponentSwizzle    a;
		},											// VkComponentMapping         components;
		{
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags    aspectMask;
			0u,							// deUint32              baseMipLevel;
			1u,							// deUint32              levelCount;
			0u,							// deUint32              baseArrayLayer;
			1u							// deUint32              layerCount;
		}											// VkImageSubresourceRange    subresourceRange;
	};

	const Unique<VkImageView>		attachmentImageView		(createImageView(vk, device, &imageViewCreateInfo));

	Unique<VkRenderPass>			renderPassA				(createSimpleRenderPass(vk, device, format,
																					VK_ATTACHMENT_LOAD_OP_CLEAR,
																					VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																					VK_ATTACHMENT_STORE_OP_DONT_CARE,
																					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

	// Create framebuffer using the first render pass
	const VkFramebufferCreateInfo	framebufferCreateInfo	=
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType             sType;
		DE_NULL,									// const void*                 pNext;
		(VkFramebufferCreateFlags)0u,				// VkFramebufferCreateFlags    flags;
		renderPassA.get(),							// VkRenderPass                renderPass;
		1u,											// deUint32                    attachmentCount;
		&attachmentImageView.get(),					// const VkImageView*          pAttachments;
		256u,										// deUint32                    width;
		256u,										// deUint32                    height;
		1u											// deUint32                    layers;
	};

	const Unique<VkFramebuffer>		frameBuffer				(createFramebuffer(vk, device, &framebufferCreateInfo));

	const Unique<VkRenderPass>		renderPassB				(createSimpleRenderPass(vk, device, format,
																					VK_ATTACHMENT_LOAD_OP_DONT_CARE,
																					VK_ATTACHMENT_LOAD_OP_LOAD,
																					VK_ATTACHMENT_STORE_OP_STORE,
																					VK_IMAGE_LAYOUT_GENERAL));

	beginRenderPass(vk, commandBuffer.get(), renderPassB.get(), frameBuffer.get(), makeRect2D(0, 0, 1u, 1u));
	endRenderPass(vk, commandBuffer.get());

	VK_CHECK(vk.endCommandBuffer(commandBuffer.get()));

	const VkSubmitInfo				submitInfo				=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType                sType;
		DE_NULL,						// const void*                    pNext;
		0u,								// deUint32                       waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*             pWaitSemaphores;
		DE_NULL,						// const VkPipelineStageFlags*    pWaitDstStageMask;
		1u,								// deUint32                       commandBufferCount;
		&commandBuffer.get(),			// const VkCommandBuffer*         pCommandBuffers;
		0u,								// deUint32                       signalSemaphoreCount;
		DE_NULL							// const VkSemaphore*             pSignalSemaphores;
	};

	VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, DE_NULL));
	VK_CHECK(vk.queueWaitIdle(queue));

	// Test should always pass
	return tcu::TestStatus::pass("Pass");
}

Move<VkDescriptorSetLayout> getDescriptorSetLayout (const DeviceInterface& vk, const VkDevice& device, deUint32 bindingCount, const VkDescriptorSetLayoutBinding* layoutBindings)
{
	const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,	// VkStructureType                        sType;
		DE_NULL,												// const void*                            pNext;
		(VkDescriptorSetLayoutCreateFlags)0u,					// VkDescriptorSetLayoutCreateFlags       flags;
		bindingCount,											// deUint32                               bindingCount;
		layoutBindings											// const VkDescriptorSetLayoutBinding*    pBindings;
	};

	return createDescriptorSetLayout(vk, device, &descriptorSetLayoutCreateInfo);
}

VkDescriptorSet getDescriptorSet (const DeviceInterface& vk, const VkDevice& device, VkDescriptorPool descriptorPool, VkDescriptorSetLayout setLayout)
{
	const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// VkStructureType                 sType;
		DE_NULL,										// const void*                     pNext;
		descriptorPool,									// VkDescriptorPool                descriptorPool;
		1u,												// deUint32                        descriptorSetCount;
		&setLayout										// const VkDescriptorSetLayout*    pSetLayouts;
	};

	VkDescriptorSet descriptorSet;
	VK_CHECK(vk.allocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));

	return descriptorSet;
}

Move<VkPipelineLayout> getPipelineLayout (const DeviceInterface& vk, const VkDevice& device, deUint32 setLayoutCount, const VkDescriptorSetLayout* setLayouts)
{
	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType                 sType;
		DE_NULL,										// const void*                     pNext;
		(VkPipelineLayoutCreateFlags)0u,				// VkPipelineLayoutCreateFlags     flags;
		setLayoutCount,									// deUint32                        setLayoutCount;
		setLayouts,										// const VkDescriptorSetLayout*    pSetLayouts;
		0u,												// deUint32                        pushConstantRangeCount;
		DE_NULL											// const VkPushConstantRange*      pPushConstantRanges;
	};

	return createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
}

Move<VkPipeline> createSimpleGraphicsPipeline (const DeviceInterface& vk, const VkDevice& device, deUint32 numShaderStages, const VkPipelineShaderStageCreateInfo* shaderStageCreateInfos, VkPipelineLayout pipelineLayout, VkRenderPass renderPass, de::SharedPtr<vk::ResourceInterface> resourceInterface)
{
#ifndef CTS_USES_VULKANSC
	DE_UNREF(resourceInterface);
#endif // CTS_USES_VULKANSC
	const VkPipelineVertexInputStateCreateInfo		vertexInputStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType                             sType;
		DE_NULL,													// const void*                                 pNext;
		(VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags       flags;
		0u,															// deUint32                                    vertexBindingDescriptionCount;
		DE_NULL,													// const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
		0u,															// deUint32                                    vertexAttributeDescriptionCount;
		DE_NULL														// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
	};

	const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType                            sType;
		DE_NULL,														// const void*                                pNext;
		(VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags    flags;
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,							// VkPrimitiveTopology                        topology;
		VK_FALSE														// VkBool32                                   primitiveRestartEnable;
	};

	const VkPipelineViewportStateCreateInfo			viewPortStateCreateInfo			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType                       sType;
		DE_NULL,												// const void*                           pNext;
		(VkPipelineViewportStateCreateFlags)0,					// VkPipelineViewportStateCreateFlags    flags;
		1,														// deUint32                              viewportCount;
		DE_NULL,												// const VkViewport*                     pViewports;
		1,														// deUint32                              scissorCount;
		DE_NULL													// const VkRect2D*                       pScissors;
	};

	const VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// VkStructureType                            sType;
		DE_NULL,													// const void*                                pNext;
		(VkPipelineRasterizationStateCreateFlags)0,					// VkPipelineRasterizationStateCreateFlags    flags;
		VK_FALSE,													// VkBool32                                   depthClampEnable;
		VK_FALSE,													// VkBool32                                   rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,										// VkPolygonMode                              polygonMode;
		VK_CULL_MODE_BACK_BIT,										// VkCullModeFlags                            cullMode;
		VK_FRONT_FACE_CLOCKWISE,									// VkFrontFace                                frontFace;
		VK_FALSE,													// VkBool32                                   depthBiasEnable;
		0.0f,														// float                                      depthBiasConstantFactor;
		0.0f,														// float                                      depthBiasClamp;
		0.0f,														// float                                      depthBiasSlopeFactor;
		1.0f														// float                                      lineWidth;
	};

	const VkPipelineMultisampleStateCreateInfo		multisampleStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType                          sType;
		DE_NULL,													// const void*                              pNext;
		(VkPipelineMultisampleStateCreateFlags)0,					// VkPipelineMultisampleStateCreateFlags    flags;
		VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits                    rasterizationSamples;
		VK_FALSE,													// VkBool32                                 sampleShadingEnable;
		0.0f,														// float                                    minSampleShading;
		DE_NULL,													// const VkSampleMask*                      pSampleMask;
		VK_FALSE,													// VkBool32                                 alphaToCoverageEnable;
		VK_FALSE													// VkBool32                                 alphaToOneEnable;
	};

	const VkPipelineColorBlendAttachmentState		colorBlendAttachmentState		=
	{
		VK_FALSE,					// VkBool32                 blendEnable;
		VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            srcColorBlendFactor;
		VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            dstColorBlendFactor;
		VK_BLEND_OP_ADD,			// VkBlendOp                colorBlendOp;
		VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ZERO,		// VkBlendFactor            dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,			// VkBlendOp                alphaBlendOp;
		(VkColorComponentFlags)0xFu	// VkColorComponentFlags    colorWriteMask;
	};

	const VkPipelineColorBlendStateCreateInfo		colorBlendStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType                               sType;
		DE_NULL,													// const void*                                   pNext;
		(VkPipelineColorBlendStateCreateFlags)0,					// VkPipelineColorBlendStateCreateFlags          flags;
		DE_FALSE,													// VkBool32                                      logicOpEnable;
		VK_LOGIC_OP_CLEAR,											// VkLogicOp                                     logicOp;
		1,															// deUint32                                      attachmentCount;
		&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*    pAttachments;
		{ 1.0f, 1.0f, 1.0f, 1.0f }									// float                                         blendConstants[4];
	};

	const VkDynamicState							dynamicStates[]					=
	{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	const VkPipelineDynamicStateCreateInfo			dynamicStateCreateInfo			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	// VkStructureType                      sType;
		DE_NULL,												// const void*                          pNext;
		(VkPipelineDynamicStateCreateFlags)0u,					// VkPipelineDynamicStateCreateFlags    flags;
		DE_LENGTH_OF_ARRAY(dynamicStates),						// deUint32                             dynamicStateCount;
		dynamicStates											// const VkDynamicState*                pDynamicStates;
	};

	const VkGraphicsPipelineCreateInfo				graphicsPipelineCreateInfo		=
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType                                  sType;
		DE_NULL,											// const void*                                      pNext;
		(VkPipelineCreateFlags)0,							// VkPipelineCreateFlags                            flags;
		numShaderStages,									// deUint32                                         stageCount;
		shaderStageCreateInfos,								// const VkPipelineShaderStageCreateInfo*           pStages;
		&vertexInputStateCreateInfo,						// const VkPipelineVertexInputStateCreateInfo*      pVertexInputState;
		&inputAssemblyStateCreateInfo,						// const VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState;
		DE_NULL,											// const VkPipelineTessellationStateCreateInfo*     pTessellationState;
		&viewPortStateCreateInfo,							// const VkPipelineViewportStateCreateInfo*         pViewportState;
		&rasterizationStateCreateInfo,						// const VkPipelineRasterizationStateCreateInfo*    pRasterizationState;
		&multisampleStateCreateInfo,						// const VkPipelineMultisampleStateCreateInfo*      pMultisampleState;
		DE_NULL,											// const VkPipelineDepthStencilStateCreateInfo*     pDepthStencilState;
		&colorBlendStateCreateInfo,							// const VkPipelineColorBlendStateCreateInfo*       pColorBlendState;
		&dynamicStateCreateInfo,							// const VkPipelineDynamicStateCreateInfo*          pDynamicState;
		pipelineLayout,										// VkPipelineLayout                                 layout;
		renderPass,											// VkRenderPass                                     renderPass;
		0u,													// deUint32                                         subpass;
		DE_NULL,											// VkPipeline                                       basePipelineHandle;
		0													// int                                              basePipelineIndex;
	};

	const VkPipelineCacheCreateInfo					pipelineCacheCreateInfo			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,					// VkStructureType               sType;
		DE_NULL,														// const void*                   pNext;
#ifndef CTS_USES_VULKANSC
		(VkPipelineCacheCreateFlags)0u,									// VkPipelineCacheCreateFlags    flags;
		0,																// size_t                        initialDataSize;
		DE_NULL															// const void*                   pInitialData;
#else
		VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
			VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT,		// VkPipelineCacheCreateFlags    flags;
		resourceInterface->getCacheDataSize(),							// deUintptr                     initialDataSize;
		resourceInterface->getCacheData()								// const void*                   pInitialData;
#endif // CTS_USES_VULKANSC
	};

	const Unique<VkPipelineCache>					pipelineCache					(createPipelineCache(vk, device, &pipelineCacheCreateInfo));

	return createGraphicsPipeline(vk, device, pipelineCache.get(), &graphicsPipelineCreateInfo);
}

tcu::TestStatus pipelineLayoutLifetimeTest (Context& context, VkPipelineBindPoint bindPoint)
{
	const DeviceInterface&					vk							= context.getDeviceInterface();
	const InstanceInterface&				vki							= context.getInstanceInterface();
	const VkDevice							device						= context.getDevice();
	const VkPhysicalDevice					physicalDevice				= context.getPhysicalDevice();
	const deUint32							queueFamilyIndex			= context.getUniversalQueueFamilyIndex();
	const bool								isGraphics					= (bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS);

	const VkCommandPoolCreateInfo			commandPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,	// VkStructureType             sType;
		DE_NULL,									// const void*                 pNext;
		(VkCommandPoolCreateFlags)0u,				// VkCommandPoolCreateFlags    flags;
		queueFamilyIndex							// deUint32                    queueFamilyIndex;
	};

	const Unique<VkCommandPool>				commandPool					(createCommandPool(vk, device, &commandPoolParams, DE_NULL));
	const Unique<VkCommandBuffer>			commandBuffer				(createCommandBuffer(vk, device, commandPool.get()));

	// Begin command buffer.
	{
		const VkCommandBufferBeginInfo commandBufferBeginInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType                          sType;
			DE_NULL,										// const void*                              pNext;
			VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,	// VkCommandBufferUsageFlags                flags;
			DE_NULL											// const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
		};

		VK_CHECK(vk.beginCommandBuffer(commandBuffer.get(), &commandBufferBeginInfo));
	}

	// These will only be used for graphics pipelines.
	Move<VkImage>			attachmentImage;
	de::MovePtr<Allocation>	attachmentImageMemory;
	Move<VkImageView>		attachmentImageView;
	Move<VkRenderPass>		renderPass;
	Move<VkFramebuffer>		frameBuffer;

	if (isGraphics)
	{
		// Create image, render pass and framebuffer.
		const VkFormat				format				= getRenderTargetFormat(vki, physicalDevice);
		const VkFormatProperties	formatProperties	(getPhysicalDeviceFormatProperties(vki, physicalDevice, format));
		const VkImageTiling			imageTiling			= (formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) ? VK_IMAGE_TILING_LINEAR
														: (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) ? VK_IMAGE_TILING_OPTIMAL
														: VK_CORE_IMAGE_TILING_LAST;

		const VkImageCreateInfo imageCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType          sType;
			DE_NULL,								// const void*              pNext;
			(VkImageCreateFlags)0u,					// VkImageCreateFlags       flags;
			VK_IMAGE_TYPE_2D,						// VkImageType              imageType;
			format,									// VkFormat                 format;
			{
				256u,	// deUint32    width;
				256u,	// deUint32    height;
				1u		// deUint32    depth;
			},										// VkExtent3D               extent;
			1u,										// deUint32                 mipLevels;
			1u,										// deUint32                 arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits    samples;
			imageTiling,							// VkImageTiling            tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,	// VkImageUsageFlags        usage;
			VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode            sharingMode;
			0u,										// deUint32                 queueFamilyIndexCount;
			DE_NULL,								// const deUint32*          pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout            initialLayout;
		};

		attachmentImage			= createImage(vk, device, &imageCreateInfo);
		attachmentImageMemory	= context.getDefaultAllocator().allocate(getImageMemoryRequirements(vk, device, *attachmentImage), MemoryRequirement::Any);

		VK_CHECK(vk.bindImageMemory(device, *attachmentImage, attachmentImageMemory->getMemory(), attachmentImageMemory->getOffset()));

		changeColorAttachmentImageLayout(vk, commandBuffer.get(), attachmentImage.get());

		const VkImageViewCreateInfo imageViewCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType            sType;
			DE_NULL,									// const void*                pNext;
			(VkImageViewCreateFlags)0u,					// VkImageViewCreateFlags     flags;
			attachmentImage.get(),						// VkImage                    image;
			VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType            viewType;
			format,										// VkFormat                   format;
			{
				VK_COMPONENT_SWIZZLE_R,	// VkComponentSwizzle    r;
				VK_COMPONENT_SWIZZLE_G,	// VkComponentSwizzle    g;
				VK_COMPONENT_SWIZZLE_B,	// VkComponentSwizzle    b;
				VK_COMPONENT_SWIZZLE_A	// VkComponentSwizzle    a;
			},											// VkComponentMapping         components;
			{
				VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags    aspectMask;
				0u,							// deUint32              baseMipLevel;
				1u,							// deUint32              levelCount;
				0u,							// deUint32              baseArrayLayer;
				1u							// deUint32              layerCount;
			}											// VkImageSubresourceRange    subresourceRange;
		};

		attachmentImageView	= createImageView(vk, device, &imageViewCreateInfo);
		renderPass			= createSimpleRenderPass(vk, device, format,
													 VK_ATTACHMENT_LOAD_OP_CLEAR,
													 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
													 VK_ATTACHMENT_STORE_OP_DONT_CARE,
													 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		const VkFramebufferCreateInfo framebufferCreateInfo =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,		// VkStructureType             sType;
			DE_NULL,										// const void*                 pNext;
			(VkFramebufferCreateFlags)0u,					// VkFramebufferCreateFlags    flags;
			renderPass.get(),								// VkRenderPass                renderPass;
			1u,												// deUint32                    attachmentCount;
			&attachmentImageView.get(),						// const VkImageView*          pAttachments;
			256u,											// deUint32                    width;
			256u,											// deUint32                    height;
			1u												// deUint32                    layers;
		};

		frameBuffer = createFramebuffer(vk, device, &framebufferCreateInfo);
	}

	const VkDescriptorPoolSize descriptorPoolSizes[] =
	{
		{
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,	// VkDescriptorType    type;
			10									// deUint32            descriptorCount;
		},
		{
			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,	// VkDescriptorType    type;
			2									// deUint32            descriptorCount;
		},
		{
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,	// VkDescriptorType    type;
			2									// deUint32            descriptorCount;
		}
	};

	const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,	// VkStructureType                sType;
		DE_NULL,										// const void*                    pNext;
		(VkDescriptorPoolCreateFlags)0u,				// VkDescriptorPoolCreateFlags    flags;
		(isGraphics ? 3u : 5u),							// deUint32                       maxSets;
		DE_LENGTH_OF_ARRAY(descriptorPoolSizes),		// deUint32                       poolSizeCount;
		descriptorPoolSizes								// const VkDescriptorPoolSize*    pPoolSizes;
	};

	Move<VkDescriptorPool> descriptorPool = createDescriptorPool(vk, device, &descriptorPoolCreateInfo, DE_NULL);

	const VkDescriptorSetLayoutBinding setLayoutBindingA[] =
	{
		{
			0,									// deUint32              binding;
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,	// VkDescriptorType      descriptorType;
			5,									// deUint32              descriptorCount;
			VK_SHADER_STAGE_ALL,				// VkShaderStageFlags    stageFlags;
			DE_NULL								// const VkSampler*      pImmutableSamplers;
		}
	};

	const VkShaderStageFlags shaderStage = (isGraphics ? VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_COMPUTE_BIT);

	const VkDescriptorSetLayoutBinding setLayoutBindingB[] =
	{
		{
			0,									// deUint32              binding;
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,	// VkDescriptorType      descriptorType;
			5,									// deUint32              descriptorCount;
			(VkShaderStageFlags)shaderStage,	// VkShaderStageFlags    stageFlags;
			DE_NULL								// const VkSampler*      pImmutableSamplers;
		}
	};

	const VkDescriptorSetLayoutBinding setLayoutBindingC[] =
	{
		{
			0,									// deUint32              binding;
			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,	// VkDescriptorType      descriptorType;
			2,									// deUint32              descriptorCount;
			VK_SHADER_STAGE_ALL,				// VkShaderStageFlags    stageFlags;
			DE_NULL								// const VkSampler*      pImmutableSamplers;
		},
		{
			1,									// deUint32              binding;
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,	// VkDescriptorType      descriptorType;
			2,									// deUint32              descriptorCount;
			VK_SHADER_STAGE_ALL,				// VkShaderStageFlags    stageFlags;
			DE_NULL								// const VkSampler*      pImmutableSamplers;
		}
	};

	const Move<VkDescriptorSetLayout>		descriptorSetLayouts[]		=
	{
		getDescriptorSetLayout(vk, device, DE_LENGTH_OF_ARRAY(setLayoutBindingA), setLayoutBindingA),
		getDescriptorSetLayout(vk, device, DE_LENGTH_OF_ARRAY(setLayoutBindingB), setLayoutBindingB),
		getDescriptorSetLayout(vk, device, DE_LENGTH_OF_ARRAY(setLayoutBindingC), setLayoutBindingC)
	};

	const VkDescriptorSetLayout				setLayoutHandlesAC[]		=
	{
		descriptorSetLayouts[0].get(),
		descriptorSetLayouts[2].get()
	};

	const VkDescriptorSetLayout				setLayoutHandlesB[]			=
	{
		descriptorSetLayouts[1].get()
	};

	const VkDescriptorSetLayout				setLayoutHandlesBC[]		=
	{
		descriptorSetLayouts[1].get(),
		descriptorSetLayouts[2].get()
	};

	const VkDescriptorSet					descriptorSets[]			=
	{
		getDescriptorSet(vk, device, descriptorPool.get(), descriptorSetLayouts[0].get()),
		getDescriptorSet(vk, device, descriptorPool.get(), descriptorSetLayouts[1].get()),
		getDescriptorSet(vk, device, descriptorPool.get(), descriptorSetLayouts[2].get())
	};

	const VkDescriptorSet					setHandlesAC[]				=
	{
		descriptorSets[0],
		descriptorSets[2]
	};

	const VkDescriptorSet					setHandlesC[]				=
	{
		descriptorSets[2]
	};

	const Unique<VkPipelineLayout>			pipelineLayoutAC			(getPipelineLayout(vk, device, DE_LENGTH_OF_ARRAY(setLayoutHandlesAC), setLayoutHandlesAC));
	const Unique<VkPipelineLayout>			pipelineLayoutBC			(getPipelineLayout(vk, device, DE_LENGTH_OF_ARRAY(setLayoutHandlesBC), setLayoutHandlesBC));

	VkPipelineLayout						pipelineLayoutB;
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType                 sType;
			DE_NULL,										// const void*                     pNext;
			(VkPipelineLayoutCreateFlags)0u,				// VkPipelineLayoutCreateFlags     flags;
			DE_LENGTH_OF_ARRAY(setLayoutHandlesB),			// deUint32                        setLayoutCount;
			setLayoutHandlesB,								// const VkDescriptorSetLayout*    pSetLayouts;
			0u,												// deUint32                        pushConstantRangeCount;
			DE_NULL											// const VkPushConstantRange*      pPushConstantRanges;
		};

		VK_CHECK(vk.createPipelineLayout(device, &pipelineLayoutCreateInfo, DE_NULL, &pipelineLayoutB));
	}

	std::vector<Move<VkShaderModule>>	shaderModules;
	Move<VkPipeline>					pipeline;

	if (isGraphics)
	{
		shaderModules.push_back(createShaderModule(vk, device, context.getBinaryCollection().get("vertex"), 0));
		shaderModules.push_back(createShaderModule(vk, device, context.getBinaryCollection().get("fragment"), 0));

		const VkPipelineShaderStageCreateInfo	shaderStageCreateInfos[]	=
		{
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType                     sType;
				DE_NULL,												// const void*                         pNext;
				(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags    flags;
				VK_SHADER_STAGE_VERTEX_BIT,								// VkShaderStageFlagBits               stage;
				shaderModules[0].get(),									// VkShaderModule                      shader;
				"main",													// const char*                         pName;
				DE_NULL,												// const VkSpecializationInfo*         pSpecializationInfo;
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType                     sType;
				DE_NULL,												// const void*                         pNext;
				(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags    flags;
				VK_SHADER_STAGE_FRAGMENT_BIT,							// VkShaderStageFlagBits               stage;
				shaderModules[1].get(),									// VkShaderModule                      shader;
				"main",													// const char*                         pName;
				DE_NULL,												// const VkSpecializationInfo*         pSpecializationInfo;
			}
		};

		pipeline = createSimpleGraphicsPipeline(vk, device, DE_LENGTH_OF_ARRAY(shaderStageCreateInfos), shaderStageCreateInfos, pipelineLayoutB, renderPass.get(), context.getResourceInterface());
	}
	else
	{
		shaderModules.push_back(createShaderModule(vk, device, context.getBinaryCollection().get("compute"), 0));

		const VkPipelineShaderStageCreateInfo	shaderStageCreateInfo		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType                     sType;
			DE_NULL,												// const void*                         pNext;
			(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags    flags;
			VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits               stage;
			shaderModules[0].get(),								// VkShaderModule                      shader;
			"main",													// const char*                         pName;
			DE_NULL,												// const VkSpecializationInfo*         pSpecializationInfo;
		};

		const VkComputePipelineCreateInfo		computePipelineCreateInfo	=
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	// VkStructureType                    sType;
			DE_NULL,										// const void*                        pNext
			(VkPipelineCreateFlags)0,						// VkPipelineCreateFlags              flags
			shaderStageCreateInfo,							// VkPipelineShaderStageCreateInfo    stage
			pipelineLayoutB,								// VkPipelineLayout                   layout
			DE_NULL,										// VkPipeline                         basePipelineHandle
			0												// int                                basePipelineIndex
		};

		pipeline = createComputePipeline(vk, device, DE_NULL, &computePipelineCreateInfo);
	}

	if (isGraphics)
	{
		beginRenderPass(vk, commandBuffer.get(), renderPass.get(), frameBuffer.get(), makeRect2D(0, 0, 256u, 256u), tcu::Vec4(0.25f, 0.25f, 0.25f, 0.0f));
	}
	vk.cmdBindPipeline(commandBuffer.get(), bindPoint, pipeline.get());

	// Destroy the pipeline layout that was used to create the pipeline
	vk.destroyPipelineLayout(device, pipelineLayoutB, DE_NULL);

	vk.cmdBindDescriptorSets(commandBuffer.get(), bindPoint, pipelineLayoutAC.get(), 0u, DE_LENGTH_OF_ARRAY(setHandlesAC), setHandlesAC, 0u, DE_NULL);
	vk.cmdBindDescriptorSets(commandBuffer.get(), bindPoint, pipelineLayoutBC.get(), 1u, DE_LENGTH_OF_ARRAY(setHandlesC), setHandlesC, 0u, DE_NULL);

	if (isGraphics)
	{
		const VkViewport	viewport	=
		{
			0.0f,	// float    x;
			0.0f,	// float    y;
			16.0f,	// float    width;
			16.0f,	// float    height;
			0.0f,	// float    minDepth;
			1.0f	// float    maxDepth;
		};

		const VkRect2D		scissor		=
		{
			{ 0u,	0u	},	// VkOffset2D    offset;
			{ 16u,	16u	}	// VkExtent2D    extent;
		};

		vk.cmdSetViewport(commandBuffer.get(), 0u, 1u, &viewport);
		vk.cmdSetScissor(commandBuffer.get(), 0u, 1u, &scissor);
	}

	vk.cmdBindDescriptorSets(commandBuffer.get(), bindPoint, pipelineLayoutAC.get(), 0u, DE_LENGTH_OF_ARRAY(setHandlesAC), setHandlesAC, 0u, DE_NULL);

	// Test should always pass
	return tcu::TestStatus::pass("Pass");
}

void createPipelineLayoutLifetimeGraphicsSource (SourceCollections& dst)
{
	dst.glslSources.add("vertex") << glu::VertexSource(
		"#version 450\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"   gl_Position = vec4(1);\n"
		"}\n");

	dst.glslSources.add("fragment") << glu::FragmentSource(
		"#version 450\n"
		"\n"
		"layout(location=0) out vec4 x;\n"
		"layout(set=0) layout(binding=0) uniform foo { int x; int y; } bar;\n"
		"void main (void)\n"
		"{\n"
		"   x = vec4(bar.y);\n"
		"}\n");
}

// This test has the same functionality as VkLayerTest.DescriptorSetCompatibility
tcu::TestStatus pipelineLayoutLifetimeGraphicsTest (Context& context)
{
	return pipelineLayoutLifetimeTest(context, VK_PIPELINE_BIND_POINT_GRAPHICS);
}

void createPipelineLayoutLifetimeComputeSource (SourceCollections& dst)
{
	dst.glslSources.add("compute") << glu::ComputeSource(
		"#version 450\n"
		"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
		"layout(set=0) layout(binding=0) uniform foo { int x; int y; } bar;\n"
		"void main (void)\n"
		"{\n"
		"    vec4 x = vec4(bar.y);\n"
		"}\n");
}

tcu::TestStatus pipelineLayoutLifetimeComputeTest (Context& context)
{
	return pipelineLayoutLifetimeTest(context, VK_PIPELINE_BIND_POINT_COMPUTE);
}

void checkSupport (Context& context)
{
	const InstanceInterface&	vki				= context.getInstanceInterface();
	const VkPhysicalDevice		physicalDevice	= context.getPhysicalDevice();

	// Throws exception if not supported
	getRenderTargetFormat(vki, physicalDevice);
}

void destroyEarlyComputeSource(SourceCollections& programs)
{
	std::string comp =
		"#version 450\n"
		"layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
		"layout (constant_id=0) const uint flag = 0;\n"
		"layout (push_constant, std430) uniform PushConstants {\n"
		"    uint base;\n"
		"};\n"
		"layout (set=0, binding=0, std430) buffer Block {\n"
		"    uint data[];\n"
		"};\n"
		"\n"
		"void main() {\n"
		"    if (flag != 0u) {\n"
		"        uint idx = gl_GlobalInvocationID.x;\n"
		"        data[idx] = data[idx] + base + idx;\n"
		"    }\n"
		"}\n";

	programs.glslSources.add("comp") << glu::ComputeSource(comp);
}

void checkMaintenance4Support(Context& context)
{
	context.requireDeviceFunctionality("VK_KHR_maintenance4");
}

enum DestroyPipelineLayoutMode
{
	DPLM_DESTROY_AFTER_END_COMMAND_BUFFER = 0,
	DPLM_DESTROY_AFTER_CREATE_COMPUTE_PIPELINES,
};

tcu::TestStatus destroyEarlyTest (Context& context, DestroyPipelineLayoutMode mode)
{
	const auto&	vkd		= context.getDeviceInterface();
	const auto	device	= context.getDevice();
	auto&		alloc	= context.getDefaultAllocator();
	const auto	queue	= context.getUniversalQueue();
	const auto	qIndex	= context.getUniversalQueueFamilyIndex();

	const deUint32	kBufferElements	= 100u;
	const deUint32	kBufferSize		= kBufferElements * sizeof(deUint32);
	const auto		kBufferSizeDS	= static_cast<VkDeviceSize>(kBufferSize);
	const deUint32	kInitialValue	= 50u;
	const deUint32	kFlagValue		= 1u;
	const deUint32	kBaseValue		= 75u;

	// Allocate and prepare buffer.
	const auto				bufferInfo	= vk::makeBufferCreateInfo(kBufferSizeDS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	vk::BufferWithMemory	buffer		(vkd, device, alloc, bufferInfo, vk::MemoryRequirement::HostVisible);
	auto&					bufferAlloc	= buffer.getAllocation();
	void*					bufferPtr	= bufferAlloc.getHostPtr();
	{
		const std::vector<deUint32> bufferValues (kBufferElements, kInitialValue);
		deMemcpy(bufferPtr, bufferValues.data(), kBufferSize);
		vk::flushAlloc(vkd, device, bufferAlloc);
	}

	// Descriptor set layout.
	vk::DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);
	const auto descriptorSetLayout = layoutBuilder.build(vkd, device);

	// Pipeline layout.
	const auto pushConstantRange = vk::makePushConstantRange(vk::VK_SHADER_STAGE_COMPUTE_BIT, 0u, static_cast<deUint32>(sizeof(kBaseValue)));

	const vk::VkPipelineLayoutCreateInfo pipelineLayoutInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	//	VkStructureType					sType;
		nullptr,											//	const void*						pNext;
		0u,													//	VkPipelineLayoutCreateFlags		flags;
		1u,													//	deUint32						setLayoutCount;
		&descriptorSetLayout.get(),							//	const VkDescriptorSetLayout*	pSetLayouts;
		1u,													//	deUint32						pushConstantRangeCount;
		&pushConstantRange,									//	const VkPushConstantRange*		pPushConstantRanges;
	};

	auto pipelineLayout = vk::createPipelineLayout(vkd, device, &pipelineLayoutInfo);

	// Shader module.
	const auto shaderModule = vk::createShaderModule(vkd, device, context.getBinaryCollection().get("comp"), 0u);

	// Pipeline, with shader and specialization info.
	const auto specConstantSize = static_cast<deUintptr>(sizeof(kFlagValue));

	const vk::VkSpecializationMapEntry mapEntry =
	{
		0u,					//	deUint32	constantID;
		0u,					//	deUint32	offset;
		specConstantSize,	//	deUintptr	size;
	};

	const vk::VkSpecializationInfo specializationInfo =
	{
		1u,					//	deUint32						mapEntryCount;
		&mapEntry,			//	const VkSpecializationMapEntry*	pMapEntries;
		specConstantSize,	//	deUintptr						dataSize;
		&kFlagValue,		//	const void*						pData;
	};

	const VkPipelineShaderStageCreateInfo shaderInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,													//	const void*							pNext;
		0u,															//	VkPipelineShaderStageCreateFlags	flags;
		vk::VK_SHADER_STAGE_COMPUTE_BIT,							//	VkShaderStageFlagBits				stage;
		shaderModule.get(),											//	VkShaderModule						module;
		"main",														//	const char*							pName;
		&specializationInfo,										//	const VkSpecializationInfo*			pSpecializationInfo;
	};
	const vk::VkComputePipelineCreateInfo pipelineInfo =
	{
		vk::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,			//	VkStructureType					sType;
		nullptr,													//	const void*						pNext;
		0u,															//	VkPipelineCreateFlags			flags;
		shaderInfo,													//	VkPipelineShaderStageCreateInfo	stage;
		pipelineLayout.get(),										//	VkPipelineLayout				layout;
		DE_NULL,													//	VkPipeline						basePipelineHandle;
		0,															//	deInt32							basePipelineIndex;
	};

	const auto pipeline = vk::createComputePipeline(vkd, device, DE_NULL, &pipelineInfo);

	// Delete pipeline layout just after creating pipeline - this is what the test is for
	if (DPLM_DESTROY_AFTER_CREATE_COMPUTE_PIPELINES == mode)
		pipelineLayout = decltype(pipelineLayout)();

	// Descriptor set.
	vk::DescriptorPoolBuilder descriptorPoolBuilder;
	descriptorPoolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	const auto descriptorPool	= descriptorPoolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const auto descriptorSet	= vk::makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());

	// Update descriptor set with buffer.
	vk::DescriptorSetUpdateBuilder updateBuilder;
	const auto descriptorInfo = vk::makeDescriptorBufferInfo(buffer.get(), static_cast<VkDeviceSize>(0), kBufferSizeDS);
	updateBuilder.writeSingle(descriptorSet.get(), vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo);
	updateBuilder.update(vkd, device);

	// Prepare command buffer.
	const auto cmdPool		= vk::makeCommandPool(vkd, device, qIndex);
	const auto cmdBufferPtr	= vk::allocateCommandBuffer(vkd, device, cmdPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();
	const auto barrier		= vk::makeMemoryBarrier(vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_HOST_READ_BIT);

	// Create new pipeline layout that will be used during dispatch
	if (DPLM_DESTROY_AFTER_CREATE_COMPUTE_PIPELINES == mode)
		pipelineLayout = vk::createPipelineLayout(vkd, device, &pipelineLayoutInfo);

	vk::beginCommandBuffer(vkd, cmdBuffer);
	vkd.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), vk::VK_SHADER_STAGE_COMPUTE_BIT, 0u, static_cast<deUint32>(sizeof(kBaseValue)), &kBaseValue);
	vkd.cmdDispatch(cmdBuffer, kBufferElements, 1u, 1u);
	vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &barrier, 0u, nullptr, 0u, nullptr);
	vk::endCommandBuffer(vkd, cmdBuffer);

	// Delete pipeline layout just after recording command buffer - this is what the test is for
	if (DPLM_DESTROY_AFTER_END_COMMAND_BUFFER == mode)
		pipelineLayout = decltype(pipelineLayout)();

	// Submit commands.
	vk::submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Check buffer.
	vk::invalidateAlloc(vkd, device, bufferAlloc);
	std::vector<deUint32> outputData (kBufferElements);
	deMemcpy(outputData.data(), bufferPtr, kBufferSize);

	for (deUint32 i = 0; i < kBufferElements; ++i)
	{
		// This matches what the shader should calculate.
		const auto expectedValue = kInitialValue + kBaseValue + i;
		if (outputData[i] != expectedValue)
		{
			std::ostringstream msg;
			msg << "Unexpected value at buffer position " << i << ": expected " << expectedValue << " but found " << outputData[i];
			return tcu::TestStatus::fail(msg.str());
		}
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus destroyAfterEndCommndBufferTest(Context& context)
{
	return destroyEarlyTest(context, DPLM_DESTROY_AFTER_END_COMMAND_BUFFER);
}

tcu::TestStatus destroyAfterCreateComputePipelineTest(Context& context)
{
	return destroyEarlyTest(context, DPLM_DESTROY_AFTER_CREATE_COMPUTE_PIPELINES);
}

tcu::TestStatus destroyAfterCreateGraphicsPipelineTest(Context& context)
{
	return drawTriangleTest(context, DTM_DESTROY_PIPELINE_LAYOUT_AFTER_CREATING_PIPELINE);
}

#ifndef CTS_USES_VULKANSC

Move<VkPipeline> createSimpleGraphicsPipelineInvalidPointers (const DeviceInterface& vk, const VkDevice& device, deUint32 numShaderStages, const VkPipelineShaderStageCreateInfo* shaderStageCreateInfos, VkPipelineLayout pipelineLayout, VkRenderPass renderPass, de::SharedPtr<vk::ResourceInterface> resourceInterface)
{
#ifndef CTS_USES_VULKANSC
	DE_UNREF(resourceInterface);
#endif // CTS_USES_VULKANSC

	const void *invalidPointer = reinterpret_cast<void*>(~(0));

	const VkPipelineVertexInputStateCreateInfo		vertexInputStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType                             sType;
		DE_NULL,													// const void*                                 pNext;
		(VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags       flags;
		0u,															// deUint32                                    vertexBindingDescriptionCount;
		DE_NULL,													// const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
		0u,															// deUint32                                    vertexAttributeDescriptionCount;
		(const VkVertexInputAttributeDescription*)invalidPointer	// const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
	};

	const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType                            sType;
		DE_NULL,														// const void*                                pNext;
		(VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags    flags;
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,							// VkPrimitiveTopology                        topology;
		VK_FALSE														// VkBool32                                   primitiveRestartEnable;
	};

	// Disable rasterization to test unused structs
	const VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// VkStructureType                            sType;
		DE_NULL,													// const void*                                pNext;
		(VkPipelineRasterizationStateCreateFlags)0,					// VkPipelineRasterizationStateCreateFlags    flags;
		VK_FALSE,													// VkBool32                                   depthClampEnable;
		VK_TRUE,													// VkBool32                                   rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,										// VkPolygonMode                              polygonMode;
		VK_CULL_MODE_BACK_BIT,										// VkCullModeFlags                            cullMode;
		VK_FRONT_FACE_CLOCKWISE,									// VkFrontFace                                frontFace;
		VK_FALSE,													// VkBool32                                   depthBiasEnable;
		0.0f,														// float                                      depthBiasConstantFactor;
		0.0f,														// float                                      depthBiasClamp;
		0.0f,														// float                                      depthBiasSlopeFactor;
		1.0f														// float                                      lineWidth;
	};

	const VkGraphicsPipelineCreateInfo				graphicsPipelineCreateInfo		=
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,				// VkStructureType                                  sType;
		DE_NULL,														// const void*                                      pNext;
		(VkPipelineCreateFlags)0,										// VkPipelineCreateFlags                            flags;
		numShaderStages,												// deUint32                                         stageCount;
		shaderStageCreateInfos,											// const VkPipelineShaderStageCreateInfo*           pStages;
		&vertexInputStateCreateInfo,									// const VkPipelineVertexInputStateCreateInfo*      pVertexInputState;
		&inputAssemblyStateCreateInfo,									// const VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState;
		DE_NULL,														// const VkPipelineTessellationStateCreateInfo*     pTessellationState;
		(const VkPipelineViewportStateCreateInfo*)invalidPointer,		// const VkPipelineViewportStateCreateInfo*         pViewportState;
		&rasterizationStateCreateInfo,									// const VkPipelineRasterizationStateCreateInfo*    pRasterizationState;
		(const VkPipelineMultisampleStateCreateInfo*)invalidPointer,	// const VkPipelineMultisampleStateCreateInfo*      pMultisampleState;
		(const VkPipelineDepthStencilStateCreateInfo*)invalidPointer,	// const VkPipelineDepthStencilStateCreateInfo*     pDepthStencilState;
		(const VkPipelineColorBlendStateCreateInfo*)invalidPointer,		// const VkPipelineColorBlendStateCreateInfo*       pColorBlendState;
		DE_NULL,														// const VkPipelineDynamicStateCreateInfo*          pDynamicState;
		pipelineLayout,													// VkPipelineLayout                                 layout;
		renderPass,														// VkRenderPass                                     renderPass;
		0u,																// deUint32                                         subpass;
		DE_NULL,														// VkPipeline                                       basePipelineHandle;
		0																// int                                              basePipelineIndex;
	};

	const VkPipelineCacheCreateInfo					pipelineCacheCreateInfo			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,	// VkStructureType               sType;
		DE_NULL,										// const void*                   pNext;
#ifndef CTS_USES_VULKANSC
		(VkPipelineCacheCreateFlags)0u,									// VkPipelineCacheCreateFlags    flags;
		0,																// size_t                        initialDataSize;
		DE_NULL															// const void*                   pInitialData;
#else
		VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
			VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT,		// VkPipelineCacheCreateFlags    flags;
		resourceInterface->getCacheDataSize(),							// deUintptr                     initialDataSize;
		invalidPointer													// const void*                   pInitialData;
#endif // CTS_USES_VULKANSC
	};

	const Unique<VkPipelineCache>					pipelineCache					(createPipelineCache(vk, device, &pipelineCacheCreateInfo));

	return createGraphicsPipeline(vk, device, pipelineCache.get(), &graphicsPipelineCreateInfo);
}

tcu::TestStatus pipelineInvalidPointersUnusedStructsTest (Context& context, VkPipelineBindPoint bindPoint)
{
	const DeviceInterface&					vk							= context.getDeviceInterface();
	const VkDevice							device						= context.getDevice();
	const VkQueue							queue						= context.getUniversalQueue();
	const deUint32							queueFamilyIndex			= context.getUniversalQueueFamilyIndex();
	const bool								isGraphics					= (bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS);
	const void								*invalidPointer				= reinterpret_cast<void*>(~(0));

	const VkCommandPoolCreateInfo			commandPoolParams			=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,	// VkStructureType             sType;
		DE_NULL,									// const void*                 pNext;
		(VkCommandPoolCreateFlags)0u,				// VkCommandPoolCreateFlags    flags;
		queueFamilyIndex							// deUint32                    queueFamilyIndex;
	};

	const Unique<VkCommandPool>				commandPool					(createCommandPool(vk, device, &commandPoolParams, DE_NULL));
	const Unique<VkCommandBuffer>			commandBuffer				(createCommandBuffer(vk, device, commandPool.get()));

	// Begin command buffer.
	{
		const VkCommandBufferBeginInfo commandBufferBeginInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType                          sType;
			DE_NULL,										// const void*                              pNext;
			VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,	// VkCommandBufferUsageFlags                flags;
			DE_NULL											// const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
		};

		VK_CHECK(vk.beginCommandBuffer(commandBuffer.get(), &commandBufferBeginInfo));
	}

	// These will only be used for graphics pipelines.
	Move<VkRenderPass>		renderPass;
	Move<VkFramebuffer>		frameBuffer;

	{
		const VkSubpassDescription		subpassDescription		=
		{
			(VkSubpassDescriptionFlags)0u,						// VkSubpassDescriptionFlags       flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint             pipelineBindPoint
			0u,													// deUint32                        inputAttachmentCount
			(const VkAttachmentReference*)invalidPointer,		// const VkAttachmentReference*    pInputAttachments
			0u,													// deUint32                        colorAttachmentCount
			(const VkAttachmentReference*)invalidPointer,		// const VkAttachmentReference*    pColorAttachments
			DE_NULL,											// const VkAttachmentReference*    pResolveAttachments
			DE_NULL,											// const VkAttachmentReference*    pDepthStencilAttachment
			0u,													// deUint32                        preserveAttachmentCount
			(const deUint32*)invalidPointer						// const deUint32*                 pPreserveAttachments
		};

		const VkRenderPassCreateInfo	renderPassCreateInfo	=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType                   sType;
			DE_NULL,											// const void*                       pNext;
			(VkRenderPassCreateFlags)0u,						// VkRenderPassCreateFlags           flags;
			0u,													// deUint32                          attachmentCount
			(const VkAttachmentDescription*)invalidPointer,		// const VkAttachmentDescription*    pAttachments
			1u,													// deUint32                          subpassCount
			&subpassDescription,								// const VkSubpassDescription*       pSubpasses
			0u,													// deUint32                          dependencyCount
			(const VkSubpassDependency*)invalidPointer			// const VkSubpassDependency*        pDependencies
		};
		renderPass			= createRenderPass(vk, device, &renderPassCreateInfo);

		const VkFramebufferCreateInfo framebufferCreateInfo =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,		// VkStructureType             sType;
			DE_NULL,										// const void*                 pNext;
			(VkFramebufferCreateFlags)0u,					// VkFramebufferCreateFlags    flags;
			renderPass.get(),								// VkRenderPass                renderPass;
			0u,												// deUint32                    attachmentCount;
			(const VkImageView*)invalidPointer,				// const VkImageView*          pAttachments;
			256u,											// deUint32                    width;
			256u,											// deUint32                    height;
			1u												// deUint32                    layers;
		};

		frameBuffer = createFramebuffer(vk, device, &framebufferCreateInfo);
	}

	Move<VkPipelineLayout>				pipelineLayout;
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType                 sType;
			DE_NULL,										// const void*                     pNext;
			(VkPipelineLayoutCreateFlags)0u,				// VkPipelineLayoutCreateFlags     flags;
			0u,												// deUint32                        setLayoutCount;
			(const VkDescriptorSetLayout*)invalidPointer,	// const VkDescriptorSetLayout*    pSetLayouts;
			0u,												// deUint32                        pushConstantRangeCount;
			(const VkPushConstantRange*)invalidPointer		// const VkPushConstantRange*      pPushConstantRanges;
		};

		pipelineLayout = vk::createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
	}

	std::vector<Move<VkShaderModule>>	shaderModules;
	Move<VkPipeline>					pipeline;

	if (isGraphics)
	{
		shaderModules.push_back(createShaderModule(vk, device, context.getBinaryCollection().get("vertex"), 0));

		const VkPipelineShaderStageCreateInfo	shaderStageCreateInfos[]	=
		{
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType                     sType;
				DE_NULL,												// const void*                         pNext;
				(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags    flags;
				VK_SHADER_STAGE_VERTEX_BIT,								// VkShaderStageFlagBits               stage;
				shaderModules[0].get(),									// VkShaderModule                      shader;
				"main",													// const char*                         pName;
				DE_NULL,												// const VkSpecializationInfo*         pSpecializationInfo;
			}
		};

		pipeline = createSimpleGraphicsPipelineInvalidPointers(vk, device, DE_LENGTH_OF_ARRAY(shaderStageCreateInfos), shaderStageCreateInfos, *pipelineLayout, renderPass.get(), context.getResourceInterface());
	}
	else
	{
		shaderModules.push_back(createShaderModule(vk, device, context.getBinaryCollection().get("compute"), 0));

		const VkPipelineShaderStageCreateInfo	shaderStageCreateInfo		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType                     sType;
			DE_NULL,												// const void*                         pNext;
			(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags    flags;
			VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits               stage;
			shaderModules[0].get(),									// VkShaderModule                      shader;
			"main",													// const char*                         pName;
			DE_NULL,												// const VkSpecializationInfo*         pSpecializationInfo;
		};

		const VkComputePipelineCreateInfo		computePipelineCreateInfo	=
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	// VkStructureType                    sType;
			DE_NULL,										// const void*                        pNext
			(VkPipelineCreateFlags)0,						// VkPipelineCreateFlags              flags
			shaderStageCreateInfo,							// VkPipelineShaderStageCreateInfo    stage
			*pipelineLayout,									// VkPipelineLayout                   layout
			DE_NULL,										// VkPipeline                         basePipelineHandle
			0												// int                                basePipelineIndex
		};

		pipeline = createComputePipeline(vk, device, DE_NULL, &computePipelineCreateInfo);
	}

	if (isGraphics)
	{
		beginRenderPass(vk, commandBuffer.get(), renderPass.get(), frameBuffer.get(), makeRect2D(0, 0, 256u, 256u), tcu::Vec4(0.25f, 0.25f, 0.25f, 0.0f));
	}
	vk.cmdBindPipeline(commandBuffer.get(), bindPoint, pipeline.get());

	if (isGraphics)
	{
		vk.cmdDraw(commandBuffer.get(), 1u, 1u, 0u, 0u);
		vk.cmdEndRenderPass(commandBuffer.get());
	}
	else
	{
		vk.cmdDispatch(commandBuffer.get(), 1u, 1u, 1u);
	}
	vk.endCommandBuffer(commandBuffer.get());

	const VkSubmitInfo				submitInfo				=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType                sType;
		DE_NULL,						// const void*                    pNext;
		0u,								// deUint32                       waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*             pWaitSemaphores;
		DE_NULL,						// const VkPipelineStageFlags*    pWaitDstStageMask;
		1u,								// deUint32                       commandBufferCount;
		&commandBuffer.get(),			// const VkCommandBuffer*         pCommandBuffers;
		0u,								// deUint32                       signalSemaphoreCount;
		DE_NULL							// const VkSemaphore*             pSignalSemaphores;
	};

	VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, DE_NULL));
	VK_CHECK(vk.queueWaitIdle(queue));

	// Test should always pass
	return tcu::TestStatus::pass("Pass");
}

void createPipelineInvalidPointersUnusedStructsGraphicsSource (SourceCollections& dst)
{
	dst.glslSources.add("vertex") << glu::VertexSource(
		"#version 450\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"   gl_Position = vec4(1.0f);\n"
		"}\n");

	dst.glslSources.add("fragment") << glu::FragmentSource(
		"#version 450\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"}\n");
}

tcu::TestStatus pipelineInvalidPointersUnusedStructsGraphicsTest (Context& context)
{
	return pipelineInvalidPointersUnusedStructsTest(context, VK_PIPELINE_BIND_POINT_GRAPHICS);
}

void createPipelineInvalidPointersUnusedStructsComputeSource (SourceCollections& dst)
{
	dst.glslSources.add("compute") << glu::ComputeSource(
		"#version 450\n"
		"layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
		"void main (void)\n"
		"{\n"
		"}\n");
}

tcu::TestStatus pipelineInvalidPointersUnusedStructsComputeTest (Context& context)
{
	return pipelineInvalidPointersUnusedStructsTest(context, VK_PIPELINE_BIND_POINT_COMPUTE);
}

#endif // CTS_USES_VULKANSC

tcu::TestCaseGroup* createrenderpassTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> renderPassTests(new tcu::TestCaseGroup(testCtx, "renderpass", "Renderpass tests"));

	addFunctionCaseWithPrograms(renderPassTests.get(), "destroy_pipeline_renderpass", "Draw after destroying the renderpass used to create a pipeline", checkSupport, createDrawTriangleSource, renderpassLifetimeTest);
	addFunctionCase(renderPassTests.get(), "framebuffer_compatible_renderpass", "Use a render pass with a framebuffer that was created using another compatible render pass", checkSupport, framebufferCompatibleRenderPassTest);

	return renderPassTests.release();
}

tcu::TestCaseGroup* createPipelineLayoutLifetimeTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> pipelineLayoutLifetimeTests(new tcu::TestCaseGroup(testCtx, "lifetime", "Pipeline layout lifetime tests"));

	addFunctionCaseWithPrograms(pipelineLayoutLifetimeTests.get(), "graphics", "Test pipeline layout lifetime in graphics pipeline", checkSupport, createPipelineLayoutLifetimeGraphicsSource, pipelineLayoutLifetimeGraphicsTest);
	addFunctionCaseWithPrograms(pipelineLayoutLifetimeTests.get(), "compute", "Test pipeline layout lifetime in compute pipeline", checkSupport, createPipelineLayoutLifetimeComputeSource, pipelineLayoutLifetimeComputeTest);
	addFunctionCaseWithPrograms(pipelineLayoutLifetimeTests.get(), "destroy_after_end", "Test destroying the pipeline layout after vkEndCommandBuffer", destroyEarlyComputeSource, destroyAfterEndCommndBufferTest);
	addFunctionCaseWithPrograms(pipelineLayoutLifetimeTests.get(), "destroy_after_compute_pipeline_construction", "Test destroying the pipeline layout after compute pipeline creation", checkMaintenance4Support, destroyEarlyComputeSource, destroyAfterCreateComputePipelineTest);
	addFunctionCaseWithPrograms(pipelineLayoutLifetimeTests.get(), "destroy_after_graphics_pipeline_construction", "Test destroying the pipeline layout after graphics pipeline creation", checkMaintenance4Support, createDrawTriangleSource, destroyAfterCreateGraphicsPipelineTest);

	return pipelineLayoutLifetimeTests.release();
}

tcu::TestCaseGroup* createPipelineLayoutTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> pipelineLayoutTests(new tcu::TestCaseGroup(testCtx, "pipeline_layout", "Pipeline layout tests"));

	pipelineLayoutTests->addChild(createPipelineLayoutLifetimeTests(testCtx));

	return pipelineLayoutTests.release();
}

#ifndef CTS_USES_VULKANSC
tcu::TestCaseGroup* createPipelineInvalidPointersUnusedStructsTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> pipelineInvalidPointersUnusedStructsTests(new tcu::TestCaseGroup(testCtx, "pipeline_invalid_pointers_unused_structs", "Create pipelines with invalid pointers for unused structs"));

	addFunctionCaseWithPrograms(pipelineInvalidPointersUnusedStructsTests.get(), "graphics", "Test structs when creating a graphics pipeline", checkSupport, createPipelineInvalidPointersUnusedStructsGraphicsSource, pipelineInvalidPointersUnusedStructsGraphicsTest);
	addFunctionCaseWithPrograms(pipelineInvalidPointersUnusedStructsTests.get(), "compute", "Test structs when creating a compute pipeline", checkSupport, createPipelineInvalidPointersUnusedStructsComputeSource, pipelineInvalidPointersUnusedStructsComputeTest);

	return pipelineInvalidPointersUnusedStructsTests.release();
}
#endif // CTS_USES_VULKANSC

} // anonymous

tcu::TestCaseGroup* createPipelineTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> pipelineTests(new tcu::TestCaseGroup(testCtx, "pipeline", "Pipeline tests"));

	pipelineTests->addChild(createrenderpassTests(testCtx));
	pipelineTests->addChild(createPipelineLayoutTests(testCtx));
#ifndef CTS_USES_VULKANSC
	pipelineTests->addChild(createPipelineInvalidPointersUnusedStructsTests(testCtx));
#endif // CTS_USES_VULKANSC

	return pipelineTests.release();
}

} // api
} // vkt
