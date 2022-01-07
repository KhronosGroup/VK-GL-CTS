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
#include "vktFragmentOperationsMakeUtil.hpp"
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
	FLAG_TEST_DEPTH							= 1u << 0,
	FLAG_TEST_STENCIL						= 1u << 1,
	FLAG_DONT_USE_TEST_ATTACHMENT			= 1u << 2,
	FLAG_DONT_USE_EARLY_FRAGMENT_TESTS		= 1u << 3,
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
};

EarlyFragmentTestInstance::EarlyFragmentTestInstance (Context& context, const deUint32 flags)
	: TestInstance			(context)
	, m_testMode			(flags & FLAG_TEST_DEPTH   ? MODE_DEPTH :
							 flags & FLAG_TEST_STENCIL ? MODE_STENCIL : MODE_INVALID)
	, m_useTestAttachment	((flags & FLAG_DONT_USE_TEST_ATTACHMENT) == 0)
	, m_useEarlyTests		((flags & FLAG_DONT_USE_EARLY_FRAGMENT_TESTS) == 0)
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

		const tcu::ConstPixelBufferAccess imagePixelAccess(mapVkFormat(colorFormat), renderSize.x(), renderSize.y(), 1, colorBufferAlloc->getHostPtr());

		tcu::TestLog& log = m_context.getTestContext().getLog();
		log << tcu::TestLog::Image("color0", "Rendered image", imagePixelAccess);
	}

	// Verify results
	{
		invalidateAlloc(vk, device, *resultBufferAlloc);

		const int  actualCounter	   = *static_cast<deInt32*>(resultBufferAlloc->getHostPtr());
		const bool expectPartialResult = (m_useEarlyTests && m_useTestAttachment);
		const int  expectedCounter	   = expectPartialResult ? renderSize.x() * renderSize.y() / 2 : renderSize.x() * renderSize.y();
		const int  tolerance		   = expectPartialResult ? de::max(renderSize.x(), renderSize.y()) * 3	: 0;
		const int  expectedMin         = de::max(0, expectedCounter - tolerance);
		const int  expectedMax		   = expectedCounter + tolerance;

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
};

EarlyFragmentDiscardTestInstance::EarlyFragmentDiscardTestInstance (Context& context, const deUint32 flags)
	: EarlyFragmentTestInstance			(context, flags)
	, m_testMode			(flags & FLAG_TEST_DEPTH   ? MODE_DEPTH :
							 flags & FLAG_TEST_STENCIL ? MODE_STENCIL : MODE_INVALID)
	, m_useTestAttachment	((flags & FLAG_DONT_USE_TEST_ATTACHMENT) == 0)
	, m_useEarlyTests		((flags & FLAG_DONT_USE_EARLY_FRAGMENT_TESTS) == 0)
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
		const int  expectedMax		   = expectedCounter + tolerance;

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
		0u,													// deUint32         attachment
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout    layout
		VK_IMAGE_ASPECT_COLOR_BIT							// VkImageAspectFlags	aspectMask;
	};

	const VkAttachmentReference2				depthStencilAttachmentRef			=
	{
		VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,												// VkStructureType		sType;
		DE_NULL,																				// const void*			pNext;
	    hasDepthStencil ? 1u : 0u,																// deUint32         attachment
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,										// VkImageLayout    layout
	    m_testMode == MODE_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_STENCIL_BIT		// VkImageAspectFlags	aspectMask;
	};

	const VkAttachmentReference2				resolveAttachmentRef					=
	{
		VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,			// VkStructureType		sType;
		DE_NULL,											// const void*			pNext;
		hasColor ? 2u : 0u,									// deUint32         attachment
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,			// VkImageLayout    layout
		VK_IMAGE_ASPECT_COLOR_BIT							// VkImageAspectFlags	aspectMask;
	};

	const VkAttachmentReference2				depthStencilResolveAttachmentRef			=
	{
		VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,												// VkStructureType		sType;
		DE_NULL,																				// const void*			pNext;
	    hasDepthStencil ? 3u : 0u,																// deUint32         attachment
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,										// VkImageLayout    layout
	    m_testMode == MODE_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_STENCIL_BIT		// VkImageAspectFlags	aspectMask;
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
}

void EarlyFragmentSampleMaskTest::checkSupport(Context& context) const
{
	EarlyFragmentTest::checkSupport(context);

	context.requireDeviceFunctionality("VK_KHR_depth_stencil_resolve");
}

} // anonymous ns

tcu::TestCaseGroup* createEarlyFragmentTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup(new tcu::TestCaseGroup(testCtx, "early_fragment", "early fragment test cases"));

	{
		static const struct
		{
			std::string caseName;
			deUint32	flags;
		} cases[] =
		{
			{ "no_early_fragment_tests_depth",					FLAG_TEST_DEPTH   | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS									},
			{ "no_early_fragment_tests_stencil",				FLAG_TEST_STENCIL | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS									},
			{ "early_fragment_tests_depth",						FLAG_TEST_DEPTH																			},
			{ "early_fragment_tests_stencil",					FLAG_TEST_STENCIL																		},
			{ "no_early_fragment_tests_depth_no_attachment",	FLAG_TEST_DEPTH   | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS | FLAG_DONT_USE_TEST_ATTACHMENT	},
			{ "no_early_fragment_tests_stencil_no_attachment",	FLAG_TEST_STENCIL | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS | FLAG_DONT_USE_TEST_ATTACHMENT	},
			{ "early_fragment_tests_depth_no_attachment",		FLAG_TEST_DEPTH   |										 FLAG_DONT_USE_TEST_ATTACHMENT	},
			{ "early_fragment_tests_stencil_no_attachment",		FLAG_TEST_STENCIL |										 FLAG_DONT_USE_TEST_ATTACHMENT	},
		};

		for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
			testGroup->addChild(new EarlyFragmentTest(testCtx, cases[i].caseName, cases[i].flags));
	}

	// Check that discard does not affect depth test writes.
	{
		static const struct
		{
			std::string caseName;
			deUint32	flags;
		} cases[] =
		{
			{ "discard_no_early_fragment_tests_depth",					FLAG_TEST_DEPTH   | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS									},
			{ "discard_no_early_fragment_tests_stencil",				FLAG_TEST_STENCIL | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS									},
			{ "discard_early_fragment_tests_depth",						FLAG_TEST_DEPTH																			},
			{ "discard_early_fragment_tests_stencil",					FLAG_TEST_STENCIL																		},
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
			{ "samplemask_no_early_fragment_tests_depth",				FLAG_TEST_DEPTH   | FLAG_DONT_USE_EARLY_FRAGMENT_TESTS,									},
			{ "samplemask_early_fragment_tests_depth",					FLAG_TEST_DEPTH,																		},
		};

		const VkSampleCountFlags sampleCounts[] = { VK_SAMPLE_COUNT_2_BIT, VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_8_BIT, VK_SAMPLE_COUNT_16_BIT };
		const std::string sampleCountsStr[] = { "samples_2", "samples_4", "samples_8", "samples_16" };

		for (deUint32 sampleCountsNdx = 0; sampleCountsNdx < DE_LENGTH_OF_ARRAY(sampleCounts); sampleCountsNdx++)
		{
			for (int i = 0; i < DE_LENGTH_OF_ARRAY(cases); ++i)
				testGroup->addChild(new EarlyFragmentSampleMaskTest(testCtx, cases[i].caseName + "_" + sampleCountsStr[sampleCountsNdx], cases[i].flags, sampleCounts[sampleCountsNdx]));
        }
	}

	return testGroup.release();
}

} // FragmentOperations
} // vkt
