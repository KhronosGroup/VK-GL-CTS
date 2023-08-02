/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
 * Copyright (c) 2023 Valve Corporation.
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
 * \brief Vulkan Fragment Shader Invocation and Sample Cound Tests
 *//*--------------------------------------------------------------------*/
#include "vktQueryPoolFragInvocationTests.hpp"
#include "tcuImageCompare.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestCaseUtil.hpp"

#include <sstream>

namespace vkt
{
namespace QueryPool
{

namespace
{

using namespace vk;

enum class QueryType { INVOCATIONS = 0, OCCLUSION };

std::string getQueryTypeName (const QueryType qType)
{
	switch (qType)
	{
	case QueryType::INVOCATIONS:		return "frag_invs";
	case QueryType::OCCLUSION:			return "occlusion";
	default: break;
	}

	DE_ASSERT(false);
	return "";
}

struct TestParams
{
	const QueryType	queryType;
	const bool		secondary;
};

tcu::Vec4 getFlatColor (void)
{
	return tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f);
}

tcu::Vec4 getClearColor (void)
{
	return tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

void checkSupport (Context& context, TestParams params)
{
	if (params.secondary)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_INHERITED_QUERIES);

	if (params.queryType == QueryType::OCCLUSION)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_OCCLUSION_QUERY_PRECISE);
	else if (params.queryType == QueryType::INVOCATIONS)
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_PIPELINE_STATISTICS_QUERY);
}

