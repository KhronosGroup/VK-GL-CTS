/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
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
 * \brief Tests fragment density map extension ( VK_EXT_fragment_density_map )
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassFragmentDensityMapTests.hpp"
#include "pipeline/vktPipelineImageUtil.hpp"
#include "deMath.h"
#include "vktTestCase.hpp"
#include "vkImageUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuTestLog.hpp"
#include <sstream>
#include <vector>

// Each test generates an image with a color gradient where all colors should be unique when rendered without density map
// ( the number of each color in a histogram should be 1 ).
// The whole density map has the same values defined by input fragment area ( one of the test input parameters ).
// With density map enabled - the number of each color in a histogram should be [ fragmentArea.x * fragmentArea.y ].
//
// Additionally test checks if gl_FragSizeEXT shader variable has proper value ( as defined by fragmentArea input parameter ).
//
// static_* tests use density map loaded from CPU.
// dynamic_* tests use density map rendered on a GPU in a separate render pass
// *_nonsubsampled tests check if it's possible to use nonsubsampled images instead of subsampled ones
// There are 3 render passes performed during the test:
//  - render pass that produces density map ( this rp is skipped when density map is static )
//  - render pass that produces subsampled image using density map
//  - render pass that copies subsampled image to traditional image using sampler with VK_SAMPLER_CREATE_SUBSAMPLED_BIT_EXT flag.
//    ( because subsampled images cannot be retrieved to CPU in any other way ).

namespace vkt
{

namespace renderpass
{

using namespace vk;

namespace
{

// set value of DRY_RUN_WITHOUT_FDM_EXTENSION to 1 if you want to check the correctness of the code without using VK_EXT_fragment_density_map extension
#define DRY_RUN_WITHOUT_FDM_EXTENSION 0

struct TestParams
{
	TestParams(bool dynamicDensity, bool nonSubsampled, const tcu::UVec2& area)
		: dynamicDensityMap{ dynamicDensity }, nonSubsampledImages{ nonSubsampled }, fragmentArea{ area }, densityMapFormat{ VK_FORMAT_R8G8_UNORM }
	{}
	bool		dynamicDensityMap;
	bool		nonSubsampledImages;
	tcu::UVec2	fragmentArea;
	VkFormat	densityMapFormat;
};

struct Vertex4RGBA
{
	tcu::Vec4	position;
	tcu::Vec4	color;
};

std::vector<Vertex4RGBA> createFullscreenQuadRG(void)
{
	const Vertex4RGBA lowerLeftVertex	= { tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f),		tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f) };
	const Vertex4RGBA upperLeftVertex	= { tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),	tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f) };
	const Vertex4RGBA lowerRightVertex	= { tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),		tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f) };
	const Vertex4RGBA upperRightVertex	= { tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f),		tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f) };

	return
	{
		lowerLeftVertex, lowerRightVertex, upperLeftVertex,
		upperLeftVertex, lowerRightVertex, upperRightVertex
	};
}

std::vector<Vertex4RGBA> createFullscreenQuadDensity(float densityX, float densityY)
{
	const Vertex4RGBA lowerLeftVertex	= { tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f),		tcu::Vec4(densityX, densityY, 0.0f, 1.0f) };
	const Vertex4RGBA upperLeftVertex	= { tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),	tcu::Vec4(densityX, densityY, 0.0f, 1.0f) };
	const Vertex4RGBA lowerRightVertex	= { tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),		tcu::Vec4(densityX, densityY, 0.0f, 1.0f) };
	const Vertex4RGBA upperRightVertex	= { tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f),		tcu::Vec4(densityX, densityY, 0.0f, 1.0f) };

	return
	{
		lowerLeftVertex, lowerRightVertex, upperLeftVertex,
		upperLeftVertex, lowerRightVertex, upperRightVertex
	};
};

template <typename T>
void createVertexBuffer(const DeviceInterface&		vk,
						VkDevice					vkDevice,
						const deUint32&				queueFamilyIndex,
						SimpleAllocator&			memAlloc,
						const std::vector<T>&		vertices,
						Move<VkBuffer>&				vertexBuffer,
						de::MovePtr<Allocation>&	vertexAlloc)
{
	const VkBufferCreateInfo vertexBufferParams =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,			// VkStructureType		sType;
		DE_NULL,										// const void*			pNext;
		0u,												// VkBufferCreateFlags	flags;
		(VkDeviceSize)(sizeof(T) * vertices.size()),	// VkDeviceSize			size;
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,				// VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode		sharingMode;
		1u,												// deUint32				queueFamilyIndexCount;
		&queueFamilyIndex								// const deUint32*		pQueueFamilyIndices;
	};

	vertexBuffer	= createBuffer(vk, vkDevice, &vertexBufferParams);
	vertexAlloc		= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *vertexBuffer), MemoryRequirement::HostVisible);
	VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, vertexAlloc->getMemory(), vertexAlloc->getOffset()));

	// Upload vertex data
	deMemcpy(vertexAlloc->getHostPtr(), vertices.data(), vertices.size() * sizeof(T));
	flushAlloc(vk, vkDevice, *vertexAlloc);
}

template<typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep, typename RenderPassCreateInfo>
Move<VkRenderPass> createRenderPassProduceDynamicDensityMap(const DeviceInterface&	vk,
															VkDevice				vkDevice,
															const TestParams&		testParams)
{
	VkImageLayout densityPassFinalLayout = testParams.dynamicDensityMap ? VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	std::vector<AttachmentDesc>		attachmentDescriptions =
	{
		{
			DE_NULL,															// const void*						pNext
			(VkAttachmentDescriptionFlags)0,									// VkAttachmentDescriptionFlags		flags
			testParams.densityMapFormat,										// VkFormat							format
			VK_SAMPLE_COUNT_1_BIT,												// VkSampleCountFlagBits			samples
			VK_ATTACHMENT_LOAD_OP_CLEAR,										// VkAttachmentLoadOp				loadOp
			VK_ATTACHMENT_STORE_OP_STORE,										// VkAttachmentStoreOp				storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,									// VkAttachmentLoadOp				stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,									// VkAttachmentStoreOp				stencilStoreOp
			VK_IMAGE_LAYOUT_UNDEFINED,											// VkImageLayout					initialLayout
			densityPassFinalLayout												// VkImageLayout					finalLayout
		}
	};

	std::vector<AttachmentRef> colorAttachmentRefs
	{
		{ DE_NULL, 0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT }
	};

	std::vector<SubpassDesc>	subpassDescriptions
	{
		{
			DE_NULL,
			(VkSubpassDescriptionFlags)0,										// VkSubpassDescriptionFlags		flags
			VK_PIPELINE_BIND_POINT_GRAPHICS,									// VkPipelineBindPoint				pipelineBindPoint
			0u,																	// deUint32							viewMask
			0u,																	// deUint32							inputAttachmentCount
			DE_NULL,															// const VkAttachmentReference*		pInputAttachments
			static_cast<deUint32>(colorAttachmentRefs.size()),					// deUint32							colorAttachmentCount
			colorAttachmentRefs.data(),											// const VkAttachmentReference*		pColorAttachments
			DE_NULL,															// const VkAttachmentReference*		pResolveAttachments
			DE_NULL,															// const VkAttachmentReference*		pDepthStencilAttachment
			0u,																	// deUint32							preserveAttachmentCount
			DE_NULL																// const deUint32*					pPreserveAttachments
		}
	};

	std::vector<SubpassDep>		subpassDependencies;
	if ( testParams.dynamicDensityMap )
	{
		subpassDependencies.emplace_back(
			SubpassDep(
				DE_NULL,														// const void*				pNext
				0u,																// uint32_t					srcSubpass
				VK_SUBPASS_EXTERNAL,											// uint32_t					dstSubpass
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,					// VkPipelineStageFlags		srcStageMask
				VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT,				// VkPipelineStageFlags		dstStageMask
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,							// VkAccessFlags			srcAccessMask
				VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT,					// VkAccessFlags			dstAccessMask
				VK_DEPENDENCY_BY_REGION_BIT,									// VkDependencyFlags		dependencyFlags
				0u																// deInt32					viewOffset
			)
		);
	};

	const RenderPassCreateInfo	renderPassInfo(
		DE_NULL,																// const void*						pNext
		(VkRenderPassCreateFlags)0,												// VkRenderPassCreateFlags			flags
		static_cast<deUint32>(attachmentDescriptions.size()),					// deUint32							attachmentCount
		attachmentDescriptions.data(),											// const VkAttachmentDescription*	pAttachments
		static_cast<deUint32>(subpassDescriptions.size()),						// deUint32							subpassCount
		subpassDescriptions.data(),												// const VkSubpassDescription*		pSubpasses
		static_cast<deUint32>(subpassDependencies.size()),						// deUint32							dependencyCount
		(!testParams.dynamicDensityMap) ? DE_NULL : subpassDependencies.data(),	// const VkSubpassDependency*		pDependencies
		0u,																		// deUint32							correlatedViewMaskCount
		DE_NULL																	// const deUint32*					pCorrelatedViewMasks
	);

	return renderPassInfo.createRenderPass(vk, vkDevice);
}

