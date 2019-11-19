/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
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
 * \brief Vulkan Performance Query Tests
 *//*--------------------------------------------------------------------*/

#include "vktQueryPoolPerformanceTests.hpp"
#include "vktTestCase.hpp"

#include "vktDrawImageObjectUtil.hpp"
#include "vktDrawBufferObjectUtil.hpp"
#include "vktDrawCreateInfoUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkQueryUtil.hpp"

#include "deMath.h"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"
#include "tcuImageCompare.hpp"
#include "vkImageUtil.hpp"
#include "tcuCommandLine.hpp"
#include "tcuRGBA.hpp"

namespace vkt
{
namespace QueryPool
{
namespace
{

using namespace vk;
using namespace Draw;

std::string uuidToHex(const deUint8 uuid[])
{
	const size_t	bytesPerPart[]	= {4, 2, 2, 2, 6};
	const deUint8*	ptr				= &uuid[0];
	const size_t	stringSize		= VK_UUID_SIZE * 2 + DE_LENGTH_OF_ARRAY(bytesPerPart) - 1;
	std::string		result;

	result.reserve(stringSize);

	for (size_t partNdx = 0; partNdx < DE_LENGTH_OF_ARRAY(bytesPerPart); ++partNdx)
	{
		const size_t	bytesInPart		= bytesPerPart[partNdx];
		const size_t	symbolsInPart	= 2 * bytesInPart;
		deUint64		part			= 0;
		std::string		partString;

		for (size_t byteInPartNdx = 0; byteInPartNdx < bytesInPart; ++byteInPartNdx)
		{
			part = (part << 8) | *ptr;
			++ptr;
		}

		partString	= tcu::toHex(part).toString();

		DE_ASSERT(partString.size() > symbolsInPart);

		result += (symbolsInPart >= partString.size()) ? partString : partString.substr(partString.size() - symbolsInPart);

		if (partNdx + 1 != DE_LENGTH_OF_ARRAY(bytesPerPart))
			result += '-';
	}

	DE_ASSERT(ptr == &uuid[VK_UUID_SIZE]);
	DE_ASSERT(result.size() == stringSize);

	return result;
}

class EnumerateAndValidateTest : public TestInstance
{
public:
						EnumerateAndValidateTest		(vkt::Context&	context, VkQueueFlagBits queueFlagBits);
	tcu::TestStatus		iterate							(void);

protected:
	void				basicValidateCounter			(const deUint32 familyIndex);

private:
	VkQueueFlagBits		m_queueFlagBits;
	bool				m_requiredExtensionsPresent;
};

EnumerateAndValidateTest::EnumerateAndValidateTest (vkt::Context& context, VkQueueFlagBits queueFlagBits)
	: TestInstance(context)
	, m_queueFlagBits(queueFlagBits)
	, m_requiredExtensionsPresent(context.requireDeviceFunctionality("VK_KHR_performance_query"))
{
}

tcu::TestStatus EnumerateAndValidateTest::iterate (void)
{
	const InstanceInterface&					vki				= m_context.getInstanceInterface();
	const VkPhysicalDevice						physicalDevice	= m_context.getPhysicalDevice();
	const std::vector<VkQueueFamilyProperties>	queueProperties	= getPhysicalDeviceQueueFamilyProperties(vki, physicalDevice);

	for (deUint32 queueNdx = 0; queueNdx < queueProperties.size(); queueNdx++)
	{
		if ((queueProperties[queueNdx].queueFlags & m_queueFlagBits) == 0)
			continue;

		deUint32 counterCount = 0;
		VK_CHECK(vki.enumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(physicalDevice, queueNdx, &counterCount, DE_NULL, DE_NULL));

		if (counterCount == 0)
			continue;

		{
			std::vector<VkPerformanceCounterKHR>	counters			(counterCount);
			deUint32								counterCountRead	= counterCount;
			std::map<std::string, size_t>			uuidValidator;

			if (counterCount > 1)
			{
				deUint32	incompleteCounterCount	= counterCount - 1;
				VkResult	result;

				result = vki.enumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(physicalDevice, queueNdx, &incompleteCounterCount, &counters[0], DE_NULL);
				if (result != VK_INCOMPLETE)
					TCU_FAIL("VK_INCOMPLETE not returned");
			}

			VK_CHECK(vki.enumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(physicalDevice, queueNdx, &counterCountRead, &counters[0], DE_NULL));

			if (counterCountRead != counterCount)
				TCU_FAIL("Number of counters read (" + de::toString(counterCountRead) + ") is not equal to number of counters reported (" + de::toString(counterCount) + ")");

			for (size_t counterNdx = 0; counterNdx < counters.size(); ++counterNdx)
			{
				const VkPerformanceCounterKHR&	counter			= counters[counterNdx];
				const std::string				uuidStr			= uuidToHex(counter.uuid);

				if (uuidValidator.find(uuidStr) != uuidValidator.end())
					TCU_FAIL("Duplicate counter UUID detected " + uuidStr);
				else
					uuidValidator[uuidStr] = counterNdx;

				if (counter.scope >= VK_PERFORMANCE_COUNTER_SCOPE_KHR_LAST)
					TCU_FAIL("Counter scope is invalid " + de::toString(static_cast<size_t>(counter.scope)));

				if (counter.storage >= VK_PERFORMANCE_COUNTER_STORAGE_KHR_LAST)
					TCU_FAIL("Counter storage is invalid " + de::toString(static_cast<size_t>(counter.storage)));

				if (counter.unit >= VK_PERFORMANCE_COUNTER_UNIT_KHR_LAST)
					TCU_FAIL("Counter unit is invalid " + de::toString(static_cast<size_t>(counter.unit)));
			}
		}
		{
			std::vector<VkPerformanceCounterDescriptionKHR>	counterDescriptors	(counterCount);
			deUint32										counterCountRead	= counterCount;

			VK_CHECK(vki.enumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(physicalDevice, queueNdx, &counterCountRead, DE_NULL, &counterDescriptors[0]));

			if (counterCountRead != counterCount)
				TCU_FAIL("Number of counters read (" + de::toString(counterCountRead) + ") is not equal to number of counters reported (" + de::toString(counterCount) + ")");

			for (size_t counterNdx = 0; counterNdx < counterDescriptors.size(); ++counterNdx)
			{
				const VkPerformanceCounterDescriptionKHR&		counterDescriptor	= counterDescriptors[counterNdx];
				const VkPerformanceCounterDescriptionFlagsKHR	allowedFlags		= VK_PERFORMANCE_COUNTER_DESCRIPTION_PERFORMANCE_IMPACTING_KHR
																					| VK_PERFORMANCE_COUNTER_DESCRIPTION_CONCURRENTLY_IMPACTED_KHR;

				if ((counterDescriptor.flags & ~allowedFlags) != 0)
					TCU_FAIL("Invalid flags present in VkPerformanceCounterDescriptionFlagsKHR");
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

class QueryTestBase : public TestInstance
{
public:
						QueryTestBase	(vkt::Context&	context);

protected:

	void				setupCounters			(void);
	Move<VkQueryPool>	createQueryPool			(deUint32 enabledCounterOffset, deUint32 enabledCounterStride);
	bool				acquireProfilingLock	(void);
	void				releaseProfilingLock	(void);
	bool				verifyQueryResults		(VkQueryPool queryPool);
	deUint32			getRequiredNumerOfPasses(void);

private:

	bool									m_requiredExtensionsPresent;
	deUint32								m_requiredNumerOfPasses;
	std::map<deUint64, deUint32>			m_enabledCountersCountMap;		// number of counters that were enabled per query pool
	std::vector<VkPerformanceCounterKHR>	m_counters;						// counters provided by the device
};

QueryTestBase::QueryTestBase(vkt::Context& context)
	: TestInstance	(context)
	, m_requiredExtensionsPresent(context.requireDeviceFunctionality("VK_KHR_performance_query"))
	, m_requiredNumerOfPasses(0)
{
}

void QueryTestBase::setupCounters()
{
	const InstanceInterface&	vki					= m_context.getInstanceInterface();
	const VkPhysicalDevice		physicalDevice		= m_context.getPhysicalDevice();
	const CmdPoolCreateInfo		cmdPoolCreateInfo	= m_context.getUniversalQueueFamilyIndex();
	deUint32					queueFamilyIndex	= cmdPoolCreateInfo.queueFamilyIndex;
	deUint32					counterCount;

	if (!m_context.getPerformanceQueryFeatures().performanceCounterQueryPools)
		TCU_THROW(NotSupportedError, "Performance counter query pools feature not supported");

	// get the number of supported counters
	VK_CHECK(vki.enumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(physicalDevice, queueFamilyIndex, &counterCount, NULL, NULL));

	if (!counterCount)
		TCU_THROW(NotSupportedError, "QualityWarning: there are no performance counters");

	// get supported counters
	m_counters.resize(counterCount);
	VK_CHECK(vki.enumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR(physicalDevice, queueFamilyIndex, &counterCount, &m_counters[0], DE_NULL));
}

Move<VkQueryPool> QueryTestBase::createQueryPool(deUint32 enabledCounterOffset, deUint32 enabledCounterStride)
{
	const InstanceInterface&	vki					= m_context.getInstanceInterface();
	const DeviceInterface&		vkd					= m_context.getDeviceInterface();
	const VkPhysicalDevice		physicalDevice		= m_context.getPhysicalDevice();
	const VkDevice				device				= m_context.getDevice();
	const CmdPoolCreateInfo		cmdPoolCreateInfo	= m_context.getUniversalQueueFamilyIndex();
	const deUint32				counterCount		= (deUint32)m_counters.size();
	deUint32					enabledIndex		= enabledCounterOffset ? 0 : enabledCounterStride;
	std::vector<deUint32>		enabledCounters;

	// enable every <enabledCounterStride> counter that has command or render pass scope
	for (deUint32 i = 0; i < counterCount; i++)
	{
		// handle offset
		if (enabledCounterOffset)
		{
			if (enabledCounterOffset == enabledIndex)
			{
				// disable handling offset
				enabledCounterOffset = 0;

				// eneble next index in stride condition
				enabledIndex = enabledCounterStride;
			}
			else
			{
				++enabledIndex;
				continue;
			}
		}

		// handle stride
		if (enabledIndex == enabledCounterStride)
		{
			enabledCounters.push_back(i);
			enabledIndex = 0;
		}
		else
			++enabledIndex;
	}

	// get number of counters that were enabled for this query pool
	deUint32 enabledCountersCount = static_cast<deUint32>(enabledCounters.size());
	if (!enabledCountersCount)
		TCU_THROW(NotSupportedError, "QualityWarning: no performance counters");

	// define performance query
	VkQueryPoolPerformanceCreateInfoKHR performanceQueryCreateInfo =
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_PERFORMANCE_CREATE_INFO_KHR,
		NULL,
		cmdPoolCreateInfo.queueFamilyIndex,			// queue family that this performance query is performed on
		enabledCountersCount,						// number of counters to enable
		&enabledCounters[0]							// array of indices of counters to enable
	};

	// get the number of passes counters will require
	vki.getPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR(physicalDevice, &performanceQueryCreateInfo, &m_requiredNumerOfPasses);

	// create query pool
	VkQueryPoolCreateInfo queryPoolCreateInfo =
	{
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
		&performanceQueryCreateInfo,
		0,											// flags
		VK_QUERY_TYPE_PERFORMANCE_QUERY_KHR,		// new query type
		1,											// queryCount
		0
	};

	Move<VkQueryPool> queryPool = vk::createQueryPool(vkd, device, &queryPoolCreateInfo);

	// memorize number of enabled counters for this query pool
	m_enabledCountersCountMap[queryPool.get().getInternal()] = enabledCountersCount;

	return queryPool;
}

bool QueryTestBase::acquireProfilingLock()
{
	const DeviceInterface&		vkd		= m_context.getDeviceInterface();
	const VkDevice				device	= m_context.getDevice();

	// acquire profiling lock before we record command buffers
	VkAcquireProfilingLockInfoKHR lockInfo =
	{
		VK_STRUCTURE_TYPE_ACQUIRE_PROFILING_LOCK_INFO_KHR,
		NULL,
		0,
		2000000000ull					// wait 2s for the lock
	};

	VkResult result = vkd.acquireProfilingLockKHR(device, &lockInfo);
	if (result == VK_TIMEOUT)
	{
		m_context.getTestContext().getLog() << tcu::TestLog::Message
			<< "Timeout reached, profiling lock wasn't acquired - test had to end earlier"
			<< tcu::TestLog::EndMessage;
		return false;
	}
	if (result != VK_SUCCESS)
		TCU_FAIL("Profiling lock wasn't acquired");

	return true;
}

void QueryTestBase::releaseProfilingLock()
{
	const DeviceInterface&	vkd		= m_context.getDeviceInterface();
	const VkDevice			device	= m_context.getDevice();

	// release the profiling lock after the command buffer is no longer in the pending state
	vkd.releaseProfilingLockKHR(device);
}

bool QueryTestBase::verifyQueryResults(VkQueryPool queryPool)
{
	const DeviceInterface&		vkd		= m_context.getDeviceInterface();
	const VkDevice				device	= m_context.getDevice();

	// create an array to hold the results of all counters
	deUint32 enabledCounterCount = m_enabledCountersCountMap[queryPool.getInternal()];
	std::vector<VkPerformanceCounterResultKHR> recordedCounters(enabledCounterCount);

	// verify that query result can be retrieved
	VkResult result = vkd.getQueryPoolResults(device, queryPool, 0, 1, sizeof(VkPerformanceCounterResultKHR) * enabledCounterCount,
		&recordedCounters[0], sizeof(VkPerformanceCounterResultKHR), VK_QUERY_RESULT_WAIT_BIT);
	if (result == VK_NOT_READY)
	{
		m_context.getTestContext().getLog() << tcu::TestLog::Message
			<< "Pass but result is not ready"
			<< tcu::TestLog::EndMessage;
		return true;
	}
	return (result == VK_SUCCESS);
}

deUint32 QueryTestBase::getRequiredNumerOfPasses()
{
	return m_requiredNumerOfPasses;
}

// Base class for all graphic tests
class GraphicQueryTestBase : public QueryTestBase
{
public:
	GraphicQueryTestBase(vkt::Context&	context);

protected:
	void initStateObjects(void);

protected:
	Move<VkPipeline>		m_pipeline;
	Move<VkPipelineLayout>	m_pipelineLayout;

	de::SharedPtr<Image>	m_colorAttachmentImage;
	Move<VkImageView>		m_attachmentView;

	Move<VkRenderPass>		m_renderPass;
	Move<VkFramebuffer>		m_framebuffer;

	de::SharedPtr<Buffer>	m_vertexBuffer;

	VkFormat				m_colorAttachmentFormat;
	deUint32				m_size;
};

GraphicQueryTestBase::GraphicQueryTestBase(vkt::Context& context)
	: QueryTestBase(context)
	, m_colorAttachmentFormat(VK_FORMAT_R8G8B8A8_UNORM)
	, m_size(32)
{
}

void GraphicQueryTestBase::initStateObjects(void)
{
	const VkDevice				device	= m_context.getDevice();
	const DeviceInterface&		vkd		= m_context.getDeviceInterface();

	//attachment images and views
	{
		VkExtent3D imageExtent =
		{
			m_size,		// width
			m_size,		// height
			1			// depth
		};

		const ImageCreateInfo colorImageCreateInfo(VK_IMAGE_TYPE_2D, m_colorAttachmentFormat, imageExtent, 1, 1,
												   VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
												   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

		m_colorAttachmentImage = Image::createAndAlloc(vkd, device, colorImageCreateInfo, m_context.getDefaultAllocator(),
													   m_context.getUniversalQueueFamilyIndex());

		const ImageViewCreateInfo attachmentViewInfo(m_colorAttachmentImage->object(), VK_IMAGE_VIEW_TYPE_2D, m_colorAttachmentFormat);
		m_attachmentView = createImageView(vkd, device, &attachmentViewInfo);
	}

	// renderpass and framebuffer
	{
		RenderPassCreateInfo renderPassCreateInfo;
		renderPassCreateInfo.addAttachment(AttachmentDescription(m_colorAttachmentFormat,				// format
																 VK_SAMPLE_COUNT_1_BIT,					// samples
																 VK_ATTACHMENT_LOAD_OP_CLEAR,			// loadOp
																 VK_ATTACHMENT_STORE_OP_DONT_CARE,		// storeOp
																 VK_ATTACHMENT_LOAD_OP_DONT_CARE,		// stencilLoadOp
																 VK_ATTACHMENT_STORE_OP_DONT_CARE,		// stencilLoadOp
																 VK_IMAGE_LAYOUT_GENERAL,				// initialLauout
																 VK_IMAGE_LAYOUT_GENERAL));				// finalLayout

		const VkAttachmentReference colorAttachmentReference =
		{
			0,																							// attachment
			VK_IMAGE_LAYOUT_GENERAL																		// layout
		};

		renderPassCreateInfo.addSubpass(SubpassDescription(VK_PIPELINE_BIND_POINT_GRAPHICS,				// pipelineBindPoint
														   0,											// flags
														   0,											// inputCount
														   DE_NULL,										// pInputAttachments
														   1,											// colorCount
														   &colorAttachmentReference,					// pColorAttachments
														   DE_NULL,										// pResolveAttachments
														   AttachmentReference(),						// depthStencilAttachment
														   0,											// preserveCount
														   DE_NULL));									// preserveAttachments

		m_renderPass = createRenderPass(vkd, device, &renderPassCreateInfo);

		std::vector<VkImageView> attachments(1);
		attachments[0] = *m_attachmentView;

		FramebufferCreateInfo framebufferCreateInfo(*m_renderPass, attachments, m_size, m_size, 1);
		m_framebuffer = createFramebuffer(vkd, device, &framebufferCreateInfo);
	}

	// pipeline
	{
		Unique<VkShaderModule> vs(createShaderModule(vkd, device, m_context.getBinaryCollection().get("vert"), 0));
		Unique<VkShaderModule> fs(createShaderModule(vkd, device, m_context.getBinaryCollection().get("frag"), 0));

		const PipelineCreateInfo::ColorBlendState::Attachment attachmentState;

		const PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
		m_pipelineLayout = createPipelineLayout(vkd, device, &pipelineLayoutCreateInfo);

		const VkVertexInputBindingDescription vf_binding_desc =
		{
			0,																// binding
			4 * (deUint32)sizeof(float),									// stride
			VK_VERTEX_INPUT_RATE_VERTEX										// inputRate
		};

		const VkVertexInputAttributeDescription vf_attribute_desc =
		{
			0,																// location
			0,																// binding
			VK_FORMAT_R32G32B32A32_SFLOAT,									// format
			0																// offset
		};

		const VkPipelineVertexInputStateCreateInfo vf_info =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// sType
			NULL,															// pNext
			0u,																// flags
			1,																// vertexBindingDescriptionCount
			&vf_binding_desc,												// pVertexBindingDescriptions
			1,																// vertexAttributeDescriptionCount
			&vf_attribute_desc												// pVertexAttributeDescriptions
		};

		PipelineCreateInfo pipelineCreateInfo(*m_pipelineLayout, *m_renderPass, 0, 0);
		pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*vs, "main", VK_SHADER_STAGE_VERTEX_BIT));
		pipelineCreateInfo.addShader(PipelineCreateInfo::PipelineShaderStage(*fs, "main", VK_SHADER_STAGE_FRAGMENT_BIT));
		pipelineCreateInfo.addState(PipelineCreateInfo::InputAssemblerState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST));
		pipelineCreateInfo.addState(PipelineCreateInfo::ColorBlendState(1, &attachmentState));
		const VkViewport viewport	= makeViewport(m_size, m_size);
		const VkRect2D scissor		= makeRect2D(m_size, m_size);
		pipelineCreateInfo.addState(PipelineCreateInfo::ViewportState(1, std::vector<VkViewport>(1, viewport), std::vector<VkRect2D>(1, scissor)));
		pipelineCreateInfo.addState(PipelineCreateInfo::DepthStencilState(false, false, VK_COMPARE_OP_GREATER_OR_EQUAL));
		pipelineCreateInfo.addState(PipelineCreateInfo::RasterizerState());
		pipelineCreateInfo.addState(PipelineCreateInfo::MultiSampleState());
		pipelineCreateInfo.addState(vf_info);
		m_pipeline = createGraphicsPipeline(vkd, device, DE_NULL, &pipelineCreateInfo);
	}

	// vertex buffer
	{
		std::vector<tcu::Vec4> vertices(3);
		vertices[0] = tcu::Vec4(0.5, 0.5, 0.0, 1.0);
		vertices[1] = tcu::Vec4(0.5, 0.0, 0.0, 1.0);
		vertices[2] = tcu::Vec4(0.0, 0.5, 0.0, 1.0);

		const size_t kBufferSize = vertices.size() * sizeof(tcu::Vec4);
		m_vertexBuffer = Buffer::createAndAlloc(vkd, device, BufferCreateInfo(kBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);

		tcu::Vec4 *ptr = reinterpret_cast<tcu::Vec4*>(m_vertexBuffer->getBoundMemory().getHostPtr());
		deMemcpy(ptr, &vertices[0], kBufferSize);

		flushMappedMemoryRange(vkd, device,	m_vertexBuffer->getBoundMemory().getMemory(), m_vertexBuffer->getBoundMemory().getOffset(), VK_WHOLE_SIZE);
	}
}


class GraphicQueryTest : public GraphicQueryTestBase
{
public:
						GraphicQueryTest	(vkt::Context&	context);
	tcu::TestStatus		iterate				(void);
};

GraphicQueryTest::GraphicQueryTest(vkt::Context& context)
	: GraphicQueryTestBase(context)
{
}

tcu::TestStatus GraphicQueryTest::iterate(void)
{
	const DeviceInterface&		vkd					= m_context.getDeviceInterface();
	const VkDevice				device				= m_context.getDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();
	const CmdPoolCreateInfo		cmdPoolCreateInfo	= m_context.getUniversalQueueFamilyIndex();
	Unique<VkCommandPool>		cmdPool				(createCommandPool(vkd, device, &cmdPoolCreateInfo));
	Unique<VkCommandBuffer>		cmdBuffer			(allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	initStateObjects();
	setupCounters();

	vk::Unique<VkQueryPool> queryPool(createQueryPool(0, 1));

	if (!acquireProfilingLock())
	{
		// lock was not acquired in given time, we can't fail the test
		return tcu::TestStatus::pass("Pass");
	}

	// reset query pool
	{
		Unique<VkCommandBuffer>		resetCmdBuffer	(allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
		const Unique<VkFence>		fence			(createFence(vkd, device));
		const VkSubmitInfo			submitInfo		=
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,						// sType
			DE_NULL,											// pNext
			0u,													// waitSemaphoreCount
			DE_NULL,											// pWaitSemaphores
			(const VkPipelineStageFlags*)DE_NULL,				// pWaitDstStageMask
			1u,													// commandBufferCount
			&resetCmdBuffer.get(),								// pCommandBuffers
			0u,													// signalSemaphoreCount
			DE_NULL,											// pSignalSemaphores
		};

		beginCommandBuffer(vkd, *resetCmdBuffer);
		vkd.cmdResetQueryPool(*resetCmdBuffer, *queryPool, 0u, 1u);
		endCommandBuffer(vkd, *resetCmdBuffer);

		VK_CHECK(vkd.queueSubmit(queue, 1u, &submitInfo, *fence));
		VK_CHECK(vkd.waitForFences(device, 1u, &fence.get(), DE_TRUE, ~0ull));
	}

	// begin command buffer
	const VkCommandBufferBeginInfo commandBufBeginParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		DE_NULL,
		VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};
	VK_CHECK(vkd.beginCommandBuffer(*cmdBuffer, &commandBufBeginParams));

	initialTransitionColor2DImage(vkd, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_GENERAL,
								  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

	// begin render pass
	VkClearValue renderPassClearValue;
	deMemset(&renderPassClearValue, 0, sizeof(VkClearValue));

	// perform query during triangle draw
	vkd.cmdBeginQuery(*cmdBuffer, *queryPool, 0, VK_QUERY_CONTROL_PRECISE_BIT);

	beginRenderPass(vkd, *cmdBuffer, *m_renderPass, *m_framebuffer,
					makeRect2D(0, 0, m_size, m_size),
					1, &renderPassClearValue);

	// bind pipeline
	vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);

	// bind vertex buffer
	VkBuffer vertexBuffer = m_vertexBuffer->object();
	const VkDeviceSize vertexBufferOffset = 0;
	vkd.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);

	vkd.cmdDraw(*cmdBuffer, 3, 1, 0, 0);

	endRenderPass(vkd, *cmdBuffer);

	vkd.cmdEndQuery(*cmdBuffer, *queryPool, 0);

	transition2DImage(vkd, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT,
					  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					  VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	endCommandBuffer(vkd, *cmdBuffer);

	// submit command buffer for each pass and wait for its completion
	for (deUint32 passIndex = 0; passIndex < getRequiredNumerOfPasses(); passIndex++)
	{
		const Unique<VkFence> fence(createFence(vkd, device));

		VkPerformanceQuerySubmitInfoKHR performanceQuerySubmitInfo =
		{
			VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_SUBMIT_INFO_KHR,
			NULL,
			passIndex
		};

		const VkSubmitInfo submitInfo =
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,						// sType
			&performanceQuerySubmitInfo,						// pNext
			0u,													// waitSemaphoreCount
			DE_NULL,											// pWaitSemaphores
			(const VkPipelineStageFlags*)DE_NULL,				// pWaitDstStageMask
			1u,													// commandBufferCount
			&cmdBuffer.get(),									// pCommandBuffers
			0u,													// signalSemaphoreCount
			DE_NULL,											// pSignalSemaphores
		};

		VK_CHECK(vkd.queueSubmit(queue, 1u, &submitInfo, *fence));
		VK_CHECK(vkd.waitForFences(device, 1u, &fence.get(), DE_TRUE, ~0ull));
	}

	releaseProfilingLock();

	VK_CHECK(vkd.resetCommandBuffer(*cmdBuffer, 0));

	if (verifyQueryResults(*queryPool))
		return tcu::TestStatus::pass("Pass");
	return tcu::TestStatus::fail("Fail");
}

class GraphicMultiplePoolsTest : public GraphicQueryTestBase
{
public:
						GraphicMultiplePoolsTest	(vkt::Context&	context);
	tcu::TestStatus		iterate						(void);
};

GraphicMultiplePoolsTest::GraphicMultiplePoolsTest(vkt::Context& context)
	: GraphicQueryTestBase(context)
{
}

tcu::TestStatus GraphicMultiplePoolsTest::iterate(void)
{
	if (!m_context.getPerformanceQueryFeatures().performanceCounterMultipleQueryPools)
		throw tcu::NotSupportedError("MultipleQueryPools not supported");

	const DeviceInterface&		vkd					= m_context.getDeviceInterface();
	const VkDevice				device				= m_context.getDevice();
	const VkQueue				queue				= m_context.getUniversalQueue();
	const CmdPoolCreateInfo		cmdPoolCreateInfo	= m_context.getUniversalQueueFamilyIndex();
	Unique<VkCommandPool>		cmdPool				(createCommandPool(vkd, device, &cmdPoolCreateInfo));
	Unique<VkCommandBuffer>		cmdBuffer			(allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	initStateObjects();
	setupCounters();

	vk::Unique<VkQueryPool> queryPool1(createQueryPool(0, 2)),
							queryPool2(createQueryPool(1, 2));

	if (!acquireProfilingLock())
	{
		// lock was not acquired in given time, we can't fail the test
		return tcu::TestStatus::pass("Pass");
	}

	// reset query pools
	{
		Unique<VkCommandBuffer>		resetCmdBuffer	(allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
		const Unique<VkFence>		fence			(createFence(vkd, device));
		const VkSubmitInfo			submitInfo		=
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,						// sType
			DE_NULL,											// pNext
			0u,													// waitSemaphoreCount
			DE_NULL,											// pWaitSemaphores
			(const VkPipelineStageFlags*)DE_NULL,				// pWaitDstStageMask
			1u,													// commandBufferCount
			&resetCmdBuffer.get(),								// pCommandBuffers
			0u,													// signalSemaphoreCount
			DE_NULL,											// pSignalSemaphores
		};

		beginCommandBuffer(vkd, *resetCmdBuffer);
		vkd.cmdResetQueryPool(*resetCmdBuffer, *queryPool1, 0u, 1u);
		vkd.cmdResetQueryPool(*resetCmdBuffer, *queryPool2, 0u, 1u);
		endCommandBuffer(vkd, *resetCmdBuffer);

		VK_CHECK(vkd.queueSubmit(queue, 1u, &submitInfo, *fence));
		VK_CHECK(vkd.waitForFences(device, 1u, &fence.get(), DE_TRUE, ~0ull));
	}

	// begin command buffer
	const VkCommandBufferBeginInfo commandBufBeginParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		DE_NULL,
		VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
		(const VkCommandBufferInheritanceInfo*)DE_NULL,
	};
	VK_CHECK(vkd.beginCommandBuffer(*cmdBuffer, &commandBufBeginParams));

	initialTransitionColor2DImage(vkd, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_LAYOUT_GENERAL,
								  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

	// begin render pass
	VkClearValue renderPassClearValue;
	deMemset(&renderPassClearValue, 0, sizeof(VkClearValue));

	VkBuffer			vertexBuffer		= m_vertexBuffer->object();
	const VkDeviceSize	vertexBufferOffset	= 0;
	const VkQueryPool	queryPools[]		=
	{
		*queryPool1,
		*queryPool2
	};

	// perform two queries during triangle draw
	for (deUint32 loop = 0; loop < DE_LENGTH_OF_ARRAY(queryPools); ++loop)
	{
		const VkQueryPool queryPool = queryPools[loop];
		vkd.cmdBeginQuery(*cmdBuffer, queryPool, 0u, (VkQueryControlFlags)0u);
		beginRenderPass(vkd, *cmdBuffer, *m_renderPass, *m_framebuffer,
						makeRect2D(0, 0, m_size, m_size),
						1, &renderPassClearValue);

		vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		vkd.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer, &vertexBufferOffset);
		vkd.cmdDraw(*cmdBuffer, 3, 1, 0, 0);

		endRenderPass(vkd, *cmdBuffer);
		vkd.cmdEndQuery(*cmdBuffer, queryPool, 0u);
	}

	transition2DImage(vkd, *cmdBuffer, m_colorAttachmentImage->object(), VK_IMAGE_ASPECT_COLOR_BIT,
					  VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					  VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	endCommandBuffer(vkd, *cmdBuffer);

	// submit command buffer for each pass and wait for its completion
	for (deUint32 passIndex = 0; passIndex < getRequiredNumerOfPasses(); passIndex++)
	{
		const Unique<VkFence> fence(createFence(vkd, device));

		VkPerformanceQuerySubmitInfoKHR performanceQuerySubmitInfo =
		{
			VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_SUBMIT_INFO_KHR,
			NULL,
			passIndex
		};

		const VkSubmitInfo submitInfo =
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,						// sType
			&performanceQuerySubmitInfo,						// pNext
			0u,													// waitSemaphoreCount
			DE_NULL,											// pWaitSemaphores
			(const VkPipelineStageFlags*)DE_NULL,				// pWaitDstStageMask
			1u,													// commandBufferCount
			&cmdBuffer.get(),									// pCommandBuffers
			0u,													// signalSemaphoreCount
			DE_NULL,											// pSignalSemaphores
		};

		VK_CHECK(vkd.queueSubmit(queue, 1u, &submitInfo, *fence));
		VK_CHECK(vkd.waitForFences(device, 1u, &fence.get(), DE_TRUE, ~0ull));
	}

	releaseProfilingLock();

	VK_CHECK(vkd.resetCommandBuffer(*cmdBuffer, 0));

	if (verifyQueryResults(*queryPool1) && verifyQueryResults(*queryPool2))
		return tcu::TestStatus::pass("Pass");
	return tcu::TestStatus::fail("Fail");
}

// Base class for all compute tests
class ComputeQueryTestBase : public QueryTestBase
{
public:
	ComputeQueryTestBase(vkt::Context&	context);

protected:
	void initStateObjects(void);

protected:
	Move<VkPipeline>		m_pipeline;
	Move<VkPipelineLayout>	m_pipelineLayout;
	de::SharedPtr<Buffer>	m_buffer;
	Move<VkDescriptorPool>	m_descriptorPool;
	Move<VkDescriptorSet>	m_descriptorSet;
	VkDescriptorBufferInfo	m_descriptorBufferInfo;
	VkBufferMemoryBarrier	m_computeFinishBarrier;
};

ComputeQueryTestBase::ComputeQueryTestBase(vkt::Context& context)
	: QueryTestBase(context)
{
}

void ComputeQueryTestBase::initStateObjects(void)
{
	const DeviceInterface&			vkd = m_context.getDeviceInterface();
	const VkDevice					device = m_context.getDevice();
	const VkDeviceSize				bufferSize = 32 * sizeof(deUint32);
	const CmdPoolCreateInfo			cmdPoolCreateInfo(m_context.getUniversalQueueFamilyIndex());
	const Unique<VkCommandPool>		cmdPool(createCommandPool(vkd, device, &cmdPoolCreateInfo));
	const Unique<VkCommandBuffer>	cmdBuffer(allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vkd, device));

