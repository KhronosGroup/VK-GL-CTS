/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 Advanced Micro Devices, Inc.
 * Copyright (c) 2019 The Khronos Group Inc.
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
 * \brief Tests for VK_AMD_buffer_marker
 *//*--------------------------------------------------------------------*/

#include "vktApiBufferMarkerTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktExternalMemoryUtil.hpp"
#include "vkPlatform.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deRandom.hpp"

#include <vector>

namespace vkt
{
namespace api
{
namespace
{
using namespace vk;
using de::UniquePtr;
using de::MovePtr;
using de::SharedPtr;
using namespace vkt::ExternalMemoryUtil;

template<typename T>
inline const T* dataOrNullPtr(const std::vector<T>& v)
{
	return (v.empty() ? DE_NULL : &v[0]);
}

template<typename T>
inline T* dataOrNullPtr(std::vector<T>& v)
{
	return (v.empty() ? DE_NULL : &v[0]);
}

//! Common test data related to the device
struct WorkingDevice
{
	Move<VkDevice>          logicalDevice;
	MovePtr<DeviceDriver>   deviceDriver;
	MovePtr<Allocator>      allocator;
	VkQueue                 queue;
	deUint32                queueFamilyIdx;
	VkQueueFamilyProperties queueProps;
};

bool queueFamilyMatchesTestCase(const VkQueueFamilyProperties& props, VkQueueFlagBits testQueue)
{
	// The goal is to find a queue family that most accurately represents the required queue flag.  For example, if flag is
	// VK_QUEUE_TRANSFER_BIT, we want to target transfer-only queues for such a test case rather than universal queues which
	// may include VK_QUEUE_TRANSFER_BIT along with other queue flags.
	const VkQueueFlags flags = props.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);

	// for VK_QUEUE_TRANSFER_BIT, target transfer-only queues:
	if (testQueue == VK_QUEUE_TRANSFER_BIT)
		return (flags == VK_QUEUE_TRANSFER_BIT);

	// for VK_QUEUE_COMPUTE_BIT, target compute only queues
	if (testQueue == VK_QUEUE_COMPUTE_BIT)
		return ((flags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == VK_QUEUE_COMPUTE_BIT);

	// for VK_QUEUE_GRAPHICS_BIT, target universal queues (queues which support graphics)
	if (testQueue == VK_QUEUE_GRAPHICS_BIT)
		return ((flags & VK_QUEUE_GRAPHICS_BIT) != 0);

	DE_FATAL("Unexpected test queue flag");

	return false;
}

// We create a custom device because we don't want to always use the universal queue.
void createDeviceWithExtension (Context& context, WorkingDevice& wd, VkQueueFlagBits testQueue, bool hostPtr)
{
	const PlatformInterface&	vkp				= context.getPlatformInterface();
	const VkInstance			instance		= context.getInstance();
	const InstanceInterface&	instanceDriver	= context.getInstanceInterface();
	const VkPhysicalDevice		physicalDevice	= context.getPhysicalDevice();

	// Create a device with extension enabled and a queue with a family which supports the buffer marker extension
	const std::vector<VkQueueFamilyProperties>	queueFamilyProperties	= getPhysicalDeviceQueueFamilyProperties(instanceDriver, physicalDevice);
	const float									queuePriority			= 1.0f;
	VkDeviceQueueCreateInfo						queueCreateInfo;
	deMemset(&queueCreateInfo, 0, sizeof(queueCreateInfo));

	for (deUint32 familyIdx = 0; familyIdx < queueFamilyProperties.size(); ++familyIdx)
	{
		if (queueFamilyMatchesTestCase(queueFamilyProperties[familyIdx], testQueue) &&
			queueFamilyProperties[familyIdx].queueCount > 0)
		{
			queueCreateInfo.sType				= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.pNext				= DE_NULL;
			queueCreateInfo.pQueuePriorities	= &queuePriority;
			queueCreateInfo.queueCount			= 1;
			queueCreateInfo.queueFamilyIndex	= familyIdx;

			break;
		}
	}

	if (queueCreateInfo.queueCount == 0)
	{
		TCU_THROW(NotSupportedError, "No compatible queue family for this test case");
	}

	std::vector<const char*> cstrDeviceExtensions;

	cstrDeviceExtensions.push_back("VK_AMD_buffer_marker");

	if (hostPtr)
		cstrDeviceExtensions.push_back("VK_EXT_external_memory_host");

	const VkDeviceCreateInfo deviceInfo =
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,				// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		0u,													// VkDeviceCreateFlags				flags;
		1,													// deUint32							queueCreateInfoCount;
		&queueCreateInfo,									// const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
		0u,													// deUint32							enabledLayerCount;
		DE_NULL,											// const char* const*				ppEnabledLayerNames;
		static_cast<deUint32>(cstrDeviceExtensions.size()),	// deUint32							enabledExtensionCount;
		dataOrNullPtr(cstrDeviceExtensions),				// const char* const*				ppEnabledExtensionNames;
		&context.getDeviceFeatures(),						// const VkPhysicalDeviceFeatures*	pEnabledFeatures;
	};

