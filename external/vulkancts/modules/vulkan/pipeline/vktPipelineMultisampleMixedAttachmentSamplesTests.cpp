/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Advanced Micro Devices, Inc.
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief Tests for VK_AMD_mixed_attachment_samples
 *//*--------------------------------------------------------------------*/

#include "vktPipelineMultisampleMixedAttachmentSamplesTests.hpp"
#include "vktPipelineSampleLocationsUtil.hpp"
#include "vktPipelineMakeUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkPlatform.hpp"
#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkPrograms.hpp"
#include "vkImageUtil.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deRandom.hpp"
#include "deMath.h"

#include "tcuVector.hpp"
#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"

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
using tcu::UVec2;
using tcu::Vec2;
using tcu::Vec4;

bool compareGreenImage (tcu::TestLog& log, const char* name, const char* description, const tcu::ConstPixelBufferAccess& image)
{
	tcu::TextureLevel greenImage(image.getFormat(), image.getWidth(), image.getHeight());
	tcu::clear(greenImage.getAccess(), tcu::RGBA::green().toIVec());
	return tcu::intThresholdCompare(log, name, description, greenImage.getAccess(), image, tcu::UVec4(2u), tcu::COMPARE_LOG_RESULT);
}

VkImageAspectFlags getImageAspectFlags (const VkFormat format)
{
	const tcu::TextureFormat tcuFormat = mapVkFormat(format);

	if      (tcuFormat.order == tcu::TextureFormat::DS)		return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	else if (tcuFormat.order == tcu::TextureFormat::D)		return VK_IMAGE_ASPECT_DEPTH_BIT;
	else if (tcuFormat.order == tcu::TextureFormat::S)		return VK_IMAGE_ASPECT_STENCIL_BIT;

	DE_ASSERT(false);
	return 0u;
}

struct CompareData
{
	Vec4		color;
	float		depth;
	deUint32	stencil;

	// Pad to 2*16 bytes, in the shader the base alignment of this structure is 16 due to vec4
	deUint32	padding[2];

	CompareData() : color(Vec4(0.0f)), depth(0.0f), stencil(0u)
	{
		padding[0] = 0u;
		padding[1] = 0u;

		static_assert(sizeof(CompareData) == (2 * 16), "Wrong structure size, expected 16 bytes");
	}
};

//! Make a (unused) sampler.
Move<VkSampler> makeSampler (const DeviceInterface& vk, const VkDevice device)
{
	const VkSamplerCreateInfo samplerParams =
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,			// VkStructureType         sType;
		DE_NULL,										// const void*             pNext;
		(VkSamplerCreateFlags)0,						// VkSamplerCreateFlags    flags;
		VK_FILTER_NEAREST,								// VkFilter                magFilter;
		VK_FILTER_NEAREST,								// VkFilter                minFilter;
		VK_SAMPLER_MIPMAP_MODE_NEAREST,					// VkSamplerMipmapMode     mipmapMode;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			// VkSamplerAddressMode    addressModeU;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			// VkSamplerAddressMode    addressModeV;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			// VkSamplerAddressMode    addressModeW;
		0.0f,											// float                   mipLodBias;
		VK_FALSE,										// VkBool32                anisotropyEnable;
		1.0f,											// float                   maxAnisotropy;
		VK_FALSE,										// VkBool32                compareEnable;
		VK_COMPARE_OP_ALWAYS,							// VkCompareOp             compareOp;
		0.0f,											// float                   minLod;
		0.0f,											// float                   maxLod;
		VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,		// VkBorderColor           borderColor;
		VK_FALSE,										// VkBool32                unnormalizedCoordinates;
	};
	return createSampler(vk, device, &samplerParams);
}

Move<VkImage> makeImage (const DeviceInterface&			vk,
						 const VkDevice					device,
						 const VkFormat					format,
						 const UVec2&					size,
						 const VkSampleCountFlagBits	samples,
						 const VkImageUsageFlags		usage)
{
	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		(VkImageCreateFlags)0,							// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,								// VkImageType				imageType;
		format,											// VkFormat					format;
		makeExtent3D(size.x(), size.y(), 1),			// VkExtent3D				extent;
		1u,												// deUint32					mipLevels;
		1u,												// deUint32					arrayLayers;
		samples,										// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling;
		usage,											// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode;
		0u,												// deUint32					queueFamilyIndexCount;
		DE_NULL,										// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			initialLayout;
	};
	return createImage(vk, device, &imageParams);
}

inline bool isDepthFormat (const VkFormat format)
{
	return (getImageAspectFlags(format) & VK_IMAGE_ASPECT_DEPTH_BIT) != 0;
}

inline bool isStencilFormat (const VkFormat format)
{
	return (getImageAspectFlags(format) & VK_IMAGE_ASPECT_STENCIL_BIT) != 0;
}

//! Create a test-specific MSAA pipeline
Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&					vk,
									   const VkDevice							device,
									   const VkPipelineLayout					pipelineLayout,
									   const VkRenderPass						renderPass,
									   const VkShaderModule						vertexModule,
									   const VkShaderModule						fragmentModule,
									   const bool								useVertexInput,
									   const deUint32							subpassNdx,
									   const UVec2&								renderSize,
									   const VkImageAspectFlags					depthStencilAspect,	//!< Used to determine which D/S tests to turn on
									   const VkSampleCountFlagBits				numSamples,
									   const bool								sampleShadingEnable,
									   const bool								useFragmentShadingRate,
									   const VkSampleLocationsInfoEXT*			pSampleLocationsInfo = DE_NULL)
{
	std::vector<VkVertexInputBindingDescription>	vertexInputBindingDescriptions;
	std::vector<VkVertexInputAttributeDescription>	vertexInputAttributeDescriptions;

	// Vertex attributes: position and color
	if (useVertexInput)
	{
		vertexInputBindingDescriptions.push_back  (makeVertexInputBindingDescription  (0u, 2 * sizeof(Vec4), VK_VERTEX_INPUT_RATE_VERTEX));
		vertexInputAttributeDescriptions.push_back(makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u));
		vertexInputAttributeDescriptions.push_back(makeVertexInputAttributeDescription(1u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(Vec4)));
	}

	const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		(VkPipelineVertexInputStateCreateFlags)0,						// VkPipelineVertexInputStateCreateFlags	flags;
		static_cast<deUint32>(vertexInputBindingDescriptions.size()),	// uint32_t									vertexBindingDescriptionCount;
		dataOrNullPtr(vertexInputBindingDescriptions),					// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
		static_cast<deUint32>(vertexInputAttributeDescriptions.size()),	// uint32_t									vertexAttributeDescriptionCount;
		dataOrNullPtr(vertexInputAttributeDescriptions),				// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	const VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,														// const void*								pNext;
		(VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags	flags;
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// VkPrimitiveTopology						topology;
		VK_FALSE,														// VkBool32									primitiveRestartEnable;
	};

	const VkViewport viewport =
	{
		0.0f, 0.0f,																	// x, y
		static_cast<float>(renderSize.x()), static_cast<float>(renderSize.y()),		// widht, height
		0.0f, 1.0f																	// minDepth, maxDepth
	};

	const VkRect2D scissor =
	{
		makeOffset2D(0, 0),
		makeExtent2D(renderSize.x(), renderSize.y()),
	};

	const VkPipelineViewportStateCreateInfo pipelineViewportStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,			// VkStructureType						sType;
		DE_NULL,														// const void*							pNext;
		(VkPipelineViewportStateCreateFlags)0,							// VkPipelineViewportStateCreateFlags	flags;
		1u,																// uint32_t								viewportCount;
		&viewport,														// const VkViewport*					pViewports;
		1u,																// uint32_t								scissorCount;
		&scissor,														// const VkRect2D*						pScissors;
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

	VkPipelineSampleLocationsStateCreateInfoEXT pipelineSampleLocationsCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT,	// VkStructureType             sType;
		DE_NULL,															// const void*                 pNext;
		VK_TRUE,															// VkBool32                    sampleLocationsEnable;
		VkSampleLocationsInfoEXT(),											// VkSampleLocationsInfoEXT    sampleLocationsInfo;
	};

	VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		(VkPipelineMultisampleStateCreateFlags)0,					// VkPipelineMultisampleStateCreateFlags	flags;
		numSamples,													// VkSampleCountFlagBits					rasterizationSamples;
		sampleShadingEnable,										// VkBool32									sampleShadingEnable;
		1.0f,														// float									minSampleShading;
		DE_NULL,													// const VkSampleMask*						pSampleMask;
		VK_FALSE,													// VkBool32									alphaToCoverageEnable;
		VK_FALSE													// VkBool32									alphaToOneEnable;
	};

	if (pSampleLocationsInfo)
	{
		pipelineSampleLocationsCreateInfo.sampleLocationsInfo	= *pSampleLocationsInfo;
		pipelineMultisampleStateInfo.pNext						= &pipelineSampleLocationsCreateInfo;
	}

	// Simply increment the buffer
	const VkStencilOpState stencilOpState = makeStencilOpState(
		VK_STENCIL_OP_KEEP,						// stencil fail
		VK_STENCIL_OP_INCREMENT_AND_CLAMP,		// depth & stencil pass
		VK_STENCIL_OP_KEEP,						// depth only fail
		VK_COMPARE_OP_ALWAYS,					// compare op
		~0u,									// compare mask
		~0u,									// write mask
		0u);									// reference

	// Always pass the depth test
	VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		(VkPipelineDepthStencilStateCreateFlags)0,					// VkPipelineDepthStencilStateCreateFlags	flags;
		(depthStencilAspect & VK_IMAGE_ASPECT_DEPTH_BIT) != 0u,		// VkBool32									depthTestEnable;
		VK_TRUE,													// VkBool32									depthWriteEnable;
		VK_COMPARE_OP_ALWAYS,										// VkCompareOp								depthCompareOp;
		VK_FALSE,													// VkBool32									depthBoundsTestEnable;
		(depthStencilAspect & VK_IMAGE_ASPECT_STENCIL_BIT) != 0u,	// VkBool32									stencilTestEnable;
		stencilOpState,												// VkStencilOpState							front;
		stencilOpState,												// VkStencilOpState							back;
		0.0f,														// float									minDepthBounds;
		1.0f,														// float									maxDepthBounds;
	};

	const VkColorComponentFlags colorComponentsAll = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	const VkPipelineColorBlendAttachmentState defaultBlendAttachmentState =
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
		&defaultBlendAttachmentState,								// const VkPipelineColorBlendAttachmentState*	pAttachments;
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

	VkPipelineFragmentShadingRateStateCreateInfoKHR shadingRateStateCreateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR,								// VkStructureType						sType;
		DE_NULL,																							// const void*							pNext;
		{ 2, 2 },																							// VkExtent2D							fragmentSize;
		{ VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR, VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR },	// VkFragmentShadingRateCombinerOpKHR	combinerOps[2];
	};

	const VkGraphicsPipelineCreateInfo graphicsPipelineInfo =
	{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,					// VkStructureType									sType;
		useFragmentShadingRate ? &shadingRateStateCreateInfo : DE_NULL,		// const void*										pNext;
		(VkPipelineCreateFlags)0,											// VkPipelineCreateFlags							flags;
		DE_LENGTH_OF_ARRAY(pShaderStages),									// deUint32											stageCount;
		pShaderStages,														// const VkPipelineShaderStageCreateInfo*			pStages;
		&vertexInputStateInfo,												// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
		&pipelineInputAssemblyStateInfo,									// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
		DE_NULL,															// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
		&pipelineViewportStateInfo,											// const VkPipelineViewportStateCreateInfo*			pViewportState;
		&pipelineRasterizationStateInfo,									// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
		&pipelineMultisampleStateInfo,										// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
		&pipelineDepthStencilStateInfo,										// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
		&pipelineColorBlendStateInfo,										// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
		DE_NULL,															// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
		pipelineLayout,														// VkPipelineLayout									layout;
		renderPass,															// VkRenderPass										renderPass;
		subpassNdx,															// deUint32											subpass;
		DE_NULL,															// VkPipeline										basePipelineHandle;
		-1,																	// deInt32											basePipelineIndex;
	};

	return createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineInfo);
}

