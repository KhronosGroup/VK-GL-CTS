/*------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2023 The Khronos Group Inc.
* Copyright (c) 2023 LunarG, Inc.
* Copyright (c) 2023 Google LLC
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
*//*
 * \file
 * \brief Extended dynamic state tests
*//*--------------------------------------------------------------------*/

#include "vktPipelineBindVertexBuffers2Tests.hpp"
#include "deUniquePtr.hpp"
#include "tcuTestCase.hpp"
#include "vktTestCase.hpp"
#include "vkTypeUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkImageUtil.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkSafetyCriticalUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vkDeviceUtil.hpp"
#include "tcuCommandLine.hpp"
#include "deRandom.hpp"

namespace vkt
{
namespace pipeline
{

struct TestParams
{
	deUint32	colorStride;
	deUint32	vertexStride;
	deUint32	colorOffset;
	deUint32	vertexOffset;
};

vk::VkImageCreateInfo makeImageCreateInfo (const vk::VkExtent2D extent, const vk::VkFormat format, const vk::VkImageUsageFlags usage)
{
	const vk::VkImageCreateInfo	imageParams	=
	{
		vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,			// VkStructureType			sType;
		DE_NULL,											// const void*				pNext;
		(vk::VkImageCreateFlags)0u,							// VkImageCreateFlags		flags;
		vk::VK_IMAGE_TYPE_2D,								// VkImageType				imageType;
		format,												// VkFormat					format;
		vk::makeExtent3D(extent.width, extent.height, 1),	// VkExtent3D				extent;
		1u,													// deUint32					mipLevels;
		1u,													// deUint32					arrayLayers;
		vk::VK_SAMPLE_COUNT_1_BIT,							// VkSampleCountFlagBits	samples;
		vk::VK_IMAGE_TILING_OPTIMAL,						// VkImageTiling			tiling;
		usage,												// VkImageUsageFlags		usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,						// VkSharingMode			sharingMode;
		0u,													// deUint32					queueFamilyIndexCount;
		DE_NULL,											// const deUint32*			pQueueFamilyIndices;
		vk::VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			initialLayout;
	};
	return imageParams;
}

//make a buffer to read an image back after rendering
std::unique_ptr<vk::BufferWithMemory> makeBufferForImage (const vk::DeviceInterface& vkd, const vk::VkDevice device, vk::Allocator& allocator, tcu::TextureFormat tcuFormat, vk::VkExtent2D imageExtent)
{
	const auto	outBufferSize		= static_cast<vk::VkDeviceSize>(static_cast<uint32_t>(tcu::getPixelSize(tcuFormat)) * imageExtent.width * imageExtent.height);
	const auto	outBufferUsage		= vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	const auto	outBufferInfo		= makeBufferCreateInfo(outBufferSize, outBufferUsage);
	auto outBuffer = std::unique_ptr<vk::BufferWithMemory>(new vk::BufferWithMemory(vkd, device, allocator, outBufferInfo, vk::MemoryRequirement::HostVisible));

	return outBuffer;
}

vk::VkVertexInputBindingDescription makeBindingDescription(uint32_t binding, uint32_t stride, vk::VkVertexInputRate inputRate)
{
	vk::VkVertexInputBindingDescription bindingDescription;

	bindingDescription.binding = binding;
	bindingDescription.stride = stride;
	bindingDescription.inputRate = inputRate;

	return bindingDescription;
}

vk::VkVertexInputAttributeDescription makeAttributeDescription (uint32_t location, uint32_t binding, vk::VkFormat format, uint32_t offset)
{
	vk::VkVertexInputAttributeDescription attributeDescription;

	attributeDescription.location = location;
	attributeDescription.binding = binding;
	attributeDescription.format = format;
	attributeDescription.offset = offset;

	return attributeDescription;
}

void copyAndFlush(const vk::DeviceInterface& vkd, vk::VkDevice device, vk::BufferWithMemory& buffer, size_t offset, const void* src, size_t size)
{
	auto&	alloc	= buffer.getAllocation();
	auto	dst		= reinterpret_cast<char*>(alloc.getHostPtr());

	deMemcpy(dst + offset, src, size);
	vk::flushAlloc(vkd, device, alloc);
}

#ifndef CTS_USES_VULKANSC
typedef de::MovePtr<vk::DeviceDriver> DeviceDriverPtr;
#else
typedef de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter> DeviceDriverPtr;
#endif // CTS_USES_VULKANSC

typedef vk::Move<vk::VkDevice> DevicePtr;

vk::Move<vk::VkDevice> createRobustBufferAccessDevice (Context& context, const vk::VkPhysicalDeviceFeatures2* enabledFeatures2)
{
	const float queuePriority = 1.0f;

	// Create a universal queue that supports graphics and compute
	const vk::VkDeviceQueueCreateInfo	queueParams =
	{
		vk::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	// VkStructureType				sType;
		DE_NULL,										// const void*					pNext;
		0u,												// VkDeviceQueueCreateFlags		flags;
		context.getUniversalQueueFamilyIndex(),			// deUint32						queueFamilyIndex;
		1u,												// deUint32						queueCount;
		&queuePriority									// const float*					pQueuePriorities;
	};

	vk::VkPhysicalDeviceFeatures enabledFeatures1 = context.getDeviceFeatures();
	enabledFeatures1.robustBufferAccess = true;

	// \note Extensions in core are not explicitly enabled even though
	//		 they are in the extension list advertised to tests.
	const auto& extensionPtrs = context.getDeviceCreationExtensions();

	void* pNext = (void*)enabledFeatures2;
#ifdef CTS_USES_VULKANSC
	VkDeviceObjectReservationCreateInfo memReservationInfo = context.getTestContext().getCommandLine().isSubProcess() ? context.getResourceInterface()->getStatMax() : resetDeviceObjectReservationCreateInfo();
	memReservationInfo.pNext = pNext;
	pNext = &memReservationInfo;

	VkPhysicalDeviceVulkanSC10Features sc10Features =
		createDefaultSC10Features();
	sc10Features.pNext = pNext;
	pNext = &sc10Features;

	VkPipelineCacheCreateInfo			pcCI;
	std::vector<VkPipelinePoolSize>		poolSizes;
	if (context.getTestContext().getCommandLine().isSubProcess())
	{
		if (context.getResourceInterface()->getCacheDataSize() > 0)
		{
			pcCI =
			{
				VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,		// VkStructureType				sType;
				DE_NULL,											// const void*					pNext;
				VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
					VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT,	// VkPipelineCacheCreateFlags	flags;
				context.getResourceInterface()->getCacheDataSize(),	// deUintptr					initialDataSize;
				context.getResourceInterface()->getCacheData()		// const void*					pInitialData;
			};
			memReservationInfo.pipelineCacheCreateInfoCount = 1;
			memReservationInfo.pPipelineCacheCreateInfos = &pcCI;
		}

		poolSizes = context.getResourceInterface()->getPipelinePoolSizes();
		if (!poolSizes.empty())
		{
			memReservationInfo.pipelinePoolSizeCount = deUint32(poolSizes.size());
			memReservationInfo.pPipelinePoolSizes = poolSizes.data();
		}
	}
#endif

	const vk::VkDeviceCreateInfo		deviceParams =
	{
		vk::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,	// VkStructureType					sType;
		pNext,										// const void*						pNext;
		0u,											// VkDeviceCreateFlags				flags;
		1u,											// deUint32							queueCreateInfoCount;
		&queueParams,								// const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
		0u,											// deUint32							enabledLayerCount;
		nullptr,									// const char* const*				ppEnabledLayerNames;
		de::sizeU32(extensionPtrs),					// deUint32							enabledExtensionCount;
		de::dataOrNull(extensionPtrs),				// const char* const*				ppEnabledExtensionNames;
		enabledFeatures2 ? nullptr : &enabledFeatures1	// const VkPhysicalDeviceFeatures*	pEnabledFeatures;
	};

	// We are creating a custom device with a potentially large amount of extensions and features enabled, using the default device
	// as a reference. Some implementations may only enable certain device extensions if some instance extensions are enabled, so in
	// this case it's important to reuse the context instance when creating the device.
	const auto& vki = context.getInstanceInterface();
	const auto	instance = context.getInstance();
	const auto	physicalDevice = chooseDevice(vki, instance, context.getTestContext().getCommandLine());

	return createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(), context.getPlatformInterface(),
		instance, vki, physicalDevice, &deviceParams);
}

enum BeyondType
{
	BUFFER,
	SIZE
};

struct TestParamsMaint5
{
	vk::VkPrimitiveTopology	topology;
	deUint32				width;
	deUint32				height;
	deUint32				bufferCount;
	deUint32				rndSeed;
	bool					wholeSize;
	BeyondType				beyondType;
};

class BindBuffers2Instance : public vkt::TestInstance
{
public:
						BindBuffers2Instance		(Context& context, const vk::PipelineConstructionType	pipelineConstructionType, const TestParams params, const bool singleBind, const deUint32 count)
							: vkt::TestInstance				(context)
							, m_pipelineConstructionType	(pipelineConstructionType)
							, m_params						(params)
							, m_singleBind					(singleBind)
							, m_count						(count)
							{}
	virtual				~BindBuffers2Instance		(void) {}

