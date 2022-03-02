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
 * \brief Tests Verifying Graphics Pipeline Libraries
 *//*--------------------------------------------------------------------*/

#include "vktPipelineLibraryTests.hpp"

#include "vkDefs.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "tcuCommandLine.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "tcuRGBA.hpp"

#include "../draw/vktDrawCreateInfoUtil.hpp"

#include <vector>
#include <set>

namespace vkt
{
namespace pipeline
{
namespace
{
using namespace vk;
using namespace vkt;
using namespace tcu;


static const deUint32								RENDER_SIZE_WIDTH							= 16u;
static const deUint32								RENDER_SIZE_HEIGHT							= 16u;
static const VkColorComponentFlags					COLOR_COMPONENTS_NO_RED						= VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
static const VkGraphicsPipelineLibraryFlagBitsEXT	GRAPHICS_PIPELINE_LIBRARY_FLAGS[]			=
{
	VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT,
	VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT,
	VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT,
	VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT,
};
static const VkGraphicsPipelineLibraryFlagsEXT		ALL_GRAPHICS_PIPELINE_LIBRARY_FLAGS			= static_cast<VkGraphicsPipelineLibraryFlagsEXT>(VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT)
																								| static_cast<VkGraphicsPipelineLibraryFlagsEXT>(VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT)
																								| static_cast<VkGraphicsPipelineLibraryFlagsEXT>(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT)
																								| static_cast<VkGraphicsPipelineLibraryFlagsEXT>(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT);

struct PipelineTreeNode
{
	deInt32		parentIndex;
	deUint32	shaderCount;
};

typedef std::vector<PipelineTreeNode>	PipelineTreeConfiguration;

struct TestParams
{
	PipelineTreeConfiguration	pipelineTreeConfiguration;
	bool						optimize;
	bool						delayedShaderCreate;
};

struct RuntimePipelineTreeNode
{
	deInt32								parentIndex;
	VkGraphicsPipelineLibraryFlagsEXT	graphicsPipelineLibraryFlags;
	VkGraphicsPipelineLibraryFlagsEXT	subtreeGraphicsPipelineLibraryFlags;
	Move<VkPipeline>					pipeline;
	std::vector<VkPipeline>				pipelineLibraries;
};

typedef std::vector<RuntimePipelineTreeNode>	RuntimePipelineTreeConfiguration;

inline UVec4 ivec2uvec (const IVec4& ivec)
{
	return UVec4
	{
		static_cast<deUint32>(ivec[0]),
		static_cast<deUint32>(ivec[1]),
		static_cast<deUint32>(ivec[2]),
		static_cast<deUint32>(ivec[3]),
	};
}

inline std::string getTestName (const PipelineTreeConfiguration& pipelineTreeConfiguration)
{
	std::string	result;
	int			level	= pipelineTreeConfiguration[0].parentIndex;

	for (const auto& node: pipelineTreeConfiguration)
	{
		if (level != node.parentIndex)
		{
			DE_ASSERT(level < node.parentIndex);

			result += '_';

			level = node.parentIndex;
		}

		result += de::toString(node.shaderCount);
	}

	return result;
}

inline bool isPartialFlagSubset (const VkFlags test, const VkFlags fullSet)
{
	if ((test & fullSet) == 0)
		return false;

	if ((test & fullSet) == fullSet)
		return false;

	return true;
}

inline VkPipelineCreateFlags calcPipelineCreateFlags (bool optimize, bool buildLibrary)
{
	VkPipelineCreateFlags	result = 0;

	if (buildLibrary)
		result |= static_cast<VkPipelineCreateFlags>(VK_PIPELINE_CREATE_LIBRARY_BIT_KHR);

	if (optimize)
	{
		if (buildLibrary)
			result |= static_cast<VkPipelineCreateFlags>(VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT);
		else
			result |= static_cast<VkPipelineCreateFlags>(VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT);
	}

	return result;
}

inline VkRenderPass getRenderPass (VkGraphicsPipelineLibraryFlagsEXT subset, VkRenderPass renderPass)
{
	static const VkGraphicsPipelineLibraryFlagsEXT	subsetRequiresRenderPass	= static_cast<VkGraphicsPipelineLibraryFlagsEXT>(VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT)
																				| static_cast<VkGraphicsPipelineLibraryFlagsEXT>(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT)
																				| static_cast<VkGraphicsPipelineLibraryFlagsEXT>(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT);
	if ((subsetRequiresRenderPass & subset) != 0)
		return renderPass;

	return DE_NULL;
}

inline VkGraphicsPipelineLibraryCreateInfoEXT makeGraphicsPipelineLibraryCreateInfo (const VkGraphicsPipelineLibraryFlagsEXT flags)
{
	const VkGraphicsPipelineLibraryCreateInfoEXT	graphicsPipelineLibraryCreateInfo =
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,	//  VkStructureType						sType;
		DE_NULL,														//  void*								pNext;
		flags,															//  VkGraphicsPipelineLibraryFlagsEXT	flags;
	};

	return graphicsPipelineLibraryCreateInfo;
}

inline VkPipelineLibraryCreateInfoKHR makePipelineLibraryCreateInfo (const std::vector<VkPipeline>& pipelineLibraries)
{
	const deUint32							libraryCount				= static_cast<deUint32>(pipelineLibraries.size());
	const VkPipeline*						libraries					= de::dataOrNull(pipelineLibraries);
	const VkPipelineLibraryCreateInfoKHR	pipelineLibraryCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR,		//  VkStructureType		sType;
		DE_NULL,												//  const void*			pNext;
		libraryCount,											//  deUint32			libraryCount;
		libraries,												//  const VkPipeline*	pLibraries;
	};

	return pipelineLibraryCreateInfo;
}

inline std::string getGraphicsPipelineLibraryFlagsString (const VkGraphicsPipelineLibraryFlagsEXT flags)
{
	std::string result;

	if ((flags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT) != 0)		result += "VERTEX_INPUT_INTERFACE ";
	if ((flags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT) != 0)	result += "PRE_RASTERIZATION_SHADERS ";
	if ((flags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT) != 0)			result += "FRAGMENT_SHADER ";
	if ((flags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT) != 0)	result += "FRAGMENT_OUTPUT_INTERFACE ";

	if (!result.empty())
		result.resize(result.size() - 1);

	return result;
};

VkImageCreateInfo makeColorImageCreateInfo (const VkFormat format, const deUint32 width, const deUint32 height)
{
	const VkImageUsageFlags	usage		= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	const VkImageCreateInfo	imageInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//  VkStructureType			sType;
		DE_NULL,								//  const void*				pNext;
		(VkImageCreateFlags)0,					//  VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//  VkImageType				imageType;
		format,									//  VkFormat				format;
		makeExtent3D(width, height, 1),			//  VkExtent3D				extent;
		1u,										//  deUint32				mipLevels;
		1u,										//  deUint32				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//  VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//  VkImageTiling			tiling;
		usage,									//  VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//  VkSharingMode			sharingMode;
		0u,										//  deUint32				queueFamilyIndexCount;
		DE_NULL,								//  const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//  VkImageLayout			initialLayout;
	};

	return imageInfo;
}

