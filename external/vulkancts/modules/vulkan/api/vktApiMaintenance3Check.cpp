/*-------------------------------------------------------------------------
* Vulkan Conformance Tests
* ------------------------
*
* Copyright (c) 2017 Khronos Group
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
* \brief API Maintenance3 Check test - checks structs and function from VK_KHR_maintenance3
*//*--------------------------------------------------------------------*/

#include "tcuTestLog.hpp"

#include "vkQueryUtil.hpp"

#include "vktApiMaintenance3Check.hpp"
#include "vktTestCase.hpp"

#define VK_DESCRIPTOR_TYPE_LAST (VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT + 1)

using namespace vk;

namespace vkt
{

namespace api
{

namespace
{
using ::std::string;
using ::std::vector;

typedef vk::VkPhysicalDeviceProperties DevProp1;
typedef vk::VkPhysicalDeviceProperties2  DevProp2;
typedef vk::VkPhysicalDeviceMaintenance3Properties MainDevProp3;


void										checkSupport							(const Context& m_context)
{
	const vector<string>					extensions								= m_context.getDeviceExtensions();

	if (!isDeviceExtensionSupported(m_context.getUsedApiVersion(), m_context.getDeviceExtensions(), "VK_KHR_maintenance3"))
		TCU_THROW(NotSupportedError, "VK_KHR_maintenance3 extension is not supported");
}

class Maintenance3StructTestInstance : public TestInstance
{
public:
											Maintenance3StructTestInstance			(Context& ctx)
												: TestInstance						(ctx)
	{}
	virtual tcu::TestStatus					iterate									(void)
	{
		checkSupport(m_context);
		tcu::TestLog&						log										= m_context.getTestContext().getLog();

		// these variables are equal to minimal values for maxMemoryAllocationSize and maxPerSetDescriptors
		const deUint32						maxMemoryAllocationSize					= 1073741824u;
		const deUint32						maxDescriptorsInSet						= 1024u;

		// set values to be a bit smaller than required minimum values
		MainDevProp3 mainProp3 =
		{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES,				//VkStructureType						sType;
			DE_NULL,																//void*									pNext;
			maxDescriptorsInSet - 1u,												//deUint32								maxPerSetDescriptors;
			maxMemoryAllocationSize - 1u											//VkDeviceSize							maxMemoryAllocationSize;
		};

		DevProp2 prop2;
		deMemset(&prop2, 0, sizeof(prop2)); // zero the structure
		prop2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		prop2.pNext = &mainProp3;

		m_context.getInstanceInterface().getPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &prop2);

		if (mainProp3.maxMemoryAllocationSize < maxMemoryAllocationSize)
			return tcu::TestStatus::fail("Fail");

		if (mainProp3.maxPerSetDescriptors < maxDescriptorsInSet)
			return tcu::TestStatus::fail("Fail");

		log << tcu::TestLog::Message << "maxMemoryAllocationSize: "	<< mainProp3.maxMemoryAllocationSize	<< tcu::TestLog::EndMessage;
		log << tcu::TestLog::Message << "maxPerSetDescriptors: "	<< mainProp3.maxPerSetDescriptors		<< tcu::TestLog::EndMessage;
		return tcu::TestStatus::pass("Pass");
	}
};

class Maintenance3StructTestCase : public TestCase
{
public:
											Maintenance3StructTestCase				(tcu::TestContext&	testCtx)
												: TestCase(testCtx, "maintenance3_properties", "tests VkPhysicalDeviceMaintenance3Properties struct")
	{}