	// create pipeline layout
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,				// sType
			DE_NULL,													// pNext
			0u,															// flags
			1u,															// setLayoutCount
			&(*descriptorSetLayout),									// pSetLayouts
			0u,															// pushConstantRangeCount
			DE_NULL,													// pPushConstantRanges
		};
		m_pipelineLayout = createPipelineLayout(vkd, device, &pipelineLayoutParams);
	}

	// create compute pipeline
	{
		const Unique<VkShaderModule> cs(createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0u));
		const VkPipelineShaderStageCreateInfo pipelineShaderStageParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// sType
			DE_NULL,													// pNext
			(VkPipelineShaderStageCreateFlags)0u,						// flags
			VK_SHADER_STAGE_COMPUTE_BIT,								// stage
			*cs,														// module
			"main",														// pName
			DE_NULL,													// pSpecializationInfo
		};
		const VkComputePipelineCreateInfo pipelineCreateInfo =
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,				// sType
			DE_NULL,													// pNext
			(VkPipelineCreateFlags)0u,									// flags
			pipelineShaderStageParams,									// stage
			*m_pipelineLayout,											// layout
			DE_NULL,													// basePipelineHandle
			0,															// basePipelineIndex
		};
		m_pipeline = createComputePipeline(vkd, device, DE_NULL, &pipelineCreateInfo);
	}

	m_buffer = Buffer::createAndAlloc(vkd, device, BufferCreateInfo(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
		m_context.getDefaultAllocator(), MemoryRequirement::HostVisible);
	m_descriptorPool = DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const VkDescriptorSetAllocateInfo allocateParams =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,		// sType
		DE_NULL,											// pNext
		*m_descriptorPool,									// descriptorPool
		1u,													// setLayoutCount
		&(*descriptorSetLayout),							// pSetLayouts
	};

	m_descriptorSet = allocateDescriptorSet(vkd, device, &allocateParams);
	const VkDescriptorBufferInfo descriptorInfo =
	{
		m_buffer->object(),	// buffer
		0ull,				// offset
		bufferSize,			// range
	};

	DescriptorSetUpdateBuilder()
		.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
		.update(vkd, device);

	// clear buffer
	const std::vector<deUint8>	data((size_t)bufferSize, 0u);
	const Allocation&			allocation = m_buffer->getBoundMemory();
	void*						allocationData = allocation.getHostPtr();
	invalidateMappedMemoryRange(vkd, device, allocation.getMemory(), allocation.getOffset(), bufferSize);
	deMemcpy(allocationData, &data[0], (size_t)bufferSize);

	const VkBufferMemoryBarrier barrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,					// sType
		DE_NULL,													// pNext
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,		// srcAccessMask
		VK_ACCESS_HOST_READ_BIT,									// dstAccessMask
		VK_QUEUE_FAMILY_IGNORED,									// srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,									// destQueueFamilyIndex
		m_buffer->object(),											// buffer
		0ull,														// offset
		bufferSize,													// size
	};
	m_computeFinishBarrier = barrier;
}

