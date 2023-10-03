/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRayTracingUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "tcuCommandLine.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "tcuRGBA.hpp"

#include "../draw/vktDrawCreateInfoUtil.hpp"
#include "deMath.h"
#include "deRandom.hpp"
#include "deClock.h"

#include <vector>
#include <chrono>
#include <set>
#include <limits>

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
	bool						useMaintenance5;
};

struct RuntimePipelineTreeNode
{
	deInt32												parentIndex;
	VkGraphicsPipelineLibraryFlagsEXT					graphicsPipelineLibraryFlags;
	VkGraphicsPipelineLibraryFlagsEXT					subtreeGraphicsPipelineLibraryFlags;
	Move<VkPipeline>									pipeline;
	std::vector<VkPipeline>								pipelineLibraries;
	// We need to track the linked libraries too, included in VkPipelineLibraryCreateInfoKHR->pLibraries
	std::vector<VkGraphicsPipelineLibraryFlagsEXT>		linkedLibraryFlags;
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
								 GraphicsPipelineCreateInfo&	graphicsPipelineCreateInfo,
								 VkPrimitiveTopology			topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
								 deUint32						vertexDescriptionCount = 1u)
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

	const VkPipelineVertexInputStateCreateInfo		vertexInputStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		0u,																// VkPipelineVertexInputStateCreateFlags	flags;
		vertexDescriptionCount,											// deUint32									vertexBindingDescriptionCount;
		&graphicsPipelineCreateInfo.m_vertexInputBindingDescription,	// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
		vertexDescriptionCount,											// deUint32									vertexAttributeDescriptionCount;
		&graphicsPipelineCreateInfo.m_vertexInputAttributeDescription,	// const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
	};
	const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		0u,																// VkPipelineInputAssemblyStateCreateFlags	flags;
		topology,														// VkPrimitiveTopology						topology;
		VK_FALSE,														// VkBool32									primitiveRestartEnable;
	};

	graphicsPipelineCreateInfo.addState(vertexInputStateCreateInfo);
	graphicsPipelineCreateInfo.addState(inputAssemblyStateCreateInfo);
}

void updatePreRasterization (Context&						context,
							 GraphicsPipelineCreateInfo&	graphicsPipelineCreateInfo,
							 bool							delayedShaderCreate,
							 VkPolygonMode					polygonMode = VK_POLYGON_MODE_FILL,
							 const VkSpecializationInfo*	specializationInfo = DE_NULL)
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
		specializationInfo										// const VkSpecializationInfo*		pSpecializationInfo;
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
		polygonMode,													// VkPolygonMode							polygonMode;
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
							  bool							delayedShaderCreate,
							  bool							enableDepth = true,
							  const VkSpecializationInfo*	specializationInfo = DE_NULL)
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
		specializationInfo										// const VkSpecializationInfo*		pSpecializationInfo;
	};

	shaderBinary.setUsed();

	// Within the VkPipelineLayout, all bindings that affect the fragment shader stage

	const VkPipelineDepthStencilStateCreateInfo		pipelineDepthStencilStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, //  VkStructureType							sType;
		DE_NULL,													//  const void*								pNext;
		0u,															//  VkPipelineDepthStencilStateCreateFlags	flags;
		enableDepth,												//  VkBool32								depthTestEnable;
		enableDepth,												//  VkBool32								depthWriteEnable;
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
									GraphicsPipelineCreateInfo& graphicsPipelineCreateInfo,
									VkColorComponentFlags		colorWriteMask = COLOR_COMPONENTS_NO_RED)
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
		colorWriteMask,					// VkColorComponentFlags	colorWriteMask;
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

VkFormat getSupportedDepthFormat(const InstanceInterface &vk, const VkPhysicalDevice physicalDevice)
{
	VkFormatProperties properties;

	const VkFormat DepthFormats[] =
	{
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_X8_D24_UNORM_PACK32,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT
	};

	for (const auto format: DepthFormats)
	{
		vk.getPhysicalDeviceFormatProperties(physicalDevice, format, &properties);

		if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
			return format;
	}

	TCU_THROW(NotSupportedError, "Depth format is not supported");
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
	const VkFormat							depthFormat				= getSupportedDepthFormat(m_context.getInstanceInterface(), m_context.getPhysicalDevice());
	const VkGraphicsPipelineLibraryFlagsEXT	vertPipelineFlags		= static_cast<VkGraphicsPipelineLibraryFlagsEXT>(VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT);
	const VkGraphicsPipelineLibraryFlagsEXT	fragPipelineFlags		= static_cast<VkGraphicsPipelineLibraryFlagsEXT>(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT);
	const VkGraphicsPipelineLibraryFlagsEXT	samePipelineFlags		= vertPipelineFlags | fragPipelineFlags;
	const deInt32							nodeNdxLast				= static_cast<deInt32>(runtimePipelineTreeConfiguration.size()) - 1;
	const Move<VkRenderPass>				renderPass				= makeRenderPass(vk, device, colorFormat, depthFormat);
	const de::MovePtr<BufferWithMemory>		zCoordBuffer			= makeZCoordBuffer();
	const de::MovePtr<BufferWithMemory>		paletteBuffer			= makePaletteBuffer();
	const Move<VkDescriptorPool>			descriptorPool			= createDescriptorPool();

	const Move<VkDescriptorSetLayout>		descriptorSetLayoutVert	= createDescriptorSetLayout(**zCoordBuffer, DE_NULL);
	const Move<VkDescriptorSetLayout>		descriptorSetLayoutFrag	= createDescriptorSetLayout(DE_NULL, **paletteBuffer);
	const Move<VkDescriptorSetLayout>		descriptorSetLayoutBoth	= createDescriptorSetLayout(**zCoordBuffer, **paletteBuffer);
	const Move<VkDescriptorSet>				descriptorSetVert		= createDescriptorSet(*descriptorPool, *descriptorSetLayoutVert, **zCoordBuffer, DE_NULL);
	const Move<VkDescriptorSet>				descriptorSetFrag		= createDescriptorSet(*descriptorPool, *descriptorSetLayoutFrag, DE_NULL , **paletteBuffer);

	VkDescriptorSet vecDescriptorSetBoth[2] = { *descriptorSetVert, *descriptorSetFrag };

	VkDescriptorSetLayout vecLayoutVert[2] = { *descriptorSetLayoutVert, DE_NULL };
	VkDescriptorSetLayout vecLayoutFrag[2] = { DE_NULL, *descriptorSetLayoutFrag };
	VkDescriptorSetLayout vecLayoutBoth[2] = { *descriptorSetLayoutVert, *descriptorSetLayoutFrag };

	VkPipelineLayoutCreateFlags pipelineLayoutCreateFlag = 0u;
	if (!m_data.useMaintenance5 && (m_data.delayedShaderCreate || (m_data.pipelineTreeConfiguration.size() > 1)))
		pipelineLayoutCreateFlag = VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT;

	const Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	const Move<VkCommandBuffer>				cmdBuffer				= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const Move<VkPipelineLayout>			pipelineLayoutSame		= makePipelineLayout(vk, device, 2, vecLayoutBoth, pipelineLayoutCreateFlag);
	Move<VkPipelineLayout>					pipelineLayoutVert;
	Move<VkPipelineLayout>					pipelineLayoutFrag;
	Move<VkPipeline>						rootPipeline;

	// Go through tree nodes and create library for each up to root
	for (deInt32 nodeNdx = nodeNdxLast; nodeNdx >= 0 ; --nodeNdx)	// We expect only backward node reference, thus build pipielines from end is safe
	{
		RuntimePipelineTreeNode&				node								= runtimePipelineTreeConfiguration[nodeNdx];
		const bool								buildLibrary						= (nodeNdx != 0);
		const VkPipelineCreateFlags				pipelineCreateFlags					= calcPipelineCreateFlags(optimize, buildLibrary);
		const VkGraphicsPipelineLibraryFlagsEXT	subtreeGraphicsPipelineLibraryFlags	= node.subtreeGraphicsPipelineLibraryFlags | node.graphicsPipelineLibraryFlags;
		const bool								samePipelineLayout					= samePipelineFlags == (samePipelineFlags & subtreeGraphicsPipelineLibraryFlags);
		const bool								vertPipelineLayout					= vertPipelineFlags == (vertPipelineFlags & subtreeGraphicsPipelineLibraryFlags);
		const bool								fragPipelineLayout					= fragPipelineFlags == (fragPipelineFlags & subtreeGraphicsPipelineLibraryFlags);

		if (samePipelineLayout)
			; // pipelineLayoutSame is always built before.
		else if (vertPipelineLayout)
		{
			if (!pipelineLayoutVert)
				pipelineLayoutVert = makePipelineLayout(vk, device, 2, vecLayoutVert, pipelineLayoutCreateFlag);
		}
		else if (fragPipelineLayout)
		{
			if (!pipelineLayoutFrag)
				pipelineLayoutFrag = makePipelineLayout(vk, device, 2, vecLayoutFrag, pipelineLayoutCreateFlag);
		}

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

		VkGraphicsPipelineLibraryFlagsEXT linkedLibrariesFlags = 0;

		for (auto flag : node.linkedLibraryFlags)
			linkedLibrariesFlags |= flag;

		// When pLibraries have any pipeline library with fragment shader state and current pipeline we try to create doesn't,
		// we need to set a MS info.
		if ((linkedLibrariesFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT) &&
				!(node.graphicsPipelineLibraryFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT) &&
				(graphicsPipelineCreateInfo.pMultisampleState == DE_NULL))
		{
			const VkPipelineMultisampleStateCreateInfo	pipelineMultisampleStateCreateInfo = makePipelineMultisampleStateCreateInfo();

			graphicsPipelineCreateInfo.addState(pipelineMultisampleStateCreateInfo);
		}


		if (!m_data.useMaintenance5 && linkedLibrariesFlags != ALL_GRAPHICS_PIPELINE_LIBRARY_FLAGS && graphicsPipelineLibraryCreateInfo.flags != 0)
			appendStructurePtrToVulkanChain(&graphicsPipelineCreateInfo.pNext, &graphicsPipelineLibraryCreateInfo);

		if (linkingInfo.libraryCount != 0)
		{
			appendStructurePtrToVulkanChain(&graphicsPipelineCreateInfo.pNext, &linkingInfo);
			graphicsPipelineCreateInfo.layout = *pipelineLayoutSame;
		}

		linkedLibrariesFlags |= node.graphicsPipelineLibraryFlags;

		// if current pipeline that we try to create and pLibraries have all states of pipelines, we are not allowed to create a pipeline library.
		if (linkedLibrariesFlags == ALL_GRAPHICS_PIPELINE_LIBRARY_FLAGS)
		{
			DE_ASSERT(!buildLibrary);
			graphicsPipelineCreateInfo.flags &= ~VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
		}

		node.pipeline = createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineCreateInfo);

		if (buildLibrary)
		{
			DE_ASSERT(de::inBounds(node.parentIndex, 0, static_cast<deInt32>(runtimePipelineTreeConfiguration.size())));

			runtimePipelineTreeConfiguration[node.parentIndex].pipelineLibraries.push_back(*node.pipeline);
			runtimePipelineTreeConfiguration[node.parentIndex].linkedLibraryFlags.push_back(linkedLibrariesFlags);
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

		vk::beginCommandBuffer(vk, *cmdBuffer, 0u);
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
		vk::endCommandBuffer(vk, *cmdBuffer);
		vk::submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), cmdBuffer.get());

		vk::invalidateAlloc(vk, device, colorBuffer.getAllocation());
		vk::invalidateAlloc(vk, device, depthBuffer.getAllocation());

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
	if (m_data.useMaintenance5)
	{
		context.requireDeviceFunctionality("VK_KHR_maintenance5");
		return;
	}

	context.requireDeviceFunctionality("VK_KHR_pipeline_library");

	if (m_data.delayedShaderCreate || (m_data.pipelineTreeConfiguration.size() > 1))
	{
		context.requireDeviceFunctionality("VK_EXT_graphics_pipeline_library");

		const VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT& graphicsPipelineLibraryFeaturesEXT = context.getGraphicsPipelineLibraryFeaturesEXT();

		if (!graphicsPipelineLibraryFeaturesEXT.graphicsPipelineLibrary)
			TCU_THROW(NotSupportedError, "graphicsPipelineLibraryFeaturesEXT.graphicsPipelineLibrary required");
	}
}

