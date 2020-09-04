/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 Google Inc.
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
 * \brief Shader cross-stage interface tests
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmCrossStageInterfaceTests.hpp"

#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuImageCompare.hpp"

#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "deSharedPtr.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "deUniquePtr.hpp"

#include "vktSpvAsmGraphicsShaderTestUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include <map>
#include <vector>

namespace vkt
{
namespace SpirVAssembly
{
using namespace vk;

namespace
{
using std::string;
using std::map;
using std::vector;

typedef de::SharedPtr<Unique<VkShaderModule> >	ShaderModuleSP;
using de::MovePtr;

using tcu::Vec4;

enum TestType
{
	TEST_TYPE_FLAT = 0,
	TEST_TYPE_NOPERSPECTIVE,
	TEST_TYPE_RELAXEDPRECISION,
	TEST_TYPE_LAST
};

struct TestParameters
{
	TestParameters (TestType q, size_t s)
		:testOptions	(s)
		,qualifier		(q)
	{}
	vector<int>		testOptions;
	TestType		qualifier;
};

VkImageCreateInfo makeImageCreateInfo (const VkImageType imageType, const VkExtent3D& extent, const VkFormat format, const VkImageUsageFlags usage, deUint32 queueFamilyIndex)
{
	const VkImageCreateInfo imageInfo	=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		(VkImageCreateFlags)0,						// VkImageCreateFlags		flags;
		imageType,									// VkImageType				imageType;
		format,										// VkFormat					format;
		{extent.width, extent.height, 1u},			// VkExtent3D				extent;
		1u,											// uint32_t					mipLevels;
		extent.depth,								// uint32_t					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,					// VkImageTiling			tiling;
		usage,										// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode			sharingMode;
		1u,											// uint32_t					queueFamilyIndexCount;
		&queueFamilyIndex,							// const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			initialLayout;
	};
	return imageInfo;
}

void imageBarrier (const DeviceInterface&			vk,
				   const VkCommandBuffer			cmdBuffer,
				   const VkImage					image,
				   const VkImageSubresourceRange	subresourceRange,
				   const VkImageLayout				oldLayout,
				   const VkImageLayout				newLayout,
				   const VkAccessFlags				srcAccessMask,
				   const VkAccessFlags				dstAccessMask,
				   const VkPipelineStageFlags		srcStageMask,
				   const VkPipelineStageFlags		dstStageMask)
{
	const VkImageMemoryBarrier		barrier				=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		srcAccessMask,							// VkAccessFlags			srcAccessMask;
		dstAccessMask,							// VkAccessFlags			dstAccessMask;
		oldLayout,								// VkImageLayout			oldLayout;
		newLayout,								// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,				// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,				// deUint32					dstQueueFamilyIndex;
		image,									// VkImage					image;
		subresourceRange,						// VkImageSubresourceRange	subresourceRange;
	};

	vk.cmdPipelineBarrier(cmdBuffer, srcStageMask, dstStageMask, (VkDependencyFlags)0, 0u, (const VkMemoryBarrier*)DE_NULL,
		0u, (const VkBufferMemoryBarrier*)DE_NULL,
		1u, &barrier);
}

class CrossStageTestInstance : public TestInstance
{
public:
	CrossStageTestInstance	(Context& context, const TestParameters& parameters)
	: TestInstance		(context)
	, m_parameters		(parameters)
	, m_verticesCount	(4u)
	, m_data			(2u * m_verticesCount)
	, m_colorFormat		(VK_FORMAT_R8G8B8A8_UNORM)
	, m_colorRed		(1.0f, 0.0f, 0.0f, 1.0f)
	, m_colorGreen		(0.0f, 1.0f, 0.0f, 1.0f)
	{
		createVertexData();
		m_extent.width	= 51u;
		m_extent.height	= 51u;
		m_extent.depth	= 1u;
	}
enum
{
	DECORATION_IN_VERTEX = 0,
	DECORATION_IN_FRAGMENT,
	DECORATION_IN_ALL_SHADERS,
	DECORATION_LAST
};
protected:
	tcu::TestStatus			iterate					(void);
private:
	void					createVertexData		(void);
	void					makeShaderModule		(map<VkShaderStageFlagBits, ShaderModuleSP>&	shaderModule,
													 const VkShaderStageFlagBits					stageFlag,
													 const int										optionNdx);

	Move<VkPipeline>		makeGraphicsPipeline	(const VkRenderPass								renderPass,
													 const VkPipelineLayout							pipelineLayout,
													 const VkShaderStageFlagBits					stageFlags,
													 map<VkShaderStageFlagBits, ShaderModuleSP>&	shaderModules,
													 const VkPrimitiveTopology						primitiveTopology);

	bool					checkImage				(VkImage										image,
													 VkCommandBuffer								cmdBuffer,
													 const string&									description,
													 const tcu::Texture2DArray&						referenceFrame);
	void					interpolationFill		(tcu::Texture2DArray&							referenceFrame);
	void					perspectiveFill			(tcu::Texture2DArray&							referenceFrame);
	void					redFill					(tcu::Texture2DArray&							referenceFrame);

	const TestParameters	m_parameters;
	const deUint32			m_verticesCount;
	vector<Vec4>			m_data;
	VkExtent3D				m_extent;
	const VkFormat			m_colorFormat;
	const Vec4				m_colorRed;
	const Vec4				m_colorGreen;
};

tcu::TestStatus CrossStageTestInstance::iterate (void)
{
	const DeviceInterface&					vk						= m_context.getDeviceInterface();
	const VkDevice							vkDevice				= m_context.getDevice();
	const VkPhysicalDeviceFeatures&			features				= m_context.getDeviceFeatures();
	const bool								supportsGeometry		= features.geometryShader == VK_TRUE;
	const bool								supportsTessellation	= features.tessellationShader == VK_TRUE;
	const VkDeviceSize						vertexDataSize			= static_cast<VkDeviceSize>(deAlignSize(static_cast<size_t>( m_data.size() * sizeof(Vec4)),
																		static_cast<size_t>(m_context.getDeviceProperties().limits.nonCoherentAtomSize)));
	const VkBufferCreateInfo				bufferInfo				= makeBufferCreateInfo(vertexDataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	Move<VkBuffer>							vertexBuffer			= createBuffer(vk, vkDevice, &bufferInfo);
	MovePtr<Allocation>						allocationVertex		= m_context.getDefaultAllocator().allocate(getBufferMemoryRequirements(vk, vkDevice, *vertexBuffer),  MemoryRequirement::HostVisible);

	const VkImageSubresourceRange			imageSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const VkImageCreateInfo					colorAttachmentInfo		= makeImageCreateInfo(VK_IMAGE_TYPE_2D, m_extent, m_colorFormat,
																		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, m_context.getUniversalQueueFamilyIndex());

	Move<VkImage>							colorAttachmentImage	= createImage(vk, vkDevice, &colorAttachmentInfo);
	MovePtr<Allocation>						allocationAttachment	= m_context.getDefaultAllocator().allocate(getImageMemoryRequirements(vk, vkDevice, *colorAttachmentImage), MemoryRequirement::Any);
	VK_CHECK(vk.bindImageMemory(vkDevice, *colorAttachmentImage, allocationAttachment->getMemory(), allocationAttachment->getOffset()));
	Move<VkImageView>						colorAttachmentView		= makeImageView(vk, vkDevice, *colorAttachmentImage, VK_IMAGE_VIEW_TYPE_2D, m_colorFormat, imageSubresourceRange);

	MovePtr<tcu::Texture2DArray>			referenceImage1			= MovePtr<tcu::Texture2DArray>(new tcu::Texture2DArray(mapVkFormat(m_colorFormat), m_extent.width, m_extent.height, m_extent.depth));
	MovePtr<tcu::Texture2DArray>			referenceImage2			= MovePtr<tcu::Texture2DArray>(new tcu::Texture2DArray(mapVkFormat(m_colorFormat), m_extent.width, m_extent.height, m_extent.depth));

	// Init host buffer data
	VK_CHECK(vk.bindBufferMemory(vkDevice, *vertexBuffer, allocationVertex->getMemory(), allocationVertex->getOffset()));
	deMemcpy(allocationVertex->getHostPtr(), m_data.data(), static_cast<size_t>(vertexDataSize));
	flushAlloc(vk, vkDevice, *allocationVertex);

	Move<VkRenderPass>						renderPass				= makeRenderPass (vk, vkDevice, m_colorFormat);
	Move<VkFramebuffer>						frameBuffer				= makeFramebuffer (vk, vkDevice, *renderPass, *colorAttachmentView, m_extent.width, m_extent.height);
	Move<VkPipelineLayout>					pipelineLayout			= makePipelineLayout (vk, vkDevice);
	Move<VkCommandPool>						cmdPool;
	Move<VkCommandBuffer>					cmdBuffer;

	// cmdPool
	{
		const VkCommandPoolCreateInfo cmdPoolParams =
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,			// VkStructureType		sType;
			DE_NULL,											// const void*			pNext;
			VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,	// VkCmdPoolCreateFlags	flags;
			m_context.getUniversalQueueFamilyIndex(),			// deUint32				queueFamilyIndex;
		};
		cmdPool = createCommandPool(vk, vkDevice, &cmdPoolParams);
	}

	// cmdBuffer
	{
		const VkCommandBufferAllocateInfo cmdBufferAllocateInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,		// VkStructureType		sType;
			DE_NULL,											// const void*			pNext;
			*cmdPool,											// VkCommandPool		commandPool;
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,					// VkCommandBufferLevel	level;
			1u,													// deUint32				bufferCount;
		};
		cmdBuffer	= allocateCommandBuffer(vk, vkDevice, &cmdBufferAllocateInfo);
	}

	if (!supportsTessellation)
		m_context.getTestContext().getLog() << tcu::TestLog::Message << "Tessellation not supported" << tcu::TestLog::EndMessage;

	if (!supportsGeometry)
		m_context.getTestContext().getLog() << tcu::TestLog::Message << "Geometry not supported" << tcu::TestLog::EndMessage;

