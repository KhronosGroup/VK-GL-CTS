/*------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2021 The Khronos Group Inc.
* Copyright (c) 2021 Google LLC.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*	  http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*//*!
* \file
* \brief SSBO corner case tests.
*//*--------------------------------------------------------------------*/
#include "deRandom.hpp"

#include "vktSSBOCornerCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"

#include <string>

namespace vkt
{
namespace ssbo
{
	using std::string;
	using std::vector;

namespace
{
class CornerCase : public TestCase
{
public:
								CornerCase			(tcu::TestContext &testCtx, const char *name, const char *description)
	: TestCase										(testCtx, name, description)
	{
		init();
	}
	virtual void				delayedInit			(void);
	virtual void				initPrograms		(vk::SourceCollections &programCollection) const;
	virtual TestInstance*		createInstance		(Context &context) const;

protected:
	string						m_computeShaderSrc;
	const int					m_testSize			= 589; // This is the minimum value of the variable that causes a crash.
};

string useCornerCaseShader (int loopCount)
{
	std::ostringstream src;
	de::Random rnd(1);

	src <<
		"#version 310 es\n"
		"#extension GL_EXT_buffer_reference : enable\n"
		"layout(std430, buffer_reference) buffer BlockA\n"
		"{\n"
		" highp ivec4 a[];\n"
		"};\n"
		// ac_numIrrelevant is not used for anything, but is needed so that compiler doesn't optimize everything out.
		"layout(std140, binding = 0) buffer AcBlock { highp uint ac_numIrrelevant; };\n"
		"\n"
		"layout (push_constant, std430) uniform PC {\n"
		" BlockA blockA;\n"
		"};\n"
		"\n"
		"bool compare_ivec4(highp ivec4 a, highp ivec4 b) { return a == b; }\n"
		"\n"
		"void main (void)\n"
		"{\n"
		" int allOk = int(true);\n";

	for (int i = 0; i < loopCount; i++)
	{
		src << " allOk = allOk & int(compare_ivec4((blockA.a[" << i << "]), ivec4("
			<< rnd.getInt(-9,9) << ", "
			<< rnd.getInt(-9,9) << ", "
			<< rnd.getInt(-9,9) << ", "
			<< rnd.getInt(-9,9) << ")));\n";
	}

	src <<
		" if (allOk != int(false))\n"
		" {\n"
		"  ac_numIrrelevant++;\n"
		" }\n"
		"}\n";

	return src.str();
}

struct Buffer
{
	deUint32	buffer;
	int			size;

	Buffer		(deUint32 buffer_, int size_)	: buffer(buffer_), size(size_) {}
	Buffer		(void)							: buffer(0), size(0) {}
};

de::MovePtr<vk::Allocation> allocateAndBindMemory (Context &context, vk::VkBuffer buffer, vk::MemoryRequirement memReqs)
{
	const vk::DeviceInterface		&vkd	= context.getDeviceInterface();
	const vk::VkMemoryRequirements	bufReqs	= vk::getBufferMemoryRequirements(vkd, context.getDevice(), buffer);
	de::MovePtr<vk::Allocation>		memory	= context.getDefaultAllocator().allocate(bufReqs, memReqs);

	vkd.bindBufferMemory(context.getDevice(), buffer, memory->getMemory(), memory->getOffset());

	return memory;
}

vk::Move<vk::VkBuffer> createBuffer (Context &context, vk::VkDeviceSize bufferSize, vk::VkBufferUsageFlags usageFlags)
{
	const vk::VkDevice			vkDevice			= context.getDevice();
	const vk::DeviceInterface	&vk					= context.getDeviceInterface();
	const deUint32				queueFamilyIndex	= context.getUniversalQueueFamilyIndex();

	const vk::VkBufferCreateInfo bufferInfo =
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
		DE_NULL,									// const void*			pNext;
		0u,											// VkBufferCreateFlags	flags;
		bufferSize,									// VkDeviceSize			size;
		usageFlags,									// VkBufferUsageFlags	usage;
		vk::VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
		1u,											// deUint32				queueFamilyCount;
		&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
	};

