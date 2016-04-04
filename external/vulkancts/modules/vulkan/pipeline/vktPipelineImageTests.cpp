/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
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
 * \brief Image Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineImageTests.hpp"
#include "vktPipelineImageSamplingInstance.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktTestCase.hpp"
#include "vkImageUtil.hpp"
#include "vkPrograms.hpp"
#include "tcuTextureUtil.hpp"
#include "deStringUtil.hpp"

#include <sstream>
#include <vector>

namespace vkt
{
namespace pipeline
{

using namespace vk;
using de::MovePtr;

namespace
{

class ImageTest : public vkt::TestCase
{
public:
							ImageTest				(tcu::TestContext&	testContext,
													 const char*		name,
													 const char*		description,
													 VkImageViewType	imageViewType,
													 VkFormat			imageFormat,
													 const tcu::IVec3&	imageSize,
													 int				arraySize);

	virtual void			initPrograms			(SourceCollections& sourceCollections) const;
	virtual TestInstance*	createInstance			(Context& context) const;
	static std::string		getGlslSamplerType		(const tcu::TextureFormat& format, VkImageViewType type);

private:
	VkImageViewType			m_imageViewType;
	VkFormat				m_imageFormat;
	tcu::IVec3				m_imageSize;
	int						m_arraySize;
};

ImageTest::ImageTest (tcu::TestContext&	testContext,
					  const char*		name,
					  const char*		description,
					  VkImageViewType	imageViewType,
					  VkFormat			imageFormat,
					  const tcu::IVec3&	imageSize,
					  int				arraySize)

	: vkt::TestCase		(testContext, name, description)
	, m_imageViewType	(imageViewType)
	, m_imageFormat		(imageFormat)
	, m_imageSize		(imageSize)
	, m_arraySize		(arraySize)
{
}

void ImageTest::initPrograms (SourceCollections& sourceCollections) const
{
	std::ostringstream				vertexSrc;
	std::ostringstream				fragmentSrc;
	const char*						texCoordSwizzle	= DE_NULL;
	const tcu::TextureFormat		format			= (isCompressedFormat(m_imageFormat)) ? tcu::getUncompressedFormat(mapVkCompressedFormat(m_imageFormat))
																						  : mapVkFormat(m_imageFormat);

	// \note We don't want to perform normalization on any compressed formats.
	//		 In case of non-sRGB LDR ASTC it would lead to lack of coverage
	//		 as uncompressed format for that is f16 but values will be in range
	//		 0..1 already.
	const tcu::TextureFormatInfo	formatInfo		= (!isCompressedFormat(m_imageFormat) ? tcu::getTextureFormatInfo(format)
																						  : tcu::getTextureFormatInfo(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8)));

	switch (m_imageViewType)
	{
		case VK_IMAGE_VIEW_TYPE_1D:
			texCoordSwizzle = "x";
			break;
		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
		case VK_IMAGE_VIEW_TYPE_2D:
			texCoordSwizzle = "xy";
			break;
		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		case VK_IMAGE_VIEW_TYPE_3D:
		case VK_IMAGE_VIEW_TYPE_CUBE:
			texCoordSwizzle = "xyz";
			break;
		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
			texCoordSwizzle = "xyzw";
			break;
		default:
			DE_ASSERT(false);
			break;
	}

	vertexSrc << "#version 440\n"
			  << "layout(location = 0) in vec4 position;\n"
			  << "layout(location = 1) in vec4 texCoords;\n"
			  << "layout(location = 0) out highp vec4 vtxTexCoords;\n"
			  << "out gl_PerVertex {\n"
			  << "	vec4 gl_Position;\n"
			  << "};\n"
			  << "void main (void)\n"
			  << "{\n"
			  << "	gl_Position = position;\n"
			  << "	vtxTexCoords = texCoords;\n"
			  << "}\n";