//! Wrap float after an increment
inline float wrapIncFloat (float a, float min, float max)
{
	return deFloatMax(min, deFloatMod(a, max));
}

//! Generate expected data for color, depth, and stencil samples of a given image.
//! Samples are ordered starting at pixel (0, 0) - see compute shader source for reference.
std::vector<CompareData> generateCompareData (const deUint32	seed,
											  const UVec2&		imageSize,
											  const deUint32	numCoverageSamples,
											  const deUint32	numColorSamples,
											  const deUint32	numDepthStencilSamples)
{
	std::vector<CompareData>	allData;
	de::Random					rng (seed);

	for (deUint32 y		 = 0u; y	  < imageSize.y();		++y)
	for (deUint32 x		 = 0u; x	  < imageSize.x();		++x)
	for (deUint32 sample = 0u; sample < numCoverageSamples; ++sample)
	{
		CompareData cd;

		if (sample < numColorSamples)
		{
			for (int i = 0; i < 3; ++i)
				cd.color[i]	= 0.1f * static_cast<float>(rng.getInt(1, 10));

			cd.color.w() = 1.0f;
		}

		if (sample < numDepthStencilSamples)
		{
			const deUint32 globalSample = sample + numColorSamples * (x + imageSize.x() * y);
			cd.depth	= wrapIncFloat(0.05f * static_cast<float>(1 + globalSample), 0.05f, 1.0f);
			cd.stencil	= 1 + globalSample % numCoverageSamples;
		}

		allData.push_back(cd);
	}

	return allData;
}

//! NDC transformation algorithm for sample locations
template<typename SampleAccessor>
std::vector<Vec2> ndcTransformEachSampleInPixel (const UVec2& framebufferSize, const deUint32 numSamplesPerPixel, const SampleAccessor& access)
{
	std::vector<Vec2> locations;

	for (deUint32 y			= 0; y			< framebufferSize.y();	++y)
	for (deUint32 x			= 0; x			< framebufferSize.x();	++x)
	for (deUint32 sampleNdx	= 0; sampleNdx	< numSamplesPerPixel;	++sampleNdx)
	{
		const Vec2& sp = access(x, y, sampleNdx);
		const float	globalX  = sp.x() + static_cast<float>(x);
		const float	globalY  = sp.y() + static_cast<float>(y);

		// Transform to [-1, 1] space
		locations.push_back(Vec2(-1.0f + 2.0f * (globalX / static_cast<float>(framebufferSize.x())),
								 -1.0f + 2.0f * (globalY / static_cast<float>(framebufferSize.y()))));
	}

	return locations;
}

class AccessStandardSampleLocationsArray
{
public:
	AccessStandardSampleLocationsArray (const Vec2* ptr) : m_pData (ptr) {}

	const Vec2& operator ()(const deUint32 x, const deUint32 y, const deUint32 sampleNdx) const
	{
		DE_UNREF(x);
		DE_UNREF(y);
		return m_pData[sampleNdx];
	}

private:
	const Vec2*	m_pData;
};

class AccessMultisamplePixelGrid
{
public:
	AccessMultisamplePixelGrid (const MultisamplePixelGrid* ptr) : m_pGrid (ptr) {}

	Vec2 operator ()(const deUint32 x, const deUint32 y, const deUint32 sampleNdx) const
	{
		const VkSampleLocationEXT& sp = m_pGrid->getSample(x, y, sampleNdx);
		return Vec2(sp.x, sp.y);
	}

private:
	const MultisamplePixelGrid*	m_pGrid;
};

//! Generate NDC space standard sample locations at each framebuffer pixel
//! Data is filled starting at pixel (0,0) and for each pixel there are numSamples samples
std::vector<Vec2> genFramebufferStandardSampleLocations (const VkSampleCountFlagBits numSamples, const UVec2& framebufferSize)
{
	static const Vec2 s_location_samples_1[] =
	{
		Vec2(0.5f, 0.5f),
	};
	static const Vec2 s_location_samples_2[] =
	{
		Vec2(0.75f, 0.75f),
		Vec2(0.25f, 0.25f),
	};
	static const Vec2 s_location_samples_4[] =
	{
		Vec2(0.375f, 0.125f),
		Vec2(0.875f, 0.375f),
		Vec2(0.125f, 0.625f),
		Vec2(0.625f, 0.875f),
	};
	static const Vec2 s_location_samples_8[] =
	{
		Vec2(0.5625f, 0.3125f),
		Vec2(0.4375f, 0.6875f),
		Vec2(0.8125f, 0.5625f),
		Vec2(0.3125f, 0.1875f),
		Vec2(0.1875f, 0.8125f),
		Vec2(0.0625f, 0.4375f),
		Vec2(0.6875f, 0.9375f),
		Vec2(0.9375f, 0.0625f),
	};
	static const Vec2 s_location_samples_16[] =
	{
		Vec2(0.5625f, 0.5625f),
		Vec2(0.4375f, 0.3125f),
		Vec2(0.3125f, 0.6250f),
		Vec2(0.7500f, 0.4375f),
		Vec2(0.1875f, 0.3750f),
		Vec2(0.6250f, 0.8125f),
		Vec2(0.8125f, 0.6875f),
		Vec2(0.6875f, 0.1875f),
		Vec2(0.3750f, 0.8750f),
		Vec2(0.5000f, 0.0625f),
		Vec2(0.2500f, 0.1250f),
		Vec2(0.1250f, 0.7500f),
		Vec2(0.0000f, 0.5000f),
		Vec2(0.9375f, 0.2500f),
		Vec2(0.8750f, 0.9375f),
		Vec2(0.0625f, 0.0000f),
	};

	const Vec2*	pSampleLocation = DE_NULL;

	switch (numSamples)
	{
		case VK_SAMPLE_COUNT_1_BIT:		pSampleLocation = s_location_samples_1;		break;
		case VK_SAMPLE_COUNT_2_BIT:		pSampleLocation = s_location_samples_2;		break;
		case VK_SAMPLE_COUNT_4_BIT:		pSampleLocation = s_location_samples_4;		break;
		case VK_SAMPLE_COUNT_8_BIT:		pSampleLocation = s_location_samples_8;		break;
		case VK_SAMPLE_COUNT_16_BIT:	pSampleLocation = s_location_samples_16;	break;

		default:
			DE_ASSERT(0);
			return std::vector<Vec2>();
	}

	return ndcTransformEachSampleInPixel(framebufferSize, static_cast<deUint32>(numSamples), AccessStandardSampleLocationsArray(pSampleLocation));
}

//! Generate NDC space custom sample locations at each framebuffer pixel, based on the given pixel grid
std::vector<Vec2> getSampleLocations (const MultisamplePixelGrid& pixelGrid, const UVec2& framebufferSize)
{
	return ndcTransformEachSampleInPixel(framebufferSize, pixelGrid.samplesPerPixel(), AccessMultisamplePixelGrid(&pixelGrid));
}

struct PositionColor
{
	tcu::Vec4	position;
	tcu::Vec4	color;

	PositionColor (const tcu::Vec4& pos, const tcu::Vec4& col) : position(pos), color(col) {}
};

//! Generate subpixel triangles containing the sample position, based on compare data.
//! Stencil values are created by overlapping triangles, so the stencil pipeline state must be set up accordingly.
std::vector<PositionColor> generateSubpixelTriangles (const UVec2&						renderSize,
													  const std::vector<CompareData>&	compareData,
													  const std::vector<Vec2>&			sampleLocations)
{
	std::vector<PositionColor>	vertices;

	// For each sample location (in the whole framebuffer), create a sub-pixel triangle that contains it.
	// NDC viewport size is 2.0 in X and Y and NDC pixel width/height depends on the framebuffer resolution.
	const Vec2			pixelSize	= Vec2(2.0f) / renderSize.cast<float>();
	const Vec2			offset		= pixelSize / 16.0f;	// 4 bits precision

	// Surround with a roughly centered triangle
	const float y1 = 0.5f  * offset.y();
	const float y2 = 0.35f * offset.y();
	const float x1 = 0.5f  * offset.x();

	DE_ASSERT(compareData.size() == sampleLocations.size());

	for (std::size_t globalSampleNdx = 0; globalSampleNdx < sampleLocations.size(); ++globalSampleNdx)
	{
		const Vec2&			loc	= sampleLocations[globalSampleNdx];
		const CompareData&	cd	= compareData	 [globalSampleNdx];

		// Overdraw at the same position to get the desired stencil
		// Draw at least once, if stencil is 0
		for (deUint32 i = 0; i < deMaxu32(1u, cd.stencil); ++i)
		{
			vertices.push_back(PositionColor(Vec4(loc.x(),      loc.y() - y1, cd.depth, 1.0f), cd.color));
			vertices.push_back(PositionColor(Vec4(loc.x() - x1, loc.y() + y2, cd.depth, 1.0f), cd.color));
			vertices.push_back(PositionColor(Vec4(loc.x() + x1, loc.y() + y2, cd.depth, 1.0f), cd.color));
		}
	}

	return vertices;
}

void reportSampleError (tcu::TestLog& log, const std::string& sampleDesc, UVec2& renderSize, const deUint32 numCoverageSamples, const deUint32 globalSampleNdx)
{
	const deUint32 pixelNdx	= globalSampleNdx / numCoverageSamples;
	const deUint32 x		= pixelNdx % renderSize.x();
	const deUint32 y		= pixelNdx / renderSize.x();
	const deUint32 sample	= globalSampleNdx % numCoverageSamples;

	log << tcu::TestLog::Message << "Incorrect " << sampleDesc << " sample (" << sample << ") at pixel (" << x << ", " << y << ")" << tcu::TestLog::EndMessage;
}

void checkSampleRequirements (Context&						context,
							  const VkSampleCountFlagBits	numColorSamples,
							  const VkSampleCountFlagBits	numDepthStencilSamples,
							  const bool					requireStandardSampleLocations)
{
	const VkPhysicalDeviceLimits& limits = context.getDeviceProperties().limits;

	if ((limits.framebufferColorSampleCounts & numColorSamples) == 0u)
		TCU_THROW(NotSupportedError, "framebufferColorSampleCounts: sample count not supported");

	if ((limits.framebufferDepthSampleCounts & numDepthStencilSamples) == 0u)
		TCU_THROW(NotSupportedError, "framebufferDepthSampleCounts: sample count not supported");

	if ((limits.framebufferStencilSampleCounts & numDepthStencilSamples) == 0u)
		TCU_THROW(NotSupportedError, "framebufferStencilSampleCounts: sample count not supported");

	if ((limits.sampledImageColorSampleCounts & numColorSamples) == 0u)
		TCU_THROW(NotSupportedError, "sampledImageColorSampleCounts: sample count not supported");

	if ((limits.sampledImageDepthSampleCounts & numDepthStencilSamples) == 0u)
		TCU_THROW(NotSupportedError, "sampledImageDepthSampleCounts: sample count not supported");

	if ((limits.sampledImageStencilSampleCounts & numDepthStencilSamples) == 0u)
		TCU_THROW(NotSupportedError, "sampledImageStencilSampleCounts: sample count not supported");

	// This is required to output geometry that is covering a specific sample
	if (requireStandardSampleLocations && !limits.standardSampleLocations)
		TCU_THROW(NotSupportedError, "standardSampleLocations: not supported");
}

