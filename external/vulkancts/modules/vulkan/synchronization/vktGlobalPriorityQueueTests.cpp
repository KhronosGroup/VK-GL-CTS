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
 * \file  vktGlobalPriorityQueueTests.cpp
 * \brief Global Priority Queue Tests
 *//*--------------------------------------------------------------------*/

#include "vktGlobalPriorityQueueTests.hpp"
#include "vktGlobalPriorityQueueUtils.hpp"

#include "vkBarrierUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "../image/vktImageTestsUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkRefUtil.hpp"

#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"

#include "deDefs.h"
#include "deMath.h"
#include "deRandom.h"
#include "deRandom.hpp"
#include "deSharedPtr.hpp"
#include "deString.h"
#include "deMemory.h"

#include "tcuStringTemplate.hpp"

#include <string>
#include <sstream>
#include <map>

using namespace vk;

namespace vkt
{
namespace synchronization
{
namespace
{

enum class SyncType
{
	None,
	Semaphore
};

struct TestConfig
{
	VkQueueFlagBits				transitionFrom;
	VkQueueFlagBits				transitionTo;
	VkQueueGlobalPriorityKHR	priorityFrom;
	VkQueueGlobalPriorityKHR	priorityTo;
	bool						enableProtected;
	bool						enableSparseBinding;
	SyncType					syncType;
	deUint32					width;
	deUint32					height;
	VkFormat					format;
	bool selectFormat (const InstanceInterface& vk, VkPhysicalDevice dev, std::initializer_list<VkFormat> formats);
};

bool TestConfig::selectFormat (const InstanceInterface& vk, VkPhysicalDevice dev, std::initializer_list<VkFormat> formats)
{
	auto doesFormatMatch = [](const VkFormat fmt) -> bool
	{
		const auto tcuFmt = mapVkFormat(fmt);
		return tcuFmt.order == tcu::TextureFormat::ChannelOrder::R;
	};
	VkFormatProperties2 props{};
	const VkFormatFeatureFlags flags = VK_FORMAT_FEATURE_TRANSFER_SRC_BIT
										| VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
	for (auto i = formats.begin(); i != formats.end(); ++i)
	{
		props.sType				= VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
		props.pNext				= nullptr;
		props.formatProperties	= {};
		const VkFormat		fmt = *i;
		vk.getPhysicalDeviceFormatProperties2(dev, fmt, &props);
		if (doesFormatMatch(fmt) && ((props.formatProperties.optimalTilingFeatures & flags) == flags))
		{
			this->format = fmt;
			return true;
		}
	}
	return false;
}

struct CheckerboardBuilder
{
	CheckerboardBuilder (uint32_t width, uint32_t height)
		: m_width(width), m_height(height) {}

	static uint32_t blackFieldCount (uint32_t w, uint32_t h)
	{
		return ((w+1)/2)*((h+1)/2)+(w/2)*(h/2);
	}

	uint32_t blackFieldCount () const
	{
		return blackFieldCount(m_width, m_height);
	}

	uint32_t fieldIndex (uint32_t x, uint32_t y) const
	{
		return blackFieldCount(m_width, y) + (x / 2);
	}

	void buildVerticesAndIndices (std::vector<float>& vertices, std::vector<uint32_t>& indices) const
	{
		const uint32_t	vertPerQuad		= 4;
		const uint32_t	indexPerQuad	= 6;
		const uint32_t	quadCount		= blackFieldCount();
		const uint32_t	compPerQuad		= vertPerQuad * 2;
		const uint32_t	vertCount		= quadCount * vertPerQuad;
		const uint32_t	indexCount		= quadCount * indexPerQuad;

		vertices.resize(vertCount * 2);
		indices.resize(indexCount);

		for (uint32_t z = 0; z < 2; ++z)
		for (uint32_t y = 0; y < m_height; ++y)
		for (uint32_t x = 0; x < m_width; ++x)
		{
			if (((x + y) % 2) == 1)
				continue;

			const float x1 = float(x) / float(m_width);
			const float y1 = float(y) / float(m_height);
			const float x2 = (float(x) + 1.0f) / float(m_width);
			const float y2 = (float(y) + 1.0f) / float(m_height);

			const float xx1 = x1 * 2.0f - 1.0f;
			const float yy1 = y1 * 2.0f - 1.0f;
			const float xx2 = x2 * 2.0f - 1.0f;
			const float yy2 = y2 * 2.0f - 1.0f;

			const uint32_t quad = fieldIndex(x, y);

			if (z == 0)
			{
				vertices[ quad * compPerQuad  + 0 ] = xx1;
				vertices[ quad * compPerQuad  + 1 ] = yy1;
				vertices[ quad * compPerQuad  + 2 ] = xx2;
				vertices[ quad * compPerQuad  + 3 ] = yy1;

				indices[ quad * indexPerQuad + 0 ] = quad * vertPerQuad + 0;
				indices[ quad * indexPerQuad + 1 ] = quad * vertPerQuad + 1;
				indices[ quad * indexPerQuad + 2 ] = quad * vertPerQuad + 2;
			}
			else
			{
				vertices[ quad * compPerQuad  + 4 ] = xx2;
				vertices[ quad * compPerQuad  + 5 ] = yy2;
				vertices[ quad * compPerQuad  + 6 ] = xx1;
				vertices[ quad * compPerQuad  + 7 ] = yy2;

				indices[ quad * indexPerQuad + 3 ] = quad * vertPerQuad + 2;
				indices[ quad * indexPerQuad + 4 ] = quad * vertPerQuad + 3;
				indices[ quad * indexPerQuad + 5 ] = quad * vertPerQuad + 0;
			}
		}
	}

private:
	uint32_t	m_width;
	uint32_t	m_height;
};

template<class T, class P = T(*)[1]>
auto begin (void* p) -> decltype(std::begin(*std::declval<P>()))
{
	return std::begin(*static_cast<P>(p));
}

class GPQInstanceBase : public TestInstance
{
public:
	typedef std::initializer_list<VkDescriptorSetLayout> DSLayouts;
	typedef tcu::ConstPixelBufferAccess	BufferAccess;