	vector<deUint32> shadersStagesFlagsBits;
	shadersStagesFlagsBits.push_back(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	if (supportsTessellation)
		shadersStagesFlagsBits.push_back(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	if (supportsGeometry)
		shadersStagesFlagsBits.push_back(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	if (supportsTessellation && supportsGeometry)
		shadersStagesFlagsBits.push_back(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	referenceImage1->allocLevel(0);
	referenceImage2->allocLevel(0);
	switch(m_parameters.qualifier)
	{
	case TEST_TYPE_FLAT:
		interpolationFill(*referenceImage1);
		redFill(*referenceImage2);
		break;
	case TEST_TYPE_NOPERSPECTIVE:
		perspectiveFill(*referenceImage1);
		interpolationFill(*referenceImage2);
		break;
	case TEST_TYPE_RELAXEDPRECISION:
		interpolationFill(*referenceImage1);
		interpolationFill(*referenceImage2);
		break;
	default:
		DE_ASSERT(0);
	}

	for (deUint32 optionNdx = 0; optionNdx < m_parameters.testOptions.size(); optionNdx++)
	for (size_t stagesNdx = 0ull; stagesNdx < shadersStagesFlagsBits.size(); stagesNdx++)
	{
		map<VkShaderStageFlagBits, ShaderModuleSP>	shaderModule;
		string										imageDescription;
		const VkClearValue							renderPassClearValue = makeClearValueColor(tcu::Vec4(0.0f));
		makeShaderModule(shaderModule, (VkShaderStageFlagBits)shadersStagesFlagsBits[stagesNdx], optionNdx);

		Move<VkPipeline>			graphicsPipeline		= makeGraphicsPipeline (*renderPass, *pipelineLayout, (VkShaderStageFlagBits)shadersStagesFlagsBits[stagesNdx], shaderModule, ((VkShaderStageFlagBits)shadersStagesFlagsBits[stagesNdx] & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
		const VkDeviceSize			vertexBufferOffset		= 0u;

		beginCommandBuffer(vk, *cmdBuffer);

		imageBarrier(vk, *cmdBuffer, *colorAttachmentImage, imageSubresourceRange,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0u, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		vk.cmdClearColorImage(*cmdBuffer, *colorAttachmentImage,  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &renderPassClearValue.color, 1, &imageSubresourceRange);

		imageBarrier(vk, *cmdBuffer, *colorAttachmentImage, imageSubresourceRange,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		beginRenderPass(vk, *cmdBuffer, *renderPass, *frameBuffer, makeRect2D(0, 0, m_extent.width, m_extent.height), tcu::Vec4(0.0f));

		vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &(*vertexBuffer), &vertexBufferOffset);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipeline);

		vk.cmdDraw(*cmdBuffer, m_verticesCount, 1u, 0u, 0u);

		endRenderPass(vk, *cmdBuffer);

		endCommandBuffer(vk, *cmdBuffer);

		submitCommandsAndWait(vk, vkDevice, m_context.getUniversalQueue(), *cmdBuffer);

		{
			const string geometry		= (VK_SHADER_STAGE_GEOMETRY_BIT & (VkShaderStageFlagBits)shadersStagesFlagsBits[stagesNdx]) ? "Geometry->" : "";
			const string tessellation	= ( VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT & (VkShaderStageFlagBits)shadersStagesFlagsBits[stagesNdx]) ? "Tessellation->" : "";
			imageDescription = "Pipeline: Vertex->" + tessellation +  geometry + "Fragment | ";
		}

		if (DECORATION_IN_VERTEX == m_parameters.testOptions[optionNdx])
			imageDescription+= "decoration in vertex | ";
		if (DECORATION_IN_FRAGMENT == m_parameters.testOptions[optionNdx])
			imageDescription+= "decoration in fragment | ";
		if (DECORATION_IN_ALL_SHADERS == m_parameters.testOptions[optionNdx])
			imageDescription+= "decoration in all shaders | ";

		{
			bool resultComparison = false;
			if (TEST_TYPE_RELAXEDPRECISION == m_parameters.qualifier)
			{
				resultComparison = checkImage(*colorAttachmentImage, *cmdBuffer, imageDescription+" Expected Pass", *referenceImage1);
			}
			else
			{
				if (DECORATION_IN_VERTEX == m_parameters.testOptions[optionNdx])
					resultComparison = checkImage(*colorAttachmentImage, *cmdBuffer, imageDescription+" Expected Pass", *referenceImage1);
				else if ((VkShaderStageFlagBits)(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT) == (VkShaderStageFlagBits)shadersStagesFlagsBits[stagesNdx])
					resultComparison = checkImage(*colorAttachmentImage, *cmdBuffer, imageDescription+" Expected Pass", *referenceImage2);
				else
					resultComparison = !checkImage(*colorAttachmentImage, *cmdBuffer, imageDescription+" Expected Fail", *referenceImage1);
			}

			if(!resultComparison)
				return tcu::TestStatus::fail("Fail");
		}
	}
	return tcu::TestStatus::pass("Pass");
}

void CrossStageTestInstance::createVertexData (void)
{
	int ndx = -1;
	if (TEST_TYPE_NOPERSPECTIVE == m_parameters.qualifier)
		m_data[++ndx] = Vec4(-2.0f,-2.0f, 1.0f, 2.0f);	//position
	else
		m_data[++ndx] = Vec4(-1.0f,-1.0f, 1.0f, 1.0f);	//position
	m_data[++ndx] = m_colorRed;

	if (TEST_TYPE_NOPERSPECTIVE == m_parameters.qualifier)
		m_data[++ndx] = Vec4(-2.0f, 2.0f, 1.0f, 2.0f);	//position
	else
		m_data[++ndx] = Vec4(-1.0f, 1.0f, 1.0f, 1.0f);	//position
	m_data[++ndx] = m_colorRed;

	m_data[++ndx] = Vec4( 1.0f,-1.0f, 1.0f, 1.0f);	//position
	m_data[++ndx] = m_colorGreen;

	m_data[++ndx] = Vec4( 1.0f, 1.0f, 1.0f, 1.0f);	//position
	m_data[++ndx] = m_colorGreen;
}

void CrossStageTestInstance::makeShaderModule (map<VkShaderStageFlagBits, ShaderModuleSP>&	shaderModule,
											   const VkShaderStageFlagBits					stageFlag,
											   const int									optionNdx)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	std::ostringstream vertex;
	vertex<<"vertex"<<optionNdx;
	std::ostringstream fragment;
	fragment<<"fragment"<<optionNdx;

	if (stageFlag & VK_SHADER_STAGE_VERTEX_BIT)
		shaderModule[VK_SHADER_STAGE_VERTEX_BIT] = (ShaderModuleSP(new Unique<VkShaderModule>(createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get(vertex.str()), 0))));

	if (stageFlag & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
		shaderModule[VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT] = (ShaderModuleSP(new Unique<VkShaderModule>(createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("tessellation_control"), 0))));

	if (stageFlag & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
		shaderModule[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT] = (ShaderModuleSP(new Unique<VkShaderModule>(createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("tessellation_evaluation"), 0))));

	if (stageFlag & VK_SHADER_STAGE_GEOMETRY_BIT)
		shaderModule[VK_SHADER_STAGE_GEOMETRY_BIT] = (ShaderModuleSP(new Unique<VkShaderModule>(createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("geometry"), 0))));

	if (stageFlag & VK_SHADER_STAGE_FRAGMENT_BIT)
		shaderModule[VK_SHADER_STAGE_FRAGMENT_BIT] = (ShaderModuleSP(new Unique<VkShaderModule>(createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get(fragment.str()), 0))));
}

Move<VkPipeline> CrossStageTestInstance::makeGraphicsPipeline (const VkRenderPass							renderPass,
															   const VkPipelineLayout						pipelineLayout,
															   const VkShaderStageFlagBits					shaderFlags,
															   map<VkShaderStageFlagBits, ShaderModuleSP>&	shaderModules,
															   const VkPrimitiveTopology					primitiveTopology)
{
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	const VkDevice			vkDevice	= m_context.getDevice();

	const VkVertexInputBindingDescription			vertexInputBindingDescription		=
	{
		0u,										// binding;
		static_cast<deUint32>(2u*sizeof(Vec4)),	// stride;
		VK_VERTEX_INPUT_RATE_VERTEX				// inputRate
	};

	const VkVertexInputAttributeDescription			vertexInputAttributeDescriptions[]	=
	{
		{
			0u,
			0u,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			0u
		},	// VertexElementData::position
		{
			1u,
			0u,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			static_cast<deUint32>(sizeof(tcu::Vec4))
		},	// VertexElementData::color
	};

	const VkPipelineVertexInputStateCreateInfo		vertexInputStateParams				=
	{																	// sType;
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// pNext;
		NULL,															// flags;
		0u,																// vertexBindingDescriptionCount;
		1u,																// pVertexBindingDescriptions;
		&vertexInputBindingDescription,									// vertexAttributeDescriptionCount;
		2u,																// pVertexAttributeDescriptions;
		vertexInputAttributeDescriptions
	};

	const std::vector<VkViewport>					viewports							(1, makeViewport(m_extent));
	const std::vector<VkRect2D>						scissors							(1, makeRect2D(m_extent));

	VkPipelineDepthStencilStateCreateInfo			depthStencilStateParams				=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType;
		DE_NULL,													// const void*								pNext;
		0u,															// VkPipelineDepthStencilStateCreateFlags	flags;
		VK_TRUE,													// VkBool32									depthTestEnable;
		VK_TRUE,													// VkBool32									depthWriteEnable;
		VK_COMPARE_OP_LESS_OR_EQUAL,								// VkCompareOp								depthCompareOp;
		VK_FALSE,													// VkBool32									depthBoundsTestEnable;
		VK_FALSE,													// VkBool32									stencilTestEnable;
		// VkStencilOpState front;
		{
			VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
			VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
			0u,						// deUint32		compareMask;
			0u,						// deUint32		writeMask;
			0u,						// deUint32		reference;
		},
		// VkStencilOpState back;
		{
			VK_STENCIL_OP_KEEP,		// VkStencilOp	failOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	passOp;
			VK_STENCIL_OP_KEEP,		// VkStencilOp	depthFailOp;
			VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
			0u,						// deUint32		compareMask;
			0u,						// deUint32		writeMask;
			0u,						// deUint32		reference;
		},
		0.0f,	// float	minDepthBounds;
		1.0f,	// float	maxDepthBounds;
	};

	const VkShaderModule	vertShader			= shaderFlags & VK_SHADER_STAGE_VERTEX_BIT ? **shaderModules[VK_SHADER_STAGE_VERTEX_BIT] : DE_NULL;
	const VkShaderModule	tessControlShader	= shaderFlags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ? **shaderModules[VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT] : DE_NULL;
	const VkShaderModule	tessEvalShader		= shaderFlags & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT ? **shaderModules[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT] : DE_NULL;
	const VkShaderModule	geomShader			= shaderFlags & VK_SHADER_STAGE_GEOMETRY_BIT ? **shaderModules[VK_SHADER_STAGE_GEOMETRY_BIT] : DE_NULL;
	const VkShaderModule	fragShader			= shaderFlags & VK_SHADER_STAGE_FRAGMENT_BIT ? **shaderModules[VK_SHADER_STAGE_FRAGMENT_BIT] : DE_NULL;

	return vk::makeGraphicsPipeline(vk,							// const DeviceInterface&                        vk
									vkDevice,					// const VkDevice                                device
									pipelineLayout,				// const VkPipelineLayout                        pipelineLayout
									vertShader,					// const VkShaderModule                          vertexShaderModule
									tessControlShader,			// const VkShaderModule                          tessellationControlShaderModule
									tessEvalShader,				// const VkShaderModule                          tessellationEvalShaderModule
									geomShader,					// const VkShaderModule                          geometryShaderModule
									fragShader,					// const VkShaderModule                          fragmentShaderModule
									renderPass,					// const VkRenderPass                            renderPass
									viewports,					// const std::vector<VkViewport>&                viewports
									scissors,					// const std::vector<VkRect2D>&                  scissors
									primitiveTopology,			// const VkPrimitiveTopology                     topology
									0u,							// const deUint32                                subpass
									4u,							// const deUint32                                patchControlPoints
									&vertexInputStateParams,	// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
									DE_NULL,					// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
									DE_NULL,					// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
									&depthStencilStateParams);	// const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
}

bool CrossStageTestInstance::checkImage (VkImage image, VkCommandBuffer cmdBuffer, const string& description, const tcu::Texture2DArray& referenceFrame)
{
	const DeviceInterface&		vk				= m_context.getDeviceInterface();
	const VkDevice				vkDevice		= m_context.getDevice();
	const int					pixelSize		= referenceFrame.getFormat().getPixelSize();
	Move<VkBuffer>				buffer;
	MovePtr<Allocation>			bufferAlloc;
	vector<deUint8>				pixelAccessData	(m_extent.width * m_extent.height * m_extent.depth * pixelSize);
	tcu::PixelBufferAccess		dst				(referenceFrame.getFormat(), m_extent.width, m_extent.height, m_extent.depth, pixelAccessData.data());
	const VkDeviceSize			pixelDataSize	= dst.getWidth() * dst.getHeight() * dst.getDepth() * pixelSize;

	// Create destination buffer
	{
		const VkBufferCreateInfo bufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			pixelDataSize,								// VkDeviceSize			size;
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			0u,											// deUint32				queueFamilyIndexCount;
			DE_NULL,									// const deUint32*		pQueueFamilyIndices;
		};

		buffer		= createBuffer(vk, vkDevice, &bufferParams);
		bufferAlloc	= m_context.getDefaultAllocator().allocate(getBufferMemoryRequirements(vk, vkDevice, *buffer), MemoryRequirement::HostVisible);
		VK_CHECK(vk.bindBufferMemory(vkDevice, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));

		deMemset(bufferAlloc->getHostPtr(), 0, static_cast<size_t>(pixelDataSize));
		flushAlloc(vk, vkDevice, *bufferAlloc);
	}

	beginCommandBuffer (vk, cmdBuffer);
	copyImageToBuffer(vk, cmdBuffer, image, *buffer, tcu::IVec2(m_extent.width, m_extent.height), 0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, m_extent.depth);
	endCommandBuffer(vk, cmdBuffer);
	submitCommandsAndWait(vk, vkDevice, m_context.getUniversalQueue(), cmdBuffer);

	// Read buffer data
	invalidateAlloc(vk, vkDevice, *bufferAlloc);
	tcu::copy(dst, tcu::ConstPixelBufferAccess(dst.getFormat(), dst.getSize(), bufferAlloc->getHostPtr()));

	if (tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Result", description.c_str(), referenceFrame.getLevel(0), dst, tcu::Vec4(0.05f), tcu::COMPARE_LOG_EVERYTHING))
		return true;
	return false;
}

void CrossStageTestInstance::interpolationFill (tcu::Texture2DArray& referenceFrame)
{
	for (deUint32 x = 0u; x < m_extent.width; ++x)
	{
		float u = static_cast<float>(x)/static_cast<float>(m_extent.width - 1);
		const Vec4 resultColor (m_colorRed.x() * (1.0f - u) + m_colorGreen.x() * u,
								m_colorRed.y() * (1.0f - u) + m_colorGreen.y() * u,
								m_colorRed.z() * (1.0f - u) + m_colorGreen.z() * u,
								m_colorRed.w() * (1.0f - u) + m_colorGreen.w() * u);

		referenceFrame.getLevel(0).setPixel(resultColor,x,0);
	}

	for (deUint32 y = 0u; y < m_extent.height; ++y)
	{
		deMemcpy (referenceFrame.getLevel(0).getPixelPtr(0,y), referenceFrame.getLevel(0).getPixelPtr(0,0), m_extent.width * m_extent.depth* referenceFrame.getFormat().getPixelSize());
	}
}

void CrossStageTestInstance::perspectiveFill (tcu::Texture2DArray& referenceFrame)
{
	float dynamics = 1.732f;
	float dynamicChange = 0.732f / static_cast<float>(m_extent.width);
	for (deUint32 x = 0u; x < m_extent.width; ++x)
	{
		float u = static_cast<float>(x)/static_cast<float>(m_extent.width - 1);
		const Vec4 resultColor (m_colorRed.x() * (1.0f - dynamics * u) + m_colorGreen.x() * u* dynamics,
								m_colorRed.y() * (1.0f - dynamics * u) + m_colorGreen.y() * u* dynamics,
								m_colorRed.z() * (1.0f - dynamics * u) + m_colorGreen.z() * u* dynamics,
								m_colorRed.w() * (1.0f - dynamics * u) + m_colorGreen.w() * u* dynamics);
		dynamics -= dynamicChange;
		if (dynamics < 1.0f)
			dynamics = 1.0f;

		referenceFrame.getLevel(0).setPixel(resultColor,x,0);
	}

	for (deUint32 y = 0u; y < m_extent.height; ++y)
	{
		deMemcpy (referenceFrame.getLevel(0).getPixelPtr(0,y), referenceFrame.getLevel(0).getPixelPtr(0,0), m_extent.width * m_extent.depth* referenceFrame.getFormat().getPixelSize());
	}
}

void CrossStageTestInstance::redFill (tcu::Texture2DArray& referenceFrame)
{
	for (deUint32 x = 0u; x < m_extent.width; ++x)
		referenceFrame.getLevel(0).setPixel(m_colorRed,x,0);

	for (deUint32 y = 0u; y < m_extent.height; ++y)
		deMemcpy (referenceFrame.getLevel(0).getPixelPtr(0,y), referenceFrame.getLevel(0).getPixelPtr(0,0), m_extent.width * m_extent.depth* referenceFrame.getFormat().getPixelSize());
}

struct Decorations
{
	Decorations()
	{};
	Decorations(const string& f, const string& v, const string& o)
		: fragment	(f)
		, vertex	(v)
		, others	(o)
	{};
	string fragment;
	string vertex;
	string others;
};

class CrossStageBasicTestsCase : public vkt::TestCase
{
public:
	CrossStageBasicTestsCase (tcu::TestContext &context, const char *name, const char *description, const TestParameters& parameters)
		: TestCase			(context, name, description)
		, m_parameters		(parameters)
	{
	}
private:
	vkt::TestInstance*	createInstance		(vkt::Context& context) const;
	void				initPrograms		(SourceCollections& programCollection) const;