void PipelineLibraryTestCase::initPrograms (SourceCollections& programCollection) const
{
	std::string	vert	=
		"#version 450\n"
		"layout(location = 0) in vec4 in_position;\n"
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

enum class MiscTestMode
{
	INDEPENDENT_PIPELINE_LAYOUT_SETS_FAST_LINKED = 0,
	INDEPENDENT_PIPELINE_LAYOUT_SETS_WITH_LINK_TIME_OPTIMIZATION_UNION_HANDLE,
	BIND_NULL_DESCRIPTOR_SET,
	BIND_NULL_DESCRIPTOR_SET_IN_MONOLITHIC_PIPELINE,
	COMPARE_LINK_TIMES,
	SHADER_MODULE_CREATE_INFO_COMP,
	SHADER_MODULE_CREATE_INFO_RT,
	SHADER_MODULE_CREATE_INFO_RT_LIB,
	NULL_RENDERING_CREATE_INFO,
};

struct MiscTestParams
{
	MiscTestMode	mode;

	// attributes used in BIND_NULL_DESCRIPTOR_SET mode
	deUint32		layoutsCount;
	deUint32		layoutsBits;
};

class PipelineLibraryMiscTestInstance : public TestInstance
{
public:
						PipelineLibraryMiscTestInstance		(Context&					context,
															 const MiscTestParams&		params);
						~PipelineLibraryMiscTestInstance	(void) = default;
	tcu::TestStatus		iterate								(void);

protected:

	tcu::TestStatus		runNullDescriptorSet					(void);
	tcu::TestStatus		runNullDescriptorSetInMonolithicPipeline(void);
	tcu::TestStatus		runIndependentPipelineLayoutSets		(bool useLinkTimeOptimization = false);
	tcu::TestStatus		runCompareLinkTimes						(void);

	struct VerificationData
	{
		const tcu::IVec2	point;
		const tcu::IVec4	color;
	};
	tcu::TestStatus		verifyResult						(const std::vector<VerificationData>&	verificationData,
															 const tcu::ConstPixelBufferAccess&		colorPixelAccess) const;

private:
	MiscTestParams					m_testParams;
	const VkFormat					m_colorFormat;
	const Vec4						m_colorClearColor;
	const VkRect2D					m_renderArea;

	de::MovePtr<ImageWithMemory>	m_colorImage;
	Move<VkImageView>				m_colorImageView;

	Move<VkRenderPass>				m_renderPass;
	Move<VkFramebuffer>				m_framebuffer;

	Move<VkCommandPool>				m_cmdPool;
	Move<VkCommandBuffer>			m_cmdBuffer;
};

PipelineLibraryMiscTestInstance::PipelineLibraryMiscTestInstance(Context& context, const MiscTestParams& params)
	: vkt::TestInstance		(context)
	, m_testParams			(params)
	, m_colorFormat			(VK_FORMAT_R8G8B8A8_UNORM)
	, m_colorClearColor		(0.0f, 0.0f, 0.0f, 1.0f)
	, m_renderArea			(makeRect2D(RENDER_SIZE_WIDTH, RENDER_SIZE_HEIGHT))
{
}

tcu::TestStatus PipelineLibraryMiscTestInstance::iterate (void)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();
	Allocator&				allocator	= m_context.getDefaultAllocator();

	// create image and image view that will hold rendered frame
	const VkImageCreateInfo colorImageCreateInfo = makeColorImageCreateInfo(m_colorFormat, m_renderArea.extent.width, m_renderArea.extent.height);
	m_colorImage = de::MovePtr<ImageWithMemory>(new ImageWithMemory(vk, device, allocator, colorImageCreateInfo, MemoryRequirement::Any));
	const VkImageViewCreateInfo		colorImageViewCreateInfo	= makeImageViewCreateInfo(**m_colorImage, m_colorFormat, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT));
	const Move<VkImageView>			colorImageView				= createImageView(vk, device, &colorImageViewCreateInfo);

	// create renderpass and framebuffer
	m_renderPass = makeRenderPass(vk, device, m_colorFormat);
	const VkFramebufferCreateInfo framebufferCreateInfo = makeFramebufferCreateInfo(*m_renderPass, 1u, &*colorImageView, m_renderArea.extent.width, m_renderArea.extent.height);
	m_framebuffer = createFramebuffer(vk, device, &framebufferCreateInfo);

	// create command pool and command buffer
	const deUint32 queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
	m_cmdPool	= createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	m_cmdBuffer	= allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	// run selected test
	if (m_testParams.mode == MiscTestMode::BIND_NULL_DESCRIPTOR_SET)
		return runNullDescriptorSet();
	else if (m_testParams.mode == MiscTestMode::BIND_NULL_DESCRIPTOR_SET_IN_MONOLITHIC_PIPELINE)
		return runNullDescriptorSetInMonolithicPipeline();
	else if (m_testParams.mode == MiscTestMode::INDEPENDENT_PIPELINE_LAYOUT_SETS_FAST_LINKED)
		return runIndependentPipelineLayoutSets();
	else if (m_testParams.mode == MiscTestMode::INDEPENDENT_PIPELINE_LAYOUT_SETS_WITH_LINK_TIME_OPTIMIZATION_UNION_HANDLE)
		return runIndependentPipelineLayoutSets(true);
	else if (m_testParams.mode == MiscTestMode::COMPARE_LINK_TIMES)
		return runCompareLinkTimes();

	DE_ASSERT(DE_FALSE);
	return tcu::TestStatus::fail("Fail");
}

tcu::TestStatus PipelineLibraryMiscTestInstance::runNullDescriptorSet(void)
{
	const DeviceInterface&			vk						= m_context.getDeviceInterface();
	const VkDevice					device					= m_context.getDevice();
	Allocator&						allocator				= m_context.getDefaultAllocator();

	const VkDeviceSize				colorBufferDataSize		= static_cast<VkDeviceSize>(m_renderArea.extent.width * m_renderArea.extent.height * tcu::getPixelSize(mapVkFormat(m_colorFormat)));
	const VkBufferCreateInfo		colorBufferCreateInfo	= makeBufferCreateInfo(colorBufferDataSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const BufferWithMemory			colorBuffer				(vk, device, allocator, colorBufferCreateInfo, MemoryRequirement::HostVisible);

	VkDeviceSize					uniformBufferDataSize	= sizeof(tcu::Vec4);
	const VkBufferCreateInfo		uniformBufferCreateInfo	= makeBufferCreateInfo(uniformBufferDataSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	de::MovePtr<BufferWithMemory>	uniformBuffer[2];

	// setup data in uniform buffers that will give us expected result for validation
	const tcu::Vec4 uniformBuffData[]
	{
		{ -1.00f,  1.00f,  2.0f, -2.00f },
		{  0.00f,  0.20f,  0.6f,  0.75f },
	};

	for (deUint32 i = 0; i < 2; ++i)
	{
		uniformBuffer[i] = de::MovePtr<BufferWithMemory>(new BufferWithMemory(vk, device, allocator, uniformBufferCreateInfo, MemoryRequirement::HostVisible));
		deMemcpy(uniformBuffer[i]->getAllocation().getHostPtr(), uniformBuffData[i].getPtr(), (size_t)uniformBufferDataSize);
		flushAlloc(vk, device, uniformBuffer[i]->getAllocation());
	}

	const deUint32 maxBitsCount = 8 * sizeof(m_testParams.layoutsBits);
	VkDescriptorSetLayout vertDescriptorSetLayouts[maxBitsCount];
	VkDescriptorSetLayout fragDescriptorSetLayouts[maxBitsCount];
	VkDescriptorSetLayout allDescriptorSetLayouts[maxBitsCount];

	// set all layouts to NULL
	deMemset(&vertDescriptorSetLayouts, DE_NULL, maxBitsCount * sizeof(VkDescriptorSetLayout));
	deMemset(&fragDescriptorSetLayouts, DE_NULL, maxBitsCount * sizeof(VkDescriptorSetLayout));

	// create used descriptor set layouts
	Move<VkDescriptorSetLayout> usedDescriptorSetLayouts[]
	{
		DescriptorSetLayoutBuilder()
			.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
			.build(vk, device),
		DescriptorSetLayoutBuilder()
			.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build(vk, device)
	};

	// create descriptor set layouts that are not used by shaders in test - finalPipelineLayout,
	// needs to always be the complete pipeline layout with no holes; we can put NULLs in
	// DescriptorSetLayouts used by partial pipelines (vertDescriptorSetLayouts and fragDescriptorSetLayouts)
	Move<VkDescriptorSetLayout> unusedDescriptorSetLayouts[maxBitsCount];
	for (deUint32 i = 0u; i < m_testParams.layoutsCount; ++i)
	{
		unusedDescriptorSetLayouts[i] = DescriptorSetLayoutBuilder().build(vk, device);

		// by default allDescriptorSetLayouts is filled with unused layouts but later
		// if test requires this proper indexes are replaced with used layouts
		allDescriptorSetLayouts[i] = *unusedDescriptorSetLayouts[i];
	}

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = initVulkanStructure();
	pipelineLayoutCreateInfo.flags = VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT;

	// find set bits
	std::vector<deUint32> bitsThatAreSet;
	for (deUint32 i = 0u; i < m_testParams.layoutsCount; ++i)
	{
		if (m_testParams.layoutsBits & (1 << (maxBitsCount - 1 - i)))
			bitsThatAreSet.push_back(i);
	}

	deUint32 usedDescriptorSets = static_cast<deUint32>(bitsThatAreSet.size());
	DE_ASSERT(usedDescriptorSets && (usedDescriptorSets < 3u));

	deUint32 vertSetIndex						= bitsThatAreSet[0];
	deUint32 fragSetIndex						= 0u;
	vertDescriptorSetLayouts[vertSetIndex]		= *usedDescriptorSetLayouts[0];
	allDescriptorSetLayouts[vertSetIndex]		= *usedDescriptorSetLayouts[0];
	pipelineLayoutCreateInfo.setLayoutCount		= vertSetIndex + 1u;
	pipelineLayoutCreateInfo.pSetLayouts		= vertDescriptorSetLayouts;

	Move<VkPipelineLayout>	vertPipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
	Move<VkPipelineLayout>	fragPipelineLayout;

	if (usedDescriptorSets == 2u)
	{
		fragSetIndex							= bitsThatAreSet[1];
		fragDescriptorSetLayouts[fragSetIndex]	= *usedDescriptorSetLayouts[1];
		allDescriptorSetLayouts[fragSetIndex]	= *usedDescriptorSetLayouts[1];
		pipelineLayoutCreateInfo.setLayoutCount	= fragSetIndex + 1u;
		pipelineLayoutCreateInfo.pSetLayouts	= fragDescriptorSetLayouts;

		fragPipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
	}
	else
	{
		pipelineLayoutCreateInfo.setLayoutCount = 0u;
		pipelineLayoutCreateInfo.pSetLayouts	= DE_NULL;
		fragPipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
	}

	// create descriptor pool
	Move<VkDescriptorPool> descriptorPool = DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, usedDescriptorSets)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, usedDescriptorSets);

	const VkDescriptorBufferInfo	vertShaderBufferInfo	= makeDescriptorBufferInfo(**uniformBuffer[0], 0u, uniformBufferDataSize);
	Move<VkDescriptorSet>			vertDescriptorSet		= makeDescriptorSet(vk, device, *descriptorPool, *usedDescriptorSetLayouts[0]);
	Move<VkDescriptorSet>			fragDescriptorSet;

	if (usedDescriptorSets == 1u)
	{
		// update single descriptors with actual buffer
		DescriptorSetUpdateBuilder()
			.writeSingle(*vertDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &vertShaderBufferInfo)
			.update(vk, device);
	}
	else
	{
		const VkDescriptorBufferInfo fragShaderBufferInfo = makeDescriptorBufferInfo(**uniformBuffer[1], 0u, uniformBufferDataSize);
		fragDescriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *usedDescriptorSetLayouts[1]);

		// update both descriptors with actual buffers
		DescriptorSetUpdateBuilder()
			.writeSingle(*vertDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &vertShaderBufferInfo)
			.writeSingle(*fragDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &fragShaderBufferInfo)
			.update(vk, device);
	}

	pipelineLayoutCreateInfo.setLayoutCount		= m_testParams.layoutsCount;
	pipelineLayoutCreateInfo.pSetLayouts		= allDescriptorSetLayouts;
	Move<VkPipelineLayout> finalPipelineLayout	= createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

	const deUint32 commonPipelinePartFlags = deUint32(VK_PIPELINE_CREATE_LIBRARY_BIT_KHR);
	GraphicsPipelineCreateInfo partialPipelineCreateInfo[]
	{
		{ *vertPipelineLayout,	*m_renderPass, 0, commonPipelinePartFlags },
		{ *fragPipelineLayout,	*m_renderPass, 0, commonPipelinePartFlags },
	};

	// fill proper portion of pipeline state
	updateVertexInputInterface(m_context, partialPipelineCreateInfo[0], VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u);
	updatePreRasterization(m_context, partialPipelineCreateInfo[0], false);
	updatePostRasterization(m_context, partialPipelineCreateInfo[1], false);
	updateFragmentOutputInterface(m_context, partialPipelineCreateInfo[1]);

	Move<VkPipeline> vertPipelinePart;
	Move<VkPipeline> fragPipelinePart;

	// extend pNext chain and create partial pipelines
	{
		VkGraphicsPipelineLibraryCreateInfoEXT libraryCreateInfo = makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT | VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT);
		appendStructurePtrToVulkanChain(&partialPipelineCreateInfo[0].pNext, &libraryCreateInfo);
		vertPipelinePart = createGraphicsPipeline(vk, device, DE_NULL, &partialPipelineCreateInfo[0]);

		libraryCreateInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT | VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT,
		appendStructurePtrToVulkanChain(&partialPipelineCreateInfo[1].pNext, &libraryCreateInfo);
		fragPipelinePart = createGraphicsPipeline(vk, device, DE_NULL, &partialPipelineCreateInfo[1]);
	}

	// create final pipeline out of two parts
	std::vector<VkPipeline>			rawParts			= { *vertPipelinePart, *fragPipelinePart };
	VkPipelineLibraryCreateInfoKHR	linkingInfo			= makePipelineLibraryCreateInfo(rawParts);
	VkGraphicsPipelineCreateInfo	finalPipelineInfo	= initVulkanStructure();

	finalPipelineInfo.layout = *finalPipelineLayout;
	appendStructurePtrToVulkanChain(&finalPipelineInfo.pNext, &linkingInfo);
	Move<VkPipeline> pipeline = createGraphicsPipeline(vk, device, DE_NULL, &finalPipelineInfo);

	vk::beginCommandBuffer(vk, *m_cmdBuffer, 0u);
	{
		// change color image layout
		const VkImageMemoryBarrier initialImageBarrier = makeImageMemoryBarrier(
			0,													// VkAccessFlags					srcAccessMask;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,				// VkAccessFlags					dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout					oldLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout					newLayout;
			**m_colorImage,										// VkImage							image;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }		// VkImageSubresourceRange			subresourceRange;
		);
		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0, 0, DE_NULL, 0, DE_NULL, 1, &initialImageBarrier);

		// wait for uniform buffers
		std::vector<VkBufferMemoryBarrier> initialBufferBarriers(2u, makeBufferMemoryBarrier(
			VK_ACCESS_HOST_WRITE_BIT,							// VkAccessFlags2KHR				srcAccessMask
			VK_ACCESS_TRANSFER_READ_BIT,						// VkAccessFlags2KHR				dstAccessMask
			uniformBuffer[0]->get(),							// VkBuffer							buffer
			0u,													// VkDeviceSize						offset
			uniformBufferDataSize								// VkDeviceSize						size
		));
		initialBufferBarriers[1].buffer = uniformBuffer[1]->get();
		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, DE_NULL, 2, initialBufferBarriers.data(), 0, DE_NULL);

		beginRenderPass(vk, *m_cmdBuffer, *m_renderPass, *m_framebuffer, m_renderArea, m_colorClearColor);

		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

		vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *finalPipelineLayout, vertSetIndex, 1u, &*vertDescriptorSet, 0u, DE_NULL);
		if (usedDescriptorSets == 2u)
			vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *finalPipelineLayout, fragSetIndex, 1u, &*fragDescriptorSet, 0u, DE_NULL);

		vk.cmdDraw(*m_cmdBuffer, 4, 1u, 0u, 0u);

		endRenderPass(vk, *m_cmdBuffer);

		const tcu::IVec2 size { (deInt32)m_renderArea.extent.width, (deInt32)m_renderArea.extent.height };
		copyImageToBuffer(vk, *m_cmdBuffer, **m_colorImage, *colorBuffer, size);
	}
	vk::endCommandBuffer(vk, *m_cmdBuffer);
	vk::submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *m_cmdBuffer);

	vk::invalidateAlloc(vk, device, colorBuffer.getAllocation());
	const tcu::ConstPixelBufferAccess colorPixelAccess(mapVkFormat(m_colorFormat), m_renderArea.extent.width, m_renderArea.extent.height, 1, colorBuffer.getAllocation().getHostPtr());

	// verify result
	deInt32 width	= (deInt32)m_renderArea.extent.width;
	deInt32 height	= (deInt32)m_renderArea.extent.height;
	const std::vector<VerificationData> verificationData
	{
		{ { 1, 1 },						{ 0, 51, 153, 191 } },		// note COLOR_COMPONENTS_NO_RED is used
		{ { width / 2, height / 2 },	{ 0, 51, 153, 191 } },
		{ { width - 2, height - 2 },	{ 0, 0, 0, 255 } }			// clear color
	};
	return verifyResult(verificationData, colorPixelAccess);
}

