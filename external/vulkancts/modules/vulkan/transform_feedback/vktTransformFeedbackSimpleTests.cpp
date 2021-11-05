/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
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
 * \brief Vulkan Transform Feedback Simple Tests
 *//*--------------------------------------------------------------------*/

#include "vktTransformFeedbackSimpleTests.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"

#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"

#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuRGBA.hpp"
#include "tcuTestLog.hpp"

#include <iostream>
#include <functional>
#include <set>
#include <algorithm>

namespace vkt
{
namespace TransformFeedback
{
namespace
{
using namespace vk;
using de::MovePtr;
using de::UniquePtr;
using de::SharedPtr;

#define VALIDATE_MINIMUM(A,B) if ((A) < (B)) TCU_FAIL(#A "==" + de::toString(A) + " which is less than required by specification (" + de::toString(B) + ")")
#define VALIDATE_BOOL(A) if (! ( (A) == VK_TRUE || (A) == VK_FALSE) ) TCU_FAIL(#A " expected to be VK_TRUE or VK_FALSE. Received " + de::toString((deUint64)(A)))

enum TestType
{
	TEST_TYPE_BASIC,
	TEST_TYPE_RESUME,
	TEST_TYPE_STREAMS,
	TEST_TYPE_XFB_POINTSIZE,
	TEST_TYPE_XFB_CLIPDISTANCE,
	TEST_TYPE_XFB_CULLDISTANCE,
	TEST_TYPE_XFB_CLIP_AND_CULL,
	TEST_TYPE_WINDING,
	TEST_TYPE_STREAMS_POINTSIZE,
	TEST_TYPE_STREAMS_CLIPDISTANCE,
	TEST_TYPE_STREAMS_CULLDISTANCE,
	TEST_TYPE_MULTISTREAMS,
	TEST_TYPE_DRAW_INDIRECT,
	TEST_TYPE_BACKWARD_DEPENDENCY,
	TEST_TYPE_QUERY_GET,
	TEST_TYPE_QUERY_COPY,
	TEST_TYPE_QUERY_RESET,
	TEST_TYPE_MULTIQUERY,
	TEST_TYPE_LAST
};

enum StreamId0Mode
{
	STREAM_ID_0_NORMAL					= 0,
	STREAM_ID_0_BEGIN_QUERY_INDEXED		= 1,
	STREAM_ID_0_END_QUERY_INDEXED		= 2,
};

struct TestParameters
{
	TestType			testType;
	deUint32			bufferSize;
	deUint32			partCount;
	deUint32			streamId;
	deUint32			pointSize;
	deUint32			vertexStride;
	StreamId0Mode		streamId0Mode;
	bool				query64bits;
	bool				noOffsetArray;
	VkPrimitiveTopology	primTopology;
};

struct TopologyInfo
{
	deUint32							primSize;			// The size of the on primitive.
	std::string							topologyName;		// The suffix for the name of test.
	std::function<deUint64(deUint64)>	getNumPrimitives;	// The number of primitives generated.
	std::function<deUint64(deUint64)>	getNumVertices;		// The number of vertices generated.
};

const std::map<VkPrimitiveTopology, TopologyInfo> topologyData =
{
	{ VK_PRIMITIVE_TOPOLOGY_POINT_LIST						, { 1, ""								,[](deUint64 vertexCount)	{	return vertexCount;				}	,[](deUint64 primCount)	{	return primCount;			}, } },
	{ VK_PRIMITIVE_TOPOLOGY_LINE_LIST						, { 2, "line_list_"						,[](deUint64 vertexCount)	{	return vertexCount / 2u;		}	,[](deUint64 primCount) {	return primCount * 2u;		}, } },
	{ VK_PRIMITIVE_TOPOLOGY_LINE_STRIP						, { 2, "line_strip_"					,[](deUint64 vertexCount)	{	return vertexCount - 1u;		}	,[](deUint64 primCount) {	return primCount + 1u;		}, } },
	{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST					, { 3, "triangle_list_"					,[](deUint64 vertexCount)	{	return vertexCount / 3u;		}	,[](deUint64 primCount) {	return primCount * 3u;		}, } },
	{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP					, { 3, "triangle_strip_"				,[](deUint64 vertexCount)	{	return vertexCount - 2u;		}	,[](deUint64 primCount) {	return primCount + 2u;		}, } },
	{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN					, { 3, "triangle_fan_"					,[](deUint64 vertexCount)	{	return vertexCount - 2u;		}	,[](deUint64 primCount) {	return primCount + 2u;		}, } },
	{ VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY		, { 2, "line_list_with_adjacency_"		,[](deUint64 vertexCount)	{	return vertexCount / 4u;		}	,[](deUint64 primCount) {	return primCount * 4u;		}, } },
	{ VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY		, { 2, "line_strip_with_adjacency_"		,[](deUint64 vertexCount)	{	return vertexCount - 3u;		}	,[](deUint64 primCount) {	return primCount + 3u;		}, } },
	{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY	, { 3, "triangle_list_with_adjacency_"	,[](deUint64 vertexCount)	{	return vertexCount / 6u;		}	,[](deUint64 primCount) {	return primCount * 6u;		}, } },
	{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY	, { 3, "triangle_strip_with_adjacency_"	,[](deUint64 vertexCount)	{	return (vertexCount - 4u) / 2u;	}	,[](deUint64 primCount) {	return primCount * 2u + 4u;	}, } },
	{ VK_PRIMITIVE_TOPOLOGY_PATCH_LIST						, { 3, "patch_list_"					,[](deUint64 vertexCount)	{	return vertexCount / 3u;		}	,[](deUint64 primCount) {	return primCount * 3u;		}, } },
};

struct TransformFeedbackQuery
{
	deUint32	written;
	deUint32	attempts;
};

const deUint32 MINIMUM_TF_BUFFER_SIZE	= (1<<27);
const deUint32 IMAGE_SIZE				= 64u;

template<typename T>
inline SharedPtr<Unique<T> > makeSharedPtr(Move<T> move)
{
	return SharedPtr<Unique<T> >(new Unique<T>(move));
}

Move<VkPipelineLayout> makePipelineLayout (const DeviceInterface&		vk,
										   const VkDevice				device)
{
	const VkPushConstantRange			pushConstantRanges			=
	{
		VK_SHADER_STAGE_VERTEX_BIT,						//  VkShaderStageFlags				stageFlags;
		0u,												//  deUint32						offset;
		sizeof(deUint32)								//  deUint32						size;
	};
	const VkPipelineLayoutCreateInfo	pipelineLayoutCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	//  VkStructureType					sType;
		DE_NULL,										//  const void*						pNext;
		(VkPipelineLayoutCreateFlags)0,					//  VkPipelineLayoutCreateFlags		flags;
		0u,												//  deUint32						setLayoutCount;
		DE_NULL,										//  const VkDescriptorSetLayout*	pSetLayouts;
		1u,												//  deUint32						pushConstantRangeCount;
		&pushConstantRanges,							//  const VkPushConstantRange*		pPushConstantRanges;
	};
	return createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
}

Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&		vk,
									   const VkDevice				device,
									   const VkPipelineLayout		pipelineLayout,
									   const VkRenderPass			renderPass,
									   const VkShaderModule			vertexModule,
									   const VkShaderModule			tessellationControlModule,
									   const VkShaderModule			tessellationEvalModule,
									   const VkShaderModule			geometryModule,
									   const VkShaderModule			fragmendModule,
									   const VkExtent2D				renderSize,
									   const deUint32				subpass,
									   const deUint32*				rasterizationStreamPtr	= DE_NULL,
									   const VkPrimitiveTopology	topology				= VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
									   const bool					inputVertices			= false)
{
	const std::vector<VkViewport>							viewports								(1, makeViewport(renderSize));
	const std::vector<VkRect2D>								scissors								(1, makeRect2D(renderSize));
	const VkPipelineVertexInputStateCreateInfo				vertexInputStateCreateInfo			=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,									//  VkStructureType									sType
		DE_NULL,																					//  const void*										pNext
		(VkPipelineVertexInputStateCreateFlags)0,													//  VkPipelineVertexInputStateCreateFlags			flags
		0u,																							//  deUint32										vertexBindingDescriptionCount
		DE_NULL,																					//  const VkVertexInputBindingDescription*			pVertexBindingDescriptions
		0u,																							//  deUint32										vertexAttributeDescriptionCount
		DE_NULL,																					//  const VkVertexInputAttributeDescription*		pVertexAttributeDescriptions
	};
	const VkPipelineVertexInputStateCreateInfo*				vertexInputStateCreateInfoPtr		= (inputVertices) ? DE_NULL : &vertexInputStateCreateInfo;
	const VkBool32											disableRasterization				= (fragmendModule == DE_NULL);
	const deUint32											rasterizationStream					= (rasterizationStreamPtr == DE_NULL) ? 0 : *rasterizationStreamPtr;
	const VkPipelineRasterizationStateStreamCreateInfoEXT	rasterizationStateStreamCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT,						//  VkStructureType										sType;
		DE_NULL,																					//  const void*											pNext;
		0,																							//  VkPipelineRasterizationStateStreamCreateFlagsEXT	flags;
		rasterizationStream																			//  deUint32											rasterizationStream;
	};
	const VkPipelineRasterizationStateCreateInfo			rasterizationStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,									//  VkStructureType							sType;
		&rasterizationStateStreamCreateInfo,														//  const void*								pNext;
		0u,																							//  VkPipelineRasterizationStateCreateFlags	flags;
		VK_FALSE,																					//  VkBool32								depthClampEnable;
		disableRasterization,																		//  VkBool32								rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,																		//  VkPolygonMode							polygonMode;
		VK_CULL_MODE_NONE,																			//  VkCullModeFlags							cullMode;
		VK_FRONT_FACE_COUNTER_CLOCKWISE,															//  VkFrontFace								frontFace;
		VK_FALSE,																					//  VkBool32								depthBiasEnable;
		0.0f,																						//  float									depthBiasConstantFactor;
		0.0f,																						//  float									depthBiasClamp;
		0.0f,																						//  float									depthBiasSlopeFactor;
		1.0f																						//  float									lineWidth;
	};
	const VkPipelineRasterizationStateCreateInfo*			rasterizationStateCreateInfoPtr		= (rasterizationStreamPtr == DE_NULL) ? DE_NULL : &rasterizationStateCreateInfo;

	return makeGraphicsPipeline(vk,											// const DeviceInterface&							vk
								device,										// const VkDevice									device
								pipelineLayout,								// const VkPipelineLayout							pipelineLayout
								vertexModule,								// const VkShaderModule								vertexShaderModule
								tessellationControlModule,					// const VkShaderModule								tessellationControlModule
								tessellationEvalModule,						// const VkShaderModule								tessellationEvalModule
								geometryModule,								// const VkShaderModule								geometryShaderModule
								fragmendModule,								// const VkShaderModule								fragmentShaderModule
								renderPass,									// const VkRenderPass								renderPass
								viewports,									// const std::vector<VkViewport>&					viewports
								scissors,									// const std::vector<VkRect2D>&						scissors
								topology,									// const VkPrimitiveTopology						topology
								subpass,									// const deUint32									subpass
								(tessellationEvalModule != DE_NULL) * 3u,	// const deUint32									patchControlPoints
								vertexInputStateCreateInfoPtr,				// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
								rasterizationStateCreateInfoPtr);			// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
}

VkImageCreateInfo makeImageCreateInfo (const VkImageCreateFlags flags, const VkImageType type, const VkFormat format, const VkExtent2D size, const deUint32 numLayers, const VkImageUsageFlags usage)
{
	const VkExtent3D		extent		= { size.width, size.height, 1u };
	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		flags,											// VkImageCreateFlags		flags;
		type,											// VkImageType				imageType;
		format,											// VkFormat					format;
		extent,											// VkExtent3D				extent;
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

Move<VkRenderPass> makeRenderPass (const DeviceInterface&		vk,
								   const VkDevice				device)
{
	std::vector<VkSubpassDescription>	subpassDescriptions;
	std::vector<VkSubpassDependency>	subpassDependencies;

	const VkSubpassDescription	description	=
	{
		(VkSubpassDescriptionFlags)0,		//  VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,	//  VkPipelineBindPoint				pipelineBindPoint;
		0u,									//  deUint32						inputAttachmentCount;
		DE_NULL,							//  const VkAttachmentReference*	pInputAttachments;
		0u,									//  deUint32						colorAttachmentCount;
		DE_NULL,							//  const VkAttachmentReference*	pColorAttachments;
		DE_NULL,							//  const VkAttachmentReference*	pResolveAttachments;
		DE_NULL,							//  const VkAttachmentReference*	pDepthStencilAttachment;
		0,									//  deUint32						preserveAttachmentCount;
		DE_NULL								//  const deUint32*					pPreserveAttachments;
	};
	subpassDescriptions.push_back(description);

	const VkSubpassDependency	dependency	=
	{
		0u,													//  deUint32				srcSubpass;
		0u,													//  deUint32				dstSubpass;
		VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,		//  VkPipelineStageFlags	srcStageMask;
		VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,				//  VkPipelineStageFlags	dstStageMask;
		VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,	//  VkAccessFlags			srcAccessMask;
		VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT,	//  VkAccessFlags			dstAccessMask;
		0u													//  VkDependencyFlags		dependencyFlags;
	};
	subpassDependencies.push_back(dependency);

	const VkRenderPassCreateInfo renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,							//  VkStructureType					sType;
		DE_NULL,															//  const void*						pNext;
		static_cast<VkRenderPassCreateFlags>(0u),							//  VkRenderPassCreateFlags			flags;
		0u,																	//  deUint32						attachmentCount;
		DE_NULL,															//  const VkAttachmentDescription*	pAttachments;
		static_cast<deUint32>(subpassDescriptions.size()),					//  deUint32						subpassCount;
		&subpassDescriptions[0],											//  const VkSubpassDescription*		pSubpasses;
		static_cast<deUint32>(subpassDependencies.size()),					//  deUint32						dependencyCount;
		subpassDependencies.size() > 0 ? &subpassDependencies[0] : DE_NULL	//  const VkSubpassDependency*		pDependencies;
	};

	return createRenderPass(vk, device, &renderPassInfo);
}

VkImageMemoryBarrier makeImageMemoryBarrier	(const VkAccessFlags			srcAccessMask,
											 const VkAccessFlags			dstAccessMask,
											 const VkImageLayout			oldLayout,
											 const VkImageLayout			newLayout,
											 const VkImage					image,
											 const VkImageSubresourceRange	subresourceRange)
{
	const VkImageMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		srcAccessMask,									// VkAccessFlags			outputMask;
		dstAccessMask,									// VkAccessFlags			inputMask;
		oldLayout,										// VkImageLayout			oldLayout;
		newLayout,										// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					destQueueFamilyIndex;
		image,											// VkImage					image;
		subresourceRange,								// VkImageSubresourceRange	subresourceRange;
	};
	return barrier;
}

VkBufferMemoryBarrier makeBufferMemoryBarrier (const VkAccessFlags	srcAccessMask,
											   const VkAccessFlags	dstAccessMask,
											   const VkBuffer		buffer,
											   const VkDeviceSize	offset,
											   const VkDeviceSize	bufferSizeBytes)
{
	const VkBufferMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	//  VkStructureType	sType;
		DE_NULL,									//  const void*		pNext;
		srcAccessMask,								//  VkAccessFlags	srcAccessMask;
		dstAccessMask,								//  VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					//  deUint32		srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					//  deUint32		destQueueFamilyIndex;
		buffer,										//  VkBuffer		buffer;
		offset,										//  VkDeviceSize	offset;
		bufferSizeBytes,							//  VkDeviceSize	size;
	};
	return barrier;
}

VkMemoryBarrier makeMemoryBarrier (const VkAccessFlags	srcAccessMask,
								   const VkAccessFlags	dstAccessMask)
{
	const VkMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_MEMORY_BARRIER,	// VkStructureType			sType;
		DE_NULL,							// const void*				pNext;
		srcAccessMask,						// VkAccessFlags			outputMask;
		dstAccessMask,						// VkAccessFlags			inputMask;
	};
	return barrier;
}

VkQueryPoolCreateInfo makeQueryPoolCreateInfo (const deUint32 queryCountersNumber)
{
	const VkQueryPoolCreateInfo			queryPoolCreateInfo		=
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,		//  VkStructureType					sType;
		DE_NULL,										//  const void*						pNext;
		(VkQueryPoolCreateFlags)0,						//  VkQueryPoolCreateFlags			flags;
		VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT,	//  VkQueryType						queryType;
		queryCountersNumber,							//  deUint32						queryCount;
		0u,												//  VkQueryPipelineStatisticFlags	pipelineStatistics;
	};

	return queryPoolCreateInfo;
}

void fillBuffer (const DeviceInterface& vk, const VkDevice device, Allocation& bufferAlloc, VkDeviceSize bufferSize, const void* data, const VkDeviceSize dataSize)
{
	const VkMappedMemoryRange	memRange		=
	{
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,	//  VkStructureType	sType;
		DE_NULL,								//  const void*		pNext;
		bufferAlloc.getMemory(),				//  VkDeviceMemory	memory;
		bufferAlloc.getOffset(),				//  VkDeviceSize	offset;
		VK_WHOLE_SIZE							//  VkDeviceSize	size;
	};
	std::vector<deUint8>		dataVec			(static_cast<deUint32>(bufferSize), 0u);

	DE_ASSERT(bufferSize >= dataSize);

	deMemcpy(&dataVec[0], data, static_cast<deUint32>(dataSize));

	deMemcpy(bufferAlloc.getHostPtr(), &dataVec[0], dataVec.size());
	VK_CHECK(vk.flushMappedMemoryRanges(device, 1u, &memRange));
}

class TransformFeedbackTestInstance : public TestInstance
{
public:
													TransformFeedbackTestInstance	(Context& context, const TestParameters& parameters);
protected:
	void											validateLimits					();
	std::vector<VkDeviceSize>						generateSizesList				(const size_t bufBytes, const size_t chunkCount);
	std::vector<VkDeviceSize>						generateOffsetsList				(const std::vector<VkDeviceSize>& sizesList);
	void											verifyTransformFeedbackBuffer	(const MovePtr<Allocation>& bufAlloc,
																					 const deUint32 bufBytes);

	const bool										m_extensions;
	const VkExtent2D								m_imageExtent2D;
	const TestParameters							m_parameters;
	VkPhysicalDeviceTransformFeedbackPropertiesEXT	m_transformFeedbackProperties;
	de::Random										m_rnd;
};

