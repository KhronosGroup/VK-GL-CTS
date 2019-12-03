/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Valve Corporation.
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
 * \brief Vulkan Memory Model padding access tests
 *//*--------------------------------------------------------------------*/

#include "vktMemoryModelPadding.hpp"
#include "vktTestCase.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkBarrierUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"

#include "deMemory.h"

namespace vkt
{
namespace MemoryModel
{

namespace
{
// The structures below match the shader declarations but have explicit padding members at the end so we can check their contents
// easily after running the shader. Using the std140 layout means structures are aligned to 16 bytes.

// Structure with a 12-byte padding at the end.
struct Pad12
{
	deInt32 a;
	deUint8 padding[12];
};

// Structure with an 8-byte padding at the end.
struct Pad8
{
	deInt32 a, b;
	deUint8 padding[8];
};

// Structure with a 4-byte padding at the end.
struct Pad4
{
	deInt32 a, b, c;
	deUint8 padding[4];
};

// Buffer structure for the input and output buffers.
struct BufferStructure
{
	static constexpr deUint32 kArrayLength = 3u;

	Pad12	subA[kArrayLength];
	Pad8	subB[kArrayLength];
	Pad4	subC[kArrayLength];

	// Pre-fill substructures with the given data.
	BufferStructure (deInt32 a, deInt32 b, deInt32 c, deUint8 paddingByte)
	{
		for (deUint32 i = 0; i < kArrayLength; ++i)
		{
			subA[i].a = a;
			subB[i].a = a;
			subC[i].a = a;
			subB[i].b = b;
			subC[i].b = b;
			subC[i].c = c;
			deMemset(subA[i].padding, static_cast<int>(paddingByte), sizeof(subA[i].padding));
			deMemset(subB[i].padding, static_cast<int>(paddingByte), sizeof(subB[i].padding));
			deMemset(subC[i].padding, static_cast<int>(paddingByte), sizeof(subC[i].padding));
		}
	}

	// Pre-fill substructures with zeros.
	BufferStructure (deUint8 paddingByte)
		: BufferStructure (0, 0, 0, paddingByte)
		{}

	// Verify members and padding bytes.
	bool checkValues (deInt32 a, deInt32 b, deInt32 c, deUint8 paddingByte) const
	{
		for (deUint32 i = 0; i < kArrayLength; ++i)
		{
			if (subA[i].a != a || subB[i].a != a || subC[i].a != a ||
				subB[i].b != b || subC[i].b != b ||
				subC[i].c != c)
				return false;
		}
		return checkPaddingBytes(paddingByte);
	}

	// Verify padding bytes have a known value.
	bool checkPaddingBytes (deUint8 value) const
	{
		for (deUint32 j = 0; j < kArrayLength; ++j)
		{
			for (int i = 0; i < DE_LENGTH_OF_ARRAY(subA[j].padding); ++i)
			{
				if (subA[j].padding[i] != value)
					return false;
			}
			for (int i = 0; i < DE_LENGTH_OF_ARRAY(subB[j].padding); ++i)
			{
				if (subB[j].padding[i] != value)
					return false;
			}
			for (int i = 0; i < DE_LENGTH_OF_ARRAY(subC[j].padding); ++i)
			{
				if (subC[j].padding[i] != value)
					return false;
			}
		}
		return true;
	}
};

class PaddingTest : public vkt::TestCase
{
public:
							PaddingTest		(tcu::TestContext& testCtx, const std::string& name, const std::string& description);
	virtual					~PaddingTest	(void) {}

	virtual void			initPrograms	(vk::SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance	(Context& context) const;
	virtual void			checkSupport	(Context& context) const;

	IterateResult			iterate			(void) { DE_ASSERT(false); return STOP; } // Deprecated in this module
};

class PaddingTestInstance : public vkt::TestInstance
{
public:
								PaddingTestInstance		(Context& context)
									: vkt::TestInstance(context)
									{}
	virtual						~PaddingTestInstance	(void) {}