template<typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep, typename RenderPassCreateInfo>
Move<VkRenderPass> createRenderPassProduceSubsampledImage(const DeviceInterface&	vk,
													 VkDevice				vkDevice,
													 const TestParams&		testParams)
{
	DE_UNREF(testParams);
	std::vector<AttachmentDesc>		attachmentDescriptions
	{
		// Output color attachment
		{
			DE_NULL,																// const void*						pNext
			(VkAttachmentDescriptionFlags)0,										// VkAttachmentDescriptionFlags		flags
			VK_FORMAT_R8G8B8A8_UNORM,												// VkFormat							format
			VK_SAMPLE_COUNT_1_BIT,													// VkSampleCountFlagBits			samples
			VK_ATTACHMENT_LOAD_OP_CLEAR,											// VkAttachmentLoadOp				loadOp
			VK_ATTACHMENT_STORE_OP_STORE,											// VkAttachmentStoreOp				storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,										// VkAttachmentLoadOp				stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,										// VkAttachmentStoreOp				stencilStoreOp
			VK_IMAGE_LAYOUT_UNDEFINED,												// VkImageLayout					initialLayout
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL								// VkImageLayout					finalLayout
		}
	};

#if !DRY_RUN_WITHOUT_FDM_EXTENSION
	{
		attachmentDescriptions.emplace_back(
			AttachmentDesc(
				DE_NULL,															// const void*						pNext
				(VkAttachmentDescriptionFlags)0,									// VkAttachmentDescriptionFlags		flags
				testParams.densityMapFormat,										// VkFormat							format
				VK_SAMPLE_COUNT_1_BIT,												// VkSampleCountFlagBits			samples
				VK_ATTACHMENT_LOAD_OP_LOAD,											// VkAttachmentLoadOp				loadOp
				VK_ATTACHMENT_STORE_OP_DONT_CARE,									// VkAttachmentStoreOp				storeOp
				VK_ATTACHMENT_LOAD_OP_DONT_CARE,									// VkAttachmentLoadOp				stencilLoadOp
				VK_ATTACHMENT_STORE_OP_DONT_CARE,									// VkAttachmentStoreOp				stencilStoreOp
				VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT,					// VkImageLayout					initialLayout
				VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT					// VkImageLayout					finalLayout
			)
		);
	}
#endif

	std::vector<AttachmentRef> colorAttachmentRefs
	{
		{ DE_NULL, 0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT }
	};

	std::vector<SubpassDesc>	subpassDescriptions
	{
		{
			DE_NULL,
			(VkSubpassDescriptionFlags)0,											// VkSubpassDescriptionFlags	flags
			VK_PIPELINE_BIND_POINT_GRAPHICS,										// VkPipelineBindPoint			pipelineBindPoint
			0u,																		// deUint32						viewMask
			0u,																		// deUint32						inputAttachmentCount
			DE_NULL,																// const VkAttachmentReference*	pInputAttachments
			static_cast<deUint32>(colorAttachmentRefs.size()),						// deUint32						colorAttachmentCount
			colorAttachmentRefs.data(),												// const VkAttachmentReference*	pColorAttachments
			DE_NULL,																// const VkAttachmentReference*	pResolveAttachments
			DE_NULL,																// const VkAttachmentReference*	pDepthStencilAttachment
			0u,																		// deUint32						preserveAttachmentCount
			DE_NULL																	// const deUint32*				pPreserveAttachments
		}
	};

	std::vector<SubpassDep>		subpassDependencies
	{
		{
			DE_NULL,																// const void*				pNext
			0u,																		// uint32_t					srcSubpass
			VK_SUBPASS_EXTERNAL,													// uint32_t					dstSubpass
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,							// VkPipelineStageFlags		srcStageMask
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,									// VkPipelineStageFlags		dstStageMask
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,									// VkAccessFlags			srcAccessMask
			VK_ACCESS_SHADER_READ_BIT,												// VkAccessFlags			dstAccessMask
			VK_DEPENDENCY_BY_REGION_BIT,											// VkDependencyFlags		dependencyFlags
			0u																		// deInt32					viewOffset
		}
	};

	VkRenderPassFragmentDensityMapCreateInfoEXT renderPassFragmentDensityMap;
	renderPassFragmentDensityMap.sType							= VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT;
	renderPassFragmentDensityMap.pNext							= DE_NULL;
	renderPassFragmentDensityMap.fragmentDensityMapAttachment	= { 1, VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT };

#if !DRY_RUN_WITHOUT_FDM_EXTENSION
	const void* renderPassInfoPNext = (const void*)&renderPassFragmentDensityMap;
#else
	const void* renderPassInfoPNext = DE_NULL;
#endif
	const RenderPassCreateInfo	renderPassInfo(
		renderPassInfoPNext,														// const void*						pNext
		(VkRenderPassCreateFlags)0,													// VkRenderPassCreateFlags			flags
		static_cast<deUint32>(attachmentDescriptions.size()),						// deUint32							attachmentCount
		attachmentDescriptions.data(),												// const VkAttachmentDescription*	pAttachments
		static_cast<deUint32>(subpassDescriptions.size()),							// deUint32							subpassCount
		subpassDescriptions.data(),													// const VkSubpassDescription*		pSubpasses
		static_cast<deUint32>(subpassDependencies.size()),							// deUint32							dependencyCount
		subpassDependencies.data(),													// const VkSubpassDependency*		pDependencies
		0u,																			// deUint32							correlatedViewMaskCount
		DE_NULL																		// const deUint32*					pCorrelatedViewMasks
	);

	return renderPassInfo.createRenderPass(vk, vkDevice);
}

