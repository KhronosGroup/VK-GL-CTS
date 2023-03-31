/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright 2014 The Android Open Source Project
 * Copyright (c) 2015 The Khronos Group Inc.
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
 * \brief Early fragment tests
 *//*--------------------------------------------------------------------*/

#include "vktFragmentOperationsEarlyFragmentTests.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTextureUtil.hpp"

#include "deUniquePtr.hpp"
#include "deStringUtil.hpp"
#include "deMath.h"

#include <string>

namespace vkt
{
namespace FragmentOperations
{
namespace
{
using namespace vk;
using de::UniquePtr;

//! Basic 2D image.
inline VkImageCreateInfo makeImageCreateInfo (const tcu::IVec2& size, const VkFormat format, const VkImageUsageFlags usage)
{
	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,					// VkStructureType			sType;
		DE_NULL,												// const void*				pNext;
		(VkImageCreateFlags)0,									// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,										// VkImageType				imageType;
		format,													// VkFormat					format;
		makeExtent3D(size.x(), size.y(), 1),					// VkExtent3D				extent;
		1u,														// deUint32					mipLevels;
		1u,														// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,									// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,								// VkImageTiling			tiling;
		usage,													// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,								// VkSharingMode			sharingMode;
		0u,														// deUint32					queueFamilyIndexCount;
		DE_NULL,												// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout			initialLayout;
	};
	return imageParams;
}

Move<VkRenderPass> makeRenderPass (const DeviceInterface&	vk,
								   const VkDevice			device,
								   const VkFormat			colorFormat,
								   const bool				useDepthStencilAttachment,
								   const VkFormat			depthStencilFormat)
{
	return makeRenderPass(vk, device, colorFormat, useDepthStencilAttachment ? depthStencilFormat : VK_FORMAT_UNDEFINED);
}

Move<VkPipeline> makeGraphicsPipeline (const DeviceInterface&	vk,
									   const VkDevice			device,
									   const VkPipelineLayout	pipelineLayout,
									   const VkRenderPass		renderPass,
									   const VkShaderModule		vertexModule,
									   const VkShaderModule		fragmentModule,
									   const tcu::IVec2&		renderSize,
									   const bool				enableDepthTest,
									   const bool				enableStencilTest,
									   const VkStencilOp		stencilFailOp = VK_STENCIL_OP_KEEP,
									   const VkStencilOp		stencilPassOp = VK_STENCIL_OP_KEEP)
{
	const std::vector<VkViewport>			viewports					(1, makeViewport(renderSize));
	const std::vector<VkRect2D>				scissors					(1, makeRect2D(renderSize));

	const VkStencilOpState					stencilOpState				= makeStencilOpState(
		stencilFailOp,			// stencil fail
		stencilPassOp,			// depth & stencil pass
		VK_STENCIL_OP_KEEP,		// depth only fail
		VK_COMPARE_OP_EQUAL,	// compare op
		0x3,					// compare mask
		0xf,					// write mask
		1u);					// reference

	VkPipelineDepthStencilStateCreateInfo	depthStencilStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType                          sType
		DE_NULL,													// const void*                              pNext
		0u,															// VkPipelineDepthStencilStateCreateFlags   flags
		enableDepthTest ? VK_TRUE : VK_FALSE,						// VkBool32                                 depthTestEnable
		enableDepthTest ? VK_TRUE : VK_FALSE,						// VkBool32                                 depthWriteEnable
		VK_COMPARE_OP_LESS,											// VkCompareOp                              depthCompareOp
		VK_FALSE,													// VkBool32                                 depthBoundsTestEnable
		enableStencilTest ? VK_TRUE : VK_FALSE,						// VkBool32                                 stencilTestEnable
		stencilOpState,												// VkStencilOpState                         front
		stencilOpState,												// VkStencilOpState                         back
		0.0f,														// float                                    minDepthBounds
		1.0f														// float                                    maxDepthBounds
	};