tcu::TestStatus PipelineLibraryMiscTestInstance::runNullDescriptorSetInMonolithicPipeline()
{
	// VK_NULL_HANDLE can be used for descriptor set layouts when creating a pipeline layout whether independent sets are used or not,
	// as long as graphics pipeline libraries are enabled; VK_NULL_HANDLE is also alowed for a descriptor set under the same conditions
	// when using vkCmdBindDescriptorSets

	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();
	Allocator&				allocator	= m_context.getDefaultAllocator();

	const VkDeviceSize				colorBufferDataSize		= static_cast<VkDeviceSize>(m_renderArea.extent.width * m_renderArea.extent.height * tcu::getPixelSize(mapVkFormat(m_colorFormat)));
	const VkBufferCreateInfo		colorBufferCreateInfo	= makeBufferCreateInfo(colorBufferDataSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const BufferWithMemory			colorBuffer(vk, device, allocator, colorBufferCreateInfo, MemoryRequirement::HostVisible);

	const tcu::Vec4 uniformBuffData	{ 0.0f, 0.20f, 0.6f, 0.75f };
	VkDeviceSize					uniformBufferDataSize = sizeof(tcu::Vec4);
	const VkBufferCreateInfo		uniformBufferCreateInfo = makeBufferCreateInfo(uniformBufferDataSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	de::MovePtr<BufferWithMemory> uniformBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(vk, device, allocator, uniformBufferCreateInfo, MemoryRequirement::HostVisible));
	deMemcpy(uniformBuffer->getAllocation().getHostPtr(), uniformBuffData.getPtr(), (size_t)uniformBufferDataSize);
	flushAlloc(vk, device, uniformBuffer->getAllocation());

	// create descriptor set layouts - first unused, second used
	Move<VkDescriptorSetLayout> descriptorSetLayout
	{
		DescriptorSetLayoutBuilder()
			.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build(vk, device)
	};

	Move<VkDescriptorPool> allDescriptorPool = DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);

	// create descriptor set
	Move<VkDescriptorSet> descriptorSet = makeDescriptorSet(vk, device, *allDescriptorPool, *descriptorSetLayout);

	// update descriptor with actual buffer
	const VkDescriptorBufferInfo shaderBufferInfo = makeDescriptorBufferInfo(**uniformBuffer, 0u, uniformBufferDataSize);
	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &shaderBufferInfo)
		.update(vk, device);

	// create a pipeline layout with its first descriptor set layout as VK_NULL_HANDLE
	// and a second with a valid descriptor set layout containing a buffer
	VkDescriptorSet			rawDescriptorSets[]			= { DE_NULL, *descriptorSet };
	VkDescriptorSetLayout	rawDescriptorSetLayouts[]	= { DE_NULL, *descriptorSetLayout };

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = initVulkanStructure();
	pipelineLayoutCreateInfo.setLayoutCount = 2u;
	pipelineLayoutCreateInfo.pSetLayouts = rawDescriptorSetLayouts;
	Move<VkPipelineLayout> pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

	// create monolithic graphics pipeline
	GraphicsPipelineCreateInfo pipelineCreateInfo(*pipelineLayout, *m_renderPass, 0, 0u);
	updateVertexInputInterface(m_context, pipelineCreateInfo, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u);
	updatePreRasterization(m_context, pipelineCreateInfo, false);
	updatePostRasterization(m_context, pipelineCreateInfo, false);
	updateFragmentOutputInterface(m_context, pipelineCreateInfo);
	Move<VkPipeline> pipeline = createGraphicsPipeline(vk, device, DE_NULL, &pipelineCreateInfo);

	vk::beginCommandBuffer(vk, *m_cmdBuffer, 0u);
	{
		// change color image layout
		const VkImageMemoryBarrier initialImageBarrier = makeImageMemoryBarrier(
			0,													// VkAccessFlags					srcAccessMask;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,				// VkAccessFlags					dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout					oldLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout					newLayout;
			**m_colorImage,										// VkImage							image;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }		// VkImageSubresourceRange			subresourceRange;
		);
		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0, 0, DE_NULL, 0, DE_NULL, 1, &initialImageBarrier);

		// wait for uniform buffer
		const VkBufferMemoryBarrier initialBufferBarrier = makeBufferMemoryBarrier(
			VK_ACCESS_HOST_WRITE_BIT,							// VkAccessFlags2KHR				srcAccessMask
			VK_ACCESS_UNIFORM_READ_BIT,							// VkAccessFlags2KHR				dstAccessMask
			uniformBuffer->get(),								// VkBuffer							buffer
			0u,													// VkDeviceSize						offset
			uniformBufferDataSize								// VkDeviceSize						size
		);
		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, (VkDependencyFlags)0, 0, DE_NULL, 1, &initialBufferBarrier, 0, DE_NULL);

		beginRenderPass(vk, *m_cmdBuffer, *m_renderPass, *m_framebuffer, m_renderArea, m_colorClearColor);

		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 2u, rawDescriptorSets, 0u, DE_NULL);
		vk.cmdDraw(*m_cmdBuffer, 4, 1u, 0u, 0u);

		endRenderPass(vk, *m_cmdBuffer);

		const tcu::IVec2 size{ (deInt32)m_renderArea.extent.width, (deInt32)m_renderArea.extent.height };
		copyImageToBuffer(vk, *m_cmdBuffer, **m_colorImage, *colorBuffer, size);
	}
	vk::endCommandBuffer(vk, *m_cmdBuffer);
	vk::submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *m_cmdBuffer);

	vk::invalidateAlloc(vk, device, colorBuffer.getAllocation());
	const tcu::ConstPixelBufferAccess colorPixelAccess(mapVkFormat(m_colorFormat), m_renderArea.extent.width, m_renderArea.extent.height, 1, colorBuffer.getAllocation().getHostPtr());

	// verify result
	deInt32		width		= (deInt32)m_renderArea.extent.width;
	deInt32		height		= (deInt32)m_renderArea.extent.height;
	tcu::IVec4	outColor
	{
		0,										// r is 0 because COLOR_COMPONENTS_NO_RED is used
		static_cast<int>(uniformBuffData[1] * 255),
		static_cast<int>(uniformBuffData[2] * 255),
		static_cast<int>(uniformBuffData[3] * 255)
	};
	const std::vector<VerificationData> verificationData
	{
		{ { 1, 1 },						outColor },
		{ { width / 2, height / 2 },	outColor },
		{ { width - 2, height - 2 },	{ 0, 0, 0, 255 } }			// clear color
	};

	return verifyResult(verificationData, colorPixelAccess);
}

