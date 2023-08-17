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
 * \brief Fragment Shading Barycentric extention tests
 *//*--------------------------------------------------------------------*/

#include "vktFragmentShadingBarycentricTests.hpp"

#include "deDefs.h"
#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkPipelineConstructionUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuImageCompare.hpp"
#include "tcuVectorUtil.hpp"

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>
#include <map>

namespace vkt
{
namespace FragmentShadingBarycentric
{
namespace
{
using namespace vk;
using namespace vkt;

using std::string;
using std::vector;
using std::map;
using de::MovePtr;
using tcu::mix;

enum TestType
{
	TEST_TYPE_DATA		= 0,
	TEST_TYPE_WEIGHTS,
};

enum TestSubtype
{
	TEST_SUBTYPE_DEFAULT = 0,
	TEST_SUBTYPE_MSAA_INTERPOLATE_AT_CENTROID,
	TEST_SUBTYPE_MSAA_INTERPOLATE_AT_SAMPLE,
	TEST_SUBTYPE_MSAA_INTERPOLATE_AT_OFFSET,
	TEST_SUBTYPE_MSAA_CENTROID_QUALIFIER,
	TEST_SUBTYPE_MSAA_SAMPLE_QUALIFIER,
	TEST_SUBTYPE_PERVERTEX_CORRECTNESS,
	TEST_SUBTYPE_TESS_SHADER,
	TEST_SUBTYPE_GEOMETRY_SHADER,
	TEST_SUBTYPE_TESSGEOM_SHADER,
};

const char* getShaderComboName(uint32_t testSubType)
{
	uint32_t idx = testSubType - TEST_SUBTYPE_TESS_SHADER;

	DE_ASSERT(idx < 3);

	static const char* sc_names[] =
	{
		"with_tess_shader",
		"with_geom_shader",
		"with_tess_geom_shader",
	};

	return sc_names[idx];
}

const size_t	DATA_TEST_WIDTH		= 8u;
const size_t	DATA_TEST_HEIGHT	= 8u;
const size_t	WEIGHT_TEST_WIDTH	= 128u;
const size_t	WEIGHT_TEST_HEIGHT	= 128u;
const float		WEIGHT_TEST_SLOPE	= 16.0f;

struct TestParams
{
	const PipelineConstructionType	pipelineConstructionType;
	TestType						testType;
	TestSubtype						testSubtype;
	VkPrimitiveTopology				topology;
	bool							dynamicIndexing;
	size_t							aggregate; // 0: value itself, 1:struct, 2+:Array
	glu::DataType					dataType;
	size_t							width;
	size_t							height;
	bool							perspective;
	bool							provokingVertexLast;
	uint32_t						rotation;
	bool							dynamicTopologyInPipeline;
	VkSampleCountFlagBits			sampleCount;
};

size_t getComponentCount (const TestParams& testParams)
{
	const size_t	scalarSize		= static_cast<size_t>(getDataTypeScalarSize(testParams.dataType));
	const size_t	aggregateSize	= (testParams.aggregate > 0) ? testParams.aggregate : 1;
	const size_t	topologySize	= 3; // Test always check three items in array: "Reads of per-vertex values for missing vertices, such as the third vertex of a line primitive, will return zero."
	const size_t	result			= scalarSize * aggregateSize * topologySize;

	return result;
}

static VkImageCreateInfo makeImageCreateInfo (const VkFormat format, const uint32_t width, uint32_t height, VkSampleCountFlagBits samples)
{
	const VkImageUsageFlags	usage			= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	const VkImageCreateInfo	imageCreateInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		0u,										// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
		format,									// VkFormat					format;
		makeExtent3D(width, height, 1u),		// VkExtent3D				extent;
		1u,										// uint32_t					mipLevels;
		1u,										// uint32_t					arrayLayers;
		samples,								// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		usage,									// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		0u,										// uint32_t					queueFamilyIndexCount;
		DE_NULL,								// const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			initialLayout;
	};

	return imageCreateInfo;
}

static Move<VkRenderPass> makeRenderPass(const DeviceInterface& vk, const VkDevice device, const VkFormat format, VkSampleCountFlagBits samples)
{
	const VkAttachmentDescription attachmentSingleSampleDesc
	{
		(VkAttachmentDescriptionFlags)0u,			// VkAttachmentDescriptionFlags		flags;
		format,										// VkFormat							format;
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits			samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp				stencilStoreOp;
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout					initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout					finalLayout;
	};

	const VkAttachmentDescription attachmentMultiSampleDesc
	{
		(VkAttachmentDescriptionFlags)0u,			// VkAttachmentDescriptionFlags		flags;
		format,										// VkFormat							format;
		samples,									// VkSampleCountFlagBits			samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp				stencilStoreOp;
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout					initialLayout;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout					finalLayout;
	};

	const VkAttachmentDescription attachments[] = { attachmentSingleSampleDesc, attachmentMultiSampleDesc };

	const VkAttachmentReference attachmentSingleSampleRef
	{
		0u,											// deUint32			attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout;
	};

	const VkAttachmentReference attachmentMultiSampleRef
	{
		1u,											// deUint32			attachment;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout	layout;
	};

	const bool useMultisampling = (samples > VK_SAMPLE_COUNT_1_BIT);
	const VkAttachmentReference* colorAttachment	= useMultisampling ? &attachmentMultiSampleRef : &attachmentSingleSampleRef;
	const VkAttachmentReference* resolveAttachment	= useMultisampling ? &attachmentSingleSampleRef : DE_NULL;

	const VkSubpassDescription subpassDescription
	{
		(VkSubpassDescriptionFlags)0u,				// VkSubpassDescriptionFlags		flags;
		VK_PIPELINE_BIND_POINT_GRAPHICS,			// VkPipelineBindPoint				pipelineBindPoint;
		0u,											// deUint32							inputAttachmentCount;
		DE_NULL,									// const VkAttachmentReference*		pInputAttachments;
		1u,											// deUint32							colorAttachmentCount;
		colorAttachment,							// const VkAttachmentReference*		pColorAttachments;
		resolveAttachment,							// const VkAttachmentReference*		pResolveAttachments;
		DE_NULL,									// const VkAttachmentReference*		pDepthStencilAttachment;
		0u,											// deUint32							preserveAttachmentCount;
		DE_NULL										// const deUint32*					pPreserveAttachments;
	};

	const VkRenderPassCreateInfo renderPassInfo
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		(VkRenderPassCreateFlags)0u,						// VkRenderPassCreateFlags			flags;
		1u + useMultisampling,								// deUint32							attachmentCount;
		attachments,										// const VkAttachmentDescription*	pAttachments;
		1u,													// deUint32							subpassCount;
		&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
		0u,													// deUint32							dependencyCount;
		DE_NULL												// const VkSubpassDependency*		pDependencies;
	};

	return createRenderPass(vk, device, &renderPassInfo, DE_NULL);
}

using GraphicsPipelinePtr = std::unique_ptr<GraphicsPipelineWrapper>;

static GraphicsPipelinePtr makeGraphicsPipeline (PipelineConstructionType			pipelineConstructionType,
												 const InstanceInterface&			vki,
												 const DeviceInterface&				vkd,
												 const VkPhysicalDevice				physicalDevice,
												 const VkDevice						device,
												 const std::vector<std::string>&	deviceExtensions,
												 const PipelineLayoutWrapper&		pipelineLayout,
												 const VkRenderPass					renderPass,
												 const ShaderWrapper				vertShaderModule,
												 const ShaderWrapper				fragShaderModule,
												 const uint32_t						width,
												 const uint32_t						height,
												 const VkPrimitiveTopology			topology,
												 const VkSampleCountFlagBits		rasterizationSamples,
												 const bool							withColor = false,
												 const bool							provokingVertexLast = false,
												 const bool							dynamicTopology = false,
												 const ShaderWrapper				tessCtrlShaderModule = ShaderWrapper(),
												 const ShaderWrapper				tessEvalShaderModule = ShaderWrapper(),
												 const ShaderWrapper				geometryShaderModule = ShaderWrapper())
{
	const std::vector<VkViewport>									viewports							(1, makeViewport(width, height));
	const std::vector<VkRect2D>										scissors							(1, makeRect2D(width, height));
	const VkVertexInputBindingDescription							vertexInputBindingDescription		=
	{
		0u,										// uint32_t				binding;
		2 * sizeof(tcu::Vec4),					// uint32_t				stride;
		VK_VERTEX_INPUT_RATE_VERTEX,			// VkVertexInputRate	inputRate;
	};
	const VkVertexInputAttributeDescription							vertexInputAttributeDescriptions[]	=
	{
		{
			0u,										// uint32_t		location;
			0u,										// uint32_t		binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,			// VkFormat		format;
			0u										// uint32_t		offset;
		},
		{
			1u,										// uint32_t		location;
			0u,										// uint32_t		binding;
			VK_FORMAT_R32G32B32A32_SFLOAT,			// VkFormat		format;
			sizeof(tcu::Vec4)						// uint32_t		offset;
		},
	};
	const VkPipelineVertexInputStateCreateInfo						vertexInputStateInfo				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType								sType;
		DE_NULL,														// const void*									pNext;
		(VkPipelineVertexInputStateCreateFlags)0,						// VkPipelineVertexInputStateCreateFlags		flags;
		1u,																// uint32_t										vertexBindingDescriptionCount;
		&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
		DE_LENGTH_OF_ARRAY(vertexInputAttributeDescriptions),			// uint32_t										vertexAttributeDescriptionCount;
		vertexInputAttributeDescriptions,								// const VkVertexInputAttributeDescription*		pVertexAttributeDescriptions;
	};
	const VkPipelineRasterizationProvokingVertexStateCreateInfoEXT	provokingVertexStateCreateInfoEXT	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT,	//  VkStructureType				sType;
		DE_NULL,																			//  const void*					pNext;
		VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT,											//  VkProvokingVertexModeEXT	provokingVertexMode;
	};
	const VkPipelineRasterizationStateCreateInfo					rasterizationStateCreateInfo		=
	{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,				//  VkStructureType							sType;
		provokingVertexLast ? &provokingVertexStateCreateInfoEXT : DE_NULL,		//  const void*								pNext;
		0u,																		//  VkPipelineRasterizationStateCreateFlags	flags;
		VK_FALSE,																//  VkBool32								depthClampEnable;
		DE_FALSE,																//  VkBool32								rasterizerDiscardEnable;
		VK_POLYGON_MODE_FILL,													//  VkPolygonMode							polygonMode;
		VK_CULL_MODE_NONE,														//  VkCullModeFlags							cullMode;
		VK_FRONT_FACE_COUNTER_CLOCKWISE,										//  VkFrontFace								frontFace;
		VK_FALSE,																//  VkBool32								depthBiasEnable;
		0.0f,																	//  float									depthBiasConstantFactor;
		0.0f,																	//  float									depthBiasClamp;
		0.0f,																	//  float									depthBiasSlopeFactor;
		1.0f																	//  float									lineWidth;
	};
	const bool									isMultiSample			= (rasterizationSamples > VK_SAMPLE_COUNT_1_BIT);
	const VkPipelineMultisampleStateCreateInfo	multisampleStateInfo
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,				// VkStructureType							sType;
		nullptr,																// const void*								pNext;
		0u,																		// VkPipelineMultisampleStateCreateFlags	flags;
		rasterizationSamples,													// VkSampleCountFlagBits					rasterizationSamples;
		(isMultiSample ? VK_TRUE : VK_FALSE),									// VkBool32									sampleShadingEnable;
		1.0f,																	// float									minSampleShading;
		DE_NULL,																// const VkSampleMask*						pSampleMask;
		VK_FALSE,																// VkBool32									alphaToCoverageEnable;
		VK_FALSE,																// VkBool32									alphaToOneEnable;
	};
	const VkDynamicState											dynamicStates[]						=
	{
		VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
	};
	const VkPipelineDynamicStateCreateInfo							dynamicStateCreateInfo				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//  VkStructureType						sType;
		DE_NULL,												//  const void*							pNext;
		0u,														//  VkPipelineDynamicStateCreateFlags	flags;
		DE_LENGTH_OF_ARRAY(dynamicStates),						//  uint32_t							dynamicStateCount;
		dynamicStates,											//  const VkDynamicState*				pDynamicStates;
	};
	const VkPipelineDynamicStateCreateInfo*							pDynamicStateCreateInfo				= dynamicTopology ? &dynamicStateCreateInfo : DE_NULL;
	const auto														pVertexInputStateCreateInfo			= (withColor ? &vertexInputStateInfo : nullptr);

	GraphicsPipelinePtr	pipelineWrapperPtr	(new GraphicsPipelineWrapper(vki, vkd, physicalDevice, device, deviceExtensions, pipelineConstructionType));
	auto&				pipelineWrapper		= *pipelineWrapperPtr.get();

	pipelineWrapper
		.setMonolithicPipelineLayout(pipelineLayout)
		.setDefaultDepthStencilState()
		.setDefaultColorBlendState()
		.setDefaultTopology(topology)
		.setDynamicState(pDynamicStateCreateInfo)
		.setupVertexInputState(pVertexInputStateCreateInfo)
		.setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass, 0u, vertShaderModule, &rasterizationStateCreateInfo,
										tessCtrlShaderModule, tessEvalShaderModule, geometryShaderModule)
		.setupFragmentShaderState(pipelineLayout, renderPass, 0u, fragShaderModule)
		.setupFragmentOutputState(renderPass, 0u, nullptr, &multisampleStateInfo)
		.buildPipeline();

	return pipelineWrapperPtr;
}