					GPQInstanceBase			(Context&					ctx,
											 const TestConfig&			cfg);
	template<class PushConstant = void>
		auto		createPipelineLayout	(DSLayouts					setLayouts)		const -> Move<VkPipelineLayout>;
	auto			createGraphicsPipeline	(VkPipelineLayout			pipelineLayout,
											 VkRenderPass				renderPass)			  -> Move<VkPipeline>;
	auto			createComputePipeline	(VkPipelineLayout			pipelineLayout)		  -> Move<VkPipeline>;
	auto			createImage				(VkImageUsageFlags			usage,
											 deUint32					queueFamilyIdx,
											 VkQueue					queue)			const -> de::MovePtr<ImageWithMemory>;
	auto			createView				(VkImage					image,
											 VkImageSubresourceRange&	range)			const -> Move<VkImageView>;
	void			submitCommands			(VkCommandBuffer			producerCmd,
											 VkCommandBuffer			consumerCmd)	const;
	bool			verify					(const BufferAccess&		result,
											 deUint32					blackColor,
											 deUint32					whiteColor)		const;
protected:
	auto createPipelineLayout			(const VkPushConstantRange*		pRange,
										 DSLayouts						setLayouts)		const -> Move<VkPipelineLayout>;
	const TestConfig		m_config;
	const SpecialDevice		m_device;
	Move<VkShaderModule>	m_vertex;
	Move<VkShaderModule>	m_fragment;
	Move<VkShaderModule>	m_compute;
};
GPQInstanceBase::GPQInstanceBase (Context& ctx, const TestConfig& cfg)
	: TestInstance	(ctx)
	, m_config		(cfg)
	, m_device		(ctx,
					 cfg.transitionFrom,	cfg.transitionTo,
					 cfg.priorityFrom,		cfg.priorityTo,
					 cfg.enableProtected,	cfg.enableSparseBinding)
	, m_vertex		()
	, m_fragment	()
	, m_compute		()
{
}

de::MovePtr<ImageWithMemory> GPQInstanceBase::createImage (VkImageUsageFlags usage, deUint32 queueFamilyIdx, VkQueue queue) const
{
	const InstanceInterface&	vki		= m_context.getInstanceInterface();
	const DeviceInterface&		vkd		= m_context.getDeviceInterface();
	const VkPhysicalDevice		phys	= m_context.getPhysicalDevice();
	const VkDevice				dev		= m_device.device;
	Allocator&					alloc	= m_device.getAllocator();
	VkImageCreateFlags			flags	= 0;

	if (m_config.enableProtected)		flags |= VK_IMAGE_CREATE_PROTECTED_BIT;
	if (m_config.enableSparseBinding)	flags |= (VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT);

	VkImageCreateInfo imageInfo{};
	imageInfo.sType					= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.pNext					= nullptr;
	imageInfo.flags					= flags;
	imageInfo.imageType				= VK_IMAGE_TYPE_2D;
	imageInfo.format				= m_config.format;
	imageInfo.extent.width			= m_config.width;
	imageInfo.extent.height			= m_config.height;
	imageInfo.extent.depth			= 1;
	imageInfo.mipLevels				= 1;
	imageInfo.arrayLayers			= 1;
	imageInfo.samples				= VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling				= VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage					= usage;
	imageInfo.sharingMode			= VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.queueFamilyIndexCount	= 1;
	imageInfo.pQueueFamilyIndices	= &queueFamilyIdx;
	imageInfo.initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;

	return de::MovePtr<ImageWithMemory>(new ImageWithMemory(vki, vkd, phys, dev, alloc, imageInfo, queue));
}

Move<VkImageView> GPQInstanceBase::createView	(VkImage image, VkImageSubresourceRange& range) const
{
	const DeviceInterface&	vkd		= m_context.getDeviceInterface();
	const VkDevice			dev		= m_device.device;

	range = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
	return makeImageView(vkd, dev, image, VK_IMAGE_VIEW_TYPE_2D, m_config.format, range);
}

Move<VkPipelineLayout>	GPQInstanceBase::createPipelineLayout (const VkPushConstantRange* pRange, DSLayouts setLayouts) const
{
	std::vector<VkDescriptorSetLayout>	layouts(setLayouts.size());
	auto ii = setLayouts.begin();
	for (auto i = ii; i != setLayouts.end(); ++i)
		layouts[std::distance(ii, i)] = *i;

	VkPipelineLayoutCreateInfo		info{};
	info.sType					= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.pNext					= nullptr;
	info.flags					= VkPipelineLayoutCreateFlags(0);
	info.setLayoutCount			= static_cast<uint32_t>(layouts.size());
	info.pSetLayouts			= layouts.size() ? layouts.data() : nullptr;
	info.pushConstantRangeCount	= pRange ? 1 : 0;
	info.pPushConstantRanges	= pRange;

	return ::vk::createPipelineLayout(m_context.getDeviceInterface(), m_device.device, &info);
}

template<> Move<VkPipelineLayout> DE_UNUSED_FUNCTION GPQInstanceBase::createPipelineLayout<void> (DSLayouts setLayouts) const
{
	return createPipelineLayout(nullptr, setLayouts);
}

template<class PushConstant> Move<VkPipelineLayout>	GPQInstanceBase::createPipelineLayout (DSLayouts setLayouts) const
{
	VkPushConstantRange	range{};
	range.stageFlags	= VK_SHADER_STAGE_ALL;
	range.offset		= 0;
	range.size			= static_cast<uint32_t>(sizeof(PushConstant));
	return createPipelineLayout(&range, setLayouts);
}

Move<VkPipeline> GPQInstanceBase::createGraphicsPipeline (VkPipelineLayout pipelineLayout, VkRenderPass renderPass)
{
	const DeviceInterface&	vkd	= m_context.getDeviceInterface();
	const VkDevice			dev	= m_device.device;

	m_vertex	= createShaderModule(vkd, dev, m_context.getBinaryCollection().get("vert"));
	m_fragment	= createShaderModule(vkd, dev, m_context.getBinaryCollection().get("frag"));

	const std::vector<VkViewport>	viewports		{ makeViewport(m_config.width, m_config.height) };
	const std::vector<VkRect2D>		scissors		{ makeRect2D(m_config.width, m_config.height) };
	const auto						vertexBinding	= makeVertexInputBindingDescription(0u, static_cast<deUint32>(2 * sizeof(float)), VK_VERTEX_INPUT_RATE_VERTEX);
	const auto						vertexAttrib	= makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32_SFLOAT, 0u);
	const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	//	VkStructureType								sType;
		nullptr,														//	const void*									pNext;
		0u,																//	VkPipelineVertexInputStateCreateFlags		flags;
		1u,																//	deUint32									vertexBindingDescriptionCount;
		&vertexBinding,													//	const VkVertexInputBindingDescription*		pVertexBindingDescriptions;
		1u,																//	deUint32									vertexAttributeDescriptionCount;
		&vertexAttrib													//	const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
	};