tcu::TestStatus PipelineLibraryMiscTestInstance::runIndependentPipelineLayoutSets (bool useLinkTimeOptimization)
{
	const DeviceInterface&			vk							= m_context.getDeviceInterface();
	const VkDevice					device						= m_context.getDevice();
	Allocator&						allocator					= m_context.getDefaultAllocator();

	const VkDeviceSize				colorBufferDataSize			= static_cast<VkDeviceSize>(m_renderArea.extent.width * m_renderArea.extent.height * tcu::getPixelSize(mapVkFormat(m_colorFormat)));
	const VkBufferCreateInfo		colorBufferCreateInfo		= makeBufferCreateInfo(colorBufferDataSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const BufferWithMemory			colorBuffer					(vk, device, allocator, colorBufferCreateInfo, MemoryRequirement::HostVisible);

	VkDeviceSize					uniformBufferDataSize		= sizeof(tcu::Vec4);
	const VkBufferCreateInfo		uniformBufferCreateInfo		= makeBufferCreateInfo(uniformBufferDataSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	de::MovePtr<BufferWithMemory>	uniformBuffer[3];

	// setup data in uniform buffers that will give us expected result for validation
	const tcu::Vec4 uniformBuffData[3]
	{
		{  4.00f,  3.00f, -1.0f,  4.00f },
		{  0.10f,  0.25f, -0.5f,  0.05f },
		{ -5.00f, -2.00f,  3.0f, -6.00f },
	};

	for (deUint32 i = 0; i < 3; ++i)
	{
		uniformBuffer[i] = de::MovePtr<BufferWithMemory>(new BufferWithMemory(vk, device, allocator, uniformBufferCreateInfo, MemoryRequirement::HostVisible));
		deMemcpy(uniformBuffer[i]->getAllocation().getHostPtr(), uniformBuffData[i].getPtr(), (size_t)uniformBufferDataSize);
		flushAlloc(vk, device, uniformBuffer[i]->getAllocation());
	}

	// create three descriptor set layouts
	Move<VkDescriptorSetLayout>	descriptorSetLayouts[3];
	descriptorSetLayouts[0] = DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(vk, device);
	descriptorSetLayouts[1] = DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(vk, device);
	descriptorSetLayouts[2] = DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
		.build(vk, device);

	// for the link time opt (and when null handle is used) use total pipeline layout recreated without the INDEPENDENT SETS bit
	deUint32 allLayoutsFlag = deUint32(VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT);
	if (useLinkTimeOptimization)
		allLayoutsFlag = 0u;

	// Pre-rasterization stage library has sets 0, 1, 2
	// * set 0 has descriptors
	// * set 1 has no descriptors
	// * set 2 has descriptors
	// Fragment stage library has sets 0, 1
	// * set 0 has descriptors
	// * set 1 has descriptors
	VkDescriptorSetLayout vertDescriptorSetLayouts[]	= { *descriptorSetLayouts[0], DE_NULL, *descriptorSetLayouts[2] };
	VkDescriptorSetLayout fragDescriptorSetLayouts[]	= { *descriptorSetLayouts[0], *descriptorSetLayouts[1] };
	VkDescriptorSetLayout allDescriptorSetLayouts[]		= { *descriptorSetLayouts[0], *descriptorSetLayouts[1], *descriptorSetLayouts[2] };

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo	= initVulkanStructure();
	pipelineLayoutCreateInfo.flags						= allLayoutsFlag;
	pipelineLayoutCreateInfo.setLayoutCount				= 3u;
	pipelineLayoutCreateInfo.pSetLayouts				= allDescriptorSetLayouts;
	Move<VkPipelineLayout> allLayouts					= createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
	pipelineLayoutCreateInfo.flags						= deUint32(VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT);
	pipelineLayoutCreateInfo.pSetLayouts				= vertDescriptorSetLayouts;
	Move<VkPipelineLayout> vertLayouts					= createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
	pipelineLayoutCreateInfo.setLayoutCount				= 2u;
	pipelineLayoutCreateInfo.pSetLayouts				= fragDescriptorSetLayouts;
	Move<VkPipelineLayout> fragLayouts					= createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

	Move<VkDescriptorPool> allDescriptorPool = DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 3);

	// create three descriptor sets
	Move<VkDescriptorSet>	descriptorSetA			= makeDescriptorSet(vk, device, *allDescriptorPool, *descriptorSetLayouts[0]);
	Move<VkDescriptorSet>	descriptorSetB			= makeDescriptorSet(vk, device, *allDescriptorPool, *descriptorSetLayouts[1]);
	Move<VkDescriptorSet>	descriptorSetC			= makeDescriptorSet(vk, device, *allDescriptorPool, *descriptorSetLayouts[2]);
	VkDescriptorSet			allDescriptorSets[]		= { *descriptorSetA , *descriptorSetB , *descriptorSetC };

	// update descriptors with actual buffers
	const VkDescriptorBufferInfo shaderBufferAInfo = makeDescriptorBufferInfo(**uniformBuffer[0], 0u, uniformBufferDataSize);
	const VkDescriptorBufferInfo shaderBufferBInfo = makeDescriptorBufferInfo(**uniformBuffer[1], 0u, uniformBufferDataSize);
	const VkDescriptorBufferInfo shaderBufferCInfo = makeDescriptorBufferInfo(**uniformBuffer[2], 0u, uniformBufferDataSize);
	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSetA, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &shaderBufferAInfo)
		.writeSingle(*descriptorSetB, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &shaderBufferBInfo)
		.writeSingle(*descriptorSetC, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &shaderBufferCInfo)
		.update(vk, device);

	deUint32 commonPipelinePartFlags	= deUint32(VK_PIPELINE_CREATE_LIBRARY_BIT_KHR);
	deUint32 finalPipelineFlag			= 0u;
	if (useLinkTimeOptimization)
	{
		commonPipelinePartFlags |= deUint32(VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT);
		finalPipelineFlag		 = deUint32(VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT);
	}

	GraphicsPipelineCreateInfo partialPipelineCreateInfo[]
	{
		{ DE_NULL,		*m_renderPass, 0, commonPipelinePartFlags },
		{ *vertLayouts,	*m_renderPass, 0, commonPipelinePartFlags },
		{ *fragLayouts,	*m_renderPass, 0, commonPipelinePartFlags },
		{ DE_NULL,		*m_renderPass, 0, commonPipelinePartFlags }
	};

	// fill proper portion of pipeline state
	updateVertexInputInterface		(m_context, partialPipelineCreateInfo[0], VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u);
	updatePreRasterization			(m_context, partialPipelineCreateInfo[1], false);
	updatePostRasterization			(m_context, partialPipelineCreateInfo[2], false);
	updateFragmentOutputInterface	(m_context, partialPipelineCreateInfo[3]);

	// extend pNext chain and create all partial pipelines
	std::vector<VkPipeline>			rawParts(4u, DE_NULL);
	std::vector<Move<VkPipeline> >	pipelineParts;
	pipelineParts.reserve(4u);
	VkGraphicsPipelineLibraryCreateInfoEXT libraryCreateInfo = makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT);
	for (deUint32 i = 0 ; i < 4u ; ++i)
	{
		libraryCreateInfo.flags = GRAPHICS_PIPELINE_LIBRARY_FLAGS[i];
		appendStructurePtrToVulkanChain(&partialPipelineCreateInfo[i].pNext, &libraryCreateInfo);
		pipelineParts.emplace_back(createGraphicsPipeline(vk, device, DE_NULL, &partialPipelineCreateInfo[i]));
		rawParts[i] = *pipelineParts[i];
	}

	// create final pipeline out of four parts
	VkPipelineLibraryCreateInfoKHR	linkingInfo			= makePipelineLibraryCreateInfo(rawParts);
	VkGraphicsPipelineCreateInfo	finalPipelineInfo	= initVulkanStructure();

	finalPipelineInfo.flags		= finalPipelineFlag;
	finalPipelineInfo.layout	= *allLayouts;

	appendStructurePtrToVulkanChain(&finalPipelineInfo.pNext, &linkingInfo);
	Move<VkPipeline> pipeline = createGraphicsPipeline(vk, device, DE_NULL, &finalPipelineInfo);

	vk::beginCommandBuffer(vk, *m_cmdBuffer, 0u);
	{
		// change color image layout
		const VkImageMemoryBarrier initialImageBarrier = makeImageMemoryBarrier(
			0,													// VkAccessFlags					srcAccessMask;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,				// VkAccessFlags					dstAccessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout					oldLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout					newLayout;
			**m_colorImage,										// VkImage							image;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }		// VkImageSubresourceRange			subresourceRange;
		);
		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0, 0, DE_NULL, 0, DE_NULL, 1, &initialImageBarrier);

		// wait for uniform buffers
		std::vector<VkBufferMemoryBarrier> initialBufferBarriers(3u, makeBufferMemoryBarrier(
			VK_ACCESS_HOST_WRITE_BIT,							// VkAccessFlags2KHR				srcAccessMask
			VK_ACCESS_UNIFORM_READ_BIT,							// VkAccessFlags2KHR				dstAccessMask
			uniformBuffer[0]->get(),							// VkBuffer							buffer
			0u,													// VkDeviceSize						offset
			uniformBufferDataSize								// VkDeviceSize						size
		));
		initialBufferBarriers[1].buffer = uniformBuffer[1]->get();
		initialBufferBarriers[2].buffer = uniformBuffer[2]->get();
		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, (VkDependencyFlags)0, 0, DE_NULL, 3, initialBufferBarriers.data(), 0, DE_NULL);

		beginRenderPass(vk, *m_cmdBuffer, *m_renderPass, *m_framebuffer, m_renderArea, m_colorClearColor);

		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *allLayouts, 0u, 3u, allDescriptorSets, 0u, DE_NULL);
		vk.cmdDraw(*m_cmdBuffer, 4, 1u, 0u, 0u);

		endRenderPass(vk, *m_cmdBuffer);

		const tcu::IVec2 size{ (deInt32)m_renderArea.extent.width, (deInt32)m_renderArea.extent.height };
		copyImageToBuffer(vk, *m_cmdBuffer, **m_colorImage, *colorBuffer, size);
	}
	vk::endCommandBuffer(vk, *m_cmdBuffer);
	vk::submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *m_cmdBuffer);

	vk::invalidateAlloc(vk, device, colorBuffer.getAllocation());
	const tcu::ConstPixelBufferAccess colorPixelAccess(mapVkFormat(m_colorFormat), m_renderArea.extent.width, m_renderArea.extent.height, 1, colorBuffer.getAllocation().getHostPtr());

	// verify result
	deInt32 width	= (deInt32)m_renderArea.extent.width;
	deInt32 height	= (deInt32)m_renderArea.extent.height;
	const std::vector<VerificationData> verificationData
	{
		{ { 1, 1 },						{ 0, 191, 127, 51 } },		// note COLOR_COMPONENTS_NO_RED is used
		{ { width / 2, height / 2 },	{ 0, 191, 127, 51 } },
		{ { width - 2, height - 2 },	{ 0, 0, 0, 255 } }			// clear color
	};
	return verifyResult(verificationData, colorPixelAccess);
}

