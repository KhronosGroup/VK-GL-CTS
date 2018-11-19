/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2014 The Android Open Source Project
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
 * \brief Geometry shader layered rendering tests
 *//*--------------------------------------------------------------------*/

#include "vktGeometryLayeredRenderingTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktGeometryTestsUtil.hpp"

#include "vkPrograms.hpp"
#include "vkStrUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTestLog.hpp"

namespace vkt
{
namespace geometry
{
namespace
{
using namespace vk;
using de::MovePtr;
using de::UniquePtr;
using tcu::Vec4;
using tcu::IVec3;

enum TestType
{
	TEST_TYPE_DEFAULT_LAYER,					// !< draw to default layer
	TEST_TYPE_SINGLE_LAYER,						// !< draw to single layer
	TEST_TYPE_ALL_LAYERS,						// !< draw all layers
	TEST_TYPE_DIFFERENT_CONTENT,				// !< draw different content to different layers
	TEST_TYPE_LAYER_ID,							// !< draw to all layers, verify gl_Layer fragment input
	TEST_TYPE_INVOCATION_PER_LAYER,				// !< draw to all layers, one invocation per layer
	TEST_TYPE_MULTIPLE_LAYERS_PER_INVOCATION,	// !< draw to all layers, multiple invocations write to multiple layers
	TEST_TYPE_LAYERED_READBACK,					// !< draw to two layers multiple times
};

struct ImageParams
{
	VkImageViewType		viewType;
	VkExtent3D			size;
	deUint32			numLayers;
};

struct TestParams
{
	TestType			testType;
	ImageParams			image;
};

static const float s_colors[][4] =
{
	{ 1.0f, 1.0f, 1.0f, 1.0f },		// white
	{ 1.0f, 0.0f, 0.0f, 1.0f },		// red
	{ 0.0f, 1.0f, 0.0f, 1.0f },		// green
	{ 0.0f, 0.0f, 1.0f, 1.0f },		// blue
	{ 1.0f, 1.0f, 0.0f, 1.0f },		// yellow
	{ 1.0f, 0.0f, 1.0f, 1.0f },		// magenta
};

tcu::Vec4 scaleColor(const tcu::Vec4& color, float factor)
{
	return tcu::Vec4(color[0] * factor,
					 color[1] * factor,
					 color[2] * factor,
					 color[3]);
}

deUint32 getTargetLayer (const ImageParams& imageParams)
{
	if (imageParams.viewType == VK_IMAGE_VIEW_TYPE_3D)
		return imageParams.size.depth / 2;
	else
		return imageParams.numLayers / 2;
}

std::string getShortImageViewTypeName (const VkImageViewType imageViewType)
{
	std::string s(getImageViewTypeName(imageViewType));
	return de::toLower(s.substr(19));
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

VkFormat getStencilBufferFormat(VkFormat depthStencilImageFormat)
{
	const tcu::TextureFormat tcuFormat = mapVkFormat(depthStencilImageFormat);
	const VkFormat result = (tcuFormat.order == tcu::TextureFormat::S || tcuFormat.order == tcu::TextureFormat::DS) ? VK_FORMAT_S8_UINT : VK_FORMAT_UNDEFINED;

	DE_ASSERT(result != VK_FORMAT_UNDEFINED);

	return result;
}

inline bool isCubeImageViewType (const VkImageViewType viewType)
{
	return viewType == VK_IMAGE_VIEW_TYPE_CUBE || viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
}

void checkImageFormatProperties (const InstanceInterface&	vki,
								 const VkPhysicalDevice&	physDevice,
								 const VkImageType&			imageType,
								 const VkImageTiling&		imageTiling,
								 const VkImageUsageFlags	imageUsageFlags,
								 const VkImageCreateFlags	imageCreateFlags,
								 const VkFormat				format,
								 const VkExtent3D&			requiredSize,
								 const deUint32				requiredLayers)
{
	VkImageFormatProperties	imageFormatProperties;
	VkResult				result;

	deMemset(&imageFormatProperties, 0, sizeof(imageFormatProperties));

	result = vki.getPhysicalDeviceImageFormatProperties(physDevice, format, imageType, imageTiling, imageUsageFlags, imageCreateFlags, &imageFormatProperties);

	if (result									!= VK_SUCCESS			||
		imageFormatProperties.maxArrayLayers	<  requiredLayers		||
		imageFormatProperties.maxExtent.height	<  requiredSize.height	||
		imageFormatProperties.maxExtent.width	<  requiredSize.width	||
		imageFormatProperties.maxExtent.depth	<  requiredSize.depth)
	{
		TCU_THROW(NotSupportedError, "Depth/stencil format is not supported");
	}
}

VkImageCreateInfo makeImageCreateInfo (const VkImageCreateFlags flags, const VkImageType type, const VkFormat format, const VkExtent3D size, const deUint32 numLayers, const VkImageUsageFlags usage)
{
	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		flags,											// VkImageCreateFlags		flags;
		type,											// VkImageType				imageType;
		format,											// VkFormat					format;
		size,											// VkExtent3D				extent;
		1u,												// deUint32					mipLevels;
		numLayers,										// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling;
		usage,											// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode;
		0u,												// deUint32					queueFamilyIndexCount;
		DE_NULL,										// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			initialLayout;
	};
	return imageParams;
}

Move<VkRenderPass> makeRenderPass (const DeviceInterface&	vk,
								   const VkDevice			device,
								   const VkFormat			colorFormat,
								   const VkFormat			dsFormat,
								   const bool				useDepthStencil)
{
	return vk::makeRenderPass(vk, device, colorFormat, useDepthStencil ? dsFormat : VK_FORMAT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_LOAD);
}


Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&		vk,
									   const VkDevice				device,
									   const VkPipelineLayout		pipelineLayout,
									   const VkRenderPass			renderPass,
									   const VkShaderModule			vertexModule,
									   const VkShaderModule			geometryModule,
									   const VkShaderModule			fragmentModule,
									   const VkExtent2D				renderSize,
									   const bool					useDepthStencil = false)
{
	const std::vector<VkViewport>			viewports						(1, makeViewport(renderSize));
	const std::vector<VkRect2D>				scissors						(1, makeRect2D(renderSize));

	const VkStencilOpState					stencilOpState					= makeStencilOpState(
		VK_STENCIL_OP_KEEP,					// stencil fail
		VK_STENCIL_OP_INCREMENT_AND_CLAMP,	// depth & stencil pass
		VK_STENCIL_OP_KEEP,					// depth only fail
		VK_COMPARE_OP_ALWAYS,				// compare op
		~0u,								// compare mask
		~0u,								// write mask
		0u);								// reference

	VkPipelineDepthStencilStateCreateInfo	pipelineDepthStencilStateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType                           sType;
		DE_NULL,													// const void*                               pNext;
		(VkPipelineDepthStencilStateCreateFlags)0,					// VkPipelineDepthStencilStateCreateFlags    flags;
		useDepthStencil ? VK_TRUE : VK_FALSE,						// VkBool32                                  depthTestEnable;
		useDepthStencil ? VK_TRUE : VK_FALSE,						// VkBool32                                  depthWriteEnable;
		VK_COMPARE_OP_LESS,											// VkCompareOp                               depthCompareOp;
		VK_FALSE,													// VkBool32                                  depthBoundsTestEnable;
		useDepthStencil ? VK_TRUE : VK_FALSE,						// VkBool32                                  stencilTestEnable;
		stencilOpState,												// VkStencilOpState                          front;
		stencilOpState,												// VkStencilOpState                          back;
		0.0f,														// float                                     minDepthBounds;
		1.0f														// float                                     maxDepthBounds;
	};