	return makeGraphicsPipeline(vkd, dev, pipelineLayout,*m_vertex,VkShaderModule(0),
						 VkShaderModule(0),VkShaderModule(0),*m_fragment,renderPass,viewports, scissors,
						 VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,0,0,&vertexInputStateCreateInfo);


}

Move<VkPipeline> GPQInstanceBase::createComputePipeline	(VkPipelineLayout	pipelineLayout)
{
	const DeviceInterface&	vk	= m_context.getDeviceInterface();
	const VkDevice			dev	= m_device.device;

	m_compute = createShaderModule(vk, dev, m_context.getBinaryCollection().get("comp"));

	VkPipelineShaderStageCreateInfo	sci{};
	sci.sType				= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	sci.pNext				= nullptr;
	sci.flags				= VkPipelineShaderStageCreateFlags(0);
	sci.stage				= VK_SHADER_STAGE_COMPUTE_BIT;
	sci.module				= *m_compute;
	sci.pName				= "main";
	sci.pSpecializationInfo	= nullptr;

	VkComputePipelineCreateInfo	ci{};
	ci.sType				= VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	ci.pNext				= nullptr;
	ci.flags				= VkPipelineCreateFlags(0);
	ci.stage				= sci;
	ci.layout				= pipelineLayout;
	ci.basePipelineHandle	= VkPipeline(0);
	ci.basePipelineIndex	= 0;

	return vk::createComputePipeline(vk, dev, VkPipelineCache(0), &ci, nullptr);
}

VkPipelineStageFlags queueFlagBitToPipelineStage (VkQueueFlagBits bit);
void GPQInstanceBase::submitCommands (VkCommandBuffer producerCmd, VkCommandBuffer consumerCmd) const
{
	const DeviceInterface&	vkd		= m_context.getDeviceInterface();
	const VkDevice			dev		= m_device.device;

	Move<VkSemaphore>		sem		= createSemaphore(vkd, dev);
	Move<VkFence>			fence	= createFence(vkd, dev);

	const VkSubmitInfo		semSubmitProducer
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType				sType;
		nullptr,						// const void*					pNext;
		0,								// deUint32						waitSemaphoreCount;
		nullptr,						// const VkSemaphore*			pWaitSemaphores;
		nullptr,						// const VkPipelineStageFlags*	pWaitDstStageMask;
		1u,								// deUint32						commandBufferCount;
		&producerCmd,					// const VkCommandBuffer*		pCommandBuffers;
		1u,								// deUint32						signalSemaphoreCount;
		&sem.get(),						// const VkSemaphore*			pSignalSemaphores;
	};

	const VkPipelineStageFlags	dstWaitStages = VK_PIPELINE_STAGE_TRANSFER_BIT |
											    queueFlagBitToPipelineStage(m_config.transitionTo);
	const VkSubmitInfo		semSubmitConsumer
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType				sType;
		nullptr,						// const void*					pNext;
		1u,								// deUint32						waitSemaphoreCount;
		&(*sem),						// const VkSemaphore*			pWaitSemaphores;
		&dstWaitStages,					// const VkPipelineStageFlags*	pWaitDstStageMask;
		1u,								// deUint32						commandBufferCount;
		&consumerCmd,					// const VkCommandBuffer*		pCommandBuffers;
		0,								// deUint32						signalSemaphoreCount;
		nullptr,						// const VkSemaphore*			pSignalSemaphores;
	};

	switch (m_config.syncType)
	{
	case SyncType::None:
		submitCommandsAndWait(vkd, dev, m_device.queueFrom, producerCmd);
		submitCommandsAndWait(vkd, dev, m_device.queueTo, consumerCmd);
		break;
	case SyncType::Semaphore:
		VK_CHECK(vkd.queueSubmit(m_device.queueFrom, 1u, &semSubmitProducer, VkFence(0)));
		VK_CHECK(vkd.queueSubmit(m_device.queueTo, 1u, &semSubmitConsumer, *fence));
		VK_CHECK(vkd.waitForFences(dev, 1u, &fence.get(), DE_TRUE, ~0ull));
		break;
	}
}

template<VkQueueFlagBits, VkQueueFlagBits> class GPQInstance;
#define DECLARE_INSTANCE(flagsFrom_, flagsTo_)									\
	template<> class GPQInstance<flagsFrom_, flagsTo_> : public GPQInstanceBase	\
	{	public:	GPQInstance (Context& ctx, const TestConfig& cfg)				\
					: GPQInstanceBase(ctx, cfg)	{ }								\
				virtual tcu::TestStatus	iterate (void) override;				}

DECLARE_INSTANCE(VK_QUEUE_GRAPHICS_BIT,	VK_QUEUE_COMPUTE_BIT);
DECLARE_INSTANCE(VK_QUEUE_COMPUTE_BIT,	VK_QUEUE_GRAPHICS_BIT);

class GPQCase;
typedef TestInstance* (GPQCase::* CreateInstanceProc)(Context&) const;
typedef std::pair<VkQueueFlagBits,VkQueueFlagBits> CreateInstanceKey;
typedef std::map<CreateInstanceKey, CreateInstanceProc> CreateInstanceMap;
#define MAPENTRY(from_,to_) m_createInstanceMap[{from_,to_}] = &GPQCase::createInstance<from_,to_>

class GPQCase : public TestCase
{
public:
					GPQCase				(tcu::TestContext&	ctx,
										 const std::string&	name,
										 const TestConfig&	cfg,
										 const std::string&	desc = {});
	void			initPrograms		(SourceCollections&	programs)	const override;
	TestInstance*	createInstance		(Context&			context)	const override;
	void			checkSupport		(Context&			context)	const override;
	static deUint32	testValue;

private:
	template<VkQueueFlagBits, VkQueueFlagBits>
		TestInstance*	createInstance	(Context&			context)	const;
	mutable TestConfig	m_config;
	CreateInstanceMap	m_createInstanceMap;
};
deUint32 GPQCase::testValue = 113;

GPQCase::GPQCase (tcu::TestContext&		ctx,
				  const std::string&	name,
				  const TestConfig&		cfg,
				  const std::string&	desc)
	: TestCase				(ctx, name, desc)
	, m_config				(cfg)
	, m_createInstanceMap	()
{
	MAPENTRY(VK_QUEUE_GRAPHICS_BIT,	VK_QUEUE_COMPUTE_BIT);
	MAPENTRY(VK_QUEUE_COMPUTE_BIT,	VK_QUEUE_GRAPHICS_BIT);
}

VkPipelineStageFlags queueFlagBitToPipelineStage (VkQueueFlagBits bit)
{
	switch (bit) {
	case VK_QUEUE_COMPUTE_BIT:
		return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	case VK_QUEUE_GRAPHICS_BIT:
		return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	default:
		DE_ASSERT(VK_FALSE);
	}
	return VK_QUEUE_FLAG_BITS_MAX_ENUM;
}

