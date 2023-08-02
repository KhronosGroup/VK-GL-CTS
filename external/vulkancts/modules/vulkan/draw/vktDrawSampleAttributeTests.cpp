/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022,2023 The Khronos Group Inc.
 * Copyright (c) 2022,2023 Valve Corporation.
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
 * \file vktDrawSampleAttributeTests.cpp
 * \brief Tests for the sample interpolation attribute
 *//*--------------------------------------------------------------------*/

#include "tcuStringTemplate.hpp"
#include "vkCmdUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vktDrawBaseClass.hpp"
#include "vktTestGroupUtil.hpp"
#include "tcuVectorUtil.hpp"

#include "vkPipelineConstructionUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkDefs.hpp"
#include "tcuImageCompare.hpp"
#include "vkImageUtil.hpp"
#include "deStringUtil.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"

namespace vkt
{
namespace Draw
{
namespace
{

using namespace vk;

enum class Trigger
{
	SAMPLE_ID_STATIC_USE = 0,
	SAMPLE_POSITION_STATIC_USE,
	SAMPLE_DECORATION_DYNAMIC_USE,
};

struct TestParameters
{
	const SharedGroupParams	general;

	// Test case variant on the fragment shader.
	Trigger					trigger;
};

/*
 * Test that sample interpolation correctly enables sample shading at a rate of 1.0
 */
class SampleShadingSampleAttributeTestCase : public vkt::TestCase
{
public:
	SampleShadingSampleAttributeTestCase    (tcu::TestContext& context, const std::string& name, const std::string& description, const TestParameters& params);
	void            initPrograms            (SourceCollections& programCollection) const override;
	TestInstance*   createInstance          (Context& context) const override;
	void            checkSupport            (Context& context) const override;

private:
	const TestParameters m_params;
};

class SampleShadingSampleAttributeTestInstance : public vkt::TestInstance
{
public:
	SampleShadingSampleAttributeTestInstance (Context& context, const TestParameters& params);
	tcu::TestStatus iterate                  (void) override;

private:
	const TestParameters m_params;

