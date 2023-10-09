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
#include <iostream>

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
	auto			createPipelineLayout	(DSLayouts					setLayouts)		const -> Move<VkPipelineLayout>;
	auto			makeCommandPool			(deUint32					qFamilyIndex)	const -> Move<VkCommandPool>;
	auto			createGraphicsPipeline	(VkPipelineLayout			pipelineLayout,
											 VkRenderPass				renderPass)			  -> Move<VkPipeline>;
	auto			createComputePipeline	(VkPipelineLayout			pipelineLayout,
											 bool						producer)			  -> Move<VkPipeline>;
	auto			createImage				(VkImageUsageFlags			usage,
											 deUint32					queueFamilyIdx,
											 VkQueue					queue)			const -> de::MovePtr<ImageWithMemory>;
	auto			createView				(VkImage					image,
											 VkImageSubresourceRange&	range)			const -> Move<VkImageView>;
	void			submitCommands			(VkCommandBuffer			producerCmd,
											 VkCommandBuffer			consumerCmd)	const;
protected:
	auto createPipelineLayout			(const VkPushConstantRange*		pRange,
										 DSLayouts						setLayouts)		const -> Move<VkPipelineLayout>;
	const TestConfig			m_config;
	const SpecialDevice			m_device;
	struct NamedShader {
		std::string				name;
		Move<VkShaderModule>	handle;
	}							m_shaders[4];
};
GPQInstanceBase::GPQInstanceBase (Context& ctx, const TestConfig& cfg)
	: TestInstance	(ctx)
	, m_config		(cfg)
	, m_device		(ctx,
					 cfg.transitionFrom,	cfg.transitionTo,
					 cfg.priorityFrom,		cfg.priorityTo,
					 cfg.enableProtected,	cfg.enableSparseBinding)
	, m_shaders		()
{
	m_shaders[0].name = "vert";	// vertex
	m_shaders[1].name = "frag";	// fragment
	m_shaders[2].name = "cpyb";	// compute
	m_shaders[3].name = "cpyi";	// compute
}

de::MovePtr<ImageWithMemory> GPQInstanceBase::createImage (VkImageUsageFlags usage, deUint32 queueFamilyIdx, VkQueue queue) const
{
	const InstanceInterface&	vki		= m_context.getInstanceInterface();
	const DeviceInterface&		vkd		= m_context.getDeviceInterface();
	const VkPhysicalDevice		phys	= m_context.getPhysicalDevice();
	const VkDevice				dev		= m_device.handle;
	Allocator&					alloc	= m_device.getAllocator();
	VkImageCreateFlags			flags	= 0;

	if (m_config.enableProtected)		flags	|= VK_IMAGE_CREATE_PROTECTED_BIT;
	if (m_config.enableSparseBinding)	flags	|= (VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT);
	const MemoryRequirement				memReqs	= m_config.enableProtected ? MemoryRequirement::Protected : MemoryRequirement::Any;

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

	return de::MovePtr<ImageWithMemory>(new ImageWithMemory(vki, vkd, phys, dev, alloc, imageInfo, queue, memReqs));
}

Move<VkImageView> GPQInstanceBase::createView (VkImage image, VkImageSubresourceRange& range) const
{
	const DeviceInterface&	vkd		= m_context.getDeviceInterface();
	const VkDevice			dev		= m_device.handle;

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
	info.pushConstantRangeCount	= (pRange != nullptr && pRange->size > 0) ? 1 : 0;
	info.pPushConstantRanges	= (pRange != nullptr && pRange->size > 0) ? pRange : nullptr;

	return ::vk::createPipelineLayout(m_context.getDeviceInterface(), m_device.handle, &info);
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

Move<VkCommandPool> GPQInstanceBase::makeCommandPool (deUint32 qFamilyIndex) const
{
	const VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
			| (m_config.enableProtected ? VK_COMMAND_POOL_CREATE_PROTECTED_BIT : 0);
	const VkCommandPoolCreateInfo commandPoolParams =
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,		// VkStructureType			sType;
		DE_NULL,										// const void*				pNext;
		flags,											// VkCommandPoolCreateFlags	flags;
		qFamilyIndex,									// deUint32					queueFamilyIndex;
	};

	return createCommandPool(m_context.getDeviceInterface(), m_device.handle, &commandPoolParams);
}