void checkImageRequirements (Context&						context,
							 const VkFormat					format,
							 const VkFormatFeatureFlags		requiredFeatureFlags,
							 const VkImageUsageFlags		requiredUsageFlags,
							 const VkSampleCountFlagBits	requiredSampleCount = VK_SAMPLE_COUNT_1_BIT)
{
	const InstanceInterface&	vki				= context.getInstanceInterface();
	const VkPhysicalDevice		physicalDevice	= context.getPhysicalDevice();
	VkImageFormatProperties		imageProperties;

	const VkFormatProperties formatProperties = getPhysicalDeviceFormatProperties(vki, physicalDevice, format);

	if ((formatProperties.optimalTilingFeatures & requiredFeatureFlags) != requiredFeatureFlags)
		TCU_THROW(NotSupportedError, (de::toString(format) + ": format features not supported").c_str());

	const VkResult result = vki.getPhysicalDeviceImageFormatProperties(physicalDevice, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, requiredUsageFlags, (VkImageCreateFlags)0, &imageProperties);

	if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
		TCU_THROW(NotSupportedError, (de::toString(format) + ": format not supported").c_str());

	if ((imageProperties.sampleCounts & requiredSampleCount) != requiredSampleCount)
		TCU_THROW(NotSupportedError, (de::toString(format) + ": sample count not supported").c_str());
}

//! Used after a render pass color output (draw or resolve)
void recordCopyOutputImageToBuffer (const DeviceInterface&	vk,
									const VkCommandBuffer	cmdBuffer,
									const UVec2&			imageSize,
									const VkImage			srcImage,
									const VkBuffer			dstBuffer)
{
	// Image read barrier after color output
	{
		const VkImageMemoryBarrier barrier =
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,									// VkStructureType            sType;
			DE_NULL,																// const void*                pNext;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,									// VkAccessFlags              srcAccessMask;
			VK_ACCESS_TRANSFER_READ_BIT,											// VkAccessFlags              dstAccessMask;
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,									// VkImageLayout              oldLayout;
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,									// VkImageLayout              newLayout;
			VK_QUEUE_FAMILY_IGNORED,												// uint32_t                   srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,												// uint32_t                   dstQueueFamilyIndex;
			srcImage,																// VkImage                    image;
			makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u),	// VkImageSubresourceRange    subresourceRange;
		};

		vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0u, DE_NULL, 0u, DE_NULL, 1u, &barrier);
	}
	// Resolve image -> host buffer
	{
		const VkBufferImageCopy region =
		{
			0ull,																// VkDeviceSize                bufferOffset;
			0u,																	// uint32_t                    bufferRowLength;
			0u,																	// uint32_t                    bufferImageHeight;
			makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u),	// VkImageSubresourceLayers    imageSubresource;
			makeOffset3D(0, 0, 0),												// VkOffset3D                  imageOffset;
			makeExtent3D(imageSize.x(), imageSize.y(), 1u),						// VkExtent3D                  imageExtent;
		};

		vk.cmdCopyImageToBuffer(cmdBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstBuffer, 1u, &region);
	}
	// Buffer write barrier
	{
		const VkBufferMemoryBarrier barrier =
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,		// VkStructureType    sType;
			DE_NULL,										// const void*        pNext;
			VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags      srcAccessMask;
			VK_ACCESS_HOST_READ_BIT,						// VkAccessFlags      dstAccessMask;
			VK_QUEUE_FAMILY_IGNORED,						// uint32_t           srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,						// uint32_t           dstQueueFamilyIndex;
			dstBuffer,										// VkBuffer           buffer;
			0ull,											// VkDeviceSize       offset;
			VK_WHOLE_SIZE,									// VkDeviceSize       size;
		};

		vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0,
							  0u, DE_NULL, 1u, &barrier, DE_NULL, 0u);
	}
}

namespace VerifySamples
{

//! The parameters that define a test case
struct TestParams
{
	struct SampleCount
	{
		VkSampleCountFlagBits	numCoverageSamples;				//!< VkPipelineMultisampleStateCreateInfo::rasterizationSamples
		VkSampleCountFlagBits	numColorSamples;				//!< VkAttachmentDescription::samples and VkImageCreateInfo::samples
		VkSampleCountFlagBits	numDepthStencilSamples;			//!< VkAttachmentDescription::samples and VkImageCreateInfo::samples
	};

	VkFormat					colorFormat;					//!< Color attachment format
	VkFormat					depthStencilFormat;				//!< D/S attachment format. Will test both aspects if it's a mixed format
	bool						useProgrammableSampleLocations;	//!< Try to use VK_EXT_sample_locations if available
	bool						useFragmentShadingRate;			//!< Try to use VK_KHR_fragment_shading_rate if available
	std::vector<SampleCount>	perSubpassSamples;				//!< Will use multiple subpasses if more than one element

	TestParams (void)
		: colorFormat						()
		, depthStencilFormat				()
		, useProgrammableSampleLocations	()
		, useFragmentShadingRate			()
	{
	}
};

//! Common data used by the test
struct WorkingData
{
	struct PerSubpass
	{
		deUint32						numVertices;				//!< Number of vertices defined in the vertex buffer
		Move<VkBuffer>					vertexBuffer;
		MovePtr<Allocation>				vertexBufferAlloc;
		Move<VkImage>					colorImage;					//!< Color image
		Move<VkImageView>				colorImageView;				//!< Color attachment
		MovePtr<Allocation>				colorImageAlloc;
		Move<VkImage>					depthStencilImage;			//!< Depth stencil image
		Move<VkImageView>				depthStencilImageView;		//!< Depth stencil attachment
		Move<VkImageView>				depthOnlyImageView;			//!< Depth aspect for shader read
		Move<VkImageView>				stencilOnlyImageView;		//!< Stencil aspect for shader read
		MovePtr<Allocation>				depthStencilImageAlloc;
		Move<VkBuffer>					compareBuffer;				//!< Buffer used to verify the images - comparison data
		MovePtr<Allocation>				compareBufferAlloc;
		VkDeviceSize					compareBufferSize;
		Move<VkBuffer>					resultBuffer;				//!< Buffer used to verify the images - results
		MovePtr<Allocation>				resultBufferAlloc;
		VkDeviceSize					resultBufferSize;
		deUint32						numResultElements;			//!< Number of checksums in the result buffer
		MovePtr<MultisamplePixelGrid>	pixelGrid;					//!< Programmable locations

		PerSubpass (void)
			: numVertices		()
			, compareBufferSize	()
			, resultBufferSize	()
			, numResultElements	()
		{
		}
	};

	UVec2											renderSize;					//!< Size of the framebuffer
	VkPhysicalDeviceSampleLocationsPropertiesEXT	sampleLocationsProperties;	//!< Used with VK_EXT_sample_locations

	std::vector<de::SharedPtr<PerSubpass> >			perSubpass;					//!< Test may use more than one set of data

