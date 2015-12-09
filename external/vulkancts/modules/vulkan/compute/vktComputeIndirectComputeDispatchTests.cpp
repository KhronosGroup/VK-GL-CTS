/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Mobica Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by Khronos,
 * at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Indirect Compute Dispatch tests
 *//*--------------------------------------------------------------------*/

#include "vktComputeIndirectComputeDispatchTests.hpp"

#include <string>
#include <map>
#include <vector>

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkMemUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkQueryUtil.hpp"

#include "tcuVector.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuRGBA.hpp"
#include "tcuStringTemplate.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deStringUtil.hpp"
#include "deArrayUtil.hpp"

#include "gluShaderUtil.hpp"

namespace vkt
{
namespace compute
{

static const deUint32 s_result_block_base_size			= 4 * sizeof(deUint32); // uvec3 + uint
static const deUint32 s_result_block_num_passed_offset	= 3 * sizeof(deUint32);
static const deUint32 s_indirect_command_size			= 3 * sizeof(deUint32);

struct DispatchCommand
{
				DispatchCommand (const deIntptr		offset,
								 const tcu::UVec3&	numWorkGroups)
					: m_offset			(offset)
					, m_numWorkGroups	(numWorkGroups) {}

	deIntptr	m_offset;
	tcu::UVec3	m_numWorkGroups;
};

typedef std::vector<DispatchCommand> DispatchCommandsVec;

struct DispatchCaseDesc
{
								DispatchCaseDesc (const char*					name,
												  const char*					description,
												  const deUintptr				bufferSize,
												  const tcu::UVec3				workGroupSize,
												  const DispatchCommandsVec&	dispatchCommands)
									: m_name				(name)
									, m_description			(description)
									, m_bufferSize			(bufferSize)
									, m_workGroupSize		(workGroupSize)
									, m_dispatchCommands	(dispatchCommands) {}

	const char*					m_name;
	const char*					m_description;
	const deUintptr				m_bufferSize;
	const tcu::UVec3			m_workGroupSize;
	const DispatchCommandsVec	m_dispatchCommands;
};

class ShaderObject
{
public:
							ShaderObject	(const vk::DeviceInterface&	device_interface,
											 const vk::VkDevice			device,
											 const vk::ProgramBinary&	programBinary,
											 const vk::VkShaderStage	shaderStage);

	vk::VkShader			getVKShader		(void) const { return *m_shader; }

private:
	vk::Move<vk::VkShader>	m_shader;
};

ShaderObject::ShaderObject (const vk::DeviceInterface&	device_interface,
							const vk::VkDevice			device,
							const vk::ProgramBinary&	programBinary,
							const vk::VkShaderStage		shaderStage)
{
	const vk::Unique<vk::VkShaderModule> shaderModule = vk::createShaderModule(device_interface, device, programBinary, (vk::VkShaderModuleCreateFlags)0u);

	const vk::VkShaderCreateInfo shaderCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO,
		DE_NULL,
		*shaderModule,		// module
		"main",				// pName
		0u,					// flags
		shaderStage
	};

	m_shader = vk::createShader(device_interface, device, &shaderCreateInfo);
}

class BufferObject
{
public:
								BufferObject	(const vk::DeviceInterface&		vki,
												 const vk::VkDevice				device,
												 vk::Allocator&					allocator);

	void						allocMemory		(const vk::VkDeviceSize			bufferSize,
												 const vk::VkBufferUsageFlags	usage,
												 const vk::MemoryRequirement	memRequirement);

	vk::VkBuffer				getVKBuffer		(void) const { return *m_buffer; }
	deUint8*					mapBuffer		(void) const;
	void						unmapBuffer		(void) const;

private:
	const vk::DeviceInterface&	m_device_interface;
	const vk::VkDevice			m_device;
	vk::Allocator&				m_allocator;
	de::MovePtr<vk::Allocation> m_allocation;
	vk::VkDeviceSize			m_bufferSize;
	vk::Move<vk::VkBuffer>		m_buffer;
};

BufferObject::BufferObject (const vk::DeviceInterface&	device_interface,
							const vk::VkDevice			device,
							vk::Allocator&				allocator)
	: m_device_interface(device_interface)
	, m_device			(device)
	, m_allocator		(allocator)
	, m_bufferSize		(0)
{
}

void BufferObject::allocMemory (const vk::VkDeviceSize			bufferSize,
								const vk::VkBufferUsageFlags	usage,
								const vk::MemoryRequirement		memRequirement)
{
	m_bufferSize = bufferSize;

	const vk::VkBufferCreateInfo bufferCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		DE_NULL,
		bufferSize,									// size
		usage,										// usage
		0u,											// flags
		vk::VK_SHARING_MODE_EXCLUSIVE,				// sharingMode
		0u,											// queueFamilyCount
		DE_NULL,									// pQueueFamilyIndices
	};

	m_buffer = vk::createBuffer(m_device_interface, m_device, &bufferCreateInfo);