	wd.logicalDevice	= createDevice(vkp, instance, instanceDriver, physicalDevice, &deviceInfo);
	wd.deviceDriver		= MovePtr<DeviceDriver>(new DeviceDriver(vkp, instance, *wd.logicalDevice));
	wd.allocator		= MovePtr<Allocator>(new SimpleAllocator(*wd.deviceDriver, *wd.logicalDevice, getPhysicalDeviceMemoryProperties(instanceDriver, physicalDevice)));
	wd.queueFamilyIdx	= queueCreateInfo.queueFamilyIndex;
	wd.queue			= getDeviceQueue(*wd.deviceDriver, *wd.logicalDevice, wd.queueFamilyIdx, 0u);
	wd.queueProps		= queueFamilyProperties[queueCreateInfo.queueFamilyIndex];
}

bool checkMarkerBuffer	(const DeviceInterface& vk, VkDevice device, const MovePtr<vk::Allocation>& memory, size_t offset,
						 const std::vector<deUint32>& expected)
{
	invalidateMappedMemoryRange(vk, device, memory->getMemory(), memory->getOffset(), VK_WHOLE_SIZE);

	const deUint32* data = reinterpret_cast<const deUint32*>(static_cast<const char*>(memory->getHostPtr()) + offset);

	for (size_t i = 0; i < expected.size(); ++i)
	{
		if (data[i] != expected[i])
			return false;
	}

	return true;
}

struct BaseTestParams
{
	VkQueueFlagBits			testQueue;	// Queue type that this test case targets
	VkPipelineStageFlagBits stage;		// Pipeline stage where any marker writes for this test case occur in
	deUint32				size;		// Number of buffer markers
	bool					useHostPtr;	// Whether to use host pointer as backing buffer memory
};

deUint32 chooseExternalMarkerMemoryType(const DeviceInterface&				vkd,
										VkDevice							device,
										VkExternalMemoryHandleTypeFlagBits	externalType,
										deUint32							allowedBits,
										MovePtr<ExternalHostMemory>&		hostMemory)
{
	VkMemoryHostPointerPropertiesEXT props =
	{
		vk::VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT,
		DE_NULL,
		0u,
	};

	if (vkd.getMemoryHostPointerPropertiesEXT(device, externalType, hostMemory->data, &props) == VK_SUCCESS)
	{
		allowedBits &= props.memoryTypeBits;
	}

	deUint32 index = 0;

	while ((index < VK_MAX_MEMORY_TYPES) && ((allowedBits & 0x1) == 0))
	{
		index++;
		allowedBits >>= 1;
	}

	return index;
}

class ExternalHostAllocation : public Allocation
{
public:
	ExternalHostAllocation(Move<VkDeviceMemory> mem, void* hostPtr) : Allocation(*mem, (VkDeviceSize)0, hostPtr), m_memHolder(mem) { }

private:
	const Unique<VkDeviceMemory>	m_memHolder;
};

void createMarkerBufferMemory(const InstanceInterface&		vki,
							 const DeviceInterface&			vkd,
							 VkPhysicalDevice				physicalDevice,
							 VkDevice						device,
							 VkBuffer						buffer,
							 MovePtr<Allocator>&			allocator,
							 const MemoryRequirement		allocRequirement,
							 bool							externalHostPtr,
							 MovePtr<ExternalHostMemory>&	hostMemory,
							 MovePtr<Allocation>&			deviceMemory)
{
	VkMemoryRequirements memReqs = getBufferMemoryRequirements(vkd, device, buffer);

	if (externalHostPtr == false)
	{
		deviceMemory = allocator->allocate(memReqs, allocRequirement);
	}
	else
	{
		const VkExternalMemoryHandleTypeFlagBits externalType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;

		const VkPhysicalDeviceExternalMemoryHostPropertiesEXT hostProps = getPhysicalDeviceExternalMemoryHostProperties(vki, physicalDevice);

		hostMemory = MovePtr<ExternalHostMemory>(new ExternalHostMemory(memReqs.size, hostProps.minImportedHostPointerAlignment));

		const deUint32 externalMemType = chooseExternalMarkerMemoryType(vkd, device, externalType, memReqs.memoryTypeBits, hostMemory);

		if (externalMemType == VK_MAX_MEMORY_TYPES)
		{
			TCU_FAIL("Failed to find compatible external host memory type for marker buffer");
		}

		const VkImportMemoryHostPointerInfoEXT	importInfo =
		{
			VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT,
			DE_NULL,
			externalType,
			hostMemory->data
		};

		const VkMemoryAllocateInfo				info =
		{
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			(const void*)&importInfo,
			hostMemory->size,
			externalMemType
		};

		deviceMemory = MovePtr<Allocation>(new ExternalHostAllocation(allocateMemory(vkd, device, &info), hostMemory->data));
	}

	VK_CHECK(vkd.bindBufferMemory(device, buffer, deviceMemory->getMemory(), deviceMemory->getOffset()));
}

tcu::TestStatus bufferMarkerSequential(Context& context, BaseTestParams params)
{
	WorkingDevice wd;

	createDeviceWithExtension(context, wd, params.testQueue, params.useHostPtr);

	const DeviceInterface&			vk(*wd.deviceDriver);
	const VkDevice					device(*wd.logicalDevice);
	const VkDeviceSize				markerBufferSize(params.size * sizeof(deUint32));
	Move<VkBuffer>					markerBuffer(makeBuffer(vk, device, markerBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	MovePtr<ExternalHostMemory>		hostMemory;
	MovePtr<Allocation>				markerMemory;

	createMarkerBufferMemory(context.getInstanceInterface(), vk, context.getPhysicalDevice(), device,
							 *markerBuffer, wd.allocator, MemoryRequirement::HostVisible, params.useHostPtr, hostMemory, markerMemory);

	de::Random						rng(12345 ^ params.size);
	std::vector<deUint32>			expected(params.size);

	for (size_t i = 0; i < params.size; ++i)
		expected[i] = rng.getUint32();

	deMemcpy(markerMemory->getHostPtr(), &expected[0], static_cast<size_t>(markerBufferSize));
	flushMappedMemoryRange(vk, device, markerMemory->getMemory(), markerMemory->getOffset(), VK_WHOLE_SIZE);

	const Unique<VkCommandPool>		cmdPool(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, wd.queueFamilyIdx));
	const Unique<VkCommandBuffer>	cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	beginCommandBuffer(vk, *cmdBuffer);

	for (size_t i = 0; i < params.size; ++i)
	{
		vk.cmdWriteBufferMarkerAMD(*cmdBuffer, params.stage, *markerBuffer, static_cast<VkDeviceSize>(sizeof(deUint32) * i), expected[i]);
	}

	const VkMemoryBarrier memoryDep =
	{
		VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		DE_NULL,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_HOST_READ_BIT,
	};

	vk.cmdPipelineBarrier(*cmdBuffer, params.stage, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &memoryDep, 0, DE_NULL, 0, DE_NULL);

	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

	submitCommandsAndWait(vk, device, wd.queue, *cmdBuffer);

	if (!checkMarkerBuffer(vk, device, markerMemory, 0, expected))
		return tcu::TestStatus::fail("Some marker values were incorrect");

	return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus bufferMarkerOverwrite(Context& context, BaseTestParams params)
{
	WorkingDevice wd;

	createDeviceWithExtension(context, wd, params.testQueue, params.useHostPtr);

	const DeviceInterface&			vk(*wd.deviceDriver);
	const VkDevice					device(*wd.logicalDevice);
	const VkDeviceSize				markerBufferSize(params.size * sizeof(deUint32));
	Move<VkBuffer>					markerBuffer(makeBuffer(vk, device, markerBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT));
	MovePtr<ExternalHostMemory>		hostMemory;
	MovePtr<Allocation>				markerMemory;

	createMarkerBufferMemory(context.getInstanceInterface(), vk, context.getPhysicalDevice(), device,
							 *markerBuffer, wd.allocator, MemoryRequirement::HostVisible, params.useHostPtr, hostMemory, markerMemory);

	de::Random						rng(12345 ^ params.size);
	std::vector<deUint32>			expected(params.size);

	for (size_t i = 0; i < params.size; ++i)
		expected[i] = 0;

	deMemcpy(markerMemory->getHostPtr(), &expected[0], static_cast<size_t>(markerBufferSize));
	flushMappedMemoryRange(vk, device, markerMemory->getMemory(), markerMemory->getOffset(), VK_WHOLE_SIZE);

	const Unique<VkCommandPool>		cmdPool(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, wd.queueFamilyIdx));
	const Unique<VkCommandBuffer>	cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	beginCommandBuffer(vk, *cmdBuffer);

	for (deUint32 i = 0; i < params.size * 10; ++i)
	{
		const deUint32 slot  = rng.getUint32() % static_cast<deUint32>(params.size);
		const deUint32 value = i;

		expected[slot] = value;

		vk.cmdWriteBufferMarkerAMD(*cmdBuffer, params.stage, *markerBuffer, static_cast<VkDeviceSize>(sizeof(deUint32) * slot), expected[slot]);
	}

	const VkMemoryBarrier memoryDep = {
		VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		DE_NULL,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_HOST_READ_BIT,
	};

	vk.cmdPipelineBarrier(*cmdBuffer, params.stage, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &memoryDep, 0, DE_NULL, 0, DE_NULL);

	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

	submitCommandsAndWait(vk, device, wd.queue, *cmdBuffer);

	if (!checkMarkerBuffer(vk, device, markerMemory, 0, expected))
		return tcu::TestStatus::fail("Some marker values were incorrect");

	return tcu::TestStatus::pass("Pass");
}

enum MemoryDepMethod
{
	MEMORY_DEP_DRAW,
	MEMORY_DEP_DISPATCH,
	MEMORY_DEP_COPY
};

struct MemoryDepParams
{
	BaseTestParams			base;
	MemoryDepMethod			method;
};

enum MemoryDepOwner
{
	MEMORY_DEP_OWNER_NOBODY = 0,
	MEMORY_DEP_OWNER_MARKER = 1,
	MEMORY_DEP_OWNER_NON_MARKER = 2
};

void computeMemoryDepBarrier(MemoryDepMethod			method,
							 MemoryDepOwner				owner,
							 VkPipelineStageFlagBits	markerStage,
							 VkAccessFlags*				memoryDepAccess,
							 VkPipelineStageFlags*		executionScope)
{
	DE_ASSERT(owner != MEMORY_DEP_OWNER_NOBODY);

	if (owner == MEMORY_DEP_OWNER_MARKER)
	{
		*memoryDepAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
		*executionScope  = markerStage;
	}
	else
	{
		if (method == MEMORY_DEP_COPY)
		{
			*memoryDepAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
			*executionScope  = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (method == MEMORY_DEP_DISPATCH)
		{
			*memoryDepAccess = VK_ACCESS_SHADER_WRITE_BIT;
			*executionScope  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		}
		else
		{
			*memoryDepAccess = VK_ACCESS_SHADER_WRITE_BIT;
			*executionScope  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
	}
}

// Randomly do buffer marker writes and other operations (draws, dispatches) that shader-write to a shared buffer.  Insert pipeline barriers
// when necessary and make sure that the synchronization between marker writes and non-marker writes are correctly handled by the barriers.
tcu::TestStatus bufferMarkerMemoryDep(Context& context, MemoryDepParams params)
{
	WorkingDevice wd;

	createDeviceWithExtension(context, wd, params.base.testQueue, params.base.useHostPtr);

	VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	if ((params.method == MEMORY_DEP_DRAW) || (params.method == MEMORY_DEP_DISPATCH))
		usageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	else
		usageFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	const deUint32					numIters(1000);
	const DeviceInterface&			vk(*wd.deviceDriver);
	const VkDevice					device(*wd.logicalDevice);
	const deUint32					size(params.base.size);
	const VkDeviceSize				markerBufferSize(params.base.size * sizeof(deUint32));
	Move<VkBuffer>					markerBuffer(makeBuffer(vk, device, params.base.size * sizeof(deUint32), usageFlags));
	MovePtr<ExternalHostMemory>		hostMemory;
	MovePtr<Allocation>				markerMemory;

	createMarkerBufferMemory(context.getInstanceInterface(), vk, context.getPhysicalDevice(), device,
		*markerBuffer, wd.allocator, MemoryRequirement::HostVisible, params.base.useHostPtr, hostMemory, markerMemory);

	de::Random						rng(size ^ params.base.size);
	std::vector<deUint32>			expected(params.base.size, 0);

	Move<VkDescriptorPool>			descriptorPool;
	Move<VkDescriptorSetLayout>		descriptorSetLayout;
	Move<VkDescriptorSet>			descriptorSet;
	Move<VkPipelineLayout>			pipelineLayout;
	VkShaderStageFlags				pushConstantStage = 0;

	if ((params.method == MEMORY_DEP_DRAW) || (params.method == MEMORY_DEP_DISPATCH))
	{
		DescriptorPoolBuilder descriptorPoolBuilder;

		descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u);
		descriptorPool = descriptorPoolBuilder.build(vk, device, 0, 1u);

		DescriptorSetLayoutBuilder setLayoutBuilder;

		setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL);
		descriptorSetLayout = setLayoutBuilder.build(vk, device);

		const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			*descriptorPool,									// VkDescriptorPool				descriptorPool;
			1u,													// deUint32						setLayoutCount;
			&descriptorSetLayout.get()						// const VkDescriptorSetLayout*	pSetLayouts;
		};

		descriptorSet = allocateDescriptorSet(vk, device, &descriptorSetAllocateInfo);

		VkDescriptorBufferInfo markerBufferInfo = { *markerBuffer, 0, VK_WHOLE_SIZE };

		VkWriteDescriptorSet writeSet[] =
		{
			{
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,	// VkStructureType                  sType;
				DE_NULL,								// const void*                      pNext;
				descriptorSet.get(),					// VkDescriptorSet                  dstSet;
				0,										// uint32_t                         dstBinding;
				0,										// uint32_t                         dstArrayElement;
				1,										// uint32_t                         descriptorCount;
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,		// VkDescriptorType                 descriptorType;
				DE_NULL,								// const VkDescriptorImageInfo*     pImageInfo;
				&markerBufferInfo,						// const VkDescriptorBufferInfo*    pBufferInfo;
				DE_NULL									// const VkBufferView*              pTexelBufferViev
			}
		};

		vk.updateDescriptorSets(device, DE_LENGTH_OF_ARRAY(writeSet), writeSet, 0, DE_NULL);

		VkDescriptorSetLayout setLayout = descriptorSetLayout.get();

		pushConstantStage = (params.method == MEMORY_DEP_DISPATCH ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_FRAGMENT_BIT);

		const VkPushConstantRange pushConstantRange =
		{
			pushConstantStage,	// VkShaderStageFlags    stageFlags;
			0u,					// uint32_t              offset;
			2*sizeof(deUint32),	// uint32_t              size;
		};

		const VkPipelineLayoutCreateInfo pipelineLayoutInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			(VkPipelineLayoutCreateFlags)0,						// VkPipelineLayoutCreateFlags	flags;
			1u,													// deUint32						setLayoutCount;
			&setLayout,											// const VkDescriptorSetLayout*	pSetLayouts;
			1u,													// deUint32						pushConstantRangeCount;
			&pushConstantRange,									// const VkPushConstantRange*	pPushConstantRanges;
		};

		pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutInfo);
	}

