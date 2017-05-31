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
 * \file vktPipelineRenderToImageTests.cpp
 * \brief Render to image tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineRenderToImageTests.hpp"
#include "vktPipelineMakeUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkPrograms.hpp"
#include "vkImageUtil.hpp"

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

typedef SharedPtr<Unique<VkImageView> >	SharedPtrVkImageView;
typedef SharedPtr<Unique<VkPipeline> >	SharedPtrVkPipeline;

enum Constants
{
	REFERENCE_COLOR_VALUE = 125
};

struct CaseDef
{
	VkImageViewType	imageType;
	IVec3			renderSize;
	int				numLayers;
	VkFormat		colorFormat;
};

template<typename T>
inline SharedPtr<Unique<T> > makeSharedPtr (Move<T> move)
{
	return SharedPtr<Unique<T> >(new Unique<T>(move));
}

template<typename T>
inline VkDeviceSize sizeInBytes (const vector<T>& vec)
{
	return vec.size() * sizeof(vec[0]);
}

Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&		vk,
									   const VkDevice				device,
									   const VkPipelineLayout		pipelineLayout,
									   const VkRenderPass			renderPass,
									   const VkShaderModule			vertexModule,
									   const VkShaderModule			fragmentModule,
									   const IVec3					renderSize,
									   const VkPrimitiveTopology	topology,
									   const deUint32				subpass)
{
	const VkVertexInputBindingDescription vertexInputBindingDescription =
	{
		0u,								// uint32_t				binding;
		sizeof(Vertex4RGBA),			// uint32_t				stride;
		VK_VERTEX_INPUT_RATE_VERTEX,	// VkVertexInputRate	inputRate;
	};

	const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[] =
	{
		{
			0u,								// uint32_t			location;
			0u,								// uint32_t			binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat			format;
			0u,								// uint32_t			offset;
		},
		{
			1u,								// uint32_t			location;
			0u,								// uint32_t			binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,	// VkFormat			format;
			sizeof(Vec4),					// uint32_t			offset;
		}
	};

	const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		(VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags	flags;
		1u,															// uint32_t									vertexBindingDescriptionCount;
		&vertexInputBindingDescription,								// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
		DE_LENGTH_OF_ARRAY(vertexInputAttributeDescriptions),		// uint32_t									vertexAttributeDescriptionCount;
		vertexInputAttributeDescriptions,							// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	const VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		(VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags	flags;
		topology,														// VkPrimitiveTopology						topology;
		VK_FALSE,														// VkBool32									primitiveRestartEnable;
	};

	const VkViewport viewport = makeViewport(
		0.0f, 0.0f,
		static_cast<float>(renderSize.x()), static_cast<float>(renderSize.y()),
		0.0f, 1.0f);

	const VkRect2D scissor =
	{
		makeOffset2D(0, 0),
		makeExtent2D(renderSize.x(), renderSize.y()),
	};

	const VkPipelineViewportStateCreateInfo pipelineViewportStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		(VkPipelineViewportStateCreateFlags)0,					// VkPipelineViewportStateCreateFlags	flags;
		1u,														// uint32_t								viewportCount;
		&viewport,												// const VkViewport*					pViewports;
		1u,														// uint32_t								scissorCount;
		&scissor,												// const VkRect2D*						pScissors;
	};

	const VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		(VkPipelineRasterizationStateCreateFlags)0,					// VkPipelineRasterizationStateCreateFlags	flags;
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

	const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		(VkPipelineMultisampleStateCreateFlags)0,					// VkPipelineMultisampleStateCreateFlags	flags;
		VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits					rasterizationSamples;
		VK_FALSE,													// VkBool32									sampleShadingEnable;
		0.0f,														// float									minSampleShading;
		DE_NULL,													// const VkSampleMask*						pSampleMask;
		VK_FALSE,													// VkBool32									alphaToCoverageEnable;
		VK_FALSE													// VkBool32									alphaToOneEnable;
	};

	const VkStencilOpState stencilOpState = makeStencilOpState(
		VK_STENCIL_OP_KEEP,		// stencil fail
		VK_STENCIL_OP_KEEP,		// depth & stencil pass
		VK_STENCIL_OP_KEEP,		// depth only fail
		VK_COMPARE_OP_ALWAYS,	// compare op
		0u,						// compare mask
		0u,						// write mask
		0u);					// reference

	VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		(VkPipelineDepthStencilStateCreateFlags)0,					// VkPipelineDepthStencilStateCreateFlags	flags;
		VK_FALSE,													// VkBool32									depthTestEnable;
		VK_FALSE,													// VkBool32									depthWriteEnable;
		VK_COMPARE_OP_LESS,											// VkCompareOp								depthCompareOp;
		VK_FALSE,													// VkBool32									depthBoundsTestEnable;
		VK_FALSE,													// VkBool32									stencilTestEnable;
		stencilOpState,												// VkStencilOpState							front;
		stencilOpState,												// VkStencilOpState							back;
		0.0f,														// float									minDepthBounds;
		1.0f,														// float									maxDepthBounds;
	};

	const VkColorComponentFlags colorComponentsAll = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	// Number of blend attachments must equal the number of color attachments during any subpass.
	const VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState =
	{
		VK_FALSE,				// VkBool32					blendEnable;
		VK_BLEND_FACTOR_ONE,	// VkBlendFactor			srcColorBlendFactor;
		VK_BLEND_FACTOR_ZERO,	// VkBlendFactor			dstColorBlendFactor;
		VK_BLEND_OP_ADD,		// VkBlendOp				colorBlendOp;
		VK_BLEND_FACTOR_ONE,	// VkBlendFactor			srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ZERO,	// VkBlendFactor			dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,		// VkBlendOp				alphaBlendOp;
		colorComponentsAll,		// VkColorComponentFlags	colorWriteMask;
	};

	const VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
		DE_NULL,													// const void*									pNext;
		(VkPipelineColorBlendStateCreateFlags)0,					// VkPipelineColorBlendStateCreateFlags			flags;
		VK_FALSE,													// VkBool32										logicOpEnable;
		VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
		1u,															// deUint32										attachmentCount;
		&pipelineColorBlendAttachmentState,							// const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },									// float										blendConstants[4];
	};

	const VkPipelineShaderStageCreateInfo pShaderStages[] =
	{
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
			DE_NULL,												// const void*							pNext;
			(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags		flags;
			VK_SHADER_STAGE_VERTEX_BIT,								// VkShaderStageFlagBits				stage;
			vertexModule,											// VkShaderModule						module;
			"main",													// const char*							pName;
			DE_NULL,												// const VkSpecializationInfo*			pSpecializationInfo;
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
			DE_NULL,												// const void*							pNext;
			(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags		flags;
			VK_SHADER_STAGE_FRAGMENT_BIT,							// VkShaderStageFlagBits				stage;
			fragmentModule,											// VkShaderModule						module;
			"main",													// const char*							pName;
			DE_NULL,												// const VkSpecializationInfo*			pSpecializationInfo;
		}
	};

	const VkGraphicsPipelineCreateInfo graphicsPipelineInfo =
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
		DE_NULL,											// const void*										pNext;
		(VkPipelineCreateFlags)0,							// VkPipelineCreateFlags							flags;
		DE_LENGTH_OF_ARRAY(pShaderStages),					// deUint32											stageCount;
		pShaderStages,										// const VkPipelineShaderStageCreateInfo*			pStages;
		&vertexInputStateInfo,								// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
		&pipelineInputAssemblyStateInfo,					// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
		DE_NULL,											// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
		&pipelineViewportStateInfo,							// const VkPipelineViewportStateCreateInfo*			pViewportState;
		&pipelineRasterizationStateInfo,					// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
		&pipelineMultisampleStateInfo,						// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
		&pipelineDepthStencilStateInfo,						// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
		&pipelineColorBlendStateInfo,						// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
		DE_NULL,											// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
		pipelineLayout,										// VkPipelineLayout									layout;
		renderPass,											// VkRenderPass										renderPass;
		subpass,											// deUint32											subpass;
		DE_NULL,											// VkPipeline										basePipelineHandle;
		0,													// deInt32											basePipelineIndex;
	};

	return createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineInfo);
}