VkImageViewCreateInfo makeImageViewCreateInfo (VkImage image, VkFormat format, VkImageAspectFlags aspectMask)
{
	const VkComponentMapping		components			=
	{
		VK_COMPONENT_SWIZZLE_R,
		VK_COMPONENT_SWIZZLE_G,
		VK_COMPONENT_SWIZZLE_B,
		VK_COMPONENT_SWIZZLE_A,
	};
	const VkImageSubresourceRange	subresourceRange	=
	{
		aspectMask,	//  VkImageAspectFlags	aspectMask;
		0,			//  deUint32			baseMipLevel;
		1,			//  deUint32			levelCount;
		0,			//  deUint32			baseArrayLayer;
		1,			//  deUint32			layerCount;
	};
	const VkImageViewCreateInfo		result				=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	//  VkStructureType			sType;
		DE_NULL,									//  const void*				pNext;
		0u,											//  VkImageViewCreateFlags	flags;
		image,										//  VkImage					image;
		VK_IMAGE_VIEW_TYPE_2D,						//  VkImageViewType			viewType;
		format,										//  VkFormat				format;
		components,									//  VkComponentMapping		components;
		subresourceRange,							//  VkImageSubresourceRange	subresourceRange;
	};

	return result;
}

VkImageCreateInfo makeDepthImageCreateInfo (const VkFormat format, const deUint32 width, const deUint32 height)
{
	const VkImageUsageFlags	usage		= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	const VkImageCreateInfo	imageInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//  VkStructureType			sType;
		DE_NULL,								//  const void*				pNext;
		(VkImageCreateFlags)0,					//  VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//  VkImageType				imageType;
		format,									//  VkFormat				format;
		makeExtent3D(width, height, 1),			//  VkExtent3D				extent;
		1u,										//  deUint32				mipLevels;
		1u,										//  deUint32				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//  VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//  VkImageTiling			tiling;
		usage,									//  VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//  VkSharingMode			sharingMode;
		0u,										//  deUint32				queueFamilyIndexCount;
		DE_NULL,								//  const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//  VkImageLayout			initialLayout;
	};

	return imageInfo;
}

const VkFramebufferCreateInfo makeFramebufferCreateInfo (const VkRenderPass	renderPass,
														 const deUint32		attachmentCount,
														 const VkImageView*	attachments,
														 const deUint32		width,
														 const deUint32		height)
{
	const VkFramebufferCreateInfo	result			=
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	//  VkStructureType				sType;
		DE_NULL,									//  const void*					pNext;
		0,											//  VkFramebufferCreateFlags	flags;
		renderPass,									//  VkRenderPass				renderPass;
		attachmentCount,							//  deUint32					attachmentCount;
		attachments,								//  const VkImageView*			pAttachments;
		width,										//  deUint32					width;
		height,										//  deUint32					height;
		1,											//  deUint32					layers;
	};

	return result;
}

const VkPipelineMultisampleStateCreateInfo makePipelineMultisampleStateCreateInfo (void)
{
	const VkPipelineMultisampleStateCreateInfo		pipelineMultisampleStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	//  VkStructureType							sType;
		DE_NULL,													//  const void*								pNext;
		0u,															//  VkPipelineMultisampleStateCreateFlags	flags;
		VK_SAMPLE_COUNT_1_BIT,										//  VkSampleCountFlagBits					rasterizationSamples;
		DE_FALSE,													//  VkBool32								sampleShadingEnable;
		0.0f,														//  float									minSampleShading;
		DE_NULL,													//  const VkSampleMask*						pSampleMask;
		DE_FALSE,													//  VkBool32								alphaToCoverageEnable;
		DE_FALSE,													//  VkBool32								alphaToOneEnable;
	};

	return pipelineMultisampleStateCreateInfo;
}

class GraphicsPipelineCreateInfo : public ::vkt::Draw::PipelineCreateInfo
{
public:
	GraphicsPipelineCreateInfo (vk::VkPipelineLayout		_layout,
								vk::VkRenderPass			_renderPass,
								int							_subpass,
								vk::VkPipelineCreateFlags	_flags)
		: ::vkt::Draw::PipelineCreateInfo	(_layout, _renderPass, _subpass, _flags)
		, m_vertexInputBindingDescription	()
		, m_vertexInputAttributeDescription	()
		, m_shaderModuleCreateInfoCount		(0)
		, m_shaderModuleCreateInfo			{ initVulkanStructure(), initVulkanStructure() }
		, m_pipelineShaderStageCreateInfo	()
		, m_vertModule						()
		, m_fragModule						()
	{
	}

	VkVertexInputBindingDescription					m_vertexInputBindingDescription;
	VkVertexInputAttributeDescription				m_vertexInputAttributeDescription;
	deUint32										m_shaderModuleCreateInfoCount;
	VkShaderModuleCreateInfo						m_shaderModuleCreateInfo[2];
	std::vector<VkPipelineShaderStageCreateInfo>	m_pipelineShaderStageCreateInfo;
	Move<VkShaderModule>							m_vertModule;
	Move<VkShaderModule>							m_fragModule;
};

void updateVertexInputInterface (Context&						context,
								 GraphicsPipelineCreateInfo&	graphicsPipelineCreateInfo)
{
	DE_UNREF(context);

	graphicsPipelineCreateInfo.m_vertexInputBindingDescription =
	{
		0u,									//  deUint32			binding;
		sizeof(tcu::Vec4),					//  deUint32			strideInBytes;
		VK_VERTEX_INPUT_RATE_VERTEX,		//  VkVertexInputRate	inputRate;
	};
	graphicsPipelineCreateInfo.m_vertexInputAttributeDescription =
	{
		0u,									//  deUint32	location;
		0u,									//  deUint32	binding;
		VK_FORMAT_R32G32B32A32_SFLOAT,		//  VkFormat	format;
		0u									//  deUint32	offsetInBytes;
	};

	const VkPipelineVertexInputStateCreateInfo		vertexInputStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		0u,																// VkPipelineVertexInputStateCreateFlags	flags;
		1u,																// deUint32									vertexBindingDescriptionCount;
		&graphicsPipelineCreateInfo.m_vertexInputBindingDescription,	// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
		1u,																// deUint32									vertexAttributeDescriptionCount;
		&graphicsPipelineCreateInfo.m_vertexInputAttributeDescription,	// const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
	};
	const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		0u,																// VkPipelineInputAssemblyStateCreateFlags	flags;
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// VkPrimitiveTopology						topology;
		VK_FALSE,														// VkBool32									primitiveRestartEnable;
	};

	graphicsPipelineCreateInfo.addState(vertexInputStateCreateInfo);
	graphicsPipelineCreateInfo.addState(inputAssemblyStateCreateInfo);
}