TransformFeedbackTestInstance::TransformFeedbackTestInstance (Context& context, const TestParameters& parameters)
	: TestInstance		(context)
	, m_extensions		(context.requireDeviceFunctionality("VK_EXT_transform_feedback"))
	, m_imageExtent2D	(makeExtent2D(IMAGE_SIZE, IMAGE_SIZE))
	, m_parameters		(parameters)
	, m_rnd				(0)
{
	const VkPhysicalDeviceTransformFeedbackFeaturesEXT&		transformFeedbackFeatures	= m_context.getTransformFeedbackFeaturesEXT();
	VkPhysicalDeviceProperties2								deviceProperties2;

	if (transformFeedbackFeatures.transformFeedback == DE_FALSE)
		TCU_THROW(NotSupportedError, "transformFeedback feature is not supported");

	deMemset(&deviceProperties2, 0, sizeof(deviceProperties2));
	deMemset(&m_transformFeedbackProperties, 0, sizeof(m_transformFeedbackProperties));

	deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	deviceProperties2.pNext = &m_transformFeedbackProperties;

	m_transformFeedbackProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT;
	m_transformFeedbackProperties.pNext = DE_NULL;

	context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &deviceProperties2);

	validateLimits();

	if (m_parameters.streamId > 0 && (m_transformFeedbackProperties.transformFeedbackRasterizationStreamSelect == VK_FALSE))
		TCU_THROW(NotSupportedError, "Implementation doesn't support streamId > 0");
}

void TransformFeedbackTestInstance::validateLimits ()
{
	VALIDATE_MINIMUM(m_transformFeedbackProperties.maxTransformFeedbackBuffers, 1);
	VALIDATE_MINIMUM(m_transformFeedbackProperties.maxTransformFeedbackBufferSize, MINIMUM_TF_BUFFER_SIZE);
	VALIDATE_MINIMUM(m_transformFeedbackProperties.maxTransformFeedbackStreamDataSize, 512);
	VALIDATE_MINIMUM(m_transformFeedbackProperties.maxTransformFeedbackBufferDataSize, 512);
	VALIDATE_MINIMUM(m_transformFeedbackProperties.maxTransformFeedbackBufferDataStride, 512);

	VALIDATE_BOOL(m_transformFeedbackProperties.transformFeedbackQueries);
	VALIDATE_BOOL(m_transformFeedbackProperties.transformFeedbackStreamsLinesTriangles);
	VALIDATE_BOOL(m_transformFeedbackProperties.transformFeedbackRasterizationStreamSelect);
	VALIDATE_BOOL(m_transformFeedbackProperties.transformFeedbackDraw);
}

std::vector<VkDeviceSize> TransformFeedbackTestInstance::generateSizesList (const size_t bufBytes, const size_t chunkCount)
{
	const int					minChunkSlot	= static_cast<int>(1);
	const int					maxChunkSlot	= static_cast<int>(bufBytes / sizeof(deUint32));
	int							prevOffsetSlot	= 0;
	std::map<int, bool>			offsetsSet;
	std::vector<VkDeviceSize>	result;

	DE_ASSERT(bufBytes <= MINIMUM_TF_BUFFER_SIZE);
	DE_ASSERT(bufBytes % sizeof(deUint32) == 0);
	DE_ASSERT(minChunkSlot <= maxChunkSlot);
	DE_ASSERT(chunkCount > 0);
	// To be effective this algorithm requires that chunkCount is much less than amount of chunks possible
	DE_ASSERT(8 * chunkCount <= static_cast<size_t>(maxChunkSlot));

	offsetsSet[0] = true;

	// Create a list of unique offsets first
	for (size_t chunkNdx = 1; chunkNdx < chunkCount; ++chunkNdx)
	{
		int chunkSlot;

		do
		{
			chunkSlot = m_rnd.getInt(minChunkSlot, maxChunkSlot - 1);
		} while (offsetsSet.find(chunkSlot) != offsetsSet.end());

		offsetsSet[chunkSlot] = true;
	}
	offsetsSet[maxChunkSlot] = true;

	// Calculate sizes of offsets list
	result.reserve(chunkCount);
	for (std::map<int, bool>::iterator mapIt = offsetsSet.begin(); mapIt != offsetsSet.end(); ++mapIt)
	{
		const int offsetSlot = mapIt->first;

		if (offsetSlot == 0)
			continue;

		DE_ASSERT(prevOffsetSlot < offsetSlot && offsetSlot > 0);

		result.push_back(static_cast<VkDeviceSize>(static_cast<size_t>(offsetSlot - prevOffsetSlot) * sizeof(deUint32)));

		prevOffsetSlot = offsetSlot;
	}

	DE_ASSERT(result.size() == chunkCount);

	return result;
}

std::vector<VkDeviceSize> TransformFeedbackTestInstance::generateOffsetsList (const std::vector<VkDeviceSize>& sizesList)
{
	VkDeviceSize				offset	= 0ull;
	std::vector<VkDeviceSize>	result;

	result.reserve(sizesList.size());

	for (size_t chunkNdx = 0; chunkNdx < sizesList.size(); ++chunkNdx)
	{
		result.push_back(offset);

		offset += sizesList[chunkNdx];
	}

	DE_ASSERT(sizesList.size() == result.size());

	return result;
}

void TransformFeedbackTestInstance::verifyTransformFeedbackBuffer (const MovePtr<Allocation>& bufAlloc,
																   const deUint32 bufBytes)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();

	invalidateAlloc(vk, device, *bufAlloc);

	const deUint32			numPoints	= static_cast<deUint32>(bufBytes / sizeof(deUint32));
	const deUint32*			tfData		= (deUint32*)bufAlloc->getHostPtr();

	for (deUint32 i = 0; i < numPoints; ++i)
		if (tfData[i] != i)
			TCU_FAIL(std::string("Failed at item ") + de::toString(i) + " received:" + de::toString(tfData[i]) + " expected:" + de::toString(i));
}

class TransformFeedbackBasicTestInstance : public TransformFeedbackTestInstance
{
public:
						TransformFeedbackBasicTestInstance	(Context& context, const TestParameters& parameters);

protected:
	tcu::TestStatus		iterate								(void);
};

TransformFeedbackBasicTestInstance::TransformFeedbackBasicTestInstance (Context& context, const TestParameters& parameters)
	: TransformFeedbackTestInstance	(context, parameters)
{
}

tcu::TestStatus TransformFeedbackBasicTestInstance::iterate (void)
{
	const DeviceInterface&				vk						= m_context.getDeviceInterface();
	const VkDevice						device					= m_context.getDevice();
	const deUint32						queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const VkQueue						queue					= m_context.getUniversalQueue();
	Allocator&							allocator				= m_context.getDefaultAllocator();

	const Unique<VkShaderModule>		vertexModule			(createShaderModule						(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkRenderPass>			renderPass				(makeRenderPass							(vk, device, VK_FORMAT_UNDEFINED));
	const Unique<VkFramebuffer>			framebuffer				(makeFramebuffer						(vk, device, *renderPass, 0u, DE_NULL, m_imageExtent2D.width, m_imageExtent2D.height));
	const Unique<VkPipelineLayout>		pipelineLayout			(TransformFeedback::makePipelineLayout	(vk, device));
	const Unique<VkPipeline>			pipeline				(makeGraphicsPipeline					(vk, device, *pipelineLayout, *renderPass, *vertexModule, DE_NULL, DE_NULL, DE_NULL, DE_NULL, m_imageExtent2D, 0u, &m_parameters.streamId));
	const Unique<VkCommandPool>			cmdPool					(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer				(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkBufferCreateInfo			tfBufCreateInfo			= makeBufferCreateInfo(m_parameters.bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	const Move<VkBuffer>				tfBuf					= createBuffer(vk, device, &tfBufCreateInfo);
	const MovePtr<Allocation>			tfBufAllocation			= allocator.allocate(getBufferMemoryRequirements(vk, device, *tfBuf), MemoryRequirement::HostVisible);
	const VkMemoryBarrier				tfMemoryBarrier			= makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, VK_ACCESS_HOST_READ_BIT);
	const std::vector<VkDeviceSize>		tfBufBindingSizes		= generateSizesList(m_parameters.bufferSize, m_parameters.partCount);
	const std::vector<VkDeviceSize>		tfBufBindingOffsets		= generateOffsetsList(tfBufBindingSizes);

	VK_CHECK(vk.bindBufferMemory(device, *tfBuf, tfBufAllocation->getMemory(), tfBufAllocation->getOffset()));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D));
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			for (deUint32 drawNdx = 0; drawNdx < m_parameters.partCount; ++drawNdx)
			{
				const deUint32	startValue	= static_cast<deUint32>(tfBufBindingOffsets[drawNdx] / sizeof(deUint32));
				const deUint32	numPoints	= static_cast<deUint32>(tfBufBindingSizes[drawNdx] / sizeof(deUint32));

				vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, 0, 1, &*tfBuf, &tfBufBindingOffsets[drawNdx], &tfBufBindingSizes[drawNdx]);

				vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0u, sizeof(startValue), &startValue);

				vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
				{
					vk.cmdDraw(*cmdBuffer, numPoints, 1u, 0u, 0u);
				}
				vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
			}
		}
		endRenderPass(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &tfMemoryBarrier, 0u, DE_NULL, 0u, DE_NULL);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	verifyTransformFeedbackBuffer(tfBufAllocation, m_parameters.bufferSize);

	return tcu::TestStatus::pass("Pass");
}

class TransformFeedbackResumeTestInstance : public TransformFeedbackTestInstance
{
public:
						TransformFeedbackResumeTestInstance	(Context& context, const TestParameters& parameters);

protected:
	tcu::TestStatus		iterate								(void);
};

TransformFeedbackResumeTestInstance::TransformFeedbackResumeTestInstance (Context& context, const TestParameters& parameters)
	: TransformFeedbackTestInstance	(context, parameters)
{
}

tcu::TestStatus TransformFeedbackResumeTestInstance::iterate (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							device					= m_context.getDevice();
	const deUint32							queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const VkQueue							queue					= m_context.getUniversalQueue();
	Allocator&								allocator				= m_context.getDefaultAllocator();

	const Unique<VkShaderModule>			vertexModule			(createShaderModule						(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkRenderPass>				renderPass				(makeRenderPass							(vk, device, VK_FORMAT_UNDEFINED));
	const Unique<VkFramebuffer>				framebuffer				(makeFramebuffer						(vk, device, *renderPass, 0u, DE_NULL, m_imageExtent2D.width, m_imageExtent2D.height));
	const Unique<VkPipelineLayout>			pipelineLayout			(TransformFeedback::makePipelineLayout	(vk, device));
	const Unique<VkPipeline>				pipeline				(makeGraphicsPipeline					(vk, device, *pipelineLayout, *renderPass, *vertexModule, DE_NULL, DE_NULL, DE_NULL, DE_NULL, m_imageExtent2D, 0u, &m_parameters.streamId));

	const Unique<VkCommandPool>				cmdPool					(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>			cmdBuffer				(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkBufferCreateInfo				tfBufCreateInfo			= makeBufferCreateInfo(m_parameters.bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	const Move<VkBuffer>					tfBuf					= createBuffer(vk, device, &tfBufCreateInfo);
	const MovePtr<Allocation>				tfBufAllocation			= allocator.allocate(getBufferMemoryRequirements(vk, device, *tfBuf), MemoryRequirement::HostVisible);
	const VkMemoryBarrier					tfMemoryBarrier			= makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, VK_ACCESS_HOST_READ_BIT);
	const std::vector<VkDeviceSize>			tfBufBindingSizes		= std::vector<VkDeviceSize>(1, m_parameters.bufferSize);
	const std::vector<VkDeviceSize>			tfBufBindingOffsets		= std::vector<VkDeviceSize>(1, 0ull);

	const size_t							tfcBufSize				= 16 * sizeof(deUint32) * m_parameters.partCount;
	const VkBufferCreateInfo				tfcBufCreateInfo		= makeBufferCreateInfo(tfcBufSize, VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT);
	const Move<VkBuffer>					tfcBuf					= createBuffer(vk, device, &tfcBufCreateInfo);
	const MovePtr<Allocation>				tfcBufAllocation		= allocator.allocate(getBufferMemoryRequirements(vk, device, *tfcBuf), MemoryRequirement::Any);
	const std::vector<VkDeviceSize>			tfcBufBindingOffsets	= generateOffsetsList(generateSizesList(tfcBufSize, m_parameters.partCount));
	const VkBufferMemoryBarrier				tfcBufBarrier			= makeBufferMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT, VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT, *tfcBuf, 0ull, VK_WHOLE_SIZE);

	const std::vector<VkDeviceSize>			chunkSizesList			= generateSizesList(m_parameters.bufferSize, m_parameters.partCount);
	const std::vector<VkDeviceSize>			chunkOffsetsList		= generateOffsetsList(chunkSizesList);

	DE_ASSERT(tfBufBindingSizes.size() == 1);
	DE_ASSERT(tfBufBindingOffsets.size() == 1);

	VK_CHECK(vk.bindBufferMemory(device, *tfBuf, tfBufAllocation->getMemory(), tfBufAllocation->getOffset()));
	VK_CHECK(vk.bindBufferMemory(device, *tfcBuf, tfcBufAllocation->getMemory(), tfcBufAllocation->getOffset()));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		for (size_t drawNdx = 0; drawNdx < m_parameters.partCount; ++drawNdx)
		{
			const deUint32	startValue = static_cast<deUint32>(chunkOffsetsList[drawNdx] / sizeof(deUint32));
			const deUint32	numPoints = static_cast<deUint32>(chunkSizesList[drawNdx] / sizeof(deUint32));
			const deUint32	countBuffersCount = (drawNdx == 0) ? 0 : 1;

			beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D));
			{

				vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

				vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, 0, 1, &*tfBuf, &tfBufBindingOffsets[0], &tfBufBindingSizes[0]);

				vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0u, sizeof(startValue), &startValue);

				vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, countBuffersCount, (drawNdx == 0) ? DE_NULL : &*tfcBuf, (drawNdx == 0) ? DE_NULL : &tfcBufBindingOffsets[drawNdx - 1]);
				{
					vk.cmdDraw(*cmdBuffer, numPoints, 1u, 0u, 0u);
				}
				vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 1, &*tfcBuf, &tfcBufBindingOffsets[drawNdx]);
			}
			endRenderPass(vk, *cmdBuffer);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0u, 0u, DE_NULL, 1u, &tfcBufBarrier, 0u, DE_NULL);
		}

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &tfMemoryBarrier, 0u, DE_NULL, 0u, DE_NULL);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	verifyTransformFeedbackBuffer(tfBufAllocation, m_parameters.bufferSize);

	return tcu::TestStatus::pass("Pass");
}

class TransformFeedbackWindingOrderTestInstance : public TransformFeedbackTestInstance
{
public:
	TransformFeedbackWindingOrderTestInstance(Context& context, const TestParameters& parameters);

protected:
	struct TopologyParameters
	{
		// number of vertex in primitive; 2 for line, 3 for triangle
		deUint32 vertexPerPrimitive;

		// pointer to function calculating number of points that
		// will be generated for given part count
		std::function<deUint32(deUint32)> getNumGeneratedPoints;

		// pointer to function generating expected values; parameter is
		// primitive index, result array with expected data for primitive vertex
		std::function<std::vector<deUint32>(deUint32)> getExpectedValuesForPrimitive;
	};
	typedef const std::map<VkPrimitiveTopology, TopologyParameters> TopologyParametersMap;

protected:
	const TopologyParametersMap&	getTopologyParametersMap					(void);
	tcu::TestStatus					iterate										(void);
	void							verifyTransformFeedbackBuffer				(const MovePtr<Allocation>& bufAlloc,
																				 const deUint32 bufBytes);

private:
	TopologyParameters				m_tParameters;
	const bool						m_requiresTesselationStage;
};

TransformFeedbackWindingOrderTestInstance::TransformFeedbackWindingOrderTestInstance(Context& context, const TestParameters& parameters)
	: TransformFeedbackTestInstance	(context, parameters)
	, m_requiresTesselationStage(parameters.primTopology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
{
	if (m_requiresTesselationStage && !context.getDeviceFeatures().tessellationShader)
		throw tcu::NotSupportedError("Tessellation shader not supported");

	TopologyParametersMap topologyParametersMap = getTopologyParametersMap();
	DE_ASSERT(topologyParametersMap.find(parameters.primTopology) != topologyParametersMap.end());
	m_tParameters = topologyParametersMap.at(parameters.primTopology);
}

const TransformFeedbackWindingOrderTestInstance::TopologyParametersMap& TransformFeedbackWindingOrderTestInstance::getTopologyParametersMap(void)
{
	static const TopologyParametersMap topologyParametersMap =
	{
		{
			VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
			{
				1u,
				[](deUint32 partCount)	{	return partCount;	},
				[](deUint32 i)			{	return std::vector<deUint32>{ i, i + 1u };	}
			}
		},
		{
			VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
			{
				2u,
				[](deUint32 partCount)	{	return partCount;	},
				[](deUint32 i)			{	return std::vector<deUint32>{ 2 * i, 2 * i + 1u };	}
			}
		},
		{
			VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
			{
				2u,
				[](deUint32 partCount)	{	return 2u * (partCount - 1);	},
				[](deUint32 i)			{	return std::vector<deUint32>{ i, i + 1u };	}
			}
		},
		{
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			{
				3u,
				[](deUint32 partCount)	{	return partCount;	},
				[](deUint32 i)			{	return std::vector<deUint32>{ 3 * i, 3 * i + 1u, 3 * i + 2u };	}
			}
		},
		{
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
			{
				3u,
				[](deUint32 partCount)	{	return 3u * (partCount - 2);	},
				[](deUint32 i)
				{
					const deUint32	iMod2 = i % 2;
					return std::vector<deUint32>{ i, i + 1 + iMod2, i + 2 - iMod2 };
				}
			}
		},
		{
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
			{
				3u,
				[](deUint32 partCount)	{	return partCount;	},
				[](deUint32 i)			{	return std::vector<deUint32>{ i + 1, i + 2, 0 };	}
			}
		},
		{
			VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,
			{
				2u,
				[](deUint32 partCount)	{	return partCount / 4u;	},		// note: this cant be replaced with partCount / 2 as for partCount=6 we will get 3 instead of 2
				[](deUint32 i)			{	return std::vector<deUint32>{ i + 1u, i + 2u };	}
			}
		},
		{
			VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,
			{
				2u,
				[](deUint32 partCount)	{	return 2u * (partCount - 3u);	},
				[](deUint32 i)			{	return std::vector<deUint32>{ i + 1u, i + 2u };	}
			}
		},
		{
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,
			{
				3u,
				[](deUint32 partCount)	{	return partCount / 2u;	},
				[](deUint32 i)			{	return std::vector<deUint32>{ 6 * i, 6 * i + 2u, 6 * i + 4u	};	}
			}
		},
		{
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY,
			{
				3u,
				[](deUint32 partCount)	{	return 3u * (partCount / 2u - 2u);	},
				[](deUint32 i)
				{
					const bool even = (0 == i % 2);
					if (even)
						return std::vector<deUint32>{ 2 * i + 0, 2 * i + 2, 2 * i + 4 };
					return std::vector<deUint32>{ 2 * i + 0, 2 * i + 4, 2 * i + 2 };
				}
			}
		},
		{
			VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
			{
				9u,
				[](deUint32 partCount)	{	return partCount * 3u;	},
				[](deUint32 i)
				{
					// we cant generate vertex numbers in tesselation evaluation shader;
					// check if patch index is correct for every 9 generated vertex
					return std::vector<deUint32>(9, i);
				}
			}
		}
	};

	return topologyParametersMap;
}

tcu::TestStatus TransformFeedbackWindingOrderTestInstance::iterate (void)
{
	DE_ASSERT(m_parameters.partCount >= 6);

	const DeviceInterface&			vk					= m_context.getDeviceInterface();
	const VkDevice					device				= m_context.getDevice();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkQueue					queue				= m_context.getUniversalQueue();
	Allocator&						allocator			= m_context.getDefaultAllocator();

	const Move<VkShaderModule>		vertexModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	Move<VkShaderModule>			tescModule;
	Move<VkShaderModule>			teseModule;
	if (m_requiresTesselationStage)
	{
		tescModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("tesc"), 0u);
		teseModule = createShaderModule(vk, device, m_context.getBinaryCollection().get("tese"), 0u);
	}

	const Unique<VkRenderPass>		renderPass			(makeRenderPass							(vk, device, VK_FORMAT_UNDEFINED));
	const Unique<VkFramebuffer>		framebuffer			(makeFramebuffer						(vk, device, *renderPass, 0u, DE_NULL, m_imageExtent2D.width, m_imageExtent2D.height));
	const Unique<VkPipelineLayout>	pipelineLayout		(TransformFeedback::makePipelineLayout	(vk, device));
	const Unique<VkPipeline>		pipeline			(makeGraphicsPipeline					(vk, device, *pipelineLayout, *renderPass,
																								 *vertexModule,
																								 m_requiresTesselationStage ? *tescModule : DE_NULL,
																								 m_requiresTesselationStage ? *teseModule : DE_NULL,
																								 DE_NULL,
																								 DE_NULL,
																								 m_imageExtent2D, 0u, DE_NULL, m_parameters.primTopology));
	const Unique<VkCommandPool>		cmdPool				(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer			(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const VkDeviceSize				bufferSize			= m_tParameters.getNumGeneratedPoints	(m_parameters.partCount) * sizeof(deUint32);
	const VkBufferCreateInfo		tfBufCreateInfo		= makeBufferCreateInfo					(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	const Move<VkBuffer>			tfBuf				= createBuffer							(vk, device, &tfBufCreateInfo);
	const MovePtr<Allocation>		tfBufAllocation		= allocator.allocate					(getBufferMemoryRequirements(vk, device, *tfBuf), MemoryRequirement::HostVisible);
	const VkMemoryBarrier			tfMemoryBarrier		= makeMemoryBarrier						(VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, VK_ACCESS_HOST_READ_BIT);
	const VkDeviceSize				tfBufBindingSize	= bufferSize;
	const VkDeviceSize				tfBufBindingOffset	= 0u;
	const deUint32					startValue			= 0u;

	VK_CHECK(vk.bindBufferMemory(device, *tfBuf, tfBufAllocation->getMemory(), tfBufAllocation->getOffset()));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D));
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, 0, 1, &*tfBuf, &tfBufBindingOffset, &tfBufBindingSize);

			vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0u, sizeof(startValue), &startValue);

			vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
			{
				vk.cmdDraw(*cmdBuffer, m_parameters.partCount, 1u, 0u, 0u);
			}
			vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
		}
		endRenderPass(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &tfMemoryBarrier, 0u, DE_NULL, 0u, DE_NULL);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	verifyTransformFeedbackBuffer(tfBufAllocation, static_cast<deUint32>(bufferSize));

	return tcu::TestStatus::pass("Pass");
}