	Move<VkRenderPass>		renderPass;
	Move<VkFramebuffer>		fbo;
	Move<VkPipeline>		pipeline;
	Move<VkShaderModule>	vertexModule;
	Move<VkShaderModule>	fragmentModule;
	Move<VkShaderModule>	computeModule;

	if (params.method == MEMORY_DEP_DRAW)
	{
		const VkSubpassDescription subpassInfo =
		{
			0,									// VkSubpassDescriptionFlags       flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,	// VkPipelineBindPoint             pipelineBindPoint;
			0,									// uint32_t                        inputAttachmentCount;
			DE_NULL,							// const VkAttachmentReference*    pInputAttachments;
			0,									// uint32_t                        colorAttachmentCount;
			DE_NULL,							// const VkAttachmentReference*    pColorAttachments;
			0,									// const VkAttachmentReference*    pResolveAttachments;
			DE_NULL,							// const VkAttachmentReference*    pDepthStencilAttachment;
			0,									// uint32_t                        preserveAttachmentCount;
			DE_NULL								// const uint32_t*                 pPreserveAttachments;
		};

		const VkRenderPassCreateInfo renderPassInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// VkStructureType                   sType;
			DE_NULL,									// const void*                       pNext;
			0,											// VkRenderPassCreateFlags           flags;
			0,											// uint32_t                          attachmentCount;
			DE_NULL,									// const VkAttachmentDescription*    pAttachments;
			1,											// uint32_t                          subpassCount;
			&subpassInfo,								// const VkSubpassDescription*       pSubpasses;
			0,											// uint32_t                          dependencyCount;
			DE_NULL										// const VkSubpassDependency*        pDependencies
		};

