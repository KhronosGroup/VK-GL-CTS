/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2018 The Khronos Group Inc.
 * Copyright (c) 2018 Danylo Piliaiev <danylo.piliaiev@gmail.com>
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
 * \brief Test for conditional rendering of vkCmdDispatch* functions
 *//*--------------------------------------------------------------------*/

#include "vktConditionalDispatchTests.hpp"
#include "vktConditionalRenderingTestUtil.hpp"

#include "vktTestCaseUtil.hpp"
#include "vktComputeTestsUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuResource.hpp"

#include "vkDefs.hpp"
#include "vkCmdUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkObjUtil.hpp"

namespace vkt
{
namespace conditional
{
namespace
{

enum DispatchCommandType
{
	DISPATCH_COMMAND_TYPE_DISPATCH = 0,
	DISPATCH_COMMAND_TYPE_DISPATCH_INDIRECT,
	DISPATCH_COMMAND_TYPE_DISPATCH_BASE,
	DISPATCH_COMMAND_TYPE_DISPATCH_LAST
};

const char* getDispatchCommandTypeName (DispatchCommandType command)
{
	switch (command)
	{
		case DISPATCH_COMMAND_TYPE_DISPATCH:			    return "dispatch";
		case DISPATCH_COMMAND_TYPE_DISPATCH_INDIRECT:	    return "dispatch_indirect";
		case DISPATCH_COMMAND_TYPE_DISPATCH_BASE:			return "dispatch_base";
		default:					                        DE_ASSERT(false);
	}
	return "";
}

struct ConditionalTestSpec
{
	DispatchCommandType	command;
	int					numCalls;
	ConditionalData		conditionalData;
};

class ConditionalDispatchTest : public vkt::TestCase
{
public:
						ConditionalDispatchTest	(tcu::TestContext&			testCtx,
												 const std::string&			name,
												 const std::string&			description,
												 const ConditionalTestSpec&	testSpec);

	void				initPrograms			(vk::SourceCollections&	sourceCollections) const;
	void				checkSupport			(Context&				context) const;
	TestInstance*		createInstance			(Context&				context) const;

private:
	const ConditionalTestSpec m_testSpec;
};

class ConditionalDispatchTestInstance : public TestInstance
{
public:
								ConditionalDispatchTestInstance	(Context &context, ConditionalTestSpec testSpec);