// Function replacing all occurrences of substring with string passed in last parameter.
static inline std::string replace(const std::string& str, const std::string& from, const std::string& to)
{
	std::string result(str);

	size_t start_pos = 0;
	while ((start_pos = result.find(from, start_pos)) != std::string::npos)
	{
		result.replace(start_pos, from.length(), to);
		start_pos += to.length();
	}

	return result;
}


class FragmentShadingBarycentricDataTestInstance : public TestInstance
{
public:
								FragmentShadingBarycentricDataTestInstance	(Context&			context,
																			 const TestParams&	testParams);
	virtual						~FragmentShadingBarycentricDataTestInstance	();
	virtual tcu::TestStatus		iterate										(void);

protected:
	vector<tcu::Vec4>			generateVertexBuffer						(void);
	MovePtr<BufferWithMemory>	createVertexBuffer							(const vector<tcu::Vec4>& vertices);
	bool						verify										(BufferWithMemory* resultBuffer);
	bool						getProvokingVertexLast						(void);

	TestParams					m_testParams;
};

FragmentShadingBarycentricDataTestInstance::FragmentShadingBarycentricDataTestInstance (Context& context, const TestParams& testParams)
	: TestInstance	(context)
	, m_testParams	(testParams)
{
}

FragmentShadingBarycentricDataTestInstance::~FragmentShadingBarycentricDataTestInstance ()
{
}

