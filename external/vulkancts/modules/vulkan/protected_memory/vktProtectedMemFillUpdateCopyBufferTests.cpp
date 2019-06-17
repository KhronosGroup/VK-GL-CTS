/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
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
 * \brief Protected memory fill/update/copy buffer tests
 *//*--------------------------------------------------------------------*/

#include "vktProtectedMemFillUpdateCopyBufferTests.hpp"

#include <limits>
#include "deRandom.hpp"
#include "tcuTestLog.hpp"
#include "tcuVector.hpp"

#include "vkPrograms.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"

#include "vktProtectedMemContext.hpp"
#include "vktProtectedMemUtils.hpp"
#include "vktProtectedMemBufferValidator.hpp"

namespace vkt
{
namespace ProtectedMem
{

namespace
{

enum {
	BUFFER_SIZE		= 64,
	MAX_POSITION	= BUFFER_SIZE / 4,
};

enum CmdType {
	FILL_BUFFER,
	UPDATE_BUFFER,
	COPY_BUFFER,
};

static const char* getTestTypeName(CmdType cmdType)
{
	switch (cmdType) {
		case FILL_BUFFER:		return "Fill buffer";
		case UPDATE_BUFFER:		return "Update buffer";
		case COPY_BUFFER:		return "Copy buffer";
		default:
			DE_ASSERT(false);
			return DE_NULL;
	}
}

template<typename T>
class FillUpdateCopyBufferTestInstance : public ProtectedTestInstance
{
public:
								FillUpdateCopyBufferTestInstance	(Context&					ctx,
																	 const deUint32				fillValue,
																	 const BufferValidator<T>&	validator,
																	 CmdType					cmdType,
																	 const CmdBufferType		cmdBufferType);
	virtual tcu::TestStatus		iterate								(void);

private:
	const deUint32				m_fillValue;
	const BufferValidator<T>&	m_validator;
	CmdType						m_cmdType;
	const CmdBufferType			m_cmdBufferType;
};


template<typename T>
class FillUpdateCopyBufferTestCase : public TestCase
{
public:
							FillUpdateCopyBufferTestCase	(tcu::TestContext&		testCtx,
															 const std::string&		name,
															 deUint32				fillValue,
															 ValidationData<T>		data,
															 CmdType				cmdType,
															 CmdBufferType			cmdBufferType)
								: TestCase			(testCtx, name, getTestTypeName(cmdType))
								, m_fillValue		(fillValue)
								, m_validator		(data)
								, m_cmdType			(cmdType)
								, m_cmdBufferType	(cmdBufferType)
							{
							}