		renderPass = createRenderPass(vk, device, &renderPassInfo);

		const VkFramebufferCreateInfo framebufferInfo =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType             sType;
			DE_NULL,									// const void*                 pNext;
			0,											// VkFramebufferCreateFlags    flags;
			renderPass.get(),							// VkRenderPass                renderPass;
			0,											// uint32_t                    attachmentCount;
			DE_NULL,									// const VkImageView*          pAttachments;
			1,											// uint32_t                    width;
			1,											// uint32_t                    height;
			1,											// uint32_t                    layers;
		};

		fbo = createFramebuffer(vk, device, &framebufferInfo);

		vertexModule   = createShaderModule(vk, device, context.getBinaryCollection().get("vert"), 0u);
		fragmentModule = createShaderModule(vk, device, context.getBinaryCollection().get("frag"), 0u);

		const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			(VkPipelineVertexInputStateCreateFlags)0,						// VkPipelineVertexInputStateCreateFlags	flags;
			0,																// uint32_t									vertexBindingDescriptionCount;
			DE_NULL,														// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			0,																// uint32_t									vertexAttributeDescriptionCount;
			DE_NULL,														// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			(VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags	flags;
			VK_PRIMITIVE_TOPOLOGY_POINT_LIST,								// VkPrimitiveTopology						topology;
			VK_FALSE,														// VkBool32									primitiveRestartEnable;
		};

		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

		{
			const VkPipelineShaderStageCreateInfo createInfo =
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
				DE_NULL,												// const void*							pNext;
				(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags		flags;
				VK_SHADER_STAGE_VERTEX_BIT,								// VkShaderStageFlagBits				stage;
				vertexModule.get(),										// VkShaderModule						module;
				"main",													// const char*							pName;
				DE_NULL,												// const VkSpecializationInfo*			pSpecializationInfo;
			};

			shaderStages.push_back(createInfo);
		}

		{
			const VkPipelineShaderStageCreateInfo createInfo =
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
				DE_NULL,												// const void*							pNext;
				(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags		flags;
				VK_SHADER_STAGE_FRAGMENT_BIT,							// VkShaderStageFlagBits				stage;
				fragmentModule.get(),									// VkShaderModule						module;
				"main",													// const char*							pName;
				DE_NULL,												// const VkSpecializationInfo*			pSpecializationInfo;
			};

			shaderStages.push_back(createInfo);
		}

		VkViewport viewport;

		viewport.x			= 0;
		viewport.y			= 0;
		viewport.width		= 1;
		viewport.height		= 1;
		viewport.minDepth	= 0.0f;
		viewport.maxDepth	= 1.0f;

		VkRect2D scissor;

		scissor.offset.x		= 0;
		scissor.offset.y		= 0;
		scissor.extent.width	= 1;
		scissor.extent.height	= 1;

		const VkPipelineViewportStateCreateInfo pipelineViewportStateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,			// VkStructureType						sType;
			DE_NULL,														// const void*							pNext;
			(VkPipelineViewportStateCreateFlags)0,							// VkPipelineViewportStateCreateFlags	flags;
			1u,																// uint32_t								viewportCount;
			&viewport,														// const VkViewport*					pViewports;
			1u,																// uint32_t								scissorCount;
			&scissor,														// const VkRect2D*						pScissors;
		};

		const VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			(VkPipelineRasterizationStateCreateFlags)0,					// VkPipelineRasterizationStateCreateFlags	flags;
			VK_FALSE,													// VkBool32									depthClampEnable;
			VK_FALSE,													// VkBool32									rasterizerDiscardEnable;
			VK_POLYGON_MODE_FILL,										// VkPolygonMode							polygonMode;
			VK_CULL_MODE_NONE,											// VkCullModeFlags							cullMode;
			VK_FRONT_FACE_COUNTER_CLOCKWISE,							// VkFrontFace								frontFace;
			VK_FALSE,													// VkBool32									depthBiasEnable;
			0.0f,														// float									depthBiasConstantFactor;
			0.0f,														// float									depthBiasClamp;
			0.0f,														// float									depthBiasSlopeFactor;
			1.0f,														// float									lineWidth;
		};

		const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo =
		{

			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			(VkPipelineMultisampleStateCreateFlags)0,					// VkPipelineMultisampleStateCreateFlags	flags;
			VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits					rasterizationSamples;
			VK_FALSE,													// VkBool32									sampleShadingEnable;
			1.0f,														// float									minSampleShading;
			DE_NULL,													// const VkSampleMask*						pSampleMask;
			VK_FALSE,													// VkBool32									alphaToCoverageEnable;
			VK_FALSE,													// VkBool32									alphaToOneEnable;
		};

		const VkStencilOpState						noStencilOp					=
		{
			VK_STENCIL_OP_KEEP,		// VkStencilOp    failOp
			VK_STENCIL_OP_KEEP,		// VkStencilOp    passOp
			VK_STENCIL_OP_KEEP,		// VkStencilOp    depthFailOp
			VK_COMPARE_OP_NEVER,	// VkCompareOp    compareOp
			0,						// deUint32       compareMask
			0,						// deUint32       writeMask
			0						// deUint32       reference
		};

		VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			(VkPipelineDepthStencilStateCreateFlags)0,					// VkPipelineDepthStencilStateCreateFlags	flags;
			VK_FALSE,													// VkBool32									depthTestEnable;
			VK_FALSE,													// VkBool32									depthWriteEnable;
			VK_COMPARE_OP_ALWAYS,										// VkCompareOp								depthCompareOp;
			VK_FALSE,													// VkBool32									depthBoundsTestEnable;
			VK_FALSE,													// VkBool32									stencilTestEnable;
			noStencilOp,												// VkStencilOpState							front;
			noStencilOp,												// VkStencilOpState							back;
			0.0f,														// float									minDepthBounds;
			1.0f,														// float									maxDepthBounds;
		};

		const VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
			DE_NULL,													// const void*									pNext;
			(VkPipelineColorBlendStateCreateFlags)0,					// VkPipelineColorBlendStateCreateFlags			flags;
			VK_FALSE,													// VkBool32										logicOpEnable;
			VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
			0,															// deUint32										attachmentCount;
			DE_NULL,													// const VkPipelineColorBlendAttachmentState*	pAttachments;
			{ 0.0f, 0.0f, 0.0f, 0.0f },									// float										blendConstants[4];
		};

		const VkGraphicsPipelineCreateInfo	graphicsPipelineInfo =
		{
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,			// VkStructureType									sType;
			DE_NULL,													// const void*										pNext;
			(VkPipelineCreateFlags)0,									// VkPipelineCreateFlags							flags;
			static_cast<deUint32>(shaderStages.size()),					// deUint32											stageCount;
			dataOrNullPtr(shaderStages),								// const VkPipelineShaderStageCreateInfo*			pStages;
			&vertexInputStateInfo,										// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
			&pipelineInputAssemblyStateInfo,							// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
			DE_NULL,													// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
			&pipelineViewportStateInfo,									// const VkPipelineViewportStateCreateInfo*			pViewportState;
			&pipelineRasterizationStateInfo,							// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
			&pipelineMultisampleStateInfo,								// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
			&pipelineDepthStencilStateInfo,								// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
			&pipelineColorBlendStateInfo,								// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
			DE_NULL,													// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
			pipelineLayout.get(),										// VkPipelineLayout									layout;
			renderPass.get(),											// VkRenderPass										renderPass;
			0,															// deUint32											subpass;
			DE_NULL,													// VkPipeline										basePipelineHandle;
			-1,															// deInt32											basePipelineIndex;
		};

		pipeline = createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineInfo);
	}
	else if (params.method == MEMORY_DEP_DISPATCH)
	{
		computeModule = createShaderModule(vk, device, context.getBinaryCollection().get("comp"), 0u);

		const VkPipelineShaderStageCreateInfo shaderStageInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
			DE_NULL,												// const void*							pNext;
			(VkPipelineShaderStageCreateFlags)0,					// VkPipelineShaderStageCreateFlags		flags;
			VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits				stage;
			computeModule.get(),									// VkShaderModule						module;
			"main",													// const char*							pName;
			DE_NULL													// const VkSpecializationInfo*			pSpecializationInfo;
		};

		const VkComputePipelineCreateInfo computePipelineInfo =
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType                    sType;
			DE_NULL,										// const void*                        pNext;
			0u,												// VkPipelineCreateFlags              flags;
			shaderStageInfo,								// VkPipelineShaderStageCreateInfo    stage;
			pipelineLayout.get(),							// VkPipelineLayout                   layout;
			DE_NULL,										// VkPipeline                         basePipelineHandle;
			0												// int32_t                            basePipelineIndex;
		};

		pipeline = createComputePipeline(vk, device, DE_NULL, &computePipelineInfo);
	}

	deMemcpy(markerMemory->getHostPtr(), &expected[0], static_cast<size_t>(markerBufferSize));
	flushMappedMemoryRange(vk, device, markerMemory->getMemory(), markerMemory->getOffset(), VK_WHOLE_SIZE);

	const Unique<VkCommandPool>		cmdPool(createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, wd.queueFamilyIdx));
	const Unique<VkCommandBuffer>	cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	beginCommandBuffer(vk, *cmdBuffer);

	VkDescriptorSet setHandle = *descriptorSet;

	std::vector<MemoryDepOwner>	dataOwner(size, MEMORY_DEP_OWNER_NOBODY);

	if (params.method == MEMORY_DEP_DRAW)
	{
		const VkRenderPassBeginInfo beginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// VkStructureType        sType;
			DE_NULL,									// const void*            pNext;
			renderPass.get(),							// VkRenderPass           renderPass;
			fbo.get(),									// VkFramebuffer          framebuffer;
			{ { 0, 0, }, { 1, 1 } },					// VkRect2D               renderArea;
			0,											// uint32_t               clearValueCount;
			DE_NULL										// const VkClearValue*    pClearValues;
		};

		vk.cmdBeginRenderPass(*cmdBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &setHandle, 0, DE_NULL);
	}
	else if (params.method == MEMORY_DEP_DISPATCH)
	{
		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1, &setHandle, 0, DE_NULL);
	}

	deMemcpy(markerMemory->getHostPtr(), &expected[0], static_cast<size_t>(markerBufferSize));
	flushMappedMemoryRange(vk, device, markerMemory->getMemory(), markerMemory->getOffset(), VK_WHOLE_SIZE);

	deUint32 writeStages = 0;
	deUint32 writeAccess = 0;

	for (deUint32 i = 0; i < numIters; ++i)
	{
		deUint32		slot		= rng.getUint32() % size;
		MemoryDepOwner	oldOwner	= dataOwner[slot];
		MemoryDepOwner	newOwner	= static_cast<MemoryDepOwner>(1 + (rng.getUint32() % 2));

		DE_ASSERT(newOwner == MEMORY_DEP_OWNER_MARKER || newOwner == MEMORY_DEP_OWNER_NON_MARKER);
		DE_ASSERT(slot < size);

		if ((oldOwner != newOwner && oldOwner != MEMORY_DEP_OWNER_NOBODY) ||
			(oldOwner == MEMORY_DEP_OWNER_NON_MARKER && newOwner == MEMORY_DEP_OWNER_NON_MARKER))
		{
			VkBufferMemoryBarrier memoryDep =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,    // VkStructureType    sType;
				DE_NULL,                                    // const void*        pNext;
				0,                                          // VkAccessFlags      srcAccessMask;
				0,                                          // VkAccessFlags      dstAccessMask;
				wd.queueFamilyIdx,                          // uint32_t           srcQueueFamilyIndex;
				wd.queueFamilyIdx,                          // uint32_t           dstQueueFamilyIndex;
				*markerBuffer,                              // VkBuffer           buffer;
				sizeof(deUint32) * slot,                    // VkDeviceSize       offset;
				sizeof(deUint32)                            // VkDeviceSize       size;
			};

			VkPipelineStageFlags srcStageMask;
			VkPipelineStageFlags dstStageMask;

			computeMemoryDepBarrier(params.method, oldOwner, params.base.stage, &memoryDep.srcAccessMask, &srcStageMask);
			computeMemoryDepBarrier(params.method, newOwner, params.base.stage, &memoryDep.dstAccessMask, &dstStageMask);

			vk.cmdPipelineBarrier(*cmdBuffer, srcStageMask, dstStageMask, 0, 0, DE_NULL, 1, &memoryDep, 0, DE_NULL);
		}

		const deUint32 value = i;

		if (newOwner == MEMORY_DEP_OWNER_MARKER)
		{
			vk.cmdWriteBufferMarkerAMD(*cmdBuffer, params.base.stage, *markerBuffer, sizeof(deUint32) * slot, value);

			writeStages |= params.base.stage;
			writeAccess |= VK_ACCESS_TRANSFER_WRITE_BIT;
		}
		else
		{
			DE_ASSERT(newOwner == MEMORY_DEP_OWNER_NON_MARKER);

			if (params.method == MEMORY_DEP_COPY)
			{
				vk.cmdUpdateBuffer(*cmdBuffer, *markerBuffer, sizeof(deUint32) * slot, sizeof(deUint32), &value);

				writeStages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
				writeAccess |= VK_ACCESS_TRANSFER_WRITE_BIT;
			}
			else if (params.method == MEMORY_DEP_DRAW)
			{
				const deUint32 pushConst[] = { slot, value };

				vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, pushConstantStage, 0, sizeof(pushConst), pushConst);
				vk.cmdDraw(*cmdBuffer, 1, 1, i, 0);

				writeStages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				writeAccess |= VK_ACCESS_SHADER_WRITE_BIT;
			}
			else
			{
				const deUint32 pushConst[] = { slot, value };

				vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, pushConstantStage, 0, sizeof(pushConst), pushConst);
				vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

				writeStages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
				writeAccess |= VK_ACCESS_SHADER_WRITE_BIT;
			}
		}

		dataOwner[slot] = newOwner;
		expected[slot]  = value;
	}

	if (params.method == MEMORY_DEP_DRAW)
	{
		vk.cmdEndRenderPass(*cmdBuffer);
	}

	const VkMemoryBarrier memoryDep =
	{
		VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		DE_NULL,
		writeAccess,
		VK_ACCESS_HOST_READ_BIT,
	};

	vk.cmdPipelineBarrier(*cmdBuffer, writeStages, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &memoryDep, 0, DE_NULL, 0, DE_NULL);

	VK_CHECK(vk.endCommandBuffer(*cmdBuffer));

	submitCommandsAndWait(vk, device, wd.queue, *cmdBuffer);

	if (!checkMarkerBuffer(vk, device, markerMemory, 0, expected))
		return tcu::TestStatus::fail("Some marker values were incorrect");

	return tcu::TestStatus::pass("Pass");
}