void TransformFeedbackWindingOrderTestInstance::verifyTransformFeedbackBuffer(const MovePtr<Allocation>& bufAlloc,
																			  const deUint32 bufBytes)
{
	const DeviceInterface&	vk		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	invalidateAlloc(vk, device, *bufAlloc);

	const deUint32	numPoints			= static_cast<deUint32>(bufBytes / sizeof(deUint32));
	const deUint32	vertexPerPrimitive	= m_tParameters.vertexPerPrimitive;
	const deUint32	numPrimitives		= numPoints / vertexPerPrimitive;
	const deUint32*	tfData				= (deUint32*)bufAlloc->getHostPtr();

	for (deUint32 primitiveIndex = 0; primitiveIndex < numPrimitives; ++primitiveIndex)
	{
		const deUint32*			tfDataForPrimitive			= &tfData[primitiveIndex * vertexPerPrimitive];
		std::vector<deUint32>	expectedDataForPrimitive	= m_tParameters.getExpectedValuesForPrimitive(primitiveIndex);

		// For multi - vertex primitives, all values for a given vertex are written before writing values for any other vertex.
		// Implementations may write out any vertex within the primitive first, but all subsequent vertices for that primitive
		// must be written out in a consistent winding order
		bool correctWinding = true;
		for (deUint32 combinationIndex = 0; combinationIndex < vertexPerPrimitive; combinationIndex++)
		{
			correctWinding = true;
			for (deUint32 vertexIndex = 0; vertexIndex < vertexPerPrimitive; vertexIndex++)
			{
				correctWinding &= (tfDataForPrimitive[vertexIndex] == expectedDataForPrimitive[(combinationIndex + vertexIndex) % vertexPerPrimitive]);

				// if data for this vertex is not correct then there
				// is no need to check other, go to next combination
				if (!correctWinding)
					break;
			}

			// no need to check other combinations, we found correct one
			if (correctWinding)
				break;
		}

		if (!correctWinding)
		{
			std::stringstream message;
			message << "Failed at primitive " << primitiveIndex << " received: [";
			for (deUint32 vertexIndex = 0; vertexIndex < vertexPerPrimitive; vertexIndex++)
				message << de::toString(tfDataForPrimitive[vertexIndex]) << " ";
			message << "] expected: [";
			for (deUint32 vertexIndex = 0; vertexIndex < vertexPerPrimitive; vertexIndex++)
				message << de::toString(expectedDataForPrimitive[vertexIndex]) << " ";
			message << "]";
			TCU_FAIL(message.str());
		}
	}
}

class TransformFeedbackBuiltinTestInstance : public TransformFeedbackTestInstance
{
public:
						TransformFeedbackBuiltinTestInstance	(Context& context, const TestParameters& parameters);

protected:
	tcu::TestStatus		iterate									(void);
	void				verifyTransformFeedbackBuffer			(const MovePtr<Allocation>& bufAlloc, const VkDeviceSize offset, const deUint32 bufBytes);
};

TransformFeedbackBuiltinTestInstance::TransformFeedbackBuiltinTestInstance (Context& context, const TestParameters& parameters)
	: TransformFeedbackTestInstance	(context, parameters)
{
	const InstanceInterface&		vki			= m_context.getInstanceInterface();
	const VkPhysicalDevice			physDevice	= m_context.getPhysicalDevice();
	const VkPhysicalDeviceFeatures	features	= getPhysicalDeviceFeatures(vki, physDevice);

	const deUint32 tfBuffersSupported	= m_transformFeedbackProperties.maxTransformFeedbackBuffers;
	const deUint32 tfBuffersRequired	= m_parameters.partCount;

	if ((m_parameters.testType == TEST_TYPE_XFB_CLIPDISTANCE || m_parameters.testType == TEST_TYPE_XFB_CLIP_AND_CULL) && !features.shaderClipDistance)
		TCU_THROW(NotSupportedError, std::string("shaderClipDistance feature is not supported"));
	if ((m_parameters.testType == TEST_TYPE_XFB_CULLDISTANCE || m_parameters.testType == TEST_TYPE_XFB_CLIP_AND_CULL) && !features.shaderCullDistance)
		TCU_THROW(NotSupportedError, std::string("shaderCullDistance feature is not supported"));
	if (tfBuffersSupported < tfBuffersRequired)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBuffers=" + de::toString(tfBuffersSupported) + ", while test requires " + de::toString(tfBuffersRequired)).c_str());
}

void TransformFeedbackBuiltinTestInstance::verifyTransformFeedbackBuffer (const MovePtr<Allocation>& bufAlloc, const VkDeviceSize offset, const deUint32 bufBytes)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();

	invalidateAlloc(vk, device, *bufAlloc);

	const deUint32			numPoints	= bufBytes / static_cast<deUint32>(sizeof(float));
	const deUint8*			tfDataBytes	= (deUint8*)bufAlloc->getHostPtr();
	const float*			tfData		= (float*)&tfDataBytes[offset];

	for (deUint32 i = 0; i < numPoints; ++i)
	{
		const deUint32	divisor		= 32768u;
		const float		epsilon		= 1.0f / float(divisor);
		const float		expected	= float(i) / float(divisor);

		if (deAbs(tfData[i] - expected) > epsilon)
			TCU_FAIL(std::string("Failed at item ") + de::toString(i) + " received:" + de::toString(tfData[i]) + " expected:" + de::toString(expected));
	}
}

tcu::TestStatus TransformFeedbackBuiltinTestInstance::iterate (void)
{
	const DeviceInterface&				vk						= m_context.getDeviceInterface();
	const VkDevice						device					= m_context.getDevice();
	const deUint32						queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const VkQueue						queue					= m_context.getUniversalQueue();
	Allocator&							allocator				= m_context.getDefaultAllocator();

	const Unique<VkShaderModule>		vertexModule			(createShaderModule						(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkRenderPass>			renderPass				(makeRenderPass							(vk, device, VK_FORMAT_UNDEFINED));
	const Unique<VkFramebuffer>			framebuffer				(makeFramebuffer						(vk, device, *renderPass, 0u, DE_NULL, m_imageExtent2D.width, m_imageExtent2D.height));
	const Unique<VkPipelineLayout>		pipelineLayout			(TransformFeedback::makePipelineLayout	(vk, device));
	const Unique<VkPipeline>			pipeline				(makeGraphicsPipeline					(vk, device, *pipelineLayout, *renderPass, *vertexModule, DE_NULL, DE_NULL, DE_NULL, DE_NULL, m_imageExtent2D, 0u, &m_parameters.streamId));
	const Unique<VkCommandPool>			cmdPool					(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer				(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkDeviceSize					tfBufSize				= m_parameters.bufferSize * m_parameters.partCount;
	const VkBufferCreateInfo			tfBufCreateInfo			= makeBufferCreateInfo(tfBufSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	const Move<VkBuffer>				tfBuf					= createBuffer(vk, device, &tfBufCreateInfo);
	const std::vector<VkBuffer>			tfBufArray				= std::vector<VkBuffer>(m_parameters.partCount, *tfBuf);
	const MovePtr<Allocation>			tfBufAllocation			= allocator.allocate(getBufferMemoryRequirements(vk, device, *tfBuf), MemoryRequirement::HostVisible);
	const VkMemoryBarrier				tfMemoryBarrier			= makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, VK_ACCESS_HOST_READ_BIT);
	const std::vector<VkDeviceSize>		tfBufBindingSizes		= std::vector<VkDeviceSize>(m_parameters.partCount, m_parameters.bufferSize);
	const std::vector<VkDeviceSize>		tfBufBindingOffsets		= generateOffsetsList(tfBufBindingSizes);
	const deUint32						perVertexDataSize		= (m_parameters.testType == TEST_TYPE_XFB_POINTSIZE)    ? static_cast<deUint32>(sizeof(float))
																: (m_parameters.testType == TEST_TYPE_XFB_CLIPDISTANCE) ? static_cast<deUint32>(8u * sizeof(float))
																: (m_parameters.testType == TEST_TYPE_XFB_CULLDISTANCE) ? static_cast<deUint32>(8u * sizeof(float))
																: (m_parameters.testType == TEST_TYPE_XFB_CLIP_AND_CULL) ? static_cast<deUint32>(6u * sizeof(float))
																: 0u;
	const deUint32						numPoints				= m_parameters.bufferSize / perVertexDataSize;

	VK_CHECK(vk.bindBufferMemory(device, *tfBuf, tfBufAllocation->getMemory(), tfBufAllocation->getOffset()));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D));
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, 0, m_parameters.partCount, &tfBufArray[0], &tfBufBindingOffsets[0], &tfBufBindingSizes[0]);

			vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
			{
				vk.cmdDraw(*cmdBuffer, numPoints, 1u, 0u, 0u);
			}
			vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
		}
		endRenderPass(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &tfMemoryBarrier, 0u, DE_NULL, 0u, DE_NULL);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	verifyTransformFeedbackBuffer(tfBufAllocation, tfBufBindingOffsets[m_parameters.partCount - 1], numPoints * perVertexDataSize);

	return tcu::TestStatus::pass("Pass");
}

class TransformFeedbackMultistreamTestInstance : public TransformFeedbackTestInstance
{
public:
								TransformFeedbackMultistreamTestInstance	(Context& context, const TestParameters& parameters);

protected:
	std::vector<VkDeviceSize>	generateSizesList							(const size_t bufBytes, const size_t chunkCount);
	void						verifyTransformFeedbackBuffer				(const MovePtr<Allocation>& bufAlloc, const deUint32 bufBytes);
	tcu::TestStatus				iterate										(void);
};

TransformFeedbackMultistreamTestInstance::TransformFeedbackMultistreamTestInstance (Context& context, const TestParameters& parameters)
	: TransformFeedbackTestInstance	(context, parameters)
{
	const InstanceInterface&								vki							= m_context.getInstanceInterface();
	const VkPhysicalDevice									physDevice					= m_context.getPhysicalDevice();
	const VkPhysicalDeviceFeatures							features					= getPhysicalDeviceFeatures(vki, physDevice);
	const VkPhysicalDeviceTransformFeedbackFeaturesEXT&		transformFeedbackFeatures	= m_context.getTransformFeedbackFeaturesEXT();
	const deUint32											streamsSupported			= m_transformFeedbackProperties.maxTransformFeedbackStreams;
	const deUint32											streamsRequired				= m_parameters.streamId + 1;
	const deUint32											tfBuffersSupported			= m_transformFeedbackProperties.maxTransformFeedbackBuffers;
	const deUint32											tfBuffersRequired			= m_parameters.partCount;
	const deUint32											bytesPerVertex				= m_parameters.bufferSize / m_parameters.partCount;
	const deUint32											tfStreamDataSizeSupported	= m_transformFeedbackProperties.maxTransformFeedbackStreamDataSize;
	const deUint32											tfBufferDataSizeSupported	= m_transformFeedbackProperties.maxTransformFeedbackBufferDataSize;
	const deUint32											tfBufferDataStrideSupported	= m_transformFeedbackProperties.maxTransformFeedbackBufferDataStride;

	DE_ASSERT(m_parameters.partCount == 2u);

	if (!features.geometryShader)
		TCU_THROW(NotSupportedError, "Missing feature: geometryShader");

	if (transformFeedbackFeatures.geometryStreams == DE_FALSE)
		TCU_THROW(NotSupportedError, "geometryStreams feature is not supported");

	if (streamsSupported < streamsRequired)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackStreams=" + de::toString(streamsSupported) + ", while test requires " + de::toString(streamsRequired)).c_str());

	if (tfBuffersSupported < tfBuffersRequired)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBuffers=" + de::toString(tfBuffersSupported) + ", while test requires " + de::toString(tfBuffersRequired)).c_str());

	if (tfStreamDataSizeSupported < bytesPerVertex)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackStreamDataSize=" + de::toString(tfStreamDataSizeSupported) + ", while test requires " + de::toString(bytesPerVertex)).c_str());

	if (tfBufferDataSizeSupported < bytesPerVertex)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBufferDataSize=" + de::toString(tfBufferDataSizeSupported) + ", while test requires " + de::toString(bytesPerVertex)).c_str());

	if (tfBufferDataStrideSupported < bytesPerVertex)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBufferDataStride=" + de::toString(tfBufferDataStrideSupported) + ", while test requires " + de::toString(bytesPerVertex)).c_str());
}

std::vector<VkDeviceSize> TransformFeedbackMultistreamTestInstance::generateSizesList (const size_t bufBytes, const size_t chunkCount)
{
	const VkDeviceSize			chunkSize	= bufBytes / chunkCount;
	std::vector<VkDeviceSize>	result		(chunkCount, chunkSize);

	DE_ASSERT(chunkSize * chunkCount == bufBytes);
	DE_ASSERT(bufBytes <= MINIMUM_TF_BUFFER_SIZE);
	DE_ASSERT(bufBytes % sizeof(deUint32) == 0);
	DE_ASSERT(chunkCount > 0);
	DE_ASSERT(result.size() == chunkCount);

	return result;
}

void TransformFeedbackMultistreamTestInstance::verifyTransformFeedbackBuffer (const MovePtr<Allocation>& bufAlloc, const deUint32 bufBytes)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();

	invalidateAlloc(vk, device, *bufAlloc);

	const deUint32			numPoints	= static_cast<deUint32>(bufBytes / sizeof(deUint32));
	const float*			tfData		= (float*)bufAlloc->getHostPtr();

	for (deUint32 i = 0; i < numPoints; ++i)
		if (tfData[i] != float(i))
			TCU_FAIL(std::string("Failed at item ") + de::toString(float(i)) + " received:" + de::toString(tfData[i]) + " expected:" + de::toString(i));
}

tcu::TestStatus TransformFeedbackMultistreamTestInstance::iterate (void)
{
	const DeviceInterface&				vk						= m_context.getDeviceInterface();
	const VkDevice						device					= m_context.getDevice();
	const deUint32						queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const VkQueue						queue					= m_context.getUniversalQueue();
	Allocator&							allocator				= m_context.getDefaultAllocator();

	const Unique<VkRenderPass>			renderPass				(makeRenderPass							(vk, device, VK_FORMAT_UNDEFINED));

	const Unique<VkShaderModule>		vertexModule			(createShaderModule						(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>		geomModule				(createShaderModule						(vk, device, m_context.getBinaryCollection().get("geom"), 0u));

	const Unique<VkFramebuffer>			framebuffer				(makeFramebuffer						(vk, device, *renderPass, 0u, DE_NULL, m_imageExtent2D.width, m_imageExtent2D.height));
	const Unique<VkPipelineLayout>		pipelineLayout			(TransformFeedback::makePipelineLayout	(vk, device));
	const Unique<VkPipeline>			pipeline				(makeGraphicsPipeline					(vk, device, *pipelineLayout, *renderPass, *vertexModule, DE_NULL, DE_NULL, *geomModule, DE_NULL, m_imageExtent2D, 0u, &m_parameters.streamId));
	const Unique<VkCommandPool>			cmdPool					(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer				(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkBufferCreateInfo			tfBufCreateInfo			= makeBufferCreateInfo(m_parameters.bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	const Move<VkBuffer>				tfBuf					= createBuffer(vk, device, &tfBufCreateInfo);
	const std::vector<VkBuffer>			tfBufArray				= std::vector<VkBuffer>(m_parameters.partCount, *tfBuf);
	const MovePtr<Allocation>			tfBufAllocation			= allocator.allocate(getBufferMemoryRequirements(vk, device, *tfBuf), MemoryRequirement::HostVisible);
	const VkMemoryBarrier				tfMemoryBarrier			= makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, VK_ACCESS_HOST_READ_BIT);
	const std::vector<VkDeviceSize>		tfBufBindingSizes		= generateSizesList(m_parameters.bufferSize, m_parameters.partCount);
	const std::vector<VkDeviceSize>		tfBufBindingOffsets		= generateOffsetsList(tfBufBindingSizes);

	VK_CHECK(vk.bindBufferMemory(device, *tfBuf, tfBufAllocation->getMemory(), tfBufAllocation->getOffset()));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D));
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, 0u, m_parameters.partCount, &tfBufArray[0], &tfBufBindingOffsets[0], &tfBufBindingSizes[0]);

			vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
			{
				vk.cmdDraw(*cmdBuffer, 1u, 1u, 0u, 0u);
			}
			vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
		}
		endRenderPass(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &tfMemoryBarrier, 0u, DE_NULL, 0u, DE_NULL);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	verifyTransformFeedbackBuffer(tfBufAllocation, m_parameters.bufferSize);

	return tcu::TestStatus::pass("Pass");
}

