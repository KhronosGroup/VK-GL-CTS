/*------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2018 The Khronos Group Inc.
* Copyright (c) 2018 Intel Corporation
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
* \brief VK_EXT_external_memory_host extension tests.
*//*--------------------------------------------------------------------*/

#include "vktMemoryExternalMemoryHostTests.hpp"

#include "vktTestCaseUtil.hpp"

#include "deMath.h"

#include "vkQueryUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"


#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"

namespace vkt
{
namespace memory
{
namespace
{

using namespace vk;

inline deUint32 getBit (deUint32 src, int ndx)
{
	return (src >> ndx) & 1;
}

inline bool isBitSet (deUint32 src, int ndx)
{
	return getBit(src, ndx) != 0;
}

struct TestParams
{
	VkFormat		m_format;
	bool			m_useOffset;

	TestParams		(VkFormat f, bool offset = false) : m_format(f) , m_useOffset(offset) {}
};

void checkExternalMemoryProperties (const vk::VkExternalMemoryProperties& properties)
{
	// If obtaining the properties did not fail, the compatible handle types should indicate our handle type at least.
	if ((properties.compatibleHandleTypes & VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT) == 0)
		TCU_FAIL("compatibleHandleTypes does not include the host allocation bit");

	// If this is host memory, it cannot require dedicated allocation.
	if ((properties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) != 0)
		TCU_FAIL("externalMemoryFeatures for host allocated format includes dedicated allocation bit");

	// Memory should be importable to bind it to an image or buffer.
	if ((properties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT) == 0)
		TCU_FAIL("externalMemoryFeatures for host allocated format does not include the importable bit");
}

class ExternalMemoryHostBaseTestInstance : public TestInstance
{
public:
									ExternalMemoryHostBaseTestInstance			(Context& context, VkDeviceSize allocationSize);
									~ExternalMemoryHostBaseTestInstance			(void);
protected:
	virtual tcu::TestStatus			iterate										(void);
	VkDeviceSize					getMinImportedHostPointerAlignment			(void);
	deUint32						getHostPointerMemoryTypeBits				(void* hostPointer);
	Move<VkDeviceMemory>			allocateMemoryFromHostPointer				(deUint32 memoryTypeIndex);
	void							logMemoryTypeIndexPropertyFlags				(deUint32 index);
	bool							findCompatibleMemoryTypeIndexToTest			(deUint32 resourceMemoryTypeBits, deUint32 hostPointerMemoryTypeBits, deUint32* outMemoryTypeIndexToTest);
	bool							findMemoryTypeIndexToTest					(deUint32 hostPointerMemoryTypeBits, deUint32* outMemoryTypeIndexToTest);

	const InstanceInterface&						m_vki;
	const DeviceInterface&							m_vkd;
	tcu::TestLog&									m_log;
	const VkDevice									m_device;
	const VkPhysicalDevice							m_physicalDevice;
	const VkQueue									m_queue;
	const vk::VkPhysicalDeviceMemoryProperties		m_memoryProps;
	VkDeviceSize									m_minImportedHostPointerAlignment;
	VkDeviceSize									m_allocationSize;
	void*											m_hostMemoryAlloc;
	Allocator&										m_allocator;
	Move<VkDeviceMemory>							m_deviceMemoryAllocatedFromHostPointer;
};

class ExternalMemoryHostRenderImageTestInstance : public ExternalMemoryHostBaseTestInstance
{
public:
									ExternalMemoryHostRenderImageTestInstance	(Context& context, TestParams testParams);
protected:
	virtual tcu::TestStatus			iterate										(void);
	Move<VkImage>					createImage									(VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage);
	Move<VkImageView>				createImageView								(void);
	Move<VkBuffer>					createBindMemoryInitializeVertexBuffer		(void);
	Move<VkBuffer>					createBindMemoryResultBuffer				(void);
	Move<VkFramebuffer>				createFramebuffer							(void);
	Move<VkDescriptorSet>			createAndUpdateDescriptorSet				(void);
	Move<VkPipelineLayout>			createPipelineLayout						(void);
	Move<VkPipeline>				createPipeline								(void);
	Move<VkRenderPass>				createRenderPass							(void);
	void							clear										(VkClearColorValue color);
	void							draw										(void);
	void							copyResultImagetoBuffer						(void);
	void							prepareReferenceImage						(tcu::PixelBufferAccess& reference);
	void							verifyFormatProperties						(VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage);

	TestParams										m_testParams;
	Move<VkImage>									m_image;
	Move<VkImageView>								m_imageView;
	Move<VkRenderPass>								m_renderPass;
	Move<VkFramebuffer>								m_framebuffer;
	Move<VkBuffer>									m_vertexBuffer;
	Move<VkBuffer>									m_resultBuffer;
	de::MovePtr<Allocation>							m_vertexBufferAllocation;
	de::MovePtr<Allocation>							m_resultBufferAllocation;
	Move<VkDescriptorPool>							m_descriptorPool;
	Move<VkDescriptorSetLayout>						m_descriptorSetLayout;
	Move<VkDescriptorSet>							m_descriptorSet;
	Move<VkShaderModule>							m_vertexShaderModule;
	Move<VkShaderModule>							m_fragmentShaderModule;
	Move<VkPipelineLayout>							m_pipelineLayout;
	Move<VkPipeline>								m_pipeline;
	Move<VkCommandPool>								m_cmdPool;
	Move<VkCommandBuffer>							m_cmdBuffer;
};

class ExternalMemoryHostSynchronizationTestInstance : public ExternalMemoryHostRenderImageTestInstance
{
public:
								ExternalMemoryHostSynchronizationTestInstance	(Context& context, TestParams testParams);
protected:
	virtual tcu::TestStatus		iterate											(void);
	void						prepareBufferForHostAccess						(VkDeviceSize size);
	void						copyResultBuffertoBuffer						(VkDeviceSize size);
	void						submitCommands									(VkCommandBuffer commandBuffer, VkFence fence);
	Move<VkBuffer>				createDataBuffer								(VkDeviceSize size, VkBufferUsageFlags usage);
	void						fillBuffer										(VkDeviceSize size);
	void						verifyBufferProperties							(VkBufferUsageFlags usage);

