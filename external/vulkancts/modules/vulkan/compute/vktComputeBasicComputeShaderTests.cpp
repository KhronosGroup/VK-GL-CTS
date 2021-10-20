/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 The Android Open Source Project
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
 * \brief Compute Shader Tests
 *//*--------------------------------------------------------------------*/

#include "vktComputeBasicComputeShaderTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktComputeTestsUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktAmberTestCase.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"

#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "deRandom.hpp"

#include <vector>
#include <memory>

using namespace vk;

namespace vkt
{
namespace compute
{
namespace
{

template<typename T, int size>
T multiplyComponents (const tcu::Vector<T, size>& v)
{
	T accum = 1;
	for (int i = 0; i < size; ++i)
		accum *= v[i];
	return accum;
}

template<typename T>
inline T squared (const T& a)
{
	return a * a;
}

inline VkImageCreateInfo make2DImageCreateInfo (const tcu::IVec2& imageSize, const VkImageUsageFlags usage)
{
	const VkImageCreateInfo imageParams =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,				// VkStructureType			sType;
		DE_NULL,											// const void*				pNext;
		0u,													// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,									// VkImageType				imageType;
		VK_FORMAT_R32_UINT,									// VkFormat					format;
		vk::makeExtent3D(imageSize.x(), imageSize.y(), 1),	// VkExtent3D				extent;
		1u,													// deUint32					mipLevels;
		1u,													// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,								// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,							// VkImageTiling			tiling;
		usage,												// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,							// VkSharingMode			sharingMode;
		0u,													// deUint32					queueFamilyIndexCount;
		DE_NULL,											// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout			initialLayout;
	};
	return imageParams;
}

inline VkBufferImageCopy makeBufferImageCopy(const tcu::IVec2& imageSize)
{
	return compute::makeBufferImageCopy(vk::makeExtent3D(imageSize.x(), imageSize.y(), 1), 1u);
}

enum BufferType
{
	BUFFER_TYPE_UNIFORM,
	BUFFER_TYPE_SSBO,
};

class SharedVarTest : public vkt::TestCase
{
public:
						SharedVarTest	(tcu::TestContext&		testCtx,
										 const std::string&		name,
										 const std::string&		description,
										 const tcu::IVec3&		localSize,
										 const tcu::IVec3&		workSize);

	void				initPrograms	(SourceCollections&		sourceCollections) const;
	TestInstance*		createInstance	(Context&				context) const;

private:
	const tcu::IVec3	m_localSize;
	const tcu::IVec3	m_workSize;
};

class SharedVarTestInstance : public vkt::TestInstance
{
public:
									SharedVarTestInstance	(Context&			context,
															 const tcu::IVec3&	localSize,
															 const tcu::IVec3&	workSize);

	tcu::TestStatus					iterate					(void);

private:
	const tcu::IVec3				m_localSize;
	const tcu::IVec3				m_workSize;
};

SharedVarTest::SharedVarTest (tcu::TestContext&		testCtx,
							  const std::string&	name,
							  const std::string&	description,
							  const tcu::IVec3&		localSize,
							  const tcu::IVec3&		workSize)
	: TestCase		(testCtx, name, description)
	, m_localSize	(localSize)
	, m_workSize	(workSize)
{
}

void SharedVarTest::initPrograms (SourceCollections& sourceCollections) const
{
	const int workGroupSize = multiplyComponents(m_localSize);
	const int workGroupCount = multiplyComponents(m_workSize);
	const int numValues = workGroupSize * workGroupCount;

	std::ostringstream src;
	src << "#version 310 es\n"
		<< "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y() << ", local_size_z = " << m_localSize.z() << ") in;\n"
		<< "layout(binding = 0) writeonly buffer Output {\n"
		<< "    uint values[" << numValues << "];\n"
		<< "} sb_out;\n\n"
		<< "shared uint offsets[" << workGroupSize << "];\n\n"
		<< "void main (void) {\n"
		<< "    uint localSize  = gl_WorkGroupSize.x*gl_WorkGroupSize.y*gl_WorkGroupSize.z;\n"
		<< "    uint globalNdx  = gl_NumWorkGroups.x*gl_NumWorkGroups.y*gl_WorkGroupID.z + gl_NumWorkGroups.x*gl_WorkGroupID.y + gl_WorkGroupID.x;\n"
		<< "    uint globalOffs = localSize*globalNdx;\n"
		<< "    uint localOffs  = gl_WorkGroupSize.x*gl_WorkGroupSize.y*gl_LocalInvocationID.z + gl_WorkGroupSize.x*gl_LocalInvocationID.y + gl_LocalInvocationID.x;\n"
		<< "\n"
		<< "    offsets[localSize-localOffs-1u] = globalOffs + localOffs*localOffs;\n"
		<< "    memoryBarrierShared();\n"
		<< "    barrier();\n"
		<< "    sb_out.values[globalOffs + localOffs] = offsets[localOffs];\n"
		<< "}\n";

	sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance* SharedVarTest::createInstance (Context& context) const
{
	return new SharedVarTestInstance(context, m_localSize, m_workSize);
}

SharedVarTestInstance::SharedVarTestInstance (Context& context, const tcu::IVec3& localSize, const tcu::IVec3& workSize)
	: TestInstance	(context)
	, m_localSize	(localSize)
	, m_workSize	(workSize)
{
}

tcu::TestStatus SharedVarTestInstance::iterate (void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			device				= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&				allocator			= m_context.getDefaultAllocator();

	const int workGroupSize = multiplyComponents(m_localSize);
	const int workGroupCount = multiplyComponents(m_workSize);

	// Create a buffer and host-visible memory for it

	const VkDeviceSize bufferSizeBytes = sizeof(deUint32) * workGroupSize * workGroupCount;
	const Buffer buffer(vk, device, allocator, makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);

	// Create descriptor set

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	const VkDescriptorBufferInfo descriptorInfo = makeDescriptorBufferInfo(*buffer, 0ull, bufferSizeBytes);
	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
		.update(vk, device);

	// Perform the computation

	const Unique<VkShaderModule> shaderModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));
	const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

	const VkBufferMemoryBarrier computeFinishBarrier = makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer, 0ull, bufferSizeBytes);

	const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
	const Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Start recording commands

	beginCommandBuffer(vk, *cmdBuffer);

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

	vk.cmdDispatch(*cmdBuffer, m_workSize.x(), m_workSize.y(), m_workSize.z());

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &computeFinishBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

	endCommandBuffer(vk, *cmdBuffer);

	// Wait for completion

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// Validate the results

	const Allocation& bufferAllocation = buffer.getAllocation();
	invalidateAlloc(vk, device, bufferAllocation);

	const deUint32* bufferPtr = static_cast<deUint32*>(bufferAllocation.getHostPtr());

	for (int groupNdx = 0; groupNdx < workGroupCount; ++groupNdx)
	{
		const int globalOffset = groupNdx * workGroupSize;
		for (int localOffset = 0; localOffset < workGroupSize; ++localOffset)
		{
			const deUint32 res = bufferPtr[globalOffset + localOffset];
			const deUint32 ref = globalOffset + squared(workGroupSize - localOffset - 1);

			if (res != ref)
			{
				std::ostringstream msg;
				msg << "Comparison failed for Output.values[" << (globalOffset + localOffset) << "]";
				return tcu::TestStatus::fail(msg.str());
			}
		}
	}
	return tcu::TestStatus::pass("Compute succeeded");
}

class SharedVarAtomicOpTest : public vkt::TestCase
{
public:
						SharedVarAtomicOpTest	(tcu::TestContext&	testCtx,
												 const std::string&	name,
												 const std::string&	description,
												 const tcu::IVec3&	localSize,
												 const tcu::IVec3&	workSize);

	void				initPrograms			(SourceCollections& sourceCollections) const;
	TestInstance*		createInstance			(Context&			context) const;

private:
	const tcu::IVec3	m_localSize;
	const tcu::IVec3	m_workSize;
};

class SharedVarAtomicOpTestInstance : public vkt::TestInstance
{
public:
									SharedVarAtomicOpTestInstance	(Context&			context,
																	 const tcu::IVec3&	localSize,
																	 const tcu::IVec3&	workSize);

	tcu::TestStatus					iterate							(void);

private:
	const tcu::IVec3				m_localSize;
	const tcu::IVec3				m_workSize;
};

SharedVarAtomicOpTest::SharedVarAtomicOpTest (tcu::TestContext&		testCtx,
											  const std::string&	name,
											  const std::string&	description,
											  const tcu::IVec3&		localSize,
											  const tcu::IVec3&		workSize)
	: TestCase		(testCtx, name, description)
	, m_localSize	(localSize)
	, m_workSize	(workSize)
{
}

void SharedVarAtomicOpTest::initPrograms (SourceCollections& sourceCollections) const
{
	const int workGroupSize = multiplyComponents(m_localSize);
	const int workGroupCount = multiplyComponents(m_workSize);
	const int numValues = workGroupSize * workGroupCount;

	std::ostringstream src;
	src << "#version 310 es\n"
		<< "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y() << ", local_size_z = " << m_localSize.z() << ") in;\n"
		<< "layout(binding = 0) writeonly buffer Output {\n"
		<< "    uint values[" << numValues << "];\n"
		<< "} sb_out;\n\n"
		<< "shared uint count;\n\n"
		<< "void main (void) {\n"
		<< "    uint localSize  = gl_WorkGroupSize.x*gl_WorkGroupSize.y*gl_WorkGroupSize.z;\n"
		<< "    uint globalNdx  = gl_NumWorkGroups.x*gl_NumWorkGroups.y*gl_WorkGroupID.z + gl_NumWorkGroups.x*gl_WorkGroupID.y + gl_WorkGroupID.x;\n"
		<< "    uint globalOffs = localSize*globalNdx;\n"
		<< "\n"
		<< "    count = 0u;\n"
		<< "    memoryBarrierShared();\n"
		<< "    barrier();\n"
		<< "    uint oldVal = atomicAdd(count, 1u);\n"
		<< "    sb_out.values[globalOffs+oldVal] = oldVal+1u;\n"
		<< "}\n";

	sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance* SharedVarAtomicOpTest::createInstance (Context& context) const
{
	return new SharedVarAtomicOpTestInstance(context, m_localSize, m_workSize);
}

SharedVarAtomicOpTestInstance::SharedVarAtomicOpTestInstance (Context& context, const tcu::IVec3& localSize, const tcu::IVec3& workSize)
	: TestInstance	(context)
	, m_localSize	(localSize)
	, m_workSize	(workSize)
{
}

tcu::TestStatus SharedVarAtomicOpTestInstance::iterate (void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			device				= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&				allocator			= m_context.getDefaultAllocator();

	const int workGroupSize = multiplyComponents(m_localSize);
	const int workGroupCount = multiplyComponents(m_workSize);

	// Create a buffer and host-visible memory for it

	const VkDeviceSize bufferSizeBytes = sizeof(deUint32) * workGroupSize * workGroupCount;
	const Buffer buffer(vk, device, allocator, makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);

	// Create descriptor set

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	const VkDescriptorBufferInfo descriptorInfo = makeDescriptorBufferInfo(*buffer, 0ull, bufferSizeBytes);
	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
		.update(vk, device);

	// Perform the computation

	const Unique<VkShaderModule> shaderModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));
	const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

	const VkBufferMemoryBarrier computeFinishBarrier = makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer, 0ull, bufferSizeBytes);

	const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
	const Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Start recording commands

	beginCommandBuffer(vk, *cmdBuffer);

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

	vk.cmdDispatch(*cmdBuffer, m_workSize.x(), m_workSize.y(), m_workSize.z());

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1u, &computeFinishBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

	endCommandBuffer(vk, *cmdBuffer);

	// Wait for completion

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// Validate the results

	const Allocation& bufferAllocation = buffer.getAllocation();
	invalidateAlloc(vk, device, bufferAllocation);

	const deUint32* bufferPtr = static_cast<deUint32*>(bufferAllocation.getHostPtr());

	for (int groupNdx = 0; groupNdx < workGroupCount; ++groupNdx)
	{
		const int globalOffset = groupNdx * workGroupSize;
		for (int localOffset = 0; localOffset < workGroupSize; ++localOffset)
		{
			const deUint32 res = bufferPtr[globalOffset + localOffset];
			const deUint32 ref = localOffset + 1;

			if (res != ref)
			{
				std::ostringstream msg;
				msg << "Comparison failed for Output.values[" << (globalOffset + localOffset) << "]";
				return tcu::TestStatus::fail(msg.str());
			}
		}
	}
	return tcu::TestStatus::pass("Compute succeeded");
}

class SSBOLocalBarrierTest : public vkt::TestCase
{
public:
						SSBOLocalBarrierTest	(tcu::TestContext&	testCtx,
												 const std::string& name,
												 const std::string&	description,
												 const tcu::IVec3&	localSize,
												 const tcu::IVec3&	workSize);

	void				initPrograms			(SourceCollections& sourceCollections) const;
	TestInstance*		createInstance			(Context&			context) const;

private:
	const tcu::IVec3	m_localSize;
	const tcu::IVec3	m_workSize;
};

class SSBOLocalBarrierTestInstance : public vkt::TestInstance
{
public:
									SSBOLocalBarrierTestInstance	(Context&			context,
																	 const tcu::IVec3&	localSize,
																	 const tcu::IVec3&	workSize);

	tcu::TestStatus					iterate							(void);

private:
	const tcu::IVec3				m_localSize;
	const tcu::IVec3				m_workSize;
};

SSBOLocalBarrierTest::SSBOLocalBarrierTest (tcu::TestContext&	testCtx,
											const std::string&	name,
											const std::string&	description,
											const tcu::IVec3&	localSize,
											const tcu::IVec3&	workSize)
	: TestCase		(testCtx, name, description)
	, m_localSize	(localSize)
	, m_workSize	(workSize)
{
}

void SSBOLocalBarrierTest::initPrograms (SourceCollections& sourceCollections) const
{
	const int workGroupSize = multiplyComponents(m_localSize);
	const int workGroupCount = multiplyComponents(m_workSize);
	const int numValues = workGroupSize * workGroupCount;

	std::ostringstream src;
	src << "#version 310 es\n"
		<< "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y() << ", local_size_z = " << m_localSize.z() << ") in;\n"
		<< "layout(binding = 0) coherent buffer Output {\n"
		<< "    uint values[" << numValues << "];\n"
		<< "} sb_out;\n\n"
		<< "void main (void) {\n"
		<< "    uint localSize  = gl_WorkGroupSize.x*gl_WorkGroupSize.y*gl_WorkGroupSize.z;\n"
		<< "    uint globalNdx  = gl_NumWorkGroups.x*gl_NumWorkGroups.y*gl_WorkGroupID.z + gl_NumWorkGroups.x*gl_WorkGroupID.y + gl_WorkGroupID.x;\n"
		<< "    uint globalOffs = localSize*globalNdx;\n"
		<< "    uint localOffs  = gl_WorkGroupSize.x*gl_WorkGroupSize.y*gl_LocalInvocationID.z + gl_WorkGroupSize.x*gl_LocalInvocationID.y + gl_LocalInvocationID.x;\n"
		<< "\n"
		<< "    sb_out.values[globalOffs + localOffs] = globalOffs;\n"
		<< "    memoryBarrierBuffer();\n"
		<< "    barrier();\n"
		<< "    sb_out.values[globalOffs + ((localOffs+1u)%localSize)] += localOffs;\n"		// += so we read and write
		<< "    memoryBarrierBuffer();\n"
		<< "    barrier();\n"
		<< "    sb_out.values[globalOffs + ((localOffs+2u)%localSize)] += localOffs;\n"
		<< "}\n";

	sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance* SSBOLocalBarrierTest::createInstance (Context& context) const
{
	return new SSBOLocalBarrierTestInstance(context, m_localSize, m_workSize);
}

SSBOLocalBarrierTestInstance::SSBOLocalBarrierTestInstance (Context& context, const tcu::IVec3& localSize, const tcu::IVec3& workSize)
	: TestInstance	(context)
	, m_localSize	(localSize)
	, m_workSize	(workSize)
{
}

tcu::TestStatus SSBOLocalBarrierTestInstance::iterate (void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			device				= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&				allocator			= m_context.getDefaultAllocator();

	const int workGroupSize = multiplyComponents(m_localSize);
	const int workGroupCount = multiplyComponents(m_workSize);

	// Create a buffer and host-visible memory for it

	const VkDeviceSize bufferSizeBytes = sizeof(deUint32) * workGroupSize * workGroupCount;
	const Buffer buffer(vk, device, allocator, makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);

	// Create descriptor set

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	const VkDescriptorBufferInfo descriptorInfo = makeDescriptorBufferInfo(*buffer, 0ull, bufferSizeBytes);
	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
		.update(vk, device);

	// Perform the computation

	const Unique<VkShaderModule> shaderModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));
	const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

	const VkBufferMemoryBarrier computeFinishBarrier = makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer, 0ull, bufferSizeBytes);

	const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
	const Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Start recording commands

	beginCommandBuffer(vk, *cmdBuffer);

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

	vk.cmdDispatch(*cmdBuffer, m_workSize.x(), m_workSize.y(), m_workSize.z());

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &computeFinishBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

	endCommandBuffer(vk, *cmdBuffer);

	// Wait for completion

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// Validate the results

	const Allocation& bufferAllocation = buffer.getAllocation();
	invalidateAlloc(vk, device, bufferAllocation);

	const deUint32* bufferPtr = static_cast<deUint32*>(bufferAllocation.getHostPtr());

	for (int groupNdx = 0; groupNdx < workGroupCount; ++groupNdx)
	{
		const int globalOffset = groupNdx * workGroupSize;
		for (int localOffset = 0; localOffset < workGroupSize; ++localOffset)
		{
			const deUint32	res		= bufferPtr[globalOffset + localOffset];
			const int		offs0	= localOffset - 1 < 0 ? ((localOffset + workGroupSize - 1) % workGroupSize) : ((localOffset - 1) % workGroupSize);
			const int		offs1	= localOffset - 2 < 0 ? ((localOffset + workGroupSize - 2) % workGroupSize) : ((localOffset - 2) % workGroupSize);
			const deUint32	ref		= static_cast<deUint32>(globalOffset + offs0 + offs1);

			if (res != ref)
			{
				std::ostringstream msg;
				msg << "Comparison failed for Output.values[" << (globalOffset + localOffset) << "]";
				return tcu::TestStatus::fail(msg.str());
			}
		}
	}
	return tcu::TestStatus::pass("Compute succeeded");
}