	virtual		tcu::TestStatus iterate							(void);
	void						recordDispatch					(const vk::DeviceInterface&	vk,
																 vk::VkCommandBuffer cmdBuffer,
																 compute::Buffer& indirectBuffer);

protected:
	const DispatchCommandType		m_command;
	const int						m_numCalls;
	const ConditionalData			m_conditionalData;
};

ConditionalDispatchTest::ConditionalDispatchTest(tcu::TestContext&			testCtx,
												 const std::string&			name,
												 const std::string&			description,
												 const ConditionalTestSpec&	testSpec)
	: TestCase		(testCtx, name, description)
	, m_testSpec	(testSpec)
{
}

void ConditionalDispatchTest::initPrograms (vk::SourceCollections& sourceCollections) const
{
	std::ostringstream src;
	src << "#version 310 es\n"
		<< "layout(local_size_x = 1u, local_size_y = 1u, local_size_z = 1u) in;\n"
		<< "layout(set = 0, binding = 0, std140) buffer Out\n"
		<< "{\n"
		<< "    coherent uint count;\n"
		<< "};\n"
		<< "void main(void)\n"
		<< "{\n"
		<< "	atomicAdd(count, 1u);\n"
		<< "}\n";

	sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

void ConditionalDispatchTest::checkSupport(Context& context) const
{
	checkConditionalRenderingCapabilities(context, m_testSpec.conditionalData);

	if (m_testSpec.command == DISPATCH_COMMAND_TYPE_DISPATCH_BASE)
		context.requireDeviceFunctionality("VK_KHR_device_group");
}

TestInstance* ConditionalDispatchTest::createInstance (Context& context) const
{
	return new ConditionalDispatchTestInstance(context, m_testSpec);
}

ConditionalDispatchTestInstance::ConditionalDispatchTestInstance (Context &context, ConditionalTestSpec testSpec)
	: TestInstance(context)
	, m_command(testSpec.command)
	, m_numCalls(testSpec.numCalls)
	, m_conditionalData(testSpec.conditionalData)
{
}

void ConditionalDispatchTestInstance::recordDispatch (const vk::DeviceInterface& vk,
													  vk::VkCommandBuffer cmdBuffer,
													  compute::Buffer& indirectBuffer)
{
	for (int i = 0; i < m_numCalls; i++)
	{
		switch (m_command)
		{
			case DISPATCH_COMMAND_TYPE_DISPATCH:
			{
				vk.cmdDispatch(cmdBuffer, 1, 1, 1);
				break;
			}
			case DISPATCH_COMMAND_TYPE_DISPATCH_INDIRECT:
			{
				vk.cmdDispatchIndirect(cmdBuffer, *indirectBuffer, 0);
				break;
			}
			case DISPATCH_COMMAND_TYPE_DISPATCH_BASE:
			{
				vk.cmdDispatchBase(cmdBuffer, 0, 0, 0, 1, 1, 1);
				break;
			}
			default: DE_ASSERT(DE_FALSE);
		}
	}
}

tcu::TestStatus ConditionalDispatchTestInstance::iterate (void)
{
	const vk::DeviceInterface&	vk					= m_context.getDeviceInterface();
	const vk::VkDevice			device				= m_context.getDevice();
	const vk::VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			    queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	vk::Allocator&				allocator			= m_context.getDefaultAllocator();

	// Create a buffer and host-visible memory for it

	const vk::VkDeviceSize bufferSizeBytes = sizeof(deUint32);
	const compute::Buffer outputBuffer(vk, device, allocator, vk::makeBufferCreateInfo(bufferSizeBytes, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), vk::MemoryRequirement::HostVisible);

	{
		const vk::Allocation& alloc = outputBuffer.getAllocation();
		deUint8* outputBufferPtr = reinterpret_cast<deUint8*>(alloc.getHostPtr());
		*(deUint32*)(outputBufferPtr) = 0;
		vk::flushAlloc(vk, device, alloc);
	}

	// Create descriptor set

	const vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(
		vk::DescriptorSetLayoutBuilder()
		.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT)
		.build(vk, device));

	const vk::Unique<vk::VkDescriptorPool> descriptorPool(
		vk::DescriptorPoolBuilder()
		.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const vk::Unique<vk::VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	const vk::VkDescriptorBufferInfo descriptorInfo = vk::makeDescriptorBufferInfo(*outputBuffer, 0ull, bufferSizeBytes);
	vk::DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo)
		.update(vk, device);

	// Setup pipeline

	const vk::Unique<vk::VkShaderModule>	shaderModule		(createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0u));
	const vk::Unique<vk::VkPipelineLayout>	pipelineLayout		(makePipelineLayout(vk, device, *descriptorSetLayout));
	const vk::Unique<vk::VkPipeline>		pipeline			(compute::makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

	const vk::Unique<vk::VkCommandPool>		cmdPool				(makeCommandPool(vk, device, queueFamilyIndex));
	const vk::Unique<vk::VkCommandBuffer>	cmdBuffer			(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	const vk::Unique<vk::VkCommandBuffer>	secondaryCmdBuffer	(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY));

	// Create indirect buffer
	const vk::VkDispatchIndirectCommand dispatchCommands[] = { { 1u, 1u, 1u } };

	compute::Buffer indirectBuffer(
		vk, device, allocator,
		vk::makeBufferCreateInfo(sizeof(dispatchCommands), vk::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
		vk::MemoryRequirement::HostVisible);

	deUint8* indirectBufferPtr = reinterpret_cast<deUint8*>(indirectBuffer.getAllocation().getHostPtr());
	deMemcpy(indirectBufferPtr, &dispatchCommands[0], sizeof(dispatchCommands));

	vk::flushAlloc(vk, device, indirectBuffer.getAllocation());

	// Start recording commands

	beginCommandBuffer(vk, *cmdBuffer);

	vk::VkCommandBuffer targetCmdBuffer = *cmdBuffer;

	const bool useSecondaryCmdBuffer = m_conditionalData.conditionInherited || m_conditionalData.conditionInSecondaryCommandBuffer;

	if (useSecondaryCmdBuffer)
	{
		const vk::VkCommandBufferInheritanceConditionalRenderingInfoEXT conditionalRenderingInheritanceInfo =
		{
			vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_CONDITIONAL_RENDERING_INFO_EXT,
			DE_NULL,
			m_conditionalData.conditionInherited ? VK_TRUE : VK_FALSE	// conditionalRenderingEnable
		};

		const vk::VkCommandBufferInheritanceInfo inheritanceInfo =
		{
			vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
			&conditionalRenderingInheritanceInfo,
			0u,										        // renderPass
			0u,												// subpass
			0u,										        // framebuffer
			VK_FALSE,										// occlusionQueryEnable
			(vk::VkQueryControlFlags)0u,					// queryFlags
			(vk::VkQueryPipelineStatisticFlags)0u,			// pipelineStatistics
		};

		const vk::VkCommandBufferBeginInfo commandBufferBeginInfo =
		{
			vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			DE_NULL,
			vk::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			&inheritanceInfo
		};

		vk.beginCommandBuffer(*secondaryCmdBuffer, &commandBufferBeginInfo);

		targetCmdBuffer = *secondaryCmdBuffer;
	}

	vk.cmdBindPipeline(targetCmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
	vk.cmdBindDescriptorSets(targetCmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

	de::SharedPtr<Draw::Buffer> conditionalBuffer = createConditionalRenderingBuffer(m_context, m_conditionalData);

	if (m_conditionalData.conditionInSecondaryCommandBuffer)
	{
		beginConditionalRendering(vk, *secondaryCmdBuffer, *conditionalBuffer, m_conditionalData);
		recordDispatch(vk, *secondaryCmdBuffer, indirectBuffer);
		vk.cmdEndConditionalRenderingEXT(*secondaryCmdBuffer);
		vk.endCommandBuffer(*secondaryCmdBuffer);
	}
	else if (m_conditionalData.conditionInherited)
	{
		recordDispatch(vk, *secondaryCmdBuffer, indirectBuffer);
		vk.endCommandBuffer(*secondaryCmdBuffer);
	}

	if (m_conditionalData.conditionInPrimaryCommandBuffer)
	{
		beginConditionalRendering(vk, *cmdBuffer, *conditionalBuffer, m_conditionalData);

		if (m_conditionalData.conditionInherited)
		{
			vk.cmdExecuteCommands(*cmdBuffer, 1, &secondaryCmdBuffer.get());
		}
		else
		{
			recordDispatch(vk, *cmdBuffer, indirectBuffer);
		}

		vk.cmdEndConditionalRenderingEXT(*cmdBuffer);
	}
	else if (useSecondaryCmdBuffer)
	{
		vk.cmdExecuteCommands(*cmdBuffer, 1, &secondaryCmdBuffer.get());
	}

	const vk::VkBufferMemoryBarrier outputBufferMemoryBarrier =
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		DE_NULL,
		vk::VK_ACCESS_SHADER_WRITE_BIT,
		vk::VK_ACCESS_HOST_READ_BIT,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		outputBuffer.get(),
		0u,
		VK_WHOLE_SIZE
	};

	vk.cmdPipelineBarrier(*cmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u, 0u, DE_NULL, 1u, &outputBufferMemoryBarrier, 0u, DE_NULL);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, *cmdBuffer);

	// Check result

	qpTestResult res = QP_TEST_RESULT_PASS;

	const vk::Allocation& outputBufferAllocation = outputBuffer.getAllocation();
	invalidateAlloc(vk, device, outputBufferAllocation);

	const deUint32* bufferPtr = static_cast<deUint32*>(outputBufferAllocation.getHostPtr());

	const deUint32 expectedResult = m_conditionalData.expectCommandExecution ? m_numCalls : 0u;
	if (bufferPtr[0] != expectedResult)
	{
		res = QP_TEST_RESULT_FAIL;
	}

	return tcu::TestStatus(res, qpGetTestResultName(res));
}

}	// anonymous

ConditionalDispatchTests::ConditionalDispatchTests (tcu::TestContext &testCtx)
	: TestCaseGroup	(testCtx, "dispatch", "Conditional Rendering Of Dispatch Commands")
{
	/* Left blank on purpose */
}

ConditionalDispatchTests::~ConditionalDispatchTests (void) {}

void ConditionalDispatchTests::init (void)
{
	for (int conditionNdx = 0; conditionNdx < DE_LENGTH_OF_ARRAY(conditional::s_testsData); conditionNdx++)
	{
		const ConditionalData& conditionData = conditional::s_testsData[conditionNdx];

		de::MovePtr<tcu::TestCaseGroup> conditionalDrawRootGroup(new tcu::TestCaseGroup(m_testCtx, de::toString(conditionData).c_str(), "Conditionaly execute dispatch calls"));

		for (deUint32 commandTypeIdx = 0; commandTypeIdx < DISPATCH_COMMAND_TYPE_DISPATCH_LAST; ++commandTypeIdx)
		{
			const DispatchCommandType command = DispatchCommandType(commandTypeIdx);

			ConditionalTestSpec testSpec;
			testSpec.command = command;
			testSpec.numCalls = 3;
			testSpec.conditionalData = conditionData;

			conditionalDrawRootGroup->addChild(new ConditionalDispatchTest(m_testCtx, getDispatchCommandTypeName(command), "", testSpec));
		}

		addChild(conditionalDrawRootGroup.release());
	}

	// Tests verifying the condition is interpreted as a 32-bit value.
	{
		de::MovePtr<tcu::TestCaseGroup> conditionSizeGroup(new tcu::TestCaseGroup(m_testCtx, "condition_size", "Tests verifying the condition is being read as a 32-bit value"));

		struct ValuePaddingExecution
		{
			deUint32	value;
			bool		padding;
			bool		execution;
			const char*	name;
		};

		const ValuePaddingExecution kConditionValueResults[] =
		{
			{	0x00000001u,	false,	true,	"first_byte"	},
			{	0x00000100u,	false,	true,	"second_byte"	},
			{	0x00010000u,	false,	true,	"third_byte"	},
			{	0x01000000u,	false,	true,	"fourth_byte"	},
			{	0u,				true,	false,	"padded_zero"	},
		};

		enum class ConditionSizeSubcaseType
		{
			PRIMARY_FLAT = 0,
			PRIMARY_WITH_SECONDARY,
			SECONDARY_NORMAL,
			SECONDARY_INHERITED,
		};

		struct ConditionSizeSubcase
		{
			ConditionSizeSubcaseType	type;
			const char*					name;
		};

		const ConditionSizeSubcase kConditionSizeSubcase[] =
		{
			{ ConditionSizeSubcaseType::PRIMARY_FLAT,				"primary"				},
			{ ConditionSizeSubcaseType::PRIMARY_WITH_SECONDARY,		"inherited"				},
			{ ConditionSizeSubcaseType::SECONDARY_NORMAL,			"secondary"				},
			{ ConditionSizeSubcaseType::SECONDARY_INHERITED,		"secondary_inherited"	},
		};

		for (int subcaseNdx = 0; subcaseNdx < DE_LENGTH_OF_ARRAY(kConditionSizeSubcase); ++subcaseNdx)
		{
			const auto& subcase = kConditionSizeSubcase[subcaseNdx];

			de::MovePtr<tcu::TestCaseGroup> subcaseGroup(new tcu::TestCaseGroup(m_testCtx, subcase.name, ""));

			ConditionalData conditionalData		= {};
			conditionalData.conditionInverted	= false;

			switch (subcase.type)
			{
				case ConditionSizeSubcaseType::PRIMARY_FLAT:
					conditionalData.conditionInPrimaryCommandBuffer		= true;
					conditionalData.conditionInSecondaryCommandBuffer	= false;
					conditionalData.conditionInherited					= false;
					break;

				case ConditionSizeSubcaseType::PRIMARY_WITH_SECONDARY:
					conditionalData.conditionInPrimaryCommandBuffer		= true;
					conditionalData.conditionInSecondaryCommandBuffer	= false;
					conditionalData.conditionInherited					= true;
					break;

				case ConditionSizeSubcaseType::SECONDARY_NORMAL:
					conditionalData.conditionInPrimaryCommandBuffer		= false;
					conditionalData.conditionInSecondaryCommandBuffer	= true;
					conditionalData.conditionInherited					= false;
					break;

				case ConditionSizeSubcaseType::SECONDARY_INHERITED:
					conditionalData.conditionInPrimaryCommandBuffer		= false;
					conditionalData.conditionInSecondaryCommandBuffer	= true;
					conditionalData.conditionInherited					= true;
					break;

				default:
					DE_ASSERT(false);
					break;
			}

			for (int valueNdx = 0; valueNdx < DE_LENGTH_OF_ARRAY(kConditionValueResults); ++valueNdx)
			{
				const auto& valueResults = kConditionValueResults[valueNdx];

				conditionalData.conditionValue			= valueResults.value;
				conditionalData.padConditionValue		= valueResults.padding;
				conditionalData.expectCommandExecution	= valueResults.execution;

				ConditionalTestSpec spec;
				spec.command			= DISPATCH_COMMAND_TYPE_DISPATCH;
				spec.numCalls			= 1;
				spec.conditionalData	= conditionalData;

				subcaseGroup->addChild(new ConditionalDispatchTest(m_testCtx, valueResults.name, "", spec));
			}

			conditionSizeGroup->addChild(subcaseGroup.release());
		}

		addChild(conditionSizeGroup.release());
	}
}

}	// conditional
}	// vkt