template<VkQueueFlagBits flagsFrom, VkQueueFlagBits flagsTo>
TestInstance* GPQCase::createInstance (Context& context) const
{
	return new GPQInstance<flagsFrom, flagsTo>(context, m_config);
}

TestInstance* GPQCase::createInstance (Context& context) const
{
	const CreateInstanceKey	key(m_config.transitionFrom, m_config.transitionTo);
	return (this->*(m_createInstanceMap.at(key)))(context);
}

std::ostream& operator<<(std::ostream& str, const VkQueueFlagBits& bit)
{
	const char* s = nullptr;
	const auto d = std::to_string(bit);
	switch (bit)
	{
		case VK_QUEUE_GRAPHICS_BIT:			s = "VK_QUEUE_GRAPHICS_BIT";		break;
		case VK_QUEUE_COMPUTE_BIT:			s = "VK_QUEUE_COMPUTE_BIT";			break;
		case VK_QUEUE_TRANSFER_BIT:			s = "VK_QUEUE_TRANSFER_BIT";		break;
		case VK_QUEUE_SPARSE_BINDING_BIT:	s = "VK_QUEUE_SPARSE_BINDING_BIT";	break;
		case VK_QUEUE_PROTECTED_BIT:		s = "VK_QUEUE_PROTECTED_BIT";		break;
		default:							s = d.c_str();						break;
	}
	return (str << s);
}

void GPQCase::checkSupport (Context& context) const
{
	const InstanceInterface&	vki = context.getInstanceInterface();
	const VkPhysicalDevice		dev = context.getPhysicalDevice();

	context.requireInstanceFunctionality("VK_KHR_get_physical_device_properties2");
	context.requireDeviceFunctionality("VK_EXT_global_priority");

	if (!m_config.selectFormat(vki, dev, { VK_FORMAT_R32_SINT, VK_FORMAT_R32_UINT, VK_FORMAT_R8_SINT, VK_FORMAT_R8_UINT }))
	{
		TCU_THROW(NotSupportedError, "Unable to find a proper format");
	}

	VkPhysicalDeviceProtectedMemoryFeatures	memFeatures
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES,
		nullptr,
		VK_FALSE
	};
	VkPhysicalDeviceFeatures2	devFeatures
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		&memFeatures,
		{}
	};
	vki.getPhysicalDeviceFeatures2(dev, &devFeatures);

	if (m_config.enableProtected && (VK_FALSE == memFeatures.protectedMemory))
	{
		TCU_THROW(NotSupportedError, "Queue families with VK_QUEUE_PROTECTED_BIT not supported");
	}

	const VkBool32 sparseEnabled = devFeatures.features.sparseBinding & devFeatures.features.sparseResidencyBuffer & devFeatures.features.sparseResidencyImage2D;
	if (m_config.enableSparseBinding && (VK_FALSE == sparseEnabled))
	{
		TCU_THROW(NotSupportedError, "Queue families with VK_QUEUE_SPARSE_BINDING_BIT not supported");
	}

	auto assertUnavailableQueue = [](const deUint32 qIdx, VkQueueFlagBits qfb, VkQueueGlobalPriorityKHR qgp)
	{
		if (qIdx == INVALID_UINT32) {
			std::ostringstream buf;
			buf << "Unable to create a queue " << qfb << " with a priority " << qgp;
			buf.flush();
			TCU_THROW(NotSupportedError, buf.str());
		}
	};

	const bool priorityQueryEnabled = context.isDeviceFunctionalitySupported("VK_EXT_global_priority_query");

	VkQueueFlags	flagFrom	= m_config.transitionFrom;
	VkQueueFlags	flagTo		= m_config.transitionTo;
	if (m_config.enableProtected)
	{
		flagFrom |= VK_QUEUE_PROTECTED_BIT;
		flagTo	|= VK_QUEUE_PROTECTED_BIT;
	}
	if (m_config.enableSparseBinding)
	{
		flagFrom |= VK_QUEUE_SPARSE_BINDING_BIT;
		flagTo	|= VK_QUEUE_SPARSE_BINDING_BIT;
	}

	const deUint32 queueFromIndex	= findQueueFamilyIndex(vki, dev, flagFrom,
														   SpecialDevice::getColissionFlags(m_config.transitionFrom),
														   priorityQueryEnabled,
														   QueueGlobalPriorities({m_config.priorityFrom}));
	assertUnavailableQueue(queueFromIndex, m_config.transitionFrom, m_config.priorityFrom);

	const deUint32 queueToIndex		= findQueueFamilyIndex(vki, dev, flagTo,
														   SpecialDevice::getColissionFlags(m_config.transitionTo),
														   priorityQueryEnabled,
														   QueueGlobalPriorities({m_config.priorityTo}));
	assertUnavailableQueue(queueToIndex, m_config.transitionTo, m_config.priorityTo);

	if (queueFromIndex == queueToIndex)
	{
		std::ostringstream buf;
		buf << "Unable to find separate queues " << m_config.transitionFrom << " and " << m_config.transitionTo;
		buf.flush();
		TCU_THROW(NotSupportedError, buf.str());
	}
}

std::string getShaderImageBufferType (const tcu::TextureFormat& format)
{
	return image::getFormatPrefix(format) + "imageBuffer";
}