class CopyImageToSSBOTest : public vkt::TestCase
{
public:
						CopyImageToSSBOTest		(tcu::TestContext&	testCtx,
												 const std::string&	name,
												 const std::string&	description,
												 const tcu::IVec2&	localSize,
												 const tcu::IVec2&	imageSize);

	void				initPrograms			(SourceCollections& sourceCollections) const;
	TestInstance*		createInstance			(Context&			context) const;

private:
	const tcu::IVec2	m_localSize;
	const tcu::IVec2	m_imageSize;
};

class CopyImageToSSBOTestInstance : public vkt::TestInstance
{
public:
									CopyImageToSSBOTestInstance		(Context&			context,
																	 const tcu::IVec2&	localSize,
																	 const tcu::IVec2&	imageSize);

	tcu::TestStatus					iterate							(void);

private:
	const tcu::IVec2				m_localSize;
	const tcu::IVec2				m_imageSize;
};

CopyImageToSSBOTest::CopyImageToSSBOTest (tcu::TestContext&		testCtx,
										  const std::string&	name,
										  const std::string&	description,
										  const tcu::IVec2&		localSize,
										  const tcu::IVec2&		imageSize)
	: TestCase		(testCtx, name, description)
	, m_localSize	(localSize)
	, m_imageSize	(imageSize)
{
	DE_ASSERT(m_imageSize.x() % m_localSize.x() == 0);
	DE_ASSERT(m_imageSize.y() % m_localSize.y() == 0);
}

void CopyImageToSSBOTest::initPrograms (SourceCollections& sourceCollections) const
{
	std::ostringstream src;
	src << "#version 310 es\n"
		<< "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y() << ") in;\n"
		<< "layout(binding = 1, r32ui) readonly uniform highp uimage2D u_srcImg;\n"
		<< "layout(binding = 0) writeonly buffer Output {\n"
		<< "    uint values[" << (m_imageSize.x() * m_imageSize.y()) << "];\n"
		<< "} sb_out;\n\n"
		<< "void main (void) {\n"
		<< "    uint stride = gl_NumWorkGroups.x*gl_WorkGroupSize.x;\n"
		<< "    uint value  = imageLoad(u_srcImg, ivec2(gl_GlobalInvocationID.xy)).x;\n"
		<< "    sb_out.values[gl_GlobalInvocationID.y*stride + gl_GlobalInvocationID.x] = value;\n"
		<< "}\n";

	sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance* CopyImageToSSBOTest::createInstance (Context& context) const
{
	return new CopyImageToSSBOTestInstance(context, m_localSize, m_imageSize);
}

CopyImageToSSBOTestInstance::CopyImageToSSBOTestInstance (Context& context, const tcu::IVec2& localSize, const tcu::IVec2& imageSize)
	: TestInstance	(context)
	, m_localSize	(localSize)
	, m_imageSize	(imageSize)
{
}

tcu::TestStatus CopyImageToSSBOTestInstance::iterate (void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			device				= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&				allocator			= m_context.getDefaultAllocator();

	// Create an image

	const VkImageCreateInfo imageParams = make2DImageCreateInfo(m_imageSize, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	const Image image(vk, device, allocator, imageParams, MemoryRequirement::Any);

	const VkImageSubresourceRange subresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const Unique<VkImageView> imageView(makeImageView(vk, device, *image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32_UINT, subresourceRange));

	// Staging buffer (source data for image)

	const deUint32 imageArea = multiplyComponents(m_imageSize);
	const VkDeviceSize bufferSizeBytes = sizeof(deUint32) * imageArea;

	const Buffer stagingBuffer(vk, device, allocator, makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT), MemoryRequirement::HostVisible);

	// Populate the staging buffer with test data
	{
		de::Random rnd(0xab2c7);
		const Allocation& stagingBufferAllocation = stagingBuffer.getAllocation();
		deUint32* bufferPtr = static_cast<deUint32*>(stagingBufferAllocation.getHostPtr());
		for (deUint32 i = 0; i < imageArea; ++i)
			*bufferPtr++ = rnd.getUint32();

		flushAlloc(vk, device, stagingBufferAllocation);
	}

	// Create a buffer to store shader output

	const Buffer outputBuffer(vk, device, allocator, makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);

	// Create descriptor set

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	// Set the bindings

	const VkDescriptorImageInfo imageDescriptorInfo = makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);
	const VkDescriptorBufferInfo bufferDescriptorInfo = makeDescriptorBufferInfo(*outputBuffer, 0ull, bufferSizeBytes);

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageDescriptorInfo)
		.update(vk, device);

	// Perform the computation
	{
		const Unique<VkShaderModule> shaderModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));
		const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
		const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

		const VkBufferMemoryBarrier computeFinishBarrier = makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *outputBuffer, 0ull, bufferSizeBytes);
		const tcu::IVec2 workSize = m_imageSize / m_localSize;

		// Prepare the command buffer

		const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
		const Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

		// Start recording commands

		beginCommandBuffer(vk, *cmdBuffer);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

		const std::vector<VkBufferImageCopy> bufferImageCopy(1, makeBufferImageCopy(m_imageSize));
		copyBufferToImage(vk, *cmdBuffer, *stagingBuffer, bufferSizeBytes, bufferImageCopy, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, *image, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		vk.cmdDispatch(*cmdBuffer, workSize.x(), workSize.y(), 1u);
		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &computeFinishBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

		endCommandBuffer(vk, *cmdBuffer);

		// Wait for completion

		submitCommandsAndWait(vk, device, queue, *cmdBuffer);
	}

	// Validate the results

	const Allocation& outputBufferAllocation = outputBuffer.getAllocation();
	invalidateAlloc(vk, device, outputBufferAllocation);

	const deUint32* bufferPtr = static_cast<deUint32*>(outputBufferAllocation.getHostPtr());
	const deUint32* refBufferPtr = static_cast<deUint32*>(stagingBuffer.getAllocation().getHostPtr());

	for (deUint32 ndx = 0; ndx < imageArea; ++ndx)
	{
		const deUint32 res = *(bufferPtr + ndx);
		const deUint32 ref = *(refBufferPtr + ndx);

		if (res != ref)
		{
			std::ostringstream msg;
			msg << "Comparison failed for Output.values[" << ndx << "]";
			return tcu::TestStatus::fail(msg.str());
		}
	}
	return tcu::TestStatus::pass("Compute succeeded");
}

class CopySSBOToImageTest : public vkt::TestCase
{
public:
						CopySSBOToImageTest	(tcu::TestContext&	testCtx,
											 const std::string&	name,
											 const std::string&	description,
											 const tcu::IVec2&	localSize,
											 const tcu::IVec2&	imageSize);

	void				initPrograms		(SourceCollections& sourceCollections) const;
	TestInstance*		createInstance		(Context&			context) const;

private:
	const tcu::IVec2	m_localSize;
	const tcu::IVec2	m_imageSize;
};

class CopySSBOToImageTestInstance : public vkt::TestInstance
{
public:
									CopySSBOToImageTestInstance	(Context&			context,
																 const tcu::IVec2&	localSize,
																 const tcu::IVec2&	imageSize);

	tcu::TestStatus					iterate						(void);

private:
	const tcu::IVec2				m_localSize;
	const tcu::IVec2				m_imageSize;
};

CopySSBOToImageTest::CopySSBOToImageTest (tcu::TestContext&		testCtx,
										  const std::string&	name,
										  const std::string&	description,
										  const tcu::IVec2&		localSize,
										  const tcu::IVec2&		imageSize)
	: TestCase		(testCtx, name, description)
	, m_localSize	(localSize)
	, m_imageSize	(imageSize)
{
	DE_ASSERT(m_imageSize.x() % m_localSize.x() == 0);
	DE_ASSERT(m_imageSize.y() % m_localSize.y() == 0);
}

void CopySSBOToImageTest::initPrograms (SourceCollections& sourceCollections) const
{
	std::ostringstream src;
	src << "#version 310 es\n"
		<< "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y() << ") in;\n"
		<< "layout(binding = 1, r32ui) writeonly uniform highp uimage2D u_dstImg;\n"
		<< "layout(binding = 0) readonly buffer Input {\n"
		<< "    uint values[" << (m_imageSize.x() * m_imageSize.y()) << "];\n"
		<< "} sb_in;\n\n"
		<< "void main (void) {\n"
		<< "    uint stride = gl_NumWorkGroups.x*gl_WorkGroupSize.x;\n"
		<< "    uint value  = sb_in.values[gl_GlobalInvocationID.y*stride + gl_GlobalInvocationID.x];\n"
		<< "    imageStore(u_dstImg, ivec2(gl_GlobalInvocationID.xy), uvec4(value, 0, 0, 0));\n"
		<< "}\n";

	sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance* CopySSBOToImageTest::createInstance (Context& context) const
{
	return new CopySSBOToImageTestInstance(context, m_localSize, m_imageSize);
}

CopySSBOToImageTestInstance::CopySSBOToImageTestInstance (Context& context, const tcu::IVec2& localSize, const tcu::IVec2& imageSize)
	: TestInstance	(context)
	, m_localSize	(localSize)
	, m_imageSize	(imageSize)
{
}

tcu::TestStatus CopySSBOToImageTestInstance::iterate (void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			device				= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&				allocator			= m_context.getDefaultAllocator();

	// Create an image

	const VkImageCreateInfo imageParams = make2DImageCreateInfo(m_imageSize, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	const Image image(vk, device, allocator, imageParams, MemoryRequirement::Any);

	const VkImageSubresourceRange subresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const Unique<VkImageView> imageView(makeImageView(vk, device, *image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32_UINT, subresourceRange));

	// Create an input buffer (data to be read in the shader)

	const deUint32 imageArea = multiplyComponents(m_imageSize);
	const VkDeviceSize bufferSizeBytes = sizeof(deUint32) * imageArea;

	const Buffer inputBuffer(vk, device, allocator, makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);

	// Populate the buffer with test data
	{
		de::Random rnd(0x77238ac2);
		const Allocation& inputBufferAllocation = inputBuffer.getAllocation();
		deUint32* bufferPtr = static_cast<deUint32*>(inputBufferAllocation.getHostPtr());
		for (deUint32 i = 0; i < imageArea; ++i)
			*bufferPtr++ = rnd.getUint32();

		flushAlloc(vk, device, inputBufferAllocation);
	}

	// Create a buffer to store shader output (copied from image data)

	const Buffer outputBuffer(vk, device, allocator, makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible);

	// Create descriptor set

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	// Set the bindings

	const VkDescriptorImageInfo imageDescriptorInfo = makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);
	const VkDescriptorBufferInfo bufferDescriptorInfo = makeDescriptorBufferInfo(*inputBuffer, 0ull, bufferSizeBytes);

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageDescriptorInfo)
		.update(vk, device);

	// Perform the computation
	{
		const Unique<VkShaderModule> shaderModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));
		const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
		const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

		const VkBufferMemoryBarrier inputBufferPostHostWriteBarrier = makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *inputBuffer, 0ull, bufferSizeBytes);

		const VkImageMemoryBarrier imageLayoutBarrier = makeImageMemoryBarrier(
			0u, VK_ACCESS_SHADER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			*image, subresourceRange);

		const tcu::IVec2 workSize = m_imageSize / m_localSize;

		// Prepare the command buffer

		const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
		const Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

		// Start recording commands

		beginCommandBuffer(vk, *cmdBuffer);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &inputBufferPostHostWriteBarrier, 1, &imageLayoutBarrier);
		vk.cmdDispatch(*cmdBuffer, workSize.x(), workSize.y(), 1u);

		copyImageToBuffer(vk, *cmdBuffer, *image, *outputBuffer, m_imageSize, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

		endCommandBuffer(vk, *cmdBuffer);

		// Wait for completion

		submitCommandsAndWait(vk, device, queue, *cmdBuffer);
	}

	// Validate the results

	const Allocation& outputBufferAllocation = outputBuffer.getAllocation();
	invalidateAlloc(vk, device, outputBufferAllocation);

	const deUint32* bufferPtr = static_cast<deUint32*>(outputBufferAllocation.getHostPtr());
	const deUint32* refBufferPtr = static_cast<deUint32*>(inputBuffer.getAllocation().getHostPtr());

	for (deUint32 ndx = 0; ndx < imageArea; ++ndx)
	{
		const deUint32 res = *(bufferPtr + ndx);
		const deUint32 ref = *(refBufferPtr + ndx);

		if (res != ref)
		{
			std::ostringstream msg;
			msg << "Comparison failed for pixel " << ndx;
			return tcu::TestStatus::fail(msg.str());
		}
	}
	return tcu::TestStatus::pass("Compute succeeded");
}

class BufferToBufferInvertTest : public vkt::TestCase
{
public:
	void								initPrograms				(SourceCollections&	sourceCollections) const;
	TestInstance*						createInstance				(Context&			context) const;

	static BufferToBufferInvertTest*	UBOToSSBOInvertCase			(tcu::TestContext&	testCtx,
																	 const std::string& name,
																	 const std::string& description,
																	 const deUint32		numValues,
																	 const tcu::IVec3&	localSize,
																	 const tcu::IVec3&	workSize);

	static BufferToBufferInvertTest*	CopyInvertSSBOCase			(tcu::TestContext&	testCtx,
																	 const std::string& name,
																	 const std::string& description,
																	 const deUint32		numValues,
																	 const tcu::IVec3&	localSize,
																	 const tcu::IVec3&	workSize);

private:
										BufferToBufferInvertTest	(tcu::TestContext&	testCtx,
																	 const std::string& name,
																	 const std::string& description,
																	 const deUint32		numValues,
																	 const tcu::IVec3&	localSize,
																	 const tcu::IVec3&	workSize,
																	 const BufferType	bufferType);

	const BufferType					m_bufferType;
	const deUint32						m_numValues;
	const tcu::IVec3					m_localSize;
	const tcu::IVec3					m_workSize;
};

class BufferToBufferInvertTestInstance : public vkt::TestInstance
{
public:
									BufferToBufferInvertTestInstance	(Context&			context,
																		 const deUint32		numValues,
																		 const tcu::IVec3&	localSize,
																		 const tcu::IVec3&	workSize,
																		 const BufferType	bufferType);

	tcu::TestStatus					iterate								(void);

private:
	const BufferType				m_bufferType;
	const deUint32					m_numValues;
	const tcu::IVec3				m_localSize;
	const tcu::IVec3				m_workSize;
};

BufferToBufferInvertTest::BufferToBufferInvertTest (tcu::TestContext&	testCtx,
													const std::string&	name,
													const std::string&	description,
													const deUint32		numValues,
													const tcu::IVec3&	localSize,
													const tcu::IVec3&	workSize,
													const BufferType	bufferType)
	: TestCase		(testCtx, name, description)
	, m_bufferType	(bufferType)
	, m_numValues	(numValues)
	, m_localSize	(localSize)
	, m_workSize	(workSize)
{
	DE_ASSERT(m_numValues % (multiplyComponents(m_workSize) * multiplyComponents(m_localSize)) == 0);
	DE_ASSERT(m_bufferType == BUFFER_TYPE_UNIFORM || m_bufferType == BUFFER_TYPE_SSBO);
}

BufferToBufferInvertTest* BufferToBufferInvertTest::UBOToSSBOInvertCase (tcu::TestContext&	testCtx,
																		 const std::string&	name,
																		 const std::string&	description,
																		 const deUint32		numValues,
																		 const tcu::IVec3&	localSize,
																		 const tcu::IVec3&	workSize)
{
	return new BufferToBufferInvertTest(testCtx, name, description, numValues, localSize, workSize, BUFFER_TYPE_UNIFORM);
}

BufferToBufferInvertTest* BufferToBufferInvertTest::CopyInvertSSBOCase (tcu::TestContext&	testCtx,
																		const std::string&	name,
																		const std::string&	description,
																		const deUint32		numValues,
																		const tcu::IVec3&	localSize,
																		const tcu::IVec3&	workSize)
{
	return new BufferToBufferInvertTest(testCtx, name, description, numValues, localSize, workSize, BUFFER_TYPE_SSBO);
}

void BufferToBufferInvertTest::initPrograms (SourceCollections& sourceCollections) const
{
	std::ostringstream src;
	if (m_bufferType == BUFFER_TYPE_UNIFORM)
	{
		src << "#version 310 es\n"
			<< "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y() << ", local_size_z = " << m_localSize.z() << ") in;\n"
			<< "layout(binding = 0) readonly uniform Input {\n"
			<< "    uint values[" << m_numValues << "];\n"
			<< "} ub_in;\n"
			<< "layout(binding = 1, std140) writeonly buffer Output {\n"
			<< "    uint values[" << m_numValues << "];\n"
			<< "} sb_out;\n"
			<< "void main (void) {\n"
			<< "    uvec3 size           = gl_NumWorkGroups * gl_WorkGroupSize;\n"
			<< "    uint numValuesPerInv = uint(ub_in.values.length()) / (size.x*size.y*size.z);\n"
			<< "    uint groupNdx        = size.x*size.y*gl_GlobalInvocationID.z + size.x*gl_GlobalInvocationID.y + gl_GlobalInvocationID.x;\n"
			<< "    uint offset          = numValuesPerInv*groupNdx;\n"
			<< "\n"
			<< "    for (uint ndx = 0u; ndx < numValuesPerInv; ndx++)\n"
			<< "        sb_out.values[offset + ndx] = ~ub_in.values[offset + ndx];\n"
			<< "}\n";
	}
	else if (m_bufferType == BUFFER_TYPE_SSBO)
	{
		src << "#version 310 es\n"
			<< "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y() << ", local_size_z = " << m_localSize.z() << ") in;\n"
			<< "layout(binding = 0, std140) readonly buffer Input {\n"
			<< "    uint values[" << m_numValues << "];\n"
			<< "} sb_in;\n"
			<< "layout (binding = 1, std140) writeonly buffer Output {\n"
			<< "    uint values[" << m_numValues << "];\n"
			<< "} sb_out;\n"
			<< "void main (void) {\n"
			<< "    uvec3 size           = gl_NumWorkGroups * gl_WorkGroupSize;\n"
			<< "    uint numValuesPerInv = uint(sb_in.values.length()) / (size.x*size.y*size.z);\n"
			<< "    uint groupNdx        = size.x*size.y*gl_GlobalInvocationID.z + size.x*gl_GlobalInvocationID.y + gl_GlobalInvocationID.x;\n"
			<< "    uint offset          = numValuesPerInv*groupNdx;\n"
			<< "\n"
			<< "    for (uint ndx = 0u; ndx < numValuesPerInv; ndx++)\n"
			<< "        sb_out.values[offset + ndx] = ~sb_in.values[offset + ndx];\n"
			<< "}\n";
	}

	sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance* BufferToBufferInvertTest::createInstance (Context& context) const
{
	return new BufferToBufferInvertTestInstance(context, m_numValues, m_localSize, m_workSize, m_bufferType);
}