	return vk::makeGraphicsPipeline(vk,									// const DeviceInterface&                        vk
									device,								// const VkDevice                                device
									pipelineLayout,						// const VkPipelineLayout                        pipelineLayout
									vertexModule,						// const VkShaderModule                          vertexShaderModule
									DE_NULL,							// const VkShaderModule                          tessellationControlModule
									DE_NULL,							// const VkShaderModule                          tessellationEvalModule
									geometryModule,						// const VkShaderModule                          geometryShaderModule
									fragmentModule,						// const VkShaderModule                          fragmentShaderModule
									renderPass,							// const VkRenderPass                            renderPass
									viewports,							// const std::vector<VkViewport>&                viewports
									scissors,							// const std::vector<VkRect2D>&                  scissors
									VK_PRIMITIVE_TOPOLOGY_POINT_LIST,	// const VkPrimitiveTopology                     topology
									0u,									// const deUint32                                subpass
									0u,									// const deUint32                                patchControlPoints
									DE_NULL,							// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
									DE_NULL,							// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
									DE_NULL,							// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
									&pipelineDepthStencilStateInfo);	// const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
}

Move<VkFramebuffer> makeFramebuffer (const DeviceInterface&	vk,
									 const VkDevice			device,
									 const VkRenderPass		renderPass,
									 const VkImageView*		pAttachments,
									 const deUint32			attachmentsCount,
									 const deUint32			width,
									 const deUint32			height,
									 const deUint32			layers)
{
	const VkFramebufferCreateInfo framebufferInfo = {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,		// VkStructureType                             sType;
		DE_NULL,										// const void*                                 pNext;
		(VkFramebufferCreateFlags)0,					// VkFramebufferCreateFlags                    flags;
		renderPass,										// VkRenderPass                                renderPass;
		attachmentsCount,								// uint32_t                                    attachmentCount;
		pAttachments,									// const VkImageView*                          pAttachments;
		width,											// uint32_t                                    width;
		height,											// uint32_t                                    height;
		layers,											// uint32_t                                    layers;
	};

	return createFramebuffer(vk, device, &framebufferInfo);
}

//! Convenience wrapper to access 1D, 2D, and 3D image layers/slices in a uniform way.
class LayeredImageAccess
{
public:
	static LayeredImageAccess create (const VkImageType type, const VkFormat format, const VkExtent3D size, const deUint32 numLayers, const void* pData)
	{
		if (type == VK_IMAGE_TYPE_1D)
			return LayeredImageAccess(format, size.width, numLayers, pData);
		else
			return LayeredImageAccess(type, format, size, numLayers, pData);
	}

	inline tcu::ConstPixelBufferAccess getLayer (const int layer) const
	{
		return tcu::getSubregion(m_wholeImage, 0, (m_1dModifier * layer), ((~m_1dModifier & 1) * layer), m_width, m_height, 1);
	}

	inline int getNumLayersOrSlices (void) const
	{
		return m_layers;
	}

private:
	// Specialized for 1D images.
	LayeredImageAccess (const VkFormat format, const deUint32 width, const deUint32 numLayers, const void* pData)
		: m_width		(static_cast<int>(width))
		, m_height		(1)
		, m_1dModifier	(1)
		, m_layers		(numLayers)
		, m_wholeImage	(tcu::ConstPixelBufferAccess(mapVkFormat(format), m_width, m_layers, 1, pData))
	{
	}

	LayeredImageAccess (const VkImageType type, const VkFormat format, const VkExtent3D size, const deUint32 numLayers, const void* pData)
		: m_width		(static_cast<int>(size.width))
		, m_height		(static_cast<int>(size.height))
		, m_1dModifier	(0)
		, m_layers		(static_cast<int>(type == VK_IMAGE_TYPE_3D ? size.depth : numLayers))
		, m_wholeImage	(tcu::ConstPixelBufferAccess(mapVkFormat(format), m_width, m_height, m_layers, pData))
	{
	}

	const int							m_width;
	const int							m_height;
	const int							m_1dModifier;
	const int							m_layers;
	const tcu::ConstPixelBufferAccess	m_wholeImage;
};

inline bool compareColors (const Vec4& colorA, const Vec4& colorB, const Vec4& threshold)
{
	return tcu::allEqual(
				tcu::lessThan(tcu::abs(colorA - colorB), threshold),
				tcu::BVec4(true, true, true, true));
}

bool verifyImageSingleColoredRow (tcu::TestLog& log, const tcu::ConstPixelBufferAccess image, const float rowWidthRatio, const tcu::Vec4& barColor)
{
	DE_ASSERT(rowWidthRatio > 0.0f);

	const Vec4				black				(0.0f, 0.0f, 0.0f, 1.0f);
	const Vec4				green				(0.0f, 1.0f, 0.0f, 1.0f);
	const Vec4				red					(1.0f, 0.0f, 0.0f, 1.0f);
	const Vec4				threshold			(0.02f);
	const int				barLength			= static_cast<int>(rowWidthRatio * static_cast<float>(image.getWidth()));
	const int				barLengthThreshold	= 1;
	tcu::TextureLevel		errorMask			(image.getFormat(), image.getWidth(), image.getHeight());
	tcu::PixelBufferAccess	errorMaskAccess		= errorMask.getAccess();

	tcu::clear(errorMask.getAccess(), green);

	log << tcu::TestLog::Message
		<< "Expecting all pixels with distance less or equal to (about) " << barLength
		<< " pixels from left border to be of color " << barColor.swizzle(0, 1, 2) << "."
		<< tcu::TestLog::EndMessage;

	bool allPixelsOk = true;

	for (int y = 0; y < image.getHeight(); ++y)
	for (int x = 0; x < image.getWidth();  ++x)
	{
		const Vec4	color		= image.getPixel(x, y);
		const bool	isBlack		= compareColors(color, black, threshold);
		const bool	isColor		= compareColors(color, barColor, threshold);

		bool isOk;

		if (x <= barLength - barLengthThreshold)
			isOk = isColor;
		else if (x >= barLength + barLengthThreshold)
			isOk = isBlack;
		else
			isOk = isColor || isBlack;

		allPixelsOk &= isOk;

		if (!isOk)
			errorMaskAccess.setPixel(red, x, y);
	}

	if (allPixelsOk)
	{
		log << tcu::TestLog::Message << "Image is valid." << tcu::TestLog::EndMessage
			<< tcu::TestLog::ImageSet("LayerContent", "Layer content")
			<< tcu::TestLog::Image("Layer", "Layer", image)
			<< tcu::TestLog::EndImageSet;
		return true;
	}
	else
	{
		log << tcu::TestLog::Message << "Image verification failed. Got unexpected pixels." << tcu::TestLog::EndMessage
			<< tcu::TestLog::ImageSet("LayerContent", "Layer content")
			<< tcu::TestLog::Image("Layer",		"Layer",	image)
			<< tcu::TestLog::Image("ErrorMask",	"Errors",	errorMask)
			<< tcu::TestLog::EndImageSet;
		return false;
	}

	log << tcu::TestLog::Image("LayerContent", "Layer content", image);

	return allPixelsOk;
}

static bool verifyImageMultipleBars (tcu::TestLog&						log,
									 const tcu::ConstPixelBufferAccess	image,
									 const float*						barWidthRatios,
									 const tcu::Vec4*					barValues,
									 const int							barsCount,
									 const int							numUsedChannels,
									 const std::string&					imageTypeName)
{
	const Vec4					green				(0.0f, 1.0f, 0.0f, 1.0f);
	const Vec4					red					(1.0f, 0.0f, 0.0f, 1.0f);
	const Vec4					threshold			(0.02f);
	const tcu::TextureFormat	errorMaskFormat		(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8));
	tcu::TextureLevel			errorMask			(errorMaskFormat, image.getWidth(), image.getHeight());
	tcu::PixelBufferAccess		errorMaskAccess		= errorMask.getAccess();
	bool						allPixelsOk			= true;

	DE_ASSERT(barsCount > 0);

	tcu::clear(errorMask.getAccess(), green);

	// Format information message
	{
		int					leftBorder	= 0;
		int					rightBorder	= 0;
		std::ostringstream	str;

		for (int barNdx = 0; barNdx < barsCount; ++barNdx)
		{
			leftBorder	= rightBorder;
			rightBorder	= static_cast<int>(barWidthRatios[barNdx] * static_cast<float>(image.getWidth()));

			DE_ASSERT(leftBorder < rightBorder);

			str << std::endl << " [" << leftBorder << "," <<rightBorder << "): ";

			switch (numUsedChannels)
			{
				case 1:	str << barValues[barNdx][0];	break;
				case 4:	str << barValues[barNdx];		break;
				default: DE_ASSERT(false);				break;
			}
		}

		log << tcu::TestLog::Message
			<< "Expecting " + imageTypeName + " values depending x-axis position to be of following values: "
			<< str.str()
			<< tcu::TestLog::EndMessage;
	}

