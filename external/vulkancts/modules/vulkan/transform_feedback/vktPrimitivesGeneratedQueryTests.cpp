/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 Google LLC
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
 * \brief VK_EXT_primitives_generated_query Tests
 *//*--------------------------------------------------------------------*/

#include "vktPrimitivesGeneratedQueryTests.hpp"

#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include <functional>
#include <map>
#include <set>

namespace vkt
{
namespace TransformFeedback
{
namespace
{
using namespace vk;

enum
{
	IMAGE_WIDTH		= 64,
	IMAGE_HEIGHT	= IMAGE_WIDTH,
};

enum QueryReadType
{
	QUERY_READ_TYPE_GET,
	QUERY_READ_TYPE_COPY,

	QUERY_READ_TYPE_LAST
};

enum QueryResetType
{
	QUERY_RESET_TYPE_QUEUE,
	QUERY_RESET_TYPE_HOST,

	QUERY_RESET_TYPE_LAST
};

enum QueryResultType
{
	QUERY_RESULT_TYPE_32_BIT,
	QUERY_RESULT_TYPE_64_BIT,
	QUERY_RESULT_TYPE_PGQ_32_XFB_64,
	QUERY_RESULT_TYPE_PGQ_64_XFB_32,

	QUERY_RESULT_TYPE_LAST
};

enum ShaderStage
{
	SHADER_STAGE_VERTEX,
	SHADER_STAGE_TESSELLATION_EVALUATION,
	SHADER_STAGE_GEOMETRY,

	SHADER_STAGE_LAST
};

enum RasterizationCase
{
	RAST_CASE_DEFAULT,
	RAST_CASE_DISCARD,
	RAST_CASE_EMPTY_FRAG,
	RAST_CASE_NO_ATTACHMENT,
	RAST_CASE_COLOR_WRITE_DISABLE_STATIC,
	RAST_CASE_COLOR_WRITE_DISABLE_DYNAMIC,

	RAST_CASE_LAST
};

enum VertexStream
{
	VERTEX_STREAM_DEFAULT	= -1,
	VERTEX_STREAM_0			= 0,
	VERTEX_STREAM_1			= 1,
	VERTEX_STREAM_NONE		= 2,
};

enum CommandBufferCase
{
	CMD_BUF_CASE_SINGLE_DRAW,
	CMD_BUF_CASE_TWO_DRAWS,

	CMD_BUF_CASE_LAST
};

enum QueryOrder
{
	QUERY_ORDER_PGQ_FIRST,
	QUERY_ORDER_XFBQ_FIRST,

	QUERY_ORDER_LAST
};

enum OutsideDraw
{
	OUTSIDE_DRAW_NONE,
	OUTSIDE_DRAW_BEFORE,
	OUTSIDE_DRAW_AFTER,

	OUTSIDE_DRAW_LAST
};

struct TestParameters
{
	QueryReadType		queryReadType;
	QueryResetType		queryResetType;
	QueryResultType		queryResultType;
	ShaderStage			shaderStage;
	deBool				transformFeedback;
	RasterizationCase	rastCase;
	deBool				depthStencilAttachment;
	VkPrimitiveTopology	primitiveTopology;
	VertexStream		pgqStream;
	VertexStream		xfbStream;
	CommandBufferCase	cmdBufCase;
	const uint32_t		queryCount;
	QueryOrder			queryOrder;
	OutsideDraw			outsideDraw;
	const bool			availabilityBit;

	bool		pgqDefault					(void)	const	{ return pgqStream == VERTEX_STREAM_DEFAULT;						}
	bool		xfbDefault					(void)	const	{ return xfbStream == VERTEX_STREAM_DEFAULT;						}
	deUint32	pgqStreamIndex				(void)	const	{ return pgqDefault() ? 0 : static_cast<deUint32>(pgqStream);		}
	deUint32	xfbStreamIndex				(void)	const	{ return xfbDefault() ? 0 : static_cast<deUint32>(xfbStream);		}
	bool		multipleStreams				(void)	const	{ return pgqStreamIndex() != xfbStreamIndex();						}
	bool		nonZeroStreams				(void)	const	{ return (pgqStreamIndex() != 0) || (xfbStreamIndex() != 0);		}
	bool		rastDiscard					(void)	const	{ return rastCase == RAST_CASE_DISCARD;								}
	bool		colorAttachment				(void)	const	{ return !rastDiscard() && rastCase != RAST_CASE_NO_ATTACHMENT;		}
	bool		staticColorWriteDisable		(void)	const	{ return rastCase == RAST_CASE_COLOR_WRITE_DISABLE_STATIC;			}
	bool		dynamicColorWriteDisable	(void)	const	{ return rastCase == RAST_CASE_COLOR_WRITE_DISABLE_DYNAMIC;			}
	bool		colorWriteDisable			(void)	const	{ return staticColorWriteDisable() || dynamicColorWriteDisable();	}
};

enum ConcurrentTestType
{
	CONCURRENT_TEST_TYPE_TWO_XFB_INSIDE_PGQ,
	CONCURRENT_TEST_TYPE_PGQ_SECONDARY_CMD_BUFFER,
	CONCURRENT_TEST_TYPE_PIPELINE_STATISTICS_1,
	CONCURRENT_TEST_TYPE_PIPELINE_STATISTICS_2,
	CONCURRENT_TEST_TYPE_PIPELINE_STATISTICS_3,

	CONCURRENT_TEST_TYPE_LAST
};

struct ConcurrentTestParameters
{
	ConcurrentTestType	concurrentTestType;
	QueryResultType		queryResultType;
	ShaderStage			shaderStage;
	VkPrimitiveTopology	primitiveTopology;
	VertexStream		pgqStream;
	VertexStream		xfbStream;
	bool				indirect;

	bool		pgqDefault					(void)	const	{ return pgqStream == VERTEX_STREAM_DEFAULT;						}
	bool		xfbDefault					(void)	const	{ return xfbStream == VERTEX_STREAM_DEFAULT;						}
	deUint32	pgqStreamIndex				(void)	const	{ return pgqDefault() ? 0 : static_cast<deUint32>(pgqStream);		}
	deUint32	xfbStreamIndex				(void)	const	{ return xfbDefault() ? 0 : static_cast<deUint32>(xfbStream);		}
	bool		multipleStreams				(void)	const	{ return pgqStreamIndex() != xfbStreamIndex();						}
	bool		nonZeroStreams				(void)	const	{ return (pgqStreamIndex() != 0) || (xfbStreamIndex() != 0);		}
};

struct TopologyInfo
{
	deUint32							primitiveSize;		// Size of the primitive.
	deBool								hasAdjacency;		// True if topology has adjacency.
	const char*							inputString;		// Layout qualifier identifier for geometry shader input.
	const char*							outputString;		// Layout qualifier identifier for geometry shader output.
	std::function<deUint64(deUint64)>	getNumPrimitives;	// Number of primitives generated.
	std::function<deUint64(deUint64)>	getNumVertices;		// Number of vertices generated.
};

const std::map<VkPrimitiveTopology, TopologyInfo> topologyData =
{
	{ VK_PRIMITIVE_TOPOLOGY_POINT_LIST,						{ 1, DE_FALSE,	"points",				"points",			[](deUint64 vtxCount) { return vtxCount;				}, [](deUint64 primCount) {	return primCount;			} } },
	{ VK_PRIMITIVE_TOPOLOGY_LINE_LIST,						{ 2, DE_FALSE,	"lines",				"line_strip",		[](deUint64 vtxCount) { return vtxCount / 2u;			}, [](deUint64 primCount) {	return primCount * 2u;		} } },
	{ VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,						{ 2, DE_FALSE,	"lines",				"line_strip",		[](deUint64 vtxCount) { return vtxCount - 1u;			}, [](deUint64 primCount) {	return primCount + 1u;		} } },
	{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,					{ 3, DE_FALSE,	"triangles",			"triangle_strip",	[](deUint64 vtxCount) { return vtxCount / 3u;			}, [](deUint64 primCount) {	return primCount * 3u;		} } },
	{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,					{ 3, DE_FALSE,	"triangles",			"triangle_strip",	[](deUint64 vtxCount) { return vtxCount - 2u;			}, [](deUint64 primCount) {	return primCount + 2u;		} } },
	{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,					{ 3, DE_FALSE,	"triangles",			"triangle_strip",	[](deUint64 vtxCount) { return vtxCount - 2u;			}, [](deUint64 primCount) {	return primCount + 2u;		} } },
	{ VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,		{ 2, DE_TRUE,	"lines_adjacency",		"line_strip",		[](deUint64 vtxCount) { return vtxCount / 4u;			}, [](deUint64 primCount) {	return primCount * 4u;		} } },
	{ VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,		{ 2, DE_TRUE,	"lines_adjacency",		"line_strip",		[](deUint64 vtxCount) { return vtxCount - 3u;			}, [](deUint64 primCount) {	return primCount + 3u;		} } },
	{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,	{ 3, DE_TRUE,	"triangles_adjacency",	"triangle_strip",	[](deUint64 vtxCount) { return vtxCount / 6u;			}, [](deUint64 primCount) {	return primCount * 6u;		} } },
	{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY,	{ 3, DE_TRUE,	"triangles_adjacency",	"triangle_strip",	[](deUint64 vtxCount) { return (vtxCount - 4u) / 2u;	}, [](deUint64 primCount) {	return primCount * 2u + 4;	} } },
	{ VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,						{ 3, DE_FALSE,	"ERROR",				"ERROR",			[](deUint64 vtxCount) { return vtxCount / 3u;			}, [](deUint64 primCount) {	return primCount * 3u;		} } },
};

class PrimitivesGeneratedQueryTestInstance : public vkt::TestInstance
{
public:
							PrimitivesGeneratedQueryTestInstance	(vkt::Context &context, const TestParameters& parameters)
								: vkt::TestInstance	(context)
								, m_parameters		(parameters)
							{
							}

private:
	tcu::TestStatus			iterate									(void);
	VkFormat				selectDepthStencilFormat				(void);
	Move<VkPipeline>		makeGraphicsPipeline					(const DeviceInterface&	vk,
																	 const VkDevice device,
																	 const VkRenderPass renderPass);
	void					fillVertexBuffer						(tcu::Vec2* vertices,
																	 const deUint64 primitivesGenerated);
	const TestParameters	m_parameters;
};

tcu::TestStatus PrimitivesGeneratedQueryTestInstance::iterate (void)
{
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					device				= m_context.getDevice();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkQueue					queue				= m_context.getUniversalQueue();
	Allocator&						allocator			= m_context.getDefaultAllocator();

	using ImageVec = std::vector<Move<VkImage>>;
	using AllocVec = std::vector<de::MovePtr<Allocation>>;

	const VkFormat					colorFormat			= m_parameters.colorAttachment() ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_UNDEFINED;
	ImageVec						colorImages;
	AllocVec						colorImageAllocations;

	if (m_parameters.colorAttachment())
	{
		const VkImageCreateInfo colorImageCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType			sType
			DE_NULL,									// const void*				pNext
			0u,											// VkImageCreateFlags		flags
			VK_IMAGE_TYPE_2D,							// VkImageType				imageType
			colorFormat,								// VkFormat					format
			makeExtent3D(IMAGE_WIDTH, IMAGE_HEIGHT, 1),	// VkExtent3D				extent
			1u,											// deUint32					mipLevels
			1u,											// deUint32					arrayLayers
			VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits	samples
			VK_IMAGE_TILING_OPTIMAL,					// VkImageTiling			tiling
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,		// VkImageUsageFlags		usage
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode			sharingMode
			0u,											// deUint32					queueFamilyIndexCount
			DE_NULL,									// const deUint32*			pQueueFamilyIndices
			VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			initialLayout
		};

		for (uint32_t queryIdx = 0u; queryIdx < m_parameters.queryCount; ++queryIdx)
		{
			auto colorImage				= makeImage(vk, device, colorImageCreateInfo);
			auto colorImageAllocation	= bindImage(vk, device, allocator, *colorImage, MemoryRequirement::Any);

			colorImages.emplace_back(colorImage);
			colorImageAllocations.emplace_back(colorImageAllocation);
		}
	}

	const VkFormat					dsFormat			= m_parameters.depthStencilAttachment ? PrimitivesGeneratedQueryTestInstance::selectDepthStencilFormat() : VK_FORMAT_UNDEFINED;

	if (m_parameters.depthStencilAttachment && dsFormat == VK_FORMAT_UNDEFINED)
		return tcu::TestStatus::fail("VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT feature must be supported for at least one of VK_FORMAT_D24_UNORM_S8_UINT and VK_FORMAT_D32_SFLOAT_S8_UINT.");

	ImageVec						dsImages;
	AllocVec						dsImageAllocations;

	if (m_parameters.depthStencilAttachment)
	{
		const VkImageCreateInfo dsImageCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType
			DE_NULL,										// const void*				pNext
			0u,												// VkImageCreateFlags		flags
			VK_IMAGE_TYPE_2D,								// VkImageType				imageType
			dsFormat,										// VkFormat					format
			makeExtent3D(IMAGE_WIDTH, IMAGE_HEIGHT, 1),		// VkExtent3D				extent
			1u,												// deUint32					mipLevels
			1u,												// deUint32					arrayLayers
			VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits	samples
			VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,	// VkImageUsageFlags		usage
			VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode
			0u,												// deUint32					queueFamilyIndexCount
			DE_NULL,										// const deUint32*			pQueueFamilyIndices
			VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			initialLayout
		};

		for (uint32_t queryIdx = 0u; queryIdx < m_parameters.queryCount; ++queryIdx)
		{
			auto dsImage			= makeImage(vk, device, dsImageCreateInfo);
			auto dsImageAllocation	= bindImage(vk, device, allocator, *dsImage, MemoryRequirement::Any);

			dsImages.emplace_back(dsImage);
			dsImageAllocations.emplace_back(dsImageAllocation);
		}
	}

	const VkDeviceSize				primitivesGenerated = 32;
	const deUint32					baseMipLevel		= 0;
	const deUint32					levelCount			= 1;
	const deUint32					baseArrayLayer		= 0;
	const deUint32					layerCount			= 1;

	using ImageViewVec = std::vector<Move<VkImageView>>;

	ImageViewVec					colorImageViews;
	ImageViewVec					dsImageViews;

	for (uint32_t queryIdx = 0u; queryIdx < m_parameters.queryCount; ++queryIdx)
	{
		if (m_parameters.colorAttachment())
		{
			const VkImageSubresourceRange colorSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, baseMipLevel, levelCount, baseArrayLayer, layerCount);
			auto colorImageView = makeImageView(vk, device, *colorImages.at(queryIdx), VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange);
			colorImageViews.emplace_back(colorImageView);
		}

		if (m_parameters.depthStencilAttachment)
		{
			const VkImageSubresourceRange dsSubresourceRange = makeImageSubresourceRange((VK_IMAGE_ASPECT_DEPTH_BIT | vk::VK_IMAGE_ASPECT_STENCIL_BIT), baseMipLevel, levelCount, baseArrayLayer, layerCount);
			auto dsImageView = makeImageView(vk, device, *dsImages.at(queryIdx), VK_IMAGE_VIEW_TYPE_2D, dsFormat, dsSubresourceRange);
			dsImageViews.emplace_back(dsImageView);
		}
	}

	using FramebufferVec = std::vector<Move<VkFramebuffer>>;

	const Unique<VkRenderPass>		renderPass			(makeRenderPass(vk, device, colorFormat, dsFormat, VK_ATTACHMENT_LOAD_OP_DONT_CARE));
	const Unique<VkPipeline>		pipeline			(PrimitivesGeneratedQueryTestInstance::makeGraphicsPipeline(vk, device, *renderPass));
	FramebufferVec					framebuffers;

	for (uint32_t queryIdx = 0u; queryIdx < m_parameters.queryCount; ++queryIdx)
	{
		std::vector<VkImageView> imageViews;

		if (m_parameters.colorAttachment())
			imageViews.push_back(*colorImageViews.at(queryIdx));

		if (m_parameters.depthStencilAttachment)
			imageViews.push_back(*dsImageViews.at(queryIdx));

		auto framebuffer = makeFramebuffer(vk, device, *renderPass, (deUint32)imageViews.size(), imageViews.data(), IMAGE_WIDTH, IMAGE_HEIGHT);
		framebuffers.emplace_back(framebuffer);
	}

	Move<VkBuffer>					vtxBuffer;
	de::MovePtr<Allocation>			vtxBufferAlloc;