	const vk::VkMemoryRequirements requirements = vk::getBufferMemoryRequirements(m_device_interface, m_device, *m_buffer);

	m_allocation = m_allocator.allocate(requirements, memRequirement);

	VK_CHECK(m_device_interface.bindBufferMemory(m_device, *m_buffer, m_allocation->getMemory(), m_allocation->getOffset()));
}

deUint8* BufferObject::mapBuffer (void) const
{
	invalidateMappedMemoryRange(m_device_interface, m_device, m_allocation->getMemory(), m_allocation->getOffset(), m_bufferSize);

	return (deUint8*)m_allocation->getHostPtr();
}

void BufferObject::unmapBuffer (void) const
{
	flushMappedMemoryRange(m_device_interface, m_device, m_allocation->getMemory(), m_allocation->getOffset(), m_bufferSize);
}

class ComputePipeline
{
public:
									ComputePipeline		(const vk::DeviceInterface&			device_interface,
														 const vk::VkDevice					device)
										: m_device_interface(device_interface)
										, m_device			(device) {}

	void							createPipeline		(const ShaderObject&				shader,
														 const deUint32						numDescriptorSets,
														 const vk::VkDescriptorSetLayout	descriptorSetLayouts);

	vk::VkPipelineLayout			getVKPipelineLayout	(void) const { return *m_pipelineLayout; };
	vk::VkPipeline					getVKPipeline		(void) const { return *m_pipeline; };

private:
	const vk::DeviceInterface&		m_device_interface;
	const vk::VkDevice				m_device;
	vk::Move<vk::VkPipelineLayout>	m_pipelineLayout;
	vk::Move<vk::VkPipeline>		m_pipeline;
};

void ComputePipeline::createPipeline (const ShaderObject&				shader,
									  const deUint32					numDescriptorSets,
									  const vk::VkDescriptorSetLayout	descriptorSetLayouts)
{
	const vk::VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		DE_NULL,
		numDescriptorSets,		// descriptorSetCount
		&descriptorSetLayouts,	// pSetLayouts
		0u,						// pushConstantRangeCount
		DE_NULL,				// pPushConstantRanges
	};

	m_pipelineLayout = vk::createPipelineLayout(m_device_interface, m_device, &pipelineLayoutCreateInfo);

	const vk::VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		DE_NULL,
		vk::VK_SHADER_STAGE_COMPUTE,			// stage
		shader.getVKShader(),					// shader
		DE_NULL,								// pSpecializationInfo
	};

	const vk::VkComputePipelineCreateInfo pipelineCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		DE_NULL,
		pipelineShaderStageCreateInfo,	// cs
		0u,								// flags
		*m_pipelineLayout,				// layout
		(vk::VkPipeline)0,				// basePipelineHandle
		0u,								// basePipelineIndex
	};

	m_pipeline = vk::createComputePipeline(m_device_interface, m_device, (vk::VkPipelineCache)0u, &pipelineCreateInfo);
}

class CommandBuffer
{
public:
								CommandBuffer			(const vk::DeviceInterface&	device_interface,
														 const vk::VkDevice			device,
														 const deUint32				queueFamilyIndex);

	vk::VkCmdBuffer				getVKCmdBuffer			(void) const { return *m_cmdBuffer; };

	void						beginRecordingCommands	(void) const;
	void						endRecordingCommands	(void) const;

private:
	const vk::DeviceInterface&	m_device_interface;
	const vk::VkDevice			m_device;
	vk::Move<vk::VkCmdPool>		m_cmdPool;
	vk::Move<vk::VkCmdBuffer>	m_cmdBuffer;
};

CommandBuffer::CommandBuffer (const vk::DeviceInterface&	device_interface,
							  const vk::VkDevice			device,
							  const deUint32				queueFamilyIndex)
	: m_device_interface(device_interface)
	, m_device			(device)
{
	const vk::VkCmdPoolCreateInfo cmdPoolCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,
		DE_NULL,
		queueFamilyIndex,						// queueFamilyIndex
		vk::VK_CMD_POOL_CREATE_TRANSIENT_BIT,	// flags
	};

	m_cmdPool = vk::createCommandPool(device_interface, device, &cmdPoolCreateInfo);

	const vk::VkCmdBufferCreateInfo	cmdBufCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,
		DE_NULL,
		*m_cmdPool,							// cmdPool
		vk::VK_CMD_BUFFER_LEVEL_PRIMARY,	// level
		0u,									// flags
	};

	m_cmdBuffer = vk::createCommandBuffer(device_interface, device, &cmdBufCreateInfo);
}

void CommandBuffer::beginRecordingCommands (void) const
{
	const vk::VkCmdBufferBeginInfo cmdBufBeginInfo =
	{
		vk::VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
		DE_NULL,
		vk::VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | vk::VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT,	// flags
		(vk::VkRenderPass)0u,																			// renderPass
		0u,																								// subpass
		(vk::VkFramebuffer)0u,																			// framebuffer
	};

	VK_CHECK(m_device_interface.beginCommandBuffer(*m_cmdBuffer, &cmdBufBeginInfo));
}