	Move<VkBuffer>				m_dataBuffer;
	Move<VkCommandPool>			m_cmdPoolCopy;
	Move<VkCommandBuffer>		m_cmdBufferCopy;
	Move<VkFence>				m_fence_1;
	Move<VkFence>				m_fence_2;
	Move<VkEvent>				m_event;
};

ExternalMemoryHostBaseTestInstance::ExternalMemoryHostBaseTestInstance (Context& context, VkDeviceSize allocationSize)
	: TestInstance							(context)
	, m_vki									(m_context.getInstanceInterface())
	, m_vkd									(m_context.getDeviceInterface())
	, m_log									(m_context.getTestContext().getLog())
	, m_device								(m_context.getDevice())
	, m_physicalDevice						(m_context.getPhysicalDevice())
	, m_queue								(m_context.getUniversalQueue())
	, m_memoryProps							(getPhysicalDeviceMemoryProperties(m_vki, m_physicalDevice))
	, m_minImportedHostPointerAlignment		(getMinImportedHostPointerAlignment())
	, m_allocationSize						(m_minImportedHostPointerAlignment * allocationSize)
	, m_allocator							(m_context.getDefaultAllocator())
{
	m_hostMemoryAlloc	=	deAlignedMalloc((size_t)m_allocationSize, (size_t)m_minImportedHostPointerAlignment);

	if (!m_hostMemoryAlloc)
		TCU_FAIL("Failed to allocate memory block.");

	DE_ASSERT(deIsAlignedPtr(m_hostMemoryAlloc, (deUintptr)m_minImportedHostPointerAlignment));
}

ExternalMemoryHostBaseTestInstance::~ExternalMemoryHostBaseTestInstance (void)
{
	deAlignedFree(m_hostMemoryAlloc);
}

VkDeviceSize ExternalMemoryHostBaseTestInstance::getMinImportedHostPointerAlignment (void)
{
	VkPhysicalDeviceExternalMemoryHostPropertiesEXT externalMemoryHostProperties	=
	{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT,		//VkStructureType		sType
		DE_NULL,																	//void*					pNext
		0																			//VkDeviceSize			minImportedHostPointerAlignment
	};

	VkPhysicalDeviceProperties2						propertiesDeviceProperties2;
	propertiesDeviceProperties2.sType				= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	propertiesDeviceProperties2.pNext				= &externalMemoryHostProperties;

	m_vki.getPhysicalDeviceProperties2(m_physicalDevice, &propertiesDeviceProperties2);

	m_log	<< tcu::TestLog::Message << "VkPhysicalDeviceExternalMemoryHostPropertiesEXT::minImportedHostPointerAlignment is "
			<< externalMemoryHostProperties.minImportedHostPointerAlignment << tcu::TestLog::EndMessage;

	if (externalMemoryHostProperties.minImportedHostPointerAlignment > 65536)
		TCU_FAIL("minImportedHostPointerAlignment is exceeding the supported limit");

	return externalMemoryHostProperties.minImportedHostPointerAlignment;
}

deUint32 ExternalMemoryHostBaseTestInstance::getHostPointerMemoryTypeBits (void* hostPointer)
{
	VkExternalMemoryHandleTypeFlagBits			externalMemoryHandleTypeFlagBits = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;

	VkMemoryHostPointerPropertiesEXT			memoryHostPointerProperties;
	memoryHostPointerProperties.sType			= VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT;
	memoryHostPointerProperties.pNext			= DE_NULL;

	VK_CHECK(m_vkd.getMemoryHostPointerPropertiesEXT(m_device, externalMemoryHandleTypeFlagBits, hostPointer, &memoryHostPointerProperties));

	m_log << tcu::TestLog::Message << "memoryTypeBits value: " << memoryHostPointerProperties.memoryTypeBits << tcu::TestLog::EndMessage;

	return memoryHostPointerProperties.memoryTypeBits;
}

Move<VkDeviceMemory> ExternalMemoryHostBaseTestInstance::allocateMemoryFromHostPointer (deUint32 memoryTypeIndex)
{
	VkImportMemoryHostPointerInfoEXT							importMemoryHostPointerInfo;
	importMemoryHostPointerInfo.sType							= VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT;
	importMemoryHostPointerInfo.pNext							= DE_NULL;
	importMemoryHostPointerInfo.handleType						= VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
	importMemoryHostPointerInfo.pHostPointer					= m_hostMemoryAlloc;

	VkMemoryAllocateInfo										memoryAllocateInfo;
	memoryAllocateInfo.sType									= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.pNext									= &importMemoryHostPointerInfo;
	memoryAllocateInfo.allocationSize							= m_allocationSize;
	memoryAllocateInfo.memoryTypeIndex							= memoryTypeIndex;

	return allocateMemory(m_vkd, m_device, &memoryAllocateInfo, DE_NULL);
}

void ExternalMemoryHostBaseTestInstance::logMemoryTypeIndexPropertyFlags (deUint32 index)
{
	m_log << tcu::TestLog::Message << "Memory Type index " << index << " property flags:" << tcu::TestLog::EndMessage;
	m_log << tcu::TestLog::Message << getMemoryPropertyFlagsStr(m_memoryProps.memoryTypes[index].propertyFlags) << tcu::TestLog::EndMessage;
}

bool ExternalMemoryHostBaseTestInstance::findCompatibleMemoryTypeIndexToTest (deUint32 resourceMemoryTypeBits, deUint32 hostPointerMemoryTypeBits, deUint32* outMemoryTypeIndexToTest)
{
	for (deUint32 bitMaskPosition = 0; bitMaskPosition < VK_MAX_MEMORY_TYPES; bitMaskPosition++)
	{
		if (isBitSet(resourceMemoryTypeBits & hostPointerMemoryTypeBits, bitMaskPosition))
		{
			logMemoryTypeIndexPropertyFlags(bitMaskPosition);
			*outMemoryTypeIndexToTest = bitMaskPosition;
			return true;
		}
	}
	return false;
}

bool ExternalMemoryHostBaseTestInstance::findMemoryTypeIndexToTest (deUint32 hostPointerMemoryTypeBits, deUint32* outMemoryTypeIndexToTest)
{
	return findCompatibleMemoryTypeIndexToTest(~0u, hostPointerMemoryTypeBits, outMemoryTypeIndexToTest);
}

tcu::TestStatus ExternalMemoryHostBaseTestInstance::iterate (void)
{
	deUint32			hostPointerMemoryTypeBits;
	deUint32			memoryTypeIndexToTest;

	//realocate to meet requirements for host memory alignment
	m_hostMemoryAlloc			= deAlignedRealloc(m_hostMemoryAlloc, (size_t)m_minImportedHostPointerAlignment, (size_t)m_minImportedHostPointerAlignment);
	m_allocationSize			= m_minImportedHostPointerAlignment;

	//check if reallocation is successfull
	if (!m_hostMemoryAlloc)
		TCU_FAIL("Failed to reallocate memory block.");

	DE_ASSERT(deIsAlignedPtr(m_hostMemoryAlloc, (deUintptr)m_minImportedHostPointerAlignment));

	//find the usable memory type index
	hostPointerMemoryTypeBits	= getHostPointerMemoryTypeBits(m_hostMemoryAlloc);
	if (findMemoryTypeIndexToTest(hostPointerMemoryTypeBits, &memoryTypeIndexToTest))
		m_deviceMemoryAllocatedFromHostPointer = allocateMemoryFromHostPointer(memoryTypeIndexToTest);
	else
		return tcu::TestStatus::fail("Fail");

	return tcu::TestStatus::pass("Pass");
}

ExternalMemoryHostRenderImageTestInstance::ExternalMemoryHostRenderImageTestInstance (Context& context, TestParams testParams)
		: ExternalMemoryHostBaseTestInstance	(context, 1)
		, m_testParams							(testParams)
{
}

void* alignedRealloc (void* ptr, VkDeviceSize size, VkDeviceSize alignment)
{
	void* newPtr = deAlignedRealloc(ptr, static_cast<size_t>(size), static_cast<size_t>(alignment));
	if (!newPtr)
		TCU_FAIL("Failed to reallocate memory block.");
	DE_ASSERT(deIsAlignedPtr(newPtr, static_cast<deUintptr>(alignment)));
	return newPtr;
}

tcu::TestStatus ExternalMemoryHostRenderImageTestInstance::iterate ()
{
	VkClearColorValue					clearColorBlue					= { { 0.0f, 0.0f, 1.0f, 1.0f } };
	const deUint32						queueFamilyIndex				= m_context.getUniversalQueueFamilyIndex();
	deUint32							memoryTypeIndexToTest;
	VkMemoryRequirements				imageMemoryRequirements;
	const VkImageTiling					tiling							= VK_IMAGE_TILING_LINEAR;
	const VkImageUsageFlags				usageFlags						= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |	VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	// Verify image format properties before proceeding.
	verifyFormatProperties(m_testParams.m_format, tiling, usageFlags);

	// Create image with external host memory.
	m_image = createImage(m_testParams.m_format, tiling, usageFlags);

	// Check memory requirements and reallocate memory if needed.
	imageMemoryRequirements = getImageMemoryRequirements(m_vkd, m_device, *m_image);

	const VkDeviceSize requiredSize = imageMemoryRequirements.size + (m_testParams.m_useOffset ? imageMemoryRequirements.alignment : 0ull);
	if (requiredSize > m_allocationSize)
	{
		// Reallocate block with a size that is a multiple of minImportedHostPointerAlignment.
		const auto newHostAllocationSize	= de::roundUp(requiredSize, m_minImportedHostPointerAlignment);
		m_hostMemoryAlloc					= alignedRealloc(m_hostMemoryAlloc, newHostAllocationSize, m_minImportedHostPointerAlignment);
		m_allocationSize					= newHostAllocationSize;

		m_log	<< tcu::TestLog::Message << "Realloc needed (required size: "  << requiredSize <<  "). " << "New host allocation size: " << newHostAllocationSize << ")."
				<< tcu::TestLog::EndMessage;
	}

	// Find the usable memory type index.
	const auto hostPointerMemoryTypeBits = getHostPointerMemoryTypeBits(m_hostMemoryAlloc);

	if (findCompatibleMemoryTypeIndexToTest(imageMemoryRequirements.memoryTypeBits, hostPointerMemoryTypeBits, &memoryTypeIndexToTest))
		m_deviceMemoryAllocatedFromHostPointer = allocateMemoryFromHostPointer(memoryTypeIndexToTest);
	else
		TCU_THROW(NotSupportedError, "Compatible memory type not found");

	VK_CHECK(m_vkd.bindImageMemory(m_device, *m_image, *m_deviceMemoryAllocatedFromHostPointer, (m_testParams.m_useOffset ? imageMemoryRequirements.alignment : 0)));

	m_imageView								= createImageView();
	m_renderPass							= createRenderPass();
	m_framebuffer							= createFramebuffer();
	m_vertexBuffer							= createBindMemoryInitializeVertexBuffer();
	m_resultBuffer							= createBindMemoryResultBuffer();

	vk::DescriptorSetLayoutBuilder			builder;

	builder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);