BufferToBufferInvertTestInstance::BufferToBufferInvertTestInstance (Context&			context,
																	const deUint32		numValues,
																	const tcu::IVec3&	localSize,
																	const tcu::IVec3&	workSize,
																	const BufferType	bufferType)
	: TestInstance	(context)
	, m_bufferType	(bufferType)
	, m_numValues	(numValues)
	, m_localSize	(localSize)
	, m_workSize	(workSize)
{
}

tcu::TestStatus BufferToBufferInvertTestInstance::iterate (void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			device				= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&				allocator			= m_context.getDefaultAllocator();

	// Customize the test based on buffer type

	const VkBufferUsageFlags inputBufferUsageFlags		= (m_bufferType == BUFFER_TYPE_UNIFORM ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	const VkDescriptorType inputBufferDescriptorType	= (m_bufferType == BUFFER_TYPE_UNIFORM ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	const deUint32 randomSeed							= (m_bufferType == BUFFER_TYPE_UNIFORM ? 0x111223f : 0x124fef);

	// Create an input buffer

	const VkDeviceSize bufferSizeBytes = sizeof(tcu::UVec4) * m_numValues;
	const Buffer inputBuffer(vk, device, allocator, makeBufferCreateInfo(bufferSizeBytes, inputBufferUsageFlags), MemoryRequirement::HostVisible);

	// Fill the input buffer with data
	{
		de::Random rnd(randomSeed);
		const Allocation& inputBufferAllocation = inputBuffer.getAllocation();
		tcu::UVec4* bufferPtr = static_cast<tcu::UVec4*>(inputBufferAllocation.getHostPtr());
		for (deUint32 i = 0; i < m_numValues; ++i)
			bufferPtr[i].x() = rnd.getUint32();

		flushAlloc(vk, device, inputBufferAllocation);
	}

	// Create an output buffer

	const Buffer outputBuffer(vk, device, allocator, makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);

	// Create descriptor set

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(inputBufferDescriptorType, VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(
		DescriptorPoolBuilder()
		.addType(inputBufferDescriptorType)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	const VkDescriptorBufferInfo inputBufferDescriptorInfo = makeDescriptorBufferInfo(*inputBuffer, 0ull, bufferSizeBytes);
	const VkDescriptorBufferInfo outputBufferDescriptorInfo = makeDescriptorBufferInfo(*outputBuffer, 0ull, bufferSizeBytes);
	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), inputBufferDescriptorType, &inputBufferDescriptorInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputBufferDescriptorInfo)
		.update(vk, device);

	// Perform the computation

	const Unique<VkShaderModule> shaderModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));
	const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

	const VkBufferMemoryBarrier hostWriteBarrier = makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *inputBuffer, 0ull, bufferSizeBytes);

	const VkBufferMemoryBarrier shaderWriteBarrier = makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *outputBuffer, 0ull, bufferSizeBytes);

	const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
	const Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Start recording commands

	beginCommandBuffer(vk, *cmdBuffer);

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &hostWriteBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	vk.cmdDispatch(*cmdBuffer, m_workSize.x(), m_workSize.y(), m_workSize.z());
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &shaderWriteBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

	endCommandBuffer(vk, *cmdBuffer);

	// Wait for completion

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// Validate the results

	const Allocation& outputBufferAllocation = outputBuffer.getAllocation();
	invalidateAlloc(vk, device, outputBufferAllocation);

	const tcu::UVec4* bufferPtr = static_cast<tcu::UVec4*>(outputBufferAllocation.getHostPtr());
	const tcu::UVec4* refBufferPtr = static_cast<tcu::UVec4*>(inputBuffer.getAllocation().getHostPtr());

	for (deUint32 ndx = 0; ndx < m_numValues; ++ndx)
	{
		const deUint32 res = bufferPtr[ndx].x();
		const deUint32 ref = ~refBufferPtr[ndx].x();

		if (res != ref)
		{
			std::ostringstream msg;
			msg << "Comparison failed for Output.values[" << ndx << "]";
			return tcu::TestStatus::fail(msg.str());
		}
	}
	return tcu::TestStatus::pass("Compute succeeded");
}

class InvertSSBOInPlaceTest : public vkt::TestCase
{
public:
						InvertSSBOInPlaceTest	(tcu::TestContext&	testCtx,
												 const std::string&	name,
												 const std::string&	description,
												 const deUint32		numValues,
												 const bool			sized,
												 const tcu::IVec3&	localSize,
												 const tcu::IVec3&	workSize);


	void				initPrograms			(SourceCollections& sourceCollections) const;
	TestInstance*		createInstance			(Context&			context) const;

private:
	const deUint32		m_numValues;
	const bool			m_sized;
	const tcu::IVec3	m_localSize;
	const tcu::IVec3	m_workSize;
};

class InvertSSBOInPlaceTestInstance : public vkt::TestInstance
{
public:
									InvertSSBOInPlaceTestInstance	(Context&			context,
																	 const deUint32		numValues,
																	 const tcu::IVec3&	localSize,
																	 const tcu::IVec3&	workSize);

	tcu::TestStatus					iterate							(void);

private:
	const deUint32					m_numValues;
	const tcu::IVec3				m_localSize;
	const tcu::IVec3				m_workSize;
};

InvertSSBOInPlaceTest::InvertSSBOInPlaceTest (tcu::TestContext&		testCtx,
											  const std::string&	name,
											  const std::string&	description,
											  const deUint32		numValues,
											  const bool			sized,
											  const tcu::IVec3&		localSize,
											  const tcu::IVec3&		workSize)
	: TestCase		(testCtx, name, description)
	, m_numValues	(numValues)
	, m_sized		(sized)
	, m_localSize	(localSize)
	, m_workSize	(workSize)
{
	DE_ASSERT(m_numValues % (multiplyComponents(m_workSize) * multiplyComponents(m_localSize)) == 0);
}

void InvertSSBOInPlaceTest::initPrograms (SourceCollections& sourceCollections) const
{
	std::ostringstream src;
	src << "#version 310 es\n"
		<< "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y() << ", local_size_z = " << m_localSize.z() << ") in;\n"
		<< "layout(binding = 0) buffer InOut {\n"
		<< "    uint values[" << (m_sized ? de::toString(m_numValues) : "") << "];\n"
		<< "} sb_inout;\n"
		<< "void main (void) {\n"
		<< "    uvec3 size           = gl_NumWorkGroups * gl_WorkGroupSize;\n"
		<< "    uint numValuesPerInv = uint(sb_inout.values.length()) / (size.x*size.y*size.z);\n"
		<< "    uint groupNdx        = size.x*size.y*gl_GlobalInvocationID.z + size.x*gl_GlobalInvocationID.y + gl_GlobalInvocationID.x;\n"
		<< "    uint offset          = numValuesPerInv*groupNdx;\n"
		<< "\n"
		<< "    for (uint ndx = 0u; ndx < numValuesPerInv; ndx++)\n"
		<< "        sb_inout.values[offset + ndx] = ~sb_inout.values[offset + ndx];\n"
		<< "}\n";

	sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance* InvertSSBOInPlaceTest::createInstance (Context& context) const
{
	return new InvertSSBOInPlaceTestInstance(context, m_numValues, m_localSize, m_workSize);
}

InvertSSBOInPlaceTestInstance::InvertSSBOInPlaceTestInstance (Context&			context,
															  const deUint32	numValues,
															  const tcu::IVec3&	localSize,
															  const tcu::IVec3&	workSize)
	: TestInstance	(context)
	, m_numValues	(numValues)
	, m_localSize	(localSize)
	, m_workSize	(workSize)
{
}

tcu::TestStatus InvertSSBOInPlaceTestInstance::iterate (void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			device				= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&				allocator			= m_context.getDefaultAllocator();

	// Create an input/output buffer

	const VkDeviceSize bufferSizeBytes = sizeof(deUint32) * m_numValues;
	const Buffer buffer(vk, device, allocator, makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);

	// Fill the buffer with data

	typedef std::vector<deUint32> data_vector_t;
	data_vector_t inputData(m_numValues);

	{
		de::Random rnd(0x82ce7f);
		const Allocation& bufferAllocation = buffer.getAllocation();
		deUint32* bufferPtr = static_cast<deUint32*>(bufferAllocation.getHostPtr());
		for (deUint32 i = 0; i < m_numValues; ++i)
			inputData[i] = *bufferPtr++ = rnd.getUint32();

		flushAlloc(vk, device, bufferAllocation);
	}

	// Create descriptor set

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	const VkDescriptorBufferInfo bufferDescriptorInfo = makeDescriptorBufferInfo(*buffer, 0ull, bufferSizeBytes);
	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo)
		.update(vk, device);

	// Perform the computation

	const Unique<VkShaderModule> shaderModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));
	const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

	const VkBufferMemoryBarrier hostWriteBarrier = makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *buffer, 0ull, bufferSizeBytes);

	const VkBufferMemoryBarrier shaderWriteBarrier = makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer, 0ull, bufferSizeBytes);

	const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
	const Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Start recording commands

	beginCommandBuffer(vk, *cmdBuffer);

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &hostWriteBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
	vk.cmdDispatch(*cmdBuffer, m_workSize.x(), m_workSize.y(), m_workSize.z());
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &shaderWriteBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

	endCommandBuffer(vk, *cmdBuffer);

	// Wait for completion

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// Validate the results

	const Allocation& bufferAllocation = buffer.getAllocation();
	invalidateAlloc(vk, device, bufferAllocation);

	const deUint32* bufferPtr = static_cast<deUint32*>(bufferAllocation.getHostPtr());

	for (deUint32 ndx = 0; ndx < m_numValues; ++ndx)
	{
		const deUint32 res = bufferPtr[ndx];
		const deUint32 ref = ~inputData[ndx];

		if (res != ref)
		{
			std::ostringstream msg;
			msg << "Comparison failed for InOut.values[" << ndx << "]";
			return tcu::TestStatus::fail(msg.str());
		}
	}
	return tcu::TestStatus::pass("Compute succeeded");
}

class WriteToMultipleSSBOTest : public vkt::TestCase
{
public:
						WriteToMultipleSSBOTest	(tcu::TestContext&	testCtx,
												 const std::string&	name,
												 const std::string&	description,
												 const deUint32		numValues,
												 const bool			sized,
												 const tcu::IVec3&	localSize,
												 const tcu::IVec3&	workSize);

	void				initPrograms			(SourceCollections& sourceCollections) const;
	TestInstance*		createInstance			(Context&			context) const;

private:
	const deUint32		m_numValues;
	const bool			m_sized;
	const tcu::IVec3	m_localSize;
	const tcu::IVec3	m_workSize;
};

class WriteToMultipleSSBOTestInstance : public vkt::TestInstance
{
public:
									WriteToMultipleSSBOTestInstance	(Context&			context,
																	 const deUint32		numValues,
																	 const tcu::IVec3&	localSize,
																	 const tcu::IVec3&	workSize);

	tcu::TestStatus					iterate							(void);

private:
	const deUint32					m_numValues;
	const tcu::IVec3				m_localSize;
	const tcu::IVec3				m_workSize;
};

WriteToMultipleSSBOTest::WriteToMultipleSSBOTest (tcu::TestContext&		testCtx,
												  const std::string&	name,
												  const std::string&	description,
												  const deUint32		numValues,
												  const bool			sized,
												  const tcu::IVec3&		localSize,
												  const tcu::IVec3&		workSize)
	: TestCase		(testCtx, name, description)
	, m_numValues	(numValues)
	, m_sized		(sized)
	, m_localSize	(localSize)
	, m_workSize	(workSize)
{
	DE_ASSERT(m_numValues % (multiplyComponents(m_workSize) * multiplyComponents(m_localSize)) == 0);
}

void WriteToMultipleSSBOTest::initPrograms (SourceCollections& sourceCollections) const
{
	std::ostringstream src;
	src << "#version 310 es\n"
		<< "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y() << ", local_size_z = " << m_localSize.z() << ") in;\n"
		<< "layout(binding = 0) writeonly buffer Out0 {\n"
		<< "    uint values[" << (m_sized ? de::toString(m_numValues) : "") << "];\n"
		<< "} sb_out0;\n"
		<< "layout(binding = 1) writeonly buffer Out1 {\n"
		<< "    uint values[" << (m_sized ? de::toString(m_numValues) : "") << "];\n"
		<< "} sb_out1;\n"
		<< "void main (void) {\n"
		<< "    uvec3 size      = gl_NumWorkGroups * gl_WorkGroupSize;\n"
		<< "    uint groupNdx   = size.x*size.y*gl_GlobalInvocationID.z + size.x*gl_GlobalInvocationID.y + gl_GlobalInvocationID.x;\n"
		<< "\n"
		<< "    {\n"
		<< "        uint numValuesPerInv = uint(sb_out0.values.length()) / (size.x*size.y*size.z);\n"
		<< "        uint offset          = numValuesPerInv*groupNdx;\n"
		<< "\n"
		<< "        for (uint ndx = 0u; ndx < numValuesPerInv; ndx++)\n"
		<< "            sb_out0.values[offset + ndx] = offset + ndx;\n"
		<< "    }\n"
		<< "    {\n"
		<< "        uint numValuesPerInv = uint(sb_out1.values.length()) / (size.x*size.y*size.z);\n"
		<< "        uint offset          = numValuesPerInv*groupNdx;\n"
		<< "\n"
		<< "        for (uint ndx = 0u; ndx < numValuesPerInv; ndx++)\n"
		<< "            sb_out1.values[offset + ndx] = uint(sb_out1.values.length()) - offset - ndx;\n"
		<< "    }\n"
		<< "}\n";

	sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance* WriteToMultipleSSBOTest::createInstance (Context& context) const
{
	return new WriteToMultipleSSBOTestInstance(context, m_numValues, m_localSize, m_workSize);
}

WriteToMultipleSSBOTestInstance::WriteToMultipleSSBOTestInstance (Context&			context,
																  const deUint32	numValues,
																  const tcu::IVec3&	localSize,
																  const tcu::IVec3&	workSize)
	: TestInstance	(context)
	, m_numValues	(numValues)
	, m_localSize	(localSize)
	, m_workSize	(workSize)
{
}

tcu::TestStatus WriteToMultipleSSBOTestInstance::iterate (void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			device				= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&				allocator			= m_context.getDefaultAllocator();

	// Create two output buffers

	const VkDeviceSize bufferSizeBytes = sizeof(deUint32) * m_numValues;
	const Buffer buffer0(vk, device, allocator, makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);
	const Buffer buffer1(vk, device, allocator, makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);

	// Create descriptor set

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	const VkDescriptorBufferInfo buffer0DescriptorInfo = makeDescriptorBufferInfo(*buffer0, 0ull, bufferSizeBytes);
	const VkDescriptorBufferInfo buffer1DescriptorInfo = makeDescriptorBufferInfo(*buffer1, 0ull, bufferSizeBytes);
	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &buffer0DescriptorInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &buffer1DescriptorInfo)
		.update(vk, device);

	// Perform the computation

	const Unique<VkShaderModule> shaderModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));
	const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

	const VkBufferMemoryBarrier shaderWriteBarriers[] =
	{
		makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer0, 0ull, bufferSizeBytes),
		makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer1, 0ull, bufferSizeBytes)
	};

	const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
	const Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Start recording commands

	beginCommandBuffer(vk, *cmdBuffer);

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

	vk.cmdDispatch(*cmdBuffer, m_workSize.x(), m_workSize.y(), m_workSize.z());
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, DE_LENGTH_OF_ARRAY(shaderWriteBarriers), shaderWriteBarriers, 0, (const VkImageMemoryBarrier*)DE_NULL);

	endCommandBuffer(vk, *cmdBuffer);

	// Wait for completion

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// Validate the results
	{
		const Allocation& buffer0Allocation = buffer0.getAllocation();
		invalidateAlloc(vk, device, buffer0Allocation);
		const deUint32* buffer0Ptr = static_cast<deUint32*>(buffer0Allocation.getHostPtr());

		for (deUint32 ndx = 0; ndx < m_numValues; ++ndx)
		{
			const deUint32 res = buffer0Ptr[ndx];
			const deUint32 ref = ndx;

			if (res != ref)
			{
				std::ostringstream msg;
				msg << "Comparison failed for Out0.values[" << ndx << "] res=" << res << " ref=" << ref;
				return tcu::TestStatus::fail(msg.str());
			}
		}
	}
	{
		const Allocation& buffer1Allocation = buffer1.getAllocation();
		invalidateAlloc(vk, device, buffer1Allocation);
		const deUint32* buffer1Ptr = static_cast<deUint32*>(buffer1Allocation.getHostPtr());

		for (deUint32 ndx = 0; ndx < m_numValues; ++ndx)
		{
			const deUint32 res = buffer1Ptr[ndx];
			const deUint32 ref = m_numValues - ndx;

			if (res != ref)
			{
				std::ostringstream msg;
				msg << "Comparison failed for Out1.values[" << ndx << "] res=" << res << " ref=" << ref;
				return tcu::TestStatus::fail(msg.str());
			}
		}
	}
	return tcu::TestStatus::pass("Compute succeeded");
}

class SSBOBarrierTest : public vkt::TestCase
{
public:
						SSBOBarrierTest		(tcu::TestContext&	testCtx,
											 const std::string&	name,
											 const std::string&	description,
											 const tcu::IVec3&	workSize);

	void				initPrograms		(SourceCollections& sourceCollections) const;
	TestInstance*		createInstance		(Context&			context) const;

private:
	const tcu::IVec3	m_workSize;
};

class SSBOBarrierTestInstance : public vkt::TestInstance
{
public:
									SSBOBarrierTestInstance		(Context&			context,
																 const tcu::IVec3&	workSize);

	tcu::TestStatus					iterate						(void);

private:
	const tcu::IVec3				m_workSize;
};

SSBOBarrierTest::SSBOBarrierTest (tcu::TestContext&		testCtx,
								  const std::string&	name,
								  const std::string&	description,
								  const tcu::IVec3&		workSize)
	: TestCase		(testCtx, name, description)
	, m_workSize	(workSize)
{
}