	virtual					~FillUpdateCopyBufferTestCase	(void) {}
	virtual TestInstance*	createInstance					(Context& ctx) const
							{
								return new FillUpdateCopyBufferTestInstance<T>(ctx, m_fillValue, m_validator, m_cmdType, m_cmdBufferType);
							}
	virtual void			initPrograms					(vk::SourceCollections& programCollection) const
							{
								m_validator.initPrograms(programCollection);
							}
	virtual void			checkSupport					(Context& context) const
							{
								checkProtectedQueueSupport(context);
							}
private:
	deUint32				m_fillValue;
	BufferValidator<T>		m_validator;
	CmdType					m_cmdType;
	CmdBufferType			m_cmdBufferType;
};

template<typename T>
FillUpdateCopyBufferTestInstance<T>::FillUpdateCopyBufferTestInstance	(Context&					ctx,
																		 const deUint32				fillValue,
																		 const BufferValidator<T>&	validator,
																		 CmdType					cmdType,
																		 const CmdBufferType		cmdBufferType)
	: ProtectedTestInstance	(ctx)
	, m_fillValue			(fillValue)
	, m_validator			(validator)
	, m_cmdType				(cmdType)
	, m_cmdBufferType		(cmdBufferType)
{
}

template<typename T>
tcu::TestStatus FillUpdateCopyBufferTestInstance<T>::iterate()
{
	ProtectedContext&					ctx					(m_protectedContext);
	const vk::DeviceInterface&			vk					= ctx.getDeviceInterface();
	const vk::VkDevice					device				= ctx.getDevice();
	const vk::VkQueue					queue				= ctx.getQueue();
	const deUint32						queueFamilyIndex	= ctx.getQueueFamilyIndex();
	const deUint32						bufferSize			= (deUint32)(BUFFER_SIZE * sizeof(deUint32));

	de::MovePtr<vk::BufferWithMemory>	dstBuffer			(makeBuffer(ctx,
																		PROTECTION_ENABLED,
																		queueFamilyIndex,
																		bufferSize,
																		vk::VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
																			| vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
																		vk::MemoryRequirement::Protected));

	de::MovePtr<vk::BufferWithMemory>	srcBuffer			(makeBuffer(ctx,
																		PROTECTION_ENABLED,
																		queueFamilyIndex,
																		bufferSize,
																		vk::VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
																			| vk::VK_BUFFER_USAGE_TRANSFER_SRC_BIT
																			| vk::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
																		vk::MemoryRequirement::Protected));

	vk::Unique<vk::VkCommandPool>		cmdPool				(makeCommandPool(vk, device, PROTECTION_ENABLED, queueFamilyIndex));
	vk::Unique<vk::VkCommandBuffer>		cmdBuffer			(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY));
	vk::Unique<vk::VkCommandBuffer>		secondaryCmdBuffer	(vk::allocateCommandBuffer(vk, device, *cmdPool, vk::VK_COMMAND_BUFFER_LEVEL_SECONDARY));
	vk::VkCommandBuffer					targetCmdBuffer		= (m_cmdBufferType == CMD_BUFFER_SECONDARY) ? *secondaryCmdBuffer : *cmdBuffer;

	// Begin cmd buffer
	beginCommandBuffer(vk, *cmdBuffer);

	if (m_cmdBufferType == CMD_BUFFER_SECONDARY)
	{
		// Begin secondary command buffer
		const vk::VkCommandBufferInheritanceInfo	secCmdBufInheritInfo	=
		{
			vk::VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
			DE_NULL,
			(vk::VkRenderPass)0u,										// renderPass
			0u,															// subpass
			(vk::VkFramebuffer)0u,										// framebuffer
			VK_FALSE,													// occlusionQueryEnable
			(vk::VkQueryControlFlags)0u,								// queryFlags
			(vk::VkQueryPipelineStatisticFlags)0u,						// pipelineStatistics
		};
		beginSecondaryCommandBuffer(vk, *secondaryCmdBuffer, secCmdBufInheritInfo);
	}

	switch (m_cmdType) {
		case FILL_BUFFER:
			{
				// Fill buffer
				vk.cmdFillBuffer(targetCmdBuffer, **dstBuffer, 0u, VK_WHOLE_SIZE, m_fillValue);
				break;
			}

		case UPDATE_BUFFER:
			{
				// Update buffer
				deUint32 data[BUFFER_SIZE];
				for (size_t ndx = 0; ndx < BUFFER_SIZE; ndx++)
					data[ndx] = m_fillValue;
				vk.cmdUpdateBuffer(targetCmdBuffer, **dstBuffer, 0u, bufferSize, (const deUint32 *) &data);
				break;
			}

		case COPY_BUFFER:
			{
				vk.cmdFillBuffer(targetCmdBuffer, **srcBuffer, 0u, VK_WHOLE_SIZE, m_fillValue);

				const vk::VkBufferMemoryBarrier copyBufferBarrier	=
				{
					vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,		// VkStructureType		sType
					DE_NULL,											// const void*			pNext
					vk::VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags		srcAccessMask
					vk::VK_ACCESS_TRANSFER_READ_BIT,					// VkAccessFlags		dstAccessMask
					queueFamilyIndex,									// uint32_t				srcQueueFamilyIndex
					queueFamilyIndex,									// uint32_t				dstQueueFamilyIndex
					**srcBuffer,										// VkBuffer				buffer
					0u,													// VkDeviceSize			offset
					VK_WHOLE_SIZE,										// VkDeviceSize			size
				};

				vk.cmdPipelineBarrier(targetCmdBuffer,
									  vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
									  vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
									  (vk::VkDependencyFlags)0,
									  0, (const vk::VkMemoryBarrier*)DE_NULL,
									  1, &copyBufferBarrier,
									  0, (const vk::VkImageMemoryBarrier*)DE_NULL);

				// Copy buffer
				const vk::VkBufferCopy				copyBufferRegion		=
				{
					0ull,								// VkDeviceSize		srcOffset;
					0ull,								// VkDeviceSize		dstOffset;
					bufferSize							// VkDeviceSize		size;
				};
				vk.cmdCopyBuffer(targetCmdBuffer, **srcBuffer, **dstBuffer, 1u, &copyBufferRegion);
				break;
			}

		default:
			DE_ASSERT(false);
			break;
	}