	virtual									~Maintenance3StructTestCase				(void)
	{}
	virtual TestInstance*					createInstance							(Context&	ctx) const
	{
		return new Maintenance3StructTestInstance(ctx);
	}

private:
};

class Maintenance3DescriptorTestInstance : public TestInstance
{
public:
											Maintenance3DescriptorTestInstance		(Context&	ctx)
												: TestInstance(ctx)
	{}
	virtual tcu::TestStatus					iterate									(void)
	{
		checkSupport(m_context);

		// these variables are equal to minimal values for maxMemoryAllocationSize and maxPerSetDescriptors
		const deUint32						maxMemoryAllocationSize					= 1073741824u;
		const deUint32						maxDescriptorsInSet						= 1024u;

		MainDevProp3						mainProp3								=
		{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES,				//VkStructureType						sType;
			DE_NULL,																//void*									pNext;
			maxDescriptorsInSet,													//deUint32								maxPerSetDescriptors;
			maxMemoryAllocationSize													//VkDeviceSize							maxMemoryAllocationSize;
		};

		DevProp2							prop2									=
		{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,							//VkStructureType						sType;
			&mainProp3,																//void*									pNext;
			VkPhysicalDeviceProperties()											//VkPhysicalDeviceProperties			properties;
		};

		DevProp1							prop1;

		m_context.getInstanceInterface().getPhysicalDeviceProperties(m_context.getPhysicalDevice(), &prop1);
		m_context.getInstanceInterface().getPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &prop2);

		// setup for descriptors sets
		VkDescriptorSetLayoutBinding		descriptorSetLayoutBinding[VK_DESCRIPTOR_TYPE_LAST];

		for (deUint32 ndx = 0u; ndx < VK_DESCRIPTOR_TYPE_LAST; ++ndx)
		{
			descriptorSetLayoutBinding[ndx].binding									= ndx;
			descriptorSetLayoutBinding[ndx].descriptorType							= static_cast<VkDescriptorType>(ndx);
			descriptorSetLayoutBinding[ndx].descriptorCount							= mainProp3.maxPerSetDescriptors;
			descriptorSetLayoutBinding[ndx].stageFlags								= VK_SHADER_STAGE_ALL;
			descriptorSetLayoutBinding[ndx].pImmutableSamplers						= DE_NULL;
		}

		// VkDescriptorSetLayoutCreateInfo setup
		vk::VkDescriptorSetLayoutCreateInfo	pCreateInfo								=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,					//VkStructureType						sType;
			DE_NULL,																//const void*							pNext;
			0u,																		//VkDescriptorSetLayoutCreateFlags		flags;
			1u,																		//deUint32								bindingCount;
			DE_NULL																	//const VkDescriptorSetLayoutBinding*	pBindings;
		};

		// VkDescriptorSetLayoutSupport setup
		vk::VkDescriptorSetLayoutSupport	pSupport								=
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT,						//VkStructureType						sType;
			DE_NULL,																//void*									pNext;
			VK_FALSE																//VkBool32								supported;
		};

		// check for single descriptors
		for (deUint32 ndx = 0u; ndx < VK_DESCRIPTOR_TYPE_LAST; ++ndx)
		{
			pCreateInfo.pBindings = &descriptorSetLayoutBinding[ndx];
			m_context.getDeviceInterface().getDescriptorSetLayoutSupport(m_context.getDevice(), &pCreateInfo, &pSupport);

			if(extraLimitCheck(descriptorSetLayoutBinding, ndx, pCreateInfo.bindingCount, prop1))
			{
				if (pSupport.supported == VK_FALSE)
					return tcu::TestStatus::fail("fail");
			}
		}

		// check for accumulated descriptors (all eleven types)

		pCreateInfo.pBindings = &descriptorSetLayoutBinding[0u];
		pCreateInfo.bindingCount = static_cast<deUint32>(VK_DESCRIPTOR_TYPE_LAST);

		deUint32 fraction = mainProp3.maxPerSetDescriptors / static_cast<deUint32>(VK_DESCRIPTOR_TYPE_LAST);
		deUint32 rest = mainProp3.maxPerSetDescriptors % static_cast<deUint32>(VK_DESCRIPTOR_TYPE_LAST);

		for (deUint32 ndx = 0u; ndx < VK_DESCRIPTOR_TYPE_LAST; ++ndx)
			descriptorSetLayoutBinding[ndx].descriptorCount = fraction;
		descriptorSetLayoutBinding[0u].descriptorCount += rest;

		m_context.getDeviceInterface().getDescriptorSetLayoutSupport(m_context.getDevice(), &pCreateInfo, &pSupport);

