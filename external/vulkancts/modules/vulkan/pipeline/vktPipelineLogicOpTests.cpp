/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2020 Valve Corporation.
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
 * \file
 * \brief Logic Operators Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineLogicOpTests.hpp"
#include "vktPipelineImageUtil.hpp"

#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkImageWithMemory.hpp"

#include "tcuVectorUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"

#include <string>
#include <limits>

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{

bool isSupportedColorAttachmentFormat (const InstanceInterface& instanceInterface,
									   VkPhysicalDevice device,
									   VkFormat format)
{
	VkFormatProperties formatProps;
	instanceInterface.getPhysicalDeviceFormatProperties(device, format, &formatProps);

	// Format also needs to be INT, UINT, or SINT but as we are the ones setting the
	// color attachment format we only need to check that it is a valid color attachment
	// format here.
	return (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
}

struct TestParams
{
	VkLogicOp					logicOp;					// Operation.
	PipelineConstructionType	pipelineConstructionType;	// Use monolithic pipeline or pipeline_library
	tcu::UVec4					fbColor;					// Framebuffer color.
	tcu::UVec4					quadColor;					// Geometry color.
	VkFormat					format;						// Framebuffer format.
	std::string					name;						// Logic operator test name.
};

deUint32 calcOpResult(VkLogicOp op, deUint32 src, deUint32 dst)
{
	// See section 29.2 "Logical Operations" in the spec.
	//
	//	AND:			SRC & DST		= 1010 & 1100		= 1000 = 0x8
	//	AND_REVERSE:	SRC & ~DST		= 0011 & 1010		= 0010 = 0x2
	//	COPY:			SRC				= 1010				= 1010 = 0xa
	//	AND_INVERTED:	~SRC & DST		= 0101 & 1100		= 0100 = 0x4
	//	NO_OP:			DST				= 1010				= 1010 = 0xa
	//	XOR:			SRC ^ DST		= 1010 ^ 1100		= 0110 = 0x6
	//	OR:				SRC | DST		= 1010 | 1100		= 1110 = 0xe
	//	NOR:			~(SRC | DST)	= ~(1010 | 1100)	= 0001 = 0x1
	//	EQUIVALENT:		~(SRC ^ DST)	= ~(1010 ^ 1100)	= 1001 = 0x9
	//	INVERT:			~DST			= ~1100				= 0011 = 0x3
	//	OR_REVERSE:		SRC | ~DST		= 1010 | 0011		= 1011 = 0xb
	//	COPY_INVERTED:	~SRC			= 0101				= 0101 = 0x5
	//	OR_INVERTED:	~SRC | DST		= 0101 | 1100		= 1101 = 0xd
	//	NAND:			~(SRC & DST)	= ~(1010 &1100)		= 0111 = 0x7
	//	SET:							= 1111				= 1111 = 0xf (sets all bits)

	switch (op)
	{
	case VK_LOGIC_OP_CLEAR:				return (0u);
	case VK_LOGIC_OP_AND:				return (src & dst);
	case VK_LOGIC_OP_AND_REVERSE:		return (src & ~dst);
	case VK_LOGIC_OP_COPY:				return (src);
	case VK_LOGIC_OP_AND_INVERTED:		return (~src & dst);
	case VK_LOGIC_OP_NO_OP:				return (dst);
	case VK_LOGIC_OP_XOR:				return (src ^ dst);
	case VK_LOGIC_OP_OR:				return (src | dst);
	case VK_LOGIC_OP_NOR:				return (~(src | dst));
	case VK_LOGIC_OP_EQUIVALENT:		return (~(src ^ dst));
	case VK_LOGIC_OP_INVERT:			return (~dst);
	case VK_LOGIC_OP_OR_REVERSE:		return (src | ~dst);
	case VK_LOGIC_OP_COPY_INVERTED:		return (~src);
	case VK_LOGIC_OP_OR_INVERTED:		return (~src | dst);
	case VK_LOGIC_OP_NAND:				return (~(src & dst));
	case VK_LOGIC_OP_SET:				return (std::numeric_limits<deUint32>::max());
	default: DE_ASSERT(false); break;
	}

	DE_ASSERT(false);
	return 0u;
}

// Gets a bitmask to filter out unused bits according to the channel size (e.g. 0xFFu for 8-bit channels).
// channelSize in bytes.
deUint32 getChannelMask (int channelSize)
{
	DE_ASSERT(channelSize >= 1 && channelSize <= 4);

	deUint64 mask = 1u;
	mask <<= (channelSize * 8);
	--mask;

	return static_cast<deUint32>(mask);
}

class LogicOpTest : public vkt::TestCase
{
public:
									LogicOpTest			(tcu::TestContext&  testCtx,
														 const std::string& name,
														 const std::string& description,
														 const TestParams &testParams);
	virtual							~LogicOpTest		(void);
	virtual		  void				initPrograms		(SourceCollections& sourceCollections) const;
	virtual		  void				checkSupport		(Context& context) const;
	virtual		  TestInstance*		createInstance		(Context& context) const;

private:
	TestParams m_params;
};

LogicOpTest::LogicOpTest (tcu::TestContext& testCtx,
						  const std::string& name,
						  const std::string& description,
						  const TestParams& testParams)
	: vkt::TestCase	(testCtx, name, description)
	, m_params		(testParams)
{
	DE_ASSERT(m_params.format != VK_FORMAT_UNDEFINED);
}

LogicOpTest::~LogicOpTest (void)
{
}

void LogicOpTest::checkSupport (Context &ctx) const
{
	const auto& features = ctx.getDeviceFeatures();

	if (!features.logicOp)
		TCU_THROW(NotSupportedError, "Logic operations not supported");

	checkPipelineConstructionRequirements(ctx.getInstanceInterface(), ctx.getPhysicalDevice(), m_params.pipelineConstructionType);

	if (!isSupportedColorAttachmentFormat(ctx.getInstanceInterface(), ctx.getPhysicalDevice(), m_params.format))
		TCU_THROW(NotSupportedError, "Unsupported color attachment format: " + std::string(getFormatName(m_params.format)));
}

void LogicOpTest::initPrograms (SourceCollections& sourceCollections) const
{
	sourceCollections.glslSources.add("color_vert") << glu::VertexSource(
		"#version 430\n"
		"vec2 vdata[] = vec2[] (\n"
		"vec2(-1.0, -1.0),\n"
		"vec2(1.0, -1.0),\n"
		"vec2(-1.0, 1.0),\n"
		"vec2(1.0, 1.0));\n"
		"void main (void)\n"
		"{\n"
		"	gl_Position = vec4(vdata[gl_VertexIndex], 0.0, 1.0);\n"
		"}\n");

	sourceCollections.glslSources.add("color_frag") << glu::FragmentSource(
		"#version 430\n"
		"layout(push_constant) uniform quadColor {\n"
		"	uvec4 val;\n"
		"} QUAD_COLOR;\n"
		"layout(location = 0) out uvec4 fragColor;\n"
		"void main (void)\n"
		"{\n"
		"	fragColor = QUAD_COLOR.val;\n"
		"}\n");
}

class LogicOpTestInstance : public vkt::TestInstance
{
public:
										LogicOpTestInstance(Context& context,
															const TestParams& params);
										~LogicOpTestInstance(void);
	virtual		tcu::TestStatus			iterate(void);

private:
	tcu::TestStatus						verifyImage(void);

	TestParams							m_params;

	// Derived from m_params.
	const tcu::TextureFormat			m_tcuFormat;
	const int							m_numChannels;
	const int							m_channelSize;
	const deUint32						m_channelMask;

	const tcu::UVec2					m_renderSize;

	VkImageCreateInfo					m_colorImageCreateInfo;
	de::MovePtr<ImageWithMemory>		m_colorImage;
	Move<VkImageView>					m_colorAttachmentView;

	RenderPassWrapper					m_renderPass;
	Move<VkFramebuffer>					m_framebuffer;

	ShaderWrapper						m_vertexShaderModule;
	ShaderWrapper						m_fragmentShaderModule;

	PipelineLayoutWrapper				m_preRasterizationStatePipelineLayout;
	PipelineLayoutWrapper				m_fragmentStatePipelineLayout;
	GraphicsPipelineWrapper				m_graphicsPipeline;

	Move<VkCommandPool>					m_cmdPool;
	Move<VkCommandBuffer>				m_cmdBuffer;
};

LogicOpTestInstance::LogicOpTestInstance (Context &ctx, const TestParams &testParams)
	: vkt::TestInstance		(ctx)
	, m_params				(testParams)
	, m_tcuFormat			(mapVkFormat(m_params.format))
	, m_numChannels			(tcu::getNumUsedChannels(m_tcuFormat.order))
	, m_channelSize			(tcu::getChannelSize(m_tcuFormat.type))
	, m_channelMask			(getChannelMask(m_channelSize))
	, m_renderSize			(32u, 32u)
	, m_graphicsPipeline	(m_context.getInstanceInterface(), m_context.getDeviceInterface(), m_context.getPhysicalDevice(), m_context.getDevice(), m_context.getDeviceExtensions(), testParams.pipelineConstructionType)
{
	DE_ASSERT(isUintFormat(m_params.format));

	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const VkDevice				vkDevice			= m_context.getDevice();
	const deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&					memAlloc			= m_context.getDefaultAllocator();
	constexpr auto				kPushConstantSize	= static_cast<deUint32>(sizeof(m_params.quadColor));

	// create color image
	{
		const VkImageCreateInfo	colorImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType			sType;
			DE_NULL,																	// const void*				pNext;
			0u,																			// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,															// VkImageType				imageType;
			m_params.format,															// VkFormat					format;
			{ m_renderSize.x(), m_renderSize.y(), 1u },									// VkExtent3D				extent;
			1u,																			// deUint32					mipLevels;
			1u,																			// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,														// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling			tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,		// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode			sharingMode;
			1u,																			// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,															// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED													// VkImageLayout			initialLayout;
		};

		m_colorImageCreateInfo	= colorImageParams;
		m_colorImage			= de::MovePtr<ImageWithMemory>(new ImageWithMemory(vk, vkDevice, memAlloc, m_colorImageCreateInfo, MemoryRequirement::Any));

		// create color attachment view
		const VkImageViewCreateInfo colorAttachmentViewParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkImageViewCreateFlags	flags;
			m_colorImage->get(),								// VkImage					image;
			VK_IMAGE_VIEW_TYPE_2D,								// VkImageViewType			viewType;
			m_params.format,									// VkFormat					format;
			{	VK_COMPONENT_SWIZZLE_IDENTITY,
				VK_COMPONENT_SWIZZLE_IDENTITY,
				VK_COMPONENT_SWIZZLE_IDENTITY,
				VK_COMPONENT_SWIZZLE_IDENTITY	},
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }		// VkImageSubresourceRange	subresourceRange;
		};

		m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
	}

	m_renderPass	= RenderPassWrapper(m_params.pipelineConstructionType, vk, vkDevice, m_params.format);
	m_renderPass.createFramebuffer(vk, vkDevice, **m_colorImage, *m_colorAttachmentView, m_renderSize.x(), m_renderSize.y());

	// create pipeline layout
	{
		const VkPushConstantRange pcRange =
		{
			VK_SHADER_STAGE_FRAGMENT_BIT,		// VkShaderStageFlags				stageFlags;
			0u,									// deUint32							offset;
			kPushConstantSize,					// deUint32							size;
		};

#ifndef CTS_USES_VULKANSC
		VkPipelineLayoutCreateFlags pipelineLayoutFlags = (m_params.pipelineConstructionType == PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC) ? 0u : deUint32(VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT);
#else
		VkPipelineLayoutCreateFlags pipelineLayoutFlags = 0u;
#endif // CTS_USES_VULKANSC

		VkPipelineLayoutCreateInfo pipelineLayoutParams
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			pipelineLayoutFlags,								// VkPipelineLayoutCreateFlags		flags;
			0u,													// deUint32							setLayoutCount;
			DE_NULL,											// const VkDescriptorSetLayout*		pSetLayouts;
			0u,													// deUint32							pushConstantRangeCount;
			DE_NULL,											// const VkPushConstantRange*		pPushConstantRanges;
		};

		m_preRasterizationStatePipelineLayout		= PipelineLayoutWrapper(m_params.pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);
		pipelineLayoutParams.pushConstantRangeCount = 1u;
		pipelineLayoutParams.pPushConstantRanges	= &pcRange;
		m_fragmentStatePipelineLayout				= PipelineLayoutWrapper(m_params.pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);
	}

	m_vertexShaderModule	= ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_vert"), 0);
	m_fragmentShaderModule	= ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_frag"), 0);

	// create pipeline
	{
		const VkPipelineVertexInputStateCreateInfo	vertexInputStateParams	= initVulkanStructure();

		const std::vector<VkViewport>				viewports				{ makeViewport(m_renderSize) };
		const std::vector<VkRect2D>					scissors				{ makeRect2D(m_renderSize) };

		VkColorComponentFlags						colorWriteMask		=	VK_COLOR_COMPONENT_R_BIT |
																			VK_COLOR_COMPONENT_G_BIT |
																			VK_COLOR_COMPONENT_B_BIT |
																			VK_COLOR_COMPONENT_A_BIT;

		const VkPipelineColorBlendAttachmentState blendAttachmentState =
		{
			VK_FALSE,					//	VkBool32				blendEnable;
			(VkBlendFactor) 0,			//	VkBlendFactor			srcColorBlendFactor;
			(VkBlendFactor) 0,			//	VkBlendFactor			dstColorBlendFactor;
			(VkBlendOp)		0,			//	VkBlendOp				colorBlendOp;
			(VkBlendFactor) 0,			//	VkBlendFactor			srcAlphaBlendFactor;
			(VkBlendFactor) 0,			//	VkBlendFactor			dstAlphaBlendFactor;
			(VkBlendOp)		0,			//	VkBlendOp				alphaBlendOp;
			colorWriteMask,				//	VkColorComponentFlags	colorWriteMask;
		};

		const VkPipelineColorBlendStateCreateInfo colorBlendStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	//	VkStructureType								sType;
			DE_NULL,													//	const void*									pNext;
			DE_NULL,													//	VkPipelineColorBlendStateCreateFlags		flags;
			VK_TRUE,													//	VkBool32									logicOpEnable;
			m_params.logicOp,											//	VkLogicOp									logicOp;
			1u,															//	uint32_t									attachmentCount;
			&blendAttachmentState,										//	const VkPipelineColorBlendAttachmentState*	pAttachments;
			{ 0.0f, 0.0f, 0.0f, 0.0f },									//	float										blendConstants[4];
		};

		m_graphicsPipeline.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
						  .setDefaultDepthStencilState()
						  .setDefaultRasterizationState()
						  .setDefaultMultisampleState()
						  .setMonolithicPipelineLayout(m_fragmentStatePipelineLayout)
						  .setupVertexInputState(&vertexInputStateParams)
						  .setupPreRasterizationShaderState(viewports,
															scissors,
															m_preRasterizationStatePipelineLayout,
															*m_renderPass,
															0u,
															m_vertexShaderModule)
						  .setupFragmentShaderState(m_fragmentStatePipelineLayout, *m_renderPass, 0u, m_fragmentShaderModule)
						  .setupFragmentOutputState(*m_renderPass, 0u, &colorBlendStateParams)
						  .buildPipeline();
	}

	// create command pool
	m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	// allocate and record command buffer
	{
		// Prepare clear color value and quad color taking into account the channel mask.
		VkClearValue	attachmentClearValue;
		tcu::UVec4		quadColor(0u, 0u, 0u, 0u);

		deMemset(&attachmentClearValue.color, 0, sizeof(attachmentClearValue.color));
		for (int c = 0; c < m_numChannels; ++c)
			attachmentClearValue.color.uint32[c] = (m_params.fbColor[c] & m_channelMask);

		for (int c = 0; c < m_numChannels; ++c)
			quadColor[c] = (m_params.quadColor[c] & m_channelMask);

		m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		beginCommandBuffer(vk, *m_cmdBuffer, 0u);
		m_renderPass.begin(vk, *m_cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()), attachmentClearValue);

		// Update push constant values
		vk.cmdPushConstants(*m_cmdBuffer, *m_fragmentStatePipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0u, kPushConstantSize, &quadColor);

		m_graphicsPipeline.bind(*m_cmdBuffer);
		vk.cmdDraw(*m_cmdBuffer, 4u, 1u, 0u, 0u);
		m_renderPass.end(vk, *m_cmdBuffer);
		endCommandBuffer(vk, *m_cmdBuffer);
	}
}