	{
		// Buffer validator reads buffer in compute shader
		const vk::VkBufferMemoryBarrier	endBufferBarrier		=
		{
			vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,		// VkStructureType		sType
			DE_NULL,											// const void*			pNext
			vk::VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags		srcAccessMask
			vk::VK_ACCESS_SHADER_READ_BIT,						// VkAccessFlags		dstAccessMask
			queueFamilyIndex,									// uint32_t				srcQueueFamilyIndex
			queueFamilyIndex,									// uint32_t				dstQueueFamilyIndex
			**dstBuffer,										// VkBuffer				buffer
			0u,													// VkDeviceSize			offset
			VK_WHOLE_SIZE,										// VkDeviceSize			size
		};
		vk.cmdPipelineBarrier(targetCmdBuffer,
							  vk::VK_PIPELINE_STAGE_TRANSFER_BIT,
							  vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							  (vk::VkDependencyFlags)0,
							  0, (const vk::VkMemoryBarrier*)DE_NULL,
							  1, &endBufferBarrier,
							  0, (const vk::VkImageMemoryBarrier*)DE_NULL);
	}

	if (m_cmdBufferType == CMD_BUFFER_SECONDARY)
	{
		endCommandBuffer(vk, *secondaryCmdBuffer);
		vk.cmdExecuteCommands(*cmdBuffer, 1u, &secondaryCmdBuffer.get());
	}

	endCommandBuffer(vk, *cmdBuffer);

	// Submit command buffer
	const vk::Unique<vk::VkFence>	fence		(vk::createFence(vk, device));
	VK_CHECK(queueSubmit(ctx, PROTECTION_ENABLED, queue, *cmdBuffer, *fence, ~0ull));

	// Log out test data
	ctx.getTestContext().getLog()
		<< tcu::TestLog::Message << "Fill value: " << m_fillValue << tcu::TestLog::EndMessage;

