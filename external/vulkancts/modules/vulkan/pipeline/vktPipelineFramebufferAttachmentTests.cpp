/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \file vktPipelineFramebuferAttachmentTests.cpp
 * \brief Render to a framebuffer with attachments of different sizes and with
 *        no attachments at all
 *
 *//*--------------------------------------------------------------------*/

#include "vktPipelineFramebufferAttachmentTests.hpp"
#include "vktPipelineMakeUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkPrograms.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"

#include <string>
#include <vector>

namespace vkt
{
namespace pipeline
{
namespace
{
using namespace vk;
using de::UniquePtr;
using de::MovePtr;
using de::SharedPtr;
using tcu::IVec3;
using tcu::Vec4;
using tcu::UVec4;
using tcu::IVec4;
using std::vector;

static const VkFormat COLOR_FORMAT	=		VK_FORMAT_R8G8B8A8_UNORM;

typedef SharedPtr<Unique<VkImageView> >	SharedPtrVkImageView;

enum MultiAttachmentsTestType
{
	MULTI_ATTACHMENTS_NONE,
	MULTI_ATTACHMENTS_DIFFERENT_SIZES,
	MULTI_ATTACHMENTS_NOT_EXPORTED,
};

struct CaseDef
{
	PipelineConstructionType	pipelineConstructionType;
	VkImageViewType				imageType;
	IVec3						renderSize;
	IVec3						attachmentSize;
	deUint32					numLayers;
	bool						multisample;
	MultiAttachmentsTestType	multiAttachmentsTestType;
};

template<typename T>
inline SharedPtr<Unique<T> > makeSharedPtr(Move<T> move)
{
	return SharedPtr<Unique<T> >(new Unique<T>(move));
}

template<typename T>
inline VkDeviceSize sizeInBytes(const vector<T>& vec)
{
	return vec.size() * sizeof(vec[0]);
}

VkImageType getImageType(const VkImageViewType viewType)
{
	switch (viewType)
	{
		case VK_IMAGE_VIEW_TYPE_1D:
		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
			return VK_IMAGE_TYPE_1D;

		case VK_IMAGE_VIEW_TYPE_2D:
		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		case VK_IMAGE_VIEW_TYPE_CUBE:
		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
			return VK_IMAGE_TYPE_2D;

		case VK_IMAGE_VIEW_TYPE_3D:
			return VK_IMAGE_TYPE_3D;

		default:
			DE_ASSERT(0);
			return VK_IMAGE_TYPE_LAST;
	}
}

//! Make a render pass with one subpass per color attachment and one attachment per image layer.
RenderPassWrapper makeRenderPass (const DeviceInterface&			vk,
								  const VkDevice					device,
								  const PipelineConstructionType	pipelineConstructionType,
								  const VkFormat					colorFormat,
								  const deUint32					numLayers,
								  const bool						multisample)
{
	vector<VkAttachmentDescription>	attachmentDescriptions		(numLayers);
	deUint32						attachmentIndex				= 0;
	vector<VkAttachmentReference>	colorAttachmentReferences	(numLayers);
	vector<VkSubpassDescription>	subpasses;

	for (deUint32 i = 0; i < numLayers; i++)
	{
		VkAttachmentDescription colorAttachmentDescription =
		{
			(VkAttachmentDescriptionFlags)0,								// VkAttachmentDescriptionFla	flags;
			colorFormat,													// VkFormat						format;
			!multisample ? VK_SAMPLE_COUNT_1_BIT : VK_SAMPLE_COUNT_4_BIT,	// VkSampleCountFlagBits		samples;
			VK_ATTACHMENT_LOAD_OP_LOAD,										// VkAttachmentLoadOp			loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp			storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,								// VkAttachmentLoadOp			stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,								// VkAttachmentStoreOp			stencilStoreOp;
			VK_IMAGE_LAYOUT_GENERAL,										// VkImageLayout				initialLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,						// VkImageLayout				finalLayout;
		};
		attachmentDescriptions[attachmentIndex++] = colorAttachmentDescription;
	}

	// Create a subpass for each attachment (each attachment is a layer of an arrayed image).
	for (deUint32 i = 0; i < numLayers; ++i)
	{
		const VkAttachmentReference attachmentRef =
		{
			i,											// deUint32			attachment;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout;
		};
		colorAttachmentReferences[i] = attachmentRef;

		const VkSubpassDescription subpassDescription =
		{
			(VkSubpassDescriptionFlags)0,		// VkSubpassDescriptionFlags		flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint				pipelineBindPoint;
			0u,									// deUint32							inputAttachmentCount;
			DE_NULL,							// const VkAttachmentReference*		pInputAttachments;
			1u,									// deUint32							colorAttachmentCount;
			&colorAttachmentReferences[i],		// const VkAttachmentReference*		pColorAttachments;
			DE_NULL,							// const VkAttachmentReference*		pResolveAttachments;
			DE_NULL,							// const VkAttachmentReference*		pDepthStencilAttachment;
			0u,									// deUint32							preserveAttachmentCount;
			DE_NULL								// const deUint32*					pPreserveAttachments;
		};
		subpasses.push_back(subpassDescription);
	}

	const VkRenderPassCreateInfo renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,									// const void*						pNext;
		(VkRenderPassCreateFlags)0,					// VkRenderPassCreateFlags			flags;
		numLayers,									// deUint32							attachmentCount;
		&attachmentDescriptions[0],					// const VkAttachmentDescription*	pAttachments;
		static_cast<deUint32>(subpasses.size()),	// deUint32							subpassCount;
		&subpasses[0],								// const VkSubpassDescription*		pSubpasses;
		0u,											// deUint32							dependencyCount;
		DE_NULL										// const VkSubpassDependency*		pDependencies;
	};

	return RenderPassWrapper(pipelineConstructionType, vk, device, &renderPassInfo);
}

void preparePipelineWrapper(GraphicsPipelineWrapper&		gpw,
							const PipelineLayoutWrapper&	pipelineLayout,
							const VkRenderPass				renderPass,
							const ShaderWrapper				vertexModule,
							const ShaderWrapper				fragmentModule,
							const IVec3						renderSize,
							const VkPrimitiveTopology		topology,
							const deUint32					subpass,
							const deUint32					numAttachments,
							const bool						multisample)
{
	const std::vector<VkViewport>				viewports							{ makeViewport(renderSize) };
	const std::vector<VkRect2D>					scissors							{ makeRect2D(renderSize) };

	const VkColorComponentFlags					colorComponentsAll					= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	const VkPipelineMultisampleStateCreateInfo	pipelineMultisampleStateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,		// VkStructureType                          sType;
		DE_NULL,														// const void*                              pNext;
		(VkPipelineMultisampleStateCreateFlags)0,						// VkPipelineMultisampleStateCreateFlags    flags;
		multisample ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT,	// VkSampleCountFlagBits                    rasterizationSamples;
		VK_FALSE,														// VkBool32                                 sampleShadingEnable;
		1.0f,															// float                                    minSampleShading;
		DE_NULL,														// const VkSampleMask*                      pSampleMask;
		VK_FALSE,														// VkBool32                                 alphaToCoverageEnable;
		VK_FALSE														// VkBool32                                 alphaToOneEnable;
	};

	const VkPipelineColorBlendAttachmentState	pipelineColorBlendAttachmentState
	{
		VK_FALSE,				// VkBool32                 blendEnable;
		VK_BLEND_FACTOR_ONE,	// VkBlendFactor            srcColorBlendFactor;
		VK_BLEND_FACTOR_ZERO,	// VkBlendFactor            dstColorBlendFactor;
		VK_BLEND_OP_ADD,		// VkBlendOp                colorBlendOp;
		VK_BLEND_FACTOR_ONE,	// VkBlendFactor            srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ZERO,	// VkBlendFactor            dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,		// VkBlendOp                alphaBlendOp;
		colorComponentsAll		// VkColorComponentFlags    colorWriteMask;
	};

	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;
	for (deUint32 attachmentIdx = 0; attachmentIdx < numAttachments; attachmentIdx++)
		colorBlendAttachmentStates.push_back(pipelineColorBlendAttachmentState);

	const VkPipelineColorBlendStateCreateInfo	pipelineColorBlendStateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,		// VkStructureType                              sType;
		DE_NULL,														// const void*                                  pNext;
		(VkPipelineColorBlendStateCreateFlags)0,						// VkPipelineColorBlendStateCreateFlags         flags;
		VK_FALSE,														// VkBool32                                     logicOpEnable;
		VK_LOGIC_OP_COPY,												// VkLogicOp                                    logicOp;
		numAttachments,													// deUint32                                     attachmentCount;
		numAttachments == 0 ? DE_NULL : &colorBlendAttachmentStates[0],	// const VkPipelineColorBlendAttachmentState*   pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f }										// float                                        blendConstants[4];
	};

	gpw.setDefaultTopology(topology)
	   .setDefaultRasterizationState()
	   .setDefaultDepthStencilState()
	   .setupVertexInputState()
	   .setupPreRasterizationShaderState(viewports,
										 scissors,
										 pipelineLayout,
										 renderPass,
										 subpass,
										 vertexModule)
	   .setupFragmentShaderState(pipelineLayout, renderPass, subpass, fragmentModule, DE_NULL, &pipelineMultisampleStateInfo)
	   .setupFragmentOutputState(renderPass, subpass, &pipelineColorBlendStateInfo, &pipelineMultisampleStateInfo)
	   .setMonolithicPipelineLayout(pipelineLayout)
	   .buildPipeline();
}

Move<VkImage> makeImage (const DeviceInterface&		vk,
						 const VkDevice				device,
						 const VkImageCreateFlags	flags,
						 const VkImageType			imageType,
						 const VkFormat				format,
						 const IVec3&				size,
						 const deUint32				numLayers,
						 const VkImageUsageFlags	usage,
						 const bool					multisample)
{
	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,							// VkStructureType			sType;
		DE_NULL,														// const void*				pNext;
		flags,															// VkImageCreateFlags		flags;
		imageType,														// VkImageType				imageType;
		format,															// VkFormat					format;
		makeExtent3D(size),												// VkExtent3D				extent;
		1u,																// deUint32					mipLevels;
		numLayers,														// deUint32					arrayLayers;
		multisample ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT,	// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,										// VkImageTiling			tiling;
		usage,															// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,										// VkSharingMode			sharingMode;
		0u,																// deUint32					queueFamilyIndexCount;
		DE_NULL,														// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,										// VkImageLayout			initialLayout;
	};

	return createImage(vk, device, &imageParams);
}

vector<tcu::Vec4> genFullQuadVertices (const int subpassCount)
{
	vector<tcu::Vec4>	vectorData;
	for (int subpassNdx = 0; subpassNdx < subpassCount; ++subpassNdx)
	{
		vectorData.push_back(Vec4(-1.0f, -1.0f, 0.0f, 1.0f));
		vectorData.push_back(Vec4(-1.0f,  1.0f, 0.0f, 1.0f));
		vectorData.push_back(Vec4(1.0f, -1.0f, 0.0f, 1.0f));
		vectorData.push_back(Vec4(1.0f,  1.0f, 0.0f, 1.0f));
	}
	return vectorData;
}