LogicOpTestInstance::~LogicOpTestInstance (void)
{
}

tcu::TestStatus LogicOpTestInstance::iterate (void)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();

	submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());
	return verifyImage();
}

tcu::TestStatus LogicOpTestInstance::verifyImage (void)
{
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const VkDevice				vkDevice			= m_context.getDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();
	const deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&					allocator			= m_context.getDefaultAllocator();
	auto&						log					= m_context.getTestContext().getLog();

	const auto					result				= readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, allocator, m_colorImage->get(), m_params.format, m_renderSize).release();
	const auto					resultAccess		= result->getAccess();
	const int					iWidth				= static_cast<int>(m_renderSize.x());
	const int					iHeight				= static_cast<int>(m_renderSize.y());
	tcu::UVec4					expectedColor		(0u, 0u, 0u, 0u);	// Overwritten below.
	tcu::TextureLevel			referenceTexture	(m_tcuFormat, iWidth, iHeight);
	auto						referenceAccess		= referenceTexture.getAccess();
	tcu::UVec4					threshold			(0u, 0u, 0u, 0u);	// Exact results.

	// Calculate proper expected color values.
	for (int c = 0; c < m_numChannels; ++c)
	{
		expectedColor[c] = calcOpResult(m_params.logicOp, m_params.quadColor[c], m_params.fbColor[c]);
		expectedColor[c] &= m_channelMask;
	}

	for (int y = 0; y < iHeight; ++y)
	for (int x = 0; x < iWidth; ++x)
		referenceAccess.setPixel(expectedColor, x, y);

	// Check result.
	bool resultOk = tcu::intThresholdCompare(log, "TestResults", "Test Result Images", referenceAccess, resultAccess, threshold, tcu::COMPARE_LOG_ON_ERROR);

	if (!resultOk)
		TCU_FAIL("Result does not match expected values; check log for details");

	return tcu::TestStatus::pass("Pass");
}

