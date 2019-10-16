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
 * \brief Tests for VK_AMD_shader_fragment_mask
 *//*--------------------------------------------------------------------*/

#include "vktPipelineMultisampleShaderFragmentMaskTests.hpp"
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

#include "tcuVector.hpp"
#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"

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
using tcu::UVec4;
using tcu::Vec2;
using tcu::Vec4;

typedef SharedPtr<Unique<VkImageView> >		ImageViewSp;
typedef SharedPtr<Unique<VkPipeline> >		PipelineSp;

struct PositionColor
{
	tcu::Vec4	        position;
	VkClearColorValue	color;

	PositionColor (const tcu::Vec4& pos, const tcu::UVec4& col) : position(pos)
    {
        deMemcpy(color.uint32, col.getPtr(), sizeof(color.uint32));
    }

	PositionColor (const tcu::Vec4& pos, const tcu::Vec4&  col) : position(pos)
    {
        deMemcpy(color.float32, col.getPtr(), sizeof(color.float32));
    }

	PositionColor (const PositionColor& rhs)
		: position	(rhs.position)
        , color     (rhs.color)
	{
	}
};

//! Make a dummy sampler.
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
						 const deUint32					layers,
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
		layers,											// deUint32					arrayLayers;
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

//! Create a test-specific MSAA pipeline
Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&					vk,
									   const VkDevice							device,
									   const VkPipelineLayout					pipelineLayout,
									   const VkRenderPass						renderPass,
									   const VkShaderModule						vertexModule,
									   const VkShaderModule						fragmentModule,
									   const bool								useVertexInput,
									   const VkFormat							vertexAttribColorFormat,
									   const bool								useColorAttachment,
									   const deUint32							subpassNdx,
									   const UVec2&								renderSize,
									   const VkSampleCountFlagBits				numSamples)
{
	std::vector<VkVertexInputBindingDescription>	vertexInputBindingDescriptions;
	std::vector<VkVertexInputAttributeDescription>	vertexInputAttributeDescriptions;

	// Vertex attributes: position and color
	if (useVertexInput)
	{
		vertexInputBindingDescriptions.push_back  (makeVertexInputBindingDescription  (0u, sizeof(PositionColor), VK_VERTEX_INPUT_RATE_VERTEX));
		vertexInputAttributeDescriptions.push_back(makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u));
		vertexInputAttributeDescriptions.push_back(makeVertexInputAttributeDescription(1u, 0u, vertexAttribColorFormat, sizeof(Vec4)));
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

	const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		(VkPipelineMultisampleStateCreateFlags)0,					// VkPipelineMultisampleStateCreateFlags	flags;
		numSamples,													// VkSampleCountFlagBits					rasterizationSamples;
		VK_FALSE,													// VkBool32									sampleShadingEnable;
		1.0f,														// float									minSampleShading;
		DE_NULL,													// const VkSampleMask*						pSampleMask;
		VK_FALSE,													// VkBool32									alphaToCoverageEnable;
		VK_FALSE													// VkBool32									alphaToOneEnable;
	};

	VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		(VkPipelineDepthStencilStateCreateFlags)0,					// VkPipelineDepthStencilStateCreateFlags	flags;
		VK_FALSE,													// VkBool32									depthTestEnable;
		VK_TRUE,													// VkBool32									depthWriteEnable;
		VK_COMPARE_OP_ALWAYS,										// VkCompareOp								depthCompareOp;
		VK_FALSE,													// VkBool32									depthBoundsTestEnable;
		VK_FALSE,													// VkBool32									stencilTestEnable;
		VkStencilOpState(),											// VkStencilOpState							front;
		VkStencilOpState(),											// VkStencilOpState							back;
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
		(useColorAttachment ? 1u : 0u),								// deUint32										attachmentCount;
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

	const VkGraphicsPipelineCreateInfo	graphicsPipelineInfo =
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
		subpassNdx,											// deUint32											subpass;
		DE_NULL,											// VkPipeline										basePipelineHandle;
		-1,													// deInt32											basePipelineIndex;
	};

	return createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineInfo);
}

std::vector<PositionColor> genShapes (const VkFormat colorFormat)
{
	std::vector<PositionColor> vertices;

	if (colorFormat == VK_FORMAT_R8G8B8A8_UNORM)
	{
		vertices.push_back(PositionColor(Vec4( 0.0f,  -0.75f, 0.0f, 1.0f), Vec4(0.5f, 0.5f, 0.5f, 1.0f)));
		vertices.push_back(PositionColor(Vec4(-0.75f,  0.75f, 0.0f, 1.0f), Vec4(1.0f, 0.5f, 0.5f, 1.0f)));
		vertices.push_back(PositionColor(Vec4( 0.75f,  0.65f, 0.0f, 1.0f), Vec4(0.0f, 0.5f, 1.0f, 1.0f)));
	}
	else
	{
		vertices.push_back(PositionColor(Vec4( 0.0f,  -0.75f, 0.0f, 1.0f), UVec4(0xabcdu, 0u, 0u, 0u)));
		vertices.push_back(PositionColor(Vec4(-0.75f,  0.75f, 0.0f, 1.0f), UVec4(0xbcdeu, 0u, 0u, 0u)));
		vertices.push_back(PositionColor(Vec4( 0.75f,  0.65f, 0.0f, 1.0f), UVec4(0xcdefu, 0u, 0u, 0u)));
	}

	return vertices;
}

//! Map color image format to a convenient format used in vertex attributes
VkFormat getVertexInputColorFormat (const VkFormat colorImageFormat)
{
	switch (tcu::getTextureChannelClass(mapVkFormat(colorImageFormat).type))
	{
		case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
		case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
			return VK_FORMAT_R32G32B32A32_SFLOAT;

		case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
			return VK_FORMAT_R32G32B32A32_SINT;

		case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
			return VK_FORMAT_R32G32B32A32_UINT;

		default:
			DE_ASSERT(0);
			return VK_FORMAT_UNDEFINED;
	}
}