void initColorPrograms (SourceCollections& programCollection, const CaseDef)
{
	// Vertex shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in vec4 in_position;\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "	vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "	gl_Position	= in_position;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) out vec4 o_color;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    o_color = vec4(1.0, 0.5, 0.25, 1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

tcu::PixelBufferAccess getExpectedData (tcu::TextureLevel& textureLevel, const CaseDef& caseDef)
{
	const tcu::PixelBufferAccess	expectedImage	(textureLevel);
	const int						renderDepth		= deMax32(caseDef.renderSize.z(), caseDef.numLayers);

	for (int z = 0; z < expectedImage.getDepth(); ++z)
	{
		for (int y = 0; y < expectedImage.getHeight(); ++y)
		{
			for (int x = 0; x < expectedImage.getWidth(); ++x)
			{
				if (x < caseDef.renderSize.x() && y < caseDef.renderSize.y() && z < renderDepth)
					expectedImage.setPixel(tcu::Vec4(1.0, 0.5, 0.25, 1.0), x, y, z);
				else
					expectedImage.setPixel(tcu::Vec4(0.0, 0.0, 0.0, 1.0), x, y, z);
			}
		}
	}
	return expectedImage;
}

inline VkImageSubresourceRange makeColorSubresourceRange (const deUint32 baseArrayLayer, const deUint32 layerCount)
{
	return makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, baseArrayLayer, layerCount);
}

// Tests rendering to a a framebuffer with color attachments larger than the
// framebuffer dimensions and verifies that rendering does not affect the areas
// of the attachment outside the framebuffer dimensions. Tests both single-sample
// and multi-sample configurations.
tcu::TestStatus test (Context& context, const CaseDef caseDef)
{
	const InstanceInterface&		vki					= context.getInstanceInterface();
	const DeviceInterface&			vk					= context.getDeviceInterface();
	const VkPhysicalDevice			physicalDevice		= context.getPhysicalDevice();
	const VkDevice					device				= context.getDevice();
	const VkQueue					queue				= context.getUniversalQueue();
	const deUint32					queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	Allocator&						allocator			= context.getDefaultAllocator();

	// Color image for rendering in single-sample tests or resolve target for multi-sample tests
	Move<VkImage>					colorImage;
	MovePtr<Allocation>				colorImageAlloc;

	// For multisampled tests, this is the rendering target
	Move<VkImage>					msColorImage;
	MovePtr<Allocation>				msColorImageAlloc;

	// Host memory buffer where we will copy the rendered image for verification
	const deUint32					att_size_x			= caseDef.attachmentSize.x();
	const deUint32					att_size_y			= caseDef.attachmentSize.y();
	const deUint32					att_size_z			= caseDef.attachmentSize.z();
	const VkDeviceSize				colorBufferSize		= att_size_x * att_size_y * att_size_z * caseDef.numLayers * tcu::getPixelSize(mapVkFormat(COLOR_FORMAT));
	const Unique<VkBuffer>			colorBuffer			(makeBuffer(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>		colorBufferAlloc	(bindBuffer(vk, device, allocator, *colorBuffer, MemoryRequirement::HostVisible));

	Move<VkBuffer>					vertexBuffer;
	MovePtr<Allocation>				vertexBufferAlloc;

	vector<SharedPtrVkImageView>	colorAttachments;
	vector<VkImage>					images;
	vector<VkImageView>				attachmentHandles;

	const PipelineLayoutWrapper		pipelineLayout		(caseDef.pipelineConstructionType, vk, device);
	vector<GraphicsPipelineWrapper>	pipeline;
	RenderPassWrapper				renderPass			(makeRenderPass(vk, device, caseDef.pipelineConstructionType, COLOR_FORMAT, caseDef.numLayers, caseDef.multisample));

	const ShaderWrapper				vertexModule		(ShaderWrapper(vk, device, context.getBinaryCollection().get("vert"), 0u));
	const ShaderWrapper				fragmentModule		(ShaderWrapper(vk, device, context.getBinaryCollection().get("frag"), 0u));

	const Unique<VkCommandPool>		cmdPool				(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer			(makeCommandBuffer(vk, device, *cmdPool));

	const VkImageViewType			imageViewType		= caseDef.imageType == VK_IMAGE_VIEW_TYPE_CUBE || caseDef.imageType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
		? VK_IMAGE_VIEW_TYPE_2D : caseDef.imageType;

	// create vertexBuffer
	{
		const vector<tcu::Vec4>	vertices			= genFullQuadVertices(caseDef.numLayers);
		const VkDeviceSize		vertexBufferSize	= sizeInBytes(vertices);

		vertexBuffer		= makeBuffer(vk, device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		vertexBufferAlloc	= bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible);

		deMemcpy(vertexBufferAlloc->getHostPtr(), &vertices[0], static_cast<std::size_t>(vertexBufferSize));
		flushAlloc(vk, device, *vertexBufferAlloc);
	}

	// create colorImage (and msColorImage) using the configured attachmentsize
	{
		const VkImageUsageFlags	colorImageUsage	= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		colorImage		= makeImage(vk, device, VkImageViewCreateFlags(0), getImageType(caseDef.imageType), COLOR_FORMAT,
			caseDef.attachmentSize, caseDef.numLayers, colorImageUsage, false);
		colorImageAlloc	= bindImage(vk, device, allocator, *colorImage, MemoryRequirement::Any);

		if (caseDef.multisample)
		{
			const VkImageUsageFlags	msImageUsage	= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

			msColorImage		= makeImage(vk, device, VkImageViewCreateFlags(0), getImageType(caseDef.imageType), COLOR_FORMAT,
				caseDef.attachmentSize, caseDef.numLayers, msImageUsage, true);
			msColorImageAlloc	= bindImage(vk, device, allocator, *msColorImage, MemoryRequirement::Any);
		}
	}

	// create attachmentHandles and pipelines (one for each layer). We use the renderSize for viewport and scissor
	pipeline.reserve(caseDef.numLayers);
	for (deUint32 layerNdx = 0; layerNdx < caseDef.numLayers; ++layerNdx)
	{
		colorAttachments.push_back(makeSharedPtr(makeImageView(vk, device, ! caseDef.multisample ? *colorImage : *msColorImage,
			imageViewType, COLOR_FORMAT, makeColorSubresourceRange(layerNdx, 1))));
		images.push_back(!caseDef.multisample ? *colorImage : *msColorImage);
		attachmentHandles.push_back(**colorAttachments.back());

		pipeline.emplace_back(vki, vk, physicalDevice, device, context.getDeviceExtensions(), caseDef.pipelineConstructionType);
		preparePipelineWrapper(pipeline.back(), pipelineLayout, *renderPass, vertexModule, fragmentModule,
							   caseDef.renderSize, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, layerNdx, 1u, caseDef.multisample);
	}

	// create framebuffer
	renderPass.createFramebuffer(vk, device, caseDef.numLayers, &images[0], &attachmentHandles[0],
		static_cast<deUint32>(caseDef.renderSize.x()), static_cast<deUint32>(caseDef.renderSize.y()));

	// record command buffer
	beginCommandBuffer(vk, *cmdBuffer);
	{
		// Clear the entire image attachment to black
		{
			const VkImageMemoryBarrier	imageLayoutBarriers[]	=
			{
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// VkStructureType			sType;
					DE_NULL,											// const void*				pNext;
					0u,													// VkAccessFlags			srcAccessMask;
					VK_ACCESS_TRANSFER_WRITE_BIT,						// VkAccessFlags			dstAccessMask;
					VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout			oldLayout;
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,				// VkImageLayout			newLayout;
					VK_QUEUE_FAMILY_IGNORED,							// deUint32					srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,							// deUint32					destQueueFamilyIndex;
					caseDef.multisample ? *msColorImage : *colorImage,	// VkImage					image;
					makeColorSubresourceRange(0, caseDef.numLayers)		// VkImageSubresourceRange	subresourceRange;
				},
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
				0u, DE_NULL, 0u, DE_NULL, 1u, imageLayoutBarriers);

			const VkImageSubresourceRange	ranges		= makeColorSubresourceRange(0, caseDef.numLayers);
			const VkClearColorValue			clearColor	=
			{
				{0.0f, 0.0f, 0.0f, 1.0f}
			};
			vk.cmdClearColorImage(*cmdBuffer, caseDef.multisample ? *msColorImage : *colorImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1u, &ranges);

			const VkImageMemoryBarrier	imageClearBarriers[]	=
			{
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// VkStructureType			sType;
					DE_NULL,											// const void*				pNext;
					VK_ACCESS_TRANSFER_WRITE_BIT,						// VkAccessFlags			srcAccessMask;
					VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,				// VkAccessFlags			dstAccessMask;
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,				// VkImageLayout			oldLayout;
					VK_IMAGE_LAYOUT_GENERAL,							// VkImageLayout			newLayout;
					VK_QUEUE_FAMILY_IGNORED,							// deUint32					srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,							// deUint32					destQueueFamilyIndex;
					caseDef.multisample ? *msColorImage : *colorImage,	// VkImage					image;
					makeColorSubresourceRange(0, caseDef.numLayers)		// VkImageSubresourceRange	subresourceRange;
				},
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u,
				0u, DE_NULL, 0u, DE_NULL, 1u, imageClearBarriers);
		}

		// Render pass: this should render only to the area defined by renderSize (smaller than the size of the attachment)
		{
			const VkDeviceSize			vertexBufferOffset	= 0ull;

			renderPass.begin(vk, *cmdBuffer, makeRect2D(0, 0, caseDef.renderSize.x(), caseDef.renderSize.y()));
			{
				vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
				for (deUint32 layerNdx = 0; layerNdx < caseDef.numLayers; ++layerNdx)
				{
					if (layerNdx != 0)
						renderPass.nextSubpass(vk, *cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);

					pipeline[layerNdx].bind(*cmdBuffer);
					vk.cmdDraw(*cmdBuffer, 4u, 1u, layerNdx*4u, 0u);
				}
			}
			renderPass.end(vk, *cmdBuffer);
		}

		// If we are using a multi-sampled render target (msColorImage), resolve it now (to colorImage)
		if (caseDef.multisample)
		{
			// Transition msColorImage (from layout COLOR_ATTACHMENT_OPTIMAL) and colorImage (from layout UNDEFINED) to layout GENERAL before resolving
			const VkImageMemoryBarrier	imageBarriers[]	=
			{
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
					DE_NULL,										// const void*				pNext;
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags			srcAccessMask;
					VK_ACCESS_TRANSFER_READ_BIT,					// VkAccessFlags			dstAccessMask;
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// VkImageLayout			oldLayout;
					VK_IMAGE_LAYOUT_GENERAL,						// VkImageLayout			newLayout;
					VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,						// deUint32					destQueueFamilyIndex;
					*msColorImage,									// VkImage					image;
					makeColorSubresourceRange(0, caseDef.numLayers)	// VkImageSubresourceRange	subresourceRange;
				},
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
					DE_NULL,										// const void*				pNext;
					(VkAccessFlags)0,								// VkAccessFlags			srcAccessMask;
					VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags			dstAccessMask;
					VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			oldLayout;
					VK_IMAGE_LAYOUT_GENERAL,						// VkImageLayout			newLayout;
					VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,						// deUint32					destQueueFamilyIndex;
					*colorImage,									// VkImage					image;
					makeColorSubresourceRange(0, caseDef.numLayers)	// VkImageSubresourceRange	subresourceRange;
				}
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
				0u, DE_NULL, 0u, DE_NULL, 2u, imageBarriers);

			const VkImageResolve	region	=
			{
				makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, caseDef.numLayers),	// VkImageSubresourceLayers    srcSubresource;
				makeOffset3D(0, 0, 0),															// VkOffset3D                  srcOffset;
				makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, caseDef.numLayers),	// VkImageSubresourceLayers    dstSubresource;
				makeOffset3D(0, 0, 0),															// VkOffset3D                  dstOffset;
				makeExtent3D(caseDef.attachmentSize)											// VkExtent3D                  extent;
			};

			vk.cmdResolveImage(*cmdBuffer, *msColorImage, VK_IMAGE_LAYOUT_GENERAL, *colorImage, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
		}

		// copy colorImage to host visible colorBuffer
		{
			const VkImageMemoryBarrier	imageBarriers[]		=
			{
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,														// VkStructureType			sType;
					DE_NULL,																					// const void*				pNext;
					(vk::VkAccessFlags)(caseDef.multisample ? VK_ACCESS_TRANSFER_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
					VK_ACCESS_TRANSFER_READ_BIT,																// VkAccessFlags			dstAccessMask;
					caseDef.multisample ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout			oldLayout;
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,														// VkImageLayout			newLayout;
					VK_QUEUE_FAMILY_IGNORED,																	// deUint32					srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,																	// deUint32					destQueueFamilyIndex;
					*colorImage,																				// VkImage					image;
					makeColorSubresourceRange(0, caseDef.numLayers)												// VkImageSubresourceRange	subresourceRange;
				}
			};

			vk.cmdPipelineBarrier(*cmdBuffer, caseDef.multisample ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
				0u, DE_NULL, 0u, DE_NULL, 1u, imageBarriers);

			const VkBufferImageCopy		region				=
			{
				0ull,																				// VkDeviceSize                bufferOffset;
				0u,																					// uint32_t                    bufferRowLength;
				0u,																					// uint32_t                    bufferImageHeight;
				makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, caseDef.numLayers),	// VkImageSubresourceLayers    imageSubresource;
				makeOffset3D(0, 0, 0),																// VkOffset3D                  imageOffset;
				makeExtent3D(caseDef.attachmentSize),												// VkExtent3D                  imageExtent;
			};

			vk.cmdCopyImageToBuffer(*cmdBuffer, *colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorBuffer, 1u, &region);

			const VkBufferMemoryBarrier	bufferBarriers[]	=
			{
				{
					VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType    sType;
					DE_NULL,									// const void*        pNext;
					VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags      srcAccessMask;
					VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags      dstAccessMask;
					VK_QUEUE_FAMILY_IGNORED,					// uint32_t           srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,					// uint32_t           dstQueueFamilyIndex;
					*colorBuffer,								// VkBuffer           buffer;
					0ull,										// VkDeviceSize       offset;
					VK_WHOLE_SIZE,								// VkDeviceSize       size;
				},
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u,
				0u, DE_NULL, DE_LENGTH_OF_ARRAY(bufferBarriers), bufferBarriers, 0u, DE_NULL);
		}
	} // beginCommandBuffer

	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// Verify results
	{
		invalidateAlloc(vk, device, *colorBufferAlloc);
		const tcu::TextureFormat			format			= mapVkFormat(COLOR_FORMAT);
		const int							depth			= deMax32(caseDef.attachmentSize.z(), caseDef.numLayers);
		tcu::TextureLevel					textureLevel	(format, caseDef.attachmentSize.x(), caseDef.attachmentSize.y(), depth);
		const tcu::PixelBufferAccess		expectedImage	= getExpectedData(textureLevel, caseDef);
		const tcu::ConstPixelBufferAccess	resultImage		(format, caseDef.attachmentSize.x(), caseDef.attachmentSize.y(), depth, colorBufferAlloc->getHostPtr());

		if (!tcu::intThresholdCompare(context.getTestContext().getLog(), "Image Comparison", "", expectedImage, resultImage, tcu::UVec4(1), tcu::COMPARE_LOG_RESULT))
			return tcu::TestStatus::fail("Fail");
	}

	return tcu::TestStatus::pass("Pass");
}