//! Make a render pass with one subpass per color attachment and one attachment per image layer.
Move<VkRenderPass> makeRenderPass (const DeviceInterface&		vk,
								   const VkDevice				device,
								   const VkFormat				colorFormat,
								   const deUint32				numLayers,
								   const VkImageLayout			initialColorImageLayout			= VK_IMAGE_LAYOUT_UNDEFINED)
{
	const VkAttachmentDescription colorAttachmentDescription =
	{
		(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags		flags;
		colorFormat,								// VkFormat							format;
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits			samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp				stencilStoreOp;
		initialColorImageLayout,					// VkImageLayout					initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout					finalLayout;
	};
	const vector<VkAttachmentDescription> attachmentDescriptions(numLayers, colorAttachmentDescription);

	// Create a subpass for each attachment (each attachement is a layer of an arrayed image).
	vector<VkAttachmentReference>	colorAttachmentReferences(numLayers);
	vector<VkSubpassDescription>	subpasses;

	for (deUint32 i = 0; i < numLayers; ++i)
	{
		const VkAttachmentReference attachmentRef =
		{
			i,												// deUint32			attachment;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL		// VkImageLayout	layout;
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
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,				// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		(VkRenderPassCreateFlags)0,								// VkRenderPassCreateFlags			flags;
		static_cast<deUint32>(attachmentDescriptions.size()),	// deUint32							attachmentCount;
		&attachmentDescriptions[0],								// const VkAttachmentDescription*	pAttachments;
		static_cast<deUint32>(subpasses.size()),				// deUint32							subpassCount;
		&subpasses[0],											// const VkSubpassDescription*		pSubpasses;
		0u,														// deUint32							dependencyCount;
		DE_NULL													// const VkSubpassDependency*		pDependencies;
	};

	return createRenderPass(vk, device, &renderPassInfo);
}

Move<VkImage> makeImage (const DeviceInterface&		vk,
						 const VkDevice				device,
						 VkImageCreateFlags			flags,
						 VkImageType				imageType,
						 const VkFormat				format,
						 const IVec3&				size,
						 const deUint32				numLayers,
						 const VkImageUsageFlags	usage)
{
	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		flags,									// VkImageCreateFlags		flags;
		imageType,								// VkImageType				imageType;
		format,									// VkFormat					format;
		makeExtent3D(size),						// VkExtent3D				extent;
		1u,										// deUint32					mipLevels;
		numLayers,								// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		usage,									// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		0u,										// deUint32					queueFamilyIndexCount;
		DE_NULL,								// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			initialLayout;
	};
	return createImage(vk, device, &imageParams);
}

inline Move<VkBuffer> makeBuffer (const DeviceInterface& vk, const VkDevice device, const VkDeviceSize bufferSize, const VkBufferUsageFlags usage)
{
	const VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(bufferSize, usage);
	return createBuffer(vk, device, &bufferCreateInfo);
}

inline VkImageSubresourceRange makeColorSubresourceRange (const int baseArrayLayer, const int layerCount)
{
	return makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, static_cast<deUint32>(baseArrayLayer), static_cast<deUint32>(layerCount));
}