	{
		const VkBufferUsageFlags		usage				= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		const std::vector<deUint32>		queueFamilyIndices	(1, queueFamilyIndex);
		const VkDeviceSize				vtxBufferSize		= topologyData.at(m_parameters.primitiveTopology).getNumVertices(primitivesGenerated) * sizeof(tcu::Vec2);
		const VkBufferCreateInfo		createInfo			= makeBufferCreateInfo(vtxBufferSize, usage, queueFamilyIndices);

		vtxBuffer			= createBuffer(vk, device, &createInfo);
		vtxBufferAlloc		= allocator.allocate(getBufferMemoryRequirements(vk, device, *vtxBuffer), MemoryRequirement::HostVisible);
	}

	const VkCommandPoolCreateFlags	cmdPoolCreateFlags	= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	const VkCommandBufferLevel		cmdBufferLevel		= VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	const Unique<VkCommandPool>		cmdPool				(createCommandPool(vk, device, cmdPoolCreateFlags, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer			(allocateCommandBuffer(vk, device, *cmdPool, cmdBufferLevel));
	const auto						resetCmdBuffer		= allocateCommandBuffer(vk, device, *cmdPool, cmdBufferLevel);

	const bool						pgq64				= (m_parameters.queryResultType == QUERY_RESULT_TYPE_64_BIT ||
														   m_parameters.queryResultType == QUERY_RESULT_TYPE_PGQ_64_XFB_32);
	const bool						xfb64				= (m_parameters.queryResultType == QUERY_RESULT_TYPE_64_BIT ||
														   m_parameters.queryResultType == QUERY_RESULT_TYPE_PGQ_32_XFB_64);
	const VkQueryResultFlags		availabilityFlags	= (m_parameters.availabilityBit ? VK_QUERY_RESULT_WITH_AVAILABILITY_BIT : 0);
	const size_t					pgqValuesPerQuery	= (m_parameters.availabilityBit ? 2 : 1);
	const size_t					xfbValuesPerQuery	= (m_parameters.availabilityBit ? 3 : 2);
	const size_t					pgqResultSize		= (pgq64 ? sizeof(deUint64) : sizeof(deUint32)) * pgqValuesPerQuery;
	const size_t					xfbResultSize		= (xfb64 ? sizeof(deUint64) : sizeof(deUint32)) * xfbValuesPerQuery;
	const size_t					pgqResultBufferSize	= pgqResultSize * m_parameters.queryCount;
	const size_t					xfbResultBufferSize	= xfbResultSize * m_parameters.queryCount;
	const VkQueryResultFlags		pgqResultWidthBit	= pgq64 ? VK_QUERY_RESULT_64_BIT : 0;
	const VkQueryResultFlags		xfbResultWidthBit	= xfb64 ? VK_QUERY_RESULT_64_BIT : 0;
	const VkQueryResultFlags		pgqResultFlags		= VK_QUERY_RESULT_WAIT_BIT | pgqResultWidthBit | availabilityFlags;
	const VkQueryResultFlags		xfbResultFlags		= VK_QUERY_RESULT_WAIT_BIT | xfbResultWidthBit | availabilityFlags;

	std::vector<deUint8>			pgqResults			(pgqResultBufferSize, 255u);
	std::vector<deUint8>			xfbResults			(xfbResultBufferSize, 255u);

	const VkQueryPoolCreateInfo		pgqCreateInfo		=
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,	// VkStructureType					sType
		DE_NULL,									// const void*						pNext
		0u,											// VkQueryPoolCreateFlags			flags
		VK_QUERY_TYPE_PRIMITIVES_GENERATED_EXT,		// VkQueryType						queryType
		m_parameters.queryCount,					// deUint32							queryCount
		0u,											// VkQueryPipelineStatisticFlags	pipelineStatistics
	};

	const Unique<VkQueryPool>		pgqPool				(createQueryPool(vk, device, &pgqCreateInfo));
	Move<VkQueryPool>				xfbPool;

	if (m_parameters.transformFeedback)
	{
		const VkQueryPoolCreateInfo xfbCreateInfo =
		{
			VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,		// VkStructureType					sType
			DE_NULL,										// const void*						pNext
			0u,												// VkQueryPoolCreateFlags			flags
			VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT,	// VkQueryType						queryType
			m_parameters.queryCount,						// deUint32							queryCount
			0u,												// VkQueryPipelineStatisticFlags	pipelineStatistics
		};

		xfbPool = createQueryPool(vk, device, &xfbCreateInfo);
	}

	Move<VkBuffer>					pgqResultsBuffer;
	Move<VkBuffer>					xfbResultsBuffer;
	de::MovePtr<Allocation>			pgqResultsBufferAlloc;
	de::MovePtr<Allocation>			xfbResultsBufferAlloc;

	if (m_parameters.queryReadType == QUERY_READ_TYPE_COPY)
	{
		const VkBufferUsageFlags	usage				= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		const std::vector<deUint32>	queueFamilyIndices	(1, queueFamilyIndex);
		const VkBufferCreateInfo	pgqBufferCreateInfo	= makeBufferCreateInfo(pgqResultBufferSize, usage, queueFamilyIndices);

		pgqResultsBuffer		= createBuffer(vk, device, &pgqBufferCreateInfo);
		pgqResultsBufferAlloc	= allocator.allocate(getBufferMemoryRequirements(vk, device, *pgqResultsBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(device, *pgqResultsBuffer, pgqResultsBufferAlloc->getMemory(), pgqResultsBufferAlloc->getOffset()));

		if (m_parameters.transformFeedback)
		{
			const VkBufferCreateInfo xfbBufferCreateInfo = makeBufferCreateInfo(xfbResultBufferSize, usage, queueFamilyIndices);

			xfbResultsBuffer		= createBuffer(vk, device, &xfbBufferCreateInfo);
			xfbResultsBufferAlloc	= allocator.allocate(getBufferMemoryRequirements(vk, device, *xfbResultsBuffer), MemoryRequirement::HostVisible);

			VK_CHECK(vk.bindBufferMemory(device, *xfbResultsBuffer, xfbResultsBufferAlloc->getMemory(), xfbResultsBufferAlloc->getOffset()));
		}
	}

	const VkDeviceSize				primitivesWritten	= primitivesGenerated - 3;
	const VkDeviceSize				verticesWritten		= topologyData.at(m_parameters.primitiveTopology).getNumVertices(primitivesWritten);
	const VkDeviceSize				primitiveSize		= m_parameters.nonZeroStreams() ? 1 : topologyData.at(m_parameters.primitiveTopology).primitiveSize;
	const VkDeviceSize				bytesPerVertex		= 4 * sizeof(float);
	const VkDeviceSize				xfbBufferSize		= primitivesWritten * primitiveSize * bytesPerVertex;

	using BufferVec = std::vector<Move<VkBuffer>>;

	BufferVec						xfbBuffers;
	AllocVec						xfbBufferAllocs;

	if (m_parameters.transformFeedback)
	{
		const VkBufferUsageFlags	usage				= VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT;
		const std::vector<deUint32>	queueFamilyIndices	(1, queueFamilyIndex);
		const VkBufferCreateInfo	createInfo			= makeBufferCreateInfo(xfbBufferSize, usage, queueFamilyIndices);

		for (uint32_t queryIdx = 0u; queryIdx < m_parameters.queryCount; ++queryIdx)
		{
			auto xfbBuffer		= createBuffer(vk, device, &createInfo);
			auto xfbBufferAlloc	= allocator.allocate(getBufferMemoryRequirements(vk, device, *xfbBuffer), MemoryRequirement::HostVisible);

			VK_CHECK(vk.bindBufferMemory(device, *xfbBuffer, xfbBufferAlloc->getMemory(), xfbBufferAlloc->getOffset()));

			xfbBuffers.emplace_back(xfbBuffer);
			xfbBufferAllocs.emplace_back(xfbBufferAlloc);
		}
	}

	fillVertexBuffer(static_cast<tcu::Vec2*>(vtxBufferAlloc.get()->getHostPtr()), primitivesGenerated);

	VK_CHECK(vk.bindBufferMemory(device, *vtxBuffer, vtxBufferAlloc->getMemory(), vtxBufferAlloc->getOffset()));

	// After query pool creation, each query must be reset before it is used.
	//
	// When resetting them using a queue, we will submit a separate command buffer with the reset operation and wait for it to
	// complete. This will make sure queries are properly reset before we attempt to get results from them. This is needed because
	// we're not going to wait for any fence when using vkGetQueryPoolResults, so there's a potential race condition with
	// vkGetQueryPoolResults attempting to get results before queries are properly reset, which is against the spec.
	if (m_parameters.queryResetType == QUERY_RESET_TYPE_QUEUE)
	{
		beginCommandBuffer(vk, *resetCmdBuffer);
		vk.cmdResetQueryPool(*resetCmdBuffer, *pgqPool, 0u, m_parameters.queryCount);
		if (m_parameters.transformFeedback)
			vk.cmdResetQueryPool(*resetCmdBuffer, *xfbPool, 0u, m_parameters.queryCount);
		endCommandBuffer(vk, *resetCmdBuffer);
		submitCommandsAndWait(vk, device, queue, *resetCmdBuffer);
	}

	beginCommandBuffer(vk, *cmdBuffer);
	{
		const VkDeviceSize	vertexBufferOffset	= static_cast<VkDeviceSize>(0);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

		vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vtxBuffer.get(), &vertexBufferOffset);

		for (uint32_t queryIdx = 0u; queryIdx < m_parameters.queryCount; ++queryIdx)
		{
			beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffers.at(queryIdx), makeRect2D(makeExtent2D(IMAGE_WIDTH, IMAGE_HEIGHT)));
			{
				const VkQueryControlFlags	queryControlFlags		= 0;

				const deUint32				firstCounterBuffer		= 0;
				const deUint32				counterBufferCount		= 0;
				const VkBuffer*				counterBuffers			= DE_NULL;
				const VkDeviceSize*			counterBufferOffsets	= DE_NULL;

				const deUint32				vertexCount				= static_cast<deUint32>(topologyData.at(m_parameters.primitiveTopology).getNumVertices(primitivesGenerated));
				const deUint32				instanceCount			= 1u;
				const deUint32				firstVertex				= 0u;
				const deUint32				firstInstance			= 0u;

				if (m_parameters.dynamicColorWriteDisable())
				{
					const deUint32	attachmentCount = 1;
					const VkBool32	colorWriteEnables = VK_FALSE;

					vk.cmdSetColorWriteEnableEXT(*cmdBuffer, attachmentCount, &colorWriteEnables);
				}

				if (m_parameters.outsideDraw == OUTSIDE_DRAW_BEFORE)
					vk.cmdDraw(*cmdBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

				if (m_parameters.queryOrder == QUERY_ORDER_PGQ_FIRST)
				{
					if (m_parameters.pgqDefault())
						vk.cmdBeginQuery(*cmdBuffer, *pgqPool, queryIdx, queryControlFlags);
					else
						vk.cmdBeginQueryIndexedEXT(*cmdBuffer, *pgqPool, queryIdx, queryControlFlags, m_parameters.pgqStreamIndex());
				}

				if (m_parameters.transformFeedback)
				{
					const deUint32		firstBinding	= 0;
					const deUint32		bindingCount	= 1;
					const VkDeviceSize	offset			= 0;

					vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, firstBinding, bindingCount, &*xfbBuffers.at(queryIdx), &offset, &xfbBufferSize);

					if (m_parameters.xfbDefault())
						vk.cmdBeginQuery(*cmdBuffer, *xfbPool, queryIdx, queryControlFlags);
					else
						vk.cmdBeginQueryIndexedEXT(*cmdBuffer, *xfbPool, queryIdx, queryControlFlags, m_parameters.xfbStreamIndex());

					vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, firstCounterBuffer, counterBufferCount, counterBuffers, counterBufferOffsets);
				}

				if (m_parameters.queryOrder != QUERY_ORDER_PGQ_FIRST)
				{
					if (m_parameters.pgqDefault())
						vk.cmdBeginQuery(*cmdBuffer, *pgqPool, queryIdx, queryControlFlags);
					else
						vk.cmdBeginQueryIndexedEXT(*cmdBuffer, *pgqPool, queryIdx, queryControlFlags, m_parameters.pgqStreamIndex());
				}

				vk.cmdDraw(*cmdBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

				if (m_parameters.cmdBufCase == CMD_BUF_CASE_TWO_DRAWS)
					vk.cmdDraw(*cmdBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

				if (m_parameters.pgqDefault())
					vk.cmdEndQuery(*cmdBuffer, *pgqPool, queryIdx);
				else
					vk.cmdEndQueryIndexedEXT(*cmdBuffer, *pgqPool, queryIdx, m_parameters.pgqStreamIndex());

				if (m_parameters.transformFeedback)
				{
					if (m_parameters.xfbDefault())
						vk.cmdEndQuery(*cmdBuffer, *xfbPool, queryIdx);
					else
						vk.cmdEndQueryIndexedEXT(*cmdBuffer, *xfbPool, queryIdx, m_parameters.xfbStreamIndex());

					vk.cmdEndTransformFeedbackEXT(*cmdBuffer, firstCounterBuffer, counterBufferCount, counterBuffers, counterBufferOffsets);
				}

				if (m_parameters.outsideDraw == OUTSIDE_DRAW_AFTER)
					vk.cmdDraw(*cmdBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
			}
			endRenderPass(vk, *cmdBuffer);
		}

		if (m_parameters.queryReadType == QUERY_READ_TYPE_COPY)
		{
			VkBufferMemoryBarrier bufferBarrier =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType
				DE_NULL,									// const void*		pNext
				VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask
				VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask
				VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex
				VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex
				*pgqResultsBuffer,							// VkBuffer			buffer
				0u,											// VkDeviceSize		offset
				VK_WHOLE_SIZE								// VkDeviceSize		size
			};

			vk.cmdCopyQueryPoolResults(*cmdBuffer, *pgqPool, 0u, m_parameters.queryCount, *pgqResultsBuffer, 0u, pgqResultSize, pgqResultFlags);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &bufferBarrier, 0u, DE_NULL);

			if (m_parameters.transformFeedback)
			{
				bufferBarrier.buffer = *xfbResultsBuffer;
				vk.cmdCopyQueryPoolResults(*cmdBuffer, *xfbPool, 0u, m_parameters.queryCount, *xfbResultsBuffer, 0u, xfbResultSize, xfbResultFlags);
				vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &bufferBarrier, 0u, DE_NULL);
			}
		}
	}
	vk::endCommandBuffer(vk, *cmdBuffer);

	// After query pool creation, each query must be reset before it is used.
	if (m_parameters.queryResetType == QUERY_RESET_TYPE_HOST)
	{
		vk.resetQueryPool(device, *pgqPool, 0u, m_parameters.queryCount);

		if (m_parameters.transformFeedback)
			vk.resetQueryPool(device, *xfbPool, 0u, m_parameters.queryCount);
	}

	const auto fence = submitCommands(vk, device, queue, *cmdBuffer);

	// To make it more interesting, attempt to get results with WAIT before waiting for the fence.
	if (m_parameters.queryReadType == QUERY_READ_TYPE_GET)
	{
		vk.getQueryPoolResults(device, *pgqPool, 0u, m_parameters.queryCount, pgqResults.size(), pgqResults.data(), pgqResultSize, pgqResultFlags);

		if (m_parameters.transformFeedback)
			vk.getQueryPoolResults(device, *xfbPool, 0u, m_parameters.queryCount, xfbResults.size(), xfbResults.data(), xfbResultSize, xfbResultFlags);
	}

	waitForFence(vk, device, *fence);

	if (m_parameters.queryReadType == QUERY_READ_TYPE_COPY)
	{
		invalidateAlloc(vk, device, *pgqResultsBufferAlloc);
		deMemcpy(pgqResults.data(), pgqResultsBufferAlloc->getHostPtr(), pgqResults.size());

		if (m_parameters.transformFeedback)
		{
			invalidateAlloc(vk, device, *xfbResultsBufferAlloc);
			deMemcpy(xfbResults.data(), xfbResultsBufferAlloc->getHostPtr(), xfbResults.size());
		}
	}

	// Validate counters.
	for (uint32_t queryIdx = 0u; queryIdx < m_parameters.queryCount; ++queryIdx)
	{
		union QueryResults
		{
			deUint32	elements32[3];
			deUint64	elements64[3];
		};

		const QueryResults*	pgqCounters		= reinterpret_cast<QueryResults*>(pgqResults.data() + queryIdx * pgqResultSize);
		const QueryResults*	xfbCounters		= reinterpret_cast<QueryResults*>(xfbResults.data() + queryIdx * xfbResultSize);
		const deUint64		pgqGenerated	= pgq64 ? pgqCounters->elements64[0] : static_cast<deUint64>(pgqCounters->elements32[0]);
		const deUint64		xfbWritten		= xfb64 ? xfbCounters->elements64[0] : static_cast<deUint64>(xfbCounters->elements32[0]);
		const deUint64		xfbGenerated	= xfb64 ? xfbCounters->elements64[1] : static_cast<deUint64>(xfbCounters->elements32[1]);
		const deUint32		drawCount		= (m_parameters.cmdBufCase == CMD_BUF_CASE_TWO_DRAWS) ? 2u : 1u;
		const uint64_t		pgqAvailability	= (m_parameters.availabilityBit ? (pgq64 ? pgqCounters->elements64[1] : static_cast<deUint64>(pgqCounters->elements32[1])) : 1);
		const uint64_t		xfbAvailability	= (m_parameters.availabilityBit ? (xfb64 ? xfbCounters->elements64[2] : static_cast<deUint64>(xfbCounters->elements32[2])) : 1);
		tcu::TestLog&		log				= m_context.getTestContext().getLog();

		if (queryIdx == 0u)
		{
			log	<<	tcu::TestLog::Message
				<<	"primitivesGenerated: " << primitivesGenerated << "\n"
				<<	"primitivesWritten: " << primitivesWritten << "\n"
				<<	"verticesWritten: " << verticesWritten << "\n"
				<<	"xfbBufferSize: " << xfbBufferSize << "\n"
				<<	tcu::TestLog::EndMessage;
		}

		const std::string logPrefix = std::string("[Query ") + std::to_string(queryIdx) + "] ";

		log << tcu::TestLog::Message << logPrefix << "PGQ: Generated " << pgqGenerated << tcu::TestLog::EndMessage;

		if (m_parameters.transformFeedback)
			log << tcu::TestLog::Message << logPrefix << "XFB: Written " << xfbWritten << ", generated " << xfbGenerated << tcu::TestLog::EndMessage;

		if (pgqGenerated != primitivesGenerated * drawCount)
		{
			const std::string message = logPrefix + "pgqGenerated == " + de::toString(pgqGenerated) + ", expected " + de::toString(primitivesGenerated * drawCount);
			return tcu::TestStatus::fail(message);
		}

		if (pgqAvailability != 1)
		{
			const std::string message = logPrefix + "pgqAvailability == " + de::toString(pgqAvailability) + ", expected 1";
			return tcu::TestStatus::fail(message);
		}

		if (m_parameters.transformFeedback)
		{
			if (xfbGenerated != primitivesGenerated * drawCount)
			{
				const std::string message = logPrefix + "xfbGenerated == " + de::toString(xfbGenerated) + ", expected " + de::toString(primitivesGenerated * drawCount);
				return tcu::TestStatus::fail(message);
			}

			if (xfbWritten != primitivesWritten)
			{
				const std::string message = logPrefix + "xfbWritten == " + de::toString(xfbWritten) + ", expected " + de::toString(primitivesWritten);
				return tcu::TestStatus::fail(message);
			}

			if (xfbWritten != (primitivesGenerated - 3))
			{
				const std::string message = logPrefix + "xfbWritten == " + de::toString(xfbWritten) + ", expected " + de::toString(primitivesGenerated - 3);
				return tcu::TestStatus::fail(message);
			}

			if (xfbAvailability != 1)
			{
				const std::string message = logPrefix + "xfbAvailability == " + de::toString(xfbAvailability) + ", expected 1";
				return tcu::TestStatus::fail(message);
			}
		}
	}

	return tcu::TestStatus::pass("Counters OK");
}

