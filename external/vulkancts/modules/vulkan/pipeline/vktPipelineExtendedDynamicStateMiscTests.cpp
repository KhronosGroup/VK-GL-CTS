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
 * \brief Extended dynamic state misc tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineExtendedDynamicStateMiscTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuImageCompare.hpp"

#include "deUniquePtr.hpp"

#include <sstream>
#include <vector>
#include <memory>
#include <utility>

namespace vkt
{
namespace pipeline
{

namespace
{

using namespace vk;

constexpr uint32_t kVertexCount = 4u;

void checkDynamicRasterizationSamplesSupport (Context& context)
{
#ifndef CTS_USES_VULKANSC
	if (!context.getExtendedDynamicState3FeaturesEXT().extendedDynamicState3RasterizationSamples)
		TCU_THROW(NotSupportedError, "extendedDynamicState3RasterizationSamples not supported");
#else
	DE_UNREF(context);
	TCU_THROW(NotSupportedError, "extendedDynamicState3RasterizationSamples not supported");
#endif // CTS_USES_VULKANSC
}

void sampleShadingWithDynamicSampleCountSupport (Context& context, PipelineConstructionType pipelineConstructionType)
{
	checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), pipelineConstructionType);
	checkDynamicRasterizationSamplesSupport(context);
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS);
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);
}

void initFullScreenQuadVertexProgram (vk::SourceCollections& programCollection, const char* name)
{
	std::ostringstream vert;
	vert
		<< "#version 460\n"
		<< "vec2 positions[" << kVertexCount << "] = vec2[](\n"
		<< "    vec2(-1.0, -1.0),\n"
		<< "    vec2(-1.0,  1.0),\n"
		<< "    vec2( 1.0, -1.0),\n"
		<< "    vec2( 1.0,  1.0)\n"
		<< ");\n"
		<< "void main (void) {\n"
		<< "    gl_Position = vec4(positions[gl_VertexIndex % " << kVertexCount << "], 0.0, 1.0);\n"
		<< "}\n"
		;
	programCollection.glslSources.add(name) << glu::VertexSource(vert.str());
}

void initBlueAndAtomicCounterFragmentProgram (vk::SourceCollections& programCollection, const char* name)
{
	std::ostringstream frag;
	frag
		<< "#version 460\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "layout (set=0, binding=0) buffer InvocationCounterBlock { uint invocations; } counterBuffer;\n"
		<< "void main (void) {\n"
		<< "    uint sampleId = gl_SampleID;\n" // Enable sample shading for shader objects by reading gl_SampleID
		<< "    atomicAdd(counterBuffer.invocations, 1u);\n"
		<< "    outColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
		<< "}\n"
		;
	programCollection.glslSources.add(name) << glu::FragmentSource(frag.str());
}

void sampleShadingWithDynamicSampleCountPrograms (vk::SourceCollections& programCollection, PipelineConstructionType)
{
	initFullScreenQuadVertexProgram(programCollection, "vert");
	initBlueAndAtomicCounterFragmentProgram(programCollection, "frag");
}

void verifyValueInRange (uint32_t value, uint32_t minValue, uint32_t maxValue, const char* valueDesc)
{
	if (value < minValue || value > maxValue)
	{
		std::ostringstream msg;
		msg << "Unexpected value found for " << valueDesc << ": " << value << " not in range [" << minValue << ", " << maxValue << "]";
		TCU_FAIL(msg.str());
	}
}
/*
 * begin cmdbuf
 * bind pipeline with sample shading disabled
 * call vkCmdSetRasterizationSamplesEXT(samples > 1)
 * draw
 * bind pipeline with sample shading enabled
 * draw
 * sample shading should work for both draws with the expected number of samples
 *
 * Each draw will use one half of the framebuffer, controlled by the viewport and scissor.
 */
