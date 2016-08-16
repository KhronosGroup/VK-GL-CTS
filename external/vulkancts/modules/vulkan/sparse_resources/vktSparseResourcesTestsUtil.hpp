#ifndef _VKTSPARSERESOURCESTESTSUTIL_HPP
#define _VKTSPARSERESOURCESTESTSUTIL_HPP
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
 * \file  vktSparseResourcesTestsUtil.hpp
 * \brief Sparse Resources Tests Utility Classes
 *//*--------------------------------------------------------------------*/

#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "deSharedPtr.hpp"

namespace vkt
{
namespace sparse
{

enum ImageType
{
	IMAGE_TYPE_1D = 0,
	IMAGE_TYPE_1D_ARRAY,
	IMAGE_TYPE_2D,
	IMAGE_TYPE_2D_ARRAY,
	IMAGE_TYPE_3D,
	IMAGE_TYPE_CUBE,
	IMAGE_TYPE_CUBE_ARRAY,
	IMAGE_TYPE_BUFFER,

	IMAGE_TYPE_LAST
};

enum MemoryAlignment
{
	MEM_ALIGN_BUFFERIMAGECOPY_OFFSET = 4u
};

class Buffer
{
public:
									Buffer			(const vk::DeviceInterface&		deviceInterface,
													 const vk::VkDevice				device,
													 vk::Allocator&					allocator,
													 const vk::VkBufferCreateInfo&	bufferCreateInfo,
													 const vk::MemoryRequirement	memoryRequirement);

	const vk::VkBuffer&				get				(void) const { return *m_buffer; }
	const vk::VkBuffer&				operator*		(void) const { return get(); }
	vk::Allocation&					getAllocation	(void) const { return *m_allocation; }

private:
	vk::Unique<vk::VkBuffer>		m_buffer;
	de::UniquePtr<vk::Allocation>	m_allocation;

									Buffer			(const Buffer&);
	Buffer&							operator=		(const Buffer&);
};

class Image
{
public:
									Image			(const vk::DeviceInterface&		deviceInterface,
													 const vk::VkDevice				device,
													 vk::Allocator&					allocator,
													 const vk::VkImageCreateInfo&	imageCreateInfo,
													 const vk::MemoryRequirement	memoryRequirement);

	const vk::VkImage&				get				(void) const { return *m_image; }
	const vk::VkImage&				operator*		(void) const { return get(); }
	vk::Allocation&					getAllocation	(void) const { return *m_allocation; }

private:
	vk::Unique<vk::VkImage>			m_image;
	de::UniquePtr<vk::Allocation>	m_allocation;

									Image			(const Image&);
	Image&							operator=		(const Image&);
};

class GraphicsPipelineBuilder
{
public:
								GraphicsPipelineBuilder	(void) : m_renderSize			(0, 0)
															   , m_shaderStageFlags		(0u)
															   , m_cullModeFlags		(vk::VK_CULL_MODE_NONE)
															   , m_frontFace			(vk::VK_FRONT_FACE_COUNTER_CLOCKWISE)
															   , m_patchControlPoints	(1u)
															   , m_attachmentsCount		(1u)
															   , m_blendEnable			(false)
															   , m_primitiveTopology	(vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) {}

	GraphicsPipelineBuilder&	setRenderSize					(const tcu::IVec2& size) { m_renderSize = size; return *this; }
	GraphicsPipelineBuilder&	setShader						(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkShaderStageFlagBits stage, const vk::ProgramBinary& binary, const vk::VkSpecializationInfo* specInfo);
	GraphicsPipelineBuilder&	setPatchControlPoints			(const deUint32 controlPoints) { m_patchControlPoints = controlPoints; return *this; }
	GraphicsPipelineBuilder&	setAttachmentsCount				(const deUint32 attachmentsCount) { m_attachmentsCount = attachmentsCount; return *this; }
	GraphicsPipelineBuilder&	setCullModeFlags				(const vk::VkCullModeFlags cullModeFlags) { m_cullModeFlags = cullModeFlags; return *this; }
	GraphicsPipelineBuilder&	setFrontFace					(const vk::VkFrontFace frontFace) { m_frontFace = frontFace; return *this; }
	GraphicsPipelineBuilder&	setBlend						(const bool enable) { m_blendEnable = enable; return *this; }