class TransformFeedbackStreamsTestInstance : public TransformFeedbackTestInstance
{
public:
						TransformFeedbackStreamsTestInstance	(Context& context, const TestParameters& parameters);

protected:
	tcu::TestStatus		iterate									(void);
	bool				verifyImage								(const VkFormat imageFormat, const VkExtent2D& size, const void* resultData);
};

TransformFeedbackStreamsTestInstance::TransformFeedbackStreamsTestInstance (Context& context, const TestParameters& parameters)
	: TransformFeedbackTestInstance	(context, parameters)
{
	const InstanceInterface&								vki							= m_context.getInstanceInterface();
	const VkPhysicalDevice									physDevice					= m_context.getPhysicalDevice();
	const VkPhysicalDeviceFeatures							features					= getPhysicalDeviceFeatures(vki, physDevice);
	const VkPhysicalDeviceTransformFeedbackFeaturesEXT&		transformFeedbackFeatures	= m_context.getTransformFeedbackFeaturesEXT();
	const deUint32											streamsSupported			= m_transformFeedbackProperties.maxTransformFeedbackStreams;
	const deUint32											streamsRequired				= m_parameters.streamId + 1;
	const bool												geomPointSizeRequired		= m_parameters.testType == TEST_TYPE_STREAMS_POINTSIZE;

	if (!features.geometryShader)
		TCU_THROW(NotSupportedError, "Missing feature: geometryShader");

	if (transformFeedbackFeatures.geometryStreams == DE_FALSE)
		TCU_THROW(NotSupportedError, "geometryStreams feature is not supported");

	if (m_transformFeedbackProperties.transformFeedbackRasterizationStreamSelect == DE_FALSE)
		TCU_THROW(NotSupportedError, "transformFeedbackRasterizationStreamSelect feature is not supported");

	if (streamsSupported < streamsRequired)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackStreams=" + de::toString(streamsSupported) + ", while test requires " + de::toString(streamsRequired)).c_str());

	if (geomPointSizeRequired && !features.shaderTessellationAndGeometryPointSize)
		TCU_THROW(NotSupportedError, "shaderTessellationAndGeometryPointSize feature is not supported");
}

bool TransformFeedbackStreamsTestInstance::verifyImage (const VkFormat imageFormat, const VkExtent2D& size, const void* resultData)
{
	const tcu::RGBA				magentaRGBA		(tcu::RGBA(0xFF, 0x00, 0xFF, 0xFF));
	const tcu::Vec4				magenta			(magentaRGBA.toVec());
	const tcu::Vec4				black			(tcu::RGBA::black().toVec());
	const tcu::TextureFormat	textureFormat	(mapVkFormat(imageFormat));
	const int					dataSize		(size.width * size.height * textureFormat.getPixelSize());
	tcu::TextureLevel			referenceImage	(textureFormat, size.width, size.height);
	tcu::PixelBufferAccess		referenceAccess	(referenceImage.getAccess());

	// Generate reference image
	if (m_parameters.testType == TEST_TYPE_STREAMS)
	{
		for (int y = 0; y < referenceImage.getHeight(); ++y)
		{
			const tcu::Vec4&	validColor = y < referenceImage.getHeight() / 2 ? black : magenta;

			for (int x = 0; x < referenceImage.getWidth(); ++x)
				referenceAccess.setPixel(validColor, x, y);
		}
	}

	if (m_parameters.testType == TEST_TYPE_STREAMS_CLIPDISTANCE || m_parameters.testType == TEST_TYPE_STREAMS_CULLDISTANCE)
	{
		for (int y = 0; y < referenceImage.getHeight(); ++y)
			for (int x = 0; x < referenceImage.getWidth(); ++x)
			{
				const tcu::Vec4&	validColor	= (y >= referenceImage.getHeight() / 2) && (x >= referenceImage.getWidth() / 2) ? magenta : black;

				referenceAccess.setPixel(validColor, x, y);
			}
	}

	if (m_parameters.testType == TEST_TYPE_STREAMS_POINTSIZE)
	{
		const int			pointSize	= static_cast<int>(m_parameters.pointSize);
		const tcu::Vec4&	validColor	= black;

		for (int y = 0; y < referenceImage.getHeight(); ++y)
			for (int x = 0; x < referenceImage.getWidth(); ++x)
				referenceAccess.setPixel(validColor, x, y);

		referenceAccess.setPixel(magenta, (1 + referenceImage.getWidth()) / 4 - 1, (referenceImage.getHeight() * 3) / 4 - 1);

		for (int y = 0; y < pointSize; ++y)
			for (int x = 0; x < pointSize; ++x)
				referenceAccess.setPixel(magenta, x + (referenceImage.getWidth() * 3) / 4 - 1, y + (referenceImage.getHeight() * 3) / 4 - 1);
	}

	if (deMemCmp(resultData, referenceAccess.getDataPtr(), dataSize) != 0)
	{
		const tcu::ConstPixelBufferAccess	resultImage	(textureFormat, size.width, size.height, 1, resultData);
		bool								ok;

		ok = tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Image comparison", "", referenceAccess, resultImage, tcu::UVec4(1), tcu::COMPARE_LOG_RESULT);

		return ok;
	}

	return true;
}

tcu::TestStatus TransformFeedbackStreamsTestInstance::iterate (void)
{
	const DeviceInterface&				vk					= m_context.getDeviceInterface();
	const VkDevice						device				= m_context.getDevice();
	const deUint32						queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkQueue						queue				= m_context.getUniversalQueue();
	Allocator&							allocator			= m_context.getDefaultAllocator();

	const Unique<VkRenderPass>			renderPass			(makeRenderPass			(vk, device, VK_FORMAT_R8G8B8A8_UNORM));

	const Unique<VkShaderModule>		vertModule			(createShaderModule		(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>		geomModule			(createShaderModule		(vk, device, m_context.getBinaryCollection().get("geom"), 0u));
	const Unique<VkShaderModule>		fragModule			(createShaderModule		(vk, device, m_context.getBinaryCollection().get("frag"), 0u));

	const VkFormat						colorFormat			= VK_FORMAT_R8G8B8A8_UNORM;
	const VkImageUsageFlags				imageUsageFlags		= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	const tcu::RGBA						clearColor			(tcu::RGBA::black());
	const VkImageSubresourceRange		colorSubresRange	(makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));
	const VkDeviceSize					colorBufferSize		(m_imageExtent2D.width * m_imageExtent2D.height * tcu::getPixelSize(mapVkFormat(colorFormat)));
	const Unique<VkImage>				colorImage			(makeImage								(vk, device, makeImageCreateInfo(0u, VK_IMAGE_TYPE_2D, colorFormat, m_imageExtent2D, 1u, imageUsageFlags)));
	const UniquePtr<Allocation>			colorImageAlloc		(bindImage								(vk, device, allocator, *colorImage, MemoryRequirement::Any));
	const Unique<VkImageView>			colorAttachment		(makeImageView							(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresRange));
	const Unique<VkBuffer>				colorBuffer			(makeBuffer								(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>			colorBufferAlloc	(bindBuffer								(vk, device, allocator, *colorBuffer, MemoryRequirement::HostVisible));

	const Unique<VkFramebuffer>			framebuffer			(makeFramebuffer						(vk, device, *renderPass, *colorAttachment, m_imageExtent2D.width, m_imageExtent2D.height));
	const Unique<VkPipelineLayout>		pipelineLayout		(TransformFeedback::makePipelineLayout	(vk, device));
	const Unique<VkPipeline>			pipeline			(makeGraphicsPipeline					(vk, device, *pipelineLayout, *renderPass, *vertModule, DE_NULL, DE_NULL, *geomModule, *fragModule, m_imageExtent2D, 0u, &m_parameters.streamId));
	const Unique<VkCommandPool>			cmdPool				(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer			(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkImageMemoryBarrier			preCopyBarrier		= makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
																					 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
																					 *colorImage, colorSubresRange);
	const VkBufferImageCopy				region				= makeBufferImageCopy(makeExtent3D(m_imageExtent2D.width, m_imageExtent2D.height, 1u),
																				  makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
	const VkBufferMemoryBarrier			postCopyBarrier		= makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *colorBuffer, 0ull, VK_WHOLE_SIZE);

	beginCommandBuffer(vk, *cmdBuffer);
	{
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D), clearColor.toVec());
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			vk.cmdDraw(*cmdBuffer, 2u, 1u, 0u, 0u);
		}
		endRenderPass(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preCopyBarrier);
		vk.cmdCopyImageToBuffer(*cmdBuffer, *colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorBuffer, 1u, &region);
		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &postCopyBarrier, DE_NULL, 0u);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	if (!verifyImage(colorFormat, m_imageExtent2D, colorBufferAlloc->getHostPtr()))
		return tcu::TestStatus::fail("Fail");

	return tcu::TestStatus::pass("Pass");
}

class TransformFeedbackIndirectDrawTestInstance : public TransformFeedbackTestInstance
{
public:
						TransformFeedbackIndirectDrawTestInstance	(Context& context, const TestParameters& parameters);

protected:
	tcu::TestStatus		iterate										(void);
	bool				verifyImage									(const VkFormat imageFormat, const VkExtent2D& size, const void* resultData);
};

TransformFeedbackIndirectDrawTestInstance::TransformFeedbackIndirectDrawTestInstance (Context& context, const TestParameters& parameters)
	: TransformFeedbackTestInstance	(context, parameters)
{
	const InstanceInterface&		vki							= m_context.getInstanceInterface();
	const VkPhysicalDevice			physDevice					= m_context.getPhysicalDevice();
	const VkPhysicalDeviceLimits	limits						= getPhysicalDeviceProperties(vki, physDevice).limits;
	const deUint32					tfBufferDataSizeSupported	= m_transformFeedbackProperties.maxTransformFeedbackBufferDataSize;
	const deUint32					tfBufferDataStrideSupported	= m_transformFeedbackProperties.maxTransformFeedbackBufferDataStride;

	if (m_transformFeedbackProperties.transformFeedbackDraw == DE_FALSE)
		TCU_THROW(NotSupportedError, "transformFeedbackDraw feature is not supported");

	if (limits.maxVertexInputBindingStride < m_parameters.vertexStride)
		TCU_THROW(NotSupportedError, std::string("maxVertexInputBindingStride=" + de::toString(limits.maxVertexInputBindingStride) + ", while test requires " + de::toString(m_parameters.vertexStride)).c_str());

	if (tfBufferDataSizeSupported < m_parameters.vertexStride)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBufferDataSize=" + de::toString(tfBufferDataSizeSupported) + ", while test requires " + de::toString(m_parameters.vertexStride)).c_str());

	if (tfBufferDataStrideSupported < m_parameters.vertexStride)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBufferDataStride=" + de::toString(tfBufferDataStrideSupported) + ", while test requires " + de::toString(m_parameters.vertexStride)).c_str());
}

bool TransformFeedbackIndirectDrawTestInstance::verifyImage (const VkFormat imageFormat, const VkExtent2D& size, const void* resultData)
{
	const tcu::Vec4				white			(tcu::RGBA::white().toVec());
	const tcu::TextureFormat	textureFormat	(mapVkFormat(imageFormat));
	const int					dataSize		(size.width * size.height * textureFormat.getPixelSize());
	tcu::TextureLevel			referenceImage	(textureFormat, size.width, size.height);
	tcu::PixelBufferAccess		referenceAccess	(referenceImage.getAccess());

	// Generate reference image
	for (int y = 0; y < referenceImage.getHeight(); ++y)
		for (int x = 0; x < referenceImage.getWidth(); ++x)
			referenceAccess.setPixel(white, x, y);

	if (deMemCmp(resultData, referenceAccess.getDataPtr(), dataSize) != 0)
	{
		const tcu::ConstPixelBufferAccess	resultImage	(textureFormat, size.width, size.height, 1, resultData);
		bool								ok;

		ok = tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Image comparison", "", referenceAccess, resultImage, tcu::UVec4(1), tcu::COMPARE_LOG_RESULT);

		return ok;
	}

	return true;
}

tcu::TestStatus TransformFeedbackIndirectDrawTestInstance::iterate (void)
{
	const DeviceInterface&				vk					= m_context.getDeviceInterface();
	const VkDevice						device				= m_context.getDevice();
	const deUint32						queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkQueue						queue				= m_context.getUniversalQueue();
	Allocator&							allocator			= m_context.getDefaultAllocator();

	const Unique<VkRenderPass>			renderPass			(makeRenderPass			(vk, device, VK_FORMAT_R8G8B8A8_UNORM));

	const Unique<VkShaderModule>		vertModule			(createShaderModule		(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>		fragModule			(createShaderModule		(vk, device, m_context.getBinaryCollection().get("frag"), 0u));

	const VkFormat						colorFormat			= VK_FORMAT_R8G8B8A8_UNORM;
	const VkImageUsageFlags				imageUsageFlags		= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	const tcu::RGBA						clearColor			(tcu::RGBA::black());
	const VkImageSubresourceRange		colorSubresRange	(makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));
	const VkDeviceSize					colorBufferSize		(m_imageExtent2D.width * m_imageExtent2D.height * tcu::getPixelSize(mapVkFormat(colorFormat)));
	const Unique<VkImage>				colorImage			(makeImage				(vk, device, makeImageCreateInfo(0u, VK_IMAGE_TYPE_2D, colorFormat, m_imageExtent2D, 1u, imageUsageFlags)));
	const UniquePtr<Allocation>			colorImageAlloc		(bindImage				(vk, device, allocator, *colorImage, MemoryRequirement::Any));
	const Unique<VkImageView>			colorAttachment		(makeImageView			(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresRange));
	const Unique<VkBuffer>				colorBuffer			(makeBuffer				(vk, device, colorBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>			colorBufferAlloc	(bindBuffer				(vk, device, allocator, *colorBuffer, MemoryRequirement::HostVisible));

	const deUint32						vertexCount			= 6u;
	const VkDeviceSize					vertexBufferSize	= vertexCount * m_parameters.vertexStride;
	const VkBufferUsageFlags			vertexBufferUsage	= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const Unique<VkBuffer>				vertexBuffer		(makeBuffer				(vk, device, vertexBufferSize, vertexBufferUsage));
	const UniquePtr<Allocation>			vertexBufferAlloc	(bindBuffer				(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible));
	const VkDeviceSize					vertexBufferOffset	(0u);
	const float							vertexBufferVals[]	=
																{
																	-1.0f, -1.0f, 0.0f, 1.0f,
																	-1.0f, +1.0f, 0.0f, 1.0f,
																	+1.0f, -1.0f, 0.0f, 1.0f,
																	-1.0f, +1.0f, 0.0f, 1.0f,
																	+1.0f, -1.0f, 0.0f, 1.0f,
																	+1.0f, +1.0f, 0.0f, 1.0f,
																};

	const deUint32						counterBufferValue	= m_parameters.vertexStride * vertexCount;
	const VkDeviceSize					counterBufferSize	= sizeof(counterBufferValue);
	const VkBufferUsageFlags			counterBufferUsage	= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const Unique<VkBuffer>				counterBuffer		(makeBuffer								(vk, device, counterBufferSize, counterBufferUsage));
	const UniquePtr<Allocation>			counterBufferAlloc	(bindBuffer								(vk, device, allocator, *counterBuffer, MemoryRequirement::HostVisible));

	const Unique<VkFramebuffer>			framebuffer			(makeFramebuffer						(vk, device, *renderPass, *colorAttachment, m_imageExtent2D.width, m_imageExtent2D.height));
	const Unique<VkPipelineLayout>		pipelineLayout		(TransformFeedback::makePipelineLayout	(vk, device));
	const Unique<VkPipeline>			pipeline			(makeGraphicsPipeline					(vk, device, *pipelineLayout, *renderPass, *vertModule, DE_NULL, DE_NULL, DE_NULL, *fragModule, m_imageExtent2D, 0u, DE_NULL, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, true));
	const Unique<VkCommandPool>			cmdPool				(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer			(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkImageMemoryBarrier			preCopyBarrier		= makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
																					 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
																					 *colorImage, colorSubresRange);
	const VkBufferImageCopy				region				= makeBufferImageCopy(makeExtent3D(m_imageExtent2D.width, m_imageExtent2D.height, 1u),
																				  makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
	const VkBufferMemoryBarrier			postCopyBarrier		= makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *colorBuffer, 0ull, VK_WHOLE_SIZE);

	fillBuffer(vk, device, *counterBufferAlloc, counterBufferSize, &counterBufferValue, counterBufferSize);
	fillBuffer(vk, device, *vertexBufferAlloc, vertexBufferSize, vertexBufferVals, sizeof(vertexBufferVals));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D), clearColor.toVec());
		{
			vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &*vertexBuffer, &vertexBufferOffset);

			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			vk.cmdDrawIndirectByteCountEXT(*cmdBuffer, 1u, 0u, *counterBuffer, 0u, 0u, m_parameters.vertexStride);
		}
		endRenderPass(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &preCopyBarrier);
		vk.cmdCopyImageToBuffer(*cmdBuffer, *colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *colorBuffer, 1u, &region);
		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &postCopyBarrier, DE_NULL, 0u);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	if (!verifyImage(colorFormat, m_imageExtent2D, colorBufferAlloc->getHostPtr()))
		return tcu::TestStatus::fail("Fail");

	return tcu::TestStatus::pass("Pass");
}

class TransformFeedbackBackwardDependencyTestInstance : public TransformFeedbackTestInstance
{
public:
								TransformFeedbackBackwardDependencyTestInstance	(Context& context, const TestParameters& parameters);

protected:
	tcu::TestStatus				iterate											(void);
	std::vector<VkDeviceSize>	generateSizesList								(const size_t bufBytes, const size_t chunkCount);
};

TransformFeedbackBackwardDependencyTestInstance::TransformFeedbackBackwardDependencyTestInstance (Context& context, const TestParameters& parameters)
	: TransformFeedbackTestInstance	(context, parameters)
{
	if (m_transformFeedbackProperties.transformFeedbackDraw == DE_FALSE)
		TCU_THROW(NotSupportedError, "transformFeedbackDraw feature is not supported");
}

std::vector<VkDeviceSize> TransformFeedbackBackwardDependencyTestInstance::generateSizesList (const size_t bufBytes, const size_t chunkCount)
{
	const VkDeviceSize			chunkSize	= bufBytes / chunkCount;
	std::vector<VkDeviceSize>	result		(chunkCount, chunkSize);

	DE_ASSERT(chunkSize * chunkCount == bufBytes);
	DE_ASSERT(bufBytes <= MINIMUM_TF_BUFFER_SIZE);
	DE_ASSERT(bufBytes % sizeof(deUint32) == 0);
	DE_ASSERT(chunkCount > 0);
	DE_ASSERT(result.size() == chunkCount);

	return result;
}

tcu::TestStatus TransformFeedbackBackwardDependencyTestInstance::iterate (void)
{
	const DeviceInterface&				vk					= m_context.getDeviceInterface();
	const VkDevice						device				= m_context.getDevice();
	const deUint32						queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const VkQueue						queue				= m_context.getUniversalQueue();
	Allocator&							allocator			= m_context.getDefaultAllocator();

	const Unique<VkShaderModule>		vertexModule		(createShaderModule						(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkRenderPass>			renderPass			(TransformFeedback::makeRenderPass		(vk, device));
	const Unique<VkFramebuffer>			framebuffer			(makeFramebuffer						(vk, device, *renderPass, 0u, DE_NULL, m_imageExtent2D.width, m_imageExtent2D.height));
	const Unique<VkPipelineLayout>		pipelineLayout		(TransformFeedback::makePipelineLayout	(vk, device));
	const Unique<VkPipeline>			pipeline			(makeGraphicsPipeline					(vk, device, *pipelineLayout, *renderPass, *vertexModule, DE_NULL, DE_NULL, DE_NULL, DE_NULL, m_imageExtent2D, 0u));
	const Unique<VkCommandPool>			cmdPool				(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer			(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkBufferCreateInfo			tfBufCreateInfo		= makeBufferCreateInfo(m_parameters.bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	const Move<VkBuffer>				tfBuf				= createBuffer(vk, device, &tfBufCreateInfo);
	const MovePtr<Allocation>			tfBufAllocation		= allocator.allocate(getBufferMemoryRequirements(vk, device, *tfBuf), MemoryRequirement::HostVisible);
	const VkMemoryBarrier				tfMemoryBarrier		= makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, VK_ACCESS_HOST_READ_BIT);
	const VkDeviceSize					tfBufBindingSize	= m_parameters.bufferSize;
	const VkDeviceSize					tfBufBindingOffset	= 0ull;

	const size_t						tfcBufSize			= sizeof(deUint32);
	const VkBufferCreateInfo			tfcBufCreateInfo	= makeBufferCreateInfo(tfcBufSize, VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
	const Move<VkBuffer>				tfcBuf				= createBuffer(vk, device, &tfcBufCreateInfo);
	const MovePtr<Allocation>			tfcBufAllocation	= allocator.allocate(getBufferMemoryRequirements(vk, device, *tfcBuf), MemoryRequirement::Any);
	const VkDeviceSize					tfcBufBindingOffset	= 0ull;
	const VkMemoryBarrier				tfcMemoryBarrier	= makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT, VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT);

	const std::vector<VkDeviceSize>		chunkSizesList		= generateSizesList(m_parameters.bufferSize, m_parameters.partCount);
	const std::vector<VkDeviceSize>		chunkOffsetsList	= generateOffsetsList(chunkSizesList);

	VK_CHECK(vk.bindBufferMemory(device, *tfBuf, tfBufAllocation->getMemory(), tfBufAllocation->getOffset()));
	VK_CHECK(vk.bindBufferMemory(device, *tfcBuf, tfcBufAllocation->getMemory(), tfcBufAllocation->getOffset()));

	DE_ASSERT(m_parameters.partCount == 2u);

	beginCommandBuffer(vk, *cmdBuffer);
	{
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D));
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, 0, 1, &*tfBuf, &tfBufBindingOffset, &tfBufBindingSize);

			{
				const deUint32	startValue	= static_cast<deUint32>(chunkOffsetsList[0] / sizeof(deUint32));
				const deUint32	numPoints	= static_cast<deUint32>(chunkSizesList[0] / sizeof(deUint32));

				vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0u, sizeof(startValue), &startValue);

				vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
				{
					vk.cmdDraw(*cmdBuffer, numPoints, 1u, 0u, 0u);
				}
				vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 1, &*tfcBuf, m_parameters.noOffsetArray ? DE_NULL : &tfcBufBindingOffset);
			}

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0u, 1u, &tfcMemoryBarrier, 0u, DE_NULL, DE_NULL, 0u);

			{
				const deUint32	startValue	= static_cast<deUint32>(chunkOffsetsList[1] / sizeof(deUint32));

				vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0u, sizeof(startValue), &startValue);

				vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, 1, &*tfcBuf, m_parameters.noOffsetArray ? DE_NULL : &tfcBufBindingOffset);
				{
					vk.cmdDrawIndirectByteCountEXT(*cmdBuffer, 1u, 0u, *tfcBuf, 0u, 0u, 4u);
				}
				vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
			}

		}
		endRenderPass(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &tfMemoryBarrier, 0u, DE_NULL, 0u, DE_NULL);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	verifyTransformFeedbackBuffer(tfBufAllocation, m_parameters.bufferSize);

	return tcu::TestStatus::pass("Pass");
}