enum SampleSource
{
	SAMPLE_SOURCE_IMAGE,			//!< texel fetch from an image
	SAMPLE_SOURCE_SUBPASS_INPUT,	//!< texel fetch from an input attachment
};

//! The parameters that define a test case
struct TestParams
{
	UVec2					renderSize;
	deUint32				numLayers;			//!< 1 or N for layered image
	SampleSource			sampleSource;		//!< source of texel fetch
	VkSampleCountFlagBits	numColorSamples;
	VkFormat				colorFormat;		//!< Color attachment format

	TestParams (void)
		: numLayers			()
		, numColorSamples	()
		, colorFormat		()
	{
	}
};

void checkRequirements (Context& context, TestParams params)
{
	context.requireDeviceFunctionality("VK_AMD_shader_fragment_mask");

	// In the subpass input case we have to store fetch results into a buffer for subsequent verification in a compute shader.
	const bool requireFragmentStores = (params.sampleSource == SAMPLE_SOURCE_SUBPASS_INPUT);

	const VkPhysicalDeviceLimits& limits = context.getDeviceProperties().limits;

	if ((limits.framebufferColorSampleCounts & params.numColorSamples) == 0u)
		TCU_THROW(NotSupportedError, "framebufferColorSampleCounts: sample count not supported");

	if ((isIntFormat(params.colorFormat) || isUintFormat(params.colorFormat)))
	{
		if ((limits.sampledImageIntegerSampleCounts & params.numColorSamples) == 0u)
			TCU_THROW(NotSupportedError, "sampledImageIntegerSampleCounts: sample count not supported");
	}
	else
	{
		if ((limits.sampledImageColorSampleCounts & params.numColorSamples) == 0u)
			TCU_THROW(NotSupportedError, "sampledImageColorSampleCounts: sample count not supported");
	}

	if (requireFragmentStores)
	{
		if (!context.getDeviceFeatures().fragmentStoresAndAtomics)
			TCU_THROW(NotSupportedError, "fragmentStoresAndAtomics: feature not supported");
	}
}

//! Common data used by the test
struct WorkingData
{
	deUint32						numVertices;				//!< Number of vertices defined in the vertex buffer
	Move<VkBuffer>					vertexBuffer;
	MovePtr<Allocation>				vertexBufferAlloc;
	Move<VkImage>					colorImage;					//!< Color image
	MovePtr<Allocation>				colorImageAlloc;
	Move<VkImageView>				colorImageView;				//!< Color image view spanning all layers
	Move<VkBuffer>					colorBuffer;				//!< Buffer used to copy image data
	MovePtr<Allocation>				colorBufferAlloc;
	VkDeviceSize					colorBufferSize;
	Move<VkSampler>					defaultSampler;				//!< Dummy sampler, we are using texel fetches

	WorkingData (void)
		: numVertices		()
		, colorBufferSize	()
	{
	}
};