void CommandBuffer::endRecordingCommands (void) const
{
	VK_CHECK(m_device_interface.endCommandBuffer(*m_cmdBuffer));
}

class Fence
{
public:
	Fence(	const vk::DeviceInterface&	device_interface,
			const vk::VkDevice			device );

	vk::VkFence getVKFence(void) const { return *m_fence; }

private:

	vk::Move<vk::VkFence> m_fence;
};

Fence::Fence (const vk::DeviceInterface&	device_interface,
			  const vk::VkDevice			device)
{
	const vk::VkFenceCreateInfo fenceCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		DE_NULL,
		0u,			// flags
	};

	m_fence = vk::createFence(device_interface, device, &fenceCreateInfo);
}

class IndirectDispatchInstanceBufferUpload : public vkt::TestInstance
{
public:
									IndirectDispatchInstanceBufferUpload	(Context&					context,
																			 const std::string&			name,
																			 const deUintptr			bufferSize,
																			 const tcu::UVec3&			workGroupSize,
																			 const DispatchCommandsVec& dispatchCommands);

	virtual							~IndirectDispatchInstanceBufferUpload	(void) {}

	virtual tcu::TestStatus			iterate									(void);

protected:
	deUint32						getResultBlockAlignedSize				(const deUint32 baseSize) const;

	virtual void					fillIndirectBufferData					(CommandBuffer&			commandBuffer,
																			 BufferObject&			indirectBuffer);

	deBool							verifyResultBuffer						(const BufferObject&	resultBuffer,
																			 const deUint32			resultBlockSize,
																			 const deUint32			resultBufferSize) const;

	Context&						m_context;
	const std::string				m_name;

	const vk::DeviceInterface&		m_device_interface;
	const vk::VkDevice				m_device;
	const vk::VkPhysicalDevice		m_physical_device;

	const vk::InstanceInterface&	m_instance_interface;

	const vk::VkQueue				m_queue;
	const deUint32					m_queueFamilyIndex;

	const deUintptr					m_bufferSize;
	const tcu::UVec3				m_workGroupSize;
	const DispatchCommandsVec		m_dispatchCommands;

	vk::Allocator&					m_allocator;

private:
	IndirectDispatchInstanceBufferUpload (const vkt::TestInstance&);
	IndirectDispatchInstanceBufferUpload& operator= (const vkt::TestInstance&);
};

IndirectDispatchInstanceBufferUpload::IndirectDispatchInstanceBufferUpload (Context&					context,
																			const std::string&			name,
																			const deUintptr				bufferSize,
																			const tcu::UVec3&			workGroupSize,
																			const DispatchCommandsVec&	dispatchCommands)
	: vkt::TestInstance		(context)
	, m_context				(context)
	, m_name				(name)
	, m_device_interface	(context.getDeviceInterface())
	, m_device				(context.getDevice())
	, m_physical_device		(context.getPhysicalDevice())
	, m_instance_interface	(context.getInstanceInterface())
	, m_queue				(context.getUniversalQueue())
	, m_queueFamilyIndex	(context.getUniversalQueueFamilyIndex())
	, m_allocator			(context.getDefaultAllocator())
	, m_bufferSize			(bufferSize)
	, m_workGroupSize		(workGroupSize)
	, m_dispatchCommands	(dispatchCommands)
{
}

deUint32 IndirectDispatchInstanceBufferUpload::getResultBlockAlignedSize (const deUint32 baseSize) const
{
	vk::VkPhysicalDeviceProperties deviceProperties;
	m_instance_interface.getPhysicalDeviceProperties(m_physical_device, &deviceProperties);
	deUint32 alignment = deviceProperties.limits.minStorageBufferOffsetAlignment;

	if (alignment == 0 || (baseSize % alignment == 0))
		return baseSize;
	else
		return (baseSize / alignment + 1)*alignment;
}

void IndirectDispatchInstanceBufferUpload::fillIndirectBufferData (CommandBuffer& commandBuffer, BufferObject& indirectBuffer)
{
	indirectBuffer.allocMemory((vk::VkDeviceSize)m_bufferSize, vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, vk::MemoryRequirement::HostVisible);

	deUint8* indirectDataPtr = indirectBuffer.mapBuffer();

	for (DispatchCommandsVec::const_iterator cmdIter = m_dispatchCommands.begin(); cmdIter != m_dispatchCommands.end(); ++cmdIter)
	{
		DE_ASSERT(cmdIter->m_offset >= 0);
		DE_ASSERT(cmdIter->m_offset % sizeof(deUint32) == 0);
		DE_ASSERT(cmdIter->m_offset + s_indirect_command_size <= (deIntptr)m_bufferSize);

		deUint32* const dstPtr = (deUint32*)&indirectDataPtr[cmdIter->m_offset];

		dstPtr[0] = cmdIter->m_numWorkGroups[0];
		dstPtr[1] = cmdIter->m_numWorkGroups[1];
		dstPtr[2] = cmdIter->m_numWorkGroups[2];
	}

	indirectBuffer.unmapBuffer();
}