tcu::TestStatus PipelineLibraryMiscTestInstance::runCompareLinkTimes (void)
{
	const deUint32				uniqueLibrariesCount	= 2u;
	const deUint32				pipelinesCount			= 4u * uniqueLibrariesCount;

	const DeviceInterface&		vk						= m_context.getDeviceInterface();
	const VkDevice				device					= m_context.getDevice();
	tcu::TestLog&				log						= m_context.getTestContext().getLog();
	bool						allChecksPassed			= true;
	VkPipelineLayoutCreateInfo	pipelineLayoutParams	= initVulkanStructure();
	Move<VkPipelineLayout>		layout					= createPipelineLayout(vk, device, &pipelineLayoutParams);

	GraphicsPipelineCreateInfo partialPipelineCreateInfo[]
	{
		{ *layout, *m_renderPass, 0, VK_PIPELINE_CREATE_LIBRARY_BIT_KHR },
		{ *layout, *m_renderPass, 0, VK_PIPELINE_CREATE_LIBRARY_BIT_KHR },
		{ *layout, *m_renderPass, 0, VK_PIPELINE_CREATE_LIBRARY_BIT_KHR },
		{ *layout, *m_renderPass, 0, VK_PIPELINE_CREATE_LIBRARY_BIT_KHR },
		{ *layout, *m_renderPass, 0, VK_PIPELINE_CREATE_LIBRARY_BIT_KHR },
		{ *layout, *m_renderPass, 0, VK_PIPELINE_CREATE_LIBRARY_BIT_KHR },
		{ *layout, *m_renderPass, 0, VK_PIPELINE_CREATE_LIBRARY_BIT_KHR },
		{ *layout, *m_renderPass, 0, VK_PIPELINE_CREATE_LIBRARY_BIT_KHR },
	};

	de::Random rnd(static_cast<deUint32>(deGetMicroseconds()));

	const uint32_t vertexRandSpecConsts[]	= { rnd.getUint32() * 2, rnd.getUint32() * 2 };
	const uint32_t fragmentRandSpecConsts[] = { rnd.getUint32() * 2, rnd.getUint32() * 2 };

	const VkSpecializationMapEntry entry =
	{
		0,					// uint32_t	constantID;
		0,					// uint32_t	offset;
		sizeof(int32_t)		// size_t	size;
	};

	const VkSpecializationInfo vertexSpecializationInfos[] =
	{
		{
			1u,							// uint32_t							mapEntryCount;
			&entry,						// const VkSpecializationMapEntry*	pMapEntries;
			sizeof(int32_t),			// size_t							dataSize;
			&vertexRandSpecConsts[0]	// const void*						pData;
		},
		{
			1u,							// uint32_t							mapEntryCount;
			&entry,						// const VkSpecializationMapEntry*	pMapEntries;
			sizeof(int32_t),			// size_t							dataSize;
			&vertexRandSpecConsts[1]	// const void*						pData;
		}
	};

	const VkSpecializationInfo fragmentSpecializationInfos[] =
	{
		{
			1u,							// uint32_t							mapEntryCount;
			&entry,						// const VkSpecializationMapEntry*	pMapEntries;
			sizeof(int32_t),			// size_t							dataSize;
			&fragmentRandSpecConsts[0]	// const void*						pData;
		},
		{
			1u,							// uint32_t							mapEntryCount;
			&entry,						// const VkSpecializationMapEntry*	pMapEntries;
			sizeof(int32_t),			// size_t							dataSize;
			&fragmentRandSpecConsts[1]	// const void*						pData;
		}
	};

	// fill proper portion of pipeline state - this cant be easily done in a scalable loop
	updateVertexInputInterface		(m_context, partialPipelineCreateInfo[0], VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
	updateVertexInputInterface		(m_context, partialPipelineCreateInfo[1], VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	updatePreRasterization			(m_context, partialPipelineCreateInfo[2], false, VK_POLYGON_MODE_FILL, &vertexSpecializationInfos[0]);
	updatePreRasterization			(m_context, partialPipelineCreateInfo[3], false, VK_POLYGON_MODE_LINE, &vertexSpecializationInfos[1]);
	updatePostRasterization			(m_context, partialPipelineCreateInfo[4], false, true,	&fragmentSpecializationInfos[0]);
	updatePostRasterization			(m_context, partialPipelineCreateInfo[5], false, false, &fragmentSpecializationInfos[1]);
	updateFragmentOutputInterface	(m_context, partialPipelineCreateInfo[6], 0xf);
	updateFragmentOutputInterface	(m_context, partialPipelineCreateInfo[7]);

	// construct all pipeline parts and mesure time it took
	struct PipelinePartData
	{
		Move<VkPipeline>							pipelineHandle;
		std::chrono::duration<deInt64, std::nano>	creationDuration;
	};
	std::vector<PipelinePartData> pipelinePartData(pipelinesCount);
	VkGraphicsPipelineLibraryCreateInfoEXT libraryCreateInfo = makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT);
	for (deUint32 i = 0; i < pipelinesCount; ++i)
	{
		appendStructurePtrToVulkanChain(&partialPipelineCreateInfo[i].pNext, &libraryCreateInfo);
		libraryCreateInfo.flags = GRAPHICS_PIPELINE_LIBRARY_FLAGS[i / 2];

		auto&	partData	= pipelinePartData[i];
		auto	timeStart	= std::chrono::high_resolution_clock::now();
		partData.pipelineHandle		= createGraphicsPipeline(vk, device, DE_NULL, partialPipelineCreateInfo + i);
		partData.creationDuration	= std::chrono::high_resolution_clock::now() - timeStart;
	}

	// iterate over all combinations of parts
	for (deUint32 i = 0u ; i < (deUint32)dePow(4, uniqueLibrariesCount) ; ++i)
	{
		// select new unique combination of parts
		deUint32 vertexInputIndex		= (i    ) % 2;
		deUint32 preRasterizationIndex	= (i / 2) % 2;
		deUint32 fragmentStateIndex		= (i / 4) % 2;
		deUint32 fragmentOutputIndex	= (i / 8) % 2;

		const auto& vertexInputData			= pipelinePartData[                           vertexInputIndex];
		const auto& preRasterizationData	= pipelinePartData[    uniqueLibrariesCount + preRasterizationIndex];
		const auto& fragmentStateData		= pipelinePartData[2 * uniqueLibrariesCount + fragmentStateIndex];
		const auto& fragmentOutputData		= pipelinePartData[3 * uniqueLibrariesCount + fragmentOutputIndex];

		std::vector<VkPipeline> pipelinesToLink
		{
			*vertexInputData.pipelineHandle,
			*preRasterizationData.pipelineHandle,
			*fragmentStateData.pipelineHandle,
			*fragmentOutputData.pipelineHandle,
		};

		VkPipelineLibraryCreateInfoKHR	linkingInfo			= makePipelineLibraryCreateInfo(pipelinesToLink);
		VkGraphicsPipelineCreateInfo	finalPipelineInfo	= initVulkanStructure();
		finalPipelineInfo.layout = *layout;

		appendStructurePtrToVulkanChain(&finalPipelineInfo.pNext, &linkingInfo);

		// link pipeline without the optimised bit, and record the time taken to link it
		auto				timeStart		= std::chrono::high_resolution_clock::now();
		Move<VkPipeline>	pipeline		= createGraphicsPipeline(vk, device, DE_NULL, &finalPipelineInfo);
		const auto			linkingTime		= std::chrono::high_resolution_clock::now() - timeStart;
		const auto			creationTime	= preRasterizationData.creationDuration + fragmentStateData.creationDuration;

		if (linkingTime > (10 * creationTime))
		{
			allChecksPassed = false;
			log << tcu::TestLog::Message
				<< "Liking time (" << linkingTime.count() << ") of combination " << i
				<< " is more then ten times greater than creation of both pre-rasterization and fragment states (" << creationTime.count() << ")"
				<< tcu::TestLog::EndMessage;
		}
	}

	if (allChecksPassed)
		return tcu::TestStatus::pass("Pass");

	return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Liking of one or more combinations took to long");
}

tcu::TestStatus PipelineLibraryMiscTestInstance::verifyResult(const std::vector<VerificationData>& verificationData, const tcu::ConstPixelBufferAccess& colorPixelAccess) const
{
	const deInt32 epsilon = 1;
	for (const auto& v : verificationData)
	{
		const IVec4	pixel = colorPixelAccess.getPixelInt(v.point.x(), v.point.y());
		const IVec4	diff = pixel - v.color;
		for (deUint32 compNdx = 0; compNdx < 4u; ++compNdx)
		{
			if (de::abs(diff[compNdx]) > epsilon)
			{
				const Vec4 pixelBias(0.0f);
				const Vec4 pixelScale(1.0f);

				m_context.getTestContext().getLog()
					<< TestLog::Image("Result", "Result", colorPixelAccess, pixelScale, pixelBias)
					<< tcu::TestLog::Message
					<< "For texel " << v.point << " expected color "
					<< v.color << " got: " << pixel
					<< tcu::TestLog::EndMessage;

				return tcu::TestStatus::fail("Fail");
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

class PipelineLibraryShaderModuleInfoInstance : public TestInstance
{
public:
					PipelineLibraryShaderModuleInfoInstance		(Context& context)
						: TestInstance	(context)
						, m_vkd			(m_context.getDeviceInterface())
						, m_device		(m_context.getDevice())
						, m_alloc		(m_context.getDefaultAllocator())
						, m_queueIndex	(m_context.getUniversalQueueFamilyIndex())
						, m_queue		(m_context.getUniversalQueue())
						, m_outVector	(kOutputBufferElements, std::numeric_limits<uint32_t>::max())
						, m_cmdBuffer	(DE_NULL)
						{}
	virtual			~PipelineLibraryShaderModuleInfoInstance	(void) {}

	static constexpr size_t kOutputBufferElements = 64u;

protected:
	void			prepareOutputBuffer							(VkShaderStageFlags stages);
	void			allocateCmdBuffers							(void);
	void			addModule									(const std::string& moduleName, VkShaderStageFlagBits stage);
	void			recordShaderToHostBarrier					(VkPipelineStageFlagBits pipelineStage) const;
	void			verifyOutputBuffer							(void);

	using BufferWithMemoryPtr = de::MovePtr<BufferWithMemory>;

	// From the context.
	const DeviceInterface&		m_vkd;
	const VkDevice				m_device;
	Allocator&					m_alloc;
	const uint32_t				m_queueIndex;
	const VkQueue				m_queue;

	Move<VkDescriptorSetLayout>	m_setLayout;
	Move<VkDescriptorPool>		m_descriptorPool;
	Move<VkDescriptorSet>		m_descriptorSet;
	std::vector<uint32_t>		m_outVector;
	BufferWithMemoryPtr			m_outputBuffer;

	Move<VkCommandPool>			m_cmdPool;
	Move<VkCommandBuffer>		m_cmdBufferPtr;
	VkCommandBuffer				m_cmdBuffer;

	std::vector<VkPipelineShaderStageCreateInfo>	m_pipelineStageInfos;
	std::vector<VkShaderModuleCreateInfo>			m_shaderModuleInfos;
};

void PipelineLibraryShaderModuleInfoInstance::prepareOutputBuffer (VkShaderStageFlags stages)
{
	const auto	descriptorType	= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	const auto	poolFlags		= VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

	// Create set layout.
	DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(descriptorType, stages);
	m_setLayout = layoutBuilder.build(m_vkd, m_device);

	// Create pool and set.
	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(descriptorType);
	m_descriptorPool	= poolBuilder.build(m_vkd, m_device, poolFlags, 1u);
	m_descriptorSet		= makeDescriptorSet(m_vkd, m_device, m_descriptorPool.get(), m_setLayout.get());

	// Create buffer.
	const auto outputBufferSize			= static_cast<VkDeviceSize>(de::dataSize(m_outVector));
	const auto outputBufferCreateInfo	= makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	m_outputBuffer = BufferWithMemoryPtr(new BufferWithMemory(m_vkd, m_device, m_alloc, outputBufferCreateInfo, MemoryRequirement::HostVisible));

	// Update set.
	const auto outputBufferDescInfo = makeDescriptorBufferInfo(m_outputBuffer->get(), 0ull, outputBufferSize);
	DescriptorSetUpdateBuilder updateBuilder;
	updateBuilder.writeSingle(m_descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType, &outputBufferDescInfo);
	updateBuilder.update(m_vkd, m_device);
}

void PipelineLibraryShaderModuleInfoInstance::addModule (const std::string& moduleName, VkShaderStageFlagBits stage)
{
	const auto& binary = m_context.getBinaryCollection().get(moduleName);

	const VkShaderModuleCreateInfo modInfo =
	{
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,			//	VkStructureType				sType;
		nullptr,												//	const void*					pNext;
		0u,														//	VkShaderModuleCreateFlags	flags;
		binary.getSize(),										//	size_t						codeSize;
		reinterpret_cast<const uint32_t*>(binary.getBinary()),	//	const uint32_t*				pCode;
	};
	m_shaderModuleInfos.push_back(modInfo);

	// Note: the pNext pointer will be updated below.
	const VkPipelineShaderStageCreateInfo stageInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,												//	const void*							pNext;
		0u,														//	VkPipelineShaderStageCreateFlags	flags;
		stage,													//	VkShaderStageFlagBits				stage;
		DE_NULL,												//	VkShaderModule						module;
		"main",													//	const char*							pName;
		nullptr,												//	const VkSpecializationInfo*			pSpecializationInfo;
	};
	m_pipelineStageInfos.push_back(stageInfo);

	DE_ASSERT(m_shaderModuleInfos.size() == m_pipelineStageInfos.size());

	// Update pNext pointers after possible reallocation.
	for (size_t i = 0u; i < m_shaderModuleInfos.size(); ++i)
		m_pipelineStageInfos[i].pNext = &(m_shaderModuleInfos[i]);
}

void PipelineLibraryShaderModuleInfoInstance::recordShaderToHostBarrier (VkPipelineStageFlagBits pipelineStage) const
{
	const auto postWriteBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	cmdPipelineMemoryBarrier(m_vkd, m_cmdBuffer, pipelineStage, VK_PIPELINE_STAGE_HOST_BIT, &postWriteBarrier);
}

void PipelineLibraryShaderModuleInfoInstance::verifyOutputBuffer (void)
{
	auto& allocation	= m_outputBuffer->getAllocation();

	invalidateAlloc(m_vkd, m_device, allocation);
	deMemcpy(m_outVector.data(), allocation.getHostPtr(), de::dataSize(m_outVector));

	for (uint32_t i = 0; i < static_cast<uint32_t>(m_outVector.size()); ++i)
	{
		if (m_outVector[i] != i)
		{
			std::ostringstream msg;
			msg << "Unexpected value found at position " << i << ": " << m_outVector[i];
			TCU_FAIL(msg.str());
		}
	}
}

void PipelineLibraryShaderModuleInfoInstance::allocateCmdBuffers (void)
{
	m_cmdPool		= makeCommandPool(m_vkd, m_device, m_queueIndex);
	m_cmdBufferPtr	= allocateCommandBuffer(m_vkd, m_device, m_cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	m_cmdBuffer		= m_cmdBufferPtr.get();
}

class PipelineLibraryShaderModuleInfoCompInstance : public PipelineLibraryShaderModuleInfoInstance
{
public:
					PipelineLibraryShaderModuleInfoCompInstance		(Context& context)
						: PipelineLibraryShaderModuleInfoInstance(context)
						{}
	virtual			~PipelineLibraryShaderModuleInfoCompInstance	(void) {}

	tcu::TestStatus	iterate											(void) override;
};

tcu::TestStatus	PipelineLibraryShaderModuleInfoCompInstance::iterate (void)
{
	const auto stage		= VK_SHADER_STAGE_COMPUTE_BIT;
	const auto bindPoint	= VK_PIPELINE_BIND_POINT_COMPUTE;

	prepareOutputBuffer(stage);
	addModule("comp", stage);
	allocateCmdBuffers();

	const auto pipelineLayout = makePipelineLayout(m_vkd, m_device, m_setLayout.get());

	const VkComputePipelineCreateInfo pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	//	VkStructureType					sType;
		nullptr,										//	const void*						pNext;
		0u,												//	VkPipelineCreateFlags			flags;
		m_pipelineStageInfos.at(0u),					//	VkPipelineShaderStageCreateInfo	stage;
		pipelineLayout.get(),							//	VkPipelineLayout				layout;
		DE_NULL,										//	VkPipeline						basePipelineHandle;
		0,												//	int32_t							basePipelineIndex;
	};

	const auto pipeline = createComputePipeline(m_vkd, m_device, DE_NULL, &pipelineCreateInfo);

	beginCommandBuffer(m_vkd, m_cmdBuffer);
	m_vkd.cmdBindDescriptorSets(m_cmdBuffer, bindPoint, pipelineLayout.get(), 0u, 1u, &m_descriptorSet.get(), 0u, nullptr);
	m_vkd.cmdBindPipeline(m_cmdBuffer, bindPoint, pipeline.get());
	m_vkd.cmdDispatch(m_cmdBuffer, 1u, 1u, 1u);
	recordShaderToHostBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	endCommandBuffer(m_vkd, m_cmdBuffer);
	submitCommandsAndWait(m_vkd, m_device, m_queue, m_cmdBuffer);
	verifyOutputBuffer();

	return tcu::TestStatus::pass("Pass");
}

class PipelineLibraryShaderModuleInfoRTInstance : public PipelineLibraryShaderModuleInfoInstance
{
public:
					PipelineLibraryShaderModuleInfoRTInstance		(Context& context, bool withLibrary)
						: PipelineLibraryShaderModuleInfoInstance	(context)
						, m_withLibrary								(withLibrary)
						{}
	virtual			~PipelineLibraryShaderModuleInfoRTInstance		(void) {}

	tcu::TestStatus	iterate											(void) override;

protected:
	bool m_withLibrary;
};

tcu::TestStatus	PipelineLibraryShaderModuleInfoRTInstance::iterate (void)
{
	const auto stage		= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	const auto bindPoint	= VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;

	prepareOutputBuffer(stage);
	addModule("rgen", stage);
	allocateCmdBuffers();

	const auto pipelineLayout = makePipelineLayout(m_vkd, m_device, m_setLayout.get());

	const VkRayTracingShaderGroupCreateInfoKHR shaderGroupInfo =
	{
		VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,	//	VkStructureType					sType;
		nullptr,													//	const void*						pNext;
		VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,				//	VkRayTracingShaderGroupTypeKHR	type;
		0u,															//	uint32_t						generalShader;
		VK_SHADER_UNUSED_KHR,										//	uint32_t						closestHitShader;
		VK_SHADER_UNUSED_KHR,										//	uint32_t						anyHitShader;
		VK_SHADER_UNUSED_KHR,										//	uint32_t						intersectionShader;
		nullptr,													//	const void*						pShaderGroupCaptureReplayHandle;
	};

	const VkPipelineCreateFlags							createFlags		= (m_withLibrary ? static_cast<VkPipelineCreateFlags>(VK_PIPELINE_CREATE_LIBRARY_BIT_KHR) : 0u);
	const VkRayTracingPipelineInterfaceCreateInfoKHR	libIfaceInfo	= initVulkanStructure();
	const VkRayTracingPipelineInterfaceCreateInfoKHR*	pLibraryIface	= (m_withLibrary ? &libIfaceInfo : nullptr);

	const VkRayTracingPipelineCreateInfoKHR pipelineCreateInfo =
	{
		VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,	//	VkStructureType										sType;
		nullptr,												//	const void*											pNext;
		createFlags,											//	VkPipelineCreateFlags								flags;
		de::sizeU32(m_pipelineStageInfos),						//	uint32_t											stageCount;
		de::dataOrNull(m_pipelineStageInfos),					//	const VkPipelineShaderStageCreateInfo*				pStages;
		1u,														//	uint32_t											groupCount;
		&shaderGroupInfo,										//	const VkRayTracingShaderGroupCreateInfoKHR*			pGroups;
		1u,														//	uint32_t											maxPipelineRayRecursionDepth;
		nullptr,												//	const VkPipelineLibraryCreateInfoKHR*				pLibraryInfo;
		pLibraryIface,											//	const VkRayTracingPipelineInterfaceCreateInfoKHR*	pLibraryInterface;
		nullptr,												//	const VkPipelineDynamicStateCreateInfo*				pDynamicState;
		pipelineLayout.get(),									//	VkPipelineLayout									layout;
		DE_NULL,												//	VkPipeline											basePipelineHandle;
		0,														//	int32_t												basePipelineIndex;
	};

	Move<VkPipeline> pipelineLib;
	Move<VkPipeline> pipeline;

	if (m_withLibrary)
	{
		pipelineLib = createRayTracingPipelineKHR(m_vkd, m_device, DE_NULL, DE_NULL, &pipelineCreateInfo);

		const VkPipelineLibraryCreateInfoKHR libraryInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR,	//	VkStructureType		sType;
			nullptr,											//	const void*			pNext;
			1u,													//	uint32_t			libraryCount;
			&pipelineLib.get(),									//	const VkPipeline*	pLibraries;
		};

		const VkRayTracingPipelineCreateInfoKHR nonLibCreateInfo =
		{
			VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,	//	VkStructureType										sType;
			nullptr,												//	const void*											pNext;
			0u,														//	VkPipelineCreateFlags								flags;
			0u,														//	uint32_t											stageCount;
			nullptr,												//	const VkPipelineShaderStageCreateInfo*				pStages;
			0u,														//	uint32_t											groupCount;
			nullptr,												//	const VkRayTracingShaderGroupCreateInfoKHR*			pGroups;
			1u,														//	uint32_t											maxPipelineRayRecursionDepth;
			&libraryInfo,											//	const VkPipelineLibraryCreateInfoKHR*				pLibraryInfo;
			pLibraryIface,											//	const VkRayTracingPipelineInterfaceCreateInfoKHR*	pLibraryInterface;
			nullptr,												//	const VkPipelineDynamicStateCreateInfo*				pDynamicState;
			pipelineLayout.get(),									//	VkPipelineLayout									layout;
			DE_NULL,												//	VkPipeline											basePipelineHandle;
			0,														//	int32_t												basePipelineIndex;
		};
		pipeline = createRayTracingPipelineKHR(m_vkd, m_device, DE_NULL, DE_NULL, &nonLibCreateInfo);
	}
	else
	{
		pipeline = createRayTracingPipelineKHR(m_vkd, m_device, DE_NULL, DE_NULL, &pipelineCreateInfo);
	}

	// Make shader binding table.
	const auto			rtProperties	= makeRayTracingProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice());
	const auto			rtHandleSize	= rtProperties->getShaderGroupHandleSize();
	const auto			sbtSize			= static_cast<VkDeviceSize>(rtHandleSize);
	const auto			sbtMemReqs		= (MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);
	const auto			sbtCreateInfo	= makeBufferCreateInfo(sbtSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	BufferWithMemoryPtr	sbt				= BufferWithMemoryPtr(new BufferWithMemory(m_vkd, m_device, m_alloc, sbtCreateInfo, sbtMemReqs));
	auto&				sbtAlloc		= sbt->getAllocation();
	void*				sbtData			= sbtAlloc.getHostPtr();

	// Copy ray gen shader group handle to the start of  the buffer.
	VK_CHECK(m_vkd.getRayTracingShaderGroupHandlesKHR(m_device, pipeline.get(), 0u, 1u, static_cast<size_t>(sbtSize), sbtData));
	flushAlloc(m_vkd, m_device, sbtAlloc);

	// Strided device address regions.
	VkStridedDeviceAddressRegionKHR rgenSBTRegion	= makeStridedDeviceAddressRegionKHR(getBufferDeviceAddress(m_vkd, m_device, sbt->get(), 0), rtHandleSize, rtHandleSize);
	VkStridedDeviceAddressRegionKHR	missSBTRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR	hitsSBTRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);
	VkStridedDeviceAddressRegionKHR	callSBTRegion	= makeStridedDeviceAddressRegionKHR(DE_NULL, 0, 0);

	beginCommandBuffer(m_vkd, m_cmdBuffer);
	m_vkd.cmdBindDescriptorSets(m_cmdBuffer, bindPoint, pipelineLayout.get(), 0u, 1u, &m_descriptorSet.get(), 0u, nullptr);
	m_vkd.cmdBindPipeline(m_cmdBuffer, bindPoint, pipeline.get());
	m_vkd.cmdTraceRaysKHR(m_cmdBuffer, &rgenSBTRegion, &missSBTRegion, &hitsSBTRegion, &callSBTRegion, kOutputBufferElements, 1u, 1u);
	recordShaderToHostBarrier(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
	endCommandBuffer(m_vkd, m_cmdBuffer);
	submitCommandsAndWait(m_vkd, m_device, m_queue, m_cmdBuffer);
	verifyOutputBuffer();

	return tcu::TestStatus::pass("Pass");
}

class NullRenderingCreateInfoInstance : public vkt::TestInstance
{
public:
						NullRenderingCreateInfoInstance		(Context& context)
							: vkt::TestInstance(context)
							{}
	virtual				~NullRenderingCreateInfoInstance	(void) {}

	tcu::TestStatus		iterate			(void) override;
};

tcu::TestStatus NullRenderingCreateInfoInstance::iterate (void)
{
	const auto			ctx				= m_context.getContextCommonData();
	const tcu::IVec3	colorExtent		(1, 1, 1);
	const auto			imageExtent		= makeExtent3D(colorExtent);
	const auto			colorFormat		= VK_FORMAT_R8G8B8A8_UNORM;
	const auto			tcuFormat		= mapVkFormat(colorFormat);
	const auto			colorUsage		= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const auto			colorSRR		= makeDefaultImageSubresourceRange();
	const auto			colorSRL		= makeDefaultImageSubresourceLayers();

	// Color buffer and view.
	ImageWithBuffer	colorBuffer	(ctx.vkd, ctx.device, ctx.allocator, imageExtent, colorFormat, colorUsage, VK_IMAGE_TYPE_2D);
	const auto		colorView	= makeImageView(ctx.vkd, ctx.device, colorBuffer.getImage(), VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSRR);

	// Verification buffer.
	const auto			verificationBufferSize	= static_cast<VkDeviceSize>(colorExtent.x() * colorExtent.y() * colorExtent.z() * tcu::getPixelSize(tcuFormat));
	const auto			verificationBufferInfo	= makeBufferCreateInfo(verificationBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory	verificationBuffer		(ctx.vkd, ctx.device, ctx.allocator, verificationBufferInfo, MemoryRequirement::HostVisible);
	auto&				verificationBufferAlloc = verificationBuffer.getAllocation();
	void*				verificationBufferPtr	= verificationBufferAlloc.getHostPtr();

	const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device);

	const VkPipelineVertexInputStateCreateInfo	vertexInputStateInfo	= initVulkanStructure();
	VkPipelineInputAssemblyStateCreateInfo		inputAssemblyStateInfo	= initVulkanStructure();
	inputAssemblyStateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	const std::vector<VkViewport>	viewports	(1u, makeViewport(imageExtent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(imageExtent));

	const auto&	binaries	= m_context.getBinaryCollection();
	const auto	vertModule	= createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
	const auto	fragModule	= createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

	// We will use a null-filled pipeline rendering info structure for all substates except the fragment output state.
	VkPipelineRenderingCreateInfo nullRenderingInfo = initVulkanStructure();
	nullRenderingInfo.colorAttachmentCount = std::numeric_limits<uint32_t>::max();

	VkPipelineRenderingCreateInfo finalRenderingInfo = initVulkanStructure();
	finalRenderingInfo.colorAttachmentCount		= 1u;
	finalRenderingInfo.pColorAttachmentFormats	= &colorFormat;

	const VkPipelineViewportStateCreateInfo viewportStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,												//	const void*							pNext;
		0u,														//	VkPipelineViewportStateCreateFlags	flags;
		de::sizeU32(viewports),									//	uint32_t							viewportCount;
		de::dataOrNull(viewports),								//	const VkViewport*					pViewports;
		de::sizeU32(scissors),									//	uint32_t							scissorCount;
		de::dataOrNull(scissors),								//	const VkRect2D*						pScissors;
	};

	const VkPipelineRasterizationStateCreateInfo rasterizationStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		//	VkStructureType							sType;
		nullptr,														//	const void*								pNext;
		0u,																//	VkPipelineRasterizationStateCreateFlags	flags;
		VK_FALSE,														//	VkBool32								depthClampEnable;
		VK_FALSE,														//	VkBool32								rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,											//	VkPolygonMode							polygonMode;
		VK_CULL_MODE_BACK_BIT,											//	VkCullModeFlags							cullMode;
		VK_FRONT_FACE_COUNTER_CLOCKWISE,								//	VkFrontFace								frontFace;
		VK_FALSE,														//	VkBool32								depthBiasEnable;
		0.0f,															//	float									depthBiasConstantFactor;
		0.0f,															//	float									depthBiasClamp;
		0.0f,															//	float									depthBiasSlopeFactor;
		1.0f,															//	float									lineWidth;
	};

	const VkPipelineMultisampleStateCreateInfo multisampleStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,													//	const void*								pNext;
		0u,															//	VkPipelineMultisampleStateCreateFlags	flags;
		VK_SAMPLE_COUNT_1_BIT,										//	VkSampleCountFlagBits					rasterizationSamples;
		VK_FALSE,													//	VkBool32								sampleShadingEnable;
		1.0f,														//	float									minSampleShading;
		nullptr,													//	const VkSampleMask*						pSampleMask;
		VK_FALSE,													//	VkBool32								alphaToCoverageEnable;
		VK_FALSE,													//	VkBool32								alphaToOneEnable;
	};

	const VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo = initVulkanStructure();

	const VkColorComponentFlags colorComponentFlags = (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

	const VkPipelineColorBlendAttachmentState colorBlendAttachmentState =
	{
		VK_FALSE,				//	VkBool32				blendEnable;
		VK_BLEND_FACTOR_ZERO,	//	VkBlendFactor			srcColorBlendFactor;
		VK_BLEND_FACTOR_ZERO,	//	VkBlendFactor			dstColorBlendFactor;
		VK_BLEND_OP_ADD,		//	VkBlendOp				colorBlendOp;
		VK_BLEND_FACTOR_ZERO,	//	VkBlendFactor			srcAlphaBlendFactor;
		VK_BLEND_FACTOR_ZERO,	//	VkBlendFactor			dstAlphaBlendFactor;
		VK_BLEND_OP_ADD,		//	VkBlendOp				alphaBlendOp;
		colorComponentFlags,	//	VkColorComponentFlags	colorWriteMask;
	};

	const VkPipelineColorBlendStateCreateInfo colorBlendStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	//	VkStructureType								sType;
		nullptr,													//	const void*									pNext;
		0u,															//	VkPipelineColorBlendStateCreateFlags		flags;
		VK_FALSE,													//	VkBool32									logicOpEnable;
		VK_LOGIC_OP_CLEAR,											//	VkLogicOp									logicOp;
		1u,															//	uint32_t									attachmentCount;
		&colorBlendAttachmentState,									//	const VkPipelineColorBlendAttachmentState*	pAttachments;
		{ 0.0f, 0.0f, 0.0f, 0.0f },									//	float										blendConstants[4];
	};

	// Build the different pipeline pieces.
	Move<VkPipeline> vertexInputLib;
	Move<VkPipeline> preRasterShaderLib;
	Move<VkPipeline> fragShaderLib;
	Move<VkPipeline> fragOutputLib;

	const VkPipelineCreateFlags libCreationFlags	= VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
	const VkPipelineCreateFlags linkFlags			= 0u;

	// Vertex input state library.
	{
		VkGraphicsPipelineLibraryCreateInfoEXT vertexInputLibInfo	= initVulkanStructure();
		vertexInputLibInfo.flags									|= VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;

		VkGraphicsPipelineCreateInfo vertexInputPipelineInfo	= initVulkanStructure(&vertexInputLibInfo);
		vertexInputPipelineInfo.flags							= libCreationFlags;
		vertexInputPipelineInfo.pVertexInputState				= &vertexInputStateInfo;
		vertexInputPipelineInfo.pInputAssemblyState				= &inputAssemblyStateInfo;

		vertexInputLib = createGraphicsPipeline(ctx.vkd, ctx.device, VK_NULL_HANDLE, &vertexInputPipelineInfo);
	}

	// Pre-rasterization shader state library.
	{
		VkGraphicsPipelineLibraryCreateInfoEXT preRasterShaderLibInfo	= initVulkanStructure(&nullRenderingInfo); // What we're testing.
		preRasterShaderLibInfo.flags									|= VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;

		VkGraphicsPipelineCreateInfo preRasterShaderPipelineInfo	= initVulkanStructure(&preRasterShaderLibInfo);
		preRasterShaderPipelineInfo.flags							= libCreationFlags;
		preRasterShaderPipelineInfo.layout							= pipelineLayout.get();
		preRasterShaderPipelineInfo.pViewportState					= &viewportStateInfo;
		preRasterShaderPipelineInfo.pRasterizationState				= &rasterizationStateInfo;

		const auto vertShaderInfo = makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertModule.get());
		preRasterShaderPipelineInfo.stageCount	= 1u;
		preRasterShaderPipelineInfo.pStages		= &vertShaderInfo;

		preRasterShaderLib = createGraphicsPipeline(ctx.vkd, ctx.device, DE_NULL, &preRasterShaderPipelineInfo);
	}

	// Fragment shader stage library.
	{
		VkGraphicsPipelineLibraryCreateInfoEXT fragShaderLibInfo	= initVulkanStructure(&nullRenderingInfo); // What we're testing.
		fragShaderLibInfo.flags										|= VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;

		VkGraphicsPipelineCreateInfo fragShaderPipelineInfo	= initVulkanStructure(&fragShaderLibInfo);
		fragShaderPipelineInfo.flags						= libCreationFlags;
		fragShaderPipelineInfo.layout						= pipelineLayout.get();
		fragShaderPipelineInfo.pMultisampleState			= &multisampleStateInfo;
		fragShaderPipelineInfo.pDepthStencilState			= &depthStencilStateInfo;

		const auto fragShaderInfo = makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragModule.get());
		fragShaderPipelineInfo.stageCount	= 1u;
		fragShaderPipelineInfo.pStages		= &fragShaderInfo;

		fragShaderLib = createGraphicsPipeline(ctx.vkd, ctx.device, DE_NULL, &fragShaderPipelineInfo);
	}

	// Fragment output library.
	{
		VkGraphicsPipelineLibraryCreateInfoEXT fragOutputLibInfo	= initVulkanStructure(&finalRenderingInfo); // Good info only in the fragment output substate.
		fragOutputLibInfo.flags										|= VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;

		VkGraphicsPipelineCreateInfo fragOutputPipelineInfo	= initVulkanStructure(&fragOutputLibInfo);
		fragOutputPipelineInfo.flags						= libCreationFlags;
		fragOutputPipelineInfo.pColorBlendState				= &colorBlendStateInfo;
		fragOutputPipelineInfo.pMultisampleState			= &multisampleStateInfo;

		fragOutputLib = createGraphicsPipeline(ctx.vkd, ctx.device, DE_NULL, &fragOutputPipelineInfo);
	}

	// Linked pipeline.
	const std::vector<VkPipeline> libraryHandles
	{
		vertexInputLib.get(),
		preRasterShaderLib.get(),
		fragShaderLib.get(),
		fragOutputLib.get(),
	};

	VkPipelineLibraryCreateInfoKHR linkedPipelineLibraryInfo	= initVulkanStructure();
	linkedPipelineLibraryInfo.libraryCount						= de::sizeU32(libraryHandles);
	linkedPipelineLibraryInfo.pLibraries						= de::dataOrNull(libraryHandles);

	VkGraphicsPipelineCreateInfo linkedPipelineInfo	= initVulkanStructure(&linkedPipelineLibraryInfo);
	linkedPipelineInfo.flags						= linkFlags;
	linkedPipelineInfo.layout						= pipelineLayout.get();

	const auto pipeline = createGraphicsPipeline(ctx.vkd, ctx.device, DE_NULL, &linkedPipelineInfo);

	CommandPoolWithBuffer cmd (ctx.vkd, ctx.device, ctx.qfIndex);
	const auto cmdBuffer = cmd.cmdBuffer.get();

	const auto clearValue = makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f);

	beginCommandBuffer(ctx.vkd, cmdBuffer);

	const auto preRenderBarrier = makeImageMemoryBarrier(0u, (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
														 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
														 colorBuffer.getImage(), colorSRR);
	cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, &preRenderBarrier);

	beginRendering(ctx.vkd, cmdBuffer, colorView.get(), scissors.at(0u), clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
	ctx.vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
	endRendering(ctx.vkd, cmdBuffer);

	const auto color2Transfer = makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
													   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
													   colorBuffer.getImage(), colorSRR);
	cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &color2Transfer);
	const auto copyRegion = makeBufferImageCopy(imageExtent, colorSRL);
	ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, colorBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, verificationBuffer.get(), 1u, &copyRegion);

	const auto transfer2Host = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &transfer2Host);

	endCommandBuffer(ctx.vkd, cmdBuffer);

	submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
	invalidateAlloc(ctx.vkd, ctx.device, verificationBufferAlloc);

	auto&						testLog			= m_context.getTestContext().getLog();
	const tcu::Vec4				expectedColor	(0.0f, 0.0f, 1.0f, 1.0f); // Must match frag shader.
	const tcu::Vec4				threshold		(0.0f, 0.0f, 0.0f, 0.0f);
	tcu::ConstPixelBufferAccess	resultAccess	(tcuFormat, colorExtent, verificationBufferPtr);

	if (!tcu::floatThresholdCompare(testLog, "Result", "", expectedColor, resultAccess, threshold, tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Unexpected color buffer contents -- check log for details");

	return tcu::TestStatus::pass("Pass");
}

class PipelineLibraryMiscTestCase : public TestCase
{
public:
	PipelineLibraryMiscTestCase			(tcu::TestContext& context, const char* name, const MiscTestParams data);
	~PipelineLibraryMiscTestCase		(void) = default;

	void			checkSupport		(Context& context) const;
	void			initPrograms		(SourceCollections& programCollection) const;
	TestInstance*	createInstance		(Context& context) const;

private:
	MiscTestParams		m_testParams;
};

PipelineLibraryMiscTestCase::PipelineLibraryMiscTestCase(tcu::TestContext& context, const char* name, const MiscTestParams params)
	: TestCase			(context, name, "")
	, m_testParams		(params)
{
}

void PipelineLibraryMiscTestCase::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_graphics_pipeline_library");

	if ((m_testParams.mode == MiscTestMode::INDEPENDENT_PIPELINE_LAYOUT_SETS_FAST_LINKED) &&
		!context.getGraphicsPipelineLibraryPropertiesEXT().graphicsPipelineLibraryFastLinking)
		TCU_THROW(NotSupportedError, "graphicsPipelineLibraryFastLinking is not supported");

	if (m_testParams.mode == MiscTestMode::SHADER_MODULE_CREATE_INFO_RT || m_testParams.mode == MiscTestMode::SHADER_MODULE_CREATE_INFO_RT_LIB)
		context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

	if (m_testParams.mode == MiscTestMode::SHADER_MODULE_CREATE_INFO_RT_LIB)
		context.requireDeviceFunctionality("VK_KHR_pipeline_library");

	if (m_testParams.mode == MiscTestMode::NULL_RENDERING_CREATE_INFO)
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
}