//! Get a reference clear value based on color format.
VkClearValue getClearValue (const VkFormat format)
{
	if (isUintFormat(format) || isIntFormat(format))
		return makeClearValueColorU32(REFERENCE_COLOR_VALUE, REFERENCE_COLOR_VALUE, REFERENCE_COLOR_VALUE, REFERENCE_COLOR_VALUE);
	else
		return makeClearValueColorF32(1.0f, 1.0f, 1.0f, 1.0f);
}

std::string getColorFormatStr (const int numComponents, const bool isUint, const bool isSint)
{
	std::ostringstream str;
	if (numComponents == 1)
		str << (isUint ? "uint" : isSint ? "int" : "float");
	else
		str << (isUint ? "u" : isSint ? "i" : "") << "vec" << numComponents;

	return str.str();
}

//! A half-viewport quad. Use with TRIANGLE_STRIP topology.
vector<Vertex4RGBA> genFullQuadVertices (const int subpassCount, const vector<Vec4>& color)
{
	vector<Vertex4RGBA>	vectorData;
	for (int subpassNdx = 0; subpassNdx < subpassCount; ++subpassNdx)
	{
		Vertex4RGBA data =
		{
			Vec4(0.0f, -1.0f, 0.0f, 1.0f),
			color[subpassNdx % color.size()],
		};
		vectorData.push_back(data);
		data.position	= Vec4(0.0f,  1.0f, 0.0f, 1.0f);
		vectorData.push_back(data);
		data.position	= Vec4(1.0f, -1.0f, 0.0f, 1.0f);
		vectorData.push_back(data);
		data.position	= Vec4(1.0f,  1.0f, 0.0f, 1.0f);
		vectorData.push_back(data);
	}
	return vectorData;
}

