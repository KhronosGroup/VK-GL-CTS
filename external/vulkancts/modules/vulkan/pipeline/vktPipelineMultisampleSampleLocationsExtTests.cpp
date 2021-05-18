/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 Advanced Micro Devices, Inc.
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
 * \file
 * \brief Tests for VK_EXT_sample_locations
 *//*--------------------------------------------------------------------*/

#include "vktPipelineMultisampleSampleLocationsExtTests.hpp"
#include "vktPipelineSampleLocationsUtil.hpp"
#include "vktPipelineMakeUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkPlatform.hpp"
#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkPrograms.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"
#include "deMath.h"

#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"
#include "tcuVectorUtil.hpp"

#include <string>
#include <vector>
#include <set>
#include <algorithm>

namespace vkt
{
namespace pipeline
{
namespace
{
using namespace vk;
using de::UniquePtr;
using de::MovePtr;
using tcu::Vec4;
using tcu::Vec2;
using tcu::UVec2;
using tcu::UVec4;
using tcu::RGBA;

static const deUint32		STENCIL_REFERENCE	= 1u;
static const float			DEPTH_CLEAR			= 1.0f;
static const float			DEPTH_REFERENCE     = 0.5f;
static const Vec4			CLEAR_COLOR_0		= Vec4(0.0f, 0.0f,  0.0f,  1.0f);
static const Vec4			CLEAR_COLOR_1		= Vec4(0.5f, 0.25f, 0.75f, 1.0f);
static const VkDeviceSize	ZERO				= 0u;

template<typename T>
inline const T* dataOrNullPtr (const std::vector<T>& v)
{
	return (v.empty() ? DE_NULL : &v[0]);
}

template<typename T>
inline T* dataOrNullPtr (std::vector<T>& v)
{
	return (v.empty() ? DE_NULL : &v[0]);
}

template<typename T>
inline void append (std::vector<T>& first, const std::vector<T>& second)
{
	first.insert(first.end(), second.begin(), second.end());
}

//! Order a Vector by X, Y, Z, and W
template<typename VectorT>
struct LessThan
{
	bool operator()(const VectorT& v1, const VectorT& v2) const
	{
		for (int i = 0; i < VectorT::SIZE; ++i)
		{
			if (v1[i] == v2[i])
				continue;
			else
				return v1[i] < v2[i];
		}

		return false;
	}
};

//! Similar to the class in vktTestCaseUtil.hpp, but uses Arg0 directly rather than through a InstanceFunction1
template<typename Arg0>
class FunctionProgramsSimple1
{
public:
	typedef void	(*Function)				(vk::SourceCollections& dst, Arg0 arg0);
					FunctionProgramsSimple1	(Function func) : m_func(func)							{}
	void			init					(vk::SourceCollections& dst, const Arg0& arg0) const	{ m_func(dst, arg0); }

private:
	const Function	m_func;
};

//! Convenience function to create a TestCase based on a freestanding initPrograms and a TestInstance implementation
template<typename Instance, typename Arg0>
void addInstanceTestCaseWithPrograms (tcu::TestCaseGroup*								group,
									  const std::string&								name,
									  const std::string&								desc,
									  typename FunctionSupport1<Arg0>::Function			checkSupport,
									  typename FunctionProgramsSimple1<Arg0>::Function	initPrograms,
									  Arg0												arg0)
{
	group->addChild(new InstanceFactory1WithSupport<Instance, Arg0, FunctionSupport1<Arg0>, FunctionProgramsSimple1<Arg0> >(
		group->getTestContext(), tcu::NODETYPE_SELF_VALIDATE, name, desc, FunctionProgramsSimple1<Arg0>(initPrograms), arg0, typename FunctionSupport1<Arg0>::Args(checkSupport, arg0)));
}

void checkSupportSampleLocations (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_sample_locations");
}

std::string getString (const VkSampleCountFlagBits sampleCount)
{
	std::ostringstream str;
	str << "samples_" << static_cast<deUint32>(sampleCount);
	return str.str();
}

bool isSupportedDepthStencilFormat (const InstanceInterface& vki, const VkPhysicalDevice physDevice, const VkFormat format)
{
	VkFormatProperties formatProps;
	vki.getPhysicalDeviceFormatProperties(physDevice, format, &formatProps);
	return (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
}

VkFormat findSupportedDepthStencilFormat (Context& context, const bool useDepth, const bool useStencil)
{
	const InstanceInterface&	vki			= context.getInstanceInterface();
	const VkPhysicalDevice		physDevice	= context.getPhysicalDevice();

	if (useDepth && !useStencil)
		return VK_FORMAT_D16_UNORM;		// must be supported

	// One of these formats must be supported.

	if (isSupportedDepthStencilFormat(vki, physDevice, VK_FORMAT_D24_UNORM_S8_UINT))
		return VK_FORMAT_D24_UNORM_S8_UINT;

	if (isSupportedDepthStencilFormat(vki, physDevice, VK_FORMAT_D32_SFLOAT_S8_UINT))
		return VK_FORMAT_D32_SFLOAT_S8_UINT;

	return VK_FORMAT_UNDEFINED;
}

VkImageAspectFlags getImageAspectFlags (const VkFormat format)
{
	const tcu::TextureFormat tcuFormat = mapVkFormat(format);

	if      (tcuFormat.order == tcu::TextureFormat::DS)		return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	else if (tcuFormat.order == tcu::TextureFormat::D)		return VK_IMAGE_ASPECT_DEPTH_BIT;
	else if (tcuFormat.order == tcu::TextureFormat::S)		return VK_IMAGE_ASPECT_STENCIL_BIT;

	DE_FATAL("Format not handled");
	return 0u;
}

VkPhysicalDeviceSampleLocationsPropertiesEXT getSampleLocationsPropertiesEXT (Context& context)
{
	const InstanceInterface&	vki				= context.getInstanceInterface();
	const VkPhysicalDevice		physicalDevice	= context.getPhysicalDevice();

	VkPhysicalDeviceSampleLocationsPropertiesEXT sampleLocationsProperties;
	deMemset(&sampleLocationsProperties, 0, sizeof(sampleLocationsProperties));

	sampleLocationsProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT;
	sampleLocationsProperties.pNext = DE_NULL;

	VkPhysicalDeviceProperties2 properties =
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,	    // VkStructureType               sType;
		&sampleLocationsProperties,								// void*                         pNext;
		VkPhysicalDeviceProperties(),							// VkPhysicalDeviceProperties    properties;
	};

	vki.getPhysicalDeviceProperties2(physicalDevice, &properties);

	return sampleLocationsProperties;
}

inline deUint32 numSamplesPerPixel (const MultisamplePixelGrid& pixelGrid)
{
	return static_cast<deUint32>(pixelGrid.samplesPerPixel());
}

inline VkSampleLocationsInfoEXT makeEmptySampleLocationsInfo ()
{
	const VkSampleLocationsInfoEXT info =
	{
		VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT,				// VkStructureType               sType;
		DE_NULL,													// const void*                   pNext;
		(VkSampleCountFlagBits)0,									// VkSampleCountFlagBits         sampleLocationsPerPixel;
		makeExtent2D(0,0),											// VkExtent2D                    sampleLocationGridSize;
		0,															// uint32_t                      sampleLocationsCount;
		DE_NULL,													// const VkSampleLocationEXT*    pSampleLocations;
	};
	return info;
}

void logPixelGrid (tcu::TestLog& log, const VkPhysicalDeviceSampleLocationsPropertiesEXT& sampleLocationsProperties, const MultisamplePixelGrid& pixelGrid)
{
	log << tcu::TestLog::Section("pixelGrid", "Multisample pixel grid configuration:")
		<< tcu::TestLog::Message << sampleLocationsProperties << tcu::TestLog::EndMessage
		<< tcu::TestLog::Message << "Specified grid size = " << pixelGrid.size() << tcu::TestLog::EndMessage;

	for (deUint32 gridY = 0; gridY < pixelGrid.size().y(); ++gridY)
	for (deUint32 gridX = 0; gridX < pixelGrid.size().x(); ++gridX)
	{
		log << tcu::TestLog::Message << "Pixel(" << gridX << ", " << gridY <<")" << tcu::TestLog::EndMessage;

		for (deUint32 sampleNdx = 0; sampleNdx < numSamplesPerPixel(pixelGrid); ++sampleNdx)
		{
			const VkSampleLocationEXT& loc = pixelGrid.getSample(gridX, gridY, sampleNdx);
			log << tcu::TestLog::Message << "* Sample(" << sampleNdx <<") = " << Vec2(loc.x, loc.y) << tcu::TestLog::EndMessage;
		}
	}

	log << tcu::TestLog::Message << "Sample locations visualization" << tcu::TestLog::EndMessage;

	{
		const deUint32		height	= deMinu32(1u << sampleLocationsProperties.sampleLocationSubPixelBits, 16u);	// increase if you want more precision
		const deUint32		width	= 2 * height;	// works well with a fixed-size font
		std::vector<char>	buffer	(width * height);

		for (deUint32 gridY = 0; gridY < pixelGrid.size().y(); ++gridY)
		for (deUint32 gridX = 0; gridX < pixelGrid.size().x(); ++gridX)
		{
			std::fill(buffer.begin(), buffer.end(), '.');

			for (deUint32 sampleNdx = 0; sampleNdx < numSamplesPerPixel(pixelGrid); ++sampleNdx)
			{
				const VkSampleLocationEXT&	loc		= pixelGrid.getSample(gridX, gridY, sampleNdx);
				const deUint32				ndx		= deMinu32(width  - 1, static_cast<deUint32>(static_cast<float>(width)  * loc.x)) +
													  deMinu32(height - 1, static_cast<deUint32>(static_cast<float>(height) * loc.y)) * width;
				const deUint32				evenNdx = ndx - ndx % 2;

				buffer[evenNdx    ] = '[';
				buffer[evenNdx + 1] = ']';
			}

			std::ostringstream str;
			str << "Pixel(" << gridX << ", " << gridY <<")\n";

			for (deUint32 lineNdx = 0; lineNdx < height; ++lineNdx)
			{
				str.write(&buffer[width * lineNdx], width);
				str << "\n";
			}

			log << tcu::TestLog::Message << str.str() << tcu::TestLog::EndMessage;
		}
	}

	log << tcu::TestLog::EndSection;
}

//! Place samples very close to each other
void fillSampleLocationsPacked (MultisamplePixelGrid& grid, const deUint32 subPixelBits)
{
	const deUint32	numLocations	= 1u << subPixelBits;
	const int		offset[3]		= { -1, 0, 1 };
	de::Random		rng				(214);

	for (deUint32 gridY = 0; gridY < grid.size().y(); ++gridY)
	for (deUint32 gridX = 0; gridX < grid.size().x(); ++gridX)
	{
		// Will start placing from this location
		const UVec2 baseLocationNdx (rng.getUint32() % numLocations,
									 rng.getUint32() % numLocations);
		UVec2		locationNdx		= baseLocationNdx;

		std::set<UVec2, LessThan<UVec2> >	takenLocationIndices;
		for (deUint32 sampleNdx = 0; sampleNdx < numSamplesPerPixel(grid); /* no increment */)
		{
			if (takenLocationIndices.find(locationNdx) == takenLocationIndices.end())
			{
				const VkSampleLocationEXT location =
				{
					static_cast<float>(locationNdx.x()) / static_cast<float>(numLocations),	// float x;
					static_cast<float>(locationNdx.y()) / static_cast<float>(numLocations),	// float y;
				};

				grid.setSample(gridX, gridY, sampleNdx, location);
				takenLocationIndices.insert(locationNdx);

				++sampleNdx;	// next sample
			}

			// Find next location by applying a small offset. Just keep iterating if a redundant location is chosen
			locationNdx.x() = static_cast<deUint32>(deClamp32(locationNdx.x() + offset[rng.getUint32() % DE_LENGTH_OF_ARRAY(offset)], 0u, numLocations - 1));
			locationNdx.y() = static_cast<deUint32>(deClamp32(locationNdx.y() + offset[rng.getUint32() % DE_LENGTH_OF_ARRAY(offset)], 0u, numLocations - 1));
		}
	}
}

//! Unorm/int compare, very low threshold as we are expecting near-exact values
bool compareGreenImage (tcu::TestLog& log, const char* name, const char* description, const tcu::ConstPixelBufferAccess& image)
{
	tcu::TextureLevel greenImage(image.getFormat(), image.getWidth(), image.getHeight());
	tcu::clear(greenImage.getAccess(), tcu::RGBA::green().toIVec());
	return tcu::intThresholdCompare(log, name, description, greenImage.getAccess(), image, tcu::UVec4(2u), tcu::COMPARE_LOG_RESULT);
}

//! Silent compare - no logging
bool intThresholdCompare (const tcu::ConstPixelBufferAccess& reference, const tcu::ConstPixelBufferAccess& result, const UVec4& threshold)
{
	using namespace tcu;

	int						width				= reference.getWidth();
	int						height				= reference.getHeight();
	int						depth				= reference.getDepth();
	UVec4					maxDiff				(0, 0, 0, 0);

	TCU_CHECK_INTERNAL(result.getWidth() == width && result.getHeight() == height && result.getDepth() == depth);

	for (int z = 0; z < depth; z++)
	{
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				IVec4	refPix	= reference.getPixelInt(x, y, z);
				IVec4	cmpPix	= result.getPixelInt(x, y, z);
				UVec4	diff	= abs(refPix - cmpPix).cast<deUint32>();

				maxDiff = max(maxDiff, diff);
			}
		}
	}

	return boolAll(lessThanEqual(maxDiff, threshold));
}

int countUniqueColors (const tcu::ConstPixelBufferAccess& image)
{
	std::set<Vec4, LessThan<Vec4> > colors;

	for (int y = 0; y < image.getHeight(); ++y)
	for (int x = 0; x < image.getWidth();  ++x)
	{
		colors.insert(image.getPixel(x, y));
	}

	return static_cast<int>(colors.size());
}

Move<VkImage> makeImage (const DeviceInterface&			vk,
						 const VkDevice					device,
						 const VkImageCreateFlags		flags,
						 const VkFormat					format,
						 const UVec2&					size,
						 const VkSampleCountFlagBits	samples,
						 const VkImageUsageFlags		usage)
{
	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		flags,											// VkImageCreateFlags		flags;
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

Move<VkEvent> makeEvent (const DeviceInterface& vk, const VkDevice device)
{
	const VkEventCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_EVENT_CREATE_INFO,		// VkStructureType       sType;
		DE_NULL,									// const void*           pNext;
		(VkEventCreateFlags)0,						// VkEventCreateFlags    flags;
	};
	return createEvent(vk, device, &createInfo);
}

//! Generate NDC space sample locations at each framebuffer pixel
//! Data is filled starting at pixel (0,0) and for each pixel there are numSamples locations
std::vector<Vec2> genFramebufferSampleLocations (const MultisamplePixelGrid& pixelGrid, const UVec2& gridSize, const UVec2& framebufferSize)
{
	std::vector<Vec2>	locations;

	for (deUint32 y			= 0; y			< framebufferSize.y();				++y)
	for (deUint32 x			= 0; x			< framebufferSize.x();				++x)
	for (deUint32 sampleNdx	= 0; sampleNdx	< numSamplesPerPixel(pixelGrid);	++sampleNdx)
	{
		const VkSampleLocationEXT&	location = pixelGrid.getSample(x % gridSize.x(), y % gridSize.y(), sampleNdx);
		const float					globalX  = location.x + static_cast<float>(x);
		const float					globalY  = location.y + static_cast<float>(y);

		// Transform to [-1, 1] space
		locations.push_back(Vec2(-1.0f + 2.0f * (globalX / static_cast<float>(framebufferSize.x())),
								 -1.0f + 2.0f * (globalY / static_cast<float>(framebufferSize.y()))));
	}

	return locations;
}

struct PositionColor
{
	tcu::Vec4	position;
	tcu::Vec4	color;