	for (int x = 0; x < image.getWidth();  ++x)
	{
		tcu::Vec4	expectedValue	= barValues[0];

		for (int barNdx = 0; barNdx < barsCount; ++barNdx)
		{
			const int rightBorder	= static_cast<int>(barWidthRatios[barNdx] * static_cast<float>(image.getWidth()));

			if (x < rightBorder)
			{
				expectedValue = barValues[barNdx];

				break;
			}
		}

		for (int y = 0; y < image.getHeight(); ++y)
		{
			const tcu::Vec4	realValue	= image.getPixel(x, y);
			bool			isOk		= false;

			switch (numUsedChannels)
			{
				case 1:	isOk = fabs(realValue[0] - expectedValue[0]) < threshold[0];	break;
				case 4:	isOk = compareColors(realValue, expectedValue, threshold);		break;
				default: DE_ASSERT(false);												break;
			}

			if (!isOk)
				errorMaskAccess.setPixel(red, x, y);

			allPixelsOk = allPixelsOk && isOk;
		}
	}

	if (allPixelsOk)
	{
		log << tcu::TestLog::Message << "Image is valid." << tcu::TestLog::EndMessage
			<< tcu::TestLog::ImageSet(imageTypeName + "LayerContent", imageTypeName + " Layer Content")
			<< tcu::TestLog::Image("Layer", "Layer", image)
			<< tcu::TestLog::EndImageSet;
	}
	else
	{
		log << tcu::TestLog::Message << "Image verification failed. Got unexpected pixels." << tcu::TestLog::EndMessage
			<< tcu::TestLog::ImageSet(imageTypeName + "LayerContent", imageTypeName + " Layer Content")
			<< tcu::TestLog::Image("Layer",		"Layer",	image)
			<< tcu::TestLog::Image("ErrorMask",	"Errors",	errorMask)
			<< tcu::TestLog::EndImageSet;
	}

	return allPixelsOk;
}

static void convertDepthToColorBufferAccess (const tcu::ConstPixelBufferAccess& inputImage, tcu::PixelBufferAccess& outputImage)
{
	for (int y = 0; y < inputImage.getHeight(); y++)
	for (int x = 0; x < inputImage.getWidth(); x++)
	{
		const float		depth	= inputImage.getPixDepth(x, y);
		const tcu::Vec4	color	= tcu::Vec4(depth, depth, depth, 1.0f);

		outputImage.setPixel(color, x, y);
	}
}

static void convertStencilToColorBufferAccess (const tcu::ConstPixelBufferAccess& inputImage, tcu::PixelBufferAccess& outputImage, int maxValue)
{
	for (int y = 0; y < inputImage.getHeight(); y++)
	for (int x = 0; x < inputImage.getWidth(); x++)
	{
		const int		stencilInt	= inputImage.getPixStencil(x, y);
		const float		stencil		= (stencilInt < maxValue) ? float(stencilInt) / float(maxValue) : 1.0f;
		const tcu::Vec4	color		= tcu::Vec4(stencil, stencil, stencil, 1.0f);

		outputImage.setPixel(color, x, y);
	}
}

bool verifyEmptyImage (tcu::TestLog& log, const tcu::ConstPixelBufferAccess image)
{
	log << tcu::TestLog::Message << "Expecting empty image" << tcu::TestLog::EndMessage;

	const Vec4	black		(0.0f, 0.0f, 0.0f, 1.0f);
	const Vec4	threshold	(0.02f);

	for (int y = 0; y < image.getHeight(); ++y)
	for (int x = 0; x < image.getWidth();  ++x)
	{
		const Vec4 color = image.getPixel(x, y);

		if (!compareColors(color, black, threshold))
		{
			log	<< tcu::TestLog::Message
				<< "Found (at least) one bad pixel at " << x << "," << y << ". Pixel color is not background color."
				<< tcu::TestLog::EndMessage
				<< tcu::TestLog::ImageSet("LayerContent", "Layer content")
				<< tcu::TestLog::Image("Layer", "Layer", image)
				<< tcu::TestLog::EndImageSet;
			return false;
		}
	}

	log << tcu::TestLog::Message << "Image is valid" << tcu::TestLog::EndMessage;

	return true;
}

bool verifyLayerContent (tcu::TestLog& log, const TestType testType, const tcu::ConstPixelBufferAccess image, const int layerNdx, const int numLayers, const bool depthCheck, const bool stencilCheck)
{
	const Vec4	white				(1.0f, 1.0f, 1.0f, 1.0f);
	const int	targetLayer			= numLayers / 2;
	const float	variableBarRatio	= static_cast<float>(layerNdx) / static_cast<float>(numLayers);

	switch (testType)
	{
		case TEST_TYPE_DEFAULT_LAYER:
			if (layerNdx == 0)
				return verifyImageSingleColoredRow(log, image, 0.5f, white);
			else
				return verifyEmptyImage(log, image);

		case TEST_TYPE_SINGLE_LAYER:
			if (layerNdx == targetLayer)
				return verifyImageSingleColoredRow(log, image, 0.5f, white);
			else
				return verifyEmptyImage(log, image);

		case TEST_TYPE_ALL_LAYERS:
		case TEST_TYPE_INVOCATION_PER_LAYER:
			return verifyImageSingleColoredRow(log, image, 0.5f, s_colors[layerNdx % DE_LENGTH_OF_ARRAY(s_colors)]);

		case TEST_TYPE_DIFFERENT_CONTENT:
		case TEST_TYPE_MULTIPLE_LAYERS_PER_INVOCATION:
			if (layerNdx == 0)
				return verifyEmptyImage(log, image);
			else
				return verifyImageSingleColoredRow(log, image, variableBarRatio, white);

		case TEST_TYPE_LAYER_ID:
		{
			// This code must be in sync with the fragment shader.
			const tcu::Vec4 layerColor( (layerNdx    % 2) == 1 ? 1.0f : 0.5f,
									   ((layerNdx/2) % 2) == 1 ? 1.0f : 0.5f,
									     layerNdx         == 0 ? 1.0f : 0.0f,
																 1.0f);
			return verifyImageSingleColoredRow(log, image, 0.5f, layerColor);
		}

		case TEST_TYPE_LAYERED_READBACK:
		{
			const float	barWidthRatios[]	= { 0.25f, 0.5f, 1.0f };
			const int	barsCount			= DE_LENGTH_OF_ARRAY(barWidthRatios);
			bool		result				= false;

			if (depthCheck)
			{
				const std::string		checkType				= "Depth";
				const float				pass0depth				= static_cast<float>(layerNdx + 1) / static_cast<float>(2 * numLayers);
				const float				pass1depth				= static_cast<float>(layerNdx + 0) / static_cast<float>(2 * numLayers);
				const tcu::Vec4			barDepths[barsCount]	= { tcu::Vec4(pass1depth), tcu::Vec4(pass0depth), tcu::Vec4(1.0f) };
				tcu::TextureLevel		depthAsColorBuffer		(tcu::TextureFormat(tcu::TextureFormat::R, tcu::TextureFormat::FLOAT), image.getWidth(), image.getHeight());
				tcu::PixelBufferAccess	depthAsColor			(depthAsColorBuffer);
				const int				numUsedChannels			(tcu::getNumUsedChannels(depthAsColor.getFormat().order));

				convertDepthToColorBufferAccess(image, depthAsColor);

				result = verifyImageMultipleBars(log, depthAsColor, barWidthRatios, barDepths, barsCount, numUsedChannels, checkType);
			}
			else if (stencilCheck)
			{
				const std::string		checkType				= "Stencil";
				const int				maxStencilValue			= 4;
				const float				pass0stencil			= static_cast<float>(1.0f / maxStencilValue);
				const float				pass1stencil			= static_cast<float>(2.0f / maxStencilValue);
				const tcu::Vec4			barStencils[barsCount]	= { tcu::Vec4(pass1stencil), tcu::Vec4(pass0stencil), tcu::Vec4(0.0f) };
				tcu::TextureLevel		stencilAsColorBuffer	(tcu::TextureFormat(tcu::TextureFormat::R, tcu::TextureFormat::FLOAT), image.getWidth(), image.getHeight());
				tcu::PixelBufferAccess	stencilAsColor			(stencilAsColorBuffer);
				const int				numUsedChannels			(tcu::getNumUsedChannels(stencilAsColor.getFormat().order));

				convertStencilToColorBufferAccess(image, stencilAsColor, maxStencilValue);

				result = verifyImageMultipleBars(log, stencilAsColor, barWidthRatios, barStencils, barsCount, numUsedChannels, checkType);
			}
			else
			{
				const std::string		checkType				= "Color";
				const tcu::Vec4			baseColor				(s_colors[layerNdx % DE_LENGTH_OF_ARRAY(s_colors)]);
				const tcu::Vec4			barColors[barsCount]	= { scaleColor(baseColor, 1.00f), scaleColor(baseColor, 0.50f), scaleColor(baseColor, 0.25f) };
				const int				numUsedChannels			(tcu::getNumUsedChannels(image.getFormat().order));

				result = verifyImageMultipleBars(log, image, barWidthRatios, barColors, barsCount, numUsedChannels, checkType);
			}

			return result;
		}

		default:
			DE_ASSERT(0);
			return false;
	};
}