	//! Applies only to pipelines without tessellation shaders.
	GraphicsPipelineBuilder&	setPrimitiveTopology			(const vk::VkPrimitiveTopology topology) { m_primitiveTopology = topology; return *this; }

	GraphicsPipelineBuilder&	addVertexBinding				(const vk::VkVertexInputBindingDescription vertexBinding) { m_vertexInputBindings.push_back(vertexBinding); return *this; }
	GraphicsPipelineBuilder&	addVertexAttribute				(const vk::VkVertexInputAttributeDescription vertexAttribute) { m_vertexInputAttributes.push_back(vertexAttribute); return *this; }
	GraphicsPipelineBuilder&	addDynamicState					(const vk::VkDynamicState dynamicState) { m_dynamicStates.push_back(dynamicState); return *this; }

	vk::Move<vk::VkPipeline>	build							(const vk::DeviceInterface& vk, const vk::VkDevice device, const vk::VkPipelineLayout pipelineLayout, const vk::VkRenderPass renderPass);

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
	std::vector<vk::VkDynamicState>						m_dynamicStates;
	vk::VkShaderStageFlags								m_shaderStageFlags;
	vk::VkCullModeFlags									m_cullModeFlags;
	vk::VkFrontFace										m_frontFace;
	deUint32											m_patchControlPoints;
	deUint32											m_attachmentsCount;
	bool												m_blendEnable;
	vk::VkPrimitiveTopology								m_primitiveTopology;

