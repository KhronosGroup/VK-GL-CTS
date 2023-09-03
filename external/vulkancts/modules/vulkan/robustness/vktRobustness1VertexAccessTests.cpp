/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
 * Copyright (c) 2022 Google Inc.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \brief Robustness1 vertex access out of range tests
 *//*--------------------------------------------------------------------*/

#include "vktRobustness1VertexAccessTests.hpp"
#include "deDefs.h"
#include "deMemory.h"
#include "gluShaderProgram.hpp"
#include "gluShaderUtil.hpp"
#include "image/vktImageLoadStoreUtil.hpp"
#include "pipeline/vktPipelineSpecConstantUtil.hpp"
#include "qpTestLog.h"
#include "tcuTestCase.hpp"
#include "tcuTestContext.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorType.hpp"
#include "tcuVectorUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkDefs.hpp"
#include "vkObjUtil.hpp"
#include "vkShaderProgram.hpp"
#include "vktRobustnessUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "tcuTestLog.hpp"
#include "deMath.h"
#include "deUniquePtr.hpp"
#include "vkRefUtil.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <sstream>
#include <tuple>
#include <vector>
#include <array>
#include <functional>
#include <cmath>
#include <limits>
#include "pipeline/vktPipelineImageUtil.hpp"

namespace vkt
{
namespace robustness
{
namespace
{

using namespace vk;
using namespace de;
using namespace tcu;

using std::function;
using std::ostringstream;
using std::string;
using std::vector;
using std::numeric_limits;
using std::setprecision;
using std::fixed;

typedef function<deUint32(Vec4, Vec4)>				AllocateVertexFn;
typedef function<void(deUint32)>					WriteIndexFn;
struct ValidityInfo
{
	bool	color0;
	bool	color1;
};

deUint32 GetVerticesCountForTriangles(deUint32 tilesX, deUint32 tilesY);
void GenerateTriangles(deUint32 tilesX, deUint32 tilesY, vector<Vec4> colors, const vector<deUint32>& invalidIndices, AllocateVertexFn allocateVertex, WriteIndexFn writeIndex = [](deUint32) { });

typedef vector<VkVertexInputBindingDescription>		VertexBindings;
typedef vector<VkVertexInputAttributeDescription>	VertexAttributes;
struct AttributeData
{
	const void*		data;
	deUint32		size;
};

struct InputInfo
{
	VertexBindings			vertexBindings;
	VertexAttributes		vertexAttributes;
	vector<AttributeData>	data;
	deUint32				vertexCount;
	vector<deUint32>		indices;
};
typedef function<TestStatus(const ConstPixelBufferAccess &)>	ValidationFn;


static const auto		expectedColor		= Vec4(0.25f, 0.0f, 0.75f, 1.0f);		// Expected color input
static const auto		unusedColor			= Vec4(0.75f, 0.0f, 0.25f, 1.0f);		// Unused color attributes
static const auto		outOfRangeColor		= Vec4(0.2f , 0.2f, 0.2f , 1.0f);		// Padding, out of range accesses - never accepted as output
static const auto		validColors			= vector<Vec4>							// Colors accepted as valid in verification shader
{
	expectedColor, unusedColor
};
static const auto		invalidColors		= vector<Vec4>							// Colors accepted as oob access in verification shader
{
	expectedColor, unusedColor, Vec4(0.0f), Vec4(0.0f, 0.0f, 0.0f, 1.0f)
};

static TestStatus		robustness1TestFn(TestContext&				testCtx,
										  Context&					context,
										  const VkDevice			device,
										  DeviceDriverPtr			deviceDriver,
										  const vector<InputInfo>&	inputs,
										  const IVec2&				renderSize);

template<typename T>
class PaddedAlloc
{
	deUint32	m_count, m_paddingCount;
	vector<T>	m_data;
public:
					PaddedAlloc		(deUint32 count, deUint32 paddingCount, const T& paddingValue);
					PaddedAlloc		(const PaddedAlloc<T>&) = delete;

	deUint32		paddedSize() const { return static_cast<deUint32>(m_data.size()); }
	deUint32		paddedStart() const { return m_paddingCount; }
	const T*		paddedData() const { return m_data.data(); }

	deUint32		size() const { return m_count; }
	const T*		data() const { return m_data.data() + m_paddingCount; }

	T&				operator[](const deUint32 index) { return m_data[m_paddingCount + index]; }
	const T&		operator[](const deUint32 index) const { return m_data[m_paddingCount + index]; }