class ComputeQueryTest : public ComputeQueryTestBase
{
public:
						ComputeQueryTest	(vkt::Context&	context);
	tcu::TestStatus		iterate				(void);
};

ComputeQueryTest::ComputeQueryTest(vkt::Context& context)
	: ComputeQueryTestBase(context)
{
}

tcu::TestStatus ComputeQueryTest::iterate(void)
{
	const DeviceInterface&			vkd					= m_context.getDeviceInterface();
	const VkDevice					device				= m_context.getDevice();
	const VkQueue					queue				= m_context.getUniversalQueue();
	const CmdPoolCreateInfo			cmdPoolCreateInfo	(m_context.getUniversalQueueFamilyIndex());
	const Unique<VkCommandPool>		cmdPool				(createCommandPool(vkd, device, &cmdPoolCreateInfo));
	const Unique<VkCommandBuffer>	resetCmdBuffer		(allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const Unique<VkCommandBuffer>	cmdBuffer			(allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	initStateObjects();
	setupCounters();

	vk::Unique<VkQueryPool> queryPool(createQueryPool(0, 1));

	if (!acquireProfilingLock())
	{
		// lock was not acquired in given time, we can't fail the test
		return tcu::TestStatus::pass("Pass");
	}

	beginCommandBuffer(vkd, *resetCmdBuffer);
	vkd.cmdResetQueryPool(*resetCmdBuffer, *queryPool, 0u, 1u);
	endCommandBuffer(vkd, *resetCmdBuffer);

	beginCommandBuffer(vkd, *cmdBuffer);
	vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipeline);
	vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0u, 1u, &(m_descriptorSet.get()), 0u, DE_NULL);

	vkd.cmdBeginQuery(*cmdBuffer, *queryPool, 0u, (VkQueryControlFlags)0u);
	vkd.cmdDispatch(*cmdBuffer, 2, 2, 2);
	vkd.cmdEndQuery(*cmdBuffer, *queryPool, 0u);

	vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
		(VkDependencyFlags)0u, 0u, (const VkMemoryBarrier*)DE_NULL, 1u, &m_computeFinishBarrier, 0u, (const VkImageMemoryBarrier*)DE_NULL);
	endCommandBuffer(vkd, *cmdBuffer);

	// submit reset of queries only once
	{
		const VkSubmitInfo submitInfo =
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,						// sType
			DE_NULL,											// pNext
			0u,													// waitSemaphoreCount
			DE_NULL,											// pWaitSemaphores
			(const VkPipelineStageFlags*)DE_NULL,				// pWaitDstStageMask
			1u,													// commandBufferCount
			&resetCmdBuffer.get(),								// pCommandBuffers
			0u,													// signalSemaphoreCount
			DE_NULL,											// pSignalSemaphores
		};

		VK_CHECK(vkd.queueSubmit(queue, 1u, &submitInfo, DE_NULL));
	}

	// submit command buffer for each pass and wait for its completion
	for (deUint32 passIndex = 0; passIndex < getRequiredNumerOfPasses(); passIndex++)
	{
		const Unique<VkFence> fence(createFence(vkd, device));

		VkPerformanceQuerySubmitInfoKHR performanceQuerySubmitInfo =
		{
			VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_SUBMIT_INFO_KHR,
			NULL,
			passIndex
		};

		const VkSubmitInfo submitInfo =
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,						// sType
			&performanceQuerySubmitInfo,						// pNext
			0u,													// waitSemaphoreCount
			DE_NULL,											// pWaitSemaphores
			(const VkPipelineStageFlags*)DE_NULL,				// pWaitDstStageMask
			1u,													// commandBufferCount
			&cmdBuffer.get(),									// pCommandBuffers
			0u,													// signalSemaphoreCount
			DE_NULL,											// pSignalSemaphores
		};

		VK_CHECK(vkd.queueSubmit(queue, 1u, &submitInfo, *fence));
		VK_CHECK(vkd.waitForFences(device, 1u, &fence.get(), DE_TRUE, ~0ull));
	}

	releaseProfilingLock();

	VK_CHECK(vkd.resetCommandBuffer(*cmdBuffer, 0));

	if (verifyQueryResults(*queryPool))
		return tcu::TestStatus::pass("Pass");
	return tcu::TestStatus::fail("Fail");
}