std::string getLayerDescription (const VkImageViewType viewType, const int layer)
{
	std::ostringstream str;
	const int numCubeFaces = 6;

	if (isCubeImageViewType(viewType))
		str << "cube " << (layer / numCubeFaces) << ", face " << (layer % numCubeFaces);
	else if (viewType == VK_IMAGE_VIEW_TYPE_3D)
		str << "slice z = " << layer;
	else
		str << "layer " << layer;

	return str.str();
}

bool verifyResults (tcu::TestLog& log, const TestParams& params, const VkFormat imageFormat, const void* resultData, const bool depthCheck = false, const bool stencilCheck = false)
{
	const LayeredImageAccess image = LayeredImageAccess::create(getImageType(params.image.viewType), imageFormat, params.image.size, params.image.numLayers, resultData);

	int numGoodLayers = 0;

	for (int layerNdx = 0; layerNdx < image.getNumLayersOrSlices(); ++layerNdx)
	{
		const tcu::ConstPixelBufferAccess layerImage = image.getLayer(layerNdx);

		log << tcu::TestLog::Message << "Verifying " << getLayerDescription(params.image.viewType, layerNdx) << tcu::TestLog::EndMessage;

		if (verifyLayerContent(log, params.testType, layerImage, layerNdx, image.getNumLayersOrSlices(), depthCheck, stencilCheck))
			++numGoodLayers;
	}

	return numGoodLayers == image.getNumLayersOrSlices();
}

std::string toGlsl (const Vec4& v)
{
	std::ostringstream str;
	str << "vec4(";
	for (int i = 0; i < 4; ++i)
		str << (i != 0 ? ", " : "") << de::floatToString(v[i], 1);
	str << ")";
	return str.str();
}