void SSBOBarrierTest::initPrograms (SourceCollections& sourceCollections) const
{
	sourceCollections.glslSources.add("comp0") << glu::ComputeSource(
		"#version 310 es\n"
		"layout (local_size_x = 1) in;\n"
		"layout(binding = 2) readonly uniform Constants {\n"
		"    uint u_baseVal;\n"
		"};\n"
		"layout(binding = 1) writeonly buffer Output {\n"
		"    uint values[];\n"
		"};\n"
		"void main (void) {\n"
		"    uint offset = gl_NumWorkGroups.x*gl_NumWorkGroups.y*gl_WorkGroupID.z + gl_NumWorkGroups.x*gl_WorkGroupID.y + gl_WorkGroupID.x;\n"
		"    values[offset] = u_baseVal + offset;\n"
		"}\n");

	sourceCollections.glslSources.add("comp1") << glu::ComputeSource(
		"#version 310 es\n"
		"layout (local_size_x = 1) in;\n"
		"layout(binding = 1) readonly buffer Input {\n"
		"    uint values[];\n"
		"};\n"
		"layout(binding = 0) coherent buffer Output {\n"
		"    uint sum;\n"
		"};\n"
		"void main (void) {\n"
		"    uint offset = gl_NumWorkGroups.x*gl_NumWorkGroups.y*gl_WorkGroupID.z + gl_NumWorkGroups.x*gl_WorkGroupID.y + gl_WorkGroupID.x;\n"
		"    uint value  = values[offset];\n"
		"    atomicAdd(sum, value);\n"
		"}\n");
}

TestInstance* SSBOBarrierTest::createInstance (Context& context) const
{
	return new SSBOBarrierTestInstance(context, m_workSize);
}

SSBOBarrierTestInstance::SSBOBarrierTestInstance (Context& context, const tcu::IVec3& workSize)
	: TestInstance	(context)
	, m_workSize	(workSize)
{
}

tcu::TestStatus SSBOBarrierTestInstance::iterate (void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			device				= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&				allocator			= m_context.getDefaultAllocator();

	// Create a work buffer used by both shaders

	const int workGroupCount = multiplyComponents(m_workSize);
	const VkDeviceSize workBufferSizeBytes = sizeof(deUint32) * workGroupCount;
	const Buffer workBuffer(vk, device, allocator, makeBufferCreateInfo(workBufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::Any);

	// Create an output buffer

	const VkDeviceSize outputBufferSizeBytes = sizeof(deUint32);
	const Buffer outputBuffer(vk, device, allocator, makeBufferCreateInfo(outputBufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);

	// Initialize atomic counter value to zero
	{
		const Allocation& outputBufferAllocation = outputBuffer.getAllocation();
		deUint32* outputBufferPtr = static_cast<deUint32*>(outputBufferAllocation.getHostPtr());
		*outputBufferPtr = 0;
		flushAlloc(vk, device, outputBufferAllocation);
	}

	// Create a uniform buffer (to pass uniform constants)

	const VkDeviceSize uniformBufferSizeBytes = sizeof(deUint32);
	const Buffer uniformBuffer(vk, device, allocator, makeBufferCreateInfo(uniformBufferSizeBytes, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT), MemoryRequirement::HostVisible);

	// Set the constants in the uniform buffer

	const deUint32	baseValue = 127;
	{
		const Allocation& uniformBufferAllocation = uniformBuffer.getAllocation();
		deUint32* uniformBufferPtr = static_cast<deUint32*>(uniformBufferAllocation.getHostPtr());
		uniformBufferPtr[0] = baseValue;

		flushAlloc(vk, device, uniformBufferAllocation);
	}

	// Create descriptor set

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2u)
		.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	const VkDescriptorBufferInfo workBufferDescriptorInfo = makeDescriptorBufferInfo(*workBuffer, 0ull, workBufferSizeBytes);
	const VkDescriptorBufferInfo outputBufferDescriptorInfo = makeDescriptorBufferInfo(*outputBuffer, 0ull, outputBufferSizeBytes);
	const VkDescriptorBufferInfo uniformBufferDescriptorInfo = makeDescriptorBufferInfo(*uniformBuffer, 0ull, uniformBufferSizeBytes);
	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputBufferDescriptorInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &workBufferDescriptorInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBufferDescriptorInfo)
		.update(vk, device);

	// Perform the computation

	const Unique<VkShaderModule> shaderModule0(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp0"), 0));
	const Unique<VkShaderModule> shaderModule1(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp1"), 0));

	const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline> pipeline0(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule0));
	const Unique<VkPipeline> pipeline1(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule1));

	const VkBufferMemoryBarrier writeUniformConstantsBarrier = makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT, *uniformBuffer, 0ull, uniformBufferSizeBytes);

	const VkBufferMemoryBarrier betweenShadersBarrier = makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *workBuffer, 0ull, workBufferSizeBytes);

	const VkBufferMemoryBarrier afterComputeBarrier = makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *outputBuffer, 0ull, outputBufferSizeBytes);

	const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
	const Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Start recording commands

	beginCommandBuffer(vk, *cmdBuffer);

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline0);
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &writeUniformConstantsBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

	vk.cmdDispatch(*cmdBuffer, m_workSize.x(), m_workSize.y(), m_workSize.z());
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &betweenShadersBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

	// Switch to the second shader program
	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline1);

	vk.cmdDispatch(*cmdBuffer, m_workSize.x(), m_workSize.y(), m_workSize.z());
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &afterComputeBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

	endCommandBuffer(vk, *cmdBuffer);

	// Wait for completion

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// Validate the results

	const Allocation& outputBufferAllocation = outputBuffer.getAllocation();
	invalidateAlloc(vk, device, outputBufferAllocation);

	const deUint32* bufferPtr = static_cast<deUint32*>(outputBufferAllocation.getHostPtr());
	const deUint32	res = *bufferPtr;
	deUint32		ref = 0;

	for (int ndx = 0; ndx < workGroupCount; ++ndx)
		ref += baseValue + ndx;

	if (res != ref)
	{
		std::ostringstream msg;
		msg << "ERROR: comparison failed, expected " << ref << ", got " << res;
		return tcu::TestStatus::fail(msg.str());
	}
	return tcu::TestStatus::pass("Compute succeeded");
}

class ImageAtomicOpTest : public vkt::TestCase
{
public:
						ImageAtomicOpTest		(tcu::TestContext&	testCtx,
												 const std::string& name,
												 const std::string& description,
												 const deUint32		localSize,
												 const tcu::IVec2&	imageSize);

	void				initPrograms			(SourceCollections& sourceCollections) const;
	TestInstance*		createInstance			(Context&			context) const;

private:
	const deUint32		m_localSize;
	const tcu::IVec2	m_imageSize;
};

class ImageAtomicOpTestInstance : public vkt::TestInstance
{
public:
									ImageAtomicOpTestInstance		(Context&			context,
																	 const deUint32		localSize,
																	 const tcu::IVec2&	imageSize);

	tcu::TestStatus					iterate							(void);

private:
	const deUint32					m_localSize;
	const tcu::IVec2				m_imageSize;
};

ImageAtomicOpTest::ImageAtomicOpTest (tcu::TestContext&		testCtx,
									  const std::string&	name,
									  const std::string&	description,
									  const deUint32		localSize,
									  const tcu::IVec2&		imageSize)
	: TestCase		(testCtx, name, description)
	, m_localSize	(localSize)
	, m_imageSize	(imageSize)
{
}

void ImageAtomicOpTest::initPrograms (SourceCollections& sourceCollections) const
{
	std::ostringstream src;
	src << "#version 310 es\n"
		<< "#extension GL_OES_shader_image_atomic : require\n"
		<< "layout (local_size_x = " << m_localSize << ") in;\n"
		<< "layout(binding = 1, r32ui) coherent uniform highp uimage2D u_dstImg;\n"
		<< "layout(binding = 0) readonly buffer Input {\n"
		<< "    uint values[" << (multiplyComponents(m_imageSize) * m_localSize) << "];\n"
		<< "} sb_in;\n\n"
		<< "void main (void) {\n"
		<< "    uint stride = gl_NumWorkGroups.x*gl_WorkGroupSize.x;\n"
		<< "    uint value  = sb_in.values[gl_GlobalInvocationID.y*stride + gl_GlobalInvocationID.x];\n"
		<< "\n"
		<< "    if (gl_LocalInvocationIndex == 0u)\n"
		<< "        imageStore(u_dstImg, ivec2(gl_WorkGroupID.xy), uvec4(0));\n"
		<< "    memoryBarrierImage();\n"
		<< "    barrier();\n"
		<< "    imageAtomicAdd(u_dstImg, ivec2(gl_WorkGroupID.xy), value);\n"
		<< "}\n";

	sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance* ImageAtomicOpTest::createInstance (Context& context) const
{
	return new ImageAtomicOpTestInstance(context, m_localSize, m_imageSize);
}

ImageAtomicOpTestInstance::ImageAtomicOpTestInstance (Context& context, const deUint32 localSize, const tcu::IVec2& imageSize)
	: TestInstance	(context)
	, m_localSize	(localSize)
	, m_imageSize	(imageSize)
{
}

tcu::TestStatus ImageAtomicOpTestInstance::iterate (void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			device				= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&				allocator			= m_context.getDefaultAllocator();

	// Create an image

	const VkImageCreateInfo imageParams = make2DImageCreateInfo(m_imageSize, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
	const Image image(vk, device, allocator, imageParams, MemoryRequirement::Any);

	const VkImageSubresourceRange subresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const Unique<VkImageView> imageView(makeImageView(vk, device, *image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32_UINT, subresourceRange));

	// Input buffer

	const deUint32 numInputValues = multiplyComponents(m_imageSize) * m_localSize;
	const VkDeviceSize inputBufferSizeBytes = sizeof(deUint32) * numInputValues;

	const Buffer inputBuffer(vk, device, allocator, makeBufferCreateInfo(inputBufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);

	// Populate the input buffer with test data
	{
		de::Random rnd(0x77238ac2);
		const Allocation& inputBufferAllocation = inputBuffer.getAllocation();
		deUint32* bufferPtr = static_cast<deUint32*>(inputBufferAllocation.getHostPtr());
		for (deUint32 i = 0; i < numInputValues; ++i)
			*bufferPtr++ = rnd.getUint32();

		flushAlloc(vk, device, inputBufferAllocation);
	}

	// Create a buffer to store shader output (copied from image data)

	const deUint32 imageArea = multiplyComponents(m_imageSize);
	const VkDeviceSize outputBufferSizeBytes = sizeof(deUint32) * imageArea;
	const Buffer outputBuffer(vk, device, allocator, makeBufferCreateInfo(outputBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible);

	// Create descriptor set

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	// Set the bindings

	const VkDescriptorImageInfo imageDescriptorInfo = makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);
	const VkDescriptorBufferInfo bufferDescriptorInfo = makeDescriptorBufferInfo(*inputBuffer, 0ull, inputBufferSizeBytes);

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageDescriptorInfo)
		.update(vk, device);

	// Perform the computation
	{
		const Unique<VkShaderModule> shaderModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));
		const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
		const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

		const VkBufferMemoryBarrier inputBufferPostHostWriteBarrier = makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *inputBuffer, 0ull, inputBufferSizeBytes);

		const VkImageMemoryBarrier imageLayoutBarrier = makeImageMemoryBarrier(
			(VkAccessFlags)0, VK_ACCESS_SHADER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			*image, subresourceRange);

		// Prepare the command buffer

		const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
		const Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

		// Start recording commands

		beginCommandBuffer(vk, *cmdBuffer);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &inputBufferPostHostWriteBarrier, 1, &imageLayoutBarrier);
		vk.cmdDispatch(*cmdBuffer, m_imageSize.x(), m_imageSize.y(), 1u);

		copyImageToBuffer(vk, *cmdBuffer, *image, *outputBuffer, m_imageSize, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

		endCommandBuffer(vk, *cmdBuffer);

		// Wait for completion

		submitCommandsAndWait(vk, device, queue, *cmdBuffer);
	}

	// Validate the results

	const Allocation& outputBufferAllocation = outputBuffer.getAllocation();
	invalidateAlloc(vk, device, outputBufferAllocation);

	const deUint32* bufferPtr = static_cast<deUint32*>(outputBufferAllocation.getHostPtr());
	const deUint32* refBufferPtr = static_cast<deUint32*>(inputBuffer.getAllocation().getHostPtr());

	for (deUint32 pixelNdx = 0; pixelNdx < imageArea; ++pixelNdx)
	{
		const deUint32	res = bufferPtr[pixelNdx];
		deUint32		ref = 0;

		for (deUint32 offs = 0; offs < m_localSize; ++offs)
			ref += refBufferPtr[pixelNdx * m_localSize + offs];

		if (res != ref)
		{
			std::ostringstream msg;
			msg << "Comparison failed for pixel " << pixelNdx;
			return tcu::TestStatus::fail(msg.str());
		}
	}
	return tcu::TestStatus::pass("Compute succeeded");
}

class ImageBarrierTest : public vkt::TestCase
{
public:
						ImageBarrierTest	(tcu::TestContext&	testCtx,
											const std::string&	name,
											const std::string&	description,
											const tcu::IVec2&	imageSize);

	void				initPrograms		(SourceCollections& sourceCollections) const;
	TestInstance*		createInstance		(Context&			context) const;

private:
	const tcu::IVec2	m_imageSize;
};

class ImageBarrierTestInstance : public vkt::TestInstance
{
public:
									ImageBarrierTestInstance	(Context&			context,
																 const tcu::IVec2&	imageSize);

	tcu::TestStatus					iterate						(void);

private:
	const tcu::IVec2				m_imageSize;
};

ImageBarrierTest::ImageBarrierTest (tcu::TestContext&	testCtx,
									const std::string&	name,
									const std::string&	description,
									const tcu::IVec2&	imageSize)
	: TestCase		(testCtx, name, description)
	, m_imageSize	(imageSize)
{
}

void ImageBarrierTest::initPrograms (SourceCollections& sourceCollections) const
{
	sourceCollections.glslSources.add("comp0") << glu::ComputeSource(
		"#version 310 es\n"
		"layout (local_size_x = 1) in;\n"
		"layout(binding = 2) readonly uniform Constants {\n"
		"    uint u_baseVal;\n"
		"};\n"
		"layout(binding = 1, r32ui) writeonly uniform highp uimage2D u_img;\n"
		"void main (void) {\n"
		"    uint offset = gl_NumWorkGroups.x*gl_NumWorkGroups.y*gl_WorkGroupID.z + gl_NumWorkGroups.x*gl_WorkGroupID.y + gl_WorkGroupID.x;\n"
		"    imageStore(u_img, ivec2(gl_WorkGroupID.xy), uvec4(offset + u_baseVal, 0, 0, 0));\n"
		"}\n");

	sourceCollections.glslSources.add("comp1") << glu::ComputeSource(
		"#version 310 es\n"
		"layout (local_size_x = 1) in;\n"
		"layout(binding = 1, r32ui) readonly uniform highp uimage2D u_img;\n"
		"layout(binding = 0) coherent buffer Output {\n"
		"    uint sum;\n"
		"};\n"
		"void main (void) {\n"
		"    uint value = imageLoad(u_img, ivec2(gl_WorkGroupID.xy)).x;\n"
		"    atomicAdd(sum, value);\n"
		"}\n");
}

TestInstance* ImageBarrierTest::createInstance (Context& context) const
{
	return new ImageBarrierTestInstance(context, m_imageSize);
}

ImageBarrierTestInstance::ImageBarrierTestInstance (Context& context, const tcu::IVec2& imageSize)
	: TestInstance	(context)
	, m_imageSize	(imageSize)
{
}

tcu::TestStatus ImageBarrierTestInstance::iterate (void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			device				= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&				allocator			= m_context.getDefaultAllocator();

	// Create an image used by both shaders

	const VkImageCreateInfo imageParams = make2DImageCreateInfo(m_imageSize, VK_IMAGE_USAGE_STORAGE_BIT);
	const Image image(vk, device, allocator, imageParams, MemoryRequirement::Any);

	const VkImageSubresourceRange subresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const Unique<VkImageView> imageView(makeImageView(vk, device, *image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R32_UINT, subresourceRange));

	// Create an output buffer

	const VkDeviceSize outputBufferSizeBytes = sizeof(deUint32);
	const Buffer outputBuffer(vk, device, allocator, makeBufferCreateInfo(outputBufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);

	// Initialize atomic counter value to zero
	{
		const Allocation& outputBufferAllocation = outputBuffer.getAllocation();
		deUint32* outputBufferPtr = static_cast<deUint32*>(outputBufferAllocation.getHostPtr());
		*outputBufferPtr = 0;
		flushAlloc(vk, device, outputBufferAllocation);
	}

	// Create a uniform buffer (to pass uniform constants)

	const VkDeviceSize uniformBufferSizeBytes = sizeof(deUint32);
	const Buffer uniformBuffer(vk, device, allocator, makeBufferCreateInfo(uniformBufferSizeBytes, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT), MemoryRequirement::HostVisible);

	// Set the constants in the uniform buffer

	const deUint32	baseValue = 127;
	{
		const Allocation& uniformBufferAllocation = uniformBuffer.getAllocation();
		deUint32* uniformBufferPtr = static_cast<deUint32*>(uniformBufferAllocation.getHostPtr());
		uniformBufferPtr[0] = baseValue;

		flushAlloc(vk, device, uniformBufferAllocation);
	}

	// Create descriptor set

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
		.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	const VkDescriptorImageInfo imageDescriptorInfo = makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);
	const VkDescriptorBufferInfo outputBufferDescriptorInfo = makeDescriptorBufferInfo(*outputBuffer, 0ull, outputBufferSizeBytes);
	const VkDescriptorBufferInfo uniformBufferDescriptorInfo = makeDescriptorBufferInfo(*uniformBuffer, 0ull, uniformBufferSizeBytes);
	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputBufferDescriptorInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageDescriptorInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBufferDescriptorInfo)
		.update(vk, device);

	// Perform the computation

	const Unique<VkShaderModule>	shaderModule0(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp0"), 0));
	const Unique<VkShaderModule>	shaderModule1(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp1"), 0));

	const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline> pipeline0(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule0));
	const Unique<VkPipeline> pipeline1(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule1));

	const VkBufferMemoryBarrier writeUniformConstantsBarrier = makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT, *uniformBuffer, 0ull, uniformBufferSizeBytes);

	const VkImageMemoryBarrier imageLayoutBarrier = makeImageMemoryBarrier(
		0u, 0u,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		*image, subresourceRange);

	const VkImageMemoryBarrier imageBarrierBetweenShaders = makeImageMemoryBarrier(
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		*image, subresourceRange);

	const VkBufferMemoryBarrier afterComputeBarrier = makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *outputBuffer, 0ull, outputBufferSizeBytes);

	const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
	const Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Start recording commands

	beginCommandBuffer(vk, *cmdBuffer);

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline0);
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &writeUniformConstantsBarrier, 1, &imageLayoutBarrier);

	vk.cmdDispatch(*cmdBuffer, m_imageSize.x(), m_imageSize.y(), 1u);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 0, (const VkBufferMemoryBarrier*)DE_NULL, 1, &imageBarrierBetweenShaders);

	// Switch to the second shader program
	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline1);

	vk.cmdDispatch(*cmdBuffer, m_imageSize.x(), m_imageSize.y(), 1u);
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &afterComputeBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

	endCommandBuffer(vk, *cmdBuffer);

	// Wait for completion

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// Validate the results

	const Allocation& outputBufferAllocation = outputBuffer.getAllocation();
	invalidateAlloc(vk, device, outputBufferAllocation);

	const int		numValues = multiplyComponents(m_imageSize);
	const deUint32* bufferPtr = static_cast<deUint32*>(outputBufferAllocation.getHostPtr());
	const deUint32	res = *bufferPtr;
	deUint32		ref = 0;

	for (int ndx = 0; ndx < numValues; ++ndx)
		ref += baseValue + ndx;

	if (res != ref)
	{
		std::ostringstream msg;
		msg << "ERROR: comparison failed, expected " << ref << ", got " << res;
		return tcu::TestStatus::fail(msg.str());
	}
	return tcu::TestStatus::pass("Compute succeeded");
}