	tcu::TestStatus		iterate						(void) override;

private:
	const vk::PipelineConstructionType	m_pipelineConstructionType;
	const TestParams					m_params;
	const bool							m_singleBind;
	const deUint32						m_count;
};

tcu::TestStatus BindBuffers2Instance::iterate (void)
{
	const vk::VkInstance			instance			= m_context.getInstance();
	const vk::InstanceDriver		instanceDriver		(m_context.getPlatformInterface(), instance);
	const vk::InstanceInterface&	vki					= m_context.getInstanceInterface();
	const vk::DeviceInterface&		vk					= m_context.getDeviceInterface();
	const vk::VkPhysicalDevice		physicalDevice		= m_context.getPhysicalDevice();
	const vk::VkDevice				device				= m_context.getDevice();
	const vk::VkQueue				queue				= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	vk::Allocator&					allocator			= m_context.getDefaultAllocator();
	const auto&						deviceExtensions	= m_context.getDeviceExtensions();
	tcu::TestLog&					log					= m_context.getTestContext().getLog();

	vk::VkExtent2D					extent				= {	32u, 32u };

	const std::vector<vk::VkViewport>	viewports	{ vk::makeViewport(extent) };
	const std::vector<vk::VkRect2D>		scissors	{ vk::makeRect2D(extent) };

	const vk::VkPipelineLayoutCreateInfo		pipelineLayoutInfo		=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType;
		DE_NULL,											// const void*					pNext;
		0u,													// VkPipelineLayoutCreateFlags	flags;
		0u,													// deUint32						descriptorSetCount;
		DE_NULL,											// const VkDescriptorSetLayout*	pSetLayouts;
		0u,													// deUint32						pushConstantRangeCount;
		DE_NULL												// const VkPushDescriptorRange*	pPushDescriptorRanges;
	};

	const vk::VkImageSubresourceRange				colorSubresourceRange = vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const vk::Move<vk::VkImage>						colorImage			(makeImage(vk, device, makeImageCreateInfo(extent, vk::VK_FORMAT_R32G32B32A32_SFLOAT, vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT)));
	const de::MovePtr<vk::Allocation>				colorImageAlloc		(bindImage(vk, device, allocator, *colorImage, vk::MemoryRequirement::Any));
	const vk::Move<vk::VkImageView>					colorImageView		(makeImageView(vk, device, *colorImage, vk::VK_IMAGE_VIEW_TYPE_2D, vk::VK_FORMAT_R32G32B32A32_SFLOAT, colorSubresourceRange));

	const vk::PipelineLayoutWrapper					pipelineLayout		(m_pipelineConstructionType, vk, device, &pipelineLayoutInfo);
	vk::RenderPassWrapper							renderPass			(m_pipelineConstructionType, vk, device, vk::VK_FORMAT_R32G32B32A32_SFLOAT);
	renderPass.createFramebuffer(vk, device, *colorImage, *colorImageView, extent.width, extent.height);
	const vk::ShaderWrapper							vertShaderModule	= vk::ShaderWrapper(vk, device, m_context.getBinaryCollection().get("vert"));
	const vk::ShaderWrapper							fragShaderModule	= vk::ShaderWrapper(vk, device, m_context.getBinaryCollection().get("frag"));

	//buffer to read the output image
	auto											outBuffer			= makeBufferForImage(vk, device, allocator, mapVkFormat(vk::VK_FORMAT_R32G32B32A32_SFLOAT), extent);
	auto&											outBufferAlloc		= outBuffer->getAllocation();

	std::vector<vk::VkVertexInputAttributeDescription>		attributes;
	if (m_count == 2)
	{
		attributes = {
			makeAttributeDescription(0, 0, vk::VK_FORMAT_R32G32_SFLOAT, 0),
			makeAttributeDescription(1, 1, vk::VK_FORMAT_R32G32_SFLOAT, 0),
			makeAttributeDescription(2, 2, vk::VK_FORMAT_R32G32_SFLOAT, 0),
			makeAttributeDescription(3, 3, vk::VK_FORMAT_R32G32_SFLOAT, 0),
		};
	}
	else if (m_count == 3)
	{
		attributes = {
			makeAttributeDescription(0, 0, vk::VK_FORMAT_R32G32_SFLOAT, 0),
			makeAttributeDescription(1, 1, vk::VK_FORMAT_R32G32_SFLOAT, 0),
			makeAttributeDescription(2, 2, vk::VK_FORMAT_R32_SFLOAT, 0),
			makeAttributeDescription(3, 3, vk::VK_FORMAT_R32_SFLOAT, 0),
			makeAttributeDescription(4, 4, vk::VK_FORMAT_R32_SFLOAT, 0),
			makeAttributeDescription(5, 5, vk::VK_FORMAT_R32_SFLOAT, 0),
		};
	}
	else if (m_count == 4)
	{
		attributes = {
			makeAttributeDescription(0, 0, vk::VK_FORMAT_R32_SFLOAT, 0),
			makeAttributeDescription(1, 1, vk::VK_FORMAT_R32_SFLOAT, 0),
			makeAttributeDescription(2, 2, vk::VK_FORMAT_R32_SFLOAT, 0),
			makeAttributeDescription(3, 3, vk::VK_FORMAT_R32_SFLOAT, 0),
			makeAttributeDescription(4, 4, vk::VK_FORMAT_R32_SFLOAT, 0),
			makeAttributeDescription(5, 5, vk::VK_FORMAT_R32_SFLOAT, 0),
			makeAttributeDescription(6, 6, vk::VK_FORMAT_R32_SFLOAT, 0),
			makeAttributeDescription(7, 7, vk::VK_FORMAT_R32_SFLOAT, 0),
		};
	}
	else
	{
		attributes = {
			makeAttributeDescription(0, 0, vk::VK_FORMAT_R32G32B32A32_SFLOAT, 0),
			makeAttributeDescription(1, 1, vk::VK_FORMAT_R32G32B32A32_SFLOAT, 0),
		};
	}

	log << tcu::TestLog::Message << "VkVertexInputAttributeDescription:" << tcu::TestLog::EndMessage;
	for (const auto& attrib : attributes) {
		log << tcu::TestLog::Message << "location " << attrib.location << ", binding " << attrib.binding << ", format " << attrib.format << tcu::TestLog::EndMessage;
	}

	std::vector<vk::VkVertexInputBindingDescription>		bindings;
	for (deUint32 i = 0; i < m_count; ++i)
	{
		bindings.push_back(makeBindingDescription(i * 2, 99 /*ignored*/, vk::VK_VERTEX_INPUT_RATE_INSTANCE));
		bindings.push_back(makeBindingDescription(i * 2 + 1, 99 /*ignored*/, vk::VK_VERTEX_INPUT_RATE_VERTEX));
	};
	log << tcu::TestLog::Message << "VkVertexInputBindingDescription:\n" << tcu::TestLog::EndMessage;
	for (const auto& binding : bindings) {
		log << tcu::TestLog::Message << "binding " << binding.binding << ", stride " << binding.stride << ", inputRate " << binding.inputRate << tcu::TestLog::EndMessage;
	}

	vk::VkPipelineVertexInputStateCreateInfo		vertexInputState	= vk::initVulkanStructure();
	vertexInputState.vertexBindingDescriptionCount = (deUint32)bindings.size();
	vertexInputState.pVertexBindingDescriptions = bindings.data();
	vertexInputState.vertexAttributeDescriptionCount = (deUint32)attributes.size();
	vertexInputState.pVertexAttributeDescriptions = attributes.data();