tcu::TestStatus IndirectDispatchInstanceBufferUpload::iterate (void)
{
	tcu::TestContext& testCtx = m_context.getTestContext();

	testCtx.getLog() << tcu::TestLog::Message << "GL_DISPATCH_INDIRECT_BUFFER size = " << m_bufferSize << tcu::TestLog::EndMessage;
	{
		tcu::ScopedLogSection section(testCtx.getLog(), "Commands", "Indirect Dispatch Commands (" + de::toString(m_dispatchCommands.size()) + " in total)");

		for (deUint32 cmdNdx = 0; cmdNdx < m_dispatchCommands.size(); ++cmdNdx)
		{
			testCtx.getLog()
				<< tcu::TestLog::Message
				<< cmdNdx << ": " << "offset = " << m_dispatchCommands[cmdNdx].m_offset << ", numWorkGroups = " << m_dispatchCommands[cmdNdx].m_numWorkGroups
				<< tcu::TestLog::EndMessage;
		}
	}

	// Create result buffer
	const deUint32 resultBlockSize = getResultBlockAlignedSize(s_result_block_base_size);
	const deUint32 resultBufferSize = resultBlockSize * (deUint32)m_dispatchCommands.size();

	BufferObject resultBuffer(m_device_interface, m_device, m_allocator);
	resultBuffer.allocMemory((vk::VkDeviceSize)resultBufferSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vk::MemoryRequirement::HostVisible);

	deUint8* resultDataPtr = resultBuffer.mapBuffer();
	for (deUint32 cmdNdx = 0; cmdNdx < m_dispatchCommands.size(); ++cmdNdx)
	{
		deUint8* const	dstPtr = &resultDataPtr[resultBlockSize*cmdNdx];

		*(deUint32*)(dstPtr + 0 * sizeof(deUint32)) = m_dispatchCommands[cmdNdx].m_numWorkGroups[0];
		*(deUint32*)(dstPtr + 1 * sizeof(deUint32)) = m_dispatchCommands[cmdNdx].m_numWorkGroups[1];
		*(deUint32*)(dstPtr + 2 * sizeof(deUint32)) = m_dispatchCommands[cmdNdx].m_numWorkGroups[2];
		*(deUint32*)(dstPtr + s_result_block_num_passed_offset) = 0;
	}
	resultBuffer.unmapBuffer();

	// Create verify compute shader
	ShaderObject verifyShader(m_device_interface, m_device, m_context.getBinaryCollection().get("indirect_dispatch_" + m_name + "_verify"), vk::VK_SHADER_STAGE_COMPUTE);

	// Create descriptorSetLayout
	vk::DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);
	vk::Unique<vk::VkDescriptorSetLayout>	descriptorSetLayout = layoutBuilder.build(m_device_interface, m_device);

	// Create compute pipeline
	ComputePipeline computePipeline(m_device_interface, m_device);
	computePipeline.createPipeline(verifyShader, 1, *descriptorSetLayout);

	// Create descriptor pool
	vk::Move<vk::VkDescriptorPool> descriptorPool = vk::DescriptorPoolBuilder()
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
		.build(m_device_interface, m_device, vk::VK_DESCRIPTOR_POOL_USAGE_DYNAMIC, (deUint32)m_dispatchCommands.size());

	const vk::VkBufferMemoryBarrier ssboPostBarrier =
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		DE_NULL,
		vk::VK_MEMORY_OUTPUT_SHADER_WRITE_BIT,		// outputMask
		vk::VK_MEMORY_INPUT_HOST_READ_BIT,			// inputMask
		vk::VK_QUEUE_FAMILY_IGNORED,				// srcQueueFamilyIndex
		vk::VK_QUEUE_FAMILY_IGNORED,				// destQueueFamilyIndex
		resultBuffer.getVKBuffer(),					// buffer
		(vk::VkDeviceSize)0u,						// offset
		(vk::VkDeviceSize)resultBufferSize,			// size
	};
	const void* const postBarriers[] = { &ssboPostBarrier };

	// Create command buffer
	CommandBuffer cmdBuffer(m_device_interface, m_device, m_queueFamilyIndex);

	// Begin recording commands
	cmdBuffer.beginRecordingCommands();

	// Create indirect buffer
	BufferObject indirectBuffer(m_device_interface, m_device, m_allocator);
	fillIndirectBufferData(cmdBuffer, indirectBuffer);

	// Bind compute pipeline
	m_device_interface.cmdBindPipeline(cmdBuffer.getVKCmdBuffer(), vk::VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.getVKPipeline());

	// Allocate descriptor sets
	std::vector< vk::Move<vk::VkDescriptorSet> > descriptorSets(m_dispatchCommands.size());

	vk::VkDeviceSize curOffset = 0;

	// Create descriptor sets
	for (deUint32 cmdNdx = 0; cmdNdx < m_dispatchCommands.size(); ++cmdNdx)
	{
		descriptorSets[cmdNdx] = allocDescriptorSet(m_device_interface, m_device, *descriptorPool, vk::VK_DESCRIPTOR_SET_USAGE_ONE_SHOT, *descriptorSetLayout);

		const vk::VkDescriptorInfo resultDescriptorInfo =
		{
			0,															// bufferView
			0,															// sampler
			0,															// imageView
			(vk::VkImageLayout)0,										// imageLayout
			{ resultBuffer.getVKBuffer(), curOffset, resultBlockSize }	// bufferInfo
		};

		vk::DescriptorSetUpdateBuilder	descriptorSetBuilder;
		descriptorSetBuilder.writeSingle(*descriptorSets[cmdNdx], vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultDescriptorInfo);
		descriptorSetBuilder.update(m_device_interface, m_device);

		// Bind descriptor set
		m_device_interface.cmdBindDescriptorSets(cmdBuffer.getVKCmdBuffer(), vk::VK_PIPELINE_BIND_POINT_COMPUTE,
			computePipeline.getVKPipelineLayout(), 0, 1, &descriptorSets[cmdNdx].get(), 0, DE_NULL);

		// Dispatch indirect compute command
		m_device_interface.cmdDispatchIndirect(cmdBuffer.getVKCmdBuffer(), indirectBuffer.getVKBuffer(), (vk::VkDeviceSize)m_dispatchCommands[cmdNdx].m_offset);

		curOffset += resultBlockSize;
	}

	// Insert memory barrier
	m_device_interface.cmdPipelineBarrier(cmdBuffer.getVKCmdBuffer(), vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_FALSE, DE_LENGTH_OF_ARRAY(postBarriers), postBarriers);

	// End recording commands
	cmdBuffer.endRecordingCommands();

	// Create fence object that will allow to wait for command buffer's execution completion
	Fence cmdBufferFence(m_device_interface, m_device);

	// Submit command buffer to queue
	vk::VkCmdBuffer vkCmdBuffer = cmdBuffer.getVKCmdBuffer();
	VK_CHECK(m_device_interface.queueSubmit(m_queue, 1, &vkCmdBuffer, cmdBufferFence.getVKFence()));

	// Wait for command buffer execution finish
	const deUint64 infiniteTimeout = ~(deUint64)0u;
	VK_CHECK(m_device_interface.waitForFences(m_device, 1, &cmdBufferFence.getVKFence(), 0u, infiniteTimeout)); // \note: timeout is failure

	// Check if result buffer contains valid values
	if (verifyResultBuffer(resultBuffer, resultBlockSize, resultBufferSize))
		return tcu::TestStatus(QP_TEST_RESULT_PASS, "Pass");
	else
		return tcu::TestStatus(QP_TEST_RESULT_FAIL, "Invalid values in result buffer");
}