void updatePreRasterization (Context&						context,
							 GraphicsPipelineCreateInfo&	graphicsPipelineCreateInfo,
							 bool							delayedShaderCreate)
{
	const ProgramBinary&		shaderBinary			= context.getBinaryCollection().get("vert");
	VkShaderModuleCreateInfo&	shaderModuleCreateInfo	= graphicsPipelineCreateInfo.m_shaderModuleCreateInfo[graphicsPipelineCreateInfo.m_shaderModuleCreateInfoCount];

	DE_ASSERT(graphicsPipelineCreateInfo.m_shaderModuleCreateInfoCount < DE_LENGTH_OF_ARRAY(graphicsPipelineCreateInfo.m_shaderModuleCreateInfo));

	shaderModuleCreateInfo	=
	{
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,	//  VkStructureType				sType;
		DE_NULL,										//  const void*					pNext;
		0u,												//  VkShaderModuleCreateFlags	flags;
		(deUintptr)shaderBinary.getSize(),				//  deUintptr					codeSize;
		(deUint32*)shaderBinary.getBinary(),			//  const deUint32*				pCode;
	};

	if (!delayedShaderCreate)
	{
		const DeviceInterface&	vk		= context.getDeviceInterface();
		const VkDevice			device	= context.getDevice();

		graphicsPipelineCreateInfo.m_vertModule = createShaderModule(vk, device, &shaderModuleCreateInfo);
	}

	const void*										pNext								= delayedShaderCreate
																						? &shaderModuleCreateInfo
																						: DE_NULL;
	const VkShaderModule							shaderModule						= delayedShaderCreate
																						? DE_NULL
																						: *graphicsPipelineCreateInfo.m_vertModule;
	const VkPipelineShaderStageCreateInfo			pipelineShaderStageCreateInfo				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType;
		pNext,													// const void*						pNext;
		0u,														// VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_VERTEX_BIT,								// VkShaderStageFlagBits			stage;
		shaderModule,											// VkShaderModule					module;
		"main",													// const char*						pName;
		DE_NULL													// const VkSpecializationInfo*		pSpecializationInfo;
	};

	shaderBinary.setUsed();

	// Within the VkPipelineLayout, all	bindings that affect the specified shader stages
	const VkViewport								viewport							= makeViewport(RENDER_SIZE_WIDTH, RENDER_SIZE_HEIGHT);
	const VkRect2D									scissor								= makeRect2D(3 * RENDER_SIZE_WIDTH / 4, RENDER_SIZE_HEIGHT);
	const VkPipelineViewportStateCreateInfo			pipelineViewportStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType						sType;
		DE_NULL,												// const void*							pNext;
		0u,														// VkPipelineViewportStateCreateFlags	flags;
		1u,														// deUint32								viewportCount;
		&viewport,												// const VkViewport*					pViewports;
		1u,														// deUint32								scissorCount;
		&scissor												// const VkRect2D*						pScissors;
	};
	const VkPipelineRasterizationStateCreateInfo	pipelineRasterizationStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		0u,																// VkPipelineRasterizationStateCreateFlags	flags;
		VK_FALSE,														// VkBool32									depthClampEnable;
		VK_FALSE,														// VkBool32									rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
		VK_CULL_MODE_NONE,												// VkCullModeFlags							cullMode;
		VK_FRONT_FACE_COUNTER_CLOCKWISE,								// VkFrontFace								frontFace;
		VK_FALSE,														// VkBool32									depthBiasEnable;
		0.0f,															// float									depthBiasConstantFactor;
		0.0f,															// float									depthBiasClamp;
		0.0f,															// float									depthBiasSlopeFactor;
		1.0f,															// float									lineWidth;
	};

	graphicsPipelineCreateInfo.m_shaderModuleCreateInfoCount++;

	graphicsPipelineCreateInfo.addShader(pipelineShaderStageCreateInfo);
	graphicsPipelineCreateInfo.addState(pipelineViewportStateCreateInfo);
	graphicsPipelineCreateInfo.addState(pipelineRasterizationStateCreateInfo);
}

void updatePostRasterization (Context&						context,
							  GraphicsPipelineCreateInfo&	graphicsPipelineCreateInfo,
							  bool							delayedShaderCreate)
{
	const ProgramBinary&		shaderBinary			= context.getBinaryCollection().get("frag");
	VkShaderModuleCreateInfo&	shaderModuleCreateInfo	= graphicsPipelineCreateInfo.m_shaderModuleCreateInfo[graphicsPipelineCreateInfo.m_shaderModuleCreateInfoCount];

	DE_ASSERT(graphicsPipelineCreateInfo.m_shaderModuleCreateInfoCount < DE_LENGTH_OF_ARRAY(graphicsPipelineCreateInfo.m_shaderModuleCreateInfo));

	shaderModuleCreateInfo	=
	{
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,	//  VkStructureType				sType;
		DE_NULL,										//  const void*					pNext;
		0u,												//  VkShaderModuleCreateFlags	flags;
		(deUintptr)shaderBinary.getSize(),				//  deUintptr					codeSize;
		(deUint32*)shaderBinary.getBinary(),			//  const deUint32*				pCode;
	};

	if (!delayedShaderCreate)
	{
		const DeviceInterface&	vk		= context.getDeviceInterface();
		const VkDevice			device	= context.getDevice();

		graphicsPipelineCreateInfo.m_fragModule = createShaderModule(vk, device, &shaderModuleCreateInfo);
	}

	const void*										pNext								= delayedShaderCreate
																						? &shaderModuleCreateInfo
																						: DE_NULL;
	const VkShaderModule							shaderModule						= delayedShaderCreate
																						? DE_NULL
																						: *graphicsPipelineCreateInfo.m_fragModule;
	const VkPipelineShaderStageCreateInfo			pipelineShaderStageCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType					sType;
		pNext,													// const void*						pNext;
		0u,														// VkPipelineShaderStageCreateFlags	flags;
		VK_SHADER_STAGE_FRAGMENT_BIT,							// VkShaderStageFlagBits			stage;
		shaderModule,											// VkShaderModule					module;
		"main",													// const char*						pName;
		DE_NULL													// const VkSpecializationInfo*		pSpecializationInfo;
	};

	shaderBinary.setUsed();

	// Within the VkPipelineLayout, all bindings that affect the fragment shader stage

	const VkPipelineDepthStencilStateCreateInfo		pipelineDepthStencilStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, //  VkStructureType							sType;
		DE_NULL,													//  const void*								pNext;
		0u,															//  VkPipelineDepthStencilStateCreateFlags	flags;
		VK_TRUE,													//  VkBool32								depthTestEnable;
		VK_TRUE,													//  VkBool32								depthWriteEnable;
		VK_COMPARE_OP_LESS_OR_EQUAL,								//  VkCompareOp								depthCompareOp;
		VK_FALSE,													//  VkBool32								depthBoundsTestEnable;
		VK_FALSE,													//  VkBool32								stencilTestEnable;
		{															//  VkStencilOpState						front;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
			VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
			0u,						// deUint32		compareMask;
			0u,						// deUint32		writeMask;
			0u,						// deUint32		reference;
		},
		{															//  VkStencilOpState						back;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
			VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
			0u,						// deUint32		compareMask;
			0u,						// deUint32		writeMask;
			0u,						// deUint32		reference;
		},
		0.0f,														//  float									minDepthBounds;
		1.0f,														//  float									maxDepthBounds;
	};

	graphicsPipelineCreateInfo.m_shaderModuleCreateInfoCount++;
	graphicsPipelineCreateInfo.addShader(pipelineShaderStageCreateInfo);

	DE_ASSERT(graphicsPipelineCreateInfo.pDepthStencilState == DE_NULL);
	graphicsPipelineCreateInfo.addState(pipelineDepthStencilStateCreateInfo);

	if (graphicsPipelineCreateInfo.pMultisampleState == DE_NULL)
	{
		const VkPipelineMultisampleStateCreateInfo	pipelineMultisampleStateCreateInfo = makePipelineMultisampleStateCreateInfo();

		graphicsPipelineCreateInfo.addState(pipelineMultisampleStateCreateInfo);
	}
}