template<typename AttachmentDesc, typename AttachmentRef, typename SubpassDesc, typename SubpassDep, typename RenderPassCreateInfo>
Move<VkRenderPass> createRenderPassOutputSubsampledImage(const DeviceInterface&	vk,
													VkDevice				vkDevice,
													const TestParams&		testParams)
{
	DE_UNREF(testParams);
	// copy subsampled image to ordinary image - you cannot retrieve subsampled image to CPU in any way. You must first convert it into plain image through rendering
	std::vector<AttachmentDesc>		attachmentDescriptions =
	{
		// output attachment
		AttachmentDesc(
			DE_NULL,											// const void*						pNext
			(VkAttachmentDescriptionFlags)0,					// VkAttachmentDescriptionFlags		flags
			VK_FORMAT_R8G8B8A8_UNORM,							// VkFormat							format
			VK_SAMPLE_COUNT_1_BIT,								// VkSampleCountFlagBits			samples
			VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp				loadOp
			VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp				stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp				stencilStoreOp
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout					initialLayout
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout					finalLayout
		),
	};

	std::vector<AttachmentRef> colorAttachmentRefs
	{
		{ DE_NULL, 0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT }
	};

	std::vector<SubpassDesc>	subpassDescriptions =
	{
		{
			DE_NULL,
			(VkSubpassDescriptionFlags)0,						// VkSubpassDescriptionFlags		flags
			VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint				pipelineBindPoint
			0u,													// deUint32							viewMask
			0u,													// deUint32							inputAttachmentCount
			DE_NULL,											// const VkAttachmentReference*		pInputAttachments
			static_cast<deUint32>(colorAttachmentRefs.size()),	// deUint32							colorAttachmentCount
			colorAttachmentRefs.data(),							// const VkAttachmentReference*		pColorAttachments
			DE_NULL,											// const VkAttachmentReference*		pResolveAttachments
			DE_NULL,											// const VkAttachmentReference*		pDepthStencilAttachment
			0u,													// deUint32							preserveAttachmentCount
			DE_NULL												// const deUint32*					pPreserveAttachments
		}
	};

	const RenderPassCreateInfo	renderPassInfo(
		DE_NULL,												// const void*						pNext
		(VkRenderPassCreateFlags)0,								// VkRenderPassCreateFlags			flags
		static_cast<deUint32>(attachmentDescriptions.size()),	// deUint32							attachmentCount
		attachmentDescriptions.data(),							// const VkAttachmentDescription*	pAttachments
		static_cast<deUint32>(subpassDescriptions.size()),		// deUint32							subpassCount
		subpassDescriptions.data(),								// const VkSubpassDescription*		pSubpasses
		0,														// deUint32							dependencyCount
		DE_NULL,												// const VkSubpassDependency*		pDependencies
		0u,														// deUint32							correlatedViewMaskCount
		DE_NULL													// const deUint32*					pCorrelatedViewMasks
	);

	return renderPassInfo.createRenderPass(vk, vkDevice);
}

Move<VkFramebuffer> createFrameBuffer( const DeviceInterface& vk, VkDevice vkDevice, VkRenderPass renderPass, const tcu::UVec2& renderSize, const std::vector<VkImageView>& imageViews)
{
	const VkFramebufferCreateInfo	framebufferParams =
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		0u,											// VkFramebufferCreateFlags	flags;
		renderPass,									// VkRenderPass				renderPass;
		static_cast<deUint32>(imageViews.size()),	// deUint32					attachmentCount;
		imageViews.data(),							// const VkImageView*		pAttachments;
		renderSize.x(),								// deUint32					width;
		renderSize.y(),								// deUint32					height;
		1u											// deUint32					layers;
	};

	return createFramebuffer(vk, vkDevice, &framebufferParams);
}

class FragmentDensityMapTest : public vkt::TestCase
{
public:
										FragmentDensityMapTest	(tcu::TestContext&	testContext,
																 const std::string&	name,
																 const std::string&	description,
																 const TestParams&	testParams);
	virtual void						initPrograms			(SourceCollections&	sourceCollections) const;
	virtual TestInstance*				createInstance			(Context&			context) const;
	virtual void						checkSupport			(Context& context) const;

private:
	const TestParams					m_testParams;
};

class FragmentDensityMapTestInstance : public vkt::TestInstance
{
public:
													FragmentDensityMapTestInstance	(Context&				context,
																					 const TestParams&		testParams);
	virtual tcu::TestStatus							iterate							(void);
private:
	tcu::TestStatus									verifyImage						(void);

	TestParams										m_testParams;
	const tcu::UVec2								m_renderSize;
	const tcu::UVec2								m_densityMapSize;

	VkPhysicalDeviceFragmentDensityMapPropertiesEXT	m_fragmentDensityMapProperties;

	Move<VkCommandPool>								m_cmdPool;

	Move<VkImage>									m_densityMapImage;
	de::MovePtr<Allocation>							m_densityMapImageAlloc;
	Move<VkImageView>								m_densityMapImageView;

	Move<VkImage>									m_colorImage;
	de::MovePtr<Allocation>							m_colorImageAlloc;
	Move<VkImageView>								m_colorImageView;

	Move<VkImage>									m_outputImage;
	de::MovePtr<Allocation>							m_outputImageAlloc;
	Move<VkImageView>								m_outputImageView;

	Move<VkSampler>									m_colorSampler;

	Move<VkRenderPass>								m_renderPassProduceDynamicDensityMap;
	Move<VkRenderPass>								m_renderPassProduceSubsampledImage;
	Move<VkRenderPass>								m_renderPassOutputSubsampledImage;
	Move<VkFramebuffer>								m_framebufferProduceDynamicDensityMap;
	Move<VkFramebuffer>								m_framebufferProduceSubsampledImage;
	Move<VkFramebuffer>								m_framebufferOutputSubsampledImage;

	Move<VkDescriptorSetLayout>						m_descriptorSetLayoutProduceSubsampled;
	Move<VkDescriptorSetLayout>						m_descriptorSetLayoutOutputSubsampledImage;
	Move<VkDescriptorPool>							m_descriptorPoolOutputSubsampledImage;
	Move<VkDescriptorSet>							m_descriptorSetOutputSubsampledImage;

	Move<VkShaderModule>							m_vertexCommonShaderModule;
	Move<VkShaderModule>							m_fragmentShaderModuleProduceSubsampledImage;
	Move<VkShaderModule>							m_fragmentShaderModuleOutputSubsampledImage;

	Move<VkBuffer>									m_vertexBuffer;
	std::vector<Vertex4RGBA>						m_vertices;
	de::MovePtr<Allocation>							m_vertexBufferAlloc;

	Move<VkBuffer>									m_vertexBufferDDM;
	std::vector<Vertex4RGBA>						m_verticesDDM;
	de::MovePtr<Allocation>							m_vertexBufferAllocDDM;

	Move<VkPipelineLayout>							m_pipelineLayoutProduceSubsampledImage;
	Move<VkPipelineLayout>							m_pipelineLayoutOutputSubsampledImage;
	Move<VkPipeline>								m_graphicsPipelineProduceDynamicDensityMap;
	Move<VkPipeline>								m_graphicsPipelineProduceSubsampledImage;
	Move<VkPipeline>								m_graphicsPipelineOutputSubsampledImage;

	Move<VkCommandBuffer>							m_cmdBuffer;
};