		if (extraLimitCheck(descriptorSetLayoutBinding, 0u, pCreateInfo.bindingCount, prop1))
		{
			if (pSupport.supported == VK_FALSE)
				return tcu::TestStatus::fail("fail");
		}

		return tcu::TestStatus::pass("Pass");
	}

private:
	bool									extraLimitCheck							(const VkDescriptorSetLayoutBinding* descriptorSetLayoutBinding, const deUint32& curNdx, const deUint32& size, const DevProp1& prop1)
	{
		deUint32							maxPerStageDescriptorSamplers			= 0u;
		deUint32							maxPerStageDescriptorUniformBuffers		= 0u;
		deUint32							maxPerStageDescriptorStorageBuffers		= 0u;
		deUint32							maxPerStageDescriptorSampledImages		= 0u;
		deUint32							maxPerStageDescriptorStorageImages		= 0u;
		deUint32							maxPerStageDescriptorInputAttachments	= 0u;

		for(deUint32 ndx = curNdx; ndx < curNdx + size; ++ndx)
		{
			if ((descriptorSetLayoutBinding[ndx].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER) ||
				(descriptorSetLayoutBinding[ndx].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER))
				maxPerStageDescriptorSamplers += descriptorSetLayoutBinding->descriptorCount;

			if ((descriptorSetLayoutBinding[ndx].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) ||
					(descriptorSetLayoutBinding[ndx].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC))
				maxPerStageDescriptorUniformBuffers += descriptorSetLayoutBinding->descriptorCount;

			if ((descriptorSetLayoutBinding[ndx].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) ||
					(descriptorSetLayoutBinding[ndx].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC))
				maxPerStageDescriptorStorageBuffers += descriptorSetLayoutBinding->descriptorCount;

			if ((descriptorSetLayoutBinding[ndx].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)	||
					(descriptorSetLayoutBinding[ndx].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)		||
					(descriptorSetLayoutBinding[ndx].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER))
				maxPerStageDescriptorSampledImages += descriptorSetLayoutBinding->descriptorCount;

			if ((descriptorSetLayoutBinding[ndx].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) ||
					(descriptorSetLayoutBinding[ndx].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER))
				maxPerStageDescriptorStorageImages += descriptorSetLayoutBinding->descriptorCount;

			if (descriptorSetLayoutBinding[ndx].descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
				maxPerStageDescriptorInputAttachments += descriptorSetLayoutBinding->descriptorCount;
		}

		if (prop1.limits.maxPerStageDescriptorSamplers < maxPerStageDescriptorSamplers)
			return false;
		if (prop1.limits.maxPerStageDescriptorUniformBuffers < maxPerStageDescriptorUniformBuffers)
			return false;
		if (prop1.limits.maxPerStageDescriptorStorageBuffers < maxPerStageDescriptorStorageBuffers)
			return false;
		if (prop1.limits.maxPerStageDescriptorSampledImages < maxPerStageDescriptorSampledImages)
			return false;
		if (prop1.limits.maxPerStageDescriptorStorageImages < maxPerStageDescriptorStorageImages)
			return false;
		if (prop1.limits.maxPerStageDescriptorInputAttachments < maxPerStageDescriptorInputAttachments)
			return false;

		return true;
	}
};

class Maintenance3DescriptorTestCase : public TestCase
{

public:
											Maintenance3DescriptorTestCase			(tcu::TestContext&	testCtx)
												: TestCase(testCtx, "descriptor_set", "tests vkGetDescriptorSetLayoutSupport struct")
	{}
	virtual									~Maintenance3DescriptorTestCase			(void)
	{}
	virtual TestInstance*					createInstance							(Context&	ctx) const
	{
		return new Maintenance3DescriptorTestInstance(ctx);
	}

private:
};

} // anonymous

	tcu::TestCaseGroup*						createMaintenance3Tests					(tcu::TestContext&	testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	main3Tests(new tcu::TestCaseGroup(testCtx, "maintenance3_check", "Maintenance3 Tests"));
	main3Tests->addChild(new Maintenance3StructTestCase(testCtx));
	main3Tests->addChild(new Maintenance3DescriptorTestCase(testCtx));

	return main3Tests.release();
}

} // api
} // vkt