class ComputeTestInstance : public vkt::TestInstance
{
public:
		ComputeTestInstance		(Context& context)
		: TestInstance			(context)
		, m_numPhysDevices		(1)
		, m_queueFamilyIndex	(0)
	{
		createDeviceGroup();
	}

	void							createDeviceGroup	(void);
	const vk::DeviceInterface&		getDeviceInterface	(void)			{ return *m_deviceDriver; }
	vk::VkInstance					getInstance			(void)			{ return m_deviceGroupInstance; }
	vk::VkDevice					getDevice			(void)			{ return *m_logicalDevice; }
	vk::VkPhysicalDevice			getPhysicalDevice	(deUint32 i = 0){ return m_physicalDevices[i]; }

protected:
	deUint32						m_numPhysDevices;
	deUint32						m_queueFamilyIndex;

private:
	CustomInstance						m_deviceGroupInstance;
	vk::Move<vk::VkDevice>				m_logicalDevice;
	std::vector<vk::VkPhysicalDevice>	m_physicalDevices;
	de::MovePtr<vk::DeviceDriver>		m_deviceDriver;
};

void ComputeTestInstance::createDeviceGroup (void)
{
	const tcu::CommandLine&							cmdLine					= m_context.getTestContext().getCommandLine();
	const deUint32									devGroupIdx				= cmdLine.getVKDeviceGroupId() - 1;
	const deUint32									physDeviceIdx			= cmdLine.getVKDeviceId() - 1;
	const float										queuePriority			= 1.0f;
	const std::vector<std::string>					requiredExtensions		(1, "VK_KHR_device_group_creation");
	m_deviceGroupInstance													= createCustomInstanceWithExtensions(m_context, requiredExtensions);
	std::vector<VkPhysicalDeviceGroupProperties>	devGroupProperties		= enumeratePhysicalDeviceGroups(m_context.getInstanceInterface(), m_deviceGroupInstance);
	m_numPhysDevices														= devGroupProperties[devGroupIdx].physicalDeviceCount;
	std::vector<const char*>						deviceExtensions;

	if (!isCoreDeviceExtension(m_context.getUsedApiVersion(), "VK_KHR_device_group"))
		deviceExtensions.push_back("VK_KHR_device_group");

	VkDeviceGroupDeviceCreateInfo					deviceGroupInfo			=
	{
		VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO_KHR,								//stype
		DE_NULL,																			//pNext
		devGroupProperties[devGroupIdx].physicalDeviceCount,								//physicalDeviceCount
		devGroupProperties[devGroupIdx].physicalDevices										//physicalDevices
	};
	const InstanceDriver&							instance				(m_deviceGroupInstance.getDriver());
	const VkPhysicalDeviceFeatures					deviceFeatures			= getPhysicalDeviceFeatures(instance, deviceGroupInfo.pPhysicalDevices[physDeviceIdx]);
	const std::vector<VkQueueFamilyProperties>		queueProps				= getPhysicalDeviceQueueFamilyProperties(instance, devGroupProperties[devGroupIdx].physicalDevices[physDeviceIdx]);

	m_physicalDevices.resize(m_numPhysDevices);
	for (deUint32 physDevIdx = 0; physDevIdx < m_numPhysDevices; physDevIdx++)
		m_physicalDevices[physDevIdx] = devGroupProperties[devGroupIdx].physicalDevices[physDevIdx];

	for (size_t queueNdx = 0; queueNdx < queueProps.size(); queueNdx++)
	{
		if (queueProps[queueNdx].queueFlags & VK_QUEUE_COMPUTE_BIT)
			m_queueFamilyIndex = (deUint32)queueNdx;
	}

	VkDeviceQueueCreateInfo							queueInfo				=
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,		// VkStructureType					sType;
		DE_NULL,										// const void*						pNext;
		(VkDeviceQueueCreateFlags)0u,					// VkDeviceQueueCreateFlags			flags;
		m_queueFamilyIndex,								// deUint32							queueFamilyIndex;
		1u,												// deUint32							queueCount;
		&queuePriority									// const float*						pQueuePriorities;
	};

	const VkDeviceCreateInfo						deviceInfo				=
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,							// VkStructureType					sType;
		&deviceGroupInfo,												// const void*						pNext;
		(VkDeviceCreateFlags)0,											// VkDeviceCreateFlags				flags;
		1u	,															// uint32_t							queueCreateInfoCount;
		&queueInfo,														// const VkDeviceQueueCreateInfo*	pQueueCreateInfos;
		0u,																// uint32_t							enabledLayerCount;
		DE_NULL,														// const char* const*				ppEnabledLayerNames;
		deUint32(deviceExtensions.size()),								// uint32_t							enabledExtensionCount;
		(deviceExtensions.empty() ? DE_NULL : &deviceExtensions[0]),	// const char* const*				ppEnabledExtensionNames;
		&deviceFeatures,												// const VkPhysicalDeviceFeatures*	pEnabledFeatures;
	};

	m_logicalDevice		= createCustomDevice(m_context.getTestContext().getCommandLine().isValidationEnabled(), m_context.getPlatformInterface(), m_deviceGroupInstance, instance, deviceGroupInfo.pPhysicalDevices[physDeviceIdx], &deviceInfo);
	m_deviceDriver		= de::MovePtr<DeviceDriver>(new DeviceDriver(m_context.getPlatformInterface(), m_deviceGroupInstance, *m_logicalDevice));
}

class DispatchBaseTest : public vkt::TestCase
{
public:
						DispatchBaseTest	(tcu::TestContext&	testCtx,
											const std::string&	name,
											const std::string&	description,
											const deUint32		numValues,
											const tcu::IVec3&	localsize,
											const tcu::IVec3&	worksize,
											const tcu::IVec3&	splitsize);

	void				initPrograms		(SourceCollections& sourceCollections) const;
	TestInstance*		createInstance		(Context&			context) const;

private:
	const deUint32					m_numValues;
	const tcu::IVec3				m_localSize;
	const tcu::IVec3				m_workSize;
	const tcu::IVec3				m_splitSize;
};

class DispatchBaseTestInstance : public ComputeTestInstance
{
public:
									DispatchBaseTestInstance	(Context&			context,
																const deUint32		numValues,
																const tcu::IVec3&	localsize,
																const tcu::IVec3&	worksize,
																const tcu::IVec3&	splitsize);

	bool							isInputVectorValid			(const tcu::IVec3& small, const tcu::IVec3& big);
	tcu::TestStatus					iterate						(void);

private:
	const deUint32					m_numValues;
	const tcu::IVec3				m_localSize;
	const tcu::IVec3				m_workSize;
	const tcu::IVec3				m_splitWorkSize;
};

DispatchBaseTest::DispatchBaseTest (tcu::TestContext&	testCtx,
									const std::string&	name,
									const std::string&	description,
									const deUint32		numValues,
									const tcu::IVec3&	localsize,
									const tcu::IVec3&	worksize,
									const tcu::IVec3&	splitsize)
	: TestCase		(testCtx, name, description)
	, m_numValues	(numValues)
	, m_localSize	(localsize)
	, m_workSize	(worksize)
	, m_splitSize	(splitsize)
{
}

void DispatchBaseTest::initPrograms (SourceCollections& sourceCollections) const
{
	std::ostringstream src;
	src << "#version 310 es\n"
		<< "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y() << ", local_size_z = " << m_localSize.z() << ") in;\n"

		<< "layout(binding = 0) buffer InOut {\n"
		<< "    uint values[" << de::toString(m_numValues) << "];\n"
		<< "} sb_inout;\n"

		<< "layout(binding = 1) readonly uniform uniformInput {\n"
		<< "    uvec3 gridSize;\n"
		<< "} ubo_in;\n"

		<< "void main (void) {\n"
		<< "    uvec3 size = ubo_in.gridSize * gl_WorkGroupSize;\n"
		<< "    uint numValuesPerInv = uint(sb_inout.values.length()) / (size.x*size.y*size.z);\n"
		<< "    uint index = size.x*size.y*gl_GlobalInvocationID.z + size.x*gl_GlobalInvocationID.y + gl_GlobalInvocationID.x;\n"
		<< "    uint offset = numValuesPerInv*index;\n"
		<< "    for (uint ndx = 0u; ndx < numValuesPerInv; ndx++)\n"
		<< "        sb_inout.values[offset + ndx] = ~sb_inout.values[offset + ndx];\n"
		<< "}\n";

	sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance* DispatchBaseTest::createInstance (Context& context) const
{
	return new DispatchBaseTestInstance(context, m_numValues, m_localSize, m_workSize, m_splitSize);
}

DispatchBaseTestInstance::DispatchBaseTestInstance (Context& context,
													const deUint32		numValues,
													const tcu::IVec3&	localsize,
													const tcu::IVec3&	worksize,
													const tcu::IVec3&	splitsize)

	: ComputeTestInstance	(context)
	, m_numValues			(numValues)
	, m_localSize			(localsize)
	, m_workSize			(worksize)
	, m_splitWorkSize		(splitsize)
{
	// For easy work distribution across physical devices:
	// WorkSize should be a multiple of SplitWorkSize only in the X component
	if ((!isInputVectorValid(m_splitWorkSize, m_workSize)) ||
		(m_workSize.x() <= m_splitWorkSize.x()) ||
		(m_workSize.y() != m_splitWorkSize.y()) ||
		(m_workSize.z() != m_splitWorkSize.z()))
		TCU_THROW(TestError, "Invalid Input.");

	// For easy work distribution within the same physical device:
	// SplitWorkSize should be a multiple of localSize in Y or Z component
	if ((!isInputVectorValid(m_localSize, m_splitWorkSize)) ||
		(m_localSize.x() != m_splitWorkSize.x()) ||
		(m_localSize.y() >= m_splitWorkSize.y()) ||
		(m_localSize.z() >= m_splitWorkSize.z()))
		TCU_THROW(TestError, "Invalid Input.");

	if ((multiplyComponents(m_workSize) / multiplyComponents(m_splitWorkSize)) < (deInt32) m_numPhysDevices)
		TCU_THROW(TestError, "Not enough work to distribute across all physical devices.");

	deUint32 totalWork = multiplyComponents(m_workSize) * multiplyComponents(m_localSize);
	if ((totalWork > numValues) || (numValues % totalWork != 0))
		TCU_THROW(TestError, "Buffer too small/not aligned to cover all values.");
}

bool DispatchBaseTestInstance::isInputVectorValid(const tcu::IVec3& small, const tcu::IVec3& big)
{
	if (((big.x() < small.x()) || (big.y() < small.y()) || (big.z() < small.z())) ||
		((big.x() % small.x() != 0) || (big.y() % small.y() != 0) || (big.z() % small.z() != 0)))
		return false;
	return true;
}

tcu::TestStatus DispatchBaseTestInstance::iterate (void)
{
	const DeviceInterface&	vk					= getDeviceInterface();
	const VkDevice			device				= getDevice();
	const VkQueue			queue				= getDeviceQueue(vk, device, m_queueFamilyIndex, 0);
	SimpleAllocator			allocator			(vk, device, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), getPhysicalDevice()));
	deUint32				totalWorkloadSize	= 0;

	// Create an uniform and input/output buffer
	const deUint32 uniformBufSize = 3; // Pass the compute grid size
	const VkDeviceSize uniformBufferSizeBytes = sizeof(deUint32) * uniformBufSize;
	const Buffer uniformBuffer(vk, device, allocator, makeBufferCreateInfo(uniformBufferSizeBytes, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT), MemoryRequirement::HostVisible);

	const VkDeviceSize bufferSizeBytes = sizeof(deUint32) * m_numValues;
	const Buffer buffer(vk, device, allocator, makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);

	// Fill the buffers with data
	typedef std::vector<deUint32> data_vector_t;
	data_vector_t uniformInputData(uniformBufSize);
	data_vector_t inputData(m_numValues);

	{
		const Allocation& bufferAllocation = uniformBuffer.getAllocation();
		deUint32* bufferPtr = static_cast<deUint32*>(bufferAllocation.getHostPtr());
		uniformInputData[0] = *bufferPtr++ = m_workSize.x();
		uniformInputData[1] = *bufferPtr++ = m_workSize.y();
		uniformInputData[2] = *bufferPtr++ = m_workSize.z();
		flushAlloc(vk, device, bufferAllocation);
	}

	{
		de::Random rnd(0x82ce7f);
		const Allocation& bufferAllocation = buffer.getAllocation();
		deUint32* bufferPtr = static_cast<deUint32*>(bufferAllocation.getHostPtr());
		for (deUint32 i = 0; i < m_numValues; ++i)
			inputData[i] = *bufferPtr++ = rnd.getUint32();

		flushAlloc(vk, device, bufferAllocation);
	}

	// Create descriptor set
	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	const VkDescriptorBufferInfo bufferDescriptorInfo = makeDescriptorBufferInfo(*buffer, 0ull, bufferSizeBytes);
	const VkDescriptorBufferInfo uniformBufferDescriptorInfo = makeDescriptorBufferInfo(*uniformBuffer, 0ull, uniformBufferSizeBytes);

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBufferDescriptorInfo)
		.update(vk, device);

	const Unique<VkShaderModule> shaderModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));
	const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *pipelineLayout, static_cast<VkPipelineCreateFlags>(VK_PIPELINE_CREATE_DISPATCH_BASE), *shaderModule, static_cast<VkPipelineShaderStageCreateFlags>(0u)));

	const VkBufferMemoryBarrier hostWriteBarrier = makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *buffer, 0ull, bufferSizeBytes);
	const VkBufferMemoryBarrier hostUniformWriteBarrier = makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT, *uniformBuffer, 0ull, uniformBufferSizeBytes);

	const VkBufferMemoryBarrier shaderWriteBarrier = makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer, 0ull, bufferSizeBytes);

	const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, m_queueFamilyIndex));
	const Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Start recording commands
	beginCommandBuffer(vk, *cmdBuffer);

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
	vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &hostUniformWriteBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &hostWriteBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

	// Split the workload across all physical devices based on m_splitWorkSize.x()
	for (deUint32 physDevIdx = 0; physDevIdx < m_numPhysDevices; physDevIdx++)
	{
		deUint32 baseGroupX = physDevIdx * m_splitWorkSize.x();
		deUint32 baseGroupY = 0;
		deUint32 baseGroupZ = 0;

		// Split the workload within the physical device based on m_localSize.y() and m_localSize.z()
		for (deInt32 localIdxY = 0; localIdxY < (m_splitWorkSize.y() / m_localSize.y()); localIdxY++)
		{
			for (deInt32 localIdxZ = 0; localIdxZ < (m_splitWorkSize.z() / m_localSize.z()); localIdxZ++)
			{
				deUint32 offsetX = baseGroupX;
				deUint32 offsetY = baseGroupY + localIdxY * m_localSize.y();
				deUint32 offsetZ = baseGroupZ + localIdxZ * m_localSize.z();

				deUint32 localSizeX = (physDevIdx == (m_numPhysDevices - 1)) ? m_workSize.x() - baseGroupX : m_localSize.x();
				deUint32 localSizeY = m_localSize.y();
				deUint32 localSizeZ = m_localSize.z();

				totalWorkloadSize += (localSizeX * localSizeY * localSizeZ);
				vk.cmdDispatchBase(*cmdBuffer, offsetX, offsetY, offsetZ, localSizeX, localSizeY, localSizeZ);
			}
		}
	}

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &shaderWriteBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

	endCommandBuffer(vk, *cmdBuffer);
	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	if (totalWorkloadSize != deUint32(multiplyComponents(m_workSize)))
		TCU_THROW(TestError, "Not covering the entire workload.");

	// Validate the results
	const Allocation& bufferAllocation = buffer.getAllocation();
	invalidateAlloc(vk, device, bufferAllocation);
	const deUint32* bufferPtr = static_cast<deUint32*>(bufferAllocation.getHostPtr());

	for (deUint32 ndx = 0; ndx < m_numValues; ++ndx)
	{
		const deUint32 res = bufferPtr[ndx];
		const deUint32 ref = ~inputData[ndx];

		if (res != ref)
		{
			std::ostringstream msg;
			msg << "Comparison failed for InOut.values[" << ndx << "]";
			return tcu::TestStatus::fail(msg.str());
		}
	}
	return tcu::TestStatus::pass("Compute succeeded");
}

class DeviceIndexTest : public vkt::TestCase
{
public:
	DeviceIndexTest		(tcu::TestContext&	testCtx,
											const std::string&	name,
											const std::string&	description,
											const deUint32		numValues,
											const tcu::IVec3&	localsize,
											const tcu::IVec3&	splitsize);

	void				initPrograms		(SourceCollections& sourceCollections) const;
	TestInstance*		createInstance		(Context&			context) const;

private:
	const deUint32					m_numValues;
	const tcu::IVec3				m_localSize;
	const tcu::IVec3				m_workSize;
	const tcu::IVec3				m_splitSize;
};