FragmentDensityMapTest::FragmentDensityMapTest (tcu::TestContext&	testContext,
												const std::string&	name,
												const std::string&	description,
												const TestParams&	testParams)
	: vkt::TestCase	(testContext, name, description)
	, m_testParams	(testParams)
{
}

void FragmentDensityMapTest::initPrograms(SourceCollections& sourceCollections) const
{
	std::ostringstream densityVertexGLSL;
	densityVertexGLSL <<
		"#version 450\n"
		"layout(location = 0) in  vec4 inPosition;\n"
		"layout(location = 1) in  vec4 inColor;\n"
		"layout(location = 0) out vec4 outColor;\n"
		"layout(location = 1) out vec2 outUV;\n"
		"void main(void)\n"
		"{\n"
		"	gl_Position = inPosition;\n"
		"	outColor = inColor;\n"
		"	outUV = 0.5 * inPosition.xy + vec2(0.5);\n"
		"}\n";
	sourceCollections.glslSources.add("densitymap_vert") << glu::VertexSource(densityVertexGLSL.str());

	std::ostringstream densityFragmentProduceGLSL;
	densityFragmentProduceGLSL <<
		"#version 450\n"
		"#extension GL_EXT_fragment_invocation_density : enable\n"
		"layout(location = 0) in  vec4 inColor;\n"
		"layout(location = 1) in  vec2 inUV;\n"
		"layout(location = 0) out vec4 fragColor;\n"
		"void main(void)\n"
		"{\n"
		"	fragColor = vec4(inColor.x, inColor.y, 1.0/float(gl_FragSizeEXT.x), 1.0/(gl_FragSizeEXT.y));\n"
		"}\n";
	sourceCollections.glslSources.add("densitymap_frag_produce") << glu::FragmentSource(densityFragmentProduceGLSL.str());

	std::ostringstream densityFragmentOutputGLSL;
	densityFragmentOutputGLSL <<
		"#version 450\n"
		"layout(location = 0) in vec4 inColor;\n"
		"layout(location = 1) in vec2 inUV;\n"
		"layout(binding = 0)  uniform sampler2D subsampledImage;\n"
		"layout(location = 0) out vec4 fragColor;\n"
		"void main(void)\n"
		"{\n"
		"	fragColor = texture(subsampledImage, inUV);\n"
		"}\n";
	sourceCollections.glslSources.add("densitymap_frag_output") << glu::FragmentSource(densityFragmentOutputGLSL.str());
}

TestInstance* FragmentDensityMapTest::createInstance(Context& context) const
{
	return new FragmentDensityMapTestInstance(context, m_testParams);
}

void FragmentDensityMapTest::checkSupport(Context& context) const
{
#if !DRY_RUN_WITHOUT_FDM_EXTENSION
	context.requireDeviceFunctionality("VK_EXT_fragment_density_map");

	VkPhysicalDeviceFeatures2 features;
	deMemset(&features, 0, sizeof(VkPhysicalDeviceFeatures2));
	features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

	VkPhysicalDeviceFragmentDensityMapFeaturesEXT fragmentDensityMapFeatures;
	deMemset(&fragmentDensityMapFeatures, 0, sizeof(VkPhysicalDeviceFragmentDensityMapFeaturesEXT));
	fragmentDensityMapFeatures.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT;
	features.pNext						= &fragmentDensityMapFeatures;

	context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features);

	if (!fragmentDensityMapFeatures.fragmentDensityMap)
		TCU_THROW(NotSupportedError, "fragmentDensityMap feature is not supported");
	if (m_testParams.dynamicDensityMap && !fragmentDensityMapFeatures.fragmentDensityMapDynamic)
		TCU_THROW(NotSupportedError, "fragmentDensityMapDynamic feature is not supported");
	if (m_testParams.nonSubsampledImages && !fragmentDensityMapFeatures.fragmentDensityMapNonSubsampledImages)
		TCU_THROW(NotSupportedError, "fragmentDensityMapNonSubsampledImages feature is not supported");
#else
	DE_UNREF(context);
#endif
}

FragmentDensityMapTestInstance::FragmentDensityMapTestInstance(Context&			context,
															const TestParams&	testParams)
	: vkt::TestInstance	( context )
	, m_testParams		( testParams )
	, m_renderSize		( 32u, 32u )
	, m_densityMapSize	( 16u, 16u )
	, m_vertices		( createFullscreenQuadRG() )
	, m_verticesDDM		( createFullscreenQuadDensity(1.0f / static_cast<float>(testParams.fragmentArea.x()), 1.0f / static_cast<float>(testParams.fragmentArea.y())) )
{
	const DeviceInterface&		vk						= m_context.getDeviceInterface();
	const VkDevice				vkDevice				= m_context.getDevice();
	const deUint32				queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	SimpleAllocator				memAlloc				(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
	const VkComponentMapping	componentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

#if !DRY_RUN_WITHOUT_FDM_EXTENSION
	{
		VkPhysicalDeviceProperties2 properties;
		deMemset(&properties, 0, sizeof(VkPhysicalDeviceProperties2));
		properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

		deMemset(&m_fragmentDensityMapProperties, 0, sizeof(VkPhysicalDeviceFragmentDensityMapPropertiesEXT));
		m_fragmentDensityMapProperties.sType	= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT;
		properties.pNext						= &m_fragmentDensityMapProperties;

		context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);
	}
#else
	{
		m_fragmentDensityMapProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT;
		m_fragmentDensityMapProperties.minFragmentDensityTexelSize.width	= 1u;
		m_fragmentDensityMapProperties.maxFragmentDensityTexelSize.width	= 1u;
		m_fragmentDensityMapProperties.minFragmentDensityTexelSize.height	= 1u;
		m_fragmentDensityMapProperties.maxFragmentDensityTexelSize.height	= 1u;
		m_fragmentDensityMapProperties.fragmentDensityInvocations			= DE_FALSE;
		m_testParams.fragmentArea.x()										= 1u;
		m_testParams.fragmentArea.y()										= 1u;
	}
#endif

	// Create density map image
	{
#if !DRY_RUN_WITHOUT_FDM_EXTENSION
		vk::VkImageUsageFlags densityMapImageUsage = m_testParams.dynamicDensityMap ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT : VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT;
#else
		vk::VkImageUsageFlags densityMapImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
#endif
		const VkImageCreateInfo	densityMapImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,											// VkStructureType			sType;
			DE_NULL,																		// const void*				pNext;
			0u,																				// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,																// VkImageType				imageType;
			m_testParams.densityMapFormat,													// VkFormat					format;
			{ m_densityMapSize.x(), m_densityMapSize.y(), 1u },								// VkExtent3D				extent;
			1u,																				// deUint32					mipLevels;
			1u,																				// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,															// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,														// VkImageTiling			tiling;
			densityMapImageUsage,															// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,														// VkSharingMode			sharingMode;
			1u,																				// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,																// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED														// VkImageLayout			initialLayout;
		};

		m_densityMapImage = createImage(vk, vkDevice, &densityMapImageParams);

		// Allocate and bind density map image memory
		VkMemoryRequirements			memoryRequirements = getImageMemoryRequirements(vk, vkDevice, *m_densityMapImage);

		m_densityMapImageAlloc = memAlloc.allocate(memoryRequirements, MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_densityMapImage, m_densityMapImageAlloc->getMemory(), m_densityMapImageAlloc->getOffset()));

		// create and fill staging buffer, copy its data to density map image