void initPrograms (SourceCollections& programCollection, const TestParams params)
{
	const bool geomOutputColor = (params.testType == TEST_TYPE_ALL_LAYERS || params.testType == TEST_TYPE_INVOCATION_PER_LAYER || params.testType == TEST_TYPE_LAYERED_READBACK);

	// Vertex shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Geometry shader
	{
		const int numLayers		= static_cast<int>(params.image.viewType == VK_IMAGE_VIEW_TYPE_3D ? params.image.size.depth : params.image.numLayers);

		const int maxVertices	= (params.testType == TEST_TYPE_DIFFERENT_CONTENT)																						? (numLayers + 1) * numLayers :
								  (params.testType == TEST_TYPE_ALL_LAYERS || params.testType == TEST_TYPE_LAYER_ID || params.testType == TEST_TYPE_LAYERED_READBACK)	? numLayers * 4 :
								  (params.testType == TEST_TYPE_MULTIPLE_LAYERS_PER_INVOCATION)																			? 6 : 4;

		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n";

		if (params.testType == TEST_TYPE_LAYERED_READBACK)
			src << "layout(binding = 0) readonly uniform Input {\n"
				<< "    int pass;\n"
				<< "} uInput;\n\n";

		if (params.testType == TEST_TYPE_INVOCATION_PER_LAYER || params.testType == TEST_TYPE_MULTIPLE_LAYERS_PER_INVOCATION)
			src << "layout(points, invocations = " << numLayers << ") in;\n";
		else
			src << "layout(points) in;\n";

		src << "layout(triangle_strip, max_vertices = " << maxVertices << ") out;\n"
			<< "\n"
			<< (geomOutputColor ? "layout(location = 0) out vec4 vert_color;\n\n" : "")
			<< "out gl_PerVertex {\n"
			<< "    vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n";

		std::ostringstream colorTable;
		{
			const int numColors = DE_LENGTH_OF_ARRAY(s_colors);

			colorTable << "    const vec4 colors[" << numColors << "] = vec4[" << numColors << "](";

			const std::string padding(colorTable.str().length(), ' ');

			for (int i = 0; i < numColors; ++i)
				colorTable << (i != 0 ? ",\n" + padding : "") << toGlsl(s_colors[i]);

			colorTable << ");\n";
		}

		if (params.testType == TEST_TYPE_DEFAULT_LAYER)
		{
			src << "    gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
				<< "    EmitVertex();\n"
				<< "\n"
				<< "    gl_Position = vec4(-1.0,  1.0, 0.0, 1.0);\n"
				<< "    EmitVertex();\n"
				<< "\n"
				<< "    gl_Position = vec4( 0.0, -1.0, 0.0, 1.0);\n"
				<< "    EmitVertex();\n"
				<< "\n"
				<< "    gl_Position = vec4( 0.0,  1.0, 0.0, 1.0);\n"
				<< "    EmitVertex();\n";
		}
		else if (params.testType == TEST_TYPE_SINGLE_LAYER)
		{
			const deUint32 targetLayer = getTargetLayer(params.image);

			src << "    gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
				<< "    gl_Layer    = " << targetLayer << ";\n"
				<< "    EmitVertex();\n"
				<< "\n"
				<< "    gl_Position = vec4(-1.0,  1.0, 0.0, 1.0);\n"
				<< "    gl_Layer    = " << targetLayer << ";\n"
				<< "    EmitVertex();\n"
				<< "\n"
				<< "    gl_Position = vec4( 0.0, -1.0, 0.0, 1.0);\n"
				<< "    gl_Layer    = " << targetLayer << ";\n"
				<< "    EmitVertex();\n"
				<< "\n"
				<< "    gl_Position = vec4( 0.0,  1.0, 0.0, 1.0);\n"
				<< "    gl_Layer    = " << targetLayer << ";\n"
				<< "    EmitVertex();\n";
		}
		else if (params.testType == TEST_TYPE_ALL_LAYERS)
		{
			src << colorTable.str()
				<< "\n"
				<< "    for (int layerNdx = 0; layerNdx < " << numLayers << "; ++layerNdx) {\n"
				<< "        const int colorNdx = layerNdx % " << DE_LENGTH_OF_ARRAY(s_colors) << ";\n"
				<< "\n"
				<< "        gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
				<< "        gl_Layer    = layerNdx;\n"
				<< "        vert_color  = colors[colorNdx];\n"
				<< "        EmitVertex();\n"
				<< "\n"
				<< "        gl_Position = vec4(-1.0,  1.0, 0.0, 1.0);\n"
				<< "        gl_Layer    = layerNdx;\n"
				<< "        vert_color  = colors[colorNdx];\n"
				<< "        EmitVertex();\n"
				<< "\n"
				<< "        gl_Position = vec4( 0.0, -1.0, 0.0, 1.0);\n"
				<< "        gl_Layer    = layerNdx;\n"
				<< "        vert_color  = colors[colorNdx];\n"
				<< "        EmitVertex();\n"
				<< "\n"
				<< "        gl_Position = vec4( 0.0,  1.0, 0.0, 1.0);\n"
				<< "        gl_Layer    = layerNdx;\n"
				<< "        vert_color  = colors[colorNdx];\n"
				<< "        EmitVertex();\n"
				<< "        EndPrimitive();\n"
				<< "    };\n";
		}
		else if (params.testType == TEST_TYPE_LAYER_ID)
		{
			src << "    for (int layerNdx = 0; layerNdx < " << numLayers << "; ++layerNdx) {\n"
				<< "        gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
				<< "        gl_Layer    = layerNdx;\n"
				<< "        EmitVertex();\n"
				<< "\n"
				<< "        gl_Position = vec4(-1.0,  1.0, 0.0, 1.0);\n"
				<< "        gl_Layer    = layerNdx;\n"
				<< "        EmitVertex();\n"
				<< "\n"
				<< "        gl_Position = vec4( 0.0, -1.0, 0.0, 1.0);\n"
				<< "        gl_Layer    = layerNdx;\n"
				<< "        EmitVertex();\n"
				<< "\n"
				<< "        gl_Position = vec4( 0.0,  1.0, 0.0, 1.0);\n"
				<< "        gl_Layer    = layerNdx;\n"
				<< "        EmitVertex();\n"
				<< "        EndPrimitive();\n"
				<< "    };\n";
		}
		else if (params.testType == TEST_TYPE_DIFFERENT_CONTENT)
		{
			src << "    for (int layerNdx = 0; layerNdx < " << numLayers << "; ++layerNdx) {\n"
				<< "        for (int colNdx = 0; colNdx <= layerNdx; ++colNdx) {\n"
				<< "            const float posX = float(colNdx) / float(" << numLayers << ") * 2.0 - 1.0;\n"
				<< "\n"
				<< "            gl_Position = vec4(posX,  1.0, 0.0, 1.0);\n"
				<< "            gl_Layer    = layerNdx;\n"
				<< "            EmitVertex();\n"
				<< "\n"
				<< "            gl_Position = vec4(posX, -1.0, 0.0, 1.0);\n"
				<< "            gl_Layer    = layerNdx;\n"
				<< "            EmitVertex();\n"
				<< "        }\n"
				<< "        EndPrimitive();\n"
				<< "    }\n";
		}
		else if (params.testType == TEST_TYPE_INVOCATION_PER_LAYER)
		{
			src << colorTable.str()
				<< "    const int colorNdx = gl_InvocationID % " << DE_LENGTH_OF_ARRAY(s_colors) << ";\n"
				<< "\n"
				<< "    gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
				<< "    gl_Layer    = gl_InvocationID;\n"
				<< "    vert_color  = colors[colorNdx];\n"
				<< "    EmitVertex();\n"
				<< "\n"
				<< "    gl_Position = vec4(-1.0,  1.0, 0.0, 1.0);\n"
				<< "    gl_Layer    = gl_InvocationID;\n"
				<< "    vert_color  = colors[colorNdx];\n"
				<< "    EmitVertex();\n"
				<< "\n"
				<< "    gl_Position = vec4( 0.0, -1.0, 0.0, 1.0);\n"
				<< "    gl_Layer    = gl_InvocationID;\n"
				<< "    vert_color  = colors[colorNdx];\n"
				<< "    EmitVertex();\n"
				<< "\n"
				<< "    gl_Position = vec4( 0.0,  1.0, 0.0, 1.0);\n"
				<< "    gl_Layer    = gl_InvocationID;\n"
				<< "    vert_color  = colors[colorNdx];\n"
				<< "    EmitVertex();\n"
				<< "    EndPrimitive();\n";
		}
		else if (params.testType == TEST_TYPE_MULTIPLE_LAYERS_PER_INVOCATION)
		{
			src << "    const int   layerA = gl_InvocationID;\n"
				<< "    const int   layerB = (gl_InvocationID + 1) % " << numLayers << ";\n"
				<< "    const float aEnd   = float(layerA) / float(" << numLayers << ") * 2.0 - 1.0;\n"
				<< "    const float bEnd   = float(layerB) / float(" << numLayers << ") * 2.0 - 1.0;\n"
				<< "\n"
				<< "    gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
				<< "    gl_Layer    = layerA;\n"
				<< "    EmitVertex();\n"
				<< "\n"
				<< "    gl_Position = vec4(-1.0,  1.0, 0.0, 1.0);\n"
				<< "    gl_Layer    = layerA;\n"
				<< "    EmitVertex();\n"
				<< "\n"
				<< "    gl_Position = vec4(aEnd, -1.0, 0.0, 1.0);\n"
				<< "    gl_Layer    = layerA;\n"
				<< "    EmitVertex();\n"
				<< "    EndPrimitive();\n"
				<< "\n"
				<< "    gl_Position = vec4(-1.0,  1.0, 0.0, 1.0);\n"
				<< "    gl_Layer    = layerB;\n"
				<< "    EmitVertex();\n"
				<< "\n"
				<< "    gl_Position = vec4(bEnd,  1.0, 0.0, 1.0);\n"
				<< "    gl_Layer    = layerB;\n"
				<< "    EmitVertex();\n"
				<< "\n"
				<< "    gl_Position = vec4(bEnd, -1.0, 0.0, 1.0);\n"
				<< "    gl_Layer    = layerB;\n"
				<< "    EmitVertex();\n"
				<< "    EndPrimitive();\n";
		}
		else if (params.testType == TEST_TYPE_LAYERED_READBACK)
		{
			src << colorTable.str()
				<< "    for (int layerNdx = 0; layerNdx < " << numLayers << "; ++layerNdx) {\n"
				<< "        const int   colorNdx   = layerNdx % " << DE_LENGTH_OF_ARRAY(s_colors) << ";\n"
				<< "        const vec3  passColor0 = (uInput.pass == 0 ? 0.5 :  1.0) * vec3(colors[colorNdx]);\n"
				<< "        const vec4  passColor  = vec4(passColor0, 1.0);\n"
				<< "        const float posX       = (uInput.pass == 0 ? 0.0 : -0.5);\n"
				<< "        const float posZ       = float(layerNdx + 1 - uInput.pass) / float(" << 2*numLayers << ");\n"
				<< "\n"
				<< "        gl_Position = vec4(-1.0, -1.0, posZ, 1.0);\n"
				<< "        gl_Layer    = layerNdx;\n"
				<< "        vert_color  = passColor;\n"
				<< "        EmitVertex();\n"
				<< "\n"
				<< "        gl_Position = vec4(-1.0,  1.0, posZ, 1.0);\n"
				<< "        gl_Layer    = layerNdx;\n"
				<< "        vert_color  = passColor;\n"
				<< "        EmitVertex();\n"
				<< "\n"
				<< "        gl_Position = vec4(posX, -1.0, posZ, 1.0);\n"
				<< "        gl_Layer    = layerNdx;\n"
				<< "        vert_color  = passColor;\n"
				<< "        EmitVertex();\n"
				<< "\n"
				<< "        gl_Position = vec4(posX,  1.0, posZ, 1.0);\n"
				<< "        gl_Layer    = layerNdx;\n"
				<< "        vert_color  = passColor;\n"
				<< "        EmitVertex();\n"
				<< "\n"
				<< "        EndPrimitive();\n"
				<< "    }\n";
		}
		else
			DE_ASSERT(0);

		src <<	"}\n";	// end main

		programCollection.glslSources.add("geom") << glu::GeometrySource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) out vec4 o_color;\n"
			<< (geomOutputColor ? "layout(location = 0) in  vec4 vert_color;\n" : "")
			<< "\n"
			<< "void main(void)\n"
			<< "{\n";

		if (params.testType == TEST_TYPE_LAYER_ID)
		{
			// This code must be in sync with verifyLayerContent()
			src << "    o_color = vec4( (gl_Layer    % 2) == 1 ? 1.0 : 0.5,\n"
				<< "                   ((gl_Layer/2) % 2) == 1 ? 1.0 : 0.5,\n"
				<< "                     gl_Layer         == 0 ? 1.0 : 0.0,\n"
				<< "                                             1.0);\n";
		}
		else if (geomOutputColor)
			src << "    o_color = vert_color;\n";
		else
			src << "    o_color = vec4(1.0);\n";

		src << "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

tcu::TestStatus test (Context& context, const TestParams params)
{
	if (VK_IMAGE_VIEW_TYPE_3D == params.image.viewType &&
		(!isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_KHR_maintenance1")))
		TCU_THROW(NotSupportedError, "Extension VK_KHR_maintenance1 not supported");

	const DeviceInterface&			vk						= context.getDeviceInterface();
	const InstanceInterface&		vki						= context.getInstanceInterface();
	const VkDevice					device					= context.getDevice();
	const VkPhysicalDevice			physDevice				= context.getPhysicalDevice();
	const deUint32					queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	const VkQueue					queue					= context.getUniversalQueue();
	Allocator&						allocator				= context.getDefaultAllocator();

	checkGeometryShaderSupport(vki, physDevice);

	const VkFormat					colorFormat				= VK_FORMAT_R8G8B8A8_UNORM;
	const deUint32					numLayers				= (VK_IMAGE_VIEW_TYPE_3D == params.image.viewType ? params.image.size.depth : params.image.numLayers);
	const Vec4						clearColor				= Vec4(0.0f, 0.0f, 0.0f, 1.0f);
	const VkDeviceSize				colorBufferSize			= params.image.size.width * params.image.size.height * params.image.size.depth * params.image.numLayers * tcu::getPixelSize(mapVkFormat(colorFormat));
	const VkImageCreateFlags		imageCreateFlags		= (isCubeImageViewType(params.image.viewType) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : (VkImageCreateFlagBits)0) |
															  (VK_IMAGE_VIEW_TYPE_3D == params.image.viewType ? VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT_KHR : (VkImageCreateFlagBits)0);
	const VkImageViewType			viewType				= (VK_IMAGE_VIEW_TYPE_3D == params.image.viewType ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : params.image.viewType);

	const Unique<VkImage>			colorImage				(makeImage				(vk, device, makeImageCreateInfo(imageCreateFlags, getImageType(params.image.viewType), colorFormat, params.image.size,
																					 params.image.numLayers, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)));
	const UniquePtr<Allocation>		colorImageAlloc			(bindImage				(vk, device, allocator, *colorImage, MemoryRequirement::Any));
	const Unique<VkImageView>		colorAttachment			(makeImageView			(vk, device, *colorImage, viewType, colorFormat, makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, numLayers)));

	const Unique<VkBuffer>			colorBuffer				(makeBuffer				(vk, device, makeBufferCreateInfo(colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT)));
	const UniquePtr<Allocation>		colorBufferAlloc		(bindBuffer				(vk, device, allocator, *colorBuffer, MemoryRequirement::HostVisible));

	const Unique<VkShaderModule>	vertexModule			(createShaderModule		(vk, device, context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>	geometryModule			(createShaderModule		(vk, device, context.getBinaryCollection().get("geom"), 0u));
	const Unique<VkShaderModule>	fragmentModule			(createShaderModule		(vk, device, context.getBinaryCollection().get("frag"), 0u));

	const Unique<VkRenderPass>		renderPass				(makeRenderPass			(vk, device, colorFormat));
	const Unique<VkFramebuffer>		framebuffer				(makeFramebuffer		(vk, device, *renderPass, &*colorAttachment, 1u, params.image.size.width,  params.image.size.height, numLayers));
	const Unique<VkPipelineLayout>	pipelineLayout			(makePipelineLayout		(vk, device));
	const Unique<VkPipeline>		pipeline				(makeGraphicsPipeline	(vk, device, *pipelineLayout, *renderPass, *vertexModule, *geometryModule, *fragmentModule,
																					 makeExtent2D(params.image.size.width, params.image.size.height)));
	const Unique<VkCommandPool>		cmdPool					(createCommandPool		(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer				(allocateCommandBuffer	(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	zeroBuffer(vk, device, *colorBufferAlloc, colorBufferSize);

	beginCommandBuffer(vk, *cmdBuffer);

	beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(0, 0, params.image.size.width, params.image.size.height), clearColor);

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
	vk.cmdDraw(*cmdBuffer, 1u, 1u, 0u, 0u);
	endRenderPass(vk, *cmdBuffer);

	// Prepare color image for copy
	{
		const VkImageSubresourceRange	colorSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, params.image.numLayers);
		const VkImageMemoryBarrier		barriers[] =
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
				colorSubresourceRange,							// VkImageSubresourceRange	subresourceRange;
			},
		};

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
			0u, DE_NULL, 0u, DE_NULL, DE_LENGTH_OF_ARRAY(barriers), barriers);
	}
	// Color image -> host buffer
	{
		const VkBufferImageCopy region =
		{
			0ull,																						// VkDeviceSize                bufferOffset;
			0u,																							// uint32_t                    bufferRowLength;
			0u,																							// uint32_t                    bufferImageHeight;
			makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, params.image.numLayers),		// VkImageSubresourceLayers    imageSubresource;
			makeOffset3D(0, 0, 0),																		// VkOffset3D                  imageOffset;
			params.image.size,																			// VkExtent3D                  imageExtent;
		};

		vk.cmdCopyImageToBuffer(*cmdBuffer, *colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorBuffer, 1u, &region);
	}
	// Buffer write barrier
	{
		const VkBufferMemoryBarrier barriers[] =
		{
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,		// VkStructureType    sType;
				DE_NULL,										// const void*        pNext;
				VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags      srcAccessMask;
				VK_ACCESS_HOST_READ_BIT,						// VkAccessFlags      dstAccessMask;
				VK_QUEUE_FAMILY_IGNORED,						// uint32_t           srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,						// uint32_t           dstQueueFamilyIndex;
				*colorBuffer,									// VkBuffer           buffer;
				0ull,											// VkDeviceSize       offset;
				VK_WHOLE_SIZE,									// VkDeviceSize       size;
			},
		};

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u,
			0u, DE_NULL, DE_LENGTH_OF_ARRAY(barriers), barriers, DE_NULL, 0u);
	}

	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	invalidateMappedMemoryRange(vk, device, colorBufferAlloc->getMemory(), colorBufferAlloc->getOffset(), colorBufferSize);

	if (!verifyResults(context.getTestContext().getLog(), params, colorFormat, colorBufferAlloc->getHostPtr()))
		return tcu::TestStatus::fail("Rendered images are incorrect");
	else
		return tcu::TestStatus::pass("OK");
}

