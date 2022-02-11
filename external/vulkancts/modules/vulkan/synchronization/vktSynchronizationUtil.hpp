#ifndef _VKTSYNCHRONIZATIONUTIL_HPP
#define _VKTSYNCHRONIZATIONUTIL_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Synchronization tests utilities
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkPrograms.hpp"
#include "tcuVector.hpp"
#include "deMutex.hpp"
#include <memory>

namespace vkt
{
namespace synchronization
{

enum class SynchronizationType
{
	LEGACY				= 0,
	SYNCHRONIZATION2,
};

class Buffer
{
public:
										Buffer			(const vk::DeviceInterface&		vk,
														 const vk::VkDevice				device,
														 vk::Allocator&					allocator,
														 const vk::VkBufferCreateInfo&	bufferCreateInfo,
														 const vk::MemoryRequirement	memoryRequirement)
		: m_buffer		(createBuffer(vk, device, &bufferCreateInfo))
		, m_allocation	(allocator.allocate(getBufferMemoryRequirements(vk, device, *m_buffer), memoryRequirement))
	{
		VK_CHECK(vk.bindBufferMemory(device, *m_buffer, m_allocation->getMemory(), m_allocation->getOffset()));
	}

										Buffer			(vk::Move<vk::VkBuffer>			buffer,
														 de::MovePtr<vk::Allocation>	allocation)
		: m_buffer		(buffer)
		, m_allocation	(allocation)
	{
	}

	const vk::VkBuffer&					get				(void) const { return *m_buffer; }
	const vk::VkBuffer&					operator*		(void) const { return get(); }
	vk::Allocation&						getAllocation	(void) const { return *m_allocation; }

private:
	const vk::Unique<vk::VkBuffer>		m_buffer;
	const de::UniquePtr<vk::Allocation>	m_allocation;

	// "deleted"
										Buffer			(const Buffer&);
	Buffer&								operator=		(const Buffer&);
};

class Image
{
public:
										Image			(const vk::DeviceInterface&		vk,
														 const vk::VkDevice				device,
														 vk::Allocator&					allocator,
														 const vk::VkImageCreateInfo&	imageCreateInfo,
														 const vk::MemoryRequirement	memoryRequirement)
		: m_image		(createImage(vk, device, &imageCreateInfo))
		, m_allocation	(allocator.allocate(getImageMemoryRequirements(vk, device, *m_image), memoryRequirement))
	{
		VK_CHECK(vk.bindImageMemory(device, *m_image, m_allocation->getMemory(), m_allocation->getOffset()));
	}
										Image			(vk::Move<vk::VkImage>&			image,
														 de::MovePtr<vk::Allocation>&	allocation)
		: m_image		(image)
		, m_allocation	(allocation)
	{
	}

	const vk::VkImage&					get				(void) const { return *m_image; }
	const vk::VkImage&					operator*		(void) const { return get(); }
	vk::Allocation&						getAllocation	(void) const { return *m_allocation; }

private:
	const vk::Unique<vk::VkImage>		m_image;
	const de::UniquePtr<vk::Allocation>	m_allocation;

	// "deleted"
										Image			(const Image&);
	Image&								operator=		(const Image&);
};

class PipelineCacheData
{
public:
									PipelineCacheData		(void);
									~PipelineCacheData		(void);

	vk::Move<vk::VkPipelineCache>	createPipelineCache		(const vk::DeviceInterface& vk, const vk::VkDevice device) const;
	void							setFromPipelineCache	(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkPipelineCache pipelineCache);

private:
	mutable de::Mutex				m_lock;
	std::vector<deUint8>			m_data;
};

class GraphicsPipelineBuilder
{
public:
								GraphicsPipelineBuilder	(void) : m_renderSize			(0, 0)
															   , m_shaderStageFlags		(0u)
															   , m_cullModeFlags		(vk::VK_CULL_MODE_NONE)
															   , m_frontFace			(vk::VK_FRONT_FACE_COUNTER_CLOCKWISE)
															   , m_patchControlPoints	(1u)
															   , m_blendEnable			(false)
															   , m_primitiveTopology	(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) {}

	GraphicsPipelineBuilder&	setRenderSize					(const tcu::IVec2& size) { m_renderSize = size; return *this; }
	GraphicsPipelineBuilder&	setShader						(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkShaderStageFlagBits stage, const vk::ProgramBinary& binary, const vk::VkSpecializationInfo* specInfo);
	GraphicsPipelineBuilder&	setPatchControlPoints			(const deUint32 controlPoints) { m_patchControlPoints = controlPoints; return *this; }
	GraphicsPipelineBuilder&	setCullModeFlags				(const vk::VkCullModeFlags cullModeFlags) { m_cullModeFlags = cullModeFlags; return *this; }
	GraphicsPipelineBuilder&	setFrontFace					(const vk::VkFrontFace frontFace) { m_frontFace = frontFace; return *this; }
	GraphicsPipelineBuilder&	setBlend						(const bool enable) { m_blendEnable = enable; return *this; }