deBool IndirectDispatchInstanceBufferUpload::verifyResultBuffer (const BufferObject&	resultBuffer,
																 const deUint32			resultBlockSize,
																 const deUint32			resultBufferSize) const
{
	deBool allOk = true;
	deUint8* resultDataPtr = resultBuffer.mapBuffer();

	for (deUint32 cmdNdx = 0; cmdNdx < m_dispatchCommands.size(); cmdNdx++)
	{
		const DispatchCommand&	cmd = m_dispatchCommands[cmdNdx];
		const deUint8* const	srcPtr = (const deUint8*)resultDataPtr + cmdNdx*resultBlockSize;
		const deUint32			numPassed = *(const deUint32*)(srcPtr + s_result_block_num_passed_offset);
		const deUint32			numInvocationsPerGroup = m_workGroupSize[0] * m_workGroupSize[1] * m_workGroupSize[2];
		const deUint32			numGroups = cmd.m_numWorkGroups[0] * cmd.m_numWorkGroups[1] * cmd.m_numWorkGroups[2];
		const deUint32			expectedCount = numInvocationsPerGroup * numGroups;

		if (numPassed != expectedCount)
		{
			tcu::TestContext& testCtx = m_context.getTestContext();

			testCtx.getLog()
				<< tcu::TestLog::Message
				<< "ERROR: got invalid result for invocation " << cmdNdx
				<< ": got numPassed = " << numPassed << ", expected " << expectedCount
				<< tcu::TestLog::EndMessage;

			allOk = false;
		}
	}

	return allOk;
}

class IndirectDispatchCaseBufferUpload : public vkt::TestCase
{
public:
								IndirectDispatchCaseBufferUpload	(tcu::TestContext&			testCtx,
																	 const DispatchCaseDesc&	caseDesc,
																	 const glu::GLSLVersion		glslVersion);

	virtual						~IndirectDispatchCaseBufferUpload	(void) {}