#if !DRY_RUN_WITHOUT_FDM_EXTENSION
		tcu::TextureFormat	densityMapTextureFormat = vk::mapVkFormat(m_testParams.densityMapFormat);

		if ( !m_testParams.dynamicDensityMap )
		{
			VkDeviceSize stagingBufferSize = tcu::getPixelSize(densityMapTextureFormat) * m_densityMapSize.x() * m_densityMapSize.y() * 1;
			const vk::VkBufferCreateInfo	stagingBufferCreateInfo =
			{
				vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				DE_NULL,
				0u,									// flags
				stagingBufferSize,					// size
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,	// usage
				vk::VK_SHARING_MODE_EXCLUSIVE,		// sharingMode
				0u,									// queueFamilyCount
				DE_NULL,							// pQueueFamilyIndices
			};
			vk::Move<vk::VkBuffer>			stagingBuffer		= vk::createBuffer(vk, vkDevice, &stagingBufferCreateInfo);
			const vk::VkMemoryRequirements	stagingRequirements = vk::getBufferMemoryRequirements(vk, vkDevice, *stagingBuffer);
			de::MovePtr<vk::Allocation>		stagingAllocation	= memAlloc.allocate(stagingRequirements, MemoryRequirement::HostVisible);
			VK_CHECK(vk.bindBufferMemory(vkDevice, *stagingBuffer, stagingAllocation->getMemory(), stagingAllocation->getOffset()));
			tcu::PixelBufferAccess			stagingBufferAccess	= tcu::PixelBufferAccess(densityMapTextureFormat, m_densityMapSize.x(), m_densityMapSize.y(), 1, stagingAllocation->getHostPtr());

			tcu::Vec4 fragmentArea { 1.0f / static_cast<float>(testParams.fragmentArea.x()), 1.0f / static_cast<float>(testParams.fragmentArea.y()), 0.0f, 1.0f };
			for (int y = 0; y < stagingBufferAccess.getHeight(); y++)
				for (int x = 0; x < stagingBufferAccess.getWidth(); x++)
					stagingBufferAccess.setPixel(fragmentArea, x, y);
			flushAlloc(vk, vkDevice, *stagingAllocation);

			std::vector<VkBufferImageCopy> copyRegions =
			{
				{
					0,													// VkDeviceSize					bufferOffset
					0,													// deUint32						bufferRowLength
					0,													// deUint32						bufferImageHeight
					{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },				// VkImageSubresourceLayers		imageSubresource
					{ 0, 0, 0 },										// VkOffset3D					imageOffset
					{ m_densityMapSize.x(), m_densityMapSize.y(), 1u }	// VkExtent3D					imageExtent
				}
			};

			vk::copyBufferToImage
			(
				vk,
				vkDevice,
				m_context.getUniversalQueue(),
				queueFamilyIndex,
				*stagingBuffer,
				stagingBufferSize,
				copyRegions,
				DE_NULL,
				VK_IMAGE_ASPECT_COLOR_BIT,
				1,
				1,
				*m_densityMapImage,
				VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT,
				VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT
			);
		}
#endif

		//create image view for fragment density map
		deUint32 densityMapImageViewCreateFlags = m_testParams.dynamicDensityMap ? (deUint32)VK_IMAGE_VIEW_CREATE_FRAGMENT_DENSITY_MAP_DYNAMIC_BIT_EXT : 0u;
		const VkImageViewCreateInfo densityMapImageViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,							// VkStructureType			sType;
			DE_NULL,															// const void*				pNext;
			(VkImageViewCreateFlags)densityMapImageViewCreateFlags,				// VkImageViewCreateFlags	flags;
			*m_densityMapImage,													// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,												// VkImageViewType			viewType;
			m_testParams.densityMapFormat,										// VkFormat					format;
			componentMappingRGBA,												// VkChannelMapping			channels;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }						// VkImageSubresourceRange	subresourceRange;
		};

		m_densityMapImageView = createImageView(vk, vkDevice, &densityMapImageViewParams);
	}

	// Create subsampled color image
	{
#if !DRY_RUN_WITHOUT_FDM_EXTENSION
	deUint32 colorImageCreateFlags = m_testParams.nonSubsampledImages ? 0u : (deUint32)VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT;
#else
	deUint32 colorImageCreateFlags = 0u;
#endif
		const VkImageCreateInfo	colorImageParams
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,								// VkStructureType			sType;
			DE_NULL,															// const void*				pNext;
			(VkImageCreateFlags)colorImageCreateFlags,							// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,													// VkImageType				imageType;
			VK_FORMAT_R8G8B8A8_UNORM,											// VkFormat					format;
			{ m_renderSize.x(), m_renderSize.y(), 1u },							// VkExtent3D				extent;
			1u,																	// deUint32					mipLevels;
			1u,																	// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,												// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,											// VkImageTiling			tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,	// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,											// VkSharingMode			sharingMode;
			1u,																	// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,													// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED											// VkImageLayout			initialLayout;
		};

		m_colorImage			= createImage(vk, vkDevice, &colorImageParams);

		// Allocate and bind color image memory
		m_colorImageAlloc		= memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(), m_colorImageAlloc->getOffset()));

		// create image view for subsampled image
		const VkImageViewCreateInfo colorImageViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkImageViewCreateFlags	flags;
			*m_colorImage,										// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,								// VkImageViewType			viewType;
			VK_FORMAT_R8G8B8A8_UNORM,							// VkFormat					format;
			componentMappingRGBA,								// VkChannelMapping			channels;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }		// VkImageSubresourceRange	subresourceRange;
		};

		m_colorImageView = createImageView(vk, vkDevice, &colorImageViewParams);
	}

	// Create output image ( data from subsampled color image will be copied into it using sampler with VK_SAMPLER_CREATE_SUBSAMPLED_BIT_EXT )
	{
		const VkImageCreateInfo	outputImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,									// VkStructureType			sType;
			DE_NULL,																// const void*				pNext;
			0u,																		// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,														// VkImageType				imageType;
			VK_FORMAT_R8G8B8A8_UNORM,												// VkFormat					format;
			{ m_renderSize.x(), m_renderSize.y(), 1u },								// VkExtent3D				extent;
			1u,																		// deUint32					mipLevels;
			1u,																		// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,													// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,												// VkImageTiling			tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,	// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,												// VkSharingMode			sharingMode;
			1u,																		// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,														// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED												// VkImageLayout			initialLayout;
		};

		m_outputImage = createImage(vk, vkDevice, &outputImageParams);

		// Allocate and bind input image memory
		m_outputImageAlloc = memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_outputImage), MemoryRequirement::Any);
		VK_CHECK(vk.bindImageMemory(vkDevice, *m_outputImage, m_outputImageAlloc->getMemory(), m_outputImageAlloc->getOffset()));

		// create image view for output image
		const VkImageViewCreateInfo outputImageViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,		// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			0u,												// VkImageViewCreateFlags	flags;
			*m_outputImage,									// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,							// VkImageViewType			viewType;
			VK_FORMAT_R8G8B8A8_UNORM,						// VkFormat					format;
			componentMappingRGBA,							// VkChannelMapping			channels;
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }	// VkImageSubresourceRange	subresourceRange;
		};

		m_outputImageView = createImageView(vk, vkDevice, &outputImageViewParams);
	}

	// create a sampler that is able to read from subsampled image
	{
#if !DRY_RUN_WITHOUT_FDM_EXTENSION
		deUint32 samplerCreateFlags = m_testParams.nonSubsampledImages ? 0u : (deUint32)VK_SAMPLER_CREATE_SUBSAMPLED_BIT_EXT;
#else
		deUint32 samplerCreateFlags = 0u;
#endif
		const struct VkSamplerCreateInfo		samplerInfo
		{
			VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,			// sType
			DE_NULL,										// pNext
			(VkSamplerCreateFlags)samplerCreateFlags,		// flags
			VK_FILTER_NEAREST,								// magFilter
			VK_FILTER_NEAREST,								// minFilter
			VK_SAMPLER_MIPMAP_MODE_NEAREST,					// mipmapMode
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			// addressModeU
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			// addressModeV
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,			// addressModeW
			0.0f,											// mipLodBias
			VK_FALSE,										// anisotropyEnable
			1.0f,											// maxAnisotropy
			DE_FALSE,										// compareEnable
			VK_COMPARE_OP_ALWAYS,							// compareOp
			0.0f,											// minLod
			0.0f,											// maxLod
			VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,		// borderColor
			VK_FALSE,										// unnormalizedCoords
		};
		m_colorSampler = createSampler(vk, vkDevice, &samplerInfo);
	}

	// Create render passes