struct NoAttCaseDef
{
	PipelineConstructionType	pipelineConstructionType;
	bool						multisample;
};

void initImagePrograms (SourceCollections& programCollection, const NoAttCaseDef caseDef)
{
	// Vertex shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in vec4 in_position;\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "	vec4 gl_Position;\n"
			<< "	float gl_PointSize;\n"
			<< "};\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "	gl_Position	= in_position;\n"
			<< "	gl_PointSize = 1.0f;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(binding = 0, rgba8) writeonly uniform image2D image;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n";
			if (!caseDef.multisample)
				src << "    imageStore(image, ivec2(gl_PrimitiveID % 4, 0), vec4(1.0, 0.5, 0.25, 1.0));\n";
			else
				src << "    imageStore(image, ivec2(gl_PrimitiveID % 4, gl_SampleID % 4), vec4(1.0, 0.5, 0.25, 1.0));\n";
			src << "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

//! Make a render pass with no attachments
RenderPassWrapper makeRenderPassNoAtt (const DeviceInterface& vk, const VkDevice device, const PipelineConstructionType pipelineConstructionType)
{
	// Create a single subpass with no attachment references
	vector<VkSubpassDescription>	subpasses;

	const VkSubpassDescription		subpassDescription	=
	{
		(VkSubpassDescriptionFlags)0,		// VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint				pipelineBindPoint;
		0u,									// deUint32							inputAttachmentCount;
		DE_NULL,							// const VkAttachmentReference*		pInputAttachments;
		0u,									// deUint32							colorAttachmentCount;
		DE_NULL,							// const VkAttachmentReference*		pColorAttachments;
		DE_NULL,							// const VkAttachmentReference*		pResolveAttachments;
		DE_NULL,							// const VkAttachmentReference*		pDepthStencilAttachment;
		0u,									// deUint32							preserveAttachmentCount;
		DE_NULL								// const deUint32*					pPreserveAttachments;
	};
	subpasses.push_back(subpassDescription);

	const VkRenderPassCreateInfo	renderPassInfo	=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,									// const void*						pNext;
		(VkRenderPassCreateFlags)0,					// VkRenderPassCreateFlags			flags;
		0,											// deUint32							attachmentCount;
		DE_NULL,									// const VkAttachmentDescription*	pAttachments;
		1,											// deUint32							subpassCount;
		&subpasses[0],								// const VkSubpassDescription*		pSubpasses;
		0u,											// deUint32							dependencyCount;
		DE_NULL										// const VkSubpassDependency*		pDependencies;
	};

	return RenderPassWrapper(pipelineConstructionType, vk, device, &renderPassInfo);
}

tcu::PixelBufferAccess getExpectedDataNoAtt (tcu::TextureLevel& textureLevel)
{
	const tcu::PixelBufferAccess	expectedImage	(textureLevel);
	for (int z = 0; z < expectedImage.getDepth(); ++z)
	{
		for (int y = 0; y < expectedImage.getHeight(); ++y)
		{
			for (int x = 0; x < expectedImage.getWidth(); ++x)
			{
				expectedImage.setPixel(tcu::Vec4(1.0, 0.5, 0.25, 1.0), x, y, z);
			}
		}
	}
	return expectedImage;
}

vector<tcu::Vec4> genPointVertices (void)
{
	vector<tcu::Vec4>	vectorData;
	vectorData.push_back(Vec4(-0.25f, -0.25f, 0, 1));
	vectorData.push_back(Vec4(-0.25f,  0.25f, 0, 1));
	vectorData.push_back(Vec4(0.25f, -0.25f, 0, 1));
	vectorData.push_back(Vec4(0.25f,  0.25f, 0, 1));
	return vectorData;
}

