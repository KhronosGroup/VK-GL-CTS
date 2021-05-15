/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 Danylo Piliaiev <danylo.piliaiev@gmail.com>
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
 * \brief Test for conditional rendering of vkCmdClearAttachments
 *//*--------------------------------------------------------------------*/

#include "vktConditionalClearAttachmentTests.hpp"
#include "vktConditionalRenderingTestUtil.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktDrawTestCaseUtil.hpp"

#include "vktDrawBaseClass.hpp"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"

#include "vkDefs.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"

namespace vkt
{
namespace conditional
{
namespace
{

struct ConditionalTestSpec : public Draw::TestSpecBase
{
	ConditionalData	conditionalData;
};

class ConditionalClearAttachmentTest : public Draw::DrawTestsBaseClass
{
public:
	typedef		ConditionalTestSpec	TestSpec;

								ConditionalClearAttachmentTest	(Context &context, ConditionalTestSpec testSpec);

	virtual		tcu::TestStatus iterate							(void);
protected:
	const ConditionalData			m_conditionalData;
	de::SharedPtr<Draw::Buffer>		m_conditionalBuffer;

	vk::Move<vk::VkCommandBuffer>	m_secondaryCmdBuffer;
};

ConditionalClearAttachmentTest::ConditionalClearAttachmentTest (Context &context, ConditionalTestSpec testSpec)
	: Draw::DrawTestsBaseClass(context, testSpec.shaders[glu::SHADERTYPE_VERTEX], testSpec.shaders[glu::SHADERTYPE_FRAGMENT], false, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
	, m_conditionalData(testSpec.conditionalData)
{
	checkConditionalRenderingCapabilities(context, m_conditionalData);

	m_data.push_back(Draw::VertexElementData(tcu::Vec4(0.0f), tcu::Vec4(0.0f), 0));

	initialize();

	m_secondaryCmdBuffer = vk::allocateCommandBuffer(m_vk, m_context.getDevice(), *m_cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY);
}

tcu::TestStatus ConditionalClearAttachmentTest::iterate (void)
{
	tcu::TestLog&		log		= m_context.getTestContext().getLog();
	const vk::VkQueue	queue	= m_context.getUniversalQueue();
	const vk::VkDevice	device	= m_context.getDevice();

	const tcu::Vec4 clearColor	= tcu::RGBA::black().toVec();
	const tcu::Vec4 drawColor	= tcu::RGBA::blue().toVec();

	const bool useSecondaryCmdBuffer = m_conditionalData.conditionInherited || m_conditionalData.conditionInSecondaryCommandBuffer;
	beginRender(useSecondaryCmdBuffer ? vk::VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : vk::VK_SUBPASS_CONTENTS_INLINE);

	vk::VkCommandBuffer targetCmdBuffer = *m_cmdBuffer;

	if (useSecondaryCmdBuffer)
	{
		const vk::VkCommandBufferInheritanceConditionalRenderingInfoEXT conditionalRenderingInheritanceInfo =
		{
			vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_CONDITIONAL_RENDERING_INFO_EXT,
			DE_NULL,
			m_conditionalData.conditionInherited ? VK_TRUE : VK_FALSE	// conditionalRenderingEnable
		};

		const vk::VkCommandBufferInheritanceInfo inheritanceInfo =
		{
			vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
			&conditionalRenderingInheritanceInfo,
			*m_renderPass,										        // renderPass
			0u,															// subpass
			*m_framebuffer,										        // framebuffer
			VK_FALSE,													// occlusionQueryEnable
			(vk::VkQueryControlFlags)0u,								// queryFlags
			(vk::VkQueryPipelineStatisticFlags)0u,						// pipelineStatistics
		};

		const vk::VkCommandBufferBeginInfo commandBufferBeginInfo =
		{
			vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			DE_NULL,
			vk::VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
			&inheritanceInfo
		};

		m_vk.beginCommandBuffer(*m_secondaryCmdBuffer, &commandBufferBeginInfo);

		targetCmdBuffer = *m_secondaryCmdBuffer;
	}

	m_vk.cmdBindPipeline(targetCmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

	const vk::VkClearAttachment	clearAttachment	=
	{
		vk::VK_IMAGE_ASPECT_COLOR_BIT,			// VkImageAspectFlags	aspectMask;
		0u,										// deUint32				colorAttachment;
		vk::makeClearValueColor(drawColor)		// VkClearValue			clearValue;
	};

	const vk::VkClearRect rect =
	{
		vk::makeRect2D(WIDTH, HEIGHT),	// VkRect2D    rect;
		0u,								// uint32_t    baseArrayLayer;
		1u,								// uint32_t    layerCount;
	};

	m_conditionalBuffer = createConditionalRenderingBuffer(m_context, m_conditionalData);

	if (m_conditionalData.conditionInSecondaryCommandBuffer)
	{
		beginConditionalRendering(m_vk, *m_secondaryCmdBuffer, *m_conditionalBuffer, m_conditionalData);
		m_vk.cmdClearAttachments(*m_secondaryCmdBuffer, 1, &clearAttachment, 1, &rect);
		m_vk.cmdEndConditionalRenderingEXT(*m_secondaryCmdBuffer);
		m_vk.endCommandBuffer(*m_secondaryCmdBuffer);
	}
	else if (m_conditionalData.conditionInherited)
	{
		m_vk.cmdClearAttachments(*m_secondaryCmdBuffer, 1, &clearAttachment, 1, &rect);
		m_vk.endCommandBuffer(*m_secondaryCmdBuffer);
	}

	if (m_conditionalData.conditionInPrimaryCommandBuffer)
	{
		beginConditionalRendering(m_vk, *m_cmdBuffer, *m_conditionalBuffer, m_conditionalData);

		if (m_conditionalData.conditionInherited)
		{
			m_vk.cmdExecuteCommands(*m_cmdBuffer, 1, &m_secondaryCmdBuffer.get());
		}
		else
		{
			m_vk.cmdClearAttachments(*m_cmdBuffer, 1, &clearAttachment, 1, &rect);
		}

		m_vk.cmdEndConditionalRenderingEXT(*m_cmdBuffer);
	}
	else if (useSecondaryCmdBuffer)
	{
		m_vk.cmdExecuteCommands(*m_cmdBuffer, 1, &m_secondaryCmdBuffer.get());
	}

	endRender();
	endCommandBuffer(m_vk, *m_cmdBuffer);

	submitCommandsAndWait(m_vk, device, queue, m_cmdBuffer.get());

	// Validation
	tcu::Texture2D referenceFrame(vk::mapVkFormat(m_colorAttachmentFormat), (int)(0.5f + static_cast<float>(WIDTH)), (int)(0.5f + static_cast<float>(HEIGHT)));
								  referenceFrame.allocLevel(0);

	const deInt32 frameWidth	= referenceFrame.getWidth();
	const deInt32 frameHeight	= referenceFrame.getHeight();

	tcu::clear(referenceFrame.getLevel(0), clearColor);

	const tcu::Vec4 referenceColor = m_conditionalData.expectCommandExecution ?
										drawColor : clearColor;

	for (int y = 0; y < frameHeight; y++)
	{
		for (int x = 0; x < frameWidth; x++)
		{
			referenceFrame.getLevel(0).setPixel(referenceColor, x, y);
		}
	}

	const vk::VkOffset3D zeroOffset = { 0, 0, 0 };
	const tcu::ConstPixelBufferAccess renderedFrame = m_colorTargetImage->readSurface(queue, m_context.getDefaultAllocator(),
		vk::VK_IMAGE_LAYOUT_GENERAL, zeroOffset, WIDTH, HEIGHT, vk::VK_IMAGE_ASPECT_COLOR_BIT);

	qpTestResult res = QP_TEST_RESULT_PASS;

	if (!tcu::fuzzyCompare(log, "Result", "Image comparison result",
		referenceFrame.getLevel(0), renderedFrame, 0.05f,
		tcu::COMPARE_LOG_RESULT))
	{
		res = QP_TEST_RESULT_FAIL;
	}

	return tcu::TestStatus(res, qpGetTestResultName(res));
}

}	// anonymous

ConditionalClearAttachmentTests::ConditionalClearAttachmentTests (tcu::TestContext &testCtx)
	: TestCaseGroup	(testCtx, "clear_attachments", "vkCmdClearAttachments with conditional rendering")
{
	/* Left blank on purpose */
}

ConditionalClearAttachmentTests::~ConditionalClearAttachmentTests (void) {}

void ConditionalClearAttachmentTests::init (void)
{
	for (int conditionNdx = 0; conditionNdx < DE_LENGTH_OF_ARRAY(conditional::s_testsData); conditionNdx++)
	{
		const ConditionalData& conditionData = conditional::s_testsData[conditionNdx];

		tcu::TestCaseGroup* conditionalDrawRootGroup = new tcu::TestCaseGroup(m_testCtx, de::toString(conditionData).c_str(), "");

		ConditionalTestSpec testSpec;
		testSpec.conditionalData = conditionData;
		testSpec.shaders[glu::SHADERTYPE_VERTEX] = "vulkan/dynamic_state/VertexFetch.vert";
		testSpec.shaders[glu::SHADERTYPE_FRAGMENT] = "vulkan/dynamic_state/VertexFetch.frag";

		conditionalDrawRootGroup->addChild(new Draw::InstanceFactory<ConditionalClearAttachmentTest>(m_testCtx, "clear_attachments", "", testSpec));

		addChild(conditionalDrawRootGroup);
	}
}

}	// conditional
}	// vkt