Move<VkPipeline> GPQInstanceBase::createGraphicsPipeline (VkPipelineLayout pipelineLayout, VkRenderPass renderPass)
{
	const DeviceInterface&	vkd	= m_context.getDeviceInterface();
	const VkDevice			dev	= m_device.handle;

	auto sh = std::find_if(std::begin(m_shaders), std::end(m_shaders), [](const NamedShader& ns){ return ns.name == "vert"; });
	if (*sh->handle == DE_NULL) sh->handle = createShaderModule(vkd, dev, m_context.getBinaryCollection().get("vert"));
	VkShaderModule vertex = *sh->handle;

	sh = std::find_if(std::begin(m_shaders), std::end(m_shaders), [](const NamedShader& ns){ return ns.name == "frag"; });
	if (*sh->handle == DE_NULL) sh->handle = createShaderModule(vkd, dev, m_context.getBinaryCollection().get("frag"));
	VkShaderModule fragment = *sh->handle;

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

	return makeGraphicsPipeline(vkd, dev, pipelineLayout,vertex,VkShaderModule(0),
						 VkShaderModule(0),VkShaderModule(0),fragment,renderPass,viewports, scissors,
						 VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,0,0,&vertexInputStateCreateInfo);
}

Move<VkPipeline> GPQInstanceBase::createComputePipeline	(VkPipelineLayout pipelineLayout, bool producer)
{
	const DeviceInterface&	vk	= m_context.getDeviceInterface();
	const VkDevice			dev	= m_device.handle;

	const std::string		compName = producer ? "cpyb" : "cpyi";
	auto comp = std::find_if(std::begin(m_shaders), std::end(m_shaders), [&](const NamedShader& ns){ return ns.name == compName; });
	if (*comp->handle == DE_NULL) comp->handle = createShaderModule(vk, dev, m_context.getBinaryCollection().get(compName));
	VkShaderModule compute = *comp->handle;

	VkPipelineShaderStageCreateInfo	sci{};
	sci.sType				= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	sci.pNext				= nullptr;
	sci.flags				= VkPipelineShaderStageCreateFlags(0);
	sci.stage				= VK_SHADER_STAGE_COMPUTE_BIT;
	sci.module				= compute;
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
	const VkDevice			dev		= m_device.handle;

	Move<VkSemaphore>		sem		= createSemaphore(vkd, dev);
	Move<VkFence>			fence	= createFence(vkd, dev);

	VkProtectedSubmitInfo protectedSubmitInfo
	{
		VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO,	// VkStructureType	sType;
		nullptr,									// void*			pNext;
		VK_TRUE										// VkBool32			protectedSubmit;
	};

	const VkSubmitInfo		producerSubmitInfo
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,				// VkStructureType				sType;
		m_config.enableProtected
				? &protectedSubmitInfo : nullptr,	// const void*					pNext;
		0,											// deUint32						waitSemaphoreCount;
		nullptr,									// const VkSemaphore*			pWaitSemaphores;
		nullptr,									// const VkPipelineStageFlags*	pWaitDstStageMask;
		1u,											// deUint32						commandBufferCount;
		&producerCmd,								// const VkCommandBuffer*		pCommandBuffers;
		1u,											// deUint32						signalSemaphoreCount;
		&sem.get(),									// const VkSemaphore*			pSignalSemaphores;
	};

	const VkPipelineStageFlags	dstWaitStages = VK_PIPELINE_STAGE_TRANSFER_BIT |
												queueFlagBitToPipelineStage(m_config.transitionTo);
	const VkSubmitInfo		consumerSubmitInfo
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,				// VkStructureType				sType;
		m_config.enableProtected
				? &protectedSubmitInfo : nullptr,	// const void*					pNext;
		1u,											// deUint32						waitSemaphoreCount;
		&sem.get(),									// const VkSemaphore*			pWaitSemaphores;
		&dstWaitStages,								// const VkPipelineStageFlags*	pWaitDstStageMask;
		1u,											// deUint32						commandBufferCount;
		&consumerCmd,								// const VkCommandBuffer*		pCommandBuffers;
		0,											// deUint32						signalSemaphoreCount;
		nullptr,									// const VkSemaphore*			pSignalSemaphores;
	};

	switch (m_config.syncType)
	{
	case SyncType::None:
		submitCommandsAndWait(vkd, dev, m_device.queueFrom, producerCmd);
		submitCommandsAndWait(vkd, dev, m_device.queueTo, consumerCmd);
		break;
	case SyncType::Semaphore:
		VK_CHECK(vkd.queueSubmit(m_device.queueFrom, 1u, &producerSubmitInfo, VkFence(0)));
		VK_CHECK(vkd.queueSubmit(m_device.queueTo, 1u, &consumerSubmitInfo, *fence));
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
	context.requireDeviceFunctionality("VK_EXT_global_priority_query");
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
			buf << "Unable to find queue " << qfb << " with priority " << qgp;
			buf.flush();
			TCU_THROW(NotSupportedError, buf.str());
		}
	};

	VkQueueFlags	flagsFrom	= m_config.transitionFrom;
	VkQueueFlags	flagsTo		= m_config.transitionTo;
	if (m_config.enableProtected)
	{
		flagsFrom |= VK_QUEUE_PROTECTED_BIT;
		flagsTo	|= VK_QUEUE_PROTECTED_BIT;
	}
	if (m_config.enableSparseBinding)
	{
		flagsFrom |= VK_QUEUE_SPARSE_BINDING_BIT;
		flagsTo	|= VK_QUEUE_SPARSE_BINDING_BIT;
	}

	const deUint32 queueFromIndex	= findQueueFamilyIndex(vki, dev, m_config.priorityFrom,
														   flagsFrom, SpecialDevice::getColissionFlags(flagsFrom),
														   INVALID_UINT32);
	assertUnavailableQueue(queueFromIndex, m_config.transitionFrom, m_config.priorityFrom);

	const deUint32 queueToIndex		= findQueueFamilyIndex(vki, dev, m_config.priorityTo,
														   flagsTo, SpecialDevice::getColissionFlags(flagsTo),
														   queueFromIndex);
	assertUnavailableQueue(queueToIndex, m_config.transitionTo, m_config.priorityTo);

	if (queueFromIndex == queueToIndex)
	{
		std::ostringstream buf;
		buf << "Unable to find separate queues " << m_config.transitionFrom << " and " << m_config.transitionTo;
		buf.flush();
		TCU_THROW(NotSupportedError, buf.str());
	}
}