	virtual void				initPrograms						(vk::SourceCollections& programCollection) const;
	virtual TestInstance*		createInstance						(Context& context) const;

protected:
	const deUintptr				m_bufferSize;
	const tcu::UVec3			m_workGroupSize;
	const DispatchCommandsVec	m_dispatchCommands;
	const glu::GLSLVersion		m_glslVersion;

private:
	IndirectDispatchCaseBufferUpload (const vkt::TestCase&);
	IndirectDispatchCaseBufferUpload& operator= (const vkt::TestCase&);
};

IndirectDispatchCaseBufferUpload::IndirectDispatchCaseBufferUpload (tcu::TestContext&		testCtx,
																	const DispatchCaseDesc& caseDesc,
																	const glu::GLSLVersion	glslVersion)
	: vkt::TestCase			(testCtx, caseDesc.m_name, caseDesc.m_description)
	, m_bufferSize			(caseDesc.m_bufferSize)
	, m_workGroupSize		(caseDesc.m_workGroupSize)
	, m_dispatchCommands	(caseDesc.m_dispatchCommands)
	, m_glslVersion			(glslVersion)
{
}

void IndirectDispatchCaseBufferUpload::initPrograms (vk::SourceCollections& programCollection) const
{
	const char* const	versionDecl = glu::getGLSLVersionDeclaration(m_glslVersion);

	std::ostringstream	verifyBuffer;

	verifyBuffer
		<< versionDecl << "\n"
		<< "layout(local_size_x = ${LOCAL_SIZE_X}, local_size_y = ${LOCAL_SIZE_Y}, local_size_z = ${LOCAL_SIZE_Z}) in;\n"
		<< "layout(set = 0, binding = 0, std430) buffer Result\n"
		<< "{\n"
		<< "    uvec3           expectedGroupCount;\n"
		<< "    coherent uint   numPassed;\n"
		<< "} result;\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "    if (all(equal(result.expectedGroupCount, gl_NumWorkGroups)))\n"
		<< "        atomicAdd(result.numPassed, 1u);\n"
		<< "}\n";

	std::map<std::string, std::string> args;

	args["LOCAL_SIZE_X"] = de::toString(m_workGroupSize.x());
	args["LOCAL_SIZE_Y"] = de::toString(m_workGroupSize.y());
	args["LOCAL_SIZE_Z"] = de::toString(m_workGroupSize.z());

	std::string verifyProgramString = tcu::StringTemplate(verifyBuffer.str()).specialize(args);

	programCollection.glslSources.add("indirect_dispatch_" + m_name + "_verify") << glu::ComputeSource(verifyProgramString);
}

TestInstance* IndirectDispatchCaseBufferUpload::createInstance (Context& context) const
{
	return new IndirectDispatchInstanceBufferUpload(context, m_name, m_bufferSize, m_workGroupSize, m_dispatchCommands);
}

class IndirectDispatchInstanceBufferGenerate : public IndirectDispatchInstanceBufferUpload
{
public:
									IndirectDispatchInstanceBufferGenerate	(Context&					context,
																			 const std::string&			name,
																			 const deUintptr			bufferSize,
																			 const tcu::UVec3&			workGroupSize,
																			 const DispatchCommandsVec&	dispatchCommands)
										: IndirectDispatchInstanceBufferUpload	(context, name, bufferSize, workGroupSize, dispatchCommands)
										, m_computePipeline						(context.getDeviceInterface(), context.getDevice()) {}

	virtual							~IndirectDispatchInstanceBufferGenerate	(void) {}

protected:
	virtual void					fillIndirectBufferData					(CommandBuffer& commandBuffer, BufferObject& indirectBuffer);

	vk::Move<vk::VkDescriptorPool>	m_descriptorPool;
	vk::Move<vk::VkDescriptorSet>	m_descriptorSet;
	ComputePipeline					m_computePipeline;
	vk::VkBufferMemoryBarrier		m_BufferBarrier;

private:
	IndirectDispatchInstanceBufferGenerate (const vkt::TestInstance&);
	IndirectDispatchInstanceBufferGenerate& operator= (const vkt::TestInstance&);
};

