/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 The Khronos Group Inc.
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
 * \brief Tests subpass merge feedback extension
 *//*--------------------------------------------------------------------*/

#include "vktRenderPassSubpassMergeFeedbackTests.hpp"
#include "pipeline/vktPipelineImageUtil.hpp"
#include "vktTestCase.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuPlatform.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuTestLog.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "deRandom.hpp"
#include <cstring>
#include <set>
#include <sstream>
#include <vector>

namespace vkt
{
namespace renderpass
{

using namespace vk;

namespace
{

struct TestParams
{
	deUint32            subpassCount;
	bool                disallowMergeRenderpass;
	bool                disallowMergeSubPass1;
};

struct Vertex4RGBA
{
	tcu::Vec4 position;
	tcu::Vec4 color;
};

class SubpassMergeFeedbackTest : public vkt::TestCase
{
public:
										SubpassMergeFeedbackTest	(tcu::TestContext&	testContext,
																     const std::string&	name,
																     const std::string&	description,
																     const TestParams&	testParams);
	virtual								~SubpassMergeFeedbackTest	(void);
	virtual TestInstance*				createInstance			(Context&			context) const;
private:
	const TestParams					m_testParams;
};

class SubpassMergeFeedbackTestInstance : public vkt::TestInstance
{
public:
										SubpassMergeFeedbackTestInstance	(Context&				context,
																		     const TestParams&		testParams);
	virtual								~SubpassMergeFeedbackTestInstance	(void);
	virtual tcu::TestStatus				iterate							    (void);
private:

	tcu::TestStatus                     createRenderPassAndVerify           (const DeviceInterface&	vk,
																			 VkDevice				vkDevice);