	static constexpr uint32_t				width			= 4u;
	static constexpr uint32_t				height			= 4u;
	static constexpr VkSampleCountFlagBits	sampleCount		= VK_SAMPLE_COUNT_4_BIT;
	static constexpr uint32_t				expectedCounter	= sampleCount * width * height;
};

SampleShadingSampleAttributeTestCase::SampleShadingSampleAttributeTestCase(tcu::TestContext& context, const std::string& name, const std::string& description, const TestParameters& params):
	vkt::TestCase(context, name, description), m_params(params) { }

void SampleShadingSampleAttributeTestCase::checkSupport(Context& context) const
{
	if (m_params.general->useDynamicRendering)
		context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS);
}

void SampleShadingSampleAttributeTestCase::initPrograms(SourceCollections& collection) const
{
	const bool sampleFragInput			= (m_params.trigger == Trigger::SAMPLE_DECORATION_DYNAMIC_USE);
	const bool declareSampleId			= (m_params.trigger == Trigger::SAMPLE_ID_STATIC_USE);
	const bool declareSamplePosition	= (m_params.trigger == Trigger::SAMPLE_POSITION_STATIC_USE);

	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "vec2 positions[3] = vec2[](\n"
			<< "    vec2(-1.0, -1.0),\n"
			<< "    vec2(3.0, -1.0),\n"
			<< "    vec2(-1.0, 3.0)\n"
			<< ");\n"
			<< (sampleFragInput ? "layout (location = 0) out float verify;\n" : "")
			<< "void main() {\n"
			<< "    const uint triIdx     = gl_VertexIndex / 3u;\n"											// In practice this will always be zero.
			<< "    const uint triVertIdx = gl_VertexIndex % 3u;\n"
			<< "    gl_Position = vec4(positions[triVertIdx], 0.0, 1.0);\n"
			<< (sampleFragInput ? "    verify = float(triIdx) + float(triVertIdx) / 16.0 + 0.75;\n" : "")	// In practice a number between 0.75 and 1.0.
			<< "}\n";
		collection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "layout (location = 0) out vec4 outColor;\n"
			<< (sampleFragInput ? "layout (location = 0) sample in float verify;\n" : "")
			<< "layout (std430, binding = 0) buffer Output {\n"
			<< "    uint invocationCount;\n"
			<< "} buf;\n"
			<< "void main() {\n"
			<< (declareSampleId ? "    gl_SampleID;\n" : "")
			<< (declareSamplePosition ? "    gl_SamplePosition;\n" : "")
			<< "    uint one   = " << (sampleFragInput ? "uint(ceil(verify))" : "1") << ";\n"
			<< "    uint index = atomicAdd(buf.invocationCount, one);\n"
			<< "    outColor = vec4(float(one), 1.0, 0.0, 1.0);\n"
			<< "}\n";
		collection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
}

TestInstance* SampleShadingSampleAttributeTestCase::createInstance(Context& context) const
{
	return new SampleShadingSampleAttributeTestInstance(context, m_params);
}

SampleShadingSampleAttributeTestInstance::SampleShadingSampleAttributeTestInstance(Context& context, const TestParameters& params) : vkt::TestInstance(context), m_params(params) { }

tcu::TestStatus SampleShadingSampleAttributeTestInstance::iterate(void)
{
	const auto ctx = m_context.getContextCommonData();

	// Verification buffer.
	const auto			bufferSize		= static_cast<VkDeviceSize>(sizeof(uint32_t));
	BufferWithMemory	buffer			(ctx.vkd, ctx.device, ctx.allocator, makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);
	auto&				bufferAlloc		= buffer.getAllocation();
	void*				bufferData		= bufferAlloc.getHostPtr();

	deMemset(bufferData, 0, static_cast<size_t>(bufferSize));
	flushAlloc(ctx.vkd, ctx.device, bufferAlloc);

	// Color attachment.
	const auto imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
	const auto imageExtent = makeExtent3D(width, height, 1u);

	const std::vector<VkViewport>	viewports	{ makeViewport(imageExtent) };
	const std::vector<VkRect2D>		scissors	{ makeRect2D(imageExtent) };

	const auto subresourceRange	= makeDefaultImageSubresourceRange();
	const auto imageUsage		= static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	const VkImageCreateInfo imageCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		//	VkStructureType				sType;
		nullptr,									//	const void*					pNext;
		0u,											//	VkImageCreateFlags			flags;
		VK_IMAGE_TYPE_2D,							//	VkImageType					imageType;
		imageFormat,								//	VkFormat					format;
		imageExtent,								//	VkExtent3D					extent;
		1u,											//	deUint32					mipLevels;
		1u,											//	deUint32					arrayLayers;
		sampleCount,								//	VkSampleCountFlagBits		samples;
		VK_IMAGE_TILING_OPTIMAL,					//	VkImageTiling				tiling;
		imageUsage,									//	VkImageUsageFlags			usage;
		VK_SHARING_MODE_EXCLUSIVE,					//	VkSharingMode				sharingMode;
		0u,											//	deUint32					queueFamilyIndexCount;
		nullptr,									//	const deUint32*				pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,					//	VkImageLayout				initialLayout;
	};

	ImageWithMemory colorAttachment		(ctx.vkd, ctx.device, ctx.allocator, imageCreateInfo, MemoryRequirement::Any);
	const auto		colorAttachmentView	= makeImageView(ctx.vkd, ctx.device, colorAttachment.get(), VK_IMAGE_VIEW_TYPE_2D, imageFormat, subresourceRange);

	// Structures used for renderpasses and dynamic rendering.
	RenderPassCreateInfo renderPassCreateInfo;