// Tests rendering to a framebuffer without color attachments, checking that
// the fragment shader is run even in the absence of color output. In this case
// we render 4 point primitives and we make the fragment shader write to a
// different pixel of an image via an imageStore command. For the single-sampled
// configuration we use a 4x1 image to record the output and for the
// multi-sampled case we use a 4x4 image to record all 16 samples produced by
// 4-sample multi-sampling
tcu::TestStatus testNoAtt (Context& context, const NoAttCaseDef caseDef)
{
	const InstanceInterface&			vki						= context.getInstanceInterface();
	const DeviceInterface&				vk						= context.getDeviceInterface();
	const VkPhysicalDevice				physicalDevice			= context.getPhysicalDevice();
	const VkDevice						device					= context.getDevice();
	const VkQueue						queue					= context.getUniversalQueue();
	const deUint32						queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	Allocator&							allocator				= context.getDefaultAllocator();
	const IVec3							renderSize				(32, 32, 1);

	Move<VkBuffer>						vertexBuffer;
	MovePtr<Allocation>					vertexBufferAlloc;

	const ShaderWrapper					vertexModule			(ShaderWrapper (vk, device, context.getBinaryCollection().get("vert"), 0u));
	const ShaderWrapper					fragmentModule			(ShaderWrapper (vk, device, context.getBinaryCollection().get("frag"), 0u));

	// Create image where we will record the writes. For single-sampled cases this is a 4x1 image
	// and for multi-sampled cases this is a 4x<num_samples> image.
	const deUint8						numSamples				= caseDef.multisample ? 4 : 1;
	const deUint8						imageWidth				= 4;
	const deUint8						imageHeight				= numSamples;
	const deUint8						imageDepth				= 1;
	const deUint8						imageLayers				= 1;
	const IVec3							imageDim				= IVec3(imageWidth, imageHeight, imageDepth);
	const VkImageUsageFlags				imageUsage				= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	const Move<VkImage>					image					= makeImage(vk, device, VkImageViewCreateFlags(0), VK_IMAGE_TYPE_2D, COLOR_FORMAT, imageDim, imageLayers, imageUsage, false);
	const VkImageSubresourceRange		imageSubresourceRange	= makeColorSubresourceRange(0u, imageLayers);
	const MovePtr<Allocation>			imageAlloc				= bindImage(vk, device, allocator, *image, MemoryRequirement::Any);
	const Move<VkImageView>				imageView				= makeImageView(vk, device, *image, VK_IMAGE_VIEW_TYPE_2D, COLOR_FORMAT, imageSubresourceRange);

	// Create a buffer where we will copy the image for verification
	const VkDeviceSize					colorBufferSize		= imageWidth * imageHeight * imageDepth * numSamples * tcu::getPixelSize(mapVkFormat(COLOR_FORMAT));
	const Unique<VkBuffer>				colorBuffer			(makeBuffer(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>			colorBufferAlloc	(bindBuffer(vk, device, allocator, *colorBuffer, MemoryRequirement::HostVisible));

	// Create pipeline descriptor set for the image
	const Move<VkDescriptorSetLayout>	descriptorSetLayout		= DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(vk, device);

	const Move<VkDescriptorPool>		descriptorPool			= DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);

	const Move<VkDescriptorSet>			descriptorSet			= makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);
	const VkDescriptorImageInfo			descriptorImageInfo		= makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);
	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfo)
		.update(vk, device);

	const PipelineLayoutWrapper			pipelineLayout			(caseDef.pipelineConstructionType, vk, device, *descriptorSetLayout);
	GraphicsPipelineWrapper				pipeline				(vki, vk, physicalDevice, device, context.getDeviceExtensions(), caseDef.pipelineConstructionType);
	RenderPassWrapper					renderPass				= makeRenderPassNoAtt(vk, device, caseDef.pipelineConstructionType);

	const Unique<VkCommandPool>			cmdPool					(createCommandPool (vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer				(makeCommandBuffer (vk, device, *cmdPool));

	// create vertexBuffer
	{
		const vector<tcu::Vec4>	vertices			= genPointVertices();
		const VkDeviceSize		vertexBufferSize	= sizeInBytes(vertices);

		vertexBuffer		= makeBuffer(vk, device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		vertexBufferAlloc	= bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible);
		deMemcpy(vertexBufferAlloc->getHostPtr(), &vertices[0], static_cast<std::size_t>(vertexBufferSize));
		flushAlloc(vk, device, *vertexBufferAlloc);
	}

	// Create pipeline
	preparePipelineWrapper(pipeline, pipelineLayout, *renderPass, vertexModule, fragmentModule,
						   renderSize, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, 0, 0u, caseDef.multisample);
	renderPass.createFramebuffer(vk, device, 0, DE_NULL, renderSize.x(), renderSize.y());

	// Record command buffer
	beginCommandBuffer(vk, *cmdBuffer);
	{
		// shader image layout transition undefined -> general
		{
			const VkImageMemoryBarrier setImageLayoutBarrier = makeImageMemoryBarrier(
				0u, VK_ACCESS_SHADER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
				*image, imageSubresourceRange);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, DE_NULL, 0, DE_NULL, 1, &setImageLayoutBarrier);
		}

		// Render pass
		{
			const VkDeviceSize vertexBufferOffset = 0ull;

			renderPass.begin(vk, *cmdBuffer, makeRect2D(0, 0, renderSize.x(), renderSize.y()));

			pipeline.bind(*cmdBuffer);
			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
			vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);
			vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);

			renderPass.end(vk, *cmdBuffer);
		}

		// copy image to host visible colorBuffer
		{
			const VkImageMemoryBarrier	imageBarriers[]		=
			{
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
					DE_NULL,									// const void*				pNext;
					VK_ACCESS_SHADER_WRITE_BIT,					// VkAccessFlags			srcAccessMask;
					VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask;
					VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout			oldLayout;
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout			newLayout;
					VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,					// deUint32					destQueueFamilyIndex;
					*image,										// VkImage					image;
					makeColorSubresourceRange(0, 1)				// VkImageSubresourceRange	subresourceRange;
				}
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
				0u, DE_NULL, 0u, DE_NULL, 1u, imageBarriers);

			const VkBufferImageCopy		region				=
			{
				0ull,																// VkDeviceSize                bufferOffset;
				0u,																	// uint32_t                    bufferRowLength;
				0u,																	// uint32_t                    bufferImageHeight;
				makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1),	// VkImageSubresourceLayers    imageSubresource;
				makeOffset3D(0, 0, 0),												// VkOffset3D                  imageOffset;
				makeExtent3D(IVec3(imageWidth, imageHeight, imageDepth)),			// VkExtent3D                  imageExtent;
			};

			vk.cmdCopyImageToBuffer(*cmdBuffer, *image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorBuffer, 1u, &region);

			const VkBufferMemoryBarrier	bufferBarriers[]	=
			{
				{
					VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType    sType;
					DE_NULL,									// const void*        pNext;
					VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags      srcAccessMask;
					VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags      dstAccessMask;
					VK_QUEUE_FAMILY_IGNORED,					// uint32_t           srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,					// uint32_t           dstQueueFamilyIndex;
					*colorBuffer,								// VkBuffer           buffer;
					0ull,										// VkDeviceSize       offset;
					VK_WHOLE_SIZE,								// VkDeviceSize       size;
				},
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u,
				0u, DE_NULL, DE_LENGTH_OF_ARRAY(bufferBarriers), bufferBarriers, 0u, DE_NULL);
		}
	} // beginCommandBuffer

	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// Verify results
	{
		invalidateAlloc(vk, device, *colorBufferAlloc);
		const tcu::TextureFormat			format			= mapVkFormat(COLOR_FORMAT);
		tcu::TextureLevel					textureLevel	(format, imageWidth, imageHeight, imageDepth);
		const tcu::PixelBufferAccess		expectedImage	= getExpectedDataNoAtt(textureLevel);
		const tcu::ConstPixelBufferAccess	resultImage		(format, imageWidth, imageHeight, imageDepth, colorBufferAlloc->getHostPtr());

		if (!tcu::intThresholdCompare(context.getTestContext().getLog(), "Image Comparison", "", expectedImage, resultImage, tcu::UVec4(1), tcu::COMPARE_LOG_RESULT))
			return tcu::TestStatus::fail("Fail");
	}

	return tcu::TestStatus::pass("Pass");
}

//! Make a render pass with three color attachments
RenderPassWrapper makeRenderPassMultiAttachments	(const DeviceInterface&			vk,
													 const VkDevice					device,
													 const PipelineConstructionType pipelineConstructionType,
													 const VkFormat					colorFormat,
													 deUint32						numAttachments,
													 const bool						multisample)
{
	vector<VkAttachmentDescription>	attachmentDescriptions		(numAttachments);
	vector<VkAttachmentReference>	colorAttachmentReferences	(numAttachments);

	for (deUint32 i = 0; i < numAttachments; i++)
	{
		VkAttachmentDescription colorAttachmentDescription =
		{
			(VkAttachmentDescriptionFlags)0,								// VkAttachmentDescriptionFla	flags;
			colorFormat,													// VkFormat						format;
			!multisample ? VK_SAMPLE_COUNT_1_BIT : VK_SAMPLE_COUNT_4_BIT,	// VkSampleCountFlagBits		samples;
			VK_ATTACHMENT_LOAD_OP_LOAD,										// VkAttachmentLoadOp			loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp			storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,								// VkAttachmentLoadOp			stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,								// VkAttachmentStoreOp			stencilStoreOp;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,						// VkImageLayout				initialLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,						// VkImageLayout				finalLayout;
		};
		attachmentDescriptions[i] = colorAttachmentDescription;

		const VkAttachmentReference attachmentRef =
		{
			i,											// deUint32			attachment;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout;
		};
		colorAttachmentReferences[i] = attachmentRef;
	}

	const VkSubpassDescription subpassDescription =
	{
		(VkSubpassDescriptionFlags)0,		// VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint				pipelineBindPoint;
		0u,									// deUint32							inputAttachmentCount;
		DE_NULL,							// const VkAttachmentReference*		pInputAttachments;
		numAttachments,						// deUint32							colorAttachmentCount;
		&colorAttachmentReferences[0],		// const VkAttachmentReference*		pColorAttachments;
		DE_NULL,							// const VkAttachmentReference*		pResolveAttachments;
		DE_NULL,							// const VkAttachmentReference*		pDepthStencilAttachment;
		0u,									// deUint32							preserveAttachmentCount;
		DE_NULL								// const deUint32*					pPreserveAttachments;
	};

	const VkRenderPassCreateInfo renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,									// const void*						pNext;
		(VkRenderPassCreateFlags)0,					// VkRenderPassCreateFlags			flags;
		numAttachments,								// deUint32							attachmentCount;
		&attachmentDescriptions[0],					// const VkAttachmentDescription*	pAttachments;
		1u,											// deUint32							subpassCount;
		&subpassDescription,						// const VkSubpassDescription*		pSubpasses;
		0u,											// deUint32							dependencyCount;
		DE_NULL										// const VkSubpassDependency*		pDependencies;
	};

	return RenderPassWrapper(pipelineConstructionType, vk, device, &renderPassInfo);
}