void initMemoryDepPrograms(SourceCollections& programCollection, const MemoryDepParams params)
{
	if (params.method == MEMORY_DEP_DRAW)
	{
		{
			std::ostringstream src;

            src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                << "layout(location = 0) flat out uint offset;\n"
                << "out gl_PerVertex { vec4 gl_Position; };\n"
				<< "void main() {\n"
				<< "	offset = gl_VertexIndex;\n"
				<< "	gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		{
			std::ostringstream src;

			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "layout(push_constant) uniform Constants { uvec2 params; } pc;\n"
				<< "layout(std430, set = 0, binding = 0) buffer Data { uint elems[]; } data;\n"
				<< "layout(location = 0) flat in uint offset;\n"
				<< "void main() {\n"
				<< "	data.elems[pc.params.x] = pc.params.y;\n"
				<< "}\n";

			programCollection.glslSources.add("frag") << glu::FragmentSource(src.str());
		}
	}
	else if (params.method == MEMORY_DEP_DISPATCH)
	{
		{
			std::ostringstream src;

			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
				<< "layout(local_size_x = 1u, local_size_y = 1u, local_size_z = 1u) in;\n"
				<< "layout(push_constant) uniform Constants { uvec2 params; } pc;\n"
				<< "layout(std430, set = 0, binding = 0) buffer Data { uint elems[]; } data;\n"
				<< "void main() {\n"
				<< "	data.elems[pc.params.x] = pc.params.y;\n"
				<< "}\n";

			programCollection.glslSources.add("comp") << glu::ComputeSource(src.str());
		}
	}
}