VkFormat PrimitivesGeneratedQueryTestInstance::selectDepthStencilFormat (void)
{
	constexpr VkFormat			formats[]		=
	{
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT
	};

	const InstanceInterface&	vki				= m_context.getInstanceInterface();
	const VkPhysicalDevice		physicalDevice	= m_context.getPhysicalDevice();

	for (VkFormat format : formats)
	{
		const VkFormatFeatureFlags features = getPhysicalDeviceFormatProperties(vki, physicalDevice, format).optimalTilingFeatures;

		if (features & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
			return format;
	}

	return VK_FORMAT_UNDEFINED;
}

Move<VkPipeline> PrimitivesGeneratedQueryTestInstance::makeGraphicsPipeline (const DeviceInterface&	vk, const VkDevice device, const VkRenderPass renderPass)
{
	const VkDescriptorSetLayout						descriptorSetLayout				= DE_NULL;
	const Unique<VkPipelineLayout>					pipelineLayout					(makePipelineLayout(vk, device, descriptorSetLayout));
	const std::vector<VkViewport>					viewports						(1, makeViewport(makeExtent2D(IMAGE_WIDTH, IMAGE_HEIGHT)));
	const std::vector<VkRect2D>						scissors						(1, makeRect2D(makeExtent2D(IMAGE_WIDTH, IMAGE_HEIGHT)));
	const deUint32									subpass							= 0u;
	const deUint32									patchControlPoints				= topologyData.at(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST).primitiveSize;
	const Unique<VkShaderModule>					vertModule						(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	Move<VkShaderModule>							tescModule;
	Move<VkShaderModule>							teseModule;
	Move<VkShaderModule>							geomModule;
	Move<VkShaderModule>							fragModule;
	VkVertexInputBindingDescription					bindingDescription;
	VkVertexInputAttributeDescription				attributeDescription;

	if (m_parameters.shaderStage == SHADER_STAGE_TESSELLATION_EVALUATION)
	{
		tescModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("tesc"), 0u);
		teseModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("tese"), 0u);
	}

	if (m_parameters.shaderStage == SHADER_STAGE_GEOMETRY)
		geomModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("geom"), 0u);

	if (!m_parameters.rastDiscard())
		fragModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u);

	bindingDescription.binding		= 0;
	bindingDescription.stride		= sizeof(tcu::Vec2);
	bindingDescription.inputRate	= VK_VERTEX_INPUT_RATE_VERTEX;

	attributeDescription.binding	= 0;
	attributeDescription.location	= 0;
	attributeDescription.format		= VK_FORMAT_R32G32_SFLOAT;
	attributeDescription.offset		= 0;

	const VkPipelineVertexInputStateCreateInfo		vertexInputStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType
		DE_NULL,													// const void*								pNext
		0u,															// VkPipelineVertexInputStateCreateFlags	flags
		1u,															// deUint32									vertexBindingDescriptionCount
		&bindingDescription,										// const VkVertexInputBindingDescription*	pVertexBindingDescriptions
		1u,															// deUint32									vertexAttributeDescriptionCount
		&attributeDescription,										// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions
	};

	const VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// VkStructureType							sType
		DE_NULL,													// const void*								pNext
		0,															// VkPipelineRasterizationStateCreateFlags	flags
		VK_FALSE,													// VkBool32									depthClampEnable
		(m_parameters.rastDiscard() ? VK_TRUE : VK_FALSE),			// VkBool32									rasterizerDiscardEnable
		VK_POLYGON_MODE_FILL,										// VkPolygonMode							polygonMode
		VK_CULL_MODE_NONE,											// VkCullModeFlags							cullMode
		VK_FRONT_FACE_COUNTER_CLOCKWISE,							// VkFrontFace								frontFace
		VK_FALSE,													// VkBool32									depthBiasEnable
		0.0f,														// float									depthBiasConstantFactor
		0.0f,														// float									depthBiasClamp
		0.0f,														// float									depthBiasSlopeFactor
		1.0f														// float									lineWidth
	};

	const VkStencilOpState							stencilOpState					=
	{
		VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp
		VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp
		VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp
		VK_COMPARE_OP_ALWAYS,	// VkCompareOp	compareOp
		0xFFu,					// deUint32		compareMask
		0xFFu,					// deUint32		writeMask
		0,						// deUint32		reference
	};

	const VkPipelineDepthStencilStateCreateInfo		depthStencilStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	//	VkStructureType							sType
		DE_NULL,													//	const void*								pNext
		0,															//	VkPipelineDepthStencilStateCreateFlags	flags
		VK_TRUE,													//	VkBool32								depthTestEnable
		VK_TRUE,													//	VkBool32								depthWriteEnable
		VK_COMPARE_OP_LESS,											//	VkCompareOp								depthCompareOp
		VK_FALSE,													//	VkBool32								depthBoundsTestEnable
		VK_FALSE,													//	VkBool32								stencilTestEnable
		stencilOpState,												//	VkStencilOpState						front
		stencilOpState,												//	VkStencilOpState						back
		0.0f,														//	float									minDepthBounds
		1.0f,														//	float									maxDepthBounds
	};

	const VkBool32									colorWriteEnables				= VK_FALSE;

	const VkPipelineColorWriteCreateInfoEXT			colorWriteCreateInfo			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_WRITE_CREATE_INFO_EXT,	// VkStructureType	sType;
		DE_NULL,												// const void*		pNext;
		1,														// deUint32			attachmentCount;
		&colorWriteEnables										// const VkBool32*	pColorWriteEnables;
	};

	const VkPipelineColorBlendAttachmentState		colorBlendAttachmentState		=
	{
		VK_FALSE,					// VkBool32					blendEnable
		VK_BLEND_FACTOR_ZERO,		// VkBlendFactor			srcColorBlendFactor
		VK_BLEND_FACTOR_ZERO,		// VkBlendFactor			dstColorBlendFactor
		VK_BLEND_OP_ADD,			// VkBlendOp				colorBlendOp
		VK_BLEND_FACTOR_ZERO,		// VkBlendFactor			srcAlphaBlendFactor
		VK_BLEND_FACTOR_ZERO,		// VkBlendFactor			dstAlphaBlendFactor
		VK_BLEND_OP_ADD,			// VkBlendOp				alphaBlendOp
		VK_COLOR_COMPONENT_R_BIT	// VkColorComponentFlags	colorWriteMask
		| VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT
	};

	const VkPipelineColorBlendStateCreateInfo		colorBlendStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType
		&colorWriteCreateInfo,										// const void*									pNext
		0,															// VkPipelineColorBlendStateCreateFlags			flags
		VK_FALSE,													// VkBool32										logicOpEnable
		VK_LOGIC_OP_NO_OP,											// VkLogicOp									logicOp
		1,															// deUint32										attachmentCount
		&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*	pAttachments
		{ 0.0f, 0.0f, 0.0f, 0.0f }									// float										blendConstants[4]
	};

	const VkDynamicState							dynamicStates					= VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT;

	const VkPipelineDynamicStateCreateInfo			pipelineDynamicStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	// VkStructureType						sType
		DE_NULL,												// const void*							pNext
		0u,														// VkPipelineDynamicStateCreateFlags	flags
		1u,														// deUint32								dynamicStateCount
		&dynamicStates											// const VkDynamicState*				pDynamicStates
	};

	const VkPipelineMultisampleStateCreateInfo		multisampleStateCreateInfo		=
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

	return vk::makeGraphicsPipeline(vk,
									device,
									*pipelineLayout,
									*vertModule,
									*tescModule,
									*teseModule,
									*geomModule,
									*fragModule,
									renderPass,
									viewports,
									scissors,
									m_parameters.primitiveTopology,
									subpass,
									patchControlPoints,
									&vertexInputStateCreateInfo,
									&rasterizationStateCreateInfo,
									&multisampleStateCreateInfo,
									m_parameters.depthStencilAttachment ? &depthStencilStateCreateInfo : DE_NULL,
									m_parameters.staticColorWriteDisable() ? &colorBlendStateCreateInfo : DE_NULL,
									m_parameters.dynamicColorWriteDisable() ? &pipelineDynamicStateCreateInfo : DE_NULL);
}

void PrimitivesGeneratedQueryTestInstance::fillVertexBuffer(tcu::Vec2* vertices, const deUint64 primitivesGenerated)
{
	const float step = 1.0f / static_cast<float>(primitivesGenerated);

	switch (m_parameters.primitiveTopology)
	{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
		{
			for (deUint32 prim = 0; prim < primitivesGenerated; ++prim)
			{
				vertices[prim] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, 0.0f);
			}
			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		{
			for (deUint32 prim = 0; prim < primitivesGenerated; ++prim)
			{
				vertices[2* prim + 0] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step,  1.0f);
				vertices[2* prim + 1] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, -1.0f);
			}
			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		{
			vertices[0] = tcu::Vec2(-1.0f,-1.0f);
			vertices[1] = tcu::Vec2(-1.0f, 1.0f);

			for (deUint32 prim = 1; prim < primitivesGenerated; ++prim)
			{
				if (prim % 2 == 0)
				{
					vertices[1 + prim] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step,  1.0f);
				}
				else
				{
					vertices[1 + prim] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, -1.0f);
				}
			}
			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		{
			vertices[0] = tcu::Vec2(-1.0f,				 1.0f);
			vertices[1] = tcu::Vec2(-1.0f,				-1.0f);
			vertices[2] = tcu::Vec2(-1.0f + 2.0f * step, 1.0f);

			for (deUint32 prim = 1; prim < primitivesGenerated; ++prim)
			{
				if (prim % 2 == 0)
				{
					vertices[2 + prim] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, 1.0f);
				}
				else
				{
					vertices[2 + prim] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, -1.0f);
				}
			}
			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
		{
			vertices[0] = tcu::Vec2(0.0f, -1.0f);

			for (deUint32 prim = 0; prim < primitivesGenerated+1; ++prim)
			{
				vertices[1 + prim] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, -1.0f + 2.0f * (float)prim * step);
			}
			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		{
			for (deUint32 prim = 0; prim < primitivesGenerated; ++prim)
			{
				vertices[4 * prim + 0] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step,  1.0f);
				vertices[4 * prim + 1] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step,  0.5f);
				vertices[4 * prim + 2] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, -0.5f);
				vertices[4 * prim + 3] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, -1.0f);
			}
			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
		{
			vertices[0] = tcu::Vec2(-1.0f,  0.0f);
			vertices[1] = tcu::Vec2(-1.0f, -1.0f);
			vertices[2] = tcu::Vec2(-1.0f,  1.0f);

			for (deUint32 prim = 1; prim < primitivesGenerated; ++prim)
			{
				if (prim % 2 == 0)
				{
					vertices[2 + prim] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step,  1.0f);
				}
				else
				{
					vertices[2 + prim] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, -1.0f);
				}
			}

			vertices[2 + primitivesGenerated] = tcu::Vec2(1.0f, 0.0f);

			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
		{
			for (deUint32 prim = 0; prim < primitivesGenerated; ++prim)
			{
				if (prim % 2 == 0)
				{
					vertices[3 * prim + 0] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step,  1.0f);
					vertices[3 * prim + 1] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step, -1.0f);
					vertices[3 * prim + 2] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 1) * step, -1.0f);
				}
				else
				{
					vertices[3 * prim + 0] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 1) * step, -1.0f);
					vertices[3 * prim + 1] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 1) * step,  1.0f);
					vertices[3 * prim + 2] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step,  1.0f);
				}
			}
			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		{
			for (deUint32 prim = 0; prim < primitivesGenerated; ++prim)
			{
				if (prim % 2 == 0)
				{
					vertices[6 * prim + 0] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step,  1.0f);
					vertices[6 * prim + 1] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step,  1.0f);
					vertices[6 * prim + 2] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step, -1.0f);
					vertices[6 * prim + 3] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step, -1.0f);
					vertices[6 * prim + 4] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 1) * step, -1.0f);
					vertices[6 * prim + 5] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 1) * step, -1.0f);
				}
				else
				{
					vertices[6 * prim + 0] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 1) * step, -1.0f);
					vertices[6 * prim + 1] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 1) * step, -1.0f);
					vertices[6 * prim + 2] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 1) * step,  1.0f);
					vertices[6 * prim + 3] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 1) * step,  1.0f);
					vertices[6 * prim + 4] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step,  1.0f);
					vertices[6 * prim + 5] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step,  1.0f);
				}
			}
			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
		{
			vertices[0] = tcu::Vec2(-1.0f,  1.0f);
			vertices[1] = tcu::Vec2(-1.0f,  1.0f);
			vertices[2] = tcu::Vec2(-1.0f, -1.0f);
			vertices[3] = tcu::Vec2(-1.0f, -1.0f);
			vertices[4] = tcu::Vec2(-1.0f + 2.0f * step, 1.0f);
			vertices[5] = tcu::Vec2(-1.0f + 2.0f * step, 1.0f);

			for (deUint32 prim = 1; prim < primitivesGenerated; ++prim)
			{
				if (prim % 2 == 0)
				{
					vertices[5 + prim + 0] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, 1.0f);
					vertices[5 + prim + 1] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, 1.0f);
				}
				else
				{
					vertices[5 + prim + 0] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, -1.0f);
					vertices[5 + prim + 1] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, -1.0f);
				}
			}
			break;
		}
		default:
			TCU_THROW(InternalError, "Unrecognized primitive topology");
	}
}