void initPrograms (SourceCollections& programCollection, const TestParams params)
{
	std::string	colorType;					//!< color pixel type used by image functions
	std::string	colorBufferType;			//!< packed pixel type as stored in a ssbo
	std::string colorBufferPack;			//!< a cast or a function call when writing back color format to the ssbo
	std::string	colorFragInQualifier;		//!< fragment shader color input qualifier
	std::string samplerPrefix;				//!< u, i, or empty

	switch (params.colorFormat)
	{
		case VK_FORMAT_R8G8B8A8_UNORM:
			colorType				= "vec4";
			colorBufferType			= "uint";
			colorBufferPack			= "packUnorm4x8";
			break;

		case VK_FORMAT_R32_UINT:
			colorType				= "uint";
			colorBufferType			= "uint";
			colorBufferPack			= colorBufferType;
			colorFragInQualifier	= "flat";
			samplerPrefix			= "u";
			break;

		case VK_FORMAT_R32_SINT:
			colorType				= "int";
			colorBufferType			= "int";
			colorBufferPack			= colorBufferType;
			colorFragInQualifier	= "flat";
			samplerPrefix			= "i";
			break;

		default:
			DE_FATAL("initPrograms not handled for this color format");
			break;
	}

	// Vertex shader - position and color
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_position;\n"
			<< "layout(location = 1) in  " << colorType << " in_color;\n"
			<< "layout(location = 0) out " << colorType << " o_color;\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "    vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			// Introduce a variance in geometry per instance index which maps to the image layer
			<< "    float a   = 0.25 * float(gl_InstanceIndex);\n"
			<< "    mat3 rm   = mat3( cos(a), sin(a), 0.0,\n"
			<< "                     -sin(a), cos(a), 0.0,\n"
			<< "                         0.0,    0.0, 1.0);\n"
			<< "    vec2 rpos = (rm * vec3(in_position.xy, 1.0)).xy;\n"
			<< "\n"
			<< "    gl_Position = vec4(rpos, in_position.zw);\n"
			<< "    o_color     = in_color;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Vertex shader - no vertex data, fill viewport with one primitive
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

		programCollection.glslSources.add("vert_full") << glu::VertexSource(src.str());
	}

	// Fragment shader - output color from VS
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in " << colorFragInQualifier << " " << colorType << " in_color;\n"
			<< "layout(location = 0) out " << colorType << " o_color;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    o_color = in_color;\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}

	// Fragment shader - FMASK fetch from an input attachment
	if (params.sampleSource == SAMPLE_SOURCE_SUBPASS_INPUT)
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "#extension GL_AMD_shader_fragment_mask : enable\n"
			<< "\n"
			<< "layout(set = 0, binding = 0) uniform " << samplerPrefix << "sampler2DMS" << (params.numLayers > 1 ? "Array" : "") << " u_image;\n"
			<< "layout(set = 0, binding = 1, std430) writeonly buffer ColorOutput {\n"
			<< "    " << colorBufferType << " color[];\n"
			<< "} sb_out;\n"
			<< "layout(input_attachment_index = " << params.numLayers << ", set = 0, binding = 2) uniform " << samplerPrefix << "subpassInputMS" << " input_attach;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    ivec2 p            = ivec2(gl_FragCoord.xy);\n"
			<< "    int   width        = " << params.renderSize.x() << ";\n"
			<< "    int   numSamples   = " << static_cast<deUint32>(params.numColorSamples) << ";\n"
			<< "    int   colorOutNdx  = numSamples * (p.x + width * p.y);\n"
			<< "\n"
			<< "    uint mask = fragmentMaskFetchAMD(input_attach);\n"
			<< "    for (int sampleNdx = 0; sampleNdx < numSamples; ++sampleNdx)\n"
			<< "    {\n"
			<< "        int fragNdx = int((mask >> (4 * sampleNdx)) & 0xf);\n"
			<< "        " << samplerPrefix << "vec4 color = fragmentFetchAMD(input_attach, fragNdx);\n"
			<< "        sb_out.color[colorOutNdx + sampleNdx] = " << colorBufferPack << "(color);\n"
			<< "    }\n"
			<< "}\n";

		programCollection.glslSources.add("frag_fmask_fetch") << glu::FragmentSource(src.str());
	}

	// Generate compute shaders
	const struct ComputeShaderParams
	{
		const char*		name;
		bool			isFmaskFetch;
		bool			enabled;
	} computeShaders[] =
	{
		// name					// FMASK?	// enabled?
		{ "comp_fetch",			false,		true,													},
		{ "comp_fmask_fetch",	true,		(params.sampleSource != SAMPLE_SOURCE_SUBPASS_INPUT)	},
	};

	for (const ComputeShaderParams* pShaderParams = computeShaders; pShaderParams != DE_ARRAY_END(computeShaders); ++pShaderParams)
	if (pShaderParams->enabled)
	{
		const std::string samplingPos = (params.numLayers == 1 ? "ivec2(gl_WorkGroupID.xy)"
															   : "ivec3(gl_WorkGroupID)");
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< (pShaderParams->isFmaskFetch ? "#extension GL_AMD_shader_fragment_mask : enable\n" : "")
			<< "#define NUM_SAMPLES " << static_cast<deUint32>(params.numColorSamples) << "\n"
			<< "\n"
			<< "layout(local_size_x = NUM_SAMPLES) in;\n"	// one work group per pixel, each sample gets a local invocation
			<< "\n"
			<< "layout(set = 0, binding = 0) uniform " << samplerPrefix << "sampler2DMS" << (params.numLayers > 1 ? "Array" : "") << " u_image;\n"
			<< "layout(set = 0, binding = 1, std430) writeonly buffer ColorOutput {\n"
			<< "    " << colorBufferType << " color[];\n"
			<< "} sb_out;\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    int sampleNdx   = int(gl_LocalInvocationID.x);\n"
			<< "    int colorOutNdx = NUM_SAMPLES * int(gl_WorkGroupID.x +\n"
			<< "                                        gl_WorkGroupID.y * gl_NumWorkGroups.x +\n"
			<< "                                        gl_WorkGroupID.z * gl_NumWorkGroups.x * gl_NumWorkGroups.y);\n"
			<< "\n";
		if (pShaderParams->isFmaskFetch)
		{
			src << "    uint  mask    = fragmentMaskFetchAMD(u_image, " << samplingPos << ");\n"
				<< "    int   fragNdx = int((mask >> (4 * sampleNdx)) & 0xf);\n"
				<< "    " << samplerPrefix << "vec4 color = fragmentFetchAMD(u_image, " << samplingPos << ", fragNdx);\n"
				<< "    sb_out.color[colorOutNdx + sampleNdx] = " << colorBufferPack << "(color);\n";
		}
		else
		{
			src << "    " << samplerPrefix << "vec4 color = texelFetch(u_image, " << samplingPos << ", sampleNdx);\n"
				<< "    sb_out.color[colorOutNdx + sampleNdx] = " << colorBufferPack << "(color);\n";
		}
		src << "}\n";

		programCollection.glslSources.add(pShaderParams->name) << glu::ComputeSource(src.str());
	}
}

std::vector<VkClearValue> genClearValues (const VkFormat format, const deUint32 count)
{
	std::vector<VkClearValue>	clearValues;
	de::Random					rng (332);

	switch (format)
	{
		case VK_FORMAT_R8G8B8A8_UNORM:
			for (deUint32 i = 0u; i < count; ++i)
				clearValues.push_back(makeClearValueColorF32(rng.getFloat(), rng.getFloat(), rng.getFloat(), 1.0f));
			break;

		case VK_FORMAT_R32_UINT:
		case VK_FORMAT_R32_SINT:
			for (deUint32 i = 0u; i < count; ++i)
				clearValues.push_back(makeClearValueColorU32(rng.getUint32(), 0u, 0u, 0u));
			break;

		default:
			DE_FATAL("Clear color not defined for this format");
			break;
	}

	return clearValues;
}

