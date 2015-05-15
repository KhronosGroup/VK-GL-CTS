/*-------------------------------------------------------------------------
 * drawElements Quality Program Vulkan Module
 * --------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief API Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiTests.hpp"

#include "vktTestCaseUtil.hpp"

#include "vkDefs.hpp"
#include "vkPlatform.hpp"
#include "vkStrUtil.hpp"
#include "vkRef.hpp"

#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"

#include "deUniquePtr.hpp"

#include "qpInfo.h"

namespace vkt
{
namespace api
{

// \todo [2015-05-11 pyry] Temporary for testing framework

using namespace vk;
using std::vector;
using tcu::TestLog;

tcu::TestStatus createSampler (Context& context)
{
	const struct VkApplicationInfo		appInfo			=
	{
		VK_STRUCTURE_TYPE_APPLICATION_INFO,		//	VkStructureType	sType;
		DE_NULL,								//	const void*		pNext;
		"deqp",									//	const char*		pAppName;
		qpGetReleaseId(),						//	deUint32		appVersion;
		"deqp",									//	const char*		pEngineName;
		qpGetReleaseId(),						//	deUint32		engineVersion;
		VK_API_VERSION							//	deUint32		apiVersion;
	};
	const struct VkInstanceCreateInfo	instanceInfo	=
	{
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,	// VkStructureType				sType;
		DE_NULL,								//	const void*					pNext;
		&appInfo,								//	const VkApplicationInfo*	pAppInfo;
		DE_NULL,								//	const VkAllocCallbacks*		pAllocCb;
		0u,										//	deUint32					extensionCount;
		DE_NULL									//	const char*const*			ppEnabledExtensionNames;
	};

	const PlatformInterface&	vkPlatform	= context.getPlatformInterface();
	TestLog&					log			= context.getTestContext().getLog();
	const Unique<VkInstanceT>	instance	(createInstance(vkPlatform, &instanceInfo));
	vector<VkPhysicalDevice>	devices;
	deUint32					numDevices	= 0;

	VK_CHECK(vkPlatform.enumeratePhysicalDevices(*instance, &numDevices, DE_NULL));

	if (numDevices > 0)
	{
		devices.resize(numDevices);
		VK_CHECK(vkPlatform.enumeratePhysicalDevices(*instance, &numDevices, &devices[0]));

		for (deUint32 deviceNdx = 0; deviceNdx < numDevices; deviceNdx++)
		{
			const struct VkDeviceQueueCreateInfo	queueInfo	=
			{
				0u,										//	deUint32	queueNodeIndex;
				1u,										//	deUint32	queueCount;
			};
			const struct VkDeviceCreateInfo			deviceInfo	=
			{
				VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,	//	VkStructureType					sType;
				DE_NULL,								//	const void*						pNext;
				1u,										//	deUint32						queueRecordCount;
				&queueInfo,								//	const VkDeviceQueueCreateInfo*	pRequestedQueues;
				0u,										//	deUint32						extensionCount;
				DE_NULL,								//	const char*const*				ppEnabledExtensionNames;
				0u,										//	VkDeviceCreateFlags				flags;
			};

			const struct VkSamplerCreateInfo		samplerInfo	=
			{
				VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,	//	VkStructureType	sType;
				DE_NULL,								//	const void*		pNext;
				VK_TEX_FILTER_NEAREST,					//	VkTexFilter		magFilter;
				VK_TEX_FILTER_NEAREST,					//	VkTexFilter		minFilter;
				VK_TEX_MIPMAP_MODE_BASE,				//	VkTexMipmapMode	mipMode;
				VK_TEX_ADDRESS_CLAMP,					//	VkTexAddress	addressU;
				VK_TEX_ADDRESS_CLAMP,					//	VkTexAddress	addressV;
				VK_TEX_ADDRESS_CLAMP,					//	VkTexAddress	addressW;
				0.0f,									//	float			mipLodBias;
				0u,										//	deUint32		maxAnisotropy;
				VK_COMPARE_OP_ALWAYS,					//	VkCompareOp		compareOp;
				0.0f,									//	float			minLod;
				0.0f,									//	float			maxLod;
				VK_BORDER_COLOR_TRANSPARENT_BLACK,		//	VkBorderColor	borderColor;
			};

			log << TestLog::Message << deviceNdx << ": " << tcu::toHex(devices[deviceNdx]) << TestLog::EndMessage;

			const DeviceDriver			vkDevice	(vkPlatform, devices[deviceNdx]);
			const Unique<VkDeviceT>		device		(createDevice(vkDevice, devices[deviceNdx], &deviceInfo));
			Move<VkSamplerT>			tmpSampler	= createSampler(vkDevice, *device, &samplerInfo);
			const Unique<VkSamplerT>	sampler		(tmpSampler);
		}
	}

	return tcu::TestStatus::pass("Creating sampler succeeded");
}

tcu::TestCaseGroup* createTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup>	apiTests	(new tcu::TestCaseGroup(testCtx, "api", "API Tests"));

	addFunctionCase(apiTests.get(), "create_sampler", "", createSampler);

	return apiTests.release();
}

} // api
} // vkt