	m_descriptorSetLayout					= builder.build(m_vkd, m_device, (vk::VkDescriptorSetLayoutCreateFlags)0);

	m_descriptorPool						= DescriptorPoolBuilder().addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
																	 .build(m_vkd, m_device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	m_pipelineLayout						= createPipelineLayout();
	m_descriptorSet							= createAndUpdateDescriptorSet();

	m_vertexShaderModule					= createShaderModule(m_vkd, m_device, m_context.getBinaryCollection().get("position_only.vert"), 0);
	m_fragmentShaderModule					= createShaderModule(m_vkd, m_device, m_context.getBinaryCollection().get("only_color_out.frag"), 0);


	m_pipeline								= createPipeline();

	m_cmdPool								= createCommandPool(m_vkd, m_device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	m_cmdBuffer								= allocateCommandBuffer(m_vkd, m_device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);


	beginCommandBuffer(m_vkd, *m_cmdBuffer);

	clear(clearColorBlue);
	draw();
	copyResultImagetoBuffer();

	endCommandBuffer(m_vkd, *m_cmdBuffer);

	submitCommandsAndWait(m_vkd, m_device, m_queue, *m_cmdBuffer);

	tcu::ConstPixelBufferAccess				result(mapVkFormat(m_testParams.m_format), tcu::IVec3(100,100,1), m_resultBufferAllocation->getHostPtr());

	std::vector<float>						referenceData(40000, 0);
	tcu::PixelBufferAccess					reference(mapVkFormat(m_testParams.m_format), tcu::IVec3(100, 100, 1), referenceData.data());

	prepareReferenceImage(reference);

	if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Comparison", "Comparison", reference, result, tcu::Vec4(0.01f), tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Fail");

	return tcu::TestStatus::pass("Pass");
}

Move<VkImage>  ExternalMemoryHostRenderImageTestInstance::createImage (VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage)
{
	const vk::VkExternalMemoryImageCreateInfo	externalInfo =
	{
		vk::VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
		DE_NULL,
		(vk::VkExternalMemoryHandleTypeFlags)VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT
	};

	const VkImageCreateInfo						imageCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType
		&externalInfo,							// const void*				pNext
		0u,										// VkImageCreateFlags		flags
		VK_IMAGE_TYPE_2D,						// VkImageType				imageType
		format,									// VkFormat					format
		{ 100, 100, 1 },						// VkExtent3D				extent
		1,										// deUint32					mipLevels
		1,										// deUint32					arrayLayers
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples
		tiling,									// VkImageTiling			tiling
		usage,									// VkImageUsageFlags		usage
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode
		0,										// deUint32					queueFamilyIndexCount
		DE_NULL,								// const deUint32*			pQueueFamilyIndices
		VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			initialLayout
	};