void updateFragmentOutputInterface (Context&					context,
									GraphicsPipelineCreateInfo& graphicsPipelineCreateInfo)
{
	DE_UNREF(context);

	// Number of blend attachments must equal the number of color attachments during any subpass.
	const VkPipelineColorBlendAttachmentState	pipelineColorBlendAttachmentState =
	{
		VK_FALSE,						// VkBool32					blendEnable;
		VK_BLEND_FACTOR_ONE,			// VkBlendFactor			srcColorBlendFactor;
		VK_BLEND_FACTOR_ZERO,			// VkBlendFactor			dstColorBlendFactor;
		VK_BLEND_OP_ADD,				// VkBlendOp				colorBlendOp;
		VK_BLEND_FACTOR_ONE,			// VkBlendFactor			srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ZERO,			// VkBlendFactor			dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,				// VkBlendOp				alphaBlendOp;
		COLOR_COMPONENTS_NO_RED,		// VkColorComponentFlags	colorWriteMask;
	};
	const VkPipelineColorBlendStateCreateInfo	pipelineColorBlendStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,		// VkStructureType								sType;
		DE_NULL,														// const void*									pNext;
		(VkPipelineColorBlendStateCreateFlags)0,						// VkPipelineColorBlendStateCreateFlags			flags;
		VK_FALSE,														// VkBool32										logicOpEnable;
		VK_LOGIC_OP_COPY,												// VkLogicOp									logicOp;
		1u,																// deUint32										attachmentCount;
		&pipelineColorBlendAttachmentState,								// const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },										// float										blendConstants[4];
	};

	graphicsPipelineCreateInfo.addState(pipelineColorBlendStateCreateInfo);

	if (graphicsPipelineCreateInfo.pMultisampleState == DE_NULL)
	{
		const VkPipelineMultisampleStateCreateInfo	pipelineMultisampleStateCreateInfo = makePipelineMultisampleStateCreateInfo();

		graphicsPipelineCreateInfo.addState(pipelineMultisampleStateCreateInfo);
	}
}

/*
	To test that each of graphics pipeline libraries have influence on final pipeline
	the functions have following features:

	updateVertexInputInterface
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
		VK_VERTEX_INPUT_RATE_VERTEX
		Z is read from uniform and written in shader

	updatePreRasterization
		VkRect2D scissor = makeRect2D(3 * RENDER_SIZE_WIDTH / 4, RENDER_SIZE_HEIGHT);

	updatePostRasterization
		Fragment shader top and bottom colors read from uniform buffer

	updateFragmentOutputInterface
		Cut off red component
*/

class PipelineLibraryTestInstance : public TestInstance
{
public:
									PipelineLibraryTestInstance		(Context&							context,
																	 const TestParams&					data);
									~PipelineLibraryTestInstance	(void);
	tcu::TestStatus					iterate							(void);

protected:
	de::MovePtr<BufferWithMemory>	makeVertexBuffer				(void);
	de::MovePtr<BufferWithMemory>	makeZCoordBuffer				(void);
	de::MovePtr<BufferWithMemory>	makePaletteBuffer				(void);
	Move<VkDescriptorPool>			createDescriptorPool			(void);
	Move<VkDescriptorSetLayout>		createDescriptorSetLayout		(const VkBuffer						vertShaderBuffer,
																	 const VkBuffer						fragShaderBuffer);
	Move<VkDescriptorSet>			createDescriptorSet				(const VkDescriptorPool				pool,
																	 const VkDescriptorSetLayout		layout,
																	 const VkBuffer						vertShaderBuffer,
																	 const VkBuffer						fragShaderBuffer);
	bool							verifyColorImage				(const tcu::ConstPixelBufferAccess&	pba);
	bool							verifyDepthImage				(const tcu::ConstPixelBufferAccess&	pba);
	bool							runTest							(RuntimePipelineTreeConfiguration&	runtimePipelineTreeConfiguration,
																	 const bool							optimize,
																	 const bool							delayedShaderCreate);
private:
	TestParams						m_data;
	std::vector<tcu::Vec4>			m_vertexData;
	std::vector<tcu::Vec4>			m_paletteData;
	std::vector<tcu::Vec4>			m_zCoordData;
};

PipelineLibraryTestInstance::PipelineLibraryTestInstance (Context& context, const TestParams& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
	, m_vertexData			()
	, m_paletteData			()
{
	m_vertexData =
	{
		{ -1.0f, -1.0f, 0.0f, 1.0f },
		{ +1.0f, -1.0f, 0.5f, 1.0f },
		{ -1.0f, +1.0f, 0.5f, 1.0f },
		{ -1.0f, +1.0f, 0.5f, 1.0f },
		{ +1.0f, -1.0f, 0.5f, 1.0f },
		{ +1.0f, +1.0f, 1.0f, 1.0f },
	};
	m_paletteData =
	{
		{ 0.25f, 1.0f, 0.0f, 1.0f },
		{ 0.75f, 0.0f, 1.0f, 1.0f },
	};
	m_zCoordData =
	{
		{ 0.25f, 0.75f, 0.0f, 1.0f },
	};
}

PipelineLibraryTestInstance::~PipelineLibraryTestInstance (void)
{
}

de::MovePtr<BufferWithMemory> PipelineLibraryTestInstance::makeVertexBuffer (void)
{
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					device				= m_context.getDevice();
	Allocator&						allocator			= m_context.getDefaultAllocator();
	const size_t					bufferDataSize		= de::dataSize(m_vertexData);
	const VkBufferCreateInfo		bufferCreateInfo	= makeBufferCreateInfo(bufferDataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	de::MovePtr<BufferWithMemory>	buffer				= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));

	deMemcpy(buffer->getAllocation().getHostPtr(), m_vertexData.data(), bufferDataSize);
	flushAlloc(vk, device, buffer->getAllocation());

	return buffer;
}

de::MovePtr<BufferWithMemory> PipelineLibraryTestInstance::makeZCoordBuffer (void)
{
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					device				= m_context.getDevice();
	Allocator&						allocator			= m_context.getDefaultAllocator();
	const size_t					bufferDataSize		= de::dataSize(m_zCoordData);
	const VkBufferCreateInfo		bufferCreateInfo	= makeBufferCreateInfo(bufferDataSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	de::MovePtr<BufferWithMemory>	buffer				= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));

	deMemcpy(buffer->getAllocation().getHostPtr(), m_zCoordData.data(), bufferDataSize);
	flushAlloc(vk, device, buffer->getAllocation());

	return buffer;
}

de::MovePtr<BufferWithMemory> PipelineLibraryTestInstance::makePaletteBuffer (void)
{
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					device				= m_context.getDevice();
	Allocator&						allocator			= m_context.getDefaultAllocator();
	const size_t					bufferDataSize		= de::dataSize(m_paletteData);
	const VkBufferCreateInfo		bufferCreateInfo	= makeBufferCreateInfo(bufferDataSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	de::MovePtr<BufferWithMemory>	buffer				= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));

	deMemcpy(buffer->getAllocation().getHostPtr(), m_paletteData.data(), bufferDataSize);
	flushAlloc(vk, device, buffer->getAllocation());

	return buffer;
}

Move<VkDescriptorPool> PipelineLibraryTestInstance::createDescriptorPool (void)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	return DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 3);
}