void checkBufferMarkerSupport (Context& context, BaseTestParams params)
{
	if (params.useHostPtr)
		context.requireDeviceFunctionality("VK_EXT_external_memory_host");

	context.requireDeviceFunctionality("VK_AMD_buffer_marker");
}

void checkBufferMarkerSupport (Context& context, MemoryDepParams params)
{
	if (params.base.useHostPtr)
		context.requireDeviceFunctionality("VK_EXT_external_memory_host");

	context.requireDeviceFunctionality("VK_AMD_buffer_marker");
}

tcu::TestCaseGroup* createBufferMarkerTestsInGroup(tcu::TestContext& testCtx)
{
	tcu::TestCaseGroup* root = (new tcu::TestCaseGroup(testCtx, "buffer_marker", "AMD_buffer_marker Tests"));

	VkQueueFlagBits queues[] = { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_COMPUTE_BIT, VK_QUEUE_TRANSFER_BIT };
	const char* queueNames[] = { "graphics", "compute", "transfer" };

	BaseTestParams base;
	deMemset(&base, 0, sizeof(base));

	for (size_t queueNdx = 0; queueNdx < DE_LENGTH_OF_ARRAY(queues); ++queueNdx)
	{
		tcu::TestCaseGroup* queueGroup = (new tcu::TestCaseGroup(testCtx, queueNames[queueNdx], "Buffer marker tests for a specific queue family"));

		const char* memoryNames[] = { "external_host_mem", "default_mem" };
		const bool memoryTypes[] = { true, false };

		base.testQueue = queues[queueNdx];

		for (size_t memNdx = 0; memNdx < DE_LENGTH_OF_ARRAY(memoryTypes); ++memNdx)
		{
			tcu::TestCaseGroup* memoryGroup = (new tcu::TestCaseGroup(testCtx, memoryNames[memNdx], "Buffer marker tests for different kinds of backing memory"));

			base.useHostPtr = memoryTypes[memNdx];

			VkPipelineStageFlagBits stages[] = { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT };
			const char* stageNames[] = { "top_of_pipe", "bottom_of_pipe" };

			for (size_t stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stages); ++stageNdx)
			{
				tcu::TestCaseGroup* stageGroup = (new tcu::TestCaseGroup(testCtx, stageNames[stageNdx], "Buffer marker tests for a specific pipeline stage"));

				base.stage = stages[stageNdx];

				{
					tcu::TestCaseGroup* sequentialGroup = (new tcu::TestCaseGroup(testCtx, "sequential", "Buffer marker tests for sequentially writing"));

					base.size = 4;

					addFunctionCase(sequentialGroup, "4", "Writes 4 sequential marker values into a buffer", checkBufferMarkerSupport, bufferMarkerSequential, base);

					base.size = 64;

					addFunctionCase(sequentialGroup, "64", "Writes 64 sequential marker values into a buffer", checkBufferMarkerSupport, bufferMarkerSequential, base);

					base.size = 65536;

					addFunctionCase(sequentialGroup, "65536", "Writes 65536 sequential marker values into a buffer", checkBufferMarkerSupport, bufferMarkerSequential, base);

					stageGroup->addChild(sequentialGroup);
				}

				{
					tcu::TestCaseGroup* overwriteGroup = (new tcu::TestCaseGroup(testCtx, "overwrite", "Buffer marker tests for overwriting values with implicit synchronization"));

					base.size = 1;

					addFunctionCase(overwriteGroup, "1", "Randomly overwrites marker values to a 1-size buffer", checkBufferMarkerSupport, bufferMarkerOverwrite, base);

					base.size = 4;

					addFunctionCase(overwriteGroup, "4", "Randomly overwrites marker values to a 4-size buffer", checkBufferMarkerSupport, bufferMarkerOverwrite, base);

					base.size = 64;

					addFunctionCase(overwriteGroup, "64", "Randomly overwrites markers values to a 64-size buffer", checkBufferMarkerSupport, bufferMarkerOverwrite, base);

					stageGroup->addChild(overwriteGroup);
				}

				{
					tcu::TestCaseGroup* memoryDepGroup = (new tcu::TestCaseGroup(testCtx, "memory_dep", "Buffer marker tests for memory dependencies between marker writes and other operations"));

					MemoryDepParams params;
					deMemset(&params, 0, sizeof(params));

					params.base		 = base;
					params.base.size = 128;

					if (params.base.testQueue == VK_QUEUE_GRAPHICS_BIT)
					{
						params.method = MEMORY_DEP_DRAW;

						addFunctionCaseWithPrograms(memoryDepGroup, "draw", "Test memory dependencies between marker writes and draws", checkBufferMarkerSupport, initMemoryDepPrograms, bufferMarkerMemoryDep, params);
					}

					if (params.base.testQueue != VK_QUEUE_TRANSFER_BIT)
					{
						params.method = MEMORY_DEP_DISPATCH;

						addFunctionCaseWithPrograms(memoryDepGroup, "dispatch", "Test memory dependencies between marker writes and compute dispatches", checkBufferMarkerSupport, initMemoryDepPrograms, bufferMarkerMemoryDep, params);
					}

					params.method = MEMORY_DEP_COPY;

					addFunctionCaseWithPrograms(memoryDepGroup, "buffer_copy", "Test memory dependencies between marker writes and buffer copies", checkBufferMarkerSupport, initMemoryDepPrograms, bufferMarkerMemoryDep, params);

					stageGroup->addChild(memoryDepGroup);
				}

				memoryGroup->addChild(stageGroup);
			}

			queueGroup->addChild(memoryGroup);
		}

		root->addChild(queueGroup);
	}

	return root;
}

} // anonymous ns

tcu::TestCaseGroup* createBufferMarkerTests (tcu::TestContext& testCtx)
{
	return createBufferMarkerTestsInGroup(testCtx);
}

} // api
} // vkt