	virtual tcu::TestStatus		iterate					(void);
};


PaddingTest::PaddingTest (tcu::TestContext& testCtx, const std::string& name, const std::string& description)
	: vkt::TestCase(testCtx, name, description)
{
}

TestInstance* PaddingTest::createInstance (Context& context) const
{
	return new PaddingTestInstance(context);
}

void PaddingTest::initPrograms (vk::SourceCollections& programCollection) const
{
	const std::string arrayLenghtStr = std::to_string(BufferStructure::kArrayLength);

	std::ostringstream shaderSrc;
	shaderSrc
		<< "#version 450\n"
		<< "#pragma use_vulkan_memory_model\n"
		<< "\n"
		<< "struct A {\n"
		<< "    int a;\n"
		<< "};\n"
		<< "\n"
		<< "struct B {\n"
		<< "    int a, b;\n"
		<< "};\n"
		<< "\n"
		<< "struct C {\n"
		<< "    int a, b, c;\n"
		<< "};\n"
		<< "\n"
		<< "struct BufferStructure {\n"
		<< "    A subA[" << arrayLenghtStr << "];\n"
		<< "    B subB[" << arrayLenghtStr << "];\n"
		<< "    C subC[" << arrayLenghtStr << "];\n"
		<< "};\n"
		<< "\n"
		<< "layout (set=0, binding=0, std140) uniform InputBlock\n"
		<< "{\n"
		<< "    BufferStructure inBlock;\n"
		<< "};\n"
		<< "\n"
		<< "layout (set=0, binding=1, std140) buffer OutputBlock\n"
		<< "{\n"
		<< "    BufferStructure outBlock;\n"
		<< "};\n"
		<< "\n"
		<< "void main()\n"
		<< "{\n"
		<< "    const uint idx = gl_GlobalInvocationID.x;\n"
		<< "    outBlock.subA[idx] = inBlock.subA[idx];\n"
		<< "    outBlock.subB[idx] = inBlock.subB[idx];\n"
		<< "    outBlock.subC[idx] = inBlock.subC[idx];\n"
		<< "}\n";

	programCollection.glslSources.add("comp") << glu::ComputeSource(shaderSrc.str());
}

void PaddingTest::checkSupport (Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_vulkan_memory_model");
	if (!context.getVulkanMemoryModelFeatures().vulkanMemoryModel)
	{
		TCU_THROW(NotSupportedError, "Vulkan memory model not supported");
	}
}

tcu::TestStatus PaddingTestInstance::iterate (void)
{
	const auto&	vkd			= m_context.getDeviceInterface();
	const auto	device		= m_context.getDevice();
	auto&		allocator	= m_context.getDefaultAllocator();
	const auto	queue		= m_context.getUniversalQueue();
	const auto	queueIndex	= m_context.getUniversalQueueFamilyIndex();

	constexpr vk::VkDeviceSize kBufferSize	= static_cast<vk::VkDeviceSize>(sizeof(BufferStructure));
	constexpr deInt32 kA					= 1;
	constexpr deInt32 kB					= 2;
	constexpr deInt32 kC					= 3;
	constexpr deUint8 kInputPaddingByte		= 0xFEu;
	constexpr deUint8 kOutputPaddingByte	= 0x7Fu;

	// Create input and output buffers.
	auto inputBufferInfo	= vk::makeBufferCreateInfo(kBufferSize, vk::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	auto outputBufferInfo	= vk::makeBufferCreateInfo(kBufferSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	vk::BufferWithMemory inputBuffer	{vkd, device, allocator, inputBufferInfo,	vk::MemoryRequirement::HostVisible};
	vk::BufferWithMemory outputBuffer	{vkd, device, allocator, outputBufferInfo,	vk::MemoryRequirement::HostVisible};

	// Fill buffers with initial contents.
	BufferStructure inputValues	{kA, kB, kC, kInputPaddingByte};
	BufferStructure outputInit	{kOutputPaddingByte};

	auto& inputAlloc	= inputBuffer.getAllocation();
	auto& outputAlloc	= outputBuffer.getAllocation();

	void* inputBufferPtr	= static_cast<deUint8*>(inputAlloc.getHostPtr()) + inputAlloc.getOffset();
	void* outputBufferPtr	= static_cast<deUint8*>(outputAlloc.getHostPtr()) + outputAlloc.getOffset();

	deMemcpy(inputBufferPtr,	&inputValues,	sizeof(inputValues));
	deMemcpy(outputBufferPtr,	&outputInit,	sizeof(outputInit));

	vk::flushAlloc(vkd, device, inputAlloc);
	vk::flushAlloc(vkd, device, outputAlloc);

	// Descriptor set layout.
	vk::DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);
	layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);
	auto descriptorSetLayout = layoutBuilder.build(vkd, device);

	// Descriptor pool.
	vk::DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	auto descriptorPool = poolBuilder.build(vkd, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

	// Descriptor set.
	const auto descriptorSet = vk::makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());

	// Update descriptor set using the buffers.
	const auto inputBufferDescriptorInfo	= vk::makeDescriptorBufferInfo(inputBuffer.get(), 0ull, VK_WHOLE_SIZE);
	const auto outputBufferDescriptorInfo	= vk::makeDescriptorBufferInfo(outputBuffer.get(), 0ull, VK_WHOLE_SIZE);

	vk::DescriptorSetUpdateBuilder updateBuilder;
	updateBuilder.writeSingle(descriptorSet.get(), vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &inputBufferDescriptorInfo);
	updateBuilder.writeSingle(descriptorSet.get(), vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputBufferDescriptorInfo);
	updateBuilder.update(vkd, device);

	// Create compute pipeline.
	auto shaderModule = vk::createShaderModule(vkd, device, m_context.getBinaryCollection().get("comp"), 0u);
	auto pipelineLayout = vk::makePipelineLayout(vkd, device, descriptorSetLayout.get());

	const vk::VkComputePipelineCreateInfo		pipelineCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		nullptr,
		0u,															// flags
		{															// compute shader
			vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// VkStructureType						sType;
			nullptr,													// const void*							pNext;
			0u,															// VkPipelineShaderStageCreateFlags		flags;
			vk::VK_SHADER_STAGE_COMPUTE_BIT,							// VkShaderStageFlagBits				stage;
			shaderModule.get(),											// VkShaderModule						module;
			"main",														// const char*							pName;
			nullptr,													// const VkSpecializationInfo*			pSpecializationInfo;
		},
		pipelineLayout.get(),										// layout
		DE_NULL,													// basePipelineHandle
		0,															// basePipelineIndex
	};
	auto pipeline = vk::createComputePipeline(vkd, device, DE_NULL, &pipelineCreateInfo);