#if !DRY_RUN_WITHOUT_FDM_EXTENSION
	if ( testParams.dynamicDensityMap )
#endif
		m_renderPassProduceDynamicDensityMap	= createRenderPassProduceDynamicDensityMap<AttachmentDescription2, AttachmentReference2, SubpassDescription2, SubpassDependency2, RenderPassCreateInfo2>(vk, vkDevice, testParams);
	m_renderPassProduceSubsampledImage		= createRenderPassProduceSubsampledImage<AttachmentDescription2, AttachmentReference2, SubpassDescription2, SubpassDependency2, RenderPassCreateInfo2>(vk, vkDevice, testParams);
	m_renderPassOutputSubsampledImage		= createRenderPassOutputSubsampledImage<AttachmentDescription2, AttachmentReference2, SubpassDescription2, SubpassDependency2, RenderPassCreateInfo2>(vk, vkDevice, testParams);

	// Create framebuffers
#if  !DRY_RUN_WITHOUT_FDM_EXTENSION
	if ( testParams.dynamicDensityMap )
#endif
		m_framebufferProduceDynamicDensityMap = createFrameBuffer(vk, vkDevice, *m_renderPassProduceDynamicDensityMap, m_densityMapSize, { *m_densityMapImageView });
#if !DRY_RUN_WITHOUT_FDM_EXTENSION
	m_framebufferProduceSubsampledImage = createFrameBuffer(vk, vkDevice, *m_renderPassProduceSubsampledImage, m_renderSize, { *m_colorImageView, *m_densityMapImageView });
#else
	m_framebufferProduceSubsampledImage = createFrameBuffer(vk, vkDevice, *m_renderPassProduceSubsampledImage, m_renderSize, { *m_colorImageView });