	// Validate resulting buffer
	if (m_validator.validateBuffer(ctx, **dstBuffer))
		return tcu::TestStatus::pass("Everything went OK");
	else
		return tcu::TestStatus::fail("Something went really wrong");
}

tcu::TestCaseGroup*	createFillUpdateCopyBufferFloatTests (tcu::TestContext& testCtx, CmdType cmdType, CmdBufferType cmdBufferType)
{
	struct {
		const union {
			float		flt;
			deUint32	uint;
		}							fillValue;
		const ValidationDataVec4	data;
	} testData[] = {
		{	{ 3.2f },
			{
				{ tcu::IVec4(1),			tcu::IVec4(2),			tcu::IVec4(3),			tcu::IVec4(4)			},
				{ tcu::Vec4(3.2f),			tcu::Vec4(3.2f),		tcu::Vec4(3.2f),		tcu::Vec4(3.2f)			}
			}
		},
		{	{ 18.8f },
			{
				{ tcu::IVec4(5),			tcu::IVec4(6),			tcu::IVec4(7),			tcu::IVec4(8)			},
				{ tcu::Vec4(18.8f),			tcu::Vec4(18.8f),		tcu::Vec4(18.8f),		tcu::Vec4(18.8f)		}
			}
		},
		{	{ 669154.6f },
			{
				{ tcu::IVec4(9),			tcu::IVec4(10),			tcu::IVec4(11),			tcu::IVec4(12)			},
				{ tcu::Vec4(669154.6f),		tcu::Vec4(669154.6f),	tcu::Vec4(669154.6f),	tcu::Vec4(669154.6f)	}
			}
		},
		{	{ -40.0f },
			{
				{ tcu::IVec4(13),			tcu::IVec4(14),			tcu::IVec4(15),			tcu::IVec4(0)			},
				{ tcu::Vec4(-40.0f),		tcu::Vec4(-40.0f),		tcu::Vec4(-40.0f),		tcu::Vec4(-40.0f)		}
			}
		},
		{	{ -915.7f },
			{
				{ tcu::IVec4(1),			tcu::IVec4(5),			tcu::IVec4(10),			tcu::IVec4(15)			},
				{ tcu::Vec4(-915.7f),		tcu::Vec4(-915.7f),		tcu::Vec4(-915.7f),		tcu::Vec4(-915.7f)		}
			}
		},
		{	{ -2548675.1f },
			{
				{ tcu::IVec4(15),			tcu::IVec4(1),			tcu::IVec4(9),			tcu::IVec4(13)			},
				{ tcu::Vec4(-2548675.1f),	tcu::Vec4(-2548675.1f),	tcu::Vec4(-2548675.1f),	tcu::Vec4(-2548675.1f)	}
			}
		},
	};

	std::string desc = std::string(getTestTypeName(cmdType)) + " (float)";

	de::MovePtr<tcu::TestCaseGroup>	staticTests		(new tcu::TestCaseGroup(testCtx, "static", (desc + " with static input").c_str()));
	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(testData); ++ndx)
	{
		DE_ASSERT(testData[ndx].data.positions[0].x() < MAX_POSITION);
		DE_ASSERT(testData[ndx].data.positions[1].x() < MAX_POSITION);
		DE_ASSERT(testData[ndx].data.positions[2].x() < MAX_POSITION);
		DE_ASSERT(testData[ndx].data.positions[3].x() < MAX_POSITION);

		const std::string name = "test_" + de::toString(ndx + 1);
		staticTests->addChild(new FillUpdateCopyBufferTestCase<tcu::Vec4>(
							testCtx, name.c_str(), testData[ndx].fillValue.uint, testData[ndx].data, cmdType, cmdBufferType));
	}

	/* Add a few randomized tests */
	de::MovePtr<tcu::TestCaseGroup>	randomTests		(new tcu::TestCaseGroup(testCtx, "random", (desc + " with random input").c_str()));
	const int						testCount		= 10;
	de::Random						rnd				(testCtx.getCommandLine().getBaseSeed());
	for (int ndx = 0; ndx < testCount; ++ndx)
	{
		const std::string	name		= "test_" + de::toString(ndx + 1);
		const union {
			float		flt;
			deUint32	uint;
		}					fillValue	= { rnd.getFloat(std::numeric_limits<float>::min(), std::numeric_limits<float>::max() - 1) };

		const tcu::Vec4		refValue	(fillValue.flt);
		const tcu::IVec4	vec0		= tcu::IVec4(rnd.getInt(0, MAX_POSITION - 1));
		const tcu::IVec4	vec1		= tcu::IVec4(rnd.getInt(0, MAX_POSITION - 1));
		const tcu::IVec4	vec2		= tcu::IVec4(rnd.getInt(0, MAX_POSITION - 1));
		const tcu::IVec4	vec3		= tcu::IVec4(rnd.getInt(0, MAX_POSITION - 1));

		ValidationDataVec4	data		=
		{
			{ vec0, vec1, vec2, vec3 },
			{ refValue, refValue, refValue, refValue }
		};

		DE_ASSERT(data.positions[0].x() < MAX_POSITION);
		DE_ASSERT(data.positions[1].x() < MAX_POSITION);
		DE_ASSERT(data.positions[2].x() < MAX_POSITION);
		DE_ASSERT(data.positions[3].x() < MAX_POSITION);

		randomTests->addChild(new FillUpdateCopyBufferTestCase<tcu::Vec4>(testCtx, name.c_str(), fillValue.uint, data, cmdType, cmdBufferType));
	}