	return vk::createImage(m_vkd, m_device, &imageCreateInfo, DE_NULL);
}

Move<VkFramebuffer>  ExternalMemoryHostRenderImageTestInstance::createFramebuffer ()
{
	const VkFramebufferCreateInfo framebufferCreateInfo =
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType				sType
		DE_NULL,											// const void*					pNext
		(VkFramebufferCreateFlags)0,
		*m_renderPass,										// VkRenderPass					renderPass
		1,													// deUint32						attachmentCount
		&m_imageView.get(),									// const VkImageView*			pAttachments
		100,												// deUint32						width
		100,												// deUint32						height
		1													// deUint32						layers
	};
	return vk::createFramebuffer(m_vkd, m_device, &framebufferCreateInfo);
}

Move<VkImageView>  ExternalMemoryHostRenderImageTestInstance::createImageView ()
{
	const VkImageViewCreateInfo		imageViewCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,																// VkStructureType			sType
		DE_NULL,																								// const void*				pNext
		0,																										// VkImageViewCreateFlags	flags
		*m_image,																								// VkImage					image
		VK_IMAGE_VIEW_TYPE_2D,																					// VkImageViewType			viewType
		m_testParams.m_format,																					// VkFormat					format
		{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},		// VkComponentMapping		components
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }																// VkImageSubresourceRange	subresourceRange
	};
	return vk::createImageView(m_vkd, m_device, &imageViewCreateInfo);
}

Move<VkBuffer>  ExternalMemoryHostRenderImageTestInstance::createBindMemoryInitializeVertexBuffer ()
{
	Move<VkBuffer>						buffer;
	float								triangleData[]					= { -1.0f,  -1.0f, 0.0f, 1.0f,
																		    -1.0f,   1.0f, 0.0f, 1.0f,
																			 0.0f,   1.0f, 0.0f, 1.0f,
																			 0.0f,  -1.0f, 0.0f, 1.0f };
	const VkBufferCreateInfo			vertexBufferCreateInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType
		DE_NULL,								// const void*			pNext
		0,										// VkBufferCreateFlags	flag
		sizeof(triangleData),					// VkDeviceSize			size
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,		// VkBufferUsageFlags	usage
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode
		0,										// deUint32				queueFamilyCount
		DE_NULL									// const deUint32*		pQueueFamilyIndices
	};
	buffer																= vk::createBuffer(m_vkd, m_device, &vertexBufferCreateInfo, DE_NULL);
	const VkMemoryRequirements			bufferMemoryRequirements		= getBufferMemoryRequirements(m_vkd, m_device, *buffer);
										m_vertexBufferAllocation		= m_allocator.allocate(bufferMemoryRequirements, MemoryRequirement::HostVisible);

	VK_CHECK(m_vkd.bindBufferMemory(m_device, *buffer, m_vertexBufferAllocation->getMemory(), m_vertexBufferAllocation->getOffset()));

	void* const							mapPtr							= m_vertexBufferAllocation->getHostPtr();

	deMemcpy(mapPtr, triangleData, sizeof(triangleData));
	flushAlloc(m_vkd, m_device, *m_vertexBufferAllocation);

	return buffer;
}

Move<VkBuffer>  ExternalMemoryHostRenderImageTestInstance::createBindMemoryResultBuffer ()
{
	Move<VkBuffer>						buffer;
	VkDeviceSize						size						= 10000 * vk::mapVkFormat(m_testParams.m_format).getPixelSize();

	const VkBufferCreateInfo			resultBufferCreateInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,						// VkStructureType		sType
		DE_NULL,													// const void*			pNext
		0,															// VkBufferCreateFlags	flags
		size,														// VkDeviceSize			size
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,							// VkBufferUsageFlags	usage
		VK_SHARING_MODE_EXCLUSIVE,									// VkSharingMode		sharingMode
		0,															// deUint32				queueFamilyCount
		DE_NULL														// const deUint32*		pQueueFamilyIndices
	};
	buffer															= vk::createBuffer(m_vkd, m_device, &resultBufferCreateInfo, DE_NULL);

	const VkMemoryRequirements			bufferMemoryRequirements	= getBufferMemoryRequirements(m_vkd, m_device, *buffer);
	m_resultBufferAllocation									    = m_allocator.allocate(bufferMemoryRequirements, MemoryRequirement::HostVisible);

	VK_CHECK(m_vkd.bindBufferMemory(m_device, *buffer, m_resultBufferAllocation->getMemory(), m_resultBufferAllocation->getOffset()));

	return buffer;
}

Move<VkDescriptorSet>  ExternalMemoryHostRenderImageTestInstance::createAndUpdateDescriptorSet ()
{
	Move<VkDescriptorSet>				descriptorSet;
	VkDescriptorBufferInfo				descriptorInfo;

	const VkDescriptorSetAllocateInfo	allocInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,	// VkStructureType                             sType
		DE_NULL,										// const void*                                 pNext
		*m_descriptorPool,								// VkDescriptorPool                            descriptorPool
		1u,												// deUint32                                    setLayoutCount
		&(m_descriptorSetLayout.get())					// const VkDescriptorSetLayout*                pSetLayouts
	};

	descriptorSet						= allocateDescriptorSet(m_vkd, m_device, &allocInfo);
	descriptorInfo						= makeDescriptorBufferInfo(*m_vertexBuffer, (VkDeviceSize)0u, sizeof(float) * 16);

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
		.update(m_vkd, m_device);

	return descriptorSet;
}

Move<VkPipelineLayout> ExternalMemoryHostRenderImageTestInstance::createPipelineLayout ()
{
	const VkPipelineLayoutCreateInfo	pipelineLayoutParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// VkStructureType				sType
		DE_NULL,										// const void*					pNext
		(VkPipelineLayoutCreateFlags)0,					// VkPipelineLayoutCreateFlags	flags
		1u,												// deUint32						descriptorSetCount
		&(m_descriptorSetLayout.get()),					// const VkDescriptorSetLayout*	pSetLayouts
		0u,												// deUint32						pushConstantRangeCount
		DE_NULL											// const VkPushConstantRange*	pPushConstantRanges
	};

	return vk::createPipelineLayout(m_vkd, m_device, &pipelineLayoutParams);
}