class PrimitivesGeneratedQueryTestCase : public vkt::TestCase
{
public:
							PrimitivesGeneratedQueryTestCase	(tcu::TestContext &context, const char *name, const char *description, const TestParameters& parameters)
								: TestCase		(context, name, description)
								, m_parameters	(parameters)
							{
							}

private:
	void					checkSupport						(vkt::Context& context) const;
	void					initPrograms						(vk::SourceCollections& programCollection) const;
	vkt::TestInstance*		createInstance						(vkt::Context& context) const { return new PrimitivesGeneratedQueryTestInstance(context, m_parameters); }

	const TestParameters	m_parameters;
};

void PrimitivesGeneratedQueryTestCase::checkSupport (vkt::Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_primitives_generated_query");
	context.requireDeviceFunctionality("VK_EXT_transform_feedback");

	const VkPhysicalDevicePrimitivesGeneratedQueryFeaturesEXT&	pgqFeatures		= context.getPrimitivesGeneratedQueryFeaturesEXT();
	const VkPhysicalDeviceTransformFeedbackFeaturesEXT&			xfbFeatures		= context.getTransformFeedbackFeaturesEXT();
	const VkPhysicalDeviceTransformFeedbackPropertiesEXT&		xfbProperties	= context.getTransformFeedbackPropertiesEXT();

	if (pgqFeatures.primitivesGeneratedQuery != VK_TRUE)
		TCU_THROW(NotSupportedError, "VK_QUERY_TYPE_PRIMITIVES_GENERATED_EXT not supported");

	if (m_parameters.rastDiscard() && (pgqFeatures.primitivesGeneratedQueryWithRasterizerDiscard != VK_TRUE))
		TCU_THROW(NotSupportedError, "primitivesGeneratedQueryWithRasterizerDiscard not supported");

	if (m_parameters.queryResetType == QUERY_RESET_TYPE_HOST)
		context.requireDeviceFunctionality("VK_EXT_host_query_reset");

	if (m_parameters.shaderStage == SHADER_STAGE_GEOMETRY || topologyData.at(m_parameters.primitiveTopology).hasAdjacency)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

	if (m_parameters.shaderStage == SHADER_STAGE_TESSELLATION_EVALUATION)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

	if (m_parameters.nonZeroStreams())
	{
		const deUint32 requiredStreams	= de::max(m_parameters.pgqStreamIndex(), m_parameters.xfbStreamIndex());

		if (m_parameters.pgqStreamIndex() > 0 && pgqFeatures.primitivesGeneratedQueryWithNonZeroStreams != VK_TRUE)
			TCU_THROW(NotSupportedError, "primitivesGeneratedQueryWithNonZeroStreams not supported");

		if (xfbProperties.maxTransformFeedbackStreams <= requiredStreams)
			TCU_THROW(NotSupportedError, "Required amount of XFB streams not supported");
	}

	if (m_parameters.transformFeedback)
	{
		if (xfbFeatures.transformFeedback != VK_TRUE)
			TCU_THROW(NotSupportedError, "transformFeedback not supported");

		if (xfbProperties.transformFeedbackQueries != VK_TRUE)
			TCU_THROW(NotSupportedError, "transformFeedbackQueries not supported");
	}

	if (m_parameters.colorWriteDisable())
	{
		context.requireDeviceFunctionality("VK_EXT_color_write_enable");

		if (context.getColorWriteEnableFeaturesEXT().colorWriteEnable != VK_TRUE)
			TCU_THROW(NotSupportedError, "colorWriteEnable not supported");
	}
}

void PrimitivesGeneratedQueryTestCase::initPrograms (vk::SourceCollections& programCollection) const
{
	// Vertex shader.
	{
		const bool			vertXfb	= (m_parameters.transformFeedback && m_parameters.shaderStage == SHADER_STAGE_VERTEX);
		std::ostringstream	src;

		src	<<	"#version 450\n";
		src << "layout(location=0) in vec2 inPosition;\n";

		if (vertXfb)
			src	<<	"layout(xfb_buffer = 0, xfb_offset = 0, xfb_stride = 16, location = 0) out vec4 out0;\n";

		src	<<	"void main (void)\n"
				"{\n";

		if (m_parameters.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST && m_parameters.shaderStage == SHADER_STAGE_VERTEX)
			src	<<	"    gl_PointSize = 1.0;\n";

		src << "	 gl_Position = vec4(inPosition, 0, 1);\n";

		if (vertXfb)
			src	<<	"    out0 = vec4(42);\n";

		src	<<	"}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Tessellation shaders.
	if (m_parameters.shaderStage == SHADER_STAGE_TESSELLATION_EVALUATION)
	{
		std::stringstream	tescSrc;
		std::stringstream	teseSrc;

		tescSrc	<<	"#version 450\n"
					"#extension GL_EXT_tessellation_shader : require\n"
					"layout(vertices = "<< topologyData.at(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST).primitiveSize << ") out;\n"
					"void main (void)\n"
					"{\n"
					"    gl_TessLevelInner[0] = 1.0;\n"
					"    gl_TessLevelInner[1] = 1.0;\n"
					"    gl_TessLevelOuter[0] = 1.0;\n"
					"    gl_TessLevelOuter[1] = 1.0;\n"
					"    gl_TessLevelOuter[2] = 1.0;\n"
					"    gl_TessLevelOuter[3] = 1.0;\n"
					"    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
					"}\n";

		teseSrc	<<	"#version 450\n"
					"#extension GL_EXT_tessellation_shader : require\n"
					"layout(triangles) in;\n";

		if (m_parameters.transformFeedback)
			teseSrc	<<	"layout(xfb_buffer = 0, xfb_offset = 0, location = 0) out vec4 out0;\n";

		teseSrc	<<	"void main (void)\n"
					"{\n";

		if (m_parameters.transformFeedback)
			teseSrc	<<	"    out0 = vec4(42);\n";

		teseSrc	<<	"    vec4 p0 = gl_TessCoord.x * gl_in[0].gl_Position;\n"
					"    vec4 p1 = gl_TessCoord.y * gl_in[1].gl_Position;\n"
					"    vec4 p2 = gl_TessCoord.z * gl_in[2].gl_Position;\n"
					"    gl_Position = p0 + p1 + p2;\n"
					"}\n";

		programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tescSrc.str());
		programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(teseSrc.str());
	}

	// Geometry shader.
	if (m_parameters.shaderStage == SHADER_STAGE_GEOMETRY)
	{
		const bool			outputPoints	= m_parameters.nonZeroStreams() || m_parameters.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		const char* const	inputTopology	= topologyData.at(m_parameters.primitiveTopology).inputString;
		const char* const	outputTopology	= outputPoints ? "points" : topologyData.at(m_parameters.primitiveTopology).outputString;
		const VkDeviceSize	outputPrimSize	= outputPoints ? 1 : topologyData.at(m_parameters.primitiveTopology).primitiveSize;
		const VkDeviceSize	maxVertices		= m_parameters.multipleStreams() ? outputPrimSize * 2 : outputPrimSize;
		const std::string	pgqEmitCommand	= m_parameters.nonZeroStreams() ? std::string("EmitStreamVertex(") + de::toString(m_parameters.pgqStreamIndex()) + ")" : "EmitVertex()";
		const std::string	xfbEmitCommand	= m_parameters.nonZeroStreams() ? std::string("EmitStreamVertex(") + de::toString(m_parameters.xfbStreamIndex()) + ")" : "EmitVertex()";
		const std::string	pgqEndCommand	= m_parameters.nonZeroStreams() ? std::string("EndStreamPrimitive(") + de::toString(m_parameters.pgqStreamIndex()) + ")" : "EndPrimitive()";
		const std::string	xfbEndCommand	= m_parameters.nonZeroStreams() ? std::string("EndStreamPrimitive(") + de::toString(m_parameters.xfbStreamIndex()) + ")" : "EndPrimitive()";
		std::ostringstream	src;

		src	<<	"#version 450\n"
				"layout(" << inputTopology << ") in;\n"
				"layout(" << outputTopology << ", max_vertices = " << maxVertices << ") out;\n";

		if (m_parameters.transformFeedback)
			src	<<	"layout(xfb_buffer = 0, xfb_offset = 0, xfb_stride = 16, location = 0, stream = " << m_parameters.xfbStreamIndex() << ") out vec4 xfb;\n";

		src	<<	"void main (void)\n"
				"{\n";

		if (outputPoints)
			src	<<	"    gl_PointSize = 1.0;\n";

		if (m_parameters.transformFeedback)
			src	<<	"    xfb = vec4(42);\n";

		for (VkDeviceSize i = 0; i < outputPrimSize; i++)
			src	<<	"    " << pgqEmitCommand << ";\n";

		src	<<	"    " << pgqEndCommand << ";\n";

		if (m_parameters.transformFeedback && m_parameters.multipleStreams())
		{
			for (VkDeviceSize i = 0; i < outputPrimSize; i++)
				src	<<	"    " << xfbEmitCommand << ";\n";

			src	<<	"    " << xfbEndCommand << ";\n";
		}

		src	<<	"}\n";

		programCollection.glslSources.add("geom") << glu::GeometrySource(src.str());
	}

	// Fragment shader.
	if (!m_parameters.rastDiscard())
	{
		std::ostringstream src;

		if (m_parameters.rastCase == RAST_CASE_EMPTY_FRAG)
		{
			src	<<	"#version 450\n"
					"void main (void) {}\n";
		}
		else
		{
			src	<<	"#version 450\n"
					"layout(location = 0) out vec4 out0;\n"
					"void main (void)\n"
					"{\n"
					"    out0 = vec4(0.0, 1.0, 0.0, 1.0);\n"
					"}\n";
		}

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}
class ConcurrentPrimitivesGeneratedQueryTestInstance : public vkt::TestInstance
{
public:
							ConcurrentPrimitivesGeneratedQueryTestInstance	(vkt::Context &context, const ConcurrentTestParameters& parameters)
																			: vkt::TestInstance	(context)
																			, m_parameters		(parameters)
																			{}

private:
	tcu::TestStatus			iterate											(void);
	void					draw											(const DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, deUint32 vertexCount, vk::VkBuffer indirectBuffer);
	Move<VkPipeline>		makeGraphicsPipeline							(const DeviceInterface&	vk,
																			 const VkDevice device,
																			 const VkRenderPass renderPass);
	void					fillVertexBuffer								(tcu::Vec2* vertices,
																			 const deUint64 primitivesGenerated);
	void					copyColorImageToBuffer							(const DeviceInterface& vk, VkCommandBuffer cmdBuffer, VkImage image, VkBuffer buffer);

	const ConcurrentTestParameters	m_parameters;
	const deUint32					m_imageWidth	= 16;
	const deUint32					m_imageHeight	= 16;
	const deUint32					instanceCount	= 1u;
	const deUint32					firstVertex		= 0u;
	const deUint32					firstInstance	= 0u;
};

// Create a render pass with an initial and final layout of VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
// The load operation will clear the attachment.
Move<VkRenderPass> makeConstantLayoutRenderPass (const DeviceInterface&				vk,
												 const VkDevice						device,
												 const VkFormat						colorFormat)
{
	const auto constantLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	const VkAttachmentDescription colorAttachmentDescription =
	{
		0u,									// VkAttachmentDescriptionFlags    flags
		colorFormat,						// VkFormat                        format
		VK_SAMPLE_COUNT_1_BIT,				// VkSampleCountFlagBits           samples
		VK_ATTACHMENT_LOAD_OP_CLEAR,		// VkAttachmentLoadOp              loadOp
		VK_ATTACHMENT_STORE_OP_STORE,		// VkAttachmentStoreOp             storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,	// VkAttachmentLoadOp              stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,	// VkAttachmentStoreOp             stencilStoreOp
		constantLayout,						// VkImageLayout                   initialLayout
		constantLayout						// VkImageLayout                   finalLayout
	};

	const VkAttachmentReference colorAttachmentRef = makeAttachmentReference(0u, constantLayout);

	const VkSubpassDescription subpassDescription =
	{
		0u,									// VkSubpassDescriptionFlags       flags
		VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint             pipelineBindPoint
		0u,									// deUint32                        inputAttachmentCount
		nullptr,							// const VkAttachmentReference*    pInputAttachments
		1u,									// deUint32                        colorAttachmentCount
		&colorAttachmentRef,				// const VkAttachmentReference*    pColorAttachments
		nullptr,							// const VkAttachmentReference*    pResolveAttachments
		nullptr,							// const VkAttachmentReference*    pDepthStencilAttachment
		0u,									// deUint32                        preserveAttachmentCount
		nullptr								// const deUint32*                 pPreserveAttachments
	};

	const VkRenderPassCreateInfo renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType                   sType
		nullptr,									// const void*                       pNext
		0u,											// VkRenderPassCreateFlags           flags
		1u,											// deUint32                          attachmentCount
		&colorAttachmentDescription,				// const VkAttachmentDescription*    pAttachments
		1u,											// deUint32                          subpassCount
		&subpassDescription,						// const VkSubpassDescription*       pSubpasses
		0u,											// deUint32                          dependencyCount
		nullptr										// const VkSubpassDependency*        pDependencies
	};

	return createRenderPass(vk, device, &renderPassInfo);
}

// Transitions the selected subresource range to color attachment optimal.
void prepareColorAttachment (const DeviceInterface& vkd, const VkCommandBuffer cmdBuffer, const VkImage image, const VkImageSubresourceRange imageSRR)
{
	const auto barrier = makeImageMemoryBarrier(0u, 0u, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, image, imageSRR);
	cmdPipelineImageMemoryBarrier(vkd, cmdBuffer, 0u, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, &barrier);
}