class TransformFeedbackQueryTestInstance : public TransformFeedbackTestInstance
{
public:
						TransformFeedbackQueryTestInstance	(Context& context, const TestParameters& parameters);

protected:
	tcu::TestStatus		iterate								(void);
};

TransformFeedbackQueryTestInstance::TransformFeedbackQueryTestInstance (Context& context, const TestParameters& parameters)
	: TransformFeedbackTestInstance	(context, parameters)
{
	const InstanceInterface&								vki							= m_context.getInstanceInterface();
	const VkPhysicalDevice									physDevice					= m_context.getPhysicalDevice();
	const VkPhysicalDeviceFeatures							features					= getPhysicalDeviceFeatures(vki, physDevice);
	const VkPhysicalDeviceTransformFeedbackFeaturesEXT&		transformFeedbackFeatures	= m_context.getTransformFeedbackFeaturesEXT();
	const deUint32											streamsSupported			= m_transformFeedbackProperties.maxTransformFeedbackStreams;
	const deUint32											streamsRequired				= m_parameters.streamId + 1;

	if (!features.geometryShader)
		TCU_THROW(NotSupportedError, "Missing feature: geometryShader");

	if (streamsRequired > 1 && transformFeedbackFeatures.geometryStreams == DE_FALSE)
		TCU_THROW(NotSupportedError, "geometryStreams feature is not supported");

	if (streamsSupported < streamsRequired)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackStreams=" + de::toString(streamsSupported) + ", while test requires " + de::toString(streamsRequired)).c_str());

	if (m_transformFeedbackProperties.transformFeedbackQueries == DE_FALSE)
		TCU_THROW(NotSupportedError, "transformFeedbackQueries feature is not supported");

	if (m_parameters.testType == TEST_TYPE_QUERY_RESET)
	{
		// Check VK_EXT_host_query_reset is supported
		m_context.requireDeviceFunctionality("VK_EXT_host_query_reset");
		if(m_context.getHostQueryResetFeatures().hostQueryReset == VK_FALSE)
			throw tcu::NotSupportedError(std::string("Implementation doesn't support resetting queries from the host").c_str());
	}
}

tcu::TestStatus TransformFeedbackQueryTestInstance::iterate (void)
{
	const DeviceInterface&				vk						= m_context.getDeviceInterface();
	const VkDevice						device					= m_context.getDevice();
	const deUint32						queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const VkQueue						queue					= m_context.getUniversalQueue();
	Allocator&							allocator				= m_context.getDefaultAllocator();

	const deUint64						overflowVertices		= 3u;
	const deUint32						bytesPerVertex			= static_cast<deUint32>(4 * sizeof(float));
	const deUint64						numVerticesInBuffer		= m_parameters.bufferSize / bytesPerVertex;
	const deUint64						numVerticesToWrite		= numVerticesInBuffer + overflowVertices;
	const Unique<VkRenderPass>			renderPass				(makeRenderPass							(vk, device, VK_FORMAT_UNDEFINED));

	const Unique<VkShaderModule>		vertModule				(createShaderModule						(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>		geomModule				(createShaderModule						(vk, device, m_context.getBinaryCollection().get("geom"), 0u));

	const Unique<VkFramebuffer>			framebuffer				(makeFramebuffer						(vk, device, *renderPass, 0u, DE_NULL, m_imageExtent2D.width, m_imageExtent2D.height));
	const Unique<VkPipelineLayout>		pipelineLayout			(TransformFeedback::makePipelineLayout	(vk, device));
	const Unique<VkPipeline>			pipeline				(makeGraphicsPipeline					(vk, device, *pipelineLayout, *renderPass, *vertModule, DE_NULL, DE_NULL, *geomModule, DE_NULL, m_imageExtent2D, 0u, &m_parameters.streamId, m_parameters.primTopology));
	const Unique<VkCommandPool>			cmdPool					(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer				(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const deUint32						tfBufferSize			= (deUint32)topologyData.at(m_parameters.primTopology).getNumPrimitives(numVerticesInBuffer) * (deUint32)topologyData.at(m_parameters.primTopology).primSize * bytesPerVertex;
	const VkBufferCreateInfo			tfBufCreateInfo			= makeBufferCreateInfo(tfBufferSize, VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	const Move<VkBuffer>				tfBuf					= createBuffer(vk, device, &tfBufCreateInfo);
	const MovePtr<Allocation>			tfBufAllocation			= bindBuffer(vk, device, allocator, *tfBuf, MemoryRequirement::HostVisible);
	const VkDeviceSize					tfBufBindingSize		= tfBufferSize;
	const VkDeviceSize					tfBufBindingOffset		= 0ull;

	const size_t						queryResultWidth		= (m_parameters.query64bits ? sizeof(deUint64) : sizeof(deUint32));
	const vk::VkQueryControlFlags		queryExtraFlags			= (m_parameters.query64bits ? vk::VK_QUERY_RESULT_64_BIT : 0);
	const deUint32						queryCountersNumber		= 1u;
	const deUint32						queryIndex				= 0u;
	constexpr deUint32					queryResultElements		= 2u;
	const deUint32						queryDataSize			= static_cast<deUint32>(queryResultElements * queryResultWidth);
	const VkQueryPoolCreateInfo			queryPoolCreateInfo		= makeQueryPoolCreateInfo(queryCountersNumber);
	const Unique<VkQueryPool>			queryPool				(createQueryPool(vk, device, &queryPoolCreateInfo));

	Move<VkBuffer>						queryPoolResultsBuffer;
	de::MovePtr<Allocation>				queryPoolResultsBufferAlloc;

	tcu::TestLog&						log						= m_context.getTestContext().getLog();

	DE_ASSERT(numVerticesInBuffer * bytesPerVertex == m_parameters.bufferSize);

	if (m_parameters.testType == TEST_TYPE_QUERY_COPY)
	{
		const VkBufferCreateInfo bufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,       // VkStructureType      sType;
			DE_NULL,                                    // const void*          pNext;
			0u,                                         // VkBufferCreateFlags  flags;
			queryDataSize,                              // VkDeviceSize         size;
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,           // VkBufferUsageFlags   usage;
			VK_SHARING_MODE_EXCLUSIVE,                  // VkSharingMode        sharingMode;
			1u,                                         // deUint32             queueFamilyCount;
			&queueFamilyIndex                           // const deUint32*      pQueueFamilyIndices;
		};

		queryPoolResultsBuffer = createBuffer(vk, device, &bufferParams);
		queryPoolResultsBufferAlloc = allocator.allocate(getBufferMemoryRequirements(vk, device, *queryPoolResultsBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(device, *queryPoolResultsBuffer, queryPoolResultsBufferAlloc->getMemory(), queryPoolResultsBufferAlloc->getOffset()));
	}

	beginCommandBuffer(vk, *cmdBuffer);
	{
		if (m_parameters.testType != TEST_TYPE_QUERY_RESET)
			vk.cmdResetQueryPool(*cmdBuffer, *queryPool, queryIndex, queryCountersNumber);

		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D));
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, 0u, 1u, &*tfBuf, &tfBufBindingOffset, &tfBufBindingSize);

			if (m_parameters.streamId == 0 && m_parameters.streamId0Mode != STREAM_ID_0_BEGIN_QUERY_INDEXED)
				vk.cmdBeginQuery(*cmdBuffer, *queryPool, queryIndex, 0u);
			else
				vk.cmdBeginQueryIndexedEXT(*cmdBuffer, *queryPool, queryIndex, 0u, m_parameters.streamId);
			{
				vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
				{
					vk.cmdDraw(*cmdBuffer, static_cast<deUint32>(numVerticesToWrite), 1u, 0u, 0u);
				}
				vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
			}
			if (m_parameters.streamId == 0 && m_parameters.streamId0Mode != STREAM_ID_0_END_QUERY_INDEXED)
				vk.cmdEndQuery(*cmdBuffer, *queryPool, queryIndex);
			else
				vk.cmdEndQueryIndexedEXT(*cmdBuffer, *queryPool, queryIndex, m_parameters.streamId);
		}
		endRenderPass(vk, *cmdBuffer);

		if (m_parameters.testType == TEST_TYPE_QUERY_COPY)
		{
			vk.cmdCopyQueryPoolResults(*cmdBuffer, *queryPool, queryIndex, queryCountersNumber, *queryPoolResultsBuffer, 0u, queryDataSize, (vk::VK_QUERY_RESULT_WAIT_BIT | queryExtraFlags));

			const VkBufferMemoryBarrier bufferBarrier =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
				DE_NULL,									// const void*		pNext;
				VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
				VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask;
				VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
				VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
				*queryPoolResultsBuffer,					// VkBuffer			buffer;
				0ull,										// VkDeviceSize		offset;
				VK_WHOLE_SIZE								// VkDeviceSize		size;
			};
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &bufferBarrier, 0u, DE_NULL);
		}

	}
	endCommandBuffer(vk, *cmdBuffer);

	if (m_parameters.testType == TEST_TYPE_QUERY_RESET)
		vk.resetQueryPool(device, *queryPool, queryIndex, queryCountersNumber);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	{
		union Results
		{
			deUint32	elements32[queryResultElements];
			deUint64	elements64[queryResultElements];
		};

		std::vector<deUint8>	queryData		(queryDataSize, 0u);
		const Results*			queryResults	= reinterpret_cast<Results*>(queryData.data());

		if (m_parameters.testType != TEST_TYPE_QUERY_COPY)
		{
			vk.getQueryPoolResults(device, *queryPool, queryIndex, queryCountersNumber, queryDataSize, queryData.data(), queryDataSize, (vk::VK_QUERY_RESULT_WAIT_BIT | queryExtraFlags));
		}
		else
		{
			invalidateAlloc(vk, device, *queryPoolResultsBufferAlloc);
			deMemcpy(queryData.data(), queryPoolResultsBufferAlloc->getHostPtr(), queryData.size());
		}

		// The number of primitives successfully written to the corresponding transform feedback buffer.
		const deUint64	numPrimitivesWritten	= (m_parameters.query64bits ? queryResults->elements64[0] : queryResults->elements32[0]);

		// The number of primitives output to the vertex stream.
		const deUint64	numPrimitivesNeeded		= (m_parameters.query64bits ? queryResults->elements64[1] : queryResults->elements32[1]);

		// Count how many primitives we should get by using selected topology.
		const auto		primitivesInBuffer		= topologyData.at(m_parameters.primTopology).getNumPrimitives(numVerticesInBuffer);
		const auto		primitivesToWrite		= topologyData.at(m_parameters.primTopology).getNumPrimitives(numVerticesToWrite);

		log << tcu::TestLog::Message << "Primitives Written / Expected :  " << de::toString(numPrimitivesWritten) << " / " << de::toString(primitivesInBuffer) << tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Primitives  Needed / Expected :  " << de::toString(numPrimitivesNeeded) << " / " << de::toString(primitivesToWrite) << tcu::TestLog::EndMessage;

		if (numPrimitivesWritten != primitivesInBuffer)
			return tcu::TestStatus::fail("numPrimitivesWritten=" + de::toString(numPrimitivesWritten) + " while expected " + de::toString(primitivesInBuffer));

		if (numPrimitivesNeeded != primitivesToWrite)
			return tcu::TestStatus::fail("numPrimitivesNeeded=" + de::toString(numPrimitivesNeeded) + " while expected " + de::toString(primitivesToWrite));
	}

	if (m_parameters.testType == TEST_TYPE_QUERY_RESET)
	{
		constexpr deUint32		queryResetElements		= queryResultElements + 1; // For the availability bit.

		union Results
		{
			deUint32	elements32[queryResetElements];
			deUint64	elements64[queryResetElements];
		};

		const deUint32			queryDataAvailSize		(static_cast<deUint32>(queryResetElements * queryResultWidth));
		std::vector<deUint8>	queryData				(queryDataAvailSize, 0u);
		Results*				queryResults			= reinterpret_cast<Results*>(queryData.data());

		// Initialize values
		if (m_parameters.query64bits)
		{
			queryResults->elements64[0] = 1u;	// numPrimitivesWritten
			queryResults->elements64[1] = 1u;	// numPrimitivesNeeded
			queryResults->elements64[2] = 1u;	// Availability bit
		}
		else
		{
			queryResults->elements32[0] = 1u;	// numPrimitivesWritten
			queryResults->elements32[1] = 1u;	// numPrimitivesNeeded
			queryResults->elements32[2] = 1u;	// Availability bit
		}

		vk.resetQueryPool(device, *queryPool, queryIndex, queryCountersNumber);

		vk::VkResult	res						= vk.getQueryPoolResults(device, *queryPool, queryIndex, queryCountersNumber, queryDataAvailSize, queryData.data(), queryDataAvailSize, (vk::VK_QUERY_RESULT_WITH_AVAILABILITY_BIT | queryExtraFlags));
		const deUint64	numPrimitivesWritten	= (m_parameters.query64bits ? queryResults->elements64[0] : queryResults->elements32[0]);
		const deUint64	numPrimitivesNeeded		= (m_parameters.query64bits ? queryResults->elements64[1] : queryResults->elements32[1]);
		const deUint64	availabilityState		= (m_parameters.query64bits ? queryResults->elements64[2] : queryResults->elements32[2]);

		/* From the Vulkan spec:
			*
			* If VK_QUERY_RESULT_WAIT_BIT and VK_QUERY_RESULT_PARTIAL_BIT are both not set then no result values are written to pData
			* for queries that are in the unavailable state at the time of the call, and vkGetQueryPoolResults returns VK_NOT_READY.
			* However, availability state is still written to pData for those queries if VK_QUERY_RESULT_WITH_AVAILABILITY_BIT is set.
			*/
		if (res != vk::VK_NOT_READY || availabilityState != 0u)
			return tcu::TestStatus::fail("QueryPoolResults incorrect reset");
	    if (numPrimitivesWritten != 1u || numPrimitivesNeeded != 1u)
			return tcu::TestStatus::fail("QueryPoolResults data was modified");

	}

	return tcu::TestStatus::pass("Pass");
}