Move<VkPipeline> ExternalMemoryHostRenderImageTestInstance::createPipeline ()
{
	Move<VkPipeline>								pipeline;
	const std::vector<VkViewport>					viewports(1, makeViewport(tcu::UVec2(100,100)));
	const std::vector<VkRect2D>						scissors(1, makeRect2D(tcu::UVec2(100, 100)));
	const VkPrimitiveTopology						topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
	const VkPipelineVertexInputStateCreateInfo		vertexInputStateParams =
	{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType								sType
		DE_NULL,													// const void*									pNext
		0u,															// vkPipelineVertexInputStateCreateFlags		flags
		0u,															// deUint32										bindingCount
		DE_NULL,													// const VkVertexInputBindingDescription*		pVertexBindingDescriptions
		0u,															// deUint32										attributeCount
		DE_NULL,													// const VkVertexInputAttributeDescription*		pVertexAttributeDescriptions
	};

	return	    makeGraphicsPipeline(	m_vkd,						// const DeviceInterface&                       vk
										m_device,					// const VkDevice                               device
										*m_pipelineLayout,			// const VkPipelineLayout                       pipelineLayout
										*m_vertexShaderModule,		// const VkShaderModule                         vertexShaderModule
										DE_NULL,					// const VkShaderModule                         tessellationControlShaderModule
										DE_NULL,					// const VkShaderModule                         tessellationEvalShaderModule
										DE_NULL,					// const VkShaderModule                         geometryShaderModule
										*m_fragmentShaderModule,	// const VkShaderModule                         fragmentShaderModule
										*m_renderPass,				// const VkRenderPass                           renderPass
										viewports,					// const std::vector<VkViewport>&               viewports
										scissors,					// const std::vector<VkRect2D>&                 scissors
										topology,					// const VkPrimitiveTopology                    topology
										0u,							// const deUint32                               subpass
										0u,							// const deUint32                               patchControlPoints
										&vertexInputStateParams);	// const VkPipelineVertexInputStateCreateInfo*  vertexInputStateCreateInfo
}

void ExternalMemoryHostRenderImageTestInstance::clear (VkClearColorValue color)
{
	const struct VkImageSubresourceRange	subRangeColor	=
	{
		VK_IMAGE_ASPECT_COLOR_BIT,  // VkImageAspectFlags  aspectMask
		0u,                         // deUint32            baseMipLevel
		1u,                         // deUint32            mipLevels
		0u,                         // deUint32            baseArrayLayer
		1u,                         // deUint32            arraySize
	};
	const VkImageMemoryBarrier				imageBarrier	=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType
		DE_NULL,										// const void*				pNext
		0u,												// VkAccessFlags			srcAccessMask
		VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags			dstAccessMask
		VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			oldLayout
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout			newLayout
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex
		*m_image,										// VkImage					image
		subRangeColor									// VkImageSubresourceRange	subresourceRange
	};

	m_vkd.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, DE_FALSE, 0u, DE_NULL, 0u, DE_NULL, 1u, &imageBarrier);
	m_vkd.cmdClearColorImage(*m_cmdBuffer, *m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &subRangeColor);
}

void ExternalMemoryHostRenderImageTestInstance::draw ()
{
	const struct VkImageSubresourceRange	subRangeColor =
	{
		VK_IMAGE_ASPECT_COLOR_BIT,  // VkImageAspectFlags  aspectMask
		0u,                         // deUint32            baseMipLevel
		1u,                         // deUint32            mipLevels
		0u,                         // deUint32            baseArrayLayer
		1u,                         // deUint32            arraySize
	};
	const VkImageMemoryBarrier				imageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,			// VkStructureType			sType
		DE_NULL,										// const void*				pNext
		VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags			srcAccessMask
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,			// VkAccessFlags			dstAccessMask
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout			oldLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// VkImageLayout			newLayout
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,						// deUint32					dstQueueFamilyIndex
		*m_image,										// VkImage					image
		subRangeColor									// VkImageSubresourceRange	subresourceRange
	};
	m_vkd.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, DE_FALSE, 0u, DE_NULL, 0u, DE_NULL, 1u, &imageBarrier);

	beginRenderPass(m_vkd, *m_cmdBuffer, *m_renderPass, *m_framebuffer, makeRect2D(0, 0, 75, 100), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
	m_vkd.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
	m_vkd.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0, 1, &(*m_descriptorSet), 0, DE_NULL);
	m_vkd.cmdDraw(*m_cmdBuffer, 4, 1, 0, 0);
	endRenderPass(m_vkd, *m_cmdBuffer);
}

void ExternalMemoryHostRenderImageTestInstance::copyResultImagetoBuffer ()
{
	copyImageToBuffer(m_vkd, *m_cmdBuffer, *m_image, *m_resultBuffer, tcu::IVec2(100, 100));
}

void ExternalMemoryHostRenderImageTestInstance::prepareReferenceImage (tcu::PixelBufferAccess& reference)
{
	for (int w=0; w < 100; w++)
		for (int h = 0; h < 100; h++)
		{
			if (w < 50)					reference.setPixel(tcu::Vec4(0, 1, 0, 1), w, h);
			if ((w >= 50) && (w < 75))	reference.setPixel(tcu::Vec4(1, 0, 0, 1), w, h);
			if (w >=75)					reference.setPixel(tcu::Vec4(0, 0, 1, 1), w, h);
		}
}