	PositionColor (const tcu::Vec4& pos, const tcu::Vec4& col) : position(pos), color(col) {}
};

std::vector<PositionColor> genVerticesFullQuad (const Vec4& color = Vec4(1.0f), const float z = 0.0f)
{
	const PositionColor vertices[] =
	{
		PositionColor(Vec4( 1.0f, -1.0f, z, 1.0f), color),
		PositionColor(Vec4(-1.0f, -1.0f, z, 1.0f), color),
		PositionColor(Vec4(-1.0f,  1.0f, z, 1.0f), color),

		PositionColor(Vec4(-1.0f,  1.0f, z, 1.0f), color),
		PositionColor(Vec4( 1.0f,  1.0f, z, 1.0f), color),
		PositionColor(Vec4( 1.0f, -1.0f, z, 1.0f), color),
	};

	return std::vector<PositionColor>(vertices, vertices + DE_LENGTH_OF_ARRAY(vertices));
}

//! Some abstract geometry with angled edges, to make multisampling visible.
std::vector<PositionColor> genVerticesShapes (const Vec4& color = Vec4(1.0f), const float z = 0.0f)
{
	std::vector<PositionColor> vertices;

	const float numSteps  = 16.0f;
	const float angleStep = (2.0f * DE_PI) / numSteps;

	for (float a = 0.0f; a <= 2.0f * DE_PI; a += angleStep)
	{
		vertices.push_back(PositionColor(Vec4(1.0f * deFloatCos(a),				1.0f * deFloatSin(a),				z, 1.0f), color));
		vertices.push_back(PositionColor(Vec4(0.1f * deFloatCos(a - angleStep), 0.1f * deFloatSin(a - angleStep),	z, 1.0f), color));
		vertices.push_back(PositionColor(Vec4(0.1f * deFloatCos(a + angleStep), 0.1f * deFloatSin(a + angleStep),	z, 1.0f), color));
	}

	return vertices;
}

//! Stencil op that only allows drawing over the cleared area of an attachment.
inline VkStencilOpState stencilOpStateDrawOnce (void)
{
	return makeStencilOpState(
		VK_STENCIL_OP_KEEP,		// stencil fail
		VK_STENCIL_OP_ZERO,		// depth & stencil pass
		VK_STENCIL_OP_KEEP,		// depth only fail
		VK_COMPARE_OP_EQUAL,	// compare op
		~0u,					// compare mask
		~0u,					// write mask
		STENCIL_REFERENCE);		// reference
}

//! Stencil op that simply increments the buffer with each passing test.
inline VkStencilOpState stencilOpStateIncrement(void)
{
	return makeStencilOpState(
		VK_STENCIL_OP_KEEP,						// stencil fail
		VK_STENCIL_OP_INCREMENT_AND_CLAMP,		// depth & stencil pass
		VK_STENCIL_OP_KEEP,						// depth only fail
		VK_COMPARE_OP_ALWAYS,					// compare op
		~0u,									// compare mask
		~0u,									// write mask
		STENCIL_REFERENCE);						// reference
}

//! A few preconfigured vertex attribute configurations
enum VertexInputConfig
{
	VERTEX_INPUT_NONE = 0u,
	VERTEX_INPUT_VEC4,
	VERTEX_INPUT_VEC4_VEC4,
};

//! Create a MSAA pipeline, with max per-sample shading
Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&				vk,
									   const VkDevice						device,
									   const std::vector<VkDynamicState>&	dynamicState,
									   const VkPipelineLayout				pipelineLayout,
									   const VkRenderPass					renderPass,
									   const VkShaderModule					vertexModule,
									   const VkShaderModule					fragmentModule,
									   const deUint32						subpassIndex,
									   const VkViewport&					viewport,
									   const VkRect2D						scissor,
									   const VkSampleCountFlagBits			numSamples,
									   const bool							useSampleLocations,
									   const VkSampleLocationsInfoEXT&		sampleLocationsInfo,
									   const bool							useDepth,
									   const bool							useStencil,
									   const VertexInputConfig				vertexInputConfig,
									   const VkPrimitiveTopology			topology,
									   const VkStencilOpState&				stencilOpState)
{
	std::vector<VkVertexInputBindingDescription>	vertexInputBindingDescriptions;
	std::vector<VkVertexInputAttributeDescription>	vertexInputAttributeDescriptions;

	const deUint32 sizeofVec4 = static_cast<deUint32>(sizeof(Vec4));

	switch (vertexInputConfig)
	{
		case VERTEX_INPUT_NONE:
			break;

		case VERTEX_INPUT_VEC4:
			vertexInputBindingDescriptions.push_back  (makeVertexInputBindingDescription  (0u, sizeofVec4, VK_VERTEX_INPUT_RATE_VERTEX));
			vertexInputAttributeDescriptions.push_back(makeVertexInputAttributeDescription(0u, 0u,		   VK_FORMAT_R32G32B32A32_SFLOAT, 0u));
			break;

		case VERTEX_INPUT_VEC4_VEC4:
			vertexInputBindingDescriptions.push_back  (makeVertexInputBindingDescription  (0u, 2u * sizeofVec4, VK_VERTEX_INPUT_RATE_VERTEX));
			vertexInputAttributeDescriptions.push_back(makeVertexInputAttributeDescription(0u, 0u,				VK_FORMAT_R32G32B32A32_SFLOAT, 0u));
			vertexInputAttributeDescriptions.push_back(makeVertexInputAttributeDescription(1u, 0u,				VK_FORMAT_R32G32B32A32_SFLOAT, sizeofVec4));
			break;

		default:
			DE_FATAL("Vertex input config not supported");
			break;
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

	const std::vector<VkViewport>	viewports	(1, viewport);
	const std::vector<VkRect2D>		scissors	(1, scissor);

	const VkPipelineSampleLocationsStateCreateInfoEXT pipelineSampleLocationsCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT,	// VkStructureType             sType;
		DE_NULL,															// const void*                 pNext;
		useSampleLocations,													// VkBool32                    sampleLocationsEnable;
		sampleLocationsInfo,												// VkSampleLocationsInfoEXT    sampleLocationsInfo;
	};

	const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
		&pipelineSampleLocationsCreateInfo,							// const void*								pNext;
		(VkPipelineMultisampleStateCreateFlags)0,					// VkPipelineMultisampleStateCreateFlags	flags;
		numSamples,													// VkSampleCountFlagBits					rasterizationSamples;
		VK_TRUE,													// VkBool32									sampleShadingEnable;
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
		useDepth,													// VkBool32									depthTestEnable;
		true,														// VkBool32									depthWriteEnable;
		VK_COMPARE_OP_LESS,											// VkCompareOp								depthCompareOp;
		VK_FALSE,													// VkBool32									depthBoundsTestEnable;
		useStencil,													// VkBool32									stencilTestEnable;
		stencilOpState,												// VkStencilOpState							front;
		stencilOpState,												// VkStencilOpState							back;
		0.0f,														// float									minDepthBounds;
		1.0f,														// float									maxDepthBounds;
	};

	const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,		// VkStructureType                      sType;
		DE_NULL,													// const void*                          pNext;
		(VkPipelineDynamicStateCreateFlags)0,						// VkPipelineDynamicStateCreateFlags    flags;
		static_cast<deUint32>(dynamicState.size()),					// uint32_t                             dynamicStateCount;
		dataOrNullPtr(dynamicState),								// const VkDynamicState*                pDynamicStates;
	};

	return makeGraphicsPipeline(vk,								// const DeviceInterface&                        vk
								device,							// const VkDevice                                device
								pipelineLayout,					// const VkPipelineLayout                        pipelineLayout
								vertexModule,					// const VkShaderModule                          vertexShaderModule
								DE_NULL,						// const VkShaderModule                          tessellationControlShaderModule
								DE_NULL,						// const VkShaderModule                          tessellationEvalShaderModule
								DE_NULL,						// const VkShaderModule                          geometryShaderModule
								fragmentModule,					// const VkShaderModule                          fragmentShaderModule
								renderPass,						// const VkRenderPass                            renderPass
								viewports,						// const std::vector<VkViewport>&                viewports
								scissors,						// const std::vector<VkRect2D>&                  scissors
								topology,						// const VkPrimitiveTopology                     topology
								subpassIndex,					// const deUint32                                subpass
								0u,								// const deUint32                                patchControlPoints
								&vertexInputStateInfo,			// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
								DE_NULL,						// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
								&pipelineMultisampleStateInfo,	// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
								&pipelineDepthStencilStateInfo,	// const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
								DE_NULL,						// const VkPipelineColorBlendStateCreateInfo*    colorBlendStateCreateInfo
								&dynamicStateCreateInfo);		// const VkPipelineDynamicStateCreateInfo*       dynamicStateCreateInfo
}

inline Move<VkPipeline> makeGraphicsPipelineSinglePassColor (const DeviceInterface&				vk,
															 const VkDevice						device,
															 const std::vector<VkDynamicState>&	dynamicState,
															 const VkPipelineLayout				pipelineLayout,
															 const VkRenderPass					renderPass,
															 const VkShaderModule				vertexModule,
															 const VkShaderModule				fragmentModule,
															 const VkViewport&					viewport,
															 const VkRect2D						scissor,
															 const VkSampleCountFlagBits		numSamples,
															 const bool							useSampleLocations,
															 const VkSampleLocationsInfoEXT&	sampleLocationsInfo,
															 const VertexInputConfig			vertexInputConfig,
															 const VkPrimitiveTopology			topology)
{
	return makeGraphicsPipeline(vk, device, dynamicState, pipelineLayout, renderPass, vertexModule, fragmentModule,
								/*subpass*/ 0u, viewport, scissor, numSamples, useSampleLocations, sampleLocationsInfo,
								/*depth test*/ false, /*stencil test*/ false, vertexInputConfig, topology, stencilOpStateIncrement());
}

//! Utility to build and maintain render pass, framebuffer and related resources.
//! Use bake() before using the render pass.
class RenderTarget
{
public:
	RenderTarget (void)
	{
		nextSubpass();
	}

	//! Returns an attachment index that is used to reference this attachment later
	deUint32 addAttachment (const VkImageView					imageView,
							const VkAttachmentDescriptionFlags	flags,
							const VkFormat						format,
							const VkSampleCountFlagBits			numSamples,
							const VkAttachmentLoadOp			loadOp,
							const VkAttachmentStoreOp			storeOp,
							const VkAttachmentLoadOp			stencilLoadOp,
							const VkAttachmentStoreOp			stencilStoreOp,
							const VkImageLayout					initialLayout,
							const VkImageLayout					finalLayout,
							const VkClearValue					clearValue,
							const VkSampleLocationsInfoEXT*		pInitialSampleLocations = DE_NULL)
	{
		const deUint32 index = static_cast<deUint32>(m_attachments.size());

		m_attachments.push_back(imageView);
		m_attachmentDescriptions.push_back(makeAttachmentDescription(
			flags,										// VkAttachmentDescriptionFlags		flags;
			format,										// VkFormat							format;
			numSamples,									// VkSampleCountFlagBits			samples;
			loadOp,										// VkAttachmentLoadOp				loadOp;
			storeOp,									// VkAttachmentStoreOp				storeOp;
			stencilLoadOp,								// VkAttachmentLoadOp				stencilLoadOp;
			stencilStoreOp,								// VkAttachmentStoreOp				stencilStoreOp;
			initialLayout,								// VkImageLayout					initialLayout;
			finalLayout									// VkImageLayout					finalLayout;
		));
		m_clearValues.push_back(clearValue);			// always add, even if unused

		if (pInitialSampleLocations)
		{
			const VkAttachmentSampleLocationsEXT attachmentSampleLocations =
			{
				index,						// uint32_t                    attachmentIndex;
				*pInitialSampleLocations,	// VkSampleLocationsInfoEXT    sampleLocationsInfo;
			};
			m_attachmentSampleLocations.push_back(attachmentSampleLocations);
		}

		return index;
	}

	void addSubpassColorAttachment (const deUint32 attachmentIndex, const VkImageLayout subpassLayout)
	{
		m_subpasses.back().colorAttachmentReferences.push_back(
			makeAttachmentReference(attachmentIndex, subpassLayout));
		m_subpasses.back().resolveAttachmentReferences.push_back(
			makeAttachmentReference(VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED));
	}

	void addSubpassColorAttachmentWithResolve (const deUint32 colorAttachmentIndex, const VkImageLayout colorSubpassLayout, const deUint32 resolveAttachmentIndex, const VkImageLayout resolveSubpassLayout)
	{
		m_subpasses.back().colorAttachmentReferences.push_back(
			makeAttachmentReference(colorAttachmentIndex, colorSubpassLayout));
		m_subpasses.back().resolveAttachmentReferences.push_back(
			makeAttachmentReference(resolveAttachmentIndex, resolveSubpassLayout));
	}

	void addSubpassDepthStencilAttachment (const deUint32 attachmentIndex, const VkImageLayout subpassLayout, const VkSampleLocationsInfoEXT* pSampleLocations = DE_NULL)
	{
		m_subpasses.back().depthStencilAttachmentReferences.push_back(
			makeAttachmentReference(attachmentIndex, subpassLayout));

		if (pSampleLocations)
		{
			const VkSubpassSampleLocationsEXT subpassSampleLocations =
			{
				static_cast<deUint32>(m_subpasses.size() - 1),		// uint32_t                    subpassIndex;
				*pSampleLocations,									// VkSampleLocationsInfoEXT    sampleLocationsInfo;
			};
			m_subpassSampleLocations.push_back(subpassSampleLocations);
		}
	}

	void addSubpassInputAttachment (const deUint32 attachmentIndex, const VkImageLayout subpassLayout)
	{
		m_subpasses.back().inputAttachmentReferences.push_back(
			makeAttachmentReference(attachmentIndex, subpassLayout));
	}

	void addSubpassPreserveAttachment (const deUint32 attachmentIndex)
	{
		m_subpasses.back().preserveAttachmentReferences.push_back(attachmentIndex);
	}

	void nextSubpass (void)
	{
		m_subpasses.push_back(SubpassDescription());
	}

	//! Create a RenderPass and Framebuffer based on provided attachments
	void bake (const DeviceInterface&							vk,
			   const VkDevice									device,
			   const UVec2&										framebufferSize)
	{
		DE_ASSERT(!m_renderPass);
		const deUint32 numSubpasses = static_cast<deUint32>(m_subpasses.size());

		std::vector<VkSubpassDescription>	subpassDescriptions;
		std::vector<VkSubpassDependency>	subpassDependencies;
		for (deUint32 subpassNdx = 0; subpassNdx < numSubpasses; ++subpassNdx)
		{
			const SubpassDescription&	sd			= m_subpasses[subpassNdx];
			const VkSubpassDescription	description	=
			{
				(VkSubpassDescriptionFlags)0,									// VkSubpassDescriptionFlags		flags;
				VK_PIPELINE_BIND_POINT_GRAPHICS,								// VkPipelineBindPoint				pipelineBindPoint;
				static_cast<deUint32>(sd.inputAttachmentReferences.size()),		// deUint32							inputAttachmentCount;
				dataOrNullPtr(sd.inputAttachmentReferences),					// const VkAttachmentReference*		pInputAttachments;
				static_cast<deUint32>(sd.colorAttachmentReferences.size()),		// deUint32							colorAttachmentCount;
				dataOrNullPtr(sd.colorAttachmentReferences),					// const VkAttachmentReference*		pColorAttachments;
				dataOrNullPtr(sd.resolveAttachmentReferences),					// const VkAttachmentReference*		pResolveAttachments;
				dataOrNullPtr(sd.depthStencilAttachmentReferences),				// const VkAttachmentReference*		pDepthStencilAttachment;
				static_cast<deUint32>(sd.preserveAttachmentReferences.size()),	// deUint32							preserveAttachmentCount;
				dataOrNullPtr(sd.preserveAttachmentReferences)					// const deUint32*					pPreserveAttachments;
			};
			subpassDescriptions.push_back(description);

			// Add a very coarse dependency enforcing sequential ordering of subpasses
			if (subpassNdx > 0)
			{
				static const VkAccessFlags	accessAny	= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
														| VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
														| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
														| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
														| VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
				const VkSubpassDependency	dependency	=
				{
					subpassNdx - 1,								// uint32_t                srcSubpass;
					subpassNdx,									// uint32_t                dstSubpass;
					VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,			// VkPipelineStageFlags    srcStageMask;
					VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,			// VkPipelineStageFlags    dstStageMask;
					accessAny,									// VkAccessFlags           srcAccessMask;
					accessAny,									// VkAccessFlags           dstAccessMask;
					(VkDependencyFlags)0,						// VkDependencyFlags       dependencyFlags;
				};
				subpassDependencies.push_back(dependency);
			}
		}
		// add a final dependency to synchronize results for the copy commands that will follow the renderpass
		const VkSubpassDependency finalDependency = {
			numSubpasses - 1,																			// uint32_t                srcSubpass;
			VK_SUBPASS_EXTERNAL,																		// uint32_t                dstSubpass;
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,	// VkPipelineStageFlags    srcStageMask;
			VK_PIPELINE_STAGE_TRANSFER_BIT,																// VkPipelineStageFlags    dstStageMask;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,		// VkAccessFlags           srcAccessMask;
			VK_ACCESS_TRANSFER_READ_BIT,																// VkAccessFlags           dstAccessMask;
			(VkDependencyFlags)0,																		// VkDependencyFlags       dependencyFlags;
		};
		subpassDependencies.push_back(finalDependency);

		const VkRenderPassCreateInfo renderPassInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,						// VkStructureType					sType;
			DE_NULL,														// const void*						pNext;
			(VkRenderPassCreateFlags)0,										// VkRenderPassCreateFlags			flags;
			static_cast<deUint32>(m_attachmentDescriptions.size()),			// deUint32							attachmentCount;
			dataOrNullPtr(m_attachmentDescriptions),						// const VkAttachmentDescription*	pAttachments;
			static_cast<deUint32>(subpassDescriptions.size()),				// deUint32							subpassCount;
			dataOrNullPtr(subpassDescriptions),								// const VkSubpassDescription*		pSubpasses;
			static_cast<deUint32>(subpassDependencies.size()),				// deUint32							dependencyCount;
			dataOrNullPtr(subpassDependencies)								// const VkSubpassDependency*		pDependencies;
		};

		m_renderPass  = createRenderPass(vk, device, &renderPassInfo);
		m_framebuffer = makeFramebuffer (vk, device, *m_renderPass, static_cast<deUint32>(m_attachments.size()), dataOrNullPtr(m_attachments), framebufferSize.x(), framebufferSize.y());
	}

	VkRenderPass getRenderPass (void) const
	{
		DE_ASSERT(m_renderPass);
		return *m_renderPass;
	}

	VkFramebuffer getFramebuffer (void) const
	{
		DE_ASSERT(m_framebuffer);
		return *m_framebuffer;
	}

	void recordBeginRenderPass (const DeviceInterface&	vk,
								const VkCommandBuffer	cmdBuffer,
								const VkRect2D&			renderArea,
								const VkSubpassContents	subpassContents) const
	{
		DE_ASSERT(m_renderPass);
		DE_ASSERT(m_framebuffer);

		const VkRenderPassSampleLocationsBeginInfoEXT renderPassSampleLocationsBeginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_SAMPLE_LOCATIONS_BEGIN_INFO_EXT,	// VkStructureType                          sType;
			DE_NULL,														// const void*                              pNext;
			static_cast<deUint32>(m_attachmentSampleLocations.size()),		// uint32_t                                 attachmentInitialSampleLocationsCount;
			dataOrNullPtr(m_attachmentSampleLocations),						// const VkAttachmentSampleLocationsEXT*    pAttachmentInitialSampleLocations;
			static_cast<deUint32>(m_subpassSampleLocations.size()),			// uint32_t                                 postSubpassSampleLocationsCount;
			dataOrNullPtr(m_subpassSampleLocations),						// const VkSubpassSampleLocationsEXT*       pPostSubpassSampleLocations;
		};

		const VkRenderPassBeginInfo renderPassBeginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,						// VkStructureType         sType;
			&renderPassSampleLocationsBeginInfo,							// const void*             pNext;
			*m_renderPass,													// VkRenderPass            renderPass;
			*m_framebuffer,													// VkFramebuffer           framebuffer;
			renderArea,														// VkRect2D                renderArea;
			static_cast<deUint32>(m_clearValues.size()),					// uint32_t                clearValueCount;
			dataOrNullPtr(m_clearValues),									// const VkClearValue*     pClearValues;
		};
		vk.cmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, subpassContents);
	}