	GraphicsPipelineBuilder (const GraphicsPipelineBuilder&);
	GraphicsPipelineBuilder& operator= (const GraphicsPipelineBuilder&);
};

enum FeatureFlagBits
{
	FEATURE_TESSELLATION_SHADER = 1u << 0,
	FEATURE_GEOMETRY_SHADER = 1u << 1,
	FEATURE_SHADER_FLOAT_64 = 1u << 2,
	FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS = 1u << 3,
	FEATURE_FRAGMENT_STORES_AND_ATOMICS = 1u << 4,
	FEATURE_SHADER_TESSELLATION_AND_GEOMETRY_POINT_SIZE = 1u << 5,
};
typedef deUint32 FeatureFlags;

// Image helper functions
vk::VkImageType		mapImageType					(const ImageType			imageType);
vk::VkImageViewType	mapImageViewType				(const ImageType			imageType);
std::string			getImageTypeName				(const ImageType			imageType);
std::string			getShaderImageType				(const tcu::TextureFormat&	format,
													 const ImageType			imageType);
std::string			getShaderImageDataType			(const tcu::TextureFormat&	format);
std::string			getShaderImageFormatQualifier	(const tcu::TextureFormat&	format);
std::string			getShaderImageCoordinates		(const ImageType			imageType,
													 const std::string&			x,
													 const std::string&			xy,
													 const std::string&			xyz);
//!< Size used for addresing image in a compute shader
tcu::UVec3			getShaderGridSize				(const ImageType			imageType,
													 const tcu::UVec3&			imageSize,
													 const deUint32				mipLevel = 0);
//!< Size of a single image layer
tcu::UVec3			getLayerSize					(const ImageType			imageType,
													 const tcu::UVec3&			imageSize);
//!< Number of array layers (for array and cube types)
deUint32			getNumLayers					(const ImageType			imageType,
													 const tcu::UVec3&			imageSize);
//!< Number of texels in an image
deUint32			getNumPixels					(const ImageType			imageType,
													 const tcu::UVec3&			imageSize);
//!< Coordinate dimension used for addressing (e.g. 3 (x,y,z) for 2d array)
deUint32			getDimensions					(const ImageType			imageType);
//!< Coordinate dimension used for addressing a single layer (e.g. 2 (x,y) for 2d array)
deUint32			getLayerDimensions				(const ImageType			imageType);
//!< Helper function for checking if requested image size does not exceed device limits
bool				isImageSizeSupported			(const vk::InstanceInterface&		instance,
													 const vk::VkPhysicalDevice			physicalDevice,
													 const ImageType					imageType,
													 const tcu::UVec3&					imageSize);

vk::VkExtent3D		mipLevelExtents					(const vk::VkExtent3D&				baseExtents,
													 const deUint32						mipLevel);

tcu::UVec3			mipLevelExtents					(const tcu::UVec3&					baseExtents,
													 const deUint32						mipLevel);

deUint32			getImageMaxMipLevels			(const vk::VkImageFormatProperties& imageFormatProperties,
													 const vk::VkExtent3D&				extent);

deUint32			getImageMipLevelSizeInBytes		(const vk::VkExtent3D&				baseExtents,
													 const deUint32						layersCount,
													 const tcu::TextureFormat&			format,
													 const deUint32						mipmapLevel,
													 const deUint32						mipmapMemoryAlignment	= 1u);

deUint32			getImageSizeInBytes				(const vk::VkExtent3D&				baseExtents,
													 const deUint32						layersCount,
													 const tcu::TextureFormat&			format,
													 const deUint32						mipmapLevelsCount		= 1u,
													 const deUint32						mipmapMemoryAlignment	= 1u);

vk::Move<vk::VkCommandPool>		makeCommandPool					(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const deUint32						queueFamilyIndex);

vk::Move<vk::VkCommandBuffer>	makeCommandBuffer				(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const vk::VkCommandPool			commandPool);

vk::Move<vk::VkPipelineLayout>	makePipelineLayout				(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const vk::VkDescriptorSetLayout	descriptorSetLayout);

vk::Move<vk::VkPipeline>		makeComputePipeline				(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const vk::VkPipelineLayout			pipelineLayout,
																 const vk::VkShaderModule			shaderModule,
																 const vk::VkSpecializationInfo*	specializationInfo = 0);

vk::Move<vk::VkBufferView>		makeBufferView					(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const vk::VkBuffer					buffer,
																 const vk::VkFormat					format,
																 const vk::VkDeviceSize				offset,
																 const vk::VkDeviceSize				size);

vk::Move<vk::VkImageView>		makeImageView					(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const vk::VkImage					image,
																 const vk::VkImageViewType			imageViewType,
																 const vk::VkFormat					format,
																 const vk::VkImageSubresourceRange	subresourceRange);

vk::Move<vk::VkDescriptorSet>	makeDescriptorSet				(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const vk::VkDescriptorPool			descriptorPool,
																 const vk::VkDescriptorSetLayout	setLayout);

vk::Move<vk::VkSemaphore>		makeSemaphore					(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device);

vk::VkBufferCreateInfo			makeBufferCreateInfo			(const vk::VkDeviceSize				bufferSize,
																 const vk::VkBufferUsageFlags		usage);

vk::VkBufferImageCopy			makeBufferImageCopy				(const vk::VkExtent3D				extent,
																 const deUint32						layersCount,
																 const deUint32						mipmapLevel = 0u,
																 const vk::VkDeviceSize				bufferOffset = 0ull);

vk::VkBufferMemoryBarrier		makeBufferMemoryBarrier			(const vk::VkAccessFlags			srcAccessMask,
																 const vk::VkAccessFlags			dstAccessMask,
																 const vk::VkBuffer					buffer,
																 const vk::VkDeviceSize				offset,
																 const vk::VkDeviceSize				bufferSizeBytes);

vk::VkImageMemoryBarrier		makeImageMemoryBarrier			(const vk::VkAccessFlags			srcAccessMask,
																 const vk::VkAccessFlags			dstAccessMask,
																 const vk::VkImageLayout			oldLayout,
																 const vk::VkImageLayout			newLayout,
																 const vk::VkImage					image,
																 const vk::VkImageSubresourceRange	subresourceRange);

vk::VkImageMemoryBarrier		makeImageMemoryBarrier			(const vk::VkAccessFlags			srcAccessMask,
																 const vk::VkAccessFlags			dstAccessMask,
																 const vk::VkImageLayout			oldLayout,
																 const vk::VkImageLayout			newLayout,
																 const deUint32						srcQueueFamilyIndex,
																 const deUint32						destQueueFamilyIndex,
																 const vk::VkImage					image,
																 const vk::VkImageSubresourceRange	subresourceRange);

vk::VkMemoryBarrier				makeMemoryBarrier				(const vk::VkAccessFlags			srcAccessMask,
																 const vk::VkAccessFlags			dstAccessMask);

void							beginCommandBuffer				(const vk::DeviceInterface&			vk,
																 const vk::VkCommandBuffer			cmdBuffer);

void							endCommandBuffer				(const vk::DeviceInterface&			vk,
																 const vk::VkCommandBuffer			cmdBuffer);

void							submitCommands					(const vk::DeviceInterface&			vk,
																 const vk::VkQueue					queue,
																 const vk::VkCommandBuffer			cmdBuffer,
																 const deUint32						waitSemaphoreCount		= 0,
																 const vk::VkSemaphore*				pWaitSemaphores			= DE_NULL,
																 const vk::VkPipelineStageFlags*	pWaitDstStageMask		= DE_NULL,
																 const deUint32						signalSemaphoreCount	= 0,
																 const vk::VkSemaphore*				pSignalSemaphores		= DE_NULL);

void							submitCommandsAndWait			(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const vk::VkQueue					queue,
																 const vk::VkCommandBuffer			cmdBuffer,
																 const deUint32						waitSemaphoreCount		= 0,
																 const vk::VkSemaphore*				pWaitSemaphores			= DE_NULL,
																 const vk::VkPipelineStageFlags*	pWaitDstStageMask		= DE_NULL,
																 const deUint32						signalSemaphoreCount	= 0,
																 const vk::VkSemaphore*				pSignalSemaphores		= DE_NULL);

vk::VkSparseImageMemoryBind		makeSparseImageMemoryBind		(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const vk::VkDeviceSize				allocationSize,
																 const deUint32						memoryType,
																 const vk::VkImageSubresource&		subresource,
																 const vk::VkOffset3D&				offset,
																 const vk::VkExtent3D&				extent);

vk::VkSparseMemoryBind			makeSparseMemoryBind			(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const vk::VkDeviceSize				allocationSize,
																 const deUint32						memoryType,
																 const vk::VkDeviceSize				resourceOffset);

vk::Move<vk::VkRenderPass>		makeRenderPass					(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const vk::VkFormat					colorFormat);

vk::Move<vk::VkRenderPass>		makeRenderPassWithoutAttachments(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device);

vk::Move<vk::VkFramebuffer>		makeFramebuffer					(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const vk::VkRenderPass				renderPass,
																 const vk::VkImageView				colorAttachment,
																 const deUint32						width,
																 const deUint32						height,
																 const deUint32						layers);

vk::Move<vk::VkFramebuffer>	makeFramebufferWithoutAttachments	(const vk::DeviceInterface&			vk,
																 const vk::VkDevice					device,
																 const vk::VkRenderPass				renderPass);

void							beginRenderPass					(const vk::DeviceInterface&				vk,
																 const vk::VkCommandBuffer				commandBuffer,
																 const vk::VkRenderPass					renderPass,
																 const vk::VkFramebuffer				framebuffer,
																 const vk::VkRect2D&					renderArea,
																 const std::vector<vk::VkClearValue>&	clearValues);

void					beginRenderPassWithRasterizationDisabled(const vk::DeviceInterface&			vk,
																 const vk::VkCommandBuffer			commandBuffer,
																 const vk::VkRenderPass				renderPass,
																 const vk::VkFramebuffer			framebuffer);

void							endRenderPass					(const vk::DeviceInterface&			vk,
																 const vk::VkCommandBuffer			commandBuffer);

void							requireFeatures					(const vk::InstanceInterface&		vki,
																 const vk::VkPhysicalDevice			physicalDevice,
																 const FeatureFlags					flags);

template<typename T>
inline de::SharedPtr<vk::Unique<T> > makeVkSharedPtr			(vk::Move<T> vkMove)
{
	return de::SharedPtr<vk::Unique<T> >(new vk::Unique<T>(vkMove));
}

template<typename T>
inline std::size_t sizeInBytes(const std::vector<T>& vec)
{
	return vec.size() * sizeof(vec[0]);
}

} // sparse
} // vkt

#endif // _VKTSPARSERESOURCESTESTSUTIL_HPP