// Tests framebuffer with attachments of different sizes
tcu::TestStatus testMultiAttachments (Context& context, const CaseDef caseDef)
{
	const InstanceInterface&		vki					= context.getInstanceInterface();
	const DeviceInterface&			vk					= context.getDeviceInterface();
	const VkPhysicalDevice			physicalDevice		= context.getPhysicalDevice();
	const VkDevice					device				= context.getDevice();
	const VkQueue					queue				= context.getUniversalQueue();
	const deUint32					queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	Allocator&						allocator			= context.getDefaultAllocator();
	const deUint32					numRenderTargets	= 3;
	const deBool					differentSizeTest	= caseDef.multiAttachmentsTestType == MULTI_ATTACHMENTS_DIFFERENT_SIZES;
	const deBool					notExportTest		= caseDef.multiAttachmentsTestType == MULTI_ATTACHMENTS_NOT_EXPORTED;

	// Color images for rendering in single-sample tests or resolve targets for multi-sample tests
	Move<VkImage>					colorImages[numRenderTargets];
	MovePtr<Allocation>				colorImageAllocs[numRenderTargets];

	// For multisampled tests, these are the rendering targets
	Move<VkImage>					msColorImages[numRenderTargets];
	MovePtr<Allocation>				msColorImageAllocs[numRenderTargets];

	Move<VkBuffer>					colorBuffers[numRenderTargets];
	MovePtr<Allocation>				colorBufferAllocs[numRenderTargets];

	// Vary attachment sizes by adding an offset to the base size.
	const IVec3						attachmentSizes[]	=
	{
		caseDef.attachmentSize,
		caseDef.attachmentSize + IVec3(10, caseDef.attachmentSize.y() == 1 ? 0 : 15, 0),
		caseDef.attachmentSize + IVec3(27, caseDef.attachmentSize.y() == 1 ? 0 : 4, 0)
	};

	// Use unique clear color for each render target to verify no leaking happens between render target clears.
	const VkClearColorValue			clearColors[]		=
	{
		{{1.0f, 0.0f, 0.0f, 1.0f}},
		{{0.0f, 1.0f, 0.0f, 1.0f}},
		{{0.0f, 0.0f, 1.0f, 1.0f}}
	};

	Move<VkBuffer>					vertexBuffer;
	MovePtr<Allocation>				vertexBufferAlloc;

	vector<SharedPtrVkImageView>	colorAttachments;
	vector<VkImage>					images;
	vector<VkImageView>				attachmentHandles;

	const PipelineLayoutWrapper		pipelineLayout		(caseDef.pipelineConstructionType, vk, device);
	GraphicsPipelineWrapper			pipeline			(vki, vk, physicalDevice, device, context.getDeviceExtensions(), caseDef.pipelineConstructionType);
	RenderPassWrapper				renderPass			(makeRenderPassMultiAttachments(vk, device, caseDef.pipelineConstructionType, COLOR_FORMAT, numRenderTargets, caseDef.multisample));

	const ShaderWrapper				vertexModule		(ShaderWrapper(vk, device, context.getBinaryCollection().get("vert"), 0u));
	const ShaderWrapper				fragmentModule		(ShaderWrapper(vk, device, context.getBinaryCollection().get("frag"), 0u));

	const Unique<VkCommandPool>		cmdPool				(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer			(makeCommandBuffer(vk, device, *cmdPool));

	const VkImageViewType			imageViewType		= caseDef.imageType == VK_IMAGE_VIEW_TYPE_CUBE || caseDef.imageType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
		? VK_IMAGE_VIEW_TYPE_2D : caseDef.imageType;

	const VkImageSubresourceRange	range				= makeColorSubresourceRange(0, 1);

	// create color buffers
	for (deUint32 renderTargetIdx = 0; renderTargetIdx < numRenderTargets; renderTargetIdx++)
	{
		const IVec3 attachmentSize = differentSizeTest ? attachmentSizes[renderTargetIdx] : caseDef.attachmentSize;

		// Host memory buffer where we will copy the rendered image for verification
		const deUint32					att_size_x			= attachmentSize.x();
		const deUint32					att_size_y			= attachmentSize.y();
		const deUint32					att_size_z			= attachmentSize.z();
		const VkDeviceSize				colorBufferSize		= att_size_x * att_size_y * att_size_z * tcu::getPixelSize(mapVkFormat(COLOR_FORMAT));
		colorBuffers[renderTargetIdx]						= makeBuffer(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		colorBufferAllocs[renderTargetIdx]					= bindBuffer(vk, device, allocator, *colorBuffers[renderTargetIdx], MemoryRequirement::HostVisible);
	}

	// create vertexBuffer
	{
		const vector<tcu::Vec4>	vertices			= genFullQuadVertices(1);
		const VkDeviceSize		vertexBufferSize	= sizeInBytes(vertices);

		vertexBuffer								= makeBuffer(vk, device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		vertexBufferAlloc							= bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible);

		deMemcpy(vertexBufferAlloc->getHostPtr(), &vertices[0], static_cast<std::size_t>(vertexBufferSize));
		flushAlloc(vk, device, *vertexBufferAlloc);
	}

	// create colorImages (and msColorImages) using the configured attachmentsize
	for (deUint32 renderTargetIdx = 0; renderTargetIdx < numRenderTargets; renderTargetIdx++)
	{
		const IVec3 attachmentSize = differentSizeTest ? attachmentSizes[renderTargetIdx] : caseDef.attachmentSize;

		const VkImageUsageFlags	colorImageUsage	= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		colorImages[renderTargetIdx]			= makeImage(vk, device, VkImageViewCreateFlags(0), getImageType(caseDef.imageType), COLOR_FORMAT,
			attachmentSize, 1, colorImageUsage, false);
		colorImageAllocs[renderTargetIdx]		= bindImage(vk, device, allocator, *colorImages[renderTargetIdx], MemoryRequirement::Any);

		if (caseDef.multisample)
		{
			const VkImageUsageFlags	msImageUsage	= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

			msColorImages[renderTargetIdx]			= makeImage(vk, device, VkImageViewCreateFlags(0), getImageType(caseDef.imageType), COLOR_FORMAT, attachmentSize, 1, msImageUsage, true);
			msColorImageAllocs[renderTargetIdx]		= bindImage(vk, device, allocator, *msColorImages[renderTargetIdx], MemoryRequirement::Any);
		}
	}

	// create attachmentHandles. We use the renderSize for viewport and scissor
	for (deUint32 renderTargetIdx = 0; renderTargetIdx < numRenderTargets; renderTargetIdx++)
	{
		colorAttachments.push_back(makeSharedPtr(makeImageView(vk, device, ! caseDef.multisample ? *colorImages[renderTargetIdx] : *msColorImages[renderTargetIdx], imageViewType, COLOR_FORMAT, range)));
		images.push_back(! caseDef.multisample ? *colorImages[renderTargetIdx] : *msColorImages[renderTargetIdx]);
		attachmentHandles.push_back(**colorAttachments.back());
	}

	preparePipelineWrapper(pipeline, pipelineLayout, *renderPass, vertexModule, fragmentModule, caseDef.renderSize, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0, numRenderTargets, caseDef.multisample);

	// create framebuffer
	renderPass.createFramebuffer(vk, device, numRenderTargets, &images[0], &attachmentHandles[0], static_cast<deUint32>(caseDef.renderSize.x()), static_cast<deUint32>(caseDef.renderSize.y()));

	// record command buffer
	beginCommandBuffer(vk, *cmdBuffer);

	// Clear image attachments
	for (deUint32 renderTargetIdx = 0; renderTargetIdx < numRenderTargets; renderTargetIdx++)
	{
		{
			const VkImageMemoryBarrier	imageLayoutBarriers[]	=
			{
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,													// VkStructureType			sType;
					DE_NULL,																				// const void*				pNext;
					0u,																						// VkAccessFlags			srcAccessMask;
					VK_ACCESS_TRANSFER_WRITE_BIT,															// VkAccessFlags			dstAccessMask;
					VK_IMAGE_LAYOUT_UNDEFINED,																// VkImageLayout			oldLayout;
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,													// VkImageLayout			newLayout;
					VK_QUEUE_FAMILY_IGNORED,																// deUint32					srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,																// deUint32					destQueueFamilyIndex;
					caseDef.multisample ? *msColorImages[renderTargetIdx] : *colorImages[renderTargetIdx],	// VkImage					image;
					range																					// VkImageSubresourceRange	subresourceRange;
				},
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
				0u, DE_NULL, 0u, DE_NULL, 1u, imageLayoutBarriers);

			vk.cmdClearColorImage(*cmdBuffer, caseDef.multisample ? *msColorImages[renderTargetIdx] : *colorImages[renderTargetIdx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColors[renderTargetIdx], 1u, &range);

			const VkImageMemoryBarrier	imageClearBarriers[]	=
			{
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,													// VkStructureType			sType;
					DE_NULL,																				// const void*				pNext;
					VK_ACCESS_TRANSFER_WRITE_BIT,															// VkAccessFlags			srcAccessMask;
					VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,													// VkAccessFlags			dstAccessMask;
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,													// VkImageLayout			oldLayout;
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,												// VkImageLayout			newLayout;
					VK_QUEUE_FAMILY_IGNORED,																// deUint32					srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,																// deUint32					destQueueFamilyIndex;
					caseDef.multisample ? *msColorImages[renderTargetIdx] : *colorImages[renderTargetIdx],	// VkImage					image;
					range																					// VkImageSubresourceRange	subresourceRange;
				},
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, imageClearBarriers);
		}
	}

	// Render pass: this should render only to the area defined by renderSize (smaller than the size of the attachment)
	{
		const VkDeviceSize			vertexBufferOffset	= 0ull;

		renderPass.begin(vk, *cmdBuffer, makeRect2D(0, 0, caseDef.renderSize.x(), caseDef.renderSize.y()));
		{
			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
			pipeline.bind(*cmdBuffer);
			vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);
		}
		renderPass.end(vk, *cmdBuffer);
	}

	// If we are using a multi-sampled render target (msColorImage), resolve it now (to colorImage)
	if (caseDef.multisample)
	{
		for (deUint32 renderTargetIdx = 0; renderTargetIdx < numRenderTargets; renderTargetIdx++)
		{
			const IVec3 attachmentSize = differentSizeTest ? attachmentSizes[renderTargetIdx] : caseDef.attachmentSize;

			// Transition msColorImage (from layout COLOR_ATTACHMENT_OPTIMAL) and colorImage (from layout UNDEFINED) to layout GENERAL before resolving
			const VkImageMemoryBarrier	imageBarriers[]	=
			{
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
					DE_NULL,									// const void*				pNext;
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,		// VkAccessFlags			srcAccessMask;
					VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags			dstAccessMask;
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout			oldLayout;
					VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout			newLayout;
					VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,					// deUint32					destQueueFamilyIndex;
					*msColorImages[renderTargetIdx],			// VkImage					image;
					range										// VkImageSubresourceRange	subresourceRange;
				},
				{
					VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,	// VkStructureType			sType;
					DE_NULL,								// const void*				pNext;
					(VkAccessFlags)0,						// VkAccessFlags			srcAccessMask;
					VK_ACCESS_TRANSFER_WRITE_BIT,			// VkAccessFlags			dstAccessMask;
					VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			oldLayout;
					VK_IMAGE_LAYOUT_GENERAL,				// VkImageLayout			newLayout;
					VK_QUEUE_FAMILY_IGNORED,				// deUint32					srcQueueFamilyIndex;
					VK_QUEUE_FAMILY_IGNORED,				// deUint32					destQueueFamilyIndex;
					*colorImages[renderTargetIdx],			// VkImage					image;
					range									// VkImageSubresourceRange	subresourceRange;
				}
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
				0u, DE_NULL, 0u, DE_NULL, 2u, imageBarriers);

			const VkImageResolve	region	=
			{
				makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1),	// VkImageSubresourceLayers    srcSubresource;
				makeOffset3D(0, 0, 0),											// VkOffset3D                  srcOffset;
				makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1),	// VkImageSubresourceLayers    dstSubresource;
				makeOffset3D(0, 0, 0),											// VkOffset3D                  dstOffset;
				makeExtent3D(attachmentSize)									// VkExtent3D                  extent;
			};

			vk.cmdResolveImage(*cmdBuffer, *msColorImages[renderTargetIdx], VK_IMAGE_LAYOUT_GENERAL, *colorImages[renderTargetIdx], VK_IMAGE_LAYOUT_GENERAL, 1, &region);
		}
	}

	for (deUint32 renderTargetIdx = 0; renderTargetIdx < numRenderTargets; renderTargetIdx++)
	{
		const IVec3 attachmentSize = differentSizeTest ? attachmentSizes[renderTargetIdx] : caseDef.attachmentSize;

		// copy colorImage to host visible colorBuffer
		const VkImageMemoryBarrier	imageBarrier		=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,														// VkStructureType			sType;
			DE_NULL,																					// const void*				pNext;
			(vk::VkAccessFlags)(caseDef.multisample ? VK_ACCESS_TRANSFER_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
			VK_ACCESS_TRANSFER_READ_BIT,																// VkAccessFlags			dstAccessMask;
			caseDef.multisample ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,														// VkImageLayout			newLayout;
			VK_QUEUE_FAMILY_IGNORED,																	// deUint32					srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,																	// deUint32					destQueueFamilyIndex;
			*colorImages[renderTargetIdx],																// VkImage					image;
			range																						// VkImageSubresourceRange	subresourceRange;
		};

		vk.cmdPipelineBarrier(*cmdBuffer, caseDef.multisample ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
			0u, DE_NULL, 0u, DE_NULL, 1u, &imageBarrier);

		const VkBufferImageCopy		region				=
		{
			0ull,																// VkDeviceSize                bufferOffset;
			0u,																	// uint32_t                    bufferRowLength;
			0u,																	// uint32_t                    bufferImageHeight;
			makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u),	// VkImageSubresourceLayers    imageSubresource;
			makeOffset3D(0, 0, 0),												// VkOffset3D                  imageOffset;
			makeExtent3D(attachmentSize),										// VkExtent3D                  imageExtent;
		};

		vk.cmdCopyImageToBuffer(*cmdBuffer, *colorImages[renderTargetIdx], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorBuffers[renderTargetIdx], 1u, &region);

		const VkBufferMemoryBarrier	bufferBarrier		=
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType    sType;
			DE_NULL,									// const void*        pNext;
			VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags      srcAccessMask;
			VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags      dstAccessMask;
			VK_QUEUE_FAMILY_IGNORED,					// uint32_t           srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,					// uint32_t           dstQueueFamilyIndex;
			*colorBuffers[renderTargetIdx],				// VkBuffer           buffer;
			0ull,										// VkDeviceSize       offset;
			VK_WHOLE_SIZE,								// VkDeviceSize       size;
		};

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u,
			0u, DE_NULL, 1u, &bufferBarrier, 0u, DE_NULL);
	}

	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// Verify results
	const deUint32						skippedRenderTarget	= notExportTest ? 1 : numRenderTargets;
	const tcu::Vec4						expectedColors[]	=
	{
		tcu::Vec4(1.0f, 0.5f, 0.25f, 1.0f),
		tcu::Vec4(0.5f, 1.0f, 0.25f, 1.0f),
		tcu::Vec4(0.25f, 0.5f, 1.0, 1.0f)
	};

	for (deUint32 renderTargetIdx = 0; renderTargetIdx < numRenderTargets; renderTargetIdx++)
	{
		const tcu::TextureFormat			format				= mapVkFormat(COLOR_FORMAT);
		const IVec3							size				= differentSizeTest ? attachmentSizes[renderTargetIdx] : caseDef.attachmentSize;
		tcu::TextureLevel					textureLevel		(format, size.x(), size.y(), size.z());
		const tcu::PixelBufferAccess		expectedImage		(textureLevel);

		// Doesn't need to check the output of unused MRT, that may be undefined.
		if (notExportTest && (renderTargetIdx==skippedRenderTarget))
			continue;

		invalidateAlloc(vk, device, *colorBufferAllocs[renderTargetIdx]);

		for (int z = 0; z < expectedImage.getDepth(); ++z)
		{
			for (int y = 0; y < expectedImage.getHeight(); ++y)
			{
				for (int x = 0; x < expectedImage.getWidth(); ++x)
				{
					if (x < caseDef.renderSize.x() && y < caseDef.renderSize.y() && z < caseDef.renderSize.z())
						expectedImage.setPixel(expectedColors[renderTargetIdx], x, y, z);
					else
						expectedImage.setPixel(tcu::Vec4(clearColors[renderTargetIdx].float32), x, y, z);
				}
			}
		}
		const tcu::ConstPixelBufferAccess	resultImage		(format, size.x(), size.y(), size.z(), colorBufferAllocs[renderTargetIdx]->getHostPtr());

		if (!tcu::intThresholdCompare(context.getTestContext().getLog(), (std::string("Image Comparison of render target ") + de::toString(renderTargetIdx)).c_str(), "", expectedImage, resultImage, tcu::UVec4(1), tcu::COMPARE_LOG_RESULT))
			return tcu::TestStatus::fail("Fail");
	}

	return tcu::TestStatus::pass("Pass");
}