tcu::TestStatus testLayeredReadBack (Context& context, const TestParams params)
{
	if (VK_IMAGE_VIEW_TYPE_3D == params.image.viewType &&
		(!isDeviceExtensionSupported(context.getUsedApiVersion(), context.getDeviceExtensions(), "VK_KHR_maintenance1")))
		TCU_THROW(NotSupportedError, "Extension VK_KHR_maintenance1 not supported");

	const DeviceInterface&				vk					= context.getDeviceInterface();
	const InstanceInterface&			vki					= context.getInstanceInterface();
	const VkDevice						device				= context.getDevice();
	const VkPhysicalDevice				physDevice			= context.getPhysicalDevice();
	const deUint32						queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	const VkQueue						queue				= context.getUniversalQueue();
	Allocator&							allocator			= context.getDefaultAllocator();

	checkGeometryShaderSupport(vki, physDevice);

	const size_t						passCount			= 2;
	const deUint32						numLayers			= (VK_IMAGE_VIEW_TYPE_3D == params.image.viewType ? params.image.size.depth : params.image.numLayers);
	const VkImageCreateFlags			imageCreateFlags	= (isCubeImageViewType(params.image.viewType) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : (VkImageCreateFlagBits)0) |
															  (VK_IMAGE_VIEW_TYPE_3D == params.image.viewType ? VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT_KHR : (VkImageCreateFlagBits)0);
	const VkImageViewType				viewType			= (VK_IMAGE_VIEW_TYPE_3D == params.image.viewType ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : params.image.viewType);
	const VkImageType					imageType			= getImageType(params.image.viewType);
	const VkExtent2D					imageExtent2D		= makeExtent2D(params.image.size.width, params.image.size.height);

	const VkFormat						colorFormat			= VK_FORMAT_R8G8B8A8_UNORM;
	const deUint32						colorImagePixelSize	= static_cast<deUint32>(tcu::getPixelSize(mapVkFormat(colorFormat)));
	const VkDeviceSize					colorBufferSize		= params.image.size.width * params.image.size.height * params.image.size.depth * params.image.numLayers * colorImagePixelSize;
	const VkImageUsageFlags				colorImageUsage		= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	const bool							dsUsed				= (VK_IMAGE_VIEW_TYPE_3D != params.image.viewType);
	const VkFormat						dsFormat			= VK_FORMAT_D24_UNORM_S8_UINT;
	const deUint32						dsImagePixelSize	= static_cast<deUint32>(tcu::getPixelSize(mapVkFormat(dsFormat)));
	const VkImageUsageFlags				dsImageUsage		= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const VkImageAspectFlags			dsAspectFlags		= VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	const VkDeviceSize					depthBufferSize		= params.image.size.width * params.image.size.height * params.image.size.depth * params.image.numLayers * dsImagePixelSize;

	const VkFormat						stencilBufferFormat	= getStencilBufferFormat(dsFormat);
	const deUint32						stencilPixelSize	= static_cast<deUint32>(tcu::getPixelSize(mapVkFormat(stencilBufferFormat)));
	const VkDeviceSize					stencilBufferSize	= params.image.size.width * params.image.size.height * params.image.size.depth * params.image.numLayers * stencilPixelSize;

	checkImageFormatProperties(vki, physDevice, imageType, VK_IMAGE_TILING_OPTIMAL, dsImageUsage, imageCreateFlags, dsFormat, params.image.size, params.image.numLayers);

	const Unique<VkImage>				colorImage			(makeImage				(vk, device, makeImageCreateInfo(imageCreateFlags, imageType, colorFormat, params.image.size, params.image.numLayers, colorImageUsage)));
	const UniquePtr<Allocation>			colorImageAlloc		(bindImage				(vk, device, allocator, *colorImage, MemoryRequirement::Any));
	const Unique<VkImageView>			colorAttachment		(makeImageView			(vk, device, *colorImage, viewType, colorFormat, makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, numLayers)));
	const Unique<VkBuffer>				colorBuffer			(makeBuffer				(vk, device, makeBufferCreateInfo(colorBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)));
	const UniquePtr<Allocation>			colorBufferAlloc	(bindBuffer				(vk, device, allocator, *colorBuffer, MemoryRequirement::HostVisible));

	const Unique<VkImage>				dsImage				(makeImage				(vk, device, makeImageCreateInfo(imageCreateFlags, imageType, dsFormat, params.image.size, params.image.numLayers, dsImageUsage)));
	const UniquePtr<Allocation>			dsImageAlloc		(bindImage				(vk, device, allocator, *dsImage, MemoryRequirement::Any));
	const Unique<VkImageView>			dsAttachment		(makeImageView			(vk, device, *dsImage, viewType, dsFormat, makeImageSubresourceRange(dsAspectFlags, 0u, 1u, 0u, numLayers)));
	const Unique<VkBuffer>				depthBuffer			(makeBuffer				(vk, device, makeBufferCreateInfo(depthBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)));
	const UniquePtr<Allocation>			depthBufferAlloc	(bindBuffer				(vk, device, allocator, *depthBuffer, MemoryRequirement::HostVisible));
	const Unique<VkBuffer>				stencilBuffer		(makeBuffer				(vk, device, makeBufferCreateInfo(stencilBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)));
	const UniquePtr<Allocation>			stencilBufferAlloc	(bindBuffer				(vk, device, allocator, *stencilBuffer, MemoryRequirement::HostVisible));

	const VkImageView					attachments[]		= {*colorAttachment, *dsAttachment};
	const deUint32						attachmentsCount	= dsUsed ? DE_LENGTH_OF_ARRAY(attachments) : 1u;

	const Unique<VkShaderModule>		vertexModule		(createShaderModule		(vk, device, context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>		geometryModule		(createShaderModule		(vk, device, context.getBinaryCollection().get("geom"), 0u));
	const Unique<VkShaderModule>		fragmentModule		(createShaderModule		(vk, device, context.getBinaryCollection().get("frag"), 0u));

	const Unique<VkRenderPass>			renderPass			(makeRenderPass			(vk, device, colorFormat, dsFormat, dsUsed));
	const Unique<VkFramebuffer>			framebuffer			(makeFramebuffer		(vk, device, *renderPass, attachments, attachmentsCount, params.image.size.width, params.image.size.height, numLayers));

	const Move<VkDescriptorPool>		descriptorPool		= DescriptorPoolBuilder()
															  .addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, passCount)
															  .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, passCount);
	const Move<VkDescriptorSetLayout>	descriptorSetLayout	= DescriptorSetLayoutBuilder()
															  .addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_GEOMETRY_BIT)
															  .build(vk, device);
	const Move<VkDescriptorSet>			descriptorSet[]		=
	{
		makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout),
		makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout),
	};

	const size_t						uniformBufSize		= sizeof(deUint32);
	const VkBufferCreateInfo			uniformBufCI		= makeBufferCreateInfo(uniformBufSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	const Move<VkBuffer>				uniformBuf[]		= { createBuffer(vk, device, &uniformBufCI), createBuffer(vk, device, &uniformBufCI) };
	const MovePtr<Allocation>			uniformBufAlloc[]	=
	{
		allocator.allocate(getBufferMemoryRequirements(vk, device, *uniformBuf[0]), MemoryRequirement::HostVisible),
		allocator.allocate(getBufferMemoryRequirements(vk, device, *uniformBuf[1]), MemoryRequirement::HostVisible),
	};
	const VkDescriptorBufferInfo		uniformBufDesc[]	=
	{
		makeDescriptorBufferInfo(*uniformBuf[0], 0ull, uniformBufSize),
		makeDescriptorBufferInfo(*uniformBuf[1], 0ull, uniformBufSize),
	};

	const Unique<VkPipelineLayout>		pipelineLayout		(makePipelineLayout		(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline>			pipeline			(makeGraphicsPipeline	(vk, device, *pipelineLayout, *renderPass, *vertexModule, *geometryModule, *fragmentModule, imageExtent2D, dsUsed));
	const Unique<VkCommandPool>			cmdPool				(createCommandPool		(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer			(allocateCommandBuffer	(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const VkImageSubresourceRange		colorSubresRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, params.image.numLayers);
	const VkImageSubresourceRange		dsSubresRange		= makeImageSubresourceRange(dsAspectFlags, 0u, 1u, 0u, params.image.numLayers);
	const VkImageMemoryBarrier			colorPassBarrier	= makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
																VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, *colorImage, colorSubresRange);
	const VkImageMemoryBarrier			dsPassBarrier		= makeImageMemoryBarrier(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
																VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, *dsImage, dsSubresRange);
	std::string							result;

	beginCommandBuffer(vk, *cmdBuffer);
	{
		const VkImageMemoryBarrier		colorBarrier	= makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
																				 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, *colorImage, colorSubresRange);
		const VkImageMemoryBarrier		dsBarrier		= makeImageMemoryBarrier(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
																				 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, *dsImage, dsSubresRange);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &colorBarrier);

		if (dsUsed)
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &dsBarrier);

		for (deUint32 layerNdx = 0; layerNdx < numLayers; ++layerNdx)
		{
			const deUint32		imageDepth	= (VK_IMAGE_VIEW_TYPE_3D == params.image.viewType) ? layerNdx :       0u;
			const deUint32		layer		= (VK_IMAGE_VIEW_TYPE_3D == params.image.viewType) ?       0u : layerNdx;
			const VkOffset3D	imageOffset = makeOffset3D(0u, 0u, imageDepth);
			const VkExtent3D	imageExtent = makeExtent3D(params.image.size.width, params.image.size.height, 1u);

			// Clear color image with initial value
			{
				const tcu::Vec4					clearColor				= scaleColor(s_colors[layerNdx % DE_LENGTH_OF_ARRAY(s_colors)], 0.25f);
				const deUint32					bufferSliceSize			= params.image.size.width * params.image.size.height * colorImagePixelSize;
				const VkDeviceSize				bufferOffset			= layerNdx * bufferSliceSize;
				const VkImageSubresourceLayers	imageSubresource		= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, layer, 1u);
				const VkBufferImageCopy			bufferImageCopyRegion	= makeBufferImageCopy(bufferOffset, imageSubresource, imageOffset, imageExtent);

				fillBuffer(vk, device, *colorBufferAlloc, bufferOffset, bufferSliceSize, colorFormat, clearColor);
				vk.cmdCopyBufferToImage(*cmdBuffer, *colorBuffer, *colorImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &bufferImageCopyRegion);
			}

			// Clear depth image with initial value
			if (dsUsed)
			{
				const float						depthValue				= 1.0f;
				const deUint32					bufferSliceSize			= params.image.size.width * params.image.size.height * dsImagePixelSize;
				const VkDeviceSize				bufferOffset			= layerNdx * bufferSliceSize;
				const VkImageSubresourceLayers	imageSubresource		= makeImageSubresourceLayers(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, layer, 1u);
				const VkBufferImageCopy			bufferImageCopyRegion	= makeBufferImageCopy(bufferOffset, imageSubresource, imageOffset, imageExtent);

				fillBuffer(vk, device, *depthBufferAlloc, bufferOffset, bufferSliceSize, dsFormat, depthValue);
				vk.cmdCopyBufferToImage(*cmdBuffer, *depthBuffer, *dsImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &bufferImageCopyRegion);
			}

			// Clear stencil image with initial value
			if (dsUsed)
			{
				const deUint8					stencilValue			= 0;
				const deUint32					bufferSliceSize			= params.image.size.width * params.image.size.height * stencilPixelSize;
				const VkDeviceSize				bufferOffset			= layerNdx * bufferSliceSize;
				const VkImageSubresourceLayers	imageSubresource		= makeImageSubresourceLayers(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, layer, 1u);
				const VkBufferImageCopy			bufferImageCopyRegion	= makeBufferImageCopy(bufferOffset, imageSubresource, imageOffset, imageExtent);
				deUint8*						bufferStart				= static_cast<deUint8*>((*stencilBufferAlloc).getHostPtr());
				deUint8*						bufferLayerStart		= &bufferStart[bufferOffset];

				deMemset(bufferLayerStart, stencilValue, bufferSliceSize);
				flushMappedMemoryRange(vk, device, stencilBufferAlloc->getMemory(), stencilBufferAlloc->getOffset() + bufferOffset, bufferSliceSize);
				vk.cmdCopyBufferToImage(*cmdBuffer, *stencilBuffer, *dsImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &bufferImageCopyRegion);
			}
		}
	}
	// Change images layouts
	{
		const VkImageMemoryBarrier		colorBarrier	= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
																				 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, *colorImage, colorSubresRange);
		const VkImageMemoryBarrier		dsBarrier		= makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
																				 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, *dsImage, dsSubresRange);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &colorBarrier);

		if (dsUsed)
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &dsBarrier);
	}

	for (deUint32 pass = 0; pass < passCount; ++pass)
	{
		DE_ASSERT(sizeof(pass) == uniformBufSize);

		VK_CHECK(vk.bindBufferMemory(device, *uniformBuf[pass], uniformBufAlloc[pass]->getMemory(), uniformBufAlloc[pass]->getOffset()));
		deMemcpy(uniformBufAlloc[pass]->getHostPtr(), &pass, uniformBufSize);
		flushMappedMemoryRange(vk, device, uniformBufAlloc[pass]->getMemory(), uniformBufAlloc[pass]->getOffset(), uniformBufSize);

		DescriptorSetUpdateBuilder()
			.writeSingle(*descriptorSet[pass], DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBufDesc[pass])
			.update(vk, device);

		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &*descriptorSet[pass], 0u, DE_NULL);
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(imageExtent2D));
		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		vk.cmdDraw(*cmdBuffer, 1u, 1u, 0u, 0u);
		endRenderPass(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &colorPassBarrier);

		if (dsUsed)
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &dsPassBarrier);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	zeroBuffer(vk, device, *colorBufferAlloc, colorBufferSize);
	zeroBuffer(vk, device, *depthBufferAlloc, depthBufferSize);
	zeroBuffer(vk, device, *stencilBufferAlloc, stencilBufferSize);

	beginCommandBuffer(vk, *cmdBuffer);
	{
		// Copy color image
		{
			const VkImageMemoryBarrier	preCopyBarrier	= makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
															VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorImage, colorSubresRange);
			const VkBufferImageCopy		region			= makeBufferImageCopy(params.image.size, makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, params.image.numLayers));
			const VkBufferMemoryBarrier	postCopyBarrier	= makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *colorBuffer, 0ull, VK_WHOLE_SIZE);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preCopyBarrier);
			vk.cmdCopyImageToBuffer(*cmdBuffer, *colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorBuffer, 1u, &region);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &postCopyBarrier, DE_NULL, 0u);
		}

		// Depth/Stencil image copy
		if (dsUsed)
		{
			const VkImageMemoryBarrier	preCopyBarrier		= makeImageMemoryBarrier(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
																VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *dsImage, dsSubresRange);
			const VkBufferImageCopy		depthCopyRegion		= makeBufferImageCopy(params.image.size, makeImageSubresourceLayers(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 0u, params.image.numLayers));
			const VkBufferImageCopy		stencilCopyRegion	= makeBufferImageCopy(params.image.size, makeImageSubresourceLayers(VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 0u, params.image.numLayers));
			const VkBufferMemoryBarrier	postCopyBarriers[]	=
			{
				makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *depthBuffer, 0ull, VK_WHOLE_SIZE),
				makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *stencilBuffer, 0ull, VK_WHOLE_SIZE),
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preCopyBarrier);
			vk.cmdCopyImageToBuffer(*cmdBuffer, *dsImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *depthBuffer, 1u, &depthCopyRegion);
			vk.cmdCopyImageToBuffer(*cmdBuffer, *dsImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *stencilBuffer, 1u, &stencilCopyRegion);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, DE_LENGTH_OF_ARRAY(postCopyBarriers), postCopyBarriers, DE_NULL, 0u);
		}
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	invalidateMappedMemoryRange(vk, device, colorBufferAlloc->getMemory(), colorBufferAlloc->getOffset(), colorBufferSize);
	invalidateMappedMemoryRange(vk, device, depthBufferAlloc->getMemory(), depthBufferAlloc->getOffset(), depthBufferSize);
	invalidateMappedMemoryRange(vk, device, stencilBufferAlloc->getMemory(), stencilBufferAlloc->getOffset(), stencilBufferSize);

	if (!verifyResults(context.getTestContext().getLog(), params, colorFormat, colorBufferAlloc->getHostPtr()))
		result += " Color";

	if (dsUsed)
	{
		if (!verifyResults(context.getTestContext().getLog(), params, dsFormat, depthBufferAlloc->getHostPtr(), true, false))
			result += " Depth";

		if (!verifyResults(context.getTestContext().getLog(), params, stencilBufferFormat, stencilBufferAlloc->getHostPtr(), false, true))
			result += " Stencil";
	}

	if (result.empty())
		return tcu::TestStatus::pass("OK");
	else
		return tcu::TestStatus::fail("Following parts of image are incorrect:" + result);
}

} // anonymous