tcu::TestStatus ConcurrentPrimitivesGeneratedQueryTestInstance::iterate (void)
{
	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					device				= m_context.getDevice();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkQueue					queue				= m_context.getUniversalQueue();
	Allocator&						allocator			= m_context.getDefaultAllocator();
	tcu::TestLog&					log					= m_context.getTestContext().getLog();

	const VkDeviceSize				primitivesGenerated = 32;
	const deUint32					baseMipLevel		= 0;
	const deUint32					levelCount			= 1;
	const deUint32					baseArrayLayer		= 0;
	const deUint32					layerCount			= 1;

	const VkFormat					colorFormat			= VK_FORMAT_R8G8B8A8_UNORM;

	const VkImageCreateInfo colorImageCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType
		DE_NULL,										// const void*				pNext
		0u,												// VkImageCreateFlags		flags
		VK_IMAGE_TYPE_2D,								// VkImageType				imageType
		colorFormat,									// VkFormat					format
		makeExtent3D(m_imageWidth, m_imageHeight, 1),	// VkExtent3D				extent
		1u,												// deUint32					mipLevels
		1u,												// deUint32					arrayLayers
		VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits	samples
		VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
		| VK_IMAGE_USAGE_TRANSFER_SRC_BIT,				// VkImageUsageFlags		usage
		VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode
		0u,												// deUint32					queueFamilyIndexCount
		DE_NULL,										// const deUint32*			pQueueFamilyIndices
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			initialLayout
	};

	const VkImageSubresourceRange	colorSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, baseMipLevel, levelCount, baseArrayLayer, layerCount);

	Move<VkImage>					colorImage				= makeImage(vk, device, colorImageCreateInfo);
	de::MovePtr<Allocation>			colorImageAllocation	= bindImage(vk, device, allocator, *colorImage, MemoryRequirement::Any);
	Move<VkImageView>				colorImageView			= makeImageView(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange);

	// By using a constant-layout render pass, we can begin and end the render pass multiple times without layout transition hazards.
	const Unique<VkRenderPass>		renderPass			(makeConstantLayoutRenderPass(vk, device, colorFormat));
	const vk::VkClearValue			clearColor			= { { { 0.0f, 0.0f, 0.0f, 0.0f } } };
	const Unique<VkFramebuffer>		framebuffer			(makeFramebuffer(vk, device, *renderPass, *colorImageView, m_imageWidth, m_imageHeight));
	const Unique<VkPipeline>		pipeline			(ConcurrentPrimitivesGeneratedQueryTestInstance::makeGraphicsPipeline(vk, device, *renderPass));

	const VkBufferUsageFlags		usage				= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	const std::vector<deUint32>		queueFamilyIndices	(1, queueFamilyIndex);
	const VkDeviceSize				vtxBufferSize		= topologyData.at(m_parameters.primitiveTopology).getNumVertices(primitivesGenerated) * sizeof(tcu::Vec2);
	const VkBufferCreateInfo		createInfo			= makeBufferCreateInfo(vtxBufferSize, usage, queueFamilyIndices);

	Move<VkBuffer>					vtxBuffer			= createBuffer(vk, device, &createInfo);
	de::MovePtr<Allocation>			vtxBufferAlloc		= allocator.allocate(getBufferMemoryRequirements(vk, device, *vtxBuffer), MemoryRequirement::HostVisible);

	const Unique<VkCommandPool>		cmdPool				(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	primaryCmdBuffer	(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const Unique<VkCommandBuffer>	secondaryCmdBuffer	(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY));

	const bool						pgq64				= (m_parameters.queryResultType == QUERY_RESULT_TYPE_64_BIT ||
														   m_parameters.queryResultType == QUERY_RESULT_TYPE_PGQ_64_XFB_32);
	const bool						xfb64				= (m_parameters.queryResultType == QUERY_RESULT_TYPE_64_BIT ||
														   m_parameters.queryResultType == QUERY_RESULT_TYPE_PGQ_32_XFB_64);
	const size_t					pgqResultSize		= pgq64 ? sizeof(deUint64) : sizeof(deUint32);
	const size_t					xfbResultSize		= xfb64 ? sizeof(deUint64) * 2 : sizeof(deUint32) * 2;
	const size_t					psResultsSize		= sizeof(deUint64);
	const VkQueryResultFlags		pgqResultWidthBit	= pgq64 ? VK_QUERY_RESULT_64_BIT : 0;
	const VkQueryResultFlags		xfbResultWidthBit	= xfb64 ? VK_QUERY_RESULT_64_BIT : 0;
	const VkQueryResultFlags		psResultWidthBit	= 0;
	const VkQueryResultFlags		pgqResultFlags		= VK_QUERY_RESULT_WAIT_BIT | pgqResultWidthBit;
	const VkQueryResultFlags		xfbResultFlags		= VK_QUERY_RESULT_WAIT_BIT | xfbResultWidthBit;
	const VkQueryResultFlags		psResultFlags		= VK_QUERY_RESULT_WAIT_BIT | psResultWidthBit;

	const deUint32					pgqQueryCount			= 1;
	const deUint32					xfbQueryCount			= (m_parameters.concurrentTestType == CONCURRENT_TEST_TYPE_TWO_XFB_INSIDE_PGQ) ? 2 : 0;
	const deUint32					psQueryCount			= (m_parameters.concurrentTestType >= CONCURRENT_TEST_TYPE_PIPELINE_STATISTICS_1) ? 1 : 0;

	std::vector<deUint8>				pgqResults			(pgqResultSize, 255u);
	std::vector<std::vector<deUint8>>	xfbResults;
	std::vector<std::vector<deUint8>>	psqResults;
	if (xfbQueryCount > 0)
	{
		xfbResults.resize(xfbQueryCount);
		for (deUint32 i = 0; i < xfbQueryCount; ++i)
			xfbResults[i] = std::vector<deUint8>(xfbResultSize, 255u);
	}
	if (psQueryCount > 0)
	{
		psqResults.resize(psQueryCount);
		for (deUint32 i = 0; i < psQueryCount; ++i)
			psqResults[i] = std::vector<deUint8>(psResultsSize, 255u);
	}

	const VkQueryPoolCreateInfo		pgqCreateInfo		=
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,	// VkStructureType					sType
		DE_NULL,									// const void*						pNext
		0u,											// VkQueryPoolCreateFlags			flags
		VK_QUERY_TYPE_PRIMITIVES_GENERATED_EXT,		// VkQueryType						queryType
		pgqQueryCount,								// deUint32							queryCount
		0u,											// VkQueryPipelineStatisticFlags	pipelineStatistics
	};

	const VkQueryPoolCreateInfo xfbCreateInfo =
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,		// VkStructureType					sType
		DE_NULL,										// const void*						pNext
		0u,												// VkQueryPoolCreateFlags			flags
		VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT,	// VkQueryType						queryType
		xfbQueryCount,									// deUint32							queryCount
		0u,												// VkQueryPipelineStatisticFlags	pipelineStatistics
	};

	const VkQueryPoolCreateInfo psCreateInfo =
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,		// VkStructureType					sType
		DE_NULL,										// const void*						pNext
		0u,												// VkQueryPoolCreateFlags			flags
		VK_QUERY_TYPE_PIPELINE_STATISTICS,				// VkQueryType						queryType
		psQueryCount,									// deUint32							queryCount
		VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT
														// VkQueryPipelineStatisticFlags	pipelineStatistics
	};

	const Move<VkQueryPool>			pgqPool				(createQueryPool(vk, device, &pgqCreateInfo));
	Move<VkQueryPool>				xfbPool;
	Move<VkQueryPool>				psqPool;
	if (xfbQueryCount > 0)
		xfbPool = createQueryPool(vk, device, &xfbCreateInfo);
	if (psQueryCount > 0)
		psqPool = createQueryPool(vk, device, &psCreateInfo);

	const VkDeviceSize				primitivesWritten	= primitivesGenerated - 3;
	const VkDeviceSize				verticesWritten		= topologyData.at(m_parameters.primitiveTopology).getNumVertices(primitivesWritten);
	const VkDeviceSize				primitiveSize		= m_parameters.nonZeroStreams() ? 1 : topologyData.at(m_parameters.primitiveTopology).primitiveSize;
	const VkDeviceSize				bytesPerVertex		= 4 * sizeof(float);
	const VkDeviceSize				xfbBufferSize		= primitivesWritten * primitiveSize * bytesPerVertex;
	Move<VkBuffer>					xfbBuffer;
	de::MovePtr<Allocation>			xfbBufferAlloc;

	{
		const VkBufferUsageFlags	xfbUsage			= VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT;
		const VkBufferCreateInfo	xfbBufferCreateInfo	= makeBufferCreateInfo(xfbBufferSize, xfbUsage);

		xfbBuffer		= createBuffer(vk, device, &xfbBufferCreateInfo);
		xfbBufferAlloc	= allocator.allocate(getBufferMemoryRequirements(vk, device, *xfbBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(device, *xfbBuffer, xfbBufferAlloc->getMemory(), xfbBufferAlloc->getOffset()));
	}

	fillVertexBuffer(static_cast<tcu::Vec2*>(vtxBufferAlloc.get()->getHostPtr()), primitivesGenerated);

	VK_CHECK(vk.bindBufferMemory(device, *vtxBuffer, vtxBufferAlloc->getMemory(), vtxBufferAlloc->getOffset()));

	const vk::VkDeviceSize				colorOutputBufferSize	= m_imageWidth * m_imageHeight* tcu::getPixelSize(vk::mapVkFormat(colorFormat));
	de::MovePtr<vk::BufferWithMemory>	colorOutputBuffer		= de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(
		vk, device, allocator, makeBufferCreateInfo(colorOutputBufferSize, vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT), vk::MemoryRequirement::HostVisible));

	de::MovePtr<vk::BufferWithMemory>	indirectBuffer			= de::MovePtr<BufferWithMemory>(new BufferWithMemory(vk, device, allocator, vk::makeBufferCreateInfo(sizeof(vk::VkDrawIndirectCommand), vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT), vk::MemoryRequirement::HostVisible));;

	const deUint32						vertexCount				= static_cast<deUint32>(topologyData.at(m_parameters.primitiveTopology).getNumVertices(primitivesGenerated));

	if (m_parameters.indirect)
	{
		const vk::Allocation&			indirectAlloc			= (*indirectBuffer).getAllocation();
		vk::VkDrawIndirectCommand*		indirectDataPtr			= reinterpret_cast<vk::VkDrawIndirectCommand*>(indirectAlloc.getHostPtr());

		indirectDataPtr->vertexCount = vertexCount;
		indirectDataPtr->instanceCount = instanceCount;
		indirectDataPtr->firstVertex = firstVertex;
		indirectDataPtr->firstInstance = firstInstance;

		flushAlloc(vk, device, indirectAlloc);
	}

	const bool					secondaryCmdBufferUsed	= m_parameters.concurrentTestType == CONCURRENT_TEST_TYPE_PGQ_SECONDARY_CMD_BUFFER;
	const VkQueryControlFlags	queryControlFlags		= 0;
	const VkDeviceSize			vertexBufferOffset		= static_cast<VkDeviceSize>(0);

	VkCommandBuffer cmdBuffer;

	if (secondaryCmdBufferUsed)
	{
		cmdBuffer = *secondaryCmdBuffer;
		beginSecondaryCommandBuffer(vk, cmdBuffer, *renderPass, *framebuffer);
	}
	else
	{
		cmdBuffer = *primaryCmdBuffer;
		beginCommandBuffer(vk, cmdBuffer);
		prepareColorAttachment(vk, cmdBuffer, *colorImage, colorSubresourceRange);
	}

	if (m_parameters.concurrentTestType == CONCURRENT_TEST_TYPE_PIPELINE_STATISTICS_3)
	{
		beginSecondaryCommandBuffer(vk, *secondaryCmdBuffer, *renderPass, *framebuffer);
		vk.cmdBeginQueryIndexedEXT(*secondaryCmdBuffer, *pgqPool, 0, queryControlFlags, m_parameters.pgqStreamIndex());
		vk.cmdBindPipeline(*secondaryCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		vk.cmdBindVertexBuffers(*secondaryCmdBuffer, 0, 1, &vtxBuffer.get(), &vertexBufferOffset);
		draw(vk, *secondaryCmdBuffer, vertexCount, indirectBuffer->get());
		vk.cmdEndQueryIndexedEXT(*secondaryCmdBuffer, *pgqPool, 0, m_parameters.pgqStreamIndex());
		vk.endCommandBuffer(*secondaryCmdBuffer);
	}

	{
		// After query pool creation, each query must be reset before it is used.
		if (!secondaryCmdBufferUsed)
		{
			vk.cmdResetQueryPool(cmdBuffer, *pgqPool, 0, pgqQueryCount);
			if (xfbQueryCount > 0)
				vk.cmdResetQueryPool(cmdBuffer, *xfbPool, 0, xfbQueryCount);
			if (psQueryCount > 0)
				vk.cmdResetQueryPool(cmdBuffer, *psqPool, 0, psQueryCount);
		}

		vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		vk.cmdBindVertexBuffers(cmdBuffer, 0, 1, &vtxBuffer.get(), &vertexBufferOffset);

		{
			const deUint32				firstCounterBuffer		= 0;
			const deUint32				counterBufferCount		= 0;
			const VkBuffer*				counterBuffers			= DE_NULL;
			const VkDeviceSize*			counterBufferOffsets	= DE_NULL;

			if (m_parameters.concurrentTestType == CONCURRENT_TEST_TYPE_TWO_XFB_INSIDE_PGQ)
			{
				beginRenderPass(vk, cmdBuffer, *renderPass, *framebuffer, makeRect2D(makeExtent2D(m_imageWidth, m_imageHeight)), clearColor);

				// Begin PGQ
				vk.cmdBeginQueryIndexedEXT(cmdBuffer, *pgqPool, 0, queryControlFlags, m_parameters.pgqStreamIndex());

				draw(vk, cmdBuffer, vertexCount, indirectBuffer->get());

				// Begin XFBQ 1
				{
					const deUint32		firstBinding = 0;
					const deUint32		bindingCount = 1;
					const VkDeviceSize	offset = 0;

					vk.cmdBindTransformFeedbackBuffersEXT(cmdBuffer, firstBinding, bindingCount, &*xfbBuffer, &offset, &xfbBufferSize);
					vk.cmdBeginQueryIndexedEXT(cmdBuffer, *xfbPool, 0, queryControlFlags, m_parameters.xfbStreamIndex());
					vk.cmdBeginTransformFeedbackEXT(cmdBuffer, firstCounterBuffer, counterBufferCount, counterBuffers, counterBufferOffsets);
				}

				draw(vk, cmdBuffer, vertexCount, indirectBuffer->get());

				// End XFBQ 1
				{
					vk.cmdEndQueryIndexedEXT(cmdBuffer, *xfbPool, 0, m_parameters.xfbStreamIndex());
					vk.cmdEndTransformFeedbackEXT(cmdBuffer, firstCounterBuffer, counterBufferCount, counterBuffers, counterBufferOffsets);
				}

				draw(vk, cmdBuffer, vertexCount, indirectBuffer->get());

				// Begin XFBQ 1
				{
					const deUint32		firstBinding = 0;
					const deUint32		bindingCount = 1;
					const VkDeviceSize	offset = 0;

					vk.cmdBindTransformFeedbackBuffersEXT(cmdBuffer, firstBinding, bindingCount, &*xfbBuffer, &offset, &xfbBufferSize);
					vk.cmdBeginQueryIndexedEXT(cmdBuffer, *xfbPool, 1, queryControlFlags, m_parameters.xfbStreamIndex());
					vk.cmdBeginTransformFeedbackEXT(cmdBuffer, firstCounterBuffer, counterBufferCount, counterBuffers, counterBufferOffsets);
				}

				draw(vk, cmdBuffer, vertexCount, indirectBuffer->get());

				// End XFBQ 1
				{
					vk.cmdEndQueryIndexedEXT(cmdBuffer, *xfbPool, 1, m_parameters.xfbStreamIndex());
					vk.cmdEndTransformFeedbackEXT(cmdBuffer, firstCounterBuffer, counterBufferCount, counterBuffers, counterBufferOffsets);
				}

				draw(vk, cmdBuffer, vertexCount, indirectBuffer->get());

				// End PGQ
				vk.cmdEndQueryIndexedEXT(cmdBuffer, *pgqPool, 0, m_parameters.pgqStreamIndex());

				endRenderPass(vk, cmdBuffer);
			}
			else if (m_parameters.concurrentTestType == CONCURRENT_TEST_TYPE_PGQ_SECONDARY_CMD_BUFFER)
			{

				vk.cmdBeginQueryIndexedEXT(cmdBuffer, *pgqPool, 0, queryControlFlags, m_parameters.pgqStreamIndex());
				draw(vk, cmdBuffer, vertexCount, indirectBuffer->get());
				vk.cmdEndQueryIndexedEXT(cmdBuffer, *pgqPool, 0, m_parameters.pgqStreamIndex());
			}
			else if (m_parameters.concurrentTestType == CONCURRENT_TEST_TYPE_PIPELINE_STATISTICS_1)
			{
				vk.cmdBeginQueryIndexedEXT(cmdBuffer, *pgqPool, 0, queryControlFlags, m_parameters.pgqStreamIndex());
				vk.cmdBeginQuery(cmdBuffer, *psqPool, 0, queryControlFlags);

				beginRenderPass(vk, cmdBuffer, *renderPass, *framebuffer, makeRect2D(makeExtent2D(m_imageWidth, m_imageHeight)), clearColor);
				draw(vk, cmdBuffer, vertexCount, indirectBuffer->get());
				endRenderPass(vk, cmdBuffer);

				vk.cmdEndQueryIndexedEXT(cmdBuffer, *pgqPool, 0, m_parameters.pgqStreamIndex());

				beginRenderPass(vk, cmdBuffer, *renderPass, *framebuffer, makeRect2D(makeExtent2D(m_imageWidth, m_imageHeight)), clearColor);
				draw(vk, cmdBuffer, vertexCount, indirectBuffer->get());
				endRenderPass(vk, cmdBuffer);

				vk.cmdEndQuery(cmdBuffer, *psqPool, 0);
			}
			else if (m_parameters.concurrentTestType == CONCURRENT_TEST_TYPE_PIPELINE_STATISTICS_2)
			{
				vk.cmdBeginQueryIndexedEXT(cmdBuffer, *pgqPool, 0, queryControlFlags, m_parameters.pgqStreamIndex());

				beginRenderPass(vk, cmdBuffer, *renderPass, *framebuffer, makeRect2D(makeExtent2D(m_imageWidth, m_imageHeight)), clearColor);
				vk.cmdBeginQuery(cmdBuffer, *psqPool, 0, queryControlFlags);
				draw(vk, cmdBuffer, vertexCount, indirectBuffer->get());
				vk.cmdEndQuery(cmdBuffer, *psqPool, 0);
				endRenderPass(vk, cmdBuffer);

				beginRenderPass(vk, cmdBuffer, *renderPass, *framebuffer, makeRect2D(makeExtent2D(m_imageWidth, m_imageHeight)), clearColor);
				draw(vk, cmdBuffer, vertexCount, indirectBuffer->get());
				endRenderPass(vk, cmdBuffer);

				vk.cmdEndQueryIndexedEXT(cmdBuffer, *pgqPool, 0, m_parameters.pgqStreamIndex());
			}
			else if (m_parameters.concurrentTestType == CONCURRENT_TEST_TYPE_PIPELINE_STATISTICS_3)
			{
				vk.cmdBeginQuery(cmdBuffer, *psqPool, 0, queryControlFlags);

				beginRenderPass(vk, cmdBuffer, *renderPass, *framebuffer, makeRect2D(makeExtent2D(m_imageWidth, m_imageHeight)), clearColor, vk::VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
				vk.cmdExecuteCommands(cmdBuffer, 1u, &*secondaryCmdBuffer);
				endRenderPass(vk, cmdBuffer);

				beginRenderPass(vk, cmdBuffer, *renderPass, *framebuffer, makeRect2D(makeExtent2D(m_imageWidth, m_imageHeight)), clearColor);
				draw(vk, cmdBuffer, vertexCount, indirectBuffer->get());
				endRenderPass(vk, cmdBuffer);

				vk.cmdEndQuery(cmdBuffer, *psqPool, 0);
			}
		}
	}

	if (!secondaryCmdBufferUsed)
		copyColorImageToBuffer(vk, cmdBuffer, *colorImage, **colorOutputBuffer);

	vk::endCommandBuffer(vk, cmdBuffer);

	if (secondaryCmdBufferUsed)
	{
		vk::beginCommandBuffer(vk, *primaryCmdBuffer);
		prepareColorAttachment(vk, *primaryCmdBuffer, *colorImage, colorSubresourceRange);
		vk.cmdResetQueryPool(*primaryCmdBuffer, *pgqPool, 0, pgqQueryCount);
		if (xfbQueryCount > 0)
			vk.cmdResetQueryPool(*primaryCmdBuffer, *xfbPool, 0, xfbQueryCount);
		if (psQueryCount > 0)
			vk.cmdResetQueryPool(cmdBuffer, *psqPool, 0, psQueryCount);
		beginRenderPass(vk, *primaryCmdBuffer, *renderPass, *framebuffer, makeRect2D(makeExtent2D(m_imageWidth, m_imageHeight)), clearColor, vk::VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
		vk.cmdExecuteCommands(*primaryCmdBuffer, 1u, &*secondaryCmdBuffer);
		endRenderPass(vk, *primaryCmdBuffer);
		copyColorImageToBuffer(vk, *primaryCmdBuffer, *colorImage, **colorOutputBuffer);
		vk::endCommandBuffer(vk, *primaryCmdBuffer);
	}

	vk::submitCommandsAndWait(vk, device, queue, *primaryCmdBuffer);

	vk.getQueryPoolResults(device, *pgqPool, 0, pgqQueryCount, pgqResults.size(), pgqResults.data(), pgqResults.size(), pgqResultFlags);
	for (deUint32 i = 0; i < xfbQueryCount; ++i)
		vk.getQueryPoolResults(device, *xfbPool, i, 1, xfbResults[i].size(), xfbResults[i].data(), xfbResults[i].size(), xfbResultFlags);
	for (deUint32 i = 0; i < psQueryCount; ++i)
		vk.getQueryPoolResults(device, *psqPool, i, 1, psqResults[i].size(), psqResults[i].data(), psqResults[i].size(), psResultFlags);

	// Validate counters.
	{
		deUint32 pgqDrawCount = 1;
		if (m_parameters.concurrentTestType == CONCURRENT_TEST_TYPE_TWO_XFB_INSIDE_PGQ)
			pgqDrawCount = 5;
		else if (m_parameters.concurrentTestType == CONCURRENT_TEST_TYPE_PIPELINE_STATISTICS_2)
			pgqDrawCount = 2;
		const deUint32 totalPrimitivesGenerated = static_cast<deUint32>(primitivesGenerated * pgqDrawCount);

		union QueryResults
		{
			deUint32	elements32[2];
			deUint64	elements64[2];
		};

		const QueryResults*			pgqCounters		= reinterpret_cast<QueryResults*>(pgqResults.data());
		const deUint64				pgqGenerated	= pgq64 ? pgqCounters->elements64[0] : static_cast<deUint64>(pgqCounters->elements32[0]);
		std::vector< QueryResults*> xfbCounters		(xfbResults.size());
		std::vector<deUint64>		xfbWritten		(xfbResults.size());
		std::vector<deUint64>		xfbGenerated	(xfbResults.size());

		for (deUint32 i = 0; i < xfbQueryCount; ++i)
		{
			xfbCounters[i] = reinterpret_cast<QueryResults*>(xfbResults[i].data());
			xfbWritten[i] = xfb64 ? xfbCounters[i]->elements64[0] : static_cast<deUint64>(xfbCounters[i]->elements32[0]);
			xfbGenerated[i] = xfb64 ? xfbCounters[i]->elements64[1] : static_cast<deUint64>(xfbCounters[i]->elements32[1]);
		}

		log	<<	tcu::TestLog::Message
			<<	"primitivesGenerated: " << totalPrimitivesGenerated << "\n"
			<<	"primitivesWritten: " << primitivesWritten << "\n"
			<<	"verticesWritten: " << verticesWritten << "\n"
			<<	"xfbBufferSize: " << xfbBufferSize << "\n"
			<<	tcu::TestLog::EndMessage;

		log << tcu::TestLog::Message << "PGQ: Generated " << pgqGenerated << tcu::TestLog::EndMessage;
		for (deUint32 i = 0; i < (deUint32)xfbCounters.size(); ++i)
			log << tcu::TestLog::Message << "XFB " << i << ": Written " << xfbWritten[i] << ", generated " << xfbGenerated[i] << tcu::TestLog::EndMessage;

		if (pgqGenerated != totalPrimitivesGenerated)
		{
			const std::string message = std::string("Total pgqGenerated == ") + de::toString(pgqGenerated) + ", expected " + de::toString(totalPrimitivesGenerated);
			return tcu::TestStatus::fail(message);
		}

		for (deUint32 i = 0; i < (deUint32)xfbCounters.size(); ++i)
		{
			if (xfbGenerated[i] != primitivesGenerated)
			{
				const std::string message = std::string("xfbGenerated == ") + de::toString(xfbGenerated[i]) + ", expected " + de::toString(primitivesGenerated);
				return tcu::TestStatus::fail(message);
			}

			if (xfbWritten[i] != primitivesWritten)
			{
				const std::string message = std::string("xfbWritten == ") + de::toString(xfbWritten[i]) + ", expected " + de::toString(primitivesWritten);
				return tcu::TestStatus::fail(message);
			}

			if (xfbWritten[i] != (primitivesGenerated - 3))
			{
				const std::string message = std::string("xfbWritten == ") + de::toString(xfbWritten[i]) + ", expected " + de::toString(primitivesGenerated - 3);
				return tcu::TestStatus::fail(message);
			}
		}

		if (psQueryCount > 0)
		{

			const QueryResults*			psqCounters					= reinterpret_cast<QueryResults*>(psqResults[0].data());
			const deUint64				inputAssemblyPrimitives		= psqCounters->elements32[0];

			deUint32 drawCount = (m_parameters.concurrentTestType == CONCURRENT_TEST_TYPE_PIPELINE_STATISTICS_1 ||
								  m_parameters.concurrentTestType == CONCURRENT_TEST_TYPE_PIPELINE_STATISTICS_3)
								 ? 2 : 1;

			if (inputAssemblyPrimitives != primitivesGenerated * drawCount)
			{
				const std::string message = std::string("input assembly primitives == ") + de::toString(inputAssemblyPrimitives) + ", expected " + de::toString(primitivesGenerated * drawCount);
				return tcu::TestStatus::fail(message);
			}
		}
	}

	const auto tcuFormat	= vk::mapVkFormat(colorFormat);
	const auto iWidth		= static_cast<int>(m_imageWidth);
	const auto iHeight		= static_cast<int>(m_imageHeight);

	tcu::ConstPixelBufferAccess	resultBuffer	= tcu::ConstPixelBufferAccess(tcuFormat, iWidth, iHeight, 1, colorOutputBuffer->getAllocation().getHostPtr());
	tcu::TextureLevel			referenceLevel	(tcuFormat, iWidth, iHeight);
	const auto					referenceAccess	= referenceLevel.getAccess();
	const tcu::Vec4				bgColor			(0.0f, 0.0f, 0.0f, 0.0f);
	const tcu::Vec4				geomColor		(0.0f, 1.0f, 0.0f, 1.0f);
	const tcu::Vec4				threshold		(0.0f, 0.0f, 0.0f, 0.0f);
	const int					colorRow		= 7;

	tcu::clear(referenceAccess, bgColor);
	const auto subregion = tcu::getSubregion(referenceAccess, 0, colorRow, iWidth, 1);
	tcu::clear(subregion, geomColor);

	if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultBuffer, threshold, tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Color buffer contains unexpected results; check log for details");
	return tcu::TestStatus::pass("Pass");
}

void ConcurrentPrimitivesGeneratedQueryTestInstance::draw (const DeviceInterface& vk, vk::VkCommandBuffer cmdBuffer, deUint32 vertexCount, vk::VkBuffer indirectBuffer)
{
	if (m_parameters.indirect)
		vk.cmdDrawIndirect(cmdBuffer, indirectBuffer, 0u, 1u, sizeof(vk::VkDrawIndirectCommand));
	else
		vk.cmdDraw(cmdBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

Move<VkPipeline> ConcurrentPrimitivesGeneratedQueryTestInstance::makeGraphicsPipeline (const DeviceInterface& vk, const VkDevice device, const VkRenderPass renderPass)
{
	const VkDescriptorSetLayout						descriptorSetLayout				= DE_NULL;
	const Unique<VkPipelineLayout>					pipelineLayout					(makePipelineLayout(vk, device, descriptorSetLayout));
	const std::vector<VkViewport>					viewports						(1, makeViewport(makeExtent2D(m_imageWidth, m_imageHeight)));
	const std::vector<VkRect2D>						scissors						(1, makeRect2D(makeExtent2D(m_imageWidth, m_imageHeight)));
	const deUint32									subpass							= 0u;
	const deUint32									patchControlPoints				= topologyData.at(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST).primitiveSize;
	const Unique<VkShaderModule>					vertModule						(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	Move<VkShaderModule>							tescModule;
	Move<VkShaderModule>							teseModule;
	Move<VkShaderModule>							geomModule;
	Move<VkShaderModule>							fragModule						= createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u);
	VkVertexInputBindingDescription					bindingDescription;
	VkVertexInputAttributeDescription				attributeDescription;

	if (m_parameters.shaderStage == SHADER_STAGE_TESSELLATION_EVALUATION)
	{
		tescModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("tesc"), 0u);
		teseModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("tese"), 0u);
	}

	if (m_parameters.shaderStage == SHADER_STAGE_GEOMETRY)
		geomModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("geom"), 0u);

	bindingDescription.binding		= 0;
	bindingDescription.stride		= sizeof(tcu::Vec2);
	bindingDescription.inputRate	= VK_VERTEX_INPUT_RATE_VERTEX;

	attributeDescription.binding	= 0;
	attributeDescription.location	= 0;
	attributeDescription.format		= VK_FORMAT_R32G32_SFLOAT;
	attributeDescription.offset		= 0;

	const VkPipelineVertexInputStateCreateInfo		vertexInputStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType
		DE_NULL,													// const void*								pNext
		0u,															// VkPipelineVertexInputStateCreateFlags	flags
		1u,															// deUint32									vertexBindingDescriptionCount
		&bindingDescription,										// const VkVertexInputBindingDescription*	pVertexBindingDescriptions
		1u,															// deUint32									vertexAttributeDescriptionCount
		&attributeDescription,										// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions
	};

	const VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// VkStructureType							sType
		DE_NULL,													// const void*								pNext
		0,															// VkPipelineRasterizationStateCreateFlags	flags
		VK_FALSE,													// VkBool32									depthClampEnable
		VK_FALSE,													// VkBool32									rasterizerDiscardEnable
		VK_POLYGON_MODE_FILL,										// VkPolygonMode							polygonMode
		VK_CULL_MODE_NONE,											// VkCullModeFlags							cullMode
		VK_FRONT_FACE_COUNTER_CLOCKWISE,							// VkFrontFace								frontFace
		VK_FALSE,													// VkBool32									depthBiasEnable
		0.0f,														// float									depthBiasConstantFactor
		0.0f,														// float									depthBiasClamp
		0.0f,														// float									depthBiasSlopeFactor
		1.0f														// float									lineWidth
	};

	const VkPipelineColorBlendAttachmentState		colorBlendAttachmentState		=
	{
		VK_TRUE,					// VkBool32					blendEnable
		VK_BLEND_FACTOR_ONE,		// VkBlendFactor			srcColorBlendFactor
		VK_BLEND_FACTOR_ONE,		// VkBlendFactor			dstColorBlendFactor
		VK_BLEND_OP_ADD,			// VkBlendOp				colorBlendOp
		VK_BLEND_FACTOR_ONE,		// VkBlendFactor			srcAlphaBlendFactor
		VK_BLEND_FACTOR_ONE,		// VkBlendFactor			dstAlphaBlendFactor
		VK_BLEND_OP_ADD,			// VkBlendOp				alphaBlendOp
		VK_COLOR_COMPONENT_R_BIT	// VkColorComponentFlags	colorWriteMask
		| VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT
	};

	const VkPipelineColorBlendStateCreateInfo		colorBlendStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType
		DE_NULL,													// const void*									pNext
		0,															// VkPipelineColorBlendStateCreateFlags			flags
		VK_FALSE,													// VkBool32										logicOpEnable
		VK_LOGIC_OP_NO_OP,											// VkLogicOp									logicOp
		1,															// deUint32										attachmentCount
		&colorBlendAttachmentState,									// const VkPipelineColorBlendAttachmentState*	pAttachments
		{ 0.0f, 0.0f, 0.0f, 0.0f }									// float										blendConstants[4]
	};

	const VkPipelineMultisampleStateCreateInfo		multisampleStateCreateInfo		=
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

	return vk::makeGraphicsPipeline(vk,
									device,
									*pipelineLayout,
									*vertModule,
									*tescModule,
									*teseModule,
									*geomModule,
									*fragModule,
									renderPass,
									viewports,
									scissors,
									m_parameters.primitiveTopology,
									subpass,
									patchControlPoints,
									&vertexInputStateCreateInfo,
									&rasterizationStateCreateInfo,
									&multisampleStateCreateInfo,
									DE_NULL,
									&colorBlendStateCreateInfo);
}