	PaddedAlloc&	operator=		(PaddedAlloc&) = delete;
};

template <typename T>
PaddedAlloc<T>::PaddedAlloc (deUint32 count, deUint32 paddingCount, const T& paddingValue)
		: m_count			(count)
		, m_paddingCount	(paddingCount)
{
	DE_ASSERT((count + 2 * paddingCount) * sizeof(T) <= numeric_limits<deUint32>::max());
	m_data.resize(count + 2 * paddingCount);
	const auto	end			= m_data.size() - 1;
	for(deUint32 i = 0; i < paddingCount; ++i)
	{
		m_data[i]			= paddingValue;
		m_data[end - i]		= paddingValue;
	}
}

typedef function<TestStatus (TestContext&,Context&, const VkDevice device, DeviceDriverPtr deviceDriver)>	TestFn;
struct Robustness1TestInfo
{
	string	name;
	string	description;
	TestFn	testFn;
};
static const auto		renderTargetSize	= IVec2(12, 12);
static const auto		robustness1Tests	= vector<Robustness1TestInfo>
{
	/* Layout of generated vertices vs location invalid vertices always at middle,
	   (3x3 tiles = 4x4 vertices):
			 0	 1	 2	 3	->	  0	  1	  2	  3
			 4 * 5 * 6	 7	->	  4	  7	  8	 11
			 8 * 9 *10	11	->	 12	 13	 14	 15
			12	13	14	15	->	* 5 * 6 * 9 *10
	*/
	{
		"out_of_bounds_stride_0",														// string			name
		"Last elements 4 out of bounds, color with stride 0",							// string			description
		[](TestContext& testContext, Context& context, const VkDevice device, DeviceDriverPtr deviceDriver)
		{
			struct Color
			{
				Vec4 unused;
				Vec4 color;
			};
			const deUint32			totalCount	= GetVerticesCountForTriangles(3, 3);
			PaddedAlloc<Vec4>		positions	(totalCount, 8, outOfRangeColor);
			PaddedAlloc<Color>		colors		(totalCount, 8, { outOfRangeColor, outOfRangeColor });
			PaddedAlloc<Vec4>		color0		(1, 4, outOfRangeColor);
			color0[0]							= expectedColor;
			vector<deUint32>		indices;
			deUint32				writeIndex	= 0;
			GenerateTriangles(
				3u,
				3u,
				{ unusedColor },
				{ 5, 6, 9, 10 },
				[&positions, &colors, &writeIndex](Vec4 position, Vec4 color)
				{
					positions[writeIndex]	= position;
					colors[writeIndex]		= { color, color };
					return writeIndex++;
				},
				[&indices](deUint32 index) { indices.push_back(index); });
			auto bindings	=
			{
				makeVertexInputBindingDescription(0u, sizeof(positions[0]), VK_VERTEX_INPUT_RATE_VERTEX),
				makeVertexInputBindingDescription(1u, 0u, VK_VERTEX_INPUT_RATE_VERTEX),
				makeVertexInputBindingDescription(2u, sizeof(colors[0]), VK_VERTEX_INPUT_RATE_VERTEX)
			};
			auto attributes	=
			{
				makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u),
				makeVertexInputAttributeDescription(1u, 1u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u),
				makeVertexInputAttributeDescription(2u, 2u, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Color, color))
			};
			return robustness1TestFn(
				testContext, context, device, deviceDriver,
				{
					{
						bindings,
						attributes,
						{
							{ positions.data()	, positions.size() * static_cast<deUint32>(sizeof(positions[0]))													},
							{ color0.data()		, color0.size()	* static_cast<deUint32>(sizeof(color0[0]))															},
							{ colors.data()		, (colors.size() - 3) * static_cast<deUint32>(sizeof(colors[0])) - static_cast<deUint32>(sizeof(Color::color) / 2)	}
						},
						static_cast<deUint32>(positions.size()),
						indices
					}
				},
				renderTargetSize);
		}
	},
	{
		"out_of_bounds_stride_16_single_buffer",										// string			name
		"Last 4 elements out of bounds, color with stride 16",							// string			description
		[](TestContext& testContext, Context& context, const VkDevice device, DeviceDriverPtr deviceDriver)
		{
			struct Vertex
			{
				Vec4 position;
				Vec4 unused1;
				Vec4 color1;
				Vec4 color2;
			};
			const deUint32			totalCount	= GetVerticesCountForTriangles(3, 3);
			PaddedAlloc<Vertex>		vertices	(totalCount, 8, { outOfRangeColor, outOfRangeColor, outOfRangeColor, outOfRangeColor });
			deUint32				writeIndex	= 0;
			vector<deUint32>		indices;
			GenerateTriangles(
				3u,
				3u,
				{ expectedColor },
				{ 5, 6, 9, 10 },
				[&vertices, &writeIndex](Vec4 position, Vec4 color)
				{
					vertices[writeIndex]	= { position, unusedColor, color, color };
					return writeIndex++;
				},
				[&indices](deUint32 index) { indices.push_back(index); });
			auto	bindings	=
			{
				makeVertexInputBindingDescription(0u, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
				makeVertexInputBindingDescription(1u, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX)
			};
			auto	attributes	=
			{
				makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, position)),
				makeVertexInputAttributeDescription(1u, 1u, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, color1)),
				makeVertexInputAttributeDescription(2u, 1u, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, color2))
			};
			return robustness1TestFn(
				testContext, context, device, deviceDriver,
				{
					{
						bindings,
						attributes,
						{
							{ vertices.data(),	static_cast<deUint32>(vertices.size() * sizeof(vertices[0]))								},
							{ vertices.data(),	static_cast<deUint32>((vertices.size() - 3) * sizeof(vertices[0]) - sizeof(Vertex::color2))	}
						},
						static_cast<deUint32>(vertices.size()),
						indices,
					}
				},
				renderTargetSize);
		}
	},
	{
		"out_of_bounds_stride_30_middle_of_buffer",										// string			name
		"Last elements 4 out of bounds, color with stride 30, data middle of buffer",	// string			description
		[](TestContext& testContext, Context& context, const VkDevice device, DeviceDriverPtr deviceDriver)
		{
			const vector<deUint32>	invalidIndices	= { 5, 6, 9, 10 };
			const deUint32			invalidCount	= static_cast<deUint32>(invalidIndices.size());
			const deUint32			totalCount		= GetVerticesCountForTriangles(3, 3);
			struct Vertex
			{
				Vec4	position;
				Vec4	color1;
				Vec4	unused1;
				Vec4	color2;
				Vec4	unused2;
			};
			PaddedAlloc<Vertex>		vertices		(totalCount, 8, { outOfRangeColor, outOfRangeColor, outOfRangeColor, outOfRangeColor, outOfRangeColor });
			deUint32				writeIndex		= 0;
			vector<deUint32>		indices;
			GenerateTriangles(
				3u,
				3u,
				{ expectedColor },
				invalidIndices,
				[&vertices, &writeIndex](Vec4 position, Vec4 color)
				{
					vertices[writeIndex] = { position, color, unusedColor, unusedColor, unusedColor };
					return writeIndex++;
				},
				[&indices](deUint32 index) { indices.push_back(index); });
			const auto elementSize	= static_cast<deUint32>(sizeof(Vertex));
			auto	bindings		=
			{
				makeVertexInputBindingDescription(0u, elementSize, VK_VERTEX_INPUT_RATE_VERTEX),
				makeVertexInputBindingDescription(1u, elementSize, VK_VERTEX_INPUT_RATE_VERTEX),
			};
			auto	attributes		=
			{
				makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, vertices.paddedStart() * elementSize + static_cast<deUint32>(offsetof(Vertex, position))),
				makeVertexInputAttributeDescription(1u, 1u, VK_FORMAT_R32G32B32A32_SFLOAT, vertices.paddedStart() * elementSize + static_cast<deUint32>(offsetof(Vertex, color1))),
				makeVertexInputAttributeDescription(2u, 1u, VK_FORMAT_R32G32B32A32_SFLOAT, vertices.paddedStart() * elementSize + static_cast<deUint32>(offsetof(Vertex, color2))),
			};
			return robustness1TestFn(
				testContext, context, device, deviceDriver,
				{
					{
						bindings,
						attributes,
						{
							{ vertices.paddedData()	, vertices.paddedSize() * elementSize					},
							{ vertices.paddedData()	, (vertices.paddedSize() - invalidCount) * elementSize	},
						},
						static_cast<deUint32>(vertices.size()),
						indices
					}
				},
				renderTargetSize);
		}
	},
	{
		"out_of_bounds_stride_8_middle_of_buffer_separate",								// string			name
		"Last elements 4 out of bounds, color with stride 8, data middle of buffer",	// string			description
		[](TestContext& testContext, Context& context, const VkDevice device, DeviceDriverPtr deviceDriver)
		{
			/* NOTE: Out of range entries ('padding') need to be initialized with unusedColor as the spec
			   allows out of range to return any value from within the bound memory range. */
			const vector<deUint32>	invalidIndices	= { 5, 6, 9, 10 };
			const deUint32			invalidCount	= static_cast<deUint32>(invalidIndices.size());
			const deUint32			totalCount		= GetVerticesCountForTriangles(3, 3);
			PaddedAlloc<Vec4>		vertices		(totalCount, 8, unusedColor);
			PaddedAlloc<Vec4>		colors			(2 * totalCount - invalidCount, 8, unusedColor);
			deUint32				writeIndex		= 0;
			vector<deUint32>		indices;
			GenerateTriangles(
				3u,
				3u,
				{ expectedColor },
				invalidIndices,
				[&vertices, &colors, &writeIndex, totalCount](Vec4 position, Vec4 color)
				{
					vertices[writeIndex]					= position;
					colors[writeIndex]						= color;
					if (totalCount + writeIndex < colors.size())
					{
						colors[totalCount + writeIndex]		= color;
					}
					return writeIndex++;
				},
				[&indices](deUint32 index) { indices.push_back(index); });
			const auto elementSize	= static_cast<deUint32>(sizeof(Vec4));
			auto	bindings		=
			{
				makeVertexInputBindingDescription(0u, elementSize, VK_VERTEX_INPUT_RATE_VERTEX),
				makeVertexInputBindingDescription(1u, elementSize, VK_VERTEX_INPUT_RATE_VERTEX)
			};
			auto	attributes		=
			{
				makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, vertices.paddedStart() * elementSize),
				makeVertexInputAttributeDescription(1u, 1u, VK_FORMAT_R32G32B32A32_SFLOAT, colors.paddedStart() * elementSize),
				makeVertexInputAttributeDescription(2u, 1u, VK_FORMAT_R32G32B32A32_SFLOAT, (colors.paddedStart() + totalCount) * elementSize)
			};
			return robustness1TestFn(
				testContext, context, device, deviceDriver,
				{
					{
						bindings,
						attributes,
						{
							{ vertices.paddedData()	, vertices.paddedSize() * elementSize	},
							{ colors.paddedData()	, colors.paddedSize() * elementSize		}
						},
						static_cast<deUint32>(vertices.size()),
						indices
					}
				},
				renderTargetSize);
		}
	}
};