	const std::string				groupName		= getCmdBufferTypeStr(cmdBufferType);
	de::MovePtr<tcu::TestCaseGroup>	primaryGroup	(new tcu::TestCaseGroup(testCtx, groupName.c_str(), (desc + " using " + groupName.c_str() + " command buffer").c_str()));
	primaryGroup->addChild(staticTests.release());
	primaryGroup->addChild(randomTests.release());

	return primaryGroup.release();
}

tcu::TestCaseGroup*	createFillUpdateCopyBufferFloatTests (tcu::TestContext& testCtx, CmdType cmdType)
{
	const std::string desc = std::string(getTestTypeName(cmdType)) + " (float)";
	de::MovePtr<tcu::TestCaseGroup> testGroup (new tcu::TestCaseGroup(testCtx, "float_buffer", desc.c_str()));
	testGroup->addChild(createFillUpdateCopyBufferFloatTests(testCtx, cmdType, CMD_BUFFER_PRIMARY));
	testGroup->addChild(createFillUpdateCopyBufferFloatTests(testCtx, cmdType, CMD_BUFFER_SECONDARY));
	return testGroup.release();
}

tcu::TestCaseGroup*	createFillUpdateCopyBufferIntegerTests (tcu::TestContext& testCtx, CmdType cmdType, CmdBufferType cmdBufferType)
{
	struct {
		const union {
			deInt32		integer;
			deUint32	uint;
		}							fillValue;
		const ValidationDataIVec4	data;
	} testData[] = {
		{	{ 3 },
			{
				{ tcu::IVec4(1),			tcu::IVec4(2),			tcu::IVec4(3),			tcu::IVec4(4)			},
				{ tcu::IVec4(3),			tcu::IVec4(3),			tcu::IVec4(3),			tcu::IVec4(3)			}
			}
		},
		{	{ 18 },
			{
				{ tcu::IVec4(5),			tcu::IVec4(6),			tcu::IVec4(7),			tcu::IVec4(8)			},
				{ tcu::IVec4(18),			tcu::IVec4(18),			tcu::IVec4(18),			tcu::IVec4(18)			}
			}
		},
		{	{ 669154 },
			{
				{ tcu::IVec4(9),			tcu::IVec4(10),			tcu::IVec4(11),			tcu::IVec4(12)			},
				{ tcu::IVec4(669154),		tcu::IVec4(669154),		tcu::IVec4(669154),		tcu::IVec4(669154)		}
			}
		},
		{	{ -40 },
			{
				{ tcu::IVec4(13),			tcu::IVec4(14),			tcu::IVec4(15),			tcu::IVec4(0)			},
				{ tcu::IVec4(-40),			tcu::IVec4(-40),		tcu::IVec4(-40),		tcu::IVec4(-40)			}
			}
		},
		{	{ -915 },
			{
				{ tcu::IVec4(1),			tcu::IVec4(5),			tcu::IVec4(10),			tcu::IVec4(15)			},
				{ tcu::IVec4(-915),			tcu::IVec4(-915),		tcu::IVec4(-915),		tcu::IVec4(-915)		}
			}
		},
		{	{ -2548675 },
			{
				{ tcu::IVec4(15),			tcu::IVec4(1),			tcu::IVec4(9),			tcu::IVec4(13)			},
				{ tcu::IVec4(-2548675),		tcu::IVec4(-2548675),	tcu::IVec4(-2548675),	tcu::IVec4(-2548675)	}
			}
		},
	};


	std::string desc = std::string(getTestTypeName(cmdType)) + " (integer)";
	de::MovePtr<tcu::TestCaseGroup>	staticTests		(new tcu::TestCaseGroup(testCtx, "static", (desc + " with static input").c_str()));
	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(testData); ++ndx)
	{
		DE_ASSERT(testData[ndx].data.positions[0].x() < MAX_POSITION);
		DE_ASSERT(testData[ndx].data.positions[1].x() < MAX_POSITION);
		DE_ASSERT(testData[ndx].data.positions[2].x() < MAX_POSITION);
		DE_ASSERT(testData[ndx].data.positions[3].x() < MAX_POSITION);

		const std::string name = "test_" + de::toString(ndx + 1);
		staticTests->addChild(new FillUpdateCopyBufferTestCase<tcu::IVec4>(
							testCtx, name.c_str(), testData[ndx].fillValue.uint, testData[ndx].data, cmdType, cmdBufferType));
	}