private:
	struct SubpassDescription
	{
		std::vector<VkAttachmentReference>	inputAttachmentReferences;
		std::vector<VkAttachmentReference>	colorAttachmentReferences;
		std::vector<VkAttachmentReference>	resolveAttachmentReferences;
		std::vector<VkAttachmentReference>	depthStencilAttachmentReferences;
		std::vector<deUint32>				preserveAttachmentReferences;
	};

	std::vector<SubpassDescription>				m_subpasses;
	std::vector<VkImageView>					m_attachments;
	std::vector<VkAttachmentDescription>		m_attachmentDescriptions;
	std::vector<VkClearValue>					m_clearValues;
	std::vector<VkAttachmentSampleLocationsEXT>	m_attachmentSampleLocations;
	std::vector<VkSubpassSampleLocationsEXT>	m_subpassSampleLocations;
	Move<VkRenderPass>							m_renderPass;
	Move<VkFramebuffer>							m_framebuffer;

	// No copying allowed
	RenderTarget (const RenderTarget&);
	RenderTarget& operator=(const RenderTarget&);
};

void recordImageBarrier (const DeviceInterface&				vk,
						 const VkCommandBuffer				cmdBuffer,
						 const VkImage						image,
						 const VkImageAspectFlags			aspect,
						 const VkPipelineStageFlags			srcStageMask,
						 const VkPipelineStageFlags			dstStageMask,
						 const VkAccessFlags				srcAccessMask,
						 const VkAccessFlags				dstAccessMask,
						 const VkImageLayout				oldLayout,
						 const VkImageLayout				newLayout,
						 const VkSampleLocationsInfoEXT*	pSampleLocationsInfo = DE_NULL)
{
	const VkImageMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,						// VkStructureType            sType;
		pSampleLocationsInfo,										// const void*                pNext;
		srcAccessMask,												// VkAccessFlags              srcAccessMask;
		dstAccessMask,												// VkAccessFlags              dstAccessMask;
		oldLayout,													// VkImageLayout              oldLayout;
		newLayout,													// VkImageLayout              newLayout;
		VK_QUEUE_FAMILY_IGNORED,									// uint32_t                   srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,									// uint32_t                   dstQueueFamilyIndex;
		image,														// VkImage                    image;
		makeImageSubresourceRange(aspect, 0u, 1u, 0u, 1u),			// VkImageSubresourceRange    subresourceRange;
	};

	vk.cmdPipelineBarrier(cmdBuffer, srcStageMask, dstStageMask, (VkDependencyFlags)0, 0u, DE_NULL, 0u, DE_NULL, 1u, &barrier);
}

void recordWaitEventWithImage (const DeviceInterface&			vk,
							   const VkCommandBuffer			cmdBuffer,
							   const VkEvent					event,
							   const VkImage					image,
							   const VkImageAspectFlags			aspect,
							   const VkPipelineStageFlags		srcStageMask,
							   const VkPipelineStageFlags		dstStageMask,
							   const VkAccessFlags				srcAccessMask,
							   const VkAccessFlags				dstAccessMask,
							   const VkImageLayout				oldLayout,
							   const VkImageLayout				newLayout,
							   const VkSampleLocationsInfoEXT*	pSampleLocationsInfo = DE_NULL)
{
	const VkImageMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,						// VkStructureType            sType;
		pSampleLocationsInfo,										// const void*                pNext;
		srcAccessMask,												// VkAccessFlags              srcAccessMask;
		dstAccessMask,												// VkAccessFlags              dstAccessMask;
		oldLayout,													// VkImageLayout              oldLayout;
		newLayout,													// VkImageLayout              newLayout;
		VK_QUEUE_FAMILY_IGNORED,									// uint32_t                   srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,									// uint32_t                   dstQueueFamilyIndex;
		image,														// VkImage                    image;
		makeImageSubresourceRange(aspect, 0u, 1u, 0u, 1u),			// VkImageSubresourceRange    subresourceRange;
	};

	vk.cmdWaitEvents(
		cmdBuffer,													// VkCommandBuffer                             commandBuffer,
		1u,															// uint32_t                                    eventCount,
		&event,														// const VkEvent*                              pEvents,
		srcStageMask,												// VkPipelineStageFlags                        srcStageMask,
		dstStageMask,												// VkPipelineStageFlags                        dstStageMask,
		0u,															// uint32_t                                    memoryBarrierCount,
		DE_NULL,													// const VkMemoryBarrier*                      pMemoryBarriers,
		0u,															// uint32_t                                    bufferMemoryBarrierCount,
		DE_NULL,													// const VkBufferMemoryBarrier*                pBufferMemoryBarriers,
		1u,															// uint32_t                                    imageMemoryBarrierCount,
		&barrier);													// const VkImageMemoryBarrier*                 pImageMemoryBarriers);
}

void recordCopyImageToBuffer (const DeviceInterface&	vk,
							  const VkCommandBuffer		cmdBuffer,
							  const UVec2&				imageSize,
							  const VkImage				srcImage,
							  const VkBuffer			dstBuffer)
{
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

void recordClearAttachments (const DeviceInterface&		vk,
							 const VkCommandBuffer		cmdBuffer,
							 const deUint32				colorAttachment,
							 const VkClearValue&		colorClearValue,
							 const VkImageAspectFlags	depthStencilAspect,
							 const VkClearValue&		depthStencilClearValue,
							 const VkRect2D&			clearRect)
{
	std::vector<VkClearAttachment> attachments;

	const VkClearRect rect =
	{
		clearRect,					// VkRect2D    rect;
		0u,							// uint32_t    baseArrayLayer;
		1u,							// uint32_t    layerCount;
	};

	// Clear color
	{
		const VkClearAttachment attachment =
		{
			VK_IMAGE_ASPECT_COLOR_BIT,	// VkImageAspectFlags    aspectMask;
			colorAttachment,			// uint32_t              colorAttachment;
			colorClearValue,			// VkClearValue          clearValue;
		};
		attachments.push_back(attachment);
	}

	if ((depthStencilAspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0u)
	{
		const VkClearAttachment attachment =
		{
			depthStencilAspect,			// VkImageAspectFlags    aspectMask;
			VK_ATTACHMENT_UNUSED,		// uint32_t              colorAttachment;
			depthStencilClearValue,		// VkClearValue          clearValue;
		};
		attachments.push_back(attachment);
	}

	vk.cmdClearAttachments(cmdBuffer, static_cast<deUint32>(attachments.size()), dataOrNullPtr(attachments), 1u, &rect);
}

//! Suitable for executing in a render pass, no queries
void beginSecondaryCommandBuffer (const DeviceInterface&	vk,
								  const VkCommandBuffer		commandBuffer,
								  const VkRenderPass		renderPass,
								  const deUint32			subpass,
								  const VkFramebuffer		framebuffer)
{
	const VkCommandBufferInheritanceInfo inheritanceInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,		// VkStructureType                  sType;
		DE_NULL,												// const void*                      pNext;
		renderPass,												// VkRenderPass                     renderPass;
		subpass,												// uint32_t                         subpass;
		framebuffer,											// VkFramebuffer                    framebuffer;
		VK_FALSE,												// VkBool32                         occlusionQueryEnable;
		(VkQueryControlFlags)0,									// VkQueryControlFlags              queryFlags;
		(VkQueryPipelineStatisticFlags)0,						// VkQueryPipelineStatisticFlags    pipelineStatistics;
	};
	const VkCommandBufferBeginInfo beginInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,		// VkStructureType                          sType;
		DE_NULL,											// const void*                              pNext;
		(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		|VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT),	// VkCommandBufferUsageFlags                flags;
		&inheritanceInfo,									// const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
	};
	VK_CHECK(vk.beginCommandBuffer(commandBuffer, &beginInfo));
}

//! Verify results of a VkPhysicalDeviceSampleLocationsPropertiesEXT query with VkPhysicalDeviceProperties2KHR
tcu::TestStatus testQuerySampleLocationProperties (Context& context)
{
	const VkPhysicalDeviceSampleLocationsPropertiesEXT sampleLocationsProperties = getSampleLocationsPropertiesEXT(context);

	context.getTestContext().getLog()
		<< tcu::TestLog::Section("VkPhysicalDeviceSampleLocationsPropertiesEXT", "Query results")
		<< tcu::TestLog::Message << sampleLocationsProperties << tcu::TestLog::EndMessage
		<< tcu::TestLog::EndSection;

	const VkSampleCountFlags allowedSampleCounts = (VK_SAMPLE_COUNT_2_BIT  |
													VK_SAMPLE_COUNT_4_BIT  |
													VK_SAMPLE_COUNT_8_BIT  |
													VK_SAMPLE_COUNT_16_BIT |
													VK_SAMPLE_COUNT_32_BIT |
													VK_SAMPLE_COUNT_64_BIT);

	if ((sampleLocationsProperties.sampleLocationSampleCounts & allowedSampleCounts) == 0)
	{
		return tcu::TestStatus::fail("VkPhysicalDeviceSampleLocationsPropertiesEXT: sampleLocationSampleCounts should specify at least one MSAA sample count");
	}

	if (sampleLocationsProperties.maxSampleLocationGridSize.width  == 0u     ||
		sampleLocationsProperties.maxSampleLocationGridSize.height == 0u     ||
		sampleLocationsProperties.maxSampleLocationGridSize.width  >  16384u || // max not specified, but try to catch nonsense values like -1
		sampleLocationsProperties.maxSampleLocationGridSize.height >  16384u)
	{
		return tcu::TestStatus::fail("VkPhysicalDeviceSampleLocationsPropertiesEXT: maxSampleLocationGridSize must be at least (1,1) size");
	}

	for (int i = 0; i < 2; ++i)
	{
		if (sampleLocationsProperties.sampleLocationCoordinateRange[i] < 0.0f ||
			sampleLocationsProperties.sampleLocationCoordinateRange[i] > 1.0f)
		{
			return tcu::TestStatus::fail("VkPhysicalDeviceSampleLocationsPropertiesEXT: sampleLocationCoordinateRange[] values must be in [0, 1] range");
		}
	}

	if (sampleLocationsProperties.sampleLocationSubPixelBits == 0u  ||
		sampleLocationsProperties.sampleLocationSubPixelBits >  64u)	// max not specified, but try to catch nonsense values
	{
		return tcu::TestStatus::fail("VkPhysicalDeviceSampleLocationsPropertiesEXT: sampleLocationSubPixelBits should be greater than 0");
	}

	return tcu::TestStatus::pass("Pass");
}

//! Verify results of vkGetPhysicalDeviceMultisamplePropertiesEXT queries
tcu::TestStatus testQueryMultisampleProperties (Context& context)
{
	const InstanceInterface&	vki				= context.getInstanceInterface();
	const VkPhysicalDevice		physicalDevice	= context.getPhysicalDevice();
	tcu::TestLog&				log				= context.getTestContext().getLog();

	const VkPhysicalDeviceSampleLocationsPropertiesEXT sampleLocationsProperties = getSampleLocationsPropertiesEXT(context);

	const VkSampleCountFlagBits	sampleCountRange[] =
	{
		VK_SAMPLE_COUNT_1_BIT,
		VK_SAMPLE_COUNT_2_BIT,
		VK_SAMPLE_COUNT_4_BIT,
		VK_SAMPLE_COUNT_8_BIT,
		VK_SAMPLE_COUNT_16_BIT,
		VK_SAMPLE_COUNT_32_BIT,
		VK_SAMPLE_COUNT_64_BIT,
	};

	bool allOk = true;

	for (const VkSampleCountFlagBits* pLoopNumSamples = sampleCountRange; pLoopNumSamples < DE_ARRAY_END(sampleCountRange); ++pLoopNumSamples)
	{
		VkMultisamplePropertiesEXT multisampleProperties =
		{
			VK_STRUCTURE_TYPE_MULTISAMPLE_PROPERTIES_EXT,		// VkStructureType    sType;
			DE_NULL,											// void*              pNext;
			VkExtent2D(),										// VkExtent2D         maxSampleLocationGridSize;
		};

		vki.getPhysicalDeviceMultisamplePropertiesEXT(physicalDevice, *pLoopNumSamples, &multisampleProperties);

		log << tcu::TestLog::Section("getPhysicalDeviceMultisamplePropertiesEXT", "Query results")
			<< tcu::TestLog::Message << "Sample count: " << *pLoopNumSamples << tcu::TestLog::EndMessage
			<< tcu::TestLog::Message << multisampleProperties << tcu::TestLog::EndMessage;

		const bool isSupportedSampleCount = (*pLoopNumSamples & sampleLocationsProperties.sampleLocationSampleCounts) != 0;

		if (isSupportedSampleCount)
		{
			if (!(multisampleProperties.maxSampleLocationGridSize.width  >= sampleLocationsProperties.maxSampleLocationGridSize.width &&
				  multisampleProperties.maxSampleLocationGridSize.height >= sampleLocationsProperties.maxSampleLocationGridSize.height))
			{
				allOk = false;
				log << tcu::TestLog::Message
					<< "FAIL: Grid size should be the same or larger than VkPhysicalDeviceSampleLocationsPropertiesEXT::maxSampleLocationGridSize"
					<< tcu::TestLog::EndMessage;
			}
		}
		else
		{
			if (!(multisampleProperties.maxSampleLocationGridSize.width  == 0u &&
				  multisampleProperties.maxSampleLocationGridSize.height == 0u))
			{
				allOk = false;
				log << tcu::TestLog::Message << "FAIL: Expected (0, 0) grid size" << tcu::TestLog::EndMessage;
			}
		}

		log << tcu::TestLog::EndSection;
	}

	return allOk ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Some values were incorrect");
}

// These tests only use a color attachment and focus on per-sample data
namespace VerifySamples
{

//! Data layout used in verify sample locations and interpolation cases
namespace SampleDataSSBO
{

static VkDeviceSize	STATIC_SIZE		= 6 * sizeof(deUint32);

static UVec2&		renderSize		(void* const basePtr) { return *reinterpret_cast<UVec2*>	(static_cast<deUint8*>(basePtr) + 0 * sizeof(deUint32)); }
static UVec2&		gridSize		(void* const basePtr) { return *reinterpret_cast<UVec2*>	(static_cast<deUint8*>(basePtr) + 2 * sizeof(deUint32)); }
static deUint32&	samplesPerPixel	(void* const basePtr) { return *reinterpret_cast<deUint32*>	(static_cast<deUint8*>(basePtr) + 4 * sizeof(deUint32)); }

template<typename T>
static T*			sampleData		(void* const basePtr) { DE_STATIC_ASSERT(sizeof(T) == sizeof(Vec2));
															return  reinterpret_cast<T*>		(static_cast<deUint8*>(basePtr) + STATIC_SIZE); }

} // SampleDataSSBO

enum TestOptionFlagBits
{
	TEST_OPTION_DYNAMIC_STATE_BIT	= 0x1,	//!< Use dynamic pipeline state to pass in sample locations
	TEST_OPTION_CLOSELY_PACKED_BIT	= 0x2,	//!< Place samples as close as possible to each other
};
typedef deUint32 TestOptionFlags;

struct TestParams
{
	VkSampleCountFlagBits	numSamples;
	TestOptionFlags			options;
};

void checkSupportVerifyTests (Context& context, const TestParams params)
{
	checkSupportSampleLocations(context);

	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);

	if ((context.getDeviceProperties().limits.framebufferColorSampleCounts & params.numSamples) == 0u)
		TCU_THROW(NotSupportedError, "framebufferColorSampleCounts: sample count not supported");

	if ((getSampleLocationsPropertiesEXT(context).sampleLocationSampleCounts & params.numSamples) == 0u)
		TCU_THROW(NotSupportedError, "VkPhysicalDeviceSampleLocationsPropertiesEXT: sample count not supported");
}


std::string declareSampleDataSSBO (void)
{
	std::ostringstream str;
	str << "layout(set = 0, binding = 0, std430) readonly buffer SampleData {\n"	// make sure this matches SampleDataSSBO definition
		<< "    uvec2 renderSize;\n"
		<< "    uvec2 gridSize;\n"
		<< "    uint  samplesPerPixel;\n"
		<< "          // padding 1-uint size;\n"
		<< "    vec2  data[];\n"
		<< "} sb_data;\n";
	return str.str();
};