VkImageType getImageType (const VkImageViewType viewType)
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

void initPrograms (SourceCollections& programCollection, const CaseDef caseDef)
{
	const int	numComponents	= getNumUsedChannels(mapVkFormat(caseDef.colorFormat).order);
	const bool	isUint			= isUintFormat(caseDef.colorFormat);
	const bool	isSint			= isIntFormat(caseDef.colorFormat);

	// Vertex shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_position;\n"
			<< "layout(location = 1) in  vec4 in_color;\n"
			<< "layout(location = 0) out vec4 out_color;\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "	vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "	gl_Position	= in_position;\n"
			<< "	out_color	= in_color;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream colorValue;
		colorValue << REFERENCE_COLOR_VALUE;
		const std::string colorFormat	= getColorFormatStr(numComponents, isUint, isSint);
		const std::string colorInteger	= (isUint || isSint ? " * "+colorFormat+"("+colorValue.str()+")" :"");

		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_color;\n"
			<< "layout(location = 0) out " << colorFormat << " o_color;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    o_color = " << colorFormat << "("
			<< (numComponents == 1 ? "in_color.r"   :
				numComponents == 2 ? "in_color.rg"  :
				numComponents == 3 ? "in_color.rgb" : "in_color")
			<< colorInteger
			<< ");\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

tcu::PixelBufferAccess getExpectedData (tcu::TextureLevel& textureLevel, const CaseDef& caseDef, const Vec4* color, const int sizeColor)
{
	const bool						isInt			= isUintFormat(caseDef.colorFormat) || isIntFormat(caseDef.colorFormat);
	const tcu::PixelBufferAccess	expectedImage	(textureLevel);

	if (isInt)
		tcu::clear(expectedImage, tcu::IVec4(REFERENCE_COLOR_VALUE));
	else
		tcu::clear(expectedImage, tcu::Vec4(1.0));

	for (int z = 0; z < expectedImage.getDepth(); ++z)
	{
		const Vec4& setColor	= color[z % sizeColor];
		const IVec4 setColorInt	= (static_cast<float>(REFERENCE_COLOR_VALUE) * setColor).cast<deInt32>();

		for (int y = 0;							y < caseDef.renderSize.y(); ++y)
		for (int x = caseDef.renderSize.x()/2;	x < caseDef.renderSize.x(); ++x)
		{
			if (isInt)
				expectedImage.setPixel(setColorInt, x, y, z);
			else
				expectedImage.setPixel(setColor, x, y, z);
		}
	}
	return expectedImage;
}

tcu::TestStatus test (Context& context, const CaseDef caseDef)
{
	if (VK_IMAGE_VIEW_TYPE_3D == caseDef.imageType &&
		(!de::contains(context.getDeviceExtensions().begin(), context.getDeviceExtensions().end(), "VK_KHR_maintenance1")))
		TCU_THROW(NotSupportedError, "Extension VK_KHR_maintenance1 not supported");

	const DeviceInterface&			vk					= context.getDeviceInterface();
	const VkDevice					device				= context.getDevice();
	const VkQueue					queue				= context.getUniversalQueue();
	const deUint32					queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	Allocator&						allocator			= context.getDefaultAllocator();
	Move<VkImage>					colorImage;
	MovePtr<Allocation>				colorImageAlloc;
	const Vec4						color[]				=
	{
		Vec4(0.9f, 0.0f, 0.0f, 1.0f),
		Vec4(0.6f, 1.0f, 0.0f, 1.0f),
		Vec4(0.3f, 0.0f, 1.0f, 1.0f),
		Vec4(0.1f, 0.0f, 1.0f, 1.0f)
	};

	const int						numLayers			= (VK_IMAGE_VIEW_TYPE_3D == caseDef.imageType ? caseDef.renderSize.z() : caseDef.numLayers);
	const VkDeviceSize				colorBufferSize		= caseDef.renderSize.x() * caseDef.renderSize.y() * caseDef.renderSize.z() * caseDef.numLayers * tcu::getPixelSize(mapVkFormat(caseDef.colorFormat));
	const Unique<VkBuffer>			colorBuffer			(makeBuffer(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>		colorBufferAlloc	(bindBuffer(vk, device, allocator, *colorBuffer, MemoryRequirement::HostVisible));

	const Unique<VkShaderModule>	vertexModule		(createShaderModule			(vk, device, context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>	fragmentModule		(createShaderModule			(vk, device, context.getBinaryCollection().get("frag"), 0u));
	const Unique<VkRenderPass>		renderPass			(makeRenderPass				(vk, device, caseDef.colorFormat, static_cast<deUint32>(numLayers),
																					 (caseDef.imageType == VK_IMAGE_VIEW_TYPE_3D) ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
																																  : VK_IMAGE_LAYOUT_UNDEFINED));
	const Unique<VkPipelineLayout>	pipelineLayout		(makePipelineLayout			(vk, device));
	vector<SharedPtrVkPipeline>		pipeline;
	const Unique<VkCommandPool>		cmdPool				(makeCommandPool  (vk, device, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer			(makeCommandBuffer(vk, device, *cmdPool));

	vector<SharedPtrVkImageView>	colorAttachments;
	vector<VkImageView>				attachmentHandles;
	Move<VkBuffer>					vertexBuffer;
	MovePtr<Allocation>				vertexBufferAlloc;
	Move<VkFramebuffer>				framebuffer;

	//create colorImage
	{
		const VkImageViewCreateFlags	flags			= (VK_IMAGE_VIEW_TYPE_3D == caseDef.imageType ? (VkImageViewCreateFlags)VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT_KHR : (VkImageViewCreateFlags)0);
		const VkImageUsageFlags			colorImageUsage	= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		colorImage = makeImage(vk, device, flags, getImageType(caseDef.imageType), caseDef.colorFormat, caseDef.renderSize, caseDef.numLayers, colorImageUsage);
		colorImageAlloc = bindImage(vk, device, allocator, *colorImage, MemoryRequirement::Any);
	}

	//create vertexBuffer
	{
		const vector<Vertex4RGBA>	vertices			= genFullQuadVertices(numLayers, vector<Vec4>(color, color + DE_LENGTH_OF_ARRAY(color)));
		const VkDeviceSize			vertexBufferSize	= sizeInBytes(vertices);

		vertexBuffer		= makeBuffer(vk, device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		vertexBufferAlloc	= bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible);
		deMemcpy(vertexBufferAlloc->getHostPtr(), &vertices[0], static_cast<std::size_t>(vertexBufferSize));
		flushMappedMemoryRange(vk, device, vertexBufferAlloc->getMemory(), vertexBufferAlloc->getOffset(), vertexBufferSize);
	}

	//create attachmentHandles and pipelines
	for (int layerNdx = 0; layerNdx < numLayers; ++layerNdx)
	{
		const VkImageViewType	imageType = (VK_IMAGE_VIEW_TYPE_3D == caseDef.imageType ? VK_IMAGE_VIEW_TYPE_2D_ARRAY :
											(VK_IMAGE_VIEW_TYPE_CUBE == caseDef.imageType || VK_IMAGE_VIEW_TYPE_CUBE_ARRAY == caseDef.imageType ? VK_IMAGE_VIEW_TYPE_2D :
											caseDef.imageType));

		colorAttachments.push_back(makeSharedPtr(makeImageView(vk, device, *colorImage, imageType, caseDef.colorFormat, makeColorSubresourceRange(layerNdx, 1))));
		attachmentHandles.push_back(**colorAttachments.back());

		pipeline.push_back(makeSharedPtr(makeGraphicsPipeline(vk, device, *pipelineLayout, *renderPass, *vertexModule, *fragmentModule,
															  caseDef.renderSize, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, static_cast<deUint32>(layerNdx))));
	}

	framebuffer = makeFramebuffer(vk, device, *renderPass, numLayers, &attachmentHandles[0], static_cast<deUint32>(caseDef.renderSize.x()), static_cast<deUint32>(caseDef.renderSize.y()));

	beginCommandBuffer(vk, *cmdBuffer);

	// Prepare color image upfront for rendering to individual slices.  3D slices aren't separate subresources, so they shouldn't be transitioned
	// during each subpass like array layers.
	if (caseDef.imageType == VK_IMAGE_VIEW_TYPE_3D)
	{
		const VkImageMemoryBarrier	imageBarrier	=
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// VkStructureType            sType;
			DE_NULL,											// const void*                pNext;
			(VkAccessFlags)0,									// VkAccessFlags              srcAccessMask;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,				// VkAccessFlags              dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout              oldLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout              newLayout;
			VK_QUEUE_FAMILY_IGNORED,							// uint32_t                   srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,							// uint32_t                   dstQueueFamilyIndex;
			*colorImage,										// VkImage                    image;
			makeColorSubresourceRange(0, caseDef.numLayers)		// VkImageSubresourceRange    subresourceRange;
		};

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u,
								0u, DE_NULL, 0u, DE_NULL, 1u, &imageBarrier);
	}

	{
		const vector<VkClearValue>	clearValues			(numLayers, getClearValue(caseDef.colorFormat));
		const VkRect2D				renderArea			=
		{
			makeOffset2D(0, 0),
			makeExtent2D(caseDef.renderSize.x(), caseDef.renderSize.y()),
		};
		const VkRenderPassBeginInfo	renderPassBeginInfo	=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType         sType;
			DE_NULL,									// const void*             pNext;
			*renderPass,								// VkRenderPass            renderPass;
			*framebuffer,								// VkFramebuffer           framebuffer;
			renderArea,									// VkRect2D                renderArea;
			static_cast<deUint32>(clearValues.size()),	// uint32_t                clearValueCount;
			&clearValues[0],							// const VkClearValue*     pClearValues;
		};
		const VkDeviceSize			vertexBufferOffset	= 0ull;

		vk.cmdBeginRenderPass(*cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
	}

	//cmdDraw
	for (deUint32 layerNdx = 0; layerNdx < static_cast<deUint32>(numLayers); ++layerNdx)
	{
		if (layerNdx != 0)
			vk.cmdNextSubpass(*cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **pipeline[layerNdx]);
		vk.cmdDraw(*cmdBuffer, 4u, 1u, layerNdx*4u, 0u);
	}

	vk.cmdEndRenderPass(*cmdBuffer);

	// copy colorImage -> host visible colorBuffer
	{
		const VkImageMemoryBarrier	imageBarriers[]	=
		{
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
				DE_NULL,										// const void*				pNext;
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags			outputMask;
				VK_ACCESS_TRANSFER_READ_BIT,					// VkAccessFlags			inputMask;
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// VkImageLayout			oldLayout;
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,			// VkImageLayout			newLayout;
				VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,						// deUint32					destQueueFamilyIndex;
				*colorImage,									// VkImage					image;
				makeColorSubresourceRange(0, caseDef.numLayers)	// VkImageSubresourceRange	subresourceRange;
			}
		};

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
							  0u, DE_NULL, 0u, DE_NULL, DE_LENGTH_OF_ARRAY(imageBarriers), imageBarriers);

		const VkBufferImageCopy		region			=
		{
			0ull,																				// VkDeviceSize                bufferOffset;
			0u,																					// uint32_t                    bufferRowLength;
			0u,																					// uint32_t                    bufferImageHeight;
			makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, caseDef.numLayers),	// VkImageSubresourceLayers    imageSubresource;
			makeOffset3D(0, 0, 0),																// VkOffset3D                  imageOffset;
			makeExtent3D(caseDef.renderSize),													// VkExtent3D                  imageExtent;
		};

		vk.cmdCopyImageToBuffer(*cmdBuffer, *colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorBuffer, 1u, &region);

		const VkBufferMemoryBarrier	bufferBarriers[] =
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

	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// Verify results
	{
		invalidateMappedMemoryRange(vk, device, colorBufferAlloc->getMemory(), colorBufferAlloc->getOffset(), VK_WHOLE_SIZE);

		const tcu::TextureFormat			format			= mapVkFormat(caseDef.colorFormat);
		const int							depth			= deMax32(caseDef.renderSize.z(), caseDef.numLayers);
		tcu::TextureLevel					textureLevel	(format, caseDef.renderSize.x(), caseDef.renderSize.y(), depth);
		const tcu::PixelBufferAccess		expectedImage	= getExpectedData (textureLevel, caseDef, color, DE_LENGTH_OF_ARRAY(color));
		const tcu::ConstPixelBufferAccess	resultImage		(format, caseDef.renderSize.x(), caseDef.renderSize.y(), depth, colorBufferAlloc->getHostPtr());

		if (!tcu::intThresholdCompare(context.getTestContext().getLog(), "Image Comparison", "", expectedImage, resultImage, tcu::UVec4(2), tcu::COMPARE_LOG_RESULT))
			return tcu::TestStatus::fail("Fail");
	}
	return tcu::TestStatus::pass("Pass");
}

std::string getSizeString (const IVec3& size, const int numLayer)
{
	std::ostringstream str;
							str <<		  size.x();
	if (size.y() > 1)		str << "x" << size.y();
	if (size.z() > 1)		str << "x" << size.z();
	if (numLayer > 1)		str << "_" << numLayer;

	return str.str();
}

std::string getFormatString (const VkFormat format)
{
	std::string name(getFormatName(format));
	return de::toLower(name.substr(10));
}

std::string getShortImageViewTypeName (const VkImageViewType imageViewType)
{
	std::string s(getImageViewTypeName(imageViewType));
	return de::toLower(s.substr(19));
}

CaseDef caseDefWithFormat (CaseDef caseDef, const VkFormat format)
{
	caseDef.colorFormat = format;
	return caseDef;
}

void addTestCasesWithFunctions (tcu::TestCaseGroup* group)
{
	const CaseDef caseDef[] =
	{
		{ VK_IMAGE_VIEW_TYPE_1D,			IVec3(54,  1, 1),	1,		VK_FORMAT_UNDEFINED},
		{ VK_IMAGE_VIEW_TYPE_1D_ARRAY,		IVec3(54,  1, 1),	4,		VK_FORMAT_UNDEFINED},
		{ VK_IMAGE_VIEW_TYPE_2D,			IVec3(22, 64, 1),	1,		VK_FORMAT_UNDEFINED},
		{ VK_IMAGE_VIEW_TYPE_2D_ARRAY,		IVec3(22, 64, 1),	4,		VK_FORMAT_UNDEFINED},
		{ VK_IMAGE_VIEW_TYPE_3D,			IVec3(22, 64, 7),	1,		VK_FORMAT_UNDEFINED},
		{ VK_IMAGE_VIEW_TYPE_CUBE,			IVec3(35, 35, 1),	6,		VK_FORMAT_UNDEFINED},
		{ VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,	IVec3(35, 35, 1),	2*6,	VK_FORMAT_UNDEFINED},
	};

	const VkFormat format[]	=
	{
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R16G16_SINT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
	};

	for (int sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(caseDef); ++sizeNdx)
	{
		MovePtr<tcu::TestCaseGroup>	imageGroup(new tcu::TestCaseGroup(group->getTestContext(), getShortImageViewTypeName(caseDef[sizeNdx].imageType).c_str(), ""));
		{
			MovePtr<tcu::TestCaseGroup>	sizeGroup(new tcu::TestCaseGroup(group->getTestContext(), getSizeString(caseDef[sizeNdx].renderSize, caseDef[sizeNdx].numLayers).c_str(), ""));

			for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(format); ++formatNdx)
				addFunctionCaseWithPrograms(sizeGroup.get(), getFormatString(format[formatNdx]).c_str(), "", initPrograms, test, caseDefWithFormat(caseDef[sizeNdx], format[formatNdx]));

			imageGroup->addChild(sizeGroup.release());
		}
		group->addChild(imageGroup.release());
	}
}

} // anonymous ns

tcu::TestCaseGroup* createRenderToImageTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "render_to_image", "Render to image tests", addTestCasesWithFunctions);
}

} // pipeline
} // vkt