class TransformFeedbackMultiQueryTestInstance : public TransformFeedbackTestInstance
{
public:
								TransformFeedbackMultiQueryTestInstance	(Context& context, const TestParameters& parameters);

protected:
	std::vector<VkDeviceSize>	generateSizesList							(const size_t bufBytes, const size_t chunkCount);
	void						verifyTransformFeedbackBuffer				(const MovePtr<Allocation>& bufAlloc, const deUint32 bufBytes, const deUint32 bufOffset, const float expected);
	tcu::TestStatus				iterate										(void);
};

TransformFeedbackMultiQueryTestInstance::TransformFeedbackMultiQueryTestInstance (Context& context, const TestParameters& parameters)
	: TransformFeedbackTestInstance	(context, parameters)
{
	const InstanceInterface&								vki							= m_context.getInstanceInterface();
	const VkPhysicalDevice									physDevice					= m_context.getPhysicalDevice();
	const VkPhysicalDeviceFeatures							features					= getPhysicalDeviceFeatures(vki, physDevice);
	const VkPhysicalDeviceTransformFeedbackFeaturesEXT&		transformFeedbackFeatures	= m_context.getTransformFeedbackFeaturesEXT();
	const deUint32											streamsSupported			= m_transformFeedbackProperties.maxTransformFeedbackStreams;
	const deUint32											streamsRequired				= m_parameters.streamId + 1;
	const deUint32											tfBuffersSupported			= m_transformFeedbackProperties.maxTransformFeedbackBuffers;
	const deUint32											tfBuffersRequired			= m_parameters.partCount;
	const deUint32											bytesPerVertex				= m_parameters.bufferSize / m_parameters.partCount;
	const deUint32											tfStreamDataSizeSupported	= m_transformFeedbackProperties.maxTransformFeedbackStreamDataSize;
	const deUint32											tfBufferDataSizeSupported	= m_transformFeedbackProperties.maxTransformFeedbackBufferDataSize;
	const deUint32											tfBufferDataStrideSupported	= m_transformFeedbackProperties.maxTransformFeedbackBufferDataStride;

	DE_ASSERT(m_parameters.partCount == 2u);

	if (!features.geometryShader)
		TCU_THROW(NotSupportedError, "Missing feature: geometryShader");

	if (transformFeedbackFeatures.geometryStreams == DE_FALSE)
		TCU_THROW(NotSupportedError, "geometryStreams feature is not supported");

	if (streamsSupported < streamsRequired)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackStreams=" + de::toString(streamsSupported) + ", while test requires " + de::toString(streamsRequired)).c_str());

	if (tfBuffersSupported < tfBuffersRequired)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBuffers=" + de::toString(tfBuffersSupported) + ", while test requires " + de::toString(tfBuffersRequired)).c_str());

	if (tfStreamDataSizeSupported < bytesPerVertex)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackStreamDataSize=" + de::toString(tfStreamDataSizeSupported) + ", while test requires " + de::toString(bytesPerVertex)).c_str());

	if (tfBufferDataSizeSupported < bytesPerVertex)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBufferDataSize=" + de::toString(tfBufferDataSizeSupported) + ", while test requires " + de::toString(bytesPerVertex)).c_str());

	if (tfBufferDataStrideSupported < bytesPerVertex)
		TCU_THROW(NotSupportedError, std::string("maxTransformFeedbackBufferDataStride=" + de::toString(tfBufferDataStrideSupported) + ", while test requires " + de::toString(bytesPerVertex)).c_str());

	if (m_transformFeedbackProperties.transformFeedbackQueries == DE_FALSE)
		TCU_THROW(NotSupportedError, "transformFeedbackQueries feature is not supported");
}

std::vector<VkDeviceSize> TransformFeedbackMultiQueryTestInstance::generateSizesList (const size_t bufBytes, const size_t chunkCount)
{
	const VkDeviceSize			chunkSize	= bufBytes / chunkCount;
	std::vector<VkDeviceSize>	result		(chunkCount, chunkSize);

	DE_ASSERT(chunkSize * chunkCount == bufBytes);
	DE_ASSERT(bufBytes <= MINIMUM_TF_BUFFER_SIZE);
	DE_ASSERT(bufBytes % sizeof(deUint32) == 0);
	DE_ASSERT(chunkCount > 0);
	DE_ASSERT(result.size() == chunkCount);

	return result;
}

void TransformFeedbackMultiQueryTestInstance::verifyTransformFeedbackBuffer (const MovePtr<Allocation>& bufAlloc, const deUint32 bufBytes, const deUint32 bufOffset, const float expected)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			device		= m_context.getDevice();

	invalidateAlloc(vk, device, *bufAlloc);

	const deUint32			numPoints	= bufBytes / static_cast<deUint32>(sizeof(float));
	const deUint8*			tfDataRaw	= reinterpret_cast<const deUint8*>(bufAlloc->getHostPtr());
	const float*			tfData		= reinterpret_cast<const float*>(&tfDataRaw[bufOffset]);

	for (deUint32 i = 0; i < numPoints; ++i)
		if (tfData[i] != expected)
			TCU_FAIL(std::string("Failed at item ") + de::toString(i) + " received:" + de::toString(tfData[i]) + " expected:" + de::toString(expected));
}

tcu::TestStatus TransformFeedbackMultiQueryTestInstance::iterate (void)
{
	const DeviceInterface&						vk							= m_context.getDeviceInterface();
	const VkDevice								device						= m_context.getDevice();
	const deUint32								queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
	const std::vector<deUint32>					queueFamilyIndices			= { queueFamilyIndex };
	const VkQueue								queue						= m_context.getUniversalQueue();
	Allocator&									allocator					= m_context.getDefaultAllocator();

	const Unique<VkRenderPass>					renderPass					(makeRenderPass							(vk, device, VK_FORMAT_UNDEFINED));

	const Unique<VkShaderModule>				vertModule					(createShaderModule						(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>				geomModule					(createShaderModule						(vk, device, m_context.getBinaryCollection().get("geom"), 0u));

	const Unique<VkFramebuffer>					framebuffer					(makeFramebuffer						(vk, device, *renderPass, 0u, DE_NULL, m_imageExtent2D.width, m_imageExtent2D.height));
	const Unique<VkPipelineLayout>				pipelineLayout				(TransformFeedback::makePipelineLayout	(vk, device));
	const Unique<VkPipeline>					pipeline					(makeGraphicsPipeline					(vk, device, *pipelineLayout, *renderPass, *vertModule, DE_NULL, DE_NULL, *geomModule, DE_NULL, m_imageExtent2D, 0u));
	const Unique<VkCommandPool>					cmdPool						(createCommandPool						(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>				cmdBuffer					(allocateCommandBuffer					(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const VkBufferCreateInfo					tfBufCreateInfo				= makeBufferCreateInfo(m_parameters.bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
	const Move<VkBuffer>						tfBuf						= createBuffer(vk, device, &tfBufCreateInfo);
	const std::vector<VkBuffer>					tfBufArray					= std::vector<VkBuffer>(m_parameters.partCount, *tfBuf);
	const MovePtr<Allocation>					tfBufAllocation				= allocator.allocate(getBufferMemoryRequirements(vk, device, *tfBuf), MemoryRequirement::HostVisible);
	const VkMemoryBarrier						tfMemoryBarrier				= makeMemoryBarrier(VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT, VK_ACCESS_HOST_READ_BIT);
	const std::vector<VkDeviceSize>				tfBufBindingSizes			= generateSizesList(m_parameters.bufferSize, m_parameters.partCount);
	const std::vector<VkDeviceSize>				tfBufBindingOffsets			= generateOffsetsList(tfBufBindingSizes);
	const std::vector<float>					tfBufExpectedValues			= { 0.5f, 0.5f + float(m_parameters.streamId) };
	const deUint32								maxBufferSizeBytes			= static_cast<deUint32>(*std::max_element(tfBufBindingSizes.begin(), tfBufBindingSizes.end()));
	const deUint32								bytesPerVertex				= static_cast<deUint32>(4 * sizeof(float));
	const deUint32								numVerticesInBuffer			= maxBufferSizeBytes / bytesPerVertex;
	const deUint32								numDrawVertices				= numVerticesInBuffer / 2;

	const deUint32								queryIndex					= 0u;
	const deUint32								queryCountersNumber			= 2u;
	const deUint32								queryStride					= sizeof(TransformFeedbackQuery);
	const deUint32								queryDataSize				= queryCountersNumber * queryStride;
	const VkQueryPoolCreateInfo					queryPoolCreateInfo			= makeQueryPoolCreateInfo(queryCountersNumber);
	const Unique<VkQueryPool>					queryPool					(createQueryPool(vk, device, &queryPoolCreateInfo));
	const deUint32								queryInvalidCounterValue	= 999999u;
	std::vector<TransformFeedbackQuery>			queryResultData				(queryCountersNumber, TransformFeedbackQuery{ queryInvalidCounterValue, queryInvalidCounterValue });
	const std::vector<TransformFeedbackQuery>	queryExpectedData			({ TransformFeedbackQuery{ numVerticesInBuffer, 3 * numDrawVertices }, TransformFeedbackQuery{ numDrawVertices, numDrawVertices } });

	const VkBufferCreateInfo					queryBufferCreateInfo		= makeBufferCreateInfo(queryDataSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, queueFamilyIndices);
	const Move<VkBuffer>						queryPoolResultsBuffer		= createBuffer(vk, device, &queryBufferCreateInfo);
	const MovePtr<Allocation>					queryPoolResultsBufferAlloc	= allocator.allocate(getBufferMemoryRequirements(vk, device, *queryPoolResultsBuffer), MemoryRequirement::HostVisible);

	DE_ASSERT(queryCountersNumber == queryExpectedData.size());

	VK_CHECK(vk.bindBufferMemory(device, *queryPoolResultsBuffer, queryPoolResultsBufferAlloc->getMemory(), queryPoolResultsBufferAlloc->getOffset()));

	VK_CHECK(vk.bindBufferMemory(device, *tfBuf, tfBufAllocation->getMemory(), tfBufAllocation->getOffset()));

	beginCommandBuffer(vk, *cmdBuffer);
	{
		vk.cmdResetQueryPool(*cmdBuffer, *queryPool, queryIndex, queryCountersNumber);

		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(m_imageExtent2D));
		{
			vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

			vk.cmdBindTransformFeedbackBuffersEXT(*cmdBuffer, 0u, m_parameters.partCount, &tfBufArray[0], &tfBufBindingOffsets[0], &tfBufBindingSizes[0]);

			vk.cmdBeginQueryIndexedEXT(*cmdBuffer, *queryPool, queryIndex + 0, 0u, 0u);
			vk.cmdBeginQueryIndexedEXT(*cmdBuffer, *queryPool, queryIndex + 1, 0u, m_parameters.streamId);
			{
				vk.cmdBeginTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
				{
					vk.cmdDraw(*cmdBuffer, numDrawVertices, 1u, 0u, 0u);
				}
				vk.cmdEndTransformFeedbackEXT(*cmdBuffer, 0, 0, DE_NULL, DE_NULL);
			}
			vk.cmdEndQueryIndexedEXT(*cmdBuffer, *queryPool, queryIndex + 1, m_parameters.streamId);
			vk.cmdEndQueryIndexedEXT(*cmdBuffer, *queryPool, queryIndex + 0, 0);
		}
		endRenderPass(vk, *cmdBuffer);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &tfMemoryBarrier, 0u, DE_NULL, 0u, DE_NULL);
	}
	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	vk.getQueryPoolResults(device, *queryPool, queryIndex, queryCountersNumber, queryDataSize, queryResultData.data(), queryStride, (vk::VK_QUERY_RESULT_WAIT_BIT));

	DE_ASSERT(queryResultData.size() == queryCountersNumber && queryExpectedData.size() == queryCountersNumber);
	DE_ASSERT(queryCountersNumber > 0);

	for (size_t counterNdx = 0; counterNdx < queryCountersNumber; ++counterNdx)
	{
		const TransformFeedbackQuery&	result		= queryResultData[counterNdx];
		const TransformFeedbackQuery&	expected	= queryExpectedData[counterNdx];

		DE_ASSERT(expected.written != queryInvalidCounterValue);
		DE_ASSERT(expected.attempts != queryInvalidCounterValue);

		if (result.written == queryInvalidCounterValue || result.attempts == queryInvalidCounterValue)
			return tcu::TestStatus::fail("Query counters read failed");

		if (result.written != expected.written)
		{
			const std::string	comment	= "At counter " + de::toString(counterNdx) + " vertices written " + de::toString(result.written) + ", while expected " + de::toString(expected.written);

			return tcu::TestStatus::fail(comment.c_str());
		}


		if (result.attempts != expected.attempts)
		{
			const std::string	comment = "At counter " + de::toString(counterNdx) + " attempts committed " + de::toString(result.attempts) + ", while expected " + de::toString(expected.attempts);

			return tcu::TestStatus::fail(comment.c_str());
		}

		verifyTransformFeedbackBuffer(tfBufAllocation, bytesPerVertex * expected.written, static_cast<deUint32>(tfBufBindingOffsets[counterNdx]), tfBufExpectedValues[counterNdx]);
	}

	return tcu::TestStatus::pass("Pass");
}


class TransformFeedbackTestCase : public vkt::TestCase
{
public:
						TransformFeedbackTestCase	(tcu::TestContext &context, const char *name, const char *description, const TestParameters& parameters);

protected:
	vkt::TestInstance*	createInstance				(vkt::Context& context) const;
	void				initPrograms				(SourceCollections& programCollection) const;

	TestParameters		m_parameters;
};

TransformFeedbackTestCase::TransformFeedbackTestCase (tcu::TestContext &context, const char *name, const char *description, const TestParameters& parameters)
	: TestCase		(context, name, description)
	, m_parameters	(parameters)
{
}

vkt::TestInstance*	TransformFeedbackTestCase::createInstance (vkt::Context& context) const
{
	if (m_parameters.testType == TEST_TYPE_BASIC)
		return new TransformFeedbackBasicTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_RESUME)
		return new TransformFeedbackResumeTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_XFB_POINTSIZE)
		return new TransformFeedbackBuiltinTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_XFB_CLIPDISTANCE)
		return new TransformFeedbackBuiltinTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_XFB_CULLDISTANCE)
		return new TransformFeedbackBuiltinTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_XFB_CLIP_AND_CULL)
		return new TransformFeedbackBuiltinTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_WINDING)
		return new TransformFeedbackWindingOrderTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_STREAMS)
		return new TransformFeedbackStreamsTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_STREAMS_POINTSIZE)
		return new TransformFeedbackStreamsTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_STREAMS_CLIPDISTANCE)
		return new TransformFeedbackStreamsTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_STREAMS_CULLDISTANCE)
		return new TransformFeedbackStreamsTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_MULTISTREAMS)
		return new TransformFeedbackMultistreamTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_DRAW_INDIRECT)
		return new TransformFeedbackIndirectDrawTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_BACKWARD_DEPENDENCY)
		return new TransformFeedbackBackwardDependencyTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_QUERY_GET	||
		m_parameters.testType == TEST_TYPE_QUERY_COPY	||
	    m_parameters.testType == TEST_TYPE_QUERY_RESET)
		return new TransformFeedbackQueryTestInstance(context, m_parameters);

	if (m_parameters.testType == TEST_TYPE_MULTIQUERY)
		return new TransformFeedbackMultiQueryTestInstance(context, m_parameters);

	TCU_THROW(InternalError, "Specified test type not found");
}