	/* Add a few randomized tests */
	de::MovePtr<tcu::TestCaseGroup>	randomTests		(new tcu::TestCaseGroup(testCtx, "random", (desc + " with random input").c_str()));
	const int						testCount		= 10;
	de::Random						rnd				(testCtx.getCommandLine().getBaseSeed());
	for (int ndx = 0; ndx < testCount; ++ndx)
	{
		const std::string	name		= "test_" + de::toString(ndx + 1);
		const union {
			deInt32		integer;
			deUint32	uint;
		}					fillValue	= { rnd.getInt(std::numeric_limits<deInt32>::min(), std::numeric_limits<deInt32>::max() - 1) };

		const tcu::IVec4	refValue	(fillValue.integer);
		const tcu::IVec4	v0			= tcu::IVec4(rnd.getInt(0, MAX_POSITION - 1));
		const tcu::IVec4	v1			= tcu::IVec4(rnd.getInt(0, MAX_POSITION - 1));
		const tcu::IVec4	v2			= tcu::IVec4(rnd.getInt(0, MAX_POSITION - 1));
		const tcu::IVec4	v3			= tcu::IVec4(rnd.getInt(0, MAX_POSITION - 1));

		ValidationDataIVec4	data		=
		{
			{ v0, v1, v2, v3 },
			{ refValue, refValue, refValue, refValue }
		};

		DE_ASSERT(data.positions[0].x() < MAX_POSITION);
		DE_ASSERT(data.positions[1].x() < MAX_POSITION);
		DE_ASSERT(data.positions[2].x() < MAX_POSITION);
		DE_ASSERT(data.positions[3].x() < MAX_POSITION);

		randomTests->addChild(new FillUpdateCopyBufferTestCase<tcu::IVec4>(testCtx, name.c_str(), fillValue.uint, data, cmdType, cmdBufferType));
	}

	const std::string				groupName		= getCmdBufferTypeStr(cmdBufferType);
	de::MovePtr<tcu::TestCaseGroup>	primaryGroup	(new tcu::TestCaseGroup(testCtx, groupName.c_str(), (desc + " using " + groupName.c_str() + " command buffer").c_str()));
	primaryGroup->addChild(staticTests.release());
	primaryGroup->addChild(randomTests.release());

	return primaryGroup.release();
}

tcu::TestCaseGroup*	createFillUpdateCopyBufferIntegerTests (tcu::TestContext& testCtx, CmdType cmdType)
{
	const std::string desc = std::string(getTestTypeName(cmdType)) + " (integer)";
	de::MovePtr<tcu::TestCaseGroup> testGroup (new tcu::TestCaseGroup(testCtx, "integer_buffer", desc.c_str()));
	testGroup->addChild(createFillUpdateCopyBufferIntegerTests(testCtx, cmdType, CMD_BUFFER_PRIMARY));
	testGroup->addChild(createFillUpdateCopyBufferIntegerTests(testCtx, cmdType, CMD_BUFFER_SECONDARY));
	return testGroup.release();
}