Move<VkDescriptorSetLayout> PipelineLibraryTestInstance::createDescriptorSetLayout (const VkBuffer			vertShaderBuffer,
																					const VkBuffer			fragShaderBuffer)
{
	const DeviceInterface&		vk		= m_context.getDeviceInterface();
	const VkDevice				device	= m_context.getDevice();
	DescriptorSetLayoutBuilder	builder;

	if (vertShaderBuffer != DE_NULL)
		builder.addIndexedBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, VK_SHADER_STAGE_VERTEX_BIT, 0u, DE_NULL);

	if (fragShaderBuffer != DE_NULL)
		builder.addIndexedBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, VK_SHADER_STAGE_FRAGMENT_BIT, 1u, DE_NULL);

	return builder.build(vk, device);
}

Move<VkDescriptorSet> PipelineLibraryTestInstance::createDescriptorSet (const VkDescriptorPool		pool,
																		const VkDescriptorSetLayout	layout,
																		const VkBuffer				vertShaderBuffer,
																		const VkBuffer				fragShaderBuffer)
{
	const DeviceInterface&				vk					= m_context.getDeviceInterface();
	const VkDevice						device				= m_context.getDevice();
	const VkDescriptorSetAllocateInfo	allocInfo			=
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	//  VkStructureType					sType;
		DE_NULL,										//  const void*						pNext;
		pool,											//  VkDescriptorPool				descriptorPool;
		1u,												//  deUint32						descriptorSetCount;
		&layout											//  const VkDescriptorSetLayout*	pSetLayouts;
	};
	Move<VkDescriptorSet>				descriptorSet		= allocateDescriptorSet(vk, device, &allocInfo);
	DescriptorSetUpdateBuilder			builder;

	if (vertShaderBuffer != DE_NULL)
	{
		const VkDeviceSize				vertShaderBufferSize	= de::dataSize(m_zCoordData);
		const VkDescriptorBufferInfo	vertShaderBufferInfo	= makeDescriptorBufferInfo(vertShaderBuffer, 0u, vertShaderBufferSize);

		builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &vertShaderBufferInfo);
	}

	if (fragShaderBuffer != DE_NULL)
	{
		const VkDeviceSize				fragShaderBufferSize	= de::dataSize(m_paletteData);
		const VkDescriptorBufferInfo	fragShaderBufferInfo	= makeDescriptorBufferInfo(fragShaderBuffer, 0u, fragShaderBufferSize);

		builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &fragShaderBufferInfo);
	}

	builder.update(vk, device);

	return descriptorSet;
}