void TransformFeedbackTestCase::initPrograms (SourceCollections& programCollection) const
{
	const bool vertexShaderOnly		=  m_parameters.testType == TEST_TYPE_BASIC
									|| m_parameters.testType == TEST_TYPE_RESUME
									|| m_parameters.testType == TEST_TYPE_BACKWARD_DEPENDENCY
									|| (m_parameters.testType == TEST_TYPE_WINDING && m_parameters.primTopology != VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);
	const bool requiresFullPipeline	=  m_parameters.testType == TEST_TYPE_STREAMS
									|| m_parameters.testType == TEST_TYPE_STREAMS_POINTSIZE
									|| m_parameters.testType == TEST_TYPE_STREAMS_CULLDISTANCE
									|| m_parameters.testType == TEST_TYPE_STREAMS_CLIPDISTANCE
									|| (m_parameters.testType == TEST_TYPE_WINDING && m_parameters.primTopology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);
	const bool xfbBuiltinPipeline	=  m_parameters.testType == TEST_TYPE_XFB_POINTSIZE
									|| m_parameters.testType == TEST_TYPE_XFB_CLIPDISTANCE
									|| m_parameters.testType == TEST_TYPE_XFB_CULLDISTANCE
									|| m_parameters.testType == TEST_TYPE_XFB_CLIP_AND_CULL;

	if (vertexShaderOnly)
	{
		// Vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(push_constant) uniform pushConstants\n"
				<< "{\n"
				<< "    uint start;\n"
				<< "} uInput;\n"
				<< "\n"
				<< "layout(xfb_buffer = 0, xfb_offset = 0, xfb_stride = 4, location = 0) out uint idx_out;\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    idx_out = uInput.start + gl_VertexIndex;\n"
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		return;
	}

	if (m_parameters.primTopology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
	{
		// Vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "layout(push_constant) uniform pushConstants\n"
				<< "{\n"
				<< "    uint start;\n"
				<< "} uInput;\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "}\n";
			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		// Tesselation control shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "layout(vertices = 3) out;\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    gl_TessLevelInner[0] = 2.0;\n" // generate three triangles out of each patch
				<< "    gl_TessLevelOuter[0] = 1.0;\n"
				<< "    gl_TessLevelOuter[1] = 1.0;\n"
				<< "    gl_TessLevelOuter[2] = 1.0;\n"
				<< "}\n";
			programCollection.glslSources.add("tesc") << glu::TessellationControlSource(src.str());
		}

		// Tessellation evaluation shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "layout(triangles, ccw) in;\n"
				<< "layout(xfb_buffer = 0, xfb_offset = 0, xfb_stride = 4, location = 0) out uint idx_out;\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    idx_out = gl_PrimitiveID;\n" // all vertex generated from patch will have its id
				<< "}\n";
			programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str());
		}

		return;
	}

	if (xfbBuiltinPipeline)
	{
		const std::string	outputBuiltIn		= (m_parameters.testType == TEST_TYPE_XFB_POINTSIZE)     ? "float gl_PointSize;\n"
												: (m_parameters.testType == TEST_TYPE_XFB_CLIPDISTANCE)  ? "float gl_ClipDistance[8];\n"
												: (m_parameters.testType == TEST_TYPE_XFB_CULLDISTANCE)  ? "float gl_CullDistance[8];\n"
												: (m_parameters.testType == TEST_TYPE_XFB_CLIP_AND_CULL) ? "float gl_CullDistance[5];\nfloat gl_ClipDistance[1];\n"
												: "";
		const std::string	operationBuiltIn	= (m_parameters.testType == TEST_TYPE_XFB_POINTSIZE)     ? "gl_PointSize = float(gl_VertexIndex) / 32768.0f;"
												: (m_parameters.testType == TEST_TYPE_XFB_CLIPDISTANCE)  ? "for (int i=0; i<8; i++) gl_ClipDistance[i] = float(8 * gl_VertexIndex + i) / 32768.0f;"
												: (m_parameters.testType == TEST_TYPE_XFB_CULLDISTANCE)  ? "for (int i=0; i<8; i++) gl_CullDistance[i] = float(8 * gl_VertexIndex + i) / 32768.0f;"
												: (m_parameters.testType == TEST_TYPE_XFB_CLIP_AND_CULL) ? "for (int i=0; i<5; i++) gl_CullDistance[i] = float(6 * gl_VertexIndex + i) / 32768.0f;\n"
																										   "gl_ClipDistance[0] = float(6 * gl_VertexIndex + 5) / 32768.0f;\n"
												: "";

		// Vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(xfb_buffer = " << m_parameters.partCount - 1 << ", xfb_offset = 0) out gl_PerVertex\n"
				<< "{\n"
				<< outputBuiltIn
				<< "};\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< operationBuiltIn
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		return;
	}

	if (m_parameters.testType == TEST_TYPE_MULTISTREAMS)
	{
		// vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		// geometry shader
		{
			const deUint32		s	= m_parameters.streamId;
			std::ostringstream	src;

			DE_ASSERT(s != 0);

			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(points) in;\n"
				<< "\n"
				<< "layout(points, max_vertices = 32) out;\n"
				<< "layout(stream = " << 0 << ", xfb_buffer = 0, xfb_offset = 0, xfb_stride = 16, location = 0) out vec4 out0;\n"
				<< "layout(stream = " << s << ", xfb_buffer = 1, xfb_offset = 0, xfb_stride = 16, location = 1) out vec4 out1;\n"
				<< "\n"
				<< "const int counts[] = int[](1, 1, 2, 4, 8);\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    int c0 = 0;\n"
				<< "    int c1 = 0;\n"
				<< "\n"
				<< "    // Start 1st buffer from point where 0th buffer ended\n"
				<< "    for (int i = 0; i < counts.length(); i++)\n"
				<< "        c1 = c1 + 4 * counts[i];\n"
				<< "\n"
				<< "    for (int i = 0; i < counts.length(); i++)\n"
				<< "    {\n"
				<< "        const int n0 = counts[i];\n"
				<< "        const int n1 = counts[counts.length() - 1 - i];\n"
				<< "\n"
				<< "        for (int j = 0; j < n0; j++)\n"
				<< "        {\n"
				<< "            out0 = vec4(ivec4(c0, c0 + 1, c0 + 2, c0 + 3));\n"
				<< "            c0 = c0 + 4;\n"
				<< "            EmitStreamVertex(0);\n"
				<< "            EndStreamPrimitive(0);\n"
				<< "        }\n"
				<< "\n"
				<< "        for (int j = 0; j < n1; j++)\n"
				<< "        {\n"
				<< "            out1 = vec4(ivec4(c1, c1 + 1, c1 + 2, c1 + 3));\n"
				<< "            c1 = c1 + 4;\n"
				<< "            EmitStreamVertex(" << s << ");\n"
				<< "            EndStreamPrimitive(" << s << ");\n"
				<< "        }\n"
				<< "    }\n"
				<< "}\n";

			programCollection.glslSources.add("geom") << glu::GeometrySource(src.str());
		}

		return;
	}

	if (requiresFullPipeline)
	{
		// vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		// geometry shader
		{
			const deUint32		s					= m_parameters.streamId;
			const bool			requirePoints		= (m_parameters.testType == TEST_TYPE_STREAMS_POINTSIZE || m_parameters.testType == TEST_TYPE_MULTISTREAMS);
			const std::string	outputPrimitiveType	= requirePoints ? "points" : "triangle_strip";
			const std::string	outputBuiltIn		= (m_parameters.testType == TEST_TYPE_STREAMS_POINTSIZE)    ? "    float gl_PointSize;\n"
													: (m_parameters.testType == TEST_TYPE_STREAMS_CLIPDISTANCE) ? "    float gl_ClipDistance[];\n"
													: (m_parameters.testType == TEST_TYPE_STREAMS_CULLDISTANCE) ? "    float gl_CullDistance[];\n"
													: "";
			std::ostringstream	src;

			DE_ASSERT(s != 0);

			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(points) in;\n"
				<< "layout(" << outputPrimitiveType << ", max_vertices = 16) out;\n"
				<< "layout(stream = " << s << ") out;\n"
				<< "layout(location = 0) out vec4 color;\n"
				<< "\n"
				<< "layout(stream = " << s << ") out gl_PerVertex\n"
				<< "{\n"
				<< "    vec4 gl_Position;\n"
				<< outputBuiltIn
				<< "};\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    // Color constants\n"
				<< "    vec4 g = vec4(0.0, 1.0, 0.0, 1.0);\n"
				<< "    vec4 m = vec4(1.0, 0.0, 1.0, 1.0);\n"
				<< "    // Coordinate constants: leftmost column\n"
				<< "    vec4 a = vec4(-1.0,-1.0, 0.0, 1.0);\n"
				<< "    vec4 b = vec4(-1.0, 0.0, 0.0, 1.0);\n"
				<< "    vec4 c = vec4(-1.0, 1.0, 0.0, 1.0);\n"
				<< "    // Coordinate constants: middle column\n"
				<< "    vec4 i = vec4( 0.0,-1.0, 0.0, 1.0);\n"
				<< "    vec4 j = vec4( 0.0, 0.0, 0.0, 1.0);\n"
				<< "    vec4 k = vec4( 0.0, 1.0, 0.0, 1.0);\n"
				<< "    // Coordinate constants: rightmost column\n"
				<< "    vec4 x = vec4( 1.0,-1.0, 0.0, 1.0);\n"
				<< "    vec4 y = vec4( 1.0, 0.0, 0.0, 1.0);\n"
				<< "    vec4 z = vec4( 1.0, 1.0, 0.0, 1.0);\n"
				<< "\n";

			if (m_parameters.testType == TEST_TYPE_STREAMS)
			{
				src << "    if (gl_PrimitiveIDIn == 0)\n"
					<< "    {\n"
					<< "        color = m; gl_Position = b; EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = y; EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = c; EmitStreamVertex(" << s << ");\n"
					<< "        EndStreamPrimitive(" << s << ");\n"
					<< "    }\n"
					<< "    else\n"
					<< "    {\n"
					<< "        color = m; gl_Position = y; EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = c; EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = z; EmitStreamVertex(" << s << ");\n"
					<< "        EndStreamPrimitive(" << s << ");\n"
					<< "    }\n";
			}

			if (m_parameters.testType == TEST_TYPE_STREAMS_POINTSIZE)
			{
				const std::string	pointSize	= "gl_PointSize = " + de::toString(m_parameters.pointSize) + ".0f";

				src << "    if (gl_PrimitiveIDIn == 0)\n"
					<< "    {\n"
					<< "        color = g; gl_Position = (a + j) / 2.0f; gl_PointSize = 1.0f; EmitStreamVertex(0);\n"
					<< "        EndStreamPrimitive(0);\n"
					<< "        color = m; gl_Position = (b + k) / 2.0f; gl_PointSize = 1.0f; EmitStreamVertex(" << s << ");\n"
					<< "        EndStreamPrimitive(" << s << ");\n"
					<< "    }\n"
					<< "    else\n"
					<< "    {\n"
					<< "        color = g; gl_Position = (j + x) / 2.0f; " << pointSize << "; EmitStreamVertex(0);\n"
					<< "        EndStreamPrimitive(0);\n"
					<< "        color = m; gl_Position = (k + y) / 2.0f; " << pointSize << "; EmitStreamVertex(" << s << ");\n"
					<< "        EndStreamPrimitive(" << s << ");\n"
					<< "    }\n";
			}

			if (m_parameters.testType == TEST_TYPE_STREAMS_CLIPDISTANCE)
			{
				src << "    if (gl_PrimitiveIDIn == 0)\n"
					<< "    {\n"
					<< "        color = m; gl_Position = b; gl_ClipDistance[0] = -1.0; EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = c; gl_ClipDistance[0] = -1.0; EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = y; gl_ClipDistance[0] =  1.0; EmitStreamVertex(" << s << ");\n"
					<< "        EndStreamPrimitive(" << s << ");\n"
					<< "    }\n"
					<< "    else\n"
					<< "    {\n"
					<< "        color = m; gl_Position = y; gl_ClipDistance[0] =  1.0; EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = c; gl_ClipDistance[0] = -1.0; EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = z; gl_ClipDistance[0] =  1.0; EmitStreamVertex(" << s << ");\n"
					<< "        EndStreamPrimitive(" << s << ");\n"
					<< "    }\n";
			}

			if (m_parameters.testType == TEST_TYPE_STREAMS_CULLDISTANCE)
			{
				src << "    if (gl_PrimitiveIDIn == 0)\n"
					<< "    {\n"
					<< "        color = m; gl_Position = b; gl_CullDistance[0] = -1.0; EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = c; gl_CullDistance[0] = -1.0; EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = j; gl_CullDistance[0] = -1.0; EmitStreamVertex(" << s << ");\n"
					<< "        EndStreamPrimitive(" << s << ");\n"
					<< "        color = m; gl_Position = j; gl_CullDistance[0] = -1.0; EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = c; gl_CullDistance[0] = -1.0; EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = k; gl_CullDistance[0] = -1.0; EmitStreamVertex(" << s << ");\n"
					<< "        EndStreamPrimitive(" << s << ");\n"
					<< "    }\n"
					<< "    else\n"
					<< "    {\n"
					<< "        color = m; gl_Position = j; gl_CullDistance[0] =  1.0; EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = k; gl_CullDistance[0] =  1.0; EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = y; gl_CullDistance[0] =  1.0; EmitStreamVertex(" << s << ");\n"
					<< "        EndStreamPrimitive(" << s << ");\n"
					<< "        color = m; gl_Position = y; gl_CullDistance[0] =  1.0; EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = k; gl_CullDistance[0] =  1.0; EmitStreamVertex(" << s << ");\n"
					<< "        color = m; gl_Position = z; gl_CullDistance[0] =  1.0; EmitStreamVertex(" << s << ");\n"
					<< "        EndStreamPrimitive(" << s << ");\n"
					<< "    }\n";
			}

			src << "}\n";

			programCollection.glslSources.add("geom") << glu::GeometrySource(src.str());
		}

		// Fragment shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(location = 0) in  vec4 i_color;\n"
				<< "layout(location = 0) out vec4 o_color;\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    o_color = i_color;\n"
				<< "}\n";

			programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
		}

		return;
	}

	if (m_parameters.testType == TEST_TYPE_DRAW_INDIRECT)
	{
		// vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(location = 0) in vec4 in_position;\n"
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
				<< "void main(void)\n"
				<< "{\n"
				<< "    o_color = vec4(1.0, 1.0, 1.0, 1.0);\n"
				<< "}\n";

			programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
		}

		return;
	}

	if (m_parameters.testType == TEST_TYPE_QUERY_GET	||
		m_parameters.testType == TEST_TYPE_QUERY_COPY	||
		m_parameters.testType == TEST_TYPE_QUERY_RESET)
	{
		struct TopologyShaderInfo
		{
			std::string glslIn;
			std::string glslOut;
			std::string spirvIn;
			std::string spirvOut;
		};

		const std::map<VkPrimitiveTopology, TopologyShaderInfo>	primitiveNames	=
		{
			{ VK_PRIMITIVE_TOPOLOGY_POINT_LIST						, { "points"				, "points"			, "InputPoints"				, "OutputPoints"		} },
			{ VK_PRIMITIVE_TOPOLOGY_LINE_LIST						, { "lines"					, "line_strip"		, "InputLines"				, "OutputLineStrip"		} },
			{ VK_PRIMITIVE_TOPOLOGY_LINE_STRIP						, { "lines"					, "line_strip"		, "InputLines"				, "OutputLineStrip"		} },
			{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST					, { "triangles"				, "triangle_strip"	, "Triangles"				, "OutputTriangleStrip"	} },
			{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP					, { "triangles"				, "triangle_strip"	, "Triangles"				, "OutputTriangleStrip"	} },
			{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN					, { "triangles"				, "triangle_strip"	, "Triangles"				, "OutputTriangleStrip"	} },
			{ VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY		, { "lines_adjacency"		, "line_strip"		, "InputLinesAdjacency"		, "OutputLineStrip"		} },
			{ VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY		, { "lines_adjacency"		, "line_strip"		, "InputLinesAdjacency"		, "OutputLineStrip"		} },
			{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY	, { "triangles_adjacency"	, "triangle_strip"	, "InputTrianglesAdjacency"	, "OutputTriangleStrip"	} },
			{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY	, { "triangles_adjacency"	, "triangle_strip"	, "InputTrianglesAdjacency"	, "OutputTriangleStrip"	} }
		};

		// Vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(location = 0) out vec4 out0;\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    float n = 4.0 * float(gl_VertexIndex);\n"
				<< "    out0 = vec4(n + 0.0, n + 1.0, n + 2.0, n + 3.0);\n"
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		// geometry shader
		if (m_parameters.streamId == 0)
		{
			std::ostringstream	src;

			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(" << primitiveNames.at(m_parameters.primTopology).glslIn << ") in;\n"
				<< "layout(location = 0) in vec4 in0[];\n"
				<< "\n"
				<< "layout(" << primitiveNames.at(m_parameters.primTopology).glslOut << ", max_vertices = " << topologyData.at(m_parameters.primTopology).primSize<< ") out;\n"
				<< "layout(xfb_buffer = 0, xfb_offset = 0, xfb_stride = 16, location = 0) out vec4 out0;\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n";

				for (deUint32 i = 0; i < topologyData.at(m_parameters.primTopology).primSize; i++)
				{
					src << "    out0 = in0[" << i << "];\n"
						<< "    EmitVertex();\n";
				}

			src << "    EndPrimitive();\n"
				<< "}\n";

			programCollection.glslSources.add("geom") << glu::GeometrySource(src.str());
		}
		else
		{
			const deUint32		s	= m_parameters.streamId;
			std::ostringstream	src;

			if (m_parameters.testType == TEST_TYPE_QUERY_GET)
			{
				// The SPIR-V program below is roughly equivalent to the following GLSL code:
				//
				// #version 450
				// #extension GL_ARB_enhanced_layouts : require
				//
				// layout(points) in;
				// layout(location = 0) in vec4 in0[];
				//
				// layout(points, max_vertices = 1) out;
				// layout(location=0, stream=1, xfb_buffer=0, xfb_stride=16) out OutBlock {
				//     layout(xfb_offset=0, location=0) vec4 out0;
				// } outBlock;
				//
				// void main(void)
				// {
				//     outBlock.out0 = in0[0];
				//     EmitStreamVertex(1);
				//     EndStreamPrimitive(1);
				// }
				//
				// However, the stream number has been parametrized and the code generated by glslang has been tuned to move the
				// Stream, XfbBuffer and XfbStride decorations to the structure member instead of the block. This allows us to test
				// transform feedback decorations on structure members as part of these basic tests.
				src	<< "; SPIR-V\n"
					<< "; Version: 1.0\n"
					<< "; Generator: Khronos Glslang Reference Front End; 10\n"
					<< "; Bound: 64\n"
					<< "; Schema: 0\n"
					<< "               OpCapability Geometry\n"
					<< "               OpCapability TransformFeedback\n"
					<< "               OpCapability GeometryStreams\n"
					<< "          %1 = OpExtInstImport \"GLSL.std.450\"\n"
					<< "               OpMemoryModel Logical GLSL450\n"
					<< "               OpEntryPoint Geometry %main \"main\" %outBlock %in0\n"
					<< "               OpExecutionMode %main Xfb\n"
					<< "               OpExecutionMode %main " << primitiveNames.at(m_parameters.primTopology).spirvIn << "\n"
					<< "               OpExecutionMode %main Invocations 1\n"
					<< "               OpExecutionMode %main  " << primitiveNames.at(m_parameters.primTopology).spirvOut << "\n"
					<< "               OpExecutionMode %main OutputVertices " << topologyData.at(m_parameters.primTopology).primSize << "\n"
					<< "               OpSource GLSL 450\n"
					<< "               OpSourceExtension \"GL_ARB_enhanced_layouts\"\n"
					<< "               OpName %main \"main\"\n"
					<< "               OpName %OutBlock \"OutBlock\"\n"
					<< "               OpMemberName %OutBlock 0 \"out0\"\n"
					<< "               OpName %outBlock \"outBlock\"\n"
					<< "               OpName %in0 \"in0\"\n"
					<< "               OpMemberDecorate %OutBlock 0 Location 0\n"
					<< "               OpMemberDecorate %OutBlock 0 Offset 0\n"
					// These Stream, XfbBuffer and XfbStride decorations have been moved to the struct member.
					<< "               OpMemberDecorate %OutBlock 0 Stream " << s << "\n"
					<< "               OpMemberDecorate %OutBlock 0 XfbBuffer 0\n"
					<< "               OpMemberDecorate %OutBlock 0 XfbStride 16\n"
					<< "               OpDecorate %OutBlock Block\n"
					// The decorations mentioned above were using OpDecorate and assigned to %outBlock itself here.
					<< "               OpDecorate %in0 Location 0\n"
					<< "       %void = OpTypeVoid\n"
					<< "          %3 = OpTypeFunction %void\n"
					<< "      %float = OpTypeFloat 32\n"
					<< "    %v4float = OpTypeVector %float 4\n"
					<< "   %OutBlock = OpTypeStruct %v4float\n"
					<< "%_ptr_Output_OutBlock = OpTypePointer Output %OutBlock\n"
					<< "   %outBlock = OpVariable %_ptr_Output_OutBlock Output\n"
					<< "        %int = OpTypeInt 32 1\n"
					<< "      %int_0 = OpConstant %int 0\n";

				for (deUint32 i = 1; i < topologyData.at(m_parameters.primTopology).primSize + 1; i++)
				{
					src << "%int_" << i << " = OpConstant %int " << i << "\n";
				}

				src << "       %uint = OpTypeInt 32 0\n"
					<< "     %uint_0 = OpConstant %uint " << topologyData.at(m_parameters.primTopology).primSize << "\n"
					<< "%_arr_v4float_uint_0 = OpTypeArray %v4float %uint_0\n"
					<< "%_ptr_Input__arr_v4float_uint_0 = OpTypePointer Input %_arr_v4float_uint_0\n"
					<< "        %in0 = OpVariable %_ptr_Input__arr_v4float_uint_0 Input\n"
					<< "%_ptr_Input_v4float = OpTypePointer Input %v4float\n"
					<< "%_ptr_Output_v4float = OpTypePointer Output %v4float\n"
					<< "  %streamNum = OpConstant %int " << s << "\n"
					<< "       %main = OpFunction %void None %3\n"
					<< "          %5 = OpLabel\n";

				for (deUint32 i = 1; i < topologyData.at(m_parameters.primTopology).primSize + 1; i++)
				{
					src << "%" << i << "1 = OpAccessChain %_ptr_Input_v4float %in0 %int_" << i << "\n"
						<< "          %" << i << "2 = OpLoad %v4float %" << i << "1\n"
						<< "          %" << i << "3 = OpAccessChain %_ptr_Output_v4float %outBlock %int_0\n"
						<< "               OpStore %" << i << "3 %" << i << "2\n"
						<< "               OpEmitStreamVertex %streamNum\n";
				}

				src << "               OpEndStreamPrimitive %streamNum\n"
					<< "               OpReturn\n"
					<< "               OpFunctionEnd\n"
					;

				programCollection.spirvAsmSources.add("geom") << src.str();
			}
			else
			{
				src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
					<< "\n"
					<< "layout(" << primitiveNames.at(m_parameters.primTopology).glslIn << ") in;\n"
					<< "layout(location = 0) in vec4 in0[];\n"
					<< "\n"
					<< "layout(" << primitiveNames.at(m_parameters.primTopology).glslOut << ", max_vertices = " << topologyData.at(m_parameters.primTopology).primSize << ") out;\n"
					<< "layout(stream = " << s << ", xfb_buffer = 0, xfb_offset = 0, xfb_stride = 16, location = 0) out vec4 out0;\n"
					<< "\n"
					<< "void main(void)\n"
					<< "{\n";

				for (deUint32 i = 0; i < topologyData.at(m_parameters.primTopology).primSize; i++)
				{
					src << "    out0 = in0[" << i << "];\n"
						<< "    EmitStreamVertex(" << s << ");\n";
				}

				src << "    EndStreamPrimitive(" << s << ");\n"
					<< "}\n";

				programCollection.glslSources.add("geom") << glu::GeometrySource(src.str());
			}
		}

		return;
	}

	if (m_parameters.testType == TEST_TYPE_MULTIQUERY)
	{
		// vertex shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(location = 0) out ivec4 out0;\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    out0 = ivec4(gl_VertexIndex);\n"
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		// geometry shader
		{
			const deUint32		s	= m_parameters.streamId;
			std::ostringstream	src;

			DE_ASSERT(s != 0);

			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "\n"
				<< "layout(points) in;\n"
				<< "\n"
				<< "layout(points, max_vertices = 4) out;\n"
				<< "\n"
				<< "layout(stream = " << 0 << ", xfb_buffer = 0, xfb_offset = 0, xfb_stride = 16, location = 0) out vec4 out0;\n"
				<< "layout(stream = " << s << ", xfb_buffer = 1, xfb_offset = 0, xfb_stride = 16, location = 1) out vec4 out1;\n"
				<< "\n"
				<< "void main(void)\n"
				<< "{\n"
				<< "    const int   n0 = 3;\n"
				<< "    const int   n1 = 1;\n"
				<< "    const float c0 = 0.5f;\n"
				<< "    const float c1 = 0.5f + float(" << s << ");\n"
				<< "\n"
				<< "    for (int j = 0; j < n0; j++)\n"
				<< "    {\n"
				<< "        out0 = vec4(c0);\n"
				<< "        EmitStreamVertex(0);\n"
				<< "        EndStreamPrimitive(0);\n"
				<< "    }\n"
				<< "\n"
				<< "    for (int j = 0; j < n1; j++)\n"
				<< "    {\n"
				<< "        out1 = vec4(c1);\n"
				<< "        EmitStreamVertex(" << s << ");\n"
				<< "        EndStreamPrimitive(" << s << ");\n"
				<< "    }\n"
				<< "}\n";

			programCollection.glslSources.add("geom") << glu::GeometrySource(src.str());
		}

		return;
	}

	DE_ASSERT(0 && "Unknown test");
}