class ComputeMultiplePoolsTest : public ComputeQueryTestBase
{
public:
					ComputeMultiplePoolsTest	(vkt::Context&	context);
	tcu::TestStatus iterate						(void);
};

ComputeMultiplePoolsTest::ComputeMultiplePoolsTest(vkt::Context& context)
	: ComputeQueryTestBase(context)
{
}

tcu::TestStatus ComputeMultiplePoolsTest::iterate(void)
{
	if (!m_context.getPerformanceQueryFeatures().performanceCounterMultipleQueryPools)
		throw tcu::NotSupportedError("MultipleQueryPools not supported");

	const DeviceInterface&			vkd = m_context.getDeviceInterface();
	const VkDevice					device = m_context.getDevice();
	const VkQueue					queue = m_context.getUniversalQueue();
	const CmdPoolCreateInfo			cmdPoolCreateInfo(m_context.getUniversalQueueFamilyIndex());
	const Unique<VkCommandPool>		cmdPool(createCommandPool(vkd, device, &cmdPoolCreateInfo));
	const Unique<VkCommandBuffer>	resetCmdBuffer(allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const Unique<VkCommandBuffer>	cmdBuffer(allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	initStateObjects();
	setupCounters();

	vk::Unique<VkQueryPool>	queryPool1(createQueryPool(0, 2)),
							queryPool2(createQueryPool(1, 2));

	if (!acquireProfilingLock())
	{
		// lock was not acquired in given time, we can't fail the test
		return tcu::TestStatus::pass("Pass");
	}

	const VkQueryPool queryPools[] =
	{
		*queryPool1,
		*queryPool2
	};

	beginCommandBuffer(vkd, *resetCmdBuffer);
	vkd.cmdResetQueryPool(*resetCmdBuffer, queryPools[0], 0u, 1u);
	vkd.cmdResetQueryPool(*resetCmdBuffer, queryPools[1], 0u, 1u);
	endCommandBuffer(vkd, *resetCmdBuffer);

	beginCommandBuffer(vkd, *cmdBuffer);
	vkd.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipeline);
	vkd.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0u, 1u, &(m_descriptorSet.get()), 0u, DE_NULL);

	// perform two queries
	for (deUint32 loop = 0; loop < DE_LENGTH_OF_ARRAY(queryPools); ++loop)
	{
		const VkQueryPool queryPool = queryPools[loop];
		vkd.cmdBeginQuery(*cmdBuffer, queryPool, 0u, (VkQueryControlFlags)0u);
		vkd.cmdDispatch(*cmdBuffer, 2, 2, 2);
		vkd.cmdEndQuery(*cmdBuffer, queryPool, 0u);
	}

	vkd.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
		(VkDependencyFlags)0u, 0u, (const VkMemoryBarrier*)DE_NULL, 1u, &m_computeFinishBarrier, 0u, (const VkImageMemoryBarrier*)DE_NULL);
	endCommandBuffer(vkd, *cmdBuffer);

	// submit reset of queries only once
	{
		const VkSubmitInfo submitInfo =
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,						// sType
			DE_NULL,											// pNext
			0u,													// waitSemaphoreCount
			DE_NULL,											// pWaitSemaphores
			(const VkPipelineStageFlags*)DE_NULL,				// pWaitDstStageMask
			1u,													// commandBufferCount
			&resetCmdBuffer.get(),								// pCommandBuffers
			0u,													// signalSemaphoreCount
			DE_NULL,											// pSignalSemaphores
		};

		VK_CHECK(vkd.queueSubmit(queue, 1u, &submitInfo, DE_NULL));
	}

	// submit command buffer for each pass and wait for its completion
	for (deUint32 passIndex = 0; passIndex < getRequiredNumerOfPasses(); passIndex++)
	{
		const Unique<VkFence> fence(createFence(vkd, device));

		VkPerformanceQuerySubmitInfoKHR performanceQuerySubmitInfo =
		{
			VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_SUBMIT_INFO_KHR,
			NULL,
			passIndex
		};

		const VkSubmitInfo submitInfo =
		{
			VK_STRUCTURE_TYPE_SUBMIT_INFO,						// sType
			&performanceQuerySubmitInfo,						// pNext
			0u,													// waitSemaphoreCount
			DE_NULL,											// pWaitSemaphores
			(const VkPipelineStageFlags*)DE_NULL,				// pWaitDstStageMask
			1u,													// commandBufferCount
			&cmdBuffer.get(),									// pCommandBuffers
			0u,													// signalSemaphoreCount
			DE_NULL,											// pSignalSemaphores
		};

		VK_CHECK(vkd.queueSubmit(queue, 1u, &submitInfo, *fence));
		VK_CHECK(vkd.waitForFences(device, 1u, &fence.get(), DE_TRUE, ~0ull));
	}

	releaseProfilingLock();

	VK_CHECK(vkd.resetCommandBuffer(*cmdBuffer, 0));

	if (verifyQueryResults(*queryPool1) && verifyQueryResults(*queryPool2))
		return tcu::TestStatus::pass("Pass");
	return tcu::TestStatus::fail("Fail");
}