tcu::TestCaseGroup*	createFillUpdateCopyBufferUnsignedTests (tcu::TestContext& testCtx, CmdType cmdType, CmdBufferType cmdBufferType)
{
	struct {
		deUint32					fillValue;
		const ValidationDataUVec4	data;
	} testData[] = {
		{	3u,
			{
				{ tcu::IVec4(1),			tcu::IVec4(2),			tcu::IVec4(3),			tcu::IVec4(4)			},
				{ tcu::UVec4(3u),			tcu::UVec4(3u),			tcu::UVec4(3u),			tcu::UVec4(3u)			}
			}
		},
		{	18u,
			{
				{ tcu::IVec4(8),			tcu::IVec4(7),			tcu::IVec4(6),			tcu::IVec4(5)			},
				{ tcu::UVec4(18u),			tcu::UVec4(18u),		tcu::UVec4(18u),		tcu::UVec4(18u)			}
			}
		},
		{	669154u,
			{
				{ tcu::IVec4(9),			tcu::IVec4(10),			tcu::IVec4(11),			tcu::IVec4(12)			},
				{ tcu::UVec4(669154u),		tcu::UVec4(669154u),	tcu::UVec4(669154u),	tcu::UVec4(669154u)		}
			}
		},
		{	40u,
			{
				{ tcu::IVec4(13),			tcu::IVec4(14),			tcu::IVec4(15),			tcu::IVec4(0)			},
				{ tcu::UVec4(40u),			tcu::UVec4(40u),		tcu::UVec4(40u),		tcu::UVec4(40u)			}
			}
		},
		{	915u,
			{
				{ tcu::IVec4(1),			tcu::IVec4(7),			tcu::IVec4(13),			tcu::IVec4(11)			},
				{ tcu::UVec4(915u),			tcu::UVec4(915u),		tcu::UVec4(915u),		tcu::UVec4(915u)		}
			}
		},
		{	2548675u,
			{
				{ tcu::IVec4(15),			tcu::IVec4(1),			tcu::IVec4(9),			tcu::IVec4(13)			},
				{ tcu::UVec4(2548675u),		tcu::UVec4(2548675u),	tcu::UVec4(2548675u),	tcu::UVec4(2548675u)	}
			}
		},
	};

	std::string desc = std::string(getTestTypeName(cmdType)) + " (unsigned)";
	de::MovePtr<tcu::TestCaseGroup>	staticTests		(new tcu::TestCaseGroup(testCtx, "static", (desc + " with static input").c_str()));

	for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(testData); ++ndx)
	{
		DE_ASSERT(testData[ndx].data.positions[0].x() < MAX_POSITION);
		DE_ASSERT(testData[ndx].data.positions[1].x() < MAX_POSITION);
		DE_ASSERT(testData[ndx].data.positions[2].x() < MAX_POSITION);
		DE_ASSERT(testData[ndx].data.positions[3].x() < MAX_POSITION);

		const std::string name = "test_" + de::toString(ndx + 1);
		staticTests->addChild(new FillUpdateCopyBufferTestCase<tcu::UVec4>(
							testCtx, name.c_str(), testData[ndx].fillValue, testData[ndx].data, cmdType, cmdBufferType));
	}

	/* Add a few randomized tests */
	de::MovePtr<tcu::TestCaseGroup>	randomTests		(new tcu::TestCaseGroup(testCtx, "random", (desc + " with random input").c_str()));
	const int						testCount		= 10;
	de::Random						rnd				(testCtx.getCommandLine().getBaseSeed());
	for (int ndx = 0; ndx < testCount; ++ndx)
	{
		const std::string	name		= "test_" + de::toString(ndx + 1);
		const deUint32		fillValue	= rnd.getUint32();
		const tcu::UVec4	refValue	(fillValue);
		const tcu::IVec4	v0			= tcu::IVec4(rnd.getInt(0, MAX_POSITION - 1));
		const tcu::IVec4	v1			= tcu::IVec4(rnd.getInt(0, MAX_POSITION - 1));
		const tcu::IVec4	v2			= tcu::IVec4(rnd.getInt(0, MAX_POSITION - 1));
		const tcu::IVec4	v3			= tcu::IVec4(rnd.getInt(0, MAX_POSITION - 1));

		ValidationDataUVec4	data		=
		{
			{ v0, v1, v2, v3 },
			{ refValue, refValue, refValue, refValue }
		};

		DE_ASSERT(data.positions[0].x() < MAX_POSITION);
		DE_ASSERT(data.positions[1].x() < MAX_POSITION);
		DE_ASSERT(data.positions[2].x() < MAX_POSITION);
		DE_ASSERT(data.positions[3].x() < MAX_POSITION);

		randomTests->addChild(new FillUpdateCopyBufferTestCase<tcu::UVec4>(testCtx, name.c_str(), fillValue, data, cmdType, cmdBufferType));
	}

	const std::string				groupName		= getCmdBufferTypeStr(cmdBufferType);
	de::MovePtr<tcu::TestCaseGroup>	testGroup	(new tcu::TestCaseGroup(testCtx, groupName.c_str(), (desc + " using " + groupName.c_str() + " command buffer").c_str()));
	testGroup->addChild(staticTests.release());
	testGroup->addChild(randomTests.release());

	return testGroup.release();
}

