/*------------------------------------------------------------------------
 *  Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
 * \brief Vulkan Buffers Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiBuffersTests.hpp"

#include "deStringUtil.hpp"
#include "gluVarType.hpp"
#include "tcuTestLog.hpp"
#include "vkPrograms.hpp"
#include "vktTestCase.hpp"

namespace vkt
{

using namespace vk;

namespace api
{

namespace
{

struct BufferCaseParameters
{
	VkBufferUsageFlags	usage;
	VkBufferCreateFlags	flags;
	VkSharingMode		sharingMode;
};

class BufferTestInstance : public TestInstance
{
public:
								BufferTestInstance			(Context&				ctx,
															 BufferCaseParameters	testCase)
									: TestInstance	(ctx)
									, m_testCase	(testCase)
								{}
	virtual tcu::TestStatus		iterate						(void);
	tcu::TestStatus				bufferCreateAndAllocTest	(VkDeviceSize		size);

private:
	BufferCaseParameters		m_testCase;
};

class BuffersTestCase : public TestCase
{
public:
							BuffersTestCase		(tcu::TestContext&		testCtx,
												 const std::string&		name,
												 const std::string&		description,
												 BufferCaseParameters	testCase)
								: TestCase(testCtx, name, description)
								, m_testCase(testCase)
							{}

	virtual					~BuffersTestCase	(void) {}
	virtual TestInstance*	createInstance		(Context&				ctx) const
							{
								tcu::TestLog& log	= m_testCtx.getLog();
								log << tcu::TestLog::Message << getBufferUsageFlagsStr(m_testCase.usage) << tcu::TestLog::EndMessage;
								return new BufferTestInstance(ctx, m_testCase);
							}

private:
	BufferCaseParameters		m_testCase;
};

 tcu::TestStatus BufferTestInstance::bufferCreateAndAllocTest (VkDeviceSize size)
{
	const VkDevice			vkDevice	= m_context.getDevice();
	const DeviceInterface&	vk			= m_context.getDeviceInterface();
	VkBuffer				testBuffer;
	VkMemoryRequirements	memReqs;
	VkDeviceMemory			memory;

	// Create buffer
	{
		const deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
		const VkBufferCreateInfo		bufferParams		=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			DE_NULL,
			m_testCase.flags,
			size,
			m_testCase.usage,
			m_testCase.sharingMode,
			1u,										//	deUint32			queueFamilyCount;
			&queueFamilyIndex,
		};

		if (vk.createBuffer(vkDevice, &bufferParams, (const VkAllocationCallbacks*)DE_NULL, &testBuffer) != VK_SUCCESS)
			return tcu::TestStatus::fail("Buffer creation failed! (requested memory size: " + de::toString(size) + ")");

		vk.getBufferMemoryRequirements(vkDevice, testBuffer, &memReqs);

		if (size > memReqs.size)
		{
			std::ostringstream errorMsg;
			errorMsg << "Requied memory size (" << memReqs.size << " bytes) smaller than the buffer's size (" << size << " bytes)!";
			return tcu::TestStatus::fail(errorMsg.str());
		}
	}

	// Allocate and bind memory
	{
		const VkMemoryAllocateInfo memAlloc =
		{
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			NULL,
			memReqs.size,
			0										//	deUint32		memoryTypeIndex
		};

		if (vk.allocateMemory(vkDevice, &memAlloc, (const VkAllocationCallbacks*)DE_NULL, &memory) != VK_SUCCESS)
			return tcu::TestStatus::fail("Alloc memory failed! (requested memory size: " + de::toString(size) + ")");

		if (vk.bindBufferMemory(vkDevice, testBuffer, memory, 0) != VK_SUCCESS)
			return tcu::TestStatus::fail("Bind buffer memory failed! (requested memory size: " + de::toString(size) + ")");
	}

	vk.freeMemory(vkDevice, memory, (const VkAllocationCallbacks*)DE_NULL);
	vk.destroyBuffer(vkDevice, testBuffer, (const VkAllocationCallbacks*)DE_NULL);

	return tcu::TestStatus::pass("Buffer test");
}

tcu::TestStatus BufferTestInstance::iterate (void)
{
	const VkDeviceSize testSizes[] =
	{
		0,
		1181,
		15991,
		16384
	};
	tcu::TestStatus	testStatus					= tcu::TestStatus::pass("Buffer test");

	for (int i = 0; i < DE_LENGTH_OF_ARRAY(testSizes); i++)
	{
		if ((testStatus = bufferCreateAndAllocTest(testSizes[i])).getCode() != QP_TEST_RESULT_PASS)
			return testStatus;
	}

	if (m_testCase.usage & (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
	{
		const VkPhysicalDevice		vkPhysicalDevice	= m_context.getPhysicalDevice();
		const InstanceInterface&	vkInstance			= m_context.getInstanceInterface();
		VkPhysicalDeviceProperties	props;

		vkInstance.getPhysicalDeviceProperties(vkPhysicalDevice, &props);

		testStatus = bufferCreateAndAllocTest(props.limits.maxTexelBufferElements);
	}

	return testStatus;
}

} // anonymous

 tcu::TestCaseGroup* createBufferTests (tcu::TestContext&	testCtx)
{
	const VkBufferUsageFlags bufferUsageModes[] =
	{
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
		VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
	};

	const VkBufferCreateFlags bufferCreateFlags[] =
	{
		VK_BUFFER_CREATE_SPARSE_BINDING_BIT,
		VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT,
		VK_BUFFER_CREATE_SPARSE_ALIASED_BIT
	};

	de::MovePtr<tcu::TestCaseGroup>	buffersTests	(new tcu::TestCaseGroup(testCtx, "buffers", "Buffers Tests"));

	deUint32	numberOfBufferUsageFlags			= DE_LENGTH_OF_ARRAY(bufferUsageModes);
	deUint32	numberOfBufferCreateFlags			= DE_LENGTH_OF_ARRAY(bufferCreateFlags);
	deUint32	maximumValueOfBufferUsageFlags		= (1 << (numberOfBufferUsageFlags - 1)) - 1;
	deUint32	maximumValueOfBufferCreateFlags		= (1 << (numberOfBufferCreateFlags)) - 1;

	for (deUint32 combinedBufferCreateFlags = 0; combinedBufferCreateFlags < maximumValueOfBufferCreateFlags; combinedBufferCreateFlags++)
	{
		for (deUint32 combinedBufferUsageFlags = 0; combinedBufferUsageFlags < maximumValueOfBufferUsageFlags; combinedBufferUsageFlags++)
		{
			BufferCaseParameters	testParams =
			{
				combinedBufferUsageFlags,
				combinedBufferCreateFlags,
				VK_SHARING_MODE_EXCLUSIVE
			};
			std::ostringstream	testName;
			std::ostringstream	testDescription;
			testName << "createBuffer_" << combinedBufferUsageFlags << "_" << combinedBufferCreateFlags;
			testDescription << "vkCreateBuffer test " << combinedBufferUsageFlags << " " << combinedBufferCreateFlags;
			buffersTests->addChild(new BuffersTestCase(testCtx, testName.str(), testDescription.str(), testParams));
		}
	}

	return buffersTests.release();
}

} // api
} // vk