tcu::TestCaseGroup* createLayeredRenderingTests (tcu::TestContext& testCtx)
{
	MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "layered", "Layered rendering tests."));

	const struct
	{
		TestType		test;
		const char*		name;
		const char*		description;
	} testTypes[] =
	{
		{ TEST_TYPE_DEFAULT_LAYER,					"render_to_default_layer",			"Render to the default layer"															},
		{ TEST_TYPE_SINGLE_LAYER,					"render_to_one",					"Render to one layer"																	},
		{ TEST_TYPE_ALL_LAYERS,						"render_to_all",					"Render to all layers"																	},
		{ TEST_TYPE_DIFFERENT_CONTENT,				"render_different_content",			"Render different data to different layers"												},
		{ TEST_TYPE_LAYER_ID,						"fragment_layer",					"Read gl_Layer in fragment shader"														},
		{ TEST_TYPE_INVOCATION_PER_LAYER,			"invocation_per_layer",				"Render to multiple layers with multiple invocations, one invocation per layer"			},
		{ TEST_TYPE_MULTIPLE_LAYERS_PER_INVOCATION,	"multiple_layers_per_invocation",	"Render to multiple layers with multiple invocations, multiple layers per invocation",	},
		{ TEST_TYPE_LAYERED_READBACK,				"readback",							"Render to multiple layers with two passes to check LOAD_OP_LOAD capability"			},
	};

	const ImageParams imageParams[] =
	{
		{ VK_IMAGE_VIEW_TYPE_1D_ARRAY,		{ 64,  1, 1 },	4	},
		{ VK_IMAGE_VIEW_TYPE_2D_ARRAY,		{ 64, 64, 1 },	4	},
		{ VK_IMAGE_VIEW_TYPE_CUBE,			{ 64, 64, 1 },	6	},
		{ VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,	{ 64, 64, 1 },	2*6	},
		{ VK_IMAGE_VIEW_TYPE_3D,			{ 64, 64, 8 },	1	}
	};

	for (int imageParamNdx = 0; imageParamNdx < DE_LENGTH_OF_ARRAY(imageParams); ++imageParamNdx)
	{
		MovePtr<tcu::TestCaseGroup> viewTypeGroup(new tcu::TestCaseGroup(testCtx, getShortImageViewTypeName(imageParams[imageParamNdx].viewType).c_str(), ""));

		for (int testTypeNdx = 0; testTypeNdx < DE_LENGTH_OF_ARRAY(testTypes); ++testTypeNdx)
		{
			const TestParams params =
			{
				testTypes[testTypeNdx].test,
				imageParams[imageParamNdx],
			};

			if (testTypes[testTypeNdx].test == TEST_TYPE_LAYERED_READBACK)
				addFunctionCaseWithPrograms(viewTypeGroup.get(), testTypes[testTypeNdx].name, testTypes[testTypeNdx].description, initPrograms, testLayeredReadBack, params);
			else
				addFunctionCaseWithPrograms(viewTypeGroup.get(), testTypes[testTypeNdx].name, testTypes[testTypeNdx].description, initPrograms, test, params);
		}

		group->addChild(viewTypeGroup.release());
	}

	return group.release();
}

} // geometry
} // vkt