//! For subpass load case draw and fetch must happen within the same render pass.
void drawAndSampleInputAttachment (Context& context, const TestParams& params, WorkingData& wd)
{
	DE_ASSERT(params.numLayers == 1u);	// subpass load with single-layer image

	const DeviceInterface&	vk		= context.getDeviceInterface();
	const VkDevice			device	= context.getDevice();

	Move<VkRenderPass>		renderPass;
	Move<VkFramebuffer>		framebuffer;

	// Create descriptor set
	const Unique<VkDescriptorSetLayout> descriptorSetLayout (DescriptorSetLayoutBuilder()
		.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	VK_SHADER_STAGE_FRAGMENT_BIT, &wd.defaultSampler.get())
		.addSingleBinding		(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,			VK_SHADER_STAGE_FRAGMENT_BIT)
		.addSingleBinding		(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,		VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool (DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet (makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	{
		const VkDescriptorImageInfo		colorImageInfo	= makeDescriptorImageInfo(DE_NULL, *wd.colorImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		const VkDescriptorBufferInfo	bufferInfo		= makeDescriptorBufferInfo(*wd.colorBuffer, 0u, wd.colorBufferSize);

		DescriptorSetUpdateBuilder	builder;

		builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &colorImageInfo);
		builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,		 &bufferInfo);

		if (params.sampleSource == SAMPLE_SOURCE_SUBPASS_INPUT)
			builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &colorImageInfo);

		builder.update(vk, device);
	}

	// Create a render pass and a framebuffer
	{
		std::vector<VkSubpassDescription>		subpasses;
		std::vector<VkSubpassDependency>		subpassDependencies;
		std::vector<VkImageView>				attachments;
		std::vector<VkAttachmentDescription>	attachmentDescriptions;
		std::vector<VkAttachmentReference>		attachmentReferences;

		// Reserve capacity to avoid invalidating pointers to elements
		attachmentReferences.reserve(2);	// color image + input attachment

		// Create a MS draw subpass
		{
			attachments.push_back(*wd.colorImageView);

			attachmentDescriptions.push_back(makeAttachmentDescription(
				(VkAttachmentDescriptionFlags)0,								// VkAttachmentDescriptionFlags		flags;
				params.colorFormat,												// VkFormat							format;
				params.numColorSamples,											// VkSampleCountFlagBits			samples;
				VK_ATTACHMENT_LOAD_OP_CLEAR,									// VkAttachmentLoadOp				loadOp;
				VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp				storeOp;
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,								// VkAttachmentLoadOp				stencilLoadOp;
				VK_ATTACHMENT_STORE_OP_DONT_CARE,								// VkAttachmentStoreOp				stencilStoreOp;
				VK_IMAGE_LAYOUT_UNDEFINED,										// VkImageLayout					initialLayout;
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL						// VkImageLayout					finalLayout;
			));

			attachmentReferences.push_back(makeAttachmentReference(static_cast<deUint32>(attachmentReferences.size()),	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
			const VkAttachmentReference* colorRef = &attachmentReferences.back();

			const VkSubpassDescription subpassDescription =
			{
				(VkSubpassDescriptionFlags)0,						// VkSubpassDescriptionFlags       flags;
				VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint             pipelineBindPoint;
				0u,													// uint32_t                        inputAttachmentCount;
				DE_NULL,											// const VkAttachmentReference*    pInputAttachments;
				1u,													// uint32_t                        colorAttachmentCount;
				colorRef,											// const VkAttachmentReference*    pColorAttachments;
				DE_NULL,											// const VkAttachmentReference*    pResolveAttachments;
				DE_NULL,											// const VkAttachmentReference*    pDepthStencilAttachment;
				0u,													// uint32_t                        preserveAttachmentCount;
				DE_NULL,											// const uint32_t*                 pPreserveAttachments;
			};

			subpasses.push_back(subpassDescription);
		}

		// Create a sampling subpass
		{
			attachmentReferences.push_back(makeAttachmentReference(0u, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
			const VkAttachmentReference* inputRef = &attachmentReferences.back();

			// No color attachment, side effects only
			VkSubpassDescription subpassDescription =
			{
				(VkSubpassDescriptionFlags)0,						// VkSubpassDescriptionFlags       flags;
				VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint             pipelineBindPoint;
				1u,													// uint32_t                        inputAttachmentCount;
				inputRef,											// const VkAttachmentReference*    pInputAttachments;
				0u,													// uint32_t                        colorAttachmentCount;
				DE_NULL,											// const VkAttachmentReference*    pColorAttachments;
				DE_NULL,											// const VkAttachmentReference*    pResolveAttachments;
				DE_NULL,											// const VkAttachmentReference*    pDepthStencilAttachment;
				0u,													// uint32_t                        preserveAttachmentCount;
				DE_NULL,											// const uint32_t*                 pPreserveAttachments;
			};

			subpasses.push_back(subpassDescription);
		}

		// Serialize the subpasses
		{
			const VkAccessFlags	dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
											  | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
											  | VK_ACCESS_SHADER_WRITE_BIT;
			const VkSubpassDependency	dependency	=
			{
				0u,																							// uint32_t                srcSubpass;
				1u,																							// uint32_t                dstSubpass;
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,		// VkPipelineStageFlags    srcStageMask;
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,														// VkPipelineStageFlags    dstStageMask;
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,														// VkAccessFlags           srcAccessMask;
				dstAccessMask,																				// VkAccessFlags           dstAccessMask;
				VK_DEPENDENCY_BY_REGION_BIT,																// VkDependencyFlags       dependencyFlags;
			};
			subpassDependencies.push_back(dependency);
		}

		VkRenderPassCreateInfo renderPassInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,				// VkStructureType					sType;
			DE_NULL,												// const void*						pNext;
			(VkRenderPassCreateFlags)0,								// VkRenderPassCreateFlags			flags;
			static_cast<deUint32>(attachmentDescriptions.size()),	// deUint32							attachmentCount;
			dataOrNullPtr(attachmentDescriptions),					// const VkAttachmentDescription*	pAttachments;
			static_cast<deUint32>(subpasses.size()),				// deUint32							subpassCount;
			dataOrNullPtr(subpasses),								// const VkSubpassDescription*		pSubpasses;
			static_cast<deUint32>(subpassDependencies.size()),		// deUint32							dependencyCount;
			dataOrNullPtr(subpassDependencies),						// const VkSubpassDependency*		pDependencies;
		};

		renderPass  = createRenderPass(vk, device, &renderPassInfo);
		framebuffer = makeFramebuffer (vk, device, *renderPass, static_cast<deUint32>(attachments.size()), dataOrNullPtr(attachments), params.renderSize.x(), params.renderSize.y());
	}

	const Unique<VkShaderModule>	vertexModuleDraw	(createShaderModule(vk, device, context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>	fragmentModuleDraw	(createShaderModule(vk, device, context.getBinaryCollection().get("frag"), 0u));

	// Create pipelines for MS draw
	const Unique<VkPipelineLayout>	pipelineLayout		(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline>		pipelineDraw		(makeGraphicsPipeline(vk, device, *pipelineLayout, *renderPass, *vertexModuleDraw, *fragmentModuleDraw,
																			  true/*use vertex attribs*/, getVertexInputColorFormat(params.colorFormat), true/*use color attach*/, 0u/*subpass*/,
																			  params.renderSize, params.numColorSamples));

	// Sampling pass is single-sampled, output to storage buffer
	const Unique<VkShaderModule>	vertexModuleSample		(createShaderModule(vk, device, context.getBinaryCollection().get("vert_full"), 0u));
	const Unique<VkShaderModule>	fragmentModuleSample	(createShaderModule(vk, device, context.getBinaryCollection().get("frag_fmask_fetch"), 0u));

	// Sampling pipeline
	const Unique<VkPipeline>		pipelineSample		(makeGraphicsPipeline(vk, device, *pipelineLayout, *renderPass, *vertexModuleSample, *fragmentModuleSample,
																			  false/*use vertex attribs*/, VK_FORMAT_UNDEFINED, false/*no color output*/, 1u/*subpass*/,
																			  params.renderSize, VK_SAMPLE_COUNT_1_BIT));

	const Unique<VkCommandPool>		cmdPool		(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, context.getUniversalQueueFamilyIndex()));
	const Unique<VkCommandBuffer>	cmdBuffer	(makeCommandBuffer(vk, device, *cmdPool));

	beginCommandBuffer(vk, *cmdBuffer);

	{
		// Generate clear values
		std::vector<VkClearValue> clearValues = genClearValues(params.colorFormat, params.numLayers);

		const VkRect2D renderArea =
		{
			{ 0u, 0u },
			{ params.renderSize.x(), params.renderSize.y() }
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

	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

	{
		const VkDeviceSize vertexBufferOffset = 0ull;
		vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &wd.vertexBuffer.get(), &vertexBufferOffset);
	}

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineDraw);
	vk.cmdDraw(*cmdBuffer, wd.numVertices, 1u, 0u, 0u);

	vk.cmdNextSubpass(*cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineSample);
	vk.cmdDraw(*cmdBuffer, 3u, 1u, 0u, 0u);	// fill the framebuffer, geometry defined in the VS

	vk.cmdEndRenderPass(*cmdBuffer);

	// Buffer write barrier
	{
		const VkBufferMemoryBarrier barrier =
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,		// VkStructureType    sType;
			DE_NULL,										// const void*        pNext;
			VK_ACCESS_SHADER_WRITE_BIT,						// VkAccessFlags      srcAccessMask;
			VK_ACCESS_HOST_READ_BIT,						// VkAccessFlags      dstAccessMask;
			VK_QUEUE_FAMILY_IGNORED,						// uint32_t           srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,						// uint32_t           dstQueueFamilyIndex;
			*wd.colorBuffer,								// VkBuffer           buffer;
			0ull,											// VkDeviceSize       offset;
			VK_WHOLE_SIZE,									// VkDeviceSize       size;
		};

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0u, DE_NULL, 1u, &barrier, DE_NULL, 0u);
	}

	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));
	submitCommandsAndWait(vk, device, context.getUniversalQueue(), *cmdBuffer);

	invalidateMappedMemoryRange(vk, device, wd.colorBufferAlloc->getMemory(), wd.colorBufferAlloc->getOffset(), VK_WHOLE_SIZE);
}

//! Only draw a multisampled image
void draw (Context& context, const TestParams& params, WorkingData& wd)
{
	const DeviceInterface&	vk		= context.getDeviceInterface();
	const VkDevice			device	= context.getDevice();

	std::vector<ImageViewSp>	imageViews;
	Move<VkRenderPass>			renderPass;
	Move<VkFramebuffer>			framebuffer;

	// Create color attachments
	for (deUint32 layerNdx = 0u; layerNdx < params.numLayers; ++layerNdx)
	{
		imageViews.push_back(ImageViewSp(new Unique<VkImageView>(
			makeImageView(vk, device, *wd.colorImage, VK_IMAGE_VIEW_TYPE_2D, params.colorFormat, makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, layerNdx, 1u)))));
	}

	// Create a render pass and a framebuffer
	{
		std::vector<VkSubpassDescription>		subpasses;
		std::vector<VkImageView>				attachments;
		std::vector<VkAttachmentDescription>	attachmentDescriptions;
		std::vector<VkAttachmentReference>		attachmentReferences;

		// Reserve capacity to avoid invalidating pointers to elements
		attachmentReferences.reserve(params.numLayers);

		// Create MS draw subpasses
		for (deUint32 layerNdx = 0u; layerNdx < params.numLayers; ++layerNdx)
		{
			attachments.push_back(**imageViews[layerNdx]);

			attachmentDescriptions.push_back(makeAttachmentDescription(
				(VkAttachmentDescriptionFlags)0,								// VkAttachmentDescriptionFlags		flags;
				params.colorFormat,												// VkFormat							format;
				params.numColorSamples,											// VkSampleCountFlagBits			samples;
				VK_ATTACHMENT_LOAD_OP_CLEAR,									// VkAttachmentLoadOp				loadOp;
				VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp				storeOp;
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,								// VkAttachmentLoadOp				stencilLoadOp;
				VK_ATTACHMENT_STORE_OP_DONT_CARE,								// VkAttachmentStoreOp				stencilStoreOp;
				VK_IMAGE_LAYOUT_UNDEFINED,										// VkImageLayout					initialLayout;
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL						// VkImageLayout					finalLayout;
			));

			attachmentReferences.push_back(makeAttachmentReference(static_cast<deUint32>(attachmentReferences.size()),	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
			const VkAttachmentReference* colorRef = &attachmentReferences.back();

			const VkSubpassDescription subpassDescription =
			{
				(VkSubpassDescriptionFlags)0,						// VkSubpassDescriptionFlags       flags;
				VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint             pipelineBindPoint;
				0u,													// uint32_t                        inputAttachmentCount;
				DE_NULL,											// const VkAttachmentReference*    pInputAttachments;
				1u,													// uint32_t                        colorAttachmentCount;
				colorRef,											// const VkAttachmentReference*    pColorAttachments;
				DE_NULL,											// const VkAttachmentReference*    pResolveAttachments;
				DE_NULL,											// const VkAttachmentReference*    pDepthStencilAttachment;
				0u,													// uint32_t                        preserveAttachmentCount;
				DE_NULL,											// const uint32_t*                 pPreserveAttachments;
			};

			subpasses.push_back(subpassDescription);
		}

		// All MS image drawing subpasses are independent
		VkRenderPassCreateInfo renderPassInfo =
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
		framebuffer = makeFramebuffer (vk, device, *renderPass, static_cast<deUint32>(attachments.size()), dataOrNullPtr(attachments), params.renderSize.x(), params.renderSize.y());
	}

	std::vector<PipelineSp>			pipelines;
	const Unique<VkPipelineLayout>	pipelineLayout		(makePipelineLayout(vk, device));
	const Unique<VkShaderModule>	vertexModuleDraw	(createShaderModule(vk, device, context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>	fragmentModuleDraw	(createShaderModule(vk, device, context.getBinaryCollection().get("frag"), 0u));

	// Create pipelines for MS draw
	for (deUint32 layerNdx = 0u; layerNdx < params.numLayers; ++layerNdx)
	{
		pipelines.push_back(PipelineSp(new Unique<VkPipeline>(
			makeGraphicsPipeline(vk, device, *pipelineLayout, *renderPass, *vertexModuleDraw, *fragmentModuleDraw,
								 true /*use vertex attribs*/, getVertexInputColorFormat(params.colorFormat), true/*use color attachment*/, layerNdx /*subpass*/,
								 params.renderSize, params.numColorSamples))));
	}

	const Unique<VkCommandPool>		cmdPool		(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, context.getUniversalQueueFamilyIndex()));
	const Unique<VkCommandBuffer>	cmdBuffer	(makeCommandBuffer(vk, device, *cmdPool));

	beginCommandBuffer(vk, *cmdBuffer);

	{
		// Generate clear values
		std::vector<VkClearValue> clearValues = genClearValues(params.colorFormat, params.numLayers);

		const VkRect2D renderArea =
		{
			{ 0u, 0u },
			{ params.renderSize.x(), params.renderSize.y() }
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

	{
		const VkDeviceSize vertexBufferOffset = 0ull;
		vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &wd.vertexBuffer.get(), &vertexBufferOffset);
	}

	for (deUint32 layerNdx = 0u; layerNdx < params.numLayers; ++layerNdx)
	{
		if (layerNdx != 0u)
			vk.cmdNextSubpass(*cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, **pipelines[layerNdx]);
		vk.cmdDraw(*cmdBuffer, wd.numVertices, 1u, 0u, layerNdx);	// pass instance index to slightly change geometry per layer
	}

	vk.cmdEndRenderPass(*cmdBuffer);

	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));
	submitCommandsAndWait(vk, device, context.getUniversalQueue(), *cmdBuffer);
}

//! Sample from an image in a compute shader, storing the result in a color buffer
void dispatchSampleImage (Context& context, const TestParams& params, WorkingData& wd, const std::string& shaderName)
{
	const DeviceInterface&	vk		= context.getDeviceInterface();
	const VkDevice			device	= context.getDevice();

	// Create descriptor set

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleSamplerBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	VK_SHADER_STAGE_COMPUTE_BIT, &wd.defaultSampler.get())
		.addSingleBinding		(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,			VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	{
		const VkDescriptorImageInfo		colorImageInfo		= makeDescriptorImageInfo(DE_NULL, *wd.colorImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		const VkDescriptorBufferInfo	resultBufferInfo	= makeDescriptorBufferInfo(*wd.colorBuffer, 0ull, wd.colorBufferSize);

		DescriptorSetUpdateBuilder	builder;

		builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &colorImageInfo);
		builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,		  &resultBufferInfo);

		builder.update(vk, device);
	}

	// Pipeline

	const Unique<VkShaderModule>	shaderModule	(createShaderModule(vk, device, context.getBinaryCollection().get(shaderName), 0u));
	const Unique<VkPipelineLayout>	pipelineLayout	(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline>		pipeline		(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule, DE_NULL));

	const Unique<VkCommandPool>		cmdPool		(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, context.getUniversalQueueFamilyIndex()));
	const Unique<VkCommandBuffer>	cmdBuffer	(makeCommandBuffer(vk, device, *cmdPool));

	beginCommandBuffer(vk, *cmdBuffer);

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

	vk.cmdDispatch(*cmdBuffer, params.renderSize.x(), params.renderSize.y(), params.numLayers);

	{
		const VkBufferMemoryBarrier barrier =
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,		// VkStructureType    sType;
			DE_NULL,										// const void*        pNext;
			VK_ACCESS_SHADER_WRITE_BIT,						// VkAccessFlags      srcAccessMask;
			VK_ACCESS_HOST_READ_BIT,						// VkAccessFlags      dstAccessMask;
			VK_QUEUE_FAMILY_IGNORED,						// uint32_t           srcQueueFamilyIndex;
			VK_QUEUE_FAMILY_IGNORED,						// uint32_t           dstQueueFamilyIndex;
			*wd.colorBuffer,								// VkBuffer           buffer;
			0ull,											// VkDeviceSize       offset;
			VK_WHOLE_SIZE,									// VkDeviceSize       size;
		};

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0,
			(const VkMemoryBarrier*)DE_NULL, 1u, &barrier, 0u, (const VkImageMemoryBarrier*)DE_NULL);
	}

	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));
	submitCommandsAndWait(vk, device, context.getUniversalQueue(), *cmdBuffer);

	invalidateMappedMemoryRange(vk, device, wd.colorBufferAlloc->getMemory(), wd.colorBufferAlloc->getOffset(), VK_WHOLE_SIZE);
}