tcu::TestCaseGroup*	createFillUpdateCopyBufferUnsignedTests (tcu::TestContext& testCtx, CmdType cmdType)
{
	const std::string desc = std::string(getTestTypeName(cmdType)) + " (unsinged)";
	de::MovePtr<tcu::TestCaseGroup> testGroup (new tcu::TestCaseGroup(testCtx, "unsigned_buffer", desc.c_str()));
	testGroup->addChild(createFillUpdateCopyBufferUnsignedTests(testCtx, cmdType, CMD_BUFFER_PRIMARY));
	testGroup->addChild(createFillUpdateCopyBufferUnsignedTests(testCtx, cmdType, CMD_BUFFER_SECONDARY));
	return testGroup.release();
}

} // anonymous

tcu::TestCaseGroup*	createFillBufferTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> testGroup (new tcu::TestCaseGroup(testCtx, "fill", "Fill Buffer Tests"));

	testGroup->addChild(createFillUpdateCopyBufferFloatTests(testCtx, FILL_BUFFER));
	testGroup->addChild(createFillUpdateCopyBufferIntegerTests(testCtx, FILL_BUFFER));
	testGroup->addChild(createFillUpdateCopyBufferUnsignedTests(testCtx, FILL_BUFFER));

	return testGroup.release();
}

tcu::TestCaseGroup*	createUpdateBufferTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> updateTests (new tcu::TestCaseGroup(testCtx, "update", "Update Buffer Tests"));

	updateTests->addChild(createFillUpdateCopyBufferFloatTests(testCtx, UPDATE_BUFFER));
	updateTests->addChild(createFillUpdateCopyBufferIntegerTests(testCtx, UPDATE_BUFFER));
	updateTests->addChild(createFillUpdateCopyBufferUnsignedTests(testCtx, UPDATE_BUFFER));

	return updateTests.release();
}

tcu::TestCaseGroup*	createCopyBufferTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> copyTests (new tcu::TestCaseGroup(testCtx, "copy", "Copy Buffer Tests"));

	copyTests->addChild(createFillUpdateCopyBufferFloatTests(testCtx, COPY_BUFFER));
	copyTests->addChild(createFillUpdateCopyBufferIntegerTests(testCtx, COPY_BUFFER));
	copyTests->addChild(createFillUpdateCopyBufferUnsignedTests(testCtx, COPY_BUFFER));

	return copyTests.release();
}

} // ProtectedMem
} // vkt