void ConcurrentPrimitivesGeneratedQueryTestInstance::fillVertexBuffer(tcu::Vec2* vertices, const deUint64 primitivesGenerated)
{
	const float step = 1.0f / static_cast<float>(primitivesGenerated - 1);

	switch (m_parameters.primitiveTopology)
	{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
		{
			for (deUint32 prim = 0; prim < primitivesGenerated; ++prim)
			{
				vertices[prim] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, 0.0f);
			}
			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		{
			for (deUint32 prim = 0; prim < primitivesGenerated; ++prim)
			{
				vertices[2* prim + 0] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step,  0.0f);
				vertices[2* prim + 1] = tcu::Vec2(-1.0f + 2.0f * (float)(prim + 1) * step, 0.0f);
			}
			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		{
			vertices[0] = tcu::Vec2(-1.0f, 0.0f);
			vertices[1] = tcu::Vec2(-1.0f, 0.0f);

			for (deUint32 prim = 1; prim < primitivesGenerated; ++prim)
				vertices[1 + prim] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step,  0.0f);
			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		{
			vertices[0] = tcu::Vec2(-1.0f,				 -0.001f);
			vertices[1] = tcu::Vec2(-1.0f,				 -0.002f);
			vertices[2] = tcu::Vec2(-1.0f + 2.0f * step, -0.002f);

			for (deUint32 prim = 1; prim < primitivesGenerated; ++prim)
			{
				vertices[2 + prim] = tcu::Vec2(-1.0f + 2.0f * (float)(prim + 1) * step, (prim % 2 == 0) ? -0.002f : -0.001f);
			}
			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
		{
			vertices[0] = tcu::Vec2(0.0f, -0.1f);
			for (deUint32 prim = 0; prim < primitivesGenerated + 1; ++prim)
			{
				vertices[1 + prim] = tcu::Vec2(-1.0f + 2.0f * (float)prim * step, 0.0f);
			}
			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		{
			for (deUint32 prim = 0; prim < primitivesGenerated; ++prim)
			{
				vertices[4 * prim + 0] = tcu::Vec2(-1.0f + 2.0f * (float)(prim - 1) * step, 0.0f);
				vertices[4 * prim + 1] = tcu::Vec2(-1.0f + 2.0f * (float)(prim + 0) * step, 0.0f);
				vertices[4 * prim + 2] = tcu::Vec2(-1.0f + 2.0f * (float)(prim + 1) * step, 0.0f);
				vertices[4 * prim + 3] = tcu::Vec2(-1.0f + 2.0f * (float)(prim + 2) * step, 0.0f);
			}
			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
		{
			vertices[0] = tcu::Vec2(-1.0f, 0.0f);
			vertices[1] = tcu::Vec2(-1.0f, 0.0f);

			for (deUint32 prim = 0; prim < primitivesGenerated; ++prim)
			{
				vertices[2 + prim] = tcu::Vec2(-1.0f + 2.0f * (float)(prim + 1) * step,  0.0f);
			}

			vertices[2 + primitivesGenerated] = tcu::Vec2(1.0f, 0.0f);

			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
		{
			for (deUint32 prim = 0; prim < primitivesGenerated; ++prim)
			{
				vertices[3 * prim + 0] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step, -0.01f);
				vertices[3 * prim + 1] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step, 0.01f);
				vertices[3 * prim + 2] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 1) * step, 0.0f);
			}
			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		{
			for (deUint32 prim = 0; prim < primitivesGenerated; ++prim)
			{
				vertices[6 * prim + 0] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step, -0.01f);
				vertices[6 * prim + 1] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step, -0.01f);
				vertices[6 * prim + 2] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step, -0.02f);
				vertices[6 * prim + 3] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 0) * step, -0.02f);
				vertices[6 * prim + 4] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 1) * step, -0.02f);
				vertices[6 * prim + 5] = tcu::Vec2(-1.0f + 2.0f * ((float)prim + 1) * step, -0.02f);
			}
			break;
		}
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
		{
			// A strip of triangles, each pair of them forming a quad, crossing the image from left to right.
			const auto	quarterRow	= 2.0f / (static_cast<float>(m_imageHeight) * 4.0f);		// In height.
			const auto	threeQRow	= 3.0f * quarterRow;										// In height.
			const auto	quadStep	= 2.0f / (static_cast<float>(primitivesGenerated) / 2.0f);	// In width.
			const float	yCoords[2]	= { -threeQRow, -quarterRow };

			// The first two points on the left edge of the image.
			vertices[0] = tcu::Vec2(-1.0f, yCoords[0]);
			vertices[1] = tcu::Vec2(-1.0f, yCoords[0]);
			vertices[2] = tcu::Vec2(-1.0f, yCoords[1]);
			vertices[3] = tcu::Vec2(-1.0f, yCoords[1]);

			for (uint32_t primIdx = 0u; primIdx < static_cast<uint32_t>(primitivesGenerated); ++primIdx)
			{
				const auto edgeIdx	= primIdx / 2u;
				const auto yIDx		= primIdx % 2u;
				const auto xCoord	= -1.0f + (static_cast<float>(edgeIdx) + 1.0f) * quadStep;
				const auto yCoord	= yCoords[yIDx];
				const auto vertIdx	= primIdx + 2u;

				vertices[vertIdx * 2u + 0u] = tcu::Vec2(xCoord, yCoord);	// Vertex.
				vertices[vertIdx * 2u + 1u] = tcu::Vec2(xCoord, yCoord);	// Adjacency point.
			}
			break;
		}
		default:
			TCU_THROW(InternalError, "Unrecognized primitive topology");
	}
}