Move<VkRenderPass> ExternalMemoryHostRenderImageTestInstance::createRenderPass ()
{
	const VkAttachmentDescription			colorAttachmentDescription =
	{
		(VkAttachmentDescriptionFlags)0,			// VkAttachmentDescriptionFlags    flags
		m_testParams.m_format,						// VkFormat                        format
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits           samples
		VK_ATTACHMENT_LOAD_OP_CLEAR,				// VkAttachmentLoadOp              loadOp
		VK_ATTACHMENT_STORE_OP_STORE,				// VkAttachmentStoreOp             storeOp
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,			// VkAttachmentLoadOp              stencilLoadOp
		VK_ATTACHMENT_STORE_OP_DONT_CARE,			// VkAttachmentStoreOp             stencilStoreOp
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	// VkImageLayout                   initialLayout
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout                   finalLayout
	};

	std::vector<VkAttachmentDescription>	attachmentDescriptions;
	attachmentDescriptions.push_back(colorAttachmentDescription);

	const VkAttachmentReference				colorAttachmentRef =
	{
		0u,											// deUint32         attachment
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// VkImageLayout    layout
	};

	const VkSubpassDescription				subpassDescription =
	{
		(VkSubpassDescriptionFlags)0,				// VkSubpassDescriptionFlags       flags
		VK_PIPELINE_BIND_POINT_GRAPHICS,			// VkPipelineBindPoint             pipelineBindPoint
		0u,											// deUint32                        inputAttachmentCount
		DE_NULL,									// const VkAttachmentReference*    pInputAttachments
		1u,											// deUint32                        colorAttachmentCount
		&colorAttachmentRef,						// const VkAttachmentReference*    pColorAttachments
		DE_NULL,									// const VkAttachmentReference*    pResolveAttachments
		DE_NULL,									// const VkAttachmentReference*    pDepthStencilAttachment
		0u,											// deUint32                        preserveAttachmentCount
		DE_NULL										// const deUint32*                 pPreserveAttachments
	};

	const VkRenderPassCreateInfo			renderPassInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType                   sType
		DE_NULL,									// const void*                       pNext
		(VkRenderPassCreateFlags)0,					// VkRenderPassCreateFlags           flags
		(deUint32)attachmentDescriptions.size(),	// deUint32                          attachmentCount
		&attachmentDescriptions[0],					// const VkAttachmentDescription*    pAttachments
		1u,											// deUint32                          subpassCount
		&subpassDescription,						// const VkSubpassDescription*       pSubpasses
		0u,											// deUint32                          dependencyCount
		DE_NULL										// const VkSubpassDependency*        pDependencies
	};

	return vk::createRenderPass(m_vkd, m_device, &renderPassInfo);
}

void ExternalMemoryHostRenderImageTestInstance::verifyFormatProperties (VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage)
{
	const VkPhysicalDeviceExternalImageFormatInfo externalInfo = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
		DE_NULL,
		VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT
	};

	const VkPhysicalDeviceImageFormatInfo2 formatInfo = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,	// VkStructureType       sType;
		&externalInfo,											// const void*           pNext;
		format,													// VkFormat              format;
		VK_IMAGE_TYPE_2D,										// VkImageType           type;
		tiling,													// VkImageTiling         tiling;
		usage,													// VkImageUsageFlags     usage;
		0u														// VkImageCreateFlags    flags;
	};

	vk::VkExternalImageFormatProperties externalProperties = {
		VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
		DE_NULL,
		vk::VkExternalMemoryProperties()
	};

	vk::VkImageFormatProperties2 formatProperties = {
		VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
		&externalProperties,
		vk::VkImageFormatProperties()
	};

	const auto result = m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties2(m_context.getPhysicalDevice(), &formatInfo, &formatProperties);
	if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
		TCU_THROW(NotSupportedError, "Image format not supported for external host memory");

	VK_CHECK(result);
	checkExternalMemoryProperties(externalProperties.externalMemoryProperties);
}

ExternalMemoryHostSynchronizationTestInstance::ExternalMemoryHostSynchronizationTestInstance (Context& context, TestParams testParams)
	: ExternalMemoryHostRenderImageTestInstance (context, testParams)
{
}