void PipelineLibraryMiscTestCase::initPrograms(SourceCollections& programCollection) const
{
	if ((m_testParams.mode == MiscTestMode::BIND_NULL_DESCRIPTOR_SET) ||
		(m_testParams.mode == MiscTestMode::BIND_NULL_DESCRIPTOR_SET_IN_MONOLITHIC_PIPELINE))
	{
		std::string vertDefinition	= "";
		std::string fragDefinition	= "";
		std::string vertValue		= "  vec4 v = vec4(-1.0, 1.0, 2.0, -2.0);\n";
		std::string fragValue		= "  vec4 v = vec4(0.0, 0.2, 0.6, 0.75);\n";

		// define lambda that creates proper uniform buffer definition
		auto constructBufferDefinition = [](deUint32 setIndex)
		{
			return std::string("layout(set = ") + std::to_string(setIndex) + ", binding = 0) uniform buf\n"
				"{\n"
				"  vec4 v;\n"
				"};\n\n";
		};

		if (m_testParams.mode == MiscTestMode::BIND_NULL_DESCRIPTOR_SET_IN_MONOLITHIC_PIPELINE)
		{
			// descriptor set 0 will be DE_NULL, descriptor set 1 will be valid buffer with color
			fragDefinition	= constructBufferDefinition(1);
			fragValue		= "";
		}
		else if (m_testParams.layoutsBits > 0u)
		{
			std::vector<deUint32>	bitsThatAreSet;
			const deUint32			maxBitsCount	= 8 * sizeof(m_testParams.layoutsBits);

			// find set bits
			for (deUint32 i = 0u; i < m_testParams.layoutsCount; ++i)
			{
				if (m_testParams.layoutsBits & (1 << (maxBitsCount - 1 - i)))
					bitsThatAreSet.push_back(i);
			}

			// there should be 1 or 2 bits set
			DE_ASSERT((bitsThatAreSet.size() > 0) && (bitsThatAreSet.size() < 3));

			vertDefinition	= constructBufferDefinition(bitsThatAreSet[0]);
			vertValue		= "";

			if (bitsThatAreSet.size() == 2u)
			{
				fragDefinition	= constructBufferDefinition(bitsThatAreSet[1]);
				fragValue		= "";
			}
		}

		programCollection.glslSources.add("vert") << glu::VertexSource(
			std::string("#version 450\n"
			"precision mediump int;\nprecision highp float;\n") +
			vertDefinition +
			"out gl_PerVertex\n"
			"{\n"
			"  vec4 gl_Position;\n"
			"};\n\n"
			"void main()\n"
			"{\n" +
			vertValue +
			"  const float x = (v.x+v.z*((gl_VertexIndex & 2)>>1));\n"
			"  const float y = (v.y+v.w* (gl_VertexIndex % 2));\n"

			// note: there won't be full screen quad because of used scissors
			"  gl_Position = vec4(x, y, 0.0, 1.0);\n"
			"}\n");

		programCollection.glslSources.add("frag") << glu::FragmentSource(
			std::string("#version 450\n"
			"precision mediump int; precision highp float;"
			"layout(location = 0) out highp vec4 o_color;\n") +
			fragDefinition +
			"void main()\n"
			"{\n" +
			fragValue +
			"  o_color = v;\n"
			"}\n");
	}
	else if ((m_testParams.mode == MiscTestMode::INDEPENDENT_PIPELINE_LAYOUT_SETS_FAST_LINKED) ||
			 (m_testParams.mode == MiscTestMode::INDEPENDENT_PIPELINE_LAYOUT_SETS_WITH_LINK_TIME_OPTIMIZATION_UNION_HANDLE))
	{
		programCollection.glslSources.add("vert") << glu::VertexSource(
			"#version 450\n"
			"precision mediump int; precision highp float;\n"
			"layout(set = 0, binding = 0) uniform bufA\n"
			"{\n"
			"  vec4 valueA;\n"
			"};\n"
			"layout(set = 2, binding = 0) uniform bufC\n"
			"{\n"
			"  vec4 valueC;\n"
			"};\n"
			"out gl_PerVertex\n"
			"{\n"
			"  vec4 gl_Position;\n"
			"};\n\n"
			"void main()\n"
			"{\n"
			// note: values in buffers were set to get vec4(-1, 1, 2, -2)
			"  const vec4  v = valueA + valueC;\n"
			"  const float x = (v.x+v.z*((gl_VertexIndex & 2)>>1));\n"
			"  const float y = (v.y+v.w* (gl_VertexIndex % 2));\n"

			// note: there won't be full screen quad because of used scissors
			"  gl_Position = vec4(x, y, 0.0, 1.0);\n"
			"}\n");

		programCollection.glslSources.add("frag") << glu::FragmentSource(
			"#version 450\n"
			"precision mediump int; precision highp float;"
			"layout(location = 0) out highp vec4 o_color;\n"
			"layout(set = 0, binding = 0) uniform bufA\n"
			"{\n"
			"  vec4 valueA;\n"
			"};\n"
			"layout(set = 1, binding = 0) uniform bufB\n"
			"{\n"
			"  vec4 valueB;\n"
			"};\n"
			"void main()\n"
			"{\n"
			// note: values in buffers were set to get vec4(0.0, 0.75, 0.5, 0.2)
			"  o_color = valueA * valueB;\n"
			"}\n");
	}
	else if (m_testParams.mode == MiscTestMode::COMPARE_LINK_TIMES)
	{
		programCollection.glslSources.add("vert") << glu::VertexSource(
			"#version 450\n"
			"precision mediump int; precision highp float;"
			"layout(location = 0) in vec4 in_position;\n"
			"out gl_PerVertex\n"
			"{\n"
			"  vec4 gl_Position;\n"
			"};\n"
			"layout(constant_id = 0) const int random = 0;\n\n"
			"void main()\n"
			"{\n"
			"   gl_Position = vec4(float(1 - 2 * int(gl_VertexIndex != 1)),\n"
			"                      float(1 - 2 * int(gl_VertexIndex > 0)), 0.0, 1.0) + float(random & 1);\n"
			"}\n");

		programCollection.glslSources.add("frag") << glu::FragmentSource(
			"#version 450\n"
			"precision mediump int; precision highp float;"
			"layout(location = 0) out highp vec4 o_color;\n"
			"layout(constant_id = 0) const int random = 0;\n\n"
			"void main()\n"
			"{\n"
			"  o_color = vec4(0.0, 1.0, 0.5, 1.0) + float(random & 1);\n"
			"}\n");
	}
	else if (m_testParams.mode == MiscTestMode::SHADER_MODULE_CREATE_INFO_COMP)
	{
		std::ostringstream comp;
		comp
			<< "#version 450\n"
			<< "layout (set=0, binding=0, std430) buffer BufferBlock {\n"
			<< "    uint values[" << PipelineLibraryShaderModuleInfoInstance::kOutputBufferElements << "];\n"
			<< "} outBuffer;\n"
			<< "layout (local_size_x=" << PipelineLibraryShaderModuleInfoInstance::kOutputBufferElements << ", local_size_y=1, local_size_z=1) in;\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    outBuffer.values[gl_LocalInvocationIndex] = gl_LocalInvocationIndex;\n"
			<< "}\n"
			;
		programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
	}
	else if (m_testParams.mode == MiscTestMode::SHADER_MODULE_CREATE_INFO_RT || m_testParams.mode == MiscTestMode::SHADER_MODULE_CREATE_INFO_RT_LIB)
	{
		const vk::ShaderBuildOptions	buildOptions (programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
		std::ostringstream				rgen;
		rgen
			<< "#version 460 core\n"
			<< "#extension GL_EXT_ray_tracing : require\n"
			<< "layout (set=0, binding=0, std430) buffer BufferBlock {\n"
			<< "    uint values[" << PipelineLibraryShaderModuleInfoInstance::kOutputBufferElements << "];\n"
			<< "} outBuffer;\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    outBuffer.values[gl_LaunchIDEXT.x] = gl_LaunchIDEXT.x;\n"
			<< "}\n"
			;
		programCollection.glslSources.add("rgen") << glu::RaygenSource(rgen.str()) << buildOptions;
	}
	else if (m_testParams.mode == MiscTestMode::NULL_RENDERING_CREATE_INFO)
	{
		std::ostringstream vert;
		vert
			<< "#version 460\n"
			<< "vec2 positions[3] = vec2[](\n"
			<< "    vec2(-1.0, -1.0),\n"
			<< "    vec2(-1.0,  3.0),\n"
			<< "    vec2( 3.0, -1.0)\n"
			<< ");\n"
			<< "void main() {\n"
			<< "    gl_Position = vec4(positions[gl_VertexIndex % 3], 0.0, 1.0);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

		std::ostringstream frag;
		frag
			<< "#version 460\n"
			<< "layout (location=0) out vec4 color;\n"
			<< "void main () {\n"
			<< "    color = vec4(0.0, 0.0, 1.0, 1.0);\n"
			<< "}\n"
			;
		programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
	}
	else
	{
		DE_ASSERT(false);
	}
}

TestInstance* PipelineLibraryMiscTestCase::createInstance(Context& context) const
{
	if (m_testParams.mode == MiscTestMode::SHADER_MODULE_CREATE_INFO_COMP)
		return new PipelineLibraryShaderModuleInfoCompInstance(context);

	if (m_testParams.mode == MiscTestMode::SHADER_MODULE_CREATE_INFO_RT)
		return new PipelineLibraryShaderModuleInfoRTInstance(context, false/*withLibrary*/);

	if (m_testParams.mode == MiscTestMode::SHADER_MODULE_CREATE_INFO_RT_LIB)
		return new PipelineLibraryShaderModuleInfoRTInstance(context, true/*withLibrary*/);

	if (m_testParams.mode == MiscTestMode::NULL_RENDERING_CREATE_INFO)
		return new NullRenderingCreateInfoInstance(context);

	return new PipelineLibraryMiscTestInstance(context, m_testParams);
}

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
	};

	for (size_t libConfigNdx = 0; libConfigNdx < DE_LENGTH_OF_ARRAY(pipelineTreeConfiguration); ++libConfigNdx)
	{
		const bool			delayedShaderCreate	= (libConfigNdx != 0);
		const TestParams	testParams			=
		{
			pipelineTreeConfiguration[libConfigNdx],	//  PipelineTreeConfiguration	pipelineTreeConfiguration;
			optimize,									//  bool						optimize;
			delayedShaderCreate,						//  bool						delayedShaderCreate;
			false										//  bool						useMaintenance5;
		};
		const std::string	testName			= getTestName(pipelineTreeConfiguration[libConfigNdx]);

		if (optimize && testParams.pipelineTreeConfiguration.size() == 1)
			continue;

		group->addChild(new PipelineLibraryTestCase(group->getTestContext(), testName.c_str(), "", testParams));
	}

	// repeat first case (one that creates montolithic pipeline) to test VK_KHR_maintenance5;
	// VkShaderModule deprecation (tested with delayedShaderCreate) was added to VK_KHR_maintenance5
	if (optimize == false)
	{
		const TestParams testParams
		{
			pipelineTreeConfiguration[0],				//  PipelineTreeConfiguration	pipelineTreeConfiguration;
			false,										//  bool						optimize;
			true,										//  bool						delayedShaderCreate;
			true										//  bool						useMaintenance5;
		};

		group->addChild(new PipelineLibraryTestCase(group->getTestContext(), "maintenance5", "", testParams));
	}
}

}	// anonymous