void ConcurrentPrimitivesGeneratedQueryTestInstance::copyColorImageToBuffer (const DeviceInterface& vk, VkCommandBuffer cmdBuffer, VkImage image, VkBuffer buffer)
{
	const VkImageSubresourceRange colorSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
	vk::VkImageMemoryBarrier postImageBarrier = vk::makeImageMemoryBarrier(vk::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, vk::VK_ACCESS_TRANSFER_READ_BIT, vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, vk::VK_IMAGE_LAYOUT_GENERAL, image, colorSubresourceRange);
	vk.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, vk::VK_PIPELINE_STAGE_TRANSFER_BIT, (vk::VkDependencyFlags)0u, 0u, (const vk::VkMemoryBarrier*)DE_NULL, 0u, (const vk::VkBufferMemoryBarrier*)DE_NULL, 1u, &postImageBarrier);
	vk::VkExtent3D extent = { m_imageWidth, m_imageHeight, 1 };
	const auto colorSubresourceLayers = vk::makeImageSubresourceLayers(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const vk::VkBufferImageCopy	colorCopyRegion = vk::makeBufferImageCopy(extent, colorSubresourceLayers);
	vk.cmdCopyImageToBuffer(cmdBuffer, image, vk::VK_IMAGE_LAYOUT_GENERAL, buffer, 1u, &colorCopyRegion);
}

class ConcurrentPrimitivesGeneratedQueryTestCase : public vkt::TestCase
{
public:
							ConcurrentPrimitivesGeneratedQueryTestCase	(tcu::TestContext &context, const char *name, const char *description, const ConcurrentTestParameters& parameters)
								: TestCase		(context, name, description)
								, m_parameters	(parameters)
							{
							}

private:
	void					checkSupport						(vkt::Context& context) const;
	void					initPrograms						(vk::SourceCollections& programCollection) const;
	vkt::TestInstance*		createInstance						(vkt::Context& context) const { return new ConcurrentPrimitivesGeneratedQueryTestInstance(context, m_parameters); }

	const ConcurrentTestParameters	m_parameters;
};

void ConcurrentPrimitivesGeneratedQueryTestCase::checkSupport (vkt::Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_primitives_generated_query");
	context.requireDeviceFunctionality("VK_EXT_transform_feedback");

	const VkPhysicalDevicePrimitivesGeneratedQueryFeaturesEXT&	pgqFeatures		= context.getPrimitivesGeneratedQueryFeaturesEXT();
	const VkPhysicalDeviceTransformFeedbackFeaturesEXT&			xfbFeatures		= context.getTransformFeedbackFeaturesEXT();
	const VkPhysicalDeviceTransformFeedbackPropertiesEXT&		xfbProperties	= context.getTransformFeedbackPropertiesEXT();

	if (pgqFeatures.primitivesGeneratedQuery != VK_TRUE)
		TCU_THROW(NotSupportedError, "VK_QUERY_TYPE_PRIMITIVES_GENERATED_EXT not supported");

	if (m_parameters.shaderStage == SHADER_STAGE_GEOMETRY || topologyData.at(m_parameters.primitiveTopology).hasAdjacency)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);

	if (m_parameters.shaderStage == SHADER_STAGE_TESSELLATION_EVALUATION)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

	if (m_parameters.nonZeroStreams())
	{
		const deUint32 requiredStreams	= de::max(m_parameters.pgqStreamIndex(), m_parameters.xfbStreamIndex());

		if (m_parameters.pgqStreamIndex() > 0 && pgqFeatures.primitivesGeneratedQueryWithNonZeroStreams != VK_TRUE)
			TCU_THROW(NotSupportedError, "primitivesGeneratedQueryWithNonZeroStreams not supported");

		if (xfbProperties.maxTransformFeedbackStreams <= requiredStreams)
			TCU_THROW(NotSupportedError, "Required amount of XFB streams not supported");
	}

	if (xfbFeatures.transformFeedback != VK_TRUE)
		TCU_THROW(NotSupportedError, "transformFeedback not supported");

	if (xfbProperties.transformFeedbackQueries != VK_TRUE)
		TCU_THROW(NotSupportedError, "transformFeedbackQueries not supported");
}