	fragmentSrc << "#version 440\n"
				<< "layout(set = 0, binding = 0) uniform highp " << getGlslSamplerType(format, m_imageViewType) << " texSampler;\n"
				<< "layout(location = 0) in highp vec4 vtxTexCoords;\n"
				<< "layout(location = 0) out highp vec4 fragColor;\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "	fragColor = (texture(texSampler, vtxTexCoords." << texCoordSwizzle << std::scientific << ") * vec4" << formatInfo.lookupScale << ") + vec4" << formatInfo.lookupBias << ";\n"
				<< "}\n";

	sourceCollections.glslSources.add("tex_vert") << glu::VertexSource(vertexSrc.str());
	sourceCollections.glslSources.add("tex_frag") << glu::FragmentSource(fragmentSrc.str());
}

TestInstance* ImageTest::createInstance (Context& context) const
{
	tcu::UVec2 renderSize;

	if (m_imageViewType == VK_IMAGE_VIEW_TYPE_1D || m_imageViewType == VK_IMAGE_VIEW_TYPE_2D)
	{
		renderSize = tcu::UVec2((deUint32)m_imageSize.x(), (deUint32)m_imageSize.y());
	}
	else
	{
		// Draw a 3x2 grid of texture layers
		renderSize = tcu::UVec2((deUint32)m_imageSize.x() * 3, (deUint32)m_imageSize.y() * 2);
	}

	const std::vector<Vertex4Tex4>	vertices			= createTestQuadMosaic(m_imageViewType);
	const VkComponentMapping		componentMapping	= getFormatComponentMapping(m_imageFormat);
	const VkImageSubresourceRange	subresourceRange	=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,
		0u,
		(deUint32)deLog2Floor32(deMax32(m_imageSize.x(), deMax32(m_imageSize.y(), m_imageSize.z()))) + 1,
		0u,
		(deUint32)m_arraySize,
	};

	const VkSamplerCreateInfo samplerParams =
	{
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,									// VkStructureType			sType;
		DE_NULL,																// const void*				pNext;
		0u,																		// VkSamplerCreateFlags		flags;
		VK_FILTER_NEAREST,														// VkFilter					magFilter;
		VK_FILTER_NEAREST,														// VkFilter					minFilter;
		VK_SAMPLER_MIPMAP_MODE_NEAREST,											// VkSamplerMipmapMode		mipmapMode;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,									// VkSamplerAddressMode		addressModeU;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,									// VkSamplerAddressMode		addressModeV;
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,									// VkSamplerAddressMode		addressModeW;
		0.0f,																	// float					mipLodBias;
		VK_FALSE,																// VkBool32					anisotropyEnable;
		1.0f,																	// float					maxAnisotropy;
		false,																	// VkBool32					compareEnable;
		VK_COMPARE_OP_NEVER,													// VkCompareOp				compareOp;
		0.0f,																	// float					minLod;
		(float)(subresourceRange.levelCount - 1),								// float					maxLod;
		getFormatBorderColor(BORDER_COLOR_TRANSPARENT_BLACK, m_imageFormat),	// VkBorderColor			borderColor;
		false																	// VkBool32					unnormalizedCoordinates;
	};