bool PipelineLibraryTestInstance::runTest (RuntimePipelineTreeConfiguration&	runtimePipelineTreeConfiguration,
										   const bool							optimize,
										   const bool							delayedShaderCreate)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	Allocator&								allocator				= m_context.getDefaultAllocator();
	tcu::TestLog&							log						= m_context.getTestContext().getLog();
	const VkFormat							colorFormat				= VK_FORMAT_R8G8B8A8_UNORM;
	const VkFormat							depthFormat				= VK_FORMAT_D32_SFLOAT;
	const VkGraphicsPipelineLibraryFlagsEXT	vertPipelineFlags		= static_cast<VkGraphicsPipelineLibraryFlagsEXT>(VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT);
	const VkGraphicsPipelineLibraryFlagsEXT	fragPipelineFlags		= static_cast<VkGraphicsPipelineLibraryFlagsEXT>(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT);
	const VkGraphicsPipelineLibraryFlagsEXT	samePipelineFlags		= vertPipelineFlags | fragPipelineFlags;
	const deInt32							nodeNdxLast				= static_cast<deInt32>(runtimePipelineTreeConfiguration.size()) - 1;
	const Move<VkRenderPass>				renderPass				= makeRenderPass(vk, device, colorFormat, depthFormat);
	const de::MovePtr<BufferWithMemory>		zCoordBuffer			= makeZCoordBuffer();
	const de::MovePtr<BufferWithMemory>		paletteBuffer			= makePaletteBuffer();
	const Move<VkDescriptorPool>			descriptorPool			= createDescriptorPool();

	const Move<VkDescriptorSetLayout>		descriptorSetLayoutBlank	= createDescriptorSetLayout(DE_NULL, DE_NULL);

	const Move<VkDescriptorSetLayout>		descriptorSetLayoutVert	= createDescriptorSetLayout(**zCoordBuffer, DE_NULL);
	const Move<VkDescriptorSetLayout>		descriptorSetLayoutFrag	= createDescriptorSetLayout(DE_NULL, **paletteBuffer);
	const Move<VkDescriptorSetLayout>		descriptorSetLayoutBoth	= createDescriptorSetLayout(**zCoordBuffer, **paletteBuffer);
	const Move<VkDescriptorSet>				descriptorSetVert		= createDescriptorSet(*descriptorPool, *descriptorSetLayoutVert, **zCoordBuffer, DE_NULL);
	const Move<VkDescriptorSet>				descriptorSetFrag		= createDescriptorSet(*descriptorPool, *descriptorSetLayoutFrag, DE_NULL , **paletteBuffer);

	VkDescriptorSet vecDescriptorSetBoth[2] = { *descriptorSetVert, *descriptorSetFrag };

	VkDescriptorSetLayout vecLayoutVert[2] = { *descriptorSetLayoutVert, *descriptorSetLayoutBlank };
	VkDescriptorSetLayout vecLayoutFrag[2] = { *descriptorSetLayoutBlank, *descriptorSetLayoutFrag };
	VkDescriptorSetLayout vecLayoutBoth[2] = { *descriptorSetLayoutVert, *descriptorSetLayoutFrag };

	const Move<VkPipelineLayout>			pipelineLayoutVert		= makePipelineLayout(vk, device, 2, vecLayoutVert);
	const Move<VkPipelineLayout>			pipelineLayoutFrag		= makePipelineLayout(vk, device, 2, vecLayoutFrag);
	const Move<VkPipelineLayout>			pipelineLayoutSame		= makePipelineLayout(vk, device, 2, vecLayoutBoth);
	const Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	const Move<VkCommandBuffer>				cmdBuffer				= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	Move<VkPipeline>						rootPipeline;

	// Go through tree nodes and create library for each up to root
	for (deInt32 nodeNdx = nodeNdxLast; nodeNdx >= 0 ; --nodeNdx)	// We expect only backward node reference, thus build pipielines from end is safe
	{
		RuntimePipelineTreeNode&				node								= runtimePipelineTreeConfiguration[nodeNdx];
		const bool								buildLibrary						= (nodeNdx != 0);
		const VkPipelineCreateFlags				pipelineCreateFlags					= calcPipelineCreateFlags(optimize, buildLibrary);
		const VkGraphicsPipelineLibraryFlagsEXT	subtreeGraphicsPipelineLibraryFlags	= node.subtreeGraphicsPipelineLibraryFlags | node.graphicsPipelineLibraryFlags;
		bool									samePipelineLayout					= samePipelineFlags == (samePipelineFlags & subtreeGraphicsPipelineLibraryFlags);
		bool									vertPipelineLayout					= vertPipelineFlags == (vertPipelineFlags & subtreeGraphicsPipelineLibraryFlags);
		bool									fragPipelineLayout					= fragPipelineFlags == (fragPipelineFlags & subtreeGraphicsPipelineLibraryFlags);
		const VkPipelineLayout					pipelineLayout						= samePipelineLayout ? *pipelineLayoutSame
																					: vertPipelineLayout ? *pipelineLayoutVert
																					: fragPipelineLayout ? *pipelineLayoutFrag
																					: DE_NULL;
		const VkRenderPass						renderPassHandle					= getRenderPass(node.graphicsPipelineLibraryFlags, *renderPass);
		VkGraphicsPipelineLibraryCreateInfoEXT	graphicsPipelineLibraryCreateInfo	= makeGraphicsPipelineLibraryCreateInfo(node.graphicsPipelineLibraryFlags);
		VkPipelineLibraryCreateInfoKHR			linkingInfo							= makePipelineLibraryCreateInfo(node.pipelineLibraries);
		GraphicsPipelineCreateInfo				graphicsPipelineCreateInfo			(pipelineLayout, renderPassHandle, 0, pipelineCreateFlags);

		for (const auto subsetFlag: GRAPHICS_PIPELINE_LIBRARY_FLAGS)
		{
			if ((node.graphicsPipelineLibraryFlags & subsetFlag) != 0)
			{
				switch (subsetFlag)
				{
					case VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT:		updateVertexInputInterface(m_context, graphicsPipelineCreateInfo);					break;
					case VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT:	updatePreRasterization(m_context, graphicsPipelineCreateInfo, delayedShaderCreate);	break;
					case VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT:				updatePostRasterization(m_context, graphicsPipelineCreateInfo, delayedShaderCreate);break;
					case VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT:	updateFragmentOutputInterface(m_context, graphicsPipelineCreateInfo);				break;
					default:																TCU_THROW(InternalError, "Unknown pipeline subset");
				}
			}
		}

		if (isPartialFlagSubset(graphicsPipelineLibraryCreateInfo.flags, ALL_GRAPHICS_PIPELINE_LIBRARY_FLAGS))
			appendStructurePtrToVulkanChain(&graphicsPipelineCreateInfo.pNext, &graphicsPipelineLibraryCreateInfo);

		if (linkingInfo.libraryCount != 0)
			appendStructurePtrToVulkanChain(&graphicsPipelineCreateInfo.pNext, &linkingInfo);

		node.pipeline = createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineCreateInfo);

		if (buildLibrary)
		{
			DE_ASSERT(de::inBounds(node.parentIndex, 0, static_cast<deInt32>(runtimePipelineTreeConfiguration.size())));

			runtimePipelineTreeConfiguration[node.parentIndex].pipelineLibraries.push_back(*node.pipeline);
		}
		else
		{
			DE_ASSERT(node.parentIndex == -1);

			rootPipeline = node.pipeline;
		}
	}

	// Queue commands and read results.
	{
		const tcu::UVec2					renderSize					= { RENDER_SIZE_WIDTH, RENDER_SIZE_HEIGHT };
		const VkRect2D						renderArea					= makeRect2D(renderSize.x(), renderSize.y());
		const de::MovePtr<BufferWithMemory>	vertexBuffer				= makeVertexBuffer();
		const deUint32						vertexCount					= static_cast<deUint32>(m_vertexData.size());
		const VkDeviceSize					vertexBufferOffset			= 0;
		const Vec4							colorClearColor				(0.0f, 0.0f, 0.0f, 1.0f);
		const VkImageCreateInfo				colorImageCreateInfo		= makeColorImageCreateInfo(colorFormat, renderSize.x(), renderSize.y());
		const ImageWithMemory				colorImage					(vk, device, allocator, colorImageCreateInfo, MemoryRequirement::Any);
		const VkImageViewCreateInfo			colorImageViewCreateInfo	= makeImageViewCreateInfo(*colorImage, colorFormat, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT));
		const Move<VkImageView>				colorImageView				= createImageView(vk, device, &colorImageViewCreateInfo);
		const VkImageCreateInfo				depthImageCreateInfo		= makeDepthImageCreateInfo(depthFormat, renderSize.x(), renderSize.y());
		const ImageWithMemory				depthImage					(vk, device, allocator, depthImageCreateInfo, MemoryRequirement::Any);
		const VkImageViewCreateInfo			depthImageViewCreateInfo	= makeImageViewCreateInfo(*depthImage, depthFormat, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT));
		const Move<VkImageView>				depthImageView				= createImageView(vk, device, &depthImageViewCreateInfo);
		const float							depthClearDepth				= 1.0f;
		const deUint32						depthClearStencil			= 0u;
		const VkDeviceSize					colorBufferDataSize			= static_cast<VkDeviceSize>(renderSize.x() * renderSize.y() * tcu::getPixelSize(mapVkFormat(colorFormat)));
		const VkBufferCreateInfo			colorBufferCreateInfo		= makeBufferCreateInfo(colorBufferDataSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		const BufferWithMemory				colorBuffer					(vk, device, allocator, colorBufferCreateInfo, MemoryRequirement::HostVisible);
		const VkDeviceSize					depthBufferDataSize			= static_cast<VkDeviceSize>(renderSize.x() * renderSize.y() * tcu::getPixelSize(mapVkFormat(depthFormat)));
		const VkBufferCreateInfo			depthBufferCreateInfo		= makeBufferCreateInfo(depthBufferDataSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		const BufferWithMemory				depthBuffer					(vk, device, allocator, depthBufferCreateInfo, MemoryRequirement::HostVisible);
		const VkImageView					attachments[]				= { *colorImageView, *depthImageView };
		const VkFramebufferCreateInfo		framebufferCreateInfo		= makeFramebufferCreateInfo(*renderPass, DE_LENGTH_OF_ARRAY(attachments), attachments, renderSize.x(), renderSize.y());
		const Move<VkFramebuffer>			framebuffer					= createFramebuffer(vk, device, &framebufferCreateInfo);

		beginCommandBuffer(vk, *cmdBuffer, 0u);
		{
			beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, colorClearColor, depthClearDepth, depthClearStencil);
			{
				vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer->get(), &vertexBufferOffset);
				vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *rootPipeline);
				vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayoutSame, 0u, 2u, vecDescriptorSetBoth, 0u, DE_NULL);
				vk.cmdDraw(*cmdBuffer, vertexCount, 1u, 0u, 0u);
			}
			endRenderPass(vk, *cmdBuffer);

			const tcu::IVec2 size = { (deInt32)renderSize.x(), (deInt32)renderSize.y() };
			copyImageToBuffer(vk, *cmdBuffer, *colorImage, *colorBuffer, size);
			copyImageToBuffer(vk, *cmdBuffer, *depthImage, *depthBuffer, size, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1u, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
		}
		endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), cmdBuffer.get());

		invalidateAlloc(vk, device, colorBuffer.getAllocation());
		invalidateAlloc(vk, device, depthBuffer.getAllocation());

		const tcu::ConstPixelBufferAccess colorPixelAccess(mapVkFormat(colorFormat), renderSize.x(), renderSize.y(), 1, colorBuffer.getAllocation().getHostPtr());
		const tcu::ConstPixelBufferAccess depthPixelAccess(mapVkFormat(depthFormat), renderSize.x(), renderSize.y(), 1, depthBuffer.getAllocation().getHostPtr());

		if (!verifyColorImage(colorPixelAccess))
		{
			log << tcu::TestLog::Image("color", "Rendered image", colorPixelAccess);

			return false;
		}

		if (!verifyDepthImage(depthPixelAccess))
		{
			log << tcu::TestLog::Image("depth", "Rendered image", depthPixelAccess);

			return false;
		}
	}

	return true;
}