TestInstance *LogicOpTest::createInstance (Context& context) const
{
	return new LogicOpTestInstance(context, m_params);
}

std::string getSimpleFormatName (VkFormat format)
{
	return de::toLower(std::string(getFormatName(format)).substr(std::string("VK_FORMAT_").size()));
}

} // anonymous namespace

tcu::TestCaseGroup* createLogicOpTests (tcu::TestContext& testCtx, PipelineConstructionType pipelineType)
{
	de::MovePtr<tcu::TestCaseGroup>	logicOpTests (new tcu::TestCaseGroup(testCtx, "logic_op", "Logical Operations tests"));

	// 4 bits are enough to check all possible combinations of logical operation inputs at once, for example s AND d:
	//
	//		1 0 1 0
	//	AND	1 1 0 0
	//	------------
	//		1 0 0 0
	//
	// However, we will choose color values such that both higher bits and lower bits are used, and the implementation will not be
	// able to mix channels by mistake.
	//
	//	0011 0101 1010 1100
	//	3    5    a    c
	//	0101 0011 1100 1010
	//	5    3    c    a

	const tcu::UVec4 kQuadColor	= { 0x35acU, 0x5ac3U, 0xac35U, 0xc35aU };
	const tcu::UVec4 kFbColor	= { 0x53caU, 0x3ca5U, 0xca53U, 0xa53cU };

	// Note: the format will be chosen and changed later.
	std::vector<TestParams> logicOpTestParams
	{
		{ VK_LOGIC_OP_CLEAR,			pipelineType,	kFbColor,	kQuadColor,		VK_FORMAT_UNDEFINED,	"clear"			},
		{ VK_LOGIC_OP_AND,				pipelineType,	kFbColor,	kQuadColor,		VK_FORMAT_UNDEFINED,	"and"			},
		{ VK_LOGIC_OP_AND_REVERSE,		pipelineType,	kFbColor,	kQuadColor,		VK_FORMAT_UNDEFINED,	"and_reverse"	},
		{ VK_LOGIC_OP_COPY,				pipelineType,	kFbColor,	kQuadColor,		VK_FORMAT_UNDEFINED,	"copy"			},
		{ VK_LOGIC_OP_AND_INVERTED,		pipelineType,	kFbColor,	kQuadColor,		VK_FORMAT_UNDEFINED,	"and_inverted"	},
		{ VK_LOGIC_OP_NO_OP,			pipelineType,	kFbColor,	kQuadColor,		VK_FORMAT_UNDEFINED,	"no_op"			},
		{ VK_LOGIC_OP_XOR,				pipelineType,	kFbColor,	kQuadColor,		VK_FORMAT_UNDEFINED,	"xor"			},
		{ VK_LOGIC_OP_OR,				pipelineType,	kFbColor,	kQuadColor,		VK_FORMAT_UNDEFINED,	"or"			},
		{ VK_LOGIC_OP_NOR,				pipelineType,	kFbColor,	kQuadColor,		VK_FORMAT_UNDEFINED,	"nor"			},
		{ VK_LOGIC_OP_EQUIVALENT,		pipelineType,	kFbColor,	kQuadColor,		VK_FORMAT_UNDEFINED,	"equivalent"	},
		{ VK_LOGIC_OP_INVERT,			pipelineType,	kFbColor,	kQuadColor,		VK_FORMAT_UNDEFINED,	"invert"		},
		{ VK_LOGIC_OP_OR_REVERSE,		pipelineType,	kFbColor,	kQuadColor,		VK_FORMAT_UNDEFINED,	"or_reverse"	},
		{ VK_LOGIC_OP_COPY_INVERTED,	pipelineType,	kFbColor,	kQuadColor,		VK_FORMAT_UNDEFINED,	"copy_inverted"	},
		{ VK_LOGIC_OP_OR_INVERTED,		pipelineType,	kFbColor,	kQuadColor,		VK_FORMAT_UNDEFINED,	"or_inverted"	},
		{ VK_LOGIC_OP_NAND,				pipelineType,	kFbColor,	kQuadColor,		VK_FORMAT_UNDEFINED,	"nand"			},
		{ VK_LOGIC_OP_SET,				pipelineType,	kFbColor,	kQuadColor,		VK_FORMAT_UNDEFINED,	"set"			},
	};

	const VkFormat formatList[] =
	{
		VK_FORMAT_R8_UINT,
		VK_FORMAT_R8G8_UINT,
		VK_FORMAT_R8G8B8_UINT,
		VK_FORMAT_B8G8R8_UINT,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_B8G8R8A8_UINT,
		VK_FORMAT_R16_UINT,
		VK_FORMAT_R16G16_UINT,
		VK_FORMAT_R16G16B16_UINT,
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32B32_UINT,
		VK_FORMAT_R32G32B32A32_UINT,
	};

	for (int formatIdx = 0; formatIdx < DE_LENGTH_OF_ARRAY(formatList); ++formatIdx)
	{
		const auto&	format		= formatList[formatIdx];
		const auto	formatName	= getSimpleFormatName(format);
		const auto	formatDesc	= "Logical operator tests with format " + formatName;

		de::MovePtr<tcu::TestCaseGroup> formatGroup (new tcu::TestCaseGroup(testCtx, formatName.c_str(), formatDesc.c_str()));

		for (auto& params : logicOpTestParams)
		{
			params.format = format;
			formatGroup->addChild(new LogicOpTest(testCtx, params.name, "Tests the " + params.name + " logical operator", params));
		}

		logicOpTests->addChild(formatGroup.release());
	}

	return logicOpTests.release();
}

} // pipeline namespace
} // vkt namespace