	const VkAttachmentReference	colorAttachmentReference =
	{
		0,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	renderPassCreateInfo.addAttachment(AttachmentDescription(imageFormat,
															 sampleCount,
															 VK_ATTACHMENT_LOAD_OP_CLEAR,
															 VK_ATTACHMENT_STORE_OP_STORE,
															 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
															 VK_ATTACHMENT_STORE_OP_DONT_CARE,
															 VK_IMAGE_LAYOUT_UNDEFINED,
															 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

	renderPassCreateInfo.addSubpass(SubpassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS,
													   0,
													   0,
													   nullptr,
													   1,
													   &colorAttachmentReference,
													   nullptr,
													   AttachmentReference(),
													   0,
													   nullptr));

	// Render pass and framebuffer.
	const auto renderPass		= createRenderPass(ctx.vkd, ctx.device, &renderPassCreateInfo);
	const auto framebuffer		= makeFramebuffer(ctx.vkd, ctx.device, renderPass.get(), colorAttachmentView.get(), imageExtent.width, imageExtent.height);
	const auto clearValueColor	= makeClearValueColor(tcu::Vec4(1.0f, 1.0f, 1.0f, 1.0f));

#ifndef CTS_USES_VULKANSC
	const VkRenderingAttachmentInfoKHR colorAttachments =
	{
		VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,	// VkStructureType						sType;
		nullptr,											// const void*							pNext;
		colorAttachmentView.get(),							// VkImageView							imageView;
		VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,				// VkImageLayout						imageLayout;
		VK_RESOLVE_MODE_NONE,								// VkResolveModeFlagBits				resolveMode;
		VK_NULL_HANDLE,										// VkImageView							resolveImageView;
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout						resolveImageLayout;
		VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp					loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp					storeOp;
		clearValueColor										// VkClearValue							clearValue;
	};

	const auto renderInfoFlags = (
		(m_params.general->useDynamicRendering
		 && !m_params.general->secondaryCmdBufferCompletelyContainsDynamicRenderpass
		 && m_params.general->useSecondaryCmdBuffer)
		 ? static_cast<VkRenderingFlags>(VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT)
		 : 0u);

	const VkRenderingInfoKHR	renderInfo		=
	{
		VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,	// VkStructureType                     sType;
		0,										// const void*                         pNext;
		renderInfoFlags,						// VkRenderingFlags                    flags;
		scissors.at(0),							// VkRect2D                            renderArea;
		1,										// uint32_t                            layerCount;
		0,										// uint32_t                            viewMask;
		1,										// uint32_t                            colorAttachmentCount;
		&colorAttachments,						// const VkRenderingAttachmentInfo*    pColorAttachments;
		VK_NULL_HANDLE,							// const VkRenderingAttachmentInfo*    pDepthAttachment;
		VK_NULL_HANDLE							// const VkRenderingAttachmentInfo*    pStencilAttachment;
	};

	const VkCommandBufferInheritanceRenderingInfoKHR inheritanceRenderingInfo
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR,	// VkStructureType					sType;
		nullptr,															// const void*						pNext;
		0,																	// VkRenderingFlagsKHR				flags;
		0u,																	// uint32_t							viewMask;
		1u,																	// uint32_t							colorAttachmentCount;
		&imageFormat,														// const VkFormat*					pColorAttachmentFormats;
		VK_FORMAT_UNDEFINED,												// VkFormat							depthAttachmentFormat;
		VK_FORMAT_UNDEFINED,												// VkFormat							stencilAttachmentFormat;
		sampleCount,														// VkSampleCountFlagBits			rasterizationSamples;
	};

	const VkCommandBufferInheritanceInfo bufferInheritanceInfo
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,					// VkStructureType					sType;
		&inheritanceRenderingInfo,											// const void*						pNext;
		VK_NULL_HANDLE,														// VkRenderPass						renderPass;
		0u,																	// deUint32							subpass;
		VK_NULL_HANDLE,														// VkFramebuffer					framebuffer;
		VK_FALSE,															// VkBool32							occlusionQueryEnable;
		(VkQueryControlFlags)0u,											// VkQueryControlFlags				queryFlags;
		(VkQueryPipelineStatisticFlags)0u									// VkQueryPipelineStatisticFlags	pipelineStatistics;
	};
#endif

	DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);