void GPQCase::initPrograms (SourceCollections& programs) const
{
	const std::string producerComp(R"glsl(
	#version 450
	layout(std430, push_constant) uniform PC
		{ uint width, height; } pc;
	struct Index { uint k; };
	struct Quad { vec2 c[4]; };
	layout(set=0, binding=0) buffer Quads
		{ Quad data[]; } quads;
	layout(set=0, binding=1) buffer Indices
		{ Index data[]; } indices;
	uint fieldCount (uint w, uint h) {
		return ((w+1)/2)*((h+1)/2)+(w/2)*(h/2);
	}
	uint fieldIndex () {
		return	fieldCount(pc.width, gl_GlobalInvocationID.y) + (gl_GlobalInvocationID.x / 2);
	}
	void main() {
		float x1 = float(gl_GlobalInvocationID.x) / float(pc.width);
		float y1 = float(gl_GlobalInvocationID.y) / float(pc.height);
		float x2 = (float(gl_GlobalInvocationID.x) + 1.0) / float(pc.width);
		float y2 = (float(gl_GlobalInvocationID.y) + 1.0) / float(pc.height);

		float xx1 = x1 * 2.0 - 1.0;
		float yy1 = y1 * 2.0 - 1.0;
		float xx2 = x2 * 2.0 - 1.0;
		float yy2 = y2 * 2.0 - 1.0;

		if (((gl_GlobalInvocationID.x + gl_GlobalInvocationID.y) % 2) == 0)
		{
			uint at = fieldIndex();

			if (gl_GlobalInvocationID.z == 0)
			{
				quads.data[at].c[0] = vec2(xx1, yy1);
				quads.data[at].c[1] = vec2(xx2, yy1);
				indices.data[at*6+0].k = at * 4 + 0;
				indices.data[at*6+1].k = at * 4 + 1;
				indices.data[at*6+2].k = at * 4 + 2;
			}
			else
			{
				quads.data[at].c[2] = vec2(xx2, yy2);
				quads.data[at].c[3] = vec2(xx1, yy2);
				indices.data[at*6+3].k = at * 4 + 2;
				indices.data[at*6+4].k = at * 4 + 3;
				indices.data[at*6+5].k = at * 4 + 0;
			}
		}
	}
	)glsl");

	const tcu::StringTemplate consumerComp(R"glsl(
	#version 450
	layout(std430, push_constant) uniform PC
		{ uint width, height; } pc;
	struct Pixel { uint k; };
	layout(${IMAGE_FORMAT}, set=0, binding=0) readonly uniform ${IMAGE_TYPE} srcImage;
	layout(binding=1) writeonly coherent buffer Pixels { Pixel data[]; } pixels;
	void main()
	{
		ivec2 pos2 = ivec2(gl_GlobalInvocationID.xy);
		int   pos1 = int(gl_GlobalInvocationID.y * pc.width + gl_GlobalInvocationID.x);
		pixels.data[pos1].k = uint(imageLoad(srcImage, pos2).r) + 1;
	}
	)glsl");

	const std::string vert(R"glsl(
	#version 450
	layout(location = 0) in vec2 pos;
	void main()
	{
	   gl_Position = vec4(pos, 0, 1);
	}
	)glsl");

	const tcu::StringTemplate frag(R"glsl(
	#version 450
	layout(location = 0) out ${COLOR_TYPE} color;
	void main()
	{
	   color = ${COLOR_TYPE}(${TEST_VALUE},0,0,1);
	}
	)glsl");

	const auto format		= mapVkFormat(m_config.format);
	const auto imageFormat	= image::getShaderImageFormatQualifier(format);
	const auto imageType	= image::getShaderImageType(format, image::ImageType::IMAGE_TYPE_2D, false);
	const auto imageBuffer	= getShaderImageBufferType(format);
	const auto colorType	= image::getGlslAttachmentType(m_config.format); // ivec4

	const std::map<std::string, std::string>	abbreviations
	{
		{ std::string("TEST_VALUE"),	std::to_string(testValue)	},
		{ std::string("IMAGE_FORMAT"),	std::string(imageFormat)	},
		{ std::string("IMAGE_TYPE"),	std::string(imageType)		},
		{ std::string("IMAGE_BUFFER"),	std::string(imageBuffer)	},
		{ std::string("COLOR_TYPE"),	std::string(colorType)		},
	};

	programs.glslSources.add("comp") << glu::ComputeSource(
											m_config.transitionFrom == VK_QUEUE_COMPUTE_BIT
												? producerComp
												: consumerComp.specialize(abbreviations));
	programs.glslSources.add("vert") << glu::VertexSource(vert);
	programs.glslSources.add("frag") << glu::FragmentSource(frag.specialize(abbreviations));
}