	//! Applies only to pipelines without tessellation shaders.
	GraphicsPipelineBuilder&	setPrimitiveTopology			(const vk::VkPrimitiveTopology topology) { m_primitiveTopology = topology; return *this; }

	GraphicsPipelineBuilder&	addVertexBinding				(const vk::VkVertexInputBindingDescription vertexBinding) { m_vertexInputBindings.push_back(vertexBinding); return *this; }
	GraphicsPipelineBuilder&	addVertexAttribute				(const vk::VkVertexInputAttributeDescription vertexAttribute) { m_vertexInputAttributes.push_back(vertexAttribute); return *this; }

	//! Basic vertex input configuration (uses biding 0, location 0, etc.)
	GraphicsPipelineBuilder&	setVertexInputSingleAttribute	(const vk::VkFormat vertexFormat, const deUint32 stride);

	vk::Move<vk::VkPipeline>	build							(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkPipelineLayout pipelineLayout, const vk::VkRenderPass renderPass, PipelineCacheData& pipelineCacheData);

private:
	tcu::IVec2											m_renderSize;
	vk::Move<vk::VkShaderModule>						m_vertexShaderModule;
	vk::Move<vk::VkShaderModule>						m_fragmentShaderModule;
	vk::Move<vk::VkShaderModule>						m_geometryShaderModule;
	vk::Move<vk::VkShaderModule>						m_tessControlShaderModule;
	vk::Move<vk::VkShaderModule>						m_tessEvaluationShaderModule;
	std::vector<vk::VkPipelineShaderStageCreateInfo>	m_shaderStages;
	std::vector<vk::VkVertexInputBindingDescription>	m_vertexInputBindings;
	std::vector<vk::VkVertexInputAttributeDescription>	m_vertexInputAttributes;
	vk::VkShaderStageFlags								m_shaderStageFlags;
	vk::VkCullModeFlags									m_cullModeFlags;
	vk::VkFrontFace										m_frontFace;
	deUint32											m_patchControlPoints;
	bool												m_blendEnable;
	vk::VkPrimitiveTopology								m_primitiveTopology;

	GraphicsPipelineBuilder (const GraphicsPipelineBuilder&); // "deleted"
	GraphicsPipelineBuilder& operator= (const GraphicsPipelineBuilder&);
};

// Base class that abstracts over legacy synchronization and synchronization changes
// introduced with VK_KHR_synchronization2 extension. Since structures in
// VK_KHR_synchronization2 have more features this wrapper uses them and when legacy
// implementation is used in tests then data from new structures is used to fill legacy ones.
class SynchronizationWrapperBase
{
public:
	SynchronizationWrapperBase(const vk::DeviceInterface& vk)
		: m_vk(vk)
	{}

	virtual ~SynchronizationWrapperBase() = default;

	virtual void			addSubmitInfo		(deUint32									waitSemaphoreInfoCount,
												 const vk::VkSemaphoreSubmitInfo*			pWaitSemaphoreInfos,
												 deUint32									commandBufferInfoCount,
												 const vk::VkCommandBufferSubmitInfo*		pCommandBufferInfos,
												 deUint32									signalSemaphoreInfoCount,
												 const vk::VkSemaphoreSubmitInfo*			pSignalSemaphoreInfos,
												 bool										usingWaitTimelineSemaphore = DE_FALSE,
												 bool										usingSignalTimelineSemaphore = DE_FALSE) = 0;

	virtual void			cmdPipelineBarrier	(vk::VkCommandBuffer						commandBuffer,
												 const vk::VkDependencyInfo*				pDependencyInfo) const = 0;

	virtual void			cmdSetEvent			(vk::VkCommandBuffer						commandBuffer,
												 vk::VkEvent								event,
												 const vk::VkDependencyInfo*				pDependencyInfo) const = 0;
	virtual void			cmdResetEvent		(vk::VkCommandBuffer						commandBuffer,
												 vk::VkEvent								event,
												 vk::VkPipelineStageFlags2					flag) const = 0;
	virtual void			cmdWaitEvents		(vk::VkCommandBuffer						commandBuffer,
												 deUint32									eventCount,
												 const vk::VkEvent*							pEvents,
												 const vk::VkDependencyInfo*				pDependencyInfo) const = 0;