	WorkingData (void)
		: sampleLocationsProperties ()
	{
	}
};

void addVerificationComputeShader (SourceCollections&			programCollection,
								   const VkSampleCountFlagBits	numCoverageSamples,
								   const VkSampleCountFlagBits	numColorSamples,
								   const VkSampleCountFlagBits	numDepthStencilSamples,
								   const VkFormat				depthStencilFormat,
								   const std::string&			nameSuffix)
{
		const bool			isColorMS			= (numColorSamples		  != VK_SAMPLE_COUNT_1_BIT);
		const bool			isDepthStencilMS	= (numDepthStencilSamples != VK_SAMPLE_COUNT_1_BIT);
		const std::string	colorBit			= de::toString(static_cast<deUint32>(VK_IMAGE_ASPECT_COLOR_BIT)) + "u";
		const std::string	depthBit			= de::toString(static_cast<deUint32>(VK_IMAGE_ASPECT_DEPTH_BIT)) + "u";
		const std::string	stencilBit			= de::toString(static_cast<deUint32>(VK_IMAGE_ASPECT_STENCIL_BIT)) + "u";

		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "struct CompareData {\n"
			<< "    vec4  color;\n"
			<< "    float depth;\n"
			<< "    uint  stencil;\n"
			<< "};\n"
			<< "\n"
			<< "layout(local_size_x = " << static_cast<deUint32>(numCoverageSamples) << ") in;\n"
			// Always use this descriptor layout and ignore unused bindings
			<< "layout(set = 0, binding = 0, std430) writeonly buffer Output {\n"
			<< "    uint values[];\n"
			<< "} sb_out;\n"
			<< "layout(set = 0, binding = 1, std430) readonly buffer InputCompare {\n"
			<< "    CompareData	data[];\n"
			<< "} sb_cmp;\n"
			<< "layout(set = 0, binding = 2) uniform sampler2D" << (isColorMS ? "MS" : "") << "  colorImage;\n"
			<< "layout(set = 0, binding = 3) uniform sampler2D" << (isDepthStencilMS ? "MS" : "") <<"  depthImage;\n"
			<< "layout(set = 0, binding = 4) uniform usampler2D" << (isDepthStencilMS ? "MS" : "") <<" stencilImage;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"

		// Data for each sample in each pixel is laid out linearly (e.g 2 samples):
		// [pixel(0, 0) sample(0)][pixel(0, 0) sample(1)][pixel(1, 0) sample(0)][pixel(1, 0) sample(1)]...

			<< "    uint  globalIndex = gl_LocalInvocationID.x + gl_WorkGroupSize.x * (gl_WorkGroupID.x + gl_WorkGroupID.y * gl_NumWorkGroups.x);\n"
			<< "    ivec2 position    = ivec2(gl_WorkGroupID.x, gl_WorkGroupID.y);\n"
			<< "    int   sampleNdx   = int(gl_LocalInvocationID.x);\n"
			<< "    uint  result      = 0u;\n"
			<< "\n"
			<< "    // Verify color samples\n"
			<< "    if (sampleNdx < " << static_cast<deUint32>(numColorSamples) << ")\n"
			<< "    {\n"
			<< "        vec4 color     = texelFetch(colorImage, position, sampleNdx);\n"	// for non-MS (1 sample) case, sampleNdx = 0 and will instead be LOD = 0
			<< "        vec4 diff      = abs(color - sb_cmp.data[globalIndex].color);\n"
			<< "        vec4 threshold = vec4(0.02);\n"
			<< "\n"
			<< "        if (all(lessThan(diff, threshold)))\n"
			<< "            result |= " << colorBit << ";\n"
			<< "    }\n"
			<< "    else\n"
			<< "        result |= " << colorBit << ";\n"	// Pass, if sample doesn't exist
			<< "\n";

		if (isDepthFormat(depthStencilFormat))
		{
			src << "    // Verify depth samples\n"
				<< "    if (sampleNdx < " << static_cast<deUint32>(numDepthStencilSamples) << ")\n"
				<< "    {\n"
				<< "        float depth     = texelFetch(depthImage, position, sampleNdx).r;\n"
				<< "        float diff      = abs(depth - sb_cmp.data[globalIndex].depth);\n"
				<< "        float threshold = 0.002;\n"
				<< "\n"
				<< "        if (diff < threshold)\n"
				<< "            result |= " << depthBit << ";\n"
				<< "    }\n"
				<< "    else\n"
				<< "        result |= " << depthBit << ";\n"
				<< "\n";
		}

		if (isStencilFormat(depthStencilFormat))
		{
			src << "    // Verify stencil samples\n"
				<< "    if (sampleNdx < " << static_cast<deUint32>(numDepthStencilSamples) << ")\n"
				<< "    {\n"
				<< "        uint stencil   = texelFetch(stencilImage, position, sampleNdx).r;\n"
				<< "        uint diff      = stencil - sb_cmp.data[globalIndex].stencil;\n"
				<< "\n"
				<< "        if (diff == 0u)\n"
				<< "            result |= " << stencilBit << ";\n"
				<< "    }\n"
				<< "    else\n"
				<< "        result |= " << stencilBit << ";\n"
				<< "\n";
		}

		src << "    sb_out.values[globalIndex] = result;\n"
			<< "}\n";
		programCollection.glslSources.add("comp" + nameSuffix) << glu::ComputeSource(src.str());
}

//! Get a compact sample count string in format X_Y_Z
std::string getSampleCountString (const TestParams::SampleCount& samples)
{
	std::ostringstream str;

	str << static_cast<deUint32>(samples.numCoverageSamples) << "_"
		<< static_cast<deUint32>(samples.numColorSamples)	 << "_"
		<< static_cast<deUint32>(samples.numDepthStencilSamples);

	return str.str();
}

void initPrograms (SourceCollections& programCollection, const TestParams params)
{
	// Vertex shader - position and color
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_position;\n"
			<< "layout(location = 1) in  vec4 in_color;\n"
			<< "layout(location = 0) out vec4 o_color;\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "    vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    gl_Position = in_position;\n"
			<< "    o_color     = in_color;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment shader - output color from VS
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_color;\n"
			<< "layout(location = 0) out vec4 o_color;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    o_color = in_color;\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}

	// Compute shader - image verification
	for (deUint32 subpassNdx = 0; subpassNdx < static_cast<deUint32>(params.perSubpassSamples.size()); ++subpassNdx)
	{
		const TestParams::SampleCount&	samples	= params.perSubpassSamples[subpassNdx];
		addVerificationComputeShader(programCollection,
									 samples.numCoverageSamples,
									 samples.numColorSamples,
									 samples.numDepthStencilSamples,
									 params.depthStencilFormat,
									 "_" + getSampleCountString(samples));
	}
}

//! A simple color, depth/stencil draw. Subpasses (if more than one) are independent
void draw (Context& context, const TestParams& params, WorkingData& wd)
{
	const DeviceInterface&	vk				= context.getDeviceInterface();
	const VkDevice			device			= context.getDevice();
	const deUint32			numSubpasses	= static_cast<deUint32>(wd.perSubpass.size());

	Move<VkRenderPass>							renderPass;
	Move<VkFramebuffer>							framebuffer;
	std::vector<VkSampleLocationsInfoEXT>		perSubpassSampleLocationsInfo;
	std::vector<VkAttachmentSampleLocationsEXT>	attachmentSampleLocations;
	std::vector<VkSubpassSampleLocationsEXT>	subpassSampleLocations;

	if (params.useProgrammableSampleLocations)
	for (deUint32 subpassNdx = 0; subpassNdx < numSubpasses; ++subpassNdx)
	{
		perSubpassSampleLocationsInfo.push_back(makeSampleLocationsInfo(*wd.perSubpass[subpassNdx]->pixelGrid));
	}

	// Create a render pass and a framebuffer
	{
		std::vector<VkSubpassDescription>		subpasses;
		std::vector<VkImageView>				attachments;
		std::vector<VkAttachmentDescription>	attachmentDescriptions;
		std::vector<VkAttachmentReference>		attachmentReferences;

		// Reserve capacity to avoid invalidating pointers to elements
		attachmentReferences.reserve(numSubpasses * 2);

		for (deUint32 subpassNdx = 0; subpassNdx < numSubpasses; ++subpassNdx)
		{
			attachments.push_back(wd.perSubpass[subpassNdx]->colorImageView.get());
			attachments.push_back(wd.perSubpass[subpassNdx]->depthStencilImageView.get());

			attachmentDescriptions.push_back(makeAttachmentDescription(
				(VkAttachmentDescriptionFlags)0,								// VkAttachmentDescriptionFlags		flags;
				params.colorFormat,												// VkFormat							format;
				params.perSubpassSamples[subpassNdx].numColorSamples,			// VkSampleCountFlagBits			samples;
				VK_ATTACHMENT_LOAD_OP_CLEAR,									// VkAttachmentLoadOp				loadOp;
				VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp				storeOp;
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,								// VkAttachmentLoadOp				stencilLoadOp;
				VK_ATTACHMENT_STORE_OP_DONT_CARE,								// VkAttachmentStoreOp				stencilStoreOp;
				VK_IMAGE_LAYOUT_UNDEFINED,										// VkImageLayout					initialLayout;
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL						// VkImageLayout					finalLayout;
			));

			attachmentDescriptions.push_back(makeAttachmentDescription(
				(VkAttachmentDescriptionFlags)0,								// VkAttachmentDescriptionFlags		flags;
				params.depthStencilFormat,										// VkFormat							format;
				params.perSubpassSamples[subpassNdx].numDepthStencilSamples,	// VkSampleCountFlagBits			samples;
				VK_ATTACHMENT_LOAD_OP_CLEAR,									// VkAttachmentLoadOp				loadOp;
				VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp				storeOp;
				VK_ATTACHMENT_LOAD_OP_CLEAR,									// VkAttachmentLoadOp				stencilLoadOp;
				VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp				stencilStoreOp;
				VK_IMAGE_LAYOUT_UNDEFINED,										// VkImageLayout					initialLayout;
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL					// VkImageLayout					finalLayout;
			));

			attachmentReferences.push_back(makeAttachmentReference(static_cast<deUint32>(attachmentReferences.size()),	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
			const VkAttachmentReference* colorRef = &attachmentReferences.back();

			attachmentReferences.push_back(makeAttachmentReference(static_cast<deUint32>(attachmentReferences.size()),	VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));
			const VkAttachmentReference* depthStencilRef = &attachmentReferences.back();

			if (params.useProgrammableSampleLocations)
			{
				const VkAttachmentSampleLocationsEXT newAttachmentSampleLocations =
				{
					attachmentReferences.back().attachment,			// uint32_t                    attachmentIndex;
					perSubpassSampleLocationsInfo[subpassNdx],		// VkSampleLocationsInfoEXT    sampleLocationsInfo;
				};
				attachmentSampleLocations.push_back(newAttachmentSampleLocations);

				const VkSubpassSampleLocationsEXT newSubpassSampleLocations =
				{
					subpassNdx,										// uint32_t                    subpassIndex;
					perSubpassSampleLocationsInfo[subpassNdx],		// VkSampleLocationsInfoEXT    sampleLocationsInfo;
				};
				subpassSampleLocations.push_back(newSubpassSampleLocations);
			}

			const VkSubpassDescription subpassDescription =
			{
				(VkSubpassDescriptionFlags)0,						// VkSubpassDescriptionFlags       flags;
				VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint             pipelineBindPoint;
				0u,													// uint32_t                        inputAttachmentCount;
				DE_NULL,											// const VkAttachmentReference*    pInputAttachments;
				1u,													// uint32_t                        colorAttachmentCount;
				colorRef,											// const VkAttachmentReference*    pColorAttachments;
				DE_NULL,											// const VkAttachmentReference*    pResolveAttachments;
				depthStencilRef,									// const VkAttachmentReference*    pDepthStencilAttachment;
				0u,													// uint32_t                        preserveAttachmentCount;
				DE_NULL,											// const uint32_t*                 pPreserveAttachments;
			};

			subpasses.push_back(subpassDescription);
		}

		// Assume there are no dependencies between subpasses
		const VkRenderPassCreateInfo renderPassInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,				// VkStructureType					sType;
			DE_NULL,												// const void*						pNext;
			(VkRenderPassCreateFlags)0,								// VkRenderPassCreateFlags			flags;
			static_cast<deUint32>(attachmentDescriptions.size()),	// deUint32							attachmentCount;
			dataOrNullPtr(attachmentDescriptions),					// const VkAttachmentDescription*	pAttachments;
			static_cast<deUint32>(subpasses.size()),				// deUint32							subpassCount;
			dataOrNullPtr(subpasses),								// const VkSubpassDescription*		pSubpasses;
			0u,														// deUint32							dependencyCount;
			DE_NULL,												// const VkSubpassDependency*		pDependencies;
		};

		renderPass  = createRenderPass(vk, device, &renderPassInfo);
		framebuffer = makeFramebuffer (vk, device, *renderPass, static_cast<deUint32>(attachments.size()), dataOrNullPtr(attachments), wd.renderSize.x(), wd.renderSize.y());
	}

	const Unique<VkShaderModule>	vertexModule	(createShaderModule(vk, device, context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>	fragmentModule	(createShaderModule(vk, device, context.getBinaryCollection().get("frag"), 0u));
	const Unique<VkPipelineLayout>	pipelineLayout	(makePipelineLayout(vk, device));

	typedef SharedPtr<Unique<VkPipeline> > PipelineSp;
	std::vector<PipelineSp> pipelines;

	for (deUint32 subpassNdx = 0; subpassNdx < numSubpasses; ++subpassNdx)
	{
		const VkSampleLocationsInfoEXT* pSampleLocationsInfo = (params.useProgrammableSampleLocations ? &perSubpassSampleLocationsInfo[subpassNdx] : DE_NULL);

		pipelines.push_back(PipelineSp(new Unique<VkPipeline>(
			makeGraphicsPipeline(vk, device, *pipelineLayout, *renderPass, *vertexModule, *fragmentModule, /*use vertex input*/ true, subpassNdx,
								 wd.renderSize, getImageAspectFlags(params.depthStencilFormat), params.perSubpassSamples[subpassNdx].numCoverageSamples,
								 /*use sample shading*/ true, params.useFragmentShadingRate, pSampleLocationsInfo))));
	}

	const Unique<VkCommandPool>		cmdPool		(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, context.getUniversalQueueFamilyIndex()));
	const Unique<VkCommandBuffer>	cmdBuffer	(makeCommandBuffer(vk, device, *cmdPool));

	beginCommandBuffer(vk, *cmdBuffer);

	{
		std::vector<VkClearValue> clearValues;

		for (deUint32 subpassNdx = 0; subpassNdx < numSubpasses; ++subpassNdx)
		{
			clearValues.push_back(makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f));
			clearValues.push_back(makeClearValueDepthStencil(1.0f, 0u));
		}

		const VkRect2D renderArea =
		{
			{ 0u, 0u },
			{ wd.renderSize.x(), wd.renderSize.y() }
		};

		VkRenderPassBeginInfo renderPassBeginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,			// VkStructureType         sType;
			DE_NULL,											// const void*             pNext;
			*renderPass,										// VkRenderPass            renderPass;
			*framebuffer,										// VkFramebuffer           framebuffer;
			renderArea,											// VkRect2D                renderArea;
			static_cast<deUint32>(clearValues.size()),			// uint32_t                clearValueCount;
			dataOrNullPtr(clearValues),							// const VkClearValue*     pClearValues;
		};

		if (params.useProgrammableSampleLocations)
		{
			const VkRenderPassSampleLocationsBeginInfoEXT renderPassSampleLocationsBeginInfo =
			{
				VK_STRUCTURE_TYPE_RENDER_PASS_SAMPLE_LOCATIONS_BEGIN_INFO_EXT,	// VkStructureType                          sType;
				DE_NULL,														// const void*                              pNext;
				static_cast<deUint32>(attachmentSampleLocations.size()),		// uint32_t                                 attachmentInitialSampleLocationsCount;
				dataOrNullPtr(attachmentSampleLocations),						// const VkAttachmentSampleLocationsEXT*    pAttachmentInitialSampleLocations;
				static_cast<deUint32>(subpassSampleLocations.size()),			// uint32_t                                 postSubpassSampleLocationsCount;
				dataOrNullPtr(subpassSampleLocations),							// const VkSubpassSampleLocationsEXT*       pPostSubpassSampleLocations;
			};

			renderPassBeginInfo.pNext = &renderPassSampleLocationsBeginInfo;

			vk.cmdBeginRenderPass(*cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		}
		else
			vk.cmdBeginRenderPass(*cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	for (deUint32 subpassNdx = 0; subpassNdx < numSubpasses; ++subpassNdx)
	{
		if (subpassNdx != 0)
			vk.cmdNextSubpass(*cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);

		const VkDeviceSize vertexBufferOffset = 0ull;
		vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &wd.perSubpass[subpassNdx]->vertexBuffer.get(), &vertexBufferOffset);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **pipelines[subpassNdx]);

		vk.cmdDraw(*cmdBuffer, wd.perSubpass[subpassNdx]->numVertices, 1u, 0u, 0u);
	}

	vk.cmdEndRenderPass(*cmdBuffer);

	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));
	submitCommandsAndWait(vk, device, context.getUniversalQueue(), *cmdBuffer);
}