bool PipelineLibraryTestInstance::verifyColorImage (const ConstPixelBufferAccess& pba)
{
	tcu::TestLog&		log				= m_context.getTestContext().getLog();
	TextureLevel		referenceImage	(pba.getFormat(), pba.getWidth(), pba.getHeight());
	PixelBufferAccess	reference		(referenceImage);
	const int			horzEdge		= 3 * reference.getWidth() / 4;
	const int			vertEdge		= reference.getHeight() / 2;
	const UVec4			green			= ivec2uvec(RGBA::green().toIVec());
	const UVec4			blue			= ivec2uvec(RGBA::blue().toIVec());
	const UVec4			black			= ivec2uvec(RGBA::black().toIVec());

	for (int y = 0; y < reference.getHeight(); ++y)
	{
		for (int x = 0; x < reference.getWidth(); ++x)
		{
			if (x < horzEdge)
			{
				if (y < vertEdge)
					reference.setPixel(green, x, y);
				else
					reference.setPixel(blue, x, y);
			}
			else
				reference.setPixel(black, x, y);
		}
	}

	return intThresholdCompare(log, "colorImage", "colorImage", reference, pba, UVec4(), COMPARE_LOG_RESULT);
}

bool PipelineLibraryTestInstance::verifyDepthImage (const ConstPixelBufferAccess& pba)
{
	tcu::TestLog&		log				= m_context.getTestContext().getLog();
	const VkFormat		compareFormat	= VK_FORMAT_R8_UNORM;
	TextureLevel		referenceImage	(mapVkFormat(compareFormat), pba.getWidth(), pba.getHeight());
	PixelBufferAccess	reference		(referenceImage);
	TextureLevel		resultImage		(mapVkFormat(compareFormat), pba.getWidth(), pba.getHeight());
	PixelBufferAccess	result			(resultImage);
	const int			horzEdge		= 3 * reference.getWidth() / 4;
	const int			diagonalEdge	= (reference.getWidth() + reference.getHeight()) / 2 - 1;
	const UVec4			red100			= ivec2uvec(RGBA::red().toIVec());
	const UVec4			red025			= UVec4(red100[0] / 4, red100[1] / 4, red100[2] / 4, red100[3]);
	const UVec4			red075			= UVec4(3 * red100[0] / 4, 3 * red100[1] / 4, 3 * red100[2] / 4, red100[3]);

	for (int y = 0; y < result.getHeight(); ++y)
		for (int x = 0; x < result.getWidth(); ++x)
		{
			const UVec4 pix(static_cast<deUint32>(static_cast<float>(red100[0]) * pba.getPixDepth(x, y)), 0, 0, 0);

			result.setPixel(pix, x, y);
		}

	for (int y = 0; y < reference.getHeight(); ++y)
	{
		for (int x = 0; x < reference.getWidth(); ++x)
		{
			if (x < horzEdge)
			{
				if (x + y < diagonalEdge)
					reference.setPixel(red025, x, y);
				else
					reference.setPixel(red075, x, y);
			}
			else
				reference.setPixel(red100, x, y);
		}
	}

	return intThresholdCompare(log, "depthImage", "depthImage", reference, result, UVec4(), COMPARE_LOG_RESULT);
}

tcu::TestStatus PipelineLibraryTestInstance::iterate (void)
{
	VkGraphicsPipelineLibraryFlagBitsEXT	graphicsPipelineLibraryFlags[]		=
	{
		VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT,
		VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT,
		VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT,
		VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT,
	};
	const auto								graphicsPipelineLibraryFlagsBegin	= graphicsPipelineLibraryFlags;
	const auto								graphicsPipelineLibraryFlagsEnd		= graphicsPipelineLibraryFlags + DE_LENGTH_OF_ARRAY(graphicsPipelineLibraryFlags);
	deUint32								permutationId						= 0;
	std::set<deUint32>						was;
	bool									result								= true;

	do
	{
		RuntimePipelineTreeConfiguration	runtimePipelineTreeConfiguration	(m_data.pipelineTreeConfiguration.size());
		size_t								subsetNdxStart						= 0;
		deUint32							uniqueTreeSubsetCode				= 0;

		for (size_t nodeNdx = 0; nodeNdx < runtimePipelineTreeConfiguration.size(); ++nodeNdx)
		{
			const deUint32				shaderCount	= m_data.pipelineTreeConfiguration[nodeNdx].shaderCount;
			RuntimePipelineTreeNode&	node		= runtimePipelineTreeConfiguration[nodeNdx];

			node.parentIndex					= m_data.pipelineTreeConfiguration[nodeNdx].parentIndex;
			node.graphicsPipelineLibraryFlags	= 0u;

			for (size_t subsetNdx = 0; subsetNdx < shaderCount; ++subsetNdx)
				node.graphicsPipelineLibraryFlags |= static_cast<VkGraphicsPipelineLibraryFlagsEXT>(graphicsPipelineLibraryFlags[subsetNdxStart + subsetNdx]);

			if (node.parentIndex > 0)
				runtimePipelineTreeConfiguration[node.parentIndex].subtreeGraphicsPipelineLibraryFlags |= node.graphicsPipelineLibraryFlags;

			// Each shader subset should be tested in each node of tree
			subsetNdxStart += shaderCount;

			uniqueTreeSubsetCode = (uniqueTreeSubsetCode << 4) | node.graphicsPipelineLibraryFlags;
		}

		// Check whether this configuration has been tried
		if (was.find(uniqueTreeSubsetCode) == was.end())
			was.insert(uniqueTreeSubsetCode);
		else
			continue;

		result = result && runTest(runtimePipelineTreeConfiguration, m_data.optimize, m_data.delayedShaderCreate);

		if (!result)
		{
			tcu::TestLog&		log = m_context.getTestContext().getLog();
			std::ostringstream	ess;

			for (size_t nodeNdx = 0; nodeNdx < runtimePipelineTreeConfiguration.size(); ++nodeNdx)
			{
				const RuntimePipelineTreeNode&	node	= runtimePipelineTreeConfiguration[nodeNdx];

				ess << node.parentIndex << " {";

				for (size_t subsetNdx = 0; subsetNdx < DE_LENGTH_OF_ARRAY(graphicsPipelineLibraryFlags); ++subsetNdx)
				{
					if ((node.graphicsPipelineLibraryFlags & graphicsPipelineLibraryFlags[subsetNdx]) == 0)
						continue;

					ess << getGraphicsPipelineLibraryFlagsString(graphicsPipelineLibraryFlags[subsetNdx]) << " ";
				}

				ess  << "}" << std::endl;
			}

			log << tcu::TestLog::Message << ess.str() << tcu::TestLog::EndMessage;

			return tcu::TestStatus::fail("At permutation " + de::toString(permutationId));
		}

		++permutationId;
	} while (std::next_permutation(graphicsPipelineLibraryFlagsBegin, graphicsPipelineLibraryFlagsEnd));

	return tcu::TestStatus::pass("OK");
}


class PipelineLibraryTestCase : public TestCase
{
	public:
							PipelineLibraryTestCase		(tcu::TestContext& context, const char* name, const char* desc, const TestParams data);
							~PipelineLibraryTestCase	(void);