tcu::TestStatus ExternalMemoryHostSynchronizationTestInstance::iterate ()
{
	DE_ASSERT(m_testParams.m_format == VK_FORMAT_R8G8B8A8_UNORM);

	const deUint32							queueFamilyIndex							= m_context.getUniversalQueueFamilyIndex();
	const VkDeviceSize						dataBufferSize								= 10000 * vk::mapVkFormat(m_testParams.m_format).getPixelSize();
	const VkBufferUsageFlags				usageFlags									= (VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	void*									pointerReturnedByMapMemory;
	deUint32								hostPointerMemoryTypeBits;
	deUint32								memoryTypeIndexToTest;
	VkMemoryRequirements					bufferMemoryRequirements;

	m_dataBuffer							= createDataBuffer(dataBufferSize, usageFlags);

	//check memory requirements
	bufferMemoryRequirements				= getBufferMemoryRequirements(m_vkd, m_device, *m_dataBuffer);
	VkDeviceSize requiredSize				= bufferMemoryRequirements.size;
	//reallocate memory if needed
	if (requiredSize > m_allocationSize)
	{
		VkDeviceSize newHostAllocationSize	= VkDeviceSize(deCeilFloatToInt32((float(requiredSize) / float(m_minImportedHostPointerAlignment))) * m_minImportedHostPointerAlignment);

		m_log << tcu::TestLog::Message << "Realloc needed (required size: " << requiredSize << "). "
			<< "New host allocation size: " << newHostAllocationSize << ")." << tcu::TestLog::EndMessage;

		m_hostMemoryAlloc					= deAlignedRealloc(m_hostMemoryAlloc, (size_t)newHostAllocationSize, (size_t)m_minImportedHostPointerAlignment);
		m_allocationSize					= newHostAllocationSize;
	}

	//check if reallocation is successfull
	if (!m_hostMemoryAlloc)
		TCU_FAIL("Failed to reallocate memory block.");

	DE_ASSERT(deIsAlignedPtr(m_hostMemoryAlloc, (deUintptr)m_minImportedHostPointerAlignment));

	//find the usable memory type index
	hostPointerMemoryTypeBits				= getHostPointerMemoryTypeBits(m_hostMemoryAlloc);
	if (findCompatibleMemoryTypeIndexToTest(bufferMemoryRequirements.memoryTypeBits, hostPointerMemoryTypeBits, &memoryTypeIndexToTest))
		m_deviceMemoryAllocatedFromHostPointer = allocateMemoryFromHostPointer(memoryTypeIndexToTest);
	else
		TCU_THROW(NotSupportedError, "Compatible memory type not found");

	// Verify buffer properties with external host memory.
	verifyBufferProperties(usageFlags);

	VK_CHECK(m_vkd.bindBufferMemory(m_device, *m_dataBuffer, *m_deviceMemoryAllocatedFromHostPointer, 0));

	m_resultBuffer							= createBindMemoryResultBuffer();
	m_cmdPool								= createCommandPool(m_vkd, m_device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);
	m_cmdBuffer								= allocateCommandBuffer(m_vkd, m_device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	m_cmdBufferCopy							= allocateCommandBuffer(m_vkd, m_device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	m_event									= createEvent(m_vkd, m_device);
	m_fence_1								= createFence(m_vkd, m_device);
	m_fence_2								= createFence(m_vkd, m_device);

	//record first command buffer
	beginCommandBuffer(m_vkd, *m_cmdBuffer);
	fillBuffer(dataBufferSize);
	prepareBufferForHostAccess(dataBufferSize);
	endCommandBuffer(m_vkd, *m_cmdBuffer);

	//record second command buffer
	beginCommandBuffer(m_vkd, *m_cmdBufferCopy);
	copyResultBuffertoBuffer(dataBufferSize);
	endCommandBuffer(m_vkd, *m_cmdBufferCopy);

	submitCommands(*m_cmdBuffer, *m_fence_1);
	submitCommands(*m_cmdBufferCopy, *m_fence_2);

	//wait for fence_1 and modify image on host
	VK_CHECK(m_vkd.waitForFences(m_device, 1u, &m_fence_1.get(), DE_TRUE, ~0ull));
	pointerReturnedByMapMemory				= mapMemory(m_vkd, m_device, *m_deviceMemoryAllocatedFromHostPointer, 0, dataBufferSize, 0);
	invalidateMappedMemoryRange(m_vkd, m_device, *m_deviceMemoryAllocatedFromHostPointer, 0, VK_WHOLE_SIZE);
	tcu::PixelBufferAccess bufferSurface(mapVkFormat(m_testParams.m_format), 100, 100, 1, (100 * vk::mapVkFormat(m_testParams.m_format).getPixelSize()), 0, m_hostMemoryAlloc);
	prepareReferenceImage(bufferSurface);
	flushMappedMemoryRange(m_vkd, m_device, *m_deviceMemoryAllocatedFromHostPointer, 0, VK_WHOLE_SIZE);
	//compare memory pointed by both pointers
	if (deMemCmp(m_hostMemoryAlloc, pointerReturnedByMapMemory, (size_t)dataBufferSize) != 0)
		TCU_FAIL("Failed memcmp check.");
	m_vkd.unmapMemory(m_device, *m_deviceMemoryAllocatedFromHostPointer);
	VK_CHECK(m_vkd.setEvent(m_device, *m_event));

	//wait for fence_2 before checking result
	VK_CHECK(m_vkd.waitForFences(m_device, 1u, &m_fence_2.get(), DE_TRUE, ~0ull));

	void * bufferDataPointer				= static_cast<char*>(m_resultBufferAllocation->getHostPtr()) + m_resultBufferAllocation->getOffset();
	tcu::ConstPixelBufferAccess				result(mapVkFormat(m_testParams.m_format), tcu::IVec3(100, 100, 1), bufferDataPointer);

	std::vector<float>						referenceData((unsigned int)dataBufferSize, 0);
	tcu::PixelBufferAccess					reference(mapVkFormat(m_testParams.m_format), tcu::IVec3(100, 100, 1), referenceData.data());

	prepareReferenceImage(reference);

	if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Comparison", "Comparison", reference, result, tcu::Vec4(0.01f), tcu::COMPARE_LOG_ON_ERROR))
		return tcu::TestStatus::fail("Fail");

	return tcu::TestStatus::pass("Pass");
}

void ExternalMemoryHostSynchronizationTestInstance::prepareBufferForHostAccess (VkDeviceSize size)
{
	const VkBufferMemoryBarrier		bufferBarrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
		VK_ACCESS_HOST_WRITE_BIT,					// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
		*m_dataBuffer,								// VkBuffer			buffer;
		0u,											// VkDeviceSize		offset;
		size										// VkDeviceSize		size;
	};

	m_vkd.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, DE_FALSE, 0u, DE_NULL, 1u, &bufferBarrier, 0u, DE_NULL);
}

void ExternalMemoryHostSynchronizationTestInstance::copyResultBuffertoBuffer (VkDeviceSize size)
{
	const VkBufferMemoryBarrier		bufferBarrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		VK_ACCESS_HOST_WRITE_BIT,					// VkAccessFlags	srcAccessMask;
		VK_ACCESS_TRANSFER_READ_BIT,				// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
		*m_dataBuffer,								// VkBuffer			buffer;
		0u,											// VkDeviceSize		offset;
		size										// VkDeviceSize		size;
	};

	const VkBufferCopy				region_all =
	{
		0,		//VkDeviceSize srcOffset;
		0,		//VkDeviceSize dstOffset;
		size	//VkDeviceSize size;
	};

	m_vkd.cmdWaitEvents(*m_cmdBufferCopy, 1, &m_event.get(), VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, DE_NULL, 1, &bufferBarrier, 0, DE_NULL);
	m_vkd.cmdCopyBuffer(*m_cmdBufferCopy, *m_dataBuffer, *m_resultBuffer, 1, &region_all);
}

void ExternalMemoryHostSynchronizationTestInstance::submitCommands (VkCommandBuffer commandBuffer, VkFence fence)
{
	const VkSubmitInfo		submitInfo =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,						// VkStructureType				sType
		DE_NULL,											// const void*					pNext
		0u,													// deUint32						waitSemaphoreCount
		DE_NULL,											// const VkSemaphore*			pWaitSemaphores
		(const VkPipelineStageFlags*)DE_NULL,				// const VkPipelineStageFlags*	pWaitDstStageMask
		1u,													// deUint32						commandBufferCount
		&commandBuffer,										// const VkCommandBuffer*		pCommandBuffers
		0u,													// deUint32						signalSemaphoreCount
		DE_NULL,											// const VkSemaphore*			pSignalSemaphores
	};

	VK_CHECK(m_vkd.queueSubmit(m_queue, 1u, &submitInfo, fence));
}

Move<VkBuffer> ExternalMemoryHostSynchronizationTestInstance::createDataBuffer (VkDeviceSize size, VkBufferUsageFlags usage)
{
	const vk::VkExternalMemoryBufferCreateInfo	externalInfo =
	{
		vk::VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,
		DE_NULL,
		(vk::VkExternalMemoryHandleTypeFlags)VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT
	};

	const VkBufferCreateInfo					dataBufferCreateInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType
		&externalInfo,							// const void*			pNext
		0,										// VkBufferCreateFlags	flag
		size,									// VkDeviceSize			size
		usage,									// VkBufferUsageFlags	usage
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode
		0,										// deUint32				queueFamilyCount
		DE_NULL									// const deUint32*		pQueueFamilyIndices
	};
	return vk::createBuffer(m_vkd, m_device, &dataBufferCreateInfo, DE_NULL);
}

void ExternalMemoryHostSynchronizationTestInstance::fillBuffer (VkDeviceSize size)
{
	const VkBufferMemoryBarrier		bufferBarrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		0u,											// VkAccessFlags	srcAccessMask;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
		*m_dataBuffer,								// VkBuffer			buffer;
		0u,											// VkDeviceSize		offset;
		size										// VkDeviceSize		size;
	};

	m_vkd.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, DE_FALSE, 0u, DE_NULL, 1u, &bufferBarrier, 0u, DE_NULL);
	m_vkd.cmdFillBuffer(*m_cmdBuffer, *m_dataBuffer, 0, size, 0xFFFFFFFF);
}