class DeviceIndexTestInstance : public ComputeTestInstance
{
public:
									DeviceIndexTestInstance	(Context&			context,
																const deUint32		numValues,
																const tcu::IVec3&	localsize,
																const tcu::IVec3&	worksize);
	tcu::TestStatus					iterate						(void);
private:
	const deUint32					m_numValues;
	const tcu::IVec3				m_localSize;
	tcu::IVec3						m_workSize;
};

DeviceIndexTest::DeviceIndexTest (tcu::TestContext&	testCtx,
									const std::string&	name,
									const std::string&	description,
									const deUint32		numValues,
									const tcu::IVec3&	localsize,
									const tcu::IVec3&	worksize)
	: TestCase		(testCtx, name, description)
	, m_numValues	(numValues)
	, m_localSize	(localsize)
	, m_workSize	(worksize)
{
}

void DeviceIndexTest::initPrograms (SourceCollections& sourceCollections) const
{
	std::ostringstream src;
	src << "#version 310 es\n"
		<< "#extension GL_EXT_device_group : require\n"
		<< "layout (local_size_x = " << m_localSize.x() << ", local_size_y = " << m_localSize.y() << ", local_size_z = " << m_localSize.z() << ") in;\n"

		<< "layout(binding = 0) buffer InOut {\n"
		<< "    uint values[" << de::toString(m_numValues) << "];\n"
		<< "} sb_inout;\n"

		<< "layout(binding = 1) readonly uniform uniformInput {\n"
		<< "    uint baseOffset[1+" << VK_MAX_DEVICE_GROUP_SIZE_KHR << "];\n"
		<< "} ubo_in;\n"

		<< "void main (void) {\n"
		<< "    uvec3 size = gl_NumWorkGroups * gl_WorkGroupSize;\n"
		<< "    uint numValuesPerInv = uint(sb_inout.values.length()) / (size.x*size.y*size.z);\n"
		<< "    uint index = size.x*size.y*gl_GlobalInvocationID.z + size.x*gl_GlobalInvocationID.y + gl_GlobalInvocationID.x;\n"
		<< "    uint offset = numValuesPerInv*index;\n"
		<< "    for (uint ndx = 0u; ndx < numValuesPerInv; ndx++)\n"
		<< "        sb_inout.values[offset + ndx] = ubo_in.baseOffset[0] + ubo_in.baseOffset[gl_DeviceIndex + 1];\n"
		<< "}\n";

	sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance* DeviceIndexTest::createInstance (Context& context) const
{
	return new DeviceIndexTestInstance(context, m_numValues, m_localSize, m_workSize);
}

DeviceIndexTestInstance::DeviceIndexTestInstance (Context& context,
													const deUint32		numValues,
													const tcu::IVec3&	localsize,
													const tcu::IVec3&	worksize)

	: ComputeTestInstance	(context)
	, m_numValues			(numValues)
	, m_localSize			(localsize)
	, m_workSize			(worksize)
{}

tcu::TestStatus DeviceIndexTestInstance::iterate (void)
{
	const DeviceInterface&			vk					= getDeviceInterface();
	const VkDevice					device				= getDevice();
	const VkQueue					queue				= getDeviceQueue(vk, device, m_queueFamilyIndex, 0);
	SimpleAllocator					allocator			(vk, device, getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), getPhysicalDevice()));
	const deUint32					allocDeviceMask		= (1 << m_numPhysDevices) - 1;
	de::Random						rnd					(0x82ce7f);
	Move<VkBuffer>					sboBuffer;
	vk::Move<vk::VkDeviceMemory>	sboBufferMemory;

	// Create an uniform and output buffer
	const deUint32 uniformBufSize = 4 * (1 + VK_MAX_DEVICE_GROUP_SIZE_KHR);
	const VkDeviceSize uniformBufferSizeBytes = sizeof(deUint32) * uniformBufSize;
	const Buffer uniformBuffer(vk, device, allocator, makeBufferCreateInfo(uniformBufferSizeBytes, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT), MemoryRequirement::HostVisible);

	const VkDeviceSize bufferSizeBytes = sizeof(deUint32) * m_numValues;
	const Buffer checkBuffer(vk, device, allocator, makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible);

	// create SBO buffer
	{
		const VkBufferCreateInfo	sboBufferParams =
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,									// sType
			DE_NULL,																// pNext
			0u,																		// flags
			(VkDeviceSize)bufferSizeBytes,											// size
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,	// usage
			VK_SHARING_MODE_EXCLUSIVE,												// sharingMode
			1u,																		// queueFamilyIndexCount
			&m_queueFamilyIndex,														// pQueueFamilyIndices
		};
		sboBuffer = createBuffer(vk, device, &sboBufferParams);

		VkMemoryRequirements memReqs = getBufferMemoryRequirements(vk, device, sboBuffer.get());
		deUint32 memoryTypeNdx = 0;
		const VkPhysicalDeviceMemoryProperties deviceMemProps = getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), getPhysicalDevice());
		for ( memoryTypeNdx = 0; memoryTypeNdx < deviceMemProps.memoryTypeCount; memoryTypeNdx++)
		{
			if ((memReqs.memoryTypeBits & (1u << memoryTypeNdx)) != 0 &&
				(deviceMemProps.memoryTypes[memoryTypeNdx].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
				break;
		}
		if (memoryTypeNdx == deviceMemProps.memoryTypeCount)
			TCU_THROW(NotSupportedError, "No compatible memory type found");

		const VkMemoryAllocateFlagsInfo allocDeviceMaskInfo =
		{
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR,	// sType
			DE_NULL,											// pNext
			VK_MEMORY_ALLOCATE_DEVICE_MASK_BIT,					// flags
			allocDeviceMask,									// deviceMask
		};

		VkMemoryAllocateInfo		allocInfo =
		{
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,			// sType
			&allocDeviceMaskInfo,							// pNext
			memReqs.size,									// allocationSize
			memoryTypeNdx,									// memoryTypeIndex
		};

		sboBufferMemory = allocateMemory(vk, device, &allocInfo);
		VK_CHECK(vk.bindBufferMemory(device, *sboBuffer, sboBufferMemory.get(), 0));
	}

	// Fill the buffers with data
	typedef std::vector<deUint32> data_vector_t;
	data_vector_t uniformInputData(uniformBufSize, 0);

	{
		const Allocation& bufferAllocation = uniformBuffer.getAllocation();
		deUint32* bufferPtr = static_cast<deUint32*>(bufferAllocation.getHostPtr());
		for (deUint32 i = 0; i < uniformBufSize; ++i)
			uniformInputData[i] = *bufferPtr++ = rnd.getUint32() / 10; // divide to prevent overflow in addition

		flushAlloc(vk, device, bufferAllocation);
	}

	// Create descriptor set
	const Unique<VkDescriptorSetLayout> descriptorSetLayout(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	const VkDescriptorBufferInfo bufferDescriptorInfo = makeDescriptorBufferInfo(*sboBuffer, 0ull, bufferSizeBytes);
	const VkDescriptorBufferInfo uniformBufferDescriptorInfo = makeDescriptorBufferInfo(*uniformBuffer, 0ull, uniformBufferSizeBytes);

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformBufferDescriptorInfo)
		.update(vk, device);

	const Unique<VkShaderModule> shaderModule(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));
	const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

	const VkBufferMemoryBarrier hostUniformWriteBarrier = makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT, *uniformBuffer, 0ull, uniformBufferSizeBytes);
	const VkBufferMemoryBarrier shaderWriteBarrier = makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT , *sboBuffer, 0ull, bufferSizeBytes);

	const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, m_queueFamilyIndex));
	const Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Verify multiple device masks
	for (deUint32 physDevMask = 1; physDevMask < (1u << m_numPhysDevices); physDevMask++)
	{
		deUint32 constantValPerLoop = 0;
		{
			const Allocation& bufferAllocation = uniformBuffer.getAllocation();
			deUint32* bufferPtr = static_cast<deUint32*>(bufferAllocation.getHostPtr());
			constantValPerLoop = *bufferPtr = rnd.getUint32() / 10;  // divide to prevent overflow in addition
			flushAlloc(vk, device, bufferAllocation);
		}
		beginCommandBuffer(vk, *cmdBuffer);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);
		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &hostUniformWriteBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

		vk.cmdSetDeviceMask(*cmdBuffer, physDevMask);
		vk.cmdDispatch(*cmdBuffer, m_workSize.x(), m_workSize.y(), m_workSize.z());

		vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &shaderWriteBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

		endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(vk, device, queue, *cmdBuffer, true, physDevMask);

		// Validate the results on all physical devices where compute shader was launched
		const VkBufferMemoryBarrier srcBufferBarrier = makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT , *sboBuffer, 0ull, bufferSizeBytes);
		const VkBufferMemoryBarrier dstBufferBarrier = makeBufferMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *checkBuffer, 0ull, bufferSizeBytes);
		const VkBufferCopy	copyParams =
		{
			(VkDeviceSize)0u,						// srcOffset
			(VkDeviceSize)0u,						// dstOffset
			bufferSizeBytes							// size
		};

		for (deUint32 physDevIdx = 0; physDevIdx < m_numPhysDevices; physDevIdx++)
		{
			if (!(1<<physDevIdx & physDevMask))
				continue;

			const deUint32 deviceMask = 1 << physDevIdx;

			beginCommandBuffer(vk, *cmdBuffer);
			vk.cmdSetDeviceMask(*cmdBuffer, deviceMask);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT , VK_PIPELINE_STAGE_TRANSFER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &srcBufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);
			vk.cmdCopyBuffer(*cmdBuffer, *sboBuffer, *checkBuffer, 1, &copyParams);
			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &dstBufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

			endCommandBuffer(vk, *cmdBuffer);
			submitCommandsAndWait(vk, device, queue, *cmdBuffer, true, deviceMask);

			const Allocation& bufferAllocation = checkBuffer.getAllocation();
			invalidateAlloc(vk, device, bufferAllocation);
			const deUint32* bufferPtr = static_cast<deUint32*>(bufferAllocation.getHostPtr());

			for (deUint32 ndx = 0; ndx < m_numValues; ++ndx)
			{
				const deUint32 res = bufferPtr[ndx];
				const deUint32 ref = constantValPerLoop + uniformInputData[4 * (physDevIdx + 1)];

				if (res != ref)
				{
					std::ostringstream msg;
					msg << "Comparison failed on physical device "<< getPhysicalDevice(physDevIdx) <<" ( deviceMask "<< deviceMask <<" ) for InOut.values[" << ndx << "]";
					return tcu::TestStatus::fail(msg.str());
				}
			}
		}
	}

	return tcu::TestStatus::pass("Compute succeeded");
}

class ConcurrentCompute : public vkt::TestCase
{
public:
						ConcurrentCompute	(tcu::TestContext&	testCtx,
											 const std::string&	name,
											 const std::string&	description);


	void				initPrograms		(SourceCollections& sourceCollections) const;
	TestInstance*		createInstance		(Context&			context) const;
};

class ConcurrentComputeInstance : public vkt::TestInstance
{
public:
									ConcurrentComputeInstance	(Context& context);

	tcu::TestStatus					iterate						(void);
};

ConcurrentCompute::ConcurrentCompute (tcu::TestContext&	testCtx,
									  const std::string&	name,
									  const std::string&	description)
	: TestCase		(testCtx, name, description)
{
}