	const TestParameters	m_parameters;

};

vkt::TestInstance* CrossStageBasicTestsCase::createInstance (vkt::Context& context) const
{
	return new CrossStageTestInstance(context, m_parameters);
}

void CrossStageBasicTestsCase::initPrograms (SourceCollections& programCollection) const
{
	vector<Decorations> decorations;
	string epsilon = "3e-7";
	switch(m_parameters.qualifier)
	{
	case TEST_TYPE_FLAT:
		decorations.push_back(Decorations("",
								//Vertex
								"OpDecorate %color_out Flat\n"
								"OpDecorate %color_in Flat\n"
								"OpDecorate %r_float_out Flat\n"
								"OpDecorate %rg_float_out Flat\n"
								"OpDecorate %rgb_float_out Flat\n"
								"OpDecorate %rgba_float_out Flat\n",
								""));
		decorations.push_back(Decorations(//Fragment
								"OpDecorate %color_in Flat\n"
								"OpDecorate %r_float_in Flat\n"
								"OpDecorate %rg_float_in Flat\n"
								"OpDecorate %rgb_float_in Flat\n"
								"OpDecorate %rgba_float_in Flat\n",
								"",
								""));

		decorations.push_back(Decorations(//Fragment
								"OpDecorate %color_in Flat\n"
								"OpDecorate %r_float_in Flat\n"
								"OpDecorate %rg_float_in Flat\n"
								"OpDecorate %rgb_float_in Flat\n"
								"OpDecorate %rgba_float_in Flat\n",
								//Vertex
								"OpDecorate %color_out Flat\n"
								"OpDecorate %color_in Flat\n"
								"OpDecorate %r_float_out Flat\n"
								"OpDecorate %rg_float_out Flat\n"
								"OpDecorate %rgb_float_out Flat\n"
								"OpDecorate %rgba_float_out Flat\n",
								""));
		epsilon = "0.0";
		break;
	case TEST_TYPE_NOPERSPECTIVE:
		decorations.push_back(Decorations("",
								//Vertex
								"OpDecorate %color_out NoPerspective\n"
								"OpDecorate %color_in NoPerspective\n"
								"OpDecorate %r_float_out NoPerspective\n"
								"OpDecorate %rg_float_out NoPerspective\n"
								"OpDecorate %rgb_float_out NoPerspective\n"
								"OpDecorate %rgba_float_out NoPerspective\n",
								""));

		decorations.push_back(Decorations(//Fragment
								"OpDecorate %color_in NoPerspective\n"
								"OpDecorate %r_float_in NoPerspective\n"
								"OpDecorate %rg_float_in NoPerspective\n"
								"OpDecorate %rgb_float_in NoPerspective\n"
								"OpDecorate %rgba_float_in NoPerspective\n",
								"",
								""));

		decorations.push_back(Decorations(//Fragment
								"OpDecorate %color_in NoPerspective\n"
								"OpDecorate %r_float_in NoPerspective\n"
								"OpDecorate %rg_float_in NoPerspective\n"
								"OpDecorate %rgb_float_in NoPerspective\n"
								"OpDecorate %rgba_float_in NoPerspective\n",
								//Vertex
								"OpDecorate %color_out NoPerspective\n"
								"OpDecorate %color_in NoPerspective\n"
								"OpDecorate %r_float_out NoPerspective\n"
								"OpDecorate %rg_float_out NoPerspective\n"
								"OpDecorate %rgb_float_out NoPerspective\n"
								"OpDecorate %rgba_float_out NoPerspective\n",
								//Others
								""));
		break;
	case TEST_TYPE_RELAXEDPRECISION:
		decorations.push_back(Decorations(//Fragment
								"OpDecorate %color_out RelaxedPrecision\n"
								"OpDecorate %color_in RelaxedPrecision\n"
								"OpDecorate %r_float_in RelaxedPrecision\n"
								"OpDecorate %rg_float_in RelaxedPrecision\n"
								"OpDecorate %rgb_float_in RelaxedPrecision\n"
								"OpDecorate %rgba_float_in RelaxedPrecision\n",
								//Vertex
								"OpDecorate %color_out RelaxedPrecision\n"
								"OpDecorate %color_in RelaxedPrecision\n"
								"OpDecorate %r_float_out RelaxedPrecision\n"
								"OpDecorate %rg_float_out RelaxedPrecision\n"
								"OpDecorate %rgb_float_out RelaxedPrecision\n"
								"OpDecorate %rgba_float_out RelaxedPrecision\n",
								//Others
								"OpDecorate %color_out RelaxedPrecision\n"
								"OpDecorate %color_in RelaxedPrecision\n"
								"OpDecorate %r_float_out RelaxedPrecision\n"
								"OpDecorate %rg_float_out RelaxedPrecision\n"
								"OpDecorate %rgb_float_out RelaxedPrecision\n"
								"OpDecorate %rgba_float_out RelaxedPrecision\n"
								"OpDecorate %r_float_in RelaxedPrecision\n"
								"OpDecorate %rg_float_in RelaxedPrecision\n"
								"OpDecorate %rgb_float_in RelaxedPrecision\n"
								"OpDecorate %rgba_float_in RelaxedPrecision\n"));
		epsilon = "2e-3";
		break;
	default:
		DE_ASSERT(0);
	}

	//Spir-v spec: decoration flat can be used only in Shader (fragment or vertex)
	for (deUint32 ndx = 0; ndx < decorations.size(); ++ndx)
	{

		/*#version 450
		layout(location = 0) in highp vec4 in_position;
		layout(location = 1) in vec4 in_color;
		layout(location = 0) out vec4 out_color;
		layout(location = 1) out float		r_float_out;
		layout(location = 2) out vec2		rg_float_out;
		layout(location = 3) out vec3		rgb_float_out;
		layout(location = 4) out vec4		rgba_float_out;
		void main (void)
		{
			gl_Position = in_position;
			out_color = in_color;
			r_float_out = in_color.r;
			rg_float_out = vec2(in_color.r, in_color.g);
			rgb_float_out = vec3(in_color.r, in_color.g,in_color.b);
			rgba_float_out = vec4(in_color.r, in_color.g, in_color.b, in_color.a);
		}
		*/
		const string vertexShaderSource =
		"; SPIR-V\n"
		"; Version: 1.3\n"
		"; Generator: Khronos Glslang Reference Front End; 2\n"
		"; Bound: 60\n"
		"; Schema: 0\n"
		"OpCapability Shader\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Vertex %4 \"main\" %13 %17 %color_out %color_in %r_float_out %rg_float_out %rgb_float_out %rgba_float_out\n"
		"OpMemberDecorate %11 0 BuiltIn Position\n"
		"OpMemberDecorate %11 1 BuiltIn PointSize\n"
		"OpMemberDecorate %11 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %11 3 BuiltIn CullDistance\n"
		"OpDecorate %11 Block\n"
		"OpDecorate %17 Location 0\n"
		"OpDecorate %color_out Location 0\n"
		"OpDecorate %color_in Location 1\n"
		"OpDecorate %r_float_out Location 1\n"
		"OpDecorate %rg_float_out Location 2\n"
		"OpDecorate %rgb_float_out Location 3\n"
		"OpDecorate %rgba_float_out Location 4\n"
		+decorations[ndx].vertex+
		"%2 = OpTypeVoid\n"
		"%3 = OpTypeFunction %2\n"
		"%6 = OpTypeFloat 32\n"
		"%7 = OpTypeVector %6 4\n"
		"%8 = OpTypeInt 32 0\n"
		"%9 = OpConstant %8 1\n"
		"%10 = OpTypeArray %6 %9\n"
		"%11 = OpTypeStruct %7 %6 %10 %10\n"
		"%12 = OpTypePointer Output %11\n"
		"%13 = OpVariable %12 Output\n"
		"%14 = OpTypeInt 32 1\n"
		"%15 = OpConstant %14 0\n"
		"%16 = OpTypePointer Input %7\n"
		"%17 = OpVariable %16 Input\n"
		"%19 = OpTypePointer Output %7\n"
		"%color_out = OpVariable %19 Output\n"
		"%color_in = OpVariable %16 Input\n"
		"%24 = OpTypePointer Output %6\n"
		"%r_float_out = OpVariable %24 Output\n"
		"%26 = OpConstant %8 0\n"
		"%27 = OpTypePointer Input %6\n"
		"%30 = OpTypeVector %6 2\n"
		"%31 = OpTypePointer Output %30\n"
		"%rg_float_out = OpVariable %31 Output\n"
		"%38 = OpTypeVector %6 3\n"
		"%39 = OpTypePointer Output %38\n"
		"%rgb_float_out = OpVariable %39 Output\n"
		"%45 = OpConstant %8 2\n"
		"%rgba_float_out = OpVariable %19 Output\n"
		"%56 = OpConstant %8 3\n"
		"%4 = OpFunction %2 None %3\n"
		"%5 = OpLabel\n"
		"%18 = OpLoad %7 %17\n"
		"%20 = OpAccessChain %19 %13 %15\n"
		"OpStore %20 %18\n"
		"%23 = OpLoad %7 %color_in\n"
		"OpStore %color_out %23\n"
		"%28 = OpAccessChain %27 %color_in %26\n"
		"%29 = OpLoad %6 %28\n"
		"OpStore %r_float_out %29\n"
		"%33 = OpAccessChain %27 %color_in %26\n"
		"%34 = OpLoad %6 %33\n"
		"%35 = OpAccessChain %27 %color_in %9\n"
		"%36 = OpLoad %6 %35\n"
		"%37 = OpCompositeConstruct %30 %34 %36\n"
		"OpStore %rg_float_out %37\n"
		"%41 = OpAccessChain %27 %color_in %26\n"
		"%42 = OpLoad %6 %41\n"
		"%43 = OpAccessChain %27 %color_in %9\n"
		"%44 = OpLoad %6 %43\n"
		"%46 = OpAccessChain %27 %color_in %45\n"
		"%47 = OpLoad %6 %46\n"
		"%48 = OpCompositeConstruct %38 %42 %44 %47\n"
		"OpStore %rgb_float_out %48\n"
		"%50 = OpAccessChain %27 %color_in %26\n"
		"%51 = OpLoad %6 %50\n"
		"%52 = OpAccessChain %27 %color_in %9\n"
		"%53 = OpLoad %6 %52\n"
		"%54 = OpAccessChain %27 %color_in %45\n"
		"%55 = OpLoad %6 %54\n"
		"%57 = OpAccessChain %27 %color_in %56\n"
		"%58 = OpLoad %6 %57\n"
		"%59 = OpCompositeConstruct %7 %51 %53 %55 %58\n"
		"OpStore %rgba_float_out %59\n"
		"OpReturn\n"
		"OpFunctionEnd\n";

		/* #version 450
		layout(location = 0) out vec4  color_out;
		layout(location = 0) in vec4   color_in;
		layout(location = 1) in float  r_float_in;
		layout(location = 2) in vec2   rg_float_in;
		layout(location = 3) in vec3   rgb_float_in;
		layout(location = 4) in vec4   rgba_float_in;
		void main()
		{
			float epsilon = 3e-7; // or 0.0 for flat, or 2e-3 for RelaxedPrecision (mediump)
			color_out = color_in;
			float epsilon_float = max(abs(r_float_in), abs(color_in.r)) * epsilon;
			if(abs(r_float_in - color_in.r) > epsilon_float)
				color_out.r = 1.0f;
			vec2 epsilon_vec2 = max(abs(rg_float_in), abs(color_in.rg)) * epsilon;
			if(any(greaterThan(abs(rg_float_in - color_in.rg), epsilon_vec2)))
				color_out.rg = vec2(1.0f);
			vec3 epsilon_vec3 = max(abs(rgb_float_in), abs(color_in.rgb)) * epsilon;
			if(any(greaterThan(abs(rgb_float_in - color_in.rgb), epsilon_vec3)))
				color_out.rgb = vec3(1.0f);
			vec4 epsilon_vec4 = max(abs(rgba_float_in), abs(color_in.rgba)) * epsilon;
			if(any(greaterThan(abs(rgba_float_in - color_in.rgba), epsilon_vec4)))
				color_out.rgba = vec4(1.0f);
		}
		*/

		const string fragmentShaderSource =
		"; SPIR-V\n"
		"; Version: 1.3\n"
		"; Generator: Khronos Glslang Reference Front End; 2\n"
		"; Bound: 64\n"
		"; Schema: 0\n"
		"OpCapability Shader\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Fragment %4 \"main\" %color_out %color_in %r_float_in %rg_float_in %rgb_float_in %rgba_float_in\n"
		"OpExecutionMode %4 OriginUpperLeft\n"
		"OpDecorate %color_out Location 0\n"
		"OpDecorate %color_in Location 0\n"
		"OpDecorate %r_float_in Location 1\n"
		"OpDecorate %rg_float_in Location 2\n"
		"OpDecorate %rgb_float_in Location 3\n"
		"OpDecorate %rgba_float_in Location 4\n"
		+decorations[ndx].fragment+
		"%2 = OpTypeVoid\n"
		"%3 = OpTypeFunction %2\n"
		"%6 = OpTypeFloat 32\n"
		"%7 = OpTypeVector %6 4\n"
		"%8 = OpTypePointer Output %7\n"
		"%color_out = OpVariable %8 Output\n"
		"%10 = OpTypePointer Input %7\n"
		"%color_in = OpVariable %10 Input\n"
		"%13 = OpTypePointer Input %6\n"
		"%r_float_in = OpVariable %13 Input\n"
		"%16 = OpTypeInt 32 0\n"
		"%17 = OpConstant %16 0\n"
		"%20 = OpTypeBool\n"
		"%ep = OpConstant %6 " + epsilon + "\n"
		"%24 = OpConstant %6 1\n"
		"%25 = OpTypePointer Output %6\n"
		"%27 = OpTypeVector %6 2\n"
		"%28 = OpTypePointer Input %27\n"
		"%rg_float_in = OpVariable %28 Input\n"
		"%ep2 = OpConstantComposite %27 %ep %ep\n"
		"%33 = OpTypeVector %20 2\n"
		"%38 = OpConstantComposite %27 %24 %24\n"
		"%41 = OpTypeVector %6 3\n"
		"%42 = OpTypePointer Input %41\n"
		"%rgb_float_in = OpVariable %42 Input\n"
		"%ep3 = OpConstantComposite %41 %ep %ep %ep\n"
		"%47 = OpTypeVector %20 3\n"
		"%52 = OpConstantComposite %41 %24 %24 %24\n"
		"%rgba_float_in = OpVariable %10 Input\n"
		"%ep4 = OpConstantComposite %7 %ep %ep %ep %ep\n"
		"%58 = OpTypeVector %20 4\n"
		"%63 = OpConstantComposite %7 %24 %24 %24 %24\n"
		"%4 = OpFunction %2 None %3\n"
		"%5 = OpLabel\n"
		"%12 = OpLoad %7 %color_in\n"
		"OpStore %color_out %12\n"
		"%15 = OpLoad %6 %r_float_in\n"
		"%18 = OpAccessChain %13 %color_in %17\n"
		"%19 = OpLoad %6 %18\n"
		"%sub = OpFSub %6 %15 %19\n"
		"%abs = OpExtInst %6 %1 FAbs %sub\n"
		"%ep1abs0 = OpExtInst %6 %1 FAbs %15\n"
		"%ep1abs1 = OpExtInst %6 %1 FAbs %19\n"
		"%ep1gt = OpFOrdGreaterThan %20 %ep1abs0 %ep1abs1\n"
		"%ep1max = OpSelect %6 %ep1gt %ep1abs0 %ep1abs1\n"
		"%ep1rel = OpFMul %6 %ep1max %ep\n"
		"%cmp = OpFOrdGreaterThan %20 %abs %ep1rel\n"
		"OpSelectionMerge %23 None\n"
		"OpBranchConditional %cmp %22 %23\n"
		"%22 = OpLabel\n"
		"%26 = OpAccessChain %25 %color_out %17\n"
		"OpStore %26 %24\n"
		"OpBranch %23\n"
		"%23 = OpLabel\n"
		"%30 = OpLoad %27 %rg_float_in\n"
		"%31 = OpLoad %7 %color_in\n"
		"%32 = OpVectorShuffle %27 %31 %31 0 1\n"
		"%sub2 = OpFSub %27 %30 %32\n"
		"%abs2 = OpExtInst %27 %1 FAbs %sub2\n"
		"%ep2abs0 = OpExtInst %27 %1 FAbs %30\n"
		"%ep2abs1 = OpExtInst %27 %1 FAbs %32\n"
		"%ep2gt = OpFOrdGreaterThan %33 %ep2abs0 %ep2abs1\n"
		"%ep2max = OpSelect %27 %ep2gt %ep2abs0 %ep2abs1\n"
		"%ep2rel = OpFMul %27 %ep2max %ep2\n"
		"%cmp2 = OpFOrdGreaterThan %33 %abs2 %ep2rel\n"
		"%35 = OpAny %20 %cmp2\n"
		"OpSelectionMerge %37 None\n"
		"OpBranchConditional %35 %36 %37\n"
		"%36 = OpLabel\n"
		"%39 = OpLoad %7 %color_out\n"
		"%40 = OpVectorShuffle %7 %39 %38 4 5 2 3\n"
		"OpStore %color_out %40\n"
		"OpBranch %37\n"
		"%37 = OpLabel\n"
		"%44 = OpLoad %41 %rgb_float_in\n"
		"%45 = OpLoad %7 %color_in\n"
		"%46 = OpVectorShuffle %41 %45 %45 0 1 2\n"
		"%sub3 = OpFSub %41 %44 %46\n"
		"%abs3 = OpExtInst %41 %1 FAbs %sub3\n"
		"%ep3abs0 = OpExtInst %41 %1 FAbs %44\n"
		"%ep3abs1 = OpExtInst %41 %1 FAbs %46\n"
		"%ep3gt = OpFOrdGreaterThan %47 %ep3abs0 %ep3abs1\n"
		"%ep3max = OpSelect %41 %ep3gt %ep3abs0 %ep3abs1\n"
		"%ep3rel = OpFMul %41 %ep3max %ep3\n"
		"%cmp3 = OpFOrdGreaterThan %47 %abs3 %ep3rel\n"
		"%49 = OpAny %20 %cmp3\n"
		"OpSelectionMerge %51 None\n"
		"OpBranchConditional %49 %50 %51\n"
		"%50 = OpLabel\n"
		"%53 = OpLoad %7 %color_out\n"
		"%54 = OpVectorShuffle %7 %53 %52 4 5 6 3\n"
		"OpStore %color_out %54\n"
		"OpBranch %51\n"
		"%51 = OpLabel\n"
		"%56 = OpLoad %7 %rgba_float_in\n"
		"%57 = OpLoad %7 %color_in\n"
		"%sub4 = OpFSub %7 %56 %57\n"
		"%abs4 = OpExtInst %7 %1 FAbs %sub4\n"
		"%ep4abs0 = OpExtInst %7 %1 FAbs %56\n"
		"%ep4abs1 = OpExtInst %7 %1 FAbs %57\n"
		"%ep4gt = OpFOrdGreaterThan %58 %ep4abs0 %ep4abs1\n"
		"%ep4max = OpSelect %7 %ep4gt %ep4abs0 %ep4abs1\n"
		"%ep4rel = OpFMul %7 %ep4max %ep4\n"
		"%cmp4 = OpFOrdGreaterThan %58 %abs4 %ep4rel\n"
		"%60 = OpAny %20 %cmp4\n"
		"OpSelectionMerge %62 None\n"
		"OpBranchConditional %60 %61 %62\n"
		"%61 = OpLabel\n"
		"OpStore %color_out %63\n"
		"OpBranch %62\n"
		"%62 = OpLabel\n"
		"OpReturn\n"
		"OpFunctionEnd\n";

		std::ostringstream vertex;
		vertex << "vertex" << ndx;
		std::ostringstream fragment;
		fragment << "fragment" << ndx;

		programCollection.spirvAsmSources.add(vertex.str()) << vertexShaderSource;
		programCollection.spirvAsmSources.add(fragment.str()) << fragmentShaderSource;
	}
	{
		/*#version 450
		#extension GL_EXT_tessellation_shader : require
		layout(vertices = 4) out;
		layout(location = 0) in vec4		in_color[];
		layout(location = 1) in float		r_float_in[];
		layout(location = 2) in vec2		rg_float_in[];
		layout(location = 3) in vec3		rgb_float_in[];
		layout(location = 4) in vec4		rgba_float_in[];
		layout(location = 0) out vec4		out_color[];
		layout(location = 1) out float		r_float_out[];
		layout(location = 2) out vec2		rg_float_out[];
		layout(location = 3) out vec3		rgb_float_out[];
		layout(location = 4) out vec4		rgba_float_out[];
		void main (void)
		{
			if ( gl_InvocationID == 0 )
			{
				gl_TessLevelInner[0] = 4.0f;
				gl_TessLevelInner[1] = 4.0f;
				gl_TessLevelOuter[0] = 4.0f;
				gl_TessLevelOuter[1] = 4.0f;
				gl_TessLevelOuter[2] = 4.0f;
				gl_TessLevelOuter[3] = 4.0f;
			}
			out_color[gl_InvocationID] = in_color[gl_InvocationID];
			r_float_out[gl_InvocationID] = r_float_in[gl_InvocationID];
			rg_float_out[gl_InvocationID] = rg_float_in[gl_InvocationID];
			rgb_float_out[gl_InvocationID] = rgb_float_in[gl_InvocationID];
			rgba_float_out[gl_InvocationID] = rgba_float_in[gl_InvocationID];
			gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
		}*/

		const string tessellationControlSource =
		"; SPIR-V\n"
		"; Version: 1.3\n"
		"; Generator: Khronos Glslang Reference Front End; 2\n"
		"; Bound: 111\n"
		"; Schema: 0\n"
		"OpCapability Tessellation\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint TessellationControl %4 \"main\" %8 %20 %29 %color_out %color_in %r_float_out %r_float_in %rg_float_out %rg_float_in %rgb_float_out %rgb_float_in %rgba_float_out %rgba_float_in %101 %106\n"
		"OpExecutionMode %4 OutputVertices 4\n"
		"OpDecorate %8 BuiltIn InvocationId\n"
		"OpDecorate %20 Patch\n"
		"OpDecorate %20 BuiltIn TessLevelInner\n"
		"OpDecorate %29 Patch\n"
		"OpDecorate %29 BuiltIn TessLevelOuter\n"
		"OpDecorate %color_out Location 0\n"
		"OpDecorate %color_in Location 0\n"
		"OpDecorate %r_float_out Location 1\n"
		"OpDecorate %r_float_in Location 1\n"
		"OpDecorate %rg_float_out Location 2\n"
		"OpDecorate %rg_float_in Location 2\n"
		"OpDecorate %rgb_float_out Location 3\n"
		"OpDecorate %rgb_float_in Location 3\n"
		"OpDecorate %rgba_float_out Location 4\n"
		"OpDecorate %rgba_float_in Location 4\n"
		+decorations[0].others+
		"OpMemberDecorate %98 0 BuiltIn Position\n"
		"OpMemberDecorate %98 1 BuiltIn PointSize\n"
		"OpMemberDecorate %98 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %98 3 BuiltIn CullDistance\n"
		"OpDecorate %98 Block\n"
		"OpMemberDecorate %103 0 BuiltIn Position\n"
		"OpMemberDecorate %103 1 BuiltIn PointSize\n"
		"OpMemberDecorate %103 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %103 3 BuiltIn CullDistance\n"
		"OpDecorate %103 Block\n"
		"%2 = OpTypeVoid\n"
		"%3 = OpTypeFunction %2\n"
		"%6 = OpTypeInt 32 1\n"
		"%7 = OpTypePointer Input %6\n"
		"%8 = OpVariable %7 Input\n"
		"%10 = OpConstant %6 0\n"
		"%11 = OpTypeBool\n"
		"%15 = OpTypeFloat 32\n"
		"%16 = OpTypeInt 32 0\n"
		"%17 = OpConstant %16 2\n"
		"%18 = OpTypeArray %15 %17\n"
		"%19 = OpTypePointer Output %18\n"
		"%20 = OpVariable %19 Output\n"
		"%21 = OpConstant %15 4\n"
		"%22 = OpTypePointer Output %15\n"
		"%24 = OpConstant %6 1\n"
		"%26 = OpConstant %16 4\n"
		"%27 = OpTypeArray %15 %26\n"
		"%28 = OpTypePointer Output %27\n"
		"%29 = OpVariable %28 Output\n"
		"%32 = OpConstant %6 2\n"
		"%34 = OpConstant %6 3\n"
		"%36 = OpTypeVector %15 4\n"
		"%37 = OpTypeArray %36 %26\n"
		"%38 = OpTypePointer Output %37\n"
		"%color_out = OpVariable %38 Output\n"
		"%41 = OpConstant %16 32\n"
		"%42 = OpTypeArray %36 %41\n"
		"%43 = OpTypePointer Input %42\n"
		"%color_in = OpVariable %43 Input\n"
		"%46 = OpTypePointer Input %36\n"
		"%49 = OpTypePointer Output %36\n"
		"%r_float_out = OpVariable %28 Output\n"
		"%53 = OpTypeArray %15 %41\n"
		"%54 = OpTypePointer Input %53\n"
		"%r_float_in = OpVariable %54 Input\n"
		"%57 = OpTypePointer Input %15\n"
		"%61 = OpTypeVector %15 2\n"
		"%62 = OpTypeArray %61 %26\n"
		"%63 = OpTypePointer Output %62\n"
		"%rg_float_out = OpVariable %63 Output\n"
		"%66 = OpTypeArray %61 %41\n"
		"%67 = OpTypePointer Input %66\n"
		"%rg_float_in = OpVariable %67 Input\n"
		"%70 = OpTypePointer Input %61\n"
		"%73 = OpTypePointer Output %61\n"
		"%75 = OpTypeVector %15 3\n"
		"%76 = OpTypeArray %75 %26\n"
		"%77 = OpTypePointer Output %76\n"
		"%rgb_float_out = OpVariable %77 Output\n"
		"%80 = OpTypeArray %75 %41\n"
		"%81 = OpTypePointer Input %80\n"
		"%rgb_float_in = OpVariable %81 Input\n"
		"%84 = OpTypePointer Input %75\n"
		"%87 = OpTypePointer Output %75\n"
		"%rgba_float_out = OpVariable %38 Output\n"
		"%rgba_float_in = OpVariable %43 Input\n"
		"%96 = OpConstant %16 1\n"
		"%97 = OpTypeArray %15 %96\n"
		"%98 = OpTypeStruct %36 %15 %97 %97\n"
		"%99 = OpTypeArray %98 %26\n"
		"%100 = OpTypePointer Output %99\n"
		"%101 = OpVariable %100 Output\n"
		"%103 = OpTypeStruct %36 %15 %97 %97\n"
		"%104 = OpTypeArray %103 %41\n"
		"%105 = OpTypePointer Input %104\n"
		"%106 = OpVariable %105 Input\n"
		"%4 = OpFunction %2 None %3\n"
		"%5 = OpLabel\n"
		"%9 = OpLoad %6 %8\n"
		"%12 = OpIEqual %11 %9 %10\n"
		"OpSelectionMerge %14 None\n"
		"OpBranchConditional %12 %13 %14\n"
		"%13 = OpLabel\n"
		"%23 = OpAccessChain %22 %20 %10\n"
		"OpStore %23 %21\n"
		"%25 = OpAccessChain %22 %20 %24\n"
		"OpStore %25 %21\n"
		"%30 = OpAccessChain %22 %29 %10\n"
		"OpStore %30 %21\n"
		"%31 = OpAccessChain %22 %29 %24\n"
		"OpStore %31 %21\n"
		"%33 = OpAccessChain %22 %29 %32\n"
		"OpStore %33 %21\n"
		"%35 = OpAccessChain %22 %29 %34\n"
		"OpStore %35 %21\n"
		"OpBranch %14\n"
		"%14 = OpLabel\n"
		"%40 = OpLoad %6 %8\n"
		"%45 = OpLoad %6 %8\n"
		"%47 = OpAccessChain %46 %color_in %45\n"
		"%48 = OpLoad %36 %47\n"
		"%50 = OpAccessChain %49 %color_out %40\n"
		"OpStore %50 %48\n"
		"%52 = OpLoad %6 %8\n"
		"%56 = OpLoad %6 %8\n"
		"%58 = OpAccessChain %57 %r_float_in %56\n"
		"%59 = OpLoad %15 %58\n"
		"%60 = OpAccessChain %22 %r_float_out %52\n"
		"OpStore %60 %59\n"
		"%65 = OpLoad %6 %8\n"
		"%69 = OpLoad %6 %8\n"
		"%71 = OpAccessChain %70 %rg_float_in %69\n"
		"%72 = OpLoad %61 %71\n"
		"%74 = OpAccessChain %73 %rg_float_out %65\n"
		"OpStore %74 %72\n"
		"%79 = OpLoad %6 %8\n"
		"%83 = OpLoad %6 %8\n"
		"%85 = OpAccessChain %84 %rgb_float_in %83\n"
		"%86 = OpLoad %75 %85\n"
		"%88 = OpAccessChain %87 %rgb_float_out %79\n"
		"OpStore %88 %86\n"
		"%90 = OpLoad %6 %8\n"
		"%92 = OpLoad %6 %8\n"
		"%93 = OpAccessChain %46 %rgba_float_in %92\n"
		"%94 = OpLoad %36 %93\n"
		"%95 = OpAccessChain %49 %rgba_float_out %90\n"
		"OpStore %95 %94\n"
		"%102 = OpLoad %6 %8\n"
		"%107 = OpLoad %6 %8\n"
		"%108 = OpAccessChain %46 %106 %107 %10\n"
		"%109 = OpLoad %36 %108\n"
		"%110 = OpAccessChain %49 %101 %102 %10\n"
		"OpStore %110 %109\n"
		"OpReturn\n"
		"OpFunctionEnd\n";

		/*#version 450
		#extension GL_EXT_tessellation_shader : require
		layout( quads, equal_spacing, ccw ) in;
		layout(location = 0) in vec4		in_color[];
		layout(location = 1) in float		r_float_in[];
		layout(location = 2) in vec2		rg_float_in[];
		layout(location = 3) in vec3		rgb_float_in[];
		layout(location = 4) in vec4		rgba_float_in[];
		layout(location = 0) out vec4		out_color;
		layout(location = 1) out float		r_float_out;
		layout(location = 2) out vec2		rg_float_out;
		layout(location = 3) out vec3		rgb_float_out;
		layout(location = 4) out vec4		rgba_float_out;
		void main (void)
		{
			const float u = gl_TessCoord.x;
			const float v = gl_TessCoord.y;
			const float w = gl_TessCoord.z;
			out_color = (1 - u) * (1 - v) * in_color[0] +(1 - u) * v * in_color[1] + u * (1 - v) * in_color[2] + u * v * in_color[3];
			r_float_out = (1 - u) * (1 - v) * r_float_in[0] +(1 - u) * v * r_float_in[1] + u * (1 - v) * r_float_in[2] + u * v * r_float_in[3];
			rg_float_out = (1 - u) * (1 - v) * rg_float_in[0] +(1 - u) * v * rg_float_in[1] + u * (1 - v) * rg_float_in[2] + u * v * rg_float_in[3];
			rgb_float_out = (1 - u) * (1 - v) * rgb_float_in[0] +(1 - u) * v * rgb_float_in[1] + u * (1 - v) * rgb_float_in[2] + u * v * rgb_float_in[3];
			rgba_float_out = (1 - u) * (1 - v) * rgba_float_in[0] +(1 - u) * v * rgba_float_in[1] + u * (1 - v) * rgba_float_in[2] + u * v * rgba_float_in[3];
			gl_Position = (1 - u) * (1 - v) * gl_in[0].gl_Position +(1 - u) * v * gl_in[1].gl_Position + u * (1 - v) * gl_in[2].gl_Position + u * v * gl_in[3].gl_Position;
		}*/

		const string tessellationEvaluationSource =
		"; SPIR-V\n"
		"; Version: 1.3\n"
		"; Generator: Khronos Glslang Reference Front End; 2\n"
		"; Bound: 253\n"
		"; Schema: 0\n"
		"OpCapability Tessellation\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint TessellationEvaluation %4 \"main\" %11 %color_out %color_in %r_float_out %r_float_in %rg_float_out %rg_float_in %rgb_float_out %rgb_float_in %rgba_float_out %rgba_float_in %216 %225\n"
		"OpExecutionMode %4 Quads\n"
		"OpExecutionMode %4 SpacingEqual\n"
		"OpExecutionMode %4 VertexOrderCcw\n"
		"OpDecorate %11 BuiltIn TessCoord\n"
		"OpDecorate %color_out Location 0\n"
		"OpDecorate %color_in Location 0\n"
		"OpDecorate %r_float_out Location 1\n"
		"OpDecorate %r_float_in Location 1\n"
		"OpDecorate %rg_float_out Location 2\n"
		"OpDecorate %rg_float_in Location 2\n"
		"OpDecorate %rgb_float_out Location 3\n"
		"OpDecorate %rgb_float_in Location 3\n"
		"OpDecorate %rgba_float_out Location 4\n"
		"OpDecorate %rgba_float_in Location 4\n"
		+decorations[0].others+
		"OpMemberDecorate %214 0 BuiltIn Position\n"
		"OpMemberDecorate %214 1 BuiltIn PointSize\n"
		"OpMemberDecorate %214 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %214 3 BuiltIn CullDistance\n"
		"OpDecorate %214 Block\n"
		"OpMemberDecorate %222 0 BuiltIn Position\n"
		"OpMemberDecorate %222 1 BuiltIn PointSize\n"
		"OpMemberDecorate %222 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %222 3 BuiltIn CullDistance\n"
		"OpDecorate %222 Block\n"
		"%2 = OpTypeVoid\n"
		"%3 = OpTypeFunction %2\n"
		"%6 = OpTypeFloat 32\n"
		"%7 = OpTypePointer Function %6\n"
		"%9 = OpTypeVector %6 3\n"
		"%10 = OpTypePointer Input %9\n"
		"%11 = OpVariable %10 Input\n"
		"%12 = OpTypeInt 32 0\n"
		"%13 = OpConstant %12 0\n"
		"%14 = OpTypePointer Input %6\n"
		"%18 = OpConstant %12 1\n"
		"%22 = OpConstant %12 2\n"
		"%25 = OpTypeVector %6 4\n"
		"%26 = OpTypePointer Output %25\n"
		"%color_out = OpVariable %26 Output\n"
		"%28 = OpConstant %6 1\n"
		"%34 = OpConstant %12 32\n"
		"%35 = OpTypeArray %25 %34\n"
		"%36 = OpTypePointer Input %35\n"
		"%color_in = OpVariable %36 Input\n"
		"%38 = OpTypeInt 32 1\n"
		"%39 = OpConstant %38 0\n"
		"%40 = OpTypePointer Input %25\n"
		"%48 = OpConstant %38 1\n"
		"%57 = OpConstant %38 2\n"
		"%65 = OpConstant %38 3\n"
		"%70 = OpTypePointer Output %6\n"
		"%r_float_out = OpVariable %70 Output\n"
		"%77 = OpTypeArray %6 %34\n"
		"%78 = OpTypePointer Input %77\n"
		"%r_float_in = OpVariable %78 Input\n"
		"%106 = OpTypeVector %6 2\n"
		"%107 = OpTypePointer Output %106\n"
		"%rg_float_out = OpVariable %107 Output\n"
		"%114 = OpTypeArray %106 %34\n"
		"%115 = OpTypePointer Input %114\n"
		"%rg_float_in = OpVariable %115 Input\n"
		"%117 = OpTypePointer Input %106\n"
		"%144 = OpTypePointer Output %9\n"
		"%rgb_float_out = OpVariable %144 Output\n"
		"%151 = OpTypeArray %9 %34\n"
		"%152 = OpTypePointer Input %151\n"
		"%rgb_float_in = OpVariable %152 Input\n"
		"%rgba_float_out = OpVariable %26 Output\n"
		"%rgba_float_in = OpVariable %36 Input\n"
		"%213 = OpTypeArray %6 %18\n"
		"%214 = OpTypeStruct %25 %6 %213 %213\n"
		"%215 = OpTypePointer Output %214\n"
		"%216 = OpVariable %215 Output\n"
		"%222 = OpTypeStruct %25 %6 %213 %213\n"
		"%223 = OpTypeArray %222 %34\n"
		"%224 = OpTypePointer Input %223\n"
		"%225 = OpVariable %224 Input\n"
		"%4 = OpFunction %2 None %3\n"
		"%5 = OpLabel\n"
		"%8 = OpVariable %7 Function\n"
		"%17 = OpVariable %7 Function\n"
		"%21 = OpVariable %7 Function\n"
		"%15 = OpAccessChain %14 %11 %13\n"
		"%16 = OpLoad %6 %15\n"
		"OpStore %8 %16\n"
		"%19 = OpAccessChain %14 %11 %18\n"
		"%20 = OpLoad %6 %19\n"
		"OpStore %17 %20\n"
		"%23 = OpAccessChain %14 %11 %22\n"
		"%24 = OpLoad %6 %23\n"
		"OpStore %21 %24\n"
		"%29 = OpLoad %6 %8\n"
		"%30 = OpFSub %6 %28 %29\n"
		"%31 = OpLoad %6 %17\n"
		"%32 = OpFSub %6 %28 %31\n"
		"%33 = OpFMul %6 %30 %32\n"
		"%41 = OpAccessChain %40 %color_in %39\n"
		"%42 = OpLoad %25 %41\n"
		"%43 = OpVectorTimesScalar %25 %42 %33\n"
		"%44 = OpLoad %6 %8\n"
		"%45 = OpFSub %6 %28 %44\n"
		"%46 = OpLoad %6 %17\n"
		"%47 = OpFMul %6 %45 %46\n"
		"%49 = OpAccessChain %40 %color_in %48\n"
		"%50 = OpLoad %25 %49\n"
		"%51 = OpVectorTimesScalar %25 %50 %47\n"
		"%52 = OpFAdd %25 %43 %51\n"
		"%53 = OpLoad %6 %8\n"
		"%54 = OpLoad %6 %17\n"
		"%55 = OpFSub %6 %28 %54\n"
		"%56 = OpFMul %6 %53 %55\n"
		"%58 = OpAccessChain %40 %color_in %57\n"
		"%59 = OpLoad %25 %58\n"
		"%60 = OpVectorTimesScalar %25 %59 %56\n"
		"%61 = OpFAdd %25 %52 %60\n"
		"%62 = OpLoad %6 %8\n"
		"%63 = OpLoad %6 %17\n"
		"%64 = OpFMul %6 %62 %63\n"
		"%66 = OpAccessChain %40 %color_in %65\n"
		"%67 = OpLoad %25 %66\n"
		"%68 = OpVectorTimesScalar %25 %67 %64\n"
		"%69 = OpFAdd %25 %61 %68\n"
		"OpStore %color_out %69\n"
		"%72 = OpLoad %6 %8\n"
		"%73 = OpFSub %6 %28 %72\n"
		"%74 = OpLoad %6 %17\n"
		"%75 = OpFSub %6 %28 %74\n"
		"%76 = OpFMul %6 %73 %75\n"
		"%80 = OpAccessChain %14 %r_float_in %39\n"
		"%81 = OpLoad %6 %80\n"
		"%82 = OpFMul %6 %76 %81\n"
		"%83 = OpLoad %6 %8\n"
		"%84 = OpFSub %6 %28 %83\n"
		"%85 = OpLoad %6 %17\n"
		"%86 = OpFMul %6 %84 %85\n"
		"%87 = OpAccessChain %14 %r_float_in %48\n"
		"%88 = OpLoad %6 %87\n"
		"%89 = OpFMul %6 %86 %88\n"
		"%90 = OpFAdd %6 %82 %89\n"
		"%91 = OpLoad %6 %8\n"
		"%92 = OpLoad %6 %17\n"
		"%93 = OpFSub %6 %28 %92\n"
		"%94 = OpFMul %6 %91 %93\n"
		"%95 = OpAccessChain %14 %r_float_in %57\n"
		"%96 = OpLoad %6 %95\n"
		"%97 = OpFMul %6 %94 %96\n"
		"%98 = OpFAdd %6 %90 %97\n"
		"%99 = OpLoad %6 %8\n"
		"%100 = OpLoad %6 %17\n"
		"%101 = OpFMul %6 %99 %100\n"
		"%102 = OpAccessChain %14 %r_float_in %65\n"
		"%103 = OpLoad %6 %102\n"
		"%104 = OpFMul %6 %101 %103\n"
		"%105 = OpFAdd %6 %98 %104\n"
		"OpStore %r_float_out %105\n"
		"%109 = OpLoad %6 %8\n"
		"%110 = OpFSub %6 %28 %109\n"
		"%111 = OpLoad %6 %17\n"
		"%112 = OpFSub %6 %28 %111\n"
		"%113 = OpFMul %6 %110 %112\n"
		"%118 = OpAccessChain %117 %rg_float_in %39\n"
		"%119 = OpLoad %106 %118\n"
		"%120 = OpVectorTimesScalar %106 %119 %113\n"
		"%121 = OpLoad %6 %8\n"
		"%122 = OpFSub %6 %28 %121\n"
		"%123 = OpLoad %6 %17\n"
		"%124 = OpFMul %6 %122 %123\n"
		"%125 = OpAccessChain %117 %rg_float_in %48\n"
		"%126 = OpLoad %106 %125\n"
		"%127 = OpVectorTimesScalar %106 %126 %124\n"
		"%128 = OpFAdd %106 %120 %127\n"
		"%129 = OpLoad %6 %8\n"
		"%130 = OpLoad %6 %17\n"
		"%131 = OpFSub %6 %28 %130\n"
		"%132 = OpFMul %6 %129 %131\n"
		"%133 = OpAccessChain %117 %rg_float_in %57\n"
		"%134 = OpLoad %106 %133\n"
		"%135 = OpVectorTimesScalar %106 %134 %132\n"
		"%136 = OpFAdd %106 %128 %135\n"
		"%137 = OpLoad %6 %8\n"
		"%138 = OpLoad %6 %17\n"
		"%139 = OpFMul %6 %137 %138\n"
		"%140 = OpAccessChain %117 %rg_float_in %65\n"
		"%141 = OpLoad %106 %140\n"
		"%142 = OpVectorTimesScalar %106 %141 %139\n"
		"%143 = OpFAdd %106 %136 %142\n"
		"OpStore %rg_float_out %143\n"
		"%146 = OpLoad %6 %8\n"
		"%147 = OpFSub %6 %28 %146\n"
		"%148 = OpLoad %6 %17\n"
		"%149 = OpFSub %6 %28 %148\n"
		"%150 = OpFMul %6 %147 %149\n"
		"%154 = OpAccessChain %10 %rgb_float_in %39\n"
		"%155 = OpLoad %9 %154\n"
		"%156 = OpVectorTimesScalar %9 %155 %150\n"
		"%157 = OpLoad %6 %8\n"
		"%158 = OpFSub %6 %28 %157\n"
		"%159 = OpLoad %6 %17\n"
		"%160 = OpFMul %6 %158 %159\n"
		"%161 = OpAccessChain %10 %rgb_float_in %48\n"
		"%162 = OpLoad %9 %161\n"
		"%163 = OpVectorTimesScalar %9 %162 %160\n"
		"%164 = OpFAdd %9 %156 %163\n"
		"%165 = OpLoad %6 %8\n"
		"%166 = OpLoad %6 %17\n"
		"%167 = OpFSub %6 %28 %166\n"
		"%168 = OpFMul %6 %165 %167\n"
		"%169 = OpAccessChain %10 %rgb_float_in %57\n"
		"%170 = OpLoad %9 %169\n"
		"%171 = OpVectorTimesScalar %9 %170 %168\n"
		"%172 = OpFAdd %9 %164 %171\n"
		"%173 = OpLoad %6 %8\n"
		"%174 = OpLoad %6 %17\n"
		"%175 = OpFMul %6 %173 %174\n"
		"%176 = OpAccessChain %10 %rgb_float_in %65\n"
		"%177 = OpLoad %9 %176\n"
		"%178 = OpVectorTimesScalar %9 %177 %175\n"
		"%179 = OpFAdd %9 %172 %178\n"
		"OpStore %rgb_float_out %179\n"
		"%181 = OpLoad %6 %8\n"
		"%182 = OpFSub %6 %28 %181\n"
		"%183 = OpLoad %6 %17\n"
		"%184 = OpFSub %6 %28 %183\n"
		"%185 = OpFMul %6 %182 %184\n"
		"%187 = OpAccessChain %40 %rgba_float_in %39\n"
		"%188 = OpLoad %25 %187\n"
		"%189 = OpVectorTimesScalar %25 %188 %185\n"
		"%190 = OpLoad %6 %8\n"
		"%191 = OpFSub %6 %28 %190\n"
		"%192 = OpLoad %6 %17\n"
		"%193 = OpFMul %6 %191 %192\n"
		"%194 = OpAccessChain %40 %rgba_float_in %48\n"
		"%195 = OpLoad %25 %194\n"
		"%196 = OpVectorTimesScalar %25 %195 %193\n"
		"%197 = OpFAdd %25 %189 %196\n"
		"%198 = OpLoad %6 %8\n"
		"%199 = OpLoad %6 %17\n"
		"%200 = OpFSub %6 %28 %199\n"
		"%201 = OpFMul %6 %198 %200\n"
		"%202 = OpAccessChain %40 %rgba_float_in %57\n"
		"%203 = OpLoad %25 %202\n"
		"%204 = OpVectorTimesScalar %25 %203 %201\n"
		"%205 = OpFAdd %25 %197 %204\n"
		"%206 = OpLoad %6 %8\n"
		"%207 = OpLoad %6 %17\n"
		"%208 = OpFMul %6 %206 %207\n"
		"%209 = OpAccessChain %40 %rgba_float_in %65\n"
		"%210 = OpLoad %25 %209\n"
		"%211 = OpVectorTimesScalar %25 %210 %208\n"
		"%212 = OpFAdd %25 %205 %211\n"
		"OpStore %rgba_float_out %212\n"
		"%217 = OpLoad %6 %8\n"
		"%218 = OpFSub %6 %28 %217\n"
		"%219 = OpLoad %6 %17\n"
		"%220 = OpFSub %6 %28 %219\n"
		"%221 = OpFMul %6 %218 %220\n"
		"%226 = OpAccessChain %40 %225 %39 %39\n"
		"%227 = OpLoad %25 %226\n"
		"%228 = OpVectorTimesScalar %25 %227 %221\n"
		"%229 = OpLoad %6 %8\n"
		"%230 = OpFSub %6 %28 %229\n"
		"%231 = OpLoad %6 %17\n"
		"%232 = OpFMul %6 %230 %231\n"
		"%233 = OpAccessChain %40 %225 %48 %39\n"
		"%234 = OpLoad %25 %233\n"
		"%235 = OpVectorTimesScalar %25 %234 %232\n"
		"%236 = OpFAdd %25 %228 %235\n"
		"%237 = OpLoad %6 %8\n"
		"%238 = OpLoad %6 %17\n"
		"%239 = OpFSub %6 %28 %238\n"
		"%240 = OpFMul %6 %237 %239\n"
		"%241 = OpAccessChain %40 %225 %57 %39\n"
		"%242 = OpLoad %25 %241\n"
		"%243 = OpVectorTimesScalar %25 %242 %240\n"
		"%244 = OpFAdd %25 %236 %243\n"
		"%245 = OpLoad %6 %8\n"
		"%246 = OpLoad %6 %17\n"
		"%247 = OpFMul %6 %245 %246\n"
		"%248 = OpAccessChain %40 %225 %65 %39\n"
		"%249 = OpLoad %25 %248\n"
		"%250 = OpVectorTimesScalar %25 %249 %247\n"
		"%251 = OpFAdd %25 %244 %250\n"
		"%252 = OpAccessChain %26 %216 %39\n"
		"OpStore %252 %251\n"
		"OpReturn\n"
		"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("tessellation_control") << tessellationControlSource;
		programCollection.spirvAsmSources.add("tessellation_evaluation") << tessellationEvaluationSource;
	}
	{

		/*#version 450
		layout(triangles) in;
		layout(triangle_strip, max_vertices = 3) out;
		layout(location = 0) in vec4    in_color[];
		layout(location = 1) in float   r_float_in[];
		layout(location = 2) in vec2    rg_float_in[];
		layout(location = 3) in vec3    rgb_float_in[];
		layout(location = 4) in vec4    rgba_float_in[];
		layout(location = 0) out vec4   out_color;
		layout(location = 1) out float  r_float_out;
		layout(location = 2) out vec2   rg_float_out;
		layout(location = 3) out vec3   rgb_float_out;
		layout(location = 4) out vec4   rgba_float_out;
		void main (void)
		{
			out_color = in_color[0];
			r_float_out = r_float_in[0];
			rg_float_out = rg_float_in[0];
			rgb_float_out = rgb_float_in[0];
			rgba_float_out = rgba_float_in[0];
			gl_Position = gl_in[0].gl_Position;
			EmitVertex();
			out_color = in_color[1];
			r_float_out = r_float_in[1];
			rg_float_out = rg_float_in[1];
			rgb_float_out = rgb_float_in[1];
			rgba_float_out = rgba_float_in[1];
			gl_Position = gl_in[1].gl_Position;
			EmitVertex();
			out_color = in_color[2];
			r_float_out = r_float_in[2];
			rg_float_out = rg_float_in[2];
			rgb_float_out = rgb_float_in[2];
			rgba_float_out = rgba_float_in[2];
			gl_Position = gl_in[2].gl_Position;
			EmitVertex();
			EndPrimitive();
		}
		*/
		const string geometrySource =
		"; SPIR-V\n"
		"; Version: 1.3\n"
		"; Generator: Khronos Glslang Reference Front End; 2\n"
		"; Bound: 90\n"
		"; Schema: 0\n"
		"OpCapability Geometry\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Geometry %4 \"main\" %color_out %color_in %r_float_out %r_float_in %rg_float_out %rg_float_in %rgb_float_out %rgb_float_in %rgba_float_out %rgba_float_in %54 %58\n"
		"OpExecutionMode %4 Triangles\n"
		"OpExecutionMode %4 Invocations 1\n"
		"OpExecutionMode %4 OutputTriangleStrip\n"
		"OpExecutionMode %4 OutputVertices 3\n"
		"OpDecorate %color_out Location 0\n"
		"OpDecorate %color_in Location 0\n"
		"OpDecorate %r_float_out Location 1\n"
		"OpDecorate %r_float_in Location 1\n"
		"OpDecorate %rg_float_out Location 2\n"
		"OpDecorate %rg_float_in Location 2\n"
		"OpDecorate %rgb_float_out Location 3\n"
		"OpDecorate %rgb_float_in Location 3\n"
		"OpDecorate %rgba_float_out Location 4\n"
		"OpDecorate %rgba_float_in Location 4\n"
		+decorations[0].others+
		"OpMemberDecorate %52 0 BuiltIn Position\n"
		"OpMemberDecorate %52 1 BuiltIn PointSize\n"
		"OpMemberDecorate %52 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %52 3 BuiltIn CullDistance\n"
		"OpDecorate %52 Block\n"
		"OpMemberDecorate %55 0 BuiltIn Position\n"
		"OpMemberDecorate %55 1 BuiltIn PointSize\n"
		"OpMemberDecorate %55 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %55 3 BuiltIn CullDistance\n"
		"OpDecorate %55 Block\n"
		"%2 = OpTypeVoid\n"
		"%3 = OpTypeFunction %2\n"
		"%6 = OpTypeFloat 32\n"
		"%7 = OpTypeVector %6 4\n"
		"%8 = OpTypePointer Output %7\n"
		"%color_out = OpVariable %8 Output\n"
		"%10 = OpTypeInt 32 0\n"
		"%11 = OpConstant %10 3\n"
		"%12 = OpTypeArray %7 %11\n"
		"%13 = OpTypePointer Input %12\n"
		"%color_in = OpVariable %13 Input\n"
		"%15 = OpTypeInt 32 1\n"
		"%16 = OpConstant %15 0\n"
		"%17 = OpTypePointer Input %7\n"
		"%20 = OpTypePointer Output %6\n"
		"%r_float_out = OpVariable %20 Output\n"
		"%22 = OpTypeArray %6 %11\n"
		"%23 = OpTypePointer Input %22\n"
		"%r_float_in = OpVariable %23 Input\n"
		"%25 = OpTypePointer Input %6\n"
		"%28 = OpTypeVector %6 2\n"
		"%29 = OpTypePointer Output %28\n"
		"%rg_float_out = OpVariable %29 Output\n"
		"%31 = OpTypeArray %28 %11\n"
		"%32 = OpTypePointer Input %31\n"
		"%rg_float_in = OpVariable %32 Input\n"
		"%34 = OpTypePointer Input %28\n"
		"%37 = OpTypeVector %6 3\n"
		"%38 = OpTypePointer Output %37\n"
		"%rgb_float_out = OpVariable %38 Output\n"
		"%40 = OpTypeArray %37 %11\n"
		"%41 = OpTypePointer Input %40\n"
		"%rgb_float_in = OpVariable %41 Input\n"
		"%43 = OpTypePointer Input %37\n"
		"%rgba_float_out = OpVariable %8 Output\n"
		"%rgba_float_in = OpVariable %13 Input\n"
		"%50 = OpConstant %10 1\n"
		"%51 = OpTypeArray %6 %50\n"
		"%52 = OpTypeStruct %7 %6 %51 %51\n"
		"%53 = OpTypePointer Output %52\n"
		"%54 = OpVariable %53 Output\n"
		"%55 = OpTypeStruct %7 %6 %51 %51\n"
		"%56 = OpTypeArray %55 %11\n"
		"%57 = OpTypePointer Input %56\n"
		"%58 = OpVariable %57 Input\n"
		"%62 = OpConstant %15 1\n"
		"%76 = OpConstant %15 2\n"
		"%4 = OpFunction %2 None %3\n"
		"%5 = OpLabel\n"
		"%18 = OpAccessChain %17 %color_in %16\n"
		"%19 = OpLoad %7 %18\n"
		"OpStore %color_out %19\n"
		"%26 = OpAccessChain %25 %r_float_in %16\n"
		"%27 = OpLoad %6 %26\n"
		"OpStore %r_float_out %27\n"
		"%35 = OpAccessChain %34 %rg_float_in %16\n"
		"%36 = OpLoad %28 %35\n"
		"OpStore %rg_float_out %36\n"
		"%44 = OpAccessChain %43 %rgb_float_in %16\n"
		"%45 = OpLoad %37 %44\n"
		"OpStore %rgb_float_out %45\n"
		"%48 = OpAccessChain %17 %rgba_float_in %16\n"
		"%49 = OpLoad %7 %48\n"
		"OpStore %rgba_float_out %49\n"
		"%59 = OpAccessChain %17 %58 %16 %16\n"
		"%60 = OpLoad %7 %59\n"
		"%61 = OpAccessChain %8 %54 %16\n"
		"OpStore %61 %60\n"
		"OpEmitVertex\n"
		"%63 = OpAccessChain %17 %color_in %62\n"
		"%64 = OpLoad %7 %63\n"
		"OpStore %color_out %64\n"
		"%65 = OpAccessChain %25 %r_float_in %62\n"
		"%66 = OpLoad %6 %65\n"
		"OpStore %r_float_out %66\n"
		"%67 = OpAccessChain %34 %rg_float_in %62\n"
		"%68 = OpLoad %28 %67\n"
		"OpStore %rg_float_out %68\n"
		"%69 = OpAccessChain %43 %rgb_float_in %62\n"
		"%70 = OpLoad %37 %69\n"
		"OpStore %rgb_float_out %70\n"
		"%71 = OpAccessChain %17 %rgba_float_in %62\n"
		"%72 = OpLoad %7 %71\n"
		"OpStore %rgba_float_out %72\n"
		"%73 = OpAccessChain %17 %58 %62 %16\n"
		"%74 = OpLoad %7 %73\n"
		"%75 = OpAccessChain %8 %54 %16\n"
		"OpStore %75 %74\n"
		"OpEmitVertex\n"
		"%77 = OpAccessChain %17 %color_in %76\n"
		"%78 = OpLoad %7 %77\n"
		"OpStore %color_out %78\n"
		"%79 = OpAccessChain %25 %r_float_in %76\n"
		"%80 = OpLoad %6 %79\n"
		"OpStore %r_float_out %80\n"
		"%81 = OpAccessChain %34 %rg_float_in %76\n"
		"%82 = OpLoad %28 %81\n"
		"OpStore %rg_float_out %82\n"
		"%83 = OpAccessChain %43 %rgb_float_in %76\n"
		"%84 = OpLoad %37 %83\n"
		"OpStore %rgb_float_out %84\n"
		"%85 = OpAccessChain %17 %rgba_float_in %76\n"
		"%86 = OpLoad %7 %85\n"
		"OpStore %rgba_float_out %86\n"
		"%87 = OpAccessChain %17 %58 %76 %16\n"
		"%88 = OpLoad %7 %87\n"
		"%89 = OpAccessChain %8 %54 %16\n"
		"OpStore %89 %88\n"
		"OpEmitVertex\n"
		"OpEndPrimitive\n"
		"OpReturn\n"
		"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("geometry") << geometrySource;
	}
}

class CrossStageInterfaceTestsCase : public vkt::TestCase
{
public:
	CrossStageInterfaceTestsCase (tcu::TestContext &context, const char *name, const char *description, const TestParameters& parameters)
		: TestCase			(context, name, description)
		, m_parameters		(parameters)
	{
	}
private:
	vkt::TestInstance*	createInstance		(vkt::Context& context) const;
	void				initPrograms		(SourceCollections& programCollection) const;