void IndirectDispatchInstanceBufferGenerate::fillIndirectBufferData (CommandBuffer& commandBuffer, BufferObject& indirectBuffer)
{
	indirectBuffer.allocMemory((vk::VkDeviceSize)m_bufferSize, vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vk::MemoryRequirement::Any);

	// Create compute shader that generates data for indirect buffer
	ShaderObject genIndirectBufferDataShader(m_device_interface, m_device, m_context.getBinaryCollection().get("indirect_dispatch_" + m_name + "_generate"), vk::VK_SHADER_STAGE_COMPUTE);

	// Create descriptorSetLayout
	vk::DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);
	vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout = layoutBuilder.build(m_device_interface, m_device);

	// Create compute pipeline
	m_computePipeline.createPipeline(genIndirectBufferDataShader, 1, *descriptorSetLayout);

	// Create descriptor pool
	m_descriptorPool = vk::DescriptorPoolBuilder()
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
		.build(m_device_interface, m_device, vk::VK_DESCRIPTOR_POOL_USAGE_DYNAMIC, 1u);

	// Create descriptor set
	m_descriptorSet = allocDescriptorSet(m_device_interface, m_device, *m_descriptorPool, vk::VK_DESCRIPTOR_SET_USAGE_ONE_SHOT, *descriptorSetLayout);

	const vk::VkDescriptorInfo indirectDescriptorInfo =
	{
		0,													// bufferView
		0,													// sampler
		0,													// imageView
		(vk::VkImageLayout)0,								// imageLayout
		{ indirectBuffer.getVKBuffer(), 0u, m_bufferSize }	// bufferInfo
	};

	vk::DescriptorSetUpdateBuilder	descriptorSetBuilder;
	descriptorSetBuilder.writeSingle(*m_descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &indirectDescriptorInfo);
	descriptorSetBuilder.update(m_device_interface, m_device);

	m_BufferBarrier = vk::VkBufferMemoryBarrier(
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		DE_NULL,
		vk::VK_MEMORY_OUTPUT_SHADER_WRITE_BIT,		// outputMask
		vk::VK_MEMORY_INPUT_INDIRECT_COMMAND_BIT,	// inputMask
		vk::VK_QUEUE_FAMILY_IGNORED,				// srcQueueFamilyIndex
		vk::VK_QUEUE_FAMILY_IGNORED,				// destQueueFamilyIndex
		indirectBuffer.getVKBuffer(),				// buffer
		(vk::VkDeviceSize)0u,						// offset
		(vk::VkDeviceSize)m_bufferSize,				// size
	});
	const void* const postBarriers[] = { &m_BufferBarrier };

	// Bind compute pipeline
	m_device_interface.cmdBindPipeline(commandBuffer.getVKCmdBuffer(), vk::VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline.getVKPipeline());

	// Bind descriptor set
	m_device_interface.cmdBindDescriptorSets(commandBuffer.getVKCmdBuffer(), vk::VK_PIPELINE_BIND_POINT_COMPUTE,
		m_computePipeline.getVKPipelineLayout(), 0, 1, &m_descriptorSet.get(), 0, DE_NULL);

	// Dispatch compute command
	m_device_interface.cmdDispatch(commandBuffer.getVKCmdBuffer(), 1, 1, 1);

	// Insert memory barrier
	m_device_interface.cmdPipelineBarrier(commandBuffer.getVKCmdBuffer(), vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, vk::VK_FALSE, DE_LENGTH_OF_ARRAY(postBarriers), postBarriers);
}

class IndirectDispatchCaseBufferGenerate : public IndirectDispatchCaseBufferUpload
{
public:
							IndirectDispatchCaseBufferGenerate	(tcu::TestContext&		testCtx,
																 const DispatchCaseDesc&	caseDesc,
																 const glu::GLSLVersion	glslVersion)
								: IndirectDispatchCaseBufferUpload(testCtx, caseDesc, glslVersion) {}

	virtual					~IndirectDispatchCaseBufferGenerate	(void) {}