//! Get a single-sampled image access from a multisampled color buffer with samples packed per pixel
tcu::ConstPixelBufferAccess getSingleSampledAccess (const void* const imageData, const TestParams& params, const deUint32 sampleNdx, const deUint32 layerNdx)
{
	const deUint32		numSamples	= static_cast<deUint32>(params.numColorSamples);
	const deUint32		pixelSize	= tcu::getPixelSize(mapVkFormat(params.colorFormat));
	const deUint32		rowSize		= pixelSize * params.renderSize.x();
	const deUint32		layerSize	= rowSize * params.renderSize.y();
	const deUint8*		src			= static_cast<const deUint8*>(imageData)
									+ (layerNdx * numSamples * layerSize)
									+ (sampleNdx * pixelSize);
	const tcu::IVec3	size		(params.renderSize.x(), params.renderSize.y(), 1);
	const tcu::IVec3	pitch		(numSamples * pixelSize,
									 numSamples * rowSize,
									 numSamples * layerSize);
	return tcu::ConstPixelBufferAccess(mapVkFormat(params.colorFormat), size, pitch, src);
}

tcu::TestStatus test (Context& context, const TestParams params)
{
	WorkingData				wd;
	const DeviceInterface&	vk		  = context.getDeviceInterface();
	const VkDevice			device	  = context.getDevice();
	MovePtr<Allocator>		allocator = MovePtr<Allocator>(new SimpleAllocator(vk, device, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice())));

	// Initialize resources
	{
		const VkImageUsageFlags	msImageUsage	= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
												| VK_IMAGE_USAGE_SAMPLED_BIT
												| (params.sampleSource == SAMPLE_SOURCE_SUBPASS_INPUT ? VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT : (VkImageUsageFlagBits)0);
		wd.colorImage		= makeImage(vk, device, params.colorFormat, params.renderSize, params.numLayers, params.numColorSamples, msImageUsage);
		wd.colorImageAlloc	= bindImage(vk, device, *allocator, *wd.colorImage, MemoryRequirement::Any);
		wd.colorImageView	= makeImageView(vk, device, *wd.colorImage, (params.numLayers == 1u ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY), params.colorFormat,
											makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, params.numLayers));

		wd.defaultSampler	= makeSampler(vk, device);

		// Color buffer is meant to hold data for all layers and all samples of the image.
		// Data is tightly packed layer by layer, for each pixel all samples are laid out together starting with sample 0.
		// E.g.: pixel(0,0)sample(0)sample(1), pixel(1,0)sample(0)sample(1), ...
		wd.colorBufferSize	= static_cast<VkDeviceSize>(tcu::getPixelSize(mapVkFormat(params.colorFormat))
														* params.renderSize.x() * params.renderSize.y() * params.numLayers * static_cast<deUint32>(params.numColorSamples));
		wd.colorBuffer		= makeBuffer(vk, device, wd.colorBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		wd.colorBufferAlloc	= bindBuffer(vk, device, *allocator, *wd.colorBuffer, MemoryRequirement::HostVisible);

		deMemset(wd.colorBufferAlloc->getHostPtr(), 0, static_cast<std::size_t>(wd.colorBufferSize));
		flushMappedMemoryRange(vk, device, wd.colorBufferAlloc->getMemory(), wd.colorBufferAlloc->getOffset(), VK_WHOLE_SIZE);

		const std::vector<PositionColor>	vertices			= genShapes(params.colorFormat);
		const VkDeviceSize					vertexBufferSize	= static_cast<VkDeviceSize>(sizeof(vertices[0]) * vertices.size());

		wd.numVertices			= static_cast<deUint32>(vertices.size());
		wd.vertexBuffer			= makeBuffer(vk, device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		wd.vertexBufferAlloc	= bindBuffer(vk, device, *allocator, *wd.vertexBuffer, MemoryRequirement::HostVisible);

		deMemcpy(wd.vertexBufferAlloc->getHostPtr(), dataOrNullPtr(vertices), static_cast<std::size_t>(vertexBufferSize));
		flushMappedMemoryRange(vk, device, wd.vertexBufferAlloc->getMemory(), wd.vertexBufferAlloc->getOffset(), VK_WHOLE_SIZE);
	}

	if (params.sampleSource == SAMPLE_SOURCE_SUBPASS_INPUT)
	{
		// Create a multisample image and sample from it
		drawAndSampleInputAttachment (context, params, wd);
	}
	else
	{
		// Draw the image, then sample from it in a CS
		draw				(context, params, wd);
		dispatchSampleImage (context, params, wd, "comp_fmask_fetch");
	}

	// Copy the result
	std::vector<deUint8> fmaskFetchColorBuffer (static_cast<deUint32>(wd.colorBufferSize));
	deMemcpy(&fmaskFetchColorBuffer[0], wd.colorBufferAlloc->getHostPtr(), static_cast<std::size_t>(wd.colorBufferSize));

	// Clear the color buffer, just to be sure we're getting the new data
	deMemset(wd.colorBufferAlloc->getHostPtr(), 0, static_cast<std::size_t>(wd.colorBufferSize));
	flushMappedMemoryRange(vk, device, wd.colorBufferAlloc->getMemory(), wd.colorBufferAlloc->getOffset(), VK_WHOLE_SIZE);

	// Sample image using the standard texel fetch
	dispatchSampleImage (context, params, wd, "comp_fetch");

	// Verify the images
	{
		const void* const fmaskResult	 = dataOrNullPtr(fmaskFetchColorBuffer);
		const void* const expectedResult = wd.colorBufferAlloc->getHostPtr();

		DE_ASSERT(!isFloatFormat(params.colorFormat));	// we're using int compare

		// Mismatch, do image compare to pinpoint the failure
		for (deUint32 layerNdx  = 0u; layerNdx  < params.numLayers;								 ++layerNdx)
		for (deUint32 sampleNdx = 0u; sampleNdx < static_cast<deUint32>(params.numColorSamples); ++sampleNdx)
		{
			const std::string					imageName	= "layer_" + de::toString(layerNdx) + "_sample_" + de::toString(sampleNdx);
			const std::string					imageDesc	= "Layer " + de::toString(layerNdx) + " Sample " + de::toString(sampleNdx);
			const tcu::ConstPixelBufferAccess	expected	= getSingleSampledAccess(expectedResult, params, sampleNdx, layerNdx);
			const tcu::ConstPixelBufferAccess	actual		= getSingleSampledAccess(fmaskResult,	 params, sampleNdx, layerNdx);
			const UVec4							threshold	(0);	// should match exactly

			const bool ok = tcu::intThresholdCompare(context.getTestContext().getLog(), imageName.c_str(), imageDesc.c_str(),
													 expected, actual, threshold, tcu::COMPARE_LOG_RESULT);

			if (!ok)
				return tcu::TestStatus::fail("Some texels were incorrect");
		}
	}

	return tcu::TestStatus::pass("Pass");
}