void initInputResolveSameAttachmentPrograms (SourceCollections& programCollection, const CaseDef caseDef)
{
	DE_UNREF(caseDef);

	// Vertex shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in vec4 in_position;\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "	vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "	gl_Position	= in_position;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout (input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inputColor;\n"
			<< "layout(location = 0) out vec4 o_color0;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    vec4 in_color = subpassLoad(inputColor);\n"
			<< "    o_color0 = vec4(1.0, in_color.y, 0.25, 1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

RenderPassWrapper makeRenderPassInputResolveSameAttachment	(const DeviceInterface&			vk,
															 const VkDevice					device,
															 const PipelineConstructionType pipelineConstructionType,
															 const VkFormat					colorFormat)
{
	std::vector<VkAttachmentDescription> attachmentDescriptions;
	VkAttachmentDescription colorAttachmentDescription =
	{
		(VkAttachmentDescriptionFlags)0,								// VkAttachmentDescriptionFla	flags;
		colorFormat,													// VkFormat						format;
		VK_SAMPLE_COUNT_4_BIT,											// VkSampleCountFlagBits		samples;
		VK_ATTACHMENT_LOAD_OP_LOAD,										// VkAttachmentLoadOp			loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp			storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,								// VkAttachmentLoadOp			stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,								// VkAttachmentStoreOp			stencilStoreOp;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,						// VkImageLayout				initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,						// VkImageLayout				finalLayout;
	};

	attachmentDescriptions.push_back(colorAttachmentDescription);

	VkAttachmentDescription inputAttachmentDescription =
	{
		(VkAttachmentDescriptionFlags)0,								// VkAttachmentDescriptionFla	flags;
		colorFormat,													// VkFormat						format;
		VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagBits		samples;
		VK_ATTACHMENT_LOAD_OP_LOAD,										// VkAttachmentLoadOp			loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp			storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,								// VkAttachmentLoadOp			stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,								// VkAttachmentStoreOp			stencilStoreOp;
		VK_IMAGE_LAYOUT_GENERAL,										// VkImageLayout				initialLayout;
		VK_IMAGE_LAYOUT_GENERAL,										// VkImageLayout				finalLayout;
	};

	attachmentDescriptions.push_back(inputAttachmentDescription);

	const VkAttachmentReference colorAttachmentRef =
	{
		0u,											// deUint32			attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout;
	};

	const VkAttachmentReference inputAttachmentRef =
	{
		1u,											// deUint32			attachment;
		VK_IMAGE_LAYOUT_GENERAL						// VkImageLayout	layout;
	};

	const VkSubpassDescription subpassDescription =
	{
		(VkSubpassDescriptionFlags)0,		// VkSubpassDescriptionFlags flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint pipelineBindPoint;
		1u,									// deUint32 inputAttachmentCount;
		&inputAttachmentRef,				// const VkAttachmentReference* pInputAttachments;
		1u,									// deUint32 colorAttachmentCount;
		&colorAttachmentRef,				// const VkAttachmentReference* pColorAttachments;
		&inputAttachmentRef,				// const VkAttachmentReference* pResolveAttachments;
		DE_NULL,							// const VkAttachmentReference* pDepthStencilAttachment;
		0u,									// deUint32 preserveAttachmentCount;
		DE_NULL								// const deUint32* pPreserveAttachments;
	};

	const VkRenderPassCreateInfo renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,									// const void*						pNext;
		(VkRenderPassCreateFlags)0,					// VkRenderPassCreateFlags			flags;
		(deUint32)attachmentDescriptions.size(),	// deUint32							attachmentCount;
		&attachmentDescriptions[0],					// const VkAttachmentDescription*	pAttachments;
		1u,											// deUint32							subpassCount;
		&subpassDescription,						// const VkSubpassDescription*		pSubpasses;
		0u,											// deUint32							dependencyCount;
		DE_NULL										// const VkSubpassDependency*		pDependencies;
	};

	return RenderPassWrapper(pipelineConstructionType, vk, device, &renderPassInfo);
}