	const TestParams					m_testParams;
};

SubpassMergeFeedbackTest::SubpassMergeFeedbackTest (tcu::TestContext&	testContext,
											const std::string&	name,
											const std::string&	description,
											const TestParams&	testParams)
	: vkt::TestCase	(testContext, name, description)
	, m_testParams	(testParams)
{
}

SubpassMergeFeedbackTest::~SubpassMergeFeedbackTest (void)
{
}

TestInstance* SubpassMergeFeedbackTest::createInstance (Context& context) const
{
	return new SubpassMergeFeedbackTestInstance(context, m_testParams);
}

SubpassMergeFeedbackTestInstance::SubpassMergeFeedbackTestInstance (Context&			context,
																	const TestParams&	testParams)
	: vkt::TestInstance	(context)
	, m_testParams      (testParams)
{
	// Check for renderpass2 extension
	context.requireDeviceFunctionality("VK_KHR_create_renderpass2");

	// Check for subpass merge feedback extension
	context.requireDeviceFunctionality("VK_EXT_subpass_merge_feedback");
}


SubpassMergeFeedbackTestInstance::~SubpassMergeFeedbackTestInstance (void)
{
}

tcu::TestStatus SubpassMergeFeedbackTestInstance::createRenderPassAndVerify (const DeviceInterface&	vk,
																			 VkDevice				vkDevice)
{
	const VkImageAspectFlags	aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;

	std::vector<AttachmentDescription2> attachmentDescriptions;
	std::vector<AttachmentReference2> resultAttachments;
	std::vector<AttachmentReference2> inputAttachments;
	std::vector<VkRenderPassCreationControlEXT> subpassMergeControls;
	std::vector<VkRenderPassSubpassFeedbackCreateInfoEXT> subpassFeedbackCreateInfos;
	std::vector<VkRenderPassSubpassFeedbackInfoEXT> subpassFeedbackInfos;
	std::vector<SubpassDescription2> subpassDescriptions;

	for (deUint32 i = 0; i < m_testParams.subpassCount; ++i)
	{
		attachmentDescriptions.emplace_back(
			nullptr,								// const void*						pNext
			(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags		flags
			VK_FORMAT_R8G8B8A8_UNORM,					// VkFormat							format
			VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits			samples
			VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp				loadOp
			VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp				storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp				stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp				stencilStoreOp
			VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout					initialLayout
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout					finalLayout
		);

		resultAttachments.emplace_back(
			nullptr,								// const void*			pNext
			i,											// deUint32				attachment
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout		layout
			aspectMask									// VkImageAspectFlags	aspectMask
		);

		inputAttachments.emplace_back(
			nullptr,								// const void*			pNext
			i,											// deUint32				attachment
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,	// VkImageLayout		layout
			aspectMask									// VkImageAspectFlags	aspectMask
		);

		VkBool32 disallowSubpassMerge = VK_FALSE;
		if (i == 1 && m_testParams.disallowMergeSubPass1)
		{
			disallowSubpassMerge = VK_TRUE;
		}

		const VkRenderPassCreationControlEXT mergeControl = {
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATION_CONTROL_EXT,			// VkStructureType			sType;
			nullptr,												// const void*				pNext;
			disallowSubpassMerge,									// VkBool32					disallowMerging;
		};
		subpassMergeControls.push_back( mergeControl );

		VkRenderPassSubpassFeedbackInfoEXT feedbackInfo = {
			VK_SUBPASS_MERGE_STATUS_MERGED_EXT,							// VkSubpassMergeStatusEXT	subpassMergeStatus;
			"",															// description[VK_MAX_DESCRIPTION_SIZE];
			0															// uint32_t					postMergeIndex;
		};
		subpassFeedbackInfos.push_back( feedbackInfo );
	}

	for (deUint32 i = 0; i < m_testParams.subpassCount; ++i)
	{
		const VkRenderPassSubpassFeedbackCreateInfoEXT feedbackCreateInfo = {
			VK_STRUCTURE_TYPE_RENDER_PASS_SUBPASS_FEEDBACK_CREATE_INFO_EXT,	// VkStructureType			sType;
			&subpassMergeControls[i],									// const void*				pNext;
			&subpassFeedbackInfos[i]
		};
		subpassFeedbackCreateInfos.push_back( feedbackCreateInfo );
	}

	for (deUint32 i = 0; i < m_testParams.subpassCount; ++i)
	{
		subpassDescriptions.emplace_back (
			&subpassFeedbackCreateInfos[i],
			(VkSubpassDescriptionFlags)0,		// VkSubpassDescriptionFlags		flags
			VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint				pipelineBindPoint
			0u,									// deUint32							viewMask
			(i>0) ? 1u : 0u,					// deUint32							inputAttachmentCount
			(i>0) ? &inputAttachments[i-1] : nullptr,						// const VkAttachmentReference*		pInputAttachments
			1u,									// deUint32							colorAttachmentCount
			&resultAttachments[i],				// const VkAttachmentReference*		pColorAttachments
			nullptr,						// const VkAttachmentReference*		pResolveAttachments
			nullptr,						// const VkAttachmentReference*		pDepthStencilAttachment
			0u,									// deUint32							preserveAttachmentCount
			nullptr						// const deUint32*					pPreserveAttachments
		);
	}

	std::vector<SubpassDependency2> subpassDependencies;
	for (deUint32 i = 1; i < m_testParams.subpassCount; ++i)
	{

		subpassDependencies.emplace_back(
			nullptr,										// const void*				pNext
			i-1,												// uint32_t					srcSubpass
			i,										// uint32_t					dstSubpass
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,	// VkPipelineStageFlags		srcStageMask
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,			// VkPipelineStageFlags		dstStageMask
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags			srcAccessMask
			VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,			// VkAccessFlags			dstAccessMask
			VK_DEPENDENCY_BY_REGION_BIT,					// VkDependencyFlags		dependencyFlags
			0u												// deInt32					viewOffset
		);
	}


	VkBool32 disallowMerging = VK_FALSE;
	if (m_testParams.disallowMergeRenderpass)
	{
		disallowMerging = VK_TRUE;
	}

	const VkRenderPassCreationControlEXT renderpassControl =
	{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATION_CONTROL_EXT,						// VkStructureType			sType;
			DE_NULL,																// const void*				pNext;
			disallowMerging															// VkBool32					disallowMerging;
	};

	VkRenderPassCreationFeedbackInfoEXT renderpassFeedbackInfo =
	{
		0                     // uint32_t                                     postMergeSubpassCount;
	};

	VkRenderPassCreationFeedbackCreateInfoEXT renderpassFeedbackCreateInfo =
	{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATION_FEEDBACK_CREATE_INFO_EXT,				// VkStructureType			sType;
			&renderpassControl,														// const void*				pNext;
			&renderpassFeedbackInfo
	};

	const RenderPassCreateInfo2	renderPassInfo					(
		&renderpassFeedbackCreateInfo,					// const void*						pNext
		(VkRenderPassCreateFlags)0,					// VkRenderPassCreateFlags			flags
		static_cast<deUint32>(attachmentDescriptions.size()),					// deUint32							attachmentCount
		attachmentDescriptions.data(),				// const VkAttachmentDescription*	pAttachments
		static_cast<deUint32>(subpassDescriptions.size()),					// deUint32							subpassCount
		subpassDescriptions.data(),					// const VkSubpassDescription*		pSubpasses
		static_cast<deUint32>(subpassDependencies.size()),											// deUint32							dependencyCount
		subpassDependencies.data(),							// const VkSubpassDependency*		pDependencies
		0u,											// deUint32							correlatedViewMaskCount
		nullptr								// const deUint32*					pCorrelatedViewMasks
	);

	Move<VkRenderPass> renderPass = renderPassInfo.createRenderPass(vk, vkDevice);

	// Verify merge status flags
	if (m_testParams.disallowMergeRenderpass)
	{
		if (renderpassFeedbackInfo.postMergeSubpassCount != m_testParams.subpassCount)
		{
			return tcu::TestStatus::fail("Fail");
		}

		for (deUint32 i = 0; i < m_testParams.subpassCount; ++i)
		{
			if (subpassFeedbackCreateInfos[i].pSubpassFeedback->subpassMergeStatus != VK_SUBPASS_MERGE_STATUS_DISALLOWED_EXT)
			{
				return tcu::TestStatus::fail("Fail");
			}

			if (i > 0 &&
				subpassFeedbackCreateInfos[i].pSubpassFeedback->postMergeIndex == subpassFeedbackCreateInfos[i-1].pSubpassFeedback->postMergeIndex)
			{
				return tcu::TestStatus::fail("Fail");
			}
		}
	}
	else
	{
		if (renderpassFeedbackInfo.postMergeSubpassCount > m_testParams.subpassCount)
		{
			return tcu::TestStatus::fail("Fail");
		}

		if (m_testParams.subpassCount == 1 &&
			subpassFeedbackCreateInfos[0].pSubpassFeedback->subpassMergeStatus != VK_SUBPASS_MERGE_STATUS_NOT_MERGED_SINGLE_SUBPASS_EXT)
		{
			return tcu::TestStatus::fail("Fail");
		}

		for (deUint32 i = 1; i < m_testParams.subpassCount; ++i)
		{
			if (i == 1 && m_testParams.disallowMergeSubPass1 &&
				subpassFeedbackCreateInfos[i].pSubpassFeedback->subpassMergeStatus != VK_SUBPASS_MERGE_STATUS_DISALLOWED_EXT)
			{
				return tcu::TestStatus::fail("Fail");
			}

			if (subpassFeedbackCreateInfos[i].pSubpassFeedback->subpassMergeStatus == VK_SUBPASS_MERGE_STATUS_MERGED_EXT &&
				subpassFeedbackCreateInfos[i].pSubpassFeedback->postMergeIndex != subpassFeedbackCreateInfos[i-1].pSubpassFeedback->postMergeIndex)
			{
				return tcu::TestStatus::fail("Fail");
			}

			if (subpassFeedbackCreateInfos[i].pSubpassFeedback->subpassMergeStatus != VK_SUBPASS_MERGE_STATUS_MERGED_EXT &&
				subpassFeedbackCreateInfos[i].pSubpassFeedback->postMergeIndex == subpassFeedbackCreateInfos[i-1].pSubpassFeedback->postMergeIndex)
			{
				return tcu::TestStatus::fail("Fail");
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus SubpassMergeFeedbackTestInstance::iterate (void)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();

	// Create render pass
	return createRenderPassAndVerify(vk, vkDevice);
}

} // anonymous

tcu::TestCaseGroup* createRenderPassSubpassMergeFeedbackTests (tcu::TestContext& testCtx, const RenderingType renderingType)
{
	if (renderingType != RENDERING_TYPE_RENDERPASS2)
	{
		return nullptr;
	}

	de::MovePtr<tcu::TestCaseGroup>		subpassMergeFeedbackTests		(new tcu::TestCaseGroup(testCtx, "subpass_merge_feedback", "Subpass merge feedback tests"));

	{
		TestParams			params;
		const std::string	testName = std::string("single_subpass");

		params.subpassCount = 1;
		params.disallowMergeRenderpass = false;
		params.disallowMergeSubPass1 = false;

		subpassMergeFeedbackTests->addChild(new SubpassMergeFeedbackTest(testCtx, testName, "", params));
	}
	{
		TestParams			params;
		const std::string	testName = std::string("single_subpass_disallow_renderpass_merge");

		params.subpassCount = 1;
		params.disallowMergeRenderpass = true;
		params.disallowMergeSubPass1 = false;

		subpassMergeFeedbackTests->addChild(new SubpassMergeFeedbackTest(testCtx, testName, "", params));
	}
	{
		TestParams			params;
		const std::string	testName = std::string("three_subpasses");

		params.subpassCount = 3;
		params.disallowMergeRenderpass = false;
		params.disallowMergeSubPass1 = false;

		subpassMergeFeedbackTests->addChild(new SubpassMergeFeedbackTest(testCtx, testName, "", params));
	}
	{
		TestParams			params;
		const std::string	testName = std::string("three_subpasses_disallow_renderpass_merge");

		params.subpassCount = 3;
		params.disallowMergeRenderpass = true;
		params.disallowMergeSubPass1 = false;

		subpassMergeFeedbackTests->addChild(new SubpassMergeFeedbackTest(testCtx, testName, "", params));
	}
	{
		TestParams			params;
		const std::string	testName = std::string("three_subpasses_disallow_subpass_merge");

		params.subpassCount = 3;
		params.disallowMergeRenderpass = false;
		params.disallowMergeSubPass1 = true;

		subpassMergeFeedbackTests->addChild(new SubpassMergeFeedbackTest(testCtx, testName, "", params));
	}
	{
		TestParams			params;
		const std::string	testName = std::string("many_subpasses");

		params.subpassCount = 32;
		params.disallowMergeRenderpass = false;
		params.disallowMergeSubPass1 = false;

		subpassMergeFeedbackTests->addChild(new SubpassMergeFeedbackTest(testCtx, testName, "", params));
	}

	return subpassMergeFeedbackTests.release();
}

} // renderpass
} // vkt