	return new ImageSamplingInstance(context, renderSize, m_imageViewType, m_imageFormat, m_imageSize, m_arraySize, componentMapping, subresourceRange, samplerParams, 0.0f, vertices);
}

std::string ImageTest::getGlslSamplerType (const tcu::TextureFormat& format, VkImageViewType type)
{
	std::ostringstream samplerType;

	if (tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
		samplerType << "u";
	else if (tcu::getTextureChannelClass(format.type) == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER)
		samplerType << "i";

	switch (type)
	{
		case VK_IMAGE_VIEW_TYPE_1D:
			samplerType << "sampler1D";
			break;

		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
			samplerType << "sampler1DArray";
			break;

		case VK_IMAGE_VIEW_TYPE_2D:
			samplerType << "sampler2D";
			break;

		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
			samplerType << "sampler2DArray";
			break;

		case VK_IMAGE_VIEW_TYPE_3D:
			samplerType << "sampler3D";
			break;

		case VK_IMAGE_VIEW_TYPE_CUBE:
			samplerType << "samplerCube";
			break;

		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
			samplerType << "samplerCubeArray";
			break;

		default:
			DE_FATAL("Unknown image view type");
			break;
	}

	return samplerType.str();
}

std::string getFormatCaseName (const VkFormat format)
{
	const std::string	fullName	= getFormatName(format);

	DE_ASSERT(de::beginsWith(fullName, "VK_FORMAT_"));

	return de::toLower(fullName.substr(10));
}

std::string getSizeName (VkImageViewType viewType, const tcu::IVec3& size, int arraySize)
{
	std::ostringstream	caseName;

	switch (viewType)
	{
		case VK_IMAGE_VIEW_TYPE_1D:
		case VK_IMAGE_VIEW_TYPE_2D:
		case VK_IMAGE_VIEW_TYPE_CUBE:
			caseName << size.x() << "x" << size.y();
			break;

		case VK_IMAGE_VIEW_TYPE_3D:
			caseName << size.x() << "x" << size.y() << "x" << size.z();
			break;

		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
			caseName << size.x() << "x" << size.y() << "_array_of_" << arraySize;
			break;

		default:
			DE_ASSERT(false);
			break;
	}

	return caseName.str();
}

de::MovePtr<tcu::TestCaseGroup> createImageSizeTests (tcu::TestContext& testCtx, VkImageViewType imageViewType, VkFormat imageFormat)
{
	using tcu::IVec3;

	std::vector<IVec3>					imageSizes;
	std::vector<int>					arraySizes;
	de::MovePtr<tcu::TestCaseGroup>		imageSizeTests	(new tcu::TestCaseGroup(testCtx, "size", ""));

	// Select image imageSizes
	switch (imageViewType)
	{
		case VK_IMAGE_VIEW_TYPE_1D:
		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
			// POT
			imageSizes.push_back(IVec3(1, 1, 1));
			imageSizes.push_back(IVec3(2, 1, 1));
			imageSizes.push_back(IVec3(32, 1, 1));
			imageSizes.push_back(IVec3(128, 1, 1));
			imageSizes.push_back(IVec3(512, 1, 1));

			// NPOT
			imageSizes.push_back(IVec3(3, 1, 1));
			imageSizes.push_back(IVec3(13, 1, 1));
			imageSizes.push_back(IVec3(127, 1, 1));
			imageSizes.push_back(IVec3(443, 1, 1));
			break;

		case VK_IMAGE_VIEW_TYPE_2D:
		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
			// POT
			imageSizes.push_back(IVec3(1, 1, 1));
			imageSizes.push_back(IVec3(2, 2, 1));
			imageSizes.push_back(IVec3(32, 32, 1));

			// NPOT
			imageSizes.push_back(IVec3(3, 3, 1));
			imageSizes.push_back(IVec3(13, 13, 1));

			// POT rectangular
			imageSizes.push_back(IVec3(8, 16, 1));
			imageSizes.push_back(IVec3(32, 16, 1));

			// NPOT rectangular
			imageSizes.push_back(IVec3(13, 23, 1));
			imageSizes.push_back(IVec3(23, 8, 1));
			break;

		case VK_IMAGE_VIEW_TYPE_3D:
			// POT cube
			imageSizes.push_back(IVec3(1, 1, 1));
			imageSizes.push_back(IVec3(2, 2, 2));
			imageSizes.push_back(IVec3(16, 16, 16));

			// POT non-cube
			imageSizes.push_back(IVec3(32, 16, 8));
			imageSizes.push_back(IVec3(8, 16, 32));
			break;

		case VK_IMAGE_VIEW_TYPE_CUBE:
		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
			// POT
			imageSizes.push_back(IVec3(32, 32, 1));

			// NPOT
			imageSizes.push_back(IVec3(13, 13, 1));
			break;

		default:
			DE_ASSERT(false);
			break;
	}

	// Select array sizes
	switch (imageViewType)
	{
		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
			arraySizes.push_back(3);
			arraySizes.push_back(6);
			break;

		case VK_IMAGE_VIEW_TYPE_CUBE:
			arraySizes.push_back(6);
			break;

		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
			arraySizes.push_back(6);
			arraySizes.push_back(6 * 6);
			break;

		default:
			arraySizes.push_back(1);
			break;
	}

	for (size_t sizeNdx = 0; sizeNdx < imageSizes.size(); sizeNdx++)
	{
		for (size_t arraySizeNdx = 0; arraySizeNdx < arraySizes.size(); arraySizeNdx++)
		{
			imageSizeTests->addChild(new ImageTest(testCtx,
												   getSizeName(imageViewType, imageSizes[sizeNdx], arraySizes[arraySizeNdx]).c_str(),
												   "",
												   imageViewType,
												   imageFormat,
												   imageSizes[sizeNdx],
												   arraySizes[arraySizeNdx]));
		}
	}

	return imageSizeTests;
}

} // anonymous

tcu::TestCaseGroup* createImageTests (tcu::TestContext& testCtx)
{
	const struct
	{
		VkImageViewType		type;
		const char*			name;
	}
	imageViewTypes[] =
	{
		{ VK_IMAGE_VIEW_TYPE_1D,			"1d" },
		{ VK_IMAGE_VIEW_TYPE_1D_ARRAY,		"1d_array" },
		{ VK_IMAGE_VIEW_TYPE_2D,			"2d" },
		{ VK_IMAGE_VIEW_TYPE_2D_ARRAY,		"2d_array" },
		{ VK_IMAGE_VIEW_TYPE_3D,			"3d" },
		{ VK_IMAGE_VIEW_TYPE_CUBE,			"cube" },
		{ VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,	"cube_array" }
	};

	// All supported dEQP formats that are not intended for depth or stencil.
	const VkFormat formats[] =
	{
		VK_FORMAT_R4G4_UNORM_PACK8,
		VK_FORMAT_R4G4B4A4_UNORM_PACK16,
		VK_FORMAT_R5G6B5_UNORM_PACK16,
		VK_FORMAT_R5G5B5A1_UNORM_PACK16,
		VK_FORMAT_R8_UNORM,
		VK_FORMAT_R8_SNORM,
		VK_FORMAT_R8_USCALED,
		VK_FORMAT_R8_SSCALED,
		VK_FORMAT_R8_UINT,
		VK_FORMAT_R8_SINT,
		VK_FORMAT_R8_SRGB,
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8_SNORM,
		VK_FORMAT_R8G8_USCALED,
		VK_FORMAT_R8G8_SSCALED,
		VK_FORMAT_R8G8_UINT,
		VK_FORMAT_R8G8_SINT,
		VK_FORMAT_R8G8_SRGB,
		VK_FORMAT_R8G8B8_UNORM,
		VK_FORMAT_R8G8B8_SNORM,
		VK_FORMAT_R8G8B8_USCALED,
		VK_FORMAT_R8G8B8_SSCALED,
		VK_FORMAT_R8G8B8_UINT,
		VK_FORMAT_R8G8B8_SINT,
		VK_FORMAT_R8G8B8_SRGB,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_USCALED,
		VK_FORMAT_R8G8B8A8_SSCALED,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_SINT,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_A2R10G10B10_UNORM_PACK32,
		VK_FORMAT_A2R10G10B10_UINT_PACK32,
		VK_FORMAT_A2R10G10B10_USCALED_PACK32,
		VK_FORMAT_R16_UNORM,
		VK_FORMAT_R16_SNORM,
		VK_FORMAT_R16_USCALED,
		VK_FORMAT_R16_SSCALED,
		VK_FORMAT_R16_UINT,
		VK_FORMAT_R16_SINT,
		VK_FORMAT_R16_SFLOAT,
		VK_FORMAT_R16G16_UNORM,
		VK_FORMAT_R16G16_SNORM,
		VK_FORMAT_R16G16_USCALED,
		VK_FORMAT_R16G16_SSCALED,
		VK_FORMAT_R16G16_UINT,
		VK_FORMAT_R16G16_SINT,
		VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R16G16B16_UNORM,
		VK_FORMAT_R16G16B16_SNORM,
		VK_FORMAT_R16G16B16_USCALED,
		VK_FORMAT_R16G16B16_SSCALED,
		VK_FORMAT_R16G16B16_UINT,
		VK_FORMAT_R16G16B16_SINT,
		VK_FORMAT_R16G16B16_SFLOAT,
		VK_FORMAT_R16G16B16A16_UNORM,
		VK_FORMAT_R16G16B16A16_SNORM,
		VK_FORMAT_R16G16B16A16_USCALED,
		VK_FORMAT_R16G16B16A16_SSCALED,
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32_SINT,
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32_SINT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_UINT,
		VK_FORMAT_R32G32B32_SINT,
		VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R32G32B32A32_SINT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_FORMAT_B10G11R11_UFLOAT_PACK32,
		VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
		VK_FORMAT_B4G4R4A4_UNORM_PACK16,
		VK_FORMAT_B5G5R5A1_UNORM_PACK16,

		// Compressed formats
		VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
		VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
		VK_FORMAT_EAC_R11_UNORM_BLOCK,
		VK_FORMAT_EAC_R11_SNORM_BLOCK,
		VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
		VK_FORMAT_EAC_R11G11_SNORM_BLOCK,
		VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
		VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
		VK_FORMAT_ASTC_5x4_UNORM_BLOCK,
		VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
		VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
		VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
		VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
		VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
		VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
		VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
		VK_FORMAT_ASTC_8x5_UNORM_BLOCK,
		VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
		VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
		VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
		VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
		VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
		VK_FORMAT_ASTC_10x5_UNORM_BLOCK,
		VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
		VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
		VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
		VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
		VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
		VK_FORMAT_ASTC_10x10_UNORM_BLOCK,
		VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
		VK_FORMAT_ASTC_12x10_UNORM_BLOCK,
		VK_FORMAT_ASTC_12x10_SRGB_BLOCK,
		VK_FORMAT_ASTC_12x12_UNORM_BLOCK,
		VK_FORMAT_ASTC_12x12_SRGB_BLOCK,
	};

	de::MovePtr<tcu::TestCaseGroup> imageTests			(new tcu::TestCaseGroup(testCtx, "image", "Image tests"));
	de::MovePtr<tcu::TestCaseGroup> viewTypeTests		(new tcu::TestCaseGroup(testCtx, "view_type", ""));

	for (int viewTypeNdx = 0; viewTypeNdx < DE_LENGTH_OF_ARRAY(imageViewTypes); viewTypeNdx++)
	{
		const VkImageViewType			viewType		= imageViewTypes[viewTypeNdx].type;
		de::MovePtr<tcu::TestCaseGroup>	viewTypeGroup	(new tcu::TestCaseGroup(testCtx, imageViewTypes[viewTypeNdx].name, (std::string("Uses a ") + imageViewTypes[viewTypeNdx].name + " view").c_str()));
		de::MovePtr<tcu::TestCaseGroup>	formatTests		(new tcu::TestCaseGroup(testCtx, "format", "Tests samplable formats"));

		for (size_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); formatNdx++)
		{
			const VkFormat	format	= formats[formatNdx];

			if (isCompressedFormat(format))
			{
				// Do not use compressed formats with 1D and 1D array textures.
				if (viewType == VK_IMAGE_VIEW_TYPE_1D || viewType == VK_IMAGE_VIEW_TYPE_1D_ARRAY)
					break;
			}

			de::MovePtr<tcu::TestCaseGroup>	formatGroup	(new tcu::TestCaseGroup(testCtx,
																				getFormatCaseName(format).c_str(),
																				(std::string("Samples a texture of format ") + getFormatName(format)).c_str()));

			de::MovePtr<tcu::TestCaseGroup> sizeTests = createImageSizeTests(testCtx, viewType, format);

			formatGroup->addChild(sizeTests.release());
			formatTests->addChild(formatGroup.release());
		}

		viewTypeGroup->addChild(formatTests.release());
		viewTypeTests->addChild(viewTypeGroup.release());
	}

	imageTests->addChild(viewTypeTests.release());

	return imageTests.release();
}

} // pipeline
} // vkt