tcu::TestStatus testInputResolveSameAttachment(Context &context, const CaseDef caseDef)
{
	const InstanceInterface&		vki					= context.getInstanceInterface();
	const DeviceInterface&			vk					= context.getDeviceInterface();
	const VkPhysicalDevice			physicalDevice		= context.getPhysicalDevice();
	const VkDevice					device				= context.getDevice();
	const VkQueue					queue				= context.getUniversalQueue();
	const deUint32					queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	Allocator&						allocator			= context.getDefaultAllocator();

	// Use unique clear color for each render target to verify no leaking happens between render target clears.
	const VkClearColorValue			clearColor[] =
	{
			{{ 1.0f, 0.0f, 0.0f, 1.0f }},
			{{ 0.0f, 0.5f, 0.0f, 1.0f }}
	};

	Move<VkBuffer>					vertexBuffer;
	MovePtr<Allocation>				vertexBufferAlloc;

	vector<SharedPtrVkImageView>	colorAttachments;
	vector<VkImage>					images;
	vector<VkImageView>				attachmentHandles;

	// Create pipeline descriptor set for the image
	const Move<VkDescriptorSetLayout>	descriptorSetLayout		= DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(vk, device);

	const Move<VkDescriptorPool>		descriptorPool			= DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);

	const Move<VkDescriptorSet>			descriptorSet			= makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);

	const PipelineLayoutWrapper			pipelineLayout			(caseDef.pipelineConstructionType, vk, device, *descriptorSetLayout);
	GraphicsPipelineWrapper				pipeline				(vki, vk, physicalDevice, device, context.getDeviceExtensions(), caseDef.pipelineConstructionType);
	RenderPassWrapper					renderPass				(makeRenderPassInputResolveSameAttachment(vk, device, caseDef.pipelineConstructionType, COLOR_FORMAT));

	const ShaderWrapper					vertexModule			(ShaderWrapper(vk, device, context.getBinaryCollection().get("vert"), 0u));
	const ShaderWrapper					fragmentModule			(ShaderWrapper(vk, device, context.getBinaryCollection().get("frag"), 0u));

	const Unique<VkCommandPool>			cmdPool					(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer				(makeCommandBuffer(vk, device, *cmdPool));

	const VkImageViewType				imageViewType			= caseDef.imageType;

	const VkImageSubresourceRange		range					= makeColorSubresourceRange(0, 1);

	// create color buffer
	const IVec3							attachmentSize			= caseDef.attachmentSize;
	const VkDeviceSize					colorBufferSize			= attachmentSize.x() * attachmentSize.y() * attachmentSize.z() * tcu::getPixelSize(mapVkFormat(COLOR_FORMAT));
	auto								colorBuffer				= makeBuffer(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	MovePtr<Allocation>					colorBufferAlloc		= bindBuffer(vk, device, allocator, *colorBuffer, MemoryRequirement::HostVisible);

	// create vertexBuffer
	{
		const vector<tcu::Vec4>			vertices				= genFullQuadVertices(1);
		const VkDeviceSize				vertexBufferSize		= sizeInBytes(vertices);

		vertexBuffer											= makeBuffer(vk, device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		vertexBufferAlloc										= bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible);

		deMemcpy(vertexBufferAlloc->getHostPtr(), &vertices[0], static_cast<std::size_t>(vertexBufferSize));
		flushAlloc(vk, device, *vertexBufferAlloc);
	}

	// create colorImages (and msColorImages)
	const VkImageUsageFlags	colorImageUsage		= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
	Move<VkImage>			colorImage			= makeImage(vk, device, VkImageViewCreateFlags(0), getImageType(caseDef.imageType), COLOR_FORMAT,
															attachmentSize, 1, colorImageUsage, false);
	MovePtr<Allocation>		colorImageAlloc		= bindImage(vk, device, allocator, *colorImage, MemoryRequirement::Any);

	const VkImageUsageFlags	msImageUsage		= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	Move<VkImage>			msColorImage		= makeImage(vk, device, VkImageViewCreateFlags(0), getImageType(caseDef.imageType), COLOR_FORMAT, attachmentSize, 1, msImageUsage, true);
	MovePtr<Allocation>		msColorImageAlloc	= bindImage(vk, device, allocator, *msColorImage, MemoryRequirement::Any);

	// create attachmentHandles. We use the renderSize for viewport and scissor
	colorAttachments.push_back(makeSharedPtr(makeImageView(vk, device, *msColorImage, imageViewType, COLOR_FORMAT, range)));
	images.push_back(*msColorImage);
	attachmentHandles.push_back(**colorAttachments.back());

	colorAttachments.push_back(makeSharedPtr(makeImageView(vk, device, *colorImage, imageViewType, COLOR_FORMAT, range)));
	images.push_back(*colorImage);
	attachmentHandles.push_back(**colorAttachments.back());

	const VkDescriptorImageInfo			descriptorImageInfo		= makeDescriptorImageInfo(DE_NULL, attachmentHandles[1], VK_IMAGE_LAYOUT_GENERAL);
	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &descriptorImageInfo)
		.update(vk, device);

	preparePipelineWrapper(pipeline, pipelineLayout, *renderPass, vertexModule, fragmentModule, caseDef.renderSize, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0, 1u, true);

	// create framebuffer
	renderPass.createFramebuffer(vk, device, 2u, &images[0], &attachmentHandles[0], static_cast<deUint32>(caseDef.renderSize.x()), static_cast<deUint32>(caseDef.renderSize.y()));

	// record command buffer
	beginCommandBuffer(vk, *cmdBuffer);

	// Clear image attachments
	{
		const VkImageMemoryBarrier	imageLayoutBarriers[]	=
		{
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,													// VkStructureType			sType;
				DE_NULL,																				// const void*				pNext;
				0u,																						// VkAccessFlags			srcAccessMask;
				VK_ACCESS_TRANSFER_WRITE_BIT,															// VkAccessFlags			dstAccessMask;
				VK_IMAGE_LAYOUT_UNDEFINED,																// VkImageLayout			oldLayout;
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,													// VkImageLayout			newLayout;
				VK_QUEUE_FAMILY_IGNORED,																// deUint32					srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,																// deUint32					destQueueFamilyIndex;
				*msColorImage,																			// VkImage					image;
				range																					// VkImageSubresourceRange	subresourceRange;
			},
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,													// VkStructureType			sType;
				DE_NULL,																				// const void*				pNext;
				(VkAccessFlags)0,																		// VkAccessFlags			srcAccessMask;
				VK_ACCESS_TRANSFER_WRITE_BIT,															// VkAccessFlags			dstAccessMask;
				VK_IMAGE_LAYOUT_UNDEFINED,																// VkImageLayout			oldLayout;
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,													// VkImageLayout			newLayout;
				VK_QUEUE_FAMILY_IGNORED,																// deUint32					srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,																// deUint32					destQueueFamilyIndex;
				*colorImage,																			// VkImage					image;
				range																					// VkImageSubresourceRange	subresourceRange;
			}
		};

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
							  0u, DE_NULL, 0u, DE_NULL, 2u, imageLayoutBarriers);

		vk.cmdClearColorImage(*cmdBuffer, *msColorImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor[0], 1u, &range);
		vk.cmdClearColorImage(*cmdBuffer, *colorImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor[1], 1u, &range);

		const VkImageMemoryBarrier	imageClearBarriers[]	=
		{
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,													// VkStructureType			sType;
				DE_NULL,																				// const void*				pNext;
				VK_ACCESS_TRANSFER_WRITE_BIT,															// VkAccessFlags			srcAccessMask;
				VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,													// VkAccessFlags			dstAccessMask;
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,													// VkImageLayout			oldLayout;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,												// VkImageLayout			newLayout;
				VK_QUEUE_FAMILY_IGNORED,																// deUint32					srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,																// deUint32					destQueueFamilyIndex;
				*msColorImage,																			// VkImage					image;
				range																					// VkImageSubresourceRange	subresourceRange;
			},
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,													// VkStructureType			sType;
				DE_NULL,																				// const void*				pNext;
				VK_ACCESS_TRANSFER_WRITE_BIT,															// VkAccessFlags			srcAccessMask;
				VK_ACCESS_SHADER_READ_BIT,																// VkAccessFlags			dstAccessMask;
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,													// VkImageLayout			oldLayout;
				VK_IMAGE_LAYOUT_GENERAL,																// VkImageLayout			newLayout;
				VK_QUEUE_FAMILY_IGNORED,																// deUint32					srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,																// deUint32					destQueueFamilyIndex;
				*colorImage,																			// VkImage					image;
				range																					// VkImageSubresourceRange	subresourceRange;
			}
		};

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &imageClearBarriers[0]);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &imageClearBarriers[1]);
	}

	// Render pass: this should render only to the area defined by renderSize (smaller than the size of the attachment)
	{
		const VkDeviceSize			vertexBufferOffset	= 0ull;

		renderPass.begin(vk, *cmdBuffer, makeRect2D(0, 0, caseDef.renderSize.x(), caseDef.renderSize.y()));
		{
			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
			pipeline.bind(*cmdBuffer);
			vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);
			vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);
		}
		renderPass.end(vk, *cmdBuffer);
	}

	// copy colorImage to host visible colorBuffer
	const VkImageMemoryBarrier	imageBarrier		=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,														// VkStructureType			sType;
		DE_NULL,																					// const void*				pNext;
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,														// VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,																// VkAccessFlags			dstAccessMask;
		VK_IMAGE_LAYOUT_GENERAL,																	// VkImageLayout			oldLayout;
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,														// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,																	// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,																	// deUint32					destQueueFamilyIndex;
		*colorImage,																				// VkImage					image;
		range																						// VkImageSubresourceRange	subresourceRange;
	};

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
						  0u, DE_NULL, 0u, DE_NULL, 1u, &imageBarrier);

	const VkBufferImageCopy		regionBufferImageCopy				=
	{
		0ull,																// VkDeviceSize                bufferOffset;
		0u,																	// uint32_t                    bufferRowLength;
		0u,																	// uint32_t                    bufferImageHeight;
		makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u),	// VkImageSubresourceLayers    imageSubresource;
		makeOffset3D(0, 0, 0),												// VkOffset3D                  imageOffset;
		makeExtent3D(attachmentSize),										// VkExtent3D                  imageExtent;
	};

	vk.cmdCopyImageToBuffer(*cmdBuffer, *colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorBuffer, 1u, &regionBufferImageCopy);

	const VkBufferMemoryBarrier	bufferBarrier		=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType    sType;
		DE_NULL,									// const void*        pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags      srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags      dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// uint32_t           srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// uint32_t           dstQueueFamilyIndex;
		*colorBuffer,								// VkBuffer           buffer;
		0ull,										// VkDeviceSize       offset;
		VK_WHOLE_SIZE,								// VkDeviceSize       size;
	};

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u,
						  0u, DE_NULL, 1u, &bufferBarrier, 0u, DE_NULL);

	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// Verify results
	const tcu::TextureFormat			format				= mapVkFormat(COLOR_FORMAT);
	tcu::TextureLevel					textureLevel		(format, attachmentSize.x(), attachmentSize.y(), attachmentSize.z());
	const tcu::PixelBufferAccess		expectedImage		(textureLevel);

	const tcu::Vec4						expectedColor		= tcu::Vec4(1.0f, 0.5f, 0.25f, 1.0f);

	invalidateAlloc(vk, device, *colorBufferAlloc);

	for (int z = 0; z < expectedImage.getDepth(); ++z)
	{
		for (int y = 0; y < expectedImage.getHeight(); ++y)
		{
			for (int x = 0; x < expectedImage.getWidth(); ++x)
			{
				if (x < caseDef.renderSize.x() && y < caseDef.renderSize.y() && z < caseDef.renderSize.z())
					expectedImage.setPixel(expectedColor, x, y, z);
				else
					expectedImage.setPixel(tcu::Vec4(clearColor[0].float32), x, y, z);
			}
		}
	}
	const tcu::ConstPixelBufferAccess	resultImage		(format, attachmentSize.x(), attachmentSize.y(), attachmentSize.z(), colorBufferAlloc->getHostPtr());

	if (!tcu::intThresholdCompare(context.getTestContext().getLog(), "Image Comparison", "", expectedImage, resultImage, tcu::UVec4(1), tcu::COMPARE_LOG_RESULT))
		return tcu::TestStatus::fail("Fail");

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus testUnusedAtt (Context& context, PipelineConstructionType pipelineConstructionType)
{
	const DeviceInterface&			vk						= context.getDeviceInterface();
	const VkDevice					device					= context.getDevice();
	const Move<VkCommandPool>		cmdPool					(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, context.getUniversalQueueFamilyIndex()));
	const Move<VkCommandBuffer>		cmdBuffer				(makeCommandBuffer(vk, device, *cmdPool));

	const VkAttachmentReference		attRef					=
	{
		VK_ATTACHMENT_UNUSED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	const VkSubpassDescription		subpass					=
	{
		0,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		0,
		DE_NULL,
		1,
		&attRef,
		DE_NULL,
		DE_NULL,
		0,
		DE_NULL
	};

	const VkRenderPassCreateInfo	renderPassCreateInfo	=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		DE_NULL,
		0,
		0,
		DE_NULL,
		1,
		&subpass,
		0,
		DE_NULL
	};

	RenderPassWrapper				renderPass				(pipelineConstructionType, vk, device, &renderPassCreateInfo);

	const VkFramebufferCreateInfo	framebufferCreateInfo	=
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		DE_NULL,
		0,
		*renderPass,
		0,
		DE_NULL,
		32,
		32,
		1
	};

	renderPass.createFramebuffer(vk, device, &framebufferCreateInfo, VK_NULL_HANDLE);

	beginCommandBuffer(vk, *cmdBuffer);
	renderPass.begin(vk, *cmdBuffer, makeRect2D(0, 0, 32u, 32u));
	renderPass.end(vk, *cmdBuffer);
	endCommandBuffer(vk, *cmdBuffer);

	return tcu::TestStatus::pass("Pass");
}

void initDifferentAttachmentSizesPrograms (SourceCollections& programCollection, const CaseDef caseDef)
{
	DE_UNREF(caseDef);

	// Vertex shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in vec4 in_position;\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "	vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "	gl_Position	= in_position;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) out vec4 o_color0;\n"
			<< "layout(location = 1) out vec4 o_color1;\n"
			<< "layout(location = 2) out vec4 o_color2;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    o_color0 = vec4(1.0,  0.5, 0.25, 1.0);\n"
			<< "    o_color1 = vec4(0.5,  1.0, 0.25, 1.0);\n"
			<< "    o_color2 = vec4(0.25, 0.5, 1.0,  1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

void initMultiAttachmentsNotExportPrograms (SourceCollections& programCollection, const CaseDef caseDef)
{
	DE_UNREF(caseDef);

	// Vertex shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in vec4 in_position;\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "	vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "	gl_Position	= in_position;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) out vec4 o_color0;\n"
			<< "layout(location = 1) out vec4 o_color1;\n"
			<< "layout(location = 2) out vec4 o_color2;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    o_color0 = vec4(1.0,  0.5, 0.25, 1.0);\n"
			<< "    o_color2 = vec4(0.25, 0.5, 1.0,  1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

std::string getShortImageViewTypeName (const VkImageViewType imageViewType)
{
	std::string	s	(getImageViewTypeName(imageViewType));
	return de::toLower(s.substr(19));
}

std::string getSizeString (const CaseDef& caseDef)
{
	std::ostringstream	str;

										str << caseDef.renderSize.x();
	if (caseDef.renderSize.y() > 1)		str << "x" << caseDef.renderSize.y();
	if (caseDef.renderSize.z() > 1)		str << "x" << caseDef.renderSize.z();

										str << "_" << caseDef.attachmentSize.x();

	if (caseDef.attachmentSize.y() > 1)	str << "x" << caseDef.attachmentSize.y();
	if (caseDef.attachmentSize.z() > 1)	str << "x" << caseDef.attachmentSize.z();
	if (caseDef.numLayers > 1)			str << "_" << caseDef.numLayers;

	return str.str();
}

std::string getTestCaseString (const CaseDef& caseDef)
{
	std::ostringstream str;

	str << getShortImageViewTypeName (caseDef.imageType).c_str();
	str << "_";
	str << getSizeString(caseDef);

	if (caseDef.multisample)
		str << "_ms";

	return str.str();
}

void checkSupport (Context& context, const CaseDef caseDef)
{
	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), caseDef.pipelineConstructionType);
}