	virtual void			initPrograms						(vk::SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance						(Context& context) const;

private:
	IndirectDispatchCaseBufferGenerate (const vkt::TestCase&);
	IndirectDispatchCaseBufferGenerate& operator= (const vkt::TestCase&);
};

void IndirectDispatchCaseBufferGenerate::initPrograms (vk::SourceCollections& programCollection) const
{
	IndirectDispatchCaseBufferUpload::initPrograms(programCollection);

	const char* const	versionDecl = glu::getGLSLVersionDeclaration(m_glslVersion);

	std::ostringstream computeBuffer;

	// Header
	computeBuffer
		<< versionDecl << "\n"
		<< "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
		<< "layout(set = 0, binding = 0, std430) buffer Out\n"
		<< "{\n"
		<< "	highp uint data[];\n"
		<< "};\n"
		<< "void writeCmd (uint offset, uvec3 numWorkGroups)\n"
		<< "{\n"
		<< "	data[offset+0u] = numWorkGroups.x;\n"
		<< "	data[offset+1u] = numWorkGroups.y;\n"
		<< "	data[offset+2u] = numWorkGroups.z;\n"
		<< "}\n"
		<< "void main (void)\n"
		<< "{\n";

	// Dispatch commands
	for (DispatchCommandsVec::const_iterator cmdIter = m_dispatchCommands.begin(); cmdIter != m_dispatchCommands.end(); ++cmdIter)
	{
		const deUint32 offs = (deUint32)(cmdIter->m_offset / sizeof(deUint32));
		DE_ASSERT((deIntptr)offs * sizeof(deUint32) == cmdIter->m_offset);

		computeBuffer
			<< "\twriteCmd(" << offs << "u, uvec3("
			<< cmdIter->m_numWorkGroups.x() << "u, "
			<< cmdIter->m_numWorkGroups.y() << "u, "
			<< cmdIter->m_numWorkGroups.z() << "u));\n";
	}

	// Ending
	computeBuffer << "}\n";

	std::string computeString = computeBuffer.str();

	programCollection.glslSources.add("indirect_dispatch_" + m_name + "_generate") << glu::ComputeSource(computeString);
}

TestInstance* IndirectDispatchCaseBufferGenerate::createInstance (Context& context) const
{
	return new IndirectDispatchInstanceBufferGenerate(context, m_name, m_bufferSize, m_workGroupSize, m_dispatchCommands);
}

tcu::TestCaseGroup* createIndirectComputeDispatchTests (tcu::TestContext& testCtx)
{
	static const DispatchCaseDesc s_dispatchCases[] =
	{
		DispatchCaseDesc("single_invocation", "Single invocation only from offset 0", s_indirect_command_size, tcu::UVec3(1, 1, 1),
		{
			DispatchCommand(0, tcu::UVec3(1, 1, 1))
		}),
		DispatchCaseDesc("multiple_groups", "Multiple groups dispatched from offset 0", s_indirect_command_size, tcu::UVec3(1, 1, 1),
		{
			DispatchCommand(0, tcu::UVec3(2, 3, 5))
		}),
		DispatchCaseDesc("multiple_groups_multiple_invocations", "Multiple groups of size 2x3x1 from offset 0", s_indirect_command_size, tcu::UVec3(2, 3, 1),
		{
			DispatchCommand(0, tcu::UVec3(1, 2, 3))
		}),
		DispatchCaseDesc("small_offset", "Small offset", 16 + s_indirect_command_size, tcu::UVec3(1, 1, 1),
		{
			DispatchCommand(16, tcu::UVec3(1, 1, 1))
		}),
		DispatchCaseDesc("large_offset", "Large offset", (2 << 20), tcu::UVec3(1, 1, 1),
		{
			DispatchCommand((1 << 20) + 12, tcu::UVec3(1, 1, 1))
		}),
		DispatchCaseDesc("large_offset_multiple_invocations", "Large offset, multiple invocations", (2 << 20), tcu::UVec3(2, 3, 1),
		{
			DispatchCommand((1 << 20) + 12, tcu::UVec3(1, 2, 3))
		}),
		DispatchCaseDesc("empty_command", "Empty command", s_indirect_command_size, tcu::UVec3(1, 1, 1),
		{
			DispatchCommand(0, tcu::UVec3(0, 0, 0))
		}),
		DispatchCaseDesc("multi_dispatch", "Dispatch multiple compute commands from single buffer", 1 << 10, tcu::UVec3(3, 1, 2),
		{
			DispatchCommand(0, tcu::UVec3(1, 1, 1)),
			DispatchCommand(s_indirect_command_size, tcu::UVec3(2, 1, 1)),
			DispatchCommand(104, tcu::UVec3(1, 3, 1)),
			DispatchCommand(40, tcu::UVec3(1, 1, 7)),
			DispatchCommand(52, tcu::UVec3(1, 1, 4))
		}),
		DispatchCaseDesc("multi_dispatch_reuse_command", "Dispatch multiple compute commands from single buffer", 1 << 10, tcu::UVec3(3, 1, 2),
		{
			DispatchCommand(0, tcu::UVec3(1, 1, 1)),
			DispatchCommand(0, tcu::UVec3(1, 1, 1)),
			DispatchCommand(0, tcu::UVec3(1, 1, 1)),
			DispatchCommand(104, tcu::UVec3(1, 3, 1)),
			DispatchCommand(104, tcu::UVec3(1, 3, 1)),
			DispatchCommand(52, tcu::UVec3(1, 1, 4)),
			DispatchCommand(52, tcu::UVec3(1, 1, 4))
		})
	};

	de::MovePtr<tcu::TestCaseGroup> indirectComputeDispatchTests(new tcu::TestCaseGroup(testCtx, "indirect_dispatch", "Indirect dispatch tests"));

	tcu::TestCaseGroup* const	groupBufferUpload = new tcu::TestCaseGroup(testCtx, "upload_buffer", "");
	indirectComputeDispatchTests->addChild(groupBufferUpload);

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_dispatchCases); ndx++)
	{
		groupBufferUpload->addChild(new IndirectDispatchCaseBufferUpload(testCtx, s_dispatchCases[ndx], glu::GLSL_VERSION_310_ES));
	}

	tcu::TestCaseGroup* const	groupBufferGenerate = new tcu::TestCaseGroup(testCtx, "gen_in_compute", "");
	indirectComputeDispatchTests->addChild(groupBufferGenerate);

	for (deUint32 ndx = 0; ndx < DE_LENGTH_OF_ARRAY(s_dispatchCases); ndx++)
	{
		groupBufferGenerate->addChild(new IndirectDispatchCaseBufferGenerate(testCtx, s_dispatchCases[ndx], glu::GLSL_VERSION_310_ES));
	}

	return indirectComputeDispatchTests.release();
}

} // compute
} // vkt