tcu::TestStatus	GPQInstance<VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT>::iterate (void)
{
	VkResult						deviceStatus		= VK_SUCCESS;
	if (!m_device.isValid(deviceStatus))
	{
		if (VK_ERROR_NOT_PERMITTED_KHR == deviceStatus)
			return tcu::TestStatus::pass(getResultName(deviceStatus));
		TCU_THROW(NotSupportedError, "Unable to create device.");
	}

	const InstanceInterface&		vki					= m_context.getInstanceInterface();
	const DeviceInterface&			vkd					= m_context.getDeviceInterface();
	const VkPhysicalDevice			phys				= m_context.getPhysicalDevice();
	const VkDevice					device				= m_device.device;
	Allocator&						allocator			= m_device.getAllocator();
	const deUint32					producerIndex		= m_device.queueFamilyIndexFrom;
	const deUint32					consumerIndex		= m_device.queueFamilyIndexTo;
	const VkQueue					producerQueue		= m_device.queueFrom;	DE_UNREF(producerQueue);
	const VkQueue					consumerQueue		= m_device.queueTo;		DE_UNREF(consumerQueue);

	const uint32_t					width				= m_config.width;
	const uint32_t					height				= m_config.height;
	const uint32_t					clearComp			= 97;
	const VkClearValue				clearColor			= makeClearValueColorU32(clearComp, clearComp, clearComp, clearComp);
	const uint32_t					quadCount			= CheckerboardBuilder::blackFieldCount(width, height);
	const uint32_t					vertexCount			= 4 * quadCount;
	const uint32_t					indexCount			= 6 * quadCount;
	const MemoryRequirement			memReqs				= (m_config.enableProtected ? MemoryRequirement::Protected : MemoryRequirement::Any);

	VkBufferCreateFlags				buffsCreateFlags	= 0;
	if (m_config.enableProtected)	buffsCreateFlags	|= VK_BUFFER_CREATE_PROTECTED_BIT;
	if (m_config.enableSparseBinding) buffsCreateFlags	|= VK_BUFFER_CREATE_SPARSE_BINDING_BIT;
	const VkBufferUsageFlags		vertBuffUsage		= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	const VkBufferUsageFlags		indexBuffUsage		= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	const VkDeviceSize				vertBuffSize		= vertexCount * mapVkFormat(VK_FORMAT_R32G32_SFLOAT).getPixelSize();
	const VkDeviceSize				indexBuffSize		= indexCount * sizeof(uint32_t);
	const VkDeviceSize				resultBuffSize		= (width * height * mapVkFormat(m_config.format).getPixelSize());
	const VkBufferCreateInfo		vertBuffInfo		= makeBufferCreateInfo(vertBuffSize, vertBuffUsage, {producerIndex}, buffsCreateFlags);
	const VkBufferCreateInfo		indexBuffInfo		= makeBufferCreateInfo(indexBuffSize, indexBuffUsage, {producerIndex}, buffsCreateFlags);
	const VkBufferCreateInfo		resultBuffInfo		= makeBufferCreateInfo(resultBuffSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	BufferWithMemory				vertexBuffer		(vki, vkd, phys, device, allocator, vertBuffInfo, memReqs, producerQueue);
	BufferWithMemory				indexBuffer			(vki, vkd, phys, device, allocator, indexBuffInfo, memReqs, producerQueue);
	BufferWithMemory				resultBuffer		(vki, vkd, phys, device, allocator, resultBuffInfo, MemoryRequirement::HostVisible);

	const VkDescriptorBufferInfo	dsVertInfo			= makeDescriptorBufferInfo(vertexBuffer.get(), 0ull, vertBuffSize);
	const VkDescriptorBufferInfo	dsIndexInfo			= makeDescriptorBufferInfo(indexBuffer.get(), 0ull, indexBuffSize);
	Move<VkDescriptorPool>			dsPool				= DescriptorPoolBuilder()
															.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
															.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
															.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	Move<VkDescriptorSetLayout>		dsLayout			= DescriptorSetLayoutBuilder()
															.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL)
															.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL)
															.build(vkd, device);
	Move<VkDescriptorSet>			descriptorSet		= makeDescriptorSet(vkd, device, *dsPool, *dsLayout);
	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &dsVertInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &dsIndexInfo)
		.update(vkd, device);

	VkImageSubresourceRange			colorResourceRange	{};
	const VkImageUsageFlags			imageUsage			= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	de::MovePtr<ImageWithMemory>	image				= this->createImage(imageUsage, consumerIndex, consumerQueue);
	Move<VkImageView>				view				= createView(**image, colorResourceRange);
	Move<VkRenderPass>				renderPass			= makeRenderPass(vkd, device, m_config.format);
	Move<VkFramebuffer>				framebuffer			= makeFramebuffer(vkd, device, *renderPass, *view, width, height);
	const VkImageMemoryBarrier		colorReadyBarrier	= makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
															VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
															**image, colorResourceRange);
	const VkBufferImageCopy			colorCopyRegion		= makeBufferImageCopy(makeExtent3D(width, height, 1),
																			  makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
	const VkMemoryBarrier			resultReadyBarrier	= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	struct PushConstant
	{ uint32_t width, height; }		pc					{ width, height };
	Move<VkPipelineLayout>			pipelineLayout		= createPipelineLayout<PushConstant>({ *dsLayout });
	Move<VkPipeline>				producerPipeline	= createComputePipeline(*pipelineLayout);
	Move<VkPipeline>				consumerPipeline	= createGraphicsPipeline(*pipelineLayout, *renderPass);

	Move<VkCommandPool>				producerPool		= makeCommandPool(vkd, device, producerIndex);
	Move<VkCommandPool>				consumerPool		= makeCommandPool(vkd, device, consumerIndex);
	Move<VkCommandBuffer>			producerCmd			= allocateCommandBuffer(vkd, device, *producerPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	Move<VkCommandBuffer>			consumerCmd			= allocateCommandBuffer(vkd, device, *consumerPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	beginCommandBuffer(vkd, *producerCmd);
		vkd.cmdBindDescriptorSets(*producerCmd, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1, &(*descriptorSet), 0, nullptr);
		vkd.cmdBindPipeline(*producerCmd, VK_PIPELINE_BIND_POINT_COMPUTE, *producerPipeline);
		vkd.cmdPushConstants(*producerCmd, *pipelineLayout, VK_SHADER_STAGE_ALL, 0, static_cast<uint32_t>(sizeof(PushConstant)), &pc);
		vkd.cmdDispatch(*producerCmd, width, height, 2);
	endCommandBuffer(vkd, *producerCmd);

	beginCommandBuffer(vkd, *consumerCmd);
		vkd.cmdBindPipeline(*consumerCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *consumerPipeline);
		vkd.cmdBindIndexBuffer(*consumerCmd, *indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkd.cmdBindVertexBuffers(*consumerCmd, 0, 1, &static_cast<const VkBuffer&>(*vertexBuffer), &static_cast<const VkDeviceSize&>(0));
		beginRenderPass(vkd, *consumerCmd, *renderPass, *framebuffer, makeRect2D(width, height), clearColor);
			vkd.cmdDrawIndexed(*consumerCmd, indexCount, 1, 0, 0, 0);
		endRenderPass(vkd, *consumerCmd);
		vkd.cmdPipelineBarrier(*consumerCmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &colorReadyBarrier);
		vkd.cmdCopyImageToBuffer(*consumerCmd, **image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *resultBuffer, 1u, &colorCopyRegion);
		vkd.cmdPipelineBarrier(*consumerCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &resultReadyBarrier, 0u, nullptr, 0u, nullptr);
	endCommandBuffer(vkd, *consumerCmd);

	submitCommands(*producerCmd, *consumerCmd);

	resultBuffer.invalidateAlloc(vkd, device);
	const tcu::ConstPixelBufferAccess	resultBufferAccess(mapVkFormat(m_config.format), width, height, 1, resultBuffer.getHostPtr());

	return verify(resultBufferAccess, GPQCase::testValue, clearComp) ? tcu::TestStatus::pass("") : tcu::TestStatus::fail("");
}

tcu::TestStatus	GPQInstance<VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT>::iterate (void)
{
	VkResult						deviceStatus		= VK_SUCCESS;
	if (!m_device.isValid(deviceStatus))
	{
		if (VK_ERROR_NOT_PERMITTED_KHR == deviceStatus)
			return tcu::TestStatus::pass(getResultName(deviceStatus));
		TCU_THROW(NotSupportedError, "Unable to create device.");
	}

	const InstanceInterface&		vki					= m_context.getInstanceInterface();
	const DeviceInterface&			vkd					= m_context.getDeviceInterface();
	const VkPhysicalDevice			phys				= m_context.getPhysicalDevice();
	const VkDevice					device				= m_device.device;
	Allocator&						allocator			= m_device.getAllocator();
	const deUint32					producerIndex		= m_device.queueFamilyIndexFrom;
	const deUint32					consumerIndex		= m_device.queueFamilyIndexTo;
	const VkQueue					producerQueue		= m_device.queueFrom;	DE_UNREF(producerQueue);
	const VkQueue					consumerQueue		= m_device.queueTo;		DE_UNREF(consumerQueue);

	const uint32_t					width				= m_config.width;
	const uint32_t					height				= m_config.height;
	const uint32_t					clearComp			= GPQCase::testValue - 11;
	const VkClearValue				clearColor			= makeClearValueColorU32(clearComp, clearComp, clearComp, clearComp);
	const uint32_t					quadCount			= CheckerboardBuilder::blackFieldCount(width, height);
	const uint32_t					vertexCount			= 4 * quadCount;
	const uint32_t					indexCount			= 6 * quadCount;

	const VkBufferCreateFlags		graphCreateFlags	= 0;
	const MemoryRequirement			graphBuffsMemReqs	= (MemoryRequirement::HostVisible);

	const VkBufferUsageFlags		vertBuffUsage		= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	const VkDeviceSize				vertBuffSize		= vertexCount * mapVkFormat(VK_FORMAT_R32G32_SFLOAT).getPixelSize();
	const VkBufferCreateInfo		vertBuffInfo		= makeBufferCreateInfo(vertBuffSize, vertBuffUsage, {producerIndex}, graphCreateFlags);
	BufferWithMemory				vertexBuffer		(vki, vkd, phys, device, allocator, vertBuffInfo, graphBuffsMemReqs, producerQueue);

	const VkBufferUsageFlags		indexBuffUsage		= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	const VkDeviceSize				indexBuffSize		= indexCount * sizeof(uint32_t);
	const VkBufferCreateInfo		indexBuffInfo		= makeBufferCreateInfo(indexBuffSize, indexBuffUsage, {producerIndex}, graphCreateFlags);
	BufferWithMemory				indexBuffer			(vki, vkd, phys, device, allocator, indexBuffInfo, graphBuffsMemReqs, producerQueue);

	VkImageSubresourceRange			producerResRange	{};
	const VkImageUsageFlags			producerUsage		= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	de::MovePtr<ImageWithMemory>	producerImage		= this->createImage(producerUsage, producerIndex, producerQueue);
	Move<VkImageView>				producerView		= createView(**producerImage, producerResRange);
	const VkDescriptorImageInfo		producerImageInfo	= makeDescriptorImageInfo(VkSampler(0), *producerView, VK_IMAGE_LAYOUT_GENERAL);

	const VkBufferCreateFlags		consumerCreateFlags	= graphCreateFlags;
	const MemoryRequirement			consumerMemReqs		= MemoryRequirement::HostVisible | MemoryRequirement::Coherent;
	const VkBufferUsageFlags		consumerUsage		= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	const VkDeviceSize				consumerBuffSize	= (width * height * sizeof(uint32_t));
	const VkBufferCreateInfo		consumerBuffInfo	= makeBufferCreateInfo(consumerBuffSize, consumerUsage, {consumerIndex}, consumerCreateFlags);
	BufferWithMemory				consumerBuffer		(vki, vkd, phys, device, allocator, consumerBuffInfo, consumerMemReqs, consumerQueue);
	const VkDescriptorBufferInfo	consumerInfo		= makeDescriptorBufferInfo(*consumerBuffer, 0, consumerBuffSize);

	const VkBufferCreateFlags		tmpCreateFlags		= graphCreateFlags;
	const MemoryRequirement			tmpMemReqs			= MemoryRequirement::HostVisible | MemoryRequirement::Coherent;
	const VkBufferUsageFlags		tmpUsage			= (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const VkDeviceSize				tmpBuffSize			= (width * height * sizeof(uint32_t));
	const VkBufferCreateInfo		tmpBuffInfo			= makeBufferCreateInfo(tmpBuffSize, tmpUsage, {consumerIndex}, tmpCreateFlags);
	BufferWithMemory				tmpBuffer			(vki, vkd, phys, device, allocator, tmpBuffInfo, tmpMemReqs, consumerQueue);
	const VkBufferImageCopy			tmpCopyRegion		= makeBufferImageCopy(makeExtent3D(width, height, 1),
																			  makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));

	Move<VkDescriptorPool>			dsPool				= DescriptorPoolBuilder()
															.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
															.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
															.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	Move<VkDescriptorSetLayout>		dsLayout			= DescriptorSetLayoutBuilder()
															.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL)
															.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL)
															.build(vkd, device);
	Move<VkDescriptorSet>			descriptorSet		= makeDescriptorSet(vkd, device, *dsPool, *dsLayout);

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &producerImageInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &consumerInfo)
		.update(vkd, device);

	Move<VkRenderPass>				renderPass			= makeRenderPass(vkd, device, m_config.format);
	Move<VkFramebuffer>				framebuffer			= makeFramebuffer(vkd, device, *renderPass, *producerView, width, height);

	const VkImageMemoryBarrier		producerReadyBarrier= makeImageMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_NONE,
															VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
															**producerImage, producerResRange);