	return vk::createBuffer(vk, vkDevice, &bufferInfo);
}
class SSBOCornerCaseInstance : public TestInstance
{
public:
								SSBOCornerCaseInstance	(Context& context, int testSize);
	virtual						~SSBOCornerCaseInstance	(void);
	virtual tcu::TestStatus		iterate					(void);

private:
	int							m_testSize;
};
SSBOCornerCaseInstance::SSBOCornerCaseInstance (Context& context, int testSize)
	: TestInstance	(context)
	, m_testSize	(testSize)
{
}
SSBOCornerCaseInstance::~SSBOCornerCaseInstance (void)
{
}

tcu::TestStatus SSBOCornerCaseInstance::iterate (void)
{
	const vk::DeviceInterface&		vk					= m_context.getDeviceInterface();
	const vk::VkDevice				device				= m_context.getDevice();
	const vk::VkQueue				queue				= m_context.getUniversalQueue();
	const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();

	vk::Move<vk::VkBuffer>			buffer;
	de::MovePtr<vk::Allocation>		alloc;

	// Create descriptor set
	const deUint32					acBufferSize		= 4;
	vk::Move<vk::VkBuffer>			acBuffer			(createBuffer(m_context, acBufferSize, vk:: VK_BUFFER_USAGE_STORAGE_BUFFER_BIT));
	de::UniquePtr<vk::Allocation>	acBufferAlloc		(allocateAndBindMemory(m_context, *acBuffer, vk::MemoryRequirement::HostVisible));

	deMemset(acBufferAlloc->getHostPtr(), 0, acBufferSize);
	flushMappedMemoryRange(vk, device, acBufferAlloc->getMemory(), acBufferAlloc->getOffset(), acBufferSize);

	vk::DescriptorSetLayoutBuilder	setLayoutBuilder;
	vk::DescriptorPoolBuilder		poolBuilder;

	setLayoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);
	poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2);

	const vk::Unique<vk::VkDescriptorSetLayout>	descriptorSetLayout	(setLayoutBuilder.build(vk, device));
	const vk::Unique<vk::VkDescriptorPool>		descriptorPool		(poolBuilder.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const vk::VkDescriptorSetAllocateInfo		allocInfo			=
	{
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		DE_NULL,
		*descriptorPool,
		1u,
		&descriptorSetLayout.get(),
	};

	const vk::Unique<vk::VkDescriptorSet>	descriptorSet	(allocateDescriptorSet(vk, device, &allocInfo));
	const vk::VkDescriptorBufferInfo		descriptorInfo	= makeDescriptorBufferInfo(*acBuffer, 0ull, acBufferSize);

	vk::DescriptorSetUpdateBuilder			setUpdateBuilder;
	vk::VkDescriptorBufferInfo				descriptor;

	setUpdateBuilder
			.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo);

	vk::VkFlags usageFlags		=	vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | vk::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	bool memoryDeviceAddress	=	false;

	if (m_context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address"))
		memoryDeviceAddress = true;

	// Upload base buffers
	const int bufferSize		=	64 * m_testSize;
	{
		vk::VkPhysicalDeviceProperties properties;
		m_context.getInstanceInterface().getPhysicalDeviceProperties(m_context.getPhysicalDevice(), &properties);

		DE_ASSERT(bufferSize > 0);

		buffer		= createBuffer(m_context, bufferSize, usageFlags);
		alloc		= allocateAndBindMemory(m_context, *buffer, vk::MemoryRequirement::HostVisible | (memoryDeviceAddress ? vk::MemoryRequirement::DeviceAddress : vk::MemoryRequirement::Any));
		descriptor	= makeDescriptorBufferInfo(*buffer, 0, bufferSize);
	}

	// Query the buffer device address and push them via push constants
	const bool useKHR = m_context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address");

	vk::VkBufferDeviceAddressInfo info =
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,	// VkStructureType	sType;
		DE_NULL,											// const void*		pNext;
		0,													// VkBuffer			buffer
	};

	info.buffer = descriptor.buffer;
	vk::VkDeviceAddress	addr;
	if (useKHR)
		addr = vk.getBufferDeviceAddress(device, &info);
	else
		addr = vk.getBufferDeviceAddressEXT(device, &info);