void ConcurrentCompute::initPrograms (SourceCollections& sourceCollections) const
{
	std::ostringstream src;
	src << "#version 310 es\n"
		<< "layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
		<< "layout(binding = 0) buffer InOut {\n"
		<< "    uint values[1024];\n"
		<< "} sb_inout;\n"
		<< "void main (void) {\n"
		<< "    uvec3 size           = gl_NumWorkGroups * gl_WorkGroupSize;\n"
		<< "    uint numValuesPerInv = uint(sb_inout.values.length()) / (size.x*size.y*size.z);\n"
		<< "    uint groupNdx        = size.x*size.y*gl_GlobalInvocationID.z + size.x*gl_GlobalInvocationID.y + gl_GlobalInvocationID.x;\n"
		<< "    uint offset          = numValuesPerInv*groupNdx;\n"
		<< "\n"
		<< "    for (uint ndx = 0u; ndx < numValuesPerInv; ndx++)\n"
		<< "        sb_inout.values[offset + ndx] = ~sb_inout.values[offset + ndx];\n"
		<< "}\n";

	sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

TestInstance* ConcurrentCompute::createInstance (Context& context) const
{
	return new ConcurrentComputeInstance(context);
}

ConcurrentComputeInstance::ConcurrentComputeInstance (Context& context)
	: TestInstance	(context)
{
}

tcu::TestStatus ConcurrentComputeInstance::iterate (void)
{
	enum {
		NO_MATCH_FOUND	= ~((deUint32)0),
		ERROR_NONE		= 0,
		ERROR_WAIT		= 1,
		ERROR_ORDER		= 2
	};

	struct Queues
	{
		VkQueue		queue;
		deUint32	queueFamilyIndex;
	};

	const DeviceInterface&					vk							= m_context.getDeviceInterface();
	const deUint32							numValues					= 1024;
	const InstanceInterface&				instance					= m_context.getInstanceInterface();
	const VkPhysicalDevice					physicalDevice				= m_context.getPhysicalDevice();
	tcu::TestLog&							log							= m_context.getTestContext().getLog();
	vk::Move<vk::VkDevice>					logicalDevice;
	std::vector<VkQueueFamilyProperties>	queueFamilyProperties;
	VkDeviceCreateInfo						deviceInfo;
	VkPhysicalDeviceFeatures				deviceFeatures;
	const float								queuePriorities[2]			= {1.0f, 0.0f};
	VkDeviceQueueCreateInfo					queueInfos[2];
	Queues									queues[2]					=
																		{
																			{DE_NULL, (deUint32)NO_MATCH_FOUND},
																			{DE_NULL, (deUint32)NO_MATCH_FOUND}
																		};

	queueFamilyProperties = getPhysicalDeviceQueueFamilyProperties(instance, physicalDevice);

	for (deUint32 queueNdx = 0; queueNdx < queueFamilyProperties.size(); ++queueNdx)
	{
		if (queueFamilyProperties[queueNdx].queueFlags & VK_QUEUE_COMPUTE_BIT)
		{
			if (NO_MATCH_FOUND == queues[0].queueFamilyIndex)
				queues[0].queueFamilyIndex = queueNdx;

			if (queues[0].queueFamilyIndex != queueNdx || queueFamilyProperties[queueNdx].queueCount > 1u)
			{
				queues[1].queueFamilyIndex = queueNdx;
				break;
			}
		}
	}

	if (queues[0].queueFamilyIndex == NO_MATCH_FOUND || queues[1].queueFamilyIndex == NO_MATCH_FOUND)
		TCU_THROW(NotSupportedError, "Queues couldn't be created");

	for (int queueNdx = 0; queueNdx < 2; ++queueNdx)
	{
		VkDeviceQueueCreateInfo queueInfo;
		deMemset(&queueInfo, 0, sizeof(queueInfo));

		queueInfo.sType				= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.pNext				= DE_NULL;
		queueInfo.flags				= (VkDeviceQueueCreateFlags)0u;
		queueInfo.queueFamilyIndex	= queues[queueNdx].queueFamilyIndex;
		queueInfo.queueCount		= (queues[0].queueFamilyIndex == queues[1].queueFamilyIndex) ? 2 : 1;
		queueInfo.pQueuePriorities	= (queueInfo.queueCount == 2) ? queuePriorities : &queuePriorities[queueNdx];

		queueInfos[queueNdx]		= queueInfo;

		if (queues[0].queueFamilyIndex == queues[1].queueFamilyIndex)
			break;
	}
	deMemset(&deviceInfo, 0, sizeof(deviceInfo));
	instance.getPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

	deviceInfo.sType					= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pNext					= DE_NULL;
	deviceInfo.enabledExtensionCount	= 0u;
	deviceInfo.ppEnabledExtensionNames	= DE_NULL;
	deviceInfo.enabledLayerCount		= 0u;
	deviceInfo.ppEnabledLayerNames		= DE_NULL;
	deviceInfo.pEnabledFeatures			= &deviceFeatures;
	deviceInfo.queueCreateInfoCount		= (queues[0].queueFamilyIndex == queues[1].queueFamilyIndex) ? 1 : 2;
	deviceInfo.pQueueCreateInfos		= queueInfos;

	logicalDevice = createCustomDevice(m_context.getTestContext().getCommandLine().isValidationEnabled(), m_context.getPlatformInterface(), m_context.getInstance(), instance, physicalDevice, &deviceInfo);

	for (deUint32 queueReqNdx = 0; queueReqNdx < 2; ++queueReqNdx)
	{
		if (queues[0].queueFamilyIndex == queues[1].queueFamilyIndex)
			vk.getDeviceQueue(*logicalDevice, queues[queueReqNdx].queueFamilyIndex, queueReqNdx, &queues[queueReqNdx].queue);
		else
			vk.getDeviceQueue(*logicalDevice, queues[queueReqNdx].queueFamilyIndex, 0u, &queues[queueReqNdx].queue);
	}

	// Create an input/output buffers
	const VkPhysicalDeviceMemoryProperties memoryProperties	= vk::getPhysicalDeviceMemoryProperties(instance, physicalDevice);

	SimpleAllocator *allocator								= new SimpleAllocator(vk, *logicalDevice, memoryProperties);
	const VkDeviceSize bufferSizeBytes						= sizeof(deUint32) * numValues;
	const Buffer buffer1(vk, *logicalDevice, *allocator, makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);
	const Buffer buffer2(vk, *logicalDevice, *allocator, makeBufferCreateInfo(bufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);

	// Fill the buffers with data

	typedef std::vector<deUint32> data_vector_t;
	data_vector_t inputData(numValues);

	{
		de::Random rnd(0x82ce7f);
		const Allocation& bufferAllocation1	= buffer1.getAllocation();
		const Allocation& bufferAllocation2	= buffer2.getAllocation();
		deUint32* bufferPtr1				= static_cast<deUint32*>(bufferAllocation1.getHostPtr());
		deUint32* bufferPtr2				= static_cast<deUint32*>(bufferAllocation2.getHostPtr());

		for (deUint32 i = 0; i < numValues; ++i)
		{
			deUint32 val = rnd.getUint32();
			inputData[i] = val;
			*bufferPtr1++ = val;
			*bufferPtr2++ = val;
		}

		flushAlloc(vk, *logicalDevice, bufferAllocation1);
		flushAlloc(vk, *logicalDevice, bufferAllocation2);
	}

	// Create descriptor sets

	const Unique<VkDescriptorSetLayout>	descriptorSetLayout1(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, *logicalDevice));

	const Unique<VkDescriptorPool>		descriptorPool1(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, *logicalDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet>		descriptorSet1(makeDescriptorSet(vk, *logicalDevice, *descriptorPool1, *descriptorSetLayout1));

	const VkDescriptorBufferInfo		bufferDescriptorInfo1	= makeDescriptorBufferInfo(*buffer1, 0ull, bufferSizeBytes);
		DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet1, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo1)
		.update(vk, *logicalDevice);

	const Unique<VkDescriptorSetLayout>	descriptorSetLayout2(
		DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, *logicalDevice));

	const Unique<VkDescriptorPool>		descriptorPool2(
		DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, *logicalDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet>		descriptorSet2(makeDescriptorSet(vk, *logicalDevice, *descriptorPool2, *descriptorSetLayout2));

	const VkDescriptorBufferInfo		bufferDescriptorInfo2	= makeDescriptorBufferInfo(*buffer2, 0ull, bufferSizeBytes);
		DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet2, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptorInfo2)
		.update(vk, *logicalDevice);

	// Perform the computation

	const Unique<VkShaderModule>		shaderModule(createShaderModule(vk, *logicalDevice, m_context.getBinaryCollection().get("comp"), 0u));

	const Unique<VkPipelineLayout>		pipelineLayout1(makePipelineLayout(vk, *logicalDevice, *descriptorSetLayout1));
	const Unique<VkPipeline>			pipeline1(makeComputePipeline(vk, *logicalDevice, *pipelineLayout1, *shaderModule));
	const VkBufferMemoryBarrier			hostWriteBarrier1		= makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *buffer1, 0ull, bufferSizeBytes);
	const VkBufferMemoryBarrier			shaderWriteBarrier1		= makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer1, 0ull, bufferSizeBytes);
	const Unique<VkCommandPool>			cmdPool1(makeCommandPool(vk, *logicalDevice, queues[0].queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer1(allocateCommandBuffer(vk, *logicalDevice, *cmdPool1, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const Unique<VkPipelineLayout>		pipelineLayout2(makePipelineLayout(vk, *logicalDevice, *descriptorSetLayout2));
	const Unique<VkPipeline>			pipeline2(makeComputePipeline(vk, *logicalDevice, *pipelineLayout2, *shaderModule));
	const VkBufferMemoryBarrier			hostWriteBarrier2		= makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, *buffer2, 0ull, bufferSizeBytes);
	const VkBufferMemoryBarrier			shaderWriteBarrier2		= makeBufferMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *buffer2, 0ull, bufferSizeBytes);
	const Unique<VkCommandPool>			cmdPool2(makeCommandPool(vk, *logicalDevice, queues[1].queueFamilyIndex));
	const Unique<VkCommandBuffer>		cmdBuffer2(allocateCommandBuffer(vk, *logicalDevice, *cmdPool2, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Command buffer 1

	beginCommandBuffer(vk, *cmdBuffer1);
	vk.cmdBindPipeline(*cmdBuffer1, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline1);
	vk.cmdBindDescriptorSets(*cmdBuffer1, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout1, 0u, 1u, &descriptorSet1.get(), 0u, DE_NULL);
	vk.cmdPipelineBarrier(*cmdBuffer1, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &hostWriteBarrier1, 0, (const VkImageMemoryBarrier*)DE_NULL);
	vk.cmdDispatch(*cmdBuffer1, 1, 1, 1);
	vk.cmdPipelineBarrier(*cmdBuffer1, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &shaderWriteBarrier1, 0, (const VkImageMemoryBarrier*)DE_NULL);
	endCommandBuffer(vk, *cmdBuffer1);

	// Command buffer 2

	beginCommandBuffer(vk, *cmdBuffer2);
	vk.cmdBindPipeline(*cmdBuffer2, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline2);
	vk.cmdBindDescriptorSets(*cmdBuffer2, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout2, 0u, 1u, &descriptorSet2.get(), 0u, DE_NULL);
	vk.cmdPipelineBarrier(*cmdBuffer2, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &hostWriteBarrier2, 0, (const VkImageMemoryBarrier*)DE_NULL);
	vk.cmdDispatch(*cmdBuffer2, 1, 1, 1);
	vk.cmdPipelineBarrier(*cmdBuffer2, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &shaderWriteBarrier2, 0, (const VkImageMemoryBarrier*)DE_NULL);
	endCommandBuffer(vk, *cmdBuffer2);

	VkSubmitInfo	submitInfo1 =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,			// sType
		DE_NULL,								// pNext
		0u,										// waitSemaphoreCount
		DE_NULL,								// pWaitSemaphores
		(const VkPipelineStageFlags*)DE_NULL,	// pWaitDstStageMask
		1u,										// commandBufferCount
		&cmdBuffer1.get(),						// pCommandBuffers
		0u,										// signalSemaphoreCount
		DE_NULL									// pSignalSemaphores
	};

	VkSubmitInfo	submitInfo2 =
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,			// sType
		DE_NULL,								// pNext
		0u,										// waitSemaphoreCount
		DE_NULL,								// pWaitSemaphores
		(const VkPipelineStageFlags*)DE_NULL,	// pWaitDstStageMask
		1u,										// commandBufferCount
		&cmdBuffer2.get(),						// pCommandBuffers
		0u,										// signalSemaphoreCount
		DE_NULL									// pSignalSemaphores
	};

	// Wait for completion
	const Unique<VkFence>	fence1(createFence(vk, *logicalDevice));
	const Unique<VkFence>	fence2(createFence(vk, *logicalDevice));

	VK_CHECK(vk.queueSubmit(queues[0].queue, 1u, &submitInfo1, *fence1));
	VK_CHECK(vk.queueSubmit(queues[1].queue, 1u, &submitInfo2, *fence2));

	int err = ERROR_NONE;

	// First wait for the low-priority queue
	if (VK_SUCCESS != vk.waitForFences(*logicalDevice, 1u, &fence2.get(), DE_TRUE, ~0ull))
		err = ERROR_WAIT;

	// If the high-priority queue hasn't finished, we have a problem.
	if (VK_SUCCESS != vk.getFenceStatus(*logicalDevice, fence1.get()))
		if (err == ERROR_NONE)
			err = ERROR_ORDER;

	// Wait for the high-priority fence so we don't get errors on teardown.
	vk.waitForFences(*logicalDevice, 1u, &fence1.get(), DE_TRUE, ~0ull);

	// If we fail() before waiting for all of the fences, error will come from
	// teardown instead of the error we want.

	if (err == ERROR_WAIT)
		return tcu::TestStatus::fail("Failed waiting for low-priority queue fence.");

	// Validate the results

	const Allocation& bufferAllocation1	= buffer1.getAllocation();
	invalidateAlloc(vk, *logicalDevice, bufferAllocation1);
	const deUint32* bufferPtr1			= static_cast<deUint32*>(bufferAllocation1.getHostPtr());

	const Allocation& bufferAllocation2	= buffer2.getAllocation();
	invalidateAlloc(vk, *logicalDevice, bufferAllocation2);
	const deUint32* bufferPtr2			= static_cast<deUint32*>(bufferAllocation2.getHostPtr());

	for (deUint32 ndx = 0; ndx < numValues; ++ndx)
	{
		const deUint32 res1	= bufferPtr1[ndx];
		const deUint32 res2	= bufferPtr2[ndx];
		const deUint32 inp	= inputData[ndx];
		const deUint32 ref	= ~inp;

		if (res1 != ref || res1 != res2)
		{
			std::ostringstream msg;
			msg << "Comparison failed for InOut.values[" << ndx << "] ref:" << ref <<" res1:" << res1 << " res2:" << res2 << " inp:" << inp;
			return tcu::TestStatus::fail(msg.str());
		}
	}

	if (err == ERROR_ORDER)
		log << tcu::TestLog::Message << "Note: Low-priority queue was faster than high-priority one. This is not an error, but priorities may be inverted." << tcu::TestLog::EndMessage;

	return tcu::TestStatus::pass("Test passed");
}

class EmptyWorkGroupCase : public vkt::TestCase
{
public:
					EmptyWorkGroupCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const tcu::UVec3& dispatchSize);
	virtual			~EmptyWorkGroupCase		(void) {}

	TestInstance*	createInstance			(Context& context) const override;
	void			initPrograms			(vk::SourceCollections& programCollection) const override;

protected:
	const tcu::UVec3 m_dispatchSize;
};

class EmptyWorkGroupInstance : public vkt::TestInstance
{
public:
						EmptyWorkGroupInstance	(Context& context, const tcu::UVec3& dispatchSize)
							: vkt::TestInstance	(context)
							, m_dispatchSize	(dispatchSize)
							{}
	virtual				~EmptyWorkGroupInstance	(void) {}

	tcu::TestStatus		iterate					(void) override;

protected:
	const tcu::UVec3 m_dispatchSize;
};

EmptyWorkGroupCase::EmptyWorkGroupCase (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const tcu::UVec3& dispatchSize)
	: vkt::TestCase		(testCtx, name, description)
	, m_dispatchSize	(dispatchSize)
{
	DE_ASSERT(m_dispatchSize.x() == 0u || m_dispatchSize.y() == 0u || m_dispatchSize.z() == 0u);
}

TestInstance* EmptyWorkGroupCase::createInstance (Context& context) const
{
	return new EmptyWorkGroupInstance(context, m_dispatchSize);
}

void EmptyWorkGroupCase::initPrograms (vk::SourceCollections& programCollection) const
{
	std::ostringstream comp;
	comp
		<< "#version 450\n"
		<< "layout (local_size_x=1, local_size_y=1, local_size_z=1) in;\n"
		<< "layout (set=0, binding=0) buffer VerificationBlock { uint value; } verif;\n"
		<< "void main () { atomicAdd(verif.value, 1u); }\n"
		;
	programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

tcu::TestStatus EmptyWorkGroupInstance::iterate (void)
{
	const auto&		vkd				= m_context.getDeviceInterface();
	const auto		device			= m_context.getDevice();
	auto&			alloc			= m_context.getDefaultAllocator();
	const auto		queueIndex		= m_context.getUniversalQueueFamilyIndex();
	const auto		queue			= m_context.getUniversalQueue();

	const auto			verifBufferSize		= static_cast<VkDeviceSize>(sizeof(uint32_t));
	const auto			verifBufferInfo		= makeBufferCreateInfo(verifBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	BufferWithMemory	verifBuffer			(vkd, device, alloc, verifBufferInfo, MemoryRequirement::HostVisible);
	auto&				verifBufferAlloc	= verifBuffer.getAllocation();
	void*				verifBufferPtr		= verifBufferAlloc.getHostPtr();

	deMemset(verifBufferPtr, 0, static_cast<size_t>(verifBufferSize));
	flushAlloc(vkd, device, verifBufferAlloc);

	DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	const auto descriptorSetLayout = layoutBuilder.build(vkd, device);

	const auto pipelineLayout	= makePipelineLayout(vkd, device, descriptorSetLayout.get());
	const auto shaderModule		= createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0u);
	const auto pipeline			= makeComputePipeline(vkd, device, pipelineLayout.get(), shaderModule.get());

	DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	const auto descriptorPool	= poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const auto descriptorSet	= makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());

	DescriptorSetUpdateBuilder updateBuilder;
	const auto verifBufferDescInfo = makeDescriptorBufferInfo(verifBuffer.get(), 0ull, verifBufferSize);
	updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &verifBufferDescInfo);
	updateBuilder.update(vkd, device);

	const auto cmdPool = makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer = cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdDispatch(cmdBuffer, m_dispatchSize.x(), m_dispatchSize.y(), m_dispatchSize.z());

	const auto readWriteAccess	= (VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT);
	const auto computeToCompute = makeMemoryBarrier(readWriteAccess, readWriteAccess);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0U, 1u, &computeToCompute, 0u, nullptr, 0u, nullptr);

	vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);

	const auto computeToHost = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &computeToHost, 0u, nullptr, 0u, nullptr);

	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	uint32_t value;
	invalidateAlloc(vkd, device, verifBufferAlloc);
	deMemcpy(&value, verifBufferPtr, sizeof(value));

	if (value != 1u)
	{
		std::ostringstream msg;
		msg << "Unexpected value found in buffer: " << value << " while expecting 1";
		TCU_FAIL(msg.str());
	}

	return tcu::TestStatus::pass("Pass");
}

class MaxWorkGroupSizeTest : public vkt::TestCase
{
public:
	enum class Axis	{ X = 0, Y = 1, Z = 2 };

	struct Params
	{
		// Which axis to maximize.
		Axis axis;
	};

							MaxWorkGroupSizeTest	(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const Params& params);
	virtual					~MaxWorkGroupSizeTest	(void) {}

	virtual void			initPrograms			(vk::SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance			(Context& context) const;
	virtual void			checkSupport			(Context& context) const;

	// Helper to transform the axis value to an index.
	static int				getIndex				(Axis axis);

	// Helper returning the number of invocations according to the test parameters.
	static deUint32			getInvocations			(const Params& params, const vk::InstanceInterface& vki, vk::VkPhysicalDevice physicalDevice, const vk::VkPhysicalDeviceProperties* devProperties = nullptr);

	// Helper returning the buffer size needed to this test.
	static deUint32			getSSBOSize				(deUint32 invocations);

private:
	Params m_params;
};

class MaxWorkGroupSizeInstance : public vkt::TestInstance
{
public:
								MaxWorkGroupSizeInstance	(Context& context, const MaxWorkGroupSizeTest::Params& params);
	virtual						~MaxWorkGroupSizeInstance	(void) {}

	virtual tcu::TestStatus		iterate			(void);

private:
	MaxWorkGroupSizeTest::Params m_params;
};

int MaxWorkGroupSizeTest::getIndex (Axis axis)
{
	const int ret = static_cast<int>(axis);
	DE_ASSERT(ret >= static_cast<int>(Axis::X) && ret <= static_cast<int>(Axis::Z));
	return ret;
}

deUint32 MaxWorkGroupSizeTest::getInvocations (const Params& params, const vk::InstanceInterface& vki, vk::VkPhysicalDevice physicalDevice, const vk::VkPhysicalDeviceProperties* devProperties)
{
	const auto axis = getIndex(params.axis);

	if (devProperties)
		return devProperties->limits.maxComputeWorkGroupSize[axis];
	return vk::getPhysicalDeviceProperties(vki, physicalDevice).limits.maxComputeWorkGroupSize[axis];
}

deUint32 MaxWorkGroupSizeTest::getSSBOSize (deUint32 invocations)
{
	return invocations * static_cast<deUint32>(sizeof(deUint32));
}

MaxWorkGroupSizeTest::MaxWorkGroupSizeTest (tcu::TestContext& testCtx, const std::string& name, const std::string& description, const Params& params)
	: vkt::TestCase	(testCtx, name, description)
	, m_params		(params)
{}

void MaxWorkGroupSizeTest::initPrograms (vk::SourceCollections& programCollection) const
{
	std::ostringstream shader;

	// The actual local sizes will be set using spec constants when running the test instance.
	shader
		<< "#version 450\n"
		<< "\n"
		<< "layout(constant_id=0) const int local_size_x_val = 1;\n"
		<< "layout(constant_id=1) const int local_size_y_val = 1;\n"
		<< "layout(constant_id=2) const int local_size_z_val = 1;\n"
		<< "\n"
		<< "layout(local_size_x_id=0, local_size_y_id=1, local_size_z_id=2) in;\n"
		<< "\n"
		<< "layout(set=0, binding=0) buffer StorageBuffer {\n"
		<< "    uint values[];\n"
		<< "} ssbo;\n"
		<< "\n"
		<< "void main() {\n"
		<< "    ssbo.values[gl_LocalInvocationIndex] = 1u;\n"
		<< "}\n"
		;

	programCollection.glslSources.add("comp") << glu::ComputeSource(shader.str());
}

TestInstance* MaxWorkGroupSizeTest::createInstance (Context& context) const
{
	return new MaxWorkGroupSizeInstance(context, m_params);
}

void MaxWorkGroupSizeTest::checkSupport (Context& context) const
{
	const auto&	vki				= context.getInstanceInterface();
	const auto	physicalDevice	= context.getPhysicalDevice();

	const auto	properties		= vk::getPhysicalDeviceProperties(vki, physicalDevice);
	const auto	invocations		= getInvocations(m_params, vki, physicalDevice, &properties);

	if (invocations > properties.limits.maxComputeWorkGroupInvocations)
		TCU_FAIL("Reported workgroup size limit in the axis is greater than the global invocation limit");

	if (properties.limits.maxStorageBufferRange / static_cast<deUint32>(sizeof(deUint32)) < invocations)
		TCU_THROW(NotSupportedError, "Maximum supported storage buffer range too small");
}

MaxWorkGroupSizeInstance::MaxWorkGroupSizeInstance (Context& context, const MaxWorkGroupSizeTest::Params& params)
	: vkt::TestInstance	(context)
	, m_params			(params)
{}

tcu::TestStatus MaxWorkGroupSizeInstance::iterate (void)
{
	const auto&	vki				= m_context.getInstanceInterface();
	const auto&	vkd				= m_context.getDeviceInterface();
	const auto	physicalDevice	= m_context.getPhysicalDevice();
	const auto	device			= m_context.getDevice();
	auto&		alloc			= m_context.getDefaultAllocator();
	const auto	queueIndex		= m_context.getUniversalQueueFamilyIndex();
	const auto	queue			= m_context.getUniversalQueue();
	auto&		log				= m_context.getTestContext().getLog();

	const auto	axis			= MaxWorkGroupSizeTest::getIndex(m_params.axis);
	const auto	invocations		= MaxWorkGroupSizeTest::getInvocations(m_params, vki, physicalDevice);
	const auto	ssboSize		= static_cast<vk::VkDeviceSize>(MaxWorkGroupSizeTest::getSSBOSize(invocations));

	log
		<< tcu::TestLog::Message
		<< "Running test with " << invocations << " invocations on axis " << axis << " using a storage buffer size of " << ssboSize << " bytes"
		<< tcu::TestLog::EndMessage
		;

	// Main SSBO buffer.
	const auto				ssboInfo	= vk::makeBufferCreateInfo(ssboSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	vk::BufferWithMemory	ssbo		(vkd, device, alloc, ssboInfo, vk::MemoryRequirement::HostVisible);

	// Shader module.
	const auto shaderModule	= vk::createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0u);

	// Descriptor set layouts.
	vk::DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);
	const auto descriptorSetLayout = layoutBuilder.build(vkd, device);