#endif
	m_framebufferOutputSubsampledImage	= createFrameBuffer( vk, vkDevice, *m_renderPassOutputSubsampledImage, m_renderSize, { *m_outputImageView } );

	// Create pipeline layout for first two render passes that do not use any descriptors
	{
		const VkPipelineLayoutCreateInfo		pipelineLayoutParams		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			0u,												// VkPipelineLayoutCreateFlags	flags;
			0u,												// deUint32						setLayoutCount;
			DE_NULL,										// const VkDescriptorSetLayout*	pSetLayouts;
			0u,												// deUint32						pushConstantRangeCount;
			DE_NULL											// const VkPushConstantRange*	pPushConstantRanges;
		};

		m_pipelineLayoutProduceSubsampledImage = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Create pipeline layout for last render pass ( output subsampled image )
	{
		std::vector<VkDescriptorSetLayoutBinding>	descriptorSetLayoutBindings =
		{
			{
				0,											// deUint32				binding;
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	// VkDescriptorType		descriptorType;
				1,											// deUint32				descriptorCount;
				VK_SHADER_STAGE_FRAGMENT_BIT,				// VkShaderStageFlags	stageFlags;
				&(m_colorSampler.get())						// const VkSampler*		pImmutableSamplers;
			},
		};

		const VkDescriptorSetLayoutCreateInfo		descriptorSetLayoutParams	=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,		// VkStructureType						sType
			DE_NULL,													// const void*							pNext
			0u,															// VkDescriptorSetLayoutCreateFlags		flags
			static_cast<deUint32>(descriptorSetLayoutBindings.size()),	// deUint32								bindingCount
			descriptorSetLayoutBindings.data()							// const VkDescriptorSetLayoutBinding*	pBindings
		};
		m_descriptorSetLayoutOutputSubsampledImage = createDescriptorSetLayout(vk, vkDevice, &descriptorSetLayoutParams);

		const VkPipelineLayoutCreateInfo		pipelineLayoutParams		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			0u,													// VkPipelineLayoutCreateFlags	flags;
			1u,													// deUint32						setLayoutCount;
			&m_descriptorSetLayoutOutputSubsampledImage.get(),	// const VkDescriptorSetLayout*	pSetLayouts;
			0u,													// deUint32						pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*	pPushConstantRanges;
		};
		m_pipelineLayoutOutputSubsampledImage = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	// Update descriptor set
	{
		{
			std::vector<VkDescriptorPoolSize> poolSizes =
			{
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u }
			};

			const VkDescriptorPoolCreateInfo	descriptorPoolCreateInfo =
			{
				VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,		// VkStructureType				sType
				DE_NULL,											// const void*					pNext
				VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,	// VkDescriptorPoolCreateFlags	flags
				1u,													// deUint32						maxSets
				static_cast<deUint32>(poolSizes.size()),			// deUint32						poolSizeCount
				poolSizes.data()									// const VkDescriptorPoolSize*	pPoolSizes
			};
			m_descriptorPoolOutputSubsampledImage = createDescriptorPool(vk, vkDevice, &descriptorPoolCreateInfo);
		}

		{
			const VkDescriptorSetAllocateInfo	descriptorSetAllocateInfo =
			{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,		// VkStructureType				sType
				DE_NULL,											// const void*					pNext
				*m_descriptorPoolOutputSubsampledImage,				// VkDescriptorPool				descriptorPool
				1u,													// deUint32						descriptorSetCount
				&m_descriptorSetLayoutOutputSubsampledImage.get(),	// const VkDescriptorSetLayout*	pSetLayouts
			};
			m_descriptorSetOutputSubsampledImage = allocateDescriptorSet(vk, vkDevice, &descriptorSetAllocateInfo);

			const VkDescriptorImageInfo			inputImageInfo =
			{
				DE_NULL,											// VkSampleri		sampler;
				*m_colorImageView,									// VkImageView		imageView;
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL			// VkImageLayout	imageLayout;
			};

			std::vector<VkWriteDescriptorSet>	descriptorWrite =
			{
				{
					VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,			// VkStructureType					sType;
					DE_NULL,										// const void*						pNext;
					*m_descriptorSetOutputSubsampledImage,			// VkDescriptorSet					dstSet;
					0u,												// deUint32							dstBinding;
					0u,												// deUint32							dstArrayElement;
					1u,												// deUint32							descriptorCount;
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,		// VkDescriptorType					descriptorType;
					&inputImageInfo,								// const VkDescriptorImageInfo*		pImageInfo;
					DE_NULL,										// const VkDescriptorBufferInfo*	pBufferInfo;
					DE_NULL											// const VkBufferView*				pTexelBufferView;
				}
			};
			vk.updateDescriptorSets(vkDevice, static_cast<deUint32>(descriptorWrite.size()), descriptorWrite.data(), 0u, DE_NULL);
		}
	}

	m_vertexCommonShaderModule						= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("densitymap_vert"), 0);
	m_fragmentShaderModuleProduceSubsampledImage	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("densitymap_frag_produce"), 0);
	m_fragmentShaderModuleOutputSubsampledImage		= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("densitymap_frag_output"), 0);

	// Create pipelines
	{
		const VkVertexInputBindingDescription		vertexInputBindingDescription		=
		{
			0u,																// deUint32					binding;
			sizeof(Vertex4RGBA),											// deUint32					strideInBytes;
			VK_VERTEX_INPUT_RATE_VERTEX										// VkVertexInputStepRate	inputRate;
		};

		std::vector<VkVertexInputAttributeDescription>	vertexInputAttributeDescriptions	=
		{
			{ 0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u },
			{ 1u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, (deUint32)(sizeof(float) * 4) }
		};

		const VkPipelineVertexInputStateCreateInfo	vertexInputStateParams				=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineVertexInputStateCreateFlags	flags;
			1u,																// deUint32									vertexBindingDescriptionCount;
			&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			static_cast<deUint32>(vertexInputAttributeDescriptions.size()),	// deUint32									vertexAttributeDescriptionCount;
			vertexInputAttributeDescriptions.data()							// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const std::vector<VkViewport>				viewportsDDM						{ makeViewport(m_densityMapSize) };
		const std::vector<VkRect2D>					scissorsDDM							{ makeRect2D(m_densityMapSize) };
		const std::vector<VkViewport>				viewports							{ makeViewport(m_renderSize) };
		const std::vector<VkRect2D>					scissors							{ makeRect2D(m_renderSize) };

#if !DRY_RUN_WITHOUT_FDM_EXTENSION
		if (testParams.dynamicDensityMap)
#endif
			m_graphicsPipelineProduceDynamicDensityMap = makeGraphicsPipeline(vk,							// const DeviceInterface&						vk
															vkDevice,										// const VkDevice								device
															*m_pipelineLayoutProduceSubsampledImage,		// const VkPipelineLayout						pipelineLayout
															*m_vertexCommonShaderModule,					// const VkShaderModule							vertexShaderModule
															DE_NULL,										// const VkShaderModule							tessellationControlModule
															DE_NULL,										// const VkShaderModule							tessellationEvalModule
															DE_NULL,										// const VkShaderModule							geometryShaderModule
															*m_fragmentShaderModuleProduceSubsampledImage,	// const VkShaderModule							fragmentShaderModule
															*m_renderPassProduceDynamicDensityMap,			// const VkRenderPass							renderPass
															viewportsDDM,									// const std::vector<VkViewport>&				viewports
															scissorsDDM,									// const std::vector<VkRect2D>&					scissors
															VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,			// const VkPrimitiveTopology					topology
															0u,												// const deUint32								subpass
															0u,												// const deUint32								patchControlPoints
															&vertexInputStateParams);						// const VkPipelineVertexInputStateCreateInfo*	vertexInputStateCreateInfo

		m_graphicsPipelineProduceSubsampledImage = makeGraphicsPipeline(vk,									// const DeviceInterface&						vk
															vkDevice,										// const VkDevice								device
															*m_pipelineLayoutProduceSubsampledImage,		// const VkPipelineLayout						pipelineLayout
															*m_vertexCommonShaderModule,					// const VkShaderModule							vertexShaderModule
															DE_NULL,										// const VkShaderModule							tessellationControlModule
															DE_NULL,										// const VkShaderModule							tessellationEvalModule
															DE_NULL,										// const VkShaderModule							geometryShaderModule
															*m_fragmentShaderModuleProduceSubsampledImage,	// const VkShaderModule							fragmentShaderModule
															*m_renderPassProduceSubsampledImage,			// const VkRenderPass							renderPass
															viewports,										// const std::vector<VkViewport>&				viewports
															scissors,										// const std::vector<VkRect2D>&					scissors
															VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,			// const VkPrimitiveTopology					topology
															0u,												// const deUint32								subpass
															0u,												// const deUint32								patchControlPoints
															&vertexInputStateParams);						// const VkPipelineVertexInputStateCreateInfo*	vertexInputStateCreateInfo

		m_graphicsPipelineOutputSubsampledImage = makeGraphicsPipeline(vk,									// const DeviceInterface&						vk
															vkDevice,										// const VkDevice								device
															*m_pipelineLayoutOutputSubsampledImage,			// const VkPipelineLayout						pipelineLayout
															*m_vertexCommonShaderModule,					// const VkShaderModule							vertexShaderModule
															DE_NULL,										// const VkShaderModule							tessellationControlModule
															DE_NULL,										// const VkShaderModule							tessellationEvalModule
															DE_NULL,										// const VkShaderModule							geometryShaderModule
															*m_fragmentShaderModuleOutputSubsampledImage,	// const VkShaderModule							fragmentShaderModule
															*m_renderPassOutputSubsampledImage,				// const VkRenderPass							renderPass
															viewports,										// const std::vector<VkViewport>&				viewports
															scissors,										// const std::vector<VkRect2D>&					scissors
															VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,			// const VkPrimitiveTopology					topology
															0u,												// const deUint32								subpass
															0u,												// const deUint32								patchControlPoints
															&vertexInputStateParams);						// const VkPipelineVertexInputStateCreateInfo*	vertexInputStateCreateInfo
	}

	// Create vertex buffers
#if !DRY_RUN_WITHOUT_FDM_EXTENSION
	if (testParams.dynamicDensityMap)
#endif
		createVertexBuffer(vk, vkDevice, queueFamilyIndex, memAlloc, m_verticesDDM, m_vertexBufferDDM, m_vertexBufferAllocDDM);
	createVertexBuffer(vk, vkDevice, queueFamilyIndex, memAlloc, m_vertices, m_vertexBuffer, m_vertexBufferAlloc);

	// Create command pool and command buffer
	m_cmdPool	= createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	const typename				RenderpassSubpass2::SubpassBeginInfo	subpassBeginInfo(DE_NULL, VK_SUBPASS_CONTENTS_INLINE);
	const typename				RenderpassSubpass2::SubpassEndInfo		subpassEndInfo(DE_NULL);
	const						VkDeviceSize							vertexBufferOffset = 0;
	std::vector<VkClearValue>	attachmentClearValuesDDM				= { makeClearValueColorF32(1.0f, 1.0f, 1.0f, 1.0f) };
	std::vector<VkClearValue>	attachmentClearValues					= { makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f) };

	beginCommandBuffer(vk, *m_cmdBuffer, 0u);

	// first render pass - render dynamic density map
#if !DRY_RUN_WITHOUT_FDM_EXTENSION
	if ( testParams.dynamicDensityMap )