	setUpdateBuilder.update(vk, device);

	const vk::VkPushConstantRange pushConstRange =
	{
		vk::VK_SHADER_STAGE_COMPUTE_BIT,		// VkShaderStageFlags	stageFlags
		0,										// deUint32				offset
		(deUint32)(sizeof(vk::VkDeviceAddress))	// deUint32				size
	};

	// Must fit in spec min max
	DE_ASSERT(pushConstRange.size <= 128);

	const vk::VkPipelineLayoutCreateInfo		pipelineLayoutParams		=
	{
	vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,			// VkStructureType					sType;
	DE_NULL,													// const void*						pNext;
	(vk::VkPipelineLayoutCreateFlags)0,
	1u,															// deUint32							descriptorSetCount;
	&*descriptorSetLayout,										// const VkDescriptorSetLayout*		pSetLayouts;
	1u,															// deUint32							pushConstantRangeCount;
	&pushConstRange,											// const VkPushConstantRange*		pPushConstantRanges;
	};
	vk::Move<vk::VkPipelineLayout>	pipelineLayout	(createPipelineLayout(vk, device, &pipelineLayoutParams));

	vk::Move<vk::VkShaderModule>	shaderModule	(createShaderModule(vk, device, m_context.getBinaryCollection().get("compute"), 0));
	const vk::VkPipelineShaderStageCreateInfo	pipelineShaderStageParams	=
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		(vk::VkPipelineShaderStageCreateFlags)0,
		vk::VK_SHADER_STAGE_COMPUTE_BIT,						// VkShaderStage					stage;
		*shaderModule,											// VkShader							shader;
		"main",													//
		DE_NULL,												// const VkSpecializationInfo*		pSpecializationInfo;
	};
	const vk::VkComputePipelineCreateInfo		pipelineCreateInfo			=
	{
		vk::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,		// VkStructureType					sType;
		DE_NULL,												// const void*						pNext;
		0,														// VkPipelineCreateFlags			flags;
		pipelineShaderStageParams,								// VkPipelineShaderStageCreateInfo	stage;
		*pipelineLayout,										// VkPipelineLayout					layout;
		DE_NULL,												// VkPipeline						basePipelineHandle;
		0,														// deInt32							basePipelineIndex;
	};
	vk::Move<vk::VkPipeline>		pipeline	(createComputePipeline(vk, device, DE_NULL, &pipelineCreateInfo));

	vk::Move<vk::VkCommandPool>		cmdPool		(createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
	vk::Move<vk::VkCommandBuffer>	cmdBuffer	(allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	beginCommandBuffer(vk, *cmdBuffer, 0u);

	vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);

	vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, vk::VK_SHADER_STAGE_COMPUTE_BIT,0, (deUint32)(sizeof(addr)), &addr);

	vk.cmdBindDescriptorSets(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

	vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	// Test always passes if it doesn't cause a crash.
	return tcu::TestStatus::pass("Test did not cause a crash");
}

void CornerCase::initPrograms (vk::SourceCollections& programCollection) const
{
	DE_ASSERT(!m_computeShaderSrc.empty());

	programCollection.glslSources.add("compute") << glu::ComputeSource(m_computeShaderSrc);
}

TestInstance* CornerCase::createInstance (Context& context) const
{
	if (!context.isBufferDeviceAddressSupported())
		TCU_THROW(NotSupportedError, "Physical storage buffer pointers not supported");
	return new SSBOCornerCaseInstance(context, m_testSize);
}

void CornerCase::delayedInit (void)
{
	m_computeShaderSrc = useCornerCaseShader(m_testSize);
}
} // anonymous

tcu::TestCaseGroup* createSSBOCornerCaseTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> cornerCaseGroup (new tcu::TestCaseGroup(testCtx, "corner_case", "Corner cases"));
	cornerCaseGroup->addChild(new CornerCase(testCtx, "long_shader_bitwise_and", ""));
	return cornerCaseGroup.release();
}
} // ssbo
} // vkt