	// Specialization constants: set the number of invocations in the appropriate local size id.
	const auto	entrySize				= static_cast<deUintptr>(sizeof(deInt32));
	deInt32		specializationData[3]	= { 1, 1, 1 };
	specializationData[axis] = static_cast<deInt32>(invocations);

	const vk::VkSpecializationMapEntry specializationMaps[3] =
	{
		{
			0u,										//	deUint32	constantID;
			0u,										//	deUint32	offset;
			entrySize,								//	deUintptr	size;
		},
		{
			1u,										//	deUint32	constantID;
			static_cast<deUint32>(entrySize),		//	deUint32	offset;
			entrySize,								//	deUintptr	size;
		},
		{
			2u,										//	deUint32	constantID;
			static_cast<deUint32>(entrySize * 2u),	//	deUint32	offset;
			entrySize,								//	deUintptr	size;
		},
	};

	const vk::VkSpecializationInfo specializationInfo =
	{
		3u,													//	deUint32						mapEntryCount;
		specializationMaps,									//	const VkSpecializationMapEntry*	pMapEntries;
		static_cast<deUintptr>(sizeof(specializationData)),	//	deUintptr						dataSize;
		specializationData,									//	const void*						pData;
	};

	// Test pipeline.
	const vk::VkPipelineLayoutCreateInfo testPipelineLayoutInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	//	VkStructureType					sType;
		nullptr,											//	const void*						pNext;
		0u,													//	VkPipelineLayoutCreateFlags		flags;
		1u,													//	deUint32						setLayoutCount;
		&descriptorSetLayout.get(),							//	const VkDescriptorSetLayout*	pSetLayouts;
		0u,													//	deUint32						pushConstantRangeCount;
		nullptr,											//	const VkPushConstantRange*		pPushConstantRanges;
	};
	const auto testPipelineLayout = vk::createPipelineLayout(vkd, device, &testPipelineLayoutInfo);

	const vk::VkComputePipelineCreateInfo testPipelineInfo =
	{
		vk::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,	//	VkStructureType					sType;
		nullptr,											//	const void*						pNext;
		0u,													//	VkPipelineCreateFlags			flags;
		{													//	VkPipelineShaderStageCreateInfo	stage;
			vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,//	VkStructureType						sType;
			nullptr,												//	const void*							pNext;
			0u,														//	VkPipelineShaderStageCreateFlags	flags;
			vk::VK_SHADER_STAGE_COMPUTE_BIT,						//	VkShaderStageFlagBits				stage;
			shaderModule.get(),										//	VkShaderModule						module;
			"main",													//	const char*							pName;
			&specializationInfo,									//	const VkSpecializationInfo*			pSpecializationInfo;
		},
		testPipelineLayout.get(),							//	VkPipelineLayout				layout;
		DE_NULL,											//	VkPipeline						basePipelineHandle;
		0u,													//	deInt32							basePipelineIndex;
	};
	const auto testPipeline = vk::createComputePipeline(vkd, device, DE_NULL, &testPipelineInfo);

	// Create descriptor pool and set.
	vk::DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	const auto descriptorPool	= poolBuilder.build(vkd, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	const auto descriptorSet	= vk::makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());

	// Update descriptor set.
	const vk::VkDescriptorBufferInfo ssboBufferInfo =
	{
		ssbo.get(),		//	VkBuffer		buffer;
		0u,				//	VkDeviceSize	offset;
		VK_WHOLE_SIZE,	//	VkDeviceSize	range;
	};

	vk::DescriptorSetUpdateBuilder updateBuilder;
	updateBuilder.writeSingle(descriptorSet.get(), vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &ssboBufferInfo);
	updateBuilder.update(vkd, device);

	// Clear buffer.
	auto& ssboAlloc	= ssbo.getAllocation();
	void* ssboPtr	= ssboAlloc.getHostPtr();
	deMemset(ssboPtr, 0, static_cast<size_t>(ssboSize));
	vk::flushAlloc(vkd, device, ssboAlloc);

	// Run pipelines.
	const auto cmdPool		= vk::makeCommandPool(vkd, device, queueIndex);
	const auto cmdBUfferPtr	= vk::allocateCommandBuffer(vkd, device, cmdPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBUfferPtr.get();

	vk::beginCommandBuffer(vkd, cmdBuffer);

	// Run the main test shader.
	const auto hostToComputeBarrier = vk::makeBufferMemoryBarrier(vk::VK_ACCESS_HOST_WRITE_BIT, vk::VK_ACCESS_SHADER_WRITE_BIT, ssbo.get(), 0ull, VK_WHOLE_SIZE);
	vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 1u, &hostToComputeBarrier, 0u, nullptr);

	vkd.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, testPipeline.get());
	vkd.cmdBindDescriptorSets(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, testPipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);

	const auto computeToHostBarrier = vk::makeBufferMemoryBarrier(vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_HOST_READ_BIT, ssbo.get(), 0ull, VK_WHOLE_SIZE);
	vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u, &computeToHostBarrier, 0u, nullptr);

	vk::endCommandBuffer(vkd, cmdBuffer);
	vk::submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Verify buffer contents.
	vk::invalidateAlloc(vkd, device, ssboAlloc);
	std::unique_ptr<deUint32[]>	valuesArray	(new deUint32[invocations]);
	deUint32*					valuesPtr	= valuesArray.get();
	deMemcpy(valuesPtr, ssboPtr, static_cast<size_t>(ssboSize));

	std::string	errorMsg;
	bool		ok			= true;

	for (size_t i = 0; i < invocations; ++i)
	{
		if (valuesPtr[i] != 1u)
		{
			ok			= false;
			errorMsg	= "Found invalid value for invocation index " + de::toString(i) + ": expected 1u and found " + de::toString(valuesPtr[i]);
			break;
		}
	}

	if (!ok)
		return tcu::TestStatus::fail(errorMsg);
	return tcu::TestStatus::pass("Pass");
}

namespace EmptyShaderTest
{

void createProgram (SourceCollections& dst)
{
	dst.glslSources.add("comp") << glu::ComputeSource(
		"#version 310 es\n"
		"layout (local_size_x = 1) in;\n"
		"void main (void) {}\n"
	);
}

tcu::TestStatus createTest (Context& context)
{
	const DeviceInterface&	vk					= context.getDeviceInterface();
	const VkDevice			device				= context.getDevice();
	const VkQueue			queue				= context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= context.getUniversalQueueFamilyIndex();

	const Unique<VkShaderModule> shaderModule(createShaderModule(vk, device, context.getBinaryCollection().get("comp"), 0u));

	const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device));
	const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

	const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
	const Unique<VkCommandBuffer> cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	// Start recording commands

	beginCommandBuffer(vk, *cmdBuffer);

	vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);

	const tcu::IVec3 workGroups(1, 1, 1);
	vk.cmdDispatch(*cmdBuffer, workGroups.x(), workGroups.y(), workGroups.z());

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	return tcu::TestStatus::pass("Compute succeeded");
}

} // EmptyShaderTest ns
} // anonymous

tcu::TestCaseGroup* createBasicComputeShaderTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> basicComputeTests(new tcu::TestCaseGroup(testCtx, "basic", "Basic compute tests"));

	addFunctionCaseWithPrograms(basicComputeTests.get(), "empty_shader", "Shader that does nothing", EmptyShaderTest::createProgram, EmptyShaderTest::createTest);

	basicComputeTests->addChild(new ConcurrentCompute(testCtx, "concurrent_compute", "Concurrent compute test"));

	basicComputeTests->addChild(new EmptyWorkGroupCase(testCtx, "empty_workgroup_x", "Use an empty workgroup with size 0 on the X axis", tcu::UVec3(0u, 2u, 3u)));
	basicComputeTests->addChild(new EmptyWorkGroupCase(testCtx, "empty_workgroup_y", "Use an empty workgroup with size 0 on the Y axis", tcu::UVec3(2u, 0u, 3u)));
	basicComputeTests->addChild(new EmptyWorkGroupCase(testCtx, "empty_workgroup_z", "Use an empty workgroup with size 0 on the Z axis", tcu::UVec3(2u, 3u, 0u)));
	basicComputeTests->addChild(new EmptyWorkGroupCase(testCtx, "empty_workgroup_all", "Use an empty workgroup with size 0 on the X, Y and Z axes", tcu::UVec3(0u, 0u, 0u)));

	basicComputeTests->addChild(new MaxWorkGroupSizeTest(testCtx, "max_local_size_x", "Use the maximum work group size on the X axis", MaxWorkGroupSizeTest::Params{MaxWorkGroupSizeTest::Axis::X}));
	basicComputeTests->addChild(new MaxWorkGroupSizeTest(testCtx, "max_local_size_y", "Use the maximum work group size on the Y axis", MaxWorkGroupSizeTest::Params{MaxWorkGroupSizeTest::Axis::Y}));
	basicComputeTests->addChild(new MaxWorkGroupSizeTest(testCtx, "max_local_size_z", "Use the maximum work group size on the Z axis", MaxWorkGroupSizeTest::Params{MaxWorkGroupSizeTest::Axis::Z}));

	basicComputeTests->addChild(BufferToBufferInvertTest::UBOToSSBOInvertCase(testCtx,	"ubo_to_ssbo_single_invocation",	"Copy from UBO to SSBO, inverting bits",	256,	tcu::IVec3(1,1,1),	tcu::IVec3(1,1,1)));
	basicComputeTests->addChild(BufferToBufferInvertTest::UBOToSSBOInvertCase(testCtx,	"ubo_to_ssbo_single_group",			"Copy from UBO to SSBO, inverting bits",	1024,	tcu::IVec3(2,1,4),	tcu::IVec3(1,1,1)));
	basicComputeTests->addChild(BufferToBufferInvertTest::UBOToSSBOInvertCase(testCtx,	"ubo_to_ssbo_multiple_invocations",	"Copy from UBO to SSBO, inverting bits",	1024,	tcu::IVec3(1,1,1),	tcu::IVec3(2,4,1)));
	basicComputeTests->addChild(BufferToBufferInvertTest::UBOToSSBOInvertCase(testCtx,	"ubo_to_ssbo_multiple_groups",		"Copy from UBO to SSBO, inverting bits",	1024,	tcu::IVec3(1,4,2),	tcu::IVec3(2,2,4)));

	basicComputeTests->addChild(BufferToBufferInvertTest::CopyInvertSSBOCase(testCtx,	"copy_ssbo_single_invocation",		"Copy between SSBOs, inverting bits",	256,	tcu::IVec3(1,1,1),	tcu::IVec3(1,1,1)));
	basicComputeTests->addChild(BufferToBufferInvertTest::CopyInvertSSBOCase(testCtx,	"copy_ssbo_multiple_invocations",	"Copy between SSBOs, inverting bits",	1024,	tcu::IVec3(1,1,1),	tcu::IVec3(2,4,1)));
	basicComputeTests->addChild(BufferToBufferInvertTest::CopyInvertSSBOCase(testCtx,	"copy_ssbo_multiple_groups",		"Copy between SSBOs, inverting bits",	1024,	tcu::IVec3(1,4,2),	tcu::IVec3(2,2,4)));

	basicComputeTests->addChild(new InvertSSBOInPlaceTest(testCtx,	"ssbo_rw_single_invocation",			"Read and write same SSBO",		256,	true,	tcu::IVec3(1,1,1),	tcu::IVec3(1,1,1)));
	basicComputeTests->addChild(new InvertSSBOInPlaceTest(testCtx,	"ssbo_rw_multiple_groups",				"Read and write same SSBO",		1024,	true,	tcu::IVec3(1,4,2),	tcu::IVec3(2,2,4)));
	basicComputeTests->addChild(new InvertSSBOInPlaceTest(testCtx,	"ssbo_unsized_arr_single_invocation",	"Read and write same SSBO",		256,	false,	tcu::IVec3(1,1,1),	tcu::IVec3(1,1,1)));
	basicComputeTests->addChild(new InvertSSBOInPlaceTest(testCtx,	"ssbo_unsized_arr_multiple_groups",		"Read and write same SSBO",		1024,	false,	tcu::IVec3(1,4,2),	tcu::IVec3(2,2,4)));

	basicComputeTests->addChild(new WriteToMultipleSSBOTest(testCtx,	"write_multiple_arr_single_invocation",			"Write to multiple SSBOs",	256,	true,	tcu::IVec3(1,1,1),	tcu::IVec3(1,1,1)));
	basicComputeTests->addChild(new WriteToMultipleSSBOTest(testCtx,	"write_multiple_arr_multiple_groups",			"Write to multiple SSBOs",	1024,	true,	tcu::IVec3(1,4,2),	tcu::IVec3(2,2,4)));
	basicComputeTests->addChild(new WriteToMultipleSSBOTest(testCtx,	"write_multiple_unsized_arr_single_invocation",	"Write to multiple SSBOs",	256,	false,	tcu::IVec3(1,1,1),	tcu::IVec3(1,1,1)));
	basicComputeTests->addChild(new WriteToMultipleSSBOTest(testCtx,	"write_multiple_unsized_arr_multiple_groups",	"Write to multiple SSBOs",	1024,	false,	tcu::IVec3(1,4,2),	tcu::IVec3(2,2,4)));

	basicComputeTests->addChild(new SSBOLocalBarrierTest(testCtx,	"ssbo_local_barrier_single_invocation",	"SSBO local barrier usage",	tcu::IVec3(1,1,1),	tcu::IVec3(1,1,1)));
	basicComputeTests->addChild(new SSBOLocalBarrierTest(testCtx,	"ssbo_local_barrier_single_group",		"SSBO local barrier usage",	tcu::IVec3(3,2,5),	tcu::IVec3(1,1,1)));
	basicComputeTests->addChild(new SSBOLocalBarrierTest(testCtx,	"ssbo_local_barrier_multiple_groups",	"SSBO local barrier usage",	tcu::IVec3(3,4,1),	tcu::IVec3(2,7,3)));

	basicComputeTests->addChild(new SSBOBarrierTest(testCtx,	"ssbo_cmd_barrier_single",		"SSBO memory barrier usage",	tcu::IVec3(1,1,1)));
	basicComputeTests->addChild(new SSBOBarrierTest(testCtx,	"ssbo_cmd_barrier_multiple",	"SSBO memory barrier usage",	tcu::IVec3(11,5,7)));

	basicComputeTests->addChild(new SharedVarTest(testCtx,	"shared_var_single_invocation",		"Basic shared variable usage",	tcu::IVec3(1,1,1),	tcu::IVec3(1,1,1)));
	basicComputeTests->addChild(new SharedVarTest(testCtx,	"shared_var_single_group",			"Basic shared variable usage",	tcu::IVec3(3,2,5),	tcu::IVec3(1,1,1)));
	basicComputeTests->addChild(new SharedVarTest(testCtx,	"shared_var_multiple_invocations",	"Basic shared variable usage",	tcu::IVec3(1,1,1),	tcu::IVec3(2,5,4)));
	basicComputeTests->addChild(new SharedVarTest(testCtx,	"shared_var_multiple_groups",		"Basic shared variable usage",	tcu::IVec3(3,4,1),	tcu::IVec3(2,7,3)));

	basicComputeTests->addChild(new SharedVarAtomicOpTest(testCtx,	"shared_atomic_op_single_invocation",		"Atomic operation with shared var",		tcu::IVec3(1,1,1),	tcu::IVec3(1,1,1)));
	basicComputeTests->addChild(new SharedVarAtomicOpTest(testCtx,	"shared_atomic_op_single_group",			"Atomic operation with shared var",		tcu::IVec3(3,2,5),	tcu::IVec3(1,1,1)));
	basicComputeTests->addChild(new SharedVarAtomicOpTest(testCtx,	"shared_atomic_op_multiple_invocations",	"Atomic operation with shared var",		tcu::IVec3(1,1,1),	tcu::IVec3(2,5,4)));
	basicComputeTests->addChild(new SharedVarAtomicOpTest(testCtx,	"shared_atomic_op_multiple_groups",			"Atomic operation with shared var",		tcu::IVec3(3,4,1),	tcu::IVec3(2,7,3)));

	basicComputeTests->addChild(new CopyImageToSSBOTest(testCtx,	"copy_image_to_ssbo_small",	"Image to SSBO copy",	tcu::IVec2(1,1),	tcu::IVec2(64,64)));
	basicComputeTests->addChild(new CopyImageToSSBOTest(testCtx,	"copy_image_to_ssbo_large",	"Image to SSBO copy",	tcu::IVec2(2,4),	tcu::IVec2(512,512)));

	basicComputeTests->addChild(new CopySSBOToImageTest(testCtx,	"copy_ssbo_to_image_small",	"SSBO to image copy",	tcu::IVec2(1, 1),	tcu::IVec2(64, 64)));
	basicComputeTests->addChild(new CopySSBOToImageTest(testCtx,	"copy_ssbo_to_image_large",	"SSBO to image copy",	tcu::IVec2(2, 4),	tcu::IVec2(512, 512)));

	basicComputeTests->addChild(new ImageAtomicOpTest(testCtx,	"image_atomic_op_local_size_1",	"Atomic operation with image",	1,	tcu::IVec2(64,64)));
	basicComputeTests->addChild(new ImageAtomicOpTest(testCtx,	"image_atomic_op_local_size_8",	"Atomic operation with image",	8,	tcu::IVec2(64,64)));

	basicComputeTests->addChild(new ImageBarrierTest(testCtx,	"image_barrier_single",		"Image barrier",	tcu::IVec2(1,1)));
	basicComputeTests->addChild(new ImageBarrierTest(testCtx,	"image_barrier_multiple",	"Image barrier",	tcu::IVec2(64,64)));

	basicComputeTests->addChild(cts_amber::createAmberTestCase(testCtx, "write_ssbo_array", "", "compute", "write_ssbo_array.amber"));

	return basicComputeTests.release();
}

tcu::TestCaseGroup* createBasicDeviceGroupComputeShaderTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> deviceGroupComputeTests(new tcu::TestCaseGroup(testCtx, "device_group", "Basic device group compute tests"));

	deviceGroupComputeTests->addChild(new DispatchBaseTest(testCtx,	"dispatch_base",	"Compute shader with base groups",				32768,	tcu::IVec3(4,2,4),	tcu::IVec3(16,8,8),	tcu::IVec3(4,8,8)));
	deviceGroupComputeTests->addChild(new DeviceIndexTest(testCtx,	"device_index",		"Compute shader using deviceIndex in SPIRV",	96,		tcu::IVec3(3,2,1),	tcu::IVec3(2,4,1)));

	return deviceGroupComputeTests.release();

}
} // compute
} // vkt