void checkSupportNoAtt (Context& context, const NoAttCaseDef caseDef)
{
	const VkPhysicalDeviceFeatures features = context.getDeviceFeatures();

	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS);

	if (!features.geometryShader && !features.tessellationShader) // Shader uses gl_PrimitiveID
		throw tcu::NotSupportedError("geometryShader or tessellationShader feature not supported");

	if (caseDef.multisample)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING); // MS shader uses gl_SampleID

	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), caseDef.pipelineConstructionType);
}

void addAttachmentTestCasesWithFunctions (tcu::TestCaseGroup* group, PipelineConstructionType pipelineConstructionType)
{
	// Add test cases for attachment strictly sizes larger than the framebuffer
	const CaseDef	caseDef[]	=
	{
		// Single-sample test cases
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_1D,			IVec3(32, 1, 1),	IVec3(64, 1, 1),	1,		false,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_1D,			IVec3(32, 1, 1),	IVec3(48, 1, 1),	1,		false,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_1D,			IVec3(32, 1, 1),	IVec3(39, 1, 1),	1,		false,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_1D,			IVec3(19, 1, 1),	IVec3(32, 1, 1),	1,		false,	MULTI_ATTACHMENTS_NONE },

		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_1D_ARRAY,	IVec3(32, 1, 1),	IVec3(64, 1, 1),	4,		false,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_1D_ARRAY,	IVec3(32, 1, 1),	IVec3(48, 1, 1),	4,		false,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_1D_ARRAY,	IVec3(32, 1, 1),	IVec3(39, 1, 1),	4,		false,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_1D_ARRAY,	IVec3(19, 1, 1),	IVec3(32, 1, 1),	4,		false,	MULTI_ATTACHMENTS_NONE },

		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D,			IVec3(32, 32, 1),	IVec3(64, 64, 1),	1,		false,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D,			IVec3(32, 32, 1),	IVec3(48, 48, 1),	1,		false,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D,			IVec3(32, 32, 1),	IVec3(39, 41, 1),	1,		false,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D,			IVec3(19, 27, 1),	IVec3(32, 32, 1),	1,		false,	MULTI_ATTACHMENTS_NONE },

		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D_ARRAY,	IVec3(32, 32, 1),	IVec3(64, 64, 1),	4,		false,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D_ARRAY,	IVec3(32, 32, 1),	IVec3(48, 48, 1),	4,		false,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D_ARRAY,	IVec3(32, 32, 1),	IVec3(39, 41, 1),	4,		false,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D_ARRAY,	IVec3(19, 27, 1),	IVec3(32, 32, 1),	4,		false,	MULTI_ATTACHMENTS_NONE },

		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_CUBE,		IVec3(32, 32, 1),	IVec3(64, 64, 1),	6,		false,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_CUBE,		IVec3(32, 32, 1),	IVec3(48, 48, 1),	6,		false,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_CUBE,		IVec3(32, 32, 1),	IVec3(39, 41, 1),	6,		false,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_CUBE,		IVec3(19, 27, 1),	IVec3(32, 32, 1),	6,		false,	MULTI_ATTACHMENTS_NONE },

		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,	IVec3(32, 32, 1),	IVec3(64, 64, 1),	6*2,	false,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,	IVec3(32, 32, 1),	IVec3(48, 48, 1),	6*2,	false,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,	IVec3(32, 32, 1),	IVec3(39, 41, 1),	6*2,	false,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,	IVec3(19, 27, 1),	IVec3(32, 32, 1),	6*2,	false,	MULTI_ATTACHMENTS_NONE },

		// Multi-sample test cases
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D,			IVec3(32, 32, 1),	IVec3(64, 64, 1),	1,		true,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D,			IVec3(32, 32, 1),	IVec3(48, 48, 1),	1,		true,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D,			IVec3(32, 32, 1),	IVec3(39, 41, 1),	1,		true,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D,			IVec3(19, 27, 1),	IVec3(32, 32, 1),	1,		true,	MULTI_ATTACHMENTS_NONE },

		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D_ARRAY,	IVec3(32, 32, 1),	IVec3(64, 64, 1),	4,		true,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D_ARRAY,	IVec3(32, 32, 1),	IVec3(48, 48, 1),	4,		true,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D_ARRAY,	IVec3(32, 32, 1),	IVec3(39, 41, 1),	4,		true,	MULTI_ATTACHMENTS_NONE },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D_ARRAY,	IVec3(19, 27, 1),	IVec3(32, 32, 1),	4,		true,	MULTI_ATTACHMENTS_NONE },
	};

	for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(caseDef); ++sizeNdx)
		addFunctionCaseWithPrograms(group, getTestCaseString(caseDef[sizeNdx]).c_str(), "", checkSupport, initColorPrograms, test, caseDef[sizeNdx]);

	// Add tests for the case where there are no color attachments but the
	// fragment shader writes to an image via imageStore().
	NoAttCaseDef noAttCaseDef { pipelineConstructionType, false };
	addFunctionCaseWithPrograms(group, "no_attachments",    "", checkSupportNoAtt, initImagePrograms, testNoAtt, noAttCaseDef);
	noAttCaseDef.multisample = true;
	addFunctionCaseWithPrograms(group, "no_attachments_ms", "", checkSupportNoAtt, initImagePrograms, testNoAtt, noAttCaseDef);

	// Test render pass with attachment set as unused.
	if (!isConstructionTypeLibrary(pipelineConstructionType))
		addFunctionCase(group, "unused_attachment", "", testUnusedAtt, pipelineConstructionType);

	// Tests with multiple attachments that have different sizes.
	const CaseDef	differentAttachmentSizesCaseDef[]	=
	{
		// Single-sample test cases
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_1D,	IVec3(32, 1, 1),	IVec3(64, 1, 1),	1,	false,	MULTI_ATTACHMENTS_DIFFERENT_SIZES },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_1D,	IVec3(32, 1, 1),	IVec3(48, 1, 1),	1,	false,	MULTI_ATTACHMENTS_DIFFERENT_SIZES },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_1D,	IVec3(32, 1, 1),	IVec3(39, 1, 1),	1,	false,	MULTI_ATTACHMENTS_DIFFERENT_SIZES },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_1D,	IVec3(19, 1, 1),	IVec3(32, 1, 1),	1,	false,	MULTI_ATTACHMENTS_DIFFERENT_SIZES },

		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D,	IVec3(32, 32, 1),	IVec3(64, 64, 1),	1,	false,	MULTI_ATTACHMENTS_DIFFERENT_SIZES },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D,	IVec3(32, 32, 1),	IVec3(48, 48, 1),	1,	false,	MULTI_ATTACHMENTS_DIFFERENT_SIZES },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D,	IVec3(32, 32, 1),	IVec3(39, 41, 1),	1,	false,	MULTI_ATTACHMENTS_DIFFERENT_SIZES },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D,	IVec3(19, 27, 1),	IVec3(32, 32, 1),	1,	false,	MULTI_ATTACHMENTS_DIFFERENT_SIZES },

		// Multi-sample test cases
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D,	IVec3(32, 32, 1),	IVec3(64, 64, 1),	1,	true,	MULTI_ATTACHMENTS_DIFFERENT_SIZES },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D,	IVec3(32, 32, 1),	IVec3(48, 48, 1),	1,	true,	MULTI_ATTACHMENTS_DIFFERENT_SIZES },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D,	IVec3(32, 32, 1),	IVec3(39, 41, 1),	1,	true,	MULTI_ATTACHMENTS_DIFFERENT_SIZES },
		{ pipelineConstructionType,		VK_IMAGE_VIEW_TYPE_2D,	IVec3(19, 27, 1),	IVec3(32, 32, 1),	1,	true,	MULTI_ATTACHMENTS_DIFFERENT_SIZES }
	};

	for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(differentAttachmentSizesCaseDef); ++sizeNdx)
		addFunctionCaseWithPrograms(group, (std::string("diff_attachments_") + getTestCaseString(differentAttachmentSizesCaseDef[sizeNdx])).c_str(), "", checkSupport, initDifferentAttachmentSizesPrograms, testMultiAttachments, differentAttachmentSizesCaseDef[sizeNdx]);

	// Tests with same attachment for input and resolving.
	const CaseDef resolveInputSameAttachmentCaseDef = { pipelineConstructionType,	VK_IMAGE_VIEW_TYPE_2D,	IVec3(64, 64, 1),	IVec3(64, 64, 1),	1,	true,	MULTI_ATTACHMENTS_NONE };
	// Input attachments are not supported with dynamic rendering
	if (!vk::isConstructionTypeShaderObject(pipelineConstructionType))
	{
		addFunctionCaseWithPrograms(group, "resolve_input_same_attachment", "", checkSupport, initInputResolveSameAttachmentPrograms, testInputResolveSameAttachment, resolveInputSameAttachmentCaseDef);
	}

	// Tests with multiple attachments, which some of them are not used in FS.
	const CaseDef AttachmentCaseDef[] = {
		// Single-sample test case
		{ pipelineConstructionType,	VK_IMAGE_VIEW_TYPE_2D,	IVec3(64, 64, 1),	IVec3(64, 64, 1),	1,	false,	MULTI_ATTACHMENTS_NOT_EXPORTED },
		// Multi-sample test case
		{ pipelineConstructionType,	VK_IMAGE_VIEW_TYPE_2D,	IVec3(64, 64, 1),	IVec3(64, 64, 1),	1,	true,	MULTI_ATTACHMENTS_NOT_EXPORTED }
	};

	for (int Ndx = 0; Ndx < DE_LENGTH_OF_ARRAY(AttachmentCaseDef); ++Ndx)
		addFunctionCaseWithPrograms(group, (std::string("multi_attachments_not_exported_") + getTestCaseString(AttachmentCaseDef[Ndx])).c_str(), "", checkSupport, initMultiAttachmentsNotExportPrograms, testMultiAttachments, AttachmentCaseDef[Ndx]);
}

} // anonymous ns

tcu::TestCaseGroup* createFramebufferAttachmentTests (tcu::TestContext& testCtx, PipelineConstructionType pipelineConstructionType)
{
	return createTestGroup(testCtx, "framebuffer_attachment", "Framebuffer attachment tests", addAttachmentTestCasesWithFunctions, pipelineConstructionType);
}

} // pipeline
} // vkt