	// Synchronization barriers.
	auto inputBufferHostToDevBarrier	= vk::makeBufferMemoryBarrier(vk::VK_ACCESS_HOST_WRITE_BIT, vk::VK_ACCESS_SHADER_READ_BIT, inputBuffer.get(), 0ull, VK_WHOLE_SIZE);
	auto outputBufferHostToDevBarrier	= vk::makeBufferMemoryBarrier(vk::VK_ACCESS_HOST_WRITE_BIT, vk::VK_ACCESS_SHADER_WRITE_BIT, outputBuffer.get(), 0ull, VK_WHOLE_SIZE);
	auto outputBufferDevToHostBarrier	= vk::makeBufferMemoryBarrier(vk::VK_ACCESS_SHADER_WRITE_BIT, vk::VK_ACCESS_HOST_READ_BIT, outputBuffer.get(), 0ull, VK_WHOLE_SIZE);

	// Command buffer.
	auto cmdPool		= vk::makeCommandPool(vkd, device, queueIndex);
	auto cmdBufferPtr	= vk::allocateCommandBuffer(vkd, device, cmdPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	auto cmdBuffer		= cmdBufferPtr.get();

	// Record and submit commands.
	vk::beginCommandBuffer(vkd, cmdBuffer);
		vkd.cmdBindPipeline(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.get());
		vkd.cmdBindDescriptorSets(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.get(), 0, 1u, &descriptorSet.get(), 0u, nullptr);
		vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 1u, &inputBufferHostToDevBarrier, 0u, nullptr);
		vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0u, 0u, nullptr, 1u, &outputBufferHostToDevBarrier, 0u, nullptr);
		vkd.cmdDispatch(cmdBuffer, BufferStructure::kArrayLength, 1u, 1u);
		vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, nullptr, 1u, &outputBufferDevToHostBarrier, 0u, nullptr);
	vk::endCommandBuffer(vkd, cmdBuffer);
	vk::submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Verify output buffer contents.
	vk::invalidateAlloc(vkd, device, outputAlloc);
	BufferStructure* outputData = reinterpret_cast<BufferStructure*>(outputBufferPtr);
	return (outputData->checkValues(kA, kB, kC, kOutputPaddingByte) ? tcu::TestStatus::pass("Pass") : tcu::TestStatus::fail("Unexpected values in output data"));
}

} // anonymous

tcu::TestCaseGroup* createPaddingTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> paddingGroup(new tcu::TestCaseGroup(testCtx, "padding", "Padding bytes tests"));
	paddingGroup->addChild(new PaddingTest(testCtx, "test", "Check padding bytes at the end of structures are not touched on copy"));

	return paddingGroup.release();
}

} // MemoryModel
} // vkt