void ConcurrentPrimitivesGeneratedQueryTestCase::initPrograms (vk::SourceCollections& programCollection) const
{
	bool transformFeedback = m_parameters.xfbStreamIndex() != VERTEX_STREAM_NONE;
	// Vertex shader.
	{
		const bool			vertXfb	= (transformFeedback && m_parameters.shaderStage == SHADER_STAGE_VERTEX);
		std::ostringstream	src;

		src	<<	"#version 450\n";
		src << "layout(location=0) in vec2 inPosition;\n";

		if (vertXfb)
			src	<<	"layout(xfb_buffer = 0, xfb_offset = 0, xfb_stride = 16, location = 0) out vec4 out0;\n";

		src	<<	"void main (void)\n"
				"{\n";

		if (m_parameters.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST && m_parameters.shaderStage == SHADER_STAGE_VERTEX)
			src	<<	"    gl_PointSize = 1.0;\n";

		src << "    gl_Position = vec4(inPosition, 0, 1);\n";

		if (vertXfb)
			src	<<	"    out0 = vec4(42);\n";

		src	<<	"}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Geometry shader.
	if (m_parameters.shaderStage == SHADER_STAGE_GEOMETRY)
	{
		const bool			outputPoints	= m_parameters.nonZeroStreams() || m_parameters.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		const char* const	inputTopology	= topologyData.at(m_parameters.primitiveTopology).inputString;
		const char* const	outputTopology	= outputPoints ? "points" : topologyData.at(m_parameters.primitiveTopology).outputString;
		const VkDeviceSize	outputPrimSize	= outputPoints ? 1 : topologyData.at(m_parameters.primitiveTopology).primitiveSize;
		VkDeviceSize		maxVertices		= (transformFeedback && m_parameters.multipleStreams()) ? outputPrimSize * 2 : outputPrimSize;
		const bool			outputZero		= m_parameters.pgqStreamIndex() != 0 && (!transformFeedback || (transformFeedback && m_parameters.xfbStreamIndex() != 0));
		if (outputZero) maxVertices *= 2;
		const std::string	pgqEmitCommand	= m_parameters.nonZeroStreams() ? std::string("EmitStreamVertex(") + de::toString(m_parameters.pgqStreamIndex()) + ")" : "EmitVertex()";
		const std::string	xfbEmitCommand	= m_parameters.nonZeroStreams() ? std::string("EmitStreamVertex(") + de::toString(m_parameters.xfbStreamIndex()) + ")" : "EmitVertex()";
		const std::string	pgqEndCommand	= m_parameters.nonZeroStreams() ? std::string("EndStreamPrimitive(") + de::toString(m_parameters.pgqStreamIndex()) + ")" : "EndPrimitive()";
		const std::string	xfbEndCommand	= m_parameters.nonZeroStreams() ? std::string("EndStreamPrimitive(") + de::toString(m_parameters.xfbStreamIndex()) + ")" : "EndPrimitive()";
		std::string			output;

		if (m_parameters.primitiveTopology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY)
		{
			output =
				// Each point will be in the middle X and Y coordinates of each triangle.
				"    const vec3 xCoords = vec3(gl_in[0].gl_Position.x, gl_in[2].gl_Position.x, gl_in[4].gl_Position.x);\n"
				"    const vec3 yCoords = vec3(gl_in[0].gl_Position.y, gl_in[2].gl_Position.y, gl_in[4].gl_Position.y);\n"
				"    const float maxX = max(max(xCoords.x, xCoords.y), xCoords.z);\n"
				"    const float minX = min(min(xCoords.x, xCoords.y), xCoords.z);\n"
				"    const float maxY = max(max(yCoords.x, yCoords.y), yCoords.z);\n"
				"    const float minY = min(min(yCoords.x, yCoords.y), yCoords.z);\n"
				"    gl_Position = vec4((maxX + minX) / 2.0, (maxY + minY) / 2.0, gl_in[0].gl_Position.z, gl_in[0].gl_Position.w);\n"
				;
		}
		else if (m_parameters.primitiveTopology != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN)
			output = "    gl_Position = gl_in[0].gl_Position;\n";
		else
			output = "    gl_Position = abs(gl_in[0].gl_Position.y) < abs(gl_in[1].gl_Position.y) ? gl_in[0].gl_Position : gl_in[1].gl_Position;\n";

		if (outputPoints)
			output += "    gl_PointSize = 1.0;\n";

		std::ostringstream	src;

		src	<<	"#version 450\n"
				"layout(" << inputTopology << ") in;\n"
				"layout(" << outputTopology << ", max_vertices = " << maxVertices << ") out;\n";

		if (transformFeedback)
			src	<<	"layout(xfb_buffer = 0, xfb_offset = 0, xfb_stride = 16, location = 0, stream = " << m_parameters.xfbStreamIndex() << ") out vec4 xfb;\n";

		src	<<	"void main (void)\n"
				"{\n";

		if (outputZero)
		{
			src << output;
			src << "    EmitVertex();\n";
			src << "    EndPrimitive();\n";
		}

		if (transformFeedback)
			src	<<	"    xfb = vec4(42);\n";

		for (VkDeviceSize i = 0; i < outputPrimSize; i++)
		{
			if (m_parameters.pgqStreamIndex() == 0)
				src << output;
			src	<<	"    " << pgqEmitCommand << ";\n";
		}

		src	<<	"    " << pgqEndCommand << ";\n";

		if (transformFeedback && m_parameters.multipleStreams())
		{
			for (VkDeviceSize i = 0; i < outputPrimSize; i++)
			{
				if (m_parameters.xfbStreamIndex() == 0)
					src << output;
				src << "    " << xfbEmitCommand << ";\n";
			}

			src	<<	"    " << xfbEndCommand << ";\n";
		}

		src	<<	"}\n";

		programCollection.glslSources.add("geom") << glu::GeometrySource(src.str());
	}

	// Fragment shader.
	{
		std::ostringstream src;

		src	<<	"#version 450\n"
				"layout(location = 0) out vec4 out0;\n"
				"void main (void)\n"
				"{\n"
				"    out0 = vec4(0.0, 1.0, 0.0, 1.0);\n"
				"}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

void testGenerator (tcu::TestCaseGroup* pgqGroup)
{
	constexpr struct ReadType
	{
		QueryReadType	type;
		const char*		name;
		const char*		desc;
	} readTypes[] =
	{
		{ QUERY_READ_TYPE_GET,	"get",	"Tests for vkGetQueryPoolResults"		},
		{ QUERY_READ_TYPE_COPY,	"copy",	"Tests for vkCmdCopyQueryPoolResults"	},
	};
	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(readTypes) == QUERY_READ_TYPE_LAST);

	constexpr struct ResetType
	{
		QueryResetType	type;
		const char*		name;
		const char*		desc;
	} resetTypes[] =
	{
		{ QUERY_RESET_TYPE_QUEUE,	"queue_reset",	"Tests for vkCmdResetQueryPool"	},
		{ QUERY_RESET_TYPE_HOST,	"host_reset",	"Tests for vkResetQueryPool"	},
	};
	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(resetTypes) == QUERY_RESET_TYPE_LAST);

	constexpr struct ResultTypes
	{
		QueryResultType	type;
		const char*		name;
		const char*		desc;
	} resultTypes[] =
	{
		{ QUERY_RESULT_TYPE_32_BIT,			"32bit",				"Tests for default query result size"							},
		{ QUERY_RESULT_TYPE_64_BIT,			"64bit",				"Tests for VK_QUERY_RESULT_64_BIT"								},
		{ QUERY_RESULT_TYPE_PGQ_32_XFB_64,	"pgq_32bit_xfb_64bit",	"Tests for PGQ without and XFBQ with VK_QUERY_RESULT_64_BIT"	},
		{ QUERY_RESULT_TYPE_PGQ_64_XFB_32,	"pgq_64bit_xfb_32bit",	"Tests for PGQ with and XFBQ without VK_QUERY_RESULT_64_BIT"	},
	};
	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(resultTypes) == QUERY_RESULT_TYPE_LAST);

	constexpr struct Shader
	{
		ShaderStage	stage;
		const char*	name;
		const char*	desc;
	} shaderStages[] =
	{
		{ SHADER_STAGE_VERTEX,					"vert",	"Vertex shader tests"					},
		{ SHADER_STAGE_TESSELLATION_EVALUATION,	"tese",	"Tessellation evaluation shader tests"	},
		{ SHADER_STAGE_GEOMETRY,				"geom",	"Geometry shader tests"					},
	};
	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(shaderStages) == SHADER_STAGE_LAST);

	constexpr struct TransformFeedbackState
	{
		deBool		enable;
		const char*	name;
		const char*	desc;
	} transformFeedbackStates[] =
	{
		{ DE_FALSE,	"no_xfb",	"Tests without transform feedback"											},
		{ DE_TRUE,	"xfb",		"Tests for comparing PGQ results against transform feedback query results"	},
	};

	constexpr struct RastCase
	{
		RasterizationCase	type;
		deBool				dsAttachment;
		const char*			name;
		const char*			desc;
	} rastCases[] =
	{
		{ RAST_CASE_DISCARD,						DE_FALSE,	"no_rast",							"Tests with rasterizer discard"																			},
		{ RAST_CASE_DEFAULT,						DE_FALSE,	"rast",								"Tests without rasterizer discard"																		},
		{ RAST_CASE_EMPTY_FRAG,						DE_FALSE,	"empty_frag",						"Tests with an empty fragment shader"																	},
		{ RAST_CASE_NO_ATTACHMENT,					DE_FALSE,	"no_attachment",					"Tests with an attachmentless render pass"																},
		{ RAST_CASE_COLOR_WRITE_DISABLE_STATIC,		DE_FALSE,	"color_write_disable_static",		"Tests disabling color output using VkPipelineColorWriteCreateInfoEXT"									},
		{ RAST_CASE_COLOR_WRITE_DISABLE_STATIC,		DE_TRUE,	"color_write_disable_static_ds",	"Tests disabling color output using VkPipelineColorWriteCreateInfoEXT with a depth stencil attachment"	},
		{ RAST_CASE_COLOR_WRITE_DISABLE_DYNAMIC,	DE_FALSE,	"color_write_disable_dynamic",		"Tests disabling color output using vkCmdSetColorWriteEnableEXT"										},
		{ RAST_CASE_COLOR_WRITE_DISABLE_DYNAMIC,	DE_TRUE,	"color_write_disable_dynamic_ds",	"Tests disabling color output using vkCmdSetColorWriteEnableEXT with a depth stencil attachment"		},
	};

	static const std::set<RasterizationCase> rasterizationCasesWithAvailability
	{
		RAST_CASE_DISCARD,
		RAST_CASE_DEFAULT,
		RAST_CASE_NO_ATTACHMENT,
	};

	constexpr struct Topology
	{
		VkPrimitiveTopology	type;
		const char*			name;
		const char*			desc;
	} topologies[] =
	{
		{ VK_PRIMITIVE_TOPOLOGY_POINT_LIST,						"point_list",						"Tests for separate point primitives"																		},
		{ VK_PRIMITIVE_TOPOLOGY_LINE_LIST,						"line_list",						"Tests for separate line primitives"																		},
		{ VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,						"line_strip",						"Tests for connected line primitives with consecutive lines sharing a vertex"								},
		{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,					"triangle_list",					"Tests for separate triangle primitives"																	},
		{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,					"triangle_strip",					"Tests for connected triangle primitives with consecutive triangles sharing an edge"						},
		{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,					"triangle_fan",						"Tests for connected triangle primitives with all triangles sharing a common vertex"						},
		{ VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,		"line_list_with_adjacency",			"Tests for separate line primitives with adjacency"															},
		{ VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,		"line_strip_with_adjacency",		"Tests for connected line primitives with adjacency, with consecutive primitives sharing three vertices"	},
		{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,	"triangle_list_with_adjacency",		"Tests for separate triangle primitives with adjacency"														},
		{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY,	"triangle_strip_with_adjacency",	"Tests for connected triangle primitives with adjacency, with consecutive triangles sharing an edge"		},
		{ VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,						"patch_list",						"Tests for separate patch primitives"																		},
	};

	static const std::set<VkPrimitiveTopology> topologiesWithAvailability
	{
		VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
	};

	// Tests for vkCmdBeginQueryIndexedEXT and vkCmdEndQueryIndexedEXT.
	constexpr struct StreamIndex
	{
		VertexStream	index;
		const char*		name;
	} streamIndices[] =
	{
		{ VERTEX_STREAM_DEFAULT,	"default"	},
		{ VERTEX_STREAM_0,			"0"			},
		{ VERTEX_STREAM_1,			"1"			},
	};

	constexpr struct CmdBufCase
	{
		CommandBufferCase	type;
		const char*			name;
		const char*			desc;
	} cmdBufCases[] =
	{
		{ CMD_BUF_CASE_SINGLE_DRAW,	"single_draw",	"Test single draw call"	},
		{ CMD_BUF_CASE_TWO_DRAWS,	"two_draws",	"Test two draw calls"	},
	};
	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(cmdBufCases) == CMD_BUF_CASE_LAST);

	constexpr struct
	{
		uint32_t			queryCount;
		const char*			nameSuffix;
		const char*			descSuffix;
	} queryCountCases[] =
	{
		{ 1u,	"",				""					},
		{ 2u,	"_2_queries",	" using 2 queries"	},
	};

	constexpr struct QueryOrderCase
	{
		QueryOrder			order;
		const char*			name;
		const char*			desc;
	} queryOrderCases[] =
	{
		{ QUERY_ORDER_PGQ_FIRST,	"pqg_first",	"Test starting primitives generated query first"	},
		{ QUERY_ORDER_XFBQ_FIRST,	"xfbq_first",	"Test starting transform feedback query first"		},
	};
	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(queryOrderCases) == QUERY_ORDER_LAST);

	constexpr struct OutsideDrawCase
	{
		OutsideDraw			outsideDraw;
		const char*			name;
		const char*			desc;
	} outsideDrawCases[] =
	{
		{ OUTSIDE_DRAW_NONE,		"none",			"Test without draws outside of active queries" },
		{ OUTSIDE_DRAW_BEFORE,		"before",		"Test with draws before active queries" },
		{ OUTSIDE_DRAW_AFTER,		"after",		"Test with draw after active queries" },
	};
	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(outsideDrawCases) == OUTSIDE_DRAW_LAST);

	tcu::TestContext& testCtx = pgqGroup->getTestContext();

	for (const ReadType& read : readTypes)
	{
		de::MovePtr<tcu::TestCaseGroup> readGroup(new tcu::TestCaseGroup(testCtx, read.name, read.desc));

		for (const ResetType& reset : resetTypes)
		{
			de::MovePtr<tcu::TestCaseGroup> resetGroup(new tcu::TestCaseGroup(testCtx, reset.name, reset.desc));

			for (const ResultTypes& result : resultTypes)
			{
				de::MovePtr<tcu::TestCaseGroup> resultGroup(new tcu::TestCaseGroup(testCtx, result.name, result.desc));

				for (const Shader& shader : shaderStages)
				{
					de::MovePtr<tcu::TestCaseGroup> shaderGroup(new tcu::TestCaseGroup(testCtx, shader.name, shader.desc));

					for (const TransformFeedbackState& xfbState : transformFeedbackStates)
					{
						de::MovePtr<tcu::TestCaseGroup> xfbGroup(new tcu::TestCaseGroup(testCtx, xfbState.name, xfbState.desc));

						// Only test multiple result types with XFB enabled.
						if ((result.type == QUERY_RESULT_TYPE_PGQ_32_XFB_64 || result.type == QUERY_RESULT_TYPE_PGQ_64_XFB_32) && !xfbState.enable)
							continue;

						for (const RastCase& rastCase : rastCases)
						{
							de::MovePtr<tcu::TestCaseGroup> rastGroup(new tcu::TestCaseGroup(testCtx, rastCase.name, rastCase.desc));

							// Skip uninteresting cases
							if ((rastCase.type > RAST_CASE_DISCARD)
								&& ((read.type != QUERY_READ_TYPE_GET)
								|| (reset.type != QUERY_RESET_TYPE_QUEUE)
								|| (result.type != QUERY_RESULT_TYPE_32_BIT)))
							{
								continue;
							}

							for (const Topology& topology : topologies)
							{
								de::MovePtr<tcu::TestCaseGroup> topologyGroup(new tcu::TestCaseGroup(testCtx, topology.name, topology.desc));

								// Only test patch lists with tessellation shaders.
								if ((topology.type == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST && shader.stage != SHADER_STAGE_TESSELLATION_EVALUATION) ||
								   ((topology.type != VK_PRIMITIVE_TOPOLOGY_PATCH_LIST && shader.stage == SHADER_STAGE_TESSELLATION_EVALUATION)))
								{
									continue;
								}

								// Only test adjacency topologies with geometry shaders.
								if (shader.stage != SHADER_STAGE_GEOMETRY && topologyData.at(topology.type).hasAdjacency)
									continue;

								for (const StreamIndex& pgqStream : streamIndices)
								{
									for (const StreamIndex& xfbStream : streamIndices)
									{
										const std::string			streamGroupName	= std::string("pgq_") + pgqStream.name + (xfbState.enable ? std::string("_xfb_") + xfbStream.name : "");
										const bool					pgqDefault		= (pgqStream.index == VERTEX_STREAM_DEFAULT);
										const bool					xfbDefault		= (xfbStream.index == VERTEX_STREAM_DEFAULT);
										const std::string			pgqDescStr		= std::string("PGQ on ") + (pgqDefault ? "default " : "") + std::string("vertex stream ") + (pgqDefault ? "" : pgqStream.name);
										const std::string			xfbDescStr		= std::string("XFB on ") + (xfbDefault ? "default " : "") + std::string("vertex stream ") + (xfbDefault ? "" : xfbStream.name);
										const std::string			streamGroupDesc	= std::string("Tests for ") + pgqDescStr + (xfbState.enable ? (std::string(" and ") + xfbDescStr) : "");
										de::MovePtr<tcu::TestCaseGroup>	streamGroup(new tcu::TestCaseGroup(testCtx, streamGroupName.c_str(), streamGroupDesc.c_str()));

										// Only test nondefault vertex streams with geometry shaders.
										if ((pgqStream.index != VERTEX_STREAM_DEFAULT || xfbStream.index != VERTEX_STREAM_DEFAULT) && shader.stage != SHADER_STAGE_GEOMETRY)
											continue;

										// Skip nondefault vertex streams for XFB when not enabled.
										if (!xfbState.enable && xfbStream.index != VERTEX_STREAM_DEFAULT)
											continue;

										for (const CmdBufCase& cmdBufCase : cmdBufCases)
										{
											de::MovePtr<tcu::TestCaseGroup>	cmdBufGroup(new tcu::TestCaseGroup(testCtx, cmdBufCase.name, cmdBufCase.desc));

											for (const QueryOrderCase& queryOrderCase : queryOrderCases)
											{
												if (queryOrderCase.order == QUERY_ORDER_XFBQ_FIRST && xfbState.enable == DE_FALSE)
													continue;

												de::MovePtr<tcu::TestCaseGroup>	queryOrderGroup(new tcu::TestCaseGroup(testCtx, queryOrderCase.name, queryOrderCase.desc));
												for (const OutsideDrawCase& outSideDrawCase : outsideDrawCases)
												{
													for (const auto& countCase : queryCountCases)
													{
														for (const auto availabilityBit : { false, true})
														{
															if (availabilityBit && topologiesWithAvailability.count(topology.type) == 0)
																continue;

															if (availabilityBit && rasterizationCasesWithAvailability.count(rastCase.type) == 0)
																continue;

															if (availabilityBit && cmdBufCase.type != CMD_BUF_CASE_SINGLE_DRAW)
																continue;

															const TestParameters	parameters =
															{
																read.type,						// QueryReadType		queryReadType
																reset.type,						// QueryResetType		queryResetType
																result.type,					// QueryResultType		queryResultType
																shader.stage,					// ShaderStage			shaderStage
																xfbState.enable,				// deBool				transformFeedback
																rastCase.type,					// RasterizationCase	rastCase
																rastCase.dsAttachment,			// deBool				depthStencilAttachment
																topology.type,					// VkPrimitiveTopology	primitiveTopology
																pgqStream.index,				// VertexStreamIndex	pgqStreamIndex
																xfbStream.index,				// VertexStreamIndex	xfbStreamIndex
																cmdBufCase.type,				// CommandBufferCase	cmdBufCase
																countCase.queryCount,			// const uint32_t		queryCount
																queryOrderCase.order,			// QueryOrder			queryOrder
																outSideDrawCase.outsideDraw,	// OutsideDraw			outsideDraw
																availabilityBit,				// const bool			availabilityBit
															};

															const auto availabilityNameSuffix = (availabilityBit ? "_with_availability" : "");
															const auto availabilityDescSuffix = (availabilityBit ? " using VK_QUERY_RESULT_WITH_AVAILABILITY_BIT" : "");

															const auto name = std::string(outSideDrawCase.name) + countCase.nameSuffix + availabilityNameSuffix;
															const auto desc = std::string(outSideDrawCase.desc) + countCase.descSuffix + availabilityDescSuffix;

															queryOrderGroup->addChild(new PrimitivesGeneratedQueryTestCase(testCtx, name.c_str(), desc.c_str(), parameters));
														}
													}
												}

												cmdBufGroup->addChild(queryOrderGroup.release());
											}

											streamGroup->addChild(cmdBufGroup.release());
										}

										topologyGroup->addChild(streamGroup.release());
									}
								}

								rastGroup->addChild(topologyGroup.release());
							}

							xfbGroup->addChild(rastGroup.release());
						}

						shaderGroup->addChild(xfbGroup.release());
					}

					resultGroup->addChild(shaderGroup.release());
				}

				resetGroup->addChild(resultGroup.release());
			}

			readGroup->addChild(resetGroup.release());
		}

		pgqGroup->addChild(readGroup.release());
	}

	constexpr struct ConcurrentTestTypeCase
	{
		ConcurrentTestType	type;
		const char*			name;
		const char*			desc;
	}
	concurrentTestTypeCases[] =
	{
		{ CONCURRENT_TEST_TYPE_TWO_XFB_INSIDE_PGQ,			"two_xfbq_inside_pgq",			"Test two transfer feedback queries inside a primitive generated query" },
		{ CONCURRENT_TEST_TYPE_PGQ_SECONDARY_CMD_BUFFER,	"pgq_secondary_cmd_buffers",	"Test primitive generated query in secondary command buffers"			},
		{ CONCURRENT_TEST_TYPE_PIPELINE_STATISTICS_1,		"pipeline_statistics_1",		"Test primitive generated query with pipeline statistics query"			},
		{ CONCURRENT_TEST_TYPE_PIPELINE_STATISTICS_2,		"pipeline_statistics_2",		"Test primitive generated query with pipeline statistics query"			},
		{ CONCURRENT_TEST_TYPE_PIPELINE_STATISTICS_3,		"pipeline_statistics_3",		"Test primitive generated query with pipeline statistics query"			},
	};
	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(concurrentTestTypeCases) == CONCURRENT_TEST_TYPE_LAST);


	constexpr struct DrawTypeCase
	{
		bool		indirect;
		const char* name;
		const char* desc;
	}
	drawTypeCases[] =
	{
		{ false,	"draw",		"cmdDraw"			},
		{ true,		"indirect",	"cmdDrawIndirect"	},
	};

	de::MovePtr<tcu::TestCaseGroup> concurrentGroup(new tcu::TestCaseGroup(testCtx, "concurrent", "Test running concurrent queries"));

	for (const ConcurrentTestTypeCase& concurrentTestType : concurrentTestTypeCases)
	{
		de::MovePtr<tcu::TestCaseGroup> concurrentTypeGroup(new tcu::TestCaseGroup(testCtx, concurrentTestType.name, concurrentTestType.desc));
		for (const ResultTypes& result : resultTypes)
		{
			de::MovePtr<tcu::TestCaseGroup> resultGroup(new tcu::TestCaseGroup(testCtx, result.name, result.desc));

			for (const Topology& topology : topologies)
			{
				// Testing only with geometry shaders, skip patch list
				if (topology.type == vk::VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
					continue;

				de::MovePtr<tcu::TestCaseGroup> topologyGroup(new tcu::TestCaseGroup(testCtx, topology.name, topology.desc));

				for (const DrawTypeCase& draw : drawTypeCases)
				{
					VertexStream pgqStreamIndex = VERTEX_STREAM_DEFAULT;
					VertexStream xfbStreamIndex = VERTEX_STREAM_NONE;
					if (concurrentTestType.type == CONCURRENT_TEST_TYPE_TWO_XFB_INSIDE_PGQ)
						xfbStreamIndex = VERTEX_STREAM_1;

					const ConcurrentTestParameters	parameters =
					{
						concurrentTestType.type,		// ConcurrentTestType	concurrentTestType;
						result.type,					// QueryResultType		queryResultType
						SHADER_STAGE_GEOMETRY,			// ShaderStage			shaderStage
						topology.type,					// VkPrimitiveTopology	primitiveTopology
						pgqStreamIndex,					// VertexStreamIndex	pgqStreamIndex
						xfbStreamIndex,					// VertexStreamIndex	xfbStreamIndex
						draw.indirect,					// bool					indirect
					};

					topologyGroup->addChild(new ConcurrentPrimitivesGeneratedQueryTestCase(testCtx, draw.name, draw.desc, parameters));
				}

				resultGroup->addChild(topologyGroup.release());
			}

			concurrentTypeGroup->addChild(resultGroup.release());
		}

		concurrentGroup->addChild(concurrentTypeGroup.release());
	}
	pgqGroup->addChild(concurrentGroup.release());
}

} // anonymous

tcu::TestCaseGroup* createPrimitivesGeneratedQueryTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "primitives_generated_query", "Primitives Generated Query Tests", testGenerator);
}

} // TransformFeedback
} // vkt