	const auto descriptorSetLayout    = layoutBuilder.build(ctx.vkd, ctx.device);
	const auto graphicsPipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, descriptorSetLayout.get());

	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	const auto descriptorPool = poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);
	const auto descriptorSetBuffer		= makeDescriptorSet(ctx.vkd, ctx.device, descriptorPool.get(), descriptorSetLayout.get());

	// Update descriptor sets.
	const auto bufferInfo = makeDescriptorBufferInfo(buffer.get(), 0ull, bufferSize);

	DescriptorSetUpdateBuilder updater;
	updater.writeSingle(descriptorSetBuffer.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo);
	updater.update(ctx.vkd, ctx.device);

	const auto vtxshader = createShaderModule(ctx.vkd, ctx.device, m_context.getBinaryCollection().get("vert"));
	const auto frgshader = createShaderModule(ctx.vkd, ctx.device, m_context.getBinaryCollection().get("frag"));

	const VkPipelineVertexInputStateCreateInfo vertexInputState = initVulkanStructure();

	// Set up a default multisample state that doesn't use sample shading and with minSampleShading set to 0.0.
	VkPipelineMultisampleStateCreateInfo multisampling = initVulkanStructure();
	multisampling.sampleShadingEnable	= VK_FALSE;
	multisampling.minSampleShading		= 0.0;
	multisampling.rasterizationSamples	= sampleCount;

	const auto pass		= (m_params.general->useDynamicRendering ? VK_NULL_HANDLE : renderPass.get());
	const auto pipeline	= makeGraphicsPipeline(
		ctx.vkd,								// const DeviceInterface&                 vk
		ctx.device,								// const VkDevice                         device
		graphicsPipelineLayout.get(),			// const VkPipelineLayout                 pipelineLayout
		*vtxshader,								// const VkShaderModule                   vertexShaderModule
		VK_NULL_HANDLE,							// const VkShaderModule                   tessellationControlModule
		VK_NULL_HANDLE,							// const VkShaderModule                   tessellationEvalModule
		VK_NULL_HANDLE,							// const VkShaderModule                   geometryShaderModule
		*frgshader,								// const VkShaderModule                   fragmentShaderModule
		pass,									// const VkRenderPass                     renderPass
		viewports,								// const std::vector<VkViewport>&         viewports
		scissors,								// const std::vector<VkRect2D>&           scissors
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology              topology
		0u,										// const deUint32                         subpass
		0u,										// const deUint32                         patchControlPoints
		&vertexInputState,						// VkPipelineVertexInputStateCreateInfo   *vertexInputStateCreateInfo
		VK_NULL_HANDLE,							// VkPipelineRasterizationStateCreateInfo *rasterizationStateCreateInfo
		&multisampling							// VkPipelineMultisampleStateCreateInfo   *multisampleStateCreateInfo
	);

	const auto commandPool				= createCommandPool(ctx.vkd, ctx.device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, ctx.qfIndex);
	const auto primaryCmdBufferPtr		= allocateCommandBuffer(ctx.vkd, ctx.device, commandPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto primaryCmdBuffer			= *primaryCmdBufferPtr;
#ifndef CTS_USES_VULKANSC
	const auto secondaryCmdBufferPtr	= allocateCommandBuffer(ctx.vkd, ctx.device, commandPool.get(), VK_COMMAND_BUFFER_LEVEL_SECONDARY);
	const auto secondaryCmdBuffer		= *secondaryCmdBufferPtr;
#endif // CTS_USES_VULKANSC

	beginCommandBuffer(ctx.vkd, primaryCmdBuffer);

	if (m_params.general->useDynamicRendering)
	{
#ifndef CTS_USES_VULKANSC
		// Transition color attachment to the proper layout.
		const auto initialBarrier = makeImageMemoryBarrier(
			0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
			colorAttachment.get(), subresourceRange);
		cmdPipelineImageMemoryBarrier(ctx.vkd, primaryCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, &initialBarrier);

		if (m_params.general->secondaryCmdBufferCompletelyContainsDynamicRenderpass)
		{
			const VkCommandBufferUsageFlags	usageFlags				= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			const VkCommandBufferBeginInfo	commandBufBeginParams	=
			{
				VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType					sType;
				nullptr,												// const void*						pNext;
				usageFlags,												// VkCommandBufferUsageFlags		flags;
				&bufferInheritanceInfo
			};

			ctx.vkd.beginCommandBuffer(secondaryCmdBuffer, &commandBufBeginParams);
			ctx.vkd.cmdBeginRendering(secondaryCmdBuffer, &renderInfo);
			ctx.vkd.cmdBindDescriptorSets(secondaryCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout.get(), 0u, 1, &descriptorSetBuffer.get(), 0u, nullptr);
			ctx.vkd.cmdBindPipeline(secondaryCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
			ctx.vkd.cmdDraw(secondaryCmdBuffer, 3u, 1u, 0u, 0u);
			ctx.vkd.cmdEndRendering(secondaryCmdBuffer);
			endCommandBuffer(ctx.vkd, secondaryCmdBuffer);
			ctx.vkd.cmdExecuteCommands(primaryCmdBuffer, 1, &secondaryCmdBuffer);
		}
		else if (!m_params.general->secondaryCmdBufferCompletelyContainsDynamicRenderpass && m_params.general->useSecondaryCmdBuffer)
		{
			const VkCommandBufferUsageFlags	usageFlags				= (VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);
			const VkCommandBufferBeginInfo	commandBufBeginParams	=
			{
				VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType					sType;
				nullptr,												// const void*						pNext;
				usageFlags,												// VkCommandBufferUsageFlags		flags;
				&bufferInheritanceInfo
			};

			ctx.vkd.cmdBeginRendering(primaryCmdBuffer, &renderInfo);
			ctx.vkd.beginCommandBuffer(secondaryCmdBuffer, &commandBufBeginParams);
			ctx.vkd.cmdBindDescriptorSets(secondaryCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout.get(), 0u, 1, &descriptorSetBuffer.get(), 0u, nullptr);
			ctx.vkd.cmdBindPipeline(secondaryCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
			ctx.vkd.cmdDraw(secondaryCmdBuffer, 3, 1, 0, 0);
			endCommandBuffer(ctx.vkd, secondaryCmdBuffer);
			ctx.vkd.cmdExecuteCommands(primaryCmdBuffer, 1, &secondaryCmdBuffer);
			ctx.vkd.cmdEndRendering(primaryCmdBuffer);
		}
		else
		{
			ctx.vkd.cmdBeginRendering(primaryCmdBuffer, &renderInfo);
			ctx.vkd.cmdBindDescriptorSets(primaryCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout.get(), 0u, 1, &descriptorSetBuffer.get(), 0u, nullptr);
			ctx.vkd.cmdBindPipeline(primaryCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
			ctx.vkd.cmdDraw(primaryCmdBuffer, 3, 1, 0, 0);
			ctx.vkd.cmdEndRendering(primaryCmdBuffer);
		}
#else
	DE_ASSERT(false);
#endif
	}
	else
	{
		const VkRenderPassBeginInfo renderPassBeginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,			// VkStructureType         sType;
			nullptr,											// const void*             pNext;
			*renderPass,										// VkRenderPass            renderPass;
			*framebuffer,										// VkFramebuffer           framebuffer;
			scissors.at(0),										// VkRect2D                renderArea;
			1,													// uint32_t                clearValueCount;
			&clearValueColor,									// const VkClearValue*     pClearValues;
		};
		ctx.vkd.cmdBeginRenderPass(primaryCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		ctx.vkd.cmdBindDescriptorSets(primaryCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout.get(), 0u, 1, &descriptorSetBuffer.get(), 0u, nullptr);
		ctx.vkd.cmdBindPipeline(primaryCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
		ctx.vkd.cmdDraw(primaryCmdBuffer, 3, 1, 0, 0);
		ctx.vkd.cmdEndRenderPass(primaryCmdBuffer);
	}

	const VkBufferMemoryBarrier renderBufferBarrier = makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, buffer.get(), 0ull, bufferSize);
	cmdPipelineBufferMemoryBarrier(ctx.vkd, primaryCmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, &renderBufferBarrier);
	endCommandBuffer(ctx.vkd, primaryCmdBuffer);
	submitCommandsAndWait(ctx.vkd, ctx.device, m_context.getUniversalQueue(), primaryCmdBuffer);

	invalidateAlloc(ctx.vkd, ctx.device, bufferAlloc);

	uint32_t result = 0;
	deMemcpy(&result, bufferData, sizeof(result));

	if (result < expectedCounter)
	{
		std::stringstream output;
		output << "Atomic counter value lower than expected: " << result;
		return tcu::TestStatus::fail(output.str());
	}

	return tcu::TestStatus::pass("Pass");
}

} // anonoymous

tcu::TestCaseGroup* createSampleAttributeTests (tcu::TestContext& testCtx, const SharedGroupParams groupParams)
{
	const struct
	{
		Trigger		trigger;
		const char*	name;
		const char*	desc;
	} triggerCases[] =
	{
		{ Trigger::SAMPLE_DECORATION_DYNAMIC_USE,	"sample_decoration_dynamic_use",	"Dynamically use the sample decoration on a frag shader input variable"	},
		{ Trigger::SAMPLE_ID_STATIC_USE,			"sample_id_static_use",				"Declare SampleId built-in in the frag shader without using it"			},
		{ Trigger::SAMPLE_POSITION_STATIC_USE,		"sample_position_static_use",		"Declare SamplePosition built-in in the frag shader without using it"	},
	};

	de::MovePtr<tcu::TestCaseGroup> group {new tcu::TestCaseGroup{testCtx, "implicit_sample_shading", ""}};

	for (const auto& triggerCase : triggerCases)
	{
		const TestParameters params { groupParams, triggerCase.trigger };
		group->addChild(new SampleShadingSampleAttributeTestCase(testCtx, triggerCase.name, triggerCase.desc, params));
	}

	return group.release();
}

} // Draw
} // vkt