void GPQCase::initPrograms (SourceCollections& programs) const
{
	const std::string producerComp(R"glsl(
	#version 450
	layout(binding=0) buffer S { float src[]; };
	layout(binding=1) buffer D { float dst[]; };
	layout(local_size_x=1,local_size_y=1) in;
	void main() {
		dst[gl_GlobalInvocationID.x] = src[gl_GlobalInvocationID.x];
	}
	)glsl");

	const tcu::StringTemplate consumerComp(R"glsl(
	#version 450
	layout(local_size_x=1,local_size_y=1) in;
	layout(${IMAGE_FORMAT}, binding=0) readonly uniform ${IMAGE_TYPE} srcImage;
	layout(binding=1) writeonly coherent buffer Pixels { uint data[]; } dstBuffer;
	void main()
	{
		ivec2 srcIdx = ivec2(gl_GlobalInvocationID.xy);
		int   width  = imageSize(srcImage).x;
		int   dstIdx = int(gl_GlobalInvocationID.y * width + gl_GlobalInvocationID.x);
		dstBuffer.data[dstIdx] = uint(imageLoad(srcImage, srcIdx).r) == ${TEST_VALUE} ? 1 : 0;
	}
	)glsl");

	const std::string vert(R"glsl(
	#version 450
	layout(location = 0) in vec2 pos;
	void main()
	{
	   gl_Position = vec4(pos, 0.0, 1.01);
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
	const auto colorType	= image::getGlslAttachmentType(m_config.format); // ivec4

	const std::map<std::string, std::string>	abbreviations
	{
		{ std::string("TEST_VALUE"),	std::to_string(testValue)	},
		{ std::string("IMAGE_FORMAT"),	std::string(imageFormat)	},
		{ std::string("IMAGE_TYPE"),	std::string(imageType)		},
		{ std::string("COLOR_TYPE"),	std::string(colorType)		},
	};

	programs.glslSources.add("cpyb") << glu::ComputeSource(producerComp);
	programs.glslSources.add("cpyi") << glu::ComputeSource(consumerComp.specialize(abbreviations));
	programs.glslSources.add("vert") << glu::VertexSource(vert);
	programs.glslSources.add("frag") << glu::FragmentSource(frag.specialize(abbreviations));
}

tcu::TestStatus	GPQInstance<VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT>::iterate (void)
{
	if (VK_SUCCESS != m_device.createResult)
	{
		if (VK_ERROR_NOT_PERMITTED_KHR == m_device.createResult)
			return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Custom device creation returned " + std::string(getResultName(m_device.createResult)));
		throw NotSupportedError(m_device.createResult, getResultName(m_device.createResult), m_device.createExpression, m_device.createFileName, m_device.createFileLine);
	}

	const InstanceInterface&		vki					= m_context.getInstanceInterface();
	const DeviceInterface&			vkd					= m_context.getDeviceInterface();
	const VkPhysicalDevice			phys				= m_context.getPhysicalDevice();
	const VkDevice					device				= m_device.handle;
	Allocator&						allocator			= m_device.getAllocator();
	const deUint32					producerIndex		= m_device.queueFamilyIndexFrom;
	const deUint32					consumerIndex		= m_device.queueFamilyIndexTo;
	const std::vector<deUint32>		producerIndices		{ producerIndex };
	const std::vector<deUint32>		consumerIndices		{ consumerIndex };
	const VkQueue					producerQueue		= m_device.queueFrom;
	const VkQueue					consumerQueue		= m_device.queueTo;

	// stagging buffer for vertices
	const std::vector<float>		positions			{ +1.f, -1.f, -1.f, -1.f, 0.f, +1.f };
	const VkBufferCreateInfo		posBuffInfo			= makeBufferCreateInfo(positions.size() * sizeof(float), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, producerIndices);
	BufferWithMemory				positionsBuffer		(vki, vkd, phys, device, allocator, posBuffInfo, MemoryRequirement::HostVisible);
	std::copy_n(positions.data(), positions.size(), begin<float>(positionsBuffer.getHostPtr()));
	const VkDescriptorBufferInfo	posDsBuffInfo		= makeDescriptorBufferInfo(positionsBuffer.get(), 0, positionsBuffer.getSize());

	// vertex buffer
	VkBufferCreateFlags				vertCreateFlags		= 0;
	if (m_config.enableProtected)	vertCreateFlags		|= VK_BUFFER_CREATE_PROTECTED_BIT;
	if (m_config.enableSparseBinding) vertCreateFlags	|= VK_BUFFER_CREATE_SPARSE_BINDING_BIT;
	const VkBufferUsageFlags		vertBuffUsage		= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	const MemoryRequirement			vertMemReqs			= (m_config.enableProtected ? MemoryRequirement::Protected : MemoryRequirement::Any);
	const VkBufferCreateInfo		vertBuffInfo		= makeBufferCreateInfo(positionsBuffer.getSize(), vertBuffUsage, producerIndices, vertCreateFlags);
	const BufferWithMemory			vertexBuffer		(vki, vkd, phys, device, allocator, vertBuffInfo, vertMemReqs, producerQueue);
	const VkDescriptorBufferInfo	vertDsBuffInfo		= makeDescriptorBufferInfo(vertexBuffer.get(), 0ull, vertexBuffer.getSize());

	// descriptor set for stagging and vertex buffers
	Move<VkDescriptorPool>			producerDsPool		= DescriptorPoolBuilder()
															.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
															.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
															.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	Move<VkDescriptorSetLayout>		producerDsLayout	= DescriptorSetLayoutBuilder()
															.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL)
															.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL)
															.build(vkd, device);
	Move<VkDescriptorSet>			producerDs			= makeDescriptorSet(vkd, device, *producerDsPool, *producerDsLayout);
	DescriptorSetUpdateBuilder()
		.writeSingle(*producerDs, DescriptorSetUpdateBuilder::Location::binding(0), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &posDsBuffInfo)
		.writeSingle(*producerDs, DescriptorSetUpdateBuilder::Location::binding(1), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &vertDsBuffInfo)
		.update(vkd, device);

	// consumer image
	const uint32_t					clearComp			= 97;
	const VkClearValue				clearColor			= makeClearValueColorU32(clearComp, clearComp, clearComp, clearComp);
	VkImageSubresourceRange			imageResourceRange	{};
	const VkImageUsageFlags			imageUsage			= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	de::MovePtr<ImageWithMemory>	image				= createImage(imageUsage, consumerIndex, consumerQueue);
	Move<VkImageView>				view				= createView(**image, imageResourceRange);
	Move<VkRenderPass>				renderPass			= makeRenderPass(vkd, device, m_config.format);
	Move<VkFramebuffer>				framebuffer			= makeFramebuffer(vkd, device, *renderPass, *view, m_config.width, m_config.height);
	const VkDescriptorImageInfo		imageDsInfo			= makeDescriptorImageInfo(VkSampler(0), *view, VK_IMAGE_LAYOUT_GENERAL);
	const VkImageMemoryBarrier		imageReadyBarrier	= makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
															VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
															**image, imageResourceRange, consumerIndex, consumerIndex);
	// stagging buffer for result
	const VkDeviceSize				resultBuffSize		= (m_config.width * m_config.height * mapVkFormat(m_config.format).getPixelSize());
	const VkBufferCreateInfo		resultBuffInfo		= makeBufferCreateInfo(resultBuffSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, consumerIndices);
	BufferWithMemory				resultBuffer		(vki, vkd, phys, device, allocator, resultBuffInfo, MemoryRequirement::HostVisible);
	const VkDescriptorBufferInfo	resultDsBuffInfo	= makeDescriptorBufferInfo(resultBuffer.get(), 0ull, resultBuffSize);
	const VkMemoryBarrier			resultReadyBarrier	= makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);

	// descriptor set for consumer image and result buffer
	Move<VkDescriptorPool>			consumerDsPool		= DescriptorPoolBuilder()
															.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
															.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
															.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	Move<VkDescriptorSetLayout>		consumerDsLayout	= DescriptorSetLayoutBuilder()
															.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL)
															.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL)
															.build(vkd, device);
	Move<VkDescriptorSet>			consumerDs			= makeDescriptorSet(vkd, device, *consumerDsPool, *consumerDsLayout);

	DescriptorSetUpdateBuilder()
		.writeSingle(*consumerDs, DescriptorSetUpdateBuilder::Location::binding(0), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageDsInfo)
		.writeSingle(*consumerDs, DescriptorSetUpdateBuilder::Location::binding(1), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultDsBuffInfo)
		.update(vkd, device);

	Move<VkPipelineLayout>			producerLayout		= createPipelineLayout<>({ *producerDsLayout });
	Move<VkPipeline>				producerPipeline	= createComputePipeline(*producerLayout, true);

	Move<VkPipelineLayout>			consumerLayout		= createPipelineLayout<>({ *consumerDsLayout });
	Move<VkPipeline>				consumerPipeline	= createGraphicsPipeline(*consumerLayout, *renderPass);

	Move<VkPipelineLayout>			resultLayout		= createPipelineLayout<>({ *consumerDsLayout });
	Move<VkCommandPool>				resultPool			= makeCommandPool(consumerIndex);
	Move<VkPipeline>				resultPipeline		= createComputePipeline(*resultLayout, false);

	Move<VkCommandPool>				producerPool		= makeCommandPool(producerIndex);
	Move<VkCommandPool>				consumerPool		= makeCommandPool(consumerIndex);
	Move<VkCommandBuffer>			producerCmd			= allocateCommandBuffer(vkd, device, *producerPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	Move<VkCommandBuffer>			consumerCmd			= allocateCommandBuffer(vkd, device, *consumerPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	beginCommandBuffer(vkd, *producerCmd);
		vkd.cmdBindPipeline(*producerCmd, VK_PIPELINE_BIND_POINT_COMPUTE, *producerPipeline);
		vkd.cmdBindDescriptorSets(*producerCmd, VK_PIPELINE_BIND_POINT_COMPUTE, *producerLayout, 0, 1, &(*producerDs), 0, nullptr);
		vkd.cmdDispatch(*producerCmd, deUint32(positions.size()), 1, 1);
	endCommandBuffer(vkd, *producerCmd);

	beginCommandBuffer(vkd, *consumerCmd);
		vkd.cmdBindPipeline(*consumerCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *consumerPipeline);
		vkd.cmdBindPipeline(*consumerCmd, VK_PIPELINE_BIND_POINT_COMPUTE, *resultPipeline);
		vkd.cmdBindDescriptorSets(*consumerCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *consumerLayout, 0, 1, &(*consumerDs), 0, nullptr);
		vkd.cmdBindDescriptorSets(*consumerCmd, VK_PIPELINE_BIND_POINT_COMPUTE, *resultLayout, 0, 1, &(*consumerDs), 0, nullptr);
		vkd.cmdBindVertexBuffers(*consumerCmd, 0, 1, vertexBuffer.getPtr(), &static_cast<const VkDeviceSize&>(0));

		beginRenderPass(vkd, *consumerCmd, *renderPass, *framebuffer, makeRect2D(m_config.width, m_config.height), clearColor);
			vkd.cmdDraw(*consumerCmd, deUint32(positions.size()), 1, 0, 0);
		endRenderPass(vkd, *consumerCmd);
		vkd.cmdPipelineBarrier(*consumerCmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u,
							   0u, nullptr, 0u, nullptr, 1u, &imageReadyBarrier);

		vkd.cmdDispatch(*consumerCmd, m_config.width, m_config.height, 1);
		vkd.cmdPipelineBarrier(*consumerCmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u,
							   1u, &resultReadyBarrier, 0u, nullptr, 0u, nullptr);
	endCommandBuffer(vkd, *consumerCmd);

	submitCommands(*producerCmd, *consumerCmd);

	resultBuffer.invalidateAlloc(vkd, device);
	const tcu::ConstPixelBufferAccess	resultBufferAccess(mapVkFormat(m_config.format), m_config.width, m_config.height, 1, resultBuffer.getHostPtr());
	const deUint32 resultValue = resultBufferAccess.getPixelUint(0, 0).x();
	const deUint32 expectedValue = 1;
	const bool ok = (resultValue == expectedValue);
	if (!ok)
	{
		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message << "Expected value: " << expectedValue << ", got " << resultValue << tcu::TestLog::EndMessage;
	}

	return ok ? tcu::TestStatus::pass("") : tcu::TestStatus::fail("");
}

tcu::TestStatus	GPQInstance<VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT>::iterate (void)
{
	if (VK_SUCCESS != m_device.createResult)
	{
		if (VK_ERROR_NOT_PERMITTED_KHR == m_device.createResult)
			return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Custom device creation returned " + std::string(getResultName(m_device.createResult)));
		throw NotSupportedError(m_device.createResult, getResultName(m_device.createResult), m_device.createExpression, m_device.createFileName, m_device.createFileLine);
	}

	const InstanceInterface&		vki					= m_context.getInstanceInterface();
	const DeviceInterface&			vkd					= m_context.getDeviceInterface();
	const VkPhysicalDevice			phys				= m_context.getPhysicalDevice();
	const VkDevice					device				= m_device.handle;
	Allocator&						allocator			= m_device.getAllocator();
	const deUint32					producerIndex		= m_device.queueFamilyIndexFrom;
	const deUint32					consumerIndex		= m_device.queueFamilyIndexTo;
	const std::vector<deUint32>		producerIndices		{ producerIndex };
	const std::vector<deUint32>		consumerIndices		{ consumerIndex };
	const VkQueue					producerQueue		= m_device.queueFrom;

	// stagging buffer for vertices
	const std::vector<float>		positions			{ +1.f, -1.f, -1.f, -1.f, 0.f, +1.f };
	const VkBufferCreateInfo		positionBuffInfo	= makeBufferCreateInfo(positions.size() * sizeof(float), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, producerIndices);
	BufferWithMemory				positionsBuffer		(vki, vkd, phys, device, allocator, positionBuffInfo, MemoryRequirement::HostVisible);
	std::copy_n(positions.data(), positions.size(), begin<float>(positionsBuffer.getHostPtr()));
	const VkDescriptorBufferInfo	posDsBuffInfo		= makeDescriptorBufferInfo(positionsBuffer.get(), 0, positionsBuffer.getSize());

	// vertex buffer
	VkBufferCreateFlags				vertCreateFlags		= 0;
	if (m_config.enableProtected)	vertCreateFlags		|= VK_BUFFER_CREATE_PROTECTED_BIT;
	if (m_config.enableSparseBinding) vertCreateFlags	|= VK_BUFFER_CREATE_SPARSE_BINDING_BIT;
	const VkBufferUsageFlags		vertBuffUsage		= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	const MemoryRequirement			vertMemReqs			= (m_config.enableProtected ? MemoryRequirement::Protected : MemoryRequirement::Any);
	const VkBufferCreateInfo		vertBuffInfo		= makeBufferCreateInfo(positionsBuffer.getSize(), vertBuffUsage, producerIndices, vertCreateFlags);
	const BufferWithMemory			vertexBuffer		(vki, vkd, phys, device, allocator, vertBuffInfo, vertMemReqs, producerQueue);
	const VkDescriptorBufferInfo	vertDsBuffInfo		= makeDescriptorBufferInfo(vertexBuffer.get(), 0ull, vertexBuffer.getSize());
	const VkBufferMemoryBarrier		producerReadyBarrier= makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
																				  vertexBuffer.get(), 0, vertexBuffer.getSize(), producerIndex, producerIndex);

	// descriptor set for stagging and vertex buffers
	Move<VkDescriptorPool>			producerDsPool		= DescriptorPoolBuilder()
															.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
															.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
															.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	Move<VkDescriptorSetLayout>		producerDsLayout	= DescriptorSetLayoutBuilder()
															.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL)
															.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL)
															.build(vkd, device);
	Move<VkDescriptorSet>			producerDs			= makeDescriptorSet(vkd, device, *producerDsPool, *producerDsLayout);
	DescriptorSetUpdateBuilder()
		.writeSingle(*producerDs, DescriptorSetUpdateBuilder::Location::binding(0), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &posDsBuffInfo)
		.writeSingle(*producerDs, DescriptorSetUpdateBuilder::Location::binding(1), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &vertDsBuffInfo)
		.update(vkd, device);

	// producer image
	const uint32_t					clearComp			= 97;
	const VkClearValue				clearColor			= makeClearValueColorU32(clearComp, clearComp, clearComp, clearComp);
	VkImageSubresourceRange			imageResourceRange	{};
	const VkImageUsageFlags			imageUsage			= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	de::MovePtr<ImageWithMemory>	image				= createImage(imageUsage, producerIndex, producerQueue);
	Move<VkImageView>				view				= createView(**image, imageResourceRange);
	Move<VkRenderPass>				renderPass			= makeRenderPass(vkd, device, m_config.format);
	Move<VkFramebuffer>				framebuffer			= makeFramebuffer(vkd, device, *renderPass, *view, m_config.width, m_config.height);
	const VkDescriptorImageInfo		imageDsInfo			= makeDescriptorImageInfo(VkSampler(0), *view, VK_IMAGE_LAYOUT_GENERAL);
	const VkImageMemoryBarrier		imageReadyBarrier	= makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
															VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
															**image, imageResourceRange, producerIndex, producerIndex);

	// stagging buffer for result
	const VkDeviceSize				resultBufferSize	= (m_config.width * m_config.height * mapVkFormat(m_config.format).getPixelSize());
	const VkBufferCreateInfo		resultBufferInfo	= makeBufferCreateInfo(resultBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, consumerIndices);
	BufferWithMemory				resultBuffer		(vki, vkd, phys, device, allocator, resultBufferInfo, MemoryRequirement::HostVisible);
	const VkDescriptorBufferInfo	resultDsBuffInfo	= makeDescriptorBufferInfo(resultBuffer.get(), 0ull, resultBufferSize);
	const VkBufferMemoryBarrier		resultReadyBarrier	= makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
																				  resultBuffer.get(), 0, resultBufferSize, consumerIndex, consumerIndex);

	// descriptor set for consumer image and result buffer
	Move<VkDescriptorPool>			consumerDsPool		= DescriptorPoolBuilder()
															.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
															.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
															.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	Move<VkDescriptorSetLayout>		consumerDsLayout	= DescriptorSetLayoutBuilder()
															.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL)
															.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL)
															.build(vkd, device);
	Move<VkDescriptorSet>			consumerDs			= makeDescriptorSet(vkd, device, *consumerDsPool, *consumerDsLayout);

	DescriptorSetUpdateBuilder()
		.writeSingle(*consumerDs, DescriptorSetUpdateBuilder::Location::binding(0), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageDsInfo)
		.writeSingle(*consumerDs, DescriptorSetUpdateBuilder::Location::binding(1), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultDsBuffInfo)
		.update(vkd, device);

	Move<VkPipelineLayout>			producer1Layout		= createPipelineLayout<>({ *producerDsLayout });
	Move<VkPipeline>				producer1Pipeline	= createComputePipeline(*producer1Layout, true);
	Move<VkPipelineLayout>			producer2Layout		= createPipelineLayout<>({});
	Move<VkPipeline>				producer2Pipeline	= createGraphicsPipeline(*producer2Layout, *renderPass);

	Move<VkPipelineLayout>			consumerLayout		= createPipelineLayout<>({ *consumerDsLayout });
	Move<VkPipeline>				consumerPipeline	= createComputePipeline(*consumerLayout, false);

	Move<VkCommandPool>				producerPool		= makeCommandPool(producerIndex);
	Move<VkCommandPool>				consumerPool		= makeCommandPool(consumerIndex);
	Move<VkCommandBuffer>			producerCmd			= allocateCommandBuffer(vkd, device, *producerPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	Move<VkCommandBuffer>			consumerCmd			= allocateCommandBuffer(vkd, device, *consumerPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);


	beginCommandBuffer(vkd, *producerCmd);
		vkd.cmdBindPipeline(*producerCmd, VK_PIPELINE_BIND_POINT_COMPUTE, *producer1Pipeline);
		vkd.cmdBindPipeline(*producerCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *producer2Pipeline);
		vkd.cmdBindVertexBuffers(*producerCmd, 0, 1, vertexBuffer.getPtr(), &static_cast<const VkDeviceSize&>(0));
		vkd.cmdBindDescriptorSets(*producerCmd, VK_PIPELINE_BIND_POINT_COMPUTE, *producer1Layout, 0, 1, &producerDs.get(), 0, nullptr);
		vkd.cmdDispatch(*producerCmd, deUint32(positions.size()), 1, 1);
		vkd.cmdPipelineBarrier(*producerCmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0,
							   0, nullptr, 1, &producerReadyBarrier, 0, nullptr);
		beginRenderPass(vkd, *producerCmd, *renderPass, *framebuffer, makeRect2D(m_config.width, m_config.height), clearColor);
			vkd.cmdDraw(*producerCmd, deUint32(positions.size()), 1, 0, 0);
		endRenderPass(vkd, *producerCmd);
		vkd.cmdPipelineBarrier(*producerCmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0,
							   0u, nullptr, 0u, nullptr, 1u, &imageReadyBarrier);
	endCommandBuffer(vkd, *producerCmd);

	beginCommandBuffer(vkd, *consumerCmd);
		vkd.cmdBindPipeline(*consumerCmd, VK_PIPELINE_BIND_POINT_COMPUTE, *consumerPipeline);
		vkd.cmdBindDescriptorSets(*consumerCmd, VK_PIPELINE_BIND_POINT_COMPUTE, *consumerLayout, 0, 1, &consumerDs.get(), 0, nullptr);
		vkd.cmdDispatch(*consumerCmd, m_config.width, m_config.height, 1);
		vkd.cmdPipelineBarrier(*consumerCmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
							   0, nullptr, 1, &resultReadyBarrier, 0, nullptr);
	endCommandBuffer(vkd, *consumerCmd);

	submitCommands(*producerCmd, *consumerCmd);

	resultBuffer.invalidateAlloc(vkd, device);
	const tcu::ConstPixelBufferAccess	resultBufferAccess(mapVkFormat(m_config.format), m_config.width, m_config.height, 1, resultBuffer.getHostPtr());
	const deUint32 resultValue = resultBufferAccess.getPixelUint(0, 0).x();
	const deUint32 expectedValue = 1;
	const bool ok = (resultValue == expectedValue);
	if (!ok)
	{
		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message << "Expected value: " << expectedValue << ", got " << resultValue << tcu::TestLog::EndMessage;
	}

	return ok ? tcu::TestStatus::pass("") : tcu::TestStatus::fail("");
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
		{ VK_QUEUE_PROTECTED_BIT,								"protected"			}
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