	vk::VkPipelineInputAssemblyStateCreateInfo		inputAssemblyState	= vk::initVulkanStructure();
	inputAssemblyState.topology = vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	const vk::VkDynamicState						dynamicState		= vk::VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT;

	const vk::VkPipelineDynamicStateCreateInfo		dynamicStateInfo	=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,													//	const void*							pNext;
		0u,															//	VkPipelineDynamicStateCreateFlags	flags;
		1u,															//	uint32_t							dynamicStateCount;
		&dynamicState,												//	const VkDynamicState*				pDynamicStates;
	};

	vk::GraphicsPipelineWrapper	graphicsPipelineWrapper	{ vki, vk, physicalDevice, device, deviceExtensions, m_pipelineConstructionType };
	graphicsPipelineWrapper.setDefaultDepthStencilState()
		.setDefaultColorBlendState()
		.setDefaultRasterizationState()
		.setDefaultMultisampleState()
		.setDynamicState(&dynamicStateInfo)
		.setupVertexInputState(&vertexInputState, &inputAssemblyState)
		.setupPreRasterizationShaderState(viewports,
			scissors,
			pipelineLayout,
			renderPass.get(),
			0u,
			vertShaderModule)
		.setupFragmentShaderState(pipelineLayout,
			renderPass.get(),
			0u,
			fragShaderModule)
		.setupFragmentOutputState(renderPass.get())
		.setMonolithicPipelineLayout(pipelineLayout)
		.buildPipeline();

	const vk::Move<vk::VkCommandPool>				cmdPool			(createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	const vk::Move<vk::VkCommandBuffer>				cmdBuffer		(allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	deUint32										instanceCount	= 4u;
	vk::VkDeviceSize								colorStride		= m_params.colorStride * sizeof(float);
	vk::VkDeviceSize								colorOffset		= m_params.colorOffset * sizeof(float);
	vk::VkDeviceSize								vertexStride	= m_params.vertexStride * sizeof(float);
	vk::VkDeviceSize								vertexOffset	= m_params.vertexOffset * sizeof(float);

	tcu::Vec4										colors[]		=
	{
		tcu::Vec4(0.21f, 0.41f, 0.61f, 0.81f),
		tcu::Vec4(0.22f, 0.42f, 0.62f, 0.82f),
		tcu::Vec4(0.23f, 0.43f, 0.63f, 0.83f),
		tcu::Vec4(0.24f, 0.44f, 0.64f, 0.84f),
	};

	tcu::Vec4										vertices[] =
	{
		tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f),
		tcu::Vec4(0.0f, 1.0f, 0.0f, 0.0f),
		tcu::Vec4(1.0f, 0.0f, 0.0f, 0.0f),
		tcu::Vec4(1.0f, 1.0f, 0.0f, 0.0f),
	};

	std::vector<float>								colorData;
	for (deUint32 i = 0; i < colorOffset / sizeof(float); ++i)
		colorData.push_back(0);

	for (deUint32 i = 0; i < 4; ++i)
	{
		colorData.push_back(colors[i].x());
		colorData.push_back(colors[i].y());
		colorData.push_back(colors[i].z());
		colorData.push_back(colors[i].w());
		for (deUint32 j = 4; j < colorStride / sizeof(float); ++j)
			colorData.push_back(0.0f);
	}

	std::vector<float>								vertexData;
	for (deUint32 i = 0; i < vertexOffset / sizeof(float); ++i)
		vertexData.push_back(0);

	for (deUint32 i = 0; i < 4; ++i)
	{
		vertexData.push_back(vertices[i].x());
		vertexData.push_back(vertices[i].y());
		vertexData.push_back(vertices[i].z());
		vertexData.push_back(vertices[i].w());
		for (deUint32 j = 4; j < vertexStride / sizeof(float); ++j)
			vertexData.push_back(0.0f);
	}

	vk::VkClearValue								clearColorValue		= defaultClearValue(vk::VK_FORMAT_R32G32B32A32_SFLOAT);
	vk::VkDeviceSize								colorBufferSize		= colorData.size() * sizeof(float);
	vk::VkDeviceSize								vertexBufferSize	= vertexData.size() * sizeof(float);

	const auto										colorCreateInfo		= vk::makeBufferCreateInfo(colorBufferSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	const auto										vertexCreateInfo	= vk::makeBufferCreateInfo(vertexBufferSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	de::MovePtr<vk::BufferWithMemory>				colorBuffer			= de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(vk, device, allocator, colorCreateInfo, vk::MemoryRequirement::HostVisible));
	de::MovePtr<vk::BufferWithMemory>				vertexBuffer		= de::MovePtr<vk::BufferWithMemory>(new vk::BufferWithMemory(vk, device, allocator, vertexCreateInfo, vk::MemoryRequirement::HostVisible));
	copyAndFlush(vk, device, *colorBuffer, 0, colorData.data(), colorData.size() * sizeof(float));
	copyAndFlush(vk, device, *vertexBuffer, 0, vertexData.data(), vertexData.size() * sizeof(float));

	beginCommandBuffer(vk, *cmdBuffer);
	renderPass.begin(vk, *cmdBuffer, vk::makeRect2D(0, 0, extent.width, extent.height), clearColorValue);
	graphicsPipelineWrapper.bind(*cmdBuffer);

	vk::VkBuffer		buffers[]	= { **colorBuffer, **vertexBuffer, **colorBuffer, **vertexBuffer, **colorBuffer, **vertexBuffer, **colorBuffer, **vertexBuffer };
	std::vector<vk::VkDeviceSize>	offsets	= { colorOffset, vertexOffset };
	if (m_count == 2)
	{
		offsets.push_back(colorOffset + sizeof(float) * 2);
		offsets.push_back(vertexOffset + sizeof(float) * 2);
	}
	else if (m_count == 3)
	{
		offsets.push_back(colorOffset + sizeof(float) * 2);
		offsets.push_back(vertexOffset + sizeof(float) * 2);
		offsets.push_back(colorOffset + sizeof(float) * 3);
		offsets.push_back(vertexOffset + sizeof(float) * 3);
	}
	else if (m_count == 4)
	{
		offsets.push_back(colorOffset + sizeof(float));
		offsets.push_back(vertexOffset + sizeof(float));
		offsets.push_back(colorOffset + sizeof(float) * 2);
		offsets.push_back(vertexOffset + sizeof(float) * 2);
		offsets.push_back(colorOffset + sizeof(float) * 3);
		offsets.push_back(vertexOffset + sizeof(float) * 3);
	}
	std::vector<vk::VkDeviceSize>	sizes;
	for (deUint32 i = 0; i < m_count; ++i)
	{
		sizes.push_back(colorBufferSize - offsets[i * 2]);
		sizes.push_back(vertexBufferSize - offsets[i * 2 + 1]);
	}
	vk::VkDeviceSize	strides[]	= { colorStride, vertexStride, colorStride, vertexStride, colorStride, vertexStride, colorStride, vertexStride };
	if (m_singleBind)
	{
#ifndef CTS_USES_VULKANSC
		vk.cmdBindVertexBuffers2(*cmdBuffer, 0, 2 * m_count, buffers, offsets.data(), sizes.data(), strides);
#else
		vk.cmdBindVertexBuffers2EXT(*cmdBuffer, 0, 2 * m_count, buffers, offsets.data(), sizes.data(), strides);
#endif
	}
	else
	{
		for (deUint32 i = 0; i < m_count * 2; ++i)
		{
#ifndef CTS_USES_VULKANSC
			vk.cmdBindVertexBuffers2(*cmdBuffer, i, 1, &buffers[i], &offsets[i], &sizes[i], &strides[i]);
#else
			vk.cmdBindVertexBuffers2EXT(*cmdBuffer, i, 1, &buffers[i], &offsets[i], &sizes[i], &strides[i]);
#endif
		}
	}
	log << tcu::TestLog::Message << "vkCmdBindVertexBuffers2" << tcu::TestLog::EndMessage;
	for (deUint32 i = 0; i < m_count * 2; ++i)
	{
		log << tcu::TestLog::Message << "binding " << i << ", buffer " << buffers[i] << ", offset " << offsets[i] << ", size " << sizes[i] << ", stride " << strides[i] << tcu::TestLog::EndMessage;
	}

	vk.cmdDraw(*cmdBuffer, 4, instanceCount, 0, 0);
	renderPass.end(vk, *cmdBuffer);

	vk::copyImageToBuffer(vk, *cmdBuffer, *colorImage, (*outBuffer).get(), tcu::IVec2(extent.width, extent.height));
	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	invalidateAlloc(vk, device, outBufferAlloc);
	const tcu::ConstPixelBufferAccess result(vk::mapVkFormat(vk::VK_FORMAT_R32G32B32A32_SFLOAT), tcu::IVec3(extent.width, extent.height, 1), (const char*)outBufferAlloc.getHostPtr());

	const deUint32 h = result.getHeight();
	const deUint32 w = result.getWidth();
	for (deUint32 y = 0; y < h; y++)
	{
		for (deUint32 x = 0; x < w; x++)
		{
			tcu::Vec4	pix = result.getPixel(x, y);

			if (x >= w / 2 && y >= h / 2 && pix != colors[0])
			{
				log << tcu::TestLog::Message << "Color at (" << x << ", " << y << ") was " << pix << ", but expected color was " << colors[0] << tcu::TestLog::EndMessage;
				return tcu::TestStatus::fail("Fail");
			}
			if (x < w / 2 && y >= h / 2 && pix != colors[colorStride == 0 ? 0 : 1])
			{
				log << tcu::TestLog::Message << "Color at (" << x << ", " << y << ") was " << pix << ", but expected color was " << colors[colorStride == 0 ? 0 : 1] << tcu::TestLog::EndMessage;
				return tcu::TestStatus::fail("Fail");
			}
			if (x >= w / 2 && y < h / 2 && pix != colors[colorStride == 0 ? 0 : 2])
			{
				log << tcu::TestLog::Message << "Color at (" << x << ", " << y << ") was " << pix << ", but expected color was " << colors[colorStride == 0 ? 0 : 2] << tcu::TestLog::EndMessage;
				return tcu::TestStatus::fail("Fail");
			}
			if (x < w / 2 && y < h / 2 && pix != colors[colorStride == 0 ? 0 : 3])
			{
				log << tcu::TestLog::Message << "Color at (" << x << ", " << y << ") was " << pix << ", but expected color was " << colors[colorStride == 0 ? 0 : 3] << tcu::TestLog::EndMessage;
				return tcu::TestStatus::fail("Fail");
			}
		}
	}

	return tcu::TestStatus::pass("Pass");
}

class BindVertexBuffers2Instance : public vkt::TestInstance
{
public:
						BindVertexBuffers2Instance	(Context&						context,
													 DeviceDriverPtr				driver,
													 DevicePtr						device,
													 vk::PipelineConstructionType	pipelineConstructionType,
													 const TestParamsMaint5&		params,
													 bool							robustness2)
							: vkt::TestInstance(context)
							, m_pipelineConstructionType(pipelineConstructionType)
							, m_params(params)
							, m_robustness2(robustness2)
							, m_deviceDriver(driver)
							, m_device(device)
							, m_physicalDevice(chooseDevice(context.getInstanceInterface(), context.getInstance(), context.getTestContext().getCommandLine()))
							, m_allocator(getDeviceInterface(), getDevice(), getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), m_physicalDevice))
							, m_pipelineWrapper(context.getInstanceInterface(), context.getDeviceInterface(), m_physicalDevice, getDevice(), m_context.getDeviceExtensions(), m_pipelineConstructionType, 0u)
							, m_vertShaderModule(context.getDeviceInterface(), getDevice(), m_context.getBinaryCollection().get("vert"))
							, m_fragShaderModule(context.getDeviceInterface(), getDevice(), m_context.getBinaryCollection().get("frag")) { }
	virtual				~BindVertexBuffers2Instance	(void) = default;

	tcu::TestStatus		iterate(void) override;

