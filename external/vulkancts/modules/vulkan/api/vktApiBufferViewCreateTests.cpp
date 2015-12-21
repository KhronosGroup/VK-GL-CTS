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
 * \brief Vulkan Buffer View Creation Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiBufferViewCreateTests.hpp"

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
	const VkDeviceSize			size					= 16 * 1024;
	VkBuffer					testBuffer;
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

	if (vk.createBuffer(vkDevice, &bufferParams, (const VkAllocationCallbacks*)DE_NULL, &testBuffer) != VK_SUCCESS)
		return tcu::TestStatus::fail("Buffer creation failed!");

	vk.getBufferMemoryRequirements(vkDevice, testBuffer, &memReqs);

	if (size > memReqs.size)
	{
		std::ostringstream errorMsg;
		errorMsg << "Requied memory size (" << memReqs.size << " bytes) smaller than the buffer's size (" << size << " bytes)!";
		return tcu::TestStatus::fail(errorMsg.str());
	}

	VkDeviceMemory				memory;
	const VkMemoryAllocateInfo	memAlloc				=
	{
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,		//	VkStructureType		sType
		NULL,										//	const void*			pNext
		memReqs.size,								//	VkDeviceSize		allocationSize
		0											//	deUint32			memoryTypeIndex
	};

	{
		// Create buffer view.
		VkBufferView					bufferView;
		const VkBufferViewCreateInfo	bufferViewCreateInfo	=
		{
			VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,	//	VkStructureType		sType;
			NULL,										//	const void*			pNext;
			(VkBufferViewCreateFlags)0,
			testBuffer,									//	VkBuffer			buffer;
			m_testCase.format,							//	VkFormat			format;
			m_testCase.offset,							//	VkDeviceSize		offset;
			m_testCase.range,							//	VkDeviceSize		range;
		};

		if (vk.allocateMemory(vkDevice, &memAlloc, (const VkAllocationCallbacks*)DE_NULL, &memory) != VK_SUCCESS)
			return tcu::TestStatus::fail("Alloc memory failed!");

		if (vk.bindBufferMemory(vkDevice, testBuffer, memory, 0) != VK_SUCCESS)
			return tcu::TestStatus::fail("Bind buffer memory failed!");

		if (vk.createBufferView(vkDevice, &bufferViewCreateInfo, (const VkAllocationCallbacks*)DE_NULL, &bufferView) != VK_SUCCESS)
			return tcu::TestStatus::fail("Buffer View creation failed!");

		vk.destroyBufferView(vkDevice, bufferView, (const VkAllocationCallbacks*)DE_NULL);
	}

	// Testing complete view size.
	{
		VkBufferView			completeBufferView;
		VkBufferViewCreateInfo	completeBufferViewCreateInfo	=
		{
			VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,	//	VkStructureType		sType;
			NULL,										//	const void*			pNext;
			(VkBufferViewCreateFlags)0,
			testBuffer,									//	VkBuffer			buffer;
			m_testCase.format,							//	VkFormat			format;
			m_testCase.offset,							//	VkDeviceSize		offset;
			size,										//	VkDeviceSize		range;
		};

		if (vk.createBufferView(vkDevice, &completeBufferViewCreateInfo, (const VkAllocationCallbacks*)DE_NULL, &completeBufferView) != VK_SUCCESS)
			return tcu::TestStatus::fail("Buffer View creation failed!");

		vk.destroyBufferView(vkDevice, completeBufferView, (const VkAllocationCallbacks*)DE_NULL);
	}

	vk.freeMemory(vkDevice, memory, (const VkAllocationCallbacks*)DE_NULL);
	vk.destroyBuffer(vkDevice, testBuffer, (const VkAllocationCallbacks*)DE_NULL);

	return tcu::TestStatus::pass("BufferView test");
}

} // anonymous

 tcu::TestCaseGroup* createBufferViewCreateTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	bufferViewTests	(new tcu::TestCaseGroup(testCtx, "create", "BufferView Construction Tests"));

	const VkDeviceSize range = 96;
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