void dispatchImageCheck (Context& context, const TestParams& params, WorkingData& wd, const deUint32 subpassNdx)
{
	const DeviceInterface&		vk			= context.getDeviceInterface();
	const VkDevice				device		= context.getDevice();
	WorkingData::PerSubpass&	subpassData	= *wd.perSubpass[subpassNdx];

	const Unique<VkSampler>	defaultSampler	(makeSampler(vk, device));

	// Create descriptor set

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding		(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,			VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding		(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,			VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	VK_SHADER_STAGE_COMPUTE_BIT, &defaultSampler.get())
		.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	VK_SHADER_STAGE_COMPUTE_BIT, &defaultSampler.get())
		.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	VK_SHADER_STAGE_COMPUTE_BIT, &defaultSampler.get())
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u)
		.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3u)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	{
		const VkDescriptorBufferInfo	compareBufferInfo	= makeDescriptorBufferInfo(*subpassData.compareBuffer, 0ull, subpassData.compareBufferSize);
		const VkDescriptorBufferInfo	resultBufferInfo	= makeDescriptorBufferInfo(*subpassData.resultBuffer, 0ull, subpassData.resultBufferSize);
		const VkDescriptorImageInfo		colorImageInfo		= makeDescriptorImageInfo(DE_NULL, *subpassData.colorImageView,			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		const VkDescriptorImageInfo		depthImageInfo		= makeDescriptorImageInfo(DE_NULL, *subpassData.depthOnlyImageView,		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		const VkDescriptorImageInfo		stencilImageInfo	= makeDescriptorImageInfo(DE_NULL, *subpassData.stencilOnlyImageView,	VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		DescriptorSetUpdateBuilder	builder;

		builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultBufferInfo);
		builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &compareBufferInfo);
		builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &colorImageInfo);

		if (subpassData.depthOnlyImageView)
			builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(3u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &depthImageInfo);

		if (subpassData.stencilOnlyImageView)
			builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(4u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &stencilImageInfo);

		builder.update(vk, device);
	}

	// Pipeline

	const std::string				shaderName		("comp_" + getSampleCountString(params.perSubpassSamples[subpassNdx]));
	const Unique<VkShaderModule>	shaderModule	(createShaderModule(vk, device, context.getBinaryCollection().get(shaderName), 0u));
	const Unique<VkPipelineLayout>	pipelineLayout	(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline>		pipeline		(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule, DE_NULL));

	const Unique<VkCommandPool>		cmdPool		(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, context.getUniversalQueueFamilyIndex()));
	const Unique<VkCommandBuffer>	cmdBuffer	(makeCommandBuffer(vk, device, *cmdPool));

	beginCommandBuffer(vk, *cmdBuffer);

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

	vk.cmdDispatch(*cmdBuffer, wd.renderSize.x(), wd.renderSize.y(), 1u);

	{
		const VkBufferMemoryBarrier barrier =
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,		// VkStructureType    sType;
			DE_NULL,										// const void*        pNext;
			VK_ACCESS_SHADER_WRITE_BIT,						// VkAccessFlags      srcAccessMask;
			VK_ACCESS_HOST_READ_BIT,						// VkAccessFlags      dstAccessMask;
			VK_QUEUE_FAMILY_IGNORED,						// uint32_t           srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,						// uint32_t           dstQueueFamilyIndex;
			*subpassData.resultBuffer,						// VkBuffer           buffer;
			0ull,											// VkDeviceSize       offset;
			VK_WHOLE_SIZE,									// VkDeviceSize       size;
		};

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0,
			(const VkMemoryBarrier*)DE_NULL, 1u, &barrier, 0u, (const VkImageMemoryBarrier*)DE_NULL);
	}

	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));
	submitCommandsAndWait(vk, device, context.getUniversalQueue(), *cmdBuffer);

	invalidateMappedMemoryRange(vk, device, subpassData.resultBufferAlloc->getMemory(), subpassData.resultBufferAlloc->getOffset(), VK_WHOLE_SIZE);
}