protected:
	typedef std::vector<vk::VkDeviceSize> Sizes;
	typedef std::vector<de::SharedPtr<vk::BufferWithMemory>> Buffers;
	const vk::DeviceInterface&	getDeviceInterface	() const;
	vk::VkDevice				getDevice			() const;
	vk::VkQueue					getQueue			() const;
	vk::Allocator&				getAllocator();
	void						createPipeline		(const vk::PipelineLayoutWrapper&	layout,
													 vk::VkRenderPass		renderPass);
	Buffers						createBuffers		(Sizes&					offsets,
													 Sizes&					strides,
													 Sizes&					sizes);

private:
	const vk::PipelineConstructionType	m_pipelineConstructionType;
	const TestParamsMaint5				m_params;
	const bool							m_robustness2;
	DeviceDriverPtr						m_deviceDriver;
	DevicePtr							m_device;
	const vk::VkPhysicalDevice			m_physicalDevice;
	vk::SimpleAllocator					m_allocator;
	vk::GraphicsPipelineWrapper			m_pipelineWrapper;
	const vk::ShaderWrapper				m_vertShaderModule;
	const vk::ShaderWrapper				m_fragShaderModule;
};

const vk::DeviceInterface& BindVertexBuffers2Instance::getDeviceInterface () const
{
	return m_robustness2 ? *m_deviceDriver : m_context.getDeviceInterface();
}

vk::VkDevice BindVertexBuffers2Instance::getDevice () const
{
	return m_robustness2 ? *m_device : m_context.getDevice();
}

vk::VkQueue BindVertexBuffers2Instance::getQueue () const
{
	vk::VkQueue queue = DE_NULL;
	if (m_robustness2)
	{
		const deUint32 queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
		m_deviceDriver->getDeviceQueue(getDevice(), queueFamilyIndex, 0, &queue);
	}
	else
	{
		queue = m_context.getUniversalQueue();
	}
	return queue;
}

vk::Allocator& BindVertexBuffers2Instance::getAllocator ()
{
	return m_allocator;
}

void BindVertexBuffers2Instance::createPipeline (const vk::PipelineLayoutWrapper& layout, vk::VkRenderPass renderPass)
{
	vk::VkPhysicalDeviceProperties	dp{};
	m_context.getInstanceInterface().getPhysicalDeviceProperties(m_physicalDevice, &dp);

	const std::vector<vk::VkViewport>	viewports	{ vk::makeViewport(m_params.width, m_params.height) };
	const std::vector<vk::VkRect2D>		scissors	{ vk::makeRect2D(m_params.width, m_params.height) };

	std::vector<vk::VkVertexInputBindingDescription>		bindings
	{
		// color buffer binding
		makeBindingDescription(0, dp.limits.maxVertexInputBindingStride /* ignored */, vk::VK_VERTEX_INPUT_RATE_VERTEX)
	};
	for (deUint32 b = 1; b < m_params.bufferCount; ++b)
	{
		// vertex buffer binding
		bindings.push_back(makeBindingDescription(b, dp.limits.maxVertexInputBindingStride /* ignored */, vk::VK_VERTEX_INPUT_RATE_VERTEX));
	}

	std::vector<vk::VkVertexInputAttributeDescription>		attributes
	{
		// color attribute layout information
		makeAttributeDescription(0, 0, vk::VK_FORMAT_R32G32B32_SFLOAT, 0)
	};
	for (deUint32 lb = 1; lb < m_params.bufferCount; ++lb)
	{
		attributes.push_back(makeAttributeDescription(lb, 1, vk::VK_FORMAT_R32G32_SFLOAT, 0));
	}

	vk::VkPipelineVertexInputStateCreateInfo				vertexInputState = vk::initVulkanStructure();
	vertexInputState.vertexBindingDescriptionCount		= (deUint32)bindings.size();
	vertexInputState.pVertexBindingDescriptions			= bindings.data();
	vertexInputState.vertexAttributeDescriptionCount	= (deUint32)attributes.size();
	vertexInputState.pVertexAttributeDescriptions		= attributes.data();

	vk::VkPipelineInputAssemblyStateCreateInfo				inputAssemblyState = vk::initVulkanStructure();
	inputAssemblyState.topology	= m_params.topology;

	const vk::VkDynamicState								dynamicState = vk::VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT;

	const vk::VkPipelineDynamicStateCreateInfo				dynamicStateInfo
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,	//	VkStructureType						sType;
		nullptr,													//	const void*							pNext;
		0u,															//	VkPipelineDynamicStateCreateFlags	flags;
		1u,															//	uint32_t							dynamicStateCount;
		&dynamicState,												//	const VkDynamicState*				pDynamicStates;
	};

	const vk::VkPipelineRasterizationStateCreateInfo		rasterizationCreateInfo
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// VkStructureType								sType
		DE_NULL,														// const void*									pNext
		0u,																// VkPipelineRasterizationStateCreateFlags		flags
		VK_FALSE,														// VkBool32										depthClampEnable
		VK_FALSE,														// VkBool32										rasterizerDiscardEnable
		vk::VK_POLYGON_MODE_FILL,										// VkPolygonMode								polygonMode
		vk::VK_CULL_MODE_NONE,											// VkCullModeFlags								cullMode
		vk::VK_FRONT_FACE_CLOCKWISE,									// VkFrontFace									frontFace
		VK_FALSE,														// VkBool32										depthBiasEnable
		0.0f,															// float										depthBiasConstantFactor
		0.0f,															// float										depthBiasClamp
		0.0f,															// float										depthBiasSlopeFactor
		1.0f															// float										lineWidth
	};

	m_pipelineWrapper.setDefaultDepthStencilState()
		.setDefaultColorBlendState()
		//.setDefaultRasterizationState()
		.setDefaultMultisampleState()
		.setDynamicState(&dynamicStateInfo)
		.setupVertexInputState(&vertexInputState, &inputAssemblyState)
		.setupPreRasterizationShaderState(
			viewports,
			scissors,
			layout,
			renderPass,
			0u,
			m_vertShaderModule,
			&rasterizationCreateInfo)
		.setupFragmentShaderState(
			layout,
			renderPass,
			0u,
			m_fragShaderModule)
		.setupFragmentOutputState(renderPass)
		.setMonolithicPipelineLayout(layout)
	.buildPipeline();
}