	const TestParameters	m_parameters;

};

vkt::TestInstance* CrossStageInterfaceTestsCase::createInstance (vkt::Context& context) const
{
	return new CrossStageTestInstance(context, m_parameters);
}

void CrossStageInterfaceTestsCase::initPrograms (SourceCollections& programCollection) const
{
	vector<Decorations> decorations;
	string epsilon = "3e-7";
	switch(m_parameters.qualifier)
	{
	case TEST_TYPE_FLAT:
		decorations.push_back(Decorations("",
								//Vertex
								"OpDecorate %color_out Flat\n"
								"OpDecorate %color_in Flat\n"
								"OpMemberDecorate %block_out 0 Flat\n"
								"OpMemberDecorate %block_out 1 Flat\n",
								""));
		decorations.push_back(Decorations(//Fragment
								"OpDecorate %color_in Flat\n"
								"OpMemberDecorate %block_in 0 Flat\n"
								"OpMemberDecorate %block_in 1 Flat\n",
								"",
								""));

		decorations.push_back(Decorations(//Fragment
								"OpDecorate %color_in Flat\n"
								"OpMemberDecorate %block_in 0 Flat\n"
								"OpMemberDecorate %block_in 1 Flat\n",
								//Vertex
								"OpDecorate %color_out Flat\n"
								"OpDecorate %color_in Flat\n"
								"OpMemberDecorate %block_out 0 Flat\n"
								"OpMemberDecorate %block_out 1 Flat\n",
								""));
		epsilon = "0.0";
		break;
	case TEST_TYPE_NOPERSPECTIVE:
		decorations.push_back(Decorations("",
								//Vertex
								"OpDecorate %color_out NoPerspective\n"
								"OpDecorate %color_in NoPerspective\n"
								"OpMemberDecorate %block_out 0 NoPerspective\n"
								"OpMemberDecorate %block_out 1 NoPerspective\n",
								""));

		decorations.push_back(Decorations(//Fragment
								"OpDecorate %color_in NoPerspective\n"
								"OpMemberDecorate %block_in 0 NoPerspective\n"
								"OpMemberDecorate %block_in 1 NoPerspective\n",
								"",
								""));

		decorations.push_back(Decorations(//Fragment
								"OpDecorate %color_in NoPerspective\n"
								"OpMemberDecorate %block_in 0 NoPerspective\n"
								"OpMemberDecorate %block_in 1 NoPerspective\n",
								//Vertex
								"OpDecorate %color_out NoPerspective\n"
								"OpDecorate %color_in NoPerspective\n"
								"OpMemberDecorate %block_out 0 NoPerspective\n"
								"OpMemberDecorate %block_out 1 NoPerspective\n",
								""));
		break;
	case TEST_TYPE_RELAXEDPRECISION:
		decorations.push_back(Decorations(//Fragment
								"OpDecorate %color_in RelaxedPrecision\n"
								"OpDecorate %color_out RelaxedPrecision\n"
								"OpMemberDecorate %block_in 0 RelaxedPrecision\n"
								"OpMemberDecorate %block_in 1 RelaxedPrecision\n",
								//Vertex
								"OpDecorate %color_out RelaxedPrecision\n"
								"OpDecorate %color_in RelaxedPrecision\n"
								"OpMemberDecorate %block_out 0 RelaxedPrecision\n"
								"OpMemberDecorate %block_out 1 RelaxedPrecision\n",
								//Others
								"OpDecorate %color_out RelaxedPrecision\n"
								"OpDecorate %color_in RelaxedPrecision\n"
								"OpMemberDecorate %block_out 0 RelaxedPrecision\n"
								"OpMemberDecorate %block_out 1 RelaxedPrecision\n"
								"OpMemberDecorate %block_in 0 RelaxedPrecision\n"
								"OpMemberDecorate %block_in 1 RelaxedPrecision\n"));
		epsilon = "2e-3";
		break;
	default:
		DE_ASSERT(0);
	}

	//Spir-v spec: decoration flat can be used only in Shader (fragment or vertex)
	for (deUint32 ndx = 0; ndx < decorations.size(); ++ndx)
	{

		/*#version 450
		layout(location = 0) in highp vec4 in_position;
		layout(location = 1) in vec4 in_color;
		layout(location = 0) out vec4 out_color;
		layout(location = 1) out ColorData
		{
		  vec4 colorVec;
		  mat2 colorMat;
		} outData;
		void main (void)
		{
		  gl_Position = in_position;
		  out_color = in_color;
		  outData.colorVec = in_color;
		  outData.colorMat = mat2(in_color.r, in_color.g, in_color.b, in_color.a);
		}
		*/
		const string vertexShaderSource =
		"; SPIR-V\n"
		"; Version: 1.3\n"
		"; Generator: Khronos Glslang Reference Front End; 2\n"
		"; Bound: 51\n"
		"; Schema: 0\n"
		"OpCapability Shader\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Vertex %4 \"main\" %13 %17 %color_out %color_in %28\n"
		"OpMemberDecorate %11 0 BuiltIn Position\n"
		"OpMemberDecorate %11 1 BuiltIn PointSize\n"
		"OpMemberDecorate %11 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %11 3 BuiltIn CullDistance\n"
		"OpDecorate %11 Block\n"
		"OpDecorate %17 Location 0\n"
		"OpDecorate %color_out Location 0\n"
		"OpDecorate %color_in Location 1\n"
		"OpDecorate %block_out Block\n"
		"OpDecorate %28 Location 1\n"
		+decorations[ndx].vertex+
		"%2 = OpTypeVoid\n"
		"%3 = OpTypeFunction %2\n"
		"%6 = OpTypeFloat 32\n"
		"%7 = OpTypeVector %6 4\n"
		"%8 = OpTypeInt 32 0\n"
		"%9 = OpConstant %8 1\n"
		"%10 = OpTypeArray %6 %9\n"
		"%11 = OpTypeStruct %7 %6 %10 %10\n"
		"%12 = OpTypePointer Output %11\n"
		"%13 = OpVariable %12 Output\n"
		"%14 = OpTypeInt 32 1\n"
		"%15 = OpConstant %14 0\n"
		"%16 = OpTypePointer Input %7\n"
		"%17 = OpVariable %16 Input\n"
		"%19 = OpTypePointer Output %7\n"
		"%color_out = OpVariable %19 Output\n"
		"%color_in = OpVariable %16 Input\n"
		"%24 = OpTypeVector %6 2\n"
		"%25 = OpTypeMatrix %24 2\n"
		"%block_out = OpTypeStruct %7 %25\n"
		"%27 = OpTypePointer Output %block_out\n"
		"%28 = OpVariable %27 Output\n"
		"%31 = OpConstant %14 1\n"
		"%32 = OpConstant %8 0\n"
		"%33 = OpTypePointer Input %6\n"
		"%38 = OpConstant %8 2\n"
		"%41 = OpConstant %8 3\n"
		"%44 = OpConstant %6 1\n"
		"%45 = OpConstant %6 0\n"
		"%49 = OpTypePointer Output %25\n"
		"%4 = OpFunction %2 None %3\n"
		"%5 = OpLabel\n"
		"%18 = OpLoad %7 %17\n"
		"%20 = OpAccessChain %19 %13 %15\n"
		"OpStore %20 %18\n"
		"%23 = OpLoad %7 %color_in\n"
		"OpStore %color_out %23\n"
		"%29 = OpLoad %7 %color_in\n"
		"%30 = OpAccessChain %19 %28 %15\n"
		"OpStore %30 %29\n"
		"%34 = OpAccessChain %33 %color_in %32\n"
		"%35 = OpLoad %6 %34\n"
		"%36 = OpAccessChain %33 %color_in %9\n"
		"%37 = OpLoad %6 %36\n"
		"%39 = OpAccessChain %33 %color_in %38\n"
		"%40 = OpLoad %6 %39\n"
		"%42 = OpAccessChain %33 %color_in %41\n"
		"%43 = OpLoad %6 %42\n"
		"%46 = OpCompositeConstruct %24 %35 %37\n"
		"%47 = OpCompositeConstruct %24 %40 %43\n"
		"%48 = OpCompositeConstruct %25 %46 %47\n"
		"%50 = OpAccessChain %49 %28 %31\n"
		"OpStore %50 %48\n"
		"OpReturn\n"
		"OpFunctionEnd\n";

		/* #version 450
		layout(location = 0) in vec4 in_color;
		layout(location = 0) out vec4 out_color;
		layout(location = 1) in ColorData
		{
		  vec4 colorVec;
		  mat2 colorMat;
		} inData;
		void main()
		{
		  float epsilon = 3e-7; // or 0.0 for flat, or 2e-3 for RelaxedPrecision (mediump)
		  out_color = in_color;
		  vec4 epsilon_vec4 = max(abs(inData.colorVec), abs(in_color)) * epsilon;
		  if(any(greaterThan(abs(inData.colorVec - in_color), epsilon_vec4)))
		    out_color.rgba = vec4(1.0f);
		  epsilon_vec4.r = max(abs(inData.colorMat[0][0]), abs(in_color.r)) * epsilon;
		  if(abs(inData.colorMat[0][0] - in_color.r) > epsilon_vec4.r)
		    out_color.rgba = vec4(1.0f);
		  epsilon_vec4.a = max(abs(inData.colorMat[1][1]), abs(in_color.a)) * epsilon;
		  if(abs(inData.colorMat[1][1] - in_color.a) > epsilon_vec4.a)
		    out_color.rgba = vec4(1.0f);
		}
		*/
		const string fragmentShaderSource =
		"; SPIR-V\n"
		"; Version: 1.3\n"
		"; Generator: Khronos Glslang Reference Front End; 2\n"
		"; Bound: 51\n"
		"; Schema: 0\n"
		"OpCapability Shader\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Fragment %4 \"main\" %color_out %color_in %17\n"
		"OpExecutionMode %4 OriginUpperLeft\n"
		"OpDecorate %color_out Location 0\n"
		"OpDecorate %color_in Location 0\n"
		"OpDecorate %block_in Block\n"
		"OpDecorate %17 Location 1\n"
		+decorations[ndx].fragment+
		"%2 = OpTypeVoid\n"
		"%3 = OpTypeFunction %2\n"
		"%6 = OpTypeFloat 32\n"
		"%7 = OpTypeVector %6 4\n"
		"%8 = OpTypePointer Output %7\n"
		"%color_out = OpVariable %8 Output\n"
		"%10 = OpTypePointer Input %7\n"
		"%color_in = OpVariable %10 Input\n"
		"%13 = OpTypeVector %6 2\n"
		"%14 = OpTypeMatrix %13 2\n"
		"%block_in = OpTypeStruct %7 %14\n"
		"%16 = OpTypePointer Input %block_in\n"
		"%17 = OpVariable %16 Input\n"
		"%18 = OpTypeInt 32 1\n"
		"%19 = OpConstant %18 0\n"
		"%23 = OpTypeBool\n"
		"%24 = OpTypeVector %23 4\n"
		"%ep = OpConstant %6 " + epsilon + "\n"
		"%ep4 = OpConstantComposite %7 %ep %ep %ep %ep\n"
		"%29 = OpConstant %6 1\n"
		"%30 = OpConstantComposite %7 %29 %29 %29 %29\n"
		"%31 = OpConstant %18 1\n"
		"%32 = OpTypeInt 32 0\n"
		"%33 = OpConstant %32 0\n"
		"%34 = OpTypePointer Input %6\n"
		"%42 = OpConstant %32 1\n"
		"%45 = OpConstant %32 3\n"
		"%4 = OpFunction %2 None %3\n"
		"%5 = OpLabel\n"
		"%12 = OpLoad %7 %color_in\n"
		"OpStore %color_out %12\n"
		"%20 = OpAccessChain %10 %17 %19\n"
		"%21 = OpLoad %7 %20\n"
		"%22 = OpLoad %7 %color_in\n"
		"%sub4 = OpFSub %7 %21 %22\n"
		"%abs4 = OpExtInst %7 %1 FAbs %sub4\n"
		"%ep4abs0 = OpExtInst %7 %1 FAbs %21\n"
		"%ep4abs1 = OpExtInst %7 %1 FAbs %22\n"
		"%ep4gt = OpFOrdGreaterThan %24 %ep4abs0 %ep4abs1\n"
		"%ep4max = OpSelect %7 %ep4gt %ep4abs0 %ep4abs1\n"
		"%ep4rel = OpFMul %7 %ep4max %ep4\n"
		"%cmp4 = OpFOrdGreaterThan %24 %abs4 %ep4rel\n"
		"%26 = OpAny %23 %cmp4\n"
		"OpSelectionMerge %28 None\n"
		"OpBranchConditional %26 %27 %28\n"
		"%27 = OpLabel\n"
		"OpStore %color_out %30\n"
		"OpBranch %28\n"
		"%28 = OpLabel\n"
		"%35 = OpAccessChain %34 %17 %31 %19 %33\n"
		"%36 = OpLoad %6 %35\n"
		"%37 = OpAccessChain %34 %color_in %33\n"
		"%38 = OpLoad %6 %37\n"
		"%subr = OpFSub %6 %36 %38\n"
		"%absr = OpExtInst %6 %1 FAbs %subr\n"
		"%ep1abs0 = OpExtInst %6 %1 FAbs %36\n"
		"%ep1abs1 = OpExtInst %6 %1 FAbs %38\n"
		"%ep1gt = OpFOrdGreaterThan %23 %ep1abs0 %ep1abs1\n"
		"%ep1max = OpSelect %6 %ep1gt %ep1abs0 %ep1abs1\n"
		"%ep1rel = OpFMul %6 %ep1max %ep\n"
		"%cmpr = OpFOrdGreaterThan %23 %absr %ep1rel\n"
		"OpSelectionMerge %41 None\n"
		"OpBranchConditional %cmpr %40 %41\n"
		"%40 = OpLabel\n"
		"OpStore %color_out %30\n"
		"OpBranch %41\n"
		"%41 = OpLabel\n"
		"%43 = OpAccessChain %34 %17 %31 %31 %42\n"
		"%44 = OpLoad %6 %43\n"
		"%46 = OpAccessChain %34 %color_in %45\n"
		"%47 = OpLoad %6 %46\n"
		"%suba = OpFSub %6 %44 %47\n"
		"%absa = OpExtInst %6 %1 FAbs %suba\n"
		"%ep1babs0 = OpExtInst %6 %1 FAbs %44\n"
		"%ep1babs1 = OpExtInst %6 %1 FAbs %47\n"
		"%ep1bgt = OpFOrdGreaterThan %23 %ep1babs0 %ep1babs1\n"
		"%ep1bmax = OpSelect %6 %ep1bgt %ep1babs0 %ep1babs1\n"
		"%ep1brel = OpFMul %6 %ep1bmax %ep\n"
		"%cmpa = OpFOrdGreaterThan %23 %absa %ep1brel\n"
		"OpSelectionMerge %50 None\n"
		"OpBranchConditional %cmpa %49 %50\n"
		"%49 = OpLabel\n"
		"OpStore %color_out %30\n"
		"OpBranch %50\n"
		"%50 = OpLabel\n"
		"OpReturn\n"
		"OpFunctionEnd\n";

		std::ostringstream vertex;
		vertex << "vertex" << ndx;
		std::ostringstream fragment;
		fragment << "fragment" << ndx;

		programCollection.spirvAsmSources.add(vertex.str()) << vertexShaderSource;
		programCollection.spirvAsmSources.add(fragment.str()) << fragmentShaderSource;
	}
	{
		/*#version 450
		#extension GL_EXT_tessellation_shader : require
		layout(vertices = 4) out;
		layout(location = 0) in vec4		in_color[];
		layout(location = 1) in ColorData
		{
		  vec4 colorVec;
		  mat2 colorMat;
		} inData[];
		layout(location = 0) out vec4		out_color[];
		layout(location = 1) out ColorData
		{
		  vec4 colorVec;
		  mat2 colorMat;
		} outData[];
		void main (void)
		{
			if ( gl_InvocationID == 0 )
			{
				gl_TessLevelInner[0] = 4.0f;
				gl_TessLevelInner[1] = 4.0f;
				gl_TessLevelOuter[0] = 4.0f;
				gl_TessLevelOuter[1] = 4.0f;
				gl_TessLevelOuter[2] = 4.0f;
				gl_TessLevelOuter[3] = 4.0f;
			}
			out_color[gl_InvocationID] = in_color[gl_InvocationID];
			outData[gl_InvocationID].colorVec = inData[gl_InvocationID].colorVec;
			outData[gl_InvocationID].colorMat = inData[gl_InvocationID].colorMat;
			gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
		}*/

		const string tessellationControlSource =
		"; SPIR-V\n"
		"; Version: 1.3\n"
		"; Generator: Khronos Glslang Reference Front End; 2\n"
		"; Bound: 88\n"
		"; Schema: 0\n"
		"OpCapability Tessellation\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint TessellationControl %4 \"main\" %8 %20 %29 %color_out %color_in %56 %61 %78 %83\n"
		"OpExecutionMode %4 OutputVertices 4\n"
		"OpDecorate %8 BuiltIn InvocationId\n"
		"OpDecorate %20 Patch\n"
		"OpDecorate %20 BuiltIn TessLevelInner\n"
		"OpDecorate %29 Patch\n"
		"OpDecorate %29 BuiltIn TessLevelOuter\n"
		"OpDecorate %color_out Location 0\n"
		"OpDecorate %color_in Location 0\n"
		"OpDecorate %block_out Block\n"
		"OpDecorate %56 Location 1\n"
		"OpDecorate %block_in Block\n"
		"OpDecorate %61 Location 1\n"
		+decorations[0].others+
		"OpMemberDecorate %75 0 BuiltIn Position\n"
		"OpMemberDecorate %75 1 BuiltIn PointSize\n"
		"OpMemberDecorate %75 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %75 3 BuiltIn CullDistance\n"
		"OpDecorate %75 Block\n"
		"OpMemberDecorate %80 0 BuiltIn Position\n"
		"OpMemberDecorate %80 1 BuiltIn PointSize\n"
		"OpMemberDecorate %80 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %80 3 BuiltIn CullDistance\n"
		"OpDecorate %80 Block\n"
		"%2 = OpTypeVoid\n"
		"%3 = OpTypeFunction %2\n"
		"%6 = OpTypeInt 32 1\n"
		"%7 = OpTypePointer Input %6\n"
		"%8 = OpVariable %7 Input\n"
		"%10 = OpConstant %6 0\n"
		"%11 = OpTypeBool\n"
		"%15 = OpTypeFloat 32\n"
		"%16 = OpTypeInt 32 0\n"
		"%17 = OpConstant %16 2\n"
		"%18 = OpTypeArray %15 %17\n"
		"%19 = OpTypePointer Output %18\n"
		"%20 = OpVariable %19 Output\n"
		"%21 = OpConstant %15 4\n"
		"%22 = OpTypePointer Output %15\n"
		"%24 = OpConstant %6 1\n"
		"%26 = OpConstant %16 4\n"
		"%27 = OpTypeArray %15 %26\n"
		"%28 = OpTypePointer Output %27\n"
		"%29 = OpVariable %28 Output\n"
		"%32 = OpConstant %6 2\n"
		"%34 = OpConstant %6 3\n"
		"%36 = OpTypeVector %15 4\n"
		"%37 = OpTypeArray %36 %26\n"
		"%38 = OpTypePointer Output %37\n"
		"%color_out = OpVariable %38 Output\n"
		"%41 = OpConstant %16 32\n"
		"%42 = OpTypeArray %36 %41\n"
		"%43 = OpTypePointer Input %42\n"
		"%color_in = OpVariable %43 Input\n"
		"%46 = OpTypePointer Input %36\n"
		"%49 = OpTypePointer Output %36\n"
		"%51 = OpTypeVector %15 2\n"
		"%52 = OpTypeMatrix %51 2\n"
		"%block_out = OpTypeStruct %36 %52\n"
		"%54 = OpTypeArray %block_out %26\n"
		"%55 = OpTypePointer Output %54\n"
		"%56 = OpVariable %55 Output\n"
		"%block_in = OpTypeStruct %36 %52\n"
		"%59 = OpTypeArray %block_in %41\n"
		"%60 = OpTypePointer Input %59\n"
		"%61 = OpVariable %60 Input\n"
		"%68 = OpTypePointer Input %52\n"
		"%71 = OpTypePointer Output %52\n"
		"%73 = OpConstant %16 1\n"
		"%74 = OpTypeArray %15 %73\n"
		"%75 = OpTypeStruct %36 %15 %74 %74\n"
		"%76 = OpTypeArray %75 %26\n"
		"%77 = OpTypePointer Output %76\n"
		"%78 = OpVariable %77 Output\n"
		"%80 = OpTypeStruct %36 %15 %74 %74\n"
		"%81 = OpTypeArray %80 %41\n"
		"%82 = OpTypePointer Input %81\n"
		"%83 = OpVariable %82 Input\n"
		"%4 = OpFunction %2 None %3\n"
		"%5 = OpLabel\n"
		"%9 = OpLoad %6 %8\n"
		"%12 = OpIEqual %11 %9 %10\n"
		"OpSelectionMerge %14 None\n"
		"OpBranchConditional %12 %13 %14\n"
		"%13 = OpLabel\n"
		"%23 = OpAccessChain %22 %20 %10\n"
		"OpStore %23 %21\n"
		"%25 = OpAccessChain %22 %20 %24\n"
		"OpStore %25 %21\n"
		"%30 = OpAccessChain %22 %29 %10\n"
		"OpStore %30 %21\n"
		"%31 = OpAccessChain %22 %29 %24\n"
		"OpStore %31 %21\n"
		"%33 = OpAccessChain %22 %29 %32\n"
		"OpStore %33 %21\n"
		"%35 = OpAccessChain %22 %29 %34\n"
		"OpStore %35 %21\n"
		"OpBranch %14\n"
		"%14 = OpLabel\n"
		"%40 = OpLoad %6 %8\n"
		"%45 = OpLoad %6 %8\n"
		"%47 = OpAccessChain %46 %color_in %45\n"
		"%48 = OpLoad %36 %47\n"
		"%50 = OpAccessChain %49 %color_out %40\n"
		"OpStore %50 %48\n"
		"%57 = OpLoad %6 %8\n"
		"%62 = OpLoad %6 %8\n"
		"%63 = OpAccessChain %46 %61 %62 %10\n"
		"%64 = OpLoad %36 %63\n"
		"%65 = OpAccessChain %49 %56 %57 %10\n"
		"OpStore %65 %64\n"
		"%66 = OpLoad %6 %8\n"
		"%67 = OpLoad %6 %8\n"
		"%69 = OpAccessChain %68 %61 %67 %24\n"
		"%70 = OpLoad %52 %69\n"
		"%72 = OpAccessChain %71 %56 %66 %24\n"
		"OpStore %72 %70\n"
		"%79 = OpLoad %6 %8\n"
		"%84 = OpLoad %6 %8\n"
		"%85 = OpAccessChain %46 %83 %84 %10\n"
		"%86 = OpLoad %36 %85\n"
		"%87 = OpAccessChain %49 %78 %79 %10\n"
		"OpStore %87 %86\n"
		"OpReturn\n"
		"OpFunctionEnd\n";

		/*#version 450
		#extension GL_EXT_tessellation_shader : require
		layout( quads, equal_spacing, ccw ) in;
		layout(location = 0) in vec4		in_color[];
		layout(location = 1) in ColorData
		{
		  vec4 colorVec;
		  mat2 colorMat;
		} inData[];
		layout(location = 0) out vec4		out_color;
		layout(location = 1) out ColorData
		{
		  vec4 colorVec;
		  mat2 colorMat;
		} outData;
		void main (void)
		{
			const float u = gl_TessCoord.x;
			const float v = gl_TessCoord.y;
			const float w = gl_TessCoord.z;
			out_color = (1 - u) * (1 - v) * in_color[0] +(1 - u) * v * in_color[1] + u * (1 - v) * in_color[2] + u * v * in_color[3];
			outData.colorVec = (1 - u) * (1 - v) * inData[0].colorVec +(1 - u) * v * inData[1].colorVec + u * (1 - v) * inData[2].colorVec + u * v * inData[3].colorVec;
			outData.colorMat = (1 - u) * (1 - v) * inData[0].colorMat +(1 - u) * v * inData[1].colorMat + u * (1 - v) * inData[2].colorMat + u * v * inData[3].colorMat;
			gl_Position = (1 - u) * (1 - v) * gl_in[0].gl_Position +(1 - u) * v * gl_in[1].gl_Position + u * (1 - v) * gl_in[2].gl_Position + u * v * gl_in[3].gl_Position;
		}*/

		const string tessellationEvaluationSource =
		"; SPIR-V\n"
		"; Version: 1.3\n"
		"; Generator: Khronos Glslang Reference Front End; 2\n"
		"; Bound: 203\n"
		"; Schema: 0\n"
		"OpCapability Tessellation\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint TessellationEvaluation %4 \"main\" %11 %color_out %color_in %74 %83 %166 %175\n"
		"OpExecutionMode %4 Quads\n"
		"OpExecutionMode %4 SpacingEqual\n"
		"OpExecutionMode %4 VertexOrderCcw\n"
		"OpDecorate %11 BuiltIn TessCoord\n"
		"OpDecorate %color_out Location 0\n"
		"OpDecorate %color_in Location 0\n"
		"OpDecorate %block_out Block\n"
		"OpDecorate %74 Location 1\n"
		"OpDecorate %block_in Block\n"
		"OpDecorate %83 Location 1\n"
		+decorations[0].others+
		"OpMemberDecorate %164 0 BuiltIn Position\n"
		"OpMemberDecorate %164 1 BuiltIn PointSize\n"
		"OpMemberDecorate %164 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %164 3 BuiltIn CullDistance\n"
		"OpDecorate %164 Block\n"
		"OpMemberDecorate %172 0 BuiltIn Position\n"
		"OpMemberDecorate %172 1 BuiltIn PointSize\n"
		"OpMemberDecorate %172 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %172 3 BuiltIn CullDistance\n"
		"OpDecorate %172 Block\n"
		"%2 = OpTypeVoid\n"
		"%3 = OpTypeFunction %2\n"
		"%6 = OpTypeFloat 32\n"
		"%7 = OpTypePointer Function %6\n"
		"%9 = OpTypeVector %6 3\n"
		"%10 = OpTypePointer Input %9\n"
		"%11 = OpVariable %10 Input\n"
		"%12 = OpTypeInt 32 0\n"
		"%13 = OpConstant %12 0\n"
		"%14 = OpTypePointer Input %6\n"
		"%18 = OpConstant %12 1\n"
		"%22 = OpConstant %12 2\n"
		"%25 = OpTypeVector %6 4\n"
		"%26 = OpTypePointer Output %25\n"
		"%color_out = OpVariable %26 Output\n"
		"%28 = OpConstant %6 1\n"
		"%34 = OpConstant %12 32\n"
		"%35 = OpTypeArray %25 %34\n"
		"%36 = OpTypePointer Input %35\n"
		"%color_in = OpVariable %36 Input\n"
		"%38 = OpTypeInt 32 1\n"
		"%39 = OpConstant %38 0\n"
		"%40 = OpTypePointer Input %25\n"
		"%48 = OpConstant %38 1\n"
		"%57 = OpConstant %38 2\n"
		"%65 = OpConstant %38 3\n"
		"%70 = OpTypeVector %6 2\n"
		"%71 = OpTypeMatrix %70 2\n"
		"%block_out = OpTypeStruct %25 %71\n"
		"%73 = OpTypePointer Output %block_out\n"
		"%74 = OpVariable %73 Output\n"
		"%block_in = OpTypeStruct %25 %71\n"
		"%81 = OpTypeArray %block_in %34\n"
		"%82 = OpTypePointer Input %81\n"
		"%83 = OpVariable %82 Input\n"
		"%116 = OpTypePointer Input %71\n"
		"%161 = OpTypePointer Output %71\n"
		"%163 = OpTypeArray %6 %18\n"
		"%164 = OpTypeStruct %25 %6 %163 %163\n"
		"%165 = OpTypePointer Output %164\n"
		"%166 = OpVariable %165 Output\n"
		"%172 = OpTypeStruct %25 %6 %163 %163\n"
		"%173 = OpTypeArray %172 %34\n"
		"%174 = OpTypePointer Input %173\n"
		"%175 = OpVariable %174 Input\n"
		"%4 = OpFunction %2 None %3\n"
		"%5 = OpLabel\n"
		"%8 = OpVariable %7 Function\n"
		"%17 = OpVariable %7 Function\n"
		"%21 = OpVariable %7 Function\n"
		"%15 = OpAccessChain %14 %11 %13\n"
		"%16 = OpLoad %6 %15\n"
		"OpStore %8 %16\n"
		"%19 = OpAccessChain %14 %11 %18\n"
		"%20 = OpLoad %6 %19\n"
		"OpStore %17 %20\n"
		"%23 = OpAccessChain %14 %11 %22\n"
		"%24 = OpLoad %6 %23\n"
		"OpStore %21 %24\n"
		"%29 = OpLoad %6 %8\n"
		"%30 = OpFSub %6 %28 %29\n"
		"%31 = OpLoad %6 %17\n"
		"%32 = OpFSub %6 %28 %31\n"
		"%33 = OpFMul %6 %30 %32\n"
		"%41 = OpAccessChain %40 %color_in %39\n"
		"%42 = OpLoad %25 %41\n"
		"%43 = OpVectorTimesScalar %25 %42 %33\n"
		"%44 = OpLoad %6 %8\n"
		"%45 = OpFSub %6 %28 %44\n"
		"%46 = OpLoad %6 %17\n"
		"%47 = OpFMul %6 %45 %46\n"
		"%49 = OpAccessChain %40 %color_in %48\n"
		"%50 = OpLoad %25 %49\n"
		"%51 = OpVectorTimesScalar %25 %50 %47\n"
		"%52 = OpFAdd %25 %43 %51\n"
		"%53 = OpLoad %6 %8\n"
		"%54 = OpLoad %6 %17\n"
		"%55 = OpFSub %6 %28 %54\n"
		"%56 = OpFMul %6 %53 %55\n"
		"%58 = OpAccessChain %40 %color_in %57\n"
		"%59 = OpLoad %25 %58\n"
		"%60 = OpVectorTimesScalar %25 %59 %56\n"
		"%61 = OpFAdd %25 %52 %60\n"
		"%62 = OpLoad %6 %8\n"
		"%63 = OpLoad %6 %17\n"
		"%64 = OpFMul %6 %62 %63\n"
		"%66 = OpAccessChain %40 %color_in %65\n"
		"%67 = OpLoad %25 %66\n"
		"%68 = OpVectorTimesScalar %25 %67 %64\n"
		"%69 = OpFAdd %25 %61 %68\n"
		"OpStore %color_out %69\n"
		"%75 = OpLoad %6 %8\n"
		"%76 = OpFSub %6 %28 %75\n"
		"%77 = OpLoad %6 %17\n"
		"%78 = OpFSub %6 %28 %77\n"
		"%79 = OpFMul %6 %76 %78\n"
		"%84 = OpAccessChain %40 %83 %39 %39\n"
		"%85 = OpLoad %25 %84\n"
		"%86 = OpVectorTimesScalar %25 %85 %79\n"
		"%87 = OpLoad %6 %8\n"
		"%88 = OpFSub %6 %28 %87\n"
		"%89 = OpLoad %6 %17\n"
		"%90 = OpFMul %6 %88 %89\n"
		"%91 = OpAccessChain %40 %83 %48 %39\n"
		"%92 = OpLoad %25 %91\n"
		"%93 = OpVectorTimesScalar %25 %92 %90\n"
		"%94 = OpFAdd %25 %86 %93\n"
		"%95 = OpLoad %6 %8\n"
		"%96 = OpLoad %6 %17\n"
		"%97 = OpFSub %6 %28 %96\n"
		"%98 = OpFMul %6 %95 %97\n"
		"%99 = OpAccessChain %40 %83 %57 %39\n"
		"%100 = OpLoad %25 %99\n"
		"%101 = OpVectorTimesScalar %25 %100 %98\n"
		"%102 = OpFAdd %25 %94 %101\n"
		"%103 = OpLoad %6 %8\n"
		"%104 = OpLoad %6 %17\n"
		"%105 = OpFMul %6 %103 %104\n"
		"%106 = OpAccessChain %40 %83 %65 %39\n"
		"%107 = OpLoad %25 %106\n"
		"%108 = OpVectorTimesScalar %25 %107 %105\n"
		"%109 = OpFAdd %25 %102 %108\n"
		"%110 = OpAccessChain %26 %74 %39\n"
		"OpStore %110 %109\n"
		"%111 = OpLoad %6 %8\n"
		"%112 = OpFSub %6 %28 %111\n"
		"%113 = OpLoad %6 %17\n"
		"%114 = OpFSub %6 %28 %113\n"
		"%115 = OpFMul %6 %112 %114\n"
		"%117 = OpAccessChain %116 %83 %39 %48\n"
		"%118 = OpLoad %71 %117\n"
		"%119 = OpMatrixTimesScalar %71 %118 %115\n"
		"%120 = OpLoad %6 %8\n"
		"%121 = OpFSub %6 %28 %120\n"
		"%122 = OpLoad %6 %17\n"
		"%123 = OpFMul %6 %121 %122\n"
		"%124 = OpAccessChain %116 %83 %48 %48\n"
		"%125 = OpLoad %71 %124\n"
		"%126 = OpMatrixTimesScalar %71 %125 %123\n"
		"%127 = OpCompositeExtract %70 %119 0\n"
		"%128 = OpCompositeExtract %70 %126 0\n"
		"%129 = OpFAdd %70 %127 %128\n"
		"%130 = OpCompositeExtract %70 %119 1\n"
		"%131 = OpCompositeExtract %70 %126 1\n"
		"%132 = OpFAdd %70 %130 %131\n"
		"%133 = OpCompositeConstruct %71 %129 %132\n"
		"%134 = OpLoad %6 %8\n"
		"%135 = OpLoad %6 %17\n"
		"%136 = OpFSub %6 %28 %135\n"
		"%137 = OpFMul %6 %134 %136\n"
		"%138 = OpAccessChain %116 %83 %57 %48\n"
		"%139 = OpLoad %71 %138\n"
		"%140 = OpMatrixTimesScalar %71 %139 %137\n"
		"%141 = OpCompositeExtract %70 %133 0\n"
		"%142 = OpCompositeExtract %70 %140 0\n"
		"%143 = OpFAdd %70 %141 %142\n"
		"%144 = OpCompositeExtract %70 %133 1\n"
		"%145 = OpCompositeExtract %70 %140 1\n"
		"%146 = OpFAdd %70 %144 %145\n"
		"%147 = OpCompositeConstruct %71 %143 %146\n"
		"%148 = OpLoad %6 %8\n"
		"%149 = OpLoad %6 %17\n"
		"%150 = OpFMul %6 %148 %149\n"
		"%151 = OpAccessChain %116 %83 %65 %48\n"
		"%152 = OpLoad %71 %151\n"
		"%153 = OpMatrixTimesScalar %71 %152 %150\n"
		"%154 = OpCompositeExtract %70 %147 0\n"
		"%155 = OpCompositeExtract %70 %153 0\n"
		"%156 = OpFAdd %70 %154 %155\n"
		"%157 = OpCompositeExtract %70 %147 1\n"
		"%158 = OpCompositeExtract %70 %153 1\n"
		"%159 = OpFAdd %70 %157 %158\n"
		"%160 = OpCompositeConstruct %71 %156 %159\n"
		"%162 = OpAccessChain %161 %74 %48\n"
		"OpStore %162 %160\n"
		"%167 = OpLoad %6 %8\n"
		"%168 = OpFSub %6 %28 %167\n"
		"%169 = OpLoad %6 %17\n"
		"%170 = OpFSub %6 %28 %169\n"
		"%171 = OpFMul %6 %168 %170\n"
		"%176 = OpAccessChain %40 %175 %39 %39\n"
		"%177 = OpLoad %25 %176\n"
		"%178 = OpVectorTimesScalar %25 %177 %171\n"
		"%179 = OpLoad %6 %8\n"
		"%180 = OpFSub %6 %28 %179\n"
		"%181 = OpLoad %6 %17\n"
		"%182 = OpFMul %6 %180 %181\n"
		"%183 = OpAccessChain %40 %175 %48 %39\n"
		"%184 = OpLoad %25 %183\n"
		"%185 = OpVectorTimesScalar %25 %184 %182\n"
		"%186 = OpFAdd %25 %178 %185\n"
		"%187 = OpLoad %6 %8\n"
		"%188 = OpLoad %6 %17\n"
		"%189 = OpFSub %6 %28 %188\n"
		"%190 = OpFMul %6 %187 %189\n"
		"%191 = OpAccessChain %40 %175 %57 %39\n"
		"%192 = OpLoad %25 %191\n"
		"%193 = OpVectorTimesScalar %25 %192 %190\n"
		"%194 = OpFAdd %25 %186 %193\n"
		"%195 = OpLoad %6 %8\n"
		"%196 = OpLoad %6 %17\n"
		"%197 = OpFMul %6 %195 %196\n"
		"%198 = OpAccessChain %40 %175 %65 %39\n"
		"%199 = OpLoad %25 %198\n"
		"%200 = OpVectorTimesScalar %25 %199 %197\n"
		"%201 = OpFAdd %25 %194 %200\n"
		"%202 = OpAccessChain %26 %166 %39\n"
		"OpStore %202 %201\n"
		"OpReturn\n"
		"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("tessellation_control") << tessellationControlSource;
		programCollection.spirvAsmSources.add("tessellation_evaluation") << tessellationEvaluationSource;
	}
	{

		/*#version 450
		layout(triangles) in;
		layout(triangle_strip, max_vertices = 3) out;
		layout(location = 0) in vec4		in_color[];
		layout(location = 1) in ColorData
		{
		  vec4 colorVec;
		  mat2 colorMat;
		} inData[];
		layout(location = 0) out vec4		out_color;
		layout(location = 1) out ColorData
		{
		  vec4 colorVec;
		  mat2 colorMat;
		} outData;
		void main (void)
		{
			out_color = in_color[0];
			outData.colorVec = inData[0].colorVec;
			outData.colorMat = inData[0].colorMat;
			gl_Position = gl_in[0].gl_Position;
			EmitVertex();
			out_color = in_color[1];
			outData.colorVec = inData[1].colorVec;
			outData.colorMat = inData[1].colorMat;
			gl_Position = gl_in[1].gl_Position;
			EmitVertex();
			out_color = in_color[2];
			outData.colorVec = inData[2].colorVec;
			outData.colorMat = inData[2].colorMat;
			gl_Position = gl_in[2].gl_Position;
			EmitVertex();
			EndPrimitive();
		}*/
		const string geometrySource =
		"; SPIR-V\n"
		"; Version: 1.3\n"
		"; Generator: Khronos Glslang Reference Front End; 2\n"
		"; Bound: 73\n"
		"; Schema: 0\n"
		"OpCapability Geometry\n"
		"%1 = OpExtInstImport \"GLSL.std.450\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint Geometry %4 \"main\" %color_out %color_in %24 %28 %42 %46\n"
		"OpExecutionMode %4 Triangles\n"
		"OpExecutionMode %4 Invocations 1\n"
		"OpExecutionMode %4 OutputTriangleStrip\n"
		"OpExecutionMode %4 OutputVertices 3\n"
		"OpDecorate %color_out Location 0\n"
		"OpDecorate %color_in Location 0\n"
		"OpDecorate %block_out Block\n"
		"OpDecorate %24 Location 1\n"
		"OpDecorate %block_in Block\n"
		"OpDecorate %28 Location 1\n"
		+decorations[0].others+
		"OpMemberDecorate %40 0 BuiltIn Position\n"
		"OpMemberDecorate %40 1 BuiltIn PointSize\n"
		"OpMemberDecorate %40 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %40 3 BuiltIn CullDistance\n"
		"OpDecorate %40 Block\n"
		"OpMemberDecorate %43 0 BuiltIn Position\n"
		"OpMemberDecorate %43 1 BuiltIn PointSize\n"
		"OpMemberDecorate %43 2 BuiltIn ClipDistance\n"
		"OpMemberDecorate %43 3 BuiltIn CullDistance\n"
		"OpDecorate %43 Block\n"
		"%2 = OpTypeVoid\n"
		"%3 = OpTypeFunction %2\n"
		"%6 = OpTypeFloat 32\n"
		"%7 = OpTypeVector %6 4\n"
		"%8 = OpTypePointer Output %7\n"
		"%color_out = OpVariable %8 Output\n"
		"%10 = OpTypeInt 32 0\n"
		"%11 = OpConstant %10 3\n"
		"%12 = OpTypeArray %7 %11\n"
		"%13 = OpTypePointer Input %12\n"
		"%color_in = OpVariable %13 Input\n"
		"%15 = OpTypeInt 32 1\n"
		"%16 = OpConstant %15 0\n"
		"%17 = OpTypePointer Input %7\n"
		"%20 = OpTypeVector %6 2\n"
		"%21 = OpTypeMatrix %20 2\n"
		"%block_out = OpTypeStruct %7 %21\n"
		"%23 = OpTypePointer Output %block_out\n"
		"%24 = OpVariable %23 Output\n"
		"%block_in = OpTypeStruct %7 %21\n"
		"%26 = OpTypeArray %block_in %11\n"
		"%27 = OpTypePointer Input %26\n"
		"%28 = OpVariable %27 Input\n"
		"%32 = OpConstant %15 1\n"
		"%33 = OpTypePointer Input %21\n"
		"%36 = OpTypePointer Output %21\n"
		"%38 = OpConstant %10 1\n"
		"%39 = OpTypeArray %6 %38\n"
		"%40 = OpTypeStruct %7 %6 %39 %39\n"
		"%41 = OpTypePointer Output %40\n"
		"%42 = OpVariable %41 Output\n"
		"%43 = OpTypeStruct %7 %6 %39 %39\n"
		"%44 = OpTypeArray %43 %11\n"
		"%45 = OpTypePointer Input %44\n"
		"%46 = OpVariable %45 Input\n"
		"%61 = OpConstant %15 2\n"
		"%4 = OpFunction %2 None %3\n"
		"%5 = OpLabel\n"
		"%18 = OpAccessChain %17 %color_in %16\n"
		"%19 = OpLoad %7 %18\n"
		"OpStore %color_out %19\n"
		"%29 = OpAccessChain %17 %28 %16 %16\n"
		"%30 = OpLoad %7 %29\n"
		"%31 = OpAccessChain %8 %24 %16\n"
		"OpStore %31 %30\n"
		"%34 = OpAccessChain %33 %28 %16 %32\n"
		"%35 = OpLoad %21 %34\n"
		"%37 = OpAccessChain %36 %24 %32\n"
		"OpStore %37 %35\n"
		"%47 = OpAccessChain %17 %46 %16 %16\n"
		"%48 = OpLoad %7 %47\n"
		"%49 = OpAccessChain %8 %42 %16\n"
		"OpStore %49 %48\n"
		"OpEmitVertex\n"
		"%50 = OpAccessChain %17 %color_in %32\n"
		"%51 = OpLoad %7 %50\n"
		"OpStore %color_out %51\n"
		"%52 = OpAccessChain %17 %28 %32 %16\n"
		"%53 = OpLoad %7 %52\n"
		"%54 = OpAccessChain %8 %24 %16\n"
		"OpStore %54 %53\n"
		"%55 = OpAccessChain %33 %28 %32 %32\n"
		"%56 = OpLoad %21 %55\n"
		"%57 = OpAccessChain %36 %24 %32\n"
		"OpStore %57 %56\n"
		"%58 = OpAccessChain %17 %46 %32 %16\n"
		"%59 = OpLoad %7 %58\n"
		"%60 = OpAccessChain %8 %42 %16\n"
		"OpStore %60 %59\n"
		"OpEmitVertex\n"
		"%62 = OpAccessChain %17 %color_in %61\n"
		"%63 = OpLoad %7 %62\n"
		"OpStore %color_out %63\n"
		"%64 = OpAccessChain %17 %28 %61 %16\n"
		"%65 = OpLoad %7 %64\n"
		"%66 = OpAccessChain %8 %24 %16\n"
		"OpStore %66 %65\n"
		"%67 = OpAccessChain %33 %28 %61 %32\n"
		"%68 = OpLoad %21 %67\n"
		"%69 = OpAccessChain %36 %24 %32\n"
		"OpStore %69 %68\n"
		"%70 = OpAccessChain %17 %46 %61 %16\n"
		"%71 = OpLoad %7 %70\n"
		"%72 = OpAccessChain %8 %42 %16\n"
		"OpStore %72 %71\n"
		"OpEmitVertex\n"
		"OpEndPrimitive\n"
		"OpReturn\n"
		"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("geometry") << geometrySource;
	}
}
} // anonymous