std::string getFormatShortString (const VkFormat format)
{
	std::string s(de::toLower(getFormatName(format)));
	return s.substr(10);
}

void createShaderFragmentMaskTestsInGroup (tcu::TestCaseGroup* rootGroup)
{
	// Per spec, the following formats must support color attachment and sampled image
	const VkFormat colorFormats[] =
	{
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32_SINT,
	};

	const VkSampleCountFlagBits	sampleCounts[] =
	{
		VK_SAMPLE_COUNT_2_BIT,
		VK_SAMPLE_COUNT_4_BIT,
		VK_SAMPLE_COUNT_8_BIT,
		VK_SAMPLE_COUNT_16_BIT,
	};

	const struct SourceCase
	{
		const char*			name;
		deUint32			numLayers;
		SampleSource		sampleSource;
	} sourceCases[] =
	{
		{ "image_2d",		1u,	SAMPLE_SOURCE_IMAGE			},
		{ "image_2d_array",	3u,	SAMPLE_SOURCE_IMAGE			},
		{ "subpass_input",	1u,	SAMPLE_SOURCE_SUBPASS_INPUT	},
	};

	// Test 1: Compare fragments fetched via FMASK and an ordinary texel fetch
	{
		for (const VkSampleCountFlagBits* pSampleCount = sampleCounts; pSampleCount != DE_ARRAY_END(sampleCounts); ++pSampleCount)
		{
			MovePtr<tcu::TestCaseGroup> sampleCountGroup (new tcu::TestCaseGroup(rootGroup->getTestContext(), ("samples_" + de::toString(*pSampleCount)).c_str(), ""));
			for (const SourceCase* pSourceCase = sourceCases; pSourceCase != DE_ARRAY_END(sourceCases); ++pSourceCase)
			{
				MovePtr<tcu::TestCaseGroup> sourceGroup (new tcu::TestCaseGroup(rootGroup->getTestContext(), pSourceCase->name, ""));
				for (const VkFormat* pColorFormat = colorFormats; pColorFormat != DE_ARRAY_END(colorFormats); ++pColorFormat)
				{
					TestParams params;
					params.renderSize		= UVec2(32, 32);
					params.colorFormat		= *pColorFormat;
					params.numColorSamples	= *pSampleCount;
					params.numLayers		= pSourceCase->numLayers;
					params.sampleSource		= pSourceCase->sampleSource;

					addFunctionCaseWithPrograms(sourceGroup.get(), getFormatShortString(*pColorFormat), "", checkRequirements, initPrograms, test, params);
				}
				sampleCountGroup->addChild(sourceGroup.release());
			}
			rootGroup->addChild(sampleCountGroup.release());
		}
	}
}

} // anonymous ns

tcu::TestCaseGroup* createMultisampleShaderFragmentMaskTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "shader_fragment_mask", "Access raw texel values in a compressed MSAA surface", createShaderFragmentMaskTestsInGroup);
}

} // pipeline
} // vkt