BindVertexBuffers2Instance::Buffers BindVertexBuffers2Instance::createBuffers (Sizes& offsets, Sizes& strides, Sizes& sizes)
{
	Buffers						buffers;
	vk::Allocator&				allocator	= getAllocator();
	const vk::DeviceInterface&	vk			= getDeviceInterface();
	const vk::VkDevice			device		= getDevice();
	de::Random					rnd			(m_params.rndSeed);
	const float					p			= 1.0f / float(m_params.bufferCount - 1); DE_ASSERT(m_params.bufferCount >= 2);
	const deUint32				compCount	= deUint32(sizeof(tcu::Vec2) / sizeof(float));

	std::vector<float>			pointTemplate;
	deUint32					returnSize	= 0;
	deUint32					sourceSize	= 0;
	deUint32					allocSize	= 0;

	if (m_params.topology == vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
	{
		//-1 / -1 / 0 / -1 / -1 / 0
		pointTemplate.push_back	(-p);
		pointTemplate.push_back	(-p);
		pointTemplate.push_back		(0.0f);
		pointTemplate.push_back		(-p);
		pointTemplate.push_back			(-p);
		pointTemplate.push_back			(0.0f);
		if (!m_robustness2)
		{
			pointTemplate.push_back(0.0f);
			pointTemplate.push_back(0.0f);
			// Beyonds do not matter
			sourceSize	= 4;
			allocSize	= 4;
			returnSize	= 4; // or WHOLE_SIZE
		}
		else
		{
			pointTemplate.push_back(+p); // those should be read as (0,0)
			pointTemplate.push_back(+p);

			switch (m_params.beyondType)
			{
			case BeyondType::BUFFER:
				sourceSize	= 3;
				allocSize	= 3;
				returnSize	= 3;
				break;
			case BeyondType::SIZE:
				DE_ASSERT(m_params.wholeSize == false);
				sourceSize	= 4;
				allocSize	= 4;
				returnSize	= 3;
				break;
			}
		}
	}
	else if (m_params.topology == vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
	{
		// -1/0/ -1/-1 /0/-1 /-1/0 /0/-1
		pointTemplate.push_back	(-p);
		pointTemplate.push_back	(0.0f);
		pointTemplate.push_back		(-p);
		pointTemplate.push_back		(-p);
		pointTemplate.push_back			(0.0);
		pointTemplate.push_back			(-p);
		pointTemplate.push_back	(-p);
		pointTemplate.push_back	(0.0f);
		pointTemplate.push_back		(0.0f);
		pointTemplate.push_back		(-p);
		if (!m_robustness2)
		{
			pointTemplate.push_back(0.0f);
			pointTemplate.push_back(0.0f);
			// Beyonds do not matter
			sourceSize	= 6;
			allocSize	= 6;
			returnSize	= 6; // or WHOLE_SIZE
		}
		else
		{
			// those should be read as (0,0)
			pointTemplate.push_back(+p);
			pointTemplate.push_back(+p);

			switch (m_params.beyondType)
			{
			case BeyondType::BUFFER:
				sourceSize	= 5;
				allocSize	= 5;
				returnSize	= 5;
				break;
			case BeyondType::SIZE:
				sourceSize	= 6;
				allocSize	= 6;
				returnSize	= 5;
				break;
			}
		}
	}
	else
	{
		DE_ASSERT(0);
	}
	DE_ASSERT((allocSize != 0) && (allocSize >= sourceSize));

	const std::vector<float>&	source		= pointTemplate;

	std::vector<tcu::Vec3> colorTemplate(7);
	for (int i = 1; i <= 7; ++i)
	{
		colorTemplate[i - 1] = {
			i & 0x1 ? 1.0f : 0.6f,
			i & 0x2 ? 1.0f : 0.6f,
			i & 0x4 ? 1.0f : 0.6f
		};
	}
	std::vector<float> colors(sourceSize * 3);
	for (deUint32 i = 0; i < sourceSize; ++i)
	{
		const tcu::Vec3& c = colorTemplate[i % colorTemplate.size()];
		colors[3 * i + 0] = c.x();
		colors[3 * i + 1] = c.y();
		colors[3 * i + 2] = c.z();
	}
	vk::VkDeviceSize clrSize = allocSize * 3 * sizeof(float);
	const vk::VkBufferCreateInfo clrCreateInfo = vk::makeBufferCreateInfo(clrSize, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	de::SharedPtr<vk::BufferWithMemory> clrBuffer(new vk::BufferWithMemory(vk, device, allocator, clrCreateInfo, vk::MemoryRequirement::HostVisible));
	copyAndFlush(vk, device, *clrBuffer, 0, colors.data(), deUint32(colors.size() * sizeof(float)));
	buffers.push_back(clrBuffer);

	sizes.resize(m_params.bufferCount);
	sizes[0] = m_params.wholeSize ? VK_WHOLE_SIZE : (returnSize * 3 * sizeof(float));

	offsets.resize(m_params.bufferCount);
	strides.resize(m_params.bufferCount);

	// random offsets multiplyied later by 4, special value 0 for no-offset
	offsets[0] = 0;
	for (deUint32 i = 1; i < m_params.bufferCount; ++i)
	{
		auto nextOffset = [&]() {
			vk::VkDeviceSize offset = rnd.getUint64() % 30;
			while (offset == 0)
				offset = rnd.getUint64() % 30;
			return offset;
		};
		offsets[i] = (m_params.rndSeed == 0) ? vk::VkDeviceSize(0) : nextOffset();
	}

	// random strides multiplyied later by 4, special value for atributes stride
	strides[0] = { sizeof(tcu::Vec3) };
	for (deUint32 i = 1; i < m_params.bufferCount; ++i)
	{
		auto nextStride = [&]() {
			vk::VkDeviceSize stride = rnd.getUint64() % 30;
			while (stride == 0)
				stride = rnd.getUint64() % 30;
			return stride;
		};
		strides[i] = (m_params.rndSeed == 0) ? vk::VkDeviceSize(0) : nextStride();
	}

	for (deUint32 i = 1; i < m_params.bufferCount; ++i)
	{
		const deUint32		stride	= deUint32(strides[i]);
		const deUint32		offset	= deUint32(offsets[i]);
		std::vector<float>	points	(offset + sourceSize * (compCount + stride));

		for (deUint32 j = 0; j < offset; ++j)
		{
			points[j] = float(i * 13) + 0.234f;
		}
		for (uint32_t j = 0; j < sourceSize; ++j)
		{
			auto k = offset + j * (compCount + stride);
			points[k + 0] = source[j * compCount + 0];
			points[k + 1] = source[j * compCount + 1];
			for (uint32_t s = 0; s < stride; ++s)
			{
				points[k + compCount + s] = float(i * 19) + 0.543f;
			}
		}

		vk::VkDeviceSize size = (offset + allocSize * (compCount + stride)) * sizeof(float);
		const vk::VkBufferCreateInfo createInfo = vk::makeBufferCreateInfo(size, vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		de::SharedPtr<vk::BufferWithMemory> buffer(new vk::BufferWithMemory(vk, device, allocator, createInfo, vk::MemoryRequirement::HostVisible));
		copyAndFlush(vk, device, *buffer, 0, points.data(), deUint32(points.size() * sizeof(float)));

		sizes[i]	= m_params.wholeSize ? VK_WHOLE_SIZE : ((compCount + stride) * returnSize * sizeof(float));
		strides[i]	= (compCount + stride) * sizeof(float);
		offsets[i]	= offset * sizeof(float);
		buffers.push_back(buffer);
	}

	return buffers;
}

template<class X> struct collection_element { };
template<template<class, class...> class coll__, class X, class... Y>
	struct collection_element<coll__<X, Y...>> { typedef X type; };
template<class coll__> using collection_element_t = typename collection_element<coll__>::type;

tcu::TestStatus BindVertexBuffers2Instance::iterate (void)
{
	const vk::DeviceInterface&				vk					= getDeviceInterface();
	const vk::VkDevice						device				= getDevice();
	const deUint32							queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	const vk::VkQueue						queue				= getQueue();
	vk::Allocator&							allocator			= getAllocator();
	tcu::TestLog&							log					= m_context.getTestContext().getLog();

	const vk::VkExtent2D					extent				{ m_params.width, m_params.height };
	const vk::VkImageSubresourceRange		colorSubresRange	= vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const vk::VkFormat						colorFormat			= vk::VK_FORMAT_R32G32B32A32_SFLOAT;
	const vk::Move<vk::VkImage>				colorImage			= makeImage(vk, device, makeImageCreateInfo(extent, colorFormat, vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
	const de::MovePtr<vk::Allocation>		colorImageAlloc		= bindImage(vk, device, allocator, *colorImage, vk::MemoryRequirement::Any);
	const vk::Move<vk::VkImageView>			colorImageView		= makeImageView(vk, device, *colorImage, vk::VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSubresRange);
	vk::RenderPassWrapper					renderPass			(m_pipelineConstructionType, vk, device, colorFormat);
	renderPass.createFramebuffer(vk, device, colorImage.get(), colorImageView.get(), extent.width, extent.height);
	const vk::VkPipelineLayoutCreateInfo	pipelineLayoutInfo	= vk::initVulkanStructure();
	const vk::PipelineLayoutWrapper			pipelineLayout(m_pipelineConstructionType, vk, device, &pipelineLayoutInfo, nullptr);

	const vk::VkClearValue					clearColorValue		= vk::makeClearValueColor(tcu::Vec4(0.5f));
	const vk::Move<vk::VkCommandPool>		cmdPool				= createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
	const vk::Move<vk::VkCommandBuffer>		cmdBuffer			= allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	Sizes									offsets;
	Sizes									strides;
	Sizes									sizes;
	Buffers									buffers				= createBuffers(offsets, strides, sizes);
	std::vector<vk::VkBuffer>				vkBuffers			(buffers.size());
	std::transform(buffers.begin(), buffers.end(), vkBuffers.begin(), [](collection_element_t<decltype(buffers)> buffer) { return **buffer; });

	deUint32								vertexCount			= 0;
	switch (m_params.topology)
	{
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
			vertexCount = 4;
			break;
		case vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
			vertexCount = 6;
			break;
		default:	DE_ASSERT(0);
			break;
	};

	std::unique_ptr<vk::BufferWithMemory>	outBuffer			= makeBufferForImage(vk, device, allocator, mapVkFormat(colorFormat), extent);
	vk::Allocation&							outBufferAlloc		= outBuffer->getAllocation();

	createPipeline(pipelineLayout, *renderPass);

	beginCommandBuffer(vk, *cmdBuffer);
		renderPass.begin(vk, *cmdBuffer, vk::makeRect2D(0, 0, extent.width, extent.height), 1u, &clearColorValue);
		m_pipelineWrapper.bind(*cmdBuffer);
#ifndef CTS_USES_VULKANSC
		vk.cmdBindVertexBuffers2(*cmdBuffer, 0, m_params.bufferCount, vkBuffers.data(), offsets.data(), sizes.data(), strides.data());
#else
		vk.cmdBindVertexBuffers2EXT(*cmdBuffer, 0, m_params.bufferCount, vkBuffers.data(), offsets.data(), sizes.data(), strides.data());
#endif
		vk.cmdDraw(*cmdBuffer, vertexCount, 1, 0, 0);
		renderPass.end(vk, *cmdBuffer);
		vk::copyImageToBuffer(vk, *cmdBuffer, *colorImage, **outBuffer, tcu::IVec2(extent.width, extent.height));
	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	invalidateAlloc(vk, device, outBufferAlloc);
	const tcu::ConstPixelBufferAccess result(vk::mapVkFormat(colorFormat), extent.width, extent.height, 1, outBufferAlloc.getHostPtr());

	bool		testPasses		= false;
	deUint32	equalClearCount = 0;
	deUint32	halfWidth		= m_params.width / 2;
	deUint32	halfHeight		= m_params.height / 2;

	for (deUint32 Y = 0; (Y < halfHeight); ++Y)
	for (deUint32 X = 0; (X < halfWidth); ++X)
	{
		const tcu::Vec4 px = result.getPixel(X, Y);
		if (	px.x() == clearColorValue.color.float32[0]
			&&	px.y() == clearColorValue.color.float32[1]
			&&	px.z() == clearColorValue.color.float32[2])
		{
			equalClearCount = equalClearCount + 1;
		}
	}
	const double mismatch = double(equalClearCount) / double(halfWidth * halfHeight);
	const std::string mismatchText = "Mismatch: " + std::to_string(deUint32(mismatch * 100.9)) + '%';

	const float			eps				= 0.2f;
	const tcu::Vec3		threshold		(eps, eps, eps);
	const tcu::UVec2	middle			(halfWidth - 1u, halfHeight - 1u);
	const tcu::Vec4		rgba			= result.getPixel(middle.x(), middle.y());
	const tcu::Vec3		rgb				= rgba.swizzle(0, 1, 2);
	const bool			belowThreshold	= tcu::boolAll(tcu::lessThan(rgb, threshold));

	if (!m_robustness2)
	{
		const auto expectedMismatch = 0.0;
		testPasses = (belowThreshold == false) && (mismatch == expectedMismatch);
		if (!testPasses)
		{
			std::ostringstream msg;
			msg << "FAILURE: no robustness; pixel at " << middle << " is " << rgb << " (should be >= "
			    << threshold << "); mismatch in upper left quarter " << mismatch << " (should be " << expectedMismatch << ")";
			log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
		}
	}
	else
	{
		const auto mismatchLimit = 0.25;
		testPasses = (belowThreshold == true) && (mismatch < mismatchLimit);
		if (!testPasses)
		{
			std::ostringstream msg;
			msg << "FAILURE: robustness2; pixel at " << middle << " is " << rgb
			    << " (should be < " << threshold << "); mismatch in upper left quarter " << mismatch
				<< " (should be below " << mismatchLimit << ")";
			log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
		}
	}

	auto logOffsets = (log << tcu::TestLog::Message << "Offsets: ");
	for (deUint32 k = 0; k < m_params.bufferCount; ++k) {
		if (k) logOffsets << ", ";
		logOffsets << offsets[k];
	} logOffsets << tcu::TestLog::EndMessage;

	auto logSizes = (log << tcu::TestLog::Message << "Sizes: ");
	for (deUint32 k = 0; k < m_params.bufferCount; ++k) {
		if (k) logSizes << ", ";
		logSizes << ((sizes[k] == VK_WHOLE_SIZE) ? "WHOLE_SIZE" : std::to_string(sizes[k]).c_str());
	} logSizes << tcu::TestLog::EndMessage;

	auto logStrides = (log << tcu::TestLog::Message << "Strides: ");
	for (deUint32 k = 0; k < m_params.bufferCount; ++k) {
		if (k) logStrides << ", ";
		logStrides << strides[k];
	} logStrides << tcu::TestLog::EndMessage;

	if (!testPasses)
	{
		std::ostringstream os;
		os << (m_params.topology == vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP ? "list" : "strip");
		os << ".buffs" << m_params.bufferCount;
		os << (m_params.wholeSize ? ".whole_size" : ".true_size");
		if (m_robustness2)
		{
			os << ".robust";
			os << (m_params.beyondType == BeyondType::BUFFER ? ".over_buff" : ".over_size");
		}
		os.flush();

		log << tcu::TestLog::ImageSet("Result", "")
			<< tcu::TestLog::Image(os.str(), "", result)
			<< tcu::TestLog::EndImageSet;
	}

	if (!testPasses)
		return tcu::TestStatus::fail(mismatchText + "; check log for details");
	return tcu::TestStatus::pass(mismatchText);
}

class BindBuffers2Case : public vkt::TestCase
{
public:
					BindBuffers2Case	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const vk::PipelineConstructionType	pipelineConstructionType, const TestParams params, const bool singleBind, const deUint32 count)
						: vkt::TestCase					(testCtx, name, description)
						, m_pipelineConstructionType	(pipelineConstructionType)
						, m_params						(params)
						, m_singleBind					(singleBind)
						, m_count						(count)
						{}
	virtual			~BindBuffers2Case	(void) {}

	void			checkSupport			(vkt::Context& context) const override;
	virtual void	initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override { return new BindBuffers2Instance(context, m_pipelineConstructionType, m_params, m_singleBind, m_count); }

private:
	const vk::PipelineConstructionType	m_pipelineConstructionType;
	const TestParams					m_params;
	const bool							m_singleBind;
	const deUint32						m_count;
};

void BindBuffers2Case::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_extended_dynamic_state");

	vk::checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_pipelineConstructionType);
}

void BindBuffers2Case::initPrograms(vk::SourceCollections& programCollection) const
{
	std::stringstream vert;
	std::stringstream frag;

	std::string inputs;
	std::string combined;
	if (m_count == 2)
	{
		inputs =
			"layout (location=0) in vec2 rg;\n"
			"layout (location=1) in vec2 xy;\n"
			"layout (location=2) in vec2 ba;\n"
			"layout (location=3) in vec2 zw;\n";
		combined =
			"    vec4 vertex = vec4(xy, zw);\n"
			"    vec4 color = vec4(rg, ba);\n";
	}
	else if (m_count == 3)
	{
		inputs =
			"layout (location=0) in vec2 rg;\n"
			"layout (location=1) in vec2 xy;\n"
			"layout (location=2) in float b;\n"
			"layout (location=3) in float z;\n"
			"layout (location=4) in float a;\n"
			"layout (location=5) in float w;\n";
		combined =
			"    vec4 vertex = vec4(xy, z, w);\n"
			"    vec4 color = vec4(rg, b, a);\n";
	}
	else if (m_count == 4)
	{
		inputs =
			"layout (location=0) in float r;\n"
			"layout (location=1) in float x;\n"
			"layout (location=2) in float g;\n"
			"layout (location=3) in float y;\n"
			"layout (location=4) in float b;\n"
			"layout (location=5) in float z;\n"
			"layout (location=6) in float a;\n"
			"layout (location=7) in float w;\n";
		combined =
			"    vec4 vertex = vec4(x, y, z, w);\n"
			"    vec4 color = vec4(r, g, b, a);\n";
	}
	else
	{
		inputs =
			"layout (location=0) in vec4 rgba;\n"
			"layout (location=1) in vec4 xyzw;\n";
		combined =
			"    vec4 vertex = vec4(xyzw);\n"
			"    vec4 color = vec4(rgba);\n";
	}

	vert
		<< "#version 450\n"
		<< inputs
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main() {\n"
		<< "    vec2 pos = vec2(-float(gl_InstanceIndex & 1), -float((gl_InstanceIndex >> 1) & 1));\n"
		<< combined
		<< "    gl_Position = vertex + vec4(pos, 0.0f, 1.0f);\n"
		<< "    outColor = color;\n"
		<< "}\n";

	frag
		<< "#version 450\n"
		<< "layout (location=0) in vec4 inColor;\n"
		<< "layout (location=0) out vec4 outColor;\n"
		<< "void main() {\n"
		<< "    outColor = inColor;\n"
		<< "}\n";

	programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

class BindVertexBuffers2Case : public vkt::TestCase
{
public:
					BindVertexBuffers2Case	(tcu::TestContext&				testCtx,
											 const std::string&				name,
											 vk::PipelineConstructionType	pipelineConstructionType,
											 const TestParamsMaint5&		params,
											 bool							robustness2)
						: vkt::TestCase(testCtx, name, std::string())
						, m_pipelineConstructionType(pipelineConstructionType)
						, m_params(params)
						, m_robustness2(robustness2) { }
	virtual			~BindVertexBuffers2Case	(void) = default;

	void			checkSupport			(vkt::Context&					context) const override;
	virtual void	initPrograms			(vk::SourceCollections&			programCollection) const override;
	TestInstance*	createInstance			(Context&						context) const override;

private:
	const vk::PipelineConstructionType	m_pipelineConstructionType;
	const TestParamsMaint5				m_params;
	const bool							m_robustness2;
};

void BindVertexBuffers2Case::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_EXT_extended_dynamic_state");

#ifndef CTS_USES_VULKANSC
	context.requireDeviceFunctionality(VK_KHR_MAINTENANCE_5_EXTENSION_NAME);
#endif // CTS_USES_VULKANSC

	if (m_robustness2)
	{
		vk::VkPhysicalDeviceFeatures2 features2 = vk::initVulkanStructure();
		context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);
		if (!features2.features.robustBufferAccess)
			TCU_THROW(NotSupportedError, "robustBufferAccess not supported by this implementation");
		context.requireDeviceFunctionality("VK_EXT_robustness2");
	}

	vk::checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_pipelineConstructionType);
}

void BindVertexBuffers2Case::initPrograms (vk::SourceCollections& programCollection) const
{
	std::ostringstream	vert;
	vert << "#version 450\n";
	vert << "layout(location = 0) in vec3 in_color;\n";
	for (deUint32 i = 1; i < m_params.bufferCount; ++i)
		vert << "layout(location = " << i << ") in vec2 pos" << i << ";\n";
	vert << "layout(location = 0) out vec3 out_color;\n";
	vert << "void main() {\n";
	vert << "  gl_Position = vec4(";
	for (deUint32 i = 1; i < m_params.bufferCount; ++i)
	{
		if (i > 1) vert << '+';
		vert << "pos" << i;
	}
	vert << ", 0.0, 1.0);\n";
	vert << "  out_color = in_color;\n";
	vert << "}\n";
	vert.flush();

	const std::string frag(
		"#version 450\n"
		"layout (location = 0) in  vec3 in_color;\n"
		"layout (location = 0) out vec4 out_color;\n"
		"void main() {\n"
		"    out_color = vec4(in_color, 1.0);\n"
		"}\n"
	);

	programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
	programCollection.glslSources.add("frag") << glu::FragmentSource(frag);
}

TestInstance* BindVertexBuffers2Case::createInstance (Context& context) const
{
	DevicePtr		device;
	DeviceDriverPtr	driver;

	if (m_robustness2)
	{
		vk::VkPhysicalDeviceFeatures2							features2				= vk::initVulkanStructure();
		vk::VkPhysicalDeviceRobustness2FeaturesEXT				robustness2Features		= vk::initVulkanStructure();
#ifndef CTS_USES_VULKANSC
		vk::VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT	gplFeatures				= vk::initVulkanStructure();
		vk::VkPhysicalDeviceShaderObjectFeaturesEXT				shaderObjectFeatures = vk::initVulkanStructure();
#endif // CTS_USES_VULKANSC

		features2.features.robustBufferAccess		= VK_TRUE;
		robustness2Features.robustBufferAccess2		= VK_TRUE;
#ifndef CTS_USES_VULKANSC
		gplFeatures.graphicsPipelineLibrary			= VK_TRUE;
		shaderObjectFeatures.shaderObject			= VK_TRUE;
#endif // CTS_USES_VULKANSC

		const auto addFeatures = vk::makeStructChainAdder(&features2);
		addFeatures(&robustness2Features);

#ifndef CTS_USES_VULKANSC
		if (vk::isConstructionTypeLibrary(m_pipelineConstructionType))
			addFeatures(&gplFeatures);
		else if (vk::isConstructionTypeShaderObject(m_pipelineConstructionType))
			addFeatures(&shaderObjectFeatures);
#else
		TCU_THROW(NotSupportedError, "VulkanSC does not support VK_EXT_graphics_pipeline_library");
#endif // CTS_USES_VULKANSC

		device = createRobustBufferAccessDevice(context, &features2);
		driver =
#ifndef CTS_USES_VULKANSC
			DeviceDriverPtr(new vk::DeviceDriver(context.getPlatformInterface(), context.getInstance(), *device, context.getUsedApiVersion()));
#else
			DeviceDriverPtr(new DeviceDriverSC(context.getPlatformInterface(), context.getInstance(), *device, context.getTestContext().getCommandLine(),
				context.getResourceInterface(), context.getDeviceVulkanSC10Properties(), context.getDeviceProperties(), context.getUsedApiVersion()),
				vk::DeinitDeviceDeleter(context.getResourceInterface().get(), *device));
#endif // CTS_USES_VULKANSC
	}

	return (new BindVertexBuffers2Instance(context, driver, device, m_pipelineConstructionType, m_params, m_robustness2));
}

tcu::TestCaseGroup* createCmdBindVertexBuffers2Tests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType);
tcu::TestCaseGroup* createCmdBindBuffers2Tests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType)
{
	de::MovePtr<tcu::TestCaseGroup> cmdBindBuffers2Group(new tcu::TestCaseGroup(testCtx, "bind_buffers_2", ""));

	const struct
	{
		TestParams	params;
		const char* name;
	} strideTests[] =
	{
		// Values are multiplied by sizeof(float) in the test
		{ {0u,		4u,		0u,		0u,		},	"stride_0_4_offset_0_0"		},
		{ {0u,		4u,		1u,		0u,		},	"stride_0_4_offset_1_0"		},
		{ {4u,		4u,		0u,		0u,		},	"stride_4_4_offset_0_0"		},
		{ {5u,		5u,		0u,		7u,		},	"stride_5_5_offset_0_7"		},
		{ {5u,		8u,		15u,	22u,	},	"stride_5_8_offset_15_22"	},
		{ {7u,		22u,	100u,	0u,		},	"stride_7_22_offset_100_0"	},
		{ {40u,		28u,	0u,		0u,		},	"stride_40_28_offset_0_0"	},
	};

	const struct
	{
		bool		singleBind;
		const char* name;
	} bindTests[] =
	{
		// Values are multiplied by sizeof(float) in the test
		{ true,		"single"		},
		{ false,	"separate"		},
	};

	const struct
	{
		deUint32	count;
		const char* name;
	} countTests[] =
	{
		{ 1,		"count_1"		},
		{ 2,		"count_2"		},
		{ 3,		"count_3"		},
		{ 4,		"count_4"		},
	};

	for (const auto bindTest : bindTests)
	{
		de::MovePtr<tcu::TestCaseGroup> bindGroup(new tcu::TestCaseGroup(testCtx, bindTest.name, ""));
		for (const auto& strideTest : strideTests)
		{
			de::MovePtr<tcu::TestCaseGroup> typeGroup(new tcu::TestCaseGroup(testCtx, strideTest.name, ""));
			for (const auto& countTest : countTests)
			{
				typeGroup->addChild(new BindBuffers2Case(testCtx, countTest.name, "", pipelineConstructionType, strideTest.params, bindTest.singleBind, countTest.count));
			}
			bindGroup->addChild(typeGroup.release());
		}
		cmdBindBuffers2Group->addChild(bindGroup.release());
	}

#ifndef CTS_USES_VULKANSC
	cmdBindBuffers2Group->addChild(createCmdBindVertexBuffers2Tests(testCtx, pipelineConstructionType));
#endif // CTS_USES_VULKANSC

	return cmdBindBuffers2Group.release();
}