deUint32 GetVerticesCountForTriangles(deUint32 tilesX, deUint32 tilesY)
{
	return (tilesX + 1) * (tilesY + 1);
}

// Generate triangles with invalid vertices placed at end of buffer. NOTE: Assumes invalidIndices to be in ascending order!
void GenerateTriangles (deUint32 tilesX, deUint32 tilesY, vector<Vec4> colors, const vector<deUint32>& invalidIndices, AllocateVertexFn allocateVertex, WriteIndexFn writeIndex)
{
	const auto			tilesStride			= (tilesX + 1);
	const auto			total				= tilesStride * (tilesY + 1);
	const auto			lastValidIndex		= total - 1 - static_cast<deUint32>(invalidIndices.size());
	const Vec2			step				(1.0f / static_cast<float>(tilesX), 1.0f / static_cast<float>(tilesY));

	vector<deUint32>	indexMappings		(total);
	deUint32			nextInvalid			= 0;
	deUint32			writeOffset			= 0;
	deUint32			nextInvalidValue	= nextInvalid < invalidIndices.size() ? invalidIndices[nextInvalid] : total;
	for(deUint32 i = 0; i < total; ++i)
	{
		if (i < nextInvalidValue)
		{
			indexMappings[writeOffset++] = i;
		}
		else
		{
			++nextInvalid;
			nextInvalidValue = nextInvalid < invalidIndices.size() ? invalidIndices[nextInvalid] : total;
		}
	}
	for(deUint32 i = 0; i < static_cast<deUint32>(invalidIndices.size()); ++i)
	{
		indexMappings[writeOffset++] = invalidIndices[i];
	}
	deUint32			count				= 0;
	const auto			vertexFn			= [lastValidIndex, &step, allocateVertex, &count](deUint32 x, deUint32 y, Vec4 color)
	{
		const auto result = allocateVertex(
			Vec4(
				2.0f * static_cast<float>(x) * step.x() - 1.0f,
				2.0f * static_cast<float>(y) * step.y() - 1.0f,
				(count <= lastValidIndex) ? 1.0f : 0.0f,
				1.0f
			), color);
		++count;
		return result;
	};
	vector<deUint32> indices(total);
	for(deUint32 index = 0; index < total; ++index)
	{
		const auto	mapped	= indexMappings[index];
		const auto	x		= mapped % tilesStride;
		const auto	y		= mapped / tilesStride;
		const auto	color	= colors[(x + y) % colors.size()];
		indices[y * tilesStride + x] = vertexFn(x, y, color);
	}
	for(deUint32 y = 0; y < tilesY; ++y)
	{
		for(deUint32 x = 0; x < tilesX; ++x)
		{
			writeIndex(indices[(y	 ) * tilesStride + x	]);
			writeIndex(indices[(y + 1) * tilesStride + x	]);
			writeIndex(indices[(y	 ) * tilesStride + x + 1]);
			writeIndex(indices[(y	 ) * tilesStride + x + 1]);
			writeIndex(indices[(y + 1) * tilesStride + x + 1]);
			writeIndex(indices[(y + 1) * tilesStride + x	]);
		}
	}
}