void createTransformFeedbackSimpleTests(tcu::TestCaseGroup* group)
{
	{
		const deUint32		bufferCounts[]	= { 1u, 2u, 4u, 8u };
		const deUint32		bufferSizes[]	= { 256u, 512u, 128u * 1024u };
		const TestType		testTypes[]		= { TEST_TYPE_BASIC, TEST_TYPE_RESUME, TEST_TYPE_XFB_POINTSIZE, TEST_TYPE_XFB_CLIPDISTANCE, TEST_TYPE_XFB_CULLDISTANCE, TEST_TYPE_XFB_CLIP_AND_CULL };
		const std::string	testTypeNames[]	= { "basic", "resume", "xfb_pointsize", "xfb_clipdistance", "xfb_culldistance", "xfb_clip_and_cull" };

		for (deUint32 testTypesNdx = 0; testTypesNdx < DE_LENGTH_OF_ARRAY(testTypes); ++testTypesNdx)
		{
			const TestType		testType	= testTypes[testTypesNdx];
			const std::string	testName	= testTypeNames[testTypesNdx];

			for (deUint32 bufferCountsNdx = 0; bufferCountsNdx < DE_LENGTH_OF_ARRAY(bufferCounts); ++bufferCountsNdx)
			{
				const deUint32	partCount	= bufferCounts[bufferCountsNdx];

				for (deUint32 bufferSizesNdx = 0; bufferSizesNdx < DE_LENGTH_OF_ARRAY(bufferSizes); ++bufferSizesNdx)
				{
					const deUint32	bufferSize	= bufferSizes[bufferSizesNdx];
					TestParameters	parameters	= { testType, bufferSize, partCount, 0u, 0u, 0u, STREAM_ID_0_NORMAL, false, false, VK_PRIMITIVE_TOPOLOGY_POINT_LIST };

					group->addChild(new TransformFeedbackTestCase(group->getTestContext(), (testName + "_" + de::toString(partCount) + "_" + de::toString(bufferSize)).c_str(), "Simple Transform Feedback test", parameters));
					parameters.streamId0Mode = STREAM_ID_0_BEGIN_QUERY_INDEXED;
					group->addChild(new TransformFeedbackTestCase(group->getTestContext(), (testName + "_beginqueryindexed_streamid_0_" + de::toString(partCount) + "_" + de::toString(bufferSize)).c_str(), "Simple Transform Feedback test", parameters));
					parameters.streamId0Mode = STREAM_ID_0_END_QUERY_INDEXED;
					group->addChild(new TransformFeedbackTestCase(group->getTestContext(), (testName + "_endqueryindexed_streamid_0_" + de::toString(partCount) + "_" + de::toString(bufferSize)).c_str(), "Simple Transform Feedback test", parameters));
				}
			}
		}
	}

	{
		const deUint32		bufferCounts[]	= { 6u, 8u, 10u, 12u };
		const TestType		testType		= TEST_TYPE_WINDING;
		const std::string	testName		= "winding";

		for (const auto& topology : topologyData)
		{
			// Note: no need to test POINT_LIST as is tested in many tests.
			if (topology.first == VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
				continue;

			for (deUint32 bufferCountsNdx = 0; bufferCountsNdx < DE_LENGTH_OF_ARRAY(bufferCounts); ++bufferCountsNdx)
			{
				const deUint32	vertexCount	= bufferCounts[bufferCountsNdx];

				TestParameters	parameters	= { testType, 0u, vertexCount, 0u, 0u, 0u, STREAM_ID_0_NORMAL, false, false, topology.first };

				group->addChild(new TransformFeedbackTestCase(group->getTestContext(), (testName + "_" + topology.second.topologyName + de::toString(vertexCount)).c_str(), "Topology winding test", parameters));
			}
		}
	}

	{
		const deUint32		vertexStrides[]	= { 4u, 61u, 127u, 251u, 509u };
		const TestType		testType		= TEST_TYPE_DRAW_INDIRECT;
		const std::string	testName		= "draw_indirect";

		for (deUint32 vertexStridesNdx = 0; vertexStridesNdx < DE_LENGTH_OF_ARRAY(vertexStrides); ++vertexStridesNdx)
		{
			const deUint32	vertexStrideBytes	= static_cast<deUint32>(sizeof(deUint32) * vertexStrides[vertexStridesNdx]);
			TestParameters	parameters			= { testType, 0u, 0u, 0u, 0u, vertexStrideBytes, STREAM_ID_0_NORMAL, false, false, VK_PRIMITIVE_TOPOLOGY_POINT_LIST };

			group->addChild(new TransformFeedbackTestCase(group->getTestContext(), (testName + "_" + de::toString(vertexStrideBytes)).c_str(), "Rendering tests with various strides", parameters));
			parameters.streamId0Mode = STREAM_ID_0_BEGIN_QUERY_INDEXED;
			group->addChild(new TransformFeedbackTestCase(group->getTestContext(), (testName + "_beginqueryindexed_streamid_0_" + de::toString(vertexStrideBytes)).c_str(), "Rendering tests with various strides", parameters));
			parameters.streamId0Mode = STREAM_ID_0_END_QUERY_INDEXED;
			group->addChild(new TransformFeedbackTestCase(group->getTestContext(), (testName + "_endqueryindexed_streamid_0_" + de::toString(vertexStrideBytes)).c_str(), "Rendering tests with various strides", parameters));
		}
	}

	{
		const TestType		testType	= TEST_TYPE_BACKWARD_DEPENDENCY;
		const std::string	testName	= "backward_dependency";
		TestParameters		parameters	= { testType, 512u, 2u, 0u, 0u, 0u, STREAM_ID_0_NORMAL, false, false, VK_PRIMITIVE_TOPOLOGY_POINT_LIST };

		group->addChild(new TransformFeedbackTestCase(group->getTestContext(), testName.c_str(), "Rendering test checks backward pipeline dependency", parameters));
		parameters.streamId0Mode = STREAM_ID_0_BEGIN_QUERY_INDEXED;
		group->addChild(new TransformFeedbackTestCase(group->getTestContext(), (testName + "_beginqueryindexed_streamid_0").c_str(), "Rendering test checks backward pipeline dependency", parameters));
		parameters.streamId0Mode = STREAM_ID_0_END_QUERY_INDEXED;
		group->addChild(new TransformFeedbackTestCase(group->getTestContext(), (testName + "_endqueryindexed_streamid_0").c_str(), "Rendering test checks backward pipeline dependency", parameters));

		parameters.noOffsetArray = true;
		group->addChild(new TransformFeedbackTestCase(group->getTestContext(), (testName + "_no_offset_array").c_str(), "Rendering test checks backward pipeline dependency (using NULL for offset array)", parameters));
	}

	{
		const deUint32		usedStreamId[]			= { 0u, 1u, 3u, 6u, 14u };
		const deUint32		vertexCounts[]			= { 6u, 61u, 127u, 251u, 509u }; // Lowest value has to be at least 6. Otherwise the triangles with adjacency can't be generated.
		const TestType		testType				= TEST_TYPE_QUERY_GET;
		const std::string	testName				= "query";
		const TestType		testTypeCopy			= TEST_TYPE_QUERY_COPY;
		const std::string	testNameCopy			= "query_copy";
		const TestType		testTypeHostQueryReset	= TEST_TYPE_QUERY_RESET;
		const std::string	testNameHostQueryReset	= "host_query_reset";

		for (const auto& topology : topologyData)
		{
			// Currently, we don't test tessellation here.
			if (topology.first == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
				continue;

			for (const auto& streamCounts : usedStreamId)
			{
				const deUint32	streamId	= streamCounts;

				for (const auto& numVertices : vertexCounts)
				{
					for (deUint32 i = 0; i < 2; ++i)
					{
						const bool				query64Bits		= (i == 1);
						const std::string		widthStr		= (query64Bits ? "_64bits" : "_32bits");

						deUint32				vertCount		= numVertices;

						// The number of vertices in original test was 4.
						if (topology.first == VK_PRIMITIVE_TOPOLOGY_POINT_LIST && vertCount == 6) vertCount -= 2;

						// Round the number of vertices to match the used primitive topology - if necessary.
						const deUint32			primitiveCount	= (deUint32)topology.second.getNumPrimitives(vertCount);
						const deUint32			vertexCount		= (deUint32)topology.second.getNumVertices(primitiveCount);

						DE_ASSERT(vertexCount > 0);

						const deUint32			bytesPerVertex	= static_cast<deUint32>(4 * sizeof(float));
						const deUint32			bufferSize		= bytesPerVertex * vertexCount;
						TestParameters			parameters		= { testType, bufferSize, 0u, streamId, 0u, 0u, STREAM_ID_0_NORMAL, query64Bits, false, topology.first };
						const std::string		fullTestName	= testName + "_" + topology.second.topologyName + de::toString(streamId) + "_" + de::toString(vertexCount) + widthStr;
						group->addChild(new TransformFeedbackTestCase(group->getTestContext(), fullTestName.c_str(), "Written primitives query test", parameters));

						const TestParameters	parametersCopy		= { testTypeCopy, bufferSize, 0u, streamId, 0u, 0u, STREAM_ID_0_NORMAL, query64Bits, false, topology.first };
						const std::string		fullTestNameCopy	= testNameCopy + "_" + topology.second.topologyName + de::toString(streamId) + "_" + de::toString(vertexCount) + widthStr;
						group->addChild(new TransformFeedbackTestCase(group->getTestContext(), fullTestNameCopy.c_str(), "Written primitives query test", parametersCopy));

						const TestParameters	parametersHostQueryReset	= { testTypeHostQueryReset, bufferSize, 0u, streamId, 0u, 0u, STREAM_ID_0_NORMAL, query64Bits, false, topology.first };
						const std::string		fullTestNameHostQueryReset	= testNameHostQueryReset + "_" + topology.second.topologyName + de::toString(streamId) + "_" + de::toString(vertexCount) + widthStr;
						group->addChild(new TransformFeedbackTestCase(group->getTestContext(), fullTestNameHostQueryReset.c_str(), "Written primitives query test", parametersHostQueryReset));

						if (streamId == 0)
						{
							std::string	testNameStream0 = fullTestName;
							testNameStream0 += "_beginqueryindexed_streamid_0";
							parameters.streamId0Mode = STREAM_ID_0_BEGIN_QUERY_INDEXED;
							group->addChild(new TransformFeedbackTestCase(group->getTestContext(), testNameStream0.c_str(), "Written primitives query test", parameters));
							testNameStream0 = fullTestName;
							testNameStream0 += "_endqueryindexed_streamid_0";
							parameters.streamId0Mode = STREAM_ID_0_END_QUERY_INDEXED;
							group->addChild(new TransformFeedbackTestCase(group->getTestContext(), testNameStream0.c_str(), "Written primitives query test", parameters));
						}
					}
				}
			}
		}
	}
}

void createTransformFeedbackStreamsSimpleTests (tcu::TestCaseGroup* group)
{
	const deUint32		usedStreamId[]		= { 1u, 3u, 6u, 14u };
	const TestType		testTypes[]			= { TEST_TYPE_STREAMS, TEST_TYPE_STREAMS_POINTSIZE, TEST_TYPE_STREAMS_CLIPDISTANCE, TEST_TYPE_STREAMS_CULLDISTANCE };
	const std::string	testTypeNames[]		= { "streams", "streams_pointsize", "streams_clipdistance", "streams_culldistance" };

	for (deUint32 testTypesNdx = 0; testTypesNdx < DE_LENGTH_OF_ARRAY(testTypes); ++testTypesNdx)
	{
		const TestType		testType	= testTypes[testTypesNdx];
		const std::string	testName	= testTypeNames[testTypesNdx];
		const deUint32		pointSize	= (testType == TEST_TYPE_STREAMS_POINTSIZE) ? 2u : 0u;

		for (deUint32 streamCountsNdx = 0; streamCountsNdx < DE_LENGTH_OF_ARRAY(usedStreamId); ++streamCountsNdx)
		{
			const deUint32	streamId	= usedStreamId[streamCountsNdx];
			TestParameters	parameters	= { testType, 0u, 0u, streamId, pointSize, 0u, STREAM_ID_0_NORMAL, false, false, VK_PRIMITIVE_TOPOLOGY_POINT_LIST };

			group->addChild(new TransformFeedbackTestCase(group->getTestContext(), (testName + "_" + de::toString(streamId)).c_str(), "Streams usage test", parameters));
		}
	}

	{
		const TestType		testType	= TEST_TYPE_MULTISTREAMS;
		const std::string	testName	= "multistreams";

		for (deUint32 bufferCountsNdx = 0; bufferCountsNdx < DE_LENGTH_OF_ARRAY(usedStreamId); ++bufferCountsNdx)
		{
			const deUint32			streamId			= usedStreamId[bufferCountsNdx];
			const deUint32			streamsUsed			= 2u;
			const deUint32			maxBytesPerVertex	= 256u;
			const TestParameters	parameters			= { testType, maxBytesPerVertex * streamsUsed, streamsUsed, streamId, 0u, 0u, STREAM_ID_0_NORMAL, false, false, VK_PRIMITIVE_TOPOLOGY_POINT_LIST };

			group->addChild(new TransformFeedbackTestCase(group->getTestContext(), (testName + "_" + de::toString(streamId)).c_str(), "Simultaneous multiple streams usage test", parameters));
		}
	}

	{
		const TestType		testType	= TEST_TYPE_MULTIQUERY;
		const std::string	testName	= "multiquery";

		for (deUint32 bufferCountsNdx = 0; bufferCountsNdx < DE_LENGTH_OF_ARRAY(usedStreamId); ++bufferCountsNdx)
		{
			const deUint32			streamId			= usedStreamId[bufferCountsNdx];
			const deUint32			streamsUsed			= 2u;
			const deUint32			maxBytesPerVertex	= 256u;
			const TestParameters	parameters			= { testType, maxBytesPerVertex * streamsUsed, streamsUsed, streamId, 0u, 0u, STREAM_ID_0_NORMAL, false, false, VK_PRIMITIVE_TOPOLOGY_POINT_LIST };

			group->addChild(new TransformFeedbackTestCase(group->getTestContext(), (testName + "_" + de::toString(streamId)).c_str(), "Simultaneous multiple queries usage test", parameters));
		}
	}
}

void createTransformFeedbackAndStreamsSimpleTests (tcu::TestCaseGroup* group)
{
	createTransformFeedbackSimpleTests(group);
	createTransformFeedbackStreamsSimpleTests(group);
}
} // anonymous

tcu::TestCaseGroup* createTransformFeedbackSimpleTests (tcu::TestContext& testCtx)
{
	return createTestGroup(testCtx, "simple", "Transform Feedback Simple tests", createTransformFeedbackAndStreamsSimpleTests);
}

} // TransformFeedback
} // vkt