void initPrograms (vk::SourceCollections& programCollection, TestParams)
{
	std::ostringstream vert;
	vert
		<< "#version 460\n"
		<< "vec2 positions[3] = vec2[](\n"
		<< "    vec2(-1.0, -1.0),"
		<< "    vec2(3.0, -1.0),"
		<< "    vec2(-1.0, 3.0)"
		<< ");\n"
		<< "void main() {\n"
		<< "    gl_Position = vec4(positions[gl_VertexIndex % 3], 0.0, 1.0);\n"
		<< "}"
		;
	programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

	std::ostringstream frag;
	frag
		<< "#version 460\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main() {\n"
		<< "    outColor = vec4" << getFlatColor() << ";\n"
		<< "}";
		;
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

void recordRenderPassCommands (const DeviceInterface& vkd, const VkCommandBuffer cmdBuffer, const VkPipelineBindPoint bindPoint, const VkPipeline pipeline)
{
	vkd.cmdBindPipeline(cmdBuffer, bindPoint, pipeline);
	vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
}

tcu::TestStatus testInvocations (Context& context, const TestParams params)
{
	const auto			ctx					= context.getContextCommonData();
	const tcu::IVec3	fbExtent			(64, 64, 1);
	const auto			vkExtent			= makeExtent3D(fbExtent);
	const auto			colorFormat			= VK_FORMAT_R8G8B8A8_UNORM;
	const auto			colorSRR			= makeDefaultImageSubresourceRange();
	const auto			colorSRL			= makeDefaultImageSubresourceLayers();
	const auto			colorUsage			= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const auto			imageType			= VK_IMAGE_TYPE_2D;
	const auto			bindPoint			= VK_PIPELINE_BIND_POINT_GRAPHICS;

	ImageWithBuffer colorBuffer (ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, colorUsage, imageType, colorSRR);

	const auto&	binaries	= context.getBinaryCollection();
	const auto	vertModule	= createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
	const auto	fragModule	= createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

	const auto pipelineLayout	= makePipelineLayout(ctx.vkd, ctx.device);
	const auto renderPass		= makeRenderPass(ctx.vkd, ctx.device, colorFormat);
	const auto framebuffer		= makeFramebuffer(ctx.vkd, ctx.device, renderPass.get(), colorBuffer.getImageView(), vkExtent.width, vkExtent.height);

	const bool isInvQuery	= (params.queryType == QueryType::INVOCATIONS);
	const auto queryType	= (isInvQuery ? VK_QUERY_TYPE_PIPELINE_STATISTICS : VK_QUERY_TYPE_OCCLUSION);
	const auto statFlags	= (isInvQuery ? static_cast<VkQueryPipelineStatisticFlags>(VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT) : 0u);
	const auto controlFlags	= (isInvQuery ? 0u : static_cast<VkQueryControlFlags>(VK_QUERY_CONTROL_PRECISE_BIT));

	const VkQueryPoolCreateInfo queryPoolCreateInfo =
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,	//	VkStructureType					sType;
		nullptr,									//	const void*						pNext;
		0u,											//	VkQueryPoolCreateFlags			flags;
		queryType,									//	VkQueryType						queryType;
		1u,											//	uint32_t						queryCount;
		statFlags,									//	VkQueryPipelineStatisticFlags	pipelineStatistics;
	};
	const auto queryPool = createQueryPool(ctx.vkd, ctx.device, &queryPoolCreateInfo);

	const std::vector<VkViewport>	viewports	(1u, makeViewport(vkExtent));
	const std::vector<VkRect2D>		scissors	(1u, makeRect2D(fbExtent));

	const VkPipelineVertexInputStateCreateInfo inputStateCreateInfo = initVulkanStructure();

	const auto pipeline = makeGraphicsPipeline(ctx.vkd, ctx.device, pipelineLayout.get(),
		vertModule.get(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, fragModule.get(), renderPass.get(),
		viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u,
		&inputStateCreateInfo);

	CommandPoolWithBuffer	cmd					(ctx.vkd, ctx.device, ctx.qfIndex);
	VkCommandBuffer			primaryCmdBuffer	= cmd.cmdBuffer.get();
	Move<VkCommandBuffer>	secCmdBufferPtr;

	if (params.secondary)
	{
		secCmdBufferPtr	= allocateCommandBuffer(ctx.vkd, ctx.device, cmd.cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_SECONDARY);
		const auto secCmdBuffer = secCmdBufferPtr.get();

		const VkCommandBufferInheritanceInfo inheritanceInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,				//	VkStructureType					sType;
			nullptr,														//	const void*						pNext;
			renderPass.get(),												//	VkRenderPass					renderPass;
			0u,																//	uint32_t						subpass;
			framebuffer.get(),												//	VkFramebuffer					framebuffer;
			((queryType == VK_QUERY_TYPE_OCCLUSION) ? VK_TRUE : VK_FALSE),	//	VkBool32						occlusionQueryEnable;
			controlFlags,													//	VkQueryControlFlags				queryFlags;
			statFlags,														//	VkQueryPipelineStatisticFlags	pipelineStatistics;
		};

		const auto usageFlags = (VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
		const VkCommandBufferBeginInfo beginInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	//	VkStructureType							sType;
			nullptr,										//	const void*								pNext;
			usageFlags,										//	VkCommandBufferUsageFlags				flags;
			&inheritanceInfo,								//	const VkCommandBufferInheritanceInfo*	pInheritanceInfo;
		};

		VK_CHECK(ctx.vkd.beginCommandBuffer(secCmdBuffer, &beginInfo));
		recordRenderPassCommands(ctx.vkd, secCmdBuffer, bindPoint, pipeline.get());
		endCommandBuffer(ctx.vkd, secCmdBuffer);
	}

	const auto subpassContents	= (params.secondary ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS : VK_SUBPASS_CONTENTS_INLINE);
	const auto clearColor		= makeClearValueColor(getClearColor());

	beginCommandBuffer(ctx.vkd, primaryCmdBuffer);
	ctx.vkd.cmdResetQueryPool(primaryCmdBuffer, queryPool.get(), 0u, 1u);
	ctx.vkd.cmdBeginQuery(primaryCmdBuffer, queryPool.get(), 0u, controlFlags);
	beginRenderPass(ctx.vkd, primaryCmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0), clearColor, subpassContents);
	if (!params.secondary)
		recordRenderPassCommands(ctx.vkd, primaryCmdBuffer, bindPoint, pipeline.get());
	else
		ctx.vkd.cmdExecuteCommands(primaryCmdBuffer, 1u, &secCmdBufferPtr.get());
	endRenderPass(ctx.vkd, primaryCmdBuffer);
	ctx.vkd.cmdEndQuery(primaryCmdBuffer, queryPool.get(), 0u);
	{
		const auto preTransferBarrier = makeImageMemoryBarrier(
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_TRANSFER_READ_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			colorBuffer.getImage(), colorSRR);
		cmdPipelineImageMemoryBarrier(ctx.vkd, primaryCmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, &preTransferBarrier);

		const auto copyRegion = makeBufferImageCopy(vkExtent, colorSRL);
		ctx.vkd.cmdCopyImageToBuffer(primaryCmdBuffer, colorBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorBuffer.getBuffer(), 1u, &copyRegion);

		const auto preHostBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
		cmdPipelineMemoryBarrier(ctx.vkd, primaryCmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &preHostBarrier);
	}
	endCommandBuffer(ctx.vkd, primaryCmdBuffer);
	submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, primaryCmdBuffer);

	const auto resultAllocation = colorBuffer.getBufferAllocation();
	invalidateAlloc(ctx.vkd, ctx.device, resultAllocation);

	uint32_t queryResult = 0u;
	VK_CHECK(ctx.vkd.getQueryPoolResults(ctx.device, queryPool.get(), 0u, 1u, sizeof(queryResult), &queryResult, static_cast<VkDeviceSize>(sizeof(queryResult)), VK_QUERY_RESULT_WAIT_BIT));

	const auto expectedResult	= vkExtent.width * vkExtent.height * vkExtent.depth;
	const bool needsExact		= (!isInvQuery);

	if (needsExact)
	{
		if (expectedResult != queryResult)
		{
			std::ostringstream msg;
			msg << "Framebuffer size: " << vkExtent.width << "x" << vkExtent.height << "; expected query result to be " << expectedResult << " but found " << queryResult;
			return tcu::TestStatus::fail(msg.str());
		}
	}
	else
	{
		if (queryResult < expectedResult)
		{
			std::ostringstream msg;
			msg << "Framebuffer size: " << vkExtent.width << "x" << vkExtent.height << "; expected query result to be at least " << expectedResult << " but found " << queryResult;
			return tcu::TestStatus::fail(msg.str());
		}
	}

	const auto					tcuFormat		= mapVkFormat(colorFormat);
	auto&						log				= context.getTestContext().getLog();
	const tcu::Vec4				colorThreshold	(0.0f, 0.0f, 0.0f, 0.0f); // Expect exact color result.
	tcu::ConstPixelBufferAccess	resultAccess	(tcuFormat, fbExtent, resultAllocation.getHostPtr());

	if (!tcu::floatThresholdCompare(log, "Result", "", getFlatColor(), resultAccess, colorThreshold, tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Unexpected results in color buffer -- check log for details");

	return tcu::TestStatus::pass("Pass");
}

} // anonymous namespace

tcu::TestCaseGroup* createFragInvocationTests (tcu::TestContext& testContext)
{
	using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

	GroupPtr mainGroup (new tcu::TestCaseGroup(testContext, "frag_invocations", "Test implementations do not optimize out fragment shader invocations"));

	for (const auto queryType : { QueryType::OCCLUSION, QueryType::INVOCATIONS })
	{
		const auto groupName = getQueryTypeName(queryType);
		GroupPtr queryTypeGroup (new tcu::TestCaseGroup(testContext, groupName.c_str(), ""));

		for (const auto secondaryCase : { false, true })
		{
			const auto testName = (secondaryCase ? "secondary" : "primary");
			const TestParams params { queryType, secondaryCase };
			addFunctionCaseWithPrograms(queryTypeGroup.get(), testName, "", checkSupport, initPrograms, testInvocations, params);
		}

		mainGroup->addChild(queryTypeGroup.release());
	}

	return mainGroup.release();
}

} // QueryPool
} // vkt