void addProgramsVerifyLocationGeometry (SourceCollections& programCollection, const TestParams)
{
	// Vertex shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in vec4 in_position;\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "    vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    gl_Position = in_position;\n"
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
			<< declareSampleDataSSBO()
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    uvec2 fragCoord = uvec2(gl_FragCoord.xy);\n"
			<< "    uint  index     = (fragCoord.y * sb_data.renderSize.x + fragCoord.x) * sb_data.samplesPerPixel + gl_SampleID;\n"
			<< "\n"
			<< "    if (gl_PrimitiveID == index)\n"
			<< "        o_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
			<< "    else\n"
			<< "        o_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

void addProgramsVerifyInterpolation (SourceCollections& programCollection, const TestParams)
{
	// Vertex shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in  vec4 in_position;\n"
			<< "layout(location = 0) out vec2 o_position;\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "    vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    gl_Position = in_position;\n"
			<< "    o_position  = in_position.xy;\n"	// user-data that will be interpolated
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment shader
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) sample in  vec2 in_value;\n"
			<< "layout(location = 0)        out vec4 o_color;\n"
			<< "\n"
			<< declareSampleDataSSBO()
			<< "\n"
			<< "void main(void)\n"
			<< "{\n"
			<< "    uvec2 fragCoord         = uvec2(gl_FragCoord.xy);\n"
			<< "    uint  index             = (fragCoord.y * sb_data.renderSize.x + fragCoord.x) * sb_data.samplesPerPixel + gl_SampleID;\n"
			<< "    vec2  diff              = abs(sb_data.data[index] - in_value);\n"
			<< "    vec2  threshold         = vec2(0.002);\n"
			<< "\n"
			<< "    if (all(lessThan(diff, threshold)))\n"
			<< "        o_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
			<< "    else\n"
			<< "        o_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

class TestBase : public TestInstance
{
public:
	TestBase (Context& context, const TestParams params)
		: TestInstance					(context)
		, m_params						(params)
		, m_sampleLocationsProperties	(getSampleLocationsPropertiesEXT(context))
		, m_colorFormat					(VK_FORMAT_R8G8B8A8_UNORM)
		, m_numVertices					(0)
		, m_currentGridNdx				(0)
	{
		VkMultisamplePropertiesEXT multisampleProperties =
		{
			VK_STRUCTURE_TYPE_MULTISAMPLE_PROPERTIES_EXT,		// VkStructureType    sType;
			DE_NULL,											// void*              pNext;
			VkExtent2D(),										// VkExtent2D         maxSampleLocationGridSize;
		};

		m_context.getInstanceInterface().getPhysicalDeviceMultisamplePropertiesEXT(m_context.getPhysicalDevice(), m_params.numSamples, &multisampleProperties);

		// Generate grid size combinations
		for (deUint32 y = multisampleProperties.maxSampleLocationGridSize.height; y >= 1u; y >>= 1)
		for (deUint32 x = multisampleProperties.maxSampleLocationGridSize.width;  x >= 1u; x >>= 1)
		{
			DE_ASSERT(multisampleProperties.maxSampleLocationGridSize.width  % x == 0u);
			DE_ASSERT(multisampleProperties.maxSampleLocationGridSize.height % y == 0u);
			m_gridSizes.push_back(UVec2(x, y));
		}
	}

	tcu::TestStatus iterate (void)
	{
		// Will be executed several times, for all possible pixel grid sizes
		if (!(currentGridSize().x() >= 1 && currentGridSize().y() >= 1))
			return tcu::TestStatus::fail("maxSampleLocationGridSize is invalid");

		// Prepare the pixel grid
		{
			const deUint32	pixelGridRepetitions = 2;	// just to make sure the pattern is consistently applied across the framebuffer
			m_renderSize = UVec2(pixelGridRepetitions * currentGridSize().x(),
								 pixelGridRepetitions * currentGridSize().y());
			m_pixelGrid = MovePtr<MultisamplePixelGrid>(new MultisamplePixelGrid(currentGridSize(), m_params.numSamples));

			if ((m_params.options & TEST_OPTION_CLOSELY_PACKED_BIT) != 0u)
				fillSampleLocationsPacked(*m_pixelGrid, m_sampleLocationsProperties.sampleLocationSubPixelBits);
			else
				fillSampleLocationsRandom(*m_pixelGrid, m_sampleLocationsProperties.sampleLocationSubPixelBits);

			logPixelGrid (m_context.getTestContext().getLog(), m_sampleLocationsProperties, *m_pixelGrid);
		}

		// Create images
		{
			const DeviceInterface&	vk			= m_context.getDeviceInterface();
			const VkDevice			device		= m_context.getDevice();
			Allocator&				allocator	= m_context.getDefaultAllocator();

			// Images and staging buffers

			m_colorImage		= makeImage(vk, device, (VkImageCreateFlags)0, m_colorFormat, m_renderSize, m_params.numSamples, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
			m_colorImageAlloc	= bindImage(vk, device, allocator, *m_colorImage, MemoryRequirement::Any);
			m_colorImageView	= makeImageView(vk, device, *m_colorImage, VK_IMAGE_VIEW_TYPE_2D, m_colorFormat, makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));

			m_resolveImage		= makeImage(vk, device, (VkImageCreateFlags)0, m_colorFormat, m_renderSize, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
			m_resolveImageAlloc	= bindImage(vk, device, allocator, *m_resolveImage, MemoryRequirement::Any);
			m_resolveImageView	= makeImageView(vk, device, *m_resolveImage, VK_IMAGE_VIEW_TYPE_2D, m_colorFormat, makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));

			const VkDeviceSize	colorBufferSize = m_renderSize.x() * m_renderSize.y() * tcu::getPixelSize(mapVkFormat(m_colorFormat));
			m_colorBuffer		= makeBuffer(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
			m_colorBufferAlloc	= bindBuffer(vk, device, allocator, *m_colorBuffer, MemoryRequirement::HostVisible);
		}

		if (!testPixelGrid())
			return tcu::TestStatus::fail("Fail");

		if (shrinkCurrentGrid())
			return tcu::TestStatus::incomplete();
		else
			return tcu::TestStatus::pass("Pass");
	}

protected:
	//! Return true if the test passed the current grid size
	virtual bool testPixelGrid (void) = 0;

	const UVec2& currentGridSize (void)
	{
		return m_gridSizes[m_currentGridNdx];
	}

	//! Return false if the grid is already at (1, 1) size
	bool shrinkCurrentGrid (void)
	{
		if (m_gridSizes.size() <= m_currentGridNdx + 1)
			return false;

		++m_currentGridNdx;
		return true;
	}

	void drawSinglePass (const VertexInputConfig vertexInputConfig)
	{
		DE_ASSERT(m_descriptorSetLayout);

		const DeviceInterface&			vk				= m_context.getDeviceInterface();
		const VkDevice					device			= m_context.getDevice();
		const VkViewport				viewport		= makeViewport(m_renderSize);
		const VkRect2D					renderArea		= makeRect2D(m_renderSize);
		const VkRect2D					scissor			= makeRect2D(m_renderSize);
		const Unique<VkShaderModule>	vertexModule	(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
		const Unique<VkShaderModule>	fragmentModule	(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u));
		const Unique<VkPipelineLayout>	pipelineLayout	(makePipelineLayout(vk, device, *m_descriptorSetLayout));

		const bool						useDynamicStateSampleLocations	= ((m_params.options & TEST_OPTION_DYNAMIC_STATE_BIT) != 0u);
		const VkSampleLocationsInfoEXT	sampleLocationsInfo				= makeSampleLocationsInfo(*m_pixelGrid);

		RenderTarget rt;

		rt.addAttachment(
			*m_colorImageView,											// VkImageView					imageView,
			(VkAttachmentDescriptionFlags)0,							// VkAttachmentDescriptionFlags	flags,
			m_colorFormat,												// VkFormat						format,
			m_params.numSamples,										// VkSampleCountFlagBits		numSamples,
			VK_ATTACHMENT_LOAD_OP_CLEAR,								// VkAttachmentLoadOp			loadOp,
			VK_ATTACHMENT_STORE_OP_STORE,								// VkAttachmentStoreOp			storeOp,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,							// VkAttachmentLoadOp			stencilLoadOp,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,							// VkAttachmentStoreOp			stencilStoreOp,
			VK_IMAGE_LAYOUT_UNDEFINED,									// VkImageLayout				initialLayout,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,					// VkImageLayout				finalLayout,
			makeClearValueColor(CLEAR_COLOR_0));						// VkClearValue					clearValue,

		rt.addAttachment(
			*m_resolveImageView,										// VkImageView					imageView,
			(VkAttachmentDescriptionFlags)0,							// VkAttachmentDescriptionFlags	flags,
			m_colorFormat,												// VkFormat						format,
			VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits		numSamples,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,							// VkAttachmentLoadOp			loadOp,
			VK_ATTACHMENT_STORE_OP_STORE,								// VkAttachmentStoreOp			storeOp,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,							// VkAttachmentLoadOp			stencilLoadOp,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,							// VkAttachmentStoreOp			stencilStoreOp,
			VK_IMAGE_LAYOUT_UNDEFINED,									// VkImageLayout				initialLayout,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,						// VkImageLayout				finalLayout,
			VkClearValue());											// VkClearValue					clearValue,

		rt.addSubpassColorAttachmentWithResolve(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
												1u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		rt.bake(vk, device, m_renderSize);

		Move<VkPipeline> pipeline;

		if (useDynamicStateSampleLocations)
		{
			std::vector<VkDynamicState>	dynamicState;
			dynamicState.push_back(VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT);

			pipeline = makeGraphicsPipelineSinglePassColor(
				vk, device, dynamicState, *pipelineLayout, rt.getRenderPass(), *vertexModule, *fragmentModule, viewport, scissor,
				m_params.numSamples, /*use sample locations*/ true, makeEmptySampleLocationsInfo(), vertexInputConfig, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		}
		else
		{
			pipeline = makeGraphicsPipelineSinglePassColor(
				vk, device, std::vector<VkDynamicState>(), *pipelineLayout, rt.getRenderPass(), *vertexModule, *fragmentModule, viewport, scissor,
				m_params.numSamples, /*use sample locations*/ true, sampleLocationsInfo, vertexInputConfig, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		}

		const Unique<VkCommandPool>		cmdPool		(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, m_context.getUniversalQueueFamilyIndex()));
		const Unique<VkCommandBuffer>	cmdBuffer	(makeCommandBuffer(vk, device, *cmdPool));

		beginCommandBuffer(vk, *cmdBuffer);

		rt.recordBeginRenderPass(vk, *cmdBuffer, renderArea, VK_SUBPASS_CONTENTS_INLINE);

		vk.cmdBindVertexBuffers(*cmdBuffer, /*first binding*/ 0u, /*num bindings*/ 1u, &m_vertexBuffer.get(), /*offsets*/ &ZERO);
		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

		if (useDynamicStateSampleLocations)
			vk.cmdSetSampleLocationsEXT(*cmdBuffer, &sampleLocationsInfo);

		if (m_descriptorSet)
			vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &m_descriptorSet.get(), 0u, DE_NULL);

		vk.cmdDraw(*cmdBuffer, m_numVertices, 1u, 0u, 0u);
		endRenderPass(vk, *cmdBuffer);

		recordCopyImageToBuffer(vk, *cmdBuffer, m_renderSize, *m_resolveImage, *m_colorBuffer);

		endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *cmdBuffer);

		invalidateAlloc(vk, device, *m_colorBufferAlloc);
	}

	void createSampleDataBufferAndDescriptors (const VkDeviceSize bufferSize)
	{
		// Make sure the old descriptor set is destroyed before we destroy its pool
		m_descriptorSet	= Move<VkDescriptorSet>();

		const DeviceInterface&	vk			= m_context.getDeviceInterface();
		const VkDevice			device		= m_context.getDevice();
		Allocator&				allocator	= m_context.getDefaultAllocator();

		m_sampleDataBuffer		= makeBuffer(vk, device, bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		m_sampleDataBufferAlloc	= bindBuffer(vk, device, allocator, *m_sampleDataBuffer, MemoryRequirement::HostVisible);

		m_descriptorSetLayout = DescriptorSetLayoutBuilder()
			.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build(vk, device);

		m_descriptorPool = DescriptorPoolBuilder()
			.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
			.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

		m_descriptorSet = makeDescriptorSet(vk, device, *m_descriptorPool, *m_descriptorSetLayout);

		const VkDescriptorBufferInfo bufferDescriptorInfo = makeDescriptorBufferInfo(*m_sampleDataBuffer, 0ull, bufferSize);
		DescriptorSetUpdateBuilder()
			.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo)
			.update(vk, device);

		SampleDataSSBO::renderSize		(m_sampleDataBufferAlloc->getHostPtr()) = m_renderSize;
		SampleDataSSBO::gridSize		(m_sampleDataBufferAlloc->getHostPtr()) = m_pixelGrid->size();
		SampleDataSSBO::samplesPerPixel	(m_sampleDataBufferAlloc->getHostPtr()) = m_pixelGrid->samplesPerPixel();

		flushAlloc(vk, device, *m_sampleDataBufferAlloc);
	}

	template<typename Vertex>
	void createVertexBuffer (const std::vector<Vertex>& vertices)
	{
		const DeviceInterface&  vk                  = m_context.getDeviceInterface();
		const VkDevice			device				= m_context.getDevice();
		Allocator&				allocator			= m_context.getDefaultAllocator();
		const VkDeviceSize      vertexBufferSize    = static_cast<VkDeviceSize>(vertices.size() * sizeof(vertices[0]));

		m_numVertices       = static_cast<deUint32>(vertices.size());
		m_vertexBuffer      = makeBuffer(vk, device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		m_vertexBufferAlloc = bindBuffer(vk, device, allocator, *m_vertexBuffer, MemoryRequirement::HostVisible);

		deMemcpy(m_vertexBufferAlloc->getHostPtr(), dataOrNullPtr(vertices), static_cast<std::size_t>(vertexBufferSize));
		flushAlloc(vk, device, *m_vertexBufferAlloc);
	}

	const TestParams									m_params;
	const VkPhysicalDeviceSampleLocationsPropertiesEXT	m_sampleLocationsProperties;
	const VkFormat										m_colorFormat;
	UVec2												m_renderSize;
	MovePtr<MultisamplePixelGrid>						m_pixelGrid;
	deUint32											m_numVertices;
	Move<VkBuffer>										m_vertexBuffer;
	MovePtr<Allocation>									m_vertexBufferAlloc;
	Move<VkImage>										m_colorImage;
	Move<VkImageView>									m_colorImageView;
	MovePtr<Allocation>									m_colorImageAlloc;
	Move<VkImage>										m_resolveImage;
	Move<VkImageView>									m_resolveImageView;
	MovePtr<Allocation>									m_resolveImageAlloc;
	Move<VkBuffer>										m_colorBuffer;
	MovePtr<Allocation>									m_colorBufferAlloc;
	Move<VkBuffer>										m_sampleDataBuffer;
	MovePtr<Allocation>									m_sampleDataBufferAlloc;
	Move<VkDescriptorSetLayout>							m_descriptorSetLayout;
	Move<VkDescriptorPool>								m_descriptorPool;
	Move<VkDescriptorSet>								m_descriptorSet;

private:
	deUint32											m_currentGridNdx;
	std::vector<UVec2>									m_gridSizes;
};

//! Check that each custom sample has the expected position
class VerifyLocationTest : public TestBase
{
public:
	VerifyLocationTest (Context& context, const TestParams params) : TestBase(context, params) {}

	bool testPixelGrid (void)
	{
		// Create vertices
		{
			// For each sample location (in the whole framebuffer), create a sub-pixel triangle that contains it.
			// NDC viewport size is 2.0 in X and Y and NDC pixel width/height depends on the framebuffer resolution.
			const Vec2			pixelSize	= Vec2(2.0f) / m_renderSize.cast<float>();
			const Vec2			offset		= pixelSize / UVec2(1u << m_sampleLocationsProperties.sampleLocationSubPixelBits).cast<float>();
			std::vector<Vec4>	vertices;

			// Surround with a roughly centered triangle
			const float y1 = 0.5f  * offset.y();
			const float y2 = 0.35f * offset.y();
			const float x1 = 0.5f  * offset.x();

			const std::vector<Vec2>	locations = genFramebufferSampleLocations(*m_pixelGrid, m_pixelGrid->size(), m_renderSize);
			for (std::vector<Vec2>::const_iterator iter = locations.begin(); iter != locations.end(); ++iter)
			{
				vertices.push_back(Vec4(iter->x(),      iter->y() - y1, 0.0f, 1.0f));
				vertices.push_back(Vec4(iter->x() - x1, iter->y() + y2, 0.0f, 1.0f));
				vertices.push_back(Vec4(iter->x() + x1, iter->y() + y2, 0.0f, 1.0f));
			}

			createVertexBuffer(vertices);
		}

		createSampleDataBufferAndDescriptors(SampleDataSSBO::STATIC_SIZE);	// no per-sample data used

		drawSinglePass(VERTEX_INPUT_VEC4);	// sample locations are taken from the pixel grid

		// Verify

		const tcu::ConstPixelBufferAccess image (tcu::ConstPixelBufferAccess(mapVkFormat(m_colorFormat), tcu::IVec3(m_renderSize.x(), m_renderSize.y(), 1), m_colorBufferAlloc->getHostPtr()));

		return compareGreenImage(m_context.getTestContext().getLog(), "resolve0", "Resolved test image", image);
	}
};

//! Verify that vertex attributes are correctly interpolated at each custom sample location
class VerifyInterpolationTest : public TestBase
{
public:
	VerifyInterpolationTest (Context& context, const TestParams params) : TestBase(context, params)	{}

	bool testPixelGrid (void)
	{
		createVertexBuffer(genVerticesFullQuad());

		// Create sample data SSBO
		{
			const deUint32		numSamples		= m_pixelGrid->samplesPerPixel();
			const deUint32		numDataEntries	= numSamples * m_renderSize.x() * m_renderSize.y();
			const VkDeviceSize  bufferSize		= SampleDataSSBO::STATIC_SIZE + sizeof(Vec2) * numDataEntries;

			createSampleDataBufferAndDescriptors(bufferSize);

			Vec2* const				pSampleData	= SampleDataSSBO::sampleData<Vec2>(m_sampleDataBufferAlloc->getHostPtr());
			const std::vector<Vec2>	locations	= genFramebufferSampleLocations(*m_pixelGrid, m_pixelGrid->size(), m_renderSize);

			// Fill SSBO with interpolated values (here: from -1.0 to 1.0 across the render area in both x and y)
			DE_ASSERT(locations.size() == numDataEntries);
			std::copy(locations.begin(), locations.end(), pSampleData);

			flushAlloc(m_context.getDeviceInterface(), m_context.getDevice(), *m_sampleDataBufferAlloc);
		}

		drawSinglePass(VERTEX_INPUT_VEC4_VEC4);	// sample locations are taken from the pixel grid

		// Verify

		const tcu::ConstPixelBufferAccess image (tcu::ConstPixelBufferAccess(mapVkFormat(m_colorFormat), tcu::IVec3(m_renderSize.x(), m_renderSize.y(), 1), m_colorBufferAlloc->getHostPtr()));

		return compareGreenImage(m_context.getTestContext().getLog(), "resolve0", "Resolved test image", image);
	}
};

template<typename Test, typename ProgramsFunc>
void addCases (tcu::TestCaseGroup* group, const VkSampleCountFlagBits numSamples, const ProgramsFunc initPrograms)
{
	TestParams params;
	deMemset(&params, 0, sizeof(params));

	params.numSamples	= numSamples;
	params.options		= (TestOptionFlags)0;

	addInstanceTestCaseWithPrograms<Test>(group, getString(numSamples).c_str(), "", checkSupportVerifyTests, initPrograms, params);

	params.options = (TestOptionFlags)TEST_OPTION_DYNAMIC_STATE_BIT;
	addInstanceTestCaseWithPrograms<Test>(group, (getString(numSamples) + "_dynamic").c_str(), "", checkSupportVerifyTests, initPrograms, params);

	params.options = (TestOptionFlags)TEST_OPTION_CLOSELY_PACKED_BIT;
	addInstanceTestCaseWithPrograms<Test>(group, (getString(numSamples) + "_packed").c_str(), "", checkSupportVerifyTests, initPrograms, params);
}

} // VerifySamples

// Draw tests with at least two "passes" where sample locations may change.
// Test case is based on a combination of parameters defined below. Not all combinations are compatible.
namespace Draw
{

//! Options common to all test cases
enum TestOptionFlagBits
{
	TEST_OPTION_SAME_PATTERN_BIT				= 1u << 0,	//!< Use the same sample pattern for all operations
	TEST_OPTION_DYNAMIC_STATE_BIT				= 1u << 1,	//!< Use dynamic pipeline state to pass in sample locations
	TEST_OPTION_SECONDARY_COMMAND_BUFFER_BIT	= 1u << 2,	//!< Put drawing commands in a secondary buffer, including sample locations change (if dynamic)
	TEST_OPTION_GENERAL_LAYOUT_BIT				= 1u << 3,	//!< Transition the image to general layout at some point in rendering
	TEST_OPTION_WAIT_EVENTS_BIT					= 1u << 4,	//!< Use image memory barriers with vkCmdWaitEvents rather than vkCmdPipelineBarrier
};
typedef deUint32 TestOptionFlags;

//! Determines where draws/clears with custom samples occur in the test
enum TestDrawIn
{
	TEST_DRAW_IN_RENDER_PASSES = 0u,	//!< Each operation in a separate render pass
	TEST_DRAW_IN_SUBPASSES,				//!< Each operation in a separate subpass of the same render pass
	TEST_DRAW_IN_SAME_SUBPASS,			//!< Each operation in the same subpass
};

//! How a clear before the second pass will be done
enum TestClears
{
	TEST_CLEARS_NO_CLEAR = 0u,				//!< Don't clear
	TEST_CLEARS_LOAD_OP_CLEAR,				//!< Render pass attachment load clear
	TEST_CLEARS_CMD_CLEAR_ATTACHMENTS,		//!< vkCmdClearAttachments within a subpass
	TEST_CLEARS_CMD_CLEAR_IMAGE,			//!< vkCmdClear{Color|DepthStencil}Image outside a render pass
};

//! What type of image will be verified with custom samples
enum TestImageAspect
{
	TEST_IMAGE_ASPECT_COLOR = 0u,			//!< Color image
	TEST_IMAGE_ASPECT_DEPTH,				//!< Depth aspect of an image (can be mixed format)
	TEST_IMAGE_ASPECT_STENCIL,				//!< Stencil aspect of an image (can be mixed format)
};

struct TestParams
{
	VkSampleCountFlagBits	numSamples;
	TestOptionFlags			options;
	TestDrawIn				drawIn;
	TestClears				clears;
	TestImageAspect			imageAspect;
};

void checkSupportDrawTests (Context& context, const TestParams params)
{
	checkSupportSampleLocations(context);

	if ((context.getDeviceProperties().limits.framebufferColorSampleCounts & params.numSamples) == 0u)
		TCU_THROW(NotSupportedError, "framebufferColorSampleCounts: sample count not supported");

	if ((getSampleLocationsPropertiesEXT(context).sampleLocationSampleCounts & params.numSamples) == 0u)
		TCU_THROW(NotSupportedError, "VkPhysicalDeviceSampleLocationsPropertiesEXT: sample count not supported");

	// Are we allowed to modify the sample pattern within the same subpass?
	if (params.drawIn == TEST_DRAW_IN_SAME_SUBPASS && ((params.options & TEST_OPTION_SAME_PATTERN_BIT) == 0) && !getSampleLocationsPropertiesEXT(context).variableSampleLocations)
		TCU_THROW(NotSupportedError, "VkPhysicalDeviceSampleLocationsPropertiesEXT: variableSampleLocations not supported");

	if (TEST_OPTION_WAIT_EVENTS_BIT & params.options &&
		context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") && !context.getPortabilitySubsetFeatures().events)
	{
		TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Events are not supported by this implementation");
	}
}

const char* getString (const TestImageAspect aspect)
{
	switch (aspect)
	{
		case TEST_IMAGE_ASPECT_COLOR:	return "color";
		case TEST_IMAGE_ASPECT_DEPTH:	return "depth";
		case TEST_IMAGE_ASPECT_STENCIL:	return "stencil";
	}
	DE_ASSERT(0);
	return DE_NULL;
}

const char* getString (const TestDrawIn drawIn)
{
	switch (drawIn)
	{
		case TEST_DRAW_IN_RENDER_PASSES:	return "separate_renderpass";
		case TEST_DRAW_IN_SUBPASSES:		return "separate_subpass";
		case TEST_DRAW_IN_SAME_SUBPASS:		return "same_subpass";
	}
	DE_ASSERT(0);
	return DE_NULL;
}

const char* getString (const TestClears clears)
{
	switch (clears)
	{
		case TEST_CLEARS_NO_CLEAR:				return "no_clear";
		case TEST_CLEARS_LOAD_OP_CLEAR:			return "load_op_clear";
		case TEST_CLEARS_CMD_CLEAR_ATTACHMENTS:	return "clear_attachments";
		case TEST_CLEARS_CMD_CLEAR_IMAGE:		return "clear_image";
	}
	DE_ASSERT(0);
	return DE_NULL;
}

std::string getTestOptionFlagsString (const deUint32 flags)
{
	std::ostringstream str;

	if ((flags & TEST_OPTION_SAME_PATTERN_BIT) != 0)				str << (str.tellp() > 0 ? "_" : "") << "same_pattern";
	if ((flags & TEST_OPTION_DYNAMIC_STATE_BIT) != 0)				str << (str.tellp() > 0 ? "_" : "") << "dynamic";
	if ((flags & TEST_OPTION_SECONDARY_COMMAND_BUFFER_BIT) != 0)	str << (str.tellp() > 0 ? "_" : "") << "secondary_cmd_buf";
	if ((flags & TEST_OPTION_GENERAL_LAYOUT_BIT) != 0)				str << (str.tellp() > 0 ? "_" : "") << "general_layout";
	if ((flags & TEST_OPTION_WAIT_EVENTS_BIT) != 0)					str << (str.tellp() > 0 ? "_" : "") << "event";

	return str.str();
}

void initPrograms (SourceCollections& programCollection, const TestParams)
{
	// Vertex shader
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
			<< "\n"
			// We use instance index to draw the left shape (index = 0) or the right shape (index = 1).
			// Vertices are squished and moved to either half of the viewport.
			<< "    if (gl_InstanceIndex == 0)\n"
			<< "        gl_Position.x = 0.5 * (gl_Position.x - 1.0);\n"
			<< "    else if (gl_InstanceIndex == 1)\n"
			<< "        gl_Position.x = 0.5 * (gl_Position.x + 1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment shader
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
}

//! Draw shapes using changing sample patterns. Add clears and other operations as necessary
class DrawTest : public TestInstance
{
	static const deUint32 NUM_PASSES = 2u;

public:
	DrawTest (Context& context, const TestParams params)
		: TestInstance					(context)
		, m_params						(params)
		, m_sampleLocationsProperties	(getSampleLocationsPropertiesEXT(context))
		, m_renderSize					(64, 32)
		, m_numVertices					(0)
		, m_colorFormat					(VK_FORMAT_R8G8B8A8_UNORM)
		, m_depthStencilFormat			(VK_FORMAT_UNDEFINED)
		, m_depthStencilAspect			(0)
	{
		VkMultisamplePropertiesEXT multisampleProperties =
		{
			VK_STRUCTURE_TYPE_MULTISAMPLE_PROPERTIES_EXT,		// VkStructureType    sType;
			DE_NULL,											// void*              pNext;
			VkExtent2D(),										// VkExtent2D         maxSampleLocationGridSize;
		};

		// For this test always use the full pixel grid

		m_context.getInstanceInterface().getPhysicalDeviceMultisamplePropertiesEXT(m_context.getPhysicalDevice(), m_params.numSamples, &multisampleProperties);
		m_gridSize.x() = multisampleProperties.maxSampleLocationGridSize.width;
		m_gridSize.y() = multisampleProperties.maxSampleLocationGridSize.height;
	}

	tcu::TestStatus iterate (void)
	{
		// Requirements
		if (!(m_gridSize.x() >= 1 && m_gridSize.y() >= 1))
			return tcu::TestStatus::fail("maxSampleLocationGridSize is invalid");

		// Images
		{
			const DeviceInterface&	vk					 = m_context.getDeviceInterface();
			const VkDevice			device				 = m_context.getDevice();
			Allocator&				allocator			 = m_context.getDefaultAllocator();
			const VkImageUsageFlags	colorImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

			m_colorImage		= makeImage(vk, device, (VkImageCreateFlags)0, m_colorFormat, m_renderSize, m_params.numSamples, colorImageUsageFlags);
			m_colorImageAlloc	= bindImage(vk, device, allocator, *m_colorImage, MemoryRequirement::Any);
			m_colorImageView	= makeImageView(vk, device, *m_colorImage, VK_IMAGE_VIEW_TYPE_2D, m_colorFormat,
												makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));

			m_resolveImage		= makeImage(vk, device, (VkImageCreateFlags)0, m_colorFormat, m_renderSize, VK_SAMPLE_COUNT_1_BIT, colorImageUsageFlags);
			m_resolveImageAlloc	= bindImage(vk, device, allocator, *m_resolveImage, MemoryRequirement::Any);
			m_resolveImageView	= makeImageView(vk, device, *m_resolveImage, VK_IMAGE_VIEW_TYPE_2D, m_colorFormat,
												makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));

			const VkDeviceSize	colorBufferSize = m_renderSize.x() * m_renderSize.y() * tcu::getPixelSize(mapVkFormat(m_colorFormat));
			m_colorBuffer		= makeBuffer(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
			m_colorBufferAlloc	= bindBuffer(vk, device, allocator, *m_colorBuffer, MemoryRequirement::HostVisible);

			if (m_params.imageAspect != TEST_IMAGE_ASPECT_COLOR)
			{
				const VkImageUsageFlags depthStencilImageUsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

				m_depthStencilFormat	 = findSupportedDepthStencilFormat(m_context, useDepth(), useStencil());
				m_depthStencilAspect	 = (useDepth()   ? VK_IMAGE_ASPECT_DEPTH_BIT   : (VkImageAspectFlagBits)0) |
										   (useStencil() ? VK_IMAGE_ASPECT_STENCIL_BIT : (VkImageAspectFlagBits)0);
				m_depthStencilImage		 = makeImage(vk, device, VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT,
													 m_depthStencilFormat, m_renderSize, m_params.numSamples, depthStencilImageUsageFlags);
				m_depthStencilImageAlloc = bindImage(vk, device, allocator, *m_depthStencilImage, MemoryRequirement::Any);
				m_depthStencilImageView	 = makeImageView(vk, device, *m_depthStencilImage, VK_IMAGE_VIEW_TYPE_2D, m_depthStencilFormat,
														 makeImageSubresourceRange(m_depthStencilAspect, 0u, 1u, 0u, 1u));
			}
		}

		// Vertices
		{
			const DeviceInterface&	vk			= m_context.getDeviceInterface();
			const VkDevice			device		= m_context.getDevice();
			Allocator&				allocator	= m_context.getDefaultAllocator();

			std::vector<PositionColor> vertices;

			if (useDepth())
			{
				append(vertices, genVerticesShapes  (RGBA::black().toVec(), DEPTH_REFERENCE / 2.0f));	// mask above (z = 0.0 is nearest)
				append(vertices, genVerticesFullQuad(RGBA::white().toVec(), DEPTH_REFERENCE));			// fill below the mask, using the depth test
			}
			else if (useStencil())
			{
				append(vertices, genVerticesShapes  (RGBA::black().toVec(), DEPTH_REFERENCE));			// first mask
				append(vertices, genVerticesFullQuad(RGBA::white().toVec(), DEPTH_REFERENCE / 2.0f));	// then fill the whole area, using the stencil test
			}
			else
				vertices = genVerticesShapes();

			const VkDeviceSize vertexBufferSize = static_cast<VkDeviceSize>(vertices.size() * sizeof(vertices[0]));

			m_numVertices       = static_cast<deUint32>(vertices.size());
			m_vertexBuffer      = makeBuffer(vk, device, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
			m_vertexBufferAlloc = bindBuffer(vk, device, allocator, *m_vertexBuffer, MemoryRequirement::HostVisible);

			deMemcpy(m_vertexBufferAlloc->getHostPtr(), dataOrNullPtr(vertices), static_cast<std::size_t>(vertexBufferSize));
			flushAlloc(vk, device, *m_vertexBufferAlloc);
		}

		// Multisample pixel grids - set up two sample patterns for two draw passes
		{
			const deUint32 numGrids = (useSameSamplePattern() ? 1u : NUM_PASSES);
			m_pixelGrids.reserve(numGrids);

			for (deUint32 passNdx = 0u; passNdx < numGrids; ++passNdx)
			{
				const deUint32 seed = 142u + 75u * passNdx;
				m_pixelGrids.push_back(MultisamplePixelGrid(m_gridSize, m_params.numSamples));
				fillSampleLocationsRandom(m_pixelGrids.back(), m_sampleLocationsProperties.sampleLocationSubPixelBits, seed);
				logPixelGrid (m_context.getTestContext().getLog(), m_sampleLocationsProperties, m_pixelGrids.back());
			}
		}

		// Some test cases will not clear the left hand image, so we can use it directly
		const bool isClearCase		= (m_params.clears != TEST_CLEARS_NO_CLEAR);
		const bool hasLeftSideImage = (!isClearCase ||
										(m_params.drawIn != TEST_DRAW_IN_RENDER_PASSES && m_params.clears != TEST_CLEARS_CMD_CLEAR_ATTACHMENTS));

		// Render second pass reference image with the first pattern
		tcu::TextureLevel refImagePattern0;
		if (!useSameSamplePattern() && !hasLeftSideImage)
		{
			const tcu::TextureFormat colorFormat = mapVkFormat(m_colorFormat);

			drawPatternChangeReference();

			refImagePattern0.setStorage(colorFormat, m_renderSize.x(), m_renderSize.y());
			tcu::copy(refImagePattern0.getAccess(), tcu::ConstPixelBufferAccess(colorFormat, tcu::IVec3(m_renderSize.x(), m_renderSize.y(), 1), m_colorBufferAlloc->getHostPtr()));
		}

		// Two-pass rendering

		switch (m_params.drawIn)
		{
			case TEST_DRAW_IN_RENDER_PASSES:	drawRenderPasses();	break;
			case TEST_DRAW_IN_SUBPASSES:		drawSubpasses();	break;
			case TEST_DRAW_IN_SAME_SUBPASS:		drawSameSubpass();	break;

			default:
				DE_ASSERT(0);
				break;
		}

		// Log the result

		const tcu::ConstPixelBufferAccess image (tcu::ConstPixelBufferAccess(mapVkFormat(m_colorFormat), tcu::IVec3(m_renderSize.x(), m_renderSize.y(), 1), m_colorBufferAlloc->getHostPtr()));

		m_context.getTestContext().getLog()
			<< tcu::TestLog::ImageSet("Result", "Final result")
			<< tcu::TestLog::Image("resolve0", "resolve0", image)
			<< tcu::TestLog::EndImageSet;

		// Verify result
		{
			DE_ASSERT((m_renderSize.x() % 2) == 0);
			DE_ASSERT((m_renderSize.y() % 2) == 0);

			// Count colors in each image half separately, each half may have its own background color
			const int  numBackgroundColors		= 1;
			const int  numExpectedColorsRight	= numBackgroundColors + static_cast<int>(m_params.numSamples);
			const int  numExpectedColorsLeft	= (isClearCase ? numBackgroundColors : numExpectedColorsRight);
			const int  numActualColorsLeft		= countUniqueColors(tcu::getSubregion(image, 0,					 0, m_renderSize.x()/2, m_renderSize.y()));
			const int  numActualColorsRight		= countUniqueColors(tcu::getSubregion(image, m_renderSize.x()/2, 0, m_renderSize.x()/2, m_renderSize.y()));

			if (numActualColorsLeft != numExpectedColorsLeft || numActualColorsRight != numExpectedColorsRight)
			{
				std::ostringstream msg;
				msg << "Expected " << numExpectedColorsLeft << " unique colors, but got " << numActualColorsLeft;

				if (numActualColorsLeft != numActualColorsRight)
					msg << " and " << numActualColorsRight;

				m_context.getTestContext().getLog()
					<< tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;

				return tcu::TestStatus::fail("Resolved image has incorrect pixels");
			}

			if (hasLeftSideImage)
			{
				// Compare the left and the right half
				const bool match = intThresholdCompare(tcu::getSubregion(image,	0,					0, m_renderSize.x()/2,	m_renderSize.y()),
													   tcu::getSubregion(image,	m_renderSize.x()/2, 0, m_renderSize.x()/2,	m_renderSize.y()),
													   UVec4(2u));
				if (useSameSamplePattern() && !match)
					return tcu::TestStatus::fail("Multisample pattern should be identical in both image halves");
				else if (!useSameSamplePattern() && match)
					return tcu::TestStatus::fail("Multisample pattern doesn't seem to change between left and right image halves");
			}
			else if (!useSameSamplePattern())
			{
				// Compare the right half with the previously rendered reference image -- patterns should be different
				bool match = intThresholdCompare(tcu::getSubregion(refImagePattern0.getAccess(),	m_renderSize.x()/2, 0, m_renderSize.x()/2,	m_renderSize.y()),
												 tcu::getSubregion(image,							m_renderSize.x()/2, 0, m_renderSize.x()/2,	m_renderSize.y()),
												 UVec4(2u));

				if (match)
					return tcu::TestStatus::fail("Multisample pattern doesn't seem to change between passes");
			}
		}

		return tcu::TestStatus::pass("Pass");
	}

protected:
	bool useDepth				(void) const { return m_params.imageAspect == TEST_IMAGE_ASPECT_DEPTH; }
	bool useStencil				(void) const { return m_params.imageAspect == TEST_IMAGE_ASPECT_STENCIL; }
	bool useSameSamplePattern	(void) const { return (m_params.options & TEST_OPTION_SAME_PATTERN_BIT) != 0u; }
	bool useDynamicState		(void) const { return (m_params.options & TEST_OPTION_DYNAMIC_STATE_BIT) != 0u; }
	bool useSecondaryCmdBuffer	(void) const { return (m_params.options & TEST_OPTION_SECONDARY_COMMAND_BUFFER_BIT) != 0u; }
	bool useGeneralLayout		(void) const { return (m_params.options & TEST_OPTION_GENERAL_LAYOUT_BIT) != 0u; }
	bool useWaitEvents			(void) const { return (m_params.options & TEST_OPTION_WAIT_EVENTS_BIT) != 0u; }

	//! Draw the second pass image, but with sample pattern from the first pass -- used to verify that the pattern is different
	void drawPatternChangeReference (void)
	{
		const DeviceInterface&			vk					= m_context.getDeviceInterface();
		const VkDevice					device				= m_context.getDevice();
		const VkViewport				viewport			= makeViewport(m_renderSize);
		const VkRect2D					renderArea			= makeRect2D(m_renderSize);
		const VkRect2D					scissor				= makeRect2D(m_renderSize);
		const Unique<VkShaderModule>	vertexModule		(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
		const Unique<VkShaderModule>	fragmentModule		(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u));
		const Unique<VkPipelineLayout>	pipelineLayout		(makePipelineLayout(vk, device));
		const VkSampleLocationsInfoEXT	sampleLocationsInfo	= makeSampleLocationsInfo(m_pixelGrids[0]);
		const VkClearValue				clearColor0			= (m_params.clears == TEST_CLEARS_NO_CLEAR ? makeClearValueColor(CLEAR_COLOR_0) : makeClearValueColor(CLEAR_COLOR_1));

		RenderTarget rt;

		rt.addAttachment(
			*m_colorImageView,											// VkImageView					imageView,
			(VkAttachmentDescriptionFlags)0,							// VkAttachmentDescriptionFlags	flags,
			m_colorFormat,												// VkFormat						format,
			m_params.numSamples,										// VkSampleCountFlagBits		numSamples,
			VK_ATTACHMENT_LOAD_OP_CLEAR,								// VkAttachmentLoadOp			loadOp,
			VK_ATTACHMENT_STORE_OP_STORE,								// VkAttachmentStoreOp			storeOp,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,							// VkAttachmentLoadOp			stencilLoadOp,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,							// VkAttachmentStoreOp			stencilStoreOp,
			VK_IMAGE_LAYOUT_UNDEFINED,									// VkImageLayout				initialLayout,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,					// VkImageLayout				finalLayout,
			clearColor0);												// VkClearValue					clearValue,

		rt.addAttachment(
			*m_resolveImageView,										// VkImageView					imageView,
			(VkAttachmentDescriptionFlags)0,							// VkAttachmentDescriptionFlags	flags,
			m_colorFormat,												// VkFormat						format,
			VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits		numSamples,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,							// VkAttachmentLoadOp			loadOp,
			VK_ATTACHMENT_STORE_OP_STORE,								// VkAttachmentStoreOp			storeOp,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,							// VkAttachmentLoadOp			stencilLoadOp,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,							// VkAttachmentStoreOp			stencilStoreOp,
			VK_IMAGE_LAYOUT_UNDEFINED,									// VkImageLayout				initialLayout,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,						// VkImageLayout				finalLayout,
			VkClearValue());											// VkClearValue					clearValue,

		rt.addSubpassColorAttachmentWithResolve(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
												1u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		if (useDepth() || useStencil())
		{
			rt.addAttachment(
				*m_depthStencilImageView,										// VkImageView					imageView,
				(VkAttachmentDescriptionFlags)0,								// VkAttachmentDescriptionFlags	flags,
				m_depthStencilFormat,											// VkFormat						format,
				m_params.numSamples,											// VkSampleCountFlagBits		numSamples,
				VK_ATTACHMENT_LOAD_OP_CLEAR,									// VkAttachmentLoadOp			loadOp,
				VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp			storeOp,
				VK_ATTACHMENT_LOAD_OP_CLEAR,									// VkAttachmentLoadOp			stencilLoadOp,
				VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp			stencilStoreOp,
				VK_IMAGE_LAYOUT_UNDEFINED,										// VkImageLayout				initialLayout,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,				// VkImageLayout				finalLayout,
				makeClearValueDepthStencil(DEPTH_CLEAR, STENCIL_REFERENCE),		// VkClearValue					clearValue,
				&sampleLocationsInfo);											// VkSampleLocationsInfoEXT*	pInitialSampleLocations

			rt.addSubpassDepthStencilAttachment(2u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, &sampleLocationsInfo);
		}

		rt.bake(vk, device, m_renderSize);

		const Unique<VkPipeline> pipeline(makeGraphicsPipeline(
				vk, device, std::vector<VkDynamicState>(), *pipelineLayout, rt.getRenderPass(), *vertexModule, *fragmentModule,
				/*subpass index*/ 0u, viewport, scissor, m_params.numSamples, /*use sample locations*/ true, sampleLocationsInfo,
				useDepth(), useStencil(), VERTEX_INPUT_VEC4_VEC4, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, stencilOpStateDrawOnce()));

		const Unique<VkCommandPool>		cmdPool				(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, m_context.getUniversalQueueFamilyIndex()));
		const Unique<VkCommandBuffer>	cmdBuffer			(makeCommandBuffer(vk, device, *cmdPool));
		Move<VkCommandBuffer>			secondaryCmdBuffer;
		VkCommandBuffer					currentCmdBuffer	= *cmdBuffer;

		beginCommandBuffer(vk, currentCmdBuffer);
		rt.recordBeginRenderPass(vk, currentCmdBuffer, renderArea, (useSecondaryCmdBuffer() ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE));

		// For maximum consistency also use a secondary command buffer, if the two-pass path uses it
		if (useSecondaryCmdBuffer())
		{
			secondaryCmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
			currentCmdBuffer = *secondaryCmdBuffer;

			beginSecondaryCommandBuffer(vk, currentCmdBuffer, rt.getRenderPass(), /*subpass*/ 0u, rt.getFramebuffer());
		}

		vk.cmdBindVertexBuffers(currentCmdBuffer, /*first binding*/ 0u, /*num bindings*/ 1u, &m_vertexBuffer.get(), /*offsets*/ &ZERO);
		vk.cmdBindPipeline(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

		// Draw the right shape only
		vk.cmdDraw(currentCmdBuffer, m_numVertices, /*instance count*/ 1u, /*first vertex*/ 0u, /*first instance*/ 1u);

		if (useSecondaryCmdBuffer())
		{
			endCommandBuffer(vk, currentCmdBuffer);
			currentCmdBuffer = *cmdBuffer;

			vk.cmdExecuteCommands(currentCmdBuffer, 1u, &secondaryCmdBuffer.get());
		}

		endRenderPass(vk, *cmdBuffer);

		recordCopyImageToBuffer(vk, *cmdBuffer, m_renderSize, *m_resolveImage, *m_colorBuffer);

		endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *cmdBuffer);

		invalidateAlloc(vk, device, *m_colorBufferAlloc);
	}

	//! Draw two shapes with distinct sample patterns, each in its own render pass
	void drawRenderPasses (void)
	{
		const DeviceInterface&			vk					= m_context.getDeviceInterface();
		const VkDevice					device				= m_context.getDevice();
		const VkViewport				viewport			= makeViewport(m_renderSize);
		const VkRect2D					renderArea			= makeRect2D(m_renderSize);
		const VkRect2D					scissor				= makeRect2D(m_renderSize);
		const Unique<VkShaderModule>	vertexModule		(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
		const Unique<VkShaderModule>	fragmentModule		(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u));
		const Unique<VkPipelineLayout>	pipelineLayout		(makePipelineLayout(vk, device));
		const VkClearValue				clearColor0			= makeClearValueColor(CLEAR_COLOR_0);
		const VkClearValue				clearColor1			= makeClearValueColor(CLEAR_COLOR_1);
		const VkClearValue				clearDepthStencil0	= makeClearValueDepthStencil(DEPTH_CLEAR, STENCIL_REFERENCE);
		const VkSampleLocationsInfoEXT	sampleLocationsInfo	[NUM_PASSES] =
		{
			makeSampleLocationsInfo(m_pixelGrids[0]),
			makeSampleLocationsInfo(m_pixelGrids[useSameSamplePattern() ? 0 : 1]),
		};
		const Unique<VkCommandPool>		cmdPool				(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, m_context.getUniversalQueueFamilyIndex()));
		Move<VkCommandBuffer>			cmdBuffer			[NUM_PASSES] =
		{
			makeCommandBuffer(vk, device, *cmdPool),
			makeCommandBuffer(vk, device, *cmdPool),
		};
		Move<VkCommandBuffer>			secondaryCmdBuffer	[NUM_PASSES];
		RenderTarget					rt					[NUM_PASSES];
		Move<VkPipeline>				pipeline			[NUM_PASSES];
		Move<VkEvent>					event				[2];	/*color and depth/stencil*/

		// Layouts expected by the second render pass
		const VkImageLayout	colorLayout1		= useGeneralLayout() && !(useDepth() || useStencil()) ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		const VkImageLayout	depthStencilLayout1	= useGeneralLayout() && (useDepth() || useStencil())  ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// First render pass - no resolves
		{
			rt[0].addAttachment(
				*m_colorImageView,											// VkImageView					imageView,
				(VkAttachmentDescriptionFlags)0,							// VkAttachmentDescriptionFlags	flags,
				m_colorFormat,												// VkFormat						format,
				m_params.numSamples,										// VkSampleCountFlagBits		numSamples,
				VK_ATTACHMENT_LOAD_OP_CLEAR,								// VkAttachmentLoadOp			loadOp,
				VK_ATTACHMENT_STORE_OP_STORE,								// VkAttachmentStoreOp			storeOp,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,							// VkAttachmentLoadOp			stencilLoadOp,
				VK_ATTACHMENT_STORE_OP_DONT_CARE,							// VkAttachmentStoreOp			stencilStoreOp,
				VK_IMAGE_LAYOUT_UNDEFINED,									// VkImageLayout				initialLayout,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,					// VkImageLayout				finalLayout,
				clearColor0);												// VkClearValue					clearValue,

			rt[0].addSubpassColorAttachment(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

			if (useDepth() || useStencil())
			{
				rt[0].addAttachment(
					*m_depthStencilImageView,										// VkImageView					imageView,
					(VkAttachmentDescriptionFlags)0,								// VkAttachmentDescriptionFlags	flags,
					m_depthStencilFormat,											// VkFormat						format,
					m_params.numSamples,											// VkSampleCountFlagBits		numSamples,
					VK_ATTACHMENT_LOAD_OP_CLEAR,									// VkAttachmentLoadOp			loadOp,
					VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp			storeOp,
					VK_ATTACHMENT_LOAD_OP_CLEAR,									// VkAttachmentLoadOp			stencilLoadOp,
					VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp			stencilStoreOp,
					VK_IMAGE_LAYOUT_UNDEFINED,										// VkImageLayout				initialLayout,
					VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,				// VkImageLayout				finalLayout,
					clearDepthStencil0,												// VkClearValue					clearValue,
					&sampleLocationsInfo[0]);										// VkSampleLocationsInfoEXT*	pInitialSampleLocations

				rt[0].addSubpassDepthStencilAttachment(1u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, &sampleLocationsInfo[0]);
			}

			rt[0].bake(vk, device, m_renderSize);
		}

		// Second render pass
		{
			const VkAttachmentLoadOp loadOp	= (m_params.clears == TEST_CLEARS_LOAD_OP_CLEAR ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD);

			rt[1].addAttachment(
				*m_colorImageView,											// VkImageView					imageView,
				(VkAttachmentDescriptionFlags)0,							// VkAttachmentDescriptionFlags	flags,
				m_colorFormat,												// VkFormat						format,
				m_params.numSamples,										// VkSampleCountFlagBits		numSamples,
				loadOp,														// VkAttachmentLoadOp			loadOp,
				VK_ATTACHMENT_STORE_OP_STORE,								// VkAttachmentStoreOp			storeOp,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,							// VkAttachmentLoadOp			stencilLoadOp,
				VK_ATTACHMENT_STORE_OP_DONT_CARE,							// VkAttachmentStoreOp			stencilStoreOp,
				colorLayout1,												// VkImageLayout				initialLayout,
				colorLayout1,												// VkImageLayout				finalLayout,
				clearColor1);												// VkClearValue					clearValue,

			rt[1].addAttachment(
				*m_resolveImageView,										// VkImageView					imageView,
				(VkAttachmentDescriptionFlags)0,							// VkAttachmentDescriptionFlags	flags,
				m_colorFormat,												// VkFormat						format,
				VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits		numSamples,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,							// VkAttachmentLoadOp			loadOp,
				VK_ATTACHMENT_STORE_OP_STORE,								// VkAttachmentStoreOp			storeOp,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,							// VkAttachmentLoadOp			stencilLoadOp,
				VK_ATTACHMENT_STORE_OP_DONT_CARE,							// VkAttachmentStoreOp			stencilStoreOp,
				VK_IMAGE_LAYOUT_UNDEFINED,									// VkImageLayout				initialLayout,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,						// VkImageLayout				finalLayout,
				VkClearValue());											// VkClearValue					clearValue,

			rt[1].addSubpassColorAttachmentWithResolve(0u, colorLayout1,
													   1u, colorLayout1);

			if (useDepth() || useStencil())
			{
				rt[1].addAttachment(
					*m_depthStencilImageView,										// VkImageView					imageView,
					(VkAttachmentDescriptionFlags)0,								// VkAttachmentDescriptionFlags	flags,
					m_depthStencilFormat,											// VkFormat						format,
					m_params.numSamples,											// VkSampleCountFlagBits		numSamples,
					loadOp,															// VkAttachmentLoadOp			loadOp,
					VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp			storeOp,
					loadOp,															// VkAttachmentLoadOp			stencilLoadOp,
					VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp			stencilStoreOp,
					depthStencilLayout1,											// VkImageLayout				initialLayout,
					depthStencilLayout1,											// VkImageLayout				finalLayout,
					clearDepthStencil0,												// VkClearValue					clearValue,
					&sampleLocationsInfo[1]);										// VkSampleLocationsInfoEXT*	pInitialSampleLocations

				rt[1].addSubpassDepthStencilAttachment(2u, depthStencilLayout1, &sampleLocationsInfo[1]);
			}

			rt[1].bake(vk, device, m_renderSize);
		}

		// Pipelines

		if (useDynamicState())
		{
			std::vector<VkDynamicState>	dynamicState;
			dynamicState.push_back(VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT);

			for (deUint32 passNdx = 0; passNdx < NUM_PASSES; ++passNdx)
			{
				pipeline[passNdx] = makeGraphicsPipeline(
					vk, device, dynamicState, *pipelineLayout, rt[passNdx].getRenderPass(), *vertexModule, *fragmentModule,
					/*subpass index*/ 0u, viewport, scissor, m_params.numSamples, /*use sample locations*/ true, makeEmptySampleLocationsInfo(),
					useDepth(), useStencil(), VERTEX_INPUT_VEC4_VEC4, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, stencilOpStateDrawOnce());
			}
		}
		else for (deUint32 passNdx = 0; passNdx < NUM_PASSES; ++passNdx)
		{
			pipeline[passNdx] = makeGraphicsPipeline(
				vk, device, std::vector<VkDynamicState>(), *pipelineLayout, rt[passNdx].getRenderPass(), *vertexModule, *fragmentModule,
				/*subpass index*/ 0u, viewport, scissor, m_params.numSamples, /*use sample locations*/ true, sampleLocationsInfo[passNdx],
				useDepth(), useStencil(), VERTEX_INPUT_VEC4_VEC4, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, stencilOpStateDrawOnce());
		}

		// Record secondary command buffers

		if (useSecondaryCmdBuffer())
		{
			secondaryCmdBuffer[0] = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
			secondaryCmdBuffer[1] = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

			// First render pass contents
			beginSecondaryCommandBuffer(vk, *secondaryCmdBuffer[0], rt[0].getRenderPass(), /*subpass*/ 0u, rt[0].getFramebuffer());
			recordFirstPassContents(*secondaryCmdBuffer[0], *pipeline[0], sampleLocationsInfo[0]);
			endCommandBuffer(vk, *secondaryCmdBuffer[0]);

			// Second render pass contents
			beginSecondaryCommandBuffer(vk, *secondaryCmdBuffer[1], rt[1].getRenderPass(), /*subpass*/ 0u, rt[1].getFramebuffer());
			recordSecondPassContents(*secondaryCmdBuffer[1], *pipeline[1], sampleLocationsInfo[1], clearColor1, clearDepthStencil0, scissor);
			endCommandBuffer(vk, *secondaryCmdBuffer[1]);
		}

		// Record primary command buffers

		VkCommandBuffer currentCmdBuffer = *cmdBuffer[0];
		beginCommandBuffer(vk, currentCmdBuffer);

		// First render pass
		if (useSecondaryCmdBuffer())
		{
			rt[0].recordBeginRenderPass(vk, currentCmdBuffer, renderArea, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			vk.cmdExecuteCommands(currentCmdBuffer, 1u, &secondaryCmdBuffer[0].get());
			endRenderPass(vk, currentCmdBuffer);
		}
		else
		{
			rt[0].recordBeginRenderPass(vk, currentCmdBuffer, renderArea, VK_SUBPASS_CONTENTS_INLINE);
			recordFirstPassContents(currentCmdBuffer, *pipeline[0], sampleLocationsInfo[0]);
			endRenderPass(vk, currentCmdBuffer);
		}

		endCommandBuffer(vk, currentCmdBuffer);

		// Record the second primary command buffer
		currentCmdBuffer = *cmdBuffer[1];
		beginCommandBuffer(vk, currentCmdBuffer);

		if (m_params.clears == TEST_CLEARS_CMD_CLEAR_IMAGE)
		{
			{
				const VkImageLayout finalLayout = (useWaitEvents() ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : colorLayout1);

				recordImageBarrier(vk, currentCmdBuffer, *m_colorImage,
									VK_IMAGE_ASPECT_COLOR_BIT,								// VkImageAspectFlags	aspect,
									VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,			// VkPipelineStageFlags srcStageMask,
									VK_PIPELINE_STAGE_TRANSFER_BIT,							// VkPipelineStageFlags dstStageMask,
									VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,					// VkAccessFlags		srcAccessMask,
									VK_ACCESS_TRANSFER_WRITE_BIT,							// VkAccessFlags		dstAccessMask,
									VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				// VkImageLayout		oldLayout,
									VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);					// VkImageLayout		newLayout)

				const VkImageSubresourceRange subresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
				vk.cmdClearColorImage(currentCmdBuffer, *m_colorImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor1.color, 1u, &subresourceRange);

				recordImageBarrier(vk, currentCmdBuffer, *m_colorImage,
						VK_IMAGE_ASPECT_COLOR_BIT,											// VkImageAspectFlags	aspect,
						VK_PIPELINE_STAGE_TRANSFER_BIT,										// VkPipelineStageFlags srcStageMask,
						VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,						// VkPipelineStageFlags dstStageMask,
						VK_ACCESS_TRANSFER_WRITE_BIT,										// VkAccessFlags		srcAccessMask,
						(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
						 VK_ACCESS_COLOR_ATTACHMENT_READ_BIT),								// VkAccessFlags		dstAccessMask,
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,								// VkImageLayout		oldLayout,
						finalLayout);														// VkImageLayout		newLayout)
			}

			if (useDepth() || useStencil())
			{
				const VkImageLayout finalLayout = (useWaitEvents() ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : depthStencilLayout1);

				recordImageBarrier(vk, currentCmdBuffer, *m_depthStencilImage,
								   getImageAspectFlags(m_depthStencilFormat),			// VkImageAspectFlags	aspect,
								   VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,			// VkPipelineStageFlags srcStageMask,
								   VK_PIPELINE_STAGE_TRANSFER_BIT,						// VkPipelineStageFlags dstStageMask,
								   VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,		// VkAccessFlags		srcAccessMask,
								   VK_ACCESS_TRANSFER_WRITE_BIT,						// VkAccessFlags		dstAccessMask,
								   VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// VkImageLayout		oldLayout,
								   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,				// VkImageLayout		newLayout)
								   &sampleLocationsInfo[0]);							// VkSampleLocationsInfoEXT

				const VkImageSubresourceRange subresourceRange = makeImageSubresourceRange(m_depthStencilAspect, 0u, 1u, 0u, 1u);
				vk.cmdClearDepthStencilImage(currentCmdBuffer, *m_depthStencilImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearDepthStencil0.depthStencil, 1u, &subresourceRange);

				recordImageBarrier(vk, currentCmdBuffer, *m_depthStencilImage,
								   getImageAspectFlags(m_depthStencilFormat),			// VkImageAspectFlags	aspect,
								   VK_PIPELINE_STAGE_TRANSFER_BIT,						// VkPipelineStageFlags srcStageMask,
								   VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,			// VkPipelineStageFlags dstStageMask,
								   VK_ACCESS_TRANSFER_WRITE_BIT,						// VkAccessFlags		srcAccessMask,
								   (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
									VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT),		// VkAccessFlags		dstAccessMask,
								   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,				// VkImageLayout		oldLayout,
								   finalLayout,											// VkImageLayout		newLayout)
								   &sampleLocationsInfo[0]);							// VkSampleLocationsInfoEXT
			}
		}
		else if (!useWaitEvents())
		{
			// Barrier between the render passes

			recordImageBarrier(vk, currentCmdBuffer, *m_colorImage,
							   VK_IMAGE_ASPECT_COLOR_BIT,								// VkImageAspectFlags	aspect,
							   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,			// VkPipelineStageFlags srcStageMask,
							   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,			// VkPipelineStageFlags dstStageMask,
							   VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,					// VkAccessFlags		srcAccessMask,
							   (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
								VK_ACCESS_COLOR_ATTACHMENT_READ_BIT),					// VkAccessFlags		dstAccessMask,
							   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				// VkImageLayout		oldLayout,
							   colorLayout1);											// VkImageLayout		newLayout)

			if (useDepth() || useStencil())
			{
				recordImageBarrier(vk, currentCmdBuffer, *m_depthStencilImage,
								   getImageAspectFlags(m_depthStencilFormat),			// VkImageAspectFlags	aspect,
								   VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,			// VkPipelineStageFlags srcStageMask,
								   VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,			// VkPipelineStageFlags dstStageMask,
								   VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,		// VkAccessFlags		srcAccessMask,
								   (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
									VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT),		// VkAccessFlags		dstAccessMask,
								   VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// VkImageLayout		oldLayout,
								   depthStencilLayout1);								// VkImageLayout		newLayout)
			}
		}

		if (useWaitEvents())
		{
			// Use events to sync both render passes
			event[0] = makeEvent(vk, device);
			vk.cmdSetEvent(currentCmdBuffer, *event[0], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

			recordWaitEventWithImage(vk, currentCmdBuffer, *event[0], *m_colorImage,
									 VK_IMAGE_ASPECT_COLOR_BIT,								// VkImageAspectFlags		aspect,
									 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,			// VkPipelineStageFlags		srcStageMask,
									 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,			// VkPipelineStageFlags		dstStageMask,
									 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,					// VkAccessFlags			srcAccessMask,
									 (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
									  VK_ACCESS_COLOR_ATTACHMENT_READ_BIT),					// VkAccessFlags			dstAccessMask,
									 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				// VkImageLayout			oldLayout,
									 colorLayout1);											// VkImageLayout			newLayout,

			if (useDepth() || useStencil())
			{
				event[1] = makeEvent(vk, device);
				vk.cmdSetEvent(currentCmdBuffer, *event[1], VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);

				recordWaitEventWithImage(vk, currentCmdBuffer, *event[1], *m_depthStencilImage,
										 getImageAspectFlags(m_depthStencilFormat),			// VkImageAspectFlags		aspect,
										 VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,			// VkPipelineStageFlags		srcStageMask,
										 VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,		// VkPipelineStageFlags		dstStageMask,
										 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,		// VkAccessFlags			srcAccessMask,
										 (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
										  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT),		// VkAccessFlags			dstAccessMask,
										 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// VkImageLayout			oldLayout,
										 depthStencilLayout1);								// VkImageLayout			newLayout,
			}
		}

		// Second render pass
		if (useSecondaryCmdBuffer())
		{
			rt[1].recordBeginRenderPass(vk, currentCmdBuffer, renderArea, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			vk.cmdExecuteCommands(currentCmdBuffer, 1u, &secondaryCmdBuffer[1].get());
			endRenderPass(vk, currentCmdBuffer);
		}
		else
		{
			rt[1].recordBeginRenderPass(vk, currentCmdBuffer, renderArea, VK_SUBPASS_CONTENTS_INLINE);
			recordSecondPassContents(currentCmdBuffer, *pipeline[1], sampleLocationsInfo[1], clearColor1, clearDepthStencil0, scissor);
			endRenderPass(vk, currentCmdBuffer);
		}

		// Resolve image -> host buffer
		recordCopyImageToBuffer(vk, currentCmdBuffer, m_renderSize, *m_resolveImage, *m_colorBuffer);

		endCommandBuffer(vk, currentCmdBuffer);

		// Submit work
		{
			const Unique<VkFence>	fence	(createFence(vk, device));
			const VkCommandBuffer	buffers	[NUM_PASSES] =
			{
				*cmdBuffer[0],
				*cmdBuffer[1],
			};

			const VkSubmitInfo submitInfo =
			{
				VK_STRUCTURE_TYPE_SUBMIT_INFO,		// VkStructureType                sType;
				DE_NULL,							// const void*                    pNext;
				0u,									// uint32_t                       waitSemaphoreCount;
				DE_NULL,							// const VkSemaphore*             pWaitSemaphores;
				DE_NULL,							// const VkPipelineStageFlags*    pWaitDstStageMask;
				DE_LENGTH_OF_ARRAY(buffers),		// uint32_t                       commandBufferCount;
				buffers,							// const VkCommandBuffer*         pCommandBuffers;
				0u,									// uint32_t                       signalSemaphoreCount;
				DE_NULL,							// const VkSemaphore*             pSignalSemaphores;
			};
			VK_CHECK(vk.queueSubmit(m_context.getUniversalQueue(), 1u, &submitInfo, *fence));
			VK_CHECK(vk.waitForFences(device, 1u, &fence.get(), DE_TRUE, ~0ull));
		}

		invalidateAlloc(vk, device, *m_colorBufferAlloc);
	}

	void recordFirstPassContents (const VkCommandBuffer				cmdBuffer,
								  const VkPipeline					pipeline,
								  const VkSampleLocationsInfoEXT&	sampleLocationsInfo)
	{
		const DeviceInterface& vk = m_context.getDeviceInterface();

		vk.cmdBindVertexBuffers(cmdBuffer, /*first binding*/ 0u, /*num bindings*/ 1u, &m_vertexBuffer.get(), /*offsets*/ &ZERO);
		vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		if (useDynamicState())
			vk.cmdSetSampleLocationsEXT(cmdBuffer, &sampleLocationsInfo);

		if (m_params.clears == TEST_CLEARS_NO_CLEAR)
			vk.cmdDraw(cmdBuffer, m_numVertices, /*instance count*/ 1u, /*first vertex*/ 0u, /*first instance*/ 0u);			// left shape only
		else
			vk.cmdDraw(cmdBuffer, m_numVertices, /*instance count*/ NUM_PASSES, /*first vertex*/ 0u, /*first instance*/ 0u);	// both shapes
	}

	void recordSecondPassContents (const VkCommandBuffer			cmdBuffer,
								   const VkPipeline					pipeline,
								   const VkSampleLocationsInfoEXT&	sampleLocationsInfo,
								   const VkClearValue&				clearColor,
								   const VkClearValue&				clearDepthStencil,
								   const VkRect2D&					clearRect)
	{
		const DeviceInterface& vk = m_context.getDeviceInterface();

		vk.cmdBindVertexBuffers(cmdBuffer, /*first binding*/ 0u, /*num bindings*/ 1u, &m_vertexBuffer.get(), /*offsets*/ &ZERO);
		vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		if (m_params.clears == TEST_CLEARS_CMD_CLEAR_ATTACHMENTS)
			recordClearAttachments(vk, cmdBuffer, 0u, clearColor, m_depthStencilAspect, clearDepthStencil, clearRect);

		if (useDynamicState())
			vk.cmdSetSampleLocationsEXT(cmdBuffer, &sampleLocationsInfo);

		// Draw the right shape only
		vk.cmdDraw(cmdBuffer, m_numVertices, /*instance count*/ 1u, /*first vertex*/ 0u, /*first instance*/ 1u);
	}

	//! Draw two shapes in two subpasses of the same render pass
	void drawSubpasses (void)
	{
		DE_ASSERT(m_params.clears != TEST_CLEARS_CMD_CLEAR_IMAGE);			// not possible in a render pass
		DE_ASSERT(m_params.clears != TEST_CLEARS_LOAD_OP_CLEAR);			// can't specify a load op for a subpass
		DE_ASSERT((m_params.options & TEST_OPTION_WAIT_EVENTS_BIT) == 0);	// can't change layouts inside a subpass

		const DeviceInterface&			vk					= m_context.getDeviceInterface();
		const VkDevice					device				= m_context.getDevice();
		const VkViewport				viewport			= makeViewport(m_renderSize);
		const VkRect2D					renderArea			= makeRect2D(m_renderSize);
		const VkRect2D					scissor				= makeRect2D(m_renderSize);
		const Unique<VkShaderModule>	vertexModule		(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
		const Unique<VkShaderModule>	fragmentModule		(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u));
		const Unique<VkPipelineLayout>	pipelineLayout		(makePipelineLayout(vk, device));
		const VkClearValue				clearColor0			= makeClearValueColor(CLEAR_COLOR_0);
		const VkClearValue				clearColor1			= makeClearValueColor(CLEAR_COLOR_1);
		const VkClearValue				clearDepthStencil0	= makeClearValueDepthStencil(DEPTH_CLEAR, STENCIL_REFERENCE);
		const VkSampleLocationsInfoEXT	sampleLocationsInfo	[NUM_PASSES] =
		{
			makeSampleLocationsInfo(m_pixelGrids[0]),
			makeSampleLocationsInfo(m_pixelGrids[useSameSamplePattern() ? 0 : 1]),
		};
		const Unique<VkCommandPool>		cmdPool				(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, m_context.getUniversalQueueFamilyIndex()));
		const Unique<VkCommandBuffer>	cmdBuffer			(makeCommandBuffer(vk, device, *cmdPool));
		Move<VkCommandBuffer>			secondaryCmdBuffer	[NUM_PASSES];
		RenderTarget					rt;
		Move<VkPipeline>				pipeline			[NUM_PASSES];
		Move<VkEvent>					event;

		// Layouts used in the second subpass
		const VkImageLayout	colorLayout1		= useGeneralLayout() && !(useDepth() || useStencil()) ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		const VkImageLayout	depthStencilLayout1	= useGeneralLayout() && (useDepth() || useStencil())  ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// Prepare the render pass
		{
			rt.addAttachment(
				*m_colorImageView,											// VkImageView					imageView,
				(VkAttachmentDescriptionFlags)0,							// VkAttachmentDescriptionFlags	flags,
				m_colorFormat,												// VkFormat						format,
				m_params.numSamples,										// VkSampleCountFlagBits		numSamples,
				VK_ATTACHMENT_LOAD_OP_CLEAR,								// VkAttachmentLoadOp			loadOp,
				VK_ATTACHMENT_STORE_OP_STORE,								// VkAttachmentStoreOp			storeOp,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,							// VkAttachmentLoadOp			stencilLoadOp,
				VK_ATTACHMENT_STORE_OP_DONT_CARE,							// VkAttachmentStoreOp			stencilStoreOp,
				VK_IMAGE_LAYOUT_UNDEFINED,									// VkImageLayout				initialLayout,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,					// VkImageLayout				finalLayout,
				clearColor0);												// VkClearValue					clearValue,

			rt.addAttachment(
				*m_resolveImageView,										// VkImageView					imageView,
				(VkAttachmentDescriptionFlags)0,							// VkAttachmentDescriptionFlags	flags,
				m_colorFormat,												// VkFormat						format,
				VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits		numSamples,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,							// VkAttachmentLoadOp			loadOp,
				VK_ATTACHMENT_STORE_OP_STORE,								// VkAttachmentStoreOp			storeOp,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,							// VkAttachmentLoadOp			stencilLoadOp,
				VK_ATTACHMENT_STORE_OP_DONT_CARE,							// VkAttachmentStoreOp			stencilStoreOp,
				VK_IMAGE_LAYOUT_UNDEFINED,									// VkImageLayout				initialLayout,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,						// VkImageLayout				finalLayout,
				VkClearValue());											// VkClearValue					clearValue,

			// First subpass
			rt.addSubpassColorAttachment(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

			if (useDepth() || useStencil())
			{
				rt.addAttachment(
					*m_depthStencilImageView,										// VkImageView					imageView,
					(VkAttachmentDescriptionFlags)0,								// VkAttachmentDescriptionFlags	flags,
					m_depthStencilFormat,											// VkFormat						format,
					m_params.numSamples,											// VkSampleCountFlagBits		numSamples,
					VK_ATTACHMENT_LOAD_OP_CLEAR,									// VkAttachmentLoadOp			loadOp,
					VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp			storeOp,
					VK_ATTACHMENT_LOAD_OP_CLEAR,									// VkAttachmentLoadOp			stencilLoadOp,
					VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp			stencilStoreOp,
					VK_IMAGE_LAYOUT_UNDEFINED,										// VkImageLayout				initialLayout,
					VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,				// VkImageLayout				finalLayout,
					clearDepthStencil0,												// VkClearValue					clearValue,
					&sampleLocationsInfo[0]);										// VkSampleLocationsInfoEXT*	pInitialSampleLocations

				rt.addSubpassDepthStencilAttachment(2u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, &sampleLocationsInfo[0]);
			}

			// Second subpass
			rt.nextSubpass();
			rt.addSubpassColorAttachmentWithResolve(0u, colorLayout1,
													1u, colorLayout1);

			if (useDepth() || useStencil())
				rt.addSubpassDepthStencilAttachment(2u, depthStencilLayout1, &sampleLocationsInfo[1]);

			rt.bake(vk, device, m_renderSize);
		}

		// Pipelines

		if (useDynamicState())
		{
			std::vector<VkDynamicState>	dynamicState;
			dynamicState.push_back(VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT);

			for (deUint32 passNdx = 0; passNdx < NUM_PASSES; ++passNdx)
			{
				pipeline[passNdx] = makeGraphicsPipeline(
					vk, device, dynamicState, *pipelineLayout, rt.getRenderPass(), *vertexModule, *fragmentModule,
					/*subpass*/ passNdx, viewport, scissor, m_params.numSamples, /*use sample locations*/ true, makeEmptySampleLocationsInfo(),
					useDepth(), useStencil(), VERTEX_INPUT_VEC4_VEC4, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, stencilOpStateDrawOnce());
			}
		}
		else for (deUint32 passNdx = 0; passNdx < NUM_PASSES; ++passNdx)
		{
			pipeline[passNdx] = makeGraphicsPipeline(
				vk, device, std::vector<VkDynamicState>(), *pipelineLayout, rt.getRenderPass(), *vertexModule, *fragmentModule,
				/*subpass*/ passNdx, viewport, scissor, m_params.numSamples, /*use sample locations*/ true, sampleLocationsInfo[passNdx],
				useDepth(), useStencil(), VERTEX_INPUT_VEC4_VEC4, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, stencilOpStateDrawOnce());
		}

		// Record secondary command buffers

		if (useSecondaryCmdBuffer())
		{
			secondaryCmdBuffer[0] = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);
			secondaryCmdBuffer[1] = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

			// First subpass contents
			beginSecondaryCommandBuffer(vk, *secondaryCmdBuffer[0], rt.getRenderPass(), /*subpass*/ 0u, rt.getFramebuffer());
			recordFirstPassContents(*secondaryCmdBuffer[0], *pipeline[0], sampleLocationsInfo[0]);
			endCommandBuffer(vk, *secondaryCmdBuffer[0]);

			// Second subpass contents
			beginSecondaryCommandBuffer(vk, *secondaryCmdBuffer[1], rt.getRenderPass(), /*subpass*/ 1u, rt.getFramebuffer());
			recordSecondPassContents(*secondaryCmdBuffer[1], *pipeline[1], sampleLocationsInfo[1], clearColor1, clearDepthStencil0, scissor);
			endCommandBuffer(vk, *secondaryCmdBuffer[1]);
		}

		// Record primary command buffer

		beginCommandBuffer(vk, *cmdBuffer);

		if (useSecondaryCmdBuffer())
		{
			rt.recordBeginRenderPass(vk, *cmdBuffer, renderArea, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			vk.cmdExecuteCommands(*cmdBuffer, 1u, &secondaryCmdBuffer[0].get());

			vk.cmdNextSubpass(*cmdBuffer, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			vk.cmdExecuteCommands(*cmdBuffer, 1u, &secondaryCmdBuffer[1].get());
		}
		else
		{
			rt.recordBeginRenderPass(vk, *cmdBuffer, renderArea, VK_SUBPASS_CONTENTS_INLINE);
			recordFirstPassContents(*cmdBuffer, *pipeline[0], sampleLocationsInfo[0]);

			vk.cmdNextSubpass(*cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
			recordSecondPassContents(*cmdBuffer, *pipeline[1], sampleLocationsInfo[1], clearColor1, clearDepthStencil0, scissor);
		}

		endRenderPass(vk, *cmdBuffer);

		// Resolve image -> host buffer
		recordCopyImageToBuffer(vk, *cmdBuffer, m_renderSize, *m_resolveImage, *m_colorBuffer);

		endCommandBuffer(vk, *cmdBuffer);

		submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *cmdBuffer);
		invalidateAlloc(vk, device, *m_colorBufferAlloc);
	}

	//! Draw two shapes within the same subpass of a renderpass
	void drawSameSubpass (void)
	{
		DE_ASSERT(m_params.clears != TEST_CLEARS_CMD_CLEAR_IMAGE);				// not possible in a render pass
		DE_ASSERT(m_params.clears != TEST_CLEARS_LOAD_OP_CLEAR);				// can't specify a load op for a subpass
		DE_ASSERT((m_params.options & TEST_OPTION_WAIT_EVENTS_BIT) == 0);		// can't change layouts inside a subpass
		DE_ASSERT((m_params.options & TEST_OPTION_GENERAL_LAYOUT_BIT) == 0);	// can't change layouts inside a subpass

		const DeviceInterface&			vk					= m_context.getDeviceInterface();
		const VkDevice					device				= m_context.getDevice();
		const VkViewport				viewport			= makeViewport(m_renderSize);
		const VkRect2D					renderArea			= makeRect2D(m_renderSize);
		const VkRect2D					scissor				= makeRect2D(m_renderSize);
		const Unique<VkShaderModule>	vertexModule		(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
		const Unique<VkShaderModule>	fragmentModule		(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u));
		const Unique<VkPipelineLayout>	pipelineLayout		(makePipelineLayout(vk, device));
		const VkClearValue				clearColor0			= makeClearValueColor(CLEAR_COLOR_0);
		const VkClearValue				clearColor1			= makeClearValueColor(CLEAR_COLOR_1);
		const VkClearValue				clearDepthStencil0	= makeClearValueDepthStencil(DEPTH_CLEAR, STENCIL_REFERENCE);
		const VkSampleLocationsInfoEXT	sampleLocationsInfo	[NUM_PASSES] =
		{
			makeSampleLocationsInfo(m_pixelGrids[0]),
			makeSampleLocationsInfo(m_pixelGrids[useSameSamplePattern() ? 0 : 1]),
		};
		const Unique<VkCommandPool>		cmdPool				(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, m_context.getUniversalQueueFamilyIndex()));
		const Unique<VkCommandBuffer>	cmdBuffer			(makeCommandBuffer(vk, device, *cmdPool));
		Move<VkCommandBuffer>			secondaryCmdBuffer;
		RenderTarget					rt;
		Move<VkPipeline>				pipeline			[NUM_PASSES];
		Move<VkEvent>					event;

		// Prepare the render pass
		{
			rt.addAttachment(
				*m_colorImageView,											// VkImageView					imageView,
				(VkAttachmentDescriptionFlags)0,							// VkAttachmentDescriptionFlags	flags,
				m_colorFormat,												// VkFormat						format,
				m_params.numSamples,										// VkSampleCountFlagBits		numSamples,
				VK_ATTACHMENT_LOAD_OP_CLEAR,								// VkAttachmentLoadOp			loadOp,
				VK_ATTACHMENT_STORE_OP_STORE,								// VkAttachmentStoreOp			storeOp,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,							// VkAttachmentLoadOp			stencilLoadOp,
				VK_ATTACHMENT_STORE_OP_DONT_CARE,							// VkAttachmentStoreOp			stencilStoreOp,
				VK_IMAGE_LAYOUT_UNDEFINED,									// VkImageLayout				initialLayout,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,					// VkImageLayout				finalLayout,
				clearColor0);												// VkClearValue					clearValue,

			rt.addAttachment(
				*m_resolveImageView,										// VkImageView					imageView,
				(VkAttachmentDescriptionFlags)0,							// VkAttachmentDescriptionFlags	flags,
				m_colorFormat,												// VkFormat						format,
				VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits		numSamples,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,							// VkAttachmentLoadOp			loadOp,
				VK_ATTACHMENT_STORE_OP_STORE,								// VkAttachmentStoreOp			storeOp,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,							// VkAttachmentLoadOp			stencilLoadOp,
				VK_ATTACHMENT_STORE_OP_DONT_CARE,							// VkAttachmentStoreOp			stencilStoreOp,
				VK_IMAGE_LAYOUT_UNDEFINED,									// VkImageLayout				initialLayout,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,						// VkImageLayout				finalLayout,
				VkClearValue());											// VkClearValue					clearValue,

			rt.addSubpassColorAttachmentWithResolve(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
													1u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

			if (useDepth() || useStencil())
			{
				rt.addAttachment(
					*m_depthStencilImageView,										// VkImageView					imageView,
					(VkAttachmentDescriptionFlags)0,								// VkAttachmentDescriptionFlags	flags,
					m_depthStencilFormat,											// VkFormat						format,
					m_params.numSamples,											// VkSampleCountFlagBits		numSamples,
					VK_ATTACHMENT_LOAD_OP_CLEAR,									// VkAttachmentLoadOp			loadOp,
					VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp			storeOp,
					VK_ATTACHMENT_LOAD_OP_CLEAR,									// VkAttachmentLoadOp			stencilLoadOp,
					VK_ATTACHMENT_STORE_OP_STORE,									// VkAttachmentStoreOp			stencilStoreOp,
					VK_IMAGE_LAYOUT_UNDEFINED,										// VkImageLayout				initialLayout,
					VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,				// VkImageLayout				finalLayout,
					clearDepthStencil0,												// VkClearValue					clearValue,
					&sampleLocationsInfo[0]);										// VkSampleLocationsInfoEXT*	pInitialSampleLocations

				rt.addSubpassDepthStencilAttachment(2u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, &sampleLocationsInfo[0]);
			}

			rt.bake(vk, device, m_renderSize);
		}

		// Pipelines

		if (useDynamicState())
		{
			std::vector<VkDynamicState>	dynamicState;
			dynamicState.push_back(VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT);

			for (deUint32 passNdx = 0; passNdx < NUM_PASSES; ++passNdx)
			{
				pipeline[passNdx] = makeGraphicsPipeline(
					vk, device, dynamicState, *pipelineLayout, rt.getRenderPass(), *vertexModule, *fragmentModule,
					/*subpass*/ 0u, viewport, scissor, m_params.numSamples, /*use sample locations*/ true, makeEmptySampleLocationsInfo(),
					useDepth(), useStencil(), VERTEX_INPUT_VEC4_VEC4, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, stencilOpStateDrawOnce());
			}
		}
		else for (deUint32 passNdx = 0; passNdx < NUM_PASSES; ++passNdx)
		{
			pipeline[passNdx] = makeGraphicsPipeline(
				vk, device, std::vector<VkDynamicState>(), *pipelineLayout, rt.getRenderPass(), *vertexModule, *fragmentModule,
				/*subpass*/ 0u, viewport, scissor, m_params.numSamples, /*use sample locations*/ true, sampleLocationsInfo[passNdx],
				useDepth(), useStencil(), VERTEX_INPUT_VEC4_VEC4, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, stencilOpStateDrawOnce());
		}

		// Record secondary command buffers

		if (useSecondaryCmdBuffer())
		{
			secondaryCmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY);

			beginSecondaryCommandBuffer(vk, *secondaryCmdBuffer, rt.getRenderPass(), /*subpass*/ 0u, rt.getFramebuffer());
			recordFirstPassContents(*secondaryCmdBuffer, *pipeline[0], sampleLocationsInfo[0]);
			recordSecondPassContents(*secondaryCmdBuffer, *pipeline[1], sampleLocationsInfo[1], clearColor1, clearDepthStencil0, scissor);
			endCommandBuffer(vk, *secondaryCmdBuffer);
		}

		// Record primary command buffer

		beginCommandBuffer(vk, *cmdBuffer);

		if (useSecondaryCmdBuffer())
		{
			rt.recordBeginRenderPass(vk, *cmdBuffer, renderArea, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
			vk.cmdExecuteCommands(*cmdBuffer, 1u, &secondaryCmdBuffer.get());
		}
		else
		{
			rt.recordBeginRenderPass(vk, *cmdBuffer, renderArea, VK_SUBPASS_CONTENTS_INLINE);
			recordFirstPassContents(*cmdBuffer, *pipeline[0], sampleLocationsInfo[0]);
			recordSecondPassContents(*cmdBuffer, *pipeline[1], sampleLocationsInfo[1], clearColor1, clearDepthStencil0, scissor);
		}

		endRenderPass(vk, *cmdBuffer);

		// Resolve image -> host buffer
		recordCopyImageToBuffer(vk, *cmdBuffer, m_renderSize, *m_resolveImage, *m_colorBuffer);

		endCommandBuffer(vk, *cmdBuffer);

		submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *cmdBuffer);
		invalidateAlloc(vk, device, *m_colorBufferAlloc);
	}

	const TestParams									m_params;
	const VkPhysicalDeviceSampleLocationsPropertiesEXT	m_sampleLocationsProperties;
	const UVec2											m_renderSize;
	UVec2												m_gridSize;
	std::vector<MultisamplePixelGrid>					m_pixelGrids;
	deUint32											m_numVertices;
	Move<VkBuffer>										m_vertexBuffer;
	MovePtr<Allocation>									m_vertexBufferAlloc;
	const VkFormat										m_colorFormat;
	Move<VkImage>										m_colorImage;
	Move<VkImageView>									m_colorImageView;
	MovePtr<Allocation>									m_colorImageAlloc;
	VkFormat											m_depthStencilFormat;
	VkImageAspectFlags									m_depthStencilAspect;
	Move<VkImage>										m_depthStencilImage;
	Move<VkImageView>									m_depthStencilImageView;
	MovePtr<Allocation>									m_depthStencilImageAlloc;
	Move<VkImage>										m_resolveImage;
	Move<VkImageView>									m_resolveImageView;
	MovePtr<Allocation>									m_resolveImageAlloc;
	Move<VkBuffer>										m_colorBuffer;
	MovePtr<Allocation>									m_colorBufferAlloc;
};

} // Draw

void createTestsInGroup (tcu::TestCaseGroup* rootGroup)
{
	// Queries
	{
		MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(rootGroup->getTestContext(), "query", ""));

		addFunctionCase(group.get(), "sample_locations_properties", "", checkSupportSampleLocations, testQuerySampleLocationProperties);
		addFunctionCase(group.get(), "multisample_properties",		"", checkSupportSampleLocations, testQueryMultisampleProperties);

		rootGroup->addChild(group.release());
	}

	const VkSampleCountFlagBits	sampleCountRange[] =
	{
		VK_SAMPLE_COUNT_2_BIT,
		VK_SAMPLE_COUNT_4_BIT,
		VK_SAMPLE_COUNT_8_BIT,
		VK_SAMPLE_COUNT_16_BIT,
		// There are no implementations that support 32 or 64 programmable samples currently
	};

	// Verify custom sample locations and interpolation
	{
		using namespace VerifySamples;

		MovePtr<tcu::TestCaseGroup> groupLocation		(new tcu::TestCaseGroup(rootGroup->getTestContext(), "verify_location", ""));
		MovePtr<tcu::TestCaseGroup> groupInterpolation	(new tcu::TestCaseGroup(rootGroup->getTestContext(), "verify_interpolation", ""));

		for (const VkSampleCountFlagBits* pLoopNumSamples = sampleCountRange; pLoopNumSamples < DE_ARRAY_END(sampleCountRange); ++pLoopNumSamples)
		{
			addCases<VerifyLocationTest>	 (groupLocation.get(),		*pLoopNumSamples, addProgramsVerifyLocationGeometry);
			addCases<VerifyInterpolationTest>(groupInterpolation.get(),	*pLoopNumSamples, addProgramsVerifyInterpolation);
		}

		rootGroup->addChild(groupLocation.release());
		rootGroup->addChild(groupInterpolation.release());
	}

	// Draw with custom samples and various options
	{
		using namespace Draw;

		const deUint32 optionSets[] =
		{
			TEST_OPTION_SAME_PATTERN_BIT,
			0u,
			TEST_OPTION_DYNAMIC_STATE_BIT,
			TEST_OPTION_SECONDARY_COMMAND_BUFFER_BIT,
			TEST_OPTION_DYNAMIC_STATE_BIT | TEST_OPTION_SECONDARY_COMMAND_BUFFER_BIT,
			TEST_OPTION_GENERAL_LAYOUT_BIT,
			TEST_OPTION_GENERAL_LAYOUT_BIT | TEST_OPTION_DYNAMIC_STATE_BIT,
			TEST_OPTION_GENERAL_LAYOUT_BIT | TEST_OPTION_SECONDARY_COMMAND_BUFFER_BIT,
			TEST_OPTION_GENERAL_LAYOUT_BIT | TEST_OPTION_DYNAMIC_STATE_BIT | TEST_OPTION_SECONDARY_COMMAND_BUFFER_BIT,
			TEST_OPTION_WAIT_EVENTS_BIT,
			TEST_OPTION_WAIT_EVENTS_BIT | TEST_OPTION_GENERAL_LAYOUT_BIT,
			TEST_OPTION_WAIT_EVENTS_BIT | TEST_OPTION_GENERAL_LAYOUT_BIT | TEST_OPTION_SECONDARY_COMMAND_BUFFER_BIT,
		};

		const struct
		{
			TestDrawIn	drawIn;
			TestClears	clears;
		} drawClearSets[] =
		{
			{ TEST_DRAW_IN_RENDER_PASSES,	TEST_CLEARS_NO_CLEAR				},
			{ TEST_DRAW_IN_RENDER_PASSES,	TEST_CLEARS_LOAD_OP_CLEAR			},
			{ TEST_DRAW_IN_RENDER_PASSES,	TEST_CLEARS_CMD_CLEAR_ATTACHMENTS	},
			{ TEST_DRAW_IN_RENDER_PASSES,	TEST_CLEARS_CMD_CLEAR_IMAGE			},
			{ TEST_DRAW_IN_SUBPASSES,		TEST_CLEARS_NO_CLEAR				},
			{ TEST_DRAW_IN_SUBPASSES,		TEST_CLEARS_CMD_CLEAR_ATTACHMENTS	},
			{ TEST_DRAW_IN_SAME_SUBPASS,	TEST_CLEARS_NO_CLEAR				},
			{ TEST_DRAW_IN_SAME_SUBPASS,	TEST_CLEARS_CMD_CLEAR_ATTACHMENTS	},
		};

		const TestImageAspect aspectRange[] =
		{
			TEST_IMAGE_ASPECT_COLOR,
			TEST_IMAGE_ASPECT_DEPTH,
			TEST_IMAGE_ASPECT_STENCIL,
		};

		MovePtr<tcu::TestCaseGroup> drawGroup (new tcu::TestCaseGroup(rootGroup->getTestContext(), "draw", ""));
		for (const TestImageAspect* pLoopImageAspect = aspectRange; pLoopImageAspect != DE_ARRAY_END(aspectRange); ++pLoopImageAspect)
		{
			MovePtr<tcu::TestCaseGroup> aspectGroup (new tcu::TestCaseGroup(drawGroup->getTestContext(), getString(*pLoopImageAspect), ""));
			for (const VkSampleCountFlagBits* pLoopNumSamples = sampleCountRange; pLoopNumSamples < DE_ARRAY_END(sampleCountRange); ++pLoopNumSamples)
			{
				MovePtr<tcu::TestCaseGroup> samplesGroup (new tcu::TestCaseGroup(aspectGroup->getTestContext(), getString(*pLoopNumSamples).c_str(), ""));

				for (deUint32		 loopDrawSetNdx = 0u;		  loopDrawSetNdx <  DE_LENGTH_OF_ARRAY(drawClearSets); ++loopDrawSetNdx)
				for (const deUint32* pLoopOptions	= optionSets; pLoopOptions	 != DE_ARRAY_END(optionSets);		   ++pLoopOptions)
				{
					const TestParams params =
					{
						*pLoopNumSamples,							// VkSampleCountFlagBits	numSamples;
						*pLoopOptions,								// TestOptionFlags			options;
						drawClearSets[loopDrawSetNdx].drawIn,		// TestDrawIn				drawIn;
						drawClearSets[loopDrawSetNdx].clears,		// TestClears				clears;
						*pLoopImageAspect,							// TestImageAspect			imageAspect;
					};

					// Filter out incompatible parameter combinations
					if (params.imageAspect != TEST_IMAGE_ASPECT_COLOR)
					{
						// If the sample pattern is changed, the D/S image must be cleared or the result is undefined
						if (((params.options & TEST_OPTION_SAME_PATTERN_BIT) == 0u) && (params.clears == TEST_CLEARS_NO_CLEAR))
							continue;
					}

					// We are using events to change image layout and this is only allowed outside a render pass
					if (((params.options & TEST_OPTION_WAIT_EVENTS_BIT) != 0u) && (params.drawIn != TEST_DRAW_IN_RENDER_PASSES))
						continue;

					// Can't change image layout inside a subpass
					if (((params.options & TEST_OPTION_GENERAL_LAYOUT_BIT) != 0u) && (params.drawIn == TEST_DRAW_IN_SAME_SUBPASS))
						continue;

					std::ostringstream caseName;
					caseName << getString(params.drawIn) << "_"
							 << getString(params.clears) << (params.options != 0 ? "_" : "")
							 << getTestOptionFlagsString(params.options);

					addInstanceTestCaseWithPrograms<DrawTest>(samplesGroup.get(), caseName.str().c_str(), "", checkSupportDrawTests, initPrograms, params);
				}
				aspectGroup->addChild(samplesGroup.release());
			}
			drawGroup->addChild(aspectGroup.release());
		}
		rootGroup->addChild(drawGroup.release());
	}
}

} // anonymous ns

tcu::TestCaseGroup* createMultisampleSampleLocationsExtTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "sample_locations_ext", "Test a graphics pipeline with user-defined sample locations", createTestsInGroup);
}

} // pipeline
} // vkt