enum TestType
{
	TT_ENUMERATE_AND_VALIDATE	= 0,
	TT_QUERY,
	TT_MULTIPLE_POOLS
};

class QueryPoolPerformanceTest : public TestCase
{
public:
	QueryPoolPerformanceTest (tcu::TestContext &context, TestType testType, VkQueueFlagBits queueFlagBits, const char *name)
		: TestCase			(context, name, "")
		, m_testType		(testType)
		, m_queueFlagBits	(queueFlagBits)
	{
	}

	vkt::TestInstance* createInstance (vkt::Context& context) const
	{
		if (m_testType == TT_ENUMERATE_AND_VALIDATE)
			return new EnumerateAndValidateTest(context, m_queueFlagBits);

		if (m_queueFlagBits == VK_QUEUE_GRAPHICS_BIT)
		{
			if (m_testType == TT_QUERY)
				return new GraphicQueryTest(context);
			return new GraphicMultiplePoolsTest(context);
		}

		// tests for VK_QUEUE_COMPUTE_BIT
		if (m_testType == TT_QUERY)
			return new ComputeQueryTest(context);
		return new ComputeMultiplePoolsTest(context);
	}

	void initPrograms (SourceCollections& programCollection) const
	{
		// validation test do not need programs
		if (m_testType == TT_ENUMERATE_AND_VALIDATE)
			return;

		if (m_queueFlagBits == VK_QUEUE_COMPUTE_BIT)
		{
			programCollection.glslSources.add("comp")
				<< glu::ComputeSource("#version 430\n"
									  "layout (local_size_x = 1) in;\n"
									  "layout(binding = 0) writeonly buffer Output {\n"
									  "		uint values[];\n"
									  "} sb_out;\n\n"
									  "void main (void) {\n"
									  "		uint index = uint(gl_GlobalInvocationID.x);\n"
									  "		sb_out.values[index] += gl_GlobalInvocationID.y*2;\n"
									  "}\n");
			return;
		}

		programCollection.glslSources.add("frag")
			<< glu::FragmentSource("#version 430\n"
								   "layout(location = 0) out vec4 out_FragColor;\n"
								   "void main()\n"
								   "{\n"
								   "	out_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
								   "}\n");

		programCollection.glslSources.add("vert")
			<< glu::VertexSource("#version 430\n"
								 "layout(location = 0) in vec4 in_Position;\n"
								 "out gl_PerVertex { vec4 gl_Position; float gl_PointSize; };\n"
								 "void main() {\n"
								 "	gl_Position  = in_Position;\n"
								 "	gl_PointSize = 1.0;\n"
								 "}\n");
	}

private:

	TestType			m_testType;
	VkQueueFlagBits		m_queueFlagBits;
};

} //anonymous

QueryPoolPerformanceTests::QueryPoolPerformanceTests (tcu::TestContext &testCtx)
	: TestCaseGroup(testCtx, "performance_query", "Tests for performance queries")
{
}

void QueryPoolPerformanceTests::init (void)
{
	addChild(new QueryPoolPerformanceTest(m_testCtx, TT_ENUMERATE_AND_VALIDATE, VK_QUEUE_GRAPHICS_BIT, "enumerate_and_validate_graphic"));
	addChild(new QueryPoolPerformanceTest(m_testCtx, TT_ENUMERATE_AND_VALIDATE, VK_QUEUE_COMPUTE_BIT,  "enumerate_and_validate_compute"));
	addChild(new QueryPoolPerformanceTest(m_testCtx, TT_QUERY, VK_QUEUE_GRAPHICS_BIT, "query_graphic"));
	addChild(new QueryPoolPerformanceTest(m_testCtx, TT_QUERY, VK_QUEUE_COMPUTE_BIT, "query_compute"));
	addChild(new QueryPoolPerformanceTest(m_testCtx, TT_MULTIPLE_POOLS, VK_QUEUE_GRAPHICS_BIT, "multiple_pools_graphic"));
	addChild(new QueryPoolPerformanceTest(m_testCtx, TT_MULTIPLE_POOLS, VK_QUEUE_COMPUTE_BIT, "multiple_pools_compute"));
}

} //QueryPool
} //vkt