#endif
	{
		const VkRenderPassBeginInfo renderPassBeginInfoProduceDynamicDensityMap =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,				// VkStructureType		sType;
			DE_NULL,												// const void*			pNext;
			*m_renderPassProduceDynamicDensityMap,					// VkRenderPass			renderPass;
			*m_framebufferProduceDynamicDensityMap,					// VkFramebuffer		framebuffer;
			makeRect2D(m_densityMapSize),							// VkRect2D				renderArea;
			static_cast<deUint32>(attachmentClearValuesDDM.size()),	// uint32_t				clearValueCount;
			attachmentClearValuesDDM.data()							// const VkClearValue*	pClearValues;
		};
		RenderpassSubpass2::cmdBeginRenderPass(vk, *m_cmdBuffer, &renderPassBeginInfoProduceDynamicDensityMap, &subpassBeginInfo);
		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineProduceDynamicDensityMap);
		vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBufferDDM.get(), &vertexBufferOffset);
		vk.cmdDraw(*m_cmdBuffer, (deUint32)m_verticesDDM.size(), 1, 0, 0);
		RenderpassSubpass2::cmdEndRenderPass(vk, *m_cmdBuffer, &subpassEndInfo);
	}

	// render subsampled image
	const VkRenderPassBeginInfo renderPassBeginInfoProduceSubsampledImage =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,				// VkStructureType		sType;
		DE_NULL,												// const void*			pNext;
		*m_renderPassProduceSubsampledImage,					// VkRenderPass			renderPass;
		*m_framebufferProduceSubsampledImage,					// VkFramebuffer		framebuffer;
		makeRect2D(m_renderSize),								// VkRect2D				renderArea;
		static_cast<deUint32>(attachmentClearValues.size()),	// uint32_t				clearValueCount;
		attachmentClearValues.data()							// const VkClearValue*	pClearValues;
	};
	RenderpassSubpass2::cmdBeginRenderPass(vk, *m_cmdBuffer, &renderPassBeginInfoProduceSubsampledImage, &subpassBeginInfo);
	vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineProduceSubsampledImage);
	vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
	vk.cmdDraw(*m_cmdBuffer, (deUint32)m_vertices.size(), 1, 0, 0);
	RenderpassSubpass2::cmdEndRenderPass(vk, *m_cmdBuffer, &subpassEndInfo);

	// copy subsampled image to ordinary image using sampler that is able to read from subsampled images( subsampled image cannot be copied using vkCmdCopyImageToBuffer )
	const VkRenderPassBeginInfo renderPassBeginInfoOutputSubsampledImage =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,				// VkStructureType		sType;
		DE_NULL,												// const void*			pNext;
		*m_renderPassOutputSubsampledImage,						// VkRenderPass			renderPass;
		*m_framebufferOutputSubsampledImage,					// VkFramebuffer		framebuffer;
		makeRect2D(m_renderSize),								// VkRect2D				renderArea;
		static_cast<deUint32>(attachmentClearValues.size()),	// uint32_t				clearValueCount;
		attachmentClearValues.data()							// const VkClearValue*	pClearValues;
	};
	RenderpassSubpass2::cmdBeginRenderPass(vk, *m_cmdBuffer, &renderPassBeginInfoOutputSubsampledImage, &subpassBeginInfo);
	vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipelineOutputSubsampledImage);
	vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayoutOutputSubsampledImage, 0, 1, &m_descriptorSetOutputSubsampledImage.get(), 0, DE_NULL);
	vk.cmdDraw(*m_cmdBuffer, (deUint32)m_vertices.size(), 1, 0, 0);
	RenderpassSubpass2::cmdEndRenderPass(vk, *m_cmdBuffer, &subpassEndInfo);

	endCommandBuffer(vk, *m_cmdBuffer);
}

tcu::TestStatus FragmentDensityMapTestInstance::iterate (void)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());

	return verifyImage();
}

struct Vec4Sorter
{
	bool operator()(const tcu::Vec4& lhs, const tcu::Vec4& rhs) const
	{
		if (lhs.x() != rhs.x())
			return lhs.x() < rhs.x();
		if (lhs.y() != rhs.y())
			return lhs.y() < rhs.y();
		if (lhs.z() != rhs.z())
			return lhs.z() < rhs.z();
		return lhs.w() < rhs.w();
	}
};

tcu::TestStatus FragmentDensityMapTestInstance::verifyImage (void)
{
	const DeviceInterface&				vk						= m_context.getDeviceInterface();
	const VkDevice						vkDevice				= m_context.getDevice();
	const VkQueue						queue					= m_context.getUniversalQueue();
	const deUint32						queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	SimpleAllocator						memAlloc				(vk, vkDevice, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice()));
	de::UniquePtr<tcu::TextureLevel>	outputImage				(pipeline::readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, memAlloc, *m_outputImage, VK_FORMAT_R8G8B8A8_UNORM, m_renderSize).release());
	const tcu::ConstPixelBufferAccess&	outputAccess			= outputImage->getAccess();
	tcu::TestLog&						log						= m_context.getTestContext().getLog();

	// log images
	log << tcu::TestLog::ImageSet("Result", "Result images")
		<< tcu::TestLog::Image("Rendered", "Rendered output image", outputAccess)
		<< tcu::TestLog::EndImageSet;

#if !DRY_RUN_WITHOUT_FDM_EXTENSION
	deUint32 estimatedColorCount = m_testParams.fragmentArea.x() * m_testParams.fragmentArea.y();
#else
	deUint32 estimatedColorCount = 1u;
#endif
	tcu::Vec2 density{
		1.0f / static_cast<float>(m_testParams.fragmentArea.x()),
		1.0f / static_cast<float>(m_testParams.fragmentArea.y())
	};
	float densityMult = density.x() * density.y();

	// create histogram of all image colors, check the value of inverted FragSizeEXT
	std::map<tcu::Vec4, deUint32, Vec4Sorter> colorCount;
	for (int y = 0; y < outputAccess.getHeight(); y++)
	{
		for (int x = 0; x < outputAccess.getWidth(); x++)
		{
			tcu::Vec4 outputColor	= outputAccess.getPixel(x, y);
			float densityClamped	= outputColor.z() * outputColor.w();
			if ((densityClamped + 0.01) < densityMult)
				return tcu::TestStatus::fail("Wrong value of FragSizeEXT variable");
			auto it = colorCount.find(outputColor);
			if (it == end(colorCount))
				it = colorCount.insert({ outputColor, 0u }).first;
			it->second++;
		}
	}

	// check if color count is the same as estimated one
	for (const auto& color : colorCount)
	{
		if (color.second > estimatedColorCount)
			return tcu::TestStatus::fail("Wrong color count");
	}

	return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup* createFragmentDensityMapTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>		fdmTests		(new tcu::TestCaseGroup(testCtx, "fragment_density_map", "VK_EXT_fragment_density_map extension tests"));

	std::vector<tcu::UVec2> fragmentArea
	{
		{ 1, 2 },
		{ 2, 1 },
		{ 2, 2 }
	};

	for (const auto& area : fragmentArea)
	{
		std::stringstream str;
		str << "_" << area.x() << "_" << area.y();
		fdmTests->addChild(new FragmentDensityMapTest(testCtx, std::string("static_subsampled")		+ str.str(), "", TestParams(false, false, area)));
		fdmTests->addChild(new FragmentDensityMapTest(testCtx, std::string("dynamic_subsampled")	+ str.str(), "", TestParams(true, false, area)));
		fdmTests->addChild(new FragmentDensityMapTest(testCtx, std::string("static_nonsubsampled")	+ str.str(), "", TestParams(false, true, area)));
		fdmTests->addChild(new FragmentDensityMapTest(testCtx, std::string("dynamic_nonsubsampled")	+ str.str(), "", TestParams(true, true, area)));
	}

	return fdmTests.release();
}

} // renderpass

} // vkt