	virtual void			checkSupport				(Context& context) const;
	virtual	void			initPrograms				(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance				(Context& context) const;
private:
	TestParams				m_data;
};

PipelineLibraryTestCase::PipelineLibraryTestCase (tcu::TestContext& context, const char* name, const char* desc, const TestParams data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

PipelineLibraryTestCase::~PipelineLibraryTestCase (void)
{
}

void PipelineLibraryTestCase::checkSupport (Context& context) const
{
	if (m_data.delayedShaderCreate || (m_data.pipelineTreeConfiguration.size() > 1))
	{
		context.requireDeviceFunctionality("VK_EXT_graphics_pipeline_library");

		const VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT& graphicsPipelineLibraryFeaturesEXT	= context.getGraphicsPipelineLibraryFeaturesEXT();

		if (!graphicsPipelineLibraryFeaturesEXT.graphicsPipelineLibrary)
			TCU_THROW(NotSupportedError, "graphicsPipelineLibraryFeaturesEXT.graphicsPipelineLibrary required");
	}
}

void PipelineLibraryTestCase::initPrograms (SourceCollections& programCollection) const
{
	std::string	vert	=
		"#version 450\n"
		"layout(location = 0) in vec4 in_position;"
		"layout(set = 0, binding = 0) uniform buf\n"
		"{\n"
		"  vec4 z_coord;\n"
		"};\n"
		"\n"
		"out gl_PerVertex\n"
		"{\n"
		"  vec4 gl_Position;\n"
		"};\n"
		"\n"
		"void main()\n"
		"{\n"
		"  const float z = gl_VertexIndex < 3 ? z_coord.x : z_coord.y;\n"
		"  gl_Position = vec4(in_position.x, in_position.y, z, 1.0f);\n"
		"}\n";

	programCollection.glslSources.add("vert") << glu::VertexSource(vert);

	std::string	frag	=
		"#version 450\n"
		"layout(location = 0) out highp vec4 o_color;\n"
		"layout(set = 1, binding = 1) uniform buf\n"
		"{\n"
		"  vec4 colorTop;\n"
		"  vec4 colorBot;\n"
		"};\n"
		"\n"
		"void main()\n"
		"{\n"
		"  const int middle = " + de::toString(RENDER_SIZE_HEIGHT / 2) + ";\n"
		"  o_color          = int(gl_FragCoord.y - 0.5f) < middle ? colorTop : colorBot;\n"
		"}\n";

	programCollection.glslSources.add("frag") << glu::FragmentSource(frag);
}

TestInstance* PipelineLibraryTestCase::createInstance (Context& context) const
{
	return new PipelineLibraryTestInstance(context, m_data);
}

}	// anonymous

void addPipelineLibraryConfigurationsTests (tcu::TestCaseGroup* group, bool optimize)
{
	const int						R							= -1;
	const PipelineTreeConfiguration	pipelineTreeConfiguration[]	=
	{
		{ {
			{ R, 4 },									/*     4     */
		} },

		{ {
			{ R, 0 },									/*     0     */
														/*  / / \ \  */
			{ 0, 1 }, { 0, 1 }, { 0, 1 }, { 0, 1 }		/*  1 1 1 1  */
		} },

		{ {
			{ R, 0 },									/*     0     */
														/*  / / \    */
			{ 0, 1 }, { 0, 1 }, { 0, 2 }				/*  1 1  2   */
		} },

		{ {
			{ R, 0 },									/*     0     */
														/*  / / \    */
			{ 0, 1 }, { 0, 2 }, { 0, 1 }				/* 1 2   1   */
		} },

		{ {
			{ R, 0 },									/*     0     */
														/*    / \    */
			{ 0, 2 }, { 0, 2 },							/*   2   2   */
		} },

		{ {
			{ R, 1 },									/*     1     */
														/*    / \    */
			{ 0, 2 }, { 0, 1 },							/*   2   1   */
		} },

		{ {
			{ R, 2 },									/*     2     */
														/*    / \    */
			{ 0, 1 }, { 0, 1 },							/*   1   1   */
		} },

		{ {
			{ R, 3 },									/*     3     */
														/*    /      */
			{ 0, 1 },									/*   1       */
		} },

		{ {
			{ R, 1 },									/*     1     */
														/*    /      */
			{ 0, 3 },									/*   3       */
		} },

		{ {
			{ R, 0 },									/*     0     */
														/*    / \    */
			{ 0, 0 },           { 0, 0 },				/*   0   0   */
														/*  / \ / \  */
			{ 1, 1 }, { 1, 1 }, { 2, 1 }, { 2, 1 },		/* 1  1 1  1 */
		} },

		{ {
			{ R, 0 },									/*     0     */
														/*    / \    */
			{ 0, 0 },           { 0, 1 },				/*   0   1   */
														/*  / \   \  */
			{ 1, 1 }, { 1, 1 }, { 2, 1 },				/* 1   1   1 */
		} },

		{ {
			{ R, 1 },									/*     1     */
														/*    / \    */
			{ 0, 0 },           { 0, 1 },				/*   0   1   */
														/*  / \      */
			{ 1, 1 }, { 1, 1 },							/* 1   1     */
		} },

		{ {
			{ R, 1 },									/*     1     */
														/*    /      */
			{ 0, 1 },									/*   1       */
														/*  / \      */
			{ 1, 1 }, { 1, 1 },							/* 1   1     */
		} },

		{ {
			{ R, 1 },									/*        1  */
														/*       /   */
			{ 0, 1 },									/*      1    */
														/*     /     */
			{ 1, 1 },									/*    1      */
														/*   /       */
			{ 2, 1 },									/*  1        */
		} },

		{ {
			{ R, 0 },									/*         0 */
														/*        /  */
			{ 0, 1 },									/*       1   */
														/*      /    */
			{ 1, 1 },									/*     1     */
														/*    /      */
			{ 2, 1 },									/*   1       */
														/*  /        */
			{ 3, 1 },									/* 1         */
		} },
	};

	for (size_t libConfigNdx = 0; libConfigNdx < DE_LENGTH_OF_ARRAY(pipelineTreeConfiguration); ++libConfigNdx)
	{
		const bool			delayedShaderCreate	= (libConfigNdx != 0);
		const TestParams	testParams			=
		{
			pipelineTreeConfiguration[libConfigNdx],	//  PipelineTreeConfiguration	pipelineTreeConfiguration;
			optimize,									//  bool						optimize;
			delayedShaderCreate							//  bool						delayedShaderCreate;
		};
		const std::string	testName			= getTestName(pipelineTreeConfiguration[libConfigNdx]);

		if (optimize && testParams.pipelineTreeConfiguration.size() == 1)
			continue;

		group->addChild(new PipelineLibraryTestCase(group->getTestContext(), testName.c_str(), "", testParams));
	}
}

tcu::TestCaseGroup*	createPipelineLibraryTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group	(new tcu::TestCaseGroup(testCtx, "graphics_library", "Tests verifying graphics pipeline libraries"));

	addTestGroup(group.get(), "fast", "Tests graphics pipeline libraries linkage without optimization", addPipelineLibraryConfigurationsTests, false);
	addTestGroup(group.get(), "optimize", "Tests graphics pipeline libraries linkage with optimization", addPipelineLibraryConfigurationsTests, true);

	return group.release();
}

}	// pipeline

}	// vkt