void createPerSubpassData (Context& context, const TestParams& params, WorkingData& wd, const deUint32 subpassNdx)
{
	const DeviceInterface&			vk			= context.getDeviceInterface();
	const VkDevice					device		= context.getDevice();
	MovePtr<Allocator>				allocator   = MovePtr<Allocator>(new SimpleAllocator(vk, device, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice())));
	const TestParams::SampleCount&	samples		= params.perSubpassSamples[subpassNdx];
	WorkingData::PerSubpass&		subpassData	= *wd.perSubpass[subpassNdx];

	// Create images
	{

		const VkImageUsageFlags	colorImageUsageFlags		= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT		  | VK_IMAGE_USAGE_SAMPLED_BIT;
		const VkImageUsageFlags depthStencilImageUsageFlags	= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		checkImageRequirements (context,
								params.colorFormat,
								VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,
								colorImageUsageFlags,
								samples.numColorSamples);

		subpassData.colorImage		= makeImage(vk, device, params.colorFormat, wd.renderSize, samples.numColorSamples, colorImageUsageFlags);
		subpassData.colorImageAlloc	= bindImage(vk, device, *allocator, *subpassData.colorImage, MemoryRequirement::Any);
		subpassData.colorImageView	= makeImageView(vk, device, *subpassData.colorImage, VK_IMAGE_VIEW_TYPE_2D, params.colorFormat, makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));

		checkImageRequirements (context,
								params.depthStencilFormat,
								VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,
								depthStencilImageUsageFlags,
								samples.numDepthStencilSamples);

		subpassData.depthStencilImage		= makeImage(vk, device, params.depthStencilFormat, wd.renderSize, samples.numDepthStencilSamples, depthStencilImageUsageFlags);
		subpassData.depthStencilImageAlloc	= bindImage(vk, device, *allocator, *subpassData.depthStencilImage, MemoryRequirement::Any);
		subpassData.depthStencilImageView	= makeImageView(vk, device, *subpassData.depthStencilImage, VK_IMAGE_VIEW_TYPE_2D, params.depthStencilFormat, makeImageSubresourceRange(getImageAspectFlags(params.depthStencilFormat), 0u, 1u, 0u, 1u));

		if (isDepthFormat(params.depthStencilFormat))
			subpassData.depthOnlyImageView	= makeImageView(vk, device, *subpassData.depthStencilImage, VK_IMAGE_VIEW_TYPE_2D, params.depthStencilFormat, makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u));

		if (isStencilFormat(params.depthStencilFormat))
			subpassData.stencilOnlyImageView	= makeImageView(vk, device, *subpassData.depthStencilImage, VK_IMAGE_VIEW_TYPE_2D, params.depthStencilFormat, makeImageSubresourceRange(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 1u, 0u, 1u));
	}

	// Create vertex and comparison buffers
	{
		const deUint32					seed		= 123 + 19 * subpassNdx;
		const std::vector<CompareData>	compareData	= generateCompareData(seed, wd.renderSize, samples.numCoverageSamples, samples.numColorSamples, samples.numDepthStencilSamples);

		subpassData.compareBufferSize	= static_cast<VkDeviceSize>(sizeof(CompareData) * compareData.size());
		subpassData.compareBuffer		= makeBuffer(vk, device, subpassData.compareBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		subpassData.compareBufferAlloc	= bindBuffer(vk, device, *allocator, *subpassData.compareBuffer, MemoryRequirement::HostVisible);

		deMemcpy(subpassData.compareBufferAlloc->getHostPtr(), dataOrNullPtr(compareData), static_cast<std::size_t>(subpassData.compareBufferSize));
		flushMappedMemoryRange(vk, device, subpassData.compareBufferAlloc->getMemory(), subpassData.compareBufferAlloc->getOffset(), VK_WHOLE_SIZE);

		subpassData.numResultElements	= static_cast<deUint32>(compareData.size());
		subpassData.resultBufferSize	= static_cast<VkDeviceSize>(sizeof(deUint32) * compareData.size());
		subpassData.resultBuffer		= makeBuffer(vk, device, subpassData.resultBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		subpassData.resultBufferAlloc	= bindBuffer(vk, device, *allocator, *subpassData.resultBuffer, MemoryRequirement::HostVisible);

		deMemset(subpassData.resultBufferAlloc->getHostPtr(), 0, static_cast<std::size_t>(subpassData.resultBufferSize));
		flushMappedMemoryRange(vk, device, subpassData.resultBufferAlloc->getMemory(), subpassData.resultBufferAlloc->getOffset(), VK_WHOLE_SIZE);

		std::vector<PositionColor> vertices;

		if (params.useProgrammableSampleLocations)
		{
			subpassData.pixelGrid = MovePtr<MultisamplePixelGrid>(new MultisamplePixelGrid(UVec2(wd.sampleLocationsProperties.maxSampleLocationGridSize.width,
																								 wd.sampleLocationsProperties.maxSampleLocationGridSize.height),
																						   samples.numCoverageSamples));

			const deUint32 locationsSeed = 211 + 4 * subpassNdx;
			fillSampleLocationsRandom(*subpassData.pixelGrid, wd.sampleLocationsProperties.sampleLocationSubPixelBits, locationsSeed);
			vertices = generateSubpixelTriangles(wd.renderSize, compareData, getSampleLocations(*subpassData.pixelGrid, wd.renderSize));
		}
		else
		{
			const std::vector<Vec2>	locations = genFramebufferStandardSampleLocations(samples.numCoverageSamples, wd.renderSize);
			vertices = generateSubpixelTriangles(wd.renderSize, compareData, locations);
		}

		const VkDeviceSize	vertexBufferSize = static_cast<VkDeviceSize>(sizeof(vertices[0]) * vertices.size());
		subpassData.numVertices			= static_cast<deUint32>(vertices.size());
		subpassData.vertexBuffer			= makeBuffer(vk, device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		subpassData.vertexBufferAlloc	= bindBuffer(vk, device, *allocator, *subpassData.vertexBuffer, MemoryRequirement::HostVisible);

		deMemcpy(subpassData.vertexBufferAlloc->getHostPtr(), dataOrNullPtr(vertices), static_cast<std::size_t>(vertexBufferSize));
		flushMappedMemoryRange(vk, device, subpassData.vertexBufferAlloc->getMemory(), subpassData.vertexBufferAlloc->getOffset(), VK_WHOLE_SIZE);
	}
}

void checkRequirements (Context& context, TestParams params)
{
	context.requireDeviceFunctionality("VK_AMD_mixed_attachment_samples");

	if (params.useProgrammableSampleLocations)
		context.requireDeviceFunctionality("VK_EXT_sample_locations");

	for (deUint32 subpassNdx = 0; subpassNdx < static_cast<deUint32>(params.perSubpassSamples.size()); ++subpassNdx)
	{
		const TestParams::SampleCount& samples = params.perSubpassSamples[subpassNdx];
		checkSampleRequirements(context, samples.numColorSamples, samples.numDepthStencilSamples, !params.useProgrammableSampleLocations);
	}

	if (params.useFragmentShadingRate)
	{
		const auto&	vki				= context.getInstanceInterface();
		const auto	physicalDevice	= context.getPhysicalDevice();

		context.requireDeviceFunctionality("VK_KHR_fragment_shading_rate");

		if (!context.getFragmentShadingRateFeatures().pipelineFragmentShadingRate)
			TCU_THROW(NotSupportedError, "pipelineFragmentShadingRate not supported");

		// Fetch information about supported fragment shading rates
		deUint32 supportedFragmentShadingRateCount = 0;
		vki.getPhysicalDeviceFragmentShadingRatesKHR(physicalDevice, &supportedFragmentShadingRateCount, DE_NULL);

		std::vector<vk::VkPhysicalDeviceFragmentShadingRateKHR> supportedFragmentShadingRates(supportedFragmentShadingRateCount,
			{
				vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR,
				DE_NULL,
				vk::VK_SAMPLE_COUNT_1_BIT,
				{1, 1}
			});
		vki.getPhysicalDeviceFragmentShadingRatesKHR(physicalDevice, &supportedFragmentShadingRateCount, supportedFragmentShadingRates.data());

		deUint32 cumulativeNeededSamples = 0;
		for (const TestParams::SampleCount& samples : params.perSubpassSamples)
			cumulativeNeededSamples |= samples.numColorSamples;

		bool requiredRateFound = false;
		for (const auto& rate : supportedFragmentShadingRates)
		{
			if ((rate.fragmentSize.width == 2u) &&
				(rate.fragmentSize.height == 2u) &&
				(rate.sampleCounts & cumulativeNeededSamples))
			{
				requiredRateFound = true;
				break;
			}
		}

		if (!requiredRateFound)
			TCU_THROW(NotSupportedError, "Required FragmentShadingRate not supported");
	}
}

//! Verify the values of all samples in all attachments.
tcu::TestStatus test (Context& context, const TestParams params)
{
	WorkingData wd;
	wd.renderSize	= UVec2(2, 2);	// Use a very small image, as we will verify all samples for all pixels

	// Query state related to programmable sample locations
	if (params.useProgrammableSampleLocations)
	{
		const InstanceInterface&	vki				= context.getInstanceInterface();
		const VkPhysicalDevice		physicalDevice	= context.getPhysicalDevice();

		wd.sampleLocationsProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT;
		wd.sampleLocationsProperties.pNext = DE_NULL;

		VkPhysicalDeviceProperties2 properties =
		{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,	    // VkStructureType               sType;
			&wd.sampleLocationsProperties,							// void*                         pNext;
			VkPhysicalDeviceProperties(),							// VkPhysicalDeviceProperties    properties;
		};

		vki.getPhysicalDeviceProperties2(physicalDevice, &properties);

		for (deUint32 subpassNdx = 0; subpassNdx < static_cast<deUint32>(params.perSubpassSamples.size()); ++subpassNdx)
		{
			if ((wd.sampleLocationsProperties.sampleLocationSampleCounts & params.perSubpassSamples[subpassNdx].numCoverageSamples) == 0u)
				TCU_THROW(NotSupportedError, "VkSampleLocationsPropertiesAMD: sample count not supported");
		}
	}

	// Create subpass data
	for (deUint32 subpassNdx = 0; subpassNdx < static_cast<deUint32>(params.perSubpassSamples.size()); ++subpassNdx)
	{
		wd.perSubpass.push_back(SharedPtr<WorkingData::PerSubpass>(new WorkingData::PerSubpass()));
		createPerSubpassData(context, params, wd, subpassNdx);
	}

	// Draw test geometry
	draw (context, params, wd);

	// Verify images with a compute shader
	for (deUint32 subpassNdx = 0; subpassNdx < static_cast<deUint32>(params.perSubpassSamples.size()); ++subpassNdx)
		dispatchImageCheck (context, params, wd, subpassNdx);

	// Test checksums
	for (deUint32 subpassNdx = 0; subpassNdx < static_cast<deUint32>(params.perSubpassSamples.size()); ++subpassNdx)
	{
		const deUint32*	const	pSampleChecksumBase	= static_cast<deUint32*>(wd.perSubpass[subpassNdx]->resultBufferAlloc->getHostPtr());
		const bool				hasDepth			= isDepthFormat(params.depthStencilFormat);
		const bool				hasStencil			= isStencilFormat(params.depthStencilFormat);
		bool					allOk				= true;

		context.getTestContext().getLog() << tcu::TestLog::Message << "Verify images in subpass " << subpassNdx << tcu::TestLog::EndMessage;

		for (deUint32 globalSampleNdx = 0; globalSampleNdx < wd.perSubpass[subpassNdx]->numResultElements; ++globalSampleNdx)
		{
			const TestParams::SampleCount&	samples	 = params.perSubpassSamples[subpassNdx];
			const deUint32					checksum = pSampleChecksumBase[globalSampleNdx];

			if ((checksum & VK_IMAGE_ASPECT_COLOR_BIT) == 0u)
			{
				reportSampleError(context.getTestContext().getLog(), "color", wd.renderSize, samples.numCoverageSamples, globalSampleNdx);
				allOk = false;
			}

			if (hasDepth && ((checksum & VK_IMAGE_ASPECT_DEPTH_BIT) == 0u))
			{
				reportSampleError(context.getTestContext().getLog(), "depth", wd.renderSize, samples.numCoverageSamples, globalSampleNdx);
				allOk = false;
			}

			if (hasStencil && ((checksum & VK_IMAGE_ASPECT_STENCIL_BIT) == 0u))
			{
				reportSampleError(context.getTestContext().getLog(), "stencil", wd.renderSize, samples.numCoverageSamples, globalSampleNdx);
				allOk = false;
			}
		}

		if (!allOk)
			return tcu::TestStatus::fail("Multisampled image has incorrect samples");
	}

	return tcu::TestStatus::pass("Pass");
}

} // VerifySamples

namespace ShaderBuiltins
{

struct TestParams
{
	VkSampleCountFlagBits		numCoverageSamples;			//!< VkPipelineMultisampleStateCreateInfo::rasterizationSamples
	VkSampleCountFlagBits		numColorSamples;			//!< VkAttachmentDescription::samples and VkImageCreateInfo::samples
	VkSampleCountFlagBits		numDepthStencilSamples;		//!< VkAttachmentDescription::samples and VkImageCreateInfo::samples
	VkFormat					colorFormat;				//!< Color attachment format
	VkFormat					depthStencilFormat;			//!< D/S attachment format. Will test both aspects if it's a mixed format
};

struct WorkingData
{
	UVec2						renderSize;					//!< Size of the framebuffer
	deUint32					numVertices;				//!< Number of vertices defined in the vertex buffer
	Move<VkBuffer>				vertexBuffer;
	MovePtr<Allocation>			vertexBufferAlloc;
	Move<VkImage>				colorImage;					//!< Color image
	Move<VkImageView>			colorImageView;				//!< Color attachment
	MovePtr<Allocation>			colorImageAlloc;
	Move<VkImage>				depthStencilImage;			//!< Depth stencil image
	Move<VkImageView>			depthStencilImageView;		//!< Depth stencil attachment
	Move<VkImageView>			depthOnlyImageView;			//!< Depth aspect for shader read
	Move<VkImageView>			stencilOnlyImageView;		//!< Stencil aspect for shader read
	MovePtr<Allocation>			depthStencilImageAlloc;
	Move<VkImage>				resolveImage;				//!< Resolve image
	Move<VkImageView>			resolveImageView;			//!< Resolve attachment
	MovePtr<Allocation>			resolveImageAlloc;
	Move<VkBuffer>				colorBuffer;				//!< Buffer used to copy resolve output
	MovePtr<Allocation>			colorBufferAlloc;
	VkDeviceSize				colorBufferSize;

	WorkingData (void)
		: numVertices		()
		, colorBufferSize	(0)
	{
	}
};

void initPrograms (SourceCollections& programCollection, const TestParams params)
{
	// Vertex shader - no vertex data
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "    vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			// Specify an oversized triangle covering the whole viewport.
			<< "    switch (gl_VertexIndex)\n"
			<< "    {\n"
			<< "        case 0:\n"
			<< "            gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
			<< "            break;\n"
			<< "        case 1:\n"
			<< "            gl_Position = vec4(-1.0,  3.0, 0.0, 1.0);\n"
			<< "            break;\n"
			<< "        case 2:\n"
			<< "            gl_Position = vec4( 3.0, -1.0, 0.0, 1.0);\n"
			<< "            break;\n"
			<< "    }\n"
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
			<< "    vec4 col = vec4(0.0, 0.0, 0.0, 1.0);\n"
			<< "\n";

		if (params.numColorSamples == VK_SAMPLE_COUNT_1_BIT)
		{
			const deUint32 expectedMask = ((1u << static_cast<deUint32>(params.numCoverageSamples)) - 1u);

			// Expect all covered samples to be lit, the rest is zero
			src << "    if (gl_SampleMaskIn[0] == " << expectedMask << ")\n"
				<< "        col.g = 1.0;\n"
				<< "    else\n"
				<< "        col.r = 1.0;\n";
		}
		else
		{
			// Expect only a matching sample to be lit
			src << "    if (gl_SampleMaskIn[0] == (1 << gl_SampleID))\n"
				<< "        col.g = 1.0;\n"
				<< "    else\n"
				<< "        col.r = 1.0;\n"
				<< "\n"
				<< "    if (gl_SampleID >= " << static_cast<deUint32>(params.numColorSamples) << ")  // number of color samples, should not happen\n"
				<< "        col.b = 1.0;\n";
		}

		src << "\n"
			<< "    o_color = col;\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

//! A simple color, depth/stencil draw. Single subpass, no vertex input
void drawResolve (Context& context, const TestParams& params, WorkingData& wd)
{
	const DeviceInterface&	vk			= context.getDeviceInterface();
	const VkDevice			device		= context.getDevice();
	const bool				needResolve	= (params.numColorSamples != VK_SAMPLE_COUNT_1_BIT);

	Move<VkRenderPass>		renderPass;
	Move<VkFramebuffer>		framebuffer;

	// Create a render pass and a framebuffer
	{
		std::vector<VkImageView>				attachments;
		std::vector<VkAttachmentDescription>	attachmentDescriptions;

		attachments.push_back(*wd.colorImageView);
		attachments.push_back(*wd.depthStencilImageView);

		attachmentDescriptions.push_back(makeAttachmentDescription(
			(VkAttachmentDescriptionFlags)0,						// VkAttachmentDescriptionFlags		flags;
			params.colorFormat,										// VkFormat							format;
			params.numColorSamples,									// VkSampleCountFlagBits			samples;
			VK_ATTACHMENT_LOAD_OP_CLEAR,							// VkAttachmentLoadOp				loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,							// VkAttachmentStoreOp				storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,						// VkAttachmentLoadOp				stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,						// VkAttachmentStoreOp				stencilStoreOp;
			VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout					initialLayout;
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL					// VkImageLayout					finalLayout;
		));

		attachmentDescriptions.push_back(makeAttachmentDescription(
			(VkAttachmentDescriptionFlags)0,						// VkAttachmentDescriptionFlags		flags;
			params.depthStencilFormat,								// VkFormat							format;
			params.numDepthStencilSamples,							// VkSampleCountFlagBits			samples;
			VK_ATTACHMENT_LOAD_OP_CLEAR,							// VkAttachmentLoadOp				loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,							// VkAttachmentStoreOp				storeOp;
			VK_ATTACHMENT_LOAD_OP_CLEAR,							// VkAttachmentLoadOp				stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_STORE,							// VkAttachmentStoreOp				stencilStoreOp;
			VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout					initialLayout;
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL		// VkImageLayout					finalLayout;
		));

		if (needResolve)
		{
			attachments.push_back(*wd.resolveImageView);

			attachmentDescriptions.push_back(makeAttachmentDescription(
				(VkAttachmentDescriptionFlags)0,					// VkAttachmentDescriptionFlags		flags;
				params.colorFormat,									// VkFormat							format;
				VK_SAMPLE_COUNT_1_BIT,								// VkSampleCountFlagBits			samples;
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp				loadOp;
				VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				storeOp;
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp				stencilLoadOp;
				VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp				stencilStoreOp;
				VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout					initialLayout;
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL				// VkImageLayout					finalLayout;
			));
		}

		const VkAttachmentReference	colorRef		= makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		const VkAttachmentReference	depthStencilRef	= makeAttachmentReference(1u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		const VkAttachmentReference	resolveRef		= makeAttachmentReference(2u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		const VkSubpassDescription subpassDescription =
		{
			(VkSubpassDescriptionFlags)0,							// VkSubpassDescriptionFlags       flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,						// VkPipelineBindPoint             pipelineBindPoint;
			0u,														// uint32_t                        inputAttachmentCount;
			DE_NULL,												// const VkAttachmentReference*    pInputAttachments;
			1u,														// uint32_t                        colorAttachmentCount;
			&colorRef,												// const VkAttachmentReference*    pColorAttachments;
			(needResolve ? &resolveRef : DE_NULL),					// const VkAttachmentReference*    pResolveAttachments;
			&depthStencilRef,										// const VkAttachmentReference*    pDepthStencilAttachment;
			0u,														// uint32_t                        preserveAttachmentCount;
			DE_NULL,												// const uint32_t*                 pPreserveAttachments;
		};

		// Assume there are no dependencies between subpasses
		VkRenderPassCreateInfo renderPassInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,				// VkStructureType					sType;
			DE_NULL,												// const void*						pNext;
			(VkRenderPassCreateFlags)0,								// VkRenderPassCreateFlags			flags;
			static_cast<deUint32>(attachmentDescriptions.size()),	// deUint32							attachmentCount;
			dataOrNullPtr(attachmentDescriptions),					// const VkAttachmentDescription*	pAttachments;
			1u,														// deUint32							subpassCount;
			&subpassDescription,									// const VkSubpassDescription*		pSubpasses;
			0u,														// deUint32							dependencyCount;
			DE_NULL,												// const VkSubpassDependency*		pDependencies;
		};

		renderPass  = createRenderPass(vk, device, &renderPassInfo);
		framebuffer = makeFramebuffer (vk, device, *renderPass, static_cast<deUint32>(attachments.size()), dataOrNullPtr(attachments), wd.renderSize.x(), wd.renderSize.y());
	}

	const Unique<VkShaderModule>	vertexModule	(createShaderModule(vk, device, context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>	fragmentModule	(createShaderModule(vk, device, context.getBinaryCollection().get("frag"), 0u));
	const Unique<VkPipelineLayout>	pipelineLayout	(makePipelineLayout(vk, device));
	const bool						useVertexInput	= false;
	const bool						sampleShading	= (params.numColorSamples != VK_SAMPLE_COUNT_1_BIT);
	const deUint32					subpassNdx		= 0u;
	const Unique<VkPipeline>		pipeline		(makeGraphicsPipeline(vk, device, *pipelineLayout, *renderPass, *vertexModule, *fragmentModule, useVertexInput,
																		  subpassNdx, wd.renderSize, getImageAspectFlags(params.depthStencilFormat),
																		  params.numCoverageSamples, sampleShading, false));

	const Unique<VkCommandPool>		cmdPool		(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, context.getUniversalQueueFamilyIndex()));
	const Unique<VkCommandBuffer>	cmdBuffer	(makeCommandBuffer(vk, device, *cmdPool));

	beginCommandBuffer(vk, *cmdBuffer);

	{
		std::vector<VkClearValue> clearValues;
		clearValues.push_back(makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f));
		clearValues.push_back(makeClearValueDepthStencil(1.0f, 0u));

		const VkRect2D renderArea =
		{
			{ 0u, 0u },
			{ wd.renderSize.x(), wd.renderSize.y() }
		};

		const VkRenderPassBeginInfo renderPassBeginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,			// VkStructureType         sType;
			DE_NULL,											// const void*             pNext;
			*renderPass,										// VkRenderPass            renderPass;
			*framebuffer,										// VkFramebuffer           framebuffer;
			renderArea,											// VkRect2D                renderArea;
			static_cast<deUint32>(clearValues.size()),			// uint32_t                clearValueCount;
			dataOrNullPtr(clearValues),							// const VkClearValue*     pClearValues;
		};
		vk.cmdBeginRenderPass(*cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
	vk.cmdDraw(*cmdBuffer, 3u, 1u, 0u, 0u);

	vk.cmdEndRenderPass(*cmdBuffer);

	if (needResolve)
		recordCopyOutputImageToBuffer(vk, *cmdBuffer, wd.renderSize, *wd.resolveImage, *wd.colorBuffer);
	else
		recordCopyOutputImageToBuffer(vk, *cmdBuffer, wd.renderSize, *wd.colorImage, *wd.colorBuffer);

	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));
	submitCommandsAndWait(vk, device, context.getUniversalQueue(), *cmdBuffer);
}

void checkRequirements (Context& context, TestParams params)
{
	context.requireDeviceFunctionality("VK_AMD_mixed_attachment_samples");

	checkSampleRequirements(context, params.numColorSamples, params.numDepthStencilSamples, false /* require standard sample locations */);
}

//! Verify the values of shader builtins
tcu::TestStatus test (Context& context, const TestParams params)
{
	WorkingData				wd;
	const DeviceInterface&	vk			= context.getDeviceInterface();
	const VkDevice			device		= context.getDevice();
	MovePtr<Allocator>		allocator	= MovePtr<Allocator>(new SimpleAllocator(vk, device, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice())));

	wd.renderSize	= UVec2(16, 16);

	// Create images and a color buffer
	{

		const VkImageUsageFlags	colorImageUsageFlags		= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		const VkImageUsageFlags depthStencilImageUsageFlags	= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		checkImageRequirements (context,
								params.colorFormat,
								VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT,
								colorImageUsageFlags,
								params.numColorSamples);

		wd.colorImage		= makeImage(vk, device, params.colorFormat, wd.renderSize, params.numColorSamples, colorImageUsageFlags);
		wd.colorImageAlloc	= bindImage(vk, device, *allocator, *wd.colorImage, MemoryRequirement::Any);
		wd.colorImageView	= makeImageView(vk, device, *wd.colorImage, VK_IMAGE_VIEW_TYPE_2D, params.colorFormat, makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));

		if (params.numColorSamples != VK_SAMPLE_COUNT_1_BIT)
		{
			wd.resolveImage			= makeImage(vk, device, params.colorFormat, wd.renderSize, VK_SAMPLE_COUNT_1_BIT, colorImageUsageFlags);
			wd.resolveImageAlloc	= bindImage(vk, device, *allocator, *wd.resolveImage, MemoryRequirement::Any);
			wd.resolveImageView		= makeImageView(vk, device, *wd.resolveImage, VK_IMAGE_VIEW_TYPE_2D, params.colorFormat, makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));
		}

		// Resolve result
		wd.colorBufferSize	= static_cast<VkDeviceSize>(tcu::getPixelSize(mapVkFormat(params.colorFormat)) * wd.renderSize.x() * wd.renderSize.y());
		wd.colorBuffer		= makeBuffer(vk, device, wd.colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		wd.colorBufferAlloc	= bindBuffer(vk, device, *allocator, *wd.colorBuffer, MemoryRequirement::HostVisible);

		deMemset(wd.colorBufferAlloc->getHostPtr(), 0, static_cast<std::size_t>(wd.colorBufferSize));
		flushMappedMemoryRange(vk, device, wd.colorBufferAlloc->getMemory(), wd.colorBufferAlloc->getOffset(), VK_WHOLE_SIZE);

		checkImageRequirements (context,
								params.depthStencilFormat,
								VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
								depthStencilImageUsageFlags,
								params.numDepthStencilSamples);

		wd.depthStencilImage		= makeImage(vk, device, params.depthStencilFormat, wd.renderSize, params.numDepthStencilSamples, depthStencilImageUsageFlags);
		wd.depthStencilImageAlloc	= bindImage(vk, device, *allocator, *wd.depthStencilImage, MemoryRequirement::Any);
		wd.depthStencilImageView	= makeImageView(vk, device, *wd.depthStencilImage, VK_IMAGE_VIEW_TYPE_2D, params.depthStencilFormat, makeImageSubresourceRange(getImageAspectFlags(params.depthStencilFormat), 0u, 1u, 0u, 1u));

		if (isDepthFormat(params.depthStencilFormat))
			wd.depthOnlyImageView	= makeImageView(vk, device, *wd.depthStencilImage, VK_IMAGE_VIEW_TYPE_2D, params.depthStencilFormat, makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u));

		if (isStencilFormat(params.depthStencilFormat))
			wd.stencilOnlyImageView	= makeImageView(vk, device, *wd.depthStencilImage, VK_IMAGE_VIEW_TYPE_2D, params.depthStencilFormat, makeImageSubresourceRange(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 1u, 0u, 1u));
	}

	// Draw, resolve, and copy to color buffer (see the fragment shader for details)
	drawResolve(context, params, wd);

	// Verify resolved image
	{
		const tcu::ConstPixelBufferAccess image (tcu::ConstPixelBufferAccess(mapVkFormat(params.colorFormat), tcu::IVec3(wd.renderSize.x(), wd.renderSize.y(), 1),wd.colorBufferAlloc->getHostPtr()));

		if (compareGreenImage(context.getTestContext().getLog(), "resolve0", "Resolved test image", image))
			return tcu::TestStatus::pass("Pass");
		else
			return tcu::TestStatus::fail("Some samples were incorrect");
	}
}

} // ShaderBuiltins