tcu::TestStatus sampleShadingWithDynamicSampleCount (Context& context, PipelineConstructionType constructionType)
{
	const auto			ctx					= context.getContextCommonData();
	const tcu::IVec3	fbExtent			(2, 2, 1);
	const auto			vkExtent			= makeExtent3D(fbExtent);
	const auto			colorFormat			= VK_FORMAT_R8G8B8A8_UNORM;
	const auto			colorUsage			= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const auto			descriptorType		= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	const auto			descriptorStages	= VK_SHADER_STAGE_FRAGMENT_BIT;
	const auto			kNumDraws			= 2u;
	const auto			bindPoint			= VK_PIPELINE_BIND_POINT_GRAPHICS;
	const auto			colorSRR			= makeDefaultImageSubresourceRange();
	const auto			kMultiSampleCount	= VK_SAMPLE_COUNT_4_BIT;
	const auto			kSingleSampleCount	= VK_SAMPLE_COUNT_1_BIT;
	const tcu::Vec4		clearColor			(0.0f, 0.0f, 0.0f, 0.0f);
	const tcu::Vec4		geomColor			(0.0f, 0.0f, 1.0f, 1.0f); // Must match frag shader.
	const auto			topology			= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	// Color buffers.
	ImageWithBuffer colorBuffer		(ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, colorUsage, VK_IMAGE_TYPE_2D, colorSRR, 1u, kMultiSampleCount);
	ImageWithBuffer resolveBuffer	(ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, colorUsage, VK_IMAGE_TYPE_2D, colorSRR, 1u, kSingleSampleCount);

	// Counter buffers.
	using BufferPtr = std::unique_ptr<BufferWithMemory>;
	using BufferVec = std::vector<BufferPtr>;

	const auto			counterBufferSize	= static_cast<VkDeviceSize>(sizeof(uint32_t));
	const auto			counterBufferInfo	= makeBufferCreateInfo(counterBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	BufferVec counterBuffers;

	for (uint32_t drawIdx = 0u; drawIdx < kNumDraws; ++drawIdx)
	{
		BufferPtr			counterBuffer		(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, counterBufferInfo, MemoryRequirement::HostVisible));
		auto&				counterBufferAlloc	= counterBuffer->getAllocation();
		void*				counterBufferPtr	= counterBufferAlloc.getHostPtr();

		deMemset(counterBufferPtr, 0, static_cast<size_t>(counterBufferSize));
		flushAlloc(ctx.vkd, ctx.device, counterBufferAlloc);

		counterBuffers.emplace_back(std::move(counterBuffer));
	}

	// Descriptor set layout, pool and set.
	DescriptorSetLayoutBuilder setLayoutbuilder;
	setLayoutbuilder.addSingleBinding(descriptorType, descriptorStages);
	const auto setLayout = setLayoutbuilder.build(ctx.vkd, ctx.device);

	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(descriptorType, kNumDraws);
	const auto descriptorPool = poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, kNumDraws);

	using DescriptorSetVec = std::vector<Move<VkDescriptorSet>>;
	DescriptorSetVec descriptorSets;

	for (uint32_t drawIdx = 0u; drawIdx < kNumDraws; ++drawIdx)
	{
		descriptorSets.emplace_back(makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *setLayout));

		DescriptorSetUpdateBuilder updateBuilder;
		const auto counterBufferDescriptorInfo = makeDescriptorBufferInfo(counterBuffers.at(drawIdx)->get(), 0ull, counterBufferSize);
		updateBuilder.writeSingle(*descriptorSets.back(), DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType, &counterBufferDescriptorInfo);
		updateBuilder.update(ctx.vkd, ctx.device);
	}

	// Render pass and framebuffer.
	const std::vector<VkAttachmentDescription> attachmentDescs
	{
		// Multisample attachment.
		makeAttachmentDescription(
			0u,
			colorFormat,
			kMultiSampleCount,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),

		// Resolve attachment.
		makeAttachmentDescription(
			0u,
			colorFormat,
			kSingleSampleCount,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
	};

	const auto colorAttRef			= makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	const auto resolveAttRef		= makeAttachmentReference(1u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	const auto subpassDescription	= makeSubpassDescription(0u, bindPoint, 0u, nullptr, 1u, &colorAttRef, &resolveAttRef, nullptr, 0u, nullptr);

	const VkRenderPassCreateInfo renderPassCreateInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	//	VkStructureType					sType;
		nullptr,									//	const void*						pNext;
		0u,											//	VkRenderPassCreateFlags			flags;
		de::sizeU32(attachmentDescs),				//	uint32_t						attachmentCount;
		de::dataOrNull(attachmentDescs),			//	const VkAttachmentDescription*	pAttachments;
		1u,											//	uint32_t						subpassCount;
		&subpassDescription,						//	const VkSubpassDescription*		pSubpasses;
		0u,											//	uint32_t						dependencyCount;
		nullptr,									//	const VkSubpassDependency*		pDependencies;
	};
	auto renderPass = RenderPassWrapper(constructionType, ctx.vkd, ctx.device, &renderPassCreateInfo);

	const std::vector<VkImage>		images		{ colorBuffer.getImage(), resolveBuffer.getImage() };
	const std::vector<VkImageView>	imageViews	{ colorBuffer.getImageView(), resolveBuffer.getImageView() };
	renderPass.createFramebuffer(ctx.vkd, ctx.device, de::sizeU32(imageViews), de::dataOrNull(images), de::dataOrNull(imageViews), vkExtent.width, vkExtent.height);

	// Pipelines.
	const auto& binaries		= context.getBinaryCollection();
	const auto& vertModule		= ShaderWrapper(ctx.vkd, ctx.device, binaries.get("vert"));
	const auto& fragModule		= ShaderWrapper(ctx.vkd, ctx.device, binaries.get("frag"));

	const std::vector<VkDynamicState>		dynamicStates		{
#ifndef CTS_USES_VULKANSC
		VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT,
#endif // CTS_USES_VULKANSC
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_VIEWPORT,
	};

	const VkPipelineDynamicStateCreateInfo	dynamicStateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,												//	const void*							pNext;
		0u,														//	VkPipelineDynamicStateCreateFlags	flags;
		de::sizeU32(dynamicStates),								//	uint32_t							dynamicStateCount;
		de::dataOrNull(dynamicStates),							//	const VkDynamicState*				pDynamicStates;
	};

	const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructureConst();

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	//	VkStructureType							sType;
		nullptr,													//	const void*								pNext;
		0u,															//	VkPipelineMultisampleStateCreateFlags	flags;
		VK_SAMPLE_COUNT_64_BIT,										//	VkSampleCountFlagBits					rasterizationSamples;
		VK_FALSE,													//	VkBool32								sampleShadingEnable;
		1.0f,														//	float									minSampleShading;
		nullptr,													//	const VkSampleMask*						pSampleMask;
		VK_FALSE,													//	VkBool32								alphaToCoverageEnable;
		VK_FALSE,													//	VkBool32								alphaToOneEnable;
	};

	const std::vector<VkViewport>	staticViewports		(1u, makeViewport(0u, 0u));
	const std::vector<VkRect2D>		staticScissors		(1u, makeRect2D(0u, 0u));
	const PipelineLayoutWrapper		pipelineLayout		(constructionType, ctx.vkd, ctx.device, *setLayout);
	const auto						renderArea			= makeRect2D(fbExtent);
	const int						halfWidth			= fbExtent.x() / 2;
	const uint32_t					halfWidthU			= static_cast<uint32_t>(halfWidth);
	const float						halfWidthF			= static_cast<float>(halfWidth);
	const float						heightF				= static_cast<float>(vkExtent.height);
	const std::vector<VkRect2D>		dynamicScissors		{ makeRect2D(0, 0, halfWidthU, vkExtent.height), makeRect2D(halfWidth, 0, halfWidthU, vkExtent.height) };
	const std::vector<VkViewport>	dynamicViewports
	{
		makeViewport(0.0f, 0.0f, halfWidthF, heightF, 0.0f, 1.0f),
		makeViewport(halfWidthF, 0.0f, halfWidthF, heightF, 0.0f, 1.0f),
	};

	using WrapperPtr = std::unique_ptr<GraphicsPipelineWrapper>;
	using WrapperVec = std::vector<WrapperPtr>;

	WrapperVec wrappers;

	for (const auto sampleShadingEnable : { false, true })
	{
		multisampleStateCreateInfo.sampleShadingEnable = sampleShadingEnable;

		WrapperPtr pipelineWrapper(new GraphicsPipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device, context.getDeviceExtensions(), constructionType));
		pipelineWrapper->setDefaultTopology(topology)
						.setDefaultRasterizationState()
						.setDefaultColorBlendState()
						.setDynamicState(&dynamicStateInfo)
						.setupVertexInputState(&vertexInputStateCreateInfo)
						.setupPreRasterizationShaderState(
							staticViewports,
							staticScissors,
							pipelineLayout,
							*renderPass,
							0u,
							vertModule)
						.setupFragmentShaderState(
							pipelineLayout,
							*renderPass,
							0u,
							fragModule,
							nullptr,
							&multisampleStateCreateInfo)
						.setupFragmentOutputState(
							*renderPass,
							0u,
							nullptr,
							&multisampleStateCreateInfo)
						.setMonolithicPipelineLayout(pipelineLayout)
						.buildPipeline();

		wrappers.emplace_back(std::move(pipelineWrapper));
	}

	CommandPoolWithBuffer cmd (ctx.vkd, ctx.device, ctx.qfIndex);
	const auto cmdBuffer = cmd.cmdBuffer.get();

	beginCommandBuffer(ctx.vkd, cmdBuffer);
	renderPass.begin(ctx.vkd, cmdBuffer, renderArea, clearColor);
	for (uint32_t drawIdx = 0u; drawIdx < kNumDraws; ++drawIdx)
	{
		wrappers.at(drawIdx)->bind(cmdBuffer);
		if (drawIdx == 0u)
		{
#ifndef CTS_USES_VULKANSC
			ctx.vkd.cmdSetRasterizationSamplesEXT(cmdBuffer, kMultiSampleCount);
#else
			DE_ASSERT(false);
#endif // CTS_USES_VULKANSC
		}
#ifndef CTS_USES_VULKANSC
		if (isConstructionTypeShaderObject(constructionType))
		{
			ctx.vkd.cmdSetScissorWithCount(cmdBuffer, 1u, &dynamicScissors.at(drawIdx));
			ctx.vkd.cmdSetViewportWithCount(cmdBuffer, 1u, &dynamicViewports.at(drawIdx));
		}
		else
#endif // CTS_USES_VULKANSC
		{
			ctx.vkd.cmdSetScissor(cmdBuffer, 0u, 1u, &dynamicScissors.at(drawIdx));
			ctx.vkd.cmdSetViewport(cmdBuffer, 0u, 1u, &dynamicViewports.at(drawIdx));
		}
		ctx.vkd.cmdBindDescriptorSets(cmdBuffer, bindPoint, *pipelineLayout, 0u, 1u, &descriptorSets.at(drawIdx).get(), 0u, nullptr);
		ctx.vkd.cmdDraw(cmdBuffer, kVertexCount, 1u, 0u, 0u);
	}
	renderPass.end(ctx.vkd, cmdBuffer);
	copyImageToBuffer(
		ctx.vkd,
		cmdBuffer,
		resolveBuffer.getImage(),
		resolveBuffer.getBuffer(),
		fbExtent.swizzle(0, 1),
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		1u,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	endCommandBuffer(ctx.vkd, cmdBuffer);
	submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

	// Verify resolve buffer and counter buffers.
	auto& log = context.getTestContext().getLog();
	{
		const tcu::Vec4	threshold			(0.0f, 0.0f, 0.0f, 0.0f); // Expect exact results.
		const auto		tcuFormat			= mapVkFormat(colorFormat);
		const auto&		resolveBufferAlloc	= resolveBuffer.getBufferAllocation();
		const auto		resolveBufferData	= resolveBufferAlloc.getHostPtr();

		invalidateAlloc(ctx.vkd, ctx.device, resolveBufferAlloc);
		const tcu::ConstPixelBufferAccess resultAccess(tcuFormat, fbExtent, resolveBufferData);

		if (!tcu::floatThresholdCompare(log, "Result", "", geomColor, resultAccess, threshold, tcu::COMPARE_LOG_ON_ERROR))
			return tcu::TestStatus::fail("Unexpected color buffer results -- check log for details");
	}
	{
		std::vector<uint32_t> counterResults (kNumDraws, 0u);
		for (uint32_t drawIdx = 0u; drawIdx < kNumDraws; ++drawIdx)
		{
			const auto& bufferAlloc = counterBuffers.at(drawIdx)->getAllocation();
			invalidateAlloc(ctx.vkd, ctx.device, bufferAlloc);
			deMemcpy(&counterResults.at(drawIdx), bufferAlloc.getHostPtr(), sizeof(counterResults.at(drawIdx)));
			log << tcu::TestLog::Message << "Draw " << drawIdx << ": " << counterResults.at(drawIdx) << " invocations" << tcu::TestLog::EndMessage;
		}

		// The first result is run without sample shading enabled, so it can have any value from 1 to 4 invocations per pixel.
		// The second result runs with sample shading enabled, so it must have exactly 4 invocations per pixel.
		const uint32_t minInvs = (vkExtent.width * vkExtent.height) / 2u;
		const uint32_t maxInvs = minInvs * static_cast<uint32_t>(kMultiSampleCount);

		verifyValueInRange(counterResults.at(0u), minInvs, maxInvs, "invocation counter without sample shading");
		verifyValueInRange(counterResults.at(1u), maxInvs, maxInvs, "invocation counter with sample shading");
	}

	return tcu::TestStatus::pass("Pass");
}

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

} // anonymous namespace

tcu::TestCaseGroup* createExtendedDynamicStateMiscTests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType)
{
	GroupPtr miscGroup (new tcu::TestCaseGroup(testCtx, "misc", "Extended dynamic state misc tests"));
	addFunctionCaseWithPrograms(miscGroup.get(), "sample_shading_dynamic_sample_count", "", sampleShadingWithDynamicSampleCountSupport, sampleShadingWithDynamicSampleCountPrograms, sampleShadingWithDynamicSampleCount, pipelineConstructionType);
	return miscGroup.release();
}

} // pipeline
} // vkt