	return vk::makeGraphicsPipeline(vk,										// const DeviceInterface&                        vk
									device,									// const VkDevice                                device
									pipelineLayout,							// const VkPipelineLayout                        pipelineLayout
									vertexModule,							// const VkShaderModule                          vertexShaderModule
									DE_NULL,								// const VkShaderModule                          tessellationControlModule
									DE_NULL,								// const VkShaderModule                          tessellationEvalModule
									DE_NULL,								// const VkShaderModule                          geometryShaderModule
									fragmentModule,							// const VkShaderModule                          fragmentShaderModule
									renderPass,								// const VkRenderPass                            renderPass
									viewports,								// const std::vector<VkViewport>&                viewports
									scissors,								// const std::vector<VkRect2D>&                  scissors
									VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology                     topology
									0u,										// const deUint32                                subpass
									0u,										// const deUint32                                patchControlPoints
									DE_NULL,								// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
									DE_NULL,								// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
									DE_NULL,								// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
									&depthStencilStateCreateInfo);			// const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
}

void commandClearStencilAttachment (const DeviceInterface&	vk,
									const VkCommandBuffer	commandBuffer,
									const VkOffset2D&		offset,
									const VkExtent2D&		extent,
									const deUint32			clearValue)
{
	const VkClearAttachment stencilAttachment =
	{
		VK_IMAGE_ASPECT_STENCIL_BIT,					// VkImageAspectFlags    aspectMask;
		0u,												// uint32_t              colorAttachment;
		makeClearValueDepthStencil(0.0f, clearValue),	// VkClearValue          clearValue;
	};

	const VkClearRect rect =
	{
		{ offset, extent },		// VkRect2D    rect;
		0u,						// uint32_t    baseArrayLayer;
		1u,						// uint32_t    layerCount;
	};

	vk.cmdClearAttachments(commandBuffer, 1u, &stencilAttachment, 1u, &rect);
}

VkImageAspectFlags getImageAspectFlags (const VkFormat format)
{
	const tcu::TextureFormat tcuFormat = mapVkFormat(format);

	if      (tcuFormat.order == tcu::TextureFormat::DS)		return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	else if (tcuFormat.order == tcu::TextureFormat::D)		return VK_IMAGE_ASPECT_DEPTH_BIT;
	else if (tcuFormat.order == tcu::TextureFormat::S)		return VK_IMAGE_ASPECT_STENCIL_BIT;

	DE_ASSERT(false);
	return 0u;
}

bool isSupportedDepthStencilFormat (const InstanceInterface& instanceInterface, const VkPhysicalDevice device, const VkFormat format)
{
	VkFormatProperties formatProps;
	instanceInterface.getPhysicalDeviceFormatProperties(device, format, &formatProps);
	return (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
}

VkFormat pickSupportedDepthStencilFormat (const InstanceInterface&	instanceInterface,
										  const VkPhysicalDevice	device,
										  const deUint32			numFormats,
										  const VkFormat*			pFormats)
{
	for (deUint32 i = 0; i < numFormats; ++i)
		if (isSupportedDepthStencilFormat(instanceInterface, device, pFormats[i]))
			return pFormats[i];
	return VK_FORMAT_UNDEFINED;
}

enum Flags
{
	FLAG_TEST_DEPTH								= 1u << 0,
	FLAG_TEST_STENCIL							= 1u << 1,
	FLAG_DONT_USE_TEST_ATTACHMENT				= 1u << 2,
	FLAG_DONT_USE_EARLY_FRAGMENT_TESTS			= 1u << 3,
	FLAG_EARLY_AND_LATE_FRAGMENT_TESTS			= 1u << 4,
};

class EarlyFragmentTest : public TestCase
{
public:
						EarlyFragmentTest	(tcu::TestContext&		testCtx,
											 const std::string		name,
											 const deUint32			flags);

	void				initPrograms		(SourceCollections&		programCollection) const;
	TestInstance*		createInstance		(Context&				context) const;
	virtual void		checkSupport		(Context&				context) const;

private:
	const deUint32		m_flags;
};

EarlyFragmentTest::EarlyFragmentTest (tcu::TestContext& testCtx, const std::string name, const deUint32 flags)
	: TestCase	(testCtx, name, "")
	, m_flags	(flags)
{
}

void EarlyFragmentTest::initPrograms (SourceCollections& programCollection) const
{
	// Vertex
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
			<< "\n"
			<< "layout(location = 0) in highp vec4 position;\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "   vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_Position = position;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment
	if ((m_flags & FLAG_EARLY_AND_LATE_FRAGMENT_TESTS) == 0)
	{
		const bool useEarlyTests = (m_flags & FLAG_DONT_USE_EARLY_FRAGMENT_TESTS) == 0;
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
			<< "\n"
			<< (useEarlyTests ? "layout(early_fragment_tests) in;\n" : "")
			<< "layout(location = 0) out highp vec4 fragColor;\n"
			<< "\n"
			<< "layout(binding = 0) coherent buffer Output {\n"
			<< "    uint result;\n"
			<< "} sb_out;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    atomicAdd(sb_out.result, 1u);\n"
			<< "	fragColor = vec4(1.0, 1.0, 0.0, 1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
	}
	else
	{
		const SpirVAsmBuildOptions	buildOptionsSpr	(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3);
		const std::string			src				=
			"; SPIR-V\n"
			"; Version: 1.0\n"
			"; Bound: 24\n"
			"; Schema: 0\n"
			"OpCapability Shader\n"
			"OpExtension \"SPV_AMD_shader_early_and_late_fragment_tests\"\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint Fragment %4 \"main\" %20\n"
			"OpExecutionMode %4 OriginUpperLeft\n"
			"OpExecutionMode %4 EarlyAndLateFragmentTestsAMD\n"
			"OpMemberDecorate %7 0 Coherent\n"
			"OpMemberDecorate %7 0 Offset 0\n"
			"OpDecorate %7 BufferBlock\n"
			"OpDecorate %9 DescriptorSet 0\n"
			"OpDecorate %9 Binding 0\n"
			"OpDecorate %20 Location 0\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeInt 32 0\n"
			"%7 = OpTypeStruct %6\n"
			"%8 = OpTypePointer Uniform %7\n"
			"%9 = OpVariable %8 Uniform\n"
			"%10 = OpTypeInt 32 1\n"
			"%11 = OpConstant %10 0\n"
			"%12 = OpTypePointer Uniform %6\n"
			"%14 = OpConstant %6 1\n"
			"%15 = OpConstant %6 0\n"
			"%17 = OpTypeFloat 32\n"
			"%18 = OpTypeVector %17 4\n"
			"%19 = OpTypePointer Output %18\n"
			"%20 = OpVariable %19 Output\n"
			"%21 = OpConstant %17 1\n"
			"%22 = OpConstant %17 0\n"
			"%23 = OpConstantComposite %18 %21 %21 %22 %21\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%13 = OpAccessChain %12 %9 %11\n"
			"%16 = OpAtomicIAdd %6 %13 %14 %15 %14\n"
			"OpStore %20 %23\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("frag") << src << buildOptionsSpr;
	}
}

class EarlyFragmentTestInstance : public TestInstance
{
public:
							EarlyFragmentTestInstance (Context& context, const deUint32 flags);

	tcu::TestStatus			iterate					  (void);

private:
	enum TestMode
	{
		MODE_INVALID,
		MODE_DEPTH,
		MODE_STENCIL,
	};

	const TestMode			m_testMode;
	const bool				m_useTestAttachment;
	const bool				m_useEarlyTests;
	const bool				m_useEarlyLateTests;
};

EarlyFragmentTestInstance::EarlyFragmentTestInstance (Context& context, const deUint32 flags)
	: TestInstance			(context)
	, m_testMode			(flags & FLAG_TEST_DEPTH   ? MODE_DEPTH :
							 flags & FLAG_TEST_STENCIL ? MODE_STENCIL : MODE_INVALID)
	, m_useTestAttachment	((flags & FLAG_DONT_USE_TEST_ATTACHMENT) == 0)
	, m_useEarlyTests		((flags & FLAG_DONT_USE_EARLY_FRAGMENT_TESTS) == 0)
	, m_useEarlyLateTests	((flags & FLAG_EARLY_AND_LATE_FRAGMENT_TESTS) == FLAG_EARLY_AND_LATE_FRAGMENT_TESTS)
{
	DE_ASSERT(m_testMode != MODE_INVALID);
}

tcu::TestStatus EarlyFragmentTestInstance::iterate (void)
{
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const InstanceInterface&	vki					= m_context.getInstanceInterface();
	const VkDevice				device				= m_context.getDevice();
	const VkPhysicalDevice		physDevice			= m_context.getPhysicalDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();
	const deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&					allocator			= m_context.getDefaultAllocator();

	// Color attachment

	const tcu::IVec2				renderSize			= tcu::IVec2(32, 32);
	const VkFormat					colorFormat			= VK_FORMAT_R8G8B8A8_UNORM;
	const VkImageSubresourceRange	colorSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const Unique<VkImage>			colorImage			(makeImage(vk, device, makeImageCreateInfo(renderSize, colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)));
	const UniquePtr<Allocation>		colorImageAlloc		(bindImage(vk, device, allocator, *colorImage, MemoryRequirement::Any));
	const Unique<VkImageView>		colorImageView		(makeImageView(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange));

	// Test attachment (depth or stencil)
	static const VkFormat stencilFormats[] =
	{
		// One of the following formats must be supported, as per spec requirement.
		VK_FORMAT_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
	};

	const VkFormat testFormat = (m_testMode == MODE_STENCIL ? pickSupportedDepthStencilFormat(vki, physDevice, DE_LENGTH_OF_ARRAY(stencilFormats), stencilFormats)
															: VK_FORMAT_D16_UNORM);		// spec requires this format to be supported
	if (testFormat == VK_FORMAT_UNDEFINED)
		return tcu::TestStatus::fail("Required depth/stencil format not supported");

	if (m_useTestAttachment)
		m_context.getTestContext().getLog() << tcu::TestLog::Message << "Using depth/stencil format " << getFormatName(testFormat) << tcu::TestLog::EndMessage;

	const VkImageSubresourceRange	testSubresourceRange	= makeImageSubresourceRange(getImageAspectFlags(testFormat), 0u, 1u, 0u, 1u);
	const Unique<VkImage>			testImage				(makeImage(vk, device, makeImageCreateInfo(renderSize, testFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)));
	const UniquePtr<Allocation>		testImageAlloc			(bindImage(vk, device, allocator, *testImage, MemoryRequirement::Any));
	const Unique<VkImageView>		testImageView			(makeImageView(vk, device, *testImage, VK_IMAGE_VIEW_TYPE_2D, testFormat, testSubresourceRange));
	const VkImageView				attachmentImages[]		= { *colorImageView, *testImageView };
	const deUint32					numUsedAttachmentImages = (m_useTestAttachment ? 2u : 1u);

	// Vertex buffer

	const deUint32					numVertices				= 6;
	const VkDeviceSize				vertexBufferSizeBytes	= sizeof(tcu::Vec4) * numVertices;
	const Unique<VkBuffer>			vertexBuffer			(makeBuffer(vk, device, vertexBufferSizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
	const UniquePtr<Allocation>		vertexBufferAlloc		(bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible));

	{
		tcu::Vec4* const pVertices = reinterpret_cast<tcu::Vec4*>(vertexBufferAlloc->getHostPtr());

		// A small +0.00001f adjustment for the z-coordinate to get the expected rounded value for depth.
		pVertices[0] = tcu::Vec4( 1.0f, -1.0f,  0.50001f,  1.0f);
		pVertices[1] = tcu::Vec4(-1.0f, -1.0f,  0.0f,      1.0f);
		pVertices[2] = tcu::Vec4(-1.0f,  1.0f,  0.50001f,  1.0f);

		pVertices[3] = tcu::Vec4(-1.0f,  1.0f,  0.50001f,  1.0f);
		pVertices[4] = tcu::Vec4( 1.0f,  1.0f,  1.0f,      1.0f);
		pVertices[5] = tcu::Vec4( 1.0f, -1.0f,  0.50001f,  1.0f);

		flushAlloc(vk, device, *vertexBufferAlloc);
		// No barrier needed, flushed memory is automatically visible
	}

	// Result buffer

	const VkDeviceSize				resultBufferSizeBytes	= sizeof(deUint32);
	const Unique<VkBuffer>			resultBuffer			(makeBuffer(vk, device, resultBufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
	const UniquePtr<Allocation>		resultBufferAlloc		(bindBuffer(vk, device, allocator, *resultBuffer, MemoryRequirement::HostVisible));

	{
		deUint32* const pData = static_cast<deUint32*>(resultBufferAlloc->getHostPtr());

		*pData = 0;
		flushAlloc(vk, device, *resultBufferAlloc);
	}

	// Render result buffer (to retrieve color attachment contents)

	const VkDeviceSize				colorBufferSizeBytes	= tcu::getPixelSize(mapVkFormat(colorFormat)) * renderSize.x() * renderSize.y();
	const Unique<VkBuffer>			colorBuffer				(makeBuffer(vk, device, colorBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>		colorBufferAlloc		(bindBuffer(vk, device, allocator, *colorBuffer, MemoryRequirement::HostVisible));

	// Descriptors

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet				 (makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const VkDescriptorBufferInfo  resultBufferDescriptorInfo = makeDescriptorBufferInfo(resultBuffer.get(), 0ull, resultBufferSizeBytes);

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultBufferDescriptorInfo)
		.update(vk, device);

	// Pipeline

	const Unique<VkShaderModule>	vertexModule  (createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>	fragmentModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u));
	const Unique<VkRenderPass>		renderPass	  (makeRenderPass(vk, device, colorFormat, m_useTestAttachment, testFormat));
	const Unique<VkFramebuffer>		framebuffer	  (makeFramebuffer(vk, device, *renderPass, numUsedAttachmentImages, attachmentImages, renderSize.x(), renderSize.y()));
	const Unique<VkPipelineLayout>	pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline>		pipeline	  (makeGraphicsPipeline(vk, device, *pipelineLayout, *renderPass, *vertexModule, *fragmentModule, renderSize,
												  (m_testMode == MODE_DEPTH), (m_testMode == MODE_STENCIL)));
	const Unique<VkCommandPool>		cmdPool		  (createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer	  (allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Draw commands

	{
		const VkRect2D renderArea = {
			makeOffset2D(0, 0),
			makeExtent2D(renderSize.x(), renderSize.y()),
		};
		const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
		const VkDeviceSize vertexBufferOffset = 0ull;

		beginCommandBuffer(vk, *cmdBuffer);

		{
			const VkImageMemoryBarrier barriers[] = {
				makeImageMemoryBarrier(
					0u, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					*colorImage, colorSubresourceRange),
				makeImageMemoryBarrier(
					0u, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					*testImage, testSubresourceRange),
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u,
				0u, DE_NULL, 0u, DE_NULL, DE_LENGTH_OF_ARRAY(barriers), barriers);
		}

		// Will clear the attachments with specified depth and stencil values.
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, clearColor, 0.5f, 0u);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);
		vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);

		// Mask half of the attachment image with value that will pass the stencil test.
		if (m_useTestAttachment && m_testMode == MODE_STENCIL)
			commandClearStencilAttachment(vk, *cmdBuffer, makeOffset2D(0, 0), makeExtent2D(renderSize.x()/2, renderSize.y()), 1u);

		vk.cmdDraw(*cmdBuffer, numVertices, 1u, 0u, 0u);
		endRenderPass(vk, *cmdBuffer);

		copyImageToBuffer(vk, *cmdBuffer, *colorImage, *colorBuffer, renderSize, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

		endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(vk, device, queue, *cmdBuffer);
	}

	// Log result image
	{
		invalidateAlloc(vk, device, *colorBufferAlloc);

		const tcu::ConstPixelBufferAccess	imagePixelAccess(mapVkFormat(colorFormat), renderSize.x(), renderSize.y(), 1, colorBufferAlloc->getHostPtr());
		tcu::TextureLevel					referenceImage(mapVkFormat(colorFormat), renderSize.x(), renderSize.y());

		if (m_useTestAttachment == true)
		{
			const tcu::Vec4	fillColor	= tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f);
			const tcu::Vec4	clearColor	= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);

			tcu::clear(referenceImage.getAccess(), clearColor);

			if (m_testMode == MODE_DEPTH)
			{
				int xOffset = 0;

				for (int y = 0; y < renderSize.y() - 1; y++)
				{
					for (int x = 0; x < renderSize.x() - 1 - xOffset; x++)
					{
						referenceImage.getAccess().setPixel(fillColor, x, y);
					}

					xOffset++;
				}
			}

			if (m_testMode == MODE_STENCIL)
			{
				for (int y = 0; y < renderSize.y(); y++)
				{
					for (int x = 0; x < renderSize.x() / 2; x++)
					{
						referenceImage.getAccess().setPixel(fillColor, x, y);
					}
				}
			}
		}
		else
		{
			const tcu::Vec4	clearColor = tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f);

			tcu::clear(referenceImage.getAccess(), clearColor);
		}

		if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", referenceImage.getAccess(), imagePixelAccess, tcu::Vec4(0.02f), tcu::COMPARE_LOG_RESULT))
			return tcu::TestStatus::fail("Rendered color image is not correct");
	}

	// Verify results
	{
		invalidateAlloc(vk, device, *resultBufferAlloc);

		const int  actualCounter	   = *static_cast<deInt32*>(resultBufferAlloc->getHostPtr());
		const bool expectPartialResult = (m_useEarlyTests && m_useTestAttachment);
		const int  expectedCounter	   = expectPartialResult ? renderSize.x() * renderSize.y() / 2 : renderSize.x() * renderSize.y();
		const int  tolerance		   = expectPartialResult ? de::max(renderSize.x(), renderSize.y()) * 3	: 0;
		const int  expectedMin         = de::max(0, expectedCounter - tolerance);
		const int  expectedMax		   = (m_useEarlyLateTests ? (renderSize.x() * renderSize.y()) : (expectedCounter + tolerance));

		tcu::TestLog& log = m_context.getTestContext().getLog();
		log << tcu::TestLog::Message << "Expected value"
			<< (expectPartialResult ? " in range: [" + de::toString(expectedMin) + ", " + de::toString(expectedMax) + "]" : ": " + de::toString(expectedCounter))
			<< tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Result value: " << de::toString(actualCounter) << tcu::TestLog::EndMessage;

		if (expectedMin <= actualCounter && actualCounter <= expectedMax)
			return tcu::TestStatus::pass("Success");
		else
			return tcu::TestStatus::fail("Value out of range");
	}
}

TestInstance* EarlyFragmentTest::createInstance (Context& context) const
{
	return new EarlyFragmentTestInstance(context, m_flags);
}

void EarlyFragmentTest::checkSupport (Context& context) const
{
	context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS);
#ifndef CTS_USES_VULKANSC
	if ((m_flags & FLAG_EARLY_AND_LATE_FRAGMENT_TESTS) == FLAG_EARLY_AND_LATE_FRAGMENT_TESTS)
	{
		context.requireDeviceFunctionality("VK_AMD_shader_early_and_late_fragment_tests");
		if (context.getShaderEarlyAndLateFragmentTestsFeaturesAMD().shaderEarlyAndLateFragmentTests == VK_FALSE)
			TCU_THROW(NotSupportedError, "shaderEarlyAndLateFragmentTests is not supported");
	}
#endif
}

class EarlyFragmentDiscardTestInstance : public EarlyFragmentTestInstance
{
public:
							EarlyFragmentDiscardTestInstance	(Context& context, const deUint32 flags);

	tcu::TestStatus			iterate								(void);

private:
	tcu::TextureLevel	generateReferenceColorImage				(const tcu::TextureFormat format, const tcu::IVec2& renderSize);
	enum TestMode
	{
		MODE_INVALID,
		MODE_DEPTH,
		MODE_STENCIL,
	};

	const TestMode			m_testMode;
	const bool				m_useTestAttachment;
	const bool				m_useEarlyTests;
	const bool				m_useEarlyLateTests;
};

EarlyFragmentDiscardTestInstance::EarlyFragmentDiscardTestInstance (Context& context, const deUint32 flags)
	: EarlyFragmentTestInstance			(context, flags)
	, m_testMode			(flags & FLAG_TEST_DEPTH   ? MODE_DEPTH :
							 flags & FLAG_TEST_STENCIL ? MODE_STENCIL : MODE_INVALID)
	, m_useTestAttachment	((flags & FLAG_DONT_USE_TEST_ATTACHMENT) == 0)
	, m_useEarlyTests		((flags & FLAG_DONT_USE_EARLY_FRAGMENT_TESTS) == 0)
	, m_useEarlyLateTests	((flags& FLAG_EARLY_AND_LATE_FRAGMENT_TESTS) != 0)
{
	DE_ASSERT(m_testMode != MODE_INVALID);
}

tcu::TextureLevel EarlyFragmentDiscardTestInstance::generateReferenceColorImage(const tcu::TextureFormat format, const tcu::IVec2 &renderSize)
{
	tcu::TextureLevel	image(format, renderSize.x(), renderSize.y());
	const tcu::Vec4		clearColor	= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);

	tcu::clear(image.getAccess(), clearColor);

	return image;
}

tcu::TestStatus EarlyFragmentDiscardTestInstance::iterate (void)
{
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const InstanceInterface&	vki					= m_context.getInstanceInterface();
	const VkDevice				device				= m_context.getDevice();
	const VkPhysicalDevice		physDevice			= m_context.getPhysicalDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();
	const deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&					allocator			= m_context.getDefaultAllocator();

	DE_ASSERT(m_useTestAttachment);

	// Color attachment
	const tcu::IVec2				renderSize				= tcu::IVec2(32, 32);
	const VkFormat					colorFormat				= VK_FORMAT_R8G8B8A8_UNORM;
	const VkImageSubresourceRange	colorSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const Unique<VkImage>			colorImage				(makeImage(vk, device, makeImageCreateInfo(renderSize, colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)));
	const UniquePtr<Allocation>		colorImageAlloc			(bindImage(vk, device, allocator, *colorImage, MemoryRequirement::Any));
	const Unique<VkImageView>		colorImageView			(makeImageView(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange));

	// Test attachment (depth or stencil)
	static const VkFormat stencilFormats[] =
	{
		// One of the following formats must be supported, as per spec requirement.
		VK_FORMAT_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
	};

	const VkFormat depthStencilFormat = (m_testMode == MODE_STENCIL ? pickSupportedDepthStencilFormat(vki, physDevice, DE_LENGTH_OF_ARRAY(stencilFormats), stencilFormats)
										 : VK_FORMAT_D16_UNORM);		// spec requires this format to be supported

	if (depthStencilFormat == VK_FORMAT_UNDEFINED)
		return tcu::TestStatus::fail("Required depth/stencil format not supported");

	m_context.getTestContext().getLog() << tcu::TestLog::Message << "Using depth/stencil format " << getFormatName(depthStencilFormat) << tcu::TestLog::EndMessage;

	const VkImageSubresourceRange	testSubresourceRange	= makeImageSubresourceRange(getImageAspectFlags(depthStencilFormat), 0u, 1u, 0u, 1u);
	const Unique<VkImage>			testImage				(makeImage(vk, device, makeImageCreateInfo(renderSize, depthStencilFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)));
	const UniquePtr<Allocation>		testImageAlloc			(bindImage(vk, device, allocator, *testImage, MemoryRequirement::Any));
	const Unique<VkImageView>		testImageView			(makeImageView(vk, device, *testImage, VK_IMAGE_VIEW_TYPE_2D, depthStencilFormat, testSubresourceRange));
	const VkImageView				attachmentImages[]		= { *colorImageView, *testImageView };
	const deUint32					numUsedAttachmentImages = DE_LENGTH_OF_ARRAY(attachmentImages);

	// Vertex buffer

	const deUint32					numVertices				= 6;
	const VkDeviceSize				vertexBufferSizeBytes	= sizeof(tcu::Vec4) * numVertices;
	const Unique<VkBuffer>			vertexBuffer			(makeBuffer(vk, device, vertexBufferSizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
	const UniquePtr<Allocation>		vertexBufferAlloc		(bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible));

	{
		tcu::Vec4* const pVertices = reinterpret_cast<tcu::Vec4*>(vertexBufferAlloc->getHostPtr());

		pVertices[0] = tcu::Vec4( 1.0f, -1.0f,  0.5f,  1.0f);
		pVertices[1] = tcu::Vec4(-1.0f, -1.0f,  0.0f,  1.0f);
		pVertices[2] = tcu::Vec4(-1.0f,  1.0f,  0.5f,  1.0f);

		pVertices[3] = tcu::Vec4(-1.0f,  1.0f,  0.5f,  1.0f);
		pVertices[4] = tcu::Vec4( 1.0f,  1.0f,  1.0f,  1.0f);
		pVertices[5] = tcu::Vec4( 1.0f, -1.0f,  0.5f,  1.0f);

		flushAlloc(vk, device, *vertexBufferAlloc);
		// No barrier needed, flushed memory is automatically visible
	}

	// Result buffer

	const VkDeviceSize				resultBufferSizeBytes	= sizeof(deUint32);
	const Unique<VkBuffer>			resultBuffer			(makeBuffer(vk, device, resultBufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
	const UniquePtr<Allocation>		resultBufferAlloc		(bindBuffer(vk, device, allocator, *resultBuffer, MemoryRequirement::HostVisible));

	{
		deUint32* const pData = static_cast<deUint32*>(resultBufferAlloc->getHostPtr());

		*pData = 0;
		flushAlloc(vk, device, *resultBufferAlloc);
	}

	// Render result buffer (to retrieve color attachment contents)

	const VkDeviceSize				colorBufferSizeBytes	= tcu::getPixelSize(mapVkFormat(colorFormat)) * renderSize.x() * renderSize.y();
	const Unique<VkBuffer>			colorBuffer				(makeBuffer(vk, device, colorBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>		colorBufferAlloc		(bindBuffer(vk, device, allocator, *colorBuffer, MemoryRequirement::HostVisible));

	// Depth stencil result buffer (to retrieve depth-stencil attachment contents)

	const VkDeviceSize				dsBufferSizeBytes	= tcu::getPixelSize(mapVkFormat(depthStencilFormat)) * renderSize.x() * renderSize.y();
	const Unique<VkBuffer>			dsBuffer			(makeBuffer(vk, device, dsBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>		dsBufferAlloc		(bindBuffer(vk, device, allocator, *dsBuffer, MemoryRequirement::HostVisible));

	// Descriptors

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet				 (makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const VkDescriptorBufferInfo  resultBufferDescriptorInfo = makeDescriptorBufferInfo(resultBuffer.get(), 0ull, resultBufferSizeBytes);

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultBufferDescriptorInfo)
		.update(vk, device);

	// Pipeline

	const Unique<VkShaderModule>	vertexModule  (createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>	fragmentModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u));
	const Unique<VkRenderPass>		renderPass	  (makeRenderPass(vk, device, colorFormat, m_useTestAttachment, depthStencilFormat));
	const Unique<VkFramebuffer>		framebuffer	  (makeFramebuffer(vk, device, *renderPass, numUsedAttachmentImages, attachmentImages, renderSize.x(), renderSize.y()));
	const Unique<VkPipelineLayout>	pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline>		pipeline	  (makeGraphicsPipeline(vk, device, *pipelineLayout, *renderPass, *vertexModule, *fragmentModule, renderSize,
																		(m_testMode == MODE_DEPTH), (m_testMode == MODE_STENCIL),
																		VK_STENCIL_OP_INCREMENT_AND_CLAMP, VK_STENCIL_OP_INCREMENT_AND_CLAMP));
	const Unique<VkCommandPool>		cmdPool		  (createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer	  (allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Draw commands
	{
		const VkRect2D renderArea = {
			makeOffset2D(0, 0),
			makeExtent2D(renderSize.x(), renderSize.y()),
		};
		const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
		const VkDeviceSize vertexBufferOffset = 0ull;

		beginCommandBuffer(vk, *cmdBuffer);

		{
			const VkImageMemoryBarrier barriers[] = {
				makeImageMemoryBarrier(
					0u, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					*colorImage, colorSubresourceRange),
				makeImageMemoryBarrier(
					0u, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					*testImage, testSubresourceRange),
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u,
				0u, DE_NULL, 0u, DE_NULL, DE_LENGTH_OF_ARRAY(barriers), barriers);
		}

		// Will clear the attachments with specified depth and stencil values.
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, clearColor, 0.5f, 3u);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);
		vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);

		// Mask half of the attachment image with value that will pass the stencil test.
		if (m_testMode == MODE_STENCIL)
			commandClearStencilAttachment(vk, *cmdBuffer, makeOffset2D(0, 0), makeExtent2D(renderSize.x()/2, renderSize.y()), 1u);

		vk.cmdDraw(*cmdBuffer, numVertices, 1u, 0u, 0u);
		endRenderPass(vk, *cmdBuffer);

		copyImageToBuffer(vk, *cmdBuffer, *colorImage, *colorBuffer, renderSize, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
		VkImageAspectFlags	dsAspect = m_testMode == MODE_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_STENCIL_BIT;
		VkImageLayout		dsImageLayout = m_testMode == MODE_DEPTH ? VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
		copyImageToBuffer(vk, *cmdBuffer, *testImage, *dsBuffer, renderSize, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, dsImageLayout, 1u, dsAspect, dsAspect);

		endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(vk, device, queue, *cmdBuffer);
	}

	// Verify color output
	{
		invalidateAlloc(vk, device, *colorBufferAlloc);

		const tcu::ConstPixelBufferAccess	imagePixelAccess(mapVkFormat(colorFormat), renderSize.x(), renderSize.y(), 1, colorBufferAlloc->getHostPtr());
		const tcu::TextureLevel				referenceImage	= generateReferenceColorImage(mapVkFormat(colorFormat), renderSize);
		if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", referenceImage.getAccess(), imagePixelAccess, tcu::Vec4(0.02f), tcu::COMPARE_LOG_RESULT))
			printf("Rendered color image is not correct");
	}

	// Verify depth-stencil output
	{
		invalidateAlloc(vk, device, *dsBufferAlloc);
		// the buffer holds only one aspect of d/s format
		tcu::TextureFormat format = vk::mapVkFormat(m_testMode == MODE_STENCIL ? VK_FORMAT_S8_UINT : depthStencilFormat);
		DE_ASSERT(format.order == tcu::TextureFormat::D || format.order == tcu::TextureFormat::S);

		const tcu::ConstPixelBufferAccess	dsPixelAccess (format, renderSize.x(), renderSize.y(), 1, dsBufferAlloc->getHostPtr());

		for(int z = 0; z < dsPixelAccess.getDepth(); z++)
		for(int y = 0; y < dsPixelAccess.getHeight(); y++)
		for(int x = 0; x < dsPixelAccess.getWidth(); x++)
		{
			float	depthValue		= (m_testMode == MODE_DEPTH) ? dsPixelAccess.getPixDepth(x, y, z) : 0.0f;
			int		stencilValue	= (m_testMode == MODE_STENCIL) ? dsPixelAccess.getPixStencil(x, y, z) : 0;

			// Depth test should write to the depth buffer even when there is a discard in the fragment shader,
			// when early fragment tests are enabled. We allow some tolerance to account for precision error
			// on depth writes.
			if (m_testMode == MODE_DEPTH)
			{
				float tolerance = 0.0001f;
				if (m_useEarlyTests && ((x + y) < 31) && depthValue >= 0.50 + tolerance)
				{
					std::ostringstream error;
					error << "Rendered depth value [ "<< x << ", " << y << ", " << z << "] is not correct: " << depthValue << " >= 0.5f";
					TCU_FAIL(error.str().c_str());
				}
				// When early fragment tests are disabled, the depth test happens after the fragment shader, but as we are discarding
				// all fragments, the stored value in the depth buffer should be the clear one (0.5f).
				if (!m_useEarlyTests && deAbs(depthValue - 0.5f) > tolerance)
				{
					std::ostringstream error;
					error << "Rendered depth value [ "<< x << ", " << y << ", " << z << "] is not correct: " << depthValue << " != 0.5f";
					TCU_FAIL(error.str().c_str());
				}
			}

			if (m_testMode == MODE_STENCIL)
			{
				if (m_useEarlyTests && ((x < 16 && stencilValue != 2u) || (x >= 16 && stencilValue != 4u)))
				{
					std::ostringstream error;
					error << "Rendered stencil value [ "<< x << ", " << y << ", " << z << "] is not correct: " << stencilValue << " != ";
					error << (x < 16 ? 2u : 4u);
					TCU_FAIL(error.str().c_str());
				}

				if (!m_useEarlyTests && ((x < 16 && stencilValue != 1u) || (x >= 16 && stencilValue != 3u)))
				{
					std::ostringstream error;
					error << "Rendered stencil value [ "<< x << ", " << y << ", " << z << "] is not correct: " << stencilValue << " != ";
					error << (x < 16 ? 1u : 3u);
					TCU_FAIL(error.str().c_str());
				}
			}
		}
	}

	// Verify we process all the fragments
	{
		invalidateAlloc(vk, device, *resultBufferAlloc);

		const int  actualCounter	   = *static_cast<deInt32*>(resultBufferAlloc->getHostPtr());
		const bool expectPartialResult = m_useEarlyTests;
		const int  expectedCounter	   = expectPartialResult ? renderSize.x() * renderSize.y() / 2 : renderSize.x() * renderSize.y();
		const int  tolerance		   = expectPartialResult ? de::max(renderSize.x(), renderSize.y()) * 3	: 0;
		const int  expectedMin         = de::max(0, expectedCounter - tolerance);
		const int  expectedMax		   = (m_useEarlyLateTests ? (renderSize.x() * renderSize.y()) : (expectedCounter + tolerance));

		tcu::TestLog& log = m_context.getTestContext().getLog();
		log << tcu::TestLog::Message << "Expected value"
			<< (expectPartialResult ? " in range: [" + de::toString(expectedMin) + ", " + de::toString(expectedMax) + "]" : ": " + de::toString(expectedCounter))
			<< tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Result value: " << de::toString(actualCounter) << tcu::TestLog::EndMessage;

		if (expectedMin <= actualCounter && actualCounter <= expectedMax)
			return tcu::TestStatus::pass("Success");
		else
			return tcu::TestStatus::fail("Value out of range");
	}
}

class EarlyFragmentDiscardTest : public EarlyFragmentTest
{
public:
						EarlyFragmentDiscardTest	(tcu::TestContext&		testCtx,
													 const std::string		name,
													 const deUint32			flags);

	void				initPrograms				(SourceCollections&		programCollection) const;
	TestInstance*		createInstance				(Context&				context) const;

private:
	const deUint32		m_flags;
};

EarlyFragmentDiscardTest::EarlyFragmentDiscardTest (tcu::TestContext& testCtx, const std::string name, const deUint32 flags)
	: EarlyFragmentTest	(testCtx, name, flags)
	, m_flags (flags)
{
}

TestInstance* EarlyFragmentDiscardTest::createInstance (Context& context) const
{
	return new EarlyFragmentDiscardTestInstance(context, m_flags);
}

void EarlyFragmentDiscardTest::initPrograms(SourceCollections &programCollection) const
{
	// Vertex
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
			<< "\n"
			<< "layout(location = 0) in highp vec4 position;\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "   vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_Position = position;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment
	{
		if ((m_flags & FLAG_EARLY_AND_LATE_FRAGMENT_TESTS) == 0)
		{
			const bool useEarlyTests = (m_flags & FLAG_DONT_USE_EARLY_FRAGMENT_TESTS) == 0;
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
				<< "\n"
				<< (useEarlyTests ? "layout(early_fragment_tests) in;\n" : "")
				<< "layout(location = 0) out highp vec4 fragColor;\n"
				<< "\n"
				<< "layout(binding = 0) coherent buffer Output {\n"
				<< "    uint result;\n"
				<< "} sb_out;\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    atomicAdd(sb_out.result, 1u);\n"
				<< "    gl_FragDepth = 0.75f;\n"
				<< "    fragColor = vec4(1.0, 1.0, 0.0, 1.0);\n"
				<< "    discard;\n"
				<< "}\n";
			programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
		}
		else
		{
			const SpirVAsmBuildOptions	buildOptionsSpr(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3);
			const std::string			src =
				"; SPIR - V\n"
				"; Version: 1.0\n"
				"; Generator: Khronos Glslang Reference Front End; 10\n"
				"; Bound: 28\n"
				"; Schema: 0\n"
				"OpCapability Shader\n"
				"OpExtension \"SPV_AMD_shader_early_and_late_fragment_tests\"\n"
				"%1 = OpExtInstImport \"GLSL.std.450\"\n"
				"OpMemoryModel Logical GLSL450\n"
				"OpEntryPoint Fragment %4 \"main\" %19 %23\n"
				"OpExecutionMode %4 OriginUpperLeft\n"
				"OpExecutionMode %4 EarlyAndLateFragmentTestsAMD\n"
				"OpExecutionMode %4 DepthReplacing\n"
				"OpExecutionMode %4 DepthGreater\n"
				"OpMemberDecorate %7 0 Coherent\n"
				"OpMemberDecorate %7 0 Offset 0\n"
				"OpDecorate %7 BufferBlock\n"
				"OpDecorate %9 DescriptorSet 0\n"
				"OpDecorate %9 Binding 0\n"
				"OpDecorate %19 BuiltIn FragDepth\n"
				"OpDecorate %23 Location 0\n"
				"%2 = OpTypeVoid\n"
				"%3 = OpTypeFunction %2\n"
				"%6 = OpTypeInt 32 0\n"
				"%7 = OpTypeStruct %6\n"
				"%8 = OpTypePointer Uniform %7\n"
				"%9 = OpVariable %8 Uniform\n"
				"%10 = OpTypeInt 32 1\n"
				"%11 = OpConstant %10 0\n"
				"%12 = OpTypePointer Uniform %6\n"
				"%14 = OpConstant %6 1\n"
				"%15 = OpConstant %6 0\n"
				"%17 = OpTypeFloat 32\n"
				"%18 = OpTypePointer Output %17\n"
				"%19 = OpVariable %18 Output\n"
				"%20 = OpConstant %17 0.75\n"
				"%21 = OpTypeVector %17 4\n"
				"%22 = OpTypePointer Output %21\n"
				"%23 = OpVariable %22 Output\n"
				"%24 = OpConstant %17 1\n"
				"%25 = OpConstant %17 0\n"
				"%26 = OpConstantComposite %21 %24 %24 %25 %24\n"
				"%4 = OpFunction %2 None %3\n"
				"%5 = OpLabel\n"
				"%13 = OpAccessChain %12 %9 %11\n"
				"%16 = OpAtomicIAdd %6 %13 %14 %15 %14\n"
				"OpStore %19 %20\n"
				"OpStore %23 %26\n"
				"OpKill\n"
				"OpFunctionEnd\n";
			programCollection.spirvAsmSources.add("frag") << src << buildOptionsSpr;
		}
	}
}

class EarlyFragmentSampleMaskTestInstance : public EarlyFragmentTestInstance
{
public:
							EarlyFragmentSampleMaskTestInstance	(Context& context, const deUint32 flags, const deUint32 sampleCount);

	tcu::TestStatus			iterate								(void);

private:
	tcu::TextureLevel	generateReferenceColorImage				(const tcu::TextureFormat format, const tcu::IVec2& renderSize);
	Move<VkRenderPass>  makeRenderPass							(const DeviceInterface&				vk,
																 const VkDevice						device,
																 const VkFormat						colorFormat,
																 const VkFormat						depthStencilFormat);
	Move<VkPipeline>	makeGraphicsPipeline					(const DeviceInterface&	vk,
																 const VkDevice			device,
																 const VkPipelineLayout	pipelineLayout,
																 const VkRenderPass		renderPass,
																 const VkShaderModule		vertexModule,
																 const VkShaderModule		fragmentModule,
																 const tcu::IVec2&		renderSize,
																 const bool				enableDepthTest,
																 const bool				enableStencilTest,
																 const VkStencilOp		stencilFailOp,
																 const VkStencilOp		stencilPassOp);
	enum TestMode
	{
		MODE_INVALID,
		MODE_DEPTH,
		MODE_STENCIL,
	};

	const TestMode			m_testMode;
	const bool				m_useTestAttachment;
	const bool				m_useEarlyTests;
	const deUint32			m_sampleCount;
};

EarlyFragmentSampleMaskTestInstance::EarlyFragmentSampleMaskTestInstance (Context& context, const deUint32 flags, const deUint32 sampleCount)
	: EarlyFragmentTestInstance			(context, flags)
	, m_testMode			(flags & FLAG_TEST_DEPTH   ? MODE_DEPTH :
							 flags & FLAG_TEST_STENCIL ? MODE_STENCIL : MODE_INVALID)
	, m_useTestAttachment	((flags & FLAG_DONT_USE_TEST_ATTACHMENT) == 0)
	, m_useEarlyTests		((flags & FLAG_DONT_USE_EARLY_FRAGMENT_TESTS) == 0)
	, m_sampleCount			(sampleCount)
{
	DE_ASSERT(m_testMode != MODE_INVALID);
}

tcu::TextureLevel EarlyFragmentSampleMaskTestInstance::generateReferenceColorImage(const tcu::TextureFormat format, const tcu::IVec2 &renderSize)
{
	tcu::TextureLevel	image(format, renderSize.x(), renderSize.y());
	const tcu::Vec4		clearColor	= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);

	tcu::clear(image.getAccess(), clearColor);

	return image;
}

Move<VkPipeline> EarlyFragmentSampleMaskTestInstance::makeGraphicsPipeline (const DeviceInterface&	vk,
																			const VkDevice			device,
																			const VkPipelineLayout	pipelineLayout,
																			const VkRenderPass		renderPass,
																			const VkShaderModule		vertexModule,
																			const VkShaderModule		fragmentModule,
																			const tcu::IVec2&		renderSize,
																			const bool				enableDepthTest,
																			const bool				enableStencilTest,
																			const VkStencilOp		stencilFailOp,
																			const VkStencilOp		stencilPassOp)
{
	const std::vector<VkViewport>			viewports					(1, makeViewport(renderSize));
	const std::vector<VkRect2D>				scissors					(1, makeRect2D(renderSize));

	const VkStencilOpState					stencilOpState				= makeStencilOpState(
		stencilFailOp,			// stencil fail
		stencilPassOp,			// depth & stencil pass
		VK_STENCIL_OP_KEEP,		// depth only fail
		VK_COMPARE_OP_EQUAL,	// compare op
		0x3,					// compare mask
		0xf,					// write mask
		1u);					// reference

	const VkPipelineDepthStencilStateCreateInfo	depthStencilStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType                          sType
		DE_NULL,													// const void*                              pNext
		0u,															// VkPipelineDepthStencilStateCreateFlags   flags
		enableDepthTest ? VK_TRUE : VK_FALSE,						// VkBool32                                 depthTestEnable
		enableDepthTest ? VK_TRUE : VK_FALSE,						// VkBool32                                 depthWriteEnable
		VK_COMPARE_OP_LESS,											// VkCompareOp                              depthCompareOp
		VK_FALSE,													// VkBool32                                 depthBoundsTestEnable
		enableStencilTest ? VK_TRUE : VK_FALSE,						// VkBool32                                 stencilTestEnable
		stencilOpState,												// VkStencilOpState                         front
		stencilOpState,												// VkStencilOpState                         back
		0.0f,														// float                                    minDepthBounds
		1.0f														// float                                    maxDepthBounds
	};

	// Only allow coverage on sample 0.
	const VkSampleMask sampleMask = 0x1;

	const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType
		DE_NULL,													// const void*								pNext
		0u,															// VkPipelineMultisampleStateCreateFlags	flags
		(VkSampleCountFlagBits)m_sampleCount,						// VkSampleCountFlagBits					rasterizationSamples
		DE_TRUE,													// VkBool32									sampleShadingEnable
		0.0f,														// float									minSampleShading
		&sampleMask,												// const VkSampleMask*						pSampleMask
		DE_FALSE,													// VkBool32									alphaToCoverageEnable
		DE_FALSE,													// VkBool32									alphaToOneEnable
	};

	return vk::makeGraphicsPipeline(vk,										// const DeviceInterface&                        vk
									device,									// const VkDevice                                device
									pipelineLayout,							// const VkPipelineLayout                        pipelineLayout
									vertexModule,							// const VkShaderModule                          vertexShaderModule
									DE_NULL,								// const VkShaderModule                          tessellationControlModule
									DE_NULL,								// const VkShaderModule                          tessellationEvalModule
									DE_NULL,								// const VkShaderModule                          geometryShaderModule
									fragmentModule,							// const VkShaderModule                          fragmentShaderModule
									renderPass,								// const VkRenderPass                            renderPass
									viewports,								// const std::vector<VkViewport>&                viewports
									scissors,								// const std::vector<VkRect2D>&                  scissors
									VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology                     topology
									0u,										// const deUint32                                subpass
									0u,										// const deUint32                                patchControlPoints
									DE_NULL,								// const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
									DE_NULL,								// const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
									&multisampleStateCreateInfo,			// const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
									&depthStencilStateCreateInfo);			// const VkPipelineDepthStencilStateCreateInfo*  depthStencilStateCreateInfo
}

Move<VkRenderPass> EarlyFragmentSampleMaskTestInstance::makeRenderPass (const DeviceInterface&				vk,
																		const VkDevice						device,
																		const VkFormat						colorFormat,
																		const VkFormat						depthStencilFormat)
{
	const bool								hasColor							= colorFormat != VK_FORMAT_UNDEFINED;
	const bool								hasDepthStencil						= depthStencilFormat != VK_FORMAT_UNDEFINED;


	const VkAttachmentDescription2			colorAttachmentDescription			=
	{
		VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,	// VkStructureType					sType;
		DE_NULL,									// const void*						pNext;
		(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags    flags
		colorFormat,								// VkFormat                        format
		(VkSampleCountFlagBits)m_sampleCount,		// VkSampleCountFlagBits           samples
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp              loadOp
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp             storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp              stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp             stencilStoreOp
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout                   initialLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout                   finalLayout
	};

	const VkAttachmentDescription2			depthStencilAttachmentDescription	=
	{
		VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,			// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		(VkAttachmentDescriptionFlags)0,					// VkAttachmentDescriptionFlags    flags
		depthStencilFormat,									// VkFormat                        format
		(VkSampleCountFlagBits)m_sampleCount,				// VkSampleCountFlagBits           samples
		VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp              loadOp
		VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp             storeOp
		VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp              stencilLoadOp
		VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp             stencilStoreOp
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// VkImageLayout                   initialLayout
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL	// VkImageLayout                   finalLayout
	};

	const VkAttachmentDescription2			resolveAttachmentDescription			=
	{
		VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,	// VkStructureType					sType;
		DE_NULL,									// const void*						pNext;
		(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags    flags
		colorFormat,								// VkFormat                        format
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits           samples
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp              loadOp
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp             storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp              stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp             stencilStoreOp
		VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout                   initialLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout                   finalLayout
	};

		const VkAttachmentDescription2		resolveDepthStencilAttachmentDescription	=
	{
		VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,			// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		(VkAttachmentDescriptionFlags)0,					// VkAttachmentDescriptionFlags    flags
		depthStencilFormat,									// VkFormat                        format
		VK_SAMPLE_COUNT_1_BIT,								// VkSampleCountFlagBits           samples
		VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp              loadOp
		VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp             storeOp
		VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp              stencilLoadOp
		VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp             stencilStoreOp
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// VkImageLayout                   initialLayout
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL	// VkImageLayout                   finalLayout
	};

	std::vector<VkAttachmentDescription2>	attachmentDescriptions;

	if (hasColor)
		attachmentDescriptions.push_back(colorAttachmentDescription);
	if (hasDepthStencil)
		attachmentDescriptions.push_back(depthStencilAttachmentDescription);
	if (hasColor)
		attachmentDescriptions.push_back(resolveAttachmentDescription);
	if (hasDepthStencil)
		attachmentDescriptions.push_back(resolveDepthStencilAttachmentDescription);

	const VkAttachmentReference2				colorAttachmentRef					=
	{
		VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,			// VkStructureType		sType;
		DE_NULL,											// const void*			pNext;
		0u,													// deUint32				attachment
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout		layout
		VK_IMAGE_ASPECT_COLOR_BIT							// VkImageAspectFlags	aspectMask;
	};

	const VkAttachmentReference2				depthStencilAttachmentRef			=
	{
		VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,																// VkStructureType		sType;
		DE_NULL,																								// const void*			pNext;
		hasDepthStencil ? 1u : 0u,																				// deUint32				attachment
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,														// VkImageLayout		layout
		VkImageAspectFlags(m_testMode == MODE_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_STENCIL_BIT)	// VkImageAspectFlags	aspectMask;
	};

	const VkAttachmentReference2				resolveAttachmentRef					=
	{
		VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,			// VkStructureType		sType;
		DE_NULL,											// const void*			pNext;
		hasColor ? 2u : 0u,									// deUint32				attachment
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout		layout
		VK_IMAGE_ASPECT_COLOR_BIT							// VkImageAspectFlags	aspectMask;
	};

	const VkAttachmentReference2				depthStencilResolveAttachmentRef			=
	{
		VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,																// VkStructureType		sType;
		DE_NULL,																								// const void*			pNext;
		hasDepthStencil ? 3u : 0u,																				// deUint32				attachment
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,														// VkImageLayout		layout
		VkImageAspectFlags(m_testMode == MODE_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_STENCIL_BIT)	// VkImageAspectFlags	aspectMask;
	};

	// Using VK_RESOLVE_MODE_SAMPLE_ZERO_BIT as resolve mode, so no need to check its support as it is mandatory in the extension.
	const VkSubpassDescriptionDepthStencilResolve depthStencilResolveDescription =
	{
		VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE,	// VkStructureType					sType;
		DE_NULL,														// const void*						pNext;
		VK_RESOLVE_MODE_SAMPLE_ZERO_BIT,								// VkResolveModeFlagBits			depthResolveMode;
		VK_RESOLVE_MODE_SAMPLE_ZERO_BIT,								// VkResolveModeFlagBits			stencilResolveMode;
		&depthStencilResolveAttachmentRef								// const VkAttachmentReference2*	pDepthStencilResolveAttachment;
	};

	const VkSubpassDescription2				subpassDescription					=
	{
		VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,						// VkStructureType					sType;
		hasDepthStencil ? &depthStencilResolveDescription : DE_NULL,	// const void*						pNext;
		(VkSubpassDescriptionFlags)0,									// VkSubpassDescriptionFlags		flags
		VK_PIPELINE_BIND_POINT_GRAPHICS,								// VkPipelineBindPoint				pipelineBindPoint
		0u,																// deUint32							viewMask;
		0u,																// deUint32							inputAttachmentCount
		DE_NULL,														// const VkAttachmentReference2*	pInputAttachments
		hasColor ? 1u : 0u,												// deUint32							colorAttachmentCount
		hasColor ? &colorAttachmentRef : DE_NULL,						// const VkAttachmentReference2*	pColorAttachments
		hasColor ? &resolveAttachmentRef : DE_NULL,						// const VkAttachmentReference2*	pResolveAttachments
		hasDepthStencil ? &depthStencilAttachmentRef : DE_NULL,			// const VkAttachmentReference2*	pDepthStencilAttachment
		0u,																// deUint32							preserveAttachmentCount
		DE_NULL															// const deUint32*					pPreserveAttachments
	};

	const VkRenderPassCreateInfo2			renderPassInfo						=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,								// VkStructureType                   sType
		DE_NULL,																	// const void*                       pNext
		(VkRenderPassCreateFlags)0,													// VkRenderPassCreateFlags           flags
		(deUint32)attachmentDescriptions.size(),									// deUint32                          attachmentCount
		attachmentDescriptions.size() > 0 ? &attachmentDescriptions[0] : DE_NULL,	// const VkAttachmentDescription2*    pAttachments
		1u,																			// deUint32                          subpassCount
		&subpassDescription,														// const VkSubpassDescription2*       pSubpasses
		0u,																			// deUint32                          dependencyCount
		DE_NULL,																	// const VkSubpassDependency*        pDependencies
		0u,																			// deUint32						correlatedViewMaskCount;
		DE_NULL,																	// const deUint32*					pCorrelatedViewMasks;
	};

	return createRenderPass2(vk, device, &renderPassInfo, DE_NULL);
}

tcu::TestStatus EarlyFragmentSampleMaskTestInstance::iterate (void)
{
	const DeviceInterface&		vk					= m_context.getDeviceInterface();
	const InstanceInterface&	vki					= m_context.getInstanceInterface();
	const VkDevice				device				= m_context.getDevice();
	const VkPhysicalDevice		physDevice			= m_context.getPhysicalDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();
	const deUint32				queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&					allocator			= m_context.getDefaultAllocator();
	const VkFormat				colorFormat			= VK_FORMAT_R8G8B8A8_UNORM;

	DE_ASSERT(m_useTestAttachment);
	DE_UNREF(m_useTestAttachment);

	// Test attachment (depth or stencil)
	static const VkFormat stencilFormats[] =
	{
		// One of the following formats must be supported, as per spec requirement.
		VK_FORMAT_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
	};

	const VkFormat depthStencilFormat = (m_testMode == MODE_STENCIL ? pickSupportedDepthStencilFormat(vki, physDevice, DE_LENGTH_OF_ARRAY(stencilFormats), stencilFormats)
										 : VK_FORMAT_D16_UNORM);		// spec requires this format to be supported

	if (depthStencilFormat == VK_FORMAT_UNDEFINED)
		return tcu::TestStatus::fail("Required depth/stencil format not supported");

	m_context.getTestContext().getLog() << tcu::TestLog::Message << "Using depth/stencil format " << getFormatName(depthStencilFormat) << tcu::TestLog::EndMessage;

	// Check support for MSAA image formats used in the test.
	VkImageFormatProperties formatProperties;
	vki.getPhysicalDeviceImageFormatProperties(physDevice, colorFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0u, &formatProperties);
	if ((formatProperties.sampleCounts & m_sampleCount) == 0)
		TCU_THROW(NotSupportedError, "Format does not support this number of samples for color format");

	vki.getPhysicalDeviceImageFormatProperties(physDevice, depthStencilFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0u, &formatProperties);
	if ((formatProperties.sampleCounts & m_sampleCount) == 0)
		TCU_THROW(NotSupportedError, "Format does not support this number of samples for depth-stencil format");

	// Color attachment
	const tcu::IVec2				renderSize				= tcu::IVec2(32, 32);
	const VkImageSubresourceRange	colorSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType			sType;
		DE_NULL,																	// const void*				pNext;
		(VkImageCreateFlags)0,														// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,															// VkImageType				imageType;
		colorFormat,																// VkFormat					format;
		makeExtent3D(renderSize.x(), renderSize.y(), 1),							// VkExtent3D				extent;
		1u,																			// deUint32					mipLevels;
		1u,																			// deUint32					arrayLayers;
		(VkSampleCountFlagBits)m_sampleCount,										// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling			tiling;
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,		// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode			sharingMode;
		0u,																			// deUint32					queueFamilyIndexCount;
		DE_NULL,																	// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,													// VkImageLayout			initialLayout;
	};
	const Unique<VkImage>			colorImage				(makeImage(vk, device, imageParams));
	const UniquePtr<Allocation>		colorImageAlloc			(bindImage(vk, device, allocator, *colorImage, MemoryRequirement::Any));
	const Unique<VkImageView>		colorImageView			(makeImageView(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange));

	const Unique<VkImage>			resolveColorImage		(makeImage(vk, device, makeImageCreateInfo(renderSize, colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)));
	const UniquePtr<Allocation>		resolveColorImageAlloc	(bindImage(vk, device, allocator, *resolveColorImage, MemoryRequirement::Any));
	const Unique<VkImageView>		resolveColorImageView	(makeImageView(vk, device, *resolveColorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange));

	// Depth-Stencil attachment
	const VkImageSubresourceRange	depthStencilSubresourceRange	= makeImageSubresourceRange(getImageAspectFlags(depthStencilFormat), 0u, 1u, 0u, 1u);

	const VkImageCreateInfo depthStencilImageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,												// VkStructureType			sType;
		DE_NULL,																			// const void*				pNext;
		(VkImageCreateFlags)0,																// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,																	// VkImageType				imageType;
		depthStencilFormat,																			// VkFormat					format;
		makeExtent3D(renderSize.x(), renderSize.y(), 1),									// VkExtent3D				extent;
		1u,																					// deUint32					mipLevels;
		1u,																					// deUint32					arrayLayers;
		(VkSampleCountFlagBits)m_sampleCount,												// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,															// VkImageTiling			tiling;
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,		// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,															// VkSharingMode			sharingMode;
		0u,																					// deUint32					queueFamilyIndexCount;
		DE_NULL,																			// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,															// VkImageLayout			initialLayout;
	};
	const Unique<VkImage>			depthStencilImage				(makeImage(vk, device, depthStencilImageParams));
	const UniquePtr<Allocation>		depthStencilImageAlloc			(bindImage(vk, device, allocator, *depthStencilImage, MemoryRequirement::Any));
	const Unique<VkImageView>		depthStencilImageView			(makeImageView(vk, device, *depthStencilImage, VK_IMAGE_VIEW_TYPE_2D, depthStencilFormat, depthStencilSubresourceRange));

	const Unique<VkImage>			resolveDepthStencilImage				(makeImage(vk, device, makeImageCreateInfo(renderSize, depthStencilFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)));
	const UniquePtr<Allocation>		resolveDepthStencilImageAlloc			(bindImage(vk, device, allocator, *resolveDepthStencilImage, MemoryRequirement::Any));
	const Unique<VkImageView>		resolveDepthStencilImageView			(makeImageView(vk, device, *resolveDepthStencilImage, VK_IMAGE_VIEW_TYPE_2D, depthStencilFormat, depthStencilSubresourceRange));

	const VkImageView				attachmentImages[]		= { *colorImageView, *depthStencilImageView, *resolveColorImageView, *resolveDepthStencilImageView };
	const deUint32					numUsedAttachmentImages = DE_LENGTH_OF_ARRAY(attachmentImages);

	// Vertex buffer

	const deUint32					numVertices				= 6u;
	const VkDeviceSize				vertexBufferSizeBytes	= sizeof(tcu::Vec4) * numVertices;
	const Unique<VkBuffer>			vertexBuffer			(makeBuffer(vk, device, vertexBufferSizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
	const UniquePtr<Allocation>		vertexBufferAlloc		(bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible));

	{
		tcu::Vec4* const pVertices = reinterpret_cast<tcu::Vec4*>(vertexBufferAlloc->getHostPtr());

		pVertices[0] = tcu::Vec4( 1.0f, -1.0f,  0.5f,  1.0f);
		pVertices[1] = tcu::Vec4(-1.0f, -1.0f,  0.0f,  1.0f);
		pVertices[2] = tcu::Vec4(-1.0f,  1.0f,  0.5f,  1.0f);

		pVertices[3] = tcu::Vec4(-1.0f,  1.0f,  0.5f,  1.0f);
		pVertices[4] = tcu::Vec4( 1.0f,  1.0f,  1.0f,  1.0f);
		pVertices[5] = tcu::Vec4( 1.0f, -1.0f,  0.5f,  1.0f);

		flushAlloc(vk, device, *vertexBufferAlloc);
		// No barrier needed, flushed memory is automatically visible
	}

	// Result buffer

	const VkDeviceSize				resultBufferSizeBytes	= sizeof(deUint32);
	const Unique<VkBuffer>			resultBuffer			(makeBuffer(vk, device, resultBufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
	const UniquePtr<Allocation>		resultBufferAlloc		(bindBuffer(vk, device, allocator, *resultBuffer, MemoryRequirement::HostVisible));

	{
		deUint32* const pData = static_cast<deUint32*>(resultBufferAlloc->getHostPtr());

		*pData = 0;
		flushAlloc(vk, device, *resultBufferAlloc);
	}

	// Render result buffer (to retrieve color attachment contents)

	const VkDeviceSize				colorBufferSizeBytes	= tcu::getPixelSize(mapVkFormat(colorFormat)) * renderSize.x() * renderSize.y();
	const Unique<VkBuffer>			colorBuffer				(makeBuffer(vk, device, colorBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>		colorBufferAlloc		(bindBuffer(vk, device, allocator, *colorBuffer, MemoryRequirement::HostVisible));

	// Depth stencil result buffer (to retrieve depth-stencil attachment contents)

	const VkDeviceSize				dsBufferSizeBytes	= tcu::getPixelSize(mapVkFormat(depthStencilFormat)) * renderSize.x() * renderSize.y();
	const Unique<VkBuffer>			dsBuffer			(makeBuffer(vk, device, dsBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>		dsBufferAlloc		(bindBuffer(vk, device, allocator, *dsBuffer, MemoryRequirement::HostVisible));

	// Descriptors

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet				 (makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));
	const VkDescriptorBufferInfo  resultBufferDescriptorInfo = makeDescriptorBufferInfo(resultBuffer.get(), 0ull, resultBufferSizeBytes);

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultBufferDescriptorInfo)
		.update(vk, device);

	// Pipeline

	const Unique<VkShaderModule>	vertexModule  (createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>	fragmentModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u));

	const Unique<VkRenderPass>		renderPass	  (makeRenderPass(vk, device, colorFormat, depthStencilFormat));
	const Unique<VkFramebuffer>		framebuffer	  (makeFramebuffer(vk, device, *renderPass, numUsedAttachmentImages, attachmentImages, renderSize.x(), renderSize.y()));
	const Unique<VkPipelineLayout>	pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline>		pipeline	  (makeGraphicsPipeline(vk, device, *pipelineLayout, *renderPass, *vertexModule, *fragmentModule, renderSize,
																		(m_testMode == MODE_DEPTH), (m_testMode == MODE_STENCIL),
																		VK_STENCIL_OP_INCREMENT_AND_CLAMP, VK_STENCIL_OP_INCREMENT_AND_CLAMP));
	const Unique<VkCommandPool>		cmdPool		  (createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer	  (allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Draw commands

	{
		const VkRect2D renderArea = {
			makeOffset2D(0, 0),
			makeExtent2D(renderSize.x(), renderSize.y()),
		};
		const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
		const VkDeviceSize vertexBufferOffset = 0ull;

		beginCommandBuffer(vk, *cmdBuffer);

		{
			const VkImageMemoryBarrier barriers[] = {
				makeImageMemoryBarrier(
					0u, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					*colorImage, colorSubresourceRange),
				makeImageMemoryBarrier(
					0u, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					*depthStencilImage, depthStencilSubresourceRange),
				makeImageMemoryBarrier(
					0u,  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					*resolveColorImage, colorSubresourceRange),
				makeImageMemoryBarrier(
					0u, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					*resolveDepthStencilImage, depthStencilSubresourceRange),
			};

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u,
				0u, DE_NULL, 0u, DE_NULL, DE_LENGTH_OF_ARRAY(barriers), barriers);
		}

		// Will clear the attachments with specified depth and stencil values.
		{
			const VkClearValue			clearValues[]		=
				{
					makeClearValueColor(clearColor),						// attachment 0
					makeClearValueDepthStencil(0.5f, 3u),					// attachment 1
					makeClearValueColor(clearColor),						// attachment 2
					makeClearValueDepthStencil(0.5f, 3u),					// attachment 3
				};

			const VkRenderPassBeginInfo	renderPassBeginInfo	=
				{
					VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType         sType;
					DE_NULL,									// const void*             pNext;
					*renderPass,								// VkRenderPass            renderPass;
					*framebuffer,								// VkFramebuffer           framebuffer;
					renderArea,									// VkRect2D                renderArea;
					DE_LENGTH_OF_ARRAY(clearValues),			// deUint32                clearValueCount;
					clearValues,								// const VkClearValue*     pClearValues;
				};

			vk.cmdBeginRenderPass(*cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		}

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);
		vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);

		// Mask half of the attachment image with value that will pass the stencil test.
		if (m_testMode == MODE_STENCIL)
			commandClearStencilAttachment(vk, *cmdBuffer, makeOffset2D(0, 0), makeExtent2D(renderSize.x()/2, renderSize.y()), 1u);

		vk.cmdDraw(*cmdBuffer, numVertices, 1u, 0u, 0u);
		endRenderPass(vk, *cmdBuffer);

		copyImageToBuffer(vk, *cmdBuffer, *resolveColorImage, *colorBuffer, renderSize, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
		VkImageAspectFlags dsAspect = m_testMode == MODE_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_STENCIL_BIT;
		copyImageToBuffer(vk, *cmdBuffer, *resolveDepthStencilImage, *dsBuffer, renderSize, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1u, dsAspect, dsAspect);

		endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(vk, device, queue, *cmdBuffer);
	}

	// Verify color output
	{
		invalidateAlloc(vk, device, *colorBufferAlloc);

		const tcu::ConstPixelBufferAccess	imagePixelAccess(mapVkFormat(colorFormat), renderSize.x(), renderSize.y(), 1, colorBufferAlloc->getHostPtr());
		const tcu::TextureLevel				referenceImage	= generateReferenceColorImage(mapVkFormat(colorFormat), renderSize);
		if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", referenceImage.getAccess(), imagePixelAccess, tcu::Vec4(0.02f), tcu::COMPARE_LOG_RESULT))
			printf("Rendered color image is not correct");
	}

	// Verify depth-stencil output
	{
		invalidateAlloc(vk, device, *dsBufferAlloc);
		tcu::TextureFormat format = mapVkFormat(depthStencilFormat);
		const tcu::ConstPixelBufferAccess	dsPixelAccess (format, renderSize.x(), renderSize.y(), 1, dsBufferAlloc->getHostPtr());

		for(int z = 0; z < dsPixelAccess.getDepth(); z++)
		for(int y = 0; y < dsPixelAccess.getHeight(); y++)
		for(int x = 0; x < dsPixelAccess.getWidth(); x++)
		{
			float	depthValue		= (m_testMode == MODE_DEPTH) ? dsPixelAccess.getPixDepth(x, y, z) : 0.0f;
			int		stencilValue	= (m_testMode == MODE_STENCIL) ? dsPixelAccess.getPixStencil(x, y, z) : 0;

			// Depth test should write to the depth buffer even when there is a discard in the fragment shader,
			// when early fragment tests are enabled.
			if (m_testMode == MODE_DEPTH)
			{
				if (m_useEarlyTests && ((x + y) < 31) && depthValue >= 0.5f)
				{
					std::ostringstream error;
					error << "Rendered depth value [ "<< x << ", " << y << ", " << z << "] is not correct: " << depthValue << " >= 0.5f";
					TCU_FAIL(error.str().c_str());
				}
				// When early fragment tests are disabled, the depth test happens after the fragment shader, but as we are discarding
				// all fragments, the stored value in the depth buffer should be the clear one (0.5f).
				if (!m_useEarlyTests && deAbs(depthValue - 0.5f) > 0.01f)
				{
					std::ostringstream error;
					error << "Rendered depth value [ "<< x << ", " << y << ", " << z << "] is not correct: " << depthValue << " != 0.5f";
					TCU_FAIL(error.str().c_str());
				}
			}

			if (m_testMode == MODE_STENCIL)
			{
				if (m_useEarlyTests && ((x < 16 && stencilValue != 2u) || (x >= 16 && stencilValue != 4u)))
				{
					std::ostringstream error;
					error << "Rendered stencil value [ "<< x << ", " << y << ", " << z << "] is not correct: " << stencilValue << " != ";
					error << (x < 16 ? 2u : 4u);
					TCU_FAIL(error.str().c_str());
				}

				if (!m_useEarlyTests && ((x < 16 && stencilValue != 1u) || (x >= 16 && stencilValue != 3u)))
				{
					std::ostringstream error;
					error << "Rendered stencil value [ "<< x << ", " << y << ", " << z << "] is not correct: " << stencilValue << " != ";
					error << (x < 16 ? 1u : 3u);
					TCU_FAIL(error.str().c_str());
				}
			}
		}
	}

	// Verify we process all the fragments
	{
		invalidateAlloc(vk, device, *resultBufferAlloc);

		const int  actualCounter	   = *static_cast<deInt32*>(resultBufferAlloc->getHostPtr());
		const bool expectPartialResult = m_useEarlyTests;
		const int  expectedCounter	   = expectPartialResult ? renderSize.x() * renderSize.y() / 2 : renderSize.x() * renderSize.y();
		const int  tolerance		   = expectPartialResult ? de::max(renderSize.x(), renderSize.y()) * 3	: 0;
		const int  expectedMin         = de::max(0, expectedCounter - tolerance);

		tcu::TestLog& log = m_context.getTestContext().getLog();
		log << tcu::TestLog::Message << "Minimum expected value: " + de::toString(expectedMin) << tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "Result value: " << de::toString(actualCounter) << tcu::TestLog::EndMessage;

		if (expectedMin <= actualCounter)
			return tcu::TestStatus::pass("Success");
		else
			return tcu::TestStatus::fail("Value out of range");
	}
}

class EarlyFragmentSampleMaskTest : public EarlyFragmentTest
{
public:
						EarlyFragmentSampleMaskTest	(tcu::TestContext&		testCtx,
													 const std::string		name,
													 const deUint32			flags,
													 const deUint32			sampleCount);

	void				initPrograms				(SourceCollections&		programCollection) const override;
	TestInstance*		createInstance				(Context&				context) const override;
	void				checkSupport				(Context&				context) const override;

private:
	const deUint32		m_flags;
	const deUint32		m_sampleCount;
};

EarlyFragmentSampleMaskTest::EarlyFragmentSampleMaskTest (tcu::TestContext& testCtx, const std::string name, const deUint32 flags, const deUint32 sampleCount)
	: EarlyFragmentTest	(testCtx, name, flags)
	, m_flags (flags)
	, m_sampleCount (sampleCount)
{
}

TestInstance* EarlyFragmentSampleMaskTest::createInstance (Context& context) const
{
	return new EarlyFragmentSampleMaskTestInstance(context, m_flags, m_sampleCount);
}

void EarlyFragmentSampleMaskTest::initPrograms(SourceCollections &programCollection) const
{
	// Vertex
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
			<< "\n"
			<< "layout(location = 0) in highp vec4 position;\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "   vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_Position = position;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
	}

	// Fragment
	{
		if ((m_flags & FLAG_EARLY_AND_LATE_FRAGMENT_TESTS) == 0)
		{
			const bool useEarlyTests = (m_flags & FLAG_DONT_USE_EARLY_FRAGMENT_TESTS) == 0;
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
				<< "\n"
				<< (useEarlyTests ? "layout(early_fragment_tests) in;\n" : "")
				<< "layout(location = 0) out highp vec4 fragColor;\n"
				<< "\n"
				<< "layout(binding = 0) coherent buffer Output {\n"
				<< "    uint result;\n"
				<< "} sb_out;\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    atomicAdd(sb_out.result, 1u);\n"
				<< "    gl_SampleMask[0] = 0x0;\n"
				<< "    fragColor = vec4(1.0, 1.0, 0.0, 1.0);\n"
				<< "    discard;\n"
				<< "}\n";

			programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
		}
		else
		{
			const SpirVAsmBuildOptions	buildOptionsSpr(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3);
			const std::string			option	= (((m_flags & FLAG_DONT_USE_EARLY_FRAGMENT_TESTS) != 0)					?
													"OpExecutionMode %4 DepthReplacing\nOpExecutionMode %4 DepthGreater\n"	:
													"");
			const std::string			src		=
				"; SPIR - V\n"
				"; Version: 1.0\n"
				"; Generator: Khronos Glslang Reference Front End; 10\n"
				"; Bound: 30\n"
				"; Schema: 0\n"
				"OpCapability Shader\n"
				"OpExtension \"SPV_AMD_shader_early_and_late_fragment_tests\"\n"
				"%1 = OpExtInstImport \"GLSL.std.450\"\n"
				"OpMemoryModel Logical GLSL450\n"
				"OpEntryPoint Fragment %4 \"main\" %19 %25\n"
				"OpExecutionMode %4 OriginUpperLeft\n"
				"OpExecutionMode %4 EarlyAndLateFragmentTestsAMD\n"
				+ option +
				"OpMemberDecorate %7 0 Coherent\n"
				"OpMemberDecorate %7 0 Offset 0\n"
				"OpDecorate %7 BufferBlock\n"
				"OpDecorate %9 DescriptorSet 0\n"
				"OpDecorate %9 Binding 0\n"
				"OpDecorate %19 BuiltIn SampleMask\n"
				"OpDecorate %25 Location 0\n"
				"%2 = OpTypeVoid\n"
				"%3 = OpTypeFunction %2\n"
				"%6 = OpTypeInt 32 0\n"
				"%7 = OpTypeStruct %6\n"
				"%8 = OpTypePointer Uniform %7\n"
				"%9 = OpVariable %8 Uniform\n"
				"%10 = OpTypeInt 32 1\n"
				"%11 = OpConstant %10 0\n"
				"%12 = OpTypePointer Uniform %6\n"
				"%14 = OpConstant %6 1\n"
				"%15 = OpConstant %6 0\n"
				"%17 = OpTypeArray %10 %14\n"
				"%18 = OpTypePointer Output %17\n"
				"%19 = OpVariable %18 Output\n"
				"%20 = OpTypePointer Output %10\n"
				"%22 = OpTypeFloat 32\n"
				"%23 = OpTypeVector %22 4\n"
				"%24 = OpTypePointer Output %23\n"
				"%25 = OpVariable %24 Output\n"
				"%26 = OpConstant %22 1\n"
				"%27 = OpConstant %22 0\n"
				"%28 = OpConstantComposite %23 %26 %26 %27 %26\n"
				"%4 = OpFunction %2 None %3\n"
				"%5 = OpLabel\n"
				"%13 = OpAccessChain %12 %9 %11\n"
				"%16 = OpAtomicIAdd %6 %13 %14 %15 %14\n"
				"%21 = OpAccessChain %20 %19 %11\n"
				"OpStore %21 %11\n"
				"OpStore %25 %28\n"
				"OpKill\n"
				"OpFunctionEnd\n";
			programCollection.spirvAsmSources.add("frag") << src << buildOptionsSpr;
		}
	}
}

void EarlyFragmentSampleMaskTest::checkSupport(Context& context) const
{
	EarlyFragmentTest::checkSupport(context);

	context.requireDeviceFunctionality("VK_KHR_depth_stencil_resolve");
}

struct SampleCountTestParams
{
	deUint32	sampleCount;
	bool		earlyAndLate;
	bool		alphaToCoverage;
	bool		useMaintenance5;
};

class EarlyFragmentSampleCountTestInstance : public EarlyFragmentTestInstance
{
public:
						EarlyFragmentSampleCountTestInstance	(Context& context, const SampleCountTestParams& testParams);

	tcu::TestStatus		iterate									(void);

private:
	tcu::TextureLevel	generateReferenceColorImage				(const tcu::TextureFormat format, const tcu::IVec2& renderSize);

	Move<VkRenderPass>	makeRenderPass							(const DeviceInterface&		vk,
																 const VkDevice				device,
																 const VkFormat				colorFormat,
																 const VkFormat				depthStencilFormat);

	Move<VkPipeline>	makeGraphicsPipeline					(const DeviceInterface&		vk,
																 const VkDevice				device,
																 const VkPipelineLayout		pipelineLayout,
																 const VkRenderPass			renderPass,
																 const VkShaderModule		vertexModule,
																 const VkShaderModule		fragmentModule,
																 const tcu::IVec2&			renderSize,
																 const VkSampleMask			sampleMask);

	const SampleCountTestParams		m_testParams;
};

EarlyFragmentSampleCountTestInstance::EarlyFragmentSampleCountTestInstance (Context& context, const SampleCountTestParams& testParams)
	: EarlyFragmentTestInstance	(context, FLAG_TEST_DEPTH)
	, m_testParams				(testParams)
{
}

Move<VkPipeline> EarlyFragmentSampleCountTestInstance::makeGraphicsPipeline	(const DeviceInterface&	vk,
																			const VkDevice			device,
																			const VkPipelineLayout	pipelineLayout,
																			const VkRenderPass		renderPass,
																			const VkShaderModule	vertexModule,
																			const VkShaderModule	fragmentModule,
																			const tcu::IVec2&		renderSize,
																			const VkSampleMask		sampleMask)
{
	const std::vector<VkViewport>				viewports					(1, makeViewport(renderSize));
	const std::vector<VkRect2D>					scissors					(1, makeRect2D(renderSize));

	const VkPipelineDepthStencilStateCreateInfo	depthStencilStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType
		DE_NULL,													// const void*								pNext
		0u,															// VkPipelineDepthStencilStateCreateFlags	flags
		VK_TRUE,													// VkBool32									depthTestEnable
		VK_TRUE,													// VkBool32									depthWriteEnable
		VK_COMPARE_OP_LESS,											// VkCompareOp								depthCompareOp
		VK_FALSE,													// VkBool32									depthBoundsTestEnable
		VK_FALSE,													// VkBool32									stencilTestEnable
		{},															// VkStencilOpState							front
		{},															// VkStencilOpState							back
		0.0f,														// float									minDepthBounds
		1.0f														// float									maxDepthBounds
	};

	const VkPipelineMultisampleStateCreateInfo	multisampleStateCreateInfo	=
	{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType
		DE_NULL,													// const void*								pNext
		0u,															// VkPipelineMultisampleStateCreateFlags	flags
		(VkSampleCountFlagBits)m_testParams.sampleCount,			// VkSampleCountFlagBits					rasterizationSamples
		DE_FALSE,													// VkBool32									sampleShadingEnable
		0.0f,														// float									minSampleShading
		&sampleMask,												// const VkSampleMask*						pSampleMask
		m_testParams.alphaToCoverage,								// VkBool32									alphaToCoverageEnable
		DE_FALSE,													// VkBool32									alphaToOneEnable
	};

	return vk::makeGraphicsPipeline(vk,										// const DeviceInterface&							vk
									device,									// const VkDevice									device
									pipelineLayout,							// const VkPipelineLayout							pipelineLayout
									vertexModule,							// const VkShaderModule								vertexShaderModule
									DE_NULL,								// const VkShaderModule								tessellationControlModule
									DE_NULL,								// const VkShaderModule								tessellationEvalModule
									DE_NULL,								// const VkShaderModule								geometryShaderModule
									fragmentModule,							// const VkShaderModule								fragmentShaderModule
									renderPass,								// const VkRenderPass								renderPass
									viewports,								// const std::vector<VkViewport>&					viewports
									scissors,								// const std::vector<VkRect2D>&						scissors
									VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// const VkPrimitiveTopology						topology
									0u,										// const deUint32									subpass
									0u,										// const deUint32									patchControlPoints
									DE_NULL,								// const VkPipelineVertexInputStateCreateInfo*		vertexInputStateCreateInfo
									DE_NULL,								// const VkPipelineRasterizationStateCreateInfo*	rasterizationStateCreateInfo
									&multisampleStateCreateInfo,			// const VkPipelineMultisampleStateCreateInfo*		multisampleStateCreateInfo
									&depthStencilStateCreateInfo);			// const VkPipelineDepthStencilStateCreateInfo*		depthStencilStateCreateInfo
}

Move<VkRenderPass> EarlyFragmentSampleCountTestInstance::makeRenderPass	(const DeviceInterface&	vk,
																		 const VkDevice			device,
																		 const VkFormat			colorFormat,
																		 const VkFormat			depthStencilFormat)
{
	const VkAttachmentDescription			colorAttachmentDescription			=
	{
		(VkAttachmentDescriptionFlags)0,					// VkAttachmentDescriptionFlags	flags;
		colorFormat,										// VkFormat						format;
		(VkSampleCountFlagBits)m_testParams.sampleCount,	// VkSampleCountFlagBits		samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp			loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp			storeOp;
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,					// VkAttachmentLoadOp			stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,					// VkAttachmentStoreOp			stencilStoreOp
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout				initialLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL			// VkImageLayout				finalLayout
	};

	const VkAttachmentDescription			depthStencilAttachmentDescription	=
	{
		(VkAttachmentDescriptionFlags)0,					// VkStructureType			sType;
		depthStencilFormat,									// VkFormat					format
		(VkSampleCountFlagBits)m_testParams.sampleCount,	// VkSampleCountFlagBits	samples
		VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp		loadOp
		VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp		storeOp
		VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp		stencilLoadOp
		VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp		stencilStoreOp
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// VkImageLayout			initialLayout
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL	// VkImageLayout			finalLayout
	};

	const VkAttachmentDescription			resolveAttachmentDescription		=
	{

		(VkAttachmentDescriptionFlags)0,			// VkSubpassDescriptionFlags	flags
		colorFormat,								// VkFormat						format
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits		samples
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp			loadOp
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp			storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp			stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp			stencilStoreOp
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout				initialLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout				finalLayout
	};

	std::vector<VkAttachmentDescription>	attachmentDescriptions;

	attachmentDescriptions.push_back(colorAttachmentDescription);
	attachmentDescriptions.push_back(depthStencilAttachmentDescription);
	attachmentDescriptions.push_back(resolveAttachmentDescription);

	const VkAttachmentReference				colorAttachmentRef					=
	{
		0u,											// deUint32			attachment
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout	layout
	};

	const VkAttachmentReference				depthStencilAttachmentRef			=
	{
		1u,													// deUint32			attachment
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// VkImageLayout	layout
	};

	const VkAttachmentReference				resolveAttachmentRef				=
	{
		2u,											// deUint32			attachment
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout	layout
	};

	const VkSubpassDescription				subpassDescription					=
	{
		(VkSubpassDescriptionFlags)0,		// VkSubpassDescriptionFlags		flags
		VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint				pipelineBindPoint
		0u,									// deUint32							inputAttachmentCount
		DE_NULL,							// const VkAttachmentReference2*	pInputAttachments
		1u,									// deUint32							colorAttachmentCount
		&colorAttachmentRef,				// const VkAttachmentReference2*	pColorAttachments
		&resolveAttachmentRef,				// const VkAttachmentReference2*	pResolveAttachments
		&depthStencilAttachmentRef,			// const VkAttachmentReference2*	pDepthStencilAttachment
		0u,									// deUint32							preserveAttachmentCount
		DE_NULL								// const deUint32*					pPreserveAttachments
	};

	const VkRenderPassCreateInfo			renderPassInfo						=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType					sType;
		DE_NULL,									// const void*						pNext;
		(VkRenderPassCreateFlags)0,					// VkRenderPassCreateFlags			flags;
		(deUint32)attachmentDescriptions.size(),	// uint32_t							attachmentCount;
		attachmentDescriptions.data(),				// const VkAttachmentDescription*	pAttachments;
		1u,											// uint32_t							subpassCount;
		&subpassDescription,						// const VkSubpassDescription*		pSubpasses;
		0u,											// uint32_t							dependencyCount;
		DE_NULL										// const VkSubpassDependency*		pDependencies;
	};

	return createRenderPass(vk, device, &renderPassInfo, DE_NULL);
}

tcu::TextureLevel EarlyFragmentSampleCountTestInstance::generateReferenceColorImage(const tcu::TextureFormat format, const tcu::IVec2 &renderSize)
{
	tcu::TextureLevel	image		(format, renderSize.x(), renderSize.y());
	const tcu::Vec4		clearColor	= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);

	tcu::clear(image.getAccess(), clearColor);

	return image;
}

tcu::TestStatus EarlyFragmentSampleCountTestInstance::iterate (void)
{
	const DeviceInterface&				vk							= m_context.getDeviceInterface();
	const VkDevice						device						= m_context.getDevice();
	const VkQueue						queue						= m_context.getUniversalQueue();
	const deUint32						queueFamilyIndex			= m_context.getUniversalQueueFamilyIndex();
	Allocator&							allocator					= m_context.getDefaultAllocator();
	const VkFormat						colorFormat					= VK_FORMAT_R8G8B8A8_UNORM;
	const VkFormat						depthFormat					= VK_FORMAT_D16_UNORM;
	VkQueryPool							queryPool;
	const deUint32						queryCount					= 2u;
	std::vector<VkDeviceSize>			sampleCounts				(queryCount);

	// Create a query pool for storing the occlusion query result
	{
		VkQueryPoolCreateInfo			queryPoolInfo
		{
			VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,	// VkStructureType					sType;
			DE_NULL,									// const void*						pNext;
			(VkQueryPoolCreateFlags)0,					// VkQueryPoolCreateFlags			flags;
			VK_QUERY_TYPE_OCCLUSION,					// VkQueryType						queryType;
			queryCount,									// uint32_t							queryCount;
			0u,											// VkQueryPipelineStatisticFlags	pipelineStatistics;
		};

		VK_CHECK(vk.createQueryPool(device, &queryPoolInfo, NULL, &queryPool));
	}

	m_context.getTestContext().getLog() << tcu::TestLog::Message << "Using depth format " << getFormatName(VK_FORMAT_D16_UNORM) << tcu::TestLog::EndMessage;

	// Color attachment
	const tcu::IVec2					renderSize					= tcu::IVec2(32, 32);
	const VkImageSubresourceRange		colorSubresourceRange		= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);

	const VkImageCreateInfo				imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,				// VkStructureType			sType;
		DE_NULL,											// const void*				pNext;
		(VkImageCreateFlags)0,								// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,									// VkImageType				imageType;
		colorFormat,										// VkFormat					format;
		makeExtent3D(renderSize.x(), renderSize.y(), 1),	// VkExtent3D				extent;
		1u,													// deUint32					mipLevels;
		1u,													// deUint32					arrayLayers;
		(VkSampleCountFlagBits)m_testParams.sampleCount,	// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,							// VkImageTiling			tiling;
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,				// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,							// VkSharingMode			sharingMode;
		0u,													// deUint32					queueFamilyIndexCount;
		DE_NULL,											// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout			initialLayout;
	};

	const Unique<VkImage>				colorImage					(makeImage(vk, device, imageParams));
	const UniquePtr<Allocation>			colorImageAlloc				(bindImage(vk, device, allocator, *colorImage, MemoryRequirement::Any));
	const Unique<VkImageView>			colorImageView				(makeImageView(vk, device, *colorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange));

	const Unique<VkImage>				resolveColorImage			(makeImage(vk, device, makeImageCreateInfo(renderSize, colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)));
	const UniquePtr<Allocation>			resolveColorImageAlloc		(bindImage(vk, device, allocator, *resolveColorImage, MemoryRequirement::Any));
	const Unique<VkImageView>			resolveColorImageView		(makeImageView(vk, device, *resolveColorImage, VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresourceRange));

	// Depth-Stencil attachment
	const VkImageSubresourceRange		depthSubresourceRange		= makeImageSubresourceRange(getImageAspectFlags(depthFormat), 0u, 1u, 0u, 1u);

	const VkImageCreateInfo				depthImageParams			=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,				// VkStructureType			sType;
		DE_NULL,											// const void*				pNext;
		(VkImageCreateFlags)0,								// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,									// VkImageType				imageType;
		depthFormat,										// VkFormat					format;
		makeExtent3D(renderSize.x(), renderSize.y(), 1),	// VkExtent3D				extent;
		1u,													// deUint32					mipLevels;
		1u,													// deUint32					arrayLayers;
		(VkSampleCountFlagBits)m_testParams.sampleCount,	// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,							// VkImageTiling			tiling;
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,		// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,							// VkSharingMode			sharingMode;
		0u,													// deUint32					queueFamilyIndexCount;
		DE_NULL,											// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout			initialLayout;
	};

	const Unique<VkImage>				depthImage					(makeImage(vk, device, depthImageParams));
	const UniquePtr<Allocation>			depthImageAlloc				(bindImage(vk, device, allocator, *depthImage, MemoryRequirement::Any));
	const Unique<VkImageView>			depthImageView				(makeImageView(vk, device, *depthImage, VK_IMAGE_VIEW_TYPE_2D, depthFormat, depthSubresourceRange));

	const VkImageView					attachmentImages[]			= { *colorImageView, *depthImageView, *resolveColorImageView };
	const deUint32						numUsedAttachmentImages		= DE_LENGTH_OF_ARRAY(attachmentImages);

	// Vertex buffer
	const deUint32						numVertices					= 6u;
	const VkDeviceSize					vertexBufferSizeBytes		= 256 * numVertices;
	const Unique<VkBuffer>				vertexBuffer				(makeBuffer(vk, device, vertexBufferSizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
	const UniquePtr<Allocation>			vertexBufferAlloc			(bindBuffer(vk, device, allocator, *vertexBuffer, MemoryRequirement::HostVisible));

	{
		tcu::Vec4* const pVertices = reinterpret_cast<tcu::Vec4*>(vertexBufferAlloc->getHostPtr());

		pVertices[0] = tcu::Vec4( 1.0f, -1.0f,  0.0f,  1.0f);
		pVertices[1] = tcu::Vec4(-1.0f, -1.0f,  0.0f,  1.0f);
		pVertices[2] = tcu::Vec4(-1.0f,  1.0f,  0.0f,  1.0f);

		pVertices[3] = tcu::Vec4(-1.0f,  1.0f,  1.0f,  1.0f);
		pVertices[4] = tcu::Vec4( 1.0f,  1.0f,  1.0f,  1.0f);
		pVertices[5] = tcu::Vec4( 1.0f, -1.0f,  1.0f,  1.0f);

		flushAlloc(vk, device, *vertexBufferAlloc);
		// No barrier needed, flushed memory is automatically visible
	}

	// Render result buffer (to retrieve color attachment contents)

	const VkDeviceSize					colorBufferSizeBytes		= tcu::getPixelSize(mapVkFormat(colorFormat)) * renderSize.x() * renderSize.y();
	const Unique<VkBuffer>				colorBufferNoEarlyResults	(makeBuffer(vk, device, colorBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>			colorBufferAllocNoEarly		(bindBuffer(vk, device, allocator, *colorBufferNoEarlyResults, MemoryRequirement::HostVisible));
	const Unique<VkBuffer>				colorBufferEarlyResults		(makeBuffer(vk, device, colorBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	const UniquePtr<Allocation>			colorBufferAllocEarly		(bindBuffer(vk, device, allocator, *colorBufferEarlyResults, MemoryRequirement::HostVisible));

	// Pipeline

	const Unique<VkShaderModule>		vertexModule				(createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0u));
	const Unique<VkShaderModule>		fragmentModuleNoEarly		(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0u));
	const Unique<VkShaderModule>		fragmentModuleEarlyFrag		(createShaderModule(vk, device, m_context.getBinaryCollection().get("frag_early"), 0u));

	const Unique<VkRenderPass>			renderPass					(makeRenderPass(vk, device, colorFormat, depthFormat));

	const Unique<VkFramebuffer>			framebuffer					(makeFramebuffer(vk, device, *renderPass, numUsedAttachmentImages, attachmentImages, renderSize.x(), renderSize.y()));

	const Unique<VkPipelineLayout>		pipelineLayout				(makePipelineLayout(vk, device, DE_NULL));

	// When we are creating a pipeline for runs without early fragment test, we are enabling all the samples for full coverage.
	// Sample mask will be modified in a fragment shader.
	const Unique<VkPipeline>			pipelineNoEarlyFrag			(makeGraphicsPipeline(vk, device, *pipelineLayout, *renderPass, *vertexModule, *fragmentModuleNoEarly, renderSize, 0xFFFFFFFF));

	// For early fragment tests, we are enabling only half of the samples.
	const Unique<VkPipeline>			pipelineEarlyFrag			(makeGraphicsPipeline(vk, device, *pipelineLayout, *renderPass, *vertexModule, *fragmentModuleEarlyFrag, renderSize, 0xAAAAAAAA));

	// Build a command buffer

	const Unique<VkCommandPool>			cmdPool						(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer					(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	enum QueryIndex
	{
		QUERY_INDEX_NO_EARLY_FRAG = 0,
		QUERY_INDEX_EARLY_FRAG = 1
	};

	{
		const VkRect2D					renderArea					=
		{
			makeOffset2D(0, 0),
			makeExtent2D(renderSize.x(), renderSize.y()),
		};

		const tcu::Vec4					clearColor					(0.0f, 0.0f, 0.0f, 1.0f);
		const VkDeviceSize				vertexBufferOffset			= 0ull;

		beginCommandBuffer(vk, *cmdBuffer);

		// transition images to proper layouts - this cant be done with renderpass as we will use same renderpass twice
		const VkImageMemoryBarrier initialImageBarriers[]
		{
			makeImageMemoryBarrier(0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, *colorImage, colorSubresourceRange),
			makeImageMemoryBarrier(0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, *resolveColorImage, colorSubresourceRange),
			makeImageMemoryBarrier(0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, *depthImage, depthSubresourceRange)
		};
		vk.cmdPipelineBarrier(*cmdBuffer, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 2u, initialImageBarriers);
		vk.cmdPipelineBarrier(*cmdBuffer, 0, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &initialImageBarriers[2]);

		const VkClearValue				clearValues[]				=
		{
			makeClearValueColor(clearColor),		// attachment 0
			makeClearValueDepthStencil(0.5f, 3u),	// attachment 1
			makeClearValueColor(clearColor),		// attachment 2
			makeClearValueDepthStencil(0.5f, 3u),	// attachment 3
		};

		const VkRenderPassBeginInfo		renderPassBeginInfo			=
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			*renderPass,								// VkRenderPass			renderPass;
			*framebuffer,								// VkFramebuffer		framebuffer;
			renderArea,									// VkRect2D				renderArea;
			DE_LENGTH_OF_ARRAY(clearValues),			// deUint32				clearValueCount;
			clearValues,								// const VkClearValue*	pClearValues;
		};

		// Reset query pool. Must be done outside of render pass
		vk.cmdResetQueryPool(*cmdBuffer, queryPool, 0, queryCount);

		vk.cmdBeginRenderPass(*cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);

		// Run without early fragment test.
		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineNoEarlyFrag);

		vk.cmdBeginQuery(cmdBuffer.get(), queryPool, QUERY_INDEX_NO_EARLY_FRAG, VK_QUERY_CONTROL_PRECISE_BIT);
		vk.cmdDraw(*cmdBuffer, numVertices, 1u, 0u, 0u);
		vk.cmdEndQuery(cmdBuffer.get(), queryPool, QUERY_INDEX_NO_EARLY_FRAG);

		endRenderPass(vk, *cmdBuffer);

		copyImageToBuffer(vk, *cmdBuffer, *resolveColorImage, *colorBufferNoEarlyResults, renderSize, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

		// after copying from resolved image we need to switch its layout back to color attachment optimal
		const VkImageMemoryBarrier postResolveImageBarrier = makeImageMemoryBarrier(VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, *resolveColorImage, colorSubresourceRange);
		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &postResolveImageBarrier);

		vk.cmdBeginRenderPass(*cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vk.cmdBindVertexBuffers(*cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);

		// Run with early fragment test.
		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineEarlyFrag);

		vk.cmdBeginQuery(cmdBuffer.get(), queryPool, QUERY_INDEX_EARLY_FRAG, VK_QUERY_CONTROL_PRECISE_BIT);
		vk.cmdDraw(*cmdBuffer, numVertices, 1u, 0u, 0u);
		vk.cmdEndQuery(cmdBuffer.get(), queryPool, QUERY_INDEX_EARLY_FRAG);

		endRenderPass(vk, *cmdBuffer);

		copyImageToBuffer(vk, *cmdBuffer, *resolveColorImage, *colorBufferEarlyResults, renderSize, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

		endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(vk, device, queue, *cmdBuffer);
	}

	// When early fragment test is enabled, all samples are killed in fragment shader. The result color should be black.
	{
		invalidateAlloc(vk, device, *colorBufferAllocEarly);

		const tcu::ConstPixelBufferAccess	imagePixelAccess(mapVkFormat(colorFormat), renderSize.x(), renderSize.y(), 1, colorBufferAllocEarly->getHostPtr());
		const tcu::TextureLevel				referenceImage = generateReferenceColorImage(mapVkFormat(colorFormat), renderSize);

		if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare color output", "Early fragment image result comparison", referenceImage.getAccess(), imagePixelAccess, tcu::Vec4(0.02f), tcu::COMPARE_LOG_RESULT))
			return tcu::TestStatus::fail("Rendered color image is not correct");
	}

	// The image has 32x32 pixels and each pixel has m_sampleCount samples. Half of these samples are discarded before sample counting.
	// This means the reference value for passed samples is ((32 x 32) / 2 / 2) * sampleCount.
	{
		deUint64	refValue	= deUint64(deUint32((renderSize.x() * renderSize.y()) / 4) * m_testParams.sampleCount);
		deUint64	tolerance	= deUint64(refValue * 5 / 100);
		deUint64	minValue	= refValue - tolerance;
		deUint64	maxValue	= refValue + tolerance;

		VK_CHECK(vk.getQueryPoolResults(device, queryPool, 0u, queryCount, queryCount * sizeof(VkDeviceSize), sampleCounts.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));

		// Log test results
		{
			tcu::TestLog& log = m_context.getTestContext().getLog();

			log << tcu::TestLog::Message << "\nAcceptable range: " << minValue << " - " << maxValue << tcu::TestLog::EndMessage;
			log << tcu::TestLog::Message << "Passed Samples (without early fragment test) : " << de::toString(sampleCounts[0]) << tcu::TestLog::EndMessage;
			log << tcu::TestLog::Message << "Passed Samples (with early fragment test)    : " << de::toString(sampleCounts[1]) << tcu::TestLog::EndMessage;
		}

#ifndef CTS_USES_VULKANSC
		vk.destroyQueryPool(device, queryPool, nullptr);
#endif // CTS_USES_VULKANSC

		// Check that number of the all passed samples are within an acceptable range.
		if (sampleCounts[QUERY_INDEX_NO_EARLY_FRAG] >= minValue && sampleCounts[QUERY_INDEX_NO_EARLY_FRAG] <= maxValue && sampleCounts[QUERY_INDEX_EARLY_FRAG] >= minValue && sampleCounts[QUERY_INDEX_EARLY_FRAG] <= maxValue)
		{
			return tcu::TestStatus::pass("Success");
		}
		else if (sampleCounts[QUERY_INDEX_NO_EARLY_FRAG] >= minValue && sampleCounts[QUERY_INDEX_NO_EARLY_FRAG] <= maxValue && sampleCounts[QUERY_INDEX_EARLY_FRAG] == 0)
		{
#ifndef CTS_USES_VULKANSC
			if (m_testParams.useMaintenance5)
			{
				if (m_context.getMaintenance5Properties().earlyFragmentMultisampleCoverageAfterSampleCounting)
					return tcu::TestStatus::fail("Fail");
				return tcu::TestStatus::pass("Pass");
			}
#endif
			// Spec says: "If the fragment shader declares the EarlyFragmentTests execution mode, fragment shading and
			// multisample coverage operations should instead be performed after sample counting.

			// since the specification says 'should', the opposite behavior is allowed, but not preferred
			return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Sample count is 0 - sample counting performed after multisample coverage and fragment shading");
		}
		else if (sampleCounts[QUERY_INDEX_NO_EARLY_FRAG] >= minValue && sampleCounts[QUERY_INDEX_NO_EARLY_FRAG] <= maxValue &&
					sampleCounts[QUERY_INDEX_EARLY_FRAG] >= (minValue * 2) && sampleCounts[QUERY_INDEX_EARLY_FRAG] <= (maxValue * 2))
		{
			// If the sample count returned is double the expected value, the sample mask test has been executed after
			// sample counting.

#ifndef CTS_USES_VULKANSC
			if (m_testParams.useMaintenance5)
			{
				if (m_context.getMaintenance5Properties().earlyFragmentSampleMaskTestBeforeSampleCounting)
					return tcu::TestStatus::fail("Fail");
				return tcu::TestStatus::pass("Pass");
			}
#endif
			// Spec says: "If there is a fragment shader and it declares the EarlyFragmentTests execution mode, ...
			// sample mask test may: instead be performed after sample counting"

			// since the specification says 'may', the opposite behavior is allowed, but not preferred
			return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Sample count is greater than expected - sample mask test performed after sample counting");
		}
		else
		{
			// Log no early frag test images
			{
				tcu::TestLog& log = m_context.getTestContext().getLog();

				invalidateAlloc(vk, device, *colorBufferAllocNoEarly);

				const tcu::ConstPixelBufferAccess	imagePixelAccess	(mapVkFormat(colorFormat), renderSize.x(), renderSize.y(), 1, colorBufferAllocNoEarly->getHostPtr());

				log << tcu::LogImageSet("No Early Frag Test Images", "No Early Fragment Test Images")
					<< tcu::TestLog::Image("color", "Rendered image (color)", imagePixelAccess)
					<< tcu::TestLog::EndImageSet;
			}

			// Log early frag test images
			{
				tcu::TestLog& log = m_context.getTestContext().getLog();

				invalidateAlloc(vk, device, *colorBufferAllocEarly);

				const tcu::ConstPixelBufferAccess	imagePixelAccess	(mapVkFormat(colorFormat), renderSize.x(), renderSize.y(), 1, colorBufferAllocEarly->getHostPtr());

				log << tcu::LogImageSet("Early Frag Test Images", "Early Fragment Test Images")
					<< tcu::TestLog::Image("color", "Rendered image (color)", imagePixelAccess)
					<< tcu::TestLog::EndImageSet;
			}

			return tcu::TestStatus::fail("Sample count value outside expected range.");
		}
	}
}

class EarlyFragmentSampleCountTest : public EarlyFragmentTest
{
public:
	EarlyFragmentSampleCountTest		(tcu::TestContext&				testCtx,
										 const std::string				name,
										 const SampleCountTestParams&	testParams);

	void				initPrograms	(SourceCollections& programCollection) const override;
	TestInstance*		createInstance	(Context& context) const override;
	void				checkSupport	(Context& context) const override;

private:
	const SampleCountTestParams m_testParams;
};

EarlyFragmentSampleCountTest::EarlyFragmentSampleCountTest(tcu::TestContext& testCtx, const std::string name, const SampleCountTestParams& testParams)
	: EarlyFragmentTest	(testCtx, name, FLAG_TEST_DEPTH)
	, m_testParams		(testParams)
{
}

TestInstance* EarlyFragmentSampleCountTest::createInstance (Context& context) const
{
	return new EarlyFragmentSampleCountTestInstance(context, m_testParams);
}

void EarlyFragmentSampleCountTest::initPrograms(SourceCollections& programCollection) const
{
	// Vertex shader
	{
		std::ostringstream vrt;

		vrt << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) in highp vec4 position;\n"
			<< "\n"
			<< "out gl_PerVertex {\n"
			<< "   vec4 gl_Position;\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_Position = position;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(vrt.str());
	}

	// Fragment shader for runs without early fragment test
	if (m_testParams.earlyAndLate == false)
	{
		std::ostringstream frg;

		frg << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(location = 0) out highp vec4 fragColor;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "    // This will kill half of the samples.\n"
			<< "    gl_SampleMask[0] = 0xAAAAAAAA;\n"
			<< "    fragColor = vec4(1.0, 1.0, 0.0, 1.0);\n"
			<< "}\n";

		programCollection.glslSources.add("frag") << glu::FragmentSource(frg.str());
	}
	else
	{
		const SpirVAsmBuildOptions	buildOptionsSpr(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3);
		const std::string			src =
			"; SPIR - V\n"
			"; Version: 1.0\n"
			"; Generator: Khronos Glslang Reference Front End; 10\n"
			"; Bound: 23\n"
			"; Schema: 0\n"
			"OpCapability Shader\n"
			"OpExtension \"SPV_AMD_shader_early_and_late_fragment_tests\"\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint Fragment %4 \"main\" %11 %19\n"
			"OpExecutionMode %4 OriginUpperLeft\n"
			"OpExecutionMode %4 EarlyAndLateFragmentTestsAMD\n"
			"OpExecutionMode %4 DepthReplacing\n"
			"OpExecutionMode %4 DepthLess\n"
			"OpDecorate %11 BuiltIn SampleMask\n"
			"OpDecorate %19 Location 0\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeInt 32 1\n"
			"%7 = OpTypeInt 32 0\n"
			"%8 = OpConstant %7 1\n"
			"%9 = OpTypeArray %6 %8\n"
			"%10 = OpTypePointer Output %9\n"
			"%11 = OpVariable %10 Output\n"
			"%12 = OpConstant %6 0\n"
			"%13 = OpConstant %6 -1431655766\n"
			"%14 = OpTypePointer Output %6\n"
			"%16 = OpTypeFloat 32\n"
			"%17 = OpTypeVector %16 4\n"
			"%18 = OpTypePointer Output %17\n"
			"%19 = OpVariable %18 Output\n"
			"%20 = OpConstant %16 1\n"
			"%21 = OpConstant %16 0\n"
			"%22 = OpConstantComposite %17 %20 %20 %21 %20\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%15 = OpAccessChain %14 %11 %12\n"
			"OpStore %15 %13\n"
			"OpStore %19 %22\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("frag") << src << buildOptionsSpr;
	}

	// Fragment shader for early fragment tests
	if (m_testParams.earlyAndLate == false)
	{
		std::ostringstream frg;

		frg << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
			<< "\n"
			<< "layout(early_fragment_tests) in;\n"
			<< "\n"
			<< "layout(location = 0) out highp vec4 fragColor;\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n";

		if (m_testParams.alphaToCoverage)
		{
			frg << "    // alphaToCoverageEnable = TRUE and emitting 0 alpha kills all the samples, but the sample counting has already happened.\n"
				<< "    fragColor = vec4(1.0, 1.0, 0.0, 0.0);\n"
				<< "}\n";
		}
		else
		{
			frg << "    // Sample mask kills all the samples, but the sample counting has already happened.\n"
				<< "    gl_SampleMask[0] = 0x0;\n"
				<< "    fragColor = vec4(1.0, 1.0, 0.0, 1.0);\n"
				<< "}\n";
		}

		programCollection.glslSources.add("frag_early") << glu::FragmentSource(frg.str());
	}
	else
	{
		const SpirVAsmBuildOptions	buildOptionsSpr(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3);
		const std::string			src	=
			"; SPIR - V\n"
			"; Version: 1.0\n"
			"; Generator: Khronos Glslang Reference Front End; 10\n"
			"; Bound: 22\n"
			"; Schema: 0\n"
			"OpCapability Shader\n"
			"OpExtension \"SPV_AMD_shader_early_and_late_fragment_tests\"\n"
			"%1 = OpExtInstImport \"GLSL.std.450\"\n"
			"OpMemoryModel Logical GLSL450\n"
			"OpEntryPoint Fragment %4 \"main\" %11 %18\n"
			"OpExecutionMode %4 OriginUpperLeft\n"
			"OpExecutionMode %4 EarlyAndLateFragmentTestsAMD\n"
			"OpDecorate %11 BuiltIn SampleMask\n"
			"OpDecorate %18 Location 0\n"
			"%2 = OpTypeVoid\n"
			"%3 = OpTypeFunction %2\n"
			"%6 = OpTypeInt 32 1\n"
			"%7 = OpTypeInt 32 0\n"
			"%8 = OpConstant %7 1\n"
			"%9 = OpTypeArray %6 %8\n"
			"%10 = OpTypePointer Output %9\n"
			"%11 = OpVariable %10 Output\n"
			"%12 = OpConstant %6 0\n"
			"%13 = OpTypePointer Output %6\n"
			"%15 = OpTypeFloat 32\n"
			"%16 = OpTypeVector %15 4\n"
			"%17 = OpTypePointer Output %16\n"
			"%18 = OpVariable %17 Output\n"
			"%19 = OpConstant %15 1\n"
			"%20 = OpConstant %15 0\n"
			"%21 = OpConstantComposite %16 %19 %19 %20 %19\n"
			"%4 = OpFunction %2 None %3\n"
			"%5 = OpLabel\n"
			"%14 = OpAccessChain %13 %11 %12\n"
			"OpStore %14 %12\n"
			"OpStore %18 %21\n"
			"OpReturn\n"
			"OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("frag_early") << src << buildOptionsSpr;
	}
}

void EarlyFragmentSampleCountTest::checkSupport(Context& context) const
{
	EarlyFragmentTest::checkSupport(context);

	// Check support for MSAA image formats used in the test.
	const InstanceInterface&	vki				= context.getInstanceInterface();
	const VkPhysicalDevice		physDevice		= context.getPhysicalDevice();
	const VkFormat				colorFormat		= VK_FORMAT_R8G8B8A8_UNORM;
	const VkFormat				depthFormat		= VK_FORMAT_D16_UNORM;

	VkImageFormatProperties		formatProperties;

	vki.getPhysicalDeviceImageFormatProperties(physDevice, colorFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0u, &formatProperties);
	if ((formatProperties.sampleCounts & m_testParams.sampleCount) == 0)
		TCU_THROW(NotSupportedError, "Format does not support this number of samples for color format");

	vki.getPhysicalDeviceImageFormatProperties(physDevice, depthFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0u, &formatProperties);
	if ((formatProperties.sampleCounts & m_testParams.sampleCount) == 0)
		TCU_THROW(NotSupportedError, "Format does not support this number of samples for depth format");

#ifndef CTS_USES_VULKANSC
	if (m_testParams.earlyAndLate)
	{
		context.requireDeviceFunctionality("VK_AMD_shader_early_and_late_fragment_tests");
		if (context.getShaderEarlyAndLateFragmentTestsFeaturesAMD().shaderEarlyAndLateFragmentTests == VK_FALSE)
			TCU_THROW(NotSupportedError, "shaderEarlyAndLateFragmentTests is not supported");
	}

	if (m_testParams.useMaintenance5)
		context.requireDeviceFunctionality("VK_KHR_maintenance5");
#endif
}

} // anonymous ns

tcu::TestCaseGroup* createEarlyFragmentTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup			(new tcu::TestCaseGroup(testCtx, "early_fragment", "early fragment test cases"));

	{
		struct TestCaseEarly
		{
			std::string caseName;
			deUint32	flags;
		};

		static const TestCaseEarly cases[] =
		{

			{ "no_early_fragment_tests_depth",					FLAG_TEST_DEPTH   | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS									},
			{ "no_early_fragment_tests_stencil",				FLAG_TEST_STENCIL | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS									},
			{ "early_fragment_tests_depth",						FLAG_TEST_DEPTH																			},
			{ "early_fragment_tests_stencil",					FLAG_TEST_STENCIL																		},
			{ "no_early_fragment_tests_depth_no_attachment",	FLAG_TEST_DEPTH   | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS | FLAG_DONT_USE_TEST_ATTACHMENT	},
			{ "no_early_fragment_tests_stencil_no_attachment",	FLAG_TEST_STENCIL | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS | FLAG_DONT_USE_TEST_ATTACHMENT	},
			{ "early_fragment_tests_depth_no_attachment",		FLAG_TEST_DEPTH   | FLAG_DONT_USE_TEST_ATTACHMENT										},
			{ "early_fragment_tests_stencil_no_attachment",		FLAG_TEST_STENCIL | FLAG_DONT_USE_TEST_ATTACHMENT										},
		};

#ifndef CTS_USES_VULKANSC
		static const TestCaseEarly casesEarlyAndLate[] =
		{
			{ "early_and_late_fragment_tests_depth",				FLAG_TEST_DEPTH   | FLAG_EARLY_AND_LATE_FRAGMENT_TESTS									},
			{ "early_and_late_fragment_tests_stencil",				FLAG_TEST_STENCIL | FLAG_EARLY_AND_LATE_FRAGMENT_TESTS									},
			{ "early_and_late_fragment_tests_depth_no_attachment",	FLAG_TEST_DEPTH   | FLAG_DONT_USE_TEST_ATTACHMENT | FLAG_EARLY_AND_LATE_FRAGMENT_TESTS	},
			{ "early_and_late_fragment_tests_stencil_no_attachment",FLAG_TEST_STENCIL | FLAG_DONT_USE_TEST_ATTACHMENT | FLAG_EARLY_AND_LATE_FRAGMENT_TESTS	},
		};
#endif

		for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
			testGroup->addChild(new EarlyFragmentTest(testCtx, cases[i].caseName, cases[i].flags));
#ifndef CTS_USES_VULKANSC
		for (int i = 0; i < DE_LENGTH_OF_ARRAY(casesEarlyAndLate); ++i)
			testGroup->addChild(new EarlyFragmentTest(testCtx, casesEarlyAndLate[i].caseName, casesEarlyAndLate[i].flags));
#endif
	}

	// Check that discard does not affect depth test writes.
	{
		static const struct
		{
			std::string caseName;
			deUint32	flags;
		} cases[] =
		{
			{ "discard_no_early_fragment_tests_depth",			FLAG_TEST_DEPTH   | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS										},
			{ "discard_no_early_fragment_tests_stencil",		FLAG_TEST_STENCIL | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS										},
			{ "discard_early_fragment_tests_depth",				FLAG_TEST_DEPTH																				},
			{ "discard_early_fragment_tests_stencil",			FLAG_TEST_STENCIL																			},
#ifndef CTS_USES_VULKANSC
			{ "discard_early_and_late_fragment_tests_depth",	FLAG_TEST_DEPTH   | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS | FLAG_EARLY_AND_LATE_FRAGMENT_TESTS	},
			{ "discard_early_and_late_fragment_tests_stencil",	FLAG_TEST_STENCIL | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS | FLAG_EARLY_AND_LATE_FRAGMENT_TESTS	},
#endif
		};

		for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
			testGroup->addChild(new EarlyFragmentDiscardTest(testCtx, cases[i].caseName, cases[i].flags));
	}

	// Check that writing to gl_SampleMask does not affect depth test writes.
	{
		static const struct
		{
			std::string caseName;
			deUint32	flags;
		} cases[] =
		{
			{ "samplemask_no_early_fragment_tests_depth",						FLAG_TEST_DEPTH | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS										},
			{ "samplemask_early_fragment_tests_depth",							FLAG_TEST_DEPTH																				},
#ifndef CTS_USES_VULKANSC
			{ "samplemask_early_and_late_fragment_tests_depth_replacing_mode",	FLAG_TEST_DEPTH | FLAG_EARLY_AND_LATE_FRAGMENT_TESTS | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS	},
			{ "samplemask_early_and_late_fragment_tests_depth",					FLAG_TEST_DEPTH | FLAG_EARLY_AND_LATE_FRAGMENT_TESTS										},
#endif
		};

		const VkSampleCountFlags sampleCounts[] = { VK_SAMPLE_COUNT_2_BIT, VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_8_BIT, VK_SAMPLE_COUNT_16_BIT };
		const std::string sampleCountsStr[] = { "samples_2", "samples_4", "samples_8", "samples_16" };

		for (deUint32 sampleCountsNdx = 0; sampleCountsNdx < DE_LENGTH_OF_ARRAY(sampleCounts); sampleCountsNdx++)
		{
			for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
				testGroup->addChild(new EarlyFragmentSampleMaskTest(testCtx, cases[i].caseName + "_" + sampleCountsStr[sampleCountsNdx], cases[i].flags, sampleCounts[sampleCountsNdx]));
		}
	}

	// We kill half of the samples at different points in the pipeline depending on early frag test, and then we verify the sample counting works as expected.
	{
		const VkSampleCountFlags	sampleCounts[]		= { VK_SAMPLE_COUNT_2_BIT, VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_8_BIT, VK_SAMPLE_COUNT_16_BIT };
		const std::string			sampleCountsStr[]	= { "samples_2", "samples_4", "samples_8", "samples_16" };

		for (deUint32 sampleCountsNdx = 0; sampleCountsNdx < DE_LENGTH_OF_ARRAY(sampleCounts); sampleCountsNdx++)
		{
			SampleCountTestParams params
			{
				sampleCounts[sampleCountsNdx],	// deUint32	sampleCount;
				false,							// bool		earlyAndLate;
				false,							// bool		alphaToCoverage;
				false,							// bool		useMaintenance5;
			};

			testGroup->addChild(new EarlyFragmentSampleCountTest(testCtx, "sample_count_early_fragment_tests_depth_" + sampleCountsStr[sampleCountsNdx], params));
#ifndef CTS_USES_VULKANSC
			params.earlyAndLate = true;
			testGroup->addChild(new EarlyFragmentSampleCountTest(testCtx, "sample_count_early_and_late_fragment_tests_depth_" + sampleCountsStr[sampleCountsNdx], params));

			params.useMaintenance5 = true;
			testGroup->addChild(new EarlyFragmentSampleCountTest(testCtx, "sample_count_early_and_late_fragment_tests_depth_" + sampleCountsStr[sampleCountsNdx] + "_maintenance5", params));
			params.earlyAndLate = false;
			testGroup->addChild(new EarlyFragmentSampleCountTest(testCtx, "sample_count_early_fragment_tests_depth_" + sampleCountsStr[sampleCountsNdx] + "_maintenance5", params));
			params.alphaToCoverage = true;
			testGroup->addChild(new EarlyFragmentSampleCountTest(testCtx, "sample_count_early_fragment_tests_depth_alpha_to_coverage_" + sampleCountsStr[sampleCountsNdx] + "_maintenance5", params));
#endif
		}
	}

	return testGroup.release();
}

} // FragmentOperations
} // vkt