vector<tcu::Vec4> FragmentShadingBarycentricDataTestInstance::generateVertexBuffer (void)
{
	size_t				verticesCount	=	static_cast<size_t>(~0ull);
	vector<tcu::Vec4>	result;

	switch (m_testParams.topology)
	{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
		{
			verticesCount = m_testParams.width * m_testParams.height;

			result.reserve(verticesCount);

			for (size_t y = 0; y < m_testParams.height; y++)
			{
				const float yy = -1.0f + 2.0f * ((0.5f + float(y)) / float(m_testParams.height));

				for (size_t x = 0; x < m_testParams.width; x++)
				{
					const float xx = -1.0f + 2.0f * ((0.5f + float(x)) / float(m_testParams.width));

					result.push_back(tcu::Vec4(xx, yy, 0.0f, 1.0f));
				}
			}

			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		{
			verticesCount = 2 * m_testParams.height;

			result.reserve(verticesCount);

			for (size_t y = 0; y < m_testParams.height; y++)
			{
				const float yy = -1.0f + 2.0f * ((0.5f + float(y)) / float(m_testParams.height));

				result.push_back(tcu::Vec4(-1.0f, yy, 0.0f, 1.0f));
				result.push_back(tcu::Vec4( 1.0f, yy, 0.0f, 1.0f));
			}

			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		{
			verticesCount = 2 * m_testParams.height;

			result.reserve(verticesCount);

			for (size_t y = 0; y < m_testParams.height; y++)
			{
				const float yy = -1.0f + 2.0f * (0.5f + float(y)) / float(m_testParams.height);;

				if (y % 2 == 0)
				{
					result.push_back(tcu::Vec4(-2.0f, yy, 0.0f, 1.0f));
					result.push_back(tcu::Vec4(+2.0f, yy, 0.0f, 1.0f));
				}
				else
				{
					result.push_back(tcu::Vec4(+2.0f, yy, 0.0f, 1.0f));
					result.push_back(tcu::Vec4(-2.0f, yy, 0.0f, 1.0f));
				}
			}

			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		{
			verticesCount = 6;

			result.reserve(verticesCount);

			result.push_back(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(-1.0f, +1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(+1.0f, +1.0f, 0.0f, 1.0f));

			result.push_back(tcu::Vec4(+1.0f, +1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(+1.0f, -1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f));

			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		{
			verticesCount = 4;

			result.reserve(verticesCount);

			result.push_back(tcu::Vec4(-1.0f, +1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(+1.0f, +1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(+1.0f, -1.0f, 0.0f, 1.0f));

			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
		{
			verticesCount = 4;

			result.reserve(verticesCount);

			result.push_back(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(-1.0f, +1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(+1.0f, +1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(+1.0f, -1.0f, 0.0f, 1.0f));

			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		{
			verticesCount = 4 * m_testParams.height;

			result.reserve(verticesCount);

			for (size_t y = 0; y < m_testParams.height; y++)
			{
				const float yy = -1.0f + 2.0f * ((0.5f + float(y)) / float(m_testParams.height));

				result.push_back(tcu::Vec4(-2.0f, yy, 0.0f, 1.0f));
				result.push_back(tcu::Vec4(-1.0f, yy, 0.0f, 1.0f));
				result.push_back(tcu::Vec4( 1.0f, yy, 0.0f, 1.0f));
				result.push_back(tcu::Vec4( 2.0f, yy, 0.0f, 1.0f));
			}

			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
		{
			verticesCount = 2 * m_testParams.height + 2;

			result.reserve(verticesCount);

			result.push_back(tcu::Vec4(-10.0f, -10.0f, 0.0f, 1.0f));

			for (size_t y = 0; y < m_testParams.height; y++)
			{
				const float ky = (0.5f + float(y)) / float(m_testParams.height);
				const float yy = -1.0f + 2.0f * ky;

				if (y % 2 == 0)
				{
					result.push_back(tcu::Vec4(-2.0f, yy, 0.0f, 1.0f));
					result.push_back(tcu::Vec4(+2.0f, yy, 0.0f, 1.0f));
				}
				else
				{
					result.push_back(tcu::Vec4(+2.0f, yy, 0.0f, 1.0f));
					result.push_back(tcu::Vec4(-2.0f, yy, 0.0f, 1.0f));
				}
			}

			result.push_back(tcu::Vec4(+10.0f, +10.0f, 0.0f, 1.0f));

			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		{
			verticesCount = 12;

			result.reserve(verticesCount);

			result.push_back(tcu::Vec4(-1.0f, +1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(+1.0f, +3.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(+1.0f, +1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(+1.0f, -1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(-3.0f, -1.0f, 0.0f, 1.0f));

			result.push_back(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(-1.0f, +1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(+1.0f, +1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(+3.0f, +1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(+1.0f, -1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(-1.0f, -3.0f, 0.0f, 1.0f));

			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
		{
			verticesCount = 8;

			result.reserve(verticesCount);

			result.push_back(tcu::Vec4(-1.0f, +1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(+1.0f, +3.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(+1.0f, +1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(-3.0f, -1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(+3.0f, +1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(+1.0f, -1.0f, 0.0f, 1.0f));
			result.push_back(tcu::Vec4(-1.0f, -3.0f, 0.0f, 1.0f));

			break;
		}

		default:
			TCU_THROW(InternalError, "Unknown topology");
	}

	DE_ASSERT(result.size() == verticesCount);

	return result;
}

bool FragmentShadingBarycentricDataTestInstance::verify (BufferWithMemory* resultBuffer)
{
	const size_t	components	= getComponentCount(m_testParams);
	const deUint32	expected	= m_testParams.testSubtype == TEST_SUBTYPE_PERVERTEX_CORRECTNESS ? 10u : static_cast<deUint32>(1 << components) - 1;
	const deUint32* retrieved	= (deUint32*)resultBuffer->getAllocation().getHostPtr();
	size_t			failures	= 0;

	{
		size_t n = 0;

		for (size_t y = 0; y < m_testParams.height; y++)
		for (size_t x = 0; x < m_testParams.width; x++)
		{
			if (retrieved[n] != expected)
				failures++;

			n++ ;
		}
	}

	if (failures)
	{
		const uint8_t			places	= static_cast<uint8_t>(components / 4);
		tcu::TestLog&			log		= m_context.getTestContext().getLog();
		size_t					n		= 0;
		std::ostringstream		s;

		s << "Expected mask:" << std::setfill('0') << std::hex << std::setw(places) << expected << std::endl;

		for (size_t y = 0; y < m_testParams.height; y++)
		{
			for (size_t x = 0; x < m_testParams.width; x++)
			{
				s << std::setw(places) << retrieved[n] << ' ';

				n++;
			}

			s << std::endl;
		}

		log << tcu::TestLog::Message << s.str() << tcu::TestLog::EndMessage;
	}

	return failures == 0;
}

MovePtr<BufferWithMemory> FragmentShadingBarycentricDataTestInstance::createVertexBuffer (const vector<tcu::Vec4>& vertices)
{
	const DeviceInterface&			vkd							= m_context.getDeviceInterface();
	const VkDevice					device						= m_context.getDevice();
	Allocator&						allocator					= m_context.getDefaultAllocator();
	const VkDeviceSize				vertexBufferSize			= vertices.size() * sizeof(vertices[0]);
	const VkBufferCreateInfo		vertexBufferCreateInfo		= makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	MovePtr<BufferWithMemory>		vertexBuffer				= MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, vertexBufferCreateInfo, MemoryRequirement::HostVisible));
	Allocation&						vertexBufferAlloc			= vertexBuffer->getAllocation();

	// Initialize vertex data
	deMemcpy(vertexBufferAlloc.getHostPtr(), vertices.data(), (size_t)vertexBufferSize);
	flushAlloc(vkd, device, vertexBufferAlloc);

	return vertexBuffer;
}

bool FragmentShadingBarycentricDataTestInstance::getProvokingVertexLast (void)
{
	if (m_testParams.provokingVertexLast && m_testParams.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
	{
		const VkPhysicalDeviceFragmentShaderBarycentricPropertiesKHR&	fragmentShaderBarycentricProperties	= m_context.getFragmentShaderBarycentricProperties();

		if (fragmentShaderBarycentricProperties.triStripVertexOrderIndependentOfProvokingVertex)
			return false;
	}

	return m_testParams.provokingVertexLast;
}

tcu::TestStatus FragmentShadingBarycentricDataTestInstance::iterate (void)
{
	const InstanceInterface&		vki							= m_context.getInstanceInterface();
	const DeviceInterface&			vkd							= m_context.getDeviceInterface();
	const VkPhysicalDevice			physicalDevice				= m_context.getPhysicalDevice();
	const VkDevice					device						= m_context.getDevice();
	const auto&						deviceExtensions			= m_context.getDeviceExtensions();
	const VkQueue					queue						= m_context.getUniversalQueue();
	Allocator&						allocator					= m_context.getDefaultAllocator();
	const uint32_t					queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();

	const VkDeviceSize				offsetZero					= 0ull;
	const VkFormat					format						= VK_FORMAT_R32_UINT;
	const uint32_t					pixelSize					= mapVkFormat(format).getPixelSize();
	const tcu::Vec4					clearColor					= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
	const uint32_t					width						= static_cast<deUint32>(m_testParams.width);
	const uint32_t					height						= static_cast<deUint32>(m_testParams.height);
	const VkPrimitiveTopology		topology					= m_testParams.topology;
	const bool						withColor					= false;
	const bool						provokingVertexLast			= getProvokingVertexLast();

	const vector<tcu::Vec4>			vertices					= generateVertexBuffer();
	const uint32_t					vertexCount					= static_cast<uint32_t>(vertices.size());
	MovePtr<BufferWithMemory>		vertexBuffer				= createVertexBuffer(vertices);

	const VkImageCreateInfo			imageCreateInfo				= makeImageCreateInfo(format, width, height, VK_SAMPLE_COUNT_1_BIT);
	const VkImageSubresourceRange	imageSubresourceRange		= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const MovePtr<ImageWithMemory>	image						= MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
	const Move<VkImageView>			imageView					= makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_2D, format, imageSubresourceRange);

	const VkBufferCreateInfo		resultBufferCreateInfo		= makeBufferCreateInfo(width * height * pixelSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	MovePtr<BufferWithMemory>		resultBuffer				= MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, resultBufferCreateInfo, MemoryRequirement::HostVisible));

	const string					shaderSuffix				= (provokingVertexLast == m_testParams.provokingVertexLast) ? "" : "-forced";
	const ShaderWrapper				vertModule					= ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("vert" + shaderSuffix), 0u);

	const ShaderWrapper				tessCtrlShaderModule		= ((m_testParams.testSubtype == TEST_SUBTYPE_TESS_SHADER) || (m_testParams.testSubtype == TEST_SUBTYPE_TESSGEOM_SHADER)) ? ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("tess_ctrl" + shaderSuffix), 0u) : ShaderWrapper();
	const ShaderWrapper				tessEvalShaderModule		= ((m_testParams.testSubtype == TEST_SUBTYPE_TESS_SHADER) || (m_testParams.testSubtype == TEST_SUBTYPE_TESSGEOM_SHADER)) ? ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("tess_eval" + shaderSuffix), 0u) : ShaderWrapper();
	const ShaderWrapper				geometryShaderModule		= ((m_testParams.testSubtype == TEST_SUBTYPE_GEOMETRY_SHADER) || (m_testParams.testSubtype == TEST_SUBTYPE_TESSGEOM_SHADER)) ? ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("geom" + shaderSuffix), 0u) : ShaderWrapper();
	const ShaderWrapper				fragModule					= ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("frag" + shaderSuffix), 0u);
	RenderPassWrapper				renderPass					= RenderPassWrapper(m_testParams.pipelineConstructionType, vkd, device, format);
	renderPass.createFramebuffer(vkd, device, **image, *imageView, width, height);
	const deUint32					pushConstants[]				= { 0, 1, 2 };
	const VkPushConstantRange		pushConstantRange			= makePushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConstants));
	const VkPushConstantRange*		pushConstantRangePtr		= m_testParams.dynamicIndexing ? &pushConstantRange : DE_NULL;
	const deUint32					pushConstantRangeCount		= m_testParams.dynamicIndexing ? 1 : 0;
	const PipelineLayoutWrapper		pipelineLayout				(m_testParams.pipelineConstructionType, vkd, device, 0, DE_NULL, pushConstantRangeCount, pushConstantRangePtr);
	const auto						pipelineWrapper				= makeGraphicsPipeline(m_testParams.pipelineConstructionType, vki, vkd, physicalDevice, device, deviceExtensions, pipelineLayout, *renderPass, vertModule, fragModule, width, height, topology, VK_SAMPLE_COUNT_1_BIT, withColor, provokingVertexLast, false, tessCtrlShaderModule, tessEvalShaderModule, geometryShaderModule);
	const Move<VkCommandPool>		commandPool					= createCommandPool(vkd, device, 0, queueFamilyIndex);
	const Move<VkCommandBuffer>		commandBuffer				= allocateCommandBuffer(vkd, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	beginCommandBuffer(vkd, *commandBuffer);
	{
		renderPass.begin(vkd, *commandBuffer, makeRect2D(width, height), clearColor);

		pipelineWrapper->bind(*commandBuffer);

		vkd.cmdBindVertexBuffers(*commandBuffer, 0u, 1u, &vertexBuffer->get(), &offsetZero);

		if (m_testParams.dynamicIndexing)
			vkd.cmdPushConstants(*commandBuffer, *pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0u, sizeof(pushConstants), &pushConstants);

		vkd.cmdDraw(*commandBuffer, vertexCount, 1u, 0u, 0u);

		renderPass.end(vkd, *commandBuffer);

		copyImageToBuffer(vkd, *commandBuffer, image->get(), resultBuffer->get(), tcu::IVec2(width, height));
	}

	endCommandBuffer(vkd, *commandBuffer);
	submitCommandsAndWait(vkd, device, queue, *commandBuffer);

	invalidateMappedMemoryRange(vkd, device, resultBuffer->getAllocation().getMemory(), resultBuffer->getAllocation().getOffset(), VK_WHOLE_SIZE);

	DE_ASSERT(8 * pixelSize >= getComponentCount(m_testParams));

	if (verify(resultBuffer.get()))
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Fail");
}


class FragmentShadingBarycentricWeightTestInstance : public TestInstance
{
public:
								FragmentShadingBarycentricWeightTestInstance	(Context&			context,
																				 const TestParams&	testParams);
	virtual						~FragmentShadingBarycentricWeightTestInstance	();
	virtual tcu::TestStatus		iterate											(void);

protected:
	void						addVertexWithColor								(vector<tcu::Vec4>&			vertices,
																				 const tcu::Vec4&				vertex,
																				 const tcu::Vec4&				color);
	vector<tcu::Vec4>			generateVertexBuffer							(void);
	MovePtr<BufferWithMemory>	createVertexBuffer								(const vector<tcu::Vec4>&	vertices);
	bool						verify											(VkFormat					format,
																				 BufferWithMemory*			referenceBuffer,
																				 BufferWithMemory*			resultBuffer);

	TestParams					m_testParams;
};

FragmentShadingBarycentricWeightTestInstance::FragmentShadingBarycentricWeightTestInstance (Context& context, const TestParams& testParams)
	: TestInstance	(context)
	, m_testParams	(testParams)
{
}

FragmentShadingBarycentricWeightTestInstance::~FragmentShadingBarycentricWeightTestInstance ()
{
}

void FragmentShadingBarycentricWeightTestInstance::addVertexWithColor (vector<tcu::Vec4>&	vertices,
																	   const tcu::Vec4&		vertex,
																	   const tcu::Vec4&		color)
{
	vertices.push_back(vertex);
	vertices.push_back(color);
}

vector<tcu::Vec4> FragmentShadingBarycentricWeightTestInstance::generateVertexBuffer (void)
{
	const float			slope			= WEIGHT_TEST_SLOPE;
	const tcu::Vec4		leftBotColor	= tcu::Vec4(0.00f, 0.00f, 0.00f, 1.0f);
	const tcu::Vec4		leftTopColor	= tcu::Vec4(1.00f, 0.00f, 0.00f, 1.0f);
	const tcu::Vec4		rightTopColor	= tcu::Vec4(0.00f, 1.00f, 0.00f, 1.0f);
	const tcu::Vec4		rightBotColor	= tcu::Vec4(0.00f, 0.00f, 1.00f, 1.0f);
	const tcu::Vec4		noneColor		= tcu::Vec4(0.25f, 0.50f, 0.75f, 1.0f);
	size_t				vertexCount		= static_cast<size_t>(~0ull);
	vector<tcu::Vec4>	result;

	DE_ASSERT(slope >= 1.0f);

	switch (m_testParams.topology)
	{
		case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
		{
			vertexCount = m_testParams.width * m_testParams.height;

			result.reserve(2 * vertexCount);

			for (size_t y = 0; y < m_testParams.height; y++)
			{
				const float		ky			= (0.5f + float(y)) / float(m_testParams.height);
				const float		yy			= -1.0f + 2.0f * ky;
				const tcu::Vec4	leftColor	= mix(leftTopColor, leftBotColor, ky);
				const tcu::Vec4	rightColor	= mix(rightTopColor, rightBotColor, ky);

				for (size_t x = 0; x < m_testParams.width; x++)
				{
					const float		kx			= (0.5f + float(x)) / float(m_testParams.width);
					const float		xx			= -1.0f + 2.0f * kx;
					const float		pointSlope	= 1.0f + kx * (slope - 1.0f);
					const tcu::Vec4	point		= tcu::Vec4(xx, yy, 0.0f, 1.0f) * pointSlope;
					const tcu::Vec4	color		= mix(leftColor, rightColor, kx);

					addVertexWithColor(result, point, color);
				}
			}

			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
		{
			vertexCount = 2 * m_testParams.height;

			result.reserve(2 * vertexCount);

			for (size_t y = 0; y < m_testParams.height; y++)
			{
				const float		ky			= (0.5f + float(y)) / float(m_testParams.height);
				const float		yy			= -1.0f + 2.0f * ky;
				const tcu::Vec4	leftColor	= mix(leftTopColor, leftBotColor, ky);
				const tcu::Vec4	rightColor	= mix(rightTopColor, rightBotColor, ky);
				const tcu::Vec4	left		= tcu::Vec4(-1.0f, yy, 0.0f, 1.0f);
				const tcu::Vec4	right		= tcu::Vec4( 1.0f, yy, 0.0f, 1.0f) * slope;

				addVertexWithColor(result, left, leftColor);
				addVertexWithColor(result, right, rightColor);
			}

			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
		{
			vertexCount = 2 * m_testParams.height;

			result.reserve(2 * vertexCount);

			for (size_t y = 0; y < m_testParams.height; y++)
			{
				const float		ky			= (0.5f + float(y)) / float(m_testParams.height);
				const float		yy			= -1.0f + 2.0f * ky;
				const tcu::Vec4	leftColor	= mix(leftTopColor, leftBotColor, ky);
				const tcu::Vec4	rightColor	= mix(rightTopColor, rightBotColor, ky);
				const tcu::Vec4	left		= tcu::Vec4(-2.0f, yy, 0.0f, 1.0f);
				const tcu::Vec4	right		= tcu::Vec4( 2.0f, yy, 0.0f, 1.0f) * slope;

				if (y % 2 == 0)
				{
					addVertexWithColor(result, left, leftColor);
					addVertexWithColor(result, right, rightColor);
				}
				else
				{
					addVertexWithColor(result, right, rightColor);
					addVertexWithColor(result, left, leftColor);
				}
			}

			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		{
			vertexCount = 6;

			result.reserve(2 * vertexCount);

			addVertexWithColor(result, tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),			leftTopColor);
			addVertexWithColor(result, tcu::Vec4(-1.0f, +1.0f, 0.0f, 1.0f),			leftBotColor);
			addVertexWithColor(result, tcu::Vec4(+1.0f, +1.0f, 0.0f, 1.0f) * slope,	rightBotColor);

			addVertexWithColor(result, tcu::Vec4(+1.0f, +1.0f, 0.0f, 1.0f) * slope,	rightBotColor);
			addVertexWithColor(result, tcu::Vec4(+1.0f, -1.0f, 0.0f, 1.0f) * slope,	rightTopColor);
			addVertexWithColor(result, tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),			leftTopColor);

			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		{
			vertexCount = 4;

			result.reserve(2 * vertexCount);

			addVertexWithColor(result, tcu::Vec4(-1.0f, +1.0f, 0.0f, 1.0f),			leftBotColor);
			addVertexWithColor(result, tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),			leftTopColor);
			addVertexWithColor(result, tcu::Vec4(+1.0f, +1.0f, 0.0f, 1.0f) * slope,	rightBotColor);
			addVertexWithColor(result, tcu::Vec4(+1.0f, -1.0f, 0.0f, 1.0f) * slope,	rightTopColor);

			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
		{
			vertexCount = 4;

			result.reserve(2 * vertexCount);

			addVertexWithColor(result, tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),			leftTopColor);
			addVertexWithColor(result, tcu::Vec4(-1.0f, +1.0f, 0.0f, 1.0f),			leftBotColor);
			addVertexWithColor(result, tcu::Vec4(+1.0f, +1.0f, 0.0f, 1.0f) * slope,	rightBotColor);
			addVertexWithColor(result, tcu::Vec4(+1.0f, -1.0f, 0.0f, 1.0f) * slope,	rightTopColor);

			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		{
			vertexCount = 4 * m_testParams.height;

			result.reserve(2 * vertexCount);

			for (size_t y = 0; y < m_testParams.height; y++)
			{
				const float		ky			= (0.5f + float(y)) / float(m_testParams.height);
				const float		yy			= -1.0f + 2.0f * ky;
				const tcu::Vec4	leftColor	= mix(leftTopColor, leftBotColor, ky);
				const tcu::Vec4	rightColor	= mix(rightTopColor, rightBotColor, ky);
				const tcu::Vec4	preLeft		= tcu::Vec4(-2.0f, yy, 0.0f, 1.0f);
				const tcu::Vec4	left		= tcu::Vec4(-1.0f, yy, 0.0f, 1.0f);
				const tcu::Vec4	right		= tcu::Vec4( 1.0f, yy, 0.0f, 1.0f) * slope;
				const tcu::Vec4	afterRight	= tcu::Vec4( 2.0f, yy, 0.0f, 1.0f) * slope;

				addVertexWithColor(result, preLeft, noneColor);
				addVertexWithColor(result, left, leftColor);
				addVertexWithColor(result, right, rightColor);
				addVertexWithColor(result, afterRight, noneColor);
			}

			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
		{
			vertexCount = 2 * m_testParams.height + 2;

			result.reserve(2 * vertexCount);

			addVertexWithColor(result, tcu::Vec4(-10.0f, -10.0f, 0.0f, 1.0f), noneColor);

			for (size_t y = 0; y < m_testParams.height; y++)
			{
				const float		ky			= (0.5f + float(y)) / float(m_testParams.height);
				const float		yy			= -1.0f + 2.0f * ky;
				const tcu::Vec4	leftColor	= mix(leftTopColor, leftBotColor, ky);
				const tcu::Vec4	rightColor	= mix(rightTopColor, rightBotColor, ky);
				const tcu::Vec4	left		= tcu::Vec4(-2.0f, yy, 0.0f, 1.0f);
				const tcu::Vec4	right		= tcu::Vec4( 2.0f, yy, 0.0f, 1.0f) * slope;

				if (y % 2 == 0)
				{
					addVertexWithColor(result, left, leftColor);
					addVertexWithColor(result, right, rightColor);
				}
				else
				{
					addVertexWithColor(result, right, rightColor);
					addVertexWithColor(result, left, leftColor);
				}
			}

			addVertexWithColor(result, tcu::Vec4(+10.0f, +10.0f, 0.0f, 1.0f), noneColor);

			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		{
			vertexCount = 12;

			result.reserve(2 * vertexCount);

			addVertexWithColor(result, tcu::Vec4(-1.0f, +1.0f, 0.0f, 1.0f),			leftBotColor);
			addVertexWithColor(result, tcu::Vec4(-3.0f, +1.0f, 0.0f, 1.0f),			noneColor);
			addVertexWithColor(result, tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),			leftTopColor);
			addVertexWithColor(result, tcu::Vec4(+1.0f, -1.0f, 0.0f, 1.0f) * slope,	noneColor);
			addVertexWithColor(result, tcu::Vec4(+1.0f, +1.0f, 0.0f, 1.0f) * slope,	rightBotColor);
			addVertexWithColor(result, tcu::Vec4(+1.0f, +3.0f, 0.0f, 1.0f) * slope,	noneColor);

			addVertexWithColor(result, tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),			leftTopColor);
			addVertexWithColor(result, tcu::Vec4(-1.0f, +1.0f, 0.0f, 1.0f),			noneColor);
			addVertexWithColor(result, tcu::Vec4(+1.0f, +1.0f, 0.0f, 1.0f) * slope,	rightBotColor);
			addVertexWithColor(result, tcu::Vec4(+3.0f, +1.0f, 0.0f, 1.0f) * slope,	noneColor);
			addVertexWithColor(result, tcu::Vec4(+1.0f, -1.0f, 0.0f, 1.0f) * slope,	rightTopColor);
			addVertexWithColor(result, tcu::Vec4(-1.0f, -3.0f, 0.0f, 1.0f),			leftTopColor);

			break;
		}

		case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
		{
			vertexCount = 8;

			result.reserve(2 * vertexCount);

			addVertexWithColor(result, tcu::Vec4(-1.0f, +1.0f, 0.0f, 1.0f),			leftBotColor);
			addVertexWithColor(result, tcu::Vec4(-3.0f, -1.0f, 0.0f, 1.0f),			noneColor);
			addVertexWithColor(result, tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),			leftTopColor);
			addVertexWithColor(result, tcu::Vec4(+1.0f, +3.0f, 0.0f, 1.0f) * slope,	noneColor);
			addVertexWithColor(result, tcu::Vec4(+1.0f, +1.0f, 0.0f, 1.0f) * slope,	rightBotColor);
			addVertexWithColor(result, tcu::Vec4(-1.0f, -3.0f, 0.0f, 1.0f),			noneColor);
			addVertexWithColor(result, tcu::Vec4(+1.0f, -1.0f, 0.0f, 1.0f) * slope,	rightTopColor);
			addVertexWithColor(result, tcu::Vec4(+1.0f, +3.0f, 0.0f, 1.0f) * slope,	noneColor);

			break;
		}

		default:
			TCU_THROW(InternalError, "Unknown topology");
	}

	DE_ASSERT(result.size() == 2 * vertexCount);

	return result;
}

bool FragmentShadingBarycentricWeightTestInstance::verify (VkFormat format, BufferWithMemory* referenceBuffer, BufferWithMemory* resultBuffer)
{
	const deUint32*						refernceData	= (deUint32*)referenceBuffer->getAllocation().getHostPtr();
	const deUint32*						resultData		= (deUint32*)resultBuffer->getAllocation().getHostPtr();
	tcu::TestLog&						log				= m_context.getTestContext().getLog();
	const tcu::ConstPixelBufferAccess	refImage		(mapVkFormat(format), (int)m_testParams.width, (int)m_testParams.height, 1u, refernceData);
	const tcu::ConstPixelBufferAccess	resultImage		(mapVkFormat(format), (int)m_testParams.width, (int)m_testParams.height, 1u, resultData);
	const tcu::UVec4					threshold		(1, 1, 1, 1);
	bool								result			= tcu::intThresholdCompare(log,
																				   "ComparisonResult",
																				   "Image comparison result",
																				   refImage,
																				   resultImage,
																				   threshold,
																				   tcu::COMPARE_LOG_ON_ERROR);

	return result;
}

MovePtr<BufferWithMemory> FragmentShadingBarycentricWeightTestInstance::createVertexBuffer (const vector<tcu::Vec4>& vertices)
{
	const DeviceInterface&			vkd						= m_context.getDeviceInterface();
	const VkDevice					device					= m_context.getDevice();
	Allocator&						allocator				= m_context.getDefaultAllocator();
	const VkDeviceSize				vertexBufferSize		= vertices.size() * sizeof(vertices[0]);
	const VkBufferCreateInfo		vertexBufferCreateInfo	= makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	MovePtr<BufferWithMemory>		vertexBuffer			= MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, vertexBufferCreateInfo, MemoryRequirement::HostVisible));
	Allocation&						vertexBufferAlloc		= vertexBuffer->getAllocation();

	// Initialize vertex data
	deMemcpy(vertexBufferAlloc.getHostPtr(), vertices.data(), (size_t)vertexBufferSize);
	flushAlloc(vkd, device, vertexBufferAlloc);

	return vertexBuffer;
}

tcu::TestStatus FragmentShadingBarycentricWeightTestInstance::iterate (void)
{
	const InstanceInterface&		vki						= m_context.getInstanceInterface();
	const DeviceInterface&			vkd						= m_context.getDeviceInterface();
	const VkPhysicalDevice			physicalDevice			= m_context.getPhysicalDevice();
	const VkDevice					device					= m_context.getDevice();
	const auto&						deviceExtensions		= m_context.getDeviceExtensions();
	const VkQueue					queue					= m_context.getUniversalQueue();
	Allocator&						allocator				= m_context.getDefaultAllocator();
	const uint32_t					queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();

	const VkDeviceSize				offsetZero				= 0ull;
	const VkFormat					format					= VK_FORMAT_R8G8B8A8_UNORM;
	const uint32_t					pixelSize				= mapVkFormat(format).getPixelSize();
	const uint32_t					width					= static_cast<deUint32>(m_testParams.width);
	const uint32_t					height					= static_cast<deUint32>(m_testParams.height);
	const bool						dynamicStateTopology	= m_testParams.dynamicTopologyInPipeline;
	const VkPrimitiveTopology		pipelineTopology		= dynamicStateTopology ? primitiveTopologyCastToList(m_testParams.topology) : m_testParams.topology;
	const bool						withColor				= true;
	const bool						provokingVertexLast		= m_testParams.provokingVertexLast;
	const bool						useMultisampling		= m_testParams.sampleCount > VK_SAMPLE_COUNT_1_BIT;
	const float						teta					= deFloatRadians(-float(m_testParams.rotation));
	const float						mvp[4*4]				=
	{
		cos(teta), -sin(teta), 0.0f, 0.0f,
		sin(teta),  cos(teta), 0.0f, 0.0f,
		0.0f,            0.0f, 1.0f, 0.0f,
		0.0f,            0.0f, 0.0f, 1.0f
	};
	const vector<tcu::Vec4>			vertices				= generateVertexBuffer();
	const uint32_t					vertexCount				= static_cast<uint32_t>(vertices.size() / 2);
	MovePtr<BufferWithMemory>		vertexBuffer			= createVertexBuffer(vertices);

	const VkBufferCreateInfo		bufferCreateInfo		= makeBufferCreateInfo(width * height * pixelSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	MovePtr<BufferWithMemory>		resultBuffer			= MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));
	MovePtr<BufferWithMemory>		referenceBuffer			= MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));
	const Move<VkRenderPass>		renderPass				= makeRenderPass(vkd, device, format, m_testParams.sampleCount);
	const Move<VkCommandPool>		commandPool				= createCommandPool(vkd, device, 0, queueFamilyIndex);
	const ShaderWrapper				vertModule				= ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
	const VkImageCreateInfo			imageCreateInfo			= makeImageCreateInfo(format, width, height, VK_SAMPLE_COUNT_1_BIT);
	const VkImageCreateInfo			msImageCreateInfo		= makeImageCreateInfo(format, width, height, m_testParams.sampleCount);
	const VkImageSubresourceRange	imageSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const std::vector<VkClearValue> clearValues				(2u, makeClearValueColorU32(0u, 0u, 0u, 0u));

	std::vector<VkImageMemoryBarrier> initialImageBarriers(2, makeImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, DE_NULL, imageSubresourceRange));

	for (size_t ndx = 0; ndx < 2; ndx++)
	{
		const MovePtr<ImageWithMemory>	image				= MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any));
		const Move<VkImageView>			imageView			= makeImageView(vkd, device, **image, VK_IMAGE_VIEW_TYPE_2D, format, imageSubresourceRange);
		MovePtr<ImageWithMemory>		msImage;
		Move<VkImageView>				msImageView;

		initialImageBarriers[0].image = **image;

		if (useMultisampling)
		{
			msImage		= MovePtr<ImageWithMemory>(new ImageWithMemory(vkd, device, allocator, msImageCreateInfo, MemoryRequirement::Any));
			msImageView	= makeImageView(vkd, device, **msImage, VK_IMAGE_VIEW_TYPE_2D, format, imageSubresourceRange);
			initialImageBarriers[1].image = **msImage;
		}

		const std::vector<VkImageView>	imageViewVect		= { *imageView, *msImageView };
		const Move<VkFramebuffer>		framebuffer			= makeFramebuffer(vkd, device, *renderPass, 1u + useMultisampling, imageViewVect.data(), width, height);
		const Move<VkCommandBuffer>		commandBuffer		= allocateCommandBuffer(vkd, device, *commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		const BufferWithMemory*			buffer				= (ndx == 0) ? resultBuffer.get() : referenceBuffer.get();
		const string					fragModuleName		= (ndx == 0) ? "frag_test" : "frag_reference";
		const ShaderWrapper				fragModule			= ShaderWrapper(vkd, device, m_context.getBinaryCollection().get(fragModuleName), 0u);
		const VkPushConstantRange		pushConstantRange	= makePushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mvp));
		const PipelineLayoutWrapper		pipelineLayout		(m_testParams.pipelineConstructionType, vkd, device, 0, DE_NULL, 1, &pushConstantRange);
		const auto						pipelineWrapper		= makeGraphicsPipeline(m_testParams.pipelineConstructionType, vki, vkd, physicalDevice, device, deviceExtensions, pipelineLayout, *renderPass, vertModule, fragModule, width, height, pipelineTopology, m_testParams.sampleCount, withColor, provokingVertexLast, dynamicStateTopology);
		const VkPipeline				pipeline			= pipelineWrapper->getPipeline();

		beginCommandBuffer(vkd, *commandBuffer);
		{
			cmdPipelineImageMemoryBarrier(vkd, *commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, initialImageBarriers.data(), 1u + useMultisampling);

			if (dynamicStateTopology)
				vkd.cmdSetPrimitiveTopology(*commandBuffer, m_testParams.topology);

			beginRenderPass(vkd, *commandBuffer, *renderPass, *framebuffer, makeRect2D(width, height), 1u + useMultisampling, clearValues.data());
			{
				vkd.cmdBindPipeline(*commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

				vkd.cmdBindVertexBuffers(*commandBuffer, 0u, 1u, &vertexBuffer->get(), &offsetZero);

				vkd.cmdPushConstants(*commandBuffer, *pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0u, sizeof(mvp), &mvp);

				vkd.cmdDraw(*commandBuffer, vertexCount, 1u, 0u, 0u);
			}
			endRenderPass(vkd, *commandBuffer);

			copyImageToBuffer(vkd, *commandBuffer, image->get(), buffer->get(), tcu::IVec2(width, height));
		}
		endCommandBuffer(vkd, *commandBuffer);
		submitCommandsAndWait(vkd, device, queue, *commandBuffer);

		invalidateMappedMemoryRange(vkd, device, buffer->getAllocation().getMemory(), buffer->getAllocation().getOffset(), VK_WHOLE_SIZE);
	}

	if (verify(format, referenceBuffer.get(), resultBuffer.get()))
		return tcu::TestStatus::pass("Pass");
	else
		return tcu::TestStatus::fail("Fail");
}


class FragmentShadingBarycentricTestCase : public TestCase
{
public:
							FragmentShadingBarycentricTestCase	(tcu::TestContext&	context,
																 const char*		name,
																 const char*		desc,
																 const TestParams	testParams);
							~FragmentShadingBarycentricTestCase	(void);

	virtual void			checkSupport						(Context&			context) const;
	virtual	void			initPrograms						(SourceCollections&	programCollection) const;
	virtual TestInstance*	createInstance						(Context&			context) const;

private:
	void					initDataPrograms					(SourceCollections&	programCollection) const;
	void					initMiscDataPrograms				(SourceCollections& programCollection) const;
	void					initMiscDataTessPrograms			(SourceCollections& programCollection, map<string, string>& attributes) const;
	void					initMiscDataGeomPrograms			(SourceCollections& programCollection, map<string, string>& attributes) const;
	void					initWeightPrograms					(SourceCollections&	programCollection) const;
	string					getDataPrimitiveFormula				(void) const;
	string					getDataVertexFormula				(const uint32_t		vertex,
																 const bool*		provokingVertexLastPtr = DE_NULL) const;

	TestParams				m_testParams;
};

FragmentShadingBarycentricTestCase::FragmentShadingBarycentricTestCase (tcu::TestContext& context, const char* name, const char* desc, const TestParams testParams)
	: vkt::TestCase	(context, name, desc)
	, m_testParams	(testParams)
{
}

FragmentShadingBarycentricTestCase::~FragmentShadingBarycentricTestCase (void)
{
}

void FragmentShadingBarycentricTestCase::checkSupport (Context& context) const
{
	const auto&		vki				= context.getInstanceInterface();
	const auto		physicalDevice	= context.getPhysicalDevice();

	context.requireDeviceFunctionality("VK_KHR_fragment_shader_barycentric");

	const VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR& fragmentShaderBarycentricFeatures = context.getFragmentShaderBarycentricFeatures();

	if (!fragmentShaderBarycentricFeatures.fragmentShaderBarycentric)
		TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR.fragmentShaderBarycentric");

	checkPipelineConstructionRequirements(vki, physicalDevice, m_testParams.pipelineConstructionType);

	if (m_testParams.provokingVertexLast)
	{
		context.requireDeviceFunctionality("VK_EXT_provoking_vertex");

		const VkPhysicalDeviceProvokingVertexFeaturesEXT&	provokingVertexFeaturesEXT	= context.getProvokingVertexFeaturesEXT();

		if (!provokingVertexFeaturesEXT.provokingVertexLast)
			TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceProvokingVertexFeaturesEXT.provokingVertexLast");
	}

	if (m_testParams.dynamicTopologyInPipeline)
	{
		context.requireDeviceFunctionality("VK_EXT_extended_dynamic_state");

		const VkPhysicalDeviceExtendedDynamicStateFeaturesEXT& extendedDynamicStateFeaturesEXT	= context.getExtendedDynamicStateFeaturesEXT();

		if (!extendedDynamicStateFeaturesEXT.extendedDynamicState)
			TCU_THROW(NotSupportedError, "Requires VkPhysicalDeviceExtendedDynamicStateFeaturesEXT.extendedDynamicState");
	}

	if ((m_testParams.dataType == glu::TYPE_DOUBLE) ||
		(m_testParams.dataType == glu::TYPE_DOUBLE_VEC2) ||
		(m_testParams.dataType == glu::TYPE_DOUBLE_VEC3) ||
		(m_testParams.dataType == glu::TYPE_DOUBLE_VEC4))
	{
		VkPhysicalDeviceFeatures2 features2;
		deMemset(&features2, 0, sizeof(features2));
		features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features2.pNext = nullptr;
		vki.getPhysicalDeviceFeatures2(physicalDevice, &features2);
		if (features2.features.shaderFloat64 != VK_TRUE)
		{
			TCU_THROW(NotSupportedError, "shaderFloat64 not supported");
		}
	}

	if ((m_testParams.testSubtype == TEST_SUBTYPE_TESS_SHADER) || (m_testParams.testSubtype == TEST_SUBTYPE_TESSGEOM_SHADER))
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

	if ((m_testParams.testSubtype == TEST_SUBTYPE_GEOMETRY_SHADER) || (m_testParams.testSubtype == TEST_SUBTYPE_TESSGEOM_SHADER))
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_GEOMETRY_SHADER);
}

TestInstance* FragmentShadingBarycentricTestCase::createInstance (Context& context) const
{
	switch (m_testParams.testType)
	{
		case TEST_TYPE_DATA:
			return new FragmentShadingBarycentricDataTestInstance(context, m_testParams);
		case TEST_TYPE_WEIGHTS:	return new FragmentShadingBarycentricWeightTestInstance(context, m_testParams);
		default:				TCU_THROW(InternalError, "Unknown testType");
	}
}

void FragmentShadingBarycentricTestCase::initPrograms (SourceCollections& programCollection) const
{
	switch (m_testParams.testType)
	{
		case TEST_TYPE_DATA:
			if (m_testParams.testSubtype == TEST_SUBTYPE_PERVERTEX_CORRECTNESS)
				initMiscDataPrograms(programCollection);
			else
				initDataPrograms(programCollection);
			break;
		case TEST_TYPE_WEIGHTS:
			initWeightPrograms(programCollection);
			break;
		default:
			TCU_THROW(InternalError, "Unknown testType");
	}
}

string FragmentShadingBarycentricTestCase::getDataPrimitiveFormula (void) const
{
	const char*	primitiveFormulas[] =
	{
		"w * y + x",		//  VK_PRIMITIVE_TOPOLOGY_POINT_LIST
		"y",				//  VK_PRIMITIVE_TOPOLOGY_LINE_LIST
		"2*y",				//  VK_PRIMITIVE_TOPOLOGY_LINE_STRIP
		"(x < y) ? 0 : 1",	//  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
		"(x < y) ? 0 : 1",	//  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
		"(x < y) ? 0 : 1",	//  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN
		"y",				//  VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY
		"2*y",				//  VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY
		"(x < y) ? 0 : 1",	//  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY
		"(x < y) ? 0 : 1",	//  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY
		"(x < y) ? 0 : 1",	//  VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
	};

	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(primitiveFormulas) == vk::VK_PRIMITIVE_TOPOLOGY_LAST);
	DE_ASSERT(m_testParams.topology < DE_LENGTH_OF_ARRAY(primitiveFormulas));

	return primitiveFormulas[m_testParams.topology];
}

string FragmentShadingBarycentricTestCase::getDataVertexFormula (const uint32_t vertex, const bool* provokingVertexLastPtr) const
{
	typedef const char* TriVertexFormula[3];

	// Accoriding "Barycentric Interpolation" section
	const TriVertexFormula	topologyVertexFormulas[]		=
	{
		{	"p",				"p",				"p"					},	//  VK_PRIMITIVE_TOPOLOGY_POINT_LIST
		{	"2*p",				"2*p+1",			"2*p+1"				},	//  VK_PRIMITIVE_TOPOLOGY_LINE_LIST
		{	"p",				"p+1",				"p+1"				},	//  VK_PRIMITIVE_TOPOLOGY_LINE_STRIP
		{	"3*p",				"3*p+1",			"3*p+2"				},	//  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
		{	"p",				"even?p+1:p+2",		"even?p+2:p+1"		},	//  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
		{	"p+1",				"p+2",				"0"					},	//  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN
		{	"4*p+1",			"4*p+2",			"4*p+2"				},	//  VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY
		{	"p+1",				"p+2",				"p+2"				},	//  VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY
		{	"6*p",				"6*p+2",			"6*p+4"				},	//  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY
		{	"2*p",				"even?2*p+2:2*p+4",	"even?2*p+4:2*p+2"	},	//  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY
		{	"3*p",				"3*p+1",			"3*p+2"				},	//  VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
	};
	const TriVertexFormula	topologyVertexFormulasLast[]	=
	{
		{	"even?p:p+1",		"even?p+1:p",		"p+2"				},	//  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
		{	"0",				"p+1",				"p+2"				},	//  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN
		{	"even?2*p:2*p+2",	"even?2*p+2:2*p",	"2*p+4"				},	//  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY
	};
	const bool				provokingVertexLast				= provokingVertexLastPtr ? (*provokingVertexLastPtr) : m_testParams.provokingVertexLast;
	const bool				provokingLastTriangleStrip		= provokingVertexLast && m_testParams.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	const bool				provokingLastTriangleFan		= provokingVertexLast && m_testParams.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
	const bool				provokingLastTriangleStripAdj	= provokingVertexLast && m_testParams.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
	const TriVertexFormula*	triVertexFormula				= provokingLastTriangleStrip    ? &topologyVertexFormulasLast[0]
															: provokingLastTriangleFan      ? &topologyVertexFormulasLast[1]
															: provokingLastTriangleStripAdj ? &topologyVertexFormulasLast[2]
															: &topologyVertexFormulas[static_cast<size_t>(m_testParams.topology)];

	DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(topologyVertexFormulas) == vk::VK_PRIMITIVE_TOPOLOGY_LAST);
	DE_ASSERT(vertex < DE_LENGTH_OF_ARRAY(triVertexFormula[0]));

	return "(" + string(triVertexFormula[0][vertex]) + ")";
}

void FragmentShadingBarycentricTestCase::initDataPrograms (SourceCollections& programCollection) const
{
	map<string, string>			attributes;
	const string				primitiveType	= string(getDataTypeName(m_testParams.dataType));
	const string				dataStructType	= m_testParams.aggregate == 1 ? "struct DataStruct {" + primitiveType + " q;};" : "";
	const string				typePrefix		= m_testParams.aggregate == 0 ? primitiveType
												: m_testParams.aggregate == 1 ? "DataStruct"
												: primitiveType;
	const string				typeSuffix		= m_testParams.aggregate == 0 ? ""
												: m_testParams.aggregate == 1 ? ""
												: "[" + de::toString(m_testParams.aggregate) + "]";
	const int					scalarSize		= getDataTypeScalarSize(m_testParams.dataType);
	const string				scalarName		= getDataTypeName(getDataTypeScalarType(m_testParams.dataType));
	const string				vectoredInit	= (scalarSize == 1) ? primitiveType + "(n)"
												: (scalarSize == 2) ? primitiveType + "(" + scalarName + "(n), " + scalarName + "(2*n))"
												: (scalarSize == 3) ? primitiveType + "(" + scalarName + "(n), " + scalarName + "(2*n), " + scalarName + "(4*n))"
												: (scalarSize == 4) ? primitiveType + "(" + scalarName + "(n), " + scalarName + "(2*n), " + scalarName + "(4*n), " + scalarName + "(8*n))"
												: "NOT IMPLEMENTED";
	const string				value			= m_testParams.aggregate == 0 ? vectoredInit
												: m_testParams.aggregate == 1 ? "DataStruct(" + vectoredInit + ")"
												: m_testParams.aggregate == 2 ? primitiveType + "[2](" + vectoredInit + ", " + scalarName + "(3)*" + vectoredInit + ")"
												: "NOT IMPLEMENTED";
	const size_t				componentCount	= getComponentCount(m_testParams);
	const string				scalarized		= (scalarSize == 1) ? "e${R},"
												: (scalarSize == 2) ? "e${R}.x,e${R}.y,"
												: (scalarSize == 3) ? "e${R}.x,e${R}.y,e${R}.z,"
												: (scalarSize == 4) ? "e${R}.x,e${R}.y,e${R}.z,e${R}.w,"
												: "NOT IMPLEMENTED";
	const string				deaggregated	= m_testParams.aggregate == 0 ? replace(scalarized, "${R}", "${S}")
												: m_testParams.aggregate == 1 ? replace(scalarized, "${R}", "${S}.q")
												: m_testParams.aggregate == 2 ? replace(scalarized, "${R}", "${S}[0]") + replace(scalarized, "${R}", "${S}[1]")
												: "NOT IMPLEMENTED";
	const string				unwrap			= replace(deaggregated, "${S}", "A") + replace(deaggregated, "${S}", "B")+ replace(deaggregated, "${S}", "C");
	const string				expected		= unwrap.substr(0, unwrap.size() - 1);
	const string				arrived			= replace(expected, "e", "v");
	const string				dynamicIndexing	= m_testParams.dynamicIndexing ? "layout(push_constant) uniform PushConstant { uint n[3]; } pc;\n" : "";
	const string				i0				= m_testParams.dynamicIndexing ? "pc.n[0]" : "0";
	const string				i1				= m_testParams.dynamicIndexing ? "pc.n[1]" : "1";
	const string				i2				= m_testParams.dynamicIndexing ? "pc.n[2]" : "2";
	const string				primitiveId		= getDataPrimitiveFormula();
	const string				vertexFormula[]	= { getDataVertexFormula(0), getDataVertexFormula(1), getDataVertexFormula(2) };
	const tcu::StringTemplate	vertShader		(string(
		"#version 450\n"
		"#extension GL_EXT_fragment_shader_barycentric : require\n"
		"\n"
		"${dataStruct}\n"
		"\n"
		"layout(location = 0) in  vec4 in_position;\n"
		"layout(location = 0) out ${typePrefix} data${typeSuffix};\n"
		"\n"
		"out gl_PerVertex\n"
		"{\n"
		"    vec4  gl_Position;\n"
		"    float gl_PointSize;\n"
		"};\n"
		"\n"
		"void main()\n"
		"{\n"
		"    const int n  = gl_VertexIndex + 1;\n"
		"    data         = ${value};\n"
		"    gl_PointSize = 1.0;\n"
		"    gl_Position  = in_position;\n"
		"}\n"));
	const tcu::StringTemplate	fragShader		(
		string("#version 450\n") +
		"#extension GL_EXT_fragment_shader_barycentric : require\n"
		"\n"
		"${dataStruct}\n"
		"\n"
		"${dynamicIndexing}\n"
		"layout(location = 0) out uvec4 out_color;\n"
		"\n"
		+ string(
		((m_testParams.testSubtype == TEST_SUBTYPE_TESS_SHADER) || (m_testParams.testSubtype == TEST_SUBTYPE_TESSGEOM_SHADER)) ?
			"struct InDataStruct {uint idx; ${typePrefix} data${typeSuffix};};\n"
			"layout(location = 0) pervertexEXT in InDataStruct inParam[];\n"
			"void getData(uint i, out ${typePrefix} ds${typeSuffix})\n"
			"{\n"
			"    for(uint k = 0; k < ${componentCount}; k++)\n"
			"    {\n"
			"        if (inParam[k].idx == i)\n"
			"            ds = " + string(m_testParams.aggregate == 2 ? "${typePrefix}${typeSuffix}(inParam[k].data[0],inParam[k].data[1])" : "inParam[k].data") + ";\n"
			"    }\n"
			"}\n"
		:
			"layout(location = 0) pervertexEXT in ${typePrefix} data[]${typeSuffix};\n"
		)
		+
		"void main()\n"
		"{\n"
		"    const int  w    = " + de::toString(m_testParams.width) + ";\n"
		"    const int  h    = " + de::toString(m_testParams.height) + ";\n"
		"    const int  x    = int(gl_FragCoord.x - 0.5f);\n"
		"    const int  y    = int(gl_FragCoord.y - 0.5f);\n"
		"    const int  p    = ${primitiveId};\n"
		"    const bool even = (p%2 == 0);\n"
		"\n"
		"    ${typePrefix} eA${typeSuffix}; { const int n = 1 + ${vertexFormula0}; eA = ${value}; }\n"
		"    ${typePrefix} eB${typeSuffix}; { const int n = 1 + ${vertexFormula1}; eB = ${value}; }\n"
		"    ${typePrefix} eC${typeSuffix}; { const int n = 1 + ${vertexFormula2}; eC = ${value}; }\n"
		"\n"
		"    ${scalarName} e[${componentCount}] = { ${expected} };\n"
		"\n"
		+ string(
		((m_testParams.testSubtype == TEST_SUBTYPE_TESS_SHADER) || (m_testParams.testSubtype == TEST_SUBTYPE_TESSGEOM_SHADER)) ?
			"    ${typePrefix} vA${typeSuffix}; { getData(${i0}, vA); }\n"
			"    ${typePrefix} vB${typeSuffix}; { getData(${i1}, vB); }\n"
			"    ${typePrefix} vC${typeSuffix}; { getData(${i2}, vC); }\n"
		:
			"    ${typePrefix} vA${typeSuffix}; { vA = " + string(m_testParams.aggregate == 2 ? "${typePrefix}${typeSuffix}(data[${i0}][0],data[${i0}][1])" : "data[${i0}]") + "; }\n"
			"    ${typePrefix} vB${typeSuffix}; { vB = " + string(m_testParams.aggregate == 2 ? "${typePrefix}${typeSuffix}(data[${i1}][0],data[${i1}][1])" : "data[${i1}]") + "; }\n"
			"    ${typePrefix} vC${typeSuffix}; { vC = " + string(m_testParams.aggregate == 2 ? "${typePrefix}${typeSuffix}(data[${i2}][0],data[${i2}][1])" : "data[${i2}]") + "; }\n"

		)
		+

		"    ${scalarName} v[${componentCount}] = { ${arrived} };\n"
		"\n"
		"    int mask = 0;\n"
		"\n"
		"    for (int i = 0; i<${componentCount}; i++)\n"
		"        if (e[i] == v[i])\n"
		"            mask = mask | (1<<i);\n"
		"\n"
		"    out_color = uvec4(mask);\n"
		"}\n");

	attributes["typePrefix"]		= typePrefix;
	attributes["typeSuffix"]		= typeSuffix;
	attributes["value"]				= value;
	attributes["componentCount"]	= de::toString(componentCount);
	attributes["expected"]			= expected;
	attributes["arrived"]			= arrived;
	attributes["scalarName"]		= scalarName;
	attributes["dataStruct"]		= dataStructType;
	attributes["dynamicIndexing"]	= dynamicIndexing;
	attributes["primitiveId"]		= primitiveId;
	attributes["i0"]				= i0;
	attributes["i1"]				= i1;
	attributes["i2"]				= i2;
	attributes["vertexFormula0"]	= vertexFormula[0];
	attributes["vertexFormula1"]	= vertexFormula[1];
	attributes["vertexFormula2"]	= vertexFormula[2];

	if (isPrimitiveTopologyLine(m_testParams.topology))
	{
		DE_ASSERT(vertexFormula[2] == vertexFormula[1]);
	}
	else if (isPrimitiveTopologyPoint(m_testParams.topology))
	{
		DE_ASSERT(vertexFormula[2] == vertexFormula[1] && vertexFormula[1] == vertexFormula[0]);
	}

	programCollection.glslSources.add("vert") << glu::VertexSource(vertShader.specialize(attributes));
	programCollection.glslSources.add("frag") << glu::FragmentSource(fragShader.specialize(attributes));

	if (m_testParams.provokingVertexLast && m_testParams.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
	{
		const bool	provokingVertexLast = false;

		attributes["vertexFormula0"] = getDataVertexFormula(0, &provokingVertexLast);
		attributes["vertexFormula1"] = getDataVertexFormula(1, &provokingVertexLast);
		attributes["vertexFormula2"] = getDataVertexFormula(2, &provokingVertexLast);

		programCollection.glslSources.add("vert-forced") << glu::VertexSource(vertShader.specialize(attributes));
		programCollection.glslSources.add("frag-forced") << glu::FragmentSource(fragShader.specialize(attributes));
	}

	if (m_testParams.testSubtype == TEST_SUBTYPE_TESS_SHADER)
		initMiscDataTessPrograms(programCollection, attributes);
	else if (m_testParams.testSubtype == TEST_SUBTYPE_GEOMETRY_SHADER)
		initMiscDataGeomPrograms(programCollection, attributes);
	else if (m_testParams.testSubtype == TEST_SUBTYPE_TESSGEOM_SHADER)
	{
		initMiscDataTessPrograms(programCollection, attributes);
		initMiscDataGeomPrograms(programCollection, attributes);
	}
}

void FragmentShadingBarycentricTestCase::initMiscDataPrograms(SourceCollections& programCollection) const
{
	const std::string vertShader(
		"#version 450\n"
		"#extension GL_EXT_fragment_shader_barycentric : require\n"
		"\n"
		"layout(location = 0) in  vec4 in_position;\n"
		"layout(location = 0) out uvec2 dataA;\n"
		"layout(location = 1) out uvec2 dataB;\n"
		"void main()\n"
		"{\n"
		// we will draw two triangles and we need to convert dataA for
		// second triangle to 0-2 range to simplify verification
		"    dataA       = uvec2(mod(gl_VertexIndex, 3));\n"
		"    dataB       = uvec2(7);\n"
		"    gl_Position = in_position;\n"
		"}\n");
	const std::string fragShader(
		"#version 450\n"
		"#extension GL_EXT_fragment_shader_barycentric : require\n"
		"layout(location = 0) pervertexEXT in uvec2 dataA[];\n"
		"layout(location = 1) flat in uvec2 dataB;\n"
		"layout(location = 0) out uvec4 out_color;\n"
		"void main()\n"
		"{\n"
		// make sure that PerVertex decoration is only applied to location 0
		// and that the location 1 isn't compacted/remapped to location 0
		// by adding all values and making sure the result is 10
		"    out_color = uvec4(dataA[0].y + dataA[1].x + dataA[2].y + dataB.x);\n"
		"}\n");

	programCollection.glslSources.add("vert") << glu::VertexSource(vertShader);
	programCollection.glslSources.add("frag") << glu::FragmentSource(fragShader);
}

void FragmentShadingBarycentricTestCase::initMiscDataTessPrograms(SourceCollections& programCollection, map<string, string>& attributes) const
{
	// Tessellation control
	const tcu::StringTemplate tesc (string(
		"#version 450\n"
		"#extension GL_EXT_tessellation_shader : require\n"
		"layout (vertices=6) out;\n"
		"in gl_PerVertex\n"
		"{\n"
		"    vec4 gl_Position;\n"
		"	float gl_PointSize;\n"
		"} gl_in[];\n"
		"out gl_PerVertex\n"
		"{\n"
		"    vec4 gl_Position;\n"
		"	float gl_PointSize;\n"
		"} gl_out[];\n"
		"\n"
		"${dataStruct}\n"
		"\n"
		"layout (location=0) in ${typePrefix} inData[]${typeSuffix};\n"
		"layout (location=0) out ${typePrefix} outData[]${typeSuffix};\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"	if (gl_InvocationID == 0)\n"
		"	{\n"
		"		gl_TessLevelInner[0] = 1.0;\n"
		"		gl_TessLevelInner[1] = 1.0;\n"
		"		gl_TessLevelOuter[0] = 1.0;\n"
		"		gl_TessLevelOuter[1] = 1.0;\n"
		"		gl_TessLevelOuter[2] = 1.0;\n"
		"		gl_TessLevelOuter[3] = 1.0;\n"
		"	}\n"
		"    outData[gl_InvocationID] = inData[gl_InvocationID];\n"
		"    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
		"}\n"
	));
	programCollection.glslSources.add("tess_ctrl") << glu::TessellationControlSource(tesc.specialize(attributes));

	// Tessellation evaluation shader
	const tcu::StringTemplate tese (string(
		"#version 450\n"
		"#extension GL_EXT_tessellation_shader : require\n"
		"layout (triangles) in;\n"
		"in gl_PerVertex\n"
		"{\n"
		"    vec4 gl_Position;\n"
		"	float gl_PointSize;\n"
		"} gl_in[];\n"
		"out gl_PerVertex\n"
		"{\n"
		"    vec4 gl_Position;\n"
		"	float gl_PointSize;\n"
		"};\n"
		"\n"
		"${dataStruct}\n"
		"\n"
		"layout (location=0) in ${typePrefix} inData[]${typeSuffix};\n"
		"struct OutDataStruct {int idx; ${typePrefix} data${typeSuffix};};\n"
		"layout (location=0) flat out OutDataStruct outParam;\n"
		"\n"
		"void main (void)\n"
		"{\n"
		"    gl_Position = (gl_TessCoord.x * gl_in[0].gl_Position) +\n"
		"                  (gl_TessCoord.y * gl_in[1].gl_Position) +\n"
		"                  (gl_TessCoord.z * gl_in[2].gl_Position);\n"
		"    if (gl_TessCoord.xyz == vec3(0.0,1.0,0.0)) {outParam.idx = ${i2}; outParam.data = " + string(m_testParams.aggregate == 2 ? "${typePrefix}${typeSuffix}(inData[${i2}][0],inData[${i2}][1])" : "inData[${i2}]") + "; }\n"
		"    else if (gl_TessCoord.xyz == vec3(1.0,0.0,0.0)) {outParam.idx = ${i0}; outParam.data = " + string(m_testParams.aggregate == 2 ? "${typePrefix}${typeSuffix}(inData[${i0}][0],inData[${i0}][1])" : "inData[${i0}]") + "; }\n"
		"    else if (gl_TessCoord.xyz == vec3(0.0,0.0,1.0)) {outParam.idx = ${i1}; outParam.data = " + string(m_testParams.aggregate == 2 ? "${typePrefix}${typeSuffix}(inData[${i1}][0],inData[${i1}][1])" : "inData[${i1}]") + "; }\n"
		"}\n"
	));
	programCollection.glslSources.add("tess_eval") << glu::TessellationEvaluationSource(tese.specialize(attributes));
}

void FragmentShadingBarycentricTestCase::initMiscDataGeomPrograms(SourceCollections& programCollection, map<string, string>& attributes) const
{
	// Geometry shader
	const tcu::StringTemplate geom (
		string(
		"#version 460\n"
		"\n"
		"layout (triangles) in;\n"
		"layout (triangle_strip, max_vertices=3) out;\n"
		"in gl_PerVertex\n"
		"{\n"
		"    vec4 gl_Position;\n"
		"	float gl_PointSize;\n"
		"} gl_in[3];\n"
		"out gl_PerVertex\n"
		"{\n"
		"    vec4 gl_Position;\n"
		"	float gl_PointSize;\n"
		"};\n"
		"\n"
		"${dataStruct}\n"
		"\n")
		+ string(
		((m_testParams.testSubtype == TEST_SUBTYPE_TESS_SHADER) || (m_testParams.testSubtype == TEST_SUBTYPE_TESSGEOM_SHADER)) ?
			"struct InOutDataStruct {uint idx; ${typePrefix} data${typeSuffix};};\n"
			"layout(location = 0) in InOutDataStruct inParam[];\n"
		:
			"layout (location=0) in ${typePrefix} inData[]${typeSuffix};\n"
		)
		+ string(
		((m_testParams.testSubtype == TEST_SUBTYPE_TESS_SHADER) || (m_testParams.testSubtype == TEST_SUBTYPE_TESSGEOM_SHADER)) ?
			"layout (location=0) flat out InOutDataStruct outParam;\n"
		:
			"layout (location=0) out ${typePrefix} outData${typeSuffix};\n"
		)
		+ string(
		"\n"
		"void main ()\n"
		"{\n"
		"    gl_Position = gl_in[0].gl_Position;\n")
		+ string(
		((m_testParams.testSubtype == TEST_SUBTYPE_TESS_SHADER) || (m_testParams.testSubtype == TEST_SUBTYPE_TESSGEOM_SHADER)) ?
			"    outParam = inParam[${i0}];\n"
		:
			"    outData = inData[${i0}];\n"
		)
		+ string(
		"    EmitVertex();\n"

		"    gl_Position = gl_in[1].gl_Position;\n")
		+ string(
		((m_testParams.testSubtype == TEST_SUBTYPE_TESS_SHADER) || (m_testParams.testSubtype == TEST_SUBTYPE_TESSGEOM_SHADER)) ?
			"    outParam = inParam[${i1}];\n"
		:
			"    outData = inData[${i1}];\n"
		)
		+ string(
		"    EmitVertex();\n"


		"    gl_Position = gl_in[2].gl_Position;\n")
		+ string(
		((m_testParams.testSubtype == TEST_SUBTYPE_TESS_SHADER) || (m_testParams.testSubtype == TEST_SUBTYPE_TESSGEOM_SHADER)) ?
			"    outParam = inParam[${i2}];\n"
		:
			"    outData = inData[${i2}];\n"
		)
		+ string(
		"    EmitVertex();\n"
		"}\n")
	);
	programCollection.glslSources.add("geom") << glu::GeometrySource(geom.specialize(attributes));
}

void FragmentShadingBarycentricTestCase::initWeightPrograms (SourceCollections& programCollection) const
{
	const string				baryCoordVariable	= m_testParams.perspective
													? "BaryCoord"
													: "BaryCoordNoPersp";
	const string				declspecRef			= m_testParams.perspective ? "" : "noperspective";
	const string				vertShader			=
		"#version 450\n"
		"\n"
		"layout(location = 0) in  vec4 in_position;\n"
		"layout(location = 1) in  vec4 in_color;\n"
		"layout(location = 0) out vec3 color;\n"
		"layout(push_constant) uniform PushConstant { mat4 mvp; } pc;\n"
		"\n"
		"void main()\n"
		"{\n"
		"    color        = in_color.xyz;\n"
		"    gl_Position  = transpose(pc.mvp) * in_position;\n"
		"    gl_PointSize = 1.0;\n"
		"}\n";
	const tcu::StringTemplate	fragShaderReference(
		"#version 450\n"
		"\n"
		"layout(location = 0) ${glslDeclspecRef} in vec3 in_color;\n"
		"layout(location = 0) out vec4 out_color;\n"
		"\n"
		"void main()\n"
		"{\n"
		"    out_color = vec4(in_color, 1.0f);\n"
		"}\n");
	const tcu::StringTemplate	fragShaderTestGLSL(
		"#version 450\n"
		"#extension GL_EXT_fragment_shader_barycentric : require\n"
		"\n"
		"layout(location = 0) pervertexEXT in vec3 in_color[];\n"
		"layout(location = 0) out vec4 out_color;\n"
		"\n"
		"void main()\n"
		"{\n"
		"    vec3 bc = ${glslFormulaeTest};\n"
		"    out_color = vec4(in_color[0] * bc.x + in_color[1] * bc.y + in_color[2] * bc.z, 1.0f);\n"
		"}\n");

	// it is not possible to add centroid/sample qualifiers to gl_BaryCoordEXT/gl_BaryCoordNoPerspEXT
	// but it is possible to do this with SPIR-V - code below is a slightly modified version of fragShaderTestGLSL
	const tcu::StringTemplate fragShaderTestSPIRV(
		"OpCapability Shader\n"
		"OpCapability FragmentBarycentricKHR\n"
		"${spirvAdditionalCapabilities}"
		"OpExtension \"SPV_KHR_fragment_shader_barycentric\"\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Fragment %4 \"main\" %var_BaryCoord %15 %20\n"
		"OpExecutionMode %4 OriginUpperLeft\n"
		"OpDecorate %var_BaryCoord BuiltIn ${spirvBaryCoordVariable}\n"
		"OpDecorate %var_BaryCoord ${spirvBaryCoordDecoration}\n"
		"OpDecorate %15 Location 0\n"
		"OpDecorate %20 Location 0\n"
		"OpDecorate %20 PerVertexKHR\n"
		"%2				= OpTypeVoid\n"
		"%3				= OpTypeFunction %2\n"
		"%6				= OpTypeFloat 32\n"
		"%7				= OpTypeVector %6 3\n"
		"%8				= OpTypePointer Function %7\n"
		"%10			= OpTypePointer Input %7\n"
		"%var_BaryCoord = OpVariable %10 Input\n"
		"%13			= OpTypeVector %6 4\n"
		"%14			= OpTypePointer Output %13\n"
		"%15			= OpVariable %14 Output\n"
		"%16			= OpTypeInt 32 0\n"
		"%17			= OpConstant %16 3\n"
		"%18			= OpTypeArray %7 %17\n"
		"%19			= OpTypePointer Input %18\n"
		"%20			= OpVariable %19 Input\n"
		"%21			= OpTypeInt 32 1\n"
		"%22			= OpConstant %21 0\n"
		"%25			= OpConstant %16 0\n"
		"%26			= OpTypePointer Function %6\n"
		"%30			= OpConstant %21 1\n"
		"%33			= OpConstant %16 1\n"
		"%38			= OpConstant %21 2\n"
		"%41			= OpConstant %16 2\n"
		"%46			= OpConstant %6 1\n"
		"%4				= OpFunction %2 None %3\n"
		"%5				= OpLabel\n"
		"%9				= OpVariable %8 Function\n"
		"%12			= OpLoad %7 %var_BaryCoord\n"
		"OpStore %9 %12\n"
		"%23			= OpAccessChain %10 %20 %22\n"
		"%24			= OpLoad %7 %23\n"
		"%27			= OpAccessChain %26 %9 %25\n"
		"%28			= OpLoad %6 %27\n"
		"%29			= OpVectorTimesScalar %7 %24 %28\n"
		"%31			= OpAccessChain %10 %20 %30\n"
		"%32			= OpLoad %7 %31\n"
		"%34			= OpAccessChain %26 %9 %33\n"
		"%35			= OpLoad %6 %34\n"
		"%36			= OpVectorTimesScalar %7 %32 %35\n"
		"%37			= OpFAdd %7 %29 %36\n"
		"%39			= OpAccessChain %10 %20 %38\n"
		"%40			= OpLoad %7 %39\n"
		"%42			= OpAccessChain %26 %9 %41\n"
		"%43			= OpLoad %6 %42\n"
		"%44			= OpVectorTimesScalar %7 %40 %43\n"
		"%45			= OpFAdd %7 %37 %44\n"
		"%47			= OpCompositeExtract %6 %45 0\n"
		"%48			= OpCompositeExtract %6 %45 1\n"
		"%49			= OpCompositeExtract %6 %45 2\n"
		"%50			= OpCompositeConstruct %13 %47 %48 %49 %46\n"
		"OpStore %15 %50\n"
		"OpReturn\n"
		"OpFunctionEnd\n");

	// use single specialization map for test and reference fragment shaders
	// as well as for spirv version of test shader
	map<string, string> attributes
	{
		{ "glslDeclspecRef",	declspecRef }
	};

	switch (m_testParams.testSubtype)
	{
	case TEST_SUBTYPE_MSAA_INTERPOLATE_AT_CENTROID:
		attributes["glslFormulaeTest"] = std::string("interpolateAtCentroid(gl_") + baryCoordVariable + "EXT)";
		attributes["glslDeclspecRef"] += " centroid";
		break;
	case TEST_SUBTYPE_MSAA_INTERPOLATE_AT_SAMPLE:
		attributes["glslFormulaeTest"] = std::string("interpolateAtSample(gl_") + baryCoordVariable + "EXT, gl_SampleID)";
		attributes["glslDeclspecRef"] += " sample";
		break;
	case TEST_SUBTYPE_MSAA_INTERPOLATE_AT_OFFSET:
		attributes["glslFormulaeTest"] = std::string("interpolateAtOffset(gl_") + baryCoordVariable + "EXT, vec2(gl_SamplePosition - vec2(0.5)))";
		attributes["glslDeclspecRef"] += " sample";
		break;
	case TEST_SUBTYPE_MSAA_CENTROID_QUALIFIER:
		attributes["spirvBaryCoordVariable"]		= baryCoordVariable + "KHR";
		attributes["spirvBaryCoordDecoration"]		= "Centroid";
		attributes["spirvAdditionalCapabilities"]	= "";
		attributes["glslDeclspecRef"]				+= " centroid";
		break;
	case TEST_SUBTYPE_MSAA_SAMPLE_QUALIFIER:
		attributes["spirvBaryCoordVariable"]		= baryCoordVariable + "KHR";
		attributes["spirvBaryCoordDecoration"]		= "Sample";
		attributes["spirvAdditionalCapabilities"]	= "OpCapability SampleRateShading\n";
		attributes["glslDeclspecRef"]				+= " sample";
		break;
	default:
		attributes["glslFormulaeTest"] = std::string("gl_") + baryCoordVariable + "EXT";
		break;
	}

	programCollection.glslSources.add("vert") << glu::VertexSource(vertShader);
	programCollection.glslSources.add("frag_reference") << glu::FragmentSource(fragShaderReference.specialize(attributes));

	// use "spirvBaryCoordVariable" key to determine if we should use glsl or spir-v version of test shader
	if (attributes.count("spirvBaryCoordVariable"))
		programCollection.spirvAsmSources.add("frag_test") << fragShaderTestSPIRV.specialize(attributes);
	else
		programCollection.glslSources.add("frag_test") << glu::FragmentSource(fragShaderTestGLSL.specialize(attributes));
}
}	// anonymous

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx, const std::string& name)
{
	const bool					notused			= false;
	MovePtr<tcu::TestCaseGroup>	group			(new tcu::TestCaseGroup(testCtx, name.c_str(), "Tests fragment shading barycentric extension"));
	MovePtr<tcu::TestCaseGroup>	libGroup		(new tcu::TestCaseGroup(testCtx, "pipeline_library", "Tests using graphics pipeline libraries"));
	MovePtr<tcu::TestCaseGroup>	fastLinkGroup	(new tcu::TestCaseGroup(testCtx, "fast_linked_library", "Tests using graphics pipeline libraries with fast linking"));

	const struct
	{
		PipelineConstructionType		constructionType;
		tcu::TestCaseGroup*				testGroup;
	} constructionTypeCases[] =
	{
		{ PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC,					group.get()			},
		{ PIPELINE_CONSTRUCTION_TYPE_LINK_TIME_OPTIMIZED_LIBRARY,	libGroup.get()		},
		{ PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY,			fastLinkGroup.get()	},
	};

	const struct PrimitiveTestSpec
	{
		VkPrimitiveTopology	topology;
		const char*			name;
	}
	topologies[] =
	{
		{ VK_PRIMITIVE_TOPOLOGY_POINT_LIST							, "point_list"						},
		{ VK_PRIMITIVE_TOPOLOGY_LINE_LIST							, "line_list"						},
		{ VK_PRIMITIVE_TOPOLOGY_LINE_STRIP							, "line_strip"						},
		{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST						, "triangle_list"					},
		{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP						, "triangle_strip"					},
		{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN						, "triangle_fan"					},
		{ VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY			, "line_list_with_adjacency"		},
		{ VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY			, "line_strip_with_adjacency"		},
		{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY		, "triangle_list_with_adjacency"	},
		{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY		, "triangle_strip_with_adjacency"	},
	};
	const glu::DataType	dataTypes[] =
	{
		glu::TYPE_FLOAT,
		glu::TYPE_FLOAT_VEC2,
		glu::TYPE_FLOAT_VEC3,
		glu::TYPE_FLOAT_VEC4,
		glu::TYPE_DOUBLE,
		glu::TYPE_DOUBLE_VEC2,
		glu::TYPE_DOUBLE_VEC3,
		glu::TYPE_DOUBLE_VEC4,
		glu::TYPE_INT,
		glu::TYPE_INT_VEC2,
		glu::TYPE_INT_VEC3,
		glu::TYPE_INT_VEC4,
		glu::TYPE_UINT,
		glu::TYPE_UINT_VEC2,
		glu::TYPE_UINT_VEC3,
		glu::TYPE_UINT_VEC4,
	};
	const struct Perspective
	{
		const char*		name;
		bool			value;
	}
	perspectives[] =
	{
		{ "perspective",	true	},
		{ "noperspective",	false	},
	};
	const struct DynamicIndexing
	{
		const char* name;
		bool		value;
	}
	dynamicIndexings[] =
	{
		{ "static",		false	},
		{ "dynamic",	true	},
	};
	const struct ProvokingVertex
	{
		const char* name;
		bool		value;
	}
	provokingVertices[] =
	{
		{ "provoking_first",	false	},
		{ "provoking_last",		true	},
	};
	const uint32_t rotations[]	= { 0, 85, 95 };
	const struct TopologyInPipeline
	{
		const char*	name;
		bool		value;
	}
	topologiesInPipeline[] =
	{
		{ "pipeline_topology_static",	false	},
		{ "pipeline_topology_dynamic",	true	},
	};

	for (const auto& constructionTypeCase : constructionTypeCases)
	{
		MovePtr<tcu::TestCaseGroup>	testTypeGroup	(new tcu::TestCaseGroup(testCtx, "data", ""));
		const TestType				testType		= TEST_TYPE_DATA;

		for (size_t provokingVertexNdx = 0; provokingVertexNdx < DE_LENGTH_OF_ARRAY(provokingVertices); ++provokingVertexNdx)
		{
			MovePtr<tcu::TestCaseGroup>	provokingVertexGroup	(new tcu::TestCaseGroup(testCtx, provokingVertices[provokingVertexNdx].name, ""));
			const bool					provokingVertexLast		= provokingVertices[provokingVertexNdx].value;

			for (size_t dynamicNdx = 0; dynamicNdx < DE_LENGTH_OF_ARRAY(dynamicIndexings); ++dynamicNdx)
			{
				MovePtr<tcu::TestCaseGroup>	dynamicIndexingGroup	(new tcu::TestCaseGroup(testCtx, dynamicIndexings[dynamicNdx].name, ""));
				const bool					dynamicIndexing			= dynamicIndexings[dynamicNdx].value;

				for (size_t topologyNdx = 0; topologyNdx < DE_LENGTH_OF_ARRAY(topologies); ++topologyNdx)
				{
					MovePtr<tcu::TestCaseGroup>	topologyGroup	(new tcu::TestCaseGroup(testCtx, topologies[topologyNdx].name, ""));
					const VkPrimitiveTopology	topology		= topologies[topologyNdx].topology;

					for (size_t aggregateNdx = 0; aggregateNdx < 3; ++aggregateNdx)
					{
						const string				aggregateName	= aggregateNdx == 0 ? "type"
																	: aggregateNdx == 1 ? "struct"
																	: "array" + de::toString(aggregateNdx);
						MovePtr<tcu::TestCaseGroup>	aggregateGroup	(new tcu::TestCaseGroup(testCtx, aggregateName.c_str(), ""));

						for (size_t dataTypeNdx = 0; dataTypeNdx < DE_LENGTH_OF_ARRAY(dataTypes); ++dataTypeNdx)
						{
							const glu::DataType	dataType		= dataTypes[dataTypeNdx];
							const char*			dataTypeName	= getDataTypeName(dataType);

							const TestParams	testParams
							{
								constructionTypeCase.constructionType,
								testType,				//  TestType				testType;
								TEST_SUBTYPE_DEFAULT,	//  TestSubtype				testSubtype;
								topology,				//  VkPrimitiveTopology		topology;
								dynamicIndexing,		//  bool					dynamicIndexing;
								aggregateNdx,			//  size_t					aggregate;
								dataType,				//  glu::DataType			dataType;
								DATA_TEST_WIDTH,		//  uint32_t				width;
								DATA_TEST_HEIGHT,		//  uint32_t				height;
								notused,				//  bool					perspective;
								provokingVertexLast,	//  bool					provokingVertexLast;
								(uint32_t)notused,		//  uint32_t				rotation;
								notused,				//  bool					dynamicTopologyInPipeline
								VK_SAMPLE_COUNT_1_BIT,	//  VkSampleCountFlagBits	sampleCount;
							};

							aggregateGroup->addChild(new FragmentShadingBarycentricTestCase(testCtx, dataTypeName, "", testParams));
						}

						topologyGroup->addChild(aggregateGroup.release());
					}

					dynamicIndexingGroup->addChild(topologyGroup.release());
				}

				provokingVertexGroup->addChild(dynamicIndexingGroup.release());
			}

			testTypeGroup->addChild(provokingVertexGroup.release());
		}

		{
			MovePtr<tcu::TestCaseGroup> miscGroup(new tcu::TestCaseGroup(testCtx, "misc", ""));
			const TestParams testParams
			{
				constructionTypeCase.constructionType,
				TEST_TYPE_DATA,							//  TestType				testType;
				TEST_SUBTYPE_PERVERTEX_CORRECTNESS,		//  TestSubtype				testSubtype;
				VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	//  VkPrimitiveTopology		topology;
				notused,								//  bool					dynamicIndexing;
				notused,								//  size_t					aggregate;
				glu::TYPE_FLOAT_VEC2,					//  glu::DataType			dataType;
				DATA_TEST_WIDTH,						//  uint32_t				width;
				DATA_TEST_HEIGHT,						//  uint32_t				height;
				notused,								//  bool					perspective;
				notused,								//  bool					provokingVertexLast;
				(uint32_t)notused,						//  uint32_t				rotation;
				notused,								//  bool					dynamicTopologyInPipeline
				VK_SAMPLE_COUNT_1_BIT,					//  VkSampleCountFlagBits	sampleCount;
			};
			miscGroup->addChild(new FragmentShadingBarycentricTestCase(testCtx, "pervertex_correctness", "", testParams));
			testTypeGroup->addChild(miscGroup.release());

		}

		{
			MovePtr<tcu::TestCaseGroup> scGroup(new tcu::TestCaseGroup(testCtx, "shader_combos", ""));
			for (uint32_t testSubType = (uint32_t) TEST_SUBTYPE_TESS_SHADER; testSubType <= (uint32_t) TEST_SUBTYPE_TESSGEOM_SHADER; testSubType++)
			{
				VkPrimitiveTopology	primitiveType	=	(((TestSubtype) testSubType) == TEST_SUBTYPE_GEOMETRY_SHADER) ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
				const std::string	shaderComboName	=	getShaderComboName(testSubType);

				MovePtr<tcu::TestCaseGroup> scSubGroup(new tcu::TestCaseGroup(testCtx, shaderComboName.c_str(), ""));

				for (size_t aggregateNdx = 0; aggregateNdx < 3; ++aggregateNdx)
				{
					const string				aggregateName	= aggregateNdx == 0 ? "type"
																: aggregateNdx == 1 ? "struct"
																: "array" + de::toString(aggregateNdx);
					MovePtr<tcu::TestCaseGroup>	aggregateGroup	(new tcu::TestCaseGroup(testCtx, aggregateName.c_str(), ""));

					for (size_t dataTypeNdx = 0; dataTypeNdx < DE_LENGTH_OF_ARRAY(dataTypes); ++dataTypeNdx)
					{
						const glu::DataType	dataType		= dataTypes[dataTypeNdx];
						const char*			dataTypeName	= getDataTypeName(dataType);

						const TestParams testParamsShaders
						{
							constructionTypeCase.constructionType,
							TEST_TYPE_DATA,							//  TestType				testType;
							(TestSubtype) testSubType,				//  TestSubtype				testSubtype;
							primitiveType,							//  VkPrimitiveTopology		topology;
							notused,								//  bool					dynamicIndexing;
							aggregateNdx,							//  size_t					aggregate;
							dataType,								//  glu::DataType			dataType;
							DATA_TEST_WIDTH,						//  uint32_t				width;
							DATA_TEST_HEIGHT,						//  uint32_t				height;
							notused,								//  bool					perspective;
							notused,								//  bool					provokingVertexLast;
							(uint32_t)notused,						//  uint32_t				rotation;
							notused,								//  bool					dynamicTopologyInPipeline
							VK_SAMPLE_COUNT_1_BIT,					//  VkSampleCountFlagBits	sampleCount;
						};
						aggregateGroup->addChild(new FragmentShadingBarycentricTestCase(testCtx, dataTypeName, "", testParamsShaders));
					}
					scSubGroup->addChild(aggregateGroup.release());
				}
				scGroup->addChild(scSubGroup.release());
			}
			testTypeGroup->addChild(scGroup.release());
		}

		constructionTypeCase.testGroup->addChild(testTypeGroup.release());
	}

	for (const auto& constructionTypeCase : constructionTypeCases)
	{
		const struct MsaaTestCase
		{
			const char*				name;
			VkSampleCountFlagBits	samples;
			TestSubtype				subtype;
		}
		msaaCases[] =
		{
			{ "single_sample",					VK_SAMPLE_COUNT_1_BIT,	TEST_SUBTYPE_DEFAULT },
			{ "msaa_interpolate_at_centroid",	VK_SAMPLE_COUNT_4_BIT,	TEST_SUBTYPE_MSAA_INTERPOLATE_AT_CENTROID },
			{ "msaa_interpolate_at_sample",		VK_SAMPLE_COUNT_4_BIT,	TEST_SUBTYPE_MSAA_INTERPOLATE_AT_SAMPLE },
			{ "msaa_interpolate_at_offset",		VK_SAMPLE_COUNT_4_BIT,	TEST_SUBTYPE_MSAA_INTERPOLATE_AT_OFFSET },
			{ "msaa_centroid_qualifier",		VK_SAMPLE_COUNT_4_BIT,	TEST_SUBTYPE_MSAA_CENTROID_QUALIFIER },
			{ "msaa_sample_qualifier",			VK_SAMPLE_COUNT_4_BIT,	TEST_SUBTYPE_MSAA_SAMPLE_QUALIFIER },
		};

		MovePtr<tcu::TestCaseGroup>	testTypeGroup	(new tcu::TestCaseGroup(testCtx, "weights", ""));
		const TestType				testType		= TEST_TYPE_WEIGHTS;

		for (size_t topologyInPipelineNdx = 0; topologyInPipelineNdx < DE_LENGTH_OF_ARRAY(topologiesInPipeline); ++topologyInPipelineNdx)
		{
			MovePtr<tcu::TestCaseGroup>	topologyInPipelineGroup	(new tcu::TestCaseGroup(testCtx, topologiesInPipeline[topologyInPipelineNdx].name, ""));
			const bool					topologyInPipeline		= topologiesInPipeline[topologyInPipelineNdx].value;

			if (topologyInPipeline)
			{
				for (size_t msaaCaseNdx = 0; msaaCaseNdx < DE_LENGTH_OF_ARRAY(msaaCases); ++msaaCaseNdx)
				{
					MovePtr<tcu::TestCaseGroup> msaaGroup(new tcu::TestCaseGroup(testCtx, msaaCases[msaaCaseNdx].name, ""));
					for (size_t topologyNdx = 0; topologyNdx < DE_LENGTH_OF_ARRAY(topologies); ++topologyNdx)
					{
						MovePtr<tcu::TestCaseGroup>	topologyGroup(new tcu::TestCaseGroup(testCtx, topologies[topologyNdx].name, ""));
						const VkPrimitiveTopology	topology = topologies[topologyNdx].topology;
						const bool					testableTopology = isPrimitiveTopologyLine(topology) || isPrimitiveTopologyTriangle(topology);

						if (!testableTopology)
							continue;

						for (size_t perspectiveNdx = 0; perspectiveNdx < DE_LENGTH_OF_ARRAY(perspectives); ++perspectiveNdx)
						{
							const bool	perspective = perspectives[perspectiveNdx].value;
							const char* perspectiveName = perspectives[perspectiveNdx].name;

							const TestParams testParams
							{
								constructionTypeCase.constructionType,
								testType,							//  TestType				testType;
								msaaCases[msaaCaseNdx].subtype,		//  TestSubtype				testSubtype;
								topology,							//  VkPrimitiveTopology		topology;
								notused,							//  bool					dynamicIndexing;
								(size_t)notused,					//  size_t					aggregate;
								(glu::DataType)notused,				//  glu::DataType			dataType;
								WEIGHT_TEST_WIDTH,					//  uint32_t				width;
								WEIGHT_TEST_HEIGHT,					//  uint32_t				height;
								perspective,						//  bool					perspective;
								false,								//  bool					provokingVertexLast;
								0,									//  uint32_t				rotation;
								topologyInPipeline,					//  bool					dynamicTopologyInPipeline
								msaaCases[msaaCaseNdx].samples,		//  VkSampleCountFlagBits	sampleCount;
							};

							topologyGroup->addChild(new FragmentShadingBarycentricTestCase(testCtx, perspectiveName, "", testParams));
						}
						msaaGroup->addChild(topologyGroup.release());
					}
					topologyInPipelineGroup->addChild(msaaGroup.release());
				}
			}
			else
			{
				for (size_t rotationNdx = 0; rotationNdx < DE_LENGTH_OF_ARRAY(rotations); ++rotationNdx)
				{
					const uint32_t				rotation		= rotations[rotationNdx];
					MovePtr<tcu::TestCaseGroup>	rotationGroup	(new tcu::TestCaseGroup(testCtx, de::toString(rotation).c_str(), ""));

					for (size_t topologyNdx = 0; topologyNdx < DE_LENGTH_OF_ARRAY(topologies); ++topologyNdx)
					{
						const VkPrimitiveTopology	topology		= topologies[topologyNdx].topology;
						MovePtr<tcu::TestCaseGroup>	topologyGroup	(new tcu::TestCaseGroup(testCtx, topologies[topologyNdx].name, ""));

						for (size_t perspectiveNdx = 0; perspectiveNdx < DE_LENGTH_OF_ARRAY(perspectives); ++perspectiveNdx)
						{
							const bool	perspective		= perspectives[perspectiveNdx].value;
							const char*	perspectiveName	= perspectives[perspectiveNdx].name;

							const TestParams testParams
							{
								constructionTypeCase.constructionType,
								testType,				//  TestType				testType;
								TEST_SUBTYPE_DEFAULT,	//  TestSubtype				testSubtype;
								topology,				//  VkPrimitiveTopology		topology;
								notused,				//  bool					dynamicIndexing;
								(size_t)-1,				//  size_t					aggregate;
								glu::TYPE_INVALID,		//  glu::DataType			dataType;
								WEIGHT_TEST_WIDTH,		//  uint32_t				width;
								WEIGHT_TEST_HEIGHT,		//  uint32_t				height;
								perspective,			//  bool					perspective;
								false,					//  bool					provokingVertexLast;
								rotation,				//  uint32_t				rotation;
								topologyInPipeline,		//  bool					dynamicTopologyInPipeline
								VK_SAMPLE_COUNT_1_BIT,	//  VkSampleCountFlagBits	sampleCount;
							};

							topologyGroup->addChild(new FragmentShadingBarycentricTestCase(testCtx, perspectiveName, "", testParams));
						}

						rotationGroup->addChild(topologyGroup.release());
					}

					topologyInPipelineGroup->addChild(rotationGroup.release());
				}
			}

			testTypeGroup->addChild(topologyInPipelineGroup.release());
		}

		constructionTypeCase.testGroup->addChild(testTypeGroup.release());
	}

	group->addChild(libGroup.release());
	group->addChild(fastLinkGroup.release());

	return group.release();
}

}	// FragmentShadingBarycentric
}	// vkt