VkImageCreateInfo makeImageCreateInfo (const tcu::IVec2& size, const VkFormat format, const VkImageUsageFlags usage)
{
	const VkImageCreateInfo imageInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType          sType;
		DE_NULL,									// const void*              pNext;
		(VkImageCreateFlags)0,						// VkImageCreateFlags       flags;
		VK_IMAGE_TYPE_2D,							// VkImageType              imageType;
		format,										// VkFormat                 format;
		makeExtent3D(size.x(), size.y(), 1),		// VkExtent3D               extent;
		1u,											// uint32_t                 mipLevels;
		1u,											// uint32_t                 arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits    samples;
		VK_IMAGE_TILING_OPTIMAL,					// VkImageTiling            tiling;
		usage,										// VkImageUsageFlags        usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode            sharingMode;
		0u,											// uint32_t                 queueFamilyIndexCount;
		DE_NULL,									// const uint32_t*          pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout            initialLayout;
	};
	return imageInfo;
}

static TestStatus robustness1TestFn (TestContext& testCtx, Context& context, const VkDevice device, DeviceDriverPtr	deviceDriver, const vector<InputInfo>& inputs, const IVec2& renderSize)
{
	const auto					colorFormat			= VK_FORMAT_R8G8B8A8_UNORM;
	const DeviceInterface&		vk					= *deviceDriver;
	auto						allocator			= SimpleAllocator(vk, device, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));

	const auto					queueFamilyIndex	= context.getUniversalQueueFamilyIndex();
	VkQueue						queue;
	vk.getDeviceQueue(device, queueFamilyIndex, 0, &queue);

	vector<Move<VkImage>>		colorImages;
	vector<MovePtr<Allocation>>	colorImageAllocs;
	vector<Move<VkImageView>>	colorViews;
	vector<VkImageView>			attachmentViews;
	VkImageCreateInfo			imageCreateInfos[]	=
	{
		makeImageCreateInfo(renderSize, colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
	};
	for(const auto& params : imageCreateInfos)
	{
		auto			image					= createImage(vk, device, &params);
		auto			imageAlloc				= allocator.allocate(getImageMemoryRequirements(vk, device, *image), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(device, *image, imageAlloc->getMemory(), imageAlloc->getOffset()));
		const auto		createInfo				= VkImageViewCreateInfo
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,											// VkStructureType			sType;
			DE_NULL,																			// const void*				pNext;
			0u,																					// VkImageViewCreateFlags	flags;
			*image,																				// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,																// VkImageViewType			viewType;
			colorFormat,																		// VkFormat					format;
			{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
			  VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },					// VkComponentMapping		components;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }										// VkImageSubresourceRange	subresourceRange;
		};
		auto			imageView				= createImageView(vk, device, &createInfo);
		attachmentViews.push_back(*imageView);
		colorImageAllocs.emplace_back(imageAlloc);
		colorViews.emplace_back(imageView);
		colorImages.emplace_back(image);
	}

	const auto		colorAttachmentDescs		= vector<VkAttachmentDescription>
	{
		{
			(VkAttachmentDescriptionFlags)0,												// VkAttachmentDescriptionFlags		flags
			colorFormat,																	// VkFormat							format
			VK_SAMPLE_COUNT_1_BIT,															// VkSampleCountFlagBits			samples
			VK_ATTACHMENT_LOAD_OP_CLEAR,													// VkAttachmentLoadOp				loadOp
			VK_ATTACHMENT_STORE_OP_STORE,													// VkAttachmentStoreOp				storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,												// VkAttachmentLoadOp				stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,												// VkAttachmentStoreOp				stencilStoreOp
			VK_IMAGE_LAYOUT_UNDEFINED,														// VkImageLayout					initialLayout
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL										// VkImageLayout					finalLayout
		},
	};
	const auto		attachmentRefs				= vector<vector<VkAttachmentReference>>
	{
		{ { 0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } },								// pass 0 color
	};
	const auto		subpassDescs				= vector<VkSubpassDescription>
	{
		{
			static_cast<VkSubpassDescriptionFlags>(0),										// VkSubpassDescriptionFlags		flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,												// VkPipelineBindPoint				pipelineBindPoint;
			0u,																				// uint32_t							inputAttachmentCount;
			DE_NULL,																		// const VkAttachmentReference*		pInputAttachments;
			static_cast<deUint32>(attachmentRefs[0].size()),								// uint32_t							colorAttachmentCount;
			attachmentRefs[0].data(),														// const VkAttachmentReference*		pColorAttachments;
			DE_NULL,																		// const VkAttachmentReference*		pResolveAttachments;
			DE_NULL,																		// const VkAttachmentReference*		pDepthStencilAttachment;
			0u,																				// uint32_t							preserveAttachmentCount;
			DE_NULL																			// const uint32_t*					pPreserveAttachments;
		}
	};
	const auto		subpassDeps					= vector<VkSubpassDependency>
	{
		{
			VK_SUBPASS_EXTERNAL,															// uint32_t					srcSubpass;
			0u,																				// uint32_t					dstSubpass;
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,												// VkPipelineStageFlags		srcStageMask;
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,									// VkPipelineStageFlags		dstStageMask;
			0u,																				// VkAccessFlags			srcAccessMask;
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,											// VkAccessFlags			dstAccessMask;
			VK_DEPENDENCY_BY_REGION_BIT														// VkDependencyFlags		dependencyFlags;
		},
		{
			0u,																				// uint32_t					srcSubpass;
			VK_SUBPASS_EXTERNAL,															// uint32_t					dstSubpass;
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,									// VkPipelineStageFlags		srcStageMask;
			VK_PIPELINE_STAGE_TRANSFER_BIT,													// VkPipelineStageFlags		dstStageMask;
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,											// VkAccessFlags			srcAccessMask;
			VK_ACCESS_TRANSFER_READ_BIT,													// VkAccessFlags			dstAccessMask;
			VK_DEPENDENCY_BY_REGION_BIT														// VkDependencyFlags		dependencyFlags;
		}
	};
	const auto		renderPassInfo				= VkRenderPassCreateInfo
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,											// VkStructureType					sType
		DE_NULL,																			// const void*						pNext
		static_cast<VkRenderPassCreateFlags>(0),											// VkRenderPassCreateFlags			flags
		static_cast<deUint32>(colorAttachmentDescs.size()),									// deUint32							attachmentCount
		colorAttachmentDescs.data(),														// const VkAttachmentDescription*	pAttachments
		static_cast<deUint32>(subpassDescs.size()),											// deUint32							subpassCount
		subpassDescs.data(),																// const VkSubpassDescription*		pSubpasses
		static_cast<deUint32>(subpassDeps.size()),											// deUint32							dependencyCount
		subpassDeps.data()																	// const VkSubpassDependency*		pDependencies
	};
	const Unique<VkRenderPass>		pass		(createRenderPass(vk, device, &renderPassInfo, DE_NULL));

	vector<Move<VkBuffer>>			vertexBuffers;
	vector<MovePtr<Allocation>>		vertexBufferAllocs;
	vector<vector<VkBuffer>>		vertexBufferPtrs;
	vector<vector<VkDeviceSize>>	vertexBufferOffsets;
	vector<Move<VkBuffer>>			indexBuffers;
	vector<MovePtr<Allocation>>		indexBufferAllocs;
	vector<Move<VkPipelineLayout>>	pipelineLayouts;
	vector<Move<VkPipeline>>		pipelines;

	const auto								descriptorPool			= DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<deUint32>(inputs.size() * 4u))
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	vector<Move<VkDescriptorSetLayout>>		descriptorSetLayouts;
	vector<Move<VkDescriptorSet>>			descriptorSets;
	vector<vector<VkDescriptorSet>>			descriptorSetPtrs;
	vector<Move<VkShaderModule>>			shaderModules;
	const vector<VkViewport>				viewports				= { makeViewport(renderSize) };
	const vector<VkRect2D>					scissors				= { makeRect2D(renderSize) };
	const vector<string>					vertexNames				= { "vertex-test" };
	const vector<string>					fragmentNames			= { "fragment-test" };
	for(vector<InputInfo>::size_type i = 0; i < inputs.size(); ++i)
	{
		const auto&						input						= inputs[i];
		vector<VkDescriptorSet>			inputDescriptorSets;
		vector<VkDescriptorSetLayout>	setLayouts;
		DescriptorSetLayoutBuilder		builder;
		for(size_t j = 0; j < input.vertexBindings.size(); ++j)
		{
			builder.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL);
		}
		auto							descriptorSetLayout			= builder.build(vk, device);
		setLayouts.push_back(*descriptorSetLayout);
		VkDescriptorSetAllocateInfo		descriptorSetAllocateInfo	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,										// VkStructureType				sType;
			DE_NULL,																			// const void*					pNext;
			*descriptorPool,																	// VkDescriptorPool				descriptorPool;
			1u,																					// deUint32						setLayoutCount;
			&*descriptorSetLayout																// const VkDescriptorSetLayout*	pSetLayouts;
		};
		auto							descriptorSet				= allocateDescriptorSet(vk, device, &descriptorSetAllocateInfo);
		inputDescriptorSets.push_back(*descriptorSet);

		VkPipelineLayoutCreateInfo			pipelineLayoutCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,										// VkStructureType				sType;
			DE_NULL,																			// const void*					pNext;
			0u,																					// VkPipelineLayoutCreateFlags	flags;
			static_cast<deUint32>(setLayouts.size()),											// deUint32						setLayoutCount;
			setLayouts.data(),																	// const VkDescriptorSetLayout*	pSetLayouts;
			0u,																					// deUint32						pushConstantRangeCount;
			DE_NULL																				// const VkPushConstantRange*	pPushConstantRanges;
		};
		auto								pipelineLayout				= createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

		descriptorSetPtrs.push_back(inputDescriptorSets);
		descriptorSetLayouts.emplace_back(descriptorSetLayout);
		descriptorSets.emplace_back(descriptorSet);

		vector<VkBuffer>	inputVertexBufferPtrs;
		for(const auto& data : input.data)
		{
			const auto	createInfo			= makeBufferCreateInfo(data.size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
			auto		buffer				= createBuffer(vk, device, &createInfo);
			auto		bufferAlloc			= allocator.allocate(getBufferMemoryRequirements(vk, device, *buffer), MemoryRequirement::HostVisible);
			VK_CHECK(vk.bindBufferMemory(device, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));
			deMemcpy(bufferAlloc->getHostPtr(), data.data, data.size);
			flushMappedMemoryRange(vk, device, bufferAlloc->getMemory(), bufferAlloc->getOffset(), VK_WHOLE_SIZE);
			inputVertexBufferPtrs.push_back(*buffer);
			vertexBufferAllocs.emplace_back(bufferAlloc);
			vertexBuffers.emplace_back(buffer);
		}
		vertexBufferOffsets.push_back(vector<VkDeviceSize>(inputVertexBufferPtrs.size(), 0ull));
		vertexBufferPtrs.push_back(inputVertexBufferPtrs);

		if (input.indices.size() > 0u)
		{
			const auto	indexDataSize		= input.indices.size() * sizeof(input.indices[0]);
			const auto	createInfo			= makeBufferCreateInfo(indexDataSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
			auto		indexBuffer			= createBuffer(vk, device, &createInfo);
			auto		indexBufferAlloc	= allocator.allocate(getBufferMemoryRequirements(vk, device, *indexBuffer), MemoryRequirement::HostVisible);
			VK_CHECK(vk.bindBufferMemory(device, *indexBuffer, indexBufferAlloc->getMemory(), indexBufferAlloc->getOffset()));
			deMemcpy(indexBufferAlloc->getHostPtr(), input.indices.data(), indexDataSize);
			flushMappedMemoryRange(vk, device, indexBufferAlloc->getMemory(), indexBufferAlloc->getOffset(), VK_WHOLE_SIZE);
			indexBufferAllocs.emplace_back(indexBufferAlloc);
			indexBuffers.emplace_back(indexBuffer);
		}
		const auto&		bindings					= input.vertexBindings;
		const auto&		attributes					= input.vertexAttributes;
		const auto		vertexInputCreateInfo		= VkPipelineVertexInputStateCreateInfo
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,						// VkStructureType							sType;
			DE_NULL,																		// const void*								pNext;
			0u,																				// VkPipelineVertexInputStateCreateFlags	flags;
			static_cast<deUint32>(bindings.size()),											// deUint32									vertexBindingDescriptionCount;
			bindings.data(),																// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			static_cast<deUint32>(attributes.size()),										// deUint32									vertexAttributeDescriptionCount;
			attributes.data()																// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};
		auto			vertexShaderModule			= createShaderModule(vk, device, context.getBinaryCollection().get(vertexNames[i % vertexNames.size()]), 0u);
		auto			fragmentShaderModule		= createShaderModule(vk, device, context.getBinaryCollection().get(fragmentNames[i % fragmentNames.size()]), 0u);
		auto			graphicsPipeline			= makeGraphicsPipeline(
			vk,																				// const DeviceInterface&							vk,
			device,																			// const VkDevice									device,
			*pipelineLayout,																// const VkPipelineLayout							pipelineLayout,
			*vertexShaderModule,															// const VkShaderModule								vertexShaderModule,
			DE_NULL,																		// const VkShaderModule								tessellationControlShaderModule,
			DE_NULL,																		// const VkShaderModule								tessellationEvalShaderModule,
			DE_NULL,																		// const VkShaderModule								geometryShaderModule,
			*fragmentShaderModule,															// const VkShaderModule								fragmentShaderModule,
			*pass,																			// const VkRenderPass								renderPass,
			viewports,																		// const std::vector<VkViewport>&					viewports,
			scissors,																		// const std::vector<VkRect2D>&						scissors,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,											// const VkPrimitiveTopology						topology,
			static_cast<deUint32>(i),														// const deUint32									subpass,
			0u,																				// const deUint32									patchControlPoints,
			&vertexInputCreateInfo);														// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo,

		pipelineLayouts.emplace_back(pipelineLayout);
		pipelines.emplace_back(graphicsPipeline);
		shaderModules.emplace_back(vertexShaderModule);
		shaderModules.emplace_back(fragmentShaderModule);
	}

	const auto						framebufferCreateInfo	= VkFramebufferCreateInfo
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,											// VkStructureType			sType;
		DE_NULL,																			// const void*				pNext;
		0u,																					// VkFramebufferCreateFlags	flags;
		*pass,																				// VkRenderPass				renderPass;
		static_cast<deUint32>(attachmentViews.size()),										// deUint32					attachmentCount;
		attachmentViews.data(),																// const VkImageView*		pAttachments;
		(deUint32)renderSize.x(),															// deUint32					width;
		(deUint32)renderSize.y(),															// deUint32					height;
		1u																					// deUint32					layers;
	};
	const Unique<VkFramebuffer>		framebuffer				(createFramebuffer(vk, device, &framebufferCreateInfo));

	const Unique<VkCommandPool>		commandPool				(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	commandBuffer			(allocateCommandBuffer(vk, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	beginCommandBuffer(vk, *commandBuffer, 0u);
	beginRenderPass(vk, *commandBuffer, *pass, *framebuffer, makeRect2D(renderSize), Vec4(0.0f));
	deUint32						nextIndex				= 0;
	for(vector<InputInfo>::size_type i = 0; i < inputs.size(); ++i)
	{
		const auto&					input					= inputs[i];
		vk.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelines[i]);
		vk.cmdBindDescriptorSets(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayouts[i], 0, static_cast<deUint32>(descriptorSetPtrs[i].size()), descriptorSetPtrs[i].data(), 0, DE_NULL);
		vk.cmdBindVertexBuffers(*commandBuffer, 0, (deUint32)vertexBufferPtrs[i].size(), vertexBufferPtrs[i].data(), vertexBufferOffsets[i].data());
		if (!input.indices.empty())
		{
			vk.cmdBindIndexBuffer(*commandBuffer, *indexBuffers[nextIndex], 0u, VK_INDEX_TYPE_UINT32);
			vk.cmdDrawIndexed(*commandBuffer, static_cast<deUint32>(input.indices.size()), 1u, 0, 0, 0u);
			++nextIndex;
		}
		else
		{
			vk.cmdDraw(*commandBuffer, input.vertexCount, 1u, 0, 0);
		}
		if (i + 1 < inputs.size())
		{
			vk.cmdNextSubpass(*commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
		}
	}
	endRenderPass(vk, *commandBuffer);

	endCommandBuffer(vk, *commandBuffer);
	submitCommandsAndWait(vk, device, queue, *commandBuffer);

	const auto		texture0		= pipeline::readColorAttachment(vk, device, queue, queueFamilyIndex, allocator, *colorImages[0], colorFormat, UVec2(renderSize.x(), renderSize.y()));

	const auto		tex1Access		= texture0->getAccess();
	for(deInt32 y = 0; y < tex1Access.getHeight(); ++y)
	{
		for(deInt32 x = 0; x < tex1Access.getWidth(); ++x)
		{
			if (tex1Access.getPixel(x, y) != Vec4(0.0f, 1.0f, 0.0f, 1.0f))
			{
				testCtx.getLog()
					<< TestLog::ImageSet("Result Images", "")
					<< TestLog::Image("Texture 0 (source)", "", texture0->getAccess())
					<< TestLog::EndImageSet;

				return TestStatus::fail("Image comparison failed.");
			}
		}
	}
	return TestStatus::pass("OK");
}
} // namespace

// Robustness1AccessInstance

template<typename T>
class Robustness1AccessInstance : public vkt::TestInstance
{
public:
								Robustness1AccessInstance	(TestContext&							testCtx,
															 Context&								context,
															 T										device,
															 DeviceDriverPtr						deviceDriver,
															 const Robustness1TestInfo&				testInfo);
	virtual						~Robustness1AccessInstance	() {}
	virtual TestStatus			iterate() override;

private:
	TestContext&												m_testCtx;
	T															m_device;
#ifndef CTS_USES_VULKANSC
	de::MovePtr<vk::DeviceDriver>								m_deviceDriver;
#else
	de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter>	m_deviceDriver;
#endif // CTS_USES_VULKANSC
	const Robustness1TestInfo&									m_testInfo;
};

template<typename T>
Robustness1AccessInstance<T>::Robustness1AccessInstance (TestContext&							testCtx,
														 Context&								context,
														 T										device,
														 DeviceDriverPtr						deviceDriver,
														 const Robustness1TestInfo&				testInfo)
	: vkt::TestInstance				(context)
	, m_testCtx						(testCtx)
	, m_device						(device)
	, m_deviceDriver				(deviceDriver)
	, m_testInfo					(testInfo)
{
}

template<typename T>
TestStatus Robustness1AccessInstance<T>::iterate ()
{
	return m_testInfo.testFn(m_testCtx, m_context, *m_device, m_deviceDriver);
}

// Robustness1AccessTest

class Robustness1AccessTest : public vkt::TestCase
{
public:
							Robustness1AccessTest	(TestContext& testContext, const Robustness1TestInfo &testInfo);
	virtual					~Robustness1AccessTest	() {}

	virtual TestInstance*	createInstance			(Context& context) const override;

protected:
	virtual void			initPrograms			(SourceCollections& programCollection) const override;

private:
	Robustness1TestInfo		m_testInfo;
};

Robustness1AccessTest::Robustness1AccessTest (TestContext &testContext, const Robustness1TestInfo& testInfo)
	: vkt::TestCase(testContext, testInfo.name, testInfo.description),
	  m_testInfo(testInfo)
{
}

TestInstance *Robustness1AccessTest::createInstance (Context &context) const
{
	Move<VkDevice>	device = createRobustBufferAccessDevice(context);
#ifndef CTS_USES_VULKANSC
	DeviceDriverPtr	deviceDriver = DeviceDriverPtr (new DeviceDriver(context.getPlatformInterface(), context.getInstance(), *device, context.getUsedApiVersion()));
#else
	DeviceDriverPtr	deviceDriver = DeviceDriverPtr (new DeviceDriverSC(context.getPlatformInterface(), context.getInstance(), *device, context.getTestContext().getCommandLine(), context.getResourceInterface(), context.getDeviceVulkanSC10Properties(), context.getDeviceProperties(), context.getUsedApiVersion()), vk::DeinitDeviceDeleter( context.getResourceInterface().get(), *device ));
#endif // CTS_USES_VULKANSC

	return new Robustness1AccessInstance<Move<VkDevice>>(m_testCtx, context, device, deviceDriver, m_testInfo);
}

void Robustness1AccessTest::initPrograms (SourceCollections& programCollection) const
{
	ostringstream vertexTestSource;
	vertexTestSource
		<< "#version 310 es\n"
		<< "precision highp float;\n"
		<< "layout(location = 0) in vec4 in_position;\n"
		<< "layout(location = 1) in vec4 in_color0;\n"
		<< "layout(location = 2) in vec4 in_color1;\n"
		<< "layout(location = 0) out vec4 out_color;\n"
		<< "bool is_valid(vec4 color)\n"
		<< "{\n"
		<< "  return\n";
	const auto compare_color = [](ostringstream& out, const string& variable, const Vec4& color)
	{
		out << setprecision(5) << fixed
			<< "	("
			<< variable << ".r - " << color.x() << " < 0.00001 && "
			<< variable << ".g - " << color.y() << " < 0.00001 && "
			<< variable << ".b - " << color.z() << " < 0.00001 && "
			<< variable << ".a - " << color.w() << " < 0.00001"
			<< ")";
	};
	for(vector<Vec4>::size_type i = 0; i < validColors.size(); ++i)
	{
		compare_color(vertexTestSource, "color", validColors[i]);
		vertexTestSource << ((i < validColors.size() - 1) ? " ||\n" : ";\n");
	}
	vertexTestSource
		<< "}\n"
		<< "bool is_invalid(vec4 color)\n"
		<< "{\n"
		<< "  return\n";
	for(vector<Vec4>::size_type i = 0; i < invalidColors.size(); ++i)
	{
		compare_color(vertexTestSource, "color", invalidColors[i]);
		vertexTestSource << ((i < invalidColors.size() - 1) ? " ||\n" : ";\n");
	}
	vertexTestSource
		<< "}\n"
		<< "bool validate(bool should_be_valid, vec4 color0, vec4 color1)\n"
		<< "{\n"
		<< "  return (should_be_valid && is_valid(color0) && is_valid(color1)) || (is_invalid(color0) && is_invalid(color1));\n"
		<< "}\n"
		<< "void main()\n"
		<< "{\n"
		<< "  out_color = validate(in_position.z >= 1.0, in_color0, in_color1) ? vec4(0,1,0,1) : in_color0;"
		<< "  gl_Position = vec4(in_position.xy, 0.0, 1.0);\n"
		<< "}\n";
	programCollection.glslSources.add("vertex-test") << glu::VertexSource(vertexTestSource.str());
	programCollection.glslSources.add("fragment-test") << glu::FragmentSource(
		"#version 310 es\n"
		"precision highp float;\n"
		"layout(location = 0) in vec4 in_color;\n"
		"layout(location = 0) out vec4 out_color;\n"
		"void main() {\n"
		"  out_color = in_color;\n"
		"}\n");
}

TestCaseGroup* createRobustness1VertexAccessTests (TestContext& testCtx)
{
	MovePtr<TestCaseGroup> robustness1AccessTests	(new TestCaseGroup(testCtx, "robustness1_vertex_access", ""));
	for(const auto& info : robustness1Tests)
	{
		robustness1AccessTests->addChild(new Robustness1AccessTest(testCtx, info));
	}

	return robustness1AccessTests.release();
}

} // robustness
} // vkt