std::string getSampleCountGroupName(const VkSampleCountFlagBits coverageCount,
									const VkSampleCountFlagBits colorCount,
									const VkSampleCountFlagBits depthStencilCount)
{
	std::ostringstream str;
	str << "coverage_"		 << static_cast<deUint32>(coverageCount)
		<< "_color_"		 << static_cast<deUint32>(colorCount)
		<< "_depth_stencil_" << static_cast<deUint32>(depthStencilCount);
	return str.str();
}

std::string getFormatShortString (const VkFormat format)
{
	std::string s(de::toLower(getFormatName(format)));
	return s.substr(10);
}

std::string getFormatCaseName (const VkFormat colorFormat,
							   const VkFormat depthStencilFormat)
{
	std::ostringstream str;
	str << getFormatShortString(colorFormat) << "_" << getFormatShortString(depthStencilFormat);
	return str.str();
}

void createMixedAttachmentSamplesTestsInGroup (tcu::TestCaseGroup* rootGroup, bool useFragmentShadingRate)
{
	const VkFormat colorFormatRange[] =
	{
		VK_FORMAT_R8G8B8A8_UNORM,
		// If you add more, make sure it is handled in the test/shader
	};

	const VkFormat depthStencilFormatRange[] =
	{
		VK_FORMAT_D16_UNORM,
		VK_FORMAT_X8_D24_UNORM_PACK32,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
	};

	// Minimal set of formats to cover depth and stencil
	const VkFormat depthStencilReducedFormatRange[] =
	{
		VK_FORMAT_D16_UNORM,				//!< Must be supported
		VK_FORMAT_D24_UNORM_S8_UINT,		//!< Either this, or the next one must be supported
		VK_FORMAT_D32_SFLOAT_S8_UINT,
	};

	struct SampleCase
	{
		VkSampleCountFlagBits	colorSamples;
		VkSampleCountFlagBits	depthStencilSamples;
	};

	// Currently supported EQAA cases
	static const SampleCase	singlePassCases[] =
	{
		// Less color than depth/stencil
		{ VK_SAMPLE_COUNT_1_BIT,	VK_SAMPLE_COUNT_2_BIT  },
		{ VK_SAMPLE_COUNT_1_BIT,	VK_SAMPLE_COUNT_4_BIT  },
		{ VK_SAMPLE_COUNT_1_BIT,	VK_SAMPLE_COUNT_8_BIT  },
		{ VK_SAMPLE_COUNT_1_BIT,	VK_SAMPLE_COUNT_16_BIT },
		{ VK_SAMPLE_COUNT_2_BIT,	VK_SAMPLE_COUNT_4_BIT  },
		{ VK_SAMPLE_COUNT_2_BIT,	VK_SAMPLE_COUNT_8_BIT  },
		{ VK_SAMPLE_COUNT_2_BIT,	VK_SAMPLE_COUNT_16_BIT },
		{ VK_SAMPLE_COUNT_4_BIT,	VK_SAMPLE_COUNT_8_BIT  },
		{ VK_SAMPLE_COUNT_4_BIT,	VK_SAMPLE_COUNT_16_BIT },
		{ VK_SAMPLE_COUNT_8_BIT,	VK_SAMPLE_COUNT_16_BIT },
	};

	// Multi-subpass cases

	static const SampleCase caseSubpassIncreaseColor_1[] =
	{
		{ VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_4_BIT },
		{ VK_SAMPLE_COUNT_2_BIT, VK_SAMPLE_COUNT_4_BIT },
	};
	static const SampleCase caseSubpassIncreaseColor_2[] =
	{
		{ VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_8_BIT },
		{ VK_SAMPLE_COUNT_2_BIT, VK_SAMPLE_COUNT_8_BIT },
		{ VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_8_BIT },
	};
	static const SampleCase caseSubpassDecreaseColor_1[] =
	{
		{ VK_SAMPLE_COUNT_2_BIT, VK_SAMPLE_COUNT_4_BIT },
		{ VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_4_BIT },
	};
	static const SampleCase caseSubpassDecreaseColor_2[] =
	{
		{ VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_8_BIT },
		{ VK_SAMPLE_COUNT_2_BIT, VK_SAMPLE_COUNT_8_BIT },
		{ VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_8_BIT },
	};
	static const SampleCase caseSubpassIncreaseCoverage_1[] =
	{
		{ VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_2_BIT },
		{ VK_SAMPLE_COUNT_2_BIT, VK_SAMPLE_COUNT_4_BIT },
	};
	static const SampleCase caseSubpassIncreaseCoverage_2[] =
	{
		{ VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_2_BIT },
		{ VK_SAMPLE_COUNT_2_BIT, VK_SAMPLE_COUNT_4_BIT },
		{ VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_8_BIT },
	};
	static const SampleCase caseSubpassDecreaseCoverage_1[] =
	{
		{ VK_SAMPLE_COUNT_2_BIT, VK_SAMPLE_COUNT_4_BIT },
		{ VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_2_BIT },
	};
	static const SampleCase caseSubpassDecreaseCoverage_2[] =
	{
		{ VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_8_BIT },
		{ VK_SAMPLE_COUNT_2_BIT, VK_SAMPLE_COUNT_4_BIT },
		{ VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_2_BIT },
	};

	static const struct
	{
		const char* const	caseName;
		const deUint32		numSampleCases;
		const SampleCase*	pSampleCase;
	} subpassCases[] =
	{
		{ "multi_subpass_decrease_color_4",		DE_LENGTH_OF_ARRAY(caseSubpassDecreaseColor_1),		caseSubpassDecreaseColor_1 },
		{ "multi_subpass_decrease_color_8",		DE_LENGTH_OF_ARRAY(caseSubpassDecreaseColor_2),		caseSubpassDecreaseColor_2 },
		{ "multi_subpass_decrease_coverage_4",	DE_LENGTH_OF_ARRAY(caseSubpassDecreaseCoverage_1),	caseSubpassDecreaseCoverage_1 },
		{ "multi_subpass_decrease_coverage_8",	DE_LENGTH_OF_ARRAY(caseSubpassDecreaseCoverage_2),	caseSubpassDecreaseCoverage_2 },
		{ "multi_subpass_increase_color_4",		DE_LENGTH_OF_ARRAY(caseSubpassIncreaseColor_1),		caseSubpassIncreaseColor_1 },
		{ "multi_subpass_increase_color_8",		DE_LENGTH_OF_ARRAY(caseSubpassIncreaseColor_2),		caseSubpassIncreaseColor_2 },
		{ "multi_subpass_increase_coverage_4",	DE_LENGTH_OF_ARRAY(caseSubpassIncreaseCoverage_1),	caseSubpassIncreaseCoverage_1 },
		{ "multi_subpass_increase_coverage_8",	DE_LENGTH_OF_ARRAY(caseSubpassIncreaseCoverage_2),	caseSubpassIncreaseCoverage_2 },
	};

	// Test 1: Per-sample expected value check
	{
		MovePtr<tcu::TestCaseGroup> standardLocationsGroup		(new tcu::TestCaseGroup(rootGroup->getTestContext(), "verify_standard_locations", ""));
		MovePtr<tcu::TestCaseGroup> programmableLocationsGroup	(new tcu::TestCaseGroup(rootGroup->getTestContext(), "verify_programmable_locations", ""));

		tcu::TestCaseGroup* locationsGroups[2] =
		{
			standardLocationsGroup.get(),
			programmableLocationsGroup.get()
		};

		for (deUint32 groupNdx = 0u; groupNdx < DE_LENGTH_OF_ARRAY(locationsGroups); ++groupNdx)
		{
			// Single subpass cases
			for (deUint32 caseNdx = 0u; caseNdx < DE_LENGTH_OF_ARRAY(singlePassCases); ++caseNdx)
			{
				VerifySamples::TestParams::SampleCount	samples;
				samples.numColorSamples					= singlePassCases[caseNdx].colorSamples;
				samples.numDepthStencilSamples			= singlePassCases[caseNdx].depthStencilSamples;
				samples.numCoverageSamples				= de::max(samples.numColorSamples, samples.numDepthStencilSamples);

				VerifySamples::TestParams params;
				params.perSubpassSamples.push_back(samples);
				params.useProgrammableSampleLocations	= (locationsGroups[groupNdx] == programmableLocationsGroup.get());
				params.useFragmentShadingRate = useFragmentShadingRate;

				MovePtr<tcu::TestCaseGroup> sampleCaseGroup(new tcu::TestCaseGroup(
					rootGroup->getTestContext(), getSampleCountGroupName(samples.numCoverageSamples, samples.numColorSamples, samples.numDepthStencilSamples).c_str(), ""));

				for (const VkFormat *pDepthStencilFormat = depthStencilFormatRange; pDepthStencilFormat != DE_ARRAY_END(depthStencilFormatRange); ++pDepthStencilFormat)
				for (const VkFormat *pColorFormat		 = colorFormatRange;		pColorFormat		!= DE_ARRAY_END(colorFormatRange);		  ++pColorFormat)
				{
					params.colorFormat			= *pColorFormat;
					params.depthStencilFormat	= *pDepthStencilFormat;

					addFunctionCaseWithPrograms(
						sampleCaseGroup.get(),
						getFormatCaseName(params.colorFormat, params.depthStencilFormat).c_str(),
						"",
						VerifySamples::checkRequirements,
						VerifySamples::initPrograms,
						VerifySamples::test, params);
				}

				locationsGroups[groupNdx]->addChild(sampleCaseGroup.release());
			}

			// Multi subpass cases
			for (deUint32 caseNdx = 0u; caseNdx < DE_LENGTH_OF_ARRAY(subpassCases); ++caseNdx)
			{
				VerifySamples::TestParams params;
				params.useProgrammableSampleLocations = (locationsGroups[groupNdx] == programmableLocationsGroup.get());
				params.useFragmentShadingRate = useFragmentShadingRate;

				for (deUint32 subpassNdx = 0; subpassNdx < subpassCases[caseNdx].numSampleCases; ++subpassNdx)
				{
					VerifySamples::TestParams::SampleCount	samples;
					samples.numColorSamples					= subpassCases[caseNdx].pSampleCase[subpassNdx].colorSamples;
					samples.numDepthStencilSamples			= subpassCases[caseNdx].pSampleCase[subpassNdx].depthStencilSamples;
					samples.numCoverageSamples				= de::max(samples.numColorSamples, samples.numDepthStencilSamples);
					params.perSubpassSamples.push_back(samples);
				}

				MovePtr<tcu::TestCaseGroup> sampleCaseGroup(new tcu::TestCaseGroup(rootGroup->getTestContext(), subpassCases[caseNdx].caseName, ""));

				for (const VkFormat *pDepthStencilFormat = depthStencilReducedFormatRange;	pDepthStencilFormat != DE_ARRAY_END(depthStencilReducedFormatRange); ++pDepthStencilFormat)
				for (const VkFormat *pColorFormat		 = colorFormatRange;				pColorFormat		!= DE_ARRAY_END(colorFormatRange);				 ++pColorFormat)
				{
					params.colorFormat			= *pColorFormat;
					params.depthStencilFormat	= *pDepthStencilFormat;

					addFunctionCaseWithPrograms(
						sampleCaseGroup.get(),
						getFormatCaseName(params.colorFormat, params.depthStencilFormat).c_str(),
						"",
						VerifySamples::checkRequirements,
						VerifySamples::initPrograms,
						VerifySamples::test, params);
				}

				locationsGroups[groupNdx]->addChild(sampleCaseGroup.release());
			}
		}

		rootGroup->addChild(standardLocationsGroup.release());
		rootGroup->addChild(programmableLocationsGroup.release());
	}

	// Test 2: Shader built-ins check
	if (!useFragmentShadingRate)
	{
		MovePtr<tcu::TestCaseGroup> builtinsGroup (new tcu::TestCaseGroup(rootGroup->getTestContext(), "shader_builtins", ""));

		for (deUint32 caseNdx = 0u; caseNdx < DE_LENGTH_OF_ARRAY(singlePassCases); ++caseNdx)
		{
			ShaderBuiltins::TestParams params;
			params.numColorSamples			= singlePassCases[caseNdx].colorSamples;
			params.numDepthStencilSamples	= singlePassCases[caseNdx].depthStencilSamples;
			params.numCoverageSamples		= de::max(params.numColorSamples, params.numDepthStencilSamples);

			MovePtr<tcu::TestCaseGroup> sampleCaseGroup(new tcu::TestCaseGroup(
				rootGroup->getTestContext(), getSampleCountGroupName(params.numCoverageSamples, params.numColorSamples, params.numDepthStencilSamples).c_str(), ""));

			for (const VkFormat *pDepthStencilFormat = depthStencilReducedFormatRange;  pDepthStencilFormat != DE_ARRAY_END(depthStencilReducedFormatRange); ++pDepthStencilFormat)
			for (const VkFormat *pColorFormat		 = colorFormatRange;				pColorFormat		!= DE_ARRAY_END(colorFormatRange);				 ++pColorFormat)
			{
				params.colorFormat			= *pColorFormat;
				params.depthStencilFormat	= *pDepthStencilFormat;

				addFunctionCaseWithPrograms(
					sampleCaseGroup.get(),
					getFormatCaseName(params.colorFormat, params.depthStencilFormat).c_str(),
					"",
					ShaderBuiltins::checkRequirements,
					ShaderBuiltins::initPrograms,
					ShaderBuiltins::test,
					params);
			}

			builtinsGroup->addChild(sampleCaseGroup.release());
		}

		rootGroup->addChild(builtinsGroup.release());
	}
}

} // anonymous ns

tcu::TestCaseGroup* createMultisampleMixedAttachmentSamplesTests (tcu::TestContext& testCtx, bool useFragmentShadingRate)
{
	return createTestGroup(testCtx, "mixed_attachment_samples", "Test a graphics pipeline with varying sample count per color and depth/stencil attachments", createMixedAttachmentSamplesTestsInGroup, useFragmentShadingRate);
}

} // pipeline
} // vkt