tcu::TestCaseGroup* createCrossStageInterfaceTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>		testGroup(new tcu::TestCaseGroup(testCtx, "cross_stage", ""));
	{
		de::MovePtr<tcu::TestCaseGroup>		basicGroup(new tcu::TestCaseGroup(testCtx, "basic_type", ""));
		de::MovePtr<tcu::TestCaseGroup>		interfaceGroup(new tcu::TestCaseGroup(testCtx, "interface_blocks", ""));
		{
			TestParameters parm(TEST_TYPE_FLAT,3);
			for (int ndx = 0; ndx < CrossStageTestInstance::DECORATION_LAST; ++ndx)
				parm.testOptions[ndx] = ndx;

			basicGroup->addChild(new CrossStageBasicTestsCase(testCtx, "flat", "", parm));
			interfaceGroup->addChild(new CrossStageInterfaceTestsCase(testCtx, "flat", "", parm));

			parm.qualifier = TEST_TYPE_NOPERSPECTIVE;
			basicGroup->addChild(new CrossStageBasicTestsCase(testCtx, "no_perspective", "", parm));
			interfaceGroup->addChild(new CrossStageInterfaceTestsCase(testCtx, "no_perspective", "", parm));
		}

		{
			TestParameters parm(TEST_TYPE_RELAXEDPRECISION,1);
			parm.testOptions[0] = CrossStageTestInstance::DECORATION_IN_ALL_SHADERS;
			basicGroup->addChild(new CrossStageBasicTestsCase(testCtx, "relaxedprecision", "", parm));
			interfaceGroup->addChild(new CrossStageInterfaceTestsCase(testCtx, "relaxedprecision", "", parm));
		}
		testGroup->addChild(basicGroup.release());
		testGroup->addChild(interfaceGroup.release());
	}

	return testGroup.release();
}

} // SpirVAssembly
} // vkt