void ExternalMemoryHostSynchronizationTestInstance::verifyBufferProperties (VkBufferUsageFlags usage)
{
	const VkPhysicalDeviceExternalBufferInfo bufferInfo = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO,	// VkStructureType                       sType;
		DE_NULL,												// const void*                           pNext;
		0,														// VkBufferCreateFlags                   flags;
		usage,													// VkBufferUsageFlags                    usage;
		VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT	// VkExternalMemoryHandleTypeFlagBits    handleType;
	};

	VkExternalBufferProperties props = {
		VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES,	// VkStructureType               sType;
		DE_NULL,										// void*                         pNext;
		VkExternalMemoryProperties()					// VkExternalMemoryProperties    externalMemoryProperties;
	};

	m_context.getInstanceInterface().getPhysicalDeviceExternalBufferProperties(m_context.getPhysicalDevice(), &bufferInfo, &props);

	checkExternalMemoryProperties(props.externalMemoryProperties);
}

struct AddPrograms
{
	void init (vk::SourceCollections& sources, TestParams testParams) const
	{
			//unused parameter
			DE_UNREF(testParams);

			const char* const vertexShader =
			"#version 430\n"

			"layout(std430, binding = 0) buffer BufferPos {\n"
			"vec4 p[100];\n"
			"} pos;\n"

			"out gl_PerVertex{\n"
			"vec4 gl_Position;\n"
			"};\n"

			"void main() {\n"
			"gl_Position = pos.p[gl_VertexIndex];\n"
			"}\n";

		sources.glslSources.add("position_only.vert")
			<< glu::VertexSource(vertexShader);

		const char* const fragmentShader =
			"#version 430\n"

			"layout(location = 0) out vec4 my_FragColor;\n"

			"void main() {\n"
			"my_FragColor = vec4(0,1,0,1);\n"
			"}\n";

		sources.glslSources.add("only_color_out.frag")
			<< glu::FragmentSource(fragmentShader);
	}
};

struct FormatName
{
	vk::VkFormat	format;
	std::string		name;
};

void checkSupport (Context& context)
{
	context.requireDeviceFunctionality("VK_EXT_external_memory_host");
}

void checkEvent (Context& context)
{
	checkSupport(context);
	if (context.isDeviceFunctionalitySupported("VK_KHR_portability_subset") && !context.getPortabilitySubsetFeatures().events)
		TCU_THROW(NotSupportedError, "VK_KHR_portability_subset: Events are not supported by this implementation");
}

} // unnamed namespace

tcu::TestCaseGroup* createMemoryExternalMemoryHostTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	group(new tcu::TestCaseGroup(testCtx, "external_memory_host", "VK_EXT_external_memory_host extension tests."));
	de::MovePtr<tcu::TestCaseGroup>	simpleAllocation(new tcu::TestCaseGroup(testCtx, "simple_allocation", "simple allocation tests."));
	de::MovePtr<tcu::TestCaseGroup>	bind_image_memory_and_render(new tcu::TestCaseGroup(testCtx, "bind_image_memory_and_render", "render tests."));
	de::MovePtr<tcu::TestCaseGroup>	with_zero_offset(new tcu::TestCaseGroup(testCtx, "with_zero_offset", "bind object with zero offset specified"));
	de::MovePtr<tcu::TestCaseGroup>	with_non_zero_offset(new tcu::TestCaseGroup(testCtx, "with_non_zero_offset", "bind object with zero offset specified"));
	de::MovePtr<tcu::TestCaseGroup>	synchronization(new tcu::TestCaseGroup(testCtx, "synchronization", "synchronization tests."));

	//test cases:
	simpleAllocation->addChild(new InstanceFactory1WithSupport<ExternalMemoryHostBaseTestInstance, VkDeviceSize, FunctionSupport0> (testCtx, tcu::NODETYPE_SELF_VALIDATE, "minImportedHostPointerAlignment_x1",
																																	"allocate minImportedHostPointerAlignment multiplied by 1", 1, checkSupport));
	simpleAllocation->addChild(new InstanceFactory1WithSupport<ExternalMemoryHostBaseTestInstance, VkDeviceSize, FunctionSupport0> (testCtx, tcu::NODETYPE_SELF_VALIDATE, "minImportedHostPointerAlignment_x3",
																																	"allocate minImportedHostPointerAlignment multiplied by 3", 3, checkSupport));
	group ->addChild(simpleAllocation.release());

	const std::vector<FormatName> testFormats = {
		{ vk::VK_FORMAT_R8G8B8A8_UNORM,			"r8g8b8a8_unorm"		},
		{ vk::VK_FORMAT_R16G16B16A16_UNORM,		"r16g16b16a16_unorm"	},
		{ vk::VK_FORMAT_R16G16B16A16_SFLOAT,	"r16g16b16a16_sfloat"	},
		{ vk::VK_FORMAT_R32G32B32A32_SFLOAT,	"r32g32b32a32_sfloat"	},
	};

	for (const auto& formatName : testFormats)
	{
		with_zero_offset->addChild(new InstanceFactory1WithSupport<ExternalMemoryHostRenderImageTestInstance, TestParams, FunctionSupport0, AddPrograms>	(testCtx, tcu::NODETYPE_SELF_VALIDATE,
																																							 formatName.name, formatName.name, AddPrograms(),
																																							 TestParams(formatName.format), checkSupport));
	}
	bind_image_memory_and_render->addChild(with_zero_offset.release());

	for (const auto& formatName : testFormats)
	{
		with_non_zero_offset->addChild(new InstanceFactory1WithSupport<ExternalMemoryHostRenderImageTestInstance, TestParams, FunctionSupport0, AddPrograms>	(testCtx, tcu::NODETYPE_SELF_VALIDATE,
																																								 formatName.name, formatName.name, AddPrograms(),
																																								 TestParams(formatName.format, true), checkSupport));
	}
	bind_image_memory_and_render->addChild(with_non_zero_offset.release());

	group->addChild(bind_image_memory_and_render.release());

	synchronization->addChild(new InstanceFactory1WithSupport<ExternalMemoryHostSynchronizationTestInstance, TestParams, FunctionSupport0, AddPrograms>	(testCtx, tcu::NODETYPE_SELF_VALIDATE,
																																						 "synchronization", "synchronization", AddPrograms(),
																																						 TestParams(testFormats[0].format, true), checkEvent));
	group->addChild(synchronization.release());
	return group.release();
}

} // memory
} // vkt