	virtual vk::VkResult	queueSubmit			(vk::VkQueue								queue,
												 vk::VkFence								fence) = 0;

protected:
	const vk::DeviceInterface& m_vk;
};

enum FeatureFlagBits
{
	FEATURE_TESSELLATION_SHADER							= 1u << 0,
	FEATURE_GEOMETRY_SHADER								= 1u << 1,
	FEATURE_SHADER_FLOAT_64								= 1u << 2,
	FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS			= 1u << 3,
	FEATURE_FRAGMENT_STORES_AND_ATOMICS					= 1u << 4,
	FEATURE_SHADER_TESSELLATION_AND_GEOMETRY_POINT_SIZE	= 1u << 5,
};
typedef deUint32 FeatureFlags;

enum SyncPrimitive
{
	SYNC_PRIMITIVE_FENCE,
	SYNC_PRIMITIVE_BINARY_SEMAPHORE,
	SYNC_PRIMITIVE_TIMELINE_SEMAPHORE,
	SYNC_PRIMITIVE_BARRIER,
	SYNC_PRIMITIVE_EVENT,
};

enum ResourceType
{
	RESOURCE_TYPE_BUFFER,
	RESOURCE_TYPE_IMAGE,
	RESOURCE_TYPE_INDIRECT_BUFFER_DRAW,
	RESOURCE_TYPE_INDIRECT_BUFFER_DRAW_INDEXED,
	RESOURCE_TYPE_INDIRECT_BUFFER_DISPATCH,
	RESOURCE_TYPE_INDEX_BUFFER,
};

struct ResourceDescription
{
	ResourceType					type;
	tcu::IVec4						size;			//!< unused components are 0, e.g. for buffers only x is meaningful
	vk::VkImageType					imageType;
	vk::VkFormat					imageFormat;
	vk::VkImageAspectFlags			imageAspect;
	vk::VkSampleCountFlagBits		imageSamples;
};

struct BufferResource
{
	vk::VkBuffer					handle;
	vk::VkDeviceSize				offset;
	vk::VkDeviceSize				size;
};

struct ImageResource
{
	vk::VkImage						handle;
	vk::VkExtent3D					extent;
	vk::VkImageType					imageType;
	vk::VkFormat					format;
	vk::VkImageSubresourceRange		subresourceRange;
	vk::VkImageSubresourceLayers	subresourceLayers;
};

typedef std::shared_ptr<SynchronizationWrapperBase> SynchronizationWrapperPtr;
SynchronizationWrapperPtr			getSynchronizationWrapper					(SynchronizationType				type,
																				 const vk::DeviceInterface&			vk,
																				 bool								usingTimelineSemaphores,
																				 deUint32							submitInfoCount = 1u);
void								submitCommandsAndWait						(SynchronizationWrapperPtr			synchronizationWrapper,
																				 const vk::DeviceInterface&			vk,
																				 const vk::VkDevice					device,
																				 const vk::VkQueue					queue,
																				 const vk::VkCommandBuffer			cmdBuffer);
vk::VkImageCreateInfo				makeImageCreateInfo							(const vk::VkImageType				imageType,
																				 const vk::VkExtent3D&				extent,
																				 const vk::VkFormat					format,
																				 const vk::VkImageUsageFlags		usage,
																				 const vk::VkSampleCountFlagBits	samples = vk::VK_SAMPLE_COUNT_1_BIT);
vk::Move<vk::VkCommandBuffer>		makeCommandBuffer							(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkCommandPool commandPool);
vk::Move<vk::VkPipeline>			makeComputePipeline							(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkPipelineLayout pipelineLayout, const vk::VkShaderModule shaderModule, const vk::VkSpecializationInfo* specInfo, PipelineCacheData& pipelineCacheData);
void								beginRenderPassWithRasterizationDisabled	(const vk::DeviceInterface& vk, const vk::VkCommandBuffer commandBuffer, const vk::VkRenderPass renderPass, const vk::VkFramebuffer framebuffer);
void								requireFeatures								(const vk::InstanceInterface& vki, const vk::VkPhysicalDevice physDevice, const FeatureFlags flags);
void								requireStorageImageSupport					(const vk::InstanceInterface& vki, const vk::VkPhysicalDevice physDevice, const vk::VkFormat fmt);
std::string							getResourceName								(const ResourceDescription& resource);
bool								isIndirectBuffer							(const ResourceType type);
vk::VkCommandBufferSubmitInfoKHR	makeCommonCommandBufferSubmitInfo			(const vk::VkCommandBuffer cmdBuf);
vk::VkSemaphoreSubmitInfoKHR		makeCommonSemaphoreSubmitInfo				(vk::VkSemaphore semaphore, deUint64 value, vk::VkPipelineStageFlags2KHR stageMask);
vk::VkDependencyInfoKHR				makeCommonDependencyInfo					(const vk::VkMemoryBarrier2KHR* pMemoryBarrier = DE_NULL, const vk::VkBufferMemoryBarrier2KHR* pBufferMemoryBarrier = DE_NULL, const vk::VkImageMemoryBarrier2KHR* pImageMemoryBarrier = DE_NULL, bool eventDependency = DE_FALSE);

} // synchronization
} // vkt

#endif // _VKTSYNCHRONIZATIONUTIL_HPP