tcu::TestCaseGroup*	createPipelineLibraryTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "graphics_library", "Tests verifying graphics pipeline libraries"));

	addTestGroup(group.get(), "fast", "Tests graphics pipeline libraries linkage without optimization", addPipelineLibraryConfigurationsTests, false);
	addTestGroup(group.get(), "optimize", "Tests graphics pipeline libraries linkage with optimization", addPipelineLibraryConfigurationsTests, true);

	de::MovePtr<tcu::TestCaseGroup> miscTests(new tcu::TestCaseGroup(testCtx, "misc", "Miscellaneous graphics pipeline library tests"));

	de::MovePtr<tcu::TestCaseGroup> independentLayoutSetsTests(new tcu::TestCaseGroup(testCtx, "independent_pipeline_layout_sets", ""));
	independentLayoutSetsTests->addChild(new PipelineLibraryMiscTestCase(testCtx, "fast_linked", { MiscTestMode::INDEPENDENT_PIPELINE_LAYOUT_SETS_FAST_LINKED, 0u, 0u }));
	independentLayoutSetsTests->addChild(new PipelineLibraryMiscTestCase(testCtx, "link_opt_union_handle", { MiscTestMode::INDEPENDENT_PIPELINE_LAYOUT_SETS_WITH_LINK_TIME_OPTIMIZATION_UNION_HANDLE, 0u, 0u }));
	miscTests->addChild(independentLayoutSetsTests.release());

	de::MovePtr<tcu::TestCaseGroup> bindNullDescriptorCombinationsTests(new tcu::TestCaseGroup(testCtx, "bind_null_descriptor_set", ""));
	const std::vector<const char*> bindNullDescriptorCombinations
	{
		// note: there will be as many descriptor sets layouts in pipeline layout as there are chcaracters in the case name;
		// '0' represents unused descriptor set layout, '1' represents used one;
		// location of first '1' represents index of layout used in vertex shader;
		// if present second '1' represents index of layout used in fragment shader
		"1",
		"11",
		"01",
		"10",
		"101",
		"1010",
		"1001"		// descriptor sets layouts for first pipeline part will be (&layoutA, NULL, NULL, NULL),
					//									 for second pipeline part (NULL, NULL, NULL, &layoutB)
	};
	for (const char* name : bindNullDescriptorCombinations)
	{
		deUint32 layoutsCount	= static_cast<deUint32>(strlen(name));
		deUint32 layoutsBits	= 0u;

		// construct deUint32 with bits sets based on case name
		for (deUint32 i = 0; i < layoutsCount; ++i)
			layoutsBits |= (name[i] == '1') * (1 << (8 * sizeof(layoutsBits) - i - 1));

		bindNullDescriptorCombinationsTests->addChild(new PipelineLibraryMiscTestCase(testCtx, name, { MiscTestMode::BIND_NULL_DESCRIPTOR_SET, layoutsCount, layoutsBits }));
	}
	miscTests->addChild(bindNullDescriptorCombinationsTests.release());

	{
		de::MovePtr<tcu::TestCaseGroup> otherTests(new tcu::TestCaseGroup(testCtx, "other", ""));
		otherTests->addChild(new PipelineLibraryMiscTestCase(testCtx, "compare_link_times", { MiscTestMode::COMPARE_LINK_TIMES, 0u, 0u }));
		otherTests->addChild(new PipelineLibraryMiscTestCase(testCtx, "null_descriptor_set_in_monolithic_pipeline", { MiscTestMode::BIND_NULL_DESCRIPTOR_SET_IN_MONOLITHIC_PIPELINE, 0u, 0u }));
		otherTests->addChild(new PipelineLibraryMiscTestCase(testCtx, "null_rendering_create_info", { MiscTestMode::NULL_RENDERING_CREATE_INFO, 0u, 0u }));
		miscTests->addChild(otherTests.release());
	}

	{
		de::MovePtr<tcu::TestCaseGroup> nonGraphicsTests(new tcu::TestCaseGroup(testCtx, "non_graphics", "Tests that do not use graphics pipelines"));
		nonGraphicsTests->addChild(new PipelineLibraryMiscTestCase(testCtx, "shader_module_info_comp",		{ MiscTestMode::SHADER_MODULE_CREATE_INFO_COMP, 0u, 0u }));
		nonGraphicsTests->addChild(new PipelineLibraryMiscTestCase(testCtx, "shader_module_info_rt",		{ MiscTestMode::SHADER_MODULE_CREATE_INFO_RT, 0u, 0u }));
		nonGraphicsTests->addChild(new PipelineLibraryMiscTestCase(testCtx, "shader_module_info_rt_lib",	{ MiscTestMode::SHADER_MODULE_CREATE_INFO_RT_LIB, 0u, 0u }));
		miscTests->addChild(nonGraphicsTests.release());
	}

	group->addChild(miscTests.release());

	return group.release();
}

}	// pipeline

}	// vkt