tcu::TestCaseGroup* createCmdBindVertexBuffers2Tests (tcu::TestContext& testCtx, vk::PipelineConstructionType pipelineConstructionType)
{
	const deUint32	counts[]		{ 5, 9 };
	const deUint32	randoms[]		{ 321, 432 };
	const deUint32	robustRandoms[]	{ 543, 654 };
	const std::pair<bool, const char*> sizes[] {
		{ true,   "whole_size"	},
		{ false,  "true_size"	}
	};
	const std::pair<BeyondType, const char*> beyondTypes[] {
		{ BeyondType::BUFFER,	"beyond_buffer"	},
		{ BeyondType::SIZE,		"beyond_size"	}
	};
	const std::pair<vk::VkPrimitiveTopology, const char*> topos[] {
		{ vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	"triangle_list"		},
		{ vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,	"triangle_strip"	},
	};

	std::string name;
	const deUint32 defaultWidth = 32;
	const deUint32 defaultHeight = 32;

	de::MovePtr<tcu::TestCaseGroup> rootGroup(new tcu::TestCaseGroup(testCtx, "maintenance5", ""));

	for (const auto& topo : topos)
	{
		de::MovePtr<tcu::TestCaseGroup> topoGroup(new tcu::TestCaseGroup(testCtx, topo.second, ""));

		for (deUint32 count : counts)
		{
			name = "buffers" + std::to_string(count);
			de::MovePtr<tcu::TestCaseGroup> countGroup(new tcu::TestCaseGroup(testCtx, name.c_str(), ""));

			for (deUint32 random : randoms)
			{
				name = "stride_offset_rnd" + std::to_string(random);
				de::MovePtr<tcu::TestCaseGroup> randomGroup(new tcu::TestCaseGroup(testCtx, name.c_str(), ""));

				for (const auto& size : sizes)
				{
					TestParamsMaint5 p;
					p.width			= defaultWidth;
					p.height		= defaultHeight;
					p.topology		= topo.first;
					p.wholeSize		= size.first;
					p.rndSeed		= random;
					p.bufferCount	= count;
					p.beyondType	= BeyondType::BUFFER;

					randomGroup->addChild(new BindVertexBuffers2Case(testCtx, size.second, pipelineConstructionType, p, false));
				}
				countGroup->addChild(randomGroup.release());
			}
			topoGroup->addChild(countGroup.release());
		}
		rootGroup->addChild(topoGroup.release());
	}

	de::MovePtr<tcu::TestCaseGroup> robustGroup(new tcu::TestCaseGroup(testCtx, "robustness2", ""));
	for (const auto& topo : topos)
	{
		de::MovePtr<tcu::TestCaseGroup> topoGroup(new tcu::TestCaseGroup(testCtx, topo.second, ""));

		for (deUint32 count : counts)
		{
			name = "buffers" + std::to_string(count);
			de::MovePtr<tcu::TestCaseGroup> countGroup(new tcu::TestCaseGroup(testCtx, name.c_str(), ""));

			for (deUint32 random : robustRandoms)
			{
				name = "stride_offset_rnd" + std::to_string(random);
				de::MovePtr<tcu::TestCaseGroup> randomGroup(new tcu::TestCaseGroup(testCtx, name.c_str(), ""));

				for (const auto& size : sizes)
				{
					de::MovePtr<tcu::TestCaseGroup> sizeGroup(new tcu::TestCaseGroup(testCtx, size.second, ""));

					TestParamsMaint5 p;
					p.width			= defaultWidth;
					p.height		= defaultHeight;
					p.topology		= topo.first;
					p.wholeSize		= size.first;
					p.rndSeed		= random;
					p.bufferCount	= count;

					if (p.wholeSize)
					{
						p.beyondType = BeyondType::BUFFER;
						auto beyondType = std::find_if(std::begin(beyondTypes), std::end(beyondTypes),
							[&](const std::pair<BeyondType, const char*>& b) { return b.first == p.beyondType; });
						sizeGroup->addChild(new BindVertexBuffers2Case(testCtx, beyondType->second, pipelineConstructionType, p, true));
					}
					else
					{
						for (const auto& beyondType : beyondTypes)
						{
							p.beyondType = beyondType.first;
							sizeGroup->addChild(new BindVertexBuffers2Case(testCtx, beyondType.second, pipelineConstructionType, p, true));
						}
					}
					randomGroup->addChild(sizeGroup.release());
				}
				countGroup->addChild(randomGroup.release());
			}
			topoGroup->addChild(countGroup.release());
		}
		robustGroup->addChild(topoGroup.release());
	}
	rootGroup->addChild(robustGroup.release());

	return rootGroup.release();
}

} // pipeline
} // vkt