//	const VkBufferMemoryBarrier		consumerReadyBarrier= makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_NONE,
//																				  *consumerBuffer, 0, consumerBuffSize,
//																				  consumerIndex, consumerIndex);
	//const VkMemoryBarrier			tmpReadyBarrier		= makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
//	const VkBufferMemoryBarrier		tmpReadyBarrier		= makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_NONE,
//																				  *tmpBuffer, 0, tmpBuffSize,
//																				  consumerIndex, consumerIndex);
	const VkBufferMemoryBarrier		consumerBarriers[]	{	makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_NONE,
																					*consumerBuffer, 0, consumerBuffSize,
																					consumerIndex, consumerIndex),
															makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_NONE,
																					*tmpBuffer, 0, tmpBuffSize,
																					consumerIndex, consumerIndex) };
	struct PushConstant
	{ uint32_t width, height; }		pc					{ width, height };
	Move<VkPipelineLayout>			pipelineLayout		= createPipelineLayout<PushConstant>({ *dsLayout });
	Move<VkPipeline>				producerPipeline	= createGraphicsPipeline(*pipelineLayout, *renderPass);
	Move<VkPipeline>				consumerPipeline	= createComputePipeline(*pipelineLayout);

	Move<VkCommandPool>				producerPool		= makeCommandPool(vkd, device, producerIndex);
	Move<VkCommandPool>				consumerPool		= makeCommandPool(vkd, device, consumerIndex);
	Move<VkCommandBuffer>			producerCmd			= allocateCommandBuffer(vkd, device, *producerPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	Move<VkCommandBuffer>			consumerCmd			= allocateCommandBuffer(vkd, device, *consumerPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	{
		std::vector<float>		vertices;
		std::vector<uint32_t>	indices;
		CheckerboardBuilder	builder(width, height);
		builder.buildVerticesAndIndices(vertices, indices);
		DE_ASSERT(vertices.size() == (vertexCount * 2));
		DE_ASSERT(indices.size() == indexCount);
		std::copy(vertices.begin(), vertices.end(), begin<float>(vertexBuffer.getHostPtr()));
		std::copy(indices.begin(), indices.end(), begin<uint32_t>(indexBuffer.getHostPtr()));
		vertexBuffer.flushAlloc(vkd, device);
		indexBuffer.flushAlloc(vkd, device);
	}

	beginCommandBuffer(vkd, *producerCmd);
		vkd.cmdBindPipeline(*producerCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *producerPipeline);
		vkd.cmdBindVertexBuffers(*producerCmd, 0, 1, &static_cast<const VkBuffer&>(*vertexBuffer), &static_cast<const VkDeviceSize&>(0));
		vkd.cmdBindIndexBuffer(*producerCmd, *indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		beginRenderPass(vkd, *producerCmd, *renderPass, *framebuffer, makeRect2D(width, height), clearColor);
			vkd.cmdDrawIndexed(*producerCmd, indexCount, 1, 0, 0, 0);
		endRenderPass(vkd, *producerCmd);
		vkd.cmdPipelineBarrier(*producerCmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VkDependencyFlags(0),
							   0u, nullptr, 0u, nullptr, 1u, &producerReadyBarrier);
	endCommandBuffer(vkd, *producerCmd);

	beginCommandBuffer(vkd, *consumerCmd);
		vkd.cmdBindPipeline(*consumerCmd, VK_PIPELINE_BIND_POINT_COMPUTE, *consumerPipeline);
		vkd.cmdBindDescriptorSets(*consumerCmd, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1, &descriptorSet.get(), 0, nullptr);
		vkd.cmdCopyImageToBuffer(*consumerCmd, **producerImage, VK_IMAGE_LAYOUT_GENERAL, *tmpBuffer, 1u, &tmpCopyRegion);
		vkd.cmdPushConstants(*consumerCmd, *pipelineLayout, VK_SHADER_STAGE_ALL, 0, static_cast<uint32_t>(sizeof(PushConstant)), &pc);
		vkd.cmdDispatch(*consumerCmd, width, height, 1);
		vkd.cmdPipelineBarrier(*consumerCmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkDependencyFlags(0),
							   0, nullptr, DE_LENGTH_OF_ARRAY(consumerBarriers), consumerBarriers, 0, nullptr);
	endCommandBuffer(vkd, *consumerCmd);

	submitCommands(*producerCmd, *consumerCmd);

	tmpBuffer.invalidateAlloc(vkd, device);
	const tcu::ConstPixelBufferAccess	tmpBufferAccess(mapVkFormat(m_config.format), width, height, 1, tmpBuffer.getHostPtr());
	const bool tmpRes = verify(tmpBufferAccess, GPQCase::testValue, clearComp);

	consumerBuffer.invalidateAlloc(vkd, device);
	const tcu::ConstPixelBufferAccess	resultBufferAccess(mapVkFormat(m_config.format), width, height, 1, consumerBuffer.getHostPtr());

	return (tmpRes && verify(resultBufferAccess, (GPQCase::testValue + 1), (clearComp + 1))) ? tcu::TestStatus::pass("") : tcu::TestStatus::fail("");
}

bool GPQInstanceBase::verify (const BufferAccess& result, deUint32 blackColor, deUint32 whiteColor) const
{
	bool ok = true;

	for (deUint32 y = 0; ok && y < m_config.height; ++y)
	{
		for (deUint32 x = 0; ok && x < m_config.width; ++x)
		{
			const deUint32 color = result.getPixelT<deUint32>(x, y).x();
			if (((x + y) % 2) == 0)
			{
				ok = color == blackColor;
			}
			else
			{
				ok = color == whiteColor;
			}
		}
	}
	return ok;
}

} // anonymous

tcu::TestCaseGroup* createGlobalPriorityQueueTests (tcu::TestContext& testCtx)
{
	typedef std::pair<VkQueueFlagBits, const char*> TransitionItem;
	TransitionItem const transitions[]
	{
		{ VK_QUEUE_GRAPHICS_BIT,	"graphics"	},
		{ VK_QUEUE_COMPUTE_BIT,		"compute"	},
	};

	auto mkGroupName = [](const TransitionItem& from, const TransitionItem& to) -> std::string
	{
		return std::string("from_") + from.second + std::string("_to_") + to.second;
	};

	std::pair<VkQueueFlags, const char*>
			const modifiers[]
	{
		{ 0,													"no_modifiers"		},
		{ VK_QUEUE_SPARSE_BINDING_BIT,							"sparse"			},
		{ VK_QUEUE_PROTECTED_BIT,								"protected"			},
		{ VK_QUEUE_SPARSE_BINDING_BIT|VK_QUEUE_PROTECTED_BIT,	"sparse_protected"	},
	};

	std::pair<VkQueueGlobalPriorityKHR, const char*>
			const prios[]
	{
		{ VK_QUEUE_GLOBAL_PRIORITY_LOW_KHR,			"low"		},
		{ VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_KHR,		"medium"	},
		{ VK_QUEUE_GLOBAL_PRIORITY_HIGH_KHR,		"high"		},
		{ VK_QUEUE_GLOBAL_PRIORITY_REALTIME_KHR,	"realtime"	},
	};

	std::pair<SyncType, const char*>
			const syncs[]
	{
		{ SyncType::None,		"no_sync"	},
		{ SyncType::Semaphore,	"semaphore"	},
	};

	const uint32_t	dim0	= 34;
	const uint32_t	dim1	= 25;
	bool			swap	= true;

	auto rootGroup = new tcu::TestCaseGroup(testCtx, "global_priority_transition", "Global Priority Queue Tests");

	for (const auto& prio : prios)
	{
		auto prioGroup = new tcu::TestCaseGroup(testCtx, prio.second, "");

		for (const auto& sync : syncs)
		{
			auto syncGroup = new tcu::TestCaseGroup(testCtx, sync.second, "");

			for (const auto& mod : modifiers)
			{
				auto modGroup = new tcu::TestCaseGroup(testCtx, mod.second, "");

				for (const auto& transitionFrom : transitions)
				{
					for (const auto& transitionTo : transitions)
					{
						if (transitionFrom != transitionTo)
						{
							TestConfig	cfg{};
							cfg.transitionFrom		= transitionFrom.first;
							cfg.transitionTo		= transitionTo.first;
							cfg.priorityFrom		= prio.first;
							cfg.priorityTo			= prio.first;
							cfg.syncType			= sync.first;
							cfg.enableProtected		= (mod.first & VK_QUEUE_PROTECTED_BIT) != 0;
							cfg.enableSparseBinding	= (mod.first & VK_QUEUE_SPARSE_BINDING_BIT) != 0;
							// Note that format is changing in GPQCase::checkSupport(...)
							cfg.format				= VK_FORMAT_R32G32B32A32_SFLOAT;
							cfg.width				= swap ? dim0 : dim1;
							cfg.height				= swap ? dim1 : dim0;

							swap ^= true;

							modGroup->addChild(new GPQCase(testCtx, mkGroupName(transitionFrom, transitionTo), cfg));
						}
					}
				}
				syncGroup->addChild(modGroup);
			}
			prioGroup->addChild(syncGroup);
		}
		rootGroup->addChild(prioGroup);
	}

	return rootGroup;
}

} // synchronization
} // vkt
