/*------------------------------------------------------------------------
 *  Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
 * \brief Vulkan Buffer View Creation Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiBufferViewCreateTests.hpp"

#include "deStringUtil.hpp"
#include "gluVarType.hpp"
#include "tcuTestLog.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vktTestCase.hpp"

namespace vkt
{

using namespace vk;

namespace api
{

namespace
{

struct BufferViewCaseParameters
{
	VkFormat				format;
	VkDeviceSize			offset;
	VkDeviceSize			range;
	VkBufferUsageFlags		usage;
	VkFormatFeatureFlags	features;
};

class BufferViewTestInstance : public TestInstance
{
public:
								BufferViewTestInstance		(Context&					ctx,
															 BufferViewCaseParameters	createInfo);
	virtual tcu::TestStatus		iterate						(void);

private:
	BufferViewCaseParameters		m_testCase;
};

class BufferViewTestCase : public TestCase
{
public:
							BufferViewTestCase		(tcu::TestContext&			testCtx,
													 const std::string&			name,
													 const std::string&			description,
													 BufferViewCaseParameters	createInfo)
								: TestCase			(testCtx, name, description)
								, m_testCase		(createInfo)
							{}

	virtual					~BufferViewTestCase		(void) {}
	virtual TestInstance*	createInstance			(Context&	ctx) const
							{
								return new BufferViewTestInstance(ctx, m_testCase);
							}

private:
	BufferViewCaseParameters	m_testCase;
};

BufferViewTestInstance::BufferViewTestInstance (Context&					ctx,
												BufferViewCaseParameters	createInfo)
	: TestInstance	(ctx)
	, m_testCase	(createInfo)
{
}

tcu::TestStatus BufferViewTestInstance::iterate (void)
{
	// Create buffer
	const VkDevice				vkDevice				= m_context.getDevice();
	const DeviceInterface&		vk						= m_context.getDeviceInterface();
	const deUint32				queueFamilyIndex		= m_context.getUniversalQueueFamilyIndex();
	const VkDeviceSize			size					= 3 * 5 * 7 * 64;
	Move<VkBuffer>				testBuffer;
	VkMemoryRequirements		memReqs;
	VkFormatProperties			properties;
	const VkBufferCreateInfo	bufferParams =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,													//	VkStructureType			sType;
		DE_NULL,																				//	const void*				pNext;
		0u,																						//	VkBufferCreateFlags		flags;
		size,																					//	VkDeviceSize			size;
		m_testCase.usage,																		//	VkBufferUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,																//	VkSharingMode			sharingMode;
		1u,																						//	deUint32				queueFamilyCount;
		&queueFamilyIndex,																		//	const deUint32*			pQueueFamilyIndices;
	};

	m_context.getInstanceInterface().getPhysicalDeviceFormatProperties(m_context.getPhysicalDevice(), m_testCase.format, &properties);
	if (!(properties.bufferFeatures & m_testCase.features))
		TCU_THROW(NotSupportedError, "Format not supported");

	try
	{
		testBuffer = createBuffer(vk, vkDevice, &bufferParams, (const VkAllocationCallbacks*)DE_NULL);
	}
	catch (const vk::Error& error)
	{
		return tcu::TestStatus::fail("Buffer creation failed! (Error code: " + de::toString(error.getMessage()) + ")");
	}

	vk.getBufferMemoryRequirements(vkDevice, *testBuffer, &memReqs);

	if (size > memReqs.size)
	{
		std::ostringstream errorMsg;
		errorMsg << "Requied memory size (" << memReqs.size << " bytes) smaller than the buffer's size (" << size << " bytes)!";
		return tcu::TestStatus::fail(errorMsg.str());
	}

	Move<VkDeviceMemory>		memory;
	const VkMemoryAllocateInfo	memAlloc				=
	{
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,		//	VkStructureType		sType
		NULL,										//	const void*			pNext
		memReqs.size,								//	VkDeviceSize		allocationSize
		(deUint32)deCtz32(memReqs.memoryTypeBits)	//	deUint32			memoryTypeIndex
	};

	{
		// Create buffer view.
		Move<VkBufferView>				bufferView;
		const VkBufferViewCreateInfo	bufferViewCreateInfo	=
		{
			VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,	//	VkStructureType		sType;
			NULL,										//	const void*			pNext;
			(VkBufferViewCreateFlags)0,
			*testBuffer,								//	VkBuffer			buffer;
			m_testCase.format,							//	VkFormat			format;
			m_testCase.offset,							//	VkDeviceSize		offset;
			m_testCase.range,							//	VkDeviceSize		range;
		};

		try
		{
			memory = allocateMemory(vk, vkDevice, &memAlloc, (const VkAllocationCallbacks*)DE_NULL);
		}
		catch (const vk::Error& error)
		{
			return tcu::TestStatus::fail("Alloc memory failed! (Error code: " + de::toString(error.getMessage()) + ")");
		}

		if (vk.bindBufferMemory(vkDevice, *testBuffer, *memory, 0) != VK_SUCCESS)
			return tcu::TestStatus::fail("Bind buffer memory failed!");

		try
		{
			bufferView = createBufferView(vk, vkDevice, &bufferViewCreateInfo, (const VkAllocationCallbacks*)DE_NULL);
		}
		catch (const vk::Error& error)
		{
			return tcu::TestStatus::fail("Buffer View creation failed! (Error code: " + de::toString(error.getMessage()) + ")");
		}
	}

	// Testing complete view size.
	{
		Move<VkBufferView>		completeBufferView;
		VkBufferViewCreateInfo	completeBufferViewCreateInfo	=
		{
			VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,	//	VkStructureType		sType;
			NULL,										//	const void*			pNext;
			(VkBufferViewCreateFlags)0,
			*testBuffer,								//	VkBuffer			buffer;
			m_testCase.format,							//	VkFormat			format;
			m_testCase.offset,							//	VkDeviceSize		offset;
			size,										//	VkDeviceSize		range;
		};

		try
		{
			completeBufferView = createBufferView(vk, vkDevice, &completeBufferViewCreateInfo, (const VkAllocationCallbacks*)DE_NULL);
		}
		catch (const vk::Error& error)
		{
			return tcu::TestStatus::fail("Buffer View creation failed! (Error code: " + de::toString(error.getMessage()) + ")");
		}
	}

	return tcu::TestStatus::pass("BufferView test");
}

} // anonymous

 tcu::TestCaseGroup* createBufferViewCreateTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	bufferViewTests	(new tcu::TestCaseGroup(testCtx, "create", "BufferView Construction Tests"));

	const VkDeviceSize range = VK_WHOLE_SIZE;
	for (deUint32 format = VK_FORMAT_UNDEFINED + 1; format < VK_FORMAT_LAST; format++)
	{
		std::ostringstream	testName;
		std::ostringstream	testDescription;
		testName << "createBufferView_" << format;
		testDescription << "vkBufferView test " << testName.str();
		{
			BufferViewCaseParameters testParams	=
			{
				(VkFormat)format,							// VkFormat				format;
				0,											// VkDeviceSize			offset;
				range,										// VkDeviceSize			range;
				VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,	// VkBufferUsageFlags	usage;
				VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT, // VkFormatFeatureFlags flags;
			};
			bufferViewTests->addChild(new BufferViewTestCase(testCtx, testName.str() + "_uniform", testDescription.str(), testParams));
		}
		{
			BufferViewCaseParameters testParams	=
			{
				(VkFormat)format,							// VkFormat				format;
				0,											// VkDeviceSize			offset;
				range,										// VkDeviceSize			range;
				VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT,	// VkBufferUsageFlags	usage;
				VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT, // VkFormatFeatureFlags flags;
			};
			bufferViewTests->addChild(new BufferViewTestCase(testCtx, testName.str() + "_storage", testDescription.str(), testParams));
		}
	}

	return bufferViewTests.release();
}

} // api
} // vk
